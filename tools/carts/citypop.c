/* de:meta
{
  "title": "city pop radio",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "step-sequencer",
    "swing-timing"
  ],
  "lineage": "Sixth radio-family station built on radio.h/solo.h chassis; novel: genre encoded as a named chord-template vocabulary (Royal Road, JTTOU, descent), truck-driver modulation, and session-tight ±2ms humanization as an explicit anti-groove design.",
  "homage": "Tatsuro Yamashita / Mariya Takeuchi (city pop)",
  "description": "Endless Japanese pop-funk - the city pop homage (Tatsuro Yamashita / Mariya Takeuchi / Plastic Love), sixth of the radio family. The canon as data: the ROYAL ROAD progression (IVmaj7-V7-iiim7-vim9, THE J-pop changes), Just-the-Two-of-Us loops (IVmaj7-III7alt-vim9-I9), the bittersweet descent (Imaj9-I9-IVmaj7-ivm6); octave-popping disco bass with double-chromatic 16th runs; two-voice guitar chucks that lay out for the bar turn; saw-brass anticipation hits; session-tight timing (the opposite of low end radio - no groove lag, +-2ms only); and the truck driver's gear change: the last chorus modulates up a whole step. Density = arrangement curve x feel knob; T cycles tone. Seed on display (CITYPOP_SEED / R / [ ] history), H help. Worked example #6 for docs/guides/game-music.md."
}
de:meta */
#include "studio.h"
#include "radio.h"
#include "solo.h"    // the jam layer — a scale-locked solo strip over the changes   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include <stdio.h>
#include <math.h>

// ── CITY POP RADIO ────────────────────────────────────────────────────────
// The neon sibling of the radio family: endless Japanese pop-funk, the
// city pop homage (Tatsuro Yamashita / Mariya Takeuchi / "Plastic Love").
// Sixth station, and the most tightly codified genre yet:
//
//   • the ROYAL ROAD progression (王道進行) — IVmaj7 | V7 | iiim7 | vim7,
//     THE J-pop changes; it resolves to the relative minor as happily as
//     to the tonic, which is the whole bittersweet-but-glossy trick.
//   • "Just the Two of Us" changes — IVmaj7 | III7alt | vim9 | I9(V/IV),
//     the loop that pulls itself back to the IV forever.
//   • OCTAVE-POP DISCO BASS — root low, octave high on the offbeats, and a
//     double-chromatic 16th run into every new chord. The bass is the funk.
//   • 16th-note GUITAR CHUCKS — two-voice muted scratches on the off-8ths,
//     full stabs on the accents.
//   • SAW BRASS on the anticipations — the horn section hits the and-of-4
//     a 16th before the bar, then falls.
//   • the TRUCK DRIVER'S GEAR CHANGE — the last chorus modulates up a whole
//     step. Of course it does.
//
// Time feel is the OPPOSITE of lowend.c: session-musician tight. No groove
// template, no tape wobble — jitter is ±2ms humanization and nothing more.
// These players got charts and union rates.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help
//
// Density = arrangement curve x feel shift (the two-dimension model from
// game-music.md): intro/outro 0 · verse 1 · chorus 2, feel shifts the whole
// curve. The chorus out-glitters the verse at any knob position.
// Seed pins the COMPOSITION; the PERFORMANCE re-rolls every playthrough.

#define CITYPOP_SEED 0   // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_GTR   5   // clean funk guitar — chucks and stabs
#define I_BASS  6   // the octave-popping star
#define I_LEAD  7   // bright chorus synth
#define I_BRASS 8   // saw stack horns
#define I_KICK  9
#define I_SNARE 10
#define I_HAT   11
#define I_SOLO  12  // the jam-strip lead — a glossy solo synth over the changes

// ── chords ────────────────────────────────────────────────────────────────
enum { Q_MAJ7, Q_MAJ9, Q_DOM7, Q_DOM9, Q_DOM13, Q_ALT7, Q_MIN7, Q_MIN9, Q_MIN6, NQ };
static const char *QN[NQ] = { "maj7", "maj9", "7", "9", "13", "7", "m7", "m9", "m6" };
static const int QV[NQ][3] = {     // rootless 3-voice voicings
    { 4, 11, 14 }, { 4, 11, 14 },             // maj7 / maj9
    { 4, 10, 14 }, { 4, 10, 14 },             // 7 / 9
    { 4, 10, 21 },                            // 13 — the glossy V
    { 4, 10, 20 },                            // 7alt (b13) — the JTTOU crunch
    { 3, 10, 14 }, { 3, 10, 14 },             // m7 / m9
    { 3, 9, 14 },                             // m6
};
static const int QT[NQ][4] = {     // chord tones for the lead
    { 0, 4, 7, 11 }, { 0, 4, 11, 14 }, { 0, 4, 7, 10 }, { 0, 4, 10, 14 },
    { 0, 4, 10, 21 }, { 0, 4, 10, 20 }, { 0, 3, 7, 10 }, { 0, 3, 10, 14 },
    { 0, 3, 9, 14 },
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

// ── progression templates — the city pop canon ────────────────────────────
typedef struct { int off, q; } Ch;
static const Ch TMPL[5][4] = {
    // royal road (王道進行): IVmaj7 | V7 | iiim7 | vim9
    { { 5, Q_MAJ7 }, { 7, Q_DOM7 }, { 4, Q_MIN7 }, { 9, Q_MIN9 } },
    // royal road, chorus gloss: IVmaj9 | V13 | iiim7 | vim9
    { { 5, Q_MAJ9 }, { 7, Q_DOM13 }, { 4, Q_MIN7 }, { 9, Q_MIN9 } },
    // "Just the Two of Us": IVmaj7 | III7alt | vim9 | I9 (V/IV — loops forever)
    { { 5, Q_MAJ7 }, { 4, Q_ALT7 }, { 9, Q_MIN9 }, { 0, Q_DOM9 } },
    // smooth verse: iim9 | V13 | Imaj9 | vim9
    { { 2, Q_MIN9 }, { 7, Q_DOM13 }, { 0, Q_MAJ9 }, { 9, Q_MIN9 } },
    // the descent: Imaj9 | I9 (V/IV) | IVmaj7 | ivm6 (borrowed — bittersweet)
    { { 0, Q_MAJ9 }, { 0, Q_DOM9 }, { 5, Q_MAJ7 }, { 5, Q_MIN6 } },
};
static const int VERSE_POOL[3]  = { 3, 4, 2 };
static const int CHORUS_POOL[3] = { 0, 1, 2 };   // the chorus IS the royal road

// the form; the LAST chorus (and outro) modulate +2 — the gear change
enum { S_INTRO, S_V, S_C, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_V, S_V, S_C, S_V, S_C, S_C, S_OUTRO };
#define MOD_BAR 48    // sections 6+7 (final chorus + outro) play a whole step up

// funk guitar: chuck mask variants (16 steps; 1 = chuck, 2 = accented stab).
// NOTHING on steps 13-15: the players lay out for the bar turn — that's where
// the brass anticipation + bass run land, and stacking chucks there too blew
// the 8-voice budget and made the synth steal ringing voices (the screech).
static const int CHUCK[3][16] = {
    { 0,0,1,0, 0,0,2,0, 0,0,1,0, 0,0,0,0 },
    { 0,0,1,1, 0,0,2,0, 0,0,1,0, 1,0,0,0 },
    { 0,0,2,0, 0,1,1,0, 0,0,2,0, 0,0,0,0 },
};
// disco kick patterns (16 steps per bar)
static const int KICKP[3][5] = {
    { 0, 8, -1, -1, -1 },          // boom-chack
    { 0, 8, 10, -1, -1 },          // + the push
    { 0, 6, 8, -1, -1 },           // + the skip
};

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    Ch   verse[4], chorus[4];
    int  chuck, kick;
    char title[24];
    float freq;
    unsigned seed;
} Song;

// composition PRNG + session history live in radio.h (RadioSeed rs); srnd is the
// SAME xorshift stream as before the migration — pinned seeds depend on it byte
// for byte. performance jitter keeps engine rnd().
static Song       sng;
static RadioSeed  rs;                       // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 136.0 };   // schedule-ahead step clock (radio.h)
// the clock's fields under their pre-migration names — keeps the body textually
// unchanged (smallest possible diff over the original)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)    rad_srnd(&rs, (n))
static int    tempo     = 110;
static int    intensity = 1;     // feel: shifts the arrangement's density curve
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    songCount = 0;
static int    gv[3]     = { 64, 67, 71 };
static bool   gvInit    = false;
static float  vu        = 0;
static int    melPitch  = 81;
static int    bassLast  = 38;
static int    cellOn[5], cellN = 0;
static char   nowChord[4][8];

static int iabs(int v) { return v < 0 ? -v : v; }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Plastic", "Midnight", "Magic", "Summer", "Crystal",
    "Tokyo", "Sparkle", "Velvet", "Downtown", "Neon", "Pacific", "Windy" };
static const char *TW2[] = { "Love", "Drive", "Wave", "Lady", "Night", "City",
    "Motion", "Breeze", "Heart", "Line", "Story", "Lights" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);   // 0 = derive fresh (same expression as ever)

    sng.keyPc = srnd(12);
    int vi = VERSE_POOL[srnd(3)], ci;
    do { ci = CHORUS_POOL[srnd(3)]; } while (ci == vi);
    for (int k = 0; k < 4; k++) { sng.verse[k] = TMPL[vi][k]; sng.chorus[k] = TMPL[ci][k]; }

    sng.chuck = srnd(3);
    sng.kick  = srnd(3);

    cellN = 0;                                           // chorus lead cell
    for (int s = 0; s < 31 && cellN < 5; s += 2)
        if (srnd(100) < (s % 8 == 0 ? 30 : 38)) cellOn[cellN++] = s;
    if (cellN < 3) { cellN = 3; cellOn[0] = 0; cellOn[1] = 6; cellOn[2] = 20; }

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 104 + srnd(15);                              // 104..118
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;
    melPitch = 81;
    bassLast = 38;
    songCount++;
}

static void fresh_song(double pos) {       // [ and ] walk the session history (radio.h)
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ────────────────────────────────────────────────────────
static int sect_of(long bar)  { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }
static const Ch *prog_of(long bar) { return sect_of(bar) == S_C ? sng.chorus : sng.verse; }
static Ch  chord_at(long bar) { return prog_of(bar)[bar % 4]; }
// the gear change lives here: everything past MOD_BAR is a whole step up
static int root_pc(Ch c, long bar) {
    return (sng.keyPc + (bar >= MOD_BAR ? 2 : 0) + c.off) % 12;
}

static void chord_label(char *out, int n, Ch c, long bar) {
    snprintf(out, n, "%s%s", PCNAME[root_pc(c, bar)], QN[c.q]);
}

// density = arrangement curve + feel shift (see game-music.md)
//   0: bass + drums          2: + brass anticipations and the lead
//   1: + guitar chucks       3: + 16th hats, the full glitter
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_V ? 1 : 2);
    int lvl = base + intensity - 1;
    return lvl < 0 ? 0 : lvl > 3 ? 3 : lvl;
}

// ── the tone knob (T cycles) — master brightness, re-issued live ──────────
static int toneSel = 2;
static const char *TONENAME[4] = { "mellow", "warm", "clear", "bright" };
static const float TONEMUL[4]  = { 0.55f, 0.78f, 1.0f, 1.28f };

static void apply_voicing(void) {
    float m = TONEMUL[toneSel];
    instrument_filter(I_GTR,   FILTER_LOW, (int)(2600 * m), 2);
    instrument_filter(I_LEAD,  FILTER_LOW, (int)(2800 * m), 2);
    instrument_filter(I_BRASS, FILTER_LOW, (int)(2000 * m), 3);
    instrument_filter(I_BASS,  FILTER_LOW, (int)(900 * (0.7f + 0.3f * m)), 2);
}

// voice-led bass register, A1..A2 — bright enough to pop
static int bass_peek(int pc) { return rad_bass_to(pc, bassLast, 33, 45); }
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

// nearest-tone voice leading — rad_lead_to (radio.h) is the shared block, the
// single biggest "sounds composed" trick. guitar register window: 57..81.
static void lead_voices(Ch c, long bar) {
    rad_lead_to(root_pc(c, bar), QV[c.q], gv, 3, 57, 81, &gvInit);
}

// chorus lead: chord tones + 9, nearest the walker (the accommodation rule)
static int pick_mel(Ch c, long bar) {
    int bestM = melPitch, bestScore = -999;
    for (int t = 0; t < 4; t++) {
        int pc = (root_pc(c, bar) + QT[c.q][t]) % 12;
        for (int oct = 6; oct <= 7; oct++) {
            int m = oct * 12 + pc;
            if (m < 76 || m > 93) continue;
            int score = 10 - iabs(m - melPitch) + rnd(4);
            if (m == melPitch) score -= 3;
            if (score > bestScore) { bestScore = score; bestM = m; }
        }
    }
    melPitch = bestM;
    return bestM;
}

// ── the step player — TIGHT. humanize is rnd(2) and nothing more ─────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    Ch   c    = chord_at(bar);
    int  lvl  = level_of(bar);

    // BASS — root low, octave pops on the offbeats, double-chromatic run home
    {
        int b = bass_near(root_pc(c, bar));
        if (step == 0)
            { schedule_hit(dly, b, I_BASS, 6, 300); vu += 3; }
        else if (step == 4 || step == 10)
            { schedule_hit(dly + rnd(2), b + 12, I_BASS, 5, 90); vu += 1.5f; }   // the pop
        else if (step == 8)
            { schedule_hit(dly + rnd(2), chance(50) ? b : b + 7, I_BASS, 5, 220); vu += 1.5f; }
        else if (step == 6 && chance(35))
            schedule_hit(dly + rnd(2), b + 12, I_BASS, 3, 70);                   // ghost pop
        else if (step >= 14) {                          // double 16th run into the change
            int nb = bass_peek(root_pc(chord_at(bar + 1), bar + 1));
            if (nb != b) {
                int from = nb > b ? nb - (16 - step) : nb + (16 - step);
                schedule_hit(dly + rnd(2), from, I_BASS, 4, 80);
                vu += 1;
            }
        }
    }

    // DRUMS — session tight
    {
        for (int i = 0; i < 5; i++)
            if (KICKP[sng.kick][i] == step)
                { schedule_hit(dly + rnd(2), 38, I_KICK, 6, 90); vu += 2; }
        if (step == 4 || step == 12)
            { schedule_hit(dly + rnd(2), 64, I_SNARE, 5, 60); vu += 2; }
        int hatEvery = (lvl >= 3) ? 1 : 2;
        if (step % hatEvery == 0) {
            bool open = (step == 14);                   // "tsss" on the and-of-4
            schedule_hit(dly + rnd(2), 92, I_HAT,
                         open ? 3 : (step % 4 == 2 ? 3 : 2), open ? 120 : 20);
        }
    }

    // GUITAR — chucks and stabs from level 1
    if (lvl >= 1) {
        int ch = CHUCK[sng.chuck][step];
        if (ch == 1) {                                  // muted scratch, two voices
            lead_voices(c, bar);
            for (int k = 1; k < 3; k++)
                schedule_hit(dly + rnd(2), gv[k], I_GTR, 2, 35);
            vu += 0.6f;
        } else if (ch == 2) {                           // accented stab
            lead_voices(c, bar);
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + k * 5 + rnd(2), gv[k], I_GTR, 3, 130);
            vu += 1.5f;
        }
    }

    // BRASS — the horn section hits the anticipation into the next bar
    if (lvl >= 2 && step == 15 && (bar % 2 == 1) && chance(55)) {
        lead_voices(chord_at(bar + 1), bar + 1);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + rnd(2), gv[k] + 12 > 88 ? gv[k] : gv[k] + 12, I_BRASS, 4, 150);
        vu += 2.5f;
    }

    // LEAD — the chorus hook, glittering up top. Lays out while the player has
    // the jam strip open (gating note-ons lets the sounding note ring to its end).
    if (lvl >= 2 && !solo_open()) {
        int s32 = (int)(s % 32);
        for (int i = 0; i < cellN; i++)
            if (cellOn[i] == s32 && chance(85)) {
                int gap = (i + 1 < cellN) ? cellOn[i + 1] - s32 : 32 - s32;
                int dur = (int)(gap * stepMs * 0.8);
                if (dur > 800) dur = 800;
                schedule_hit(dly + rnd(3), pick_mel(c, bar), I_LEAD, 3, dur);
                vu += 1.5f;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_GTR, INSTR_TRI, 0, 140, 1, 60);             // clean funk pluck
    instrument_env(I_GTR, 0, ENV_CUTOFF, 0, 60, 1200);       // scratch attack

    instrument(I_BASS, INSTR_TRI, 1, 160, 4, 70);            // bright pop bass
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 18, 6);          // the slap snap

    instrument(I_LEAD, INSTR_SQUARE, 6, 160, 5, 140);        // glossy synth
    instrument_duty(I_LEAD, 0.30f);
    instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.6f, 0.10f);

    instrument(I_BRASS, INSTR_SAW, 4, 180, 5, 120);          // the section
    instrument_env(I_BRASS, 0, ENV_PITCH, 0, 40, -2);        // the fall

    instrument(I_KICK, INSTR_SINE, 0, 80, 0, 30);
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 40, 14);

    instrument(I_SNARE, INSTR_NOISE, 0, 75, 0, 50);          // bright 80s crack
    instrument_filter(I_SNARE, FILTER_BAND, 2400, 5);
    instrument_env(I_SNARE, 0, ENV_PITCH, 0, 30, 10);

    instrument(I_HAT, INSTR_NOISE, 0, 16, 2, 70);            // sustain>0 so the open hat
    instrument_filter(I_HAT, FILTER_HIGH, 8000, 2);          // actually washes; low res — a
                                                             // resonant highpass on noise whistles

    instrument(I_SOLO, INSTR_SQUARE, 6, 170, 6, 220);        // the jam lead — glossy, sits on top
    instrument_duty(I_SOLO, 0.32f);
    instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.6f, 0.16f);       // a singing chorus-vibrato
    instrument_filter(I_SOLO, FILTER_LOW, 4200, 3);          // brighter than the comp lead
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        apply_voicing();
        if (CITYPOP_SEED) { new_song(pos, CITYPOP_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    // the shared input block (radio.h): feel/tempo/tone/help handled inside, the cart
    // reacts to the events. ntone=4 — citypop's mellow/warm/clear/bright tone knob.
    int ev = rad_input(&tempo, 96, 124, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_TONE)   apply_voicing();                 // re-aim the filters live
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) note_off_all();
        else scheduled = (long)pos;
    }

    if (radioOn) {
        long st;                           // schedule one step ahead of the clock
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= 64L * 16) fresh_song(pos);

        long bar = songStep >= 0 ? songStep / 16 : 0;
        const Ch *p = prog_of(bar);
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 8, p[i], bar);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long bar = ss >= 0 ? ss / 16 : 0;
    static const char *SN[4] = { "intro", "verse", "chorus", "outro" };
    watch("song", "%d", songCount);
    watch("sect", "%s", SN[sect_of(bar)]);
    watch("chord", "%s", nowChord[(int)(bar % 4)]);
    watch("geared", "%d", bar >= MOD_BAR ? 1 : 0);
#endif
}

// ── draw — the radio under the vaporwave sun (shared chassis from radio.h; the
// window art — the vaporwave sunset, sun, palm, neon skyline — stays citypop's
// own, and so does the white-plastic face with its pink/teal double pinstripe) ─
void draw(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();                                         // knobs are touch-draggable
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;

    // body — white 80s plastic, pink and teal trim. rad_body lays the shell +
    // black face; citypop overpaints a WHITE face + a second pinstripe (identity).
    rad_body(CLR_LIGHT_GREY, CLR_PINK);
    rectfill(24, 20, 272, 160, CLR_WHITE);
    line(24, 22, 295, 22, CLR_PINK);
    line(24, 24, 295, 24, CLR_BLUE);

    // dial strip — citypop's own black strip with pink ticks, teal needle
    rectfill(32, 28, 218, 16, CLR_BLACK);            // short of the tune button
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * RAD_DIAL_SP;
        line(x, 38, x, 42, CLR_DARK_GREY);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 30, CLR_PINK);
        }
    }
    int nx = 36 + (int)((rad_needle_freq(sng.freq) - 88.0f) * RAD_DIAL_SP);
    line(nx, 29, nx, 43, CLR_BLUE);
    rad_tuner_button(CLR_PINK);

    // the window — vaporwave sunset: banded sun, palm, neon skyline
    rectfill(34, 52, 102, 116, CLR_DARK_PURPLE);        // dusk
    rectfill(34, 84, 102, 24, CLR_DARK_RED);
    rectfill(34, 104, 102, 20, CLR_DARK_ORANGE);
    circfill(85, 106, 22, CLR_ORANGE);                  // the sun
    circfill(85, 102, 20, CLR_YELLOW);
    for (int i = 0; i < 4; i++)                         // its scanlines
        rectfill(50, 96 + i * 7, 70, 2 + i, CLR_DARK_ORANGE);
    // skyline silhouette + neon windows
    unsigned hsh = sng.seed;
    int sx = 36;
    while (sx < 130) {
        hsh = hsh * 1664525u + 1013904223u;
        int bw = 8 + (int)(hsh % 12);
        int bh = 18 + (int)((hsh >> 8) % 26);
        if (sx + bw > 134) bw = 134 - sx;
        rectfill(sx, 144 - bh, bw, bh + 24, CLR_DARKER_BLUE);
        if ((hsh >> 16) % 3 == 0)
            rectfill(sx + 2, 146 - bh, bw - 4, 2, (hsh >> 18) % 2 ? CLR_PINK : CLR_BLUE);
        sx += bw + 2;
    }
    // palm tree
    line(118, 166, 121, 138, CLR_BLACK);
    for (int f = 0; f < 5; f++) {
        float fa = -0.4f - f * 0.55f;
        line(121, 138, 121 + (int)(cosf(fa) * 12), 138 + (int)(sinf(fa) * 7), CLR_BLACK);
    }
    rect(34, 52, 102, 116, CLR_LIGHT_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_PINK);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_PINK);
        char l2[32];
        int dispKey = (sng.keyPc + (bar >= MOD_BAR ? 2 : 0)) % 12;
        snprintf(l2, 32, "%.1f FM  key %s%s", sng.freq, PCNAME[dispKey],
                 bar >= MOD_BAR ? "!" : "");
        print(l2, 154, 70, CLR_BLUE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_BLUE);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_PINK);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // the progression, current chord boxed
    if (radioOn) {
        int ci = (int)(bar % 4);
        int x = 152;
        for (int i = 0; i < 4; i++) {
            int cw = text_width(nowChord[i]);
            if (x + cw > 292) break;
            if (i == ci) {
                rectfill(x - 2, 102, cw + 4, 12, CLR_PINK);
                print(nowChord[i], x, 104, CLR_BLACK);
            } else
                print(nowChord[i], x, 104, CLR_DARK_GREY);
            x += cw + 8;
        }
        static const char *SN[4] = { "intro", "verse", "chorus", "outro" };
        print(bar >= MOD_BAR && sect_of(bar) == S_C ? "chorus+2!" : SN[sect_of(bar)],
              152, 120, CLR_PINK);
        for (int i = 0; i < 8; i++)
            circfill(232 + i * 7, 124, 1, i <= bar / 8 ? CLR_PINK : CLR_DARK_GREY);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "4am", "cruise", "downtown", "neon" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_PINK);
    if (rad_knob_int(&tempo, 96, 124, 2, 218, 148, 9, "tempo", CLR_PINK)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, TONENAME[toneSel], CLR_PINK)) apply_voicing();
    rad_power_led(radioOn, CLR_RED, CLR_DARK_RED);

    rad_help_button(CLR_PINK);
    if (showHelp) {
        static const char *HELP[9][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same song again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - shifts the density curve" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
            { "L",          "jam ch/sc: chord-lock / free" },
        };
        static const char *NOTES[3] = {
            "the #number on the display IS the song.",
            "pin it for good: #define CITYPOP_SEED 0x...",
            "the last chorus modulates up. of course.",
        };
        rad_help_panel("CITY POP RADIO", HELP, 9, NOTES, 3, CLR_PINK);
    }

    // the jam strip — CHORD-LOCK + CHORD-TRIGGER (Omnichord): every cell is the
    // live chord's own tones, re-shaped each chord change as the Royal Road and the
    // +2 gear change move under you. A press fires the whole TRIAD as a Juno-style
    // comp stab; the station holds the harmony, you just strum it — vertical drag
    // = strum spread (top = tight block, bottom = a harp roll). You can't miss.
    int chord[4]; {
        Ch c = chord_at(bar);
        for (int k = 0; k < 4; k++) chord[k] = (root_pc(c, bar) + QT[c.q][k]) % 12;
    }
    int soloRoot = (sng.keyPc + (bar >= MOD_BAR ? 2 : 0)) % 12;
    static const int PENT[5] = { 0, 2, 4, 7, 9 };  // the "sc" toggle's fallback: free pentatonic over the changes
    //                root      scale nsc chord nc instr   lo  hi  quant  ymode(unused) yMin yMax struck chordLock chordTrig
    SoloCtx jc = { soloRoot, PENT, 5, chord, 4, I_SOLO, 72, 91, false, SOLO_Y_OFF, 0, 0, false, true, true };
    solo_strip(&jc, 28, 170, 250, 18, CLR_PINK);

    ui_end();
}
