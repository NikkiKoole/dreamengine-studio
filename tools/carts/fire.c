/* de:meta
{
  "title": "fire!",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "save-load-persistence"
  ],
  "lineage": "Demake of Nintendo's Fire! Game & Watch (1980); juggling timing loop with gravity and particle bursts on misses, structurally straightforward.",
  "genre": "arcade",
  "homage": "Fire! (Game & Watch, 1980)",
  "description": "Demake of the 1980 Nintendo Game & Watch. People leap from a burning building — run left/right with the firefighters' rescue net to bounce them across the screen and into the ambulance. Each bounce flings them further right, so you juggle new jumpers off the building while shepherding others along. Miss one and they hit the ground; three misses ends it. Speeds up as you rack up rescues, best score saved. Left/Right or A/D to move the net."
}
de:meta */
#include "studio.h"

// FIRE!  — after the 1980 Nintendo Game & Watch
// People leap from the burning building. Run left/right with the rescue net
// to bounce them across the screen and into the ambulance. Each bounce flings
// them further right; miss one and they hit the ground. Don't let three slip.
// Left/Right (or A/D): move the net.

#define NET_Y     168
#define NET_HW    16          // net half-width
#define GROUND    184
#define BLDG_W    60
#define AMB_X     280         // descending past here = saved
#define GRAV      0.18f
#define BOUNCE    -4.6f
#define PUSH      1.9f
#define MAX_V     10

typedef struct { float x, y, vx, vy; int bounces, kind; bool active; } Victim;
typedef struct { float x, y, vx, vy, life; int col; bool on; } Bit;

static Victim vic[MAX_V];
static Bit    bits[40];
static float  net_x;
static int    lives, score, hiscore, saves, level;
static float  spawn_at;
static int    state;          // 0 play, 1 game over
static int    flash;          // building flash on a miss

static const int WIN_Y[3] = { 44, 80, 116 };
static const int VCOL[4]   = { CLR_RED, CLR_BLUE, CLR_GREEN, CLR_PINK };

// ====================================================================

static void spawn_bits(int x, int y, int col, int n) {
    for (int i = 0; i < 40 && n > 0; i++) {
        if (!bits[i].on) {
            float a = rnd(360);
            bits[i] = (Bit){ x, y, dx(rnd_float_between(1, 3), a), dy(rnd_float_between(1, 3), a) - 1.0f,
                             rnd_float_between(0.4f, 0.9f), col, true };
            n--;
        }
    }
}

static void spawn_victim() {
    for (int i = 0; i < MAX_V; i++) {
        if (!vic[i].active) {
            vic[i].x = 52; vic[i].y = WIN_Y[rnd(3)];
            vic[i].vx = 0; vic[i].vy = 0.4f;
            vic[i].bounces = 0; vic[i].kind = rnd(4); vic[i].active = true;
            return;
        }
    }
}

static void reset_game() {
    for (int i = 0; i < MAX_V; i++) vic[i].active = false;
    for (int i = 0; i < 40; i++)    bits[i].on = false;
    net_x = SCREEN_W / 2;
    lives = 3; score = 0; saves = 0; level = 1; state = 0; flash = 0;
    spawn_at = now() + 1.0f;
}

void init() { hiscore = load(0); reset_game(); }

static void lose_life() {
    lives--;
    flash = 12;
    note(34, INSTR_NOISE, 6);
    if (lives <= 0) {
        state = 1;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
    }
}

// ====================================================================

void update() {
    if (state == 1) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) reset_game();
        return;
    }
    if (flash > 0) flash--;

    // move the net
    if (btn(0, BTN_LEFT))  net_x -= 3.2f;
    if (btn(0, BTN_RIGHT)) net_x += 3.2f;
    net_x = clamp(net_x, BLDG_W - 12, SCREEN_W - 18);

    // spawn
    if (now() > spawn_at) {
        spawn_victim();
        float gap = clamp(2.4f - level * 0.18f, 0.7f, 2.4f);
        spawn_at = now() + gap;
    }

    for (int i = 0; i < MAX_V; i++) {
        Victim *v = &vic[i];
        if (!v->active) continue;
        v->vy += GRAV;
        v->x  += v->vx;
        v->y  += v->vy;

        // bounce off the net (only while falling, and not yet at the ambulance)
        if (v->vy > 0 && v->x < AMB_X &&
            v->y >= NET_Y - 4 && v->y <= NET_Y + 6 &&
            v->x > net_x - NET_HW && v->x < net_x + NET_HW) {
            v->vy = BOUNCE;
            v->vx = PUSH;
            v->y  = NET_Y - 4;
            v->bounces++;
            score += 5;
            hit(72 + v->bounces * 4, INSTR_SQUARE, 3, 60);
            spawn_bits((int)v->x, NET_Y, CLR_WHITE, 3);
        }

        // saved — came down over the ambulance
        if (v->vy > 0 && v->x >= AMB_X && v->y >= 150) {
            v->active = false;
            score += 100; saves++;
            level = 1 + saves / 8;
            hit(76, INSTR_SINE, 4, 100);
            hit(83, INSTR_SINE, 4, 100);
            continue;
        }

        // missed — hit the ground
        if (v->y >= GROUND) {
            v->active = false;
            spawn_bits((int)v->x, GROUND, CLR_RED, 8);
            lose_life();
        }
    }

    for (int i = 0; i < 40; i++) {
        if (!bits[i].on) continue;
        bits[i].vy += 0.25f;
        bits[i].x  += bits[i].vx;
        bits[i].y  += bits[i].vy;
        bits[i].life -= 1.0f / 60.0f;
        if (bits[i].life <= 0 || bits[i].y > GROUND + 4) bits[i].on = false;
    }
}

// ====================================================================
// drawing
// ====================================================================

static void draw_victim(Victim *v) {
    int x = (int)v->x, y = (int)v->y;
    int wig = (int)(sin_deg(now() * 600 + x * 30) * 2);   // panicked flailing
    circfill(x, y - 4, 3, CLR_LIGHT_PEACH);               // head
    rectfill(x - 2, y - 1, 4, 5, VCOL[v->kind]);          // body
    line(x - 2, y, x - 5, y - 3 - wig, CLR_LIGHT_PEACH);  // arms up
    line(x + 2, y, x + 5, y - 3 + wig, CLR_LIGHT_PEACH);
    line(x - 1, y + 4, x - 3, y + 6, VCOL[v->kind]);      // legs
    line(x + 1, y + 4, x + 3, y + 6, VCOL[v->kind]);
}

static void draw_fireman(int x) {
    rectfill(x - 3, GROUND - 10, 6, 10, CLR_YELLOW);      // coat
    circfill(x, GROUND - 12, 3, CLR_LIGHT_PEACH);         // face
    rectfill(x - 4, GROUND - 15, 8, 2, CLR_RED);          // helmet brim
    rectfill(x - 2, GROUND - 17, 4, 2, CLR_RED);
    rectfill(x - 1, GROUND, 2, 0, CLR_DARK_BLUE);
}

void draw() {
    cls(CLR_DARKER_BLUE);

    // ground
    rectfill(0, GROUND, SCREEN_W, SCREEN_H - GROUND, CLR_DARK_GREY);

    // burning building
    int bc = (flash > 0 && (flash / 2) % 2) ? CLR_WHITE : CLR_DARK_BROWN;
    rectfill(0, 24, BLDG_W, GROUND - 24, bc);
    for (int wy = 0; wy < 3; wy++)
        for (int wx = 0; wx < 2; wx++) {
            int x = 10 + wx * 26, y = WIN_Y[wy] - 6;
            rectfill(x, y, 14, 12, ((frame() / 6 + wx + wy) % 2) ? CLR_ORANGE : CLR_YELLOW);
        }
    // flames licking the roof
    for (int f = 0; f < BLDG_W; f += 6) {
        int h = 8 + (int)(noise(f * 0.3f + now() * 4) * 14);
        trifill(f, 24, f + 3, 24 - h, f + 6, 24, (f / 6) % 2 ? CLR_ORANGE : CLR_YELLOW);
    }

    // ambulance
    int ax = AMB_X;
    rectfill(ax, GROUND - 18, SCREEN_W - ax, 18, CLR_WHITE);
    rectfill(ax, GROUND - 18, SCREEN_W - ax, 4, blink(8) ? CLR_BLUE : CLR_RED); // light bar
    rectfill(ax + 6, GROUND - 12, 8, 2, CLR_RED);          // red cross
    rectfill(ax + 9, GROUND - 15, 2, 8, CLR_RED);
    circfill(ax + 4, GROUND, 3, CLR_BLACK);
    circfill(SCREEN_W - 6, GROUND, 3, CLR_BLACK);

    // victims + debris
    for (int i = 0; i < MAX_V; i++) if (vic[i].active) draw_victim(&vic[i]);
    for (int i = 0; i < 40; i++) if (bits[i].on) pset((int)bits[i].x, (int)bits[i].y, bits[i].col);

    // firemen + net
    draw_fireman((int)net_x - NET_HW);
    draw_fireman((int)net_x + NET_HW);
    line((int)net_x - NET_HW, NET_Y, (int)net_x, NET_Y + 3, CLR_LIGHT_GREY);
    line((int)net_x, NET_Y + 3, (int)net_x + NET_HW, NET_Y, CLR_LIGHT_GREY);
    line((int)net_x - NET_HW, NET_Y - 1, (int)net_x + NET_HW, NET_Y - 1, CLR_RED);

    // HUD
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_right(str("HI %d", hiscore), SCREEN_W - 4, 3, CLR_YELLOW);
    print_centered(str("SAVED %d", saves), SCREEN_W/2, 3, CLR_LIGHT_GREY);
    for (int i = 0; i < lives; i++) {
        int ix = 70 + i * 12;
        rectfill(ix - 4, 5, 8, 2, CLR_RED);
        rectfill(ix - 2, 3, 4, 2, CLR_RED);
    }

    if (state == 1) {
        rectfill(SCREEN_W / 2 - 64, SCREEN_H / 2 - 24, 128, 50, CLR_BLACK);
        rect    (SCREEN_W / 2 - 64, SCREEN_H / 2 - 24, 128, 50, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H / 2 - 14, CLR_RED);
        print_centered(str("SAVED %d", saves), SCREEN_W/2, SCREEN_H / 2 - 1, CLR_YELLOW);
        print_centered("Z to try again", SCREEN_W/2, SCREEN_H / 2 + 13, CLR_LIGHT_GREY);
    }
}
