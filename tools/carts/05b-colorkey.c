/* de:meta
{
  "title": "5b. colorkey",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Make sprite backgrounds transparent with colorkey(). Step-by-step: see the pink box, make it vanish, float over any background."
}
de:meta */
#include "studio.h"

// Tutorial: colorkey()
// 4 steps — press Z to advance, X to restart from step 3.

static int  step   = 0;
static bool key_on = false;

static void enter(int s) {
    step = s;
    key_on = false;
    // set colorkey once per step — never rebuild in draw()
    if (s == 1 || s == 2) colorkey(CLR_PINK);
    else colorkey(-1);
}

void update() {
    if (step < 3) {
        if (btnp(0, BTN_A)) enter(step + 1);
    } else {
        if (btnp(0, BTN_A)) {
            key_on = !key_on;
            colorkey(key_on ? CLR_PINK : -1);
        }
        if (btnp(0, BTN_B)) enter(0);
    }
}

void draw() {
    cls(CLR_DARK_BLUE);

    // ── step 0: the problem ──────────────────────────────────
    if (step == 0) {
        print("Sprites have a background", 10, 10, CLR_WHITE);
        print("When you draw a sprite it comes", 10, 28, CLR_LIGHT_GREY);
        print("painted on a color — like pink.", 10, 38, CLR_LIGHT_GREY);
        print("That color shows up in your", 10, 48, CLR_LIGHT_GREY);
        print("game even when you don't want it!", 10, 58, CLR_LIGHT_GREY);

        spr(1, 152, 90);  // pink box clearly visible

        print("           ^ pink box!", 10, 108, CLR_PINK);

        print("Z: next -->", SCREEN_W - 80, SCREEN_H - 12, CLR_YELLOW);
    }

    // ── step 1: the fix ──────────────────────────────────────
    if (step == 1) {
        // colorkey(CLR_PINK) is already ON from enter()
        // trick: fake "without" by drawing a pink rect first,
        // then the sprite — transparent pink pixels reveal the rect below.

        print("colorkey() to the rescue", 10, 10, CLR_WHITE);

        // left: "without colorkey"
        print("without:", 48, 35, CLR_RED);
        rectfill(72, 46, 16, 16, CLR_PINK);  // fake pink background
        spr(1, 72, 46);

        // right: "with colorkey"
        print("colorkey(CLR_PINK);", 160, 35, CLR_GREEN);
        spr(1, 232, 46);

        print("Pick one color to skip.", 10, 80, CLR_LIGHT_GREY);
        print("Pixels that color become", 10, 90, CLR_LIGHT_GREY);
        print("invisible — like magic!", 10, 100, CLR_LIGHT_GREY);
        print("Use it for any background color.", 10, 110, CLR_LIGHT_GREY);

        print("Z: next -->", SCREEN_W - 80, SCREEN_H - 12, CLR_YELLOW);
    }

    // ── step 2: floating over any background ─────────────────
    if (step == 2) {
        // colorkey(CLR_PINK) still on
        rectfill(0,    0,  160, 100, CLR_DARK_GREEN);
        rectfill(160,  0,  160, 100, CLR_DARK_PURPLE);
        rectfill(0,   100, 160, 100, CLR_BROWN);
        rectfill(160, 100, 160, 100, CLR_DARK_BLUE);

        spr(1, 152, 92);

        print("It works on any background!", 10, 10, CLR_WHITE);
        print("No more pink box.", 10, 150, CLR_LIGHT_GREY);
        print("Your sprite floats cleanly", 10, 160, CLR_LIGHT_GREY);
        print("wherever you put it.", 10, 170, CLR_LIGHT_GREY);

        print("Z: try it! -->", SCREEN_W - 100, SCREEN_H - 12, CLR_YELLOW);
    }

    // ── step 3: free play ────────────────────────────────────
    if (step == 3) {
        rectfill(30,  30, 130, 90,  CLR_DARK_GREEN);
        rectfill(130, 55,  90, 90,  CLR_ORANGE);
        rectfill(70,  90, 120, 70,  CLR_BLUE);

        spr(1, 148, 80);

        print("Your turn!", 10, 10, CLR_WHITE);

        if (key_on) {
            print("colorkey: ON",  10, 28, CLR_GREEN);
            print("pink = gone",   10, 38, CLR_LIGHT_GREY);
        } else {
            print("colorkey: OFF", 10, 28, CLR_RED);
            print("pink = solid",  10, 38, CLR_LIGHT_GREY);
        }

        print("Z: toggle colorkey", 10, SCREEN_H - 20, CLR_LIGHT_GREY);
        print("X: restart",         10, SCREEN_H - 10, CLR_LIGHT_GREY);
    }
}
