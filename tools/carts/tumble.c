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
  "description": "A slingshot and a stack of crates. Drag to pull the pouch back, release to fling a heavy ball, and knock the pyramid down. Every crate is a real Box2D v3 rigid body — it rotates as it topples, rests solidly on the crate below (stable stacking, the thing a verlet blob can't do), and slides on friction. TOPPLED counts how many you've knocked over; R rebuilds the stack. First cart on the engine's new pure-C Box2D backend."
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

#define NCRATE  10
#define NBALL   14
#define CH      0.28f          // crate half-extent (m)  -> ~13px box
#define BR      0.34f          // ball radius (m)        -> ~8px
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
static int       nball  = 0;
static bool      aiming = false;
static int       built  = 0;

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

static void spawn_ball(float x, float y, float vx, float vy) {
    if (nball >= NBALL) {                       // recycle the oldest
        b2DestroyBody(ball[0]);
        for (int i = 1; i < nball; i++) ball[i-1] = ball[i];
        nball--;
    }
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){x, y};
    bd.linearVelocity = (b2Vec2){vx, vy};
    bd.userData = (void*)(intptr_t)T_BALL;
    b2BodyId id = b2CreateBody(world, &bd);
    b2Circle c = {{0.0f, 0.0f}, BR};
    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 5.0f;                           // heavy — it bowls crates over
    sd.material.friction = 0.4f;
    sd.material.restitution = 0.15f;
    sd.enableHitEvents = true;                   // -> impact sounds
    b2CreateCircleShape(id, &sd, &c);
    ball[nball++] = id;
}

static void build_stack(void) {
    // pyramid: rows of 4,3,2,1 from the floor up, centred right of the arena
    float cx = 9.8f, step = 2.0f*CH + 0.02f, y0 = FLOORY + CH;
    int k = 0;
    for (int row = 0; row < 4 && k < NCRATE; row++) {
        int n = 4 - row;
        float left = cx - (n*step)/2.0f + CH;
        for (int i = 0; i < n && k < NCRATE; i++) {
            float x = left + i*step, y = y0 + row*step;
            crate[k]  = make_crate(x, y);
            crate0[k] = (b2Vec2){x, y};
            k++;
        }
    }
}

static void reset_world(void) {
    if (built) b2DestroyWorld(world);           // frees every body it owns
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){0.0f, -10.0f};
    world = b2CreateWorld(&wd);
    built = 1;
    nball = 0;
    aiming = false;

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
    for (int i = 0; i < NCRATE; i++) {
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

void update(void) {
    if (!built) reset_world();
    if (keyp('R')) reset_world();

    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT))  aiming = true;
    if (aiming && mouse_released(MOUSE_LEFT)) {
        b2Vec2 v = launch_vel(mx, my);
        float sp = sqrtf(v.x*v.x + v.y*v.y);
        int vol = 3 + (int)(sp * 0.14f); if (vol > 7) vol = 7;
        hit(55 + rnd(5), SL_TWANG, vol, 170);   // the release twang
        spawn_ball(LX, LY, v.x, v.y);
        aiming = false;
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

    for (int i = 0; i < nball; ) {              // cull balls that leave the arena
        b2Vec2 p = b2Body_GetPosition(ball[i]);
        if (p.y < -1.0f || p.x < -2.0f || p.x > SCREEN_W/PPM + 2.0f) {
            b2DestroyBody(ball[i]);
            for (int j = i+1; j < nball; j++) ball[j-1] = ball[j];
            nball--;
        } else i++;
    }

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

    for (int i = 0; i < NCRATE; i++) draw_crate(crate[i]);
    for (int i = 0; i < nball; i++) {
        b2Vec2 p = b2Body_GetPosition(ball[i]);
        circfill(SX(p.x), SY(p.y), (int)(BR*PPM), CLR_DARK_GREY);
        circ(SX(p.x), SY(p.y), (int)(BR*PPM), CLR_LIGHT_GREY);
    }
    draw_launcher();
    if (aiming) draw_aim();

    char buf[32];
    snprintf(buf, sizeof buf, "TOPPLED %d/%d", count_toppled(), NCRATE);
    print(buf, 4, 4, CLR_WHITE);
    print("DRAG AIM  R RESET", 4, SCREEN_H - 10, CLR_LIGHT_GREY);
}
