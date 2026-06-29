/* de:meta
{
  "title": "rocket league",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "no-sprite-vehicles",
    "verlet-integration"
  ],
  "lineage": "2D demake of Rocket League (2015); angle-based boost thrust and circle-vs-circle impulse resolution are the technical core — cars rendered as rotated rects with no sprites.",
  "genre": "sports",
  "homage": "Rocket League (2015)",
  "description": "2D car soccer demake. Drive, jump, and BOOST to fly — boost thrusts along the way you point, so you pull off aerials in the air. Punchy ball physics, cars bump each other. First to 5 wins. P1 blue: A/D drive, W jump, Z boost — P2 orange: </> drive, ^ jump, , boost."
}
de:meta */
#include "studio.h"

// ROCKET LEAGUE — 2D car soccer, couch 2-player
// drive on the ground, jump, and BOOST to fly + pull off aerials.
// player 1 (blue):   A/D drive   W jump   Z boost
// player 2 (orange): </> drive   ^ jump   , boost
// first to 5 wins — Z or , to replay

// ---- field ----------------------------------------------------------------
#define GROUND_Y    178                 // ground surface (pixels from top)
#define GOAL_H       52                 // goal mouth height
#define GOAL_DEPTH    8                 // how deep the net pocket is

// ---- car ------------------------------------------------------------------
#define CAR_W        18
#define CAR_H        10
#define CAR_HW       (CAR_W / 2)
#define CAR_HH       (CAR_H / 2)
#define CAR_R         9                 // collision radius (car treated as a circle vs ball)
#define DRIVE       0.55f               // ground acceleration
#define MAX_DRIVE   4.2f                // top ground speed (without boost)
#define GROUND_FRIC 0.86f               // coast-down when no input
#define JUMP_V     -5.2f                // jump impulse
#define ROT_SPEED   4.5f                // air rotation (deg / frame)
#define BOOST_ACC   0.45f               // thrust per frame while boosting
#define GRAV        0.28f               // gravity on the car

// ---- ball -----------------------------------------------------------------
#define BALL_R        6
#define BALL_GRAV   0.22f
#define BALL_BOUNCE 0.80f
#define MAX_BALL_V  7.5f

#define WIN_SCORE     5
#define FLOOR_Y     (GROUND_Y - CAR_HH) // car-center rest height

typedef struct {
    float x, y, vx, vy, ang;            // ang in degrees: 0 = facing right, 180 = left
    int   dir;                          // ground facing: -1 left, +1 right
    bool  grounded;
    bool  djump;                        // double-jump still available?
    float boost;                        // 0..100
    int   color, accent;
} Car;

static Car   car[2];
static float bx, by, bvx, bvy;          // ball
static int   score[2];
static bool  over;
static int   kickoff;                   // countdown frames before play resumes

// ---------------------------------------------------------------------------
static void kick_off(void) {
    bx = SCREEN_W / 2; by = SCREEN_H / 2 - 20; bvx = bvy = 0;

    car[0].x = 70;            car[1].x = SCREEN_W - 70;
    car[0].dir = +1;          car[1].dir = -1;
    car[0].ang = 0;           car[1].ang = 180;
    for (int i = 0; i < 2; i++) {
        car[i].y = FLOOR_Y;
        car[i].vx = car[i].vy = 0;
        car[i].grounded = true;
        car[i].djump = false;
        car[i].boost = 100;
    }
    kickoff = 50;
    hit(84, INSTR_SINE, 3, 120);        // ref whistle
}

void init(void) {
    score[0] = score[1] = 0;
    over = false;
    car[0].color = CLR_BLUE;   car[0].accent = CLR_LIGHT_PEACH;
    car[1].color = CLR_ORANGE; car[1].accent = CLR_LIGHT_PEACH;
    kick_off();
}

static void scored(int who) {
    score[who]++;
    chord(60, CHORD_MAJ, INSTR_SAW, 6);
    if (score[who] >= WIN_SCORE) { over = true; strum(60, CHORD_MAJ7, INSTR_SQUARE, 5, 60); }
    else                          kick_off();
}

// car-vs-ball: separate, reflect along the contact normal, add a boost-charged kick
static void hit_ball(Car *c, bool boosting) {
    float dx_ = bx - c->x, dy_ = by - c->y;
    float d   = distance((int)c->x, (int)c->y, (int)bx, (int)by);
    if (d >= CAR_R + BALL_R || d <= 0.01f) return;

    float nx = dx_ / d, ny = dy_ / d;
    bx = c->x + nx * (CAR_R + BALL_R);  // push the ball out of the car
    by = c->y + ny * (CAR_R + BALL_R);

    float rvx = bvx - c->vx, rvy = bvy - c->vy;
    float dot = rvx * nx + rvy * ny;
    if (dot < 0) { bvx -= (1 + BALL_BOUNCE) * dot * nx; bvy -= (1 + BALL_BOUNCE) * dot * ny; }

    float kick = boosting ? 4.2f : 2.4f;
    bvx += nx * kick + c->vx * 0.45f;
    bvy += ny * kick + c->vy * 0.45f;

    note(46 + rnd(10), INSTR_SQUARE, 4);
}

void update(void) {
    if (over) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) init(); return; }

    if (kickoff > 0) { kickoff--; return; }   // frozen during countdown

    // ---- cars ----
    for (int i = 0; i < 2; i++) {
        Car *c = &car[i];
        bool left  = btn(i, BTN_LEFT);
        bool right = btn(i, BTN_RIGHT);
        bool boost = btn(i, BTN_A) && c->boost > 0;

        if (c->grounded) {
            if (left)  { c->vx -= DRIVE; c->dir = -1; }
            if (right) { c->vx += DRIVE; c->dir = +1; }
            if (!left && !right) c->vx *= GROUND_FRIC;
            c->ang = (c->dir > 0) ? 0 : 180;
            float mx = boost ? 7.0f : MAX_DRIVE;
            c->vx = clamp(c->vx, -mx, mx);
            if (btnp(i, BTN_UP)) { c->vy = JUMP_V; c->grounded = false; c->djump = true;
                                   note(64, INSTR_SQUARE, 3); }
        } else {
            if (left)  c->ang -= ROT_SPEED;
            if (right) c->ang += ROT_SPEED;
            if (btnp(i, BTN_UP) && c->djump) { c->vy = JUMP_V * 0.85f; c->djump = false;
                                               note(67, INSTR_SQUARE, 3); }
        }

        // boost — thrust along facing (horizontal on the ground, along ang in the air)
        if (boost) {
            if (c->grounded) c->vx += c->dir * BOOST_ACC * 1.3f;
            else { c->vx += cos_deg(c->ang) * BOOST_ACC; c->vy += sin_deg(c->ang) * BOOST_ACC; }
            c->boost -= 1.1f;
            if (frame() % 3 == 0) hit(38, INSTR_NOISE, 2, 40);
        } else if (c->boost < 100) {
            c->boost += 0.3f;
        }

        // integrate
        if (!c->grounded) c->vy += GRAV;
        c->vx = clamp(c->vx, -8, 8);
        c->vy = clamp(c->vy, -9, 9);
        c->x += c->vx;
        c->y += c->vy;

        // floor / ceiling
        if (c->y >= FLOOR_Y) { c->y = FLOOR_Y; if (c->vy > 0) c->vy = 0;
                               c->grounded = true; c->djump = false; c->ang = (c->dir > 0) ? 0 : 180; }
        else c->grounded = false;
        if (c->y < CAR_HH + 2) { c->y = CAR_HH + 2; if (c->vy < 0) c->vy = 0; }

        // side walls
        if (c->x < CAR_HW)             { c->x = CAR_HW;             if (c->vx < 0) c->vx = 0; }
        if (c->x > SCREEN_W - CAR_HW)  { c->x = SCREEN_W - CAR_HW;  if (c->vx > 0) c->vx = 0; }

        hit_ball(c, boost);
    }

    // ---- car-vs-car bump ----
    {
        float dx_ = car[1].x - car[0].x, dy_ = car[1].y - car[0].y;
        float d   = distance((int)car[0].x, (int)car[0].y, (int)car[1].x, (int)car[1].y);
        float minD = CAR_R * 2;
        if (d > 0.01f && d < minD) {
            float nx = dx_ / d, ny = dy_ / d, push = (minD - d) / 2;
            car[0].x -= nx * push; car[0].y -= ny * push;
            car[1].x += nx * push; car[1].y += ny * push;
            float t = car[0].vx; car[0].vx = car[1].vx * 0.7f; car[1].vx = t * 0.7f;
        }
    }

    // ---- ball ----
    bvy += BALL_GRAV;
    bvx *= 0.996f;
    bvx = clamp(bvx, -MAX_BALL_V, MAX_BALL_V);
    bvy = clamp(bvy, -MAX_BALL_V, MAX_BALL_V);
    bx += bvx;
    by += bvy;

    float ballFloor = GROUND_Y - BALL_R;
    if (by >= ballFloor) { by = ballFloor; bvy = -bvy * BALL_BOUNCE; bvx *= 0.97f; }
    if (by <= BALL_R + 1) { by = BALL_R + 1; bvy = -bvy * BALL_BOUNCE; }

    float mouthTop = GROUND_Y - GOAL_H;
    // left edge
    if (bx - BALL_R <= 0) {
        if (by + BALL_R > mouthTop) scored(1);              // into blue's net → orange scores
        else { bx = BALL_R; bvx = -bvx * BALL_BOUNCE; }
    }
    // right edge
    if (bx + BALL_R >= SCREEN_W) {
        if (by + BALL_R > mouthTop) scored(0);              // into orange's net → blue scores
        else { bx = SCREEN_W - BALL_R; bvx = -bvx * BALL_BOUNCE; }
    }
}

// compute the 4 rotated corners of a car body
static void corners(Car *c, float ox[4], float oy[4]) {
    const float lx[4] = { -CAR_HW,  CAR_HW, CAR_HW, -CAR_HW };
    const float ly[4] = { -CAR_HH, -CAR_HH, CAR_HH,  CAR_HH };
    float cs = cos_deg(c->ang), sn = sin_deg(c->ang);
    for (int k = 0; k < 4; k++) {
        ox[k] = c->x + lx[k] * cs - ly[k] * sn;
        oy[k] = c->y + lx[k] * sn + ly[k] * cs;
    }
}

static void draw_car(Car *c, bool boosting) {
    float px[4], py[4];
    corners(c, px, py);

    // boost flame behind the car
    if (boosting) {
        float fx = (px[0] + px[3]) / 2, fy = (py[0] + py[3]) / 2;
        float tx = fx - cos_deg(c->ang) * 9, ty = fy - sin_deg(c->ang) * 9;
        float perpx = -sin_deg(c->ang) * 3, perpy = cos_deg(c->ang) * 3;
        int flame = (frame() % 4 < 2) ? CLR_YELLOW : CLR_ORANGE;
        trifill((int)(fx + perpx), (int)(fy + perpy),
                (int)(fx - perpx), (int)(fy - perpy), (int)tx, (int)ty, flame);
    }

    // body (two triangles)
    trifill((int)px[0], (int)py[0], (int)px[1], (int)py[1], (int)px[2], (int)py[2], c->color);
    trifill((int)px[0], (int)py[0], (int)px[2], (int)py[2], (int)px[3], (int)py[3], c->color);

    // wheels at the two bottom corners
    pset((int)px[2], (int)py[2], CLR_BLACK);
    circfill((int)px[2], (int)py[2], 2, CLR_BLACK);
    circfill((int)px[3], (int)py[3], 2, CLR_BLACK);

    // nose accent (front of car)
    float nx = (px[1] + px[2]) / 2, ny = (py[1] + py[2]) / 2;
    circfill((int)nx, (int)ny, 2, c->accent);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // arena floor
    rectfill(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, CLR_DARK_GREEN);
    line(0, GROUND_Y, SCREEN_W, GROUND_Y, CLR_GREEN);

    // center line + circle
    for (int y = 0; y < GROUND_Y; y += 10) pset(SCREEN_W / 2, y, CLR_DARKER_GREY);
    circ(SCREEN_W / 2, GROUND_Y, 22, CLR_DARK_GREY);

    // goals
    int mouthTop = GROUND_Y - GOAL_H;
    rect(0, mouthTop, GOAL_DEPTH, GOAL_H, CLR_BLUE);
    rect(SCREEN_W - GOAL_DEPTH, mouthTop, GOAL_DEPTH, GOAL_H, CLR_ORANGE);
    for (int y = mouthTop; y < GROUND_Y; y += 5) {
        pset(GOAL_DEPTH / 2, y, CLR_DARK_GREY);
        pset(SCREEN_W - GOAL_DEPTH / 2, y, CLR_DARK_GREY);
    }
    line(0, mouthTop, GOAL_DEPTH, mouthTop, CLR_LIGHT_GREY);
    line(SCREEN_W - GOAL_DEPTH, mouthTop, SCREEN_W, mouthTop, CLR_LIGHT_GREY);

    // ball
    circfill((int)bx, (int)by, BALL_R, CLR_WHITE);
    circ((int)bx, (int)by, BALL_R, CLR_LIGHT_GREY);
    pset((int)bx, (int)by, CLR_DARK_GREY);

    // cars
    draw_car(&car[0], btn(0, BTN_A) && car[0].boost > 0);
    draw_car(&car[1], btn(1, BTN_A) && car[1].boost > 0);

    // boost meters
    rectfill(6,  6, (int)(car[0].boost * 0.30f), 3, CLR_YELLOW);
    rectfill(SCREEN_W - 6 - (int)(car[1].boost * 0.30f), 6, (int)(car[1].boost * 0.30f), 3, CLR_YELLOW);

    // score
    print(str("%d", score[0]), SCREEN_W / 2 - 20, 4, CLR_BLUE);
    print("-", SCREEN_W / 2 - 2, 4, CLR_LIGHT_GREY);
    print(str("%d", score[1]), SCREEN_W / 2 + 14, 4, CLR_ORANGE);

    // kickoff countdown
    if (kickoff > 0 && !over) {
        int n = kickoff / 17 + 1;
        print_centered(str("%d", n), SCREEN_W/2, 70, CLR_WHITE);
    }

    // controls hint — first 5 seconds
    if (frame() < 300) {
        print("A/D  W jump  Z boost", 14, SCREEN_H - 12, CLR_DARK_GREY);
        print_right("</>  ^ jump  , boost", SCREEN_W - 14, SCREEN_H - 12, CLR_DARK_GREY);
    }

    // game over
    if (over) {
        rectfill(80, 80, 160, 40, CLR_BLACK);
        rect(80, 80, 160, 40, CLR_WHITE);
        print_centered(score[0] >= WIN_SCORE ? "BLUE WINS!" : "ORANGE WINS!", SCREEN_W/2, 90, score[0] >= WIN_SCORE ? CLR_BLUE : CLR_ORANGE);
        print_centered("Z / , to replay", SCREEN_W/2, 104, CLR_LIGHT_GREY);
    }
}
