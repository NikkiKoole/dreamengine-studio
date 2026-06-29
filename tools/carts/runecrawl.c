/* de:meta
{
  "title": "runecrawl",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "turn-based-loop",
    "raycasting",
    "grid-movement",
    "combinational-logic",
    "dungeon-generation",
    "finite-state-ai"
  ],
  "lineage": "rogue.c's FOV + turn loop fused with the logic cart's signal sim (a MechUpdateSignals subset) — mechanisms as diegetic dungeon locks; grown into a procedural circuit-roguelike with five rune-gate types (AND/OR/XOR/NOT/CLOCK) and chasing wraiths.",
  "genre": "puzzle",
  "description": "A logic-puzzle roguelike where the dungeon and the machine share a turn. Each floor is a procedurally-generated chain of vaults, every door a portcullis wired to a RUNE-GATE you must satisfy to pass — AND needs both its levers, OR either, XOR exactly one, a NOT-trap starts thrown (turn it OFF), a CLOCK-rune blinks the door open so you dash through before it shuts, and a PRESSURE PLATE holds the door while you stand on it (it lingers a few steps so you can run for the gap — or lure a wraith onto it to hold it for you). Wraiths prowl the deeper vaults — bump one to cut it down, but let it reach you and it bites (you have 3 hearts). Explore by torchlight (raycast fog-of-war), satisfy each gate and take the stairs DOWN to a deeper, tougher floor — there is no top, only how far you descend (best depth is saved); death sends you back to the surface. The signal sim from the `logic` cart made diegetic — one circuit tick per step. Arrows/WASD move (into a lever to throw it, into a wraith to strike), Z/. wait, R descend anew."
}
de:meta */
#include "studio.h"
#include <stdio.h>

// RUNECRAWL
// A logic-puzzle roguelike. Each floor is a chain of vaults, every door a
// portcullis wired to a RUNE-GATE you must satisfy to pass: an AND needs both
// its levers, an OR either, an XOR exactly one, a NOT-trap starts thrown (turn
// it OFF), and a CLOCK-rune blinks the door open — dash through before it shuts.
// Wraiths prowl the deeper vaults; bump one to cut it down, but let it reach you
// and it bites. Reach the stairs to escape. Floors are generated fresh each run.
// The signal sim from the `logic` cart, made diegetic: one circuit tick a step.
//
// Arrows / WASD: move (into a lever = throw it; into a wraith = strike).
// Z or .: wait a turn.  R: descend anew (after escaping or dying).

#define GW    26
#define GH    15
#define TILE  12
#define FOV_R 6

#define NR    4          // rooms per floor (linear chain)
#define RW    5          // room interior width
#define RH    7          // room interior height
#define RY    3          // top row of the room band
#define DR    (RY + 3)   // door row (band mid)
#define CLOCKP 3         // clock gate: moves per phase
#define PLATEH 5         // pressure plate: moves it stays powered after you step off
#define MAXMON 5
#define HPMAX  3

enum { T_WALL, T_FLOOR, T_STAIRS };
enum { D_NONE, D_WIRE, D_LEVER, D_AND, D_NOT, D_DOOR, D_OR, D_XOR, D_CLOCK, D_PLATE };
enum { DN, DE, DS, DW };
enum { ST_PLAY, ST_DEAD };

static unsigned char terr[GH][GW], dev[GH][GW], face[GH][GW], on[GH][GW];
static int  ctimer[GH][GW];                     // clock-gate countdown (in moves)
static int  sig[2][GH][GW], sread;
static int  qx[GW*GH], qy[GW*GH], seedv[GH][GW];
static unsigned char seen[GH][GW], vis[GH][GW];

typedef struct { int x, y, alive; } Mon;
static Mon mon[MAXMON];
static int nmon;

static int px, py, hp, turns, state, depth, best;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static int in_grid(int x, int y) { return x >= 0 && x < GW && y >= 0 && y < GH; }
static void doff(int d, int *dx, int *dy) {
    *dx = 0; *dy = 0;
    if (d == DN) *dy = -1; else if (d == DE) *dx = 1; else if (d == DS) *dy = 1; else *dx = -1;
}
static int  opp(int d) { return (d + 2) & 3; }
static void gins(int f, int *a, int *b) { *a = (f + 1) & 3; *b = (f + 3) & 3; }
static int  room_x0(int i) { return 1 + i * (RW + 1); }   // rooms tiled with a 1-wall gap

// ---------------------------------------------------------------------------
// signal sim (logic cart's MechUpdateSignals, trimmed): levers + clocks are
// sources, AND/OR/XOR/NOT are gates, wires flood, doors are sinks. Clocks are
// advanced once per MOVE by clock_step(), NOT inside the combinational settle.
// ---------------------------------------------------------------------------
static int rsig(int x, int y) { return in_grid(x, y) ? sig[sread][y][x] : 0; }
static void seedw(int ns[GH][GW], int x, int y, int v, int *qt) {
    if (!in_grid(x, y) || dev[y][x] != D_WIRE || ns[y][x] >= v) return;
    ns[y][x] = v; seedv[y][x] = v; qx[*qt] = x; qy[*qt] = y; (*qt)++;
}
static void seed_adj(int ns[GH][GW], int x, int y, int v, int *qt) {
    for (int d = 0; d < 4; d++) { int dx, dy; doff(d, &dx, &dy); seedw(ns, x + dx, y + dy, v, qt); }
}
static void seed_face(int ns[GH][GW], int x, int y, int f, int v, int *qt) {
    int dx, dy; doff(f, &dx, &dy); seedw(ns, x + dx, y + dy, v, qt);
}
static void sim_tick(void) {
    int ns[GH][GW];
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { ns[y][x] = 0; seedv[y][x] = 0; }
    int qh = 0, qt = 0;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        int d = dev[y][x];
        if (d == D_LEVER || d == D_CLOCK || d == D_PLATE) { if (on[y][x]) seed_adj(ns, x, y, 1, &qt); }
        else if (d == D_NOT) {
            int dx, dy; doff(opp(face[y][x]), &dx, &dy);
            if (!rsig(x + dx, y + dy)) seed_face(ns, x, y, face[y][x], 1, &qt);
        } else if (d == D_AND || d == D_OR || d == D_XOR) {
            int a, b; gins(face[y][x], &a, &b);
            int dx, dy; doff(a, &dx, &dy); int ia = rsig(x + dx, y + dy);
            doff(b, &dx, &dy); int ib = rsig(x + dx, y + dy);
            int o = d == D_AND ? (ia && ib) : d == D_XOR ? (ia != ib) : (ia || ib);
            if (o) seed_face(ns, x, y, face[y][x], 1, &qt);
        }
    }
    while (qh < qt) {
        int wx = qx[qh], wy = qy[qh], v = seedv[wy][wx]; qh++;
        for (int d = 0; d < 4; d++) {
            int dx, dy; doff(d, &dx, &dy); int nx = wx + dx, ny = wy + dy;
            if (in_grid(nx, ny) && dev[ny][nx] == D_WIRE && ns[ny][nx] < v) {
                ns[ny][nx] = v; seedv[ny][nx] = v; qx[qt] = nx; qy[qt] = ny; qt++;
            }
        }
    }
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) sig[1 - sread][y][x] = ns[y][x];
    sread = 1 - sread;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (dev[y][x] == D_DOOR) {
            int s = 0;
            for (int d = 0; d < 4; d++) { int dx, dy; doff(d, &dx, &dy); if (in_grid(x+dx, y+dy) && ns[y+dy][x+dx] > s) s = ns[y+dy][x+dx]; }
            on[y][x] = (s > 0);
        }
    }
}
static void settle(void) { for (int i = 0; i < 8; i++) sim_tick(); }
static void clock_step(void) {   // advance clock-gates once per move
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if (dev[y][x] == D_CLOCK && --ctimer[y][x] <= 0) { on[y][x] ^= 1; ctimer[y][x] = CLOCKP; }
}
static int occupied(int x, int y);   // fwd
static void plate_step(void) {   // a plate is powered while a creature stands on it, then lingers PLATEH moves
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (dev[y][x] != D_PLATE) continue;
        if (occupied(x, y)) { on[y][x] = 1; ctimer[y][x] = PLATEH; }
        else if (ctimer[y][x] > 0 && --ctimer[y][x] <= 0) on[y][x] = 0;
    }
}

// ---------------------------------------------------------------------------
// generation
// ---------------------------------------------------------------------------
static void fill(int x0, int y0, int x1, int y1, int t) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (in_grid(x, y)) terr[y][x] = t;
}
static void setdev(int x, int y, int d, int f) { terr[y][x] = T_FLOOR; dev[y][x] = d; face[y][x] = f; on[y][x] = 0; ctimer[y][x] = 0; }

// stamp a gate puzzle at gate cell (gx,gy); the door it opens sits at (gx+2,gy).
static void stamp_gate(int gx, int gy, int type) {
    setdev(gx + 2, gy, D_DOOR, DN);
    setdev(gx + 1, gy, D_WIRE, 0);                 // output wire -> door
    if (type == D_AND || type == D_OR || type == D_XOR) {
        setdev(gx, gy, type, DE);                  // gate faces E; inputs N & S
        setdev(gx, gy - 1, D_WIRE, 0); setdev(gx, gy + 1, D_WIRE, 0);
        setdev(gx, gy - 2, D_LEVER, 0); setdev(gx, gy + 2, D_LEVER, 0);
    } else if (type == D_NOT) {
        setdev(gx, gy, D_NOT, DE);                 // input from the W; lever pre-thrown
        setdev(gx - 1, gy, D_WIRE, 0);
        setdev(gx - 2, gy, D_LEVER, 0); on[gy][gx - 2] = 1;
    } else if (type == D_CLOCK) {
        setdev(gx, gy, D_CLOCK, DN); ctimer[gy][gx] = CLOCKP;
    } else { // D_PLATE: stand on it (or lure a wraith on); it lingers while you dash the wire to the door
        setdev(gx, gy, D_WIRE, 0); setdev(gx - 1, gy, D_WIRE, 0);
        setdev(gx - 2, gy, D_PLATE, 0);
    }
}

static int mon_at(int x, int y) {
    for (int i = 0; i < nmon; i++) if (mon[i].alive && mon[i].x == x && mon[i].y == y) return i;
    return -1;
}
static int occupied(int x, int y) { return (px == x && py == y) || mon_at(x, y) >= 0; }

static void compute_fov(void);   // defined in the fov section below

// generate ONE floor's layout (uses `depth` for difficulty). Does NOT touch hp/state.
static void gen_floor(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        terr[y][x] = T_WALL; dev[y][x] = D_NONE; face[y][x] = 0; on[y][x] = 0; ctimer[y][x] = 0;
        seen[y][x] = vis[y][x] = 0;
    }
    for (int b = 0; b < 2; b++) for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) sig[b][y][x] = 0;
    sread = 0;

    for (int i = 0; i < NR; i++) fill(room_x0(i), RY, room_x0(i) + RW - 1, RY + RH - 1, T_FLOOR);

    int types[] = { D_AND, D_OR, D_XOR, D_NOT, D_CLOCK, D_PLATE };
    for (int i = 0; i < NR - 1; i++)
        stamp_gate(room_x0(i) + 3, DR, types[rnd(6)]);

    terr[DR][room_x0(NR - 1) + 2] = T_STAIRS;
    px = room_x0(0) + 1; py = DR;

    nmon = 0;
    int want = depth + rnd(2); if (want < 1) want = 1; if (want > MAXMON) want = MAXMON;  // scales with depth
    for (int tries = 0; tries < 300 && nmon < want; tries++) {
        int r = 1 + rnd(NR - 1);                                   // never the start room
        int x = room_x0(r) + rnd(RW), y = RY + rnd(RH);
        if (terr[y][x] != T_FLOOR || dev[y][x] != D_NONE || mon_at(x, y) >= 0) continue;
        mon[nmon].x = x; mon[nmon].y = y; mon[nmon].alive = 1; nmon++;
    }
    plate_step(); settle();   // charge a plate the player spawns on, and settle doors for frame 1
}

static void new_run(void) { depth = 1; hp = HPMAX; turns = 0; state = ST_PLAY; gen_floor(); compute_fov(); }
static void descend(void) { depth++; if (hp < HPMAX) hp++; gen_floor(); compute_fov(); shake(2); note(72, INSTR_SQUARE, 3); }

// ---------------------------------------------------------------------------
// movement + fov
// ---------------------------------------------------------------------------
static int solid_dev(int d) { return d == D_LEVER || d == D_AND || d == D_NOT || d == D_OR || d == D_XOR || d == D_CLOCK; }
static int walkable(int x, int y) {
    if (!in_grid(x, y) || terr[y][x] == T_WALL) return 0;
    int d = dev[y][x];
    if (d == D_DOOR) return on[y][x];
    return !solid_dev(d);
}
static int opaque(int x, int y) { return !in_grid(x, y) || terr[y][x] == T_WALL; }

static void trace_fov(int tx, int ty) {
    int x0 = px, y0 = py, dx = abs(tx - x0), dy = -abs(ty - y0);
    int sx = x0 < tx ? 1 : -1, sy = y0 < ty ? 1 : -1, err = dx + dy, x = x0, y = y0;
    while (1) {
        if (x == tx && y == ty) break;
        if (!(x == x0 && y == y0) && opaque(x, y)) return;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
    vis[ty][tx] = 1; seen[ty][tx] = 1;
}
static void compute_fov(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) vis[y][x] = 0;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if ((x - px) * (x - px) + (y - py) * (y - py) <= FOV_R * FOV_R) trace_fov(x, y);
    vis[py][px] = seen[py][px] = 1;
}

static void monsters_act(void) {
    for (int i = 0; i < nmon; i++) {
        Mon *m = &mon[i];
        if (!m->alive) continue;
        int dxp = px - m->x, dyp = py - m->y, adist = abs(dxp) + abs(dyp);
        if (adist == 1) {                                  // adjacent -> bite
            hp--; shake(2); note(46, INSTR_SAW, 3);
            if (hp <= 0) { hp = 0; state = ST_DEAD; }
            continue;
        }
        int nx = m->x, ny = m->y;
        if (vis[m->y][m->x] && adist <= FOV_R + 1) {       // hunt the player
            if (abs(dxp) >= abs(dyp)) nx += sgn(dxp); else ny += sgn(dyp);
            if (!(walkable(nx, ny) && mon_at(nx, ny) < 0 && !(nx == px && ny == py))) {
                nx = m->x; ny = m->y;
                if (abs(dxp) >= abs(dyp)) ny += sgn(dyp); else nx += sgn(dxp);
            }
        } else {                                           // idle wander
            int d = rnd(4); int wx, wy; doff(d, &wx, &wy); nx += wx; ny += wy;
        }
        if (walkable(nx, ny) && mon_at(nx, ny) < 0 && !(nx == px && ny == py)) { m->x = nx; m->y = ny; }
    }
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------
static int powered(int x, int y) { return sig[sread][y][x] > 0; }

static void draw_wire(int X, int Y, int x, int y, int lit) {
    int cx = X + 6, cy = Y + 6, p = powered(x, y);
    int col = !lit ? CLR_DARKER_BLUE : p ? CLR_LIGHT_YELLOW : CLR_TRUE_BLUE;
    for (int d = 0; d < 4; d++) {
        int dx, dy; doff(d, &dx, &dy);
        if (in_grid(x + dx, y + dy) && dev[y + dy][x + dx] != D_NONE) line(cx, cy, cx + dx * 6, cy + dy * 6, col);
    }
    pset(cx, cy, (p && lit) ? CLR_WHITE : col);
}

static void draw_cell(int x, int y) {
    if (!seen[y][x]) return;
    int lit = vis[y][x], X = x * TILE, Y = y * TILE, cx = X + 6, cy = Y + 6;
    if (terr[y][x] == T_WALL) {
        rectfill(X, Y, TILE, TILE, lit ? CLR_DARK_GREY : CLR_DARKER_GREY);
        rect(X, Y, TILE, TILE, lit ? CLR_MEDIUM_GREY : CLR_BROWNISH_BLACK);
        return;
    }
    rectfill(X, Y, TILE, TILE, lit ? CLR_BROWNISH_BLACK : CLR_BLACK);
    if (terr[y][x] == T_STAIRS) { font(FONT_NORMAL); print(">", cx - 3, cy - 3, lit ? CLR_YELLOW : CLR_DARK_GREY); }

    switch (dev[y][x]) {
        case D_WIRE: draw_wire(X, Y, x, y, lit); break;
        case D_LEVER: {
            int o = on[y][x], col = !lit ? CLR_DARK_GREY : o ? CLR_ORANGE : CLR_LIGHT_GREY;
            rectfill(cx - 2, Y + 8, 4, 3, lit ? CLR_DARKER_GREY : CLR_BROWNISH_BLACK);
            line(cx, Y + 9, cx + (o ? 3 : -3), Y + 3, col); pset(cx + (o ? 3 : -3), Y + 3, col);
            break;
        }
        case D_CLOCK: {
            int o = on[y][x], col = !lit ? CLR_DARKER_PURPLE : o ? CLR_LIGHT_YELLOW : CLR_INDIGO;
            circ(cx, cy, 3, col); line(cx, cy, cx, cy - 2, col); line(cx, cy, cx + 2, cy, col);
            break;
        }
        case D_AND: case D_NOT: case D_OR: case D_XOR: {
            int hot = 0, dx, dy; doff(face[y][x], &dx, &dy);
            if (in_grid(x + dx, y + dy)) hot = powered(x + dx, y + dy);
            int col = !lit ? CLR_DARKER_PURPLE : hot ? CLR_PINK : CLR_INDIGO;
            rectfill(X + 1, Y + 1, TILE - 2, TILE - 2, lit ? CLR_DARKER_PURPLE : CLR_BLACK);
            rect(X + 1, Y + 1, TILE - 2, TILE - 2, col);
            const char *g = dev[y][x] == D_AND ? "&" : dev[y][x] == D_OR ? "|" : dev[y][x] == D_XOR ? "^" : "!";
            font(FONT_SMALL); print(g, cx - 1, cy - 2, col);
            break;
        }
        case D_PLATE: {
            int p = on[y][x];
            int col = !lit ? CLR_DARKER_GREY : p ? CLR_LIGHT_YELLOW : CLR_DARK_GREY;
            rect(X + 2, Y + 2, TILE - 4, TILE - 4, col);
            if (p && lit) { rect(X + 3, Y + 3, TILE - 6, TILE - 6, CLR_YELLOW); }
            break;
        }
        case D_DOOR: {
            int col = !lit ? CLR_DARK_GREY : CLR_MEDIUM_GREY;
            for (int i = 0; i < 3; i++) rectfill(X + 2 + i * 4, Y, 2, on[y][x] ? 3 : TILE, col);
            break;
        }
    }
}

static void draw_mon(int i) {
    Mon *m = &mon[i];
    if (!m->alive || !vis[m->y][m->x]) return;
    int cx = m->x * TILE + 6, cy = m->y * TILE + 6;
    circfill(cx, cy, 4, CLR_DARK_PURPLE);
    rectfill(cx - 3, cy + 1, 6, 4, CLR_DARK_PURPLE);
    pset(cx - 2, cy - 1, CLR_RED); pset(cx + 2, cy - 1, CLR_RED);
}

static void draw_hud(void) {
    rectfill(0, GH * TILE, SCREEN_W, SCREEN_H - GH * TILE, CLR_BLACK);
    font(FONT_SMALL);
    char t[32];
    if (state == ST_DEAD) {
        snprintf(t, sizeof t, "THE WRAITHS TAKE YOU - depth %d (best %d)", depth, best);
        print(t, 6, GH*TILE+4, CLR_RED);
        print("R: descend anew", 6, GH*TILE+13, CLR_LIGHT_GREY);
        return;
    }
    for (int i = 0; i < HPMAX; i++) circfill(8 + i * 9, GH*TILE+7, 3, i < hp ? CLR_RED : CLR_DARKER_GREY);
    snprintf(t, sizeof t, "depth %d   best %d", depth, best);
    print(t, SCREEN_W - 80, GH*TILE+4, CLR_LIGHT_GREY);
    print("satisfy each rune-gate, take the stairs down", 38, GH*TILE+4, CLR_LIGHT_GREY);
    print("arrows move - bump levers/wraiths - Z wait", 38, GH*TILE+12, CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_BLACK);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) draw_cell(x, y);
    for (int i = 0; i < nmon; i++) draw_mon(i);
    font(FONT_NORMAL); print("@", px * TILE + 3, py * TILE + 3, state == ST_DEAD ? CLR_DARK_RED : CLR_WHITE);
    draw_hud();
}

// ---------------------------------------------------------------------------
// turn loop
// ---------------------------------------------------------------------------
void init(void) { best = load_int("best", 0); new_run(); }

void update(void) {
    if (state != ST_PLAY) { if (keyp('R')) new_run(); return; }

    int mvx = 0, mvy = 0, wait = 0, act = 0;
    if (btnp(0, BTN_LEFT)  || keyp(KEY_LEFT)  || keyp('A')) mvx = -1;
    else if (btnp(0, BTN_RIGHT) || keyp(KEY_RIGHT) || keyp('D')) mvx = 1;
    else if (btnp(0, BTN_UP)    || keyp(KEY_UP)    || keyp('W')) mvy = -1;
    else if (btnp(0, BTN_DOWN)  || keyp(KEY_DOWN)  || keyp('S')) mvy = 1;
    else if (keyp('Z') || keyp('.')) wait = 1;

    if (mvx || mvy) {
        int tx = px + mvx, ty = py + mvy, mi = mon_at(tx, ty);
        if (mi >= 0) { mon[mi].alive = 0; shake(3); note(60, INSTR_SQUARE, 2); act = 1; }   // strike
        else if (in_grid(tx, ty) && dev[ty][tx] == D_LEVER) { on[ty][tx] ^= 1; act = 1; }     // throw
        else if (walkable(tx, ty)) { px = tx; py = ty; act = 1; }
    } else if (wait) act = 1;

    if (act) {
        clock_step();
        monsters_act();            // creatures move first...
        plate_step();              // ...then plates read who's standing where
        settle();                  // ...then signals propagate to the doors
        turns++;
        if (state == ST_DEAD) { if (depth > best) { best = depth; save_int("best", best); } }
        else if (terr[py][px] == T_STAIRS) descend();   // down to a deeper floor
        compute_fov();
    }
}

// ---------------------------------------------------------------------------
// spec() — headless correctness gate (node tools/spec.js). Pins each rune-gate's
// truth table and the two generation invariants that keep every floor fair:
// stairs UNREACHABLE with all gates shut (no bypass) and REACHABLE with all open.
// ---------------------------------------------------------------------------
#ifdef DE_SPEC
#include "spec.h"

// settle one gate of `type` placed at (10,7) with its levers set, return door state.
static int sp_gate(int type, int la, int lb) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { terr[y][x] = T_FLOOR; dev[y][x] = D_NONE; face[y][x] = 0; on[y][x] = 0; ctimer[y][x] = 0; }
    for (int b = 0; b < 2; b++) for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) sig[b][y][x] = 0;
    sread = 0;
    int gx = 10, gy = 7;
    stamp_gate(gx, gy, type);
    if (type == D_AND || type == D_OR || type == D_XOR) { on[gy-2][gx] = la; on[gy+2][gx] = lb; }
    else if (type == D_NOT) on[gy][gx-2] = la;           // lever (pre-thrown by stamp; override)
    else if (type == D_CLOCK) on[gy][gx] = la;           // la = clock phase
    else if (type == D_PLATE) on[gy][gx-2] = la;         // la = plate occupied
    settle();
    return on[gy][gx+2];
}

static int sp_pass(int x, int y, int doors_open) {
    if (terr[y][x] == T_WALL) return 0;
    int d = dev[y][x];
    if (d == D_DOOR) return doors_open;
    return !solid_dev(d);
}
static int sp_reach_stairs(int doors_open) {              // BFS from player; can we touch the stairs?
    unsigned char vd[GH][GW]; for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) vd[y][x] = 0;
    int hx[GW*GH], hy[GW*GH], h = 0, t = 0;
    vd[py][px] = 1; hx[t] = px; hy[t] = py; t++;
    while (h < t) {
        int x = hx[h], y = hy[h]; h++;
        if (terr[y][x] == T_STAIRS) return 1;
        for (int d = 0; d < 4; d++) {
            int dx, dy; doff(d, &dx, &dy); int nx = x + dx, ny = y + dy;
            if (in_grid(nx, ny) && !vd[ny][nx] && sp_pass(nx, ny, doors_open)) { vd[ny][nx] = 1; hx[t] = nx; hy[t] = ny; t++; }
        }
    }
    return 0;
}

void spec(void) {
    expect(sp_gate(D_AND, 1, 1) == 1, "AND opens with both levers");
    expect(sp_gate(D_AND, 1, 0) == 0, "AND stays shut with one lever");
    expect(sp_gate(D_AND, 0, 0) == 0, "AND stays shut with none");
    expect(sp_gate(D_OR,  1, 0) == 1, "OR opens with either lever");
    expect(sp_gate(D_OR,  0, 0) == 0, "OR stays shut with none");
    expect(sp_gate(D_XOR, 1, 0) == 1, "XOR opens with exactly one");
    expect(sp_gate(D_XOR, 1, 1) == 0, "XOR stays shut with both");
    expect(sp_gate(D_XOR, 0, 0) == 0, "XOR stays shut with none");
    expect(sp_gate(D_NOT, 0, 0) == 1, "NOT-trap opens when its lever is OFF");
    expect(sp_gate(D_NOT, 1, 0) == 0, "NOT-trap shut while its lever is thrown");
    expect(sp_gate(D_CLOCK, 1, 0) == 1, "CLOCK door open on its high phase");
    expect(sp_gate(D_CLOCK, 0, 0) == 0, "CLOCK door shut on its low phase");
    expect(sp_gate(D_PLATE, 1, 0) == 1, "PLATE door open while it is pressed");
    expect(sp_gate(D_PLATE, 0, 0) == 0, "PLATE door shut when nothing stands on it");

    depth = 1;
    for (int i = 0; i < 8; i++) {            // every generated floor: gates mandatory + completable
        gen_floor();
        expect(!sp_reach_stairs(0), "no bypass: stairs unreachable with every gate shut");
        expect(sp_reach_stairs(1),  "solvable: stairs reachable with every gate open");
    }
}
#endif
