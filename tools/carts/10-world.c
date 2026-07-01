/* de:meta
{
  "title": "10. build a world",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Paint tiles in the map editor. Your player walks through the world and the camera follows."
}
de:meta */
#include "studio.h"

float px = 32;
float py = 32;

void init(void) {
    touch_layout(TOUCH_ANALOG, 0);   // poke-only: free 8-way move, no buttons
}

void update() {
    float speed = 2;
    float nx = px, ny = py;

    if (btn(0, BTN_LEFT))  nx -= speed;
    if (btn(0, BTN_RIGHT)) nx += speed;
    if (btn(0, BTN_UP))    ny -= speed;
    if (btn(0, BTN_DOWN))  ny += speed;

    // simple wall collision using the map
    if (!touching_map((int)nx, (int)py, 16, 16)) px = nx;
    if (!touching_map((int)px, (int)ny, 16, 16)) py = ny;

    follow((int)px + 8, (int)py + 8, MAP_W * CELL_W, MAP_H * CELL_H);
}

void draw() {
    cls(CLR_DARK_GREEN);
    map(0, 0, 0, 0, MAP_W, MAP_H);
    spr(2, (int)px, (int)py);
    print("paint walls in the map tab.", 4, 4, CLR_WHITE);
    print("wasd or arrows to move.", 4, 14, CLR_LIGHT_GREY);
}
