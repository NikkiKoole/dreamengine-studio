/* de:meta
{
  "slug": "boxjelly",
  "collection": ["physics"],
  "title": "Box Jelly — verlet blobs + Box2D crates + a self-righting puppet, one world",
  "status": "active",
  "created": "2026-07-23",
  "kind": ["tech-demo", "probe"],
  "teaches": ["verlet-integration", "soft-body", "rigid-body", "procedural-mesh"],
  "lineage": "The two-physics-worlds experiment, now with all three prior carts in it: dreamengine's verlet toolkit (runtime/physics.h — jelly.c soft-body blobs) AND the vendored Box2D v3 (rigid crates + the boxlab.c self-righting puppet, its parts auto-hulled+textured via boxrig.h) in ONE scene, COUPLED both ways. Box2D->verlet is hard (blob points pushed out of rigid shapes so they rest/drape — vs boxes AND the puppet's convex-polygon parts); verlet->Box2D is a capped impulse (a squishy character can shove a crate or lean on the puppet). Culmination of puppet/boxlab/boxskin.",
  "description": {
    "summary": "A Box2D + verlet PLAYGROUND: squishy textured blob characters (verlet, runtime/physics.h), hard Box2D crates, a self-righting Box2D puppet (boxlab's balance controller), one of each Box2D body type — a STATIC tilted ramp, a KINEMATIC spinner, a DYNAMIC motorised pinwheel — plus a spring-levelled SEE-SAW (revolute-pivoted plank), bouncy vs dead BALLS (circle shapes, a restitution showcase), a PRISMATIC LAUNCHER (a pad on a vertical rail that slams up and flings its load), and a cursor EXPLOSION (native b2World_Explode) — all sharing one world. The two physics engines don't know about each other, so a coupling layer glues them: blobs drape over the ledge, crates, ramp, spinner, puppet, see-saw, balls and the launcher pad; a blob's weight can shove a crate, weigh down the see-saw, or press on the puppet, which fights to stay upright. Drag a blob, crate, ball or puppet part; C crate, B blob, V ball, E boom, F fire the launcher, R reset.",
    "detail": "Two independent solvers, one world. VERLET (px space): jelly.c-style pressurized blobs (rim ring + area preservation + tritex face skin). BOX2D (metre space): floor, walls, a static ledge, dynamic crates, a 10-part revolute-jointed self-righting puppet (auto-hulled+textured by boxrig.h, torso PD controller from boxlab.c), a see-saw (a plank on a spring-levelled, angle-limited revolute joint), bouncy/dead balls (b2Circle shapes A/B'd on restitution), a prismatic launcher (a motor-driven pad on a b2PrismaticJoint rail, auto-firing on a timer / F), and a cursor detonation via native b2World_Explode. Intermingling is a per-frame coupling: every verlet rim point is resolved against each Box2D shape (point->metres, push-out, back->px) — the HARD direction — for oriented boxes (crates/ledge/see-saw), the puppet's convex-polygon parts, and the balls (circle push-out); the reverse direction applies a CAPPED impulse so a blob can nudge them without the two solvers exploding. The explosion hits every dynamic body via b2World_Explode and kicks the verlet rim points by hand (they live outside Box2D). Blob-vs-blob stays SAT. Shows the seam where a code-first particle system and an off-the-shelf rigid-body lib meet.",
    "controls": "Drag a blob rim, a crate, or a ball to fling it. C = drop a crate. B = drop a blob. V = drop a bouncy ball. E = explosion at the cursor. F = fire the launcher. R = reset."
  },
  "todo": [
    "Two-way friction/rolling so a blob can roll a crate, not just shove it.",
    "Verlet ragdoll characters (not just blobs) intermingling — needs limb skinning (see boxskin)."
  ]
}
de:meta */
#include "studio.h"
#include "physics.h"          // verlet toolkit (blobs)
#include "box2d/box2d.h"      // vendored Box2D v3 (rigid crates + the puppet)
#include "boxrig.h"           // sprite -> hull + textured draw + verlet<->box2d coupling

#define PPM     22.0f
#define pxX(m)  ((m) * PPM)
#define pxY(m)  (SCREEN_H - (m) * PPM)
#define mX(px)  ((px) / PPM)
#define mY(px)  ((SCREEN_H - (px)) / PPM)

// ── verlet blobs (jelly.c mechanics) ─────────────────────────────────────────
#define NB       6
#define NR       14
#define R0       13.0f
#define GRAV     0.34f
#define DAMP     0.93f
#define ITERS    3
#define SUBSTEPS 4
#define PRESSURE 0.18f
#define FLOORY   (SCREEN_H - 8)
#define WALLX    2.0f
#define WORLD_CX 160.0f          // px — the ledge/crate/blob cluster is PINNED here (not SCREEN_W*0.5),
                                 // so widening the canvas only opens floor on the right (the launcher bay)
#define SHAPE_CIRCLE 0
#define SHAPE_RRECT  1

typedef struct {
    PhysPt hub, rim[NR];
    float  texU[NR], texV[NR], edgeRest[NR], restArea;
    int    skin;
} Blob;
static Blob blob[NB]; static int nblob = 0;

static float vlen(float x, float y) { return fsqrt(x*x + y*y); }
static float poly_area(Blob *b) {
    float A = 0; for (int k=0;k<NR;k++){ int j=(k+1)%NR; A += b->rim[k].x*b->rim[j].y - b->rim[j].x*b->rim[k].y; } return 0.5f*A;
}
static void set_hub(Blob *b) {
    float sx=0,sy=0; for (int k=0;k<NR;k++){ sx+=b->rim[k].x; sy+=b->rim[k].y; } b->hub.x=sx/NR; b->hub.y=sy/NR;
}
static void shape_pt(int shape, float t, float *ox, float *oy) {
    float a=t*360.0f, c=cos_deg(a), s=sin_deg(a);
    if (shape==SHAPE_RRECT){ float ac=c<0?-c:c, as=s<0?-s:s; *ox=R0*1.40f*(c<0?-1:1)*fsqrt(ac); *oy=R0*0.80f*(s<0?-1:1)*fsqrt(as); }
    else { *ox=c*R0; *oy=s*R0; }
}
static void shape_uv(int shape, float t, float *u, float *v) {
    float a=t*360.0f, c=cos_deg(a), s=sin_deg(a);
    if (shape==SHAPE_RRECT){ float ac=c<0?-c:c, as=s<0?-s:s; *u=8.0f+7.2f*(c<0?-1:1)*fsqrt(ac); *v=8.0f+7.2f*(s<0?-1:1)*fsqrt(as); }
    else { *u=8.0f+c*6.0f; *v=8.0f+s*6.0f; }
}
static void spawn_blob(float cx, float cy, int shape, int skin) {
    if (nblob >= NB) return;
    Blob *b = &blob[nblob++];
    for (int k=0;k<NR;k++){ float ox,oy; shape_pt(shape,(float)k/NR,&ox,&oy); phys_pt(&b->rim[k], cx+ox, cy+oy, 1.0f, 2.0f); shape_uv(shape,(float)k/NR,&b->texU[k],&b->texV[k]); }
    for (int k=0;k<NR;k++){ int j=(k+1)%NR; b->edgeRest[k]=vlen(b->rim[j].x-b->rim[k].x, b->rim[j].y-b->rim[k].y); }
    set_hub(b); b->restArea=poly_area(b); b->skin=skin;
}
static void blob_vs_point(PhysPt *P, Blob *B) {   // SAT push-out (jelly.c)
    float maxSD=-1e9f, mnx=0, mny=0; int me=-1;
    for (int e=0;e<NR;e++){ int e2=(e+1)%NR; float ax=B->rim[e].x, ay=B->rim[e].y, ex=B->rim[e2].x-ax, ey=B->rim[e2].y-ay; float el=vlen(ex,ey); if(el<0.001f)continue;
        float nx=ey/el, ny=-ex/el, midx=ax+ex*0.5f, midy=ay+ey*0.5f; if((midx-B->hub.x)*nx+(midy-B->hub.y)*ny<0){nx=-nx;ny=-ny;}
        float sd=(P->x-ax)*nx+(P->y-ay)*ny; if(sd>maxSD){maxSD=sd;mnx=nx;mny=ny;me=e;} }
    if (me<0||maxSD>=P->r) return;
    float push=P->r-maxSD; int e2=(me+1)%NR; PhysPt *a=&B->rim[me],*b=&B->rim[e2];
    if (P->w){ P->x+=mnx*push*0.5f; P->y+=mny*push*0.5f; float tx=-mny,ty=mnx,vt=(P->x-P->px)*tx+(P->y-P->py)*ty; P->px+=tx*vt*0.35f; P->py+=ty*vt*0.35f; }
    if (a->w){ a->x-=mnx*push*0.25f; a->y-=mny*push*0.25f; }
    if (b->w){ b->x-=mnx*push*0.25f; b->y-=mny*push*0.25f; }
}

// ── Box2D rigid world ────────────────────────────────────────────────────────
#define MAXCRATE 24
#define CH       0.34f          // crate half-size (metres)
static b2WorldId world;
static b2BodyId  ground, ledge, crate[MAXCRATE]; static int ncrate = 0;
// per-crate coupling impulse accumulator (metres) + representative contact point
static float accX[MAXCRATE], accY[MAXCRATE], accPX[MAXCRATE], accPY[MAXCRATE]; static int accN[MAXCRATE];
#define K_COUPLE 1.4f           // verlet->box2d push strength
#define K_CAPMAG 0.06f          // max impulse magnitude per crate per frame

// the three Box2D body types, on show (metres, half-extents)
static b2BodyId ramp;    // STATIC   — a fixed tilted block (static bodies can't spin)
static b2BodyId spinner; // KINEMATIC — turns at a constant rate, immune to forces
static b2BodyId pinwheel;// DYNAMIC  — spun by a joint motor, but collisions can fight it
#define RAMP_HW 0.90f
#define RAMP_HH 0.16f
#define SPIN_HL 0.85f
#define SPIN_HT 0.14f
#define PIN_HL  0.60f
#define PIN_HT  0.11f
// a see-saw: a plank on a free revolute pivot (spring-levelled + angle-limited) — drop a crate on
// one end to launch a blob off the other. Teaches torque + mass with the bodies already in the scene.
static b2BodyId seesaw;
#define SAW_HW  1.50f
#define SAW_HH  0.12f
#define SAW_CX  46.0f          // screen px — the open left floor, clear of the ledge
#define SAW_PIV 22.0f          // pivot height above the floor (px) — enough travel for the ends
// a PRISMATIC launcher: a pad locked to a vertical rail; a motor slams it up its stroke to boop
// whatever rests on it, then retracts. The one joint type not shown in any other Box2D cart.
static b2BodyId  launcher; static b2JointId launchJoint;
static int  launchT = 90;      // frames until the next auto-fire
static bool launchUp = false;  // mid-stroke (driving up)
#define LAUNCH_HW  0.66f
#define LAUNCH_HH  0.10f
#define LAUNCH_CX  355.0f      // px — the open right bay the widened canvas opened up
#define LAUNCH_TOP 1.05f       // stroke length (metres)

static b2BodyId make_crate(float mx, float my) {
    b2BodyDef bd = b2DefaultBodyDef(); bd.type=b2_dynamicBody; bd.position=(b2Vec2){mx,my};
    b2BodyId id = b2CreateBody(world, &bd);
    b2Polygon box = b2MakeBox(CH, CH);
    b2ShapeDef sd = b2DefaultShapeDef(); sd.density=0.6f; sd.material.friction=0.5f;
    b2CreatePolygonShape(id, &sd, &box);
    return id;
}
static void spawn_crate(float mx, float my) { if (ncrate<MAXCRATE) crate[ncrate++] = make_crate(mx,my); }

// ── bouncy balls (CIRCLE shapes — a restitution showcase: springy vs dead) ────
#define MAXBALL 10
typedef struct { b2BodyId body; float r; int bouncy; } Ball;   // r in metres
static Ball ball[MAXBALL]; static int nball = 0;
static void spawn_ball(float mx, float my, float r_m, int bouncy) {
    if (nball>=MAXBALL) return;
    b2BodyDef bd=b2DefaultBodyDef(); bd.type=b2_dynamicBody; bd.position=(b2Vec2){mx,my};
    b2BodyId id=b2CreateBody(world,&bd);
    b2Circle c={{0,0}, r_m};
    b2ShapeDef sd=b2DefaultShapeDef(); sd.density=0.7f; sd.material.friction=0.4f;
    sd.material.restitution = bouncy ? 0.82f : 0.04f;   // the whole point: one springs, one thuds
    b2CreateCircleShape(id,&sd,&c);
    ball[nball++]=(Ball){ id, r_m, bouncy };
}


// ── the self-righting puppet (boxlab.c rig) dropped into this shared world ─────
#define NPART   10
#define PUP_OX  (-72.0f)       // stand the figure left of the crates/ledge
#define PUP_OY  8.0f
#define DEG     (3.14159265f/180.0f)
#define BAL_KP  120.0f
#define BAL_KD  22.0f
typedef struct { const char *name; int ox,oy,w,h; float cx,cy; float fric; } PartDef;
typedef struct { int a,b; float ax,ay; float lo,hi,hz; } JointDef;
static const PartDef PART[NPART] = {
    {"torso", 16,0,20,34, 160,108, 0},        // 0 (root)
    {"uArmL", 40,0,10,24, 149,106, 0},
    {"uArmR", 40,0,10,24, 171,106, 0},
    {"uLegL", 64,0,12,28, 154,140, 0},
    {"uLegR", 64,0,12,28, 166,140, 0},
    {"lArmL", 52,0,10,22, 147,128, 0},
    {"lArmR", 52,0,10,22, 173,128, 0},
    {"lLegL", 78,0,12,26, 153,167, 2.5f},     // feet: grippy
    {"lLegR", 78,0,12,26, 167,167, 2.5f},
    {"head",   0,0,16,16, 160, 78, 0},
};
static const JointDef JOINT[9] = {
    {0,9, 160,89,  -40, 40, 4.0f},   // neck
    {0,1, 149,96, -100,100, 1.5f}, {1,5, 147,118,-120,120, 1.5f},  // L arm
    {0,2, 171,96, -100,100, 1.5f}, {2,6, 173,118,-120,120, 1.5f},  // R arm
    {0,3, 154,127, -55, 55, 8.0f}, {3,7, 153,154,-100,100, 8.0f},  // L leg (stiff)
    {0,4, 166,127, -55, 55, 8.0f}, {4,8, 167,154,-100,100, 8.0f},  // R leg
};
typedef struct { b2BodyId body; b2Polygon poly; float pcx, pcy; } Part;
static Part part[NPART];

static void build_puppet(void) {
    for (int i=0;i<NPART;i++){ const PartDef *pd=&PART[i]; Part *p=&part[i];
        b2Hull bh = boxrig_hull(pd->ox, pd->oy, pd->w, pd->h, PPM, &p->pcx, &p->pcy);
        b2BodyDef bd=b2DefaultBodyDef(); bd.type=b2_dynamicBody;
        bd.position=(b2Vec2){ mX(pd->cx+PUP_OX), mY(pd->cy+PUP_OY) };
        p->body=b2CreateBody(world,&bd);
        p->poly=b2MakePolygon(&bh,0.0f);
        b2ShapeDef sd=b2DefaultShapeDef(); sd.density=1.0f; sd.material.friction = pd->fric>0?pd->fric:0.6f;
        b2CreatePolygonShape(p->body,&sd,&p->poly);
    }
    for (int j=0;j<9;j++){ const JointDef *jd=&JOINT[j];
        b2Vec2 w={ mX(jd->ax+PUP_OX), mY(jd->ay+PUP_OY) };
        b2RevoluteJointDef d=b2DefaultRevoluteJointDef();
        d.bodyIdA=part[jd->a].body; d.bodyIdB=part[jd->b].body;
        d.localAnchorA=b2Body_GetLocalPoint(part[jd->a].body,w);
        d.localAnchorB=b2Body_GetLocalPoint(part[jd->b].body,w);
        d.collideConnected=false;
        d.enableLimit=true; d.lowerAngle=jd->lo*DEG; d.upperAngle=jd->hi*DEG;
        d.enableSpring=true; d.hertz=jd->hz; d.dampingRatio=0.7f; d.targetAngle=0;
        b2CreateRevoluteJoint(world,&d);
    }
}
static void balance(void) {                          // keep the torso pointing up (PD)
    b2BodyId t=part[0].body;
    float ang=b2Rot_GetAngle(b2Body_GetRotation(t)), w=b2Body_GetAngularVelocity(t), I=b2Body_GetRotationalInertia(t);
    b2Body_ApplyTorque(t, (-BAL_KP*ang - BAL_KD*w)*I, true);
}
static void build(void) {
    b2WorldDef wd = b2DefaultWorldDef(); wd.gravity=(b2Vec2){0,-10}; world=b2CreateWorld(&wd);
    b2BodyDef gd = b2DefaultBodyDef(); ground=b2CreateBody(world,&gd);
    float W=mX(SCREEN_W), TOP=mY(0), fy=mY(FLOORY);
    b2ShapeDef gs=b2DefaultShapeDef(); gs.material.friction=0.8f;
    b2Segment fl={{0,fy},{W,fy}}; b2CreateSegmentShape(ground,&gs,&fl);
    b2Segment lf={{mX(WALLX),fy},{mX(WALLX),TOP}}; b2CreateSegmentShape(ground,&gs,&lf);
    b2Segment rt={{mX(SCREEN_W-WALLX),fy},{mX(SCREEN_W-WALLX),TOP}}; b2CreateSegmentShape(ground,&gs,&rt);
    // a static ledge for blobs + crates to rest on / drape over
    b2BodyDef ld=b2DefaultBodyDef(); ld.position=(b2Vec2){mX(WORLD_CX), mY(FLOORY-46)};
    ledge=b2CreateBody(world,&ld);
    b2Polygon lb=b2MakeBox(mX(48)-mX(0), mX(6)-mX(0));      // half 48px wide, 6px tall
    b2ShapeDef ls=b2DefaultShapeDef(); ls.material.friction=0.8f; b2CreatePolygonShape(ledge,&ls,&lb);

    ncrate=0; nblob=0; nball=0;
    float ledgeTop = mY(FLOORY-46-6);                       // metres, top face of the ledge
    spawn_crate(mX(WORLD_CX-15), ledgeTop+CH);              // a 2+1 stack ON the ledge
    spawn_crate(mX(WORLD_CX+15), ledgeTop+CH);
    spawn_crate(mX(WORLD_CX),    ledgeTop+CH*3.05f);
    spawn_blob(WORLD_CX-14, 34, SHAPE_CIRCLE, 0);           // blobs dropped onto the stack
    spawn_blob(WORLD_CX+16, 18, SHAPE_RRECT,  1);
    spawn_blob(WORLD_CX,    52, SHAPE_CIRCLE, 2);
    build_puppet();                                        // a self-righting figure stands to the left

    // ── the three body types (right side of the world) ──
    b2ShapeDef ds = b2DefaultShapeDef(); ds.material.friction = 0.6f;
    // STATIC: a fixed tilted ramp things slide down (static bodies never move — so no spin)
    b2BodyDef rd = b2DefaultBodyDef(); rd.position = (b2Vec2){ mX(286), mY(FLOORY-16) }; rd.rotation = b2MakeRot(-0.38f);
    ramp = b2CreateBody(world, &rd);
    b2Polygon rp = b2MakeBox(RAMP_HW, RAMP_HH); b2CreatePolygonShape(ramp, &ds, &rp);
    // KINEMATIC: a floor-level turnstile spinning at a constant rate (sweeps blobs + crates)
    b2BodyDef kd = b2DefaultBodyDef(); kd.type = b2_kinematicBody; kd.position = (b2Vec2){ mX(238), mY(FLOORY-22) };
    spinner = b2CreateBody(world, &kd);
    b2Polygon kp = b2MakeBox(SPIN_HL, SPIN_HT); b2CreatePolygonShape(spinner, &ds, &kp);
    b2Body_SetAngularVelocity(spinner, 1.8f);
    // DYNAMIC: a pinwheel driven by a joint MOTOR, but heavy hits can slow/stall it
    b2BodyDef pd = b2DefaultBodyDef(); pd.type = b2_dynamicBody; pd.position = (b2Vec2){ mX(266), mY(FLOORY-96) };
    pinwheel = b2CreateBody(world, &pd);
    b2Polygon pp = b2MakeBox(PIN_HL, PIN_HT); b2ShapeDef ps = b2DefaultShapeDef(); ps.density = 0.8f; ps.material.friction = 0.5f;
    b2CreatePolygonShape(pinwheel, &ps, &pp);
    b2RevoluteJointDef pj = b2DefaultRevoluteJointDef();
    pj.bodyIdA = ground; pj.bodyIdB = pinwheel;
    b2Vec2 pc = b2Body_GetPosition(pinwheel);
    pj.localAnchorA = b2Body_GetLocalPoint(ground, pc); pj.localAnchorB = (b2Vec2){0,0};
    pj.enableMotor = true; pj.motorSpeed = 3.0f; pj.maxMotorTorque = 9.0f; pj.collideConnected = false;
    b2CreateRevoluteJoint(world, &pj);

    // ── SEE-SAW: a plank pivoted at its centre on a free revolute joint ──
    b2BodyDef sawd=b2DefaultBodyDef(); sawd.type=b2_dynamicBody;
    sawd.position=(b2Vec2){ mX(SAW_CX), mY(FLOORY-SAW_PIV) };
    seesaw=b2CreateBody(world,&sawd);
    b2Polygon sp=b2MakeBox(SAW_HW,SAW_HH);
    b2ShapeDef saws=b2DefaultShapeDef(); saws.density=0.5f; saws.material.friction=0.7f;
    b2CreatePolygonShape(seesaw,&saws,&sp);
    b2RevoluteJointDef sj=b2DefaultRevoluteJointDef();
    sj.bodyIdA=ground; sj.bodyIdB=seesaw;
    b2Vec2 sc=b2Body_GetPosition(seesaw);
    sj.localAnchorA=b2Body_GetLocalPoint(ground,sc); sj.localAnchorB=(b2Vec2){0,0};
    sj.collideConnected=false;
    sj.enableSpring=true; sj.hertz=1.1f; sj.dampingRatio=0.6f; sj.targetAngle=0;   // self-levels when empty
    sj.enableLimit=true; sj.lowerAngle=-30*DEG; sj.upperAngle=30*DEG;               // ends stay above the floor
    b2CreateRevoluteJoint(world,&sj);

    // two balls to show restitution: a springy one that lands on the see-saw, a dead one that thuds
    spawn_ball(mX(34),  mY(40), 0.28f, 1);   // bouncy → lands on the see-saw, tips it, bounces
    spawn_ball(mX(305), mY(30), 0.30f, 0);   // dead → thuds down by the ramp on the right (clear of the puppet)

    // ── PRISMATIC LAUNCHER: a pad on a vertical rail, motor-slammed up to fling its load ──
    b2BodyDef lad=b2DefaultBodyDef(); lad.type=b2_dynamicBody;
    lad.position=(b2Vec2){ mX(LAUNCH_CX), mY(FLOORY-8) };
    launcher=b2CreateBody(world,&lad);
    b2Polygon lp=b2MakeBox(LAUNCH_HW,LAUNCH_HH);
    b2ShapeDef lsh=b2DefaultShapeDef(); lsh.density=1.4f; lsh.material.friction=0.8f;
    b2CreatePolygonShape(launcher,&lsh,&lp);
    b2PrismaticJointDef ljd=b2DefaultPrismaticJointDef();
    ljd.bodyIdA=ground; ljd.bodyIdB=launcher;
    b2Vec2 lc=b2Body_GetPosition(launcher);
    ljd.localAnchorA=b2Body_GetLocalPoint(ground,lc); ljd.localAnchorB=(b2Vec2){0,0};
    ljd.localAxisA=(b2Vec2){0,1};                       // slides straight up (world +Y)
    ljd.enableLimit=true; ljd.lowerTranslation=0.0f; ljd.upperTranslation=LAUNCH_TOP;
    ljd.enableMotor=true; ljd.maxMotorForce=900.0f; ljd.motorSpeed=-3.0f;   // rests parked at the bottom
    launchJoint=b2CreatePrismaticJoint(world,&ljd);
    launchT=90; launchUp=false;
    spawn_crate(mX(LAUNCH_CX), mY(FLOORY-24));           // a crate parked on the pad, ready to be flung
}
static void reset(void) { b2DestroyWorld(world); build(); }

// grab state
static int grabBl=-1, grabK=-1; static float gw, pmx, pmy;
static b2JointId mjoint; static bool dragCrate=false;
static float LHW, LHH;   // ledge half-size cache (metres)
static int boomF=0; static int boomX, boomY;   // explosion: frames left + screen-px centre (for the ring)
#define BOOM_LIFE 11
#define BOOM_R    2.2f   // metres — blast radius (also the verlet-kick reach)

static bool inited=false;
void update(void) {
    if (!inited){ build(); LHW=mX(48)-mX(0); LHH=mX(6)-mX(0); inited=true; }
    float mx=(float)mouse_x(), my=(float)mouse_y(), mwx=mX(mx), mwy=mY(my);
    if (keyp('R')) reset();
    if (keyp('C')) spawn_crate(mwx,mwy);
    if (keyp('B')) spawn_blob(mx,my, SHAPE_CIRCLE, (nblob*7)%3);
    if (keyp('V')) spawn_ball(mwx,mwy, 0.28f, 1);            // drop a bouncy ball at the cursor
    if (keyp('E')) {                                        // BOOM — radial impulse at the cursor
        b2ExplosionDef ed=b2DefaultExplosionDef();
        ed.position=(b2Vec2){mwx,mwy}; ed.radius=BOOM_R; ed.falloff=1.3f; ed.impulsePerLength=4.5f;
        b2World_Explode(world,&ed);                         // hits every DYNAMIC body (crates, balls, pinwheel, puppet, see-saw)
        float RAD=BOOM_R*PPM, RAD2=RAD*RAD;                 // verlet blobs live outside Box2D → kick them by hand
        for (int i=0;i<nblob;i++) for (int k=0;k<NR;k++){ PhysPt *P=&blob[i].rim[k]; if(!P->w)continue;
            float dx=P->x-mx, dy=P->y-my, d2=dx*dx+dy*dy;
            if (d2<RAD2 && d2>1.0f){ float d=fsqrt(d2), f=(1.0f-d/RAD)*7.0f; P->px-=dx/d*f; P->py-=dy/d*f; } }
        boomX=(int)mx; boomY=(int)my; boomF=BOOM_LIFE; shake(5.0f);
    }
    // launcher: auto-fire on a timer (or F) — slam the pad up, retract once it tops out
    if (keyp('F') || --launchT<=0){ b2PrismaticJoint_SetMotorSpeed(launchJoint, 11.0f); launchUp=true; launchT=150; }
    if (launchUp && b2PrismaticJoint_GetTranslation(launchJoint) >= LAUNCH_TOP-0.05f){
        b2PrismaticJoint_SetMotorSpeed(launchJoint,-4.0f); launchUp=false; }

    if (mouse_pressed(MOUSE_LEFT)) {
        float best=16.0f; grabBl=-1;                        // try a blob rim point first
        for (int i=0;i<nblob;i++) for (int k=0;k<NR;k++){ float d=vlen(mx-blob[i].rim[k].x,my-blob[i].rim[k].y); if(d<best){best=d;grabBl=i;grabK=k;} }
        if (grabBl>=0){ gw=blob[grabBl].rim[grabK].w; blob[grabBl].rim[grabK].w=0; }
        else {                                              // else grab a crate, else a puppet part
            b2BodyId pick; bool got=false; int bi=-1; float bd=0.6f;
            for (int i=0;i<ncrate;i++){ b2Vec2 c=b2Body_GetPosition(crate[i]); float d=vlen(mwx-c.x,mwy-c.y); if(d<bd){bd=d;bi=i;} }
            if (bi>=0){ pick=crate[bi]; got=true; }
            for (int i=0;i<nball && !got;i++){ b2Vec2 c=b2Body_GetPosition(ball[i].body);   // then a ball (grab within its disc)
                if (vlen(mwx-c.x,mwy-c.y) < ball[i].r+0.15f){ pick=ball[i].body; got=true; } }
            if (!got) for (int q=0;q<NPART;q++) if (boxrig_point_in_body(part[q].body,&part[q].poly,mwx,mwy)){ pick=part[q].body; got=true; break; }
            if (got){ b2MouseJointDef d=b2DefaultMouseJointDef(); d.bodyIdA=ground; d.bodyIdB=pick; d.target=(b2Vec2){mwx,mwy}; d.hertz=6; d.dampingRatio=0.7f; d.maxForce=1400.0f*b2Body_GetMass(pick); mjoint=b2CreateMouseJoint(world,&d); b2Body_SetAwake(pick,true); dragCrate=true; }
        }
    }
    if (grabBl>=0 && mouse_down(MOUSE_LEFT)) phys_grab(&blob[grabBl].rim[grabK], mx, my, pmx, pmy);
    if (dragCrate && mouse_down(MOUSE_LEFT)) b2MouseJoint_SetTarget(mjoint,(b2Vec2){mwx,mwy});
    if (mouse_released(MOUSE_LEFT)) {
        if (grabBl>=0){ blob[grabBl].rim[grabK].w=gw; grabBl=-1; }
        if (dragCrate){ b2DestroyJoint(mjoint); dragCrate=false; }
    }

    for (int c=0;c<ncrate;c++){ accX[c]=accY[c]=accPX[c]=accPY[c]=0; accN[c]=0; }

    for (int s=0;s<SUBSTEPS;s++){
        for (int i=0;i<nblob;i++) for (int k=0;k<NR;k++) phys_integrate(&blob[i].rim[k], 0, GRAV/SUBSTEPS, DAMP);
        for (int it=0;it<ITERS;it++){
            for (int i=0;i<nblob;i++){ Blob *b=&blob[i];
                for (int k=0;k<NR;k++) phys_link(&b->rim[k], &b->rim[(k+1)%NR], b->edgeRest[k]);
                float corr=PRESSURE*(b->restArea-poly_area(b))/b->restArea;
                for (int k=0;k<NR;k++){ if(b->rim[k].w==0)continue; int kp=(k+NR-1)%NR,kn=(k+1)%NR; b->rim[k].x+=0.5f*(b->rim[kn].y-b->rim[kp].y)*corr; b->rim[k].y+=0.5f*(b->rim[kp].x-b->rim[kn].x)*corr; }
            }
            for (int i=0;i<nblob;i++) set_hub(&blob[i]);
            for (int i=0;i<nblob;i++) for (int j=0;j<nblob;j++){ if(i==j)continue; for (int p=0;p<NR;p++) blob_vs_point(&blob[i].rim[p], &blob[j]); }
            bool lastPass = (s==SUBSTEPS-1 && it==ITERS-1);
            for (int i=0;i<nblob;i++) for (int k=0;k<NR;k++){
                PhysPt *P=&blob[i].rim[k];
                boxrig_resolve_box(P, ledge, LHW, LHH, PPM, SCREEN_H);          // static: push-out only
                boxrig_resolve_box(P, ramp, RAMP_HW, RAMP_HH, PPM, SCREEN_H);    // static ramp
                boxrig_resolve_box(P, spinner, SPIN_HL, SPIN_HT, PPM, SCREEN_H); // kinematic: sweeps, no reaction
                { BrHit h = boxrig_resolve_box(P, pinwheel, PIN_HL, PIN_HT, PPM, SCREEN_H);   // dynamic: can be fought
                  if (h.hit && lastPass){ float im=1.2f*h.pen; if(im>0.05f)im=0.05f; b2Body_ApplyLinearImpulse(pinwheel,(b2Vec2){-h.nx*im,-h.ny*im},(b2Vec2){h.cx,h.cy},true); } }
                for (int c=0;c<ncrate;c++){                                     // crates: accumulate reaction
                    BrHit h = boxrig_resolve_box(P, crate[c], CH, CH, PPM, SCREEN_H);
                    if (h.hit && lastPass){ accX[c]-=h.nx*h.pen; accY[c]-=h.ny*h.pen; accPX[c]+=h.cx; accPY[c]+=h.cy; accN[c]++; }
                }
                for (int q=0;q<NPART;q++){                                      // puppet: immediate capped impulse
                    BrHit h = boxrig_resolve_poly(P, part[q].body, &part[q].poly, PPM, SCREEN_H);
                    if (h.hit && lastPass){ float im=1.2f*h.pen; if(im>0.05f)im=0.05f; b2Body_ApplyLinearImpulse(part[q].body,(b2Vec2){-h.nx*im,-h.ny*im},(b2Vec2){h.cx,h.cy},true); }
                }
                { BrHit h = boxrig_resolve_box(P, seesaw, SAW_HW, SAW_HH, PPM, SCREEN_H);   // see-saw: dynamic, blobs can weigh it down
                  if (h.hit && lastPass){ float im=1.2f*h.pen; if(im>0.05f)im=0.05f; b2Body_ApplyLinearImpulse(seesaw,(b2Vec2){-h.nx*im,-h.ny*im},(b2Vec2){h.cx,h.cy},true); } }
                { BrHit h = boxrig_resolve_box(P, launcher, LAUNCH_HW, LAUNCH_HH, PPM, SCREEN_H);   // launcher pad (blobs ride it up too)
                  if (h.hit && lastPass){ float im=1.2f*h.pen; if(im>0.05f)im=0.05f; b2Body_ApplyLinearImpulse(launcher,(b2Vec2){-h.nx*im,-h.ny*im},(b2Vec2){h.cx,h.cy},true); } }
                for (int bi2=0;bi2<nball;bi2++){                                 // balls: circle push-out + capped reaction
                    b2Vec2 c=b2Body_GetPosition(ball[bi2].body);
                    float cxp=pxX(c.x), cyp=pxY(c.y), rr=ball[bi2].r*PPM+P->r;
                    float dx=P->x-cxp, dy=P->y-cyp, d2=dx*dx+dy*dy;
                    if (d2 < rr*rr && d2 > 0.01f){ float d=fsqrt(d2), pen=rr-d, nsx=dx/d, nsy=dy/d;   // screen-space outward normal
                        if (P->w){ P->x+=nsx*pen; P->y+=nsy*pen; }
                        if (lastPass){ float im=1.2f*(pen/PPM); if(im>0.05f)im=0.05f;   // world normal flips Y; react = shove the ball off
                            b2Body_ApplyLinearImpulse(ball[bi2].body,(b2Vec2){-nsx*im, nsy*im},(b2Vec2){c.x,c.y},true); } }
                }
                phys_bounds(P, WALLX, 0, SCREEN_W-WALLX, FLOORY, 0.3f, 0.6f);
            }
        }
    }
    for (int i=0;i<nblob;i++) set_hub(&blob[i]);

    // apply the accumulated coupling impulses to crates (capped)
    for (int c=0;c<ncrate;c++){
        if (accN[c]==0) continue;
        float ix=accX[c]*K_COUPLE, iy=accY[c]*K_COUPLE, m=vlen(ix,iy);
        if (m>K_CAPMAG){ ix*=K_CAPMAG/m; iy*=K_CAPMAG/m; }
        b2Body_ApplyLinearImpulse(crate[c], (b2Vec2){ix,iy}, (b2Vec2){accPX[c]/accN[c], accPY[c]/accN[c]}, true);
    }
    balance();                          // the puppet fights to stay upright
    b2World_Step(world, 1.0f/60.0f, 4);
    pmx=mx; pmy=my;
    if (boomF>0) boomF--;               // explosion ring lifetime

#ifdef DE_TRACE
    watch("blobs","%d",nblob); watch("crates","%d",ncrate);
#endif
}

// oriented box (any half-extents/colour), transformed by the body — reads its live rotation
static void draw_obox(b2BodyId id, float hw, float hh, int fill, int edge) {
    b2Vec2 p=b2Body_GetPosition(id); b2Rot r=b2Body_GetRotation(id);
    const float lx[4]={-hw,hw,hw,-hw}, ly[4]={-hh,-hh,hh,hh}; int xy[8];
    for (int i=0;i<4;i++){ float wx=p.x+(r.c*lx[i]-r.s*ly[i]), wy=p.y+(r.s*lx[i]+r.c*ly[i]); xy[i*2]=(int)pxX(wx); xy[i*2+1]=(int)pxY(wy); }
    polyfill(xy,4,fill);
    poly(xy,4,edge);   // coverage stroke (not line()) so the outline hugs the coverage fill at EVERY angle — a DDA line() drifts off the fill boundary on rotated edges in the software canvas (HW hides it)
}
static void draw_crate(b2BodyId id) {
    b2Vec2 p=b2Body_GetPosition(id); b2Rot r=b2Body_GetRotation(id);
    draw_obox(id, CH, CH, CLR_ORANGE, CLR_BROWN);
    const float lx[4]={-CH,CH,CH,-CH}, ly[4]={-CH,-CH,CH,CH}; int xy[8];
    for (int i=0;i<4;i++){ float wx=p.x+(r.c*lx[i]-r.s*ly[i]), wy=p.y+(r.s*lx[i]+r.c*ly[i]); xy[i*2]=(int)pxX(wx); xy[i*2+1]=(int)pxY(wy); }
    // X brace — nudge each corner 1px toward the box centre before joining, so a line() endpoint
    // lands INSIDE the coverage fill (whose far corner is x1-1,y1-1) instead of on the raw geometric
    // corner (x1,y1) — otherwise the diagonals poke a 1px "foot" past the box on the software canvas.
    int cx=(int)pxX(p.x), cy=(int)pxY(p.y), bx[4], by[4];
    for (int i=0;i<4;i++){ bx[i]=xy[i*2]+(xy[i*2]>cx?-1:xy[i*2]<cx?1:0); by[i]=xy[i*2+1]+(xy[i*2+1]>cy?-1:xy[i*2+1]<cy?1:0); }
    line(bx[0],by[0],bx[2],by[2],CLR_BROWN); line(bx[1],by[1],bx[3],by[3],CLR_BROWN);   // X brace
}
// a ball: a filled disc + a spin spoke (so you can see it roll). Pink = bouncy, grey = dead.
static void draw_ball(Ball *b) {
    b2Vec2 p=b2Body_GetPosition(b->body); b2Rot r=b2Body_GetRotation(b->body);
    int cx=(int)pxX(p.x), cy=(int)pxY(p.y), rr=(int)(b->r*PPM);
    circfill(cx,cy,rr, b->bouncy?CLR_PINK:CLR_DARKER_GREY);
    circ(cx,cy,rr, CLR_WHITE);
    line(cx,cy, cx+(int)(r.c*(rr-1)), cy-(int)(r.s*(rr-1)), CLR_WHITE);   // spoke ends 1px inside the rim (screen Y flipped)
}
// label a body with tiny text above its centre
static void label(b2BodyId id, const char *s) {
    b2Vec2 p=b2Body_GetPosition(id);
    print(s, (int)pxX(p.x)-text_width(s)/2, (int)pxY(p.y)-16, CLR_LIGHT_GREY);
}
#define FACE_V 64.0f                 // blob face discs live on sheet row 4 (y=64)
static void draw_blob(Blob *b) {
    float u0=b->skin*16.0f;
    for (int k=0;k<NR;k++){ int k2=(k+1)%NR;
        tritex((int)b->hub.x,(int)b->hub.y, u0+8.0f, FACE_V+8.0f,
               (int)b->rim[k].x,(int)b->rim[k].y, u0+b->texU[k], FACE_V+b->texV[k],
               (int)b->rim[k2].x,(int)b->rim[k2].y, u0+b->texU[k2], FACE_V+b->texV[k2]);
    }
}

void draw(void) {
    if (!inited){ build(); LHW=mX(48)-mX(0); LHH=mX(6)-mX(0); inited=true; }
    cls(CLR_DARK_BLUE);
    rectfill(0, FLOORY, SCREEN_W, SCREEN_H-FLOORY, CLR_DARK_GREEN);
    rectfill((int)(WORLD_CX-48), FLOORY-46-6, 96, 12, CLR_DARK_GREY);   // ledge
    rect((int)(WORLD_CX-48), FLOORY-46-6, 96, 12, CLR_LIGHT_GREY);

    // see-saw fulcrum (a static wedge under the plank pivot)
    { int fx=(int)SAW_CX, fyt=FLOORY-(int)SAW_PIV;
      trifill(fx, fyt, fx-8, FLOORY, fx+8, FLOORY, CLR_DARK_GREY); }

    draw_obox(ramp, RAMP_HW, RAMP_HH, CLR_MEDIUM_GREY, CLR_LIGHT_GREY);      // STATIC
    draw_obox(spinner, SPIN_HL, SPIN_HT, CLR_BLUE, CLR_LIGHT_GREY);          // KINEMATIC
    draw_obox(pinwheel, PIN_HL, PIN_HT, CLR_YELLOW, CLR_ORANGE);             // DYNAMIC
    draw_obox(seesaw, SAW_HW, SAW_HH, CLR_BROWN, CLR_ORANGE);                // the see-saw plank
    // launcher: two static guide posts (the rail) + the sliding pad
    { int lx=(int)LAUNCH_CX, hw=(int)(LAUNCH_HW*PPM), top=FLOORY-(int)(LAUNCH_TOP*PPM)-6;
      rectfill(lx-hw-1, top, 2, FLOORY-top, CLR_DARK_GREY);
      rectfill(lx+hw-1, top, 2, FLOORY-top, CLR_DARK_GREY); }
    draw_obox(launcher, LAUNCH_HW, LAUNCH_HH, CLR_LIME_GREEN, CLR_WHITE);    // the launcher pad
    for (int i=0;i<ncrate;i++) draw_crate(crate[i]);
    for (int i=0;i<NPART;i++) boxrig_draw(part[i].body, part[i].poly, part[i].pcx, part[i].pcy, PPM, SCREEN_H);
    for (int i=0;i<nball;i++) draw_ball(&ball[i]);
    for (int i=0;i<nblob;i++) draw_blob(&blob[i]);

    // explosion shockwave ring
    if (boomF>0){ float t=1.0f-(float)boomF/BOOM_LIFE; int rr=(int)(t*66)+4;
        circ(boomX,boomY,rr, t<0.5f?CLR_YELLOW:CLR_ORANGE); if(rr>6) circ(boomX,boomY,rr-4,CLR_WHITE); }

    font(FONT_SMALL);
    label(ramp, "static"); label(spinner, "kinematic"); label(pinwheel, "dynamic");
    label(seesaw, "see-saw"); label(launcher, "launcher");
    print("verlet + crates + puppet + static/kinematic/dynamic + see-saw + balls + launcher", 4, 4, CLR_WHITE);
    print("drag  C crate  B blob  V ball  E boom  F fire  R reset", 4, SCREEN_H-10, CLR_LIGHT_GREY);
}
