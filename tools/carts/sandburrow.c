/* de:meta
{
  "title": "Sand Burrow",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "cellular-automata",
    "grid-movement",
    "title-play-gameover-loop"
  ],
  "lineage": "Directly ports the falling-sand automaton from sand.c and embeds it as live terrain in a Boulderdash-style dig-and-collect game.",
  "genre": "arcade",
  "description": "A whole cave of live falling-sand: dig tunnels and the sand above pours down to refill them — grab every gem and reach the exit before a collapsing column buries you. A/D or arrows walk + dig sideways, W/up climb + dig up, S/down dig straight down, Z/X/R restart."
}
de:meta */
#include "studio.h"
#include <string.h>

// ── SAND BURROW — a Boulderdash dig dropped into terrain that actually flows ──
//
// The whole cave is live falling-sand cells (sand.c's automaton: a grain falls
// straight down, else slides to an open diagonal, so columns slump and collapse).
// You are a little miner who DIGS: walk into sand and the cells in front of you
// are scooped away, opening a tunnel. But the sand ABOVE then pours down to refill
// it — carve a vertical shaft and linger under a loaded column and you get BURIED.
// Scatter gems through the cave; grab them all to open the exit. Reach it alive.
//
//   A/D or ←/→  walk + dig sideways      W or ↑  climb / dig up
//   S or ↓      dig straight down        Z/X or R  restart
//
// Burial is real: stand in packed sand too long (the meter fills) and you suffocate.
// Keep moving, keep an air pocket above your head.

// ── grid ─────────────────────────────────────────────────────────────
#define GW   160          // cells wide
#define GH   100          // cells tall
#define CELL 2            // pixels per cell  → 320 × 200

enum { EMPTY, SAND, ROCK, GEM, EXITC };   // cell materials
static unsigned char cells[GW * GH];

// ── player (a small box measured in cells) ─────────────────────────────
#define PW 5              // player width  in cells
#define PH 8              // player height in cells

static float px, py;      // top-left of the player box, in cell units (float for smooth fall)
static int   facing;      // -1 left, +1 right
static float bury;        // 0..1 suffocation meter
static int   gems, quota;
static bool  won, dead;
static const char *death_msg;

static float settle_acc;  // automaton tick accumulator
static int   start_cx, start_cy;

// ── helpers ────────────────────────────────────────────────────────────
static int  idx(int x, int y) { return y * GW + x; }
static bool inb(int x, int y) { return x >= 0 && x < GW && y >= 0 && y < GH; }
static unsigned char at(int x, int y) { return inb(x, y) ? cells[idx(x, y)] : ROCK; } // OOB reads as solid wall
static bool blocking(unsigned char m) { return m == SAND || m == ROCK; }              // stops the player

static void swp(int i, int j) { unsigned char t = cells[i]; cells[i] = cells[j]; cells[j] = t; }

// ── falling-sand automaton (ported from sand.c) ────────────────────────
// Scan bottom-up. A grain falls into empty below; else slides into an open
// diagonal so piles slope and shafts collapse. ROCK / GEM / EXIT never move
// (gems sit in carved pockets so they stay grabbable). We forbid sand from
// flowing into the cells the player currently occupies — the miner's body
// holds back the wall — but the instant he steps away, it pours in.
static int pminx, pminy, pmaxx, pmaxy;   // player's occupied cell box, set each frame

static bool is_player_cell(int x, int y) {
    return x >= pminx && x <= pmaxx && y >= pminy && y <= pmaxy;
}

static void settle(void) {
    for (int y = GH - 2; y >= 0; y--) {
        int dir = ((y + frame()) & 1) ? 1 : -1;     // alternate scan bias → symmetric piles
        for (int xi = 0; xi < GW; xi++) {
            int x = dir > 0 ? xi : GW - 1 - xi;
            int i = idx(x, y);
            if (cells[i] != SAND) continue;
            int below = i + GW;
            if (cells[below] == EMPTY && !is_player_cell(x, y + 1)) { swp(i, below); continue; }
            bool okl = x > 0      && cells[below - 1] == EMPTY && !is_player_cell(x - 1, y + 1);
            bool okr = x < GW - 1 && cells[below + 1] == EMPTY && !is_player_cell(x + 1, y + 1);
            if (okl && okr) swp(i, rnd(2) ? below - 1 : below + 1);
            else if (okl)   swp(i, below - 1);
            else if (okr)   swp(i, below + 1);
        }
    }
}

// ── collision queries over the player box at a candidate (top-left) cell ──
static bool box_hits_solid(int bx, int by) {
    for (int y = by; y < by + PH; y++)
        for (int x = bx; x < bx + PW; x++)
            if (blocking(at(x, y))) return true;
    return false;
}

// how many sand cells currently overlap the player's body (for the bury meter)
static int sand_in_box(int bx, int by) {
    int n = 0;
    for (int y = by; y < by + PH; y++)
        for (int x = bx; x < bx + PW; x++)
            if (at(x, y) == SAND) n++;
    return n;
}

// scoop the sand cells in the column/row the miner is digging into
static void dig(int dirx, int diry) {
    int bx = (int)(px + 0.5f), by = (int)(py + 0.5f);
    if (dirx) {                                   // dig the vertical edge ahead
        int col = dirx > 0 ? bx + PW : bx - 1;
        for (int y = by; y < by + PH; y++) {
            if (at(col, y) == SAND)      cells[idx(col, y)] = EMPTY;
            if (at(col, y) == GEM)       { /* collected on touch below */ }
        }
    } else if (diry > 0) {                        // dig the row below
        int row = by + PH;
        for (int x = bx; x < bx + PW; x++)
            if (at(x, row) == SAND) cells[idx(x, row)] = EMPTY;
    } else if (diry < 0) {                        // dig the row above (climb)
        int row = by - 1;
        for (int x = bx; x < bx + PW; x++)
            if (at(x, row) == SAND) cells[idx(x, row)] = EMPTY;
    }
}

// pick up any gem cells the body overlaps; touch the exit
static void collect_and_exit(void) {
    int bx = (int)(px + 0.5f), by = (int)(py + 0.5f);
    for (int y = by - 1; y < by + PH + 1; y++)
        for (int x = bx - 1; x < bx + PW + 1; x++) {
            if (!inb(x, y)) continue;
            int i = idx(x, y);
            if (cells[i] == GEM) {
                cells[i] = EMPTY; gems++;
                note(84, INSTR_SINE, 4); hit(91, INSTR_SINE, 4, 90);
            } else if (cells[i] == EXITC && gems >= quota) {
                won = true;
                strum(67, CHORD_MAJ7, INSTR_TRI, 5, 55);
            }
        }
}

// ── level build ────────────────────────────────────────────────────────
static void carve_pocket(int cx, int cy, int r) {     // small empty bubble
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++)
            if (inb(x, y) && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r)
                cells[idx(x, y)] = EMPTY;
}

static void build(void) {
    gems = 0; quota = 0; bury = 0; won = dead = false; settle_acc = 0;
    death_msg = "";

    // fill the cave with sand, ringed by an indestructible rock border
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int i = idx(x, y);
            bool border = x < 2 || x >= GW - 2 || y >= GH - 2 || y < 2;
            cells[i] = border ? ROCK : SAND;
        }

    // a few solid rock ledges/pillars so not everything collapses — they make
    // the sand drape and slump around hard edges, and give safe footing.
    for (int x = 30; x < 70; x++)  { cells[idx(x, 55)] = ROCK; cells[idx(x, 56)] = ROCK; }
    for (int x = 95; x < 140; x++) { cells[idx(x, 40)] = ROCK; cells[idx(x, 41)] = ROCK; }
    for (int y = 30; y < 70; y++)  { cells[idx(78, y)] = ROCK; cells[idx(79, y)] = ROCK; }
    for (int x = 12; x < 50; x++)  { cells[idx(x, 80)] = ROCK; cells[idx(x, 81)] = ROCK; }
    for (int x = 100; x < 150; x++){ cells[idx(x, 72)] = ROCK; cells[idx(x, 73)] = ROCK; }

    // gems sit in carved pockets so they hang in cleared space (you tunnel to them)
    static const int gx[] = { 24, 50, 88, 120, 145, 40, 110, 66 };
    static const int gy[] = { 70, 30, 26, 55,  60,  90, 60,  90 };
    int ng = (int)(sizeof(gx) / sizeof(gx[0]));
    for (int g = 0; g < ng; g++) {
        carve_pocket(gx[g], gy[g], 2);
        cells[idx(gx[g], gy[g])] = GEM;
        quota++;
    }

    // the exit: a rock-framed door low-right, lit once the quota is met
    for (int y = GH - 9; y < GH - 2; y++)
        for (int x = GW - 9; x < GW - 2; x++) {
            cells[idx(x, y)] = (x == GW - 9 || x == GW - 3 || y == GH - 9) ? ROCK : EMPTY;
        }
    cells[idx(GW - 6, GH - 4)] = EXITC;
    carve_pocket(GW - 6, GH - 5, 1);

    // player start: a roomy top-left chamber so the first move is free
    start_cx = 8; start_cy = 6;
    px = start_cx; py = start_cy;
    for (int y = 4; y < 4 + PH + 6; y++)
        for (int x = 5; x < 5 + PW + 10; x++)
            if (inb(x, y) && cells[idx(x, y)] != ROCK) cells[idx(x, y)] = EMPTY;

    facing = 1;

    // let the world settle into rest before play starts
    for (int i = 0; i < 60; i++) settle();
}

void init(void) { build(); }

// ── update ───────────────────────────────────────────────────────────
void update(void) {
    if (won || dead) {
        if (keyp('R') || key('R') || btnp(0, BTN_A) || btnp(0, BTN_B) ||
            keyp(KEY_SPACE) || keyp(KEY_ENTER)) build();
        return;
    }

    // mark the player's occupied cell box so the automaton respects his body
    int bx = (int)(px + 0.5f), by = (int)(py + 0.5f);
    pminx = bx; pminy = by; pmaxx = bx + PW - 1; pmaxy = by + PH - 1;

    // ── horizontal: walk + dig sideways ──
    int wantx = 0;
    if (btn(0, BTN_LEFT))  wantx = -1;
    if (btn(0, BTN_RIGHT)) wantx =  1;
    if (wantx) {
        facing = wantx;
        float nx = px + wantx * 0.6f;
        int   tx = (int)(nx + 0.5f);
        if (!box_hits_solid(tx, by)) px = nx;             // open lane → glide
        else { dig(wantx, 0); note(38 + rnd(3), INSTR_NOISE, 1); }  // wall → scoop it
    }

    // ── climb / dig up ──
    if (btn(0, BTN_UP)) {
        bx = (int)(px + 0.5f);
        if (!box_hits_solid(bx, by - 1)) py -= 0.55f;     // open above → climb
        else { dig(0, -1); note(36 + rnd(3), INSTR_NOISE, 1); }
    }

    // ── dig straight down ──
    if (btn(0, BTN_DOWN)) { dig(0, 1); note(34 + rnd(3), INSTR_NOISE, 1); }

    // ── gravity: the miner falls when nothing solid is under his feet ──
    bx = (int)(px + 0.5f); by = (int)(py + 0.5f);
    if (!box_hits_solid(bx, by + 1)) {
        py += 0.5f;
    } else {
        // settle onto the surface (snap up out of any cell he sank into)
        int snap = by;
        while (snap > 0 && box_hits_solid(bx, snap)) snap--;
        py = snap;
    }

    // clamp inside the rock border
    if (px < 2) px = 2;
    if (px > GW - 2 - PW) px = GW - 2 - PW;
    if (py < 2) py = 2;
    if (py > GH - 2 - PH) py = GH - 2 - PH;

    collect_and_exit();

    // ── burial: count sand packed into the body; meter fills, then suffocate ──
    bx = (int)(px + 0.5f); by = (int)(py + 0.5f);
    int packed = sand_in_box(bx, by);
    int packed_cap = PW * PH;
    float pload = (float)packed / (float)(packed_cap / 2);   // half-full body = max danger rate
    if (pload > 1) pload = 1;
    if (packed > 2) {
        bury += pload * dt() * 0.9f;
        if (bury > 0.35f && chance(8)) shake(pload * 3);     // struggling rumble
    } else {
        bury -= dt() * 1.4f;                                // breathing room → recover
    }
    if (bury < 0) bury = 0;
    if (bury >= 1.0f) {
        dead = true; death_msg = "BURIED ALIVE";
        shake(6); note(33, INSTR_SAW, 6); note(28, INSTR_SAW, 6);
    }

    // ── the falling-sand world ticks at a steady cadence (reads as a cascade) ──
    settle_acc += dt();
    while (settle_acc >= 0.045f) { settle_acc -= 0.045f; settle(); }
}

// ── drawing ────────────────────────────────────────────────────────────
static int sand_color(int x, int y) {
    // two-tone speckle so piles read as grainy, with a darker damp band deep down
    int h = (x * 13 + y * 7) & 3;
    if (y > 78) return h == 0 ? CLR_BROWN : CLR_DARK_BROWN;          // damp depths
    return h == 0 ? CLR_YELLOW : CLR_ORANGE;
}

static void draw_player(void) {
    int x = (int)(px + 0.5f) * CELL, y = (int)(py + 0.5f) * CELL;
    int w = PW * CELL, h = PH * CELL;
    int cx = x + w / 2;

    // legs
    rectfill(x + 1, y + h - 4, 3, 4, CLR_DARK_BLUE);
    rectfill(x + w - 4, y + h - 4, 3, 4, CLR_DARK_BLUE);
    // body — color shifts toward red as burial rises (the danger tell)
    int body = bury > 0.6f ? (blink(4) ? CLR_RED : CLR_WHITE)
             : bury > 0.3f ? CLR_LIGHT_PEACH : CLR_WHITE;
    rectfill(x + 1, y + 4, w - 2, h - 7, body);
    // head + miner cap
    circfill(cx, y + 4, 3, CLR_LIGHT_PEACH);
    rectfill(x + 1, y, w - 2, 3, CLR_RED);
    pset(cx + facing, y + 4, CLR_BLACK);                 // eye, looks where he faces
    // helmet lamp glow when digging in the dark
    if (frame() % 30 < 18) pset(cx, y, CLR_YELLOW);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // draw every non-empty cell as a CELL×CELL block
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            unsigned char m = cells[idx(x, y)];
            if (m == EMPTY) continue;
            int sx = x * CELL, sy = y * CELL, col;
            if      (m == SAND) col = sand_color(x, y);
            else if (m == ROCK) col = ((x * 5 + y * 3) & 3) == 0 ? CLR_LIGHT_GREY : CLR_DARK_GREY;
            else if (m == GEM)  col = blink(8) ? CLR_WHITE : CLR_BLUE;
            else /* EXITC */    col = (gems >= quota) ? (blink(6) ? CLR_YELLOW : CLR_GREEN) : CLR_DARK_GREEN;
            rectfill(sx, sy, CELL, CELL, col);
        }

    draw_player();

    // ── HUD ──
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("SAND BURROW", 4, 2, CLR_ORANGE);
    int gc = gems >= quota ? CLR_GREEN : CLR_LIGHT_YELLOW;
    print_centered(str("GEMS %d/%d", gems, quota), SCREEN_W/2, 2, gc);

    // burial meter — fills red, the core tension readout
    int bw = 60, bxx = SCREEN_W - bw - 4;
    print_right("AIR", bxx - 2, 2, bury > 0.5f ? CLR_RED : CLR_LIGHT_GREY);
    bar(bxx, 2, bw, 7, 1.0f - bury, bury > 0.6f ? CLR_RED : CLR_GREEN, CLR_DARKER_GREY);

    if (gems >= quota && !won && !dead)
        print_centered(blink(12) ? "EXIT OPEN — head bottom-right" : "", SCREEN_W/2, 200 - 9, CLR_GREEN);

    if (dead) {
        fade(0.55f);
        int w = 220, bxw = (SCREEN_W - w) / 2;
        rectfill(bxw, 80, w, 42, CLR_DARK_RED);
        rect(bxw, 80, w, 42, CLR_RED);
        print_centered(death_msg, SCREEN_W/2, 90, CLR_WHITE);
        print_centered("keep an air pocket overhead!", SCREEN_W/2, 102, CLR_LIGHT_PEACH);
        print_centered("Z / R to dig again", SCREEN_W/2, 112, CLR_LIGHT_GREY);
    }
    if (won) {
        int w = 220, bxw = (SCREEN_W - w) / 2;
        rectfill(bxw, 80, w, 42, CLR_DARK_GREEN);
        rect(bxw, 80, w, 42, CLR_LIME_GREEN);
        print_centered("ESCAPED THE BURROW!", SCREEN_W/2, 90, CLR_WHITE);
        print_centered(str("all %d gems out alive", quota), SCREEN_W/2, 102, CLR_LIGHT_YELLOW);
        print_centered("Z / R to dig again", SCREEN_W/2, 112, CLR_LIGHT_GREY);
    }
}
