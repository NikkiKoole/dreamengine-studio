/* de:meta
{
  "slug": "bomberman",
  "title": "bomberman",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "finite-state-ai",
    "tilemap-collision"
  ],
  "lineage": "Direct port of Bomberman battle mode; novel: a precomputed danger map (flame + fuse projections) that the AI reads to flee vs. hunt, giving coherent tactical behavior from a few dozen lines.",
  "genre": "arcade",
  "homage": "Bomberman (1983)",
  "description": "Battle mode: you (white) versus three AI bombers in a destructible maze of hard pillars and soft crates. Drop bombs that burst after a moment into a cross of flame — it breaks crates (sometimes revealing power-ups), chains other bombs, and kills any bomber it catches, including you. Grab power-ups: B = more bombs at once, F = bigger fire, S = more speed. The AI hunts crates and rivals but flees its own blasts. Last bomber standing wins. Arrows / WASD move, Z or X drops a bomb.",
  "todo": [
    "Better fire spritework (neighbor-aware tiling)."
  ]
}
de:meta */
#include "studio.h"

// BOMBERMAN  — battle mode
// You (white) versus three AI bombers in a destructible maze. Drop bombs; after
// a moment they burst in a cross of flame that breaks soft crates (revealing
// power-ups), chains other bombs, and kills any bomber it touches — including
// you. Grab power-ups: B = more bombs at once, F = bigger fire, S = more speed.
// Last bomber standing wins. Arrows / WASD move, Z or X drops a bomb.

#define GW    19
#define GH    11
#define TILE  16
#define OX    8
#define OY    16
#define MAXBOMB 48
#define NB    4            // bombers
#define BOMB_T  120        // fuse frames
#define FLAME_T 28

enum { EMPTY, SOFT, HARD };
enum { PU_NONE, PU_BOMB, PU_FIRE, PU_SPEED };

typedef struct { int tx, ty, owner, timer, range; bool active; } Bomb;
typedef struct {
    float x, y; int dir;
    int max_bombs, active, range, spd;
    bool alive, human;
    int col, ai_dir, ai_timer, ai_bcd;
} Bomber;

static int  tile[GH][GW];
static int  flame[GH][GW];
static int  pup[GH][GW];
static bool danger[GH][GW];
static Bomb bomb[MAXBOMB];
static Bomber bm[NB];
static int  state;        // 0 play, 1 win, 2 lose
static int  winner;

static const int BCOL[NB] = { CLR_WHITE, CLR_RED, CLR_GREEN, CLR_BLUE };

// ====================================================================

static bool is_solid_tile(int tx, int ty) {
    if (tx < 0 || tx >= GW || ty < 0 || ty >= GH) return true;
    return tile[ty][tx] != EMPTY;
}
static int bomb_at(int tx, int ty) {
    for (int i = 0; i < MAXBOMB; i++) if (bomb[i].active && bomb[i].tx == tx && bomb[i].ty == ty) return i;
    return -1;
}
static int tcx(float x) { return (int)(x / TILE); }
static float center(int t) { return t * TILE + TILE / 2; }

// solid for a moving bomber: walls always; a bomb blocks unless the bomber is still standing on it
static bool blocked(Bomber *b, int tx, int ty) {
    if (is_solid_tile(tx, ty)) return true;
    int bi = bomb_at(tx, ty);
    if (bi >= 0) { if (tcx(b->x) == tx && tcx(b->y) == ty) return false; return true; }
    return false;
}

static void gen_level() {
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            flame[y][x] = 0; pup[y][x] = PU_NONE;
            if (x == 0 || y == 0 || x == GW - 1 || y == GH - 1) tile[y][x] = HARD;
            else if (x % 2 == 0 && y % 2 == 0)                  tile[y][x] = HARD;
            else                                                tile[y][x] = (rnd(100) < 75) ? SOFT : EMPTY;
        }
    // clear the four spawn corners + their neighbours
    int cx[NB] = { 1, GW - 2, 1, GW - 2 }, cy[NB] = { 1, 1, GH - 2, GH - 2 };
    for (int i = 0; i < NB; i++) {
        int x = cx[i], y = cy[i];
        tile[y][x] = EMPTY;
        if (x + 1 < GW) tile[y][x + (x == 1 ? 1 : -1)] = EMPTY;
        if (y + 1 < GH) tile[y + (y == 1 ? 1 : -1)][x] = EMPTY;
    }
}

static void start_game() {
    gen_level();
    for (int i = 0; i < MAXBOMB; i++) bomb[i].active = false;
    int cx[NB] = { 1, GW - 2, 1, GW - 2 }, cy[NB] = { 1, 1, GH - 2, GH - 2 };
    for (int i = 0; i < NB; i++) {
        bm[i] = (Bomber){ center(cx[i]), center(cy[i]), 2, 1, 0, 2, 0, true, i == 0, BCOL[i], rnd(4), 0, 0 };
    }
    state = 0; winner = -1;
}

void init() { start_game(); }

// ---- bombs / explosions ----
static void place_bomb(int who) {
    Bomber *b = &bm[who];
    int tx = tcx(b->x), ty = tcx(b->y);
    if (b->active >= b->max_bombs || bomb_at(tx, ty) >= 0) return;
    for (int i = 0; i < MAXBOMB; i++)
        if (!bomb[i].active) { bomb[i] = (Bomb){ tx, ty, who, BOMB_T, b->range, true }; b->active++; note(48, INSTR_SQUARE, 2); return; }
}

static void explode(int bi) {
    Bomb *b = &bomb[bi];
    if (!b->active) return;
    b->active = false;
    if (bm[b->owner].active > 0) bm[b->owner].active--;
    int tx = b->tx, ty = b->ty;
    flame[ty][tx] = FLAME_T;
    note(40, INSTR_NOISE, 5);
    int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };
    for (int d = 0; d < 4; d++) {
        for (int r = 1; r <= b->range; r++) {
            int x = tx + dxs[d] * r, y = ty + dys[d] * r;
            if (x < 0 || x >= GW || y < 0 || y >= GH) break;
            if (tile[y][x] == HARD) break;
            if (tile[y][x] == SOFT) {
                tile[y][x] = EMPTY; flame[y][x] = FLAME_T;
                if (rnd(100) < 35) pup[y][x] = 1 + rnd(3);
                break;
            }
            flame[y][x] = FLAME_T;
            int ob = bomb_at(x, y);
            if (ob >= 0 && bomb[ob].timer > 1) bomb[ob].timer = 1;   // chain
        }
    }
}

static void compute_danger() {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) danger[y][x] = flame[y][x] > 0;
    int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };
    for (int i = 0; i < MAXBOMB; i++) {
        if (!bomb[i].active) continue;
        danger[bomb[i].ty][bomb[i].tx] = true;
        for (int d = 0; d < 4; d++)
            for (int r = 1; r <= bomb[i].range; r++) {
                int x = bomb[i].tx + dxs[d] * r, y = bomb[i].ty + dys[d] * r;
                if (x < 0 || x >= GW || y < 0 || y >= GH || tile[y][x] == HARD) break;
                danger[y][x] = true;
                if (tile[y][x] == SOFT) break;
            }
    }
}

// ---- movement ----
static void move_bomber(Bomber *b, int dir, float spd) {
    if (dir < 0) return;
    b->dir = dir;
    int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };
    float HWf = 6;
    if (dir < 2) {                     // horizontal — align to row centre
        float cyc = center(tcx(b->y));
        b->y += clamp(cyc - b->y, -1.6f, 1.6f);
        float nx = b->x + dxs[dir] * spd;
        int lead = (int)((nx + dxs[dir] * HWf) / TILE);
        if (!blocked(b, lead, tcx(b->y))) b->x = nx;
    } else {                            // vertical — align to column centre
        float cxc = center(tcx(b->x));
        b->x += clamp(cxc - b->x, -1.6f, 1.6f);
        float ny = b->y + dys[dir] * spd;
        int lead = (int)((ny + dys[dir] * HWf) / TILE);
        if (!blocked(b, tcx(b->x), lead)) b->y = ny;
    }
}

static void collect(Bomber *b) {
    int tx = tcx(b->x), ty = tcx(b->y);
    if (pup[ty][tx] == PU_NONE) return;
    if (pup[ty][tx] == PU_BOMB)  b->max_bombs = min(8, b->max_bombs + 1);
    if (pup[ty][tx] == PU_FIRE)  b->range = min(8, b->range + 1);
    if (pup[ty][tx] == PU_SPEED) b->spd = min(5, b->spd + 1);
    pup[ty][tx] = PU_NONE;
    note(76, INSTR_SINE, 2);
}

// ---- AI ----
static void ai_think(int who) {
    Bomber *b = &bm[who];
    int tx = tcx(b->x), ty = tcx(b->y);
    int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };

    // aligned to a tile centre? only re-decide there
    bool aligned = (abs((int)(b->x - center(tx))) < 2 && abs((int)(b->y - center(ty))) < 2);

    if (danger[ty][tx]) {                       // flee to a safe neighbour
        if (aligned) {
            int best = -1;
            for (int d = 0; d < 4; d++) {
                int nx = tx + dxs[d], ny = ty + dys[d];
                if (!is_solid_tile(nx, ny) && bomb_at(nx, ny) < 0 && !danger[ny][nx]) { best = d; break; }
            }
            if (best < 0) for (int d = 0; d < 4; d++) { int nx = tx + dxs[d], ny = ty + dys[d]; if (!is_solid_tile(nx, ny) && bomb_at(nx, ny) < 0) { best = d; break; } }
            if (best >= 0) b->ai_dir = best;
        }
        move_bomber(b, b->ai_dir, 1.2f + b->spd * 0.3f);
        return;
    }

    // wander; bomb crates / rivals when it's safe to
    if (b->ai_bcd > 0) b->ai_bcd--;
    if (aligned) {
        bool near_target = false;
        for (int d = 0; d < 4; d++) {
            int nx = tx + dxs[d], ny = ty + dys[d];
            if (tile[ny][nx] == SOFT) near_target = true;
            if (bomb_at(nx, ny) < 0)
                for (int j = 0; j < NB; j++)
                    if (j != who && bm[j].alive && tcx(bm[j].x) == nx && tcx(bm[j].y) == ny) near_target = true;
        }
        // count open exits so it doesn't trap itself
        int exits = 0;
        for (int d = 0; d < 4; d++) if (!is_solid_tile(tx + dxs[d], ty + dys[d]) && bomb_at(tx + dxs[d], ty + dys[d]) < 0) exits++;
        if (near_target && exits >= 2 && b->ai_bcd == 0 && b->active < b->max_bombs) {
            place_bomb(who); b->ai_bcd = 90; return;
        }
        // pick a new direction sometimes or when blocked
        if (b->ai_timer-- <= 0 || is_solid_tile(tx + dxs[b->ai_dir], ty + dys[b->ai_dir]) || bomb_at(tx + dxs[b->ai_dir], ty + dys[b->ai_dir]) >= 0) {
            int tries = 0, d;
            do { d = rnd(4); tries++; } while (tries < 8 && (is_solid_tile(tx + dxs[d], ty + dys[d]) || bomb_at(tx + dxs[d], ty + dys[d]) >= 0));
            b->ai_dir = d; b->ai_timer = 20 + rnd(40);
        }
    }
    move_bomber(b, b->ai_dir, 1.2f + b->spd * 0.3f);
}

void update() {
    if (state != 0) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B) || btnp(1, BTN_A)) start_game();
        return;
    }

    compute_danger();

    // human (reads both control sets so arrows or WASD work)
    if (bm[0].alive) {
        int dir = -1;
        if (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) dir = 0;
        else if (btn(0, BTN_LEFT) || btn(1, BTN_LEFT)) dir = 1;
        else if (btn(0, BTN_DOWN) || btn(1, BTN_DOWN)) dir = 2;
        else if (btn(0, BTN_UP) || btn(1, BTN_UP)) dir = 3;
        move_bomber(&bm[0], dir, 1.2f + bm[0].spd * 0.3f);
        collect(&bm[0]);
        if (btnp(0, BTN_A) || btnp(0, BTN_B) || btnp(1, BTN_A) || btnp(1, BTN_B)) place_bomb(0);
    }
    for (int i = 1; i < NB; i++) if (bm[i].alive) { ai_think(i); collect(&bm[i]); }

    // bombs tick
    for (int i = 0; i < MAXBOMB; i++)
        if (bomb[i].active && --bomb[i].timer <= 0) explode(i);

    // flames fade
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) if (flame[y][x] > 0) flame[y][x]--;

    // deaths
    int alive = 0, last = -1;
    for (int i = 0; i < NB; i++) {
        if (!bm[i].alive) continue;
        if (flame[tcx(bm[i].y)][tcx(bm[i].x)] > 0) { bm[i].alive = false; note(34, INSTR_NOISE, 6); continue; }
        alive++; last = i;
    }
    if (!bm[0].alive) { state = 2; }
    else if (alive == 1 && last == 0) { state = 1; winner = 0; }
}

// ====================================================================
// drawing
// ====================================================================

static void draw_bomber(Bomber *b) {
    if (!b->alive) return;
    int x = OX + (int)b->x, y = OY + (int)b->y;
    circfill(x, y - 2, 5, b->col);
    rectfill(x - 4, y - 1, 8, 6, b->col);
    rectfill(x - 4, y - 4, 8, 2, b->human ? CLR_BLUE : CLR_DARK_GREY);   // visor band
    pset(x - 2, y - 2, CLR_BLACK); pset(x + 2, y - 2, CLR_BLACK);
}

void draw() {
    cls(CLR_DARK_GREEN);

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int sx = OX + x * TILE, sy = OY + y * TILE;
            if (tile[y][x] == HARD) { rectfill(sx, sy, TILE, TILE, CLR_DARK_GREY); rectfill(sx + 1, sy + 1, TILE - 3, TILE - 3, CLR_LIGHT_GREY); }
            else {
                rectfill(sx, sy, TILE, TILE, ((x + y) & 1) ? CLR_DARK_GREEN : CLR_DARKER_PURPLE);
                if (tile[y][x] == SOFT) { rectfill(sx + 1, sy + 1, TILE - 2, TILE - 2, CLR_BROWN); rect(sx + 2, sy + 2, TILE - 4, TILE - 4, CLR_DARK_BROWN); }
            }
            if (pup[y][x] != PU_NONE && tile[y][x] == EMPTY) {
                int c = pup[y][x] == PU_BOMB ? CLR_PINK : pup[y][x] == PU_FIRE ? CLR_ORANGE : CLR_BLUE;
                rectfill(sx + 3, sy + 3, TILE - 6, TILE - 6, c);
                print(pup[y][x] == PU_BOMB ? "B" : pup[y][x] == PU_FIRE ? "F" : "S", sx + 5, sy + 4, CLR_BLACK);
            }
        }

    // bombs
    for (int i = 0; i < MAXBOMB; i++) {
        if (!bomb[i].active) continue;
        int sx = OX + bomb[i].tx * TILE + TILE / 2, sy = OY + bomb[i].ty * TILE + TILE / 2;
        int r = 5 + ((bomb[i].timer / 6) % 2);
        circfill(sx, sy, r, CLR_BLACK);
        circ(sx, sy, r, CLR_LIGHT_GREY);
        pset(sx, sy - r - 1, CLR_ORANGE);
    }
    // flames
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (flame[y][x] > 0) {
                int sx = OX + x * TILE, sy = OY + y * TILE;
                rectfill(sx + 1, sy + 1, TILE - 2, TILE - 2, blink(3) ? CLR_YELLOW : CLR_ORANGE);
            }

    for (int i = 0; i < NB; i++) draw_bomber(&bm[i]);

    // HUD
    print(str("BOMBS %d", bm[0].max_bombs), 4, 3, CLR_WHITE);
    print(str("FIRE %d", bm[0].range), 84, 3, CLR_ORANGE);
    print(str("SPD %d", bm[0].spd), 150, 3, CLR_BLUE);
    int alive = 0; for (int i = 0; i < NB; i++) if (bm[i].alive) alive++;
    print_right(str("ALIVE %d", alive), SCREEN_W - 4, 3, CLR_LIGHT_GREY);

    if (state == 1) {
        rectfill(SCREEN_W / 2 - 64, SCREEN_H / 2 - 18, 128, 40, CLR_BLACK);
        rect    (SCREEN_W / 2 - 64, SCREEN_H / 2 - 18, 128, 40, CLR_GREEN);
        print_centered("YOU WIN!", SCREEN_W/2, SCREEN_H / 2 - 9, CLR_GREEN);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H / 2 + 5, CLR_LIGHT_GREY);
    }
    if (state == 2) {
        rectfill(SCREEN_W / 2 - 64, SCREEN_H / 2 - 18, 128, 40, CLR_BLACK);
        rect    (SCREEN_W / 2 - 64, SCREEN_H / 2 - 18, 128, 40, CLR_RED);
        print_centered("BOOM — YOU LOSE", SCREEN_W/2, SCREEN_H / 2 - 9, CLR_RED);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H / 2 + 5, CLR_LIGHT_GREY);
    }
}
