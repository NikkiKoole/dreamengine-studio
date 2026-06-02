#include "studio.h"

// FILLP_ANCHOR — patterned shapes that carry their dither with them.
//
// Every shape drifts, bounces off the walls, and spins. Its 4×4 fill pattern is
// anchored to the shape's OWN center via fillp_anchor(), so the dither travels
// with the shape. Without that, the pattern is pinned to the world grid and the
// shape just slides across it — the dither appears to "crawl". Press Z to toggle
// anchoring off and watch the crawl come back.
//
//   Z — toggle pattern anchoring on / off

#define N 9

static float sx[N], sy[N], vx[N], vy[N], ang[N], spin[N];
static int   kind[N], sides[N], rad[N], fg[N], bg[N], pat[N];
static bool  anchored = true;
static bool  prev_z   = false;
static bool  ready    = false;

static void setup(void) {
    // a distinct foreground (0-bits) + background (1-bits) colour per shape
    int fgs[N]  = { CLR_YELLOW, CLR_RED,    CLR_BLUE,       CLR_GREEN,     CLR_PINK,
                    CLR_ORANGE, CLR_PEACH,  CLR_LIME_GREEN, CLR_WHITE };
    int bgs[N]  = { CLR_DARK_PURPLE, CLR_DARK_RED, CLR_DARKER_BLUE, CLR_DARK_GREEN, CLR_MAUVE,
                    CLR_BROWN,       CLR_DARK_PEACH, CLR_BLUE_GREEN, CLR_DARK_GREY };
    int pats[N] = { FILL_CHECKER, FILL_DOTS, FILL_HLINES, FILL_VLINES, FILL_DIAG,
                    FILL_GRID,    FILL_CHECKER, FILL_DIAG, FILL_DOTS };

    for (int i = 0; i < N; i++) {
        sx[i]   = rnd_float_between(40, SCREEN_W - 40);
        sy[i]   = rnd_float_between(40, SCREEN_H - 40);
        float a = rnd_float_between(0, 360);
        float sp= rnd_float_between(20, 45);
        vx[i]   = cos_deg(a) * sp;
        vy[i]   = sin_deg(a) * sp;
        ang[i]  = rnd_float_between(0, 360);
        spin[i] = rnd_float_between(-100, 100);
        kind[i] = i % 2;                 // 0 = polygon, 1 = star
        sides[i]= 3 + (i % 4);           // 3..6
        rad[i]  = 15 + (i % 3) * 6;      // 15, 21, 27
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
        ang[i] += spin[i] * d;
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
        if (kind[i]) starfill(x, y, rad[i], rad[i] / 2, sides[i] + 2, ang[i], fg[i]);
        else         ngonfill(x, y, rad[i], sides[i], ang[i], fg[i]);
        fillp_anchor(0, 0);
        fillp_reset();
    }

    print("fillp_anchor", 8, 8, CLR_WHITE);
    if (anchored) print("ON  - dither rides with each shape", 8, 20, CLR_LIME_GREEN);
    else          print("OFF - dither crawls (world-pinned)", 8, 20, CLR_RED);
    print("Z: toggle anchoring", 8, SCREEN_H - 12, CLR_LIGHT_GREY);
}
