/* de:meta
{
  "slug": "puppet",
  "collection": ["physics"],
  "title": "Puppet — sprite parts as <=8-vert Box2D polys + texture",
  "status": "active",
  "created": "2026-07-23",
  "kind": ["tech-demo", "probe"],
  "teaches": ["rigid-body", "procedural-mesh", "marching-squares"],
  "lineage": "The 'puppetmaker' representation on real Box2D: every body part is ONE convex polygon of <=8 (x,y) points (Box2D's B2_MAX_POLYGON_VERTICES) plus a texture — derived automatically from a sprite's alpha (convex-hull the opaque pixels -> reduce to <=8 -> b2ComputeHull/b2MakePolygon), drawn with tritex straight from the polygon's own verts so the paint EXACTLY covers the collision shape. The whole figure is DATA: a Rig table of parts (sheet rects + rest centres) and joints (with angle limits + springs). Two rigs — a hung marionette and a bolted desk lamp — run on the same engine to prove retargeting to any sheet. Rigid-body sibling of the verlet ragdoll.c and the soft-body carfit spike.",
  "description": {
    "summary": "Marionette and desk lamp built from sprites: every part's opaque pixels are convex-hulled down to <=8 points (Box2D's polygon budget), turned into a real b2MakePolygon collision shape, and textured with tritex from that same polygon — so what you see is exactly what collides. The whole figure is a data table (part rects + joints with limits + springs); [1]/[2] swap rigs. Drag a part to swing it; springs snap it back to pose.",
    "detail": "Two directions in one cart. RETARGETING: nothing is hard-coded geometry — a Rig is a table of PartDefs (a sprite rect + a rest-pose centre) and JointDefs (two parts, a world anchor, angle limits, a spring). Per part: sget() reads the rect into an alpha cloud, a monotone-chain convex hull + a min-area vertex cull reduce it to <=8 points, b2ComputeHull + b2MakePolygon make the rigid shape, and tritex fans the sprite across the returned polygon's verts (UV = each vert mapped back to its sheet pixel) so texture == hull. Swap the table and you retarget to any sheet: RIG 1 is a 10-part hung marionette, RIG 2 a 3-part desk lamp on a bolted (static) base. POSE: every joint carries enableLimit (lowerAngle/upperAngle around the rest pose) + an enableSpring toward targetAngle, so limbs can't hyperextend and the figure holds its pose and springs back when you let go — the lamp stands upright, the marionette keeps a relaxed stance. Contrast carfit (soft deform mesh) and ragdoll (verlet sticks): this is the rigid, joint-driven, real-Box2D path.",
    "controls": "[1] marionette  [2] desk lamp.  Drag any movable part with the mouse to swing / bend it; release and the springs pull it back to pose. R resets the current rig."
  },
  "todo": [
    "Load the Rig table from external data (a region table baked into the sheet / a .cart.js) so an imported character becomes a puppet with no code edit.",
    "Per-part rest ANGLE so a rig can pose limbs pre-bent (currently every part rests upright).",
    "Optional multi-hull parts (convex decomposition) for concave silhouettes beyond the <=8-vert budget."
  ]
}
de:meta */
#include "studio.h"
#include "box2d/box2d.h"      // opt-in: make-cart/play link the vendored Box2D v3
#include "boxrig.h"           // shared sprite -> <=8-vert hull + textured draw
#include <math.h>
#include <stdio.h>            // snprintf

// Box2D is METRES, y-UP; studio.h is PIXELS, y-DOWN. Bridge with PPM.
#define PPM     22.0f
#define SX(wx)  ((int)((wx) * PPM))
#define SY(wy)  ((int)(SCREEN_H - (wy) * PPM))
#define WX(sx)  ((sx) / PPM)
#define WY(sy)  ((SCREEN_H - (sy)) / PPM)
#define DEG     (3.14159265f / 180.0f)
#define FLOORY  0.4f          // metres
#define MAXPART 16

// ── THE RIG: pure data. Swap these tables to retarget to any sprite sheet. ─────
typedef struct {
    const char *name;
    int   ox, oy, w, h;       // this part's sprite rect on the sheet
    float cx, cy;             // rest-pose CENTRE on screen (px)
    bool  fixed;              // true = static (bolted down, e.g. a lamp base)
} PartDef;

typedef struct {
    int   a, b;               // part indices this hinge joins
    float ax, ay;             // hinge anchor in the rest pose (screen px)
    float lo, hi;             // angle limits (degrees, around rest); 0,0 = no limit
    float hz;                 // spring stiffness Hz toward the rest angle; 0 = floppy
} JointDef;

typedef struct {
    const char *title;
    const PartDef  *parts;  int nparts;   // drawn in this order (back → front)
    const JointDef *joints; int njoints;
    int   hangPart; float hangX, hangY;   // hangPart<0 = no hang (grounded rig)
} Rig;

// RIG 1 — a marionette (parts listed back-to-front so head draws on top)
static const PartDef HUMAN_P[] = {
    {"torso", 16,0,20,34, 160, 60, false},   // 0
    {"uArmL", 40,0,10,24, 150, 58, false},   // 1
    {"uArmR", 40,0,10,24, 170, 58, false},   // 2
    {"uLegL", 64,0,12,28, 154, 92, false},   // 3
    {"uLegR", 64,0,12,28, 166, 92, false},   // 4
    {"lArmL", 52,0,10,22, 150, 82, false},   // 5
    {"lArmR", 52,0,10,22, 170, 82, false},   // 6
    {"lLegL", 78,0,12,26, 154,120, false},   // 7
    {"lLegR", 78,0,12,26, 166,120, false},   // 8
    {"head",   0,0,16,16, 160, 30, false},   // 9
};
static const JointDef HUMAN_J[] = {
    {0,9, 160,40,  -60, 60, 2.0f},   // neck
    {0,1, 150,48, -110,110, 1.5f},   // shoulderL
    {1,5, 150,70, -120,120, 1.5f},   // elbowL
    {0,2, 170,48, -110,110, 1.5f},   // shoulderR
    {2,6, 170,70, -120,120, 1.5f},   // elbowR
    {0,3, 155,78,  -90, 90, 1.8f},   // hipL
    {3,7, 154,106, -110,110, 1.8f},  // kneeL
    {0,4, 165,78,  -90, 90, 1.8f},   // hipR
    {4,8, 166,106, -110,110, 1.8f},  // kneeR
};

// RIG 2 — a desk lamp: a bolted base + a sprung arm + a shade that holds upright
static const PartDef LAMP_P[] = {
    {"base", 0,40,24,14, 210,180, true },    // 0 static (bolted foot)
    {"arm",  26,40,8,34, 210,158, false},    // 1
    {"head", 36,40,22,14, 210,134, false},   // 2
};
static const JointDef LAMP_J[] = {
    {0,1, 210,174, -45,45, 4.0f},    // base hinge — springs upright
    {1,2, 210,142, -40,40, 4.0f},    // shade nod
};

static const Rig RIGS[] = {
    {"marionette", HUMAN_P, 10, HUMAN_J, 9, 9, 160, 18},
    {"desk lamp",  LAMP_P,  3,  LAMP_J,  2, -1, 0, 0},
};
#define NRIG ((int)(sizeof(RIGS)/sizeof(RIGS[0])))

// ── live state ────────────────────────────────────────────────────────────────
typedef struct { b2BodyId body; b2Polygon poly; float pcx, pcy; bool fixed; } Part;
static b2WorldId world;
static b2BodyId  ground;
static Part      part[MAXPART];
static int       curRig = 0;
static const Rig *rig;
static b2JointId mjoint; static bool dragging = false;

// build one part: sprite alpha → <=8-vert hull (boxrig.h) → body + shape
static void build_part(const PartDef *pd, Part *p) {
    b2Hull bh = boxrig_hull(pd->ox, pd->oy, pd->w, pd->h, PPM, &p->pcx, &p->pcy);

    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = pd->fixed ? b2_staticBody : b2_dynamicBody;
    bd.position = (b2Vec2){ WX(pd->cx), WY(pd->cy) };
    p->body = b2CreateBody(world, &bd);
    p->fixed = pd->fixed;

    p->poly = b2MakePolygon(&bh, 0.0f);
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 1.0f;
    sd.material.friction = 0.6f;
    b2CreatePolygonShape(p->body, &sd, &p->poly);
}

static void link_joint(const JointDef *jd) {
    b2Vec2 w = { WX(jd->ax), WY(jd->ay) };
    b2RevoluteJointDef d = b2DefaultRevoluteJointDef();
    d.bodyIdA = part[jd->a].body; d.bodyIdB = part[jd->b].body;
    d.localAnchorA = b2Body_GetLocalPoint(part[jd->a].body, w);
    d.localAnchorB = b2Body_GetLocalPoint(part[jd->b].body, w);
    d.collideConnected = false;
    if (jd->lo != 0 || jd->hi != 0) {
        d.enableLimit = true;
        d.lowerAngle = jd->lo * DEG; d.upperAngle = jd->hi * DEG;
    }
    if (jd->hz > 0) {                                       // spring back toward the rest pose
        d.enableSpring = true; d.hertz = jd->hz; d.dampingRatio = 0.6f; d.targetAngle = 0;
    }
    b2CreateRevoluteJoint(world, &d);
}

static void build(void) {
    rig = &RIGS[curRig];
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){ 0.0f, -10.0f };
    world = b2CreateWorld(&wd);

    b2BodyDef gd = b2DefaultBodyDef();
    ground = b2CreateBody(world, &gd);
    float W = SCREEN_W / PPM;
    b2Segment floor = {{0, FLOORY}, {W, FLOORY}};
    b2ShapeDef fs = b2DefaultShapeDef();
    b2CreateSegmentShape(ground, &fs, &floor);

    for (int i = 0; i < rig->nparts; i++) build_part(&rig->parts[i], &part[i]);
    for (int j = 0; j < rig->njoints; j++) link_joint(&rig->joints[j]);

    if (rig->hangPart >= 0) {                               // marionette string (springy → tug-able)
        b2Vec2 hw = { WX(rig->hangX), WY(rig->hangY) };
        b2BodyId hb = part[rig->hangPart].body;
        b2Vec2 hp = b2Body_GetPosition(hb);
        float dx = hp.x - hw.x, dy = hp.y - hw.y, len = sqrtf(dx*dx + dy*dy);
        b2DistanceJointDef hd = b2DefaultDistanceJointDef();
        hd.bodyIdA = ground; hd.bodyIdB = hb;
        hd.localAnchorA = b2Body_GetLocalPoint(ground, hw);
        hd.localAnchorB = (b2Vec2){ 0, 0 };                // head centre
        hd.length = len;
        hd.enableSpring = true; hd.hertz = 4.0f; hd.dampingRatio = 0.7f;
        hd.enableLimit = true; hd.minLength = 0.0f; hd.maxLength = len + 1.2f;
        b2CreateDistanceJoint(world, &hd);
    }
    dragging = false;
}

static void reset(void) { b2DestroyWorld(world); build(); }
static void switch_rig(int r) { b2DestroyWorld(world); curRig = r; build(); }

// pick the part under the cursor: point-INSIDE a part's polygon first (click anywhere on
// a visible limb), else the nearest part centre within a forgiving radius.
static int pick_part(float mwx, float mwy) {
    for (int i = 0; i < rig->nparts; i++)                  // click INSIDE a part (boxrig.h)
        if (!part[i].fixed && boxrig_point_in_body(part[i].body, &part[i].poly, mwx, mwy)) return i;
    int best = -1; float bd = 0.9f;                        // fallback: nearest centre
    for (int i = 0; i < rig->nparts; i++) {
        if (part[i].fixed) continue;
        b2Vec2 c = b2Body_GetPosition(part[i].body);
        float d = sqrtf((mwx-c.x)*(mwx-c.x) + (mwy-c.y)*(mwy-c.y));
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static bool inited = false;
void update(void) {
    if (!inited) { build(); inited = true; }
    if (keyp('R')) reset();
    if (keyp('1') && curRig != 0) switch_rig(0);
    if (keyp('2') && curRig != 1) switch_rig(1);

    float mwx = WX(mouse_x()), mwy = WY(mouse_y());
    if (mouse_pressed(MOUSE_LEFT)) {                        // grab the part under the cursor
        int best = pick_part(mwx, mwy);
        if (best >= 0) {
            b2MouseJointDef d = b2DefaultMouseJointDef();
            d.bodyIdA = ground; d.bodyIdB = part[best].body;
            d.target = (b2Vec2){ mwx, mwy };
            d.hertz = 6.0f; d.dampingRatio = 0.7f;
            d.maxForce = 1500.0f * b2Body_GetMass(part[best].body);
            mjoint = b2CreateMouseJoint(world, &d);
            b2Body_SetAwake(part[best].body, true);
            dragging = true;
        }
    }
    if (dragging && mouse_down(MOUSE_LEFT)) b2MouseJoint_SetTarget(mjoint, (b2Vec2){ mwx, mwy });
    if (mouse_released(MOUSE_LEFT) && dragging) { b2DestroyJoint(mjoint); dragging = false; }

    b2World_Step(world, 1.0f / 60.0f, 4);

#ifdef DE_TRACE
    int tv = 0; for (int i = 0; i < rig->nparts; i++) tv += part[i].poly.count;
    watch("rig", "%d", curRig);
    watch("parts", "%d", rig->nparts);
    watch("hullVerts", "%d", tv);
#endif
}

void draw(void) {
    if (!inited) { build(); inited = true; }
    cls(CLR_DARK_BLUE);
    rectfill(0, SY(FLOORY), SCREEN_W, SCREEN_H, CLR_DARK_GREEN);

    if (rig->hangPart >= 0) {                               // marionette string
        b2Vec2 hp = b2Body_GetPosition(part[rig->hangPart].body);
        line(SX(WX(rig->hangX)), SY(WY(rig->hangY)), SX(hp.x), SY(hp.y), CLR_LIGHT_GREY);
    }
    for (int i = 0; i < rig->nparts; i++)
        boxrig_draw(part[i].body, part[i].poly, part[i].pcx, part[i].pcy, PPM, SCREEN_H);

    font(FONT_SMALL);
    char b[96];
    snprintf(b, sizeof b, "%s: %d parts, each a <=8-vert convex hull -> b2MakePolygon + tritex", rig->title, rig->nparts);
    print(b, 4, 4, CLR_WHITE);
    print("[1] marionette  [2] lamp   drag a part to swing   R reset", 4, SCREEN_H - 10, CLR_LIGHT_GREY);
}
