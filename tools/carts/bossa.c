/* de:meta
{
  "title": "bossa radio",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "step-sequencer",
    "schedule-driven-agents"
  ],
  "lineage": "Extends the radio.h station chassis with a full jazz-harmony engine: Markov chord walks over harmonic functions, rootless 3-voice voice-leading, and a schedule_hit() step sequencer — the worked example for docs/guides/game-music.md.",
  "description": "An endless radio station that composes bossa nova. Every song is generated from a 32-bit seed shown on the display: key, AABA chord progression (Markov walk over jazz harmony - ii-V-I, tritone subs, backdoor dominants), voice-led guitar comping on the bossa clave, surdo bass, shaker, a One Note Samba-style melody cell, tempo, even the title. SPACE next song, R play the same chart again, [ ] song history, LEFT/RIGHT feel (layers), UP/DOWN tempo, M power. Pin a favourite: put its #seed in BOSSA_SEED at the top of the source. The worked example for docs/guides/game-music.md."
}
de:meta */
#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include "solo.h"    // the jam layer — a scale-locked solo strip over the changes
#include <stdio.h>
#include <math.h>

// ── BOSSA RADIO ───────────────────────────────────────────────────────────
// A generative music cart: an endless radio station that composes bossa nova.
// Every "song" is generated — key, chord progression (Markov walk over jazz
// harmony functions, AABA form), guitar comping pattern, melody cell, tempo,
// even the title. After a few choruses the station moves to the next tune.
//
// This cart doubles as the worked example for docs/guides/game-music.md —
// the recipes here (chord brain, voice-leading, clave time-feel, the
// schedule-ahead step sequencer) are the building blocks for ANY game
// soundtrack. If you're an agent asked to "add music to my game", start there.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (layers)   UP/DOWN tempo
//
// Every song IS a 32-bit seed — the display shows it (#XXXXXXXX). Hear one you
// love? Note the seed and pin it below; the radio then opens with that tune.
// The seed fixes the COMPOSITION (key, changes, melody cell, tempo, title);
// the PERFORMANCE (strum timing, ghost notes, the melody's pitch path) stays
// live — replaying a seed is the band playing the same chart again, not a tape.
//
// Architecture, bottom-up:
//   1. step clock   — a 16th-note grid driven by beat()/beat_pos(); every step
//                     is scheduled ONE STEP AHEAD with schedule_hit() so notes
//                     land sample-accurate, free of 60fps frame jitter.
//   2. chord brain  — gen_section() walks a weighted transition table over
//                     harmonic functions (I, ii, V7, tritone subs, backdoor
//                     dominants…), always cadencing ii→V at bar 7-8.
//   3. voice leading— the guitar never re-stacks chords from the root; its 3
//                     voices each move to the NEAREST tone of the next chord
//                     (lead_voices). This is what makes it sound composed.
//   4. time feel    — bass plays the surdo pattern, guitar comps the bossa
//                     clave (with anticipations: hits in the last 8th of a bar
//                     already play the NEXT bar's chord), shaker runs 16ths.
//   5. melody       — One Note Samba trick: ONE syncopated rhythm cell repeats
//                     all song while its pitches re-resolve to each new chord.

#define BOSSA_SEED 0          // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_GTR    5   // nylon guitar pluck
#define I_BASS   6   // soft fingered bass
#define I_FLUTE  7   // breathy lead
#define I_SHAKER 8   // 16th-note caxixi
#define I_RIM    9   // cross-stick clave
#define I_SOLO   10  // the jam-strip lead — a present flute over the changes

// ── chord qualities ───────────────────────────────────────────────────────
enum { Q_MAJ7, Q_MIN7, Q_DOM7, Q_M7B5, Q_MIN6, NQUAL };
static const char *QNAME[NQUAL] = { "maj7", "m7", "7", "m7b5", "m6" };
// rootless 3-voice guitar voicings (intervals above chord root) — the bass
// owns the root, the guitar plays the color: 3rd + 7th + 9th (or 5th/6th)
static const int QVOICE[NQUAL][3] = {
    { 4, 11, 14 },   // maj7  → 3 7 9
    { 3, 10, 14 },   // m7    → b3 b7 9
    { 4, 10, 14 },   // 7     → 3 b7 9
    { 3,  6, 10 },   // m7b5  → b3 b5 b7
    { 3,  9, 14 },   // m6    → b3 6 9
};
static const int QTONES[NQUAL][4] = {  // chord tones for the melody to target
    { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 3, 6, 10 }, { 0, 3, 7, 9 },
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

// ── harmonic functions (scale-degree offset + quality) ────────────────────
enum { F_I, F_ii, F_iii, F_IV, F_V, F_vi, F_II7, F_VI7, F_bII7, F_iv, F_bVII7, F_v, F_I7, NFUNC };
static const int F_OFF[NFUNC]  = { 0, 2, 4, 5, 7, 9, 2, 9, 1, 5, 10, 7, 0 };
static const int F_QUAL[NFUNC] = { Q_MAJ7, Q_MIN7, Q_MIN7, Q_MAJ7, Q_DOM7, Q_MIN7,
                                   Q_DOM7, Q_DOM7, Q_DOM7, Q_MIN6, Q_DOM7, Q_MIN7, Q_DOM7 };

// weighted transitions: where can each function go? (repeats = more likely)
// reads like a jazz harmony cheat-sheet: ii→V→I, secondary dominants resolve
// down a fifth (VI7→ii, II7→V), bII7 is the tritone sub of V, iv/bVII7 is the
// backdoor cadence, v→I7 is "ii-V of IV" en route to the subdominant.
#define T(name, ...) static const int name[] = { __VA_ARGS__ };
T(T_I,     F_vi, F_vi, F_vi, F_II7, F_II7, F_IV, F_IV, F_iii, F_iii, F_VI7, F_VI7, F_v)
T(T_ii,    F_V, F_V, F_V, F_V, F_V, F_bII7, F_bII7)
T(T_iii,   F_VI7, F_VI7, F_VI7, F_vi, F_vi, F_ii)
T(T_IV,    F_iv, F_iv, F_iv, F_ii, F_ii, F_I, F_I, F_bVII7, F_bVII7)
T(T_V,     F_I, F_I, F_I, F_I, F_I, F_iii, F_vi)
T(T_vi,    F_ii, F_ii, F_ii, F_ii, F_II7, F_iv)
T(T_II7,   F_ii, F_ii, F_ii, F_V, F_V)
T(T_VI7,   F_ii, F_ii, F_ii, F_ii, F_ii)
T(T_bII7,  F_I, F_I, F_I, F_I, F_I)
T(T_iv,    F_bVII7, F_bVII7, F_bVII7, F_I, F_I)
T(T_bVII7, F_I, F_I, F_I, F_I)
T(T_v,     F_I7, F_I7, F_I7, F_I7)
T(T_I7,    F_IV, F_IV, F_IV, F_IV)
#undef T
static const int *TRANS[NFUNC] = { T_I, T_ii, T_iii, T_IV, T_V, T_vi, T_II7,
                                   T_VI7, T_bII7, T_iv, T_bVII7, T_v, T_I7 };
static const int TRANSN[NFUNC] = { 12, 7, 6, 9, 7, 6, 5, 5, 5, 5, 4, 4, 4 };

// ── comping patterns — 2-bar masks over a 32-step (16th-note) grid ────────
// the classic bossa clave is  |X..X..X.|..X..X..|  (8th-note positions)
static const int COMPS[3][6] = {
    { 0, 6, 12, 20, 26, -1 },   // the classic João pattern
    { 0, 6, 12, 20, 26, 30 },   // + a pickup anticipating the next two bars
    { 4, 10, 16, 22, 28, -1 },  // reversed (2-3) feel
};
static const int CLAVE[2][5] = {
    { 0, 6, 12, 20, 26 },       // 3-2 bossa clave
    { 4, 10, 16, 22, 28 },      // 2-3 (reversed)
};

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;            // 0..11
    int  prog[16];         // function per bar — [0..7] = A section, [8..15] = B
    int  comp;             // which comping mask
    int  claveSwap;        // rim plays 3-2 or 2-3
    int  cellOn[6], cellN; // melody rhythm cell: onsets within the 32-step grid
    int  choruses;         // play the AABA form this many times, then next song
    char title[24];
    float freq;            // fake FM dial position
    unsigned seed;         // the whole composition, replayable from this one number
} Song;

// composition PRNG + session history live in radio.h (RadioSeed rs); srnd is the
// SAME xorshift stream as before the migration — pinned seeds depend on it byte
// for byte. performance jitter keeps engine rnd().
static Song       sng;
static RadioSeed  rs;                       // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 119.0 };   // schedule-ahead step clock (radio.h)
// the clock's fields under their pre-migration names — keeps the body textually
// unchanged (smallest possible diff over the original)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)    rad_srnd(&rs, (n))
#define schance(p) (srnd(100) < (p))
static int    tempo      = 126;
static int    intensity  = 2;        // 0 bass+gtr · 1 +shaker · 2 +rim+melody · 3 everything louder
static bool   radioOn    = true;
static int    songCount  = 0;
static int    gv[3]      = { 64, 67, 71 };  // guitar voices (midi), led chord to chord
static int    gtrPluck   = 0;              // 0 = TRI fake, 1 = INSTR_PLUCK real string
static bool   gvInit     = false;
static int    melPitch   = 79;       // melody walker position
static bool   melOn      = true;     // does the cell play this 2-bar instance?
static float  vu         = 0;        // VU needle drive
static char   nowChord[3][8];        // prev / current / next chord names for the display
static bool   showHelp   = false;    // H or the ? button
static RadBand band;                 // THE BAND (B): the guitar chair — nylon TRI / real string
static int    chGtr;                 // its chair index
static int    toneSel    = 0;        // bossa has no tone knob — rad_input gets ntone=0

static int iabs(int v) { return v < 0 ? -v : v; }

// ── song generation ───────────────────────────────────────────────────────
static int markov_next(int f) { return TRANS[f][srnd(TRANSN[f])]; }

static void gen_section(int *bars, int startFunc) {
    bars[0] = startFunc;
    for (int i = 1; i < 6; i++) bars[i] = markov_next(bars[i - 1]);
    bars[6] = F_ii;                                  // always cadence home:
    bars[7] = schance(25) ? F_bII7 : F_V;            // ii → V (or tritone sub)
}

static const char *TW1[] = { "Onda","Praia","Lua","Saudade","Garota","Brisa","Chuva",
    "Tarde","Janela","Estrela","Areia","Flor","Sombra","Manha","Ipanema","Coqueiro" };
static const char *TW2[] = { "de Verao","do Mar","da Manha","do Rio","de Marco",
    "da Lua","do Vento","de Abril","da Praia","do Sol" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);   // 0 = derive fresh (same expression as ever)

    sng.keyPc = srnd(12);
    gen_section(sng.prog,     F_I);
    gen_section(sng.prog + 8, schance(60) ? F_IV : F_vi);  // bridge leaves home
    sng.comp      = srnd(3);
    sng.claveSwap = (sng.comp == 2) ? 0 : schance(50);
    sng.choruses  = 2 + srnd(2);

    // melody rhythm cell: 3..6 syncopated onsets on the 8th-note grid,
    // leaning into offbeats — this ONE cell repeats the whole song
    sng.cellN = 0;
    for (int s = 0; s < 31 && sng.cellN < 6; s += 2) {
        int p = (s % 8 == 0) ? 22 : (s % 4 == 2 ? 42 : 30);
        if (schance(p)) sng.cellOn[sng.cellN++] = s;
    }
    if (sng.cellN < 3) {                                   // fallback: a known-good cell
        sng.cellN = 4;
        sng.cellOn[0] = 2; sng.cellOn[1] = 8; sng.cellOn[2] = 14; sng.cellOn[3] = 22;
    }

    if (schance(20)) snprintf(sng.title, sizeof sng.title, "Minha %s", TW1[srnd(16)]);
    else             snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(16)], TW2[srnd(10)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 112 + srnd(28);
    bpm(tempo);
    songBase = (long)pos + 8;          // half a bar of air between tunes
    gvInit   = false;
    melPitch = 79;
    songCount++;
}

static void fresh_song(double pos) {       // [ and ] walk the session history (radio.h)
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── harmony helpers ───────────────────────────────────────────────────────
static int bar_to_prog(long bar32) {   // AABA: bars 0-7 A, 8-15 A, 16-23 B, 24-31 A
    long b = bar32 % 32;
    if (b < 16) return (int)(b % 8);
    if (b < 24) return (int)(8 + b - 16);
    return (int)(b - 24);
}

// chord governing song-step s — hits in the last 8th of a bar anticipate the
// next bar's chord (the signature bossa "early" change)
static int func_at(long s) {
    long bar = s / 16;
    if (s % 16 >= 14) bar++;
    return sng.prog[bar_to_prog(bar)];
}

static int root_pc(int f) { return (sng.keyPc + F_OFF[f]) % 12; }

// move each guitar voice to the nearest tone of the new chord — rad_lead_to
// (radio.h) is the shared block, the single biggest "sounds composed" trick;
// this just resolves the function into root + the quality's voicing intervals.
// guitar register window: 58..82.
static void lead_voices(int f) {
    rad_lead_to(root_pc(f), QVOICE[F_QUAL[f]], gv, 3, 58, 82, &gvInit);
}

// melody: pick the chord tone (or 9th) nearest the walker, with a little drift
static int pick_mel(int f) {
    int q = F_QUAL[f], rp = root_pc(f);
    int bestM = melPitch, bestScore = -999;
    for (int t = 0; t < 5; t++) {
        int off = (t < 4) ? QTONES[q][t] : 14;
        int pc = (rp + off) % 12;
        for (int oct = 6; oct <= 7; oct++) {
            int m = oct * 12 + pc;
            if (m < 72 || m > 91) continue;
            int score = 12 - iabs(m - melPitch) + rnd(5);
            if (m == melPitch) score -= 4;           // discourage hammering one note
            if (score > bestScore) { bestScore = score; bestM = m; }
        }
    }
    melPitch = bestM;
    return bestM;
}

// ── the step player — schedules everything for absolute 16th-step `abs` ───
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;                              // between songs: air
    int dly = rad_step_dly(&clk, abs, pos);
    int step = (int)(s % 16);                       // 16th within the bar
    int s32  = (int)(s % 32);                       // position in the 2-bar pattern
    int f    = func_at(s);

    // BASS — the surdo pattern: root on 1, fifth on the and-of-2 and on 3,
    // chromatic approach into the next bar on the and-of-4
    if (step == 0 || step == 8 || (step == 6 && chance(50)) || (step == 14 && chance(35))) {
        int b = 36 + root_pc(f);
        int n = b, vol = 5, dur = (int)(stepMs * 5);
        if (step == 6)  { n = b + 7 <= 50 ? b + 7 : b - 5; vol = 3; dur = (int)(stepMs * 1.6); }
        if (step == 8)  { n = b + 7 <= 50 ? b + 7 : b - 5; vol = 4; }
        if (step == 14) { n = 36 + root_pc(func_at(s + 2)) + (chance(50) ? -1 : 1); vol = 3; dur = (int)(stepMs * 1.6); }
        schedule_hit(dly, n, I_BASS, vol, dur);
        vu += vol * 0.8f;
    }

    // GUITAR — comp the clave; voice-lead, then strum the 3 voices 8ms apart
    for (int i = 0; i < 6; i++) {
        if (COMPS[sng.comp][i] != s32) continue;
        lead_voices(f);
        int gap = 32;                               // 16ths until the next comp hit
        for (int j = 0; j < 6; j++) {
            int o = COMPS[sng.comp][j];
            if (o < 0) continue;
            int g = (o - s32 + 32) % 32;
            if (g > 0 && g < gap) gap = g;
        }
        int dur = (int)(gap * stepMs * 0.8);
        if (dur > 360) dur = 360;
        int vol = (s32 == COMPS[sng.comp][0]) ? 4 : 3;
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 8 + rnd(4), gv[k], I_GTR, vol, dur);
        vu += vol;
    }

    // SHAKER — straight 16ths, accent on the offbeat 8th (the caxixi push)
    if (intensity >= 1) {
        int vol = (step % 4 == 2) ? 3 : (step % 2 == 0 ? 2 : 1);
        if (intensity == 0) vol--;
        schedule_hit(dly + rnd(4), 84, I_SHAKER, vol, vol >= 3 ? 55 : 35);
    }

    // RIM — the cross-stick clave
    if (intensity >= 2)
        for (int i = 0; i < 5; i++)
            if (CLAVE[sng.claveSwap][i] == s32) {
                schedule_hit(dly, 76, I_RIM, 3, 30);
                vu += 1.5f;
            }

    // MELODY — the repeating cell, re-pitched to each chord; rests breathe.
    // The band's flute lays out while you have the jam strip open: gating new
    // note-ons (not the sounding voice) lets the current note ring to its end,
    // so the soloist finishes on the beat and yields the lead — no mid-note chop.
    if (intensity >= 2 && !solo_open()) {
        if (s32 == 0) melOn = chance(intensity >= 3 ? 85 : 60);
        if (melOn)
            for (int i = 0; i < sng.cellN; i++)
                if (sng.cellOn[i] == s32) {
                    int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - s32 : 32 - s32;
                    int dur = (int)(gap * stepMs * 0.9);
                    if (dur > 900) dur = 900;
                    schedule_hit(dly + 10 + rnd(8),      // sing slightly behind the beat
                                 pick_mel(f), I_FLUTE, intensity >= 3 ? 4 : 3, dur);
                    vu += 2;
                }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
// the guitar chair has two candidates — A/B them live with G.
// (gtrPluck=0) TRI + filter env = the shipped bossa nylon fake.
// (gtrPluck=1) INSTR_PLUCK with low timbre/morph = nylon character from the real string engine.
static void setup_gtr(void) {
    if (!gtrPluck) {
        instrument(I_GTR, INSTR_TRI, 1, 180, 1, 120);
        instrument_filter(I_GTR, FILTER_LOW, 2200, 3);
        instrument_env(I_GTR, 0, ENV_CUTOFF, 0, 90, 1400);   // attack sparkle
    } else {
        instrument(I_GTR, INSTR_PLUCK, 1, 0, 7, 120);
        instrument_filter(I_GTR, FILTER_LOW, 2200, 3);        // same warmth cap
        instrument_harmonics(I_GTR, 0.40f);                   // shorter ring — nylon decays fast
        instrument_timbre(I_GTR, 0.15f);                      // soft thumb, no pick brightness
        instrument_morph(I_GTR, 0.45f);                       // mid-string warmth
    }
}

static void setup_instruments(void) {
    setup_gtr();
    chGtr = rad_chair(&band, "guitar", "TRI", "PLUCK", NULL, NULL);   // the one A/B chair

    instrument(I_BASS, INSTR_TRI, 2, 200, 4, 80);            // round fingered bass
    instrument_filter(I_BASS, FILTER_LOW, 700, 2);

    instrument(I_FLUTE, INSTR_SINE, 25, 120, 5, 140);        // breathy lead
    instrument_lfo(I_FLUTE, 0, LFO_PITCH, 5.2f, 0.18f);      // vibrato
    instrument_filter(I_FLUTE, FILTER_LOW, 2600, 2);

    instrument(I_SHAKER, INSTR_NOISE, 1, 45, 0, 25);         // caxixi
    instrument_filter(I_SHAKER, FILTER_HIGH, 5200, 4);

    instrument(I_RIM, INSTR_NOISE, 0, 28, 0, 18);            // woody cross-stick
    instrument_filter(I_RIM, FILTER_BAND, 1800, 9);
    instrument_env(I_RIM, 0, ENV_PITCH, 0, 20, 18);

    instrument(I_SOLO, INSTR_SINE, 20, 140, 6, 200);         // the jam flute — sits on top
    instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.6f, 0.22f);       // a singing vibrato
    instrument_filter(I_SOLO, FILTER_LOW, 3600, 2);          // brighter than the comp flute
}

// ── update ────────────────────────────────────────────────────────────────
static void chord_label(char *out, int n, int f) {
    snprintf(out, n, "%s%s", PCNAME[root_pc(f)], QNAME[F_QUAL[f]]);
}

void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (BOSSA_SEED) { new_song(pos, BOSSA_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    // the shared input block (radio.h): feel/tempo/help handled inside, the cart
    // reacts to the events. ntone=0 — bossa has no tone knob.
    int ev = rad_input(&tempo, 100, 152, 4, &intensity, &toneSel, 0, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);                   // same chart, fresh take
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) note_off_all();
        else scheduled = (long)pos;        // rejoin the broadcast mid-song
    }
    int chair = rad_band_input(&band, &showHelp);            // THE BAND (B) — A/B the guitar chair
    if (chair == chGtr) { gtrPluck = band.c[chGtr].sel; setup_gtr(); }   // mid-song swap

    if (radioOn) {
        long st;                           // schedule one step ahead of the clock
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= (long)sng.choruses * 512) fresh_song(pos);   // station moves on

        // keep the chord display fresh
        long bar = songStep >= 0 ? songStep / 16 : 0;
        chord_label(nowChord[0], 8, sng.prog[bar_to_prog(bar > 0 ? bar - 1 : 0)]);
        chord_label(nowChord[1], 8, sng.prog[bar_to_prog(bar)]);
        chord_label(nowChord[2], 8, sng.prog[bar_to_prog(bar + 1)]);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    watch("song", "%d", songCount);
    watch("bar", "%ld", ss >= 0 ? (ss / 16) % 32 : -1);
    watch("chord", "%s", nowChord[1]);
    watch("step", "%ld", ss >= 0 ? ss % 16 : -1);
#endif
}

// ── draw — the radio face (shared chassis from radio.h; the window art — the
// speaker grille pulsing with the VU — stays bossa's own) ──────────────────
void draw(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();                              // contacts in — the knobs are touch-draggable
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? (songStep / 16) % 32 : 0;

    rad_body(CLR_BROWN, CLR_ORANGE);     // warm brown shell, a tropical accent
    rad_dial(sng.freq, CLR_ORANGE);

    // window — the speaker grille, pulsing gently with the VU
    rectfill(34, 52, 102, 116, CLR_DARK_BROWN);
    for (int x = 38; x < 132; x += 4)
        line(x, 56, x, 164, (x / 4) % 2 ? CLR_BLACK : CLR_DARK_BROWN);
    int pr = 24 + (int)(vu * 1.2f);
    if (radioOn) circ(85, 110, pr > 50 ? 50 : pr, CLR_BLACK);
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_ORANGE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_LIME_GREEN);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  key %s", sng.freq, PCNAME[sng.keyPc]);
        print(l2, 154, 70, CLR_MEDIUM_GREEN);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_MEDIUM_GREEN);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // chord readout: prev / CURRENT / next — bossa's own three-chord look
    if (radioOn) {
        print(nowChord[0], 152, 106, CLR_DARK_PEACH);
        int cw = text_width(nowChord[1]);
        rectfill(214 - cw / 2 - 3, 102, cw + 6, 13, CLR_ORANGE);
        print(nowChord[1], 214 - cw / 2, 105, CLR_BLACK);
        print(nowChord[2], 288 - text_width(nowChord[2]), 106, CLR_DARK_PEACH);

        // AABA form + bar dots
        const char *FORM = "AABA";
        for (int i = 0; i < 4; i++) {
            char c[2] = { FORM[i], 0 };
            print(c, 166 + i * 14, 122, i == bar / 8 ? CLR_WHITE : CLR_MEDIUM_GREY);
        }
        rad_phrase_dots(236, 126, 8, bar % 8, CLR_YELLOW);
    }

    // knobs (feel + tempo are draggable; vu is a meter) + power LED
    static const char *FEEL[4] = { "midnight", "cafe", "classic", "festival" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_YELLOW);
    if (rad_knob_int(&tempo, 100, 152, 4, 218, 148, 9, "tempo", CLR_YELLOW)) bpm(tempo);
    float vt = vu / 12.0f;
    rad_knob(262, 148, 11, vt > 1 ? 1 : vt, "vu", CLR_RED);
    rad_power_led(radioOn, CLR_RED, CLR_DARK_RED);

    rad_help_button(CLR_ORANGE);
    rad_band_button(CLR_ORANGE);

    if (showHelp) {
        static const char *HELP[9][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same song again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - how many layers play" },
            { "UP/DOWN",    "tempo of this tune" },
            { "B",          "the band - swap the guitar timbre" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
            { "L",          "jam ch/sc: chord-lock / free" },
        };
        static const char *NOTES[3] = {
            "the #number on the display IS the song.",
            "pin it for good: #define BOSSA_SEED 0x...",
            "seeded composition, played fresh every time",
        };
        rad_help_panel("BOSSA RADIO", HELP, 9, NOTES, 3, CLR_ORANGE);
    }
    rad_band_panel(&band, CLR_ORANGE);

    // the jam strip — CHORD-LOCK (Omnichord): the strip is the current chord's own
    // tones (root/3rd/5th/9th), re-shaped as the ii-V-I changes move under you. Bossa's
    // jazz harmony is too rich for one scale; chord-lock keeps every note consonant over
    // the secondary dominants and tritone subs (J or tap the corner to open)
    int chord[4]; {
        int f = sng.prog[bar_to_prog(bar)], q = F_QUAL[f], rp = root_pc(f);
        for (int k = 0; k < 4; k++) chord[k] = (rp + QTONES[q][k]) % 12;
    }
    static const int PENT[5] = { 0, 2, 4, 7, 9 };  // the "sc" toggle's fallback: free pentatonic over the changes
    // vertical = breath dynamics (a flute leans soft→loud)
    SoloCtx jc = { sng.keyPc, PENT, 5, chord, 4, I_SOLO, 72, 91, false, SOLO_Y_VOL, 2, 6, false, true };
    solo_strip(&jc, 28, 170, 250, 18, CLR_ORANGE);

    ui_end();                                // resolve this frame's knob + jam grabs
}
