#include "studio.h"

int x = 152;
int y = 92;

void update() {
    if (btn(0, BTN_RIGHT)) x += 2;
    if (btn(0, BTN_LEFT))  x -= 2;
    if (btn(0, BTN_UP))    y -= 2;
    if (btn(0, BTN_DOWN))  y += 2;

    if (x < 0)              x = 0;
    if (x > SCREEN_W - 16)  x = SCREEN_W - 16;
    if (y < 0)              y = 0;
    if (y > SCREEN_H - 16)  y = SCREEN_H - 16;
}

void draw() {
    cls(CLR_BLACK);
    spr(0, x, y);
    print("use arrow keys", 86, 180, CLR_WHITE);
}
