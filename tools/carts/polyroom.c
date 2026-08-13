/* de:meta
{
  "title": "polyroom",
  "slug": "polyroom",
  "kind": ["probe", "tech-demo"],
  "teaches": ["software-rasterizer", "isometric-projection"],
  "created": "2026-08-13",
  "lineage": "A look probe for tenement, whose maker's verdict was that the picture reads as an architectural diagram with a caption. Asks one question: does the same flat, rendered as low-poly flat-shaded triangles instead of baked voxel sprites, read as FOUR PEOPLE IN A BUILDING? Renderer chassis from solid3d (rot/cull/light/sort/trifill + the fillp checker half-shade); shapes transcribed from tools/voxel-models/tenement.js so the comparison is of RENDERINGS, not of two different object sets.",
  "todo": [
    "THE DEPTH BUFFER IS BUILT AND MEASURED (X toggles it) AND THE RESULT IS THE OPPOSITE OF EXPECTED: it is not slower, it is slightly FASTER. Sorted path 0.93ms avg / 2.77ms peak with 551 trifill; depth-tested path 0.86ms / 2.23ms with 3996 one-row rectfill — from CART LAND, through the public API, which was supposed to be the expensive way. Two reasons: the sort draws every triangle in full and the depth test only emits runs that survive, and zsort is an INSERTION sort, so 551 triangles is up to ~150k comparisons a frame that the depth path simply does not do. Engine-side would be faster still (direct framebuffer writes, no per-run call).",
    "HOW TO TELL A SORT BUG FROM EVERYTHING ELSE, and it is the most reusable thing in this cart: render the orbit TWICE, once sorted and once depth-tested, and diff the frames. Anything present in both is not a sorting error. That scan (47-204 differing pixels of 56320 across eight angles) is what proved the last four defects were a rasterization CRACK at the floor-wall junction, a sub-pixel FLOAT gap under the sleeper, a shade-budget tie, and a scene-layout collision — none of them the sort, which is where all four would otherwise have been blamed. Two scripts and a blend=difference; do this before theorising.",
    "AND THE PICTURES AGREE: 103 differing pixels of 56320 (0.18%), max delta 146, at the angle that used to fail. So the depth buffer is NOT fixing a visible bug today — subdivision plus the abut rule already do. Its value is deleting those compensations, and that is a real but different argument. Read the residual 0.18% before believing either path: they are edge pixels where this cart's own rasterizer and the engine's trifill coverage rule disagree, NOT sort errors, and confirming that is the next job.",
    "SO THE ENGINE QUESTION IS NOW EVIDENCED RATHER THAN SPECULATIVE. ADR-0009 scoped general 3D out and chose leaf helpers, which was right against a 3D ENGINE; what this measures is much narrower — a per-pixel depth compare inside trifill plus one 320x200 depth array. It is cheap, it is faster than sorting, and it removes four separate authoring constraints. It still wants an ADR and a det-probes gate rather than a patch to a hot shared file, and the numbers above are the evidence to argue it with.",
    "SUBDIVISION IS NOW LOAD-BEARING and its bound is asserted, not assumed: no quad may span more than two tiles of depth (spec case 5). If a future mesh gets big flat faces, they subdivide automatically — but the constant is tuned to a 6-voxel tile, so changing TILE changes the sort's soundness. The cost of the bound is measured: tri count 372 to 551, frame 0.59ms to 0.93ms.",
    "THE SHADE COUNT IS WHERE THE 32-SLOT BUDGET ACTUALLY HURTS, and both symptoms the maker found came from it. A resident with 2 shades gives BOTH its side faces one dark tone, so a sleeping figure was a bright top and a flat shadow — it reads as missing geometry. Fixed by giving households 3 shades, paid for by dropping PORCELAIN out of the hex set (it is white, and the base palette already holds a real neutral ramp, so it loses least). Four shades also cannot keep all three simultaneously-visible face directions distinct through a dot product — that is provable, not tuning, and it is why screen mode now shades by up-ness and screen-right-ness instead. If the palette ever widens past 64 slots, revisit both.",
    "THE HEX MODE IS THE OPEN QUESTION, and it is bigger than this cart. D's third setting abandons the dither and writes real ramps into palette slots 32-63, which are free (PALETTE_SIZE is 64 and only 0-31 are named). That is not off-grain: palette-and-color.md already carries a release gate that no paid app ships on the borrowed PICO-8 set. But the BUDGET is the finding — 32 slots buys six materials four shades each plus four households two each, and nothing for skin, trim, trousers or the floor. If the answer is 'real shades, always', the next question is not this cart's: it is whether the console's palette stops being 32 fixed entries, which is blend-tables and dynamic-palettes territory.",
    "PERSPECTIVE is not implemented and the seam is marked in pr_project: this is a stylized shear (the ground squash and the height scale are two independent numbers, inherited from the voxel bake's 2px-per-voxel convention, which no single orthographic elevation can produce). A real perspective divide is a different projection and a different question; add it only if the ortho look wins first.",
    "Instance yaw is not implemented — every object is placed in its authored orientation, and the room was laid out to suit that. The voxel set solves this by baking NS and EW wall variants; a poly set would rotate the mesh, which is one of the things this probe exists to prove is cheaper.",
    "Tri count is reported but frame cost is not. If the look wins, measure before wiring it in: node tools/profile-fleet.js polyroom, and canvas-diff for GPU-vs-software agreement (the 3D path is the one place they have historically diverged).",
    "The shapes are a FIRST TRANSCRIPTION, deliberately not a furniture catalogue, exactly as the voxel set is. Judge the READ (can you count the people, whose flat is that, is the loom machinery) and not the modelling."
  ],
  "description": {
    "summary": "One flat, two renderers, one key between them: low-poly triangles against the baked voxel sprites. Which one looks like people live there?",
    "detail": "tenement's problem is not its simulation, it is that you cannot see who lives in the building: at 24 pixels a resident is a one-tile blob in the same colour family as the furniture. This probe rebuilds the identical scene as flat-shaded low-poly triangles and puts the two renderings one keypress apart, so the comparison is a picture rather than an argument. Both halves walk the SAME item list at the SAME scale through the SAME projection, and at yaw 45 degrees the polygon projection reproduces the voxel one exactly, which is what makes TAB a fair test. What triangles buy: any camera angle instead of four baked ones, diagonals (a sofa back rakes, a torso widens into shoulders, a loom is skeletal), and lighting that can be pinned to the screen or left in the world. What they cost: crisp snapped edges, which crawl once the angle is not one the sprites were baked at.",
    "controls": "X toggles the depth buffer: the sorted painter's path against a real per-pixel depth test built in cart land. TAB flips POLY / VOXEL. Q and E turn: a free orbit in POLY, a quarter-turn step in VOXEL, because the sprites exist at four angles and nowhere in between. W and S raise and lower the camera (POLY only, for the same reason). Z cycles zoom 1x 2x 3x. D cycles the shading three ways: dithered half-shades, flat, or real hex ramps written into the palette's unused upper 32 slots. L moves the light between screen-fixed (what the voxel bake does) and world-fixed. H toggles per-household colour on the residents. SPACE resets the camera."
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
// RECOLOURING A BAKED SPRITE TAKES THREE pal() CALLS, NOT ONE, and finding that out is worth more
// than the feature. voxel-bake shades in SCREEN SPACE, so one material becomes a TRIPLE of palette
// indices — [top, screen-right, screen-left] — auto-derived down a luminance ramp. The shirt (a
// bare `b: 8`) bakes to [8, 24, 20], so pal(CLR_RED, …) alone recolours a third of a person and the
// tint reads as almost nothing. Re-derive with:
//     node -e "console.log(require('./tools/voxel-bake.js').materialTones(8))"
// The polygon half has no equivalent problem — a mesh takes its colour at draw time — and that
// asymmetry is a genuine point in the mesh's favour rather than an artefact of this cart.
static const unsigned char SHIRT_TONE[3] = { CLR_RED, CLR_DARK_RED, CLR_DARK_BROWN };
static const unsigned char HH_TONE[HH_COUNT][3] = {          // top, right, left — per household
    { CLR_DARK_PEACH, CLR_RED,          CLR_DARK_RED       },
    { CLR_GREEN,      CLR_MEDIUM_GREEN, CLR_BLUE_GREEN     },
    { CLR_INDIGO,     CLR_DARK_PURPLE,  CLR_DARKER_PURPLE  },
    { CLR_ORANGE,     CLR_BROWN,        CLR_DARK_BROWN     },
};

// ── the third way to shade: REAL colours, out of the palette's unused half ──
// The dither fakes in-between shades because a 32-colour palette has no room for them. But the
// palette is 64 slots wide (`PALETTE_SIZE 64`, studio.c) and only 0-31 are used — "only
// palette_hex() writes the upper half today". So the whole upper half is free, and a material can
// simply HAVE its shades instead of faking them.
//
// This is not off-grain. palette-and-color.md carries a release gate — no paid app ships on the
// borrowed PICO-8 set — so an original palette is already a prerequisite. What this mode probes is
// the follow-on question that gate does not answer: given our own palette, is a lo-fi look better
// served by dithered fake shades or by real ones? D cycles all three so it is a look, not a debate.
//
// THE BUDGET IS THE INTERESTING PART, and it is why the split below is uneven: 32 slots buys six
// materials four shades each (24) plus four households two each (8), and NOT a shade for every
// material. Skin, trim, trousers and the floor keep their base-palette entries — they are small or
// flat, and something always has to lose when the budget is a fixed 32.
// THE SPLIT IS 5 MATERIALS x 4 + 4 HOUSEHOLDS x 3, and the reallocation was forced by looking at it:
// two shades gave a resident's BOTH side faces the same dark tone, so a sleeping figure read as a
// bright top and one flat shadow, which is what "missing triangles" looks like. Three shades give
// the voxel bake's own [top, screen-right, screen-left] reading. The slot for it came from dropping
// PORCELAIN out of the hex set — it is white, and the base palette already holds a real neutral
// ramp (dark grey, light grey, white), so it loses least. 32 slots means something always loses.
#define PR_SLOT0    32                 // first free palette slot
#define PR_MAT_SH   4                  // shades per material group
#define PR_HH_SH    3                  // shades per household shirt
enum { SG_WOOD, SG_WALL, SG_METAL, SG_UPH, SG_CUSH, SG_COUNT };
static const int SG_BASE[SG_COUNT] = {              // seeded from the pico32 entry each replaces,
    0xab5236, 0x754665, 0xc2c3c7,                   // so identity survives the change of technique
    0x29adff, 0xff77a8,
};
static const int HH_BASE[HH_COUNT] = { 0xff004d, 0x00e436, 0x83769c, 0xffa300 };
static signed char SG_OF[M_COUNT];     // material -> smooth group, or -1

// Shades are NOT a multiply. A flat scale toward black is what makes cheap 3D look like plastic;
// real shadow goes cool and real highlight goes warm, so the ramp bends through a violet dark and a
// warm white. Same trick a painter uses, and the reason four shades can carry a whole material.
static int pr_mix(int a, int b, float t) {
    const int ar = (a>>16)&255, ag = (a>>8)&255, ab = a&255;
    const int br = (b>>16)&255, bg = (b>>8)&255, bb = b&255;
    const int r = (int)(ar + (br-ar)*t), g = (int)(ag + (bg-ag)*t), bl = (int)(ab + (bb-ab)*t);
    return (r<<16) | (g<<8) | bl;
}
static void pr_build_palette(void) {
    const int SHADOW = 0x1a1420, LIGHT = 0xfff1e8;   // cool dark, warm light
    for (int g = 0; g < SG_COUNT; g++)
        for (int s = 0; s < PR_MAT_SH; s++) {
            const float t = (float)s / (PR_MAT_SH - 1);       // 0 = darkest, 1 = lightest
            const int hex = (t < 0.5f) ? pr_mix(SG_BASE[g], SHADOW, (0.5f - t) * 1.5f)
                                       : pr_mix(SG_BASE[g], LIGHT,  (t - 0.5f) * 1.1f);
            palette_hex(PR_SLOT0 + g * PR_MAT_SH + s, hex);
        }
    for (int h = 0; h < HH_COUNT; h++)
        for (int s = 0; s < PR_HH_SH; s++) {
            const float t = (float)s / (PR_HH_SH - 1);
            // Households shade LESS far toward the cool shadow than materials do (0.9 against 1.5).
            // The cool-shadow trick overshoots on a warm hue: amber mixed 65% into a violet dark
            // comes out OLIVE, which reads as a different material rather than as the same blanket
            // in shade. A resident is also the thing you most need to recognise at a glance.
            const int hex = (t < 0.5f) ? pr_mix(HH_BASE[h], SHADOW, (0.5f - t) * 0.9f)
                                       : pr_mix(HH_BASE[h], LIGHT,  (t - 0.5f) * 0.9f);
            palette_hex(PR_SLOT0 + SG_COUNT * PR_MAT_SH + h * PR_HH_SH + s, hex);
        }
    for (int m = 0; m < M_COUNT; m++) SG_OF[m] = -1;
    SG_OF[M_WOOD] = SG_WOOD;   SG_OF[M_WALL] = SG_WALL;   SG_OF[M_METAL] = SG_METAL;
    SG_OF[M_UPH]  = SG_UPH;    SG_OF[M_CUSH] = SG_CUSH;   // PORC falls back to the base neutral ramp
}

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
// Note the y values: seat, arms and cushion all start clear of 1.9, which is where the RAKED back's
// bounding box ends. Nothing here overlaps anything — see the spec's interpenetration gate.
static const Part P_SOFA[] = {
    PRISM(0,0,0, 12,6,2.4f,  0.4f,0.4f,-0.4f,-0.4f, M_WOOD),        // plinth, tapered to fake feet
    PRISM(0,0,2.4f, 12,1.9f,6,  0,-0.9f,0,-0.9f,    M_UPH),         // back, RAKED
    BOX  (1.35f,1.95f,2.4f, 10.65f,6,3.9f,          M_UPH),         // seat
    PRISM(0,1.95f,2.4f, 1.3f,6,4.7f, 0.2f,0,-0.2f,0, M_UPH),        // arms, tapered
    PRISM(10.7f,1.95f,2.4f, 12,6,4.7f, 0.2f,0,-0.2f,0, M_UPH),
    BOX  (1.4f,2.05f,3.9f, 10.6f,5.4f,4.4f,         M_CUSH),        // a cushion, the colour accent
};

// bed 6x12x4 — and the FOUR is the constraint, not a suggestion. The voxel bed is four voxels tall
// (frame 2, mattress 1, cover 1) because the models file's rule is that the bed stays LOW so its
// outline cannot be confused with the sofa's. The first draft of this mesh gave it a 5.6-tall raked
// headboard, which looked better and was cheating: pr_selfcheck caught it, and that is the whole
// reason the check exists. A look probe that quietly makes the new furniture bigger proves nothing.
// So the diagonals here have to earn their keep INSIDE four voxels: the headboard rakes, the duvet
// is a wedge thicker at the foot, and the pillow is a tapered pad.
static const Part P_BED[] = {
    PRISM(0,0,0, 6,1,4.0f,  0,0.5f,0,0.5f,           M_WOOD),       // headboard, RAKED back
    BOX  (0,1.6f,0, 6,12,1.8f,                       M_WOOD),       // frame, clear of the rake
    BOX  (0.4f,1.8f,1.8f, 5.6f,11.6f,2.6f,           M_PORC),       // mattress
    PRISM(0.9f,1.9f,2.6f, 5.1f,3.8f,3.3f, 0.3f,0.2f,-0.3f,-0.2f, M_PORC), // pillow
    // Duvet top is 4.0, NOT 3.7, and the 0.3 matters: a sleeper is placed at the bed's DECLARED
    // height (ISO_FOOTPRINT's nz = 4), so a duvet stopping at 3.7 left the figure floating a third
    // of a voxel above it. That gap is under a pixel wide, so it rasterized as a ragged dashed line
    // of duvet showing under the sleeper's edge — which reads as a torn seam rather than as a gap.
    // The lesson generalises: anything a resident stands or lies ON must reach its declared height.
    PRISM(0.3f,4,2.6f, 5.7f,11.8f,4.0f, 0.3f,0,-0.3f,-0.4f, M_CUSH),// duvet, wedged
};

// toilet 6x6x6 — tall tank behind, flaring bowl in front. The flare is the point: a cylinder read
// out of a prism, which is as close to round as this scale needs.
// The bowl flares FORWARD only (dy0 = 0). Flaring it backward too pushed it into the cistern, and a
// painter's sort cannot draw two solids that pass through each other — it tore along the join.
static const Part P_TOILET[] = {
    PRISM(1,0,0, 5,2,5.4f,  0.2f,0.1f,-0.2f,-0.1f,   M_PORC),       // cistern
    PRISM(1.6f,2.05f,0, 4.4f,5.2f,2.4f, -0.5f,0,0.5f,0.3f, M_PORC), // bowl, FLARED outward
    PRISM(1,2.05f,2.4f, 5,5.5f,3.0f, 0.1f,0.1f,-0.1f,-0.1f, M_PORC),// seat
    BOX  (1.4f,0.4f,5.4f, 4.6f,1.8f,5.8f,            M_PORC),       // lid on the cistern
};

// fridge 6x6x12 — as tall as the figure. A box, honestly, but the door seam and handle are
// geometry rather than a painted-on voxel row, and the top is slightly tapered so the silhouette
// is not a perfect rectangle at every angle.
// Door seam and handle sit FLUSH ON the +y face, not sunk into it. Two reasons, and the second one
// only showed up on screen: sinking them 0.15 into the body is an interpenetration, and +y is the
// face you can actually SEE (the camera looks at each object's +x and +y sides), so details on the
// -y face were both wrong and invisible.
static const Part P_FRIDGE[] = {
    PRISM(0,0,0, 6,6,11.6f,  0.15f,0.15f,-0.15f,-0.15f, M_METAL),
    BOX  (0.3f,6.0f,5.4f, 5.7f,6.25f,5.9f,           M_TRIM),       // door seam
    BOX  (4.4f,6.0f,6.2f, 5.3f,6.45f,9.8f,           M_TRIM),       // handle, standing a little prouder
    PRISM(0,0,11.6f, 6,6,12,  0.15f,0.15f,-0.15f,-0.15f, M_TRIM),   // dark top, kills the box read
};

// counter 6x6x7 — the toe kick (bottom sheared in) and the overhanging top are both things a
// voxel grid cannot do at this size, and together they are what makes it read as joinery.
// The worktop still overhangs — but it overhangs the CABINET, not the tile. Sticking it out to
// -0.3 put it inside the neighbouring fridge and inside the back wall, which is a spatial conflict
// in a game where a tile is a tile. Slimming the cabinet keeps the read and stays in its square.
static const Part P_COUNTER[] = {
    PRISM(0.6f,0.8f,0, 5.4f,6,5.4f, -0.3f,-0.5f,0.3f,0,  M_WOOD),   // cabinet, kicked in at the toe
    BOX  (0,0,5.4f, 6,6,6.2f,                        M_METAL),      // worktop, overhangs the cabinet
    BOX  (0.5f,6.0f,2.4f, 5.5f,6.15f,2.7f,           M_TRIM),       // drawer line, on the seen face
};

// loom 6x4x12 — the punch list says the voxel loom "reads as a second wardrobe". This is the
// clearest test in the cart: skeletal legs, a top beam, and a RAKED WARP PLANE. A leaning plane is
// exactly the shape that says machine, and it is exactly the shape a voxel grid cannot make.
// This one was the worst offender on screen: the warp board ran x 1.0..5.0 straight THROUGH both
// posts (x 0.2..1.2 and 4.8..5.8), and the beam sank into their tops. Interpenetration is the one
// artifact a painter's sort cannot resolve at any granularity, and it shredded the board into
// slivers. The warp now spans strictly BETWEEN the posts and the beam sits ON them.
static const Part P_LOOM[] = {
    BOX  (0.2f,0.2f,0, 1.2f,1.2f,11,                 M_WOOD),       // four posts
    BOX  (4.8f,0.2f,0, 5.8f,1.2f,11,                 M_WOOD),
    BOX  (0.2f,2.8f,0, 1.2f,3.8f,9,                  M_WOOD),
    BOX  (4.8f,2.8f,0, 5.8f,3.8f,9,                  M_WOOD),
    BOX  (1.3f,1.3f,2.4f, 4.7f,3.1f,3.1f,            M_WOOD),       // treadle bar
    PRISM(1.3f,1.0f,3.2f, 4.7f,1.6f,10.4f, 0,2.0f,0,2.0f, M_CUSH),  // the warp, LEANING
    BOX  (0,0.4f,11.0f, 6,3.6f,11.9f,                M_WOOD),       // top beam, resting on the posts
};

// wardrobe 6x4x10 — chest height, obviously a box you open. The cornice overhang is the read.
static const Part P_WARDROBE[] = {
    PRISM(0.4f,0.4f,0, 5.6f,4.0f,9.4f, 0.1f,0.1f,-0.1f,-0.1f, M_WOOD),
    BOX  (2.8f,4.0f,0.6f, 3.2f,4.25f,9.2f,           M_TRIM),       // door gap, on the seen face
    BOX  (0,0.2f,9.4f, 6,4.2f,10,                    M_WOOD),       // cornice, overhangs the body
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
    PRISM(0.5f,0,0.2f, 2.5f,1.3f,1.9f, 0.2f,0.15f,-0.2f,-0.15f, M_SKIN),
    PRISM(0.2f,1.4f,0, 2.8f,7.6f,1.6f, 0.2f,0,-0.2f,-0.4f, M_SHIRT),
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
    // The fridge is at (0,1) rather than the corner (0,0) deliberately. A 12-voxel tower in the far
    // corner is the worst place in the room: at one yaw it is the furthest thing and the sofa lands
    // on the same screen column, at the opposite yaw the two cut-away corner walls pass in front of
    // its base and it reads as standing outside the building. One tile in fixes both.
    pr_add_tile(ISO_FRIDGE,   0, 1, -1);
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

enum { SH_DITHER, SH_FLAT, SH_SMOOTH, SH_COUNT };   // D cycles these
static const char *SH_NAME[SH_COUNT] = { "dither", "flat", "hex" };
static float pr_yaw = 45.0f, pr_squash = 1.0f;
// Boots into POLY + HEX because that is the combination the maker picked (2026-08-13). D still
// cycles back to the dither and TAB back to the sprites — the comparison is the cart, so nothing
// was removed, only reordered.
static int   pr_zoom = 1, pr_poly = 1, pr_shade = SH_SMOOTH, pr_lightcam = 1, pr_tint = 1;
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
            // The EW runs are stretched 2 voxels PAST the room at each end so they cover the CORNER
            // squares. Four runs that stop exactly at the room bounds leave a 2x2 hole at every
            // corner, which is the notch the maker photographed — the floor showed through it.
            // Stretching the EW pair (not both pairs) means the runs still only TOUCH, never overlap.
            const float ly = ew ? (ROOM_H * TILE + 4.0f) / TILE : 1.0f;
            pr_add_run(cell, side[s].vx, ew ? -2.0f : side[s].vy, ew ? 1.0f : (float)ROOM_W, ly);
        } else {
            const int n = ew ? ROOM_H : ROOM_W;
            for (int i = 0; i < n; i++)
                pr_add(cell, side[s].vx + (ew ? 0 : (float)(i * TILE)),
                             side[s].vy + (ew ? (float)(i * TILE) : 0), 0, -1);
        }
    }
    // The WC stubs are one tile each and always low, so they need no run. Both sit OUTSIDE the WC
    // tile, the same convention the perimeter uses: a wall two voxels thick placed at TILE-2 would
    // sit INSIDE the tile and share space with the toilet standing there, which is exactly the
    // "the wall renders over the toilet" the maker spotted. Not a sort bug — the two solids really
    // were in the same place, and a painter's sort has no right answer for that.
    pr_add(ISO_WALL_LOW_EW, (float)(5 * TILE) - 2.0f, 0.0f,          0, -1);
    pr_add(ISO_WALL_LOW_NS, (float)(5 * TILE),        (float)TILE,   0, -1);
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
// zv[] is the PER-VERTEX depth, needed only by the z-buffer path: a painter's sort wants one number
// per face, a depth test wants one per pixel. Screen-space linear interpolation of depth is EXACT
// here rather than approximate, because the projection is orthographic — there is no perspective
// divide, so depth really is affine in screen x/y. A perspective renderer would need 1/z.
typedef struct { int x[3], y[3]; float zv[3]; float depth, bright; unsigned char mat, hh; } Tri;
#define MAX_TRIS 3072
static Tri pr_tri[MAX_TRIS];
static int pr_tri_n, pr_tri_drawn, pr_order[MAX_TRIS];
static float pr_key[MAX_TRIS];
static float pr_max_span;    // worst depth range any single quad spans — the sort's soundness bound

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

static void pr_tri_push(const float a[3], const float b[3], const float c[3],
                        float bright, float depth, int mat, int hh) {
    if (pr_tri_n >= MAX_TRIS) return;
    Tri *t = &pr_tri[pr_tri_n];
    const float *v[3] = { a, b, c };
    float dx, dy, dz; pr_viewdir(&dx, &dy, &dz);
    for (int i = 0; i < 3; i++) {
        float sx, sy; pr_project(v[i][0], v[i][1], v[i][2], &sx, &sy);
        t->x[i] = (int)(sx + pr_cx); t->y[i] = (int)(sy + pr_cy);
        t->zv[i] = -(v[i][0]*dx + v[i][1]*dy + v[i][2]*dz);   // negated nearness: smaller = nearer
    }
    t->depth = depth; t->bright = bright;
    t->mat = (unsigned char)mat; t->hh = (unsigned char)(hh < 0 ? 255 : hh);
    pr_tri_n++;
}

// THE QUAD IS THE UNIT, NOT THE TRIANGLE, and both halves of that fix a real artifact.
//
// 1. ONE NORMAL PER FACE. A prism whose top corners move by DIFFERENT amounts has non-planar side
//    faces (the duvet, the toilet bowl, every tapered torso), so a per-triangle cross product gives
//    the two halves different normals — and a visible diagonal seam straight across the face. The
//    Newell normal is the best-fit plane for a polygon that has no exact one, which is exactly the
//    case here.
// 2. ONE DEPTH PER FACE. Sorting each triangle by its own centroid let the two halves of one face
//    sort INDEPENDENTLY — so on a thin leaning slab a triangle of the top face landed in front of a
//    triangle of the back face and cut a bright diagonal sliver through it. That was the loom's
//    warp board looking shredded. Two triangles of one quad can never occlude each other, so giving
//    them one key is not a heuristic, it is the correct grouping. zsort's insertion pass leaves
//    equal keys in insertion order, so they stay adjacent.
//
// What this does NOT fix, and cannot: two PARTS that genuinely interpenetrate. A painter's sort has
// no answer for that at any granularity — it needs a z-buffer. The rule that falls out is an
// AUTHORING rule, gated in spec(): parts must ABUT, never overlap.
static void pr_quad(const float a[3], const float b[3], const float c[3], const float d[3],
                    int mat, int hh) {
    const float *p[4] = { a, b, c, d };
    float n[3] = { 0, 0, 0 };
    for (int i = 0; i < 4; i++) {                        // Newell
        const float *u = p[i], *w = p[(i + 1) & 3];
        n[0] += (u[1] - w[1]) * (u[2] + w[2]);
        n[1] += (u[2] - w[2]) * (u[0] + w[0]);
        n[2] += (u[0] - w[0]) * (u[1] + w[1]);
    }
    const float len = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    if (len < 1e-6f) return;                             // degenerate: a zero-thickness part face
    n[0] /= len; n[1] /= len; n[2] /= len;

    float dx, dy, dz; pr_viewdir(&dx, &dy, &dz);
    if (n[0]*dx + n[1]*dy + n[2]*dz <= 0.0f) return;     // backface. With no z-buffer this is not
                                                         // an optimization, it is correctness
    // ── BRIGHTNESS, and the two modes are genuinely different KINDS of thing ────────────────────
    //
    // SCREEN mode is not "the light rotates with the camera" — that framing is what broke it. A
    // dot product against any single light direction TIES two perpendicular faces at whichever yaw
    // puts them symmetric about the light, and then the corner between them vanishes: a fridge came
    // out as one flat grey pane over part of the orbit. The voxel bake never has this problem
    // because it does not use a dot product at all — it assigns [top, screen-right, screen-left] by
    // face DIRECTION, and screen-right is never equal to screen-left.
    //
    // So screen mode shades by UP-ness and SCREEN-RIGHT-ness, which is that rule made continuous so
    // rakes and tapers still get in-between values. Why it cannot tie: the two visible side faces
    // are perpendicular in the ground plane and the view direction BISECTS them, so their
    // screen-right-ness is always opposite in sign. Guaranteed at every yaw, not tuned per angle.
    //
    // Screen-right is (cos a, -sin a, 0), NOT the world x axis — sx = (x cos a - y sin a) * SX, so
    // +y world moves screen-LEFT. Getting that wrong is why the first fill-light attempt failed:
    // it gave +y and -y the same right-ness (zero) and could not separate them.
    float br;
    if (pr_lightcam) {
        const float ar = pr_yaw * 3.14159265f / 180.0f;
        const float right = n[0] * cosf(ar) - n[1] * sinf(ar);
        const float up = n[2] > 0.0f ? n[2] : 0.0f;
        // Weights are not free parameters: the two side faces must land in DIFFERENT shade bins at
        // every yaw in the quadrant, and the top must clear both. 0.20/0.38/0.40 does that with the
        // least contrast — the first pass used 0.12/0.42/0.45, which separated them and made the
        // shaded side nearly black, trading one artifact for a worse-looking picture.
        br = 0.20f + 0.38f * up + 0.40f * (0.5f + 0.5f * right);
    } else {
        // WORLD mode keeps a real directional light — each face then has a CONSTANT brightness as
        // you orbit, which is the honest reading of a room turning under a fixed sun. The ambient is
        // a fill rather than a flat floor so two unlit faces (the -x/-y pair) do not both bottom out
        // onto the same shade, which was the other half of the flat-fridge bug.
        const float d = n[0]*LX + n[1]*LY + n[2]*LZ;
        const float fill = 0.14f + 0.16f * (0.5f + 0.5f * n[0]);
        br = fill + (1.0f - fill) * (d < 0.0f ? 0.0f : d);
    }
    if (br > 1.0f) br = 1.0f;  if (br < 0.0f) br = 0.0f;

    // ── SUBDIVISION: the requirement that makes a painter's sort SOUND ──────────────────────────
    // A painter's sort gives each polygon ONE depth, so a polygon that spans a large depth range is
    // unsortable by construction — no single answer is right for all of it. Merging each wall side
    // into one long box (to kill the per-tile seams) made its inner face a single quad 36 voxels
    // long and 12 tall, and at some angles its NEAR end is genuinely in front of the toilet while
    // its FAR end is genuinely behind. One quad cannot be both, so it was drawn over everything.
    //
    // BOTH AXES MATTER, and the vertical one is the counter-intuitive half: height contributes to
    // depth in this projection (the view ray has a +z component), so a 12-tall wall's face centroid
    // sits at z=6 while the toilet's sits at z=2.7 — and the wall won on HEIGHT alone even after
    // splitting along its length. Splitting vertically too puts the overlapping piece at z=3, which
    // sorts correctly. Measured, not guessed: yaw 315 was the failing case.
    //
    // The normal and brightness are computed ONCE for the whole face and shared, so subdividing is
    // invisible in the shading — it only changes the sort granularity.
    const float ux = b[0]-a[0], uy = b[1]-a[1], uz = b[2]-a[2];
    const float vx = d[0]-a[0], vy = d[1]-a[1], vz = d[2]-a[2];
    int nu = (int)(sqrtf(ux*ux + uy*uy + uz*uz) / (float)TILE + 0.999f);
    int nv = (int)(sqrtf(vx*vx + vy*vy + vz*vz) / (float)TILE + 0.999f);
    if (nu < 1) nu = 1;  if (nu > 8) nu = 8;
    if (nv < 1) nv = 1;  if (nv > 8) nv = 8;

    for (int j = 0; j < nv; j++) for (int i = 0; i < nu; i++) {
        const float s0 = (float)i / nu, s1 = (float)(i+1) / nu;
        const float t0 = (float)j / nv, t1 = (float)(j+1) / nv;
        float q[4][3];
        const float ss[4] = { s0, s1, s1, s0 }, tt[4] = { t0, t0, t1, t1 };
        for (int k = 0; k < 4; k++) for (int e = 0; e < 3; e++) {
            // bilinear over the face's own corners, so sub-quads share vertices EXACTLY and the
            // subdivision cannot open a crack
            const float top = a[e] + (b[e] - a[e]) * ss[k];
            const float bot = d[e] + (c[e] - d[e]) * ss[k];
            q[k][e] = top + (bot - top) * tt[k];
        }
        float cen[3] = { 0, 0, 0 };
        for (int k = 0; k < 4; k++) for (int e = 0; e < 3; e++) cen[e] += q[k][e];
        // NEGATED nearness — see pr_viewdir. zsort draws the big key first, so big must mean far.
        const float depth = -(cen[0]*dx + cen[1]*dy + cen[2]*dz) * 0.25f;
        const float span = fabsf(ux*dx + uy*dy + uz*dz) / nu
                         + fabsf(vx*dx + vy*dy + vz*dz) / nv;
        if (span > pr_max_span) pr_max_span = span;
        pr_tri_push(q[0], q[1], q[2], br, depth, mat, hh);
        pr_tri_push(q[0], q[2], q[3], br, depth, mat, hh);
    }
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
// Decide a face's colour WITHOUT drawing it, so the sorted path and the depth-tested path cannot
// drift apart on shading — the only thing X may change is which pixels survive. *hi/*lo carry the
// dither pair when the face is dithered, and are -1 when it is solid.
static void pr_shade_of(const Tri *t, int *color, int *hi, int *lo) {
    *hi = -1; *lo = -1;
    // THE FLOOR IS NEVER SHADED AND NEVER DITHERED, and both halves of that are deliberate. It is
    // one flat plane at one brightness, so a checker between two shades cannot read as shading —
    // it can only read as a chequered lino, which is what it did (a 50% brown/tan pattern over the
    // whole room, which looked like a texture decision and was an accident). Drawing it as the
    // literal two browns the voxel half uses also keeps the A/B honest: the floor is then the same
    // pixels in both modes, so every difference you see is furniture and people.
    if (t->mat >= M_FLOOR_A) { *color = RAMP[t->mat][2]; return; }
    const int is_shirt = (t->hh < HH_COUNT && pr_tint && t->mat == M_SHIRT);

    // HEX mode: the material simply has its shades, out of the palette's free upper half. No
    // pattern, no ramp-hopping between unrelated hues — just the colour, darker and lighter.
    if (pr_shade == SH_SMOOTH) {
        int slot = -1;
        if (is_shirt) {
            // Three bands chosen to land the three faces a standing figure shows on three DIFFERENT
            // shades (top 0.87, screen-right 0.61, screen-left 0.42) — the voxel bake's own reading.
            const int s = t->bright > 0.72f ? 2 : (t->bright > 0.50f ? 1 : 0);
            slot = PR_SLOT0 + SG_COUNT * PR_MAT_SH + t->hh * PR_HH_SH + s;
        } else if (SG_OF[t->mat] >= 0) {
            int s = (int)(t->bright * (PR_MAT_SH - 0.001f));
            if (s > PR_MAT_SH - 1) s = PR_MAT_SH - 1;  if (s < 0) s = 0;
            slot = PR_SLOT0 + SG_OF[t->mat] * PR_MAT_SH + s;
        }
        if (slot >= 0) { *color = slot; return; }
        // No slot in the budget (skin, trim, trousers) — fall through to the flat base ramp.
    }

    const unsigned char *ramp = is_shirt ? HH_RAMP[t->hh] : RAMP[t->mat];
    // THE DITHER HAS A SIZE LIMIT, and finding it is one of the things this cart is for. A 50%
    // checker of two palette colours reads as a SHADE on a small face and as WALLPAPER on a big
    // one — the wall and fridge faces came out visibly chequered, which looked like a texture
    // decision and was an accident of the trick's range. So a triangle past the threshold snaps to
    // the nearest solid step. Threshold is in canvas pixels because the 4x4 pattern is: it does not
    // scale with the zoom, so a face that grows past it genuinely does start showing the weave.
    const long area2 = labs((long)(t->x[1]-t->x[0]) * (t->y[2]-t->y[0])
                          - (long)(t->x[2]-t->x[0]) * (t->y[1]-t->y[0]));   // 2x the triangle area
    if (pr_shade != SH_DITHER || area2 > 400) {
        int s = (int)(t->bright * 3.999f); if (s > 3) s = 3; if (s < 0) s = 0;
        *color = ramp[s];
        return;
    }
    int s = (int)(t->bright * 6.999f); if (s > 6) s = 6; if (s < 0) s = 0;
    *color = ramp[s / 2];
    if (s & 1) { *color = ramp[s / 2 + 1]; *hi = ramp[s / 2 + 1]; *lo = ramp[s / 2]; }
}

static void pr_shade_fill(const Tri *t) {
    int col, hi, lo; pr_shade_of(t, &col, &hi, &lo);
    if (hi >= 0) {
        fillp(FILL_CHECKER, lo);
        trifill(t->x[0], t->y[0], t->x[1], t->y[1], t->x[2], t->y[2], hi);
        fillp_reset();                 // fillp is STICKY and into the next frame — always reset
    } else {
        trifill(t->x[0], t->y[0], t->x[1], t->y[1], t->x[2], t->y[2], col);
    }
}

// Split from the draw so spec() can assert on the triangle list without rasterizing anything.
static void pr_build_tris(void) {
    pr_tri_n = 0;
    pr_max_span = 0.0f;
    // Floor: one quad per tile, alternating, same two browns the voxel half uses. Flat on z=0, so
    // it needs no normal — but it goes through the same emit so it sorts with everything else.
    for (int ty = 0; ty < ROOM_H; ty++) for (int tx = 0; tx < ROOM_W; tx++) {
        float x0 = (float)(tx*TILE), y0 = (float)(ty*TILE);
        float x1 = x0 + TILE, y1 = y0 + TILE;
        // THE OUTER TILES RUN 2 VOXELS UNDER THE WALLS, which closes the dotted dark line that
        // showed along every floor-to-wall junction. The floor's outer edge and the wall's base are
        // the SAME LINE in space, and two polygons meeting exactly on a line leave gaps under
        // pixel-centre coverage: neither owns the boundary pixel, so the background shows through in
        // a dashed pattern. This is a CRACK, not a sorting error — the sort-vs-depth-buffer scan
        // showed it identically in both paths, which is how it was told apart. Overlapping by the
        // wall thickness is the standard fix and costs nothing, because the wall hides the overlap.
        if (tx == 0)          x0 -= 2.0f;
        if (ty == 0)          y0 -= 2.0f;
        if (tx == ROOM_W - 1) x1 += 2.0f;
        if (ty == ROOM_H - 1) y1 += 2.0f;
        const float a[3] = {x0,y0,0}, b[3] = {x1,y0,0};
        const float c[3] = {x1,y1,0}, d[3] = {x0,y1,0};
        pr_quad(a, b, c, d, ((tx + ty) & 1) ? M_FLOOR_A : M_FLOOR_B, -1);
    }
    for (int i = 0; i < pr_item_n; i++) {
        const Item *it = &pr_item[i];
        const Mesh *m = &MESHES[it->cell];
        for (int p = 0; p < m->n; p++)
            pr_part(&m->p[p], it->vx, it->vy, it->vz, it->hh, it->lx, it->ly);
    }
}

// ── A Z-BUFFER, IN CART LAND ────────────────────────────────────────────────
// Built here rather than in the engine ON PURPOSE, and the reasoning matters more than the code.
// Everything the sorted path needs — one depth per face, subdivide to a tile, parts must abut, walls
// merged for seams but split for sorting — is compensation for not having a depth test. Whether the
// depth test is worth having is a question about the PICTURE, so it gets answered by looking, and
// looking needs no engine change. X toggles it, so the artifacts appear and disappear side by side.
//
// WHERE IT WOULD EVENTUALLY LIVE IS STILL THE ENGINE, and this cart cannot pretend otherwise: from
// cart land every pixel run is a `rectfill` call through the full public path, where the engine
// writes its framebuffer directly. So treat the frame cost below as an upper bound on a real
// implementation and NOT as evidence against one — that would be measuring the wrapper.
//
// The depth test also makes the sort unnecessary rather than better: in this mode zsort is not
// called at all and draw order does not matter. That IS the argument, in one sentence.
static float pr_zbuf[SCREEN_W * SCREEN_H];
static int   pr_zbuf_on = 0, pr_zspans;
#define PR_ZFAR 1e18f

// Flat-shaded triangle with a depth test, as edge functions stepped incrementally. Runs of pixels
// that pass are emitted as ONE rectfill, because a flat triangle's passing pixels are contiguous
// until something occludes them — that keeps the call count near the scanline count rather than the
// pixel count, which is the difference between this being usable and being a slideshow.
static void pr_raster_z(const Tri *t, int color, int dither_hi, int dither_lo) {
    int x0 = t->x[0], y0 = t->y[0], x1 = t->x[1], y1 = t->y[1], x2 = t->x[2], y2 = t->y[2];
    float z0 = t->zv[0], z1 = t->zv[1], z2 = t->zv[2];
    float area = (float)(x1-x0) * (y2-y0) - (float)(x2-x0) * (y1-y0);
    if (area == 0.0f) return;
    if (area < 0.0f) {                                  // keep one winding so "inside" is one test
        int tx = x1; x1 = x2; x2 = tx;  int ty = y1; y1 = y2; y2 = ty;
        float tz = z1; z1 = z2; z2 = tz;  area = -area;
    }
    int lox = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int hix = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int loy = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int hiy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    if (lox < 0) lox = 0;  if (hix > SCREEN_W - 1) hix = SCREEN_W - 1;
    if (loy < 0) loy = 0;  if (hiy > SCREEN_H - 1) hiy = SCREEN_H - 1;
    if (lox > hix || loy > hiy) return;

    const float inv = 1.0f / area;
    // w0 is the weight of vertex 0 and is the edge function of the OPPOSITE edge (v1,v2).
    const float a0 = -(float)(y2 - y1), b0 = (float)(x2 - x1);
    const float a1 = -(float)(y0 - y2), b1 = (float)(x0 - x2);
    const float a2 = -(float)(y1 - y0), b2 = (float)(x1 - x0);

    for (int y = loy; y <= hiy; y++) {
        const float py = (float)y + 0.5f, px0 = (float)lox + 0.5f;
        float w0 = a0 * (px0 - (float)x1) + b0 * (py - (float)y1);
        float w1 = a1 * (px0 - (float)x2) + b1 * (py - (float)y2);
        float w2 = a2 * (px0 - (float)x0) + b2 * (py - (float)y0);
        int run_x = -1, run_n = 0, run_col = 0;
        for (int x = lox; x <= hix; x++, w0 += a0, w1 += a1, w2 += a2) {
            int keep = 0, col = color;
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                const float z = (w0 * z0 + w1 * z1 + w2 * z2) * inv;
                float *slot = &pr_zbuf[y * SCREEN_W + x];
                if (z < *slot) {                        // smaller = nearer
                    *slot = z; keep = 1;
                    // The dither is a 50% checker, so in this path it is a per-pixel parity choice
                    // rather than a global fillp — fillp cannot be honoured one pixel at a time.
                    if (dither_hi >= 0) col = ((x ^ y) & 1) ? dither_hi : dither_lo;
                }
            }
            if (keep && (run_n == 0 || col == run_col)) {
                if (run_n == 0) { run_x = x; run_col = col; }
                run_n++;
            } else {
                if (run_n) { rectfill(run_x, y, run_n, 1, run_col); pr_zspans++; }
                run_n = keep;  run_x = x;  run_col = col;
            }
        }
        if (run_n) { rectfill(run_x, y, run_n, 1, run_col); pr_zspans++; }
    }
}

static void pr_draw_poly(void) {
    pr_build_tris();
    if (pr_zbuf_on) {
        // NO SORT AT ALL. Not a faster sort, not a better one — none. Draw order stops mattering,
        // which is the whole argument for a depth buffer in one line of code.
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) pr_zbuf[i] = PR_ZFAR;
        pr_zspans = 0;
        for (int i = 0; i < pr_tri_n; i++) {
            int col, hi, lo; pr_shade_of(&pr_tri[i], &col, &hi, &lo);
            pr_raster_z(&pr_tri[i], col, hi, lo);
        }
    } else {
        for (int i = 0; i < pr_tri_n; i++) pr_key[i] = pr_tri[i].depth;
        zsort(pr_key, pr_order, pr_tri_n);             // far -> near; approximate by construction
        for (int i = 0; i < pr_tri_n; i++) pr_shade_fill(&pr_tri[pr_order[i]]);
    }
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
        // Undone with pal(c,c) rather than pal_reset(), deliberately: pal_reset() restores the
        // SHIPPED palette, which would wipe the hex ramps written into slots 32-63 the moment you
        // visited voxel mode and came back.
        const int tinted = pr_tint && it->hh >= 0 && it->hh < HH_COUNT;
        if (tinted) for (int k = 0; k < 3; k++) pal(SHIRT_TONE[k], HH_TONE[it->hh][k]);
        // The DESTINATION rect scales with the zoom, and the cell's origin offset with it. Drawing
        // a baked cell 1:1 while its POSITION scales pulls the room apart into floating pillars —
        // it looked like a rendering finding and was a bug in this cart. Integer upscale only,
        // which is what the engine already does for the window scale.
        sspr(c->x, c->y, c->w, c->h,
             (int)(sx + pr_cx) - c->ox * pr_zoom, (int)(sy + pr_cy) - c->oy * pr_zoom,
             c->w * pr_zoom, c->h * pr_zoom);
        if (tinted) for (int k = 0; k < 3; k++) pal(SHIRT_TONE[k], SHIRT_TONE[k]);
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
    pr_build_palette();
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
    if (pr_poly) {
        if (key('Q')) pr_yaw -= SPIN;
        if (key('E')) pr_yaw += SPIN;
        if (key('W')) { pr_squash += TILT; if (pr_squash > 1.35f) pr_squash = 1.35f; }
        if (key('S')) { pr_squash -= TILT; if (pr_squash < 0.30f) pr_squash = 0.30f; }
    } else {
        // THE VOXEL HALF TURNS IN QUARTERS, and this is not a courtesy — it is the only thing it
        // can do. Letting Q/E run the yaw continuously here put the LAYOUT at 67 degrees while
        // every sprite was still the 45-degree bake: the walls staircased apart, the furniture
        // faced a direction the room did not, and the whole flat came to pieces. That is what the
        // four baked angles MEAN, and the honest way to show it is to make the camera step.
        if (keyp('Q')) pr_yaw -= 90.0f;
        if (keyp('E')) pr_yaw += 90.0f;
    }
    if (keyp(KEY_TAB)) {
        pr_poly = !pr_poly;
        // Leaving POLY snaps to a baked angle, because the sprites have exactly four and there is
        // nothing to interpolate. The snap IS one of the findings; do not smooth it away.
        if (!pr_poly) { pr_yaw = 45.0f + 90.0f * pr_rot_index(); pr_squash = 1.0f; }
    }
    if (keyp('Z')) pr_zoom = pr_zoom % 3 + 1;
    if (keyp('D')) pr_shade = (pr_shade + 1) % SH_COUNT;
    if (keyp('X')) pr_zbuf_on = !pr_zbuf_on;
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
    print(str("yaw %3.0f  %dx  tris %d  %s", pr_yaw, pr_zoom, pr_tri_drawn,
              pr_poly ? (pr_zbuf_on ? "ZBUF" : "sort") : ""),
          32, SCREEN_H - 17, pr_zbuf_on ? CLR_LIME_GREEN : CLR_LIGHT_GREY);
    print(str("TAB a/b  QE turn  WS tilt  Z zoom  X depth:%s  D shade:%s  L light:%s  H hh:%s",
              pr_zbuf_on ? "ON" : "sort", SH_NAME[pr_shade],
              pr_lightcam ? "screen" : "world", pr_tint ? "on" : "off"),
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

    // 2b. NO TWO PARTS OF A MESH MAY INTERPENETRATE. This is the one artifact class the renderer
    //     cannot fix at any granularity: a painter's sort orders whole faces, so two solids that
    //     pass THROUGH each other have no correct order and the seam tears whichever way you sort.
    //     A z-buffer would fix it and there isn't one. So it becomes an authoring rule — parts
    //     ABUT, never overlap — and a rule nobody checks is a rule that rots. It found the loom's
    //     warp board overlapping its own posts by 0.2 voxels, which is what shredded it on screen.
    //     Touching is fine (a mattress sits ON a frame): only a real overlap in ALL THREE axes counts.
    for (int c = 0; c < ISO_MODEL_COUNT; c++) {
        int clashes = 0;
        for (int i = 0; i < MESHES[c].n; i++) for (int j = i + 1; j < MESHES[c].n; j++) {
            const Part *p = &MESHES[c].p[i], *q = &MESHES[c].p[j];
            const float pxa = fminf(p->x0, p->x0 + p->dx0), pxb = fmaxf(p->x1, p->x1 + p->dx1);
            const float pya = fminf(p->y0, p->y0 + p->dy0), pyb = fmaxf(p->y1, p->y1 + p->dy1);
            const float qxa = fminf(q->x0, q->x0 + q->dx0), qxb = fmaxf(q->x1, q->x1 + q->dx1);
            const float qya = fminf(q->y0, q->y0 + q->dy0), qyb = fmaxf(q->y1, q->y1 + q->dy1);
            const float E = 0.05f;                       // touching is not overlapping
            const float ox = fminf(pxb, qxb) - fmaxf(pxa, qxa);
            const float oy = fminf(pyb, qyb) - fmaxf(pya, qya);
            const float oz = fminf(p->z1, q->z1) - fmaxf(p->z0, q->z0);
            if (ox > E && oy > E && oz > E) clashes++;
        }
        if (MESHES[c].n) expect(clashes == 0, str("%s parts abut, never interpenetrate", ISO_NAMES[c]));
    }

    // 2c. THE SAME RULE ONE LEVEL UP: no two PLACED items may share space either. The part-level
    //     gate above cannot see a wall standing in the same square as a toilet, and that is the
    //     form the bug actually took. Bounds come from each mesh, not from ISO_FOOTPRINT, so this
    //     measures what is DRAWN rather than what was declared.
    pr_yaw = 45.0f; pr_walls(1);
    int placed_clashes = 0;
    for (int i = 0; i < pr_item_n; i++) for (int j = i + 1; j < pr_item_n; j++) {
        float a[6], b[6];
        const Item *it[2] = { &pr_item[i], &pr_item[j] };
        float *box[2] = { a, b };
        for (int k = 0; k < 2; k++) {
            const Mesh *m = &MESHES[it[k]->cell];
            float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
            for (int p = 0; p < m->n; p++) {
                const Part *q = &m->p[p];
                const float xs[4] = { q->x0, q->x1, q->x0 + q->dx0, q->x1 + q->dx1 };
                const float ys[4] = { q->y0, q->y1, q->y0 + q->dy0, q->y1 + q->dy1 };
                for (int e = 0; e < 4; e++) {
                    const float X = xs[e] * it[k]->lx + it[k]->vx, Y = ys[e] * it[k]->ly + it[k]->vy;
                    if (X < lo[0]) lo[0] = X;  if (X > hi[0]) hi[0] = X;
                    if (Y < lo[1]) lo[1] = Y;  if (Y > hi[1]) hi[1] = Y;
                }
                const float Z0 = q->z0 + it[k]->vz, Z1 = q->z1 + it[k]->vz;
                if (Z0 < lo[2]) lo[2] = Z0;  if (Z1 > hi[2]) hi[2] = Z1;
            }
            for (int e = 0; e < 3; e++) { box[k][e] = lo[e]; box[k][e + 3] = hi[e]; }
        }
        const float E = 0.05f;
        int all = 1;
        for (int e = 0; e < 3; e++) if (fminf(a[e+3], b[e+3]) - fmaxf(a[e], b[e]) <= E) all = 0;
        if (all) placed_clashes++;
    }
    expect_eq(placed_clashes, 0, "no two placed items share space");

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

    // 5. THE SORT'S SOUNDNESS BOUND. A painter's sort gives one depth per polygon, so it is only
    //    sound while no polygon spans much depth. This asserts the subdivision actually holds that
    //    bound at every angle — the property that was silently violated when the walls were merged,
    //    and the reason a wall could be drawn over a toilet standing in front of it.
    float worst = 0.0f;
    for (int a = 0; a < 24; a++) {
        pr_yaw = (float)a * 15.0f;
        pr_walls(1); pr_build_tris();
        if (pr_max_span > worst) worst = pr_max_span;
    }
    expect(worst < (float)TILE * 2.05f,
           str("no quad spans more than 2 tiles of depth (worst %.1f voxels)", worst));

    pr_yaw = 45.0f;
}
