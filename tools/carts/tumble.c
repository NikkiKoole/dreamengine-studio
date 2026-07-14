/* de:meta
{
  "slug": "tumble",
  "title": "Tumble",
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
  "lineage": "The rigid-body counterpart to the verlet toolkit (runtime/physics.h): the first cart on the vendored pure-C Box2D v3 (runtime/box2d/). Deliberately does the things verlet is bad at — crisp rotation, stable resting contact, friction — so it reads as the honest A/B against the jelly/waterjar particle carts. See docs/design/box2d-integration.md.",
  "description": "A slingshot and a huge wall of crates. Grab the slingshot pouch and pull it back to fling a heavy ball into the block, or grab any crate (or ball) directly and drag it around with a mouse joint — yank a load-bearing crate out and the pile collapses. Press SPACE to detonate your last ball into a burst of shrapnel that tears through the stack. Hundreds of real Box2D v3 rigid bodies — each crate rotates as it topples, rests solidly on the one below (stable stacking, the thing a verlet blob can't do), and slides on friction; balls ricochet off the bouncy arena walls and never disappear. Impact sounds scale with force (deep thud, woody clack, grass rustle, wall tik). TOPPLED counts the wreckage; R rebuilds. First cart on the engine's new pure-C Box2D backend."
}
de:meta */
#include "studio.h"
#include "box2d/box2d.h"      // opt-in: links runtime/box2d (built on demand by make-cart/play)
#include <math.h>
#include <stdio.h>
#include <stdint.h>

// TUMBLE — a rigid-body slingshot on real Box2D v3.
//   Box2D is METRES, y-UP, gravity -y; studio.h is PIXELS, y-DOWN. SX/SY/WX/WY
//   bridge the two (PPM = pixels per metre). Keep bodies ~0.1..10m — box2d hates
//   pixel-sized numbers. Everything here is a body + shape; we just read back
//   position + rotation each frame to draw.

#define PPM     24.0f
#define SX(wx)  ((int)((wx) * PPM))
#define SY(wy)  ((int)(SCREEN_H - (wy) * PPM))
#define WX(sx)  ((sx) / PPM)
#define WY(sy)  ((SCREEN_H - (sy)) / PPM)

#define NCRATE  512            // stress test: a big block of crates (build_stack fills what fits)
#define NBALL   300            // every ball you fire stays — no recycling, no culling
#define CH      0.20f          // crate half-extent (m)  -> ~10px box (small, so hundreds fit)
#define BR      0.34f          // ball radius (m)        -> a wrecking ball vs the little crates
#define LX      4.0f           // slingshot launcher (m) — set right so there's room to pull back
#define LY      2.2f
#define FLOORY  0.70f          // floor height (m)
#define POWER   3.0f           // pull-back -> launch speed
#define MAXV    34.0f          // launch speed clamp (m/s) — headroom for a big wind-up

// body userData tags, so a hit event can tell what struck what
enum { T_GROUND = 1, T_CRATE = 2, T_BALL = 3, T_WALL = 4 };
#define SL_WOOD  8             // INSTR_MEMBRANE — ball thud + crate clack (pitch tells them apart)
#define SL_TWANG 9             // INSTR_PLUCK — the slingshot release twang

static b2WorldId world;
static b2BodyId  crate[NCRATE];
static b2Vec2    crate0[NCRATE];   // start pose, for the TOPPLED test
static b2BodyId  ball[NBALL];
static float     ballr[NBALL];     // per-ball radius (fragments are smaller than the shot)
static int       nball  = 0;
static int       ncrate = 0;       // how many crates build_stack actually placed
static int       built  = 0;

enum { M_NONE, M_AIM, M_DRAG };    // what the left button is doing
static int       mmode = M_NONE;
static b2JointId mjoint;           // the mouse joint while M_DRAG
static b2BodyId  ground_body;      // static anchor for the mouse joint
static float     ex_x, ex_y;       // last explosion centre (screen px) — visual juice
static int       ex_age = -1;      // frames since the blast; -1 = none

static b2BodyId make_crate(float x, float y) {
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){x, y};
    bd.userData = (void*)(intptr_t)T_CRATE;
    b2BodyId id = b2CreateBody(world, &bd);
    b2Polygon box = b2MakeBox(CH, CH);                  // sharp: collision hull == the drawn quad (no skin-radius aura)
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 1.0f;
    sd.material.friction = 0.6f;
    sd.material.restitution = 0.0f;
    sd.enableHitEvents = true;                          // -> impact sounds
    b2CreatePolygonShape(id, &sd, &box);
    return id;
}

static void spawn_ball(float x, float y, float vx, float vy, float r) {
    if (nball >= NBALL) return;                 // cap reached — keep every existing ball, just stop adding
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){x, y};
    bd.linearVelocity = (b2Vec2){vx, vy};
    bd.userData = (void*)(intptr_t)T_BALL;
    bd.isBullet = true;                          // CCD — a fast ball/fragment can't tunnel the thin walls or crates
    b2BodyId id = b2CreateBody(world, &bd);
    b2Circle c = {{0.0f, 0.0f}, r};
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 5.0f;                           // heavy — it bowls crates over (small r -> light, fast shrapnel)
    sd.material.friction = 0.4f;
    sd.material.restitution = 0.15f;
    sd.enableHitEvents = true;                   // -> impact sounds
    b2CreateCircleShape(id, &sd, &c);
    ballr[nball] = r;
    ball[nball++] = id;
}

// split a ball into a burst of smaller balls flung radially outward — real shrapnel does the damage
static void explode_ball(int idx) {
    b2Vec2 p  = b2Body_GetPosition(ball[idx]);
    b2Vec2 v  = b2Body_GetLinearVelocity(ball[idx]);
    float  pr = ballr[idx];
    b2DestroyBody(ball[idx]);                    // the parent is consumed
    for (int j = idx + 1; j < nball; j++) { ball[j-1] = ball[j]; ballr[j-1] = ballr[j]; }
    nball--;

    float fr = pr * 0.45f; if (fr < 0.09f) fr = 0.09f;   // fragment radius (min floor)
    int N = 14;
    for (int i = 0; i < N; i++) {
        float a  = (float)i / N * 6.2832f + rnd(30) * 0.01f;
        float dx = cosf(a), dy = sinf(a), sp = 12.0f + rnd(6);
        spawn_ball(p.x + dx*(pr+fr), p.y + dy*(pr+fr),    // start on a ring so they don't overlap
                   dx*sp + v.x*0.3f, dy*sp + v.y*0.3f, fr);
    }
    shake(4.0f);
    ex_x = SX(p.x); ex_y = SY(p.y); ex_age = 0;
}

static void build_stack(void) {
    // a big block of crates filling the arena right of the slingshot — the stress test
    float W = SCREEN_W / PPM, TOP = SCREEN_H / PPM;
    float step = 2.0f*CH + 0.01f, bx0 = 5.6f;   // bx0 = left edge, clear of the launcher (LX=4)
    int cols = (int)((W - 0.1f - bx0) / step);
    int rows = (int)((TOP - 0.1f - (FLOORY + CH)) / step);
    int k = 0;
    for (int row = 0; row < rows && k < NCRATE; row++) {
        for (int i = 0; i < cols && k < NCRATE; i++) {
            float x = bx0 + CH + i*step, y = FLOORY + CH + row*step;
            crate[k]  = make_crate(x, y);
            crate0[k] = (b2Vec2){x, y};
            k++;
        }
    }
    ncrate = k;
}

static void reset_world(void) {
    if (built) b2DestroyWorld(world);           // frees every body it owns
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){0.0f, -10.0f};
    world = b2CreateWorld(&wd);
    built = 1;
    nball = 0;
    mmode = M_NONE;                             // any in-progress grab is gone with the world
    ex_age = -1;

    b2World_SetHitEventThreshold(world, 1.5f);  // only real impacts sound; settling ticks stay quiet

    static int audio_ready = 0;                 // one-time SFX slot setup
    if (!audio_ready) {
        instrument(SL_WOOD,  INSTR_MEMBRANE, 1, 0, 7, 200);   // struck drumhead -> thud/clack
        instrument(SL_TWANG, INSTR_PLUCK,    1, 0, 5, 160);   // rubber-band twang
        audio_ready = 1;
    }

    float W = SCREEN_W / PPM, TOP = SCREEN_H / PPM;

    b2BodyDef gd = b2DefaultBodyDef();          // static floor (grass)
    gd.userData = (void*)(intptr_t)T_GROUND;
    b2BodyId ground = b2CreateBody(world, &gd);
    ground_body = ground;                       // reused as the mouse-joint anchor
    b2Segment floor = {{0.0f, FLOORY}, {W, FLOORY}};
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.material.friction = 0.8f;
    sd.enableHitEvents = true;
    b2CreateSegmentShape(ground, &sd, &floor);

    b2BodyDef wd2 = b2DefaultBodyDef();          // bouncy left/right/top walls
    wd2.userData = (void*)(intptr_t)T_WALL;
    b2BodyId walls = b2CreateBody(world, &wd2);
    b2ShapeDef wsd = b2DefaultShapeDef();
    wsd.material.restitution = 0.5f;             // lively bounce (mixed max with the ball's 0.15)
    wsd.material.friction = 0.3f;
    wsd.enableHitEvents = true;
    b2Segment left  = {{0.0f, FLOORY}, {0.0f, TOP}};
    b2Segment right = {{W,    FLOORY}, {W,    TOP}};
    b2Segment ceil  = {{0.0f, TOP},    {W,    TOP}};
    b2CreateSegmentShape(walls, &wsd, &left);
    b2CreateSegmentShape(walls, &wsd, &right);
    b2CreateSegmentShape(walls, &wsd, &ceil);

    build_stack();
}

// aim vector -> clamped launch velocity (pull the pouch BACK from the launcher)
static b2Vec2 launch_vel(int mx, int my) {
    float vx = (LX - WX(mx)) * POWER, vy = (LY - WY(my)) * POWER;
    float sp = sqrtf(vx*vx + vy*vy);
    if (sp > MAXV) { vx *= MAXV/sp; vy *= MAXV/sp; }
    return (b2Vec2){vx, vy};
}

static int count_toppled(void) {
    int n = 0;
    for (int i = 0; i < ncrate; i++) {
        b2Vec2 p = b2Body_GetPosition(crate[i]);
        b2Rot  r = b2Body_GetRotation(crate[i]);
        float dx = p.x - crate0[i].x, dy = p.y - crate0[i].y;
        if (fabsf(r.s) > 0.5f || sqrtf(dx*dx + dy*dy) > 0.7f) n++;   // r.s = sin(angle): >30 deg
    }
    return n;
}

// map a hit event to a sound; volume scales with the impact speed (vol is 0..7)
static void impact_sound(int ta, int tb, float speed) {
    int vol = 2 + (int)(speed * 0.35f);
    if (vol > 7) vol = 7;
    if (ta == T_GROUND || tb == T_GROUND)                     // grass: soft dull rustle
        hit(28 + rnd(4), INSTR_NOISE, vol > 5 ? 5 : vol, 45);
    else if (ta == T_WALL || tb == T_WALL)                    // wall bounce: light woody tik
        hit(66 + rnd(4), SL_WOOD, vol > 4 ? 4 : vol, 35);
    else if (ta == T_BALL || tb == T_BALL)                    // ball impact: deep thud
        hit(36 + rnd(3), SL_WOOD, vol, 90);
    else                                                      // crate vs crate: woody clack
        hit(55 + rnd(4), SL_WOOD, vol, 45);
}

// topmost body under a world point (balls first, then crates) — for the drag gesture
static bool body_at(float wx, float wy, b2BodyId *out) {
    for (int i = nball - 1; i >= 0; i--) {
        b2Vec2 p = b2Body_GetPosition(ball[i]);
        float dx = wx - p.x, dy = wy - p.y, rr = ballr[i] + 0.06f;
        if (dx*dx + dy*dy <= rr*rr) { *out = ball[i]; return true; }
    }
    for (int i = 0; i < ncrate; i++) {
        b2Vec2 p = b2Body_GetPosition(crate[i]);
        b2Rot  r = b2Body_GetRotation(crate[i]);
        float dx = wx - p.x, dy = wy - p.y;
        float lx = r.c*dx + r.s*dy, ly = -r.s*dx + r.c*dy;   // into the crate's local frame
        if (fabsf(lx) <= CH + 0.06f && fabsf(ly) <= CH + 0.06f) { *out = crate[i]; return true; }
    }
    return false;
}

void update(void) {
    if (!built) reset_world();
    if (keyp('R')) reset_world();
    if (ex_age >= 0 && ++ex_age > 10) ex_age = -1;   // age the blast flash

    int mx = mouse_x(), my = mouse_y();
    float mwx = WX(mx), mwy = WY(my);

    if (mouse_pressed(MOUSE_LEFT)) {
        float gx = mwx - LX, gy = mwy - LY;
        b2BodyId pick;
        if (gx*gx + gy*gy < 0.9f*0.9f) {                    // grabbed the slingshot pouch -> load a shot
            mmode = M_AIM;
        } else if (body_at(mwx, mwy, &pick)) {              // grabbed any body -> drag it with a mouse joint
            b2MouseJointDef d = b2DefaultMouseJointDef();
            d.bodyIdA = ground_body;
            d.bodyIdB = pick;
            d.target = (b2Vec2){mwx, mwy};
            d.hertz = 6.0f; d.dampingRatio = 0.7f;
            d.maxForce = 900.0f * b2Body_GetMass(pick);     // scale with mass so it can drag heavy bodies
            mjoint = b2CreateMouseJoint(world, &d);
            b2Body_SetAwake(pick, true);
            mmode = M_DRAG;
        }
    }
    if (mmode == M_DRAG && mouse_down(MOUSE_LEFT))
        b2MouseJoint_SetTarget(mjoint, (b2Vec2){mwx, mwy});

    if (mouse_released(MOUSE_LEFT)) {
        if (mmode == M_AIM) {                               // fly a ball off the slingshot
            b2Vec2 v = launch_vel(mx, my);
            float sp = sqrtf(v.x*v.x + v.y*v.y);
            int vol = 3 + (int)(sp * 0.14f); if (vol > 7) vol = 7;
            hit(55 + rnd(5), SL_TWANG, vol, 170);           // the release twang
            spawn_ball(LX, LY, v.x, v.y, BR);
        } else if (mmode == M_DRAG) {
            b2DestroyJoint(mjoint);
        }
        mmode = M_NONE;
    }

    if (keyp(KEY_SPACE) && nball > 0 && mmode != M_DRAG) {  // detonate the last ball into shrapnel
        hit(30, INSTR_NOISE, 6, 120);            // boom
        explode_ball(nball - 1);
    }

    b2World_Step(world, 1.0f/60.0f, 4);

    b2ContactEvents ev = b2World_GetContactEvents(world);   // impact sounds (capped per frame)
    for (int i = 0, played = 0; i < ev.hitCount && played < 4; i++) {
        b2ContactHitEvent *h = &ev.hitEvents[i];
        if (h->approachSpeed < 1.5f) continue;
        int ta = (int)(intptr_t)b2Body_GetUserData(b2Shape_GetBody(h->shapeIdA));
        int tb = (int)(intptr_t)b2Body_GetUserData(b2Shape_GetBody(h->shapeIdB));
        impact_sound(ta, tb, h->approachSpeed);
        played++;
    }
    // no culling — the bouncy walls contain every ball, and we keep them all

#ifdef DE_TRACE
    watch("toppled", "%d", count_toppled());
    watch("balls",   "%d", nball);
#endif
}

static void draw_crate(b2BodyId id) {
    b2Vec2 p = b2Body_GetPosition(id);
    b2Rot  r = b2Body_GetRotation(id);
    const float lx[4] = {-CH, CH, CH, -CH}, ly[4] = {-CH, -CH, CH, CH};
    int xy[8];
    for (int i = 0; i < 4; i++) {
        float wx = p.x + (r.c*lx[i] - r.s*ly[i]);
        float wy = p.y + (r.s*lx[i] + r.c*ly[i]);
        xy[i*2] = SX(wx); xy[i*2+1] = SY(wy);
    }
    polyfill(xy, 4, CLR_ORANGE);
    for (int i = 0; i < 4; i++) {               // outline + X brace = crate look
        int a = i*2, b = ((i+1)%4)*2;
        line(xy[a], xy[a+1], xy[b], xy[b+1], CLR_BROWN);
    }
    line(xy[0], xy[1], xy[4], xy[5], CLR_BROWN);
    line(xy[2], xy[3], xy[6], xy[7], CLR_BROWN);
}

static void draw_launcher(void) {
    int sx = SX(LX), sy = SY(LY);
    line(sx, sy, sx, SY(FLOORY), CLR_BROWN);    // post
    line(sx, sy, sx - 5, sy - 8, CLR_BROWN);    // fork
    line(sx, sy, sx + 5, sy - 8, CLR_BROWN);
}

static void draw_aim(void) {
    int sx = SX(LX), sy = SY(LY), mx = mouse_x(), my = mouse_y();
    line(sx - 5, sy - 8, mx, my, CLR_LIGHT_GREY);       // rubber band
    line(sx + 5, sy - 8, mx, my, CLR_LIGHT_GREY);
    circfill(mx, my, (int)(BR*PPM), CLR_DARK_GREY);     // pouch ball
    circ(mx, my, (int)(BR*PPM), CLR_LIGHT_GREY);
    b2Vec2 v = launch_vel(mx, my);                       // dotted trajectory preview
    float px = LX, py = LY, dt = 1.0f/60.0f, vy = v.y;
    for (int i = 0; i < 48; i++) {
        vy += -10.0f * dt;
        px += v.x * dt; py += vy * dt;
        if (py < FLOORY) break;
        if (i % 3 == 0) pset(SX(px), SY(py), CLR_YELLOW);
    }
}

void draw(void) {
    if (!built) { cls(CLR_DARK_BLUE); return; }
    cls(CLR_DARK_BLUE);

    int fy = SY(FLOORY);
    rectfill(0, fy, SCREEN_W, SCREEN_H - fy, CLR_DARK_GREEN);
    line(0, fy, SCREEN_W, fy, CLR_GREEN);

    line(0, 0, 0, fy, CLR_DARK_GREY);                       // arena walls the ball bounces off
    line(SCREEN_W - 1, 0, SCREEN_W - 1, fy, CLR_DARK_GREY);
    line(0, 0, SCREEN_W, 0, CLR_DARK_GREY);

    for (int i = 0; i < ncrate; i++) draw_crate(crate[i]);
    for (int i = 0; i < nball; i++) {
        b2Vec2 p = b2Body_GetPosition(ball[i]);
        int r = (int)(ballr[i]*PPM); if (r < 1) r = 1;
        circfill(SX(p.x), SY(p.y), r, CLR_DARK_GREY);
        circ(SX(p.x), SY(p.y), r, CLR_LIGHT_GREY);
    }
    draw_launcher();
    if (mmode == M_AIM) draw_aim();

    if (ex_age >= 0) {                                       // blast: expanding ring + bright core, cooling
        int r = 4 + ex_age * 6;
        int col = ex_age < 2 ? CLR_WHITE : ex_age < 4 ? CLR_YELLOW : ex_age < 7 ? CLR_ORANGE : CLR_RED;
        circ((int)ex_x, (int)ex_y, r, col);
        if (ex_age < 3) circfill((int)ex_x, (int)ex_y, 7 - ex_age*2, CLR_WHITE);
    }

    char buf[32];
    snprintf(buf, sizeof buf, "TOPPLED %d/%d  BALLS %d", count_toppled(), ncrate, nball);
    print(buf, 4, 4, CLR_WHITE);
    print("POUCH=SHOOT  GRAB=DRAG  SPACE BOOM  R", 4, SCREEN_H - 10, CLR_LIGHT_GREY);
}
