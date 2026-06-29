/* de:meta
{
  "title": "cannon fodder",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "finite-state-ai",
    "particle-system",
    "save-load-persistence"
  ],
  "lineage": "Homage to Sensible Software's Cannon Fodder; squad follows a leader-chain trail toward a click target, enemies wander then chase with line-of-sight autofire, and the roster survives between missions via save_bytes — permanent death carrying over is the defining mechanic.",
  "genre": "strategy",
  "homage": "Cannon Fodder (1993)",
  "description": "Top-down run-and-gun squad tactics, mouse-commanded. Lead up to five tiny soldiers across three scrolling battlefields — LEFT-CLICK ground to send the squad there in a loose trailing formation; they AUTO-FIRE their rifles at any enemy in range and line of sight; RIGHT-CLICK lobs an arcing grenade with a big AoE to level the huts and gun-nests that are your objectives. The bittersweet hook: soldiers die PERMANENTLY in a single hit, and the survivors carry between missions and earn promotions (more HP, range and accuracy) — a fallen name goes on the casualty ticker for good. One soldier sprite is pal()-recolored into green squad vs red enemies, and one green tileset is pal()-swapped per biome (jungle, desert, snow). Big fillp+circfill+shake explosions, blood and dirt particles, a grim man-down sting, and a sparse euclid snare bed. Roster persists between runs via save_bytes. LEFT-click move, RIGHT-click grenade, SPACE halt, R restart."
}
de:meta */
#include "studio.h"
#include <stddef.h>   // NULL

// CANNON FODDER — top-down run-and-gun squad tactics, mouse-commanded.
//
// You lead a tiny squad across three scrolling battlefields. Soldiers die
// PERMANENTLY and the survivors carry over between missions — that's the whole
// bittersweet hook. Blow the huts and gun-nests to rubble, then walk a survivor
// to the green exit.
//
//   LEFT-CLICK ground  — order the squad to move there (leader + a loose trail)
//   RIGHT-CLICK        — the leader lobs a grenade (arcs in, big AoE)
//   your soldiers AUTO-FIRE their rifles at any enemy in range + line of sight
//   SPACE  halt   ·   R  restart the campaign
//
// One tileset is recolored per biome with pal() (jungle → desert → snow). Tiny
// pal()-tinted soldiers (crowd.c idiom), robotron.c-style entity pools, big
// fillp+circfill+shake explosions. Roster persists between runs via save_bytes.

// ---- map tiles (these double as sprite-sheet slots) ----
#define T_GROUND 8
#define T_WATER  9
#define T_TREE   10
#define T_HUT    11
#define T_NEST   12
#define T_EXIT   13

// ---- world (cells) ----
#define MW 24
#define MH 16
#define WORLD_W (MW * 16)
#define WORLD_H (MH * 16)

// ---- soldier sprites + magic recolor placeholders ----
#define SPR_STAND 0
#define SPR_WALK  1
#define MAGIC_UNI 28        // uniform pixels — pal()'d to the team color
#define MAGIC_HEL 29        // helmet pixels — pal()'d to mark leader / team

// ---- pools ----
#define MAXSQUAD  5
#define MAXENE    18
#define MAXBUL    96
#define MAXGREN   6
#define MAXPART   160
#define MAXEXP    14
#define MAXTGT    24
#define MAXCRAT   28
#define NUM_MISSIONS 3

#define SAVE_MAGIC 0x4346   // 'CF'

enum { ST_BRIEF, ST_PLAY, ST_WIN, ST_FAIL, ST_CAMPAIGN };
enum { K_SMOKE, K_BLOOD, K_DIRT, K_SPARK };

typedef struct { int alive, rank, name_id; } Trooper;     // persistent identity
typedef struct { float x, y; int hp; float aim, fire_cd, flash, muzzle; } Unit;
typedef struct { bool on; float x, y; int hp; float aim, fire_cd, flash, muzzle;
                 float hx, hy, wt; bool alert; } Enemy;   // hx,hy = home; wanders, then chases
typedef struct { bool on; float x, y, vx, vy, life; bool enemy; } Bullet;
typedef struct { bool on; float x, y, sx, sy, tx, ty, t, dur; } Grenade;
typedef struct { bool on; float x, y, vx, vy, life, maxlife, size; int color, kind; } Part;
typedef struct { bool on; float x, y, t, dur, r; } Expl;
typedef struct { bool on; int cx, cy, hp; bool nest; float fire_cd; } Target;
typedef struct { bool on; float x, y, r; } Crater;

// ---- persistent + campaign state ----
static Trooper roster[MAXSQUAD];
static int  cur_mission, total_kia, state, biome;
static const char *mission_name, *mission_obj;
static int  spawn_cx, spawn_cy;

// ---- live pools ----
static Unit    squad[MAXSQUAD];
static Enemy   enes[MAXENE];
static Bullet  buls[MAXBUL];
static Grenade grens[MAXGREN];
static Part    parts[MAXPART];
static Expl    expls[MAXEXP];
static Target  tgts[MAXTGT];
static Crater  crats[MAXCRAT];

static int   cam_x, cam_y;
static float mvx, mvy;          // squad move order (world coords)
static bool  has_move;
static float gren_cd, flash_t, gun_sfx_cd;
static float mandown_t, promo_t, win_t;
static char  mandown_name[10];
static int   kia_ring[8], kia_n;
static int   last_beat = -1;

static const char *NAMES[10] = {
    "JONES","SMITH","WUI","BROWN","DUTCH","RICO","MAC","HUDSON","VASQ","DRAKE"
};
static const char *RANKS[5] = { "PVT","CPL","SGT","LT","CPT" };

// =====================================================================
// helpers
// =====================================================================

static int leader_idx(void) {
    for (int i = 0; i < MAXSQUAD; i++) if (roster[i].alive) return i;
    return -1;
}
static int alive_squad(void) {
    int n = 0;
    for (int i = 0; i < MAXSQUAD; i++) if (roster[i].alive) n++;
    return n;
}
static int targets_left(void) {
    int n = 0;
    for (int i = 0; i < MAXTGT; i++) if (tgts[i].on) n++;
    return n;
}

static bool walkable(float px, float py) {
    if (px < 4 || py < 4 || px > WORLD_W - 4 || py > WORLD_H - 4) return false;
    int t = tile_at((int)px, (int)py);
    return t == T_GROUND || t == T_EXIT;
}

// line of sight: clear unless a tree / building blocks it
static bool los(float x1, float y1, float x2, float y2) {
    float d = distance((int)x1, (int)y1, (int)x2, (int)y2);
    int n = (int)(d / 6.0f);
    if (n < 1) return true;
    for (int k = 1; k < n; k++) {
        float t = (float)k / n;
        int t2 = tile_at((int)lerp(x1, x2, t), (int)lerp(y1, y2, t));
        if (t2 == T_TREE || t2 == T_HUT || t2 == T_NEST) return false;
    }
    return true;
}

static void add_part(float x, float y, float vx, float vy, float grav,
                     float life, float size, int color, int kind) {
    for (int i = 0; i < MAXPART; i++) if (!parts[i].on) {
        parts[i] = (Part){ true, x, y, vx, vy + 0, life, life, size, color, kind };
        parts[i].vx = vx; parts[i].vy = vy;          // (grav folded into update via kind)
        (void)grav;
        return;
    }
}

static void add_bullet(float x, float y, float deg, bool enemy) {
    float spd = enemy ? 130.0f : 175.0f;
    for (int i = 0; i < MAXBUL; i++) if (!buls[i].on) {
        buls[i] = (Bullet){ true, x, y, dx(spd, deg), dy(spd, deg), 0.9f, enemy };
        return;
    }
}

static void add_crater(float x, float y, float r) {
    for (int i = 0; i < MAXCRAT; i++) if (!crats[i].on) {
        crats[i] = (Crater){ true, x, y, r };
        return;
    }
    crats[rnd(MAXCRAT)] = (Crater){ true, x, y, r };   // recycle oldest-ish
}

static Target *target_at(int cx, int cy) {
    for (int i = 0; i < MAXTGT; i++)
        if (tgts[i].on && tgts[i].cx == cx && tgts[i].cy == cy) return &tgts[i];
    return NULL;
}

static void kill_enemy(Enemy *e) {
    e->on = false;
    for (int k = 0; k < 8; k++) {
        float a = rnd(360), s = rnd_float_between(30, 90);
        add_part(e->x, e->y, dx(s, a), dy(s, a), 1, rnd_float_between(0.3f, 0.6f),
                 rnd_float_between(1, 2), CLR_RED, K_BLOOD);
    }
    hit(40, INSTR_NOISE, 3, 70);
}

static void kill_trooper(int i) {
    if (!roster[i].alive) return;
    roster[i].alive = 0;
    for (int k = 0; k < 10; k++) {
        float a = rnd(360), s = rnd_float_between(30, 100);
        add_part(squad[i].x, squad[i].y, dx(s, a), dy(s, a), 1,
                 rnd_float_between(0.3f, 0.7f), rnd_float_between(1, 2), CLR_RED, K_BLOOD);
    }
    int n = roster[i].name_id;
    const char *nm = NAMES[n];
    int j = 0; for (; nm[j] && j < 9; j++) mandown_name[j] = nm[j];
    mandown_name[j] = 0;
    mandown_t = 2.6f;
    if (kia_n < 8) kia_ring[kia_n++] = n;
    total_kia++;
    shake(5);
    // a grim descending sting
    schedule(0,   55, INSTR_SAW, 4);
    schedule(120, 50, INSTR_SAW, 4);
    schedule(260, 43, INSTR_SAW, 5);
}

static void save_game(void);

// big bang: visuals + AoE damage. friendly => own squad takes a hit too.
static void do_explosion(float x, float y, float r, int power, bool friendly) {
    for (int i = 0; i < MAXEXP; i++) if (!expls[i].on) {
        expls[i] = (Expl){ true, x, y, 0, 0.42f, r }; break;
    }
    shake(clamp(r * 0.32f, 2, 9));
    flash_t = clamp(flash_t + r * 0.012f, 0, 0.7f);
    add_crater(x, y, r * 0.45f);
    for (int k = 0; k < 16; k++) {
        float a = rnd(360), s = rnd_float_between(20, r * 4);
        add_part(x, y, dx(s, a), dy(s, a), -1, rnd_float_between(0.4f, 0.9f),
                 rnd_float_between(2, 4), (k % 3 == 0) ? CLR_DARK_GREY : CLR_LIGHT_GREY, K_SMOKE);
    }
    for (int k = 0; k < 10; k++) {
        float a = rnd(360), s = rnd_float_between(60, r * 6);
        add_part(x, y, dx(s, a), dy(s, a), 1, rnd_float_between(0.15f, 0.4f),
                 rnd_float_between(1, 2), (k & 1) ? CLR_ORANGE : CLR_YELLOW, K_SPARK);
    }
    for (int i = 0; i < MAXENE; i++)
        if (enes[i].on && near((int)x, (int)y, (int)enes[i].x, (int)enes[i].y, (int)r)) {
            enes[i].hp -= power; enes[i].flash = 0.12f;
            if (enes[i].hp <= 0) kill_enemy(&enes[i]);
        }
    for (int i = 0; i < MAXTGT; i++)
        if (tgts[i].on) {
            int tx = tgts[i].cx * 16 + 8, ty = tgts[i].cy * 16 + 8;
            if (near((int)x, (int)y, tx, ty, (int)r + 6)) tgts[i].hp -= power;
        }
    if (friendly)
        for (int i = 0; i < MAXSQUAD; i++)
            if (roster[i].alive && near((int)x, (int)y, (int)squad[i].x, (int)squad[i].y, (int)r)) {
                squad[i].hp -= power; squad[i].flash = 0.15f;
                if (squad[i].hp <= 0) kill_trooper(i);
            }
}

static void destroy_target(int i) {
    Target *t = &tgts[i];
    int x = t->cx * 16 + 8, y = t->cy * 16 + 8;
    mset(t->cx, t->cy, T_GROUND);
    t->on = false;
    do_explosion(x, y, t->nest ? 18 : 22, 2, false);   // rubble blast (won't re-trigger targets)
    hit(30, INSTR_NOISE, 6, 320);
    note(36, INSTR_SINE, 6);
}

// =====================================================================
// missions
// =====================================================================

static int n_enemies;
static struct { int cx, cy; } enemy_spawns[MAXENE];

static void fill_tiles(int x, int y, int w, int h, int t) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            if (xx >= 0 && xx < MW && yy >= 0 && yy < MH) mset(xx, yy, t);
}

static void clear_pools(void) {
    for (int i = 0; i < MAXENE;  i++) enes[i].on  = false;
    for (int i = 0; i < MAXBUL;  i++) buls[i].on  = false;
    for (int i = 0; i < MAXGREN; i++) grens[i].on = false;
    for (int i = 0; i < MAXPART; i++) parts[i].on = false;
    for (int i = 0; i < MAXEXP;  i++) expls[i].on = false;
    for (int i = 0; i < MAXTGT;  i++) tgts[i].on  = false;
    for (int i = 0; i < MAXCRAT; i++) crats[i].on = false;
    has_move = false; gren_cd = 0; flash_t = 0;
    mandown_t = 0; promo_t = 0; win_t = 0; kia_n = 0;
}

static void add_enemy_spawn(int cx, int cy) {
    if (n_enemies < MAXENE) { enemy_spawns[n_enemies].cx = cx; enemy_spawns[n_enemies].cy = cy; n_enemies++; }
}

static void build_mission(int m) {
    clear_pools();
    n_enemies = 0;
    fill_tiles(0, 0, MW, MH, T_GROUND);
    // tree border so the playfield is bounded
    fill_tiles(0, 0, MW, 1, T_TREE);
    fill_tiles(0, MH - 1, MW, 1, T_TREE);
    fill_tiles(0, 0, 1, MH, T_TREE);
    fill_tiles(MW - 1, 0, 1, MH, T_TREE);

    if (m == 0) {
        biome = 0; mission_name = "MISSION 1 - THE DELTA";
        mission_obj = "Level the village. Reach the exit.";
        spawn_cx = 2; spawn_cy = 13;
        // a river splitting the map, with a crossing
        fill_tiles(11, 1, 2, 6, T_WATER);
        fill_tiles(11, 10, 2, 5, T_WATER);
        // tree cover
        fill_tiles(5, 4, 2, 2, T_TREE); fill_tiles(17, 9, 2, 2, T_TREE);
        mset(8, 11, T_TREE); mset(20, 3, T_TREE); mset(6, 8, T_TREE);
        // target huts + a nest
        mset(4, 2, T_HUT); mset(5, 2, T_HUT); mset(4, 3, T_HUT);
        mset(18, 5, T_HUT); mset(19, 5, T_HUT);
        mset(16, 11, T_NEST);
        mset(22, 14, T_EXIT);
        add_enemy_spawn(15, 4); add_enemy_spawn(18, 8);
        add_enemy_spawn(8, 12);  add_enemy_spawn(20, 12);
    } else if (m == 1) {
        biome = 1; mission_name = "MISSION 2 - SCORCHED SANDS";
        mission_obj = "Knock out the gun nests. Get out east.";
        spawn_cx = 2; spawn_cy = 8;
        mset(7, 4, T_TREE); mset(7, 11, T_TREE); mset(14, 6, T_TREE); mset(14, 9, T_TREE);
        fill_tiles(10, 7, 2, 2, T_WATER);              // an oasis
        mset(6, 3, T_NEST); mset(6, 12, T_NEST); mset(18, 7, T_NEST);
        mset(16, 3, T_HUT); mset(17, 3, T_HUT); mset(16, 12, T_HUT);
        mset(22, 8, T_EXIT);
        add_enemy_spawn(9, 3); add_enemy_spawn(9, 12); add_enemy_spawn(15, 7);
        add_enemy_spawn(19, 4); add_enemy_spawn(19, 11); add_enemy_spawn(13, 8);
    } else {
        biome = 2; mission_name = "MISSION 3 - DEAD WINTER";
        mission_obj = "Take the fortress. No survivors but ours.";
        spawn_cx = 2; spawn_cy = 2;
        fill_tiles(8, 5, 1, 7, T_TREE); fill_tiles(15, 4, 1, 7, T_TREE);
        mset(5, 8, T_TREE); mset(12, 3, T_TREE); mset(19, 11, T_TREE);
        fill_tiles(11, 8, 3, 3, T_HUT);                // the fortress block
        mset(11, 7, T_NEST); mset(13, 7, T_NEST);
        mset(4, 13, T_HUT); mset(20, 4, T_HUT);
        mset(21, 13, T_EXIT);
        add_enemy_spawn(10, 5);  add_enemy_spawn(14, 5); add_enemy_spawn(12, 12);
        add_enemy_spawn(6, 11);  add_enemy_spawn(18, 9); add_enemy_spawn(18, 13);
        add_enemy_spawn(5, 5);   add_enemy_spawn(20, 6);
    }

    // collect targets from the grid
    int ti = 0;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            int t = mget(x, y);
            if ((t == T_HUT || t == T_NEST) && ti < MAXTGT) {
                tgts[ti] = (Target){ true, x, y, t == T_NEST ? 5 : 6, t == T_NEST, rnd_float_between(0.5f, 1.5f) };
                ti++;
            }
        }

    // spawn enemies (hp scales a touch with mission)
    for (int i = 0; i < n_enemies; i++) {
        Enemy *e = &enes[i];
        e->on = true;
        e->x = enemy_spawns[i].cx * 16 + 8;
        e->y = enemy_spawns[i].cy * 16 + 8;
        e->hx = e->x; e->hy = e->y;
        e->hp = 1 + (m >= 2 ? 1 : 0);
        e->aim = rnd(360); e->fire_cd = rnd_float_between(0.5f, 1.5f);
        e->wt = rnd_float_between(0, 2); e->alert = false;
        e->flash = e->muzzle = 0;
    }

    // place the living squad near the spawn
    int slot = 0;
    for (int i = 0; i < MAXSQUAD; i++) {
        if (!roster[i].alive) continue;
        squad[i].x = spawn_cx * 16 + 8 + (slot % 2) * 10 - 5;
        squad[i].y = spawn_cy * 16 + 8 + (slot / 2) * 10;
        squad[i].hp = 1 + roster[i].rank;
        squad[i].aim = 0; squad[i].fire_cd = 0; squad[i].flash = squad[i].muzzle = 0;
        slot++;
    }
}

static void enter_mission(int m) {
    cur_mission = m;
    build_mission(m);
    state = ST_BRIEF;
}

static void new_campaign(void) {
    for (int i = 0; i < MAXSQUAD; i++)
        roster[i] = (Trooper){ 1, 0, i };
    cur_mission = 0; total_kia = 0;
    save_game();
    enter_mission(0);
}

static void save_game(void) {
    struct { int magic, mission, kia; Trooper r[MAXSQUAD]; } s;
    s.magic = SAVE_MAGIC; s.mission = cur_mission; s.kia = total_kia;
    for (int i = 0; i < MAXSQUAD; i++) s.r[i] = roster[i];
    save_bytes(&s, sizeof s);
}

// =====================================================================
// init
// =====================================================================

void init(void) {
    colorkey(0);
    bpm(104);
    struct { int magic, mission, kia; Trooper r[MAXSQUAD]; } s;
    if (load_bytes(&s, sizeof s) == (int)sizeof s && s.magic == SAVE_MAGIC) {
        cur_mission = clamp(s.mission, 0, NUM_MISSIONS - 1);
        total_kia = s.kia;
        for (int i = 0; i < MAXSQUAD; i++) roster[i] = s.r[i];
        if (alive_squad() == 0) new_campaign();
        else enter_mission(cur_mission);
    } else {
        new_campaign();
    }
}

// =====================================================================
// update
// =====================================================================

static void update_play(void) {
    float d = dt();
    if (flash_t > 0)   flash_t   = clamp(flash_t - d * 2.2f, 0, 1);
    if (gren_cd > 0)   gren_cd  -= d;
    if (gun_sfx_cd > 0) gun_sfx_cd -= d;
    if (mandown_t > 0) mandown_t -= d;

    int lead = leader_idx();
    if (lead < 0) { state = ST_FAIL; return; }

    // ---- commands ----
    int wmx = mouse_x() + cam_x, wmy = mouse_y() + cam_y;
    if (mouse_pressed(MOUSE_LEFT)) { mvx = wmx; mvy = wmy; has_move = true; }
    if (keyp(KEY_SPACE)) has_move = false;
    if (mouse_pressed(MOUSE_RIGHT) && gren_cd <= 0) {
        float ang = angle_to((int)squad[lead].x, (int)squad[lead].y, wmx, wmy);
        float dist = clamp(distance((int)squad[lead].x, (int)squad[lead].y, wmx, wmy), 8, 96);
        for (int i = 0; i < MAXGREN; i++) if (!grens[i].on) {
            grens[i] = (Grenade){ true, squad[lead].x, squad[lead].y,
                                  squad[lead].x, squad[lead].y,
                                  squad[lead].x + dx(dist, ang), squad[lead].y + dy(dist, ang),
                                  0, 0.72f };
            gren_cd = 1.0f;
            hit(52, INSTR_NOISE, 3, 45);
            break;
        }
    }

    // ---- squad movement: leader to the order, others trail the one ahead ----
    int prev = -1;
    for (int i = 0; i < MAXSQUAD; i++) {
        if (!roster[i].alive) continue;
        Unit *u = &squad[i];
        float tx, ty, stop;
        if (prev < 0) { tx = mvx; ty = mvy; stop = 3; }            // leader
        else          { tx = squad[prev].x; ty = squad[prev].y; stop = 13; }
        bool want = (prev < 0) ? has_move : true;
        if (u->flash > 0) u->flash -= d;
        if (u->muzzle > 0) u->muzzle -= d;
        float dd = distance((int)u->x, (int)u->y, (int)tx, (int)ty);
        if (want && dd > stop) {
            float ang = angle_to((int)u->x, (int)u->y, (int)tx, (int)ty);
            float step = 44.0f * d;
            float nx = u->x + dx(step, ang), ny = u->y + dy(step, ang);
            if (walkable(nx, u->y)) u->x = nx;
            if (walkable(u->x, ny)) u->y = ny;
        } else if (prev < 0 && dd <= stop) {
            has_move = false;                                       // leader arrived
        }
        prev = i;
    }

    // ---- squad auto-fire at the nearest visible enemy ----
    for (int i = 0; i < MAXSQUAD; i++) {
        if (!roster[i].alive) continue;
        Unit *u = &squad[i];
        if (u->fire_cd > 0) u->fire_cd -= d;
        int best = -1; float bd = 9999;
        for (int j = 0; j < MAXENE; j++) {
            if (!enes[j].on) continue;
            float dj = distance((int)u->x, (int)u->y, (int)enes[j].x, (int)enes[j].y);
            if (dj < bd) { bd = dj; best = j; }
        }
        if (best >= 0) {
            u->aim = angle_to((int)u->x, (int)u->y, (int)enes[best].x, (int)enes[best].y);
            float range = 70 + roster[i].rank * 6;
            if (bd < range && los(u->x, u->y, enes[best].x, enes[best].y) && u->fire_cd <= 0) {
                float spread = (4 - roster[i].rank) * 2.5f;
                add_bullet(u->x + dx(7, u->aim), u->y + dy(7, u->aim),
                           u->aim + rnd_float_between(-spread, spread), false);
                u->fire_cd = 0.55f - roster[i].rank * 0.06f;
                u->muzzle = 0.06f;
                if (gun_sfx_cd <= 0) { hit(70 + rnd(8), INSTR_NOISE, 2, 24); gun_sfx_cd = 0.05f; }
            }
        } else {
            u->aim = angle_to((int)u->x, (int)u->y, mouse_x() + cam_x, mouse_y() + cam_y);
        }
    }

    // ---- enemies: wander home, then spot, chase and shoot ----
    for (int i = 0; i < MAXENE; i++) {
        Enemy *e = &enes[i];
        if (!e->on) continue;
        if (e->flash > 0)  e->flash -= d;
        if (e->muzzle > 0) e->muzzle -= d;
        if (e->fire_cd > 0) e->fire_cd -= d;

        int tgt = -1; float bd = 9999;
        for (int j = 0; j < MAXSQUAD; j++) {
            if (!roster[j].alive) continue;
            float dj = distance((int)e->x, (int)e->y, (int)squad[j].x, (int)squad[j].y);
            if (dj < bd && dj < 100 && los(e->x, e->y, squad[j].x, squad[j].y)) { bd = dj; tgt = j; }
        }
        if (tgt >= 0) {
            e->alert = true;
            e->aim = angle_to((int)e->x, (int)e->y, (int)squad[tgt].x, (int)squad[tgt].y);
            if (bd > 48) {                                          // close the distance
                float step = 30.0f * d;
                float nx = e->x + dx(step, e->aim), ny = e->y + dy(step, e->aim);
                if (walkable(nx, e->y)) e->x = nx;
                if (walkable(e->x, ny)) e->y = ny;
            }
            if (e->fire_cd <= 0) {
                add_bullet(e->x + dx(7, e->aim), e->y + dy(7, e->aim),
                           e->aim + rnd_float_between(-7, 7), true);
                e->fire_cd = rnd_float_between(0.9f, 1.6f);
                e->muzzle = 0.06f;
            }
        } else {
            e->wt -= d;                                             // idle wander near home
            if (e->wt <= 0) {
                e->wt = rnd_float_between(0.8f, 2.2f);
                e->aim = rnd(360);
            }
            float step = 14.0f * d;
            float nx = e->x + dx(step, e->aim), ny = e->y + dy(step, e->aim);
            if (distance((int)nx, (int)ny, (int)e->hx, (int)e->hy) < 40 && walkable(nx, ny)) {
                e->x = nx; e->y = ny;
            } else e->wt = 0;
        }
    }

    // ---- bullets ----
    for (int i = 0; i < MAXBUL; i++) {
        Bullet *b = &buls[i];
        if (!b->on) continue;
        b->x += b->vx * d; b->y += b->vy * d;
        b->life -= d;
        if (b->life <= 0 || b->x < 0 || b->y < 0 || b->x > WORLD_W || b->y > WORLD_H) { b->on = false; continue; }
        int t = tile_at((int)b->x, (int)b->y);
        if (t == T_TREE) { b->on = false; add_part(b->x, b->y, 0, 0, 0, 0.2f, 1, CLR_DARK_GREEN, K_DIRT); continue; }
        if (t == T_HUT || t == T_NEST) {
            Target *tg = target_at((int)b->x / 16, (int)b->y / 16);
            if (tg) { tg->hp -= 1; add_part(b->x, b->y, 0, 0, 0, 0.2f, 1, CLR_LIGHT_GREY, K_DIRT); }
            b->on = false; continue;
        }
        if (!b->enemy) {
            for (int j = 0; j < MAXENE; j++)
                if (enes[j].on && near((int)b->x, (int)b->y, (int)enes[j].x, (int)enes[j].y, 6)) {
                    enes[j].hp--; enes[j].flash = 0.12f; b->on = false;
                    add_part(b->x, b->y, 0, 0, 0, 0.15f, 1, CLR_RED, K_BLOOD);
                    if (enes[j].hp <= 0) kill_enemy(&enes[j]);
                    break;
                }
        } else {
            for (int j = 0; j < MAXSQUAD; j++)
                if (roster[j].alive && near((int)b->x, (int)b->y, (int)squad[j].x, (int)squad[j].y, 5)) {
                    squad[j].hp--; squad[j].flash = 0.15f; b->on = false; shake(2);
                    add_part(b->x, b->y, 0, 0, 0, 0.15f, 1, CLR_RED, K_BLOOD);
                    if (squad[j].hp <= 0) kill_trooper(j);
                    break;
                }
        }
    }

    // ---- grenades (faked arc) ----
    for (int i = 0; i < MAXGREN; i++) {
        Grenade *g = &grens[i];
        if (!g->on) continue;
        g->t += d / g->dur;
        g->x = lerp(g->sx, g->tx, g->t);
        g->y = lerp(g->sy, g->ty, g->t);
        if (g->t >= 1) { g->on = false; do_explosion(g->tx, g->ty, 26, 3, true); }
    }

    // ---- destroy depleted targets (nests can fire while alive) ----
    for (int i = 0; i < MAXTGT; i++) {
        if (!tgts[i].on) continue;
        if (tgts[i].hp <= 0) { destroy_target(i); continue; }
        if (tgts[i].nest) {
            tgts[i].fire_cd -= d;
            int ex = tgts[i].cx * 16 + 8, ey = tgts[i].cy * 16 + 8;
            int tgt = -1; float bd = 9999;
            for (int j = 0; j < MAXSQUAD; j++) {
                if (!roster[j].alive) continue;
                float dj = distance(ex, ey, (int)squad[j].x, (int)squad[j].y);
                if (dj < bd && dj < 110 && los(ex, ey, squad[j].x, squad[j].y)) { bd = dj; tgt = j; }
            }
            if (tgt >= 0 && tgts[i].fire_cd <= 0) {
                float ang = angle_to(ex, ey, (int)squad[tgt].x, (int)squad[tgt].y);
                add_bullet(ex + dx(8, ang), ey + dy(8, ang), ang + rnd_float_between(-5, 5), true);
                tgts[i].fire_cd = rnd_float_between(0.8f, 1.4f);
            }
        }
    }

    // ---- particles + explosions ----
    for (int i = 0; i < MAXPART; i++) {
        Part *p = &parts[i];
        if (!p->on) continue;
        p->x += p->vx * d; p->y += p->vy * d;
        if (p->kind == K_SMOKE) { p->vy -= 18 * d; p->vx *= 0.94f; }
        else if (p->kind == K_SPARK || p->kind == K_BLOOD) p->vy += 80 * d;
        p->vx *= 0.97f;
        p->life -= d;
        if (p->life <= 0) p->on = false;
    }
    for (int i = 0; i < MAXEXP; i++) {
        if (!expls[i].on) continue;
        expls[i].t += d / expls[i].dur;
        if (expls[i].t >= 1) expls[i].on = false;
    }

    // ---- ambient militaristic bed (once per beat) ----
    int bt = beat();
    if (bt != last_beat) {
        last_beat = bt;
        if (every(2))            hit(31, INSTR_SINE, 1, 130);          // low pulse
        if (euclid(5, 16, bt))   hit(64, INSTR_NOISE, 1, 18);          // sparse snare
        if (euclid(8, 16, bt))   hit(80, INSTR_NOISE, 1, 8);           // hat
    }

    // ---- win / lose ----
    if (alive_squad() == 0) { state = ST_FAIL; return; }
    if (targets_left() == 0) {
        state = ST_WIN; win_t = 0;
        for (int i = 0; i < MAXSQUAD; i++)
            if (roster[i].alive && roster[i].rank < 4) roster[i].rank++;   // promotions
        promo_t = 2.0f;
        save_game();
        note(72, INSTR_SINE, 5); schedule(120, 76, INSTR_SINE, 5); schedule(240, 79, INSTR_SINE, 6);
    }
}

void update(void) {
    if (state == ST_BRIEF) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE) || keyp(KEY_ENTER)) state = ST_PLAY;
    } else if (state == ST_PLAY) {
        update_play();
    } else if (state == ST_WIN) {
        win_t += dt();
        if (win_t > 0.4f && (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE))) {
            int next = cur_mission + 1;
            if (next >= NUM_MISSIONS) { state = ST_CAMPAIGN; }
            else { enter_mission(next); save_game(); }
        }
    } else { // ST_FAIL or ST_CAMPAIGN
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE) || keyp('R')) new_campaign();
    }
    if (keyp('R')) new_campaign();
}

// =====================================================================
// draw
// =====================================================================

static void apply_biome(int b) {
    if (b == 1) { pal(CLR_DARK_GREEN, CLR_BROWN); pal(CLR_MEDIUM_GREEN, CLR_ORANGE); pal(CLR_GREEN, CLR_LIGHT_YELLOW); }
    else if (b == 2) { pal(CLR_DARK_GREEN, CLR_LIGHT_GREY); pal(CLR_MEDIUM_GREEN, CLR_LIGHT_GREY); pal(CLR_GREEN, CLR_WHITE); }
}

static void draw_soldier(float fx, float fy, int uni, int hel, float aim,
                         float flash, float muzzle, bool firing) {
    int x = (int)fx, y = (int)fy;
    int step = (anim(2, 6.0f, fx * 0.013f) == 0) ? SPR_STAND : SPR_WALK;
    ovalfill(x, y + 6, 5, 2, CLR_BROWNISH_BLACK);             // shadow
    if (flash > 0) { pal(MAGIC_UNI, CLR_WHITE); pal(MAGIC_HEL, CLR_WHITE); pal(15, CLR_WHITE); pal(CLR_BROWN, CLR_WHITE); }
    else           { pal(MAGIC_UNI, uni);       pal(MAGIC_HEL, hel); }
    spr(step, x - 8, y - 8);
    pal_reset();
    // rifle points where the soldier aims
    int gx = x + (int)dx(8, aim), gy = y + (int)dy(8, aim);
    line(x, y, gx, gy, CLR_DARK_GREY);
    if (firing && muzzle > 0) { circfill(gx, gy, 2, CLR_YELLOW); pset(gx, gy, CLR_WHITE); }
}

static void draw_world(void) {
    int bg = (biome == 2) ? CLR_LIGHT_GREY : (biome == 1 ? CLR_BROWN : CLR_DARK_GREEN);
    cls(bg);

    int lead = leader_idx();
    float lx = lead >= 0 ? squad[lead].x : spawn_cx * 16 + 8;
    float ly = lead >= 0 ? squad[lead].y : spawn_cy * 16 + 8;
    cam_x = (int)clamp(lx - SCREEN_W / 2, 0, WORLD_W - SCREEN_W);
    cam_y = (int)clamp(ly - SCREEN_H / 2, 0, WORLD_H - SCREEN_H);
    camera(cam_x, cam_y);

    apply_biome(biome);
    map(0, 0, 0, 0, MW, MH);
    pal_reset();

    // craters
    for (int i = 0; i < MAXCRAT; i++)
        if (crats[i].on) {
            ovalfill((int)crats[i].x, (int)crats[i].y, (int)crats[i].r, (int)(crats[i].r * 0.7f), CLR_BROWNISH_BLACK);
            ovalfill((int)crats[i].x, (int)crats[i].y, (int)crats[i].r - 1, (int)(crats[i].r * 0.5f), CLR_DARK_BROWN);
        }

    // damaged-target health pips
    for (int i = 0; i < MAXTGT; i++)
        if (tgts[i].on) {
            int mx = tgts[i].cx * 16, my = tgts[i].cy * 16;
            int full = tgts[i].nest ? 5 : 6;
            if (tgts[i].hp < full)
                bar(mx + 2, my - 3, 12, 2, (float)tgts[i].hp / full, CLR_RED, CLR_DARKER_GREY);
        }

    // move-order flag
    if (has_move && state == ST_PLAY) {
        int fxp = (int)mvx, fyp = (int)mvy;
        line(fxp, fyp - 8, fxp, fyp, CLR_WHITE);
        trifill(fxp, fyp - 8, fxp + 6, fyp - 6, fxp, fyp - 4, CLR_GREEN);
    }

    // grenades (shadow + arcing pip)
    for (int i = 0; i < MAXGREN; i++)
        if (grens[i].on) {
            float h = sin_deg(grens[i].t * 180) * 14;
            ovalfill((int)grens[i].x, (int)grens[i].y, 2, 1, CLR_BROWNISH_BLACK);
            circfill((int)grens[i].x, (int)(grens[i].y - h), 2, CLR_DARK_GREEN);
            pset((int)grens[i].x, (int)(grens[i].y - h) - 1, CLR_GREEN);
        }

    // enemies
    for (int i = 0; i < MAXENE; i++)
        if (enes[i].on)
            draw_soldier(enes[i].x, enes[i].y, CLR_RED, CLR_DARK_RED,
                         enes[i].aim, enes[i].flash, enes[i].muzzle, enes[i].muzzle > 0);

    // squad (leader gets a yellow helmet + a marker)
    for (int i = 0; i < MAXSQUAD; i++) {
        if (!roster[i].alive) continue;
        bool is_lead = (i == lead);
        draw_soldier(squad[i].x, squad[i].y, CLR_GREEN, is_lead ? CLR_YELLOW : CLR_DARK_GREEN,
                     squad[i].aim, squad[i].flash, squad[i].muzzle, squad[i].muzzle > 0);
        if (is_lead) trifill((int)squad[i].x, (int)squad[i].y - 12,
                             (int)squad[i].x - 3, (int)squad[i].y - 16,
                             (int)squad[i].x + 3, (int)squad[i].y - 16, CLR_YELLOW);
    }

    // bullets
    for (int i = 0; i < MAXBUL; i++)
        if (buls[i].on) {
            int c = buls[i].enemy ? CLR_RED : CLR_YELLOW;
            line((int)buls[i].x, (int)buls[i].y,
                 (int)(buls[i].x - buls[i].vx * 0.02f), (int)(buls[i].y - buls[i].vy * 0.02f), c);
        }

    // particles
    for (int i = 0; i < MAXPART; i++) {
        Part *p = &parts[i];
        if (!p->on) continue;
        if (p->kind == K_SMOKE) {
            int r = (int)(p->size * (1.4f - p->life / p->maxlife) + 1);
            fillp(FILL_CHECKER, -1);
            circfill((int)p->x, (int)p->y, r, p->color);
            fillp_reset();
        } else {
            circfill((int)p->x, (int)p->y, (int)p->size, p->color);
        }
    }

    // explosions (expanding fireball)
    for (int i = 0; i < MAXEXP; i++) {
        if (!expls[i].on) continue;
        float a = ease_out(expls[i].t);
        int r = (int)(expls[i].r * a);
        fillp(FILL_CHECKER, -1);
        circfill((int)expls[i].x, (int)expls[i].y, r + 3, CLR_DARK_GREY);
        fillp_reset();
        circfill((int)expls[i].x, (int)expls[i].y, (int)(r * 0.7f), CLR_ORANGE);
        circfill((int)expls[i].x, (int)expls[i].y, (int)(r * 0.4f), CLR_YELLOW);
    }

    camera(0, 0);
}

static void draw_hud(void) {
    // top bar
    rectfill(0, 0, SCREEN_W, 11, CLR_BROWNISH_BLACK);
    print(mission_name, 3, 2, CLR_LIGHT_YELLOW);
    print_right(str("TARGETS %d", targets_left()), SCREEN_W - 3, 2, CLR_ORANGE);

    // bottom roster strip
    rectfill(0, SCREEN_H - 11, SCREEN_W, 11, CLR_BROWNISH_BLACK);
    int lead = leader_idx();
    for (int i = 0; i < MAXSQUAD; i++) {
        int x = 4 + i * 63, y = SCREEN_H - 9;
        if (!roster[i].alive) {
            print(str("%s KIA", NAMES[roster[i].name_id]), x, y, CLR_DARK_GREY);
            continue;
        }
        int uc = (i == lead) ? CLR_YELLOW : CLR_GREEN;
        rectfill(x, y, 3, 3, uc);
        print(str("%s %s", RANKS[roster[i].rank], NAMES[roster[i].name_id]), x + 5, y, CLR_WHITE);
        for (int h = 0; h < squad[i].hp; h++) circfill(x + 6 + h * 4, y + 8, 1, CLR_RED);
    }

    // casualty ticker (this campaign)
    if (kia_n > 0) {
        char line_buf[40]; int p = 0;
        const char *pre = "KIA: ";
        for (int k = 0; pre[k] && p < 38; k++) line_buf[p++] = pre[k];
        for (int i = 0; i < kia_n && p < 34; i++) {
            const char *nm = NAMES[kia_ring[i]];
            for (int k = 0; nm[k] && p < 38; k++) line_buf[p++] = nm[k];
            if (i < kia_n - 1 && p < 37) line_buf[p++] = ' ';
        }
        line_buf[p] = 0;
        print(line_buf, 3, 13, CLR_RED);
    }

    // crosshair
    int mx = mouse_x(), my = mouse_y();
    int cc = (gren_cd <= 0) ? CLR_GREEN : CLR_RED;
    circ(mx, my, 4, cc);
    line(mx - 6, my, mx - 2, my, cc); line(mx + 2, my, mx + 6, my, cc);
    line(mx, my - 6, mx, my - 2, cc); line(mx, my + 2, mx, my + 6, cc);

    // man-down banner
    if (mandown_t > 0) {
        int w = 120;
        rectfill(SCREEN_W / 2 - w / 2, 30, w, 22, CLR_BLACK);
        rect(SCREEN_W / 2 - w / 2, 30, w, 22, CLR_RED);
        print_centered("MAN DOWN", SCREEN_W/2, 34, CLR_RED);
        print_centered(mandown_name, SCREEN_W/2, 43, CLR_WHITE);
    }
}

static void panel(const char *title, int tcol, const char *l1, const char *l2, const char *l3) {
    fade(0.45f);
    int w = 220, h = 92, x = SCREEN_W / 2 - w / 2, y = SCREEN_H / 2 - h / 2;
    rectfill(x, y, w, h, CLR_BROWNISH_BLACK);
    rect(x, y, w, h, tcol);
    print_centered(title, SCREEN_W/2, y + 8, tcol);
    if (l1) print_centered(l1, SCREEN_W/2, y + 28, CLR_WHITE);
    if (l2) print_centered(l2, SCREEN_W/2, y + 40, CLR_LIGHT_GREY);
    if (l3) print_centered(l3, SCREEN_W/2, y + 70, blink(24) ? CLR_YELLOW : CLR_LIGHT_GREY);
}

void draw(void) {
    draw_world();
    draw_hud();

    // white flash on big blasts (dithered to fake alpha)
    if (flash_t > 0.05f) {
        fillp(flash_t > 0.4f ? FILL_SOLID : FILL_CHECKER, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE);
        fillp_reset();
    }

    if (state == ST_BRIEF) {
        panel("CANNON FODDER", CLR_LIGHT_YELLOW, mission_name, mission_obj,
              "LEFT-click move  RIGHT grenade  -  click to deploy");
        print_centered(str("SQUAD: %d   FALLEN: %d", alive_squad(), total_kia), SCREEN_W/2, SCREEN_H / 2 + 22, CLR_GREEN);
    } else if (state == ST_WIN) {
        panel("OBJECTIVE COMPLETE", CLR_GREEN, "The survivors are promoted.",
              str("%d made it out.", alive_squad()), "click to advance");
    } else if (state == ST_FAIL) {
        panel("SQUAD WIPED OUT", CLR_RED, "Every last one of them, gone.",
              str("%d fell in the campaign.", total_kia), "click to enlist fresh recruits");
    } else if (state == ST_CAMPAIGN) {
        panel("CAMPAIGN WON", CLR_LIGHT_YELLOW, "What's left of the squad goes home.",
              str("Survivors: %d   Fallen: %d", alive_squad(), total_kia), "click to start over");
    }
}
