/* de:meta
{
  "title": "Jumpstar",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "camera-follow",
    "parallax",
    "particle-system",
    "screen-shake-juice"
  ],
  "lineage": "Doodle Jump (2009) homage; adds crumbling and moving platform types, a parallax starfield, squash-on-bounce juice, and save-load best score — all layered on top of the standard endless-jumper loop.",
  "genre": "platformer",
  "homage": "Doodle Jump (2009)",
  "description": "An endless vertical jumper in the Doodle-Jump grain: a little astronaut auto-bounces forever off procedurally spawned platforms while you steer to keep landing on the next ledge up. Green platforms are solid, blue ones slide and bounce at the edges, and cracked brown ones crumble away after a single bounce, so dawdling drops you. Grab bobbing stars for a sparkly bonus, ride the climbing camera through a parallax starfield, and don't fall off the bottom — every landing squashes the astronaut, shatters fling debris, and the screen kicks on a break. Height is your score and your best run is saved between sessions. Steer with A/D or Left/Right (wraps at the screen edges); jumping is automatic; press Z to jump again after a fall.",
  "todo": [
    "The end-fade issue (recurring across carts) — apply the shared end-state treatment."
  ]
}
de:meta */
#include "studio.h"

// JUMPSTAR — endless vertical jumper (Doodle-Jump grain).
// You auto-bounce off platforms forever; steer Left/Right (wraps at edges).
// Land on the next platform up, grab stars, don't fall off the bottom.
// Some platforms move, some crumble after one bounce. Height = score. Best is saved.
//
// Controls: A/D or Left/Right steer (wrap). Jump is automatic. Z/A restart.

#define N_PLAT     14
#define N_STAR     10
#define N_DUST     48

#define GRAV       900.0f     // px/s^2 (world units)
#define JUMP_VY   -360.0f     // upward impulse on landing
#define MOVE_SPD   135.0f     // horizontal steer speed
#define REACH_X    64.0f      // max horizontal offset between stacked platforms
                              // (one bounce covers ~90px sideways; keep gaps makeable)
#define PLAT_H     5
#define PW         (R*2)

#define R          5.0f       // astronaut "radius"-ish
#define SQ_TIME    0.18f      // squash duration (seconds)

// platform types
enum { P_NORMAL, P_MOVING, P_BREAK };

typedef struct {
    float x, y, w;     // world-space (y grows down)
    int   type;
    float vx;          // for moving
    bool  broken;      // breakable already used → crumbling
    float fall_vy;     // crumble fall speed
} Plat;

typedef struct { float x, y, phase; bool alive; } Star;
typedef struct { float x, y, vx, vy, life; int col; } Dust;

static float px, py, vx, vy;      // player (world space)
static int   facing;              // -1 / +1
static float squash;              // 0..SQ_TIME timer
static float cam_y;               // world-space top of view
static float highest_spawn_y;     // smallest (highest) platform y so far
static float last_spawn_cx;       // center-x of the most recently spawned platform
static int   score, best, star_bonus;
static bool  dead;
static float death_t;
static float shk;

static Plat plats[N_PLAT];
static Star stars[N_STAR];
static Dust dust[N_DUST];

// ----- helpers -----

static void spawn_dust(float x, float y, int col, int n) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < N_DUST; i++)
            if (dust[i].life <= 0) {
                dust[i] = (Dust){ x, y,
                    rnd_float_between(-90, 90), rnd_float_between(-120, 20),
                    rnd_float_between(0.3f, 0.7f), col };
                break;
            }
}

// pick a platform type weighted by how high we've climbed
static int pick_type(void) {
    int climb = -(int)highest_spawn_y;   // bigger = higher
    int r = rnd(100);
    if (climb > 1500 && r < 22) return P_BREAK;
    if (climb > 700  && r < 18) return P_BREAK;
    if (climb > 400  && r < 30) return P_MOVING;
    if (r < 18) return P_MOVING;
    return P_NORMAL;
}

// place platform p as a fresh one above highest_spawn_y
static void respawn_plat(int i) {
    float gap = rnd_float_between(34, 52);
    highest_spawn_y -= gap;
    Plat *p = &plats[i];
    p->y = highest_spawn_y;
    p->w = rnd_float_between(36, 62);
    // place within a single jump's horizontal reach of the platform below,
    // then keep it fully on screen (clamping only shortens the gap)
    float half = p->w * 0.5f;
    float cx = rnd_float_between(last_spawn_cx - REACH_X, last_spawn_cx + REACH_X);
    if (cx < half)             cx = half;
    if (cx > SCREEN_W - half)  cx = SCREEN_W - half;
    p->x = cx - half;
    last_spawn_cx = cx;
    p->type = pick_type();
    p->broken = false;
    p->fall_vy = 0;
    p->vx = (p->type == P_MOVING) ? (rnd(2) ? 1 : -1) * rnd_float_between(35, 70) : 0;

    // maybe drop a star above this platform
    for (int s = 0; s < N_STAR; s++) {
        if (!stars[s].alive && chance(28)) {
            stars[s].alive = true;
            stars[s].x = p->x + p->w * 0.5f;
            stars[s].y = p->y - rnd_float_between(12, 22);
            stars[s].phase = rnd_float_between(0, 360);
            break;
        }
    }
}

static void start_run(void) {
    cam_y = 0;
    highest_spawn_y = 0;
    score = 0; star_bonus = 0;
    dead = false; death_t = 0; shk = 0;
    facing = 1; squash = 0;

    for (int i = 0; i < N_STAR; i++) stars[i].alive = false;
    for (int i = 0; i < N_DUST; i++) dust[i].life = 0;

    // guaranteed solid floor platform near the bottom
    plats[0] = (Plat){ SCREEN_W/2 - 28, SCREEN_H - 24, 56, P_NORMAL, 0, false, 0 };
    highest_spawn_y = plats[0].y;
    last_spawn_cx = SCREEN_W / 2;   // start the ladder from the floor platform's center
    for (int i = 1; i < N_PLAT; i++) respawn_plat(i);

    // start standing on the floor
    px = SCREEN_W/2; py = plats[0].y - R; vx = 0; vy = 1;
}

void init(void) {
    best = load(0);
    start_run();
    touch_layout(TOUCH_ANALOG, 1);   // jumping is automatic; A is only "jump again" after a fall
}

void update(void) {
    float t = dt();
    if (shk > 0) shk -= shk * 6.0f * t;

    // dust always animates
    for (int i = 0; i < N_DUST; i++)
        if (dust[i].life > 0) {
            dust[i].x += dust[i].vx * t;
            dust[i].y += dust[i].vy * t;
            dust[i].vy += GRAV * 0.5f * t;
            dust[i].life -= t;
        }

    if (dead) {
        death_t += t;
        if (death_t > 0.4f && (btnp(0, BTN_A) || btnp(0, BTN_B) || keyp(KEY_SPACE)))
            start_run();
        return;
    }

    if (squash > 0) squash -= t;

    // steer
    float ax = 0;
    if (btn(0, BTN_LEFT))  ax -= 1;
    if (btn(0, BTN_RIGHT)) ax += 1;
    if (ax != 0) facing = (ax < 0) ? -1 : 1;
    vx = ax * MOVE_SPD;
    px += vx * t;
    // wrap
    if (px < 0)        px += SCREEN_W;
    if (px >= SCREEN_W) px -= SCREEN_W;

    // gravity
    vy += GRAV * t;
    py += vy * t;

    // move/crumble platforms
    for (int i = 0; i < N_PLAT; i++) {
        Plat *p = &plats[i];
        if (p->type == P_MOVING && !p->broken) {
            p->x += p->vx * t;
            if (p->x < 0)            { p->x = 0; p->vx = -p->vx; }
            if (p->x > SCREEN_W - p->w) { p->x = SCREEN_W - p->w; p->vx = -p->vx; }
        }
        if (p->broken) p->fall_vy += GRAV * t, p->y += p->fall_vy * t;
    }

    // landing — only when falling down, feet crossing the platform top
    if (vy > 0) {
        float feet = py + R;
        for (int i = 0; i < N_PLAT; i++) {
            Plat *p = &plats[i];
            if (p->broken) continue;
            float top = p->y;
            if (feet >= top && feet <= top + PLAT_H + 8
                && px >= p->x - 2 && px <= p->x + p->w + 2) {
                // bounce!
                py = top - R;
                vy = JUMP_VY;
                squash = SQ_TIME;
                note(60 + rnd(4), INSTR_SINE, 3);
                if (p->type == P_BREAK) {
                    p->broken = true;
                    p->fall_vy = 40;
                    spawn_dust(p->x + p->w*0.5f, top, CLR_BROWN, 8);
                    hit(40, INSTR_NOISE, 4, 80);
                    shk = 3.5f;
                }
                break;
            }
        }
    }

    // collect stars
    for (int i = 0; i < N_STAR; i++) {
        if (!stars[i].alive) continue;
        if (near((int)px, (int)py, (int)stars[i].x, (int)stars[i].y, 9)) {
            stars[i].alive = false;
            star_bonus += 50;
            spawn_dust(stars[i].x, stars[i].y, CLR_YELLOW, 6);
            note(84, INSTR_SINE, 4);
            note(88, INSTR_SINE, 3);
        }
    }

    // camera follows up (only ever climbs)
    float target = py - SCREEN_H * 0.45f;
    if (target < cam_y) cam_y = target;

    // height score (climb in "metres")
    int climb = (int)(-cam_y / 8);
    if (climb > score) score = climb;

    // recycle platforms that fell below the view, and any star far below
    float bottom = cam_y + SCREEN_H + 20;
    for (int i = 0; i < N_PLAT; i++)
        if (plats[i].y > bottom) respawn_plat(i);
    for (int i = 0; i < N_STAR; i++)
        if (stars[i].alive && stars[i].y > bottom) stars[i].alive = false;
    for (int i = 0; i < N_STAR; i++) stars[i].phase += 180 * t;

    // death — fell below the bottom edge
    if (py - R > cam_y + SCREEN_H) {
        dead = true; death_t = 0;
        int total = score + star_bonus;
        if (total > best) { best = total; save(0, best); }
        note(48, INSTR_SAW, 4);
        schedule(120, 41, INSTR_SAW, 4);
        shk = 5.0f;
    }

#ifdef DE_TRACE
    watch("score", "%d", score + star_bonus);
    watch("py", "%.0f", py);
    watch("camy", "%.0f", cam_y);
#endif
}

// ----- draw -----

static void draw_astro(int x, int y) {
    // squash: flatten vertically right after a bounce, spring back
    float rx = R, ry = R;
    if (squash > 0) {
        float amt = ease_out(squash / SQ_TIME) * 0.5f;
        rx = R * (1 + amt);
        ry = R * (1 - amt);
    }
    int cy = y + (int)(R - ry);
    // body
    ovalfill(x, cy, (int)rx, (int)ry, CLR_WHITE);
    // helmet visor
    ovalfill(x + facing * 2, cy - 1, (int)(rx*0.45f), (int)(ry*0.45f), CLR_BLUE);
    // little feet flame trail while rising
    if (vy < -40 && !dead) {
        int fy = cy + (int)ry;
        line(x - 2, fy, x - 1, fy + 2 + rnd(2), CLR_ORANGE);
        line(x + 2, fy, x + 1, fy + 2 + rnd(2), CLR_YELLOW);
    }
}

static void draw_plat(Plat *p, int sy) {
    int x = (int)p->x, w = (int)p->w;
    if (p->type == P_BREAK) {
        if (p->broken) {
            // crumbling: two halves drifting
            rectfill(x, sy, w/2 - 2, PLAT_H, CLR_DARK_BROWN);
            rectfill(x + w/2 + 2, sy, w/2 - 2, PLAT_H, CLR_DARK_BROWN);
        } else {
            rectfill(x, sy, w, PLAT_H, CLR_BROWN);
            // cracks
            line(x + w/3, sy, x + w/3 - 1, sy + PLAT_H, CLR_DARK_BROWN);
            line(x + 2*w/3, sy, x + 2*w/3 + 1, sy + PLAT_H, CLR_DARK_BROWN);
        }
    } else if (p->type == P_MOVING) {
        rectfill(x, sy, w, PLAT_H, CLR_BLUE);
        rectfill(x, sy, w, 1, CLR_LIGHT_GREY);
        // arrow hint
        int mx = x + w/2;
        int d = (p->vx > 0) ? 1 : -1;
        line(mx, sy + 2, mx + d*3, sy + 2, CLR_WHITE);
    } else {
        rectfill(x, sy, w, PLAT_H, CLR_GREEN);
        rectfill(x, sy, w, 1, CLR_LIME_GREEN);
    }
}

static void draw_star(Star *s, int sy) {
    int x = (int)s->x;
    int y = sy + (int)(sin_deg(s->phase) * 2.0f);   // gentle bob
    int c = blink(8) ? CLR_WHITE : CLR_YELLOW;
    circfill(x, y, 2, c);
    pset(x - 3, y, CLR_YELLOW); pset(x + 3, y, CLR_YELLOW);
    pset(x, y - 3, CLR_YELLOW); pset(x, y + 3, CLR_YELLOW);
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // screen shake
    int ox = 0, oy = 0;
    if (shk > 0.4f) { ox = (int)rnd_float_between(-shk, shk); oy = (int)rnd_float_between(-shk, shk); }
    camera(ox, oy);

    // parallax starfield (scrolls slower than world)
    for (int i = 0; i < 60; i++) {
        int baseX = (i * 97 + 11) % SCREEN_W;
        int sy = (int)((i * 53 + 7) - cam_y * 0.35f);
        sy = ((sy % SCREEN_H) + SCREEN_H) % SCREEN_H;
        pset(baseX, sy, (i % 6 == 0) ? CLR_WHITE : CLR_DARK_GREY);
    }

    // platforms
    for (int i = 0; i < N_PLAT; i++) {
        int sy = (int)(plats[i].y - cam_y);
        if (sy > -PLAT_H && sy < SCREEN_H + PLAT_H) draw_plat(&plats[i], sy);
    }

    // stars
    for (int i = 0; i < N_STAR; i++)
        if (stars[i].alive) {
            int sy = (int)(stars[i].y - cam_y);
            if (sy > -6 && sy < SCREEN_H + 6) draw_star(&stars[i], sy);
        }

    // dust
    for (int i = 0; i < N_DUST; i++)
        if (dust[i].life > 0)
            pset((int)dust[i].x, (int)(dust[i].y - cam_y), dust[i].col);

    // player
    if (!dead) draw_astro((int)px, (int)(py - cam_y));

    camera(0, 0);

    // death tint
    if (dead) {
        fade(clamp(death_t * 1.2f, 0, 0.55f));
    }

    // HUD
    print(str("%dm", score), 4, 4, CLR_WHITE);
    print_right(str("BEST %d", best), SCREEN_W - 4, 4, CLR_LIGHT_YELLOW);
    if (star_bonus > 0)
        print_centered(str("+%d *", star_bonus), SCREEN_W/2, 4, CLR_YELLOW);

    if (dead) {
        int total = score + star_bonus;
        int by = SCREEN_H/2 - 28;
        rectfill(SCREEN_W/2 - 70, by, 140, 60, CLR_BLACK);
        rect    (SCREEN_W/2 - 70, by, 140, 60, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, by + 8, CLR_RED);
        print_centered(str("HEIGHT %dm", score), SCREEN_W/2, by + 22, CLR_WHITE);
        print_centered(str("STARS  %d", star_bonus), SCREEN_W/2, by + 32, CLR_YELLOW);
        print_centered(str("SCORE  %d", total), SCREEN_W/2, by + 42, CLR_LIGHT_YELLOW);
        if (blink(20))
            print_centered("Z to jump again", SCREEN_W/2, by + 54, CLR_LIGHT_GREY);
    }
}
