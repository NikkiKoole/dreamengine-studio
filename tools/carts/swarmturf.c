/* de:meta
{
  "slug": "swarmturf",
  "title": "Swarmturf",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "flocking",
    "steering-behaviors"
  ],
  "lineage": "Splatoon-inspired territory game built on a boids simulation; novel in making paint coverage the emergent residue of flock flight rather than a stamped fill, so the player steers a pull force rather than individual agents.",
  "genre": "sandbox",
  "description": "Splatoon x boids: you steer a flock that PAINTS the ground it flies over, carving turf out of a rival swarm's color. Hold LEFT-CLICK (or WASD) to pull your swarm toward the cursor; most painted ground when the timer runs out wins. Click / Z to replay."
}
de:meta */
#include "studio.h"
#include <math.h>

// SWARMTURF — Splatoon x boids. Your flock PAINTS the ground it flies over.
//
// Territory here isn't a fill grid you stamp; it's the emergent residue of
// flight. Two flocks roam one arena. Every boid paints the cell beneath it in
// its team color as it crosses — flying over the rival's color OVER-PAINTS it,
// flipping the turf. Coverage is whatever your trails happened to cover.
//
// You don't control a boid; you control a PULL. Hold the mouse (or WASD) and
// your whole swarm is attracted toward that point — sweep it across enemy turf
// to carve trails through their color while they sweep back. The rival flock
// roams and contests on its own, biasing toward the ground it doesn't own yet.
//
// Each flock obeys the three boid urges (separation / alignment / cohesion)
// plus the team pull, so the spread of paint follows the natural curl of the
// swarm. Own the most painted ground when the timer runs out.
//
//   mouse:  hold LEFT to pull your flock toward the cursor (or WASD)
//   most coverage after the round wins — click / Z to replay

#define HUD_H      14
#define CELL        4
#define GW         (SCREEN_W / CELL)         // 80 paint columns
#define GH         (SCREEN_H / CELL)         // 50 paint rows
#define ROW_TOP    (HUD_H / CELL + 1)        // first paintable row (clear of the HUD)
#define CELLS      (GW * GH)
#define ROUND_SEC  45

#define NB         34                        // boids per flock
#define VIEW       22.0f                     // neighbour radius
#define VIEW2      (VIEW * VIEW)
#define SEP_D2     90.0f                     // crowd-too-close radius (squared)
#define MAXSPD     1.9f
#define MINSPD     0.9f

#define NEUTRAL    CLR_DARKER_GREY
// team 0 = YOU (blue), team 1 = RIVAL (orange)
static const int INK   [2] = { CLR_BLUE,      CLR_ORANGE };
static const int INK_HI[2] = { CLR_TRUE_BLUE, CLR_DARK_ORANGE };
static const int BODY  [2] = { CLR_DARK_BLUE, CLR_BROWN };

typedef struct { float x, y, vx, vy; } Boid;
static Boid flock[2][NB];

static unsigned char owner[CELLS];           // 0 neutral, 1 = team0, 2 = team1
static int  cover[2];                        // painted cells per team
static int  paintable;                       // total paintable cells

static bool  started, over, floor_inited;
static float t_end;
static int   best;                           // best coverage % you ever scored

// rival flock's roaming pull target (steered by simple AI)
static float aix, aiy, ai_retarget;

static int idx(int gx, int gy) { return gy * GW + gx; }

// ── paint the cell under a world point in team `who` (0/1) ───────────
static void paint_at(float x, float y, int who) {
    int gx = (int)x / CELL, gy = (int)y / CELL;
    if (gx < 0 || gx >= GW || gy < ROW_TOP || gy >= GH) return;
    owner[idx(gx, gy)] = (unsigned char)(who + 1);
}

static void count_cover(void) {
    cover[0] = cover[1] = 0;
    for (int i = 0; i < CELLS; i++) {
        if      (owner[i] == 1) cover[0]++;
        else if (owner[i] == 2) cover[1]++;
    }
}

// ── one flocking step for team `t` pulled toward (px,py) ─────────────
static void step_flock(int t, float px, float py, float pull) {
    Boid *b = flock[t];
    for (int i = 0; i < NB; i++) {
        float sepx = 0, sepy = 0, alx = 0, aly = 0, cox = 0, coy = 0;
        int n = 0;
        for (int j = 0; j < NB; j++) {
            if (j == i) continue;
            float dxx = b[j].x - b[i].x, dyy = b[j].y - b[i].y;
            float d2 = dxx * dxx + dyy * dyy;
            if (d2 < VIEW2) {
                n++; alx += b[j].vx; aly += b[j].vy; cox += b[j].x; coy += b[j].y;
                if (d2 < SEP_D2) { sepx -= dxx; sepy -= dyy; }
            }
        }
        if (n > 0) {
            alx /= n; aly /= n; cox /= n; coy /= n;
            b[i].vx += (alx - b[i].vx) * 0.06f + (cox - b[i].x) * 0.0010f + sepx * 0.022f;
            b[i].vy += (aly - b[i].vy) * 0.06f + (coy - b[i].y) * 0.0010f + sepy * 0.022f;
        }
        // team pull toward the steer point
        b[i].vx += (px - b[i].x) * pull;
        b[i].vy += (py - b[i].y) * pull;

        // soft-bounce off the play bounds so they stay on the canvas
        if (b[i].x < 4)            b[i].vx += 0.5f;
        if (b[i].x > SCREEN_W - 4) b[i].vx -= 0.5f;
        if (b[i].y < HUD_H + 4)    b[i].vy += 0.5f;
        if (b[i].y > SCREEN_H - 4) b[i].vy -= 0.5f;

        // clamp speed
        float sp = sqrtf(b[i].vx * b[i].vx + b[i].vy * b[i].vy);
        if (sp > MAXSPD)       { b[i].vx = b[i].vx / sp * MAXSPD; b[i].vy = b[i].vy / sp * MAXSPD; }
        else if (sp < MINSPD && sp > 0.01f) { b[i].vx = b[i].vx / sp * MINSPD; b[i].vy = b[i].vy / sp * MINSPD; }

        b[i].x = clamp(b[i].x + b[i].vx, 1, SCREEN_W - 1);
        b[i].y = clamp(b[i].y + b[i].vy, HUD_H + 1, SCREEN_H - 1);

        paint_at(b[i].x, b[i].y, t);
    }
}

// ── rival AI: aim its pull at the ground it owns LEAST ────────────────
static void ai_retarget_now(void) {
    // sample a few random points, pick the one in the most enemy/neutral turf
    float bestx = SCREEN_W / 2, besty = SCREEN_H / 2;
    int   best_score = -1;
    for (int s = 0; s < 8; s++) {
        int sx = rnd_between(8, SCREEN_W - 8);
        int sy = rnd_between(HUD_H + 8, SCREEN_H - 8);
        // count nearby cells NOT owned by the rival (team 1 -> owner 2)
        int gx = sx / CELL, gy = sy / CELL, score = 0;
        for (int oy = -3; oy <= 3; oy++)
            for (int ox = -3; ox <= 3; ox++) {
                int cx = gx + ox, cy = gy + oy;
                if (cx < 0 || cx >= GW || cy < ROW_TOP || cy >= GH) continue;
                if (owner[idx(cx, cy)] != 2) score++;   // neutral or yours = worth taking
            }
        if (score > best_score) { best_score = score; bestx = sx; besty = sy; }
    }
    aix = bestx; aiy = besty;
    ai_retarget = now() + rnd_float_between(1.1f, 2.2f);
}

static void seed_flock(int t, float cx, float cy) {
    for (int i = 0; i < NB; i++) {
        float a = rnd(360);
        flock[t][i].x = clamp(cx + rnd(40) - 20, 6, SCREEN_W - 6);
        flock[t][i].y = clamp(cy + rnd(40) - 20, HUD_H + 6, SCREEN_H - 6);
        flock[t][i].vx = cos_deg(a) * MAXSPD;
        flock[t][i].vy = sin_deg(a) * MAXSPD;
    }
}

static void reset(void) {
    for (int i = 0; i < CELLS; i++) owner[i] = 0;
    paintable = GW * (GH - ROW_TOP);
    seed_flock(0, SCREEN_W * 0.22f, SCREEN_H * 0.7f);
    seed_flock(1, SCREEN_W * 0.78f, SCREEN_H * 0.35f);
    cover[0] = cover[1] = 0;
    started = false; over = false; floor_inited = false;
    aix = SCREEN_W * 0.5f; aiy = SCREEN_H * 0.5f; ai_retarget = 0;
}

void init(void) {
    best = load(0);
    bpm(96);
    // soft sine pad for the win chime
    instrument(6, INSTR_SINE, 6, 200, 3, 320);
    reset();
}

// the mouse/steer point handed from update() to draw() for the reticle
static float dsteerx, dsteery;
static bool  d_pull;

void update(void) {
    if (over) {
        if (mouse_pressed(MOUSE_LEFT) || btnp(0, BTN_A)) reset();
        return;
    }

    if (!started) {
        started = true;
        t_end = now() + ROUND_SEC;
        ai_retarget_now();
        // let the swarms settle a touch before the clock matters (paint as they go)
        for (int k = 0; k < 40; k++) {
            step_flock(0, SCREEN_W * 0.22f, SCREEN_H * 0.7f, 0.0008f);
            step_flock(1, aix, aiy, 0.0008f);
        }
    }

    // ── YOUR steer point: mouse while held, else WASD nudge, else drift ──
    static float steerx = SCREEN_W * 0.5f, steery = SCREEN_H * 0.5f;
    float my_pull = 0.0006f;                     // gentle default cohesion pull
    if (mouse_down(MOUSE_LEFT)) {
        steerx = mouse_x(); steery = clamp(mouse_y(), HUD_H + 2, SCREEN_H - 2);
        my_pull = 0.0042f;                       // strong pull toward the cursor
    } else {
        int mvx = btn(0, BTN_RIGHT) - btn(0, BTN_LEFT);
        int mvy = btn(0, BTN_DOWN)  - btn(0, BTN_UP);
        if (mvx || mvy) {
            steerx = clamp(steerx + mvx * 4, 4, SCREEN_W - 4);
            steery = clamp(steery + mvy * 4, HUD_H + 4, SCREEN_H - 4);
            my_pull = 0.0040f;
        }
    }

    // ── rival AI steer point ──
    if (now() >= ai_retarget) ai_retarget_now();

    // hand the steer point + pulling flag to draw() for the reticle
    dsteerx = steerx; dsteery = steery; d_pull = (my_pull > 0.0010f);

    step_flock(0, steerx, steery, my_pull);
    step_flock(1, aix, aiy, 0.0034f);

    // sound: a soft paint tick when your flock is working enemy ground
    if (frame() % 7 == 0 && mouse_down(MOUSE_LEFT))
        hit(60 + rnd(5), INSTR_SINE, 1, 30);

    if (frame() % 6 == 0) count_cover();

    if (now() >= t_end) {
        over = true;
        count_cover();
        int pct = paintable ? cover[0] * 100 / paintable : 0;
        if (cover[0] > cover[1]) {
            if (pct > best) { best = pct; save(0, best); }
            strum(60, CHORD_MAJ7, 6, 5, 60);
        } else {
            note(40, INSTR_TRI, 4);
        }
    }
}

void draw(void) {
    if (!floor_inited) { cls(NEUTRAL); floor_inited = true; }

    // repaint the whole floor from the grid every frame (clean, no smearing).
    // CELL is small (4px) so this is just one rectfill per painted cell.
    for (int gy = ROW_TOP; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++) {
            int o = owner[idx(gx, gy)];
            int col = o == 1 ? INK[0] : o == 2 ? INK[1] : NEUTRAL;
            rectfill(gx * CELL, gy * CELL, CELL, CELL, col);
        }

    // your steer marker (mouse pull point) — a little reticle on the ground
    if (!over) {
        int sx, sy; bool pulling;
        if (mouse_down(MOUSE_LEFT)) { sx = mouse_x(); sy = (int)clamp(mouse_y(), HUD_H + 2, SCREEN_H - 2); pulling = true; }
        else { sx = (int)dsteerx; sy = (int)dsteery; pulling = d_pull; }
        if (pulling) {
            int r = 4 + (int)(2 * sin_deg(now() * 360));
            circ(sx, sy, r + 4, INK_HI[0]);
            line(sx - 6, sy, sx + 6, sy, INK[0]);
            line(sx, sy - 6, sx, sy + 6, INK[0]);
        }
    }

    // ── boids: little arrowheads pointing along their velocity ──
    for (int t = 0; t < 2; t++)
        for (int i = 0; i < NB; i++) {
            Boid *b = &flock[t][i];
            float sp = sqrtf(b->vx * b->vx + b->vy * b->vy);
            if (sp < 0.01f) sp = 0.01f;
            float nx = b->vx / sp, ny = b->vy / sp;
            int tipx = (int)(b->x + nx * 4),     tipy = (int)(b->y + ny * 4);
            int lx   = (int)(b->x - nx * 3 - ny * 2), ly = (int)(b->y - ny * 3 + nx * 2);
            int rx   = (int)(b->x - nx * 3 + ny * 2), ry = (int)(b->y - ny * 3 - nx * 2);
            trifill(tipx, tipy, lx, ly, rx, ry, BODY[t]);
            pset(tipx, tipy, INK_HI[t]);
        }

    // ── HUD ──
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    int p0 = paintable ? cover[0] * 100 / paintable : 0;
    int p1 = paintable ? cover[1] * 100 / paintable : 0;

    // single split coverage bar: yours from the left, rival from the right,
    // neutral gap in the middle reads as "unclaimed ground left to grab"
    int bx = 56, bw = SCREEN_W - 56 - 56, by = 4, bh = 6;
    rectfill(bx, by, bw, bh, NEUTRAL);
    int w0 = bw * cover[0] / (paintable ? paintable : 1);
    int w1 = bw * cover[1] / (paintable ? paintable : 1);
    rectfill(bx, by, w0, bh, INK[0]);
    rectfill(bx + bw - w1, by, w1, bh, INK[1]);
    rect(bx, by, bw, bh, CLR_DARK_GREY);

    print(str("%d%%", p0), 4, 4, INK[0]);
    print_right(str("%d%%", p1), SCREEN_W - 4, 4, INK[1]);

    int secs = (int)(t_end - now());
    if (!started) secs = ROUND_SEC;
    print_centered(str("%d", secs < 0 ? 0 : secs), SCREEN_W/2, 4, CLR_WHITE);

    if (over) {
        fade(0.45f);
        rectfill(SCREEN_W / 2 - 84, SCREEN_H / 2 - 28, 168, 56, CLR_BLACK);
        rect(SCREEN_W / 2 - 84, SCREEN_H / 2 - 28, 168, 56, CLR_WHITE);
        const char *msg = cover[0] > cover[1] ? "YOU OWN THE TURF!" :
                          cover[1] > cover[0] ? "RIVAL OWNS THE TURF" : "DEAD HEAT!";
        int c = cover[0] > cover[1] ? INK[0] : cover[1] > cover[0] ? INK[1] : CLR_WHITE;
        print_centered(msg, SCREEN_W/2, SCREEN_H / 2 - 16, c);
        print_centered(str("%d%%  vs  %d%%", p0, p1), SCREEN_W/2, SCREEN_H / 2 - 2, CLR_LIGHT_GREY);
        print_centered(str("best %d%%", best), SCREEN_W/2, SCREEN_H / 2 + 8, CLR_YELLOW);
        if (blink(20)) print_centered("click / Z to replay", SCREEN_W/2, SCREEN_H / 2 + 18, CLR_DARK_GREY);
    } else {
        print_centered("hold LEFT-CLICK / WASD: pull your swarm", SCREEN_W/2, SCREEN_H - 9, CLR_DARKER_GREY);
    }
}
