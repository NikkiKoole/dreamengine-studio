/* de:meta
{
  "title": "doom",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game",
    "tech-demo"
  ],
  "teaches": [
    "raycasting",
    "particle-system",
    "finite-state-ai",
    "title-play-gameover-loop"
  ],
  "lineage": "Extends the repo's raycaster base (DDA per-column wall strips) with a z-buffer for billboard-sprite occlusion — the classical Doom renderer trick; fixed authored level rather than procedural.",
  "genre": "shooter",
  "homage": "Doom (1993)",
  "description": "A Doom-style first-person shooter on the raycaster — stalk a base of corridors blasting charging imps and shooting gunners, drawn as distance-scaled billboard sprites that hide behind walls via a depth buffer. Two weapons (pistol / shotgun), pickups, a red keycard that unlocks the exit, muzzle-flash room lighting and gore. WASD/arrows move + turn, Q/E strafe, Z fire, X (or 1/2) switch weapon, SPACE open doors / hit the exit switch."
}
de:meta */
#include "studio.h"
#include <math.h>   // fabsf — used in the DDA and angle math

// DOOM — a Wolfenstein/Doom-style corridor shooter built on the raycaster.
//
// The world is a 24×16 grid of walls. For every screen column we shoot one DDA
// ray and draw a distance-shaded wall strip — exactly the raycaster base — but
// now we also stash each column's wall depth in a Z-BUFFER. Enemies and pickups
// are flat billboard sprites: we project each to a screen X, scale it by
// distance, then draw it as 1-px vertical strips, skipping any strip the wall in
// front already covers. That gives proper occlusion — demons hide behind walls.
//
// Two weapons (pistol / shotgun), two enemy types (charging imps, shooting
// gunners), pickups, a red keycard gating the exit door, and a switch to escape.
//
//   WASD / arrows   move + turn        Q / E   strafe
//   Z  fire          X (or 1/2) weapon  SPACE   open door / hit exit switch

#define GW       24
#define GH       16
#define FOV      66.0f
#define HUD_H    34
#define VIEW_H   (SCREEN_H - HUD_H)   // 3D viewport height (top of screen)
#define HORIZON  (VIEW_H / 2)

// ── world ───────────────────────────────────────────────────────────────────
// cell codes: 0 floor · 1/2/3 wall variants · 8 door · 9 locked door · 10 exit
static int grid[GH][GW];

static int grid_get(int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= GW || cy >= GH) return 2;   // out of bounds = wall
    return grid[cy][cx];
}
static int solidf(float x, float y) { return grid_get((int)x, (int)y) != 0; }

// line-of-sight: march from a→b in small steps, blocked by any solid cell
static bool los(float x0, float y0, float x1, float y1) {
    float dd = distance((int)(x0*100), (int)(y0*100), (int)(x1*100), (int)(y1*100)) / 100.0f;
    int steps = (int)(dd / 0.08f);
    for (int i = 1; i < steps; i++) {
        float t = (float)i / steps;
        if (solidf(x0 + (x1-x0)*t, y0 + (y1-y0)*t)) return false;
    }
    return true;
}

static int wall_color(int v, int dark) {
    switch (v) {
        case 1:  return dark ? 5  : 6;    // grey
        case 2:  return dark ? 21 : 22;   // warm tech
        case 3:  return dark ? 19 : 12;   // blue tech
        case 8:  return dark ? 4  : 9;    // door (brown/orange)
        case 9:  return dark ? 24 : 8;    // locked door (red)
        case 10: return blink(14) ? 11 : 26;  // exit switch — pulsing green
        default: return dark ? 5  : 6;
    }
}

// ── player ──────────────────────────────────────────────────────────────────
static float px, py, pa;
static int   hp, armor, ammo, weapon;   // weapon 0=pistol 1=shotgun
static bool  has_key;
static float bob, fire_cd, flash_muzzle, flash_dmg, msg_t;
static const char *msg = "";
static int   gamestate;                 // 0 play · 1 dead · 2 won
static float zbuf[SCREEN_W];

// ── enemies ─────────────────────────────────────────────────────────────────
#define MAXE 12
typedef struct { bool active; int type, hp, state; float x, y, t, cd, flash_t; } Enemy;
// type 0=imp(melee) 1=gunner(ranged) · state 0=asleep 1=chase 2=attack 3=dead
static Enemy en[MAXE];

// ── pickups ─────────────────────────────────────────────────────────────────
#define MAXP 8
typedef struct { bool active; int kind; float x, y; } Pickup;   // kind 0=ammo 1=med 2=armor 3=key
static Pickup pk[MAXP];

// ── particles (screen-space blood / gibs / sparks) ───────────────────────────
#define MAXPART 80
typedef struct { bool a; float x, y, vx, vy, life; int col; } Part;
static Part part[MAXPART];

static void spawn_burst(int x, int y, int n, int c1, int c2) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < MAXPART; i++)
            if (!part[i].a) {
                float ang = rnd_between(0, 360), sp = rnd_float_between(0.6f, 2.6f);
                part[i] = (Part){ true, x, y, dx(sp, ang), dy(sp, ang) - 1.0f,
                                  rnd_float_between(0.3f, 0.75f), rnd(2) ? c1 : c2 };
                break;
            }
}

// ── level setup ──────────────────────────────────────────────────────────────
static void add_enemy(int type, float x, float y) {
    for (int i = 0; i < MAXE; i++)
        if (!en[i].active) {
            en[i] = (Enemy){ true, type, type == 0 ? 4 : 3, 0, x, y, 0, 0, 0 };
            return;
        }
}
static void add_pickup(int kind, float x, float y) {
    for (int i = 0; i < MAXP; i++)
        if (!pk[i].active) { pk[i] = (Pickup){ true, kind, x, y }; return; }
}

static void reset_level(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) grid[y][x] = 0;
    for (int x = 0; x < GW; x++) { grid[0][x] = 2; grid[GH-1][x] = 2; }
    for (int y = 0; y < GH; y++) { grid[y][0] = 2; grid[y][GW-1] = 2; }
    for (int y = 1; y < GH-1; y++) grid[y][16] = 2;     // wall sealing the exit room
    grid[4][16]  = 9;                                   // locked door → exit room
    grid[7][GW-1] = 10;                                 // exit switch (east wall)
    for (int y = 1; y <= 6; y++) grid[y][8] = 2;        // start-room / hub divider
    grid[3][8] = 8;                                     // normal door
    for (int x = 1; x <= 15; x++) grid[8][x] = 2;       // upper / lower divider
    grid[8][4] = 0; grid[8][11] = 0;                    // two gaps through it
    grid[10][6] = 3; grid[12][10] = 3;                  // cover pillars

    px = 2.5f; py = 2.5f; pa = 8.0f;
    hp = 100; armor = 0; ammo = 24; weapon = 0; has_key = false;
    bob = fire_cd = flash_muzzle = flash_dmg = msg_t = 0; gamestate = 0;

    for (int i = 0; i < MAXE; i++)  en[i].active = false;
    for (int i = 0; i < MAXP; i++)  pk[i].active = false;
    for (int i = 0; i < MAXPART; i++) part[i].a = false;

    add_enemy(0,  5.5f,  2.5f);   // imp in the start room — instant action
    add_enemy(0, 12.5f,  3.5f);   // hub
    add_enemy(1, 13.5f,  5.5f);
    add_enemy(1,  4.5f, 11.5f);   // lower room
    add_enemy(0, 12.5f, 12.5f);
    add_enemy(1, 19.5f,  7.5f);   // exit-room guard

    add_pickup(0, 10.5f,  2.5f);  // ammo  (hub)
    add_pickup(2, 14.5f,  2.5f);  // armor (hub)
    add_pickup(1,  3.5f, 13.5f);  // medkit(lower)
    add_pickup(3, 13.5f, 13.5f);  // keycard(lower) — needed for the exit
}

void init(void) {
    colorkey(0);                 // sprite index 0 = transparent
    bpm(132);
    instrument(5, INSTR_NOISE, 1, 40, 0, 60);   instrument_filter(5, FILTER_HIGH, 1500, 5);  // pistol crack
    instrument(6, INSTR_NOISE, 1, 130, 0, 180); instrument_filter(6, FILTER_LOW, 1000, 4);   // boom / hurt
    instrument(7, INSTR_SAW, 40, 200, 3, 260);  instrument_filter(7, FILTER_LOW, 500, 9);    // demon growl
    instrument_lfo(7, 0, LFO_PITCH, 5.0f, 0.6f);
    instrument(8, INSTR_TRI, 500, 300, 5, 900);                                              // ambient drone
    instrument(9, INSTR_SQUARE, 1, 50, 0, 70);                                               // pickup blip
    instrument(10, INSTR_SAW, 2, 90, 2, 130);   instrument_filter(10, FILTER_LOW, 800, 7);   // metal bass
    reset_level();
}

// ── combat ───────────────────────────────────────────────────────────────────
static bool enemy_aim(int i, float *rel, float *ed) {
    float r = angle_to((int)(px*100), (int)(py*100), (int)(en[i].x*100), (int)(en[i].y*100)) - pa;
    while (r > 180) r -= 360; while (r < -180) r += 360;
    *rel = r;
    *ed  = distance((int)(px*100), (int)(py*100), (int)(en[i].x*100), (int)(en[i].y*100)) / 100.0f;
    return cos_deg(r) > 0;   // in front of the player
}

static void damage_enemy(int i, int dmg) {
    en[i].hp -= dmg; en[i].flash_t = 0.12f;
    if (en[i].state == 0) en[i].state = 1;             // shooting wakes them
    float rel, ed; enemy_aim(i, &rel, &ed);
    int sx = (int)((rel / FOV + 0.5f) * (SCREEN_W - 1));
    spawn_burst(sx, HORIZON, 5, CLR_RED, CLR_DARK_RED);
    if (en[i].hp <= 0) {
        en[i].state = 3; en[i].t = 0;
        spawn_burst(sx, HORIZON, 14, CLR_RED, CLR_DARK_RED);
        note(rnd_between(48, 58), 6, 4);
        note(rnd_between(28, 34), 7, 4);
    }
}

static int aim_target(float tol, float range) {
    int best = -1; float bestd = 1e9f;
    for (int i = 0; i < MAXE; i++) {
        if (!en[i].active || en[i].state == 3) continue;
        float rel, ed;
        if (!enemy_aim(i, &rel, &ed)) continue;
        if (ed < range && fabsf(rel) < tol && ed < bestd && los(en[i].x, en[i].y, px, py)) {
            best = i; bestd = ed;
        }
    }
    return best;
}

static void shoot(void) {
    if (fire_cd > 0 || gamestate != 0) return;
    if (weapon == 1 && ammo <= 0) { note(45, 9, 2); return; }   // empty click
    flash_muzzle = 0.08f;
    spawn_burst(SCREEN_W/2, VIEW_H - 52, 6, CLR_YELLOW, CLR_ORANGE);
    if (weapon == 0) {                                          // pistol — precise, infinite
        fire_cd = 0.20f; hit(64, 5, 5, 60);
        int t = aim_target(7.0f, 16.0f);
        if (t >= 0) damage_enemy(t, 1);
    } else {                                                    // shotgun — spread, ammo
        ammo--; fire_cd = 0.70f; shake(7); hit(40, 6, 6, 210);
        for (int i = 0; i < MAXE; i++) {
            if (!en[i].active || en[i].state == 3) continue;
            float rel, ed;
            if (!enemy_aim(i, &rel, &ed)) continue;
            if (ed < 11.0f && fabsf(rel) < 15.0f && los(en[i].x, en[i].y, px, py))
                damage_enemy(i, fabsf(rel) < 6.0f ? 4 : 1);
        }
    }
}

static void hurt_player(int dmg) {
    if (armor > 0) { int a = dmg / 2; if (a > armor) a = armor; armor -= a; dmg -= a; }
    hp -= dmg; flash_dmg = 0.4f; shake(5); hit(42, 6, 5, 180);
    if (hp <= 0) { hp = 0; gamestate = 1; note(30, 7, 5); }
}

static void try_use(void) {
    for (float r = 0.4f; r <= 1.3f; r += 0.3f) {
        int cx = (int)(px + dx(r, pa)), cy = (int)(py + dy(r, pa));
        int v = grid_get(cx, cy);
        if (v == 8) { grid[cy][cx] = 0; shake(3); hit(30, 6, 4, 260); msg = "DOOR OPENED"; msg_t = 1.0f; return; }
        if (v == 9) {
            if (has_key) { grid[cy][cx] = 0; shake(3); note(72, 9, 5); schedule(90, 84, 9, 5);
                           msg = "KEYCARD ACCEPTED"; msg_t = 1.2f; }
            else         { note(36, 9, 3); msg = "NEED THE RED KEYCARD"; msg_t = 1.2f; }
            return;
        }
        if (v == 10) {                                          // win!
            gamestate = 2;
            schedule(0, 72, 9, 6); schedule(160, 76, 9, 6);
            schedule(320, 79, 9, 6); schedule(480, 84, 9, 6);
            return;
        }
    }
}

// ── update ────────────────────────────────────────────────────────────────────
void update(void) {
    // restart on the death / win screen
    if (gamestate != 0) {
        if (btnp(0, BTN_A) || btnp(1, BTN_A) || keyp(KEY_SPACE)) reset_level();
        return;
    }

    float spd = 2.6f * dt(), trn = 130.0f * dt();
    if (btn(0, BTN_LEFT)  || btn(1, BTN_LEFT))  pa -= trn;
    if (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) pa += trn;

    float mv = 0, st = 0;
    if (btn(0, BTN_UP)   || btn(1, BTN_UP))   mv =  spd;
    if (btn(0, BTN_DOWN) || btn(1, BTN_DOWN)) mv = -spd;
    if (key('Q')) st = -spd;
    if (key('E')) st =  spd;
    if (mv != 0 || st != 0) {
        float mx = dx(mv, pa) + dx(st, pa + 90), my = dy(mv, pa) + dy(st, pa + 90);
        float pad = 0.2f, nx = px + mx, ny = py + my;
        if (!solidf(nx + (mx > 0 ? pad : -pad), py)) px = nx;
        if (!solidf(px, ny + (my > 0 ? pad : -pad))) py = ny;
        bob += dt() * 420.0f;                              // head-bob while moving
    }

    if (btnp(0, BTN_B) || btnp(1, BTN_B)) { weapon ^= 1; note(60, 9, 3); }
    if (keyp('1')) weapon = 0;
    if (keyp('2')) weapon = 1;
    if (btnp(0, BTN_A) || btnp(1, BTN_A)) shoot();
    if (keyp(KEY_SPACE)) try_use();

    // enemies
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &en[i];
        if (!e->active) continue;
        if (e->flash_t > 0) e->flash_t -= dt();
        if (e->cd > 0)      e->cd      -= dt();
        if (e->state == 3) continue;

        float d  = distance((int)(px*100), (int)(py*100), (int)(e->x*100), (int)(e->y*100)) / 100.0f;
        bool  lo = los(e->x, e->y, px, py);
        float ang = angle_to((int)(e->x*100), (int)(e->y*100), (int)(px*100), (int)(py*100));

        if (e->state == 0) {
            if (d < 8.0f && lo) { e->state = 1; e->t = 0; note(rnd_between(30, 40), 7, 4); }
        } else if (e->state == 1) {
            float es = (e->type == 0 ? 1.5f : 1.0f) * dt();   // imps charge faster
            float ex = e->x + dx(es, ang), ey = e->y + dy(es, ang);
            if (!solidf(ex, e->y)) e->x = ex;
            if (!solidf(e->x, ey)) e->y = ey;
            if (e->type == 0) { if (d < 1.1f) { e->state = 2; e->t = 0; } }
            else              { if (d < 6.0f && lo && e->cd <= 0) { e->state = 2; e->t = 0; note(rnd_between(34,42),7,3); } }
        } else if (e->state == 2) {
            e->t += dt();
            float tele = (e->type == 0 ? 0.35f : 0.55f);      // wind-up telegraph
            if (e->t >= tele) {
                if (e->type == 0) { if (d < 1.4f) hurt_player(12); e->cd = 0.5f; }
                else              { if (lo && d < 8.0f && chance(70)) hurt_player(10); e->cd = 1.4f; }
                e->state = 1;
            }
        }
    }

    // pickups
    for (int i = 0; i < MAXP; i++) {
        if (!pk[i].active) continue;
        if (distance((int)(px*100),(int)(py*100),(int)(pk[i].x*100),(int)(pk[i].y*100))/100.0f < 0.6f) {
            switch (pk[i].kind) {
                case 0: ammo += 8; msg = "+8 SHELLS";  break;
                case 1: hp = min(100, hp + 25); msg = "+25 HEALTH"; break;
                case 2: armor = min(100, armor + 50); msg = "+50 ARMOR"; break;
                case 3: has_key = true; msg = "GOT THE RED KEYCARD"; break;
            }
            msg_t = 1.4f; pk[i].active = false;
            note(72, 9, 4); schedule(70, 79, 9, 4);
        }
    }

    // particles
    for (int i = 0; i < MAXPART; i++)
        if (part[i].a) {
            part[i].x += part[i].vx; part[i].y += part[i].vy; part[i].vy += 0.15f;
            part[i].life -= dt();
            if (part[i].life <= 0) part[i].a = false;
        }

    // timers + a driving metal/ambient bed
    if (flash_muzzle > 0) flash_muzzle -= dt();
    if (flash_dmg > 0)    flash_dmg    -= dt();
    if (msg_t > 0)        msg_t        -= dt();
    if (every(2)) note(28, 10, 2);                       // low bass pulse
    if (every(8)) note(24, 8, 2);                        // ambient drone
    if (hp > 0 && hp < 30 && frame() % 40 == 0) hit(33, 6, 3, 150);   // low-health heartbeat
}

// ── billboard sprite (1-px strips, occluded by the wall Z-buffer) ─────────────
static void draw_sprite_world(float wx, float wy, int srcX, int srcY,
                              float hf, float yoff, int flashKind) {
    float rel = angle_to((int)(px*100),(int)(py*100),(int)(wx*100),(int)(wy*100)) - pa;
    while (rel > 180) rel -= 360; while (rel < -180) rel += 360;
    float ed = distance((int)(px*100),(int)(py*100),(int)(wx*100),(int)(wy*100)) / 100.0f;
    float cd = ed * cos_deg(rel);
    if (cd < 0.25f) return;

    float scale   = VIEW_H / cd;
    float spriteH = scale * hf, spriteW = spriteH;
    float floorLn = HORIZON + scale * 0.5f;
    int   drawTop = (int)(floorLn - yoff * scale - spriteH);
    float screenX = (rel / FOV + 0.5f) * (SCREEN_W - 1);
    int   x0 = (int)(screenX - spriteW / 2), iw = (int)spriteW; if (iw < 1) iw = 1;
    int   ih = (int)spriteH; if (ih < 1) ih = 1;

    if (flashKind == 0) { pal(8,7); pal(24,7); pal(15,7); pal(10,7); pal(16,7); }
    else if (flashKind == 1) { pal(11,7); pal(3,7); pal(15,7); pal(8,7); pal(16,7); }

    for (int sx = x0; sx < x0 + iw; sx++) {
        if (sx < 0 || sx >= SCREEN_W || cd >= zbuf[sx]) continue;
        int texX = (int)((float)(sx - x0) / iw * 16); if (texX < 0) texX = 0; if (texX > 15) texX = 15;
        sspr(srcX + texX, srcY, 1, 16, sx, drawTop, 1, ih);
    }
    if (flashKind >= 0) pal_reset();
}

static int enemy_slot(Enemy *e) {
    int base = (e->type == 0) ? 0 : 4;
    if (e->state == 3) return base + 3;                 // corpse
    if (e->state == 2) return base + 2;                 // attack telegraph
    return base + anim(2, 6, e->x * 0.13f);             // walk cycle
}

void draw(void) {
    clip(0, 0, SCREEN_W, VIEW_H);

    // ceiling + floor
    rectfill(0, 0, SCREEN_W, HORIZON, CLR_DARKER_BLUE);
    rectfill(0, HORIZON, SCREEN_W, VIEW_H - HORIZON, CLR_BROWNISH_BLACK);

    // walls (DDA per column) — fills the depth buffer
    for (int x = 0; x < SCREEN_W; x++) {
        float offset = ((float)x / (SCREEN_W - 1) - 0.5f) * FOV;
        float ra = pa + offset, rdx = cos_deg(ra), rdy = sin_deg(ra);
        int mapX = (int)px, mapY = (int)py;
        float ddx = (rdx == 0) ? 1e30f : fabsf(1.0f / rdx);
        float ddy = (rdy == 0) ? 1e30f : fabsf(1.0f / rdy);
        int stepX, stepY; float sideX, sideY;
        if (rdx < 0) { stepX = -1; sideX = (px - mapX) * ddx; } else { stepX = 1; sideX = (mapX + 1.0f - px) * ddx; }
        if (rdy < 0) { stepY = -1; sideY = (py - mapY) * ddy; } else { stepY = 1; sideY = (mapY + 1.0f - py) * ddy; }
        int side = 0, hit = 0;
        for (int g = 0; g < 96 && !hit; g++) {
            if (sideX < sideY) { sideX += ddx; mapX += stepX; side = 0; }
            else               { sideY += ddy; mapY += stepY; side = 1; }
            if (grid_get(mapX, mapY)) hit = 1;
        }
        float dist = (side == 0) ? (sideX - ddx) : (sideY - ddy);
        dist *= cos_deg(offset);
        if (dist < 0.01f) dist = 0.01f;
        zbuf[x] = dist;
        int h = (int)(VIEW_H / dist);
        int y0 = HORIZON - h / 2, y1 = HORIZON + h / 2;
        if (y0 < 0) y0 = 0; if (y1 > VIEW_H - 1) y1 = VIEW_H - 1;
        int dark = (side == 1) || (dist > 5.0f);
        line(x, y0, x, y1, wall_color(grid_get(mapX, mapY), dark));
    }

    // gather sprites (enemies + pickups), sort far→near, draw with occlusion
    typedef struct { float d, wx, wy, hf, yoff; int srcX, srcY, flash; } Spr;
    Spr list[MAXE + MAXP]; int n = 0;
    for (int i = 0; i < MAXE; i++) {
        if (!en[i].active) continue;
        int slot = enemy_slot(&en[i]);
        list[n++] = (Spr){
            distance((int)(px*100),(int)(py*100),(int)(en[i].x*100),(int)(en[i].y*100))/100.0f,
            en[i].x, en[i].y, en[i].state == 3 ? 0.45f : 0.9f, 0,
            (slot % 8) * 16, (slot / 8) * 16, en[i].flash_t > 0 ? en[i].type : -1 };
    }
    for (int i = 0; i < MAXP; i++) {
        if (!pk[i].active) continue;
        int slot = (int[]){ 11, 9, 10, 8 }[pk[i].kind];   // ammo med armor key
        list[n++] = (Spr){
            distance((int)(px*100),(int)(py*100),(int)(pk[i].x*100),(int)(pk[i].y*100))/100.0f,
            pk[i].x, pk[i].y, 0.42f, 0.06f + 0.04f * sin_deg(now() * 140 + pk[i].x * 50),
            (slot % 8) * 16, (slot / 8) * 16, -1 };
    }
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++)
        if (list[b].d > list[a].d) { Spr t = list[a]; list[a] = list[b]; list[b] = t; }
    for (int i = 0; i < n; i++)
        draw_sprite_world(list[i].wx, list[i].wy, list[i].srcX, list[i].srcY,
                          list[i].hf, list[i].yoff, list[i].flash);

    // particles
    for (int i = 0; i < MAXPART; i++)
        if (part[i].a) rectfill((int)part[i].x, (int)part[i].y, 2, 2, part[i].col);

    // muzzle flash lights the room
    if (flash_muzzle > 0) {
        fillp(FILL_DOTS, -1); rectfill(0, 0, SCREEN_W, VIEW_H, CLR_YELLOW); fillp_reset();
        ovalfill(SCREEN_W / 2, VIEW_H - 50, 20, 13, CLR_WHITE);
    }

    // weapon view-model (32×16 sheet region scaled 4×), with bob + recoil
    {
        bool firing = flash_muzzle > 0;
        int srcX = firing ? 32 : 0, srcY = (weapon == 0) ? 32 : 48;
        int bx = (int)(sin_deg(bob) * 4), by = (int)(-fabsf(cos_deg(bob)) * 3) + (firing ? 9 : 0);
        sspr(srcX, srcY, 32, 16, (SCREEN_W - 128) / 2 + bx, VIEW_H - 64 + by, 128, 64);
    }

    // crosshair
    line(SCREEN_W/2 - 3, HORIZON, SCREEN_W/2 + 3, HORIZON, CLR_RED);
    line(SCREEN_W/2, HORIZON - 3, SCREEN_W/2, HORIZON + 3, CLR_RED);

    if (msg_t > 0) print_centered(msg, SCREEN_W/2, VIEW_H - 16, CLR_YELLOW);
    if (now() < 7) print_centered("WASD MOVE  QE STRAFE  Z FIRE  X WEAPON  SPACE USE", SCREEN_W/2, 3, CLR_LIGHT_GREY);

    clip(0, 0, 0, 0);

    // full-screen damage flash (covers the HUD too)
    if (flash_dmg > 0) {
        fillp(flash_dmg > 0.25f ? FILL_CHECKER : FILL_DOTS, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_RED); fillp_reset();
    }

    // ── HUD ──
    rectfill(0, VIEW_H, SCREEN_W, HUD_H, CLR_BROWNISH_BLACK);
    line(0, VIEW_H, SCREEN_W, VIEW_H, CLR_DARK_GREY);
    int fslot = (flash_dmg > 0 || hp < 30) ? 13 : 12;   // hurt vs ok face
    sspr((fslot % 8) * 16, (fslot / 8) * 16, 16, 16, SCREEN_W/2 - 12, VIEW_H + 5, 24, 24);
    print("HEALTH", 6, VIEW_H + 3, CLR_LIGHT_GREY);
    print(str("%d", hp), 6, VIEW_H + 12, CLR_WHITE);
    bar(6, VIEW_H + 22, 72, 5, hp / 100.0f, hp < 30 ? CLR_RED : CLR_GREEN, CLR_DARKER_GREY);
    print("ARMOR", 92, VIEW_H + 3, CLR_LIGHT_GREY);
    bar(92, VIEW_H + 12, 50, 5, armor / 100.0f, CLR_BLUE, CLR_DARKER_GREY);
    print_right(weapon == 0 ? "PISTOL" : "SHOTGUN", SCREEN_W - 6, VIEW_H + 3, CLR_YELLOW);
    print_right(weapon == 0 ? "INF" : str("%d", ammo), SCREEN_W - 6, VIEW_H + 12, CLR_WHITE);
    if (has_key) print_right("KEYCARD", SCREEN_W - 6, VIEW_H + 22, CLR_RED);

    // overlays
    if (gamestate == 1) {
        fade(0.55f);
        const char *t = "YOU DIED";
        print_scaled(t, (SCREEN_W - text_width(t) * 3) / 2, 64, CLR_RED, 3);
        print_centered("Z TO RESTART", SCREEN_W/2, 108, CLR_WHITE);
    } else if (gamestate == 2) {
        fade(0.55f);
        const char *t = "LEVEL CLEAR";
        print_scaled(t, (SCREEN_W - text_width(t) * 2) / 2, 62, CLR_YELLOW, 2);
        print_centered("YOU ESCAPED THE BASE", SCREEN_W/2, 92, CLR_WHITE);
        print_centered("Z TO PLAY AGAIN", SCREEN_W/2, 108, CLR_LIGHT_GREY);
    }
}
