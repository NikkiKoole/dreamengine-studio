/* de:meta
{
  "slug": "27-collide-intro",
  "title": "27. what is collision?",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "collision-detection"
  ],
  "description": "The one big idea: collision is just asking 'are these two boxes in the same place right now?' every single frame."
}
de:meta */
#include "studio.h"

// ---------------------------------------------------------------------------
// THE BIG IDEA
//
// "Collision" sounds fancy, but it is just a yes/no question:
//     are these two things touching RIGHT NOW?
//
// We cannot ask it once and remember the answer, because things MOVE.
// So we ask it again EVERY frame (60 times a second), inside update().
// Move the blue box onto the grey box and watch the answer flip.
// ---------------------------------------------------------------------------

// The wall: a box that never moves, parked in the middle.
int wall_x = 148, wall_y = 88, wall_w = 24, wall_h = 24;

// The player: a box YOU move with the arrow keys.
int px = 40, py = 40;
int pw = 16, ph = 16;

// The live answer to "are we touching?" this frame.
bool touching = false;

// Last frame's answer. We keep it so we can spot the EXACT moment the
// answer changes from "no" to "yes" — that first touch is when we react.
bool was_touching = false;

void update() {
    // Move the player box. speed 2 = two pixels per frame.
    if (btn(0, BTN_LEFT))  px -= 2;
    if (btn(0, BTN_RIGHT)) px += 2;
    if (btn(0, BTN_UP))    py -= 2;
    if (btn(0, BTN_DOWN))  py += 2;

    // Keep the box on screen so you can never lose it off an edge.
    px = clamp(px, 0, SCREEN_W - pw);
    py = clamp(py, 0, SCREEN_H - ph);

    // THE QUESTION, asked fresh every frame:
    // do the player box and the wall box overlap?
    // boxes_touch() answers yes/no for two rectangles (x, y, width, height).
    touching = boxes_touch(px, py, pw, ph, wall_x, wall_y, wall_w, wall_h);

    // EDGE TRIGGER: react only on the frame we FIRST touch, not every
    // frame we stay touching. "touching now" AND "wasn't touching before".
    if (touching && !was_touching) {
        note(72, INSTR_TRI, 4);   // a soft blip so the touch is HEARD
    }

    // Remember this frame's answer for next frame's comparison.
    was_touching = touching;
}

void draw() {
    cls(CLR_DARK_BLUE);

    // The wall flashes yellow the instant something touches it.
    int wall_col = touching ? CLR_YELLOW : CLR_MEDIUM_GREY;
    rectfill(wall_x, wall_y, wall_w, wall_h, wall_col);

    // The player box.
    rectfill(px, py, pw, ph, CLR_BLUE);

    // HUD: name the concept and show its live yes/no answer.
    print("arrows: move the blue box onto the grey", 4, 4, CLR_WHITE);
    print("collision = 'same place, right now?'", 4, 14, CLR_LIGHT_GREY);
    print("...asked every single frame", 4, 24, CLR_LIGHT_GREY);

    // The big answer word, in a hot colour when we overlap.
    font(FONT_COMIC);
    if (touching)
        print("TOUCHING!", 100, 150, CLR_YELLOW);
    else
        print("apart", 130, 150, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
