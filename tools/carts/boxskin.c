/* de:meta
{
  "slug": "boxskin",
  "collection": ["physics"],
  "title": "Box Skin — one texture skinned across a Box2D joint",
  "status": "active",
  "created": "2026-07-23",
  "kind": ["tech-demo", "probe"],
  "teaches": ["rigid-body", "procedural-mesh"],
  "lineage": "Slice 2 of the playtime-into-dreamengine port (after puppet.c rigid parts + boxlab.c self-righting): the playtime meshusert idea — a SINGLE texture spanning several Box2D bones, linear-blend-skinned so it bends smoothly at the joint instead of tearing into rigid halves. Auto-meshes the sprite (carfit grid-clip) and weights each vertex to two bones.",
  "description": {
    "summary": "One arm sprite, skinned across an elbow. A grid mesh is auto-built over the sprite's alpha; each vertex is weighted to the upper-arm and forearm Box2D bones, so when the physics elbow bends the single texture deforms smoothly across the joint. Press SPACE to A/B it against rigid two-part rendering — watch the seam tear at the elbow in rigid mode and stay smooth when skinned. Drag the hand to bend it.",
    "detail": "The playtime meshusert move, minimal: (1) a grid over the sprite's opaque pixels becomes a triangle mesh (the carfit clip); (2) each mesh vertex gets linear-blend-skinning weights to the two bones by distance along the limb, blending across the elbow; (3) each frame the vertex world position = sum of weight * (bonePos + rotate(restOffset)) over its bones, then tritex draws the mesh. Two Box2D bodies (upper arm, forearm) joined by a sprung revolute elbow, hung from a sprung shoulder; drag the hand with a mouse joint to bend the arm. SPACE toggles SKINNED vs RIGID (each bone draws its own sprite half as a rigid quad) so the seam at a flexed elbow is obvious. This is LBS — the simpler cousin of playtime's DQS; DQS (no candy-wrapper collapse at big bends) is the next step.",
    "controls": "Drag the hand to bend the arm. SPACE = toggle skinned / rigid. M = show the mesh wireframe. R = reset."
  },
  "todo": [
    "DQS (dual-quaternion skinning) to kill the candy-wrapper volume-collapse at sharp bends (playtime already has this).",
    "Alpha-aware / painted weights instead of distance-along-limb.",
    "Skin a whole multi-bone chain (tentacle) and the puppet's limbs, not just one elbow."
  ]
}
de:meta */
#include "studio.h"
#include "box2d/box2d.h"      // opt-in: make-cart/play link the vendored Box2D v3
#include <math.h>
#include <stdio.h>

#define PPM     22.0f
#define SX(wx)  ((int)((wx) * PPM))
#define SY(wy)  ((int)(SCREEN_H - (wy) * PPM))
#define WX(sx)  ((sx) / PPM)
#define WY(sy)  ((SCREEN_H - (sy)) / PPM)
#define DEG     (3.14159265f/180.0f)

// the limb sprite on the sheet, and where it hangs on screen
#define AW  20
#define AH  64
#define SHX 160          // shoulder screen x
#define SHY 34           // shoulder screen y
#define STEP 4           // mesh grid spacing (sprite px)
#define GW  (AW/STEP+1)
#define GH  (AH/STEP+1)
#define MAXV (GW*GH)
#define MAXT (GW*GH*2)

// two bones: upper arm (0), forearm (1). Centres in SPRITE px (col 10 = limb axis).
static const float BONE_SY[2] = {16, 46};
#define ELBOW_SY 31
#define SHLDR_SY 3

static b2WorldId world;
static b2BodyId  ground, bone[2];
static b2Vec2    bonePos0[2];    // rest positions (metres)

// mesh
typedef struct { float vx, vy; float uvx, uvy; float w[2]; b2Vec2 off[2]; } Vtx;
static Vtx  vtx[MAXV];
static int  vid[GH][GW], nv;
static int  mtri[MAXT][3], nt;

static b2JointId mjoint; static bool dragging = false;
static bool skinned = true, showMesh = false;

static inline bool opaque(int sx, int sy) { return sget(sx, sy) != 0; }

// vertex rest position on screen for sprite pixel (sx,sy): sprite col 10 sits on the axis
static void rest_screen(int sx, int sy, float *rxs, float *rys) {
    *rxs = SHX + (sx - 10); *rys = SHY + sy;
}

static void build_mesh(void) {
    nv = nt = 0;
    for (int gy = 0; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++) {
            int sx = gx*STEP; if (sx >= AW) sx = AW-1;
            int sy = gy*STEP; if (sy >= AH) sy = AH-1;
            if (!opaque(sx, sy)) { vid[gy][gx] = -1; continue; }
            float rxs, rys; rest_screen(sx, sy, &rxs, &rys);
            Vtx *v = &vtx[nv];
            v->vx = WX(rxs); v->vy = WY(rys);         // rest world (metres)
            v->uvx = sx; v->uvy = sy;                 // UV = sheet pixel
            // LBS weights: distance along the limb to each bone centre, blended
            float d0 = fabsf(sy - BONE_SY[0]) + 2.0f, d1 = fabsf(sy - BONE_SY[1]) + 2.0f;
            float w0 = 1.0f/d0, w1 = 1.0f/d1, s = w0 + w1;
            v->w[0] = w0/s; v->w[1] = w1/s;
            v->off[0] = (b2Vec2){ v->vx - bonePos0[0].x, v->vy - bonePos0[0].y };
            v->off[1] = (b2Vec2){ v->vx - bonePos0[1].x, v->vy - bonePos0[1].y };
            vid[gy][gx] = nv++;
        }
    for (int gy = 0; gy < GH-1; gy++)
        for (int gx = 0; gx < GW-1; gx++) {
            int a=vid[gy][gx], b=vid[gy][gx+1], c=vid[gy+1][gx+1], d=vid[gy+1][gx];
            if (a>=0&&b>=0&&c>=0&&d>=0){ mtri[nt][0]=a;mtri[nt][1]=b;mtri[nt][2]=c;nt++; mtri[nt][0]=a;mtri[nt][1]=c;mtri[nt][2]=d;nt++; }
        }
}

static b2BodyId make_bone(float cxs, float cys, float halfH) {
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){ WX(cxs), WY(cys) };
    b2BodyId id = b2CreateBody(world, &bd);
    b2Polygon box = b2MakeBox(0.16f, halfH);
    b2ShapeDef sd = b2DefaultShapeDef(); sd.density = 1.0f; sd.material.friction = 0.5f;
    b2CreatePolygonShape(id, &sd, &box);
    return id;
}
static void spring_revolute(b2BodyId a, b2BodyId b, float axs, float ays, float lo, float hi, float hz) {
    b2Vec2 w = { WX(axs), WY(ays) };
    b2RevoluteJointDef d = b2DefaultRevoluteJointDef();
    d.bodyIdA = a; d.bodyIdB = b;
    d.localAnchorA = b2Body_GetLocalPoint(a, w);
    d.localAnchorB = b2Body_GetLocalPoint(b, w);
    d.collideConnected = false;
    d.enableLimit = true; d.lowerAngle = lo*DEG; d.upperAngle = hi*DEG;
    d.enableSpring = true; d.hertz = hz; d.dampingRatio = 0.6f; d.targetAngle = 0;
    b2CreateRevoluteJoint(world, &d);
}

static void build(void) {
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){ 0, -10 };
    world = b2CreateWorld(&wd);
    b2BodyDef gd = b2DefaultBodyDef();
    ground = b2CreateBody(world, &gd);

    bone[0] = make_bone(SHX, SHY + BONE_SY[0], (ELBOW_SY - SHLDR_SY)/2.0f/PPM);
    bone[1] = make_bone(SHX, SHY + BONE_SY[1], (AH - ELBOW_SY)/2.0f/PPM);
    bonePos0[0] = b2Body_GetPosition(bone[0]);
    bonePos0[1] = b2Body_GetPosition(bone[1]);

    spring_revolute(ground, bone[0], SHX, SHY + SHLDR_SY, -120, 120, 2.5f);   // shoulder
    spring_revolute(bone[0], bone[1], SHX, SHY + ELBOW_SY, -150, 10, 2.0f);   // elbow (bends one way)

    build_mesh();
    dragging = false;
}
static void reset(void) { b2DestroyWorld(world); build(); }

// skin a mesh vertex to screen coords via LBS over its two bones
static void skin(Vtx *v, int *xs, int *ys) {
    b2Vec2 p0 = b2Body_GetPosition(bone[0]), p1 = b2Body_GetPosition(bone[1]);
    b2Rot  r0 = b2Body_GetRotation(bone[0]), r1 = b2Body_GetRotation(bone[1]);
    float wx = v->w[0]*(p0.x + r0.c*v->off[0].x - r0.s*v->off[0].y)
             + v->w[1]*(p1.x + r1.c*v->off[1].x - r1.s*v->off[1].y);
    float wy = v->w[0]*(p0.y + r0.s*v->off[0].x + r0.c*v->off[0].y)
             + v->w[1]*(p1.y + r1.s*v->off[1].x + r1.c*v->off[1].y);
    *xs = SX(wx); *ys = SY(wy);
}

static bool inited = false;
void update(void) {
    if (!inited) { build(); inited = true; }
    if (keyp('R')) reset();
    if (keyp(' ')) skinned = !skinned;
    if (keyp('M')) showMesh = !showMesh;

    float mwx = WX(mouse_x()), mwy = WY(mouse_y());
    if (mouse_pressed(MOUSE_LEFT)) {                     // grab the forearm (hand end)
        b2Vec2 c = b2Body_GetPosition(bone[1]);
        if ((mwx-c.x)*(mwx-c.x)+(mwy-c.y)*(mwy-c.y) < 1.2f*1.2f) {
            b2MouseJointDef d = b2DefaultMouseJointDef();
            d.bodyIdA = ground; d.bodyIdB = bone[1];
            d.target = (b2Vec2){mwx,mwy}; d.hertz = 5.0f; d.dampingRatio = 0.7f;
            d.maxForce = 1500.0f * b2Body_GetMass(bone[1]);
            mjoint = b2CreateMouseJoint(world, &d); b2Body_SetAwake(bone[1], true); dragging = true;
        }
    }
    if (dragging && mouse_down(MOUSE_LEFT)) b2MouseJoint_SetTarget(mjoint,(b2Vec2){mwx,mwy});
    if (mouse_released(MOUSE_LEFT) && dragging){ b2DestroyJoint(mjoint); dragging=false; }

    b2World_Step(world, 1.0f/60.0f, 4);

#ifdef DE_TRACE
    watch("verts","%d",nv); watch("tris","%d",nt);
    watch("elbow","%.2f", b2Rot_GetAngle(b2Body_GetRotation(bone[1])) - b2Rot_GetAngle(b2Body_GetRotation(bone[0])));
#endif
}

// RIGID mode: each bone draws its own sprite half as a rigid quad (shows the seam)
static void draw_rigid_half(b2BodyId b, float cSy, int y0, int y1) {
    b2Vec2 p = b2Body_GetPosition(b); b2Rot r = b2Body_GetRotation(b);
    // quad corners in sprite px → local metres about the bone centre (col 10 axis)
    int csx[4]={0,AW,AW,0}, csy[4]={y0,y0,y1,y1}; int X[4],Y[4];
    for (int i=0;i<4;i++){
        float ox=(csx[i]-10)/PPM, oy=-(csy[i]-cSy)/PPM;
        float wx=p.x + r.c*ox - r.s*oy, wy=p.y + r.s*ox + r.c*oy;
        X[i]=SX(wx); Y[i]=SY(wy);
    }
    tritex(X[0],Y[0], 0,y0,  X[1],Y[1], AW,y0, X[2],Y[2], AW,y1);
    tritex(X[0],Y[0], 0,y0,  X[2],Y[2], AW,y1, X[3],Y[3], 0,y1);
}

void draw(void) {
    if (!inited) { build(); inited = true; }
    cls(CLR_DARK_BLUE);
    // shoulder pin marker
    circfill(SX(WX(SHX)), SY(WY(SHY+SHLDR_SY)), 2, CLR_LIGHT_GREY);

    if (skinned) {
        for (int t=0;t<nt;t++){
            int a=mtri[t][0],b=mtri[t][1],c=mtri[t][2], ax,ay,bx,by,cx,cy;
            skin(&vtx[a],&ax,&ay); skin(&vtx[b],&bx,&by); skin(&vtx[c],&cx,&cy);
            tritex(ax,ay, vtx[a].uvx,vtx[a].uvy, bx,by, vtx[b].uvx,vtx[b].uvy, cx,cy, vtx[c].uvx,vtx[c].uvy);
        }
        if (showMesh)
            for (int t=0;t<nt;t++){
                int a=mtri[t][0],b=mtri[t][1],c=mtri[t][2], ax,ay,bx,by,cx,cy;
                skin(&vtx[a],&ax,&ay); skin(&vtx[b],&bx,&by); skin(&vtx[c],&cx,&cy);
                line(ax,ay,bx,by,CLR_DARK_GREY); line(bx,by,cx,cy,CLR_DARK_GREY); line(cx,cy,ax,ay,CLR_DARK_GREY);
            }
    } else {
        draw_rigid_half(bone[0], BONE_SY[0], 0, ELBOW_SY);      // upper arm half
        draw_rigid_half(bone[1], BONE_SY[1], ELBOW_SY, AH);     // forearm half
    }

    font(FONT_SMALL);
    print(skinned ? "SKINNED: one texture, LBS across the elbow" : "RIGID: two halves — watch the seam tear", 4, 4, skinned ? CLR_WHITE : CLR_YELLOW);
    print("drag the hand to bend   SPACE skinned/rigid   M mesh   R reset", 4, SCREEN_H-10, CLR_LIGHT_GREY);
}
