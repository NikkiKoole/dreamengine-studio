// ─────────────────────────────────────────────────────────────────────────────
// lockup/art.h — MODULE: art.  EVERYTHING THE PLAYER SEES OF THE PRISON.
// Design: docs/design/lockup.md §4 (the visual bar).  Contract: lockup/model.h.
//
// OWNS (defines) ─ nothing in the contract's global list.  The only shared state
//   this module WRITES is `Tile.light` (the contract names art its owner) plus
//   its own lkr_-tagged statics: the particle pool, the rain field, the warm-light
//   and dynamic-light buffers, and the cached view rect.
// READS ─ lk_t / lk_room / lk_nroom / lk_a / lk_nact / lk (clock, alarm, day) and,
//   through the contract only, lk_indoors / lk_room_of / lk_cell_at /
//   lk_block_tension / lk_tx / lk_ty.  Never writes a tile field but `light`.
//
// ── THE FIVE THINGS THAT MAKE IT LOOK BUILT ─────────────────────────────────
//
// 1. ONE LIGHT DIRECTION, NO EXCEPTIONS: north-west.  Every highlight sits on a
//    top/left edge, every shade on a bottom/right edge, and every cast shadow
//    steps LKR_SHADOW px to the south-east.  That single rule is what turns a
//    grid of coloured squares into architecture, so nothing here breaks it —
//    which is also why floor sprites are never flipped (a flip would mirror the
//    lighting the sprite art baked in) and why object facing is drawn as a floor
//    scuff rather than a mirrored sprite.
//
// 2. WALLS ARE DRAWN, NOT TILED.  lkr_wall_body() reads Tile.joins (bit0 N, 1 E,
//    2 S, 3 W, 4 NE, 5 SE, 6 SW, 7 NW) and paints a CONTINUOUS structure: an edge
//    highlight/shade appears only on a face with NO wall neighbour, so a straight
//    run has no internal seams, an end cap is bright on three sides, a tee is
//    bright on two, and an inner corner gets its own notch.  Brick courses and
//    concrete panel seams are indexed off WORLD coordinates, not tile-local ones,
//    so the coursing runs unbroken through a fifty-tile wall.  Materials differ in
//    temperature as well as pattern (brick warm, concrete cool, perimeter heavy
//    and near-black, fence see-through) so a player reads material at a glance.
//
// 3. THE SHADOW PASS IS THE WHOLE TRICK.  Between the floor pass and the wall
//    pass, every wall / door jamb / solid object stamps its own footprint offset
//    +3,+3 under blend(BLEND_AVG) — a 50 % darken.  The structure pass then
//    covers the un-offset part, leaving exactly the L-shaped hard shadow on the
//    south and east.  One blend scope, one rect per caster, and rooms suddenly
//    look like they have height.
//
// 4. NIGHT IS LIGHT, NOT A TINT.  lkr_light_bake() (4–5 Hz, never per frame)
//    writes Tile.light: an outdoor/indoor ambient from lk_daylight(), then a
//    BLOCKED breadth-first spill from every lamp-ish object — light does not pass
//    a brick wall or a shut door, but a fence and an open door let it through.
//    Guard torches are stamped per frame into a separate dynamic buffer so they
//    move without a rebake.  Drawing darkens at HALF-TILE resolution (a 4-tap
//    bilinear of the tile field) so a lamp pool has an organic edge instead of a
//    16-px staircase, and the darkness is a SIX-STOP ramp composed from four
//    SOLID blend(BLEND_MUL) washes (grey, indigo, indigo², indigo³), run-length
//    merged per row.  Solid, not dithered, because a dithered full-screen fill is
//    a per-pixel loop on the software canvas (i.e. on web).  Lamp warmth is a
//    separate dithered ADD pass, and it is small.  A lit block in a black yard
//    falls straight out of this.
//
// 5. STATE READS WITHOUT WORDS.  lk_art_actors() sorts by y, recolours each body
//    with at most six pal() swaps (uniform by role — and by security category for
//    prisoners — trousers, skin, hair, plus a night dim), then draws every state
//    cue in a SECOND pass with the palette already reset: cuffs and a tether for
//    an escort, an impact star for a fight, a jitter and a red ring for a riot, a
//    blood pool for a body on the floor, drifting z's for a sleeper, spray for a
//    shower, a health tick for the injured.
//
// ── IMPLEMENTATION NOTES ────────────────────────────────────────────────────
// * NEEDED FROM THE CART: call lk_art_init() once, lk_art_update(dt) once per
//   frame with the RAW real delta, then inside your camera() transform
//   lk_art_world → lk_art_actors → lk_art_fx → lk_art_overlay → lk_art_ghost.
//   lk_art_fx/ghost/overlay reuse the (camx,camy,vw,vh) the last lk_art_world
//   call was given, so lk_art_world must come first each frame.
// * The DARKNESS pass lives at the end of lk_art_world, so actors drawn afterwards
//   are not covered by it.  They are dimmed individually instead, through the same
//   pal() swap that colours them (LKR_DIM is a one-step-darker ramp over all 32
//   palette entries).  This is deliberate: it keeps people readable at night — a
//   Prison Architect habit — while the architecture genuinely goes dark.
// * COLORKEY is owned here: floors are drawn with colorkey(-1) (opaque, so a black
//   pixel in a floor sprite stays black) and everything else with
//   colorkey(CLR_BLACK), which is this repo's sprite convention.  The resting
//   state after a world pass is colorkey(CLR_BLACK) — the HUD can rely on it.
// * NO WINDOW OBJECT EXISTS in the frozen contract, so the design's "lit windows
//   spill onto the floor" is served by OB_LIGHT, plus a smaller warm spill from
//   OB_COOKER and a cool one from OB_TV, and by open doors leaking light between
//   rooms.  If an OB_WINDOW is ever added, register it in LKR_LAMP and it lights
//   itself — nothing else changes.
// * Tile.obj_dir IS drawn, but as a worn approach-scuff on the facing edge of the
//   floor, not as a mirrored sprite: mirroring would invert the north-west light
//   the sprite art bakes in, and the contract's own grid notes say footprints do
//   not rotate.  Flagged as a deliberate deviation from "honour obj_dir" by flip.
// * PRISONER UNIFORM: LK_ROLE_UNIFORM[RL_PRISONER] is the base, then security
//   category shifts it (min = yellow, normal = orange, max = dark orange) so a
//   max-sec prisoner is visible in a crowd.  Staff use the table verbatim.
// * lk_daylight() is a pure function of lk.clock (cosine, 0 at midnight, 1 at
//   noon).  Everything else — dusk length, rain dimming — is derived from it here
//   so the contract's one-line meaning stays true.
// * Randomness: this module NEVER calls rnd().  Cosmetics come from Tile.var, a
//   hash of (x,y), or lkr_rand(), a private xorshift — so it cannot perturb the
//   sim's shared stream, and particles are reproducible from a seed.
// * lk_block_tension() is sampled on a 3-tile lattice at 4 Hz for OV_NEEDS, never
//   per tile per frame.
// * Cost, at a 45×28-tile view: ~1 260 floor sprites, ~11 000 ground pixels (per-tile
//   detail + the area-scale scatter + material fringes — see §2, this is the single
//   biggest line and it is what buys a calm exterior), a few hundred structure
//   primitives, one shadow rect per caster, and ~3×60 run-merged darkness rects.
//   Overlays add a dithered fill only while held.  Measured 2.9 ms avg / 4.7 ms max
//   (profile-fleet), against the cart's ~8 ms budget.
// * ⚠ blend() is DELIBERATELY not GPU/software-identical (docs/design/blend-tables.md:
//   the GPU path reads a per-scope SNAPSHOT, the canvas reads live dst), so OVERLAPPING
//   blended shapes accumulate on one renderer and not the other.  Every blend scope
//   here must therefore lay down each darkening ONCE — never a shape on top of a shape.
//   This is also why canvas-diff cannot reach 0 for this cart: the wall/object shadow
//   pass unavoidably overlaps object shadows against wall shadows (~94 px of one-index
//   difference in the very darkest tones, invisible), and the actor shadows land on top
//   of those.  If lockup ever needs a green canvas-diff, declare the budget in the
//   cart's `// canvas-diff: max N` directive rather than flattening the shadows.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LOCKUP_ART_H
#define LOCKUP_ART_H

#include "studio.h"
#include "lockup/model.h"

// ═══ tuning ══════════════════════════════════════════════════════════════════
#define LKR_SHADOW        3      // px the cast shadow steps south-east
#define LKR_MAXPART     384
#define LKR_MAXDROP     224
#define LKR_MAXLAMP     192
#define LKR_BAKE_SECS   0.22f    // seconds between light bakes
#define LKR_BAKE_FRAMES   24     // safety net: rebake anyway every N frames
#define LKR_GUARD_PX     80      // px a guard's presence carries (5 tiles) — the
                                 // OV_DEPLOY ring.  Mirrors actors' LKA_GUARD_REACH
                                 // without reaching into another module's statics.

// Bayer-consistent dither ramp.  fillp()'s 0-bits take the DRAW colour and its
// 1-bits take hole_color, so these are the COMPLEMENTS of the threshold masks:
// the number is the count of drawn pixels per 4×4 cell.
#define LKR_D2    0x7F7F
#define LKR_D4    0x5F5F
#define LKR_D8    0x5A5A
#define LKR_D12   0xA0A0
#define LKR_HATCH 0x7BDE         // 4 pixels on a diagonal — zone/overlay hatch

// half-tile darkness lattice: the whole 96×64 map, plus a tile of margin
#define LKR_SUBW  (LK_MW * 2 + 4)
#define LKR_SUBH  (LK_MH * 2 + 4)

// ── one step darker, for every palette entry ─────────────────────────────────
// Used to dim actors at night and to derive shade tones from a body colour, so
// the whole module has exactly one opinion about "darker".
static const unsigned char LKR_DIM[32] = {
/* 0 black       */ CLR_BLACK,
/* 1 dark blue   */ CLR_DARKER_BLUE,
/* 2 dark purple */ CLR_DARKER_PURPLE,
/* 3 dark green  */ CLR_BLUE_GREEN,
/* 4 brown       */ CLR_DARK_BROWN,
/* 5 dark grey   */ CLR_DARKER_GREY,
/* 6 light grey  */ CLR_DARK_GREY,
/* 7 white       */ CLR_LIGHT_GREY,
/* 8 red         */ CLR_DARK_RED,
/* 9 orange      */ CLR_DARK_ORANGE,
/*10 yellow      */ CLR_ORANGE,
/*11 green       */ CLR_MEDIUM_GREEN,
/*12 blue        */ CLR_TRUE_BLUE,
/*13 indigo      */ CLR_MAUVE,
/*14 pink        */ CLR_DARK_RED,
/*15 light peach */ CLR_PEACH,
/*16 brwn black  */ CLR_BLACK,
/*17 darker blue */ CLR_BLACK,
/*18 darker purp */ CLR_BROWNISH_BLACK,
/*19 blue green  */ CLR_DARKER_BLUE,
/*20 dark brown  */ CLR_BROWNISH_BLACK,
/*21 darker grey */ CLR_BROWNISH_BLACK,
/*22 medium grey */ CLR_DARK_GREY,
/*23 light yell  */ CLR_ORANGE,
/*24 dark red    */ CLR_DARK_PURPLE,
/*25 dark orange */ CLR_BROWN,
/*26 lime green  */ CLR_MEDIUM_GREEN,
/*27 med green   */ CLR_DARK_GREEN,
/*28 true blue   */ CLR_DARK_BLUE,
/*29 mauve       */ CLR_DARKER_PURPLE,
/*30 dark peach  */ CLR_BROWN,
/*31 peach       */ CLR_DARK_PEACH,
};
static inline int lkr_dim(int c, int n) {
    c &= 31;
    while (n-- > 0) c = LKR_DIM[c & 31];
    return c;
}

// ── material palettes ───────────────────────────────────────────────────────
// body = the mass, hi = the NW-lit edge, lo = the SE shade, ln = the joint /
// mortar / seam line, deep = the outline that separates it from the floor.
typedef struct { unsigned char body, hi, lo, ln, deep; } LkrTone;

// WL_BRICK's body used to be CLR_BROWN (#ab5236).  With the exterior calmed down to a
// khaki, a whole cell block in that colour became the loudest object in the frame — a
// bright orange-red rectangle, which is not what an institution looks like from the
// air.  It is now the darker CLR_DARK_BROWN with CLR_BROWN kept as the odd LIGHTER
// brick, so the coursing still reads warm and varied but the mass is calm and the buff
// highlight along the north-west crown is what catches the eye instead.
static const LkrTone LKR_WALL_TONE[WL_COUNT] = {
    /* WL_NONE     */ { 0, 0, 0, 0, 0 },
    /* WL_BRICK    */ { CLR_DARK_BROWN,  CLR_MEDIUM_GREY, CLR_BROWNISH_BLACK, CLR_BROWNISH_BLACK, CLR_BLACK },
    /* WL_CONCRETE */ { CLR_DARK_GREY,   CLR_LIGHT_GREY,  CLR_DARKER_GREY,    CLR_DARKER_GREY,    CLR_BROWNISH_BLACK },
    /* WL_FENCE    */ { CLR_DARK_GREY,   CLR_LIGHT_GREY,  CLR_DARKER_GREY,    CLR_MEDIUM_GREY,    CLR_BROWNISH_BLACK },
    /* WL_PERIM    */ { CLR_DARKER_GREY, CLR_MEDIUM_GREY, CLR_BROWNISH_BLACK, CLR_DARK_GREY,      CLR_BLACK },
};

// Floor detail tones: fleck_dark / fleck_light / joint / joint_lit / base.
// `base` is the material's OWN dominant colour — what it scatters into a neighbouring
// material at a boundary (lkr_floor_edges), so it has to be the colour a player would
// name the surface, not a highlight.
//
// ⚠ THE EXTERIOR IS THE HARDEST THING IN THIS RENDERER TO GET RIGHT, and every
// number below is a retreat from a louder first pass.  Dirt's flecks were
// CLR_DARK_BROWN (#742f29 — brick red) on a CLR_DARK_BROWN sprite, so the whole
// outdoors was red-brown and clashed with the actual brick; grass's lit fleck was
// CLR_LIME_GREEN (#a8e72e) two or three times a tile, which is what made a prison
// yard look like an arcade lawn; gravel's was CLR_LIGHT_GREY, the widest value jump
// this palette owns, three times a tile.  The rule that came out of it: OUTDOOR
// texture is a whisper at 1×.  If you can see the pattern without leaning in, it is
// wrong, because the ground is two thirds of the frame and it must not compete with
// the building or the people.  Tile grout also moved off CLR_INDIGO, which read as
// purple grout in a canteen.
typedef struct { unsigned char dk, lt, jn, jl, base; } LkrFloorTone;
static const LkrFloorTone LKR_FLOOR_TONE[FL_COUNT] = {
    /* dirt     */ { CLR_DARKER_GREY, CLR_MEDIUM_GREY, CLR_BROWNISH_BLACK, CLR_MEDIUM_GREY, CLR_DARK_GREY   },
    /* grass    */ { CLR_BLUE_GREEN,  CLR_MEDIUM_GREEN,CLR_BLUE_GREEN,     CLR_MEDIUM_GREEN,CLR_DARK_GREEN  },
    /* gravel   */ { CLR_DARKER_GREY, CLR_MEDIUM_GREY, CLR_DARK_GREY,      CLR_MEDIUM_GREY, CLR_MEDIUM_GREY },
    /* concrete */ { CLR_DARKER_GREY, CLR_MEDIUM_GREY, CLR_DARKER_GREY,    CLR_MEDIUM_GREY, CLR_DARK_GREY   },
    /* tile     */ { CLR_MEDIUM_GREY, CLR_WHITE,       CLR_DARK_GREY,      CLR_WHITE,       CLR_LIGHT_GREY  },
    /* wood     */ { CLR_DARK_BROWN,  CLR_MEDIUM_GREY, CLR_DARK_BROWN,     CLR_MEDIUM_GREY, CLR_BROWN       },
    /* asphalt  */ { CLR_BROWNISH_BLACK, CLR_DARK_GREY,CLR_DARKER_GREY,    CLR_DARK_GREY,   CLR_DARKER_GREY },
};

// Which materials are LAND rather than paving.  Land encroaches on paving (earth
// creeps onto a path, turf tufts over a kerb) and never the other way round, so this
// is what decides which side of a boundary scatters into the other.
static const unsigned char LKR_NATURAL[FL_COUNT] = { 1, 1, 1, 0, 0, 0, 0 };

// The AREA-scale wash: what a dry patch and a damp patch of each material look like.
// 0xFF = this material has no weather (it is indoors, or it is paving).
// ⚠ The damp tone must be NEUTRAL-dark, not CLR_DARKER_GREY: #49333b is plum, and at
// the wash's first (far too heavy) density it turned the exercise yard purple.  It is
// dithered in at 12–25%, so read these as "one eighth of a tone", not as a colour.
#define LKR_NOWASH 0xFF
static const unsigned char LKR_WASH_DRY[FL_COUNT] = {
    CLR_MEDIUM_GREY, CLR_MEDIUM_GREY, CLR_MEDIUM_GREY,
    LKR_NOWASH, LKR_NOWASH, LKR_NOWASH, LKR_NOWASH };
static const unsigned char LKR_WASH_WET[FL_COUNT] = {
    CLR_BROWNISH_BLACK, CLR_BLUE_GREEN, CLR_BROWNISH_BLACK,
    LKR_NOWASH, LKR_NOWASH, LKR_NOWASH, LKR_NOWASH };

// ── light emitters.  strength 0..1, radius in tiles, warm 0..255 ─────────────
typedef struct { unsigned char obj; unsigned char rad; unsigned char warm; float str; } LkrLamp;
static const LkrLamp LKR_LAMP[] = {
    { OB_LIGHT,  7, 235, 1.00f },   // the ceiling lamp: the reason a block glows
    { OB_COOKER, 3, 255, 0.42f },   // a hot ring
    { OB_TV,     3,  40, 0.46f },   // cold screen wash
    { OB_MEDBED, 2, 120, 0.30f },   // the infirmary's own lamp
    { OB_DESK,   2, 180, 0.26f },   // a desk lamp
};
#define LKR_NLAMPDEF ((int)(sizeof LKR_LAMP / sizeof LKR_LAMP[0]))

// ── prisoner uniform by security category (min / normal / max) ───────────────
static const unsigned char LKR_SEC_UNIFORM[SEC_COUNT] = {
    CLR_YELLOW, CLR_ORANGE, CLR_DARK_ORANGE
};
// ── STAFF WEAR THE JOB ON THEIR HEAD ────────────────────────────────────────
// The sprite's crown is the LK_MAGIC_HAIR index, so pal()ing it per ROLE turns it
// into a cap for free — no extra sprite, no extra swap, still one bucket per role.
// It matters because the frozen contract gives the workman a BROWN uniform, and a
// brown figure on buff ground is the one role that cannot carry its own colour; a
// yellow hard hat makes him hi-vis anyway.  Guards go navy-on-navy (unmistakable),
// the doctor gets a blue scrub cap so he isn't just a paler cook, and prisoners keep
// their OWN hair — which is what makes them read as people rather than as units.
// 0 = no cap, use the actor's hair.
static const unsigned char LKR_ROLE_CAP[RL_COUNT] = {
    0, CLR_DARK_BLUE, CLR_YELLOW, CLR_WHITE, CLR_BLUE
};
static const unsigned char LKR_SKIN[4] = {
    CLR_LIGHT_PEACH, CLR_PEACH, CLR_BROWN, CLR_DARK_BROWN
};
static const unsigned char LKR_HAIR[5] = {
    CLR_BROWNISH_BLACK, CLR_DARK_BROWN, CLR_BROWN, CLR_DARK_ORANGE, CLR_LIGHT_GREY
};
// heat ramp for OV_NEEDS / OV_SAFETY
static const unsigned char LKR_HEAT[5] = {
    CLR_MEDIUM_GREEN, CLR_LIME_GREEN, CLR_YELLOW, CLR_ORANGE, CLR_RED
};

// ═══ module state ════════════════════════════════════════════════════════════
static int   lkr_vx, lkr_vy, lkr_vw = 320, lkr_vh = 200;   // last world view rect
static int   lkr_tx0, lkr_ty0, lkr_tx1, lkr_ty1;           // last visible tile span
static int   lkr_frame;
static float lkr_time;
static unsigned int lkr_rs = 0x9E3779B9u;

static unsigned char lkr_warm[LK_N];        // baked warmth of the light at a tile
static unsigned char lkr_dyn[LK_N];         // per-frame dynamic (torch) light
static unsigned char lkr_dist[LK_N];        // spill BFS distance scratch
static int           lkr_stamp[LK_N];       // spill BFS visited epoch
static int           lkr_epoch;
static int           lkr_q[LK_N];           // spill BFS queue
static short         lkr_dynlist[8192];     // tiles touched by dynamic light
static int           lkr_dynn;

static unsigned char lkr_lvl[LKR_SUBW * LKR_SUBH];    // darkness level per half-tile
static unsigned char lkr_wrm[LKR_SUBW * LKR_SUBH];    // warmth level per half-tile
static unsigned char lkr_grnd[LK_N];        // area-scale ground level, 0 = neutral
                                            // (2 damp · 1 damp-weak · 3/4 sun-dried)

static float lkr_light_t;
static int   lkr_light_dirty = 1;
static int   lkr_amb_out = 255, lkr_amb_in = 255;
static float lkr_night;                     // 0 = broad day .. 1 = pitch dark
static int   lkr_nlamp;
static short lkr_lamp_c[LKR_MAXLAMP];       // lamp origin tiles, for the hotspots

// weather: deterministic from lk.day + lk.clock, never a die roll
static int   lkr_rain;
static float lkr_wet;
static int   lkr_wday = -0x7fff;
static int   lkr_wstart, lkr_wlen;

typedef struct {
    float x, y, vx, vy, life, max, size;
    unsigned char col, kind;                // kind 0 dust, 1 blood, 2 spark
} LkrPart;
static LkrPart lkr_p[LKR_MAXPART];
static int     lkr_pn;

static struct { float x, y, len, spd; } lkr_drop[LKR_MAXDROP];
static int lkr_drops_ready;

// OV_NEEDS / OV_SAFETY sample lattice: lk_block_tension is O(actors) per call, so
// it is sampled every LKR_HSTEP tiles at 4 Hz and bilinearly interpolated back out
// to per-tile when drawn — a heatmap that is smooth without being expensive.
#define LKR_HSTEP 3
#define LKR_HW    (LK_MW / LKR_HSTEP + 3)
#define LKR_HH    (LK_MH / LKR_HSTEP + 3)
static unsigned char lkr_heat[LKR_HW * LKR_HH];      // 0..255 intensity
static float lkr_heat_t = 999.0f;
static int   lkr_heat_mode = -1;
static int   lkr_heat_ox = -9999, lkr_heat_oy = -9999;

// ═══ small utilities ═════════════════════════════════════════════════════════
static inline unsigned int lkr_rand(void) {
    lkr_rs ^= lkr_rs << 13; lkr_rs ^= lkr_rs >> 17; lkr_rs ^= lkr_rs << 5;
    return lkr_rs;
}
static inline float lkr_frand(void) { return (float)(lkr_rand() & 0xFFFFFF) / 16777216.0f; }
static inline unsigned int lkr_hash(int a, int b) {
    unsigned int h = (unsigned int)(a * 374761393) + (unsigned int)(b * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static inline int lkr_fdiv(int a, int b) { return (a >= 0) ? a / b : -(((-a) + b - 1) / b); }
static inline int lkr_min(int a, int b) { return a < b ? a : b; }
static inline int lkr_max(int a, int b) { return a > b ? a : b; }

// integer → text, for the few numbers the overlays show
static const char *lkr_num(int v) {
    static char buf[4][14];
    static int slot;
    char *p = buf[slot++ & 3], tmp[12];
    int n = 0, neg = v < 0;
    unsigned int u = (unsigned int)(neg ? -v : v);
    do { tmp[n++] = (char)('0' + (u % 10u)); u /= 10u; } while (u && n < 11);
    int k = 0;
    if (neg) p[k++] = '-';
    while (n > 0) p[k++] = tmp[--n];
    p[k] = 0;
    return p;
}
// "used/cap"
static const char *lkr_ratio(int a, int b) {
    static char buf[2][20];
    static int slot;
    char *p = buf[slot++ & 1];
    const char *s = lkr_num(a);
    int k = 0;
    while (*s && k < 8) p[k++] = *s++;
    p[k++] = '/';
    s = lkr_num(b);
    while (*s && k < 18) p[k++] = *s++;
    p[k] = 0;
    return p;
}

// world → visible tile span, with one tile of margin
static void lkr_range(int camx, int camy, int vw, int vh) {
    if (vw <= 0) vw = 1;
    if (vh <= 0) vh = 1;
    lkr_tx0 = lkr_max(0, lkr_fdiv(camx, LK_TS) - 1);
    lkr_ty0 = lkr_max(0, lkr_fdiv(camy, LK_TS) - 1);
    lkr_tx1 = lkr_min(LK_MW - 1, lkr_fdiv(camx + vw - 1, LK_TS) + 1);
    lkr_ty1 = lkr_min(LK_MH - 1, lkr_fdiv(camy + vh - 1, LK_TS) + 1);
}

static inline bool lkr_solid_wall(int m) {
    return m == WL_BRICK || m == WL_CONCRETE || m == WL_PERIM;
}
// does light get past this tile?
static inline bool lkr_opaque(const Tile *t) {
    if (lkr_solid_wall(t->wall)) return true;
    if (t->door != DR_NONE && t->door_open < 90) return true;
    return false;
}

// ═══ 1. DAYLIGHT + THE LIGHT BAKE ════════════════════════════════════════════
float lk_daylight(void) {
    // 0 at midnight, 1 at noon — a plain cosine, so the contract's one-liner is
    // literally what this returns.  Dusk shaping lives in lkr_sun().
    float v = 0.5f - 0.5f * de_cosf(lk.clock * (3.14159265f / 12.0f));
    return clamp(v, 0.0f, 1.0f);
}
// the sun as the RENDERER wants it: a long day with a quick dawn and dusk.
static float lkr_sun(void) { return clamp((lk_daylight() - 0.10f) / 0.30f, 0.0f, 1.0f); }

// A blocked breadth-first spill into a flat byte field (the DYNAMIC lights).
// Light reaches a wall tile — it is lit — but never expands FROM one, which is
// what keeps a lamp inside the cell it hangs in.  `record` lists the touched
// tiles so the next frame can clear exactly those and nothing else.
static void lkr_spill(int c0, int rad, float str, unsigned char *dst, bool record) {
    if (!dst || c0 < 0 || c0 >= LK_N || rad <= 0) return;
    lkr_epoch++;
    int head = 0, tail = 0;
    lkr_q[tail++] = c0;
    lkr_stamp[c0] = lkr_epoch;
    lkr_dist[c0] = 0;
    float inv = 1.0f / (float)(rad + 1);
    while (head < tail) {
        int c = lkr_q[head++];
        int d = lkr_dist[c];
        float f = 1.0f - (float)d * inv;
        if (f < 0.0f) f = 0.0f;
        f = f * f;                              // inverse-square-ish falloff
        int add = (int)(str * f * 255.0f);
        if (add > 0) {
            int v = dst[c] + add;
            dst[c] = (unsigned char)(v > 255 ? 255 : v);
            if (record && lkr_dynn < (int)(sizeof lkr_dynlist / sizeof lkr_dynlist[0]))
                lkr_dynlist[lkr_dynn++] = (short)c;
        }
        if (d >= rad) continue;
        if (c != c0 && lkr_opaque(&lk_t[c])) continue;    // lit, but casts no further
        int x = lk_tx(c), y = lk_ty(c);
        static const int ox[4] = { 0, 1, 0, -1 }, oy[4] = { -1, 0, 1, 0 };
        for (int k = 0; k < 4; k++) {
            int nx = x + ox[k], ny = y + oy[k];
            if (!lk_in(nx, ny)) continue;
            int n = lk_idx(nx, ny);
            if (lkr_stamp[n] == lkr_epoch) continue;
            lkr_stamp[n] = lkr_epoch;
            lkr_dist[n] = (unsigned char)(d + 1);
            lkr_q[tail++] = n;
        }
    }
}

// The same walk for the BAKE, which accumulates into Tile.light (a field inside a
// struct array, so it can't share the flat-field version) and also lays down the
// warmth that makes a lamp-lit room read amber instead of merely brighter.
static void lkr_spill_static(int c0, int rad, float str, int warm) {
    if (c0 < 0 || c0 >= LK_N || rad <= 0) return;
    lkr_epoch++;
    int head = 0, tail = 0;
    lkr_q[tail++] = c0; lkr_stamp[c0] = lkr_epoch; lkr_dist[c0] = 0;
    float inv = 1.0f / (float)(rad + 1);
    while (head < tail) {
        int c = lkr_q[head++];
        int d = lkr_dist[c];
        float f = 1.0f - (float)d * inv;
        if (f < 0.0f) f = 0.0f;
        f = f * f;
        int add = (int)(str * f * 255.0f);
        if (add > 0) {
            int v = lk_t[c].light + add;
            lk_t[c].light = (unsigned char)(v > 255 ? 255 : v);
            int w = lkr_warm[c] + (int)((float)warm * f);
            lkr_warm[c] = (unsigned char)(w > 255 ? 255 : w);
        }
        if (d >= rad) continue;
        if (c != c0 && lkr_opaque(&lk_t[c])) continue;
        int x = lk_tx(c), y = lk_ty(c);
        static const int ox[4] = { 0, 1, 0, -1 }, oy[4] = { -1, 0, 1, 0 };
        for (int k = 0; k < 4; k++) {
            int nx = x + ox[k], ny = y + oy[k];
            if (!lk_in(nx, ny)) continue;
            int n = lk_idx(nx, ny);
            if (lkr_stamp[n] == lkr_epoch) continue;
            lkr_stamp[n] = lkr_epoch;
            lkr_dist[n] = (unsigned char)(d + 1);
            lkr_q[tail++] = n;
        }
    }
}

// guard torches — stamped every frame so they move without a rebake
static void lkr_dyn_stamp(void) {
    for (int i = 0; i < lkr_dynn; i++) lkr_dyn[lkr_dynlist[i]] = 0;
    lkr_dynn = 0;
    if (lkr_night < 0.22f) return;
    int margin = LK_TS * 8;
    for (int i = 0; i < lk_nact && lkr_dynn < 7000; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive) continue;
        if (a->role == RL_PRISONER) continue;
        if (a->state == AS_DOWN) continue;
        if (a->x < lkr_vx - margin || a->x > lkr_vx + lkr_vw + margin) continue;
        if (a->y < lkr_vy - margin || a->y > lkr_vy + lkr_vh + margin) continue;
        int c = lk_cell_at(a->x, a->y);
        float str = (a->role == RL_GUARD) ? 0.62f : 0.40f;
        lkr_spill(c, (a->role == RL_GUARD) ? 4 : 3, str * lkr_night, lkr_dyn, true);
        // the cone: one extra reach in the facing direction
        int fx = (int)(de_cosf(a->face) * 2.4f), fy = (int)(de_sinf(a->face) * 2.4f);
        int cx = lk_tx(c) + fx, cy = lk_ty(c) + fy;
        if (lk_in(cx, cy))
            lkr_spill(lk_idx(cx, cy), 3, str * 0.62f * lkr_night, lkr_dyn, true);
    }
}

static inline int lkr_lit_at(int x, int y) {
    if (!lk_in(x, y)) return lkr_amb_out;
    int c = lk_idx(x, y);
    int v = lk_t[c].light, d = lkr_dyn[c];
    v = v + d;                                   // torches ADD to the ambient
    return v > 255 ? 255 : v;
}
static inline int lkr_warm_at(int x, int y) {
    if (!lk_in(x, y)) return 0;
    return lkr_warm[lk_idx(x, y)];
}

// ═══ 2. FLOORS — surfaces, not tiles ═════════════════════════════════════════
//
// The floor SPRITES are deliberately flat and boring (lockup.cart.js explains why: a
// 16×16 sprite repeats identically on every tile, so any feature you can pick out of
// it at 1× comes back as a regular LATTICE of that feature).  Everything a player is
// actually meant to see about the ground is added here, at the three scales a
// repeating sprite cannot reach:
//
//   lkr_floor_detail  per TILE — pebbles, clods, tufts, slab joints.  Hashed on the
//                     tile's own (x,y), so neighbours never agree.
//   lkr_floor_edges   per BOUNDARY — where two materials meet, the LAND side
//                     scatters its own pixels a pixel or two into the paving/other
//                     side, and grass throws tufts over the line.  Without this,
//                     grass-against-dirt is a hard aliased staircase, which is the
//                     single most "tile game" thing a top-down ground can do.
//   lkr_ground_field  per AREA — a smooth ~5-tile value-noise field sorting the
//   + _scatter        exterior into damp hollows and sun-dried rises, spent as loose
//                     hashed pixels (and as the tone of the per-tile detail).  This is
//                     the one that decides whether the outdoors reads as TERRAIN at
//                     all: without it every tile is independently noisy, which is
//                     exactly what a checkerboard is.
//   lkr_room_paint    the yard's worn boundary line, because an UNENCLOSED room has no
//                     walls to say where it is.
//
// All of them are pure functions of tile coordinates, so the ground is identical every
// frame and cannot crawl, shimmer or reroll under a scrolling camera.

// A smooth 0..255 value-noise field: one hashed lattice node every LKR_PATCH tiles,
// bilinearly interpolated.  Deliberately coarse — the whole point is variation at a
// scale SEVERAL TILES ACROSS, so the eye groups the ground into areas.
#define LKR_PATCH 5
static int lkr_field(int x, int y, int salt) {
    int gx = lkr_fdiv(x, LKR_PATCH), gy = lkr_fdiv(y, LKR_PATCH);
    int fx = x - gx * LKR_PATCH, fy = y - gy * LKR_PATCH;
    int s = salt * 977 + 13;
    int a = (int)(lkr_hash(gx,     gy)     & 255u) ^ (s & 63);
    int b = (int)(lkr_hash(gx + 1, gy)     & 255u) ^ (s & 63);
    int c = (int)(lkr_hash(gx,     gy + 1) & 255u) ^ (s & 63);
    int d = (int)(lkr_hash(gx + 1, gy + 1) & 255u) ^ (s & 63);
    int top = a * (LKR_PATCH - fx) + b * fx;
    int bot = c * (LKR_PATCH - fx) + d * fx;
    return (top * (LKR_PATCH - fy) + bot * fy) / (LKR_PATCH * LKR_PATCH);
}

static void lkr_floor_detail(int c, int px, int py) {
    const Tile *t = &lk_t[c];
    int f = t->floor;
    if (f >= FL_COUNT) return;
    const LkrFloorTone *tn = &LKR_FLOOR_TONE[f];
    unsigned int v = t->var;
    int x = lk_tx(c), y = lk_ty(c);
    // the AREA field also colours the per-tile features, so a damp hollow's clods are
    // dark and a sun-dried rise's are buff — the two scales agree instead of merely
    // sitting on top of one another
    int wet = lkr_grnd[c] && lkr_grnd[c] <= 2, dry = lkr_grnd[c] >= 3;

    switch (f) {
    case FL_GRASS: {
        // ONE standing tuft per tile (a second on a quarter of them): a dark blade
        // with a lit tip, so even grass obeys the north-west light.  It used to be
        // two or three per tile with a #a8e72e tip, and that was most of what made
        // the yard read as confetti.
        int n = ((v & 3u) == 0) ? 1 : 0;
        for (int i = 0; i <= n; i++) {
            unsigned int h = lkr_hash(x * 7 + i, y * 13 + i * 5);
            int ax = px + 2 + (int)(h % 12u), ay = py + 4 + (int)((h >> 7) % 10u);
            rectfill(ax, ay, 1, 3, tn->dk);
            pset(ax, ay - 1, dry ? CLR_MEDIUM_GREY : tn->lt);      // a scorched tip
            pset(ax + 1, ay + 1, tn->dk);
        }
        break;
    }
    case FL_GRAVEL: {
        // one loose stone sat proud of the compacted surface, plus a rare second
        unsigned int h = lkr_hash(x * 11, y * 17);
        int ax = px + 2 + (int)(h % 12u), ay = py + 2 + (int)((h >> 6) % 12u);
        pset(ax, ay, wet ? tn->dk : tn->lt);
        pset(ax + 1, ay + 1, tn->dk);
        if ((v & 3u) == 0) { pset(ax - 2, ay + 2, tn->lt); pset(ax - 1, ay + 3, tn->dk); }
        break;
    }
    case FL_DIRT: {
        // a clod — buff where the ground is dried out, dark where it is damp — and
        // one small stone on a tile in eight
        unsigned int h = lkr_hash(x * 5, y * 23);
        int ax = px + 2 + (int)(h % 12u), ay = py + 2 + (int)((h >> 5) % 12u);
        int cl = wet ? tn->dk : tn->lt;
        pset(ax, ay, cl); pset(ax + 1, ay, cl);
        pset(ax, ay + 1, wet ? tn->jn : tn->dk);
        if ((v & 7u) == 0) {
            int bx = px + 3 + (int)((h >> 11) % 10u), by = py + 4 + (int)((h >> 17) % 9u);
            pset(bx, by, tn->jn);
            pset(bx, by - 1, tn->lt);
        }
        break;
    }
    case FL_WOOD: {
        // a staggered plank butt-joint, lit on its north side
        int jx = px + 2 + (int)(v % 12u);
        rectfill(jx, py, 1, LK_TS, tn->dk);
        rectfill(jx + 1, py, 1, LK_TS, tn->lt);
        break;
    }
    case FL_TILE: {
        // grout at the tile boundary so a big tiled room reads as one grid
        rectfill(px, py, LK_TS, 1, tn->jn);
        rectfill(px, py, 1, LK_TS, tn->jn);
        if ((v & 31u) == 0) pset(px + 3, py + 3, tn->jl);        // a rare sheen
        break;
    }
    case FL_CONCRETE:
    case FL_ASPHALT: {
        // expansion joints at slab scale (every 3 tiles), only where the slab
        // actually continues — a joint must never run off the end of the concrete.
        bool same_w = lk_in(x - 1, y) && lk_t[lk_idx(x - 1, y)].floor == f;
        bool same_n = lk_in(x, y - 1) && lk_t[lk_idx(x, y - 1)].floor == f;
        if (same_w && (x % 3) == 0) {
            rectfill(px, py, 1, LK_TS, tn->jn);
            rectfill(px + 1, py, 1, LK_TS, tn->jl);
        }
        if (same_n && (y % 3) == 0) {
            rectfill(px, py, LK_TS, 1, tn->jn);
            rectfill(px, py + 1, LK_TS, 1, tn->jl);
        }
        // one blemish per tile in four, and nothing brighter than the joint tone:
        // the sprite already carries the grain, and doubling it here is what made an
        // indoor slab fizz the same way the exterior did.
        if ((v & 3u) == 0) pset(px + 5 + (int)((v >> 4) & 5u), py + 6 + (int)((v >> 2) & 7u), tn->dk);
        break;
    }
    default: break;
    }
}

// ── material EDGES ──────────────────────────────────────────────────────────
// Two materials that merely butt up against each other read as an aliased jump.  So
// every tile takes a few pixels of each LAND neighbour's own base tone one or two
// pixels into itself, and a grass neighbour additionally throws a couple of tufts
// over the line — earth encroaches on paving and turf encroaches on earth, never the
// reverse (LKR_NATURAL decides which side gives).  Hashed on the tile and the
// direction, so the fringe is fixed in the world and identical every frame.
static void lkr_floor_edges(int c, int px, int py) {
    int f = lk_t[c].floor;
    if (f >= FL_COUNT) return;
    int x = lk_tx(c), y = lk_ty(c);
    static const int ox[4] = { 0, 1, 0, -1 }, oy[4] = { -1, 0, 1, 0 };
    for (int k = 0; k < 4; k++) {
        int nx = x + ox[k], ny = y + oy[k];
        if (!lk_in(nx, ny)) continue;
        int nf = lk_t[lk_idx(nx, ny)].floor;
        if (nf == f || nf >= FL_COUNT || !LKR_NATURAL[nf]) continue;
        const LkrFloorTone *nt = &LKR_FLOOR_TONE[nf];
        unsigned int h = lkr_hash(x * 31 + k, y * 17 + nf * 7);
        for (int i = 0; i < 7; i++) {
            int u = (int)((h >> (i * 3)) & 15u);          // position along the edge
            int d = (int)((h >> (i * 2 + 1)) & 3u);       // depth in: 0..3
            int ax = (k & 1) ? (k == 1 ? px + LK_TS - 1 - d : px + d) : px + u;
            int ay = (k & 1) ? py + u : (k == 0 ? py + d : py + LK_TS - 1 - d);
            pset(ax, ay, nt->base);
            if (d == 0) pset(ax + ((k == 1) ? -1 : (k == 3) ? 1 : 0),
                             ay + ((k == 0) ?  1 : (k == 2) ? -1 : 0), nt->base);
        }
        if (nf == FL_GRASS) {
            for (int i = 0; i < 2; i++) {
                int u = (int)((h >> (i * 6 + 3)) & 15u);
                int d = 1 + (int)((h >> (i * 3)) & 3u);
                int ax = (k & 1) ? (k == 1 ? px + LK_TS - 1 - d : px + d) : px + u;
                int ay = (k & 1) ? py + u : (k == 0 ? py + d : py + LK_TS - 2 - d);
                rectfill(ax, ay, 1, 2, nt->dk);
                pset(ax, ay - 1, nt->lt);
            }
        }
    }
}

// ── the AREA-scale ground: DRY PATCHES AND DAMP PATCHES ─────────────────────
// The smooth 5-tile field sorts every exterior tile into damp (2 = strong, 1 = weak),
// neutral (0) or sun-dried (3 weak, 4 strong).  Computed once per frame for the
// visible span, because lkr_floor_detail reads it too: the same field that darkens a
// hollow also makes that hollow's clods darker, which is what ties the two scales
// together instead of laying one on top of the other.
static void lkr_ground_field(void) {
    for (int y = lkr_ty0; y <= lkr_ty1; y++) {
        for (int x = lkr_tx0; x <= lkr_tx1; x++) {
            int c = lk_idx(x, y);
            int f = lk_t[c].floor;
            if (f >= FL_COUNT || LKR_WASH_DRY[f] == LKR_NOWASH) { lkr_grnd[c] = 0; continue; }
            int fv = lkr_field(x, y, 0);
            lkr_grnd[c] = (unsigned char)(fv < 58 ? 2 : fv < 94 ? 1
                                        : fv > 198 ? 4 : fv > 164 ? 3 : 0);
        }
    }
}

// …and the tone shift itself, as SCATTER rather than as a dithered overlay.
// ⚠ This was a fillp() wash first, and that is a trap worth writing down: fillp's
// pattern is a 4×4 lattice, so over a patch of ground several tiles across it stops
// reading as weather and starts reading as a WINDOW SCREEN laid over the map — the
// yard came out visibly combed with vertical dashes at 2px pitch, which is a worse
// artefact than the per-tile noise the field was added to cure.  Any regular lattice
// loses at this scale.  So the field spends its budget on hashed loose pixels
// instead: organic at every zoom, no lattice to line up, and a handful of psets.
static void lkr_ground_scatter(int c, int px, int py, int f) {
    int lvl = lkr_grnd[c];
    if (!lvl) return;
    int col = (lvl <= 2) ? LKR_WASH_WET[f] : LKR_WASH_DRY[f];
    int n = (lvl == 2 || lvl == 4) ? 8 : 4;
    unsigned int h = lkr_hash(lk_tx(c) * 97 + 5, lk_ty(c) * 61 + 11);
    for (int i = 0; i < n; i++) {
        h = h * 1664525u + 1013904223u;
        pset(px + (int)((h >> 8) & 15u), py + (int)((h >> 18) & 15u), col);
    }
}

// ── an OUTDOOR room has no walls to say where it is ─────────────────────────
// RM_YARD is the one room type the contract lets be unenclosed, which means a
// stranger sees three benches and a weights bar sitting on bare gravel with nothing
// to explain them — a critic called them "unexplained brown objects outside", and
// they were right: there was no yard, only furniture.  A worn painted boundary (the
// thing a real yard actually has) turns that into "the exercise yard, with its
// furniture in it".  Kept to one buff pixel with gaps, because it is ground marking,
// not UI — the OV_ROOMS overlay is still where a player goes for the real answer.
static void lkr_room_paint(void) {
    static const int ox[4] = { 0, 1, 0, -1 }, oy[4] = { -1, 0, 1, 0 };
    for (int y = lkr_ty0; y <= lkr_ty1; y++) {
        for (int x = lkr_tx0; x <= lkr_tx1; x++) {
            int c = lk_idx(x, y);
            int rid = lk_t[c].room;
            if (rid <= 0 || rid >= LK_MAXROOM) continue;
            int rt = lk_room[rid].type;
            if (rt <= RM_NONE || rt >= RM_COUNT || LK_ROOM[rt].enclosed) continue;
            int px = x * LK_TS, py = y * LK_TS;
            unsigned int h = lkr_hash(x * 13, y * 29);
            for (int k = 0; k < 4; k++) {
                int nx = x + ox[k], ny = y + oy[k];
                if (lk_in(nx, ny) && lk_t[lk_idx(nx, ny)].room == rid) continue;
                for (int i = 1; i < LK_TS - 1; i++) {
                    if ((h >> (i & 15)) & 1u) continue;          // where the paint wore off
                    int ax = (k & 1) ? (k == 1 ? px + LK_TS - 2 : px + 1) : px + i;
                    int ay = (k & 1) ? py + i : (k == 0 ? py + 1 : py + LK_TS - 2);
                    // CLR_LIGHT_GREY, not CLR_MEDIUM_GREY: buff paint on a buff-flecked
                    // gravel is invisible, which is exactly how the first attempt failed
                    pset(ax, ay, CLR_LIGHT_GREY);
                }
            }
        }
    }
}

// ═══ 3. WALLS — one structure, drawn from the join mask ══════════════════════
static void lkr_razor(int px, int py, bool horiz, int col) {
    // coils along the crown: three little arcs plus the strand
    if (horiz) {
        line(px, py - 2, px + LK_TS - 1, py - 2, lkr_dim(col, 1));
        for (int k = 0; k < 3; k++) arc(px + 3 + k * 5, py - 2, 3, 190.0f, 350.0f, col);
    } else {
        line(px - 2, py, px - 2, py + LK_TS - 1, lkr_dim(col, 1));
        for (int k = 0; k < 3; k++) arc(px - 2, py + 3 + k * 5, 3, 100.0f, 260.0f, col);
    }
}

static void lkr_wall_body(int px, int py, int mat, int joins, int var) {
    if (mat <= WL_NONE || mat >= WL_COUNT) return;
    const LkrTone *w = &LKR_WALL_TONE[mat];
    int N = joins & 1, E = joins & 2, S = joins & 4, W = joins & 8;

    if (mat == WL_FENCE) {
        // See-through: the floor is already down, chain-link goes over it.  Each
        // JOINED direction gets its own half-segment from the centre out, so an end
        // is an end, a corner is a corner and a tee is a tee — never a full cross.
        int mid = LK_TS / 2;
        int have = (N || E || S || W);
        if (!have) { E = W = 2; }                       // a lone post still reads
        if (W) {                                        // west half of the E-W run
            rectfill(px, py + mid - 3, mid, 1, w->ln);
            rectfill(px, py + mid + 2, mid, 1, w->lo);
            for (int k = 0; k < 2; k++) {
                int ax = px + k * 4;
                line(ax, py + mid + 2, ax + 3, py + mid - 3, w->hi);
                line(ax, py + mid - 3, ax + 3, py + mid + 2, w->ln);
            }
        }
        if (E) {                                        // east half
            rectfill(px + mid, py + mid - 3, mid, 1, w->ln);
            rectfill(px + mid, py + mid + 2, mid, 1, w->lo);
            for (int k = 0; k < 2; k++) {
                int ax = px + mid + k * 4;
                line(ax, py + mid + 2, ax + 3, py + mid - 3, w->hi);
                line(ax, py + mid - 3, ax + 3, py + mid + 2, w->ln);
            }
        }
        if (N) {                                        // north half of the N-S run
            rectfill(px + mid - 3, py, 1, mid, w->ln);
            rectfill(px + mid + 2, py, 1, mid, w->lo);
            for (int k = 0; k < 2; k++) {
                int ay = py + k * 4;
                line(px + mid - 3, ay, px + mid + 2, ay + 3, w->hi);
                line(px + mid + 2, ay, px + mid - 3, ay + 3, w->ln);
            }
        }
        if (S) {                                        // south half
            rectfill(px + mid - 3, py + mid, 1, mid, w->ln);
            rectfill(px + mid + 2, py + mid, 1, mid, w->lo);
            for (int k = 0; k < 2; k++) {
                int ay = py + mid + k * 4;
                line(px + mid - 3, ay, px + mid + 2, ay + 3, w->hi);
                line(px + mid + 2, ay, px + mid - 3, ay + 3, w->ln);
            }
        }
        // the post: every tile gets one, lit on its north-west
        rectfill(px + mid - 1, py + mid - 5, 2, 11, w->body);
        rectfill(px + mid - 1, py + mid - 5, 1, 11, w->hi);
        pset(px + mid - 1, py + mid - 6, w->hi);
        pset(px + mid,     py + mid + 5, w->deep);
        return;
    }

    // ── the mass ────────────────────────────────────────────────────────────
    rectfill(px, py, LK_TS, LK_TS, w->body);

    if (mat == WL_BRICK) {
        // courses indexed off WORLD y, staggered per course, so a long run is one
        // continuous bond instead of a repeating 16-px stamp
        for (int r = 0; r < 4; r++) {
            int yy = py + r * 4;
            int course = (py >> 2) + r;
            int off = (course & 1) ? 4 : 0;
            rectfill(px, yy + 3, LK_TS, 1, w->ln);              // mortar bed
            rectfill(px + off, yy, 1, 3, w->ln);                // perpends
            rectfill(px + off + 8, yy, 1, 3, w->ln);
            if (((course + (int)(var & 3u)) & 3) == 0)           // an odd LIGHTER brick
                rectfill(px + off + 1, yy, 7, 3, CLR_BROWN);
        }
    } else if (mat == WL_CONCRETE) {
        bool horiz = (E || W);
        bool vert  = (N || S);
        // a faint pour variation, then panel seams every two tiles
        if ((var & 3u) == 0) rectfill(px + 3, py + 6, 6, 4, lkr_dim(w->body, 1));
        if (horiz && (((px >> 4) & 1) == 0)) {
            rectfill(px, py, 1, LK_TS, w->ln);
            rectfill(px + 1, py, 1, LK_TS, w->hi);
        }
        if (vert && (((py >> 4) & 1) == 0)) {
            rectfill(px, py, LK_TS, 1, w->ln);
            rectfill(px, py + 1, LK_TS, 1, w->hi);
        }
    } else if (mat == WL_PERIM) {
        // heavy: horizontal form-lines, a dark base, and razor wire on the crown
        rectfill(px, py + 5,  LK_TS, 1, w->ln);
        rectfill(px, py + 10, LK_TS, 1, w->ln);
        rectfill(px, py + LK_TS - 3, LK_TS, 3, w->lo);
        if ((var & 1u) == 0) rectfill(px + 9, py + 6, 5, 3, lkr_dim(w->body, 1));
    }

    // ── edges: highlight only on a face with NO wall neighbour ──────────────
    if (!N) {
        rectfill(px, py, LK_TS, 1, w->hi);
        rectfill(px, py + 1, LK_TS, 1, lkr_dim(w->hi, 1));
    }
    if (!W) {
        rectfill(px, py, 1, LK_TS, w->hi);
        rectfill(px + 1, py, 1, LK_TS, lkr_dim(w->hi, 1));
    }
    if (!S) {
        rectfill(px, py + LK_TS - 2, LK_TS, 1, w->lo);
        rectfill(px, py + LK_TS - 1, LK_TS, 1, w->deep);
    }
    if (!E) {
        rectfill(px + LK_TS - 2, py, 1, LK_TS, w->lo);
        rectfill(px + LK_TS - 1, py, 1, LK_TS, w->deep);
    }
    // inner corners: a wall on both of two faces but not on the diagonal between
    // them is a concave junction — give it a small lit/shaded quoin so an L-shaped
    // room corner doesn't read as a flat plane.
    if (N && W && !(joins & 0x80)) rectfill(px, py, 2, 2, w->hi);
    if (N && E && !(joins & 0x10)) rectfill(px + LK_TS - 2, py, 2, 2, lkr_dim(w->hi, 1));
    if (S && W && !(joins & 0x40)) rectfill(px, py + LK_TS - 2, 2, 2, w->lo);
    if (S && E && !(joins & 0x20)) rectfill(px + LK_TS - 2, py + LK_TS - 2, 2, 2, w->deep);

    if (mat == WL_PERIM) {
        if (!N) lkr_razor(px, py, true, CLR_LIGHT_GREY);
        else if (!W) lkr_razor(px, py, false, CLR_LIGHT_GREY);
    }
}

// ═══ 4. DOORS — jambs, a swinging leaf, and a visible lock ═══════════════════
// local coords: u runs ALONG the wall, v ACROSS it
static void lkr_lrect(int px, int py, bool horiz, int u, int v, int lu, int lv, int col) {
    if (horiz) rectfill(px + u, py + v, lu, lv, col);
    else       rectfill(px + v, py + u, lv, lu, col);
}
static int lkr_neigh_wall(int c) {
    int x = lk_tx(c), y = lk_ty(c);
    static const int ox[4] = { 0, 1, 0, -1 }, oy[4] = { -1, 0, 1, 0 };
    for (int k = 0; k < 4; k++) {
        if (!lk_in(x + ox[k], y + oy[k])) continue;
        int m = lk_t[lk_idx(x + ox[k], y + oy[k])].wall;
        if (m != WL_NONE) return m;
    }
    return WL_CONCRETE;
}

static void lkr_door_tile(int c, int px, int py, int kind, int joins, int open, int locked) {
    if (kind <= DR_NONE || kind >= DR_COUNT) return;
    int ew = ((joins & 2) ? 1 : 0) + ((joins & 8) ? 1 : 0);
    int ns = ((joins & 1) ? 1 : 0) + ((joins & 4) ? 1 : 0);
    bool horiz = ew >= ns;
    const LkrTone *w = &LKR_WALL_TONE[lkr_neigh_wall(c)];
    float op = (float)open / 255.0f;

    // jambs: two stubs of the parent wall, so the doorway is a GAP in a structure
    for (int s = 0; s < 2; s++) {
        int u = s ? LK_TS - 3 : 0;
        lkr_lrect(px, py, horiz, u, 0, 3, LK_TS, w->body);
        lkr_lrect(px, py, horiz, u, 0, 3, 1, w->hi);                 // north/west lit
        lkr_lrect(px, py, horiz, u, LK_TS - 1, 3, 1, w->deep);
        if (!s) lkr_lrect(px, py, horiz, 0, 0, 1, LK_TS, w->hi);
        else    lkr_lrect(px, py, horiz, LK_TS - 1, 0, 1, LK_TS, w->deep);
    }
    // the threshold: a worn sill across the opening
    lkr_lrect(px, py, horiz, 3, 6, 10, 4, lkr_dim(w->body, 1));
    lkr_lrect(px, py, horiz, 3, 6, 10, 1, w->ln);
    lkr_lrect(px, py, horiz, 3, 9, 10, 1, lkr_dim(w->body, 2));

    int hu = 3, hv = LK_TS / 2;                       // hinge, in local coords
    int hx = horiz ? px + hu : px + hv;
    int hy = horiz ? py + hv : py + hu;
    int leafL = 10;

    if (kind == DR_GATE) {
        // heavy sliding gate: the slab retracts into the jamb along the run
        int len = (int)((float)leafL * (1.0f - op) + 0.5f);
        if (len > 0) {
            lkr_lrect(px, py, horiz, 3, LK_TS / 2 - 3, len, 6, CLR_DARK_GREY);
            lkr_lrect(px, py, horiz, 3, LK_TS / 2 - 3, len, 1, CLR_LIGHT_GREY);
            lkr_lrect(px, py, horiz, 3, LK_TS / 2 + 2, len, 1, CLR_BROWNISH_BLACK);
            for (int k = 0; k < len - 1; k += 3)      // hazard chevrons
                line(horiz ? px + 3 + k : px + LK_TS / 2 - 3,
                     horiz ? py + LK_TS / 2 + 2 : py + 3 + k,
                     horiz ? px + 3 + k + 2 : px + LK_TS / 2 + 2,
                     horiz ? py + LK_TS / 2 - 3 : py + 3 + k + 2, CLR_YELLOW);
        }
        // the rail it rides on
        lkr_lrect(px, py, horiz, 3, LK_TS / 2 + 4, 10, 1, CLR_DARKER_GREY);
    } else {
        // a swinging leaf: hinged on the low end, opening toward -v (north/west)
        float base = horiz ? 0.0f : 90.0f;
        float deg  = base - 84.0f * op * (horiz ? 1.0f : -1.0f);
        float rad  = deg * 0.017453293f;
        float ux = de_cosf(rad), uy = de_sinf(rad);
        float cx = (float)hx + ux * (float)leafL * 0.5f;
        float cy = (float)hy + uy * (float)leafL * 0.5f;
        int leaf_col  = (kind == DR_JAIL)  ? CLR_DARKER_GREY
                      : (kind == DR_STAFF) ? CLR_DARK_GREY : CLR_BROWN;
        int leaf_hi   = (kind == DR_PLAIN) ? CLR_MEDIUM_GREY : CLR_LIGHT_GREY;
        rectfill_rot((int)cx, (int)cy, leafL, 4, deg, leaf_col);
        // the lit edge of the leaf, 1 px toward the light
        rectfill_rot((int)cx - 1, (int)cy - 1, leafL, 1, deg, leaf_hi);

        if (kind == DR_JAIL) {
            // bars, seen edge-on: bright ticks across the leaf
            float px2 = -uy, py2 = ux;
            for (int k = 0; k < 4; k++) {
                float f = 0.16f + 0.23f * (float)k;
                float bx = (float)hx + ux * (float)leafL * f;
                float by = (float)hy + uy * (float)leafL * f;
                thickline((int)(bx - px2 * 1.5f), (int)(by - py2 * 1.5f),
                          (int)(bx + px2 * 1.5f), (int)(by + py2 * 1.5f), 1, CLR_LIGHT_GREY);
            }
        } else if (kind == DR_STAFF) {
            float f = 0.66f;
            int wx = (int)((float)hx + ux * (float)leafL * f);
            int wy = (int)((float)hy + uy * (float)leafL * f);
            rectfill(wx - 1, wy - 1, 3, 2, CLR_BLUE);            // the wired window
            pset(wx - 1, wy - 1, CLR_WHITE);
        } else {
            float f = 0.82f;
            pset((int)((float)hx + ux * (float)leafL * f),
                 (int)((float)hy + uy * (float)leafL * f), CLR_YELLOW);   // handle
        }
        // ── LOCKED: a red bolt thrown across the leaf, plus a padlock on the jamb
        if (locked) {
            float f = 0.5f;
            int bx = (int)((float)hx + ux * (float)leafL * f);
            int by = (int)((float)hy + uy * (float)leafL * f);
            float px2 = -uy, py2 = ux;
            thickline((int)(bx - px2 * 3.0f), (int)(by - py2 * 3.0f),
                      (int)(bx + px2 * 3.0f), (int)(by + py2 * 3.0f), 2, CLR_DARK_RED);
            thickline((int)(bx - px2 * 2.0f), (int)(by - py2 * 2.0f),
                      (int)(bx + px2 * 2.0f), (int)(by + py2 * 2.0f), 1, CLR_RED);
            int lx = horiz ? px + 1 : px + LK_TS / 2 - 1;
            int ly = horiz ? py + LK_TS / 2 - 1 : py + 1;
            rectfill(lx, ly, 3, 3, CLR_YELLOW);
            pset(lx + 1, ly - 1, CLR_LIGHT_GREY);
            pset(lx + 1, ly + 1, CLR_BROWNISH_BLACK);
        }
    }
}

// ═══ 5. OBJECTS ══════════════════════════════════════════════════════════════
static void lkr_obj_tile(int c, int px, int py) {
    const Tile *t = &lk_t[c];
    int ob = t->obj;
    if (ob == OB_NONE || ob >= OB_COUNT || t->obj_ref) return;
    const ObjDef *o = &LK_OBJ[ob];
    for (int dy = 0; dy < o->h; dy++)
        for (int dx = 0; dx < o->w; dx++)
            spr(o->sprite + dy * 8 + dx, px + dx * LK_TS, py + dy * LK_TS);

    // FACING, drawn as wear on the floor rather than a mirrored sprite (see notes).
    // Only onto a tile you could actually stand on, so it never speckles a wall.
    int fw = o->w * LK_TS, fh = o->h * LK_TS;
    int scuff = (t->floor == FL_GRASS) ? CLR_DARK_GREEN : CLR_DARKER_GREY;
    int x = lk_tx(c), y = lk_ty(c);
    static const int sox[4] = { 0, 0, 1, -1 }, soy[4] = { 1, -1, 0, 0 };
    int dir = t->obj_dir & 3;
    int nx = x + sox[dir] * (dir == 2 ? o->w : 1), ny = y + soy[dir] * (dir == 0 ? o->h : 1);
    if (!lk_in(nx, ny) || lk_solid(lk_idx(nx, ny))) return;
    switch (dir) {
    case 0: for (int k = 3; k < fw - 3; k += 3) pset(px + k, py + fh, scuff); break;
    case 1: for (int k = 3; k < fw - 3; k += 3) pset(px + k, py - 1, scuff); break;
    case 2: for (int k = 3; k < fh - 3; k += 3) pset(px + fw, py + k, scuff); break;
    default:for (int k = 3; k < fh - 3; k += 3) pset(px - 1, py + k, scuff); break;
    }
}

// ═══ 6. THE SHADOW PASS ══════════════════════════════════════════════════════
static void lkr_shadow_pass(void) {
    blend(BLEND_AVG);
    for (int y = lkr_ty0; y <= lkr_ty1; y++) {
        for (int x = lkr_tx0; x <= lkr_tx1; x++) {
            int c = lk_idx(x, y);
            const Tile *t = &lk_t[c];
            int px = x * LK_TS, py = y * LK_TS;
            if (lkr_solid_wall(t->wall)) {
                rectfill(px + LKR_SHADOW, py + LKR_SHADOW, LK_TS, LK_TS, CLR_BLACK);
                continue;
            }
            if (t->wall == WL_FENCE) {          // a fence casts a thin one
                rectfill(px + LKR_SHADOW, py + LK_TS / 2 - 2 + LKR_SHADOW, LK_TS, 4, CLR_BLACK);
                continue;
            }
            if (t->door != DR_NONE) {           // only the jambs cast
                int ew = ((t->joins & 2) ? 1 : 0) + ((t->joins & 8) ? 1 : 0);
                int ns = ((t->joins & 1) ? 1 : 0) + ((t->joins & 4) ? 1 : 0);
                bool horiz = ew >= ns;
                lkr_lrect(px + LKR_SHADOW, py + LKR_SHADOW, horiz, 0, 0, 3, LK_TS, CLR_BLACK);
                lkr_lrect(px + LKR_SHADOW, py + LKR_SHADOW, horiz, LK_TS - 3, 0, 3, LK_TS, CLR_BLACK);
                continue;
            }
            if (t->obj != OB_NONE && !t->obj_ref) {
                const ObjDef *o = &LK_OBJ[t->obj];
                if (o->solid)
                    rectfill(px + 2, py + 2, o->w * LK_TS, o->h * LK_TS, CLR_BLACK);
                else
                    ovalfill(px + LK_TS / 2 + 1, py + LK_TS - 3, 5, 2, CLR_BLACK);
            }
        }
    }
    blend_reset();
}

// ═══ 7. THE LIGHT PASS — solid MUL washes + a dithered warm ADD ══════════════
//
// Darkness is a 6-stop ramp built by COMPOSING three solid blend(BLEND_MUL)
// washes, because multiplying twice squares the factor and that buys stops for
// free.  Rough luminance factors:
//
//   1  light grey                 0.77   the first hint of dusk
//   2  indigo                     0.50
//   3  indigo · light grey        0.39
//   4  indigo²                    0.25
//   5  indigo² · light grey       0.19
//   6  indigo³                    0.13   deep night
//
// so a pass is needed for {1,3,5} (light grey), {2,3,4,5,6} (indigo ≥1),
// {4,5,6} (indigo ≥2) and {6} (indigo ≥3) — four SOLID run-merged passes inside
// one blend scope.  Solid and not dithered on purpose: a dithered full-screen
// fill is a per-pixel loop on the software canvas (i.e. in the browser), whereas
// a solid one is a row memset.  The cost of that choice is that dusk steps
// through six discrete tones over about a minute of real time instead of fading
// continuously — the honest limit of 32 colours, and it reads as a sky changing
// rather than a slider moving.
#define LKR_DARK_STOPS 6
static const unsigned char LKR_DARK_BOUND[LKR_DARK_STOPS] = { 29, 93, 142, 173, 199, 215 };
#define LKR_M_GREY  0x2A     // levels 1,3,5
#define LKR_M_IND1  0x7C     // levels 2,3,4,5,6
#define LKR_M_IND2  0x70     // levels 4,5,6
#define LKR_M_IND3  0x40     // level 6

// Draw every cell of `g` whose level is in `mask` as run-length-merged 8-px rects.
static void lkr_runs(const unsigned char *g, int sw, int sh, int ox, int oy,
                     int mask, int col) {
    for (int j = 0; j < sh; j++) {
        const unsigned char *row = g + j * LKR_SUBW;
        int i = 0;
        while (i < sw) {
            if (!((mask >> row[i]) & 1)) { i++; continue; }
            int s = i;
            while (i < sw && ((mask >> row[i]) & 1)) i++;
            rectfill(ox + s * 8, oy + j * 8, (i - s) * 8, 8, col);
        }
    }
}

static void lkr_light_pass(void) {
    int tw = lkr_tx1 - lkr_tx0 + 1, th = lkr_ty1 - lkr_ty0 + 1;
    if (tw <= 0 || th <= 0) return;
    int sw = tw * 2, sh = th * 2;
    if (sw > LKR_SUBW) sw = LKR_SUBW;
    if (sh > LKR_SUBH) sh = LKR_SUBH;
    int maxlvl = 0, maxwrm = 0;
    float nightw = lkr_night;

    for (int ty = 0; ty < th; ty++) {
        for (int tyq = 0; tyq < 2; tyq++) {
            int j = ty * 2 + tyq;
            if (j >= sh) break;
            unsigned char *lrow = lkr_lvl + j * LKR_SUBW;
            unsigned char *wrow = lkr_wrm + j * LKR_SUBW;
            int y = lkr_ty0 + ty, ny = y + (tyq ? 1 : -1);
            for (int tx = 0; tx < tw; tx++) {
                int x = lkr_tx0 + tx;
                for (int txq = 0; txq < 2; txq++) {
                    int i = tx * 2 + txq;
                    if (i >= sw) break;
                    int nx = x + (txq ? 1 : -1);
                    // 4-tap bilinear of the tile field → an organic pool edge
                    int lv = (9 * lkr_lit_at(x, y) + 3 * lkr_lit_at(nx, y)
                                                   + 3 * lkr_lit_at(x, ny)
                                                   + 1 * lkr_lit_at(nx, ny)) >> 4;
                    int dk = 255 - lv;
                    int lvl = 0;
                    while (lvl < LKR_DARK_STOPS && dk >= LKR_DARK_BOUND[lvl]) lvl++;
                    lrow[i] = (unsigned char)lvl;
                    if (lvl > maxlvl) maxlvl = lvl;
                    int wv = 0;
                    if (nightw > 0.14f) {
                        wv = (9 * lkr_warm_at(x, y) + 3 * lkr_warm_at(nx, y)
                                                    + 3 * lkr_warm_at(x, ny)
                                                    + 1 * lkr_warm_at(nx, ny)) >> 4;
                        wv = (int)((float)wv * nightw);
                    }
                    int wl = wv <= 22 ? 0 : wv <= 74 ? 1 : wv <= 150 ? 2 : 3;
                    wrow[i] = (unsigned char)wl;
                    if (wl > maxwrm) maxwrm = wl;
                }
            }
        }
    }

    int ox = lkr_tx0 * LK_TS, oy = lkr_ty0 * LK_TS;
    if (maxlvl > 0) {
        // four SOLID multiply washes composing the six-stop ramp (see above)
        blend(BLEND_MUL);
        lkr_runs(lkr_lvl, sw, sh, ox, oy, LKR_M_GREY, CLR_LIGHT_GREY);
        lkr_runs(lkr_lvl, sw, sh, ox, oy, LKR_M_IND1, CLR_INDIGO);
        lkr_runs(lkr_lvl, sw, sh, ox, oy, LKR_M_IND2, CLR_INDIGO);
        lkr_runs(lkr_lvl, sw, sh, ox, oy, LKR_M_IND3, CLR_INDIGO);
        blend_reset();
    }
    if (maxwrm > 0) {
        // the warm half: small, so it can afford the dither that makes it glow
        blend(BLEND_ADD);
        fillp(LKR_D2, -1);  lkr_runs(lkr_wrm, sw, sh, ox, oy, 1 << 1, CLR_DARK_BROWN);
        fillp(LKR_D4, -1);  lkr_runs(lkr_wrm, sw, sh, ox, oy, 1 << 2, CLR_BROWN);
        fillp(LKR_D8, -1);  lkr_runs(lkr_wrm, sw, sh, ox, oy, 1 << 3, CLR_BROWN);
        fillp_reset();
        // the bulbs themselves: a hard hot core, so a lamp reads as a lamp
        for (int i = 0; i < lkr_nlamp; i++) {
            int c = lkr_lamp_c[i];
            int x = lk_tx(c), y = lk_ty(c);
            if (x < lkr_tx0 || x > lkr_tx1 || y < lkr_ty0 || y > lkr_ty1) continue;
            int px = x * LK_TS + LK_TS / 2, py = y * LK_TS + LK_TS / 2;
            circfill(px, py, 3, CLR_DARK_ORANGE);
            circfill(px, py, 1, CLR_LIGHT_YELLOW);
        }
        blend_reset();
    }
}

// collect the lamp origins once per bake, for the hot cores above
static void lkr_lamp_gather(void) {
    lkr_nlamp = 0;
    for (int c = 0; c < LK_N && lkr_nlamp < LKR_MAXLAMP; c++) {
        const Tile *t = &lk_t[c];
        if (t->obj != OB_LIGHT || t->obj_ref) continue;
        lkr_lamp_c[lkr_nlamp++] = (short)c;
    }
}

// ═══ 8. WEATHER ══════════════════════════════════════════════════════════════
static void lkr_weather_update(float d) {
    if (lk.day != lkr_wday) {
        lkr_wday = lk.day;
        unsigned int h = lkr_hash(lk.day + 1, lk.seed + 7);
        lkr_rain  = ((h % 5u) < 2u) ? 1 : 0;         // rain on ~2 days in 5
        lkr_wstart = (int)((h >> 8) % 18u);
        lkr_wlen   = 2 + (int)((h >> 16) % 4u);
    }
    bool on = lkr_rain && lk.clock >= (float)lkr_wstart
                       && lk.clock <  (float)(lkr_wstart + lkr_wlen);
    float target = on ? 1.0f : 0.0f;
    if (lkr_wet < target) lkr_wet = clamp(lkr_wet + d * 0.20f, 0.0f, 1.0f);
    else if (lkr_wet > target) lkr_wet = clamp(lkr_wet - d * 0.16f, 0.0f, 1.0f);
}

static void lkr_drops_init(void) {
    for (int i = 0; i < LKR_MAXDROP; i++) {
        lkr_drop[i].x   = lkr_frand() * 1024.0f;
        lkr_drop[i].y   = lkr_frand() * 1024.0f;
        lkr_drop[i].len = 3.0f + lkr_frand() * 4.0f;
        lkr_drop[i].spd = 320.0f + lkr_frand() * 260.0f;
    }
    lkr_drops_ready = 1;
}

// ═══ 9. PARTICLES ════════════════════════════════════════════════════════════
void lk_puff(float wx, float wy, float size, float life, int colour) {
    int kind = 0;
    if (colour == CLR_RED || colour == CLR_DARK_RED || colour == CLR_DARK_PURPLE) kind = 1;
    else if (colour == CLR_YELLOW || colour == CLR_LIGHT_YELLOW || colour == CLR_WHITE) kind = 2;
    int n = kind == 1 ? 5 : kind == 2 ? 4 : 3;
    if (size > 6.0f) n += 2;
    for (int i = 0; i < n; i++) {
        LkrPart *q;
        if (lkr_pn < LKR_MAXPART) {
            q = &lkr_p[lkr_pn++];
        } else {                                     // pool full: take the faintest
            int worst = 0;
            for (int k = 1; k < LKR_MAXPART; k++)
                if (lkr_p[k].life < lkr_p[worst].life) worst = k;
            q = &lkr_p[worst];
        }
        q->x = wx + (lkr_frand() - 0.5f) * size * 0.6f;
        q->y = wy + (lkr_frand() - 0.5f) * size * 0.6f;
        q->vx = (lkr_frand() - 0.5f) * size * 3.0f;
        q->vy = (lkr_frand() - 0.5f) * size * 3.0f - (kind == 0 ? size : 0.0f);
        q->life = q->max = life * (0.6f + lkr_frand() * 0.7f);
        q->size = 1.0f + lkr_frand() * (size * 0.28f);
        q->col = (unsigned char)colour;
        q->kind = (unsigned char)kind;
    }
}

static void lkr_part_update(float d) {
    for (int i = 0; i < lkr_pn; ) {
        LkrPart *q = &lkr_p[i];
        q->life -= d;
        if (q->life <= 0.0f) { lkr_p[i] = lkr_p[--lkr_pn]; continue; }
        q->x += q->vx * d;
        q->y += q->vy * d;
        if (q->kind == 0) { q->vy -= 6.0f * d; q->vx *= 0.94f; q->size += 6.0f * d; }
        else if (q->kind == 1) { q->vy += 42.0f * d; q->vx *= 0.90f; }
        else { q->vy += 90.0f * d; q->vx *= 0.88f; }
        i++;
    }
}

// ═══ 10. lk_art_world ════════════════════════════════════════════════════════
void lk_art_world(int camx, int camy, int vw, int vh) {
    lkr_vx = camx; lkr_vy = camy; lkr_vw = vw; lkr_vh = vh;
    lkr_frame++;
    lkr_range(camx, camy, vw, vh);

    if (lkr_light_dirty || (lkr_frame % LKR_BAKE_FRAMES) == 0) {
        float sun = lkr_sun();
        lkr_night = 1.0f - sun;
        lkr_amb_out = (int)((0.10f + 0.90f * sun) * (1.0f - 0.34f * lkr_wet) * 255.0f);
        lkr_amb_in  = (int)((0.18f + 0.78f * sun) * (1.0f - 0.10f * lkr_wet) * 255.0f);
        if (lkr_amb_out > 255) lkr_amb_out = 255;
        if (lkr_amb_in  > 255) lkr_amb_in  = 255;
        for (int c = 0; c < LK_N; c++) {
            lk_t[c].light = (unsigned char)(lk_indoors(c) ? lkr_amb_in : lkr_amb_out);
            lkr_warm[c] = 0;
        }
        for (int c = 0; c < LK_N; c++) {
            const Tile *t = &lk_t[c];
            if (t->door != DR_NONE && t->door_open > 120 && lk_t[c].light < lkr_amb_out)
                lk_t[c].light = (unsigned char)lkr_amb_out;
            if (t->obj == OB_NONE || t->obj_ref) continue;
            for (int i = 0; i < LKR_NLAMPDEF; i++) {
                if (LKR_LAMP[i].obj != t->obj) continue;
                lkr_spill_static(c, LKR_LAMP[i].rad, LKR_LAMP[i].str, LKR_LAMP[i].warm);
                break;
            }
        }
        lkr_lamp_gather();
        lkr_light_dirty = 0;
        lkr_light_t = 0.0f;
    }
    lkr_dyn_stamp();

    // ── pass 1: floors, opaque (a black pixel in a floor sprite stays black) ──
    colorkey(-1);
    for (int y = lkr_ty0; y <= lkr_ty1; y++) {
        int py = y * LK_TS;
        for (int x = lkr_tx0; x <= lkr_tx1; x++) {
            const Tile *t = &lk_t[lk_idx(x, y)];
            int f = t->floor < FL_COUNT ? t->floor : FL_DIRT;
            spr(LK_FLOOR_SPR[f][t->var & 1], x * LK_TS, py);
        }
    }
    // ── pass 1b: the ground at three scales — AREA field first, because the
    // per-tile detail reads it; then detail, the area scatter over it, and the
    // material fringe last so a boundary stays crisp under all of it.
    lkr_ground_field();
    for (int y = lkr_ty0; y <= lkr_ty1; y++)
        for (int x = lkr_tx0; x <= lkr_tx1; x++) {
            int c = lk_idx(x, y);
            if (lk_t[c].wall != WL_NONE) continue;    // the structure pass covers it
            int px = x * LK_TS, py = y * LK_TS;
            lkr_floor_detail(c, px, py);
            lkr_ground_scatter(c, px, py, lk_t[c].floor);
            lkr_floor_edges(c, px, py);
        }
    lkr_room_paint();

    // ── pass 2: filth, dithered IN rather than tinting the whole tile ────────
    static const unsigned short filth_pat[3] = { LKR_D2, LKR_D4, LKR_D8 };
    static const unsigned char  filth_col[3] = { CLR_DARK_BROWN, CLR_DARK_BROWN, CLR_BROWNISH_BLACK };
    for (int lvl = 0; lvl < 3; lvl++) {
        bool any = false;
        for (int y = lkr_ty0; y <= lkr_ty1 && !any; y++)
            for (int x = lkr_tx0; x <= lkr_tx1; x++) {
                int dd = lk_t[lk_idx(x, y)].dirty;
                int l = dd > 190 ? 2 : dd > 110 ? 1 : dd > 45 ? 0 : -1;
                if (l == lvl) { any = true; break; }
            }
        if (!any) continue;
        fillp(filth_pat[lvl], -1);
        for (int y = lkr_ty0; y <= lkr_ty1; y++)
            for (int x = lkr_tx0; x <= lkr_tx1; x++) {
                int dd = lk_t[lk_idx(x, y)].dirty;
                int l = dd > 190 ? 2 : dd > 110 ? 1 : dd > 45 ? 0 : -1;
                if (l != lvl) continue;
                rectfill(x * LK_TS, y * LK_TS, LK_TS, LK_TS, filth_col[lvl]);
            }
        fillp_reset();
    }

    // ── pass 3: every caster stamps its footprint to the south-east ──────────
    lkr_shadow_pass();

    // ── pass 4: the structure ───────────────────────────────────────────────
    for (int y = lkr_ty0; y <= lkr_ty1; y++) {
        for (int x = lkr_tx0; x <= lkr_tx1; x++) {
            int c = lk_idx(x, y);
            const Tile *t = &lk_t[c];
            if (t->wall != WL_NONE) lkr_wall_body(x * LK_TS, y * LK_TS, t->wall, t->joins, t->var);
            else if (t->door != DR_NONE)
                lkr_door_tile(c, x * LK_TS, y * LK_TS, t->door, t->joins, t->door_open, t->locked);
        }
    }

    // ── pass 5: objects ─────────────────────────────────────────────────────
    colorkey(CLR_BLACK);
    for (int y = lkr_ty0; y <= lkr_ty1; y++)
        for (int x = lkr_tx0; x <= lkr_tx1; x++)
            lkr_obj_tile(lk_idx(x, y), x * LK_TS, y * LK_TS);

    // ── pass 6: night ───────────────────────────────────────────────────────
    lkr_light_pass();
}

// ═══ 11. ACTORS ══════════════════════════════════════════════════════════════
static int lkr_face_dir(float face) {
    // atan2 convention, +y south.  Sprite dirs: 0 S, 1 N, 2 E, 3 W.
    float c = de_cosf(face), s = de_sinf(face);
    if (c < 0) c = -c;
    float as = s < 0 ? -s : s;
    if (c >= as) return de_cosf(face) >= 0 ? 2 : 3;
    return s >= 0 ? 0 : 1;
}
static int lkr_stride(const Actor *a) {
    int k = ((int)a->bob) & 3;
    return k == 3 ? 1 : k;          // 0,1,2,1 — a proper walk cycle
}
static bool lkr_sitting(const Actor *a) {
    if (a->state == AS_EAT) return true;
    if (a->state == AS_SOLITARY) return true;
    if (a->state != AS_USE && a->state != AS_WORK) return false;
    if (a->use_tile < 0 || a->use_tile >= LK_N) return false;
    int ob = lk_t[a->use_tile].obj;
    return ob == OB_BENCH || ob == OB_CHAIR || ob == OB_TOILET
        || ob == OB_TABLE || ob == OB_PHONE || ob == OB_DESK;
}

void lk_art_actors(int camx, int camy, int vw, int vh) {
    lkr_vx = camx; lkr_vy = camy; lkr_vw = vw; lkr_vh = vh;
    static short ord[LK_MAXACT];
    int n = 0;
    int m = LK_TS * 2;
    for (int i = 0; i < lk_nact && n < LK_MAXACT; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive) continue;
        if (a->x < camx - m || a->x > camx + vw + m) continue;
        if (a->y < camy - m || a->y > camy + vh + m) continue;
        ord[n++] = (short)i;
    }
    // painter's order: nearer (larger y) draws last
    for (int i = 1; i < n; i++) {
        short v = ord[i];
        float vy = lk_a[v].y;
        int k = i - 1;
        while (k >= 0 && lk_a[ord[k]].y > vy) { ord[k + 1] = ord[k]; k--; }
        ord[k + 1] = v;
    }

    // ── GROUNDING SHADOWS, one blend scope for all of them ──────────────────
    // A PLANTED shadow (flank.c:628's trick): it sits on the ground at the feet,
    // steps south-east with the module's one light direction, and does NOT move with
    // the walk cycle or the fight jitter — the body bobs, the contact patch does not.
    // At 16px on a textured ground that contact is half of why a prisoner is findable
    // at all, so it is bigger than the token 4×2 dot it used to be.
    //
    // ⚠ ONE oval per actor, never two overlapping ones.  blend() is DELIBERATELY not
    // GPU/software identical (blend-tables.md: the GPU path reads a per-scope
    // SNAPSHOT, the software canvas reads live dst) so overlapping blended shapes
    // accumulate on the canvas and DON'T on the GPU.  A two-oval "pool plus darker
    // core" therefore renders differently on desktop and on web — it also showed up
    // as 108 extra pixels in canvas-diff, which is how it was caught.  Any shading
    // inside a blend scope has to come from ONE shape.
    blend(BLEND_AVG);
    for (int i = 0; i < n; i++) {
        const Actor *a = &lk_a[ord[i]];
        int c = lk_cell_at(a->x, a->y);
        if (lkr_lit_at(lk_tx(c), lk_ty(c)) < 46) continue;      // nothing to cast by
        int ax = (int)a->x, ay = (int)a->y;
        if (a->state == AS_DOWN || a->state == AS_RESTRAINED)
            ovalfill(ax + 3, ay + 4, 8, 4, CLR_BLACK);          // a whole body's worth
        else
            ovalfill(ax + 2, ay + 6, 6, 3, CLR_BLACK);
    }
    blend_reset();

    // ── tethers first, so a body always covers its own leash ────────────────
    for (int i = 0; i < n; i++) {
        const Actor *a = &lk_a[ord[i]];
        if (a->state != AS_ESCORTED || a->escort < 0 || a->escort >= LK_MAXACT) continue;
        const Actor *g = &lk_a[a->escort];
        if (!g->alive) continue;
        line((int)a->x, (int)a->y + 1, (int)g->x, (int)g->y + 1, CLR_LIGHT_GREY);
    }

    // ── bodies, with the palette swapped per actor ───────────────────────────
    colorkey(CLR_BLACK);
    unsigned int cur = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) {
        const Actor *a = &lk_a[ord[i]];
        int c = lk_cell_at(a->x, a->y);
        int lit = lkr_lit_at(lk_tx(c), lk_ty(c));
        int dim = lit > 190 ? 0 : lit > 120 ? 1 : lit > 60 ? 2 : 3;

        int uni = LK_ROLE_UNIFORM[a->role < RL_COUNT ? a->role : 0];
        if (a->role == RL_PRISONER) uni = LKR_SEC_UNIFORM[a->sec < SEC_COUNT ? a->sec : 1];
        int tro = LK_ROLE_TROUSER[a->role < RL_COUNT ? a->role : 0];
        if (a->role == RL_PRISONER && (a->tint & 1)) tro = lkr_dim(tro, 1);
        int skn = LKR_SKIN[a->skin & 3];
        int cap = LKR_ROLE_CAP[a->role < RL_COUNT ? a->role : 0];
        int hir = cap ? cap : LKR_HAIR[a->hair % 5];
        uni = lkr_dim(uni, dim); tro = lkr_dim(tro, dim);
        skn = lkr_dim(skn, dim); hir = lkr_dim(hir, dim);

        unsigned int key = (unsigned int)uni | ((unsigned int)tro << 6)
                         | ((unsigned int)skn << 12) | ((unsigned int)hir << 18)
                         | ((unsigned int)dim << 24);
        if (key != cur) {
            pal_reset();
            pal(LK_MAGIC_UNIFORM, uni);
            pal(LK_MAGIC_TROUSER, tro);
            pal(LK_MAGIC_SKIN,    skn);
            pal(LK_MAGIC_HAIR,    hir);
            if (dim) {                                   // dim the trim too
                pal(CLR_WHITE,      lkr_dim(CLR_WHITE, dim));
                pal(CLR_LIGHT_GREY, lkr_dim(CLR_LIGHT_GREY, dim));
            }
            cur = key;
        }

        int s;
        switch (a->state) {
        case AS_SLEEP:      s = SP_SLEEP; break;
        case AS_DOWN:
        case AS_RESTRAINED: s = SP_DOWN;  break;
        case AS_FIGHT:
        case AS_RIOT:       s = SP_FIGHT; break;
        default:
            s = lkr_sitting(a) ? SP_SIT
              : SP_PERSON + lkr_face_dir(a->face) * 3
                          + (a->state == AS_WALK || a->state == AS_ESCORTED
                             || a->state == AS_ESCORTING || a->state == AS_PATROL
                             || a->state == AS_ESCAPE ? lkr_stride(a) : 0);
            break;
        }
        int jx = 0, jy = 0;
        // a sleeper lies ALONG a two-tile bed, not curled on its pillow tile
        if (a->state == AS_SLEEP && a->use_tile >= 0 && a->use_tile < LK_N) {
            int ob = lk_t[a->use_tile].obj;
            if (ob > OB_NONE && ob < OB_COUNT && LK_OBJ[ob].h > 1) jy += LK_TS / 2;
        }
        if (a->state == AS_FIGHT || a->state == AS_RIOT) {
            unsigned int h = lkr_hash((int)(lkr_time * 24.0f) + a->id, a->id * 31);
            jx = (int)(h & 1u) - (int)((h >> 1) & 1u);
            jy = (int)((h >> 2) & 1u) - (int)((h >> 3) & 1u);
        }
        spr(s, (int)a->x - LK_TS / 2 + jx, (int)a->y - LK_TS / 2 - 2 + jy);
    }
    pal_reset();

    // ── state cues, drawn with a clean palette ──────────────────────────────
    for (int i = 0; i < n; i++) {
        const Actor *a = &lk_a[ord[i]];
        int ax = (int)a->x, ay = (int)a->y;
        switch (a->state) {
        case AS_DOWN: {
            ovalfill(ax, ay + 3, 6, 3, CLR_DARK_RED);
            ovalfill(ax - 2, ay + 4, 3, 1, CLR_RED);
            if (((int)(lkr_time * 2.0f) & 1) == 0) pset(ax, ay - 9, CLR_RED);
            break;
        }
        case AS_RESTRAINED:
            rectfill(ax - 3, ay + 1, 7, 1, CLR_WHITE);            // cuffs
            rectfill(ax - 1, ay,     3, 1, CLR_LIGHT_GREY);
            break;
        case AS_SLEEP: {
            // drifting z's, drawn as a tiny zigzag so there is no text on the map
            float ph = lkr_time * 0.9f + (float)a->id * 0.37f;
            int zy = ay - 9 - (int)(de_sinf(ph) * 2.0f + 2.0f);
            int zx = ax + 4 + (int)(de_cosf(ph * 0.7f) * 1.5f);
            line(zx, zy, zx + 2, zy, CLR_WHITE);
            line(zx + 2, zy, zx, zy + 2, CLR_WHITE);
            line(zx, zy + 2, zx + 2, zy + 2, CLR_WHITE);
            break;
        }
        case AS_FIGHT: {
            float ph = lkr_time * 9.0f + (float)a->id;
            int r = 3 + (int)(de_sinf(ph) * 1.6f + 1.6f);
            starfill(ax + 4, ay - 6, r, r / 2 + 1, 5, ph * 30.0f, CLR_YELLOW);
            starfill(ax + 4, ay - 6, r / 2, 1, 5, ph * 30.0f, CLR_WHITE);
            break;
        }
        case AS_RIOT: {
            float ph = lkr_time * 6.0f + (float)a->id * 0.9f;
            int r = 7 + (int)(de_sinf(ph) * 1.5f);
            circ(ax, ay, r, CLR_RED);
            pset(ax - 4, ay - 8, CLR_DARK_RED);
            pset(ax + 4, ay - 8, CLR_DARK_RED);
            break;
        }
        case AS_ESCAPE: {
            int t = (int)(lkr_time * 6.0f) & 1;
            trifill(ax, ay - 11 - t, ax - 3, ay - 7 - t, ax + 3, ay - 7 - t, CLR_RED);
            break;
        }
        case AS_ESCORTED:
            rectfill(ax - 3, ay + 1, 7, 1, CLR_WHITE);
            break;
        case AS_WASH: {
            unsigned int h = lkr_hash((int)(lkr_time * 30.0f), a->id);
            for (int k = 0; k < 3; k++)
                pset(ax - 3 + (int)((h >> (k * 3)) % 7u), ay - 6 + (int)((h >> (k * 4 + 2)) % 9u), CLR_BLUE);
            break;
        }
        case AS_COOK:
        case AS_WORK: {
            if (((int)(lkr_time * 4.0f) & 1) == 0) {
                rectfill(ax + 4, ay - 7, 3, 1, CLR_MEDIUM_GREY);
                pset(ax + 6, ay - 8, CLR_LIGHT_GREY);
            }
            break;
        }
        case AS_TREAT:
            rectfill(ax + 3, ay - 8, 3, 1, CLR_WHITE);
            rectfill(ax + 4, ay - 9, 1, 3, CLR_WHITE);
            break;
        default: break;
        }
        // injured but still standing: a short red tick under the feet
        if (a->health < 0.62f && a->state != AS_DOWN) {
            int wpx = 1 + (int)(a->health * 6.0f);
            rectfill(ax - 3, ay + 7, 6, 1, CLR_DARKER_GREY);
            rectfill(ax - 3, ay + 7, wpx, 1, a->health < 0.35f ? CLR_RED : CLR_ORANGE);
        }
        // contraband: a single dark pip, so a shakedown has something to find
        if (a->contraband && a->role == RL_PRISONER && ((int)(lkr_time * 1.5f) & 1) == 0)
            pset(ax - 5, ay - 5, CLR_DARK_PURPLE);
    }

    // the inspector's target
    if (lk_sel_actor >= 0 && lk_sel_actor < LK_MAXACT && lk_a[lk_sel_actor].alive) {
        const Actor *a = &lk_a[lk_sel_actor];
        int ax = (int)a->x, ay = (int)a->y;
        int ph = (int)(lkr_time * 8.0f);
        for (int k = 0; k < 12; k++) {
            if (((k + ph) & 1) == 0) continue;
            float t = (float)k * (6.2831853f / 12.0f);
            pset(ax + (int)(de_cosf(t) * 9.0f), ay + (int)(de_sinf(t) * 9.0f), CLR_WHITE);
        }
    }
}

// ═══ 12. OVERLAYS ════════════════════════════════════════════════════════════
static void lkr_heat_refresh(int mode) {
    int hw = LKR_HW;
    // guards, gathered once instead of re-scanned per lattice cell
    static float gx[64], gy[64];
    int ng = 0;
    if (mode == OV_SAFETY) {
        for (int i = 0; i < lk_nact && ng < 64; i++) {
            const Actor *a = &lk_a[i];
            if (!a->alive || a->role != RL_GUARD || a->state == AS_DOWN) continue;
            gx[ng] = a->x; gy[ng] = a->y; ng++;
        }
    }
    int hx0 = lkr_max(0, lkr_tx0 / LKR_HSTEP - 1), hx1 = lkr_min(hw - 1, lkr_tx1 / LKR_HSTEP + 2);
    int hy0 = lkr_max(0, lkr_ty0 / LKR_HSTEP - 1), hy1 = lkr_min(LKR_HH - 1, lkr_ty1 / LKR_HSTEP + 2);
    for (int hy = hy0; hy <= hy1; hy++) {
        for (int hx = hx0; hx <= hx1; hx++) {
            int x = hx * LKR_HSTEP, y = hy * LKR_HSTEP;
            if (!lk_in(x, y)) { lkr_heat[hy * hw + hx] = 0; continue; }
            int c = lk_idx(x, y);
            float v = 0.0f;
            if (mode == OV_NEEDS) {
                v = lk_block_tension(c, 4);
            } else {
                // OV_SAFETY: how far is the nearest guard, in tiles.  Only somewhere
                // a prisoner can be — an empty field needs no guard.
                if (lk_room_of(c) == 0 && !lk_indoors(c)) { lkr_heat[hy * hw + hx] = 0; continue; }
                float best = 1e9f;
                for (int i = 0; i < ng; i++) {
                    float dx = gx[i] - (float)lk_cx(c), dy = gy[i] - (float)lk_cy(c);
                    float dd = dx * dx + dy * dy;
                    if (dd < best) best = dd;
                }
                float tiles = fsqrt(best) / (float)LK_TS;
                v = clamp((tiles - 3.0f) / 11.0f, 0.0f, 1.0f);
            }
            int iv = (int)(v * 255.0f);
            lkr_heat[hy * hw + hx] = (unsigned char)(iv < 0 ? 0 : iv > 255 ? 255 : iv);
        }
    }
    lkr_heat_t = 0.0f;
    lkr_heat_mode = mode;
    lkr_heat_ox = lkr_tx0;
    lkr_heat_oy = lkr_ty0;
}

// bilinear read of the lattice at a TILE, so the heatmap doesn't read as 48-px blocks
static int lkr_heat_at(int x, int y) {
    int hw = LKR_HW;
    int hx = x / LKR_HSTEP, hy = y / LKR_HSTEP;
    int fx = x - hx * LKR_HSTEP, fy = y - hy * LKR_HSTEP;
    if (hx + 1 >= hw) hx = hw - 2;
    if (hy + 1 >= LKR_HH) hy = LKR_HH - 2;
    if (hx < 0) hx = 0;
    if (hy < 0) hy = 0;
    int a = lkr_heat[hy * hw + hx],       b = lkr_heat[hy * hw + hx + 1];
    int c2 = lkr_heat[(hy + 1) * hw + hx], d = lkr_heat[(hy + 1) * hw + hx + 1];
    int top = a * (LKR_HSTEP - fx) + b * fx;
    int bot = c2 * (LKR_HSTEP - fx) + d * fx;
    return (top * (LKR_HSTEP - fy) + bot * fy) / (LKR_HSTEP * LKR_HSTEP);
}

void lk_art_overlay(int mode, int camx, int camy, int vw, int vh) {
    if (mode <= OV_NONE || mode >= OV_COUNT) return;
    lkr_vx = camx; lkr_vy = camy; lkr_vw = vw; lkr_vh = vh;
    lkr_range(camx, camy, vw, vh);

    if (mode == OV_ROOMS) {
        // the fill: dithered so the world stays visible underneath
        fillp(LKR_D4, -1);
        for (int y = lkr_ty0; y <= lkr_ty1; y++)
            for (int x = lkr_tx0; x <= lkr_tx1; x++) {
                const Tile *t = &lk_t[lk_idx(x, y)];
                int rid = t->room, ty2 = RM_NONE;
                if (rid > 0 && rid < LK_MAXROOM) ty2 = lk_room[rid].type;
                else if (t->paint) ty2 = t->paint;
                if (ty2 == RM_NONE || ty2 >= RM_COUNT) continue;
                rectfill(x * LK_TS, y * LK_TS, LK_TS, LK_TS, LK_ROOM[ty2].colour);
            }
        // pending paint that has NOT become a room yet: a red hatch, "not a room"
        fillp(LKR_HATCH, -1);
        for (int y = lkr_ty0; y <= lkr_ty1; y++)
            for (int x = lkr_tx0; x <= lkr_tx1; x++) {
                const Tile *t = &lk_t[lk_idx(x, y)];
                if (!t->paint || t->room) continue;
                rectfill(x * LK_TS, y * LK_TS, LK_TS, LK_TS, CLR_RED);
            }
        fillp_reset();
        // borders: an outline is what makes a fill read as a ROOM
        for (int y = lkr_ty0; y <= lkr_ty1; y++)
            for (int x = lkr_tx0; x <= lkr_tx1; x++) {
                int c = lk_idx(x, y);
                int rid = lk_t[c].room;
                if (rid <= 0 || rid >= LK_MAXROOM) continue;
                const Room *rm = &lk_room[rid];
                int col = rm->valid ? LK_ROOM[rm->type < RM_COUNT ? rm->type : 0].colour : CLR_RED;
                int px = x * LK_TS, py = y * LK_TS;
                if (!lk_in(x, y - 1) || lk_t[lk_idx(x, y - 1)].room != rid)
                    rectfill(px, py, LK_TS, 1, col);
                if (!lk_in(x, y + 1) || lk_t[lk_idx(x, y + 1)].room != rid)
                    rectfill(px, py + LK_TS - 1, LK_TS, 1, col);
                if (!lk_in(x - 1, y) || lk_t[lk_idx(x - 1, y)].room != rid)
                    rectfill(px, py, 1, LK_TS, col);
                if (!lk_in(x + 1, y) || lk_t[lk_idx(x + 1, y)].room != rid)
                    rectfill(px + LK_TS - 1, py, 1, LK_TS, col);
            }
        // labels + the reason a room is invalid
        font(FONT_SMALL);
        for (int r = 1; r < lk_nroom && r < LK_MAXROOM; r++) {
            const Room *rm = &lk_room[r];
            if (rm->type == RM_NONE || rm->type >= RM_COUNT) continue;
            if (rm->cx < lkr_tx0 || rm->cx > lkr_tx1 || rm->cy < lkr_ty0 || rm->cy > lkr_ty1) continue;
            int lx = rm->cx * LK_TS + LK_TS / 2, ly = rm->cy * LK_TS - 4;
            const char *nm = LK_ROOM[rm->type].name;
            print_outline(nm, lx - text_width(nm) / 2, ly,
                          rm->valid ? CLR_WHITE : CLR_RED, CLR_BROWNISH_BLACK);
            if (!rm->valid) {
                const char *why = rm->leaks ? "not enclosed"
                                : (rm->missing && rm->missing < OB_COUNT)
                                    ? LK_OBJ[rm->missing].name
                                    : (rm->area < LK_ROOM[rm->type].min_area ? "too small" : "invalid");
                const char *pre = rm->leaks ? "" : (rm->missing ? "needs " : "");
                int wpx = text_width(pre) + text_width(why);
                int bx = lx - wpx / 2;
                if (*pre) bx = print_outline(pre, bx, ly + 7, CLR_ORANGE, CLR_BROWNISH_BLACK);
                print_outline(why, bx, ly + 7, CLR_ORANGE, CLR_BROWNISH_BLACK);
                // an unmistakable cross at the centroid
                int mx = rm->cx * LK_TS + LK_TS / 2, my = rm->cy * LK_TS + LK_TS / 2 + 8;
                line(mx - 3, my - 3, mx + 3, my + 3, CLR_RED);
                line(mx + 3, my - 3, mx - 3, my + 3, CLR_RED);
            } else if (rm->cap > 0) {
                const char *cp = lkr_ratio(rm->used, rm->cap);
                print_outline(cp, lx - text_width(cp) / 2, ly + 7,
                              rm->used >= rm->cap ? CLR_ORANGE : CLR_LIGHT_GREY,
                              CLR_BROWNISH_BLACK);
            }
        }
        font(FONT_NORMAL);
        return;
    }

    if (mode == OV_DEPLOY) {
        fillp(LKR_HATCH, -1);
        for (int y = lkr_ty0; y <= lkr_ty1; y++)
            for (int x = lkr_tx0; x <= lkr_tx1; x++) {
                int z = lk_t[lk_idx(x, y)].zone;
                if (z == ZN_OPEN) continue;
                rectfill(x * LK_TS, y * LK_TS, LK_TS, LK_TS,
                         z == ZN_STAFF ? CLR_TRUE_BLUE : CLR_DARK_RED);
            }
        fillp_reset();
        // zone borders
        for (int y = lkr_ty0; y <= lkr_ty1; y++)
            for (int x = lkr_tx0; x <= lkr_tx1; x++) {
                int z = lk_t[lk_idx(x, y)].zone;
                if (z == ZN_OPEN) continue;
                int col = z == ZN_STAFF ? CLR_BLUE : CLR_RED;
                int px = x * LK_TS, py = y * LK_TS;
                if (!lk_in(x, y - 1) || lk_t[lk_idx(x, y - 1)].zone != z) rectfill(px, py, LK_TS, 1, col);
                if (!lk_in(x, y + 1) || lk_t[lk_idx(x, y + 1)].zone != z) rectfill(px, py + LK_TS - 1, LK_TS, 1, col);
                if (!lk_in(x - 1, y) || lk_t[lk_idx(x - 1, y)].zone != z) rectfill(px, py, 1, LK_TS, col);
                if (!lk_in(x + 1, y) || lk_t[lk_idx(x + 1, y)].zone != z) rectfill(px + LK_TS - 1, py, 1, LK_TS, col);
            }
        // every guard: where they are, where they are going, and how far they reach
        for (int i = 0; i < lk_nact; i++) {
            const Actor *a = &lk_a[i];
            if (!a->alive || a->role != RL_GUARD) continue;
            if (a->x < camx - 64 || a->x > camx + vw + 64) continue;
            if (a->y < camy - 64 || a->y > camy + vh + 64) continue;
            circ((int)a->x, (int)a->y, LKR_GUARD_PX, CLR_TRUE_BLUE);
            for (int k = a->pi; k + 1 < a->plen; k += 2) {
                int c0 = a->path[k], c1 = a->path[k + 1];
                if (c0 < 0 || c0 >= LK_N || c1 < 0 || c1 >= LK_N) continue;
                line(lk_cx(c0), lk_cy(c0), lk_cx(c1), lk_cy(c1), CLR_BLUE);
            }
            if (a->target >= 0 && a->target < LK_N) {
                int tx = lk_cx(a->target), ty2 = lk_cy(a->target);
                rect(tx - 3, ty2 - 3, 7, 7, CLR_BLUE);
                pset(tx, ty2, CLR_WHITE);
            }
            circfill((int)a->x, (int)a->y - 1, 2, CLR_BLUE);
        }
        return;
    }

    // OV_NEEDS / OV_SAFETY: a coarse lattice, sampled at 4 Hz, drawn per tile
    if (lkr_heat_mode != mode || lkr_heat_t > 0.25f
        || lkr_heat_ox != lkr_tx0 || lkr_heat_oy != lkr_ty0) lkr_heat_refresh(mode);
    static const unsigned short heat_pat[5] = { LKR_D2, LKR_D2, LKR_D4, LKR_D8, LKR_D12 };
    // one interpolation pass, then one dithered fill per level
    static unsigned char lvlbuf[LK_N];
    int worst = -1, wv = 150;
    for (int y = lkr_ty0; y <= lkr_ty1; y++)
        for (int x = lkr_tx0; x <= lkr_tx1; x++) {
            int v = lkr_heat_at(x, y);
            int c = lk_idx(x, y);
            lvlbuf[c] = (unsigned char)(v <= 26 ? 0 : v <= 76 ? 1 : v <= 128 ? 2 : v <= 190 ? 3 : 4);
            if (v > wv) { wv = v; worst = c; }
        }
    for (int lvl = 1; lvl <= 4; lvl++) {
        fillp(heat_pat[lvl], -1);
        for (int y = lkr_ty0; y <= lkr_ty1; y++)
            for (int x = lkr_tx0; x <= lkr_tx1; x++) {
                int c = lk_idx(x, y);
                if (lvlbuf[c] != lvl) continue;
                rectfill(x * LK_TS, y * LK_TS, LK_TS, LK_TS, LKR_HEAT[lvl]);
            }
    }
    fillp_reset();
    // mark the single worst spot so the eye goes straight there
    if (worst >= 0) {
        colorkey(CLR_BLACK);
        spr(mode == OV_NEEDS ? SP_ICON_SKULL : SP_ICON_ALARM,
            lk_tx(worst) * LK_TS, lk_ty(worst) * LK_TS);
    }
    if (mode == OV_SAFETY) {
        for (int i = 0; i < lk_nact; i++) {
            const Actor *a = &lk_a[i];
            if (!a->alive || a->role != RL_GUARD) continue;
            circ((int)a->x, (int)a->y, 5, CLR_GREEN);
            pset((int)a->x, (int)a->y, CLR_WHITE);
        }
    }
}

// ═══ 13. FX — particles, rain, the alarm ═════════════════════════════════════
void lk_art_fx(int camx, int camy) {
    // reuses the view rect lk_art_world was last given; a defensive default keeps
    // the rain wrap below from spinning if a caller ever hands us a zero viewport
    int vw = lkr_vw > 0 ? lkr_vw : 320, vh = lkr_vh > 0 ? lkr_vh : 200;
    lkr_vx = camx; lkr_vy = camy;

    // ── particles ───────────────────────────────────────────────────────────
    for (int i = 0; i < lkr_pn; i++) {
        const LkrPart *q = &lkr_p[i];
        if (q->x < camx - 16 || q->x > camx + vw + 16) continue;
        if (q->y < camy - 16 || q->y > camy + vh + 16) continue;
        float f = q->max > 0.0f ? q->life / q->max : 0.0f;
        int col = q->col;
        if (f < 0.34f) col = lkr_dim(col, 2);
        else if (f < 0.66f) col = lkr_dim(col, 1);
        if (q->kind == 0) {
            int r = (int)(q->size * (0.5f + f * 0.9f));
            if (r <= 0) { pset((int)q->x, (int)q->y, col); continue; }
            fillp(f > 0.5f ? LKR_D8 : LKR_D4, -1);
            circfill((int)q->x, (int)q->y, r, col);
            fillp_reset();
        } else if (q->kind == 1) {
            pset((int)q->x, (int)q->y, col);
            if (q->size > 2.0f) pset((int)q->x + 1, (int)q->y, col);
        } else {
            line((int)q->x, (int)q->y, (int)(q->x - q->vx * 0.03f), (int)(q->y - q->vy * 0.03f), col);
        }
    }

    // ── rain, only where the sky can reach ──────────────────────────────────
    if (lkr_wet > 0.02f) {
        int count = (int)(LKR_MAXDROP * lkr_wet);
        int slant = 3;
        for (int i = 0; i < count; i++) {
            float sx = lkr_drop[i].x, sy = lkr_drop[i].y;
            while (sx > (float)vw) sx -= (float)vw;
            while (sy > (float)vh) sy -= (float)vh;
            int wx = camx + (int)sx, wy = camy + (int)sy;
            int c = lk_cell_at((float)wx, (float)wy);
            if (lk_indoors(c)) continue;
            int len = (int)lkr_drop[i].len;
            line(wx, wy, wx - slant, wy + len, (i & 3) ? CLR_BLUE : CLR_LIGHT_GREY);
            if ((i & 7) == 0) {                    // a splash where it lands
                pset(wx - slant - 1, wy + len + 1, CLR_LIGHT_GREY);
                pset(wx - slant + 1, wy + len + 1, CLR_LIGHT_GREY);
            }
        }
    }

    // ── the alarm: a red pulse around the edge of the view, never a wash ─────
    if (lk.alarm >= AL_RIOT) {
        float ph = de_sinf(lkr_time * 6.0f) * 0.5f + 0.5f;
        int bands = 1 + (int)(ph * 3.0f);
        fillp(LKR_D8, -1);
        for (int k = 0; k < bands; k++) {
            int in = k * 3;
            rectfill(camx + in, camy + in, vw - in * 2, 3, CLR_RED);
            rectfill(camx + in, camy + vh - in - 3, vw - in * 2, 3, CLR_RED);
            rectfill(camx + in, camy + in, 3, vh - in * 2, CLR_RED);
            rectfill(camx + vw - in - 3, camy + in, 3, vh - in * 2, CLR_RED);
        }
        fillp_reset();
    }
}

// ═══ 14. THE BUILD GHOST ═════════════════════════════════════════════════════
static int lkr_ghost_joins(int c, int mat) {
    // what mask WOULD this tile have?  So the preview shows the join it will make.
    int x = lk_tx(c), y = lk_ty(c), m = 0;
    static const int ox[8] = { 0, 1, 0, -1, 1, 1, -1, -1 };
    static const int oy[8] = { -1, 0, 1, 0, -1, 1, 1, -1 };
    bool mesh = (mat == WL_FENCE || mat == WL_PERIM);
    for (int b = 0; b < 8; b++) {
        int nx = x + ox[b], ny = y + oy[b];
        if (!lk_in(nx, ny)) continue;
        const Tile *t = &lk_t[lk_idx(nx, ny)];
        if (t->door != DR_NONE) { m |= 1 << b; continue; }
        if (t->wall == WL_NONE) continue;
        bool nmesh = (t->wall == WL_FENCE || t->wall == WL_PERIM);
        if (nmesh == mesh) m |= 1 << b;
    }
    return m;
}

void lk_art_ghost(int c, int job, int arg, bool ok) {
    if (c < 0 || c >= LK_N || job <= JB_NONE || job >= JB_COUNT) return;
    int px = lk_tx(c) * LK_TS, py = lk_ty(c) * LK_TS;
    int w = 1, h = 1;
    if (job == JB_OBJECT && arg > OB_NONE && arg < OB_COUNT) {
        w = LK_OBJ[arg].w; h = LK_OBJ[arg].h;
    }
    int tint = ok ? CLR_GREEN : CLR_RED;

    // what it will look like
    colorkey(CLR_BLACK);
    switch (job) {
    case JB_FLOOR:
        if (arg >= 0 && arg < FL_COUNT) {
            colorkey(-1);
            spr(LK_FLOOR_SPR[arg][lk_t[c].var & 1], px, py);
            colorkey(CLR_BLACK);
        }
        break;
    case JB_WALL:
        if (arg > WL_NONE && arg < WL_COUNT)
            lkr_wall_body(px, py, arg, lkr_ghost_joins(c, arg), lk_t[c].var);
        break;
    case JB_DOOR:
        if (arg > DR_NONE && arg < DR_COUNT)
            lkr_door_tile(c, px, py, arg, lkr_ghost_joins(c, WL_CONCRETE), 0, 0);
        break;
    case JB_OBJECT:
        if (arg > OB_NONE && arg < OB_COUNT)
            for (int dy = 0; dy < h; dy++)
                for (int dx = 0; dx < w; dx++)
                    spr(LK_OBJ[arg].sprite + dy * 8 + dx, px + dx * LK_TS, py + dy * LK_TS);
        break;
    default: break;
    }

    // the verdict, over the top: pulsing dither + a hard outline
    float ph = de_sinf(lkr_time * 7.0f) * 0.5f + 0.5f;
    fillp(ph > 0.5f ? LKR_D4 : LKR_D2, -1);
    rectfill(px, py, w * LK_TS, h * LK_TS, tint);
    fillp_reset();
    rect(px, py, w * LK_TS, h * LK_TS, tint);
    // corner ticks so the footprint reads even over busy art
    for (int sy = 0; sy < 2; sy++)
        for (int sx = 0; sx < 2; sx++) {
            int cx = px + (sx ? w * LK_TS - 3 : 0), cy = py + (sy ? h * LK_TS - 3 : 0);
            rectfill(cx, cy, 3, 1, CLR_WHITE);
            rectfill(cx, cy, 1, 3, CLR_WHITE);
        }
    if (job == JB_DEMOLISH || !ok) {
        line(px, py, px + w * LK_TS - 1, py + h * LK_TS - 1, ok ? CLR_ORANGE : CLR_RED);
        line(px + w * LK_TS - 1, py, px, py + h * LK_TS - 1, ok ? CLR_ORANGE : CLR_RED);
    }
}

// ═══ 15. LIFECYCLE ═══════════════════════════════════════════════════════════
void lk_art_init(void) {
    lkr_rs = 0x9E3779B9u ^ (unsigned int)(lk.seed * 2654435761u);
    if (!lkr_rs) lkr_rs = 1u;
    lkr_pn = 0;
    lkr_dynn = 0;
    lkr_epoch = 0;
    lkr_frame = 0;
    lkr_time = 0.0f;
    lkr_wet = 0.0f;
    lkr_rain = 0;
    lkr_wday = -0x7fff;
    lkr_light_dirty = 1;
    lkr_light_t = 0.0f;
    lkr_heat_t = 999.0f;
    lkr_heat_mode = -1;
    for (int c = 0; c < LK_N; c++) { lkr_warm[c] = 0; lkr_dyn[c] = 0; lkr_stamp[c] = 0; }
    lkr_drops_init();
    colorkey(CLR_BLACK);
}

void lk_art_update(float d) {
    if (d < 0.0f) d = 0.0f;
    if (d > 0.25f) d = 0.25f;              // a hitch must not teleport the rain
    lkr_time += d;
    lkr_weather_update(d);
    lkr_part_update(d);
    lkr_heat_t += d;

    // rain field, in view-relative coords (rain falls with the world, not the map)
    if (lkr_wet > 0.0f) {
        if (!lkr_drops_ready) lkr_drops_init();
        float vwf = (float)(lkr_vw > 0 ? lkr_vw : 320);
        float vhf = (float)(lkr_vh > 0 ? lkr_vh : 200);
        for (int i = 0; i < LKR_MAXDROP; i++) {
            lkr_drop[i].y += lkr_drop[i].spd * d;
            lkr_drop[i].x -= lkr_drop[i].spd * 0.30f * d;
            if (lkr_drop[i].y > vhf) { lkr_drop[i].y -= vhf; lkr_drop[i].x = lkr_frand() * vwf; }
            if (lkr_drop[i].x < 0.0f) lkr_drop[i].x += vwf;
        }
    }

    // the light bake cadence.  lk_art_world has a frame-counter safety net for the
    // paused case, where d can legitimately be 0.
    lkr_light_t += d;
    if (lkr_light_t >= LKR_BAKE_SECS) lkr_light_dirty = 1;
}

#endif // LOCKUP_ART_H
