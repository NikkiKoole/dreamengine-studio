#include "studio.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// BOOM  —  a destruction sandbox for a criminal city. A petri dish of blast
//          effects over an honest fire + material world. Click to detonate;
//          watch the blast ignite, shatter, and collapse the block, and the
//          fire crawl, chain, and smoke on the wind.
//
//   MODES (TAB toggles)
//     BOOM    pick a blast with 1-5; click anywhere (or SPACE) → detonate.
//             mouse WHEEL = charge size.
//                1 BLAST    the all-rounder: shockwave, fireball, crater
//                2 GRENADE  small + sharp, all shrapnel, tiny fireball
//                3 MOLOTOV  no shockwave — a flame splash that paints fire
//                4 CAR BOMB medium, debris-heavy, throws chunks far
//                5 GAS MAIN a hiss, then a column of fire ERUPTS upward
//     BUILD   click / drag to paint the world. 1-0 pick a material;
//             T = torch: light a fire by hand.
//
//   ALWAYS    C clear all fire & smoke      R reset the scene
//
// ── what's honest here (two layers, each doing one real job) ─────────────────
//
//   1. THE WORLD is a grid of MATERIAL cells, each with FUEL, a FIRE level
//      0-7, and structural HP — navkit's fire model flattened to 2D, plus a
//      pressure layer. Fire spreads to neighbours by probability (base +
//      level − the target's ignition resistance), biased downwind, choked by
//      water. Fuel burns down; spent cells turn to ASH. Grass flashes over,
//      wood smoulders for ages, oil goes up eager and hot. BARRELS and CARS
//      cook off into their own blasts (a yard of barrels chains; a torched car
//      car-bombs itself). GLASS and CONCRETE don't burn — they answer to
//      PRESSURE: a blast wave shatters glass to nothing and chips concrete HP
//      until a wall COLLAPSES to rubble. Two reactions, two causes: heat and
//      pressure, never crossed.
//
//   2. EVERYTHING AIRBORNE is a PARTICLE — fireball, sparks, ballistic debris,
//      glass shards, and the smoke column. Flames are born white-hot and COOL
//      along a blackbody ramp (white→yellow→orange→red→dark) as they age,
//      rising on their own buoyancy. A detonation reads the way a real one
//      does: a bright core, an expanding shockwave RING that shoves nearby
//      particles outward, the fireball, then a slow smoke plume.
//
// A detonation seeds layer 1 (ignites fuel, craters the core, shatters glass,
// collapses concrete) and layer 1's burning cells feed layer 2 (every hot cell
// breathes flame, embers, and smoke). The five blast archetypes are five
// recipes over the SAME two systems — the petri dish — and the wind decides
// what's left standing. The sandbox seed for the roam-and-burn city game.
// ============================================================================

#define CELL  4
#define GW    (SCREEN_W / CELL)
#define GH    (SCREEN_H / CELL)
#define NCELL (GW * GH)
#define IDX(x,y) ((y) * GW + (x))

// ── materials ────────────────────────────────────────────────────────────────
// fuel  = how long a cell burns once lit.   resist = % shaved off a neighbour's
// spread roll (high = hard to catch; oil/barrel negative = eager).   hp =
// structural integrity for non-flammable props (glass/concrete).   col = tint.
enum { MAT_GROUND, MAT_ROAD, MAT_GRASS, MAT_WOOD, MAT_OIL, MAT_WATER, MAT_ASH,
       MAT_BARREL, MAT_CAR, MAT_GLASS, MAT_CONCRETE, MAT_RUBBLE, MAT_KINDS };
#define TORCH MAT_KINDS                  // brush sentinel: light a fire by hand
static const int MAT_FUEL[MAT_KINDS]   = {  0,  0, 20, 90, 45,  0,  0,  30, 60,  0,  0,  0 };
static const int MAT_RESIST[MAT_KINDS] = {  0,  0,  0, 20,-40,  0,  0, -20, 40,  0,  0,  0 };
static const int MAT_HP[MAT_KINDS]     = {  0,  0,  0,  0,  0,  0,  0,   0,  0,  4, 16,  0 };
static const int MAT_COL[MAT_KINDS]    = {
    CLR_BROWNISH_BLACK, CLR_DARK_GREY, CLR_DARK_GREEN, CLR_BROWN,
    CLR_DARKER_GREY,    CLR_DARK_BLUE, CLR_BROWNISH_BLACK,
    CLR_RED,            CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_LIGHT_GREY, CLR_MEDIUM_GREY
};
static const char *MAT_NAME[MAT_KINDS] = {
    "ground","road","grass","wood","oil","water","ash",
    "barrel","car","glass","concrete","rubble"
};

// brushes you can paint (results like ash/rubble are not paintable)
static const int BRUSH_MAT[] = { MAT_GROUND, MAT_ROAD, MAT_GRASS, MAT_WOOD, MAT_OIL,
                                 MAT_WATER, MAT_BARREL, MAT_CAR, MAT_GLASS, MAT_CONCRETE };
#define NBRUSH 10

// ── the world grid ────────────────────────────────────────────────────────────
typedef struct { unsigned char mat, fuel, fire, hp; } Cell;
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

// ── blast archetypes (the petri dish) ─────────────────────────────────────────
// each is one recipe over the two systems below — scales on the base blast.
enum { BL_BLAST, BL_GRENADE, BL_MOLOTOV, BL_CARBOMB, BL_GASMAIN, BL_KINDS };
typedef struct {
    const char *name;
    float rad;       // radius scale (× the charge radius)
    float shock;     // shockwave push strength (0 = no ring, no push)
    float fireball;  // fireball-count scale
    float heat;      // fireball birth heat
    float sparks;    // spark/shrapnel-count scale
    float debris;    // ballistic-debris-count scale
    float smoke;     // smoke-count scale
    float shake;     // screen-shake scale
    float ignite;    // ignition-probability scale inside the radius
    float pressure;  // structural damage strength (glass/concrete)
    int   crater;    // 1 = vaporise the core to ash
    int   delay;     // frames to wait before the blast fires (gas main hiss)
    int   up;        // 1 = fireball erupts upward as a column
} BlastSpec;
static const BlastSpec BSPEC[BL_KINDS] = {
//    name        rad   shock fball heat  spk   deb   smk   shk   ign   prs  crat dly up
    { "blast",    1.00f,1.0f, 1.0f, 0.85f,1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  1,   0, 0 },
    { "grenade",  0.45f,0.6f, 0.3f, 0.90f,2.6f, 2.0f, 0.3f, 0.6f, 0.7f, 0.8f,  1,   0, 0 },
    { "molotov",  0.60f,0.0f, 0.5f, 0.80f,0.2f, 0.0f, 0.5f, 0.3f, 1.6f, 0.0f,  0,   0, 0 },
    { "car bomb", 0.85f,1.1f, 0.8f, 0.85f,1.2f, 3.0f, 1.2f, 1.2f, 1.0f, 1.6f,  1,   0, 0 },
    { "gas main", 1.30f,1.4f, 1.6f, 1.00f,1.0f, 0.5f, 2.2f, 1.6f, 1.2f, 1.4f,  1,  28, 1 },
};

// ── particles (everything airborne) ───────────────────────────────────────────
enum { PK_FREE, PK_SMOKE, PK_FLAME, PK_EMBER, PK_SPARK, PK_DEBRIS };
typedef struct {
    float x, y, vx, vy;
    float life, maxlife;
    float heat;          // 0..1, flames cool toward 0
    float size;
    unsigned char kind, col;
} P;
#define MAXP 2200
static P ps[MAXP];
static int pcur = 0;

// shockwave rings (visual + a one-shot push happens at detonation time)
typedef struct { float x, y, r, maxr; int on; } Ring;
static Ring rings[12];

// pending delayed blasts (the gas-main hiss before the column)
typedef struct { float x, y; int chg, type, t, on; } Pending;
static Pending pend[8];

// ── physics blocks: chunks of a destroyed wall, knocked loose as real bodies ──
// This is the prototype of sloop's deferred "demolition rung" (sloop.c:527 — its
// obstacles already carry per-cell mass/strength + a knocked vx,vy,vr, but a wall
// is still ONE rigid body; here we detach per-cell). A destroyed wall cell flies
// off as a Block carrying that material's colour, tumbles, slides to rest under
// top-down friction (the same law PK_DEBRIS settles by), then DEPOSITS rubble
// where it lands — terrain → physical object → terrain again.
typedef struct {
    float x, y, vx, vy, ang, spin;
    int   settle;                 // frames at rest before it deposits its rubble
    unsigned char on, col, mat;
} Block;
#define MAXBLK 500
static Block blk[MAXBLK];
static int blkcur = 0;

// ── global state ───────────────────────────────────────────────────────────
static int   mode;            // 0 = BOOM, 1 = BUILD
static int   blast = BL_BLAST;// selected detonation archetype
static int   brush = MAT_WOOD;
static int   charge = 1;      // 1..5 (starts small; wheel up)
static float windx, windy;    // current wind vector
static float wtx, wty;        // wind target (lerped toward)
static int   wind_t;
static int   spread_t;
static int   flash;           // brief screen-edge flash frames
static int   dets_this_tick;  // cap secondary (chain) detonations
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

// knock a wall chunk loose: launch it radially away from the blast centre (ox,oy),
// carrying its material's colour, with a random tumble. Drops if the pool is full.
static void eject_block(float x, float y, float ox, float oy, int col, int mat) {
    for (int n = 0; n < MAXBLK; n++) {
        int i = (blkcur + n) % MAXBLK;
        if (!blk[i].on) {
            float dx = x - ox, dy = y - oy;
            float d  = sqrtf(dx * dx + dy * dy) + 0.01f;
            float spd = 1.2f + frand() * 2.6f;
            blk[i] = (Block){
                x, y,
                dx / d * spd + (frand() - 0.5f) * 1.2f,
                dy / d * spd + (frand() - 0.5f) * 1.2f,
                frand() * 6.2832f, (frand() - 0.5f) * 0.6f,
                0, 1, (unsigned char)col, (unsigned char)mat
            };
            blkcur = (i + 1) % MAXBLK;
            return;
        }
    }
}

// blast radius in cells before the per-archetype scale — charge 1 is small and
// punchy, climbing to a city-block crater at charge 5. (debris COUNT below keeps
// its own chg curve; this is only the footprint.)
#define BASE_RADC(c) (2 + (c) * 2)

// ── the detonation (immediate) ─────────────────────────────────────────────
static void detonate_now(float cx, float cy, int chg, int type) {
    const BlastSpec *b = &BSPEC[type];
    if (chg < 1) chg = 1;
    float R    = BASE_RADC(chg) * CELL * b->rad;       // effect radius in pixels
    int   radc = (int)(BASE_RADC(chg) * b->rad);        // and in cells
    if (radc < 1) radc = 1;

    shake((2.0f + chg * 2.2f) * b->shake);
    flash = (int)((3 + chg) * (b->shock > 0 ? 1.0f : 0.4f)) + 1;

    // sound per archetype
    switch (type) {
        case BL_GRENADE:
            hit(46 + rnd(4), INSTR_NOISE, 55 + chg * 4, 70);    // sharp crack
            hit(30, INSTR_NOISE, 30, 110);
            break;
        case BL_MOLOTOV:
            hit(40, INSTR_NOISE, 30, 520);                      // whoosh
            hit(72 + rnd(6), INSTR_NOISE, 22, 60);              // glass tink
            break;
        case BL_CARBOMB:
            hit(24 + rnd(3), INSTR_NOISE, 78 + chg * 6, 300);   // mid thud
            hit(18, INSTR_NOISE, 50, 420);
            break;
        case BL_GASMAIN:
            hit(16, INSTR_NOISE, 92, 560);                      // big low rumble
            hit(20, INSTR_NOISE, 70 + chg * 6, 360);
            break;
        default:
            hit(28 + rnd(4), INSTR_NOISE, 70 + chg * 6, 220 + chg * 40);
            hit(20, INSTR_NOISE, 40 + chg * 5, 320);
    }

    // shockwave ring (only blasts with pressure throw one)
    if (b->shock > 0) {
        for (int i = 0; i < 12; i++) if (!rings[i].on) {
            rings[i] = (Ring){ cx, cy, 2, R * 1.5f, 1 };
            break;
        }
    }

    // fireball — flames born hot, flung outward, buoyant (or up, for gas main)
    int nf = (int)((16 + chg * 22) * b->fireball);
    for (int i = 0; i < nf; i++) {
        float a = frand() * 6.2832f;
        float s = rnd_float_between(0.4f, 2.2f + chg * 0.5f);
        float vx = cosf(a) * s, vy = sinf(a) * s;
        float life = 16 + frand() * (20 + chg * 4);
        if (b->up) { vy = -(1.5f + frand() * 3.0f); vx *= 0.5f; life *= 1.5f; }
        psp(cx, cy, vx, vy, PK_FLAME, life, 2 + frand() * 4,
            b->heat + frand() * 0.3f, 0);
    }
    // sparks / shrapnel — fast, gravity, bright, brief
    int ns = (int)((10 + chg * 8) * b->sparks);
    for (int i = 0; i < ns; i++) {
        float a = frand() * 6.2832f;
        float s = rnd_float_between(1.5f, 4.0f + chg);
        psp(cx, cy, cosf(a) * s, sinf(a) * s, PK_SPARK,
            8 + frand() * 12, 1, 1, CLR_LIGHT_YELLOW);
    }
    // debris — ballistic chunks, heavy drag
    int nd = (int)((4 + chg * 4) * b->debris);
    for (int i = 0; i < nd; i++) {
        float a = frand() * 6.2832f;
        float s = rnd_float_between(1.5f, 3.5f + chg);
        psp(cx, cy, cosf(a) * s, sinf(a) * s, PK_DEBRIS,
            22 + frand() * 24, 1 + frand() * 2, 0,
            frand() < 0.5f ? CLR_DARK_GREY : CLR_BROWN);
    }
    // a first breath of smoke
    int nm = (int)((6 + chg * 4) * b->smoke);
    for (int i = 0; i < nm; i++) {
        float a = frand() * 6.2832f;
        float s = rnd_float_between(0.2f, 1.0f);
        psp(cx, cy, cosf(a) * s, sinf(a) * s - 0.4f, PK_SMOKE,
            60 + frand() * 80, 3 + frand() * 4, 0, 0);
    }

    // shove existing particles outward (the blast wave)
    if (b->shock > 0) {
        float R2 = R * 1.6f;
        for (int i = 0; i < MAXP; i++) {
            if (ps[i].kind == PK_FREE) continue;
            float dx = ps[i].x - cx, dy = ps[i].y - cy;
            float d = sqrtf(dx * dx + dy * dy) + 0.01f;
            if (d < R2) {
                float push = (R2 - d) / R2 * (3.0f + chg) * b->shock;
                ps[i].vx += dx / d * push;
                ps[i].vy += dy / d * push;
            }
        }
    }

    int cgx = (int)(cx / CELL), cgy = (int)(cy / CELL);

    // ── pressure pass: glass shatters, concrete chips and collapses ──────────
    if (b->pressure > 0) {
        float press = b->pressure * (0.6f + chg * 0.5f);
        int   pr    = radc + 1;
        for (int gy = cgy - pr; gy <= cgy + pr; gy++) {
            if (gy < 0 || gy >= GH) continue;
            for (int gx = cgx - pr; gx <= cgx + pr; gx++) {
                if (gx < 0 || gx >= GW) continue;
                float dx = gx - cgx, dy = gy - cgy;
                float dd = sqrtf(dx * dx + dy * dy);
                if (dd > pr) continue;
                float t = 1.0f - dd / pr;
                Cell *c = &W[IDX(gx, gy)];
                float px = gx * CELL + 2, py = gy * CELL + 2;
                if (c->mat == MAT_GLASS) {
                    if (press * t > 0.35f) {                 // the window blows in
                        c->mat = MAT_GROUND; c->hp = 0;
                        for (int s = 0; s < 3; s++)
                            psp(px, py, frand() * 3 - 1.5f, frand() * 3 - 1.5f, PK_SPARK,
                                10 + frand() * 10, 1, 1,
                                frand() < 0.5f ? CLR_BLUE_GREEN : CLR_WHITE);
                        if (frand() < 0.4f)                  // a pane chunk cartwheels out
                            eject_block(px, py, cx, cy, CLR_BLUE_GREEN, MAT_GLASS);
                    }
                } else if (c->mat == MAT_CONCRETE) {
                    int dmg = (int)(press * t * 11.0f);
                    if (dmg <= 0) continue;
                    if (c->hp > dmg) {
                        c->hp -= (unsigned char)dmg;
                    } else {                                  // the wall comes down
                        c->hp = 0; c->fuel = 0; c->fire = 0;
                        if (frand() < 0.7f) {                 // most of the wall flies off as blocks
                            c->mat = MAT_GROUND;              // a hole punched clean through
                            eject_block(px, py, cx, cy, MAT_COL[MAT_CONCRETE], MAT_CONCRETE);
                        } else {
                            c->mat = MAT_RUBBLE;              // the rest crumbles where it stood
                        }
                        psp(px, py, frand() * 0.6f - 0.3f, -0.3f, PK_SMOKE,
                            50 + frand() * 40, 2 + frand() * 2, 0, 0);
                    }
                } else if (c->mat == MAT_WOOD) {
                    // a strong wave splinters a wood wall into flying planks (the rest
                    // is left to CATCH instead, in the heat pass below)
                    if (press * t > 0.6f && frand() < 0.5f) {
                        c->mat = MAT_GROUND; c->fuel = 0; c->fire = 0;
                        eject_block(px, py, cx, cy, MAT_COL[MAT_WOOD], MAT_WOOD);
                    }
                }
            }
        }
    }

    // ── heat pass: crater the core to ash, ignite fuel in the radius ─────────
    for (int gy = cgy - radc; gy <= cgy + radc; gy++) {
        if (gy < 0 || gy >= GH) continue;
        for (int gx = cgx - radc; gx <= cgx + radc; gx++) {
            if (gx < 0 || gx >= GW) continue;
            float dx = gx - cgx, dy = gy - cgy;
            float dd = sqrtf(dx * dx + dy * dy);
            if (dd > radc) continue;
            Cell *c = &W[IDX(gx, gy)];
            if (b->crater && dd < radc * 0.32f) {            // vaporised core
                if (MAT_FUEL[c->mat] > 0) { c->mat = MAT_ASH; c->fire = 0; c->fuel = 0; }
            } else if (MAT_FUEL[c->mat] > 0 && c->fire == 0) {
                float t = 1.0f - dd / radc;
                if (frand() < (0.30f + 0.55f * t) * b->ignite) ignite(gx, gy);
            }
        }
    }

    // molotov: a wet flame splash that lingers over the pour (no crater/shock)
    if (type == BL_MOLOTOV) {
        int splash = 14 + chg * 6;
        for (int i = 0; i < splash; i++) {
            float a = frand() * 6.2832f;
            float s = rnd_float_between(0.0f, R * 0.05f);
            psp(cx + cosf(a) * s * 4, cy + sinf(a) * s * 4,
                frand() * 0.4f - 0.2f, -0.2f - frand() * 0.4f,
                PK_FLAME, 20 + frand() * 30, 1.5f + frand() * 2,
                0.55f + frand() * 0.25f, 0);
        }
    }
}

// ── detonation entry point: delayed blasts (gas main) queue + hiss first ─────
static void detonate(float cx, float cy, int chg, int type) {
    if (BSPEC[type].delay > 0) {
        for (int i = 0; i < 8; i++) if (!pend[i].on) {
            pend[i] = (Pending){ cx, cy, chg, type, BSPEC[type].delay, 1 };
            break;
        }
        hit(44, INSTR_NOISE, 20, 360);    // the gas hiss before it goes
        shake(0.5f);
        return;
    }
    detonate_now(cx, cy, chg, type);
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

        // things that cook off into their own blast (consume themselves) ------
        if (c->mat == MAT_OIL && c->fire >= 5 && dets_this_tick < 6 && frand() < 0.05f) {
            dets_this_tick++;
            detonate(x * CELL + CELL * 0.5f, y * CELL + CELL * 0.5f, 1, BL_BLAST);
            continue;
        }
        if (c->mat == MAT_BARREL && c->fire >= 3 && dets_this_tick < 6 && frand() < 0.22f) {
            dets_this_tick++;
            W[IDX(x, y)].mat = MAT_RUBBLE; c->fire = 0; c->fuel = 0;
            detonate(x * CELL + CELL * 0.5f, y * CELL + CELL * 0.5f, 2, BL_BLAST);
            continue;
        }
        if (c->mat == MAT_CAR && c->fire >= 5 && dets_this_tick < 4 && frand() < 0.08f) {
            dets_this_tick++;
            W[IDX(x, y)].mat = MAT_RUBBLE; c->fire = 0; c->fuel = 0;
            detonate(x * CELL + CELL * 0.5f, y * CELL + CELL * 0.5f, 1, BL_CARBOMB);
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
        for (int i = 0; i < 8; i++) pend[i].on = 0;
        for (int i = 0; i < MAXBLK; i++) blk[i].on = 0;
    }
    if (mode == 0) {                                          // BOOM: pick blast
        for (int i = 0; i < BL_KINDS; i++) if (keyp('1' + i)) blast = i;
    } else {                                                  // BUILD: pick brush
        for (int i = 0; i < 9; i++) if (keyp('1' + i)) brush = BRUSH_MAT[i];
        if (keyp('0')) brush = BRUSH_MAT[9];
        if (keyp('T')) brush = TORCH;
    }

    float w = mouse_wheel();
    if (w != 0) { charge += (w > 0 ? 1 : -1); if (charge < 1) charge = 1; if (charge > 5) charge = 5; }

    int mx = mouse_x(), my = mouse_y();
    if (mode == 0) {                                  // BOOM
        if (mouse_pressed(0) || keyp(KEY_SPACE)) detonate(mx, my, charge, blast);
    } else {                                          // BUILD
        if (mouse_down(0)) {
            int gx = mx / CELL, gy = my / CELL;
            for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++) {
                int x = gx + ox, y = gy + oy;
                if (x < 0 || y < 0 || x >= GW || y >= GH) continue;
                if (brush == TORCH) { ignite(x, y); }
                else { Cell *c = &W[IDX(x, y)]; c->mat = (unsigned char)brush;
                       c->fire = 0; c->fuel = (unsigned char)MAT_FUEL[brush];
                       c->hp = (unsigned char)MAT_HP[brush]; }
            }
        }
    }

    // delayed blasts: build a gas plume, then erupt
    for (int i = 0; i < 8; i++) if (pend[i].on) {
        if ((int)(frand() * 100) < 45)
            psp(pend[i].x + frand() * 4 - 2, pend[i].y, frand() * 0.3f - 0.15f,
                -0.6f - frand() * 0.6f, PK_SMOKE, 30 + frand() * 30,
                1.5f + frand() * 1.5f, 0, 0);
        if ((int)(frand() * 100) < 22)
            psp(pend[i].x, pend[i].y, frand() * 0.4f - 0.2f, -1.0f - frand(),
                PK_SPARK, 10 + frand() * 8, 1, 1, CLR_LIGHT_YELLOW);
        if (--pend[i].t <= 0) {
            pend[i].on = 0;
            detonate_now(pend[i].x, pend[i].y, pend[i].chg, pend[i].type);
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

    // physics blocks: slide + tumble, settle by friction, deposit rubble at rest
    for (int i = 0; i < MAXBLK; i++) {
        Block *bk = &blk[i];
        if (!bk->on) continue;
        bk->x += bk->vx; bk->y += bk->vy;
        bk->vx *= 0.90f; bk->vy *= 0.90f;          // same top-down settle as PK_DEBRIS
        bk->ang += bk->spin; bk->spin *= 0.94f;
        if (bk->x < -4 || bk->y < -4 || bk->x > SCREEN_W + 4 || bk->y > SCREEN_H + 4) {
            bk->on = 0; continue;                  // slid off the world
        }
        if (bk->vx * bk->vx + bk->vy * bk->vy < 0.02f) {       // come to rest
            if (++bk->settle > 6) {
                int gx = (int)(bk->x / CELL), gy = (int)(bk->y / CELL);
                if (gx >= 0 && gy >= 0 && gx < GW && gy < GH) {
                    Cell *c = &W[IDX(gx, gy)];
                    // pile up as rubble on open ground — never bury water or a live wall
                    if (c->mat == MAT_GROUND || c->mat == MAT_ROAD || c->mat == MAT_GRASS ||
                        c->mat == MAT_ASH || c->mat == MAT_RUBBLE) {
                        c->mat = MAT_RUBBLE; c->fuel = 0; c->fire = 0;
                    }
                }
                bk->on = 0;
            }
        } else bk->settle = 0;
    }
}

// ── default scene: a block of the criminal city to set alight ───────────────
static void stamp_building(int bx, int by, int bw, int bh, int mat, int glassfront) {
    for (int y = by; y < by + bh && y < GH; y++)
    for (int x = bx; x < bx + bw && x < GW; x++) {
        if (x < 0 || y < 0) continue;
        Cell *c = &W[IDX(x, y)];
        int m = mat;
        // line the street-facing wall (top row) with glass
        if (glassfront && y == by && mat == MAT_CONCRETE) m = MAT_GLASS;
        c->mat = (unsigned char)m;
        c->fire = 0;
        c->fuel = (unsigned char)MAT_FUEL[m];
        c->hp   = (unsigned char)MAT_HP[m];
    }
}

void reset_world(void) {
    for (int i = 0; i < NCELL; i++) { W[i].mat = MAT_GROUND; W[i].fire = 0; W[i].fuel = 0; W[i].hp = 0; }
    // grass everywhere as a base, then carve roads through it
    for (int i = 0; i < NCELL; i++) { W[i].mat = MAT_GRASS; W[i].fuel = (unsigned char)MAT_FUEL[MAT_GRASS]; }
    for (int x = 0; x < GW; x++) for (int dy = 0; dy < 2; dy++) {
        W[IDX(x, GH/2 + dy)].mat = MAT_ROAD; W[IDX(x, GH/2 + dy)].fuel = 0;
    }
    for (int y = 0; y < GH; y++) for (int dx = 0; dx < 2; dx++) {
        W[IDX(GW/3 + dx, y)].mat = MAT_ROAD; W[IDX(GW/3 + dx, y)].fuel = 0;
    }

    // concrete blocks with glass shopfronts, plus a couple of wood shacks
    stamp_building( 6,  6, 12,  9, MAT_CONCRETE, 1);
    stamp_building(50,  6, 16, 11, MAT_CONCRETE, 1);
    stamp_building(55, 33, 14, 10, MAT_CONCRETE, 1);
    stamp_building(10, 34,  9,  7, MAT_WOOD,     0);
    stamp_building(34, 35,  8,  6, MAT_WOOD,     0);

    // an oil slick, a barrel cluster feeding it, and a pond to fight it
    for (int y = 22; y < 28; y++) for (int x = 50; x < 62; x++) {
        W[IDX(x, y)].mat = MAT_OIL; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_OIL];
    }
    for (int y = 22; y < 26; y++) for (int x = 44; x < 49; x++) {
        W[IDX(x, y)].mat = MAT_BARREL; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_BARREL];
    }
    for (int y = 38; y < 46; y++) for (int x = 6; x < 16; x++) {
        W[IDX(x, y)].mat = MAT_WATER; W[IDX(x, y)].fuel = 0;
    }

    // cars parked along the roads
    int cx[] = { 10, 40, 62, 8 }, cy[] = { GH/2 - 1, GH/2 + 2, GH/2 - 1, 18 };
    for (int i = 0; i < 4; i++) for (int dx = 0; dx < 3; dx++) {
        int x = cx[i] + dx, y = cy[i];
        if (x < 0 || x >= GW || y < 0 || y >= GH) continue;
        W[IDX(x, y)].mat = MAT_CAR; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_CAR];
    }
    // a couple parked on the vertical road
    for (int dy = 0; dy < 3; dy++) {
        int x = GW/3 - 1, y = 8 + dy;
        if (x >= 0 && y < GH) { W[IDX(x, y)].mat = MAT_CAR; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_CAR]; }
    }

    for (int i = 0; i < MAXP; i++) ps[i].kind = PK_FREE;
    for (int i = 0; i < 12; i++) rings[i].on = 0;
    for (int i = 0; i < 8; i++) pend[i].on = 0;
    for (int i = 0; i < MAXBLK; i++) blk[i].on = 0;
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
        // a little prop detail so cars/barrels/glass read at 4px
        if (c->mat == MAT_CAR) {
            pset(x * CELL + 1, y * CELL + 1, CLR_BLUE_GREEN);   // windshield glint
        } else if (c->mat == MAT_BARREL) {
            pset(x * CELL + 1, y * CELL + 1, CLR_DARK_RED);
            pset(x * CELL + 2, y * CELL + 2, CLR_ORANGE);       // hazard cap
        } else if (c->mat == MAT_GLASS) {
            pset(x * CELL, y * CELL, CLR_WHITE);                // sheen
        }
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
    // physics blocks — knocked wall chunks, a facet pixel orbiting to read the tumble
    for (int i = 0; i < MAXBLK; i++) {
        Block *bk = &blk[i];
        if (!bk->on) continue;
        int bx = (int)bk->x, by = (int)bk->y;
        rectfill(bx - 1, by - 1, 3, 3, bk->col);
        pset(bx + (int)(cosf(bk->ang) * 1.5f), by + (int)(sinf(bk->ang) * 1.5f), CLR_WHITE);
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

    // gas-main fuse: a pulsing marker over a primed eruption
    for (int i = 0; i < 8; i++) if (pend[i].on) {
        int r = 2 + (int)((pend[i].t / 6) % 3);
        circ((int)pend[i].x, (int)pend[i].y, r, CLR_YELLOW);
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
        int R = (int)(BASE_RADC(charge) * CELL * BSPEC[blast].rad);
        int col = blast == BL_MOLOTOV ? CLR_ORANGE : blast == BL_GASMAIN ? CLR_YELLOW : CLR_RED;
        circ(mx, my, R, col);
        circ(mx, my, 2, CLR_WHITE);
    } else {
        int gx = mx / CELL, gy = my / CELL;
        int bc = brush == TORCH ? CLR_ORANGE : MAT_COL[brush];
        rect((gx - 1) * CELL, (gy - 1) * CELL, CELL * 3, CELL * 3, bc);
    }

    // HUD ---------------------------------------------------------------------
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    char buf[64];
    if (mode == 0) {
        snprintf(buf, sizeof buf, "BOOM  %s  charge %d", BSPEC[blast].name, charge);
    } else {
        const char *bn = brush == TORCH ? "torch" : MAT_NAME[brush];
        snprintf(buf, sizeof buf, "BUILD  brush: %s", bn);
    }
    print(buf, 3, 1, CLR_WHITE);

    // a small selectable strip under the bar (the petri-dish menu)
    font(FONT_SMALL);
    int sx = 3, sy = 11;
    if (mode == 0) {
        for (int i = 0; i < BL_KINDS; i++) {
            char t[24]; snprintf(t, sizeof t, "%d %s", i + 1, BSPEC[i].name);
            print(t, sx, sy, i == blast ? CLR_WHITE : CLR_MEDIUM_GREY);
            sx += (int)strlen(t) * 4 + 5;
        }
    } else {
        for (int i = 0; i < NBRUSH; i++) {
            int m = BRUSH_MAT[i];
            char t[24]; snprintf(t, sizeof t, "%d%s", (i + 1) % 10, MAT_NAME[m]);
            print(t, sx, sy, brush == m ? CLR_WHITE : CLR_MEDIUM_GREY);
            sx += (int)strlen(t) * 4 + 4;
        }
        print("T torch", sx, sy, brush == TORCH ? CLR_WHITE : CLR_MEDIUM_GREY);
    }
    font(FONT_NORMAL);

    print("TAB mode  wheel size  C clear  R reset", 3, SCREEN_H - 8, CLR_LIGHT_GREY);

    // wind tell-tale (top-right arrow)
    int wx = SCREEN_W - 14, wy = 5;
    line(wx, wy, wx + (int)(windx * 9), wy + (int)(windy * 9), CLR_BLUE);
    pset(wx + (int)(windx * 9), wy + (int)(windy * 9), CLR_WHITE);
}
