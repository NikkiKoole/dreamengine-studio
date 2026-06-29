/* de:meta
{
  "title": "steer (car drift)",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "spring-damper",
    "particle-system"
  ],
  "lineage": "Direct sequel to thrust.c — extends the facing/velocity model with partial coupling (GRIP), speed-scaled steering, and skid-mark particles to teach the car drift idiom.",
  "genre": "racing",
  "description": "The car model — the second half of the steering paradigm. thrust showed facing and velocity fully decoupled (a spaceship); a car is the in-between: velocity is PARTIALLY coupled to the facing, and the coupling strength is GRIP. Three rules: turning scales with speed (a parked car cannot turn — try it), the gas pushes along the heading, and each frame the velocity lerps toward the heading — fast lerp carves, slow lerp DRIFTS. Skid marks appear exactly when the lateral slide exceeds the tires. The green line is where you are going, the nose is where you point. X swaps racing tires for drift tires. Collect the cones. LEFT/RIGHT steer, UP gas, DOWN brake, X tires, Z restart."
}
de:meta */
#include "studio.h"

// STEER — the car model, the second half of the steering paradigm. thrust.c
// showed facing and velocity fully DECOUPLED (a spaceship). A car is the
// interesting in-between: velocity is PARTIALLY coupled to the facing, and the
// coupling strength is called GRIP. Three rules make a car a car:
//
//  1. SPEED-SCALED STEERING — turning is `STEER * (speed / MAX_SPD)`. A parked
//     car cannot turn (try it!), a slow car turns tight, a fast car carves.
//     This single line is why it feels like driving and not like a tank.
//  2. ACCELERATION LIVES ON THE HEADING — the gas pedal pushes where the nose
//     points; a scalar `spd` with friction handles engine and rolling drag.
//  3. GRIP — each frame the velocity vector LERPS toward "spd along the
//     heading". Grip high: it snaps — the car goes where it points (racing
//     line). Grip low: velocity lags behind the nose — the car POINTS one way
//     and SLIDES another. That lag IS drifting; the skid marks appear exactly
//     when the lateral slide exceeds the tires' grip.
//
// The green line is your velocity, the nose is your heading — same teaching
// device as thrust.c. Press X to swap racing tires for drift tires.
//
// Collect the cones.  LEFT/RIGHT steer, UP gas, DOWN brake/reverse.
// X: grip / drift.   Z: restart.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING — edit + hot-reload ───────────────────────────────────────────────
#define ACCEL      0.06f    // gas, px/frame² along the heading
#define BRAKE      0.12f
#define REV_MAX   -1.0f     // reverse speed cap
#define FRICTION   0.985f   // engine speed kept per frame
#define MAX_SPD    3.0f
#define STEER      3.4f     // degrees/frame at FULL speed (scales down with speed)
#define GRIP_RACE  0.25f    // velocity→heading lerp: high = carves where you point
#define GRIP_DRIFT 0.07f    // low = the back end comes loose
#define SLIDE_MIN  0.55f    // lateral speed where the tires start marking

#define NCONE 8
static const int cone_pos[NCONE][2] = {
    {  50,  45 }, { 270,  45 }, { 160,  70 }, {  60, 155 },
    { 260, 155 }, { 160, 175 }, {  40, 100 }, { 280, 100 },
};

#define MAXSKID 192
typedef struct { int x, y; int life; } Skid;

STATE {
    float px, py;        // position
    float vx, vy;        // velocity — partially coupled to the heading via GRIP
    float spd;           // engine speed (scalar, along the heading)
    float ang;           // heading, degrees
    bool  drift;         // tire choice
    bool  got[NCONE];
    int   ngot;
    bool  win;
    Skid  skid[MAXSKID];
    int   skid_head;
};

static void reset_game(void) {
    S->px = SCREEN_W / 2.0f; S->py = 130;
    S->vx = 0; S->vy = 0; S->spd = 0; S->ang = -90;
    S->drift = false;
    for (int i = 0; i < NCONE; i++) S->got[i] = false;
    S->ngot = 0; S->win = false;
    for (int i = 0; i < MAXSKID; i++) S->skid[i].life = 0;
}

void init(void) { reset_game(); }

static void lay_skid(float x, float y) {
    S->skid[S->skid_head % MAXSKID] = (Skid){ (int)x, (int)y, 160 };
    S->skid_head++;
}

void update(void) {
    // lateral slide = how much of the velocity is sideways to the heading
    float lat = S->vx * dx(1, S->ang + 90) + S->vy * dy(1, S->ang + 90);
#ifdef DE_TRACE
    watch("c", "lat=%.2f a=%.0f s=%.2f dr=%d x=%.0f y=%.0f",
          lat, S->ang, S->spd, S->drift, S->px, S->py);
#endif
    if (S->win) { if (btnp(0, BTN_A)) reset_game(); return; }

    if (btnp(0, BTN_B)) S->drift = !S->drift;

    // 1. steering scales with speed — a parked car can't turn
    float turn_in = (btn(0, BTN_RIGHT) ? 1.0f : 0.0f) - (btn(0, BTN_LEFT) ? 1.0f : 0.0f);
    float sc = S->spd / MAX_SPD; if (sc < 0) sc = -sc;
    S->ang += turn_in * STEER * sc;

    // 2. gas and brake act on the engine-speed scalar
    if (btn(0, BTN_UP))   S->spd += ACCEL;
    if (btn(0, BTN_DOWN)) S->spd -= BRAKE;
    S->spd *= FRICTION;
    S->spd = clamp(S->spd, REV_MAX, MAX_SPD);

    // 3. grip: the velocity chases "spd along the heading" — how fast it
    //    catches up is the whole difference between carving and drifting
    float g = S->drift ? GRIP_DRIFT : GRIP_RACE;
    S->vx = lerp(S->vx, dx(S->spd, S->ang), g);
    S->vy = lerp(S->vy, dy(S->spd, S->ang), g);
    S->px += S->vx; S->py += S->vy;

    // soft screen edges: slide along them, kill the inward velocity
    if (S->px < 10)            { S->px = 10;            if (S->vx < 0) S->vx = 0; }
    if (S->px > SCREEN_W - 10) { S->px = SCREEN_W - 10; if (S->vx > 0) S->vx = 0; }
    if (S->py < 22)            { S->py = 22;            if (S->vy < 0) S->vy = 0; }
    if (S->py > SCREEN_H - 10) { S->py = SCREEN_H - 10; if (S->vy > 0) S->vy = 0; }

    // skid marks from the rear wheels, exactly while the tires are sliding
    if ((lat > SLIDE_MIN || lat < -SLIDE_MIN) && (S->spd > 1.0f || S->spd < -1.0f)) {
        float rx_ = S->px + dx(-5, S->ang), ry_ = S->py + dy(-5, S->ang);
        lay_skid(rx_ + dx(3, S->ang + 90), ry_ + dy(3, S->ang + 90));
        lay_skid(rx_ + dx(3, S->ang - 90), ry_ + dy(3, S->ang - 90));
    }
    for (int i = 0; i < MAXSKID; i++)
        if (S->skid[i].life > 0) S->skid[i].life--;

    // cones
    for (int i = 0; i < NCONE; i++) {
        if (S->got[i]) continue;
        if (near((int)S->px, (int)S->py, cone_pos[i][0], cone_pos[i][1], 11)) {
            S->got[i] = true; S->ngot++;
            note(76 + S->ngot * 2, INSTR_TRI, 3);
            if (S->ngot == NCONE) { S->win = true; strum(72, CHORD_MAJ, INSTR_TRI, 4, 40); }
        }
    }
}

void draw(void) {
    cls(CLR_DARK_GREY);                                 // asphalt

    for (int i = 0; i < MAXSKID; i++)                   // skid marks fade out
        if (S->skid[i].life > 0)
            pset(S->skid[i].x, S->skid[i].y,
                 S->skid[i].life > 60 ? CLR_BROWNISH_BLACK : CLR_DARKER_GREY);

    for (int i = 0; i < NCONE; i++) {
        if (S->got[i]) continue;
        int x = cone_pos[i][0], y = cone_pos[i][1];
        trifill(x, y - 5, x + 4, y + 3, x - 4, y + 3, CLR_ORANGE);
        pset(x, y - 1, CLR_LIGHT_YELLOW);
    }

    // the car: body quad + cab, built from the heading angle
    float fx_ = dx(6, S->ang),       fy_ = dy(6, S->ang);
    float sx_ = dx(3.5f, S->ang + 90), sy_ = dy(3.5f, S->ang + 90);
    quadfill((int)(S->px + fx_ + sx_), (int)(S->py + fy_ + sy_),
             (int)(S->px + fx_ - sx_), (int)(S->py + fy_ - sy_),
             (int)(S->px - fx_ - sx_), (int)(S->py - fy_ - sy_),
             (int)(S->px - fx_ + sx_), (int)(S->py - fy_ + sy_), CLR_RED);
    float cx_ = dx(1, S->ang), cy_ = dy(1, S->ang);
    float kx_ = dx(2.2f, S->ang + 90), ky_ = dy(2.2f, S->ang + 90);
    quadfill((int)(S->px + cx_ * 3 + kx_), (int)(S->py + cy_ * 3 + ky_),
             (int)(S->px + cx_ * 3 - kx_), (int)(S->py + cy_ * 3 - ky_),
             (int)(S->px - cx_ * 2 - kx_), (int)(S->py - cy_ * 2 - ky_),
             (int)(S->px - cx_ * 2 + kx_), (int)(S->py - cy_ * 2 + ky_), CLR_TRUE_BLUE);

    // the teaching device: green = where you're GOING, nose = where you POINT
    line((int)S->px, (int)S->py,
         (int)(S->px + S->vx * 10), (int)(S->py + S->vy * 10), CLR_GREEN);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("steer", 4, 2, CLR_WHITE);
    print(str("cones %d/%d", S->ngot, NCONE), 110, 2, CLR_YELLOW);
    print(S->drift ? "drift tires (X)" : "racing tires(X)",
          SCREEN_W - 124, 2, S->drift ? CLR_ORANGE : CLR_GREEN);

    if (S->win) {
        print_centered("ALL CONES!", SCREEN_W / 2, SCREEN_H / 2 - 6, CLR_YELLOW);
        print_centered("press Z to play again", SCREEN_W / 2, SCREEN_H / 2 + 4, CLR_WHITE);
    }
}
