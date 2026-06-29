/* de:meta
{
  "title": "flappy",
  "status": "active",
  "kind": [
    "game",
    "tutorial"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "save-load-persistence",
    "parallax",
    "screen-shake-juice"
  ],
  "lineage": "Capstone of the tutorial curriculum; a minimal Flappy Bird clone whose stated purpose is to demonstrate the complete game loop arc (attract → play → death → game over with persisted hi-score) and a full juice repertoire.",
  "genre": "arcade",
  "description": "The smallest *complete* game, start to finish: an animated attract/title screen, the play loop, a death sequence, and a game-over panel with a medal + a hi-score that survives restarts. ONE BUTTON on every device - SPACE / Z / gamepad-A / mouse-click / screen-tap all flap, so it's fully playable on a phone with no on-screen controls. The bird is drawn in code (sprite-draw.js): one bird() function sampled at three wing heights makes the flap cycle, plus a dead frame, soft clouds, and grass bushes. Juice on every beat: flap stretches the bird upward + puffs feathers + tilts its nose up; falling tilts it into a dive (sspr_ex rotate + squash); passing a pipe pops the score bigger with an ascending ding; crashing flashes the screen white, shakes, hit-stops, then tumbles the bird off spinning. Parallax clouds + scrolling ground. The capstone the tutorials were missing - the bridge from learning primitives to a finished game. Tap / Space / click to play."
}
de:meta */
// flappy — the smallest *complete* game: attract screen → play → game over,
// hi-score that survives restarts, and juice on every beat. The capstone the
// tutorials were missing (cart-library-direction §3 / tutorial-curriculum #40):
// it shows the whole arc, not just a mechanic.
//
// ONE BUTTON, every device: SPACE / Z / gamepad-A / mouse-click / screen-tap all
// "flap" — `tapp(0,0,SCREEN_W,SCREEN_H)` catches mouse+touch, btnp catches
// key+pad. So it's playable on a phone with no on-screen controls at all.
//
// The juice, by event (see CLAUDE.md "game feel"):
//   flap   → upward stretch + wing blip + a puff of feathers + nose tilts up
//   fall   → bird tilts toward a nose-dive the faster it drops
//   score  → ascending ding + the number pops bigger for a moment
//   crash  → screen flash + shake + hit-stop, then the bird tumbles off, spinning
//   best   → "NEW BEST!" + a brighter medal
#include "studio.h"
#include <math.h>
#include <stdio.h>

// ── tunables ──────────────────────────────────────────────────────────────
#define BIRD_CX     78          // bird stays here; the world scrolls past it
#define BIRD_R       5           // collision radius (a touch forgiving)
#define GRAVITY      0.34f
#define FLAP_V      -3.7f
#define MAX_FALL     5.4f
#define GROUND_Y   180           // playfield is 0..GROUND_Y; ground below
#define PIPE_W      28
#define GAP_H       30           // half the gap — total opening = 2×
#define SPEED        1.5f
#define SPACING    130           // horizontal gap between pipe centers
#define NPIPES       4           // on screen at once (SCREEN_W/SPACING + slack)

// ── state machine ───────────────────────────────────────────────────────────
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };
static int   state = ST_TITLE;
static int   t = 0;              // frames in the current state

// ── bird ──────────────────────────────────────────────────────────────────
static float by, vy;             // center y + vertical velocity
static float rot;                // visual tilt (degrees; + = nose down)
static float stretch;            // >0 just after a flap → tall squash, decays
static bool  landed;             // dead bird has hit the ground

// ── pipes ───────────────────────────────────────────────────────────────────
static struct { float x; int gap; bool scored; } pipe[NPIPES];

// ── scoring + flow ──────────────────────────────────────────────────────────
static int   score, best, hitstop, flash, pop, newbest;
static float gscroll;            // ground-stripe offset (wraps at 16)
static float scrollx;            // continuous prop scroll (bushes) — never wraps

// ── parallax clouds ──────────────────────────────────────────────────────────
static struct { float x, y; int big; } cloud[4];

// ── particles (feathers / dust) ──────────────────────────────────────────────
static struct { float x, y, vx, vy; int life, max, col; } pt[40];

static void spawn(float x, float y, int n, int spread, int col) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < 40; i++)
            if (pt[i].life <= 0) {
                pt[i].x = x; pt[i].y = y;
                pt[i].vx = rnd_float_between(-spread, spread) * 0.06f - 0.4f;
                pt[i].vy = rnd_float_between(-spread, spread) * 0.06f;
                pt[i].max = pt[i].life = rnd_between(16, 30);
                pt[i].col = col;
                break;
            }
}

// universal "flap this frame?" — key, pad, mouse, or touch
static bool flap_input(void) {
    return keyp(KEY_SPACE) || btnp(0, BTN_A) || tapp(0, 0, SCREEN_W, SCREEN_H);
}

static void place_pipe(int i, float x) {
    pipe[i].x = x;
    pipe[i].gap = rnd_between(GAP_H + 18, GROUND_Y - GAP_H - 12);
    pipe[i].scored = false;
}

static void start_game(void) {
    by = GROUND_Y * 0.42f; vy = 0; rot = 0; stretch = 0; landed = false;
    score = 0; newbest = 0; pop = 0;
    for (int i = 0; i < NPIPES; i++) place_pipe(i, SCREEN_W + 30 + i * SPACING);
}

static void do_flap(void) {
    vy = FLAP_V;
    stretch = 1.0f;
    spawn(BIRD_CX - 4, by + 3, 3, 14, CLR_WHITE);
    hit(70 + rnd_between(0, 4), INSTR_SQUARE, 3, 38);
}

static void die(void) {
    state = ST_DEAD; t = 0; landed = false;
    hitstop = 4; flash = 9; shake(7);
    spawn(BIRD_CX, by, 12, 30, CLR_YELLOW);
    hit(46, INSTR_NOISE, 6, 90);
    hit(33, INSTR_SINE, 6, 200);
    if (score > best) { best = score; newbest = 1; save_int("flappy_best", best); }
}

void init(void) {
    colorkey(0);                 // index 0 = the sprites' blank background → transparent
    best = load_int("flappy_best", 0);
    for (int i = 0; i < 4; i++) { cloud[i].x = i * 90 + 10; cloud[i].y = 18 + (i % 3) * 26; cloud[i].big = i & 1; }
    start_game();
    state = ST_TITLE; t = 0;
}

// world that scrolls behind the bird (clouds always; ground only when alive)
static void scroll_world(bool ground) {
    for (int i = 0; i < 4; i++) {
        cloud[i].x -= SPEED * 0.25f;
        if (cloud[i].x < -34) { cloud[i].x = SCREEN_W + rnd_between(0, 40); cloud[i].y = 14 + rnd_between(0, 70); }
    }
    if (ground) { gscroll = fmodf(gscroll + SPEED, 16.0f); scrollx += SPEED; }
}

static void tick_particles(void) {
    for (int i = 0; i < 40; i++) if (pt[i].life > 0) {
        pt[i].x += pt[i].vx; pt[i].y += pt[i].vy; pt[i].vy += 0.12f; pt[i].life--;
    }
}

void update(void) {
    t++;
    if (pop > 0) pop--;
    if (flash > 0) flash--;
    tick_particles();

#ifdef DE_TRACE
    watch("state", "%d", state);
    watch("score", "%d", score);
    watch("by", "%.1f", by);
#endif

    if (state == ST_TITLE) {
        scroll_world(true);
        by = GROUND_Y * 0.42f + sinf(frame() * 0.06f) * 6.0f;   // gentle bob
        rot = sinf(frame() * 0.06f) * 6.0f;
        if (flap_input()) { start_game(); do_flap(); state = ST_PLAY; t = 0; }
        return;
    }

    if (state == ST_OVER) {
        scroll_world(false);
        if (t > 25 && flap_input()) { start_game(); state = ST_TITLE; t = 0; }
        return;
    }

    if (state == ST_DEAD) {
        if (hitstop > 0) { hitstop--; return; }      // freeze for impact weight
        scroll_world(false);
        if (!landed) {                                // bird tumbles off
            vy += GRAVITY; if (vy > MAX_FALL) vy = MAX_FALL;
            by += vy; rot += 9.0f;
            if (by >= GROUND_Y - BIRD_R) { by = GROUND_Y - BIRD_R; landed = true; spawn(BIRD_CX, by, 6, 20, CLR_DARK_BROWN); }
        }
        if (t > 36) { state = ST_OVER; t = 0; }
        return;
    }

    // ── ST_PLAY ──
    if (flap_input()) do_flap();
    vy += GRAVITY; if (vy > MAX_FALL) vy = MAX_FALL;
    by += vy;
    stretch = lerp(stretch, 0.0f, 0.2f);
    // tilt: nose up while rising, dive harder the faster we fall
    float target = vy < 0 ? -26.0f : clamp(vy * 12.0f, 0.0f, 80.0f);
    rot = lerp(rot, target, 0.18f);
    scroll_world(true);

    if (by < BIRD_R) { by = BIRD_R; vy = 0; }         // ceiling clamps (no death)

    for (int i = 0; i < NPIPES; i++) {
        pipe[i].x -= SPEED;
        if (pipe[i].x < -PIPE_W) place_pipe(i, pipe[i].x + NPIPES * SPACING);

        // score the moment a pipe's right edge clears the bird
        if (!pipe[i].scored && pipe[i].x + PIPE_W < BIRD_CX) {
            pipe[i].scored = true; score++; pop = 14;
            hit(81, INSTR_TRI, 4, 55);
            schedule_hit(55, 88, INSTR_TRI, 4, 70);
        }
        // collision: bird box vs the two pipe rects
        float bl = BIRD_CX - BIRD_R, br = BIRD_CX + BIRD_R, bt = by - BIRD_R, bb = by + BIRD_R;
        if (br > pipe[i].x && bl < pipe[i].x + PIPE_W &&
            (bt < pipe[i].gap - GAP_H || bb > pipe[i].gap + GAP_H)) { die(); return; }
    }
    if (by + BIRD_R >= GROUND_Y) { by = GROUND_Y - BIRD_R; die(); }   // hit the ground
}

// ── drawing ─────────────────────────────────────────────────────────────────
static void draw_bird(float cx, float cy, float r, int slot, float st) {
    int sx = (slot % 8) * 16, sy = (slot / 8) * 16;
    int dw = (int)(16 * (1.0f - st * 0.18f) + 0.5f);
    int dh = (int)(16 * (1.0f + st * 0.30f) + 0.5f);
    sspr_ex(sx, sy, 16, 16, (int)(cx - dw / 2.0f), (int)(cy - dh / 2.0f), dw, dh, r, dw / 2, dh / 2);
}

static void draw_pipes(void) {
    for (int i = 0; i < NPIPES; i++) {
        int x = (int)pipe[i].x, g = pipe[i].gap;
        for (int pass = 0; pass < 2; pass++) {
            int top = pass == 0 ? 0 : g + GAP_H;
            int bot = pass == 0 ? g - GAP_H : GROUND_Y;
            rectfill(x, top, PIPE_W, bot - top, CLR_GREEN);
            rectfill(x, top, 5, bot - top, CLR_LIME_GREEN);        // left highlight
            rectfill(x + PIPE_W - 7, top, 7, bot - top, CLR_MEDIUM_GREEN); // right shade
            rect(x, top, PIPE_W, bot - top, CLR_DARK_GREEN);
            // lip (the classic capped mouth) at the gap end
            int lipY = pass == 0 ? g - GAP_H - 11 : g + GAP_H;
            rectfill(x - 3, lipY, PIPE_W + 6, 11, CLR_GREEN);
            rectfill(x - 3, lipY, 6, 11, CLR_LIME_GREEN);
            rect(x - 3, lipY, PIPE_W + 6, 11, CLR_DARK_GREEN);
        }
    }
}

static void draw_world(void) {
    // sky gradient — deep blue up top warming toward the horizon
    cls(CLR_TRUE_BLUE);
    rectfill(0, 30, SCREEN_W, 50, CLR_BLUE);
    rectfill(0, 80, SCREEN_W, GROUND_Y - 80, CLR_LIGHT_PEACH);
    // clouds (parallax, soft, drawn bigger than their 16×16 sprite). slot 6 → (96,0)
    for (int i = 0; i < 4; i++)
        sspr(96, 0, 16, 16, (int)cloud[i].x, (int)cloud[i].y,
             cloud[i].big ? 34 : 22, cloud[i].big ? 22 : 14);
}

static void draw_ground(void) {
    rectfill(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, CLR_DARK_BROWN);
    rectfill(0, GROUND_Y, SCREEN_W, 5, CLR_LIME_GREEN);            // grass cap
    rectfill(0, GROUND_Y + 5, SCREEN_W, 2, CLR_MEDIUM_GREEN);
    for (int x = -16; x < SCREEN_W + 16; x += 16)                  // scrolling stripes
        line(x + (int)(-gscroll), GROUND_Y + 7, x + 8 - (int)gscroll, SCREEN_H, CLR_BROWN);
}

static void draw_particles(void) {
    for (int i = 0; i < 40; i++) if (pt[i].life > 0) {
        int s = pt[i].life > pt[i].max / 2 ? 2 : 1;
        circfill((int)pt[i].x, (int)pt[i].y, s, pt[i].col);
    }
}

static void medal(int cx, int cy) {
    int col = score >= 35 ? CLR_LIGHT_YELLOW : score >= 20 ? CLR_LIGHT_GREY
            : score >= 10 ? CLR_BROWN : -1;
    if (col < 0) return;
    circfill(cx, cy, 9, col);
    circ(cx, cy, 9, CLR_WHITE);
    circfill(cx, cy, 4, CLR_WHITE);            // a star-ish center
    circ(cx, cy, 4, col);
}

void draw(void) {
    draw_world();
    if (state == ST_PLAY || state == ST_DEAD || state == ST_OVER) draw_pipes();
    draw_ground();
    for (int i = 0; i < 5; i++) {                                   // bushes scroll with the ground
        int period = SCREEN_W + 40;
        int x = (((i * 88 - (int)scrollx) % period) + period) % period - 20;
        spr(7, x, GROUND_Y - 11);
    }

    // bird — animated flap while alive, dead frame while tumbling
    int slot = state == ST_DEAD || state == ST_OVER ? 5 : 1 + anim(4, state == ST_PLAY ? 11 : 6, 0);
    draw_bird(BIRD_CX, by, rot, slot, stretch);
    draw_particles();

    // ── HUD / overlays ──
    if (state == ST_TITLE) {
        const char *T = "FLAPPY";                                   // print_scaled is left-anchored
        int w = text_width(T) * 4, x = SCREEN_W / 2 - w / 2, y = 34;
        print_scaled(T, x + 2, y + 3, CLR_DARK_BLUE, 4);           // drop shadow
        print_scaled(T, x,     y,     CLR_YELLOW,     4);
        if (blink(22)) print_centered("tap / space to flap", SCREEN_W / 2, 100, CLR_WHITE);
        char b[24]; snprintf(b, sizeof b, "best  %d", best);
        print_centered(b, SCREEN_W / 2, 116, CLR_LIGHT_GREY);
    }

    if (state == ST_PLAY || state == ST_DEAD) {
        int sc = pop > 0 ? 4 : 3;                                   // the number pops on a point
        char b[8]; snprintf(b, sizeof b, "%d", score);
        int w = text_width(b) * sc;
        print_scaled(b, SCREEN_W / 2 - w / 2 + 1, 13, CLR_DARK_BLUE, sc);   // shadow
        print_scaled(b, SCREEN_W / 2 - w / 2,     12, CLR_WHITE,     sc);
    }

    if (state == ST_OVER) {
        int px = SCREEN_W / 2 - 70, py = 40, pw = 140, ph = 96;
        rectfill(px, py, pw, ph, CLR_DARK_BLUE);
        rect(px, py, pw, ph, CLR_WHITE);
        print_centered("GAME OVER", SCREEN_W / 2, py + 10, CLR_LIGHT_YELLOW);
        medal(px + 30, py + 52);
        char b[24];
        snprintf(b, sizeof b, "SCORE %d", score); print(b, px + 52, py + 40, CLR_WHITE);
        snprintf(b, sizeof b, "BEST  %d", best);  print(b, px + 52, py + 56, CLR_LIGHT_GREY);
        if (newbest && blink(18)) print_centered("NEW BEST!", SCREEN_W / 2, py + 74, CLR_ORANGE);
        if (t > 25 && blink(20)) print_centered("tap to play again", SCREEN_W / 2, py + ph - 2, CLR_WHITE);
    }

    // death flash — a quick white wash that thins over a few frames
    if (flash > 0) {
        int pats[5] = { 0xFFFF, 0xFFFF, 0xA5A5, 0xAAAA, 0x8020 };
        int idx = 9 - flash; if (idx > 4) idx = 4; if (idx < 0) idx = 0;
        fillp(pats[idx], -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE);
        fillp_reset();
    }
}
