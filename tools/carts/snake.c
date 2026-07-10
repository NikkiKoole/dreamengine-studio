/* de:meta
{
  "slug": "snake",
  "title": "snake",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "game"
  ],
  "teaches": [
    "grid-movement",
    "title-play-gameover-loop",
    "save-load-persistence"
  ],
  "lineage": "classic Snake; ring-buffer body on a fixed-step grid tick, buffered turn input preventing 180-degree reversal, speed ramps with length.",
  "genre": "arcade",
  "description": "Classic snake. Eat the flashing food to grow, avoid walls and your own tail. Speeds up as you get longer. Arrow keys to steer."
}
de:meta */
#include "studio.h"

// SNAKE — arrow keys to steer, eat food, don't hit walls or yourself

#define CELL   8
#define GW     (SCREEN_W / CELL)   // 40 columns
#define GH     (SCREEN_H / CELL)   // 25 rows
#define MXLEN  (GW * GH)           // 1000 max segments

static int  gx[MXLEN], gy[MXLEN]; // snake body ring buffer (tail → head)
static int  head, tail, snlen;

static int  dir_x, dir_y;   // current step direction
static int  nxt_x, nxt_y;   // buffered next direction (applied on each tick)

static int  fx, fy;         // food cell

static int  score, hiscore;
static bool over;
static int  tick, speed;    // speed = frames per snake step

// ---- helpers ----

static void place_food() {
    while (1) {
        int pfx = rnd(GW), pfy = rnd(GH);
        bool hit = false;
        for (int i = 0; i < snlen && !hit; i++) {
            int idx = (tail + i) % MXLEN;
            if (gx[idx]==pfx && gy[idx]==pfy) hit = true;
        }
        if (!hit) { fx=pfx; fy=pfy; return; }
    }
}

static void new_game() {
    // start with a 4-segment snake in the center, moving right
    snlen=4; tail=0; head=3;
    for (int i=0; i<4; i++) { gx[i]=GW/2-3+i; gy[i]=GH/2; }
    dir_x=1; dir_y=0;
    nxt_x=1; nxt_y=0;
    score=0; over=false; tick=0; speed=8;
    place_food();
}

void init() { hiscore=load(0); new_game(); }

void update() {
    if (over) {
        if (btnp(0,BTN_A)||btnp(0,BTN_B)) new_game();
        return;
    }

    // buffer input — prevent 180° reversal
    if      (btn(0,BTN_UP)    && dir_y==0) { nxt_x= 0; nxt_y=-1; }
    else if (btn(0,BTN_DOWN)  && dir_y==0) { nxt_x= 0; nxt_y= 1; }
    else if (btn(0,BTN_LEFT)  && dir_x==0) { nxt_x=-1; nxt_y= 0; }
    else if (btn(0,BTN_RIGHT) && dir_x==0) { nxt_x= 1; nxt_y= 0; }

    if (++tick < speed) return;
    tick = 0;

    dir_x=nxt_x; dir_y=nxt_y;

    int nhx = gx[head] + dir_x;
    int nhy = gy[head] + dir_y;

    // wall collision
    if (nhx<0||nhx>=GW||nhy<0||nhy>=GH) {
        over=true;
        note(36, INSTR_NOISE, 6);
        if (score>hiscore) { hiscore=score; save(0,hiscore); }
        return;
    }

    // self collision — skip tail segment when not eating (it moves away)
    bool eating = (nhx==fx && nhy==fy);
    int  start  = eating ? 0 : 1;
    for (int i=start; i<snlen; i++) {
        int idx=(tail+i)%MXLEN;
        if (gx[idx]==nhx && gy[idx]==nhy) {
            over=true;
            note(36, INSTR_NOISE, 6);
            if (score>hiscore) { hiscore=score; save(0,hiscore); }
            return;
        }
    }

    // advance head
    head=(head+1)%MXLEN;
    gx[head]=nhx; gy[head]=nhy;

    if (eating) {
        snlen++;
        score++;
        note(60+(score%12)*2, INSTR_SINE, 4);
        speed = max(3, 8 - score/4);
        place_food();
    } else {
        tail=(tail+1)%MXLEN;
    }
}

void draw() {
    cls(CLR_BLACK);

    // food — pulses between two colors
    int fc = blink(10) ? CLR_RED : CLR_ORANGE;
    rectfill(fx*CELL+1, fy*CELL+1, CELL-2, CELL-2, fc);

    // snake body — draw tail to head so head renders on top
    for (int i=0; i<snlen; i++) {
        int idx = (tail+i)%MXLEN;
        int px  = gx[idx]*CELL, py = gy[idx]*CELL;
        bool is_head = (idx==head);
        rectfill(px+1, py+1, CELL-2, CELL-2, is_head ? CLR_GREEN : CLR_MEDIUM_GREEN);
        if (is_head) {
            // eyes — two pixels on the leading face
            int ex1, ey1, ex2, ey2;
            if      (dir_x== 1) { ex1=px+5; ey1=py+2; ex2=px+5; ey2=py+5; }
            else if (dir_x==-1) { ex1=px+2; ey1=py+2; ex2=px+2; ey2=py+5; }
            else if (dir_y==-1) { ex1=px+2; ey1=py+2; ex2=px+5; ey2=py+2; }
            else                { ex1=px+2; ey1=py+5; ex2=px+5; ey2=py+5; }
            pset(ex1, ey1, CLR_BLACK);
            pset(ex2, ey2, CLR_BLACK);
        }
    }

    // HUD
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_right(str("BEST %d", hiscore), SCREEN_W-4, 3, CLR_YELLOW);

    if (over) {
        rectfill(SCREEN_W/2-64, SCREEN_H/2-22, 128, 52, CLR_BLACK);
        rect    (SCREEN_W/2-64, SCREEN_H/2-22, 128, 52, CLR_WHITE);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H/2-12, CLR_RED);
        print_centered(str("LENGTH %d", snlen), SCREEN_W/2, SCREEN_H/2+2, CLR_YELLOW);
        print_centered("Z to restart", SCREEN_W/2, SCREEN_H/2+14, CLR_LIGHT_GREY);
    }
}
