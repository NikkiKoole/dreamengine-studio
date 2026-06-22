#include "studio.h"
#include <stdio.h>

// RUNECRAWL
// A logic-puzzle roguelike. Explore a dungeon by torchlight (turn-based, with a
// fog-of-war you reveal as you go) and reach the stairs. The catch: the way is
// barred by a portcullis wired to ancient runes — a glyph-gate fed by two
// levers. Throw BOTH and the AND-rune powers the line, the bars grind open.
// The signal layer from the `logic` cart, made diegetic: one circuit tick per
// step you take, so the dungeon and the machine share a turn.
//
// Arrows / WASD: move (walk into a lever to throw it).  Z or .: wait a turn.
// This is a hand-authored single floor — the vertical slice of the planned
// circuit-dungeon (procedural floors, more gates, monsters come later).

#define GW    26
#define GH    15
#define TILE  12
#define FOV_R 6

enum { T_WALL, T_FLOOR, T_STAIRS };
enum { D_NONE, D_WIRE, D_LEVER, D_AND, D_NOT, D_DOOR, D_OR };
enum { DN, DE, DS, DW };

static unsigned char terr[GH][GW];
static unsigned char dev[GH][GW];
static unsigned char face[GH][GW];
static unsigned char on[GH][GW];      // lever thrown / door open
static int  sig[2][GH][GW], sread;
static int  qx[GW*GH], qy[GW*GH], seedv[GH][GW];
static unsigned char seen[GH][GW], vis[GH][GW];

static int px, py, won, turns;

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

// ---------------------------------------------------------------------------
// signal sim — the logic cart's MechUpdateSignals, trimmed to lever/wire/AND/NOT
// + a door sink. One settle() per player turn.
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
        if (dev[y][x] == D_LEVER) { if (on[y][x]) seed_adj(ns, x, y, 1, &qt); }
        else if (dev[y][x] == D_NOT) {
            int dx, dy; doff(opp(face[y][x]), &dx, &dy);
            if (!rsig(x + dx, y + dy)) seed_face(ns, x, y, face[y][x], 1, &qt);
        } else if (dev[y][x] == D_AND || dev[y][x] == D_OR) {
            int a, b; gins(face[y][x], &a, &b);
            int dx, dy; doff(a, &dx, &dy); int ia = rsig(x + dx, y + dy);
            doff(b, &dx, &dy); int ib = rsig(x + dx, y + dy);
            int o = dev[y][x] == D_AND ? (ia && ib) : (ia || ib);
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

// ---------------------------------------------------------------------------
// map
// ---------------------------------------------------------------------------
static void fill(int x0, int y0, int x1, int y1, int t) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (in_grid(x, y)) terr[y][x] = t;
}
static void setdev(int x, int y, int d, int f) { terr[y][x] = T_FLOOR; dev[y][x] = d; face[y][x] = f; on[y][x] = 0; }

static void build(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { terr[y][x] = T_WALL; dev[y][x] = D_NONE; face[y][x] = 0; on[y][x] = 0; }
    for (int b = 0; b < 2; b++) for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) sig[b][y][x] = 0;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) seen[y][x] = vis[y][x] = 0;
    sread = 0;

    fill(1, 5, 6, 9, T_FLOOR);     // start room
    fill(6, 7, 24, 7, T_FLOOR);    // spine corridor (row 7)
    fill(6, 2, 10, 5, T_FLOOR);    // AND chamber (above-left, links to start room at col6)
    fill(15, 8, 17, 12, T_FLOOR);  // NOT chamber (below, all WEST of door B — no bypass)
    fill(21, 5, 24, 9, T_FLOOR);   // goal room
    terr[7][22] = T_STAIRS;

    // Puzzle 1 — AND: both levers ON. gate (8,4) faces S; inputs W(7,4) & E(9,4); out S(8,5).
    setdev(8, 4, D_AND, DS);
    setdev(7, 4, D_WIRE, 0); setdev(9, 4, D_WIRE, 0);
    setdev(6, 4, D_LEVER, 0); setdev(10, 4, D_LEVER, 0);
    setdev(8, 5, D_WIRE, 0); setdev(8, 6, D_WIRE, 0);     // output down to door A
    setdev(8, 7, D_DOOR, DN);                              // door A on the spine

    // Puzzle 2 — NOT (the trap): lever starts THROWN, so the door is shut. Turn it OFF to open.
    setdev(15, 9, D_NOT, DN);                              // NOT faces N; input S(15,10), out N(15,8)
    setdev(15, 10, D_WIRE, 0);
    setdev(15, 11, D_LEVER, 0); on[11][15] = 1;            // pre-thrown ON  -> NOT -> door shut
    setdev(15, 8, D_WIRE, 0); setdev(16, 8, D_WIRE, 0);    // output run east to the door
    setdev(17, 8, D_WIRE, 0); setdev(18, 8, D_WIRE, 0);
    setdev(18, 7, D_DOOR, DN);                              // door B on the spine

    px = 3; py = 7; won = 0; turns = 0;
}

// ---------------------------------------------------------------------------
// movement + fov
// ---------------------------------------------------------------------------
static int solid_dev(int d) { return d == D_LEVER || d == D_AND || d == D_NOT || d == D_OR; }
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
        if (!(x == x0 && y == y0) && opaque(x, y)) return;   // a wall blocks sight past it
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

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------
static int powered(int x, int y) { return sig[sread][y][x] > 0; }

static void draw_wire(int X, int Y, int x, int y, int lit) {
    int cx = X + 6, cy = Y + 6;
    int p = powered(x, y);
    int col = !lit ? CLR_DARKER_BLUE : p ? CLR_LIGHT_YELLOW : CLR_TRUE_BLUE;
    for (int d = 0; d < 4; d++) {
        int dx, dy; doff(d, &dx, &dy);
        if (in_grid(x + dx, y + dy) && dev[y + dy][x + dx] != D_NONE)
            line(cx, cy, cx + dx * 6, cy + dy * 6, col);
    }
    if (p && lit) { pset(cx, cy, CLR_WHITE); pset(cx + 1, cy, CLR_WHITE); }
    else pset(cx, cy, col);
}

static void draw_cell(int x, int y) {
    if (!seen[y][x]) return;
    int lit = vis[y][x];
    int X = x * TILE, Y = y * TILE, cx = X + 6, cy = Y + 6;

    if (terr[y][x] == T_WALL) {
        rectfill(X, Y, TILE, TILE, lit ? CLR_DARK_GREY : CLR_DARKER_GREY);
        rect(X, Y, TILE, TILE, lit ? CLR_MEDIUM_GREY : CLR_BROWNISH_BLACK);
        return;
    }
    // floor / stairs base
    rectfill(X, Y, TILE, TILE, lit ? CLR_BROWNISH_BLACK : CLR_BLACK);
    if (terr[y][x] == T_STAIRS) { font(FONT_NORMAL); print(">", cx - 3, cy - 3, lit ? CLR_YELLOW : CLR_DARK_GREY); }

    switch (dev[y][x]) {
        case D_WIRE: draw_wire(X, Y, x, y, lit); break;
        case D_LEVER: {
            int o = on[y][x];
            int col = !lit ? CLR_DARK_GREY : o ? CLR_ORANGE : CLR_LIGHT_GREY;
            rectfill(cx - 2, Y + 8, 4, 3, lit ? CLR_DARKER_GREY : CLR_BROWNISH_BLACK);  // base
            line(cx, Y + 9, cx + (o ? 3 : -3), Y + 3, col);                              // handle
            pset(cx + (o ? 3 : -3), Y + 3, col);
            break;
        }
        case D_AND: case D_NOT: case D_OR: {
            int hot = 0;   // the rune glows when its output wire carries signal
            int dx, dy; doff(face[y][x], &dx, &dy);
            if (in_grid(x + dx, y + dy)) hot = powered(x + dx, y + dy);
            int col = !lit ? CLR_DARKER_PURPLE : hot ? CLR_PINK : CLR_INDIGO;
            rectfill(X + 1, Y + 1, TILE - 2, TILE - 2, lit ? CLR_DARKER_PURPLE : CLR_BLACK);
            rect(X + 1, Y + 1, TILE - 2, TILE - 2, col);
            font(FONT_SMALL); print(dev[y][x] == D_AND ? "&" : dev[y][x] == D_OR ? "|" : "!", cx - 1, cy - 2, col);
            break;
        }
        case D_DOOR: {
            int open = on[y][x];
            int col = !lit ? CLR_DARK_GREY : CLR_MEDIUM_GREY;
            if (open) { for (int i = 0; i < 3; i++) rectfill(X + 2 + i * 4, Y, 2, 3, col); }   // retracted stubs
            else      { for (int i = 0; i < 3; i++) rectfill(X + 2 + i * 4, Y, 2, TILE, col); } // bars down
            break;
        }
    }
}

static void draw_hud(void) {
    rectfill(0, GH * TILE, SCREEN_W, SCREEN_H - GH * TILE, CLR_BLACK);
    font(FONT_SMALL);
    if (won) {
        print("YOU ESCAPED THE VAULT", 6, GH * TILE + 4, CLR_LIME_GREEN);
        print("R: again", 6, GH * TILE + 13, CLR_LIGHT_GREY);
    } else {
        int da = on[7][8], db = on[7][18];   // gate A (AND), gate B (NOT)
        const char *msg = (da && db) ? "both gates yield - the stairs lie open"
                        : da          ? "the AND-gate opens - now the NOT-rune ahead"
                        :               "two levers feed the AND-rune that bars the way";
        print(msg, 6, GH * TILE + 4, (da && db) ? CLR_LIME_GREEN : da ? CLR_YELLOW : CLR_LIGHT_GREY);
        char t[24]; snprintf(t, sizeof t, "turn %d", turns);
        print(t, SCREEN_W - 48, GH * TILE + 4, CLR_DARK_GREY);
        print("arrows move - walk into a lever to throw it", 6, GH * TILE + 13, CLR_DARK_GREY);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) draw_cell(x, y);
    // player
    int X = px * TILE, Y = py * TILE;
    font(FONT_NORMAL); print("@", X + 3, Y + 3, CLR_WHITE);
    draw_hud();
}

// ---------------------------------------------------------------------------
// turn loop
// ---------------------------------------------------------------------------
void init(void) { build(); compute_fov(); }

void update(void) {
    if (won) { if (keyp('R')) { build(); compute_fov(); } return; }

    int mvx = 0, mvy = 0, wait = 0, act = 0;
    if (btnp(0, BTN_LEFT)  || keyp(KEY_LEFT)  || keyp('A')) mvx = -1;
    else if (btnp(0, BTN_RIGHT) || keyp(KEY_RIGHT) || keyp('D')) mvx = 1;
    else if (btnp(0, BTN_UP)    || keyp(KEY_UP)    || keyp('W')) mvy = -1;
    else if (btnp(0, BTN_DOWN)  || keyp(KEY_DOWN)  || keyp('S')) mvy = 1;
    else if (keyp('Z') || keyp('.')) wait = 1;

    if (mvx || mvy) {
        int tx = px + mvx, ty = py + mvy;
        if (in_grid(tx, ty) && dev[ty][tx] == D_LEVER) { on[ty][tx] ^= 1; act = 1; }   // bump = throw
        else if (walkable(tx, ty)) { px = tx; py = ty; act = 1; }
    } else if (wait) act = 1;

    if (act) {
        settle();
        turns++;
        compute_fov();
        if (terr[py][px] == T_STAIRS) won = 1;
    }
}
