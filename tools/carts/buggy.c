/* de:meta
{
  "slug": "buggy",
  "title": "Buggy",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "rigid-body",
    "collision-detection"
  ],
  "lineage": "The second Box2D v3 cart (runtime/box2d/), the vehicle counterpart to tumble's destruction: shows the joints-and-terrain half of the engine — WHEEL joints (built-in spring suspension + a driven motor) on a car rolling over a b2Chain (smooth one-sided procedural terrain, no ghost bumps). Backlog: docs/design/box2d-cart-ideas.md; backend: docs/design/box2d-integration.md.",
  "description": "A hill-climb buggy. Drive a little car over rolling procedural hills that get bumpier the further you go, and reach the flag without landing on your roof. The wheels ride on real Box2D v3 WHEEL joints — spring suspension that soaks up the bumps plus a motor you drive — and the ground is a b2Chain (smooth, one-sided terrain). A revving engine (a held voice whose firing rate + snarl pitch up with your wheel speed, ported from enginelab) drones and climbs as you accelerate. RIGHT = gas, LEFT = reverse/brake; catch air off the crests and the buggy coasts through the jump on real ballistics. The camera follows; R restarts. Second cart on the engine's pure-C Box2D backend, the vehicle twin of tumble."
}
de:meta */
#include "studio.h"
#include "box2d/box2d.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>

// BUGGY — a hill-climb car on Box2D v3 wheel joints + a b2Chain terrain.
//   Box2D is METRES, y-UP; studio.h is PIXELS, y-DOWN. SX/SY bridge them AND apply
//   a follow-camera (camx/camy = where the view is centred, in metres).

#define PPM     14.0f
#define SX(wx)  ((int)(((wx) - camx) * PPM + SCREEN_W * 0.5f))
#define SY(wy)  ((int)(SCREEN_H * 0.5f - ((wy) - camy) * PPM))

#define TN      400            // terrain points
#define TSTEP   1.0f           // metres between terrain points (fine = smooth ride at speed)
#define CHW     1.05f          // chassis half-width (wide wheelbase = stable, resists wheelie flips)
#define CHH     0.22f          // chassis half-height (low)
#define WHEELR  0.42f          // wheel radius
#define DRIVE   20.0f          // motor speed (rad/s) — top wheel spin
#define TORQUE  20.0f          // maxMotorTorque per wheel — punchy climbing power
#define STAB    140.0f         // ground stability: auto-counter an excessive pitch so power climbs / brakes flat, not loops
#define GOAL    80.0f          // flag distance (m from the start)
#define SLOT_ENG 8             // engine-sound instrument slot

enum { T_GROUND = 1, T_CAR = 2 };

static b2WorldId world;
static b2BodyId  chassis, wheelF, wheelB;
static b2JointId wjF, wjB;
static b2Vec2    terr[TN];
static float     camx, camy;
static int       built = 0, won = 0;
static float     start_x, flag_x;
static int       vEng = -1;        // held engine voice
static float     throttle_s = 0;   // ramped throttle (0..1)
static void      engine_build(void);   // defined below, called from reset_world

static float terrain_h(float x) {                 // the surface height used to build the chain
    float h = 4.0f;
    float amp = 0.35f + x * 0.006f;               // hills grow with distance (gently)
    h += sinf(x * 0.22f) * amp + sinf(x * 0.57f + 1.3f) * amp * 0.45f;
    return h;
}

// the ACTUAL chain surface at x (linear interp of terr[], incl. the flat pad) — used for
// the airborne test; terrain_h() alone mismatches the pad and would misfire the air-lean.
static float surface_at(float x) {
    float fi = x / TSTEP; int i = (int)fi;
    if (i < 0) i = 0; if (i >= TN - 1) i = TN - 2;
    float t = fi - i;
    return terr[i].y * (1.0f - t) + terr[i + 1].y * t;
}

static void reset_world(void) {
    if (built) b2DestroyWorld(world);
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){0.0f, -10.0f};
    world = b2CreateWorld(&wd);
    built = 1; won = 0;

    for (int i = 0; i < TN; i++) {                // procedural terrain, flat launch pad up front
        float x = i * TSTEP;
        terr[i] = (b2Vec2){x, x < 8.0f ? 4.0f : terrain_h(x)};
    }
    b2BodyDef gd = b2DefaultBodyDef();
    gd.userData = (void*)(intptr_t)T_GROUND;
    b2BodyId ground = b2CreateBody(world, &gd);
    b2ChainDef cd = b2DefaultChainDef();          // one smooth one-sided surface (no ghost bumps)
    b2Vec2 rev[TN];                               // reversed winding -> solid side faces UP (drive on top)
    for (int i = 0; i < TN; i++) rev[i] = terr[TN - 1 - i];
    cd.points = rev; cd.count = TN;
    b2SurfaceMaterial mat = b2DefaultSurfaceMaterial();
    mat.friction = 0.9f;
    cd.materials = &mat; cd.materialCount = 1;
    b2CreateChain(ground, &cd);

    // the car: a chassis box + two wheels on wheel joints (suspension spring + motor)
    start_x = 6.0f;
    flag_x  = start_x + GOAL;
    float sy = 4.0f + 1.0f;                        // just above the flat launch pad (terr = 4.0 there)
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){start_x, sy};
    bd.userData = (void*)(intptr_t)T_CAR;
    bd.isBullet = true;                           // CCD: fast wheels/chassis mustn't tunnel the thin chain
    chassis = b2CreateBody(world, &bd);
    b2Polygon body = b2MakeBox(CHW, CHH);
    b2ShapeDef csd = b2DefaultShapeDef();
    csd.density = 3.0f; csd.material.friction = 0.3f;   // heavy chassis resists the motor's flip reaction
    b2CreatePolygonShape(chassis, &csd, &body);

    b2ShapeDef wsd = b2DefaultShapeDef();
    wsd.density = 1.2f; wsd.material.friction = 1.4f;   // grippy tyres
    b2Circle wc = {{0.0f, 0.0f}, WHEELR};

    float ax[2] = {0.85f, -0.85f};                // front, back mount x on the chassis (wide stance)
    b2JointId *wj[2] = {&wjF, &wjB};
    b2BodyId  *wb[2] = {&wheelF, &wheelB};
    for (int i = 0; i < 2; i++) {
        b2BodyDef wd2 = b2DefaultBodyDef();
        wd2.type = b2_dynamicBody;
        wd2.position = (b2Vec2){start_x + ax[i], sy - 0.45f};
        wd2.userData = (void*)(intptr_t)T_CAR;
        wd2.isBullet = true;                      // CCD so fast wheels grip the chain instead of tunneling
        b2BodyId w = b2CreateBody(world, &wd2);
        b2CreateCircleShape(w, &wsd, &wc);
        b2WheelJointDef jd = b2DefaultWheelJointDef();
        jd.bodyIdA = chassis; jd.bodyIdB = w;
        jd.localAnchorA = (b2Vec2){ax[i], -0.32f};
        jd.localAnchorB = (b2Vec2){0.0f, 0.0f};
        jd.localAxisA = (b2Vec2){0.0f, 1.0f};     // suspension travels vertically
        jd.enableSpring = true; jd.hertz = 4.0f; jd.dampingRatio = 0.7f;
        jd.enableMotor = true; jd.maxMotorTorque = TORQUE; jd.motorSpeed = 0.0f;
        *wb[i] = w;
        *wj[i] = b2CreateWheelJoint(world, &jd);
    }
    camx = start_x; camy = sy;
    if (vEng < 0) engine_build();                 // build the held engine voice once
}

// engine sound (compact port of enginelab's gas engine): one held SAW voice pitched at
// the firing rate, brightening + snarling with revs, with a tremolo AT the firing rate = the chug.
static void engine_build(void) {
    instrument(SLOT_ENG, INSTR_SAW, 30, 0, 7, 220);
    instrument_filter(SLOT_ENG, FILTER_LOW, 400, 6);
    instrument_drive_mode(SLOT_ENG, DRIVE_ASYM);
    vEng = note_on(40, SLOT_ENG, 0);
    note_glide(vEng, 60);
}

static void engine_sound(float rpm, float thr) {
    if (vEng < 0) return;
    float r = rpm < 0 ? 0 : rpm > 1 ? 1 : rpm;
    float chug = 18.0f + 92.0f * r;                        // firing Hz: idle 18 -> redline 110
    float bmidi = 12.0f * log2f(chug / 440.0f) + 69.0f;    // body pitch tracks the firing rate
    int bvol = (int)(3.0f + r * 3.0f + thr * 1.2f); if (bvol > 7) bvol = 7;
    int bcut = 400 + (int)((r * 0.7f + thr * 0.3f) * 2400);
    note_pitch (vEng, bmidi);
    note_vol   (vEng, bvol);
    note_cutoff(vEng, bcut);
    note_drive (vEng, 0.1f + (r * 0.5f + thr * 0.5f) * 0.4f);   // saturation snarl climbs with load
    note_lfo   (vEng, 0, LFO_VOLUME, chug, 0.5f);              // the chug that speeds up with revs
}

void update(void) {
    if (!built) reset_world();
    if (keyp('R')) reset_world();

    bool gas = key(KEY_RIGHT) || key('D');
    bool rev = key(KEY_LEFT)  || key('A');
    float vx = b2Body_GetLinearVelocity(chassis).x;
    // LEFT brakes (holds the wheels at 0) while still rolling forward, then spins into reverse once
    // slowed — slamming straight to full reverse spun a huge braking torque that nose-dived the car.
    float drive = gas ? -DRIVE
                : rev ? (vx > 1.5f ? 0.0f : +DRIVE)
                : 0.0f;
    b2WheelJoint_SetMotorSpeed(wjF, drive);
    b2WheelJoint_SetMotorSpeed(wjB, drive);
    // air control ONLY when airborne — on the ground the gas just drives (no flip-happy wheelies)
    b2Vec2 wf = b2Body_GetPosition(wheelF), wb = b2Body_GetPosition(wheelB);
    bool grounded = (wf.y - WHEELR < surface_at(wf.x) + 0.35f) ||
                    (wb.y - WHEELR < surface_at(wb.x) + 0.35f);
    // airborne: NOTHING touches the body — the wheels just spin, the chassis coasts (honest ballistics).
    // on the ground: a gentle anti-loop assist so the power climbs instead of wheelie-flipping.
    if (grounded) {
        float ang = b2Rot_GetAngle(b2Body_GetRotation(chassis));
        if (ang >  0.35f) b2Body_ApplyTorque(chassis, -STAB * (ang - 0.35f), true);
        if (ang < -0.35f) b2Body_ApplyTorque(chassis, -STAB * (ang + 0.35f), true);
    }

    b2World_Step(world, 1.0f/60.0f, 4);

    float rpm = fabsf(b2Body_GetAngularVelocity(wheelB)) / DRIVE;   // revs from wheel spin
    float thr_target = (gas || rev) ? 1.0f : 0.0f;
    throttle_s += (thr_target - throttle_s) * 0.15f;
    engine_sound(rpm, throttle_s);

    b2Vec2 p = b2Body_GetPosition(chassis);       // follow-camera (lerp + a little look-ahead)
    b2Vec2 v = b2Body_GetLinearVelocity(chassis);
    float tx = p.x + v.x * 0.25f, ty = p.y + 2.0f;
    camx += (tx - camx) * 0.1f;
    camy += (ty - camy) * 0.08f;

    if (!won && p.x >= flag_x) won = 1;

#ifdef DE_TRACE
    watch("dist", "%.1f", p.x - start_x);
    watch("won",  "%d", won);
#endif
}

static void draw_wheel(b2BodyId w) {
    b2Vec2 p = b2Body_GetPosition(w);
    b2Rot  r = b2Body_GetRotation(w);
    int cx = SX(p.x), cy = SY(p.y), rr = (int)(WHEELR * PPM);
    circfill(cx, cy, rr, CLR_DARKER_GREY);
    circ(cx, cy, rr, CLR_LIGHT_GREY);
    line(cx, cy, cx + (int)(r.c * rr), cy - (int)(r.s * rr), CLR_LIGHT_GREY);  // spoke -> shows spin
}

void draw(void) {
    if (!built) { cls(CLR_DARK_BLUE); return; }
    cls(CLR_DARK_BLUE);

    // terrain: fill each visible segment from the surface down past the screen
    for (int i = 0; i < TN - 1; i++) {
        float ax0 = terr[i].x, ax1 = terr[i+1].x;
        if (ax1 < camx - 14.0f || ax0 > camx + 14.0f) continue;   // cull off-screen
        int x0 = SX(ax0), y0 = SY(terr[i].y), x1 = SX(ax1), y1 = SY(terr[i+1].y);
        int xy[8] = { x0, y0, x1, y1, x1, SCREEN_H, x0, SCREEN_H };
        polyfill(xy, 4, CLR_BROWN);
        line(x0, y0, x1, y1, CLR_GREEN);          // grass line
    }

    // flag at the goal
    int fsx = SX(flag_x), fsy = SY(terrain_h(flag_x));
    line(fsx, fsy, fsx, fsy - 34, CLR_LIGHT_GREY);
    int tri[6] = { fsx, fsy - 34, fsx + 16, fsy - 28, fsx, fsy - 22 };
    trifill(tri[0], tri[1], tri[2], tri[3], tri[4], tri[5], CLR_RED);

    draw_wheel(wheelF);
    draw_wheel(wheelB);

    b2Vec2 p = b2Body_GetPosition(chassis);       // chassis as a rotated box
    b2Rot  r = b2Body_GetRotation(chassis);
    const float lx[4] = {-CHW, CHW, CHW, -CHW}, ly[4] = {-CHH, -CHH, CHH, CHH};
    int cxy[8];
    for (int i = 0; i < 4; i++) {
        float wx = p.x + (r.c*lx[i] - r.s*ly[i]);
        float wy = p.y + (r.s*lx[i] + r.c*ly[i]);
        cxy[i*2] = SX(wx); cxy[i*2+1] = SY(wy);
    }
    polyfill(cxy, 4, CLR_ORANGE);
    for (int i = 0; i < 4; i++) line(cxy[i*2], cxy[i*2+1], cxy[((i+1)%4)*2], cxy[((i+1)%4)*2+1], CLR_RED);

    char buf[32];
    float dist = p.x - start_x; if (dist < 0) dist = 0;
    snprintf(buf, sizeof buf, "%.0f m", dist);
    print(buf, 4, 4, CLR_WHITE);
    print("RIGHT GAS  LEFT REVERSE  R", 4, SCREEN_H - 10, CLR_LIGHT_GREY);
    if (won) print("REACHED THE FLAG!", SCREEN_W/2 - 34, 20, CLR_YELLOW);
}
