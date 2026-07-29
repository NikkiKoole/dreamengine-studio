/* de:meta
{
  "slug": "boxhuman",
  "collection": ["physics"],
  "title": "Box Human — five builds of one rig table, fifteen bones each",
  "status": "active",
  "created": "2026-07-28",
  "kind": ["tech-demo", "probe"],
  "teaches": ["rigid-body", "procedural-mesh"],
  "lineage": "Slice 4 of the playtime-into-dreamengine port. puppet.c gave the data-driven Rig (parts + sprung hinges); boxskin.c skinned ONE texture across ONE elbow with LBS. This is the whole figure: the 1:1 between sprite and body is broken (15 invisible bones, 6 sprite skins), weights come from distance to a bone SEGMENT rather than to a bone centre (playtime's applySegmentWeights lesson: point-distance collapses mesh width at bone midpoints), and the blend is DQS rather than LBS so a folded knee doesn't candy-wrapper. A whole leg is ONE texture bending at hip, knee AND ankle.",
  "description": {
    "summary": "FIVE humanoids on a horizon, all instances of one rig table, each built from three numbers: a stiffness multiplier on its joint springs and KEEP_ANGLE gains (the floppiest slumps and piles up, the stiffest stands rigid), an overall SCALE, and a GIRTH that widens it across its own centre line. That gives a small wiry kid, a short barrel, the rig exactly as authored, a tall thin one and a broad bruiser, with no second rig table and no second sprite sheet anywhere. Each limb is a single continuous sprite strip stretched over several Box2D bones. The right leg is one 20x62 texture that bends at the hip, the knee and the ankle; the left leg is the SAME texture mirrored, and all five guys share that one sheet. Each figure gets its own Box2D collision group, so a guy ignores his own limbs but bumps into his neighbours. Press F to fold a limb to its limit, then SPACE to A/B four ways of deforming that texture: a Bezier SPINE, dual-quaternion and linear weighted blends, and rigid one-bone-per-vertex. Every limb is BANDED across its axis on purpose: a flat colour tells you nothing about how a skin deformed, but a regular stripe fans out on the outside of a bend, bunches on the inside, and shears visibly the moment a blend mode gets it wrong.",
    "detail": "Four deformation modes over one rig, because the interesting question is not how to attach a texture to a bone but what happens at a hard bend. SPINE is playtime's MESHUSERT spine-bind ported: each vertex is stored as (t, s) — arc-length fraction along a Bezier drawn through the limb's joint chain, plus signed perpendicular offset — and replanted each frame on the live curve, with no weights and nothing to tune. On top of it sits a CURVATURE CLAMP. playtime's own miter clamp (written for its ribbon path, never wired into spine-bind) was tried first and measured worse than no clamp: it is the right formula for a polyline corner, but a Bezier spine has no such corner, so min(segA,segB) only measures sample spacing and collapses exactly at a bend. The correct bound is the local radius of curvature - an offset curve folds on the concave side once the offset exceeds R - and clamping to 0.92R takes the fold artifact to zero at every angle tested. The cart carries its own oracle for this — every triangle's winding is baked at bind time and compared each frame, so 'inverted triangles' is a number, not an opinion. At a full elbow-and-knee fold: SPINE+clamp 4, SPINE without clamp 7, DQS 19, LBS 19, RIGID 43, out of 880. The structural move over puppet.c: BONES and SKINS are separate lists. 15 bones are invisible Box2D boxes joined by sprung, angle-limited revolutes (pelvis, chest, head, and upper/lower/extremity for each of four limbs). 6 skins are sprite rects from the sheet, each naming the subset of bones that deforms it. Per skin: a grid over the rect's opaque pixels becomes a triangle mesh; each vertex is weighted by 1-smoothstep(0, radius, distanceToBoneSegment), pruned to the best 3 bones and renormalised; bind pose stores the vertex in each bone's LOCAL frame plus that bone's rest angle. Each frame the vertex is placed by blending the bones' DELTA rotations as a circular mean, rotating the bind vertex once by that blend, then adding the weight-averaged translation residual, and tritex draws the triangles with UVs that never move. Left and right limbs share one sprite rect: the geometry mirrors, the UVs do not. The sprite sheet is drawn as a deformation TEST PATTERN — breton torso, banded sleeves and shorts, striped socks, cross-banded shoes — so the eye can read stretch and shear directly instead of guessing from a silhouette. The per-bone blend radius is what keeps a foot rigid while a thigh blends over 14 pixels. BUILDS: size and girth are one anisotropic transform (uniform about the authored SOLE line so a small guy still stands on the floor, x-only about the CENTRE line so a wide guy stays over his own feet) through which every authored coordinate already passed on its way in. Applying it at BIND rather than at draw is what keeps it free: the mesh is born the size it will be, so the weights, the (t,s) spine pair and the baked rest windings all describe this particular guy, and no per-frame code knows a figure isn't the authored size. Three things have to ride it or the illusion breaks, and each was a visible bug first: the blend radius (a px distance, so a wide guy's outer column would fall outside every radius and skin rigidly), the collision half-width (or neighbours sink into a barrel's belly, since only the shape is solid), and a per-skin share of the girth, because a barrel wants a wide belly and not a wide head. The winding oracle says the transform is honest: in SPINE mode a 400-frame fold peaks at 12 inverted triangles of 22000 against 10 before, statistically the same. The weighted modes do get worse (DQS peaks 45 against 33), and the new per-BUILD flip counter says why, which is the point of having it: the flips land on the small floppy build, which now gets shoved around by heavier neighbours and reaches deeper collapses, not on the wide ones. Springs deliberately do NOT scale with size; the source carries the measurement that says the correction is inert across this range.",
    "controls": "Drag any bone with the mouse to pose the figure. F drives the right elbow and knee to a full fold (the extreme-pose test - a mouse drag cannot reliably reach it, the chain would rather swing at the hip). SPACE cycles SPINE / DQS / LBS / RIGID. C toggles spine mode's curvature clamp. M shows the mesh wireframe. B shows the bone skeleton. R resets."
  },
  "todo": [
    "Real PROPORTIONS (long legs, short torso, big head) need the rig authored as bone LENGTHS and DIRECTIONS with positions derived by walking the parent chain. BONES[] stores absolute screen coordinates, so lengthening a thigh today would leave the knee, the ankle and the leg skin's rest placement behind. Scale and girth are as far as an affine transform of the authored table can go. That refactor is worth ~60 lines and pays for itself twice: it gives posing for free, and it is the shape boneskin.h wants anyway.",
    "Girth is one number per figure with a per-skin share; the honest next rung is per-BONE thickness. In SPINE mode that is nearly free (one multiply on s, the perpendicular offset), which would give a pot belly on thin arms. In DQS the bind offsets are absolute vectors, so it needs decomposing each into along-bone and across-bone components first.",
    "Stretched stripes: the banding is baked into the sheet, so a scaled limb gets scaled banding. It reads as 'different build' rather than wrong, but five scales of one drawing is not five characters. Genuinely different silhouettes need their own sprite rects.",
    "Promote the skinning half to runtime/boneskin.h once a second cart wants it - the Fig struct, both binds, the four modes and the winding oracle. The crowd refactor already proved the seam: one rig TABLE, N instances.",
    "Spine-bind can't branch, so the torso and head still fall back to DQS. playtime's multi-chain bind (hard per-vertex chain assignment) is the next rung.",
    "Adaptive grid: denser rows near a joint, coarser along a straight limb (playtime's deform-textured learning).",
    "Alpha-aware weights so a bone's influence can't leak across a transparent gap to the other limb."
  ]
}
de:meta */
#include "studio.h"
#include "box2d/box2d.h"      // opt-in: make-cart/play link the vendored Box2D v3
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PPM     22.0f
#define SX(wx)  ((int)((wx) * PPM))
#define SY(wy)  ((int)(SCREEN_H - (wy) * PPM))
#define WX(sx)  ((sx) / PPM)
#define WY(sy)  ((SCREEN_H - (sy)) / PPM)
#define DEG     (3.14159265f/180.0f)
#define PI      3.14159265f
#define FLOOR_PY 132        // the horizon, screen px — the figure rests its feet here

// ── the rig, as data ───────────────────────────────────────────────────
// A BONE is an invisible Box2D box. Its segment (ax,ay)->(bx,by) is both the
// box's extent and the line that skin vertices measure their distance to.
typedef struct {
    const char *name;
    int   parent;                 // bone index, -1 = root (hangs from ground)
    float ax, ay, bx, by;         // segment, screen px, rest pose
    float radius;                 // skinning blend radius (px) — small = stays rigid
    // Hinge limits (deg, around rest) + spring Hz. These are deliberately
    // PERMISSIVE (-145 elbow, -135 knee) so the fold test can reach the angles
    // where the blend modes visibly disagree. A weighted-bone skin is a flat
    // sheet with no volume: past roughly 110 deg the inner side of the bend has
    // nowhere to go and DQS/LBS turn the mesh inside out (swept clean at -70 and
    // -95, breaking by -120, plainly inverted at -138). SPINE mode doesn't have
    // that ceiling — the Bezier spreads the corner over 33 samples so no vertex
    // pair ever crosses — which is exactly why the mode is worth having. Capping
    // the joint at ~110 is the alternative if you only ever want DQS.
    float lo, hi, hz;
    // KEEP_ANGLE (playtime's per-body behaviour, src/keep-angle.lua). Gain of a
    // PD controller that steers this bone back to the world angle it was
    // AUTHORED at — upright for the spine bones, flat for the feet. 0 = off.
    float keepKp;
    // Half-WIDTH of the collision box, metres. Not cosmetic: a 2px-thin bone has
    // almost no rotational inertia, so a KEEP_ANGLE controller writing its
    // angular velocity gets absorbed by its heavier children and loses (measured:
    // the pelvis sat 97 deg off target). Torso bones need real bulk.
    float hw;
} BoneDef;

// A SKIN is one sprite rect deformed by a SUBSET of the bones. Breaking the
// 1:1 between sprite and body is the whole point: a leg strip spans four.
typedef struct {
    const char *name;
    int   ox, oy, w, h;           // rect on the sheet
    float px, py;                 // where its top-left sits at rest (screen px)
    bool  mirror;                 // flip the GEOMETRY horizontally; UVs unchanged
    float gm;                     // how much of the figure's GIRTH this skin takes
                                  // (1 = all of it). A barrel wants a wide belly,
                                  // not a wide head, and a sleeve reads best a
                                  // little narrower than the torso it hangs off.
    int   bones[6], nbones;
    // SPINE mode only: the bones that form this skin's CHAIN, root to tip. The
    // chain's points are bone[0]'s start followed by every bone's end, so 3
    // bones give a 4-point spine. A branching skin (torso, head) leaves this
    // empty and stays on DQS — a spine is a line, it can't fork.
    int   chain[5], nchain;
} SkinDef;

enum { B_PELVIS, B_CHEST, B_HEAD,
       B_UARM_R, B_LARM_R, B_HAND_R,
       B_UARM_L, B_LARM_L, B_HAND_L,
       B_THIGH_R, B_SHIN_R, B_FOOT_R,
       B_THIGH_L, B_SHIN_L, B_FOOT_L, NBONE };

static const BoneDef BONES[NBONE] = {
    // name      parent      ax   ay    bx   by   rad    lo    hi   hz   keepKp   hw
    {"pelvis",  -1,        160,  72,  160,  60,  16,     0,    0, 0.0f, 20.0f, 0.30f},
    {"chest",    B_PELVIS, 160,  60,  160,  44,  16,   -25,   25, 6.0f,  0.0f, 0.32f},
    {"head",     B_CHEST,  160,  44,  160,  28,  10,   -40,   40, 3.0f,  6.0f, 0.26f},

    {"uArmR",    B_CHEST,  176,  48,  176,  65,  11,  -150,  150, 1.2f,  0.0f, 0.09f},
    {"lArmR",    B_UARM_R, 176,  65,  176,  82,  10,  -145,    5, 1.2f,  0.0f, 0.08f},
    {"handR",    B_LARM_R, 176,  82,  176,  90,   6,   -60,   60, 2.0f,  0.0f, 0.09f},

    {"uArmL",    B_CHEST,  144,  48,  144,  65,  11,  -150,  150, 1.2f,  0.0f, 0.09f},
    {"lArmL",    B_UARM_L, 144,  65,  144,  82,  10,    -5,  145, 1.2f,  0.0f, 0.08f},
    {"handL",    B_LARM_L, 144,  82,  144,  90,   6,   -60,   60, 2.0f,  0.0f, 0.09f},

    {"thighR",   B_PELVIS, 166,  74,  166,  97,  13,   -95,   95,10.0f,  9.0f, 0.15f},
    {"shinR",    B_THIGH_R,166,  97,  166, 118,  12,  -135,    3,10.0f,  9.0f, 0.13f},
    {"footR",    B_SHIN_R, 166, 118,  178, 124,   7,   -35,   45, 6.0f, 12.0f, 0.10f},

    {"thighL",   B_PELVIS, 154,  74,  154,  97,  13,   -95,   95,10.0f,  9.0f, 0.15f},
    {"shinL",    B_THIGH_L,154,  97,  154, 118,  12,  -135,    3,10.0f,  9.0f, 0.13f},
    {"footL",    B_SHIN_L, 154, 118,  142, 124,   7,   -45,   35, 6.0f, 12.0f, 0.10f},
};

// Listed BACK to FRONT — the painter's order is the whole depth story.
static const SkinDef SKINS[] = {
    {"armL",  56, 0, 14, 50, 138, 43, true,  0.85f, {B_CHEST, B_UARM_L, B_LARM_L, B_HAND_L}, 4,
                                                    {B_UARM_L, B_LARM_L, B_HAND_L}, 3},
    {"legL",  76, 0, 20, 62, 141, 69, true,  0.90f, {B_PELVIS, B_THIGH_L, B_SHIN_L, B_FOOT_L}, 4,
                                                    {B_THIGH_L, B_SHIN_L, B_FOOT_L}, 3},
    {"legR",  76, 0, 20, 62, 160, 69, false, 0.90f, {B_PELVIS, B_THIGH_R, B_SHIN_R, B_FOOT_R}, 4,
                                                    {B_THIGH_R, B_SHIN_R, B_FOOT_R}, 3},
    {"torso", 24, 0, 28, 41, 146, 40, false, 1.00f, {B_PELVIS, B_CHEST, B_HEAD}, 3,  {0}, 0},
    {"armR",  56, 0, 14, 50, 169, 43, false, 0.85f, {B_CHEST, B_UARM_R, B_LARM_R, B_HAND_R}, 4,
                                                    {B_UARM_R, B_LARM_R, B_HAND_R}, 3},
    {"head",   0, 0, 20, 20, 150, 20, false, 0.45f, {B_HEAD, B_CHEST}, 2,            {0}, 0},
};
#define NSKIN ((int)(sizeof(SKINS)/sizeof(SKINS[0])))

// ── mesh storage ───────────────────────────────────────────────────────
#define STEP  3               // grid spacing over the sprite, in sheet px
#define MAXG  24              // grid columns/rows per skin
#define MAXV  (MAXG*MAXG)
#define MAXT  (MAXV*2)
#define KB    3               // bones blended per vertex (after pruning)

typedef struct {
    float  uvx, uvy;          // sheet pixel — never moves
    b2Vec2 bind;              // rest position, world metres
    int    bone[KB];
    float  w[KB];
    b2Vec2 off[KB];           // rest position in that bone's LOCAL frame
    float  bindAng[KB];       // that bone's rest angle
    int    nb;
    float  t, s;              // SPINE bind: arc-length fraction along the chain's
                              // dense polyline, + signed perpendicular offset (px)
} Vtx;

typedef struct {
    Vtx vtx[MAXV]; int nv;
    int tri[MAXT][3]; int nt;
    signed char restSign[MAXT];   // winding at rest; a flip at draw time == the
                                  // mesh folded through itself right there
} Mesh;

// ── the crowd ──────────────────────────────────────────────────────────
// One rig TABLE, N instances of it. Everything that was a global array is now
// per-figure: the bodies, the joints, and the six deformed meshes (bind data is
// per-instance because it is captured in each figure's own world coordinates).
#define NFIG 5
typedef struct {
    b2BodyId  bone[NBONE];
    b2JointId hinge[NBONE];
    b2Vec2    locA[NBONE], locB[NBONE];   // segment ends, body-local
    float     rest[NBONE];                // world angle at rest = the KEEP_ANGLE target
    Mesh      mesh[NSKIN];
    float     dx;                         // shift from the authored rig, screen px
    float     stiff;                      // per-guy multiplier on springs + keep gain:
                                          // the crowd is one table, five temperaments
    float     sc;                         // overall SIZE, 1 = the authored rig
    float     girth;                      // thickness ACROSS the figure, 1 = authored
} Fig;
static Fig fig[NFIG];

// Five BUILDS from one rig: a spring multiplier (temperament), a uniform SCALE,
// and an x-only GIRTH. Nothing here is a second rig table — the last two are one
// anisotropic transform (figx/figy below) through which every authored
// coordinate already passes, so no stage downstream of it — weights, both binds,
// the winding oracle — needs to know a figure isn't the authored size.
typedef struct { const char *name; float dx, stiff, sc, girth; } BuildDef;
static const BuildDef BUILDS[NFIG] = {
    //   name        dx   stiff     sc   girth
    { "kid",       -124, 0.35f, 0.72f, 0.95f },   // small, slim, floppiest
    { "barrel",     -62, 0.70f, 0.86f, 1.32f },   // short and wide
    { "author",       0, 1.00f, 1.00f, 1.00f },   // the rig exactly as authored
    { "lanky",       62, 1.45f, 1.10f, 0.78f },   // tall and thin
    { "bruiser",    124, 2.10f, 1.14f, 1.20f },   // tall, broad, stiffest
};

// The authored rig's centre line and SOLE line. Size scales about the sole, so a
// small guy still stands ON the floor instead of hovering or sinking; girth
// scales about the centre line, so a wide guy stays centred over his own feet.
#define RIG_CX   160.0f
#define RIG_BY   131.0f
static inline float figy(const Fig *F, float y) { return RIG_BY + (y - RIG_BY) * F->sc; }
// `g` is the girth actually applied: bones always take the figure's in full,
// a skin takes the fraction its SkinDef.gm asks for.
static inline float figxg(const Fig *F, float x, float g) {
    return RIG_CX + F->dx + (x - RIG_CX) * F->sc * g;
}
static inline float figx(const Fig *F, float x) { return figxg(F, x, F->girth); }
static inline float skin_girth(const Fig *F, const SkinDef *s) {
    return 1.0f + (F->girth - 1.0f) * s->gm;
}

// SPRINGS DO NOT GET RESCALED WITH SIZE, and that was worth measuring rather
// than assuming. The suspicion was a scaling law: gravity's torque on a limb
// grows as L^3 (mass L^2 times a moment arm L) while the joint's rotational
// inertia grows as L^4, so a fixed spring should let a smaller figure droop
// further, and hertz being a FREQUENCY (a pendulum's is sqrt(g/L)) suggests a
// 1/sqrt(L) correction. Tried, and it is INERT over the range this table uses:
// 1/sqrt(0.72) is an 18% bump and the smallest build's standing height came out
// identical to two decimals at every sample (0.88 0.86 0.76 0.76 with and
// without). Forcing the factor to 3.0 DOES hold him up flat at 0.88, so the
// mechanism is real — it just sits far outside a 0.72..1.14 spread. Left out:
// a knob with no measurable effect is worse than no knob. Push the sc column
// past roughly 2x either way and this is the first thing to revisit.
//
// (The floppiest build slowly folding onto the floor over ~300 frames is NOT
// this. It does that at authored size too — checked against the previous commit
// frame for frame. It is what stiff=0.35 means, and it is half the demo.)
static inline float size_hz(const Fig *F) { return F->stiff; }

static b2WorldId world;
static b2BodyId  ground;
static int       dragFig = -1, dragBone = -1;

static b2JointId mjoint; static bool dragging = false;
static int  mode = 3;   // 0=DQS 1=LBS 2=RIGID 3=SPINE. SPINE is the default now.
                        // On ONE figure the two tie (elbow-only fold sweep at
                        // -70/-95/-120/-138 gives 0 0 0 0 for both; spine with the
                        // clamp off gives 0 1 4 5, the offset-curve fold). But a
                        // CROWD reaches poses one posed figure never does — the
                        // floppy guys collapse and pile up — and there they part.
                        // Peak inverted triangles over a 300-frame F fold, out of
                        // the crowd's 22000, and the same number split by BUILD
                        // (summed over the run) now that the five differ in size:
                        //                 peak    kid barrel author lanky bruiser
                        //   SPINE+clamp     12     451   718     72    40     115
                        //   DQS             45    4521  1734    221  1076      42
                        //   LBS             45    4521  1734    221  1076      42
                        //   RIGID          118   11555  6412   3110  2997       0
                        // Two things fall out of that split. DQS and LBS are byte
                        // identical here, as they were before the crowd grew: the
                        // triangles that invert are ones both blends get wrong.
                        // And the weighted modes' extra flips are NOT the girth —
                        // the widest build is the CLEANEST of the five in DQS (42
                        // of 7594) while the small floppy one owns 60% of them.
                        // Being wide doesn't break a weighted skin; collapsing in
                        // a heap does, and the floppiest build is the one doing it.
                        // Torso and head have no chain and fall back to DQS either
                        // way, so nothing is lost by defaulting to spine.
static bool showMesh = false, showBones = false, folding = false, curveClamp = true;
static int  keepMode = 1;   // 0 = off, 1 = playtime omega-write, 2 = torque.
                            // 1 is the default because it MEASURES best, see keep_angle().
static const char *KEEP_NAME[3] = { "off", "omega", "torque" };
static const char *MODE_NAME[4] = { "DQS", "LBS", "RIGID", "SPINE" };

// ── little maths ───────────────────────────────────────────────────────
static float smoothstep(float e0, float e1, float v) {
    if (v <= e0) return 0; if (v >= e1) return 1;
    float t = (v - e0) / (e1 - e0);
    return t * t * (3 - 2 * t);
}
// Distance from p to the SEGMENT ab. Using the segment, not the bone's centre,
// is what stops a limb pinching at the middle of each bone.
static float dist_seg(float px, float py, float ax, float ay, float bx, float by) {
    float abx = bx - ax, aby = by - ay, l2 = abx*abx + aby*aby;
    float t = (l2 < 1e-6f) ? 0 : ((px-ax)*abx + (py-ay)*aby) / l2;
    if (t < 0) t = 0; if (t > 1) t = 1;
    float dx = px - (ax + abx*t), dy = py - (ay + aby*t);
    return sqrtf(dx*dx + dy*dy);
}
static inline float bone_angle(const Fig *F, int b) { return b2Rot_GetAngle(b2Body_GetRotation(F->bone[b])); }
// Distance from a rest-pose skin pixel to bone b's segment, both in THIS figure's
// transformed coordinates — the one place the build's scale/girth has to meet the
// authored rig table, so weights measure the guy who exists, not the table's guy.
static inline float bone_dist(const Fig *F, int b, float rx, float ry) {
    const BoneDef *d = &BONES[b];
    return dist_seg(rx, ry, figx(F, d->ax), figy(F, d->ay), figx(F, d->bx), figy(F, d->by));
}

// ── SPINE bind — playtime's MESHUSERT spine mode, ported ───────────────
// Instead of weighting a vertex to bones, decompose it against the limb's AXIS:
// t = arc-length fraction along a Bezier through the joint chain, s = signed
// perpendicular offset. At draw time the Bezier is rebuilt from live joints and
// the vertex is replanted at curve(t) + s * left-normal. No weights, no radii,
// nothing to tune. Everything below works in SCREEN pixels (float).
//
// The one rule that makes rest pose round-trip exactly (spine-mesh.lua's header
// says this cost them a bug): bind AND evaluate must both go through the same
// DENSE POLYLINE, and t must be ARC-LENGTH fraction, not the Bezier parameter.
// Those are different mappings; mixing them shifts every vertex on bind.
#define SPN_SAMP  33          // dense samples per chain (spine-mesh.lua's depth 5)
#define SPN_CTRL  24
#define SPN_BEND  2           // control-point duplication — playtime's "bendiness"

// de Casteljau over n control points.
static void bez_eval(const float *c, int n, float u, float *x, float *y) {
    float tx[SPN_CTRL], ty[SPN_CTRL];
    for (int i = 0; i < n; i++) { tx[i] = c[i*2]; ty[i] = c[i*2+1]; }
    for (int k = n - 1; k > 0; k--)
        for (int i = 0; i < k; i++) {
            tx[i] += u * (tx[i+1] - tx[i]);
            ty[i] += u * (ty[i+1] - ty[i]);
        }
    *x = tx[0]; *y = ty[0];
}

// Chain points -> dense polyline. Middle control points are duplicated so the
// curve HUGS the joints instead of smoothing them away (doubleControlPoints).
static int spine_sample(const float *pts, int npt, float *out) {
    float c[SPN_CTRL * 2]; int n = 0;
    for (int i = 0; i < npt; i++) {
        c[n*2] = pts[i*2]; c[n*2+1] = pts[i*2+1]; n++;
        if (i > 0 && i < npt - 1)
            for (int d = 0; d < SPN_BEND && n < SPN_CTRL; d++) {
                c[n*2] = pts[i*2]; c[n*2+1] = pts[i*2+1]; n++;
            }
    }
    for (int i = 0; i < SPN_SAMP; i++)
        bez_eval(c, n, (float)i / (SPN_SAMP - 1), &out[i*2], &out[i*2+1]);
    return SPN_SAMP;
}

static float spine_arcs(const float *poly, int n, float *arcs) {
    arcs[0] = 0;
    for (int i = 1; i < n; i++) {
        float dx = poly[i*2] - poly[(i-1)*2], dy = poly[i*2+1] - poly[(i-1)*2+1];
        arcs[i] = arcs[i-1] + sqrtf(dx*dx + dy*dy);
    }
    return arcs[n-1];
}

// Project (px,py) onto the polyline -> (t, s). Overshoot past the first/last
// segment is preserved rather than clamped, so a vertex beyond the chain's end
// (the hand past the wrist) records its along-axis offset instead of collapsing
// onto the endpoint.
static void spine_project(float px, float py, const float *poly, int n,
                          const float *arcs, float total, float *outT, float *outS) {
    float bestD2 = 1e18f; *outT = 0; *outS = 0;
    for (int i = 1; i < n; i++) {
        float ax = poly[(i-1)*2], ay = poly[(i-1)*2+1];
        float dx = poly[i*2] - ax, dy = poly[i*2+1] - ay;
        float l2 = dx*dx + dy*dy;
        if (l2 < 1e-12f) continue;
        float u = ((px-ax)*dx + (py-ay)*dy) / l2;
        if (i > 1     && u < 0) u = 0;
        if (i < n - 1 && u > 1) u = 1;
        float qx = px - (ax + u*dx), qy = py - (ay + u*dy);
        float d2 = qx*qx + qy*qy;
        if (d2 < bestD2) {
            bestD2 = d2;
            *outT  = (arcs[i-1] + u * sqrtf(l2)) / (total > 1e-9f ? total : 1e-9f);
            *outS  = ((qx*(-dy) + qy*dx) >= 0 ? 1.0f : -1.0f) * sqrtf(d2);
        }
    }
}

// The MITER CLAMP, from playtime's texturedCurve (box2d-draw-textured.lua:1461).
// DEFAULT OFF, on measurement: it makes things worse here (rest 6 / fold 16 with
// it, 0 / 5 without). It assumes a smoothly swept ribbon width; our dense Bezier
// bunches its samples near a corner, so min(segA,segB) collapses there and the
// limit crushes the cross-section instead of protecting it. Clamping relative to
// the bind-pose limit rather than absolutely was tried too, and did not help.
// Kept behind key C because it is playtime's actual ribbon math and worth seeing.
// At a bend the inner edge of a swept width crosses itself; the largest offset
// that cannot cross is min(segA,segB) * tan(interior half-angle). This is the
// piece playtime wrote for its RIBBON path and never wired into spine-bind
// (SPINE-MESH-PLAN.md phase 4 still lists it as to-do) — it is what stops the
// fold-over the joint limits are otherwise there to dodge.
// Which polyline segment does arc-length `target` fall in? Bind and draw MUST
// use this same routine, or a bind-time and a draw-time measurement can land on
// different corners of the curve.
static int spine_seg(const float *arcs, int n, float target) {
    if (target <= arcs[0])   return 1;
    if (target >= arcs[n-1]) return n - 1;
    for (int i = 1; i < n; i++) if (target <= arcs[i]) return i;
    return n - 1;
}

// Local radius of curvature at sample i, from the circumradius of the triangle
// (P[i-1], P[i], P[i+1]). `turn` is the signed cross product: >0 means the curve
// bends toward the LEFT normal, so positive s is the concave (foldable) side.
// Returns false where the curve is straight enough that no clamp is needed.
static bool spine_curvature(const float *poly, int n, int i, float *R, float *turn) {
    if (i < 1 || i > n - 2) return false;
    float ax = poly[(i-1)*2], ay = poly[(i-1)*2+1];
    float bx = poly[i*2],     by = poly[i*2+1];
    float cx = poly[(i+1)*2], cy = poly[(i+1)*2+1];
    float abx = bx-ax, aby = by-ay, bcx = cx-bx, bcy = cy-by, acx = cx-ax, acy = cy-ay;
    float cross = abx*bcy - aby*bcx;
    if (fabsf(cross) < 1e-6f) return false;                       // straight
    float lab = sqrtf(abx*abx+aby*aby), lbc = sqrtf(bcx*bcx+bcy*bcy), lac = sqrtf(acx*acx+acy*acy);
    *R = (lab * lbc * lac) / (2.0f * fabsf(cross));               // circumradius
    *turn = cross;
    return true;
}

// Replant a vertex: walk to arc-length t, step s along the local left-normal.
static void spine_place(const float *poly, int n, const float *arcs, float total,
                        float t, float s, float *ox, float *oy) {
    float target = t * total;
    int seg = spine_seg(arcs, n, target);

    float ax = poly[(seg-1)*2], ay = poly[(seg-1)*2+1];
    float dx = poly[seg*2] - ax, dy = poly[seg*2+1] - ay;
    float segLen = arcs[seg] - arcs[seg-1];
    float u = segLen > 1e-9f ? (target - arcs[seg-1]) / segLen : 0.0f;
    float dlen = sqrtf(dx*dx + dy*dy);
    float tx = dlen > 1e-9f ? dx/dlen : 1.0f, ty = dlen > 1e-9f ? dy/dlen : 0.0f;

    if (curveClamp) {
        // CURVATURE CLAMP. An offset curve folds through itself on the CONCAVE
        // side as soon as the offset exceeds the local radius of curvature R —
        // that, not playtime's miter formula, is the actual condition here.
        // (The miter limit is for a polyline corner where two straight segments
        // meet; our spine is a smooth Bezier, so its samples never form that
        // corner and min(segA,segB) just measures sample spacing, which is why
        // the ported miter clamp measured WORSE than no clamp at all.)
        // Only the concave side can fold, so the convex side is left alone.
        float R, turn;
        if (spine_curvature(poly, n, seg, &R, &turn)) {
            float lim = R * 0.92f;                    // a hair inside the cusp
            if (turn > 0 ? (s > lim) : (s < -lim)) s = (turn > 0) ? lim : -lim;
        }
    }
    *ox = ax + u*dx + s * (-ty);
    *oy = ay + u*dy + s * ( tx);
}

// The chain's points in SCREEN px: bone[0]'s start, then every bone's end.
// `live` reads the bodies; otherwise the rest pose straight off the table.
static int spine_chain(const Fig *F, const SkinDef *sk, bool live, float *out) {
    int n = 0;
    for (int k = 0; k <= sk->nchain; k++) {
        int b = sk->chain[k == 0 ? 0 : k - 1];
        if (live) {
            b2Vec2 w = b2Body_GetWorldPoint(F->bone[b], k == 0 ? F->locA[b] : F->locB[b]);
            out[n*2] = w.x * PPM; out[n*2+1] = SCREEN_H - w.y * PPM;
        } else {
            out[n*2]   = figx(F, (k == 0) ? BONES[b].ax : BONES[b].bx);
            out[n*2+1] = figy(F, (k == 0) ? BONES[b].ay : BONES[b].by);
        }
        n++;
    }
    return n;
}

// ── build ──────────────────────────────────────────────────────────────
static void make_bones(Fig *F, int fi) {
    for (int i = 0; i < NBONE; i++) {
        const BoneDef *d = &BONES[i];
        float ax = figx(F, d->ax), ay = figy(F, d->ay);       // this BUILD's coords,
        float bx = figx(F, d->bx), by = figy(F, d->by);       // not the table's
        float mx = (ax + bx) * 0.5f, my = (ay + by) * 0.5f;
        float dx = bx - ax, dy = -(by - ay);                  // screen y is down
        float len = sqrtf(dx*dx + dy*dy) / PPM;
        // Every bone body rests at rotation IDENTITY and bakes its direction into
        // the SHAPE instead. Why: a revolute's limit is
        // b2RelativeAngle(qB,qA) - referenceAngle, and b2RelativeAngle returns
        // atan2 in (-pi, pi]. Rotating the bodies to point along their segments
        // gives the pelvis->thigh and chest->arm joints a reference angle of
        // exactly +/-180 (spine points up, limbs point down) — dead on the branch
        // cut, where a hair of numerical noise flips the measured angle by a full
        // turn. The solver then reads a colossal limit violation and snaps the
        // figure apart on frame 1 (measured: the pelvis 16 deg off after ONE step,
        // 56 deg after five). Identity bodies make every referenceAngle 0, so no
        // joint can ever sit on the cut, and bone_angle() becomes a clean
        // "rotation away from the authored pose".
        b2BodyDef bd = b2DefaultBodyDef();
        bd.type = b2_dynamicBody;
        bd.position = (b2Vec2){ WX(mx), WY(my) };
        F->bone[i] = b2CreateBody(world, &bd);
        // The collision box thickens with the build too, or a barrel's neighbours
        // would sink into his belly: the skin is wide, but only the SHAPE is solid.
        // (hw is measured across the bone, so girth is exact for the upright bones
        // and a slight over-estimate for the near-horizontal feet.)
        b2Polygon box = b2MakeOffsetBox(len * 0.5f, d->hw * F->sc * F->girth, (b2Vec2){0,0},
                                        b2MakeRot(atan2f(dy, dx)));
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.density = 1.0f; sd.material.friction = 0.5f;
        // A NEGATIVE group never collides with itself, so one group per figure
        // means each guy ignores his own limbs but still bumps into the others.
        sd.filter.groupIndex = -(fi + 1);
        b2CreatePolygonShape(F->bone[i], &sd, &box);
        F->locA[i] = b2Body_GetLocalPoint(F->bone[i], (b2Vec2){ WX(ax), WY(ay) });
        F->locB[i] = b2Body_GetLocalPoint(F->bone[i], (b2Vec2){ WX(bx), WY(by) });
        F->rest[i] = bone_angle(F, i);      // the pose the KEEP_ANGLE controller defends
    }
    for (int i = 0; i < NBONE; i++) {
        const BoneDef *d = &BONES[i];
        if (d->parent < 0) continue;      // the root is FREE now: it stands on the
                                          // floor (and can be thrown off it) rather
                                          // than hanging from a hook.
        b2BodyId a = F->bone[d->parent];
        b2Vec2 w = { WX(figx(F, d->ax)), WY(figy(F, d->ay)) };
        b2RevoluteJointDef j = b2DefaultRevoluteJointDef();
        j.bodyIdA = a; j.bodyIdB = F->bone[i];
        j.localAnchorA = b2Body_GetLocalPoint(a, w);
        j.localAnchorB = b2Body_GetLocalPoint(F->bone[i], w);
        j.collideConnected = false;
        // 0, and it MUST be: see the branch-cut note in make_bones. Every bone
        // rests at identity, so "joint angle 0" already means "the authored pose"
        // and lo/hi/targetAngle read as degrees away from it.
        j.referenceAngle = 0.0f;
        j.enableLimit = true; j.lowerAngle = d->lo*DEG; j.upperAngle = d->hi*DEG;
        j.enableSpring = d->hz > 0; j.hertz = d->hz * size_hz(F); j.dampingRatio = 0.7f;
        j.targetAngle = 0;
        F->hinge[i] = b2CreateRevoluteJoint(world, &j);
    }
}

// Rest screen position of sheet pixel (sx,sy) inside skin s. Mirroring flips
// the GEOMETRY across the rect; the UV stays (sx,sy), which is the whole trick
// that lets both legs share one texture.
// The build's transform is applied HERE, at bind, not at draw: the mesh is born
// at the size and width this figure is, so scale and girth are baked into every
// bind — the weights, the (t,s) spine pair and the rest windings all describe
// THIS guy. Nothing per-frame pays for a figure being big.
static void rest_px(const Fig *F, const SkinDef *s, int sx, int sy, float *rx, float *ry) {
    int dx = sx - s->ox;
    float ax = s->px + (s->mirror ? (s->w - 1 - dx) : dx);
    float ay = s->py + (sy - s->oy);
    *rx = figxg(F, ax, skin_girth(F, s));
    *ry = figy(F, ay);
}

static void bind_vertex(const Fig *F, const SkinDef *s, Vtx *v, float rx, float ry) {
    // weight against every bone this skin names, by distance to its segment
    // NOTE: `mirror` deliberately does NOT touch the bone segments. It flips the
    // sprite's GEOMETRY only; the left bones already carry left-side coordinates,
    // so mirroring here would send them back across the body.
    float wt[6]; int n = s->nbones;
    for (int k = 0; k < n; k++) {
        const BoneDef *d = &BONES[s->bones[k]];
        // The blend radius rides the build with the mesh. It has to: it is a
        // distance in px, so leaving it authored-size would make a wide guy's
        // outer column fall outside every radius and skin RIGIDLY, and a small
        // guy's whole limb sit deep inside one radius and over-blend.
        wt[k] = 1.0f - smoothstep(0, d->radius * F->sc * F->girth, bone_dist(F, s->bones[k], rx, ry));
    }
    // keep the best KB, renormalise (playtime's pruneTopK)
    v->nb = 0;
    for (int slot = 0; slot < KB; slot++) {
        int best = -1; float bw = 1e-6f;
        for (int k = 0; k < n; k++) {
            bool taken = false;
            for (int q = 0; q < v->nb; q++) if (v->bone[q] == s->bones[k]) taken = true;
            if (!taken && wt[k] > bw) { bw = wt[k]; best = k; }
        }
        if (best < 0) break;
        v->bone[v->nb] = s->bones[best]; v->w[v->nb] = bw; v->nb++;
    }
    if (v->nb == 0) {   // outside every radius — glue to the nearest segment
        int best = 0; float bd = 1e9f;
        for (int k = 0; k < n; k++) {
            float dd = bone_dist(F, s->bones[k], rx, ry);
            if (dd < bd) { bd = dd; best = k; }
        }
        v->bone[0] = s->bones[best]; v->w[0] = 1.0f; v->nb = 1;
    }
    float sum = 0; for (int k = 0; k < v->nb; k++) sum += v->w[k];
    for (int k = 0; k < v->nb; k++) v->w[k] /= sum;

    v->bind = (b2Vec2){ WX(rx), WY(ry) };
    for (int k = 0; k < v->nb; k++) {
        v->off[k]     = b2Body_GetLocalPoint(F->bone[v->bone[k]], v->bind);
        v->bindAng[k] = bone_angle(F, v->bone[k]);
    }
}

static bool cell_opaque(int sx, int sy) {
    for (int y = sy - STEP; y <= sy + STEP; y++)
        for (int x = sx - STEP; x <= sx + STEP; x++)
            if (x >= 0 && y >= 0 && x < 128 && y < 128 && sget(x, y) != 0) return true;
    return false;
}

static void build_mesh(Fig *F, int si) {
    const SkinDef *s = &SKINS[si];
    Mesh *m = &F->mesh[si];
    static int vid[MAXG][MAXG];
    int gw = s->w / STEP + 1, gh = s->h / STEP + 1;
    if (gw > MAXG) gw = MAXG;
    if (gh > MAXG) gh = MAXG;
    m->nv = m->nt = 0;
    for (int gy = 0; gy < gh; gy++)
        for (int gx = 0; gx < gw; gx++) {
            int sx = s->ox + gx*STEP; if (sx >= s->ox + s->w) sx = s->ox + s->w - 1;
            int sy = s->oy + gy*STEP; if (sy >= s->oy + s->h) sy = s->oy + s->h - 1;
            // Keep the vertex if ANY pixel in its cell is opaque, not just the
            // sample. Single-pixel testing chews holes wherever the silhouette
            // pinches (the torso's waist) — a cell that straddles the edge
            // should stay, its transparent texels simply draw as nothing.
            if (!cell_opaque(sx, sy)) { vid[gy][gx] = -1; continue; }
            Vtx *v = &m->vtx[m->nv];
            v->uvx = sx; v->uvy = sy;
            float rx, ry; rest_px(F, s, sx, sy, &rx, &ry);
            bind_vertex(F, s, v, rx, ry);
            v->t = v->s = 0;
            if (s->nchain > 0) {                  // second, independent bind
                float pts[8], poly[SPN_SAMP*2], arcs[SPN_SAMP];
                int np = spine_chain(F, s, false, pts);
                int nd = spine_sample(pts, np, poly);
                float total = spine_arcs(poly, nd, arcs);
                spine_project(rx, ry, poly, nd, arcs, total, &v->t, &v->s);
            }
            vid[gy][gx] = m->nv++;
        }
    for (int gy = 0; gy < gh-1; gy++)
        for (int gx = 0; gx < gw-1; gx++) {
            int a = vid[gy][gx], b = vid[gy][gx+1], c = vid[gy+1][gx+1], d = vid[gy+1][gx];
            if (a>=0 && b>=0 && c>=0 && d>=0) {
                m->tri[m->nt][0]=a; m->tri[m->nt][1]=b; m->tri[m->nt][2]=c; m->nt++;
                m->tri[m->nt][0]=a; m->tri[m->nt][1]=c; m->tri[m->nt][2]=d; m->nt++;
            }
        }
    // Bake each triangle's rest winding so draw() can spot inversions.
    for (int t = 0; t < m->nt; t++) {
        const Vtx *A = &m->vtx[m->tri[t][0]], *B = &m->vtx[m->tri[t][1]], *C = &m->vtx[m->tri[t][2]];
        float axr = A->bind.x, ayr = A->bind.y, bxr = B->bind.x, byr = B->bind.y, cxr = C->bind.x, cyr = C->bind.y;
        float cr = (bxr-axr)*(cyr-ayr) - (byr-ayr)*(cxr-axr);
        m->restSign[t] = cr >= 0 ? 1 : -1;
    }
}

static void build(void) {
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){ 0, -10 };
    world = b2CreateWorld(&wd);
    b2BodyDef gd = b2DefaultBodyDef();
    ground = b2CreateBody(world, &gd);
    // A horizon to stand on, plus side walls so a thrown figure stays on screen.
    // High friction: without grippy feet the KEEP_ANGLE torque just spins it.
    {
        float W = SCREEN_W/PPM, TOP = SCREEN_H/PPM, fy = WY(FLOOR_PY);
        b2ShapeDef gs = b2DefaultShapeDef(); gs.material.friction = 1.0f;
        b2Segment floorSeg = {{0,fy},{W,fy}};  b2CreateSegmentShape(ground, &gs, &floorSeg);
        b2Segment lft = {{0,fy},{0,TOP}};      b2CreateSegmentShape(ground, &gs, &lft);
        b2Segment rgt = {{W,fy},{W,TOP}};      b2CreateSegmentShape(ground, &gs, &rgt);
    }
    for (int f = 0; f < NFIG; f++) {
        fig[f].dx    = BUILDS[f].dx;
        fig[f].stiff = BUILDS[f].stiff;
        fig[f].sc    = BUILDS[f].sc;
        fig[f].girth = BUILDS[f].girth;
        make_bones(&fig[f], f);
        for (int i = 0; i < NSKIN; i++) build_mesh(&fig[f], i);
    }
    // The mesh deliberately overshoots the silhouette by one cell (see
    // cell_opaque), so it DOES sample transparent texels — without a colorkey
    // those are opaque black and every limb wears a black halo.
    colorkey(CLR_BLACK);
    dragging = false; dragFig = dragBone = -1;
}
static void reset(void) { b2DestroyWorld(world); build(); }

// ── skinning ───────────────────────────────────────────────────────────
// DQS in 2D: blend each bone's DELTA rotation as a circular mean, apply that
// ONE rotation to the bind vertex, then add the weight-averaged translation
// residual. Blending a single rotation (rather than averaging already-rotated
// positions, which is LBS) is what stops a folded knee collapsing inward.
// The live spine for the skin currently being drawn (rebuilt once per skin per
// frame in draw(), not per vertex). spnN == 0 means "this skin has no chain, or
// we're not in spine mode" and the weighted path below runs instead.
static int   flipped = 0;      // triangles inside-out this frame (the fold-over oracle)
static int   flipPerSkin[8];   // ...broken down by skin, to localise a residual
static int   flipPerFig[NFIG]; // ...and by BUILD, which is how you tell a skinning
                               // bug from a build simply being wider than the
                               // weighted modes can bend (see the girth note below)
static float flipVMin = 1e9f, flipVMax = -1e9f;   // uv-v span of the flipped tris:
                                                  // WHERE along the strip they live
static float spnPoly[SPN_SAMP*2], spnArcs[SPN_SAMP], spnTotal;
static int   spnN = 0;

static void skin_vertex(const Fig *F, const Vtx *v, int *xs, int *ys) {
    if (mode == 3 && spnN > 0) {
        float x, y;
        spine_place(spnPoly, spnN, spnArcs, spnTotal, v->t, v->s, &x, &y);
        *xs = (int)x; *ys = (int)y; return;
    }
    b2Vec2 P[KB];
    for (int k = 0; k < v->nb; k++) P[k] = b2Body_GetWorldPoint(F->bone[v->bone[k]], v->off[k]);

    if (mode == 2) {                                   // RIGID: strongest bone only
        int best = 0;
        for (int k = 1; k < v->nb; k++) if (v->w[k] > v->w[best]) best = k;
        *xs = SX(P[best].x); *ys = SY(P[best].y); return;
    }
    if (mode == 1) {                                   // LBS: average the positions
        float x = 0, y = 0;
        for (int k = 0; k < v->nb; k++) { x += v->w[k]*P[k].x; y += v->w[k]*P[k].y; }
        *xs = SX(x); *ys = SY(y); return;
    }
    float cs = 0, sn = 0;
    for (int k = 0; k < v->nb; k++) {
        float dth = bone_angle(F, v->bone[k]) - v->bindAng[k];
        cs += v->w[k]*cosf(dth); sn += v->w[k]*sinf(dth);
    }
    float th = (cs*cs + sn*sn > 1e-12f) ? atan2f(sn, cs) : 0.0f;
    float c = cosf(th), s = sinf(th);
    float rx = c*v->bind.x - s*v->bind.y;              // rotate the BIND vertex once
    float ry = s*v->bind.x + c*v->bind.y;
    float tx = 0, ty = 0;
    for (int k = 0; k < v->nb; k++) { tx += v->w[k]*(P[k].x - rx); ty += v->w[k]*(P[k].y - ry); }
    *xs = SX(rx + tx); *ys = SY(ry + ty);
}

// ── KEEP_ANGLE — ported from playtime src/keep-angle.lua ───────────────
// A PD controller in OMEGA SPACE: it writes the angular velocity straight onto
// the body instead of applying a torque. That looks like a cheat — it overrides
// the solver, so a keep-angle bone wins every rotational argument it has with
// its joints and the floor — but it is playtime's choice and it MEASURES BEST.
// Head height above the floor (rest 98px), same rig, same frames:
//
//     keep mode        f60     f200    f390
//     off              85.4    83.9    64.3   slowly topples
//     omega (this)     84.9    84.0    84.0   still standing
//     torque           85.2    83.8    32.8   collapsed
//
// The torque form (boxlab's balance(), kept on mode 2) is the more physical
// answer and it loses: it asks politely and gravity out-votes it. Judge this by
// HEAD HEIGHT, never by pelvis tilt — mode 1 writes the pelvis angle directly,
// so it scores a perfect 0 tilt while lying flat on the floor.
//
// playtime's other rule matters as much: switch the behaviour OFF for whatever
// the user is currently dragging (its `hitted` list), or you are fighting the hand.
//
// The target is each bone's REST world angle, captured at build: +90 deg for the
// spine bones (upright) and the authored tilt for the feet (flat on the floor).
#define KEEP_KD    0.000015f    // playtime's value — so small it is nearly pure P
#define KEEP_MAXW  15.0f        // rad/s clamp, also playtime's
static void keep_angle(void) {
    if (keepMode == 0) return;
    for (int f = 0; f < NFIG; f++) {
      Fig *F = &fig[f];
      for (int i = 0; i < NBONE; i++) {
        float kp = BONES[i].keepKp * size_hz(F);
        if (kp <= 0.0f) continue;
        if (dragging && dragFig == f && dragBone == i) continue;   // playtime's `hitted` rule
        float diff = F->rest[i] - bone_angle(F, i);
        diff = fmodf(diff + PI, 2.0f*PI);                  // wrap to [-pi, pi]
        if (diff < 0) diff += 2.0f*PI;
        diff -= PI;
        float w = b2Body_GetAngularVelocity(F->bone[i]);

        if (keepMode == 1) {
            // playtime's literal behaviour: WRITE the angular velocity.
            float cmd = kp * diff - KEEP_KD * w;
            if (cmd >  KEEP_MAXW) cmd =  KEEP_MAXW;
            if (cmd < -KEEP_MAXW) cmd = -KEEP_MAXW;
            b2Body_SetAngularVelocity(F->bone[i], cmd);
        } else {
            // Torque instead (boxlab's balance()). Same PD, but it ASKS the solver
            // rather than overruling it, so joint and contact impulses survive.
            float I  = b2Body_GetRotationalInertia(F->bone[i]);
            float kd = 2.0f * sqrtf(kp);                   // ~critical damping
            b2Body_ApplyTorque(F->bone[i], (kp * diff - kd * w) * I, true);
        }
        b2Body_SetAwake(F->bone[i], true);
      }
    }
}

// ── loop ───────────────────────────────────────────────────────────────
static bool inited = false;
void update(void) {
    if (!inited) { build(); inited = true; }
    if (keyp('R')) reset();
    if (keyp(' ')) mode = (mode + 1) % 4;
    if (keyp('M')) showMesh = !showMesh;
    if (keyp('B')) showBones = !showBones;
    if (keyp('C')) curveClamp = !curveClamp;   // SPINE mode: the concave-side clamp
    if (keyp('G')) keepMode = (keepMode + 1) % 3;   // off / omega / torque
    // F = the EXTREME-POSE test. Dragging with the mouse can't reliably reach a
    // full fold (the mouse joint is force-capped, and the chain would rather
    // swing at the hip than bend the knee), so drive the two joints straight to
    // their limit through their own springs. This is the pose that decides
    // whether the skinning holds — check it after ANY weight/radius change.
    if (keyp('F')) {
        folding = !folding;
        const struct { int b; float deg; } FOLD[2] = { {B_LARM_R, -138}, {B_SHIN_R, -128} };
        for (int f = 0; f < NFIG; f++)
            for (int k = 0; k < 2; k++) {
                b2RevoluteJoint_SetTargetAngle(fig[f].hinge[FOLD[k].b], folding ? FOLD[k].deg*DEG : 0.0f);
                b2RevoluteJoint_SetSpringHertz(fig[f].hinge[FOLD[k].b],
                    folding ? 9.0f * size_hz(&fig[f]) : BONES[FOLD[k].b].hz * size_hz(&fig[f]));
                b2Body_SetAwake(fig[f].bone[FOLD[k].b], true);
            }
    }

    float mwx = WX(mouse_x()), mwy = WY(mouse_y());
    if (mouse_pressed(MOUSE_LEFT)) {                   // grab the nearest bone centre
        int best = -1, bestF = -1; float bd = 0.55f*0.55f;
        for (int f = 0; f < NFIG; f++)
            for (int i = 0; i < NBONE; i++) {
                b2Vec2 c = b2Body_GetPosition(fig[f].bone[i]);
                float d = (mwx-c.x)*(mwx-c.x) + (mwy-c.y)*(mwy-c.y);
                if (d < bd) { bd = d; best = i; bestF = f; }
            }
        if (best >= 0) {
            b2MouseJointDef d = b2DefaultMouseJointDef();
            d.bodyIdA = ground; d.bodyIdB = fig[bestF].bone[best];
            d.target = (b2Vec2){mwx, mwy}; d.hertz = 5.0f; d.dampingRatio = 0.9f;
            // deliberately weak: a strong mouse joint drags the whole figure off its
            // hook instead of articulating the limb you grabbed.
            d.maxForce = 1200.0f * b2Body_GetMass(fig[bestF].bone[best]);
            mjoint = b2CreateMouseJoint(world, &d);
            b2Body_SetAwake(fig[bestF].bone[best], true);
            dragging = true; dragFig = bestF; dragBone = best;
        }
    }
    if (dragging && mouse_down(MOUSE_LEFT)) b2MouseJoint_SetTarget(mjoint, (b2Vec2){mwx, mwy});
    if (mouse_released(MOUSE_LEFT) && dragging) { b2DestroyJoint(mjoint); dragging = false; dragFig = dragBone = -1; }

    keep_angle();                      // steer the upright parts BEFORE the solve
    b2World_Step(world, 1.0f/60.0f, 4);

#ifdef DE_TRACE
    int tv = 0, tt = 0;
    for (int f = 0; f < NFIG; f++)
        for (int i = 0; i < NSKIN; i++) { tv += fig[f].mesh[i].nv; tt += fig[f].mesh[i].nt; }
    watch("verts", "%d", tv); watch("tris", "%d", tt);
    watch("knee", "%.1f", (bone_angle(&fig[2], B_SHIN_R) - bone_angle(&fig[2], B_THIGH_R)) / DEG);
    watch("elbow", "%.1f", (bone_angle(&fig[2], B_LARM_R) - bone_angle(&fig[2], B_UARM_R)) / DEG);
    watch("mode", "%s", MODE_NAME[mode]);
    watch("flipped", "%d", flipped);
    // Both breakdowns list only what actually FLIPPED, because watch()'s value
    // field is 40 chars: naming all six skins overflowed it and the line ended
    // "...armR:0 hea", which reads as a missing skin rather than a clipped
    // string. Nonzero-only fits, and is the half you wanted to read anyway.
    { char b[48]; int n = 0;
      for (int i = 0; i < NSKIN; i++) {
        if (!flipPerSkin[i]) continue;
        if (n > 30) { snprintf(b+n, sizeof(b)-n, " .."); break; }
        n += snprintf(b+n, sizeof(b)-n, "%s%s:%d", n?" ":"", SKINS[i].name, flipPerSkin[i]);
      }
      watch("flipBySkin", "%s", n ? b : "none"); }
    { char b[48]; int n = 0;
      for (int f = 0; f < NFIG; f++) {
        if (!flipPerFig[f]) continue;
        if (n > 30) { snprintf(b+n, sizeof(b)-n, " .."); break; }
        n += snprintf(b+n, sizeof(b)-n, "%s%s:%d", n?" ":"", BUILDS[f].name, flipPerFig[f]);
      }
      watch("flipByFig", "%s", n ? b : "none"); }
    watch("flipV", "%.0f..%.0f", flipped ? flipVMin : 0, flipped ? flipVMax : 0);
    watch("pelvisTilt", "%.1f", (bone_angle(&fig[2], B_PELVIS) - fig[2].rest[B_PELVIS]) / DEG);
    watch("pelvisRest", "%.1f", fig[2].rest[B_PELVIS] / DEG);
    watch("pelvisAng",  "%.1f", bone_angle(&fig[2], B_PELVIS) / DEG);
    watch("keep",       "%s", KEEP_NAME[keepMode]);
    // Height of the head above the floor: the only honest "is it still
    // standing" number. Pelvis TILT is useless for judging keep-angle mode 1,
    // which writes that very angle and so scores a perfect 0 while lying down.
    watch("headUp",     "%.1f", (float)FLOOR_PY - (SCREEN_H - b2Body_GetPosition(fig[2].bone[B_HEAD]).y*PPM));
    // ...and the same number for every build, as a FRACTION of the height that
    // build stands at rest: the builds have different absolute head heights now,
    // so only the ratio says "still standing" for all five in one glance (1.00 =
    // upright, and a keep-angle that can't hold a small light guy shows up here).
    { char b[64]; int n = 0;
      for (int f = 0; f < NFIG; f++) {
        float up = (float)FLOOR_PY - (SCREEN_H - b2Body_GetPosition(fig[f].bone[B_HEAD]).y*PPM);
        n += snprintf(b+n, sizeof(b)-n, "%s%.2f", f?" ":"", up / (95.0f * BUILDS[f].sc));
      }
      watch("standing", "%s", b); }
    watch("chestRel",   "%.1f", (bone_angle(&fig[2], B_CHEST)  - bone_angle(&fig[2], B_PELVIS)) / DEG);
    watch("hipRel",     "%.1f", (bone_angle(&fig[2], B_THIGH_R)- bone_angle(&fig[2], B_PELVIS)) / DEG);
#endif
}

void draw(void) {
    if (!inited) { build(); inited = true; }
    cls(CLR_DARKER_BLUE);
    rectfill(0, FLOOR_PY, SCREEN_W-1, SCREEN_H-1, CLR_DARKER_GREY);   // the ground
    line(0, FLOOR_PY, SCREEN_W-1, FLOOR_PY, CLR_DARK_GREY);           // the horizon
    font(FONT_SMALL);                    // name each build, on the floor below its feet
    for (int f = 0; f < NFIG; f++) {
        int n = (int)strlen(BUILDS[f].name);
        print(BUILDS[f].name, (int)(RIG_CX + BUILDS[f].dx) - n*2, FLOOR_PY + 16, CLR_MEDIUM_GREY);
    }

    flipped = 0;
    for (int k = 0; k < 8; k++) flipPerSkin[k] = 0;
    for (int k = 0; k < NFIG; k++) flipPerFig[k] = 0;
    flipVMin = 1e9f; flipVMax = -1e9f;
    for (int f = 0; f < NFIG; f++) {
      Fig *F = &fig[f];
      for (int i = 0; i < NSKIN; i++) {
        Mesh *m = &F->mesh[i];
        spnN = 0;
        if (mode == 3 && SKINS[i].nchain > 0) {          // rebuild this limb's spine
            float pts[8];
            int np = spine_chain(F, &SKINS[i], true, pts);
            spnN = spine_sample(pts, np, spnPoly);
            spnTotal = spine_arcs(spnPoly, spnN, spnArcs);
        }
        for (int t = 0; t < m->nt; t++) {
            const Vtx *a = &m->vtx[m->tri[t][0]], *b = &m->vtx[m->tri[t][1]], *c = &m->vtx[m->tri[t][2]];
            int ax, ay, bx, by, cx, cy;
            skin_vertex(F, a, &ax, &ay); skin_vertex(F, b, &bx, &by); skin_vertex(F, c, &cx, &cy);
            // screen y is DOWN, world y is UP, so a screen cross of one sign
            // corresponds to the opposite rest sign — hence the negation.
            float cr = (float)(bx-ax)*(cy-ay) - (float)(by-ay)*(cx-ax);
            if ((cr >= 0 ? -1 : 1) != m->restSign[t]) { flipped++; flipPerSkin[i]++; flipPerFig[f]++;
                float vs[3] = { a->uvy, b->uvy, c->uvy };
                for (int q = 0; q < 3; q++) { if (vs[q] < flipVMin) flipVMin = vs[q];
                                              if (vs[q] > flipVMax) flipVMax = vs[q]; } }
            tritex(ax, ay, a->uvx, a->uvy, bx, by, b->uvx, b->uvy, cx, cy, c->uvx, c->uvy);
        }
        if (showMesh)
            for (int t = 0; t < m->nt; t++) {
                const Vtx *a = &m->vtx[m->tri[t][0]], *b = &m->vtx[m->tri[t][1]], *c = &m->vtx[m->tri[t][2]];
                int ax, ay, bx, by, cx, cy;
                skin_vertex(F, a, &ax, &ay); skin_vertex(F, b, &bx, &by); skin_vertex(F, c, &cx, &cy);
                line(ax,ay,bx,by,CLR_DARK_GREY); line(bx,by,cx,cy,CLR_DARK_GREY); line(cx,cy,ax,ay,CLR_DARK_GREY);
            }
      }
    }

    if (showBones)
      for (int f = 0; f < NFIG; f++)
        for (int i = 0; i < NBONE; i++) {
            b2Vec2 A = b2Body_GetWorldPoint(fig[f].bone[i], fig[f].locA[i]);
            b2Vec2 B = b2Body_GetWorldPoint(fig[f].bone[i], fig[f].locB[i]);
            line(SX(A.x), SY(A.y), SX(B.x), SY(B.y), CLR_RED);
            circfill(SX(A.x), SY(A.y), 1, CLR_YELLOW);
        }

    font(FONT_SMALL);
    print(MODE_NAME[mode], 4, 4, mode == 0 ? CLR_WHITE : CLR_YELLOW);
    print(mode == 0 ? "weighted bones, rotation-blended"
        : mode == 1 ? "linear blend — a folded knee pinches"
        : mode == 2 ? "rigid — one bone per vertex, seams tear"
        : curveClamp ? "spine (t,s) on a Bezier + curvature clamp"
                    : "spine (t,s) on a Bezier, clamp OFF", 30, 4, CLR_LIGHT_GREY);
    // The key list has to FIT: at FONT_SMALL's 5px advance the screen holds 63
    // characters, and the old single line was 73 — it ran off the edge reading
    // "...B bone", so R reset was never visible to anyone (ui-audit's catch).
    // The keep-angle STATE moves up to the mode line, where there is room.
    char hud[48];
    snprintf(hud, sizeof hud, "keep:%s", KEEP_NAME[keepMode]);
    print(hud, SCREEN_W - 4 - (int)strlen(hud)*5, 4, CLR_LIGHT_GREY);
    print("drag SPACE mode G keep F fold C clamp M mesh B bones R reset",
          4, SCREEN_H-10, CLR_LIGHT_GREY);   // 60 chars = 304px, inside 320
}
