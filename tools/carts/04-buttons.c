/* de:meta
{
  "title": "4. press a button",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Read the keyboard with btn() and move a character around the screen.",
  "todo": [
    "Hook up a real gamepad controller and test that it works."
  ]
}
de:meta */
#include "studio.h"

int px = 152;
int py = 92;

void update() {
    if (btn(0, BTN_LEFT))  px -= 2;
    if (btn(0, BTN_RIGHT)) px += 2;
    if (btn(0, BTN_UP))    py -= 2;
    if (btn(0, BTN_DOWN))  py += 2;

    if (px < 8)             px = 8;
    if (px > SCREEN_W - 8) px = SCREEN_W - 8;
    if (py < 8)             py = 8;
    if (py > SCREEN_H - 8) py = SCREEN_H - 8;
}

void draw() {
    cls(CLR_DARK_GREEN);
    circfill(px, py, 8, CLR_YELLOW);
    print("wasd or arrow keys to move.", 4, 4, CLR_WHITE);
    print(str("x=%d  y=%d", px, py), 4, 14, CLR_LIGHT_GREY);
}
