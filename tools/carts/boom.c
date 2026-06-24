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
//   TOOLS are picked by CLICKING cute pixel-art buttons — TWO rows always on
//   screen: BLASTS on top, BUILD BRUSHES below (icons live in boom.cart.js).
//   Clicking any tool also sets its mode, so no keyboard shortcuts are needed
//   (TAB still flips mode as a shortcut). A label by the cursor names exactly
//   what cell you're hovering.
//
//   THE TWO TOOL ROWS
//     BLASTS  click a blast, then click anywhere (or SPACE) → detonate.
//             mouse WHEEL = charge size.
//                BLAST    the all-rounder: shockwave, fireball, crater
//                GRENADE  small + sharp, all shrapnel, tiny fireball
//                MOLOTOV  no shockwave — a flame splash that paints fire
//                CAR BOMB medium, debris-heavy, throws chunks far
//                GAS MAIN a hiss, then a column of fire ERUPTS upward
//                RAM CAR  a car screams in from off-screen at your click and
//                         crashes (car-bomb) into the first wall it meets
//     BRUSHES click a brush, then click / drag to paint the world.
//                materials: ground/road/grass/wood/oil/water/barrel/car/glass/concrete
//                TORCH      light a fire by hand
//                CRATE      drop a loose crate to blow around
//                GASOLINE   pour a fuse (catches instantly, races along its trail)
//                EXPLOSIVE  a charge that detonates when the fuse-fire reaches it
//
//   ALWAYS    C clear all fire & smoke      R reset the scene
//
//   DESTRUCTIBLE WALLS: a blast knocks concrete/glass/wood cells loose as real
//   physics blocks that fly, tumble, slide to rest, and settle back into rubble
//   (the prototype of sloop's deferred per-tile demolition rung).
//   STRUCTURAL COLLAPSE: each building is one connected mass; sever a piece off
//   its main lump and that piece can't stand — it collapses (blow a wall, drop
//   the wing). LOOSE OBJECTS: the blast wave is a PUSHING FORCE — it flings nearby
//   parked cars, shoves debris/blocks already on the ground, and slides CRATES
//   (wooden boxes that sit until something hits them, then tumble and settle,
//   splintering if a blast lands close or they catch fire). A ram car bowls crates
//   over too. Set off a blast near a lot or a crate stack and it all scatters.
//   PEDESTRIANS: little people wander the open streets, FLEE from fire and blasts,
//   and go down (killed by a blast, by standing in fire, or run over) leaving a body.
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

// ── toolbar: a row of cute clickable pixel-art buttons (sprites in boom.cart.js).
// Picking a tool is CLICK-ONLY now — no keyboard shortcuts. BOOM mode shows the
// blast icons (slots 0..5); BUILD mode shows the brush icons (slot = 16 + value).
#define TB_X   2
#define TB_Y0  11        // row 0: the BOOM blasts
#define TB_Y1  29        // row 1: the BUILD brushes
#define TB_SZ  16
#define TB_GAP 2
#define TB_PITCH (TB_SZ + TB_GAP)
#define TB_NBOOM  (BL_KINDS + 1)   // blast buttons (incl. ram car)

// ── materials ────────────────────────────────────────────────────────────────
// fuel  = how long a cell burns once lit.   resist = % shaved off a neighbour's
// spread roll (high = hard to catch; oil/barrel negative = eager).   hp =
// structural integrity for non-flammable props (glass/concrete).   col = tint.
// GASOLINE is a thin, eager fuse fuel: it catches almost instantly and races
// along its own trail (very negative resist), then burns out fast (low fuel) —
// pour a line, light one end, watch the flame run to whatever it reaches.
// TNT is a placed charge: once fire touches it, it cooks off into a real blast.
// Together they're the fuse-and-bomb toy — lay gasoline to a TNT, torch the fuse.
enum { MAT_GROUND, MAT_ROAD, MAT_GRASS, MAT_WOOD, MAT_OIL, MAT_WATER, MAT_ASH,
       MAT_BARREL, MAT_CAR, MAT_GLASS, MAT_CONCRETE, MAT_RUBBLE,
       MAT_GAS, MAT_TNT, MAT_KINDS };
#define TORCH MAT_KINDS                  // brush sentinel: light a fire by hand
#define CRATE_BRUSH (MAT_KINDS + 1)      // brush sentinel: drop a loose crate (a body, not a cell)
static const int MAT_FUEL[MAT_KINDS]   = {  0,  0,  8, 90, 45,  0,  0,  30, 60,  0,  0,  0,   6, 10 };
static const int MAT_RESIST[MAT_KINDS] = {  0,  0, 80, 20,-40,  0,  0, -20, 40,  0,  0,  0, -60,-10 };
static const int MAT_HP[MAT_KINDS]     = {  0,  0,  0,  0,  0,  0,  0,   0,  0,  4, 16,  0,   0,  0 };
static const int MAT_COL[MAT_KINDS]    = {
    CLR_BROWNISH_BLACK, CLR_DARK_GREY, CLR_DARK_GREEN, CLR_BROWN,
    CLR_DARKER_GREY,    CLR_DARK_BLUE, CLR_BROWNISH_BLACK,
    CLR_RED,            CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_LIGHT_GREY, CLR_MEDIUM_GREY,
    CLR_DARK_PEACH,     CLR_DARK_RED
};
static const char *MAT_NAME[MAT_KINDS] = {
    "ground","road","grass","wood","oil","water","ash",
    "barrel","car","glass","concrete","rubble",
    "gasoline","explosive"
};

// the BUILD palette, in toolbar order. Each entry maps to icon slot 16 + value
// (so TORCH/CRATE_BRUSH, being MAT_KINDS / MAT_KINDS+1, get slots 30 / 31).
static const int BUILD_BRUSHES[] = { MAT_GROUND, MAT_ROAD, MAT_GRASS, MAT_WOOD, MAT_OIL,
                                     MAT_WATER, MAT_BARREL, MAT_CAR, MAT_GLASS, MAT_CONCRETE,
                                     MAT_GAS, MAT_TNT, TORCH, CRATE_BRUSH };
#define NBUILD ((int)(sizeof BUILD_BRUSHES / sizeof *BUILD_BRUSHES))

// ── the world grid ────────────────────────────────────────────────────────────
// bid = building id (1+) for concrete, so structural collapse knows which cells
// belong to the same building; 0 = ungrouped (ground, props, player-painted wall).
typedef struct { unsigned char mat, fuel, fire, hp, bid; } Cell;
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

// ── ram car: a kamikaze vehicle that screams in from off-screen at the click,
// crashes into the first wall (or the target) and goes up as a car bomb. Not a
// car SIM — a directed projectile with a heading, dust, and an impact. ─────────
typedef struct { float x, y, vx, vy, tx, ty; int chg; unsigned char on, col; } Ramcar;
#define MAXCAR 4
static Ramcar rcar[MAXCAR];

// ── crates: loose wooden boxes the blast wave SHOVES around. Unlike a wall cell
// (fixed) or a Block (transient debris that settles to rubble), a crate is a
// persistent body that sits until a force hits it — a blast push, a ram car —
// then slides, tumbles, and settles, pushable again. A close blast smashes it to
// planks; fire eats it. The toy that makes "a blast is a pushing force" legible.
typedef struct { float x, y, vx, vy, ang, spin; int hp; unsigned char on; } Crate;
#define MAXCRATE 48
static Crate crate[MAXCRATE];

static void place_crate(float x, float y) {
    for (int i = 0; i < MAXCRATE; i++) if (!crate[i].on) {
        crate[i] = (Crate){ x, y, 0, 0, 0, 0, 28, 1 };
        return;
    }
}

// ── pedestrians: the city's little lives. They wander the open streets, FLEE
// from fire and blasts (run away from the nearest threat), and go DOWN — killed
// by a blast, by standing in fire, or run over by a ram car — leaving a body.
// What turns the destruction sandbox into a scene that reacts to you.
enum { PED_CALM, PED_FLEE, PED_DOWN };
typedef struct {
    float x, y, vx, vy, wx, wy;   // pos, knockback vel, heading unit-vector
    int   panic, hdg_t, state;
    unsigned char on, col;
} Ped;
#define MAXPED 40
static Ped peds[MAXPED];
static const int PED_CLOTHES[] = {
    CLR_BLUE, CLR_RED, CLR_DARK_GREEN, CLR_INDIGO, CLR_DARK_PURPLE, CLR_PINK, CLR_TRUE_BLUE
};

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

// the ram car is selectable as a 6th BOOM "blast" past the real archetypes
#define RAMCAR BL_KINDS

// launch a car from the far horizontal edge, aimed straight at (tx,ty) — it
// crosses the screen so it reads as "screaming in", then crashes on arrival.
static void launch_car(float tx, float ty, int chg) {
    for (int i = 0; i < MAXCAR; i++) if (!rcar[i].on) {
        // come from the NEAR horizontal edge so it converges on the click (and
        // crashes into the wall THERE, not one it meets crossing the whole map)
        float sx = (tx < SCREEN_W * 0.5f) ? -8.0f : SCREEN_W + 8.0f;
        float sy = ty + (frand() - 0.5f) * 8.0f;
        float dx = tx - sx, dy = ty - sy;
        float d  = sqrtf(dx * dx + dy * dy) + 0.01f;
        float spd = 4.5f + chg * 0.4f;
        rcar[i] = (Ramcar){ sx, sy, dx / d * spd, dy / d * spd, tx, ty, chg, 1,
                            (unsigned char)(frand() < 0.5f ? CLR_TRUE_BLUE : CLR_RED) };
        hit(58, INSTR_NOISE, 32, 240);   // tyre screech as it launches
        return;
    }
}

// is this cell something a speeding car would crash INTO (a wall/prop, not road)?
static int car_blocks(int gx, int gy) {
    if (gx < 0 || gy < 0 || gx >= GW || gy >= GH) return 0;
    int m = W[IDX(gx, gy)].mat;
    return m == MAT_CONCRETE || m == MAT_GLASS || m == MAT_WOOD ||
           m == MAT_CAR || m == MAT_BARREL || m == MAT_RUBBLE;
}

// blast radius in cells before the per-archetype scale — charge 1 is small and
// punchy, climbing to a city-block crater at charge 5. (debris COUNT below keeps
// its own chg curve; this is only the footprint.)
#define BASE_RADC(c) (2 + (c) * 2)

// ── structural collapse ──────────────────────────────────────────────────────
// Top-down, "support" = connectivity. A building (a bid group) STANDS as its
// largest connected lump of concrete; once a blast severs a smaller piece off
// that lump, the piece can't hold itself up and COLLAPSES to rubble + sheds
// blocks. So blowing a wall out drops the wing beyond it — blow a building in
// half and the smaller half comes down. Player-painted concrete is bid 0
// (ungrouped): there a piece just needs < COLLAPSE_MIN cells to fall. Runs once
// per blast (after the pressure pass) — a flood-fill over concrete, never per-frame.
#define COLLAPSE_MIN 5
static int ccomp[NCELL];      // component id per cell (-1 = not concrete)
static int csize[NCELL];      // cell count of each component
static int cbidv[NCELL];      // building id of each component
static int cstack[NCELL];
static int cbig[256];         // largest standing component size, per building id

static void collapse_concrete(float ox, float oy) {
    for (int i = 0; i < NCELL; i++) ccomp[i] = -1;
    int nc = 0;
    for (int s = 0; s < NCELL; s++) {                  // label connected concrete lumps
        if (W[s].mat != MAT_CONCRETE || ccomp[s] >= 0) continue;
        int top = 0, n = 0;
        cstack[top++] = s; ccomp[s] = nc;
        while (top) {
            int idx = cstack[--top];
            n++;
            int x = idx % GW, y = idx / GW;
            int nb[4] = { x > 0 ? idx-1 : -1, x < GW-1 ? idx+1 : -1,
                          y > 0 ? idx-GW : -1, y < GH-1 ? idx+GW : -1 };
            for (int k = 0; k < 4; k++) {
                int j = nb[k];
                if (j >= 0 && ccomp[j] < 0 && W[j].mat == MAT_CONCRETE) { ccomp[j] = nc; cstack[top++] = j; }
            }
        }
        csize[nc] = n; cbidv[nc] = W[s].bid; nc++;
    }
    // the biggest lump of each building is its standing core
    for (int b = 0; b < 256; b++) cbig[b] = 0;
    for (int c = 0; c < nc; c++) if (csize[c] > cbig[cbidv[c]]) cbig[cbidv[c]] = csize[c];

    for (int s = 0; s < NCELL; s++) {                  // collapse the severed pieces
        if (W[s].mat != MAT_CONCRETE) continue;
        int c = ccomp[s], b = cbidv[c];
        int fall = b > 0 ? (csize[c] < cbig[b]) : (csize[c] < COLLAPSE_MIN);
        if (!fall) continue;
        int x = s % GW, y = s / GW;
        float px = x * CELL + 2, py = y * CELL + 2;
        W[s].mat = MAT_RUBBLE; W[s].hp = 0; W[s].fuel = 0; W[s].fire = 0; W[s].bid = 0;
        if (frand() < 0.5f) eject_block(px, py, ox, oy, MAT_COL[MAT_CONCRETE], MAT_CONCRETE);
        psp(px, py, frand() * 0.5f - 0.25f, -0.2f - frand() * 0.3f, PK_SMOKE,
            40 + frand() * 40, 2 + frand() * 2, 0, 0);
    }
}

// a cell a pedestrian can stand/walk on (open ground; not water, walls, or props)
static int ped_walkable(int gx, int gy) {
    if (gx < 0 || gy < 0 || gx >= GW || gy >= GH) return 0;
    int m = W[IDX(gx, gy)].mat;
    return !(m == MAT_WATER || m == MAT_CONCRETE || m == MAT_GLASS ||
             m == MAT_WOOD || m == MAT_CAR || m == MAT_BARREL);
}

// drop a pedestrian: a body, a little dark-red splatter (a lick of flame if fiery)
static void ped_kill(Ped *p, int fiery) {
    if (p->state == PED_DOWN) return;
    p->state = PED_DOWN; p->vx = p->vy = 0; p->panic = 0;
    for (int s = 0; s < 4; s++)
        psp(p->x, p->y, frand() * 2 - 1, frand() * 2 - 1, PK_SPARK, 8 + frand() * 6, 1, 1, CLR_DARK_RED);
    if (fiery) psp(p->x, p->y, 0, -0.5f, PK_FLAME, 14, 1.6f, 0.6f, 0);
}

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

    // shove everything LOOSE outward (the blast wave) — particles AND the physics
    // blocks/cars already lying on the ground, so a blast scatters nearby debris
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
        for (int i = 0; i < MAXBLK; i++) {
            if (!blk[i].on) continue;
            float dx = blk[i].x - cx, dy = blk[i].y - cy;
            float d = sqrtf(dx * dx + dy * dy) + 0.01f;
            if (d < R2) {
                float push = (R2 - d) / R2 * (2.0f + chg) * b->shock;
                blk[i].vx += dx / d * push; blk[i].vy += dy / d * push;
                blk[i].spin += (frand() - 0.5f) * 0.4f; blk[i].settle = 0;
            }
        }
        for (int i = 0; i < MAXCRATE; i++) {           // shove crates; smash the close ones
            if (!crate[i].on) continue;
            float dx = crate[i].x - cx, dy = crate[i].y - cy;
            float d = sqrtf(dx * dx + dy * dy) + 0.01f;
            if (d < R2) {
                float push = (R2 - d) / R2 * (2.5f + chg) * b->shock;
                crate[i].vx += dx / d * push; crate[i].vy += dy / d * push;
                crate[i].spin += (frand() - 0.5f) * 0.6f;
                if (d < R * 0.7f) crate[i].hp -= (int)(22 * (1.0f - d / (R * 0.7f)));
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
                } else if (c->mat == MAT_CAR) {
                    // the blast wave flips a nearby car — it's flung as a loose body
                    // (cars farther out stay put and CATCH FIRE in the heat pass)
                    if (press * t > 0.55f) {
                        c->mat = MAT_GROUND; c->fuel = 0; c->fire = 0;
                        eject_block(px, py, cx, cy, MAT_COL[MAT_CAR], MAT_CAR);
                        if (frand() < 0.4f)
                            psp(px, py, frand() * 2 - 1, frand() * 2 - 1, PK_SPARK,
                                8 + frand() * 8, 1, 1, CLR_LIGHT_YELLOW);
                    }
                }
            }
        }
        // a severed chunk of a building can no longer stand → bring it down
        collapse_concrete(cx, cy);
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

    // pedestrians: the close ones go down, the rest panic and flee the blast
    // (panic fires for ANY blast — even the shockless molotov is terrifying)
    for (int i = 0; i < MAXPED; i++) {
        Ped *p = &peds[i];
        if (!p->on || p->state == PED_DOWN) continue;
        float dx = p->x - cx, dy = p->y - cy;
        float d = sqrtf(dx * dx + dy * dy) + 0.01f;
        if (d < R * 0.55f) {
            ped_kill(p, 1);
        } else if (d < R * 2.4f) {
            p->state = PED_FLEE; p->panic = 100;
            p->wx = dx / d; p->wy = dy / d;                // flee straight away
            if (b->shock > 0) {                            // and get physically shoved
                float push = (R * 2.4f - d) / (R * 2.4f) * (2.0f + chg) * b->shock;
                p->vx += dx / d * push; p->vy += dy / d * push;
            }
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
        // a placed charge: the moment the fuse-fire reaches it, it goes off as a
        // proper blast (eager — fire >= 2 is enough — so the fuse "arrives" and BOOMs)
        if (c->mat == MAT_TNT && c->fire >= 2 && dets_this_tick < 6 && frand() < 0.6f) {
            dets_this_tick++;
            W[IDX(x, y)].mat = MAT_RUBBLE; c->fire = 0; c->fuel = 0;
            detonate(x * CELL + CELL * 0.5f, y * CELL + CELL * 0.5f, 3, BL_BLAST);
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

// the player-facing name of a toolbar button (row 0 = blasts, row 1 = brushes)
static const char *tool_name(int row, int i) {
    if (row == 0) return (i == RAMCAR) ? "ram car" : BSPEC[i].name;
    int b = BUILD_BRUSHES[i];
    return b == TORCH ? "torch" : b == CRATE_BRUSH ? "crate" : MAT_NAME[b];
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
        for (int i = 0; i < MAXCAR; i++) rcar[i].on = 0;
    }
    float w = mouse_wheel();
    if (w != 0) { charge += (w > 0 ? 1 : -1); if (charge < 1) charge = 1; if (charge > 5) charge = 5; }

    int mx = mouse_x(), my = mouse_y();

    // ── toolbar: BOTH rows always on screen (blasts above, brushes below).
    // Clicking any button picks that tool AND sets the matching mode — so every
    // tool is one click away, no TAB needed. (TAB still flips mode as a shortcut.)
    int over_tb = 0, hit_row = -1, hit_i = -1;
    {
        int ry[2] = { TB_Y0, TB_Y1 }, rn[2] = { TB_NBOOM, NBUILD };
        for (int r = 0; r < 2; r++)
            if (my >= ry[r] && my < ry[r] + TB_SZ && mx >= TB_X) {
                int i = (mx - TB_X) / TB_PITCH;
                if (i >= 0 && i < rn[r]) { over_tb = 1; hit_row = r; hit_i = i; }
            }
    }
    if (over_tb && mouse_pressed(0)) {
        if (hit_row == 0) { mode = 0; blast = hit_i; }     // 0..5, 5 = RAMCAR
        else              { mode = 1; brush = BUILD_BRUSHES[hit_i]; }
        hit(74, INSTR_SQUARE, 1, 26);                      // soft UI click
    }

    if (mode == 0) {                                  // BOOM
        if (!over_tb && (mouse_pressed(0) || keyp(KEY_SPACE))) {
            if (blast == RAMCAR) launch_car(mx, my, charge);
            else                 detonate(mx, my, charge, blast);
        }
    } else if (brush == CRATE_BRUSH) {                // BUILD: drop a crate per click
        if (!over_tb && mouse_pressed(0)) place_crate(mx, my);
    } else {                                          // BUILD: paint cells
        if (!over_tb && mouse_down(0)) {
            int gx = mx / CELL, gy = my / CELL;
            for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++) {
                int x = gx + ox, y = gy + oy;
                if (x < 0 || y < 0 || x >= GW || y >= GH) continue;
                if (brush == TORCH) { ignite(x, y); }
                else { Cell *c = &W[IDX(x, y)]; c->mat = (unsigned char)brush;
                       c->fire = 0; c->fuel = (unsigned char)MAT_FUEL[brush];
                       c->hp = (unsigned char)MAT_HP[brush]; c->bid = 0; }   // player wall = ungrouped
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

    // ram cars: drive toward the target, trail dust + sparks, crash on contact
    for (int i = 0; i < MAXCAR; i++) {
        Ramcar *r = &rcar[i];
        if (!r->on) continue;
        r->x += r->vx; r->y += r->vy;
        // tyre dust + friction sparks streaming off the back
        float bx = r->x - r->vx, by = r->y - r->vy;
        psp(bx, by, -r->vx * 0.1f + frand() * 0.3f - 0.15f, -r->vy * 0.1f - 0.2f,
            PK_SMOKE, 18 + frand() * 16, 1.5f + frand() * 1.5f, 0, 0);
        if ((int)(frand() * 100) < 40)
            psp(bx, by, -r->vx * 0.2f, -r->vy * 0.2f, PK_SPARK,
                6 + frand() * 6, 1, 1, CLR_LIGHT_YELLOW);
        for (int q = 0; q < MAXCRATE; q++) {                   // bowl over crates in its path
            if (!crate[q].on) continue;
            float ddx = crate[q].x - r->x, ddy = crate[q].y - r->y;
            if (ddx * ddx + ddy * ddy < 49) {                  // within ~7px
                crate[q].vx += r->vx * 0.6f; crate[q].vy += r->vy * 0.6f;
                crate[q].spin += (frand() - 0.5f) * 0.6f;
            }
        }
        for (int q = 0; q < MAXPED; q++) {                     // run down anyone in the way
            if (!peds[q].on || peds[q].state == PED_DOWN) continue;
            float ddx = peds[q].x - r->x, ddy = peds[q].y - r->y;
            if (ddx * ddx + ddy * ddy < 36) ped_kill(&peds[q], 0);
        }

        int gx = (int)(r->x / CELL), gy = (int)(r->y / CELL);
        float dtx = r->tx - r->x, dty = r->ty - r->y;
        int reached = (dtx * r->vx + dty * r->vy) <= 0;        // passed the target point
        int offmap  = r->x < -12 || r->x > SCREEN_W + 12 || r->y < -12 || r->y > SCREEN_H + 12;
        if (car_blocks(gx, gy) || reached) {                   // CRASH
            hit(34, INSTR_NOISE, 70, 90);                      // metal bang
            detonate_now(r->x, r->y, r->chg + 1, BL_CARBOMB);
            // a forward spray of wreckage in the travel direction
            float d = sqrtf(r->vx * r->vx + r->vy * r->vy) + 0.01f;
            for (int s = 0; s < 6; s++)
                psp(r->x, r->y, r->vx / d * (1 + frand() * 2) + frand() - 0.5f,
                    r->vy / d * (1 + frand() * 2) + frand() - 0.5f, PK_DEBRIS,
                    20 + frand() * 20, 1 + frand() * 2, 0,
                    frand() < 0.5f ? CLR_DARK_GREY : r->col);
            r->on = 0;
        } else if (offmap) r->on = 0;                          // missed entirely, despawn quietly
    }

    // crates: slide + tumble from a push, settle (persist, pushable again),
    // splinter into planks once smashed (close blast) or burnt (sitting in fire)
    for (int i = 0; i < MAXCRATE; i++) {
        Crate *cr = &crate[i];
        if (!cr->on) continue;
        cr->x += cr->vx; cr->y += cr->vy;
        cr->vx *= 0.90f; cr->vy *= 0.90f;
        cr->ang += cr->spin; cr->spin *= 0.92f;
        if (cr->x < -6 || cr->y < -6 || cr->x > SCREEN_W + 6 || cr->y > SCREEN_H + 6) { cr->on = 0; continue; }
        int gx = (int)(cr->x / CELL), gy = (int)(cr->y / CELL);
        if (gx >= 0 && gy >= 0 && gx < GW && gy < GH && W[IDX(gx, gy)].fire > 0) {   // burning
            cr->hp -= 2;
            if ((int)(frand() * 100) < 25)
                psp(cr->x, cr->y, frand() * 0.4f - 0.2f, -0.5f - frand() * 0.4f, PK_FLAME,
                    10 + frand() * 8, 1.5f + frand(), 0.6f, 0);
        }
        if (cr->hp <= 0) {                           // splinter into wood blocks
            for (int s = 0; s < 4; s++) {
                float a = frand() * 6.2832f;
                eject_block(cr->x, cr->y, cr->x - cosf(a), cr->y - sinf(a), MAT_COL[MAT_WOOD], MAT_WOOD);
            }
            psp(cr->x, cr->y, 0, -0.3f, PK_SMOKE, 30 + frand() * 20, 2 + frand() * 2, 0, 0);
            cr->on = 0;
        }
    }

    // pedestrians: wander → flee fire/blasts → die in fire; bodies stay
    for (int i = 0; i < MAXPED; i++) {
        Ped *p = &peds[i];
        if (!p->on || p->state == PED_DOWN) continue;
        int gx = (int)(p->x / CELL), gy = (int)(p->y / CELL);
        if (gx >= 0 && gy >= 0 && gx < GW && gy < GH && W[IDX(gx, gy)].fire > 0) { ped_kill(p, 1); continue; }
        // sense the nearest fire within ~4 cells → run away from it
        float nfx = 0, nfy = 0; int nfd = 999;
        for (int oy = -4; oy <= 4; oy++)
        for (int ox = -4; ox <= 4; ox++) {
            int cx2 = gx + ox, cy2 = gy + oy;
            if (cx2 < 0 || cy2 < 0 || cx2 >= GW || cy2 >= GH) continue;
            if (W[IDX(cx2, cy2)].fire > 0) { int dd = ox*ox + oy*oy; if (dd < nfd) { nfd = dd; nfx = ox; nfy = oy; } }
        }
        if (nfd < 999) {
            float l = sqrtf(nfx*nfx + nfy*nfy) + 0.01f;
            p->wx = -nfx / l; p->wy = -nfy / l; p->state = PED_FLEE; if (p->panic < 60) p->panic = 60;
        }
        if (p->state == PED_CALM && --p->hdg_t <= 0) {   // pick a new stroll heading
            float a = frand() * 6.2832f; p->wx = cosf(a); p->wy = sinf(a); p->hdg_t = 60 + rnd(120);
        }
        if (p->state == PED_FLEE && --p->panic <= 0) p->state = PED_CALM;
        float spd = (p->state == PED_FLEE) ? 1.3f : 0.32f;
        float nx = p->x + p->wx * spd + p->vx, ny = p->y + p->wy * spd + p->vy;
        if (ped_walkable((int)nx / CELL, gy)) p->x = nx; else p->wx = -p->wx;   // bounce off walls/water
        if (ped_walkable(gx, (int)ny / CELL)) p->y = ny; else p->wy = -p->wy;
        p->vx *= 0.85f; p->vy *= 0.85f;                  // knockback decays
        if (p->x < 1) p->x = 1; if (p->x > SCREEN_W - 1) p->x = SCREEN_W - 1;
        if (p->y < 10) p->y = 10; if (p->y > SCREEN_H - 1) p->y = SCREEN_H - 1;
    }
}

// ── default scene: a block of the criminal city to set alight ───────────────
static void stamp_building(int bx, int by, int bw, int bh, int mat, int glassfront, int bid) {
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
        c->bid  = (m == MAT_CONCRETE) ? (unsigned char)bid : 0;   // group for collapse
    }
}

void reset_world(void) {
    for (int i = 0; i < NCELL; i++) { W[i].mat = MAT_GROUND; W[i].fire = 0; W[i].fuel = 0; W[i].hp = 0; W[i].bid = 0; }
    // grass everywhere as a base, then carve roads through it
    for (int i = 0; i < NCELL; i++) { W[i].mat = MAT_GRASS; W[i].fuel = (unsigned char)MAT_FUEL[MAT_GRASS]; }
    for (int x = 0; x < GW; x++) for (int dy = 0; dy < 2; dy++) {
        W[IDX(x, GH/2 + dy)].mat = MAT_ROAD; W[IDX(x, GH/2 + dy)].fuel = 0;
    }
    for (int y = 0; y < GH; y++) for (int dx = 0; dx < 2; dx++) {
        W[IDX(GW/3 + dx, y)].mat = MAT_ROAD; W[IDX(GW/3 + dx, y)].fuel = 0;
    }

    // concrete blocks with glass shopfronts (each its own bid for collapse), plus wood shacks
    stamp_building( 6,  6, 12,  9, MAT_CONCRETE, 1, 1);
    stamp_building(50,  6, 16, 11, MAT_CONCRETE, 1, 2);
    stamp_building(55, 33, 14, 10, MAT_CONCRETE, 1, 3);
    stamp_building(10, 34,  9,  7, MAT_WOOD,     0, 0);
    stamp_building(34, 35,  8,  6, MAT_WOOD,     0, 0);

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

    // cars parked all over — fodder for the blast wave. each car is 3 cells long.
    static const int cx[] = { 10, 24, 40, 54, 62, 8, 30, 4, 70 };
    static const int cy[] = { GH/2 - 1, GH/2 + 2, GH/2 + 2, GH/2 - 1, GH/2 - 1, 18, 18, 30, 30 };
    for (int i = 0; i < (int)(sizeof cx / sizeof *cx); i++)
    for (int dx = 0; dx < 3; dx++) {
        int x = cx[i] + dx, y = cy[i];
        if (x < 0 || x >= GW || y < 0 || y >= GH) continue;
        W[IDX(x, y)].mat = MAT_CAR; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_CAR];
    }
    // a packed parking lot on open ground — rows of cars, the chain-reaction showpiece
    for (int row = 0; row < 3; row++)
    for (int car = 0; car < 4; car++) {
        int bx0 = 30 + car * 4, y = 30 + row * 2;
        for (int dx = 0; dx < 3; dx++) {
            int x = bx0 + dx;
            if (x < GW && y < GH) { W[IDX(x, y)].mat = MAT_CAR; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_CAR]; }
        }
    }
    // a couple parked on the vertical road
    for (int dy = 0; dy < 3; dy++) {
        int x = GW/3 - 1, y = 8 + dy;
        if (x >= 0 && y < GH) { W[IDX(x, y)].mat = MAT_CAR; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_CAR]; }
    }

    // a demo fuse: a gasoline trail running into a little cluster of explosives,
    // down in the open at the bottom — torch the far end and the flame races in
    {
        int fy = GH - 5;
        for (int x = 22; x < 44 && x < GW; x++)
            if (fy >= 0 && fy < GH && W[IDX(x, fy)].mat == MAT_GRASS) {
                W[IDX(x, fy)].mat = MAT_GAS; W[IDX(x, fy)].fuel = (unsigned char)MAT_FUEL[MAT_GAS];
            }
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 3; dx++) {
            int x = 44 + dx, y = fy - 1 + dy;
            if (x >= 0 && x < GW && y >= 0 && y < GH) {
                W[IDX(x, y)].mat = MAT_TNT; W[IDX(x, y)].fuel = (unsigned char)MAT_FUEL[MAT_TNT];
            }
        }
    }

    // crate stacks on open ground — loose boxes for the blast wave to shove + smash
    int kn = 0;
    static const int kbx[]   = { 72, 150, 160 }, kby[]   = { 64, 152, 92 };
    static const int kcols[] = {  4,   3,   2 }, krows[] = {  2,   2,  2 };
    for (int cl = 0; cl < 3; cl++)
    for (int r = 0; r < krows[cl]; r++)
    for (int c = 0; c < kcols[cl]; c++)
        if (kn < MAXCRATE)
            crate[kn++] = (Crate){ kbx[cl] + c * 7.0f, kby[cl] + r * 7.0f, 0, 0, 0, 0, 28, 1 };
    for (; kn < MAXCRATE; kn++) crate[kn].on = 0;

    // pedestrians wandering the open streets (skip walls/water)
    int pn = 0;
    for (int tries = 0; tries < 600 && pn < 26; tries++) {
        int gx = rnd(GW), gy = rnd(GH);
        if (!ped_walkable(gx, gy)) continue;
        float a = frand() * 6.2832f;
        peds[pn++] = (Ped){ gx * CELL + 2.0f, gy * CELL + 2.0f, 0, 0, cosf(a), sinf(a),
                            0, rnd(120), PED_CALM, 1, (unsigned char)PED_CLOTHES[rnd(7)] };
    }
    for (; pn < MAXPED; pn++) peds[pn].on = 0;

    for (int i = 0; i < MAXP; i++) ps[i].kind = PK_FREE;
    for (int i = 0; i < 12; i++) rings[i].on = 0;
    for (int i = 0; i < 8; i++) pend[i].on = 0;
    for (int i = 0; i < MAXBLK; i++) blk[i].on = 0;
    for (int i = 0; i < MAXCAR; i++) rcar[i].on = 0;
}

static int inited = 0;

// ── render ───────────────────────────────────────────────────────────────
void draw(void) {
    if (!inited) { reset_world(); inited = 1; wind_t = 1; colorkey(0); }   // index 0 = transparent for toolbar icons

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
        } else if (c->mat == MAT_GAS) {
            pset(x * CELL + 1, y * CELL + 2, CLR_PEACH);        // wet gasoline sheen
        } else if (c->mat == MAT_TNT) {
            pset(x * CELL + 1, y * CELL + 1, CLR_YELLOW);       // charge marker
            pset(x * CELL + 2, y * CELL + 2, CLR_BLACK);
        }
        if (c->fire > 0) {
            int idx = c->fire - 1 - (int)(frand() * 2);     // flicker
            if (idx < 0) idx = 0;
            rectfill(x * CELL, y * CELL, CELL, CELL, FLAME_RAMP[idx]);
        }
    }

    // smoke (behind), then debris, then flames/embers/sparks (in front).
    // Thick (young) smoke is SOLID + dark; only as it thins does it break into
    // dither (1-bits transparent via hole_color -1) — 50% checker, then wispy
    // sparse dots and lighter grey — so it dissolves into the background at the end
    // of its life. Each puff ANCHORS the lattice to its own centre (fillp_anchor),
    // so the pattern travels WITH the cloud — its own offset, no crawl as it drifts.
    for (int i = 0; i < MAXP; i++) {
        P *p = &ps[i];
        if (p->kind != PK_SMOKE) continue;
        float t = p->life / p->maxlife;          // 1 young → 0 old
        int col = t > 0.55f ? CLR_DARKER_GREY : t > 0.3f ? CLR_DARK_GREY : CLR_MEDIUM_GREY;
        if (t > 0.55f) fillp_reset();            // thick smoke: solid, no dither
        else {
            fillp_anchor((int)p->x, (int)p->y);  // this cloud's own dither offset
            fillp(t > 0.3f ? FILL_CHECKER        // thinning: 50%
                           : (~FILL_DOTS & 0xFFFF), -1);   // wispy tail: mostly holes
        }
        circfill((int)p->x, (int)p->y, (int)p->size, col);
    }
    fillp_reset();                               // back to solid for everything below
    fillp_anchor(0, 0);                          // restore the default lattice origin
    for (int i = 0; i < MAXP; i++) {
        P *p = &ps[i];
        if (p->kind != PK_DEBRIS) continue;
        rectfill((int)p->x, (int)p->y, (int)p->size + 1, (int)p->size + 1, p->col);
    }
    // pedestrians — a 2px figure (body + head); a downed one is a dark-red splat
    for (int i = 0; i < MAXPED; i++) {
        Ped *p = &peds[i];
        if (!p->on) continue;
        int px = (int)p->x, py = (int)p->y;
        if (p->state == PED_DOWN) {
            pset(px - 1, py, CLR_DARK_RED); pset(px, py, CLR_DARK_RED); pset(px + 1, py, CLR_DARK_RED);
        } else {
            pset(px, py, p->col);                                       // body (clothing)
            pset(px, py - 1, p->state == PED_FLEE ? CLR_LIGHT_PEACH : CLR_PEACH);  // head
        }
    }
    // crates — wooden boxes (drawn under the debris so chunks land on top)
    for (int i = 0; i < MAXCRATE; i++) {
        Crate *cr = &crate[i];
        if (!cr->on) continue;
        int bx = (int)cr->x, by = (int)cr->y;
        rectfill(bx - 2, by - 2, 5, 5, CLR_BROWN);
        rect(bx - 2, by - 2, 5, 5, CLR_DARK_BROWN);                 // slats
        pset(bx + (int)(cosf(cr->ang) * 1.5f), by + (int)(sinf(cr->ang) * 1.5f), CLR_DARK_PEACH);
    }
    // physics blocks — knocked wall chunks (car chunks read bigger), a facet pixel for the tumble
    for (int i = 0; i < MAXBLK; i++) {
        Block *bk = &blk[i];
        if (!bk->on) continue;
        int bx = (int)bk->x, by = (int)bk->y;
        int h = (bk->mat == MAT_CAR) ? 2 : 1;                       // flung cars read as chunks, not specks
        rectfill(bx - h, by - h, h * 2 + 1, h * 2 + 1, bk->col);
        pset(bx + (int)(cosf(bk->ang) * 1.5f), by + (int)(sinf(bk->ang) * 1.5f), CLR_WHITE);
    }

    // ram cars — body long-axis along travel, a bright headlight pixel up front
    for (int i = 0; i < MAXCAR; i++) {
        Ramcar *r = &rcar[i];
        if (!r->on) continue;
        int horiz = fabsf(r->vx) >= fabsf(r->vy);
        int cxp = (int)r->x, cyp = (int)r->y;
        if (horiz) rectfill(cxp - 3, cyp - 1, 7, 3, r->col);
        else       rectfill(cxp - 1, cyp - 3, 3, 7, r->col);
        float d = sqrtf(r->vx * r->vx + r->vy * r->vy) + 0.01f;
        pset(cxp + (int)(r->vx / d * 3), cyp + (int)(r->vy / d * 3), CLR_WHITE);  // headlight
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
    if (mode == 0 && blast == RAMCAR) {
        // target reticle — the car comes to this spot from the far edge
        circ(mx, my, 5, CLR_RED);
        line(mx - 8, my, mx + 8, my, CLR_RED);
        line(mx, my - 8, mx, my + 8, CLR_RED);
    } else if (mode == 0) {
        int R = (int)(BASE_RADC(charge) * CELL * BSPEC[blast].rad);
        int col = blast == BL_MOLOTOV ? CLR_ORANGE : blast == BL_GASMAIN ? CLR_YELLOW : CLR_RED;
        circ(mx, my, R, col);
        circ(mx, my, 2, CLR_WHITE);
    } else {
        int gx = mx / CELL, gy = my / CELL;
        int bc = brush == TORCH ? CLR_ORANGE : brush == CRATE_BRUSH ? CLR_BROWN : MAT_COL[brush];
        if (brush == CRATE_BRUSH) rect(mx - 2, my - 2, 5, 5, bc);   // a box footprint
        else rect((gx - 1) * CELL, (gy - 1) * CELL, CELL * 3, CELL * 3, bc);
    }

    // cursor probe: name PRECISELY what's under the pointer (cell + its state),
    // in a little label that flips side/below so it never runs off-screen
    {
        int gx = mx / CELL, gy = my / CELL;
        int in_tb = 0;
        {
            int ry[2] = { TB_Y0, TB_Y1 }, rn[2] = { TB_NBOOM, NBUILD };
            for (int r = 0; r < 2; r++)
                if (my >= ry[r] && my < ry[r] + TB_SZ && mx >= TB_X &&
                    (mx - TB_X) / TB_PITCH < rn[r]) in_tb = 1;
        }
        if (!in_tb && gx >= 0 && gy >= 0 && gx < GW && gy < GH) {
            Cell *c = &W[IDX(gx, gy)];
            char lbl[48];
            if (c->fire > 0)
                snprintf(lbl, sizeof lbl, "%s  burning %d", MAT_NAME[c->mat], c->fire);
            else if (MAT_FUEL[c->mat] > 0)
                snprintf(lbl, sizeof lbl, "%s  fuel %d", MAT_NAME[c->mat], c->fuel);
            else if (MAT_HP[c->mat] > 0)
                snprintf(lbl, sizeof lbl, "%s  hp %d", MAT_NAME[c->mat], c->hp);
            else
                snprintf(lbl, sizeof lbl, "%s", MAT_NAME[c->mat]);
            font(FONT_SMALL);
            int tw = (int)strlen(lbl) * 4;
            int lx = mx + 6, ly = my - 8;
            if (lx + tw + 1 > SCREEN_W) lx = mx - 6 - tw;          // flip to the left edge
            if (lx < 1) lx = 1;
            if (ly < 10) ly = my + 8;                              // drop below if it'd hit the HUD
            rectfill(lx - 1, ly - 1, tw + 2, 7, CLR_BLACK);
            print(lbl, lx, ly, CLR_WHITE);
            font(FONT_NORMAL);
        }
    }

    // HUD ---------------------------------------------------------------------
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    char buf[64];
    if (mode == 0) {
        const char *bn = (blast == RAMCAR) ? "ram car" : BSPEC[blast].name;
        snprintf(buf, sizeof buf, "BOOM  %s  charge %d", bn, charge);
    } else {
        const char *bn = brush == TORCH ? "torch" : brush == CRATE_BRUSH ? "crate" : MAT_NAME[brush];
        snprintf(buf, sizeof buf, "BUILD  brush: %s", bn);
    }
    print(buf, 3, 1, CLR_WHITE);

    // toolbar: BOTH rows of cute clickable icons, always on screen. The active
    // tool gets a white frame + yellow halo; the hovered one a light frame.
    int ry[2] = { TB_Y0, TB_Y1 }, rn[2] = { TB_NBOOM, NBUILD };
    int hov_row = -1, hov_i = -1;
    for (int r = 0; r < 2; r++)
        if (my >= ry[r] && my < ry[r] + TB_SZ && mx >= TB_X) {
            int i = (mx - TB_X) / TB_PITCH;
            if (i >= 0 && i < rn[r]) { hov_row = r; hov_i = i; }
        }
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < rn[r]; i++) {
            int bx = TB_X + i * TB_PITCH, by = ry[r];
            int slot = (r == 0) ? i : (16 + BUILD_BRUSHES[i]);
            int sel  = (r == 0) ? (mode == 0 && i == blast)
                                : (mode == 1 && BUILD_BRUSHES[i] == brush);
            rectfill(bx - 1, by - 1, TB_SZ + 2, TB_SZ + 2, CLR_BLACK);   // button panel
            spr(slot, bx, by);
            rect(bx - 1, by - 1, TB_SZ + 2, TB_SZ + 2,
                 sel ? CLR_WHITE : (r == hov_row && i == hov_i ? CLR_LIGHT_GREY : CLR_DARKER_GREY));
            if (sel) rect(bx - 2, by - 2, TB_SZ + 4, TB_SZ + 4, CLR_YELLOW);
        }
    // hover tooltip — name the button under the pointer
    if (hov_row >= 0) {
        const char *nm = tool_name(hov_row, hov_i);
        font(FONT_SMALL);
        int tw = (int)strlen(nm) * 4;
        int lx = TB_X + hov_i * TB_PITCH;
        if (lx + tw + 1 > SCREEN_W) lx = SCREEN_W - tw - 1;
        int ly = TB_Y1 + TB_SZ + 2;
        rectfill(lx - 1, ly - 1, tw + 2, 7, CLR_BLACK);
        print(nm, lx, ly, CLR_YELLOW);
        font(FONT_NORMAL);
    }

    print("click a tool  wheel charge  C clear  R reset", 3, SCREEN_H - 8, CLR_LIGHT_GREY);

    // wind tell-tale (top-right arrow)
    int wx = SCREEN_W - 14, wy = 5;
    line(wx, wy, wx + (int)(windx * 9), wy + (int)(windy * 9), CLR_BLUE);
    pset(wx + (int)(windx * 9), wy + (int)(windy * 9), CLR_WHITE);
}
