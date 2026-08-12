// ─────────────────────────────────────────────────────────────────────────────
// tenement/model.h — THE CONTRACT for the TENEMENT sim (tools/carts/tenement.c)
//
// Design: docs/design/tenement.md.  The honest core: the building is a market for
// attention and space is the scarce good. The player shapes space and never
// touches a person.
//
// This file declares EVERY shared type, table, global and function. The modules
// each implement one slice and include ONLY this file plus engine headers, never
// each other. One translation unit: the cart includes each module header once.
//
// ── THE ONE PRINCIPLE ───────────────────────────────────────────────────────
//   NOTHING ENUMERATES INSTANCES. EVERYTHING DECLARES PROPERTIES AND MATCHES
//   ON TAGS.
//
// This is not style, it is the whole architecture, and it exists because the
// sibling project navkit made the same mistake three times (design doc §3):
//   • workshops enumerated PLACES and gave each one recipes  -> nine templates,
//     one work tile each, and no way to combine two tools
//   • stockpile filters enumerated ITEM TYPES and gave each a checkbox -> 30
//     rows and 34 of 36 keyboard keys gone
//   • containers nearly became a special entity, instead of "an item with
//     capacity"
// So, concretely, and these are the rules that get reviewed:
//   1. NO PLACE EVER OWNS A RECIPE. Recipes live in TN_RECIPES and declare the
//      capabilities they need. A room, a spot, a building is only ever a
//      collection of things that provide capabilities.
//   2. NO SYSTEM SWITCHES ON A CONCRETE OBJECT KIND. If you write
//      `if (obj->kind == TN_OBJ_FRIDGE)` you have broken the contract. Ask what
//      it OFFERS.
//   3. A NEW OBJECT / RECIPE / ITEM IS A TABLE ROW. If it needs a code path,
//      the tag vocabulary is wrong. Fix the vocabulary, not the caller.
//
// ── RULES FOR MODULE AUTHORS ────────────────────────────────────────────────
//  1. Every global declared `extern` here is DEFINED in exactly one module (the
//     owner is named in the comment). Never define one you don't own.
//  2. Internal helpers are `static` AND prefixed with your module's tag, so two
//     modules can't collide in the shared TU:
//        world  → tnw_    offer  → tno_    agents → tna_
//        work   → tnk_    econ   → tne_    store  → tns_
//        art    → tnr_    hud    → tnh_    cart   → tnc_
//     Public functions keep the `tn_` prefix used below.
//  3. Never rename or re-type anything here without saying so. It is FROZEN for
//     the duration of parallel construction. Need something added? Add it at the
//     bottom of your own declaration block and flag it.
//  4. No engine-namespace collisions: `map`, `line`, `rect`, `circ`, `print`,
//     `spr`, `timer`, `now`, `paused`, `hud`, `SCALE`, `SCREEN_W/H` are all
//     taken by studio.h. (`line` and `hud` both bit isoroom.)
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_MODEL_H
#define TENEMENT_MODEL_H

#include "studio.h"

// NOTE: this contract does NOT include a baked atlas header. The geometry below is
// the settled truth; the CELLS are art, and only the art module includes the
// generated `tenement/atlas.h` (produced by tools/voxel-bake.js from this game's
// own tools/voxel-models/). Coupling the contract to isoroom's generated header
// would tie the game to a probe cart's art, which is backwards.

// ── geometry: inherited from the SHIPPED probe, do not re-derive ────────────
// docs/design/iso-rooms.md §8 settled all of this and measured it at ~0.68ms
// on a Mac. The constraints that bite:
//   • FOUR rotations, all 45-degree diagonals. The cardinal four were built,
//     looked at, and CUT (no X/Y mixing means no depth cue).
//   • ISO_TW must stay a multiple of 4 or the 2:1 diamond leaves the pixel
//     grid. Scale is set by VOXEL COUNT, so changing it re-authors every model
//     in tools/voxel-models/. It is not a knob.
//   • Objects read by SILHOUETTE only. Detail is invisible at this size.
//   • A turned non-square object's footprint does NOT follow its art. Walls
//     dodge this with per-orientation models; anything the player can rotate
//     must solve it.
#define TN_TILE_VOX   6                   // voxels per floor tile
#define TN_TW         4                   // px across one voxel. MUST be a multiple of 4.
#define TN_TH         2                   // == TN_TW/2, the 2:1 diamond
#define TN_ZH         2                   // px per voxel of height
#define TN_ROTS       4                   // the four 45-degree diagonals, and only those
#define TN_TILE_PX    (TN_TILE_VOX * TN_TW / 2)   // 24 px across one floor tile
#define TN_MW         32                  // building width  in tiles
#define TN_MH         24                  // building depth  in tiles
#define TN_N          (TN_MW * TN_MH)

#define TN_MAX_AGENTS      24
#define TN_MAX_HOUSEHOLDS   8
#define TN_MAX_OBJECTS    192
#define TN_MAX_ITEMS      256
#define TN_MAX_ORDERS      64
#define TN_MAX_PATH       128
#define TN_MAX_OFFERS       6             // offers one object may declare

// ── TnIdx: the type of EVERY index-or-none field ────────────────────────────
// These were `signed char` and that was a silent corruption bug, found by the `store` agent: the
// arrays run to 256 and 192, so index 128 and up wrap NEGATIVE, and negative already means "none"
// (loose on the floor / empty-handed / unclaimed). No crash, no warning, just an item that
// teleports. It is the third width mistake in this contract after `minutes` as unsigned char, so
// it gets a TYPE and an assertion rather than four separate fixes.
typedef short TnIdx;
#define TN_NONE ((TnIdx)-1)
_Static_assert(TN_MAX_ITEMS   <= 32767 && TN_MAX_OBJECTS <= 32767 &&
               TN_MAX_AGENTS  <= 32767 && TN_MAX_ORDERS  <= 32767,
               "an array outgrew TnIdx: widen TnIdx before raising the bound");

// Pins the geometry the probe settled, so a later "let's just try tw=6" stops
// compiling instead of quietly leaving the pixel grid (iso-rooms.md §8).
_Static_assert(TN_TW % 4 == 0,  "TN_TW must be a multiple of 4 or the 2:1 diamond aliases");
_Static_assert(TN_TH * 2 == TN_TW, "the diamond must stay exactly 2:1");

// ── CART CONFIG (mirror in tools/carts/tenement.cart.js — keep in sync) ─────
//   screenW 320, screenH 200, scale 4      (the probe's canvas, unchanged)

// ─────────────────────────────────────────────────────────────────────────────
// TAGS — the matching vocabulary. THE central table of the whole design.
//
// A tag is a thing an object OFFERS or a consumer REQUIRES. Needs, capabilities
// and storage classes all live in ONE namespace on purpose: it means the offer
// index has exactly one shape and one lookup, and that a fridge advertising
// both TN_SERVE_HUNGER and TN_STORE_FOOD needs no special case anywhere.
//
// ADDING A TAG IS THE NORMAL WAY TO EXTEND THIS GAME. Adding a code path is not.
// ─────────────────────────────────────────────────────────────────────────────
typedef enum {
    // ── needs an object can serve (seam 1: needs are DATA) ──
    TN_SERVE_HUNGER = 0,
    TN_SERVE_REST,
    TN_SERVE_HYGIENE,
    TN_SERVE_BLADDER,
    TN_SERVE_FUN,
    TN_SERVE_COUNT,                       // ← needs are exactly [0, TN_SERVE_COUNT)

    // ── capabilities a workspot can provide (design §3, the navkit fix) ──
    // A recipe requires capabilities. It NEVER names a workshop.
    TN_CAP_WORK,                          // a bare spot: hand-work, Tier 1
    TN_CAP_HEAT,                          // Tier 2+: a stove, a kiln
    TN_CAP_CUT,
    TN_CAP_POWER,                         // Tier 3: reserved, nothing provides it in v1

    // ── storage classes (design §6: storage is an OFFER, not a system) ──
    TN_STORE_FOOD,
    TN_STORE_GOODS,
    TN_STORE_CLOTHES,

    TN_TAG_COUNT
} TnTag;

// Needs are the leading run of the tag enum, so a need index IS a tag. Asserted
// rather than commented, because the day someone inserts a tag above
// TN_SERVE_HUNGER this stops compiling instead of silently mis-indexing.
#define TN_NEED_COUNT TN_SERVE_COUNT
_Static_assert(TN_SERVE_HUNGER == 0, "needs must be the leading run of TnTag");

// ── one OFFER: what an object gives, how well, at what cost ─────────────────
// `strength` and `cost` are what make the agent's choice an ARGMAX over numbers
// rather than a hand-written priority list, which is what makes it oracle-able.
typedef struct {
    unsigned char tag;                    // TnTag
    signed   char strength;               // need points restored, or -1 for a capability
    short         minutes;                // how long using it takes. SHORT, not char:
                                          // a night's sleep is 480 minutes and would
                                          // silently wrap to 224 in a byte. The compiler
                                          // caught this before anyone built against it.
    unsigned char capacity;               // simultaneous users; 1 = the queue-former
} TnOffer;

// ─────────────────────────────────────────────────────────────────────────────
// OBJECTS — furniture, machines, storage. All the same struct.
//
// TnObjKind exists ONLY to pick the sprite and the offer row. NOTHING may
// branch on it (contract rule 2). It is art and data, not behaviour.
// ─────────────────────────────────────────────────────────────────────────────
typedef enum { TN_OBJ_BED = 0, TN_OBJ_FRIDGE, TN_OBJ_COUNTER, TN_OBJ_TOILET,
               TN_OBJ_SOFA, TN_OBJ_LOOM, TN_OBJ_WARDROBE, TN_OBJ_KIND_COUNT } TnObjKind;

typedef struct {
    unsigned char kind;                   // TnObjKind: sprite + offer lookup ONLY
    unsigned char tx, ty;                 // tile position of the footprint origin
    unsigned char facing;                 // quarter turns; see the geometry warning
    signed   char household;              // owner, or -1 for communal (design §6)
    unsigned char users;                   // in use right now, vs offer capacity
} TnObject;

// The offer table: kind → what it offers. THE data table of the game.
// Owner: offer module. Defined once, in tno_*.c.
extern const TnOffer TN_OFFERS[TN_OBJ_KIND_COUNT][TN_MAX_OFFERS];
extern const unsigned char TN_OFFER_N[TN_OBJ_KIND_COUNT];
extern const short TN_OBJ_PRICE[TN_OBJ_KIND_COUNT];   // money sink (design §5)

// ── ITEMS — a good, a meal, a bolt of cloth. Tagged, never enumerated. ──────
// Storage accepts a TAG SET, so a new item with TN_STORE_FOOD is accepted by
// every food store already placed. That is the navkit filter fix (design §3b):
// no per-item-type checkbox, ever.
typedef struct {
    unsigned char store_tag;              // which class of storage accepts it
    unsigned char value;                  // what it sells for at TN_SEAM_EXTERNAL
    TnIdx         held_by;                // agent index, or TN_NONE
    TnIdx         stored_in;              // object index, or TN_NONE
    unsigned char tx, ty;                 // valid only when loose on the floor
} TnItem;

// ─────────────────────────────────────────────────────────────────────────────
// AGENTS — residents. Dumb by design: they take the best offer going.
// ─────────────────────────────────────────────────────────────────────────────
typedef enum { TN_SPECIES_ADULT = 0, TN_SPECIES_COUNT } TnSpecies;   // seam 3

typedef enum {
    TN_ACT_IDLE = 0, TN_ACT_WALK, TN_ACT_USE, TN_ACT_WORK, TN_ACT_HAUL,
    TN_ACT_OFF_LOT                        // seam 4 — see the note below
} TnActivity;

typedef struct {
    unsigned char species;                // TnSpecies (seam 3)
    unsigned char household;
    unsigned char need[TN_NEED_COUNT];    // 0 = desperate, 255 = sated (seam 1)
    unsigned char activity;               // TnActivity
    TnIdx         target_obj;             // what it is walking to / using, or TN_NONE
    TnIdx         carrying;               // item index, or TN_NONE
    short         tx, ty;                 // tile position
    short         until;                  // minute-of-day the current activity ends
    short         return_at;              // OFF_LOT only: minute-of-day it returns
    unsigned char facing;                 // 0..3, indexes the same baked ring
    // The bid this agent last WON. Lives here rather than in a module-private array because two
    // modules need it: `agents` writes it, `hud` draws it. Showing the winning bid is a design
    // feature (design §1: the interesting half of this sim is invisible), not debug output.
    unsigned char bid_tag;                // TnTag, or TN_SERVE_COUNT for "nothing on offer"
    int           bid_score;
} TnAgent;

// OFF_LOT IS A PLACEHOLDER, NOT THE ECONOMY (design §5). It is the degenerate
// case: work whose capabilities are not yet modelled inside the building. As
// capabilities are added, work migrates on-lot and traffic here shrinks. The
// architecture succeeds when this state can be DELETED without anything else
// moving. Never make it the primary way money arrives.

// ── HOUSEHOLDS — first-class, with their own purse (seam 5) ─────────────────
typedef struct {
    short  money;
    unsigned char members[6], member_n;
    unsigned char rent;                   // per rent day
    unsigned char dwelling;                // room id
} TnHousehold;

// ── TIME — a calendar, not a clock (seam 6) ─────────────────────────────────
typedef struct { short minute; short day; } TnClock;   // minute 0..1439

// ─────────────────────────────────────────────────────────────────────────────
// WORK — recipes require CAPABILITIES. This is the navkit lesson, in a struct.
//
// A recipe NEVER names a place. The work module scans for a spot whose offers
// satisfy `needs_cap`, which is what lets a workspot be a bare floor tile now
// and a powered machine later with no change here.
//
// v1 recipes have NO input (`in_n == 0`): a dumb machine turns TIME into a
// GOOD. That is an open loop by the autarky gate and it is MARKED as such, not
// hidden. Giving a recipe an input closes it and changes no structure.
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    unsigned char needs_cap;              // TnTag capability required
    unsigned char in_n;                   // v1: 0. Non-zero closes the loop.
    unsigned char in_store_tag;           // which item class it consumes
    unsigned char out_store_tag;          // what it produces
    unsigned char out_value;
    short         minutes;                // SHORT: an 8-hour shift is 480 (see TnOffer)
} TnRecipe;

extern const TnRecipe TN_RECIPES[];       // owner: work module
extern const int      TN_RECIPE_N;

typedef struct {
    unsigned char recipe;
    TnIdx         claimed_by;             // agent index, or TN_NONE
    TnIdx         at_obj;                 // resolved workspot, or TN_NONE (DYNAMIC)
    unsigned char household;              // who gets paid
} TnOrder;

// ─────────────────────────────────────────────────────────────────────────────
// THE OFFER INDEX — the one lookup, three consumers (design §2)
// Owner: offer module.
// ─────────────────────────────────────────────────────────────────────────────

// ── THE CORE OF THE SIM, and read this before touching it ───────────────────
//
// ONE argmax over EVERY (object, need) pair at once. Not "pick the most urgent
// need, then find an object for it" — that is urgency-sort, it is what `sims`
// already does, and it is the thing this design claims to invert. The whole
// point of smart objects is that the agent never decides what it wants; it
// takes the single best offer on the table, and the deficit is one term in the
// score rather than a pre-filter.
//
// The difference is observable, which is what makes it worth the distinction: a
// sim with high hunger passes a free toilet to reach a distant fridge under
// urgency-sort, whereas here an adjacent nearly-free toilet can outbid the
// fridge. That is the behaviour people recognise as Sims-like.
//
// SCORING lives in ONE function so no module invents its own. Sketch:
//     score = deficit(need) * offer.strength / (travel + queue_penalty)
// Every term is a number, so the choice is oracle-able: given this building and
// these needs, the agent MUST pick X. That is the spec() this cart will carry.
//
// Returns the object index and writes which tag won, or -1 if nothing offers
// anything (a legitimate answer the caller must handle).
int   tn_best_action(int agent, TnTag *out_tag, int *out_score);

// Secondary: best object offering a KNOWN tag. For consumers that legitimately
// have one already — a work order needs a capability, an item needs a store —
// never for deciding what an agent wants. Use tn_best_action for that.
int   tn_best_offer(int agent, TnTag tag, int *out_score);

// Does this object offer `tag`, and how strongly? `strength` may be NULL.
bool  tn_offers(int obj, TnTag tag, int *strength);

// A workspot satisfying `cap`, nearest to `agent`. The recipe never names one.
int   tn_find_workspot(int agent, TnTag cap);

// Where can this item go? Nearest store accepting its tag that the household
// may use. -1 if nowhere, which is a legitimate answer the caller must handle.
int   tn_find_store(int agent, int item);

// THE ONE PLACE MONEY ENTERS THE WORLD (design §5). Named so it can be found
// and eventually narrowed. Every other money movement is a transfer.
#define TN_SEAM_EXTERNAL 1
void  tn_sell(int household, int item);

// ── spawning (owner: world). Public because spec() builds scenarios with them, and a
// scenario built by the same code the game uses cannot drift from the game. ──
int  tn_add_obj(int kind, int tx, int ty, int household);   // returns the index, or -1 if full
int  tn_add_agent(int household, int tx, int ty);

// One offer's score for one agent. Public so the HUD can show every bid an agent considered
// rather than only the winner, which is the whole legibility argument (design §1).
int  tn_score_offer(int agent, int obj, TnTag tag);

// ── module entry points ─────────────────────────────────────────────────────
void tn_world_init(void);                 // world
void tn_agents_tick(void);                // agents: decay, choose, act
void tn_work_tick(void);                  // work:   orders, claims, production
void tn_econ_tick(void);                  // econ:   rent, bills, purchases
void tn_store_tick(void);                 // store:  hauling, containers, ownership
void tn_camera(void);                     // art: recentre for the current rotation
void tn_draw_world(void);                 // art
void tn_draw_hud(void);                   // hud

// ── globals (each DEFINED in exactly one module) ────────────────────────────
// Every one of these is EXTERN. A bare `int tn_obj_n;` in a header is a
// DEFINITION, so it would multiply-define the moment two modules include this.
extern TnObject    tn_obj[TN_MAX_OBJECTS];      // owner: world
extern int         tn_obj_n;                    // owner: world
extern TnItem      tn_item[TN_MAX_ITEMS];       // owner: world
extern int         tn_item_n;                   // owner: world
extern TnAgent     tn_agent[TN_MAX_AGENTS];     // owner: agents
extern int         tn_agent_n;                  // owner: agents
extern TnHousehold tn_house[TN_MAX_HOUSEHOLDS]; // owner: econ
extern int         tn_house_n;                  // owner: econ
extern TnOrder     tn_order[TN_MAX_ORDERS];     // owner: work
extern int         tn_order_n;                  // owner: work
extern TnClock     tn_clock;                    // owner: world
// The building ACTUALLY in use, in tiles. TN_MW/TN_MH above are array bounds; these are the
// current extent, and they are variables rather than #defines because the `build` agent needs the
// player to be able to grow the place. Read by art (the camera and the floor) and by path.
extern int         tn_bw, tn_bh;                // owner: world
extern int         tn_rot;                      // owner: cart — 0..3 view rotation

#endif // TENEMENT_MODEL_H
