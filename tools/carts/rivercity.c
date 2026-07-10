/* de:meta
{
  "slug": "rivercity",
  "title": "river city",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "finite-state-ai",
    "tilemap-collision",
    "screen-shake-juice"
  ],
  "lineage": "Homage to River City Ransom / Double Dragon belt-scroll brawlers, adapting the crowd.c pal()-recolor trick and a depth-axis hit rule into a four-zone procedural brawler with a light RPG upgrade shop.",
  "genre": "fighting",
  "homage": "River City Ransom (1989)",
  "description": "A procedural belt-scroll brawler-RPG. Fight your way through four re-rolled gangs across downtown, the park, the mall and the docks, scoop the coins they drop, and spend them at the diner on stats (HP/STR/DEF/STAMINA), a heal, and new moves — a spin special and a dash. The whole cast is one 16x32 sprite set recolored per-gang with pal(); floors are tile maps rebuilt per zone; combat has hitstop, shake, flash, combos and a depth-based hit rule. Beat the docks boss to clear the city. Arrows move (up/down = depth), Z attack (tap x3 combo / air = jump kick), X jump, down+Z spin special, double-tap to dash. Diner: up/down pick, Z buy, X leave."
}
de:meta */
#include "studio.h"
#include "endcard.h"
#include <math.h>

// RIVER CITY — a procedural belt-scroll brawler with a light RPG skin.
//
// Roam four scroll-locked zones (DOWNTOWN, PARK, MALL, DOCKS), beat the roving
// gang in each, scoop the coins they drop, and spend them at the diner between
// zones on STATS (max HP, STR, DEF, STAMINA), a heal, and two moves. Beat the
// boss at the docks to win. Every entry RE-ROLLS the gang — counts, toughness,
// positions and palette — so each run plays out differently.
//
// The whole cast (hero + every thug + the boss) is ONE 16x32 sprite set; each
// fighter gets a per-gang shirt/pants/hair via pal() "magic colors" (the crowd.c
// trick). Floors are a real tile map() rebuilt with mset() on every zone entry,
// so four themed zones share one grid. Combat is a little state machine with
// hitstop / shake / flash juice and a depth (belt-scroll) hit rule.
//
//   ARROWS / WASD  walk (also up/down for depth)
//   Z   attack — tap-tap-tap = 3-hit combo (last one knocks down). In air = jump kick.
//   X   jump
//   DOWN + Z   SPIN special (once bought; costs stamina)
//   double-tap LEFT/RIGHT  dash/run (once bought; drains stamina)
//   In the diner: up/down pick, Z buy, X leave.   Clear a gang, walk off the right edge.

// ---------------------------------------------------------------------------
// magic palette indices baked into the sprites, recolored per fighter via pal()
// ---------------------------------------------------------------------------
#define MAGIC_SHIRT 28
#define MAGIC_PANTS 29
#define MAGIC_HAIR  26
#define SKIN        15   // face (recolored to white on a hurt flash)
#define OUTLINE     16

#define MAX_FOES   16
#define MAX_COINS  48
#define MAX_SPARK  28

#define ZONE_CELLS 45
#define ZONE_W     (ZONE_CELLS * 16)   // 720 px world width
#define HORIZON    96
#define FLOOR_TOP  110                 // nearest the camera the feet can go
#define FLOOR_BOT  196
#define DEPTH_TOL  12.0f               // belt-scroll: hits land only at similar y
#define JUMP       3.7f
#define GRAV       0.22f
#define SPEC_COST  28.0f
#define LAST_ZONE  3

enum { K_HERO, K_THUG, K_TOUGH, K_BOSS };
enum { ST_IDLE, ST_WALK, ST_PUNCH, ST_AIR, ST_SPECIAL, ST_HURT, ST_DEAD };
enum { G_TITLE, G_PLAY, G_SHOP, G_WIN, G_OVER };
static float end_t;      // seconds since the end card came up

typedef struct {
    int   active;
    float x, y;            // world feet position (y also = depth + draw baseline)
    float vx, vy;
    float z, vz;           // hop height above the baseline
    int   facing;          // +1 right, -1 left
    int   hp, hpmax;
    int   state, sframe;   // state + frames in it
    int   pcombo;          // hero punch chain 0..2
    int   hit_done;        // one connect per swing
    int   stun;            // hurt-stun frames (also i-frames)
    int   hurt_flash;
    int   atk_cd;          // enemy attack cooldown
    int   kind;
    int   shirt, pants, hair;
} Fighter;

typedef struct { int active; float sx, sy; float t; int value; } Coin;
typedef struct { int t; float x, y; int col; } Spark;

static Fighter hero;
static Fighter foes[MAX_FOES];
static Coin    coins[MAX_COINS];
static Spark   sparks[MAX_SPARK];

// run-wide RPG state (lives outside Fighter so the struct stays generic)
static int   g_str, g_def, hp_max;
static float stam, stam_max;
static bool  has_spin, has_dash;
static int   cash;
static float cash_shown;
static int   hitcombo, combo_timer;

static int   phase, zone, camx, intro_t, win_t;
static bool  cleared;
static int   hitstop;
static int   shop_cursor;
static int   last_beat = -1;
static int   zone_root, zone_scale;

typedef struct { int magic, beat_game, best_cash; } Save;
static Save  best;

static const char *ZONE_NAME[4] = { "DOWNTOWN", "THE PARK", "THE MALL", "THE DOCKS" };
static const int   ZONE_BG[4]   = { CLR_DARKER_BLUE, CLR_DARK_GREEN, CLR_DARKER_PURPLE, CLR_DARK_BLUE };
static const int   FLOOR_A[4]   = { 16, 18, 20, 22 };   // primary floor tile per zone
static const int   FLOOR_B[4]   = { 17, 19, 21, 23 };   // accent tile

// per-zone gang palette: { shirt, pants, hair }
static const int GANG[4][3] = {
    { 8,  24, 16 },   // downtown — red shirts
    { 27, 20, 16 },   // park     — green
    { 13, 2,  10 },   // mall     — indigo / blonde
    { 12, 1,  16 },   // docks    — blue
};

// ---------------------------------------------------------------------------
// little fx helpers
// ---------------------------------------------------------------------------
static void spark_at(float x, float y, int col) {
    for (int i = 0; i < MAX_SPARK; i++) if (sparks[i].t <= 0) {
        sparks[i] = (Spark){ 9, x, y, col }; return;
    }
}
static void coin_at(float wx, float wy, int value) {
    for (int i = 0; i < MAX_COINS; i++) if (!coins[i].active) {
        coins[i] = (Coin){ 1, wx - camx, wy, 0.0f, value }; return;
    }
}
static void sfx_punch(void)  { hit(70, INSTR_NOISE, 2, 28); }
static void sfx_hit(int big) { hit(big ? 44 : 54, INSTR_NOISE, 5, big ? 90 : 50); }
static void sfx_coin(void)   { hit(88, INSTR_SQUARE, 3, 40); hit(95, INSTR_SQUARE, 3, 50); }
static void sfx_jump(void)   { hit(76, INSTR_SQUARE, 2, 45); }

// ---------------------------------------------------------------------------
// zone setup + procedural gang
// ---------------------------------------------------------------------------
static void build_floor(int z) {
    for (int cy = 0; cy < MAP_H; cy++)
        for (int cx = 0; cx < MAP_W; cx++) mset(cx, cy, 0);
    for (int cy = 6; cy <= 12; cy++)
        for (int cx = 0; cx < ZONE_CELLS; cx++) {
            int n = (cx * 7 + cy * 13) % 9;
            mset(cx, cy, n == 0 ? FLOOR_B[z] : FLOOR_A[z]);
        }
}

static void spawn_foe(float x, float y, int kind, int z) {
    for (int i = 0; i < MAX_FOES; i++) if (!foes[i].active) {
        Fighter *f = &foes[i];
        *f = (Fighter){0};
        f->active = 1; f->kind = kind;
        f->x = x; f->y = y; f->facing = -1; f->state = ST_IDLE;
        f->shirt = GANG[z][0]; f->pants = GANG[z][1]; f->hair = GANG[z][2];
        if (kind == K_THUG)  { f->hpmax = 10 + z * 4; }
        if (kind == K_TOUGH) { f->hpmax = 20 + z * 6; f->shirt = 7; }   // a paler, beefier goon
        if (kind == K_BOSS)  { f->hpmax = 70; f->shirt = 10; f->pants = 4; f->hair = 24; }
        f->hp = f->hpmax;
        f->atk_cd = rnd_between(20, 70);
        return;
    }
}

static void enter_zone(int z) {
    zone = z; cleared = false; intro_t = 45; win_t = 0;
    for (int i = 0; i < MAX_FOES; i++) foes[i].active = 0;
    for (int i = 0; i < MAX_COINS; i++) coins[i].active = 0;
    build_floor(z);

    hero.x = 40; hero.y = 150; hero.z = hero.vz = 0; hero.vx = hero.vy = 0;
    hero.facing = 1; hero.state = ST_IDLE; hero.stun = 0; hero.hpmax = hp_max;

    // roll the gang
    int n = (z == LAST_ZONE) ? rnd_between(2, 4) : rnd_between(3, 6);
    for (int i = 0; i < n; i++) {
        float fx = 230 + i * (ZONE_W - 300) / (n > 1 ? n - 1 : 1) + rnd_between(-20, 20);
        float fy = rnd_between(FLOOR_TOP + 6, FLOOR_BOT - 4);
        int kind = chance(20 + z * 12) ? K_TOUGH : K_THUG;
        spawn_foe(fx, fy, kind, z);
    }
    if (z == LAST_ZONE) spawn_foe(ZONE_W - 70, 150, K_BOSS, z);

    // per-zone musical bed
    static const int ROOTS[4]  = { 45, 41, 48, 40 };
    static const int SCALES[4] = { SCALE_PENTA_MIN, SCALE_PENTA, SCALE_PENTA, SCALE_MINOR };
    static const int TEMPO[4]  = { 110, 96, 124, 134 };
    zone_root = ROOTS[z]; zone_scale = SCALES[z]; bpm(TEMPO[z]); last_beat = -1;
}

static void new_run(void) {
    hp_max = 40; g_str = 3; g_def = 0; stam_max = 50; stam = stam_max;
    has_spin = has_dash = false; cash = 0; cash_shown = 0;
    hero = (Fighter){0};
    hero.active = 1; hero.kind = K_HERO;
    hero.shirt = 12; hero.pants = 1; hero.hair = 4;   // the hero's blue jacket / brown hair
    hero.hpmax = hp_max; hero.hp = hp_max;
    enter_zone(0);
    phase = G_PLAY; end_t = 0;
}

void init(void) {
    colorkey(0);
    if (load_bytes(&best, sizeof best) < (int)sizeof best || best.magic != 0x5243)
        best = (Save){ 0x5243, 0, 0 };
    // synth voices for the bed
    instrument(5, INSTR_SAW, 8, 130, 3, 110); instrument_filter(5, FILTER_LOW, 650, 6);  // bass
    instrument(6, INSTR_SQUARE, 4, 70, 2, 90); instrument_duty(6, 0.3f);                  // lead
    new_run();      // drop straight into Downtown
}

// ---------------------------------------------------------------------------
// combat
// ---------------------------------------------------------------------------
static void begin_death(Fighter *f) {
    f->state = ST_DEAD; f->sframe = 0; f->vz = -1.5f;
    int n = (f->kind == K_BOSS) ? 8 : (f->kind == K_TOUGH ? 3 : 2);
    int v = (f->kind == K_BOSS) ? 15 : (f->kind == K_TOUGH ? 8 : 5);
    for (int i = 0; i < n; i++) coin_at(f->x + rnd_between(-6, 6), f->y - 16, v);
}

static void hurt_foe(Fighter *f, int dmg, int kdir, float knock, int stun, int big) {
    f->hp -= dmg;
    f->vx = kdir * knock;
    f->stun = stun; f->hurt_flash = 6;
    f->state = ST_HURT; f->sframe = 0;
    spark_at(f->x, f->y - 16, CLR_WHITE);
    hitstop = mid(2, big ? 5 : 3, 6);
    shake(big ? 3.0f : 1.2f);
    sfx_hit(big);
    hitcombo++; combo_timer = 60;
    if (f->hp <= 0) begin_death(f);
}

static void hurt_hero(int dmg, int kdir) {
    if (hero.stun > 0 || hero.state == ST_DEAD) return;
    dmg = max(1, dmg - g_def);
    hero.hp -= dmg;
    hero.vx = kdir * 2.6f; hero.vy = 0;
    hero.stun = 16; hero.hurt_flash = 8;
    hero.state = ST_HURT; hero.sframe = 0;
    spark_at(hero.x, hero.y - 16, CLR_RED);
    shake(2.0f); sfx_hit(1);
    hitcombo = 0;
    if (hero.hp <= 0) { hero.hp = 0; shake(6.0f); phase = G_OVER;
        if (cash > best.best_cash) { best.best_cash = cash; save_bytes(&best, sizeof best); } }
}

// is target in front of attacker, within reach, at a similar depth?
static bool in_reach(Fighter *a, Fighter *t, float reach) {
    if (fabsf(a->y - t->y) > DEPTH_TOL) return false;
    float ahead = (t->x - a->x) * a->facing;
    return ahead > -7 && ahead < reach && fabsf(t->z - a->z) < 22;
}

static void resolve_hero_attack(void) {
    if (hero.hit_done) return;
    int reach = 0, dmg = 0, knock = 0, stun = 8, big = 0, active = 0, aoe = 0;

    if (hero.state == ST_PUNCH) {
        active = (hero.sframe >= 4 && hero.sframe <= (hero.pcombo == 2 ? 9 : 8));
        reach = hero.pcombo == 2 ? 19 : 15;
        dmg   = g_str + (hero.pcombo == 2 ? 4 : 2);
        knock = hero.pcombo == 2 ? 4 : 2;
        stun  = hero.pcombo == 2 ? 16 : 8;
        big   = hero.pcombo == 2;
    } else if (hero.state == ST_AIR) {
        active = hero.sframe >= 2; reach = 19; dmg = g_str + 3; knock = 3; stun = 12; big = 1;
    } else if (hero.state == ST_SPECIAL) {
        active = (hero.sframe == 6); aoe = 1; dmg = g_str + 6; knock = 5; stun = 18; big = 1;
    } else return;
    if (!active) return;

    int hitany = 0;
    for (int i = 0; i < MAX_FOES; i++) {
        Fighter *f = &foes[i];
        if (!f->active || f->state == ST_DEAD) continue;
        bool ok = aoe ? (fabsf(f->x - hero.x) < 32 && fabsf(f->y - hero.y) < DEPTH_TOL * 1.6f)
                      : in_reach(&hero, f, reach);
        if (ok) { hurt_foe(f, dmg, (f->x >= hero.x ? 1 : -1), knock, stun, big); hitany = 1; }
    }
    if (hitany || aoe) hero.hit_done = 1;
}

// ---------------------------------------------------------------------------
// hero update
// ---------------------------------------------------------------------------
static int  tap_dir = 0, tap_frame = -99;
static bool running = false; static int run_dir = 0;

static void hero_update(float k) {
    bool grounded = hero.z <= 0.01f;

    if (hero.hurt_flash > 0) hero.hurt_flash--;
    if (hero.stun > 0) {
        hero.stun--;
        hero.x += hero.vx * k; hero.vx *= 0.85f;
        if (hero.stun == 0) hero.state = ST_IDLE;
    } else {
        // ---- attacks & jump ----
        bool acting = hero.state == ST_PUNCH || hero.state == ST_SPECIAL || hero.state == ST_AIR;

        if (!acting && grounded && has_spin && btn(0, BTN_DOWN) && btnp(0, BTN_A) && stam >= SPEC_COST) {
            hero.state = ST_SPECIAL; hero.sframe = 0; hero.hit_done = 0; hero.vx = 0;
            stam -= SPEC_COST; shake(3.0f);
            hit(50, INSTR_SAW, 4, 90); hit(62, INSTR_SAW, 4, 140);
        } else if (btnp(0, BTN_A) && !acting) {
            if (!grounded) { hero.state = ST_AIR; hero.sframe = 0; hero.hit_done = 0; sfx_punch(); }
            else { hero.state = ST_PUNCH; hero.sframe = 0; hero.hit_done = 0; hero.pcombo = 0; sfx_punch(); }
        } else if (btnp(0, BTN_B) && grounded && !acting) {
            hero.vz = -JUMP; sfx_jump();
        }

        // chain the next punch on a fresh tap during recovery
        if (hero.state == ST_PUNCH && hero.sframe >= 8 && hero.pcombo < 2 && btnp(0, BTN_A)) {
            hero.pcombo++; hero.sframe = 0; hero.hit_done = 0; sfx_punch();
        }

        // ---- walking (only when free) ----
        if (hero.state == ST_IDLE || hero.state == ST_WALK) {
            // double-tap dash
            if (has_dash) {
                if (btnp(0, BTN_RIGHT)) { if (tap_dir == 1 && frame() - tap_frame < 14) { running = true; run_dir = 1; } tap_dir = 1; tap_frame = frame(); }
                if (btnp(0, BTN_LEFT))  { if (tap_dir == -1 && frame() - tap_frame < 14) { running = true; run_dir = -1; } tap_dir = -1; tap_frame = frame(); }
                if (running && (!btn(0, run_dir == 1 ? BTN_RIGHT : BTN_LEFT) || stam <= 2)) running = false;
            }
            float sp = running ? 2.4f : 1.3f;
            float mvx = (btn(0, BTN_RIGHT) - btn(0, BTN_LEFT)) * sp;
            float mvy = (btn(0, BTN_DOWN)  - btn(0, BTN_UP))   * sp * 0.7f;
            hero.x += mvx * k; hero.y += mvy * k;
            if (mvx != 0) hero.facing = mvx > 0 ? 1 : -1;
            hero.state = (mvx || mvy) ? ST_WALK : ST_IDLE;
            if (running) stam = clamp(stam - 0.6f * k, 0, stam_max);
        }
    }

    // attack timers
    if (hero.state == ST_PUNCH) { resolve_hero_attack(); if (++hero.sframe > 16) hero.state = ST_IDLE; }
    if (hero.state == ST_SPECIAL) { resolve_hero_attack(); if (++hero.sframe > 24) hero.state = ST_IDLE; }
    if (hero.state == ST_AIR) resolve_hero_attack();

    // hop physics
    if (!grounded || hero.vz < 0) {
        hero.vz += GRAV; hero.z -= hero.vz;
        if (hero.z <= 0) { hero.z = 0; hero.vz = 0; if (hero.state == ST_AIR) hero.state = ST_IDLE; hero.sframe = 0; }
    }

    // stamina regen when not sprinting
    if (!running) stam = clamp(stam + 0.35f * k, 0, stam_max);

    hero.x = clamp(hero.x, 8, ZONE_W - 8);
    hero.y = clamp(hero.y, FLOOR_TOP, FLOOR_BOT);
    if (combo_timer > 0 && --combo_timer == 0) hitcombo = 0;
}

// ---------------------------------------------------------------------------
// enemy update
// ---------------------------------------------------------------------------
static void foe_update(Fighter *f, float k) {
    if (f->state == ST_DEAD) {
        f->sframe++; f->vz += GRAV; f->z -= f->vz;
        if (f->sframe > 26) f->active = 0;
        return;
    }
    if (f->hurt_flash > 0) f->hurt_flash--;
    f->facing = hero.x >= f->x ? 1 : -1;

    if (f->stun > 0) {
        f->stun--;
        f->x += f->vx * k; f->vx *= 0.85f;
        f->x = clamp(f->x, 8, ZONE_W - 8);
        if (f->stun == 0) f->state = ST_IDLE;
        return;
    }
    if (f->atk_cd > 0) f->atk_cd--;

    if (f->state == ST_PUNCH) {
        f->sframe++;
        int active = f->kind == K_BOSS ? 8 : 7;
        if (f->sframe == active && in_reach(f, &hero, f->kind == K_BOSS ? 20 : 14))
            hurt_hero((f->kind == K_BOSS ? 9 : 5) + zone, f->facing);
        if (f->sframe > (f->kind == K_BOSS ? 22 : 18)) { f->state = ST_IDLE; f->atk_cd = rnd_between(35, 80); }
        return;
    }

    // approach the hero, line up depth, strike when close
    float sp = f->kind == K_BOSS ? 1.0f : f->kind == K_TOUGH ? 0.85f : 1.05f;
    float dyy = hero.y - f->y, want = hero.x - f->facing * (f->kind == K_BOSS ? 18 : 12);
    if (fabsf(dyy) > 4) f->y += sgn(dyy) * sp * 0.7f * k;
    if (fabsf(want - f->x) > 2) f->x += sgn(want - f->x) * sp * k;
    f->state = ST_WALK;
    f->y = clamp(f->y, FLOOR_TOP, FLOOR_BOT);
    f->x = clamp(f->x, 8, ZONE_W - 8);

    bool aligned = fabsf(hero.y - f->y) < DEPTH_TOL && fabsf(hero.x - f->x) < (f->kind == K_BOSS ? 22 : 16);
    if (aligned && f->atk_cd <= 0 && hero.state != ST_DEAD) {
        f->state = ST_PUNCH; f->sframe = 0; sfx_punch();
    }

    // gentle separation so the gang doesn't stack into one pixel
    for (int j = 0; j < MAX_FOES; j++) {
        Fighter *o = &foes[j];
        if (o == f || !o->active || o->state == ST_DEAD) continue;
        if (fabsf(o->x - f->x) < 12 && fabsf(o->y - f->y) < 8) {
            f->x -= sgn(o->x - f->x) * 0.5f * k;
            f->y -= sgn(o->y - f->y) * 0.4f * k;
        }
    }
}

// ---------------------------------------------------------------------------
// music bed
// ---------------------------------------------------------------------------
static void play_music(void) {
    int b = beat();
    if (b == last_beat) return;
    last_beat = b;
    if (b % 2 == 0) hit(zone_root, 5, 4, 200);                 // bass on the beat
    if (euclid(4, 8, b % 8)) hit(36, INSTR_NOISE, 5, 30);      // kick
    if (b % 2 == 1) hit(64, INSTR_NOISE, 1, 18);               // hat
    if (chance(zone == LAST_ZONE ? 70 : 45))                   // sparse lead
        hit(degree(zone_scale, 6, (b * 3) % 7), 6, 3, 110);
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------
void update(void) {
    if (phase == G_OVER || phase == G_WIN) { end_t += dt(); if (btnp(0, BTN_A)) new_run(); return; }

    if (phase == G_SHOP) {
        // 0 heal 1 +hp 2 +str 3 +def 4 +stam 5 spin 6 dash
        if (btnp(0, BTN_UP))   shop_cursor = (shop_cursor + 6) % 7;
        if (btnp(0, BTN_DOWN)) shop_cursor = (shop_cursor + 1) % 7;
        if (btnp(0, BTN_A)) {
            int c = shop_cursor;
            int price[7] = { 20, 40, 50, 40, 30, 80, 60 };
            bool owned = (c == 5 && has_spin) || (c == 6 && has_dash);
            if (!owned && cash >= price[c]) {
                cash -= price[c];
                if (c == 0) hero.hp = hp_max;
                if (c == 1) { hp_max += 6; hero.hpmax = hp_max; hero.hp += 6; }
                if (c == 2) g_str += 1;
                if (c == 3) g_def += 1;
                if (c == 4) { stam_max += 10; stam = stam_max; }
                if (c == 5) has_spin = true;
                if (c == 6) has_dash = true;
                // level-up flourish
                strum(60, CHORD_MAJ7, 6, 4, 35);
            } else hit(40, INSTR_NOISE, 3, 80);   // denied
        }
        if (btnp(0, BTN_B)) { enter_zone(zone + 1); phase = G_PLAY; }
        return;
    }

    // ---- G_PLAY ----
    play_music();
    if (intro_t > 0) intro_t--;
    if (hitstop > 0) { hitstop--; return; }
    float k = clamp(dt() * 60.0f, 0.3f, 2.2f);

    hero_update(k);
    for (int i = 0; i < MAX_FOES; i++) if (foes[i].active) foe_update(&foes[i], k);

    // coins arc to the counter
    for (int i = 0; i < MAX_COINS; i++) if (coins[i].active) {
        coins[i].t += dt() * 2.1f;
        if (coins[i].t >= 1.0f) { cash += coins[i].value; coins[i].active = 0; sfx_coin(); }
    }
    cash_shown = lerp(cash_shown, cash, 0.18f);

    for (int i = 0; i < MAX_SPARK; i++) if (sparks[i].t > 0) sparks[i].t--;

    // clear check
    int alive = 0;
    for (int i = 0; i < MAX_FOES; i++) if (foes[i].active && foes[i].state != ST_DEAD) alive++;
    if (!alive && !cleared) {
        cleared = true;
        strum(72, CHORD_MAJ, 6, 5, 45);
        if (zone == LAST_ZONE) { win_t = 1; best.beat_game = 1;
            if (cash > best.best_cash) best.best_cash = cash;
            save_bytes(&best, sizeof best); }
    }
    if (win_t > 0 && ++win_t > 70) phase = G_WIN;

    // walk off the right edge of a cleared (non-final) zone → the diner
    if (cleared && zone < LAST_ZONE && hero.x >= ZONE_W - 10) { phase = G_SHOP; shop_cursor = 0; }
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------
static void set_fighter_pal(Fighter *f) {
    pal(MAGIC_SHIRT, f->shirt); pal(MAGIC_PANTS, f->pants); pal(MAGIC_HAIR, f->hair);
    if (f->hurt_flash > 0 && (f->hurt_flash / 2) % 2 == 0) {
        pal(MAGIC_SHIRT, 7); pal(MAGIC_PANTS, 7); pal(MAGIC_HAIR, 7);
        pal(SKIN, 7); pal(OUTLINE, 7);
    }
}

static void draw_fighter(Fighter *f) {
    int baseY = (int)(f->y + 0.5f);
    int sx = (int)(f->x - 8 + 0.5f);
    int sy = baseY - 32 - (int)f->z;
    bool flip = f->facing < 0;

    // shadow (shrinks with hop height)
    int sr = 6 - (int)(f->z / 8); if (sr < 3) sr = 3;
    ovalfill((int)f->x, baseY, sr, 2, CLR_DARKER_GREY);

    set_fighter_pal(f);
    if (f->state == ST_DEAD) {
        spr_rot(0, sx, sy, f->sframe * 26.0f);
    } else if (f->kind == K_BOSS) {
        sspr_ex(0, 0, flip ? -16 : 16, 32, (int)f->x - 13, baseY - 52 - (int)f->z, 26, 52, 0, 0, 0);
    } else {
        int legf = (f->state == ST_WALK) ? anim(4, 8.0f, f->x / 40.0f) : 0;
        sprf(0, sx, sy, flip, false);
        sprf(8 + legf, sx, sy + 16, flip, false);
    }
    pal_reset();

    // attack overlays drawn as primitives (skin-colored fist / foot)
    int dir = f->facing;
    if (f->state == ST_PUNCH && f->sframe >= 3 && f->sframe <= 9) {
        int reach = (f->kind == K_BOSS) ? 18 : (f == &hero && hero.pcombo == 2) ? 16 : 13;
        int ay = sy + (f->kind == K_BOSS ? 22 : 12);
        line((int)f->x, ay, (int)f->x + dir * reach, ay, CLR_LIGHT_PEACH);
        circfill((int)f->x + dir * reach, ay, 2, CLR_LIGHT_PEACH);
    } else if (f->state == ST_AIR) {
        int ay = sy + 26;
        line((int)f->x, sy + 16, (int)f->x + dir * 16, ay, CLR_LIGHT_PEACH);
        circfill((int)f->x + dir * 16, ay, 2, CLR_LIGHT_PEACH);
    } else if (f->state == ST_SPECIAL) {
        float r = 8 + f->sframe;
        circ((int)f->x, sy + 18, (int)r, CLR_LIGHT_YELLOW);
        circ((int)f->x, sy + 18, (int)r - 3, CLR_ORANGE);
    }
}

static void draw_backdrop(int z) {
    if (z == 0) {                                   // downtown — building silhouettes + windows
        for (int bx = 0; bx < ZONE_W; bx += 56) {
            int h = 40 + ((bx * 7) % 30);
            rectfill(bx + 4, HORIZON - h, 48, h, CLR_DARK_BLUE);
            for (int wy = HORIZON - h + 5; wy < HORIZON - 6; wy += 9)
                for (int wx = bx + 9; wx < bx + 48; wx += 9)
                    rectfill(wx, wy, 4, 4, ((wx + wy) % 3) ? CLR_DARKER_GREY : CLR_LIGHT_YELLOW);
        }
    } else if (z == 1) {                            // park — sky band + round trees + fence
        fillp(FILL_CHECKER, -1);
        rectfill(0, 0, ZONE_W, HORIZON, CLR_BLUE); fillp_reset();
        for (int tx = 20; tx < ZONE_W; tx += 70) {
            rectfill(tx + 7, HORIZON - 22, 4, 22, CLR_BROWN);
            circfill(tx + 9, HORIZON - 26, 13, CLR_DARK_GREEN);
            circfill(tx + 2, HORIZON - 20, 9, CLR_MEDIUM_GREEN);
        }
        for (int fx = 0; fx < ZONE_W; fx += 12) line(fx, HORIZON - 8, fx, HORIZON, CLR_MEDIUM_GREY);
    } else if (z == 2) {                            // mall — interior wall + lit storefronts
        rectfill(0, 0, ZONE_W, HORIZON, CLR_DARKER_PURPLE);
        for (int sx2 = 8; sx2 < ZONE_W; sx2 += 64) {
            rectfill(sx2, HORIZON - 34, 52, 28, CLR_MAUVE);
            rectfill(sx2 + 4, HORIZON - 30, 44, 16, CLR_DARK_BLUE);
            print("SALE", sx2 + 12, HORIZON - 44, blink(20) ? CLR_PINK : CLR_LIGHT_YELLOW);
        }
    } else {                                        // docks — sunset, water, distant crane
        circfill(ZONE_W / 3, 40, 22, CLR_DARK_ORANGE);
        for (int wy = 50; wy < HORIZON; wy += 6)
            line(0, wy, ZONE_W, wy, ((wy / 6 + frame() / 8) % 2) ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
        line(ZONE_W - 120, HORIZON, ZONE_W - 120, 24, CLR_DARKER_GREY);
        line(ZONE_W - 120, 24, ZONE_W - 70, 38, CLR_DARKER_GREY);
    }
}

static void draw_world(void) {
    // y-sort hero + foes
    Fighter *ord[MAX_FOES + 1]; int n = 0;
    if (hero.state != ST_DEAD) ord[n++] = &hero;
    for (int i = 0; i < MAX_FOES; i++) if (foes[i].active) ord[n++] = &foes[i];
    for (int i = 1; i < n; i++) { Fighter *v = ord[i]; int j = i - 1;
        while (j >= 0 && ord[j]->y > v->y) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = v; }
    for (int i = 0; i < n; i++) draw_fighter(ord[i]);

    for (int i = 0; i < MAX_SPARK; i++) if (sparks[i].t > 0)
        circfill((int)sparks[i].x, (int)sparks[i].y, sparks[i].t / 3 + 1, sparks[i].col);

    // exit marker once the zone is clear
    if (cleared && zone < LAST_ZONE) {
        int ex = ZONE_W - 18;
        print(">>", ex, 100, blink(15) ? CLR_YELLOW : CLR_ORANGE);
        print("EXIT", ex - 6, 112, CLR_WHITE);
    }
}

static void hud(void) {
    // HP + stamina (top-left)
    bar(6, 6, 90, 6, (float)hero.hp / hero.hpmax, hero.hp > hero.hpmax / 3 ? CLR_GREEN : CLR_RED, CLR_DARKER_GREY);
    print(str("%d", hero.hp), 99, 5, CLR_WHITE);
    bar(6, 15, 70, 4, stam / stam_max, CLR_BLUE, CLR_DARKER_GREY);
    print(str("STR %d  DEF %d", g_str, g_def), 6, 23, CLR_LIGHT_GREY);

    // cash + coin icon (top-right)
    circfill(SCREEN_W - 56, 9, 4, CLR_YELLOW); circ(SCREEN_W - 56, 9, 4, CLR_ORANGE);
    print_right(str("$%d", (int)cash_shown), SCREEN_W - 6, 5, CLR_YELLOW);

    // zone progress dots (top-center)
    for (int i = 0; i < 4; i++) {
        int cx = SCREEN_W / 2 - 24 + i * 16;
        int c = i < zone ? CLR_GREEN : i == zone ? (blink(20) ? CLR_YELLOW : CLR_ORANGE) : CLR_DARK_GREY;
        circfill(cx, 9, 3, c);
    }
    print_centered(ZONE_NAME[zone], SCREEN_W/2, 16, CLR_WHITE);

    if (hitcombo > 1) print_centered(str("COMBO x%d", hitcombo), SCREEN_W/2, 30, CLR_LIGHT_YELLOW);

    if (intro_t > 0) print_centered(str("%s — clear the gang!", ZONE_NAME[zone]), SCREEN_W/2, SCREEN_H - 14, CLR_YELLOW);
    else if (cleared && zone < LAST_ZONE) print_centered("ZONE CLEAR! head to the diner >>", SCREEN_W/2, SCREEN_H - 14, CLR_GREEN);
}

static void draw_coins(void) {
    for (int i = 0; i < MAX_COINS; i++) if (coins[i].active) {
        float t = ease_in(coins[i].t);
        int x = (int)lerp(coins[i].sx, SCREEN_W - 56, t);
        int y = (int)lerp(coins[i].sy, 9, t);
        int rx = (frame() / 3 + i) % 2 ? 3 : 1;        // spin
        ovalfill(x, y, rx, 3, CLR_YELLOW);
    }
}

static void draw_shop(void) {
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, 0, SCREEN_W, 24, CLR_DARK_RED);
    print_centered("RIVER CITY DINER", SCREEN_W/2, 8, CLR_LIGHT_YELLOW);
    print_right(str("$%d", cash), SCREEN_W - 8, 8, CLR_YELLOW);

    const char *names[7] = { "BURGER (heal full)", "+6 MAX HP", "+1 STR", "+1 DEF",
                             "+10 STAMINA", "SPIN SPECIAL  (down+Z)", "DASH RUN  (double-tap)" };
    int price[7] = { 20, 40, 50, 40, 30, 80, 60 };
    for (int i = 0; i < 7; i++) {
        int y = 36 + i * 18;
        bool sel = i == shop_cursor;
        bool owned = (i == 5 && has_spin) || (i == 6 && has_dash);
        if (sel) { rectfill(8, y - 2, SCREEN_W - 16, 16, CLR_DARK_PURPLE); rect(8, y - 2, SCREEN_W - 16, 16, CLR_YELLOW); }
        print(names[i], 16, y + 1, owned ? CLR_DARK_GREY : sel ? CLR_WHITE : CLR_LIGHT_GREY);
        if (owned) print_right("OWNED", SCREEN_W - 16, y + 1, CLR_GREEN);
        else print_right(str("$%d", price[i]), SCREEN_W - 16, y + 1, cash >= price[i] ? CLR_GREEN : CLR_RED);
    }
    print_centered("up/down pick   Z buy   X leave for the next zone", SCREEN_W/2, SCREEN_H - 10, CLR_LIGHT_GREY);
}

void draw(void) {
    if (phase == G_SHOP) { draw_shop(); return; }

    cls(ZONE_BG[zone]);
    camera(camx / 2, 0); draw_backdrop(zone);     // parallax scenery
    camx = (int)clamp(hero.x - SCREEN_W / 2, 0, ZONE_W - SCREEN_W);
    camera(camx, 0);
    map(0, 6, 0, HORIZON, ZONE_CELLS, 7);          // tiled floor band
    draw_world();
    camera(0, 0);

    draw_coins();
    hud();

    if (intro_t > 0) fade(intro_t / 45.0f);

    // end cards — the shared end-screen treatment (endcard.h)
    if (phase == G_OVER) {
        EndCard c = endcard(end_t, 240, 52, 76, CLR_BROWNISH_BLACK, CLR_RED, CLR_DARK_RED);
        if (c.settled) {
            print_centered("KNOCKED OUT", SCREEN_W/2, c.y + 8, CLR_RED);
            print_centered(str("you reached %s with $%d", ZONE_NAME[zone], cash), SCREEN_W/2, c.y + 24, CLR_WHITE);
            if (blink(18)) print_centered("press Z for a new run", SCREEN_W/2, c.y + 38, CLR_YELLOW);
        }
    } else if (phase == G_WIN) {
        EndCard c = endcard(end_t, 250, 62, 66, CLR_DARK_BLUE, CLR_LIGHT_YELLOW, CLR_ORANGE);
        if (c.settled) {
            print_scaled("CITY CLEARED!", SCREEN_W / 2 - 78, c.y + 8, CLR_LIGHT_YELLOW, 2);
            print_centered(str("you took down the docks boss  -  $%d", cash), SCREEN_W/2, c.y + 32, CLR_WHITE);
            if (blink(18)) print_centered("press Z to run it again", SCREEN_W/2, c.y + 48, CLR_GREEN);
        }
    } else if (win_t > 0) {
        print_centered("BOSS DOWN!", SCREEN_W/2, 84, CLR_LIGHT_YELLOW);
    }
}
