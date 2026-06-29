/* de:meta
{
  "title": "rogue",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "dungeon-generation",
    "turn-based-loop",
    "finite-state-ai",
    "save-load-persistence"
  ],
  "lineage": "Rogue (1980) in 350 lines — BSP-free room-in-cell grid generation with L-corridor chains, Bresenham radial FOV, and bump-to-attack turns; the FOV and procedural floor generation are the two learnable cores.",
  "genre": "rpg",
  "homage": "Rogue (1980)",
  "description": "The original dungeon crawl. Every floor is procedurally generated — rooms joined by corridors — and revealed by line-of-sight fog of war as you explore. Turn-based: walk into a monster (rats, kobolds, orcs, trolls) to attack it, and it hunts you back. Grab gold and potions, gain XP and level up, and take the stairs deeper. Find the Amulet of Yendor on floor 8 to win. Death is permanent. Arrows / WASD move + attack, A quaffs a potion, B descends stairs."
}
de:meta */
#include "studio.h"

// ROGUE
// A classic dungeon crawl. Explore procedurally-built floors, fight what hunts
// you (walk into it to attack), grab gold and potions, and take the stairs ">"
// deeper. Find the Amulet of Yendor on floor 8 to win. Death is permanent.
// Arrows / WASD: move + attack    A: quaff a potion    B: descend stairs

#define GW    40
#define GH    22
#define TILE  8
#define MAXR  12
#define MAXM  40
#define MAXI  40
#define R_FOV 6
#define AMULET_DEPTH 8

enum { WALL, FLOOR, STAIRS };
enum { PLAY, DEAD, WIN };

// monster types: rat, kobold, orc, troll
static const int   MHP[4]    = { 3, 6, 10, 18 };
static const int   MDMG[4]   = { 1, 2, 3, 5 };
static const int   MXP[4]    = { 2, 4, 8, 15 };
static const char  MGLY[4]   = { 'r', 'k', 'o', 'T' };
static const int   MCOL[4]   = { CLR_MEDIUM_GREY, CLR_GREEN, CLR_RED, CLR_LIME_GREEN };
static const char *MNAME[4]  = { "rat", "kobold", "orc", "troll" };

enum { IT_GOLD, IT_POTION, IT_AMULET };

typedef struct { int x, y, hp, type; bool alive; } Mon;
typedef struct { int x, y, type, amount; bool active; } Item;

static int  dmap[GH][GW];
static bool explored[GH][GW], visible[GH][GW];
static Mon  mon[MAXM];
static Item item[MAXI];

static int px, py, hp, maxhp, atk, gold, potions, plevel, xp, depth;
static int score, hiscore, state;
static int mcd;                // movement cooldown (held-key repeat)
static char msg[80];

// ====================================================================

static void set_msg(const char *s) { int i = 0; while (s[i] && i < 79) { msg[i] = s[i]; i++; } msg[i] = 0; }

static void dig_h(int x1, int x2, int y) { for (int x = min(x1, x2); x <= max(x1, x2); x++) if (dmap[y][x] == WALL) dmap[y][x] = FLOOR; }
static void dig_v(int y1, int y2, int x) { for (int y = min(y1, y2); y <= max(y1, y2); y++) if (dmap[y][x] == WALL) dmap[y][x] = FLOOR; }

static int monster_at(int x, int y) {
    for (int i = 0; i < MAXM; i++) if (mon[i].alive && mon[i].x == x && mon[i].y == y) return i;
    return -1;
}

static int pick_type(int d) {
    int r = rnd(8) + d;
    if (r < 4) return 0;
    if (r < 9) return 1;
    if (r < 13) return 2;
    return 3;
}

static void gen_level() {
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) { dmap[y][x] = WALL; explored[y][x] = false; visible[y][x] = false; }
    for (int i = 0; i < MAXM; i++) mon[i].alive = false;
    for (int i = 0; i < MAXI; i++) item[i].active = false;

    int rcx[MAXR], rcy[MAXR], rx0[MAXR], ry0[MAXR], rw_[MAXR], rh_[MAXR], nr = 0;
    for (int cj = 0; cj < 3; cj++)
        for (int ci = 0; ci < 4; ci++) {
            int cellx = ci * 10, celly = cj * 7;
            int rw = rnd_between(4, 9), rh = rnd_between(3, 6);
            int rx = cellx + 1 + rnd(10 - rw - 1);
            int ry = celly + 1 + rnd(7 - rh - 1);
            for (int y = ry; y < ry + rh; y++)
                for (int x = rx; x < rx + rw; x++) dmap[y][x] = FLOOR;
            rx0[nr] = rx; ry0[nr] = ry; rw_[nr] = rw; rh_[nr] = rh;
            rcx[nr] = rx + rw / 2; rcy[nr] = ry + rh / 2; nr++;
        }
    // connect rooms in a chain → always fully reachable
    for (int i = 1; i < nr; i++) {
        dig_h(rcx[i - 1], rcx[i], rcy[i - 1]);
        dig_v(rcy[i - 1], rcy[i], rcx[i]);
    }

    px = rcx[0]; py = rcy[0];
    dmap[rcy[nr - 1]][rcx[nr - 1]] = STAIRS;

    // monsters
    int nm = min(MAXM, 4 + depth);
    for (int i = 0; i < nm; i++) {
        int r = rnd_between(1, nr);                      // not the start room
        int x = rx0[r] + rnd(rw_[r]), y = ry0[r] + rnd(rh_[r]);
        if (dmap[y][x] != FLOOR || monster_at(x, y) >= 0) continue;
        int t = pick_type(depth);
        mon[i].x = x; mon[i].y = y; mon[i].type = t; mon[i].hp = MHP[t]; mon[i].alive = true;
    }
    // items
    int ii = 0;
    int ng = 3 + rnd(3);
    for (int g = 0; g < ng && ii < MAXI; g++) {
        int r = rnd(nr); int x = rx0[r] + rnd(rw_[r]), y = ry0[r] + rnd(rh_[r]);
        if (dmap[y][x] != FLOOR) continue;
        item[ii].x = x; item[ii].y = y; item[ii].type = IT_GOLD; item[ii].amount = 5 + rnd(20); item[ii].active = true; ii++;
    }
    int npot = rnd(3);
    for (int p = 0; p < npot && ii < MAXI; p++) {
        int r = rnd(nr); int x = rx0[r] + rnd(rw_[r]), y = ry0[r] + rnd(rh_[r]);
        if (dmap[y][x] != FLOOR) continue;
        item[ii].x = x; item[ii].y = y; item[ii].type = IT_POTION; item[ii].active = true; ii++;
    }
    if (depth >= AMULET_DEPTH && ii < MAXI) {
        item[ii].x = rcx[nr - 1]; item[ii].y = ry0[nr - 1]; item[ii].type = IT_AMULET; item[ii].active = true; ii++;
    }
}

static void compute_fov();

static void new_game() {
    hp = maxhp = 24; atk = 4; gold = 0; potions = 1; plevel = 1; xp = 0; depth = 1;
    score = 0; state = PLAY;
    set_msg("you enter the dungeon...");
    gen_level();
    compute_fov();
}

void init() { hiscore = load(0); new_game(); }

// ---- field of view (ray cast to every tile in radius) ----
static void los(int x1, int y1) {
    int dx = abs(x1 - px), dy = abs(y1 - py);
    int sx = px < x1 ? 1 : -1, sy = py < y1 ? 1 : -1;
    int err = dx - dy, x = px, y = py;
    while (1) {
        if (!(x == px && y == py)) {
            if (x < 0 || x >= GW || y < 0 || y >= GH) return;
            visible[y][x] = true; explored[y][x] = true;
            if (dmap[y][x] == WALL) return;
        }
        if (x == x1 && y == y1) return;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
}
static void compute_fov() {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) visible[y][x] = false;
    visible[py][px] = explored[py][px] = true;
    for (int ty = py - R_FOV; ty <= py + R_FOV; ty++)
        for (int tx = px - R_FOV; tx <= px + R_FOV; tx++)
            if ((tx - px) * (tx - px) + (ty - py) * (ty - py) <= R_FOV * R_FOV) los(tx, ty);
}

// ====================================================================
// turns
// ====================================================================

static void check_levelup() {
    int need = plevel * 10;
    if (xp >= need) {
        xp -= need; plevel++; maxhp += 5; hp = maxhp; atk += 1;
        set_msg(str("you reach level %d!", plevel));
        hit(76, INSTR_SINE, 4, 120);
    }
}

static void player_attack(int mi) {
    int d = atk + rnd(3);
    mon[mi].hp -= d;
    note(60, INSTR_SQUARE, 2);
    if (mon[mi].hp <= 0) {
        mon[mi].alive = false;
        xp += MXP[mon[mi].type]; score += MXP[mon[mi].type] * 2;
        set_msg(str("you slay the %s", MNAME[mon[mi].type]));
        check_levelup();
    } else {
        set_msg(str("you hit the %s for %d", MNAME[mon[mi].type], d));
    }
}

static void monsters_act() {
    for (int i = 0; i < MAXM; i++) {
        Mon *m = &mon[i];
        if (!m->alive) continue;
        int dxp = px - m->x, dyp = py - m->y;
        int adist = abs(dxp) + abs(dyp);
        if (adist == 1) {                              // adjacent → attack
            int d = MDMG[m->type] + rnd(2);
            hp -= d;
            note(48, INSTR_SAW, 3);
            set_msg(str("the %s hits you for %d", MNAME[m->type], d));
            if (hp <= 0) {
                hp = 0; state = DEAD;
                if (score > hiscore) { hiscore = score; save(0, hiscore); }
                return;
            }
        } else if (visible[m->y][m->x] && adist <= R_FOV + 2) {  // hunt the player
            int sx = sgn(dxp), sy = sgn(dyp);
            int nx = m->x, ny = m->y;
            if (abs(dxp) >= abs(dyp)) nx += sx; else ny += sy;
            if (dmap[ny][nx] != WALL && monster_at(nx, ny) < 0 && !(nx == px && ny == py)) { m->x = nx; m->y = ny; }
            else {                                      // blocked → try the other axis
                nx = m->x; ny = m->y;
                if (abs(dxp) >= abs(dyp)) ny += sy; else nx += sx;
                if (dmap[ny][nx] != WALL && monster_at(nx, ny) < 0 && !(nx == px && ny == py)) { m->x = nx; m->y = ny; }
            }
        }
    }
}

static void pickup() {
    for (int i = 0; i < MAXI; i++) {
        if (!item[i].active || item[i].x != px || item[i].y != py) continue;
        item[i].active = false;
        if (item[i].type == IT_GOLD)   { gold += item[i].amount; score += item[i].amount; set_msg(str("you pick up %d gold", item[i].amount)); note(72, INSTR_SQUARE, 2); }
        else if (item[i].type == IT_POTION) { potions++; set_msg("you find a potion"); note(67, INSTR_SQUARE, 2); }
        else if (item[i].type == IT_AMULET) { state = WIN; score += 1000; if (score > hiscore) { hiscore = score; save(0, hiscore); } set_msg("THE AMULET OF YENDOR!"); }
    }
}

static void end_turn() { monsters_act(); if (state == PLAY) compute_fov(); }

static void do_move(int dx, int dy) {
    int nx = px + dx, ny = py + dy;
    if (nx < 0 || nx >= GW || ny < 0 || ny >= GH) return;
    int mi = monster_at(nx, ny);
    if (mi >= 0) { player_attack(mi); end_turn(); }
    else if (dmap[ny][nx] != WALL) { px = nx; py = ny; pickup(); end_turn(); }
}

void update() {
    if (state != PLAY) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) new_game();
        return;
    }

    int dx = 0, dy = 0;
    if (btn(0, BTN_LEFT))       dx = -1;
    else if (btn(0, BTN_RIGHT)) dx = 1;
    else if (btn(0, BTN_UP))    dy = -1;
    else if (btn(0, BTN_DOWN))  dy = 1;

    if (dx || dy) { if (mcd <= 0) { do_move(dx, dy); mcd = 7; } } else mcd = 0;
    if (mcd > 0) mcd--;

    if (btnp(0, BTN_A)) {                    // quaff potion
        if (potions > 0 && hp < maxhp) { potions--; hp = min(maxhp, hp + 12); set_msg("you drink a potion"); note(79, INSTR_SINE, 3); end_turn(); }
        else if (potions == 0) set_msg("no potions left");
    }
    if (btnp(0, BTN_B)) {                     // descend
        if (dmap[py][px] == STAIRS) { depth++; set_msg(str("you descend to floor %d", depth)); gen_level(); compute_fov(); }
        else set_msg("no stairs here");
    }
}

// ====================================================================
// drawing
// ====================================================================

static void glyph(char c, int tx, int ty, int col) { print(str("%c", c), tx * TILE, ty * TILE, col); }

void draw() {
    cls(CLR_BLACK);

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            if (!explored[y][x]) continue;
            bool v = visible[y][x];
            int sx = x * TILE, sy = y * TILE;
            if (dmap[y][x] == WALL) {
                rectfill(sx, sy, TILE, TILE, v ? CLR_LIGHT_GREY : CLR_DARK_GREY);
            } else if (dmap[y][x] == STAIRS) {
                glyph('>', x, y, v ? CLR_WHITE : CLR_DARK_GREY);
            } else {
                pset(sx + TILE / 2, sy + TILE / 2, v ? CLR_DARK_GREY : CLR_BROWNISH_BLACK);
            }
        }

    // items (only when in view)
    for (int i = 0; i < MAXI; i++) {
        if (!item[i].active || !visible[item[i].y][item[i].x]) continue;
        if (item[i].type == IT_GOLD)        glyph('$', item[i].x, item[i].y, CLR_YELLOW);
        else if (item[i].type == IT_POTION) glyph('!', item[i].x, item[i].y, CLR_PINK);
        else                                glyph('&', item[i].x, item[i].y, blink(8) ? CLR_WHITE : CLR_YELLOW);
    }
    // monsters
    for (int i = 0; i < MAXM; i++) {
        if (!mon[i].alive || !visible[mon[i].y][mon[i].x]) continue;
        glyph(MGLY[mon[i].type], mon[i].x, mon[i].y, MCOL[mon[i].type]);
    }
    // player
    glyph('@', px, py, CLR_YELLOW);

    // HUD
    rectfill(0, GH * TILE, SCREEN_W, SCREEN_H - GH * TILE, CLR_DARKER_BLUE);
    int by = GH * TILE + 3;
    int barw = 60 * hp / maxhp;
    rect(4, by, 62, 6, CLR_DARK_GREY);
    rectfill(5, by + 1, barw, 4, hp > maxhp / 3 ? CLR_GREEN : CLR_RED);
    print(str("HP %d/%d", hp, maxhp), 70, by, CLR_WHITE);
    print(str("LV%d", plevel), 138, by, CLR_LIGHT_GREY);
    print(str("$%d", gold), 168, by, CLR_YELLOW);
    print(str("POT %d", potions), 210, by, CLR_PINK);
    print_right(str("FLOOR %d", depth), SCREEN_W - 4, by, CLR_LIGHT_GREY);
    print(msg, 4, by + 11, CLR_LIGHT_PEACH);

    if (state == DEAD) {
        rectfill(SCREEN_W / 2 - 70, SCREEN_H / 2 - 26, 140, 52, CLR_BLACK);
        rect    (SCREEN_W / 2 - 70, SCREEN_H / 2 - 26, 140, 52, CLR_RED);
        print_centered("YOU DIED", SCREEN_W/2, SCREEN_H / 2 - 16, CLR_RED);
        print_centered(str("floor %d  score %d", depth, score), SCREEN_W/2, SCREEN_H / 2 - 2, CLR_YELLOW);
        print_centered("Z to descend again", SCREEN_W/2, SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    }
    if (state == WIN) {
        rectfill(SCREEN_W / 2 - 74, SCREEN_H / 2 - 26, 148, 52, CLR_BLACK);
        rect    (SCREEN_W / 2 - 74, SCREEN_H / 2 - 26, 148, 52, CLR_YELLOW);
        print_centered("YOU WIN!", SCREEN_W/2, SCREEN_H / 2 - 16, CLR_YELLOW);
        print_centered(str("escaped with the amulet! %d", score), SCREEN_W/2, SCREEN_H / 2 - 2, CLR_LIGHT_PEACH);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    }
}
