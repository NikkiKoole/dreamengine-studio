/* de:meta
{
  "slug": "33-pixel-collide",
  "title": "33. reading pixels",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Collision by colour — read the pixel under your dot with pget() and get hit by any shape for free."
}
de:meta */
#include "studio.h"

// THE IDEA: instead of doing maths to test "am I inside the lava?", we just
// ASK THE SCREEN what colour is under us. pget(x, y) returns the palette index
// (0..31) of a pixel that was already drawn. If that pixel is lava-coloured,
// we're standing in lava. Collision follows the drawn shape exactly — a blob,
// a squiggle, anything — with zero extra geometry.

int px, py;     // the player dot's position
int burning;    // is the dot touching the danger colour right now?

void init(void) {
    // Read-back is OFF by default (it costs a little), so we MUST switch it on
    // before pget() will return anything. Forgetting this is the #1 gotcha.
    enable_pget(true);

    px = 60;    // start well clear of the lava
    py = 40;
}

void update() {
    // move the dot with the arrow keys
    if (btn(0, BTN_LEFT))  px -= 2;
    if (btn(0, BTN_RIGHT)) px += 2;
    if (btn(0, BTN_UP))    py -= 2;
    if (btn(0, BTN_DOWN))  py += 2;
    px = clamp(px, 4, SCREEN_W - 4);
    py = clamp(py, 4, SCREEN_H - 4);

    // GOTCHA: pget reads the pixels from the PREVIOUS frame's draw(). That's
    // fine here — draw() paints the lava every frame, so what we read is one
    // frame old but the blob doesn't move. Rule of thumb: draw the zone, then
    // test against it.
    int under = pget(px, py);
    bool touching = (under == CLR_ORANGE || under == CLR_RED);

    // fire the reaction only on the frame we first enter the lava (an "event"),
    // so the sound and shake trigger once, not 60 times a second.
    if (touching && !burning) {
        note(48, INSTR_SAW, 6);   // a low buzz to FEEL the hit
        shake(4);                 // kick the screen
    }
    burning = touching;
}

void draw() {
    cls(CLR_DARK_BLUE);

    // the DANGER ZONE: a lava blob made of overlapping circles. Its shape is
    // irregular on purpose — pget doesn't care, it just reads pixels.
    circfill(180, 120, 44, CLR_RED);
    circfill(140, 140, 30, CLR_ORANGE);
    circfill(220, 150, 34, CLR_ORANGE);
    circfill(160, 100, 24, CLR_RED);
    circfill(240, 110, 26, CLR_RED);

    // the player dot — turns yellow normally, white when it's burning
    circfill(px, py, 4, burning ? CLR_WHITE : CLR_YELLOW);

    // HUD: name the concept and show its live state
    print("arrows: move the dot into the lava", 4, 4, CLR_WHITE);
    print(str("pget(%d,%d) = colour %d", px, py, pget(px, py)), 4, 14, CLR_LIGHT_GREY);

    if (burning) {
        rectfill(0, 180, SCREEN_W, 20, CLR_RED);
        print("OUCH! touching lava", 108, 186, CLR_WHITE);
    } else {
        print("safe", 148, 186, CLR_LIGHT_GREY);
    }
}
