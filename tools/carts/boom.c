#include "studio.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// BOOM  —  an explosion + fire sandbox. Click to detonate; watch the blast
//          ignite the world and the fire crawl, spread, and smoke on the wind.
//
//   MODES (TAB toggles)
//     BOOM    click anywhere (or SPACE) → detonate. mouse WHEEL = charge size.
//     BUILD   click / drag to paint the world. 1-6 pick a material;
//             7 (or T) = torch: light a fire by hand.
//
//   ALWAYS    C clear all fire & smoke      R reset the scene
//
// ── what's honest here (two layers, each doing one real job) ─────────────────
//
//   1. THE WORLD is a grid of MATERIAL cells (road / grass / wood / oil / water),
//      each with FUEL and a FIRE level 0-7 — navkit's fire model flattened to 2D.
//      Fire spreads to neighbours by probability (base + level − the target's
//      ignition resistance), biased downwind, choked by water. Fuel burns down;
//      when it runs out the cell turns to ASH. Grass flashes over, wood smoulders
//      for ages, oil goes up eager and hot, road & water never burn. Nothing
//      scripted — the fire front is whatever the fuel map and the wind allow.
//
//   2. EVERYTHING AIRBORNE is a PARTICLE — fireball, sparks, ballistic debris,
//      and the smoke column. Flames are born white-hot and COOL along a blackbody
//      ramp (white→yellow→orange→red→dark) as they age, rising on their own
//      buoyancy. A detonation reads the way a real one does: a bright core, an
//      expanding shockwave RING that shoves nearby particles outward, the
//      fireball, then a slow smoke plume that lingers long after the flame.
//
// A detonation seeds layer 1 (ignites fuel in its radius, craters the core to
// ash) and layer 1's burning cells feed layer 2 (every hot cell breathes flame,
// embers, and smoke). Oil that catches hot can POP a secondary blast, so a slick
// chains across itself and the wind decides what's left standing. This is the
// sandbox seed for the roam-and-burn city game.
// ============================================================================

#define CELL  4
#define GW    (SCREEN_W / CELL)
#define GH    (SCREEN_H / CELL)
#define NCELL (GW * GH)
#define IDX(x,y) ((y) * GW + (x))

// ── materials ────────────────────────────────────────────────────────────────
// fuel  = how long a cell burns once lit.   resist = % shaved off a neighbour's
// spread roll (high = hard to catch; oil is negative = eager).   col = cold tint.
enum { MAT_GROUND, MAT_ROAD, MAT_GRASS, MAT_WOOD, MAT_OIL, MAT_WATER, MAT_ASH, MAT_KINDS };
static const int MAT_FUEL[MAT_KINDS]   = {   0,   0,  20,  90,  45,   0,   0 };
static const int MAT_RESIST[MAT_KINDS] = {   0,   0,   0,  20, -40,   0,   0 };
static const int MAT_COL[MAT_KINDS]    = {
    CLR_BROWNISH_BLACK, CLR_DARK_GREY, CLR_DARK_GREEN, CLR_BROWN,
    CLR_DARKER_GREY,    CLR_DARK_BLUE, CLR_BROWNISH_BLACK
};
static const char *MAT_NAME[MAT_KINDS] = { "ground","road","grass","wood","oil","water","ash" };

// ── the world grid ────────────────────────────────────────────────────────────
typedef struct { unsigned char mat, fuel, fire; } Cell;
static Cell W[NCELL];
static unsigned char prevfire[NCELL];   // snapshot so spread reads a stable frame

// ── fire CA tuning (the navkit numbers, per-tick instead of per-game-hour) ─────
#define SPREAD_EVERY   5     // frames between spread/burn ticks
#define SPREAD_BASE    8
#define SPREAD_PER     11    // chance = BASE + level*PER − resist
#define FIRE_MAX       7
#define MIN_SPREAD     2

// blackbody flame ramp, cool → hot (index by heat 0..1)
static const int FLAME_RAMP[] = {
    CLR_DARK_RED, CLR_RED, CLR_DARK_ORANGE, CLR_ORANGE,
    CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_WHITE
};
#define NFLAME 7

// ── particles (everything airborne) ───────────────────────────────────────────
enum { PK_FREE, PK_SMOKE, PK_FLAME, PK_EMBER, PK_SPARK, PK_DEBRIS };
typedef struct {
    float x, y, vx, vy;
    float life, maxlife;
    float heat;          // 0..1, flames cool toward 0
    float size;
    unsigned char kind, col;
} P;
#define MAXP 1800
static P ps[MAXP];
static int pcur = 0;

// shockwave rings (visual + a one-shot push happens at detonation time)
typedef struct { float x, y, r, maxr; int on; } Ring;
static Ring rings[12];

// ── global state ───────────────────────────────────────────────────────────
static int   mode;            // 0 = BOOM, 1 = BUILD
static int   brush = MAT_WOOD;
static int   charge = 3;      // 1..5
static float windx, windy;    // current wind vector
static float wtx, wty;        // wind target (lerped toward)
static int   wind_t;
static int   spread_t;
static int   flash;           // brief screen-edge flash frames
static int   dets_this_tick;  // cap secondary (oil) detonations
static unsigned int rng = 0x1234567u;

// cheap deterministic-enough float for shimmer (rnd_float is fine too)
static inline float frand(void) {
    rng = rng * 1664525u + 1013904223u;
    return (rng >> 8) / 16777216.0f;
}

// ── particle spawn ─────────────────────────────────────────────────────────
static void psp(float x, float y, float vx, float vy, int kind,
                float life, float size, float heat, int col) {
    for (int n = 0; n < MAXP; n++) {
        int i = (pcur + n) % MAXP;
        if (ps[i].kind == PK_FREE) {
            ps[i] = (P){ x, y, vx, vy, life, life, heat, size,
                         (unsigned char)kind, (unsigned char)col };
            pcur = (i + 1) % MAXP;
            return;
        }
    }
}

static void ignite(int gx, int gy) {
    if (gx < 0 || gy < 0 || gx >= GW || gy >= GH) return;
    Cell *c = &W[IDX(gx, gy)];
    if (MAT_FUEL[c->mat] == 0 || c->fire > 0) return;
    if (c->fuel == 0) c->fuel = (unsigned char)MAT_FUEL[c->mat];
    c->fire = MIN_SPREAD;
}

// ── the detonation ───────────────────────────────────────────────────────────
static void detonate(float cx, float cy, int chg) {
    if (chg < 1) chg = 1;
    float R = (4 + chg * 4) * CELL;          // effect radius in pixels
    int   radc = 4 + chg * 4;                 // and in cells

    shake(2.0f + chg * 2.2f);
    flash = 3 + chg;
    // boom: a low noise thud + a sub rumble, louder with charge
    hit(28 + rnd(4), INSTR_NOISE, 70 + chg * 6, 220 + chg * 40);
    hit(20, INSTR_NOISE, 40 + chg * 5, 320);

    // shockwave ring
    for (int i = 0; i < 12; i++) if (!rings[i].on) {
        rings[i] = (Ring){ cx, cy, 2, R * 1.5f, 1 };
        break;
    }

    // fireball — flames born hot, flung outward, buoyant
    int nf = 16 + chg * 22;
    for (int i = 0; i < nf; i++) {
        float a = frand() * 6.2832f;
        float s = rnd_float_between(0.4f, 2.2f + chg * 0.5f);
        psp(cx, cy, cosf(a) * s, sinf(a) * s, PK_FLAME,
            16 + frand() * (20 + chg * 4), 2 + frand() * 4,
            0.7f + frand() * 0.3f, 0);
    }
    // sparks — fast, gravity, bright, brief
    int ns = 10 + chg * 8;
    for (int i = 0; i < ns; i++) {
        float a = frand() * 6.2832f;
        float s = rnd_float_between(1.5f, 4.0f + chg);
        psp(cx, cy, cosf(a) * s, sinf(a) * s, PK_SPARK,
            8 + frand() * 12, 1, 1, CLR_LIGHT_YELLOW);
    }
    // debris — ballistic chunks, heavy drag
    int nd = 4 + chg * 4;
    for (int i = 0; i < nd; i++) {
        float a = frand() * 6.2832f;
        float s = rnd_float_between(1.5f, 3.5f + chg);
        psp(cx, cy, cosf(a) * s, sinf(a) * s, PK_DEBRIS,
            22 + frand() * 24, 1 + frand() * 2, 0,
            frand() < 0.5f ? CLR_DARK_GREY : CLR_BROWN);
    }
    // a first breath of smoke
    int nm = 6 + chg * 4;
    for (int i = 0; i < nm; i++) {
        float a = frand() * 6.2832f;
        float s = rnd_float_between(0.2f, 1.0f);
        psp(cx, cy, cosf(a) * s, sinf(a) * s - 0.4f, PK_SMOKE,
            60 + frand() * 80, 3 + frand() * 4, 0, 0);
    }

    // shove existing particles outward (the blast wave)
    float R2 = R * 1.6f;
    for (int i = 0; i < MAXP; i++) {
        if (ps[i].kind == PK_FREE) continue;
        float dx = ps[i].x - cx, dy = ps[i].y - cy;
        float d = sqrtf(dx * dx + dy * dy) + 0.01f;
        if (d < R2) {
            float push = (R2 - d) / R2 * (3.0f + chg);
            ps[i].vx += dx / d * push;
            ps[i].vy += dy / d * push;
        }
    }

    // the world: crater the core to ash, ignite a burning ring around it
    int cgx = (int)(cx / CELL), cgy = (int)(cy / CELL);
    for (int gy = cgy - radc; gy <= cgy + radc; gy++) {
        if (gy < 0 || gy >= GH) continue;
        for (int gx = cgx - radc; gx <= cgx + radc; gx++) {
            if (gx < 0 || gx >= GW) continue;
            float dx = gx - cgx, dy = gy - cgy;
            float dd = sqrtf(dx * dx + dy * dy);
            if (dd > radc) continue;
            Cell *c = &W[IDX(gx, gy)];
            if (dd < radc * 0.32f) {                 // vaporised core
                if (MAT_FUEL[c->mat] > 0) { c->mat = MAT_ASH; c->fire = 0; c->fuel = 0; }
            } else if (MAT_FUEL[c->mat] > 0 && c->fire == 0) {
                float t = 1.0f - dd / radc;
                if (frand() < 0.30f + 0.55f * t) ignite(gx, gy);
            }
        }
    }
}

// ── fire simulation tick ───────────────────────────────────────────────────
static int adjacent_water(int x, int y) {
    if (x > 0      && W[IDX(x-1,y)].mat == MAT_WATER) return 1;
    if (x < GW-1   && W[IDX(x+1,y)].mat == MAT_WATER) return 1;
    if (y > 0      && W[IDX(x,y-1)].mat == MAT_WATER) return 1;
    if (y < GH-1   && W[IDX(x,y+1)].mat == MAT_WATER) return 1;
    return 0;
}

static void fire_tick(void) {
    for (int i = 0; i < NCELL; i++) prevfire[i] = W[i].fire;
    dets_this_tick = 0;

    // 8 neighbours; diagonals roll at half chance for a rounder front
    static const int DX[8] = { -1, 1, 0, 0, -1, 1, -1, 1 };
    static const int DY[8] = { 0, 0, -1, 1, -1, -1, 1, 1 };

    for (int y = 0; y < GH; y++)
    for (int x = 0; x < GW; x++) {
        int srcfire = prevfire[IDX(x, y)];
        if (srcfire == 0) continue;
        Cell *c = &W[IDX(x, y)];

        // burn fuel; flare up while fuelled, die down when spent
        if (c->fuel > 0) {
            int burn = 1 + c->fire / 2;
            c->fuel = (c->fuel > burn) ? c->fuel - burn : 0;
            if (c->fire < FIRE_MAX) c->fire++;
        } else {
            c->fire--;
            if (c->fire == 0) { c->mat = MAT_ASH; continue; }
        }

        // oil that's burning hot can pop a secondary blast (consumes itself)
        if (c->mat == MAT_OIL && c->fire >= 5 && dets_this_tick < 6 && frand() < 0.05f) {
            dets_this_tick++;
            detonate(x * CELL + CELL * 0.5f, y * CELL + CELL * 0.5f, 1);
            continue;
        }

        if (srcfire < MIN_SPREAD) continue;

        for (int k = 0; k < 8; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || ny < 0 || nx >= GW || ny >= GH) continue;
            Cell *nb = &W[IDX(nx, ny)];
            if (MAT_FUEL[nb->mat] == 0 || nb->fire > 0) continue;

            int chance = SPREAD_BASE + srcfire * SPREAD_PER - MAT_RESIST[nb->mat];
            if (k >= 4) chance /= 2;                          // diagonal
            // wind bias: downwind easier, upwind harder
            float dot = DX[k] * windx + DY[k] * windy;
            chance += (int)(dot * 18.0f);
            if (adjacent_water(nx, ny)) chance /= 4;
            if (chance < 2) chance = 2;

            if ((int)(frand() * 100) < chance) ignite(nx, ny);
        }
    }
}

// ── per-frame update ───────────────────────────────────────────────────────
void update(void) {
    // wind drifts toward a new random target every few seconds (gusts)
    if (--wind_t <= 0) {
        float a = frand() * 6.2832f;
        float str = 0.3f + frand() * 0.5f;
        wtx = cosf(a) * str; wty = sinf(a) * str;
        wind_t = 150 + rnd(180);
    }
    windx += (wtx - windx) * 0.01f;
    windy += (wty - windy) * 0.01f;
    if (flash > 0) flash--;

    // input ------------------------------------------------------------------
    if (keyp(KEY_TAB)) mode ^= 1;
    if (keyp('R')) { extern void reset_world(void); reset_world(); }
    if (keyp('C')) {
        for (int i = 0; i < NCELL; i++) W[i].fire = 0;
        for (int i = 0; i < MAXP; i++) ps[i].kind = PK_FREE;
        for (int i = 0; i < 12; i++) rings[i].on = 0;
    }
    for (int d = '1'; d <= '6'; d++) if (keyp(d)) { brush = d - '1'; mode = 1; }
    if (keyp('7') || keyp('T')) { brush = MAT_KINDS; mode = 1; }   // torch sentinel

    float w = mouse_wheel();
    if (w != 0) { charge += (w > 0 ? 1 : -1); if (charge < 1) charge = 1; if (charge > 5) charge = 5; }

    int mx = mouse_x(), my = mouse_y();
    if (mode == 0) {                                  // BOOM
        if (mouse_pressed(0) || keyp(KEY_SPACE)) detonate(mx, my, charge);
    } else {                                          // BUILD
        if (mouse_down(0)) {
            int gx = mx / CELL, gy = my / CELL;
            for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++) {
                int x = gx + ox, y = gy + oy;
                if (x < 0 || y < 0 || x >= GW || y >= GH) continue;
                if (brush == MAT_KINDS) { ignite(x, y); }
                else { Cell *c = &W[IDX(x, y)]; c->mat = (unsigned char)brush;
                       c->fire = 0; c->fuel = (unsigned char)MAT_FUEL[brush]; }
            }
        }
    }

    // fire CA tick
    if (--spread_t <= 0) { spread_t = SPREAD_EVERY; fire_tick(); }

    // burning cells breathe flame / embers / smoke into the air
    for (int y = 0; y < GH; y++)
    for (int x = 0; x < GW; x++) {
        int f = W[IDX(x, y)].fire;
        if (f == 0) continue;
        float px = x * CELL + CELL * 0.5f, py = y * CELL + CELL * 0.5f;
        if ((int)(frand() * 100) < f * 4)            // licking flame
            psp(px + frand() * 3 - 1.5f, py, frand() * 0.4f - 0.2f + windx * 0.5f,
                -0.6f - frand() * 0.8f, PK_FLAME, 10 + frand() * 12,
                1.5f + frand() * 2, 0.55f + f / 14.0f, 0);
        if ((int)(frand() * 100) < f * 2)            // smoke
            psp(px, py, windx * 0.6f + frand() * 0.3f - 0.15f, -0.4f - frand() * 0.4f,
                PK_SMOKE, 80 + frand() * 90, 2 + frand() * 3, 0, 0);
        if ((int)(frand() * 100) < f)                // ember
            psp(px, py, frand() * 1.0f - 0.5f + windx, -1.0f - frand() * 1.2f,
                PK_EMBER, 20 + frand() * 30, 1, 1, CLR_ORANGE);
    }

    // advance particles
    for (int i = 0; i < MAXP; i++) {
        P *p = &ps[i];
        if (p->kind == PK_FREE) continue;
        p->x += p->vx; p->y += p->vy; p->life--;
        switch (p->kind) {
            case PK_FLAME:
                p->vy -= 0.05f; p->vx += windx * 0.02f;
                p->vx *= 0.94f; p->vy *= 0.94f;
                p->heat -= 0.04f; p->size += 0.06f;
                if (p->heat <= 0 || p->life <= 0) {
                    // a dying flame becomes a wisp of smoke
                    if (frand() < 0.4f) { p->kind = PK_SMOKE; p->life = p->maxlife = 50 + frand() * 50;
                        p->size = 2 + frand() * 2; p->vy = -0.4f; }
                    else p->kind = PK_FREE;
                }
                break;
            case PK_EMBER:
                p->vy += 0.04f; p->vx += windx * 0.03f; p->vx *= 0.97f;
                if (p->life <= 0) p->kind = PK_FREE;
                break;
            case PK_SPARK:
                p->vy += 0.18f; p->vx *= 0.97f;
                if (p->life <= 0) p->kind = PK_FREE;
                break;
            case PK_DEBRIS:
                p->vx *= 0.90f; p->vy *= 0.90f;          // top-down: friction settles it
                if (p->life <= 0) p->kind = PK_FREE;
                break;
            case PK_SMOKE:
                p->vy -= 0.012f; p->vx += windx * 0.05f;
                p->vx *= 0.98f; p->vy *= 0.99f;
                p->size += 0.05f;
                if (p->life <= 0) p->kind = PK_FREE;
                break;
        }
    }

    // rings
    for (int i = 0; i < 12; i++) if (rings[i].on) {
        rings[i].r += 4.5f;
        if (rings[i].r > rings[i].maxr) rings[i].on = 0;
    }
}

// ── default scene: a scrap of city to set alight ───────────────────────────
void reset_world(void) {
    for (int i = 0; i < NCELL; i++) { W[i].mat = MAT_GROUND; W[i].fire = 0; W[i].fuel = 0; }
    // grass everywhere as a base, then carve roads through it
    for (int i = 0; i < NCELL; i++) { W[i].mat = MAT_GRASS; W[i].fuel = (unsigned char)MAT_FUEL[MAT_GRASS]; }
    for (int x = 0; x < GW; x++) for (int dy = 0; dy < 2; dy++) {
        W[IDX(x, GH/2 + dy)].mat = MAT_ROAD; W[IDX(x, GH/2 + dy)].fuel = 0;
    }
    for (int y = 0; y < GH; y++) for (int dx = 0; dx < 2; dx++) {
        W[IDX(GW/3 + dx, y)].mat = MAT_ROAD; W[IDX(GW/3 + dx, y)].fuel = 0;
    }
    // a few wood buildings
    int bx[] = { 6, 46, 60, 22 }, by[] = { 6, 8, 30, 34 }, bw[] = { 10, 14, 12, 9 }, bh[] = { 8, 10, 9, 7 };
    for (int b = 0; b < 4; b++)
        for (int y = by[b]; y < by[b] + bh[b] && y < GH; y++)
        for (int x = bx[b]; x < bx[b] + bw[b] && x < GW; x++) {
            W[IDX(x, y)].mat = MAT_WOOD; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_WOOD];
        }
    // an oil slick and a pond
    for (int y = 22; y < 28; y++) for (int x = 50; x < 62; x++) {
        W[IDX(x, y)].mat = MAT_OIL; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_OIL];
    }
    for (int y = 38; y < 46; y++) for (int x = 6; x < 16; x++) {
        W[IDX(x, y)].mat = MAT_WATER; W[IDX(x, y)].fuel = 0;
    }
    for (int i = 0; i < MAXP; i++) ps[i].kind = PK_FREE;
    for (int i = 0; i < 12; i++) rings[i].on = 0;
}

static int inited = 0;

// ── render ───────────────────────────────────────────────────────────────
void draw(void) {
    if (!inited) { reset_world(); inited = 1; wind_t = 1; }

    cls(MAT_COL[MAT_GROUND]);

    // world cells + burning glow
    for (int y = 0; y < GH; y++)
    for (int x = 0; x < GW; x++) {
        Cell *c = &W[IDX(x, y)];
        if (c->mat != MAT_GROUND)
            rectfill(x * CELL, y * CELL, CELL, CELL, MAT_COL[c->mat]);
        if (c->fire > 0) {
            int idx = c->fire - 1 - (int)(frand() * 2);     // flicker
            if (idx < 0) idx = 0;
            rectfill(x * CELL, y * CELL, CELL, CELL, FLAME_RAMP[idx]);
        }
    }

    // smoke (behind), then debris, then flames/embers/sparks (in front)
    for (int i = 0; i < MAXP; i++) {
        P *p = &ps[i];
        if (p->kind != PK_SMOKE) continue;
        float t = p->life / p->maxlife;          // 1 young → 0 old
        int col = t > 0.6f ? CLR_DARKER_GREY : t > 0.3f ? CLR_DARK_GREY : CLR_MEDIUM_GREY;
        circfill((int)p->x, (int)p->y, (int)p->size, col);
    }
    for (int i = 0; i < MAXP; i++) {
        P *p = &ps[i];
        if (p->kind != PK_DEBRIS) continue;
        rectfill((int)p->x, (int)p->y, (int)p->size + 1, (int)p->size + 1, p->col);
    }
    for (int i = 0; i < MAXP; i++) {
        P *p = &ps[i];
        if (p->kind == PK_FLAME) {
            float h = p->heat; if (h < 0) h = 0; if (h > 1) h = 1;
            int idx = (int)(h * (NFLAME - 1) + 0.5f);
            circfill((int)p->x, (int)p->y, (int)p->size, FLAME_RAMP[idx]);
        } else if (p->kind == PK_EMBER) {
            pset((int)p->x, (int)p->y, p->life > p->maxlife * 0.4f ? CLR_LIGHT_YELLOW : CLR_DARK_ORANGE);
        } else if (p->kind == PK_SPARK) {
            pset((int)p->x, (int)p->y, p->col);
        }
    }

    // shockwave rings + bright young core
    for (int i = 0; i < 12; i++) if (rings[i].on) {
        float t = rings[i].r / rings[i].maxr;
        int col = t < 0.4f ? CLR_WHITE : t < 0.7f ? CLR_LIGHT_YELLOW : CLR_DARK_ORANGE;
        circ((int)rings[i].x, (int)rings[i].y, (int)rings[i].r, col);
        if (rings[i].r < 6) circfill((int)rings[i].x, (int)rings[i].y, (int)(6 - rings[i].r), CLR_WHITE);
    }

    // flash: brighten the screen border briefly
    if (flash > 0) {
        int col = flash > 3 ? CLR_WHITE : CLR_LIGHT_YELLOW;
        rect(0, 0, SCREEN_W, SCREEN_H, col);
        rect(1, 1, SCREEN_W - 2, SCREEN_H - 2, col);
    }

    // cursor: blast radius (BOOM) or brush footprint (BUILD)
    int mx = mouse_x(), my = mouse_y();
    if (mode == 0) {
        int R = (4 + charge * 4) * CELL;
        circ(mx, my, R, CLR_RED);
        circ(mx, my, 2, CLR_WHITE);
    } else {
        int gx = mx / CELL, gy = my / CELL;
        int bc = brush == MAT_KINDS ? CLR_ORANGE : MAT_COL[brush];
        rect((gx - 1) * CELL, (gy - 1) * CELL, CELL * 3, CELL * 3, bc);
    }

    // HUD ---------------------------------------------------------------------
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    char buf[64];
    if (mode == 0) {
        snprintf(buf, sizeof buf, "BOOM  charge %d  (wheel)", charge);
    } else {
        const char *bn = brush == MAT_KINDS ? "torch" : MAT_NAME[brush];
        snprintf(buf, sizeof buf, "BUILD  brush: %s  (1-6,T)", bn);
    }
    print(buf, 3, 1, CLR_WHITE);
    print("TAB mode  SPACE boom  C clear  R reset", 3, SCREEN_H - 8, CLR_LIGHT_GREY);

    // wind tell-tale (top-right arrow)
    int wx = SCREEN_W - 14, wy = 5;
    line(wx, wy, wx + (int)(windx * 9), wy + (int)(windy * 9), CLR_BLUE);
    pset(wx + (int)(windx * 9), wy + (int)(windy * 9), CLR_WHITE);
}
