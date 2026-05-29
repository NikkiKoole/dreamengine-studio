#include "studio.h"

// 2-player pong
// player 1: W / S      player 2: up / down arrows
// first to 7 wins — press Z to restart

#define PADDLE_W   6
#define PADDLE_H   36
#define BALL_SIZE  4
#define P_SPEED    3
#define WIN        7

#define P1_X  8
#define P2_X  (SCREEN_W - 8 - PADDLE_W)

static int   p1y, p2y;
static float bx, by, vx, vy;
static int   s1, s2;
static bool  over;

static void serve(int dir) {
    bx = SCREEN_W / 2 - BALL_SIZE / 2;
    by = SCREEN_H / 2 - BALL_SIZE / 2;
    vx = 3.0f * dir;
    vy = rnd(2) ? 2.0f : -2.0f;
}

static void reset() {
    p1y  = SCREEN_H / 2 - PADDLE_H / 2;
    p2y  = SCREEN_H / 2 - PADDLE_H / 2;
    s1   = 0;
    s2   = 0;
    over = false;
    serve(1);
}

void update() {
    if (over) {
        if (btnp(0, BTN_A) || btnp(1, BTN_A)) reset();
        return;
    }

    // move paddles
    if (btn(0, BTN_UP)   && p1y > 1)                        p1y -= P_SPEED;
    if (btn(0, BTN_DOWN) && p1y + PADDLE_H < SCREEN_H - 1)  p1y += P_SPEED;
    if (btn(1, BTN_UP)   && p2y > 1)                        p2y -= P_SPEED;
    if (btn(1, BTN_DOWN) && p2y + PADDLE_H < SCREEN_H - 1)  p2y += P_SPEED;

    // move ball
    bx += vx;
    by += vy;

    // top / bottom walls
    if (by <= 1)                      { by = 1;                     vy = -vy; }
    if (by + BALL_SIZE >= SCREEN_H-1) { by = SCREEN_H-1-BALL_SIZE; vy = -vy; }

    // left paddle
    if (vx < 0 && boxes_touch((int)bx, (int)by, BALL_SIZE, BALL_SIZE, P1_X, p1y, PADDLE_W, PADDLE_H)) {
        bx = P1_X + PADDLE_W;
        vx = -vx;
        float hit = (by + BALL_SIZE / 2.0f - p1y) / PADDLE_H - 0.5f;
        vy = hit * 7.0f;
        sfx(2);
    }

    // right paddle
    if (vx > 0 && boxes_touch((int)bx, (int)by, BALL_SIZE, BALL_SIZE, P2_X, p2y, PADDLE_W, PADDLE_H)) {
        bx = P2_X - BALL_SIZE;
        vx = -vx;
        float hit = (by + BALL_SIZE / 2.0f - p2y) / PADDLE_H - 0.5f;
        vy = hit * 7.0f;
        sfx(2);
    }

    // score
    if (bx + BALL_SIZE < 0)  { s2++; sfx(3); over = s2 >= WIN; if (!over) serve(-1); }
    if (bx > SCREEN_W)       { s1++; sfx(3); over = s1 >= WIN; if (!over) serve(1);  }
}

void draw() {
    cls(CLR_BLACK);

    // dashed center line
    for (int y = 0; y < SCREEN_H; y += 10)
        rectfill(SCREEN_W / 2 - 1, y, 2, 5, CLR_DARK_GREY);

    // top / bottom borders
    line(0, 0, SCREEN_W - 1, 0, CLR_DARK_GREY);
    line(0, SCREEN_H - 1, SCREEN_W - 1, SCREEN_H - 1, CLR_DARK_GREY);

    // scores
    print(str("%d", s1), SCREEN_W / 2 - 18, 4, CLR_WHITE);
    print(str("%d", s2), SCREEN_W / 2 + 10, 4, CLR_WHITE);

    // paddles
    rectfill(P1_X, p1y, PADDLE_W, PADDLE_H, CLR_WHITE);
    rectfill(P2_X, p2y, PADDLE_W, PADDLE_H, CLR_WHITE);

    // ball
    rectfill((int)bx, (int)by, BALL_SIZE, BALL_SIZE, CLR_YELLOW);

    // controls hint — first 5 seconds
    if (frame() < 300) {
        print("W/S", P1_X + PADDLE_W + 3, SCREEN_H / 2 - 4, CLR_DARK_GREY);
        print("up/dn", P2_X - 42, SCREEN_H / 2 - 4, CLR_DARK_GREY);
    }

    // game over overlay
    if (over) {
        rectfill(90, 82, 140, 36, CLR_BLACK);
        rect(90, 82, 140, 36, CLR_WHITE);
        const char *msg = s1 >= WIN ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
        print_centered(msg,          90, CLR_YELLOW);
        print_centered("Z to replay", 104, CLR_LIGHT_GREY);
    }
}
