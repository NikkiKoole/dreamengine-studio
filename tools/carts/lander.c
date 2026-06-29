/* de:meta
{
  "title": "lunar lander",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop"
  ],
  "lineage": "Direct homage to Atari Lunar Lander (1979); uses simple Euler-integration physics (gravity + thrust vector) with procedurally generated terrain per level — no notable algorithmic novelty beyond the arcade loop structure.",
  "genre": "arcade",
  "homage": "Lunar Lander (1979)",
  "description": "Physics-based lander. Rotate and thrust your way onto the lit green pad slowly enough to survive. Levels get harder — pads get narrower. Left/right to rotate, up or Z to thrust."
}
de:meta */
#include "studio.h"

// LUNAR LANDER
// Left/Right: rotate   Up or Z: thrust
// Land on the lit green pad — watch speed and angle!

#define SEG_W     8
#define N_SEGS    (SCREEN_W / SEG_W)   // 40
#define GRAVITY   0.030f
#define THRUST    0.09f
#define ROT_SPD   3.0f
#define FUEL_MAX  500
#define SAFE_VY   1.5f
#define SAFE_VX   1.2f
#define SAFE_ANG  25.0f

typedef enum { FLYING, LANDED, CRASHED } Phase;

static int   terrain[N_SEGS + 1];
static int   pad_l, pad_r, pad_y;

static float lx, ly, vx, vy, ang;
static int   fuel;
static Phase phase;
static bool  thrusting;
static int   crash_f;

static int   score, last_gain, lives, level;

// ---- terrain ----

static void gen_terrain() {
    int min_w = max(3, 7 - level);
    pad_l = 4 + rnd(N_SEGS / 2);
    pad_r = pad_l + min_w + rnd(4);
    if (pad_r > N_SEGS - 4) { pad_r = N_SEGS - 4; pad_l = max(4, pad_r - min_w); }
    pad_y = 135 + rnd(35);

    int h = 150;
    for (int i = 0; i <= N_SEGS; i++) {
        if (i >= pad_l && i <= pad_r) {
            terrain[i] = pad_y; h = pad_y;
        } else {
            h += rnd(16) - 7;
            h = mid(118, h, 184);
            terrain[i] = h;
        }
    }
}

static float ground_at(float x) {
    int seg = (int)(x / SEG_W);
    seg = mid(0, seg, N_SEGS - 1);
    float t = (x - seg * SEG_W) / SEG_W;
    return lerp((float)terrain[seg], (float)terrain[seg + 1], t);
}

// ---- lander ----

static void reset_ship() {
    lx  = SCREEN_W / 2 + rnd(60) - 30;
    ly  = 18.0f;
    vx  = rnd_float_between(-0.4f, 0.4f);
    vy  = 0.1f;
    ang = -90.0f;
    fuel = FUEL_MAX;
    phase = FLYING;
    thrusting = false;
}

void init() {
    lives = 3; score = 0; level = 0;
    gen_terrain();
    reset_ship();
}

void update() {
    if (phase != FLYING) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) {
            if (lives <= 0)          { lives=3; score=0; level=0; gen_terrain(); }
            else if (phase == LANDED) { level++; gen_terrain(); }
            reset_ship();
        }
        return;
    }

    if (btn(0, BTN_LEFT))  ang -= ROT_SPD;
    if (btn(0, BTN_RIGHT)) ang += ROT_SPD;

    thrusting = (btn(0, BTN_UP) || btn(0, BTN_A)) && fuel > 0;
    if (thrusting) {
        vx  += dx(THRUST, ang);
        vy  += dy(THRUST, ang);
        fuel -= 2;
        if (fuel < 0) fuel = 0;
        if (frame() % 10 == 0) note(48, INSTR_NOISE, 2);
    }

    vy += GRAVITY;
    lx += vx;
    ly += vy;

    // wrap x, clamp top
    if (lx < 0)         lx += SCREEN_W;
    if (lx >= SCREEN_W) lx -= SCREEN_W;
    if (ly < 0)         { ly = 0; vy = 0; }

    // terrain collision
    float gnd = ground_at(lx);
    if (ly + 14.0f >= gnd) {
        ly = gnd - 14.0f;

        // angle from vertical
        float a = ang + 90.0f;
        while (a >  180.0f) a -= 360.0f;
        while (a < -180.0f) a += 360.0f;

        int  seg    = (int)(lx / SEG_W);
        bool on_pad = (seg >= pad_l && seg < pad_r);
        bool good   = on_pad && vy < SAFE_VY
                             && vx > -SAFE_VX && vx < SAFE_VX
                             && a > -SAFE_ANG && a < SAFE_ANG;

        if (good) {
            phase = LANDED;
            int pad_w  = pad_r - pad_l;
            int bonus  = (pad_w <= 3) ? 3 : (pad_w <= 5) ? 2 : 1;
            last_gain  = (fuel / 8 + 60) * bonus;
            score     += last_gain;
            note(72, INSTR_SINE, 5);
        } else {
            phase = CRASHED;
            crash_f = frame();
            lives--;
            note(36, INSTR_NOISE, 7);
        }
        vx = vy = 0;
    }
}

// ---- draw helpers ----

static void draw_lander(float x, float y, float a, int col) {
    float top_x = x + dx(9, a),       top_y = y + dy(9, a);
    float lft_x = x + dx(7, a+130),   lft_y = y + dy(7, a+130);
    float rgt_x = x + dx(7, a-130),   rgt_y = y + dy(7, a-130);
    float btm_x = x + dx(8, a+180),   btm_y = y + dy(8, a+180);
    float ll_x  = lft_x+dx(7,a+155),  ll_y  = lft_y+dy(7,a+155);
    float rl_x  = rgt_x+dx(7,a-155),  rl_y  = rgt_y+dy(7,a-155);

    line((int)top_x,(int)top_y, (int)lft_x,(int)lft_y, col);
    line((int)top_x,(int)top_y, (int)rgt_x,(int)rgt_y, col);
    line((int)lft_x,(int)lft_y, (int)btm_x,(int)btm_y, col);
    line((int)rgt_x,(int)rgt_y, (int)btm_x,(int)btm_y, col);
    line((int)lft_x,(int)lft_y, (int)ll_x,(int)ll_y,   col);
    line((int)rgt_x,(int)rgt_y, (int)rl_x,(int)rl_y,   col);
    line((int)ll_x,(int)ll_y,   (int)btm_x,(int)btm_y+1, col);
    line((int)rl_x,(int)rl_y,   (int)btm_x,(int)btm_y+1, col);

    if (thrusting && frame()%3 < 2) {
        float flx = btm_x + dx(8+rnd(6), a+180);
        float fly = btm_y + dy(8+rnd(6), a+180);
        line((int)btm_x,(int)btm_y,(int)flx,(int)fly, CLR_ORANGE);
        float flx2 = btm_x + dx(5+rnd(4), a+177);
        float fly2 = btm_y + dy(5+rnd(4), a+177);
        line((int)btm_x,(int)btm_y,(int)flx2,(int)fly2, CLR_YELLOW);
    }
}

void draw() {
    cls(CLR_BLACK);

    // stars
    for (int i = 0; i < 55; i++) {
        int sx = (i*97+11) % SCREEN_W, sy = (i*73+7) % 115;
        pset(sx, sy, i%5==0 ? CLR_WHITE : CLR_DARK_GREY);
    }

    // terrain
    for (int i = 0; i < N_SEGS; i++) {
        bool is_pad = (i >= pad_l && i < pad_r);
        line(i*SEG_W, terrain[i], (i+1)*SEG_W, terrain[i+1],
             is_pad ? CLR_GREEN : CLR_LIGHT_GREY);
    }

    // pad beacon lights (blinking)
    if (blink(12)) {
        circfill(pad_l*SEG_W, pad_y-3, 2, CLR_GREEN);
        circfill(pad_r*SEG_W, pad_y-3, 2, CLR_GREEN);
    }

    // lander / crash debris
    if (phase == CRASHED) {
        int t = min(frame() - crash_f, 55);
        for (int i = 0; i < 10; i++) {
            float da   = i * 36.0f + t * 4.0f;
            float dist = t * 0.65f;
            pset((int)(lx+dx(dist,da)), (int)(ly+dy(dist,da)),
                 i%2==0 ? CLR_ORANGE : CLR_RED);
        }
    } else {
        draw_lander(lx, ly, ang, phase==LANDED ? CLR_GREEN : CLR_WHITE);
    }

    // HUD — top bar
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_centered(str("LVL %d", level+1), SCREEN_W/2, 3, CLR_DARK_GREY);
    // lives as small ship icons
    for (int i = 0; i < lives; i++) {
        int ix = SCREEN_W - 10 - i*12, iy = 3;
        line(ix+3,iy,  ix,  iy+5, CLR_LIGHT_GREY);
        line(ix+3,iy,  ix+6,iy+5, CLR_LIGHT_GREY);
        line(ix+1,iy+4,ix+5,iy+4, CLR_LIGHT_GREY);
    }

    // fuel bar (left side, vertical)
    int bx=4, by=14, bh=50;
    rect(bx, by, 8, bh, CLR_DARK_GREY);
    int fill = (fuel * bh) / FUEL_MAX;
    int fc   = fuel > FUEL_MAX*2/3 ? CLR_GREEN :
               fuel > FUEL_MAX/5   ? CLR_YELLOW : CLR_RED;
    if (fill > 0) rectfill(bx+1, by+bh-fill, 6, fill, fc);
    print("F", bx+1, by+bh+3, CLR_DARK_GREY);

    // velocity + angle readout (bottom)
    bool vok = vy < SAFE_VY && vx > -SAFE_VX && vx < SAFE_VX;
    float afrom_up = ang+90.0f;
    while (afrom_up>180.0f)  afrom_up-=360.0f;
    while (afrom_up<-180.0f) afrom_up+=360.0f;
    bool aok = afrom_up>-SAFE_ANG && afrom_up<SAFE_ANG;
    print(str("V%.1f H%.1f", vy, vx), 20, SCREEN_H-10, vok ? CLR_GREEN : CLR_RED);
    print(str("ANG%+.0f", afrom_up), SCREEN_W/2-16, SCREEN_H-10, aok ? CLR_GREEN : CLR_ORANGE);

    // overlay
    if (phase == LANDED) {
        rectfill(SCREEN_W/2-64, SCREEN_H/2-20, 128, 48, CLR_BLACK);
        rect    (SCREEN_W/2-64, SCREEN_H/2-20, 128, 48, CLR_GREEN);
        print_centered("LANDED!", SCREEN_W/2, SCREEN_H/2-12, CLR_GREEN);
        print_centered(str("+%d pts", last_gain), SCREEN_W/2, SCREEN_H/2, CLR_YELLOW);
        print_centered("Z for next level", SCREEN_W/2, SCREEN_H/2+12, CLR_LIGHT_GREY);
    }
    if (phase == CRASHED) {
        rectfill(SCREEN_W/2-64, SCREEN_H/2-20, 128, 48, CLR_BLACK);
        rect    (SCREEN_W/2-64, SCREEN_H/2-20, 128, 48, CLR_RED);
        if (lives > 0) {
            print_centered("CRASHED!", SCREEN_W/2, SCREEN_H/2-12, CLR_RED);
            print_centered(str("LIVES LEFT %d", lives), SCREEN_W/2, SCREEN_H/2, CLR_WHITE);
            print_centered("Z to retry", SCREEN_W/2, SCREEN_H/2+12, CLR_LIGHT_GREY);
        } else {
            print_centered("GAME OVER", SCREEN_W/2, SCREEN_H/2-12, CLR_RED);
            print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H/2, CLR_YELLOW);
            print_centered("Z to restart", SCREEN_W/2, SCREEN_H/2+12, CLR_LIGHT_GREY);
        }
    }
}
