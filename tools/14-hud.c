#include "studio.h"

int score = 0;
int lives = 3;
float spawn_t = 0;
int ex, ey;

void spawn() {
    ex = rnd_between(16, SCREEN_W - 16);
    ey = rnd_between(32, SCREEN_H - 16);
    spawn_t = now();
}

void update() {
    if (lives <= 0) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) { score = 0; lives = 3; spawn(); }
        return;
    }

    if (ex == 0) spawn();

    // star shrinks and disappears — catch it while it's big
    float age = now() - spawn_t;
    if (age > 2.5f) { lives--; spawn(); return; }

    if (near(ex, ey, SCREEN_W/2, SCREEN_H/2, 8 + (int)((2.5f - age) * 8))) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) { score++; spawn(); }
    }
}

void draw() {
    cls(CLR_DARK_BLUE);

    if (lives <= 0) {
        print_centered("game over!", SCREEN_H/2 - 12, CLR_RED);
        print_centered(str("score: %d", score), SCREEN_H/2 + 4, CLR_WHITE);
        print_centered("z or x to restart", SCREEN_H/2 + 20, CLR_LIGHT_GREY);
        return;
    }

    // the shrinking star
    float age = now() - spawn_t;
    int r = 8 + (int)((2.5f - age) * 8);
    circfill(ex, ey, r, CLR_YELLOW);

    // press z/x while near center to catch
    circfill(SCREEN_W/2, SCREEN_H/2, 8, CLR_GREEN);
    print_centered("z or x to catch!", SCREEN_H - 16, CLR_LIGHT_GREY);

    // HUD using print_centered and print_right
    rectfill(0, 0, SCREEN_W, 14, CLR_DARKER_BLUE);
    print_centered("catch the star", 3, CLR_LIGHT_GREY);
    print(str("lives: %d", lives), 4, 3, CLR_RED);
    print_right(str("score: %d", score), SCREEN_W - 4, 3, CLR_YELLOW);
}
