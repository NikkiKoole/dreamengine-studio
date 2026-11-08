/* de:meta
{
  "slug": "acidref",
  "title": "acid ref probe",
  "status": "active",
  "created": "2026-11-08",
  "kind": ["probe"],
  "teaches": ["analog-voice-modeling"],
  "lineage": "The acid303.h half of an A/B against Robin Schmidt's Open303 (MIT): plays the exact pattern tools/ref-render/open303render.cpp plays, on the same sample grid, so the two WAVs drop into the same oracles. Prompted by X0X (charlesvestal/fm1-x0x), whose 303 is an Open303 port tested against the original.",
  "description": "Not a toy: a measurement probe. Plays a fixed 16-step acid line twice (accents on 3/7/10/14, slides into 5/8/13), then one long plain note and one long accented note, through acid303.h. Render with node tools/play.js acidref run --headless --frames 704 --wav out.wav and compare with bash tools/ref-render/run.sh open303. DE_DEFINES: ACID_KNOB=0.5 sets the five core knobs; ACID_DF=1 uses the Devil Fish voicing instead of acid_stock."
}
de:meta */
#include "studio.h"
#include "acid303.h"

// ── the pattern: keep IDENTICAL to tools/ref-render/open303render.cpp ────
static const int N[16] = { 0, 0, 12, 0, 3, 0, 7, 5, 0, 12, 10, 0, 3, 15, 0, 7 };
static const int A[16] = { 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0 };
static const int S[16] = { 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0 };
#define BASE 36
#define LONG_A 40
#define LONG_B 64
#define LONG_LEN 15
#ifndef ACID_KNOB
#define ACID_KNOB 0.5f
#endif

static Acid a;
static int f;
static int amp_f0;          // AB_AMP: the frame the VCA envelope last restarted
static float amp_vol;

// ── A/B switches (DE_DEFINES), each one of the differences Open303 showed. All off = acid303.h exactly
// (ab_note below is acid_note's own logic, minus the sub-osc/tracking/sweep the stock voicing parks).
//   AB_AMP   held notes FADE: the VCA decays exponentially (tau 1230 ms, Open303's) instead of sustaining at 6/7.
//            Ridden per frame with note_vol: the engine's own decay stage is the WRONG SHAPE for this (it stays
//            near flat then drops; measured, no decay time matches both -4.9 dB at 0.7 s and -12 dB at 1.7 s)
//   AB_ACC   accent +6.6 dB over a plain note (plain drops to vol 3.27, accent stays 7) instead of +2.9
//   AB_ADEC  an accent's filter decays in a FIXED 200 ms, whatever DECAY says
//   AB_RANGE the stock pot ranges at 12 o'clock: cutoff 867 Hz, decay 632 ms (ours: 480 Hz, 280 ms)
static void ab_define(void) {
    acid_define(&a);
}

static void ab_note(int midi, int accent, int slide) {
#ifdef AB_ACC
    float vol = accent ? 7.0f : 3.27f;
#else
    float vol = accent ? 7.0f : 5.0f;
#endif
    if (a.h >= 0 && a.prev_slide) {
        note_glide(a.h, acid_slide_ms(&a)); note_pitch(a.h, (float)midi); note_vol(a.h, vol);
    } else {
        if (a.h >= 0) note_off(a.h);
        float e = acid_env_hz(&a) * acid_sq_mul(&a) * (accent ? acid_acc_mul(&a) : 1.0f);
#ifdef AB_RANGE
        int dec = 632;
#else
        int dec = acid_dec_ms(&a);
#endif
        if (accent) {
#ifdef AB_ADEC
            dec = 200;
#else
            dec = acid_adec_ms(&a);
#endif
        }
        instrument_filter(a.slot, FILTER_DIODE, acid_cut_hz(&a), acid_res_q(&a));
        a._lc = acid_cut_hz(&a); a._lr = acid_res_q(&a);
        instrument_env(a.slot, 0, ENV_CUTOFF, 0, dec, (int)e);
        a.h = note_on(midi, a.slot, (int)vol);
        if (vol != (int)vol) note_vol(a.h, vol);
        note_glide(a.h, 0);
        amp_f0 = f;                  // a fresh note restarts the VCA; a slide does not
    }
    amp_vol = vol;
    a.prev_slide = slide;
}

void init(void) {
    acid_init(&a, 6, -1);
#ifndef ACID_DF
    acid_stock(&a);
#endif
    a.drift = 0.0f;                  // dead-flat: a measurement, not a performance
    a.echo_send = 0.0f;              // dry: Open303 has no slapback
    a.p[ACID_CUT] = a.p[ACID_RES] = a.p[ACID_ENV] = a.p[ACID_DEC] = a.p[ACID_ACC] = ACID_KNOB;
    a.p[ACID_DRV] = 0.0f;
    a.p[ACID_ADEC] = a.p[ACID_DEC];
#ifdef AB_RANGE
    a.p[ACID_CUT] = 0.642f;          // 60 * 2^(0.642 * 6) = 867 Hz, the stock pot's middle
#endif
    ab_define();
}

void update(void) {
    int st = f / 8, sub = f % 8;
    acid_ride(&a);
    if (st < 32) {
        int k = st % 16;
        if (sub == 0) ab_note(BASE + N[k], A[k], S[k]);
        if (sub == 5 && (!S[k] || st == 31)) acid_off(&a);
    } else if ((st == LONG_A || st == LONG_B) && sub == 0) {
        ab_note(BASE + 12, st == LONG_B, 0);
    } else if ((st == LONG_A + LONG_LEN || st == LONG_B + LONG_LEN) && sub == 5) {
        acid_off(&a);
    }
#ifdef AB_AMP
    if (a.h >= 0) note_vol(a.h, amp_vol * expf(-(f - amp_f0) / (60.0f * 1.23f)));
#endif
    f++;
}

void draw(void) {
    cls(CLR_BLACK);
    print("acidref: a measurement probe", 8, 8, CLR_LIGHT_GREY);
    print("see tools/ref-render/run.sh open303", 8, 20, CLR_DARK_GREY);
}
