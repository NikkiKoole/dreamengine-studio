/* de:meta
{
  "title": "arcs",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "A tour of the arc primitives — arc (part of a circle's rim), arcfill (a filled pie wedge / sector), and ring (a thick radial band). Angles are degrees, 0=right and 90=down like dx/dy. Shows a pie chart, an animated radial cooldown that sweeps and counts %, a chomping pacman, a needle gauge, a fillp-dithered half-dome (fills honor the dither pattern), and a little rainbow of concentric rim arcs. No controls — it just animates."
}
de:meta */
#include "studio.h"

// ARCS — the arc / arcfill / ring primitives: part of a circle's rim, a filled
// pie wedge (sector), and a thick radial band. Angles are degrees, 0 = right,
// 90 = down (the same convention as dx/dy/sin_deg). Everything on screen is one
// of those three calls — plus they honor fillp() dither (see the half-circle).

static float absf(float v) { return v < 0 ? -v : v; }

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("ARC  /  ARCFILL  /  RING", 8, 5, CLR_LIGHT_PEACH);
    float t = now();

    // 1) PIE CHART — four arcfill wedges around a full circle
    int px = 54, py = 60;
    arcfill(px, py, 26,   0, 120, CLR_RED);
    arcfill(px, py, 26, 120, 200, CLR_ORANGE);
    arcfill(px, py, 26, 200, 295, CLR_YELLOW);
    arcfill(px, py, 26, 295, 360, CLR_GREEN);
    circ(px, py, 26, CLR_WHITE);
    print("pie", px - 11, py + 32, CLR_LIGHT_GREY);

    // 2) RADIAL COOLDOWN — a ring that sweeps fill over a 2s loop
    int cx = 160, cy = 60;
    float p = ((int)(t * 1000) % 2000) / 2000.0f;
    ring(cx, cy, 15, 25,  0, 360, CLR_DARKER_GREY);              // empty track
    ring(cx, cy, 15, 25, -90, -90 + 360 * p, CLR_BLUE);         // filling
    print(str("%d%%", (int)(p * 100)), cx - 8, cy - 3, CLR_WHITE);
    print("cooldown", cx - 31, cy + 32, CLR_LIGHT_GREY);

    // 3) PACMAN — arcfill body with an animated mouth on the right
    int mx = 266, my = 60;
    float mouth = 6 + 30 * absf(sin_deg(t * 400));
    arcfill(mx, my, 24, mouth, 360 - mouth, CLR_YELLOW);
    pset(mx, my - 10, CLR_BLACK);                                // eye
    circfill(mx, my - 10, 2, CLR_BLACK);
    print("pacman", mx - 23, my + 32, CLR_LIGHT_GREY);

    // 4) GAUGE — a 240° dial track + a colored value fill + a needle
    int gx = 54, gy = 140;
    float g = 0.5f + 0.5f * sin_deg(t * 90);
    ring(gx, gy, 17, 22, 150, 390, CLR_DARKER_GREY);
    ring(gx, gy, 17, 22, 150, 150 + 240 * g, g > 0.8f ? CLR_RED : CLR_GREEN);
    float na = 150 + 240 * g;
    line(gx, gy, gx + (int)dx(19, na), gy + (int)dy(19, na), CLR_WHITE);
    circfill(gx, gy, 2, CLR_WHITE);
    print("gauge", gx - 19, gy + 30, CLR_LIGHT_GREY);

    // 5) HALF-CIRCLE — a fillp-dithered dome; arc() draws the white rim and caps
    //    the fill exactly (same circle math), so no green pokes past the border.
    int hx = 160, hy = 150;
    fillp(FILL_CHECKER, CLR_DARK_GREEN);
    arcfill(hx, hy, 22, 180, 360, CLR_LIME_GREEN);
    fillp_reset();
    arc(hx, hy, 22, 180, 360, CLR_WHITE);
    line(hx - 22, hy, hx + 22, hy, CLR_WHITE);
    print("half", hx - 15, hy + 10, CLR_LIGHT_GREY);

    // 6) RIM ARCS — concentric arc() outlines, a little rainbow
    int rx = 266, ry = 152;
    int cols[6] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_INDIGO };
    for (int i = 0; i < 6; i++) arc(rx, ry, 9 + i * 3, 180, 360, cols[i]);
    print("rims", rx - 15, ry + 10, CLR_LIGHT_GREY);

    print("degrees: 0=right, 90=down", 8, SCREEN_H - 9, CLR_DARK_GREY);
}
