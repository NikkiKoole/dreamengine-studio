/* de:meta
{
  "slug": "maze",
  "title": "maze generator",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "maze-generation",
    "pathfinding"
  ],
  "lineage": "Standalone algorithm visualiser with no direct sibling cart; pairs recursive-backtracker carving with BFS flood-fill shortest-path solve in a single animated demo.",
  "description": "Carve a maze, then solve it — two classic algorithms on one screen. A recursive backtracker knocks down walls to random unvisited neighbours and backs up a stack at dead ends, guaranteeing every cell is reachable. Then a breadth-first flood spreads out ring by ring from the start; the moment it touches the exit, the came-from links trace the shortest path. Z builds a fresh maze and animates the carve + solve.",
  "todo": [
    "Add a cute pixel button (try a larger 24×24 or 32×32 — worth a shared API), including a big 'generate again' button."
  ]
}
de:meta */
#include "studio.h"

// MAZE — carve it, then solve it. Two classic algorithms on one screen.
//
// CARVING (recursive backtracker): stand in a cell, knock down a wall to a
// random unvisited neighbour, and move there. Dead end? Back up along your
// trail (a stack) until you find a new way. Every cell ends up reachable.
//
// SOLVING (breadth-first flood): from the start, step outward one ring at a
// time, remembering where each cell was reached from. The first time the flood
// touches the exit, walk the "came-from" links back — that's the shortest path.
//
//   Z   build a fresh maze and watch it carve + solve

#define GW   22
#define GH   13
#define CELL 12
#define OX   ((SCREEN_W - GW * CELL) / 2)
#define OY   22
#define IDX(x,y) ((y) * GW + (x))

static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
static const int BIT[4] = { 1, 2, 4, 8 }, OPP[4] = { 4, 8, 1, 2 };   // N E S W

static unsigned char wall[GW * GH];
static bool vis[GW * GH];
static int  stk[GW * GH], sp, cur;
static int  dist[GW * GH], came[GW * GH], bq[GW * GH], qh, qt;
static int  phase, target;   // 0 carve, 1 solve, 2 done

static void reset_all(void) {
    for (int i = 0; i < GW * GH; i++) { wall[i] = 15; vis[i] = false; dist[i] = -1; came[i] = -1; }
    sp = 0; cur = 0; vis[0] = true; stk[sp++] = 0; phase = 0; target = GW * GH - 1;
}

static bool carve_step(void) {
    if (sp == 0) return false;
    cur = stk[sp - 1];
    int cx = cur % GW, cy = cur / GW, cand[4], nc = 0;
    for (int d = 0; d < 4; d++) {
        int nx = cx + DX[d], ny = cy + DY[d];
        if (nx >= 0 && nx < GW && ny >= 0 && ny < GH && !vis[IDX(nx, ny)]) cand[nc++] = d;
    }
    if (nc == 0) { sp--; return true; }
    int d = cand[rnd(nc)], ni = IDX(cx + DX[d], cy + DY[d]);
    wall[cur] &= ~BIT[d]; wall[ni] &= ~OPP[d];
    vis[ni] = true; stk[sp++] = ni; cur = ni;
    return true;
}

static void start_solve(void) {
    for (int i = 0; i < GW * GH; i++) { dist[i] = -1; came[i] = -1; }
    qh = qt = 0; dist[0] = 0; bq[qt++] = 0; phase = 1;
}

static bool solve_step(void) {
    if (qh == qt) { phase = 2; return false; }
    int c = bq[qh++];
    if (c == target) { phase = 2; return false; }
    int cx = c % GW, cy = c / GW;
    for (int d = 0; d < 4; d++) {
        if (wall[c] & BIT[d]) continue;
        int nx = cx + DX[d], ny = cy + DY[d];
        if (nx < 0 || nx >= GW || ny < 0 || ny >= GH) continue;
        int ni = IDX(nx, ny);
        if (dist[ni] != -1) continue;
        dist[ni] = dist[c] + 1; came[ni] = c; bq[qt++] = ni;
    }
    return true;
}

void init(void) {
    reset_all();
    while (carve_step()) {}     // instant build for the thumbnail
    start_solve();
    while (solve_step()) {}
}

void update(void) {
    if (btnp(0, BTN_A)) { reset_all(); return; }   // animated rebuild on demand
    if (phase == 0) { for (int k = 0; k < 4; k++) if (!carve_step()) { start_solve(); break; } }
    else if (phase == 1) { for (int k = 0; k < 6; k++) if (!solve_step()) break; }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    static const int RAMP[4] = { CLR_BLUE_GREEN, CLR_TRUE_BLUE, CLR_DARK_BLUE, CLR_INDIGO };

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int i = IDX(x, y), px = OX + x * CELL, py = OY + y * CELL;
            if (phase >= 1 && dist[i] >= 0) rectfill(px, py, CELL, CELL, RAMP[dist[i] % 4]);
            else if (phase == 0 && vis[i])  rectfill(px, py, CELL, CELL, CLR_DARKER_PURPLE);
        }

    if (phase == 2)   // shortest path back from the exit
        for (int c = target; c != -1; c = came[c]) {
            int x = c % GW, y = c / GW;
            rectfill(OX + x * CELL + 2, OY + y * CELL + 2, CELL - 4, CELL - 4, CLR_YELLOW);
        }

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int i = IDX(x, y), px = OX + x * CELL, py = OY + y * CELL;
            if (wall[i] & 1) line(px, py, px + CELL, py, CLR_LIGHT_GREY);
            if (wall[i] & 2) line(px + CELL, py, px + CELL, py + CELL, CLR_LIGHT_GREY);
            if (wall[i] & 4) line(px, py + CELL, px + CELL, py + CELL, CLR_LIGHT_GREY);
            if (wall[i] & 8) line(px, py, px, py + CELL, CLR_LIGHT_GREY);
        }

    if (phase == 0) { int x = cur % GW, y = cur / GW; rectfill(OX + x * CELL + 3, OY + y * CELL + 3, CELL - 6, CELL - 6, CLR_RED); }
    rectfill(OX + 2, OY + 2, CELL - 4, CELL - 4, CLR_GREEN);
    if (phase != 2) { int tx = target % GW, ty = target / GW; rectfill(OX + tx * CELL + 2, OY + ty * CELL + 2, CELL - 4, CELL - 4, CLR_RED); }

    print(phase == 0 ? "carving (recursive backtracker)" : phase == 1 ? "solving (breadth-first flood)" : "solved - shortest path", 4, 4, CLR_WHITE);
    print("Z: new maze (watch it carve + solve)", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
