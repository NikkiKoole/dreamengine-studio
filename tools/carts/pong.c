/* de:meta
{
  "title": "pong",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop"
  ],
  "lineage": "Pong (1972) clone; paddle-offset spin on the ball, two-player local, scale-degree hit blips.",
  "genre": "sports",
  "homage": "Pong (1972)",
  "description": "2-player pong. First to 7 wins. Player 1: W/S — Player 2: up/down arrows."
}
de:meta */
#include "studio.h"

int de_players(void) { return 2; }   // 2-player → show the multiplayer menu/lobby

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
static bool  ready;

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
#ifdef DE_TRACE
    // full sim state per frame — the lockstep-netplay check (play.js netdemo)
    // diffs both machines' traces; any divergence here = desync
    watch("p1y",   "%d", p1y);
    watch("p2y",   "%d", p2y);
    watch("ball",  "%.2f,%.2f v%.2f,%.2f", bx, by, vx, vy);
    watch("score", "%d-%d", s1, s2);
#endif
    if (!ready) {
        // slot 5: a soft, warm blip — triangle wave, quick pluck, lowpass for roundness
        instrument(5, INSTR_TRI, 2, 55, 0, 45);
        instrument_filter(5, FILTER_LOW, 1400, 2);
        reset();
        ready = true;
    }
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

    // top / bottom walls — soft tick + a little kick
    if (by <= 1)                      { by = 1;                     vy = -vy; hit(64, 5, 2, 45); shake(1.0f); }
    if (by + BALL_SIZE >= SCREEN_H-1) { by = SCREEN_H-1-BALL_SIZE; vy = -vy; hit(59, 5, 2, 45); shake(1.0f); }

    // left paddle
    if (vx < 0 && boxes_touch((int)bx, (int)by, BALL_SIZE, BALL_SIZE, P1_X, p1y, PADDLE_W, PADDLE_H)) {
        bx = P1_X + PADDLE_W;
        vx = -vx;
        float off = (by + BALL_SIZE / 2.0f - p1y) / PADDLE_H - 0.5f;
        vy = off * 7.0f;
        // pitch rises toward the paddle edges — a little pentatonic blip
        hit(degree(SCALE_PENTA, 4, (int)((off + 0.5f) * 5)), 5, 4, 70);
        shake(2.5f);
    }

    // right paddle
    if (vx > 0 && boxes_touch((int)bx, (int)by, BALL_SIZE, BALL_SIZE, P2_X, p2y, PADDLE_W, PADDLE_H)) {
        bx = P2_X - BALL_SIZE;
        vx = -vx;
        float off = (by + BALL_SIZE / 2.0f - p2y) / PADDLE_H - 0.5f;
        vy = off * 7.0f;
        hit(degree(SCALE_PENTA, 4, (int)((off + 0.5f) * 5)), 5, 4, 70);
        shake(2.5f);
    }

    // score — scorer gets a soft arpeggio; the winning point gets a fuller chord
    if (bx + BALL_SIZE < 0) {
        s2++; over = s2 >= WIN;
        shake(over ? 6.0f : 4.0f);
        if (over) strum(60, CHORD_MAJ7, 5, 5, 70);
        else { strum(57, CHORD_MAJ, 5, 4, 45); serve(1); }
    }
    if (bx > SCREEN_W) {
        s1++; over = s1 >= WIN;
        shake(over ? 6.0f : 4.0f);
        if (over) strum(60, CHORD_MAJ7, 5, 5, 70);
        else { strum(64, CHORD_MAJ, 5, 4, 45); serve(-1); }
    }
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
        print_centered(msg, SCREEN_W/2, 90, CLR_YELLOW);
        print_centered("Z to replay", SCREEN_W/2, 104, CLR_LIGHT_GREY);
    }
}
