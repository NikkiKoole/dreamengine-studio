/* de:meta
{
  "title": "Dwarffort",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "pathfinding",
    "finite-state-ai",
    "particle-system"
  ],
  "lineage": "Pocket Dwarf Fortress demake — designation-driven colony loop where each dwarf A*-paths to the nearest unclaimed job and transitions through idle/going/working states; the only agent-colony cart in the library.",
  "genre": "simulation",
  "homage": "Dwarf Fortress (2006)",
  "description": "A pocket-sized Dwarf Fortress: a top-down colony base-builder on one screen where you never steer a dwarf — you designate the work. Drag over solid rock to mark it for digging (mining hews stone) or over open floor to queue a wall (a dwarf hauls a block and raises it), and a handful of stubby, helmeted dwarves A*-path to the nearest reachable job, swing their picks through puffs of dust, and build out the corridors you carve while a day clock ticks toward day 12. Pickaxe taps ride a euclidean rhythm over a slow cavern bed, the stone counter rolls, and a new-day bell eases a banner across the cave. Left-drag to designate with the current tool, right-drag to erase; 1 = DIG, 2 = WALL; SPACE pauses; reach day 12 and the fortress endures — click to found a new fort."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// ── DWARFFORT (lite) — a pocket Dwarf Fortress ──────────────────────────────
// Top-down colony base-builder, one screen. You never steer a dwarf — you
// DESIGNATE work and the little guys get on with it. Drag over solid rock to
// mark it for DIGGING (mining yields STONE); drag over open floor to queue a
// WALL (costs one stone, a dwarf hauls a block and raises it). A handful of
// dwarves A*-path to the nearest reachable job, swing a pick, build, repeat.
// A day counter ticks the whole time — endure long enough and the fortress
// stands.
//
//   LEFT-DRAG  = designate with the current tool   RIGHT-DRAG = erase a mark
//   1 = DIG tool      2 = WALL tool      SPACE = pause
//   reach day 12 and the fortress endures.  click / space to start over.

// ── geometry ────────────────────────────────────────────────────────────────
#define GW    24
#define GH    14
#define TILE  12
#define N     (GW * GH)
#define MAPX  4
#define MAPY  18
#define MAPW  (GW * TILE)
#define MAPH  (GH * TILE)
#define MAXPATH 96

// ── tiles ─────────────────────────────────────────────────────────────────
enum { T_ROCK, T_FLOOR };
enum { J_NONE, J_DIG, J_WALL };          // pending job designated on a tile
enum { TOOL_DIG, TOOL_WALL };

typedef struct {
    unsigned char type;     // T_ROCK / T_FLOOR
    unsigned char job;      // J_NONE / J_DIG / J_WALL
    unsigned char claimed;  // a dwarf is already working this cell
    unsigned char var;      // cosmetic speckle
    float hard;             // remaining dig/build work
} Tile;
static Tile grid[N];        // NB: 'grid', not 'map' (map() is an API call)

// ── dwarves ─────────────────────────────────────────────────────────────────
enum { S_IDLE, S_GOING, S_WORKING };
#define NDWARF 5
#define SPEED  34.0f        // px/sec
#define DIG_TIME  2.4f
#define WALL_TIME 1.6f

typedef struct {
    float x, y, face;
    int   state;
    int   job;              // cell being worked (-1 none)
    int   path[MAXPATH], plen, pi;
    float repath;
    bool  carry;            // hauling a block toward a wall job
    float bob;              // walk-bob phase
    int   hue;              // 0..2 beard/coat tint
    float swing;            // pick-swing timer while working
} Dwarf;
static Dwarf dw[NDWARF];

// ── effects (dust puffs / stone sparkle) ─────────────────────────────────────
typedef struct { bool active; float x, y, t, maxt, size; int col; } Puff;
#define MAXPUFF 48
static Puff puffs[MAXPUFF];

// ── globals ─────────────────────────────────────────────────────────────────
static int   stone;
static float shown_stone;
static int   day;
static float day_t;          // seconds into the current day
static float banner_t;       // new-day banner ease-in
static int   tool;           // TOOL_DIG / TOOL_WALL
static bool  sim_paused;
static int   over;           // 0 play, 1 endured
static bool  drag_l, drag_r; // left/right designation drags
#define DAY_SECS   16.0f
#define WIN_DAY    12

// ── A* scratch ───────────────────────────────────────────────────────────────
static int  g_came[N], g_g[N];
static bool g_open[N], g_closed[N];
static const int DX4[4] = { 0, 1, 0, -1 }, DY4[4] = { -1, 0, 1, 0 };

// ── helpers ─────────────────────────────────────────────────────────────────
static int  idx(int cx, int cy) { return cy * GW + cx; }
static int  pcx(int cx) { return MAPX + cx * TILE + TILE / 2; }
static int  pcy(int cy) { return MAPY + cy * TILE + TILE / 2; }
static bool in_grid(int cx, int cy) { return cx >= 0 && cy >= 0 && cx < GW && cy < GH; }
static bool is_floor(int c) { return c >= 0 && c < N && grid[c].type == T_FLOOR; }
static int  cell_of(float x, float y) {
    int cx = mid(0, (int)((x - MAPX) / TILE), GW - 1);
    int cy = mid(0, (int)((y - MAPY) / TILE), GH - 1);
    return idx(cx, cy);
}
static bool in_map(int x, int y) { return x >= MAPX && y >= MAPY && x < MAPX + MAPW && y < MAPY + MAPH; }

// ── A* over walkable floor — goal must itself be floor ───────────────────────
static int heur(int a, int b) { return abs(a % GW - b % GW) + abs(a / GW - b / GW); }
static int find_path(int s, int gc, int *out) {
    if (s < 0 || gc < 0 || s == gc) return 0;
    for (int i = 0; i < N; i++) { g_g[i] = 1000000000; g_came[i] = -1; g_open[i] = g_closed[i] = false; }
    g_g[s] = 0; g_open[s] = true;
    int guard = 0;
    while (1) {
        int best = -1, bf = 2000000000;
        for (int i = 0; i < N; i++) if (g_open[i]) { int f = g_g[i] + heur(i, gc); if (f < bf) { bf = f; best = i; } }
        if (best < 0) return 0;
        if (best == gc) break;
        g_open[best] = false; g_closed[best] = true;
        int bx = best % GW, by = best / GW;
        for (int d = 0; d < 4; d++) {
            int nx = bx + DX4[d], ny = by + DY4[d];
            if (!in_grid(nx, ny)) continue;
            int ni = idx(nx, ny);
            if (g_closed[ni] || !is_floor(ni)) continue;
            int tg = g_g[best] + 1;
            if (tg < g_g[ni]) { g_came[ni] = best; g_g[ni] = tg; g_open[ni] = true; }
        }
        if (++guard > 9000) return 0;
    }
    int tmp[N], n = 0;
    for (int c = gc; c != -1 && c != s; c = g_came[c]) if (n < N) tmp[n++] = c;
    int len = 0;
    for (int i = n - 1; i >= 0 && len < MAXPATH; i--) out[len++] = tmp[i];
    return len;
}
static void set_path(Dwarf *u, int gc) { u->plen = find_path(cell_of(u->x, u->y), gc, u->path); u->pi = 0; }
static bool step_path(Dwarf *u) {
    if (u->pi >= u->plen) return true;
    int c = u->path[u->pi];
    float tx = pcx(c % GW), ty = pcy(c / GW);
    float a = angle_to((int)u->x, (int)u->y, (int)tx, (int)ty);
    u->face = a;
    float sp = SPEED * dt();
    u->x += dx(sp, a); u->y += dy(sp, a);
    u->bob += dt() * 12;
    if (near((int)u->x, (int)u->y, (int)tx, (int)ty, 2)) u->pi++;
    return u->pi >= u->plen;
}

// ── effects ─────────────────────────────────────────────────────────────────
static void puff(float x, float y, float size, float maxt, int col) {
    for (int i = 0; i < MAXPUFF; i++)
        if (!puffs[i].active) { puffs[i] = (Puff){ true, x, y, 0, maxt, size, col }; return; }
}

// ── job finding — the floor cell a dwarf stands on to work `job` ──────────────
static int work_spot(int jobcell) {
    // DIG/WALL are worked from an adjacent floor tile; pick the nearest reachable one.
    int cx = jobcell % GW, cy = jobcell / GW;
    for (int d = 0; d < 4; d++) {
        int nx = cx + DX4[d], ny = cy + DY4[d];
        if (in_grid(nx, ny) && is_floor(idx(nx, ny))) return idx(nx, ny);
    }
    return -1;
}
// WALL is queued on a floor tile; the dwarf can also stand ON it to finish — but to
// keep pathing simple the wall is built from an adjacent floor tile too.
static int find_job(Dwarf *u) {
    int sc = cell_of(u->x, u->y);
    // gather candidate jobs, nearest-first, try a few until one is reachable
    int order[N], on = 0;
    for (int i = 0; i < N; i++) {
        if (grid[i].claimed) continue;
        if (grid[i].job == J_DIG && grid[i].type == T_ROCK && work_spot(i) >= 0) order[on++] = i;
        else if (grid[i].job == J_WALL && grid[i].type == T_FLOOR && stone > 0 && work_spot(i) >= 0) order[on++] = i;
    }
    for (int tries = 0; tries < 6 && on > 0; tries++) {
        int bi = -1, bd = 1 << 30, bk = -1;
        for (int k = 0; k < on; k++) { int d = heur(order[k], sc); if (d < bd) { bd = d; bi = order[k]; bk = k; } }
        if (bi < 0) break;
        int ws = work_spot(bi);
        if (cell_of(u->x, u->y) == ws) { u->job = bi; u->plen = 0; u->pi = 0; return bi; }
        set_path(u, ws);
        if (u->plen > 0) { u->job = bi; return bi; }
        order[bk] = order[--on];   // unreachable — drop and retry
    }
    return -1;
}

// ── dwarf AI ─────────────────────────────────────────────────────────────────
static void update_dwarf(Dwarf *u) {
    float D = dt();
    if (u->repath > 0) u->repath -= D;

    switch (u->state) {
    case S_IDLE: {
        int j = find_job(u);
        if (j >= 0) {
            // for a WALL job, claim a block now so two dwarves don't fight over one stone
            if (grid[j].job == J_WALL) { if (stone <= 0) { u->job = -1; break; } stone--; u->carry = true; }
            grid[j].claimed = 1;
            u->state = (u->plen > 0) ? S_GOING : S_WORKING;
            break;
        }
        // idle shuffle so they don't freeze in a clump
        if (chance(2)) { int t = rnd(N); if (is_floor(t)) set_path(u, t); }
        step_path(u);
    } break;

    case S_GOING: {
        int j = u->job;
        // job vanished out from under us?
        if (j < 0 || grid[j].job == J_NONE) { if (u->carry) { stone++; u->carry = false; } if (j >= 0) grid[j].claimed = 0; u->state = S_IDLE; u->job = -1; break; }
        bool there = step_path(u);
        int ws = work_spot(j);
        if (ws < 0) { grid[j].claimed = 0; if (u->carry) { stone++; u->carry = false; } u->state = S_IDLE; u->job = -1; break; }
        if (there || cell_of(u->x, u->y) == ws || near((int)u->x, (int)u->y, pcx(ws % GW), pcy(ws / GW), 3)) {
            u->face = angle_to((int)u->x, (int)u->y, pcx(j % GW), pcy(j / GW));
            u->state = S_WORKING; u->swing = 0;
        }
    } break;

    case S_WORKING: {
        int j = u->job;
        if (j < 0 || grid[j].job == J_NONE) { if (u->carry) { stone++; u->carry = false; } if (j >= 0) grid[j].claimed = 0; u->state = S_IDLE; u->job = -1; break; }
        u->face = angle_to((int)u->x, (int)u->y, pcx(j % GW), pcy(j / GW));
        u->swing += D * 9;
        grid[j].hard -= D;
        // pick taps / chips while mining
        if (grid[j].job == J_DIG && chance(8)) puff(pcx(j % GW) + rnd_between(-4, 4), pcy(j / GW) + rnd_between(-4, 4), 2, 0.3f, CLR_DARK_GREY);
        if (grid[j].hard <= 0) {
            if (grid[j].job == J_DIG) {
                grid[j].type = T_FLOOR; grid[j].job = J_NONE; grid[j].claimed = 0;
                stone += rnd_between(1, 3);
                puff(pcx(j % GW), pcy(j / GW), 6, 0.45f, CLR_LIGHT_GREY);
                note(74, 6, 2);                     // stone chime
            } else { // J_WALL
                grid[j].type = T_ROCK; grid[j].job = J_NONE; grid[j].claimed = 0;
                grid[j].var = rnd(4); u->carry = false;
                puff(pcx(j % GW), pcy(j / GW), 5, 0.35f, CLR_BROWN);
                note(50, 7, 3); shake(1.5f);        // block thunk
            }
            u->state = S_IDLE; u->job = -1; u->plen = 0;
        }
    } break;
    }
}

// ── designation painting ─────────────────────────────────────────────────────
static void designate(int cx, int cy, bool erase) {
    if (!in_grid(cx, cy)) return;
    int c = idx(cx, cy);
    if (erase) { if (!grid[c].claimed) grid[c].job = J_NONE; return; }
    if (tool == TOOL_DIG) {
        if (grid[c].type == T_ROCK && grid[c].job != J_DIG) { grid[c].job = J_DIG; grid[c].hard = DIG_TIME; }
    } else { // TOOL_WALL — only on open floor with a floor neighbour to stand on
        if (grid[c].type == T_FLOOR && grid[c].job != J_WALL && work_spot(c) >= 0) { grid[c].job = J_WALL; grid[c].hard = WALL_TIME; }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────────
static void carve(int cx, int cy) { if (in_grid(cx, cy)) { int i = idx(cx, cy); grid[i].type = T_FLOOR; grid[i].job = J_NONE; grid[i].claimed = 0; } }

static void reset_game(void) {
    for (int i = 0; i < N; i++) {
        grid[i] = (Tile){ 0 };
        grid[i].type = T_ROCK;
        grid[i].var = (i * 7 + (i / GW) * 13) % 4;
    }
    for (int i = 0; i < MAXPUFF; i++) puffs[i].active = false;

    // a small starter chamber in the middle so dwarves have somewhere to stand
    for (int y = 5; y <= 8; y++) for (int x = 9; x <= 14; x++) carve(x, y);

    // a couple of dig designations to kick the loop off
    grid[idx(8, 6)].job = J_DIG; grid[idx(8, 6)].hard = DIG_TIME;
    grid[idx(15, 7)].job = J_DIG; grid[idx(15, 7)].hard = DIG_TIME;
    grid[idx(11, 4)].job = J_DIG; grid[idx(11, 4)].hard = DIG_TIME;
    grid[idx(12, 9)].job = J_DIG; grid[idx(12, 9)].hard = DIG_TIME;

    for (int i = 0; i < NDWARF; i++) {
        dw[i] = (Dwarf){ 0 };
        int c = idx(10 + i % 4, 6 + i / 4);
        dw[i].x = pcx(c % GW); dw[i].y = pcy(c / GW);
        dw[i].state = S_IDLE; dw[i].job = -1; dw[i].hue = i % 3; dw[i].face = 90;
    }

    stone = 6; shown_stone = 6;
    day = 1; day_t = 0; banner_t = 1.0f;
    tool = TOOL_DIG; sim_paused = false; over = 0;
    drag_l = drag_r = false;
}

void init(void) {
    instrument(5, INSTR_NOISE, 1, 60, 0, 50);  instrument_filter(5, FILTER_BAND, 1500, 6); // pickaxe tap
    instrument(6, INSTR_SINE, 1, 60, 2, 150);                                              // stone chime
    instrument(7, INSTR_TRI, 2, 70, 0, 120);                                               // block thunk
    instrument(8, INSTR_SAW, 240, 320, 4, 700); instrument_filter(8, FILTER_LOW, 380, 3);
    instrument_lfo(8, 0, LFO_CUTOFF, 0.25f, 140);                                          // cavern bed
    lfo_shape(8, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)
    instrument(9, INSTR_TRI, 3, 120, 4, 360);                                              // new-day bell
    bpm(76);
    reset_game();
}

// ── update ─────────────────────────────────────────────────────────────────────
void update(void) {
    if (over) { if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE) || btnp(0, BTN_A)) reset_game(); return; }
    float D = dt();
    if (banner_t > 0) banner_t -= D;

    // tool + pause hotkeys
    if (keyp('1')) tool = TOOL_DIG;
    if (keyp('2')) tool = TOOL_WALL;
    if (keyp(KEY_SPACE)) sim_paused = !sim_paused;
    if (mouse_wheel() > 0) tool = TOOL_DIG;
    if (mouse_wheel() < 0) tool = TOOL_WALL;

    int mx = mouse_x(), my = mouse_y();
    int cx = (mx - MAPX) / TILE, cy = (my - MAPY) / TILE;

    // designation drags
    if (mouse_pressed(MOUSE_LEFT) && in_map(mx, my))  drag_l = true;
    if (mouse_pressed(MOUSE_RIGHT) && in_map(mx, my)) drag_r = true;
    if (mouse_released(MOUSE_LEFT))  drag_l = false;
    if (mouse_released(MOUSE_RIGHT)) drag_r = false;
    if (drag_l && in_map(mx, my)) designate(cx, cy, false);
    if (drag_r && in_map(mx, my)) designate(cx, cy, true);

    if (sim_paused) { shown_stone = lerp(shown_stone, stone, clamp(7 * D, 0, 1)); return; }

    // day clock
    day_t += D;
    if (day_t >= DAY_SECS) {
        day_t = 0; day++; banner_t = 1.2f; note(67, 9, 4);
        if (day >= WIN_DAY) over = 1;
    }

    for (int i = 0; i < NDWARF; i++) update_dwarf(&dw[i]);
    for (int i = 0; i < MAXPUFF; i++) if (puffs[i].active) { puffs[i].t += D; if (puffs[i].t >= puffs[i].maxt) puffs[i].active = false; }

    // sound bed + reactive mining rhythm
    if (every(4)) { int root = 33 + (beat() / 16 % 2) * 5; note(root, 8, 2); }
    bool mining = false;
    for (int i = 0; i < NDWARF; i++) if (dw[i].state == S_WORKING && dw[i].job >= 0 && grid[dw[i].job].job == J_DIG) mining = true;
    if (mining && euclid(5, 8, beat())) hit(rnd_between(62, 68), 5, 1, 40);

    shown_stone = lerp(shown_stone, stone, clamp(7 * D, 0, 1));
}

// ── drawing ─────────────────────────────────────────────────────────────────
static void draw_tile(int x, int y) {
    int i = idx(x, y), px = MAPX + x * TILE, py = MAPY + y * TILE;
    Tile *t = &grid[i];
    if (t->type == T_ROCK) {
        rectfill(px, py, TILE, TILE, CLR_DARK_GREY);
        // chiseled-stone speckle
        pset(px + 2 + t->var, py + 3, CLR_DARKER_GREY);
        pset(px + 7, py + 8 - t->var, CLR_LIGHT_GREY);
        pset(px + 4, py + 6, CLR_DARKER_GREY);
    } else {
        rectfill(px, py, TILE, TILE, CLR_BROWNISH_BLACK);
        pset(px + 3 + t->var, py + 5, CLR_DARK_BROWN);
        pset(px + 8, py + 9, CLR_DARKER_GREY);
    }
    // designation overlays — DF-style yellow/teal stripes
    if (t->job == J_DIG) {
        fillp(FILL_CHECKER, -1); rectfill(px, py, TILE, TILE, CLR_YELLOW); fillp_reset();
        rect(px, py, TILE, TILE, CLR_DARK_ORANGE);
    } else if (t->job == J_WALL) {
        fillp(FILL_CHECKER, -1); rectfill(px, py, TILE, TILE, CLR_BLUE_GREEN); fillp_reset();
        rect(px, py, TILE, TILE, CLR_BLUE);
    }
}

static void draw_dwarf(Dwarf *u) {
    int x = (int)u->x, y = (int)u->y;
    int bob = (u->state == S_GOING || u->state == S_IDLE) ? (int)(sin_deg(u->bob * 30) * 1.0f) : 0;
    y += bob;
    int coat = u->hue == 0 ? CLR_DARK_RED : u->hue == 1 ? CLR_TRUE_BLUE : CLR_DARK_GREEN;
    int beard = u->hue == 1 ? CLR_LIGHT_GREY : CLR_ORANGE;
    // shadow
    ovalfill(x, y + 5, 3, 1, CLR_BROWNISH_BLACK);
    // body
    ovalfill(x, y + 1, 3, 3, coat);
    // head + beard
    circfill(x, y - 3, 2, CLR_LIGHT_PEACH);
    rectfill(x - 2, y - 2, 4, 2, beard);
    // helmet
    rectfill(x - 2, y - 5, 4, 2, CLR_DARK_GREY);
    pset(x, y - 6, CLR_LIGHT_GREY);
    // pick swing while working
    if (u->state == S_WORKING) {
        float sw = sin_deg(u->swing * 60) * 4;
        int hx = x + (int)dx(5 + sw, u->face), hy = y - 1 + (int)dy(5 + sw, u->face);
        line(x, y - 1, hx, hy, CLR_BROWN);
        pset(hx, hy, CLR_LIGHT_GREY);
    }
    // carried block toward a wall
    if (u->carry) rectfill(x - 1, y - 8, 3, 3, CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_BLACK);

    // fort border
    rect(MAPX - 1, MAPY - 1, MAPW + 2, MAPH + 2, CLR_DARKER_GREY);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) draw_tile(x, y);

    // dwarves, row-sorted for a touch of depth
    for (int y = 0; y < GH; y++) {
        int lo = MAPY + y * TILE, hi = lo + TILE;
        for (int i = 0; i < NDWARF; i++) if ((int)dw[i].y >= lo && (int)dw[i].y < hi) draw_dwarf(&dw[i]);
    }

    // puffs
    for (int i = 0; i < MAXPUFF; i++) if (puffs[i].active) {
        Puff *p = &puffs[i];
        float t = clamp(p->t / p->maxt, 0, 1);
        int r = (int)(p->size * ease_out(t));
        fillp(FILL_CHECKER, -1); circfill((int)p->x, (int)p->y - (int)(t * 4), r, p->col); fillp_reset();
    }

    // hover ghost for the active tool
    if (in_map(mouse_x(), mouse_y())) {
        int cx = (mouse_x() - MAPX) / TILE, cy = (mouse_y() - MAPY) / TILE;
        if (in_grid(cx, cy)) {
            int c = idx(cx, cy);
            bool ok = (tool == TOOL_DIG) ? (grid[c].type == T_ROCK)
                                         : (grid[c].type == T_FLOOR && work_spot(c) >= 0);
            rect(MAPX + cx * TILE, MAPY + cy * TILE, TILE, TILE, ok ? (tool == TOOL_DIG ? CLR_YELLOW : CLR_BLUE) : CLR_RED);
        }
    }

    // ── top HUD ──
    rectfill(0, 0, SCREEN_W, MAPY - 2, CLR_BROWNISH_BLACK);
    print("DWARFFORT", 4, 3, CLR_ORANGE);
    print(str("STONE %d", (int)(shown_stone + 0.5f)), 90, 3, CLR_LIGHT_GREY);
    // day clock with a little fill bar
    print_right(str("DAY %d/%d", day, WIN_DAY), SCREEN_W - 4, 3, CLR_LIGHT_YELLOW);
    bar(SCREEN_W - 80, 11, 76, 3, day_t / DAY_SECS, CLR_DARK_ORANGE, CLR_DARKER_GREY);

    // ── tool palette ──
    int by = SCREEN_H - 14;
    rectfill(0, by - 2, SCREEN_W, 16, CLR_BROWNISH_BLACK);
    // DIG
    rectfill(4, by, 56, 11, tool == TOOL_DIG ? CLR_DARK_ORANGE : CLR_DARKER_GREY);
    rect(4, by, 56, 11, tool == TOOL_DIG ? CLR_YELLOW : CLR_DARK_GREY);
    print("1 DIG", 9, by + 2, tool == TOOL_DIG ? CLR_BLACK : CLR_LIGHT_GREY);
    // WALL
    rectfill(64, by, 60, 11, tool == TOOL_WALL ? CLR_BLUE : CLR_DARKER_GREY);
    rect(64, by, 60, 11, tool == TOOL_WALL ? CLR_WHITE : CLR_DARK_GREY);
    print("2 WALL", 69, by + 2, tool == TOOL_WALL ? CLR_BLACK : CLR_LIGHT_GREY);
    // hint / pause
    if (sim_paused) print(blink(18) ? "-- PAUSED (space) --" : "", 150, by + 2, CLR_YELLOW);
    else print("L-drag mark  R-drag erase", 132, by + 2, CLR_DARK_GREY);

    // new-day banner
    if (banner_t > 0) {
        float e = ease_out(clamp(banner_t / 1.2f, 0, 1));
        int ty = MAPY + 4 + (int)((1 - e) * -14);
        print_centered(str("DAY %d", day), SCREEN_W/2, ty, CLR_LIGHT_YELLOW);
    }

    // ── end screen ──
    if (over) {
        fade(0.4f);
        rectfill(SCREEN_W / 2 - 84, SCREEN_H / 2 - 24, 168, 48, CLR_BROWNISH_BLACK);
        rect(SCREEN_W / 2 - 84, SCREEN_H / 2 - 24, 168, 48, CLR_ORANGE);
        print_centered("THE FORTRESS ENDURES", SCREEN_W/2, SCREEN_H / 2 - 14, CLR_LIGHT_YELLOW);
        print_centered(str("%d days dug, %d stone hewn", WIN_DAY, stone), SCREEN_W/2, SCREEN_H / 2 - 1, CLR_LIGHT_GREY);
        print_centered("click to found a new fort", SCREEN_W/2, SCREEN_H / 2 + 11, blink(20) ? CLR_WHITE : CLR_DARK_GREY);
    }
}
