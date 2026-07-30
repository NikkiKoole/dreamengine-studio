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
    "summary": "FIVE DIFFERENT PEOPLE built from one rig table and one sprite sheet: a small kid with a big head and short limbs, a barrel with a long thick torso on short legs, the rig exactly as authored, a lanky one with long legs and a small head, and a broad bruiser. Nothing about them is hand-placed. Each build states a stiffness multiplier for its joint springs (the floppiest sags, the stiffest stands rigid), an overall size, an overall thickness, how long and how thick the bones of each GROUP are, and a short list of joints turned away from the authored rest to give it an ATTITUDE: the kid cocks its head and stands with its legs apart, the barrel leans back with its arms off its belly and its feet planted wide, the lanky one stoops with its arms tucked and its knees together, the bruiser cannot put its arms down. Only the middle figure is left square-on, as the control. Press H and the whole line goes over into handstands, one after another, as a wave rolling down it: the arms come up, the body turns over about its hips, the legs open into a straddle and the palms turn down to meet the floor. Each build goes at its own speed, off the same stiffness number that gives it its temperament, so the bruiser snaps over while the kid takes half again as long. They hold it on their hands with their palms planted, heads hanging between their arms, legs straddled just wide enough to fit under the top of the screen. The rig then walks its own parent chain to work out where every bone and every sprite pixel ends up. Lengthen a thigh and the knee, the ankle, the foot and the whole leg strip follow; tilt a chest and the shoulders, both arms, the head and their skins swing with it. Each limb is a single continuous sprite strip stretched over several Box2D bones. The right leg is one 20x62 texture that bends at the hip, the knee and the ankle; the left leg is the SAME texture mirrored, and all five guys share that one sheet. Each figure gets its own Box2D collision group, so a guy ignores his own limbs but bumps into his neighbours. Press F to fold a limb to its limit, then SPACE to A/B four ways of deforming that texture: a Bezier SPINE, dual-quaternion and linear weighted blends, and rigid one-bone-per-vertex. Every limb is BANDED across its axis on purpose: a flat colour tells you nothing about how a skin deformed, but a regular stripe fans out on the outside of a bend, bunches on the inside, and shears visibly the moment a blend mode gets it wrong.",
    "detail": "Four deformation modes over one rig, because the interesting question is not how to attach a texture to a bone but what happens at a hard bend. SPINE is playtime's MESHUSERT spine-bind ported: each vertex is stored as (t, s) — arc-length fraction along a Bezier drawn through the limb's joint chain, plus signed perpendicular offset — and replanted each frame on the live curve, with no weights and nothing to tune. On top of it sits a CURVATURE CLAMP. playtime's own miter clamp (written for its ribbon path, never wired into spine-bind) was tried first and measured worse than no clamp: it is the right formula for a polyline corner, but a Bezier spine has no such corner, so min(segA,segB) only measures sample spacing and collapses exactly at a bend. The correct bound is the local radius of curvature - an offset curve folds on the concave side once the offset exceeds R - and clamping to 0.92R takes the fold artifact to zero at every angle tested. The cart carries its own oracle for this — every triangle's winding is baked at bind time and compared each frame, so 'inverted triangles' is a number, not an opinion. At a full elbow-and-knee fold: SPINE+clamp 4, SPINE without clamp 7, DQS 19, LBS 19, RIGID 43, out of 880. The structural move over puppet.c: BONES and SKINS are separate lists. 15 bones are invisible Box2D boxes joined by sprung, angle-limited revolutes (pelvis, chest, head, and upper/lower/extremity for each of four limbs). 6 skins are sprite rects from the sheet, each naming the subset of bones that deforms it. Per skin: a grid over the rect's opaque pixels becomes a triangle mesh; each vertex is weighted by 1-smoothstep(0, radius, distanceToBoneSegment), pruned to the best 3 bones and renormalised; bind pose stores the vertex in each bone's LOCAL frame plus that bone's rest angle. Each frame the vertex is placed by blending the bones' DELTA rotations as a circular mean, rotating the bind vertex once by that blend, then adding the weight-averaged translation residual, and tritex draws the triangles with UVs that never move. Left and right limbs share one sprite rect: the geometry mirrors, the UVs do not. The sprite sheet is drawn as a deformation TEST PATTERN — breton torso, banded sleeves and shorts, striped socks, cross-banded shoes — so the eye can read stretch and shear directly instead of guessing from a silhouette. The per-bone blend radius is what keeps a foot rigid while a thigh blends over 14 pixels. BUILDS, and why the rig is now WALKED: an affine transform of the table can make a figure bigger or thicker, and that is all it can do. Real proportions cannot come from it, because BONES[] stores absolute positions, so lengthening a thigh would leave the knee, the ankle, the foot and the leg skin's rest placement exactly where they were. So the table became the REFERENCE pose and each build derives its own rig from it, parent first: a bone keeps its authored direction, takes its group's length, and starts at the same spot on its parent's DERIVED segment, which is what makes a longer chest carry the shoulder up with it and a wider pelvis spread both legs. Two primitives do all of it, and they are the spine bind's decomposition applied per bone instead of per chain: a point maps to (fraction along a bone, signed perpendicular offset), and that pair maps back onto the build's version of that bone with the offset scaled by its thickness. The skins ride the same remap, weight-blended by the smoothstep the skinning already uses so seams between bones stay smooth, which is the substitution that turns a scaled copy into a different build: the leg strip's rest position becomes a function of where the shin ENDED UP. Doing it at BIND and not at draw keeps it free, since the mesh is born the shape this figure is and no per-frame code knows the difference. Two invariants guard it. Round-tripping through both primitives is the identity, so the unproportioned 'author' build must render exactly as the pre-derivation cart did: it does, bit for bit, once the derived rest pose is snapped to a 1/256 px grid (without the snap a vertex authored exactly on an integer came back a hair either side of it and moved a shared triangle edge by a pixel). And the winding oracle says the shapes are honest: a 300-frame fold peaks at 15 inverted triangles of 22000 against 12 for five plain scaled copies, with the weighted modes slightly BETTER than before. The per-build flip counter earns its keep here by showing the two mode families fail for opposite reasons: the weighted blends are wrecked by deep poses (the floppiest build owns 60% of their inversions, the thickest is nearly clean) while SPINE is indifferent to pose and vulnerable to THICKNESS, since an offset curve folds once the offset passes the local radius of curvature. A build's POSE rides the same walk (a rotation folded into the chain, accumulating down it the way a real skeleton does) and becomes that build's rest: the joint limits are measured from it and KEEP_ANGLE defends it, so a stoop stays a stoop under gravity rather than being a starting condition that decays. The bodies still rest at rotation identity with every reference angle at zero, because the pose goes into the SHAPE exactly as the authored direction always did, which keeps the joint branch-cut trap shut. Shape and attitude both feed back into the physics, which is the honest half of doing it this way rather than drawing five sprites: the big-headed kid is top-heavy and needs roughly twice the joint stiffness of the old floppiest build to stay on its feet (measured 0.41 of its rest height at stiff 0.35, 0.82 at 0.65), and the PLANE the rig is drawn in bounds what a pose can even say. The rig is a FRONT view, both limbs of a pair being separate bones either side of the centre line, so an angle here swings a limb OUT rather than forward, which is what makes a STANCE expressible at all. The feet are the exception, and it cost a measurement to find: in this plane a foot angle rolls the sole onto its edge, and a figure balancing on the edge of its shoe sags (0.14 of its standing height on the smallest build, next to which the head angle first blamed for it measured nothing). So a foot alone does not inherit the chain's rotation, which is both the fix and the anatomy: you splay your legs and your feet stay flat because the ankle takes up the difference. A stance is therefore one number per build rather than an angle plus a counter-rotation nobody remembers, and it cannot be got wrong by forgetting the second half. The IDLE fidget is the same ask one layer further out, and it exposed something about the rig worth writing down: two different authorities own a bone's angle here. Bones with no keep-angle gain (chest, arms) are steered by their hinge SPRING, so setting the joint's target makes the spring chase it and the build's temperament shows up for free as lag and undershoot. The bones that hold the figure up (head, thighs, shins, feet) are steered by the KEEP_ANGLE controller, which writes angular velocity outright and would simply erase a joint target, so for those the fidget is added to the angle it DEFENDS instead. One ask, accumulated down the chain exactly like the pose, feeding whichever authority owns each bone. It costs nothing measurable: the fold oracle reads 21 inverted triangles of 22000 with five bodies breathing and 20 with them frozen. The HANDSTAND is where that two-authority structure gets tested properly, and the first attempt failed in a way worth keeping: leave the bodies alone, add 180 degrees to every angle the controller defends, and let physics work out how. It does not work. Four of the five rolled onto their backs with their limbs waving and the stiffest simply stayed standing, because an orientation controller owns every bone's angle and not one bone's height, so 'be upside down' is satisfied just as well by lying down. The fix is the division of labour that makes them stand at all: nothing in this cart ever made a figure stand up, it was PLACED standing and the controller kept it there. So the flip is a placement too, and it is point reflections with no trigonometry in it, reflect each arm chain through its own shoulder to put the arms overhead, then reflect everything through the pelvis to turn the figure over, then re-anchor onto the sole line so the hands come down where the feet stood. Composing those two is why the arms end up pointing at the floor again: they were hanging, and now they are holding. The controller's targets fall out of the same composition, and the keep gains take the FIRMER of each limb's own job and its counterpart's, because standing on your arms means the arms are legs now and the chest inherits the pelvis's job of holding the line, which is the change that stopped two builds jackknifing. The straddle is not a constant but the smallest angle each build can open its legs to and still fit under the top of the screen, because arms overhead make an inverted figure taller than a standing one, and wide enough to hold straight was also wide enough for the crowd to shove itself 52 px sideways. The lanky build solves for 23 degrees and everyone else for 8. The last question the handstand answered is the one it invites: the arms visibly bend under the load, so are they too weak? No, and the A/B is the most surprising measurement here. Three ways of firming them up, judged by how far the torso ends up off vertical after 500 frames: as authored 0.5 degrees, with knee-stiff springs 34, with a leg's bulk 2.6, with all of it 53. Stiffening the arms straightens the elbow and RUINS the handstand, because a rigid strut fights the chest's own balance controller instead of yielding to it and the compromise tips the whole body over. What helped was not strength but CONTACT: the figure has feet and no hands, a foot being a wide flat sole and a hand the tip of a thin vertical bone, so the wrist now turns the palm flat against the floor the way anybody planting their hands would, which halves the arm bend for two degrees of lean. A little bend in the arms is not weakness, it is the compliance the balance needs, and the number that says whether a handstand is any good is the TORSO's angle, never the elbow's. The CARTWHEEL is what those five derivation steps turn out to have contained all along. The flip was instantaneous, a gymnast placed rather than one going over, and what it needed was not new machinery but a parameter: run the same five steps at overlapping rates and the arms come up over the first 45 per cent, the body turns over about the hips from 15 per cent to the end, the legs open into the straddle from 35, and the palms turn down over the last 30, which is those steps in the order a body actually does them. The rig can only do this at all because of the plane it lives in, the third time that plane has decided something here. A handstand ENTRY is sagittal, bend at the hips, plant the hands ahead of you, kick the legs up, and this rig cannot express a single frame of it. A cartwheel is coronal, a rotation in exactly the plane the rig is drawn in, and a cartwheel is straddled, which is the shape the ceiling solve had already arrived at for a completely unrelated reason. Two things keep it honest. The ENDPOINTS are untouched: every sub-phase returns exactly 0 at the start and exactly 1 at the end, and at the end each rotation takes the exact point-reflection path rather than a cosine of pi (sinf(PI) is 8.7e-8, and a rig that lands a hair off the reflection makes every settled number un-A/B-able), so the pose the crowd holds is the pose it always held: head-over-hip identical on all five builds, torso within 0.1 degrees, standing fraction identical. And mid-turnover a figure is PLACED, by the same authority that stood it up at build, with its keep-angle controller switched off for the duration, because an orientation controller writing angular velocity into a body whose transform is about to be overwritten is not steering anything, it is seeding the next contact solve with spin. The turnover costs nothing measurable: the fold oracle peaks at 23 inverted triangles of 22000 across the flip against 25 for the instant one, and reads 0 at mid-flight where every limb is straight. Two numbers per figure do the rest. SPEED comes off the same stiffness multiplier that gives each build its temperament everywhere else, as a square root, because that is the law a pendulum's rate follows and a turnover about the hips is one: the bruiser goes over in 32 frames and the kid in 57 without either being told to. The STAGGER is a fix that looks like a flourish. A figure halfway over is lying horizontal, a horizontal figure is wider than the 62 px between neighbours, and a crowd that all went at once would sweep through each other; offsetting the starts means no two are horizontal at the same moment, and what the fix looks like is a wave. Springs deliberately do NOT scale with size; the source carries the measurement that says the correction is inert across this range.",
    "controls": "Drag any bone with the mouse to pose the figure. F drives the right elbow and knee to a full fold (the extreme-pose test - a mouse drag cannot reliably reach it, the chain would rather swing at the hip). H sends the whole crowd over into handstands and, pressed again, stands them back up. They go one after another, each at its own speed, and it composes with everything: fold a knee while inverted and you get a tucked handstand. I stops the idle fidget, which is also how you measure a still pose. SPACE cycles SPINE / DQS / LBS / RIGID. C toggles spine mode's curvature clamp. M shows the mesh wireframe. B shows the bone skeleton. R resets."
  },
  "todo": [
    "Proportions are stated per GROUP (torso, head, arm, leg), because 15 numbers per build would be the second rig table this cart refuses to have. Per-BONE would buy a pot belly over a normal chest, or one arm longer than the other, and the machinery already takes it: bg[] and the derived segments are per bone, only the authoring is grouped. What that needs is a way to state it without a wall of numbers, which is a design question rather than a code one.",
    "Poses are authored in degrees per joint, which is honest but blunt: what a build actually wants to say is 'stand like this', and the same attitude on a 0.72x kid and a 1.08x bruiser does not read the same. A named pose vocabulary (slouch, swagger, at-ease) resolved against the build would be the next step, and it is the same question the per-bone proportion rung asks: how do you state this without a wall of numbers.",
    "The turnover's hip ARC is the one stated number in the whole flip. A placed figure has no ballistics to integrate, so the push off the floor is 12 authored px of lift shaped by a sine, not a trajectory anybody solved; take it out and the turnover reads as a body rotating about a nail driven through its pelvis. The honest version is the same centre-of-mass controller the fidget rung wants, and it would buy the thing this cannot do: a flip that MISSES.",
    "A cartwheel in place is a turnover, not a cartwheel. A real one travels, hand-hand-foot-foot, and lands a body-width sideways; this one comes back down where it left. Travel is not another rest pose, it is a contact policy (which hand is down, when it is allowed to leave, what carries the weight in between), which is the same locomotion question the fidget rung asks and the reason both are parked behind the same controller.",
    "Mid-turnover a figure is PLACED, so it can sweep through a neighbour: the stagger stops any two being horizontal at the same moment, but adjacent phases still overlap in space, and a placed body shoves a simulated one. Measured at up to 7 px of drift during the wave, against the 25 px the held handstand slides on its own, so it is currently the smaller of the two effects. The real fix is a contact policy for a body that is being placed rather than wider spacing.",
    "The idle fidget is three hand-authored sinusoids per build, which is enough to stop them reading as props but is not a MOTION model: nothing in it knows where the figure's weight actually is, so it cannot catch a stumble or lean into a shove. The honest version reads the centre of mass and steers the ask from it, which is the same controller a walk cycle would need and is where this cart would turn into a locomotion probe.",
    "The fidget deliberately does NOT touch proportion(), because re-deriving the rig means rebinding the meshes and binding once is the whole reason the shapes are free. That does mean a build can never breathe with its RIBS (a girth oscillation) the way it can with its joints. A per-frame thickness on the spine bind's s offset would get there for the chain skins without a rebind, and would go nowhere for the torso, which has no chain.",
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

// This table is the REFERENCE pose, not any figure on screen: every build
// derives its own rig from it (proportion()), and nothing outside that function
// reads these coordinates directly. Parents MUST come before their children —
// the chain walk depends on it, and so does bone_group()'s range test.
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
    {"armL",  56, 0, 14, 50, 138, 43, true,  {B_CHEST, B_UARM_L, B_LARM_L, B_HAND_L}, 4,
                                             {B_UARM_L, B_LARM_L, B_HAND_L}, 3},
    {"legL",  76, 0, 20, 62, 141, 69, true,  {B_PELVIS, B_THIGH_L, B_SHIN_L, B_FOOT_L}, 4,
                                             {B_THIGH_L, B_SHIN_L, B_FOOT_L}, 3},
    {"legR",  76, 0, 20, 62, 160, 69, false, {B_PELVIS, B_THIGH_R, B_SHIN_R, B_FOOT_R}, 4,
                                             {B_THIGH_R, B_SHIN_R, B_FOOT_R}, 3},
    {"torso", 24, 0, 28, 41, 146, 40, false, {B_PELVIS, B_CHEST, B_HEAD}, 3,  {0}, 0},
    {"armR",  56, 0, 14, 50, 169, 43, false, {B_CHEST, B_UARM_R, B_LARM_R, B_HAND_R}, 4,
                                             {B_UARM_R, B_LARM_R, B_HAND_R}, 3},
    {"head",   0, 0, 20, 20, 150, 20, false, {B_HEAD, B_CHEST}, 2,            {0}, 0},
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
    // This build's rig, DERIVED from the authored table by proportion() below:
    // each bone's rest segment in authored space, and its thickness multiplier.
    // Everything (bones, joints, skin rest placement) reads these, never BONES[]
    // coordinates directly — BONES[] is the reference pose, not any figure.
    float     pax[NBONE], pay[NBONE], pbx[NBONE], pby[NBONE];
    float     bg[NBONE];
    float     pang[NBONE];                // accumulated rest rotation down the chain
    float     idleW[NBONE];               // this frame's fidget, as a WORLD angle offset
    // The FLIP rig: this build's derived pose, turned over as far as fu says.
    // Not two rest poses with a snap between them but ONE rest pose parameterised
    // by how far through the turnover it is, which is what makes a cartwheel a
    // trajectory rather than a second pose (derive_flip below).
    float     hax[NBONE], hay[NBONE], hbx[NBONE], hby[NBONE];
    float     hang[NBONE];                // ...and the world angle each bone turns by
    float     hstraddle;                  // the straddle it solved for, degrees
    float     hshift;                      // step 5's sole-line re-anchor at fu = 1, px:
                                           // solved once, then ridden by the turnover
    float     fu;                          // HOW FAR OVER: 0 standing, 1 holding a
                                           // handstand, anything between = mid-flip and
                                           // this figure is being PLACED, not simulated
    float     restUp;                     // head height above the floor at rest, px.
                                          // Captured per build because proportions
                                          // change it: "is he still standing" can
                                          // only be read as a fraction of his OWN
                                          // height, never against a shared number.
} Fig;
static Fig fig[NFIG];

// Bones come in four GROUPS, and a build's proportions are stated per group
// rather than per bone: 15 numbers per figure would be a second rig table, which
// is the thing this cart refuses to have. The enum order below is what makes the
// lookup a range test — parents first, then the six arm bones, then the six leg
// bones (the same ordering proportion()'s chain walk depends on).
enum { G_TORSO, G_HEAD, G_ARM, G_LEG, NGROUP };
static int bone_group(int b) {
    if (b == B_HEAD)    return G_HEAD;
    if (b <= B_CHEST)   return G_TORSO;    // pelvis, chest
    if (b <= B_HAND_L)  return G_ARM;      // upper/lower/hand, both sides
    return G_LEG;                          // thigh/shin/foot, both sides
}

// Five BUILDS from one rig. Four knobs, no second rig table:
//   stiff  — multiplier on the joint springs and KEEP_ANGLE gains (temperament)
//   sc     — uniform SIZE, applied last, about the authored sole line
//   girth  — overall thickness, multiplying every bone's group thickness
//   len[] / grth[] — per-GROUP PROPORTIONS: how long and how thick the bones of
//            each group are relative to the authored rig. This is the knob that
//            an affine transform of the table could never give: lengthening a
//            thigh has to carry the knee, the ankle, the foot and the leg skin's
//            whole rest placement with it, which is why the rig is now WALKED
//            (proportion()) instead of read off the table.
//   pose[]  — the ATTITUDE: a sparse list of joints turned away from the authored
//            rest, in degrees. Sparse and per-joint because that is what a pose
//            is — a slouch is two joints, not fifteen. It rides the same walk as
//            everything else (a rotation folded into the chain), so a tilted
//            chest carries the shoulders, the arms, the head and their skins with
//            it, and it becomes this build's REST: joint limits are measured from
//            it and KEEP_ANGLE defends it, which is why a build can stand with
//            its feet turned out and keep them turned out.
//            SIGN: positive turns a bone that points DOWN toward -x (screen left),
//            so an arm swings OUT with a negative angle on the right side of the
//            body and a positive one on the left.
//            The rig is a FRONT view — both arms and both legs are separate bones
//            either side of the centre line — so an angle here is a limb swinging
//            OUT or IN, not forward. That is why a STANCE is expressible at all:
//            turn the thighs apart and the figure stands with its legs wide.
//            The exception is the two FEET, and it cost a measurement to find. In
//            this plane a foot angle rolls the sole onto its edge, and a figure
//            balancing on the edge of its shoe sags: 0.14 of its standing height
//            on the smallest build, next to which the head angle I first blamed
//            measured exactly nothing. So a foot does not INHERIT the chain's
//            rotation the way every other bone does (proportion() cuts it), which
//            is also the anatomy: you splay your legs and your feet stay flat,
//            because the ankle takes up the difference. A stance is therefore ONE
//            number per build, and the sole cannot be tilted by forgetting to
//            counter-rotate it.
typedef struct { int bone; float deg; } PoseKey;
typedef struct {
    const char *name;
    float dx, stiff, sc, girth;
    float len[NGROUP], grth[NGROUP];      // { torso, head, arm, leg }
    PoseKey pose[6]; int npose;
} BuildDef;
static const BuildDef BUILDS[NFIG] = {
    //   name        dx   stiff     sc   girth  len{tor  head  arm   leg}  girth{tor head  arm   leg}
    { "kid",       -114, 0.65f, 0.72f, 0.95f, {0.95f,1.15f,0.85f,0.82f}, {1.00f,1.25f,0.95f,1.00f},
        { {B_HEAD,-8}, {B_UARM_R,-9}, {B_UARM_L,9},
          {B_THIGH_R,-6}, {B_THIGH_L,6} }, 5 },                      // head cocked, arms and legs out
    { "barrel",     -57, 0.85f, 0.86f, 1.28f, {1.08f,0.95f,0.95f,0.85f}, {1.10f,0.80f,0.95f,1.00f},
        { {B_CHEST,4}, {B_UARM_R,-13}, {B_UARM_L,13},
          {B_THIGH_R,-8}, {B_THIGH_L,8} }, 5 },                      // leaning back, arms off the belly, planted wide
    { "author",       0, 1.00f, 1.00f, 1.00f, {1.00f,1.00f,1.00f,1.00f}, {1.00f,1.00f,1.00f,1.00f},
        { {0,0} }, 0 },                                              // the reference: NO pose, by definition
    { "lanky",       57, 1.45f, 1.02f, 0.80f, {1.00f,0.92f,1.15f,1.22f}, {1.00f,1.05f,0.90f,0.90f},
        { {B_CHEST,-7}, {B_HEAD,9}, {B_UARM_R,5}, {B_UARM_L,-5},
          {B_THIGH_R,2}, {B_THIGH_L,-2} }, 6 },                      // stooped, arms tucked, knees together
    { "bruiser",    114, 2.10f, 1.08f, 1.18f, {1.05f,0.95f,1.10f,0.95f}, {1.15f,0.85f,1.10f,1.05f},
        { {B_UARM_R,-17}, {B_UARM_L,17}, {B_CHEST,-3},
          {B_THIGH_R,-7}, {B_THIGH_L,7} }, 5 },                      // arms out, chest up, stance wide
    // kid: short limbs under a big head (a child's proportions, not a small
    // adult). barrel: long thick torso on short legs. lanky: long legs and arms,
    // small head, thin everywhere. bruiser: heavy torso and arms, short legs.
};
static float pose_deg(const BuildDef *B, int b) {
    for (int i = 0; i < B->npose; i++) if (B->pose[i].bone == b) return B->pose[i].deg;
    return 0.0f;
}

// The authored rig's centre line and SOLE line. Size scales about the sole so a
// small guy still stands ON the floor rather than hovering or sinking, and about
// the centre line so he stays over his own feet. This transform is ISOTROPIC:
// thickness is no longer a squash in x (it was, for one commit) but a real
// offset from each bone's own axis, so it survives a limb pointing sideways.
#define RIG_CX   160.0f
#define RIG_BY   131.0f
static inline float figx(const Fig *F, float x) { return RIG_CX + F->dx + (x - RIG_CX) * F->sc; }
static inline float figy(const Fig *F, float y) { return RIG_BY + (y - RIG_BY) * F->sc; }
// This build's rest segment for bone b, in screen px. EVERY consumer goes
// through these four — the derived rig, then the size transform.
static inline float bone_ax(const Fig *F, int b) { return figx(F, F->pax[b]); }
static inline float bone_ay(const Fig *F, int b) { return figy(F, F->pay[b]); }
static inline float bone_bx(const Fig *F, int b) { return figx(F, F->pbx[b]); }
static inline float bone_by(const Fig *F, int b) { return figy(F, F->pby[b]); }

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
                        // (summed over the run) now that the five are proportioned
                        // AND posed:
                        //                 peak    kid barrel author lanky bruiser
                        //   SPINE+clamp     18     439   732     98   430     702
                        //   DQS             45    3739  2060    205   388     663
                        //   LBS             45    3739  2060    205   388     663
                        //   RIGID          122    8600  6851   3063  2092       0
                        // Three things fall out of that split. DQS and LBS are
                        // byte identical, as they were before the crowd grew: the
                        // triangles that invert are ones both blends get wrong.
                        // Second, the two FAMILIES fail for opposite reasons. The
                        // weighted modes are wrecked by POSE — the floppiest build
                        // owns 60% of their flips while the thickest is nearly
                        // clean — because a flat weighted sheet has nowhere to put
                        // the inside of a hard bend. SPINE inverts that ranking:
                        // it doesn't care how deep the pose goes, but thickness is
                        // exactly its weak spot, since an offset curve folds once
                        // the offset passes the local radius of curvature, so the
                        // two thick builds contribute 2213 of its 2972 and the
                        // small one almost none. That asymmetry is the argument for
                        // keeping both modes in the cart: neither is strictly the
                        // better skin, they are wrong about different things.
                        // Third, and the reason the table is here: shapes and
                        // attitudes cost surprisingly little. Five plain scaled
                        // copies peaked at 12; proportioning them took it to 15,
                        // posing them to 18, the idle fidget to 21, and then giving
                        // four of them a STANCE took it back DOWN to 18 while the
                        // weighted modes fell from 52 to 45. Legs turned apart at
                        // the hip have more room to fold into, so the pose that
                        // reads as the most deliberate is also the kindest to the
                        // skin. The fidget is free too: holding I to switch it off
                        // measured 20 against 21 before the stance went in.
                        // The one
                        // build knob that DOES move this number is thickness on a
                        // limb that is already bent at rest: the bruiser's arms at
                        // 1.42x measured 23 peak and 1584 of its own, at 1.30x it
                        // is 18 and 1041, which is why the table ships the thinner
                        // one — same silhouette, a third fewer inversions.
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
// Distance from a rest-pose skin pixel to bone b's segment, both in THIS build's
// coordinates, so weights measure the guy who exists and not the table's guy.
static inline float bone_dist(const Fig *F, int b, float rx, float ry) {
    return dist_seg(rx, ry, bone_ax(F,b), bone_ay(F,b), bone_bx(F,b), bone_by(F,b));
}

// ── proportions: the rig as a CHAIN, not a list of coordinates ──────────
// The affine transform above can make a figure bigger or thicker, but it can
// never make its legs longer, because BONES[] stores absolute positions:
// lengthening a thigh leaves the knee, the ankle, the foot and the leg skin's
// rest placement exactly where they were. So the build's rig is DERIVED. Two
// primitives do all of it, and they are the same decomposition the spine bind
// uses, applied per bone instead of per chain:
//
//   bone_uv   a point -> (u, d) against a bone's AUTHORED segment: u is the
//             fraction along it (deliberately NOT clamped, so a shoe hanging
//             past the ankle keeps its overhang), d the signed perpendicular
//             offset in px, positive toward the left normal.
//   bone_place  (u, d) -> a point on that bone's PROPORTIONED segment, with d
//             scaled by the bone's thickness.
//
// Round-tripping a point through both is the identity when nothing is
// proportioned, which is the invariant that keeps the "author" build a pixel-
// exact copy of the rig as drawn (verified by diffing its render).
static void bone_uv(int b, float px, float py, float *u, float *d) {
    const BoneDef *k = &BONES[b];
    float dx = k->bx - k->ax, dy = k->by - k->ay, l2 = dx*dx + dy*dy;
    if (l2 < 1e-9f) { *u = 0; *d = 0; return; }
    float l = sqrtf(l2);
    *u = ((px - k->ax)*dx + (py - k->ay)*dy) / l2;
    *d = ((px - k->ax)*(-dy) + (py - k->ay)*dx) / l;
}
static void bone_place(const Fig *F, int b, float u, float d, float *ox, float *oy) {
    float ax = F->pax[b], ay = F->pay[b];
    float dx = F->pbx[b] - ax, dy = F->pby[b] - ay, l = sqrtf(dx*dx + dy*dy);
    if (l < 1e-9f) { *ox = ax; *oy = ay; return; }
    float g = d * F->bg[b];
    *ox = ax + u*dx + g * (-dy/l);
    *oy = ay + u*dy + g * ( dx/l);
}

// Walk the rig parent-first, building this figure's rest segments. A bone takes
// its group's LENGTH, its own pose ANGLE plus every angle its ancestors turned
// (so the pose accumulates down the chain, the way a real skeleton does), and it
// starts at the same (u,d) spot on its parent's DERIVED segment. That last part
// is what makes the chain a chain: a longer chest carries the shoulder up with
// it, a thicker chest carries it outward, a tilted chest swings it round, and
// both legs spread when the pelvis widens.
//
// The bodies still rest at rotation IDENTITY with every joint's reference angle
// at 0 — the pose goes into the SHAPE, exactly as the authored direction always
// did, so the branch-cut trap in make_bones stays shut and "joint angle 0" still
// means "this build's rest pose".
static void proportion(Fig *F, const BuildDef *B) {
    for (int i = 0; i < NBONE; i++) {              // parents first: the enum guarantees it
        const BoneDef *d = &BONES[i];
        int g = bone_group(i);
        F->bg[i] = B->grth[g] * B->girth;
        F->pang[i] = (d->parent >= 0 ? F->pang[d->parent] : 0.0f) + pose_deg(B, i)*DEG;
        // ...except a FOOT, which never inherits: the sole stays flat on the floor
        // whatever the leg above it is doing (the ankle absorbs it, exactly as a
        // real one does). This is what lets a stance be a single thigh angle
        // instead of a thigh angle plus a counter-rotation nobody remembers.
        if (i == B_FOOT_R || i == B_FOOT_L) F->pang[i] = pose_deg(B, i)*DEG;
        float sx = d->ax, sy = d->ay;
        if (d->parent >= 0) {
            float u, off;
            bone_uv(d->parent, d->ax, d->ay, &u, &off);
            bone_place(F, d->parent, u, off, &sx, &sy);
        }
        // Scaling a bone's LENGTH is just scaling its authored delta, and posing
        // it is rotating that same delta. Normalising to a unit direction and
        // multiplying the length back in says the same thing and rounds twice
        // doing it, which showed up as a pixel or two of drift on the
        // unproportioned build. An unposed bone takes cos 1 / sin 0 and so comes
        // through this exactly, which is what keeps "author" bit-exact.
        float dx = (d->bx - d->ax) * B->len[g], dy = (d->by - d->ay) * B->len[g];
        float c = cosf(F->pang[i]), s = sinf(F->pang[i]);
        F->pax[i] = sx; F->pay[i] = sy;
        F->pbx[i] = sx + dx*c - dy*s;
        F->pby[i] = sy + dx*s + dy*c;
    }
    // Re-anchor the SOLE. Longer legs grow DOWNWARD from the pelvis, so without
    // this a long-legged build spawns with its feet through the floor and gets
    // shoved out on frame 1. Shift the derived rig so its lowest bone point sits
    // where the authored rig's does; the size transform about RIG_BY does the rest.
    float lowRef = -1e9f, lowThis = -1e9f;
    for (int i = 0; i < NBONE; i++) {
        if (BONES[i].ay > lowRef)  lowRef  = BONES[i].ay;
        if (BONES[i].by > lowRef)  lowRef  = BONES[i].by;
        if (F->pay[i]   > lowThis) lowThis = F->pay[i];
        if (F->pby[i]   > lowThis) lowThis = F->pby[i];
    }
    float shift = lowRef - lowThis;
    for (int i = 0; i < NBONE; i++) { F->pay[i] += shift; F->pby[i] += shift; }
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
            out[n*2]   = (k == 0) ? bone_ax(F, b) : bone_bx(F, b);
            out[n*2+1] = (k == 0) ? bone_ay(F, b) : bone_by(F, b);
        }
        n++;
    }
    return n;
}

// ── build ──────────────────────────────────────────────────────────────
static void make_bones(Fig *F, int fi) {
    for (int i = 0; i < NBONE; i++) {
        const BoneDef *d = &BONES[i];
        float ax = bone_ax(F, i), ay = bone_ay(F, i);         // this BUILD's derived
        float bx = bone_bx(F, i), by = bone_by(F, i);         // rig, not the table's
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
        b2Polygon box = b2MakeOffsetBox(len * 0.5f, d->hw * F->sc * F->bg[i], (b2Vec2){0,0},
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
        b2Vec2 w = { WX(bone_ax(F, i)), WY(bone_ay(F, i)) };
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
//
// The build reaches the skin HERE, at bind, and it reaches it THROUGH THE BONES.
// The sprite is authored against the reference rig, so the pixel is measured
// against the authored segments and replanted on this figure's derived ones — a
// per-bone (u,d) remap, weight-blended by the same smoothstep the skinning uses
// so the seams between bones stay smooth instead of stepping. That single
// substitution is what turns "a scaled copy" into "a different build": a 22%
// longer shin drags its share of the leg strip down with it, because the strip's
// rest position is now a function of where the shin ENDED UP, not of the table.
//
// Doing it at bind and not at draw is what keeps it free: the mesh is born the
// shape this figure is, so the weights, the (t,s) spine pair and the baked rest
// windings all describe THIS guy, and no per-frame code knows the difference.
static void rest_px(const Fig *F, const SkinDef *s, int sx, int sy, float *rx, float *ry) {
    int dx = sx - s->ox;
    float ax = s->px + (s->mirror ? (s->w - 1 - dx) : dx);
    float ay = s->py + (sy - s->oy);

    float wt[6], sum = 0; int n = s->nbones;
    for (int k = 0; k < n; k++) {
        const BoneDef *d = &BONES[s->bones[k]];
        wt[k] = 1.0f - smoothstep(0, d->radius, dist_seg(ax, ay, d->ax, d->ay, d->bx, d->by));
        sum += wt[k];
    }
    if (sum < 1e-6f) {                 // outside every radius: follow the nearest alone
        int best = 0; float bd = 1e9f;
        for (int k = 0; k < n; k++) {
            const BoneDef *d = &BONES[s->bones[k]];
            float dd = dist_seg(ax, ay, d->ax, d->ay, d->bx, d->by);
            if (dd < bd) { bd = dd; best = k; }
        }
        for (int k = 0; k < n; k++) wt[k] = (k == best) ? 1.0f : 0.0f;
        sum = 1.0f;
    }
    float bx = 0, by = 0;
    for (int k = 0; k < n; k++) {
        if (wt[k] <= 0) continue;
        float u, d, px, py;
        bone_uv(s->bones[k], ax, ay, &u, &d);
        bone_place(F, s->bones[k], u, d, &px, &py);
        bx += wt[k]*px; by += wt[k]*py;
    }
    // The remap is an algebraic identity when nothing is proportioned, but only
    // to float rounding: a vertex authored exactly ON an integer comes back a
    // hair either side of it, and that is enough for tritex's int cast to move a
    // shared triangle edge by a whole pixel. Measured on the unproportioned
    // build: ~20 pixels of 5700 wrong while the figure was still moving, zero
    // once it settled. Snapping the derived rest pose to a 1/256 px grid absorbs
    // an error four orders of magnitude smaller than the quantum, which makes
    // the "author" build BIT-exact against the pre-derivation renderer (checked
    // frame by frame) and is invisible to every other build.
    *rx = figx(F, floorf(bx/sum * 256.0f + 0.5f) / 256.0f);
    *ry = figy(F, floorf(by/sum * 256.0f + 0.5f) / 256.0f);
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
        wt[k] = 1.0f - smoothstep(0, d->radius * F->sc * F->bg[s->bones[k]], bone_dist(F, s->bones[k], rx, ry));
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

// ── HANDSTAND — H turns the whole crowd over ────────────────────────────
// The first attempt was pure control: leave the bodies where they are, add 180
// degrees to every angle KEEP_ANGLE defends, and let the controller and the floor
// work out how. It does not work, and the way it fails is worth keeping. Four of
// the five rolled onto their backs with their limbs waving in the air and the
// stiffest just stayed standing (head-over-hip at frame 399: +15 +11 -5 +1 -28,
// where a real handstand is strongly positive and zero means lying down), and the
// inverted-triangle count went from 2 to 32 because a heap is a much harder pose
// to skin than a handstand is. The reason is simple once seen: an orientation
// controller cannot LIFT anything. It owns every bone's angle and not one bone's
// height, so "be upside down" is satisfied just as well by lying on your back.
//
// So the flip is a PLACEMENT and the controller only has to HOLD it, which is the
// same division of labour as standing up: nothing in this cart ever made a figure
// stand, it was placed standing and the controller kept it there. Two point
// reflections do the whole pose, no trigonometry involved:
//   1. raise the arms overhead — reflect each arm chain through its own shoulder;
//   2. turn the figure over — reflect everything through the pelvis;
// then re-anchor onto the sole line exactly as the standing rig does, so the hands
// come down where the feet stood. Composing those two is why the arms end up
// pointing at the FLOOR again: they were hanging, and now they are holding.
//
// That composition also states the controller's targets, and they are only two
// rules: every bone defends its standing angle plus 180, EXCEPT the arm chain,
// which defends the angle it already had. And the KEEP GAINS swap ROLES, because
// standing on your arms means the arms are legs now: upper arm takes the thigh's
// gain, forearm the shin's, hand the foot's, and the legs inherit the arms'
// nothing. A rule rather than a second gain column, and it says the thing a column
// would have hidden — a handstand is not a new skill for this rig, it is the same
// skill pointed the other way.
static bool  handstand = false;   // the TARGET, not the state: each figure's own fu
static float flipClock = 0.0f;    // says where he actually is. Frames since the key.
#define FLIP_FRAMES  46.0f        // frames for a turnover at stiff = 1
#define FLIP_STAGGER 11.0f        // ...and between one figure starting and the next
// WHY THE ARMS ARE LEFT FLOPPY, which is the least obvious thing in this cart and
// took an A/B to believe. Watching the crowd hold a handstand, the arms visibly bend
// (the elbow swings to 27 degrees on the way in) and the obvious reading is that they
// are too weak for the job — they are authored 1.8 px thin at 1.2 Hz because a
// HANGING arm should be floppy. Three ways of firming them up, each measured on the
// author build after 500 frames, by how far its torso ends up off vertical and how
// far the elbow bends:
//
//   arms as authored                    0.5 deg off vertical   elbow 27.0
//   + knee-stiff springs (1.2 -> 10 Hz)  34.0                  elbow  6.7
//   + a leg's bulk (density x1.6)         2.6                  elbow 29.6
//   + planted palms (see derive_handstand) 2.5                 elbow 11.9
//   all three                            53.1                  elbow  7.1
//
// Stiffening the arms straightens them and RUINS the handstand: a rigid strut fights
// the chest's own balance controller instead of yielding to it, and the compromise
// tips the whole body over. Bulk buys nothing. What actually helped was not strength
// at all but a bigger contact patch: planting the palms halves the arm bend and costs
// two degrees of lean. So the arms stay exactly as authored. A little bend in the
// arms is not weakness, it is the compliance the balance needs — and the number that
// says whether a handstand is any good is the TORSO's angle, never the elbow's.
static int keep_role(int b) {
    switch (b) {
        case B_UARM_R:  return B_THIGH_R;  case B_THIGH_R: return B_UARM_R;
        case B_LARM_R:  return B_SHIN_R;   case B_SHIN_R:  return B_LARM_R;
        case B_HAND_R:  return B_FOOT_R;   case B_FOOT_R:  return B_HAND_R;
        case B_UARM_L:  return B_THIGH_L;  case B_THIGH_L: return B_UARM_L;
        case B_LARM_L:  return B_SHIN_L;   case B_SHIN_L:  return B_LARM_L;
        case B_HAND_L:  return B_FOOT_L;   case B_FOOT_L:  return B_HAND_L;
        // Standing, the PELVIS defends the spine and the chest is free to sway.
        // Upside down the chest is the lower link, the one carrying the body's line
        // out of the shoulders, so it takes the base's job as well.
        case B_CHEST:   return B_PELVIS;
        default:        return b;          // the pelvis and head keep their own
    }
}
// A STRADDLE, not a straight-legged handstand, and the reason is the same one the
// stance rung ran into. This is a front view, so a knee tuck is not available (in
// this plane bending a knee swings the shin sideways, it cannot fold the heel back)
// but splaying the legs apart at the hip is — which is a real gymnastic straddle,
// and it happens to fix the thing that made the first version look broken: arms
// overhead make an inverted figure TALLER than a standing one (hand to shoulder
// adds height that a hanging arm never did), so the tallest builds had their shoes
// clipped off the top of the screen. Opening the legs brings the toes back down,
// drops the centre of mass and reads as deliberate.
//
// The angle is a TRADE, and both ends of it were measured. Wide (34 deg) holds the
// straightest handstand, but a straddled inverted figure is wider than the 62 px
// between neighbours, so the crowd shoves itself sideways and the middle build ends
// up 52 px from where it was placed. Narrow keeps everyone on their own spot but
// leaves the tallest build's shoes clipped off the top of the screen. Both wants
// point the same way — open the legs as little as the ceiling allows — so each
// build SOLVES for its own angle rather than sharing a magic number: the smallest
// straddle that brings its toes under the top edge. The kid barely needs one, the
// tall builds open wide, and nobody is wider than they have to be. Change a build's
// scale and its straddle follows on its own.
// (More keep-angle GAIN was the obvious third option and it measures worse at every
// value tried — 1.8x and 3.0x both sag further and multiply inverted triangles. A
// handstand is not held by squeezing harder. What DID fix the two builds that
// jackknifed is in keep_role: the chest takes the base's job when inverted.)
#define HS_MIN_STRADDLE   8.0f
#define HS_MAX_STRADDLE  40.0f
#define HS_TOP_MARGIN     5.0f    // screen px to leave above the highest toe
static void rot_about(float *x, float *y, float cx, float cy, float c, float sn) {
    float dx = *x - cx, dy = *y - cy;
    *x = cx + dx*c - dy*sn;
    *y = cy + dx*sn + dy*c;
}
// ── the flip as a TRAJECTORY, not a second pose ─────────────────────────
// The handstand rest pose, and every pose on the way to it. Everything here is in
// the same authored space proportion() left the standing rig in, and each step
// records the WORLD ANGLE it turned each bone by, because that same number is what
// the controller has to defend and what place_pose has to write.
//
// This started as five steps run once at build, which put a figure upside down in
// one frame: a gymnast PLACED rather than one going over. The whole of the
// difference is the parameter `u` below. The five steps did not need replacing to
// get a turnover out of them, they needed to be taken IN TIME — the derivation
// already had the choreography in it, listed in the order a body actually does it,
// and running the steps at overlapping rates is the entire cartwheel:
//
//   u1  0.00 .. 0.45   the arms come up overhead
//   u2  0.15 .. 1.00   and over you go, pivoting about the hips
//   u3  0.35 .. 1.00   the legs open into the straddle
//   u4  0.70 .. 1.00   the palms turn down to meet the floor
//
// WHY THIS RIG CAN DO IT AT ALL, which is the payoff of a cost paid twice already.
// Being a FRONT view is what made a stance expressible (an angle swings a limb OUT,
// not forward) and what forced the straddle (a knee tuck is not available in this
// plane). It also decides which turnover is on the menu. A handstand ENTRY is
// sagittal — bend at the hips, plant the hands ahead of you, kick the legs up — and
// this rig cannot express one frame of it. A CARTWHEEL is coronal, a rotation in
// exactly the plane the rig lives in, and a cartwheel is straddled, which is the
// shape the ceiling solve already arrived at for its own reasons. The move this rig
// can do is the one it was already holding.
//
// Two ways this stays honest. The ENDPOINTS are untouched: every sub-phase returns
// exactly 0 at u = 0 and exactly 1 at u = 1, and at u = 1 the rotations take the
// exact point-reflection path (see turn_about), so the pose the crowd holds is the
// same pose bit for bit and the settled handstand numbers A/B against the instant
// flip. And nothing here is a new authority — mid-flip a figure is PLACED, exactly
// as it was placed standing at build and placed inverted on the old keypress, so
// this adds frames to a mechanism that already existed rather than a second one.
#define HS_ARC   12.0f    // authored px the hips rise at mid-turnover
// A sub-phase: 0 before it starts, 1 after it ends, eased in between. Overlapping
// ranges are what stop this reading as four separate mechanical moves.
static float seg(float u, float a, float b) {
    if (u <= a) return 0.0f;
    if (u >= b) return 1.0f;
    return smoothstep(a, b, u);
}
// Turn a point u of the way through a half-turn about (cx,cy), sg picking the
// direction. At u == 1 take the POINT REFLECTION path the derivation used, because
// it is exact where the rotation is not: cosf(PI) does come back -1, but sinf(PI)
// is 8.7e-8, and a handstand rig that lands a hair off the exact reflection makes
// every settled number un-A/B-able against the flip that came before it.
static void turn_about(float *x, float *y, float cx, float cy, float u, float sg) {
    if (u >= 1.0f) { *x = 2*cx - *x; *y = 2*cy - *y; return; }
    rot_about(x, y, cx, cy, cosf(u*PI), sg*sinf(u*PI));
}
// `solve` runs the two steps that need the FINISHED pose (the straddle's ceiling
// scan and the sole-line re-anchor) and caches them; it is only ever called with
// u = 1, at build. Every later call rides those two numbers.
static void derive_flip(Fig *F, float u, bool solve) {
    float u1 = seg(u, 0.00f, 0.45f), u2 = seg(u, 0.15f, 1.00f);
    float u3 = seg(u, 0.35f, 1.00f), u4 = seg(u, 0.70f, 1.00f);
    for (int i = 0; i < NBONE; i++) {
        F->hax[i] = F->pax[i]; F->hay[i] = F->pay[i];
        F->hbx[i] = F->pbx[i]; F->hby[i] = F->pby[i];
        F->hang[i] = 0.0f;
    }
    const int ARM[2][3] = { {B_UARM_R, B_LARM_R, B_HAND_R}, {B_UARM_L, B_LARM_L, B_HAND_L} };
    const int LEG[2][3] = { {B_THIGH_R, B_SHIN_R, B_FOOT_R}, {B_THIGH_L, B_SHIN_L, B_FOOT_L} };
    // 1. arms overhead, about the shoulder. The DIRECTION only matters in the
    //    middle (a half-turn lands in the same place either way), and it is the
    //    one anybody raising an arm uses: out to the side and up, not across the
    //    chest. Out is a negative angle on the body's right, per the pose sign rule.
    if (u1 > 0.0f) for (int s = 0; s < 2; s++) {
        float sg = (s == 0 ? -1.0f : 1.0f);
        float sx = F->hax[ARM[s][0]], sy = F->hay[ARM[s][0]];
        for (int k = 0; k < 3; k++) {
            int b = ARM[s][k];
            turn_about(&F->hax[b], &F->hay[b], sx, sy, u1, sg);
            turn_about(&F->hbx[b], &F->hby[b], sx, sy, u1, sg);
            F->hang[b] += sg * u1 * PI;              // (2PI by the end = none, which is
        }                                            //  why the arms end up HOLDING)
    }
    if (u2 > 0.0f) {                                 // 2. over you go, about the pelvis
        float cx = F->hax[B_PELVIS], cy = F->hay[B_PELVIS];
        for (int i = 0; i < NBONE; i++) {
            turn_about(&F->hax[i], &F->hay[i], cx, cy, u2, 1.0f);
            turn_about(&F->hbx[i], &F->hby[i], cx, cy, u2, 1.0f);
            F->hang[i] += u2 * PI;
        }
    }
    // 3. open the legs, as little as the ceiling allows. The figure is scaled
    //    about RIG_BY afterwards, so the authored-space ceiling depends on this
    //    build's own size — invert figy() to find it.
    if (solve) {
        float ceil = RIG_BY - (RIG_BY - HS_TOP_MARGIN) / F->sc;
        float deg  = HS_MIN_STRADDLE;
        for (; deg < HS_MAX_STRADDLE; deg += 1.0f) {
            float c = cosf(deg*DEG), sn = sinf(deg*DEG), top = 1e9f;
            for (int i = 0; i < NBONE; i++) {                  // the still parts
                if (i >= B_THIGH_R) continue;
                if (F->hay[i] < top) top = F->hay[i];
                if (F->hby[i] < top) top = F->hby[i];
            }
            for (int s = 0; s < 2; s++) {                      // ...and the legs, if opened
                float sg = (s == 0 ? -1.0f : 1.0f);
                float hx = F->hax[LEG[s][0]], hy = F->hay[LEG[s][0]];
                for (int k = 0; k < 3; k++) {
                    int b = LEG[s][k];
                    float x1 = F->hax[b], y1 = F->hay[b], x2 = F->hbx[b], y2 = F->hby[b];
                    rot_about(&x1, &y1, hx, hy, c, sg*sn);
                    rot_about(&x2, &y2, hx, hy, c, sg*sn);
                    if (y1 < top) top = y1;
                    if (y2 < top) top = y2;
                }
            }
            if (top >= ceil) break;                            // it fits
        }
        F->hstraddle = deg;
    }
    if (u3 > 0.0f) for (int s = 0; s < 2; s++) {
        float a = (s == 0 ? -1.0f : 1.0f) * u3 * F->hstraddle * DEG;
        float c = cosf(a), sn = sinf(a);
        float hx = F->hax[LEG[s][0]], hy = F->hay[LEG[s][0]];   // about the hip
        for (int k = 0; k < 3; k++) {
            int b = LEG[s][k];
            rot_about(&F->hax[b], &F->hay[b], hx, hy, c, sn);
            rot_about(&F->hbx[b], &F->hby[b], hx, hy, c, sn);
            F->hang[b] += a;
        }
    }
    // 4. PLANT THE PALMS. This is the one the pictures kept pointing at and the
    //    numbers kept confirming: with gains, springs and bulk all swapped, the
    //    upper arm still sat 90 degrees off target and the body sank between the
    //    arms. Nothing was buckling. The figure simply has FEET and no HANDS. A
    //    foot is a wide flat sole, authored 13 px long and lying along the floor;
    //    a hand is the tip of a thin vertical bone, a contact patch about 4 px
    //    across, and two of those slide apart under load exactly the way real
    //    hands do on a smooth floor. So the wrist turns the hand flat, fingers
    //    outward, the way anybody planting their palms would — and it turns LAST,
    //    on the way down, which is when a real hand opens to take the floor.
    if (u4 > 0.0f) for (int s = 0; s < 2; s++) {
        int h = (s == 0) ? B_HAND_R : B_HAND_L;
        float a = (s == 0 ? -90.0f : 90.0f) * u4 * DEG;      // down -> outward
        float c = cosf(a), sn = sinf(a);
        rot_about(&F->hbx[h], &F->hby[h], F->hax[h], F->hay[h], c, sn);   // about the wrist
        F->hang[h] += a;
    }
    if (solve) {
        float lowRef = -1e9f, lowThis = -1e9f;           // 5. palms onto the sole line,
        for (int i = 0; i < NBONE; i++) {                //    where the feet used to be
            if (BONES[i].ay > lowRef)  lowRef  = BONES[i].ay;
            if (BONES[i].by > lowRef)  lowRef  = BONES[i].by;
            if (F->hay[i]   > lowThis) lowThis = F->hay[i];
            if (F->hby[i]   > lowThis) lowThis = F->hby[i];
        }
        F->hshift = lowRef - lowThis;
    }
    // ...and that same re-anchor, RIDDEN by the turnover, is the hips travelling
    // from hip height to hand height instead of teleporting there. The ARC on top
    // of it is the one number in this cart nobody derived: a placed figure has no
    // ballistics to integrate, so the push off the floor is stated. Take it out and
    // the turnover reads as a body rotating about a nail driven through its pelvis.
    float lift  = (u2 <= 0.0f || u2 >= 1.0f) ? 0.0f : -HS_ARC * sinf(PI * u2);
    float shift = u2 * F->hshift + lift;
    for (int i = 0; i < NBONE; i++) { F->hay[i] += shift; F->hby[i] += shift; }
}
// Solve the flip once at build, then leave the rig STANDING, so h* always agrees
// with fu instead of holding a pose nobody is in.
static void flip_solve(Fig *F) { derive_flip(F, 1.0f, true); derive_flip(F, 0.0f, false); }
// Is this figure mid-turnover, i.e. being placed rather than simulated? Held at
// either end (0 or 1) it is a rest pose and physics owns it again.
static inline bool flipping(const Fig *F) { return F->fu > 0.0f && F->fu < 1.0f; }
// Screen rotations turn the other way from world ones (y is flipped), so the one
// place both the body transform and the controller target come from is here. hang
// is zero at fu = 0, so this needs no "am I inverted" question asked of it.
static inline float hs_rot(const Fig *F, int b) { return -F->hang[b]; }
// A planted palm is a quarter turn at the wrist, and the authored wrist only travels
// 60 degrees, so the limit has to let go of it while inverted. This is ALL that had
// to change at the joints — see the table above keep_role for the three firmer-arm
// ideas that measured worse.
static void apply_role_joints(Fig *F) {
    for (int h = 0; h < 2; h++) {
        int i = h ? B_HAND_L : B_HAND_R;
        float ex = (F->fu > 0.0f) ? 100.0f*DEG : 0.0f;   // let go of it the moment he
        b2RevoluteJoint_SetLimits(F->hinge[i], BONES[i].lo*DEG - ex, BONES[i].hi*DEG + ex);
    }                                                    // leaves the floor, take it back
}                                                        // the moment he lands on it

// Put the bodies into the rest pose the flip parameter currently describes — which
// at fu = 0 is this build's standing rig, so ONE path covers standing, holding a
// handstand, and every frame in between. The bind data is all body-LOCAL (offsets,
// and the spine's chain fractions), so every skin follows for free — no rebind,
// which is the whole reason a keypress could afford to do this once and a turnover
// can afford to do it sixty times.
static void place_pose(Fig *F) {
    for (int i = 0; i < NBONE; i++) {
        float ax = figx(F,F->hax[i]), ay = figy(F,F->hay[i]);
        float bx = figx(F,F->hbx[i]), by = figy(F,F->hby[i]);
        b2Vec2 mid = { WX((ax+bx)*0.5f), WY((ay+by)*0.5f) };
        // The authored direction is baked in the SHAPE, so a body only ever needs
        // the DELTA from its standing rest.
        b2Body_SetTransform(F->bone[i], mid, b2MakeRot(hs_rot(F, i)));
        b2Body_SetLinearVelocity(F->bone[i], (b2Vec2){0,0});
        b2Body_SetAngularVelocity(F->bone[i], 0.0f);
        b2Body_SetAwake(F->bone[i], true);
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
    handstand = false;                     // R is a real reset: nobody is mid-flip,
    for (int f = 0; f < NFIG; f++) {       // and nobody's controller is still
        fig[f].dx    = BUILDS[f].dx;       // defending an upside-down target
        fig[f].stiff = BUILDS[f].stiff;
        fig[f].sc    = BUILDS[f].sc;
        fig[f].girth = BUILDS[f].girth;
        fig[f].fu    = 0.0f;
        proportion(&fig[f], &BUILDS[f]);   // derive the rig BEFORE any body exists
        make_bones(&fig[f], f);
        flip_solve(&fig[f]);               // the same pose, upside down, and the way there (H)
        for (int i = 0; i < NSKIN; i++) build_mesh(&fig[f], i);
        fig[f].restUp = (float)FLOOR_PY - (SCREEN_H - b2Body_GetPosition(fig[f].bone[B_HEAD]).y*PPM);
    }
    // The mesh deliberately overshoots the silhouette by one cell (see
    // cell_opaque), so it DOES sample transparent texels — without a colorkey
    // those are opaque black and every limb wears a black halo.
    colorkey(CLR_BLACK);
    dragging = false; dragFig = dragBone = -1;
}
static void reset(void) { b2DestroyWorld(world); build(); }

// ── idle: standing still is the one thing a body never does ─────────────
// Five figures holding an identical pose read as five props, so each build gets
// a slow silent fidget: a breath at the chest, a weight shift the thighs trade
// between them, the arms swaying with it and the head looking around. Nothing
// here touches the render. It is an ask stated in JOINT space, exactly like a
// pose, and it arrives two ways because this rig has two authorities over a
// bone's angle:
//   the hinge SPRING for bones with no keep-angle gain (chest, arms) — set the
//   joint's target and the spring has to get there, so a floppy build lags and
//   undershoots its own fidget while a stiff one tracks it;
//   the KEEP_ANGLE controller for the bones that hold the figure up (head,
//   thighs, shins, feet), which writes angular velocity and would simply erase a
//   joint target — so the fidget is added to the angle it DEFENDS instead.
// One ask, accumulated down the chain the same way the pose is, feeding whichever
// authority owns each bone. And the same rule as always: never write a joint
// something else owns this frame, meaning the two the F fold drives and whatever
// the mouse is holding (playtime's `hitted` rule, one layer up).
static bool  idle = true;
static float idleClock = 0;      // ours, not timer(): a replay has to be deterministic
static void idle_pose(void) {
    for (int f = 0; f < NFIG; f++) {
        Fig *F = &fig[f];
        float ask[NBONE] = {0};
        // Mid-turnover the flip owns every joint on this figure, so the ask goes to
        // zero rather than being layered under it: a fidget is what a body does when
        // it is standing still, and this one is not.
        if (idle && !flipping(F)) {
            float ph     = f * 1.7f;                              // no two in lockstep
            float breath = sinf(idleClock * 1.55f + ph);           // ~0.25 Hz
            float shift  = sinf(idleClock * 0.72f + ph * 0.6f);    // ~0.11 Hz, the weight
            float look   = sinf(idleClock * 0.41f + ph * 1.3f);
            ask[B_CHEST]   = (breath * 1.1f - shift * 1.4f) * DEG;
            ask[B_HEAD]    =  look * 4.0f * DEG;
            ask[B_UARM_R]  = -shift * 2.2f * DEG;
            ask[B_UARM_L]  = -shift * 2.2f * DEG;
            ask[B_THIGH_R] =  shift * 1.2f * DEG;
            ask[B_THIGH_L] = -shift * 1.2f * DEG;                  // antiphase = weight moves
        }
        for (int i = 0; i < NBONE; i++) {
            int p = BONES[i].parent;
            F->idleW[i] = (p >= 0 ? F->idleW[p] : 0.0f) + ask[i];
            if (p < 0) continue;                                   // the root has no hinge
            if (folding && (i == B_LARM_R || i == B_SHIN_R)) continue;   // the fold owns those
            if (dragging && dragFig == f && dragBone == i) continue;     // the hand owns this
            b2RevoluteJoint_SetTargetAngle(F->hinge[i], ask[i]);
            if (ask[i] != 0.0f) b2Body_SetAwake(F->bone[i], true);
        }
    }
}

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
      // Mid-turnover this figure is PLACED, and an orientation controller writing
      // angular velocity into a body whose transform is overwritten before the next
      // step is not steering anything — it is just seeding the contact solve with
      // spin. It gets its job back when he lands.
      if (flipping(F)) continue;
      for (int i = 0; i < NBONE; i++) {
        // Which job is this bone doing? Standing, its own. Upside down, the FIRMER
        // of its own and its counterpart's (see keep_role) — a straight swap was the
        // first idea and it measured badly, because handing the legs the arms' job
        // hands them the arms' floppiness, and five figures sagged onto their faces
        // with their legs waving. Inverted, BOTH pairs are load-bearing: the arms
        // hold the floor and the legs hold the body's line. The handover rides fu
        // like everything else, so it is a lerp between the two jobs rather than a
        // question about which pose he is in.
        float own = BONES[i].keepKp, other = BONES[keep_role(i)].keepKp;
        float firmer = other > own ? other : own;
        float kp = (own + F->fu * (firmer - own)) * size_hz(F);
        if (kp <= 0.0f) continue;
        if (dragging && dragFig == f && dragBone == i) continue;   // playtime's `hitted` rule
        float want = F->rest[i] + F->idleW[i] + hs_rot(F, i);
        float diff = want - bone_angle(F, i);                      // rest + fidget + the flip
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
    if (keyp('I')) idle = !idle;               // the fidget — off to measure a still pose
    if (keyp('H')) { handstand = !handstand; flipClock = 0.0f; }   // ...and over they go
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

    // ── the turnover ────────────────────────────────────────────────────
    // Two numbers per figure and the trajectory falls out. SPEED comes off the same
    // stiffness multiplier that gives each build its temperament everywhere else —
    // as a square root, because that is the law a pendulum's rate follows and a
    // turnover about the hips is one — so the bruiser snaps over in 32 frames and
    // the kid takes 57 without either being told to.
    //
    // The STAGGER is not decoration. A figure halfway through a turnover is lying
    // horizontal and a horizontal figure is wider than the 62 px between
    // neighbours, so a crowd that all went at once would sweep through each other.
    // Offsetting the start means no two are horizontal at the same moment, and what
    // the fix looks like is a wave rolling down the line.
    flipClock += 1.0f;
    for (int f = 0; f < NFIG; f++) {
        Fig *F = &fig[f];
        float tgt = handstand ? 1.0f : 0.0f;
        if (F->fu == tgt) continue;                       // held at one end: physics has him
        if (flipClock < f * FLIP_STAGGER) continue;       // the wave has not reached him
        float step = sqrtf(F->stiff) / FLIP_FRAMES;
        F->fu += (tgt > F->fu) ? step : -step;
        if (F->fu > 1.0f) F->fu = 1.0f;
        if (F->fu < 0.0f) F->fu = 0.0f;                   // lands EXACTLY on the endpoint,
        derive_flip(F, F->fu, false);                     // which is what keeps the held
        place_pose(F);                                    // pose bit-identical to the
        apply_role_joints(F);                             // instant flip's
    }

    idleClock += 1.0f/60.0f;
    idle_pose();                       // ask for the fidget, then...
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
        n += snprintf(b+n, sizeof(b)-n, "%s%.2f", f?" ":"", up / fig[f].restUp);
      }
      watch("standing", "%s", b); }
    // Is he actually INVERTED, or just in a heap? Head depth minus hip depth, both
    // measured DOWN from the top of the screen: strongly negative standing (the head
    // is the higher of the two), strongly positive in a handstand, near zero means he
    // is lying down and the flip failed. The one honest handstand number (`standing`
    // above only means anything in the upright world).
    { char b[64]; int n = 0;
      for (int f = 0; f < NFIG; f++) {
        float hip  = SCREEN_H - b2Body_GetPosition(fig[f].bone[B_PELVIS]).y*PPM;
        float head = SCREEN_H - b2Body_GetPosition(fig[f].bone[B_HEAD]).y*PPM;
        n += snprintf(b+n, sizeof(b)-n, "%s%+.0f", f?" ":"", head - hip);
      }
      watch("headOverHip", "%s", b); }
    // How far each figure has WANDERED from where it was placed, px. Standing it
    // is pinned by two flat soles; on its hands it can slide, and this is the
    // number that says whether a fidget has turned into a walk.
    { char b[64]; int n = 0;
      for (int f = 0; f < NFIG; f++)
        n += snprintf(b+n, sizeof(b)-n, "%s%+.0f", f?" ":"",
                      b2Body_GetPosition(fig[f].bone[B_PELVIS]).x*PPM - (RIG_CX + BUILDS[f].dx));
      watch("drift", "%s", b); }
    { char b[64]; int n = 0;                 // the straddle each build solved for
      for (int f = 0; f < NFIG; f++) n += snprintf(b+n, sizeof(b)-n, "%s%.0f", f?" ":"", fig[f].hstraddle);
      watch("straddle", "%s", b); }
    // How far over each figure is: 0 standing, 1 holding, between = mid-turnover and
    // being placed. Reading the wave roll down the line is what says the stagger
    // fired, and a figure stuck between the endpoints is a flip that never landed.
    { char b[64]; int n = 0;
      for (int f = 0; f < NFIG; f++) n += snprintf(b+n, sizeof(b)-n, "%s%.2f", f?" ":"", fig[f].fu);
      watch("flipU", "%s", b); }
    watch("chestRel",   "%.1f", (bone_angle(&fig[2], B_CHEST)  - bone_angle(&fig[2], B_PELVIS)) / DEG);
    // Where a handstand LEANS. Every one of these is a joint that could be giving
    // way, and the answer was not the one that looked obvious from the picture.
    watch("shoulderRel","%.1f", (bone_angle(&fig[2], B_UARM_R) - bone_angle(&fig[2], B_CHEST))  / DEG);
    watch("wristRel",   "%.1f", (bone_angle(&fig[2], B_HAND_R) - bone_angle(&fig[2], B_LARM_R)) / DEG);
    watch("chestAbs",   "%.1f", bone_angle(&fig[2], B_CHEST) / DEG);   // ~180 when inverted
    watch("uArmAbs",    "%.1f", bone_angle(&fig[2], B_UARM_R) / DEG);  // ~0 when holding
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
    // Kept SHORT on purpose: the state chip is right-aligned on this same line,
    // and at 41 characters the old spine caption ran into it ("...clampkeep:omega").
    print(mode == 0 ? "weighted bones, rotation-blended"
        : mode == 1 ? "linear blend, a folded knee pinches"
        : mode == 2 ? "rigid, one bone per vertex, tears"
        : curveClamp ? "spine (t,s) on a Bezier, clamped"
                    : "spine (t,s) on a Bezier, clamp OFF", 30, 4, CLR_LIGHT_GREY);
    // The key list has to FIT: at FONT_SMALL's 5px advance the screen holds 63
    // characters, and one line of all nine keys is 74 — the old single line ran
    // off the edge reading "...B bone", so R reset was never visible to anyone
    // (ui-audit's catch). Split by AUDIENCE instead of by width: what a stranger
    // does with the cart on the bottom line, the research A/B toggles dimmer
    // above it, and the two cycling STATES parked top-right where the mode name
    // already lives. Both lines are now comfortably inside 320.
    char hud[48];
    snprintf(hud, sizeof hud, "keep:%s%s", KEEP_NAME[keepMode], idle ? "  idle" : "");
    print(hud, SCREEN_W - 4 - (int)strlen(hud)*5, 4, CLR_LIGHT_GREY);
    print("C clamp  G keep  M mesh  B bones",      4, SCREEN_H-18, CLR_MEDIUM_GREY);
    print("drag  SPACE mode  I idle  F fold  H handstand  R reset", 4, SCREEN_H-10, CLR_LIGHT_GREY);
}
