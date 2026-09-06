// pmpatch.h — THE PATCH VECTOR: what the search is allowed to move, and how a
// normalized [0,1] coordinate becomes a real dreamengine call argument.
//
// Shared by BOTH sides on purpose, because they must not drift:
//   · pmcart.c (cart side) APPLIES these numbers to a slot,
//   · pm.c     (host side) PRINTS them as the C snippet you paste.
// If the mapping lived twice, the emitted snippet would eventually stop being
// the thing that was actually rendered and scored — the one bug this tool
// cannot survive, since its entire output is "these numbers sound like that".
// So: pure float math here, no studio.h, no engine calls.
//
// EVERYTHING IS 0..1. Not cosmetic: differential evolution takes differences
// between parameter vectors, so a dimension measured in Hz (40..16000) would
// dominate every step it took part in and a 0..1 mix knob would never move.
#ifndef PMPATCH_H
#define PMPATCH_H

#include <math.h>

#define PM_SLOT 5          // the instrument slot every candidate is built on

// ── the VOICE half (stage 1 + 2) ────────────────────────────────────────────
enum {
    V_HARM = 0,   // instrument_harmonics — engine macro 1
    V_TIMB,       // instrument_timbre    — engine macro 2
    V_MORPH,      // instrument_morph     — engine macro 3
    V_ATK, V_DEC, V_SUS, V_REL,        // the amp envelope: instrument()
    V_FMODE, V_CUT, V_RES,             // instrument_filter
    V_ENVAMT, V_ENVDEC,                // ENV_CUTOFF_OCT — the pluck "pew"
    V_VIBDEP, V_VIBRATE,               // LFO_PITCH — vibrato / tape-ish wobble
    V_TREMDEP,                         // LFO_VOLUME — tremolo
    V_MODE0, V_MODE1, V_MODE2, V_MODE3,// per-engine MODE_* (see pm_engine_modes)
    PM_NV
};

// ── the FX half (stage 3, voice frozen) ─────────────────────────────────────
// Per-INSTRUMENT effects throughout, so the answer stays one pasteable block
// rather than a patch plus a pile of master state. The exception is noted at
// F_RVBSIZE: the reverb tank itself is master-wide.
enum {
    F_DRIVE = 0, F_DRIVEMODE,
    F_TAPEWOW, F_TAPEFLUT, F_TAPESAT,
    F_CRUSHBITS, F_CRUSHRATE, F_CRUSHMIX,
    F_CHRATE, F_CHDEP, F_CHMIX,
    F_TREMRATE, F_TREMDEP,
    F_ECHOTIME, F_ECHOFB, F_ECHOTONE, F_ECHOSEND,
    F_RVBSIZE, F_RVBDAMP, F_RVBSEND,
    F_EQLOW, F_EQMID, F_EQHIGH,
    PM_NF
};

typedef struct {
    int   engine;        // an INSTR_* id
    int   nmode;         // how many MODE_* dials this patch OWNS (see pm_patch_default)
    float v[PM_NV];      // voice coordinates, all 0..1
    float f[PM_NF];      // fx coordinates, all 0..1
} PmPatch;

// ── voice mapping ───────────────────────────────────────────────────────────
// The curves are deliberate. Squaring the times spends most of the dial on the
// short end, which is where struck and plucked sounds live; a linear 0..3000ms
// decay would put a toy glockenspiel inside the bottom 3% of the range and the
// search would have to resolve it there.
static inline int   pm_atk_ms(float x)  { return (int)(x * x * 1500.0f); }
static inline int   pm_dec_ms(float x)  { return (int)(x * x * 3000.0f); }
static inline int   pm_sus(float x)     { return (int)(x * 7.999f); }
static inline int   pm_rel_ms(float x)  { return (int)(x * x * 4000.0f); }
static inline int   pm_cut_hz(float x)  { return (int)(40.0f * powf(400.0f, x)); }   // 40Hz..16kHz, log
static inline int   pm_res(float x)     { return (int)(x * 15.0f); }
static inline float pm_env_oct(float x) { return x * 4.0f; }                          // ENV_CUTOFF_OCT
static inline int   pm_env_ms(float x)  { return (int)(x * x * 1200.0f); }
static inline float pm_vib_semi(float x){ return x * x * 1.5f; }
static inline float pm_vib_hz(float x)  { return 0.5f + x * 8.0f; }

// The filter mode is DISCRETE inside a continuous search, so it is binned. Four
// bins, not ten: every extra mode is a cliff the optimizer has to fall off to
// discover, and these four cover the tones a keyboard sample actually has.
// Values are the FILTER_* ids (studio.h): OFF 0, LOW 1, BAND 3, LADDER 5.
static const int PM_FILTER_BIN[4] = { 0, 1, 5, 3 };
static const char *const PM_FILTER_NAME[4] = { "FILTER_OFF", "FILTER_LOW", "FILTER_LADDER", "FILTER_BAND" };
static inline int pm_bin(float x, int n) { int i = (int)(x * (float)n); return i < 0 ? 0 : (i >= n ? n - 1 : i); }

// ── fx mapping ──────────────────────────────────────────────────────────────
static const char *const PM_DRIVE_NAME[4] = { "DRIVE_SOFT", "DRIVE_HARD", "DRIVE_FOLD", "DRIVE_ASYM" };
static inline float pm_crush_bits(float x) { return 16.0f - x * 15.0f; }   // 16 clean .. 1 gnarly
static inline float pm_crush_rate(float x) { return 1.0f + x * 63.0f; }
static inline float pm_ch_rate(float x)    { return 0.1f + x * 4.9f; }
static inline float pm_trem_rate(float x)  { return 0.1f + x * 19.9f; }
static inline int   pm_echo_ms(float x)    { return 1 + (int)(x * x * 800.0f); }
static inline float pm_echo_fb(float x)    { return x * 0.9f; }            // never >1: that is runaway
static inline float pm_eq_db(float x)      { return (x - 0.5f) * 24.0f; }  // ±12 dB

// ── which MODE_* dials an engine actually answers ───────────────────────────
// Four slots because that is what the vector has; an engine with fewer simply
// leaves the tail unused (pm_engine_modes returns the count). Indices are the
// MODE_* ids from studio.h, written as literals so this header stays free of it.
static inline int pm_engine_modes(int engine, int *idx)
{
    switch (engine) {
        case 27: idx[0]=0; idx[1]=1; idx[2]=2; idx[3]=5; return 4;  // PIANO  weight/click/decay/stiff
        case 26: idx[0]=0; idx[1]=1;                     return 2;  // GUITAR weight/click
        case 19: idx[0]=0; idx[1]=1; idx[2]=6;           return 3;  // ORGAN  perc3rd/percslow/leak
        case 28: idx[0]=0; idx[1]=1; idx[2]=2;           return 3;  // BOWED  pizz/body/size
        default: return 0;
    }
}

// ── the engines the race runs over ──────────────────────────────────────────
// The four bare waves are in because a filtered saw or square IS a toy keyboard
// sound, and leaving them out would force every such target onto a physical
// model that has to work to imitate one. NOISE is out: the targets are pitched.
#define PM_NENGINES 18
static const int PM_ENGINE[PM_NENGINES] = {
    0, 1, 2, 4,                              // SQUARE SAW TRI SINE
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29
};
static const char *const PM_ENGINE_NAME[PM_NENGINES] = {
    "INSTR_SQUARE", "INSTR_SAW", "INSTR_TRI", "INSTR_SINE",
    "INSTR_PLUCK", "INSTR_MALLET", "INSTR_FM", "INSTR_ORGAN", "INSTR_EPIANO",
    "INSTR_PD", "INSTR_MEMBRANE", "INSTR_REED", "INSTR_VOICE", "INSTR_PIPE",
    "INSTR_GUITAR", "INSTR_PIANO", "INSTR_BOWED", "INSTR_BRASS"
};
static inline const char *pm_engine_name(int e) {
    for (int i = 0; i < PM_NENGINES; i++) if (PM_ENGINE[i] == e) return PM_ENGINE_NAME[i];
    return "INSTR_?";
}

// ── the SNAPPED macro axes, MEASURED (tools/patch-match/detents.c) ──────────
// studio.h says several macros are "snapped" but never says how many positions
// or where. These were measured, not guessed: a snapped axis quantizes before
// the DSP, so every value inside one detent renders BYTE-IDENTICALLY, and
// sweeping the macro and grouping identical renders recovers the boundaries
// exactly. `detents.c --check` re-measures and fails if the engine moves under
// this table, so it is a recorded measurement rather than a copied constant.
//
// It matters because differential evolution takes DIFFERENCES between parameter
// vectors: on a stepped axis it is climbing a staircase while expecting a slope.
// With the centres known, the search stops guessing and simply TRIES each one.
// Only axes with few enough detents to enumerate are listed; PLUCK morph (79),
// PIANO timbre (53) and BOWED harmonics (85) are quantized too finely to be
// worth it and behave like continuous axes.
typedef struct { int engine; int dim; int n; float centre[10]; } PmDetents;
#define PM_NDETENT 5
static const PmDetents PM_DETENT[PM_NDETENT] = {
    { 18, V_HARM, 10, { 0.050f, 0.155f, 0.255f, 0.355f, 0.455f, 0.555f, 0.655f, 0.755f, 0.855f, 0.955f } }, // FM     carrier:mod ratio
    { 19, V_HARM,  8, { 0.060f, 0.190f, 0.315f, 0.440f, 0.565f, 0.690f, 0.815f, 0.940f } },                 // ORGAN  drawbar registrations
    { 21, V_HARM,  8, { 0.060f, 0.190f, 0.315f, 0.440f, 0.565f, 0.690f, 0.815f, 0.940f } },                 // PD     wavetypes
    { 20, V_HARM,  3, { 0.165f, 0.500f, 0.835f } },                                                         // EPIANO Rhodes/Wurli/Clav
    { 27, V_HARM,  6, { 0.080f, 0.250f, 0.420f, 0.585f, 0.750f, 0.920f } },                                 // PIANO  the six voicings
};
static inline const PmDetents *pm_detents_for(int engine, int dim)
{
    for (int i = 0; i < PM_NDETENT; i++)
        if (PM_DETENT[i].engine == engine && PM_DETENT[i].dim == dim) return &PM_DETENT[i];
    return NULL;
}

// A neutral starting patch: every modulation OFF, every effect BYPASSED. Stage 1
// searches a subset of this and the frozen dimensions must be inert, or the
// engine race would be scoring a random vibrato it never chose.
// ⚠ nmode STARTS AT ZERO, and that is load-bearing rather than lazy. There is no
// value that means "leave this MODE_* alone": the dials are not all continuous.
// MODE_BOW_PIZZ is a THRESHOLD (>= 0.5 plucks the string instead of bowing it),
// so a "neutral" 0.5 sits exactly on the wrong side of it and every BOWED
// candidate in the race was a PIZZICATO -- the one thing a violin is not. Same
// shape for MODE_ORGAN_PERC_THIRD/_SLOW. Since every candidate now renders on a
// FRESH instance there is nothing to inherit, so a dial the search does not own
// is best left UNWRITTEN, at whatever the engine itself considers default.
static inline void pm_patch_default(PmPatch *p, int engine)
{
    p->engine = engine;
    p->nmode = 0;
    for (int i = 0; i < PM_NV; i++) p->v[i] = 0.5f;
    p->v[V_ATK] = 0.0f; p->v[V_DEC] = 0.5f; p->v[V_SUS] = 0.9f; p->v[V_REL] = 0.2f;
    p->v[V_FMODE] = 0.0f;                       // bin 0 = FILTER_OFF
    p->v[V_ENVAMT] = 0.0f; p->v[V_ENVDEC] = 0.3f;
    p->v[V_VIBDEP] = 0.0f; p->v[V_VIBRATE] = 0.5f; p->v[V_TREMDEP] = 0.0f;
    for (int i = 0; i < PM_NF; i++) p->f[i] = 0.0f;
    p->f[F_CRUSHBITS] = 0.0f;                   // 0 -> 16 bits, i.e. clean
    p->f[F_EQLOW] = p->f[F_EQMID] = p->f[F_EQHIGH] = 0.5f;   // 0.5 -> 0 dB, flat
}

#endif
