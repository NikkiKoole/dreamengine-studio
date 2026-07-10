/* de:meta
{
  "slug": "gta",
  "title": "GTA",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "game"
  ],
  "teaches": [
    "camera-follow",
    "tilemap-collision",
    "no-sprite-vehicles"
  ],
  "lineage": "Top-down homage to Grand Theft Auto (1997) — combines angular car physics (heading + speed → AABB wall-slide) with a wanted-star escalation loop and pedestrian random-walk AI.",
  "genre": "sandbox",
  "homage": "Grand Theft Auto (1997)",
  "description": "Top-down open world sandbox. Walk and shoot twin-stick, or jack a car and run people over. 5 stars = BUSTED. WASD: move/drive — Arrows: aim+shoot — Z: enter/exit car."
}
de:meta */
#include "studio.h"

// GTA — top-down open world sandbox
// WASD: move (on foot) / drive (in car)
// Arrows: aim + auto-shoot
// Z: enter / exit car

// World: 40×25 cells of 16px = 640×400 px
#define WC     CELL_W          // cell pixel size (16)
#define WMAP_W 40
#define WMAP_H 25
#define WW     (WMAP_W * WC)
#define WH     (WMAP_H * WC)

// ── objects ──────────────────────────────────────────────────────
#define MAX_CARS 6
#define MAX_PEDS 12
#define MAX_BULS 10

typedef struct { float x, y, ang, spd; int color; bool driven; } Car;
typedef struct { float x, y, ang; bool alive; int timer, color; } Ped;
typedef struct { float x, y, vx, vy; bool on; int age; } Bul;

static Car  cars[MAX_CARS];
static Ped  peds[MAX_PEDS];
static Bul  buls[MAX_BULS];

// ── player ───────────────────────────────────────────────────────
static float px, py, pang;
static bool  in_car;
static int   car_idx;
static int   shoot_cd;
static int   shoot_dx, shoot_dy;

// ── game state ───────────────────────────────────────────────────
static int   score, hiscore, stars;
static float last_kill_t;
static bool  busted;
static float bust_t;

// ── helpers ──────────────────────────────────────────────────────

static bool is_wall_px(float wx, float wy) {
    if (wx < 0 || wy < 0 || wx >= WW || wy >= WH) return true;
    return mget((int)(wx / WC), (int)(wy / WC)) != 0;
}

static bool road_pos(float x, float y) {
    return mget((int)(x / WC), (int)(y / WC)) == 0
        && x > 0 && x < WW - 1 && y > 0 && y < WH - 1;
}

static void rand_road(float *ox, float *oy) {
    for (int t = 0; t < 600; t++) {
        float x = 2 + rnd(WW - 4);
        float y = 2 + rnd(WH - 4);
        if (road_pos(x, y)) { *ox = x; *oy = y; return; }
    }
    *ox = 5.0f * WC;
    *oy = 11.5f * WC;
}

static void kill_ped(int i, int pts) {
    peds[i].alive = false;
    score += pts;
    stars = min(stars + 1, 5);
    last_kill_t = now();
    if (!busted) hit(48, INSTR_NOISE, 5, 180);
}

// ── init ─────────────────────────────────────────────────────────

static void reset_game() {
    // parking spots on vertical/horizontal road intersections
    static const float SP[MAX_CARS][2] = {
        {12.5f*WC,  2.5f*WC},
        {25.5f*WC,  2.5f*WC},
        { 5.0f*WC, 11.5f*WC},
        {20.0f*WC, 11.5f*WC},
        {33.0f*WC, 11.5f*WC},
        {12.5f*WC, 18.0f*WC},
    };
    static const int COL[MAX_CARS] = {
        CLR_RED, CLR_BLUE, CLR_GREEN, CLR_YELLOW, CLR_PINK, CLR_ORANGE
    };
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].x = SP[i][0]; cars[i].y = SP[i][1];
        cars[i].ang = 0; cars[i].spd = 0;
        cars[i].color = COL[i]; cars[i].driven = false;
    }

    static const int PCOL[4] = {CLR_LIGHT_PEACH, CLR_YELLOW, CLR_PINK, CLR_ORANGE};
    for (int i = 0; i < MAX_PEDS; i++) {
        rand_road(&peds[i].x, &peds[i].y);
        peds[i].ang = rnd(360); peds[i].alive = true;
        peds[i].timer = 30 + rnd(90); peds[i].color = PCOL[i % 4];
    }

    px = 5.0f*WC; py = 11.5f*WC; pang = 0;
    in_car = false; car_idx = -1;
    shoot_cd = 0; shoot_dx = 1; shoot_dy = 0;
    stars = 0; score = 0; busted = false; last_kill_t = now();
}

void init() {
    hiscore = load(0);
    reset_game();
}

// ── update ───────────────────────────────────────────────────────

void update() {
    // busted screen
    if (busted) {
        if (now() - bust_t > 3.5f) {
            if (score > hiscore) { hiscore = score; save(0, hiscore); }
            reset_game();
        }
        follow((int)px, (int)py, WW, WH);
        return;
    }

    // wanted star decay (1 star per 10s of quiet, but not at 5 stars)
    if (stars > 0 && stars < 5 && now() - last_kill_t > 10.0f) {
        stars--;
        last_kill_t = now();
    }

    // busted after 5 seconds at 5 stars
    if (stars >= 5 && now() - last_kill_t > 5.0f) {
        busted = true; bust_t = now();
        note(36, INSTR_NOISE, 7);
        follow((int)px, (int)py, WW, WH);
        return;
    }

    // ── shoot input (arrows = player 1) ──────────────────────────
    int sx = 0, sy = 0;
    if (btn(1, BTN_UP))    sy = -1;
    if (btn(1, BTN_DOWN))  sy =  1;
    if (btn(1, BTN_LEFT))  sx = -1;
    if (btn(1, BTN_RIGHT)) sx =  1;
    if (sx || sy) { shoot_dx = sx; shoot_dy = sy; }
    if (shoot_cd > 0) shoot_cd--;

    if (!in_car) {
        // ── on foot (WASD = player 0) ─────────────────────────────
        float mvx = 0, mvy = 0, spd = 1.5f;
        if (btn(0, BTN_UP))    mvy -= spd;
        if (btn(0, BTN_DOWN))  mvy += spd;
        if (btn(0, BTN_LEFT))  mvx -= spd;
        if (btn(0, BTN_RIGHT)) mvx += spd;

        // face shoot direction, fall back to move direction
        if (sx || sy) {
            if      (sx== 1&&sy== 0) pang=  0;
            else if (sx== 1&&sy== 1) pang= 45;
            else if (sx== 0&&sy== 1) pang= 90;
            else if (sx==-1&&sy== 1) pang=135;
            else if (sx==-1&&sy== 0) pang=180;
            else if (sx==-1&&sy==-1) pang=225;
            else if (sx== 0&&sy==-1) pang=270;
            else                     pang=315;
        } else if (mvx || mvy) {
            if      (mvx> 0&&mvy==0) pang=  0;
            else if (mvx> 0&&mvy> 0) pang= 45;
            else if (mvx==0&&mvy> 0) pang= 90;
            else if (mvx< 0&&mvy> 0) pang=135;
            else if (mvx< 0&&mvy==0) pang=180;
            else if (mvx< 0&&mvy< 0) pang=225;
            else if (mvx==0&&mvy< 0) pang=270;
            else                     pang=315;
        }

        // wall-sliding movement (player = 8×8 box)
        int r = 4;
        if (!touching_map((int)(px+mvx)-r, (int)py-r, r*2, r*2)) px += mvx;
        if (!touching_map((int)px-r, (int)(py+mvy)-r, r*2, r*2)) py += mvy;
        px = clamp(px, r+1, WW-r-1);
        py = clamp(py, r+1, WH-r-1);

        // enter car
        if (btnp(0, BTN_A)) {
            for (int i = 0; i < MAX_CARS; i++) {
                if (!cars[i].driven && near((int)px,(int)py,(int)cars[i].x,(int)cars[i].y, 22)) {
                    in_car = true; car_idx = i; cars[i].driven = true;
                    note(60, INSTR_SQUARE, 4);
                    break;
                }
            }
        }

        // shoot
        if ((sx || sy) && shoot_cd <= 0) {
            for (int i = 0; i < MAX_BULS; i++) {
                if (!buls[i].on) {
                    float n = (sx && sy) ? 0.707f : 1.0f;
                    buls[i].x  = px + shoot_dx * 8;
                    buls[i].y  = py + shoot_dy * 8;
                    buls[i].vx = shoot_dx * 5.5f * n;
                    buls[i].vy = shoot_dy * 5.5f * n;
                    buls[i].on = true; buls[i].age = 0;
                    hit(78, INSTR_SQUARE, 2, 35);
                    shoot_cd = 7;
                    break;
                }
            }
        }

    } else {
        // ── in car (WASD steers) ──────────────────────────────────
        Car *c = &cars[car_idx];
        float turn = 3.5f;

        if (btn(0, BTN_LEFT))  c->ang -= turn;
        if (btn(0, BTN_RIGHT)) c->ang += turn;
        if (btn(0, BTN_UP))        c->spd = clamp(c->spd + 0.18f, -4.5f, 4.5f);
        else if (btn(0, BTN_DOWN)) c->spd = clamp(c->spd - 0.35f, -2.0f, 4.5f);
        else c->spd *= 0.95f;

        float nmx = dx(c->spd, c->ang), nmy = dy(c->spd, c->ang);
        // AABB: 22×14 centered on car
        int hw = 11, hh = 7;
        if      (!touching_map((int)(c->x+nmx)-hw, (int)(c->y+nmy)-hh, hw*2, hh*2))
            { c->x += nmx; c->y += nmy; }
        else if (!touching_map((int)(c->x+nmx)-hw, (int)(c->y)-hh, hw*2, hh*2))
            { c->x += nmx; c->spd *= 0.6f; }
        else if (!touching_map((int)(c->x)-hw, (int)(c->y+nmy)-hh, hw*2, hh*2))
            { c->y += nmy; c->spd *= 0.6f; }
        else
            { c->spd *= -0.3f; }

        c->x = clamp(c->x, hw+1, WW-hw-1);
        c->y = clamp(c->y, hh+1, WH-hh-1);
        px = c->x; py = c->y; pang = c->ang;

        // run over peds
        for (int i = 0; i < MAX_PEDS; i++) {
            if (!peds[i].alive) continue;
            if (near((int)c->x,(int)c->y,(int)peds[i].x,(int)peds[i].y, 14))
                kill_ped(i, 50);
        }

        // exit car
        if (btnp(0, BTN_A)) {
            float ex = c->x + dx(22, c->ang + 90);
            float ey = c->y + dy(22, c->ang + 90);
            if (is_wall_px(ex, ey)) {
                ex = c->x + dx(22, c->ang - 90);
                ey = c->y + dy(22, c->ang - 90);
            }
            px = ex; py = ey;
            in_car = false; cars[car_idx].driven = false; car_idx = -1;
            note(55, INSTR_SQUARE, 3);
        }
    }

    // ── bullets ──────────────────────────────────────────────────
    for (int i = 0; i < MAX_BULS; i++) {
        if (!buls[i].on) continue;
        buls[i].x += buls[i].vx;
        buls[i].y += buls[i].vy;
        buls[i].age++;
        if (buls[i].age > 55 || is_wall_px(buls[i].x, buls[i].y))
            { buls[i].on = false; continue; }
        for (int j = 0; j < MAX_PEDS; j++) {
            if (!peds[j].alive) continue;
            if (near((int)buls[i].x,(int)buls[i].y,(int)peds[j].x,(int)peds[j].y, 6)) {
                kill_ped(j, 100);
                buls[i].on = false;
                break;
            }
        }
    }

    // ── pedestrians ──────────────────────────────────────────────
    for (int i = 0; i < MAX_PEDS; i++) {
        if (!peds[i].alive) continue;
        if (--peds[i].timer <= 0) {
            peds[i].ang = rnd(360);
            peds[i].timer = 40 + rnd(80);
        }
        float nx = peds[i].x + dx(0.5f, peds[i].ang);
        float ny = peds[i].y + dy(0.5f, peds[i].ang);
        if (!touching_map((int)nx-3, (int)ny-3, 6, 6) && nx>1 && nx<WW-2 && ny>1 && ny<WH-2)
            { peds[i].x = nx; peds[i].y = ny; }
        else
            { peds[i].ang = rnd(360); peds[i].timer = 20; }
    }

    follow((int)px, (int)py, WW, WH);
}

// ── draw helpers ─────────────────────────────────────────────────

static void draw_car(float cx, float cy, float ang, int color) {
    float hl = 10, hw = 6;
    float fa = ang, ra = ang + 90;
    float c0x = cx+dx(hl,fa)+dx(hw,ra), c0y = cy+dy(hl,fa)+dy(hw,ra);
    float c1x = cx+dx(hl,fa)-dx(hw,ra), c1y = cy+dy(hl,fa)-dy(hw,ra);
    float c2x = cx-dx(hl,fa)-dx(hw,ra), c2y = cy-dy(hl,fa)-dy(hw,ra);
    float c3x = cx-dx(hl,fa)+dx(hw,ra), c3y = cy-dy(hl,fa)+dy(hw,ra);
    line((int)c0x,(int)c0y,(int)c1x,(int)c1y, color);
    line((int)c1x,(int)c1y,(int)c2x,(int)c2y, color);
    line((int)c2x,(int)c2y,(int)c3x,(int)c3y, color);
    line((int)c3x,(int)c3y,(int)c0x,(int)c0y, color);
    // windshield
    int w1x = (int)(cx+dx(hl*0.45f,fa)+dx(hw*0.75f,ra));
    int w1y = (int)(cy+dy(hl*0.45f,fa)+dy(hw*0.75f,ra));
    int w2x = (int)(cx+dx(hl*0.45f,fa)-dx(hw*0.75f,ra));
    int w2y = (int)(cy+dy(hl*0.45f,fa)-dy(hw*0.75f,ra));
    line(w1x, w1y, w2x, w2y, CLR_BLUE);
}

static void draw_ped(int x, int y, int color) {
    circfill(x, y-4, 2, color);
    line(x, y-2, x, y+2, color);
    line(x, y-1, x-2, y+1, color);
    line(x, y-1, x+2, y+1, color);
    line(x, y+2, x-2, y+5, color);
    line(x, y+2, x+2, y+5, color);
}

// ── draw ─────────────────────────────────────────────────────────

void draw() {
    cls(CLR_DARK_GREY);
    map(0, 0, 0, 0, WMAP_W, WMAP_H);

    // pedestrians
    for (int i = 0; i < MAX_PEDS; i++)
        if (peds[i].alive) draw_ped((int)peds[i].x, (int)peds[i].y, peds[i].color);

    // cars
    for (int i = 0; i < MAX_CARS; i++)
        draw_car(cars[i].x, cars[i].y, cars[i].ang, cars[i].color);

    // player (on foot)
    if (!in_car) {
        int x = (int)px, y = (int)py;
        circfill(x, y, 4, CLR_WHITE);
        line(x, y, x + (int)dx(7, pang), y + (int)dy(7, pang), CLR_YELLOW);
    }

    // bullets
    for (int i = 0; i < MAX_BULS; i++) {
        if (!buls[i].on) continue;
        pset((int)buls[i].x,   (int)buls[i].y, CLR_YELLOW);
        pset((int)buls[i].x+1, (int)buls[i].y, CLR_WHITE);
    }

    // ── HUD (fixed — reset camera first) ─────────────────────────
    camera(0, 0);
    rectfill(0, SCREEN_H-12, SCREEN_W, 12, CLR_BROWNISH_BLACK);
    print(str("SCORE %d", score),     4,            SCREEN_H-10, CLR_WHITE);
    print_right(str("BEST %d", hiscore), SCREEN_W-4, SCREEN_H-10, CLR_YELLOW);

    // wanted stars (yellow filled squares)
    int sw = 7, gap = 2, total = stars * (sw+gap);
    int sx0 = SCREEN_W/2 - total/2;
    for (int i = 0; i < 5; i++) {
        int col = (i < stars) ? CLR_YELLOW : CLR_DARK_GREY;
        rectfill(sx0 + i*(sw+gap), SCREEN_H-10, sw, 5, col);
    }

    // context hint
    if (!in_car) {
        bool near_car = false;
        for (int i = 0; i < MAX_CARS; i++)
            if (!cars[i].driven && near((int)px,(int)py,(int)cars[i].x,(int)cars[i].y, 22))
                { near_car = true; break; }
        if (near_car)
            print_centered("Z: get in", SCREEN_W/2, SCREEN_H-22, CLR_LIGHT_GREY);
    } else {
        print_centered("Z: get out", SCREEN_W/2, SCREEN_H-22, CLR_LIGHT_GREY);
    }

    // busted overlay
    if (busted) {
        rectfill(SCREEN_W/2-60, SCREEN_H/2-22, 120, 44, CLR_RED);
        rectfill(SCREEN_W/2-58, SCREEN_H/2-20, 116, 40, CLR_BLACK);
        print_centered("BUSTED!", SCREEN_W/2, SCREEN_H/2-12, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H/2+2, CLR_YELLOW);
    }
}
