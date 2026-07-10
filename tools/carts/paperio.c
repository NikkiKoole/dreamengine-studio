/* de:meta
{
  "slug": "paperio",
  "title": "paper.io",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "grid-movement",
    "title-play-gameover-loop"
  ],
  "lineage": "Clone of Voodoo's 2016 paper.io; the territory-capture rule (flood-fill from the border to find unclaimed enclosed cells) is the algorithmic core, paired with heuristic bot AI that tracks a home-centroid and targets rival trails.",
  "genre": "sandbox",
  "homage": "Paper.io (2016)",
  "description": "Territory capture on a grid. Leave your zone and a trail follows you — loop back to your own color and a flood fill claims everything you enclosed. Cross your OWN trail and you die; cut a rival's trail and THEY die. Three AI rivals expand and cut each other too. First to 60% wins, best run saved. Arrows / WASD to steer (you move on your own)."
}
de:meta */
#include "studio.h"

// PAPER.IO
// Leave your zone and a trail follows you. Loop back to your own colour and
// everything you enclosed is captured. Cross your OWN trail and you die —
// but cut a rival's trail and THEY die. Grab the most ground.
// Arrows / WASD to steer. You move on your own. Reach 60% to win.

#define CELL    4
#define GW      80          // grid width  (320 / 4)
#define GH      50          // grid height (200 / 4)
#define CELLS   (GW * GH)
#define NP      4           // 1 human + 3 bots
#define STEP    4           // frames between grid steps
#define WIN_PCT 60

static const int DX[4] = {  0, 1, 0, -1 };   // up, right, down, left
static const int DY[4] = { -1, 0, 1,  0 };

// colours per player: solid territory, bright trail
static const int TERR [NP] = { CLR_TRUE_BLUE, CLR_DARK_RED,  CLR_DARK_GREEN, CLR_BROWN  };
static const int TRAIL[NP] = { CLR_BLUE,      CLR_RED,       CLR_GREEN,      CLR_ORANGE };

typedef struct {
    int   x, y, dir, next_dir;
    int   hx, hy;         // home centroid (bot AI steers back here)
    int   trail_len, target_len;
    bool  has_trail, alive, is_bot;
    float respawn_at;
} Player;

static unsigned char owner[CELLS];   // 0 = empty, else player id (p+1)
static unsigned char trail[CELLS];   // 0 = none,  else player id (p+1)
static unsigned char outside[CELLS]; // flood-fill scratch
static int           fstack[CELLS], f_top;

static Player pl[NP];
static int    area[NP];
static int    state;       // 0 play, 1 game over
static int    winner;      // -1 none, else player index
static int    best_pct;

static int idx(int x, int y) { return y * GW + x; }
static bool inb(int x, int y) { return x >= 0 && x < GW && y >= 0 && y < GH; }

// ====================================================================

static void recompute_areas() {
    long sx[NP] = {0}, sy[NP] = {0};
    for (int p = 0; p < NP; p++) area[p] = 0;
    for (int i = 0; i < CELLS; i++) {
        int o = owner[i];
        if (o) { int p = o - 1; area[p]++; sx[p] += i % GW; sy[p] += i / GW; }
    }
    for (int p = 0; p < NP; p++)
        if (area[p] > 0) { pl[p].hx = sx[p] / area[p]; pl[p].hy = sy[p] / area[p]; }
}

static void fpush(int x, int y, int id) {
    if (!inb(x, y)) return;
    int i = idx(x, y);
    if (owner[i] == id || outside[i]) return;
    outside[i] = 1; fstack[f_top++] = i;
}

// close the loop: trail + everything it encloses becomes player p's land
static void capture(int p) {
    int id = p + 1;
    for (int i = 0; i < CELLS; i++) if (trail[i] == id) { owner[i] = id; trail[i] = 0; }

    for (int i = 0; i < CELLS; i++) outside[i] = 0;
    f_top = 0;
    for (int x = 0; x < GW; x++) { fpush(x, 0, id); fpush(x, GH - 1, id); }
    for (int y = 0; y < GH; y++) { fpush(0, y, id); fpush(GW - 1, y, id); }
    while (f_top > 0) {
        int i = fstack[--f_top], x = i % GW, y = i / GW;
        fpush(x - 1, y, id); fpush(x + 1, y, id);
        fpush(x, y - 1, id); fpush(x, y + 1, id);
    }
    for (int i = 0; i < CELLS; i++)
        if (owner[i] != id && !outside[i]) owner[i] = id;

    recompute_areas();
    hit(70, INSTR_SINE, 3, 90);
}

static void spawn_block(int p, int cx, int cy, int dir) {
    int id = p + 1;
    for (int y = cy - 2; y <= cy + 2; y++)
        for (int x = cx - 2; x <= cx + 2; x++)
            if (inb(x, y)) owner[idx(x, y)] = id;
    pl[p].x = cx; pl[p].y = cy; pl[p].dir = dir; pl[p].next_dir = dir;
    pl[p].hx = cx; pl[p].hy = cy;
    pl[p].has_trail = false; pl[p].trail_len = 0; pl[p].target_len = 6;
    pl[p].alive = true;
}

static void kill_player(int p) {
    int id = p + 1;
    for (int i = 0; i < CELLS; i++) if (trail[i] == id) trail[i] = 0;
    pl[p].has_trail = false; pl[p].trail_len = 0;

    if (!pl[p].is_bot) {
        state = 1; winner = -1;
        int pct = area[0] * 100 / CELLS;
        if (pct > best_pct) { best_pct = pct; save(0, best_pct); }
        note(36, INSTR_NOISE, 6);
    } else {
        for (int i = 0; i < CELLS; i++) if (owner[i] == id) owner[i] = 0;
        pl[p].alive = false;
        pl[p].respawn_at = now() + 1.4f;
        recompute_areas();
        note(44, INSTR_NOISE, 4);
    }
}

static void reset_game() {
    for (int i = 0; i < CELLS; i++) { owner[i] = 0; trail[i] = 0; }
    for (int p = 0; p < NP; p++) pl[p].is_bot = (p != 0);
    spawn_block(0, 12, GH - 12, 0);
    spawn_block(1, GW - 12, 12, 2);
    spawn_block(2, 12, 12, 1);
    spawn_block(3, GW - 12, GH - 12, 3);
    recompute_areas();
    state = 0; winner = -1;
}

void init() {
    best_pct = load(0);
    reset_game();
}

// ====================================================================
// bot AI — pick a non-suicidal direction, wander out then loop home
// ====================================================================

static void bot_think(int p) {
    Player *o = &pl[p];
    int id = p + 1, opp = (o->dir + 2) % 4;
    bool returning = o->has_trail && o->trail_len >= o->target_len;

    int best = -1, best_score = -100000;
    for (int d = 0; d < 4; d++) {
        if (d == opp) continue;
        int nx = o->x + DX[d], ny = o->y + DY[d];
        if (!inb(nx, ny)) continue;
        int ni = idx(nx, ny);
        if (trail[ni] == id) continue;            // own trail = death

        int s = (d == o->dir) ? 3 : 0;            // momentum
        s += rnd(3);
        if (trail[ni] != 0) s += 8;               // cut a rival's trail!
        if (returning) {
            if (owner[ni] == id) s += 40;         // get home & close the loop
            int cur = abs(o->x - o->hx) + abs(o->y - o->hy);
            int nxt = abs(nx   - o->hx) + abs(ny  - o->hy);
            s += (cur - nxt) * 4;
        } else {
            if (owner[ni] != id) s += 4;          // venture out to claim ground
            int cur = abs(o->x - o->hx) + abs(o->y - o->hy);
            if (cur > 16 && owner[ni] == id) s += 6;   // don't stray too far
        }
        if (s > best_score) { best_score = s; best = d; }
    }
    o->next_dir = (best >= 0) ? best : o->dir;
}

static void step_player(int p) {
    Player *o = &pl[p];
    if (!o->alive) return;
    o->dir = o->next_dir;
    int nx = o->x + DX[o->dir], ny = o->y + DY[o->dir];
    if (!inb(nx, ny)) { kill_player(p); return; }
    int ni = idx(nx, ny), id = p + 1;

    if (trail[ni] != 0) {                 // someone's trail is here
        int q = trail[ni] - 1;
        kill_player(q);
        if (q == p) return;               // we ran into our own trail
    }

    o->x = nx; o->y = ny;
    if (owner[ni] == id) {
        if (o->has_trail) { capture(p); o->has_trail = false; o->trail_len = 0; }
    } else {
        if (!o->has_trail) o->target_len = 5 + rnd(10);
        trail[ni] = id; o->has_trail = true; o->trail_len++;
    }
}

void update() {
    if (state == 1) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) reset_game();
        return;
    }

    // steer the human (no instant reversing)
    int d = pl[0].dir;
    if (btn(0, BTN_LEFT)  && d != 1) pl[0].next_dir = 3;
    if (btn(0, BTN_RIGHT) && d != 3) pl[0].next_dir = 1;
    if (btn(0, BTN_UP)    && d != 2) pl[0].next_dir = 0;
    if (btn(0, BTN_DOWN)  && d != 0) pl[0].next_dir = 2;

    // respawn dead bots
    for (int p = 1; p < NP; p++)
        if (!pl[p].alive && now() > pl[p].respawn_at) {
            int cx = rnd_between(6, GW - 6), cy = rnd_between(6, GH - 6);
            spawn_block(p, cx, cy, rnd(4));
            recompute_areas();
        }

    if (frame() % STEP == 0) {
        for (int p = 1; p < NP; p++) if (pl[p].alive) bot_think(p);
        for (int p = 0; p < NP; p++) step_player(p);

        // someone reached the goal?
        for (int p = 0; p < NP; p++)
            if (area[p] * 100 / CELLS >= WIN_PCT) {
                state = 1; winner = p;
                int pct = area[0] * 100 / CELLS;
                if (pct > best_pct) { best_pct = pct; save(0, best_pct); }
            }
    }
}

// ====================================================================
// drawing
// ====================================================================

void draw() {
    cls(CLR_DARKER_BLUE);

    // territory
    for (int i = 0; i < CELLS; i++)
        if (owner[i]) {
            int x = i % GW, y = i / GW;
            rectfill(x * CELL, y * CELL, CELL, CELL, TERR[owner[i] - 1]);
        }
    // trails (drawn over territory)
    for (int i = 0; i < CELLS; i++)
        if (trail[i]) {
            int x = i % GW, y = i / GW;
            rectfill(x * CELL, y * CELL, CELL, CELL, TRAIL[trail[i] - 1]);
        }
    // heads
    for (int p = 0; p < NP; p++) {
        if (!pl[p].alive) continue;
        int hx = pl[p].x * CELL, hy = pl[p].y * CELL;
        rectfill(hx - 1, hy - 1, CELL + 2, CELL + 2, CLR_WHITE);
        rectfill(hx, hy, CELL, CELL, TRAIL[p]);
    }

    // HUD — your score big, rivals as a little bar of squares
    int pct = area[0] * 100 / CELLS;
    print(str("YOU %d%%", pct), 4, 3, CLR_WHITE);
    print_right(str("BEST %d%%", best_pct), SCREEN_W - 4, 3, CLR_YELLOW);
    for (int p = 1; p < NP; p++) {
        int bw = area[p] * 60 / CELLS + 1;
        rectfill(110 + (p - 1) * 30, 4, bw, 6, TERR[p]);
    }

    if (state == 1) {
        rectfill(SCREEN_W / 2 - 72, SCREEN_H / 2 - 28, 144, 58, CLR_BLACK);
        rect    (SCREEN_W / 2 - 72, SCREEN_H / 2 - 28, 144, 58, CLR_WHITE);
        if (winner == 0)        print_centered("YOU WIN!", SCREEN_W/2, SCREEN_H / 2 - 18, CLR_GREEN);
        else if (winner > 0)    print_centered("A RIVAL WON", SCREEN_W/2, SCREEN_H / 2 - 18, CLR_RED);
        else                    print_centered("YOU CRASHED", SCREEN_W/2, SCREEN_H / 2 - 18, CLR_RED);
        print_centered(str("you claimed %d%%", pct), SCREEN_W/2, SCREEN_H / 2 - 2, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H / 2 + 14, CLR_LIGHT_GREY);
    }
}
