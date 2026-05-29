#include "studio.h"

int px, py, sx, sy, ex, ey, score;
int dead = 0;

void reset() {
    px = 152; py = 92;
    sx = rnd(SCREEN_W - 8); sy = rnd(SCREEN_H - 24) + 20;
    ex = 0;   ey = SCREEN_H / 2;
    score = 0; dead = 0;
}

void update() {
    if (dead) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) reset();
        return;
    }

    if (btn(0, BTN_LEFT))  px -= 2;
    if (btn(0, BTN_RIGHT)) px += 2;
    if (btn(0, BTN_UP))    py -= 2;
    if (btn(0, BTN_DOWN))  py += 2;
    px = clamp(px, 0, SCREEN_W - 8);
    py = clamp(py, 16, SCREEN_H - 8);

    // enemy chases player
    float a = angle_to(ex, ey, px, py);
    ex += (int)dx(1, a);
    ey += (int)dy(1, a);

    // catch star
    if (near(px + 4, py + 4, sx, sy, 12)) {
        score++;
        sx = rnd(SCREEN_W - 8); sy = rnd(SCREEN_H - 24) + 20;
        note(72, INSTR_TRI, 4);
    }

    // enemy catches player
    if (near(px + 4, py + 4, ex, ey, 10)) {
        dead = 1;
        note(36, INSTR_NOISE, 5);
    }
}

void draw() {
    cls(CLR_DARK_BLUE);

    if (dead) {
        print("game over!", 108, 80, CLR_RED);
        print(str("score: %d", score), 112, 96, CLR_WHITE);
        print("press z or x to restart.", 60, 120, CLR_LIGHT_GREY);
        return;
    }

    circfill(sx, sy, 5, CLR_YELLOW);
    rectfill(px, py, 8, 8, CLR_GREEN);
    circfill(ex, ey, 6, CLR_RED);

    rectfill(0, 0, SCREEN_W, 14, CLR_DARKER_BLUE);
    print("catch stars, dodge the enemy!", 4, 3, CLR_WHITE);
    print(str("score: %d", score), 224, 3, CLR_YELLOW);
}
