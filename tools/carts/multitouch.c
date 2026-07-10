/* de:meta
{
  "slug": "multitouch",
  "title": "multitouch paint",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "Finger-paint playground for the touch API. Every finger paints its own color at the same time - touch_id() keeps a finger's color stable while other fingers come and go - a new finger landing plays a mallet blip pitched by where it touched, and the CLEAR button shows tapp() edge-triggering: one tap, one clear. Best on a touchscreen: settings > Build for web, then open the LAN url from the build log on an iPad/phone on the same wifi. On desktop the mouse paints as a single finger."
}
de:meta */
// multitouch — finger-paint playground for the touch API
//
// Every finger paints its own color at the same time. touch_id() keeps a
// finger's color stable while other fingers come and go (indices shuffle,
// ids don't). A new finger landing plays a mallet blip pitched by where it
// lands, and the CLEAR button shows tapp() edge-triggering: one tap, one
// clear — not 60 clears a second.
//
// Best on a touchscreen: settings → "Build for web", then open the LAN url
// from the build log on an iPad/phone on the same wifi. On desktop the
// mouse paints as a single finger.
#include "studio.h"
#include <math.h>
#include <stdio.h>

#define MAX_DOTS 8192
static short dot_px[MAX_DOTS], dot_py[MAX_DOTS];
static unsigned char dot_col[MAX_DOTS];
static int dot_n = 0;

static int held_id[10];   // ids that were down last frame — to spot fresh fingers
static int held_n = 0;

static const int FINGER_COLS[8] = {
    CLR_RED, CLR_BLUE, CLR_GREEN, CLR_YELLOW,
    CLR_ORANGE, CLR_PINK, CLR_LIME_GREEN, CLR_PEACH,
};

static int id_color(int id) {
    int h = id < 0 ? -id : id;          // mouse-touch id is -2
    return FINGER_COLS[h % 8];
}

static void add_dot(int x, int y, int col) {
    if (dot_n >= MAX_DOTS) return;
    dot_px[dot_n] = (short)x;
    dot_py[dot_n] = (short)y;
    dot_col[dot_n] = (unsigned char)col;
    dot_n++;
}

void init(void) {
    // seed a few colored ribbons so the cart doesn't open onto a black screen
    for (int k = 0; k < 5; k++)
        for (int t = 0; t < 70; t++) {
            int x = 20 + t * (SCREEN_W - 40) / 70;
            int y = 110 + (int)(sinf(t * 0.12f + k * 1.4f) * (16 + k * 9));
            add_dot(x, y, FINGER_COLS[(k + 2) % 8]);
        }
}

void update(void) {
    // CLEAR button — tapp() fires only on the frame a touch begins
    if (tapp(SCREEN_W - 52, 4, 48, 13)) dot_n = 0;

    int n = touch_count();
    for (int i = 0; i < n; i++) {
        int id = touch_id(i);
        int x = touch_x(i), y = touch_y(i);
        bool fresh = true;
        for (int j = 0; j < held_n; j++)
            if (held_id[j] == id) { fresh = false; break; }
        if (fresh)   // new finger just landed → blip, pitch follows x
            hit(48 + 36 * x / SCREEN_W, INSTR_MALLET, 4, 260);
        if (y > 20) add_dot(x, y, id_color(id));   // keep the hud strip clean
    }

    held_n = n > 10 ? 10 : n;
    for (int i = 0; i < held_n; i++) held_id[i] = touch_id(i);
}

void draw(void) {
    cls(CLR_BLACK);

    for (int i = 0; i < dot_n; i++)
        circfill(dot_px[i], dot_py[i], 4, dot_col[i]);

    // live fingers: white ring + stable id label
    int n = touch_count();
    for (int i = 0; i < n; i++) {
        int x = touch_x(i), y = touch_y(i);
        circ(x, y, 9, CLR_WHITE);
        char tag[8];
        snprintf(tag, sizeof tag, "%d", touch_id(i));
        print(tag, x + 11, y - 3, CLR_WHITE);
    }

    // hud
    char info[40];
    snprintf(info, sizeof info, "MULTITOUCH PAINT  fingers:%d  dots:%d", n, dot_n);
    print(info, 4, 6, CLR_WHITE);
    rectfill(SCREEN_W - 52, 4, 48, 13, CLR_DARK_BLUE);
    rect(SCREEN_W - 52, 4, 48, 13, CLR_BLUE);
    print("CLEAR", SCREEN_W - 44, 7, CLR_WHITE);
}
