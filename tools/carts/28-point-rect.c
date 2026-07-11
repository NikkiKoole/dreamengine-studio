/* de:meta
{
  "slug": "28-point-rect",
  "title": "28. is a point in a box?",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "collision-detection"
  ],
  "description": "point_in_box() — the tiny test every button uses to know your cursor is over it."
}
de:meta */
#include "studio.h"

/*
   THE ONE IDEA: point_in_box(px, py, bx, by, bw, bh) answers a yes/no question —
   is the point (px, py) sitting inside the rectangle at (bx, by) that is bw wide
   and bh tall? It returns a bool: true = inside, false = outside.

   This is the whole secret behind clicking a button. A button is just a box; the
   engine asks "is the mouse point inside that box?" every frame. If yes, it lights
   up (hover); if yes AND you click, it fires. Same test, every UI in the world.

   Move the cursor with the MOUSE, or nudge it with the ARROW KEYS. Watch the box
   light up and the label flip the instant the point crosses the edge.
*/

// our box — fixed in the middle of the screen
#define BX 120
#define BY 70
#define BW 80
#define BH 60

int cx, cy;          // the cursor point we test
int mx_last, my_last; // last mouse position, so we can tell mouse from keys apart
bool was_inside;     // were we inside LAST frame? (so we only tick on ENTRY)

void init(void) {
    cx = SCREEN_W / 2;
    cy = SCREEN_H / 2 + 30;   // start just below the box, clearly OUTSIDE
    mx_last = mouse_x();
    my_last = mouse_y();
    was_inside = false;
}

void update() {
    // Follow the mouse — but only when it actually moved. That way the arrow keys
    // still work when the mouse is sitting still (great on a keyboard-only setup).
    int mx = mouse_x();
    int my = mouse_y();
    if (mx != mx_last || my != my_last) {
        cx = mx;
        cy = my;
    }
    mx_last = mx;
    my_last = my;

    // Arrow keys nudge the same cursor point.
    if (btn(0, BTN_LEFT))  cx -= 2;
    if (btn(0, BTN_RIGHT)) cx += 2;
    if (btn(0, BTN_UP))    cy -= 2;
    if (btn(0, BTN_DOWN))  cy += 2;
    cx = clamp(cx, 0, SCREEN_W - 1);
    cy = clamp(cy, 0, SCREEN_H - 1);

    // THE TEST — one line, one bool answer.
    bool inside = point_in_box(cx, cy, BX, BY, BW, BH);

    // Play a tick only on the frame we ENTER the box (not every frame we're in it).
    if (inside && !was_inside) note(72, INSTR_TRI, 4);
    was_inside = inside;
}

void draw() {
    cls(CLR_DARK_BLUE);

    bool inside = point_in_box(cx, cy, BX, BY, BW, BH);

    // The box: filled bright when the point is inside, just an outline when it isn't.
    if (inside) rectfill(BX, BY, BW, BH, CLR_GREEN);
    else        rect(BX, BY, BW, BH, CLR_MEDIUM_GREY);

    // The cursor point — a little crosshair so you can see exactly where it is.
    int c = inside ? CLR_WHITE : CLR_YELLOW;
    line(cx - 4, cy, cx + 4, cy, c);
    line(cx, cy - 4, cx, cy + 4, c);

    // HUD: name the concept and show its live yes/no answer.
    print("point_in_box: the button test", 4, 4, CLR_WHITE);
    print("move the mouse or arrow keys", 4, 14, CLR_LIGHT_GREY);
    if (inside) print("INSIDE", 4, SCREEN_H - 12, CLR_GREEN);
    else        print("outside", 4, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
