#include "studio.h"

float px = 32;
float py = 32;

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

    // camera follows player, clamped to world bounds
    int world_w = MAP_W * CELL_W;
    int world_h = MAP_H * CELL_H;
    int cx = clamp((int)px + 8 - SCREEN_W / 2, 0, world_w - SCREEN_W);
    int cy = clamp((int)py + 8 - SCREEN_H / 2, 0, world_h - SCREEN_H);
    camera(cx, cy);
}

void draw() {
    cls(CLR_DARK_GREEN);
    map(0, 0, 0, 0, MAP_W, MAP_H);
    spr(2, (int)px, (int)py);
    print("paint walls in the map tab.", 4, 4, CLR_WHITE);
    print("wasd or arrows to move.", 4, 14, CLR_LIGHT_GREY);
}
