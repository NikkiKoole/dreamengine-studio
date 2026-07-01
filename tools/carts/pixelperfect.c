/* de:meta
{
  "title": "Pixel-Perfect Scaling",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "software-rasterizer"
  ],
  "lineage": "Interactive side-by-side comparison of four texture-sampling kernels (nearest / bilinear / sharp-bilinear / gamma-correct sharp-bilinear) written in CPU pset_rgb; reproduces what a GPU fragment shader does at zero engine cost.",
  "description": "How do you scale thin pixel-art lines by a FRACTIONAL (float) factor without the lines wobbling/crawling as they move - and without blurring them to mush? This draws one world of thin 1px lines scaled the SAME way four times, side by side, each with a 7x MAGNIFIER below so the per-pixel structure is unmistakable: NEAREST (uneven pixel widths, hard edges crawl/shimmer as it scrolls), BILINEAR (no wobble but every edge is a wide soft ramp - blurry), SHARP (sharp-bilinear: each source pixel stays a FLAT colour, the blend confined to a 1-output-pixel seam at texel edges - crisp like nearest, stable like bilinear, single pass, the answer), and SHARP+g (same but the seam is blended in LINEAR LIGHT / gamma-correct, so a moving bright line keeps its true brightness instead of pulsing dark each sub-pixel step). Watch the magnifiers while it scrolls: nearest's bright core jumps 2px<->3px, bilinear fades over several blocks, sharp holds a flat core with one soft seam-block sliding smoothly. All CPU here via pset_rgb, reproducing exactly what a GPU blit does (which is one effectively-free shader pass), so the comparison is faithful with zero engine changes. See docs/design/pixel-perfect-scaling.md (this filter comparison) and docs/design/window-fill-scaling.md (the other, unbuilt half of the original idea: filling a resizable/web/fullscreen window fractionally instead of locking to the last integer scale step). Controls: LEFT/RIGHT arrows change the scale (0.05 steps), Z toggles auto-scroll (on by default - scrolling animates the wobble), X toggles a slow scale 'breathe'."
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// PIXEL-PERFECT SCALING DEMO — how do you scale thin pixel-art lines by a
// FRACTIONAL factor without the lines wobbling/crawling as they move, and
// WITHOUT blurring them to mush? Four approaches, same world, same scale, side
// by side, all scrolling so you can SEE it — plus a 7x MAGNIFIER under each so
// the per-pixel structure is unmistakable:
//
//   NEAREST   — uneven pixel widths; hard edges crawl/shimmer as it scrolls.
//   BILINEAR  — no wobble, but every edge is a wide soft ramp. blurry. too much.
//   SHARP     — "sharp bilinear": each source pixel stays a FLAT colour, the
//               blend confined to a 1-output-pixel seam at texel edges. crisp
//               like nearest, stable like bilinear. single pass. the answer.
//   SHARP+g   — sharp bilinear, but the seam is blended in LINEAR LIGHT (gamma
//               correct). a half-covered pixel keeps its true brightness, so a
//               moving bright line stops "pulsing" dark at every sub-pixel step.
//
// Watch the magnifiers while it scrolls: NEAREST's bright core jumps 2px<->3px
// (the crawl); BILINEAR fades over several blocks (the blur); SHARP holds a
// flat core with ONE soft seam-block that slides smoothly (crisp AND stable).
//
// All CPU here (pset_rgb), reproducing exactly what a GPU blit does — faithful,
// zero engine changes. On a GPU this whole thing is ONE fragment-shader pass,
// effectively free; the ~3-4ms CPU cost here is just the per-pixel pset_rgb.
// See docs/design/pixel-perfect-scaling.md — and docs/design/window-fill-scaling.md
// for the other half of the original idea (fractional window-fill) that never
// got wired into the engine's present blit; a natural home for a future 5th
// comparison column here (integer-locked vs. fill-the-window).
//
//   ← / →   scale down / up (0.05 steps)
//   Z       toggle auto-scroll (on by default — scrolling animates the wobble)
//   X       toggle scale "breathe" (slowly oscillate the scale)

// ---- the world: thin 1px line art, sampled at integer source coords --------
static int world_px(int sx, int sy) {
    if (sx < 0) sx = -sx;            // mirror into the positive quadrant so a
    if (sy < 0) sy = -sy;            // panning offset never falls off the edge
    int c = 0x0e141c;                // dark background
    if ((sy % 7) == 0)        c = 0x2c4a66;   // horizontal lines
    if (((sx + sy) % 11) == 0) c = 0x3a7a48;  // diagonal lines
    if ((sx % 4) == 0)        c = 0xc8d8ec;   // vertical lines (brightest, on top)
    return c;
}

// ---- 24-bit colour helpers --------------------------------------------------
static float flerp(float a, float b, float t) { return a + (b - a) * t; }
static int   clampi(int v)                     { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// plain bilinear blend of 4 corners, done in (gamma) sRGB space.
static int blend4(int c00, int c10, int c01, int c11, float fx, float fy) {
    int r = (int)(flerp(flerp((c00>>16)&255, (c10>>16)&255, fx),
                        flerp((c01>>16)&255, (c11>>16)&255, fx), fy) + 0.5f);
    int g = (int)(flerp(flerp((c00>>8)&255,  (c10>>8)&255,  fx),
                        flerp((c01>>8)&255,  (c11>>8)&255,  fx), fy) + 0.5f);
    int b = (int)(flerp(flerp(c00&255,        c10&255,       fx),
                        flerp(c01&255,        c11&255,       fx), fy) + 0.5f);
    return (clampi(r)<<16) | (clampi(g)<<8) | clampi(b);
}

// gamma-correct bilinear: convert to LINEAR light, blend, convert back. keeps
// a half-covered bright pixel at its true perceived brightness (no dark pulse).
static float s2l[256];                 // sRGB byte -> linear light (LUT)
static int   lut_ready = 0;
static void ensure_lut(void) {
    if (lut_ready) return;
    for (int i = 0; i < 256; i++) s2l[i] = powf(i / 255.0f, 2.2f);
    lut_ready = 1;
}
static int chan_lin(int c00, int c10, int c01, int c11, int sh, float fx, float fy) {
    float v = flerp(flerp(s2l[(c00>>sh)&255], s2l[(c10>>sh)&255], fx),
                    flerp(s2l[(c01>>sh)&255], s2l[(c11>>sh)&255], fx), fy);
    return clampi((int)(powf(v, 1.0f/2.2f) * 255.0f + 0.5f));   // back to sRGB
}
static int blend4_lin(int c00, int c10, int c01, int c11, float fx, float fy) {
    return (chan_lin(c00,c10,c01,c11,16,fx,fy)<<16)
         | (chan_lin(c00,c10,c01,c11, 8,fx,fy)<<8)
         |  chan_lin(c00,c10,c01,c11, 0,fx,fy);
}

static float scale   = 2.6f;   // deliberately fractional
static float ox      = 0.0f;   // pan offset in SOURCE pixels (sub-pixel = shimmer)
static int   subpan = 1;
static int   breathe = 0;
static float t       = 0.0f;

// sample the scaled image at source coord (su,sv). mode: 0 nearest 1 bilinear
// 2 sharp 3 sharp+gamma. this is THE per-pixel kernel, shared by the panels and
// the magnifiers so they show the exact same thing.
static int sample(float su, float sv, int mode) {
    float s = scale;
    int x0 = (int)floorf(su), y0 = (int)floorf(sv);
    if (mode == 0) return world_px(x0, y0);                 // nearest
    int a = world_px(x0, y0),     b = world_px(x0 + 1, y0);
    int c = world_px(x0, y0 + 1), d = world_px(x0 + 1, y0 + 1);
    if (mode == 1) return blend4(a, b, c, d, su - x0, sv - y0);   // bilinear
    // sharp: remap within-texel fraction to flat-except-a-1-output-pixel seam
    float fu = (su - x0 - 0.5f) * s + 0.5f; fu = fu < 0 ? 0 : (fu > 1 ? 1 : fu);
    float fv = (sv - y0 - 0.5f) * s + 0.5f; fv = fv < 0 ? 0 : (fv > 1 ? 1 : fv);
    return (mode == 2) ? blend4(a, b, c, d, fu, fv) : blend4_lin(a, b, c, d, fu, fv);
}

// ---- viewport geometry ------------------------------------------------------
#define NPANEL 4
#define VW 72        // viewport width  (a fixed window onto the zooming world)
#define VH 88        // viewport height
#define VY 14        // viewport top
#define LN 9         // magnifier samples a LN x LN block of the panel...
#define LZ 7         // ...drawn at LZ pixels each
#define LDX (VW/2 - LN/2)   // which panel block the magnifier shows (centred)
#define LDY (VH/2 - LN/2)

static void draw_view(int px, int mode) {
    float s = scale;
    for (int dy = 0; dy < VH; dy++)
        for (int dx = 0; dx < VW; dx++)
            pset_rgb(px + dx, VY + dy, sample(ox + dx / s, dy / s, mode));
}

void update(void) {
    t += 1.0f / 60.0f;
    if (subpan) ox += 0.50f;            // continuous sub-pixel scroll → shimmer
    if (ox > 100000.0f) ox -= 100000.0f;
    if (breathe) scale = 2.6f + 0.7f * sinf(t * 0.7f);

    if (keyp(KEY_LEFT)  || btnp(0, BTN_LEFT))  { scale -= 0.05f; breathe = 0; }
    if (keyp(KEY_RIGHT) || btnp(0, BTN_RIGHT)) { scale += 0.05f; breathe = 0; }
    if (scale < 1.2f) scale = 1.2f;
    if (scale > 4.0f) scale = 4.0f;
    if (keyp('Z') || btnp(0, BTN_A)) subpan = !subpan;
    if (keyp('X') || btnp(0, BTN_B)) breathe = !breathe;
}

void draw(void) {
    ensure_lut();
    cls(CLR_BLACK);

    float s = scale;
    int xs[NPANEL]   = { 4, 82, 160, 238 };
    const char *labels[NPANEL] = { "NEAREST", "BILINEAR", "SHARP", "SHARP+g" };
    int lcol[NPANEL] = { CLR_RED, CLR_ORANGE, CLR_GREEN, CLR_LIME_GREEN };

    font(FONT_NORMAL);
    print("PIXEL-PERFECT  -  wobble vs blur", 4, 3, CLR_LIGHT_GREY);

    int loupe_y = VY + VH + 18;
    for (int i = 0; i < NPANEL; i++) {
        int px = xs[i];
        draw_view(px, i);
        rect(px - 1, VY - 1, VW + 2, VH + 2, CLR_DARK_GREY);
        // box on the panel marking what the magnifier below is showing
        rect(px + LDX - 1, VY + LDY - 1, LN + 2, LN + 2, CLR_YELLOW);

        // the magnifier: same block, same algorithm, LZ pixels per sample
        int lx0 = px + (VW - LN * LZ) / 2;
        for (int ly = 0; ly < LN; ly++)
            for (int lx = 0; lx < LN; lx++)
                rectfill_rgb(lx0 + lx * LZ, loupe_y + ly * LZ, LZ, LZ,
                             sample(ox + (LDX + lx) / s, (LDY + ly) / s, i));
        rect(lx0 - 1, loupe_y - 1, LN * LZ + 2, LN * LZ + 2, CLR_YELLOW);

        font(FONT_SMALL);
        print(labels[i], px, VY + VH + 6, lcol[i]);
        print("7x", lx0 + LN * LZ - 9, loupe_y - 8, CLR_DARK_GREY);
    }

    font(FONT_SMALL);
    char buf[80];
    snprintf(buf, sizeof buf, "scale %.2f   (the magnifiers show the same block in each)", s);
    print(buf, 4, loupe_y + LN * LZ + 4, CLR_MEDIUM_GREY);
    print("<-/-> scale    Z scroll    X breathe", 4, loupe_y + LN * LZ + 12, CLR_MEDIUM_GREY);
}
