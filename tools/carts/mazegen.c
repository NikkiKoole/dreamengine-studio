/* de:meta
{
  "title": "maze maker",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "maze-generation",
    "algorithm-visualization"
  ],
  "lineage": "Standalone algorithm demo with no direct cart ancestor; shows randomized DFS backtracker and randomized Prim side-by-side with live stack/frontier highlighting.",
  "description": "Carve a perfect maze, step by step, with two classic algorithms side by side — watch the difference. The BACKTRACKER (randomized depth-first search) dives deep and backtracks off a stack, leaving long winding corridors; PRIM grows outward evenly from a frontier, leaving lots of short dead-ends. Both guarantee exactly one path between any two cells. The current cell glows yellow and the live stack/frontier is highlighted so you can see how each one decides where to go next. Loops forever. A regenerates, B switches algorithm."
}
de:meta */
#include "studio.h"

// MAZE GENERATION  — carve a perfect maze, step by step
// Both algorithms guarantee exactly one path between any two cells; they just
// FEEL different to watch:
//   BACKTRACKER (randomized DFS) — dives deep, long winding corridors, backtracks
//   PRIM        (randomized)     — grows outward evenly, lots of short dead-ends
// The "current" cell is yellow; the DFS stack / Prim frontier is highlighted so
// you can see how each one decides where to go next. Loops forever.
// A: regenerate    B: switch algorithm

#define CW   20
#define CH   12
#define TILE 14
#define OX   20
#define OY   6
#define N    (CW * CH)

enum { BACKTRACK, PRIM };
static const char *MNAME[2] = { "BACKTRACKER (DFS)", "PRIM" };

// wall bits per cell: N=1 E=2 S=4 W=8 (set = wall present)
static int  walls[N];
static bool visited[N], in_stack[N], in_front[N];
static int  stack[N], stop;        // DFS stack
static int  front[N], fcount;      // Prim frontier
static int  mode, current, made;
static bool done;
static float replay_at;

static const int DX[4] = { 0, 1, 0, -1 };
static const int DY[4] = { -1, 0, 1, 0 };

// ====================================================================

static int cxx(int i) { return i % CW; }
static int cyy(int i) { return i / CW; }
static int nbr(int i, int d) {
    int x = cxx(i) + DX[d], y = cyy(i) + DY[d];
    if (x < 0 || x >= CW || y < 0 || y >= CH) return -1;
    return y * CW + x;
}
static void carve(int a, int b, int d) {        // remove the wall between a and its d-neighbour b
    walls[a] &= ~(1 << d);
    walls[b] &= ~(1 << ((d + 2) % 4));
}

static void restart() {
    for (int i = 0; i < N; i++) { walls[i] = 15; visited[i] = in_stack[i] = in_front[i] = false; }
    stop = fcount = made = 0; done = false;
    int start = rnd(N);
    visited[start] = true; made = 1; current = start;
    if (mode == BACKTRACK) {
        stack[stop++] = start; in_stack[start] = true;
    } else {
        for (int d = 0; d < 4; d++) { int nb = nbr(start, d); if (nb >= 0 && !in_front[nb]) { front[fcount++] = nb; in_front[nb] = true; } }
    }
}

void init() {
    mode = BACKTRACK;
    restart();
    while (!done) {                              // pre-generate once for a clean first frame
        // inline one step (same as step() below)
        extern void step_once();
        step_once();
    }
}

void step_once() {
    if (done) return;
    if (mode == BACKTRACK) {
        if (stop == 0) { done = true; replay_at = now() + 2.5f; return; }
        int cur = stack[stop - 1];
        current = cur;
        int opts[4], n = 0;
        for (int d = 0; d < 4; d++) { int nb = nbr(cur, d); if (nb >= 0 && !visited[nb]) opts[n++] = d; }
        if (n == 0) { in_stack[cur] = false; stop--; return; }
        int d = opts[rnd(n)], nb = nbr(cur, d);
        carve(cur, nb, d);
        visited[nb] = true; made++;
        stack[stop++] = nb; in_stack[nb] = true;
        note(60 + cyy(nb) % 12, INSTR_SQUARE, 1);
    } else {
        if (fcount == 0) { done = true; replay_at = now() + 2.5f; return; }
        int fi = rnd(fcount), cell = front[fi];
        // connect to a random already-in-maze neighbour
        int ins[4], ni = 0;
        for (int d = 0; d < 4; d++) { int nb = nbr(cell, d); if (nb >= 0 && visited[nb]) ins[ni++] = d; }
        if (ni > 0) { int d = ins[rnd(ni)]; carve(cell, nbr(cell, d), d); }
        visited[cell] = true; made++; current = cell;
        in_front[cell] = false;
        front[fi] = front[--fcount];             // swap-remove from frontier
        for (int d = 0; d < 4; d++) { int nb = nbr(cell, d); if (nb >= 0 && !visited[nb] && !in_front[nb]) { front[fcount++] = nb; in_front[nb] = true; } }
        note(60 + cxx(cell) % 12, INSTR_SQUARE, 1);
    }
}

void update() {
    if (btnp(0, BTN_A)) restart();
    if (btnp(0, BTN_B)) { mode = (mode + 1) % 2; restart(); }

    if (!done) { step_once(); step_once(); }
    else if (now() > replay_at) restart();
}

// ====================================================================

void draw() {
    cls(CLR_BLACK);

    for (int i = 0; i < N; i++) {
        int cx = cxx(i), cy = cyy(i);
        int sx = OX + cx * TILE, sy = OY + cy * TILE;
        int col;
        if (i == current && !done) col = CLR_YELLOW;
        else if (mode == BACKTRACK && in_stack[i]) col = CLR_TRUE_BLUE;
        else if (mode == PRIM && in_front[i])       col = CLR_DARK_PURPLE;
        else if (visited[i])                        col = CLR_BLUE_GREEN;
        else continue;                              // unvisited stays black

        // cell body (inset so the 4px gap forms walls)
        rectfill(sx + 2, sy + 2, TILE - 4, TILE - 4, col);
        // open passages bridge the gap to the neighbour
        if (!(walls[i] & 1)) rectfill(sx + 2, sy - 2, TILE - 4, 4, col);          // N
        if (!(walls[i] & 2)) rectfill(sx + TILE - 2, sy + 2, 4, TILE - 4, col);   // E
        if (!(walls[i] & 4)) rectfill(sx + 2, sy + TILE - 2, TILE - 4, 4, col);   // S
        if (!(walls[i] & 8)) rectfill(sx - 2, sy + 2, 4, TILE - 4, col);          // W
    }

    // HUD
    int hy = OY + CH * TILE + 4;
    print(str("ALGORITHM: %s", MNAME[mode]), 6, hy, CLR_LIGHT_PEACH);
    print_right(done ? "done" : str("cells %d/%d", made, N), SCREEN_W - 6, hy, done ? CLR_GREEN : CLR_BLUE);
    print("A: regenerate    B: switch algorithm", 6, hy + 10, CLR_DARK_GREY);
}
