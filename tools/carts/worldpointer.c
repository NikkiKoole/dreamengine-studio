#include "studio.h"

// Shows off the camera + the world-space mouse, the combo that makes mouse_world worth it:
//   camera_ex(x,y,zoom,angle) — scroll + ZOOM + ROTATE the whole view in one call
//   mouse_world_x/y           — clicks land on the world even under zoom/rotation,
//                               which would be a nasty inverse-transform to hand-roll
//   save_int/load_int         — remember a high score by name, no slot numbers

#define WORLD_W (SCREEN_W * 3)
#define MAX_FLAGS 64

int   fx[MAX_FLAGS], fy[MAX_FLAGS];
int   planted = 0;
int   best    = 0;
float scroll  = 0;          // camera left edge, in world space
float zoom    = 1.0f;
float angle   = 0.0f;       // degrees

void init() {
    best = load_int("most_flags", 0);   // name, not a magic slot number
}

void update() {
    if (btn(0, BTN_RIGHT)) scroll += 3;
    if (btn(0, BTN_LEFT))  scroll -= 3;
    scroll = mid(0, (int)scroll, WORLD_W - SCREEN_W);

    zoom += mouse_wheel() * 0.1f;        // wheel zooms
    if (zoom < 0.5f) zoom = 0.5f;
    if (zoom > 4.0f) zoom = 4.0f;
    if (key('Q')) angle -= 2;            // Q / E rotate the whole view
    if (key('E')) angle += 2;

    // set the camera HERE, in update(), so mouse_world_x/y() invert the SAME transform
    // we'll draw with. (draw() ends with camera(0,0) for the HUD; relying on that would
    // make clicks land wrong — and now, under zoom/rotation, wrong in a much worse way.)
    camera_ex((int)scroll, 0, zoom, angle);

    // plant a flag where the mouse points — in WORLD space. one line does the full
    // inverse of scroll + zoom + rotation; doing it by hand would be real matrix math.
    if (mouse_pressed(MOUSE_LEFT) && planted < MAX_FLAGS) {
        fx[planted] = mouse_world_x();
        fy[planted] = mouse_world_y();
        planted++;
        hit(72, INSTR_SQUARE, 3, 40);
        if (planted > best) { best = planted; save_int("most_flags", best); }
    }

    if (btnp(0, BTN_B)) planted = 0;     // clear (best stays saved)
}

void draw() {
    camera_ex((int)scroll, 0, zoom, angle);
    cls(CLR_DARK_BLUE);

    // ground + a few hills, in world coords — the camera scrolls/zooms/spins them
    rectfill(0, 150, WORLD_W, SCREEN_H - 150, CLR_DARK_GREEN);
    for (int i = 0; i < 12; i++) circfill(i * 120 + 40, 150, 30, CLR_GREEN);

    // the planted flags live in world space
    for (int i = 0; i < planted; i++) {
        line(fx[i], fy[i], fx[i], fy[i] - 12, CLR_BROWN);
        rectfill(fx[i], fy[i] - 12, 8, 5, CLR_RED);
    }

    // cursor dot at the world point the mouse is over — tracks even when zoomed/rotated
    circ(mouse_world_x(), mouse_world_y(), 3, CLR_WHITE);

    // ---- HUD: camera(0,0) = back to plain screen space, drawn over the world ----
    camera(0, 0);
    print(str("flags: %d", planted), 4, 4, CLR_WHITE);
    print(str("best: %d", best),     4, 14, CLR_YELLOW);
    print(str("zoom: %.1f", zoom),   4, 24, CLR_LIGHT_GREY);
    print("click plant   arrows scroll   wheel zoom", 4, 180, CLR_LIGHT_GREY);
    print("Q/E rotate   X clear", 4, 190, CLR_LIGHT_GREY);
}
