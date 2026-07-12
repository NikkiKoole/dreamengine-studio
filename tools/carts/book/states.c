// Chapter-12 illustration: a game is really a few SCREENS it flips between — title,
// playing, game-over — plus one number that survives closing the game: the high score.
// This one plays itself through the loop so you can see all three.
#include "studio.h"

enum { TITLE, PLAY, OVER };
static int state, t, score, best, run, newbest, ready;

void init(void) {
    best = load_int("book_best", 0);               // read the saved high score (0 if none yet)
    state = TITLE; t = score = run = newbest = 0; ready = 1;
}

void update(void) {
    if (!ready) init();
    t++;
    if (state == TITLE) { if (t > 40) { state = PLAY; t = 0; score = 0; } }
    else if (state == PLAY) {
        score++;                                   // "playing" — the score ticks up
        int target = 10 + (run % 4) * 6;           // each run aims a bit different
        if (score >= target) {
            state = OVER; t = 0;
            newbest = score > best;
            if (newbest) { best = score; save_int("book_best", best); }  // remember it!
            run++;
        }
    } else { if (t > 55) { state = TITLE; t = 0; } }
}

static void big(const char *s, int y, int col) {
    print_scaled(s, SCREEN_W / 2 - text_width(s), y, col, 2);   // centred, 2× size
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    if (state == TITLE) {
        big("CATCH!", 52, CLR_YELLOW);
        print_centered("press A to start", SCREEN_W / 2, 104, CLR_LIGHT_GREY);
        print_centered(str("best   %d", best), SCREEN_W / 2, 130, CLR_GREEN);
    } else if (state == PLAY) {
        print(str("score  %d", score), 10, 10, CLR_YELLOW);
        print(str("best   %d", best), 10, 24, CLR_LIGHT_GREY);
        circfill(SCREEN_W / 2, 100 + (int)(sin_deg(frame() * 12) * 12), 12, CLR_RED); // a little life
        print_centered("(playing...)", SCREEN_W / 2, 150, CLR_DARK_GREEN);
    } else {
        big("GAME OVER", 46, CLR_RED);
        print_centered(str("score  %d", score), SCREEN_W / 2, 100, CLR_WHITE);
        print_centered(str("best   %d", best), SCREEN_W / 2, 118, CLR_YELLOW);
        if (newbest && blink(15)) print_centered("NEW BEST!", SCREEN_W / 2, 146, CLR_GREEN);
    }
}
