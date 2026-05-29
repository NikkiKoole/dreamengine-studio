#include "studio.h"

int px = 152;
int py = 92;
int facing = 0;  // 0 = right, 1 = left

void update() {
    if (btn(0, BTN_LEFT))  { px -= 2; facing = 1; }
    if (btn(0, BTN_RIGHT)) { px += 2; facing = 0; }
    if (btn(0, BTN_UP))    py -= 2;
    if (btn(0, BTN_DOWN))  py += 2;

    if (px < 0)             px = 0;
    if (px > SCREEN_W - 16) px = SCREEN_W - 16;
    if (py < 0)             py = 0;
    if (py > SCREEN_H - 16) py = SCREEN_H - 16;
}

void draw() {
    cls(CLR_DARK_BLUE);

    // sprf flips horizontally when facing left
    sprf(1, px, py, facing, 0);

    print("draw your character in the", 4, 4, CLR_WHITE);
    print("sprites tab  ->  slot 1.", 4, 14, CLR_YELLOW);
    print("wasd or arrows to move.", 4, 24, CLR_LIGHT_GREY);
}
