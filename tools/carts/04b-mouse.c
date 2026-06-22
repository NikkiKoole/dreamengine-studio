#include "studio.h"

// 4b — THE MOUSE. The pointer is the keyboard's sibling (04-buttons). The calls:
//   mouse_x() / mouse_y()    where the cursor is (canvas pixels)
//   mouse_pressed(button)    true only on the frame a button goes down — a "click"
//   mouse_down(button)       true the whole time it's held — that's how you DRAG
//   mouse_released(button)   true only on the frame it comes back up
//   mouse_wheel()            scroll this frame: + up, - down, 0 if no scroll
//   buttons: MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE
//
// Try it: LEFT-drag the box · wheel ZOOMS it toward the cursor · LEFT-click empty space
//         drops a dot · RIGHT-click clears the dots · MIDDLE-click resets the box.

int  bx = 140, by = 84, bsize = 40;   // the draggable box
bool dragging = false;
int  grabx, graby;                    // where inside the box we grabbed (so it won't jump)

#define MAXDOT 80
int dotx[MAXDOT], doty[MAXDOT], ndot = 0;   // dots dropped with left-click

void update() {
    int mx = mouse_x(), my = mouse_y();
    bool over = mx >= bx && mx < bx + bsize && my >= by && my < by + bsize;

    // a CLICK (pressed = just this frame): grab the box, or drop a dot on empty space
    if (mouse_pressed(MOUSE_LEFT)) {
        if (over) { dragging = true; grabx = mx - bx; graby = my - by; }
        else if (ndot < MAXDOT) { dotx[ndot] = mx; doty[ndot] = my; ndot++; }
    }
    if (mouse_released(MOUSE_LEFT)) dragging = false;
    if (dragging) { bx = mx - grabx; by = my - graby; }   // HELD = drag: follow the mouse

    if (mouse_pressed(MOUSE_RIGHT))  ndot = 0;                            // right-click clears
    if (mouse_pressed(MOUSE_MIDDLE)) { bx = 140; by = 84; bsize = 40; }  // middle-click resets

    // wheel ZOOMS the box TOWARD THE CURSOR: scale it, then re-place it so the point that
    // was under the pointer stays under the pointer (the classic "zoom to cursor" trick).
    // (modrack does the same trick on the whole VIEW with camera_ex — see the camera tutorial.)
    float w = mouse_wheel();
    if (w != 0) {
        int old = bsize;
        bsize = (int)clamp(bsize + w * 4, 12, 120);
        float fx = (mx - bx) / (float)old;   // how far across the box the cursor sits (0..1)
        float fy = (my - by) / (float)old;
        bx = (int)(mx - fx * bsize);         // keep that same fraction under the cursor
        by = (int)(my - fy * bsize);
    }
}

void draw() {
    cls(CLR_DARK_BLUE);
    int mx = mouse_x(), my = mouse_y();
    bool over = mx >= bx && mx < bx + bsize && my >= by && my < by + bsize;

    for (int i = 0; i < ndot; i++) circfill(dotx[i], doty[i], 2, CLR_YELLOW);

    // box brightens on hover, brightest while dragged — feedback you can FEEL
    int col = dragging ? CLR_WHITE : over ? CLR_LIGHT_GREY : CLR_INDIGO;
    rectfill(bx, by, bsize, bsize, col);
    rect(bx, by, bsize, bsize, CLR_WHITE);

    print("THE MOUSE", 4, 4, CLR_WHITE);
    print("LEFT-drag the box", 4, 16, CLR_LIGHT_GREY);
    print("wheel: zoom box toward cursor", 4, 24, CLR_LIGHT_GREY);
    print("LEFT-click empty: drop a dot", 4, 32, CLR_LIGHT_GREY);
    print("RIGHT: clear    MIDDLE: reset", 4, 40, CLR_LIGHT_GREY);

    // the live API values, so you can watch them change
    print(str("mouse_x %d   mouse_y %d", mx, my), 4, SCREEN_H - 22, CLR_PEACH);
    print(str("L %s  M %s  R %s  dots %d",
              mouse_down(MOUSE_LEFT)   ? "down" : "up",
              mouse_down(MOUSE_MIDDLE) ? "down" : "up",
              mouse_down(MOUSE_RIGHT)  ? "down" : "up", ndot),
          4, SCREEN_H - 12, CLR_PEACH);

    // draw our own crosshair cursor at the pointer
    line(mx - 5, my, mx + 5, my, CLR_WHITE);
    line(mx, my - 5, mx, my + 5, CLR_WHITE);
}
