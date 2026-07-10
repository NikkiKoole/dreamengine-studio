/* de:meta
{
  "slug": "transitions",
  "title": "Transitions",
  "status": "active",
  "created": "2026-06-17",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "dithering-gradient",
    "state-machine"
  ],
  "lineage": "A standalone showcase of classic screen-wipe patterns (iris, clock, dissolve, slide) drawn as black masks over a live scene; the Bayer ordered-dither dissolve is the conceptual centrepiece, and the OUT/swap/IN phase loop is a minimal state machine.",
  "description": "The screen wipes that bridge two scenes, side by side over a live world. Ten: IRIS (a circle closes onto the hero, SNES Mario / Looney Tunes), CLOCK WIPE (a radial sweep, Star Wars), WIPE (a hard edge slides across), CURTAIN (top + bottom slabs meet), BLINDS (venetian slats snap shut), DISSOLVE (an ordered-dither pixel fade, Genesis/Sonic), FADE (dim to black), LUMA (a grayscale swirl thresholds in PIXEL-PERFECT via per-row spans — the GENERALIZATION: any grayscale 'image' is a transition, the shape becomes data not code), TRIANGLE (a wipe with symmetric triangle teeth — also a luma field: a ramp + a vertical tooth wave, monotonic so one span per row; saw vs triangle is just the waveform), and SLIDE PUSH (the new scene shoves the old one off-screen). The mask-based nine each play OUT, swap a sunny hilltop for a starlit cave at full black, then play back IN — the trick is one black mask whose shape encodes how-covered-are-we (iris=ring(), clock=arcfill(), dissolve=fillp() Bayer dither, fade=fade(), luma=threshold a grayscale field by coverage); SLIDE instead camera-shifts both scenes in one motion. A tour of the masking primitives, with a selectable easing curve (linear / in / out / in-out / back) — ease-back's overshoot is visible on the slide. Left/right pick, Z play, up/down speed, X cycle easing."
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// TRANSITIONS — the screen wipes that bridge two scenes.
//
// The classics every game leans on, side by side over a live scene:
//   IRIS     — a circle closes onto the hero, then opens on the next place
//              (SNES Mario, Looney Tunes "that's all folks")
//   CLOCK    — a radial sweep wipes round like a clock hand (Star Wars)
//   WIPE     — a hard black edge slides across the screen
//   CURTAIN  — top and bottom slabs slide shut, then part again
//   BLINDS   — venetian slats snap closed
//   DISSOLVE — an ordered-dither pixel dissolve (Genesis / Sonic)
//   FADE     — the whole screen dims to black and back
//   LUMA     — a grayscale "image" thresholded by coverage: ANY shape becomes a
//              transition (here a swirl). the GENERALIZATION of every mask above —
//              the shape is DATA, not code (could be a grayscale sprite). drawn
//              PIXEL-PERFECT: scan each row for covered runs (gray < p) and fill
//              them — 64k cheap compares + ~1k spans, never 64k psets.
//   TRIANGLE — a wipe whose advancing edge is triangle teeth (symmetric points).
//              ALSO a luma field (ramp + a vertical TOOTH wave — triangle here,
//              sawtooth by swapping one line), monotonic in x so each row is ONE
//              span solved directly — the cheap analytic face of luma.
//   SLIDE    — the new scene shoves the old one off-screen (no black middle).
//              this is the one where ease-back's overshoot is VISIBLE — the
//              incoming scene slides a touch too far, then springs back to rest.
//
// ←/→ pick a transition · Z play it (it wipes OUT, swaps the scene, wipes
// back IN) · ↑/↓ change speed · X cycle the EASING curve. A linear wipe feels
// mechanical; the eased curves (the engine's ease_in/out/in_out) give it the
// accelerate-into-black, settle-on-the-reveal weight the classics have.
//
// The trick worth stealing: render the scene normally, THEN overlay a black
// mask whose shape encodes "how covered are we" (p = 0 clear … 1 black). OUT
// ramps p 0→1, we swap scenes at black, IN ramps p 1→0. Every transition is
// just a different black mask — see mask() below; most are a single primitive.

typedef enum { T_IRIS, T_CLOCK, T_WIPE, T_CURTAIN, T_BLINDS, T_DISSOLVE, T_FADE, T_LUMA, T_TRI, T_SLIDE, T_COUNT } Trans;
static const char *NAMES[T_COUNT] = {
    "IRIS", "CLOCK WIPE", "WIPE", "CURTAIN", "BLINDS", "DISSOLVE", "FADE", "LUMA", "TRIANGLE", "SLIDE PUSH"
};

typedef enum { PH_IDLE, PH_OUT, PH_IN } Phase;

typedef enum { E_LINEAR, E_IN, E_OUT, E_INOUT, E_BACK, E_COUNT } Ease;
static const char *EASE_NAMES[E_COUNT] = { "linear", "ease-in", "ease-out", "ease-in-out", "ease-back" };
static float apply_ease(int e, float x) {
    switch (e) {
        case E_IN:    return ease_in(x);
        case E_OUT:   return ease_out(x);
        case E_INOUT: return ease_in_out(x);
        case E_BACK:  return ease_back(x);  // overshoots >1 — masks clamp for it
        default:      return x;
    }
}

#define NSTARS 40
#define BLACK  CLR_BLACK

static int   sel;            // selected transition
static int   scene;          // which scene is showing (0 day, 1 cave)
static Phase phase;
static float t;              // 0..1 progress inside the current phase
static float speed = 0.025f; // p per frame
static int   ease = E_INOUT; // easing curve applied to coverage

static float fx, fy;         // hero focal point (iris / clock centre)
static int   star_x[NSTARS], star_y[NSTARS], star_b[NSTARS];

// ── LUMA: a grayscale field thresholded by coverage — THE generalization (any
//    grayscale "image" is a transition: a pixel goes black when its gray < p).
//    Computed once at full resolution into lumapx[] (a swirl); the LUMA transition
//    renders it PIXEL-PERFECT via per-row spans (scan each row for covered runs).
//    (An A/B against a chunky cell version chose pixel — see design/transitions.md.)
static unsigned char lumapx[SCREEN_H][SCREEN_W]; // 0..255 grayscale; the SHAPE, as data

// ── farthest-corner radius from a focal point: how big the mask must grow ──
static float corner_radius(float cx, float cy) {
    float best = 0;
    int xs[2] = {0, SCREEN_W}, ys[2] = {0, SCREEN_H};
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
        float dx = xs[i] - cx, dy = ys[j] - cy;
        float d = sqrtf(dx * dx + dy * dy);
        if (d > best) best = d;
    }
    return best;
}

void init(void) {
    sel = T_IRIS; scene = 0; phase = PH_IDLE; t = 0;
    fx = SCREEN_W / 2; fy = SCREEN_H / 2 + 24;
    for (int i = 0; i < NSTARS; i++) {
        star_x[i] = rnd_between(0, SCREEN_W);
        star_y[i] = rnd_between(0, SCREEN_H - 50);
        star_b[i] = rnd_between(0, 3);   // twinkle phase
    }
    // bake the luma field once at full res (a 3-arm swirl). This IS the "image" —
    // swap the formula (or read a sprite) for a different transition, free.
    for (int y = 0; y < SCREEN_H; y++) for (int x = 0; x < SCREEN_W; x++) {
        float dx = x - SCREEN_W / 2.0f, dy = y - SCREEN_H / 2.0f;
        float r = sqrtf(dx * dx + dy * dy);
        float a = atan2f(dy, dx);
        float v = 0.5f + 0.5f * sinf(a * 3.0f + r * 0.06f);   // swirl arms
        lumapx[y][x] = (unsigned char)(clamp(v, 0.0f, 0.98f) * 255.0f); // <255 so p≈1 covers all
    }
}

// ── the two places we travel between ──────────────────────────────────────
static void draw_hero(void) {
    float bob = sinf(now() * 3.0f) * 2.0f;
    int x = (int)fx, y = (int)(fy + bob);
    circfill(x, y - 10, 5, CLR_PEACH);              // head
    rectfill(x - 4, y - 5, 8, 11, CLR_RED);         // body
    rectfill(x - 4, y + 6, 3, 5, CLR_DARK_BLUE);    // legs
    rectfill(x + 1, y + 6, 3, 5, CLR_DARK_BLUE);
}

static void scene_day(void) {
    vgradient(0, 0, SCREEN_W, SCREEN_H - 40, CLR_BLUE, CLR_LIGHT_GREY);
    circfill(SCREEN_W - 48, 40, 18, CLR_YELLOW);                 // sun
    // rolling hills
    arcfill(70, SCREEN_H + 30, 90, 180, 360, CLR_GREEN);
    arcfill(220, SCREEN_H + 40, 110, 180, 360, CLR_GREEN);
    rectfill(0, SCREEN_H - 40, SCREEN_W, 40, CLR_GREEN);
    // a little cloud drifting
    int cx = (int)(now() * 12) % (SCREEN_W + 60) - 30;
    circfill(cx, 34, 9, CLR_WHITE);
    circfill(cx + 12, 30, 11, CLR_WHITE);
    circfill(cx + 24, 34, 9, CLR_WHITE);
    draw_hero();
}

static void scene_cave(void) {
    rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_DARK_BLUE);  // not cls() — must shift under camera() for SLIDE
    for (int i = 0; i < NSTARS; i++) {
        float tw = sinf(now() * 2.0f + star_b[i] * 1.7f);
        pset(star_x[i], star_y[i], tw > 0.2f ? CLR_WHITE : CLR_INDIGO);
    }
    circfill(54, 44, 15, CLR_LIGHT_GREY);                        // moon
    circfill(48, 40, 13, CLR_DARK_BLUE);                         // crescent bite
    // jagged cave floor
    rectfill(0, SCREEN_H - 36, SCREEN_W, 36, CLR_DARK_GREY);
    for (int x = 0; x < SCREEN_W; x += 18)
        trifill(x, SCREEN_H - 36, x + 9, SCREEN_H - 50, x + 18, SCREEN_H - 36, CLR_DARK_GREY);
    draw_hero();
}

static void draw_scene(int which) {
    if (which == 0) scene_day(); else scene_cave();
}

// ── the black mask for transition `tr` at coverage p (0 clear … 1 black) ───
static void mask(int tr, float p) {
    if (p <= 0.0001f) return;
    switch (tr) {
        case T_IRIS: {
            // a black annulus from the shrinking hole out past the corner.
            float maxR = corner_radius(fx, fy) + 2;
            float hole = clamp(maxR * (1.0f - p), 0, maxR);  // ease_back can push p>1
            ring((int)fx, (int)fy, (int)hole, (int)maxR, 0, 360, BLACK);
        } break;
        case T_CLOCK: {
            // a pie wedge sweeping round from 12 o'clock.
            float maxR = corner_radius(SCREEN_W / 2, SCREEN_H / 2) + 2;
            arcfill(SCREEN_W / 2, SCREEN_H / 2, (int)maxR, -90, -90 + p * 360, BLACK);
        } break;
        case T_WIPE: {
            rectfill(0, 0, (int)(SCREEN_W * p), SCREEN_H, BLACK);
        } break;
        case T_CURTAIN: {
            int h = (int)(SCREEN_H / 2 * p);
            rectfill(0, 0, SCREEN_W, h, BLACK);
            rectfill(0, SCREEN_H - h, SCREEN_W, h, BLACK);
        } break;
        case T_BLINDS: {
            int slats = 8, band = SCREEN_H / slats;
            int h = (int)(band * p);
            for (int i = 0; i < slats; i++) rectfill(0, i * band, SCREEN_W, h, BLACK);
        } break;
        case T_DISSOLVE: {
            // ordered 4×4 Bayer dither: switch on cells whose threshold < level.
            static const int bayer[16] = { 0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5 };
            int level = (int)(p * 16.0f + 0.5f);
            int pat = 0;
            for (int c = 0; c < 16; c++) if (bayer[c] < level) pat |= (1 << (15 - c));
            fillp(pat, -1);                                  // 1-bits transparent
            rectfill(0, 0, SCREEN_W, SCREEN_H, BLACK);
            fillp_reset();
        } break;
        case T_FADE: {
            fade(p);
        } break;
        case T_LUMA: {
            // PIXEL-PERFECT: scan each row of the precomputed field for covered runs
            // (gray < p) and fill each run — 64k cheap compares + ~1k fills, NOT
            // 64k psets. ANY grayscale "image" becomes a smooth transition this way.
            int thr = (int)(p * 255.0f);
            for (int y = 0; y < SCREEN_H; y++) {
                int x = 0;
                while (x < SCREEN_W) {
                    if (lumapx[y][x] < thr) {
                        int x0 = x;
                        while (x < SCREEN_W && lumapx[y][x] < thr) x++;
                        rectfill(x0, y, x - x0, 1, BLACK);
                    } else x++;
                }
            }
        } break;
        case T_TRI: {
            // a TRIANGLE-tooth wipe — same luma field idea (ramp x/W + a vertical
            // tooth wave), monotonic in x so each row is ONE span: fill 0..edge.
            // The tooth WAVEFORM is the only knob: triangle here; sawtooth would be
            // s = (float)(y % teeth) / teeth.
            float amp = 0.10f; int teeth = 40;            // tooth depth (frac W), rows/tooth
            for (int y = 0; y < SCREEN_H; y++) {
                float ph = (float)(y % teeth) / teeth;    // 0..1 within a tooth
                float s = 1.0f - fabsf(2.0f * ph - 1.0f); // triangle: 0 → 1 → 0
                float cov = clamp(p * (1.0f + amp) - amp * s, 0.0f, 1.0f);
                int edge = (int)(cov * SCREEN_W);
                if (edge > 0) rectfill(0, y, edge, 1, BLACK);
            }
        } break;
    }
}

// SLIDE is the odd one out: no black mask, no swap-at-black. It draws BOTH
// scenes side by side and shoves them left by an eased pixel offset (0 → one
// full screen). Because the offset is a POSITION, ease-back's overshoot shows
// up for real — the incoming scene slides past its resting spot, then springs
// back. cls() would ignore camera() and wipe everything, so the scenes paint
// their backgrounds with rectfill instead (see scene_cave).
static void draw_slide(float p) {
    int off = (int)(SCREEN_W * p);
    camera(off, 0);              draw_scene(scene);       // outgoing slides left
    camera(off - SCREEN_W, 0);   draw_scene(1 - scene);   // incoming follows from the right
    camera(0, 0);
}

void update(void) {
    if (phase == PH_IDLE) {
        if (btnp(0, BTN_LEFT))  sel = (sel + T_COUNT - 1) % T_COUNT;
        if (btnp(0, BTN_RIGHT)) sel = (sel + 1) % T_COUNT;
        if (btnp(0, BTN_UP))    speed = clamp(speed + 0.005f, 0.008f, 0.06f);
        if (btnp(0, BTN_DOWN))  speed = clamp(speed - 0.005f, 0.008f, 0.06f);
        if (btnp(0, BTN_B))     ease = (ease + 1) % E_COUNT;
        if (btnp(0, BTN_A))     { phase = PH_OUT; t = 0; }
        return;
    }
    t += speed;
    if (t >= 1.0f) {
        if (phase == PH_OUT && sel == T_SLIDE) { scene = 1 - scene; phase = PH_IDLE; t = 0; }  // single motion
        else if (phase == PH_OUT)              { scene = 1 - scene; phase = PH_IN;   t = 0; }  // swap at black
        else                                   { phase = PH_IDLE; t = 0; }
    }
}

void draw(void) {
    if (phase != PH_IDLE && sel == T_SLIDE) {
        draw_slide(apply_ease(ease, t));   // one continuous push, both scenes on screen
        return;
    }

    draw_scene(scene);

    // ease the coverage: OUT runs forward, IN mirrors it (eased open) — so the
    // motion accelerates into black and settles softly onto the revealed scene.
    if (phase == PH_OUT) mask(sel, apply_ease(ease, t));
    else if (phase == PH_IN) mask(sel, apply_ease(ease, 1.0f - t));

    if (phase == PH_IDLE) {
        // chrome
        rectfill(0, 0, SCREEN_W, 16, BLACK);
        int w = print(NAMES[sel], 0, 0, CLR_WHITE);
        print("<", w + 6, 0, CLR_YELLOW);
        print(">", w + 16, 0, CLR_YELLOW);
        print(EASE_NAMES[ease], w + 30, 0, CLR_GREEN);
        char sp[24]; snprintf(sp, sizeof sp, "spd %d", (int)(speed * 1000));
        print(sp, SCREEN_W - 52, 0, CLR_MEDIUM_GREY);

        print("Z play  <> pick  ^v speed  X ease", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
    }
}
