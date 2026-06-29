/* de:meta
{
  "title": "breakout",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop"
  ],
  "lineage": "Breakout (1976) clone; paddle-position-to-angle reflection and nearest-face brick bounce, with row-pitched hit sounds.",
  "genre": "arcade",
  "homage": "Breakout (1976)",
  "description": "Classic brick-breaker. Rainbow rows of bricks, paddle angle controls the bounce direction. 3 lives. Left/right to move, A to launch."
}
de:meta */
#include "studio.h"

// breakout
// left/right to move paddle, A to launch

#define COLS  10
#define ROWS   6
#define BW    28    // brick width
#define BH     7    // brick height
#define BGAP   2    // gap between bricks
#define BOFF_X ((SCREEN_W - COLS * (BW + BGAP) + BGAP) / 2)
#define BOFF_Y  18
#define PW    40    // paddle width
#define PH     5    // paddle height
#define PY    (SCREEN_H - 14)
#define BS     4    // ball size

static int   bricks[ROWS][COLS];
static int   n_bricks;
static float px;
static float bx, by, vx, vy;
static bool  launched;
static int   lives, score;
static bool  game_over, won;

static int colors[ROWS] = {
    CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_PINK
};

static void reset_ball(void) {
    bx = px + PW/2 - BS/2;
    by = PY - BS - 2;
    vx = vy = 0;
    launched = false;
}

static void reset(void) {
    px = SCREEN_W/2 - PW/2;
    lives = 3; score = 0;
    game_over = won = false;
    n_bricks = ROWS * COLS;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            bricks[r][c] = 1;
    reset_ball();
}

void init(void) { reset(); }

void update(void) {
    if (game_over || won) {
        if (btnp(0, BTN_A)) reset();
        return;
    }

    // move paddle
    if (btn(0, BTN_LEFT))  px -= 3;
    if (btn(0, BTN_RIGHT)) px += 3;
    px = clamp(px, 0, SCREEN_W - PW);

    // ball sits on paddle until launched
    if (!launched) {
        bx = px + PW/2 - BS/2;
        if (btnp(0, BTN_A)) {
            launched = true;
            float angle = 270.0f + rnd_between(-30, 30);
            vx = dx(3.0f, angle);
            vy = dy(3.0f, angle);
        }
        return;
    }

    bx += vx;
    by += vy;

    // wall bounces
    if (bx < 0)             { bx = 0;             vx = -vx; hit(69, INSTR_SQUARE, 3, 25); }
    if (bx > SCREEN_W - BS) { bx = SCREEN_W - BS; vx = -vx; hit(69, INSTR_SQUARE, 3, 25); }
    if (by < 0)             { by = 0;             vy = -vy; hit(69, INSTR_SQUARE, 3, 25); }

    // paddle — angle depends on where ball hits
    if (vy > 0 &&
        by + BS >= PY && by <= PY + PH &&
        bx + BS >= px && bx <= px + PW) {
        float rel   = clamp((bx + BS/2.0f - px) / PW, 0.0f, 1.0f);
        float angle = lerp(225.0f, 315.0f, rel);
        vx = dx(3.0f, angle);
        vy = dy(3.0f, angle);
        by = PY - BS;
        hit(60, INSTR_SQUARE, 5, 35);   // paddle thud
    }

    // bricks
    bool brick_hit = false;
    for (int r = 0; r < ROWS && !brick_hit; r++) {
        for (int c = 0; c < COLS && !brick_hit; c++) {
            if (!bricks[r][c]) continue;
            int rx = BOFF_X + c * (BW + BGAP);
            int ry = BOFF_Y + r * (BH + BGAP);
            if (!boxes_touch((int)bx, (int)by, BS, BS, rx, ry, BW, BH)) continue;
            bricks[r][c] = 0;
            n_bricks--;
            score += (ROWS - r) * 10;
            // pitch rises toward the top rows
            hit(48 + (ROWS - 1 - r) * 5, INSTR_SQUARE, 6, 55);
            // reflect off nearest face
            int adx = abs((int)(bx + BS/2) - (rx + BW/2));
            int ady = abs((int)(by + BS/2) - (ry + BH/2));
            if (adx * BH > ady * BW) vx = -vx; else vy = -vy;
            brick_hit = true;
            if (n_bricks == 0) {
                won = true;
                // win fanfare — ascending arpeggio
                hit(60, INSTR_TRI, 7, 120);
                schedule(130, 64, INSTR_TRI, 7);
                schedule(260, 67, INSTR_TRI, 7);
                schedule(390, 72, INSTR_TRI, 7);
                schedule(520, 76, INSTR_TRI, 7);
            }
        }
    }

    // ball lost
    if (by > SCREEN_H) {
        hit(48, INSTR_SINE, 6, 180);
        schedule(200, 43, INSTR_SINE, 5);
        schedule(400, 36, INSTR_SINE, 4);
        if (--lives == 0) game_over = true;
        else reset_ball();
    }
}

void draw(void) {
    cls(CLR_BLACK);

    if (game_over || won) {
        print_centered(won ? "YOU WIN!" : "GAME OVER", SCREEN_W/2, SCREEN_H/2 - 10, won ? CLR_GREEN : CLR_RED);
        print_centered(str("score: %d", score), SCREEN_W/2, SCREEN_H/2 + 4, CLR_WHITE);
        print_centered("press A to play again", SCREEN_W/2, SCREEN_H/2 + 18, CLR_DARK_GREY);
        return;
    }

    // bricks
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (!bricks[r][c]) continue;
            int rx = BOFF_X + c * (BW + BGAP);
            int ry = BOFF_Y + r * (BH + BGAP);
            rectfill(rx, ry, BW, BH, colors[r]);
            line(rx, ry, rx + BW - 1, ry, CLR_WHITE);
        }
    }

    // paddle
    rectfill((int)px, PY, PW, PH, CLR_LIGHT_GREY);
    line((int)px, PY, (int)px + PW - 1, PY, CLR_WHITE);

    // ball
    rectfill((int)bx, (int)by, BS, BS, CLR_WHITE);

    // HUD
    print(str("score %d", score), 4, 2, CLR_WHITE);
    for (int i = 0; i < lives; i++)
        circfill(SCREEN_W - 7 - i * 10, 5, 3, CLR_LIGHT_GREY);

    if (!launched)
        print_centered("press A to launch", SCREEN_W/2, SCREEN_H - 26, CLR_DARK_GREY);
}
