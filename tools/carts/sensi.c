/* de:meta
{
  "slug": "sensi",
  "title": "sensible soccer",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "steering-behaviors",
    "camera-follow"
  ],
  "lineage": "Sensible Soccer (1992) demake on a 320×640 scrolling pitch; the headline mechanic is after-touch ball curl (a post-kick directional impulse), sitting on top of steer-to-nearest-target AI for all ten CPU-driven players and a palette-swap kit trick.",
  "genre": "sports",
  "homage": "Sensible Soccer (1992)",
  "description": "Top-down arcade footy down a vertically-scrolling pitch, 6-a-side. The headline is AFTER-TOUCH: kick the ball, then HOLD a direction while it's in flight and it bends in the air — banana shots that curl round the keeper, just like the Amiga classic. You always control the team-mate nearest the ball (it auto-switches, with a ring to show who); the CPU steers your other five plus the whole opposing team. The mow-striped pitch is a real tile map() with primitive markings and goals, the camera follows the ball, and both kits + keepers come from ONE tiny player sprite recoloured per-team with pal() (the crowd-cart magic-colour trick). Ball shadow that lifts on lobs, net-ripple + screen-shake + a crowd roar on goals, a ref whistle, and a crowd bed that swells as play nears a goal. Two 90s halves with a half-time end-swap, win/lose/draw, best result saved. D-pad run, Z kick/tackle, then hold to bend."
}
de:meta */
#include "studio.h"

// SENSIBLE SOCCER — top-down arcade footy, 6-a-side, one button.
//
// The headline mechanic is AFTER-TOUCH: kick the ball, then HOLD a direction
// while it's in flight and it bends in the air. That curl is the whole soul of
// the original — banana shots round the keeper, curling crosses to the far post.
//
// You always control the team-mate NEAREST the ball (it auto-switches); the CPU
// runs your other five plus the whole opposing team with simple steer-to-target
// AI. The pitch is a real mow-striped tile map() in a 320x640 world the camera
// scrolls vertically via follow(); the markings, goals and ball are primitives.
// Two sprite kits + keepers all come from ONE tiny player sprite recoloured with
// pal() (shirt = magic index 28, shorts = 29), exactly like the crowd cart.
//
//   D-pad : run            Z : kick / shoot  (when you have it)
//   Z     : tackle / press (when you don't)
//   after a kick, HOLD a direction to BEND the ball in flight.

// ---- world / pitch (world is taller than the screen → vertical scroll) ------
#define WORLD_W   320
#define WORLD_H   640
#define PITCH_L    18
#define PITCH_R   302
#define PITCH_T    30
#define PITCH_B   610
#define PITCH_W   (PITCH_R - PITCH_L)        // 284
#define MID_Y     ((PITCH_T + PITCH_B) / 2)  // 320
#define GOAL_L    128
#define GOAL_R    192
#define GOAL_W    (GOAL_R - GOAL_L)          // 64
#define GOAL_DEPTH 14

// ---- teams / players --------------------------------------------------------
#define NP        12        // 6 per side, index t*6 is that team's keeper
#define GK         0
#define FIELD      1

#define HUMAN_SPD  1.95f
#define AI_SPD     1.70f
#define GK_SPD     1.55f
#define PLAYER_ACC 0.42f
#define AI_ACC     0.34f
#define GK_ACC     0.40f
#define FRIC       0.82f

// ---- ball -------------------------------------------------------------------
#define BALL_R         3
#define BALL_GRAV      0.30f
#define MAX_BALL_V     6.2f
#define SHOT_POWER     4.7f
#define PASS_POWER     3.4f
#define GK_CLEAR_POWER 5.0f
#define SHOT_LOFT      3.2f
#define PASS_LOFT      1.4f
#define CURVE          0.17f   // after-touch bend per frame
#define AFTERTOUCH     36      // frames you can bend a kicked ball
#define CONTROL_R      8
#define TACKLE_R       9
#define SHOOT_DIST     135

#define HALF_SECS      90

// ---- match modes ------------------------------------------------------------
#define M_KICKOFF 0
#define M_PLAY    1
#define M_GOAL    2
#define M_HALF    3
#define M_FULL    4

typedef struct {
    float x, y, vx, vy;
    int   team;            // 0 = YOU, 1 = CPU
    int   role;            // GK / FIELD
    float dir;             // facing (degrees) — last run/kick direction
    int   shirt, shorts;   // palette indices the magic colours recolour into
} Player;

static Player pl[NP];

// formation (slot 0 = keeper): cross fraction + depth-from-own-goal fraction
static const float FX[6] = { 0.50f, 0.26f, 0.74f, 0.50f, 0.34f, 0.66f };
static const float FF[6] = { 0.06f, 0.27f, 0.27f, 0.50f, 0.72f, 0.72f };

// ball
static float bx, by, bvx, bvy, bz, bvz;
static int   owner;          // player index in possession, or -1 (loose / in flight)
static int   lastTouch;      // team that last touched it (for restarts)
static int   recap;          // frames after a kick where nobody can collect
static int   ownGrace;       // frames a fresh owner can't be tackled
static int   atouch;         // after-touch frames remaining
static bool  humanShot;      // was the live kick taken by the player?

static int   controlled;     // the team-0 player you're driving
static int   presser[2];     // each team's nearest field player to the ball

static int   score[2];
static int   attackGoalY[2]; // PITCH_T or PITCH_B — which goal each team attacks
static int   half;
static float mclock;         // seconds elapsed in this half
static int   mode, koTeam;
static float modeT;          // countdown for KICKOFF / GOAL / HALF
static float netHit[2];      // net-ripple amount per goal (0 top, 1 bottom)

static const char *flashMsg = "";
static float flashT = 0;

static int  best, bestSet;   // saved best goal difference

static void setFlash(const char *m, float t) { flashMsg = m; flashT = t; }

// ---------------------------------------------------------------------------
static void whistle(void)      { hit(99, 7, 4, 160); schedule(90, 103, 7, 3); }
static void whistle_soft(void) { hit(96, 7, 3, 90); }

static int nearest_field(int t) {
    int best_i = t * 6 + 1; float bd = 1e9f;
    for (int i = t * 6 + 1; i < t * 6 + 6; i++) {
        float d = distance((int)pl[i].x, (int)pl[i].y, (int)bx, (int)by);
        if (d < bd) { bd = d; best_i = i; }
    }
    return best_i;
}

static void set_home(int i, bool weHave, float *hx, float *hy) {
    int t = pl[i].team, s = i - t * 6;
    float ownY = (attackGoalY[t] == PITCH_T) ? (float)PITCH_B : (float)PITCH_T;
    float atkY = (float)attackGoalY[t];
    float bias = weHave ? 0.10f : -0.05f;
    float depth = clamp(FF[s] + bias, 0.03f, 0.95f);
    float yy = lerp(ownY, atkY, depth);
    float xx = PITCH_L + FX[s] * PITCH_W;
    xx += (bx - xx) * 0.12f;       // drift toward the ball's side
    yy += (by - yy) * 0.10f;
    *hx = clamp(xx, PITCH_L + 6, PITCH_R - 6);
    *hy = clamp(yy, PITCH_T + 6, PITCH_B - 6);
}

static void place_all(void) {
    for (int i = 0; i < NP; i++) {
        int t = pl[i].team, s = i - t * 6;
        float ownY = (attackGoalY[t] == PITCH_T) ? (float)PITCH_B : (float)PITCH_T;
        float atkY = (float)attackGoalY[t];
        pl[i].x = PITCH_L + FX[s] * PITCH_W;
        pl[i].y = lerp(ownY, atkY, FF[s]);
        pl[i].vx = pl[i].vy = 0;
    }
}

// best forward team-mate to pass to, or -1
static int best_pass(int from) {
    int t = pl[from].team; int atk = attackGoalY[t];
    int best_j = -1; float bd = 1e9f;
    for (int j = t * 6 + 1; j < t * 6 + 6; j++) {
        if (j == from) continue;
        float ahead = (atk == PITCH_T) ? (pl[from].y - pl[j].y) : (pl[j].y - pl[from].y);
        if (ahead < 25) continue;
        float d = distance((int)pl[from].x, (int)pl[from].y, (int)pl[j].x, (int)pl[j].y);
        if (d < 150 && d < bd) { bd = d; best_j = j; }
    }
    return best_j;
}

static void kick(int who, float deg, float power, float loft, bool human) {
    owner = -1; lastTouch = pl[who].team;
    bvx = dx(power, deg); bvy = dy(power, deg);
    bvz = loft; bz = 1.0f;
    recap = 14;
    pl[who].dir = deg;
    if (human) { atouch = AFTERTOUCH; humanShot = true; }
    else       { atouch = 0;        humanShot = false; }
    hit(46 + rnd(8), INSTR_SQUARE, 4, 45);
}

// ---- restarts ---------------------------------------------------------------
static void kickoff(int team) {
    place_all();
    bx = 160; by = MID_Y; bvx = bvy = bz = bvz = 0;
    int ko = team * 6 + 3;                         // central mid takes it
    pl[ko].x = 160;
    pl[ko].y = MID_Y + (attackGoalY[team] == PITCH_T ? 14 : -14);
    owner = ko; lastTouch = team; ownGrace = 40; recap = 0; atouch = 0; humanShot = false;
    controlled = (team == 0) ? ko : nearest_field(0);
    mode = M_KICKOFF; modeT = 1.2f;
    whistle();
}

static void do_goal(int side) {                    // 0 = top net, 1 = bottom net
    int line = side ? PITCH_B : PITCH_T;
    int scorer = (attackGoalY[0] == line) ? 0 : 1;
    score[scorer]++;
    netHit[side] = 1.0f;
    koTeam = 1 - scorer;
    mode = M_GOAL; modeT = 2.6f;
    by = clamp(by, (float)(PITCH_T - GOAL_DEPTH + 3), (float)(PITCH_B + GOAL_DEPTH - 3));
    bvx = bvy = bvz = 0; bz = 0; owner = -1;
    shake(6.0f);
    whistle();
    chord(60, CHORD_MAJ, INSTR_SAW, 6);
    setFlash(scorer == 0 ? "G O A L ! !" : "CPU SCORES", 2.4f);
}

static void throw_in(void) {
    float ty = clamp(by, PITCH_T + 14, PITCH_B - 14);
    float tx = (bx < PITCH_L) ? PITCH_L + 3 : PITCH_R - 3;
    bx = tx; by = ty; bvx = bvy = bz = bvz = 0;
    int recv = 1 - lastTouch;
    int taker = nearest_field(recv);
    pl[taker].x = tx + (tx < 160 ? 7 : -7); pl[taker].y = ty;
    owner = taker; lastTouch = recv; ownGrace = 20; recap = 0;
    setFlash("THROW IN", 0.9f); whistle_soft();
}

static void goal_kick(int side) {                  // ball out over a goal line, not in
    int other = side ? PITCH_T : PITCH_B;
    int defTeam = (attackGoalY[0] == other) ? 0 : 1;
    float gy = side ? PITCH_B - 42 : PITCH_T + 42;
    bx = 160; by = gy; bvx = bvy = bz = bvz = 0;
    int taker = defTeam * 6;                        // the keeper
    pl[taker].x = 160; pl[taker].y = side ? PITCH_B - 16 : PITCH_T + 16;
    owner = taker; lastTouch = defTeam; ownGrace = 16; recap = 0;
    setFlash("GOAL KICK", 0.9f); whistle_soft();
}

// ---- control ----------------------------------------------------------------
static void human_control(void) {
    int i = controlled; Player *p = &pl[i];
    float ix = 0, iy = 0;
    if (btn(0, BTN_LEFT))  ix -= 1; if (btn(0, BTN_RIGHT)) ix += 1;
    if (btn(0, BTN_UP))    iy -= 1; if (btn(0, BTN_DOWN))  iy += 1;
    bool moving = (ix != 0 || iy != 0);
    if (moving) {
        float l = fsqrt(ix * ix + iy * iy); ix /= l; iy /= l;
        p->vx += ix * PLAYER_ACC; p->vy += iy * PLAYER_ACC;
        p->dir = angle_to(0, 0, (int)(ix * 100), (int)(iy * 100));
    }
    if (btnp(0, BTN_A)) {
        if (owner == i) {                           // shoot / pass
            float kd = moving ? p->dir
                              : angle_to((int)p->x, (int)p->y, WORLD_W / 2, attackGoalY[0]);
            kick(i, kd, SHOT_POWER, SHOT_LOFT, true);
            shake(1.5f);
        } else {                                    // tackle / press
            float a = angle_to((int)p->x, (int)p->y, (int)bx, (int)by);
            p->vx += dx(2.0f, a); p->vy += dy(2.0f, a); p->dir = a;
            if (owner >= 0 && pl[owner].team != 0 &&
                near((int)p->x, (int)p->y, (int)pl[owner].x, (int)pl[owner].y, TACKLE_R + 2)) {
                owner = i; lastTouch = 0; ownGrace = 12; recap = 0;
                hit(34, INSTR_NOISE, 3, 60); shake(1.2f);
            }
        }
    }
}

static void ai_control(int i) {
    Player *p = &pl[i];
    int t = p->team;
    float acc = AI_ACC;

    if (p->role == GK) {
        if (owner == i) {                           // collected → clear it long
            float kd = angle_to((int)p->x, (int)p->y, WORLD_W / 2, attackGoalY[t]);
            kick(i, kd, GK_CLEAR_POWER, 3.0f, false);
            return;
        }
        float lineY = (attackGoalY[t] == PITCH_T) ? PITCH_B - 10 : PITCH_T + 10;
        float tx = clamp(bx, GOAL_L + 8, GOAL_R - 8), ty = lineY;
        float ownGoalY = (attackGoalY[t] == PITCH_T) ? PITCH_B : PITCH_T;
        if (distance((int)bx, (int)by, WORLD_W / 2, (int)ownGoalY) < 80) {
            tx = clamp(bx, GOAL_L, GOAL_R); ty = lerp(lineY, by, 0.25f);
        }
        p->dir = angle_to((int)p->x, (int)p->y, (int)tx, (int)ty);
        p->vx += dx(GK_ACC, p->dir); p->vy += dy(GK_ACC, p->dir);
        return;
    }

    bool weHave = (owner >= 0 && pl[owner].team == t);

    if (weHave && owner == i) {                      // carrying — drive at goal
        float gx = WORLD_W / 2, gy = attackGoalY[t];
        float dg = distance((int)p->x, (int)p->y, (int)gx, (int)gy);
        p->dir = angle_to((int)p->x, (int)p->y, (int)gx, (int)gy);
        if (dg < SHOOT_DIST) {
            float aim = angle_to((int)p->x, (int)p->y, (int)(gx + rnd_between(-16, 16)), (int)gy);
            kick(i, aim, SHOT_POWER, SHOT_LOFT, false); shake(1.0f);
            return;
        }
        if (frame() % 40 == 0 && chance(45)) {
            int j = best_pass(i);
            if (j >= 0) {
                float pa = angle_to((int)p->x, (int)p->y, (int)pl[j].x, (int)pl[j].y);
                kick(i, pa, PASS_POWER, PASS_LOFT, false); return;
            }
        }
        p->vx += dx(acc, p->dir); p->vy += dy(acc, p->dir);
        return;
    }

    if (!weHave && i == presser[t]) {                // chase the loose / enemy ball
        float tx = bx + bvx * 4, ty = by + bvy * 4;
        p->dir = angle_to((int)p->x, (int)p->y, (int)tx, (int)ty);
        p->vx += dx(acc, p->dir); p->vy += dy(acc, p->dir);
        return;
    }

    // otherwise hold a formation slot (support in attack, screen in defence)
    float hx, hy; set_home(i, weHave, &hx, &hy);
    float dh = distance((int)p->x, (int)p->y, (int)hx, (int)hy);
    if (dh > 6) {
        p->dir = angle_to((int)p->x, (int)p->y, (int)hx, (int)hy);
        float a = acc * clamp(dh / 40.0f, 0.3f, 1.0f);
        p->vx += dx(a, p->dir); p->vy += dy(a, p->dir);
    }
}

static void integrate(float k) {
    for (int i = 0; i < NP; i++) {
        Player *p = &pl[i];
        p->vx *= FRIC; p->vy *= FRIC;
        float mx = (i == controlled) ? HUMAN_SPD : (p->role == GK ? GK_SPD : AI_SPD);
        float sp = fsqrt(p->vx * p->vx + p->vy * p->vy);
        if (sp > mx) { p->vx = p->vx / sp * mx; p->vy = p->vy / sp * mx; }
        p->x += p->vx * k; p->y += p->vy * k;
        p->x = clamp(p->x, PITCH_L + 4, PITCH_R - 4);
        p->y = clamp(p->y, PITCH_T + 2, PITCH_B - 2);
    }
    // light separation so players don't stack (same idea as the crowd cart)
    for (int i = 0; i < NP; i++)
        for (int j = i + 1; j < NP; j++) {
            float ddx = pl[j].x - pl[i].x, ddy = pl[j].y - pl[i].y;
            float d = fsqrt(ddx * ddx + ddy * ddy);
            if (d > 0.1f && d < 9.0f) {
                float push = (9.0f - d) * 0.5f, nx = ddx / d, ny = ddy / d;
                pl[i].x -= nx * push; pl[i].y -= ny * push;
                pl[j].x += nx * push; pl[j].y += ny * push;
            }
        }
}

static void dribble(float k) {
    Player *o = &pl[owner];
    float aheadx = o->x + dx(8, o->dir), aheady = o->y + dy(8, o->dir);
    bx = lerp(bx, aheadx, 0.5f); by = lerp(by, aheady, 0.5f);
    bvx = o->vx; bvy = o->vy; bz = 0; bvz = 0;
    (void)k;
}

static void acquire(void) {
    float bd = CONTROL_R; int bi = -1;
    for (int i = 0; i < NP; i++) {
        float d = distance((int)pl[i].x, (int)pl[i].y, (int)bx, (int)by);
        if (d < bd) { bd = d; bi = i; }
    }
    if (bi >= 0) { owner = bi; lastTouch = pl[bi].team; ownGrace = 10; humanShot = false; atouch = 0; }
}

static void ball_physics(float k) {
    // after-touch: HOLD a direction to bend a kicked ball in flight
    if (humanShot && atouch > 0) {
        float ix = 0, iy = 0;
        if (btn(0, BTN_LEFT))  ix -= 1; if (btn(0, BTN_RIGHT)) ix += 1;
        if (btn(0, BTN_UP))    iy -= 1; if (btn(0, BTN_DOWN))  iy += 1;
        if (ix != 0 || iy != 0) {
            float l = fsqrt(ix * ix + iy * iy); ix /= l; iy /= l;
            bvx += ix * CURVE; bvy += iy * CURVE;
        }
        atouch--; if (atouch <= 0) humanShot = false;
    }

    bz += bvz * k; bvz -= BALL_GRAV * k;
    if (bz <= 0) {
        bz = 0;
        if (bvz < -0.4f) { bvz = -bvz * 0.42f; hit(40, INSTR_SQUARE, 2, 22); }
        else bvz = 0;
    }
    float f = (bz > 2.0f) ? 0.99f : 0.95f;
    bvx *= f; bvy *= f;
    float bs = fsqrt(bvx * bvx + bvy * bvy);
    if (bs > MAX_BALL_V) { bvx = bvx / bs * MAX_BALL_V; bvy = bvy / bs * MAX_BALL_V; }
    bx += bvx * k; by += bvy * k;
    if (recap > 0) recap--;

    if (by < PITCH_T - 1) { if (bx > GOAL_L && bx < GOAL_R) do_goal(0); else goal_kick(0); return; }
    if (by > PITCH_B + 1) { if (bx > GOAL_L && bx < GOAL_R) do_goal(1); else goal_kick(1); return; }
    if (bx < PITCH_L)     { throw_in(); return; }
    if (bx > PITCH_R)     { throw_in(); return; }

    if (recap <= 0 && bz < 3.0f) acquire();
}

static void do_steal(void) {
    if (owner < 0 || ownGrace > 0) return;
    for (int j = 0; j < NP; j++) {
        if (pl[j].team == pl[owner].team) continue;
        if (near((int)pl[j].x, (int)pl[j].y, (int)pl[owner].x, (int)pl[owner].y, TACKLE_R) && chance(7)) {
            owner = j; lastTouch = pl[j].team; ownGrace = 12;
            hit(34, INSTR_NOISE, 2, 50);
            return;
        }
    }
}

static void fulltime(void) {
    mode = M_FULL;
    int diff = score[0] - score[1];
    if (!bestSet || diff > best) { best = diff; bestSet = 1; save(0, best); save(1, 1); }
    whistle();
}

static void end_half(void) {
    whistle();
    if (half == 1) { mode = M_HALF; modeT = 3.0f; setFlash("HALF TIME", 2.8f); }
    else            fulltime();
}

static void start_half2(void) {
    int tmp = attackGoalY[0]; attackGoalY[0] = attackGoalY[1]; attackGoalY[1] = tmp;
    half = 2; mclock = 0; kickoff(1);
}

// ---------------------------------------------------------------------------
void init(void) {
    colorkey(0);
    instrument(5, INSTR_NOISE, 20, 150, 2, 200);    // crowd murmur
    instrument_filter(5, FILTER_LOW, 650, 2);
    instrument(7, INSTR_SINE, 3, 20, 7, 40);        // ref whistle
    instrument_lfo(7, 0, LFO_PITCH, 15, 1.1f);      // trill
    bpm(120);

    for (int i = 0; i < NP; i++) {
        int t = i / 6;
        pl[i].team = t;
        pl[i].role = (i % 6 == 0) ? GK : FIELD;
        if (t == 0) { pl[i].shirt = CLR_BLUE; pl[i].shorts = CLR_WHITE; }
        else        { pl[i].shirt = CLR_RED;  pl[i].shorts = CLR_DARK_GREY; }
        if (pl[i].role == GK) {
            pl[i].shirt  = (t == 0) ? CLR_GREEN : CLR_YELLOW;
            pl[i].shorts = (t == 0) ? CLR_DARK_BLUE : CLR_DARK_GREY;
        }
    }

    score[0] = score[1] = 0;
    half = 1; mclock = 0;
    attackGoalY[0] = PITCH_T;       // YOU attack up first half
    attackGoalY[1] = PITCH_B;
    netHit[0] = netHit[1] = 0;
    owner = -1; humanShot = false; atouch = 0;
    bestSet = load(1); best = load(0);

    kickoff(0);
}

void update(void) {
    float k = dt() * 60.0f; if (k > 3) k = 3;
    if (flashT > 0) flashT -= dt();
    if (netHit[0] > 0) netHit[0] -= dt() * 1.6f;
    if (netHit[1] > 0) netHit[1] -= dt() * 1.6f;

    if (mode == M_FULL) { if (btnp(0, BTN_A)) init(); return; }

    if (mode == M_HALF) { modeT -= dt(); if (btnp(0, BTN_A) || modeT <= 0) start_half2(); return; }

    if (mode == M_KICKOFF) {
        modeT -= dt();
        if (frame() % 8 == 0) hit(26 + rnd(4), 5, 1, 70);
        if (modeT <= 0) mode = M_PLAY;
        return;
    }

    if (mode == M_GOAL) {
        modeT -= dt();
        if (frame() % 3 == 0) hit(28 + rnd(8), 5, 5, 90);   // crowd roar swell
        if (modeT <= 0) kickoff(koTeam);
        return;
    }

    // ---- M_PLAY ----
    presser[0] = nearest_field(0);
    presser[1] = nearest_field(1);
    if (owner >= 0 && pl[owner].team == 0 && pl[owner].role == FIELD) controlled = owner;
    else controlled = nearest_field(0);

    for (int i = 0; i < NP; i++) {
        if (i == controlled) human_control();
        else                 ai_control(i);
    }
    integrate(k);

    if (owner >= 0) dribble(k);
    else            ball_physics(k);

    do_steal();
    if (ownGrace > 0) ownGrace--;

    // crowd bed — rises as the ball nears either goal
    float dG  = distance((int)bx, (int)by, 160, PITCH_T);
    float dG2 = distance((int)bx, (int)by, 160, PITCH_B);
    float dmin = (dG < dG2) ? dG : dG2;
    float e = clamp(remap(dmin, 50, 250, 1.0f, 0.15f), 0.12f, 1.0f);
    if (frame() % 5 == 0) hit(26 + rnd(6), 5, 1 + (int)(e * 3), 70);

    mclock += dt();
    if (mclock >= HALF_SECS) end_half();
}

// ---- rendering --------------------------------------------------------------
static void draw_field(void) {
    // touchlines + halfway line
    rect(PITCH_L, PITCH_T, PITCH_W, PITCH_B - PITCH_T, CLR_WHITE);
    line(PITCH_L, MID_Y, PITCH_R, MID_Y, CLR_WHITE);
    circ(160, MID_Y, 36, CLR_WHITE);
    circfill(160, MID_Y, 1, CLR_WHITE);
    // penalty + 6-yard boxes, both ends
    rect(96, PITCH_T, 128, 60, CLR_WHITE);
    rect(124, PITCH_T, 72, 24, CLR_WHITE);
    circfill(160, PITCH_T + 42, 1, CLR_WHITE);
    rect(96, PITCH_B - 60, 128, 60, CLR_WHITE);
    rect(124, PITCH_B - 24, 72, 24, CLR_WHITE);
    circfill(160, PITCH_B - 42, 1, CLR_WHITE);
}

static void draw_goal(int lineY, int dir, float rip) {
    int back = lineY + dir * GOAL_DEPTH;
    int y0 = (dir < 0) ? back : lineY;
    int y1 = (dir < 0) ? lineY : back;
    rectfill(GOAL_L, y0, GOAL_W, GOAL_DEPTH, CLR_BROWNISH_BLACK);
    if (rip < 0) rip = 0;
    for (int x = GOAL_L + 3; x < GOAL_R; x += 5) {
        int off = (int)(sin_deg(frame() * 9 + (x - GOAL_L) * 22) * 3.0f * rip);
        line(x, y0, x + off, y1, CLR_DARK_GREY);
    }
    for (int yy = y0 + 3; yy < y1; yy += 4)
        line(GOAL_L, yy, GOAL_R, yy, CLR_DARK_GREY);
    line(GOAL_L, lineY, GOAL_L, back, CLR_WHITE);
    line(GOAL_R, lineY, GOAL_R, back, CLR_WHITE);
    line(GOAL_L, back, GOAL_R, back, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARK_GREEN);
    follow((int)bx, (int)by, WORLD_W, WORLD_H);
    map(0, 0, 0, 0, 20, 40);
    draw_field();
    draw_goal(PITCH_T, -1, netHit[0]);
    draw_goal(PITCH_B, +1, netHit[1]);

    // players, back-to-front by y so nearer ones overlap
    int ord[NP];
    for (int i = 0; i < NP; i++) ord[i] = i;
    for (int i = 1; i < NP; i++) {
        int v = ord[i], j = i - 1;
        while (j >= 0 && pl[ord[j]].y > pl[v].y) { ord[j + 1] = ord[j]; j--; }
        ord[j + 1] = v;
    }
    for (int n = 0; n < NP; n++) {
        int i = ord[n]; Player *p = &pl[i];
        int px = (int)p->x, py = (int)p->y;
        ovalfill(px, py + 5, 5, 2, CLR_BROWNISH_BLACK);          // shadow
        if (i == controlled) circ(px, py + 3, 8, CLR_YELLOW);    // who you drive
        int fr = 0;
        float sp = fsqrt(p->vx * p->vx + p->vy * p->vy);
        if (sp > 0.25f) fr = anim(4, 11.0f, i * 0.11f);
        pal(28, p->shirt); pal(29, p->shorts);
        spr(fr, px - 8, py - 13);
        pal_reset();
    }

    // ball + lifting shadow
    int gx = (int)bx, gy = (int)by, yy = gy - (int)bz;
    ovalfill(gx, gy + 2, BALL_R, 2, CLR_BROWNISH_BLACK);
    circfill(gx, yy, BALL_R, CLR_WHITE);
    circ(gx, yy, BALL_R, CLR_LIGHT_GREY);
    pset(gx, yy, CLR_DARK_GREY);

    // ---- HUD (screen space) ----
    camera(0, 0);
    rectfill(0, 0, SCREEN_W, 11, CLR_DARKER_BLUE);
    print("YOU", 4, 2, CLR_BLUE);
    print(str("%d", score[0]), 28, 2, CLR_WHITE);
    print_right("CPU", SCREEN_W - 4, 2, CLR_RED);
    print_right(str("%d", score[1]), SCREEN_W - 28, 2, CLR_WHITE);
    int tt = (int)mclock; if (tt > HALF_SECS) tt = HALF_SECS;
    print_centered(str("H%d  %d:%02d", half, tt / 60, tt % 60), SCREEN_W/2, 2, CLR_LIGHT_YELLOW);

    if (humanShot && atouch > 0 && blink(4))
        print_centered("BEND IT!", SCREEN_W/2, 22, CLR_YELLOW);
    if (half == 1 && mclock < 6 && mode == M_PLAY)
        print("Z=kick/tackle  hold to bend", 6, SCREEN_H - 9, CLR_LIGHT_GREY);

    if (flashT > 0) print_centered(flashMsg, SCREEN_W/2, SCREEN_H / 2 - 24, CLR_WHITE);
    if (mode == M_KICKOFF) print_centered(str("%d", (int)modeT + 1), SCREEN_W/2, SCREEN_H / 2, CLR_WHITE);

    if (mode == M_HALF) {
        rectfill(60, 78, 200, 44, CLR_BLACK); rect(60, 78, 200, 44, CLR_WHITE);
        print_centered("HALF TIME", SCREEN_W/2, 86, CLR_LIGHT_YELLOW);
        print_centered(str("%d - %d", score[0], score[1]), SCREEN_W/2, 100, CLR_WHITE);
        print_centered("Z to kick off", SCREEN_W/2, 112, CLR_LIGHT_GREY);
    }
    if (mode == M_FULL) {
        rectfill(50, 68, 220, 64, CLR_BLACK); rect(50, 68, 220, 64, CLR_WHITE);
        const char *r = score[0] > score[1] ? "YOU WIN!" : score[0] < score[1] ? "CPU WINS" : "DRAW";
        int rc = score[0] > score[1] ? CLR_GREEN : score[0] < score[1] ? CLR_RED : CLR_LIGHT_GREY;
        print_centered(r, SCREEN_W/2, 78, rc);
        print_centered(str("%d - %d", score[0], score[1]), SCREEN_W/2, 94, CLR_WHITE);
        if (bestSet) print_centered(str("best diff %+d", best), SCREEN_W/2, 108, CLR_LIGHT_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, 120, CLR_LIGHT_GREY);
    }
}
