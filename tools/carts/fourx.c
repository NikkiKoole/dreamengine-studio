/* de:meta
{
  "title": "micro 4x",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "turn-based-loop",
    "noise-terrain",
    "grid-movement",
    "finite-state-ai"
  ],
  "lineage": "A micro Civilization/4X (explore-expand-exploit-exterminate) on a 15x11 fog-of-war tile map with settler/warrior units, city economy, and a greedy AI rival — a minimal complete 4X loop in ~400 lines.",
  "genre": "strategy",
  "description": "A tiny turn-based 4X strategy game (eXplore, eXpand, eXploit, eXterminate), you (blue) versus an AI rival (red) on a fogged tile map. Mouse-driven: click your unit then an adjacent tile to move, or onto an enemy to attack. A Settler founds a city on grass or forest; click a city to spend the gold it earns on Warriors or Settlers. END TURN lets the rival act and collects income. Reveal the map, expand your cities, and wipe out the enemy to win. Each map is freshly generated."
}
de:meta */
#include "studio.h"

// ── MICRO 4X — eXplore, eXpand, eXploit, eXterminate ──────────
// A tiny turn-based strategy game on a fogged tile map, you (blue)
// vs an AI rival (red). Mouse-driven.
//
//   • click your unit, then click an adjacent tile to MOVE
//     (move onto an enemy to ATTACK)
//   • a SETTLER can FOUND a city (button) on grass/forest
//   • click your CITY to BUY units with the gold it earns
//   • END TURN lets the rival act and collects everyone's income
//
// Wipe out every enemy city and unit to win. Lose all of yours
// (with no settler left to rebuild) and it's over.

#define COLS 15
#define ROWS 11
#define TS   16
#define MAPX 0
#define MAPY 12
#define PANELX 242
#define SIGHT  2

enum { T_WATER, T_GRASS, T_FOREST, T_MOUNT };
enum { U_NONE, U_SETTLER, U_WARRIOR };
#define MAXU 40
#define MAXC 16
#define COST_WARRIOR 10
#define COST_SETTLER 25

typedef struct { int type, owner, x, y, moved; } Unit;
typedef struct { int owner, x, y; } City;

static int   terr[ROWS][COLS];
static bool  seen[ROWS][COLS];
static Unit  units[MAXU];
static City  cities[MAXC];
static int   gold[2];
static int   turn, sel = -1, selc = -1, winner = -1;
static int   ai_flash;
static bool  ready = false;

// ── lookups ───────────────────────────────────────────────────
static bool passable(int x, int y) {
    if (x < 0 || y < 0 || x >= COLS || y >= ROWS) return false;
    return terr[y][x] == T_GRASS || terr[y][x] == T_FOREST;
}
static int unit_at(int x, int y) {
    for (int i = 0; i < MAXU; i++)
        if (units[i].type && units[i].x == x && units[i].y == y) return i;
    return -1;
}
static int city_at(int x, int y) {
    for (int i = 0; i < MAXC; i++)
        if (cities[i].owner >= 0 && cities[i].x == x && cities[i].y == y) return i;
    return -1;
}
static int count_owned(int owner, bool wantCities) {
    int n = 0;
    if (wantCities) { for (int i = 0; i < MAXC; i++) if (cities[i].owner == owner) n++; }
    else            { for (int i = 0; i < MAXU; i++) if (units[i].type && units[i].owner == owner) n++; }
    return n;
}
static int count_settlers(int owner) {
    int n = 0;
    for (int i = 0; i < MAXU; i++) if (units[i].type == U_SETTLER && units[i].owner == owner) n++;
    return n;
}

// ── reveal fog around the player's pieces ─────────────────────
static void reveal(void) {
    for (int i = 0; i < MAXU; i++) if (units[i].type && units[i].owner == 0)
        for (int dy = -SIGHT; dy <= SIGHT; dy++) for (int dx = -SIGHT; dx <= SIGHT; dx++) {
            int x = units[i].x + dx, y = units[i].y + dy;
            if (x >= 0 && y >= 0 && x < COLS && y < ROWS) seen[y][x] = true;
        }
    for (int i = 0; i < MAXC; i++) if (cities[i].owner == 0)
        for (int dy = -SIGHT; dy <= SIGHT; dy++) for (int dx = -SIGHT; dx <= SIGHT; dx++) {
            int x = cities[i].x + dx, y = cities[i].y + dy;
            if (x >= 0 && y >= 0 && x < COLS && y < ROWS) seen[y][x] = true;
        }
}
// is (x,y) within sight of any player piece? (for showing enemy units)
static bool player_can_see(int x, int y) {
    for (int i = 0; i < MAXU; i++) if (units[i].type && units[i].owner == 0)
        if (abs(units[i].x - x) <= SIGHT && abs(units[i].y - y) <= SIGHT) return true;
    for (int i = 0; i < MAXC; i++) if (cities[i].owner == 0)
        if (abs(cities[i].x - x) <= SIGHT && abs(cities[i].y - y) <= SIGHT) return true;
    return false;
}

// ── spawning ──────────────────────────────────────────────────
static int add_unit(int type, int owner, int x, int y) {
    for (int i = 0; i < MAXU; i++) if (!units[i].type) {
        units[i] = (Unit){ type, owner, x, y, 0 };
        return i;
    }
    return -1;
}
static void found_city(int ui) {
    Unit *u = &units[ui];
    for (int i = 0; i < MAXC; i++) if (cities[i].owner < 0) {
        cities[i] = (City){ u->owner, u->x, u->y };
        u->type = U_NONE;                      // settler is consumed
        break;
    }
    if (u->owner == 0) { sel = -1; reveal(); note(64, INSTR_TRI, 3); }
}
static void buy_unit(int ci, int type) {
    City *c = &cities[ci];
    int cost = (type == U_SETTLER) ? COST_SETTLER : COST_WARRIOR;
    if (gold[c->owner] < cost) return;
    for (int d = 0; d < 9; d++) {
        int x = c->x + (d % 3) - 1, y = c->y + (d / 3) - 1;
        if (passable(x, y) && unit_at(x, y) < 0 && city_at(x, y) < 0) {
            add_unit(type, c->owner, x, y);
            gold[c->owner] -= cost;
            if (c->owner == 0) { reveal(); note(76, INSTR_SINE, 2); }
            return;
        }
    }
}

// ── combat: attacker on (au) strikes (tx,ty) ──────────────────
static void attack(int au, int tx, int ty) {
    Unit *a = &units[au];
    int di = unit_at(tx, ty), dc = city_at(tx, ty);
    int astr = 2 + rnd(3);
    int dstr = 1 + rnd(3);                       // defender's edge
    if (di >= 0) dstr += (units[di].type == U_WARRIOR) ? 2 : 0;
    else if (dc >= 0) dstr += 2;
    note(48, INSTR_NOISE, 3);
    if (astr > dstr) {                            // attacker wins
        if (di >= 0) units[di].type = U_NONE;
        if (dc >= 0 && unit_at(tx, ty) < 0) cities[dc].owner = a->owner;   // capture
        if (unit_at(tx, ty) < 0) { a->x = tx; a->y = ty; }                 // advance
    } else {
        a->type = U_NONE;                         // attacker destroyed
    }
    a->moved = 1;
    if (a->owner == 0) reveal();
}

// ── try to move/attack the player's selected unit to (tx,ty) ──
static void try_move(int tx, int ty) {
    if (sel < 0) return;
    Unit *u = &units[sel];
    if (u->moved) return;
    if (abs(tx - u->x) > 1 || abs(ty - u->y) > 1 || (tx == u->x && ty == u->y)) return;
    int ei = unit_at(tx, ty), ec = city_at(tx, ty);
    bool enemy = (ei >= 0 && units[ei].owner != 0) || (ec >= 0 && cities[ec].owner != 0);
    if (enemy) {
        if (u->type == U_WARRIOR) attack(sel, tx, ty);
        return;
    }
    if (!passable(tx, ty) || ei >= 0 || ec >= 0) return;
    u->x = tx; u->y = ty; u->moved = 1;
    reveal();
}

// ── the AI's whole turn ───────────────────────────────────────
static void ai_step_toward(Unit *u, int tx, int ty) {
    int dx = sgn(tx - u->x), dy = sgn(ty - u->y);
    int nx = u->x + dx, ny = u->y + dy;
    int ei = unit_at(nx, ny), ec = city_at(nx, ny);
    bool foe = (ei >= 0 && units[ei].owner == 0) || (ec >= 0 && cities[ec].owner == 0);
    if (foe) { if (u->type == U_WARRIOR) { /* find index */ for (int k=0;k<MAXU;k++) if(&units[k]==u){attack(k,nx,ny);break;} } return; }
    if (passable(nx, ny) && unit_at(nx, ny) < 0 && city_at(nx, ny) < 0) { u->x = nx; u->y = ny; }
}
static void run_ai(void) {
    for (int i = 0; i < MAXU; i++) units[i].moved = 0;
    // move units
    for (int i = 0; i < MAXU; i++) {
        Unit *u = &units[i];
        if (!u->type || u->owner != 1 || u->moved) continue;
        if (u->type == U_SETTLER) {
            // found if far enough from any city, else wander
            int near_city = 0;
            for (int c = 0; c < MAXC; c++) if (cities[c].owner >= 0 && abs(cities[c].x - u->x) <= 2 && abs(cities[c].y - u->y) <= 2) near_city = 1;
            if (!near_city && passable(u->x, u->y)) { found_city(i); continue; }
            ai_step_toward(u, u->x + (rnd(3) - 1), u->y + (rnd(3) - 1));
        } else {
            // hunt the nearest player piece
            int bx = -1, by = -1, bd = 999;
            for (int k = 0; k < MAXU; k++) if (units[k].type && units[k].owner == 0) {
                int d = abs(units[k].x - u->x) + abs(units[k].y - u->y);
                if (d < bd) { bd = d; bx = units[k].x; by = units[k].y; }
            }
            for (int c = 0; c < MAXC; c++) if (cities[c].owner == 0) {
                int d = abs(cities[c].x - u->x) + abs(cities[c].y - u->y);
                if (d < bd) { bd = d; bx = cities[c].x; by = cities[c].y; }
            }
            if (bx >= 0) ai_step_toward(u, bx, by);
        }
        u->moved = 1;
    }
    // buy
    for (int c = 0; c < MAXC; c++) if (cities[c].owner == 1) {
        if (gold[1] >= COST_SETTLER && count_settlers(1) == 0 && count_owned(1, true) < 3) buy_unit(c, U_SETTLER);
        else if (gold[1] >= COST_WARRIOR) buy_unit(c, U_WARRIOR);
    }
}

// ── economy + turn end ────────────────────────────────────────
static int city_income(int ci) {
    int inc = 3;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int x = cities[ci].x + dx, y = cities[ci].y + dy;
        if (x >= 0 && y >= 0 && x < COLS && y < ROWS && (terr[y][x] == T_GRASS || terr[y][x] == T_FOREST)) inc++;
    }
    return inc;
}
static void check_winner(void) {
    bool ai = count_owned(1, true) > 0 || count_owned(1, false) > 0;
    bool me = count_owned(0, true) > 0 || count_settlers(0) > 0;
    if (!ai) winner = 0;
    else if (!me) winner = 1;
}
static void end_turn(void) {
    if (winner >= 0) return;
    run_ai();
    for (int c = 0; c < MAXC; c++) if (cities[c].owner >= 0) gold[cities[c].owner] += city_income(c);
    for (int i = 0; i < MAXU; i++) units[i].moved = 0;
    turn++; sel = -1; selc = -1; ai_flash = 40;
    reveal();
    check_winner();
}

// ── map generation ────────────────────────────────────────────
static void gen_map(void) {
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        float n = noise2(x * 0.42f + 3.1f, y * 0.42f + 1.7f);
        if      (n < 0.32f) terr[y][x] = T_WATER;
        else if (n > 0.78f) terr[y][x] = T_MOUNT;
        else if (noise2(x * 0.9f + 9.0f, y * 0.9f) > 0.62f) terr[y][x] = T_FOREST;
        else terr[y][x] = T_GRASS;
        seen[y][x] = false;
    }
    terr[ROWS - 2][1] = T_GRASS; terr[ROWS - 3][1] = T_GRASS;   // player start clear
    terr[1][COLS - 2] = T_GRASS; terr[2][COLS - 2] = T_GRASS;   // ai start clear
}
static void reset_game(void) {
    for (int i = 0; i < MAXU; i++) units[i].type = U_NONE;
    for (int i = 0; i < MAXC; i++) cities[i].owner = -1;
    gen_map();
    gold[0] = gold[1] = 10; turn = 1; sel = selc = -1; winner = -1;
    add_unit(U_SETTLER, 0, 1, ROWS - 2); add_unit(U_WARRIOR, 0, 1, ROWS - 3);
    add_unit(U_SETTLER, 1, COLS - 2, 1);  add_unit(U_WARRIOR, 1, COLS - 2, 2);
    reveal();
    ready = true;
}

// ── input ─────────────────────────────────────────────────────
static bool clicked(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) && mouse_x() >= x && mouse_x() < x + w && mouse_y() >= y && mouse_y() < y + h;
}
void update(void) {
    if (!ready) reset_game();
    if (winner >= 0) { if (btnp(0, BTN_A) || clicked(0, 0, SCREEN_W, SCREEN_H)) reset_game(); return; }
    if (ai_flash > 0) ai_flash--;

    // panel buttons
    if (clicked(PANELX + 4, 178, 74, 18)) { end_turn(); return; }
    if (sel >= 0 && units[sel].type == U_SETTLER && passable(units[sel].x, units[sel].y)
        && clicked(PANELX + 4, 120, 74, 16)) { found_city(sel); return; }
    if (selc >= 0 && cities[selc].owner == 0) {
        if (clicked(PANELX + 4, 110, 74, 14)) { buy_unit(selc, U_WARRIOR); return; }
        if (clicked(PANELX + 4, 128, 74, 14)) { buy_unit(selc, U_SETTLER); return; }
    }

    // map clicks
    if (mouse_pressed(MOUSE_LEFT) && mouse_x() < PANELX && mouse_y() >= MAPY) {
        int cx = (mouse_x() - MAPX) / TS, cy = (mouse_y() - MAPY) / TS;
        if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS) return;
        int ui = unit_at(cx, cy), ci = city_at(cx, cy);
        if (ui >= 0 && units[ui].owner == 0) { sel = ui; selc = -1; }
        else if (ci >= 0 && cities[ci].owner == 0) { selc = ci; sel = -1; }
        else try_move(cx, cy);
    }
}

// ── drawing ───────────────────────────────────────────────────
static void draw_piece(int type, int owner, int sx, int sy) {
    int col = owner == 0 ? CLR_BLUE : CLR_RED;
    int dk  = owner == 0 ? CLR_TRUE_BLUE : CLR_DARK_RED;
    if (type == 0) {   // city
        rectfill(sx + 3, sy + 7, 10, 7, col);
        trifill(sx + 2, sy + 7, sx + 14, sy + 7, sx + 8, sy + 2, dk);
        rectfill(sx + 7, sy + 9, 2, 3, CLR_LIGHT_PEACH);
    } else {
        circfill(sx + 8, sy + 8, 5, col);
        circ(sx + 8, sy + 8, 5, dk);
        print(type == U_SETTLER ? "S" : "W", sx + 6, sy + 5, CLR_WHITE);
    }
}
void draw(void) {
    if (!ready) reset_game();
    cls(CLR_BLACK);

    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        int sx = MAPX + x * TS, sy = MAPY + y * TS;
        if (!seen[y][x]) { rectfill(sx, sy, TS, TS, CLR_BLACK); rect(sx, sy, TS, TS, CLR_DARKER_BLUE); continue; }
        int base = terr[y][x] == T_WATER ? CLR_TRUE_BLUE : terr[y][x] == T_MOUNT ? CLR_DARK_GREY
                 : terr[y][x] == T_FOREST ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
        rectfill(sx, sy, TS, TS, base);
        if (terr[y][x] == T_FOREST) { trifill(sx + 5, sy + 11, sx + 11, sy + 11, sx + 8, sy + 4, CLR_DARK_BROWN); }
        if (terr[y][x] == T_MOUNT)  { trifill(sx + 3, sy + 13, sx + 13, sy + 13, sx + 8, sy + 3, CLR_LIGHT_GREY); }
        if (terr[y][x] == T_WATER)  { line(sx + 2, sy + 8, sx + 13, sy + 8, CLR_BLUE); }
        rect(sx, sy, TS, TS, CLR_DARKER_BLUE);
    }

    // cities
    for (int i = 0; i < MAXC; i++) if (cities[i].owner >= 0 && seen[cities[i].y][cities[i].x])
        draw_piece(0, cities[i].owner, MAPX + cities[i].x * TS, MAPY + cities[i].y * TS);
    // units (enemy only when in sight)
    for (int i = 0; i < MAXU; i++) if (units[i].type) {
        if (units[i].owner != 0 && !player_can_see(units[i].x, units[i].y)) continue;
        if (!seen[units[i].y][units[i].x]) continue;
        draw_piece(units[i].type, units[i].owner, MAPX + units[i].x * TS, MAPY + units[i].y * TS);
    }

    // selection + reachable tiles
    if (sel >= 0 && units[sel].type) {
        int sx = MAPX + units[sel].x * TS, sy = MAPY + units[sel].y * TS;
        rect(sx, sy, TS, TS, CLR_YELLOW);
        if (!units[sel].moved)
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int x = units[sel].x + dx, y = units[sel].y + dy;
                if ((dx || dy) && passable(x, y)) rect(MAPX + x * TS + 2, MAPY + y * TS + 2, TS - 4, TS - 4, CLR_LIGHT_YELLOW);
            }
    }
    if (selc >= 0 && cities[selc].owner == 0)
        rect(MAPX + cities[selc].x * TS, MAPY + cities[selc].y * TS, TS, TS, CLR_YELLOW);

    // top HUD
    rectfill(0, 0, SCREEN_W, MAPY, CLR_DARKER_PURPLE);
    print(str("GOLD %d", gold[0]), 3, 2, CLR_YELLOW);
    print(str("TURN %d", turn), 96, 2, CLR_WHITE);
    print(str("you %d-%d  ai %d-%d", count_owned(0,true), count_owned(0,false),
              count_owned(1,true), count_owned(1,false)), 156, 2, CLR_LIGHT_GREY);

    // side panel
    rectfill(PANELX, MAPY, SCREEN_W - PANELX, SCREEN_H - MAPY, CLR_DARKER_BLUE);
    line(PANELX, MAPY, PANELX, SCREEN_H, CLR_DARK_GREY);
    print("MICRO 4X", PANELX + 6, 16, CLR_LIGHT_PEACH);
    if (sel >= 0 && units[sel].type) {
        print(units[sel].type == U_SETTLER ? "SETTLER" : "WARRIOR", PANELX + 6, 34, CLR_WHITE);
        print(units[sel].moved ? "(moved)" : "move it", PANELX + 6, 46, CLR_LIGHT_GREY);
        if (units[sel].type == U_SETTLER && passable(units[sel].x, units[sel].y)) {
            rectfill(PANELX + 4, 120, 74, 16, CLR_DARK_GREEN); rect(PANELX + 4, 120, 74, 16, CLR_WHITE);
            print("FOUND", PANELX + 22, 124, CLR_WHITE);
        }
    } else if (selc >= 0 && cities[selc].owner == 0) {
        print("YOUR CITY", PANELX + 6, 34, CLR_WHITE);
        print(str("+%dg/turn", city_income(selc)), PANELX + 6, 46, CLR_LIGHT_YELLOW);
        rectfill(PANELX + 4, 110, 74, 14, gold[0] >= COST_WARRIOR ? CLR_DARK_GREEN : CLR_DARK_GREY);
        print(str("WAR  %dg", COST_WARRIOR), PANELX + 9, 113, CLR_WHITE);
        rectfill(PANELX + 4, 128, 74, 14, gold[0] >= COST_SETTLER ? CLR_DARK_GREEN : CLR_DARK_GREY);
        print(str("SET  %dg", COST_SETTLER), PANELX + 9, 131, CLR_WHITE);
    } else {
        print("pick unit", PANELX + 6, 34, CLR_DARK_GREY);
        print("or city", PANELX + 6, 46, CLR_DARK_GREY);
    }
    rectfill(PANELX + 4, 178, 74, 18, CLR_DARK_PURPLE); rect(PANELX + 4, 178, 74, 18, CLR_WHITE);
    print("END TURN", PANELX + 12, 183, CLR_LIGHT_PEACH);
    if (ai_flash > 0) print("ai moved", PANELX + 6, 160, CLR_RED);

    if (winner >= 0) {
        int w = 200, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 82, w, 32, winner == 0 ? CLR_DARK_GREEN : CLR_DARK_PURPLE);
        rect(bx, 82, w, 32, CLR_WHITE);
        print_centered(winner == 0 ? "VICTORY!" : "DEFEAT", SCREEN_W/2, 90, winner == 0 ? CLR_GREEN : CLR_RED);
        print_centered("press A for a new map", SCREEN_W/2, 102, CLR_LIGHT_GREY);
    }
}
