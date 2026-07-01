/* de:meta
{
  "title": "10b. camera and clip",
  "status": "active",
  "created": "2026-07-01",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "viewport-clipping"
  ],
  "lineage": "the manual-camera sibling of 10-world (which uses the follow() convenience helper instead of camera()/clip() directly)",
  "description": {
    "summary": "Pan a raw camera() by hand over a world bigger than the screen, and clip() a second view into a corner window.",
    "detail": "10-world's camera follows the player via follow() — a convenience wrapper. Here: camera(x, y) is driven directly by input to show what it actually does (shift everything drawn by -x,-y), camera(0,0) resets it before drawing a screen-fixed HUD, and clip(x,y,w,h) restricts drawing to a rect — used to pin an independent, differently-positioned view of the same world into a corner (a minimap/inset window).",
    "controls": "arrows/dpad pan the camera"
  }
}
de:meta */
#include "studio.h"

// 10B. CAMERA AND CLIP — the manual-camera sibling of 10-world.
//
// 10-world moved the camera with follow() (clamped auto-tracking of the player).
// Here the camera is driven BY HAND so you can see what it actually does:
// camera(x, y) shifts everything drawn after it by (-x, -y) — draw calls don't
// know or care, they just land somewhere else on screen. camera(0, 0) resets it,
// which is why a HUD is drawn AFTER that reset (else it would scroll with the world).
// clip(x, y, w, h) is the other half: a scissor rect that restricts drawing to a
// window — paired with a second camera(), it's how you pin an independent peek
// into the world (a minimap, a security-camera inset) onto a fixed corner.

#define CELL         40
#define WORLD_COLS   16
#define WORLD_ROWS   10
#define WORLD_W (CELL * WORLD_COLS)   // 640 — wider than the 320-wide screen
#define WORLD_H (CELL * WORLD_ROWS)   // 400 — taller than the 200-tall screen

static int cam_x = 0;
static int cam_y = 0;

// the same world, drawn however the caller's camera()/clip() currently frame it
void draw_world() {
    for (int r = 0; r < WORLD_ROWS; r++) {
        for (int c = 0; c < WORLD_COLS; c++) {
            int col = ((c + r) % 2) ? CLR_DARK_GREEN : CLR_DARK_BLUE;
            rectfill(c * CELL, r * CELL, CELL, CELL, col);
            rect(c * CELL, r * CELL, CELL, CELL, CLR_BLACK);
            print(str("%d,%d", c, r), c * CELL + 3, r * CELL + 14, CLR_WHITE);
        }
    }
}

void update() {
    int speed = 6;
    if (btn(0, BTN_LEFT))  cam_x -= speed;
    if (btn(0, BTN_RIGHT)) cam_x += speed;
    if (btn(0, BTN_UP))    cam_y -= speed;
    if (btn(0, BTN_DOWN))  cam_y += speed;
    cam_x = max(0, min(cam_x, WORLD_W - SCREEN_W));
    cam_y = max(0, min(cam_y, WORLD_H - SCREEN_H));
}

void draw() {
    cls(CLR_BLACK);

    camera(cam_x, cam_y);           // shift the world by (-cam_x, -cam_y)
    draw_world();

    camera(0, 0);                   // reset — the HUD below must NOT scroll
    rectfill(0, 0, SCREEN_W, 44, CLR_BLACK);   // backdrop, else world text bleeds through
    print("camera and clip.", 4, 4, CLR_WHITE);
    print("camera(x,y) shifts everything drawn -- arrows pan it", 4, 14, CLR_LIGHT_GREY);
    print(str("camera at %d,%d", cam_x, cam_y), 4, 24, CLR_LIGHT_GREY);
    print("clip(x,y,w,h) windows a SECOND, fixed view ->", 4, 36, CLR_LIGHT_GREY);

    // a clipped inset: the same world, a different fixed camera, pinned to a corner
    int ix = SCREEN_W - 84, iy = 4, iw = 80, ih = 60;
    clip(ix, iy, iw, ih);
    rectfill(ix, iy, iw, ih, CLR_DARKER_BLUE);
    // camera() shifts from the SCREEN origin, not the clip rect — subtract (ix,iy) too,
    // or the inset shows whatever world column happens to land under the corner instead
    // of the spot you asked for.
    int peek_x = WORLD_W / 2 - iw / 2 - ix;
    int peek_y = WORLD_H / 2 - ih / 2 - iy;
    camera(peek_x, peek_y);         // now really centered on the world's middle
    draw_world();
    camera(0, 0);
    clip(0, 0, 0, 0);                // disable clipping
    rect(ix, iy, iw, ih, CLR_WHITE); // frame it, unclipped
}
