/* de:meta
{
  "title": "falling sand",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "cellular-automata"
  ],
  "description": "A powder toy from a handful of per-cell rules. Each cell is one material: sand falls and slides down diagonals so it piles into slopes (and sinks through water), water falls then spreads sideways to find its level, stone never moves. No pile or puddle is ever drawn on purpose — the shapes emerge from grains each making one tiny local decision, the same idea as the fire and ripple carts. WASD move the cursor, hold Z to paint, X cycles material (sand/water/stone/eraser).",
  "todo": [
    "Add cute sprite buttons to select what you draw; make them work with the mouse."
  ]
}
de:meta */
#include "studio.h"

// FALLING SAND — a powder toy from a handful of per-cell rules.
//
// Every cell is one material. Each frame, scanning bottom-up:
//   SAND  falls straight down; if blocked, it slides to an open diagonal — so
//         it piles into neat slopes. It also sinks through water.
//   WATER falls too, but when it can't, it spreads sideways to find its level.
//   STONE never moves (your walls and ledges).
// No pile or puddle is ever drawn on purpose — the shapes emerge from grains
// each making one tiny local decision. Same idea as the fire and ripple carts.
//
//   WASD: move cursor    Z: paint (hold)    X: change material

#define GW   160
#define GH   94
#define CELL 2
enum { EMPTY, SAND, WATER, STONE };

static unsigned char cells[GW * GH];
static int cx = GW / 2, cy = 16, mat = SAND;
static const char *MNAME[4] = { "eraser", "sand", "water", "stone" };
static const int   MCOL[4]  = { CLR_BLACK, CLR_ORANGE, CLR_BLUE, CLR_DARK_GREY };

static void swp(int i, int j) { unsigned char t = cells[i]; cells[i] = cells[j]; cells[j] = t; }

static void step(void) {
    for (int y = GH - 2; y >= 0; y--) {
        int dir = ((y + frame()) & 1) ? 1 : -1;
        for (int xi = 0; xi < GW; xi++) {
            int x = dir > 0 ? xi : GW - 1 - xi;
            int i = y * GW + x, below = i + GW;
            int m = cells[i];
            if (m == SAND) {
                if (cells[below] == EMPTY || cells[below] == WATER) { swp(i, below); continue; }
                bool okl = x > 0      && (cells[below - 1] == EMPTY || cells[below - 1] == WATER);
                bool okr = x < GW - 1 && (cells[below + 1] == EMPTY || cells[below + 1] == WATER);
                if (okl && okr) swp(i, rnd(2) ? below - 1 : below + 1);
                else if (okl) swp(i, below - 1);
                else if (okr) swp(i, below + 1);
            } else if (m == WATER) {
                if (cells[below] == EMPTY) { swp(i, below); continue; }
                bool okl = x > 0      && cells[below - 1] == EMPTY;
                bool okr = x < GW - 1 && cells[below + 1] == EMPTY;
                if (okl && okr) swp(i, rnd(2) ? below - 1 : below + 1);
                else if (okl) swp(i, below - 1);
                else if (okr) swp(i, below + 1);
                else {                                   // level out sideways
                    bool sl = x > 0      && cells[i - 1] == EMPTY;
                    bool sr = x < GW - 1 && cells[i + 1] == EMPTY;
                    if (sl && sr) swp(i, dir > 0 ? i + 1 : i - 1);
                    else if (sl) swp(i, i - 1);
                    else if (sr) swp(i, i + 1);
                }
            }
        }
    }
}

static void paint(int px, int py, int m) {
    for (int dy = -3; dy <= 3; dy++)
        for (int dx = -3; dx <= 3; dx++) {
            if (dx * dx + dy * dy > 9) continue;
            int x = px + dx, y = py + dy;
            if (x >= 0 && x < GW && y >= 0 && y < GH) cells[y * GW + x] = m;
        }
}

void init(void) {
    for (int i = 0; i < GW * GH; i++) cells[i] = EMPTY;
    for (int x = 0; x < GW; x++) { cells[(GH - 1) * GW + x] = STONE; cells[(GH - 2) * GW + x] = STONE; }
    for (int x = 18; x < 72; x++) { int yy = 70 - (x - 18) / 4; cells[yy * GW + x] = STONE; cells[(yy + 1) * GW + x] = STONE; }
    for (int x = 96; x < 152; x++) { int yy = 48 + (x - 96) / 6; cells[yy * GW + x] = STONE; }
    for (int y = 4; y < 26; y++) for (int x = 38; x < 56; x++) cells[y * GW + x] = SAND;
    for (int y = 4; y < 24; y++) for (int x = 106; x < 132; x++) cells[y * GW + x] = WATER;
    for (int i = 0; i < 150; i++) step();
}

void update(void) {
    if (btn(0, BTN_LEFT))  cx = max(0, cx - 2);
    if (btn(0, BTN_RIGHT)) cx = min(GW - 1, cx + 2);
    if (btn(0, BTN_UP))    cy = max(0, cy - 2);
    if (btn(0, BTN_DOWN))  cy = min(GH - 1, cy + 2);
    if (btnp(0, BTN_B)) mat = (mat + 1) % 4;
    if (btn(0, BTN_A))  paint(cx, cy, mat);
    step();
}

void draw(void) {
    cls(CLR_BLACK);
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int m = cells[y * GW + x];
            if (m == EMPTY) continue;
            int h = (x * 13 + y * 7) & 3;
            int col = m == SAND  ? (h == 0 ? CLR_YELLOW : CLR_ORANGE)
                    : m == WATER ? (h == 0 ? CLR_BLUE : CLR_TRUE_BLUE)
                                 : (h == 0 ? CLR_LIGHT_GREY : CLR_DARK_GREY);
            rectfill(x * CELL, y * CELL, CELL, CELL, col);
        }
    circ(cx * CELL, cy * CELL, 7, MCOL[mat] == CLR_BLACK ? CLR_LIGHT_GREY : MCOL[mat]);

    print("FALLING SAND", 4, 4, CLR_WHITE);
    print_right("WASD move  Z paint  X swap", SCREEN_W - 4, 4, CLR_DARK_GREY);
    print(str("material: %s", MNAME[mat]), 4, SCREEN_H - 9, MCOL[mat] == CLR_BLACK ? CLR_LIGHT_GREY : MCOL[mat]);
}
