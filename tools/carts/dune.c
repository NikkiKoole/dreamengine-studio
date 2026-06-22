#include "studio.h"
#include "cursor.h"

// ── ARRAKIS — Spice & Steel ───────────────────────────────────────────────
// A tiny Dune/C&C base-builder. Harvesters crawl to the orange spice fields,
// fill up, and haul it back to a REFINERY for credits. Spend credits from the
// sidebar to place buildings and queue units, while the red CPU faction mines,
// builds and throws tank waves at you. Wipe out the enemy base to win; lose all
// your buildings and Arrakis is theirs.
//
//   LEFT-CLICK a unit/building to select   •  DRAG a box to select a group
//   with units picked: LEFT-CLICK ground = move,  LEFT-CLICK red = attack
//   click a BUILD button (or keys 1-4) then click the map to place a structure
//   select your FACTORY to queue HARVESTER / TRIKE / TANK   •  RIGHT-CLICK cancels
//   harvesters auto-mine the nearest spice  •  turrets & idle units auto-fire

// ── geometry ──────────────────────────────────────────────────────────────
#define GW    17
#define GH    13
#define TILE  14
#define N     (GW * GH)
#define MAPX  1
#define MAPY  12
#define PANELX 242
#define PW    (SCREEN_W - PANELX)
#define MAXPATH 48

enum { T_SAND, T_ROCK, T_SPICE };
enum { TEAM_P, TEAM_E };
enum { U_HARVESTER, U_TRIKE, U_TANK, U_COUNT };
enum { B_REFINERY, B_POWER, B_FACTORY, B_TURRET, B_COUNT };
// unit states
enum { ST_IDLE, ST_MOVE, ST_ATTACK, ST_SEEK, ST_HARVEST, ST_RETURN, ST_DUMP };
// attack target kinds
enum { TK_NONE, TK_UNIT, TK_BLD };

#define MAXU   48
#define MAXB   32
#define MAXPROJ 64
#define MAXBOOM 32
#define CARRY_CAP 30.0f
#define SPICE_VALUE 2          // credits per spice unit dumped

// ── stat tables ─────────────────────────────────────────────────────────────
static const int   unit_cost[U_COUNT]   = { 50, 30, 80 };
static const int   unit_hp[U_COUNT]      = { 70, 24, 90 };
static const float unit_speed[U_COUNT]   = { 26, 54, 28 };     // px/sec
static const int   unit_dmg[U_COUNT]     = { 0, 4, 13 };
static const int   unit_range[U_COUNT]   = { 0, 40, 50 };
static const float unit_firecd[U_COUNT]  = { 0, 0.35f, 0.95f };
static const float unit_build_t[U_COUNT] = { 4.0f, 2.5f, 5.0f };
static const char *UNAME[U_COUNT]        = { "HARVESTER", "TRIKE", "TANK" };

static const int bld_cost[B_COUNT]   = { 150, 40, 100, 60 };
static const int bld_hp[B_COUNT]     = { 320, 120, 200, 150 };
static const int bld_powgen[B_COUNT] = { 0, 100, 0, 0 };
static const int bld_powuse[B_COUNT] = { 30, 0, 40, 20 };
static const char *BNAME[B_COUNT]    = { "REFINERY", "POWER", "FACTORY", "TURRET" };
#define TUR_DMG    9
#define TUR_RANGE  64
#define TUR_FIRECD 0.8f

// ── data ─────────────────────────────────────────────────────────────────────
typedef struct {
    bool active; int type, team; float x, y; int hp, maxhp; int state;
    int path[MAXPATH]; int plen, pi; float repath_cd; float face;
    float carry; int harv_cell;            // harvester
    int tk, ti; int goal_cell; float fire_cd; float flash;   // combat
} Unit;

typedef struct {
    bool active; int type, team; int cx, cy; int hp, maxhp;
    float prod_t; int queue[6], qn; float fire_cd; float build_anim; float flash;
} Bld;

typedef struct { bool active; float x, y; int team, dmg, tk, ti; } Proj;
typedef struct { bool active; float x, y, t, maxt, size; int col; } Boom;

static int   terr[N];
static float spice[N];
static unsigned char tvar[N];
static Unit  units[MAXU];
static Bld   blds[MAXB];
static Proj  proj[MAXPROJ];
static Boom  booms[MAXBOOM];

static int   credits[2], powgen[2], powuse[2];
static bool  low_power[2];
static float shown_credits, low_warn_t;
static float ai_t, game_t; static bool ai_assault;
static int   over;                 // 0 playing, 1 win, 2 lose
static int   placing;              // -1 none, else B_* type to place
static int   selB; static bool selU[MAXU]; static int selN;
static bool  dragging; static int dragx0, dragy0;
static int   fire_voice;           // throttles weapon-fire sound

// ── pathfinding scratch ──────────────────────────────────────────────────────
static int  g_came[N], g_g[N]; static bool g_open[N], g_closed[N];
static const int DX4[4] = { 0, 1, 0, -1 }, DY4[4] = { -1, 0, 1, 0 };

// ── small helpers ──────────────────────────────────────────────────────────
static int  idx(int cx, int cy) { return cy * GW + cx; }
static int  pcx(int cx) { return MAPX + cx * TILE + TILE / 2; }
static int  pcy(int cy) { return MAPY + cy * TILE + TILE / 2; }
static bool in_grid(int cx, int cy) { return cx >= 0 && cy >= 0 && cx < GW && cy < GH; }
static int  team_col(int t) { return t == TEAM_P ? CLR_BLUE : CLR_RED; }
static int  team_dk(int t)  { return t == TEAM_P ? CLR_TRUE_BLUE : CLR_DARK_RED; }

static int bld_at(int cell) {
    for (int i = 0; i < MAXB; i++)
        if (blds[i].active && idx(blds[i].cx, blds[i].cy) == cell) return i;
    return -1;
}
static int cell_of(float x, float y) {
    int cx = mid(0, (int)((x - MAPX) / TILE), GW - 1);
    int cy = mid(0, (int)((y - MAPY) / TILE), GH - 1);
    return idx(cx, cy);
}
static int count_units(int team, int type) {
    int n = 0;
    for (int i = 0; i < MAXU; i++)
        if (units[i].active && units[i].team == team && (type < 0 || units[i].type == type)) n++;
    return n;
}
static int count_blds(int team, int type) {
    int n = 0;
    for (int i = 0; i < MAXB; i++)
        if (blds[i].active && blds[i].team == team && (type < 0 || blds[i].type == type)) n++;
    return n;
}

// ── A* on the building-blocked grid (goal cell is always allowed) ────────────
static int heur(int a, int b) { return abs(a % GW - b % GW) + abs(a / GW - b / GW); }
static int find_path(int s, int gcell, int *out) {
    if (s < 0 || gcell < 0 || s == gcell) return 0;
    for (int i = 0; i < N; i++) { g_g[i] = 1000000000; g_came[i] = -1; g_open[i] = g_closed[i] = false; }
    g_g[s] = 0; g_open[s] = true;
    int guard = 0;
    while (1) {
        int best = -1, bf = 2000000000;
        for (int i = 0; i < N; i++) if (g_open[i]) { int f = g_g[i] + heur(i, gcell); if (f < bf) { bf = f; best = i; } }
        if (best < 0) return 0;
        if (best == gcell) break;
        g_open[best] = false; g_closed[best] = true;
        int bx = best % GW, by = best / GW;
        for (int d = 0; d < 4; d++) {
            int nx = bx + DX4[d], ny = by + DY4[d];
            if (!in_grid(nx, ny)) continue;
            int ni = idx(nx, ny);
            if (g_closed[ni]) continue;
            if (ni != gcell && bld_at(ni) >= 0) continue;      // buildings block
            int tg = g_g[best] + 1;
            if (tg < g_g[ni]) { g_came[ni] = best; g_g[ni] = tg; g_open[ni] = true; }
        }
        if (++guard > 6000) return 0;
    }
    int tmp[N], n = 0;
    for (int c = gcell; c != -1 && c != s; c = g_came[c]) if (n < N) tmp[n++] = c;
    int len = 0;
    for (int i = n - 1; i >= 0 && len < MAXPATH; i--) out[len++] = tmp[i];
    return len;
}
static void set_path(Unit *u, int gcell) {
    u->plen = find_path(cell_of(u->x, u->y), gcell, u->path);
    u->pi = 0;
}
// returns true once the unit has consumed its whole path
static bool step_path(Unit *u) {
    if (u->pi >= u->plen) return true;
    int c = u->path[u->pi];
    float tx = pcx(c % GW), ty = pcy(c / GW);
    float a = angle_to((int)u->x, (int)u->y, (int)tx, (int)ty);
    u->face = a;
    float sp = unit_speed[u->type] * dt();
    u->x += dx(sp, a); u->y += dy(sp, a);
    if (near((int)u->x, (int)u->y, (int)tx, (int)ty, 2)) u->pi++;
    return u->pi >= u->plen;
}

// ── lookups ──────────────────────────────────────────────────────────────────
static int free_neighbor(int cx, int cy) {
    for (int d = 0; d < 8; d++) {
        int nx = cx + (d % 3) - 1, ny = cy + (d / 3) - 1;
        if ((nx == cx && ny == cy) || !in_grid(nx, ny)) continue;
        if (bld_at(idx(nx, ny)) < 0) return idx(nx, ny);
    }
    return idx(cx, cy);
}
static int nearest_refinery(int team, float x, float y) {
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < MAXB; i++)
        if (blds[i].active && blds[i].team == team && blds[i].type == B_REFINERY) {
            float d = distance((int)x, (int)y, pcx(blds[i].cx), pcy(blds[i].cy));
            if (d < bd) { bd = d; best = i; }
        }
    return best;
}
static int nearest_spice(float x, float y) {
    int sc = cell_of(x, y), best = -1, bd = 99999;
    for (int i = 0; i < N; i++)
        if (terr[i] == T_SPICE && spice[i] > 1.0f) {
            int d = heur(i, sc);
            if (d < bd) { bd = d; best = i; }
        }
    return best;
}
static int nearest_enemy_unit(int team, float x, float y, int maxr) {
    int best = -1; float bd = maxr;
    for (int i = 0; i < MAXU; i++)
        if (units[i].active && units[i].team != team) {
            float d = distance((int)x, (int)y, (int)units[i].x, (int)units[i].y);
            if (d <= bd) { bd = d; best = i; }
        }
    return best;
}
static int nearest_enemy_bld(int team, float x, float y, int maxr) {
    int best = -1; float bd = maxr;
    for (int i = 0; i < MAXB; i++)
        if (blds[i].active && blds[i].team != team) {
            float d = distance((int)x, (int)y, pcx(blds[i].cx), pcy(blds[i].cy));
            if (d <= bd) { bd = d; best = i; }
        }
    return best;
}

// ── effects ──────────────────────────────────────────────────────────────────
static void boom(float x, float y, float size, float maxt, int col) {
    for (int i = 0; i < MAXBOOM; i++)
        if (!booms[i].active) { booms[i] = (Boom){ true, x, y, 0, maxt, size, col }; return; }
}
static void fire_proj(float x, float y, int team, int dmg, int tk, int ti) {
    for (int i = 0; i < MAXPROJ; i++)
        if (!proj[i].active) { proj[i] = (Proj){ true, x, y, team, dmg, tk, ti }; break; }
    boom(x, y, 3, 0.10f, CLR_YELLOW);                          // muzzle flash
    if ((fire_voice++ & 3) == 0) note(74, 5, 1);               // thinned weapon blip
}

// ── damage ───────────────────────────────────────────────────────────────────
static void check_over(void) {
    if (count_blds(TEAM_E, -1) == 0) over = 1;
    else if (count_blds(TEAM_P, -1) == 0) over = 2;
}
static void hurt_unit(int i, int dmg) {
    Unit *u = &units[i]; u->hp -= dmg; u->flash = 0.12f;
    if (u->hp <= 0) {
        u->active = false; selU[i] = false;
        boom(u->x, u->y, u->type == U_TANK ? 11 : 8, 0.45f, CLR_ORANGE);
        hit(40, 6, 3, 180); shake(3);
    }
}
static void hurt_bld(int i, int dmg) {
    Bld *b = &blds[i]; b->hp -= dmg; b->flash = 0.12f;
    if (b->hp <= 0) {
        b->active = false; if (selB == i) selB = -1;
        boom(pcx(b->cx), pcy(b->cy), TILE, 0.6f, CLR_DARK_ORANGE);
        boom(pcx(b->cx), pcy(b->cy), TILE - 4, 0.45f, CLR_YELLOW);
        hit(30, 6, 5, 280); shake(7);
        check_over();
    }
}

// ── spawning ─────────────────────────────────────────────────────────────────
static int free_spawn(int cx, int cy) {
    for (int r = 1; r <= 4; r++)
        for (int dy = -r; dy <= r; dy++) for (int dxx = -r; dxx <= r; dxx++) {
            if (abs(dxx) != r && abs(dy) != r) continue;
            int nx = cx + dxx, ny = cy + dy;
            if (in_grid(nx, ny) && bld_at(idx(nx, ny)) < 0) return idx(nx, ny);
        }
    return idx(cx, cy);
}
static int spawn_unit(int type, int team, int cell) {
    for (int i = 0; i < MAXU; i++) if (!units[i].active) {
        Unit *u = &units[i];
        *u = (Unit){ 0 };
        u->active = true; u->type = type; u->team = team;
        u->x = pcx(cell % GW); u->y = pcy(cell / GW);
        u->hp = u->maxhp = unit_hp[type];
        u->state = (type == U_HARVESTER) ? ST_SEEK : ST_IDLE;
        u->harv_cell = -1; u->goal_cell = cell;
        u->face = (team == TEAM_P) ? 0 : 180;
        return i;
    }
    return -1;
}
static int make_bld(int type, int team, int cx, int cy) {
    for (int i = 0; i < MAXB; i++) if (!blds[i].active) {
        blds[i] = (Bld){ 0 };
        blds[i].active = true; blds[i].type = type; blds[i].team = team;
        blds[i].cx = cx; blds[i].cy = cy;
        blds[i].hp = blds[i].maxhp = bld_hp[type];
        blds[i].build_anim = 1.0f;
        return i;
    }
    return -1;
}

// ── map generation + reset ───────────────────────────────────────────────────
static void stamp_spice(int ccx, int ccy, int rad) {
    for (int dy = -rad; dy <= rad; dy++) for (int dxx = -rad; dxx <= rad; dxx++) {
        int x = ccx + dxx, y = ccy + dy;
        if (!in_grid(x, y)) continue;
        float d = length(dxx, dy);
        if (d > rad + 0.3f) continue;
        int i = idx(x, y);
        if (terr[i] == T_ROCK) continue;
        terr[i] = T_SPICE;
        float amt = remap(d, 0, rad, 100, 30);
        if (amt > spice[i]) spice[i] = clamp(amt, 0, 100);
    }
}
static void gen_map(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        int i = idx(x, y);
        tvar[i] = (x * 7 + y * 13) % 4;
        float n = noise2(x * 0.55f + 4.2f, y * 0.55f + 1.3f);
        terr[i] = (n > 0.66f) ? T_ROCK : T_SAND;
        spice[i] = 0;
    }
    // keep both base footprints clear sand
    int bx[] = { 1, 2, 3, 0, 4 }, by[] = { 10, 11, 12, 11, 11 };
    for (int k = 0; k < 5; k++) if (in_grid(bx[k], by[k])) terr[idx(bx[k], by[k])] = T_SAND;
    int ex[] = { GW - 2, GW - 3, GW - 4, GW - 1, GW - 5 }, ey[] = { 2, 1, 0, 1, 1 };
    for (int k = 0; k < 5; k++) if (in_grid(ex[k], ey[k])) terr[idx(ex[k], ey[k])] = T_SAND;
    // spice fields: near each base + a contested centre
    stamp_spice(5, GH - 4, 2);
    stamp_spice(GW - 6, 3, 2);
    stamp_spice(GW / 2, GH / 2, 3);
}
static void reset_game(void) {
    for (int i = 0; i < MAXU; i++) units[i].active = false;
    for (int i = 0; i < MAXB; i++) blds[i].active = false;
    for (int i = 0; i < MAXPROJ; i++) proj[i].active = false;
    for (int i = 0; i < MAXBOOM; i++) booms[i].active = false;
    for (int i = 0; i < MAXU; i++) selU[i] = false;
    gen_map();
    credits[TEAM_P] = credits[TEAM_E] = 400;
    shown_credits = 400; selB = -1; selN = 0; placing = -1; dragging = false;
    over = 0; ai_t = 2.0f; ai_assault = false; game_t = 0; low_warn_t = 0;
    // player base (bottom-left)
    make_bld(B_REFINERY, TEAM_P, 2, 10);
    make_bld(B_POWER,    TEAM_P, 0, 11);
    make_bld(B_FACTORY,  TEAM_P, 3, 12);
    spawn_unit(U_HARVESTER, TEAM_P, idx(2, 8));
    spawn_unit(U_HARVESTER, TEAM_P, idx(4, 9));
    // enemy base (top-right)
    make_bld(B_REFINERY, TEAM_E, GW - 3, 2);
    make_bld(B_POWER,    TEAM_E, GW - 1, 1);
    make_bld(B_FACTORY,  TEAM_E, GW - 4, 0);
    spawn_unit(U_HARVESTER, TEAM_E, idx(GW - 3, 4));
    spawn_unit(U_HARVESTER, TEAM_E, idx(GW - 5, 3));
}

void init(void) {
    instrument(5, INSTR_SQUARE, 1, 50, 1, 40);  instrument_duty(5, 0.3f);                 // weapon
    instrument(6, INSTR_NOISE, 1, 150, 0, 220); instrument_filter(6, FILTER_LOW, 700, 4); // boom
    instrument(7, INSTR_SINE, 1, 90, 3, 130);                                             // credit chime
    instrument(8, INSTR_TRI, 4, 120, 4, 200);                                             // build sting
    instrument(9, INSTR_NOISE, 220, 200, 3, 600); instrument_filter(9, FILTER_LOW, 380, 2);
    instrument_lfo(9, 0, LFO_CUTOFF, 0.4f, 180);                                          // desert wind
    lfo_shape(9, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)
    bpm(96);
    reset_game();
}

// ── unit AI / movement ───────────────────────────────────────────────────────
static int target_cell_for(Unit *u) {
    if (u->tk == TK_UNIT && units[u->ti].active)
        return cell_of(units[u->ti].x, units[u->ti].y);
    if (u->tk == TK_BLD && blds[u->ti].active)
        return free_neighbor(blds[u->ti].cx, blds[u->ti].cy);
    return -1;
}
static bool target_alive(Unit *u) {
    return (u->tk == TK_UNIT && units[u->ti].active) || (u->tk == TK_BLD && blds[u->ti].active);
}
static float target_dist(Unit *u) {
    if (u->tk == TK_UNIT) return distance((int)u->x, (int)u->y, (int)units[u->ti].x, (int)units[u->ti].y);
    if (u->tk == TK_BLD)  return distance((int)u->x, (int)u->y, pcx(blds[u->ti].cx), pcy(blds[u->ti].cy));
    return 1e9f;
}
static void fire_at_target(Unit *u) {
    int tx, ty;
    if (u->tk == TK_UNIT) { tx = (int)units[u->ti].x; ty = (int)units[u->ti].y; }
    else                  { tx = pcx(blds[u->ti].cx); ty = pcy(blds[u->ti].cy); }
    u->face = angle_to((int)u->x, (int)u->y, tx, ty);
    fire_proj(u->x + dx(7, u->face), u->y + dy(7, u->face), u->team, unit_dmg[u->type], u->tk, u->ti);
    u->fire_cd = unit_firecd[u->type];
}

static void update_unit(int i) {
    Unit *u = &units[i];
    float D = dt();
    if (u->flash > 0) u->flash -= D;
    if (u->fire_cd > 0) u->fire_cd -= D;
    if (u->repath_cd > 0) u->repath_cd -= D;

    if (u->type == U_HARVESTER) {
        switch (u->state) {
        case ST_IDLE: u->state = ST_SEEK; break;
        case ST_SEEK: {
            int sc = nearest_spice(u->x, u->y);
            if (sc < 0) { u->state = ST_IDLE; break; }
            if (sc != u->harv_cell || u->plen == 0) { u->harv_cell = sc; set_path(u, sc); }
            if (step_path(u)) u->state = (cell_of(u->x, u->y) == u->harv_cell) ? ST_HARVEST : ST_SEEK;
        } break;
        case ST_HARVEST: {
            int c = u->harv_cell;
            float take = clamp(40 * D, 0, clamp(spice[c], 0, CARRY_CAP - u->carry));
            spice[c] -= take; u->carry += take;
            if (chance(8)) boom(u->x + rnd_between(-4, 4), u->y - 4, 2, 0.3f, CLR_ORANGE);
            if (u->carry >= CARRY_CAP - 0.5f || spice[c] <= 0.5f) u->state = ST_RETURN;
        } break;
        case ST_RETURN: {
            int r = nearest_refinery(u->team, u->x, u->y);
            if (r < 0) { u->state = ST_IDLE; break; }
            int goal = free_neighbor(blds[r].cx, blds[r].cy);
            if (u->plen == 0 || u->repath_cd <= 0) { set_path(u, goal); u->repath_cd = 0.8f; }
            if (step_path(u) || near((int)u->x, (int)u->y, pcx(blds[r].cx), pcy(blds[r].cy), TILE + 2))
                u->state = ST_DUMP;
        } break;
        case ST_DUMP: {
            credits[u->team] += (int)(u->carry * SPICE_VALUE);
            if (u->team == TEAM_P) { note(76, 7, 2); boom(u->x, u->y - 4, 4, 0.4f, CLR_LIGHT_YELLOW); }
            u->carry = 0; u->state = ST_SEEK; u->plen = 0;
        } break;
        case ST_MOVE: if (step_path(u)) u->state = ST_SEEK; break;
        }
        return;
    }

    // ── combat units ──
    switch (u->state) {
    case ST_MOVE:
        if (step_path(u)) u->state = ST_IDLE;
        break;
    case ST_IDLE: {
        int e = nearest_enemy_unit(u->team, u->x, u->y, unit_range[u->type] + 22);
        if (e >= 0) { u->tk = TK_UNIT; u->ti = e; u->state = ST_ATTACK; break; }
        int b = nearest_enemy_bld(u->team, u->x, u->y, unit_range[u->type] + 8);
        if (b >= 0) { u->tk = TK_BLD; u->ti = b; u->state = ST_ATTACK; }
    } break;
    case ST_ATTACK: {
        if (!target_alive(u)) {
            u->tk = TK_NONE;
            if (u->team == TEAM_E && ai_assault) {            // enemy presses the assault
                int b = nearest_enemy_bld(u->team, u->x, u->y, 9999);
                if (b >= 0) { u->tk = TK_BLD; u->ti = b; break; }
            }
            u->state = ST_IDLE; break;
        }
        if (target_dist(u) <= unit_range[u->type]) {
            if (u->tk == TK_UNIT) u->face = angle_to((int)u->x, (int)u->y, (int)units[u->ti].x, (int)units[u->ti].y);
            else                  u->face = angle_to((int)u->x, (int)u->y, pcx(blds[u->ti].cx), pcy(blds[u->ti].cy));
            if (u->fire_cd <= 0) fire_at_target(u);
        } else {
            if (u->pi >= u->plen || u->repath_cd <= 0) { set_path(u, target_cell_for(u)); u->repath_cd = 0.5f; }
            step_path(u);
        }
    } break;
    }
}

// ── buildings: production + turrets ──────────────────────────────────────────
static void enqueue(Bld *b, int type) {
    if (b->qn >= 6) return;
    if (credits[b->team] < unit_cost[type]) return;
    credits[b->team] -= unit_cost[type];
    b->queue[b->qn++] = type;
}
static void update_bld(int i) {
    Bld *b = &blds[i];
    float D = dt();
    if (b->build_anim > 0) b->build_anim -= D * 2.0f;
    if (b->flash > 0) b->flash -= D;

    if (b->type == B_FACTORY && b->qn > 0) {
        b->prod_t += D * (low_power[b->team] ? 0.5f : 1.0f);
        if (b->prod_t >= unit_build_t[b->queue[0]]) {
            int cell = free_spawn(b->cx, b->cy);
            spawn_unit(b->queue[0], b->team, cell);
            if (b->team == TEAM_P) note(67, 8, 2);
            b->prod_t = 0; b->qn--;
            for (int q = 0; q < b->qn; q++) b->queue[q] = b->queue[q + 1];
        }
    }
    if (b->type == B_TURRET) {
        if (b->fire_cd > 0) b->fire_cd -= D;
        if (b->fire_cd <= 0) {
            int e = nearest_enemy_unit(b->team, pcx(b->cx), pcy(b->cy), TUR_RANGE);
            int tk = TK_UNIT, ti = e;
            if (e < 0) { ti = nearest_enemy_bld(b->team, pcx(b->cx), pcy(b->cy), TUR_RANGE); tk = TK_BLD; }
            if (ti >= 0) {
                fire_proj(pcx(b->cx), pcy(b->cy) - 4, b->team, TUR_DMG, tk, ti);
                b->fire_cd = TUR_FIRECD * (low_power[b->team] ? 1.7f : 1.0f);
            }
        }
    }
}

// ── projectiles ───────────────────────────────────────────────────────────────
static void update_proj(int i) {
    Proj *p = &proj[i];
    int tx, ty; bool alive;
    if (p->tk == TK_UNIT) { alive = units[p->ti].active; tx = (int)units[p->ti].x; ty = (int)units[p->ti].y; }
    else                  { alive = blds[p->ti].active;  tx = pcx(blds[p->ti].cx); ty = pcy(blds[p->ti].cy); }
    if (!alive) { p->active = false; return; }
    float a = angle_to((int)p->x, (int)p->y, tx, ty);
    float sp = 150 * dt();
    p->x += dx(sp, a); p->y += dy(sp, a);
    if (near((int)p->x, (int)p->y, tx, ty, 5)) {
        if (p->tk == TK_UNIT) hurt_unit(p->ti, p->dmg);
        else                  hurt_bld(p->ti, p->dmg);
        boom(p->x, p->y, 3, 0.12f, CLR_WHITE);
        p->active = false;
    }
}

// ── enemy AI ──────────────────────────────────────────────────────────────────
static int find_build_cell(int ccx, int ccy) {
    for (int r = 1; r <= 6; r++)
        for (int dy = -r; dy <= r; dy++) for (int dxx = -r; dxx <= r; dxx++) {
            if (abs(dxx) != r && abs(dy) != r) continue;
            int nx = ccx + dxx, ny = ccy + dy;
            if (in_grid(nx, ny) && terr[idx(nx, ny)] != T_SPICE && bld_at(idx(nx, ny)) < 0)
                return idx(nx, ny);
        }
    return -1;
}
static void ai_think(void) {
    int T = TEAM_E, bc = idx(GW - 3, 2);
    int nref = count_blds(T, B_REFINERY), npow = count_blds(T, B_POWER);
    int nfac = count_blds(T, B_FACTORY), ntur = count_blds(T, B_TURRET);
    int nharv = count_units(T, U_HARVESTER), ntank = count_units(T, U_TANK) + count_units(T, U_TRIKE);

    // structure priorities
    int want = -1;
    if (nref == 0)                                   want = B_REFINERY;
    else if (low_power[T] || npow == 0)              want = B_POWER;
    else if (nfac == 0)                              want = B_FACTORY;
    else if (ntur < 2 && credits[T] >= bld_cost[B_TURRET] + 80) want = B_TURRET;
    if (want >= 0 && credits[T] >= bld_cost[want]) {
        int cell = find_build_cell(bc % GW, bc / GW);
        if (cell >= 0) { credits[T] -= bld_cost[want]; make_bld(want, T, cell % GW, cell / GW); }
        return;
    }
    // factory production
    int fi = -1;
    for (int i = 0; i < MAXB; i++) if (blds[i].active && blds[i].team == T && blds[i].type == B_FACTORY && blds[i].qn == 0) { fi = i; break; }
    if (fi >= 0) {
        if (nharv < 2)      enqueue(&blds[fi], U_HARVESTER);
        else if (ntank < 6) enqueue(&blds[fi], (frame() & 1) ? U_TANK : U_TRIKE);
    }
    // launch / regroup the assault
    if (ntank >= 3) ai_assault = true;
    if (ntank == 0) ai_assault = false;
    if (ai_assault) {
        for (int i = 0; i < MAXU; i++) {
            Unit *u = &units[i];
            if (!u->active || u->team != T || u->type == U_HARVESTER) continue;
            if (u->state == ST_IDLE || (u->state == ST_ATTACK && !target_alive(u))) {
                int b = nearest_enemy_bld(T, u->x, u->y, 9999);
                if (b >= 0) { u->tk = TK_BLD; u->ti = b; u->state = ST_ATTACK; u->repath_cd = 0; }
            }
        }
    }
}

// ── input ────────────────────────────────────────────────────────────────────
static bool in_map(int x, int y) { return x >= MAPX && y >= MAPY && x < MAPX + GW * TILE && y < MAPY + GH * TILE; }
static int unit_at_px(int team, int x, int y) {
    int best = -1; float bd = 10;
    for (int i = 0; i < MAXU; i++)
        if (units[i].active && units[i].team == team) {
            float d = distance(x, y, (int)units[i].x, (int)units[i].y);
            if (d < bd) { bd = d; best = i; }
        }
    return best;
}
static void clear_sel(void) { for (int i = 0; i < MAXU; i++) selU[i] = false; selN = 0; selB = -1; }

static void command_to(int cx, int cy) {
    int cell = idx(cx, cy);
    int eu = -1; float bd = 11;
    for (int i = 0; i < MAXU; i++) if (units[i].active && units[i].team == TEAM_E) {
        float d = distance(pcx(cx), pcy(cy), (int)units[i].x, (int)units[i].y);
        if (d < bd) { bd = d; eu = i; }
    }
    int eb = bld_at(cell);
    if (eb >= 0 && blds[eb].team != TEAM_P) eb = eb; else if (eb >= 0) eb = -1;
    for (int i = 0; i < MAXU; i++) if (selU[i] && units[i].active) {
        Unit *u = &units[i];
        if (eu >= 0)       { u->tk = TK_UNIT; u->ti = eu; u->state = ST_ATTACK; u->repath_cd = 0; }
        else if (eb >= 0)  { u->tk = TK_BLD;  u->ti = eb; u->state = ST_ATTACK; u->repath_cd = 0; }
        else {
            if (u->type == U_HARVESTER) { u->state = ST_MOVE; }
            else                        { u->state = ST_MOVE; u->tk = TK_NONE; }
            u->goal_cell = cell; set_path(u, cell);
        }
    }
}

static void handle_map_click(void) {
    int mx = mouse_x(), my = mouse_y();
    int cx = (mx - MAPX) / TILE, cy = (my - MAPY) / TILE;
    if (!in_grid(cx, cy)) return;
    // placing a structure
    if (placing >= 0) {
        int cell = idx(cx, cy);
        bool ok = terr[cell] != T_SPICE && bld_at(cell) < 0 && credits[TEAM_P] >= bld_cost[placing];
        if (ok) {
            credits[TEAM_P] -= bld_cost[placing];
            make_bld(placing, TEAM_P, cx, cy);
            strum(60, CHORD_MAJ, 8, 3, 40);
            if (!key(KEY_TAB)) placing = -1;     // hold TAB to place several
        }
        return;
    }
    // select own unit
    int su = unit_at_px(TEAM_P, mx, my);
    if (su >= 0) { clear_sel(); selU[su] = true; selN = 1; return; }
    // select own building
    int sb = bld_at(idx(cx, cy));
    if (sb >= 0 && blds[sb].team == TEAM_P) { clear_sel(); selB = sb; return; }
    // command selected units, else clear
    if (selN > 0) command_to(cx, cy);
    else clear_sel();
}

static void box_select(int x0, int y0, int x1, int y1) {
    int lx = min(x0, x1), ly = min(y0, y1), hx = max(x0, x1), hy = max(y0, y1);
    clear_sel();
    for (int i = 0; i < MAXU; i++)
        if (units[i].active && units[i].team == TEAM_P &&
            point_in_box((int)units[i].x, (int)units[i].y, lx, ly, hx - lx, hy - ly)) {
            selU[i] = true; selN++;
        }
}

// ── sidebar buttons ──────────────────────────────────────────────────────────
static bool clicked_rect(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) && mouse_x() >= x && mouse_x() < x + w && mouse_y() >= y && mouse_y() < y + h;
}
static bool button(int x, int y, int w, int h, const char *label, const char *cost, int accent, bool enabled, bool hot) {
    int bg = enabled ? (hot ? CLR_DARK_GREEN : CLR_DARKER_GREY) : CLR_BROWNISH_BLACK;
    rectfill(x, y, w, h, bg);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_DARK_GREY);
    rectfill(x + 2, y + 2, 3, h - 4, accent);
    print(label, x + 7, y + 2, enabled ? CLR_WHITE : CLR_DARK_GREY);
    if (cost) print_right(cost, x + w - 3, y + 2, enabled ? CLR_YELLOW : CLR_DARK_GREY);
    return enabled && clicked_rect(x, y, w, h);
}

static void update_sidebar(void) {
    int x = PANELX + 2, w = PW - 4, y = 42;
    // structure build buttons
    for (int t = 0; t < B_COUNT; t++) {
        bool en = credits[TEAM_P] >= bld_cost[t];
        bool hot = placing == t;
        int ac = t == B_TURRET ? CLR_ORANGE : t == B_POWER ? CLR_YELLOW : t == B_REFINERY ? CLR_LIME_GREEN : CLR_BLUE;
        if (button(x, y, w, 13, BNAME[t], str("%d", bld_cost[t]), ac, en, hot))
            placing = (placing == t) ? -1 : t;
        y += 14;
    }
    // factory production
    if (selB >= 0 && blds[selB].active && blds[selB].type == B_FACTORY && blds[selB].team == TEAM_P) {
        y = 110;
        for (int u = 0; u < U_COUNT; u++) {
            bool en = credits[TEAM_P] >= unit_cost[u] && blds[selB].qn < 6;
            if (button(x, y, w, 13, UNAME[u], str("%d", unit_cost[u]), CLR_BLUE, en, false))
                enqueue(&blds[selB], u);
            y += 14;
        }
    }
}

// ── main update ────────────────────────────────────────────────────────────
void update(void) {
    if (over) {
        if (btnp(0, BTN_A) || mouse_pressed(MOUSE_LEFT)) reset_game();
        return;
    }
    float D = dt();
    game_t += D;

    // power per team
    for (int t = 0; t < 2; t++) {
        powgen[t] = powuse[t] = 0;
        bool was = low_power[t];
        for (int i = 0; i < MAXB; i++) if (blds[i].active && blds[i].team == t) {
            powgen[t] += bld_powgen[blds[i].type];
            powuse[t] += bld_powuse[blds[i].type];
        }
        low_power[t] = powuse[t] > powgen[t];
        if (t == TEAM_P && low_power[t] && !was && low_warn_t <= 0) { note(45, 5, 2); low_warn_t = 3; }
    }
    if (low_warn_t > 0) low_warn_t -= D;

    // hotkeys for build
    for (int t = 0; t < B_COUNT; t++)
        if (keyp('1' + t) && credits[TEAM_P] >= bld_cost[t]) placing = (placing == t) ? -1 : t;
    if (mouse_pressed(MOUSE_RIGHT) || keyp(KEY_ESCAPE)) { placing = -1; clear_sel(); }

    // mouse: drag-box / click in map
    if (mouse_pressed(MOUSE_LEFT) && in_map(mouse_x(), mouse_y()) && placing < 0) {
        dragging = true; dragx0 = mouse_x(); dragy0 = mouse_y();
    }
    if (dragging && mouse_released(MOUSE_LEFT)) {
        dragging = false;
        if (abs(mouse_x() - dragx0) > 5 || abs(mouse_y() - dragy0) > 5)
            box_select(dragx0, dragy0, mouse_x(), mouse_y());
        else handle_map_click();
    }
    // placing places on press (no drag)
    if (placing >= 0 && mouse_pressed(MOUSE_LEFT) && in_map(mouse_x(), mouse_y())) handle_map_click();

    update_sidebar();

    // AI
    ai_t -= D;
    if (ai_t <= 0) { ai_t = 1.4f; ai_think(); }

    // sim
    for (int i = 0; i < MAXB; i++) if (blds[i].active) update_bld(i);
    for (int i = 0; i < MAXU; i++) if (units[i].active) update_unit(i);
    for (int i = 0; i < MAXPROJ; i++) if (proj[i].active) update_proj(i);
    for (int i = 0; i < MAXBOOM; i++) if (booms[i].active) { booms[i].t += D; if (booms[i].t >= booms[i].maxt) booms[i].active = false; }

    // spice slowly regrows
    for (int i = 0; i < N; i++) if (terr[i] == T_SPICE && spice[i] < 100) spice[i] = clamp(spice[i] + 1.5f * D, 0, 100);

    // ambient desert wind
    if (every(2) && chance(35)) note(rnd_between(30, 38), 9, 1);

    shown_credits = lerp(shown_credits, credits[TEAM_P], clamp(6 * D, 0, 1));
}

// ── drawing ──────────────────────────────────────────────────────────────────
static void draw_terrain(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        int i = idx(x, y), px = MAPX + x * TILE, py = MAPY + y * TILE;
        if (terr[i] == T_ROCK) {
            rectfill(px, py, TILE, TILE, CLR_DARK_GREY);
            pset(px + 3 + tvar[i], py + 4, CLR_DARKER_GREY);
            pset(px + 8, py + 9 - tvar[i], CLR_LIGHT_GREY);
        } else if (terr[i] == T_SPICE && spice[i] > 1.0f) {
            int base = spice[i] > 66 ? CLR_ORANGE : spice[i] > 33 ? CLR_DARK_ORANGE : CLR_BROWN;
            rectfill(px, py, TILE, TILE, base);
            if ((frame() + x * 3 + y * 5) % 26 < 3)
                pset(px + 3 + (tvar[i] * 2), py + 4 + tvar[i], CLR_LIGHT_YELLOW);
        } else {
            rectfill(px, py, TILE, TILE, CLR_BROWN);
            pset(px + 2 + tvar[i] * 3, py + 5, CLR_DARK_PEACH);
            pset(px + 9, py + 10 - tvar[i], CLR_DARK_BROWN);
        }
    }
}
static void draw_bld(int i) {
    Bld *b = &blds[i];
    int px = MAPX + b->cx * TILE, py = MAPY + b->cy * TILE;
    int col = team_col(b->team), dk = team_dk(b->team);
    int body = b->flash > 0 ? CLR_WHITE : dk;
    rectfill(px + 1, py + 1, TILE - 2, TILE - 2, body);
    rect(px + 1, py + 1, TILE - 2, TILE - 2, col);
    int cx = px + TILE / 2, cy = py + TILE / 2;
    switch (b->type) {
    case B_REFINERY: ovalfill(cx, cy + 1, 4, 3, CLR_MEDIUM_GREY); rectfill(cx - 1, py + 2, 2, 3, CLR_ORANGE); break;
    case B_POWER:    line(cx + 1, py + 3, cx - 2, cy, CLR_YELLOW); line(cx - 2, cy, cx + 1, py + TILE - 3, CLR_YELLOW); break;
    case B_FACTORY:  rectfill(cx - 2, cy, 4, 5, CLR_BLACK); line(px + 3, py + 3, px + TILE - 4, py + 3, CLR_MEDIUM_GREY); break;
    case B_TURRET: {
        circfill(cx, cy, 3, CLR_DARK_GREY);
        int e = nearest_enemy_unit(b->team, cx, cy, 9999);
        float ang = e >= 0 ? angle_to(cx, cy, (int)units[e].x, (int)units[e].y) : (b->team == TEAM_P ? -45 : 135);
        line(cx, cy, cx + (int)dx(6, ang), cy + (int)dy(6, ang), col);
    } break;
    }
    if (b->build_anim > 0 && blink(4)) rect(px, py, TILE, TILE, CLR_WHITE);
    // health bar
    if (b->hp < b->maxhp)
        bar(px + 1, py - 3, TILE - 2, 2, (float)b->hp / b->maxhp, CLR_GREEN, CLR_DARK_RED);
    if (selB == i) rect(px - 1, py - 1, TILE + 2, TILE + 2, CLR_GREEN);
}
static void draw_unit(int i) {
    Unit *u = &units[i];
    int x = (int)u->x, y = (int)u->y;
    int col = u->flash > 0 ? CLR_WHITE : team_col(u->team);
    int dk = team_dk(u->team);
    if (selU[i]) circ(x, y, 8, CLR_GREEN);
    switch (u->type) {
    case U_HARVESTER:
        rectfill(x - 5, y - 4, 10, 8, dk);
        rect(x - 5, y - 4, 10, 8, col);
        rectfill(x - 5, y + 4, 10, 1, CLR_DARK_GREY);          // tread
        if (u->carry > 0)                                       // spice load
            rectfill(x - 4, y - 3, (int)(8 * u->carry / CARRY_CAP), 3, CLR_ORANGE);
        if (u->carry >= CARRY_CAP - 1 && blink(8)) pset(x, y - 6, CLR_LIGHT_YELLOW);
        break;
    case U_TRIKE:
        circfill(x, y, 3, col);
        line(x, y, x + (int)dx(6, u->face), y + (int)dy(6, u->face), dk);
        break;
    case U_TANK:
        rectfill(x - 5, y - 4, 10, 8, dk);
        rect(x - 5, y - 4, 10, 8, col);
        circfill(x, y, 3, col);
        line(x, y, x + (int)dx(8, u->face), y + (int)dy(8, u->face), CLR_DARK_GREY);
        break;
    }
    if (u->hp < u->maxhp)
        bar(x - 6, y - 8, 12, 2, (float)u->hp / u->maxhp, CLR_GREEN, CLR_DARK_RED);
}
static void draw_panel(void) {
    rectfill(PANELX, 0, PW, SCREEN_H, CLR_BROWNISH_BLACK);
    line(PANELX, 0, PANELX, SCREEN_H, CLR_DARK_GREY);
    print("ARRAKIS", PANELX + 4, 3, CLR_ORANGE);
    print(str("$%d", (int)shown_credits), PANELX + 4, 14, CLR_YELLOW);
    int pc = low_power[TEAM_P] ? (blink(15) ? CLR_RED : CLR_DARK_RED) : CLR_LIME_GREEN;
    print(str("PWR %d/%d", powgen[TEAM_P], powuse[TEAM_P]), PANELX + 4, 24, pc);
    // context info at bottom
    int y = 156;
    if (selB >= 0 && blds[selB].active && blds[selB].type == B_FACTORY) {
        Bld *b = &blds[selB];
        print("QUEUE", PANELX + 4, y, CLR_LIGHT_GREY);
        for (int q = 0; q < b->qn; q++)
            rectfill(PANELX + 4 + q * 6, y + 9, 5, 5, q == 0 ? CLR_BLUE : CLR_TRUE_BLUE);
        if (b->qn > 0) bar(PANELX + 4, y + 17, PW - 8, 3, b->prod_t / unit_build_t[b->queue[0]], CLR_LIME_GREEN, CLR_DARKER_GREY);
    } else if (selN > 0) {
        print(str("%d UNIT%s", selN, selN > 1 ? "S" : ""), PANELX + 4, y, CLR_WHITE);
        print("click to move", PANELX + 4, y + 10, CLR_LIGHT_GREY);
        print("red = attack", PANELX + 4, y + 19, CLR_LIGHT_GREY);
    } else if (placing >= 0) {
        print(str("PLACE %s", BNAME[placing]), PANELX + 4, y, CLR_WHITE);
        print("click the map", PANELX + 4, y + 10, CLR_LIME_GREEN);
    } else {
        print("pick a unit,", PANELX + 4, y, CLR_DARK_GREY);
        print("building, or", PANELX + 4, y + 9, CLR_DARK_GREY);
        print("BUILD above", PANELX + 4, y + 18, CLR_DARK_GREY);
    }
    print(str("ENEMY: %d", count_blds(TEAM_E, -1)), PANELX + 4, SCREEN_H - 9, CLR_RED);

    // redraw the build buttons (visual; clicks handled in update)
    int bx = PANELX + 2, bw = PW - 4, by = 42;
    for (int t = 0; t < B_COUNT; t++) {
        bool en = credits[TEAM_P] >= bld_cost[t];
        bool hot = placing == t;
        int ac = t == B_TURRET ? CLR_ORANGE : t == B_POWER ? CLR_YELLOW : t == B_REFINERY ? CLR_LIME_GREEN : CLR_BLUE;
        button(bx, by, bw, 13, BNAME[t], str("%d", bld_cost[t]), ac, en, hot);
        by += 14;
    }
    if (selB >= 0 && blds[selB].active && blds[selB].type == B_FACTORY) {
        by = 110;
        for (int u = 0; u < U_COUNT; u++) {
            bool en = credits[TEAM_P] >= unit_cost[u] && blds[selB].qn < 6;
            button(bx, by, bw, 13, UNAME[u], str("%d", unit_cost[u]), CLR_BLUE, en, false);
            by += 14;
        }
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    draw_terrain();

    for (int i = 0; i < MAXB; i++) if (blds[i].active) draw_bld(i);
    for (int i = 0; i < MAXU; i++) if (units[i].active) draw_unit(i);
    for (int i = 0; i < MAXPROJ; i++) if (proj[i].active)
        circfill((int)proj[i].x, (int)proj[i].y, 2, proj[i].team == TEAM_P ? CLR_BLUE : CLR_RED);
    for (int i = 0; i < MAXBOOM; i++) if (booms[i].active) {
        Boom *bm = &booms[i];
        float t = clamp(bm->t / bm->maxt, 0, 1);
        int r = (int)(bm->size * ease_out(t));
        fillp(FILL_CHECKER, -1);
        circfill((int)bm->x, (int)bm->y, r, bm->col);
        fillp_reset();
        circ((int)bm->x, (int)bm->y, r, CLR_YELLOW);
    }

    // drag-box
    if (dragging) {
        int lx = min(dragx0, mouse_x()), ly = min(dragy0, mouse_y());
        rect(lx, ly, abs(mouse_x() - dragx0), abs(mouse_y() - dragy0), CLR_GREEN);
    }
    // placement ghost
    if (placing >= 0 && in_map(mouse_x(), mouse_y())) {
        int cx = (mouse_x() - MAPX) / TILE, cy = (mouse_y() - MAPY) / TILE;
        if (in_grid(cx, cy)) {
            bool ok = terr[idx(cx, cy)] != T_SPICE && bld_at(idx(cx, cy)) < 0 && credits[TEAM_P] >= bld_cost[placing];
            fillp(FILL_CHECKER, -1);
            rectfill(MAPX + cx * TILE, MAPY + cy * TILE, TILE, TILE, ok ? CLR_GREEN : CLR_RED);
            fillp_reset();
        }
    }
    // crosshair cursor in map
    if (in_map(mouse_x(), mouse_y()) && placing < 0) {
        int mx = mouse_x(), my = mouse_y();
        line(mx - 4, my, mx + 4, my, CLR_WHITE);
        line(mx, my - 4, mx, my + 4, CLR_WHITE);
    }

    // top HUD
    rectfill(0, 0, PANELX, MAPY, CLR_BLACK);
    print(str("$%d", (int)shown_credits), 3, 2, CLR_YELLOW);
    print(str("PWR %d/%d", powgen[TEAM_P], powuse[TEAM_P]),
          80, 2, low_power[TEAM_P] ? CLR_RED : CLR_LIME_GREEN);
    print(str("%02d:%02d", (int)game_t / 60, (int)game_t % 60), 190, 2, CLR_LIGHT_GREY);
    if (low_power[TEAM_P] && blink(15)) print_right("LOW POWER", PANELX - 60, 2, CLR_RED);

    draw_panel();

    if (over) {
        rectfill(SCREEN_W / 2 - 78, SCREEN_H / 2 - 24, 156, 48, CLR_BLACK);
        rect(SCREEN_W / 2 - 78, SCREEN_H / 2 - 24, 156, 48, over == 1 ? CLR_GREEN : CLR_RED);
        print_centered(over == 1 ? "ARRAKIS IS YOURS" : "BASE DESTROYED", SCREEN_W/2, SCREEN_H / 2 - 14, over == 1 ? CLR_LIME_GREEN : CLR_RED);
        print_centered(str("%02d:%02d on the clock", (int)game_t / 60, (int)game_t % 60), SCREEN_W/2, SCREEN_H / 2 - 1, CLR_YELLOW);
        print_centered("Z or click for a new map", SCREEN_W/2, SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    }
    cursor_draw(placing >= 0 ? CUR_CROSS : CUR_ARROW);
}
