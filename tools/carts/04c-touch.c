/* de:meta
{
  "slug": "04c-touch",
  "title": "4c. touch & tap",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "lineage": "the touchscreen counterpart of 04b-mouse (the pointer)",
  "description": "Read a touchscreen. touch_count() says how many fingers are down, touch_x(i)/touch_y(i) find each one, and tap(x,y,w,h) tests whether a finger is inside a box. A phone reports MANY fingers at once; a mouse is just one. Touch the target to light it up and hear it ring."
}
de:meta */
#include "studio.h"

// 4c — TOUCH & TAP. The touchscreen sibling of the mouse (04b-mouse). The calls:
//   touch_count()      how many fingers are down RIGHT NOW (0..10)
//   touch_x(i)         x of finger number i, for i in 0..count-1  (-1 if invalid)
//   touch_y(i)         y of finger number i                       (-1 if invalid)
//   tap(x,y,w,h)       true while ANY finger is inside this box   (canvas pixels)
//
// The big idea: a phone can report MANY fingers at ONCE. Unlike the mouse — which is
// always exactly one pointer — touch_count() can be 0, 1, 2, 3... So we LOOP over the
// fingers (i from 0 to touch_count()-1) and draw a ring at each one.
//
// Testable on desktop: a mouse click usually shows up as ONE touch (count == 1), so you
// can click the target to light it up. On a phone, press it with several fingers to watch
// the count climb and see a numbered ring appear under each finger.

// the target box the player taps
int tx = 110, ty = 70, tw = 100, th = 60;
bool lit_prev = false;   // was the target tapped LAST frame? (so the note rings once, not 60x/sec)

void draw() {
    cls(CLR_DARK_BLUE);

    // is a finger inside the target box this frame?
    bool lit = tap(tx, ty, tw, th);

    // ring the note only on the FRAME the tap begins (a rising edge), not every frame it's held
    if (lit && !lit_prev) note(72, INSTR_TRI, 6);
    lit_prev = lit;

    // the target: bright + a label when touched, dim when not — colour AND sound = felt, not read
    rectfill(tx, ty, tw, th, lit ? CLR_ORANGE : CLR_DARKER_BLUE);
    rect(tx, ty, tw, th, CLR_WHITE);
    print(lit ? "TOUCHING!" : "tap here", tx + 10, ty + th / 2 - 3,
          lit ? CLR_WHITE : CLR_LIGHT_GREY);

    // loop over every active finger and draw a ring + its index number where it touches
    int n = touch_count();
    for (int i = 0; i < n; i++) {
        int fx = touch_x(i), fy = touch_y(i);
        circ(fx, fy, 12, CLR_YELLOW);
        circ(fx, fy, 11, CLR_YELLOW);
        print(str("%d", i), fx - 1, fy - 2, CLR_WHITE);   // finger index 0,1,2...
    }

    // explain what's on screen
    print("TOUCH & TAP", 4, 4, CLR_WHITE);
    print("tap the box  -  a phone tracks", 4, 16, CLR_LIGHT_GREY);
    print("many fingers, a mouse just one", 4, 24, CLR_LIGHT_GREY);

    // the live values, so you can watch them change as fingers come and go
    print(str("touch_count() = %d", n), 4, SCREEN_H - 22, CLR_PEACH);
    print(str("target: %s", lit ? "TOUCHING" : "apart"), 4, SCREEN_H - 12, CLR_PEACH);
}
