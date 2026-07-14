/* de:meta
{
  "slug": "ymo",
  "collection": ["radio"],
  "title": "ymo radio",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "step-sequencer"
  ],
  "lineage": "Eighth radio.h station; lifts drum voice circuits verbatim from cr78.c and adds a Hosono melodic bassline generator and Sakamoto harmony templates.",
  "homage": "Yellow Magic Orchestra (techno-kayo)",
  "description": "Endless techno-kayo - the Yellow Magic Orchestra homage (Hosono/Sakamoto/Takahashi), eighth of the radio family, and the one genre where the synth isn't imitating anything: square waves and white noise ARE the original instruments. Sakamoto harmony as templates (the chromatic mediant 'Tong Poo' pull, maj7 planing in whole steps, the kayo minor cadence with its harmonic-minor V), a HOSONO BASSLINE GENERATOR (melodic 8th-note counterpoint with directional inertia and octave leaps - the bassline is a second melody), yonanuki pentatonic lead, a sequencer arp ticking chord tones, and drum voices ON LOAN FROM THE CR-78 CART (cr78.c's circuit recipes: soft round kick, two-layer snare, the swishy hats, the metallic clang) driven by machine-tight pattern strings. No swing, no lag - it's 1979 and the machine is the point. Seed on display (YMO_SEED / R / [ ] history), T tone, H help. Worked example #8 for docs/guides/game-music.md."
}
de:meta */
#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include <stdio.h>
#include <math.h>

// ── YMO RADIO ─────────────────────────────────────────────────────────────
// The eighth station: endless techno-kayō — the Yellow Magic Orchestra
// homage (Hosono / Sakamoto / Takahashi). The one genre where this synth
// isn't imitating the source instruments: square waves and white noise ARE
// the original instruments.
//
//   • SAKAMOTO HARMONY as templates — the chromatic mediant pull (I to III
//     major, the "Tong Poo" move), maj7 PLANING (parallel maj7 chords
//     sliding in whole steps), the kayō minor cadence (i-iv-bVI-V, that V
//     borrowed from harmonic minor — the tear-jerker), and bright kayō
//     major (I-bVII-IV-V).
//   • the HOSONO BASSLINE GENERATOR — not root-thumping: a melodic 8th-note
//     counterpoint that walks chord tones with directional inertia, octave
//     drops, and a seeded rhythm mask. The bassline is a second melody.
//   • YONANUKI lead — the Japanese pentatonic (do-re-mi-so-la / its minor
//     twin). The melody cell sings in it; that scale IS the kayō color.
//   • the SEQUENCER — an arpeggio layer ticking chord tones on 8ths (16ths
//     when it's cooking): the Technodelic conveyor belt.
//   • CR-78 DRUMS — the voice circuits are lifted from tools/carts/cr78.c
//     (the Compurhythm homage): soft round kick, the two-layer snare
//     (tonal shell + bandpassed rattle), the famous swishy hats, and the
//     metallic beat (detuned filtered squares — the Kraftwerkian clang).
//     Patterns are machine-tight 16-step rows in cr78.c's string format.
//     No swing, no lag, ±2ms. It's 1979 and the machine is the point.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help
//
// Density = arrangement curve x feel shift (game-music.md). Seed pins the
// COMPOSITION (key, templates, bass mask, lead cell, kit pattern, title);
// the PERFORMANCE (bass contour, lead path, fills) re-rolls every listen.

#define YMO_SEED 0   // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_BASS  5   // the Hosono counterpoint
#define I_ARP   6   // the sequencer
#define I_LEAD  7   // yonanuki melody
// drum slots 9..15 — recipes from cr78.c
#define SL_KICK 9
#define SL_SNB  10
#define SL_SNN  11
#define SL_HATC 12
#define SL_HATO 13
#define SL_MET  14
#define SL_CLV  15

// ── chords ────────────────────────────────────────────────────────────────
enum { Q_MAJ, Q_MAJ7, Q_MIN, Q_MIN7, NQ };
static const char *QN[NQ] = { "", "maj7", "m", "m7" };
static const int QV[NQ][3] = {
    { 4, 7, 12 },    // maj — plain triad, the synthpop block chord
    { 4, 7, 11 },    // maj7 — for the planing
    { 3, 7, 12 },    // m
    { 3, 7, 10 },    // m7
};
static const int QT[NQ][4] = {
    { 0, 4, 7, 12 }, { 0, 4, 7, 11 }, { 0, 3, 7, 12 }, { 0, 3, 7, 10 },
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
// yonanuki pentatonics — THE kayō melody scales
static const int PENTMAJ[5] = { 0, 2, 4, 7, 9 };    // do re mi so la
static const int PENTMIN[5] = { 0, 3, 5, 7, 10 };   // the minor twin

// ── progression templates — the Sakamoto playbook ─────────────────────────
typedef struct { int off, q; } Ch;
static const Ch TMPL[5][4] = {
    // the mediant pull: I | IIImaj (chromatic mediant!) | IV | V
    { { 0, Q_MAJ7 }, { 4, Q_MAJ }, { 5, Q_MAJ7 }, { 7, Q_MAJ } },
    // maj7 planing: parallel maj7s sliding in whole steps
    { { 0, Q_MAJ7 }, { 10, Q_MAJ7 }, { 8, Q_MAJ7 }, { 10, Q_MAJ7 } },
    // the minor drive: i | i | bVI | bVII (Technopolis energy)
    { { 0, Q_MIN }, { 0, Q_MIN }, { 8, Q_MAJ }, { 10, Q_MAJ } },
    // bright kayō: I | bVII | IV | V
    { { 0, Q_MAJ }, { 10, Q_MAJ }, { 5, Q_MAJ }, { 7, Q_MAJ } },
    // kayō minor cadence: i | iv | bVI | V — the harmonic-minor V, the tear
    { { 0, Q_MIN7 }, { 5, Q_MIN7 }, { 8, Q_MAJ7 }, { 7, Q_MAJ } },
};
static const int VERSE_POOL[3]  = { 2, 4, 1 };
static const int CHORUS_POOL[3] = { 0, 3, 1 };

enum { S_INTRO, S_V, S_C, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_V, S_V, S_C, S_V, S_C, S_C, S_OUTRO };

// ── drum machine patterns, in cr78.c's string format (16 steps) ───────────
typedef struct {
    const char *kick, *snare, *hatc, *hato, *metal, *clv;
} Kit;
static const Kit KITS[3] = {
    // "technopolis": four on the floor, offbeat open hat, the clang on 1
    { "x...x...x...x...", "....x.......x...", "xx.xxx.xxx.xxx.x",
      "..x...x...x...x.", "x...............", NULL },
    // "rydeen gallop": driving 8th kick pairs, claves answering
    { "x..x..x.x..x..x.", "....x.......x...", "x.x.x.x.x.x.x.x.",
      "..............x.", NULL, "..x...x...x..x.." },
    // "bgm": sparser, the machine breathing
    { "x.....x...x.....", "....x.......x...", "x.x.x.x.x.x.x.x.",
      "......x.........", "........x.......", NULL },
};
static const char *KITNAME[3] = { "technopolis", "gallop", "bgm" };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    Ch   verse[4], chorus[4];
    int  kit;
    int  minorKey;            // picks the yonanuki twin for the lead
    int  bassMask;            // 8 bits: which 8ths the bassline plays
    int  cellOn[5], cellN;    // lead cell
    char title[24];
    float freq;
    unsigned seed;
} Song;

// composition PRNG + session history live in radio.h (RadioSeed rs); srnd is the
// SAME xorshift stream as before the migration — pinned seeds depend on it byte
// for byte. performance jitter keeps engine rnd().
static Song       sng;
static RadioSeed  rs;                       // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 125.0 };   // schedule-ahead step clock (radio.h)
// the clock's fields under their pre-migration names — keeps the body textually
// unchanged (smallest possible diff over the original)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)    rad_srnd(&rs, (n))
static int    tempo     = 120;
static int    intensity = 1;     // feel: shifts the arrangement's density curve
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    songCount = 0;
static int    gv[3]     = { 64, 67, 71 };
static bool   gvInit    = false;
static float  vu        = 0;
static int    melPitch  = 81;
static int    bassWalk  = 45;    // the Hosono line's walker
static int    bassDir   = 1;
static char   nowChord[4][8];

static int iabs(int v) { return v < 0 ? -v : v; }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Techno", "Cosmic", "Solid", "Neon", "Tokio",
    "Magnetic", "Digital", "Oriental", "Crystal", "Future", "Gamma", "Citizen" };
static const char *TW2[] = { "Polis", "Surfin", "Express", "Garden", "Signal",
    "Holiday", "Picnic", "Survivor", "Game", "Light", "Train", "Mirage" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);   // 0 = derive fresh (same expression as ever)

    sng.keyPc = srnd(12);
    int vi = VERSE_POOL[srnd(3)], ci;
    do { ci = CHORUS_POOL[srnd(3)]; } while (ci == vi);
    for (int k = 0; k < 4; k++) { sng.verse[k] = TMPL[vi][k]; sng.chorus[k] = TMPL[ci][k]; }
    sng.minorKey = (sng.verse[0].q == Q_MIN || sng.verse[0].q == Q_MIN7);

    sng.kit = srnd(3);

    // the Hosono mask: which 8ths the bassline speaks on (always the one)
    sng.bassMask = 1;
    for (int b = 1; b < 8; b++)
        if (srnd(100) < 70) sng.bassMask |= 1 << b;

    sng.cellN = 0;                                       // lead cell
    for (int s = 0; s < 31 && sng.cellN < 5; s += 2)
        if (srnd(100) < (s % 8 == 0 ? 32 : 38)) sng.cellOn[sng.cellN++] = s;
    if (sng.cellN < 3) { sng.cellN = 3; sng.cellOn[0] = 0; sng.cellOn[1] = 8; sng.cellOn[2] = 20; }

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 112 + srnd(21);                              // 112..132
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;
    melPitch = 81;
    bassWalk = 45;
    bassDir  = 1;
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
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }

static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", PCNAME[root_pc(c)], QN[c.q]);
}

// density = arrangement curve + feel shift (see game-music.md)
//   0: bass + drums          2: + the lead and the clang
//   1: + the sequencer arp   3: + 16th arp, denser hats
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_V ? 1 : 2);
    int lvl = base + intensity - 1;
    return lvl < 0 ? 0 : lvl > 3 ? 3 : lvl;
}

// ── the tone knob (T cycles) — master brightness, re-issued live ──────────
// RAD_TONENAME / RAD_TONEMUL live in radio.h (the same 4-step scale)
static int toneSel = 2;

static void apply_voicing(void) {
    float m = RAD_TONEMUL[toneSel];
    instrument_filter(I_ARP,  FILTER_LOW, (int)(2200 * m), 2);
    instrument_filter(I_LEAD, FILTER_LOW, (int)(2600 * m), 2);
    instrument_filter(I_BASS, FILTER_LOW, (int)(1100 * (0.7f + 0.3f * m)), 2);
}

// nearest-tone voice leading — rad_lead_to (radio.h) is the shared block, the
// single biggest "sounds composed" trick (used by the arp); this just resolves
// the chord into root + the quality's voicing intervals. arp register: 57..81.
static void lead_voices(Ch c) {
    rad_lead_to(root_pc(c), QV[c.q], gv, 3, 57, 81, &gvInit);
}

// the HOSONO LINE — melodic counterpoint, not root-thumping. Walks chord
// tones with directional inertia; flips at the range edges; sometimes leaps
// an octave because Harry felt like it.
static int hosono_note(Ch c) {
    if (chance(22)) bassDir = -bassDir;
    int bestM = bassWalk, bestScore = -999;
    for (int t = 0; t < 4; t++) {
        int pc = (root_pc(c) + QT[c.q][t]) % 12;
        for (int oct = 3; oct <= 4; oct++) {
            int m = oct * 12 + pc;
            if (m < 36 || m > 55) continue;
            int score = 12 - iabs(m - (bassWalk + bassDir * 2)) + rnd(3);
            if (m == bassWalk) score -= 4;               // keep moving
            if (score > bestScore) { bestScore = score; bestM = m; }
        }
    }
    if (chance(12)) bestM += (bestM > 45 ? -12 : 12);    // the octave leap
    if (bestM < 36) bestM += 12;
    if (bestM > 55) bestM -= 12;
    if (bestM >= bassWalk + 3) bassDir = 1;
    else if (bestM <= bassWalk - 3) bassDir = -1;
    bassWalk = bestM;
    return bestM;
}

// yonanuki lead: the pentatonic of the key, nearest the walker
static int pick_mel(void) {
    const int *sc = sng.minorKey ? PENTMIN : PENTMAJ;
    int bestM = melPitch, bestScore = -999;
    for (int d = 0; d < 5; d++) {
        int pc = (sng.keyPc + sc[d]) % 12;
        for (int oct = 6; oct <= 7; oct++) {
            int m = oct * 12 + pc;
            if (m < 74 || m > 91) continue;
            int score = 10 - iabs(m - melPitch) + rnd(4);
            if (m == melPitch) score -= 3;
            if (score > bestScore) { bestScore = score; bestM = m; }
        }
    }
    melPitch = bestM;
    return bestM;
}

// ── the step player — machine tight, ±2ms ────────────────────────────────
static void hitrow(const char *row, int step, int dly, int midi, int slot, int vol, int dur) {
    if (row && row[step] == 'x') schedule_hit(dly + rnd(2), midi, slot, vol, dur);
}

static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    Ch   c    = chord_at(bar);
    int  lvl  = level_of(bar);
    const Kit *kit = &KITS[sng.kit];

    // DRUMS — the CR-78 circuits, driven by the pattern strings
    hitrow(kit->kick, step, dly, 33, SL_KICK, 6, 220);
    if (kit->snare && kit->snare[step] == 'x') {         // the two-layer snare
        schedule_hit(dly + rnd(2), 53, SL_SNB, 4, 100);
        schedule_hit(dly + rnd(2), 60, SL_SNN, 4, 170);
        vu += 2;
    }
    int hatVol = (step % 4 == 0) ? 3 : 2;
    if (lvl >= 3 || (kit->hatc && kit->hatc[step] == 'x'))
        schedule_hit(dly + rnd(2), 90, SL_HATC, hatVol, 60);
    hitrow(kit->hato, step, dly, 90, SL_HATO, 3, 360);
    hitrow(kit->clv,  step, dly, 97, SL_CLV, 4, 55);
    if (lvl >= 2 && kit->metal && kit->metal[step] == 'x' && bar % 2 == 0) {
        schedule_hit(dly + rnd(2), 88, SL_MET, 3, 210);  // the Kraftwerkian clang
        schedule_hit(dly + rnd(2), 93, SL_MET, 2, 210);  // (two of cr78.c's three
        vu += 1.5f;                                      //  squares — voice budget)
    }
    if (kit->kick && kit->kick[step] == 'x') vu += 2;

    // THE HOSONO LINE — masked 8ths, melodic counterpoint
    if (step % 2 == 0 && (sng.bassMask >> (step / 2)) & 1) {
        int n;
        if (step == 0) {                                 // land the bar on the root
            n = 36 + root_pc(c);
            while (n > 50) n -= 12;
            while (n < 36) n += 12;
            bassWalk = n;
        } else
            n = hosono_note(c);
        schedule_hit(dly + rnd(2), n, I_BASS, 5, (int)(stepMs * 1.7));
        vu += 1.5f;
    }

    // THE SEQUENCER — arp ticking chord tones; 16ths when it's cooking
    if (lvl >= 1) {
        int arpEvery = (lvl >= 3) ? 1 : 2;
        if (step % arpEvery == 0) {
            lead_voices(c);
            int v = gv[(step / arpEvery) % 3] + 12;
            if (v > 93) v -= 12;
            schedule_hit(dly + rnd(2), v, I_ARP, 2, (int)(stepMs * 0.9));
            vu += 0.5f;
        }
    }

    // THE LEAD — yonanuki melody, square and proud
    if (lvl >= 2) {
        int s32 = (int)(s % 32);
        for (int i = 0; i < sng.cellN; i++)
            if (sng.cellOn[i] == s32 && chance(85)) {
                int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - s32 : 32 - s32;
                int dur = (int)(gap * stepMs * 0.8);
                if (dur > 700) dur = 700;
                schedule_hit(dly + rnd(2), pick_mel(), I_LEAD, 3, dur);
                vu += 1.5f;
            }
    }
}

// ── setup — drum recipes lifted from tools/carts/cr78.c ───────────────────
static void setup_instruments(void) {
    instrument(I_BASS, INSTR_SQUARE, 1, 140, 4, 60);         // synth bass, not upright
    instrument_duty(I_BASS, 0.35f);
    instrument_env(I_BASS, 0, ENV_CUTOFF, 0, 70, 700);

    instrument(I_ARP, INSTR_TRI, 0, 70, 2, 40);              // the conveyor belt

    instrument(I_LEAD, INSTR_SQUARE, 4, 150, 5, 120);        // the proud square
    instrument_duty(I_LEAD, 0.25f);
    instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.6f, 0.12f);

    // — CR-78 circuits, verbatim from cr78.c —
    instrument(SL_KICK, INSTR_SINE, 0, 170, 0, 40);          // soft round kick
    instrument_filter(SL_KICK, FILTER_LOW, 320, 2);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 45, 13.0f);

    instrument(SL_SNB, INSTR_SINE, 0, 70, 0, 25);            // snare shell
    instrument_filter(SL_SNB, FILTER_LOW, 900, 1);
    instrument_env(SL_SNB, 0, ENV_PITCH, 0, 25, 5.0f);
    instrument(SL_SNN, INSTR_NOISE, 0, 130, 0, 35);          // snare rattle
    instrument_filter(SL_SNN, FILTER_BAND, 1700, 3);
    instrument_env(SL_SNN, 0, ENV_CUTOFF, 0, 90, 900.0f);

    instrument(SL_HATC, INSTR_NOISE, 0, 40, 0, 18);          // the swish
    instrument_filter(SL_HATC, FILTER_BAND, 7800, 4);
    instrument(SL_HATO, INSTR_NOISE, 0, 320, 0, 80);
    instrument_filter(SL_HATO, FILTER_BAND, 7200, 4);

    instrument(SL_MET, INSTR_SQUARE, 0, 210, 0, 60);         // the metallic beat
    instrument_filter(SL_MET, FILTER_BAND, 3100, 6);

    instrument(SL_CLV, INSTR_TRI, 0, 36, 0, 14);             // claves
    instrument_filter(SL_CLV, FILTER_LOW, 3600, 6);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        apply_voicing();
        if (YMO_SEED) { new_song(pos, YMO_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    // the shared input block (radio.h): feel/tempo/tone/help handled inside,
    // the cart reacts to the events. ntone=4 — the tone knob re-aims the filters.
    int ev = rad_input(&tempo, 104, 140, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_TONE)   apply_voicing();
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
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 8, p[i]);
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
    watch("kit", "%s", KITNAME[sng.kit]);
#endif
}

// ── draw — the rising sun console (shared chassis from radio.h; the window
// art — the rising sun over the BGM dot grid — stays YMO's own) ────────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();                                         // knobs are touch-draggable
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;

    rad_body(CLR_DARKER_GREY, CLR_RED);   // lacquer black, one red line (the bow ties)
    rad_dial(sng.freq, CLR_RED);

    // the window — the rising sun over a dot-matrix grid
    rectfill(34, 52, 102, 116, CLR_BLACK);
    float breathe = vu / 12.0f;
    circfill(85, 102, 26 + (int)(breathe * 3), CLR_RED);     // the sun, breathing
    circfill(85, 102, 22 + (int)(breathe * 3), CLR_DARK_RED);
    circfill(85, 102, 16, CLR_RED);
    for (int gy2 = 58; gy2 < 164; gy2 += 8)                  // the BGM dot grid
        for (int gx2 = 40; gx2 < 132; gx2 += 8) {
            int dx = gx2 - 85, dy2 = gy2 - 102;
            if (dx * dx + dy2 * dy2 > 750)
                pset(gx2, gy2, ((gx2 + gy2) / 8 + frame() / 30) % 5 ? CLR_DARK_GREY : CLR_RED);
        }
    rect(34, 52, 102, 116, CLR_DARKER_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_RED);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_RED);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq, KITNAME[sng.kit]);
        print(l2, 154, 70, CLR_DARK_RED);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_DARK_RED);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_RED);
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
                rectfill(x - 2, 102, cw + 4, 12, CLR_RED);
                print(nowChord[i], x, 104, CLR_BLACK);
            } else
                print(nowChord[i], x, 104, CLR_LIGHT_GREY);
            x += cw + 8;
        }
        static const char *SN[4] = { "intro", "verse", "chorus", "outro" };
        print(SN[sect_of(bar)], 152, 120, CLR_RED);
        rad_phrase_dots(232, 124, 8, bar / 8, CLR_RED);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "bgm", "kayo", "techno", "rydeen" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_RED);
    if (rad_knob_int(&tempo, 104, 140, 2, 218, 148, 9, "tempo", CLR_RED)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_RED)) apply_voicing();
    rad_power_led(radioOn, CLR_RED, CLR_DARK_RED);

    rad_help_button(CLR_RED);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same song again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - shifts the density curve" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        static const char *NOTES[3] = {
            "the #number on the display IS the song.",
            "pin it for good: #define YMO_SEED 0x...",
            "drum circuits on loan from the cr-78 cart",
        };
        rad_help_panel("YMO RADIO", HELP, 8, NOTES, 3, CLR_RED);
    }

    ui_end();
}
