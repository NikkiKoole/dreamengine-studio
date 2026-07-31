// ─────────────────────────────────────────────────────────────────────────────
// lockup/model.h — THE CONTRACT for the LOCKUP prison sim (tools/carts/lockup.c)
//
// Design: docs/design/lockup.md.  The honest core: a prison is a machine for
// meeting needs on a schedule, and the player only ever touches the machine.
//
// This file declares EVERY shared type, table, global and function. The modules
// (grid / path / actors / econ / art / score / hud) each implement their own
// slice and include ONLY this file plus engine headers — never each other.
// One translation unit: the cart includes each module header exactly once.
//
// ── RULES FOR MODULE AUTHORS ────────────────────────────────────────────────
//  1. Every global declared `extern` here is DEFINED in exactly one module
//     (the owner is named in the comment). Never define one you don't own.
//  2. Internal helpers are `static` AND prefixed with your module's tag, so two
//     modules can't collide in the shared TU:
//        grid → lkg_    path → lkp_    actors → lka_    econ → lke_
//        art  → lkr_    score → lks_   hud   → lkh_     cart → lkc_
//     Public functions keep the `lk_` prefix already used below.
//  3. Never rename or re-type anything here without saying so — it is frozen
//     for the duration of parallel construction. NEED something added? Add it
//     at the bottom of your own module's declaration block and flag it.
//  4. No engine-namespace collisions: `map`, `line`, `rect`, `circ`, `print`,
//     `spr`, `timer`, `now`, `SCALE`, `SCREEN_W/H` are all taken by studio.h.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LOCKUP_MODEL_H
#define LOCKUP_MODEL_H

#include "studio.h"

// ── geometry ────────────────────────────────────────────────────────────────
#define LK_TS    16                       // world px per tile == sprite size
#define LK_MW    96                       // map width  in tiles
#define LK_MH    64                       // map height in tiles
#define LK_N     (LK_MW * LK_MH)
#define LK_WPX   (LK_MW * LK_TS)          // world px wide  (1536)
#define LK_HPX   (LK_MH * LK_TS)          // world px high  (1024)

#define LK_MAXPATH  192
#define LK_MAXACT   256
#define LK_MAXROOM  256
#define LK_MAXJOB   512

// ── CART CONFIG (mirrored in tools/carts/lockup.cart.js — keep in sync) ─────
//   screenW 720, screenH 450, scale 2, resizable  → 45 × 28 tiles of view
//
// ── SPRITE SLOT MAP — FROZEN ────────────────────────────────────────────────
// The sheet is a HARD 128×128 = 8 cols × 8 rows of 16×16 = 64 slots, no more
// (tools/make-cart.js:161). slot = row*8 + col. A multi-tile object uses
// s, s+1 across and s+8 down — i.e. exactly what sprite-draw's split() emits.
//
//   row 0   0..7   person  S0 S1 S2  N0 N1 N2  E0 E1
//   row 1   8..15  person  E2  W0 W1 W2 · sleep down fight sit
//   row 2  16..23  floors  dirt grass gravel concrete concrete2 tile tile2 wood
//   row 3  24..31  bed(top) toilet sink showerhead bench serving cooker fridge
//   row 4  32..39  bed(bot) desk chair cabinet tv phone weights bookshelf
//   row 5  40..47  table(L) table(R) pool(TL) pool(TR) medbed locker light detector
//   row 6  48..55  asphalt concrete3 pool(BL) pool(BR) cctv bin bunk grass2
//   row 7  56..63  icons: alarm money tray contraband wrench key star skull
//
// WALLS ARE NOT SPRITES. They are drawn procedurally in C from Tile.joins so
// corners/tees/ends join as one structure and every material is free. That is
// what buys us the slot budget above — do not "fix" it with a wall tileset.
#define SP_PERSON        0   // dir*3 + stride, dir 0=S 1=N 2=E 3=W, stride 0..2
#define SP_SLEEP        12
#define SP_DOWN         13
#define SP_FIGHT        14
#define SP_SIT          15
#define SP_ICON         56   // + 0..7, see row 7 above
#define SP_ICON_ALARM   56
#define SP_ICON_MONEY   57
#define SP_ICON_TRAY    58
#define SP_ICON_BAND    59   // contraband
#define SP_ICON_WRENCH  60
#define SP_ICON_KEY     61
#define SP_ICON_STAR    62
#define SP_ICON_SKULL   63

// magic palette indices for pal() recolouring of people (finalfight convention)
#define LK_MAGIC_UNIFORM  28   // CLR_TRUE_BLUE   → jumpsuit / uniform body
#define LK_MAGIC_TROUSER  29   // CLR_MAUVE       → trousers / dark accent
#define LK_MAGIC_SKIN     30   // CLR_DARK_PEACH  → skin
#define LK_MAGIC_HAIR     26   // CLR_LIME_GREEN  → hair

// ── floor materials ─────────────────────────────────────────────────────────
enum { FL_DIRT = 0, FL_GRASS, FL_GRAVEL, FL_CONCRETE, FL_TILE, FL_WOOD,
       FL_ASPHALT, FL_COUNT };

// two sprite variants per material, picked by Tile.var, so a big slab of
// concrete doesn't visibly repeat at grid pitch.
static const unsigned char LK_FLOOR_SPR[FL_COUNT][2] = {
    { 16, 16 },   // FL_DIRT
    { 17, 55 },   // FL_GRASS
    { 18, 49 },   // FL_GRAVEL
    { 19, 20 },   // FL_CONCRETE
    { 21, 22 },   // FL_TILE
    { 23, 23 },   // FL_WOOD
    { 48, 48 },   // FL_ASPHALT
};
static const short LK_FLOOR_COST[FL_COUNT] = { 0, 2, 3, 6, 10, 12, 8 };
static const char *const LK_FLOOR_NAME[FL_COUNT] = {
    "dirt", "grass", "gravel", "concrete", "tile", "wood", "asphalt"
};

// ── wall materials.  WL_NONE == no wall.  Walls are drawn PROCEDURALLY in C
// from `joins` (never a sprite), so corners/tees/ends read as one structure. ──
enum { WL_NONE = 0, WL_BRICK, WL_CONCRETE, WL_FENCE, WL_PERIM, WL_COUNT };

// ── doors.  Traversable but PERMISSIONED (see lk_can_pass). ─────────────────
enum { DR_NONE = 0, DR_PLAIN, DR_JAIL, DR_STAFF, DR_GATE, DR_COUNT };

// ── objects ─────────────────────────────────────────────────────────────────
enum { OB_NONE = 0,
       OB_BED, OB_BUNK, OB_TOILET, OB_SINK, OB_SHOWERHEAD,
       OB_TABLE, OB_BENCH, OB_SERVING, OB_COOKER, OB_FRIDGE,
       OB_DESK, OB_CHAIR, OB_CABINET, OB_TV, OB_POOLTABLE,
       OB_PHONE, OB_WEIGHTS, OB_BOOKSHELF, OB_MEDBED, OB_LOCKER,
       OB_LIGHT, OB_DETECTOR, OB_CCTV, OB_BIN,
       OB_COUNT };

typedef struct {
    const char   *name;
    short         cost;          // $ on delivery
    unsigned char w, h;          // footprint in tiles (1..2)
    unsigned char sprite;        // first sprite slot; multi-tile uses +1 across, +8 down
    unsigned char serves;        // ND_* it satisfies, or ND_COUNT for none
    unsigned char slots;         // capacity this object contributes to its room
    unsigned char solid;         // 1 = blocks walking (still usable from adjacent)
    unsigned char staff_only;    // 1 = prisoners never target it
} ObjDef;

// ── rooms ───────────────────────────────────────────────────────────────────
enum { RM_NONE = 0, RM_CELL, RM_DORM, RM_CANTEEN, RM_KITCHEN, RM_SHOWER,
       RM_YARD, RM_COMMON, RM_SOLITARY, RM_OFFICE, RM_STAFFROOM,
       RM_INFIRMARY, RM_HOLDING, RM_STORE, RM_WORKSHOP, RM_VISIT,
       RM_COUNT };

typedef struct {
    const char   *name;
    unsigned char enclosed;      // 1 = must not leak to outdoors (yard = 0)
    unsigned char req[4];        // required OB_*, 0-terminated
    unsigned char cap_obj;       // the OB_* whose `slots` set this room's capacity
    unsigned char colour;        // CLR_* for the room overlay + designation tint
    unsigned char min_area;      // tiles
    unsigned char prisoner_ok;   // 1 = prisoners may be sent here
} RoomDef;

typedef struct {
    unsigned char  type;         // RM_*
    unsigned char  valid;        // 1 = enclosed + has every required object
    unsigned char  missing;      // OB_* that's absent (0 if none) — the UI reason
    unsigned char  leaks;        // 1 = fill escaped outdoors
    unsigned short area;
    unsigned short cap, used;    // capacity slots / currently claimed
    short          cx, cy;       // centroid tile, for labels + as a goal
    short          seed_tile;    // a tile known to belong to this room
} Room;
extern Room lk_room[LK_MAXROOM];                         // owner: grid
extern int  lk_nroom;                                    // owner: grid

// ── construction / demolition jobs ──────────────────────────────────────────
enum { JB_NONE = 0, JB_FLOOR, JB_WALL, JB_DOOR, JB_OBJECT, JB_DEMOLISH,
       JB_COUNT };

// ── deployment zones ────────────────────────────────────────────────────────
enum { ZN_OPEN = 0, ZN_STAFF, ZN_SECURE, ZN_COUNT };

// ── a tile ──────────────────────────────────────────────────────────────────
typedef struct {
    unsigned char  floor;        // FL_*
    unsigned char  wall;         // WL_*
    unsigned char  door;         // DR_*
    unsigned char  door_open;    // 0..255 animation / open state
    unsigned char  locked;       // 1 = jail door locked (lockdown, or by zone)
    unsigned char  joins;        // wall neighbour mask: bit0 N,1 E,2 S,3 W, 4 NE,5 SE,6 SW,7 NW
    unsigned char  obj;          // OB_*  (origin tile of the object)
    unsigned char  obj_ref;      // 1 = covered by a multi-tile object whose origin is elsewhere
    unsigned char  obj_dir;      // 0..3 facing
    unsigned char  obj_used;     // 1 = an actor currently occupies this object
    unsigned short room;         // room id (index into lk_room), 0 = none
    unsigned char  paint;        // RM_* the player painted here (intent, pre-validation)
    unsigned char  zone;         // ZN_*
    unsigned char  job;          // JB_*
    unsigned char  job_arg;      // material / OB_* for the job
    unsigned char  claimed;      // a workman owns this job
    unsigned char  var;          // cosmetic variation seed — NEVER gameplay
    unsigned char  dirty;        // filth / blood 0..255
    unsigned char  light;        // baked light 0..255 (owner: art)
    float          work;         // job work remaining, seconds
} Tile;
extern Tile lk_t[LK_N];                                  // owner: grid

// ── needs ───────────────────────────────────────────────────────────────────
enum { ND_SLEEP = 0, ND_FOOD, ND_BLADDER, ND_HYGIENE, ND_REC,
       ND_FAMILY, ND_COMFORT, ND_SAFETY, ND_PRIVACY, ND_COUNT };

typedef struct {
    const char   *name;
    const char   *abbr;          // 4 chars max, for the compact bars
    float         decay;         // per in-game hour, 0..1 scale
    float         fill;          // per second of use
    unsigned char room;          // RM_* that serves it (RM_NONE = not room-bound)
    unsigned char obj;           // OB_* that serves it (OB_NONE = not object-bound)
    unsigned char colour;        // CLR_* for bars/heatmap
    unsigned char weight;        // contribution to volatility when unmet
} NeedDef;

// ── the regime ──────────────────────────────────────────────────────────────
enum { AC_SLEEP = 0, AC_EAT, AC_YARD, AC_SHOWER, AC_FREE, AC_WORK,
       AC_LOCKUP, AC_COUNT };
typedef struct { const char *name; const char *abbr; unsigned char colour; } ActDef;
extern unsigned char lk_regime[24];                      // owner: actors — hour → AC_*

// ── time ────────────────────────────────────────────────────────────────────
#define LK_HOUR_SECS  25.0f       // real seconds per in-game hour at speed 1
                                  // → a full day is 10 real minutes

// ── actors ──────────────────────────────────────────────────────────────────
enum { RL_PRISONER = 0, RL_GUARD, RL_WORKMAN, RL_COOK, RL_DOCTOR, RL_COUNT };
enum { SEC_MIN = 0, SEC_NORM, SEC_MAX, SEC_COUNT };

enum { AS_IDLE = 0, AS_WALK, AS_USE, AS_SLEEP, AS_EAT, AS_WASH,
       AS_ESCORTED, AS_ESCORTING, AS_FIGHT, AS_DOWN, AS_RESTRAINED,
       AS_SOLITARY, AS_WORK, AS_PATROL, AS_COOK, AS_TREAT,
       AS_RIOT, AS_ESCAPE, AS_COUNT };

typedef struct {
    unsigned char alive;
    unsigned char role;          // RL_*
    unsigned char state;         // AS_*
    unsigned char sec;           // SEC_* (prisoners)
    float         x, y;          // world px, tile centre = tx*LK_TS + LK_TS/2
    float         face;          // radians, for the 4-way facing pick
    float         bob;           // walk-cycle phase
    float         need[ND_COUNT];// 0 = satisfied, 1 = desperate
    float         vol;           // volatility 0..1  — the integral of unmet need
    float         supp;          // suppression 0..1 — guard presence damping vol
    float         health;        // 0..1
    short         cell;          // assigned cell room id, -1 none (prisoners)
    short         target;        // goal tile index, -1 none
    short         use_tile;      // tile of the object being used, -1 none
    short         escort;        // partner actor index, -1 none
    short         path[LK_MAXPATH];
    short         plen, pi;
    float         t;             // state timer
    float         repath;        // cooldown before another path attempt
    unsigned char contraband;    // bitmask of CB_*
    unsigned char grudge[4];     // actor ids this one hates
    unsigned char tint;          // pal() uniform/skin variant
    unsigned char skin, hair;
    unsigned char sector;        // guard deployment sector
    short         id;
} Actor;
extern Actor lk_a[LK_MAXACT];                            // owner: actors
extern int    lk_nact;                                   // owner: actors

enum { CB_NONE = 0, CB_WEAPON = 1, CB_DRUGS = 2, CB_TOOL = 4, CB_PHONE = 8 };

// ── alarm level ─────────────────────────────────────────────────────────────
enum { AL_CALM = 0, AL_INCIDENT, AL_RIOT, AL_LOCKDOWN, AL_COUNT };

// ── global sim state ────────────────────────────────────────────────────────
typedef struct {
    float clock;                 // 0..24 in-game hours
    int   day;
    int   hour;                  // (int)clock, cached
    int   money;
    int   speed;                 // 0 = paused, 1, 2, 4
    float tension;               // 0..1 — the score's single input
    int   alarm;                 // AL_*
    float alarm_t;
    int   n_prisoners, n_staff;
    int   n_beds, n_cells;
    int   incidents, deaths, escapes;
    int   grade[5];              // safety / hygiene / food / rec / reform, 0..5
    int   seed;
    int   over;                  // 0 running, 1 bankrupt, 2 lost control, 3 endured
    float intake_t;              // countdown to the next prisoner bus
} Sim;
extern Sim lk;                                           // owner: cart

// ═══ FROZEN TABLES ══════════════════════════════════════════════════════════
// Defined here, not in a module, precisely so no two modules can disagree about
// what a cell requires or what a bed costs. Read them; never shadow them.

//                     name             cost  w  h  spr  serves        slots solid staff
static const ObjDef LK_OBJ[OB_COUNT] = {
    { "nothing",         0, 1, 1,  0, ND_COUNT,   0, 0, 0 },
    { "bed",           100, 1, 2, 24, ND_SLEEP,   1, 1, 0 },
    { "bunk bed",      150, 1, 1, 54, ND_SLEEP,   2, 1, 0 },
    { "toilet",        100, 1, 1, 25, ND_BLADDER, 1, 1, 0 },
    { "sink",          100, 1, 1, 26, ND_HYGIENE, 1, 1, 0 },
    { "shower head",   200, 1, 1, 27, ND_HYGIENE, 1, 0, 0 },
    { "table",         150, 2, 1, 40, ND_COMFORT, 0, 1, 0 },
    { "bench",         100, 1, 1, 28, ND_COMFORT, 2, 1, 0 },
    { "serving table", 200, 1, 1, 29, ND_FOOD,    4, 1, 0 },
    { "cooker",        300, 1, 1, 30, ND_COUNT,   0, 1, 1 },
    { "fridge",        300, 1, 1, 31, ND_COUNT,   0, 1, 1 },
    { "desk",          200, 1, 1, 33, ND_COUNT,   0, 1, 1 },
    { "chair",          50, 1, 1, 34, ND_COMFORT, 1, 0, 0 },
    { "cabinet",       150, 1, 1, 35, ND_COUNT,   0, 1, 1 },
    { "television",    200, 1, 1, 36, ND_REC,     4, 1, 0 },
    { "pool table",    600, 2, 2, 42, ND_REC,     2, 1, 0 },
    { "phone",         150, 1, 1, 37, ND_FAMILY,  1, 0, 0 },
    { "weights",       300, 1, 1, 38, ND_REC,     1, 1, 0 },
    { "bookshelf",     200, 1, 1, 39, ND_REC,     1, 1, 0 },
    { "medical bed",   400, 1, 1, 44, ND_COUNT,   1, 1, 1 },
    { "locker",        100, 1, 1, 45, ND_COUNT,   0, 1, 1 },
    { "light",          50, 1, 1, 46, ND_COUNT,   0, 0, 0 },
    { "metal detector",400, 1, 1, 47, ND_COUNT,   0, 0, 0 },
    { "cctv camera",   300, 1, 1, 52, ND_COUNT,   0, 0, 0 },
    { "bin",            50, 1, 1, 53, ND_COUNT,   0, 1, 0 },
};

//                       name           encl  required objects                      cap_obj        colour            min  pris
static const RoomDef LK_ROOM[RM_COUNT] = {
    { "unassigned",   0, { 0, 0, 0, 0 },                          OB_NONE,       CLR_BLACK,        0, 0 },
    { "cell",         1, { OB_BED, OB_TOILET, 0, 0 },             OB_BED,        CLR_BLUE,         4, 1 },
    { "dormitory",    1, { OB_BED, OB_TOILET, 0, 0 },             OB_BED,        CLR_DARK_BLUE,   12, 1 },
    { "canteen",      1, { OB_TABLE, OB_BENCH, OB_SERVING, 0 },   OB_BENCH,      CLR_ORANGE,      12, 1 },
    { "kitchen",      1, { OB_COOKER, OB_FRIDGE, OB_SINK, 0 },    OB_COOKER,     CLR_YELLOW,       9, 0 },
    { "shower",       1, { OB_SHOWERHEAD, 0, 0, 0 },              OB_SHOWERHEAD, CLR_BLUE,         4, 1 },
    { "yard",         0, { 0, 0, 0, 0 },                          OB_NONE,       CLR_GREEN,       20, 1 },
    { "common room",  1, { OB_TV, 0, 0, 0 },                      OB_TV,         CLR_LIME_GREEN,   9, 1 },
    { "solitary",     1, { OB_TOILET, 0, 0, 0 },                  OB_TOILET,     CLR_DARK_RED,     2, 1 },
    { "office",       1, { OB_DESK, OB_CHAIR, 0, 0 },             OB_DESK,       CLR_INDIGO,       4, 0 },
    { "staff room",   1, { OB_CHAIR, 0, 0, 0 },                   OB_CHAIR,      CLR_MEDIUM_GREY,  6, 0 },
    { "infirmary",    1, { OB_MEDBED, 0, 0, 0 },                  OB_MEDBED,     CLR_WHITE,        6, 1 },
    { "holding cell", 1, { OB_BENCH, 0, 0, 0 },                   OB_BENCH,      CLR_BROWN,        9, 1 },
    { "storeroom",    1, { 0, 0, 0, 0 },                          OB_NONE,       CLR_DARK_BROWN,   4, 0 },
    { "workshop",     1, { OB_TABLE, 0, 0, 0 },                   OB_TABLE,      CLR_DARK_ORANGE, 12, 1 },
    { "visitation",   1, { OB_TABLE, OB_CHAIR, 0, 0 },            OB_TABLE,      CLR_PINK,         9, 1 },
};

// decay is per IN-GAME HOUR, fill is per REAL SECOND of use.
//                      name         abbr    decay  fill   room          object          colour           wt
static const NeedDef LK_NEED[ND_COUNT] = {
    { "Sleep",      "SLP", 0.055f, 0.006f, RM_CELL,    OB_BED,        CLR_DARK_BLUE,    2 },
    { "Food",       "FOOD", 0.090f, 0.050f, RM_CANTEEN, OB_SERVING,    CLR_ORANGE,       3 },
    { "Bladder",    "BLAD", 0.140f, 0.120f, RM_NONE,    OB_TOILET,     CLR_LIME_GREEN,   2 },
    { "Hygiene",    "HYG",  0.050f, 0.080f, RM_SHOWER,  OB_SHOWERHEAD, CLR_BLUE,         1 },
    { "Recreation", "REC",  0.060f, 0.020f, RM_YARD,    OB_NONE,       CLR_GREEN,        2 },
    { "Family",     "FAM",  0.030f, 0.040f, RM_COMMON,  OB_PHONE,      CLR_PINK,         1 },
    { "Comfort",    "CMFT", 0.080f, 0.030f, RM_NONE,    OB_BENCH,      CLR_MEDIUM_GREY,  1 },
    { "Safety",     "SAFE", 0.000f, 0.050f, RM_NONE,    OB_NONE,       CLR_RED,          3 },
    { "Privacy",    "PRIV", 0.020f, 0.010f, RM_CELL,    OB_NONE,       CLR_INDIGO,       1 },
};

static const ActDef LK_ACT[AC_COUNT] = {
    { "Sleep",     "SLP", CLR_DARK_BLUE   },
    { "Eat",       "EAT", CLR_ORANGE      },
    { "Yard",      "YRD", CLR_GREEN       },
    { "Shower",    "SHW", CLR_BLUE        },
    { "Free Time", "FRE", CLR_LIME_GREEN  },
    { "Work",      "WRK", CLR_DARK_ORANGE },
    { "Lockup",    "LCK", CLR_DARK_RED    },
};

static const char *const LK_ROLE_NAME[RL_COUNT] = {
    "Prisoner", "Guard", "Workman", "Cook", "Doctor"
};
// uniform / trouser colour per role, fed to pal(LK_MAGIC_UNIFORM, …)
static const unsigned char LK_ROLE_UNIFORM[RL_COUNT] = {
    CLR_ORANGE, CLR_TRUE_BLUE, CLR_BROWN, CLR_WHITE, CLR_LIGHT_GREY
};
static const unsigned char LK_ROLE_TROUSER[RL_COUNT] = {
    CLR_DARK_ORANGE, CLR_DARK_BLUE, CLR_DARK_BROWN, CLR_LIGHT_GREY, CLR_BLUE
};
static const short LK_ROLE_WAGE[RL_COUNT] = { 0, 100, 80, 90, 200 };

static const char *const LK_WALL_NAME[WL_COUNT] = {
    "none", "brick", "concrete", "fence", "perimeter"
};
static const short LK_WALL_COST[WL_COUNT] = { 0, 20, 30, 10, 60 };
static const char *const LK_DOOR_NAME[DR_COUNT] = {
    "none", "door", "jail door", "staff door", "gate"
};
static const short LK_DOOR_COST[DR_COUNT] = { 0, 50, 150, 120, 300 };

// ═══ MODULE: grid  (runtime/lockup/grid.h) ══════════════════════════════════
void  lk_grid_init(int seed);
static inline int  lk_idx(int x, int y) { return y * LK_MW + x; }
static inline int  lk_tx(int c) { return c % LK_MW; }
static inline int  lk_ty(int c) { return c / LK_MW; }
static inline bool lk_in(int x, int y) { return x >= 0 && y >= 0 && x < LK_MW && y < LK_MH; }
static inline int  lk_cx(int c) { return lk_tx(c) * LK_TS + LK_TS / 2; }   // world px centre
static inline int  lk_cy(int c) { return lk_ty(c) * LK_TS + LK_TS / 2; }
int   lk_cell_at(float wx, float wy);          // world px → tile index (clamped)

bool  lk_solid(int c);                         // wall or solid object — no door logic
bool  lk_can_pass(int c, int role);            // walkable AND permitted for this role
bool  lk_indoors(int c);

void  lk_set_floor(int c, int mat);
void  lk_set_wall(int c, int mat);
void  lk_set_door(int c, int kind);
bool  lk_place_obj(int c, int ob, int dir);    // false if it doesn't fit
void  lk_clear_tile(int c);                    // remove wall/door/object, keep floor
void  lk_joins_refresh(int c);                 // recompute this tile + 8 neighbours

void  lk_paint_room(int c, int type);          // set intent; 0 clears
void  lk_rooms_rebuild(void);                  // full re-derivation from paint+walls
void  lk_rooms_touch(int c);                   // incremental: re-fill the affected room
int   lk_room_of(int c);                        // room id at tile, 0 = none
int   lk_room_find(int type, int nth);         // nth valid room of a type, -1 none
int   lk_room_free_slot(int rid, int need);    // a tile holding a FREE serving object
void  lk_room_release(int c);                  // mark an object's slot free again

void  lk_queue(int c, int job, int arg);       // designate construction
void  lk_unqueue(int c);
int   lk_job_claim(int from_c, int role);      // nearest unclaimed job tile, -1 none
void  lk_job_progress(int c, float work);      // advance; completes + applies itself
int   lk_job_count(void);

void  lk_grid_update(float d);                 // door animation, filth spread
extern int lk_dirty_struct;                    // owner: grid — set on any structural
                                               // change; path module clears it

// ═══ MODULE: path  (runtime/lockup/path.h) ══════════════════════════════════
enum { FF_TOILET = 0, FF_BED, FF_SHOWER, FF_BENCH, FF_PHONE, FF_YARD,
       FF_TV, FF_SERVING, FF_SOLITARY, FF_EXIT, FF_COUNT };

void  lk_path_init(void);
void  lk_path_update(void);                    // once per frame: deferred relabel +
                                               // at most one flow-field rebuild
int   lk_region(int c, int role);              // connected component id, -1 if solid
bool  lk_reachable(int from, int to, int role);// O(1) via component labels
int   lk_find_path(int from, int to, int role, short *out, int cap);   // heap A*
int   lk_nearest(int from, int kind, int role);// flow-field query → tile index, -1 none
int   lk_step_cost(int c, int role);           // 1 normal, higher through doors/crowd

// ═══ MODULE: actors  (runtime/lockup/actors.h) ══════════════════════════════
void  lk_actors_init(void);
int   lk_spawn(int role, int c);               // → actor index, -1 if full
void  lk_actors_update(float d);               // the whole sim step for every actor
void  lk_intake(int n);                        // a bus of n prisoners arrives
int   lk_assign_cell(int ai);                  // find + claim a bed, -1 none
void  lk_actor_release(int ai);                // free whatever slot it holds
float lk_block_tension(int c, int radius);     // local unrest, for the score + heatmap
void  lk_set_alarm(int level);
void  lk_shakedown(void);                      // player-ordered contraband search
int   lk_actor_at(float wx, float wy, int slack);   // pick for the inspector, -1 none
const char *lk_state_name(int state);
const char *lk_need_worst(int ai);             // name of this actor's worst need

// ═══ MODULE: econ  (runtime/lockup/econ.h) ══════════════════════════════════
void  lk_econ_init(void);
void  lk_econ_update(float d);
void  lk_spend(int amount, const char *why);
void  lk_earn(int amount, const char *why);
void  lk_econ_day_end(void);                   // wages, fees, grants, grading
void  lk_grade_recalc(void);
int   lk_valuation(void);
const char *lk_econ_line(int i, int *amount);  // last-day ledger, i < lk_econ_lines()
int   lk_econ_lines(void);

// ═══ MODULE: art  (runtime/lockup/art.h) ════════════════════════════════════
enum { OV_NONE = 0, OV_ROOMS, OV_DEPLOY, OV_NEEDS, OV_SAFETY, OV_COUNT };

void  lk_art_init(void);
void  lk_art_update(float d);                  // light bake, weather, particles
void  lk_art_world(int camx, int camy, int vw, int vh);   // floors, walls, objects
void  lk_art_actors(int camx, int camy, int vw, int vh);
void  lk_art_overlay(int mode, int camx, int camy, int vw, int vh);
void  lk_art_fx(int camx, int camy);           // particles, blood, dust, rain
void  lk_puff(float wx, float wy, float size, float life, int colour);
void  lk_art_ghost(int c, int job, int arg, bool ok);     // build-tool preview
float lk_daylight(void);                       // 0 = midnight .. 1 = noon

// ═══ MODULE: score  (runtime/lockup/score.h) ════════════════════════════════
enum { SFX_CLICK = 0, SFX_DENY, SFX_PLACE, SFX_BUILD, SFX_DOOR, SFX_LOCK,
       SFX_ALARM, SFX_WHISTLE, SFX_FIGHT, SFX_MEAL, SFX_CASH, SFX_BUS,
       SFX_COUNT };

void  lk_score_init(void);
void  lk_score_update(float d);                // owns ALL music; reads lk.tension
void  lk_sfx(int kind);
void  lk_sfx_at(int kind, float wx, float wy); // panned/attenuated by camera
const char *lk_score_layer_name(int i);        // for the debug readout
int   lk_score_layers(void);

// ═══ MODULE: hud  (runtime/lockup/hud.h) ════════════════════════════════════
enum { TL_SELECT = 0, TL_FLOOR, TL_WALL, TL_DOOR, TL_OBJECT, TL_ROOM,
       TL_ZONE, TL_DEMOLISH, TL_COUNT };

void  lk_hud_init(void);
void  lk_hud_update(float d);                  // input, tools, camera, panels
void  lk_hud_draw(void);                       // chrome only — the map is drawn by art
void  lk_hud_toast(const char *msg);
void  lk_hud_viewport(int *x, int *y, int *w, int *h);    // the world view rect

extern int lk_tool, lk_tool_arg;               // owner: hud
extern int lk_overlay;                         // owner: hud — OV_*
extern int lk_cam_x, lk_cam_y;                 // owner: hud — world px, top-left
extern int lk_sel_actor;                       // owner: hud — inspector target, -1 none
extern int lk_sel_room;                        // owner: hud

#endif // LOCKUP_MODEL_H
