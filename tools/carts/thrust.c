/* de:meta
{
  "title": "thrust (inertia)",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "spring-damper",
    "particle-system"
  ],
  "lineage": "Asteroids movement distilled to a pure teaching cart; steer.c is its direct sequel (the car model, partial coupling).",
  "genre": "space",
  "description": "The fourth movement paradigm: steering & inertia. The ship has a FACING and a VELOCITY and they are not the same thing — turning moves nothing, thrusting pushes along the facing, momentum carries you, drag bleeds it off. The green line shows where you are GOING while the nose shows where you are POINTING: watch them disagree mid-drift. Press X for tank mode (velocity = facing, release stops dead) and feel how much of the asteroids sensation IS the inertia. Collect the stars; the screen wraps. LEFT/RIGHT turn, UP thrusts, X toggles inertia, Z restarts."
}
de:meta */
#include "studio.h"

// THRUST — the fourth movement paradigm: steering & inertia. The others move a
// position directly; here the ship has a FACING and a VELOCITY, and they are
// not the same thing. Left/right turn the nose — that alone moves nothing.
// Thrust adds a push *along the facing* to the velocity; momentum carries you;
// a little drag bleeds it off. Everything about the asteroids feel lives in
// that separation:
//
//   facing  = where the nose points        (changed by turning)
//   velocity = where you're actually going (changed only by thrusting)
//
// Press X to collapse the two back together ("tank mode": holding thrust just
// moves you the way you face, releasing stops dead) and feel how much of the
// game IS the inertia. Drifting past a star nose-first while your velocity
// slides you sideways is the whole sensation.
//
// Collect the stars.  LEFT/RIGHT turn, UP thrusts.  X: toggle inertia.  Z: restart.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING — edit + hot-reload ───────────────────────────────────────────────
#define TURN     4.0f    // degrees per frame
#define THRUST   0.085f  // velocity added per frame of burn (along the facing)
#define DRAG     0.992f  // velocity kept per frame (1 = never slows, 0.9 = syrup)
#define MAX_SPD  3.2f    // speed cap
#define TANK_SPD 1.8f    // movement speed in no-inertia "tank" mode
#define SHIP_R   7       // ship size

#define NSTAR 8
static const int star_pos[NSTAR][2] = {
    {  60,  50 }, { 260,  40 }, { 160, 100 }, {  40, 150 },
    { 280, 160 }, { 120,  30 }, { 220, 110 }, {  80, 100 },
};

#define MAXP 32
typedef struct { float x, y, vx, vy; int life; } Puff;

STATE {
    float px, py;        // position
    float vx, vy;        // velocity — its OWN thing, separate from the facing
    float ang;           // facing, degrees
    bool  inertia;
    bool  got[NSTAR];
    int   ngot;
    bool  win;
    Puff  puff[MAXP];
};

static void reset_game(void) {
    S->px = SCREEN_W / 2.0f; S->py = SCREEN_H / 2.0f;
    S->vx = 0; S->vy = 0; S->ang = -90;          // nose up
    S->inertia = true;
    for (int i = 0; i < NSTAR; i++) S->got[i] = false;
    S->ngot = 0; S->win = false;
}

void init(void) { reset_game(); }

// exhaust puffs — they shoot BACKWARD from the nozzle, which both looks right
// and constantly shows the player the difference between facing and velocity
static void spawn_puff(void) {
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < MAXP; i++)
            if (S->puff[i].life <= 0) {
                float back = S->ang + 180 + rnd_between(-14, 15);
                S->puff[i] = (Puff){
                    S->px + dx(SHIP_R, S->ang + 180), S->py + dy(SHIP_R, S->ang + 180),
                    S->vx + dx(1.4f, back), S->vy + dy(1.4f, back),
                    rnd_between(8, 16) };
                break;
            }
}

void update(void) {
#ifdef DE_TRACE
    watch("t", "px=%.0f py=%.0f ang=%.0f vx=%.2f vy=%.2f inertia=%d stars=%d",
          S->px, S->py, S->ang, S->vx, S->vy, S->inertia, S->ngot);
#endif
    if (S->win) { if (btnp(0, BTN_A)) reset_game(); return; }

    if (btnp(0, BTN_B)) { S->inertia = !S->inertia; S->vx = 0; S->vy = 0; }

    // turning changes the FACING only — with inertia on, this moves nothing
    if (btn(0, BTN_LEFT))  S->ang -= TURN;
    if (btn(0, BTN_RIGHT)) S->ang += TURN;

    if (S->inertia) {
        if (btn(0, BTN_UP)) {                       // burn: push along the facing
            S->vx += dx(THRUST, S->ang);
            S->vy += dy(THRUST, S->ang);
            spawn_puff();
        }
        S->vx *= DRAG; S->vy *= DRAG;               // space with a whiff of friction
        float spd = fsqrt(S->vx * S->vx + S->vy * S->vy);
        if (spd > MAX_SPD) { S->vx *= MAX_SPD / spd; S->vy *= MAX_SPD / spd; }
        S->px += S->vx; S->py += S->vy;
    } else {                                        // tank mode: velocity IS the facing
        if (btn(0, BTN_UP)) {
            S->px += dx(TANK_SPD, S->ang);
            S->py += dy(TANK_SPD, S->ang);
            spawn_puff();
        }
        S->vx = 0; S->vy = 0;                       // release = stop dead
    }

    // wrap — flying off one edge brings you in on the other
    if (S->px < 0) S->px += SCREEN_W;  if (S->px >= SCREEN_W) S->px -= SCREEN_W;
    if (S->py < 0) S->py += SCREEN_H;  if (S->py >= SCREEN_H) S->py -= SCREEN_H;

    // stars
    for (int i = 0; i < NSTAR; i++) {
        if (S->got[i]) continue;
        if (near((int)S->px, (int)S->py, star_pos[i][0], star_pos[i][1], 10)) {
            S->got[i] = true; S->ngot++;
            note(76 + S->ngot * 2, INSTR_TRI, 3);
            if (S->ngot == NSTAR) { S->win = true; strum(72, CHORD_MAJ, INSTR_TRI, 4, 40); }
        }
    }

    for (int i = 0; i < MAXP; i++)
        if (S->puff[i].life > 0) {
            S->puff[i].x += S->puff[i].vx; S->puff[i].y += S->puff[i].vy;
            S->puff[i].life--;
        }
}

void draw(void) {
    cls(CLR_BLACK);

    // a sparse starfield (fixed, cheap)
    for (int i = 0; i < 40; i++)
        pset((i * 53) % SCREEN_W, (i * 97 + 30) % SCREEN_H, (i % 3) ? CLR_DARK_GREY : CLR_LIGHT_GREY);

    for (int i = 0; i < NSTAR; i++) {
        if (S->got[i]) continue;
        int x = star_pos[i][0], y = star_pos[i][1];
        circfill(x, y, 3, blink(20) ? CLR_YELLOW : CLR_LIGHT_YELLOW);
        pset(x, y - 5, CLR_YELLOW); pset(x, y + 5, CLR_YELLOW);
        pset(x - 5, y, CLR_YELLOW); pset(x + 5, y, CLR_YELLOW);
    }

    for (int i = 0; i < MAXP; i++)
        if (S->puff[i].life > 0)
            pset((int)S->puff[i].x, (int)S->puff[i].y,
                 S->puff[i].life > 8 ? CLR_ORANGE : CLR_DARK_GREY);

    // the ship: a triangle built from the facing angle
    int nx = (int)(S->px + dx(SHIP_R + 2, S->ang)),       ny = (int)(S->py + dy(SHIP_R + 2, S->ang));
    int lx = (int)(S->px + dx(SHIP_R, S->ang + 140)),     ly = (int)(S->py + dy(SHIP_R, S->ang + 140));
    int rx = (int)(S->px + dx(SHIP_R, S->ang - 140)),     ry = (int)(S->py + dy(SHIP_R, S->ang - 140));
    trifill(nx, ny, lx, ly, rx, ry, CLR_WHITE);
    tri(nx, ny, lx, ly, rx, ry, CLR_LIGHT_GREY);

    // velocity vector — the lesson made visible: the green line is where you're
    // GOING; the nose is where you're POINTING. Watch them disagree mid-drift.
    if (S->inertia)
        line((int)S->px, (int)S->py,
             (int)(S->px + S->vx * 12), (int)(S->py + S->vy * 12), CLR_GREEN);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("thrust", 4, 2, CLR_WHITE);
    print(str("stars %d/%d", S->ngot, NSTAR), 110, 2, CLR_YELLOW);
    print(S->inertia ? "inertia ON (X)" : "inertia OFF(X)",
          SCREEN_W - 116, 2, S->inertia ? CLR_GREEN : CLR_RED);

    if (S->win) {
        print_centered("ALL STARS!", SCREEN_W / 2, SCREEN_H / 2 - 6, CLR_YELLOW);
        print_centered("press Z to play again", SCREEN_W / 2, SCREEN_H / 2 + 4, CLR_WHITE);
    }
}
