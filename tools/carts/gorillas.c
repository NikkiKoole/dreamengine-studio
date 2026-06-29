/* de:meta
{
  "title": "gorillas",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "title-play-gameover-loop"
  ],
  "lineage": "Direct port of the QBasic Gorillas (1991) classic; adds wind-affected parabolic banana physics and a 4-state turn machine (aim → fly → post-explosion → gameover).",
  "genre": "strategy",
  "homage": "Gorillas (QBasic, 1991)",
  "description": "Classic QBasic Gorillas. Two gorillas on a city skyline take turns hurling exploding bananas. Adjust angle and power, account for wind. First to 3 hits wins. Left/right: angle — Up/down: power — Z: fire."
}
de:meta */
#include "studio.h"

// GORILLAS — 2-player banana-throwing game (QBasic classic)
// Both players share keyboard — pass it between turns.
// LEFT/RIGHT: adjust angle    UP/DOWN: adjust power    Z: fire!
// Wind blows the banana — watch the arrow. First to 3 hits wins!

#define N_BLDS      8
#define GOR_W       16
#define GOR_H       16
#define GRAV        0.10f
#define WIN_HITS    3
#define HUD_H       14    // top HUD bar height

static const int BCOLS[3] = { CLR_DARK_GREY, CLR_DARK_PURPLE, CLR_DARKER_BLUE };

typedef struct { int x, w, h, col; } Bld;
static Bld   blds[N_BLDS];

static int   gx[2], gy[2];   // gorilla: center-x, top-y
static int   pts[2];
static int   cur;             // whose turn: 0 or 1
static float ang[2], pwr[2]; // angle + power (persisted between turns)
static float wind;

static bool  flying;
static float bx, by, bvx, bvy;

static bool  exploding;
static float ex, ey, et;

// 0=aim  1=fly  2=post-explosion  3=gameover
static int gstate;

// ---- helpers ----
static bool ab(int b)  { return btn(0,b)  || btn(1,b);  }
static bool abp(int b) { return btnp(0,b) || btnp(1,b); }

static void gen_city() {
    int seg = SCREEN_W / N_BLDS;
    for (int i = 0; i < N_BLDS; i++) {
        blds[i].x   = i * seg + 1;
        blds[i].w   = seg - 2;
        blds[i].h   = 55 + rnd(100);
        blds[i].col = BCOLS[i % 3];
    }
}

static void place_gorillas() {
    gx[0] = blds[1].x + blds[1].w / 2;
    gy[0] = SCREEN_H - blds[1].h - GOR_H;
    gx[1] = blds[N_BLDS-2].x + blds[N_BLDS-2].w / 2;
    gy[1] = SCREEN_H - blds[N_BLDS-2].h - GOR_H;
}

static void do_explode(float x, float y) {
    ex = x; ey = y; et = now();
    exploding = true;
    flying = false;
}

static void fire() {
    // player 0 throws right, player 1 mirrors the angle left
    float a = (cur == 0) ? ang[cur] : 180.0f - ang[cur];
    bx  = (float)gx[cur];
    by  = (float)(gy[cur] + GOR_H / 2);
    bvx = cos_deg(a) * pwr[cur] * 0.05f;
    bvy = -sin_deg(a) * pwr[cur] * 0.05f;
    flying = true;
    gstate = 1;
    note(65, INSTR_SQUARE, 3);
}

static void new_round() {
    gen_city();
    place_gorillas();
    wind = rnd_float_between(-12.0f, 12.0f);
    flying = false;
    exploding = false;
    gstate = 0;
}

static void new_game() {
    pts[0] = pts[1] = 0;
    cur    = 0;
    ang[0] = 45.0f;  ang[1] = 45.0f;
    pwr[0] = 90.0f;  pwr[1] = 90.0f;
    new_round();
}

void init() {
    colorkey(0);
    new_game();
}

void update() {
    if (gstate == 3) {
        if (now() - et > 1.0f && (abp(BTN_A) || abp(BTN_B))) new_game();
        return;
    }

    if (gstate == 0) {
        if (ab(BTN_LEFT))  ang[cur] = clamp(ang[cur] - 1.5f, 1.0f, 89.0f);
        if (ab(BTN_RIGHT)) ang[cur] = clamp(ang[cur] + 1.5f, 1.0f, 89.0f);
        if (ab(BTN_UP))    pwr[cur] = clamp(pwr[cur] + 1.5f, 5.0f, 200.0f);
        if (ab(BTN_DOWN))  pwr[cur] = clamp(pwr[cur] - 1.5f, 5.0f, 200.0f);
        if (abp(BTN_A) || abp(BTN_B)) fire();
        return;
    }

    if (gstate == 1) {
        bvx += wind * 0.0005f;
        bvy += GRAV;
        bx  += bvx;
        by  += bvy;

        // hit opponent gorilla?
        int opp = 1 - cur;
        if (circles_touch((int)bx, (int)by, 3, gx[opp], gy[opp]+GOR_H/2, 10)) {
            do_explode(bx, by);
            note(48, INSTR_NOISE, 7);
            pts[cur]++;
            gstate = (pts[cur] >= WIN_HITS) ? 3 : 2;
            return;
        }

        // hit a building?
        for (int i = 0; i < N_BLDS; i++) {
            if (bx >= blds[i].x && bx <= blds[i].x + blds[i].w &&
                by >= SCREEN_H - blds[i].h) {
                do_explode(bx, by);
                note(36, INSTR_NOISE, 4);
                gstate = 2;
                return;
            }
        }

        // hit ground / went off bottom?
        if (by > SCREEN_H) { do_explode(bx, SCREEN_H); note(36, INSTR_NOISE, 4); gstate = 2; return; }

        // flew off screen sides → miss, switch turns
        if (bx < -30 || bx > SCREEN_W + 30) {
            flying = false;
            cur = 1 - cur;
            gstate = 0;
        }
        return;
    }

    if (gstate == 2) {
        if (now() - et > 0.7f) {
            exploding = false;
            cur = 1 - cur;
            gstate = 0;
        }
    }
}

void draw() {
    cls(CLR_DARK_BLUE);

    // fixed star field (deterministic)
    for (int i = 0; i < 30; i++) {
        int sx = (i * 127 + 13) % SCREEN_W;
        int sy = HUD_H + 4 + (i * 53 + 5) % (SCREEN_H - HUD_H - 70);
        pset(sx, sy, (i%5==0) ? CLR_LIGHT_GREY : CLR_WHITE);
    }

    // buildings + windows
    for (int i = 0; i < N_BLDS; i++) {
        int ty = SCREEN_H - blds[i].h;
        rectfill(blds[i].x, ty, blds[i].w, blds[i].h, blds[i].col);
        for (int wy = ty + 4; wy < SCREEN_H - 4; wy += 8) {
            for (int wx = blds[i].x + 2; wx < blds[i].x + blds[i].w - 2; wx += 7) {
                // deterministic lit/unlit pattern per window
                if ((i*7 + (wx/7)*13 + (wy/8)*17) % 5 != 0)
                    rectfill(wx, wy, 4, 5, CLR_YELLOW);
            }
        }
    }

    // gorillas
    for (int p = 0; p < 2; p++) {
        spr(1, gx[p] - GOR_W/2, gy[p]);
    }

    // dashed aim line (current player only, while aiming)
    if (gstate == 0) {
        float a = (cur == 0) ? ang[cur] : 180.0f - ang[cur];
        for (int d = 6; d <= 28; d += 5) {
            int px = gx[cur] + (int)(cos_deg(a) * d);
            int py = gy[cur] + GOR_H/2 - (int)(sin_deg(a) * d);
            pset(px, py, CLR_WHITE);
        }
    }

    // banana in flight
    if (flying) {
        circfill((int)bx, (int)by, 2, CLR_YELLOW);
        pset((int)bx, (int)by, CLR_ORANGE);
    }

    // explosion (during post-explosion state AND during game-over win moment)
    if (exploding || gstate == 3) {
        float age = now() - et;
        int r = min((int)(age * 80), 28);
        if (r > 0) {
            circfill((int)ex, (int)ey, r,      CLR_ORANGE);
            circfill((int)ex, (int)ey, r*2/3,  CLR_YELLOW);
            circfill((int)ex, (int)ey, r/3,    CLR_WHITE);
        }
    }

    // ---- HUD top bar ----
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);

    // player scores (active player highlighted green)
    int c0 = (cur==0 && gstate==0) ? CLR_GREEN : CLR_WHITE;
    int c1 = (cur==1 && gstate==0) ? CLR_GREEN : CLR_WHITE;
    print(str("P1 %d", pts[0]), 4, 3, c0);
    print_right(str("P2 %d", pts[1]), SCREEN_W-4, 3, c1);

    // wind arrow (centered)
    int wmid = SCREEN_W/2;
    print_centered("WIND", SCREEN_W/2, 3, CLR_DARK_GREY);
    int wlen = (int)(wind * 3.0f);
    if (wlen != 0) {
        line(wmid+12, 7, wmid+12+wlen, 7, CLR_LIGHT_GREY);
        int tip = wmid + 12 + wlen;
        int dir = (wlen > 0) ? -1 : 1;
        pset(tip, 6, CLR_LIGHT_GREY);
        pset(tip, 8, CLR_LIGHT_GREY);
        pset(tip+dir, 7, CLR_LIGHT_GREY);
    }

    // bottom status bar when aiming
    if (gstate == 0) {
        int bc = (cur == 0) ? CLR_GREEN : CLR_PINK;
        rectfill(0, SCREEN_H-10, SCREEN_W, 10, CLR_BLACK);
        print(str("P%d  ANGLE:%d  POWER:%d  [Z]=fire",
              cur+1, (int)ang[cur], (int)pwr[cur]), 4, SCREEN_H-8, bc);
    }

    // game over overlay (shown 1s after winning explosion)
    if (gstate == 3 && now() - et > 1.0f) {
        int w = (pts[0] >= WIN_HITS) ? 1 : 2;
        rectfill(SCREEN_W/2-76, SCREEN_H/2-20, 152, 42, CLR_BLACK);
        rect    (SCREEN_W/2-76, SCREEN_H/2-20, 152, 42, CLR_YELLOW);
        print_centered(str("PLAYER %d WINS!", w), SCREEN_W/2, SCREEN_H/2-10, CLR_YELLOW);
        print_centered("Z to play again", SCREEN_W/2, SCREEN_H/2+6, CLR_LIGHT_GREY);
    }
}
