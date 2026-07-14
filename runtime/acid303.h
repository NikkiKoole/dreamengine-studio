// acid303.h — the shared TB-303 acid VOICE, extracted so the acid carts stop
// drifting. tb303.c and acidrack.c had each grown their own copy (survey: acidrack
// was the superset — Devil Fish depth: variable slide, two-decay, soft attack,
// filter tracking, sub-osc, drive mode; tb303 had the swing-aware staccato gate +
// a baked slapback). This is the UNION — the one honest acid core.
//
// A cart owns its PATTERN + UI; this owns the SOUND: a FILTER_DIODE held voice
// with a non-retriggering slide, accent (louder + a harder filter-env kick), a
// two-decay, a soft attack, filter tracking, an optional octave-down sub-osc, and
// live cutoff/reso you ride while it rings (the whole point of acid). Params are
// floats 0..1 (ui_knob's range), indexed by the ACID_* enum.
//
//   #include "studio.h"
//   #include "acid303.h"
//   static Acid a;
//   void init(void)   { acid_init(&a, 6, 34); }        // voice slot 6, sub-osc slot 34 (-1 = no sub)
//   void update(void) {
//       acid_ride(&a);                                  // each frame: cutoff/reso/drive follow the ring
//       if (step_changed) {
//           if (on[s])       acid_note(&a, BASE + pitch[s], accent[s], slide[s]);  // trigger a step
//           else if (tie[s]) acid_tie(&a, slide[s]);    // hold the held note through this step
//           else             acid_off(&a);              // rest → release the gate
//       } else               acid_gate(&a, frac, swing_onset, next_ties);          // staccato release mid-step
//   }
// Set params directly: a.p[ACID_CUT] = 0.55f;  bind a knob to &a.p[ACID_CUT].

#ifndef ACID303_H
#define ACID303_H
#include <math.h>   // powf

enum { ACID_CUT, ACID_RES, ACID_ENV, ACID_DEC, ACID_ACC, ACID_DRV, ACID_SQL,
       ACID_ADEC, ACID_SLDT, ACID_ATK, ACID_TRK, ACID_SUB, ACID_NPARAM };
// suggested labels for a UI (index-aligned)
static const char *ACID_PNAME[ACID_NPARAM] = {
    "CUT", "RES", "ENV", "DEC", "ACC", "DRV", "SQL", "ADEC", "SLDT", "ATK", "TRK", "SUB",
};

typedef struct {
    int   slot, subslot;          // voice slot; sub-osc slot (-1 = no sub voice)
    int   wave;                   // INSTR_SAW / INSTR_SQUARE
    int   drvmode;                // waveshaper: DRIVE_SOFT / HARD / FOLD / ASYM
    int   sweep;                  // accent-sweep onset mode 0=off 1=slow 2=med 3=fast
    int   base;                   // midi note the pattern's pitch 0 maps to (303 default 36)
    float cut_top;                // cutoff range: CUT knob spans this many octaves above 60Hz.
                                  // 6.38 = Devil Fish wide (~5100Hz, the default); 6.0 = vanilla (~3840Hz).
    float echo_send, rev_send;    // per-voice sends (0 = dry). echo_send = tb303's slapback (an OPTION, 0.10 stock).
    float p[ACID_NPARAM];         // params, 0..1 — index by ACID_CUT .. ACID_SUB
    int   h, hsub;                // held handles (-1 = none)
    int   prev_slide;             // did the step we just played arm a slide into the next?
    int   _lc, _lr;               // last cutoff/reso pushed to the instrument (change-guard)
} Acid;

// ── param → engine-unit mappings (acidrack's superset curves) ────────────────
static int   acid_cut_hz(Acid *a)   { return (int)(60.0f * powf(2.0f, a->p[ACID_CUT] * a->cut_top)); } // range set by cut_top (6.38 DF / 6.0 vanilla)
static int   acid_res_q(Acid *a)    { return (int)(a->p[ACID_RES] * 15.0f); }                     // 0..15 Q
static float acid_env_hz(Acid *a)   { return a->p[ACID_ENV] * 3000.0f; }                          // filter-env depth Hz
static int   acid_dec_ms(Acid *a)   { return 30 + (int)(a->p[ACID_DEC]  * 500.0f); }              // 30..530 ms
static int   acid_adec_ms(Acid *a)  { return 30 + (int)(a->p[ACID_ADEC] * 500.0f); }              // accent's own decay (two-decay)
static int   acid_slide_ms(Acid *a) { return 20 + (int)(a->p[ACID_SLDT] * 280.0f); }              // portamento 20..300 (stock ~60)
static int   acid_atk_ms(Acid *a)   { return (int)(0.5f + a->p[ACID_ATK] * 29.5f); }              // amp attack 0.5..30 ms
static float acid_acc_mul(Acid *a)  { return 1.0f + a->p[ACID_ACC] * 1.5f; }                      // accent env multiplier 1..2.5
static float acid_sq_mul(Acid *a)   { return 1.0f + a->p[ACID_SQL] * 2.0f; }                      // env-sweep depth 1..3
static int   acid_sweep_atk(Acid *a){ static const int A[4] = { 0, 10, 5, 2 }; return A[a->sweep & 3]; }

// ── voice definition (call on init + after ATK / wave / drvmode change) ──────
static void acid_define(Acid *a) {
    instrument(a->slot, a->wave, acid_atk_ms(a), 60, 6, 25);
    instrument_duty(a->slot, 0.48f);
    instrument_filter(a->slot, FILTER_DIODE, acid_cut_hz(a), acid_res_q(a));
    instrument_drive(a->slot, a->p[ACID_DRV]);
    instrument_drive_mode(a->slot, a->drvmode);
    if (a->echo_send > 0.0f) instrument_echo(a->slot, a->echo_send);
    if (a->rev_send  > 0.0f) instrument_reverb(a->slot, a->rev_send);
    if (a->subslot >= 0)     instrument(a->subslot, INSTR_TRI, acid_atk_ms(a), 60, 6, 25);
    a->_lc = acid_cut_hz(a); a->_lr = acid_res_q(a);
}

// set up an Acid with stock-303 defaults. subslot = -1 for no sub-osc.
static void acid_init(Acid *a, int slot, int subslot) {
    static const float def[ACID_NPARAM] =
        { 0.45f, 0.70f, 0.60f, 0.40f, 0.60f, 0.35f, 0.33f, 0.40f, 0.14f, 0.05f, 0.0f, 0.0f };
    a->slot = slot; a->subslot = subslot; a->wave = INSTR_SAW;
    a->drvmode = DRIVE_SOFT; a->sweep = 0; a->base = 36;
    a->cut_top = 6.38f;                            // Devil Fish wide range by default
    a->echo_send = 0.10f; a->rev_send = 0.0f;      // tb303's subtle slapback, dry reverb
    a->h = a->hsub = -1; a->prev_slide = 0;
    for (int i = 0; i < ACID_NPARAM; i++) a->p[i] = def[i];
    acid_define(a);
}

// voice it as a STOCK (vanilla, pre-Devil-Fish) TB-303: the extended params park at
// their stock values (they collapse to vanilla behaviour), the narrower cutoff range,
// and the subtle slapback echo. The depth is still all there — dial the params up for
// Devil Fish. This is "both voicings, one header, chosen by data." Call after acid_init.
static void acid_stock(Acid *a) {
    a->cut_top       = 6.0f;                        // vanilla ceiling ~3840 Hz
    a->p[ACID_SLDT]  = 0.14f;                       // ~60 ms fixed-feel slide
    a->p[ACID_ADEC]  = a->p[ACID_DEC];              // accent decays like a normal note (two-decay off)
    a->p[ACID_ATK]   = 0.05f;                       // ~2 ms attack
    a->p[ACID_TRK]   = 0.0f;                        // no filter tracking
    a->p[ACID_SUB]   = 0.0f;                        // no sub-osc
    a->sweep         = 0;                           // accent-sweep off
    a->echo_send     = 0.10f;                       // the classic slapback
    acid_define(a);
}

// each frame: ride cutoff/reso/drive live on the ringing voice + keep the
// instrument's filter baseline current (guarded so it only reconfigures on change).
static void acid_ride(Acid *a) {
    int c = acid_cut_hz(a), r = acid_res_q(a);
    if (c != a->_lc || r != a->_lr) { instrument_filter(a->slot, FILTER_DIODE, c, r); a->_lc = c; a->_lr = r; }
    if (a->h >= 0) { note_cutoff(a->h, c); note_res(a->h, (float)r); note_drive(a->h, a->p[ACID_DRV]); }
}

// release everything (rest / stop / pattern switch)
static void acid_off(Acid *a) {
    if (a->h    >= 0) { note_off(a->h);    a->h    = -1; }
    if (a->hsub >= 0) { note_off(a->hsub); a->hsub = -1; }
    a->prev_slide = 0;
}

// trigger a step. midi = absolute note; accent/slide = 0/1. A slid step glides the
// held voice (no retrigger); otherwise a fresh note with the filter-env kick.
static void acid_note(Acid *a, int midi, int accent, int slide) {
    int vol = accent ? 7 : 5;
    if (a->h >= 0 && a->prev_slide) {                       // SLIDE — hold + glide, env does NOT refire
        int g = acid_slide_ms(a);
        note_glide(a->h, g); note_pitch(a->h, (float)midi); note_vol(a->h, vol);
        if (a->hsub >= 0) { note_glide(a->hsub, g); note_pitch(a->hsub, (float)midi - 12); }
    } else {
        if (a->h    >= 0) note_off(a->h);
        if (a->hsub >= 0) { note_off(a->hsub); a->hsub = -1; }
        float e    = acid_env_hz(a) * acid_sq_mul(a) * (accent ? acid_acc_mul(a) : 1.0f);
        int   dec  = accent ? acid_adec_ms(a) : acid_dec_ms(a);       // two-decay
        int   soft = acid_atk_ms(a) - 2; if (soft < 0) soft = 0;      // soft attack rounds the filter onset too
        int   atk  = accent ? acid_sweep_atk(a) : 0;                  // accent-sweep onset
        if (soft > atk) atk = soft;
        int   cut = acid_cut_hz(a);
        if (a->p[ACID_TRK] > 0.001f) { cut += (int)(a->p[ACID_TRK] * (midi - a->base) * 170.0f); if (cut < 30) cut = 30; }
        instrument_filter(a->slot, FILTER_DIODE, cut, acid_res_q(a)); a->_lc = cut; a->_lr = acid_res_q(a);
        instrument_env(a->slot, 0, ENV_CUTOFF, atk, dec, (int)e);
        a->h = note_on(midi, a->slot, vol); note_glide(a->h, 0);
        float sl = a->p[ACID_SUB];                                    // sub-osc, an octave down
        if (a->subslot >= 0 && sl > 0.001f) {
            a->hsub = note_on(midi - 12, a->subslot, 0);
            note_vol(a->hsub, vol * sl); note_glide(a->hsub, 0);
        }
    }
    a->prev_slide = slide;
}

// a TIE step: hold the previous note through this step (no retrigger). A slide flag
// on a tie arms a glide OUT of the held note into the next.
static void acid_tie(Acid *a, int slide) { a->prev_slide = slide; }

// staccato release mid-step: cut the gate ~70% through, swing-onset aware (tb303's
// rule — the superset). `swing_onset` = 0 for a straight (unswung) step. Skip if
// the next step ties (let it ring on).
static void acid_gate(Acid *a, float frac, float swing_onset, int next_ties) {
    if (next_ties) return;
    if (a->h >= 0 && frac > swing_onset + 0.7f * (1.0f - swing_onset)) {
        note_off(a->h); a->h = -1;
        if (a->hsub >= 0) { note_off(a->hsub); a->hsub = -1; }
    }
}

#endif // ACID303_H
