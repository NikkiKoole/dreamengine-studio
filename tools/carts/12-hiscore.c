/* de:meta
{
  "title": "12. high score",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Mash buttons for 10 seconds. Your best score saves between runs with save() and load().",
  "todo": [
    "Optional: support mouse/touch."
  ]
}
de:meta */
#include "studio.h"

int score    = 0;
int hi       = 0;
bool playing = false;

void update() {
    hi = load(0);

    if (!playing) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) {
            score = 0;
            timer_reset();
            playing = true;
        }
        return;
    }

    if (timer() >= 10.0f) {
        if (score > hi) save(0, score);
        playing = false;
        return;
    }

    if (btnp(0, BTN_A) || btnp(0, BTN_B)) score++;
}

void draw() {
    cls(CLR_DARK_PURPLE);

    // header
    rectfill(0, 0, SCREEN_W, 18, CLR_DARKER_PURPLE);
    print("button masher", 4, 5, CLR_LIGHT_GREY);
    print(str("best: %d", hi), 240, 5, CLR_YELLOW);

    if (!playing) {
        print("press z or x to play!", 72, 76, CLR_WHITE);
        print("mash for 10 seconds.", 80, 92, CLR_LIGHT_GREY);
        print("your best saves between runs.", 44, 108, CLR_LIGHT_GREY);
        print("save(0, score)  /  load(0)", 60, 130, CLR_DARK_GREY);
        return;
    }

    int secs_left = 10 - (int)timer();

    print(str("score: %d", score), 4, 28, CLR_WHITE);
    print(str("time:  %d", secs_left), 4, 44, CLR_ORANGE);

    // timer bar
    float pct = 1.0f - timer() / 10.0f;
    rectfill(4, 60, (int)(312 * pct), 10, CLR_ORANGE);
    rect    (4, 60, 312, 10, CLR_DARK_GREY);

    print("mash z and x!", 104, 88, CLR_YELLOW);
}
