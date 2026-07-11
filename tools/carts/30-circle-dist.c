/* de:meta
{
  "slug": "30-circle-dist",
  "title": "30. circles & distance",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Move one circle into another and watch them touch — collision is just: is the gap smaller than the two radii added up?"
}
de:meta */
#include "studio.h"

// TWO CIRCLES. You steer the blue one with the arrow keys.
// The big question: are the two circles TOUCHING?
//
// The trick is simpler than it looks. Measure the DISTANCE between
// their two centre points. If that gap is smaller than the two
// radii added together, the circles must be overlapping.
//
//        gap < radiusA + radiusB   →   TOUCHING
//
// That's all circle collision is. No pixels, no magic — just one
// comparison. The engine gives us three helpers for it:
//   distance(x1,y1,x2,y2)               → the gap between two points
//   circles_touch(ax,ay,ar, bx,by,br)   → the whole test in one call
//   near(ax,ay, bx,by, d)               → are two points within d pixels?

// the fixed circle (the target)
int fx = 160, fy = 100, fr = 24;

// the circle you move
int px = 60,  py = 100, pr = 16;

// so we only play the "touch" sound once per touch, not every frame
bool was_touching = false;

void update() {
    // move your circle with the arrow keys
    if (btn(0, BTN_LEFT))  px -= 2;
    if (btn(0, BTN_RIGHT)) px += 2;
    if (btn(0, BTN_UP))    py -= 2;
    if (btn(0, BTN_DOWN))  py += 2;

    // keep it on screen
    px = clamp(px, pr, SCREEN_W - pr);
    py = clamp(py, pr, SCREEN_H - pr);

    // circles_touch() does the whole gap-vs-radii test for us.
    bool touching = circles_touch(px, py, pr, fx, fy, fr);

    // play a note the MOMENT they start touching (not every frame)
    if (touching && !was_touching) note(72, INSTR_TRI, 5);
    was_touching = touching;
}

void draw() {
    cls(CLR_DARK_BLUE);

    // measure the gap between the two centres, and the reach of both circles
    float gap  = distance(px, py, fx, fy);
    int   reach = pr + fr;                 // the two radii added up
    bool  touching = circles_touch(px, py, pr, fx, fy, fr);

    // the line between the two centres — this IS the "gap" we measure
    line(px, py, fx, fy, CLR_DARK_GREY);

    // the fixed target circle — glows white when touched
    circfill(fx, fy, fr, touching ? CLR_WHITE : CLR_RED);

    // your circle
    circfill(px, py, pr, CLR_BLUE);

    // HUD: name the concept and show its live numbers
    print("arrows: move the blue circle", 4, 4, CLR_WHITE);
    print(str("gap between centres: %.0f", gap), 4, 16, CLR_LIGHT_GREY);
    print(str("two radii added up:  %d", reach), 4, 26, CLR_LIGHT_GREY);

    // the verdict — the whole lesson in one line
    if (touching)
        print("gap < radii  ->  TOUCHING!", 4, 40, CLR_YELLOW);
    else
        print("gap > radii  ->  apart", 4, 40, CLR_MEDIUM_GREY);
}
