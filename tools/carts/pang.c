/* de:meta
{
  "title": "pang",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "save-load-persistence"
  ],
  "lineage": "Faithful demake of Capcom's Pang/Buster Bros (1989); ball-splitting arc with gravity and size-tiered bounce constants, no notable algorithmic novelty beyond the arcade loop.",
  "genre": "arcade",
  "homage": "Pang (1989)",
  "description": "Demake of the 1989 Capcom classic (Buster Bros). Big balls bounce around the arena under gravity. Fire your harpoon-wire straight up and a hit SPLITS a ball into two smaller, faster ones — medium into small, small into tiny, and the tiniest just pop. Clear them all to advance; a ball touching you costs a life. Each size bounces to its own height, so the screen fills with chaos as you break the big ones down. Left/Right move, A or Up fires (up to two wires at once)."
}
de:meta */
#include "studio.h"

// PANG  (Buster Bros)
// Bouncing balls drift around under gravity. Fire your harpoon-wire straight up:
// a hit splits a ball into two smaller, faster ones — and the smallest just pop.
// Clear them all to advance. A ball touching you costs a life.
// Left/Right: move    A or Up: fire wire (up to two at once)

#define LEFTX   4
#define RIGHTX  316
#define CEIL    14
#define FLOOR   176
#define MAXB    40
#define MAXW    2          // wires on screen at once
#define WIRE_SPD 6.0f
#define GRAV    0.12f

// indexed by size: 0 tiny, 1 small, 2 medium, 3 big
static const int   RAD[4]   = { 4, 7, 11, 16 };
static const float BV[4]    = { 2.4f, 3.1f, 3.9f, 4.7f };   // bounce-up speed
static const float VXS[4]   = { 1.4f, 1.2f, 1.0f, 0.8f };   // horizontal speed
static const int   PTS[4]   = { 50, 30, 20, 10 };

typedef struct { float x, y, vx, vy; int size, col; bool active; } Ball;
typedef struct { float x, tip; bool active; } Wire;

static Ball ball[MAXB];
static Wire wire[MAXW];
static float pxf;            // player x
static int   facing;
static int   lives, score, hiscore, level, state;   // 0 play, 1 over, 2 win
static float inv_until, msg_until;

// ====================================================================

static void add_ball(float x, float y, float vx, float vy, int size, int col) {
    for (int i = 0; i < MAXB; i++)
        if (!ball[i].active) { ball[i] = (Ball){ x, y, vx, vy, size, col, true }; return; }
}

static int count_balls() { int n = 0; for (int i = 0; i < MAXB; i++) if (ball[i].active) n++; return n; }

static void spawn_level() {
    for (int i = 0; i < MAXB; i++) ball[i].active = false;
    for (int i = 0; i < MAXW; i++) wire[i].active = false;
    int n = min(4, 1 + level / 2);
    int cols[4] = { CLR_RED, CLR_BLUE, CLR_GREEN, CLR_ORANGE };
    for (int i = 0; i < n; i++) {
        float x = 50 + rnd(220);
        add_ball(x, 60, (i & 1) ? VXS[3] : -VXS[3], -BV[3] * 0.5f, 3, cols[i % 4]);
    }
    pxf = SCREEN_W / 2;
    msg_until = now() + 1.6f;
}

static void new_game() { lives = 3; score = 0; level = 1; state = 0; spawn_level(); }

void init() { hiscore = load(0); new_game(); }

static void split(int i) {
    Ball b = ball[i];
    ball[i].active = false;
    score += PTS[b.size];
    if (b.size == 0) { note(72, INSTR_SQUARE, 2); return; }   // tiniest just pops
    int cs = b.size - 1;
    add_ball(b.x, b.y, -VXS[cs], -BV[cs], cs, b.col);
    add_ball(b.x, b.y,  VXS[cs], -BV[cs], cs, b.col);
    note(60 + cs * 4, INSTR_SQUARE, 3);
}

static void hurt() {
    lives--;
    note(36, INSTR_NOISE, 6);
    for (int i = 0; i < MAXW; i++) wire[i].active = false;
    if (lives <= 0) {
        state = 1;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
    } else {
        pxf = SCREEN_W / 2; inv_until = now() + 1.8f;
    }
}

void update() {
    if (state != 0) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) { if (state == 2) { level++; state = 0; spawn_level(); } else new_game(); }
        return;
    }

    if (btn(0, BTN_LEFT))  { pxf -= 2.2f; facing = -1; }
    if (btn(0, BTN_RIGHT)) { pxf += 2.2f; facing = 1; }
    pxf = clamp(pxf, LEFTX + 8, RIGHTX - 8);

    if (btnp(0, BTN_A) || btnp(0, BTN_UP)) {
        for (int i = 0; i < MAXW; i++)
            if (!wire[i].active) { wire[i] = (Wire){ pxf, FLOOR, true }; note(80, INSTR_SAW, 2); break; }
    }

    // wires rise; pop a ball they touch
    for (int w = 0; w < MAXW; w++) {
        if (!wire[w].active) continue;
        wire[w].tip -= WIRE_SPD;
        if (wire[w].tip <= CEIL) { wire[w].active = false; continue; }
        for (int i = 0; i < MAXB; i++) {
            if (!ball[i].active) continue;
            float dxw = ball[i].x - wire[w].x;
            if (dxw < 0) dxw = -dxw;
            if (dxw <= RAD[ball[i].size] && ball[i].y >= wire[w].tip - RAD[ball[i].size]) {
                wire[w].active = false;
                split(i);
                break;
            }
        }
    }

    // balls bounce around under gravity
    for (int i = 0; i < MAXB; i++) {
        Ball *b = &ball[i];
        if (!b->active) continue;
        int r = RAD[b->size];
        b->vy += GRAV;
        b->x += b->vx; b->y += b->vy;
        if (b->x - r < LEFTX)  { b->x = LEFTX + r;  b->vx =  VXS[b->size]; }
        if (b->x + r > RIGHTX) { b->x = RIGHTX - r; b->vx = -VXS[b->size]; }
        if (b->y - r < CEIL)   { b->y = CEIL + r;   b->vy = -b->vy; }
        if (b->y + r > FLOOR)  { b->y = FLOOR - r;  b->vy = -BV[b->size]; }

        if (now() > inv_until && distance((int)b->x, (int)b->y, (int)pxf, FLOOR - 7) < r + 6) { hurt(); return; }
    }

    if (count_balls() == 0) {
        state = 2;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
    }
}

// ====================================================================

static void draw_player(int x, int y, bool flash) {
    if (flash && blink(3)) return;
    rectfill(x - 4, y - 8, 8, 8, CLR_BLUE);          // body
    circfill(x, y - 11, 3, CLR_LIGHT_PEACH);          // head
    rectfill(x - 4, y - 14, 8, 2, CLR_RED);           // cap
    rectfill(x - 3, y, 2, 2, CLR_DARK_BLUE);          // legs
    rectfill(x + 1, y, 2, 2, CLR_DARK_BLUE);
    pset(x + facing * 2, y - 11, CLR_BLACK);
}

void draw() {
    cls(CLR_DARKER_BLUE);

    // arena walls
    rectfill(0, 0, SCREEN_W, CEIL, CLR_DARK_PURPLE);
    rectfill(0, FLOOR, SCREEN_W, SCREEN_H - FLOOR, CLR_BROWN);
    rectfill(0, CEIL, LEFTX, FLOOR - CEIL, CLR_DARK_PURPLE);
    rectfill(RIGHTX, CEIL, SCREEN_W - RIGHTX, FLOOR - CEIL, CLR_DARK_PURPLE);

    // wires
    for (int w = 0; w < MAXW; w++) {
        if (!wire[w].active) continue;
        int x = (int)wire[w].x;
        line(x, FLOOR, x, (int)wire[w].tip, CLR_LIGHT_GREY);
        trifill(x - 3, (int)wire[w].tip + 2, x, (int)wire[w].tip - 4, x + 3, (int)wire[w].tip + 2, CLR_YELLOW);
    }

    // balls
    for (int i = 0; i < MAXB; i++) {
        if (!ball[i].active) continue;
        int x = (int)ball[i].x, y = (int)ball[i].y, r = RAD[ball[i].size];
        circfill(x, y, r, ball[i].col);
        circ(x, y, r, CLR_WHITE);
        circfill(x - r / 3, y - r / 3, max(1, r / 4), CLR_WHITE);   // highlight
    }

    draw_player((int)pxf, FLOOR, now() < inv_until);

    // HUD
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_right(str("BEST %d", hiscore), SCREEN_W - 4, 3, CLR_YELLOW);
    print_centered(str("LVL %d", level), SCREEN_W/2, 3, CLR_LIGHT_GREY);
    for (int i = 0; i < lives; i++) circfill(60 + i * 9, 7, 3, CLR_RED);

    if (now() < msg_until) print_centered(str("LEVEL %d", level), SCREEN_W/2, SCREEN_H / 2 - 20, CLR_LIGHT_YELLOW);

    if (state == 1) {
        rectfill(SCREEN_W / 2 - 64, SCREEN_H / 2 - 24, 128, 50, CLR_BLACK);
        rect    (SCREEN_W / 2 - 64, SCREEN_H / 2 - 24, 128, 50, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H / 2 - 14, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 - 1, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H / 2 + 13, CLR_LIGHT_GREY);
    }
    if (state == 2) {
        rectfill(SCREEN_W / 2 - 64, SCREEN_H / 2 - 18, 128, 38, CLR_BLACK);
        rect    (SCREEN_W / 2 - 64, SCREEN_H / 2 - 18, 128, 38, CLR_GREEN);
        print_centered("CLEAR!", SCREEN_W/2, SCREEN_H / 2 - 9, CLR_GREEN);
        print_centered("Z for next level", SCREEN_W/2, SCREEN_H / 2 + 5, CLR_LIGHT_GREY);
    }
}
