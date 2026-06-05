// HIGH NOON — a quick-draw reaction duel. Wait for DRAW!... first trigger wins.
//
// Every big word on screen was baked from the Google Font "Smokum" (the
// wanted-poster slab serif) at BUILD time by tools/font-bake.js — see
// highnoon.cart.js. The words are the feedback: DRAW! is the start signal,
// TOO SOON / DEAD / YOU WIN the verdicts. Baked sheet regions (all sspr'd 2x):
//   (0,  0) 128x32 HIGH NOON   (0, 32) 128x32 DRAW!     (0, 96) 128x32 YOU WIN
//   (0, 64)  64x32 DEAD        (64,64)  64x32 EARLY!
//
// Z draws. Beat all five outlaws — each one's trigger finger is faster.
#include "studio.h"
#include <math.h>
#include <stdio.h>

enum { ST_TITLE, ST_INTRO, ST_STANDOFF, ST_DRAW, ST_WIN, ST_DEAD, ST_SOON, ST_CHAMP };

typedef struct { const char *name; int ms; } Outlaw;
static const Outlaw outlaws[5] = {
    { "SLOW JOE",        600 },
    { "MEAN SUE",        450 },
    { "EL RAPIDO",       340 },
    { "THE UNDERTAKER",  270 },
    { "DEATH HIMSELF",   215 },
};

static int st = ST_TITLE;
static int tmr;            // frames in current state
static int duelist;        // which outlaw (0..4)
static int wait_f;         // standoff delay before DRAW!
static int enemy_f;        // outlaw reaction in frames
static int react_f;        // player reaction in frames (set on win)
static float p_arm, e_arm; // 0 = holstered, 1 = leveled
static int p_fall, e_fall; // 0 = standing, counts up while falling
static int flash;          // white-flash frames
static int muzzle;         // muzzle-flash frames (+ = player, - = enemy)
static float tumble_x = -20;

void init(void) {
    colorkey(0);            // baked banners: palette 0 around the words = transparent
}

static void enter(int s) { st = s; tmr = 0; }

static void start_duel(void) {
    p_arm = e_arm = 0; p_fall = e_fall = 0; muzzle = 0;
    wait_f = rnd_between(70, 240);                  // 1.2s .. 4s of sweat
    enemy_f = outlaws[duelist].ms * 60 / 1000;
    enter(ST_STANDOFF);
}

static void gunshot(void) {
    hit(28, INSTR_NOISE, 7, 80);
    schedule_hit(60, 24, INSTR_NOISE, 4, 160);      // the echo off the canyon
    shake(5);
    flash = 2;                                      // shorter than the muzzle flash
}

void update(void) {
    tmr++;
    if (flash > 0) flash--;
    if (muzzle > 0) muzzle--;
    if (muzzle < 0) muzzle++;
    if (p_fall > 0 && p_fall < 30) p_fall++;
    if (e_fall > 0 && e_fall < 30) e_fall++;
    // your gun comes up only when YOU fire (win, or the shameful early shot)
    p_arm = lerp(p_arm, (st == ST_WIN || st == ST_SOON) && !p_fall ? 1.0f : 0.0f, 0.5f);
    e_arm = lerp(e_arm, (st == ST_DRAW && tmr >= enemy_f) || st == ST_DEAD ? 1.0f : 0.0f, 0.5f);

    switch (st) {
    case ST_TITLE:
        if (btnp(0, BTN_A)) { duelist = 0; enter(ST_INTRO); }
        break;
    case ST_INTRO:
        if (tmr > 110) start_duel();
        break;
    case ST_STANDOFF:
        tumble_x += 0.6f;
        if (tumble_x > SCREEN_W + 20) tumble_x = -20;
        if (tmr % 45 == 0) hit(78, INSTR_TRI, 2, 30);          // the clock ticks
        if (btnp(0, BTN_A)) {                                   // jumped the gun
            gunshot(); muzzle = 6; enter(ST_SOON);
        } else if (tmr >= wait_f) {
            hit(96, INSTR_SQUARE, 6, 50); flash = 2;            // DRAW!
            enter(ST_DRAW);
        }
        break;
    case ST_DRAW:
        if (btnp(0, BTN_A)) {                                   // you fired
            react_f = tmr; gunshot(); muzzle = 6; e_fall = 1;
            schedule_hit(350, 33, INSTR_NOISE, 4, 150);         // body hits dirt
            enter(ST_WIN);
        } else if (tmr > enemy_f + 2) {                         // they fired
            gunshot(); muzzle = -6; p_fall = 1;
            enter(ST_DEAD);
        }
        break;
    case ST_WIN:
        if (tmr > 50 && btnp(0, BTN_A)) {
            duelist++;
            if (duelist >= 5) {
                schedule_hit(0, 60, INSTR_TRI, 6, 150);         // the ballad of you
                schedule_hit(180, 64, INSTR_TRI, 6, 150);
                schedule_hit(360, 67, INSTR_TRI, 6, 150);
                schedule_hit(540, 72, INSTR_TRI, 7, 400);
                enter(ST_CHAMP);
            } else enter(ST_INTRO);
        }
        break;
    case ST_DEAD:
    case ST_SOON:
        if (tmr > 50 && btnp(0, BTN_A)) enter(ST_INTRO);        // same outlaw again
        break;
    case ST_CHAMP:
        if (btnp(0, BTN_A)) enter(ST_TITLE);
        break;
    }

#ifdef DE_TRACE
    watch("duel", "st=%d tmr=%d duelist=%d wait=%d react=%d", st, tmr, duelist, wait_f, react_f);
#endif
}

// black silhouette gunslinger. dir +1 faces right. gy = ground (feet) line.
// arm 0..1 holstered->leveled; fall 0 standing, 1..29 sinking, 30 in the dirt.
static void cowboy(int x, int gy, int dir, float arm, int fall, bool flash_muzzle) {
    if (fall >= 15) {                                           // in the dirt
        rectfill(x - 10, gy - 4, 22, 4, CLR_BLACK);             // body, horizontal
        circfill(x + dir * 13, gy - 3, 3, CLR_BLACK);         // head
        rectfill(x + dir * 16 - 4, gy - 2, 9, 2, CLR_BLACK);    // hat, knocked off
        return;
    }
    int sink = fall;                                            // knees buckle first
    int y = gy - 34 + sink;                                     // top of figure
    rectfill(x - 4, gy - 14 + sink / 2, 3, 14 - sink / 2, CLR_BLACK);  // legs
    rectfill(x + 2, gy - 14 + sink / 2, 3, 14 - sink / 2, CLR_BLACK);
    rectfill(x - 5, y + 9, 11, 12, CLR_BLACK);                  // torso + duster
    rectfill(x - 6, y + 17, 13, 4, CLR_BLACK);                  // coat flare
    circfill(x, y + 5, 3, CLR_BLACK);                         // head
    rectfill(x - 7, y + 1, 15, 2, CLR_BLACK);                   // hat brim
    rectfill(x - 4, y - 3, 9, 4, CLR_BLACK);                    // hat crown
    // gun arm: swings from straight down to leveled toward dir
    int ax = x + dir * (int)(3 + 9 * arm);
    int ay = y + 12 + (int)(7 * (1 - arm));
    thickline(x + dir * 3, y + 11, ax, ay, 2, CLR_BLACK);
    if (arm > 0.5f) rectfill(dir > 0 ? ax : ax - 4, ay - 1, 4, 2, CLR_BLACK);  // the iron
    if (flash_muzzle) {
        circfill(ax + dir * 6, ay, 3, CLR_YELLOW);
        circfill(ax + dir * 6, ay, 1, CLR_WHITE);
    }
}

static void scene(void) {
    int hz = 132;
    vgradient(0, 0, SCREEN_W, 60, CLR_YELLOW, CLR_ORANGE);      // sunset sky
    vgradient(0, 60, SCREEN_W, hz - 60, CLR_ORANGE, CLR_DARK_PURPLE);
    circfill(160, hz - 4, 24, CLR_LIGHT_YELLOW);              // sun on the horizon
    rectfill(0, hz, SCREEN_W, SCREEN_H - hz, CLR_BROWNISH_BLACK);
    line(0, hz, SCREEN_W, hz, CLR_DARK_BROWN);
    // cacti
    rectfill(24, hz - 18, 4, 18, CLR_BLACK); rectfill(18, hz - 12, 6, 3, CLR_BLACK);
    rectfill(18, hz - 16, 3, 6, CLR_BLACK);  rectfill(292, hz - 13, 4, 13, CLR_BLACK);
    rectfill(296, hz - 9, 6, 3, CLR_BLACK);  rectfill(299, hz - 13, 3, 6, CLR_BLACK);
}

void draw(void) {
    if (flash > 0) { cls(CLR_WHITE); return; }
    scene();

    cowboy(74, 150, 1, p_arm, p_fall, muzzle > 0);
    cowboy(246, 150, -1, e_arm, e_fall, muzzle < 0);

    if (st == ST_TITLE) {
        sspr(0, 0, 128, 32, 32, 36, 256, 64);                   // HIGH NOON, 2x
        print_centered("A QUICK-DRAW DUEL", SCREEN_W / 2, 144, CLR_LIGHT_PEACH);
        print_centered("FIVE OUTLAWS. ONE BULLET EACH.", SCREEN_W / 2, 158, CLR_MEDIUM_GREY);
        if ((frame() / 24) % 2)
            print_centered("PRESS Z", SCREEN_W / 2, 180, CLR_WHITE);
        return;
    }

    char buf[64];
    switch (st) {
    case ST_INTRO:
        snprintf(buf, sizeof buf, "OUTLAW %d OF 5", duelist + 1);
        print_centered(buf, SCREEN_W / 2, 44, CLR_MEDIUM_GREY);
        print_centered(outlaws[duelist].name, SCREEN_W / 2, 58, CLR_WHITE);
        snprintf(buf, sizeof buf, "DRAWS IN %d.%03ds", outlaws[duelist].ms / 1000, outlaws[duelist].ms % 1000);
        print_centered(buf, SCREEN_W / 2, 72, CLR_LIGHT_PEACH);
        print_centered("WAIT FOR THE SIGNAL...", SCREEN_W / 2, 176, CLR_MEDIUM_GREY);
        break;
    case ST_STANDOFF: {
        // tumbleweed — it means nothing. don't shoot it.
        int tx = (int)tumble_x, ty = 146 + (int)(2 * sinf(tumble_x * 0.4f));
        circ(tx, ty, 5, CLR_DARK_BROWN);
        circ(tx + 1, ty - 1, 3, CLR_DARK_BROWN);
        if ((frame() / 30) % 2) print_centered("WAIT...", SCREEN_W / 2, 176, CLR_LIGHT_PEACH);
        break;
    }
    case ST_DRAW: {
        int pop = tmr < 3 ? (3 - tmr) * 8 : 0;                  // banner slams in
        sspr(0, 32, 128, 32, 32 - pop, 40 - pop / 2, 256 + pop * 2, 64 + pop);
        break;
    }
    case ST_WIN:
        sspr(0, 32, 128, 32, 32, 40, 256, 64);                  // DRAW! stays up
        if (tmr > 25) {
            snprintf(buf, sizeof buf, "YOU: 0.%03ds   %s: 0.%03ds",
                     react_f * 1000 / 60, outlaws[duelist].name, outlaws[duelist].ms);
            print_centered(buf, SCREEN_W / 2, 160, CLR_LIGHT_YELLOW);
            if (tmr > 50 && (frame() / 24) % 2)
                print_centered(duelist >= 4 ? "Z..." : "Z = NEXT OUTLAW", SCREEN_W / 2, 176, CLR_WHITE);
        }
        break;
    case ST_DEAD:
        sspr(0, 64, 64, 32, 96, 40, 128, 64);                   // DEAD
        snprintf(buf, sizeof buf, "%s WAS FASTER", outlaws[duelist].name);
        print_centered(buf, SCREEN_W / 2, 160, CLR_LIGHT_PEACH);
        if (tmr > 50 && (frame() / 24) % 2)
            print_centered("Z = TRY AGAIN", SCREEN_W / 2, 176, CLR_WHITE);
        break;
    case ST_SOON:
        sspr(64, 64, 64, 32, 96, 40, 128, 64);                  // EARLY!
        print_centered("YOU SHOT BEFORE THE SIGNAL", SCREEN_W / 2, 160, CLR_LIGHT_PEACH);
        if (tmr > 50 && (frame() / 24) % 2)
            print_centered("Z = TRY AGAIN", SCREEN_W / 2, 176, CLR_WHITE);
        break;
    case ST_CHAMP:
        sspr(0, 96, 128, 32, 32, 36, 256, 64);                  // YOU WIN
        print_centered("FASTEST GUN IN THE WEST", SCREEN_W / 2, 148, CLR_LIGHT_YELLOW);
        snprintf(buf, sizeof buf, "LAST DRAW: 0.%03ds", react_f * 1000 / 60);
        print_centered(buf, SCREEN_W / 2, 162, CLR_LIGHT_PEACH);
        if ((frame() / 24) % 2)
            print_centered("Z = RIDE AGAIN", SCREEN_W / 2, 176, CLR_WHITE);
        break;
    }
}
