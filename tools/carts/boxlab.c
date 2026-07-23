/* de:meta
{
  "slug": "boxlab",
  "collection": ["physics"],
  "title": "Box Lab — a self-righting Box2D puppet playground",
  "status": "active",
  "created": "2026-07-23",
  "kind": ["tech-demo", "probe"],
  "teaches": ["rigid-body", "procedural-mesh"],
  "lineage": "Brings the playtime (LÖVE2D Box2D character editor) idea back into dreamengine: a grounded puppet — each part an auto <=8-vert convex hull + tritex texture (see puppet.c) — that TRIES TO STAY UPRIGHT via joint springs toward a standing pose plus a torso up-righting PD controller, in a little world you can shove around. Slice 1 of a Box2D playground (next: texture deform across joints, then light in-cart editing).",
  "description": {
    "summary": "A little ragdoll made of textured sprite-parts that struggles to stand up. Each limb is an auto-derived <=8-vert Box2D polygon (the puppet.c pipeline); joint springs hold a standing pose and a balance controller keeps the torso vertical, so when you grab, throw, or pelt it with crates it flails and fights its way back upright. A playground for the same ideas as the playtime editor.",
    "detail": "The self-righting is two cheap parts layered together: (1) every joint carries an angle-limited spring toward the standing rest pose (stiff legs, loose arms), so the SHAPE wants to be a standing figure; (2) a PD controller applies a torque to the torso proportional to its tilt-from-vertical and angular velocity (torque = (-KP*angle - KD*omega) * inertia), so the ROOT wants to point up — with high-friction feet planted, the two together wobble the whole body upright. Knock it over and it struggles back (from a big tumble it may not fully recover — a real get-up controller is future work). Drag any part with a mouse joint; press C to drop a crate at the cursor, K to shove the puppet, R to reset. Built on the same 'sprite alpha -> convex hull -> b2MakePolygon + tritex' parts as puppet.c.",
    "controls": "Drag any body part to fling it around. C = drop a crate at the cursor. K = shove the puppet sideways. R = reset."
  },
  "todo": [
    "A real get-up-from-prone controller (COM over support, staged recovery) so it recovers from any tumble.",
    "Slice 2: texture that deforms ACROSS joints (DQS-lite skinning), the playtime meshusert idea.",
    "Slice 3: light in-cart editing — spawn shapes, pin joints, drag anchors (the box2d-editor seed)."
  ]
}
de:meta */
#include "studio.h"
#include "box2d/box2d.h"      // opt-in: make-cart/play link the vendored Box2D v3
#include "boxrig.h"           // shared sprite -> <=8-vert hull + textured draw
#include <math.h>
#include <stdio.h>            // snprintf

#define PPM     22.0f
#define SX(wx)  ((int)((wx) * PPM))
#define SY(wy)  ((int)(SCREEN_H - (wy) * PPM))
#define WX(sx)  ((sx) / PPM)
#define WY(sy)  ((SCREEN_H - (sy)) / PPM)
#define DEG     (3.14159265f / 180.0f)
#define FLOOR_PY 186          // floor line (screen px)
#define NPART    10
#define MAXCRATE 24

// balance controller gains (torso → vertical)
#define BAL_KP  120.0f
#define BAL_KD  22.0f

// ── the rig (data) — a standing figure whose feet rest on the floor ───────────
typedef struct { const char *name; int ox,oy,w,h; float cx,cy; float fric; } PartDef;
typedef struct { int a,b; float ax,ay; float lo,hi,hz; } JointDef;

static const PartDef PART[NPART] = {
    {"torso", 16,0,20,34, 160,108, 0},        // 0 (root)
    {"uArmL", 40,0,10,24, 149,106, 0},        // 1
    {"uArmR", 40,0,10,24, 171,106, 0},        // 2
    {"uLegL", 64,0,12,28, 154,140, 0},        // 3
    {"uLegR", 64,0,12,28, 166,140, 0},        // 4
    {"lArmL", 52,0,10,22, 147,128, 0},        // 5
    {"lArmR", 52,0,10,22, 173,128, 0},        // 6
    {"lLegL", 78,0,12,26, 153,167, 2.5f},     // 7 (feet: grippy)
    {"lLegR", 78,0,12,26, 167,167, 2.5f},     // 8
    {"head",   0,0,16,16, 160, 78, 0},        // 9 (drawn last)
};
static const JointDef JOINT[9] = {
    {0,9, 160,89,  -40, 40, 4.0f},   // neck
    {0,1, 149,96, -100,100, 1.5f},   // shoulderL (loose → flails)
    {1,5, 147,118,-120,120, 1.5f},   // elbowL
    {0,2, 171,96, -100,100, 1.5f},   // shoulderR
    {2,6, 173,118,-120,120, 1.5f},   // elbowR
    {0,3, 154,127, -55, 55, 8.0f},   // hipL (stiff → holds a stance)
    {3,7, 153,154,-100,100, 8.0f},   // kneeL
    {0,4, 166,127, -55, 55, 8.0f},   // hipR
    {4,8, 167,154,-100,100, 8.0f},   // kneeR
};

typedef struct { b2BodyId body; b2Polygon poly; float pcx, pcy; } Part;
static b2WorldId world;
static b2BodyId  ground;
static Part      part[NPART];
static b2BodyId  crate[MAXCRATE]; static int ncrate = 0;
static b2JointId mjoint; static bool dragging = false;

// build one part: sprite alpha → <=8-vert hull (boxrig.h) → dynamic body + shape
static void build_part(const PartDef *pd, Part *p) {
    b2Hull bh = boxrig_hull(pd->ox, pd->oy, pd->w, pd->h, PPM, &p->pcx, &p->pcy);
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){ WX(pd->cx), WY(pd->cy) };
    p->body = b2CreateBody(world, &bd);
    p->poly = b2MakePolygon(&bh, 0.0f);
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 1.0f;
    sd.material.friction = pd->fric > 0 ? pd->fric : 0.6f;
    b2CreatePolygonShape(p->body, &sd, &p->poly);
}
static void link_joint(const JointDef *jd) {
    b2Vec2 w = { WX(jd->ax), WY(jd->ay) };
    b2RevoluteJointDef d = b2DefaultRevoluteJointDef();
    d.bodyIdA = part[jd->a].body; d.bodyIdB = part[jd->b].body;
    d.localAnchorA = b2Body_GetLocalPoint(part[jd->a].body, w);
    d.localAnchorB = b2Body_GetLocalPoint(part[jd->b].body, w);
    d.collideConnected = false;
    d.enableLimit = true; d.lowerAngle = jd->lo*DEG; d.upperAngle = jd->hi*DEG;
    d.enableSpring = true; d.hertz = jd->hz; d.dampingRatio = 0.7f; d.targetAngle = 0;
    b2CreateRevoluteJoint(world, &d);
}

static void spawn_crate(float wx, float wy) {
    if (ncrate >= MAXCRATE) return;
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody; bd.position = (b2Vec2){ wx, wy };
    b2BodyId id = b2CreateBody(world, &bd);
    b2Polygon box = b2MakeBox(0.35f, 0.35f);
    b2ShapeDef sd = b2DefaultShapeDef(); sd.density = 0.7f; sd.material.friction = 0.6f;
    b2CreatePolygonShape(id, &sd, &box);
    crate[ncrate++] = id;
}

static void build(void) {
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){ 0.0f, -10.0f };
    world = b2CreateWorld(&wd);

    b2BodyDef gd = b2DefaultBodyDef();
    ground = b2CreateBody(world, &gd);
    float W = SCREEN_W/PPM, TOP = SCREEN_H/PPM, fy = WY(FLOOR_PY);
    b2ShapeDef gs = b2DefaultShapeDef(); gs.material.friction = 1.0f;
    b2Segment floor = {{0,fy},{W,fy}};   b2CreateSegmentShape(ground, &gs, &floor);
    b2Segment lft = {{0,fy},{0,TOP}};    b2CreateSegmentShape(ground, &gs, &lft);
    b2Segment rgt = {{W,fy},{W,TOP}};    b2CreateSegmentShape(ground, &gs, &rgt);

    for (int i=0;i<NPART;i++) build_part(&PART[i], &part[i]);
    for (int j=0;j<9;j++) link_joint(&JOINT[j]);
    ncrate = 0; dragging = false;
}
static void reset(void) { b2DestroyWorld(world); build(); }

// keep the torso pointing up: a PD controller (angle + angular velocity → torque)
static void balance(void) {
    b2BodyId t = part[0].body;
    float ang = b2Rot_GetAngle(b2Body_GetRotation(t));       // 0 = upright
    float w   = b2Body_GetAngularVelocity(t);
    float I   = b2Body_GetRotationalInertia(t);
    b2Body_ApplyTorque(t, (-BAL_KP*ang - BAL_KD*w) * I, true);
}

static bool inited = false;
void update(void) {
    if (!inited) { build(); inited = true; }
    if (keyp('R')) reset();

    float mwx = WX(mouse_x()), mwy = WY(mouse_y());
    if (keyp('C')) spawn_crate(mwx, mwy);
    if (keyp('K')) b2Body_ApplyLinearImpulseToCenter(part[0].body,
                        (b2Vec2){ 2.5f * b2Body_GetMass(part[0].body), 0 }, true);

    if (mouse_pressed(MOUSE_LEFT)) {                          // grab the part under the cursor
        int best=-1; float bd=0.6f;
        for (int i=0;i<NPART;i++)                             // click INSIDE a part (boxrig.h)
            if (boxrig_point_in_body(part[i].body, &part[i].poly, mwx, mwy)) { best=i; break; }
        if (best<0) for (int i=0;i<NPART;i++){                // fallback: nearest centre
            b2Vec2 c=b2Body_GetPosition(part[i].body);
            float d=sqrtf((mwx-c.x)*(mwx-c.x)+(mwy-c.y)*(mwy-c.y));
            if(d<bd){bd=d;best=i;}
        }
        if (best>=0){
            b2MouseJointDef d=b2DefaultMouseJointDef();
            d.bodyIdA=ground; d.bodyIdB=part[best].body;
            d.target=(b2Vec2){mwx,mwy}; d.hertz=6.0f; d.dampingRatio=0.7f;
            d.maxForce=1500.0f*b2Body_GetMass(part[best].body);
            mjoint=b2CreateMouseJoint(world,&d); b2Body_SetAwake(part[best].body,true); dragging=true;
        }
    }
    if (dragging && mouse_down(MOUSE_LEFT)) b2MouseJoint_SetTarget(mjoint,(b2Vec2){mwx,mwy});
    if (mouse_released(MOUSE_LEFT) && dragging){ b2DestroyJoint(mjoint); dragging=false; }

    balance();
    b2World_Step(world, 1.0f/60.0f, 4);

#ifdef DE_TRACE
    watch("torsoTilt", "%.3f", b2Rot_GetAngle(b2Body_GetRotation(part[0].body)));
    watch("crates", "%d", ncrate);
#endif
}

static void draw_box(b2BodyId id) {
    b2Vec2 p=b2Body_GetPosition(id); b2Rot r=b2Body_GetRotation(id);
    const float h=0.35f; const float lx[4]={-h,h,h,-h}, ly[4]={-h,-h,h,h};
    int xy[8];
    for (int i=0;i<4;i++){ float wx=p.x+(r.c*lx[i]-r.s*ly[i]), wy=p.y+(r.s*lx[i]+r.c*ly[i]); xy[i*2]=SX(wx); xy[i*2+1]=SY(wy); }
    polyfill(xy,4,CLR_ORANGE);
    for (int i=0;i<4;i++){ int a=i*2,b=((i+1)%4)*2; line(xy[a],xy[a+1],xy[b],xy[b+1],CLR_BROWN); }
}

void draw(void) {
    if (!inited){ build(); inited=true; }
    cls(CLR_DARK_BLUE);
    rectfill(0, FLOOR_PY, SCREEN_W, SCREEN_H-FLOOR_PY, CLR_DARK_GREEN);

    for (int i=0;i<ncrate;i++) draw_box(crate[i]);
    for (int i=0;i<NPART;i++) boxrig_draw(part[i].body, part[i].poly, part[i].pcx, part[i].pcy, PPM, SCREEN_H);

    font(FONT_SMALL);
    print("box lab: a textured Box2D puppet that tries to stay upright", 4, 4, CLR_WHITE);
    print("drag a limb   C crate   K shove   R reset", 4, SCREEN_H-10, CLR_LIGHT_GREY);
}
