/* de:meta
{
  "title": "platform rails (slopes)",
  "status": "active",
  "created": "2026-06-03",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "state-machine"
  ],
  "lineage": "Teaches the Donkey Kong / Lode Runner rail model as an explicit tutorial contrast to the solid-space collision used in rects/paint/tiles; novel in using sloped girder segments so position is (girder, offset-along-girder) rather than a free 2D coordinate.",
  "genre": "platformer",
  "description": "The OTHER kind of platformer. The rects/paint/tiles carts use solid-space collision (free position, gravity always on, collision discovers the ground) which can only make flat ground. This is the rail model behind Donkey Kong, Lode Runner and BurgerTime: the player is bound to a girder, so position is 'which girder + how far along it' and height is just a formula (py = surface(pg, x)) — which means SLOPES come for free. Ladders connect the girders; the player is a run/climb/jump state machine. Run up the slanted girders, climb the ladders, reach the flag. Arrows/WASD move, up/down climb a ladder, Z to jump."
}
de:meta */
#include "studio.h"

// PLATFORM RAILS — the OTHER kind of platformer. The rects/paint/tiles carts use
// "solid-space" collision: the player has a free 2D position, gravity is always
// on, and collision DISCOVERS the ground by stopping you. That model can only
// ever give you flat, axis-aligned ground — no slopes.
//
// This is the RAIL model (Donkey Kong, Lode Runner, BurgerTime). The player is
// BOUND to a girder: your real position is "which girder (pg) + how far along it
// (px)", and the height is just a formula — py = surface(pg, px). A girder is a
// line, y = y0 + slope*(x - x0), so SLOPES come for free. Ladders are connectors
// that move you between girders. There's almost no physics: you don't fall, you
// follow the slope. Gravity only switches on during a jump.
//
// The player is a little state machine: RUN (on a girder) / CLIMB (on a ladder)
// / JUMP. Run up the slopes, climb the ladders, reach the flag at the top.
//
// Move: arrows / WASD.   Climb: up/down at a ladder.   Jump: Z.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING — edit + hot-reload ───────────────────────────────────────────────
#define RUN_SPD   58.0f    // px/sec along a girder
#define CLIMB_SPD 46.0f    // px/sec up a ladder
#define JUMP_V0  -120.0f   // jump launch velocity
#define GRAV      440.0f   // gravity while jumping
#define PH        12       // player height (px)

// ── the rails: girders are line segments (edges of the graph) ────────────────
// y = y0 + slope*(x - x0). Slopes alternate so you zig-zag up the tower.
#define NG 4
typedef struct { int x0, x1; float y0, slope; } Girder;
static const Girder gird[NG] = {
    {  16, 304, 182.0f, -0.060f },   // 0 bottom — rises to the right
    {  16, 304, 132.0f,  0.060f },   // 1        — rises to the left
    {  16, 304,  82.0f, -0.060f },   // 2        — rises to the right
    {  16, 304,  36.0f,  0.000f },   // 3 top    — flat, the flag sits here
};

static float surface(int g, float x) {
    if (x < gird[g].x0) x = gird[g].x0;
    if (x > gird[g].x1) x = gird[g].x1;
    return gird[g].y0 + gird[g].slope * (x - gird[g].x0);
}

// ── ladders: connectors between girder `lo` and `lo+1` at a given x ──────────
#define NL 3
typedef struct { int x, lo; } Ladder;
static const Ladder lad[NL] = {
    { 272, 0 },   // g0 -> g1 on the right
    {  48, 1 },   // g1 -> g2 on the left
    { 272, 2 },   // g2 -> g3 on the right
};
#define LAD_W 10
#define GOAL_X 150

enum { RUN, CLIMB, JUMP };

STATE {
    float px;          // how far along the current girder
    float py;          // surface height (derived, but kept for the jump arc)
    int   pg;          // which girder the player is on
    int   face;        // -1 / +1
    int   mode;        // RUN / CLIMB / JUMP
    int   lad_i;       // ladder being climbed
    float jvy, jbase;  // jump velocity + the surface height we left
    bool  win;
};

// is there a ladder going UP from this girder, near px?
static int ladder_up_here(void) {
    for (int i = 0; i < NL; i++)
        if (lad[i].lo == S->pg && (S->px - lad[i].x < 6 && lad[i].x - S->px < 6)) return i;
    return -1;
}
static int ladder_down_here(void) {
    for (int i = 0; i < NL; i++)
        if (lad[i].lo + 1 == S->pg && (S->px - lad[i].x < 6 && lad[i].x - S->px < 6)) return i;
    return -1;
}

static void spawn(void) {
    S->pg = 0; S->px = 28; S->py = surface(0, S->px);
    S->face = 1; S->mode = RUN; S->win = false;
}

void init(void) { spawn(); }

void update(void) {
#ifdef DE_TRACE
    watch("p", "pg=%d mode=%d px=%.0f py=%.0f win=%d", S->pg, S->mode, S->px, S->py, S->win);
#endif
    if (S->win) {
        if (btnp(0, BTN_A)) { spawn(); }
        return;
    }
    float d = dt();

    if (S->mode == RUN) {
        if (btn(0, BTN_LEFT))  { S->px -= RUN_SPD * d; S->face = -1; }
        if (btn(0, BTN_RIGHT)) { S->px += RUN_SPD * d; S->face =  1; }
        S->px = clamp(S->px, gird[S->pg].x0, gird[S->pg].x1);
        S->py = surface(S->pg, S->px);                       // <-- bound to the rail

        if (btn(0, BTN_UP)) {
            int u = ladder_up_here();
            if (u >= 0) { S->mode = CLIMB; S->lad_i = u; S->px = lad[u].x; note(64, INSTR_SINE, 2); }
        }
        if (btn(0, BTN_DOWN)) {
            int dn = ladder_down_here();
            if (dn >= 0) { S->mode = CLIMB; S->lad_i = dn; S->px = lad[dn].x; S->py -= 1; }
        }
        if (btnp(0, BTN_A)) { S->mode = JUMP; S->jvy = JUMP_V0; S->jbase = S->py; note(60, INSTR_SQUARE, 3); }

    } else if (S->mode == CLIMB) {
        if (btn(0, BTN_UP))   S->py -= CLIMB_SPD * d;
        if (btn(0, BTN_DOWN)) S->py += CLIMB_SPD * d;
        int topg = lad[S->lad_i].lo + 1, botg = lad[S->lad_i].lo;
        float ty = surface(topg, lad[S->lad_i].x), by = surface(botg, lad[S->lad_i].x);
        S->px = lad[S->lad_i].x;
        if (S->py <= ty) {                                   // reached the upper girder
            S->py = ty; S->pg = topg; S->mode = RUN;
            if (S->pg == NG - 1) { S->win = true; strum(72, CHORD_MAJ, INSTR_TRI, 4, 40); }
        } else if (S->py >= by) {                            // back down to the lower girder
            S->py = by; S->pg = botg; S->mode = RUN;
        }

    } else { // JUMP — the only time real physics runs
        S->jvy += GRAV * d;
        S->py  += S->jvy * d;
        S->px  += S->face * RUN_SPD * 0.6f * d;
        S->px   = clamp(S->px, gird[S->pg].x0, gird[S->pg].x1);
        float surf = surface(S->pg, S->px);
        if (S->jvy > 0 && S->py >= surf) { S->py = surf; S->mode = RUN; }   // land back on the rail
    }
}

// ── drawing ──────────────────────────────────────────────────────────────────
static void draw_girder(int g) {
    for (int x = gird[g].x0; x <= gird[g].x1; x++) {
        int y = (int)surface(g, x);
        rectfill(x, y, 1, 5, CLR_DARK_RED);
        pset(x, y, CLR_RED);
    }
    for (int x = gird[g].x0; x <= gird[g].x1; x += 12)       // rivets
        pset(x, (int)surface(g, x) + 2, CLR_ORANGE);
}

static void draw_ladder(int i) {
    int x = lad[i].x;
    int yt = (int)surface(lad[i].lo + 1, x), yb = (int)surface(lad[i].lo, x);
    line(x - LAD_W / 2, yt, x - LAD_W / 2, yb, CLR_LIGHT_PEACH);
    line(x + LAD_W / 2, yt, x + LAD_W / 2, yb, CLR_LIGHT_PEACH);
    for (int y = yt + 3; y < yb; y += 6) line(x - LAD_W / 2, y, x + LAD_W / 2, y, CLR_PEACH);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    for (int g = 0; g < NG; g++) draw_girder(g);
    for (int i = 0; i < NL; i++) draw_ladder(i);

    // goal flag on the top girder
    int gy = (int)surface(NG - 1, GOAL_X);
    line(GOAL_X, gy - 16, GOAL_X, gy, CLR_LIGHT_GREY);
    rectfill(GOAL_X + 1, gy - 16, 9, 6, blink(20) ? CLR_GREEN : CLR_YELLOW);

    // player
    int x = (int)S->px, top = (int)S->py - PH;
    if (S->mode == CLIMB) {
        rectfill(x - 3, top + 2, 6, 7, CLR_BLUE);
        circfill(x, top, 2, CLR_LIGHT_PEACH);
        int s = anim(2, 6, 0);
        rectfill(x - 3, top + 9, 2, 3, s ? CLR_DARK_BLUE : CLR_BLUE);
        rectfill(x + 1, top + 9, 2, 3, s ? CLR_BLUE : CLR_DARK_BLUE);
    } else {
        bool moving = btn(0, BTN_LEFT) || btn(0, BTN_RIGHT);
        int step = (S->mode == JUMP) ? 2 : (moving ? (anim(2, 8, 0) ? 2 : -2) : 0);
        rectfill(x - 3 - (step < 0 ? 1 : 0), top + 7, 3, 5, CLR_DARK_BLUE);
        rectfill(x + (step > 0 ? 1 : 0),     top + 7, 3, 5, CLR_DARK_BLUE);
        rectfill(x - 3, top + 1, 6, 7, CLR_RED);
        rectfill(x - 3, top + 4, 6, 4, CLR_BLUE);
        circfill(x, top - 1, 3, CLR_LIGHT_PEACH);
        rectfill(x - 3, top - 3, 6, 2, CLR_RED);
        pset(x + S->face * 2, top - 1, CLR_BLACK);
    }

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("run + climb to the flag", 4, 2, CLR_WHITE);
    print(str("girder %d", S->pg), SCREEN_W - 64, 2, CLR_LIGHT_GREY);
    if (S->win) {
        print_centered("YOU MADE IT!", SCREEN_W / 2, SCREEN_H / 2 - 6, CLR_YELLOW);
        print_centered("press Z to play again", SCREEN_W / 2, SCREEN_H / 2 + 4, CLR_WHITE);
    }
}
