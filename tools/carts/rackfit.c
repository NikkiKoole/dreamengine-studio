/* de:meta
{
  "title": "rackfit",
  "status": "active",
  "created": "2026-07-03",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "The Phase-0 proof for device-adaptive-layout.md: one music rack (a knob bank, a 16-step strip, a keybed) laid out for iPhone AND iPad, both orientations, against a FAKE device screen - before any of it touches the engine. The point respond.c doesn't make: the driver is PHYSICAL finger size, not aspect ratio. Every control is sized to a real 44pt touch target, so 'how many fit' falls out of the device's points-per-finger (iPhone ~390pt wide = few fat controls; iPad ~1194pt = many precise ones). The arrangement is almost entirely EMERGENT - lay_wrap flows the knobs and steps into as many columns as finger-sized cells fit, and the keybed shows as many finger-wide octaves as fit - so the phone gets 2-3 knob rows + 1 octave and the iPad gets one dense row + 3 octaves from the SAME code, no per-device layout branch. Presets auto-cycle (iPhone/iPad x portrait/landscape); keys 1-4 lock one. If this looks right here, the lay_* helpers graduate to runtime/lay.h and take the real device rect + backing scale instead of this fake one."
}
de:meta */
// RACKFIT — Phase-0 proof for docs/design/device-adaptive-layout.md.
//
// respond.c proved the lay_* primitives reflow. This proves the DESIGN CLAIM
// that doc rests on: iPhone and iPad want different layouts because a fingertip
// (~9mm ≈ 44pt) covers a different FRACTION of each screen — not because the
// aspect ratio differs. So we size every control to a physical 44pt target and
// let the arrangement be EMERGENT: lay_wrap() flows the knobs / steps into as
// many finger-sized columns as fit, and the keybed fills with as many
// finger-wide octaves as fit. Same code → phone shows few big controls in more
// rows, iPad shows many precise controls in one row. No per-device layout.
//
// The "device" is faked: a preset gives logical POINTS (390×844 iPhone, etc.)
// scaled to fit our canvas; a 44pt finger becomes 44×scale px, so the
// finger-to-screen RATIO is physically honest. Graduation swaps the fake rect +
// scale for the real ones the iOS layer already knows (device-adaptive-layout.md
// §engine plumbing #1–#2). The API doesn't change; only what you pass in does.
//
// The chrome (title bar, keybed) is DOCKED with lay_split (fixed band off an
// edge, remainder is the body); the notch/home-bar is a per-side lay_pad (an
// ASYMMETRIC safe-area, which uniform inset can't express); and the multi-cart
// "back to root" chip reserves just a top CORNER (not the whole edge — phone
// height is precious), with the title text padded off that side so nothing
// collides. In the real app the shell draws that chip + hands the cart the
// keep-out rect; here we fake all three from the preset.
//
//   Presets auto-cycle every ~2s. Keys 1/2/3/4 lock iPhone-P / iPhone-L /
//   iPad-P / iPad-L. Watch the SAME rack re-flow to fit the finger.

#include "studio.h"
#include <math.h>

// ───────────────────────── lay: the candidate primitive set (== respond.c) ───
// Copied verbatim from respond.c — these are the runtime/lay.h candidates. Box
// in → Box out, immediate-mode, called every frame.
typedef struct { float x, y, w, h; } Box;
static Box box(float x, float y, float w, float h) { Box b = {x, y, w, h}; return b; }
static float lay_clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float lay_fluid(float pct, float container, float lo, float hi) { return lay_clamp(pct * container, lo, hi); }
// pad(): per-side inset — CSS padding (top, right, bottom, left). Per-side is
// what an ASYMMETRIC safe-area needs (a notch tops one edge, the home-bar the
// opposite). Uniform inset() is just the m,m,m,m case.
static Box lay_pad(Box c, float t, float r, float b, float l) { return box(c.x + l, c.y + t, c.w - l - r, c.h - t - b); }
static Box lay_inset(Box c, float m) { return lay_pad(c, m, m, m, m); }

enum { L_TL, L_T, L_TR, L_L, L_C, L_R, L_BL, L_B, L_BR };
static Box lay_at(Box c, int anchor, float w, float h, float inset) {
    int col = anchor % 3, row = anchor / 3;
    float x = col == 0 ? c.x + inset : col == 1 ? c.x + (c.w - w) / 2 : c.x + c.w - w - inset;
    float y = row == 0 ? c.y + inset : row == 1 ? c.y + (c.h - h) / 2 : c.y + c.h - h - inset;
    return box(x, y, w, h);
}
// split(): dock a fixed-size BAND off one edge and return it; the REMAINDER is
// written to *rest (NULL to ignore). CSS flex fixed-basis + a flex:1 sibling —
// the app-chrome workhorse (title / tab / tool bars, keybed). `size` is fixed px
// OR a fraction of the container.
enum { EDGE_TOP, EDGE_BOTTOM, EDGE_LEFT, EDGE_RIGHT };
static Box lay_split(Box c, int edge, float size, Box *rest) {
    Box band = c, rem = c;
    switch (edge) {
        case EDGE_TOP:    band.h = size;                        rem.y += size; rem.h -= size; break;
        case EDGE_BOTTOM: band.y += c.h - size; band.h = size;                rem.h -= size; break;
        case EDGE_LEFT:   band.w = size;                        rem.x += size; rem.w -= size; break;
        case EDGE_RIGHT:  band.x += c.w - size; band.w = size;                rem.w -= size; break;
    }
    if (rest) *rest = rem;
    return band;
}

// wrap(): i-th of n items in an auto-flowing grid — as many ~minItem-wide columns
// as fit in c.w, wrapping to rows. This is the primitive that makes the layout
// EMERGENT: feed it a finger-sized minItem and the column count IS the density.
static Box lay_wrap(Box c, int n, int i, float minItem, float gap) {
    int cols = (int)((c.w + gap) / (minItem + gap));
    if (cols < 1) cols = 1;
    if (cols > n) cols = n;
    int rows = (n + cols - 1) / cols;
    float cw = (c.w - gap * (cols - 1)) / cols;
    float ch = (c.h - gap * (rows - 1)) / rows;
    int cx = i % cols, cy = i / cols;
    return box(c.x + cx * (cw + gap), c.y + cy * (ch + gap), cw, ch);
}
static int wrap_cols(Box c, int n, float minItem, float gap) {   // for reporting
    int cols = (int)((c.w + gap) / (minItem + gap));
    if (cols < 1) cols = 1; if (cols > n) cols = n; return cols;
}

// ───────────────────────── drawing sugar ─────────────────────────────────────
static void boxfill(Box b, int c) { rectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, c); }
static void boxrect(Box b, int c) { rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, c); }

// ───────────────────────── the fake device ───────────────────────────────────
// Logical POINTS per device (the physical size the OS reports, roughly device-
// independent of pixel density — a 44pt target is ~9mm on all of them).
typedef struct { const char *name; float wpt, hpt; } Device;
static Device DEVS[4] = {
    { "iPhone \x18",  390, 844 },   // portrait  (\x18 = up arrow glyph in dos font)
    { "iPhone \x1a",  844, 390 },   // landscape
    { "iPad \x18",    834, 1194 },
    { "iPad \x1a",   1194, 834 },
};
#define FINGER_PT 44.0f            // Apple HIG minimum comfortable touch target
#define TABLET_PT 700.0f           // min-dimension >= this ⇒ tablet-class density

static int  tick = 0;
static int  locked = -1;           // -1 = auto-cycle; else a pinned preset

void update(void) {
    tick++;
    for (int k = 0; k < 4; k++) if (keyp('1' + k)) locked = k;
    if (keyp('0')) locked = -1;    // back to auto-cycle
}

// draw a rotary knob: body + tick pointer (angle just varies by index so it
// reads as a real panel, nothing is wired).
static void knob(Box cell, int i) {
    float r = lay_clamp((cell.w < cell.h ? cell.w : cell.h) * 0.42f, 3, 60);
    float cx = cell.x + cell.w / 2, cy = cell.y + cell.h * 0.42f;
    circfill((int)cx, (int)cy, (int)r, CLR_MEDIUM_GREY);
    circ((int)cx, (int)cy, (int)r, CLR_DARK_GREY);
    float a = 2.3f + i * 0.7f;
    line((int)cx, (int)cy, (int)(cx + r * 0.85f * cosf(a)), (int)(cy + r * 0.85f * sinf(a)), CLR_ORANGE);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    int preset = locked >= 0 ? locked : (tick / 120) % 4;
    Device d = DEVS[preset];

    // ---- header on the REAL screen (not part of the rack) -------------------
    font(FONT_SMALL);
    print("RACKFIT - one rack, sized to the FINGER (device-adaptive-layout.md Phase 0)", 4, 3, CLR_LIGHT_GREY);

    // ---- fit the device rect into the canvas, preserving its aspect ---------
    float availX = 6, availY = 26, availW = SCREEN_W - 12, availH = SCREEN_H - 34;
    float scale = availW / d.wpt; if (d.hpt * scale > availH) scale = availH / d.hpt;
    float dw = d.wpt * scale, dh = d.hpt * scale;
    Box dev = box(availX + (availW - dw) / 2, availY + (availH - dh) / 2, dw, dh);

    float fu = FINGER_PT * scale;                     // one finger, in canvas px
    bool tablet = (d.wpt < d.hpt ? d.wpt : d.hpt) >= TABLET_PT;
    bool portrait = d.hpt > d.wpt;

    // bezel + screen
    boxfill(lay_inset(dev, -3), CLR_DARK_GREY);
    boxfill(dev, CLR_DARKER_BLUE);

    // SAFE-AREA (faked): the notch/home-bar inset is ASYMMETRIC — this is why a
    // per-side lay_pad() exists and a uniform lay_inset() won't do. Portrait: a
    // notch tops it + a home-bar strip on the bottom; landscape: the notch moves
    // to a side. (Graduated, these numbers come from the real safe_rect().)
    float bez = lay_fluid(0.02f, dev.w, 2, 6);
    Box screen = portrait ? lay_pad(dev, bez + fu * 0.35f, bez, bez + fu * 0.25f, bez)
                          : lay_pad(dev, bez, bez + fu * 0.45f, bez, bez + fu * 0.15f);

    // ---- the rack: DOCK the chrome bands, the remainder is the body ---------
    // lay_split docks a fixed band off an edge and hands back the rest — the
    // title/keybed/body carve that used to be hand-rolled arithmetic.
    float gap = lay_clamp(fu * 0.12f, 1, 6);
    float titleH = lay_clamp(fu * 0.5f, 7, 22);
    float keyH   = lay_clamp(screen.h * (portrait ? 0.30f : 0.34f), fu * 1.1f, fu * 2.2f);
    Box body;
    Box title = lay_split(screen, EDGE_TOP,    titleH, &body);   // title off the top
    Box kbed  = lay_split(body,   EDGE_BOTTOM, keyH,   &body);   // keybed off the bottom
    Box mid   = lay_pad(body, gap, 0, gap, 0);                   // breathing room top/bottom
    Box stepsA;
    Box knobsA = lay_split(mid, EDGE_TOP, mid.h * 0.56f, &stepsA);
    knobsA = lay_pad(knobsA, 0, 0, gap * 0.5f, 0);
    stepsA = lay_pad(stepsA, gap * 0.5f, 0, 0, 0);

    // BACK-TO-ROOT keep-out: the multi-cart shell owns a home chip in a top
    // corner (tl/tr). A rack must not put a control under it — but reserving the
    // whole top edge wastes precious phone height, so we keep just the CORNER: a
    // square in the title band, and pad the TITLE TEXT off that side so nothing
    // collides. (Graduated, the shell hands the cart this rect; see notes.)
    float homeSz = lay_clamp(fu * 0.8f, 8, 40);   // finger-sized tap target
    Box home = lay_at(title, L_TR, homeSz, homeSz, 1);
    Box titleTxt = lay_pad(title, 0, homeSz + 2, 0, 0);   // title text avoids the corner

    // title
    boxfill(title, CLR_TRUE_BLUE);
    font(FONT_SMALL);
    print("ACID RACK", (int)titleTxt.x + 3, (int)(titleTxt.y + (titleTxt.h - 6) / 2), CLR_LIGHT_PEACH);
    // the reserved back-to-root chip (drawn by the shell in the real app)
    boxfill(home, CLR_DARKER_BLUE);
    boxrect(home, CLR_LIGHT_GREY);
    print_centered("<", (int)(home.x + home.w / 2), (int)(home.y + (home.h - 6) / 2), CLR_LIGHT_PEACH);

    // 8 knobs — EMERGENT columns: lay_wrap flows them into as many cells as fit.
    // A comfortable knob is ~60pt (bigger than the 44pt min), so on the iPad all
    // 8 sit in one row (~40% of width) but on the iPhone they overflow and wrap
    // to 2 rows — the density reflow, with zero per-device branch.
    int NKN = 8; float knMin = fu * 1.4f;
    int kncols = wrap_cols(knobsA, NKN, knMin, gap);
    for (int i = 0; i < NKN; i++) {
        Box cell = lay_wrap(knobsA, NKN, i, knMin, gap);
        if (cell.w < 4 || cell.h < 4) continue;
        knob(cell, i);
    }

    // 16 steps — half-finger cells (a step is a precision target, not a big pad),
    // so iPad shows one row of 16, iPhone wraps to 2 rows of 8. Emergent again.
    int NST = 16; float stMin = fu * 0.5f;
    for (int i = 0; i < NST; i++) {
        Box s = lay_wrap(stepsA, NST, i, stMin, gap * 0.6f);
        if (s.w < 2 || s.h < 2) continue;
        int on = (i % 4 == 0) || (i == 6) || (i == 11);
        boxfill(lay_inset(s, 0.5f), on ? CLR_LIME_GREEN : CLR_DARKER_PURPLE);
        boxrect(s, CLR_DARKER_BLUE);
    }

    // keybed — as many finger-wide WHITE keys as fit, rounded to whole octaves.
    // iPad landscape (wide, small fu-ratio) → 3 octaves; iPhone portrait → 1.
    float wkw = lay_clamp(fu * 0.62f, 4, 40);
    int whites = (int)(kbed.w / wkw); if (whites < 7) whites = 7;
    int octaves = whites / 7; if (octaves < 1) octaves = 1; if (octaves > 3) octaves = 3;
    whites = octaves * 7;
    wkw = kbed.w / whites;
    for (int i = 0; i < whites; i++) {
        Box wk = box(kbed.x + i * wkw, kbed.y, wkw, kbed.h);
        boxfill(lay_inset(wk, 0.5f), CLR_LIGHT_PEACH);
    }
    // black keys sit between whites at scale positions 0,1,3,4,5 within an octave
    int blk[5] = {0, 1, 3, 4, 5};
    float bkw = wkw * 0.6f, bkh = kbed.h * 0.6f;
    for (int o = 0; o < octaves; o++)
        for (int j = 0; j < 5; j++) {
            float wx = kbed.x + (o * 7 + blk[j] + 1) * wkw;
            boxfill(box(wx - bkw / 2, kbed.y, bkw, bkh), CLR_BROWNISH_BLACK);
        }

    // ---- readout: WHY the layout differs (the physical proof) ---------------
    font(FONT_TINY);
    int cls_line_y = SCREEN_H - 6;
    print(str("%s  %dx%dpt   %s   finger 44pt=%dpx (%.0f%% of width)   knobs:%d cols  keys:%d oct",
              d.name, (int)d.wpt, (int)d.hpt, tablet ? "TABLET" : "PHONE",
              (int)fu, 100.0f * FINGER_PT / d.wpt, kncols, octaves),
          4, cls_line_y, tablet ? CLR_GREEN : CLR_ORANGE);
    print_right(locked >= 0 ? "[locked - press 0 to auto-cycle]" : "[auto-cycle - keys 1-4 lock]",
                SCREEN_W - 4, 3, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
