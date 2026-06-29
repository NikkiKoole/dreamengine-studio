/* de:meta
{
  "title": "caustics",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "fluid-sim",
    "dithering-gradient"
  ],
  "lineage": "Follows shadelab (CPU shader exploration) and precedes raymarch; adds Hugo Elias's classic demoscene 2D wave-equation height-field sim alongside a sine-only fake mode, teaching both paths to displacement mapping.",
  "description": "Water light effects and a two-part lesson in DISPLACEMENT MAPPING: warp WHERE you read a colour and the picture appears to ripple. Press TAB to flip between the same idea done two ways. FAKED (the N64 way): the displacement is a few scrolling SINE waves computed on the fly over a procedural pool floor - no memory, no physics, basically free, exactly how Mario 64 and Ocarina of Time faked moving water on a GPU that could not run a shader. REAL (displacement mapping proper): the displacement now comes from a real HEIGHT-FIELD WATER SIMULATION - two ping-pong buffers holding the surface height, each cell becoming the average of its neighbours minus its previous value times a little damping (the wave equation, the previous frame feeding the next is the feedback), and the surface SLOPE steers where each pixel LOOKS UP A REAL CAPTURED IMAGE (src[]); drop a stone and the rings expand, reflect off the walls, cross and interfere, and fade - the classic demoscene 2D water (Hugo Elias). Same umbrella technique, two ways to make the map: a formula, or a simulation you can poke. Everything painted in 24-bit true colour through pset_rgb / rectfill_rgb. Three floor textures / source images: pool tiles, a sandy tropical bottom, and a high-contrast checkerboard whose straight lines visibly BEND so you see the distortion itself. TAB switch FAKED/REAL, drag the mouse to push the water, CLICK to drop a stone, C toggles caustics (the light web), W toggles waves (FAKED), UP/DOWN strength, [ ] pixel size, SPACE freezes, 1/2/3 or LEFT/RIGHT switch floor. Companion to shadelab."
}
de:meta */
#include "studio.h"

// CAUSTICS — "water light effects", and a two-part lesson in DISPLACEMENT MAPPING:
// warp WHERE you read a colour, and the picture appears to ripple. The cart shows the
// SAME idea done two ways, so you can feel the difference — press TAB to flip:
//
//   ▸ FAKED  (the N64 way) — the displacement is a few scrolling SINE waves computed on
//     the fly, and the "floor" is a procedural function. No memory, no physics, basically
//     free. This is how Mario 64 / Ocarina of Time faked moving water on hardware that
//     couldn't run a shader. Cheap and convincing — but every ripple is the same wobble.
//
//   ▸ REAL  (displacement mapping proper) — now the displacement comes from a real
//     HEIGHT-FIELD WATER SIMULATION: two buffers holding the water's surface height, and
//     each frame every cell becomes the average of its neighbours minus its own previous
//     value, times a little damping — that's the WAVE EQUATION, and the previous frame
//     feeding the next is the FEEDBACK. The surface's slope is the displacement map, and
//     it steers where each pixel LOOKS UP A REAL CAPTURED IMAGE (src[]). Drop a stone and
//     the rings expand, reflect off the walls, cross and interfere, and fade — real waves.
//     (This is the classic demoscene "2D water" — Hugo Elias's tutorial.)
//
// Same umbrella technique, two ways to make the map: a formula, or a simulation you can
// poke. Everything is painted in 24-bit true colour through pset_rgb / rectfill_rgb.
//
//   TAB : FAKED ⇄ REAL          1/2/3 or LEFT/RIGHT : floor / source image
//   C : caustics (the light web)    W : waves on/off (FAKED only — see the still floor)
//   UP/DOWN : strength   [ ] : pixel size   SPACE : freeze
//   MOUSE : drag to push the water    CLICK : drop a stone

// ── colour helpers (a CPU shader's toolkit — same as shadelab) ───────────────────
static int rgb(float r, float g, float b) {                 // 0..1 channels → 0xRRGGBB
    int R = (int)(clamp(r, 0, 1) * 255);
    int G = (int)(clamp(g, 0, 1) * 255);
    int B = (int)(clamp(b, 0, 1) * 255);
    return (R << 16) | (G << 8) | B;
}
static int scale_rgb(int c, float b) {                      // dim a 0xRRGGBB by 0..1
    int R = (int)(((c >> 16) & 0xFF) * b), G = (int)(((c >> 8) & 0xFF) * b), B = (int)((c & 0xFF) * b);
    return (R << 16) | (G << 8) | B;
}
static int cadd(int a, int b) {                             // saturating add of two 0xRRGGBB
    int R = ((a >> 16) & 0xFF) + ((b >> 16) & 0xFF); if (R > 255) R = 255;
    int G = ((a >> 8)  & 0xFF) + ((b >> 8)  & 0xFF); if (G > 255) G = 255;
    int B = (a & 0xFF)         + (b & 0xFF);          if (B > 255) B = 255;
    return (R << 16) | (G << 8) | B;
}
static int cmix(int a, int b, float t) {                    // blend two 0xRRGGBB, t=0→a 1→b
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return rgb((ar + (br - ar) * t) / 255.0f, (ag + (bg - ag) * t) / 255.0f, (ab + (bb - ab) * t) / 255.0f);
}
static float ffabs(float x) { return x < 0 ? -x : x; }
static float fmin2(float a, float b) { return a < b ? a : b; }
static float fract(float x) { return x - (int)x; }

// ── THE FLOOR TEXTURES — pure functions (u,v) → 0xRRGGBB, the pool bottom from above.
//    In FAKED mode they're sampled live at a warped coord; in REAL mode they're rendered
//    ONCE into src[] (the captured "image") and then displaced by the simulation. ──

// 1. POOL TILES — an aqua mosaic with darker grout, like a swimming-pool floor.
static int tex_tiles(float u, float v) {
    const float TS = 7.0f;                                  // tiles across
    float fx = u * TS, fy = v * TS;
    int tx = (int)fx, ty = (int)fy;
    float gx = fract(fx), gy = fract(fy);                   // position within the tile, 0..1
    float edge = fmin2(fmin2(gx, 1 - gx), fmin2(gy, 1 - gy));
    float var = noise2(tx * 1.7f + 0.5f, ty * 1.3f + 0.5f); // per-tile shade jitter
    if (edge < 0.07f) return rgb(0.02f, 0.16f, 0.22f);      // grout line
    return rgb(0.02f, 0.42f + var * 0.18f, 0.52f + var * 0.16f);
}

// 2. SANDY BOTTOM — warm tan with rolling sand ripples + a noise speckle. Tropical lagoon.
static int tex_sand(float u, float v) {
    float ripple = 0.5f + 0.5f * sin_deg(v * 1300.0f + noise2(u * 4.0f, v * 4.0f) * 180.0f);
    float speck  = noise2(u * 40.0f, v * 40.0f);            // grain
    float b = 0.62f + ripple * 0.22f + speck * 0.12f;
    return rgb(b, b * 0.86f, b * 0.6f);                     // sand = warm, low blue
}

// 3. CHECKERBOARD — high contrast so the DISTORTION ITSELF is the star: you see the
//    straight grid lines bend into the water's wobble. The "show me the math" texture.
static int tex_check(float u, float v) {
    int cx = (int)(u * 9) & 1, cy = (int)(v * 9) & 1;
    return (cx ^ cy) ? 0xe6f3ff : 0x0a2740;
}

typedef int (*Tex)(float, float);
static Tex   TEX[]  = { tex_tiles, tex_sand, tex_check };
static const char *TEXNAME[] = { "pool tiles", "sandy bottom", "checker (see the bend)" };
#define NTEX 3

// caustics — the dancing light web, shared by both modes. Two scrolling sine grids
// multiplied (bright knots where both crest), cubed to thin them into sharp filaments.
static float caustic_at(float u, float v, float ti) {
    float a = sin_deg((u * 1200.0f + sin_deg(v * 800.0f + ti * 40.0f) * 200.0f) + ti * 55.0f);
    float b = sin_deg((v * 1400.0f - sin_deg(u * 900.0f - ti * 35.0f) * 200.0f) - ti * 48.0f);
    float c = a * b; if (c < 0) c = 0;
    c = c * c * c;                                          // sharpen to thin bright veins
    return c * (1.0f - v * 0.5f);                           // brighter where it's shallow (top)
}

// ── UNIFORMS refreshed each frame from update() ──────────────────────────────────
static float mu = 0.5f, mv = 0.5f;     // mouse in 0..1
static bool  mdown;

// click-drops (FAKED mode): a stone sends an expanding ring of sine displacement out.
#define NDROP 5
static struct { float u, v, age; bool live; } drop[NDROP];
static int dropi;

// ── REAL displacement-mapping mode: a height-field water sim + a captured source image ──
static int   src[SCREEN_W * SCREEN_H];        // THE IMAGE we look pixels up in (captured once)
static float hbuf[2][SCREEN_W * SCREEN_H];    // two height buffers — the water surface, ping-ponged
static int   hcur;                            // index of the current height buffer
static bool  src_ready;

static int   mode = 0;                 // 0 = FAKED (procedural sine), 1 = REAL (sim + image lookup)
static int   cur  = 0;                 // which floor texture / source image
static int   ps   = 2;                 // pixel size — bigger = chunkier + faster
static float amt  = 1.0f;              // wave / displacement strength (UP/DOWN)
static bool  caus = true, waves = true, frozen;
static float t;                        // shader time (seconds)

// render the chosen floor ONCE into src[] — this is the "image" the REAL mode samples.
static void capture_source(void) {
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            src[y * SCREEN_W + x] = TEX[cur]((x + 0.5f) / SCREEN_W, (y + 0.5f) / SCREEN_H);
    src_ready = true;
}
static void reset_water(void) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) hbuf[0][i] = hbuf[1][i] = 0;
}
// one step of the wave equation: new height = (avg of 4 neighbours) - previous, * damping.
// The PREVIOUS frame's buffer feeds the next — that's the feedback that keeps waves moving.
static void water_step(void) {
    float *b1 = hbuf[hcur], *b2 = hbuf[hcur ^ 1];
    for (int y = 1; y < SCREEN_H - 1; y++)
        for (int x = 1; x < SCREEN_W - 1; x++) {
            int i = y * SCREEN_W + x;
            float nv = (b1[i - 1] + b1[i + 1] + b1[i - SCREEN_W] + b1[i + SCREEN_W]) * 0.5f - b2[i];
            b2[i] = nv * 0.965f;                            // damping → ripples decay and settle
        }
    hcur ^= 1;                                              // the buffer we just wrote is now current
}
// disturb the surface — drop a stone (or drag a finger). This is the only INPUT to the sim.
static void poke(float fu, float fv, float power, int rad) {
    int cx = (int)(fu * SCREEN_W), cy = (int)(fv * SCREEN_H);
    float *h = hbuf[hcur];
    for (int yy = -rad; yy <= rad; yy++)
        for (int xx = -rad; xx <= rad; xx++) {
            int x = cx + xx, y = cy + yy;
            if (x < 1 || x >= SCREEN_W - 1 || y < 1 || y >= SCREEN_H - 1) continue;
            if (xx * xx + yy * yy > rad * rad) continue;
            h[y * SCREEN_W + x] += power;
        }
}
// REAL render: the surface SLOPE at (px,py) is the displacement — it bends where we read
// the captured image. A steep slope toward the light also throws a bright specular glint.
static int sample_displaced(int px, int py) {
    if (px < 1 || px >= SCREEN_W - 1 || py < 1 || py >= SCREEN_H - 1)
        return src[py * SCREEN_W + px];
    float *h = hbuf[hcur];
    int i = py * SCREEN_W + px;
    float gx = h[i - 1] - h[i + 1];                         // horizontal slope → x refraction
    float gy = h[i - SCREEN_W] - h[i + SCREEN_W];           // vertical slope   → y refraction
    int sx = (int)clamp(px + gx * 0.15f * amt, 0, SCREEN_W - 1);
    int sy = (int)clamp(py + gy * 0.15f * amt, 0, SCREEN_H - 1);
    int c = src[sy * SCREEN_W + sx];                        // ← LOOK UP A PIXEL IN THE IMAGE
    float hl = (gx + gy) * 0.012f * amt;                    // lit from the top-left
    if (hl > 0) c = cadd(c, scale_rgb(0xffffff, clamp(hl, 0, 0.7f)));
    if (caus) c = cadd(c, scale_rgb(0xd8ffff, caustic_at((float)px / SCREEN_W, (float)py / SCREEN_H, t) * 0.7f));
    return c;
}

// ── FAKED render: displacement is computed live from scrolling sines (the N64 trick) ──
static int sh_water(float u, float v, float ti) {
    float amp = 0.012f * amt;
    float ou = sin_deg(v * 900.0f  + ti * 70.0f) * amp
             + sin_deg((u + v) * 620.0f - ti * 50.0f) * amp * 0.6f;
    float ov = sin_deg(u * 1000.0f - ti * 77.0f) * amp
             + cos_deg((u - v) * 620.0f + ti * 50.0f) * amp * 0.6f;
    if (mdown) {                                            // hover ring while dragging
        float dx = u - mu, dy = v - mv, d = fsqrt(dx * dx + dy * dy);
        if (d > 0.001f && d < 0.35f) {
            float w = sin_deg(d * 4200.0f - ti * 300.0f) * 0.010f * (1 - d / 0.35f);
            ou += dx / d * w; ov += dy / d * w;
        }
    }
    for (int k = 0; k < NDROP; k++) {                       // expanding click-drop rings
        if (!drop[k].live) continue;
        float dx = u - drop[k].u, dy = v - drop[k].v, d = fsqrt(dx * dx + dy * dy);
        float front = drop[k].age * 0.45f;
        float band  = 1 - clamp(ffabs(d - front) / 0.12f, 0, 1);
        float fade  = clamp(1 - drop[k].age / 2.2f, 0, 1);
        if (band > 0 && d > 0.001f) {
            float w = sin_deg((d - front) * 5000.0f) * 0.022f * band * fade;
            ou += dx / d * w; ov += dy / d * w;
        }
    }
    if (!waves) { ou = 0; ov = 0; }
    int floorc = TEX[cur](u + ou, v + ov);
    floorc = cmix(floorc, scale_rgb(0x0a3a5c, 1.0f), clamp(v * 0.5f, 0, 0.5f));   // depth tint
    if (caus) floorc = cadd(floorc, scale_rgb(0xd8ffff, caustic_at(u, v, ti) * 0.9f));
    return floorc;
}

void init(void) { mode = 0; cur = 0; ps = 2; amt = 1.0f; caus = true; waves = true; t = 0; }

void update(void) {
    mu = (float)mouse_x() / SCREEN_W;
    mv = (float)mouse_y() / SCREEN_H;
    mdown = mouse_down(MOUSE_LEFT);

    if (keyp(KEY_TAB)) {                                    // flip FAKED ⇄ REAL
        mode ^= 1;
        if (mode == 1) { capture_source(); reset_water(); }
    }

    // mouse → disturb the water (mode decides how)
    if (mode == 1) {
        if (mouse_pressed(MOUSE_LEFT))    poke(mu, mv, 360.0f, 4);   // a stone
        else if (mouse_down(MOUSE_LEFT))  poke(mu, mv, 90.0f, 2);    // dragging a finger
    } else if (mouse_pressed(MOUSE_LEFT)) {
        drop[dropi].u = mu; drop[dropi].v = mv; drop[dropi].age = 0; drop[dropi].live = true;
        dropi = (dropi + 1) % NDROP;
    }
    for (int k = 0; k < NDROP; k++)
        if (drop[k].live) { drop[k].age += dt(); if (drop[k].age > 2.2f) drop[k].live = false; }

    if (!frozen) t += dt();
    if (keyp(KEY_SPACE)) frozen = !frozen;

    int pc = cur;                                           // floor / source switch
    if (keyp(KEY_RIGHT)) cur = (cur + 1) % NTEX;
    if (keyp(KEY_LEFT))  cur = (cur + NTEX - 1) % NTEX;
    for (int k = 0; k < NTEX; k++) if (keyp('1' + k)) cur = k;
    if (cur != pc && mode == 1) { capture_source(); reset_water(); }  // recapture the image

    if (keyp('c') || keyp('C')) caus  = !caus;
    if (keyp('w') || keyp('W')) waves = !waves;
    if (keyp(KEY_UP))   amt = clamp(amt + 0.25f, 0, 3.0f);
    if (keyp(KEY_DOWN)) amt = clamp(amt - 0.25f, 0, 3.0f);
    if (keyp(']') && ps < 8) ps++;
    if (keyp('[') && ps > 1) ps--;

    if (mode == 1 && !frozen) water_step();                // advance the wave equation
#ifdef DE_TRACE
    watch("water", "%s %s amt=%.2f caus=%d ps=%d", mode ? "REAL" : "FAKED", TEXNAME[cur], amt, caus, ps);
#endif
}

void draw(void) {
    // evaluate one fragment per ps×ps cell, paint it in one true-colour block.
    for (int sy = 0; sy < SCREEN_H; sy += ps)
        for (int sx = 0; sx < SCREEN_W; sx += ps) {
            int bw = (sx + ps <= SCREEN_W) ? ps : SCREEN_W - sx;
            int bh = (sy + ps <= SCREEN_H) ? ps : SCREEN_H - sy;
            int c;
            if (mode == 1) {
                c = sample_displaced(sx + ps / 2, sy + ps / 2);     // REAL: image lookup via the sim
            } else {
                float u = (sx + ps * 0.5f) / SCREEN_W, v = (sy + ps * 0.5f) / SCREEN_H;
                c = sh_water(u, v, t);                              // FAKED: sine displacement
            }
            rectfill_rgb(sx, sy, bw, bh, c);
        }

    // bright surface glints riding the caustics (FAKED mode — REAL gets specular from slope)
    if (mode == 0 && caus && !frozen)
        for (int i = 0; i < 5; i++) {
            int gx = (int)(fract(sin_deg(i * 97.0f + t * 30.0f) * 0.5f + 0.5f) * SCREEN_W);
            int gy = (int)(fract(cos_deg(i * 131.0f + t * 24.0f) * 0.5f + 0.5f) * SCREEN_H);
            if (blink(8)) pset_rgb(gx, gy, 0xffffff);
        }

    // HUD (normal palette colours — pset_rgb/rectfill_rgb didn't touch the palette)
    rectfill(0, 0, SCREEN_W, 10, CLR_BLACK);
    print(mode ? "REAL: sim + image lookup" : "FAKED: sine displacement",
          4, 2, mode ? CLR_LIME_GREEN : CLR_WHITE);
    print_right("TAB", SCREEN_W - 4, 2, CLR_YELLOW);
    rectfill(0, SCREEN_H - 9, SCREEN_W, 9, CLR_BLACK);
    print(str("floor: %s   C caustics %s   str %.1f   space %s",
              TEXNAME[cur], caus ? "on" : "OFF", amt, frozen ? "frozen" : "live"),
          4, SCREEN_H - 7, CLR_LIGHT_GREY);
}
