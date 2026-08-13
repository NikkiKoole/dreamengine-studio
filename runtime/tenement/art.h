// ─────────────────────────────────────────────────────────────────────────────
// tenement/art.h — the isometric view: projection, camera, painter's sort, contact shadows.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnr_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_ART_H
#define TENEMENT_ART_H

#include "tenement/atlas.h"   // the generated cell table. ONLY the art module includes this.

// which sprite each object kind draws with. ART ONLY — never branched on for behaviour.
static const int OBJ_CELL[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = ISO_BED, [TN_OBJ_FRIDGE] = ISO_FRIDGE, [TN_OBJ_COUNTER] = ISO_COUNTER,
    [TN_OBJ_TOILET] = ISO_TOILET, [TN_OBJ_SOFA] = ISO_SOFA, [TN_OBJ_LOOM] = ISO_LOOM,
    [TN_OBJ_WARDROBE] = ISO_WARDROBE,
};
// WHERE A BODY RESTS ON THIS OBJECT, in voxels — 0 means "the top of it", which is the old
// behaviour. The old code always lifted a non-standing resident to the object's full HEIGHT. That is
// right for a bed (you lie on the mattress, and the mattress IS the top) and wrong for everything you
// SIT on: a sofa is six voxels tall because of its backrest, so a sitter was placed on top of the
// backrest, and a toilet put one on top of the cistern. WHOLE voxels only — a fractional lift puts
// corners between lattice points and strays pixels everywhere (iso-rooms.md §7's measured table).
// ART ONLY, like OBJ_CELL: this says where a body looks right, never what the object does.
static const short ISO_REST_Z[ISO_MODEL_COUNT] = {
    [ISO_SOFA]   = 4,     // the seat cushion, not the top of the backrest
    [ISO_TOILET] = 3,     // the seat, not the top of the cistern
};
static float cam_x, cam_y;   // owner: art. The HUD does not need these.
static void tnr_iso_turn(int q, float x, float y, float *X, float *Y) {
    switch (q & 3) { case 0: *X= x; *Y= y; break; case 1: *X=-y; *Y= x; break;
                     case 2: *X=-x; *Y=-y; break; default: *X= y; *Y=-x; break; }
}
static void tnr_iso_project(int r, float vx, float vy, float vz, float *sx, float *sy) {
    float X, Y; tnr_iso_turn(r, vx, vy, &X, &Y);
    *sx = (X - Y) * (ISO_TW * 0.5f);
    *sy = (X + Y) * (ISO_TH * 0.5f) - vz * ISO_ZH;
}
static float tnr_iso_depth(int r, float vx, float vy) {
    float X, Y; tnr_iso_turn(r, vx, vy, &X, &Y); return X + Y;
}
void tn_camera(void) {
    float minx=1e9f, maxx=-1e9f, miny=1e9f, maxy=-1e9f;
    const float W = tn_bw * TN_TILE_VOX, H = tn_bh * TN_TILE_VOX;
    for (int c = 0; c < 8; c++) {
        float sx, sy;
        tnr_iso_project(tn_rot, (c&1)?W:0, (c&2)?H:0, (c&4)?12.0f:0, &sx, &sy);
        if (sx<minx) minx=sx; if (sx>maxx) maxx=sx; if (sy<miny) miny=sy; if (sy>maxy) maxy=sy;
    }
    // Floored: a half-pixel camera rounds adjacent sprites opposite ways and opens 1px seams
    // (iso-rooms.md §7). Learned there, not rediscovered here.
    cam_x = floorf((SCREEN_W - (maxx - minx)) * 0.5f - minx);
    cam_y = floorf((SCREEN_H - 26 - (maxy - miny)) * 0.5f - miny + 12);
}

// `hh` is the household whose colour a RESIDENT wears, or -1 for anything that is not a resident.
// Furniture's own `household` field is OWNERSHIP, not paint, so it is deliberately not put here.
// `myaw` turns the MESH in world space, in quarter turns — needed the moment a figure stopped being
// symmetric. The sprite path has no use for it (a baked cell already IS a rotation), so it is only
// read by the polygon renderer.
// `droop` is HOW BADLY IT IS GOING, 0..1, and it is a property of the instance rather than of the
// mesh — which is the whole reason it is a number here and not a fifth pose. Furniture and walls
// leave it 0 by omission (these initialisers are positional), so only residents can slump.
typedef struct { float depth; int cell, rot; float vx, vy, vz; int fp0, fp1; int shadow; int hh; int myaw;
                 float droop; } Draw;
// + 2*TN_N because every tile can carry a north AND a west edge wall.
static Draw tnr_dl[TN_MAX_OBJECTS + TN_MAX_AGENTS + 2 * TN_N];
static int tnr_dl_n;
static int tnr_cmp_draw(const void *a, const void *b) {
    const float d = ((const Draw*)a)->depth - ((const Draw*)b)->depth;
    return d < 0 ? -1 : (d > 0 ? 1 : 0);
}

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// THE POLYGON VIEW — the same building, as flat-shaded low-poly triangles.
//
// WHY. The maker's 2026-08-13 verdict was that this cart reads as an architectural diagram with a
// caption: in a game about people sharing a building the PEOPLE were the least visible thing on it,
// 1-tile blobs in the same colour family as the furniture. That was proved out in the `polyroom`
// probe rather than argued about here, and low-poly + real palette ramps won. This is that probe
// landed. polyroom is kept as the A/B bench and as the place the findings are written down.
//
// WHAT IS DELIBERATELY UNCHANGED, because it is what keeps 242 assertions and the player's verb
// working. The PROJECTION is untouched (tnr_iso_project), so build.h's tnb_unproject still turns a
// click into the right tile — that inverse is byte-identical to the forward transform and would
// break silently if the view moved. tn_rot stays 0..3, tn_camera() stays, and the DRAW LIST above
// (depth order, near-wall cutaway, the arithmetic that puts a sleeper on a mattress) is reused
// exactly as it is. Only the final step changes: a mesh per cell instead of a baked sprite.
//
// FOUR THINGS THE PROBE MEASURED, so nobody re-derives them here:
//   · Sort per FACE, never per triangle: two triangles of one quad cannot occlude each other, and
//     sorting them apart cut bright slivers across thin geometry.
//   · One NORMAL per face (Newell): prisms with unequal top offsets have non-planar sides, and a
//     per-triangle cross product puts a diagonal shading seam across them.
//   · SUBDIVIDE to about a tile. A painter's sort gives one depth per polygon, so a polygon
//     spanning a large depth range is unsortable by construction — a full-length wall face was
//     drawn over furniture standing in front of it. Vertical splitting matters as much as
//     horizontal, because height contributes to depth in this projection.
//   · Parts must ABUT, never interpenetrate: a painter's sort has no answer for two solids passing
//     through each other at any granularity. polyroom's spec() gates that rule on these meshes.
//
// AND ONE THING THAT IS NEW HERE, because tenement is 4x the scene: zsort() is an INSERTION sort,
// so at a couple of thousand triangles it is millions of comparisons a frame. This path qsorts,
// the same way the draw list above already does.
// ═══════════════════════════════════════════════════════════════════════════════════════════════
int tn_poly = 1;          // owner: art. The cart toggles it; 0 = the baked voxel sprites.

// ── FREE ORBIT AND TILT, which only the polygon view can have ───────────────────────────────────
// A baked sprite exists at four angles and nowhere between, so tn_rot stays an int 0..3 and keeps
// driving the sprite view, the cell lookup, and — the load-bearing one — build.h's click-to-tile
// picking, whose inverse transform only knows the four quarter turns. A mesh has no such limit, so
// the polygon view reads these two instead.
//
// AT tn_yaw = 45 + 90*tn_rot AND tn_tilt = 1 THE TWO PROJECTIONS ARE IDENTICAL. That is not a
// coincidence to be grateful for, it is the property that lets one draw list feed both views, and
// polyroom's spec() asserts it. The cart keeps them in step and snaps back to a quarter turn before
// build mode opens, because a click has to land on the tile you think you clicked.
float tn_yaw  = 45.0f;    // degrees; 45 + 90*rot reproduces the baked view exactly
float tn_tilt = 1.0f;     // ground squash; 1.0 is the classic 2:1 iso, lower is a flatter camera
static float tnr_pcx, tnr_pcy;                 // the polygon view's own camera offset

static void tnr_poly_project(float vx, float vy, float vz, float *sx, float *sy) {
    const float a = tn_yaw * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
    const float Xr = vx*c - vy*s, Yr = vx*s + vy*c;
    *sx = Xr * (ISO_TW * 0.5f) * 1.41421356f;
    *sy = Yr * (ISO_TH * 0.5f) * 1.41421356f * tn_tilt - vz * ISO_ZH;
}
// The view ray, DERIVED from that projection rather than hardcoded, so it stays correct when the
// tilt changes: a world offset leaves the screen position alone when Xr does not move and the ground
// and height terms cancel. Larger dot = NEARER, matching the draw list's convention.
static void tnr_viewdir(float *dx, float *dy, float *dz) {
    const float a = tn_yaw * 3.14159265f / 180.0f;
    *dx = sinf(a);  *dy = cosf(a);
    *dz = (ISO_TH * 0.5f) * 1.41421356f * tn_tilt / (float)ISO_ZH;
}
// SCREEN-RIGHT is (cos a, -sin a), NOT the world x axis: sx = (x cos a - y sin a) * k, so world +y
// moves screen-LEFT. Shading needs this and getting it wrong makes two faces tie.
static float tnr_screen_right(const float n[3]) {
    const float a = tn_yaw * 3.14159265f / 180.0f;
    return n[0] * cosf(a) - n[1] * sinf(a);
}
static void tnr_poly_camera(void) {
    float minx=1e9f, maxx=-1e9f, miny=1e9f, maxy=-1e9f;
    const float W = tn_bw * TN_TILE_VOX, H = tn_bh * TN_TILE_VOX;
    for (int c = 0; c < 8; c++) {
        float sx, sy;
        tnr_poly_project((c&1)?W:-2.0f, (c&2)?H:-2.0f, (c&4)?12.0f:0.0f, &sx, &sy);
        if (sx<minx) minx=sx; if (sx>maxx) maxx=sx; if (sy<miny) miny=sy; if (sy>maxy) maxy=sy;
    }
    // Floored for the same reason tn_camera is: a half-pixel camera rounds adjacent shapes opposite
    // ways and opens 1px seams (iso-rooms.md §7).
    tnr_pcx = floorf((SCREEN_W - (maxx - minx)) * 0.5f - minx);
    tnr_pcy = floorf((SCREEN_H - 26 - (maxy - miny)) * 0.5f - miny + 12);
}

enum { TNM_WOOD, TNM_UPH, TNM_CUSH, TNM_METAL, TNM_PORC, TNM_TRIM, TNM_WALL,
       TNM_SHIRT, TNM_TROUSER, TNM_SKIN, TNM_FLOOR_A, TNM_FLOOR_B, TNM_COUNT };

// Base-palette ramps, dark -> light. Used for whatever the hex budget below cannot reach, and for
// the floor (which is drawn as the literal two browns the sprite path uses, so the ground is the
// same pixels in both views and every difference you see is furniture and people).
static const unsigned char TNR_RAMP[TNM_COUNT][4] = {
    [TNM_WOOD]    = { CLR_BROWNISH_BLACK, CLR_DARK_BROWN,  CLR_BROWN,       CLR_MEDIUM_GREY },
    [TNM_UPH]     = { CLR_DARKER_BLUE,    CLR_TRUE_BLUE,   CLR_BLUE,        CLR_LIGHT_GREY  },
    [TNM_CUSH]    = { CLR_DARKER_PURPLE,  CLR_DARK_PURPLE, CLR_PINK,        CLR_PEACH       },
    [TNM_METAL]   = { CLR_DARKER_GREY,    CLR_DARK_GREY,   CLR_LIGHT_GREY,  CLR_WHITE       },
    [TNM_PORC]    = { CLR_DARK_GREY,      CLR_LIGHT_GREY,  CLR_WHITE,       CLR_WHITE       },
    [TNM_TRIM]    = { CLR_BLACK,          CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_DARK_GREY },
    [TNM_WALL]    = { CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_MAUVE,       CLR_MEDIUM_GREY },
    [TNM_SHIRT]   = { CLR_DARK_RED,       CLR_RED,         CLR_DARK_PEACH,  CLR_PEACH       },
    [TNM_TROUSER] = { CLR_BLACK,          CLR_DARKER_BLUE, CLR_DARK_BLUE,   CLR_TRUE_BLUE   },
    [TNM_SKIN]    = { CLR_DARK_PEACH,     CLR_PEACH,       CLR_LIGHT_PEACH, CLR_WHITE       },
    [TNM_FLOOR_A] = { CLR_BROWNISH_BLACK, CLR_DARK_BROWN,  CLR_BROWN,       CLR_MEDIUM_GREY },
    [TNM_FLOOR_B] = { CLR_BLACK,          CLR_BROWNISH_BLACK, CLR_DARK_BROWN, CLR_BROWN     },
};

// PER-HOUSEHOLD COLOUR, which is the punch-list item "so you can see whose flat someone is standing
// in without reading the HUD". THE BUDGET DECIDED WHERE THESE LIVE: the palette's free upper half is
// 32 slots, and tenement has EIGHT households, so eight hex ramps would eat 24 of them and leave
// almost nothing for materials. Households therefore take base-palette triples (a shirt is small,
// and pico32 has enough distinct hues) and the hex slots all go to materials, which cover the screen.
static const unsigned char TNR_HH[TN_MAX_HOUSEHOLDS][3] = {
    { CLR_DARK_RED,      CLR_RED,          CLR_DARK_PEACH  },   // 0 red
    { CLR_BLUE_GREEN,    CLR_MEDIUM_GREEN, CLR_GREEN       },   // 1 green
    { CLR_DARKER_PURPLE, CLR_DARK_PURPLE,  CLR_INDIGO      },   // 2 violet
    { CLR_DARK_BROWN,    CLR_BROWN,        CLR_ORANGE      },   // 3 amber
    { CLR_DARKER_BLUE,   CLR_TRUE_BLUE,    CLR_BLUE        },   // 4 blue
    { CLR_DARK_PURPLE,   CLR_PINK,         CLR_PEACH       },   // 5 pink
    { CLR_DARK_GREEN,    CLR_LIME_GREEN,   CLR_LIGHT_YELLOW},   // 6 lime
    { CLR_DARKER_GREY,   CLR_MEDIUM_GREY,  CLR_WHITE       },   // 7 bone
};

// ── the hex ramps: real shades out of the palette's unused upper half ────────────────────────────
// 7 material groups x 4 shades = 28 of the 32 free slots. Shadows bend COOL and highlights WARM
// rather than scaling toward black, because a flat multiply is what makes cheap 3D look like plastic.
#define TNR_SLOT0  32
#define TNR_MAT_SH 4
enum { TNG_WOOD, TNG_WALL, TNG_METAL, TNG_UPH, TNG_CUSH, TNG_PORC, TNG_SKIN, TNG_COUNT };
static const int TNG_BASE[TNG_COUNT] = {
    0xab5236, 0x754665, 0xc2c3c7, 0x29adff, 0xff77a8, 0xfff1e8, 0xffccaa,
};
static signed char TNG_OF[TNM_COUNT];

static int tnr_mix(int a, int b, float t) {
    const int ar=(a>>16)&255, ag=(a>>8)&255, ab=a&255, br=(b>>16)&255, bg=(b>>8)&255, bb=b&255;
    return ((int)(ar+(br-ar)*t)<<16) | ((int)(ag+(bg-ag)*t)<<8) | (int)(ab+(bb-ab)*t);
}
static void tnr_build_palette(void) {
    const int SHADOW = 0x1a1420, LIGHT = 0xfff1e8;
    for (int g = 0; g < TNG_COUNT; g++)
        for (int s = 0; s < TNR_MAT_SH; s++) {
            const float t = (float)s / (TNR_MAT_SH - 1);
            palette_hex(TNR_SLOT0 + g * TNR_MAT_SH + s,
                        (t < 0.5f) ? tnr_mix(TNG_BASE[g], SHADOW, (0.5f - t) * 1.5f)
                                   : tnr_mix(TNG_BASE[g], LIGHT,  (t - 0.5f) * 1.1f));
        }
    for (int m = 0; m < TNM_COUNT; m++) TNG_OF[m] = -1;
    TNG_OF[TNM_WOOD] = TNG_WOOD;  TNG_OF[TNM_WALL] = TNG_WALL;  TNG_OF[TNM_METAL] = TNG_METAL;
    TNG_OF[TNM_UPH]  = TNG_UPH;   TNG_OF[TNM_CUSH] = TNG_CUSH;  TNG_OF[TNM_PORC]  = TNG_PORC;
    TNG_OF[TNM_SKIN] = TNG_SKIN;
}

// ── the authoring primitive: a PRISM ────────────────────────────────────────────────────────────
// A bottom rectangle, a height, and four offsets moving the TOP rectangle's corners. Box, taper,
// shear and wedge in one thing — which is the whole reason this look is not the voxel look: a sofa
// back can rake, a torso can widen into shoulders, a bowl can flare. Units are VOXELS, the same
// units tools/voxel-models/tenement.js uses, so a mesh lands on the footprint of the sprite it
// replaces. polyroom's spec() asserts exactly that, plus the no-interpenetration rule.
typedef struct {
    float x0, y0, z0, x1, y1, z1;
    float dx0, dy0, dx1, dy1;
    unsigned char mat;
} TnPart;
#define TNR_BOX(x0,y0,z0,x1,y1,z1,m)           { x0,y0,z0,x1,y1,z1, 0,0,0,0, m }
#define TNR_PRISM(x0,y0,z0,x1,y1,z1,a,b,c,d,m) { x0,y0,z0,x1,y1,z1, a,b,c,d, m }

static const TnPart TNP_SOFA[] = {
    TNR_PRISM(0,0,0, 12,6,2.4f,  0.4f,0.4f,-0.4f,-0.4f, TNM_WOOD),
    TNR_PRISM(0,0,2.4f, 12,1.9f,6,  0,-0.9f,0,-0.9f,    TNM_UPH),      // back, RAKED
    TNR_BOX  (1.35f,1.95f,2.4f, 10.65f,6,3.9f,          TNM_UPH),
    TNR_PRISM(0,1.95f,2.4f, 1.3f,6,4.7f, 0.2f,0,-0.2f,0, TNM_UPH),     // arms
    TNR_PRISM(10.7f,1.95f,2.4f, 12,6,4.7f, 0.2f,0,-0.2f,0, TNM_UPH),
    TNR_BOX  (1.4f,2.05f,3.9f, 10.6f,5.4f,4.4f,         TNM_CUSH),
};
// Bed stays FOUR voxels tall — the voxel models file's rule, so its outline cannot be confused with
// the sofa's. The diagonals have to earn their keep inside that.
static const TnPart TNP_BED[] = {
    TNR_PRISM(0,0,0, 6,1,4.0f,  0,0.5f,0,0.5f,          TNM_WOOD),     // headboard, RAKED
    TNR_BOX  (0,1.6f,0, 6,12,1.8f,                      TNM_WOOD),
    TNR_BOX  (0.4f,1.8f,1.8f, 5.6f,11.6f,2.6f,          TNM_PORC),
    TNR_PRISM(0.9f,1.9f,2.6f, 5.1f,3.8f,3.3f, 0.3f,0.2f,-0.3f,-0.2f, TNM_PORC),   // pillow
    // Duvet reaches 4.0, the bed's DECLARED height, because a sleeper is placed there: stopping at
    // 3.7 left the figure floating a third of a voxel and the gap rasterized as a torn seam.
    TNR_PRISM(0.3f,4,2.6f, 5.7f,11.8f,4.0f, 0.3f,0,-0.3f,-0.4f, TNM_CUSH),
};
static const TnPart TNP_TOILET[] = {
    TNR_PRISM(1,0,0, 5,2,5.4f,  0.2f,0.1f,-0.2f,-0.1f,  TNM_PORC),     // cistern
    TNR_PRISM(1.6f,2.05f,0, 4.4f,5.2f,2.4f, -0.5f,0,0.5f,0.3f, TNM_PORC),  // bowl, FLARED forward
    TNR_PRISM(1,2.05f,2.4f, 5,5.5f,3.0f, 0.1f,0.1f,-0.1f,-0.1f, TNM_PORC),
    TNR_BOX  (1.4f,0.4f,5.4f, 4.6f,1.8f,5.8f,           TNM_PORC),
};
// Two stacked doors with a recessed dark band, not a seam plate: a thin plate on one face projects
// its top surface as a 1px diagonal (a scratch across the door) and only exists from one quarter of
// the orbit. The band goes all the way round.
static const TnPart TNP_FRIDGE[] = {
    TNR_BOX  (0,0,0, 6,6,5.4f,                          TNM_METAL),
    TNR_BOX  (0.2f,0.2f,5.4f, 5.8f,5.8f,5.8f,           TNM_TRIM),
    TNR_BOX  (0,0,5.8f, 6,6,11.6f,                      TNM_METAL),
    TNR_BOX  (4.4f,6.0f,6.2f, 5.3f,6.45f,9.8f,          TNM_TRIM),     // handle
    TNR_PRISM(0,0,11.6f, 6,6,12, 0.15f,0.15f,-0.15f,-0.15f, TNM_TRIM),
};
static const TnPart TNP_COUNTER[] = {
    TNR_PRISM(0.6f,0.8f,0, 5.4f,6,5.4f, -0.3f,-0.5f,0.3f,0, TNM_WOOD), // toe kick
    TNR_BOX  (0,0,5.4f, 6,6,6.2f,                       TNM_METAL),
    TNR_BOX  (0.5f,6.0f,2.4f, 5.5f,6.15f,2.7f,          TNM_TRIM),
};
// The loom must read as MACHINERY, not a second wardrobe (a punch-list item). Skeletal posts, a top
// beam, and a RAKED WARP PLANE — a leaning plane is the shape a voxel grid cannot make.
static const TnPart TNP_LOOM[] = {
    TNR_BOX  (0.2f,0.2f,0, 1.2f,1.2f,11,                TNM_WOOD),
    TNR_BOX  (4.8f,0.2f,0, 5.8f,1.2f,11,                TNM_WOOD),
    TNR_BOX  (0.2f,2.8f,0, 1.2f,3.8f,9,                 TNM_WOOD),
    TNR_BOX  (4.8f,2.8f,0, 5.8f,3.8f,9,                 TNM_WOOD),
    TNR_BOX  (1.3f,1.3f,2.4f, 4.7f,3.1f,3.1f,           TNM_WOOD),
    TNR_PRISM(1.3f,1.0f,3.2f, 4.7f,1.6f,10.4f, 0,2.0f,0,2.0f, TNM_CUSH),   // the warp, LEANING
    TNR_BOX  (0,0.4f,11.0f, 6,3.6f,11.9f,               TNM_WOOD),
};
static const TnPart TNP_WARDROBE[] = {
    TNR_PRISM(0.4f,0.4f,0, 5.6f,4.0f,9.4f, 0.1f,0.1f,-0.1f,-0.1f, TNM_WOOD),
    TNR_BOX  (2.8f,4.0f,0.6f, 3.2f,4.25f,9.2f,          TNM_TRIM),
    TNR_BOX  (0,0.2f,9.4f, 6,4.2f,10,                   TNM_WOOD),     // cornice
};
// THE FIGURE, and it is the whole point of this change. The voxel version needed literal arm voxels
// to get a shoulder line wider than the head (its own comment says the armless cut "read as a lamp").
// A prism gets it free: the torso is simply WIDER AT THE TOP. Four parts, 48 triangles, a person
// rather than a 12-voxel column.
static const TnPart TNP_PERSON[] = {
    TNR_PRISM(1.0f,0.9f,0, 2.2f,2.1f,5.2f,  0.15f,0,-0.15f,0, TNM_TROUSER),
    TNR_PRISM(2.8f,0.9f,0, 4.0f,2.1f,5.2f,  0.15f,0,-0.15f,0, TNM_TROUSER),
    TNR_PRISM(1.1f,0.7f,5.2f, 3.9f,2.3f,9.3f, -0.7f,-0.1f,0.7f,0.1f, TNM_SHIRT),  // SHOULDERS
    TNR_PRISM(1.6f,0.9f,9.3f, 3.4f,2.1f,11.6f, 0.25f,0.2f,-0.25f,-0.2f, TNM_SKIN),
};
// WORKING: a standing figure with both arms reaching forward along +y. design §4 wants labour to be
// VISIBLE, and arms are the only silhouette at this size that says busy. Straight from the shoulder
// tips — at this scale an elbow is noise, an outline is meaning.
static const TnPart TNP_PERSON_WORK[] = {
    TNR_PRISM(1.0f,0.9f,0, 2.2f,2.1f,5.2f,  0.15f,0,-0.15f,0, TNM_TROUSER),
    TNR_PRISM(2.8f,0.9f,0, 4.0f,2.1f,5.2f,  0.15f,0,-0.15f,0, TNM_TROUSER),
    TNR_PRISM(1.1f,0.7f,5.2f, 3.9f,2.3f,9.3f, -0.7f,-0.1f,0.7f,0.1f, TNM_SHIRT),   // torso
    TNR_BOX  (0.4f,2.45f,7.4f, 1.5f,4.0f,8.5f,           TNM_SHIRT),   // arms, reaching +y
    TNR_BOX  (3.5f,2.45f,7.4f, 4.6f,4.0f,8.5f,           TNM_SHIRT),
    TNR_PRISM(1.6f,0.9f,9.3f, 3.4f,2.1f,11.6f, 0.25f,0.2f,-0.25f,-0.2f, TNM_SKIN),
};

// HAULING: the same figure with a LOAD held proud of the chest. The box is what you see — it breaks
// the silhouette's vertical edge in +y, the way the sitting pose's thighs do — and the arms are only
// there to explain it. TNM_WOOD rather than TNM_SHIRT on purpose: the thing being carried must not
// take the household colour, or it reads as part of the body and a hauler just looks stout.
//
// This pose was unreachable art until the storage flip: work.h sold each good where it was made and
// deleted it in the same breath, so `carrying` was never non-negative and TN_ACT_HAUL was a state
// nothing could enter. Drawing it was never the blocker.
static const TnPart TNP_PERSON_HAUL[] = {
    TNR_PRISM(1.0f,0.9f,0, 2.2f,2.1f,5.2f,  0.15f,0,-0.15f,0, TNM_TROUSER),
    TNR_PRISM(2.8f,0.9f,0, 4.0f,2.1f,5.2f,  0.15f,0,-0.15f,0, TNM_TROUSER),
    TNR_PRISM(1.1f,0.7f,5.2f, 3.9f,2.3f,9.3f, -0.7f,-0.1f,0.7f,0.1f, TNM_SHIRT),  // torso
    TNR_BOX  (0.5f,2.3f,6.2f, 1.5f,3.4f,7.4f,            TNM_SHIRT),   // arms, cradling
    TNR_BOX  (3.5f,2.3f,6.2f, 4.5f,3.4f,7.4f,            TNM_SHIRT),
    TNR_PRISM(1.2f,2.4f,6.0f, 3.8f,4.3f,8.0f, 0.1f,0,-0.1f,0, TNM_WOOD),  // THE LOAD
    TNR_PRISM(1.6f,0.9f,9.3f, 3.4f,2.1f,11.6f, 0.25f,0.2f,-0.25f,-0.2f, TNM_SKIN),
};

// SITTING: thighs projecting forward, torso up, shoulders wider than the head. Eight voxels, and the
// forward projection is what carries the read — at this size a seated figure is recognised by its
// outline breaking the vertical, never by proportion.
static const TnPart TNP_PERSON_SIT[] = {
    TNR_BOX  (1.0f,4.0f,0, 4.0f,6.0f,1.9f,              TNM_TROUSER),   // thighs, forward
    TNR_PRISM(1.1f,2.2f,1.9f, 3.9f,3.9f,6.0f, -0.7f,-0.1f,0.7f,0.1f, TNM_SHIRT),  // torso -> shoulders
    TNR_PRISM(1.6f,2.4f,6.0f, 3.4f,3.7f,8.0f, 0.25f,0.2f,-0.25f,-0.2f, TNM_SKIN), // head
};
static const TnPart TNP_PERSON_LIE[] = {
    TNR_PRISM(0.5f,0,0.2f, 2.5f,1.3f,1.9f, 0.2f,0.15f,-0.2f,-0.15f, TNM_SKIN),
    TNR_PRISM(0.2f,1.4f,0, 2.8f,7.6f,1.6f, 0.2f,0,-0.2f,-0.4f, TNM_SHIRT),
};
static const TnPart TNP_WALL_FULL_NS[] = { TNR_BOX(0,0,0, 6,2,12, TNM_WALL) };
static const TnPart TNP_WALL_FULL_EW[] = { TNR_BOX(0,0,0, 2,6,12, TNM_WALL) };
static const TnPart TNP_WALL_LOW_NS[]  = { TNR_PRISM(0,0,0, 6,2,4, 0.2f,0.2f,-0.2f,-0.2f, TNM_WALL) };
static const TnPart TNP_WALL_LOW_EW[]  = { TNR_PRISM(0,0,0, 2,6,4, 0.2f,0.2f,-0.2f,-0.2f, TNM_WALL) };

// cell -> mesh, indexed by the SAME enum the baked atlas uses, so one draw-list entry addresses
// either renderer and the two views cannot drift apart on what a thing is.
typedef struct { const TnPart *p; int n; } TnMesh;
#define TNR_MESH(a) { a, (int)(sizeof(a)/sizeof((a)[0])) }
static const TnMesh TNR_MESHES[ISO_MODEL_COUNT] = {
    [ISO_SOFA]         = TNR_MESH(TNP_SOFA),         [ISO_BED]          = TNR_MESH(TNP_BED),
    [ISO_TOILET]       = TNR_MESH(TNP_TOILET),       [ISO_FRIDGE]       = TNR_MESH(TNP_FRIDGE),
    [ISO_COUNTER]      = TNR_MESH(TNP_COUNTER),      [ISO_LOOM]         = TNR_MESH(TNP_LOOM),
    [ISO_WARDROBE]     = TNR_MESH(TNP_WARDROBE),     [ISO_PERSON]       = TNR_MESH(TNP_PERSON),
    [ISO_PERSON_LIE]   = TNR_MESH(TNP_PERSON_LIE), [ISO_PERSON_SIT]   = TNR_MESH(TNP_PERSON_SIT),
    [ISO_PERSON_WORK]  = TNR_MESH(TNP_PERSON_WORK), [ISO_PERSON_HAUL] = TNR_MESH(TNP_PERSON_HAUL),
    [ISO_WALL_FULL_NS] = TNR_MESH(TNP_WALL_FULL_NS), [ISO_WALL_FULL_EW] = TNR_MESH(TNP_WALL_FULL_EW),
    [ISO_WALL_LOW_NS]  = TNR_MESH(TNP_WALL_LOW_NS),  [ISO_WALL_LOW_EW]  = TNR_MESH(TNP_WALL_LOW_EW),
};

// ── the triangle buffer ─────────────────────────────────────────────────────────────────────────
// `near` is larger for NEARER, matching the draw list's own convention above, and is sorted
// ASCENDING so far draws first. Derived from the projection rather than assumed: a world offset
// leaves the screen position unchanged when X==Y and (X+Y)*ISO_TH/2 == z*ISO_ZH, which for this
// projection makes the view ray (1,1,1) in turned coords — so nearness is X + Y + z.
// ── A DEPTH BUFFER, and why this path has NO SORT AT ALL ────────────────────────────────────────
// Three separate defects here turned out to be one thing: two solids sharing space, which a
// painter's sort cannot resolve at ANY granularity. Wall corners strobed because a north wall and a
// west wall share a column. Furniture punches through walls because a wall straddles its edge and
// intrudes a voxel into a tile the furniture fills. Both were "fixed" by shuffling geometry, and the
// second fix made the first worse. A per-pixel depth test ends the whole class instead.
//
// It also DELETES constraints rather than adding them: no sort, so no stability trap (qsort is not
// stable, and swapping the stable zsort for it is what made the corners strobe in the first place);
// no need for parts to abut; and subdivision is no longer load-bearing for correctness. Measured in
// the `polyroom` probe at slightly FASTER than sorting, because the sort draws every triangle in
// full while the depth test only emits the runs that survive.
//
// Depth interpolates EXACTLY in screen space here rather than approximately: the projection is
// orthographic, so there is no perspective divide and depth is affine in screen x/y. A perspective
// renderer would have to interpolate 1/z instead.
//
// `zv` is per-VERTEX nearness, LARGER = NEARER (the draw list's own convention above).
typedef struct { int x[3], y[3]; float zv[3]; unsigned char col; } TnTri;
#define TNR_MAX_TRIS 8192
static TnTri tnr_tri[TNR_MAX_TRIS];
static int   tnr_tri_n;
static float tnr_zbuf[SCREEN_W * SCREEN_H];
#define TNR_ZNEAR (-1e18f)
// ── HOW BADLY IT IS GOING, as one number ────────────────────────────────────────────────────────
// The WORST need, not the average, because that is the one a person would tell you about: somebody
// fed, rested and entertained who has not washed in two days is a filthy person, and averaging five
// needs would report them as fine. Same quantity the HUD's bottom band already prints a word for.
//
// 96 IS hud.h's OWN THRESHOLD (`wv < 96` is where it starts printing "bursting" instead of a bar),
// and the two must move together or the picture and the caption disagree about who is in trouble —
// which is the exact failure this item exists to fix. It is duplicated rather than shared because a
// module may not include a sibling (model.h's rules) and art.h is included BEFORE hud.h anyway.
//
// ART READS A SIM FACT AND DECIDES NOTHING. Nobody's behaviour changes because they look tired.
#define TNR_DISTRESS_AT 96
static float tnr_distress(const TnAgent *a) {
    int worst = 255;
    for (int n = 0; n < TN_NEED_COUNT; n++) if (a->need[n] < worst) worst = a->need[n];
    if (worst >= TNR_DISTRESS_AT) return 0.0f;                 // fine, and fine looks like nothing
    return (float)(TNR_DISTRESS_AT - worst) / (float)TNR_DISTRESS_AT;
}

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// THE EVENTS, IN THE PICTURE — punch-list item 3.
//
// The moment someone fails to find a thing, or wants the WC that is taken, or gives up and walks
// away, IS the comedy of this sim, and all of it used to scroll past as text in a band at the top.
// It wants to happen above their head, where you are already looking.
//
// THREE RULES THIS FOLLOWS, and they are what keep it from becoming noise:
//   · ONLY THE NOTABLE. A resident doing its job successfully gets nothing. Every glyph here is a
//     thing GOING WRONG or a thing being carried, so a quiet building is a blank one and a busy
//     one is legible at a glance.
//   · TRANSIENT, AND ON THE EDGE. The timer restarts when the event CHANGES, not while it holds,
//     so a resident queueing for twenty minutes flashes once rather than strobing "!" all morning.
//   · NO NEW SIM STATE. Every kind below is derived from what the agent already is, so nothing in
//     the contract moves and the glyph cannot disagree with the simulation. This state is ART's,
//     it lives here, and deleting the whole block changes no behaviour.
//
// The vocabulary is deliberately tiny and shape-based rather than lettered: at 320x200 a word is
// four times the size of the person it belongs to. fxicons.h is the precedent — one glyph per kind,
// shared, so the same thing always looks the same.
enum { TNR_EV_NONE = 0, TNR_EV_BLOCKED, TNR_EV_NOWHERE, TNR_EV_HAUL };
#define TNR_BUB_FRAMES 48
// The head point is kept in WORLD space and projected at draw time, not here: the polygon camera
// (tnr_pcx/pcy) is recomputed inside tnr_draw_poly, which runs after this list is built, so
// projecting early would peg every chip to the PREVIOUS frame's camera — invisible while the view
// is still, and a chip sliding off its owner the moment you orbit.
static struct { unsigned char kind, last; short left; float wx, wy, wz; } tnr_bub[TN_MAX_AGENTS];

// WHAT IS HAPPENING TO THIS RESIDENT, in the order that matters when two are true at once.
// Reading `activity` and `bid_tag` only: art asks the sim what it decided, it never decides.
static int tnr_event_of(const TnAgent *a) {
    // Carrying beats everything: it is the only one that is not a complaint, and store.h gives
    // hauling the last word over wanting for the same reason (you put your load down first).
    if (a->activity == TN_ACT_HAUL || a->carrying >= 0) return TNR_EV_HAUL;
    // IDLE while still pointed at something is agents.h's own "arrived, and it was full" state —
    // its comment calls keeping the target the thing that turns a priced wait into a visible queue.
    // This is that queue, made visible.
    if (a->activity == TN_ACT_IDLE && a->target_obj >= 0) return TNR_EV_BLOCKED;
    // tn_best_action found NOTHING worth getting up for: either the building cannot serve what this
    // resident needs, or (with R on) everything on offer fell under the boredom floor.
    if (a->bid_tag >= TN_SERVE_COUNT && a->target_obj < 0 && a->activity != TN_ACT_USE)
        return TNR_EV_NOWHERE;
    return TNR_EV_NONE;
}

// A 7x7 chip: a dark plate so the mark survives any floor colour, then the mark. Shapes rather than
// letters — a "!" is legible at 5px, a word is not, and a shape needs no language.
static void tnr_glyph(int x, int y, int kind) {
    if (kind == TNR_EV_NONE) return;
    rectfill(x, y, 7, 7, CLR_BROWNISH_BLACK);
    pset(x,     y,     CLR_DARK_BLUE);  pset(x + 6, y,     CLR_DARK_BLUE);   // nipped corners
    pset(x,     y + 6, CLR_DARK_BLUE);  pset(x + 6, y + 6, CLR_DARK_BLUE);
    pset(x + 3, y + 8, CLR_BROWNISH_BLACK);                                   // a tail, so it points
    pset(x + 3, y + 7, CLR_BROWNISH_BLACK);
    switch (kind) {
    case TNR_EV_BLOCKED:                        // "!"  — I want that and I cannot have it
        rectfill(x + 3, y + 1, 1, 3, CLR_RED);
        pset(x + 3, y + 5, CLR_RED);
        break;
    case TNR_EV_NOWHERE:                        // "…"  — nothing on offer, at a loss
        pset(x + 1, y + 4, CLR_LIGHT_GREY);
        pset(x + 3, y + 4, CLR_LIGHT_GREY);
        pset(x + 5, y + 4, CLR_LIGHT_GREY);
        break;
    case TNR_EV_HAUL:                           // a crate — carrying something somewhere
        rectfill(x + 1, y + 2, 5, 4, CLR_BROWN);
        rectfill(x + 1, y + 3, 5, 1, CLR_DARK_BROWN);
        break;
    default: break;
    }
}

static float tnr_near3(float vx, float vy, float vz) {
    float dx, dy, dz; tnr_viewdir(&dx, &dy, &dz);
    return vx*dx + vy*dy + vz*dz;
}

// Brightness -> a palette index. Screen-space rule rather than a dot product against a light, and
// that is a fix not a style: any single light direction TIES two perpendicular faces at whichever
// rotation puts them symmetric about it, and then the corner between them vanishes (a fridge came
// out as one flat pane). The voxel bake never has this because it assigns [top, screen-right,
// screen-left] by DIRECTION. This is that rule made continuous, so rakes and tapers still get
// in-between values, and it cannot tie: the view direction bisects the two visible side faces, so
// their screen-right-ness is always opposite in sign.
// `droop` (0..1) drains the SKIN and only the skin. The household colour is left alone on purpose:
// it is the one thing on screen that says whose flat this is, and a resident who goes grey when
// hungry would be trading the identity read for the mood read when we want both. Skin is the right
// carrier because it is the only material a person has that furniture does not, so nothing else in
// the picture can be mistaken for distress.
static int tnr_shade(const float n[3], int mat, int hh, float droop) {
    const float right = tnr_screen_right(n);
    const float up    = n[2] > 0.0f ? n[2] : 0.0f;
    float br = 0.20f + 0.38f * up + 0.40f * (0.5f + 0.5f * right);
    if (mat == TNM_SKIN) br -= 0.26f * droop;         // hollow rather than lit
    if (br > 1.0f) br = 1.0f;  if (br < 0.0f) br = 0.0f;

    if (mat >= TNM_FLOOR_A) return TNR_RAMP[mat][2];   // floor: flat, and the sprite view's colour
    if (mat == TNM_SHIRT && hh >= 0 && hh < TN_MAX_HOUSEHOLDS)
        return TNR_HH[hh][br > 0.72f ? 2 : (br > 0.50f ? 1 : 0)];
    if (TNG_OF[mat] >= 0) {
        int s = (int)(br * (TNR_MAT_SH - 0.001f));
        if (s > TNR_MAT_SH - 1) s = TNR_MAT_SH - 1;  if (s < 0) s = 0;
        return TNR_SLOT0 + TNG_OF[mat] * TNR_MAT_SH + s;
    }
    int s = (int)(br * 3.999f);  if (s > 3) s = 3;  if (s < 0) s = 0;
    return TNR_RAMP[mat][s];
}

static void tnr_push(const float a[3], const float b[3], const float c[3], int col) {
    if (tnr_tri_n >= TNR_MAX_TRIS) return;
    TnTri *t = &tnr_tri[tnr_tri_n];
    const float *v[3] = { a, b, c };
    for (int i = 0; i < 3; i++) {
        float sx, sy; tnr_poly_project(v[i][0], v[i][1], v[i][2], &sx, &sy);
        t->x[i] = (int)(sx + tnr_pcx);  t->y[i] = (int)(sy + tnr_pcy);
        t->zv[i] = tnr_near3(v[i][0], v[i][1], v[i][2]);
    }
    t->col = (unsigned char)col;
    tnr_tri_n++;
}

// One face: Newell normal (correct for the non-planar sides a taper produces), backface cull, one
// shade for the whole face, then subdivided to about a tile so the sort stays sound.
static void tnr_face(const float a[3], const float b[3], const float c[3], const float d[3],
                     int mat, int hh, float droop) {
    const float *p[4] = { a, b, c, d };
    float n[3] = { 0, 0, 0 };
    for (int i = 0; i < 4; i++) {
        const float *u = p[i], *w = p[(i + 1) & 3];
        n[0] += (u[1]-w[1]) * (u[2]+w[2]);
        n[1] += (u[2]-w[2]) * (u[0]+w[0]);
        n[2] += (u[0]-w[0]) * (u[1]+w[1]);
    }
    const float len = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    if (len < 1e-6f) return;
    n[0] /= len; n[1] /= len; n[2] /= len;
    // A face is visible when its normal points along the view ray. Same derivation as tnr_near3, so
    // it stays correct at any yaw and tilt rather than only at the four baked angles.
    float dx, dy, dz; tnr_viewdir(&dx, &dy, &dz);
    if (n[0]*dx + n[1]*dy + n[2]*dz <= 0.0f) return;   // backface
    const int col = tnr_shade(n, mat, hh, droop);

    // NO SUBDIVISION, and this is the depth buffer PAYING FOR ITSELF. Splitting every polygon down
    // to about a tile was never wanted for its own sake — it existed because a painter's sort gives
    // one depth per polygon, so a polygon spanning a large depth range is unsortable and a
    // full-length wall face got drawn over furniture in front of it. A per-pixel test has no such
    // limit, so the split is pure waste: it cost ~40% of the triangles and every one of them a
    // rectfill run. Deleting it is what makes the depth test cheaper than the sort it replaced.
    tnr_push(a, b, c, col);
    tnr_push(a, c, d, col);
}

// A prism's six faces, wound counter-clockwise seen from OUTSIDE so the normal points out.
// A quarter turn of the model about its own footprint, applied BEFORE placing it. The linear part is
// (x,y) -> (-y,x) and the footprint term is only there to keep coordinates positive; determinant is
// +1, so face winding — and therefore every outward normal — survives untouched.
static void tnr_spin(int q, float fx, float fy, float x, float y, float *ox, float *oy) {
    switch (q & 3) {
        case 1:  *ox = fy - y;  *oy = x;       break;
        case 2:  *ox = fx - x;  *oy = fy - y;  break;
        case 3:  *ox = y;       *oy = fx - x;  break;
        default: *ox = x;       *oy = y;       break;
    }
}
// ── THE SLUMP, and it is ONE GENERIC RULE rather than a mesh per mood ────────────────────────────
// The punch list asked for posture to carry the WORST NEED, because the HUD prints "filthy" and
// "bursting" in a text band while the figure stands there unbothered. The cheap answer would be a
// fifth and sixth person mesh, and it does not scale: five needs times three poses is fifteen
// drawings, and none of them can show a resident who is halfway gone.
//
// So distress is a SCALAR applied to whatever mesh is already there, and the trick is that the prism
// primitive makes three separate reads fall out of two lines of arithmetic:
//   · SQUASH z          — the figure gets shorter. Height is the loudest thing in a 24px silhouette.
//   · SHRINK the top offsets — and this is the one that does the work. The healthy figure is
//     recognised by SHOULDERS WIDER THAN THE HEAD (this file's own note: without that contrast it
//     "read as a lamp"), and that contrast IS the shirt prism's outward taper. Scaling every dx/dy
//     toward zero un-flares it, so a distressed resident collapses toward the lamp it started as.
//     Exactly the inverse of the healthy read, for free, with no per-part special case.
//   · SHEAR along +y     — a forward hunch. Proportional to each part's own height above the floor,
//     so the feet stay planted and the head travels furthest.
// It applies to any mesh, so it needs no knowledge of which pose it is decorating, and a sitter
// slumps in its chair by the same code that makes a stander stoop.
static void tnr_part(const TnPart *p, float ox, float oy, float oz, int hh,
                     int q, float fx, float fy, float droop) {
    TnPart s = *p;
    if (droop > 0.0f) {
        // MEASURED, not guessed. The first cut of these was ~half as strong and changed 15 pixels
        // across an 1800-frame run, because most of a resident is behind furniture and only the head
        // and shoulders are ever visible — so whatever carries the mood has to carry it UP THERE.
        // Sized against the traced distribution of the worst need (median 96, p25 44, 23% under 32),
        // so the common case of "somewhat badly" is already a couple of pixels rather than nothing.
        const float squash = 1.0f - 0.24f * droop;      // shorter
        const float flare  = 1.0f - 0.85f * droop;      // shoulders in toward the head
        const float lean   = 0.11f * droop;             // voxels of hunch per voxel of height
        s.z0 = p->z0 * squash;              s.z1 = p->z1 * squash;
        s.dx0 = p->dx0 * flare;             s.dx1 = p->dx1 * flare;
        s.dy0 = p->dy0 * flare;             s.dy1 = p->dy1 * flare;
        // The shear moves the BASE by its own height and the TOP by the part's full height, which is
        // what makes the stack lean as one body instead of each part shearing on its own spot.
        s.y0 += lean * s.z0;                s.y1 += lean * s.z0;
        s.dy0 += lean * (s.z1 - s.z0);      s.dy1 += lean * (s.z1 - s.z0);
        p = &s;
    }
    float rb0x,rb0y, rb1x,rb1y, rb2x,rb2y, rb3x,rb3y;
    float ru0x,ru0y, ru1x,ru1y, ru2x,ru2y, ru3x,ru3y;
    tnr_spin(q, fx, fy, p->x0, p->y0, &rb0x, &rb0y);
    tnr_spin(q, fx, fy, p->x1, p->y0, &rb1x, &rb1y);
    tnr_spin(q, fx, fy, p->x1, p->y1, &rb2x, &rb2y);
    tnr_spin(q, fx, fy, p->x0, p->y1, &rb3x, &rb3y);
    tnr_spin(q, fx, fy, p->x0 + p->dx0, p->y0 + p->dy0, &ru0x, &ru0y);
    tnr_spin(q, fx, fy, p->x1 + p->dx1, p->y0 + p->dy0, &ru1x, &ru1y);
    tnr_spin(q, fx, fy, p->x1 + p->dx1, p->y1 + p->dy1, &ru2x, &ru2y);
    tnr_spin(q, fx, fy, p->x0 + p->dx0, p->y1 + p->dy1, &ru3x, &ru3y);
    const float z0=p->z0+oz, z1=p->z1+oz;
    const float b0[3]={rb0x+ox,rb0y+oy,z0}, b1[3]={rb1x+ox,rb1y+oy,z0};
    const float b2[3]={rb2x+ox,rb2y+oy,z0}, b3[3]={rb3x+ox,rb3y+oy,z0};
    const float u0[3]={ru0x+ox,ru0y+oy,z1}, u1[3]={ru1x+ox,ru1y+oy,z1};
    const float u2[3]={ru2x+ox,ru2y+oy,z1}, u3[3]={ru3x+ox,ru3y+oy,z1};
    const int m = p->mat;
    tnr_face(u0,u1,u2,u3, m,hh,droop);  tnr_face(b0,b3,b2,b1, m,hh,droop);   // top, bottom
    tnr_face(b0,b1,u1,u0, m,hh,droop);  tnr_face(b2,b3,u3,u2, m,hh,droop);   // front, back
    tnr_face(b3,b0,u0,u3, m,hh,droop);  tnr_face(b1,b2,u2,u1, m,hh,droop);   // left, right
}

// Flat-shaded triangle with a depth test, edge functions stepped incrementally. Runs of surviving
// pixels are emitted as ONE rectfill, because a flat triangle's passing pixels are contiguous until
// something occludes them — that keeps the call count near the SCANLINE count rather than the pixel
// count, which is the difference between this being usable and being a slideshow.
static void tnr_raster(const TnTri *t) {
    int x0=t->x[0], y0=t->y[0], x1=t->x[1], y1=t->y[1], x2=t->x[2], y2=t->y[2];
    float z0=t->zv[0], z1=t->zv[1], z2=t->zv[2];
    float area = (float)(x1-x0)*(y2-y0) - (float)(x2-x0)*(y1-y0);
    if (area == 0.0f) return;
    if (area < 0.0f) {                                  // one winding, so "inside" is one test
        int tx=x1; x1=x2; x2=tx;  int ty=y1; y1=y2; y2=ty;
        float tz=z1; z1=z2; z2=tz;  area = -area;
    }
    int lox = x0<x1 ? (x0<x2?x0:x2) : (x1<x2?x1:x2), hix = x0>x1 ? (x0>x2?x0:x2) : (x1>x2?x1:x2);
    int loy = y0<y1 ? (y0<y2?y0:y2) : (y1<y2?y1:y2), hiy = y0>y1 ? (y0>y2?y0:y2) : (y1>y2?y1:y2);
    if (lox < 0) lox = 0;  if (hix > SCREEN_W-1) hix = SCREEN_W-1;
    if (loy < 0) loy = 0;  if (hiy > SCREEN_H-1) hiy = SCREEN_H-1;
    if (lox > hix || loy > hiy) return;

    const float inv = 1.0f / area;
    const float a0 = -(float)(y2-y1), b0 = (float)(x2-x1);
    const float a1 = -(float)(y0-y2), b1 = (float)(x0-x2);
    const float a2 = -(float)(y1-y0), b2 = (float)(x1-x0);
    const int col = t->col;
    for (int y = loy; y <= hiy; y++) {
        const float py = (float)y + 0.5f, px0 = (float)lox + 0.5f;
        float w0 = a0*(px0-(float)x1) + b0*(py-(float)y1);
        float w1 = a1*(px0-(float)x2) + b1*(py-(float)y2);
        float w2 = a2*(px0-(float)x0) + b2*(py-(float)y0);
        float *row = &tnr_zbuf[y * SCREEN_W];
        int run_x = 0, run_n = 0;
        for (int x = lox; x <= hix; x++, w0 += a0, w1 += a1, w2 += a2) {
            int keep = 0;
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                const float z = (w0*z0 + w1*z1 + w2*z2) * inv;
                if (z > row[x]) { row[x] = z; keep = 1; }   // larger = nearer
            }
            if (keep) { if (!run_n) run_x = x;  run_n++; }
            else if (run_n) { rectfill(run_x, y, run_n, 1, col); run_n = 0; }
        }
        if (run_n) rectfill(run_x, y, run_n, 1, col);
    }
}

// Which household a draw-list entry belongs to, for the shirt colour. Only residents carry one; the
// furniture's own `household` is ownership, not paint, so it is deliberately NOT read here.
static void tnr_draw_poly(void) {
    tnr_tri_n = 0;
    tnr_poly_camera();
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) tnr_zbuf[i] = TNR_ZNEAR;
    // Floor. The outer tiles run 2 voxels UNDER the walls: the floor's edge and the wall's base are
    // the same line in space, and two polygons meeting exactly on a line leave the boundary pixel
    // unowned under pixel-centre coverage — which showed as a dashed dark seam all round the room.
    for (int ty = 0; ty < tn_bh; ty++) for (int tx = 0; tx < tn_bw; tx++) {
        float x0 = (float)(tx*TN_TILE_VOX), y0 = (float)(ty*TN_TILE_VOX);
        float x1 = x0 + TN_TILE_VOX, y1 = y0 + TN_TILE_VOX;
        if (tx == 0)          x0 -= 2.0f;
        if (ty == 0)          y0 -= 2.0f;
        if (tx == tn_bw - 1)  x1 += 2.0f;
        if (ty == tn_bh - 1)  y1 += 2.0f;
        const float a[3]={x0,y0,0}, b[3]={x1,y0,0}, c[3]={x1,y1,0}, d[3]={x0,y1,0};
        tnr_face(a, b, c, d, ((tx+ty)&1) ? TNM_FLOOR_A : TNM_FLOOR_B, -1, 0.0f);
    }
    for (int i = 0; i < tnr_dl_n; i++) {
        const int cell = tnr_dl[i].cell;
        const TnMesh *m = &TNR_MESHES[cell];
        // WALLS KEEP THE DRAW LIST'S OWN STRADDLED PLACEMENT, centred on their edge, the same as the
        // sprite view. An earlier pass nudged them a voxel off the boundary to stop corners
        // interpenetrating, and that traded one overlap for a worse one: a wall intrudes into the
        // adjacent tile, furniture FILLS its tile, so doubling the intrusion made furniture visibly
        // punch through walls. The depth test resolves both without moving anything, which is the
        // whole reason it is here.
        const short *mfp = ISO_FOOTPRINT[cell];
        for (int p = 0; p < m->n; p++)
            tnr_part(&m->p[p], tnr_dl[i].vx, tnr_dl[i].vy, tnr_dl[i].vz, tnr_dl[i].hh,
                     tnr_dl[i].myaw, (float)mfp[0], (float)mfp[1], tnr_dl[i].droop);
    }
    // NO SORT. Not a faster sort, not a stabler one — none. Draw order stops mattering entirely,
    // which is the whole argument for a depth buffer in one line of code.
    for (int i = 0; i < tnr_tri_n; i++) tnr_raster(&tnr_tri[i]);
}

// The chips, LAST and on top of everything. Deliberately not depth-tested: a chip is an annotation
// about a person, not a thing in the room, and one hidden behind the wall its owner is standing
// behind would report nothing at exactly the moment worth reporting. Runs after the render so the
// camera it projects through is this frame's.
static void tnr_draw_bubbles(void) {
    for (int i = 0; i < tn_agent_n && i < TN_MAX_AGENTS; i++) {
        if (tnr_bub[i].left <= 0) continue;
        float px, py;
        if (tn_poly) { tnr_poly_project(tnr_bub[i].wx, tnr_bub[i].wy, tnr_bub[i].wz, &px, &py);
                       px += tnr_pcx; py += tnr_pcy; }
        else         { tnr_iso_project(tn_rot, tnr_bub[i].wx, tnr_bub[i].wy, tnr_bub[i].wz, &px, &py);
                       px += cam_x;   py += cam_y; }
        const int x = (int)px - 3, y = (int)py - 7;            // 7 wide, sitting above the head
        // The HUD owns the top 18 rows and the bottom band from 174 (hud.h's own note). A chip that
        // strays into either is a ui-audit overlap, so it is simply not drawn there — losing one
        // glyph at the edge of the room beats drawing over the numbers.
        if (x < 0 || x > SCREEN_W - 7 || y < 19 || y > 165) continue;
        tnr_glyph(x, y, tnr_bub[i].kind);
    }
}

void tn_draw_world(void) {
    static int pal_done = 0;
    if (!pal_done) { tnr_build_palette(); pal_done = 1; }   // lazy: the frozen contract gains no entry
    if (!tn_poly)
    for (int ty = 0; ty < tn_bh; ty++) for (int tx = 0; tx < tn_bw; tx++) {
        float c[4][2]; const float vx = tx*TN_TILE_VOX, vy = ty*TN_TILE_VOX;
        tnr_iso_project(tn_rot, vx,             vy,             0, &c[0][0], &c[0][1]);
        tnr_iso_project(tn_rot, vx+TN_TILE_VOX, vy,             0, &c[1][0], &c[1][1]);
        tnr_iso_project(tn_rot, vx+TN_TILE_VOX, vy+TN_TILE_VOX, 0, &c[2][0], &c[2][1]);
        tnr_iso_project(tn_rot, vx,             vy+TN_TILE_VOX, 0, &c[3][0], &c[3][1]);
        quadfill((int)(c[0][0]+cam_x),(int)(c[0][1]+cam_y), (int)(c[1][0]+cam_x),(int)(c[1][1]+cam_y),
                 (int)(c[2][0]+cam_x),(int)(c[2][1]+cam_y), (int)(c[3][0]+cam_x),(int)(c[3][1]+cam_y),
                 ((tx+ty)&1) ? CLR_BROWN : CLR_DARK_BROWN);
    }
    tnr_dl_n = 0;
    for (int o = 0; o < tn_obj_n; o++) {
        const int cell = OBJ_CELL[tn_obj[o].kind];
        const short *fp = ISO_FOOTPRINT[cell];
        tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, tn_obj[o].tx*TN_TILE_VOX + fp[0]*0.5f,
                                               tn_obj[o].ty*TN_TILE_VOX + fp[1]*0.5f),
                             cell, tn_rot, (float)tn_obj[o].tx*TN_TILE_VOX,
                             (float)tn_obj[o].ty*TN_TILE_VOX, 0.0f, fp[0], fp[1], 1, -1, 0 };
    }
    for (int i = 0; i < tn_agent_n; i++) {
        // The cell comes from the agent's POSE, which came from the offer it is using. No branch on
        // object kind anywhere: a bed does not tell the renderer it is a bed, it tells the sim that
        // using it means lying down (contract rule 2). Standing on a mattress was the symptom of
        // there being no notion of posture at all.
        // THE POSE PICKS THE CELL. SIT used to fall through to the standing figure, so a resident on
        // the sofa or the toilet was drawn upright — the sim had said SIT since offer.h was written
        // and only the art was missing.
        //
        // WORKING is read from the winning bid's TAG, not from the activity: nothing in the cart ever
        // sets TN_ACT_WORK, so `activity` cannot tell a shift at the loom from a snack at the fridge.
        // `bid_tag` is the sim's own answer to what this resident is doing, and TN_CAP_WORK covers the
        // loom and the counter both. Reading it here is ART reading a decision, never making one.
        const int working = (tn_agent[i].pose == TN_POSE_STAND &&
                             tn_agent[i].activity == TN_ACT_USE &&
                             tn_agent[i].bid_tag == TN_CAP_WORK);
        // CARRYING is read from the sim's own answer — `carrying` is a live item index, so the pose
        // needs no new state, which is what the punch list said when it asked for this. It ranks
        // below the seated and lying poses (you put a thing down before you sit on the sofa, and
        // store.h enforces exactly that by refusing to interrupt a USE) and above the standing one.
        const int hauling = (tn_agent[i].pose == TN_POSE_STAND && tn_agent[i].carrying >= 0);
        const int pcell = (tn_agent[i].pose == TN_POSE_LIE) ? ISO_PERSON_LIE
                        : (tn_agent[i].pose == TN_POSE_SIT) ? ISO_PERSON_SIT
                        : hauling                           ? ISO_PERSON_HAUL
                        : working                           ? ISO_PERSON_WORK : ISO_PERSON;
        const short *fp = ISO_FOOTPRINT[pcell];
        // A resident who is not standing is ON the thing it is using, not on the floor beside it.
        // Position comes from the OBJECT and height from that object's own voxel depth (ISO_FOOTPRINT
        // carries nx,ny,NZ), so a taller bed lifts the sleeper without anyone hardcoding a number.
        // Lying beside the bed was the second half of the standing-on-the-bed bug: posture without
        // placement just moves the wrongness.
        float ax = (float)tn_agent[i].tx * TN_TILE_VOX, ay = (float)tn_agent[i].ty * TN_TILE_VOX;
        float az = 0.0f;
        int   arot = (tn_rot + tn_agent[i].facing) & 3;
        // POINT THE ARMS AT THE WORK, derived from where the object IS rather than from `facing`
        // (which is only the last step of the walk — arrive from the north and you face south whether
        // or not the loom is). The mesh is authored with its arms along +y, and the quarter turns map
        // +y to: 0 south, 1 west, 2 north, 3 east. Everything else is unturned: a sitter is already
        // aligned with its furniture below, and the standing figure is symmetric enough not to care.
        int   amyaw = 0;
        if (working && tn_agent[i].target_obj >= 0) {
            const TnObject *wo = &tn_obj[tn_agent[i].target_obj];
            const int ddx = wo->tx - tn_agent[i].tx, ddy = wo->ty - tn_agent[i].ty;
            if (abs(ddx) > abs(ddy)) amyaw = (ddx > 0) ? 3 : 1;     // east : west
            else                     amyaw = (ddy > 0) ? 0 : 2;     // south : north
        }
        if (tn_agent[i].pose != TN_POSE_STAND && tn_agent[i].target_obj >= 0) {
            const TnObject *ob = &tn_obj[tn_agent[i].target_obj];
            const short *ofp = ISO_FOOTPRINT[OBJ_CELL[ob->kind]];
            // CENTRED on the furniture, not parked at its corner: a bed is 6x12 voxels and a lying
            // figure is 3x8, so using the object's origin puts the sleeper half off the mattress.
            // WHOLE voxels. `* 0.5f` gave +1.5 for a bed, and iso-rooms.md §7 measured exactly this:
            // a fractional offset puts corners between lattice points and the two rasterizers then
            // disagree everywhere (+0.25 was forty times worse than 0). A critic traced the remaining
            // stray pixels on occupied beds back to this line.
            ax = (float)(ob->tx * TN_TILE_VOX + (ofp[0] - fp[0]) / 2);
            ay = (float)(ob->ty * TN_TILE_VOX + (ofp[1] - fp[1]) / 2);
            // Rest on the SURFACE A BODY USES, which is the top only when nothing says otherwise.
            const short rest = ISO_REST_Z[OBJ_CELL[ob->kind]];
            az = (float)(rest ? rest : ofp[2]);
            arot = tn_rot;                            // align with the furniture, not with the walk
        }
        // A SLEEPER DOES NOT SLUMP, and this is the one exception the rule needs. Every other pose is
        // a body holding itself up, so a collapse of that effort reads; a lying figure is already
        // horizontal, so squashing and shearing it just makes a tidy shape untidy and says nothing.
        // Someone in bed is also, right then, doing the thing about it.
        const float droop = (pcell == ISO_PERSON_LIE) ? 0.0f : tnr_distress(&tn_agent[i]);
        tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, ax + fp[0]*0.5f, ay + fp[1]*0.5f) + 0.5f,
                             pcell, arot, ax, ay, az, fp[0], fp[1], az == 0.0f,
                             (int)tn_agent[i].household, amyaw, droop };

        // ── the event chip: WHERE it goes, decided here where the body's placement is already known.
        // Head height comes from the POSE's own footprint (fp[2] is nz), so a chip floats the same
        // gap above a sitter as above a stander without anyone writing a number per pose.
        // The timer restarts only when the event CHANGES, so a long queue blinks once (see the block
        // above tnr_event_of). Decremented here because tn_draw_world runs exactly once a frame.
        const int ev = tnr_event_of(&tn_agent[i]);
        if (ev != TNR_EV_NONE && ev != tnr_bub[i].last) { tnr_bub[i].kind = (unsigned char)ev;
                                                          tnr_bub[i].left = TNR_BUB_FRAMES; }
        tnr_bub[i].last = (unsigned char)ev;
        if (tnr_bub[i].left > 0) tnr_bub[i].left--;
        tnr_bub[i].wx = ax + fp[0] * 0.5f;
        tnr_bub[i].wy = ay + fp[1] * 0.5f;
        tnr_bub[i].wz = az + (float)fp[2] + 3.0f;              // clear of the head, in voxels
    }
    // ── EDGE WALLS ──────────────────────────────────────────────────────────
    // world.h stores walls on tile EDGES, not tiles, and only each tile's north and west edge (the
    // south edge IS the next tile's north). So iterating N and W per tile visits every wall exactly
    // once, with no double-draw to guard against.
    //
    // Orientation needs no `facing` field and cannot be got wrong: the DIRECTION is the orientation,
    // so a north/south edge takes the *_NS model and an east/west edge the *_EW one. That was the
    // world agent's reason for choosing edges, and it makes iso-rooms §8's turned-footprint bug
    // unrepresentable here rather than merely avoided.
    //
    // A wall model is 6 voxels long and 2 thick, so it straddles the boundary: offset by half its
    // thickness so it sits ON the edge rather than inside one of the two tiles.
    // NEAR WALLS ARE CUT, and the far shell is drawn. Both were missing and a critic measured the
    // cost: across 64 object-by-rotation samples, mean visibility 52%, twenty-one under 25%, and two
    // beds at exactly 0% — with 89% of all occluded furniture pixels covered by a wall colour. The
    // one shared toilet, which design §1's soul is literally about, showed ZERO of its 136 bright
    // pixels at three of the four rotations.
    //
    // This is not a new idea: iso-rooms.md §8 settled it ("FULL height with the near side cut away,
    // which beat Theme Hospital's low stubs plainly"). It simply was never implemented here, because
    // the first pass drew every edge unconditionally.
    //
    // Nearness is DERIVED from the projection, not hardcoded per rotation, so it survives a change to
    // the turn: step along the edge's outward normal and see whether depth rises. And the loops run
    // to <= so the far shell exists at all — tn_edge_at already reports the outside ring SOLID, so
    // this needs no new data.
    // The cutaway follows the CONTINUOUS view direction, so a wall cuts away at any orbit angle and
    // not only at the four baked ones. Still derived from the projection rather than tabulated per
    // rotation: stepping along an edge's outward normal and asking whether nearness rises is exactly
    // the old test, and at tn_yaw = 45 + 90*tn_rot the two agree — which is why the sprite view sees
    // no change. (Written out: north is near when cos(yaw) < 0, west when sin(yaw) < 0.)
    float vdx, vdy, vdz; tnr_viewdir(&vdx, &vdy, &vdz);
    const int near_n = (-vdy) > 0.0f;                   // a north edge faces the camera
    const int near_w = (-vdx) > 0.0f;                   // a west edge faces the camera
    for (int ty = 0; ty <= tn_bh; ty++) {
        for (int tx = 0; tx <= tn_bw; tx++) {
            const float vx = tx * TN_TILE_VOX, vy = ty * TN_TILE_VOX;
            if (tx < tn_bw) {
                const int en = tn_edge_at(tx, ty, TN_DIR_N);
                if (en != TN_WALL_NONE) {
                    // A door already draws low. A NEAR wall draws low for the same reason: you have to
                    // see past it. Costs no atlas and no new art, because both models exist.
                    const int cell = (en == TN_WALL_DOOR || near_n) ? ISO_WALL_LOW_NS : ISO_WALL_FULL_NS;
                    const short *fp = ISO_FOOTPRINT[cell];
                    tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, vx + fp[0] * 0.5f, vy),
                                                 cell, tn_rot, vx, vy - 1.0f, 0.0f, fp[0], fp[1], 0, -1, 0 };
                }
            }
            if (ty < tn_bh) {
                const int ew = tn_edge_at(tx, ty, TN_DIR_W);
                if (ew != TN_WALL_NONE) {
                    const int cell = (ew == TN_WALL_DOOR || near_w) ? ISO_WALL_LOW_EW : ISO_WALL_FULL_EW;
                    const short *fp = ISO_FOOTPRINT[cell];
                    tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, vx, vy + fp[1] * 0.5f),
                                                 cell, tn_rot, vx - 1.0f, vy, 0.0f, fp[0], fp[1], 0, -1, 0 };
                }
            }
        }
    }

    qsort(tnr_dl, tnr_dl_n, sizeof tnr_dl[0], tnr_cmp_draw);
    if (tn_poly) { tnr_draw_poly(); tnr_draw_bubbles(); return; }
    for (int i = 0; i < tnr_dl_n; i++) {
        const IsoCell *c = &ISO_CELLS[tnr_dl[i].cell][tnr_dl[i].rot];
        float sx, sy; tnr_iso_project(tn_rot, tnr_dl[i].vx, tnr_dl[i].vy, tnr_dl[i].vz, &sx, &sy);
        // A contact shadow, but only for things that stand on a TILE. A wall stands on an EDGE, so
        // its footprint quad would smear a dark stripe along the boundary between two rooms.
        if (tnr_dl[i].shadow) {
            // one-voxel-INSET: an integer inset, because a fractional pad puts the quad between
            // lattice points and strays pixels everywhere (iso-rooms.md §7's measured table)
            float q[4][2]; const float pad = 1.0f;
            const float x0 = tnr_dl[i].vx+pad, y0 = tnr_dl[i].vy+pad;
            const float x1 = tnr_dl[i].vx+tnr_dl[i].fp0-pad, y1 = tnr_dl[i].vy+tnr_dl[i].fp1-pad;
            tnr_iso_project(tn_rot, x0,y0,0,&q[0][0],&q[0][1]); tnr_iso_project(tn_rot, x1,y0,0,&q[1][0],&q[1][1]);
            tnr_iso_project(tn_rot, x1,y1,0,&q[2][0],&q[2][1]); tnr_iso_project(tn_rot, x0,y1,0,&q[3][0],&q[3][1]);
            quadfill((int)(q[0][0]+cam_x),(int)(q[0][1]+cam_y),(int)(q[1][0]+cam_x),(int)(q[1][1]+cam_y),
                     (int)(q[2][0]+cam_x),(int)(q[2][1]+cam_y),(int)(q[3][0]+cam_x),(int)(q[3][1]+cam_y),
                     CLR_BROWNISH_BLACK);
        }
        sspr(c->x, c->y, c->w, c->h, (int)(sx+cam_x)-c->ox, (int)(sy+cam_y)-c->oy, c->w, c->h);
    }
    tnr_draw_bubbles();
}

#endif // TENEMENT_ART_H
