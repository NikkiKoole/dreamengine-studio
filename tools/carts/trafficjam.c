#include "studio.h"

// ── TRAFFIC JAM ───────────────────────────────────────────────────────────────
// A generative cars toy in the rectangular style of Donkey Kong / Boulder Dash /
// the Love Parade — every vehicle is built from rectfill / polyfill / circfill,
// no sprite art. A car is a BAG OF GENE-DIMENSIONS rolled within its TYPE, so the
// same "sedan" is never the same sedan twice. The ten knobs that vary one car:
//
//   1 length      2 body height   3 roof height    4 cabin length  5 cabin position
//   6 windscreen rake   7 backlight rake   8 wheel radius   9 ground clearance
//  10 wheelbase   (+ colours, two-tone, passengers, spoiler, roof-rack, antenna…)
//
// 17 types: micro hatch sedan wagon suv mpv coupe sports supercar muscle luxury
//           convertible antique pickup van bus  and a long-ass semi truck.
//
// They roll past in a four-lane procession governed by a TRAFFIC LIGHT — go red and
// a stop-and-go jam piles up behind the line and ripples backward (real car-following),
// go green and it flows again. Honk by clicking a car; click the light to flip it.
//
//   click a car → HONK     click the light / L → flip it     SPACE → re-roll the fleet

// ── palettes ────────────────────────────────────────────────────────────────────
static const int CARCOL[] = {
    CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_TRUE_BLUE, CLR_INDIGO,
    CLR_PINK, CLR_WHITE, CLR_DARK_GREY, CLR_MAUVE, CLR_DARK_RED, CLR_MEDIUM_GREEN,
    CLR_DARK_PURPLE, CLR_LIGHT_GREY, CLR_DARK_ORANGE, CLR_DARK_BLUE, CLR_LIME_GREEN,
    CLR_DARK_GREEN, CLR_BLUE_GREEN,
};
static const int GLASS[] = { CLR_LIGHT_GREY, CLR_BLUE_GREEN, CLR_DARKER_BLUE, CLR_LIGHT_YELLOW, CLR_INDIGO };
static const int SKIN[]  = { CLR_LIGHT_PEACH, CLR_PEACH, CLR_DARK_PEACH, CLR_BROWN, CLR_MEDIUM_GREY };
static const int SHIRT[] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_PINK,
                             CLR_WHITE, CLR_DARK_PURPLE, CLR_LIME_GREEN, CLR_TRUE_BLUE };
#define PICK(a) ((a)[rnd(sizeof(a)/sizeof((a)[0]))])

// ── car types & body shapes ─────────────────────────────────────────────────────
enum { T_MICRO, T_HATCH, T_SEDAN, T_WAGON, T_SUV, T_MPV, T_COUPE, T_SPORTS, T_SUPER,
       T_MUSCLE, T_LUXURY, T_CONVERT, T_ANTIQUE, T_PICKUP, T_VAN, T_BUS, T_TRUCK, NTYPE };
enum { SH_CAR, SH_CONVERT, SH_PICKUP, SH_BOX, SH_TRUCK };
static const char *TNAME[NTYPE] = {
    "micro","hatchback","sedan","wagon","SUV","MPV","coupe","sports car","supercar",
    "muscle car","luxury","convertible","antique","pickup","van","bus","semi truck" };
// spawn weights — common cars turn up more than supercars and semis
static const int TWEIGHT[NTYPE] = { 6,9,10,5,7,5, 6,4,2, 3,3, 3,2, 5,3,2, 3 };

typedef struct {
    int   type, shape, lane, dir;
    float x;                  // world-x of the car's CENTRE along the track
    float scale;              // lane depth size
    // geometry, in design px (local frame: x 0..L from rear, y measured up from ground)
    float L, deck, bot, roof; // length, body-top, body-bottom (clearance), roof-top
    float cab[4];             // cabin x as fraction of L: rearBase, rearTop, frontTop, frontBase
    float wx[6]; int nWheel;  // wheel centres (fraction of L)
    float wr;                 // wheel radius
    float frontSlope;         // SH_BOX: hood length fraction; SH_TRUCK unused
    int   nWin, nPass;
    // flags
    int   antique, twoTone, spoiler, roofRack, antenna;
    // colours
    int   cBody, cBody2, cWin, cTire, cHub, cTrim;
    int   cHead[4], cTorso[4];
    // sim
    float v, vmax, pv;        // speed, target cap, previous speed
    int   braking;
    float honk, stoptime;     // honk flash; how long stuck (for spontaneous honks)
} Car;

#define MAXC 40
static Car cars[MAXC];
static int ncars = 0;

// ── road / lanes ─────────────────────────────────────────────────────────────────
#define MARGIN 60
static int   TRACK;                                  // wrap length (set in init)
#define NLANE 4
static const int   LANE_Y[NLANE] = { 124, 146, 172, 200 };
static const float LANE_S[NLANE] = { 0.60f, 0.74f, 0.90f, 1.10f };
static float XSTOP;                                  // world-x of the stop line

// ── traffic light ────────────────────────────────────────────────────────────────
enum { LIGHT_GREEN, LIGHT_YELLOW, LIGHT_RED };
static int   lstate = LIGHT_GREEN;
static float ltimer = 0;
static const float LDUR[3] = { 7.0f, 1.6f, 5.5f };   // green, yellow, red seconds

// ── audio ────────────────────────────────────────────────────────────────────────
#define I_ENG  5
#define I_HORN 6
#define I_DING 7
static int  engine = -1;
static bool soundOn = true;

// ── little fx pools ────────────────────────────────────────────────────────────────
typedef struct { float x, y, vx, vy; int life, col; } Puff;
#define NPUFF 60
static Puff puffs[NPUFF];
typedef struct { float x, y; int life; } Ring;     // honk ripple
#define NRING 24
static Ring rings[NRING];

// scene skyline (precomputed so it doesn't flicker)
#define NBLD 26
static int bldx[NBLD], bldw[NBLD], bldh[NBLD], bldc[NBLD];

// ── draw context (set per car so the helpers can map local→screen) ────────────────
static float gS; static int gX0, gYb; static float gLf;
static int SXf(float lx) { return gX0 + (int)(lx * gS + 0.5f); }
static int SY (float ly) { return gYb - (int)(ly * gS + 0.5f); }
static int SL (float v)  { return (int)(v * gS + 0.5f); }

static void boxL(float x0, float x1, float ybot, float ytop, int c) {
    int a = SXf(x0), b = SXf(x1); if (a > b) { int t = a; a = b; b = t; }
    int top = SY(ytop), bot = SY(ybot);
    rectfill(a, top, b - a + 1, bot - top + 1, c);
}
static void rboxL(float x0, float x1, float ybot, float ytop, int r, int c) {
    int a = SXf(x0), b = SXf(x1); if (a > b) { int t = a; a = b; b = t; }
    int top = SY(ytop), bot = SY(ybot);
    rrectfill(a, top, b - a + 1, bot - top + 1, max(1, SL(r)), c);
}
static void quadL(float ax, float ay, float bx, float by, float cx, float cy, float dx_, float dy_, int c) {
    int xy[8] = { SXf(ax), SY(ay), SXf(bx), SY(by), SXf(cx), SY(cy), SXf(dx_), SY(dy_) };
    polyfill(xy, 4, c);
}

// ── shared parts ───────────────────────────────────────────────────────────────────
static void draw_wheel(Car *c, float cx, float r) {
    int wx = SXf(cx * gLf), rr = max(2, SL(r)), wy = gYb - rr;
    circfill(wx, wy, rr, c->cTire);
    if (c->antique) {
        for (int a = 0; a < 8; a++)
            line(wx, wy, wx + (int)dx(rr - 1, a * 45), wy + (int)dy(rr - 1, a * 45), c->cHub);
        circfill(wx, wy, max(1, (int)(rr * 0.34f)), c->cHub);
    } else {
        circfill(wx, wy, max(1, (int)(rr * 0.52f)), c->cHub);
        circfill(wx, wy, max(1, (int)(rr * 0.22f)), CLR_DARK_GREY);
        pset(wx - (int)(rr * 0.3f), wy - (int)(rr * 0.3f), CLR_WHITE);
    }
}
// fit occupants into the cabin band [yseat .. yceil] (world-y, up = +): the head
// top sits flush under the ceiling and the torso fills down to the seat, so a
// passenger can never poke out through the roof. heads scale with the headroom.
// (pass a yceil above the body — e.g. for a convertible — to let heads rise.)
static void draw_people(Car *c, float xa, float xb, float yseat, float yceil) {
    int sceil = SY(yceil), sseat = SY(yseat);
    int hr = max(1, (int)(0.275f * (yceil - yseat) * gS + 0.5f));
    int headcy = sceil + hr;                              // head top flush with the ceiling
    for (int i = 0; i < c->nPass; i++) {
        float t  = (c->nPass == 1) ? 0.5f : (0.18f + 0.64f * i / (c->nPass - 1));
        int hx = SXf(lerp(xa, xb, t));
        int th = sseat - headcy; if (th < 2) th = 2;      // torso fills down to the seat
        rectfill(hx - hr, headcy, 2 * hr, th, c->cTorso[i]);  // shoulders/torso
        circfill(hx, headcy, hr, c->cHead[i]);                // head (sits on the torso)
    }
}
static void draw_lights(Car *c, float lo) {
    float my = (c->deck + c->bot) * 0.5f;
    int tl = c->braking ? CLR_RED : CLR_DARK_RED;
    // tail (rear / left), head (front / right)
    boxL(0.0f, lo, my - 1.2f, my + 1.2f, tl);
    if (c->braking) { int gx = SXf(0.0f), gy = SY(my); circfill(gx, gy, max(1, SL(1.6f)), CLR_RED); }
    boxL(c->L - lo, c->L, my - 1.0f, my + 1.6f, CLR_LIGHT_YELLOW);
}

// ── the standard car (micro…luxury, antique) ────────────────────────────────────────
static void draw_car(Car *c) {
    float L = c->L;
    boxL(2, L - 2, c->bot, c->deck, c->twoTone ? c->cBody2 : c->cBody);     // lower body
    rboxL(2, L - 2, c->bot, c->deck, 3, c->twoTone ? c->cBody2 : c->cBody);
    if (c->twoTone)                                                          // beltline stripe
        boxL(2, L - 2, c->deck - (c->deck - c->bot) * 0.42f, c->deck, c->cBody);
    if (c->antique) {                                                        // running board
        boxL(c->L * 0.18f, c->L * 0.82f, c->bot - 1.5f, c->bot + 0.5f, CLR_BROWNISH_BLACK);
        boxL(c->L * 0.62f, c->L * 0.95f, c->bot, c->deck * 0.7f, c->cBody2); // separate fender
    }
    // cabin (greenhouse)
    quadL(c->cab[0] * L, c->deck, c->cab[1] * L, c->roof, c->cab[2] * L, c->roof, c->cab[3] * L, c->deck, c->cBody);
    // glass, inset
    float gl = L * 0.03f, gv = 1.4f / (gS > 0.01f ? gS : 1), gb = 2.2f / (gS > 0.01f ? gS : 1);
    quadL(c->cab[0] * L + gl, c->deck + gb, c->cab[1] * L + gl * 0.5f, c->roof - gv,
          c->cab[2] * L - gl * 0.5f, c->roof - gv, c->cab[3] * L - gl, c->deck + gb, c->cWin);
    for (int k = 1; k < c->nWin; k++) {                                      // pillars
        float px = lerp(c->cab[1] * L, c->cab[2] * L, (float)k / c->nWin);
        boxL(px - 0.6f, px + 0.6f, c->deck + gb, c->roof - gv, c->cBody);
    }
    draw_people(c, c->cab[1] * L + L * 0.02f, c->cab[2] * L - L * 0.02f, c->deck, c->roof);
    if (c->spoiler) {
        boxL(2, c->L * 0.14f, c->deck + 1, c->deck + 3.5f, CLR_DARK_GREY);   // rear wing
        boxL(2, 4, c->deck, c->deck + 3.5f, CLR_DARK_GREY);
    }
    if (c->roofRack) boxL(c->cab[1] * L, c->cab[2] * L, c->roof, c->roof + 1.4f, CLR_DARK_GREY);
    draw_wheel(c, c->wx[0], c->wr);
    draw_wheel(c, c->wx[1], c->wr);
    draw_lights(c, 2.0f);
    if (c->antenna) line(SXf(c->cab[2] * L), SY(c->roof), SXf(c->cab[2] * L), SY(c->roof + 4), CLR_DARK_GREY);
}

// ── convertible — no roof, open cockpit ───────────────────────────────────────────
static void draw_convert(Car *c) {
    float L = c->L;
    rboxL(2, L - 2, c->bot, c->deck, 3, c->cBody);
    boxL(c->cab[0] * L, c->cab[3] * L, c->deck - 1, c->deck, CLR_BROWNISH_BLACK);  // cockpit cut
    boxL(c->cab[0] * L + 1, c->cab[3] * L - 1, c->deck - 0.5f, c->deck + 1.5f, c->cBody2); // seats
    draw_people(c, c->cab[0] * L + L * 0.08f, c->cab[3] * L - L * 0.08f, c->deck, c->deck + 4.0f);
    // raked windscreen frame at the front of the cockpit
    float wf = c->cab[3] * L;
    int x0 = SXf(wf - 1.5f), y0 = SY(c->deck), x1 = SXf(wf - 4), y1 = SY(c->deck + 4.5f);
    thickline(x0, y0, x1, y1, max(1, SL(1.0f)), CLR_LIGHT_GREY);
    draw_wheel(c, c->wx[0], c->wr);
    draw_wheel(c, c->wx[1], c->wr);
    draw_lights(c, 2.0f);
}

// ── pickup — front cab + open bed ──────────────────────────────────────────────────
static void draw_pickup(Car *c) {
    float L = c->L;
    rboxL(2, L - 2, c->bot, c->deck, 2, c->cBody);
    // bed in the rear: a lowered open box with side wall
    float bedEnd = c->cab[0] * L;
    boxL(2, bedEnd, c->deck, c->deck + 3.0f, c->cBody);            // bed side wall
    boxL(4, bedEnd - 1, c->deck - 0.5f, c->deck + 2.0f, CLR_BROWNISH_BLACK); // bed interior
    if (chance(45)) {                                              // random cargo
        int n = rnd_between(1, 4);
        for (int i = 0; i < n; i++) {
            float bx = lerp(6, bedEnd - 3, (n == 1) ? 0.5f : (float)i / (n - 1));
            boxL(bx - 2, bx + 2, c->deck, c->deck + 2.5f, PICK(CARCOL));
        }
    }
    quadL(c->cab[0] * L, c->deck, c->cab[1] * L, c->roof, c->cab[2] * L, c->roof, c->cab[3] * L, c->deck, c->cBody);
    quadL(c->cab[0] * L + 1.5f, c->deck + 2, c->cab[1] * L + 1, c->roof - 1.5f,
          c->cab[2] * L - 1, c->roof - 1.5f, c->cab[3] * L - 1.5f, c->deck + 2, c->cWin);
    draw_people(c, c->cab[1] * L + 2, c->cab[2] * L - 2, c->deck, c->roof);
    draw_wheel(c, c->wx[0], c->wr);
    draw_wheel(c, c->wx[1], c->wr);
    draw_lights(c, 2.0f);
}

// ── van / bus — tall box with a window row ─────────────────────────────────────────
static void draw_box(Car *c) {
    float L = c->L, fs = c->frontSlope;
    float hoodY = c->bot + (c->roof - c->bot) * 0.46f;
    if (fs > 0.01f) {  // van: sloped hood up front
        int xy[12] = { SXf(2), SY(c->bot), SXf(2), SY(c->roof),
                       SXf(L * (1 - fs - 0.14f)), SY(c->roof), SXf(L - 2), SY(hoodY),
                       SXf(L - 2), SY(c->bot), SXf(2), SY(c->bot) };
        polyfill(xy, 6, c->cBody);
        // windscreen on the slope
        quadL(L * (1 - fs - 0.14f) + 1, c->roof - 1.5f, L * (1 - fs - 0.14f) + 1, hoodY + 2,
              L - 3, hoodY + 1, L - 3, hoodY + 2, c->cWin);
    } else {           // bus: flat front
        rboxL(2, L - 2, c->bot, c->roof, 3, c->cBody);
    }
    // window row
    float wy0 = c->roof - 1.5f, wy1 = c->roof - (c->roof - c->bot) * 0.42f;
    int n = c->nWin; float span0 = L * 0.10f, span1 = L * (fs > 0.01f ? 0.74f : 0.92f);
    for (int k = 0; k < n; k++) {
        float a = lerp(span0, span1, (float)k / n) + 0.8f;
        float b = lerp(span0, span1, (float)(k + 1) / n) - 0.8f;
        boxL(a, b, wy1, wy0, c->cWin);
    }
    boxL(L * 0.04f, L * 0.09f, c->bot + 1, c->roof - 1, c->cBody2);  // door
    draw_people(c, span0 + 2, span1 - 2, wy1, wy0);
    for (int i = 0; i < c->nWheel; i++) draw_wheel(c, c->wx[i], c->wr);
    draw_lights(c, 2.0f);
}

// ── the long-ass semi truck — cab + container trailer ──────────────────────────────
static void draw_truck(Car *c) {
    float L = c->L;
    float cabX = L * 0.74f, trEnd = L * 0.68f, trRoof = c->roof, cabRoof = c->roof * 0.86f;
    // trailer / container
    rboxL(2, trEnd, c->deck, trRoof, 2, c->cBody2);
    boxL(2, trEnd, c->deck + (trRoof - c->deck) * 0.45f, c->deck + (trRoof - c->deck) * 0.6f, c->cTrim); // stripe
    boxL(2, c->bot, c->deck - 0.5f, c->deck, CLR_DARK_GREY);                // rear bumper bar
    // tractor cab
    rboxL(cabX, L - 2, c->bot, cabRoof, 2, c->cBody);
    quadL(cabX + 1, c->bot + (cabRoof - c->bot) * 0.4f, cabX + 1, cabRoof - 1.5f,
          L - 3, cabRoof - 1.5f, L - 3, c->bot + (cabRoof - c->bot) * 0.4f, c->cWin); // windscreen+window
    {   // driver
        int hx = SXf(L - 4), hy = SY(c->bot + (cabRoof - c->bot) * 0.62f), hr = max(1, SL(c->wr * 0.4f));
        rectfill(hx - hr, hy, 2 * hr, hr * 2, c->cTorso[0]);
        circfill(hx, hy - hr, hr, c->cHead[0]);
    }
    boxL(cabX - 2, cabX + 1, c->bot, c->bot + (cabRoof - c->bot) * 0.7f, CLR_DARK_GREY); // exhaust stack
    boxL(cabX - 2, cabX + 1, cabRoof * 0.7f, cabRoof + 2.0f, CLR_DARK_GREY);
    for (int i = 0; i < c->nWheel; i++) draw_wheel(c, c->wx[i], c->wr);
    draw_lights(c, 2.0f);
}

static void render_car(Car *c) {
    gS = c->scale; gLf = c->L; gYb = LANE_Y[c->lane];
    gX0 = (int)((c->x - MARGIN) - c->L * 0.5f * c->scale + 0.5f);
    // ground shadow
    int sx = SXf(c->L * 0.5f), rr = (int)(c->L * 0.5f * c->scale);
    ovalfill(sx, gYb + 1, rr, max(1, SL(c->wr * 0.35f)), CLR_DARKER_GREY);
    switch (c->shape) {
        case SH_CONVERT: draw_convert(c); break;
        case SH_PICKUP:  draw_pickup(c);  break;
        case SH_BOX:     draw_box(c);     break;
        case SH_TRUCK:   draw_truck(c);   break;
        default:         draw_car(c);     break;
    }
    if (c->honk > 0) {   // a little "beep" tag over the car
        int tx = SXf(c->L * 0.5f), ty = SY(c->roof + 5);
        if (blink(4)) print_outline("beep!", tx - 9, ty, CLR_YELLOW, CLR_DARK_RED);
    }
}

// ── rolling a car ────────────────────────────────────────────────────────────────────
static float jf(float c, float spread) { return c * rnd_float_between(1 - spread, 1 + spread); }

static int weighted_type(void) {
    int total = 0; for (int i = 0; i < NTYPE; i++) total += TWEIGHT[i];
    int r = rnd(total);
    for (int i = 0; i < NTYPE; i++) { r -= TWEIGHT[i]; if (r < 0) return i; }
    return T_SEDAN;
}

static void roll_car(Car *c, int type, int lane) {
    *c = (Car){0};
    c->type = type; c->lane = lane; c->dir = 1; c->scale = LANE_S[lane];
    c->shape = SH_CAR;
    c->cBody = PICK(CARCOL); do { c->cBody2 = PICK(CARCOL); } while (c->cBody2 == c->cBody);
    c->cWin = PICK(GLASS);
    c->cTire = chance(80) ? CLR_BLACK : CLR_BROWNISH_BLACK;
    c->cHub = CLR_LIGHT_GREY; c->cTrim = CLR_LIGHT_GREY;
    c->twoTone = chance(28);
    c->antenna = chance(35);
    c->nWheel = 2;
    c->vmax = rnd_float_between(34, 46);

    // base geometry per type, then jitter the ten dimensions
    switch (type) {
        case T_MICRO:  c->L=40; c->deck=16; c->bot=6; c->roof=30; c->wr=6;
            c->cab[0]=.26f;c->cab[1]=.30f;c->cab[2]=.58f;c->cab[3]=.76f; c->nWin=1; c->nPass=1; break;
        case T_HATCH:  c->L=56; c->deck=15; c->bot=6; c->roof=27; c->wr=7;
            c->cab[0]=.30f;c->cab[1]=.37f;c->cab[2]=.66f;c->cab[3]=.82f; c->nWin=2; c->nPass=2; break;
        case T_SEDAN:  c->L=72; c->deck=15; c->bot=6; c->roof=25; c->wr=7;
            c->cab[0]=.30f;c->cab[1]=.41f;c->cab[2]=.64f;c->cab[3]=.80f; c->nWin=2; c->nPass=2; break;
        case T_WAGON:  c->L=76; c->deck=15; c->bot=6; c->roof=26; c->wr=7; c->roofRack=chance(50);
            c->cab[0]=.27f;c->cab[1]=.33f;c->cab[2]=.74f;c->cab[3]=.84f; c->nWin=3; c->nPass=3; break;
        case T_SUV:    c->L=72; c->deck=17; c->bot=9; c->roof=33; c->wr=9; c->roofRack=chance(60);
            c->cab[0]=.26f;c->cab[1]=.33f;c->cab[2]=.70f;c->cab[3]=.83f; c->nWin=3; c->nPass=3; break;
        case T_MPV:    c->L=76; c->deck=16; c->bot=7; c->roof=34; c->wr=7;
            c->cab[0]=.22f;c->cab[1]=.28f;c->cab[2]=.78f;c->cab[3]=.92f; c->nWin=3; c->nPass=4; break;
        case T_COUPE:  c->L=70; c->deck=14; c->bot=5; c->roof=23; c->wr=7;
            c->cab[0]=.36f;c->cab[1]=.46f;c->cab[2]=.62f;c->cab[3]=.82f; c->nWin=1; c->nPass=2; break;
        case T_SPORTS: c->L=72; c->deck=12; c->bot=4; c->roof=20; c->wr=7; c->spoiler=chance(40);
            c->cab[0]=.38f;c->cab[1]=.48f;c->cab[2]=.60f;c->cab[3]=.84f; c->nWin=1; c->nPass=1;
            c->vmax=rnd_float_between(44,56); break;
        case T_SUPER:  c->L=76; c->deck=11; c->bot=4; c->roof=18; c->wr=8; c->spoiler=chance(75);
            c->cab[0]=.44f;c->cab[1]=.52f;c->cab[2]=.62f;c->cab[3]=.88f; c->nWin=1; c->nPass=1;
            c->vmax=rnd_float_between(48,60); break;
        case T_MUSCLE: c->L=76; c->deck=14; c->bot=5; c->roof=22; c->wr=8; c->spoiler=chance(35);
            c->cab[0]=.36f;c->cab[1]=.45f;c->cab[2]=.62f;c->cab[3]=.82f; c->nWin=1; c->nPass=2;
            c->vmax=rnd_float_between(42,54); break;
        case T_LUXURY: c->L=82; c->deck=15; c->bot=6; c->roof=26; c->wr=7; c->twoTone=chance(55);
            c->cab[0]=.32f;c->cab[1]=.43f;c->cab[2]=.65f;c->cab[3]=.82f; c->nWin=2; c->nPass=3;
            c->cTrim=CLR_YELLOW; break;
        case T_CONVERT:c->shape=SH_CONVERT; c->L=70; c->deck=15; c->bot=6; c->roof=15; c->wr=7;
            c->cab[0]=.38f;c->cab[1]=.42f;c->cab[2]=.58f;c->cab[3]=.66f; c->nWin=0; c->nPass=2; break;
        case T_ANTIQUE:c->antique=1; c->L=58; c->deck=18; c->bot=10; c->roof=35; c->wr=11;
            c->cab[0]=.40f;c->cab[1]=.46f;c->cab[2]=.58f;c->cab[3]=.70f; c->nWin=1; c->nPass=2;
            c->cHub=CLR_DARK_BROWN; c->cTire=CLR_BROWNISH_BLACK; c->vmax=rnd_float_between(28,36);
            c->twoTone=1; c->antenna=0; break;
        case T_PICKUP: c->shape=SH_PICKUP; c->L=80; c->deck=16; c->bot=7; c->roof=29; c->wr=8;
            c->cab[0]=.46f;c->cab[1]=.54f;c->cab[2]=.70f;c->cab[3]=.86f; c->nWin=1; c->nPass=2; break;
        case T_VAN:    c->shape=SH_BOX; c->L=76; c->deck=16; c->bot=7; c->roof=35; c->wr=7;
            c->frontSlope=0.16f; c->nWin=2; c->nPass=2;
            c->wx[0]=.20f; c->wx[1]=.84f; c->nWheel=2; break;
        case T_BUS:    c->shape=SH_BOX; c->L=108; c->deck=18; c->bot=8; c->roof=42; c->wr=8;
            c->frontSlope=0.0f; c->nWin=6; c->nPass=4;
            c->wx[0]=.14f; c->wx[1]=.84f; c->nWheel=2; c->vmax=rnd_float_between(30,40); break;
        case T_TRUCK:  c->shape=SH_TRUCK; c->L=132; c->deck=12; c->bot=5; c->roof=40; c->wr=8;
            c->cBody2 = chance(55) ? CLR_WHITE : CLR_LIGHT_GREY; c->cTrim = PICK(CARCOL);
            c->nWheel=5; c->wx[0]=.06f; c->wx[1]=.16f; c->wx[2]=.60f; c->wx[3]=.84f; c->wx[4]=.93f;
            c->nPass=1; c->vmax=rnd_float_between(28,38); break;
    }

    // ── the ten dimensions wobble within type ───────────────────────────────
    c->L    = jf(c->L,    0.10f);   // 1 length
    c->deck = jf(c->deck, 0.08f);   // 2 body height
    c->roof = jf(c->roof, 0.07f);   // 3 roof height (clamped above deck below)
    c->bot  = jf(c->bot,  0.18f);   // 9 ground clearance
    c->wr   = jf(c->wr,   0.10f);   //10 wheel radius
    for (int i = 0; i < 4; i++) c->cab[i] += rnd_float_between(-0.03f, 0.03f); // 4/5/6/7 cabin len, pos, rakes
    if (c->roof < c->deck + 5) c->roof = c->deck + 5;
    // keep cabin fractions ordered
    for (int i = 1; i < 4; i++) if (c->cab[i] < c->cab[i-1] + 0.02f) c->cab[i] = c->cab[i-1] + 0.02f;

    // standard cars: roll a wheelbase (8 wheel radius already, this is dimension #8/#10 wheel positions)
    if (c->shape == SH_CAR || c->shape == SH_CONVERT || c->shape == SH_PICKUP) {
        float frontW = (c->shape == SH_PICKUP) ? rnd_float_between(0.80f, 0.88f) : rnd_float_between(0.76f, 0.86f);
        float rearW  = rnd_float_between(0.14f, 0.24f);
        c->wx[0] = rearW; c->wx[1] = frontW; c->nWheel = 2;
    }

    // passengers
    for (int i = 0; i < 4; i++) { c->cHead[i] = PICK(SKIN); c->cTorso[i] = PICK(SHIRT); }
    if (c->nPass < 1) c->nPass = 1; if (c->nPass > 4) c->nPass = 4;
    c->v = c->vmax * rnd_float_between(0.4f, 1.0f); c->pv = c->v;
}

// ── fx ───────────────────────────────────────────────────────────────────────────────
static void spawn_puff(float x, float y, float sc) {
    for (int n = 0; n < 3; n++)
        for (int i = 0; i < NPUFF; i++)
            if (puffs[i].life <= 0) {
                puffs[i] = (Puff){ x + rnd_between(-2, 3), y, rnd_float_between(-0.6f, 0.6f),
                    rnd_float_between(-0.7f, -0.2f) * sc, rnd_between(12, 24),
                    chance(50) ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY };
                break;
            }
}
static void spawn_ring(float x, float y) {
    for (int i = 0; i < NRING; i++) if (rings[i].life <= 0) { rings[i] = (Ring){ x, y, 18 }; return; }
}

static void honk(Car *c) {
    c->honk = 0.5f;
    spawn_ring(c->x - MARGIN, LANE_Y[c->lane] - c->roof * c->scale - 4);
    if (!soundOn) return;
    int base = (c->shape == SH_TRUCK || c->type == T_BUS) ? 40 : 56 - (int)((c->L - 60) * 0.2f);
    int dur  = (c->shape == SH_TRUCK) ? 420 : 240;
    hit(base, I_HORN, 4, dur);
    hit(base + (chance(50) ? 4 : 5), I_HORN, 3, dur);
}

// ── traffic light ────────────────────────────────────────────────────────────────────
static void draw_light(void) {
    int sx = (int)(XSTOP - MARGIN) + 8, sy = 70;        // signal head, roadside
    int lx = (int)(XSTOP - MARGIN);                     // stop line across the road
    fillp(0xA5A5, -1);
    rectfill(lx - 2, 100, 4, SCREEN_H - 100, CLR_WHITE);
    fillp_reset();
    // post + head
    rectfill(sx + 4, sy + 24, 3, 40, CLR_DARK_GREY);
    rrectfill(sx, sy, 12, 30, 3, CLR_BROWNISH_BLACK);
    int on[3] = { lstate == LIGHT_RED, lstate == LIGHT_YELLOW, lstate == LIGHT_GREEN };
    int col[3] = { CLR_RED, CLR_YELLOW, CLR_GREEN };
    for (int i = 0; i < 3; i++)
        circfill(sx + 6, sy + 6 + i * 9, 3, on[i] ? col[i] : CLR_DARKER_GREY);
}

// ── scene ──────────────────────────────────────────────────────────────────────────
void init(void) {
    colorkey(-1);
    TRACK = SCREEN_W + 2 * MARGIN;
    XSTOP = MARGIN + SCREEN_W * 0.66f;
    // skyline
    int x = -4;
    for (int i = 0; i < NBLD; i++) {
        bldx[i] = x; bldw[i] = rnd_between(14, 30); bldh[i] = rnd_between(16, 48);
        bldc[i] = (int[]){ CLR_DARK_BLUE, CLR_DARKER_BLUE, CLR_INDIGO, CLR_DARKER_PURPLE }[rnd(4)];
        x += bldw[i] + rnd_between(-2, 4);
        if (x > SCREEN_W) x = -4;
    }
    // fill the lanes with an initial procession
    ncars = 0;
    for (int l = 0; l < NLANE; l++) {
        float px = rnd_between(0, 60);
        while (px < TRACK && ncars < MAXC) {
            Car *c = &cars[ncars];
            roll_car(c, weighted_type(), l);
            c->x = px + c->L * 0.5f * c->scale;
            px += c->L * c->scale + rnd_between(14, 40);
            ncars++;
        }
    }
    // audio: a low engine drone we ride with traffic density
    instrument(I_ENG, INSTR_SAW, 200, 400, 5, 600);
    instrument_filter(I_ENG, FILTER_LOW, 360, 3);
    instrument(I_HORN, INSTR_SQUARE, 4, 200, 5, 120); instrument_duty(I_HORN, 0.5f);
    instrument(I_DING, INSTR_TRI, 2, 120, 2, 160);
    if (soundOn) engine = note_on(28, I_ENG, 2);
}

static int hover_car(int mx, int my) {
    int best = -1;
    for (int l = 0; l < NLANE; l++)
        for (int i = 0; i < ncars; i++) {
            if (cars[i].lane != l) continue;
            Car *c = &cars[i];
            int half = (int)(c->L * 0.5f * c->scale);
            int cx = (int)(c->x - MARGIN);
            if (mx >= cx - half && mx <= cx + half &&
                my <= LANE_Y[l] + 2 && my >= LANE_Y[l] - (int)(c->roof * c->scale) - 6)
                best = i;
        }
    return best;
}

// ── update ──────────────────────────────────────────────────────────────────────────
void update(void) {
    float t = dt();
    if (keyp(KEY_SPACE)) { init(); return; }
    if (keyp('M')) { soundOn = !soundOn; if (!soundOn && engine >= 0) { note_off(engine); engine = -1; }
                     else if (soundOn && engine < 0) engine = note_on(28, I_ENG, 2); }

    int mx = mouse_x(), my = mouse_y();
    // traffic light: auto-cycle, or click the head / press L to flip
    ltimer += t;
    if (ltimer > LDUR[lstate]) {
        ltimer = 0; lstate = (lstate + 1) % 3;
        if (soundOn) note(lstate == LIGHT_GREEN ? 76 : 64, I_DING, 3);
    }
    int lhx = (int)(XSTOP - MARGIN) + 8;
    bool clickLight = mouse_pressed(MOUSE_LEFT) && mx >= lhx - 2 && mx <= lhx + 14 && my >= 66 && my <= 104;
    if (keyp('L') || clickLight) {
        lstate = (lstate == LIGHT_RED) ? LIGHT_GREEN : LIGHT_RED; ltimer = 0;
        if (soundOn) note(lstate == LIGHT_GREEN ? 76 : 64, I_DING, 3);
    }
    if (mouse_pressed(MOUSE_LEFT) && !clickLight) {
        int h = hover_car(mx, my); if (h >= 0) honk(&cars[h]);
    }

    // ── car-following per lane ───────────────────────────────────────────────
    bool red = (lstate != LIGHT_GREEN);
    for (int l = 0; l < NLANE; l++) {
        int idx[MAXC], n = 0;
        for (int i = 0; i < ncars; i++) if (cars[i].lane == l) idx[n++] = i;
        // insertion sort by x
        for (int a = 1; a < n; a++) { int k = idx[a], b = a - 1;
            while (b >= 0 && cars[idx[b]].x > cars[k].x) { idx[b+1] = idx[b]; b--; } idx[b+1] = k; }
        for (int a = 0; a < n; a++) {
            Car *c = &cars[idx[a]];
            float half = c->L * 0.5f * c->scale;
            float front = c->x + half;
            // gap to the car ahead (next larger x, wrapping)
            Car *ah = &cars[idx[(a + 1) % n]];
            float aheadRear = ah->x - ah->L * 0.5f * ah->scale;
            if (a + 1 >= n) aheadRear += TRACK;          // wrap
            float gap = aheadRear - front;
            float desired = clamp((gap - 6.0f) * 4.0f, 0, c->vmax);
            // obey the light if the stop line is ahead of us
            if (red) {
                float d = XSTOP - front;
                if (d > -3 && d < 90) desired = min(desired, clamp(d * 1.4f, 0, c->vmax));
            }
            c->pv = c->v;
            if (desired > c->v) c->v = min(desired, c->v + 70 * t);
            else                c->v = max(desired, c->v - 150 * t);
            if (c->v < 0) c->v = 0;
            c->braking = (c->v < c->pv - 0.05f) || (desired < c->v - 0.5f && c->v < c->vmax * 0.9f);
            // exhaust when pulling away from near-stop
            if (c->pv < 3 && c->v > 4 && chance(40))
                spawn_puff(c->x - MARGIN - half + 1, LANE_Y[l] - c->bot * c->scale, c->scale);
            c->x += c->v * t;
            if (c->x - half > TRACK) {                    // wrapped fully past the end → re-roll
                roll_car(c, weighted_type(), l);
                c->x -= TRACK;
            }
            // spontaneous honks when stuck a while
            if (c->v < 2) c->stoptime += t; else c->stoptime = 0;
            if (c->stoptime > 2.5f && chance(2)) { honk(c); c->stoptime = 0; }
            if (c->honk > 0) c->honk -= t;
        }
    }

    // engine drone tracks how much traffic is moving
    if (soundOn && engine >= 0) {
        float mv = 0; for (int i = 0; i < ncars; i++) mv += cars[i].v;
        mv = clamp(mv / (ncars * 35.0f + 1), 0, 1);
        note_vol(engine, 1 + (int)(mv * 3));
        note_cutoff(engine, 300 + (int)(mv * 900));
    }

    for (int i = 0; i < NPUFF; i++) if (puffs[i].life > 0) {
        puffs[i].x += puffs[i].vx; puffs[i].y += puffs[i].vy; puffs[i].vy *= 0.96f; puffs[i].life--; }
    for (int i = 0; i < NRING; i++) if (rings[i].life > 0) rings[i].life--;
}

// ── render ──────────────────────────────────────────────────────────────────────────
void draw(void) {
    vgradient(0, 0, SCREEN_W, 72, CLR_BLUE, CLR_LIGHT_PEACH);     // sky
    for (int i = 0; i < NBLD; i++)                                // skyline
        rectfill(bldx[i], 72 - bldh[i], bldw[i], bldh[i], bldc[i]);
    rectfill(0, 70, SCREEN_W, SCREEN_H - 70, CLR_DARKER_GREY);    // ground/asphalt base
    rectfill(0, LANE_Y[0] - 14, SCREEN_W, SCREEN_H, CLR_DARK_GREY);
    // lane dividers (dashed)
    for (int l = 0; l < NLANE - 1; l++) {
        int y = (LANE_Y[l] + LANE_Y[l+1]) / 2 - 6;
        for (int x = (frame() / 2) % 16 - 16; x < SCREEN_W; x += 16)
            rectfill(x, y, 7, 1, CLR_MEDIUM_GREY);
    }
    rectfill(0, SCREEN_H - 6, SCREEN_W, 6, CLR_DARKER_GREY);      // near kerb

    draw_light();

    // far → near so nearer lanes overlap correctly; within a lane, painter-sort
    // by x (rear → front) so packed cars never paint a body over the previous
    // car's wheels or passengers — same sort the car-following loop uses
    for (int l = 0; l < NLANE; l++) {
        int idx[MAXC], n = 0;
        for (int i = 0; i < ncars; i++) if (cars[i].lane == l) idx[n++] = i;
        for (int a = 1; a < n; a++) { int k = idx[a], b = a - 1;
            while (b >= 0 && cars[idx[b]].x > cars[k].x) { idx[b+1] = idx[b]; b--; } idx[b+1] = k; }
        for (int a = 0; a < n; a++) render_car(&cars[idx[a]]);
    }

    for (int i = 0; i < NPUFF; i++) if (puffs[i].life > 0)
        circfill((int)puffs[i].x, (int)puffs[i].y, puffs[i].life > 14 ? 2 : 1, puffs[i].col);
    for (int i = 0; i < NRING; i++) if (rings[i].life > 0) {
        int r = 18 - rings[i].life; arc((int)rings[i].x, (int)rings[i].y, r + 3, 200, 340, CLR_YELLOW); }

    // hover label — name the car under the pointer
    int h = hover_car(mouse_x(), mouse_y());
    if (h >= 0) {
        Car *c = &cars[h];
        int cx = (int)(c->x - MARGIN), ty = LANE_Y[c->lane] - (int)(c->roof * c->scale) - 8;
        font(FONT_SMALL);
        print_outline(TNAME[c->type], cx - text_width(TNAME[c->type]) / 2, ty, CLR_WHITE, CLR_BLACK);
        font(FONT_NORMAL);
    }

    // title + hud
    print_scaled("TRAFFIC JAM", 5, 5, CLR_BROWNISH_BLACK, 2);
    print_scaled("TRAFFIC JAM", 4, 4, CLR_YELLOW, 2);
    font(FONT_SMALL);
    const char *ls = lstate == LIGHT_RED ? "RED" : lstate == LIGHT_YELLOW ? "WAIT" : "GO";
    print_outline(str("%d cars   light: %s", ncars, ls),
                  SCREEN_W - 4 - text_width(str("%d cars   light: %s", ncars, ls)), 6, CLR_WHITE, CLR_DARK_BLUE);
    print_outline("click a car = honk    click light / L = flip    SPACE = re-roll",
                  4, SCREEN_H - 8, CLR_LIGHT_PEACH, CLR_DARKER_PURPLE);
    font(FONT_NORMAL);
}
