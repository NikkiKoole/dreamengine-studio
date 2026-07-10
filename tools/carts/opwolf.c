/* de:meta
{
  "slug": "opwolf",
  "title": "operation wolf",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "particle-system",
    "state-machine"
  ],
  "lineage": "Homage to Taito's Operation Wolf (1987) — on-rails light-gun shooter with a scrolling tilemap, per-stage pal() recolors, and enemies that telegraph then fire projectiles at the screen; novel in the civilian-avoidance penalty and the grenade arc that must be shot out of the air.",
  "genre": "shooter",
  "homage": "Operation Wolf (1987)",
  "description": "An on-rails light-gun shooter, mouse to the core. The world scrolls past on a rail — one tile map() recolored per stage with pal() (jungle, enemy camp, airfield) — while soldiers rise from the treeline, telegraph with a raised rifle, then loose bullets and lobbed grenades that come AT the screen; shoot them out of the air. LEFT mouse is the magazine machine gun, RIGHT is the scarce rocket, and you RELOAD by shooting the drifting supply crates. Don't gun down the hands-up civilians — it costs health and score. Wave-based across three stages with a health bar (no lives), best score saved. Muzzle flash, recoil, brass casings, tracer, white hit-flash, fillp explosion rings, screen shake, a damage flash and a low-health alarm. Move mouse to aim, LEFT shoot, RIGHT rocket, shoot a crate to reload."
}
de:meta */
#include "studio.h"

// OPERATION WOLF — an on-rails light-gun arcade shooter.
//
// The world is ONE tile map() scrolling on a rail; the same tileset is recolored
// per stage with pal() (jungle -> camp -> airfield). Targets live in SCREEN space:
// soldiers rise from the treeline, TELEGRAPH (raise rifle + a tick), then fire
// bullets and lobbed grenades that come AT the screen — shoot them out of the air.
// Left mouse = machine gun (magazine), right mouse = rocket (scarce). RELOAD by
// shooting the drifting supply crates. DON'T shoot the hands-up civilians.
//
// MOUSE: move to aim - LEFT shoot - RIGHT rocket - shoot a crate to reload.

// ---- gun anchor (bottom-center, where shots originate) ----
#define GX        (SCREEN_W / 2)
#define GY        (SCREEN_H + 6)      // just off-screen bottom = "you"
#define GROUND_Y  (SCREEN_H - 22)     // where lobbed grenades land

// ---- pools ----
#define MAX_E     16                  // enemies + civilians
#define MAX_P     24                  // incoming enemy projectiles
#define MAX_C      4                  // supply crates
#define MAX_PART 140                  // shells / smoke / sparks
#define MAX_BOOM   8                  // explosion rings

// magic placeholder colors in the soldier sprite — swapped per enemy type
#define MAG_UNI   28                  // uniform
#define MAG_HELM  29                  // helmet

// actor types
#define T_GRUNT    0                  // rises, shoots a bullet after a telegraph
#define T_THROWER  1                  // lobs an arcing grenade
#define T_ARMORED  2                  // takes several hits, flashes each time
#define T_CIVILIAN 3                  // DON'T shoot — costs health + score

// actor states
#define S_RISE   0
#define S_LIVE   1
#define S_DIE    2

// projectile types
#define P_BULLET 0
#define P_GRENADE 1

// game states
#define G_PLAY   0
#define G_CLEAR  1
#define G_OVER   2

typedef struct {
    float x, y;              // screen position (top-left of 16x16 sprite)
    float vx, vy;            // motion (rise / civilian walk)
    float ty;                // target y to settle at after rising
    int   type, state, hp, maxhp;
    int   fire_cd;           // counts down to next shot
    int   tele;              // telegraph windup frames left (>0 = winding up)
    int   flash;             // hit-flash frames
    float phase;             // anim offset
    bool  on;
} Actor;

typedef struct {
    float x, y, vx, vy;
    float prog;              // 0..1 how "close" it is (drives the grow)
    int   type, life;
    bool  on;
} Proj;

typedef struct {
    float x, y, vx;
    int   type;              // 0 = ammo, 1 = rocket
    int   hp;
    bool  on;
} Crate;

typedef struct {
    float x, y, vx, vy, grav, life;
    int   maxlife, color;
    float size;
} Part;

typedef struct { float x, y, r, maxr, life, maxlife; bool smoke; bool on; } Boom;

static Actor enemies[MAX_E];
static Proj  projs[MAX_P];
static Crate crates[MAX_C];
static Part  parts[MAX_PART];
static Boom  booms[MAX_BOOM];

// player / run state
static int   health, ammo, ammo_max, rockets;
static int   score, best, stage, wave, gstate;
static float scroll;                  // map scroll accumulator (px)
static float cx, cy;                  // lagged crosshair
static int   fire_cd, muzzle, recoil; // gun feel timers
static int   dmg_flash, hitmark;      // red flash / hit-marker on crosshair
static int   to_spawn, spawn_cd;      // remaining enemies this wave + spawn timer
static float banner_t;                // stage / wave banner countdown
static bool  win;
static int   alarm_cd;

// per-stage flavor
static const int STAGE_WAVES[3] = { 3, 3, 4 };
static const char *STAGE_NAME[3] = { "JUNGLE", "ENEMY CAMP", "AIRFIELD" };
static const int STAGE_SKY[3]    = { CLR_DARKER_BLUE, CLR_DARK_PURPLE, CLR_DARKER_GREY };

// =====================================================================
// pools
// =====================================================================

static Part *free_part(void) {
    for (int i = 0; i < MAX_PART; i++) if (parts[i].life <= 0) return &parts[i];
    return 0;
}

static void spark(float x, float y, float vx, float vy, float grav,
                  int life, float size, int color) {
    Part *p = free_part();
    if (!p) return;
    *p = (Part){ x, y, vx, vy, grav, (float)life, life, color, size };
}

static void shell(float x, float y) {           // ejected brass casing
    spark(x, y, rnd_float_between(-2.2f, -0.6f), rnd_float_between(-2.6f, -1.2f),
          0.34f, rnd_between(22, 34), 1.0f, CLR_YELLOW);
}

static void puff(float x, float y, int n, int c) {
    for (int i = 0; i < n; i++) {
        float a = rnd(360), s = rnd_float_between(0.4f, 2.4f);
        spark(x, y, dx(s, a), dy(s, a), 0.02f, rnd_between(10, 20),
              rnd_float_between(1, 2.5f), c);
    }
}

static void boom_at(float x, float y, float r, bool smoke) {
    for (int i = 0; i < MAX_BOOM; i++) {
        if (!booms[i].on) {
            booms[i] = (Boom){ x, y, 1, r, 18, 18, smoke, true };
            return;
        }
    }
}

// =====================================================================
// explosion (rocket / grenade)
// =====================================================================

static void explode(float x, float y, float radius, int dmg_player) {
    boom_at(x, y, radius, false);
    boom_at(x, y, radius * 1.5f, true);
    puff(x, y, 18, CLR_ORANGE);
    puff(x, y, 10, CLR_LIGHT_GREY);
    shake(7);
    // a chunky low blast
    hit(30, INSTR_NOISE, 7, 320);
    note(33, INSTR_SINE, 5);

    // AoE on enemies + incoming projectiles
    for (int i = 0; i < MAX_E; i++) {
        Actor *e = &enemies[i];
        if (!e->on || e->state == S_DIE) continue;
        if (e->type == T_CIVILIAN) continue;   // rockets spare civilians
        if (near((int)(e->x + 8), (int)(e->y + 8), (int)x, (int)y, (int)radius)) {
            e->hp = 0; e->state = S_DIE; e->flash = 1; e->fire_cd = 24;
            score += (e->type == T_ARMORED) ? 300 : (e->type == T_THROWER ? 150 : 100);
            puff(e->x + 8, e->y + 8, 8, CLR_RED);
        }
    }
    for (int i = 0; i < MAX_P; i++)
        if (projs[i].on && near((int)projs[i].x, (int)projs[i].y, (int)x, (int)y, (int)radius))
            projs[i].on = false;

    if (dmg_player > 0) {
        health = max(0, health - dmg_player);
        dmg_flash = 8;
    }
}

// =====================================================================
// spawning
// =====================================================================

static void spawn_proj(float x, float y, int type) {
    for (int i = 0; i < MAX_P; i++) {
        if (projs[i].on) continue;
        Proj *p = &projs[i];
        p->x = x; p->y = y; p->prog = 0; p->on = true; p->type = type; p->life = 600;
        if (type == P_BULLET) {
            float a = angle_to((int)x, (int)y, GX, GY);
            float spd = 1.9f + stage * 0.25f;
            p->vx = dx(spd, a); p->vy = dy(spd, a);
            note(57, INSTR_SQUARE, 2);
        } else { // grenade — lob: toward player x, tossed UP first, gravity pulls down
            p->vx = (GX - x) / 90.0f + rnd_float_between(-0.3f, 0.3f);
            p->vy = -2.6f;
            note(45, INSTR_TRI, 3);
        }
        return;
    }
}

static void spawn_enemy(int type) {
    for (int i = 0; i < MAX_E; i++) {
        if (enemies[i].on) continue;
        Actor *e = &enemies[i];
        e->type = type; e->on = true; e->flash = 0; e->tele = 0; e->phase = rnd_float();
        e->x = rnd_between(14, SCREEN_W - 30);
        if (type == T_CIVILIAN) {
            e->state = S_LIVE;
            e->y = GROUND_Y - 4; e->ty = e->y;
            e->vx = rnd(2) ? 0.5f : -0.5f; e->vy = 0;
            e->hp = e->maxhp = 1;
            e->fire_cd = 99999;
        } else {
            e->state = S_RISE;
            e->y = 86;                          // start sunk behind the treeline
            e->ty = rnd_between(64, 92);         // settle a touch forward
            e->vx = 0; e->vy = -1.1f;
            e->maxhp = (type == T_ARMORED) ? 4 : 1;
            e->hp = e->maxhp;
            e->fire_cd = rnd_between(60, 120) - stage * 8;
        }
        return;
    }
}

static void spawn_crate(void) {
    for (int i = 0; i < MAX_C; i++) {
        if (crates[i].on) continue;
        bool left = rnd(2);
        crates[i].x  = left ? -16 : SCREEN_W;
        crates[i].y  = rnd_between(40, GROUND_Y - 18);
        crates[i].vx = (left ? 1 : -1) * rnd_float_between(0.5f, 0.9f);
        crates[i].type = (rnd(100) < 28) ? 1 : 0;     // mostly ammo, some rockets
        crates[i].hp = 1; crates[i].on = true;
        return;
    }
}

// enemies remaining to deal with this wave
static int live_enemies(void) {
    int n = 0;
    for (int i = 0; i < MAX_E; i++)
        if (enemies[i].on && enemies[i].type != T_CIVILIAN && enemies[i].state != S_DIE) n++;
    return n;
}

static int wave_quota(void) {
    return 4 + wave + stage * 2;
}

static void start_wave(void) {
    to_spawn = wave_quota();
    spawn_cd = 30;
    banner_t = 1.4f;
}

static void start_stage(void) {
    for (int i = 0; i < MAX_E; i++) enemies[i].on = false;
    for (int i = 0; i < MAX_P; i++) projs[i].on = false;
    for (int i = 0; i < MAX_C; i++) crates[i].on = false;
    scroll = 0; wave = 1;
    start_wave();
    banner_t = 2.4f;
}

static void new_game(void) {
    for (int i = 0; i < MAX_PART; i++) parts[i].life = 0;
    for (int i = 0; i < MAX_BOOM; i++) booms[i].on = false;
    health = 100; ammo_max = 40; ammo = ammo_max; rockets = 3;
    score = 0; stage = 0; gstate = G_PLAY; win = false;
    cx = GX; cy = SCREEN_H / 2;
    fire_cd = muzzle = recoil = dmg_flash = hitmark = alarm_cd = 0;
    start_stage();
}

// =====================================================================
// the gun — hitscan at the crosshair
// =====================================================================

// returns true if something valid was hit (for the hit-marker)
static bool resolve_shot(int hx, int hy, int radius) {
    // 1) incoming projectiles first — most satisfying to swat
    for (int i = 0; i < MAX_P; i++) {
        if (projs[i].on && near(hx, hy, (int)projs[i].x, (int)projs[i].y, radius + 2)) {
            projs[i].on = false;
            puff(projs[i].x, projs[i].y, 6, CLR_YELLOW);
            score += 20;
            return true;
        }
    }
    // 2) crates — reload by shooting
    for (int i = 0; i < MAX_C; i++) {
        Crate *c = &crates[i];
        if (c->on && near(hx, hy, (int)c->x + 8, (int)c->y + 8, radius + 4)) {
            c->on = false;
            puff(c->x + 8, c->y + 8, 10, c->type ? CLR_RED : CLR_GREEN);
            if (c->type) { rockets++; note(72, INSTR_SQUARE, 4); note(79, INSTR_SQUARE, 4); }
            else         { ammo = ammo_max; hit(60, INSTR_SQUARE, 4, 60); hit(67, INSTR_SQUARE, 4, 70); }
            shake(2);
            return true;
        }
    }
    // 3) actors — nearest under the crosshair
    int best_i = -1; float best_d = 1e9f;
    for (int i = 0; i < MAX_E; i++) {
        Actor *e = &enemies[i];
        if (!e->on || e->state == S_DIE) continue;
        if (near(hx, hy, (int)e->x + 8, (int)e->y + 8, radius + 3)) {
            float d = distance(hx, hy, (int)e->x + 8, (int)e->y + 8);
            if (d < best_d) { best_d = d; best_i = i; }
        }
    }
    if (best_i >= 0) {
        Actor *e = &enemies[best_i];
        if (e->type == T_CIVILIAN) {                 // penalty!
            e->state = S_DIE; e->flash = 1; e->fire_cd = 30;
            health = max(0, health - 15);
            score = max(0, score - 200);
            dmg_flash = 10; shake(5);
            hit(40, INSTR_SAW, 6, 300);              // ugly sting
            puff(e->x + 8, e->y + 8, 8, CLR_PINK);
            return false;
        }
        e->hp--;
        e->flash = 4;
        puff(hx, hy, 3, CLR_WHITE);
        if (e->hp <= 0) {
            e->state = S_DIE; e->fire_cd = 24;
            score += (e->type == T_ARMORED) ? 300 : (e->type == T_THROWER ? 150 : 100);
            puff(e->x + 8, e->y + 10, 7, CLR_RED);
            shake(2);
            hit(48, INSTR_NOISE, 4, 90);
        } else {
            note(64, INSTR_SQUARE, 2);               // armor ping
        }
        return true;
    }
    return false;
}

static void fire_gun(void) {
    if (fire_cd > 0) return;
    if (ammo <= 0) {                                 // empty-mag click
        hit(84, INSTR_SQUARE, 1, 25);
        fire_cd = 8;
        return;
    }
    ammo--;
    fire_cd = 4;
    muzzle = 3; recoil = 3;
    shake(1.2f);
    shell(GX + 6, GY - 18);
    // a chunky filtered-noise rattle, pitch wobbles a little per round
    hit(46 + rnd(6), 5, 4, 45);

    bool good = resolve_shot((int)cx, (int)cy, 5);
    if (good) hitmark = 5;
    // tracer impact dust where the round lands
    puff(cx, cy, 2, CLR_ORANGE);
}

static void fire_rocket(void) {
    if (rockets <= 0) { hit(84, INSTR_SQUARE, 1, 25); return; }
    rockets--;
    muzzle = 4; recoil = 6;
    explode(cx, cy, 30, 0);
    hitmark = 6;
}

// =====================================================================
// update
// =====================================================================

void init(void) {
    colorkey(0);                                  // index 0 transparent for sprites
    bpm(96);
    // slot 5 — the machine gun: a tight, filtered noise "chunk"
    instrument(5, INSTR_NOISE, 0, 38, 0, 16);
    instrument_filter(5, FILTER_LOW, 1300, 5);
    // slot 6 — a low ambience drone (per-stage bed)
    instrument(6, INSTR_TRI, 300, 200, 4, 600);
    instrument_filter(6, FILTER_LOW, 600, 3);
    best = load(0);
    new_game();

    // a lively first frame for the thumbnail: a firefight already underway
    enemies[0].on = true; enemies[0].type = T_GRUNT; enemies[0].state = S_LIVE;
    enemies[0].x = 70; enemies[0].y = 70; enemies[0].ty = 70; enemies[0].hp = 1; enemies[0].maxhp = 1;
    enemies[1].on = true; enemies[1].type = T_ARMORED; enemies[1].state = S_LIVE;
    enemies[1].x = 210; enemies[1].y = 66; enemies[1].ty = 66; enemies[1].hp = 4; enemies[1].maxhp = 4; enemies[1].tele = 18;
    crates[0].on = true; crates[0].x = 150; crates[0].y = 48; crates[0].vx = 0.6f; crates[0].type = 0; crates[0].hp = 1;
    cx = 150; cy = 80;
    explode(96, 96, 24, 0);
    muzzle = 3; recoil = 3;
    spawn_proj(214, 74, P_BULLET);
}

void update(void) {
    float k = dt() * 60.0f;

    // ---- end states ----
    if (gstate == G_OVER) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE)) new_game();
        return;
    }

    // ---- crosshair: snappy but slightly lagged follow ----
    cx = lerp(cx, mouse_x(), clamp(0.5f * k, 0, 1));
    cy = lerp(cy, mouse_y(), clamp(0.5f * k, 0, 1));

    if (muzzle > 0) muzzle--;
    if (recoil > 0) recoil--;
    if (fire_cd > 0) fire_cd--;
    if (dmg_flash > 0) dmg_flash--;
    if (hitmark > 0) hitmark--;

    // ---- particles + booms always animate ----
    for (int i = 0; i < MAX_PART; i++) {
        if (parts[i].life <= 0) continue;
        parts[i].vy += parts[i].grav;
        parts[i].x += parts[i].vx; parts[i].y += parts[i].vy;
        parts[i].life -= 1;
    }
    for (int i = 0; i < MAX_BOOM; i++) {
        if (!booms[i].on) continue;
        booms[i].life -= 1;
        booms[i].r = lerp(booms[i].maxr, 0, booms[i].life / booms[i].maxlife);
        if (booms[i].life <= 0) booms[i].on = false;
    }

    // ---- stage-clear interlude ----
    if (gstate == G_CLEAR) {
        banner_t -= dt();
        if (banner_t <= 0) {
            stage++;
            if (stage >= 3) { win = true; gstate = G_OVER;
                              if (score > best) { best = score; save(0, best); } }
            else { gstate = G_PLAY; start_stage(); }
        }
        return;
    }

    // ---- PLAY ----
    if (banner_t > 0) banner_t -= dt();
    scroll += (0.35f + stage * 0.12f) * k;        // the rail keeps rolling

    // ambience bed + low-health alarm
    if (every(2)) note(36 - stage * 2, 6, 2);
    if (health < 25) {
        if (alarm_cd <= 0) { note(76, INSTR_SQUARE, 3); alarm_cd = 28; }
        else alarm_cd -= (int)k;
    }

    // ---- shooting ----
    if (mouse_down(MOUSE_LEFT))  fire_gun();
    if (mouse_pressed(MOUSE_RIGHT)) fire_rocket();

    // ---- spawn drip ----
    if (to_spawn > 0) {
        spawn_cd -= (int)k;
        if (spawn_cd <= 0) {
            int r = rnd(100);
            int type = T_GRUNT;
            if (stage >= 1 && r < 26) type = T_THROWER;
            else if (stage >= 2 && r < 46) type = T_ARMORED;
            spawn_enemy(type);
            to_spawn--;
            spawn_cd = rnd_between(34, 70) - stage * 5;
            if (rnd(100) < 22) spawn_enemy(T_CIVILIAN);   // a bystander wanders in
        }
    }
    // keep a crate or two drifting through
    {
        int n = 0; for (int i = 0; i < MAX_C; i++) if (crates[i].on) n++;
        if (n == 0 && rnd(100) < 3) spawn_crate();
        else if (n < 2 && rnd(100) < 1) spawn_crate();
    }

    // ---- enemies ----
    for (int i = 0; i < MAX_E; i++) {
        Actor *e = &enemies[i];
        if (!e->on) continue;
        if (e->flash > 0) e->flash--;

        if (e->state == S_DIE) {
            e->fire_cd--;
            e->y += 0.6f * k;                     // slump
            if (e->fire_cd <= 0) e->on = false;
            continue;
        }

        if (e->type == T_CIVILIAN) {
            e->x += e->vx * k;
            if (e->x < 6 || e->x > SCREEN_W - 22) e->vx = -e->vx;
            continue;
        }

        if (e->state == S_RISE) {
            e->y += e->vy * k;
            if (e->y <= e->ty) { e->y = e->ty; e->state = S_LIVE; }
            continue;
        }

        // S_LIVE: count down to a shot, with a visible telegraph window
        if (e->tele > 0) {
            e->tele -= (int)k;
            if (e->tele <= 0) {                   // release!
                if (e->type == T_THROWER) spawn_proj(e->x + 8, e->y + 6, P_GRENADE);
                else                      spawn_proj(e->x + 8, e->y + 6, P_BULLET);
                e->fire_cd = rnd_between(80, 150) - stage * 10;
            }
        } else {
            e->fire_cd -= (int)k;
            if (e->fire_cd <= 0) e->tele = 26;    // wind up before firing
        }
    }

    // ---- incoming projectiles ----
    for (int i = 0; i < MAX_P; i++) {
        Proj *p = &projs[i];
        if (!p->on) continue;
        if (p->type == P_GRENADE) p->vy += 0.06f * k;
        p->x += p->vx * k; p->y += p->vy * k;
        p->prog = clamp((p->y - 40) / (float)(GROUND_Y - 40), 0, 1);
        if (--p->life <= 0) { p->on = false; continue; }

        if (p->type == P_BULLET) {
            if (p->y >= SCREEN_H - 6 || p->x < -4 || p->x > SCREEN_W + 4) {
                if (p->y >= SCREEN_H - 6) {       // it reached you
                    health = max(0, health - 8);
                    dmg_flash = 7; shake(4);
                    note(40, INSTR_NOISE, 5);
                }
                p->on = false;
            }
        } else { // grenade lands and detonates
            if (p->y >= GROUND_Y && p->vy > 0) {
                p->on = false;
                explode(p->x, GROUND_Y, 22, 18);
            }
        }
    }

    // ---- crates drift ----
    for (int i = 0; i < MAX_C; i++) {
        if (!crates[i].on) continue;
        crates[i].x += crates[i].vx * k;
        crates[i].y += sin_deg(now() * 90 + i * 60) * 0.3f;   // gentle bob
        if (crates[i].x < -20 || crates[i].x > SCREEN_W + 4) crates[i].on = false;
    }

    // ---- death check ----
    if (health <= 0 && gstate == G_PLAY) {
        gstate = G_OVER; win = false;
        if (score > best) { best = score; save(0, best); }
        explode(GX, SCREEN_H - 30, 36, 0);
    }

    // ---- wave / stage progression ----
    if (gstate == G_PLAY && to_spawn == 0 && live_enemies() == 0) {
        // clear any civilians + stray projectiles, advance
        for (int i = 0; i < MAX_P; i++) projs[i].on = false;
        if (wave < STAGE_WAVES[stage]) {
            wave++; start_wave();
        } else {
            score += 1000;                        // stage bonus
            gstate = G_CLEAR; banner_t = 2.6f;
            note(72, INSTR_SINE, 5); schedule(160, 79, INSTR_SINE, 5);
        }
    }
}

// =====================================================================
// draw
// =====================================================================

// recolor the single tileset to the current stage's palette
static void stage_palette(void) {
    if (stage == 0) return;                       // jungle = the natural greens
    if (stage == 1) { pal(3, 20); pal(11, 4);  }  // camp  — khaki / brown
    else            { pal(3, 17); pal(11, 5);  }  // airfield — grey tarmac
}

static void draw_soldier(Actor *e) {
    int x = (int)e->x, y = (int)e->y;
    bool firing = (e->tele > 0 && blink(3)) || e->state == S_DIE;
    int  frame_slot = firing ? 2 : 1;

    if (e->flash > 0) {                           // hit-flash: blow the sprite white
        pal(MAG_UNI, 7); pal(MAG_HELM, 7); pal(15, 7); pal(5, 7);
    } else {
        switch (e->type) {
            case T_GRUNT:   pal(MAG_UNI, 3);  pal(MAG_HELM, 5);  break;
            case T_THROWER: pal(MAG_UNI, 4);  pal(MAG_HELM, 16); break;
            case T_ARMORED: pal(MAG_UNI, 6);  pal(MAG_HELM, 13); break;
        }
    }
    spr(frame_slot, x, y);
    pal_reset();

    // telegraph tick: a little red aim spark above the muzzle while winding up
    if (e->tele > 0 && e->state == S_LIVE && blink(2))
        pset(x + 12, y + 4, CLR_RED);
}

void draw(void) {
    cls(STAGE_SKY[stage]);

    // a soft dithered haze just above the horizon
    fillp(FILL_CHECKER, -1);
    rectfill(0, 60, SCREEN_W, 28, stage == 0 ? CLR_BLUE_GREEN : (stage == 1 ? CLR_DARK_ORANGE : CLR_TRUE_BLUE));
    fillp_reset();

    // the scrolling world — one tileset, recolored per stage
    camera((int)scroll % 128, 0);
    stage_palette();
    map(0, 0, 0, 0, MAP_W, 13);
    pal_reset();
    camera(0, 0);

    // crates (behind actors)
    for (int i = 0; i < MAX_C; i++)
        if (crates[i].on) spr(crates[i].type ? 5 : 4, (int)crates[i].x, (int)crates[i].y);

    // actors — civilians + soldiers, with feet shadows
    for (int i = 0; i < MAX_E; i++) {
        Actor *e = &enemies[i];
        if (!e->on) continue;
        ovalfill((int)e->x + 8, (int)e->y + 15, 6, 2, CLR_BLACK);
        if (e->type == T_CIVILIAN) {
            if (e->state == S_DIE) { pal(7, 8); spr(3, (int)e->x, (int)e->y); pal_reset(); }
            else                     spr(3, (int)e->x, (int)e->y);
        } else {
            draw_soldier(e);
        }
    }

    // incoming projectiles — grow as they near you
    for (int i = 0; i < MAX_P; i++) {
        Proj *p = &projs[i];
        if (!p->on) continue;
        if (p->type == P_BULLET) {
            int r = 1 + (int)(p->prog * 2.5f);
            circfill((int)p->x, (int)p->y, r + 1, CLR_DARK_RED);
            circfill((int)p->x, (int)p->y, r, CLR_ORANGE);
            pset((int)p->x, (int)p->y, CLR_YELLOW);
        } else {
            circfill((int)p->x, (int)p->y, 3, CLR_DARK_GREY);
            circ((int)p->x, (int)p->y, 3, CLR_LIGHT_GREY);
            pset((int)p->x - 1, (int)p->y - 1, CLR_WHITE);
        }
    }

    // explosion rings
    for (int i = 0; i < MAX_BOOM; i++) {
        Boom *b = &booms[i];
        if (!b->on) continue;
        if (b->smoke) {
            fillp(FILL_CHECKER, -1);
            circfill((int)b->x, (int)b->y, (int)b->r, CLR_DARK_GREY);
            fillp_reset();
        } else {
            float t = b->life / b->maxlife;
            int c = t > 0.6f ? CLR_WHITE : (t > 0.3f ? CLR_YELLOW : CLR_ORANGE);
            circfill((int)b->x, (int)b->y, (int)b->r, c);
        }
    }

    // particles (shells, sparks, dust)
    for (int i = 0; i < MAX_PART; i++) {
        if (parts[i].life <= 0) continue;
        float t = parts[i].life / (float)parts[i].maxlife;
        int r = (int)(parts[i].size * (0.4f + t));
        if (r <= 0) pset((int)parts[i].x, (int)parts[i].y, parts[i].color);
        else        circfill((int)parts[i].x, (int)parts[i].y, r, parts[i].color);
    }

    // ---- the gun: a barrel aimed from you toward the crosshair, recoiling ----
    if (gstate != G_OVER) {
        float a = angle_to(GX, GY, (int)cx, (int)cy);
        int blen = 26 - recoil * 2;
        int mx = GX + (int)dx(blen, a), my = GY + (int)dy(blen, a);
        // hands / receiver
        rectfill(GX - 9, GY - 8 + recoil, 18, 14, CLR_DARKER_GREY);
        rectfill(GX - 6, GY - 12 + recoil, 12, 8, CLR_DARK_GREY);
        // barrel (two lines for weight)
        line(GX - 1, GY - 4 + recoil, mx - 1, my, CLR_DARK_GREY);
        line(GX + 1, GY - 4 + recoil, mx + 1, my, CLR_LIGHT_GREY);
        line(GX,     GY - 4 + recoil, mx,     my, CLR_MEDIUM_GREY);
        // muzzle flash + tracer
        if (muzzle > 0) {
            line(mx, my, (int)cx, (int)cy, CLR_YELLOW);
            circfill(mx, my, 4, CLR_WHITE);
            circfill(mx, my, 6, CLR_YELLOW);
            for (int s = 0; s < 4; s++) {
                float fa = a + rnd_between(-30, 30);
                line(mx, my, mx + (int)dx(9, fa), my + (int)dy(9, fa), CLR_ORANGE);
            }
        }
    }

    // ---- crosshair: tightens + turns red over a valid target ----
    {
        int ix = (int)cx, iy = (int)cy;
        bool over = false;
        for (int i = 0; i < MAX_E && !over; i++)
            if (enemies[i].on && enemies[i].state != S_DIE && enemies[i].type != T_CIVILIAN
                && near(ix, iy, (int)enemies[i].x + 8, (int)enemies[i].y + 8, 9)) over = true;
        for (int i = 0; i < MAX_C && !over; i++)
            if (crates[i].on && near(ix, iy, (int)crates[i].x + 8, (int)crates[i].y + 8, 10)) over = true;
        int rr = over ? 5 : 8;
        int col = hitmark > 0 ? CLR_WHITE : (over ? CLR_RED : CLR_LIGHT_GREY);
        circ(ix, iy, rr, col);
        line(ix - rr - 3, iy, ix - 2, iy, col);
        line(ix + 2, iy, ix + rr + 3, iy, col);
        line(ix, iy - rr - 3, ix, iy - 2, col);
        line(ix, iy + 2, ix, iy + rr + 3, col);
        pset(ix, iy, col);
        if (hitmark > 0) {                          // hit-marker X
            line(ix - 6, iy - 6, ix - 3, iy - 3, CLR_WHITE);
            line(ix + 6, iy - 6, ix + 3, iy - 3, CLR_WHITE);
            line(ix - 6, iy + 6, ix - 3, iy + 3, CLR_WHITE);
            line(ix + 6, iy + 6, ix + 3, iy + 3, CLR_WHITE);
        }
    }

    // ---- damage flash + low-health vignette ----
    if (dmg_flash > 0) {
        fillp(FILL_CHECKER, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_RED);
        fillp_reset();
    }
    if (health < 25 && blink(18)) {
        rect(0, 0, SCREEN_W, SCREEN_H, CLR_RED);
        rect(1, 1, SCREEN_W - 2, SCREEN_H - 2, CLR_DARK_RED);
    }

    // ---- HUD ----
    rectfill(0, 0, SCREEN_W, 13, CLR_BROWNISH_BLACK);
    print("HP", 4, 3, CLR_LIGHT_GREY);
    bar(20, 3, 64, 6, health / 100.0f, health < 25 ? CLR_RED : CLR_GREEN, CLR_DARKER_GREY);
    print("AMMO", 96, 3, CLR_LIGHT_GREY);
    bar(126, 3, 50, 6, ammo / (float)ammo_max, ammo <= 6 ? CLR_ORANGE : CLR_YELLOW, CLR_DARKER_GREY);
    for (int i = 0; i < rockets; i++)                // rockets as little icons
        rectfill(182 + i * 6, 3, 4, 6, CLR_DARK_ORANGE);
    print(str("S%d-%d", stage + 1, wave), SCREEN_W - 92, 3, CLR_LIGHT_PEACH);
    print_right(str("%d", score), SCREEN_W - 4, 3, CLR_WHITE);

    // ---- banners ----
    if (gstate == G_PLAY && banner_t > 0) {
        if (wave == 1 && stage <= 2) {              // stage intro
            print_centered(str("STAGE %d", stage + 1), SCREEN_W/2, SCREEN_H / 2 - 12, CLR_YELLOW);
            print_centered(STAGE_NAME[stage], SCREEN_W/2, SCREEN_H / 2, CLR_WHITE);
        } else {
            print_centered(str("WAVE %d", wave), SCREEN_W/2, SCREEN_H / 2 - 4, CLR_YELLOW);
        }
    }
    if (gstate == G_CLEAR) {
        rectfill(SCREEN_W / 2 - 70, SCREEN_H / 2 - 18, 140, 36, CLR_BLACK);
        rect    (SCREEN_W / 2 - 70, SCREEN_H / 2 - 18, 140, 36, CLR_GREEN);
        print_centered("STAGE CLEAR!", SCREEN_W/2, SCREEN_H / 2 - 10, CLR_GREEN);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 + 2, CLR_YELLOW);
    }
    if (gstate == G_OVER) {
        rectfill(SCREEN_W / 2 - 78, SCREEN_H / 2 - 28, 156, 60, CLR_BLACK);
        rect    (SCREEN_W / 2 - 78, SCREEN_H / 2 - 28, 156, 60, win ? CLR_YELLOW : CLR_RED);
        print_centered(win ? "MISSION COMPLETE" : "K.I.A.", SCREEN_W/2, SCREEN_H / 2 - 18, win ? CLR_YELLOW : CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 - 4, CLR_WHITE);
        print_centered(str("BEST %d", best), SCREEN_W/2, SCREEN_H / 2 + 6, CLR_LIGHT_GREY);
        if (blink(20)) print_centered("CLICK TO REDEPLOY", SCREEN_W/2, SCREEN_H / 2 + 18, CLR_GREEN);
    }
}
