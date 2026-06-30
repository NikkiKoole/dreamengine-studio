/* de:meta
{
  "title": "pathfinding",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "pathfinding"
  ],
  "lineage": "Single priority-queue loop reused as BFS/Greedy/A* so the search frontier differences are visible side by side; hand-seeded serpentine walls.",
  "description": "Watch a search find its way around walls — the SAME loop runs three classic algorithms so you can see the difference. BFS floods evenly in every direction (ignores the goal); GREEDY rushes straight at the goal and gets fooled by walls; A* combines distance-so-far plus an estimate, so it's informed and optimal. Frontier cells glow bright, visited cells fade, the final path lights up yellow, and it re-animates on a loop. Move the cursor and press A to draw/erase walls, B to switch algorithm — and watch it re-solve live.",
  "todo": [
    "Support the mouse: drag start and end; add a wall-draw button and an eraser as cute pixel buttons.",
    "Add cute pixel buttons for the various path algorithms.",
    "Don't auto-restart when the path completes.",
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
#include "studio.h"

// PATHFINDING  — watch a search find its way around walls
// The same loop runs three classic algorithms so you can SEE the difference:
//   BFS    floods evenly in all directions (ignores the goal)
//   GREEDY rushes straight at the goal (and gets fooled by walls)
//   A*     combines distance-so-far + estimate, so it's informed AND optimal
// Frontier = bright, visited = dim, final path = yellow.
// Arrows / WASD: move cursor   A: toggle a wall   B: switch algorithm

#define GW    30
#define GH    18
#define TILE  10
#define OX    10
#define OY    4
#define N     (GW * GH)

enum { UNSEEN, OPEN, CLOSED };
enum { BFS, GREEDY, ASTAR };

static bool wall[N];
static int  status[N], g[N], parent[N], ord[N];
static bool inpath[N];
static int  start, goal, cursor, mode;
static int  seq, visited, path_len;
static bool done, found;
static float replay_at;
static int  cur_cd;

static const char *MODE_NAME[3] = { "BFS", "GREEDY", "A*" };

// ====================================================================

static int cx_(int i) { return i % GW; }
static int cy_(int i) { return i / GW; }
static int idx(int x, int y) { return y * GW + x; }
static int heur(int i) { return abs(cx_(i) - cx_(goal)) + abs(cy_(i) - cy_(goal)); }

static int prio(int i) {
    if (mode == BFS)    return ord[i];          // insertion order → FIFO
    if (mode == GREEDY) return heur(i);
    return g[i] + heur(i);                       // A*
}

static void restart_search() {
    for (int i = 0; i < N; i++) { status[i] = UNSEEN; g[i] = 99999; parent[i] = -1; inpath[i] = false; }
    seq = 0; visited = 0; path_len = 0; done = false; found = false;
    g[start] = 0; status[start] = OPEN; ord[start] = seq++;
}

static void reconstruct() {
    int c = goal;
    while (c != -1) { inpath[c] = true; path_len++; c = parent[c]; }
}

static void step() {
    if (done) return;
    // pick the open node with the lowest priority
    int best = -1, bestp = 0;
    for (int i = 0; i < N; i++)
        if (status[i] == OPEN && (best < 0 || prio(i) < bestp)) { best = i; bestp = prio(i); }

    if (best < 0) { done = true; found = false; replay_at = now() + 2.5f; return; }
    if (best == goal) { done = true; found = true; reconstruct(); replay_at = now() + 2.8f; note(76, INSTR_SINE, 3); return; }

    status[best] = CLOSED; visited++;
    int x = cx_(best), y = cy_(best);
    int nx[4] = { x - 1, x + 1, x, x }, ny[4] = { y, y, y - 1, y + 1 };
    for (int d = 0; d < 4; d++) {
        if (nx[d] < 0 || nx[d] >= GW || ny[d] < 0 || ny[d] >= GH) continue;
        int nb = idx(nx[d], ny[d]);
        if (wall[nb]) continue;
        int ng = g[best] + 1;
        if (status[nb] == UNSEEN || ng < g[nb]) {
            g[nb] = ng; parent[nb] = best; ord[nb] = seq++;
            if (status[nb] != CLOSED) status[nb] = OPEN;
        }
    }
}

static void seed_maze() {
    for (int i = 0; i < N; i++) wall[i] = false;
    // three serpentine barriers with offset gaps
    int cols[3] = { 8, 15, 22 };
    for (int c = 0; c < 3; c++) {
        int gap = (c % 2 == 0) ? GH - 3 : 2;            // gap low / high alternating
        for (int y = 0; y < GH; y++)
            if (y < gap || y > gap + 2) wall[idx(cols[c], y)] = true;
    }
    start = idx(2, GH / 2);
    goal  = idx(GW - 3, GH / 2);
    wall[start] = wall[goal] = false;
}

void init() {
    seed_maze();
    cursor = idx(GW / 2, GH / 2);
    mode = ASTAR;
    restart_search();
    while (!done) step();        // solve instantly once (nice first frame)
}

void update() {
    // move cursor
    int dx = 0, dy = 0;
    if (btn(0, BTN_LEFT))       dx = -1;
    else if (btn(0, BTN_RIGHT)) dx = 1;
    else if (btn(0, BTN_UP))    dy = -1;
    else if (btn(0, BTN_DOWN))  dy = 1;
    if (dx || dy) {
        if (cur_cd <= 0) {
            int nx = mid(0, cx_(cursor) + dx, GW - 1), ny = mid(0, cy_(cursor) + dy, GH - 1);
            cursor = idx(nx, ny); cur_cd = 6;
        }
    } else cur_cd = 0;
    if (cur_cd > 0) cur_cd--;

    if (btnp(0, BTN_A) && cursor != start && cursor != goal) {
        wall[cursor] = !wall[cursor];
        note(64, INSTR_SQUARE, 1);
        restart_search();
    }
    if (btnp(0, BTN_B)) {
        mode = (mode + 1) % 3;
        note(70, INSTR_SQUARE, 2);
        restart_search();
    }

    if (!done) { step(); step(); }               // animate ~2 expansions / frame
    else if (now() > replay_at) restart_search(); // loop the visualization
}

// ====================================================================

void draw() {
    cls(CLR_BLACK);

    for (int i = 0; i < N; i++) {
        int sx = OX + cx_(i) * TILE, sy = OY + cy_(i) * TILE;
        int col;
        if (wall[i])            col = CLR_DARK_GREY;
        else if (inpath[i])     col = CLR_YELLOW;
        else if (status[i] == OPEN)   col = CLR_BLUE;
        else if (status[i] == CLOSED) col = CLR_INDIGO;
        else                    col = CLR_DARKER_BLUE;
        rectfill(sx + 1, sy + 1, TILE - 1, TILE - 1, col);
    }

    // start / goal
    int ssx = OX + cx_(start) * TILE, ssy = OY + cy_(start) * TILE;
    int gsx = OX + cx_(goal) * TILE,  gsy = OY + cy_(goal) * TILE;
    rectfill(ssx + 1, ssy + 1, TILE - 1, TILE - 1, CLR_GREEN);
    rectfill(gsx + 1, gsy + 1, TILE - 1, TILE - 1, CLR_RED);
    print("S", ssx + 3, ssy + 2, CLR_BLACK);
    print("G", gsx + 3, gsy + 2, CLR_BLACK);

    // cursor
    int csx = OX + cx_(cursor) * TILE, csy = OY + cy_(cursor) * TILE;
    rect(csx, csy, TILE, TILE, CLR_WHITE);

    // HUD
    int hy = OY + GH * TILE + 2;
    print(str("ALGORITHM: %s", MODE_NAME[mode]), 6, hy, CLR_LIGHT_PEACH);
    print_right(str("visited %d", visited), SCREEN_W - 6, hy, CLR_BLUE);
    if (done && found) print(str("path %d", path_len), 150, hy, CLR_YELLOW);
    if (done && !found) print("NO PATH", 150, hy, CLR_RED);
    print("A:wall  B:algorithm  move:cursor", 6, hy + 9, CLR_DARK_GREY);
}
