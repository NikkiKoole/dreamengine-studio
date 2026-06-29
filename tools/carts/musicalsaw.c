/* de:meta
{
  "title": "musical saw",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "instrument",
    "probe"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Spiritual sibling of theremin — the one cart that drives note_lfo live from gesture (bow speed → volume, wrist wobble → LFO depth+rate), rather than setting it once at init.",
  "description": "The theremin's spooky cousin, and the one cart that drives note_lfo LIVE — the vibrato is something you PLAY, not a fixed setting. One sustained sine voice (note_on) bowed and bent by hand. BOW it: hold the mouse and saw LEFT-RIGHT — your bow speed is the volume (note_vol), so it dies when you stop, like a real bow leaving the blade. BEND it: up/down flexes the steel blade into a deeper or shallower S-curve and the CURVATURE is the pitch (note_pitch + note_glide for the smear). SING it: wobble your hand vertically while bowing and that wrist-shake feeds a live note_lfo vibrato whose depth AND rate climb with how hard you shake — you hear it shiver and watch the blade shimmer. Ethereal, vocal, haunting."
}
de:meta */
#include "studio.h"
#include <math.h>

// Musical saw — the theremin's spooky cousin, built on the HELD NOTE api. One
// sustained sine voice (note_on) bowed and bent live. What makes it its OWN
// instrument and not a reskinned theremin: it drives note_lfo LIVE. Every other
// cart sets its LFO once at init; here the VIBRATO is something you play —
//
//   * BOW it: hold the mouse and saw LEFT-RIGHT. Your horizontal bow speed is the
//     volume (note_vol) — stop sawing and it dies, like a real bow leaving the
//     blade. Hold still = silence.
//   * BEND it: up/down flexes the steel blade into a deeper or shallower S-curve,
//     and the CURVATURE is the pitch (note_pitch + note_glide for the smear).
//   * SING it: wobble your hand vertically while bowing and that wrist-shake feeds
//     a live note_lfo vibrato — depth AND rate climb with how hard you wobble, so
//     you hear (and watch the blade) shiver. The saw's whole eerie identity.
//
// CONTROLS: hold + saw left-right to bow · up/down bends the pitch · wiggle to sing.

#define SLOT 5
#define X0   24            // blade root (the handle)
#define X1   304           // blade tip
#define CY   102           // straight-blade centre line

static int   voice;        // the one held sine voice
static int   have_prev;    // mouse-velocity warm-up guard
static float prev_mx, prev_my;
static float bow_spd;      // smoothed horizontal bow speed (px/frame) → volume
static float wobble;        // smoothed vertical wobble (px/frame)   → vibrato
static float bend_vis;     // smoothed blade curvature for drawing
static float vib_vis;      // smoothed vibrato amount for drawing
static float lfo_phase;    // visual wobble phase

static float cub(float a, float b, float c, float d, float t) {   // cubic bezier
    float u = 1 - t;
    return u*u*u*a + 3*u*u*t*b + 3*u*t*t*c + t*t*t*d;
}

void init(void) {
    instrument(SLOT, INSTR_SINE, 120, 200, 7, 600);     // smooth swell, long tail
    instrument_lfo(SLOT, 1, LFO_VOLUME, 5.2f, 0.22f);   // the constant eerie waver
    voice = note_on(60, SLOT, 0);                       // alive but silent
    note_glide(voice, 80);                              // the bent-blade smear
}

void update(void) {
    int   down = mouse_down(MOUSE_LEFT);
    float mx = mouse_x(), my = mouse_y();
    float vx = 0, vy = 0;
    if (have_prev) { vx = mx - prev_mx; vy = my - prev_my; }
    prev_mx = mx; prev_my = my; have_prev = 1;

    // bow speed (horizontal) → loudness;  wrist wobble (vertical) → vibrato
    bow_spd = lerp(bow_spd, down ? fabsf(vx) : 0, 0.25f);
    wobble   = lerp(wobble,   down ? fabsf(vy) : 0, 0.30f);

    float vol01 = clamp(bow_spd / 9.0f, 0, 1);
    note_vol(voice, vol01 * 7.0f);

    // up/down flexes the blade; the curvature IS the pitch
    float raw_bend = CY - my;                           // + above centre
    float pitch = clamp(60 + raw_bend * 0.22f, 40, 81);
    note_pitch(voice, pitch);

    // LIVE vibrato: depth and rate both rise with how hard you wobble
    float vibd = clamp(wobble / 8.0f, 0, 1);
    note_lfo(voice, 0, LFO_PITCH, 5.0f + vibd * 2.5f, vibd * 1.3f);

    // smoothed values for a calm, shimmering drawing
    bend_vis  = lerp(bend_vis, raw_bend, 0.22f);
    vib_vis   = lerp(vib_vis,  vibd,     0.30f);
    lfo_phase += (5.0f + vibd * 2.5f) * 0.11f;
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    int sounding = bow_spd > 0.4f;
    float bend = bend_vis;
    float vibpx = vib_vis * 6.0f;                       // visual wobble amplitude

    // wooden handle
    rectfill(X0 - 16, CY - 10, 18, 20, CLR_BROWN);
    rect(X0 - 16, CY - 10, 18, 20, CLR_DARK_BROWN);

    // the flexing steel blade: a tapering S-curve drawn as vertical slices, with a
    // bright top edge and the vibrato shivering the tip
    int body  = sounding ? CLR_WHITE : CLR_LIGHT_GREY;
    int edge  = sounding ? CLR_LIGHT_PEACH : CLR_MEDIUM_GREY;
    for (float t = 0; t <= 1.0f; t += 0.008f) {
        float x  = cub(X0, X0 + (X1 - X0) * 0.34f, X0 + (X1 - X0) * 0.66f, X1, t);
        float yb = cub(CY, CY - bend, CY + bend, CY, t);
        float y  = yb + sinf(lfo_phase + t * 7.0f) * vibpx * t;   // tip wobbles most
        int   hw = (int)lerp(7, 1.2f, t);                          // taper to the tip
        line((int)x, (int)y - hw, (int)x, (int)y + hw, body);
        pset((int)x, (int)y - hw, edge);                           // shimmer highlight
        pset((int)x, (int)y + hw, CLR_DARKER_GREY);                // under-shadow
    }

    // the sounding "node" — the sweet spot where the S crosses centre — glows with volume
    if (sounding) {
        int gx = (int)cub(X0, X0 + (X1 - X0) * 0.34f, X0 + (X1 - X0) * 0.66f, X1, 0.5f);
        int r  = 2 + (int)(bow_spd * 0.5f);
        circfill(gx, CY, r, CLR_WHITE);
        circ(gx, CY, r + 2, CLR_PINK);
    }

    // ---- HUD ----
    print("MUSICAL SAW", 6, 6, CLR_INDIGO);
    print("hold + saw L-R to bow   up/down: pitch", 6, 178, CLR_MAUVE);
    print("wiggle while bowing to sing (vibrato)", 6, 188, CLR_DARKER_GREY);

    // meters: bow (loudness) and vibrato depth
    font(FONT_SMALL);
    print("BOW", 226, 11, CLR_DARKER_GREY);
    print("VIB", 226, 21, CLR_DARKER_GREY);
    font(FONT_NORMAL);
    rect(250, 10, 62, 6, CLR_DARKER_GREY);
    rectfill(251, 11, (int)(clamp(bow_spd / 9.0f, 0, 1) * 60), 4, CLR_LIGHT_PEACH);
    rect(250, 20, 62, 6, CLR_DARKER_GREY);
    rectfill(251, 21, (int)(vib_vis * 60), 4, CLR_PINK);
}
