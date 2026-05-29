#include "studio.h"

// Sprite slots 1-4 hold a 4-frame walk cycle (defined in 15-anim.cart.js)
#define COUNT 7
#define FPS   5.0f

void draw() {
    cls(CLR_DARK_BLUE);

    // ── top row: all in sync ─────────────────────────────────
    print("phase 0 — in sync", 4, 4, CLR_LIGHT_GREY);
    for (int i = 0; i < COUNT; i++) {
        spr(1 + anim(4, FPS, 0.0f), 8 + i * 44, 18);
    }

    // ── bottom row: staggered ────────────────────────────────
    print("phase i/count — staggered", 4, 96, CLR_LIGHT_GREY);
    for (int i = 0; i < COUNT; i++) {
        spr(1 + anim(4, FPS, (float)i / COUNT), 8 + i * 44, 110);
    }

    // ── usage hint ───────────────────────────────────────────
    print("spr(1 + anim(4, fps,", 4, 162, CLR_YELLOW);
    print("    (float)i/n), x, y);", 4, 172, CLR_YELLOW);
    print("phase 0..1 shifts the cycle start.", 4, 185, CLR_DARK_GREY);
}
