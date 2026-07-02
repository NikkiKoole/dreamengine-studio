/* de:meta
{
  "title": "pocket groove box",
  "status": "active",
  "created": "2026-07-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "subtractive-synth",
    "drum-synthesis",
    "swing-timing"
  ],
  "todo": [
    "seeded ROLL-a-groove generator (the tinyjam generate-play-export angle; docs/design/pocketbox.md open Qs)",
    "spec() for the sequencer brain: trigger conditions, chord-degree resolution, pattern link, song parts",
    "WAV export button (arm .bake/wav_request like acidrack)",
    "mixer page with per-track level meters (the combo VU proxy)",
    "per-track arpeggiator page (NM_ARP note mode exists; no rate/direction control yet)",
    "extend the FX-copy idiom to steps and tracks (patterns only today)",
    "chained playback drags the editing view along (tr.pat is both play + edit pointer) - split them?",
    "hold-Play+step key solo (mute ships; solo missing)"
  ],
  "homage": "Wee Noise Makers PGB-1 (Fabien Chouteau's open-source Ada groovebox) — the buttons-only grammar reimagined: 16 step keys, 4 modes, hold-combos, one touch strip, an honest 128x64 'OLED'.",
  "description": {
    "summary": "A pocket groovebox with NO knobs: 8 fixed-role tracks (kick/snare/hat/bass/lead/chords + 2 drawn-wave tracks), 16 patterns each, p-locks, ratchets, trigger conditions, a chord track that basslines follow, live FX, song parts.",
    "detail": "Everything is 16 step keys + 4 mode keys + hold-combos + one touch strip, like the hardware. TRACK mode: keys pick a track, pages set engine/params/LFO/FX bus. STEP mode: keys toggle steps; HOLD a step + strip/arrows to set per-step note, velocity, ratchets, Elektron-style conditions and parameter locks. PATTERN mode: 16 slots per track (independent = polymeter), length + link. SONG mode: 12 parts. Hold EDIT = play notes; hold PLAY + key = mute; hold FX + key = live FX (filter sweeps, tape dive, crush, stutter-freeze, fill). Wave tracks play single-cycle waves you draw with a finger right on the OLED.",
    "controls": "tap keys or: Z/X/C/V=track/step/pattern/song mode, 1-8+Q-I=the 16 step keys, arrows=pages/values, Enter=A, Backspace=B, Space=play (hold+arrows=BPM, hold+key=mute), N(hold)=edit/keyboard (GarageBand keys AWSEDF..., Z/X=octave), M(hold)=FX/copy, strip=drag the bottom ribbon"
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// pocket groove box — the Wee Noise Makers PGB-1 reimagined (see
// docs/design/pocketbox.md for the full design + research pass).
//
// The whole idea: a real groovebox operated through 16 step keys, 4 mode keys,
// hold-combos and ONE touch strip — no knobs anywhere. The 128x64 "OLED" is a
// real window drawn 1:1: header, dot page-indicator, one parameter at a time.
//
//   TRACK mode   step keys 1-8 select a track; pages: ENGINE, 4 params, VOL,
//                PAN, FX BUS, OCT, SHUFFLE, LFO x3 (+DRAW on wave tracks)
//   STEP mode    step keys toggle steps; HOLD a step key + strip/up-down edits
//                that step on the current page (NOTE/VEL/DUR/REP/RATE/COND/
//                LK1-4 = parameter locks)
//   PATTERN mode step keys pick this track's pattern (per-track = polymeter);
//                pages: LENGTH, LINK. hold FX + source key + dest key = copy
//   SONG mode    step keys 1-12 pick a part; A captures the current pattern
//                setup into it, up/down = repeats, CHAIN page arms song mode
//   hold EDIT    the 16 keys become a chromatic keyboard (and GarageBand keys
//                AWSEDFTGYHUJ... play too, Z/X octave); in STEP mode a played
//                note is written to the held/selected step
//   hold PLAY    + step key = mute that track; + left/right = BPM
//   hold FX      + step keys = live FX toggles: LPF HPF DIVE CRUSH TREM
//                FREEZE ROLL FILL; + strip = ride the filter cutoff
//
// The chord track (track 6) is the song's harmony: its steps place chords
// (root + quality), and bass/lead steps in CHD1..4 note mode follow whatever
// chord is sounding. Wave tracks 7/8 play the four drawn single-cycle waves
// (INSTR_USER0..3) — draw them with a finger on the OLED's DRAW page.

// ── screen geometry (320x200) ─────────────────────────────────────────────
#define OX 96          // OLED window
#define OY 4
#define OW 128
#define OH 64

#define MKX 4          // mode keys column (left of OLED)
#define MKW 84
#define MKH 15
#define MKS 17

#define TRY 74         // transport row (PLAY EDIT FX)
#define TRH 22

#define SKX 2          // step keys: two rows of 8
#define SKW 36
#define SKH 32
#define SKS 40
#define SKY1 100
#define SKY2 136

#define STY 180        // touch strip
#define STH 18

// ── data model ────────────────────────────────────────────────────────────
#define NPP    4       // engine params per track
#define NSTEP  16
#define NPAT   16
#define NPART  12

enum { T_KICK, T_SNARE, T_HAT, T_BASS, T_LEAD, T_CHORD, T_WAVE1, T_WAVE2, NTRACK };
#define SLOT(t) (5 + (t) * 2)
#define AUXS(t) (6 + (t) * 2)

enum { NM_FIX, NM_CH1, NM_CH2, NM_CH3, NM_CH4, NM_ARP, NNM };            // melodic note modes
enum { Q_MIN, Q_MAJ, Q_MIN7, Q_MAJ7, Q_DOM7, Q_SUS4, NQ };               // chord-track qualities
enum { BUS_DRY, BUS_DRIVE, BUS_VERB, BUS_CRUSH, NBUS };

typedef struct {                    // one step (chord track: note=root, nmode=quality)
    unsigned char on, nmode, note, vel, dur, rep, rate, cond;
    signed char   lock[NPP];        // p-locks on the 4 engine params, -1 = none
} Step;

typedef struct { Step st[NSTEP]; unsigned char len, link; } Pattern;

typedef struct {                    // everything saved per track
    unsigned char engine, bus, oct, shuf, lfod;
    float p[NPP], vol, pan, lfor, lfoa;
    unsigned char pat;              // current pattern slot — per-track = polymeter
    Pattern bank[NPAT];
} Track;

typedef struct { unsigned char pat[NTRACK]; unsigned char reps; } Part;  // reps 0 = empty

// ── engines: name + 4 macro params + setup/fire per role ─────────────────
typedef struct {
    const char *name;
    const char *pn[NPP];
    float def[NPP];
    int   defdur;                                              // ms when step dur = AUTO
    void (*setup)(int t, const float *p);
    void (*fire)(int t, int midi, int vol, int dur, const float *p);  // NULL = plain hit
} Engine;

static int tmidi(float p, int base, int range) { return base + (int)((p - 0.5f) * range); }

// KICK ---------------------------------------------------------------------
static void su_k909(int t, const float *p) {
    instrument(SLOT(t), INSTR_SINE, 0, 120 + (int)(p[1] * 480), 0, 60);
    instrument_env(SLOT(t), 0, ENV_PITCH, 0, 55, 10 + p[2] * 38);
    instrument(AUXS(t), INSTR_NOISE, 0, 16, 0, 10);
}
static void fi_k909(int t, int midi, int vol, int dur, const float *p) {
    (void)midi;
    hit(tmidi(p[0], 34, 20), SLOT(t), vol, dur);
    if (p[3] > 0.03f) hit(72, AUXS(t), (int)(vol * p[3]), 12);
}
static void su_k808(int t, const float *p) {
    instrument(SLOT(t), INSTR_SINE, 0, 260 + (int)(p[1] * 640), 0, 120);
    instrument_env(SLOT(t), 0, ENV_PITCH, 0, 80, 4 + p[2] * 20);
    instrument_filter(SLOT(t), FILTER_LOW, 160 + (int)(p[3] * 1800), 0);
}
static void fi_k808(int t, int midi, int vol, int dur, const float *p) {
    (void)midi; hit(tmidi(p[0], 30, 18), SLOT(t), vol, dur);
}
static void su_kmem(int t, const float *p) {
    instrument(SLOT(t), INSTR_MEMBRANE, 0, 100 + (int)(p[1] * 500), 0, 80);
    instrument_harmonics(SLOT(t), p[2]);
    instrument_timbre(SLOT(t), p[3]);
}
static void fi_kmem(int t, int midi, int vol, int dur, const float *p) {
    (void)midi; hit(tmidi(p[0], 40, 24), SLOT(t), vol, dur);
}

// SNARE --------------------------------------------------------------------
static void su_s909(int t, const float *p) {
    instrument(SLOT(t), INSTR_NOISE, 0, 70 + (int)(p[1] * 240), 0, 50);
    instrument_filter(SLOT(t), FILTER_BAND, 900 + (int)(p[0] * 1900), 3);
    instrument(AUXS(t), INSTR_TRI, 0, 45, 0, 30);
}
static void fi_s909(int t, int midi, int vol, int dur, const float *p) {
    (void)midi;
    hit(58, SLOT(t), vol, dur);
    if (p[2] > 0.03f) hit(tmidi(p[3], 53, 14), AUXS(t), (int)(vol * p[2]), 45);
}
static void su_s808(int t, const float *p) {
    instrument(SLOT(t), INSTR_NOISE, 0, 60 + (int)(p[1] * 180), 0, 40);
    instrument_filter(SLOT(t), FILTER_BAND, 600 + (int)(p[0] * 1200), 2);
    instrument(AUXS(t), INSTR_SINE, 0, 60, 0, 40);
}
static void fi_s808(int t, int midi, int vol, int dur, const float *p) {
    (void)midi;
    hit(55, SLOT(t), vol, dur);
    if (p[2] > 0.03f) hit(tmidi(p[3], 50, 12), AUXS(t), (int)(vol * p[2]), 60);
}
static void su_clap(int t, const float *p) {
    instrument(SLOT(t), INSTR_NOISE, 0, 45 + (int)(p[1] * 80), 0, 30);
    instrument_filter(SLOT(t), FILTER_BAND, 800 + (int)(p[0] * 1400), 5);
}
static void fi_clap(int t, int midi, int vol, int dur, const float *p) {
    (void)midi; (void)dur;
    int sp = 8 + (int)(p[2] * 16);                             // burst spacing
    hit(64, SLOT(t), vol, 35);
    schedule_hit(sp, 64, SLOT(t), vol > 1 ? vol - 1 : 1, 30);
    schedule_hit(sp * 2, 64, SLOT(t), vol, 40 + (int)(p[3] * 120));
}

// HAT ----------------------------------------------------------------------
static void su_h808(int t, const float *p) {
    instrument(SLOT(t), INSTR_NOISE, 0, 16 + (int)(p[1] * 280), 0, 12);
    instrument_filter(SLOT(t), FILTER_HIGH, 4200 + (int)(p[0] * 5200), (int)(p[2] * 7));
}
static void fi_h808(int t, int midi, int vol, int dur, const float *p) {
    (void)midi; hit(92, SLOT(t), vol, dur > 0 ? dur : 18 + (int)(p[1] * 280)); (void)p;
}
static void su_hfm(int t, const float *p) {
    instrument(SLOT(t), INSTR_FM, 0, 18 + (int)(p[1] * 240), 0, 16);
    instrument_harmonics(SLOT(t), 0.55f + p[2] * 0.4f);        // off-integer = clangy bell
    instrument_timbre(SLOT(t), 0.5f + p[3] * 0.5f);
}
static void fi_hfm(int t, int midi, int vol, int dur, const float *p) {
    (void)midi; hit(tmidi(p[0], 96, 12), SLOT(t), vol, dur > 0 ? dur : 18 + (int)(p[1] * 240));
}

// BASS ---------------------------------------------------------------------
static void su_bacid(int t, const float *p) {
    instrument(SLOT(t), INSTR_SAW, 0, 90 + (int)(p[3] * 320), 2, 60);
    instrument_filter(SLOT(t), FILTER_DIODE, 110 + (int)(p[0] * 2600), (int)(p[1] * 14));
    instrument_env(SLOT(t), 0, ENV_CUTOFF, 0, 60 + (int)(p[3] * 220), p[2] * 3200);
}
static void su_bsub(int t, const float *p) {
    instrument(SLOT(t), INSTR_SINE, 0, 140 + (int)(p[1] * 500), 3, 80);
    instrument_env(SLOT(t), 0, ENV_PITCH, 0, 30, p[2] * 12);
    instrument_filter(SLOT(t), FILTER_LOW, 200 + (int)(p[0] * 900), (int)(p[3] * 6));
}
static void su_bfm(int t, const float *p) {
    instrument(SLOT(t), INSTR_FM, 0, 100 + (int)(p[3] * 400), 2, 70);
    instrument_harmonics(SLOT(t), 0.12f + p[0] * 0.3f);        // low integers = FM bass
    instrument_timbre(SLOT(t), p[1]);
    instrument_filter(SLOT(t), FILTER_LOW, 300 + (int)(p[2] * 3000), 1);
}
static void su_bpd(int t, const float *p) {
    instrument(SLOT(t), INSTR_PD, 0, 100 + (int)(p[3] * 400), 2, 70);
    instrument_harmonics(SLOT(t), p[0]);
    instrument_timbre(SLOT(t), p[1]);
    instrument_filter(SLOT(t), FILTER_LOW, 300 + (int)(p[2] * 3000), 2);
}

// LEAD ---------------------------------------------------------------------
static void su_lpd(int t, const float *p) {
    instrument(SLOT(t), INSTR_PD, 2, 120 + (int)(p[3] * 500), 3, 90);
    instrument_harmonics(SLOT(t), p[0]);
    instrument_timbre(SLOT(t), p[1]);
    instrument_filter(SLOT(t), FILTER_LOW, 500 + (int)(p[2] * 5000), 2);
}
static void su_lchip(int t, const float *p) {
    instrument(SLOT(t), INSTR_SQUARE, 0, 80 + (int)(p[3] * 400), 3, 40);
    instrument_duty(SLOT(t), 0.08f + p[0] * 0.42f);
    instrument_filter(SLOT(t), FILTER_LOW, 600 + (int)(p[1] * 6000), (int)(p[2] * 8));
}
static void su_lpluck(int t, const float *p) {
    instrument(SLOT(t), INSTR_PLUCK, 0, 200 + (int)(p[3] * 500), 0, 120);
    instrument_harmonics(SLOT(t), p[0]);                       // ring time
    instrument_timbre(SLOT(t), p[1]);                          // pick brightness
    instrument_filter(SLOT(t), FILTER_LOW, 800 + (int)(p[2] * 7000), 0);
}
static void su_lbell(int t, const float *p) {
    instrument(SLOT(t), INSTR_FM, 0, 200 + (int)(p[3] * 700), 1, 200);
    instrument_harmonics(SLOT(t), 0.62f + p[0] * 0.35f);       // off-integer = bell
    instrument_timbre(SLOT(t), p[1]);
    instrument_filter(SLOT(t), FILTER_LOW, 1000 + (int)(p[2] * 7000), 0);
}

// CHORD --------------------------------------------------------------------
static void su_cpad(int t, const float *p) {
    instrument(SLOT(t), INSTR_SAW, 18 + (int)(p[2] * 120), 200 + (int)(p[3] * 600), 3, 160);
    instrument_filter(SLOT(t), FILTER_LOW, 300 + (int)(p[0] * 3500), (int)(p[1] * 8));
}
static void su_cep(int t, const float *p) {
    instrument(SLOT(t), INSTR_EPIANO, 0, 250 + (int)(p[3] * 700), 1, 200);
    instrument_harmonics(SLOT(t), p[0]);
    instrument_timbre(SLOT(t), p[1]);
    instrument_filter(SLOT(t), FILTER_LOW, 800 + (int)(p[2] * 6000), 0);
}
static void su_corg(int t, const float *p) {
    instrument(SLOT(t), INSTR_ORGAN, 4, 200 + (int)(p[3] * 600), 4, 120);
    instrument_harmonics(SLOT(t), p[0]);
    instrument_timbre(SLOT(t), p[1]);
    instrument_filter(SLOT(t), FILTER_LOW, 600 + (int)(p[2] * 6000), 0);
}

// WAVE (the drawn single-cycle waves, INSTR_USER0..3) ------------------------
static void su_wave_n(int t, const float *p, int which) {
    instrument(SLOT(t), INSTR_USER0 + which, 0, 60 + (int)(p[0] * 700), 2, 60);
    instrument_filter(SLOT(t), FILTER_LOW, 200 + (int)(p[1] * 6000), (int)(p[2] * 12));
    instrument_env(SLOT(t), 0, ENV_PITCH, 0, 40, p[3] * 24);
}
static void su_w0(int t, const float *p) { su_wave_n(t, p, 0); }
static void su_w1(int t, const float *p) { su_wave_n(t, p, 1); }
static void su_w2(int t, const float *p) { su_wave_n(t, p, 2); }
static void su_w3(int t, const float *p) { su_wave_n(t, p, 3); }

static const Engine EN_KICK[] = {
    { "909 KICK", { "TUNE", "DEC", "PNCH", "CLIK" }, { 0.4f, 0.35f, 0.5f, 0.4f }, 240, su_k909, fi_k909 },
    { "808 KICK", { "TUNE", "DEC", "PNCH", "TONE" }, { 0.4f, 0.5f, 0.3f, 0.5f }, 420, su_k808, fi_k808 },
    { "DRUMHEAD", { "TUNE", "DEC", "MATL", "STIK" }, { 0.4f, 0.4f, 0.4f, 0.5f }, 300, su_kmem, fi_kmem },
};
static const Engine EN_SNARE[] = {
    { "909 SNR",  { "TONE", "DEC", "BODY", "TUNE" }, { 0.3f, 0.3f, 0.5f, 0.5f }, 120, su_s909, fi_s909 },
    { "808 SNR",  { "TONE", "DEC", "BODY", "TUNE" }, { 0.4f, 0.3f, 0.55f, 0.5f }, 110, su_s808, fi_s808 },
    { "CLAP",     { "TONE", "DEC", "SPRD", "TAIL" }, { 0.25f, 0.4f, 0.3f, 0.3f }, 100, su_clap, fi_clap },
};
static const Engine EN_HAT[] = {
    { "808 HAT",  { "CUT", "DEC", "RES", "-" },      { 0.5f, 0.1f, 0.2f, 0 },    30,  su_h808, fi_h808 },
    { "FM HAT",   { "TUNE", "DEC", "CLNG", "BITE" }, { 0.5f, 0.15f, 0.5f, 0.5f }, 30, su_hfm, fi_hfm },
};
static const Engine EN_BASS[] = {
    { "ACID",     { "CUT", "RES", "ENV", "DEC" },    { 0.35f, 0.55f, 0.45f, 0.3f }, 130, su_bacid, 0 },
    { "SUB",      { "TONE", "DEC", "PNCH", "RES" },  { 0.5f, 0.4f, 0.3f, 0.1f },  200, su_bsub, 0 },
    { "FM BASS",  { "RATO", "BITE", "CUT", "DEC" },  { 0.3f, 0.5f, 0.5f, 0.3f },  160, su_bfm, 0 },
    { "PD BASS",  { "HARM", "DIST", "CUT", "DEC" },  { 0.4f, 0.5f, 0.4f, 0.3f },  160, su_bpd, 0 },
};
static const Engine EN_LEAD[] = {
    { "PD LEAD",  { "HARM", "DIST", "CUT", "DEC" },  { 0.5f, 0.4f, 0.6f, 0.4f },  200, su_lpd, 0 },
    { "CHIP",     { "DUTY", "CUT", "RES", "DEC" },   { 0.4f, 0.7f, 0.2f, 0.3f },  150, su_lchip, 0 },
    { "PLUCK",    { "RING", "PICK", "CUT", "DEC" },  { 0.6f, 0.6f, 0.7f, 0.4f },  300, su_lpluck, 0 },
    { "FM BELL",  { "RATO", "BITE", "CUT", "DEC" },  { 0.4f, 0.5f, 0.6f, 0.5f },  400, su_lbell, 0 },
};
static const Engine EN_CHORD[] = {
    { "SAW PAD",  { "CUT", "RES", "ATT", "DEC" },    { 0.45f, 0.25f, 0.25f, 0.5f }, 400, su_cpad, 0 },
    { "E.PIANO",  { "TINE", "BARK", "CUT", "DEC" },  { 0.5f, 0.4f, 0.6f, 0.5f },  400, su_cep, 0 },
    { "ORGAN",    { "DRAW", "TONE", "CUT", "DEC" },  { 0.5f, 0.5f, 0.6f, 0.4f },  350, su_corg, 0 },
};
static const Engine EN_WAVE[] = {
    { "WAVE A",   { "DEC", "CUT", "RES", "PNCH" },   { 0.35f, 0.6f, 0.2f, 0 },   250, su_w0, 0 },
    { "WAVE B",   { "DEC", "CUT", "RES", "PNCH" },   { 0.35f, 0.6f, 0.2f, 0 },   250, su_w1, 0 },
    { "WAVE C",   { "DEC", "CUT", "RES", "PNCH" },   { 0.35f, 0.6f, 0.2f, 0 },   250, su_w2, 0 },
    { "WAVE D",   { "DEC", "CUT", "RES", "PNCH" },   { 0.35f, 0.6f, 0.2f, 0 },   250, su_w3, 0 },
};
static const struct { const Engine *e; int n; } MENU[NTRACK] = {
    { EN_KICK, 3 }, { EN_SNARE, 3 }, { EN_HAT, 2 }, { EN_BASS, 4 },
    { EN_LEAD, 4 }, { EN_CHORD, 3 }, { EN_WAVE, 4 }, { EN_WAVE, 4 },
};

static const char *TNAME[NTRACK] = { "KICK", "SNARE", "HAT", "BASS", "LEAD", "CHORD", "WAVE1", "WAVE2" };
static const int   TCLR[NTRACK]  = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_BLUE, CLR_PINK, CLR_GREEN,
                                     CLR_INDIGO, CLR_PEACH };
static const unsigned char DEFNOTE[NTRACK] = { 36, 38, 42, 36, 60, 48, 60, 60 };
static const unsigned char DEFNM[NTRACK]   = { NM_FIX, NM_FIX, NM_FIX, NM_CH1, NM_CH1, Q_MIN, NM_FIX, NM_FIX };

// ── trigger conditions (Elektron-style) ──────────────────────────────────
typedef struct { const char *name; signed char num, den, pct; } Cond;
static const Cond CONDS[] = {
    { "ALL", 0, 0, 100 }, { "75%", 0, 0, 75 }, { "50%", 0, 0, 50 }, { "25%", 0, 0, 25 },
    { "FILL", 0, 0, -1 }, { "!FIL", 0, 0, -2 },
    { "1:2", 1, 2, 0 }, { "2:2", 2, 2, 0 }, { "1:3", 1, 3, 0 }, { "3:3", 3, 3, 0 },
    { "1:4", 1, 4, 0 }, { "2:4", 2, 4, 0 }, { "4:4", 4, 4, 0 },
};
#define NCOND ((int)(sizeof CONDS / sizeof CONDS[0]))

// chord qualities → intervals
static const char *QNAME[NQ] = { "MIN", "MAJ", "MIN7", "MAJ7", "DOM7", "SUS4" };
static const int QIV[NQ][4] = {
    { 0, 3, 7, 12 }, { 0, 4, 7, 12 }, { 0, 3, 7, 10 }, { 0, 4, 7, 11 }, { 0, 4, 7, 10 }, { 0, 5, 7, 12 },
};
static const char *NOTENAME[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// ── state ─────────────────────────────────────────────────────────────────
enum { M_TRACK, M_STEP, M_PATT, M_SONG, NMODE };
static const char *MODENAME[NMODE] = { "TRACK", "STEP", "PATTERN", "SONG" };

static Track tr[NTRACK];
static Part  parts[NPART];
static signed char waves[4][64];      // the drawn single-cycle waves, -100..100
static int   tempo = 118;
static bool  songmode;

static int   mode = M_STEP;
static int   seltrack = T_KICK;
static int   selstep;                 // last touched step (per current track view)
static int   selpart;
static int   tpage[NTRACK];           // TRACK-mode page per track
static int   spage;                   // STEP-mode page
static int   ppage, gpage;            // PATTERN / SONG mode pages

static bool  running = true;
static int   master16 = -1;           // last integer sixteenth of the master clock
static int   ttick[NTRACK];           // per-track step tick since transport start
static int   tlast16[NTRACK];         // per-track last swung sixteenth
static int   tstep[NTRACK];           // per-track current step (view/playhead)
static int   tloops[NTRACK];          // completed pattern loops (for X:Y conditions)
static float applied[NTRACK][NPP];    // memoized engine params (p-locks touch these)
static int   applied_eng[NTRACK];
static int   arpi[NTRACK];
static int   cur_chord[4] = { 48, 51, 55, 60 };   // C minor until the chord track speaks
static int   flash[NTRACK];           // frame() when a track last fired
static bool  mute[NTRACK];

static int   kb_oct;                  // hold-EDIT keyboard octave offset

// live FX
enum { LFX_LP, LFX_HP, LFX_DIVE, LFX_CRUSH, LFX_TREM, LFX_FREEZE, LFX_ROLL, LFX_FILL, NFX };
static const char *FXNAME[NFX] = { "LPF", "HPF", "DIVE", "CRSH", "TREM", "FRZ", "ROLL", "FILL" };
static bool  fxon[NFX];
static float fxcut = 0.5f;            // strip-ridden filter position

// hold-combo state
static bool  play_held, edit_held, fx_held, prev_play_held;
static bool  play_combo;
static int   held_stepkey = -1;       // which step key a finger is holding (for p-lock grammar)
static int   copy_src = -1;           // FX-copy source (PATT mode)
static int   save_cooldown;
static int   songbar, songpos;

// ── save blob ─────────────────────────────────────────────────────────────
typedef struct {
    unsigned magic;
    int      tempo;
    unsigned char songmode;
    Track    tr[NTRACK];
    Part     parts[NPART];
    signed char waves[4][64];
} Blob;
#define MAGIC 0x50474231u             // 'PGB1'

static void mark_dirty(void) { save_cooldown = 90; }

// ── engine application ────────────────────────────────────────────────────
static void apply_engine(int t, const float *p) {
    const Engine *e = &MENU[t].e[tr[t].engine];
    e->setup(t, p);
    for (int i = 0; i < NPP; i++) applied[t][i] = p[i];
    applied_eng[t] = tr[t].engine;
}
static void apply_bus(int t) {
    for (int s = SLOT(t); s <= AUXS(t); s++) {
        instrument_drive(s, tr[t].bus == BUS_DRIVE ? 0.45f : 0);
        instrument_reverb(s, tr[t].bus == BUS_VERB ? 0.55f : 0);
        instrument_crush(s, 6, 5, tr[t].bus == BUS_CRUSH ? 0.8f : 0);
    }
}
static void apply_lfo(int t) {
    static const int DEST[] = { 0, LFO_PITCH, LFO_VOLUME, LFO_CUTOFF, LFO_DUTY, LFO_PAN };
    float dep = tr[t].lfoa;
    int   d   = tr[t].lfod;
    float amt = d == 1 ? dep * 7 : d == 3 ? dep * 3000 : dep;      // semitones / Hz / 0..1
    instrument_lfo(SLOT(t), 2, d ? DEST[d] : LFO_PITCH, 0.1f + tr[t].lfor * 11.9f, d ? amt : 0);
}
static void apply_track(int t) {
    apply_engine(t, tr[t].p);
    apply_bus(t);
    apply_lfo(t);
    instrument_pan(SLOT(t), tr[t].pan * 2 - 1);
    instrument_pan(AUXS(t), tr[t].pan * 2 - 1);
}

// ── sequencer ─────────────────────────────────────────────────────────────
static Pattern *curpat(int t) { return &tr[t].bank[tr[t].pat]; }

static bool cond_ok(int t, const Step *st) {
    const Cond *c = &CONDS[st->cond < NCOND ? st->cond : 0];
    if (c->den) return (tloops[t] % c->den) == c->num - 1;
    if (c->pct == -1) return fxon[LFX_FILL];
    if (c->pct == -2) return !fxon[LFX_FILL];
    if (c->pct >= 100) return true;
    return rnd(100) < c->pct;
}

static int resolve_note(int t, const Step *st) {
    int oc = ((int)tr[t].oct - 2) * 12;
    if (t == T_CHORD) return st->note + oc;                        // root; quality in nmode
    switch (st->nmode) {
        case NM_CH1: case NM_CH2: case NM_CH3: case NM_CH4:
            return cur_chord[st->nmode - NM_CH1] + oc;
        case NM_ARP: return cur_chord[(arpi[t]++) & 3] + oc;
        default:     return st->note + oc;
    }
}

static void fire_one(int t, int midi, int vol, int durms, const float *p) {
    const Engine *e = &MENU[t].e[tr[t].engine];
    if (e->fire) e->fire(t, midi, vol, durms, p);
    else         hit(midi, SLOT(t), vol, durms);
}

static void trig(int t, Step *st, int stepms, bool audition) {
    if (!audition && (mute[t] || !cond_ok(t, st))) return;
    const Engine *e = &MENU[t].e[tr[t].engine];

    float eff[NPP]; bool differ = applied_eng[t] != tr[t].engine;
    for (int i = 0; i < NPP; i++) {
        eff[i] = st->lock[i] >= 0 ? st->lock[i] / 127.0f : tr[t].p[i];
        if (eff[i] != applied[t][i]) differ = true;
    }
    if (differ) apply_engine(t, eff);

    int vol = 1 + (int)(st->vel / 127.0f * tr[t].vol * 6);
    if (vol > 7) vol = 7;
    int durms = st->dur ? st->dur * stepms : e->defdur;

    if (t == T_CHORD) {                                            // stack the chord tones
        int q = st->nmode < NQ ? st->nmode : Q_MIN;
        int root = resolve_note(t, st);
        for (int i = 0; i < 4; i++) {
            int m = root + QIV[q][i];
            if (i == 0) fire_one(t, m, vol, durms, eff);
            else        schedule_hit(i * 6, m, SLOT(t), vol, durms);   // tiny strum
            cur_chord[i] = m;
        }
    } else {
        int midi = resolve_note(t, st);
        fire_one(t, midi, vol, durms, eff);
        int reps = fxon[LFX_ROLL] && st->rep == 0 ? 1 : st->rep;    // ROLL = ratchet-all
        if (reps) {                                                // ratchet sub-hits
            int n = reps + 1, sp = stepms / n;
            for (int k = 1; k < n; k++)
                schedule_hit(k * sp, midi, SLOT(t), vol > 1 ? vol - 1 : 1, durms < sp ? durms : sp);
        }
    }
    flash[t] = frame();
}

// pattern linking: after the last pattern of a linked run, rewind to the run's start
static int link_next(int t) {
    int p = tr[t].pat;
    if (tr[t].bank[p].link) return (p + 1) % NPAT;
    while (p > 0 && tr[t].bank[p - 1].link) p--;                   // walk back to run start
    return p;
}

static void song_apply(void) {
    for (int t = 0; t < NTRACK; t++) { tr[t].pat = parts[songpos].pat[t]; ttick[t] = 0; tloops[t] = 0; }
}
static void song_advance(void) {
    if (++songbar < parts[songpos].reps) return;
    songbar = 0;
    for (int k = 1; k <= NPART; k++) {
        int q = (songpos + k) % NPART;
        if (parts[q].reps) { songpos = q; break; }
    }
    song_apply();
}

// ── defaults: waves + the demo groove ─────────────────────────────────────
static void default_waves(void) {
    for (int i = 0; i < 64; i++) {
        float ph = i / 64.0f;
        waves[0][i] = (signed char)(sinf(ph * 6.2831853f) * 90);                       // sine
        waves[1][i] = (signed char)((ph < 0.5f ? ph * 2 : ph * 2 - 2) * 85);       // saw
        waves[2][i] = (signed char)((ph < 0.5f ? 1 : -1) * (75 - 40 * ph));        // decaying square
        waves[3][i] = (signed char)((sinf(ph * 6.2831853f) * 0.6f + sinf(ph * 18.849556f) * 0.4f) * 85); // organ-ish
    }
}
static void push_waves(void) {
    float w[64];
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < 64; i++) w[i] = waves[k][i] / 100.0f;
        wave_set(k, w, 64);
    }
}

static void set_step(Step *st, int t, int on) {
    if (on && st->vel == 0) {                                      // first ever enable → defaults
        st->nmode = DEFNM[t]; st->note = DEFNOTE[t]; st->vel = 100;
        st->dur = 0; st->rep = 0; st->rate = 0; st->cond = 0;
        for (int i = 0; i < NPP; i++) st->lock[i] = -1;
    }
    st->on = (unsigned char)on;
}

static void demo_groove(void) {
    static const char *G[NTRACK] = {
        "x...x...x...x...",   // kick
        "....x.......x..x",   // snare
        "x.x.x.x.x.x.x.x.",   // hat — p-locked open on the off-beats below
        "x..x..x...x..x..",   // bass CHD1
        "..x.....x.....x.",   // lead
        "x.......x.......",   // chords: Cm, then Ab
        "................",   // wave1
        "................",   // wave2
    };
    for (int t = 0; t < NTRACK; t++) {
        Pattern *pp = &tr[t].bank[0];
        for (int c = 0; c < NSTEP; c++)
            if (G[t][c] == 'x') set_step(&pp->st[c], t, 1);
    }
    Pattern *hat = &tr[T_HAT].bank[0];
    for (int c = 2; c < NSTEP; c += 4) hat->st[c].lock[1] = 70;    // DEC p-lock = open hat
    Pattern *ch = &tr[T_CHORD].bank[0];
    ch->st[0].note = 48; ch->st[0].nmode = Q_MIN;                  // Cm
    ch->st[8].note = 44; ch->st[8].nmode = Q_MAJ;                  // Ab
    Pattern *ld = &tr[T_LEAD].bank[0];
    ld->st[2].nmode = NM_CH3; ld->st[8].nmode = NM_CH2; ld->st[14].nmode = NM_CH4;
    ld->st[14].cond = 2;                                           // 50%
    tr[T_LEAD].bus = BUS_VERB;
    tr[T_CHORD].bus = BUS_VERB;
}

static void reset_tracks(void) {
    for (int t = 0; t < NTRACK; t++) {
        Track *k = &tr[t];
        k->engine = 0; k->bus = BUS_DRY; k->oct = 2; k->shuf = 0; k->lfod = 0;
        k->vol = 0.7f; k->pan = 0.5f; k->lfor = 0.3f; k->lfoa = 0.4f;
        k->pat = 0;
        for (int i = 0; i < NPP; i++) k->p[i] = MENU[t].e[0].def[i];
        for (int b = 0; b < NPAT; b++) { k->bank[b].len = NSTEP; k->bank[b].link = 0; }
    }
}

// ── save / load ───────────────────────────────────────────────────────────
static void save_all(void) {
    static Blob b;
    b.magic = MAGIC; b.tempo = tempo; b.songmode = songmode;
    for (int t = 0; t < NTRACK; t++) b.tr[t] = tr[t];
    for (int i = 0; i < NPART; i++) b.parts[i] = parts[i];
    for (int k = 0; k < 4; k++) for (int i = 0; i < 64; i++) b.waves[k][i] = waves[k][i];
    save_bytes(&b, sizeof b);
}
static bool load_all(void) {
    static Blob b;
    if (load_bytes(&b, sizeof b) != (int)sizeof b || b.magic != MAGIC) return false;
    tempo = b.tempo; songmode = b.songmode;
    for (int t = 0; t < NTRACK; t++) tr[t] = b.tr[t];
    for (int i = 0; i < NPART; i++) parts[i] = b.parts[i];
    for (int k = 0; k < 4; k++) for (int i = 0; i < 64; i++) waves[k][i] = b.waves[k][i];
    return true;
}

void init() {
    reset_tracks();
    default_waves();
    if (!load_all()) demo_groove();
    push_waves();
    for (int t = 0; t < NTRACK; t++) { apply_track(t); applied_eng[t] = tr[t].engine; }
}

// ── page tables ───────────────────────────────────────────────────────────
enum { PG_ENGINE, PG_P1, PG_P2, PG_P3, PG_P4, PG_VOL, PG_PAN, PG_BUS, PG_OCT,
       PG_SHUF, PG_LFOD, PG_LFOR, PG_LFOA, PG_DRAW, NTPAGE };
static int npages_track(int t) { return (t >= T_WAVE1) ? NTPAGE : NTPAGE - 1; }

enum { SP_NOTE, SP_VEL, SP_DUR, SP_REP, SP_RATE, SP_COND, SP_LK1, SP_LK2, SP_LK3, SP_LK4, NSPAGE };
static const char *SPNAME[NSPAGE] = { "NOTE", "VEL", "DUR", "REPEAT", "RATE", "COND", "LOCK 1", "LOCK 2", "LOCK 3", "LOCK 4" };

enum { PP_LEN, PP_LINK, NPPAGE };
enum { GP_PART, GP_REPS, GP_CHAIN, NGPAGE };

static const char *BUSNAME[NBUS] = { "DRY", "DRIVE", "REVERB", "CRUSH" };
static const char *LFODNAME[6]  = { "OFF", "PITCH", "VOL", "CUTOFF", "DUTY", "PAN" };

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

// adjust the current TRACK-mode page: abs01 >= 0 sets absolutely (the strip),
// else delta = arrow steps
static void tpage_adjust(int t, float abs01, int delta) {
    Track *k = &tr[t];
    int pg = tpage[t];
    switch (pg) {
        case PG_ENGINE: {
            int n = MENU[t].n;
            int e = abs01 >= 0 ? (int)(abs01 * (n - 0.01f)) : (k->engine + delta + n) % n;
            if (e != k->engine) {
                k->engine = (unsigned char)e;
                for (int i = 0; i < NPP; i++) k->p[i] = MENU[t].e[e].def[i];
                apply_track(t);
            }
            break;
        }
        case PG_P1: case PG_P2: case PG_P3: case PG_P4: {
            float *v = &k->p[pg - PG_P1];
            *v = clamp01(abs01 >= 0 ? abs01 : *v + delta * 0.05f);
            apply_engine(t, k->p);
            break;
        }
        case PG_VOL: k->vol = clamp01(abs01 >= 0 ? abs01 : k->vol + delta * 0.05f); break;
        case PG_PAN:
            k->pan = clamp01(abs01 >= 0 ? abs01 : k->pan + delta * 0.05f);
            instrument_pan(SLOT(t), k->pan * 2 - 1); instrument_pan(AUXS(t), k->pan * 2 - 1);
            break;
        case PG_BUS:
            k->bus = (unsigned char)(abs01 >= 0 ? (int)(abs01 * (NBUS - 0.01f)) : (k->bus + delta + NBUS) % NBUS);
            apply_bus(t);
            break;
        case PG_OCT: {
            int o = abs01 >= 0 ? (int)(abs01 * 4.99f) : (int)k->oct + delta;
            k->oct = (unsigned char)(o < 0 ? 0 : o > 4 ? 4 : o);
            break;
        }
        case PG_SHUF: {
            int s = abs01 >= 0 ? (int)(abs01 * 60) : k->shuf + delta * 5;
            k->shuf = (unsigned char)(s < 0 ? 0 : s > 60 ? 60 : s);
            break;
        }
        case PG_LFOD:
            k->lfod = (unsigned char)(abs01 >= 0 ? (int)(abs01 * 5.99f) : (k->lfod + delta + 6) % 6);
            apply_lfo(t); break;
        case PG_LFOR: k->lfor = clamp01(abs01 >= 0 ? abs01 : k->lfor + delta * 0.05f); apply_lfo(t); break;
        case PG_LFOA: k->lfoa = clamp01(abs01 >= 0 ? abs01 : k->lfoa + delta * 0.05f); apply_lfo(t); break;
    }
    mark_dirty();
}

// adjust the current STEP-mode page on step `si` of the selected track
static void spage_adjust(int si, float abs01, int delta) {
    Step *st = &curpat(seltrack)->st[si];
    if (!st->on) return;
    switch (spage) {
        case SP_NOTE:
            if (seltrack == T_CHORD) {
                int n = abs01 >= 0 ? 36 + (int)(abs01 * 24) : st->note + delta;
                st->note = (unsigned char)(n < 24 ? 24 : n > 72 ? 72 : n);
            } else {
                int n = abs01 >= 0 ? 36 + (int)(abs01 * 36) : st->note + delta;
                st->note = (unsigned char)(n < 12 ? 12 : n > 108 ? 108 : n);
            }
            break;
        case SP_VEL: {
            int v = abs01 >= 0 ? 1 + (int)(abs01 * 126) : st->vel + delta * 8;
            st->vel = (unsigned char)(v < 1 ? 1 : v > 127 ? 127 : v);
            break;
        }
        case SP_DUR: {
            int d = abs01 >= 0 ? (int)(abs01 * 16.99f) : st->dur + delta;
            st->dur = (unsigned char)(d < 0 ? 0 : d > 16 ? 16 : d);
            break;
        }
        case SP_REP: {
            int r = abs01 >= 0 ? (int)(abs01 * 3.99f) : st->rep + delta;
            st->rep = (unsigned char)(r < 0 ? 0 : r > 3 ? 3 : r);
            break;
        }
        case SP_RATE: {
            int r = abs01 >= 0 ? (int)(abs01 * 2.99f) : st->rate + delta;
            st->rate = (unsigned char)(r < 0 ? 0 : r > 2 ? 2 : r);
            break;
        }
        case SP_COND: {
            int c = abs01 >= 0 ? (int)(abs01 * (NCOND - 0.01f)) : (st->cond + delta + NCOND) % NCOND;
            st->cond = (unsigned char)c;
            break;
        }
        case SP_LK1: case SP_LK2: case SP_LK3: case SP_LK4: {
            signed char *lk = &st->lock[spage - SP_LK1];
            if (abs01 >= 0) *lk = (signed char)(abs01 * 127);
            else {
                int base = *lk >= 0 ? *lk : (int)(tr[seltrack].p[spage - SP_LK1] * 127);
                int v = base + delta * 6;
                *lk = (signed char)(v < 0 ? 0 : v > 127 ? 127 : v);
            }
            break;
        }
    }
    mark_dirty();
}

// ── keyboard overlay (hold EDIT) — the GarageBand musical-typing map ─────
static const char KBMAP[] = "AWSEDFTGYHUJKOLP;'";

static void play_note(int midi) {
    Track *k = &tr[seltrack];
    int vol = 1 + (int)(0.8f * k->vol * 6);
    int oc = ((int)k->oct - 2) * 12;
    if (seltrack == T_CHORD) {
        Step tmp = { 1, Q_MIN, (unsigned char)midi, 100, 0, 0, 0, 0, { -1, -1, -1, -1 } };
        trig(T_CHORD, &tmp, 120, true);
        return;
    }
    fire_one(seltrack, midi + oc, vol, MENU[seltrack].e[tr[seltrack].engine].defdur, tr[seltrack].p);
    flash[seltrack] = frame();
    // write to the held/selected step in STEP mode
    if (mode == M_STEP) {
        int si = held_stepkey >= 0 ? held_stepkey : selstep;
        Step *st = &curpat(seltrack)->st[si];
        if (st->on) { st->nmode = NM_FIX; st->note = (unsigned char)midi; mark_dirty(); }
    }
}

// ── input plumbing ────────────────────────────────────────────────────────
static int  stepkey_rect(int i, int *x, int *y) {                  // i = 0..15
    *x = SKX + (i % 8) * SKS; *y = i < 8 ? SKY1 : SKY2; return 1;
}
static bool key_tap(int i)  { int x, y; stepkey_rect(i, &x, &y); return tapp(x, y, SKW, SKH); }
static bool key_held(int i) { int x, y; stepkey_rect(i, &x, &y); return tap(x, y, SKW, SKH); }

static const char SKCHARS[17] = "12345678QWERTYUI";
static bool skeyp(int i) { return keyp(SKCHARS[i]); }
static bool skey(int i)  { return key(SKCHARS[i]); }

// on-screen key rects (shared by update hit-tests + draw)
#define R_PLAY  4, TRY, 64, TRH
#define R_EDIT  72, TRY, 56, TRH
#define R_FX    132, TRY, 56, TRH
#define R_UP    252, 6, 28, 18
#define R_LEFT  220, 27, 28, 18
#define R_DOWN  252, 27, 28, 18
#define R_RIGHT 284, 27, 28, 18
#define R_B     224, 50, 40, 18
#define R_A     272, 50, 40, 18

static bool arrow_l, arrow_r, arrow_u, arrow_d, but_a, but_b;      // edge-triggered, both inputs

static void toggle_fx(int i) {
    fxon[i] = !fxon[i];
    switch (i) {
        case LFX_LP: if (fxon[i]) { fxon[LFX_HP] = false; } break;
        case LFX_HP: if (fxon[i]) { fxon[LFX_LP] = false; } break;
        case LFX_DIVE:  varispeed(fxon[i] ? 0.5f : 1.0f); break;
        case LFX_CRUSH: crush(6, 6, fxon[i] ? 0.55f : 0); break;
        case LFX_TREM:  tremolo(5.5f, fxon[i] ? 0.5f : 0, LFO_SHAPE_SINE); break;
        default: break;
    }
    if (!fxon[LFX_LP] && !fxon[LFX_HP]) filter(FILTER_OFF, 0, 0);
}

static void stepkey_pressed(int i) {
    // overlays first: they own the keys while held
    if (edit_held) { play_note(48 + kb_oct * 12 + i); return; }
    if (play_held) { if (i < NTRACK) mute[i] = !mute[i]; play_combo = true; return; }
    if (fx_held && mode != M_PATT) { if (i < NFX) toggle_fx(i); return; }

    switch (mode) {
        case M_TRACK:
            if (i < NTRACK) {
                seltrack = i;
                Step tmp = { 1, DEFNM[i], DEFNOTE[i], 90, 0, 0, 0, 0, { -1, -1, -1, -1 } };
                trig(i, &tmp, 120, true);                           // audition on select
            }
            break;
        case M_STEP: {
            if (i >= curpat(seltrack)->len) break;
            Step *st = &curpat(seltrack)->st[i];
            set_step(st, seltrack, !st->on);
            selstep = i;
            if (st->on) trig(seltrack, st, 120, true);
            mark_dirty();
            break;
        }
        case M_PATT:
            if (fx_held) {                                          // the copy idiom
                if (copy_src < 0) copy_src = i;
                else {
                    tr[seltrack].bank[i] = tr[seltrack].bank[copy_src];
                    copy_src = -1; mark_dirty();
                }
            } else { tr[seltrack].pat = (unsigned char)i; mark_dirty(); }   // tick keeps running: seamless switch
            break;
        case M_SONG:
            if (i < NPART) selpart = i;
            break;
    }
}

void update() {
    bpm(tempo);

    // ── hold-keys state (touch rect OR keyboard) ──
    prev_play_held = play_held;
    play_held = tap(R_PLAY) || key(KEY_SPACE);
    edit_held = tap(R_EDIT) || key('N');
    fx_held   = tap(R_FX)   || key('M');
    if (!fx_held) copy_src = -1;

    // edge-triggered arrows + A/B from both inputs
    arrow_l = tapp(R_LEFT)  || keyp(KEY_LEFT);
    arrow_r = tapp(R_RIGHT) || keyp(KEY_RIGHT);
    arrow_u = tapp(R_UP)    || keyp(KEY_UP);
    arrow_d = tapp(R_DOWN)  || keyp(KEY_DOWN);
    but_a   = tapp(R_A)     || keyp(KEY_ENTER);
    but_b   = tapp(R_B)     || keyp(KEY_BACKSPACE);

    // ── PLAY: toggle on release when no combo was used; hold+arrows = BPM ──
    if (play_held) {
        if (arrow_l) { tempo = tempo > 60 ? tempo - 2 : 60; play_combo = true; arrow_l = false; mark_dirty(); }
        if (arrow_r) { tempo = tempo < 220 ? tempo + 2 : 220; play_combo = true; arrow_r = false; mark_dirty(); }
    }
    if (prev_play_held && !play_held) {
        if (!play_combo) {
            running = !running;
            if (running) { master16 = -1; for (int t = 0; t < NTRACK; t++) { ttick[t] = 0; tloops[t] = 0; tlast16[t] = -9; } }
        }
        play_combo = false;
    }

    // ── mode keys ──
    struct { int x, y; } mk;
    for (int m = 0; m < NMODE; m++) {
        mk.x = MKX; mk.y = 4 + m * MKS;
        bool kb = !edit_held && keyp("ZXCV"[m]);                    // Z/X are octave while EDIT held
        if (tapp(mk.x, mk.y, MKW, MKH) || kb) mode = m;
    }
    if (edit_held) {                                                // keyboard octave
        if ((keyp('Z') || arrow_d) && kb_oct > -2) { kb_oct--; arrow_d = false; }
        if ((keyp('X') || arrow_u) && kb_oct <  2) { kb_oct++; arrow_u = false; }
        for (int i = 0; KBMAP[i]; i++)
            if (keyp(KBMAP[i])) play_note(60 + kb_oct * 12 + i);
    }

    // ── step keys: edge = action, held = p-lock grammar ──
    held_stepkey = -1;
    for (int i = 0; i < 16; i++) {
        if (key_tap(i) || (!edit_held && skeyp(i))) stepkey_pressed(i);
        if (mode == M_STEP && (key_held(i) || (!edit_held && skey(i))) && curpat(seltrack)->st[i].on)
            held_stepkey = i;
    }
    if (held_stepkey >= 0) selstep = held_stepkey;

    // ── arrows: pages (←/→) + values (↑/↓) per mode ──
    int npg = mode == M_TRACK ? npages_track(seltrack)
            : mode == M_STEP  ? NSPAGE
            : mode == M_PATT  ? NPPAGE : NGPAGE;
    int *pg = mode == M_TRACK ? &tpage[seltrack]
            : mode == M_STEP  ? &spage
            : mode == M_PATT  ? &ppage : &gpage;
    if (arrow_l) *pg = (*pg + npg - 1) % npg;
    if (arrow_r) *pg = (*pg + 1) % npg;
    int dv = (arrow_u ? 1 : 0) - (arrow_d ? 1 : 0);
    if (dv) {
        if (mode == M_TRACK) tpage_adjust(seltrack, -1, dv);
        else if (mode == M_STEP) spage_adjust(selstep, -1, dv);
        else if (mode == M_PATT) {
            Pattern *pp = curpat(seltrack);
            if (ppage == PP_LEN) {
                int L = pp->len + dv; pp->len = (unsigned char)(L < 1 ? 1 : L > 16 ? 16 : L);
            } else pp->link = !pp->link;
            mark_dirty();
        } else if (mode == M_SONG && gpage == GP_REPS) {
            int r = parts[selpart].reps + dv;
            parts[selpart].reps = (unsigned char)(r < 0 ? 0 : r > 16 ? 16 : r);
            mark_dirty();
        }
    }

    // ── A / B per mode ──
    if (but_a) {
        if (mode == M_STEP && spage == SP_NOTE && seltrack == T_CHORD) {
            Step *st = &curpat(seltrack)->st[selstep];
            if (st->on) { st->nmode = (unsigned char)((st->nmode + 1) % NQ); mark_dirty(); }
        } else if (mode == M_STEP && spage == SP_NOTE) {
            Step *st = &curpat(seltrack)->st[selstep];
            if (st->on) { st->nmode = (unsigned char)((st->nmode + 1) % NNM); mark_dirty(); }
        } else if (mode == M_PATT && ppage == PP_LINK) {
            curpat(seltrack)->link = !curpat(seltrack)->link; mark_dirty();
        } else if (mode == M_SONG) {
            if (gpage == GP_CHAIN) { songmode = !songmode; songpos = -1; songbar = 0;
                if (songmode) { for (int q = 0; q < NPART; q++) if (parts[q].reps) { songpos = q; break; }
                    if (songpos < 0) songmode = false; else song_apply(); } }
            else { for (int t = 0; t < NTRACK; t++) parts[selpart].pat[t] = tr[t].pat;
                   if (!parts[selpart].reps) parts[selpart].reps = 1; mark_dirty(); }
        }
    }
    if (but_b) {
        if (mode == M_STEP && spage >= SP_LK1) {                    // B clears a p-lock
            Step *st = &curpat(seltrack)->st[selstep];
            st->lock[spage - SP_LK1] = -1; mark_dirty();
        } else if (mode == M_SONG) { parts[selpart].reps = 0; mark_dirty(); }
    }

    // ── the touch strip ──
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (ty < STY || ty >= STY + STH || tx < 2 || tx > 318) continue;
        float v = clamp01((tx - 4) / 312.0f);
        if (fx_held) {
            fxcut = v;                                              // ride the live filter
        } else if (mode == M_TRACK) {
            if (tpage[seltrack] != PG_DRAW) tpage_adjust(seltrack, v, 0);
        } else if (mode == M_STEP) {
            spage_adjust(held_stepkey >= 0 ? held_stepkey : selstep, v, 0);
        } else if (mode == M_PATT && ppage == PP_LEN) {
            curpat(seltrack)->len = (unsigned char)(1 + (int)(v * 15.99f)); mark_dirty();
        } else if (mode == M_SONG && gpage == GP_REPS) {
            parts[selpart].reps = (unsigned char)(v * 16.99f); mark_dirty();
        }
    }

    // ── wave drawing: finger on the OLED while the DRAW page shows ──
    if (mode == M_TRACK && tpage[seltrack] == PG_DRAW && seltrack >= T_WAVE1) {
        int wsel = tr[seltrack].engine;                             // engine = which USER wave
        for (int i = 0; i < touch_count(); i++) {
            int tx = touch_x(i) - OX, ty = touch_y(i) - OY;
            if (tx < 0 || tx >= OW || ty < 12 || ty >= OH) continue;
            int idx = tx / 2; if (idx > 63) idx = 63;
            float v = 1.0f - (ty - 12) / (float)(OH - 12 - 1) * 2;  // +1..-1
            waves[wsel][idx] = (signed char)(v * 100);
            push_waves();
            mark_dirty();
        }
    }

    // ── the live filter (LP/HP) is ride-safe: apply every frame while on ──
    if (fxon[LFX_LP])      filter(FILTER_LOW, 80 + fxcut * fxcut * 12000, 0.55f);
    else if (fxon[LFX_HP]) filter(FILTER_HIGH, 80 + fxcut * fxcut * 8000, 0.55f);

    // ── the sequencer clock ──
    if (running) {
        float pos16 = beat() * 4 + beat_pos() * 4.0f;
        int   n16   = (int)pos16;
        float frac  = pos16 - n16;
        int   stepms = (int)(60000.0f / tempo / 4);

        if (n16 != master16) {                                     // master bar / song clock
            int prev = master16;
            master16 = n16;
            if (songmode && prev >= 0 && n16 / 16 != prev / 16) song_advance();
        }
        for (int t = 0; t < NTRACK; t++) {
            // per-track shuffle, drummachine-style: an odd 16th only counts once
            // the swung onset clears (shuf% of a step late); on-beats stay straight
            int sw = (n16 % 2 == 1 && frac < tr[t].shuf / 100.0f) ? n16 - 1 : n16;
            if (sw == tlast16[t]) continue;
            tlast16[t] = sw;

            Pattern *pp = curpat(t);
            int tick = fxon[LFX_FREEZE] ? ttick[t] : ttick[t]++;   // FREEZE = beat-repeat
            int stp = tick % pp->len;
            tstep[t] = stp;
            Step *st = &pp->st[stp];
            if (st->on) trig(t, st, stepms, false);
            if (!fxon[LFX_FREEZE] && (tick + 1) % pp->len == 0) {  // pattern wrapped
                tloops[t]++;
                int nx = link_next(t);
                if (nx != tr[t].pat) { tr[t].pat = (unsigned char)nx; ttick[t] = 0; }
            }
        }
    }

    // ── autosave ──
    if (save_cooldown > 0 && --save_cooldown == 0) save_all();

#ifdef DE_TRACE
    watch("mode", "%s", MODENAME[mode]);
    watch("trk", "%s", TNAME[seltrack]);
    watch("pat", "%d", tr[seltrack].pat);
    watch("run", "%d", running ? 1 : 0);
#endif
}

// ── OLED rendering ────────────────────────────────────────────────────────
#define OINK  CLR_WHITE
#define ODIM  CLR_MEDIUM_GREY
#define OBG   CLR_BLACK

static void oled_header(const char *left, const char *right) {
    rectfill(OX, OY, OW, 9, OINK);
    font(FONT_SMALL);
    print(left, OX + 2, OY + 2, OBG);
    print_right(right, OX + OW - 2, OY + 2, OBG);
    font(FONT_NORMAL);
}
static void oled_dots(int n, int cur) {
    int w = n * 6, x0 = OX + (OW - w) / 2;
    for (int i = 0; i < n; i++) {
        if (i == cur) rectfill(x0 + i * 6 + 1, OY + 11, 4, 4, OINK);
        else          rectfill(x0 + i * 6 + 2, OY + 12, 2, 2, ODIM);
    }
}
static void oled_value(const char *label, const char *val, float bar) {
    font(FONT_SMALL);
    print(label, OX + 4, OY + 22, ODIM);
    font(FONT_NORMAL);
    print(val, OX + 4, OY + 32, OINK);
    if (bar >= 0) {
        rect(OX + 4, OY + 48, OW - 8, 8, ODIM);
        rectfill(OX + 5, OY + 49, (int)((OW - 10) * bar), 6, OINK);
    }
}

static void oled_track(void) {
    Track *k = &tr[seltrack];
    const Engine *e = &MENU[seltrack].e[k->engine];
    oled_header(TNAME[seltrack], str("P%02d %s", k->pat + 1, running ? ">" : "II"));
    oled_dots(npages_track(seltrack), tpage[seltrack]);
    switch (tpage[seltrack]) {
        case PG_ENGINE: oled_value("ENGINE", e->name, (k->engine + 0.5f) / MENU[seltrack].n); break;
        case PG_P1: case PG_P2: case PG_P3: case PG_P4: {
            int i = tpage[seltrack] - PG_P1;
            oled_value(e->pn[i], str("%d", (int)(k->p[i] * 127)), k->p[i]);
            break;
        }
        case PG_VOL: oled_value("VOLUME", str("%d", (int)(k->vol * 127)), k->vol); break;
        case PG_PAN: oled_value("PAN", k->pan < 0.48f ? str("L%d", (int)((0.5f - k->pan) * 200))
                                     : k->pan > 0.52f ? str("R%d", (int)((k->pan - 0.5f) * 200)) : "CENTER", k->pan); break;
        case PG_BUS: oled_value("FX BUS", BUSNAME[k->bus], (k->bus + 0.5f) / NBUS); break;
        case PG_OCT: oled_value("OCTAVE", str("%+d", (int)k->oct - 2), k->oct / 4.0f); break;
        case PG_SHUF: oled_value("SHUFFLE", str("%d%%", k->shuf), k->shuf / 60.0f); break;
        case PG_LFOD: oled_value("LFO DEST", LFODNAME[k->lfod], (k->lfod + 0.5f) / 6); break;
        case PG_LFOR: oled_value("LFO RATE", str("%.1fHZ", 0.1f + k->lfor * 11.9f), k->lfor); break;
        case PG_LFOA: oled_value("LFO AMT", str("%d", (int)(k->lfoa * 127)), k->lfoa); break;
        case PG_DRAW: {                                            // the wave-draw page
            font(FONT_SMALL);
            print(str("DRAW %s WITH A FINGER", e->name), OX + 4, OY + 12, ODIM);
            font(FONT_NORMAL);
            int wsel = k->engine, cy = OY + 12 + (OH - 12) / 2;
            for (int i = 0; i < 64; i++) {
                int v = waves[wsel][i] * (OH - 16) / 220;
                rectfill(OX + i * 2, cy - (v > 0 ? v : 0), 2, (v > 0 ? v : -v) + 1, OINK);
            }
            break;
        }
    }
}

static void oled_step(void) {
    Step *st = &curpat(seltrack)->st[selstep];
    oled_header(str("%s ST%02d", TNAME[seltrack], selstep + 1),
                str("P%02d %s", tr[seltrack].pat + 1, running ? ">" : "II"));
    oled_dots(NSPAGE, spage);
    if (!st->on) { oled_value(SPNAME[spage], "(step off)", -1); return; }
    switch (spage) {
        case SP_NOTE:
            if (seltrack == T_CHORD)
                oled_value("CHORD  (A=quality)", str("%s%d %s", NOTENAME[st->note % 12], st->note / 12 - 1, QNAME[st->nmode % NQ]), -1);
            else if (st->nmode == NM_FIX)
                oled_value("NOTE  (A=mode)", str("%s%d", NOTENAME[st->note % 12], st->note / 12 - 1), -1);
            else
                oled_value("NOTE  (A=mode)", st->nmode == NM_ARP ? "ARP" : str("CHORD %d", st->nmode), -1);
            break;
        case SP_VEL: oled_value("VELOCITY", str("%d", st->vel), st->vel / 127.0f); break;
        case SP_DUR: oled_value("DURATION", st->dur ? str("%d/16", st->dur) : "AUTO", st->dur / 16.0f); break;
        case SP_REP: oled_value("REPEATS", st->rep ? str("x%d", st->rep + 1) : "OFF", st->rep / 3.0f); break;
        case SP_RATE: oled_value("REP RATE", str("1/%d", 32 << st->rate), st->rate / 2.0f); break;
        case SP_COND: oled_value("CONDITION", CONDS[st->cond % NCOND].name, (st->cond + 0.5f) / NCOND); break;
        default: {
            int i = spage - SP_LK1;
            const Engine *e = &MENU[seltrack].e[tr[seltrack].engine];
            if (st->lock[i] >= 0)
                oled_value(str("LOCK %s (B=clear)", e->pn[i]), str("%d", st->lock[i]), st->lock[i] / 127.0f);
            else
                oled_value(str("LOCK %s", e->pn[i]), "--", tr[seltrack].p[i]);
            break;
        }
    }
}

static void oled_patt(void) {
    Pattern *pp = curpat(seltrack);
    oled_header(str("%s PATTERNS", TNAME[seltrack]), str("P%02d", tr[seltrack].pat + 1));
    oled_dots(NPPAGE, ppage);
    if (ppage == PP_LEN) oled_value("LENGTH", str("%d STEPS", pp->len), pp->len / 16.0f);
    else oled_value("LINK NEXT (A)", pp->link ? "ON -> chains" : "OFF", -1);
    if (copy_src >= 0) { font(FONT_SMALL); print(str("COPY P%02d -> tap dest", copy_src + 1), OX + 4, OY + 56, OINK); font(FONT_NORMAL); }
}

static void oled_song(void) {
    oled_header("SONG", songmode ? str("PT%d>", songpos + 1) : "off");
    oled_dots(NGPAGE, gpage);
    if (gpage == GP_PART) {
        font(FONT_SMALL);
        print(str("PART %d  %s", selpart + 1, parts[selpart].reps ? str("x%d bars", parts[selpart].reps) : "(empty)"),
              OX + 4, OY + 20, OINK);
        print("A=capture pats B=clear", OX + 4, OY + 30, ODIM);
        if (parts[selpart].reps) {
            for (int t = 0; t < NTRACK; t++)
                print(str("%d", parts[selpart].pat[t] + 1), OX + 4 + t * 15, OY + 42, ODIM);
        }
        font(FONT_NORMAL);
    } else if (gpage == GP_REPS) {
        oled_value("PART BARS", str("%d", parts[selpart].reps), parts[selpart].reps / 16.0f);
    } else {
        oled_value("CHAIN (A)", songmode ? "PLAYING PARTS" : "OFF", -1);
    }
}

// ── faceplate drawing ─────────────────────────────────────────────────────
static void draw_key(int x, int y, int w, int h, const char *label, bool held, int lit) {
    rectfill(x, y, w, h, held ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    rect(x, y, w, h, lit >= 0 ? lit : CLR_MEDIUM_GREY);
    font(FONT_SMALL);
    int tw = text_width(label);
    print(label, x + (w - tw) / 2, y + (h - 6) / 2 + 1, held ? CLR_BLACK : CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}

static int stepkey_color(int i) {                                  // LED language per mode/overlay
    if (edit_held) {                                               // keyboard: piano coloring
        int pc = i % 12;
        bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        return black ? CLR_DARKER_GREY : CLR_LIGHT_GREY;
    }
    if (play_held) return i < NTRACK ? (mute[i] ? CLR_RED : CLR_DARK_GREEN) : -2;
    if (fx_held && mode != M_PATT) return i < NFX ? (fxon[i] ? CLR_ORANGE : CLR_DARKER_GREY) : -2;
    switch (mode) {
        case M_TRACK:
            if (i >= NTRACK) return -2;
            return i == seltrack ? TCLR[i] : ((frame() - flash[i]) < 5 ? CLR_WHITE : CLR_DARKER_GREY);
        case M_STEP: {
            Pattern *pp = curpat(seltrack);
            if (i >= pp->len) return -2;
            Step *st = &pp->st[i];
            if (running && i == tstep[seltrack]) return CLR_WHITE;
            if (st->on) {
                bool locked = st->lock[0] >= 0 || st->lock[1] >= 0 || st->lock[2] >= 0 || st->lock[3] >= 0;
                return locked ? CLR_LIGHT_YELLOW : TCLR[seltrack];
            }
            return (i % 4 == 0) ? CLR_DARKER_BLUE : CLR_DARKER_GREY;
        }
        case M_PATT: {
            bool has = false;
            for (int s = 0; s < NSTEP; s++) if (tr[seltrack].bank[i].st[s].on) { has = true; break; }
            if (i == tr[seltrack].pat) return (running && (frame() / 15) % 2) ? CLR_GREEN : CLR_BLUE;
            if (i == copy_src) return CLR_ORANGE;
            if (tr[seltrack].bank[i].link) return CLR_BLUE_GREEN;
            return has ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
        }
        default:                                                   // SONG
            if (i >= NPART) return -2;
            if (songmode && i == songpos) return CLR_GREEN;
            if (i == selpart) return CLR_BLUE;
            return parts[i].reps ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
    }
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    // ── mode keys ──
    for (int m = 0; m < NMODE; m++)
        draw_key(MKX, 4 + m * MKS, MKW, MKH, MODENAME[m], false, m == mode ? CLR_WHITE : -1);

    // ── OLED ──
    rect(OX - 2, OY - 2, OW + 4, OH + 4, CLR_DARK_GREY);
    rectfill(OX, OY, OW, OH, OBG);
    clip(OX, OY, OW, OH);
    switch (mode) {
        case M_TRACK: oled_track(); break;
        case M_STEP:  oled_step();  break;
        case M_PATT:  oled_patt();  break;
        default:      oled_song();  break;
    }
    clip(0, 0, 0, 0);

    // ── arrows + A/B ──
    draw_key(R_UP, "^", false, -1);
    draw_key(R_LEFT, "<", false, -1);
    draw_key(R_DOWN, "v", false, -1);
    draw_key(R_RIGHT, ">", false, -1);
    draw_key(R_B, "B", false, -1);
    draw_key(R_A, "A", false, -1);

    // ── transport ──
    draw_key(R_PLAY, running ? "PLAY >" : "PLAY II", play_held, running ? CLR_GREEN : -1);
    draw_key(R_EDIT, "EDIT", edit_held, edit_held ? CLR_LIGHT_YELLOW : -1);
    draw_key(R_FX, "CPY/FX", fx_held, fx_held ? CLR_ORANGE : -1);
    font(FONT_SMALL);
    print(str("%d BPM", tempo), 196, TRY + 3, CLR_LIGHT_GREY);
    print(str("%s%s", TNAME[seltrack], mute[seltrack] ? " MUTE" : ""), 196, TRY + 12, TCLR[seltrack]);
    font(FONT_NORMAL);

    // ── step keys ──
    for (int i = 0; i < 16; i++) {
        int x, y; stepkey_rect(i, &x, &y);
        int c = stepkey_color(i);
        bool held = key_held(i);
        if (c == -2) {                                             // unused in this view
            rectfill(x, y, SKW, SKH, CLR_DARKER_GREY);
            rect(x, y, SKW, SKH, CLR_DARK_GREY);
        } else {
            rectfill(x, y, SKW, SKH, c);
            rect(x, y, SKW, SKH, held ? CLR_WHITE : CLR_DARK_GREY);
        }
        // playhead sweep marker + selected-step ring in STEP mode
        if (mode == M_STEP && !edit_held && !play_held && !fx_held) {
            if (i == selstep) rect(x - 1, y - 1, SKW + 2, SKH + 2, CLR_GREEN);
        }
        font(FONT_TINY);
        const char *lb = "";
        if (edit_held) {
            int m = 48 + kb_oct * 12 + i;
            lb = str("%s%d", NOTENAME[m % 12], m / 12 - 1);
        } else if (play_held)               lb = i < NTRACK ? TNAME[i] : "";
        else if (fx_held && mode != M_PATT) lb = i < NFX ? FXNAME[i] : "";
        else switch (mode) {
            case M_TRACK: lb = i < NTRACK ? TNAME[i] : ""; break;
            case M_STEP:  lb = str("%d", i + 1); break;
            case M_PATT:  lb = str("P%d", i + 1); break;
            default:      lb = i < NPART ? str("PT%d", i + 1) : ""; break;
        }
        int lc = (c == -2 || c == CLR_DARKER_GREY || c == CLR_DARKER_BLUE) ? CLR_MEDIUM_GREY : CLR_BLACK;
        print(lb, x + 2, y + 2, lc);
        font(FONT_NORMAL);
    }

    // ── the hint line: what the 16 keys mean RIGHT NOW ──
    const char *hint;
    if (edit_held)      hint = str("KEYS PLAY %s - Z/X = OCTAVE - NOTES WRITE TO STEP", TNAME[seltrack]);
    else if (play_held) hint = "TAP KEY = MUTE TRACK - </> = BPM - RELEASE = PLAY/STOP";
    else if (fx_held)   hint = mode == M_PATT ? "COPY: TAP SOURCE PATTERN, THEN DESTINATION"
                                              : "TAP KEY = TOGGLE EFFECT - STRIP = FILTER SWEEP";
    else switch (mode) {
        case M_TRACK: hint = "KEYS = PICK A TRACK - </> PAGES - ^/v OR STRIP = VALUE"; break;
        case M_STEP:  hint = str("KEYS = %s STEPS - HOLD ONE + STRIP = TWEAK IT", TNAME[seltrack]); break;
        case M_PATT:  hint = str("KEYS = %s PATTERNS - HOLD CPY/FX + SRC + DEST = COPY", TNAME[seltrack]); break;
        default:      hint = "KEYS = SONG PARTS - A = CAPTURE PATTERNS - ^/v = BARS"; break;
    }
    font(FONT_SMALL);
    print(hint, 4, 170, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // ── the touch strip ──
    rectfill(2, STY, 316, STH, CLR_DARKER_GREY);
    rect(2, STY, 316, STH, CLR_DARK_GREY);
    for (int i = 0; i <= 8; i++) pset(4 + i * 39, STY + STH / 2, CLR_DARK_GREY);
    // context readout: what the strip is currently riding
    float sv = -1;
    const char *slab = "";
    if (fx_held) { sv = fxcut; slab = "FILTER"; }
    else if (mode == M_TRACK && tpage[seltrack] < PG_DRAW) { slab = "VALUE"; sv = -1; }
    if (sv >= 0) rectfill(4, STY + 2, (int)(312 * sv), STH - 4, CLR_ORANGE);
    font(FONT_TINY);
    print(slab, 6, STY + 6, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
