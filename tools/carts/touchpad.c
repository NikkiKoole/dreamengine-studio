/* de:meta
{
  "title": "touchpad",
  "status": "active",
  "created": "2026-06-30",
  "kind": [
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "Pixel-art reference for the proposed on-screen touch controls (docs/design/touch-controls.md): a live parts demo, plus the three responsive placements - deck, rails, overlay - mocked in device frames.",
    "detail": "Design exploration, NOT wired into the engine yet. The sprites (touchpad.cart.js) are drawn with our own tools at native resolution and colorkey'd so they sit transparently on the game - a 4-way and 8-way d-pad, an analog stick, A/B buttons with idle/pressed states, fire buttons. Page through four views with SPACE or a click: (1) PARTS + a live demo you drive with arrows/Z/X; (2) DECK - portrait, game on top and controls in the band below (PICO-8 layout); (3) RAILS - landscape, game fills the height and controls ride the side rails; (4) OVERLAY - no spare space, controls hug the thumb corners on top (the fallback). The placement that wins is chosen by the available letterbox, per the design doc.",
    "controls": "SPACE / click = next view   ·   (view 1) arrows = stick + d-pad, Z = A, X = B"
  }
}
de:meta */
// touchpad — pixel-art on-screen touch controls, for a look.
//
// Companion to docs/design/touch-controls.md. The art lives in touchpad.cart.js
// (sprite-draw.js); this cart lays it out four ways: a live PARTS demo, then the
// DECK / RAILS / OVERLAY placements mocked inside device frames so you can see
// where the controls land in each case. Nothing here is the real engine widget.
#include "studio.h"
#include <string.h>

static int ssx(int slot) { return (slot % 8) * 16; }
static int ssy(int slot) { return (slot / 8) * 16; }
static void spr2(int slot, int x, int y) { sspr(ssx(slot), ssy(slot), 16, 16, x, y, 32, 32); }

// tiny label centred under a 32px cell at column x
static void tinyc(const char *s, int x, int y) {
    font(FONT_TINY);
    print(s, x + 16 - (int)strlen(s) * 2, y, CLR_LIGHT_GREY);
}

// a tiny mock "game" so the placement views read as a real screen
static void mockgame(int x, int y, int w, int h) {
    rectfill(x, y, w, h, CLR_DARK_BLUE);                       // sky
    int gh = h / 3;
    rectfill(x, y + h - gh, w, gh, CLR_DARK_GREEN);            // ground
    circfill(x + w - 11, y + 10, 5, CLR_YELLOW);              // sun
    circfill(x + 12, y + 11, 3, CLR_LIGHT_GREY);             // cloud
    circfill(x + 18, y + 11, 3, CLR_LIGHT_GREY);
    rectfill(x + w / 2 - 2, y + h - gh - 6, 4, 6, CLR_RED);   // player
}

// ── view 1: parts gallery + live demo ──────────────────────────────────────────
static void view_parts(void) {
    font(FONT_COMIC); print("TOUCH CONTROLS", 8, 5, CLR_WHITE);
    font(FONT_TINY);  print("PIXEL-ART PARTS  +  LIVE DEMO", 8, 28, CLR_INDIGO);

    const int   slots[] = { 0, 2, 4, 5, 8, 10, 12 };
    const char *labs[]  = { "DPAD", "8WAY", "BASE", "KNOB", "A", "B", "FIRE" };
    const int   n = 7, gap = 8, step = 32 + gap;
    const int   sx0 = (SCREEN_W - (n * 32 + (n - 1) * gap)) / 2, gy = 42;
    for (int i = 0; i < n; i++) { int x = sx0 + i * step; spr2(slots[i], x, gy); tinyc(labs[i], x, gy + 34); }
    line(8, gy + 46, SCREEN_W - 8, gy + 46, CLR_DARKER_GREY);

    font(FONT_TINY); print("LIVE   arrows = stick + d-pad    Z = A    X = B", 8, 96, CLR_YELLOW);
    int u = btn(0, BTN_UP), d = btn(0, BTN_DOWN), l = btn(0, BTN_LEFT), r = btn(0, BTN_RIGHT);
    int a = btn(0, BTN_A),  b = btn(0, BTN_B), ly = 120;

    int bx = 36; spr2(4, bx, ly);
    int dx = (r ? 1 : 0) - (l ? 1 : 0), dy = (d ? 1 : 0) - (u ? 1 : 0);
    spr2(5, bx + dx * 8, ly + dy * 8); tinyc("STICK", bx, ly + 34);

    int px = 132;
    if (u || d || l || r) { float deg = u ? 0.f : r ? 90.f : d ? 180.f : 270.f; sspr_ex(ssx(1), ssy(1), 16, 16, px, ly, 32, 32, deg, 16, 16); }
    else spr2(0, px, ly);
    tinyc("D-PAD", px, ly + 34);

    int ax = 224, bxb = 268;
    spr2(a ? 9 : 8, ax, ly); spr2(b ? 11 : 10, bxb, ly); tinyc("A / B", (ax + bxb) / 2, ly + 34);
}

// frame + caption shared by the placement views
static void titlecap(const char *title, const char *cap) {
    font(FONT_COMIC); print(title, 8, 5, CLR_WHITE);
    font(FONT_TINY);  print(cap, 8, 28, CLR_INDIGO);
}

// ── view 2: DECK (portrait — game on top, controls in the band below) ───────────
static void view_deck(void) {
    titlecap("DECK", "PORTRAIT: game on top, controls in the band below");
    int dw = 116, dh = 150, dx = (SCREEN_W - dw) / 2, dy = 40;
    rect(dx, dy, dw, dh, CLR_LIGHT_GREY);                         // device frame
    int gw = dw - 8, gh = (gw * 5) / 8, gx = dx + 4, gy = dy + 4;
    mockgame(gx, gy, gw, gh);                                     // game (8:5) on top
    int by = gy + gh;                                             // band below
    rectfill(dx + 1, by, dw - 2, dy + dh - by - 1, CLR_DARKER_GREY);
    int bandc = by + (dy + dh - by) / 2 - 16;
    spr2(0, dx + 8, bandc);                                       // d-pad left
    spr2(8, dx + dw - 76, bandc);                                // A
    spr2(10, dx + dw - 40, bandc);                               // B
}

// ── view 3: RAILS (landscape — game fills height, controls on the side rails) ───
static void view_rails(void) {
    titlecap("RAILS", "LANDSCAPE: game fills height, controls on the side rails");
    int dw = 300, dh = 150, dx = (SCREEN_W - dw) / 2, dy = 40;
    rect(dx, dy, dw, dh, CLR_LIGHT_GREY);
    int gh = dh - 8, gw = (gh * 8) / 5, gx = dx + (dw - gw) / 2, gy = dy + 4;
    mockgame(gx, gy, gw, gh);                                     // game fills the height
    rectfill(dx + 1, dy + 1, gx - dx - 1, dh - 2, CLR_DARKER_GREY);          // left rail
    rectfill(gx + gw, dy + 1, dx + dw - (gx + gw) - 1, dh - 2, CLR_DARKER_GREY); // right rail
    int railL = dx + (gx - dx) / 2 - 16, mid = dy + dh / 2;
    spr2(4, railL, mid - 16); spr2(5, railL, mid - 16);          // stick on left rail
    int railR = gx + gw + (dx + dw - (gx + gw)) / 2 - 16;
    spr2(8, railR, mid - 38); spr2(10, railR, mid + 6);          // A / B stacked, right rail
}

// ── view 4: OVERLAY (no letterbox — controls hug the thumb corners, on top) ─────
static void view_overlay(void) {
    titlecap("OVERLAY", "NO SPARE SPACE: controls hug the thumb corners, ON TOP");
    int dw = 300, dh = 150, dx = (SCREEN_W - dw) / 2, dy = 40;
    mockgame(dx, dy, dw, dh);                                     // game fills the device
    rect(dx, dy, dw, dh, CLR_LIGHT_GREY);
    spr2(0, dx + 8, dy + dh - 40);                               // d-pad bottom-left, on top
    spr2(8, dx + dw - 76, dy + dh - 40);                         // A bottom-right
    spr2(10, dx + dw - 40, dy + dh - 40);                        // B
}

void draw(void) {
    static int inited = 0, page = 0;
    if (!inited) { colorkey(CLR_BLACK); inited = 1; }            // key out the sprites' index-0 bg
    if (keyp(KEY_SPACE) || mouse_pressed(0)) page = (page + 1) % 4;

    cls(CLR_DARK_BLUE);
    if (page == 0) view_parts();
    else if (page == 1) view_deck();
    else if (page == 2) view_rails();
    else view_overlay();

    font(FONT_TINY);
    print("SPACE / click = next view", 8, SCREEN_H - 10, CLR_DARK_GREY);
    char tag[16]; tag[0] = (char)('1' + page); tag[1] = '/'; tag[2] = '4'; tag[3] = 0;
    print(tag, SCREEN_W - 22, SCREEN_H - 10, CLR_LIGHT_GREY);
}
