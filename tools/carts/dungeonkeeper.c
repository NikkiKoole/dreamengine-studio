/* de:meta
{
  "title": "dungeon keeper",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "pathfinding",
    "finite-state-ai",
    "euclidean-rhythm",
    "tilemap-collision"
  ],
  "lineage": "Dungeon Keeper (1997) homage; novel in compressing the full dig/build/creature-wage/wave-defense RTS loop into one cart with the SLAP-HAND mechanic and A* imps that autonomously mine, claim, haul, and fight.",
  "genre": "strategy",
  "homage": "Dungeon Keeper (1997)",
  "description": "Be the evil Keeper: carve a dungeon out of solid rock, claim the floor, and fend off the heroes who march in to smash your Dungeon Heart. LEFT-DRAG over rock to designate it for DIGGING - imps A*-path in, mine it out (lifting the fog and exposing buried gold seams), then auto-CLAIM the dug floor and HAUL gold back to the treasury. Spend that gold on three rooms built by dragging on claimed floor: a TREASURY to store gold and pay wages, a LAIR that attracts monsters (and where they sleep to heal), and a TRAINING room where they level up. Unpaid monsters sour and leave. The signature is the SLAP-HAND: click-hold any imp or monster to pick it up and release to slap it down somewhere - rush a digger to a fresh seam or hurl a fighter onto an invader. Five escalating hero waves tunnel in from the gate toward your Heart; your monsters auto-fight and you slap in reinforcements or hit RALLY to call them all home. Torch-lit pal()/fillp dungeon mood, a creepy minor-organ bed with euclidean pick-axe taps, dig-dust, gold sparkle + rollup, combat sparks and a red heart-alarm pulse. Survive all 5 waves to win; lose the Heart and the do-gooders take the land. MOUSE: left-drag dig / build / grab+slap, buttons 1-3 pick a room, 4 or CREATE IMP makes a digger, SPACE/RALLY summons the monsters, right-click cancels."
}
de:meta */
#include "studio.h"

// ── THE UNDERLORD — a tiny Dungeon Keeper ──────────────────────────────────
// You are the evil Keeper. Carve a dungeon out of solid rock, claim the floor,
// build rooms that house and train your monsters, and repel the do-gooder heroes
// who march in to smash your Dungeon Heart. The SLAP-HAND is the soul of it:
// grab an imp and fling it at a fresh gold seam, or hurl a monster onto an invader.
//
//   LEFT-DRAG over rock     = designate it for DIGGING (imps mine it out)
//   imps auto-CLAIM dug floor and HAUL gold to the treasury
//   click a ROOM button (1-3) then drag on CLAIMED floor to build (costs gold)
//   CLICK-HOLD a creature   = pick it up      RELEASE = slap it down (the Hand)
//   4 / CREATE IMP makes a digger    SPACE / RALLY calls all monsters to the Heart
//   RIGHT-CLICK cancels.  Survive 5 hero waves to win — lose the Heart and it's over.

// ── geometry ───────────────────────────────────────────────────────────────
#define GW    20
#define GH    15
#define TILE  12
#define N     (GW * GH)
#define MAPX  1
#define MAPY  14
#define PANELX 242
#define PW    (SCREEN_W - PANELX)
#define MAXPATH 80

// ── tiles ──────────────────────────────────────────────────────────────────
enum { T_ROCK, T_GOLD, T_FLOOR };          // FLOOR = dug out
enum { R_NONE, R_TREASURY, R_LAIR, R_TRAIN };

typedef struct {
    unsigned char type;     // T_*
    unsigned char room;     // R_* (only on claimed floor)
    bool  claimed;          // floor belongs to the Keeper
    bool  dig;              // marked for digging (rock / gold)
    bool  seen;             // revealed (lifts the fog forever)
    float hard;             // remaining dig hardness
    unsigned char var;      // cosmetic speckle
} Tile;
static Tile grid[N];        // NB: 'grid', not 'map' (map() is an API call)

// ── creatures ──────────────────────────────────────────────────────────────
enum { C_IMP, C_BEAST, C_HERO, C_COUNT };
enum { S_IDLE, S_GO_DIG, S_DIG, S_GO_CLAIM, S_CLAIM, S_HAUL, S_SLEEP, S_TRAIN, S_FIGHT, S_INVADE, S_HELD };

static const float c_speed[C_COUNT] = { 42, 30, 24 };     // px/sec
static const int   c_hp[C_COUNT]    = { 18, 60, 46 };
static const int   c_dmg[C_COUNT]   = { 3, 11, 9 };
static const int   c_aggro[C_COUNT] = { 0, 56, 44 };
static const float c_firecd[C_COUNT]= { 0, 0.65f, 0.8f };
#define MELEE     11
#define MAXLVL    3

typedef struct {
    bool  active; int type; float x, y;
    int   hp, maxhp, state;
    int   path[MAXPATH], plen, pi; float repath, face;
    int   job;            // rock cell being dug / target cell
    float carry;          // gold an imp is hauling
    float work;           // dig / claim / train progress
    int   level; float happy;
    int   target;         // creature index in combat
    float fire_cd, flash;
    int   hue;            // recolor variety
} Cre;

#define MAXC   72
static Cre cre[MAXC];

// ── effects ──────────────────────────────────────────────────────────────────
typedef struct { bool active; float x, y, t, maxt, size; int col; } Boom;
#define MAXBOOM 64
static Boom booms[MAXBOOM];

// ── globals ────────────────────────────────────────────────────────────────
static int   gold;
static float shown_gold;
static int   heart_cell, heart_hp, heart_max;
static float heart_flash, heart_pulse;
static int   over;                 // 0 play, 1 win, 2 lose
static int   placing;              // -1 none, else R_* to build
static bool  dig_drag; static bool dig_set;     // left-drag designating
static int   held;                 // creature index in the Hand, or -1
static float slap_t;               // squash timer for a just-dropped creature
static int   slap_who;
static float rally_t;
static float pay_t, attract_t;
static int   wave, wave_state;     // wave 1..; state 0=prep 1=active 2=won
static float wave_t;
static int   gate_cell;
static int   sound_voice;
#define TOTAL_WAVES 5
enum { W_PREP, W_ACTIVE };

// ── room costs / names ───────────────────────────────────────────────────────
static const int  room_cost[4] = { 0, 15, 25, 35 };
static const char *room_name[4] = { "", "TREASURY", "LAIR", "TRAINING" };
#define IMP_COST   60

// ── pathfinding scratch ──────────────────────────────────────────────────────
static int  g_came[N], g_g[N]; static bool g_open[N], g_closed[N];
static const int DX4[4] = { 0, 1, 0, -1 }, DY4[4] = { -1, 0, 1, 0 };

// ── small helpers ─────────────────────────────────────────────────────────────
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

static int count_room(int r) {
    int n = 0;
    for (int i = 0; i < N; i++) if (grid[i].type == T_FLOOR && grid[i].claimed && grid[i].room == r) n++;
    return n;
}
static int count_cre(int type) {
    int n = 0;
    for (int i = 0; i < MAXC; i++) if (cre[i].active && cre[i].type == type) n++;
    return n;
}
static int gold_cap(void) { return 200 + count_room(R_TREASURY) * 150; }

// ── weighted A* — rock blocks for your units; heroes smash through it ─────────
static int heur(int a, int b) { return abs(a % GW - b % GW) + abs(a / GW - b / GW); }
static int find_path(int s, int gc, int *out, bool hero) {
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
            if (g_closed[ni]) continue;
            int step;
            if (hero) step = is_floor(ni) ? 1 : 6;             // heroes can tunnel
            else { if (!is_floor(ni) && ni != gc) continue; step = 1; }
            int tg = g_g[best] + step;
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
static void set_path(Cre *u, int gc, bool hero) { u->plen = find_path(cell_of(u->x, u->y), gc, u->path, hero); u->pi = 0; }
static bool step_path(Cre *u) {
    if (u->pi >= u->plen) return true;
    int c = u->path[u->pi];
    float tx = pcx(c % GW), ty = pcy(c / GW);
    float a = angle_to((int)u->x, (int)u->y, (int)tx, (int)ty);
    u->face = a;
    float sp = c_speed[u->type] * dt();
    // heroes smash any rock they step into
    if (u->type == C_HERO && grid[c].type != T_FLOOR && near((int)u->x, (int)u->y, (int)tx, (int)ty, TILE)) {
        grid[c].type = T_FLOOR; grid[c].claimed = false; grid[c].room = R_NONE; grid[c].dig = false;
        for (int k = 0; k < 3; k++) {
            for (int b = 0; b < MAXBOOM; b++) if (!booms[b].active) { booms[b] = (Boom){ true, tx + rnd_between(-4, 4), ty + rnd_between(-4, 4), 0, 0.35f, 4, CLR_DARK_GREY }; break; }
        }
        shake(2);
    }
    u->x += dx(sp, a); u->y += dy(sp, a);
    if (near((int)u->x, (int)u->y, (int)tx, (int)ty, 2)) u->pi++;
    return u->pi >= u->plen;
}

// ── effects ──────────────────────────────────────────────────────────────────
static void boom(float x, float y, float size, float maxt, int col) {
    for (int i = 0; i < MAXBOOM; i++)
        if (!booms[i].active) { booms[i] = (Boom){ true, x, y, 0, maxt, size, col }; return; }
}
static void blip(int midi, int instr, int vol, int dur) {
    if ((sound_voice++ & 1) == 0) hit(midi, instr, vol, dur);   // thin the spam a touch
}

// ── reveal fog ────────────────────────────────────────────────────────────────
static void reveal(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        int i = idx(x, y);
        if (grid[i].type == T_FLOOR) { grid[i].seen = true; }
    }
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        int i = idx(x, y);
        if (grid[i].seen) continue;
        for (int d = 0; d < 8 && !grid[i].seen; d++) {
            int nx = x + (d % 3) - 1, ny = y + (d / 3) - 1;
            if (in_grid(nx, ny) && grid[idx(nx, ny)].type == T_FLOOR) grid[i].seen = true;
        }
    }
}

// ── spawning ─────────────────────────────────────────────────────────────────
static int spawn_cre(int type, int cell) {
    for (int i = 0; i < MAXC; i++) if (!cre[i].active) {
        Cre *u = &cre[i];
        *u = (Cre){ 0 };
        u->active = true; u->type = type;
        u->x = pcx(cell % GW); u->y = pcy(cell / GW);
        u->hp = u->maxhp = c_hp[type];
        u->state = S_IDLE; u->job = -1; u->target = -1;
        u->happy = 1.0f; u->level = 0;
        u->hue = rnd(3);
        return i;
    }
    return -1;
}

// ── map gen + reset ───────────────────────────────────────────────────────────
static void carve(int cx, int cy, bool claim) {
    if (!in_grid(cx, cy)) return;
    int i = idx(cx, cy);
    grid[i].type = T_FLOOR; grid[i].claimed = claim; grid[i].room = R_NONE; grid[i].dig = false;
}
static void stamp_gold(int cx, int cy, int rad) {
    for (int dy = -rad; dy <= rad; dy++) for (int dxx = -rad; dxx <= rad; dxx++) {
        int x = cx + dxx, y = cy + dy;
        if (!in_grid(x, y)) continue;
        if (length(dxx, dy) > rad + 0.3f) continue;
        int i = idx(x, y);
        if (grid[i].type == T_ROCK) { grid[i].type = T_GOLD; grid[i].hard = 4.5f; }
    }
}
static void reset_game(void) {
    for (int i = 0; i < MAXC; i++) cre[i].active = false;
    for (int i = 0; i < MAXBOOM; i++) booms[i].active = false;
    for (int i = 0; i < N; i++) {
        grid[i] = (Tile){ 0 };
        grid[i].type = T_ROCK; grid[i].hard = 3.0f;
        grid[i].var = (i * 7 + (i / GW) * 13) % 4;
    }
    // gold seams buried in the rock
    stamp_gold(4, 8, 1); stamp_gold(15, 9, 1); stamp_gold(10, 5, 1);
    stamp_gold(3, 3, 1); stamp_gold(16, 4, 1); stamp_gold(7, 11, 0); stamp_gold(13, 12, 0);

    // the Keeper's heartland — a claimed pocket near the bottom
    heart_cell = idx(10, 12);
    for (int y = 10; y <= 13; y++) for (int x = 8; x <= 12; x++) carve(x, y, true);
    // a starter treasury so the first gold has a home
    grid[idx(8, 10)].room = R_TREASURY; grid[idx(9, 10)].room = R_TREASURY;
    heart_hp = heart_max = 130; heart_flash = 0; heart_pulse = 0;

    // hero gate at the top — pre-dug, the do-gooders pour in here
    gate_cell = idx(10, 0);
    carve(10, 0, false); carve(10, 1, false);

    // a couple of dig designations to get the loop going
    grid[idx(7, 11)].dig = true; grid[idx(6, 11)].dig = true; grid[idx(5, 11)].dig = true;
    grid[idx(12, 11)].dig = true; grid[idx(13, 11)].dig = true;

    gold = 250; shown_gold = 250;
    over = 0; placing = -1; dig_drag = false; held = -1; slap_t = 0; rally_t = 0;
    pay_t = 6; attract_t = 4;
    wave = 1; wave_state = W_PREP; wave_t = 26;
    reveal();

    // starting crew
    spawn_cre(C_IMP, idx(9, 12));
    spawn_cre(C_IMP, idx(11, 12));
    spawn_cre(C_BEAST, idx(10, 13));
}

void init(void) {
    instrument(5, INSTR_NOISE, 1, 70, 0, 60);  instrument_filter(5, FILTER_BAND, 1400, 6);   // pickaxe tap
    instrument(6, INSTR_NOISE, 1, 130, 0, 180); instrument_filter(6, FILTER_LOW, 600, 3);    // combat hit
    instrument(7, INSTR_SINE, 1, 60, 3, 160);                                                // gold chime
    instrument(8, INSTR_TRI, 6, 140, 4, 240);                                                // build sting
    instrument(9, INSTR_SAW, 220, 300, 4, 700); instrument_filter(9, FILTER_LOW, 420, 3);
    instrument_lfo(9, 0, LFO_CUTOFF, 0.3f, 160);                                              // dungeon organ bed
    lfo_shape(9, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)
    instrument(10, INSTR_SAW, 8, 200, 5, 280); instrument_filter(10, FILTER_LOW, 700, 2);    // invasion horn
    instrument(11, INSTR_SQUARE, 1, 110, 0, 120); instrument_duty(11, 0.2f);                 // slap thwack
    instrument(12, INSTR_SQUARE, 2, 60, 5, 120);                                             // heart alarm
    bpm(84);
    reset_game();
}

// ── lookups ──────────────────────────────────────────────────────────────────
static int floor_neighbor(int cell) {
    int cx = cell % GW, cy = cell / GW;
    for (int d = 0; d < 4; d++) {
        int nx = cx + DX4[d], ny = cy + DY4[d];
        if (in_grid(nx, ny) && is_floor(idx(nx, ny))) return idx(nx, ny);
    }
    return -1;
}
static bool has_claimed_neighbor(int cell) {
    int cx = cell % GW, cy = cell / GW;
    for (int d = 0; d < 4; d++) {
        int nx = cx + DX4[d], ny = cy + DY4[d];
        if (in_grid(nx, ny) && grid[idx(nx, ny)].claimed) return true;
    }
    return false;
}
static int nearest_treasury(float x, float y) {
    int sc = cell_of(x, y), best = -1, bd = 99999;
    for (int i = 0; i < N; i++)
        if (grid[i].type == T_FLOOR && grid[i].room == R_TREASURY) {
            int d = heur(i, sc); if (d < bd) { bd = d; best = i; }
        }
    return best;
}
static int nearest_room_tile(int r, float x, float y) {
    int sc = cell_of(x, y), best = -1, bd = 99999;
    for (int i = 0; i < N; i++)
        if (grid[i].type == T_FLOOR && grid[i].room == r) {
            int d = heur(i, sc); if (d < bd) { bd = d; best = i; }
        }
    return best;
}
static int nearest_enemy(int team_type, float x, float y, int maxr) {
    // beasts/imps hunt heroes; heroes hunt beasts+imps
    int best = -1; float bd = maxr;
    for (int i = 0; i < MAXC; i++) if (cre[i].active && cre[i].state != S_HELD) {
        bool foe = (team_type == C_HERO) ? (cre[i].type != C_HERO) : (cre[i].type == C_HERO);
        if (!foe) continue;
        float d = distance((int)x, (int)y, (int)cre[i].x, (int)cre[i].y);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// ── digging: find the next reachable job ─────────────────────────────────────
static int pick_dig(Cre *u) {
    int sc = cell_of(u->x, u->y);
    int order[N], on = 0;
    for (int i = 0; i < N; i++)
        if (grid[i].dig && (grid[i].type == T_ROCK || grid[i].type == T_GOLD) && floor_neighbor(i) >= 0)
            order[on++] = i;
    // insertion-ish: try up to a few nearest until one is reachable
    for (int tries = 0; tries < 5 && on > 0; tries++) {
        int bi = -1, bd = 1 << 30, bk = -1;
        for (int k = 0; k < on; k++) { int d = heur(order[k], sc); if (d < bd) { bd = d; bi = order[k]; bk = k; } }
        if (bi < 0) break;
        int fn = floor_neighbor(bi);
        set_path(u, fn, false);
        if (u->plen > 0 || cell_of(u->x, u->y) == fn) { u->job = bi; return fn; }
        order[bk] = order[--on];     // drop the unreachable one and retry
    }
    return -1;
}
static int pick_claim(Cre *u) {
    int sc = cell_of(u->x, u->y);
    int best = -1, bd = 1 << 30;
    for (int i = 0; i < N; i++)
        if (grid[i].type == T_FLOOR && !grid[i].claimed && has_claimed_neighbor(i)) {
            int d = heur(i, sc); if (d < bd) { bd = d; best = i; }
        }
    return best;
}

// ── damage ────────────────────────────────────────────────────────────────────
static void check_over(void) { if (heart_hp <= 0) over = 2; }
static void hurt_cre(int i, int dmg, int by_type) {
    Cre *u = &cre[i]; u->hp -= dmg; u->flash = 0.12f;
    boom(u->x, u->y, 3, 0.14f, CLR_WHITE);
    if (u->hp <= 0) {
        u->active = false;
        if (held == i) held = -1;
        boom(u->x, u->y, u->type == C_BEAST ? 9 : 6, 0.45f, u->type == C_HERO ? CLR_BLUE : CLR_DARK_RED);
        hit(u->type == C_HERO ? 52 : 40, 6, 4, 180); shake(3);
        if (u->type == C_HERO) { gold = min(gold + 15, gold_cap()); boom(u->x, u->y - 4, 4, 0.4f, CLR_YELLOW); note(72, 7, 2); }
        (void)by_type;
    }
}

// ── creature AI ──────────────────────────────────────────────────────────────
static void update_imp(Cre *u, int self) {
    (void)self;
    switch (u->state) {
    case S_IDLE: {
        if (u->carry >= 1 && nearest_treasury(u->x, u->y) >= 0) { u->state = S_HAUL; u->plen = 0; break; }
        int fn = pick_dig(u);
        if (fn >= 0) { u->state = S_GO_DIG; break; }
        int cc = pick_claim(u);
        if (cc >= 0) { u->job = cc; set_path(u, cc, false); u->state = S_GO_CLAIM; break; }
        // idle wander inside the dungeon
        if (chance(2)) { int t = rnd(N); if (is_floor(t)) { set_path(u, t, false); } }
        step_path(u);
    } break;
    case S_GO_DIG: {
        bool there = step_path(u);
        if (grid[u->job].type != T_ROCK && grid[u->job].type != T_GOLD) { u->state = S_IDLE; break; }
        if (!grid[u->job].dig) { u->state = S_IDLE; break; }
        if (there || near((int)u->x, (int)u->y, pcx(u->job % GW), pcy(u->job / GW), TILE + 2)) u->state = S_DIG;
    } break;
    case S_DIG: {
        int c = u->job;
        if ((grid[c].type != T_ROCK && grid[c].type != T_GOLD) || !grid[c].dig) { u->state = S_IDLE; break; }
        u->face = angle_to((int)u->x, (int)u->y, pcx(c % GW), pcy(c / GW));
        grid[c].hard -= dt();
        if (chance(10)) boom(pcx(c % GW) + rnd_between(-4, 4), pcy(c / GW) + rnd_between(-4, 4), 2, 0.3f, CLR_DARK_GREY);
        if (chance(7)) blip(rnd_between(64, 70), 5, 2, 50);
        if (grid[c].hard <= 0) {
            bool wasgold = grid[c].type == T_GOLD;
            grid[c].type = T_FLOOR; grid[c].claimed = false; grid[c].room = R_NONE; grid[c].dig = false;
            reveal();
            if (wasgold) { u->carry += 70; boom(pcx(c % GW), pcy(c / GW), 5, 0.4f, CLR_YELLOW); note(79, 7, 3); }
            u->state = S_IDLE;
        }
    } break;
    case S_GO_CLAIM: {
        bool there = step_path(u);
        if (grid[u->job].claimed || !is_floor(u->job)) { u->state = S_IDLE; break; }
        if (there || cell_of(u->x, u->y) == u->job) u->state = S_CLAIM;
    } break;
    case S_CLAIM: {
        int c = u->job;
        if (!is_floor(c) || grid[c].claimed) { u->state = S_IDLE; break; }
        u->work += dt();
        if (chance(6)) boom(pcx(c % GW), pcy(c / GW), 2, 0.25f, CLR_MAUVE);
        if (u->work > 0.6f) { grid[c].claimed = true; u->work = 0; reveal(); blip(60, 7, 1, 60); u->state = S_IDLE; }
    } break;
    case S_HAUL: {
        int t = nearest_treasury(u->x, u->y);
        if (t < 0) { u->state = S_IDLE; break; }
        if (u->plen == 0 || u->repath <= 0) { set_path(u, t, false); u->repath = 0.8f; }
        bool there = step_path(u);
        if (there || near((int)u->x, (int)u->y, pcx(t % GW), pcy(t / GW), TILE)) {
            gold = min(gold + (int)u->carry, gold_cap());
            boom(u->x, u->y - 4, 4, 0.4f, CLR_LIGHT_YELLOW); note(76, 7, 2);
            u->carry = 0; u->state = S_IDLE; u->plen = 0;
        }
    } break;
    default: u->state = S_IDLE; break;
    }
}

static void update_beast(Cre *u, int self) {
    int e = nearest_enemy(u->type, u->x, u->y, rally_t > 0 ? 9999 : c_aggro[u->type]);
    if (e >= 0) { u->target = e; u->state = S_FIGHT; }

    switch (u->state) {
    case S_FIGHT: {
        if (u->target < 0 || !cre[u->target].active || cre[u->target].type != C_HERO) { u->target = -1; u->state = S_IDLE; break; }
        Cre *t = &cre[u->target];
        float d = distance((int)u->x, (int)u->y, (int)t->x, (int)t->y);
        if (d <= MELEE) {
            u->face = angle_to((int)u->x, (int)u->y, (int)t->x, (int)t->y);
            if (u->fire_cd <= 0) {
                int dm = c_dmg[u->type] + u->level * 3;
                hurt_cre(u->target, dm, u->type); u->fire_cd = c_firecd[u->type];
                boom((u->x + t->x) / 2, (u->y + t->y) / 2, 4, 0.15f, CLR_ORANGE);
                blip(48, 6, 3, 90);
            }
        } else {
            if (u->pi >= u->plen || u->repath <= 0) { set_path(u, cell_of(t->x, t->y), false); u->repath = 0.4f; }
            step_path(u);
        }
    } break;
    case S_SLEEP: {
        int l = nearest_room_tile(R_LAIR, u->x, u->y);
        if (l < 0) { u->state = S_IDLE; break; }
        if (cell_of(u->x, u->y) == l || near((int)u->x, (int)u->y, pcx(l % GW), pcy(l / GW), TILE)) {
            u->hp = min(u->maxhp, u->hp + (int)(14 * dt()) + (chance(40) ? 1 : 0));
            if (u->hp >= u->maxhp) u->state = S_IDLE;
        } else { if (u->plen == 0) set_path(u, l, false); step_path(u); }
    } break;
    case S_TRAIN: {
        int tr = nearest_room_tile(R_TRAIN, u->x, u->y);
        if (tr < 0 || u->level >= MAXLVL) { u->state = S_IDLE; break; }
        if (cell_of(u->x, u->y) == tr || near((int)u->x, (int)u->y, pcx(tr % GW), pcy(tr / GW), TILE)) {
            u->work += dt();
            if (chance(8)) boom(u->x + rnd_between(-5, 5), u->y - 6, 2, 0.25f, CLR_LIGHT_GREY);
            if (u->work > 5.0f) { u->level++; u->maxhp += 18; u->hp = u->maxhp; u->work = 0; boom(u->x, u->y - 6, 7, 0.5f, CLR_LIME_GREEN); strum(60, CHORD_MAJ, 8, 2, 30); u->state = S_IDLE; }
        } else { if (u->plen == 0) set_path(u, tr, false); step_path(u); }
    } break;
    default: { // IDLE
        if (rally_t > 0) { if (u->plen == 0 || u->repath <= 0) { set_path(u, heart_cell, false); u->repath = 0.6f; } step_path(u); break; }
        if (u->hp < u->maxhp * 6 / 10 && count_room(R_LAIR) > 0) { u->state = S_SLEEP; u->plen = 0; break; }
        if (u->level < MAXLVL && count_room(R_TRAIN) > 0 && chance(2)) { u->state = S_TRAIN; u->plen = 0; break; }
        if (chance(2)) { int t = nearest_room_tile(R_LAIR, u->x, u->y); if (t < 0) t = rnd(N); if (is_floor(t)) set_path(u, t, false); }
        step_path(u);
    } break;
    }
    (void)self;
}

static void update_hero(Cre *u, int self) {
    (void)self;
    int e = nearest_enemy(u->type, u->x, u->y, c_aggro[u->type]);
    // attack the heart if right next to it
    float hd = distance((int)u->x, (int)u->y, pcx(heart_cell % GW), pcy(heart_cell / GW));
    if (hd <= MELEE + 4) {
        u->face = angle_to((int)u->x, (int)u->y, pcx(heart_cell % GW), pcy(heart_cell / GW));
        if (u->fire_cd <= 0) {
            heart_hp -= 4; heart_flash = 0.5f; u->fire_cd = 0.9f;
            boom(u->x, u->y, 5, 0.2f, CLR_RED); shake(4); note(40, 12, 4); check_over();
        }
        return;
    }
    if (e >= 0) {
        Cre *t = &cre[e];
        float d = distance((int)u->x, (int)u->y, (int)t->x, (int)t->y);
        if (d <= MELEE) {
            u->face = angle_to((int)u->x, (int)u->y, (int)t->x, (int)t->y);
            if (u->fire_cd <= 0) { hurt_cre(e, c_dmg[u->type], u->type); u->fire_cd = c_firecd[u->type]; boom((u->x + t->x) / 2, (u->y + t->y) / 2, 4, 0.15f, CLR_LIGHT_GREY); blip(50, 6, 3, 90); }
            return;
        }
        if (d < c_aggro[u->type]) { if (u->pi >= u->plen || u->repath <= 0) { set_path(u, cell_of(t->x, t->y), false); u->repath = 0.4f; } step_path(u); return; }
    }
    // march on the heart (tunnelling through rock as needed)
    if (u->pi >= u->plen || u->repath <= 0) { set_path(u, heart_cell, true); u->repath = 0.9f; }
    step_path(u);
}

static void update_cre(int i) {
    Cre *u = &cre[i];
    float D = dt();
    if (u->flash > 0) u->flash -= D;
    if (u->fire_cd > 0) u->fire_cd -= D;
    if (u->repath > 0) u->repath -= D;
    if (u->state == S_HELD) { u->x = mouse_x(); u->y = mouse_y(); return; }
    if (u->type == C_IMP)        update_imp(u, i);
    else if (u->type == C_BEAST) update_beast(u, i);
    else                         update_hero(u, i);
}

// ── waves ──────────────────────────────────────────────────────────────────
static void spawn_wave(void) {
    int n = 1 + wave;                       // grows each wave
    note(43, 10, 5); note(50, 10, 4); shake(4);
    for (int k = 0; k < n; k++) {
        int gx = (gate_cell % GW) + rnd_between(-1, 2);
        int cell = in_grid(gx, 0) ? idx(gx, 0) : gate_cell;
        if (!is_floor(cell)) cell = gate_cell;
        int hi = spawn_cre(C_HERO, cell);
        if (hi >= 0) { cre[hi].hp = cre[hi].maxhp = c_hp[C_HERO] + wave * 6; cre[hi].x += rnd_between(-3, 3); }
    }
}

// ── the Hand / input ─────────────────────────────────────────────────────────
static int my_cre_at_px(int x, int y) {
    int best = -1; float bd = 8;
    for (int i = 0; i < MAXC; i++)
        if (cre[i].active && cre[i].type != C_HERO && cre[i].state != S_HELD) {
            float d = distance(x, y, (int)cre[i].x, (int)cre[i].y);
            if (d < bd) { bd = d; best = i; }
        }
    return best;
}
static bool in_map(int x, int y) { return x >= MAPX && y >= MAPY && x < MAPX + GW * TILE && y < MAPY + GH * TILE; }

static void try_place_room(int cx, int cy) {
    if (!in_grid(cx, cy) || placing <= 0) return;
    int c = idx(cx, cy);
    if (grid[c].type != T_FLOOR || !grid[c].claimed) return;
    if (grid[c].room == placing) return;
    if (gold < room_cost[placing]) return;
    if (c == heart_cell) return;
    gold -= room_cost[placing]; grid[c].room = placing;
    boom(pcx(cx), pcy(cy), 5, 0.3f, CLR_LIME_GREEN); blip(67, 8, 2, 120);
}

static void drop_held(void) {
    if (held < 0) return;
    Cre *u = &cre[held];
    int c = cell_of(u->x, u->y);
    if (!is_floor(c)) {           // nudge to a nearby floor cell
        int fn = floor_neighbor(c); if (fn >= 0) c = fn; else c = heart_cell;
        u->x = pcx(c % GW); u->y = pcy(c / GW);
    }
    u->state = S_IDLE; u->plen = 0; u->repath = 0;
    slap_t = 0.25f; slap_who = held;
    boom(u->x, u->y + 3, 7, 0.3f, CLR_LIGHT_GREY); shake(2); note(36, 11, 4);
    held = -1;
}

static void rally(void) {
    if (rally_t > 0) return;
    rally_t = 5; note(45, 10, 4); strum(48, CHORD_MIN, 10, 3, 40); shake(2);
}

// ── sidebar ───────────────────────────────────────────────────────────────────
static bool clicked_rect(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) && mouse_x() >= x && mouse_x() < x + w && mouse_y() >= y && mouse_y() < y + h;
}
static bool button(int x, int y, int w, int h, const char *label, const char *cost, int accent, bool enabled, bool hot) {
    int bg = enabled ? (hot ? CLR_DARK_PURPLE : CLR_DARKER_GREY) : CLR_BROWNISH_BLACK;
    rectfill(x, y, w, h, bg);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_DARK_GREY);
    rectfill(x + 2, y + 2, 3, h - 4, accent);
    print(label, x + 7, y + 2, enabled ? CLR_WHITE : CLR_DARK_GREY);
    if (cost) print_right(cost, x + w - 3, y + 2, enabled ? CLR_YELLOW : CLR_DARK_GREY);
    return enabled && clicked_rect(x, y, w, h);
}

static const int SBY = 40;
// one function for both the click-logic (act=true, in update) and the visuals
// (act=false, in draw) so a click is never processed twice in a frame.
static void sidebar(bool act) {
    int x = PANELX + 2, w = PW - 4, y = SBY;
    for (int r = R_TREASURY; r <= R_TRAIN; r++) {
        bool en = gold >= room_cost[r];
        int ac = r == R_TREASURY ? CLR_YELLOW : r == R_LAIR ? CLR_DARK_RED : CLR_LIME_GREEN;
        bool hit_b = button(x, y, w, 13, room_name[r], str("%d", room_cost[r]), ac, en, placing == r);
        if (act && hit_b) placing = (placing == r) ? -1 : r;
        y += 14;
    }
    y += 3;
    if (button(x, y, w, 13, "CREATE IMP", str("%d", IMP_COST), CLR_DARK_RED, gold >= IMP_COST, false) && act) {
        gold -= IMP_COST; spawn_cre(C_IMP, heart_cell); boom(pcx(heart_cell % GW), pcy(heart_cell / GW), 6, 0.4f, CLR_RED); note(60, 8, 3);
    }
    y += 14;
    if (button(x, y, w, 13, "RALLY!", rally_t > 0 ? "ON" : "Z", CLR_ORANGE, rally_t <= 0, rally_t > 0) && act) rally();
}

// ── update ─────────────────────────────────────────────────────────────────
void update(void) {
    if (over) { if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A) || keyp(KEY_SPACE)) reset_game(); return; }
    float D = dt();
    if (heart_flash > 0) heart_flash -= D;
    if (slap_t > 0) slap_t -= D;
    if (rally_t > 0) rally_t -= D;
    heart_pulse += D;

    // hotkeys
    for (int r = R_TREASURY; r <= R_TRAIN; r++) if (keyp('0' + r)) placing = (placing == r) ? -1 : r;
    if (keyp('4') && gold >= IMP_COST) { gold -= IMP_COST; spawn_cre(C_IMP, heart_cell); note(60, 8, 3); }
    if (keyp(KEY_SPACE) || keyp('c') || keyp('C')) rally();
    if (mouse_pressed(MOUSE_RIGHT) || keyp(KEY_ESCAPE)) { placing = -1; if (held >= 0) drop_held(); }

    int mx = mouse_x(), my = mouse_y();
    int cx = (mx - MAPX) / TILE, cy = (my - MAPY) / TILE;

    // mouse press: place room / pick up creature / start a dig-drag
    if (mouse_pressed(MOUSE_LEFT) && in_map(mx, my) && held < 0) {
        if (placing > 0) { try_place_room(cx, cy); }
        else {
            int ci = my_cre_at_px(mx, my);
            if (ci >= 0) { held = ci; cre[ci].state = S_HELD; note(72, 11, 2); }
            else if (in_grid(cx, cy)) {
                int c = idx(cx, cy);
                if ((grid[c].type == T_ROCK || grid[c].type == T_GOLD) && grid[c].seen) {
                    dig_drag = true; dig_set = !grid[c].dig; grid[c].dig = dig_set; blip(64, 5, 1, 40);
                }
            }
        }
    }
    // mouse held: continue drag (paint dig marks or room tiles)
    if (mouse_down(MOUSE_LEFT) && in_map(mx, my) && in_grid(cx, cy)) {
        int c = idx(cx, cy);
        if (dig_drag) { if ((grid[c].type == T_ROCK || grid[c].type == T_GOLD) && grid[c].seen) grid[c].dig = dig_set; }
        else if (placing > 0 && held < 0) try_place_room(cx, cy);
    }
    if (mouse_released(MOUSE_LEFT)) { dig_drag = false; if (held >= 0) drop_held(); }

    sidebar(true);

    // economy: pay the monsters from the treasury, or they sour and leave
    pay_t -= D;
    if (pay_t <= 0) {
        pay_t = 6;
        int beasts = count_cre(C_BEAST);
        int wage = beasts * 4;
        if (gold >= wage) { gold -= wage; for (int i = 0; i < MAXC; i++) if (cre[i].active && cre[i].type == C_BEAST) cre[i].happy = clamp(cre[i].happy + 0.25f, 0, 1); }
        else for (int i = 0; i < MAXC; i++) if (cre[i].active && cre[i].type == C_BEAST) {
            cre[i].happy -= 0.34f;
            if (cre[i].happy <= 0) { boom(cre[i].x, cre[i].y, 6, 0.5f, CLR_INDIGO); note(40, 8, 2); cre[i].active = false; }
        }
    }
    // lair attracts new monsters up to its capacity
    attract_t -= D;
    if (attract_t <= 0) {
        attract_t = 5;
        int cap = count_room(R_LAIR);
        if (count_cre(C_BEAST) < cap && gold > 0) {
            int b = spawn_cre(C_BEAST, heart_cell);
            if (b >= 0) { boom(pcx(heart_cell % GW), pcy(heart_cell / GW), 7, 0.5f, CLR_DARK_PURPLE); strum(48, CHORD_MIN, 9, 3, 50); }
        }
    }

    // waves
    int heroes = count_cre(C_HERO);
    if (wave_state == W_PREP) {
        wave_t -= D;
        if (wave_t <= 0) { spawn_wave(); wave_state = W_ACTIVE; }
    } else if (wave_state == W_ACTIVE && heroes == 0) {
        if (wave >= TOTAL_WAVES) { over = 1; }
        else { wave++; wave_state = W_PREP; wave_t = 22; }
    }

    for (int i = 0; i < MAXC; i++) if (cre[i].active) update_cre(i);
    for (int i = 0; i < MAXBOOM; i++) if (booms[i].active) { booms[i].t += D; if (booms[i].t >= booms[i].maxt) booms[i].active = false; }

    // sound bed: a slow minor dungeon organ + a pick-axe pulse while digging
    if (every(2)) { int root = 36 + (beat() / 8 % 2) * 3; chord(root, CHORD_MIN, 9, 2); }
    bool digging = false;
    for (int i = 0; i < MAXC; i++) if (cre[i].active && cre[i].type == C_IMP && cre[i].state == S_DIG) digging = true;
    if (digging && euclid(5, 8, beat())) hit(rnd_between(60, 66), 5, 1, 40);
    if (heart_hp < heart_max / 4 && every(1)) note(48, 12, 2);   // heart-in-danger alarm

    shown_gold = lerp(shown_gold, gold, clamp(7 * D, 0, 1));
}

// ── drawing ─────────────────────────────────────────────────────────────────
static void draw_tile(int x, int y) {
    int i = idx(x, y), px = MAPX + x * TILE, py = MAPY + y * TILE;
    Tile *t = &grid[i];
    if (!t->seen) { rectfill(px, py, TILE, TILE, CLR_BLACK); return; }

    if (t->type == T_ROCK) {
        rectfill(px, py, TILE, TILE, CLR_DARKER_GREY);
        pset(px + 2 + t->var, py + 3, CLR_DARKER_PURPLE);
        pset(px + 7, py + 8 - t->var, CLR_DARK_GREY);
    } else if (t->type == T_GOLD) {
        rectfill(px, py, TILE, TILE, CLR_DARK_GREY);
        for (int g = 0; g < 4; g++) pset(px + 2 + (g * 3 + t->var) % (TILE - 3), py + 2 + (g * 5) % (TILE - 3), (frame() / 6 + g) % 3 == 0 ? CLR_YELLOW : CLR_ORANGE);
    } else {
        // dug floor — claimed glows warm (torch-lit), unclaimed stays earthy
        if (t->claimed) {
            rectfill(px, py, TILE, TILE, CLR_DARKER_PURPLE);
            fillp(FILL_CHECKER, -1); rectfill(px, py, TILE, TILE, CLR_DARKER_GREY); fillp_reset();
            if ((i * 5 + frame() / 9) % 23 == 0) pset(px + 3 + t->var, py + 4, CLR_DARK_ORANGE);   // ember
        } else {
            rectfill(px, py, TILE, TILE, CLR_BROWNISH_BLACK);
            pset(px + 3 + t->var, py + 6, CLR_DARK_BROWN);
        }
        // rooms
        int cxp = px + TILE / 2, cyp = py + TILE / 2;
        if (t->room == R_TREASURY) {
            circfill(cxp, cyp + 2, 3, CLR_DARK_ORANGE);
            pset(cxp - 1, cyp, CLR_YELLOW); pset(cxp + 2, cyp + 1, CLR_LIGHT_YELLOW); pset(cxp, cyp + 3, CLR_YELLOW);
        } else if (t->room == R_LAIR) {
            ovalfill(cxp, cyp + 2, 4, 2, CLR_DARK_BROWN);
            ovalfill(cxp, cyp + 1, 2, 1, CLR_DARK_RED);
        } else if (t->room == R_TRAIN) {
            rectfill(cxp - 1, cyp - 3, 2, 7, CLR_BROWN);
            circ(cxp, cyp - 3, 2, CLR_LIGHT_GREY);
        }
    }
    // dig designation overlay
    if (t->dig) {
        fillp(FILL_CHECKER, -1); rectfill(px, py, TILE, TILE, CLR_YELLOW); fillp_reset();
        rect(px, py, TILE, TILE, CLR_DARK_ORANGE);
    }
}

static void draw_heart(void) {
    int x = pcx(heart_cell % GW), y = pcy(heart_cell / GW);
    float p = 0.5f + 0.5f * sin_deg(heart_pulse * 220);
    int r = 4 + (int)(p * 1.5f);
    int col = heart_flash > 0 && blink(2) ? CLR_WHITE : (heart_hp < heart_max / 4 && blink(6) ? CLR_RED : CLR_DARK_RED);
    circfill(x, y, r + 2, CLR_DARKER_PURPLE);
    circfill(x, y, r, col);
    pset(x - 1, y - 1, CLR_PINK);
    circ(x, y, r + 3 + (int)(p * 2), CLR_RED);
}

static void draw_cre(int i) {
    Cre *u = &cre[i];
    int x = (int)u->x, y = (int)u->y;
    bool held_here = (u->state == S_HELD);
    int sq = (slap_who == i && slap_t > 0) ? 2 : 0;       // squash on landing
    int fl = u->flash > 0;

    if (u->type == C_IMP) {
        int body = fl ? CLR_WHITE : CLR_DARK_RED;
        ovalfill(x, y + 1 + sq, 3, 3 - sq, body);
        pset(x - 1, y - 1, CLR_YELLOW); pset(x + 1, y - 1, CLR_YELLOW);   // eyes
        if (u->carry >= 1) pset(x, y - 4, CLR_YELLOW);
    } else if (u->type == C_BEAST) {
        int body = fl ? CLR_WHITE : (u->hue == 0 ? CLR_DARK_PURPLE : u->hue == 1 ? CLR_MAUVE : CLR_DARK_RED);
        ovalfill(x, y + sq, 4, 4 - sq, body);
        line(x - 3, y - 3, x - 5, y - 6, CLR_LIGHT_GREY);                  // horns
        line(x + 3, y - 3, x + 5, y - 6, CLR_LIGHT_GREY);
        pset(x - 1, y - 1, CLR_RED); pset(x + 2, y - 1, CLR_RED);
        for (int l = 0; l < u->level; l++) pset(x - 2 + l * 2, y - 6, CLR_LIME_GREEN);   // rank pips
        if (u->hp < u->maxhp) bar(x - 5, y - 8, 10, 2, (float)u->hp / u->maxhp, CLR_GREEN, CLR_DARK_RED);
    } else { // hero
        int body = fl ? CLR_WHITE : CLR_LIGHT_GREY;
        rectfill(x - 2, y - 1, 4, 5, body);
        circfill(x, y - 3, 2, CLR_LIGHT_PEACH);
        line(x + 3, y - 4, x + 3, y + 2, CLR_YELLOW);                       // sword
        if (u->hp < u->maxhp) bar(x - 5, y - 8, 10, 2, (float)u->hp / u->maxhp, CLR_RED, CLR_DARKER_GREY);
    }
    if (held_here) {   // glow when carried by the Hand
        circ(x, y, 7, blink(4) ? CLR_LIGHT_YELLOW : CLR_YELLOW);
    }
}

static void draw_hand(void) {
    int x = mouse_x(), y = mouse_y();
    bool closed = held >= 0 || mouse_down(MOUSE_LEFT);
    int pal_c = CLR_LIGHT_PEACH;
    // palm
    circfill(x, y + 4, 3, pal_c);
    circ(x, y + 4, 3, CLR_BROWNISH_BLACK);
    // fingers — splayed when open, curled when grabbing
    int spread = closed ? 1 : 3;
    for (int f = -1; f <= 1; f++) {
        line(x + f * spread, y + 2, x + f * spread * 2, y - (closed ? 1 : 4), pal_c);
    }
    line(x - 3, y + 4, x - 5, y + (closed ? 4 : 2), pal_c);   // thumb
    pset(x, y + 4, CLR_PEACH);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) draw_tile(x, y);
    draw_heart();

    // depth-ish: draw creatures in row order
    for (int y = 0; y < GH; y++) {
        int lo = MAPY + y * TILE, hi = lo + TILE;
        for (int i = 0; i < MAXC; i++) if (cre[i].active && cre[i].state != S_HELD && (int)cre[i].y >= lo && (int)cre[i].y < hi) draw_cre(i);
    }
    // effects
    for (int i = 0; i < MAXBOOM; i++) if (booms[i].active) {
        Boom *b = &booms[i];
        float t = clamp(b->t / b->maxt, 0, 1);
        int r = (int)(b->size * ease_out(t));
        fillp(FILL_CHECKER, -1); circfill((int)b->x, (int)b->y, r, b->col); fillp_reset();
    }
    // held creature + hand drawn on top
    if (held >= 0) draw_cre(held);

    // hero-gate marker
    {
        int gx = pcx(gate_cell % GW), gy = pcy(gate_cell / GW);
        if (grid[gate_cell].seen) { rect(gx - 5, gy - 5, 10, 10, blink(20) ? CLR_BLUE : CLR_TRUE_BLUE); print("IN", gx - 7, gy - 12, CLR_BLUE); }
    }
    // dig-mode crosshair / placement ghost
    if (in_map(mouse_x(), mouse_y())) {
        int cx = (mouse_x() - MAPX) / TILE, cy = (mouse_y() - MAPY) / TILE;
        if (in_grid(cx, cy) && placing > 0) {
            int c = idx(cx, cy);
            bool ok = grid[c].type == T_FLOOR && grid[c].claimed && c != heart_cell && gold >= room_cost[placing];
            fillp(FILL_CHECKER, -1); rectfill(MAPX + cx * TILE, MAPY + cy * TILE, TILE, TILE, ok ? CLR_LIME_GREEN : CLR_RED); fillp_reset();
        }
    }
    draw_hand();

    // heart-under-attack red wash
    if (heart_flash > 0 && blink(3)) { fillp(FILL_CHECKER, -1); rectfill(0, 0, MAPX + GW * TILE, SCREEN_H, CLR_DARK_RED); fillp_reset(); }

    // ── top HUD ──
    rectfill(0, 0, PANELX, MAPY, CLR_BLACK);
    print(str("GOLD %d", (int)shown_gold), 3, 3, CLR_YELLOW);
    print_right(str("/%d", gold_cap()), 118, 3, CLR_DARK_ORANGE);
    print(str("MONSTERS %d/%d", count_cre(C_BEAST), count_room(R_LAIR)), 124, 3, CLR_MAUVE);

    // ── sidebar ──
    rectfill(PANELX, 0, PW, SCREEN_H, CLR_BROWNISH_BLACK);
    line(PANELX, 0, PANELX, SCREEN_H, CLR_DARK_GREY);
    print("THE", PANELX + 4, 3, CLR_DARK_RED);
    print("UNDERLORD", PANELX + 4, 12, CLR_RED);
    print("BUILD", PANELX + 4, 30, CLR_LIGHT_GREY);
    sidebar(false);

    // heart hp + wave info at the bottom of the panel
    int y = 122;
    print("HEART", PANELX + 4, y, CLR_PINK);
    bar(PANELX + 4, y + 9, PW - 8, 4, (float)heart_hp / heart_max, heart_hp < heart_max / 4 ? CLR_RED : CLR_DARK_RED, CLR_DARKER_GREY);
    y = 146;
    if (wave_state == W_PREP) {
        print(str("WAVE %d/%d", wave, TOTAL_WAVES), PANELX + 4, y, CLR_LIGHT_GREY);
        print(str("in %ds", (int)wave_t + 1), PANELX + 4, y + 9, blink(20) ? CLR_ORANGE : CLR_DARK_ORANGE);
    } else {
        print(str("WAVE %d/%d", wave, TOTAL_WAVES), PANELX + 4, y, CLR_RED);
        print(str("%d heroes!", count_cre(C_HERO)), PANELX + 4, y + 9, blink(12) ? CLR_RED : CLR_DARK_RED);
    }
    // contextual hint
    int hy = 172;
    if (held >= 0)        { print("SLAP it down", PANELX + 4, hy, CLR_LIME_GREEN); print("to relocate", PANELX + 4, hy + 9, CLR_LIGHT_GREY); }
    else if (placing > 0) { print(str("PLACE %s", room_name[placing]), PANELX + 4, hy, CLR_LIME_GREEN); print("on claimed", PANELX + 4, hy + 9, CLR_LIGHT_GREY); }
    else                  { print("drag rock to", PANELX + 4, hy, CLR_DARK_GREY); print("DIG it out", PANELX + 4, hy + 9, CLR_DARK_GREY); }

    // ── end screens ──
    if (over) {
        rectfill(SCREEN_W / 2 - 84, SCREEN_H / 2 - 26, 168, 52, CLR_BLACK);
        rect(SCREEN_W / 2 - 84, SCREEN_H / 2 - 26, 168, 52, over == 1 ? CLR_LIME_GREEN : CLR_RED);
        print_centered(over == 1 ? "THE LAND IS YOURS" : "THE HEART IS BROKEN", SCREEN_W/2, SCREEN_H / 2 - 16, over == 1 ? CLR_LIME_GREEN : CLR_RED);
        print_centered(over == 1 ? "all 5 waves repelled" : "the heroes have won", SCREEN_W/2, SCREEN_H / 2 - 3, CLR_YELLOW);
        print_centered("click for a new dungeon", SCREEN_W/2, SCREEN_H / 2 + 11, CLR_LIGHT_GREY);
    }
}
