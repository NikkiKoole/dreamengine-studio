/* de:meta
{
  "title": "A* pathfinding",
  "status": "active",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "pathfinding"
  ],
  "description": "How a game finds its way around walls. A* always expands the most promising cell, scoring f = g + h: steps taken plus a guess (Manhattan distance) of steps remaining — that guess steers it toward the goal instead of flooding everywhere. Closed cells are explored, open cells are the frontier, yellow is the shortest path. Arrows move the cursor, A toggles a wall, B runs the search or returns to editing."
}
de:meta */
#include "studio.h"

// A* PATHFINDING — how a game finds its way around walls.
//
// A* explores the grid from the start, always expanding the cell that looks
// most promising: f = g + h, where g is the steps taken so far and h is a guess
// of the steps still to go (here, the straight-line "Manhattan" distance to the
// goal). That guess is what makes A* beat blind flooding — it heads toward the
// target instead of spreading everywhere. Closed cells are explored, open cells
// are the frontier, and the yellow line is the shortest path it found.
//
//   move cursor: arrows/WASD     A: toggle a wall     B: run / edit

#define GW   32
#define GH   18
#define CELL 9
#define OX   ((SCREEN_W - GW * CELL) / 2)
#define OY   18
#define IDX(x,y) ((y) * GW + (x))

static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };

static bool wall[GW * GH], inopen[GW * GH], closed[GW * GH];
static int  g[GW * GH], f[GW * GH], came[GW * GH];
static int  start, goal, curx, cury, phase;   // 0 edit, 1 search, 2 found, 3 no path
static bool found;

static int heur(int a, int b) { return abs(a % GW - b % GW) + abs(a / GW - b / GW); }

static void reset_search(void) {
    for (int i = 0; i < GW * GH; i++) { g[i] = f[i] = 1000000; came[i] = -1; inopen[i] = closed[i] = false; }
    g[start] = 0; f[start] = heur(start, goal); inopen[start] = true; phase = 1; found = false;
}

static bool astar_step(void) {
    int best = -1, bf = 2000000000;
    for (int i = 0; i < GW * GH; i++) if (inopen[i] && f[i] < bf) { bf = f[i]; best = i; }
    if (best < 0) { phase = 3; return false; }
    if (best == goal) { found = true; phase = 2; return false; }
    inopen[best] = false; closed[best] = true;
    int cx = best % GW, cy = best / GW;
    for (int d = 0; d < 4; d++) {
        int nx = cx + DX[d], ny = cy + DY[d];
        if (nx < 0 || nx >= GW || ny < 0 || ny >= GH) continue;
        int ni = IDX(nx, ny);
        if (wall[ni] || closed[ni]) continue;
        int tg = g[best] + 1;
        if (tg < g[ni]) { came[ni] = best; g[ni] = tg; f[ni] = tg + heur(ni, goal); inopen[ni] = true; }
    }
    return true;
}

static void preset(void) {
    for (int i = 0; i < GW * GH; i++) wall[i] = false;
    for (int y = 3; y < 14; y++) wall[IDX(10, y)] = true;
    for (int y = 4; y < 15; y++) wall[IDX(20, y)] = true;
    for (int x = 20; x < 28; x++) wall[IDX(x, 4)] = true;
    for (int x = 4; x < 11; x++)  wall[IDX(x, 13)] = true;
    start = IDX(2, 9); goal = IDX(29, 9); curx = 15; cury = 9;
}

void init(void) { preset(); reset_search(); while (astar_step()) {} }

void update(void) {
    if (phase == 0) {
        if (btnp(0, BTN_LEFT))  curx = max(0, curx - 1);
        if (btnp(0, BTN_RIGHT)) curx = min(GW - 1, curx + 1);
        if (btnp(0, BTN_UP))    cury = max(0, cury - 1);
        if (btnp(0, BTN_DOWN))  cury = min(GH - 1, cury + 1);
        int ci = IDX(curx, cury);
        if (btnp(0, BTN_A) && ci != start && ci != goal) wall[ci] = !wall[ci];
        if (btnp(0, BTN_B)) reset_search();
    } else {
        for (int k = 0; k < 3; k++) if (!astar_step()) break;
        if (btnp(0, BTN_B)) phase = 0;
    }
}

void draw(void) {
    cls(CLR_BLACK);
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int i = IDX(x, y), px = OX + x * CELL, py = OY + y * CELL, col = -1;
            if (wall[i])        col = CLR_DARK_GREY;
            else if (closed[i]) col = CLR_DARKER_PURPLE;
            else if (inopen[i]) col = CLR_BLUE_GREEN;
            if (col >= 0) rectfill(px, py, CELL - 1, CELL - 1, col);
        }
    if (phase == 2) for (int c = goal; c != -1; c = came[c]) {
        int x = c % GW, y = c / GW;
        rectfill(OX + x * CELL + 1, OY + y * CELL + 1, CELL - 3, CELL - 3, CLR_YELLOW);
    }
    int sx = start % GW, sy = start / GW, gx = goal % GW, gy = goal / GW;
    rectfill(OX + sx * CELL, OY + sy * CELL, CELL - 1, CELL - 1, CLR_GREEN);
    rectfill(OX + gx * CELL, OY + gy * CELL, CELL - 1, CELL - 1, CLR_RED);
    if (phase == 0) rect(OX + curx * CELL - 1, OY + cury * CELL - 1, CELL + 1, CELL + 1, CLR_WHITE);

    print(phase == 0 ? "EDIT: A toggles wall, B runs" :
          phase == 1 ? "searching..." :
          phase == 2 ? "path found! B to edit" : "no path! B to edit", 4, 4, CLR_WHITE);
    print("arrows: cursor   A: wall   B: run/edit", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
