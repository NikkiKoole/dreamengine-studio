// ── TANGO RADIO ───────────────────────────────────────────────────────────
// The fifteenth station: endless tango — the Golden Age orquesta típica
// homage (D'Arienzo's drive, Pugliese's yumba, Troilo's singing bandoneón).
// Nobody dances alone, and the band breathes together.
//
// THE NEW BRAIN — TEMPO AS A VOICE (time brain): the first station where the
// clock itself is an instrument. A per-frame CONDUCTOR eases a rubato
// multiplier toward phrase-shaped targets and drives bpm() live — a lean
// forward through every phrase, a real ritardando into every 8-bar section
// boundary with a hard a-tempo SNAP on the next downbeat (rising eases fast,
// falling eases slow — the orquesta snaps back together), and a long
// rallentando into the ending. Depth is seeded: every song breathes its own
// amount. The schedule-ahead clock survives untouched — pos derives from the
// engine's own beat clock, exactly as the guide promised since bossa.
//
// Plus the tango kit nobody else has:
//   • MARCATO / SÍNCOPA alternation — the floor rolls per 4-bar phrase
//     between four heavy quarters and the 3-3-2 anticipation (the accent ON
//     the and-of-2: the síncopa IS the accent).
//   • THE ARRASTRE — the chromatic bass drag into accented downbeats: three
//     crescendo grace notes smeared into beat one.
//   • CHICHARRA / GOLPE — no drum kit at all (like satie): the percussion is
//     the band — a scratch behind the violin's bridge, knuckles on the box.
//   • THE CHAN-CHAN — every song actually ENDS: rallentando, near-silence,
//     V7b9... i. Then the next record drops.
//
// Harmony: a minor functional walk in the harmonic-minor dialect (iiø→V7b9
// cadence forced into bars 6-7, the Neapolitan bII, an Andalusian-tetrachord
// flavor roll) with PARALLEL-MAJOR cantabile B sections — the violin takes
// the song. Form: intro A B A B A VARIACIÓN (continuous-16th bandoneón runs)
// final. The bandoneón is wave_set's second station gig: a drawn free-reed
// cycle with an LFO_VOLUME bellows tremble. Watch the window: the bellows
// breathe WITH the rubato curve — the new brain, visible.
//
//   SPACE next tango   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define TANGO_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_BAND   5   // bandoneón left hand — INSTR_USER0 drawn reed, chords
#define I_BANDL  6   // bandoneón right hand — the canto + the variación
#define I_VLN    7   // the violins, one desk — saw + vibrato + the scoop
#define I_PNO    8   // piano — marcato left hand / cantabile arpeggios
#define I_BASS   9   // the upright, marcato
#define SL_CHIC 10   // chicharra — the scratch behind the bridge
#define SL_GLP  11   // golpe — knuckles on the bandoneón box

// ── harmony — harmonic-minor dialect, parallel-major B sections ───────────
enum { Q_MIN7, Q_MIN6, Q_M7B5, Q_DOM7, Q_DOM7B9, Q_MAJ7, NQ };
static const char *QN[NQ] = { "m7", "m6", "m7b5", "7", "7b9", "maj7" };
static const int QV[NQ][3] = {
    { 3, 10, 14 },   // m7
    { 3,  9, 14 },   // m6   — the tango tonic color
    { 3,  6, 10 },   // m7b5 — the iiø of the cadence
    { 4, 10, 14 },   // 7
    { 4, 10, 13 },   // 7b9  — the tango dominant (harmonic minor's gift)
    { 4, 11, 14 },   // maj7
};
// functions: offsets from the MINOR tonic; FM_* live in the parallel major
enum { F_i, F_iiq, F_iv, F_V, F_bVI, F_bVII, F_bII, F_I7, F_III,
       FM_I, FM_ii, FM_IV, FM_V, FM_vi, FM_II7, NF };
static const int F_OFF[NF]  = { 0, 2, 5, 7, 8, 10, 1, 0, 3,  0, 2, 5, 7, 9, 2 };
static const int F_QUAL[NF] = {
    Q_MIN6, Q_M7B5, Q_MIN7, Q_DOM7B9, Q_MAJ7, Q_DOM7, Q_MAJ7, Q_DOM7, Q_MAJ7,
    Q_MAJ7, Q_MIN7, Q_MAJ7, Q_DOM7,  Q_MIN7, Q_DOM7 };
// where can each function go? repeats = more likely (the tango cheat-sheet:
// iv→V→i pull, bVI→V, the Neapolitan resolving to V, I7 = V/iv)
static const int FNEXT[NF][6] = {
    { F_iv, F_V, F_iiq, F_bVI, F_bVII, F_I7 },    // i
    { F_V, F_V, F_V, F_bII, F_V, F_V },           // iiø -> V
    { F_V, F_iiq, F_i, F_bVII, F_V, F_bII },      // iv
    { F_i, F_i, F_i, F_bVI, F_i, F_i },           // V -> home (rarely deceptive)
    { F_V, F_iiq, F_bII, F_V, F_bVII, F_V },      // bVI -> V (the classic descent)
    { F_bVI, F_III, F_bVI, F_i, F_bVI, F_iv },    // bVII
    { F_V, F_V, F_i, F_V, F_V, F_iiq },           // bII (Neapolitan) -> V
    { F_iv, F_iv, F_iv, F_iv, F_iv, F_iv },       // I7 = V/iv
    { F_iv, F_bVI, F_iiq, F_iv, F_bVII, F_iv },   // III
    { FM_IV, FM_vi, FM_ii, FM_II7, FM_IV, FM_V }, // I (major B section)
    { FM_V, FM_V, FM_V, FM_V, FM_II7, FM_V },     // ii
    { FM_V, FM_I, FM_ii, FM_V, FM_vi, FM_V },     // IV
    { FM_I, FM_I, FM_I, FM_vi, FM_I, FM_I },      // V -> home
    { FM_ii, FM_IV, FM_ii, FM_II7, FM_ii, FM_IV },// vi
    { FM_V, FM_V, FM_V, FM_V, FM_V, FM_V },       // II7 -> V
};
static const int TETRA[4] = { F_i, F_bVII, F_bVI, F_V };   // the Andalusian descent

// ── the form — A A B A + the variación, like the old charts ──────────────
enum { S_INTRO, S_A, S_B, S_VAR, S_FINAL };
static const int FORM[8] = { S_INTRO, S_A, S_B, S_A, S_B, S_A, S_VAR, S_FINAL };
static const char *SECTNAME[5] = { "intro", "tango", "cantabile", "variacion", "final" };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc, cancion, yumba, andalus;
    int  fn[64];               // the walk, one function per bar, pre-rolled
    int  feel[16];             // per 4-bar phrase: 0 = marcato, 1 = síncopa
    int  melOn[6], melN;       // the canto: onsets in a 32-step (2-bar) cell
    float rubDepth;            // how much this song breathes (seeded)
    int  cutBand, cutBandL, cutVln, cutPno;   // rolled filter bases (T multiplies)
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 125.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

static int    tempo     = 120;   // the BASE — the conductor multiplies it
static int    liveBpm   = 120;   // what bpm() was last told (the breathing number)
static float  rubMul    = 1.0f;  // the conductor's current multiplier
static int    intensity = 1;
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    toneSel   = 2;
static int    songCount = 0;
static float  vu        = 0;
static int    bassLast  = 33;
static int    melLast   = 72;
static int    varLast   = 74, varDir = 1;
static int    melAnim   = 0;     // frames since the canto sang (button glints)
static char   nowChord[4][12];

static int  gvBand[3] = { 62, 65, 69 };
static bool bandInit = false;

#define srnd(n) rad_srnd(&rs, (n))

// ── song generation — composition AND the band, all from the seed ─────────
static const char *TW1[] = { "Tango", "Luna", "Noche", "Calle", "Sombra", "Humo",
    "Alma", "Fuego", "Barrio", "Viento", "Adios", "Milonga" };
static const char *TW2[] = { "del Sur", "de Ayer", "de Plata", "del Barrio",
    "de Humo", "de Abril", "del Alba", "del Rio", "Porteno", "de Carmin",
    "del Adios", "Querido" };

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_BAND,  FILTER_LOW, (int)(sng.cutBand  * tm), 1);
    instrument_filter(I_BANDL, FILTER_LOW, (int)(sng.cutBandL * tm), 1);
    instrument_filter(I_VLN,   FILTER_LOW, (int)(sng.cutVln   * tm), 1);
    instrument_filter(I_PNO,   FILTER_LOW, (int)(sng.cutPno   * tm), 1);
}

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.keyPc   = srnd(12);
    sng.cancion = srnd(100) < 25;                   // the slow, deep-breathing take
    sng.andalus = srnd(100) < 25;                   // i-bVII-bVI-V, locked
    sng.yumba   = srnd(100) < 30;                   // the Pugliese thud
    sng.rubDepth = (sng.cancion ? 0.85f : 0.55f) + srnd(35) * 0.01f;

    // pre-roll the WHOLE walk: weighted steps, the cadence FORCED into bars
    // 6-7 of every 8 (iiø -> V7b9, or the Neapolitan), tonic on every section
    int f = F_i;
    for (int b = 0; b < 64; b++) {
        int  sct = FORM[b / 8];
        bool mj  = sct == S_B;                      // cantabile = parallel major
        int  ib  = b % 8;
        if (sng.andalus && !mj)  f = TETRA[b % 4];
        else if (ib == 6)        f = mj ? FM_ii : F_iiq;
        else if (ib == 7)        f = mj ? FM_V : (srnd(100) < 22 ? F_bII : F_V);
        else if (ib == 0)        f = mj ? FM_I : F_i;
        else                     f = FNEXT[f][srnd(6)];
        sng.fn[b] = f;
    }

    // the canto: 3..6 onsets in 2 bars, on-beat-leaning (tango sings square,
    // the rubato supplies the lean)
    sng.melN = 0;
    static const int MC[14] = { 0, 2, 4, 6, 8, 11, 12, 14, 16, 19, 22, 24, 27, 28 };
    for (int i = 0; i < 14 && sng.melN < 6; i++)
        if (srnd(100) < 32) sng.melOn[sng.melN++] = MC[i];
    if (sng.melN < 3) {
        sng.melN = 4;
        sng.melOn[0] = 0; sng.melOn[1] = 6; sng.melOn[2] = 12; sng.melOn[3] = 22;
    }

    // marcato / síncopa, rolled per 4-bar phrase (yumba songs stay heavier)
    for (int i = 0; i < 16; i++) sng.feel[i] = srnd(100) < (sng.yumba ? 30 : 45);

    // THE BAND, rolled per song: reed brightness, bellows tremble, the
    // violin's vibrato + scoop, piano voicing dust (two SPACE = two records)
    sng.cutBand  = 1500 + srnd(500);
    sng.cutBandL = 2300 + srnd(700);
    sng.cutVln   = 2100 + srnd(700);
    sng.cutPno   = 1800 + srnd(600);
    instrument_filter(I_BASS, FILTER_LOW, 430 + srnd(160), 1);
    instrument_lfo(I_BAND, 0, LFO_VOLUME, 4.2f + srnd(18) * 0.1f, 0.05f + srnd(6) * 0.01f);
    instrument_lfo(I_VLN,  0, LFO_PITCH,  5.0f + srnd(14) * 0.1f, 0.10f + srnd(8) * 0.01f);
    instrument_env(I_VLN, 0, ENV_PITCH, 0, 70 + srnd(50), -(1.0f + srnd(12) * 0.1f));
    apply_tone();

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = sng.cancion ? 100 + srnd(13) : 116 + srnd(21);   // 100..112 / 116..136
    bpm(tempo);
    liveBpm = tempo;
    rubMul  = 1.0f;
    songBase = (long)pos + 8;
    bandInit = false;
    bassLast = 33; melLast = 72; varLast = 74; varDir = 1;
    songCount++;
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ────────────────────────────────────────────────────────
static int sect_of(long bar) { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_FINAL); }
static int fn_at(long bar)   { return sng.fn[bar < 0 ? 0 : bar % 64]; }
static int root_pc(int f)    { return (sng.keyPc + F_OFF[f]) % 12; }
static int qual(int f)       { return F_QUAL[f]; }

static void chord_label(char *out, int n, int f) {
    snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(f)], QN[qual(f)]);
}

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO) ? 0 : (s == S_VAR ? 2 : 1);
    return rad_level(base, intensity);
}

// ══ TEMPO AS A VOICE — the conductor's curve ══════════════════════════════
// Target multiplier as a function of the fractional bar position. Phrases
// lean forward then relax (a sine breath over 4 bars, deeper in cantabile);
// bar 7 of every section is a real ritardando; the last two bars of the song
// are the rallentando into the chan-chan. update() eases rubMul toward this
// (fast up, slow down: the a-tempo SNAP) and feeds bpm() live.
static float rub_target(double barF) {
    if (barF >= 64.0) return 1.0f;
    long  bar  = (long)barF;
    float frac = (float)(barF - bar);
    float d    = sng.rubDepth;
    if (bar >= 62)                                   // the final rallentando
        return 1.0f - 0.04f - (float)(barF - 62.0) * 0.09f;
    if (bar % 8 == 7)                                // section-end ritardando
        return 1.0f - (0.03f + 0.11f * frac) * d;
    float ph = ((bar % 4) + frac) / 4.0f;            // the phrase breath
    float depth = sect_of(bar) == S_B ? 0.045f : 0.015f;
    return 1.0f + sinf(ph * 6.2832f) * depth * d;
}

// ── voices ────────────────────────────────────────────────────────────────
static int bass_peek(int pc) {
    int d = ((pc - bassLast) % 12 + 18) % 12 - 6;
    int n = bassLast + d;
    while (n < 26) n += 12;
    while (n > 43) n -= 12;
    return n;
}
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

// the canto: nearest chord tone to where the line sits (the re-pitched cell)
static int mel_note(int f, int reg) {
    int best = melLast, bestD = 99;
    for (int k = 0; k < 3; k++) {
        int pc = (root_pc(f) + QV[qual(f)][k]) % 12;
        int d  = ((pc - melLast) % 12 + 18) % 12 - 6;
        if (rad_iabs(d) + rnd(2) < bestD) { bestD = rad_iabs(d); best = melLast + d; }
    }
    while (best < reg - 7) best += 12;
    while (best > reg + 9) best -= 12;
    return melLast = best;
}

// the variación: walk to the next chord tone in the run's direction, bounce
// at the register walls — continuous 16ths arpeggiating the harmony
static int var_note(int f) {
    int pcs[4];
    pcs[0] = root_pc(f);
    for (int k = 0; k < 3; k++) pcs[k + 1] = (root_pc(f) + QV[qual(f)][k]) % 12;
    int n = varLast;
    for (int g = 0; g < 24; g++) {
        n += varDir;
        if (n > 86) { varDir = -1; n -= 2; }
        if (n < 60) { varDir =  1; n += 2; }
        bool hit = false;
        for (int k = 0; k < 4; k++) if (n % 12 == pcs[k]) hit = true;
        if (hit) break;
    }
    return varLast = n;
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    int  f    = fn_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  feel = sng.feel[(int)((bar / 4) % 16)];
    bool legato = sect == S_B;                       // cantabile: the floor melts

    // ── THE CHAN-CHAN — the song actually ends ──
    if (bar == 63) {
        if (step == 4 || step == 12) {
            bool last = step == 12;                  // V7b9 ... then HOME
            int pc = last ? sng.keyPc : (sng.keyPc + 7) % 12;
            int rt = 56 + ((pc - 56) % 12 + 12) % 12;
            static const int IV_MIN[3] = { 3, 7, 12 };
            const int *iv = last ? IV_MIN : QV[Q_DOM7B9];
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + k * 4, rt + iv[k], I_BAND, last ? 6 : 5,
                             last ? 260 : (int)(stepMs * 5));
            schedule_hit(dly, bass_near(pc), I_BASS, 6, last ? 220 : (int)(stepMs * 5));
            schedule_hit(dly, rt - 12, I_PNO, 5, last ? 240 : (int)(stepMs * 5));
            vu += 4;
        }
        return;
    }
    bool thin = bar == 62;                           // the rallentando bar

    // ── MARCATO / SÍNCOPA — the floor: bass + piano left hand ──
    if (!legato || thin) {
        if (feel == 0 || thin) {                     // marcato in four
            if (step % 4 == 0) {
                int beatN = step / 4;
                int pcq = (beatN % 2 == 0) ? root_pc(f) : (root_pc(f) + 7) % 12;
                int vol = (beatN % 2 == 0) ? 5 : 4;
                int n = bass_near(pcq);
                schedule_hit(dly + 2, n, I_BASS, vol, (int)(stepMs * 3.2));
                schedule_hit(dly + 2, n + 12, I_PNO, vol - 1, (int)(stepMs * 1.8));
                if (sng.yumba && lvl >= 2 && beatN % 2 == 0)
                    schedule_hit(dly, 38, SL_GLP, 2, 40);    // the Pugliese thud
                vu += 1.3f;
            }
        } else {                                     // síncopa: the 3-3-2 skeleton
            if (step == 0 || step == 6 || step == 12) {
                int vol = step == 6 ? 6 : 4;         // the anticipation IS the accent
                int pcq = step == 12 ? (root_pc(f) + 7) % 12 : root_pc(f);
                int n = bass_near(pcq);
                schedule_hit(dly + 2, n, I_BASS, vol, (int)(stepMs * (step == 6 ? 5 : 3)));
                schedule_hit(dly + 2, n + 12, I_PNO, vol - 1, (int)(stepMs * 2));
                vu += 1.4f;
            }
        }
    } else {
        // cantabile: bass in halves, the piano rolls 8th-note arpeggios
        if (step == 0 || step == 8) {
            int pcq = step ? (root_pc(f) + 7) % 12 : root_pc(f);
            schedule_hit(dly + 2, bass_near(pcq), I_BASS, 4, (int)(stepMs * 7));
            vu += 1;
        }
        if (lvl >= 1 && step % 2 == 0) {
            int idx = step / 2, ai = idx < 4 ? idx : 7 - idx;        // up then down
            int rt = 48 + ((root_pc(f) - 48) % 12 + 12) % 12;
            int n = ai == 0 ? rt + 12 : gvBand[(ai - 1) % 3];
            schedule_hit(dly + 3, n, I_PNO, 3, (int)(stepMs * 1.8));
            vu += 0.5f;
        }
    }

    // ── THE ARRASTRE — three crescendo graces dragged into the downbeat ──
    if (!legato && !thin && step == 14 && (bar + 1) % 2 == 0 && bar + 1 < 62 && chance(60)) {
        int nr = bass_peek(root_pc(fn_at(bar + 1)));
        int tms = (int)(stepMs * 2);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + tms - 84 + k * 28, nr - 3 + k, I_BASS, 1 + k, 26);
    }

    // ── THE BANDONEÓN — chords: held in marcato, stabbed in síncopa ──
    bool compOn = (sect != S_INTRO || bar >= 4) && !thin && lvl >= 1;
    if (compOn) {
        if (legato || feel == 0) {
            if (step == 0) {
                rad_lead_to(root_pc(f), QV[qual(f)], gvBand, 3, 58, 76, &bandInit);
                for (int k = 0; k < 3; k++)
                    schedule_hit(dly + k * 11, gvBand[k], I_BAND, legato ? 3 : 4,
                                 (int)(stepMs * 14));
                vu += 1.5f;
            }
        } else if (step == 0 || step == 6 || step == 12) {
            if (step == 0)
                rad_lead_to(root_pc(f), QV[qual(f)], gvBand, 3, 58, 76, &bandInit);
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + k * 6, gvBand[k], I_BAND, step == 6 ? 5 : 3,
                             (int)(stepMs * 2.4));
            vu += 1.2f;
        }
    }

    // ── CHICHARRA — the scratch, off-beats, rhythmic sections only ──
    if (!legato && sect != S_INTRO && !thin && lvl >= 2
        && (step == 2 || step == 10) && chance(30)) {
        schedule_hit(dly + rnd(4), 70, SL_CHIC, 2, 36);
        vu += 0.3f;
    }

    // ── THE CANTO — one cell, re-pitched; bandoneón sings A, violin sings B ──
    if (bar >= 8 && !thin && sect != S_VAR && lvl >= 1) {
        int cs = (int)(s % 32);
        for (int i = 0; i < sng.melN; i++)
            if (sng.melOn[i] == cs && chance(90)) {
                int gap = (i + 1 < sng.melN ? sng.melOn[i + 1] : sng.melOn[0] + 32) - cs;
                if (legato || sect == S_FINAL) {     // the violin takes the song
                    int m = mel_note(f, 79);
                    schedule_hit(dly + 16 + rnd(6), m, I_VLN, sect == S_FINAL ? 4 : 5,
                                 (int)(stepMs * gap * 0.92));
                } else {                             // the bandoneón sings
                    int m = mel_note(f, 72);
                    schedule_hit(dly + 14 + rnd(5), m, I_BANDL, 5,
                                 (int)(stepMs * gap * 0.7));
                }
                melAnim = 1;
                vu += 1.6f;
            }
    }

    // ── THE VARIACIÓN — the bandoneón's continuous-16th finale ──
    if (sect == S_VAR) {
        int every = lvl >= 2 ? 1 : 2;
        if (step % every == 0) {
            if (chance(8)) varDir = -varDir;         // the run turns on a whim
            int m = var_note(f);
            schedule_hit(dly + 4, m, I_BANDL, step % 4 == 0 ? 5 : 4,
                         (int)(stepMs * 0.95 * every));
            vu += 0.8f;
        }
        if (step == 0 && lvl >= 2) {                 // long bows over the runs
            int m = gvBand[2] + 12;
            while (m > 90) m -= 12;
            schedule_hit(dly + 10, m, I_VLN, 3, (int)(stepMs * 15));
        }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    // THE BANDONEÓN — wave_set's second station gig: a drawn free-reed cycle
    // (odd-harmonic-leaning buzz; the bellows tremble is an LFO_VOLUME)
    float t[64];
    for (int i = 0; i < 64; i++) {
        float ph = i / 64.0f * 6.2832f;
        t[i] = 0.46f * sinf(ph)     + 0.20f * sinf(ph * 2)
             + 0.30f * sinf(ph * 3) + 0.10f * sinf(ph * 4)
             + 0.16f * sinf(ph * 5) + 0.06f * sinf(ph * 6)
             + 0.10f * sinf(ph * 7) + 0.05f * sinf(ph * 9);
    }
    wave_set(0, t, 64);
    instrument(I_BAND,  INSTR_USER0, 14, 320, 5, 120);   // left hand: chords
    instrument(I_BANDL, INSTR_USER0,  6, 220, 5, 110);   // right hand: the song
    instrument_lfo(I_BANDL, 0, LFO_VOLUME, 5.6f, 0.08f); // bellows tremble

    instrument(I_VLN, INSTR_SAW, 70, 400, 6, 220);       // the violins, one desk
    // (vibrato + the SCOOP into each note are rolled per song)

    instrument(I_PNO, INSTR_TRI, 2, 520, 2, 200);        // satie's felt piano
    instrument_env(I_PNO, 0, ENV_CUTOFF, 0, 70, 700);

    instrument(I_BASS, INSTR_TRI, 3, 280, 5, 100);       // the upright
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 2);

    instrument(SL_CHIC, INSTR_NOISE, 1, 34, 0, 20);      // chicharra scratch
    instrument_filter(SL_CHIC, FILTER_BAND, 3400, 8);
    instrument(SL_GLP, INSTR_NOISE, 0, 40, 0, 24);       // golpe on the box
    instrument_filter(SL_GLP, FILTER_LOW, 900, 4);
    instrument_env(SL_GLP, 0, ENV_PITCH, 0, 30, 4);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;

    if (!booted) {
        setup_instruments();
        if (TANGO_SEED) { new_song(pos, TANGO_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 96, 140, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) { note_off_all(); sfx(-1); }
        else scheduled = (long)pos;
    }
    if (ev & RAD_EV_TONE) apply_tone();

    // ══ TEMPO AS A VOICE — the conductor, every frame ══
    // ease toward the curve: fast when rising (the a-tempo snap), slow when
    // falling (the band sags together); feed bpm() only when the int moves
    double barF = (pos - (double)songBase) / 16.0;
    float  tgt  = (radioOn && barF >= 0) ? rub_target(barF) : 1.0f;
    rubMul += (tgt - rubMul) * (tgt > rubMul ? 0.22f : 0.07f);
    int lb = (int)(tempo * rubMul + 0.5f);
    if (lb != liveBpm) { liveBpm = lb; bpm(liveBpm); }
    stepMs = 60000.0 / (liveBpm * 4.0);

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        if (scheduled - songBase >= 64L * 16 + 8) fresh_song(pos);   // a breath, then the next record
        long bar = (scheduled - songBase) >= 0 ? (scheduled - songBase) / 16 : 0;
        for (int i = 0; i < 4; i++)
            chord_label(nowChord[i], 12, fn_at((bar / 4) * 4 + i));
    }

    if (melAnim > 0 && ++melAnim > 40) melAnim = 0;
    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("chord", "%s", nowChord[(int)(tbar % 4)]);
    watch("bpm", "%d", liveBpm);
    watch("feel", "%s", sng.feel[(int)((tbar / 4) % 16)] ? "sincopa" : "marcato");
#endif
}

// ── draw — the milonga chassis; window art = the breathing bandoneón ──────
void draw(void) {
    cls(CLR_BLACK);
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARK_BROWN, CLR_RED);            // mahogany shell, carmine trim
    rad_dial(sng.freq, CLR_RED);

    // the window — a milonga at midnight: one lamp, the bandoneón breathing
    rectfill(34, 52, 102, 116, CLR_BROWNISH_BLACK);
    circfill(85, 58, 3, CLR_LIGHT_YELLOW);                    // the lamp
    line(85, 52, 85, 55, CLR_DARK_GREY);
    line(85, 60, 62, 100, CLR_DARKER_GREY);                   // the cone of light
    line(85, 60, 108, 100, CLR_DARKER_GREY);

    {   // THE BANDONEÓN — its bellows breathe with the rubato curve
        float open = (1.015f - rubMul) * 9.0f;                // rit = bellows open
        if (open < 0) open = 0;
        if (open > 1) open = 1;
        int bw = 26 + (int)(open * 26);                       // bellows width 26..52
        int cx = 85, top = 104, bot = 140;
        int lx = cx - bw / 2 - 16, rx2 = cx + bw / 2;
        // the bellows folds (cream / grey, a carmine diamond on the light ones)
        int x0 = cx - bw / 2;
        for (int i = 0; i < 7; i++) {
            int xa = x0 + i * bw / 7, xb = x0 + (i + 1) * bw / 7;
            rectfill(xa, top, xb - xa, bot - top, (i % 2) ? CLR_MEDIUM_GREY : CLR_LIGHT_PEACH);
            if (i % 2 == 0) circfill((xa + xb) / 2, (top + bot) / 2, 1, CLR_DARK_RED);
        }
        rect(x0, top, bw, bot - top, CLR_BROWNISH_BLACK);
        // the end boxes, rosewood; the button field glints as the canto sings
        rectfill(lx, top - 4, 16, bot - top + 8, CLR_DARK_BROWN);
        rectfill(rx2, top - 4, 16, bot - top + 8, CLR_DARK_BROWN);
        rect(lx, top - 4, 16, bot - top + 8, CLR_BROWNISH_BLACK);
        rect(rx2, top - 4, 16, bot - top + 8, CLR_BROWNISH_BLACK);
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 3; c++) {
                bool gl = melAnim > 0 && (r * 3 + c) == (melAnim / 3) % 12;
                circfill(lx + 4 + c * 4, top + 2 + r * 8, 1,
                         gl ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);
                circfill(rx2 + 4 + c * 4, top + 4 + r * 8, 1,
                         gl ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);
            }
    }
    line(36, 158, 134, 158, CLR_DARKER_GREY);                 // the table edge
    line(98, 164, 110, 161, CLR_DARK_GREEN);                  // a rose, left behind
    circfill(112, 160, 2, CLR_RED);
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_RED);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_RED);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq,
                 sng.cancion ? "cancion" : sng.yumba ? "yumba"
                 : sng.andalus ? "andaluz" : "tango");
        print(l2, 154, 70, CLR_DARK_RED);
        snprintf(l2, 32, "%d bpm #%08X", liveBpm, sng.seed);  // the number breathes
        print(l2, 154, 82, CLR_DARK_RED);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_RED);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        rad_chord_row(nowChord, 4, (int)(bar % 4), 152, 104, CLR_RED);
        print(SECTNAME[sect], 152, 120, CLR_RED);
        rad_phrase_dots(232, 124, 8, bar / 8, CLR_RED);
    }

    static const char *FEEL[4] = { "salon", "tanda", "orquesta", "pugliese" };
    rad_knob(168, 148, 9, intensity / 3.0f, FEEL[intensity], CLR_RED);
    rad_knob(218, 148, 9, (tempo - 96) / 44.0f, "tempo", CLR_RED);
    rad_knob(262, 148, 11, toneSel / 3.0f, RAD_TONENAME[toneSel], CLR_RED);
    rad_power_led(radioOn, CLR_RED, CLR_DARKER_PURPLE);

    rad_help_button(CLR_RED);
    rad_footer("SPACE next tango   H help");

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tango (rolls a new seed)" },
            { "R",          "the same tango again" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - salon ... pugliese" },
            { "UP/DOWN",    "base tempo (the rubato rides it)" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        static const char *NOTES[3] = {
            "TEMPO AS A VOICE: live bpm() rubato -",
            "watch the bellows breathe. arrastre,",
            "sincopa, chan-chan. pin: TANGO_SEED 0x..",
        };
        rad_help_panel("TANGO RADIO", HELP, 8, NOTES, 3, CLR_RED);
    }
}
