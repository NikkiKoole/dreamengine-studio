/* de:meta
{
  "title": "tower defense",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "pathfinding"
  ],
  "lineage": "Classic tower defense loop; creeps follow a fixed zigzag waypoint path (wp index tracks progress), towers target the furthest-along creep in range — a textbook waypoint-walking enemy pattern.",
  "genre": "strategy",
  "description": "Creeps march along the path from left to right — build towers on the grass to stop them before they leak out the far side. Three tower types: GUN (cheap, fast), FROST (slows what it hits), and CANNON (slow but heavy splash). Kills earn gold to build more or upgrade existing towers (3 levels each). Twelve escalating waves with normal, fast, and tank creeps; survive them all to win. Move the cursor with arrows/WASD, A builds or upgrades, B switches tower type."
}
de:meta */
#include "studio.h"

// TOWER DEFENSE
// Creeps march along the path from left to right. Build towers on the grass to
// shoot them before they leak out the far side. Kills earn gold; spend it on
// more towers or upgrade the ones you have. Survive all the waves.
//   GUN   — cheap, fast single shots
//   FROST — slows everything it hits
//   CANNON— slow but heavy, splash damage
// Arrows / WASD: move cursor   A: build / upgrade   B: change tower type

#define GW    20
#define GH    11
#define TILE  16
#define N     (GW * GH)
#define MAXC  64
#define MAXP  96
#define MAXWAVE 12

enum { T_NONE, T_GUN, T_FROST, T_CANNON };
enum { C_NORMAL, C_FAST, C_TANK };
enum { BUILD, FIGHT, WIN, LOSE };

typedef struct { float x, y; int wp, hp, maxhp, type; float slow_until; bool active; } Creep;
typedef struct { float x, y; int target, dmg, kind; bool active; } Proj;

static bool is_path[N];
static int  t_type[N], t_level[N], t_cd[N];

static Creep creep[MAXC];
static Proj  proj[MAXP];

// path waypoints (tile coords) — a left-to-right zigzag
static const int WPX[] = { 0, 6, 6, 13, 13, 19 };
static const int WPY[] = { 2, 2, 8, 8,  3,  3  };
#define NWP 6

static int  cursor, sel;
static int  money, lives, wave, state;
static int  spawn_left, spawn_cd;
static float build_timer;
static int  cur_cd, hiwave;

// ====================================================================

static int cx_(int i) { return i % GW; }
static int cy_(int i) { return i / GW; }
static int idx(int x, int y) { return y * GW + x; }
static int wpx(int i) { return WPX[i] * TILE + TILE / 2; }
static int wpy(int i) { return WPY[i] * TILE + TILE / 2; }

static int cost_of(int type)  { return type == T_GUN ? 20 : type == T_FROST ? 30 : 45; }
static int range_of(int type, int lv) { return (type == T_GUN ? 42 : type == T_FROST ? 40 : 52) + lv * 4; }
static int dmg_of(int type, int lv)   { return (type == T_GUN ? 4 : type == T_FROST ? 1 : 9) * lv; }
static int delay_of(int type, int lv) { int d = type == T_GUN ? 16 : type == T_FROST ? 26 : 48; return max(6, d - lv * 2); }
static const char *TNAME[4] = { "", "GUN", "FROST", "CANNON" };
static const int   TCOL[4]  = { 0, CLR_BLUE, CLR_LIGHT_GREY, CLR_ORANGE };

static void mark_path() {
    for (int i = 0; i < N; i++) is_path[i] = false;
    for (int s = 0; s < NWP - 1; s++) {
        int x1 = WPX[s], y1 = WPY[s], x2 = WPX[s + 1], y2 = WPY[s + 1];
        int dx = sgn(x2 - x1), dy = sgn(y2 - y1);
        int x = x1, y = y1;
        while (1) {
            if (x >= 0 && x < GW && y >= 0 && y < GH) is_path[idx(x, y)] = true;
            if (x == x2 && y == y2) break;
            x += dx; y += dy;
        }
    }
}

static void start_game() {
    mark_path();
    for (int i = 0; i < N; i++) { t_type[i] = T_NONE; t_level[i] = 0; t_cd[i] = 0; }
    for (int i = 0; i < MAXC; i++) creep[i].active = false;
    for (int i = 0; i < MAXP; i++) proj[i].active = false;
    money = 120; lives = 20; wave = 0; state = BUILD; build_timer = 4.0f;
    spawn_left = 0; cursor = idx(3, 5); sel = T_GUN;
}

void init() { hiwave = load(0); start_game(); }

// ---- waves ----
static void begin_wave() {
    wave++;
    spawn_left = 6 + wave * 2;
    spawn_cd = 0;
    state = FIGHT;
}

static void spawn_creep() {
    for (int i = 0; i < MAXC; i++) {
        if (creep[i].active) continue;
        Creep *c = &creep[i];
        c->x = wpx(0); c->y = wpy(0); c->wp = 1; c->slow_until = 0; c->active = true;
        int roll = rnd(100);
        if (wave >= 3 && roll < 25)      { c->type = C_FAST; c->maxhp = 6 + wave * 3; }
        else if (wave >= 4 && roll < 45) { c->type = C_TANK; c->maxhp = 20 + wave * 8; }
        else                             { c->type = C_NORMAL; c->maxhp = 8 + wave * 4; }
        c->hp = c->maxhp;
        return;
    }
}

static float creep_speed(Creep *c) {
    float base = c->type == C_FAST ? 1.1f : c->type == C_TANK ? 0.45f : 0.7f;
    if (now() < c->slow_until) base *= 0.45f;
    return base;
}

// ====================================================================

static void fire_proj(float x, float y, int target, int dmg, int kind) {
    for (int i = 0; i < MAXP; i++)
        if (!proj[i].active) { proj[i] = (Proj){ x, y, target, dmg, kind, true }; return; }
}

static void towers_fire() {
    for (int i = 0; i < N; i++) {
        if (t_type[i] == T_NONE) continue;
        if (t_cd[i] > 0) { t_cd[i]--; continue; }
        int tx = cx_(i) * TILE + TILE / 2, ty = cy_(i) * TILE + TILE / 2;
        int rng = range_of(t_type[i], t_level[i]);
        int best = -1, bestwp = -1;
        for (int c = 0; c < MAXC; c++) {
            if (!creep[c].active) continue;
            if (distance(tx, ty, (int)creep[c].x, (int)creep[c].y) > rng) continue;
            if (creep[c].wp > bestwp) { bestwp = creep[c].wp; best = c; }   // furthest along
        }
        if (best >= 0) {
            fire_proj(tx, ty, best, dmg_of(t_type[i], t_level[i]), t_type[i]);
            t_cd[i] = delay_of(t_type[i], t_level[i]);
            note(t_type[i] == T_CANNON ? 50 : 70, INSTR_SQUARE, 1);
        }
    }
}

static void hurt_creep(int c, int dmg) {
    creep[c].hp -= dmg;
    if (creep[c].hp <= 0) {
        creep[c].active = false;
        money += 3 + wave;
    }
}

static void update_proj() {
    for (int i = 0; i < MAXP; i++) {
        Proj *p = &proj[i];
        if (!p->active) continue;
        Creep *c = &creep[p->target];
        if (!c->active) { p->active = false; continue; }
        float a = angle_to((int)p->x, (int)p->y, (int)c->x, (int)c->y);
        p->x += dx(5.0f, a); p->y += dy(5.0f, a);
        if (distance((int)p->x, (int)p->y, (int)c->x, (int)c->y) < 6) {
            if (p->kind == T_CANNON) {                       // splash
                for (int j = 0; j < MAXC; j++)
                    if (creep[j].active && distance((int)c->x, (int)c->y, (int)creep[j].x, (int)creep[j].y) < 22)
                        hurt_creep(j, p->dmg);
            } else if (p->kind == T_FROST) {
                c->slow_until = now() + 1.4f;
                hurt_creep(p->target, p->dmg);
            } else {
                hurt_creep(p->target, p->dmg);
            }
            p->active = false;
        }
    }
}

void update() {
    if (state == WIN || state == LOSE) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) start_game();
        return;
    }

    // cursor
    int dx_ = 0, dy_ = 0;
    if (btn(0, BTN_LEFT))       dx_ = -1;
    else if (btn(0, BTN_RIGHT)) dx_ = 1;
    else if (btn(0, BTN_UP))    dy_ = -1;
    else if (btn(0, BTN_DOWN))  dy_ = 1;
    if (dx_ || dy_) {
        if (cur_cd <= 0) { cursor = idx(mid(0, cx_(cursor) + dx_, GW - 1), mid(0, cy_(cursor) + dy_, GH - 1)); cur_cd = 6; }
    } else cur_cd = 0;
    if (cur_cd > 0) cur_cd--;

    if (btnp(0, BTN_B)) sel = (sel % 3) + 1;     // cycle GUN/FROST/CANNON

    if (btnp(0, BTN_A)) {
        if (t_type[cursor] != T_NONE) {           // upgrade
            int lv = t_level[cursor];
            if (lv < 3) {
                int up = cost_of(t_type[cursor]) * lv;
                if (money >= up) { money -= up; t_level[cursor]++; note(74, INSTR_SINE, 2); }
            }
        } else if (!is_path[cursor]) {            // build
            int c = cost_of(sel);
            if (money >= c) { money -= c; t_type[cursor] = sel; t_level[cursor] = 1; t_cd[cursor] = 0; note(67, INSTR_SQUARE, 2); }
        }
    }

    // wave flow
    if (state == BUILD) {
        build_timer -= 1.0f / 60.0f;
        if (build_timer <= 0) begin_wave();
    } else if (state == FIGHT) {
        if (spawn_left > 0) {
            if (--spawn_cd <= 0) { spawn_creep(); spawn_left--; spawn_cd = max(18, 40 - wave); }
        } else {
            bool any = false;
            for (int i = 0; i < MAXC; i++) if (creep[i].active) { any = true; break; }
            if (!any) {
                if (wave >= MAXWAVE) { state = WIN; if (wave > hiwave) { hiwave = wave; save(0, hiwave); } }
                else { state = BUILD; build_timer = 5.0f; }
            }
        }
    }

    // move creeps
    for (int i = 0; i < MAXC; i++) {
        Creep *c = &creep[i];
        if (!c->active) continue;
        float a = angle_to((int)c->x, (int)c->y, wpx(c->wp), wpy(c->wp));
        float sp = creep_speed(c);
        c->x += dx(sp, a); c->y += dy(sp, a);
        if (distance((int)c->x, (int)c->y, wpx(c->wp), wpy(c->wp)) < sp + 0.5f) {
            c->wp++;
            if (c->wp >= NWP) {                   // leaked
                c->active = false;
                lives--; note(40, INSTR_NOISE, 4);
                if (lives <= 0) { state = LOSE; if (wave > hiwave) { hiwave = wave; save(0, hiwave); } }
            }
        }
    }

    towers_fire();
    update_proj();
}

// ====================================================================
// drawing
// ====================================================================

static void draw_creep(Creep *c) {
    int col = c->type == C_FAST ? CLR_YELLOW : c->type == C_TANK ? CLR_DARK_PURPLE : CLR_RED;
    circfill((int)c->x, (int)c->y, c->type == C_TANK ? 6 : 4, col);
    if (now() < c->slow_until) circ((int)c->x, (int)c->y, 7, CLR_BLUE);
    // hp bar
    int w = 12, fw = w * c->hp / c->maxhp;
    rectfill((int)c->x - w / 2, (int)c->y - 9, w, 2, CLR_DARK_GREY);
    rectfill((int)c->x - w / 2, (int)c->y - 9, fw, 2, CLR_GREEN);
}

void draw() {
    cls(CLR_DARK_GREEN);

    // terrain
    for (int i = 0; i < N; i++) {
        int sx = cx_(i) * TILE, sy = cy_(i) * TILE;
        if (is_path[i]) rectfill(sx, sy, TILE, TILE, CLR_BROWN);
        else { rectfill(sx, sy, TILE, TILE, CLR_DARK_GREEN); rect(sx, sy, TILE, TILE, CLR_DARKER_PURPLE); }
    }

    // towers
    for (int i = 0; i < N; i++) {
        if (t_type[i] == T_NONE) continue;
        int sx = cx_(i) * TILE + TILE / 2, sy = cy_(i) * TILE + TILE / 2;
        rectfill(sx - 5, sy - 5, 10, 10, CLR_DARK_GREY);
        circfill(sx, sy, 4, TCOL[t_type[i]]);
        for (int l = 0; l < t_level[i]; l++) pset(sx - 4 + l * 3, sy + 6, CLR_YELLOW);   // level pips
    }

    for (int i = 0; i < MAXC; i++) if (creep[i].active) draw_creep(&creep[i]);

    for (int i = 0; i < MAXP; i++)
        if (proj[i].active) circfill((int)proj[i].x, (int)proj[i].y, 2, proj[i].kind == T_CANNON ? CLR_ORANGE : proj[i].kind == T_FROST ? CLR_BLUE : CLR_WHITE);

    // cursor + range preview
    int csx = cx_(cursor) * TILE, csy = cy_(cursor) * TILE;
    int ccx = csx + TILE / 2, ccy = csy + TILE / 2;
    bool ok = !is_path[cursor];
    rect(csx, csy, TILE, TILE, ok ? CLR_WHITE : CLR_RED);
    if (t_type[cursor] != T_NONE) circ(ccx, ccy, range_of(t_type[cursor], t_level[cursor]), CLR_LIGHT_GREY);
    else if (ok) circ(ccx, ccy, range_of(sel, 1), CLR_DARK_GREY);

    // HUD
    rectfill(0, GH * TILE, SCREEN_W, SCREEN_H - GH * TILE, CLR_BLACK);
    int hy = GH * TILE + 2;
    print(str("$%d", money), 4, hy, CLR_YELLOW);
    print(str("LIVES %d", lives), 56, hy, CLR_RED);
    print(str("WAVE %d/%d", wave, MAXWAVE), 130, hy, CLR_WHITE);
    print(str("BUILD: %s $%d", TNAME[sel], cost_of(sel)), 4, hy + 10, TCOL[sel]);
    if (state == BUILD) print_right(str("next wave %.0f", build_timer + 0.9f), SCREEN_W - 4, hy, CLR_LIGHT_GREY);
    print_right("A:build/upgrade B:type", SCREEN_W - 4, hy + 10, CLR_DARK_GREY);

    if (state == WIN || state == LOSE) {
        rectfill(SCREEN_W / 2 - 70, SCREEN_H / 2 - 24, 140, 50, CLR_BLACK);
        rect    (SCREEN_W / 2 - 70, SCREEN_H / 2 - 24, 140, 50, state == WIN ? CLR_GREEN : CLR_RED);
        print_centered(state == WIN ? "VICTORY!" : "OVERRUN", SCREEN_W/2, SCREEN_H / 2 - 14, state == WIN ? CLR_GREEN : CLR_RED);
        print_centered(str("reached wave %d", wave), SCREEN_W/2, SCREEN_H / 2 - 1, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H / 2 + 13, CLR_LIGHT_GREY);
    }
}
