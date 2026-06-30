/* de:meta
{
  "title": "tiny city",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "cellular-automata"
  ],
  "lineage": "Homage to SimCity (1989); the R/C/I demand feedback loop where road-adjacent zones grow or shrink each tick based on supply/demand imbalance is the conceptual core — a simple CA-style simulation.",
  "genre": "simulation",
  "homage": "SimCity (1989)",
  "description": "A little SimCity, mouse-driven. Pick a tool from the bottom bar, then click & drag on the map to build. Zone land Residential, Commercial or Industrial and connect it with Roads — zoned land only develops next to a road. Watch the R/C/I demand bars up top: build what is in demand and the city grows; taxes from buildings fund more building. Left-drag = build, right-drag = bulldoze, A cycles tools.",
  "todo": [
    "Smarter tile-neighbor checking so roads orient correctly based on their neighbors."
  ]
}
de:meta */
#include "studio.h"

// ── tiny city builder ─────────────────────────────────────────
// A little SimCity. Use the MOUSE:
//   • pick a tool from the bar along the bottom
//   • click & drag in the map to build
//
// Zone land Residential (R), Commercial (C) or Industrial (I),
// then connect it with Roads — zoned land only develops when it
// touches a road. Watch the little R/C/I demand bars in the top
// bar: build what's in demand and the city grows. Taxes from
// developed buildings pay for more building.
//
// This cart shows off the mouse API: mouse_x/mouse_y give a live
// hover highlight (no button needed), mouse_down paints while held.

// ── layout (pixels) ───────────────────────────────────────────
#define GX     4        // map origin x
#define GY     13       // map origin y (below the top HUD)
#define CELL   12       // cell size
#define COLS   26
#define ROWS   14
#define BAR_Y  182      // tool bar top

// ── tile types ────────────────────────────────────────────────
#define T_GRASS 0
#define T_ROAD  1
#define T_RES   2
#define T_COM   3
#define T_IND   4
#define T_PARK  5

#define NTOOLS  6
static const int tools[NTOOLS] = { T_GRASS, T_ROAD, T_RES, T_COM, T_IND, T_PARK };
static const char *labels[NTOOLS] = { "clear", "road", "R", "C", "I", "park" };

#define TICK_FRAMES 40   // frames per simulated month

static int  tile[ROWS][COLS];
static int  lvl [ROWS][COLS];   // development level 0..3 for zoned tiles
static int  money;
static int  months;
static int  tool;               // currently selected tile type
static int  simctr;
static bool ready = false;

// stats (recomputed each tick) + demand (shown as bars)
static int  housing, jobsC, jobsI, pop;
static int  rDem, cDem, iDem;

// ── helpers ───────────────────────────────────────────────────
static bool is_zone(int t) { return t == T_RES || t == T_COM || t == T_IND; }

static int cost_of(int t) {
    if (t == T_GRASS) return 2;     // bulldoze
    if (t == T_ROAD)  return 10;
    if (t == T_PARK)  return 20;
    return 25;                      // any zone
}

static int zone_color(int t) {
    return t == T_RES ? CLR_GREEN : t == T_COM ? CLR_BLUE : CLR_ORANGE;
}

static bool road_adjacent(int r, int c) {
    if (r > 0      && tile[r-1][c] == T_ROAD) return true;
    if (r < ROWS-1 && tile[r+1][c] == T_ROAD) return true;
    if (c > 0      && tile[r][c-1] == T_ROAD) return true;
    if (c < COLS-1 && tile[r][c+1] == T_ROAD) return true;
    return false;
}

static void reset_city(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) { tile[r][c] = T_GRASS; lvl[r][c] = 0; }
    money  = 1500;
    months = 0;
    tool   = T_ROAD;
    simctr = 0;
    ready  = true;
}

static void compute_stats(void) {
    housing = jobsC = jobsI = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if      (tile[r][c] == T_RES) housing += lvl[r][c];
            else if (tile[r][c] == T_COM) jobsC   += lvl[r][c];
            else if (tile[r][c] == T_IND) jobsI   += lvl[r][c];
        }
    pop = housing * 4;
}

// ── one simulated month ───────────────────────────────────────
static void sim_tick(void) {
    compute_stats();
    // simple R/C/I demand model — a feedback loop:
    //  homes want jobs nearby, shops/factories want residents.
    rDem = (jobsC + jobsI) + 6 - housing;   // +6 seed lets a fresh town start
    cDem = housing - jobsC;
    iDem = housing - jobsI;

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (!is_zone(tile[r][c])) continue;
            if (!road_adjacent(r, c)) {                 // no road → slowly abandoned
                if (lvl[r][c] > 0 && chance(25)) lvl[r][c]--;
                continue;
            }
            int dem = tile[r][c] == T_RES ? rDem : tile[r][c] == T_COM ? cDem : iDem;
            if (dem > 0 && lvl[r][c] < 3 && chance(mid(15, 15 + dem * 8, 85)))
                lvl[r][c]++;                            // grow toward demand
            else if (dem < -1 && lvl[r][c] > 0 && chance(20))
                lvl[r][c]--;                            // shrink when oversupplied
        }

    compute_stats();
    money += (housing + jobsC + jobsI) * 3;             // monthly tax
    months++;
}

// ── apply the current tool to a cell ──────────────────────────
static void apply_tool(int r, int c) {
    int target = tool;
    if (tile[r][c] == target) return;       // no change → no charge (lets you drag freely)
    int cost = cost_of(target);
    if (money < cost) return;
    money -= cost;
    tile[r][c] = target;
    lvl[r][c]  = 0;
}

void update(void) {
    if (!ready) reset_city();

    int  mx = mouse_x(), my = mouse_y();
    bool press = mouse_pressed(MOUSE_LEFT);

    // tool bar: a fresh click selects a tool (and never paints the map)
    if (my >= BAR_Y) {
        if (press) {
            int idx = mx / (SCREEN_W / NTOOLS);
            if (idx >= 0 && idx < NTOOLS) { tool = tools[idx]; note(72, INSTR_SINE, 2); }
        }
        return;
    }

    // right-click clears a cell no matter which tool is selected
    if (mouse_down(MOUSE_RIGHT)) {
        int c = (mx - GX) / CELL, r = (my - GY) / CELL;
        if (c >= 0 && c < COLS && r >= 0 && r < ROWS && tile[r][c] != T_GRASS) {
            tile[r][c] = T_GRASS; lvl[r][c] = 0;
        }
    }
    // left button paints the current tool (hold to drag)
    else if (mouse_down(MOUSE_LEFT)) {
        int c = (mx - GX) / CELL, r = (my - GY) / CELL;
        if (c >= 0 && c < COLS && r >= 0 && r < ROWS) apply_tool(r, c);
    }

    // keyboard: A / B cycle the tool too (for gamepad / no-mouse)
    if (btnp(0, BTN_A)) tool = tools[(tool + 1) % NTOOLS];

    // advance the simulation
    if (++simctr >= TICK_FRAMES) { simctr = 0; sim_tick(); }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_cell(int r, int c) {
    int px = GX + c * CELL, py = GY + r * CELL;
    int t  = tile[r][c];

    // grass ground (checkerboard) under everything
    rectfill(px, py, CELL, CELL, ((r + c) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN);

    if (t == T_ROAD) {
        rectfill(px, py, CELL, CELL, CLR_DARK_GREY);
        line(px, py + CELL / 2, px + CELL - 1, py + CELL / 2, CLR_LIGHT_YELLOW);
    } else if (t == T_PARK) {
        rectfill(px + 1, py + 1, CELL - 2, CELL - 2, CLR_DARK_GREEN);
        rectfill(px + CELL / 2 - 1, py + 5, 2, 5, CLR_BROWN);     // trunk
        circfill(px + CELL / 2, py + 5, 3, CLR_LIME_GREEN);       // canopy
    } else if (is_zone(t)) {
        int col = zone_color(t);
        int L   = lvl[r][c];
        if (L == 0) {
            // zoned but empty — faint outline + the zone letter
            rect(px + 1, py + 1, CELL - 2, CELL - 2, col);
            print(t == T_RES ? "R" : t == T_COM ? "C" : "I", px + 3, py + 2, col);
        } else {
            int h  = 2 + L * 3;                  // taller building = higher level
            int w  = CELL - 4;
            int bx = px + 2, by = py + CELL - 1 - h;
            rectfill(bx, by, w, h, col);
            rect(bx, by, w, h, CLR_BLACK);
            for (int wy = by + 1; wy < by + h - 1; wy += 3)       // windows
                for (int wx = bx + 1; wx < bx + w - 1; wx += 3)
                    pset(wx, wy, CLR_LIGHT_YELLOW);
        }
    }
}

// a small up/down demand bar centered on a baseline
static void demand_bar(int x, int v, int col) {
    int base = 9;
    v = mid(-4, v, 4);
    if (v > 0)      rectfill(x, base - v, 3, v, col);
    else if (v < 0) rectfill(x, base, 3, -v, CLR_RED);
    else            pset(x + 1, base, col);
}

void draw(void) {
    if (!ready) reset_city();
    cls(CLR_DARK_BLUE);

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) draw_cell(r, c);

    // hover highlight — the whole reason for the mouse API
    int mx = mouse_x(), my = mouse_y();
    if (mx >= GX && mx < GX + COLS * CELL && my >= GY && my < GY + ROWS * CELL) {
        int c = (mx - GX) / CELL, r = (my - GY) / CELL;
        rect(GX + c * CELL, GY + r * CELL, CELL, CELL, CLR_WHITE);
    }

    // ── top HUD ──
    rectfill(0, 0, SCREEN_W, 11, CLR_DARKER_BLUE);
    print(str("$%d", money), 2, 2, money < cost_of(tool) ? CLR_RED : CLR_YELLOW);
    print(str("POP %d", pop), 62, 2, CLR_WHITE);
    print(str("JOB %d", jobsC + jobsI), 122, 2, CLR_LIGHT_GREY);
    demand_bar(186, rDem, CLR_GREEN);
    demand_bar(192, cDem, CLR_BLUE);
    demand_bar(198, iDem, CLR_ORANGE);
    print_right(str("Yr %d Mo %d", months / 12 + 1, months % 12 + 1), 318, 2, CLR_LIGHT_GREY);

    // ── tool bar ──
    int bw = SCREEN_W / NTOOLS;
    for (int i = 0; i < NTOOLS; i++) {
        int bx = i * bw;
        bool sel = tools[i] == tool;
        rectfill(bx, BAR_Y, bw - 1, SCREEN_H - BAR_Y, sel ? CLR_DARK_GREY : CLR_DARKER_PURPLE);
        // little swatch
        int sw = tools[i] == T_GRASS ? CLR_MEDIUM_GREEN
               : tools[i] == T_ROAD  ? CLR_LIGHT_GREY
               : tools[i] == T_PARK  ? CLR_LIME_GREEN
               : zone_color(tools[i]);
        rectfill(bx + 3, BAR_Y + 4, 6, 6, sw);
        print(labels[i], bx + 12, BAR_Y + 5, sel ? CLR_WHITE : CLR_LIGHT_GREY);
        if (sel) rect(bx, BAR_Y, bw - 1, SCREEN_H - BAR_Y, CLR_WHITE);
    }
}
