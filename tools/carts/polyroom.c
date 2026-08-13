/* de:meta
{
  "title": "polyroom",
  "slug": "polyroom",
  "kind": ["probe", "tech-demo"],
  "teaches": ["software-rasterizer", "isometric-projection"],
  "created": "2026-08-13",
  "lineage": "A look probe for tenement, whose maker's verdict was that the picture reads as an architectural diagram with a caption. Asks one question: does the same flat, rendered as low-poly flat-shaded triangles instead of baked voxel sprites, read as FOUR PEOPLE IN A BUILDING? Renderer chassis from solid3d (rot/cull/light/sort/trifill + the fillp checker half-shade); shapes transcribed from tools/voxel-models/tenement.js so the comparison is of RENDERINGS, not of two different object sets.",
  "todo": [
    "PERSPECTIVE is not implemented and the seam is marked in pr_project: this is a stylized shear (the ground squash and the height scale are two independent numbers, inherited from the voxel bake's 2px-per-voxel convention, which no single orthographic elevation can produce). A real perspective divide is a different projection and a different question; add it only if the ortho look wins first.",
    "Instance yaw is not implemented — every object is placed in its authored orientation, and the room was laid out to suit that. The voxel set solves this by baking NS and EW wall variants; a poly set would rotate the mesh, which is one of the things this probe exists to prove is cheaper.",
    "Tri count is reported but frame cost is not. If the look wins, measure before wiring it in: node tools/profile-fleet.js polyroom, and canvas-diff for GPU-vs-software agreement (the 3D path is the one place they have historically diverged).",
    "The shapes are a FIRST TRANSCRIPTION, deliberately not a furniture catalogue, exactly as the voxel set is. Judge the READ (can you count the people, whose flat is that, is the loom machinery) and not the modelling."
  ],
  "description": {
    "summary": "One flat, two renderers, one key between them: low-poly triangles against the baked voxel sprites. Which one looks like people live there?",
    "detail": "tenement's problem is not its simulation, it is that you cannot see who lives in the building: at 24 pixels a resident is a one-tile blob in the same colour family as the furniture. This probe rebuilds the identical scene as flat-shaded low-poly triangles and puts the two renderings one keypress apart, so the comparison is a picture rather than an argument. Both halves walk the SAME item list at the SAME scale through the SAME projection, and at yaw 45 degrees the polygon projection reproduces the voxel one exactly, which is what makes TAB a fair test. What triangles buy: any camera angle instead of four baked ones, diagonals (a sofa back rakes, a torso widens into shoulders, a loom is skeletal), and lighting that can be pinned to the screen or left in the world. What they cost: crisp snapped edges, which crawl once the angle is not one the sprites were baked at.",
    "controls": "TAB flips POLY / VOXEL. Q and E orbit (the voxel half snaps to its nearest baked angle, because it only has four). W and S raise and lower the camera. Z cycles zoom 1x 2x 3x. D toggles the dither half-shades, L moves the light between world-fixed and screen-fixed, H toggles per-household colour on the residents, SPACE resets the camera."
  }
}
de:meta */

// polyroom — the same room twice: flat-shaded triangles, and the voxel sprites tenement ships.
//
// WHY THIS CART EXISTS. tenement cleared every oracle it has (242 assertions, canvas-diff and
// ui-audit clean) and still failed the half of the ADR-0022 bar no oracle checks: it reads as an
// architectural diagram with a caption. The residents are the least visible thing in a game about
// residents. Before spending that budget on rims and glyphs inside a cart with 242 assertions
// pinned to it, this asks whether the whole rendering is the wrong one.
//
// THE ONE RULE THAT MAKES THE COMPARISON MEAN ANYTHING: there is ONE scene. Both renderers walk
// the same `pr_item[]` list, at the same scale, through the same projection function. At yaw 45
// the polygon projection reproduces the voxel bake's exactly (verified below), so TAB changes the
// RENDERING and nothing else. A probe that quietly gave the new look a better camera, a bigger
// scale or nicer furniture would answer a question nobody asked.
//
// WHY IT #includes runtime/tenement/atlas.h, which that module reserves for tenement/art.h.
// A deliberate exception with a reason: the voxel half has to be the REAL one, and copying the
// generated rect table would put a silently-drifting duplicate in the repo. atlas.h is generated,
// read-only, has no dependency on the contract, and carries ISO_SIG — which polyroom.cart.js
// re-derives, so a stale header is a loud build failure rather than furniture drawn with last
// week's rectangles. This is a probe FOR tenement; the coupling is the point. It is NOT licence to
// include a tenement module from anywhere else — those are one agent's private statics.
//
// Renderer chassis is solid3d's, unchanged in outline: rotate, cross-product normal, backface
// cull, brightness from normal-dot-light, painter's sort (there is no z-buffer), one trifill per
// triangle, and the in-between shades faked with fillp(FILL_CHECKER) between two palette colours.
// That dither is what lets 32 colours carry a 7-step ramp per material.
//
// Costs, already measured elsewhere so nobody re-derives them: trifill is cheap (tristress puts
// 421 tiny triangles at 0.97ms and it already gets the span fill), and ADR-0024's "3D is GPU-only,
// ~10fps on a phone" predates the sw_tritex scan-box clamp that took podracer 19.1ms -> 1.6ms.
// This cart draws no textured triangles at all, so neither number is the ceiling here.

#include "studio.h"
#include <math.h>
#include <stdlib.h>
#include "spec.h"
#include "tenement/atlas.h"   // the voxel reference half. See the note above before copying this.

#define TILE 6                // voxels per floor tile. Set by the models, not a knob: see the
                              // long note at the top of tools/voxel-models/tenement.js.
#define ROOM_W 6              // tiles
#define ROOM_H 5

// ── materials ────────────────────────────────────────────────────────────────
// One ramp per material, dark -> light, four palette entries. With the checker dither between
// adjacent entries that is SEVEN shades from four colours, which is solid3d's whole trick and the
// only reason a 32-colour palette can carry per-face lighting without banding into slabs.
//
// The ramps are seeded from the voxel set's material list so the two halves agree on what wood
// and porcelain are. One deliberate inheritance: the WALL ramp is off the neutral greys (mauve
// rather than grey) because the voxel models file records that a neutral wall and the white goods
// came out byte-identical and the fridge dissolved into the wall behind it. Same trap here.
enum { M_WOOD, M_UPH, M_CUSH, M_METAL, M_PORC, M_TRIM, M_WALL, M_SHIRT, M_TROUSER, M_SKIN,
       M_FLOOR_A, M_FLOOR_B, M_COUNT };
static const unsigned char RAMP[M_COUNT][4] = {
    [M_WOOD]    = { CLR_BROWNISH_BLACK, CLR_DARK_BROWN,  CLR_BROWN,       CLR_MEDIUM_GREY },
    [M_UPH]     = { CLR_DARKER_BLUE,    CLR_TRUE_BLUE,   CLR_BLUE,        CLR_LIGHT_GREY  },
    [M_CUSH]    = { CLR_DARKER_PURPLE,  CLR_DARK_PURPLE, CLR_PINK,        CLR_PEACH       },
    [M_METAL]   = { CLR_DARKER_GREY,    CLR_DARK_GREY,   CLR_LIGHT_GREY,  CLR_WHITE       },
    [M_PORC]    = { CLR_DARK_GREY,      CLR_LIGHT_GREY,  CLR_WHITE,       CLR_WHITE       },
    [M_TRIM]    = { CLR_BLACK,          CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_DARK_GREY },
    [M_WALL]    = { CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_MAUVE,       CLR_MEDIUM_GREY },
    [M_SHIRT]   = { CLR_DARK_RED,       CLR_RED,         CLR_DARK_PEACH,  CLR_PEACH       },
    [M_TROUSER] = { CLR_BLACK,          CLR_DARKER_BLUE, CLR_DARK_BLUE,   CLR_TRUE_BLUE   },
    [M_SKIN]    = { CLR_DARK_PEACH,     CLR_PEACH,       CLR_LIGHT_PEACH, CLR_WHITE       },
    [M_FLOOR_A] = { CLR_BROWNISH_BLACK, CLR_DARK_BROWN,  CLR_BROWN,       CLR_MEDIUM_GREY },
    [M_FLOOR_B] = { CLR_BLACK,          CLR_BROWNISH_BLACK, CLR_DARK_BROWN, CLR_BROWN     },
};

// Per-household shirt ramps. This is the tenement punch-list item "per-household colour so you
// can see whose flat someone is standing in without reading the HUD", made visible here because a
// mesh takes its colour at draw time. The voxel half gets the same thing through pal(), so the
// comparison stays about SHAPE rather than about who is allowed a palette.
#define HH_COUNT 4
static const unsigned char HH_RAMP[HH_COUNT][4] = {
    { CLR_DARK_RED,     CLR_RED,          CLR_DARK_PEACH,  CLR_PEACH       },  // red
    { CLR_BLUE_GREEN,   CLR_MEDIUM_GREEN, CLR_GREEN,       CLR_LIME_GREEN  },  // green
    { CLR_DARKER_PURPLE,CLR_DARK_PURPLE,  CLR_INDIGO,      CLR_PINK        },  // violet
    { CLR_DARK_BROWN,   CLR_BROWN,        CLR_ORANGE,      CLR_YELLOW      },  // amber
};
static const unsigned char HH_KEY[HH_COUNT] = { CLR_RED, CLR_GREEN, CLR_INDIGO, CLR_ORANGE };

// ── the authoring primitive ─────────────────────────────────────────────────
// A PRISM: a bottom rectangle, a height, and four offsets that move the TOP rectangle's corners.
// One primitive covers box, taper, shear and wedge, which is the whole reason this look is not the
// voxel look — a sofa back can rake, a torso can widen into shoulders, a bowl can flare. Diagonals
// come free; the voxel bake cannot express one at any price.
//
// Units are VOXELS, the same units the voxel models use, so a transcribed shape lands on the same
// footprint as the sprite it replaces (checked against ISO_FOOTPRINT at init).
typedef struct {
    float x0, y0, z0, x1, y1, z1;      // bottom rect + top height
    float dx0, dy0, dx1, dy1;          // added to x0,y0,x1,y1 at the TOP — taper and shear
    unsigned char mat;
} Part;
#define BOX(x0,y0,z0,x1,y1,z1,m)            { x0,y0,z0,x1,y1,z1, 0,0,0,0, m }
#define PRISM(x0,y0,z0,x1,y1,z1,a,b,c,d,m)  { x0,y0,z0,x1,y1,z1, a,b,c,d, m }

// ── the object set, transcribed from tools/voxel-models/tenement.js ─────────
// Transcribed, NOT redesigned: same footprints, same heights, same silhouette intent, because the
// question is what triangles do to a rendering and a nicer sofa would confound it. Where the voxel
// version had to fake something with stacked blocks, the prism does it directly — and those are
// the places to look when judging.

// sofa 12x6x6 — the voxel note says the sofa and bed once read as the same pale slab and the fix
// was HEIGHT AND OUTLINE, not detail. The back now rakes (dy0/dy1 negative at the top), which is
// the outline cue the block version could only approximate with a straight wall.
static const Part P_SOFA[] = {
    PRISM(0,0,0, 12,6,2.4f,  0.4f,0.4f,-0.4f,-0.4f, M_WOOD),        // plinth, tapered to fake feet
    BOX  (0.9f,1.2f,2.4f, 11.1f,6,3.9f,             M_UPH),         // seat
    PRISM(0,0,2.4f, 12,1.9f,6,  0,-0.9f,0,-0.9f,    M_UPH),         // back, RAKED
    PRISM(0,1,2.4f, 1.3f,6,4.7f, 0.2f,0,-0.2f,0,    M_UPH),         // arms, tapered
    PRISM(10.7f,1,2.4f, 12,6,4.7f, 0.2f,0,-0.2f,0,  M_UPH),
    BOX  (1.2f,1.4f,3.9f, 10.8f,5.4f,4.4f,          M_CUSH),        // a cushion, the colour accent
};

// bed 6x12x4 — and the FOUR is the constraint, not a suggestion. The voxel bed is four voxels tall
// (frame 2, mattress 1, cover 1) because the models file's rule is that the bed stays LOW so its
// outline cannot be confused with the sofa's. The first draft of this mesh gave it a 5.6-tall raked
// headboard, which looked better and was cheating: pr_selfcheck caught it, and that is the whole
// reason the check exists. A look probe that quietly makes the new furniture bigger proves nothing.
// So the diagonals here have to earn their keep INSIDE four voxels: the headboard rakes, the duvet
// is a wedge thicker at the foot, and the pillow is a tapered pad.
static const Part P_BED[] = {
    BOX  (0,0.8f,0, 6,12,1.8f,                       M_WOOD),       // frame
    BOX  (0.4f,1.2f,1.8f, 5.6f,11.6f,2.6f,           M_PORC),       // mattress
    PRISM(0.3f,4,2.6f, 5.7f,11.8f,3.7f, 0.3f,0,-0.3f,-0.4f, M_CUSH),// duvet, wedged
    PRISM(0.9f,1.4f,2.6f, 5.1f,3.8f,3.3f, 0.3f,0.2f,-0.3f,-0.2f, M_PORC), // pillow
    PRISM(0,0,0, 6,1,4.0f,  0,0.6f,0,0.6f,           M_WOOD),       // headboard, RAKED back
};

// toilet 6x6x6 — tall tank behind, flaring bowl in front. The flare is the point: a cylinder read
// out of a prism, which is as close to round as this scale needs.
static const Part P_TOILET[] = {
    PRISM(1,0,0, 5,2,5.4f,  0.2f,0.1f,-0.2f,-0.1f,   M_PORC),       // cistern
    PRISM(1.6f,2,0, 4.4f,5.2f,2.4f, -0.5f,-0.2f,0.5f,0.3f, M_PORC), // bowl, FLARED outward
    PRISM(1,2,2.4f, 5,5.5f,3.0f, 0.1f,0.1f,-0.1f,-0.1f, M_PORC),    // seat
    BOX  (1.4f,0.4f,5.4f, 4.6f,1.8f,5.8f,            M_PORC),       // lid on the cistern
};

// fridge 6x6x12 — as tall as the figure. A box, honestly, but the door seam and handle are
// geometry rather than a painted-on voxel row, and the top is slightly tapered so the silhouette
// is not a perfect rectangle at every angle.
static const Part P_FRIDGE[] = {
    PRISM(0,0,0, 6,6,11.6f,  0.15f,0.15f,-0.15f,-0.15f, M_METAL),
    BOX  (0.3f,-0.25f,5.4f, 5.7f,0.15f,5.9f,         M_TRIM),       // door seam
    BOX  (4.5f,-0.5f,6.4f, 5.2f,0.1f,9.6f,           M_TRIM),       // handle
    PRISM(0,0,11.6f, 6,6,12,  0.15f,0.15f,-0.15f,-0.15f, M_TRIM),   // dark top, kills the box read
};

// counter 6x6x7 — the toe kick (bottom sheared in) and the overhanging top are both things a
// voxel grid cannot do at this size, and together they are what makes it read as joinery.
static const Part P_COUNTER[] = {
    PRISM(0.4f,0.6f,0, 5.6f,6,5.4f, -0.4f,-0.6f,0.4f,0,  M_WOOD),   // cabinet, kicked in at the toe
    BOX  (-0.3f,-0.3f,5.4f, 6.3f,6.3f,6.2f,          M_METAL),      // overhanging worktop
    BOX  (0.5f,-0.4f,2.4f, 5.5f,0.1f,2.7f,           M_TRIM),       // drawer line
};

// loom 6x4x12 — the punch list says the voxel loom "reads as a second wardrobe". This is the
// clearest test in the cart: skeletal legs, a top beam, and a RAKED WARP PLANE. A leaning plane is
// exactly the shape that says machine, and it is exactly the shape a voxel grid cannot make.
static const Part P_LOOM[] = {
    BOX  (0.2f,0.2f,0, 1.2f,1.2f,11,                 M_WOOD),       // four posts
    BOX  (4.8f,0.2f,0, 5.8f,1.2f,11,                 M_WOOD),
    BOX  (0.2f,2.8f,0, 1.2f,3.8f,9,                  M_WOOD),
    BOX  (4.8f,2.8f,0, 5.8f,3.8f,9,                  M_WOOD),
    PRISM(1.0f,1.0f,3.2f, 5.0f,1.6f,10.4f, 0,2.0f,0,2.0f, M_CUSH),  // the warp, LEANING
    BOX  (0,0.4f,10.6f, 6,3.6f,11.6f,                M_WOOD),       // top beam
    BOX  (0.6f,1.2f,2.6f, 5.4f,3.4f,3.4f,            M_WOOD),       // treadle bar
};

// wardrobe 6x4x10 — chest height, obviously a box you open. The cornice overhang is the read.
static const Part P_WARDROBE[] = {
    PRISM(0.2f,0.2f,0, 5.8f,3.8f,9.4f, 0.1f,0.1f,-0.1f,-0.1f, M_WOOD),
    BOX  (2.8f,-0.25f,0.6f, 3.2f,0.15f,9.2f,         M_TRIM),       // the door gap
    BOX  (-0.3f,-0.3f,9.4f, 6.3f,4.3f,10,            M_WOOD),       // cornice
};

// ── the figure, and this is the whole cart in one model ─────────────────────
// The voxel version needed literal arm voxels sticking out at torso level to get a shoulder line
// wider than the head, because that width contrast is what makes a small figure read as a person
// (its own comment records that the armless first cut "read as a lamp"). A prism gets it for free:
// the torso is simply WIDER AT THE TOP. Two legs with a gap, a tapered skull, and a shoulder line
// — four parts, 48 triangles, and it is a person rather than a 12-voxel column.
static const Part P_PERSON[] = {
    PRISM(1.0f,0.9f,0, 2.2f,2.1f,5.2f,  0.15f,0,-0.15f,0, M_TROUSER),  // legs, narrow at the foot
    PRISM(2.8f,0.9f,0, 4.0f,2.1f,5.2f,  0.15f,0,-0.15f,0, M_TROUSER),
    PRISM(1.1f,0.7f,5.2f, 3.9f,2.3f,9.3f, -0.7f,-0.1f,0.7f,0.1f, M_SHIRT), // torso -> SHOULDERS
    PRISM(1.6f,0.9f,9.3f, 3.4f,2.1f,11.6f, 0.25f,0.2f,-0.25f,-0.2f, M_SKIN), // head, tapered
};

// person_lie 3x8x2 — a low silhouette, long with a head at one end, per the voxel file's rule.
static const Part P_PERSON_LIE[] = {
    PRISM(0.2f,1.4f,0, 2.8f,7.6f,1.6f, 0.2f,0,-0.2f,-0.4f, M_SHIRT),
    PRISM(0.5f,0,0.2f, 2.5f,1.5f,1.9f, 0.2f,0.15f,-0.2f,-0.15f, M_SKIN),
};

// Walls are prisms too, so they flow through the one item list and the one renderer instead of
// getting a special case. Same footprints as the baked cells (6x2 or 2x6, 12 or 4 tall).
static const Part P_WALL_FULL_NS[] = { BOX(0,0,0, 6,2,12, M_WALL) };
static const Part P_WALL_FULL_EW[] = { BOX(0,0,0, 2,6,12, M_WALL) };
static const Part P_WALL_LOW_NS[]  = { PRISM(0,0,0, 6,2,4, 0.2f,0.2f,-0.2f,-0.2f, M_WALL) };
static const Part P_WALL_LOW_EW[]  = { PRISM(0,0,0, 2,6,4, 0.2f,0.2f,-0.2f,-0.2f, M_WALL) };

// cell -> mesh. Indexed by the voxel atlas's own enum so one item can address both renderers, and
// the name comes from ISO_NAMES so the two halves cannot drift apart on what a thing is called.
typedef struct { const Part *p; int n; } Mesh;
#define MESH(a) { a, (int)(sizeof(a)/sizeof((a)[0])) }
static const Mesh MESHES[ISO_MODEL_COUNT] = {
    [ISO_SOFA]         = MESH(P_SOFA),         [ISO_BED]        = MESH(P_BED),
    [ISO_TOILET]       = MESH(P_TOILET),       [ISO_FRIDGE]     = MESH(P_FRIDGE),
    [ISO_COUNTER]      = MESH(P_COUNTER),      [ISO_LOOM]       = MESH(P_LOOM),
    [ISO_WARDROBE]     = MESH(P_WARDROBE),     [ISO_PERSON]     = MESH(P_PERSON),
    [ISO_PERSON_LIE]   = MESH(P_PERSON_LIE),
    [ISO_WALL_FULL_NS] = MESH(P_WALL_FULL_NS), [ISO_WALL_FULL_EW] = MESH(P_WALL_FULL_EW),
    [ISO_WALL_LOW_NS]  = MESH(P_WALL_LOW_NS),  [ISO_WALL_LOW_EW]  = MESH(P_WALL_LOW_EW),
};

// ── the scene: ONE item list, walked by BOTH renderers ──────────────────────
// Position is in VOXELS, resolved once at init, so neither renderer gets to place anything its own
// way. `hh` is the household (-1 for furniture), which is the only thing a resident carries here.
// `lx`/`ly` stretch the mesh along the ground, and exist for exactly one reason: WALLS. The voxel
// half must tile them, one baked cell per tile, because that is what a sprite atlas can hold. The
// polygon half has no such constraint and a run of six abutting boxes is actively worse than one
// long box — each abutment leaves a sliver of a side face lit at a different tone, which drew a
// vertical seam every six voxels along every wall. It read as z-fighting and was a decomposition
// nobody would choose if the renderer were not inherited from an atlas.
//
// So the walls are the ONE thing the two halves build differently, and the difference is itself a
// finding rather than a cheat. Furniture and residents — the actual subject — stay one shared list.
typedef struct { int cell; float vx, vy, vz; float lx, ly; int hh; } Item;
#define MAX_ITEMS 96
static Item pr_item[MAX_ITEMS];
static int  pr_item_n;      // total this frame
static int  pr_fixed_n;     // furniture + residents; the walls after this are rebuilt every frame

static void pr_add(int cell, float vx, float vy, float vz, int hh) {
    if (pr_item_n < MAX_ITEMS) pr_item[pr_item_n++] = (Item){ cell, vx, vy, vz, 1.0f, 1.0f, hh };
}
static void pr_add_run(int cell, float vx, float vy, float lx, float ly) {
    if (pr_item_n < MAX_ITEMS) pr_item[pr_item_n++] = (Item){ cell, vx, vy, 0.0f, lx, ly, -1 };
}
static void pr_add_tile(int cell, int tx, int ty, int hh) {
    pr_add(cell, (float)(tx * TILE), (float)(ty * TILE), 0.0f, hh);
}

// Walls live on tile EDGES and their HEIGHT is decided per frame rather than stored, so they are
// rebuilt into the item list every frame by pr_walls() instead of being placed once here.

// The flat. Laid out to suit each model's authored orientation, since instance yaw is not
// implemented (see the todo) — a real consumer would rotate the mesh, which is one of the things
// this probe is meant to prove is cheaper than baking a second variant.
//
//   y=0  fridge  counter  .       .       .       [WC: toilet]
//   y=1  .       .        person  .       .       .
//   y=2  .       .        sofa ---------- .       .
//   y=3  bed     .        .       .       loom    .
//   y=4  (bed)   wardrobe person  .       person  .
static void pr_scene(void) {
    pr_item_n = 0;
    pr_add_tile(ISO_FRIDGE,   0, 0, -1);
    pr_add_tile(ISO_COUNTER,  1, 0, -1);
    pr_add_tile(ISO_TOILET,   5, 0, -1);
    pr_add_tile(ISO_SOFA,     2, 2, -1);
    pr_add_tile(ISO_BED,      0, 3, -1);
    pr_add_tile(ISO_WARDROBE, 1, 4, -1);
    pr_add_tile(ISO_LOOM,     4, 3, -1);

    // Residents. Three standing and one asleep, and two of them adjacent on purpose: the punch
    // list says two waiters on neighbouring tiles read as ONE BLOB, so a queue of three looks like
    // a queue of one. Whether you can COUNT these four is half of what this cart is for.
    pr_add_tile(ISO_PERSON, 2, 1, 0);
    pr_add_tile(ISO_PERSON, 2, 4, 1);
    pr_add_tile(ISO_PERSON, 4, 4, 2);
    // Asleep: centred on the bed and lifted onto its surface, the same arithmetic art.h does, in
    // whole voxels (a fractional offset puts corners between lattice points — iso-rooms.md §7).
    {
        const short *bfp = ISO_FOOTPRINT[ISO_BED], *pfp = ISO_FOOTPRINT[ISO_PERSON_LIE];
        pr_add(ISO_PERSON_LIE, (float)(0 * TILE + (bfp[0] - pfp[0]) / 2),
                               (float)(3 * TILE + (bfp[1] - pfp[1]) / 2), (float)bfp[2], 3);
    }
    pr_fixed_n = pr_item_n;   // everything after this point is walls, rebuilt each frame
}

// ── camera + projection ─────────────────────────────────────────────────────
// The projection is the voxel bake's, generalized to a continuous yaw:
//
//     Xr = x cos a - y sin a          sx = Xr * SX
//     Yr = x sin a + y cos a          sy = Yr * SY * squash - z * SZ
//
// At a = 45 this is exactly `sx = (x-y)*2, sy = (x+y) - z*2`, which is tnr_iso_project in
// tenement/art.h. That identity is what makes TAB a fair test, and pr_selfcheck() asserts it.
//
// SY and SZ are two INDEPENDENT numbers (1.414 and 2.0), which no single orthographic elevation
// can produce — 2px per voxel of height is the voxel bake's art choice, not geometry. So this is a
// stylized shear rather than a rotation, which is also why there is no perspective toggle: that
// would be a different projection answering a different question. Seam marked, todo written.
#define PR_SX 2.8284271f      // 2*sqrt(2)
#define PR_SY 1.4142136f      // sqrt(2)
#define PR_SZ 2.0f            // px per voxel of height

static float pr_yaw = 45.0f, pr_squash = 1.0f;
static int   pr_zoom = 1, pr_poly = 1, pr_dither = 1, pr_lightcam = 1, pr_tint = 1;
static float pr_cx, pr_cy;    // camera offset, in pixels

static void pr_project(float vx, float vy, float vz, float *sx, float *sy) {
    const float a = pr_yaw * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
    const float Xr = vx * c - vy * s, Yr = vx * s + vy * c;
    *sx = Xr * PR_SX * pr_zoom;
    *sy = (Yr * PR_SY * pr_squash - vz * PR_SZ) * pr_zoom;
}

// The camera's view ray, DERIVED from the projection rather than hardcoded, so it stays correct
// when the squash changes. A world offset d leaves the screen position unchanged when Xr does not
// move and the ground and height terms cancel:
//     (dx,dy) = (sin a, cos a)      dz = SY*squash / SZ
//
// `d` points TOWARD the eye (check it at yaw 45: d = (.707,.707,.707) has Xr+Yr = 1 > 0, and larger
// X+Y is nearer in this projection — art.h's own convention). So dot(p,d) is NEARNESS, a face is
// visible when dot(n,d) > 0, and the painter's key must be NEGATED, because zsort is descending:
// big key drawn first = big key must mean FAR. Getting that sign backwards draws the room inside
// out and still looks plausible in a still frame, which is why it is spelled out here.
static void pr_viewdir(float *dx, float *dy, float *dz) {
    const float a = pr_yaw * 3.14159265f / 180.0f;
    *dx = sinf(a); *dy = cosf(a); *dz = PR_SY * pr_squash / PR_SZ;
}
static float pr_nearness(float vx, float vy, float vz) {
    float dx, dy, dz; pr_viewdir(&dx, &dy, &dz);
    return vx * dx + vy * dy + vz * dz;
}

// Rebuild the walls for this frame. A full-height wall standing between the camera and the room
// hides exactly what the cart exists to look at, so a wall NEARER than the room's centre draws as
// a low stub instead. iso-rooms.md §8 settled this shape ("FULL height with the near side cut
// away, which beat Theme Hospital's low stubs plainly"); nearness is DERIVED from the projection,
// so it survives any change to the camera rather than being a table of four hardcoded cases.
static void pr_walls(int merged) {
    pr_item_n = pr_fixed_n;
    const float mid = pr_nearness(ROOM_W * TILE * 0.5f, ROOM_H * TILE * 0.5f, 0);
    // A whole side is one nearness test at its midpoint. The perimeter runs are decided together
    // whether or not they are drawn together, so both halves cut away the same walls.
    const float W = (float)(ROOM_W * TILE), H = (float)(ROOM_H * TILE);
    const struct { float vx, vy; int ew; } side[4] = {
        { 0, -2.0f, 0 }, { 0, H, 0 }, { -2.0f, 0, 1 }, { W, 0, 1 },
    };
    for (int s = 0; s < 4; s++) {
        const int ew = side[s].ew;
        const float cx = side[s].vx + (ew ? 1.0f : W * 0.5f);
        const float cy = side[s].vy + (ew ? H * 0.5f : 1.0f);
        const int cut = pr_nearness(cx, cy, 0) > mid;
        const int cell = cut ? (ew ? ISO_WALL_LOW_EW : ISO_WALL_LOW_NS)
                             : (ew ? ISO_WALL_FULL_EW : ISO_WALL_FULL_NS);
        if (merged) {
            pr_add_run(cell, side[s].vx, side[s].vy, ew ? 1.0f : (float)ROOM_W,
                                                    ew ? (float)ROOM_H : 1.0f);
        } else {
            const int n = ew ? ROOM_H : ROOM_W;
            for (int i = 0; i < n; i++)
                pr_add(cell, side[s].vx + (ew ? 0 : (float)(i * TILE)),
                             side[s].vy + (ew ? (float)(i * TILE) : 0), 0, -1);
        }
    }
    // The WC stubs are one tile each and always low, so they need no run.
    pr_add(ISO_WALL_LOW_EW, (float)(5 * TILE) - 2.0f, 0.0f,            0, -1);
    pr_add(ISO_WALL_LOW_NS, (float)(5 * TILE),        (float)TILE - 2, 0, -1);
}

// Frame the room: project the eight corners of its bounding box and centre what comes back. The
// offset is FLOORED, because a half-pixel camera rounds adjacent shapes opposite ways and opens
// 1px seams (iso-rooms.md §7 — learned there, not rediscovered here).
static void pr_camera(void) {
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    const float W = ROOM_W * TILE, H = ROOM_H * TILE;
    for (int c = 0; c < 8; c++) {
        float sx, sy;
        pr_project((c & 1) ? W : -2.0f, (c & 2) ? H : -2.0f, (c & 4) ? 12.0f : 0.0f, &sx, &sy);
        if (sx < minx) minx = sx;  if (sx > maxx) maxx = sx;
        if (sy < miny) miny = sy;  if (sy > maxy) maxy = sy;
    }
    pr_cx = floorf((SCREEN_W - (maxx - minx)) * 0.5f - minx);
    pr_cy = floorf((SCREEN_H - 22 - (maxy - miny)) * 0.5f - miny + 11);
}

// ── the polygon renderer ────────────────────────────────────────────────────
typedef struct { int x[3], y[3]; float depth, bright; unsigned char mat, hh; } Tri;
#define MAX_TRIS 3072
static Tri pr_tri[MAX_TRIS];
static int pr_tri_n, pr_tri_drawn, pr_order[MAX_TRIS];
static float pr_key[MAX_TRIS];

// The light. World-fixed is the honest one (the room turns under a fixed sun). Screen-fixed
// reproduces what the voxel bake does — its tones are literally [top, screen-right, screen-left],
// so its light rotates with the camera. iso-rooms.md's constraint was that shading a BAKED cell by
// world normal makes the light appear to rotate with the room; a live mesh can do either, and L
// switches, because which one reads better is a real question and not a settled one.
//
// THE DIRECTION IS NOT A TASTE CHOICE, it is pinned to the voxel bake's three tones. At yaw 45 the
// camera sees each object's +x, +y and +z faces (d = (.707,.707,.707), and a face is visible when
// its normal dots positive with d), and +x is SCREEN-RIGHT because sx = (x-y)*2. So a light that
// wants to reproduce [top, screen-right, screen-left] = bright, mid, dim must lean +z, then +x,
// then +y — which lands those three faces on ramp entries 3, 2 and 1 exactly.
//
// The first draft had this pointing the other way (negative x and y), which put every side face on
// the ambient floor: the whole room went muddy and the far walls disappeared into the background.
// It read as a verdict about low-poly and it was a sign error, which is the trap in a LOOK probe —
// a rendering bug and an ugly look are the same picture.
static const float LX = 0.50f, LY = 0.26f, LZ = 0.83f;

static void pr_emit(const float v[3][3], int mat, int hh) {
    if (pr_tri_n >= MAX_TRIS) return;
    float e1[3] = { v[1][0]-v[0][0], v[1][1]-v[0][1], v[1][2]-v[0][2] };
    float e2[3] = { v[2][0]-v[1][0], v[2][1]-v[1][1], v[2][2]-v[1][2] };
    float n[3] = { e1[1]*e2[2] - e1[2]*e2[1], e1[2]*e2[0] - e1[0]*e2[2], e1[0]*e2[1] - e1[1]*e2[0] };
    const float len = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    if (len < 1e-6f) return;                       // degenerate: a zero-thickness part edge
    n[0] /= len; n[1] /= len; n[2] /= len;

    float dx, dy, dz; pr_viewdir(&dx, &dy, &dz);
    if (n[0]*dx + n[1]*dy + n[2]*dz <= 0.0f) return;   // backface. With no z-buffer this is not an
                                                       // optimization, it is correctness
    // Brightness. Screen-fixed mode spins the normal by the camera yaw before lighting it, which is
    // the same thing as spinning the light the other way — and matches the baked tones exactly.
    float ln[3] = { n[0], n[1], n[2] };
    if (pr_lightcam) {
        const float a = (pr_yaw - 45.0f) * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
        ln[0] = n[0]*c - n[1]*s; ln[1] = n[0]*s + n[1]*c;
    }
    float b = ln[0]*LX + ln[1]*LY + ln[2]*LZ;
    b = 0.22f + 0.78f * (b < 0.0f ? 0.0f : b);     // ambient floor, or an unlit face is a black hole

    Tri *t = &pr_tri[pr_tri_n];
    for (int i = 0; i < 3; i++) {
        float sx, sy; pr_project(v[i][0], v[i][1], v[i][2], &sx, &sy);
        t->x[i] = (int)(sx + pr_cx); t->y[i] = (int)(sy + pr_cy);
    }
    // NEGATED nearness — see pr_viewdir. zsort draws the big key first, so big must mean far.
    t->depth = -((v[0][0]+v[1][0]+v[2][0]) * dx + (v[0][1]+v[1][1]+v[2][1]) * dy
               + (v[0][2]+v[1][2]+v[2][2]) * dz) * (1.0f/3.0f);
    t->bright = b; t->mat = (unsigned char)mat; t->hh = (unsigned char)(hh < 0 ? 255 : hh);
    pr_tri_n++;
}

static void pr_quad(const float a[3], const float b[3], const float c[3], const float d[3],
                    int mat, int hh) {
    float t1[3][3] = { {a[0],a[1],a[2]}, {b[0],b[1],b[2]}, {c[0],c[1],c[2]} };
    float t2[3][3] = { {a[0],a[1],a[2]}, {c[0],c[1],c[2]}, {d[0],d[1],d[2]} };
    pr_emit(t1, mat, hh); pr_emit(t2, mat, hh);
}

// A prism's six faces, wound counter-clockwise seen from OUTSIDE so the cross product points out.
// lx/ly stretch it along the ground before placing (walls only — see the Item comment).
static void pr_part(const Part *p, float ox, float oy, float oz, int hh, float lx, float ly) {
    const float bx0 = p->x0 * lx + ox, by0 = p->y0 * ly + oy;
    const float bx1 = p->x1 * lx + ox, by1 = p->y1 * ly + oy;
    const float tx0 = (p->x0 + p->dx0) * lx + ox, ty0 = (p->y0 + p->dy0) * ly + oy;
    const float tx1 = (p->x1 + p->dx1) * lx + ox, ty1 = (p->y1 + p->dy1) * ly + oy;
    const float z0 = p->z0 + oz, z1 = p->z1 + oz;
    const float b0[3] = {bx0,by0,z0}, b1[3] = {bx1,by0,z0}, b2[3] = {bx1,by1,z0}, b3[3] = {bx0,by1,z0};
    const float u0[3] = {tx0,ty0,z1}, u1[3] = {tx1,ty0,z1}, u2[3] = {tx1,ty1,z1}, u3[3] = {tx0,ty1,z1};
    const int m = p->mat;
    pr_quad(u0, u1, u2, u3, m, hh);    // top    (+z)
    pr_quad(b0, b3, b2, b1, m, hh);    // bottom (-z)
    pr_quad(b0, b1, u1, u0, m, hh);    // front  (-y)
    pr_quad(b2, b3, u3, u2, m, hh);    // back   (+y)
    pr_quad(b3, b0, u0, u3, m, hh);    // left   (-x)
    pr_quad(b1, b2, u2, u1, m, hh);    // right  (+x)
}

// Brightness -> a shade. Four ramp entries become SEVEN steps: the odd ones are a checker of two
// adjacent colours, which is a shade the palette does not contain. D turns it off and the gradient
// collapses into four hard bands, which is the fastest way to see what the dither is buying.
static void pr_shade_fill(const Tri *t) {
    // THE FLOOR IS NEVER SHADED AND NEVER DITHERED, and both halves of that are deliberate. It is
    // one flat plane at one brightness, so a checker between two shades cannot read as shading —
    // it can only read as a chequered lino, which is what it did (a 50% brown/tan pattern over the
    // whole room, which looked like a texture decision and was an accident). Drawing it as the
    // literal two browns the voxel half uses also keeps the A/B honest: the floor is then the same
    // pixels in both modes, so every difference you see is furniture and people.
    if (t->mat >= M_FLOOR_A) {
        trifill(t->x[0], t->y[0], t->x[1], t->y[1], t->x[2], t->y[2], RAMP[t->mat][2]);
        return;
    }
    const unsigned char *ramp = (t->hh < HH_COUNT && pr_tint && t->mat == M_SHIRT)
                              ? HH_RAMP[t->hh] : RAMP[t->mat];
    // THE DITHER HAS A SIZE LIMIT, and finding it is one of the things this cart is for. A 50%
    // checker of two palette colours reads as a SHADE on a small face and as WALLPAPER on a big
    // one — the wall and fridge faces came out visibly chequered, which looked like a texture
    // decision and was an accident of the trick's range. So a triangle past the threshold snaps to
    // the nearest solid step. Threshold is in canvas pixels because the 4x4 pattern is: it does not
    // scale with the zoom, so a face that grows past it genuinely does start showing the weave.
    const long area2 = labs((long)(t->x[1]-t->x[0]) * (t->y[2]-t->y[0])
                          - (long)(t->x[2]-t->x[0]) * (t->y[1]-t->y[0]));   // 2x the triangle area
    if (!pr_dither || area2 > 400) {
        int s = (int)(t->bright * 3.999f); if (s > 3) s = 3; if (s < 0) s = 0;
        trifill(t->x[0], t->y[0], t->x[1], t->y[1], t->x[2], t->y[2], ramp[s]);
        return;
    }
    int s = (int)(t->bright * 6.999f); if (s > 6) s = 6; if (s < 0) s = 0;
    if (s & 1) {
        fillp(FILL_CHECKER, ramp[s / 2]);
        trifill(t->x[0], t->y[0], t->x[1], t->y[1], t->x[2], t->y[2], ramp[s / 2 + 1]);
        fillp_reset();                 // fillp is STICKY and into the next frame — always reset
    } else {
        trifill(t->x[0], t->y[0], t->x[1], t->y[1], t->x[2], t->y[2], ramp[s / 2]);
    }
}

// Split from the draw so spec() can assert on the triangle list without rasterizing anything.
static void pr_build_tris(void) {
    pr_tri_n = 0;
    // Floor: one quad per tile, alternating, same two browns the voxel half uses. Flat on z=0, so
    // it needs no normal — but it goes through the same emit so it sorts with everything else.
    for (int ty = 0; ty < ROOM_H; ty++) for (int tx = 0; tx < ROOM_W; tx++) {
        const float x0 = (float)(tx*TILE), y0 = (float)(ty*TILE);
        const float a[3] = {x0,y0,0}, b[3] = {x0+TILE,y0,0};
        const float c[3] = {x0+TILE,y0+TILE,0}, d[3] = {x0,y0+TILE,0};
        pr_quad(a, b, c, d, ((tx + ty) & 1) ? M_FLOOR_A : M_FLOOR_B, -1);
    }
    for (int i = 0; i < pr_item_n; i++) {
        const Item *it = &pr_item[i];
        const Mesh *m = &MESHES[it->cell];
        for (int p = 0; p < m->n; p++)
            pr_part(&m->p[p], it->vx, it->vy, it->vz, it->hh, it->lx, it->ly);
    }
}

static void pr_draw_poly(void) {
    pr_build_tris();
    for (int i = 0; i < pr_tri_n; i++) pr_key[i] = pr_tri[i].depth;
    zsort(pr_key, pr_order, pr_tri_n);                 // far -> near; there is no z-buffer
    for (int i = 0; i < pr_tri_n; i++) pr_shade_fill(&pr_tri[pr_order[i]]);
    pr_tri_drawn = pr_tri_n;
}

// ── the voxel renderer: the reference half ──────────────────────────────────
// A faithful reduction of tenement/art.h — same cells, same painter's sort, same contact shadow —
// over the same item list. It can only draw the four angles the sprites were baked at, which is
// not a limitation of this cart but the finding: TAB snaps the yaw, and you watch it snap.
static int pr_rot_index(void) {
    int r = (int)floorf((pr_yaw - 45.0f) / 90.0f + 0.5f) & 3;
    return r;
}
static float pr_iso_depth_of(const Item *it) {
    const short *fp = ISO_FOOTPRINT[it->cell];
    // Negated, same reason as the polygon half: nearness in, far-first key out.
    return -pr_nearness(it->vx + fp[0] * 0.5f, it->vy + fp[1] * 0.5f, 0);
}
static void pr_draw_voxel(void) {
    for (int ty = 0; ty < ROOM_H; ty++) for (int tx = 0; tx < ROOM_W; tx++) {
        float c[4][2]; const float x0 = (float)(tx*TILE), y0 = (float)(ty*TILE);
        pr_project(x0,      y0,      0, &c[0][0], &c[0][1]);
        pr_project(x0+TILE, y0,      0, &c[1][0], &c[1][1]);
        pr_project(x0+TILE, y0+TILE, 0, &c[2][0], &c[2][1]);
        pr_project(x0,      y0+TILE, 0, &c[3][0], &c[3][1]);
        quadfill((int)(c[0][0]+pr_cx),(int)(c[0][1]+pr_cy), (int)(c[1][0]+pr_cx),(int)(c[1][1]+pr_cy),
                 (int)(c[2][0]+pr_cx),(int)(c[2][1]+pr_cy), (int)(c[3][0]+pr_cx),(int)(c[3][1]+pr_cy),
                 ((tx+ty)&1) ? CLR_BROWN : CLR_DARK_BROWN);
    }
    for (int i = 0; i < pr_item_n; i++) pr_key[i] = pr_iso_depth_of(&pr_item[i]);
    zsort(pr_key, pr_order, pr_item_n);
    const int rot = pr_rot_index();
    for (int i = 0; i < pr_item_n; i++) {
        const Item *it = &pr_item[pr_order[i]];
        const IsoCell *c = &ISO_CELLS[it->cell][rot];
        const short *fp = ISO_FOOTPRINT[it->cell];
        float sx, sy; pr_project(it->vx, it->vy, it->vz, &sx, &sy);
        if (it->vz == 0.0f && it->cell != ISO_WALL_FULL_NS && it->cell != ISO_WALL_FULL_EW
                           && it->cell != ISO_WALL_LOW_NS  && it->cell != ISO_WALL_LOW_EW) {
            float q[4][2]; const float pad = 1.0f;                 // integer inset — §7 again
            pr_project(it->vx+pad,        it->vy+pad,        0, &q[0][0], &q[0][1]);
            pr_project(it->vx+fp[0]-pad,  it->vy+pad,        0, &q[1][0], &q[1][1]);
            pr_project(it->vx+fp[0]-pad,  it->vy+fp[1]-pad,  0, &q[2][0], &q[2][1]);
            pr_project(it->vx+pad,        it->vy+fp[1]-pad,  0, &q[3][0], &q[3][1]);
            quadfill((int)(q[0][0]+pr_cx),(int)(q[0][1]+pr_cy),(int)(q[1][0]+pr_cx),(int)(q[1][1]+pr_cy),
                     (int)(q[2][0]+pr_cx),(int)(q[2][1]+pr_cy),(int)(q[3][0]+pr_cx),(int)(q[3][1]+pr_cy),
                     CLR_BROWNISH_BLACK);
        }
        // Per-household colour on the voxel half too, via pal() on the shirt index — so the A/B
        // stays about SHAPE and not about who is allowed a palette (ADR-0007: pal recolors sprites).
        const int tinted = pr_tint && it->hh >= 0 && it->hh < HH_COUNT;
        if (tinted) pal(CLR_RED, HH_KEY[it->hh]);
        // The DESTINATION rect scales with the zoom, and the cell's origin offset with it. Drawing
        // a baked cell 1:1 while its POSITION scales pulls the room apart into floating pillars —
        // it looked like a rendering finding and was a bug in this cart. Integer upscale only,
        // which is what the engine already does for the window scale.
        sspr(c->x, c->y, c->w, c->h,
             (int)(sx + pr_cx) - c->ox * pr_zoom, (int)(sy + pr_cy) - c->oy * pr_zoom,
             c->w * pr_zoom, c->h * pr_zoom);
        if (tinted) pal_reset();
    }
    pr_tri_drawn = 0;
}

// ── the identity that makes TAB fair ────────────────────────────────────────
// At yaw 45 the generalized projection must reproduce tenement/art.h's exactly, or the two halves
// are being drawn at different scales and every judgement made from this cart is worthless. Cheap
// enough to assert at init rather than trust.
static int pr_selfcheck_ok = 1;
static const char *pr_selfcheck_why = "";     // names the offender via ISO_NAMES, so a failure is
                                              // actionable rather than a red line on the screen
static void pr_selfcheck(void) {
    const float saved_yaw = pr_yaw, saved_sq = pr_squash; const int saved_z = pr_zoom;
    pr_yaw = 45.0f; pr_squash = 1.0f; pr_zoom = 1;
    for (int i = 0; i < 12; i++) {
        const float x = (float)(i * 3 % 7), y = (float)(i * 5 % 11), z = (float)(i % 4);
        float sx, sy; pr_project(x, y, z, &sx, &sy);
        if (fabsf(sx - (x - y) * 2.0f) > 0.01f || fabsf(sy - ((x + y) - z * 2.0f)) > 0.01f) {
            pr_selfcheck_ok = 0; pr_selfcheck_why = "projection";
        }
    }
    // And each mesh must sit inside the footprint of the sprite it stands in for, or the poly half
    // is quietly bigger than the voxel half and "it reads better" means "it is larger".
    for (int c = 0; c < ISO_MODEL_COUNT; c++) {
        if (!MESHES[c].n) continue;
        float mx = 0, my = 0, mz = 0;
        for (int p = 0; p < MESHES[c].n; p++) {
            const Part *q = &MESHES[c].p[p];
            const float ex = fmaxf(q->x1, q->x1 + q->dx1), ey = fmaxf(q->y1, q->y1 + q->dy1);
            if (ex > mx) mx = ex;  if (ey > my) my = ey;  if (q->z1 > mz) mz = q->z1;
        }
        const short *fp = ISO_FOOTPRINT[c];
        if (mx > fp[0] + 0.51f || my > fp[1] + 0.51f || mz > fp[2] + 0.51f) {
            pr_selfcheck_ok = 0; pr_selfcheck_why = ISO_NAMES[c];
        }
    }
    pr_yaw = saved_yaw; pr_squash = saved_sq; pr_zoom = saved_z;
}

// ── entry points ────────────────────────────────────────────────────────────
void init(void) {
    colorkey(-1);
    pr_scene();
    pr_selfcheck();
    pr_camera();
}

void update(void) {
    // PER FRAME, NOT PER SECOND, and deliberately not dt()-scaled. This is a look probe whose
    // output is frame dumps and recorded clips, and a headless harness run compresses real elapsed
    // time to nearly nothing — a dt()-driven orbit turned 35 degrees in 400 frames instead of 395,
    // so a scripted clip could not land on a chosen angle. A fixed step makes every dump
    // reproducible. It also means the orbit runs at the refresh rate, which for a probe is fine.
    const float SPIN = 1.5f, TILT = 0.02f;
    if (key('Q')) pr_yaw -= SPIN;
    if (key('E')) pr_yaw += SPIN;
    if (key('W')) { pr_squash += TILT; if (pr_squash > 1.35f) pr_squash = 1.35f; }
    if (key('S')) { pr_squash -= TILT; if (pr_squash < 0.30f) pr_squash = 0.30f; }
    if (keyp(KEY_TAB)) {
        pr_poly = !pr_poly;
        // Leaving POLY snaps to a baked angle, because the sprites have exactly four and there is
        // nothing to interpolate. The snap IS one of the findings; do not smooth it away.
        if (!pr_poly) { pr_yaw = 45.0f + 90.0f * pr_rot_index(); pr_squash = 1.0f; }
    }
    if (keyp('Z')) pr_zoom = pr_zoom % 3 + 1;
    if (keyp('D')) pr_dither = !pr_dither;
    if (keyp('L')) pr_lightcam = !pr_lightcam;
    if (keyp('H')) pr_tint = !pr_tint;
    if (keyp(KEY_SPACE)) { pr_yaw = 45.0f; pr_squash = 1.0f; pr_zoom = 1; }
    while (pr_yaw < 0.0f)    pr_yaw += 360.0f;
    while (pr_yaw >= 360.0f) pr_yaw -= 360.0f;
    if (!pr_poly) pr_squash = 1.0f;                 // the sprites were baked at one squash only
    pr_camera();
#ifdef DE_TRACE
    watch("mode",  "%s", pr_poly ? "poly" : "voxel");
    watch("yaw",   "%.1f", pr_yaw);
    watch("tris",  "%d", pr_tri_drawn);
    watch("ok",    "%d", pr_selfcheck_ok);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    pr_walls(pr_poly);          // merged runs for the mesh, one cell per tile for the sprites
    if (pr_poly) pr_draw_poly(); else pr_draw_voxel();

    // The band. Deliberately thin: this cart's whole complaint about tenement is that the picture
    // was carrying none of the meaning and the text band was carrying all of it. If you find
    // yourself reading this strip to know what you are looking at, the look has not won.
    rectfill(0, SCREEN_H - 20, SCREEN_W, 20, CLR_BLACK);
    font(FONT_SMALL);
    print(pr_poly ? "POLY" : "VOXEL", 3, SCREEN_H - 17, pr_poly ? CLR_LIGHT_PEACH : CLR_BLUE);
    print(str("yaw %3.0f  %dx  tris %d", pr_yaw, pr_zoom, pr_tri_drawn),
          32, SCREEN_H - 17, CLR_LIGHT_GREY);
    print(str("TAB a/b  QE orbit  WS tilt  Z zoom  D dith:%s  L light:%s  H hh:%s",
              pr_dither ? "on" : "off", pr_lightcam ? "screen" : "world", pr_tint ? "on" : "off"),
          3, SCREEN_H - 9, CLR_DARK_GREY);
    if (!pr_selfcheck_ok)
        print(str("SELFCHECK FAILED (%s) - the halves are not at the same scale", pr_selfcheck_why),
              3, 3, CLR_RED);
    font(FONT_NORMAL);
}

// ── spec: the A/B is only evidence if the two halves are the same test ──────
// This cart's conclusion is a JUDGEMENT ("does it read as people in a building"), which no oracle
// can check — that is the ADR-0022 half tenement failed, and inventing a number for it would be
// the same mistake again. What CAN be gated is everything that would make the judgement WORTHLESS:
// a projection that quietly differs between modes, meshes bigger than the sprites they stand in
// for, or a cutaway that shows one half a wall the other half hides. All three would read as "the
// new look is better" and all three would be a bug in this cart.
void spec(void) {
    step(1);
    expect(pr_selfcheck_ok, "init selfcheck passes");

    // 1. At yaw 45 the generalized projection IS tenement/art.h's. Same scale, same screen.
    pr_yaw = 45.0f; pr_squash = 1.0f; pr_zoom = 1;
    int identical = 1;
    for (int i = 0; i < 20; i++) {
        const float x = (float)(i % 7), y = (float)(i % 11), z = (float)(i % 5);
        float sx, sy; pr_project(x, y, z, &sx, &sy);
        if (!spec_close(sx, (x - y) * 2.0f, 0.01f) ||
            !spec_close(sy, (x + y) - z * 2.0f, 0.01f)) identical = 0;
    }
    expect(identical, "yaw 45 reproduces the voxel projection exactly");

    // 2. Every mesh fits inside the footprint of the sprite it replaces. Without this, "the polygon
    //    version reads better" can just mean "the polygon version is bigger".
    for (int c = 0; c < ISO_MODEL_COUNT; c++) {
        if (!MESHES[c].n) continue;
        float mx = 0, my = 0, mz = 0;
        for (int p = 0; p < MESHES[c].n; p++) {
            const Part *q = &MESHES[c].p[p];
            const float ex = fmaxf(q->x1, q->x1 + q->dx1), ey = fmaxf(q->y1, q->y1 + q->dy1);
            if (ex > mx) mx = ex;  if (ey > my) my = ey;  if (q->z1 > mz) mz = q->z1;
        }
        const short *fp = ISO_FOOTPRINT[c];
        expect(mx <= fp[0] + 0.51f && my <= fp[1] + 0.51f && mz <= fp[2] + 0.51f,
               str("%s mesh fits its voxel footprint", ISO_NAMES[c]));
    }

    // 3. The cutaway agrees across modes at every angle, and never removes the whole room. The
    //    merged run and the per-tile cells are DIFFERENT geometry making the SAME decision.
    int agree = 1, always_some_wall = 1, always_some_cut = 1;
    for (int a = 0; a < 24; a++) {
        pr_yaw = (float)a * 15.0f;
        int full_poly = 0, cut_poly = 0, full_vox = 0;
        pr_walls(1);
        for (int i = pr_fixed_n; i < pr_item_n; i++) {
            if (pr_item[i].cell == ISO_WALL_FULL_NS || pr_item[i].cell == ISO_WALL_FULL_EW) full_poly++;
            else cut_poly++;
        }
        pr_walls(0);
        for (int i = pr_fixed_n; i < pr_item_n; i++)
            if (pr_item[i].cell == ISO_WALL_FULL_NS || pr_item[i].cell == ISO_WALL_FULL_EW) full_vox++;
        // The per-tile half draws ROOM_W or ROOM_H cells where the merged half draws one run, so
        // compare "is any side full", not the counts.
        if ((full_poly > 0) != (full_vox > 0)) agree = 0;
        if (!full_poly) always_some_wall = 0;      // a room with no back wall is a floating floor
        if (cut_poly < 2) always_some_cut = 0;     // and one with no cutaway hides its own contents
    }
    expect(agree, "poly and voxel cut away the same walls at every angle");
    expect(always_some_wall, "some wall stays full height at every angle");
    expect(always_some_cut, "some wall is cut away at every angle");

    // 4. Backface culling actually culls, and the budget holds all round. A silent overflow past
    //    MAX_TRIS drops the FAR triangles (they are emitted first), which looks like a hole.
    int min_tris = 99999, max_tris = 0;
    for (int a = 0; a < 24; a++) {
        pr_yaw = (float)a * 15.0f;
        pr_walls(1); pr_build_tris();
        if (pr_tri_n < min_tris) min_tris = pr_tri_n;
        if (pr_tri_n > max_tris) max_tris = pr_tri_n;
    }
    expect(min_tris > 200, "the room is never mostly culled away");
    expect(max_tris < MAX_TRIS, str("tri budget holds (peak %d of %d)", max_tris, MAX_TRIS));

    pr_yaw = 45.0f;
}
