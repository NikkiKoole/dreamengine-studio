/* de:meta
{
  "title": "paratrooper",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop"
  ],
  "lineage": "Homage to Paratrooper (1982); a faithful port of the classic arcade loop — helicopters drop paratroopers, the gun rotates to shoot — with no conceptual additions beyond the original.",
  "genre": "shooter",
  "homage": "Paratrooper (1982)",
  "description": "Rotate your AA gun and shoot down paratroopers before they capture it. Helicopters drop soldiers — shoot them in the air or on the ground. Game over when 2 reach each side of your gun. Left/right to aim, Z to fire."
}
de:meta */
#include "studio.h"

// PARATROOPER
// Left/Right: rotate gun   Z or X: fire
// Shoot paratroopers before they capture the gun from both sides!

#define GUN_X       (SCREEN_W / 2)
#define GROUND_Y    (SCREEN_H - 18)
#define GUN_CY      (GROUND_Y - 6)
#define BARREL_LEN  17
#define MAX_ANG     82.0f

#define MAX_HELIS    3
#define MAX_PARAS   16
#define MAX_WALKERS 16
#define MAX_BULS     6

typedef struct { float x, y, vx; bool on; int drop; int wait; } Heli;
typedef struct { float x, y;     bool on; bool open;           } Para;
typedef struct { float x;        bool on;                      } Walker;
typedef struct { float x, y, vx, vy; bool on;                  } Bul;

static float   gun_ang;
static Heli    helis[MAX_HELIS];
static Para    paras[MAX_PARAS];
static Walker  walkers[MAX_WALKERS];
static Bul     buls[MAX_BULS];
static int     score;
static bool    gameover;
static int     fire_cool;

// ---- setup ----

static void spawn_heli(int i) {
    bool left = rnd(2);
    float spd = 1.0f + rnd_float() + score / 1500.0f;
    helis[i].x    = left ? -20.0f : SCREEN_W + 20.0f;
    helis[i].y    = 12.0f + rnd(50);
    helis[i].vx   = left ? spd : -spd;
    helis[i].on   = true;
    helis[i].drop = 70 + rnd(90);
    helis[i].wait = 0;
}

static void new_game() {
    gun_ang   = -90.0f;
    score     = 0;
    gameover  = false;
    fire_cool = 0;
    for (int i=0;i<MAX_HELIS;  i++) { helis[i].on=false;   helis[i].wait=0; }
    for (int i=0;i<MAX_PARAS;  i++) paras[i].on=false;
    for (int i=0;i<MAX_WALKERS;i++) walkers[i].on=false;
    for (int i=0;i<MAX_BULS;   i++) buls[i].on=false;
    spawn_heli(0);
    spawn_heli(1);
}

void init() { new_game(); }

static void drop_para(float x, float y) {
    for (int i=0;i<MAX_PARAS;i++)
        if (!paras[i].on) { paras[i].x=x; paras[i].y=y; paras[i].on=true; paras[i].open=false; return; }
}

static void land_walker(float x) {
    for (int i=0;i<MAX_WALKERS;i++)
        if (!walkers[i].on) { walkers[i].x=x; walkers[i].on=true; return; }
}

// ---- update ----

void update() {
    if (gameover) {
        if (btnp(0,BTN_A)||btnp(0,BTN_B)) new_game();
        return;
    }

    // rotate gun
    if (btn(0,BTN_LEFT))  gun_ang -= 3.0f;
    if (btn(0,BTN_RIGHT)) gun_ang += 3.0f;
    gun_ang = clamp(gun_ang, -90.0f-MAX_ANG, -90.0f+MAX_ANG);

    // fire
    if (fire_cool > 0) fire_cool--;
    if ((btn(0,BTN_A)||btn(0,BTN_B)) && fire_cool==0) {
        for (int i=0;i<MAX_BULS;i++) {
            if (!buls[i].on) {
                buls[i].x  = GUN_X + dx(BARREL_LEN, gun_ang);
                buls[i].y  = GUN_CY + dy(BARREL_LEN, gun_ang);
                buls[i].vx = dx(9.0f, gun_ang);
                buls[i].vy = dy(9.0f, gun_ang);
                buls[i].on = true;
                fire_cool  = 5;
                hit(72, INSTR_SQUARE, 3, 35);
                break;
            }
        }
    }

    // move bullets
    for (int i=0;i<MAX_BULS;i++) {
        if (!buls[i].on) continue;
        buls[i].x += buls[i].vx;
        buls[i].y += buls[i].vy;
        if (buls[i].x<0||buls[i].x>SCREEN_W||buls[i].y<0||buls[i].y>SCREEN_H)
            buls[i].on=false;
    }

    // helicopters
    for (int i=0;i<MAX_HELIS;i++) {
        if (!helis[i].on) {
            if (i==2 && score<400) continue;
            if (++helis[i].wait > 200) spawn_heli(i);
            continue;
        }
        helis[i].x += helis[i].vx;
        if (--helis[i].drop <= 0) {
            drop_para(helis[i].x, helis[i].y + 8);
            helis[i].drop = 60 + rnd(80);
            note(52, INSTR_NOISE, 1);
        }
        if (helis[i].x < -35 || helis[i].x > SCREEN_W+35)
            { helis[i].on=false; helis[i].wait=0; }
        // bullet collision
        for (int j=0;j<MAX_BULS;j++) {
            if (!buls[j].on) continue;
            if (near((int)buls[j].x,(int)buls[j].y,(int)helis[i].x,(int)helis[i].y, 14)) {
                helis[i].on=false; helis[i].wait=0;
                buls[j].on=false;
                score += 150;
                note(36, INSTR_NOISE, 6);
            }
        }
    }

    // parachutists in air
    for (int i=0;i<MAX_PARAS;i++) {
        if (!paras[i].on) continue;
        paras[i].y += paras[i].open ? 0.65f : 2.5f;
        if (!paras[i].open && paras[i].y > 36) paras[i].open = true;
        // landed
        if (paras[i].y >= GROUND_Y - 14) {
            paras[i].on = false;
            land_walker(paras[i].x);
            note(44, INSTR_NOISE, 3);
        }
        // bullet hit — large radius covers both canopy and figure
        for (int j=0;j<MAX_BULS;j++) {
            if (!buls[j].on) continue;
            if (near((int)buls[j].x,(int)buls[j].y,(int)paras[i].x,(int)paras[i].y, 16)) {
                paras[i].on=false; buls[j].on=false;
                score += 25;
                note(60, INSTR_NOISE, 3);
            }
        }
    }

    // walkers on the ground
    int left_close=0, right_close=0;
    for (int i=0;i<MAX_WALKERS;i++) {
        if (!walkers[i].on) continue;
        float dist = walkers[i].x - GUN_X;
        walkers[i].x += dist < 0 ? 0.5f : -0.5f;
        if (dist < -2  && dist > -28) left_close++;
        if (dist >  2  && dist <  28) right_close++;
        // bullet hit
        for (int j=0;j<MAX_BULS;j++) {
            if (!buls[j].on) continue;
            if (near((int)buls[j].x,(int)buls[j].y,(int)walkers[i].x, GROUND_Y-8, 8)) {
                walkers[i].on=false; buls[j].on=false;
                score += 50;
                note(65, INSTR_NOISE, 3);
            }
        }
    }
    // capture: 2+ walkers close on each side simultaneously
    if (left_close >= 2 && right_close >= 2) {
        gameover = true;
        note(24, INSTR_NOISE, 7);
    }
}

// ---- draw helpers ----

static void draw_heli(int x, int y) {
    // main body
    rectfill(x-11, y-3, 22, 8, CLR_DARK_GREY);
    // cockpit bump + window
    rectfill(x-8,  y-7, 12, 6, CLR_DARK_GREY);
    rectfill(x-6,  y-6, 8,  4, CLR_BLUE);
    // tail boom + tail rotor
    line(x+11, y,   x+18, y-7, CLR_DARK_GREY);
    line(x+17, y-9, x+17, y-5, CLR_LIGHT_GREY);
    // main rotor (3-frame spin)
    int r = (frame()/3)%3;
    if      (r==0) line(x-15, y-8, x+15, y-8, CLR_LIGHT_GREY);
    else if (r==1) line(x-11, y-4, x+11, y-12, CLR_LIGHT_GREY);
    else           line(x-11, y-12, x+11, y-4, CLR_LIGHT_GREY);
}

static void draw_para(int x, int y, bool open) {
    if (open) {
        // canopy — 4 line arc
        line(x-12, y-4,  x-6,  y-12, CLR_WHITE);
        line(x-6,  y-12, x,    y-15, CLR_WHITE);
        line(x,    y-15, x+6,  y-12, CLR_WHITE);
        line(x+6,  y-12, x+12, y-4,  CLR_WHITE);
        // strings
        line(x-12, y-4, x-1, y+3, CLR_LIGHT_GREY);
        line(x+12, y-4, x+1, y+3, CLR_LIGHT_GREY);
    }
    // figure
    circfill(x, y+2, 2, CLR_LIGHT_PEACH);
    line(x,   y+4, x,   y+9,  CLR_WHITE);
    line(x-3, y+6, x+3, y+6,  CLR_LIGHT_GREY);
    line(x,   y+9, x-2, y+13, CLR_WHITE);
    line(x,   y+9, x+2, y+13, CLR_WHITE);
}

static void draw_walker(int x) {
    int y  = GROUND_Y - 13;
    int lg = (frame()/6)%2;
    circfill(x, y, 2, CLR_LIGHT_PEACH);
    line(x, y+2, x, y+7,  CLR_WHITE);
    line(x-3, y+4, x+3, y+4, CLR_LIGHT_GREY);
    if (lg) { line(x, y+7, x-3, y+12, CLR_WHITE); line(x, y+7, x+2, y+12, CLR_WHITE); }
    else    { line(x, y+7, x+3, y+12, CLR_WHITE); line(x, y+7, x-2, y+12, CLR_WHITE); }
}

void draw() {
    cls(CLR_DARK_BLUE);

    // stars
    for (int i=0;i<35;i++) {
        int sx=(i*97+11)%SCREEN_W, sy=(i*73+7)%85;
        pset(sx, sy, i%5==0 ? CLR_WHITE : CLR_DARK_GREY);
    }

    // ground strip
    rectfill(0, GROUND_Y, SCREEN_W, SCREEN_H-GROUND_Y, CLR_DARK_GREEN);
    line(0, GROUND_Y, SCREEN_W, GROUND_Y, CLR_GREEN);

    // helicopters
    for (int i=0;i<MAX_HELIS;  i++) if (helis[i].on)   draw_heli((int)helis[i].x,(int)helis[i].y);
    for (int i=0;i<MAX_PARAS;  i++) if (paras[i].on)   draw_para((int)paras[i].x,(int)paras[i].y, paras[i].open);
    for (int i=0;i<MAX_WALKERS;i++) if (walkers[i].on) draw_walker((int)walkers[i].x);

    // bullets
    for (int i=0;i<MAX_BULS;i++)
        if (buls[i].on) circfill((int)buls[i].x,(int)buls[i].y, 2, CLR_YELLOW);

    // gun base + turret
    rectfill(GUN_X-14, GROUND_Y-8, 28,  8, CLR_DARK_GREY);
    rectfill(GUN_X-10, GROUND_Y-14, 20, 8, CLR_MEDIUM_GREY);
    // barrel — two parallel lines for thickness
    int tx = GUN_X + (int)dx(BARREL_LEN, gun_ang);
    int ty = GUN_CY + (int)dy(BARREL_LEN, gun_ang);
    line(GUN_X-1, GUN_CY, tx-1, ty, CLR_LIGHT_GREY);
    line(GUN_X,   GUN_CY, tx,   ty, CLR_WHITE);
    line(GUN_X+1, GUN_CY, tx+1, ty, CLR_LIGHT_GREY);

    // HUD
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    // brief control hint
    if (frame() < 360)
        print("left/right: aim   Z: fire", SCREEN_W/2-72, 3, CLR_DARK_GREY);

    if (gameover) {
        rectfill(SCREEN_W/2-68, SCREEN_H/2-22, 136, 52, CLR_BLACK);
        rect    (SCREEN_W/2-68, SCREEN_H/2-22, 136, 52, CLR_RED);
        print_centered("GUN CAPTURED!", SCREEN_W/2, SCREEN_H/2-12, CLR_RED);
        print_centered(str("SCORE %d",score), SCREEN_W/2, SCREEN_H/2+2, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H/2+14, CLR_LIGHT_GREY);
    }
}
