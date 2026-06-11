#include "studio.h"

// SHADELAB — a CPU fragment shader, the dreamengine way. There's no GPU shader
// hook, so instead each pixel is just a little C function: you get the pixel's
// coordinate (u,v) in 0..1 and the time t, and you return a colour. The engine
// has no per-pixel TRUE-colour write either... except now it does — pset_rgb()
// paints a raw 0xRRGGBB straight to the canvas, so a shader can use all 16.7M
// colours, not the 32-colour palette. This cart evaluates one shader over the
// whole screen, every frame, on the CPU. And the LAST shader (feedback) READS the
// canvas back with pget_rgb — read AND write, so the screen becomes its own memory.
//
// LEFT/RIGHT (or 1..9): switch shader   UP/DOWN: pixel size (see the fragments!)
// SPACE: freeze time   MOUSE: drives the cursor shaders + paints into the feedback one
//
// READ THE SHADER FUNCTIONS BELOW — each is a complete lesson in a few lines.

// ── colour helpers ───────────────────────────────────────────────────────────
static int rgb(float r, float g, float b) {            // 0..1 channels → 0xRRGGBB
    int R = (int)(clamp(r, 0, 1) * 255);
    int G = (int)(clamp(g, 0, 1) * 255);
    int B = (int)(clamp(b, 0, 1) * 255);
    return (R << 16) | (G << 8) | B;
}
static int hsv(float h, float s, float v) {            // hue/sat/val → 0xRRGGBB (h wraps)
    h = (h - (int)h + 1.0f); h = h - (int)h;           // fract(h) into 0..1
    float i = h * 6.0f, f = i - (int)i;
    float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch ((int)i % 6) {
        case 0: return rgb(v, t, p);  case 1: return rgb(q, v, p);
        case 2: return rgb(p, v, t);  case 3: return rgb(p, q, v);
        case 4: return rgb(t, p, v);  default: return rgb(v, p, q);
    }
}
static float fract(float x) { return x - (int)x; }
static int scale_rgb(int c, float b) {                 // dim a 0xRRGGBB by 0..1
    int R = (int)(((c >> 16) & 0xFF) * b), G = (int)(((c >> 8) & 0xFF) * b), B = (int)((c & 0xFF) * b);
    return (R << 16) | (G << 8) | B;
}
static int cadd(int a, int b) {                        // saturating add of two 0xRRGGBB
    int R = ((a >> 16) & 0xFF) + ((b >> 16) & 0xFF); if (R > 255) R = 255;
    int G = ((a >> 8)  & 0xFF) + ((b >> 8)  & 0xFF); if (G > 255) G = 255;
    int B = (a & 0xFF)         + (b & 0xFF);          if (B > 255) B = 255;
    return (R << 16) | (G << 8) | B;
}

// ── UNIFORMS — globals the shaders read, refreshed each frame in update(). Like a
//    real shader's iMouse: the cursor in 0..1, plus whether the button is held. ──
static float mu = 0.5f, mv = 0.5f;   // mouse position, normalised
static bool  mdown;                  // left button held

// ── THE SHADERS — (u,v) in 0..1, t in seconds. each returns a 0xRRGGBB colour. ──

// 1. coordinates ARE colour: red rises with x, green with y. the first shader.
static int sh_uv(float u, float v, float t) { (void)t; return rgb(u, v, 0.4f); }

// 2. a rainbow sweep: hue from x, scrolling in time. hsv is the shader toolkit.
static int sh_sweep(float u, float v, float t) { (void)v; return hsv(u + t * 0.15f, 0.9f, 1.0f); }

// 3. a distance field: how far is this pixel from the centre? colour the rings.
static int sh_sdf(float u, float v, float t) {
    float d = fsqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
    float ring = 0.5f + 0.5f * sin_deg(d * 2600.0f - t * 200.0f);   // concentric pulse
    return hsv(d - t * 0.1f, 0.8f, ring);
}

// 4. PLASMA — sums of sines in x, y and the diagonal. the classic demo effect.
static int sh_plasma(float u, float v, float t) {
    float a = sin_deg(u * 720.0f + t * 70.0f);
    float b = sin_deg(v * 540.0f - t * 50.0f);
    float c = sin_deg((u + v) * 480.0f + t * 90.0f);
    return hsv((a + b + c) * 0.16f + t * 0.05f, 0.85f, 1.0f);
}

// 5. warped value-noise clouds drifting in time (noise2 is the engine's Perlin).
static int sh_noise(float u, float v, float t) {
    float n = noise2(u * 4.0f + t * 0.2f, v * 4.0f - t * 0.15f);
    float m = noise2(u * 9.0f - t * 0.1f, v * 9.0f + t * 0.12f);
    float f = n * 0.7f + m * 0.3f;
    return hsv(0.55f + f * 0.4f, 0.6f, 0.4f + f * 0.6f);          // bluish drifting fog
}

// 6. a checkerboard that warps with a sine — UVs are yours to bend.
static int sh_check(float u, float v, float t) {
    u += sin_deg(v * 360.0f + t * 60.0f) * 0.05f;                  // bend the columns
    int cx = (int)(u * 8) & 1, cy = (int)(v * 8) & 1;
    return (cx ^ cy) ? hsv(t * 0.1f, 0.7f, 1.0f) : rgb(0.05f, 0.05f, 0.1f);
}

// 7. interference: two moving wells, their summed ripples beat against each other.
static int sh_interf(float u, float v, float t) {
    float w1 = fsqrt((u - 0.3f) * (u - 0.3f) + (v - 0.5f) * (v - 0.5f));
    float w2 = fsqrt((u - 0.7f) * (u - 0.7f) + (v - 0.5f) * (v - 0.5f));
    float p = sin_deg(w1 * 3000.0f - t * 200.0f) + sin_deg(w2 * 3000.0f - t * 200.0f);
    return hsv(0.08f, 0.5f, 0.5f + p * 0.25f);                     // warm interference
}

// 8. a torch that follows the cursor: brightness falls off with distance to the
//    mouse uniform. hold the button to widen the beam — uniforms in action.
static int sh_light(float u, float v, float t) {
    float d = fsqrt((u - mu) * (u - mu) + (v - mv) * (v - mv));
    float reach = mdown ? 0.45f : 0.22f;
    float b = clamp(1.0f - d / reach, 0, 1); b *= b;               // soft falloff
    return hsv(0.10f + t * 0.05f, 0.5f, b);                        // warm torchlight
}

// 9. ripples spreading from the cursor: rings of sin() centred on the mouse.
static int sh_ripple(float u, float v, float t) {
    float d = fsqrt((u - mu) * (u - mu) + (v - mv) * (v - mv));
    float r = 0.5f + 0.5f * sin_deg(d * 3600.0f - t * 260.0f);
    return hsv(0.55f - d, 0.7f, r);
}

// 10. FEEDBACK — the read-AND-write shader. Every other shader is a pure function of
//     (u,v,t); this one READS THE PREVIOUS FRAME BACK with pget_rgb, so the screen
//     becomes its own memory. Each cell samples last frame at a slightly rotated +
//     zoomed source position, dims it a touch (the trail decay), and emitters inject
//     fresh light — so colour smears into a glowing, swirling tunnel. Move the MOUSE
//     and HOLD to paint white sparks into the flow. pset_rgb wrote; pget_rgb reads it
//     back. This one isn't in the SHADERS[] table — it needs the whole canvas, not a
//     lone (u,v). NB: ps must stay small here, or the rotate+zoom can't smear smoothly.
//
//     A feedback buffer reads back WHATEVER is on screen, so it must own a clean region:
//     we confine it to the play area BETWEEN the HUD bars (HUD_T..HUD_B) and CLAMP every
//     sample into that box. Skip this and the HUD text/bars (drawn on top each frame) get
//     read back and smeared into dripping curtains, and off-screen samples feed black in
//     from the edges — both were the early "weirdness".
#define HUD_T 10                       // top HUD bar height
#define HUD_B 9                        // bottom HUD bar height
static void sh_feedback(float t, int ps) {
    int y0 = HUD_T, y1 = SCREEN_H - HUD_B;            // the clean interior, no HUD rows
    float cxp = SCREEN_W * 0.5f, cyp = (y0 + y1) * 0.5f;
    float ca = cos_deg(1.5f), sa = sin_deg(1.5f);     // 1.5°/frame swirl
    float zoom = 1.012f;                               // >1 pulls from further out → zoom-in feel
    float ex = cxp + cos_deg(t * 80.0f) * SCREEN_W * 0.22f;   // auto emitter, orbiting (inside the bright zone)
    float ey = cyp + sin_deg(t * 80.0f) * (y1 - y0) * 0.22f;  // same rate on both axes = a true circle (no self-crossing "shadow" trail)
    for (int sy = y0; sy < y1; sy += ps)
        for (int sx = 0; sx < SCREEN_W; sx += ps) {
            float px = sx + ps * 0.5f, py = sy + ps * 0.5f;
            float dx = px - cxp, dy = py - cyp;
            int rx = (int)(cxp + (dx * ca - dy * sa) * zoom);  // rotated+zoomed source
            int ry = (int)(cyp + (dx * sa + dy * ca) * zoom);
            if (rx < 0) rx = 0; else if (rx >= SCREEN_W) rx = SCREEN_W - 1;   // clamp into the box:
            if (ry < y0) ry = y0; else if (ry >= y1) ry = y1 - 1;            // never read HUD / off-screen
            // radial vignette: fade the decay toward black near the edges so the rectangular
            // box boundary can't light up — without it, clamped edge pixels spiral inward as
            // a rotating rectangle. with it the attractor is a smooth round tunnel.
            float nx = (px - cxp) / (SCREEN_W * 0.5f), ny = (py - cyp) / ((y1 - y0) * 0.5f);
            float vig = clamp((1.05f - fsqrt(nx * nx + ny * ny)) * 2.5f, 0, 1);  // full-bright interior, hard fade in the outer ring
            int c = scale_rgb(pget_rgb(rx, ry), 0.95f * vig); // ← READ LAST FRAME BACK, then vignetted trail-decay
            float d = fsqrt((px - ex) * (px - ex) + (py - ey) * (py - ey));
            if (d < 16) { float b = clamp(1 - d / 16, 0, 1); b *= b; c = cadd(c, scale_rgb(hsv(t * 0.15f, 0.8f, 1), b)); }
            if (mdown) {
                float md = fsqrt((px - mu * SCREEN_W) * (px - mu * SCREEN_W) + (py - mv * SCREEN_H) * (py - mv * SCREEN_H));
                if (md < 18) { float b = clamp(1 - md / 18, 0, 1); b *= b; c = cadd(c, scale_rgb(0xFFFFFF, b)); }
            }
            int bw = (sx + ps <= SCREEN_W) ? ps : SCREEN_W - sx;
            int bh = (sy + ps <= y1) ? ps : y1 - sy;
            rectfill_rgb(sx, sy, bw, bh, c);
        }
}

typedef int (*Shader)(float, float, float);
static Shader SHADERS[] = { sh_uv, sh_sweep, sh_sdf, sh_plasma, sh_noise, sh_check, sh_interf,
                            sh_light, sh_ripple };
static const char *NAMES[] = { "uv coords", "hue sweep", "distance field", "plasma",
                               "value noise", "warped checker", "interference",
                               "cursor light", "cursor ripple", "feedback" };
static const char *NOTE[]  = { "red=x  green=y", "hue = x + time", "d = dist to centre",
                               "sum of sines", "noise2(uv + t)", "bend uv, then tile",
                               "two wells, summed", "dist to MOUSE (click)", "rings from MOUSE",
                               "pget_rgb: read-back! HOLD mouse to paint" };
#define NSHADER 10
#define SH_FEEDBACK 9    // the one shader that reads the canvas back — handled apart from SHADERS[]

static int   cur;        // which shader
static int   ps = 3;     // pixel size — bigger = chunkier + faster (fewer evals)
static float t;          // shader time (seconds)
static bool  frozen;

void init(void) { cur = 0; ps = 3; t = 0; }

void update(void) {
    mu = (float)mouse_x() / SCREEN_W;        // refresh the uniforms from the pointer
    mv = (float)mouse_y() / SCREEN_H;
    mdown = mouse_down(MOUSE_LEFT);
    if (!frozen) t += dt();
    if (keyp(KEY_SPACE)) frozen = !frozen;
    if (keyp(KEY_RIGHT)) cur = (cur + 1) % NSHADER;
    if (keyp(KEY_LEFT))  cur = (cur + NSHADER - 1) % NSHADER;
    for (int k = 0; k < NSHADER; k++) if (keyp('1' + k)) cur = k;
    if (keyp(KEY_UP)   && ps > 1) ps--;
    if (keyp(KEY_DOWN) && ps < 10) ps++;
#ifdef DE_TRACE
    watch("shader", "%d %s ps=%d", cur, NAMES[cur], ps);
#endif
}

void draw(void) {
    if (cur == SH_FEEDBACK) {
        sh_feedback(t, ps);                  // reads the previous frame back — its own branch
    } else {
        Shader sh = SHADERS[cur];
        // evaluate one fragment per ps×ps cell, then paint the cell in ONE true-colour
        // call — rectfill_rgb beats a pset_rgb pixel-loop as the cells get chunky.
        for (int sy = 0; sy < SCREEN_H; sy += ps)
            for (int sx = 0; sx < SCREEN_W; sx += ps) {
                float u = (sx + ps * 0.5f) / SCREEN_W;
                float v = (sy + ps * 0.5f) / SCREEN_H;
                int bw = (sx + ps <= SCREEN_W) ? ps : SCREEN_W - sx;
                int bh = (sy + ps <= SCREEN_H) ? ps : SCREEN_H - sy;
                rectfill_rgb(sx, sy, bw, bh, sh(u, v, t));
            }
    }

    // a crosshair on the mouse for the cursor-driven shaders (NOT feedback — it reads the
    // canvas back, so a crosshair drawn here would smear into the flow as a ghost trail)
    if (cur == 7 || cur == 8) {
        int cx = mouse_x(), cy = mouse_y();
        line(cx - 4, cy, cx + 4, cy, CLR_WHITE);
        line(cx, cy - 4, cx, cy + 4, CLR_WHITE);
    }

    // HUD (normal palette colours — pset_rgb didn't touch the palette)
    rectfill(0, 0, SCREEN_W, 10, CLR_BLACK);
    print(str("%d/%d  %s", cur + 1, NSHADER, NAMES[cur]), 4, 2, CLR_WHITE);
    print_right(NOTE[cur], SCREEN_W - 4, 2, CLR_YELLOW);
    rectfill(0, SCREEN_H - 9, SCREEN_W, 9, CLR_BLACK);
    print(str("<-/->  shader     up/dn  pixel %d     space  %s", ps, frozen ? "frozen" : "live"),
          4, SCREEN_H - 7, CLR_LIGHT_GREY);
}
