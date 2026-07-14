/* de:meta
{
  "slug": "silverball",
  "title": "Silverball",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "game",
    "tech-demo"
  ],
  "genre": "arcade",
  "teaches": [
    "rigid-body",
    "collision-detection"
  ],
  "lineage": "The third Box2D v3 cart (runtime/box2d/) — the MOTORED-JOINT showcase, distinct from the hand-rolled `pinball` table. Real revolute joints with a motor + angle limits ARE the flippers (a snap you can feel), b2 sensors are the drain + score lanes, and contact events ring the bumpers scaled by impact. Backlog: docs/design/box2d-cart-ideas.md.",
  "description": "A mini pinball table where the flippers are real Box2D v3 revolute joints — a motor drives each one against an angle limit, so they snap up and fling the ball with genuine physics, not a scripted animation. Pop bumpers kick the ball and ring (louder the harder you hit), and the drain between the flippers is a real sensor. LEFT/Z = left flipper, RIGHT/X = right flipper, SPACE = serve the next ball. You get 3 balls; rack up points off the bumpers. The motored-joint sibling to the hand-built pinball cart, on the engine's pure-C Box2D backend."
}
de:meta */
#include "studio.h"
#include "box2d/box2d.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>

// SILVERBALL — pinball on Box2D v3. The flippers are REVOLUTE joints (motor + angle
// limits): a strong motor drives each flipper toward its limit, so it snaps and flings
// the ball. Bumpers score via contact events; the drain is a sensor.
//   Box2D metres/y-up  <->  studio pixels/y-down via SX/SY.

#define PPM     20.0f
#define SX(wx)  ((int)((wx) * PPM))
#define SY(wy)  ((int)(SCREEN_H - (wy) * PPM))

#define BALLR   0.24f
#define FLIPHL  0.85f          // flipper half-length
#define NBUMP   3

enum { T_WALL = 1, T_FLIP = 2, T_BALL = 3, T_BUMP = 4, T_DRAIN = 5 };
#define SL_DING 8              // bumper ding
#define SL_FLIP 9              // flipper thwack

static b2WorldId world;
static b2BodyId  ball;
static b2ShapeId ballShape;
static int       ball_live = 0;
static b2JointId jL, jR;       // flipper revolute joints
static float     restL, flipL, restR, flipR;   // motor speeds
static b2Vec2    bump[NBUMP];
static b2ShapeId bumpShape[NBUMP];
static b2ShapeId drainShape;
static int       score = 0, balls = 3, built = 0;

static void wall(b2BodyId b, float x0, float y0, float x1, float y1) {
    b2Segment s = {{x0, y0}, {x1, y1}};
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.material.friction = 0.1f; sd.material.restitution = 0.3f;
    b2CreateSegmentShape(b, &sd, &s);
}

// a flipper: revolute joint at (px,py); pivotEndX = which local end is the pivot (-hl left, +hl right);
// rest/flip are the two motor speeds, lo/hi the angle limits, restA the built angle.
static b2JointId make_flipper(float px, float py, float pivotEndX, float lo, float hi, float restA,
                              float rest, float flip) {
    b2BodyDef ad = b2DefaultBodyDef();
    ad.position = (b2Vec2){px, py};
    b2BodyId anchor = b2CreateBody(world, &ad);

    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){px - pivotEndX * cosf(restA), py - pivotEndX * sinf(restA)};
    bd.rotation = b2MakeRot(restA);
    bd.userData = (void*)(intptr_t)T_FLIP;
    bd.enableSleep = false;                        // stay awake — a sleeping flipper ignores SetMotorSpeed
    b2BodyId f = b2CreateBody(world, &bd);
    b2Polygon box = b2MakeBox(FLIPHL, 0.11f);
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 8.0f; sd.material.friction = 0.4f; sd.material.restitution = 0.2f;
    b2CreatePolygonShape(f, &sd, &box);

    b2RevoluteJointDef jd = b2DefaultRevoluteJointDef();
    jd.bodyIdA = anchor; jd.bodyIdB = f;
    jd.localAnchorA = (b2Vec2){0, 0};
    jd.localAnchorB = (b2Vec2){pivotEndX, 0};
    jd.enableLimit = true; jd.lowerAngle = lo; jd.upperAngle = hi;
    jd.enableMotor = true; jd.maxMotorTorque = 1400.0f; jd.motorSpeed = rest;
    return b2CreateRevoluteJoint(world, &jd);
}

static void serve(void) {
    if (ball_live || balls <= 0) return;
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){2.5f + rnd(7), 13.8f};        // random lane across the top (breaks symmetry)
    bd.linearVelocity = (b2Vec2){(rnd(9) - 4) * 0.7f, -1.0f};
    bd.isBullet = true;
    bd.userData = (void*)(intptr_t)T_BALL;
    ball = b2CreateBody(world, &bd);
    b2Circle c = {{0, 0}, BALLR};
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 1.0f; sd.material.friction = 0.05f; sd.material.restitution = 0.35f;
    sd.enableHitEvents = true; sd.enableSensorEvents = true;
    ballShape = b2CreateCircleShape(ball, &sd, &c);
    ball_live = 1;
}

static void reset_world(void) {
    if (built) b2DestroyWorld(world);
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){0, -14.0f};
    world = b2CreateWorld(&wd);
    b2World_SetHitEventThreshold(world, 1.0f);
    built = 1; score = 0; balls = 3; ball_live = 0;

    static int audio = 0;
    if (!audio) {
        instrument(SL_DING, INSTR_MALLET, 0, 120, 0, 200);   // bell-ish bumper
        instrument(SL_FLIP, INSTR_NOISE, 0, 40, 0, 60);      // flipper thwack
        audio = 1;
    }

    b2BodyDef gd = b2DefaultBodyDef();
    gd.userData = (void*)(intptr_t)T_WALL;
    b2BodyId cab = b2CreateBody(world, &gd);
    float W = 12.0f, H = 16.0f;
    wall(cab, 1.0f, 1.5f, 1.0f, H - 0.5f);              // left
    wall(cab, W - 1.0f, 1.5f, W - 1.0f, H - 0.5f);      // right
    wall(cab, 1.0f, H - 0.5f, W - 1.0f, H - 0.5f);      // top
    wall(cab, 1.0f, 4.2f, 3.5f, 2.5f);                  // lower-left funnel — ends AT the flipper pivot (no pocket)
    wall(cab, W - 1.0f, 4.2f, W - 3.5f, 2.5f);          // lower-right funnel

    // drain sensor across the bottom-centre gap
    b2ShapeDef dsd = b2DefaultShapeDef();
    dsd.isSensor = true; dsd.enableSensorEvents = true;
    b2Polygon dpoly = b2MakeOffsetBox(2.0f, 0.4f, (b2Vec2){6.0f, 0.6f}, b2MakeRot(0));
    drainShape = b2CreatePolygonShape(cab, &dsd, &dpoly);

    // bumpers
    bump[0] = (b2Vec2){3.5f, 10.0f};
    bump[1] = (b2Vec2){6.0f, 12.0f};
    bump[2] = (b2Vec2){8.5f, 10.0f};
    for (int i = 0; i < NBUMP; i++) {
        b2BodyDef bd = b2DefaultBodyDef();
        bd.position = bump[i];
        bd.userData = (void*)(intptr_t)T_BUMP;
        b2BodyId b = b2CreateBody(world, &bd);
        b2Circle c = {{0, 0}, 0.55f};
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.material.restitution = 0.6f;
        sd.enableHitEvents = true;
        bumpShape[i] = b2CreateCircleShape(b, &sd, &c);
    }

    // flippers — left pivot on its left end, right pivot on its right end
    jL = make_flipper(3.5f, 2.5f, -FLIPHL, -0.42f, 0.42f, -0.42f, -16.0f, 30.0f);
    jR = make_flipper(W - 3.5f, 2.5f, FLIPHL, -0.42f, 0.42f, 0.42f, 16.0f, -30.0f);
    restL = -16.0f; flipL = 30.0f; restR = 16.0f; flipR = -30.0f;
}

void update(void) {
    if (!built) reset_world();
    if (keyp('R')) reset_world();
    if (keyp(KEY_SPACE)) serve();

    int lf = key(KEY_LEFT)  || key('Z');
    int rf = key(KEY_RIGHT) || key('X');
    b2RevoluteJoint_SetMotorSpeed(jL, lf ? flipL : restL);
    b2RevoluteJoint_SetMotorSpeed(jR, rf ? flipR : restR);
    if (keyp(KEY_LEFT) || keyp('Z') || keyp(KEY_RIGHT) || keyp('X'))
        hit(60, SL_FLIP, 3, 40);

    b2World_Step(world, 1.0f / 60.0f, 4);

    // bumper hits — score + a pop kick + a ding scaled by impact
    b2ContactEvents ce = b2World_GetContactEvents(world);
    for (int i = 0; i < ce.hitCount; i++) {
        b2ContactHitEvent *h = &ce.hitEvents[i];
        int ta = (int)(intptr_t)b2Body_GetUserData(b2Shape_GetBody(h->shapeIdA));
        int tb = (int)(intptr_t)b2Body_GetUserData(b2Shape_GetBody(h->shapeIdB));
        if ((ta == T_BUMP || tb == T_BUMP) && ball_live && h->approachSpeed > 1.0f) {
            score += 100;
            int v = 3 + (int)(h->approachSpeed * 0.3f); if (v > 7) v = 7;
            hit(72, SL_DING, v, 180);
            // pop: shove the ball away from the bumper centre
            b2Vec2 bp = b2Body_GetPosition(ball);
            b2Vec2 n = {bp.x - h->point.x, bp.y - h->point.y};
            float L = sqrtf(n.x*n.x + n.y*n.y); if (L < 0.01f) L = 1;
            float jx = (rnd(11) - 5) * 0.18f;             // jitter so a dead-centre hit can't pogo
            b2Body_ApplyLinearImpulseToCenter(ball, (b2Vec2){n.x/L*2.5f + jx, n.y/L*2.5f}, true);
        }
    }

    // drain sensor — ball down the middle = lost
    b2SensorEvents se = b2World_GetSensorEvents(world);
    for (int i = 0; i < se.beginCount; i++) {
        if (ball_live && se.beginEvents[i].sensorShapeId.index1 == drainShape.index1) {
            b2DestroyBody(ball); ball_live = 0; balls--;
        }
    }

#ifdef DE_TRACE
    watch("score", "%d", score);
    watch("balls", "%d", balls);
    watch("live",  "%d", ball_live);
#endif
}

static void draw_flipper(b2JointId j, float pivotEndX) {
    // reconstruct the flipper body from the joint's body B
    b2BodyId f = b2Joint_GetBodyB(j);
    b2Vec2 p = b2Body_GetPosition(f);
    b2Rot  r = b2Body_GetRotation(f);
    const float lx[4] = {-FLIPHL, FLIPHL, FLIPHL, -FLIPHL}, ly[4] = {-0.11f, -0.11f, 0.11f, 0.11f};
    int xy[8];
    for (int i = 0; i < 4; i++) {
        float wx = p.x + (r.c*lx[i] - r.s*ly[i]);
        float wy = p.y + (r.s*lx[i] + r.c*ly[i]);
        xy[i*2] = SX(wx); xy[i*2+1] = SY(wy);
    }
    polyfill(xy, 4, CLR_WHITE);
}

void draw(void) {
    if (!built) { cls(CLR_DARK_PURPLE); return; }
    cls(CLR_DARK_PURPLE);

    // cabinet walls (draw the two funnels + sides + top as lines)
    line(SX(1), SY(1.5f), SX(1), SY(15.5f), CLR_INDIGO);
    line(SX(11), SY(1.5f), SX(11), SY(15.5f), CLR_INDIGO);
    line(SX(1), SY(15.5f), SX(11), SY(15.5f), CLR_INDIGO);
    line(SX(1), SY(4.2f), SX(3.5f), SY(2.5f), CLR_INDIGO);
    line(SX(11), SY(4.2f), SX(8.5f), SY(2.5f), CLR_INDIGO);

    for (int i = 0; i < NBUMP; i++) {              // bumpers
        circfill(SX(bump[i].x), SY(bump[i].y), (int)(0.55f*PPM), CLR_ORANGE);
        circ(SX(bump[i].x), SY(bump[i].y), (int)(0.55f*PPM), CLR_YELLOW);
    }
    draw_flipper(jL, -FLIPHL);
    draw_flipper(jR,  FLIPHL);

    if (ball_live) {                               // the ball
        b2Vec2 bp = b2Body_GetPosition(ball);
        circfill(SX(bp.x), SY(bp.y), (int)(BALLR*PPM), CLR_LIGHT_GREY);
        circ(SX(bp.x), SY(bp.y), (int)(BALLR*PPM), CLR_WHITE);
    }

    char buf[32];
    snprintf(buf, sizeof buf, "%d", score);
    print(buf, 4, 4, CLR_WHITE);
    snprintf(buf, sizeof buf, "BALLS %d", balls);
    print(buf, 4, SCREEN_H - 10, CLR_LIGHT_GREY);
    if (!ball_live) print(balls > 0 ? "SPACE=SERVE" : "GAME OVER  R", SCREEN_W/2 - 34, SCREEN_H/2, CLR_YELLOW);
}
