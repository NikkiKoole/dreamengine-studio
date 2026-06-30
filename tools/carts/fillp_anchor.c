/* de:meta
{
  "title": "24. moving patterns",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "dithering-gradient"
  ],
  "lineage": "Original API demo for fillp_anchor(); shows the crawling-dither artifact that appears when a 4×4 fill lattice is world-pinned under a moving shape, and how anchoring to the shape's own center fixes it.",
  "description": "fillp_anchor(): why a fill pattern 'crawls' under a MOVING shape and how to stop it. By default the 4x4 lattice is pinned to the world, so a shape that drifts just slides across it. Anchor the pattern to each shape's own center and the dither travels WITH it. Nine spinning, bouncing two-tone shapes (polygons + stars), each its own fg/bg color and pattern. Z toggles anchoring off so you can watch the crawl come back.",
  "todo": [
    "Make the button label clickable."
  ]
}
de:meta */
#include "studio.h"

// FILLP_ANCHOR — patterned shapes that carry their dither with them.
//
// A 4x4 fill pattern is normally pinned to the world grid, so a MOVING shape
// just slides across it and the dither seems to "crawl". fillp_anchor() pins the
// pattern to the shape's OWN center instead, so it travels with the shape.
//
// Three groups, so you can read the effect cleanly:
//   • SPINNING polygons/stars  — flavour (the silhouette turns; the lattice stays
//                                 axis-aligned, so spin alone hides the anchoring).
//   • drifting SQUARES         — translate only, axis-aligned: the clearest tell.
//   • TILTED polygons/stars    — a fixed angle, translate only: shows the dither
//                                 glues at any orientation (anchoring is about
//                                 movement, not rotation).
// With anchoring ON the dither sits still inside every translate-only shape;
// press Z and it crawls.
//
//   Z — toggle pattern anchoring on / off

#define NROT  9                      // spinning polygons + stars (flavour)
#define NSQ   4                      // translate-only squares (clear, axis-aligned)
#define NTILT 4                      // fixed-angle, translate-only polys/stars
#define N     (NROT + NSQ + NTILT)

static float sx[N], sy[N], vx[N], vy[N], ang[N], spin[N];
static int   kind[N], sides[N], rad[N], fg[N], bg[N], pat[N];
static bool  anchored = true;
static bool  prev_z   = false;
static bool  ready    = false;

static void setup(void) {
    // a distinct foreground (0-bits) + background (1-bits) colour per shape
    int fgs[N]  = { CLR_YELLOW, CLR_RED, CLR_BLUE, CLR_GREEN, CLR_PINK, CLR_ORANGE, CLR_PEACH, CLR_LIME_GREEN, CLR_WHITE,
                    CLR_LIGHT_PEACH, CLR_LIGHT_YELLOW, CLR_MEDIUM_GREEN, CLR_TRUE_BLUE,
                    CLR_ORANGE, CLR_BLUE, CLR_PINK, CLR_LIGHT_YELLOW };
    int bgs[N]  = { CLR_DARK_PURPLE, CLR_DARK_RED, CLR_DARKER_BLUE, CLR_DARK_GREEN, CLR_MAUVE, CLR_BROWN, CLR_DARK_PEACH, CLR_BLUE_GREEN, CLR_DARK_GREY,
                    CLR_DARK_BROWN, CLR_DARKER_PURPLE, CLR_DARK_BLUE, CLR_DARKER_GREY,
                    CLR_DARK_RED, CLR_DARKER_BLUE, CLR_MAUVE, CLR_DARK_GREEN };
    int pats[N] = { FILL_CHECKER, FILL_DOTS, FILL_HLINES, FILL_VLINES, FILL_DIAG, FILL_GRID, FILL_CHECKER, FILL_DIAG, FILL_DOTS,
                    FILL_CHECKER, FILL_GRID, FILL_DIAG, FILL_HLINES,
                    FILL_GRID, FILL_CHECKER, FILL_VLINES, FILL_DIAG };

    for (int i = 0; i < N; i++) {
        bool sq   = (i >= NROT && i < NROT + NSQ);
        bool tilt = (i >= NROT + NSQ);
        sx[i]   = rnd_float_between(50, SCREEN_W - 50);
        sy[i]   = rnd_float_between(50, SCREEN_H - 50);
        float a = rnd_float_between(0, 360);
        float sp= rnd_float_between(18, 40);
        vx[i]   = cos_deg(a) * sp;
        vy[i]   = sin_deg(a) * sp;
        ang[i]  = sq ? 0 : rnd_float_between(0, 360);        // squares stay axis-aligned
        spin[i] = (sq || tilt) ? 0 : rnd_float_between(-100, 100);  // only the first group spins
        kind[i] = sq ? 2 : (i % 2);          // 0 = polygon, 1 = star, 2 = square
        sides[i]= 3 + (i % 4);               // 3..6
        rad[i]  = sq ? 22 : 15 + (i % 3) * 6;
        fg[i]   = fgs[i];
        bg[i]   = bgs[i];
        pat[i]  = pats[i];
    }
    ready = true;
}

void update(void) {
    if (!ready) setup();

    bool z = btn(0, BTN_A);              // Z on player 0
    if (z && !prev_z) anchored = !anchored;
    prev_z = z;

    float d = dt();
    for (int i = 0; i < N; i++) {
        sx[i]  += vx[i] * d;
        sy[i]  += vy[i] * d;
        ang[i] += spin[i] * d;           // no-op for squares and tilted shapes (spin == 0)
        int m = rad[i] + 2;
        if (sx[i] < m)            { sx[i] = m;            vx[i] = -vx[i]; }
        if (sx[i] > SCREEN_W - m) { sx[i] = SCREEN_W - m; vx[i] = -vx[i]; }
        if (sy[i] < m)            { sy[i] = m;            vy[i] = -vy[i]; }
        if (sy[i] > SCREEN_H - m) { sy[i] = SCREEN_H - m; vy[i] = -vy[i]; }
    }
}

void draw(void) {
    cls(CLR_BLACK);

    for (int i = 0; i < N; i++) {
        int x = (int)sx[i], y = (int)sy[i];
        fillp(pat[i], bg[i]);                       // fg = 0-bits, bg = 1-bits
        fillp_anchor(anchored ? x : 0, anchored ? y : 0);   // glue pattern to this shape
        if (kind[i] == 2)      rectfill(x - rad[i], y - rad[i], rad[i] * 2, rad[i] * 2, fg[i]);
        else if (kind[i] == 1) starfill(x, y, rad[i], rad[i] / 2, sides[i] + 2, ang[i], fg[i]);
        else                   ngonfill(x, y, rad[i], sides[i], ang[i], fg[i]);
        fillp_anchor(0, 0);
        fillp_reset();
    }

    print("fillp_anchor", 9, 9, CLR_BLACK);
    print("fillp_anchor", 8, 8, CLR_WHITE);
    if (anchored) { print("ON  - dither glued to each shape", 9, 21, CLR_BLACK); print("ON  - dither glued to each shape", 8, 20, CLR_LIME_GREEN); }
    else          { print("OFF - dither crawls (world-pinned)", 9, 21, CLR_BLACK); print("OFF - dither crawls (world-pinned)", 8, 20, CLR_RED); }
    print("Z: toggle  -  squares & tilts show it", 9, SCREEN_H - 11, CLR_BLACK);
    print("Z: toggle  -  squares & tilts show it", 8, SCREEN_H - 12, CLR_LIGHT_GREY);
}
