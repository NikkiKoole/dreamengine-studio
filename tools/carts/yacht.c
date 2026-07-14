/* de:meta
{
  "slug": "yacht",
  "collection": ["radio"],
  "title": "yacht radio",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "chord-voicing",
    "swing-timing",
    "generative-melody"
  ],
  "lineage": "Built on the radio.h scaffold (sibling of citypop.c), specializing in the Steely Dan/AOR idiom with a named-chord vocabulary (the mu chord), the Purdie shuffle groove engine, and CR-78 drum circuits borrowed from cr78.c.",
  "homage": "Steely Dan / Hall & Oates (yacht rock)",
  "description": "Endless yacht rock / AOR - the Steely Dan / Doobies / Hall & Oates homage, station #12 (second born on the radio.h scaffold). Built around THE MU CHORD - Steely Dan's named invention, a major triad with the added 9th voiced 3-5-9 - in stolen-playbook loop templates (the two-mu vamp, ii-V13 lush, maj7 PLANING, dorian funk) with mu-ify and sus-melt flavor rolls. The comping instrument is the FM epiano on the 1:1 detent, so the DX tine pings every chord; clean PLUCK strat stabs, breathy sax lead, chromatic bass runs INTO every change, citypop anticipations on the and-of-4, and a +2 gear change in the last chorus. Three kit personalities rolled per song: tight session 16ths, the PURDIE half-time shuffle (swung ghosts at vol 1), or CR-78 circuits on loan from cr78.c (the Hall & Oates 'I Can't Go For That' move). SPACE next song, R replay, [ ] history, LEFT/RIGHT feel (dock/cruise/regatta/open sea), UP/DOWN tempo, T tone, M power, B band (swap chairs live), H help."
}
de:meta */
// ── YACHT RADIO ───────────────────────────────────────────────────────────
// The twelfth station: endless yacht rock / AOR — the Steely Dan / Doobies /
// Hall & Oates homage. The most session-perfectionist music ever recorded,
// which is exactly why it fits (the citypop lesson: machine-tight genres get
// authenticity for free). Scores 4/4 on the citypop conditions:
//
//   • THE MU CHORD — Steely Dan's named invention: a major triad with the
//     added 9th (no 7th), voiced 3-5-9. It's the first chord QUALITY a
//     station has been built around: half the loop templates lean on it,
//     and the "mu-ify" flavor roll melts plain maj7s into it.
//   • THE FM EPIANO IS THE BAND — the comping instrument is INSTR_FM on the
//     1:1 detent (the DX tine ping on every chord), the engine's first real
//     workout in a station. Anticipations push the and-of-4 like citypop.
//   • THE PURDIE SHUFFLE — the session-drummer signature as a groove
//     template: half-time backbeat, swung 16ths (58%), ghost snares at
//     vol 1-2 filling the pockets. Rolled per song against a straight
//     session groove and...
//   • ...CR-78 DRUMS ON LOAN from tools/carts/cr78.c — the Hall & Oates
//     "I Can't Go For That" move: some songs swap the live kit for the
//     CompuRhythm circuits (kick/snare/hat, the measured params). The
//     machine-soul end of the marina.
//   • THE GEAR CHANGE — the last chorus lifts +2 semitones (the truck
//     driver's modulation, on loan from citypop.c's playbook).
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define YACHT_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_EP    5    // FM electric piano — the band's center (1:1 detent, tine on)
#define I_BASS  6    // round fingered electric, chromatic approach runs
#define I_GTR   7    // clean strat 9th-stabs — INSTR_PLUCK, bright
#define I_SAX   8    // the chorus lead — narrow pulse, breathy
#define I_PAD   9    // soft strings, choruses only
// kit (live session OR cr-78 circuits — rolled per song)
#define SL_KICK 10
#define SL_SN   11
#define SL_HATC 12
#define SL_HATO 13
#define SL_RIDE 14

// ── harmony — the mu chord + the codified changes ─────────────────────────
enum { Q_MU, Q_MAJ7, Q_MIN9, Q_DOM13, Q_SUS9, NQ };
static const char *QN[NQ] = { "mu", "maj7", "m9", "13", "9sus" };
static const int QV[NQ][3] = {
    { 4,  7, 14 },   // mu    — 3-5-9, the Steely voicing (no 7th!)
    { 4, 11, 14 },   // maj7  — with the 9, session lush
    { 3, 10, 14 },   // m9
    { 4, 10, 21 },   // 13    — 3-b7-13, the dominant that smiles
    { 5, 10, 14 },   // 9sus  — the V that never commits
};

typedef struct { int off, q; } Ch;
// 4-bar loops in the idiom (generic yacht changes, era-flavored):
static const Ch TMPL_MAJ[4][4] = {
    { {0,Q_MU},   {10,Q_MAJ7}, {5,Q_MAJ7}, {7,Q_SUS9} },   // "reeling": Imu bVII IV V9sus
    { {2,Q_MIN9}, {7,Q_DOM13}, {0,Q_MU},   {9,Q_MIN9} },   // "babylon": ii V13 Imu vi
    { {0,Q_MAJ7}, {3,Q_MAJ7},  {8,Q_MAJ7}, {1,Q_MAJ7} },   // "deacon": maj7 PLANING
    { {0,Q_MU},   {5,Q_MU},    {0,Q_MU},   {5,Q_MU} },     // "nineteen": the two-mu vamp
};
static const Ch TMPL_MIN[2][4] = {
    { {0,Q_MIN9}, {5,Q_DOM13}, {0,Q_MIN9}, {8,Q_MAJ7} },   // "royal": dorian i IV13 i bVI
    { {0,Q_MIN9}, {10,Q_MAJ7}, {8,Q_MAJ7}, {7,Q_SUS9} },   // "gaucho": i bVII bVI V9sus
};

// ── the form — verse/chorus with a bridge lift + the final gear change ────
enum { S_INTRO, S_VERSE, S_CHORUS, S_BRIDGE, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_VERSE, S_CHORUS, S_VERSE,
                             S_CHORUS, S_BRIDGE, S_CHORUS, S_OUTRO };
static const char *SECTNAME[5] = { "intro", "verse", "chorus", "bridge", "outro" };

// ── the generated song ────────────────────────────────────────────────────
enum { G_SESSION, G_PURDIE, G_CR78, NGROOVE };   // the kit personality, rolled
static const char *GROOVENAME[NGROOVE] = { "session", "purdie", "cr-78" };

typedef struct {
    int  keyPc, minorKey;
    Ch   loop[4];
    int  groove;               // session 16ths / purdie shuffle / CR-78 machine
    int  gearUp;               // last chorus lifts +2 (the truck driver's move)
    int  melOn[6], melN;       // the sax cell (32-step grid)
    int  melReg;
    int  hasRide;              // ride cymbal in the chorus
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 142.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

static int    tempo     = 104;
static int    intensity = 1;
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    toneSel   = 2;
static int    songCount = 0;
static float  vu        = 0;
static int    bassLast  = 33;
static int    melLast   = 70;
static char   nowChord[4][12];

static int  gvEp[3]  = { 62, 66, 69 };
static int  gvPad[3] = { 55, 59, 62 };
static bool epInit = false, padInit = false;

// THE BAND (B) — the chairs and their candidates, from radio-instrument-options.md.
// Each chair's sel 0 is the shipped marina sound; the panel is the live override.
// The toolkit never touches rad_srnd, so pinned seeds stay byte-identical.
static RadBand band;
static int chEp, chBass, chLead, chPad;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);   // defined with the chairs, below

// ── song generation — the tune AND the band, all from the seed ────────────
static const char *TW1[] = { "Marina", "Pacific", "Chrome", "Midnight", "Coastal",
    "Royal", "Century", "Harbor", "Velvet", "Crystal", "Golden", "Babylon" };
static const char *TW2[] = { "Standard", "Shuffle", "Avenue", "Mirage", "Lights",
    "Charade", "Tan", "Pier", "Nineteen", "Reflection", "Operator", "Cove" };

#ifdef DE_TRACE
// the acceptance-corpus hash: every composed field, folded FNV-1a. yachtrack.c
// (the rack) carries the same function over its verbatim new_song port and must
// match it per seed — the radio→rack seed-compat oracle (yacht-rack.md §3).
static unsigned song_sum(void) {
    unsigned h = 2166136261u;
    #define F(v) h = (h ^ (unsigned)(v)) * 16777619u
    F(sng.keyPc); F(sng.minorKey);
    for (int i = 0; i < 4; i++) { F(sng.loop[i].off); F(sng.loop[i].q); }
    F(sng.groove); F(sng.gearUp); F(sng.hasRide);
    F(sng.melN); for (int i = 0; i < sng.melN; i++) F(sng.melOn[i]);
    F(sng.melReg);
    for (const char *p = sng.title; *p; p++) F(*p);
    F((int)(sng.freq * 10 + 0.5f)); F(tempo);
    #undef F
    return h;
}
#endif

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.keyPc    = srnd(12);
    sng.minorKey = srnd(100) < 30;                  // the marina leans major
    {
        const Ch *t = sng.minorKey ? TMPL_MIN[srnd(2)] : TMPL_MAJ[srnd(4)];
        for (int i = 0; i < 4; i++) sng.loop[i] = t[i];
    }
    if (!sng.minorKey && srnd(100) < 35)            // the MU-IFY roll: melt a maj7
        for (int i = 0; i < 4; i++)                 // into the Steely chord
            if (sng.loop[i].q == Q_MAJ7 && sng.loop[i].off != 1)
                { sng.loop[i].q = Q_MU; break; }
    if (srnd(100) < 30)                             // melt the V into a 9sus
        for (int i = 1; i < 4; i++)
            if (sng.loop[i].q == Q_DOM13)
                { sng.loop[i].q = Q_SUS9; break; }

    sng.groove = srnd(100) < 25 ? G_CR78 : (srnd(100) < 45 ? G_PURDIE : G_SESSION);
    sng.gearUp = srnd(100) < 45;                    // the last-chorus lift
    sng.hasRide = srnd(100) < 60;

    // the sax cell: 3..5 onsets in 2 bars, leaning on the off-beats
    sng.melN = 0;
    static const int MCAND[12] = { 0, 2, 6, 10, 13, 16, 18, 22, 25, 27, 29, 30 };
    for (int i = 0; i < 12 && sng.melN < 5; i++)
        if (srnd(100) < 32) sng.melOn[sng.melN++] = MCAND[i];
    if (sng.melN < 3) { sng.melN = 3; sng.melOn[0] = 2; sng.melOn[1] = 13; sng.melOn[2] = 22; }
    sng.melReg = 67 + srnd(7);

    // THE BAND, rolled per song: the epiano's tine + warmth, the strat's pick,
    // the bass register — two SPACE presses = two records
    instrument_timbre(I_EP, 0.38f + srnd(22) * 0.01f);       // strike brightness
    instrument_morph(I_EP,  0.06f + srnd(10) * 0.01f);       // a touch of growl, never dirty
    instrument_timbre(I_GTR, 0.55f + srnd(30) * 0.01f);
    instrument_morph(I_GTR,  0.15f + srnd(25) * 0.01f);
    instrument_filter(I_BASS, FILTER_LOW, 520 + srnd(220), 2);

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 92 + srnd(23);                          // 92..114
    bpm(tempo);
    apply_band_overrides();          // picked chairs beat the per-song rolls above
    songBase = (long)pos + 8;
    epInit = padInit = false;
    bassLast = 33;
    melLast = sng.melReg;
    songCount++;
#ifdef DE_TRACE
    watch("songsum", "%08X %08X", sng.seed, song_sum());
#endif
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ────────────────────────────────────────────────────────
static int sect_of(long bar)  { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }

// the bridge lifts the whole loop to the subdominant; the LAST chorus (bars
// 48..55) takes the +2 gear change when the song rolled one
static int key_shift(long bar) {
    int s = sect_of(bar);
    if (s == S_BRIDGE) return 5;
    if (sng.gearUp && bar >= 48 && s == S_CHORUS) return 2;
    return 0;
}
static Ch  chord_at(long bar) { return sng.loop[bar % 4]; }
static int root_pc_at(long bar) {
    Ch c = chord_at(bar);
    return (sng.keyPc + c.off + key_shift(bar)) % 12;
}

static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", RAD_PCNAME[(sng.keyPc + c.off) % 12], QN[c.q]);
}

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_CHORUS ? 2 : 1);
    return rad_level(base, intensity);
}

static int mel_note(long bar) {
    Ch c = chord_at(bar);
    int rp = root_pc_at(bar);
    int best = melLast, bestD = 99;
    for (int k = 0; k < 3; k++) {
        int pc = (rp + QV[c.q][k]) % 12;
        int d  = ((pc - melLast) % 12 + 18) % 12 - 6;
        if (rad_iabs(d) + rnd(2) < bestD) { bestD = rad_iabs(d); best = melLast + d; }
    }
    while (best < sng.melReg - 7) best += 12;
    while (best > sng.melReg + 9) best -= 12;
    return melLast = best;
}

static int bass_near(int pc) { return bassLast = rad_bass_to(pc, bassLast, 26, 43); }

// the Purdie swing: off-beat 8ths land at 58%, not 50% (ms added to dly)
static int swing_ms(int step) {
    if (sng.groove != G_PURDIE) return 0;
    return (step % 4 == 2) ? (int)(stepMs * 4 * 0.08) : 0;
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
    int  rp   = root_pc_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  sw   = swing_ms(step);
    bool kit  = sect != S_INTRO && !(sect == S_OUTRO && bar % 8 >= 6);

    // ── THE KIT — three personalities, one seed roll ──
    if (kit) {
        if (sng.groove == G_PURDIE) {
            // half-time: kick 1 (+ pickup), backbeat snare on 3, swung ghosts
            if (step == 0 || (step == 10 && chance(40)))
                { schedule_hit(dly + 1, 36, SL_KICK, 5, 120); vu += 2; }
            if (step == 8)
                { schedule_hit(dly + 1, 60, SL_SN, 5, 90); vu += 2; }
            if (lvl >= 1 && (step == 3 || step == 7 || step == 11 || step == 13) && chance(55))
                schedule_hit(dly + sw + rnd(3), 60, SL_SN, 1, 40);        // the ghosts
            if (step % 2 == 0)
                schedule_hit(dly + sw + 1, 80, SL_HATC, step % 8 == 0 ? 3 : 2, 40);
        } else {
            // straight session 16ths (or the CR-78 reading the same chart)
            if (step == 0 || step == 7 || (step == 10 && chance(30)))
                { schedule_hit(dly + 1, 36, SL_KICK, 5, 110); vu += 2; }
            if (step == 4 || step == 12)
                { schedule_hit(dly + 1, 60, SL_SN, 4, 80); vu += 1.6f; }
            if (step % 2 == 0)
                schedule_hit(dly + 1, 80, SL_HATC, step % 4 == 0 ? 3 : 2, 35);
            else if (lvl >= 3)
                schedule_hit(dly + 1, 80, SL_HATC, 1, 25);
            if (step == 14 && chance(25))
                schedule_hit(dly + 1, 80, SL_HATO, 2, 180);
        }
        if (sng.hasRide && sect == S_CHORUS && step % 4 == 2)
            schedule_hit(dly + sw + 1, 84, SL_RIDE, 2, 200);
    }

    // ── BASS — roots with the chromatic approach run INTO the change ──
    if (sect != S_INTRO) {
        if (step == 0)
            { schedule_hit(dly + 4, bass_near(rp), I_BASS, 5, (int)(stepMs * 3)); vu += 1.6f; }
        else if (step == 8 && sng.groove != G_PURDIE)
            schedule_hit(dly + 4, bassLast + (chance(50) ? 7 : 12), I_BASS, 4, (int)(stepMs * 2));
        else if (step == 14) {                       // the run: approach by semitone
            int np = root_pc_at(bar + 1);
            if (np != rp) {
                int target = bass_near(np);
                bassLast = (bassLast * 0 + target);  // keep the led register
                schedule_hit(dly + 4, target + (chance(60) ? -1 : 1), I_BASS, 4, 90);
                schedule_hit(dly + 4 + (int)stepMs, target, I_BASS, 3, 80);
            }
        }
    }

    // ── THE EPIANO — comping with the citypop anticipation ──
    if (lvl >= 1 || sect == S_INTRO) {
        bool hit_now = (step == 0 && bar % 2 == 0) || step == 6;
        bool anticip = step == 14;                   // pushes the NEXT bar's chord
        if (hit_now || anticip) {
            long cb = anticip ? bar + 1 : bar;
            Ch cc = chord_at(cb);
            rad_lead_to(root_pc_at(cb), QV[cc.q], gvEp, 3, 58, 76, &epInit);
            int vol = anticip ? 4 : (sect == S_CHORUS ? 5 : 4);
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + sw + k * 7 + rnd(3), gvEp[k], I_EP, vol, 700);
            vu += 1.8f;
        }
    }

    // ── STRAT — clean 9th-stab on the back half, never at bar turns ──
    if (lvl >= 2 && (step == 10 || (step == 5 && bar % 2 == 1))) {
        int n = 64 + ((rp + QV[c.q][chance(50) ? 0 : 2]) % 12);
        schedule_hit(dly + sw + rnd(3), n, I_GTR, 3, 350);
        vu += 0.8f;
    }

    // ── SAX — the chorus hook: one cell, re-pitched, a little behind ──
    if (sect == S_CHORUS || (sect == S_BRIDGE && lvl >= 2)) {
        int cs = (int)(s % 32);
        for (int i = 0; i < sng.melN; i++)
            if (sng.melOn[i] == cs && chance(85)) {
                int m = mel_note(bar);
                int gap = (i + 1 < sng.melN ? sng.melOn[i + 1] : sng.melOn[0] + 32) - cs;
                schedule_hit(dly + sw + 14 + rnd(6), m, I_SAX, 5,
                             gap >= 6 ? (int)(stepMs * 5) : (int)(stepMs * 2));
                vu += 1.4f;
            }
    }

    // ── PAD — strings under the chorus only ──
    if (sect == S_CHORUS && step == 0) {
        rad_lead_to(root_pc_at(bar), QV[c.q], gvPad, 3, 50, 67, &padInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 11, gvPad[k], I_PAD, 2, (int)(stepMs * 15));
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_kit_session(void) {
    instrument(SL_KICK, INSTR_SINE, 0, 230, 0, 50);          // tight studio kick
    instrument_filter(SL_KICK, FILTER_LOW, 300, 2);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 35, 16.0f);
    instrument(SL_SN, INSTR_NOISE, 0, 120, 0, 60);           // dry session snare
    instrument_filter(SL_SN, FILTER_BAND, 1500, 4);
    instrument_env(SL_SN, 0, ENV_CUTOFF, 0, 80, 800.0f);
    instrument(SL_HATC, INSTR_NOISE, 0, 35, 0, 16);
    instrument_filter(SL_HATC, FILTER_HIGH, 8000, 3);
    instrument(SL_HATO, INSTR_NOISE, 0, 240, 0, 90);
    instrument_filter(SL_HATO, FILTER_HIGH, 7400, 3);
}
static void setup_kit_cr78(void) {
    // — CompuRhythm circuits, verbatim from cr78.c (the Hall & Oates move) —
    instrument(SL_KICK, INSTR_SINE, 0, 170, 0, 40);
    instrument_filter(SL_KICK, FILTER_LOW, 320, 2);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 45, 13.0f);
    instrument(SL_SN, INSTR_NOISE, 0, 130, 0, 35);
    instrument_filter(SL_SN, FILTER_BAND, 1700, 3);
    instrument_env(SL_SN, 0, ENV_CUTOFF, 0, 90, 900.0f);
    instrument(SL_HATC, INSTR_NOISE, 0, 40, 0, 18);
    instrument_filter(SL_HATC, FILTER_BAND, 7800, 4);
    instrument(SL_HATO, INSTR_NOISE, 0, 320, 0, 80);
    instrument_filter(SL_HATO, FILTER_BAND, 7200, 4);
}

// ── THE BAND — each chair's candidates are full recipes; switching re-aims the
// slot from scratch so a swap mid-song is clean. sel 0 is always the shipped
// marina sound (verbatim from the original setup_instruments). Candidates from
// docs/design/radio-instrument-options.md (the yacht section).
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chEp) {
        if (sel == 0) {
            // dx tine — INSTR_FM, 1:1 detent: the DX tine pings every comp hit
            instrument(I_EP, INSTR_FM, 2, 700, 3, 350);
            instrument_harmonics(I_EP, 0.15f);               // the epiano detent
            instrument_lfo(I_EP, 0, LFO_VOLUME, 4.6f, 0.10f); // the session tremolo
        } else if (sel == 1) {
            // soft rhodes — duller tine, a touch more tremolo
            instrument(I_EP, INSTR_FM, 2, 700, 3, 350);
            instrument_harmonics(I_EP, 0.15f);
            instrument_timbre(I_EP, 0.30f);
            instrument_lfo(I_EP, 0, LFO_VOLUME, 4.6f, 0.14f);
        } else {
            // clavinet — bright + percussive, near-bridge pluck
            instrument(I_EP, INSTR_PLUCK, 1, 0, 6, 300);
            instrument_harmonics(I_EP, 0.35f);
            instrument_timbre(I_EP, 0.85f);
            instrument_morph(I_EP, 0.15f);
            instrument_filter(I_EP, FILTER_LOW, 3000, 1);
        }
    } else if (idx == chBass) {
        if (sel == 0) {
            instrument(I_BASS, INSTR_TRI, 2, 220, 4, 90);    // round fingered electric
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 14, 3);
        } else if (sel == 1) {
            instrument(I_BASS, INSTR_SINE, 2, 220, 4, 90);   // round — pure low end
            instrument_filter(I_BASS, FILTER_LOW, 500, 2);
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 14, 3);
        } else {
            instrument(I_BASS, INSTR_SAW, 2, 220, 4, 90);    // bright — slap-adjacent
            instrument_filter(I_BASS, FILTER_LOW, 900, 2);
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 14, 3);
        }
    } else if (idx == chLead) {
        if (sel == 0) {
            instrument(I_SAX, INSTR_SQUARE, 25, 160, 5, 130); // breathy narrow pulse
            instrument_duty(I_SAX, 0.12f);
            instrument_filter(I_SAX, FILTER_LOW, 1900, 2);
            instrument_lfo(I_SAX, 0, LFO_PITCH, 5.1f, 0.16f);
        } else if (sel == 1) {
            instrument(I_SAX, INSTR_SQUARE, 25, 160, 5, 130); // synth — fatter pulse
            instrument_duty(I_SAX, 0.30f);
            instrument_filter(I_SAX, FILTER_LOW, 2200, 2);
            instrument_lfo(I_SAX, 0, LFO_PITCH, 5.1f, 0.10f);
        } else {
            instrument(I_SAX, INSTR_PLUCK, 1, 0, 6, 500);     // the session guitar solo
            instrument_harmonics(I_SAX, 0.5f);
            instrument_timbre(I_SAX, 0.75f);
            instrument_filter(I_SAX, FILTER_LOW, 2600, 1);
        }
    } else if (idx == chPad) {
        if (sel == 0) {
            instrument(I_PAD, INSTR_SAW, 260, 400, 5, 700);   // soft strings
            instrument_filter(I_PAD, FILTER_LOW, 850, 1);
        } else {
            instrument(I_PAD, INSTR_SAW, 10, 400, 5, 700);    // syn brass — short attack
            instrument_filter(I_PAD, FILTER_LOW, 1400, 1);
            instrument_env(I_PAD, 0, ENV_PITCH, 0, 40, -2);   // the citypop brass fall
        }
    }
}

// re-assert any non-default chair (new_song re-rolls some slots per song;
// default chairs keep that roll, picked chairs win over it)
static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chEp   = rad_chair(&band, "epiano", "dx tine", "soft rhodes", "clavinet", NULL);
    chBass = rad_chair(&band, "bass",   "fingered", "round", "bright", NULL);
    chLead = rad_chair(&band, "lead",   "sax", "synth", "guitar", NULL);
    chPad  = rad_chair(&band, "pad",    "strings", "syn brass", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);

    instrument(I_GTR, INSTR_PLUCK, 1, 0, 7, 600);            // clean strat stabs
    instrument_harmonics(I_GTR, 0.35f);
    instrument_filter(I_GTR, FILTER_LOW, 2800, 1);

    instrument(SL_RIDE, INSTR_SQUARE, 0, 300, 0, 160);       // ride ping
    instrument_filter(SL_RIDE, FILTER_HIGH, 6000, 4);
    setup_kit_session();
}

// ── update ────────────────────────────────────────────────────────────────
static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_EP,  FILTER_LOW, (int)(3200 * tm), 1);
    instrument_filter(I_SAX, FILTER_LOW, (int)(1900 * tm), 2);
}

void update(void) {
    static bool booted = false;
    static int  kitNow = -1;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (YACHT_SEED) { new_song(pos, YACHT_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 84, 120, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) { note_off_all(); sfx(-1); }
        else scheduled = (long)pos;
    }
    if (ev & (RAD_EV_TONE | RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD) || kitNow != sng.groove) {
        apply_tone();
        if (kitNow != sng.groove) {                  // swap the kit circuits per song
            if (sng.groove == G_CR78) setup_kit_cr78();
            else setup_kit_session();
            kitNow = sng.groove;
        }
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

    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("chord", "%s", nowChord[(int)(tbar % 4)]);
    watch("groove", "%s", GROOVENAME[sng.groove]);
    watch("gear", "%d", key_shift(tbar));
#endif
}

// ── draw — the marina chassis; window art = a yacht at golden hour ────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_LIGHT_GREY, CLR_TRUE_BLUE);      // chrome shell, ocean trim
    rad_dial(sng.freq, CLR_TRUE_BLUE);

    // the window — golden hour at the marina
    rectfill(34, 52, 102, 116, CLR_DARKER_BLUE);             // evening sky
    circfill(58, 96, 9, CLR_YELLOW);                         // the low sun
    circfill(58, 96, 6, CLR_LIGHT_YELLOW);
    rectfill(34, 108, 102, 60, CLR_BLUE_GREEN);              // the sea
    for (int y = 112; y < 164; y += 5) {                     // sun's wake
        int w = 6 + (164 - y) / 4;
        int x0 = 58 - w / 2 + (int)(sinf(timer() * 1.1f + y) * 2);
        line(x0, y, x0 + w, y, (y / 5) % 2 ? CLR_YELLOW : CLR_DARK_ORANGE);
    }
    // the yacht, bobbing on the swell
    {
        int bx = 92, by = 116 + (int)(sinf(timer() * 0.9f) * 2);
        trifill(bx - 14, by, bx + 14, by, bx + 9, by + 6, CLR_WHITE);     // hull
        line(bx, by - 1, bx, by - 26, CLR_DARK_GREY);                     // mast
        trifill(bx + 1, by - 25, bx + 1, by - 6, bx + 13, by - 6, CLR_LIGHT_PEACH);  // main
        trifill(bx - 1, by - 22, bx - 1, by - 6, bx - 10, by - 6, CLR_WHITE);        // jib
    }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_TRUE_BLUE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_BLUE);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq, GROOVENAME[sng.groove]);
        print(l2, 154, 70, CLR_TRUE_BLUE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_TRUE_BLUE);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_BLUE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        rad_chord_row(nowChord, 4, (int)(bar % 4), 152, 104, CLR_BLUE);
        char sl[24];
        snprintf(sl, 24, "%s%s", SECTNAME[sect],
                 key_shift(bar) == 2 ? " +2!" : key_shift(bar) == 5 ? " (IV)" : "");
        print(sl, 152, 120, CLR_BLUE);
        rad_phrase_dots(232, 124, 8, bar / 8, CLR_BLUE);
    }

    static const char *FEEL[4] = { "dock", "cruise", "regatta", "open sea" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_BLUE);
    if (rad_knob_int(&tempo, 84, 120, 2, 218, 148, 9, "tempo", CLR_BLUE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_BLUE)) apply_tone();
    rad_power_led(radioOn, CLR_BLUE, CLR_DARKER_BLUE);

    rad_help_button(CLR_BLUE);
    rad_band_button(CLR_BLUE);

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next track (rolls a new seed)" },
            { "R",          "same track, same band, again" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - dock/cruise/regatta/sea" },
            { "UP/DOWN",    "tempo of this track" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - swap chairs live" },
        };
        static const char *NOTES[3] = {
            "the MU chord + the FM epiano's tine on",
            "every comp. grooves: session / purdie",
            "shuffle / cr-78. pin: YACHT_SEED 0x...",
        };
        rad_help_panel("YACHT RADIO", HELP, 8, NOTES, 3, CLR_BLUE);
    }
    rad_band_panel(&band, CLR_BLUE);
    ui_end();
}
