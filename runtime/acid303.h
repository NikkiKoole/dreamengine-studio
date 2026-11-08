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
#include "cart_ctx.h"   // per-instance state seam (docs/design/engine-context.md)

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
    int   classic;                // 0 = Devil Fish (default); 1 = vanilla pre-DF 303, read LIVE — NON-destructive
                                  // (p[] untouched, so DF knob values survive a flip; just toggle + re-acid_define).
                                  // Forces vanilla: 6.0 cutoff range · fixed 60ms slide · 2ms attack · two-decay OFF
                                  // (accent decays like a normal note) · no filter-track/sub-osc/accent-sweep. The
                                  // data-driven twin of acid_stock() — a runtime FLIP instead of a one-shot re-voice.
    float drift;                  // analog drift 0..1: slow filtered-random pitch+cutoff wander so the
                                  // voice isn't frozen-perfect (the "digital" tell). 0 = dead-flat/off.
    int   clean;                  // 0 = RAW naive saw (default — the lo-fi grit the maker likes); 1 = PolyBLEP
                                  // anti-aliased saw (clean bright lead, no fizz up high). Toggle + re-acid_define.
    float echo_send, rev_send;    // per-voice sends (0 = dry). echo_send = tb303's slapback (an OPTION, 0.10 stock).
    float p[ACID_NPARAM];         // params, 0..1 — index by ACID_CUT .. ACID_SUB
    int   h, hsub;                // held handles (-1 = none)
    int   prev_slide;             // did the step we just played arm a slide into the next?
    int   _lc, _lr;               // last cutoff/reso pushed to the instrument (change-guard)
    float _ld;                    // last drift pushed (change-guard, so DRIFT can ride live)
} Acid;

// ── THE REAL-303 BEHAVIOURS (2026-11-08) ──────────────────────────────────────
// Measured against Robin Schmidt's Open303 (MIT; its filter-env mapping is fitted to a real unit)
// with tools/ref-render/run.sh open303 + tools/carts/acidref.c, then accepted BY EAR (build/ab303):
//   1. held notes FADE — the VCA decays (tau 1230 ms) even while the gate is held, as a 303 does
//   2. accent +6.6 dB over a plain note (plain plays at 3.27, accent at 7), not +2.9 dB
//   3. an accent's filter decays in a FIXED 200 ms, whatever DECAY says (a 303 bypasses the knob)
//   4. the stock POT RANGES: cutoff 314..2394 Hz and decay 200..2000 ms, both exponential
// ON for the STOCK voicing only: acid_stock() (cut_top 6.0) or the live `classic` flip. The Devil
// Fish voicing (acid_init's default) is untouched, byte for byte. Keyed off EXISTING fields on
// purpose: acidcandy SAVES an Acid in the middle of its session state, so a new field would shift
// every field after it and old saves would restore into the wrong ones (tools/lint-saved-state.js).
static int acid_real303(Acid *a) { return a->classic || a->cut_top <= 6.0f; }

// the fade needs to know when each voice's VCA last restarted. Kept BESIDE the Acid, keyed by its
// address, for the reason above. 8 voices is more than any cart holds; a cart with more just loses
// the fade on the extra ones (they sustain, the old behaviour).
#define ACID_FADE_N 8
typedef struct { const Acid *who; int frames; float vol; } AcidFade;
#define ACID303_STATE(X) X(AcidFade, acid_fade_, [ACID_FADE_N], {{0}})
#ifndef DE_CART_CTX
DE_CTX_STATICS(ACID303_STATE)
#else
// SCRATCH, deliberately: it holds Acid ADDRESSES, which mean nothing in another process, and a
// restored rack starts with no note sounding anyway (so no fade in flight to carry over).
DE_CTX_BLOCK(acid, Acid303, ACID303_STATE)
static void acid_ctx_init_(Acid303Ctx *c) { (void)c; }
#define acid_fade_ (acid_ctx_()->acid_fade_)
#endif
static int acid_fade_slot_(const Acid *a) {
    for (int i = 0; i < ACID_FADE_N; i++) if (acid_fade_[i].who == a) return i;
    for (int i = 0; i < ACID_FADE_N; i++) if (!acid_fade_[i].who) { acid_fade_[i].who = a; return i; }
    return -1;
}

// ── param → engine-unit mappings (acidrack's superset curves) ────────────────
static int   acid_cut_hz(Acid *a)   { if (acid_real303(a)) return (int)(314.0f * de_powf(7.625f, a->p[ACID_CUT])); float top = a->cut_top; return (int)(60.0f * de_powf(2.0f, a->p[ACID_CUT] * top)); } // real 303: the stock pot, 314..2394 Hz exp; Devil Fish: 60 Hz .. cut_top octaves up
static int   acid_res_q(Acid *a)    { return (int)(a->p[ACID_RES] * 15.0f); }                     // 0..15 Q (int — instrument_filter's baseline)
static float acid_res_f(Acid *a)    { return a->p[ACID_RES] * 15.0f; }                            // 0..15 CONTINUOUS — the live note_res ride (analog res, no 16-step stepping)
static float acid_env_hz(Acid *a)   { return a->p[ACID_ENV] * 3000.0f; }                          // filter-env depth Hz
static int   acid_dec_ms(Acid *a)   { if (acid_real303(a)) return (int)(200.0f * de_powf(10.0f, a->p[ACID_DEC])); return 30 + (int)(a->p[ACID_DEC] * 500.0f); }   // real 303: 200..2000 ms exp; Devil Fish: 30..530 ms
static int   acid_adec_ms(Acid *a)  { return 30 + (int)(a->p[ACID_ADEC] * 500.0f); }              // accent's own decay (two-decay)
static int   acid_slide_ms(Acid *a) { return a->classic ? 60 : 20 + (int)(a->p[ACID_SLDT] * 280.0f); } // portamento 20..300 (classic = vanilla's fixed 60)
static int   acid_atk_ms(Acid *a)   { return a->classic ? 2 : (int)(0.5f + a->p[ACID_ATK] * 29.5f); } // amp attack 0.5..30 ms (classic = vanilla's 2ms)
static float acid_acc_mul(Acid *a)  { return 1.0f + a->p[ACID_ACC] * 1.5f; }                      // accent env multiplier 1..2.5
static float acid_sq_mul(Acid *a)   { return 1.0f + a->p[ACID_SQL] * 2.0f; }                      // env-sweep depth 1..3
static int   acid_sweep_atk(Acid *a){ static const int A[4] = { 0, 10, 5, 2 }; return A[a->sweep & 3]; }

// analog drift — the missing "living" imperfection. LFO_SHAPE_RANDOM is the engine's slow
// filtered-random wander; a touch on PITCH (~cents) + CUTOFF (~Hz) so the saw+filter aren't a
// frozen-perfect digital tone. Slots 0/1 (2 free); different rates so they never lock. Each
// note-on re-seeds independently, so 303a/303b drift apart. drift=0 → depth 0 → skipped (byte-
// identical to the old dead-flat voice). Broken out so DRIFT can RIDE (acid_ride re-applies on change).
static void acid_set_drift(Acid *a) {
    instrument_lfo(a->slot, 0, LFO_PITCH,  0.13f, 0.05f * a->drift); lfo_shape(a->slot, 0, LFO_SHAPE_RANDOM);
    instrument_lfo(a->slot, 1, LFO_CUTOFF, 0.19f, 40.0f * a->drift); lfo_shape(a->slot, 1, LFO_SHAPE_RANDOM);
    if (a->subslot >= 0) { instrument_lfo(a->subslot, 0, LFO_PITCH, 0.11f, 0.06f * a->drift); lfo_shape(a->subslot, 0, LFO_SHAPE_RANDOM); }
    a->_ld = a->drift;
}

// ── voice definition (call on init + after ATK / wave / drvmode change) ──────
static void acid_define(Acid *a) {
    instrument(a->slot, a->wave, acid_atk_ms(a), 60, 6, 25);
    instrument_duty(a->slot, 0.48f);
    instrument_filter(a->slot, FILTER_DIODE, acid_cut_hz(a), acid_res_q(a));
    instrument_drive(a->slot, a->p[ACID_DRV]);
    instrument_drive_mode(a->slot, a->drvmode);
    instrument_bandlimit(a->slot, a->clean);       // raw naive saw (default) vs PolyBLEP clean saw (sub is TRI → n/a)
    if (a->echo_send > 0.0f) instrument_echo(a->slot, a->echo_send);
    if (a->rev_send  > 0.0f) instrument_reverb(a->slot, a->rev_send);
    if (a->subslot >= 0)     instrument(a->subslot, INSTR_TRI, acid_atk_ms(a), 60, 6, 25);
    acid_set_drift(a);                             // the living analog wander (sets a->_ld)
    a->_lc = acid_cut_hz(a); a->_lr = acid_res_q(a);
}

// set up an Acid with stock-303 defaults. subslot = -1 for no sub-osc.
static void acid_init(Acid *a, int slot, int subslot) {
    static const float def[ACID_NPARAM] =
        { 0.45f, 0.70f, 0.60f, 0.40f, 0.60f, 0.35f, 0.33f, 0.40f, 0.14f, 0.05f, 0.0f, 0.0f };
    a->slot = slot; a->subslot = subslot; a->wave = INSTR_SAW;
    a->drvmode = DRIVE_SOFT; a->sweep = 0; a->base = 36;
    a->cut_top = 6.38f;                            // Devil Fish wide range by default
    a->drift   = 0.5f;                             // gentle analog drift on by default (set 0 for dead-flat)
    a->classic = 0;                                // Devil Fish voicing by default (set 1 for vanilla pre-DF)
    a->clean   = 0;                                // RAW saw by default (the grit); set 1 for the PolyBLEP clean saw
    a->echo_send = 0.10f; a->rev_send = 0.0f;      // tb303's subtle slapback, dry reverb
    a->h = a->hsub = -1; a->prev_slide = 0;
    for (int i = 0; i < ACID_NPARAM; i++) a->p[i] = def[i];
    acid_define(a);
}

// voice it as a STOCK (vanilla, pre-Devil-Fish) TB-303: the extended params park at
// their stock values (they collapse to vanilla behaviour), the narrower cutoff range,
// and the subtle slapback echo. The depth is still all there — dial the params up for
// Devil Fish. This is "both voicings, one header, chosen by data." Call after acid_init.
//
// GOTCHA — the two-decay: this sets ADEC = DEC ONCE. If your cart moves DEC at runtime
// (a knob) and you want to STAY vanilla (accent decays like a normal note), you must
// re-sync `a.p[ACID_ADEC] = a.p[ACID_DEC]` every frame — otherwise ADEC stays frozen at
// the init DEC and the accent decay silently diverges as you turn DEC. (tb303 does this
// in its update() sync; acidrack intentionally lets ADEC be its own knob = two-decay on.)
static void acid_stock(Acid *a) {
    a->cut_top       = 6.0f;                        // vanilla ceiling ~3840 Hz
    a->p[ACID_SLDT]  = 0.143f;                      // exactly 60 ms slide (vanilla's fixed glide)
    a->p[ACID_ADEC]  = a->p[ACID_DEC];              // two-decay off (the stock voicing's accent decay is the FIXED 200 ms, see the real-303 block)
    a->p[ACID_ATK]   = 0.051f;                      // exactly 2 ms attack (matches vanilla's define)
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
    if (a->drift != a->_ld) acid_set_drift(a);     // DRIFT rides live (re-apply the wander LFOs only on change)
    if (a->h >= 0) { note_cutoff(a->h, c); note_res(a->h, acid_res_f(a)); note_drive(a->h, a->p[ACID_DRV]); }
    if (a->h >= 0 && acid_real303(a)) {            // (1) the VCA decays while held: one frame per call
        int k = acid_fade_slot_(a);
        if (k >= 0) { note_vol(a->h, acid_fade_[k].vol * de_expf(-acid_fade_[k].frames / (60.0f * 1.23f))); acid_fade_[k].frames++; }
    }
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
    float vol = accent ? 7.0f : (acid_real303(a) ? 3.27f : 5.0f);   // (2) real 303: accent +6.6 dB over plain
    if (a->h >= 0 && a->prev_slide) {                       // SLIDE — hold + glide, env does NOT refire
        int g = acid_slide_ms(a);
        note_glide(a->h, g); note_pitch(a->h, (float)midi);
        float fv = vol;                                       // real 303: a slide keeps the VCA's fade going
        if (acid_real303(a)) { int k = acid_fade_slot_(a); if (k >= 0) fv = vol * de_expf(-acid_fade_[k].frames / (60.0f * 1.23f)); }
        note_vol(a->h, fv);
        if (a->hsub >= 0) { note_glide(a->hsub, g); note_pitch(a->hsub, (float)midi - 12); }
    } else {
        if (a->h    >= 0) note_off(a->h);
        if (a->hsub >= 0) { note_off(a->hsub); a->hsub = -1; }
        float e    = acid_env_hz(a) * acid_sq_mul(a) * (accent ? acid_acc_mul(a) : 1.0f);
        int   dec  = (accent && acid_real303(a)) ? 200                            // (3) real 303: an accent's decay is FIXED
                   : (accent && !a->classic) ? acid_adec_ms(a) : acid_dec_ms(a);  // Devil Fish two-decay
        int   soft = acid_atk_ms(a) - 2; if (soft < 0) soft = 0;      // soft attack rounds the filter onset too
        int   atk  = (accent && !a->classic) ? acid_sweep_atk(a) : 0; // accent-sweep onset (classic: no sweep)
        if (soft > atk) atk = soft;
        int   cut = acid_cut_hz(a);
        if (!a->classic && a->p[ACID_TRK] > 0.001f) { cut += (int)(a->p[ACID_TRK] * (midi - a->base) * 170.0f); if (cut < 30) cut = 30; }  // classic: no filter-tracking
        instrument_filter(a->slot, FILTER_DIODE, cut, acid_res_q(a)); a->_lc = cut; a->_lr = acid_res_q(a);
        instrument_env(a->slot, 0, ENV_CUTOFF, atk, dec, (int)e);
        a->h = note_on(midi, a->slot, (int)vol); note_glide(a->h, 0);
        if (vol != (float)(int)vol) note_vol(a->h, vol);
        if (acid_real303(a)) { int k = acid_fade_slot_(a); if (k >= 0) acid_fade_[k].frames = 0; }   // a fresh note restarts the VCA
        float sl = a->classic ? 0.0f : a->p[ACID_SUB];                // sub-osc, an octave down (classic: no sub)
        if (a->subslot >= 0 && sl > 0.001f) {
            a->hsub = note_on(midi - 12, a->subslot, 0);
            note_vol(a->hsub, vol * sl); note_glide(a->hsub, 0);
        }
    }
    if (acid_real303(a)) { int k = acid_fade_slot_(a); if (k >= 0) acid_fade_[k].vol = vol; }   // a slide keeps fading from where it is
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
