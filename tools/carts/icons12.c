/* de:meta
{
  "title": "icons12",
  "status": "active",
  "created": "2026-07-09",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [],
  "description": {
    "summary": "A little sheet of four hand-usable 12×12 PICO-palette icons — a smiley, the acid-house smiley, a steam locomotive and a palm tree — drawn programmatically with sprite-draw.js and shown big + at native size.",
    "detail": "Each icon is authored on a 12×12 canvas in tools/carts/icons12.cart.js then centered into the normal 16×16 sprite slot (offset 2,2), so it packs into the standard 8×8 sheet and spr()s as a clean 12×12 glyph with 2px of transparent breathing room. The cart is a display board: a row of the four icons scaled up 4× with labels, then the same four at actual 12×12 size to prove they read small. Copy the .cart.js generators straight into any cart that needs these glyphs.",
    "controls": "None — just look. Icons are sprite slots 0 smiley · 1 acid · 2 loco · 3 palm."
  }
}
de:meta */

// icons12 — a display board for four code-drawn 12×12 icons.
// The art lives in icons12.cart.js; here we just lay them out on screen.

#include "studio.h"

// The 12×12 art sits at offset (2,2) inside each 16×16 slot; slots are 16 apart.
static void icon_big(int slot, int dx, int dy, int size) {
    sspr(slot * 16 + 2, 2, 12, 12, dx, dy, size, size);
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    print("12x12 PICO ICONS", 92, 12, CLR_WHITE);

    const char *names[4] = { "smiley", "acid", "loco", "palm" };

    // big row — each icon scaled to 48x48 (4x), on a soft tile
    int big = 48;
    for (int i = 0; i < 4; i++) {
        int x = 18 + i * 76;
        int y = 46;
        rrectfill(x - 6, y - 6, big + 12, big + 12, 4, CLR_DARK_GREY);
        icon_big(i, x, y, big);
        // center the label under the tile
        print(names[i], x + (big - text_width(names[i])) / 2, y + big + 10, CLR_LIGHT_GREY);
    }

    // native-size row — proof they read at a true 12x12
    print("actual size:", 18, 150, CLR_MEDIUM_GREY);
    for (int i = 0; i < 4; i++) {
        icon_big(i, 118 + i * 20, 148, 12);
    }
}
