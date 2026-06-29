/* de:meta
{
  "title": "fxmod",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "instrument"
  ],
  "teaches": [],
  "description": "The showcase for fx_mod() / fx_lfo() - the MODULATION layer over effects (the synth-rack 'CV into an effect'). Effects keep their own knobs; this RIDES a curated, sweep-safe one under a control signal, the way LFO_TIMBRE rides an instrument macro. A held saw drone runs through the master effects; 1..3 pick the TARGET param (filter cutoff / drive / shimmer mix), then choose how it's driven: L = engine LFO (fx_lfo(): set once, the engine sweeps it), C = CV by hand (fx_mod(): the cart pushes a value every frame - what modrack does), O = off (the param sits at its static baseline). RATE/DEPTH/CENTER shape the sweep; the big bar shows the value being written. Two APIs, one job: fx_lfo is fire-and-forget, fx_mod is the per-frame sink you drive from your own LFO/envelope/sequencer. Only cheap params are exposed, so it can never reconfigure a buffer effect into a stutter. H help. (ADR 0018.)"
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>

// ── FX MOD ──────────────────────────────────────────────────────────────────
// The showcase for fx_mod() / fx_lfo() — the MODULATION layer over effects (ADR
// 0018). The effects keep their own knobs; this RIDES a curated, sweep-safe one
// under a control signal, the way LFO_TIMBRE rides an instrument macro.
//
// A held saw drone runs through the master effects the whole time.
//   1 2 3   pick the TARGET param to modulate (filter cutoff / drive / shimmer)
//   L       engine LFO  — fx_lfo(): set once, the engine sweeps it (no per-frame calls)
//   C       CV / hand   — fx_mod(): the cart pushes a sine every frame (what modrack does)
//   O       off         — the param sits at its static baseline
//   RATE DEPTH CENTER   shape the sweep
//   H help
//
// Two APIs, one job: fx_lfo is fire-and-forget; fx_mod is the per-frame sink you
// drive from your OWN lfo/envelope/sequencer. The bar shows the value being written.

#define SL_PAD 8

static const int CHORD[4] = { 40, 47, 52, 59 };   // a low saw drone — rich harmonics so the filter sweep SINGS
static int  pad_h[4] = { -1, -1, -1, -1 };

static const int   TGT[3]      = { FXMOD_FILTER_CUT, FXMOD_DRIVE, FXMOD_SHIMMER_MIX };
static const char *TGT_NAME[3] = { "FILTER CUT", "DRIVE", "SHIMMER MIX" };

static int   target = 0;          // index into TGT[]
static int   mode   = 1;          // 0 = off, 1 = engine LFO (fx_lfo), 2 = CV (fx_mod)
static float k_rate = 0.30f;      // 0..1 → 0.05..6 Hz
static float k_depth= 0.45f;      // peak deviation
static float k_ctr  = 0.50f;      // center
static bool  show_help = false;

static float cv_phase = 0.0f;     // the cart's own LFO phase (CV mode + the on-screen meter)
static float disp_val = 0.5f;     // value currently written to the param (for the bar)

// re-arm whenever target/mode/shape changes (NOT every frame — set-and-hold)
static int   a_target = -1, a_mode = -1;
static float a_rate = -1, a_depth = -1, a_ctr = -1;

static float rate_hz(void) { return 0.05f + k_rate * 6.0f; }

static void rearm(void) {
    for (int i = 0; i < 3; i++) fx_lfo(0, TGT[i], 0.0f, 0.0f, 0.0f, LFO_SHAPE_SINE);  // detach every engine LFO (+ any CV)
    // restore the effects' STATIC baselines (modulation overrides these while a target is active)
    filter(FILTER_LOW, 1600.0f, 0.55f);          // a mid-open lowpass — FILTER_CUT rides this
    drive_insert(0.0f, DRIVE_SOFT, 1.0f);        // amt 0 = clean; mix ready so FXMOD_DRIVE is audible when ridden
    shimmer(0.82f, 0.40f, 0.60f, 0.0f);          // mix 0 = off; FXMOD_SHIMMER_MIX blooms it in
    if (mode == 1) fx_lfo(0, TGT[target], rate_hz(), k_depth, k_ctr, LFO_SHAPE_SINE);  // engine LFO arms here; CV pushes in update()
    a_target = target; a_mode = mode; a_rate = k_rate; a_depth = k_depth; a_ctr = k_ctr;
}

void update(void) {
    float dt = 1.0f / 60.0f;
    if (keyp('1')) target = 0;
    if (keyp('2')) target = 1;
    if (keyp('3')) target = 2;
    if (keyp('L')) mode = 1;
    if (keyp('C')) mode = 2;
    if (keyp('O')) mode = 0;
    if (keyp('H')) show_help = !show_help;

    if (target != a_target || mode != a_mode || k_rate != a_rate || k_depth != a_depth || k_ctr != a_ctr)
        rearm();

    // advance a matching sine — drives fx_mod() in CV mode, and the on-screen bar in every mode
    cv_phase += rate_hz() * dt;
    if (cv_phase >= 1.0f) cv_phase -= 1.0f;
    float v = k_ctr + k_depth * sinf(cv_phase * 6.2831853f);
    if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;

    if (mode == 2) { fx_mod(0, TGT[target], v); disp_val = v; }       // CV sink — every frame; engine slews
    else if (mode == 1) disp_val = v;                                 // engine is sweeping; mirror for the meter
    else disp_val = (target == 0) ? 0.5f : 0.0f;                      // off = the static baseline
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    print("FX MOD", 8, 6, CLR_LIGHT_PEACH);

    // target tabs
    font(FONT_SMALL);
    for (int i = 0; i < 3; i++) {
        int tx = 70 + i * 78;
        int c = (i == target) ? CLR_WHITE : CLR_DARK_PURPLE;
        rectfill(tx, 5, 74, 11, (i == target) ? CLR_INDIGO : CLR_DARK_BLUE);
        char b[8]; snprintf(b, sizeof b, "%d", i + 1);
        print(b, tx + 3, 7, CLR_PEACH);
        print(TGT_NAME[i], tx + 12, 7, c);
    }
    font(FONT_NORMAL);

    // which API is driving
    const char *mname = mode == 1 ? "engine LFO  fx_lfo()" : mode == 2 ? "CV / hand  fx_mod()" : "off  (static baseline)";
    int mc = mode == 1 ? CLR_GREEN : mode == 2 ? CLR_YELLOW : CLR_MEDIUM_GREY;
    print(mname, 8, 26, mc);

    // the modulation value being written — a big horizontal bar
    int bx = 8, by = 52, bw = SCREEN_W - 16, bh = 22;
    rect(bx, by, bw, bh, CLR_DARK_PURPLE);
    int fillw = (int)(disp_val * (bw - 2));
    if (fillw < 0) fillw = 0; if (fillw > bw - 2) fillw = bw - 2;
    rectfill(bx + 1, by + 1, fillw, bh - 2, mode == 0 ? CLR_DARK_GREY : mc);
    char vb[40]; snprintf(vb, sizeof vb, "%s = %.2f", TGT_NAME[target], disp_val);
    print(vb, bx + 4, by + 7, CLR_WHITE);

    // sliders
    ui_begin();
    int sy = SCREEN_H - 40, sw = 84;
    ui_slider(&k_rate,  8,            sy, sw, "RATE");
    ui_slider(&k_depth, 8 + (sw+8),   sy, sw, "DEPTH");
    ui_slider(&k_ctr,   8 + (sw+8)*2, sy, sw, "CENTER");
    ui_end();

    font(FONT_SMALL);
    print("1-3 target   L engine  C hand  O off   H help", 8, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(20, 22, SCREEN_W - 40, 104, CLR_DARK_BLUE);
        rect(20, 22, SCREEN_W - 40, 104, CLR_INDIGO);
        print("fx_mod / fx_lfo: modulate FX", 28, 30, CLR_WHITE);
        print("the effect keeps its knobs;", 28, 44, CLR_LIGHT_PEACH);
        print("this RIDES one sweep-safe param.", 28, 54, CLR_LIGHT_PEACH);
        print("fx_lfo = engine sweeps it (set", 28, 68, CLR_GREEN);
        print("once). fx_mod = YOU push a value", 28, 78, CLR_YELLOW);
        print("each frame (modrack's CV sink).", 28, 88, CLR_YELLOW);
        print("only cheap params exposed -> no", 28, 102, CLR_INDIGO);
        print("buffer-effect stutter. H close", 28, 112, CLR_INDIGO);
    }
}

void init(void) {
    instrument(SL_PAD, INSTR_SAW, 140, 0, 6, 500);
    for (int i = 0; i < 4; i++) pad_h[i] = note_on(CHORD[i], SL_PAD, 4);
    rearm();
}
