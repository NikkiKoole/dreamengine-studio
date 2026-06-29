/* de:meta
{
  "title": "Menigte",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "schedule-driven-agents",
    "camera-follow"
  ],
  "lineage": "Scale experiment companion to procplaces/sloop; the novel thesis is that 10k always-live NPCs are cheap when each position is a pure function of (identity, time-of-day) via a piecewise keyframe lerp — expensive walk-sim reserved for the visible hot tier only.",
  "description": "DRIVE a car through a living town of 10,000 NPCs. The SCALE experiment: every person is always alive because 'having a life' is not 'running a simulation' - each one's position is a PURE FUNCTION of (identity, time-of-day): home, commute (along the road grid), work, commute home, sleep. No pathfinding, just a few compares and a lerp, so all 10k stay live every frame even off-screen. The expensive walk-anim is reserved for the visible few (zoom in and the dots become little walking figures). Watch the tide: morning ORANGE rush toward downtown, midday BLUE at work, evening GREEN back to the suburbs. Drive: UP/W gas, DOWN/S brake-reverse, LEFT-RIGHT / A-D steer; Z/X or wheel zoom; F free camera; ,/. clock speed; T +1h; SPACE re-roll town; H anchors. Companion to procplaces (the world) and sloop (the full car sim)."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// MENIGTE — 10,000 people, one living town. The SCALE experiment for the
// "drive a large world full of meaningful NPCs" idea (see sloop / procplaces).
//
// The thesis it proves: 10k NPCs is CHEAP, because "having a life" is not the
// same as "running a simulation". Every person's position is a *pure function*
// of (identity, time-of-day): an archetype (worker / student / night-shift /
// homebody) gives them a day expressed as keyframes — home, commute the road grid,
// work or school, maybe lunch out, an errand on the way home, sleep — and the
// position is just a piecewise lerp between them. No pathfinding, no per-step
// integration. 10k of those per frame is nothing, so all 10k are ALWAYS live with
// a correct position, even off-screen. There is no "dehydration" of state.
//
// The expensive stuff (walk animation, steering, later road collision) is the HOT
// tier — reserved for the handful actually on screen. Zoomed out, everyone is a
// coloured dot at their closed-form spot; zoom in past HOT_ZOOM and the visible
// few become little walking figures that EASE toward that same spot (seeded there
// on entry, so nobody pops at the screen edge). Colour reads what they're doing:
//   ORANGE → heading to work/school     GREEN → heading home
//   YELLOW → off to (or at) a café/shop  BLUE → at work   PINK → at school
//   dim/grey → home (darker at night)
//
// You DRIVE a car through it (a simple top-down arcade model): the camera follows
// the car and the town's daily tide flows around you. Press F to detach into a
// free observer camera.
//
//   ↑/W gas   ↓/S brake-reverse   ←→ / A D steer      Z / X (or wheel)  zoom
//   F  toggle free camera (pan with WASD/arrows)       , / .  clock speed
//   T  jump +1 hour     SPACE  re-roll the town        H  toggle anchor markers

#define NPOP        10000
#define WORLD_W     3200          // a town you cross in a few seconds — dense enough to
#define WORLD_H     2300          // feel populated while driving (10k is cheap at any size)
#define NWORK       24            // workplaces (downtown cluster + a few outliers)
#define NSCHOOL     6             // schools — students' day anchor
#define NLEISURE    24            // cafés / shops — lunch + after-work errands
#define DAY_SECS    720.0f        // real seconds per in-game day at 1x (12 min; press . to fast-forward)
#define HOT_ZOOM    0.8f          // at/above this zoom, on-screen people get the walk-sim
#define NHOT        768           // walk-sim slots (only the visible few are ever HOT)
#define TR_DAY      1.0f          // home <-> work/school travel window (hours) — a calm walk
#define TR_LEIS     0.5f          // hop to a nearby café/shop
#define MAXKF       13            // most keyframes a day's itinerary can need
#define CAR_ACCEL   240.0f        // throttle, world px/s^2
#define CAR_DRAG    1.6f          // velocity decay per second
#define CAR_VMAX    600.0f        // top speed, world px/s
#define CAR_TURN    2.8f          // steer authority (rad/s), scaled by speed

// who someone is decides their day — archetype is a pure function of their hash
enum { AR_WORKER, AR_STUDENT, AR_NIGHT, AR_HOMEBODY };
// the kinds of place a person visits (drives dwell colour + "heading there" colour)
enum { PT_HOME, PT_WORK, PT_SCHOOL, PT_LEISURE };
// render states the draw loop colours by (dwelling vs moving, and toward what)
enum { RS_HOME, RS_WORK, RS_SCHOOL, RS_LEISURE, RS_OUT, RS_BACK, RS_TOLEIS };

typedef struct {
    float hx, hy;     // home
    float dx, dy;     // day anchor — work (workers/night) or school (students)
    float lx, ly;     // a café/shop near home: lunch + an after-work errand
    float leave, ret; // hour they leave home / leave the day anchor (jittered)
    int   arch;       // AR_*
    int   lunch;      // 1 = goes out for lunch
    int   errand;     // 1 = stops at the shop on the way home
    int   hot;        // walk-sim slot while HOT, else -1
} Person;

typedef struct { float h, x, y; int pt; } KF;   // itinerary keyframe: be at (x,y) [kind pt] by hour h

// HOT tier: a smoothed walk-sim that EASES toward the closed-form schedule
// position. The invariant (see docs/design/menigte.md): sx/sy must track the
// closed-form (x,y) so nobody pops when they cross the screen edge — on claim we
// seed sx/sy = closed-form, so at the boundary sim == truth.
typedef struct { int idx; float sx, sy; int seen; } Hot;
static Hot hot[NHOT];
static int hotFree[NHOT], nHotFree;

static Person  pop[NPOP];
static float   workX[NWORK],       workY[NWORK];
static float   schoolX[NSCHOOL],   schoolY[NSCHOOL];
static float   leisureX[NLEISURE], leisureY[NLEISURE];
static float   ccx, ccy;          // camera centre, world coords
static float   zoom = 1.7f;       // street-level: you drive among walking figures
static float   clock_h;           // current hour 0..24
static float   speed = 1.0f;      // clock multiplier
static int     show_anchors = 1;
static int     seed = 1;

// the car — a simple top-down arcade vehicle you drive through the living town
static float   carX, carY, carH;  // world position + heading (radians, 0 = east)
static float   carV;              // signed speed, world px/s
static int     freecam = 0;       // F: detach the camera to free-pan / observe

// --- deterministic identity: everything is a pure function of index + seed ---
static unsigned hsh(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16; return x;
}
static float h01(unsigned a, unsigned b) {          // hash → [0,1)
    return (hsh(a * 2654435761U ^ (b + 0x9e3779b9U)) & 0xFFFFFF) / (float)0x1000000;
}

static void reset_hot(void) {
    nHotFree = NHOT;
    for (int s = 0; s < NHOT; s++) { hot[s].idx = -1; hotFree[s] = s; }
}

static void build_town(void) {
    unsigned s = (unsigned)seed;
    reset_hot();
    // work anchors: most clustered in a central downtown, a few scattered
    for (int j = 0; j < NWORK; j++) {
        if (j < NWORK - 5) {                         // a tight, busy downtown
            workX[j] = WORLD_W * 0.5f + (h01(s, 100 + j) - 0.5f) * WORLD_W * 0.22f;
            workY[j] = WORLD_H * 0.5f + (h01(s, 200 + j) - 0.5f) * WORLD_H * 0.22f;
        } else {                                     // outlying sites
            workX[j] = h01(s, 300 + j) * WORLD_W;
            workY[j] = h01(s, 400 + j) * WORLD_H;
        }
    }
    for (int j = 0; j < NSCHOOL; j++) {              // schools: out among the suburbs
        schoolX[j] = h01(s, 500 + j) * WORLD_W;
        schoolY[j] = h01(s, 600 + j) * WORLD_H;
    }
    for (int j = 0; j < NLEISURE; j++) {             // cafés / shops: everywhere
        leisureX[j] = h01(s, 700 + j) * WORLD_W;
        leisureY[j] = h01(s, 800 + j) * WORLD_H;
    }
    for (int i = 0; i < NPOP; i++) {
        Person *p = &pop[i];
        p->hx = h01(s, i)        * WORLD_W;           // home: scattered everywhere
        p->hy = h01(s, i + NPOP) * WORLD_H;
        float r = h01(s, i + 4*NPOP);                 // pick an archetype
        p->arch = r < 0.60f ? AR_WORKER : r < 0.78f ? AR_STUDENT
                : r < 0.90f ? AR_NIGHT  : AR_HOMEBODY;
        if (p->arch == AR_STUDENT) {                  // day anchor: school for students,
            int j = hsh(s ^ (i + 222)) % NSCHOOL;  p->dx = schoolX[j]; p->dy = schoolY[j];
        } else {                                      // work for everyone else
            int j = hsh(s ^ (i + 7777)) % NWORK;   p->dx = workX[j];   p->dy = workY[j];
        }
        int lj = hsh(s ^ (i + 999)) % NLEISURE; p->lx = leisureX[lj]; p->ly = leisureY[lj];
        if (p->arch == AR_STUDENT) {                  // school day
            p->leave = 7.5f  + h01(s, i + 2*NPOP) * 1.0f;
            p->ret   = 14.5f + h01(s, i + 3*NPOP) * 1.5f;
        } else if (p->arch == AR_NIGHT) {             // night shift — out in the evening
            p->leave = 21.5f + h01(s, i + 2*NPOP) * 1.5f;
            p->ret   = 6.0f  + h01(s, i + 3*NPOP) * 1.0f;
        } else {                                      // 9-to-5 (homebodies ignore these)
            p->leave = 6.5f  + h01(s, i + 2*NPOP) * 2.5f;
            p->ret   = 16.0f + h01(s, i + 3*NPOP) * 3.0f;
        }
        p->lunch  = (p->arch == AR_WORKER) && (hsh(s ^ (i + 11)) & 1);
        p->errand = (p->arch == AR_WORKER || p->arch == AR_STUDENT) && (hsh(s ^ (i + 33)) % 3 == 0);
        p->hot    = -1;
    }
}

// --- the road field (mirrors procplaces' lattice: streets at multiples of PITCH,
//     class by line index %3/9/27). Kept self-contained for now; the real
//     integration is a shared roads.h both carts include — see docs/design/menigte.md.
#define PITCH 100
static float snap_road(float v) { return roundf(v / PITCH) * PITCH; }

// Closed-form point at t∈[0,1] along an L-shaped route a→b that HUGS the grid:
// hop onto the street row nearest the origin, run along it, turn down the street
// column nearest the destination, hop to the door. Axis-aligned legs = it reads as
// using roads, and it's still pure (a piecewise lerp) — so all 10k stay cheap.
static void route_point(float ax, float ay, float bx, float by, float t,
                        float *ox, float *oy) {
    float row = snap_road(ay), col = snap_road(bx);
    float wx[5] = { ax, ax,  col, col, bx };
    float wy[5] = { ay, row, row, by,  by };
    float seg[4], total = 0;
    for (int k = 0; k < 4; k++) { seg[k] = fabsf(wx[k+1]-wx[k]) + fabsf(wy[k+1]-wy[k]); total += seg[k]; }
    if (total < 1e-3f) { *ox = ax; *oy = ay; return; }
    float d = t * total;
    for (int k = 0; k < 4; k++) {
        if (d <= seg[k] || k == 3) {
            float f = seg[k] > 1e-3f ? d / seg[k] : 0.0f;
            *ox = lerp(wx[k], wx[k+1], f); *oy = lerp(wy[k], wy[k+1], f); return;
        }
        d -= seg[k];
    }
}

// Build a person's day as keyframes "be at (x,y) [kind pt] by hour h". Consecutive
// same-place keyframes = a dwell; different places = travel between them. A pure
// function of the person, rebuilt per query — a dozen writes, cheap even ×10k.
static int build_itin(const Person *p, KF *k) {
    int n = 0;
    #define ADD(H,X,Y,PT) do { k[n].h=(H); k[n].x=(X); k[n].y=(Y); k[n].pt=(PT); n++; } while (0)
    if (p->arch == AR_HOMEBODY) {                        // never leaves
        ADD(0.0f,  p->hx, p->hy, PT_HOME);
        ADD(24.0f, p->hx, p->hy, PT_HOME);
    } else if (p->arch == AR_NIGHT) {                    // works overnight, sleeps by day
        ADD(0.0f,              p->dx, p->dy, PT_WORK);   // still at work past midnight
        ADD(p->ret,            p->dx, p->dy, PT_WORK);   // clock off in the morning
        ADD(p->ret + TR_DAY,   p->hx, p->hy, PT_HOME);
        ADD(p->leave,          p->hx, p->hy, PT_HOME);   // home / asleep all day
        ADD(p->leave + TR_DAY, p->dx, p->dy, PT_WORK);   // back out in the evening
        ADD(24.0f,             p->dx, p->dy, PT_WORK);
    } else {                                             // worker / student
        int dpt = (p->arch == AR_STUDENT) ? PT_SCHOOL : PT_WORK;
        ADD(0.0f,               p->hx, p->hy, PT_HOME);
        ADD(p->leave,           p->hx, p->hy, PT_HOME);
        ADD(p->leave + TR_DAY,  p->dx, p->dy, dpt);
        if (p->lunch) {                                  // out for lunch, then back
            float L = 12.3f;
            ADD(L,                      p->dx, p->dy, dpt);
            ADD(L + TR_LEIS,            p->lx, p->ly, PT_LEISURE);
            ADD(L + TR_LEIS + 0.6f,     p->lx, p->ly, PT_LEISURE);
            ADD(L + 2*TR_LEIS + 0.6f,   p->dx, p->dy, dpt);
        }
        ADD(p->ret,             p->dx, p->dy, dpt);
        if (p->errand) {                                 // shop on the way home
            ADD(p->ret + TR_LEIS,            p->lx, p->ly, PT_LEISURE);
            ADD(p->ret + 2*TR_LEIS,          p->lx, p->ly, PT_LEISURE);
            ADD(p->ret + 2*TR_LEIS + TR_DAY, p->hx, p->hy, PT_HOME);
        } else {
            ADD(p->ret + TR_DAY,    p->hx, p->hy, PT_HOME);
        }
        ADD(24.0f,              p->hx, p->hy, PT_HOME);
    }
    #undef ADD
    return n;
}

// closed-form position + render-state at a given hour — the whole "AI", for all 10k
static void person_at(const Person *p, float h, float *ox, float *oy, int *rs) {
    KF k[MAXKF];
    int n = build_itin(p, k);
    for (int i = 0; i < n - 1; i++) {
        if (h < k[i].h || h >= k[i+1].h) continue;
        if (k[i].x == k[i+1].x && k[i].y == k[i+1].y) {            // dwelling at a place
            *ox = k[i].x; *oy = k[i].y;
            *rs = k[i].pt == PT_WORK ? RS_WORK : k[i].pt == PT_SCHOOL ? RS_SCHOOL
                : k[i].pt == PT_LEISURE ? RS_LEISURE : RS_HOME;
        } else {                                                   // travelling i → i+1
            route_point(k[i].x, k[i].y, k[i+1].x, k[i+1].y, (h - k[i].h) / (k[i+1].h - k[i].h), ox, oy);
            *rs = k[i+1].pt == PT_HOME ? RS_BACK : k[i+1].pt == PT_LEISURE ? RS_TOLEIS : RS_OUT;
        }
        return;
    }
    *ox = p->hx; *oy = p->hy; *rs = RS_HOME;                       // fallback (h past last kf)
}

void init(void) {
    build_town();
    carX = ccx = WORLD_W * 0.5f; carY = ccy = WORLD_H * 0.5f;
    carH = 0; carV = 0; clock_h = 8.5f;
}

void update(void) {
    if (key('Z')) zoom *= 1.04f;
    if (key('X')) zoom *= 0.96f;
    zoom += mouse_wheel() * 0.03f * zoom;
    if (zoom < 0.05f) zoom = 0.05f; if (zoom > 6.0f) zoom = 6.0f;
    if (keyp('F')) freecam = !freecam;

    if (freecam) {                                 // detached observer: pan the camera
        float pan = 14.0f / zoom;
        if (btn(0, BTN_LEFT)  || key('A')) ccx -= pan;
        if (btn(0, BTN_RIGHT) || key('D')) ccx += pan;
        if (btn(0, BTN_UP)    || key('W')) ccy -= pan;
        if (btn(0, BTN_DOWN)  || key('S')) ccy += pan;
    } else {                                       // drive the car; camera follows it
        float thr   = (btn(0, BTN_UP)    || key('W')) - (btn(0, BTN_DOWN) || key('S'));
        float steer = (btn(0, BTN_RIGHT) || key('D')) - (btn(0, BTN_LEFT) || key('A'));
        carV += thr * CAR_ACCEL * dt();
        carV -= carV * CAR_DRAG * dt();            // drag → coasts to a stop
        if (carV >  CAR_VMAX)        carV =  CAR_VMAX;
        if (carV < -CAR_VMAX * 0.35f) carV = -CAR_VMAX * 0.35f;   // slow reverse
        carH += steer * CAR_TURN * (carV / CAR_VMAX) * dt();      // can't turn while stopped
        carX += cosf(carH) * carV * dt();
        carY += sinf(carH) * carV * dt();
        ccx = carX; ccy = carY;
    }

    if (keyp(','))  speed = speed <= 1.0f ? 0.25f : speed - 1.0f;
    if (keyp('.')) speed += 1.0f;
    if (speed > 64.0f) speed = 64.0f;
    if (keyp('T')) clock_h += 1.0f;
    if (keyp('H')) show_anchors = !show_anchors;
    if (keyp(KEY_SPACE)) { seed++; build_town(); }

    clock_h += (24.0f / DAY_SECS) * speed * dt();
    while (clock_h >= 24.0f) clock_h -= 24.0f;
}

static int act_col(int rs, int night) {
    switch (rs) {
        case RS_OUT:     return CLR_ORANGE;                    // heading to work/school
        case RS_BACK:    return CLR_GREEN;                     // heading home
        case RS_TOLEIS:  return CLR_YELLOW;                    // off to a café/shop
        case RS_WORK:    return CLR_BLUE;
        case RS_SCHOOL:  return CLR_PINK;
        case RS_LEISURE: return CLR_YELLOW;
        default:         return night ? CLR_DARK_GREY : CLR_LIGHT_GREY;  // home
    }
}

// the visible street grid (same lattice route_point hugs): class by line index,
// wider/distinct for arterials & highways so the hierarchy reads from above
static int road_class(int k) { return (k % 27 == 0) ? 3 : (k % 9 == 0) ? 2 : (k % 3 == 0) ? 1 : 0; }
static void draw_roads(float minx, float maxx, float miny, float maxy) {
    static const int W[4] = { 1, 2, 3, 5 };
    static const int C[4] = { CLR_DARK_GREY, CLR_MEDIUM_GREY, CLR_MEDIUM_GREY, CLR_DARK_GREY };
    for (int k = (int)floorf(minx / PITCH); k <= (int)ceilf(maxx / PITCH); k++) {
        int cls = road_class(k), w = W[cls];
        rectfill(k * PITCH - w / 2, (int)miny, w, (int)(maxy - miny), C[cls]);
    }
    for (int j = (int)floorf(miny / PITCH); j <= (int)ceilf(maxy / PITCH); j++) {
        int cls = road_class(j), w = W[cls];
        rectfill((int)minx, j * PITCH - w / 2, (int)(maxx - minx), w, C[cls]);
    }
}

// landmark buildings at the work / school / shop anchors (people dwell at the
// centre, drawn on top). Colour-matched to the activity palette so they reinforce
// the legend: blue offices, pink school, yellow-awning shop.
static void draw_tower(int cx, int cy) {
    int w = 12, h = 18, x = cx - w / 2, y = cy - h / 2;
    rectfill(x, y, w, h, CLR_INDIGO);
    rect(x, y, w, h, CLR_DARK_BLUE);
    for (int wy = y + 2; wy < y + h - 2; wy += 4)        // lit windows
        for (int wx = x + 2; wx < x + w - 2; wx += 4)
            pset(wx, wy, CLR_YELLOW);
}
static void draw_school(int cx, int cy) {
    int w = 18, h = 11, x = cx - w / 2, y = cy - h / 2;
    rectfill(x, y, w, h, CLR_PINK);
    rect(x, y, w, h, CLR_DARK_PURPLE);
    for (int wx = x + 3; wx < x + w - 3; wx += 4)         // window row
        rectfill(wx, y + 3, 2, 2, CLR_WHITE);
    rectfill(cx - 1, y + h - 4, 3, 4, CLR_DARK_PURPLE);   // door
}
static void draw_shop(int cx, int cy) {
    int w = 9, h = 7, x = cx - w / 2, y = cy - h / 2;
    rectfill(x, y, w, h, CLR_LIGHT_GREY);
    rectfill(x, y, w, 2, CLR_YELLOW);                     // awning
    rect(x, y, w, h, CLR_DARK_GREY);
}

// the player's car — an oriented quad (two trifills) + a white nose, world space
static void draw_car(void) {
    float c = cosf(carH), s = sinf(carH);
    float L = 7.0f, W = 4.0f;                      // half length / half width
    float fx = carX + c * L, fy = carY + s * L;    // front + back centres
    float bx = carX - c * L, by = carY - s * L;
    float px = -s * W, py = c * W;                 // perpendicular (half width)
    int flx = (int)(fx + px), fly = (int)(fy + py), frx = (int)(fx - px), fry = (int)(fy - py);
    int blx = (int)(bx + px), bly = (int)(by + py), brx = (int)(bx - px), bry = (int)(by - py);
    trifill(flx, fly, frx, fry, brx, bry, CLR_RED);
    trifill(flx, fly, brx, bry, blx, bly, CLR_RED);
    line((int)carX, (int)carY, (int)fx, (int)fy, CLR_WHITE);   // heading / windshield
}

// a little person (world space): skin head, body in their activity colour, legs
// that splay + a body bob while walking. ~6px tall so it reads as a figure, not a dot.
static void draw_walker(int i, float fx, float fy, int rs, int c) {
    int bx = (int)fx, by = (int)fy;
    int moving = (rs == RS_OUT || rs == RS_BACK || rs == RS_TOLEIS);
    int skin = (hsh(i) & 1) ? CLR_PEACH : CLR_LIGHT_PEACH;
    float ph = now() * 6.0f + i;                  // gait phase, de-synced per person
    int step = moving && (sinf(ph) > 0.0f);
    int bob  = (moving && sinf(ph) > 0.6f) ? -1 : 0;
    int top  = by - 5 + bob;                      // feet land near (bx, by)
    rectfill(bx - 1, top,     2, 2, skin);        // head
    rectfill(bx - 1, top + 2, 2, 3, c);           // torso, in the activity colour
    pset(bx - 1 - (step ? 1 : 0), top + 5, c);    // legs splay while walking
    pset(bx + 1 + (step ? 1 : 0), top + 5, c);
}

void draw(void) {
    // day/night sky: lerp a couple of bands by hour
    int sky = (clock_h < 5 || clock_h > 21) ? CLR_BLACK
            : (clock_h < 7 || clock_h > 19) ? CLR_DARK_BLUE
            :                                 CLR_INDIGO;
    cls(sky);
    int night = (clock_h < 6.0f || clock_h > 20.0f);

    camera_ex((int)(ccx - SCREEN_W / 2.0f), (int)(ccy - SCREEN_H / 2.0f), zoom, 0);

    // visible world rect (+margin) — cull off-screen people from the DRAW only
    float hw = (SCREEN_W / 2.0f) / zoom + 4, hh = (SCREEN_H / 2.0f) / zoom + 4;
    float minx = ccx - hw, maxx = ccx + hw, miny = ccy - hh, maxy = ccy + hh;

    draw_roads(minx, maxx, miny, maxy);

    if (show_anchors) {                                // the town's landmark buildings
        for (int j = 0; j < NWORK; j++)    draw_tower((int)workX[j],    (int)workY[j]);
        for (int j = 0; j < NSCHOOL; j++)  draw_school((int)schoolX[j],  (int)schoolY[j]);
        for (int j = 0; j < NLEISURE; j++) draw_shop((int)leisureX[j],   (int)leisureY[j]);
    }

    int doHot = (zoom >= HOT_ZOOM);
    for (int s = 0; s < NHOT; s++) hot[s].seen = 0;    // mark all slots stale

    int shown = 0, nhot = 0;
    for (int i = 0; i < NPOP; i++) {
        float x, y; int rs;
        person_at(&pop[i], clock_h, &x, &y, &rs);   // ← always computed: the life
        if (x < minx || x > maxx || y < miny || y > maxy) continue;  // ← but only drawn if visible
        int c = act_col(rs, night);

        if (doHot) {                                   // HOT: the smoothed walk-sim
            if (pop[i].hot < 0 && nHotFree > 0) {      // entering HOT — seed sim AT the
                int s = hotFree[--nHotFree];           // closed-form truth (no edge-pop)
                pop[i].hot = s; hot[s].idx = i; hot[s].sx = x; hot[s].sy = y;
            }
            if (pop[i].hot >= 0) {
                Hot *h = &hot[pop[i].hot];
                h->seen = 1;
                h->sx = lerp(h->sx, x, 0.30f);         // ease toward truth — must track it,
                h->sy = lerp(h->sy, y, 0.30f);         // or people pop at the screen edge
                draw_walker(i, h->sx, h->sy, rs, c);
                shown++; nhot++;
                continue;
            }
        }
        pset((int)x, (int)y, c);                       // WARM (or HOT pool full): a dot
        if (zoom > 1.2f) pset((int)x + 1, (int)y, c);
        shown++;
    }
    // reclaim slots whose person left view or dropped below HOT zoom this frame
    for (int s = 0; s < NHOT; s++)
        if (hot[s].idx >= 0 && !hot[s].seen) {
            pop[hot[s].idx].hot = -1; hot[s].idx = -1; hotFree[nHotFree++] = s;
        }

    draw_car();                                    // on top of the crowd

    // HUD (screen space) — one compact line
    camera(0, 0);
    int hr = (int)clock_h, mn = (int)((clock_h - hr) * 60);
    char buf[80];
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    if (freecam) snprintf(buf, sizeof buf, "%02d:%02d x%.2g FREECAM", hr, mn, speed);
    else         snprintf(buf, sizeof buf, "%02d:%02d x%.2g %dkm/h", hr, mn, speed, (int)(fabsf(carV) * 0.15f));
    print(buf, 3, 1, CLR_WHITE);
    snprintf(buf, sizeof buf, "vis%d hot%d %dfps", shown, nhot, fps());
    print(buf, SCREEN_W - 8 * (int)strlen(buf) + 2, 1, CLR_LIGHT_GREY);
#ifdef DE_TRACE
    watch("hour", "%.2f", clock_h);
    watch("drawn", "%d", shown);
    watch("hot", "%d", nhot);
    watch("carV", "%.1f", carV);
    watch("carX", "%.1f", carX);
    watch("carY", "%.1f", carY);
#endif
}
