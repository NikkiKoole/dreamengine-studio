/* de:meta
{
  "title": "Civ (tiny)",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "turn-based-loop",
    "grid-movement",
    "noise-terrain",
    "save-load-persistence"
  ],
  "lineage": "Pocket distillation of Civilization (1991) stripped to explore-and-found 4X with no war or rivals; fog-of-war reveal and a noise-generated continent are the structural core.",
  "genre": "strategy",
  "homage": "Civilization (1991)",
  "description": "A pocket-sized 4X on a fogged continent: start with one settler and one warrior in a sea of black, walk out, found cities, and watch the unknown peel back tile by tile. Cities grow a population bar and pump out a fresh unit every few turns while a single research bar slowly fills and unlocks Irrigation — the one improvement, which makes every city grow faster. No rivals, no war, no diplomacy; the map and the clock are the opponent. Found 3 cities before turn 30 to win, with your fastest run saved as a best. Controls: click a unit then click an adjacent revealed tile to move (1 tile/turn), B to found a city with a settler, E (or END TURN) to advance the turn, click for a new continent after the game ends."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// ── CIV (tiny) — a pocket 4X ──────────────────────────────────────────────────
// Explore a fogged continent, found cities, watch them grow. No war, no rivals —
// just you, one settler, one warrior, and the unknown. Found 3 cities before
// turn 30 to win.
//
//   • click a UNIT, then click an adjacent revealed tile to MOVE (1 tile/turn)
//   • B (or FOUND) — a settler founds a city on grass/forest
//   • E (or END TURN) — cities grow + build, research fills, fog re-reveals
//   • a single RESEARCH bar unlocks IRRIGATION, which boosts every city's growth
//
// The map itself is the opponent. Win by founding 3 cities; if turn 30 passes
// without them, the expedition stalls — try a fresh continent.

#define COLS    15
#define ROWS    11
#define TS      16
#define MAPX    0
#define MAPY    12
#define PANELX  242
#define PANELW  (SCREEN_W - PANELX)
#define SIGHT   2

#define WIN_CITIES  3
#define TURN_LIMIT  30

enum { T_WATER, T_GRASS, T_FOREST, T_MOUNT };
enum { U_NONE, U_SETTLER, U_WARRIOR };

#define MAXU 24
#define MAXC 8

typedef struct { int type, x, y, moved; float bob; } Unit;
typedef struct {
    bool used;
    int  x, y, pop;
    float growth;   // 0..1 population meter
    float prod;     // 0..1 production meter
} City;

static int   terr[ROWS][COLS];
static bool  seen[ROWS][COLS];
static Unit  units[MAXU];
static City  cities[MAXC];

static int   turn, sel = -1, selc = -1;
static int   result;            // 0 = playing, 1 = win, 2 = stalled
static float research;          // 0..1
static bool  irrigation;        // the one unlocked improvement
static bool  ready = false;

static int   founded;           // running city count (for win check)
static int   best;              // fewest turns to win, saved
static int   tech_flash;        // frames of "IRRIGATION!" banner
static int   spawn_flash;       // frames of new-unit glow
static int   spawn_x, spawn_y;  // where the last unit popped
static int   msg_timer;         // transient toast
static char  msg[32];

// ── helpers ───────────────────────────────────────────────────────────────────
static bool in_grid(int x, int y) { return x >= 0 && y >= 0 && x < COLS && y < ROWS; }
static bool land(int x, int y) {
    return in_grid(x, y) && (terr[y][x] == T_GRASS || terr[y][x] == T_FOREST);
}
static int unit_at(int x, int y) {
    for (int i = 0; i < MAXU; i++)
        if (units[i].type && units[i].x == x && units[i].y == y) return i;
    return -1;
}
static int city_at(int x, int y) {
    for (int i = 0; i < MAXC; i++)
        if (cities[i].used && cities[i].x == x && cities[i].y == y) return i;
    return -1;
}
static int count_cities(void) {
    int n = 0; for (int i = 0; i < MAXC; i++) if (cities[i].used) n++; return n;
}
static int count_settlers(void) {
    int n = 0; for (int i = 0; i < MAXU; i++) if (units[i].type == U_SETTLER) n++; return n;
}
static void toast(const char *t) {
    int k = 0; while (t[k] && k < 31) { msg[k] = t[k]; k++; } msg[k] = 0;
    msg_timer = 90;
}

// how rich a tile's neighbourhood is — drives growth + research yield
static int neighbourhood(int cx, int cy) {
    int v = 1;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int x = cx + dx, y = cy + dy;
        if (!in_grid(x, y)) continue;
        if (terr[y][x] == T_GRASS)  v += 2;
        if (terr[y][x] == T_FOREST) v += 1;
        if (terr[y][x] == T_WATER)  v += 1;  // coast / fish
    }
    return v;
}

// ── fog ─────────────────────────────────────────────────────────────────────
static void reveal_at(int cx, int cy) {
    for (int dy = -SIGHT; dy <= SIGHT; dy++) for (int dx = -SIGHT; dx <= SIGHT; dx++) {
        int x = cx + dx, y = cy + dy;
        if (in_grid(x, y) && abs(dx) + abs(dy) <= SIGHT + 1) seen[y][x] = true;
    }
}
static void reveal_all_owned(void) {
    for (int i = 0; i < MAXU; i++) if (units[i].type) reveal_at(units[i].x, units[i].y);
    for (int i = 0; i < MAXC; i++) if (cities[i].used) reveal_at(cities[i].x, cities[i].y);
}

// ── spawning ──────────────────────────────────────────────────────────────────
static int add_unit(int type, int x, int y) {
    for (int i = 0; i < MAXU; i++) if (!units[i].type) {
        units[i] = (Unit){ type, x, y, 0, 0.0f };
        return i;
    }
    return -1;
}

static void check_win(void) {
    if (result) return;
    if (count_cities() >= WIN_CITIES) {
        result = 1;
        if (best == 0 || turn < best) { best = turn; save(0, best); }
        strum(60, CHORD_MAJ7, 4, 5, 70);
        schedule(220, 72, 4, 5);
    } else if (turn > TURN_LIMIT) {
        result = 2;
        note(40, INSTR_TRI, 3);
    }
}

static void found_city(int ui) {
    Unit *u = &units[ui];
    if (u->type != U_SETTLER || !land(u->x, u->y)) return;
    if (city_at(u->x, u->y) >= 0) return;
    for (int i = 0; i < MAXC; i++) if (!cities[i].used) {
        cities[i] = (City){ true, u->x, u->y, 1, 0.0f, 0.0f };
        u->type = U_NONE;            // settler consumed
        founded++;
        sel = -1; selc = i;
        chord(57, CHORD_MAJ, INSTR_TRI, 4);
        schedule(120, 64, INSTR_SINE, 3);
        shake(2.0f);
        reveal_all_owned();
        toast("city founded");
        check_win();
        return;
    }
}

// produce one unit from a city's accumulated production
static void city_build(int ci) {
    City *c = &cities[ci];
    // pick a free adjacent land tile
    for (int d = 0; d < 9; d++) {
        int x = c->x + (d % 3) - 1, y = c->y + (d / 3) - 1;
        if ((x == c->x && y == c->y)) continue;
        if (land(x, y) && unit_at(x, y) < 0 && city_at(x, y) < 0) {
            // mostly warriors; an occasional settler so the empire can keep expanding
            int type = (count_settlers() == 0 && count_cities() < WIN_CITIES && chance(45))
                       ? U_SETTLER : U_WARRIOR;
            add_unit(type, x, y);
            spawn_flash = 30; spawn_x = x; spawn_y = y;
            note(76, INSTR_SINE, 3);
            reveal_all_owned();
            return;
        }
    }
}

// ── the turn engine ─────────────────────────────────────────────────────────
static void end_turn(void) {
    if (result) return;

    float total_yield = 0;
    for (int i = 0; i < MAXC; i++) if (cities[i].used) {
        City *c = &cities[i];
        int nb = neighbourhood(c->x, c->y);
        float food = nb * (irrigation ? 0.055f : 0.035f);
        float work = nb * 0.040f;

        c->growth += food;
        if (c->growth >= 1.0f) { c->growth -= 1.0f; if (c->pop < 12) c->pop++; }

        c->prod += work;
        if (c->prod >= 1.0f) { c->prod -= 1.0f; city_build(i); }

        total_yield += nb;
    }

    // research fills from total city output; one tech, then it caps
    if (!irrigation) {
        research += total_yield * 0.012f;
        if (research >= 1.0f) {
            research = 1.0f; irrigation = true; tech_flash = 120;
            strum(60, CHORD_MAJ, INSTR_TRI, 4, 50);
            schedule(180, 67, INSTR_SINE, 4);
            toast("IRRIGATION unlocked");
        }
    }

    for (int i = 0; i < MAXU; i++) units[i].moved = 0;
    turn++;
    sel = -1; selc = -1;
    reveal_all_owned();
    note(50, INSTR_TRI, 2);
    check_win();
}

// ── move the selected unit to an adjacent revealed tile ───────────────────────
static void try_move(int tx, int ty) {
    if (sel < 0) return;
    Unit *u = &units[sel];
    if (u->moved) return;
    if (!in_grid(tx, ty) || !seen[ty][tx]) return;
    if (abs(tx - u->x) > 1 || abs(ty - u->y) > 1 || (tx == u->x && ty == u->y)) return;
    if (!land(tx, ty)) return;
    if (unit_at(tx, ty) >= 0 || city_at(tx, ty) >= 0) return;
    u->x = tx; u->y = ty; u->moved = 1;
    note(64, INSTR_SQUARE, 1);
    reveal_all_owned();
}

// ── map gen + reset ───────────────────────────────────────────────────────────
static void gen_map(void) {
    float ox = rnd_float() * 40.0f, oy = rnd_float() * 40.0f;
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        float n = noise2(x * 0.40f + ox, y * 0.40f + oy);
        if      (n < 0.34f) terr[y][x] = T_WATER;
        else if (n > 0.80f) terr[y][x] = T_MOUNT;
        else if (noise2(x * 0.9f + 9.0f + ox, y * 0.9f) > 0.62f) terr[y][x] = T_FOREST;
        else terr[y][x] = T_GRASS;
        seen[y][x] = false;
    }
    // guarantee a clear start cluster bottom-left
    for (int y = ROWS - 3; y < ROWS; y++) for (int x = 0; x < 3; x++)
        if (in_grid(x, y)) terr[y][x] = T_GRASS;
}

static void reset_game(void) {
    for (int i = 0; i < MAXU; i++) units[i].type = U_NONE;
    for (int i = 0; i < MAXC; i++) cities[i].used = false;
    gen_map();
    turn = 1; sel = selc = -1; result = 0;
    research = 0; irrigation = false; founded = 0;
    tech_flash = spawn_flash = msg_timer = 0;
    best = load(0);
    add_unit(U_SETTLER, 1, ROWS - 2);
    add_unit(U_WARRIOR, 1, ROWS - 1);
    reveal_all_owned();
    ready = true;
}

// ── input ─────────────────────────────────────────────────────────────────────
static bool clicked(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) &&
           mouse_x() >= x && mouse_x() < x + w &&
           mouse_y() >= y && mouse_y() < y + h;
}
static bool can_found_sel(void) {
    return sel >= 0 && units[sel].type == U_SETTLER && land(units[sel].x, units[sel].y)
        && city_at(units[sel].x, units[sel].y) < 0;
}

void init(void) {
    bpm(72);
    reset_game();
}

void update(void) {
    if (!ready) reset_game();

    if (msg_timer  > 0) msg_timer--;
    if (tech_flash > 0) tech_flash--;
    if (spawn_flash > 0) spawn_flash--;
    for (int i = 0; i < MAXU; i++) if (units[i].type) units[i].bob += dt() * 6.0f;

    // soft ambient pad, two notes a bar, never while a banner is up
    if (!result && every(2)) note(48 + (beat() % 3) * 4, INSTR_TRI, 1);

    if (result) {
        if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_SPACE)) reset_game();
        return;
    }

    // keyboard shortcuts
    if (keyp('E') || keyp(KEY_SPACE)) { end_turn(); return; }
    if (keyp('B') && can_found_sel()) { found_city(sel); return; }

    // panel buttons
    if (clicked(PANELX + 4, 178, PANELW - 8, 18)) { end_turn(); return; }
    if (can_found_sel() && clicked(PANELX + 4, 120, PANELW - 8, 16)) { found_city(sel); return; }

    // map clicks
    if (mouse_pressed(MOUSE_LEFT) && mouse_x() < PANELX && mouse_y() >= MAPY) {
        int cx = (mouse_x() - MAPX) / TS, cy = (mouse_y() - MAPY) / TS;
        if (!in_grid(cx, cy)) return;
        int ui = unit_at(cx, cy), ci = city_at(cx, cy);
        if (ui >= 0 && seen[cy][cx])      { sel = ui; selc = -1; }
        else if (ci >= 0 && seen[cy][cx]) { selc = ci; sel = -1; }
        else                              { try_move(cx, cy); }
    }
}

// ── drawing ─────────────────────────────────────────────────────────────────
static void draw_tile(int x, int y) {
    int sx = MAPX + x * TS, sy = MAPY + y * TS;
    if (!seen[y][x]) {
        rectfill(sx, sy, TS, TS, CLR_BLACK);
        // faint fog speckle so the void isn't dead flat
        if (((x * 7 + y * 3) & 3) == 0) pset(sx + 8, sy + 8, CLR_DARKER_BLUE);
        return;
    }
    int t = terr[y][x];
    int base = t == T_WATER ? CLR_TRUE_BLUE : t == T_MOUNT ? CLR_DARK_GREY
             : t == T_FOREST ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
    // irrigated grass reads a touch lusher
    if (t == T_GRASS && irrigation) base = CLR_GREEN;
    rectfill(sx, sy, TS, TS, base);
    if (t == T_FOREST) {
        trifill(sx + 5, sy + 12, sx + 11, sy + 12, sx + 8, sy + 3, CLR_DARK_BROWN);
        trifill(sx + 6, sy + 9, sx + 10, sy + 9, sx + 8, sy + 4, CLR_MEDIUM_GREEN);
    } else if (t == T_MOUNT) {
        trifill(sx + 2, sy + 13, sx + 14, sy + 13, sx + 8, sy + 2, CLR_LIGHT_GREY);
        trifill(sx + 6, sy + 6, sx + 10, sy + 6, sx + 8, sy + 2, CLR_WHITE);
    } else if (t == T_WATER) {
        line(sx + 2, sy + 7, sx + 7, sy + 7, CLR_BLUE);
        line(sx + 9, sy + 11, sx + 13, sy + 11, CLR_BLUE);
    } else if (t == T_GRASS && irrigation) {
        line(sx + 3, sy + 12, sx + 12, sy + 12, CLR_MEDIUM_GREEN);
    }
    rect(sx, sy, TS, TS, CLR_DARKER_BLUE);
}

static void draw_city(int i) {
    City *c = &cities[i];
    int sx = MAPX + c->x * TS, sy = MAPY + c->y * TS;
    if (spawn_flash > 0 && spawn_x == c->x && spawn_y == c->y) pal(CLR_LIGHT_PEACH, CLR_WHITE);
    rectfill(sx + 3, sy + 8, 10, 6, CLR_LIGHT_PEACH);
    trifill(sx + 2, sy + 8, sx + 14, sy + 8, sx + 8, sy + 2, CLR_DARK_ORANGE);
    rectfill(sx + 5, sy + 10, 2, 3, CLR_BROWN);
    rectfill(sx + 9, sy + 10, 2, 2, CLR_YELLOW);   // a lit window
    pal_reset();
    // pop pips above the city
    for (int p = 0; p < c->pop && p < 6; p++)
        pset(sx + 3 + p * 2, sy, CLR_LIGHT_YELLOW);
}

static void draw_unit(int i) {
    Unit *u = &units[i];
    if (!seen[u->y][u->x]) return;
    int sx = MAPX + u->x * TS, sy = MAPY + u->y * TS;
    int lift = (!u->moved && !result) ? (int)(sin_deg(u->bob * 20.0f) * 1.5f - 1.5f) : 0;
    if (u->type == U_SETTLER) {
        circfill(sx + 8, sy + 9 + lift, 5, CLR_PEACH);
        circ(sx + 8, sy + 9 + lift, 5, CLR_BROWN);
        rectfill(sx + 6, sy + 5 + lift, 4, 2, CLR_WHITE);   // a little cart/cap
        print("S", sx + 6, sy + 6 + lift, CLR_DARK_BROWN);
    } else {
        circfill(sx + 8, sy + 9 + lift, 5, CLR_BLUE);
        circ(sx + 8, sy + 9 + lift, 5, CLR_TRUE_BLUE);
        print("W", sx + 6, sy + 6 + lift, CLR_WHITE);
    }
}

void draw(void) {
    if (!ready) reset_game();
    cls(CLR_BLACK);

    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) draw_tile(x, y);
    for (int i = 0; i < MAXC; i++) if (cities[i].used && seen[cities[i].y][cities[i].x]) draw_city(i);
    for (int i = 0; i < MAXU; i++) if (units[i].type) draw_unit(i);

    // selection + reachable hint
    if (sel >= 0 && units[sel].type) {
        Unit *u = &units[sel];
        int sx = MAPX + u->x * TS, sy = MAPY + u->y * TS;
        rect(sx, sy, TS, TS, CLR_YELLOW);
        if (!u->moved)
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int x = u->x + dx, y = u->y + dy;
                if ((dx || dy) && land(x, y) && seen[y][x]
                    && unit_at(x, y) < 0 && city_at(x, y) < 0)
                    rect(MAPX + x * TS + 2, MAPY + y * TS + 2, TS - 4, TS - 4,
                         blink(12) ? CLR_LIGHT_YELLOW : CLR_YELLOW);
            }
    }
    if (selc >= 0 && cities[selc].used)
        rect(MAPX + cities[selc].x * TS, MAPY + cities[selc].y * TS, TS, TS, CLR_YELLOW);

    // top HUD
    rectfill(0, 0, SCREEN_W, MAPY, CLR_DARKER_PURPLE);
    print(str("TURN %d/%d", turn, TURN_LIMIT), 3, 2, turn > TURN_LIMIT - 5 ? CLR_ORANGE : CLR_WHITE);
    print(str("CITIES %d/%d", count_cities(), WIN_CITIES), 96, 2, CLR_LIGHT_YELLOW);
    print(irrigation ? "TECH: IRRIGATION" : "researching...", 178, 2, irrigation ? CLR_GREEN : CLR_LIGHT_GREY);

    // side panel
    rectfill(PANELX, MAPY, PANELW, SCREEN_H - MAPY, CLR_DARKER_BLUE);
    line(PANELX, MAPY, PANELX, SCREEN_H, CLR_DARK_GREY);
    print("CIV tiny", PANELX + 6, 16, CLR_LIGHT_PEACH);

    // research bar (always shown)
    print("RESEARCH", PANELX + 6, 30, CLR_LIGHT_GREY);
    bar(PANELX + 6, 40, PANELW - 12, 6, irrigation ? 1.0f : research,
        irrigation ? CLR_GREEN : CLR_BLUE, CLR_DARKER_PURPLE);

    if (sel >= 0 && units[sel].type) {
        print(units[sel].type == U_SETTLER ? "SETTLER" : "WARRIOR", PANELX + 6, 56, CLR_WHITE);
        print(units[sel].moved ? "(moved)" : "click a tile", PANELX + 6, 68, CLR_LIGHT_GREY);
        if (can_found_sel()) {
            rectfill(PANELX + 4, 120, PANELW - 8, 16, CLR_DARK_GREEN);
            rect(PANELX + 4, 120, PANELW - 8, 16, CLR_WHITE);
            print("FOUND  (B)", PANELX + 12, 124, CLR_WHITE);
        } else if (units[sel].type == U_SETTLER) {
            print("need grass", PANELX + 6, 122, CLR_DARK_GREY);
        }
    } else if (selc >= 0 && cities[selc].used) {
        City *c = &cities[selc];
        print("CITY", PANELX + 6, 56, CLR_WHITE);
        print(str("pop %d", c->pop), PANELX + 6, 68, CLR_LIGHT_YELLOW);
        print("GROW", PANELX + 6, 84, CLR_LIGHT_GREY);
        bar(PANELX + 6, 94, PANELW - 12, 5, c->growth, CLR_GREEN, CLR_DARKER_PURPLE);
        print("BUILD", PANELX + 6, 104, CLR_LIGHT_GREY);
        bar(PANELX + 6, 114, PANELW - 12, 5, c->prod, CLR_ORANGE, CLR_DARKER_PURPLE);
    } else {
        print("pick a unit", PANELX + 6, 56, CLR_DARK_GREY);
        print("or a city", PANELX + 6, 68, CLR_DARK_GREY);
        print("settle 3", PANELX + 6, 92, CLR_LIGHT_GREY);
        print("cities to", PANELX + 6, 102, CLR_LIGHT_GREY);
        print("win.", PANELX + 6, 112, CLR_LIGHT_GREY);
    }

    if (best > 0) print(str("best: %d t", best), PANELX + 6, 160, CLR_INDIGO);

    rectfill(PANELX + 4, 178, PANELW - 8, 18, CLR_DARK_PURPLE);
    rect(PANELX + 4, 178, PANELW - 8, 18, CLR_WHITE);
    print("END TURN E", PANELX + 8, 183, CLR_LIGHT_PEACH);

    // toast
    if (msg_timer > 0) {
        int w = text_width(msg) + 10, bx = (PANELX - w) / 2;
        rectfill(bx, 150, w, 14, CLR_BROWNISH_BLACK);
        rect(bx, 150, w, 14, CLR_YELLOW);
        print(msg, bx + 5, 153, CLR_LIGHT_PEACH);
    }

    // tech unlock banner
    if (tech_flash > 0) {
        int w = 150, bx = (PANELX - w) / 2;
        rectfill(bx, 90, w, 24, CLR_DARK_GREEN);
        rect(bx, 90, w, 24, blink(6) ? CLR_WHITE : CLR_GREEN);
        print_centered("IRRIGATION!", SCREEN_W/2, 95, CLR_WHITE);   // centered over whole screen ok
        print("cities grow faster", bx + 16, 104, CLR_LIGHT_YELLOW);
    }

    // end screen
    if (result) {
        fade(0.45f);
        int w = 220, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 78, w, 46, result == 1 ? CLR_DARK_GREEN : CLR_DARKER_PURPLE);
        rect(bx, 78, w, 46, CLR_WHITE);
        print_centered(result == 1 ? "EMPIRE FOUNDED!" : "EXPEDITION STALLED", SCREEN_W/2, 86, result == 1 ? CLR_GREEN : CLR_ORANGE);
        print_centered(result == 1 ? str("3 cities in %d turns", turn)
                                   : "ran out of turns", SCREEN_W/2, 100, CLR_LIGHT_GREY);
        print_centered("click for a new continent", SCREEN_W/2, 112, CLR_YELLOW);
    }
}
