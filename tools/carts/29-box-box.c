/* de:meta
{
  "slug": "29-box-box",
  "title": "29. box vs box (AABB)",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "collision-detection"
  ],
  "description": "AABB collision — push one box into another and watch the exact overlap rectangle appear."
}
de:meta */
#include "studio.h"

/*
  AABB = Axis-Aligned Bounding Box: a rectangle whose sides line up with the
  screen (never rotated). Two AABBs OVERLAP when they overlap on BOTH axes.

  boxes_touch(ax,ay,aw,ah, bx,by,bw,bh) answers yes/no for us. But this cart
  goes one step further and computes HOW MUCH they overlap — the little
  rectangle where they share space. That shared box is the "intersection":
      left   = the bigger of the two left edges   -> max(ax, bx)
      top    = the bigger of the two top edges     -> max(ay, by)
      right  = the smaller of the two right edges  -> min(ax+aw, bx+bw)
      bottom = the smaller of the two bottom edges -> min(ay+ah, by+bh)
  If left < right AND top < bottom, that rectangle is real and we draw it.
*/

// the FIXED box — it never moves.
int fx = 130, fy = 80, fw = 60, fh = 60;

// the MOVING box — arrows push it around.
int mx = 40, my = 40, mw = 50, mh = 40;

int was_touching = 0;   // remembers last frame, so we play the sound only on FIRST contact

void update() {
    // move the box with arrow keys
    if (btn(0, BTN_LEFT))  mx -= 2;
    if (btn(0, BTN_RIGHT)) mx += 2;
    if (btn(0, BTN_UP))    my -= 2;
    if (btn(0, BTN_DOWN))  my += 2;

    // keep it on screen
    mx = clamp(mx, 0, SCREEN_W - mw);
    my = clamp(my, 16, SCREEN_H - mh);

    // did the boxes just touch THIS frame but not last frame? -> a little blip
    int touching = boxes_touch(mx, my, mw, mh, fx, fy, fw, fh);
    if (touching && !was_touching) note(72, INSTR_TRI, 4);
    was_touching = touching;
}

void draw() {
    cls(CLR_DARK_BLUE);

    int touching = boxes_touch(mx, my, mw, mh, fx, fy, fw, fh);

    // the fixed box turns red the moment something is inside it
    rectfill(fx, fy, fw, fh, touching ? CLR_RED : CLR_DARK_PURPLE);
    // the moving box you control
    rectfill(mx, my, mw, mh, CLR_GREEN);

    if (touching) {
        // compute the overlap rectangle (the intersection)
        int ix = max(mx, fx);                 // left edge of the shared box
        int iy = max(my, fy);                 // top edge
        int ir = min(mx + mw, fx + fw);       // right edge
        int ib = min(my + mh, fy + fh);       // bottom edge
        int iw = ir - ix;                     // its width
        int ih = ib - iy;                     // its height

        // paint the shared area in a third colour so you SEE how much they share
        rectfill(ix, iy, iw, ih, CLR_YELLOW);
        print(str("overlap: %d x %d", iw, ih), 4, 190, CLR_YELLOW);
    }

    // HUD: name the concept and show the live state
    print("AABB: push the green box (arrows)", 4, 4, CLR_WHITE);
    print(touching ? "TOUCHING" : "apart", 4, 178,
          touching ? CLR_YELLOW : CLR_LIGHT_GREY);
}
