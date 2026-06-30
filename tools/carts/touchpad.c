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
    "summary": "Pixel-art reference for the proposed on-screen touch controls (docs/design/touch-controls.md): a parts gallery up top, a live pokeable demo below.",
    "detail": "Design exploration, NOT wired into the engine yet. The sprites (in touchpad.cart.js) are drawn with our own tools at native resolution so they read as part of the console - a 4-way and 8-way d-pad, an analog stick (base + knob), A/B buttons with idle/pressed states, and unlabeled fire buttons. Every shape is mirror-symmetric (tested against the true grid centre). The A/B labels use the engine's own dos_8x8 font via sprite-draw's new print(), so they match what print() renders at runtime. The live row reacts to input: the stick knob slides, the d-pad lights the pressed arm (spr_ex rotation of the pressed-up art), and A/B swap to their pressed sprite.",
    "controls": "arrows = stick + d-pad   ·   Z = A   ·   X = B   (works on keyboard, gamepad, or the on-screen overlay)"
  }
}
de:meta */
// touchpad — pixel-art on-screen touch controls, for a look.
//
// Companion to docs/design/touch-controls.md. The art lives in touchpad.cart.js
// (sprite-draw.js); this cart just lays it out: a labelled PARTS gallery, then a
// LIVE row you can drive with the arrows / Z / X to feel idle vs pressed states.
// Nothing here is the real engine widget — it's the look, up for review.
#include "studio.h"
#include <string.h>

// sprite-sheet is 8 slots wide → slot index to its (x,y) on the sheet
static int ssx(int slot) { return (slot % 8) * 16; }
static int ssy(int slot) { return (slot / 8) * 16; }

// draw a 16×16 slot scaled 2× (→ 32×32) at (x,y)
static void spr2(int slot, int x, int y) {
    sspr(ssx(slot), ssy(slot), 16, 16, x, y, 32, 32);
}

// FONT_TINY label, centred under a 32px cell at column x
static void tinyc(const char *s, int x, int y) {
    font(FONT_TINY);
    print(s, x + 16 - (int)strlen(s) * 2, y, CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    font(FONT_COMIC);
    print("TOUCH CONTROLS", 8, 5, CLR_WHITE);
    font(FONT_TINY);
    print("PIXEL-ART REFERENCE  -  docs/design/touch-controls.md", 8, 28, CLR_INDIGO);

    // ── PARTS gallery (2× sprites + tiny labels) ──────────────────────────────
    const int   slots[] = { 0, 2, 4, 5, 8, 10, 12 };
    const char *labs[]  = { "DPAD", "8WAY", "BASE", "KNOB", "A", "B", "FIRE" };
    const int   n = 7, gap = 8, step = 32 + gap;
    const int   sx0 = (SCREEN_W - (n * 32 + (n - 1) * gap)) / 2;
    const int   gy = 42;
    for (int i = 0; i < n; i++) {
        int x = sx0 + i * step;
        spr2(slots[i], x, gy);
        tinyc(labs[i], x, gy + 34);
    }
    line(8, gy + 46, SCREEN_W - 8, gy + 46, CLR_DARKER_GREY);

    // ── LIVE demo ─────────────────────────────────────────────────────────────
    font(FONT_TINY);
    print("LIVE   arrows = stick + d-pad    Z = A    X = B", 8, 96, CLR_YELLOW);

    int u = btn(0, BTN_UP), d = btn(0, BTN_DOWN), l = btn(0, BTN_LEFT), r = btn(0, BTN_RIGHT);
    int a = btn(0, BTN_A),  b = btn(0, BTN_B);
    int ly = 120;

    // stick — base fixed, knob slides with the d-pad axes
    int bx = 36;
    spr2(4, bx, ly);
    int dx = (r ? 1 : 0) - (l ? 1 : 0), dy = (d ? 1 : 0) - (u ? 1 : 0);
    spr2(5, bx + 8 + dx * 8, ly + 8 + dy * 8);
    tinyc("STICK", bx, ly + 34);

    // d-pad — idle, or the pressed-up art rotated toward the held direction
    int px = 132;
    if (u || d || l || r) {
        float deg = u ? 0.f : r ? 90.f : d ? 180.f : 270.f;
        sspr_ex(ssx(1), ssy(1), 16, 16, px, ly, 32, 32, deg, 16, 16);
    } else {
        spr2(0, px, ly);
    }
    tinyc("D-PAD", px, ly + 34);

    // A / B — swap to the pressed sprite while held
    int ax = 224, bxb = 268;
    spr2(a ? 9 : 8,  ax,  ly);
    spr2(b ? 11 : 10, bxb, ly);
    tinyc("A / B", (ax + bxb) / 2, ly + 34);
}
