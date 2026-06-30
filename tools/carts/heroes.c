/* de:meta
{
  "title": "heroes of might & magic",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "pathfinding",
    "turn-based-loop",
    "grid-movement",
    "tilemap-collision"
  ],
  "lineage": "A demake of Heroes of Might & Magic (3DO, 1995); the three-mode loop (adventure → town → battle) is faithful to the original's structure, compressed to a 20×10 grid.",
  "genre": "strategy",
  "homage": "Heroes of Might and Magic (1995)",
  "description": "A pocket Heroes of Might & Magic with the whole loop: explore, recruit, build, then DO BATTLE. On the ADVENTURE map your hero spends MOVEMENT POINTS to roam a small continent (terrain costs MP) - click a tile to walk there, scooping gold, ore and treasure chests, flagging the ore MINE for daily income, ducking into YOUR TOWN, or charging the rival. END DAY refreshes movement and advances the enemy hero, who marches on your town - so build an army before he arrives. In TOWN you build up to three creature dwellings (gold + ore) and recruit Pikemen, Archers, Griffins and Ogres into your army, with a fresh batch each new week. The climax is the STACK BATTLE: a unit is one icon labelled with a count (x22) and damage removes whole creatures from it; initiative runs by speed, melee attacks draw RETALIATION, archers shoot at range with no counter, and the hero casts BOLT (damage) or BLESS (buff) - a live damage forecast shows before you commit, with lunges, count pops, sparks and screen-shake. Win the battle or seize the enemy town to win the war; lose your army or your town and it's over. Mouse-primary: click to move/attack, click buttons to build/recruit/cast. Keys: SPACE end day, B leave town, Z confirm.",
  "todo": [
    "ui-audit: \"army 12\" overlaps \"END DAY\"; the \"click a tile to move\" hint overlaps the \"road\" label."
  ]
}
de:meta */
#include "studio.h"
#include "cursor.h"

// ── HEROES OF MIGHT & MAGIC — a tiny HoMM demake ──────────────────────────────
// Three modes, one loop:  explore → recruit → build → DO BATTLE.
//
//  ADVENTURE  one hero with MOVEMENT POINTS roams a small continent. Click a tile
//             to walk there (terrain costs MP), scoop gold/ore/treasure, flag the
//             mine for daily ore, duck into YOUR TOWN, or charge the enemy hero.
//             END DAY refreshes movement and advances the rival, who marches on
//             your town — so build an army before he arrives.
//  TOWN       build up to three creature dwellings (gold + ore) and recruit
//             creatures into your army. A new week grows each dwelling's pool.
//  BATTLE     the climax. Your STACKS vs theirs on a small grid. A stack is one
//             icon with a count ("x22"); damage removes whole creatures from it.
//             Initiative by speed, melee RETALIATION, ranged archers (no counter),
//             plus two hero spells (BOLT / BLESS). Win the battle = win the war.
//
// Mouse-primary. Keyboard fallback: SPACE = end day, B = leave town, Z = confirm.

// ── creatures ─────────────────────────────────────────────────────────────────
enum { C_PIKE, C_ARCHER, C_GRIFFIN, C_OGRE, NCREAT };
typedef struct {
    int hp, atk, def, dlo, dhi, speed, shots, sprite;
    int gcost, ocost;          // recruit cost (gold, ore) per creature
    const char *name;
} CDef;
static const CDef CD[NCREAT] = {
    //  hp atk def dlo dhi spd sht spr  gold ore  name
    {   4,  4,  3,  1,  3,  4,  0, 16,   60,  0, "PIKEMAN"  },
    {   6,  6,  3,  2,  3,  4, 12, 17,  120,  0, "ARCHER"   },
    {  14,  9,  7,  3,  6,  7,  0, 18,  280,  1, "GRIFFIN"  },
    {  32, 13, 12,  6, 12,  5,  0, 19,  600,  2, "OGRE"     },
};
static const int DWELL_G[NCREAT] = {   0, 200, 500, 1100 };  // dwelling build cost
static const int DWELL_O[NCREAT] = {   0,   0,   2,    5 };
static const int GROWTH [NCREAT] = {   9,   6,   3,    1 };  // weekly population gain

// ── modes ───────────────────────────────────────────────────────────────────
enum { M_ADV, M_TOWN, M_BATTLE, M_OVER };
static int mode = M_ADV;
static int win_state = -1;     // 0 = player victory, 1 = defeat (in M_OVER)
static const char *over_reason = "";

// ── adventure map ─────────────────────────────────────────────────────────────
#define COLS 20
#define ROWS 10
#define TILE 16
#define HUD_Y 24
#define MAP_Y HUD_Y
#define INFO_Y (MAP_Y + ROWS*TILE)   // 184

enum { TT_GRASS, TT_FOREST, TT_MTN, TT_WATER, TT_ROAD, NTERR };
static const int TERR_SLOT[NTERR] = { 1, 2, 3, 4, 5 };
static const int MOVE_COST[NTERR] = { 1, 2, 99, 99, 1 };   // 99 = impassable
static const char *TERR_NAME[NTERR] = { "grass", "forest", "mountain", "water", "road" };

static int terr[ROWS][COLS];

// map objects
enum { O_GOLD, O_ORE, O_CHEST, O_MINE, O_TOWN_P, O_TOWN_E };
typedef struct { int type, x, y, taken, owner; } Obj;  // owner: -1 neutral / 0 you / 1 cpu
#define MAXOBJ 16
static Obj objs[MAXOBJ];
static int nobj;

// heroes
typedef struct { int x, y, alive; } MapHero;
static MapHero phero, ehero;
static int p_gold, p_ore, p_mp, p_maxmp = 8;
static int p_army[NCREAT];        // your travelling army
static int e_army[NCREAT];        // the rival's army
static int day = 1, wins;

// town
static int built[NCREAT];         // dwelling built?
static int avail[NCREAT];         // creatures available to recruit

// adventure interaction
static int sel_path_x[64], sel_path_y[64], path_len, path_step;
static float walk_t;              // per-step walk timer
static int walk_engage;           // 1 = battle the rival when the walk finishes
static int cursor_cx = 3, cursor_cy = 8;
static char msg[40]; static int msg_t;
static float float_gold, float_ore;
static int lastmx = -1, lastmy = -1;

// ── tiny effect pools (shared by adventure + battle) ──────────────────────────
typedef struct { float x, y, vx, vy; int life, col; } Spark;
typedef struct { float x, y; int life, col; char txt[10]; } Pop;
static Spark sparks[64];
static Pop   pops[12];
static int   flash;               // white screen-flash frames

static void add_spark(float x, float y, int col) {
    for (int i = 0; i < 64; i++) if (sparks[i].life <= 0) {
        float a = rnd(360), s = rnd_float_between(0.5f, 2.4f);
        sparks[i] = (Spark){ x, y, dx(s, a), dy(s, a) - 0.5f, rnd_between(16, 30), col };
        return;
    }
}
static void add_pop(float x, float y, int col, const char *t) {
    for (int i = 0; i < 12; i++) if (pops[i].life <= 0) {
        pops[i].x = x; pops[i].y = y; pops[i].life = 40; pops[i].col = col;
        int k = 0; while (t[k] && k < 9) { pops[i].txt[k] = t[k]; k++; } pops[i].txt[k] = 0;
        return;
    }
}
static void set_msg(const char *s) { int k = 0; while (s[k] && k < 38) { msg[k] = s[k]; k++; } msg[k] = 0; }
static void age_effects(void) {
    for (int i = 0; i < 64; i++) if (sparks[i].life > 0) {
        sparks[i].x += sparks[i].vx; sparks[i].y += sparks[i].vy; sparks[i].vy += 0.12f; sparks[i].life--;
    }
    for (int i = 0; i < 12; i++) if (pops[i].life > 0) { pops[i].y -= 0.5f; pops[i].life--; }
    if (flash > 0) flash--;
}

// ── owner recolor (the crowd-cart magic-color trick) ──────────────────────────
static void owner_pal(int owner) {
    if (owner == 0)      { pal(28, CLR_BLUE);       pal(29, CLR_TRUE_BLUE); }
    else if (owner == 1) { pal(28, CLR_RED);        pal(29, CLR_DARK_RED);  }
    else                 { pal(28, CLR_LIGHT_GREY); pal(29, CLR_DARK_GREY); }
}

// ── sound ─────────────────────────────────────────────────────────────────────
static void snd_step(void)   { note(72, 8, 1); }
static void snd_coin(void)   { note(84, 9, 3); schedule(70, 88, 9, 3); }
static void snd_ore(void)    { note(60, 9, 3); }
static void snd_flag(void)   { strum(60, CHORD_MAJ, 5, 3, 50); }
static void snd_build(void)  { strum(55, CHORD_MAJ7, 5, 4, 70); }
static void snd_recruit(void){ note(76, 5, 3); schedule(60, 79, 5, 3); }
static void snd_clash(void)  { note(40, 7, 4); shake(3.0f); }
static void snd_arrow(void)  { note(66, 8, 2); }
static void snd_bolt(void)   { note(50, 1, 5); shake(4.0f); }
static void snd_bless(void)  { strum(67, CHORD_MAJ, 5, 4, 40); }
static void snd_day(void)    { chord(48, CHORD_SUS4, 4, 2); }
static void snd_fanfare(int win) {
    if (win) { schedule(0, 60, 2, 6); schedule(150, 64, 2, 6); schedule(300, 67, 2, 6); schedule(450, 72, 2, 7); }
    else     { schedule(0, 55, 1, 5); schedule(200, 51, 1, 5); schedule(400, 46, 1, 5); }
}

// ── adventure helpers ─────────────────────────────────────────────────────────
static int scx(int x) { return x * TILE; }
static int scy(int y) { return MAP_Y + y * TILE; }
static bool in_grid(int x, int y) { return x >= 0 && y >= 0 && x < COLS && y < ROWS; }
static bool passable(int x, int y) { return in_grid(x, y) && MOVE_COST[terr[y][x]] < 99; }
static int obj_at(int x, int y) {
    for (int i = 0; i < nobj; i++) if (objs[i].x == x && objs[i].y == y && !objs[i].taken) return i;
    return -1;
}
static bool blocked(int x, int y) {   // can the hero's path NOT pass through here?
    if (!passable(x, y)) return true;
    if (ehero.alive && ehero.x == x && ehero.y == y) return true;
    int o = obj_at(x, y);
    if (o >= 0 && objs[o].type == O_TOWN_E) return true;   // enemy town blocks pathing
    return false;
}

// Dijkstra movement range from the player's hero, budget = p_mp
#define BIG 9999
static int dist[ROWS][COLS], prevc[ROWS][COLS];
static void compute_reach(void) {
    bool done[ROWS][COLS];
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        dist[y][x] = BIG; prevc[y][x] = -1; done[y][x] = false;
    }
    dist[phero.y][phero.x] = 0;
    for (;;) {
        int bx = -1, by = -1, bd = BIG;
        for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++)
            if (!done[y][x] && dist[y][x] < bd) { bd = dist[y][x]; bx = x; by = y; }
        if (bx < 0) break;
        done[by][bx] = true;
        static const int DX[4] = {0,0,-1,1}, DY[4] = {-1,1,0,0};
        for (int k = 0; k < 4; k++) {
            int nx = bx + DX[k], ny = by + DY[k];
            if (!in_grid(nx, ny) || blocked(nx, ny)) continue;
            int nd = dist[by][bx] + MOVE_COST[terr[ny][nx]];
            if (nd <= p_mp && nd < dist[ny][nx]) { dist[ny][nx] = nd; prevc[ny][nx] = by * COLS + bx; }
        }
    }
}
static bool reachable(int x, int y) { return in_grid(x, y) && dist[y][x] <= p_mp && dist[y][x] < BIG; }

// build a forward path from the hero to (tx,ty) into sel_path_*; returns step count
static int build_path(int tx, int ty) {
    if (!reachable(tx, ty)) return 0;
    int tmpx[64], tmpy[64], n = 0;
    int px = tx, py = ty;
    while ((px != phero.x || py != phero.y) && n < 64) {
        tmpx[n] = px; tmpy[n] = py;
        int p = prevc[py][px]; if (p < 0) break;
        px = p % COLS; py = p / COLS; n++;
    }
    path_len = 0;
    for (int i = n - 1; i >= 0; i--) { sel_path_x[path_len] = tmpx[i]; sel_path_y[path_len] = tmpy[i]; path_len++; }
    path_step = 0; walk_t = 0;
    return path_len;
}
// nearest reachable cell orthogonally adjacent to (tx,ty)
static bool adj_reach(int tx, int ty, int *ox, int *oy) {
    static const int DX[4] = {0,0,-1,1}, DY[4] = {-1,1,0,0};
    int best = BIG;
    for (int k = 0; k < 4; k++) {
        int x = tx + DX[k], y = ty + DY[k];
        if (reachable(x, y) && dist[y][x] < best) { best = dist[y][x]; *ox = x; *oy = y; }
    }
    return best < BIG;
}

static int army_total(const int *a) {
    int n = 0; for (int t = 0; t < NCREAT; t++) n += a[t]; return n;
}

// ── pick up / interact when the hero lands on a cell ──────────────────────────
static void enter_battle(void);
static void land_on(int x, int y) {
    int o = obj_at(x, y);
    if (o < 0) return;
    Obj *b = &objs[o];
    switch (b->type) {
        case O_GOLD:  p_gold += 250; b->taken = 1; snd_coin();
                      add_pop(scx(x)+2, scy(y)-2, CLR_YELLOW, "+250g"); break;
        case O_CHEST: p_gold += 600; b->taken = 1; snd_coin();
                      add_pop(scx(x)+2, scy(y)-2, CLR_YELLOW, "+600g"); break;
        case O_ORE:   p_ore += 3; b->taken = 1; snd_ore();
                      add_pop(scx(x)+2, scy(y)-2, CLR_ORANGE, "+3 ore"); break;
        case O_MINE:  if (b->owner != 0) { b->owner = 0; snd_flag();
                          add_pop(scx(x)+2, scy(y)-2, CLR_GREEN, "FLAGGED");
                          for (int s = 0; s < 5; s++) add_spark(scx(x)+8, scy(y)+4, CLR_GREEN);
                          set_msg("ore mine flagged: +2 ore / day"); msg_t = 120; }
                      break;
        case O_TOWN_P: mode = M_TOWN; break;
        case O_TOWN_E: win_state = 0; over_reason = "you seized the enemy town!";
                       wins++; save(0, wins); mode = M_OVER; snd_fanfare(1); break;
        default: break;
    }
}

// ── the rival's daily march ───────────────────────────────────────────────────
static int e_townx, e_towny, p_townx, p_towny;
static void cpu_day(void) {
    if (!ehero.alive) return;
    // target: your hero if reasonably close, else your town
    int tx = p_townx, ty = p_towny;
    if (abs(ehero.x - phero.x) + abs(ehero.y - phero.y) <= 7) { tx = phero.x; ty = phero.y; }
    int budget = 3;   // rival movement per day (in tiles)
    while (budget-- > 0) {
        // adjacent to your hero? engage (rival attacks)
        if (abs(ehero.x - phero.x) + abs(ehero.y - phero.y) == 1) { enter_battle(); return; }
        int bestx = ehero.x, besty = ehero.y, bestd = abs(ehero.x - tx) + abs(ehero.y - ty);
        static const int DX[4] = {0,0,-1,1}, DY[4] = {-1,1,0,0};
        for (int k = 0; k < 4; k++) {
            int nx = ehero.x + DX[k], ny = ehero.y + DY[k];
            if (!passable(nx, ny)) continue;
            if (phero.x == nx && phero.y == ny) { enter_battle(); return; }
            int o = obj_at(nx, ny);
            if (o >= 0 && (objs[o].type == O_TOWN_P || objs[o].type == O_TOWN_E)) continue;
            int d = abs(nx - tx) + abs(ny - ty);
            if (d < bestd) { bestd = d; bestx = nx; besty = ny; }
        }
        if (bestx == ehero.x && besty == ehero.y) break;   // stuck
        ehero.x = bestx; ehero.y = besty;
        if (ehero.x == p_townx && ehero.y == p_towny) {     // rival took your town
            win_state = 1; over_reason = "the enemy stormed your town"; mode = M_OVER; snd_fanfare(0); return;
        }
    }
}

static void end_day(void) {
    if (mode != M_ADV) return;
    day++;
    p_gold += 150;                                   // town treasury
    for (int i = 0; i < nobj; i++) if (objs[i].type == O_MINE && objs[i].owner == 0) p_ore += 2;
    if (day % 7 == 1) {                              // a new week — dwellings grow
        for (int t = 0; t < NCREAT; t++) if (built[t]) avail[t] += GROWTH[t];
        set_msg("a new week dawns - dwellings grew"); msg_t = 120;
    }
    p_mp = p_maxmp;
    snd_day();
    cpu_day();
}

// ════════════════════════════════════════════════════════════════════════════
//  BATTLE
// ════════════════════════════════════════════════════════════════════════════
#define BCOLS 15
#define BROWS 8
#define BATX 40
#define BATY 36
#define BINFO_Y (BATY + BROWS*TILE)   // 164

typedef struct {
    int type, count, hp;    // hp = current HP of the top (wounded) creature
    int team, x, y;
    int alive, retaliated, defending, bless;
} BStack;
#define MAXBS 10
static BStack bs[MAXBS];
static int nbs;
static int border[MAXBS], norder;     // initiative order this round
static int turn_idx, round_no;
static int b_obst[BROWS][BCOLS];      // 1 = rock (blocked)

// battle sub-state
enum { BM_INTRO, BM_PLAYER, BM_CPU, BM_OVER };
static int bmode;
static int b_active;                  // index of acting stack
static float cpu_delay;               // pause so the player can read CPU moves
static int b_winner = -1;
static int hero_cast;                 // spell used this round?
static int spell_pts = 8;
enum { SP_NONE, SP_BOLT, SP_BLESS };
static int spell_arm;                 // armed spell awaiting a target
static float intro_t;
static int b_lunge_i; static float b_lunge_t; static int b_lunge_dir;

static int bstack_at(int x, int y) {
    for (int i = 0; i < nbs; i++) if (bs[i].alive && bs[i].x == x && bs[i].y == y) return i;
    return -1;
}
static int bsx(int x) { return BATX + x * TILE; }
static int bsy(int y) { return BATY + y * TILE; }
static bool bin_grid(int x, int y) { return x >= 0 && y >= 0 && x < BCOLS && y < BROWS; }

// BFS movement range for the active stack (rocks + stacks block)
static int bdist[BROWS][BCOLS];
static void battle_reach(int si) {
    for (int y = 0; y < BROWS; y++) for (int x = 0; x < BCOLS; x++) bdist[y][x] = BIG;
    int qx[BROWS*BCOLS], qy[BROWS*BCOLS], h = 0, t = 0;
    bdist[bs[si].y][bs[si].x] = 0; qx[t] = bs[si].x; qy[t] = bs[si].y; t++;
    static const int DX[4] = {0,0,-1,1}, DY[4] = {-1,1,0,0};
    while (h < t) {
        int cx = qx[h], cy = qy[h]; h++;
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (!bin_grid(nx, ny) || b_obst[ny][nx]) continue;
            if (bstack_at(nx, ny) >= 0) continue;          // can't pass through a stack
            if (bdist[ny][nx] != BIG) continue;
            bdist[ny][nx] = bdist[cy][cx] + 1;
            qx[t] = nx; qy[t] = ny; t++;
        }
    }
}
static bool b_reachable(int si, int x, int y) {
    return bin_grid(x, y) && bdist[y][x] <= CD[bs[si].type].speed && bstack_at(x, y) < 0 && !b_obst[y][x];
}
static bool b_adj_reach(int si, int tx, int ty, int *ox, int *oy) {
    static const int DX[4] = {0,0,-1,1}, DY[4] = {-1,1,0,0};
    int best = BIG;
    for (int k = 0; k < 4; k++) {
        int x = tx + DX[k], y = ty + DY[k];
        if (b_reachable(si, x, y) && bdist[y][x] < best) { best = bdist[y][x]; *ox = x; *oy = y; }
        if (bs[si].x == x && bs[si].y == y) { best = 0; *ox = x; *oy = y; }   // already adjacent
    }
    return best < BIG;
}

static int count_team(int team) {
    int n = 0; for (int i = 0; i < nbs; i++) if (bs[i].alive && bs[i].team == team) n++; return n;
}

// damage: removes whole creatures from the defender's pool
static int apply_damage(int di, int loss) {
    BStack *d = &bs[di];
    int mx = CD[d->type].hp;
    int before = d->count;
    int pool = (d->count - 1) * mx + d->hp - loss;
    if (pool <= 0) { d->count = 0; d->hp = 0; d->alive = 0; return before; }
    int nc = (pool + mx - 1) / mx;
    d->count = nc; d->hp = pool - (nc - 1) * mx;
    return before - nc;
}
static float dmg_factor(const BStack *a, const BStack *d) {
    float f = 1.0f + 0.05f * (CD[a->type].atk + a->bless - CD[d->type].def - (d->defending ? 3 : 0));
    return clamp(f, 0.3f, 2.6f);
}
static int forecast_loss(const BStack *a, const BStack *d) {
    float avg = (CD[a->type].dlo + CD[a->type].dhi) / 2.0f;
    return (int)(a->count * avg * dmg_factor(a, d));
}
static int roll_loss(const BStack *a, const BStack *d) {
    int roll = rnd_between(CD[a->type].dlo, CD[a->type].dhi + 1);
    return (int)(a->count * roll * dmg_factor(a, d));
}

static void hit_fx(int di, int col, int n) {
    for (int s = 0; s < n; s++) add_spark(bsx(bs[di].x) + 8, bsy(bs[di].y) + 8, s & 1 ? col : CLR_WHITE);
    flash = 2;
}
static void resolve_shoot(int ai, int di) {
    snd_arrow();
    int loss = roll_loss(&bs[ai], &bs[di]);
    int kills = apply_damage(di, loss);
    hit_fx(di, CLR_LIGHT_YELLOW, 4);
    add_pop(bsx(bs[di].x) + 2, bsy(bs[di].y) - 2, CLR_RED, kills > 0 ? str("-%d!", kills) : "0");
}
static void resolve_melee(int ai, int di) {
    snd_clash();
    b_lunge_i = ai; b_lunge_t = 1.0f; b_lunge_dir = sgn(bs[di].x - bs[ai].x);
    int loss = roll_loss(&bs[ai], &bs[di]);
    int kills = apply_damage(di, loss);
    hit_fx(di, CLR_ORANGE, 6);
    add_pop(bsx(bs[di].x) + 2, bsy(bs[di].y) - 2, CLR_RED, kills > 0 ? str("-%d", kills) : "0");
    // retaliation: a living melee/any defender strikes back once per round
    if (bs[di].alive && !bs[di].retaliated) {
        bs[di].retaliated = 1;
        int back = roll_loss(&bs[di], &bs[ai]);
        int k2 = apply_damage(ai, back);
        add_pop(bsx(bs[ai].x) + 2, bsy(bs[ai].y) - 2, CLR_WHITE, k2 > 0 ? str("-%d", k2) : "0");
        for (int s = 0; s < 3; s++) add_spark(bsx(bs[ai].x) + 8, bsy(bs[ai].y) + 8, CLR_LIGHT_GREY);
    }
}

static void finish_battle(int winner);
static void check_battle_over(void) {
    if (count_team(0) == 0) { b_winner = 1; bmode = BM_OVER; snd_fanfare(0); }
    else if (count_team(1) == 0) { b_winner = 0; bmode = BM_OVER; snd_fanfare(1); }
}

static void build_order(void) {
    norder = 0;
    for (int i = 0; i < nbs; i++) if (bs[i].alive) border[norder++] = i;
    // sort by speed desc; ties: player first, then index (simple insertion sort)
    for (int i = 1; i < norder; i++) {
        int v = border[i], j = i - 1;
        while (j >= 0) {
            int a = border[j];
            int sa = CD[bs[a].type].speed * 10 + (bs[a].team == 0 ? 1 : 0);
            int sv = CD[bs[v].type].speed * 10 + (bs[v].team == 0 ? 1 : 0);
            if (sa >= sv) break;
            border[j + 1] = border[j]; j--;
        }
        border[j + 1] = v;
    }
}
static void begin_active(void);
static void start_round(void) {
    round_no++; hero_cast = 0;
    for (int i = 0; i < nbs; i++) { bs[i].retaliated = 0; bs[i].defending = 0; }
    build_order(); turn_idx = 0;
}
static void next_turn(void) {
    spell_arm = SP_NONE;
    check_battle_over();
    if (bmode == BM_OVER) return;
    turn_idx++;
    while (turn_idx < norder && !bs[border[turn_idx]].alive) turn_idx++;
    if (turn_idx >= norder) start_round();
    begin_active();
}
static void begin_active(void) {
    while (turn_idx < norder && !bs[border[turn_idx]].alive) turn_idx++;
    if (turn_idx >= norder) { start_round(); }
    b_active = border[turn_idx];
    if (bs[b_active].team == 0) { bmode = BM_PLAYER; battle_reach(b_active); }
    else { bmode = BM_CPU; cpu_delay = 0.45f; }
}

static void enter_battle(void) {
    // build battle stacks from both armies
    nbs = 0;
    int prow = 1;
    for (int t = 0; t < NCREAT; t++) if (p_army[t] > 0) {
        bs[nbs] = (BStack){ t, p_army[t], CD[t].hp, 0, 1, prow, 1, 0, 0, 0 };
        prow += 2; if (prow >= BROWS) prow = 2; nbs++;
    }
    int erow = 1;
    for (int t = 0; t < NCREAT; t++) if (e_army[t] > 0) {
        bs[nbs] = (BStack){ t, e_army[t], CD[t].hp, 1, BCOLS - 2, erow, 1, 0, 0, 0 };
        erow += 2; if (erow >= BROWS) erow = 2; nbs++;
    }
    // a few rocks down the middle for tactics
    for (int y = 0; y < BROWS; y++) for (int x = 0; x < BCOLS; x++) b_obst[y][x] = 0;
    b_obst[1][7] = 1; b_obst[3][6] = 1; b_obst[5][8] = 1; b_obst[6][7] = 1;
    spell_pts = 8; spell_arm = SP_NONE; b_winner = -1; round_no = 0;
    intro_t = 1.1f; bmode = BM_INTRO; b_lunge_t = 0;
    mode = M_BATTLE;
    bpm(110);
}

static void finish_battle(int winner) {
    if (winner == 0) {                 // you routed the enemy hero
        ehero.alive = 0;
        win_state = 0; over_reason = "the enemy hero is routed!";
        wins++; save(0, wins);
    } else {                           // your army is destroyed
        phero.alive = 0;
        win_state = 1; over_reason = "your hero has fallen";
    }
    mode = M_OVER;
}

// ── battle CPU ────────────────────────────────────────────────────────────────
static void cpu_act(int si) {
    BStack *u = &bs[si];
    battle_reach(si);
    // best target = highest forecast damage, kill bonus
    int bestT = -1, bestScore = -1;
    for (int i = 0; i < nbs; i++) if (bs[i].alive && bs[i].team != u->team) {
        int loss = forecast_loss(u, &bs[i]);
        int score = loss * 4;
        if (loss >= (bs[i].count - 1) * CD[bs[i].type].hp + bs[i].hp) score += 80;  // wipe
        int ox, oy;
        bool reach = CD[u->type].shots > 0 || b_adj_reach(si, bs[i].x, bs[i].y, &ox, &oy);
        if (!reach && CD[u->type].shots == 0) score -= 1000;  // can't reach this turn
        if (score > bestScore) { bestScore = score; bestT = i; }
    }
    if (bestT < 0) { next_turn(); return; }
    if (CD[u->type].shots > 0) { resolve_shoot(si, bestT); next_turn(); return; }
    int ox, oy;
    if (b_adj_reach(si, bs[bestT].x, bs[bestT].y, &ox, &oy)) {
        u->x = ox; u->y = oy; resolve_melee(si, bestT); next_turn(); return;
    }
    // can't reach a target: step toward the nearest enemy
    int tx = -1, ty = -1, bd = BIG;
    for (int i = 0; i < nbs; i++) if (bs[i].alive && bs[i].team != u->team) {
        int d = abs(u->x - bs[i].x) + abs(u->y - bs[i].y);
        if (d < bd) { bd = d; tx = bs[i].x; ty = bs[i].y; }
    }
    int bx = u->x, by = u->y, best = BIG;
    for (int y = 0; y < BROWS; y++) for (int x = 0; x < BCOLS; x++) if (b_reachable(si, x, y)) {
        int d = abs(x - tx) + abs(y - ty);
        if (d < best) { best = d; bx = x; by = y; }
    }
    u->x = bx; u->y = by; next_turn();
}

// ════════════════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════════════════
static const char *LAYOUT[ROWS] = {
    "~~mm....ff....mm..~~",
    "~..m...........m...~",
    "....f....rr....f....",
    ".ff.....rrr.....ff..",
    "...m...rr.......m...",
    "...m..rr........m...",
    "..ff.rr....mm..ff...",
    "....rr.....mm.......",
    "~..rr....ff.......m~",
    "~~rr..mm....ff..mm~~",
};
static int char_terr(char c) {
    switch (c) {
        case 'f': return TT_FOREST; case 'm': return TT_MTN;
        case '~': return TT_WATER;  case 'r': return TT_ROAD;
        default:  return TT_GRASS;
    }
}
static bool ready = false;
static void add_obj(int type, int x, int y, int owner) {
    if (nobj < MAXOBJ) objs[nobj++] = (Obj){ type, x, y, 0, owner };
}
static void reset_game(void) {
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        const char *row = LAYOUT[y];
        terr[y][x] = char_terr(row[x] ? row[x] : '.');
        mset(x, y, TERR_SLOT[terr[y][x]]);
    }
    nobj = 0;
    p_townx = 2; p_towny = 8; e_townx = 17; e_towny = 1;
    add_obj(O_TOWN_P, p_townx, p_towny, 0);
    add_obj(O_TOWN_E, e_townx, e_towny, 1);
    add_obj(O_MINE, 10, 2, -1);
    add_obj(O_GOLD,  6, 8, -1);
    add_obj(O_GOLD, 13, 3, -1);
    add_obj(O_ORE,   4, 4, -1);
    add_obj(O_ORE,  15, 6, -1);
    add_obj(O_CHEST, 9, 5, -1);
    add_obj(O_CHEST,12, 8, -1);
    // make sure object tiles are walkable grass
    for (int i = 0; i < nobj; i++) { terr[objs[i].y][objs[i].x] = TT_GRASS; mset(objs[i].x, objs[i].y, 1); }

    phero = (MapHero){ 3, 8, 1 };
    ehero = (MapHero){ 16, 2, 1 };
    p_gold = 1000; p_ore = 3; p_mp = p_maxmp; day = 1;
    for (int t = 0; t < NCREAT; t++) { p_army[t] = 0; built[t] = 0; avail[t] = 0; }
    p_army[C_PIKE] = 12;            // a starter garrison
    built[C_PIKE] = 1; avail[C_PIKE] = 9;
    e_army[C_PIKE] = 16; e_army[C_ARCHER] = 7; e_army[C_GRIFFIN] = 4; e_army[C_OGRE] = 1;

    float_gold = p_gold; float_ore = p_ore;
    msg[0] = 0; msg_t = 0; path_len = 0; path_step = 0; walk_engage = 0;
    for (int i = 0; i < 64; i++) sparks[i].life = 0;
    for (int i = 0; i < 12; i++) pops[i].life = 0;
    flash = 0; mode = M_ADV; win_state = -1; wins = load(0);
    cursor_cx = phero.x; cursor_cy = phero.y;
    ready = true;
}

void init(void) {
    colorkey(0);
    instrument(5, INSTR_TRI,   3, 90, 3, 160);   // lute / chime
    instrument(7, INSTR_NOISE, 1, 120, 1, 200);  // clash
    instrument_filter(7, FILTER_LOW, 600, 3);
    instrument(8, INSTR_SQUARE, 1, 40, 0, 30);   // blip
    bpm(90);
    reset_game();
}

// ════════════════════════════════════════════════════════════════════════════
//  UPDATE
// ════════════════════════════════════════════════════════════════════════════
static bool clicked(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) && mouse_x() >= x && mouse_x() < x + w && mouse_y() >= y && mouse_y() < y + h;
}

// ── adventure update ──────────────────────────────────────────────────────────
static void update_adventure(void) {
    if (msg_t > 0) msg_t--;
    // soft wandering-bard bed
    static int lb = -1; int b = beat();
    if (b != lb) { lb = b; if (b % 2 == 0 && chance(55)) note(degree(SCALE_PENTA, 4, rnd(5)), 5, 1); }

    compute_reach();

    // walking along a queued path
    if (path_len > 0) {
        walk_t += dt();
        if (walk_t >= 0.10f) {
            walk_t = 0;
            int nx = sel_path_x[path_step], ny = sel_path_y[path_step];
            p_mp -= MOVE_COST[terr[ny][nx]];
            phero.x = nx; phero.y = ny; path_step++;
            snd_step();
            land_on(nx, ny);
            if (mode != M_ADV) { path_len = 0; return; }     // entered town / won
            if (path_step >= path_len) {                     // arrived
                path_len = 0;
                if (walk_engage && ehero.alive &&
                    abs(phero.x - ehero.x) + abs(phero.y - ehero.y) == 1) { walk_engage = 0; enter_battle(); return; }
                walk_engage = 0;
            }
            compute_reach();
        }
        return;   // ignore input while walking
    }

    // cursor follows the mouse
    if (mouse_x() != lastmx || mouse_y() != lastmy) {
        lastmx = mouse_x(); lastmy = mouse_y();
        int mx = mouse_x() / TILE, my = (mouse_y() - MAP_Y) / TILE;
        if (mouse_y() >= MAP_Y && mouse_y() < INFO_Y && in_grid(mx, my)) { cursor_cx = mx; cursor_cy = my; }
    }

    // end day button / SPACE
    if (clicked(SCREEN_W - 66, 4, 62, HUD_Y - 8) || keyp(KEY_SPACE)) { end_day(); return; }

    // a left click on the map
    if (mouse_pressed(MOUSE_LEFT) && mouse_y() >= MAP_Y && mouse_y() < INFO_Y) {
        int gx = mouse_x() / TILE, gy = (mouse_y() - MAP_Y) / TILE;
        if (!in_grid(gx, gy)) return;
        // click the rival hero → walk adjacent, then battle
        if (ehero.alive && ehero.x == gx && ehero.y == gy) {
            int ox, oy;
            if (abs(phero.x - gx) + abs(phero.y - gy) == 1) { enter_battle(); return; }
            if (adj_reach(gx, gy, &ox, &oy) && build_path(ox, oy)) walk_engage = 1;
            else { set_msg("out of reach this day"); msg_t = 90; }
            return;
        }
        int o = obj_at(gx, gy);
        if (o >= 0 && objs[o].type == O_TOWN_E) {            // enemy town → path adjacent or onto it
            if (phero.x == gx && phero.y == gy) { land_on(gx, gy); return; }
            int ox, oy;
            if (adj_reach(gx, gy, &ox, &oy)) { build_path(ox, oy); walk_engage = 0; }
            return;
        }
        if (reachable(gx, gy)) build_path(gx, gy);
        else { set_msg("out of reach this day"); msg_t = 90; }
    }
}

// ── town update ───────────────────────────────────────────────────────────────
static int town_row_y(int t) { return 36 + t * 34; }
static void update_town(void) {
    if (keyp('B') || keyp(KEY_ESCAPE) || btnp(0, BTN_B) || clicked(SCREEN_W - 66, 4, 62, 16)) { mode = M_ADV; return; }
    for (int t = 0; t < NCREAT; t++) {
        int y = town_row_y(t);
        if (!built[t]) {
            if (clicked(214, y + 2, 96, 22)) {
                if (p_gold >= DWELL_G[t] && p_ore >= DWELL_O[t]) {
                    p_gold -= DWELL_G[t]; p_ore -= DWELL_O[t];
                    built[t] = 1; avail[t] += GROWTH[t]; snd_build();
                    for (int s = 0; s < 6; s++) add_spark(150, y + 12, CLR_YELLOW);
                } else { set_msg("not enough resources"); msg_t = 90; }
            }
        } else {
            // left-click recruit 1, right-click recruit max
            bool one = clicked(214, y + 2, 96, 22);
            bool many = mouse_pressed(MOUSE_RIGHT) && mouse_x() >= 214 && mouse_x() < 310
                        && mouse_y() >= y + 2 && mouse_y() < y + 24;
            if (one || many) {
                int n = 0;
                while (avail[t] > 0 && p_gold >= CD[t].gcost && p_ore >= CD[t].ocost) {
                    p_gold -= CD[t].gcost; p_ore -= CD[t].ocost;
                    avail[t]--; p_army[t]++; n++;
                    if (!many) break;
                }
                if (n > 0) { snd_recruit(); for (int s = 0; s < 5; s++) add_spark(260, y + 12, CLR_GREEN); }
                else { set_msg("can't recruit any"); msg_t = 90; }
            }
        }
    }
    if (msg_t > 0) msg_t--;
}

// ── battle update ─────────────────────────────────────────────────────────────
static int hover_bcell_x(void) { return (mouse_x() - BATX) / TILE; }
static int hover_bcell_y(void) { return (mouse_y() - BATY) / TILE; }

static void player_attack_or_move(void) {
    int si = b_active;
    int gx = hover_bcell_x(), gy = hover_bcell_y();
    if (!bin_grid(gx, gy)) return;
    int ti = bstack_at(gx, gy);
    if (ti >= 0 && bs[ti].team != bs[si].team) {                 // attack an enemy
        if (CD[bs[si].type].shots > 0) { resolve_shoot(si, ti); next_turn(); return; }
        int ox, oy;
        if (b_adj_reach(si, gx, gy, &ox, &oy)) { bs[si].x = ox; bs[si].y = oy; resolve_melee(si, ti); next_turn(); }
        else { set_msg("too far to strike"); msg_t = 70; }
        return;
    }
    if (b_reachable(si, gx, gy)) { bs[si].x = gx; bs[si].y = gy; next_turn(); }  // move (ends turn)
}

static void update_battle(void) {
    if (bmode == BM_INTRO) {
        intro_t -= dt();
        if (intro_t <= 0 || mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A)) { start_round(); begin_active(); }
        return;
    }
    // martial drum bed
    static int lb = -1; int b = beat();
    if (b != lb && bmode != BM_OVER) { lb = b; if (b % 2 == 0) note(36, 4, 2); else if (chance(60)) note(60, 8, 1); }

    if (bmode == BM_OVER) {
        if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A)) finish_battle(b_winner);
        return;
    }
    if (bmode == BM_CPU) {
        cpu_delay -= dt();
        if (cpu_delay <= 0) cpu_act(b_active);
        return;
    }
    // BM_PLAYER
    if (msg_t > 0) msg_t--;
    int si = b_active;
    // spell buttons
    if (spell_pts >= 3 && !hero_cast && clicked(4, BINFO_Y + 4, 60, 14)) { spell_arm = (spell_arm == SP_BOLT) ? SP_NONE : SP_BOLT; return; }
    if (spell_pts >= 2 && !hero_cast && clicked(4, BINFO_Y + 20, 60, 14)) { spell_arm = (spell_arm == SP_BLESS) ? SP_NONE : SP_BLESS; return; }
    if (clicked(SCREEN_W - 64, BINFO_Y + 4, 60, 14) || keyp(KEY_SPACE)) { bs[si].defending = 1; next_turn(); return; }  // DEFEND

    if (spell_arm != SP_NONE && mouse_pressed(MOUSE_LEFT)) {
        int gx = hover_bcell_x(), gy = hover_bcell_y(), ti = bstack_at(gx, gy);
        if (ti >= 0) {
            if (spell_arm == SP_BOLT && bs[ti].team != bs[si].team) {
                snd_bolt(); int k = apply_damage(ti, 16); hero_cast = 1; spell_pts -= 3;
                hit_fx(ti, CLR_BLUE, 8);
                add_pop(bsx(bs[ti].x) + 2, bsy(bs[ti].y) - 2, CLR_BLUE, k > 0 ? str("-%d!", k) : "0");
                spell_arm = SP_NONE; check_battle_over(); return;
            }
            if (spell_arm == SP_BLESS && bs[ti].team == bs[si].team) {
                snd_bless(); bs[ti].bless += 4; hero_cast = 1; spell_pts -= 2;
                for (int s = 0; s < 6; s++) add_spark(bsx(bs[ti].x) + 8, bsy(bs[ti].y) + 8, CLR_YELLOW);
                add_pop(bsx(bs[ti].x) + 2, bsy(bs[ti].y) - 2, CLR_YELLOW, "BLESS");
                spell_arm = SP_NONE; return;
            }
        }
        spell_arm = SP_NONE; return;
    }
    if (mouse_pressed(MOUSE_LEFT)) player_attack_or_move();
}

void update(void) {
    if (!ready) reset_game();
    age_effects();
    if (b_lunge_t > 0) b_lunge_t -= dt() * 4.0f;
    // gold/ore counters roll toward their target
    float_gold = lerp(float_gold, p_gold, 0.2f);
    float_ore  = lerp(float_ore,  p_ore,  0.2f);

    if (mode == M_ADV)         update_adventure();
    else if (mode == M_TOWN)   update_town();
    else if (mode == M_BATTLE) update_battle();
    else if (mode == M_OVER)   { if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A)) reset_game(); }
}

// ════════════════════════════════════════════════════════════════════════════
//  DRAW
// ════════════════════════════════════════════════════════════════════════════
static void draw_obj(const Obj *o) {
    if (o->taken) return;
    int sx = scx(o->x), sy = scy(o->y);
    int slot = 0, owner = -1;
    switch (o->type) {
        case O_GOLD: slot = 6; break;
        case O_ORE:  slot = 7; break;
        case O_CHEST:slot = 8; break;
        case O_MINE: slot = 9; owner = o->owner; break;
        case O_TOWN_P: slot = 10; owner = 0; break;
        case O_TOWN_E: slot = 10; owner = 1; break;
    }
    if (slot == 9 || slot == 10) { owner_pal(owner); spr(slot, sx, sy); pal_reset(); }
    else spr(slot, sx, sy);
}

static void draw_adventure(void) {
    cls(CLR_DARK_BLUE);
    map(0, 0, 0, MAP_Y, COLS, ROWS);

    // reachable-tile shimmer
    if (path_len == 0) {
        fillp(FILL_CHECKER, -1);
        for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++)
            if (reachable(x, y) && !(x == phero.x && y == phero.y)) rectfill(scx(x), scy(y), TILE, TILE, CLR_BLUE);
        fillp_reset();
    }

    for (int i = 0; i < nobj; i++) draw_obj(&objs[i]);

    // the rival hero
    if (ehero.alive) {
        int bob = anim(2, 2.0f, 0.3f) ? -1 : 0;
        owner_pal(1); spr(11, scx(ehero.x), scy(ehero.y) + bob); pal_reset();
    }
    // your hero (walk-bob while travelling)
    {
        int bob = (path_len > 0) ? (anim(2, 8.0f, 0.0f) ? -2 : 0) : (anim(2, 1.6f, 0.0f) ? -1 : 0);
        owner_pal(0); spr(11, scx(phero.x), scy(phero.y) + bob); pal_reset();
    }

    // sparks + pops
    for (int i = 0; i < 64; i++) if (sparks[i].life > 0) pset((int)sparks[i].x, (int)sparks[i].y, sparks[i].col);
    for (int i = 0; i < 12; i++) if (pops[i].life > 0) print(pops[i].txt, (int)pops[i].x, (int)pops[i].y, pops[i].col);

    // cursor
    {
        int p = (frame() % 30 < 15) ? 0 : 1;
        int col = (ehero.alive && cursor_cx == ehero.x && cursor_cy == ehero.y) ? CLR_RED : CLR_YELLOW;
        rect(scx(cursor_cx) - p, scy(cursor_cy) - p, TILE + 2*p, TILE + 2*p, col);
    }

    // ── top HUD ──
    rectfill(0, 0, SCREEN_W, HUD_Y, CLR_BROWNISH_BLACK);
    print(str("GOLD %d", (int)(float_gold + 0.5f)), 6, 3, CLR_YELLOW);
    print(str("ORE %d", (int)(float_ore + 0.5f)), 92, 3, CLR_ORANGE);
    print(str("DAY %d  WK %d", day, (day - 1) / 7 + 1), 6, 13, CLR_LIGHT_GREY);
    bar(150, 14, 60, 7, p_mp / (float)p_maxmp, CLR_GREEN, CLR_DARKER_GREY);
    print("MOVE", 150, 3, CLR_GREEN);
    print(str("army %d", army_total(p_army)), 216, 3, CLR_PEACH);
    rectfill(SCREEN_W - 66, 4, 62, HUD_Y - 8, CLR_DARK_GREEN); rect(SCREEN_W - 66, 4, 62, HUD_Y - 8, CLR_WHITE);
    print("END DAY", SCREEN_W - 60, 9, CLR_WHITE);

    // ── bottom info strip ──
    rectfill(0, INFO_Y, SCREEN_W, SCREEN_H - INFO_Y, CLR_DARKER_PURPLE);
    if (msg_t > 0) print(msg, 4, INFO_Y + 4, CLR_LIGHT_YELLOW);
    else {
        int o = obj_at(cursor_cx, cursor_cy);
        const char *what = TERR_NAME[terr[cursor_cy][cursor_cx]];
        if (o >= 0) {
            switch (objs[o].type) {
                case O_GOLD: what = "gold pile (+250)"; break;
                case O_ORE:  what = "ore pile (+3)"; break;
                case O_CHEST:what = "treasure chest"; break;
                case O_MINE: what = objs[o].owner == 0 ? "your ore mine" : "ore mine — flag it"; break;
                case O_TOWN_P: what = "YOUR TOWN — enter"; break;
                case O_TOWN_E: what = "enemy town — seize it"; break;
            }
        } else if (ehero.alive && cursor_cx == ehero.x && cursor_cy == ehero.y) what = "ENEMY HERO - attack!";
        print(what, 4, INFO_Y + 4, CLR_WHITE);
        print_right("click a tile to move   SPACE end day", SCREEN_W - 4, INFO_Y + 4, CLR_INDIGO);
    }
}

// ── creature stack icon + count chip ──────────────────────────────────────────
static void draw_bstack(int i) {
    BStack *u = &bs[i];
    if (!u->alive) return;
    int sx = bsx(u->x), sy = bsy(u->y);
    int lx = 0;
    if (i == b_lunge_i && b_lunge_t > 0) lx = (int)(b_lunge_dir * b_lunge_t * 5);
    owner_pal(u->team);
    spr(CD[u->type].sprite, sx + lx, sy);
    pal_reset();
    if (u->bless) { if (blink(6)) { pal(CLR_WHITE, CLR_YELLOW); spr(CD[u->type].sprite, sx + lx, sy); pal_reset(); } }
    // count chip
    rectfill(sx + 2, sy + 11, 13, 5, CLR_BLACK);
    print(str("%d", u->count), sx + 3, sy + 11, u->team == 0 ? CLR_LIGHT_PEACH : CLR_LIGHT_YELLOW);
}

static void draw_battle(void) {
    cls(CLR_BROWNISH_BLACK);
    // field
    for (int y = 0; y < BROWS; y++) for (int x = 0; x < BCOLS; x++) {
        spr(12, bsx(x), bsy(y));
        if (b_obst[y][x]) spr(13, bsx(x), bsy(y));
    }
    // move range for the active player stack
    if (bmode == BM_PLAYER) {
        int si = b_active;
        fillp(FILL_CHECKER, -1);
        for (int y = 0; y < BROWS; y++) for (int x = 0; x < BCOLS; x++)
            if (b_reachable(si, x, y)) rectfill(bsx(x), bsy(y), TILE, TILE, CLR_BLUE);
        fillp_reset();
        // active highlight
        rect(bsx(bs[si].x), bsy(bs[si].y), TILE, TILE, CLR_YELLOW);
    }

    for (int i = 0; i < nbs; i++) draw_bstack(i);

    for (int i = 0; i < 64; i++) if (sparks[i].life > 0) pset((int)sparks[i].x, (int)sparks[i].y, sparks[i].col);
    for (int i = 0; i < 12; i++) if (pops[i].life > 0) print(pops[i].txt, (int)pops[i].x, (int)pops[i].y, pops[i].col);

    if (flash > 0) { fillp(FILL_CHECKER, -1); rectfill(BATX, BATY, BCOLS*TILE, BROWS*TILE, CLR_WHITE); fillp_reset(); }

    // ── top bar ──
    rectfill(0, 0, SCREEN_W, 16, CLR_DARKER_BLUE);
    print(str("ROUND %d", round_no), 6, 4, CLR_WHITE);
    print_centered(bmode == BM_PLAYER ? "your move" : bmode == BM_CPU ? "enemy moving..." : "", SCREEN_W/2, 4, bmode == BM_PLAYER ? CLR_GREEN : CLR_RED);
    print_right(str("SPELL %d", spell_pts), SCREEN_W - 4, 4, CLR_BLUE);

    // ── forecast when hovering an enemy with the active stack ──
    if (bmode == BM_PLAYER) {
        int si = b_active, gx = hover_bcell_x(), gy = hover_bcell_y(), ti = bstack_at(gx, gy);
        if (ti >= 0 && bs[ti].team != bs[si].team) {
            int loss = forecast_loss(&bs[si], &bs[ti]);
            int bx = clamp(bsx(bs[ti].x) - 20, 2, SCREEN_W - 70), by = clamp(bsy(bs[ti].y) - 22, 18, BINFO_Y - 22);
            rectfill(bx, by, 68, 20, CLR_BLACK); rect(bx, by, 68, 20, CLR_RED);
            print(CD[bs[ti].type].name, bx + 3, by + 2, CLR_LIGHT_GREY);
            print(str("~ -%d", loss), bx + 3, by + 11, CLR_YELLOW);
            // target ring
            rect(bsx(bs[ti].x), bsy(bs[ti].y), TILE, TILE, CLR_RED);
        }
    }

    // ── bottom strip: active stack + actions ──
    rectfill(0, BINFO_Y, SCREEN_W, SCREEN_H - BINFO_Y, CLR_DARKER_PURPLE);
    if (bmode == BM_PLAYER || bmode == BM_CPU) {
        BStack *u = &bs[b_active];
        owner_pal(u->team); spr(CD[u->type].sprite, SCREEN_W/2 - 8, BINFO_Y + 2); pal_reset();
        print_centered(str("%s x%d", CD[u->type].name, u->count), SCREEN_W/2, BINFO_Y + 2, CLR_WHITE);
        print_centered(CD[u->type].shots > 0 ? "ranged - shoot any target" :
                       "melee - click a tile or foe", SCREEN_W/2, BINFO_Y + 22, CLR_LIGHT_GREY);
    }
    if (bmode == BM_PLAYER) {
        // spell buttons (left)
        int b1 = (spell_arm == SP_BOLT) ? CLR_BLUE : CLR_DARKER_BLUE;
        int b2 = (spell_arm == SP_BLESS) ? CLR_DARK_GREEN : CLR_DARKER_BLUE;
        bool can1 = spell_pts >= 3 && !hero_cast, can2 = spell_pts >= 2 && !hero_cast;
        rectfill(4, BINFO_Y + 4, 60, 14, b1); rect(4, BINFO_Y + 4, 60, 14, can1 ? CLR_WHITE : CLR_DARK_GREY);
        print("BOLT 3", 9, BINFO_Y + 7, can1 ? CLR_WHITE : CLR_DARK_GREY);
        rectfill(4, BINFO_Y + 20, 60, 14, b2); rect(4, BINFO_Y + 20, 60, 14, can2 ? CLR_WHITE : CLR_DARK_GREY);
        print("BLESS 2", 9, BINFO_Y + 23, can2 ? CLR_WHITE : CLR_DARK_GREY);
        // defend (right)
        rectfill(SCREEN_W - 64, BINFO_Y + 4, 60, 14, CLR_DARK_PURPLE); rect(SCREEN_W - 64, BINFO_Y + 4, 60, 14, CLR_WHITE);
        print("DEFEND", SCREEN_W - 58, BINFO_Y + 7, CLR_WHITE);
    }

    if (bmode == BM_INTRO) {
        fade(0.4f);
        print_centered("BATTLE!", SCREEN_W/2, 86, CLR_RED);
        print_centered("click to begin", SCREEN_W/2, 100, CLR_LIGHT_GREY);
    }
    if (bmode == BM_OVER) {
        fade(0.5f);
        int w = 200, bx = (SCREEN_W - w) / 2;
        rectfill(bx, 80, w, 36, b_winner == 0 ? CLR_DARK_GREEN : CLR_DARK_PURPLE);
        rect(bx, 80, w, 36, CLR_WHITE);
        print_centered(b_winner == 0 ? "FIELD WON" : "ARMY ROUTED", SCREEN_W/2, 88, b_winner == 0 ? CLR_GREEN : CLR_RED);
        print_centered("click to continue", SCREEN_W/2, 102, CLR_LIGHT_GREY);
    }
}

static void draw_town(void) {
    cls(CLR_DARKER_BLUE);
    // header
    rectfill(0, 0, SCREEN_W, 28, CLR_BROWNISH_BLACK);
    owner_pal(0); spr(10, 6, 6); pal_reset();
    print_scaled("YOUR TOWN", 28, 4, CLR_LIGHT_PEACH, 2);
    print(str("GOLD %d", p_gold), 200, 4, CLR_YELLOW);
    print(str("ORE %d", p_ore), 200, 16, CLR_ORANGE);
    rectfill(SCREEN_W - 66, 4, 62, 16, CLR_DARK_PURPLE); rect(SCREEN_W - 66, 4, 62, 16, CLR_WHITE);
    print("LEAVE", SCREEN_W - 52, 8, CLR_WHITE);

    for (int t = 0; t < NCREAT; t++) {
        int y = town_row_y(t);
        rectfill(4, y, 306, 30, CLR_DARKER_PURPLE); rect(4, y, 306, 30, CLR_DARK_GREY);
        owner_pal(0); spr(CD[t].sprite, 8, y + 6); pal_reset();
        print(CD[t].name, 30, y + 3, built[t] ? CLR_WHITE : CLR_LIGHT_GREY);
        print(str("hp%d atk%d dmg%d-%d spd%d%s", CD[t].hp, CD[t].atk, CD[t].dlo, CD[t].dhi,
                  CD[t].speed, CD[t].shots > 0 ? " RNG" : ""), 30, y + 14, CLR_INDIGO);
        print(str("have %d", p_army[t]), 150, y + 3, CLR_PEACH);

        int bx = 214, bw = 96;
        if (!built[t]) {
            bool ok = p_gold >= DWELL_G[t] && p_ore >= DWELL_O[t];
            rectfill(bx, y + 2, bw, 22, ok ? CLR_DARK_GREEN : CLR_DARKER_GREY);
            rect(bx, y + 2, bw, 22, CLR_WHITE);
            print("BUILD", bx + 6, y + 5, CLR_WHITE);
            print(str("%dg %do", DWELL_G[t], DWELL_O[t]), bx + 6, y + 14, ok ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
        } else {
            bool ok = avail[t] > 0 && p_gold >= CD[t].gcost && p_ore >= CD[t].ocost;
            rectfill(bx, y + 2, bw, 22, ok ? CLR_DARK_BLUE : CLR_DARKER_GREY);
            rect(bx, y + 2, bw, 22, CLR_WHITE);
            print(str("RECRUIT (%d)", avail[t]), bx + 4, y + 5, CLR_WHITE);
            int costcol = ok ? CLR_LIGHT_YELLOW : CLR_DARK_GREY;
            if (CD[t].ocost) print(str("%dg %do ea", CD[t].gcost, CD[t].ocost), bx + 4, y + 14, costcol);
            else             print(str("%dg ea", CD[t].gcost), bx + 4, y + 14, costcol);
        }
    }
    if (msg_t > 0) print_centered(msg, SCREEN_W/2, SCREEN_H - 10, CLR_LIGHT_YELLOW);
    else print_centered("BUILD a dwelling, then RECRUIT (right-click = max).  B to leave", SCREEN_W/2, SCREEN_H - 10, CLR_INDIGO);

    for (int i = 0; i < 64; i++) if (sparks[i].life > 0) pset((int)sparks[i].x, (int)sparks[i].y, sparks[i].col);
}

static void draw_over(void) {
    if (win_state == 0) draw_adventure(); else draw_battle();
    fade(0.55f);
    int w = 240, bx = (SCREEN_W - w) / 2;
    rectfill(bx, 70, w, 56, win_state == 0 ? CLR_DARK_GREEN : CLR_DARK_PURPLE);
    rect(bx, 70, w, 56, CLR_WHITE);
    print_centered(win_state == 0 ? "VICTORY!" : "DEFEAT", SCREEN_W/2, 78, win_state == 0 ? CLR_GREEN : CLR_RED);
    print_centered(over_reason, SCREEN_W/2, 94, CLR_LIGHT_GREY);
    print_centered(str("victories: %d", wins), SCREEN_W/2, 106, CLR_LIGHT_YELLOW);
    print_centered("click for a new campaign", SCREEN_W/2, 116, CLR_YELLOW);
}

void draw(void) {
    if (!ready) reset_game();
    if (mode == M_ADV)         draw_adventure();
    else if (mode == M_TOWN)   draw_town();
    else if (mode == M_BATTLE) draw_battle();
    else if (mode == M_OVER)   draw_over();
    cursor_draw(CUR_ARROW);
}
