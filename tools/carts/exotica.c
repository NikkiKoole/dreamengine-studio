// ── EXOTICA RADIO ─────────────────────────────────────────────────────────
// The eleventh station: endless exotica — the Martin Denny / Les Baxter /
// Arthur Lyman homage. Faux-tropical lounge from 1957: lush vibraphone over
// lazy latin percussion, and a band that answers the jungle with bird calls.
// Hosono's actual origin story (the Tropical Trilogy is exotica homage).
//
// Three firsts on this station:
//   • FIRST CART ON THE radio.h SCAFFOLD — born from the toolkit (seed/history,
//     voice leading, step clock, input block, chassis) instead of a copy of
//     dub.c. The window art + the music are all that's cart-specific.
//   • ALL THREE ENGINES ON ONE DIAL — the vibraphone lead is INSTR_MALLET
//     (motor tremolo on), the rhythm guitar is INSTR_PLUCK (soft nylon), the
//     glass-bell sparkles are INSTR_FM (the 3.5 bell detent). The timbre
//     ceiling of the whole engine project, audible in one place.
//   • THE AVIARY — bird calls and frog croaks as an ALEATORIC PERFORMANCE
//     channel: chance() per bar fires pitch-swooped calls that never repeat
//     and are never seeded (pure engine rnd) — exactly like Denny's band
//     improvising the jungle on Quiet Village. Replaying a seed replays the
//     tune; the birds never sing it the same way twice.
//
// Per-song timbre roll from day one (game-music.md "the same band every
// night"): the seed rolls the vibes' macro position, the guitar's pick, the
// bird species pair, the kit dressing — two SPACE presses = two records.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define EXOTICA_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_VIBE  5    // vibraphone lead — INSTR_MALLET, the star
#define I_GTR   6    // rhythm guitar  — INSTR_PLUCK, soft nylon comp
#define I_BELL  7    // glass sparkle  — INSTR_FM on the bell detent
#define I_BASS  8    // upright-ish two-feel bass
// kit
#define SL_RIM   9   // clave / rim tick
#define SL_SHAK 10   // shaker
#define SL_CONGA 11  // low boo-bam / conga
#define SL_FCYM 12   // finger cymbal
// the aviary
#define I_CHIRP 13   // descending chirp
#define I_SWOOP 14   // rising swoop / gull
#define I_FROG  15   // the frog croak

// ── harmony — lounge changes, lush voicings ───────────────────────────────
enum { Q_MAJ9, Q_MIN9, Q_69, Q_DOM9, Q_MIN6, NQ };
static const char *QN[NQ] = { "maj9", "m9", "6/9", "9", "m6" };
static const int QV[NQ][3] = {
    { 4, 11, 14 },   // maj9 — the lounge default
    { 3, 10, 14 },   // m9
    { 4,  9, 14 },   // 6/9  — the vibraphone chord
    { 4, 10, 14 },   // 9
    { 3,  9, 14 },   // m6   — the borrowed-iv dusk color
};

typedef struct { int off, q; } Ch;
// 4-bar lounge loops — generic late-50s changes, exotica-flavored:
// the borrowed iv (m6) is the instant dusk; bVI/bVII give the "pagan" minor.
static const Ch TMPL_MAJ[4][4] = {
    { {0,Q_69},   {0,Q_MAJ9}, {5,Q_MIN6}, {7,Q_DOM9} },   // "quiet":  I  I  ivm6 V9
    { {0,Q_MAJ9}, {9,Q_MIN9}, {2,Q_MIN9}, {7,Q_DOM9} },   // "lagoon": I  vi  ii  V
    { {0,Q_69},   {3,Q_MAJ9}, {2,Q_MIN9}, {1,Q_DOM9} },   // "sunset": I bIII ii bII9
    { {0,Q_MAJ9}, {5,Q_DOM9}, {0,Q_69},   {5,Q_MIN6} },   // "tide":   I  IV9  I  ivm6
};
static const Ch TMPL_MIN[3][4] = {
    { {0,Q_MIN9}, {0,Q_MIN9}, {5,Q_MIN9}, {7,Q_DOM9} },   // "taboo":  i  i  iv  V9
    { {0,Q_MIN6}, {8,Q_MAJ9}, {0,Q_MIN6}, {10,Q_DOM9} },  // "ritual": i bVI i bVII9
    { {0,Q_MIN9}, {3,Q_MAJ9}, {8,Q_MAJ9}, {7,Q_DOM9} },   // "pagan":  i bIII bVI V9
};

// ── the form — 8 sections x 8 bars; the JUNGLE break is the signature ─────
enum { S_INTRO, S_TUNE, S_JUNGLE, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_TUNE, S_TUNE, S_JUNGLE,
                             S_TUNE,  S_JUNGLE, S_TUNE, S_OUTRO };
static const char *SECTNAME[4] = { "intro", "tune", "jungle", "outro" };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc, minorKey;
    Ch   loop[4];
    int  melOn[6], melN;       // the vibes cell: onsets in a 32-step (2-bar) grid
    int  melReg;               // melody register base
    int  birdA, birdB;         // the rolled species pair (slots)
    int  birdProb;             // calls per bar, percent
    int  hasShaker, hasFcym;   // kit dressing
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 156.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

static int    tempo     = 92;
static int    intensity = 1;
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    toneSel   = 2;
static int    songCount = 0;
static float  vu        = 0;
static int    bassLast  = 36;
static int    melLast   = 72;
static char   nowChord[4][12];
static int    birdAnim  = 0;     // frames since a bird call fired (drives the window art)
static int    birdCount = 0;

static int  gvGtr[3] = { 55, 60, 64 };
static bool gtrInit = false;

// THE BAND (B) — the chairs and their candidates, from radio-instrument-options.md.
// Only the two mallet chairs are seats here: the vibes lead (marimba / Denny piano
// poles) and the FM sparkle (celesta alt). The comp guitar already rolls its pick
// per song and the aviary stays unseeded — the doc says leave both.
static RadBand band;
static int chVibe, chBell;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);   // defined with the chairs, below

// ── song generation — composition AND the band, all from the seed ─────────
static const char *TW1[] = { "Quiet", "Jungle", "Coral", "Moonlit", "Tiki", "Velvet",
    "Pagan", "Lagoon", "Sunset", "Hidden", "Distant", "Emerald" };
static const char *TW2[] = { "Village", "Lagoon", "Ritual", "Tide", "Island", "Moon",
    "Paradise", "Drums", "Cove", "Bird", "Waterfall", "Reef" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.keyPc    = srnd(12);
    sng.minorKey = srnd(100) < 35;                  // exotica leans major-lush
    {
        const Ch *t = sng.minorKey ? TMPL_MIN[srnd(3)] : TMPL_MAJ[srnd(4)];
        for (int i = 0; i < 4; i++) sng.loop[i] = t[i];
    }
    if (!sng.minorKey && srnd(100) < 30)            // flavor roll: melt V9 to a m6 dusk
        for (int i = 1; i < 4; i++)
            if (sng.loop[i].off == 7 && sng.loop[i].q == Q_DOM9)
                { sng.loop[i].off = 5; sng.loop[i].q = Q_MIN6; break; }

    // the vibes cell: 3..5 onsets in 2 bars, lazy (biased late + off-beat)
    sng.melN = 0;
    static const int MCAND[12] = { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30 };
    for (int i = 0; i < 12 && sng.melN < 5; i++)
        if (srnd(100) < 34) sng.melOn[sng.melN++] = MCAND[i];
    if (sng.melN < 3) { sng.melN = 3; sng.melOn[0] = 0; sng.melOn[1] = 11; sng.melOn[2] = 22; }
    sng.melReg = 69 + srnd(8);                      // the vibes live up high

    // THE BAND, rolled per song (two SPACE presses = two records):
    // the vibes' bar + mallet + motor, the guitar's pick, the kit dressing
    instrument_harmonics(I_VIBE, 0.12f + srnd(28) * 0.01f);   // wood->silvery
    instrument_timbre(I_VIBE,    0.40f + srnd(30) * 0.01f);   // mallet hardness
    instrument_morph(I_VIBE,     0.72f + srnd(24) * 0.01f);   // long ring, motor ON
    instrument_timbre(I_GTR,     0.25f + srnd(25) * 0.01f);   // felt..soft pick
    instrument_morph(I_GTR,      0.20f + srnd(30) * 0.01f);   // pick position
    instrument_filter(I_BASS, FILTER_LOW, 480 + srnd(180), 2);
    sng.hasShaker = srnd(100) < 70;
    sng.hasFcym   = srnd(100) < 55;

    // the aviary species pair + how talkative the jungle is (the PROB is
    // seeded; every actual call is pure engine rnd — the band improvising)
    {
        int species[3] = { I_CHIRP, I_SWOOP, I_FROG };
        int a = srnd(3), b = (a + 1 + srnd(2)) % 3;
        sng.birdA = species[a];
        sng.birdB = species[b];
        sng.birdProb = 12 + srnd(16);               // 12..27% per bar (jungle: x2)
    }

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 84 + srnd(17);                          // 84..100 — lazy
    bpm(tempo);
    apply_band_overrides();          // picked chairs beat the per-song macro roll above
    songBase = (long)pos + 8;
    gtrInit = false;
    bassLast = 36;
    melLast = sng.melReg;
    songCount++;
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ────────────────────────────────────────────────────────
static int sect_of(long bar)  { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }
static Ch  chord_at(long bar) { return sng.loop[bar % 4]; }
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }

static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c)], QN[c.q]);
}

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_JUNGLE ? 2 : 1);
    return rad_level(base, intensity);
}

// melody: nearest chord tone (or 9th) to where the line sits — the
// One Note Samba trick, vibes edition
static int mel_note(Ch c) {
    int best = melLast, bestD = 99;
    for (int k = 0; k < 3; k++) {
        int pc = (root_pc(c) + QV[c.q][k]) % 12;
        int d  = ((pc - melLast) % 12 + 18) % 12 - 6;
        int n  = melLast + d;
        if (rad_iabs(d) + rnd(2) < bestD) { bestD = rad_iabs(d); best = n; }
    }
    while (best < sng.melReg - 7) best += 12;
    while (best > sng.melReg + 9) best -= 12;
    return melLast = best;
}

static int bass_near(int pc) {
    int d = ((pc - bassLast) % 12 + 18) % 12 - 6;
    int n = bassLast + d;
    while (n < 26) n += 12;
    while (n > 43) n -= 12;
    return bassLast = n;
}

// ── THE AVIARY — never seeded: the same tune, a different jungle every time ─
static void bird_call(int dly) {
    int sp = chance(60) ? sng.birdA : sng.birdB;
    if (sp == I_CHIRP) {                            // 2-4 falling chirps
        int n = 2 + rnd(3), m = 88 + rnd(8);
        for (int k = 0; k < n; k++)
            schedule_hit(dly + k * (60 + rnd(40)), m + rnd(3), I_CHIRP, 2, 45);
    } else if (sp == I_SWOOP) {                     // one long rising gull cry
        schedule_hit(dly, 79 + rnd(7), I_SWOOP, 2, 420 + rnd(300));
    } else {                                        // the frog: two low croaks
        schedule_hit(dly, 41 + rnd(4), I_FROG, 3, 110);
        if (chance(60)) schedule_hit(dly + 160 + rnd(80), 41 + rnd(4), I_FROG, 2, 90);
    }
    birdAnim = 1;
    birdCount++;
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    Ch   c    = chord_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);

    // the jungle answers — aleatoric, per bar, denser in the break
    if (step == 0) {
        int prob = sng.birdProb * (sect == S_JUNGLE ? 2 : 1);
        if (sect == S_INTRO || sect == S_OUTRO) prob += 8;     // dawn chorus
        if (chance(prob)) bird_call(dly + rnd(300));
    }

    // KIT — lazy latin: rim clave-ish, conga heartbeat, shaker 8ths up top
    bool kit = sect != S_INTRO || bar % 8 >= 4;
    if (kit) {
        if (step == 0 || step == 7 || step == 10)
            { schedule_hit(dly + rnd(4), 76, SL_RIM, 2, 30); vu += 0.6f; }
        if (step == 4 || step == 12)
            { schedule_hit(dly + rnd(4), step == 4 ? 43 : 38, SL_CONGA, lvl >= 1 ? 4 : 3, 120); vu += 1.2f; }
        if (sect == S_JUNGLE && step == 14 && chance(50))      // the answering drum
            schedule_hit(dly + rnd(4), 46, SL_CONGA, 4, 90);
        if (sng.hasShaker && lvl >= 2 && step % 2 == 0)
            schedule_hit(dly + rnd(6), 90, SL_SHAK, 1, 30);
        if (sng.hasFcym && lvl >= 1 && step == 8 && bar % 2 == 1)
            { schedule_hit(dly + rnd(4), 96, SL_FCYM, 2, 600); vu += 0.5f; }
    }

    // BASS — the lazy two-feel: root on 1, fifth on 3, leaning back
    if (sect != S_INTRO) {
        if (step == 0)
            { schedule_hit(dly + 14, bass_near(root_pc(c)), I_BASS, 5, (int)(stepMs * 6)); vu += 1.5f; }
        else if (step == 8)
            { schedule_hit(dly + 14, bass_near((root_pc(c) + 7) % 12), I_BASS, 4, (int)(stepMs * 5)); vu += 1; }
    }

    // GUITAR — soft nylon comp on the off-beats (the Lyman strum)
    if (lvl >= 1 && (step == 4 || step == 12)) {
        rad_lead_to(root_pc(c), QV[c.q], gvGtr, 3, 52, 71, &gtrInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 9 + rnd(4), gvGtr[k], I_GTR, 3, 900);
        vu += 1.2f;
    }

    // VIBES — the cell, re-pitched; long gaps earn a mallet ROLL
    if (sect == S_TUNE || sect == S_OUTRO || (sect == S_JUNGLE && lvl >= 2)) {
        int cs = (int)(s % 32);
        for (int i = 0; i < sng.melN; i++)
            if (sng.melOn[i] == cs && chance(88)) {
                int m = mel_note(c);
                int gap = (i + 1 < sng.melN ? sng.melOn[i + 1] : sng.melOn[0] + 32) - cs;
                if (gap >= 8 && chance(30)) {                  // the tremolo roll
                    for (int r = 0; r < 6; r++)
                        schedule_hit(dly + 18 + r * 72, m, I_VIBE, 3 + (r & 1), 2500);
                } else
                    schedule_hit(dly + 18 + rnd(8), m, I_VIBE, 5, 3000);   // behind the beat
                vu += 1.8f;
            }
    }

    // BELLS — FM glass sparkle: an upward arpeggio at phrase tops
    if (lvl >= 2 && step == 0 && bar % 4 == 0 && chance(65)) {
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + 40 + k * 90, 81 + ((root_pc(c) + QV[c.q][k]) % 12), I_BELL, 2, 500);
        vu += 1;
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
// Each chair's candidates are full recipes — switching re-aims the slot from
// scratch, so a swap mid-song is clean. sel 0 is always the shipped sound;
// for the vibes chair, sel 0 is JUST the base instrument() call so new_song's
// per-song macro roll (harmonics/timbre/morph) stays authoritative.
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chVibe) {
        if (sel == 0) {                                      // "vibes" — shipped MALLET base
            instrument(I_VIBE, INSTR_MALLET, 1, 0, 7, 1500); //   (new_song rolls the macros)
        } else if (sel == 1) {                               // "marimba" — wood bar, drier ring
            instrument(I_VIBE, INSTR_MALLET, 1, 0, 7, 1500);
            instrument_harmonics(I_VIBE, 0.05f);
            instrument_timbre(I_VIBE, 0.45f);
            instrument_morph(I_VIBE, 0.30f);
        } else {                                             // "denny piano" — the other pole
            instrument(I_VIBE, INSTR_TRI, 2, 600, 2, 240);   //   felt piano (satie/cocktail recipe)
            instrument_env(I_VIBE, 0, ENV_CUTOFF, 0, 70, 700);
        }
    } else if (idx == chBell) {
        if (sel == 0) {                                      // "fm glass" — shipped bell detent
            instrument(I_BELL, INSTR_FM, 1, 500, 2, 400);
            instrument_harmonics(I_BELL, 0.55f);             // the 3.5 bell detent
            instrument_timbre(I_BELL, 0.55f);
            instrument_morph(I_BELL, 0.12f);
        } else {                                             // "celesta" — mallet sparkle
            instrument(I_BELL, INSTR_MALLET, 1, 500, 2, 400);
            instrument_harmonics(I_BELL, 0.50f);             //   (the celesta preset from mallet.c)
            instrument_timbre(I_BELL, 0.55f);
            instrument_morph(I_BELL, 0.45f);
        }
    }
}

// re-assert any non-default chair (new_song re-rolls macros/filters over the
// slots; default chairs keep the per-song roll, picked chairs win over it)
static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chVibe = rad_chair(&band, "vibes",   "vibes",   "marimba", "denny piano", NULL);
    chBell = rad_chair(&band, "sparkle", "fm glass", "celesta", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);         // base sounds (sel 0)

    // the rest of the band (per-song macros rolled in new_song)
    instrument(I_GTR,  INSTR_PLUCK,  1, 0, 7, 900);          // soft nylon comp
    instrument_harmonics(I_GTR, 0.42f);

    instrument(I_BASS, INSTR_TRI, 4, 260, 4, 140);           // upright-ish
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 18, 2);

    instrument(SL_RIM, INSTR_NOISE, 0, 26, 0, 16);           // clave tick
    instrument_filter(SL_RIM, FILTER_BAND, 2000, 9);
    instrument(SL_SHAK, INSTR_NOISE, 1, 40, 0, 22);
    instrument_filter(SL_SHAK, FILTER_HIGH, 5600, 3);
    instrument(SL_CONGA, INSTR_SINE, 0, 190, 0, 70);         // boo-bam heartbeat
    instrument_env(SL_CONGA, 0, ENV_PITCH, 0, 40, 7);
    instrument(SL_FCYM, INSTR_FM, 0, 700, 0, 500);           // finger cymbal = FM clang, tiny
    instrument_harmonics(SL_FCYM, 0.55f);
    instrument_timbre(SL_FCYM, 0.75f);
    instrument_morph(SL_FCYM, 0.55f);

    // the aviary
    instrument(I_CHIRP, INSTR_SINE, 1, 60, 2, 30);           // falling chirp
    instrument_env(I_CHIRP, 0, ENV_PITCH, 0, 50, 7);
    instrument_lfo(I_CHIRP, 0, LFO_PITCH, 11.0f, 0.5f);
    instrument(I_SWOOP, INSTR_SINE, 30, 300, 5, 160);        // rising gull
    instrument_env(I_SWOOP, 0, ENV_PITCH, 0, 380, -9);       // starts LOW, swoops up
    instrument_lfo(I_SWOOP, 0, LFO_PITCH, 6.5f, 0.35f);
    instrument(I_FROG, INSTR_SQUARE, 4, 90, 2, 40);          // the croak
    instrument_duty(I_FROG, 0.18f);
    instrument_filter(I_FROG, FILTER_LOW, 700, 5);
    instrument_env(I_FROG, 0, ENV_PITCH, 0, 90, 3);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (EXOTICA_SEED) { new_song(pos, EXOTICA_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 76, 112, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) { note_off_all(); sfx(-1); }
        else scheduled = (long)pos;
    }
    if (ev & (RAD_EV_TONE | RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD)) {
        // tone knob: the whole band brightens/mellows (re-aim, keeping song rolls)
        float tm = RAD_TONEMUL[toneSel];
        instrument_filter(I_GTR,  FILTER_LOW, (int)(2400 * tm), 1);
        instrument_filter(I_VIBE, FILTER_LOW, (int)(4200 * tm), 0);
        instrument_filter(I_BELL, FILTER_LOW, (int)(3800 * tm), 0);
    }

    int chair = rad_band_input(&band, &showHelp);   // THE BAND — B, then click/number
    if (chair >= 0) apply_chair(chair);

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= 64L * 16) fresh_song(pos);
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 12, sng.loop[i]);
    }

    if (birdAnim > 0 && ++birdAnim > 100) birdAnim = 0;
    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("chord", "%s", nowChord[(int)(tbar % 4)]);
    watch("birds", "%d", birdCount);
#endif
}

// ── draw — the tiki chassis; window art = the lagoon at sunset ────────────
void draw(void) {
    cls(CLR_BLACK);
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_BROWN, CLR_ORANGE);              // bamboo shell, sunset trim
    rad_dial(sng.freq, CLR_ORANGE);

    // the window — sunset lagoon: sky, sun on the water, palm, and the bird
    rectfill(34, 52, 102, 116, CLR_DARKER_PURPLE);           // dusk sky
    rectfill(34, 118, 102, 50, CLR_BLUE_GREEN);              // the lagoon
    circfill(76, 116, 16, CLR_ORANGE);                       // the sun, half-set
    rectfill(34, 118, 102, 2, CLR_DARK_ORANGE);              // horizon glow
    for (int y = 122; y < 164; y += 6) {                     // sun glitter path
        int w = 10 + (164 - y) / 3;
        int x0 = 76 - w / 2 + (int)(sinf(timer() * 1.3f + y) * 3);
        line(x0, y, x0 + w, y, (y / 6) % 2 ? CLR_DARK_ORANGE : CLR_ORANGE);
    }
    // the palm, leaning in from the left
    line(40, 166, 52, 110, CLR_DARK_BROWN);
    line(41, 166, 53, 110, CLR_DARK_BROWN);
    for (int k = 0; k < 5; k++) {
        float a = 2.4f + k * 0.55f;
        bezier(52, 110, 52 + (int)(cosf(a) * 18), 110 - (int)(sinf(a) * 10),
               52 + (int)(cosf(a) * 34), 110 - (int)(sinf(a) * 16) + 6, CLR_DARK_GREEN);
    }
    // a bird crosses the window when the jungle answers
    if (birdAnim > 0) {
        int bx = 36 + birdAnim, by = 70 + (int)(sinf(birdAnim * 0.18f) * 6);
        if (bx < 132) {
            int f = (birdAnim / 4) % 2 ? 2 : 0;
            line(bx - 3, by - 1 + f, bx, by, CLR_BLACK);
            line(bx, by, bx + 3, by - 1 + f, CLR_BLACK);
        }
    }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_ORANGE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq, sng.minorKey ? "ritual" : "lagoon");
        print(l2, 154, 70, CLR_DARK_ORANGE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_DARK_ORANGE);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_ORANGE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        rad_chord_row(nowChord, 4, (int)(bar % 4), 152, 104, CLR_ORANGE);
        print(SECTNAME[sect], 152, 120, CLR_ORANGE);
        rad_phrase_dots(232, 124, 8, bar / 8, CLR_ORANGE);
    }

    static const char *FEEL[4] = { "lull", "lounge", "luau", "rite" };
    rad_knob(168, 148, 9, intensity / 3.0f, FEEL[intensity], CLR_ORANGE);
    rad_knob(218, 148, 9, (tempo - 76) / 36.0f, "tempo", CLR_ORANGE);
    rad_knob(262, 148, 11, toneSel / 3.0f, RAD_TONENAME[toneSel], CLR_ORANGE);
    rad_power_led(radioOn, CLR_ORANGE, CLR_DARK_BROWN);

    rad_help_button(CLR_ORANGE);
    rad_footer("SPACE next song   B band   H help");

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new seed)" },
            { "R",          "same tune again - new birds" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - lull/lounge/luau/rite" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - swap chairs live" },
        };
        static const char *NOTES[3] = {
            "vibes=MALLET gtr=PLUCK bells=FM - all 3 engines",
            "the birds are never seeded: a fresh jungle",
            "every listen. pin: #define EXOTICA_SEED 0x...",
        };
        rad_help_panel("EXOTICA RADIO", HELP, 8, NOTES, 3, CLR_ORANGE);
    }
    rad_band_panel(&band, CLR_ORANGE);
}
