#include "studio.h"

int px, py, sx, sy, score;

void update() {
    if (btn(0, BTN_LEFT))  px -= 2;
    if (btn(0, BTN_RIGHT)) px += 2;
    if (btn(0, BTN_UP))    py -= 2;
    if (btn(0, BTN_DOWN))  py += 2;

    px = clamp(px, 0, SCREEN_W - 8);
    py = clamp(py, 16, SCREEN_H - 8);

    if (near(px + 4, py + 4, sx, sy, 12)) {
        score++;
        sx = rnd(SCREEN_W - 8);
        sy = rnd(SCREEN_H - 24) + 20;
        note(72, INSTR_TRI, 4);
    }
}

void draw() {
    cls(CLR_DARK_BLUE);

    // star
    circfill(sx, sy, 5, CLR_YELLOW);

    // player
    rectfill(px, py, 8, 8, CLR_GREEN);

    // hud
    rectfill(0, 0, SCREEN_W, 14, CLR_DARKER_BLUE);
    print("catch the star!", 4, 3, CLR_WHITE);
    print(str("score: %d", score), 200, 3, CLR_YELLOW);
}
