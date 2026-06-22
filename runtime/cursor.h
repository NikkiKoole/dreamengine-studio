// cursor.h — a pixel mouse cursor drawn into the canvas, cart-land.
//
// Why this exists: the OS pointer is a crisp hi-res arrow floating over chunky
// 4x-scaled pixel art (it breaks the look), and — because the engine captures
// the RENDER TEXTURE — it never appears in screenshots, GIFs, or the debug
// harness's frame dumps. So a cursor-centric cart (the logic sandbox, the
// sprite/map editors) shoots demos with no pointer visible. This header hides
// the OS pointer and blits its own cursor at native resolution: pixel-coherent
// at any scale AND present in every capture. Engine deliberately does NOT own
// this (decision-0006 lane); carts #include it. Everything is static — each
// cart compiles its own copy (the ui.h / pointer.h pattern).
//
// Usage — call cursor_draw() LAST in draw(), on top of everything:
//
//   #include "cursor.h"
//   void draw(void) {
//       cls(CLR_BLACK);
//       ... your scene ...
//       cursor_draw(CUR_ARROW);              // the pointer, on top
//   }
//
// It manages the OS pointer for you: it hides the system cursor once it sees a
// real mouse (free movement with no finger down, or a scroll), and shows it
// again otherwise — so on a TOUCH device nothing is drawn and the OS pointer is
// left alone (a finger is its own pointer). No init() call needed.
//
//   cursor_draw(shape)        — CUR_ARROW / CUR_HAND / CUR_GRAB / CUR_CROSS / CUR_MOVE
//   cursor_draw_tint(shape,c) — same, but fill with palette colour c (e.g. a
//                               crosshair tinted to the active tool)
//   cursor_active()           — true once a real mouse is detected (gate mouse-only UI on it)
//
// Tunables (#define before the include): CURSOR_FILL, CURSOR_OUTLINE.

#ifndef CURSOR_H
#define CURSOR_H

#include "studio.h"

#ifndef CURSOR_FILL
#define CURSOR_FILL    CLR_WHITE
#endif
#ifndef CURSOR_OUTLINE
#define CURSOR_OUTLINE CLR_BLACK
#endif

enum { CUR_ARROW = 0, CUR_HAND, CUR_GRAB, CUR_CROSS, CUR_MOVE, CUR__COUNT };

// 'X' = a filled pixel; anything else is transparent. A 1px black halo is added
// automatically at draw time, so these read on any background.
static const char *const cursor__arrow[] = {
    "X.......", "XX......", "XXX.....", "XXXX....", "XXXXX...", "XXXXXX..",
    "XXXXXXX.", "XXXXXXXX", "XXXX....", "X..XX...", "...XX...", "....XX..",
};
static const char *const cursor__hand[] = {   // pointing hand, index extended
    "..XX....",
    "..XX....",
    "..XX....",
    "..XXX.X.",   // index + folded-finger bumps (1px gaps → black separators via the halo)
    "..XXXXXX",
    "XX.XXXXX",   // thumb on the left, gap separates it from the palm
    "XXXXXXXX",
    ".XXXXXXX",
    ".XXXXXXX",
    ".XXXXXXX",
    "..XXXXX.",
};
static const char *const cursor__grab[] = {    // closed/grabbing hand — SAME grid + hotspot + palm rows as cursor__hand, so
    "........",                                // the hover→drag swap reads as the finger curling into a fist IN PLACE (no jump).
    "........",                                // knuckle bumps on top + a thumb-crease band (gaps go black via the halo).
    "........",
    ".X.X.X.X",   // knuckles
    "XXXXXXXX",
    "XX....XX",   // thumb curled across — a dark crease
    "XXXXXXXX",
    ".XXXXXXX",
    ".XXXXXXX",
    ".XXXXXXX",
    "..XXXXX.",
};
static const char *const cursor__cross[] = {   // crosshair, open centre on the target
    "....X....", "....X....", "....X....", ".........", "XXX...XXX",
    ".........", "....X....", "....X....", "....X....",
};
static const char *const cursor__move[] = {    // four-way move/pan
    "....X....", "...XXX...", "....X....", "X...X...X", "XXXXXXXXX",
    "X...X...X", "....X....", "...XXX...", "....X....",
};

typedef struct { const char *const *rows; short n, w, hotx, hoty; } Cursor__Shape;
static const Cursor__Shape cursor__shapes[CUR__COUNT] = {
    [CUR_ARROW] = { cursor__arrow, 12, 8, 0, 0 },
    [CUR_HAND]  = { cursor__hand,  11, 8, 3, 0 },
    [CUR_GRAB]  = { cursor__grab,  11, 8, 3, 0 },   // same hotspot/size as CUR_HAND → no jump on the swap
    [CUR_CROSS] = { cursor__cross,  9, 9, 4, 4 },
    [CUR_MOVE]  = { cursor__move,   9, 9, 4, 4 },
};

static int   cursor__has_mouse = 0;
static int   cursor__os_hidden = 0;
static int   cursor__px = -9999, cursor__py = -9999;

static void cursor__blit(const Cursor__Shape *s, int px, int py, int fill) {
    for (int r = 0; r < s->n; r++)               // black halo first (outline)
        for (int c = 0; c < s->w; c++)
            if (s->rows[r][c] == 'X')
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        pset(px + c + dx, py + r + dy, CURSOR_OUTLINE);
    for (int r = 0; r < s->n; r++)               // fill on top
        for (int c = 0; c < s->w; c++)
            if (s->rows[r][c] == 'X')
                pset(px + c, py + r, fill);
}

// A real mouse reveals itself by moving freely (no finger down) or scrolling —
// neither is possible on a pure touch device. Sticky once seen.
static void cursor__detect(void) {
    int mx = mouse_x(), my = mouse_y();
    if (touch_count() == 0 && cursor__px != -9999 && (mx != cursor__px || my != cursor__py))
        cursor__has_mouse = 1;
    if (mouse_wheel() != 0.0f) cursor__has_mouse = 1;
    cursor__px = mx; cursor__py = my;
}

static void cursor_draw_tint(int shape, int fill) {
    cursor__detect();
    if (!cursor__has_mouse) {                    // touch / before first move: leave the OS pointer
        if (cursor__os_hidden) { mouse_show(); cursor__os_hidden = 0; }
        return;
    }
    if (!cursor__os_hidden) { mouse_hide(); cursor__os_hidden = 1; }
    if (shape < 0 || shape >= CUR__COUNT) shape = CUR_ARROW;
    const Cursor__Shape *s = &cursor__shapes[shape];
    cursor__blit(s, mouse_x() - s->hotx, mouse_y() - s->hoty, fill);
}

static void cursor_draw(int shape) { cursor_draw_tint(shape, CURSOR_FILL); }

static int cursor_active(void) { return cursor__has_mouse; }

#endif // CURSOR_H
