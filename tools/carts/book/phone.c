// Chapter-14 illustration: what on-screen touch controls look like. The engine draws
// the real ones for you on a device (and hides them on desktop) — so this cart MOCKS
// them, and animates a hero as if thumbs were driving it, to show the shape of it.
#include "studio.h"

#define GROUND 150

void draw(void) {
    cls(CLR_DARK_BLUE);
    int t = frame();
    float dir = sin_deg(t * 2.5f);                 // pretend-stick: sways left/right
    static float hx = 160; hx += dir * 1.7f;
    if (hx < 20) hx = 20; if (hx > SCREEN_W - 20) hx = SCREEN_W - 20;
    int cyc = t % 48, hopping = cyc < 24;
    int hop = hopping ? (int)(sin_deg(cyc * 7.5f) * 28) : 0;

    rectfill(0, GROUND, SCREEN_W, 50, CLR_DARK_GREEN);
    // the hero (a green walker), hopping when "A" is held
    int hy = GROUND - 14 - hop;
    ovalfill((int)hx, hy, 11, 14, CLR_GREEN);
    ovalfill((int)hx, hy + 4, 6, 8, CLR_YELLOW);
    circfill((int)hx - 4, hy - 5, 3, CLR_WHITE); circfill((int)hx + 4, hy - 5, 3, CLR_WHITE);
    pset((int)hx - 4, hy - 5, CLR_BLACK); pset((int)hx + 4, hy - 5, CLR_BLACK);

    // --- the on-screen controls (a mock of what touch_layout draws) ---
    int sxb = 40, syb = GROUND + 20;
    circ(sxb, syb, 20, CLR_LIGHT_GREY);                         // stick base
    circfill(sxb + (int)(dir * 12), syb, 10, CLR_LIGHT_GREY);   // thumb knob, deflected
    int ax = SCREEN_W - 36, ay = GROUND + 24, bxx = SCREEN_W - 64, byy = GROUND + 6;
    circfill(ax, ay, 14, hopping ? CLR_YELLOW : CLR_DARK_GREY); // "A" flashes on jump
    print_centered("A", ax, ay - 3, hopping ? CLR_BLACK : CLR_LIGHT_GREY);
    circfill(bxx, byy, 14, CLR_DARK_GREY);
    print_centered("B", bxx, byy - 3, CLR_LIGHT_GREY);

    print("touch_layout(TOUCH_DPAD8, 2);", 8, 8, CLR_LIGHT_GREY);
}
