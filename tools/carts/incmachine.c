/* de:meta
{
  "title": "The Incredible Machine",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "rigid-body",
    "particle-system"
  ],
  "lineage": "Homage to The Incredible Machine (1993); everything reduces to line-segment reflections so one pinball-style reflect routine drives all collisions, with seesaws getting torque and angular velocity on top.",
  "genre": "puzzle",
  "homage": "The Incredible Machine (1993)",
  "description": "A gravity-physics puzzle sandbox in the spirit of The Incredible Machine: a red ball must fall into the GOAL bucket, and it's on you to build the contraption. Drag ramps, see-saws, fans, walls and spare bumper-balls out of the parts tray onto the board, then hit RUN and watch gravity take over — the ball reflects off your bars pinball-style, fans blow it upward in animated wind arcs, see-saws tip toward the weight, and hard bounces spark particles and ding the synth. Two hand-built levels with fixed pegs and ledges; solve both for the victory screen. Left-drag parts from the tray onto the board (right-click a placed part to lift it), R to run / reset the ball, C to clear the board.",
  "todo": [
    "Make the R and C buttons cute, touch-clickable pixel buttons.",
    "Add a little blip sfx when the ball hits the ground."
  ]
}
de:meta */
// THE INCREDIBLE MACHINE — a gravity-physics puzzle sandbox.
// Drag parts from the tray onto the board, hit R to RUN, and route the ball
// into the bucket. Every part reduces to line-segments (and maybe a force
// zone), so one pinball-style reflect routine drives all collisions. 2 levels.
//
// Controls: left-drag a tray part onto the board, right-click a placed part to
// lift it, R = run / reset (parts stay), C = clear placed parts.

#include <stddef.h>
#include "studio.h"

// ---------------------------------------------------------------- tuning
#define BALL_R       4
#define GRAV         420.0f
#define MAXSPD       560.0f
#define REST_WALL    0.42f
#define SUBSTEPS     6
#define NPART        48
#define MAX_PLACED   24
#define DEG2RAD      0.0174532925f

#define TRAY_X       0
#define TRAY_W       52        // left tray column
#define BOARD_X      TRAY_W
#define BUCKET_W     30
#define BUCKET_H     18

// part kinds (tray order)
enum { P_RAMP = 0, P_SEESAW, P_FAN, P_WALL, P_BALL, N_KINDS };

// ---------------------------------------------------------------- types
typedef struct { float x, y, vx, vy; bool alive; } Ball;
typedef struct {
    int   kind;
    float x, y;        // center on board
    float ang;         // seesaw live angle (deg); ramp uses fixed slope
    float vang;        // seesaw angular velocity
} Placed;
typedef struct { float x, y, vx, vy, life, maxlife; int col; } Part;

// ---------------------------------------------------------------- state
static int   gstate;                 // 0 build, 1 running, 2 won-level, 3 won-game
static int   level;
static bool  running;                // physics live?
static Ball  ball;
static float spawnx, spawny;
static int   bucketx, buckety;       // bucket top-left
static float winTimer;
static int   moves;                  // parts dropped (a soft score)

static Placed placed[MAX_PLACED];
static int    nplaced;

// fixed level geometry as segments
typedef struct { float x1, y1, x2, y2; } LSeg;
#define MAX_LSEG 24
static LSeg lsegs[MAX_LSEG];
static int  nlseg;

static Part  parts[NPART];

// drag state
static int   dragging = -1;          // part kind being dragged, or -1
static float winCelebrate;           // chord cooldown

static const char *KIND_NAME[N_KINDS] = { "RAMP", "SEESAW", "FAN", "WALL", "BALL" };

// ---------------------------------------------------------------- helpers
static void add_lseg(float x1, float y1, float x2, float y2) {
    if (nlseg >= MAX_LSEG) return;
    lsegs[nlseg++] = (LSeg){ x1, y1, x2, y2 };
}

static void spawn_part(float x, float y, float vx, float vy, float life, int col) {
    for (int i = 0; i < NPART; i++) {
        if (parts[i].life <= 0) {
            parts[i] = (Part){ x, y, vx, vy, life, life, col };
            return;
        }
    }
}

// pinball segment reflect: pushes ball out + reflects velocity. returns impact speed.
static float reflect_seg(Ball *b, float x1, float y1, float x2, float y2, float e) {
    float ex = x2 - x1, ey = y2 - y1;
    float L2 = ex * ex + ey * ey;
    float t = 0;
    if (L2 > 0) t = ((b->x - x1) * ex + (b->y - y1) * ey) / L2;
    t = clamp(t, 0, 1);
    float cx = x1 + ex * t, cy = y1 + ey * t;
    float ndx = b->x - cx, ndy = b->y - cy;
    float d = fsqrt(ndx * ndx + ndy * ndy);
    if (d > BALL_R) return -1;
    float nx, ny;
    if (d > 0.0001f) { nx = ndx / d; ny = ndy / d; } else { nx = 0; ny = -1; }
    b->x += nx * (BALL_R - d);
    b->y += ny * (BALL_R - d);
    float vn = b->vx * nx + b->vy * ny;
    if (vn < 0) {
        b->vx -= (1 + e) * vn * nx;
        b->vy -= (1 + e) * vn * ny;
        return -vn;     // impact speed (positive)
    }
    return -1;
}

// the two endpoints of a placed part's reflecting segment (in board space)
static void part_segment(Placed *p, float *x1, float *y1, float *x2, float *y2) {
    float half = 18.0f;
    if (p->kind == P_WALL) {
        *x1 = p->x; *y1 = p->y - half; *x2 = p->x; *y2 = p->y + half;
    } else { // ramp + seesaw: a tilted bar
        float a = (p->kind == P_SEESAW) ? p->ang : -22.0f; // ramp fixed slope
        *x1 = p->x - cos_deg(a) * half; *y1 = p->y - sin_deg(a) * half;
        *x2 = p->x + cos_deg(a) * half; *y2 = p->y + sin_deg(a) * half;
    }
}

// ---------------------------------------------------------------- levels
static void load_level(int n) {
    nlseg = 0;
    int top = 22; // below HUD
    // board border (left wall is the tray edge, ball can't escape into tray)
    add_lseg(BOARD_X, top, SCREEN_W, top);                 // ceiling
    add_lseg(BOARD_X, top, BOARD_X, SCREEN_H);             // left wall
    add_lseg(SCREEN_W - 1, top, SCREEN_W - 1, SCREEN_H);   // right wall

    if (n == 0) {
        spawnx = BOARD_X + 24; spawny = top + 10;
        // a couple of fixed pegs/ledges to make routing interesting
        add_lseg(BOARD_X + 60, 70, 150, 90);   // ledge slanting right
        add_lseg(210, 120, 280, 140);          // lower-right ledge
        bucketx = 150; buckety = SCREEN_H - BUCKET_H - 4;
    } else {
        spawnx = SCREEN_W - 24; spawny = top + 10;
        add_lseg(180, 64, 250, 64);            // flat shelf upper-right
        add_lseg(BOARD_X + 30, 110, 120, 130); // upper-left slope
        add_lseg(140, 150, 220, 150);          // mid floating bar
        bucketx = BOARD_X + 18; buckety = SCREEN_H - BUCKET_H - 4;
    }
}

static void reset_ball(void) {
    ball = (Ball){ spawnx, spawny, 0, 0, true };
    running = false;
    gstate = 0;
    winTimer = 0;
    // re-level the seesaws
    for (int i = 0; i < nplaced; i++)
        if (placed[i].kind == P_SEESAW) { placed[i].ang = 0; placed[i].vang = 0; }
}

static void new_level(int n) {
    level = n;
    nplaced = 0;
    moves = 0;
    for (int i = 0; i < NPART; i++) parts[i].life = 0;
    load_level(n);
    reset_ball();
}

// ---------------------------------------------------------------- physics
static void step_ball(float sdt) {
    Ball *b = &ball;
    if (!b->alive) return;
    b->vy += GRAV * sdt;

    // fan force zones: push the ball away (upward bias) when nearby
    for (int i = 0; i < nplaced; i++) {
        if (placed[i].kind != P_FAN) continue;
        float fdx = b->x - placed[i].x, fdy = b->y - placed[i].y;
        float dd = fsqrt(fdx * fdx + fdy * fdy);
        if (dd < 46 && dd > 0.001f) {
            float strength = (1.0f - dd / 46.0f) * 900.0f;
            b->vx += (fdx / dd) * strength * sdt;
            b->vy += ((fdy / dd) - 0.7f) * strength * sdt; // bias up
        }
    }

    float sp = fsqrt(b->vx * b->vx + b->vy * b->vy);
    if (sp > MAXSPD) { b->vx = b->vx / sp * MAXSPD; b->vy = b->vy / sp * MAXSPD; }

    b->x += b->vx * sdt;
    b->y += b->vy * sdt;

    // fixed geometry
    for (int i = 0; i < nlseg; i++) {
        float s = reflect_seg(b, lsegs[i].x1, lsegs[i].y1, lsegs[i].x2, lsegs[i].y2, REST_WALL);
        if (s > 60) {
            shake(clamp(s / 200.0f, 0, 4));
            note(degree(SCALE_PENTA, 4, rnd(4)), 7, clamp(s / 90.0f, 1, 4));
            for (int k = 0; k < 4; k++)
                spawn_part(b->x, b->y, rnd_float_between(-60, 60), rnd_float_between(-80, 20),
                           0.3f, CLR_LIGHT_GREY);
        }
    }

    // placed parts
    for (int i = 0; i < nplaced; i++) {
        Placed *p = &placed[i];
        if (p->kind == P_FAN || p->kind == P_BALL) continue;
        float x1, y1, x2, y2;
        part_segment(p, &x1, &y1, &x2, &y2);
        float s = reflect_seg(&ball, x1, y1, x2, y2, REST_WALL);
        if (s >= 0) {
            // seesaw: torque from where the ball struck relative to pivot
            if (p->kind == P_SEESAW) {
                float side = (ball.x - p->x);
                p->vang += clamp(side, -14, 14) * 1.4f;
            }
            if (s > 60) {
                shake(clamp(s / 220.0f, 0, 3));
                note(degree(SCALE_PENTA, 5, rnd(4)), 8, clamp(s / 100.0f, 1, 4));
                for (int k = 0; k < 4; k++)
                    spawn_part(ball.x, ball.y, rnd_float_between(-50, 50),
                               rnd_float_between(-70, 10), 0.3f, CLR_YELLOW);
            }
        }
    }

    // "ball" parts act as round bumpers (extra obstacles you can drop)
    for (int i = 0; i < nplaced; i++) {
        if (placed[i].kind != P_BALL) continue;
        float ndx = ball.x - placed[i].x, ndy = ball.y - placed[i].y;
        float d = fsqrt(ndx * ndx + ndy * ndy);
        float rr = BALL_R + 6;
        if (d < rr && d > 0.001f) {
            float nx = ndx / d, ny = ndy / d;
            ball.x += nx * (rr - d); ball.y += ny * (rr - d);
            float vn = ball.vx * nx + ball.vy * ny;
            if (vn < 0) { ball.vx -= (1 + 0.55f) * vn * nx; ball.vy -= (1 + 0.55f) * vn * ny; }
        }
    }
}

// ---------------------------------------------------------------- placement
static int tray_cell_at(int my) {
    // each kind gets a 36px tall cell starting at y=26
    int idx = (my - 26) / 36;
    if (idx < 0 || idx >= N_KINDS) return -1;
    return idx;
}

static int placed_at(int mx, int my) {
    for (int i = nplaced - 1; i >= 0; i--)
        if (near(mx, my, (int)placed[i].x, (int)placed[i].y, 16)) return i;
    return -1;
}

static void drop_part(int kind, int x, int y) {
    if (nplaced >= MAX_PLACED) return;
    if (x < BOARD_X + 14) x = BOARD_X + 14;
    if (x > SCREEN_W - 14) x = SCREEN_W - 14;
    if (y < 30) y = 30;
    if (y > SCREEN_H - 14) y = SCREEN_H - 14;
    placed[nplaced++] = (Placed){ kind, (float)x, (float)y, 0, 0 };
    moves++;
    hit(50, 6, 3, 40);
}

// ---------------------------------------------------------------- init
void init(void) {
    bpm(120);
    instrument(6, INSTR_NOISE,  1, 30, 0, 40);     // click / clack
    instrument(7, INSTR_SINE,   2, 60, 0, 90);     // wall ding
    instrument(8, INSTR_TRI,    2, 70, 0, 120);    // part ding
    instrument(9, INSTR_SQUARE, 3, 80, 4, 220);    // win arp
    instrument(10, INSTR_NOISE, 30, 200, 4, 300);  // fan whoosh
    instrument_filter(10, FILTER_BAND, 1200, 8);
    new_level(0);
}

// ---------------------------------------------------------------- update
void update(void) {
    float d = dt();
    if (winCelebrate > 0) winCelebrate -= d;

    // particles
    for (int i = 0; i < NPART; i++) {
        if (parts[i].life <= 0) continue;
        parts[i].vy   += 300 * d;
        parts[i].x    += parts[i].vx * d;
        parts[i].y    += parts[i].vy * d;
        parts[i].life -= d;
    }

    // fan whoosh while running
    if (running && gstate == 1) {
        for (int i = 0; i < nplaced; i++)
            if (placed[i].kind == P_FAN && every(1) && chance(50)) { hit(40, 10, 1, 90); break; }
    }

    if (gstate == 3) { // game complete
        if (key('R') || keyp(KEY_SPACE)) new_level(0);
        return;
    }

    if (gstate == 2) { // level cleared, advancing
        winTimer -= d;
        if (winCelebrate <= 0) {
            // rising arp
            static int arp; arp = (arp + 1) % 4;
            schedule(0, degree(SCALE_MAJOR, 5, arp), 9, 5);
            winCelebrate = 0.12f;
        }
        if (winTimer <= 0) {
            if (level >= 1) gstate = 3;
            else new_level(level + 1);
        }
        return;
    }

    // ----- build / run controls -----
    if (keyp('R')) {
        if (running) { reset_ball(); note(36, 8, 4); }       // reset
        else { running = true; gstate = 1; ball.alive = true; note(60, 8, 4); }
        return;
    }
    if (keyp('C') && !running) {
        nplaced = 0; moves = 0; note(34, 6, 3);
        return;
    }

    // mouse: drag from tray, right-click to lift, only while building
    int mx = mouse_x(), my = mouse_y();
    if (!running) {
        // start a drag from the tray
        if (mouse_pressed(MOUSE_LEFT)) {
            if (mx < TRAY_W) {
                int c = tray_cell_at(my);
                if (c >= 0) dragging = c;
            } else {
                // grab an already-placed part to reposition it
                int pi = placed_at(mx, my);
                if (pi >= 0) {
                    dragging = placed[pi].kind;
                    // remove it; it re-drops on release
                    for (int j = pi; j < nplaced - 1; j++) placed[j] = placed[j + 1];
                    nplaced--; moves--;
                }
            }
        }
        if (mouse_released(MOUSE_LEFT) && dragging >= 0) {
            if (mx >= BOARD_X) drop_part(dragging, mx, my);
            dragging = -1;
        }
        // right-click a placed part to delete it
        if (mouse_pressed(MOUSE_RIGHT)) {
            int pi = placed_at(mx, my);
            if (pi >= 0) {
                for (int j = pi; j < nplaced - 1; j++) placed[j] = placed[j + 1];
                nplaced--; if (moves > 0) moves--;
                hit(44, 6, 3, 40);
            }
        }
    }

    // ----- physics -----
    if (running && gstate == 1) {
        for (int s = 0; s < SUBSTEPS; s++) {
            float sdt = d / SUBSTEPS;
            // seesaw integration (gravity restores toward level, damped)
            for (int i = 0; i < nplaced; i++) {
                if (placed[i].kind != P_SEESAW) continue;
                placed[i].vang += -placed[i].ang * 6.0f * sdt; // spring to 0
                placed[i].vang *= 0.985f;
                placed[i].ang  += placed[i].vang * sdt;
                placed[i].ang   = clamp(placed[i].ang, -40, 40);
            }
            step_ball(sdt);
        }

        // win check: ball inside bucket mouth, moving slowly
        if (ball.x > bucketx + 3 && ball.x < bucketx + BUCKET_W - 3 &&
            ball.y > buckety && ball.y < buckety + BUCKET_H) {
            ball.alive = false;
            gstate = 2;
            running = false;
            winTimer = 1.4f;
            winCelebrate = 0;
            shake(5);
            for (int k = 0; k < 16; k++)
                spawn_part(ball.x, buckety, rnd_float_between(-90, 90),
                           rnd_float_between(-180, -40), 0.7f,
                           (k & 1) ? CLR_YELLOW : CLR_GREEN);
        }
        // off the bottom (missed) — just let it rest at the floor; reflect floor
        if (ball.y > SCREEN_H + 30) { ball.y = SCREEN_H - BALL_R; ball.vy = 0; }
        if (ball.y > SCREEN_H - BALL_R) {
            ball.y = SCREEN_H - BALL_R;
            if (ball.vy > 0) ball.vy = -ball.vy * 0.3f;
            ball.vx *= 0.96f;
        }
    }
}

// ---------------------------------------------------------------- drawing
static void draw_part_icon(int kind, int x, int y, bool ghost) {
    int c = ghost ? CLR_LIGHT_GREY : CLR_WHITE;
    switch (kind) {
        case P_RAMP: {
            float a = -22.0f, h = 14;
            line(x - cos_deg(a) * h, y - sin_deg(a) * h,
                 x + cos_deg(a) * h, y + sin_deg(a) * h, CLR_ORANGE);
            line(x - cos_deg(a) * h, y - sin_deg(a) * h + 1,
                 x + cos_deg(a) * h, y + sin_deg(a) * h + 1, CLR_BROWN);
        } break;
        case P_SEESAW: {
            line(x - 14, y, x + 14, y, CLR_BLUE);
            tri(x, y + 2, x - 4, y + 8, x + 4, y + 8, CLR_DARK_GREY);
        } break;
        case P_FAN: {
            circ(x, y, 8, CLR_GREEN);
            for (int k = 0; k < 3; k++) {
                float a = k * 120 + (ghost ? 0 : now() * 200);
                line(x, y, x + dx(7, a), y + dy(7, a), CLR_LIME_GREEN);
            }
        } break;
        case P_WALL: {
            line(x, y - 14, x, y + 14, CLR_INDIGO);
            line(x + 1, y - 14, x + 1, y + 14, CLR_MAUVE);
        } break;
        case P_BALL: {
            circfill(x, y, 6, CLR_LIGHT_GREY);
            circ(x, y, 6, CLR_WHITE);
        } break;
    }
    (void)c;
}

static void draw_placed(Placed *p) {
    if (p->kind == P_FAN) {
        // wind arcs when running
        if (running) {
            for (int k = 0; k < 3; k++) {
                float a = k * 120 + now() * 300;
                arc((int)p->x, (int)p->y, 20 + k * 8, a, a + 60, CLR_LIME_GREEN);
            }
        }
        circfill((int)p->x, (int)p->y, 9, CLR_DARK_GREEN);
        for (int k = 0; k < 3; k++) {
            float a = k * 120 + now() * 360;
            line((int)p->x, (int)p->y, (int)(p->x + dx(8, a)), (int)(p->y + dy(8, a)), CLR_LIME_GREEN);
        }
        circ((int)p->x, (int)p->y, 9, CLR_GREEN);
        return;
    }
    if (p->kind == P_BALL) {
        circfill((int)p->x, (int)p->y, 6, CLR_MEDIUM_GREY);
        circfill((int)p->x + 1, (int)p->y + 1, 4, CLR_LIGHT_GREY);
        circ((int)p->x, (int)p->y, 6, CLR_WHITE);
        return;
    }
    float x1, y1, x2, y2;
    part_segment(p, &x1, &y1, &x2, &y2);
    int col = (p->kind == P_WALL) ? CLR_INDIGO : (p->kind == P_SEESAW) ? CLR_BLUE : CLR_ORANGE;
    line((int)x1, (int)y1, (int)x2, (int)y2, col);
    line((int)x1, (int)y1 + 1, (int)x2, (int)y2 + 1, col);
    if (p->kind == P_SEESAW)
        tri((int)p->x, (int)p->y + 2, (int)p->x - 4, (int)p->y + 9, (int)p->x + 4, (int)p->y + 9, CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // board background grid
    for (int gx = BOARD_X; gx < SCREEN_W; gx += 16)
        line(gx, 22, gx, SCREEN_H, CLR_DARK_BLUE);
    for (int gy = 22; gy < SCREEN_H; gy += 16)
        line(BOARD_X, gy, SCREEN_W, gy, CLR_DARK_BLUE);

    // fixed geometry
    for (int i = 0; i < nlseg; i++)
        line((int)lsegs[i].x1, (int)lsegs[i].y1, (int)lsegs[i].x2, (int)lsegs[i].y2, CLR_LIGHT_GREY);

    // spawn marker
    if (!running || gstate != 1) {
        circ((int)spawnx, (int)spawny, BALL_R + 2, CLR_DARK_GREY);
        print("S", (int)spawnx - 2, (int)spawny - 12, CLR_DARK_GREY);
    }

    // bucket
    rectfill(bucketx, buckety, BUCKET_W, BUCKET_H, CLR_DARK_GREEN);
    rect(bucketx, buckety, BUCKET_W, BUCKET_H, gstate == 2 && blink(4) ? CLR_YELLOW : CLR_GREEN);
    line(bucketx, buckety, bucketx + 4, buckety + BUCKET_H, CLR_GREEN);
    line(bucketx + BUCKET_W, buckety, bucketx + BUCKET_W - 4, buckety + BUCKET_H, CLR_GREEN);
    print("GOAL", bucketx + 4, buckety - 9, CLR_GREEN);

    // placed parts
    for (int i = 0; i < nplaced; i++) draw_placed(&placed[i]);

    // particles
    for (int i = 0; i < NPART; i++) {
        if (parts[i].life <= 0) continue;
        float t = parts[i].life / parts[i].maxlife;
        if (t > 0.5f) circfill((int)parts[i].x, (int)parts[i].y, 2, parts[i].col);
        else          pset((int)parts[i].x, (int)parts[i].y, parts[i].col);
    }

    // ball
    if (ball.alive || gstate == 1) {
        for (int k = 1; k <= 2; k++)
            circfill((int)(ball.x - ball.vx * 0.006f * k), (int)(ball.y - ball.vy * 0.006f * k),
                     BALL_R - 1, CLR_DARK_GREY);
        circfill((int)ball.x, (int)ball.y, BALL_R, CLR_RED);
        circfill((int)ball.x + 1, (int)ball.y + 1, BALL_R - 2, CLR_DARK_RED);
        pset((int)ball.x - 1, (int)ball.y - 1, CLR_WHITE);
    }

    // ----- tray -----
    rectfill(TRAY_X, 0, TRAY_W, SCREEN_H, CLR_BLACK);
    line(TRAY_W, 0, TRAY_W, SCREEN_H, CLR_DARK_GREY);
    print("PARTS", 6, 4, CLR_YELLOW);
    for (int i = 0; i < N_KINDS; i++) {
        int cy = 26 + i * 36;
        rect(4, cy, TRAY_W - 8, 32, CLR_DARK_GREY);
        draw_part_icon(i, TRAY_W / 2, cy + 13, false);
        print(KIND_NAME[i], 6, cy + 24, CLR_LIGHT_GREY);
    }

    // ghost while dragging
    if (dragging >= 0) {
        int mx = mouse_x(), my = mouse_y();
        if (mx >= BOARD_X) draw_part_icon(dragging, mx, my, true);
    }

    // HUD strip across the board top
    rectfill(BOARD_X, 0, SCREEN_W - BOARD_X, 18, CLR_BLACK);
    line(BOARD_X, 18, SCREEN_W, 18, CLR_DARK_GREY);
    print(str("LEVEL %d/2", level + 1), BOARD_X + 4, 4, CLR_WHITE);
    print(str("PARTS %d", nplaced), BOARD_X + 90, 4, CLR_LIGHT_GREY);
    print_right(running ? "R:RESET" : "R:RUN  C:CLEAR", SCREEN_W - 4, 4,
                running ? CLR_GREEN : CLR_YELLOW);

    // overlays
    if (gstate == 2)
        print_centered("IN THE BUCKET!", SCREEN_W/2, SCREEN_H / 2 - 4, blink(4) ? CLR_YELLOW : CLR_GREEN);

    if (gstate == 3) {
        fade(0.5f);
        rectfill(60, 70, 200, 70, CLR_DARK_BLUE);
        rect(60, 70, 200, 70, CLR_YELLOW);
        print_centered("MACHINE COMPLETE!", SCREEN_W/2, 84, CLR_WHITE);
        print_centered("both contraptions solved", SCREEN_W/2, 100, CLR_LIGHT_GREY);
        if (blink(20)) print_centered("R  TO  PLAY  AGAIN", SCREEN_W/2, 118, CLR_GREEN);
    }

    if (gstate == 0 && nplaced == 0)
        print_centered("drag parts in, then press R to run", SCREEN_W/2, SCREEN_H - 12, CLR_DARK_GREY);
}
