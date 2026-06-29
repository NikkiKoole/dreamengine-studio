/* de:meta
{
  "title": "frogger",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop"
  ],
  "lineage": "Port of Konami's 1981 Frogger; canonical grid-hop + log-riding loop, lane data-driven via a static LaneCfg table.",
  "genre": "arcade",
  "homage": "Frogger (1981)",
  "description": "Classic Frogger. Hop across 4 lanes of traffic, ride logs across the river, land on all 5 lily pads to advance. Each round speeds up. 3 lives. Arrow keys to hop."
}
de:meta */
#include "studio.h"

// FROGGER — hop to all five lily pads!
// Arrow keys to hop. Ride logs across the river. Dodge cars.
// Fill all 5 pads to advance. 3 lives.

#define LANE_H  16
#define CELL    16
#define ROWS    11   // lanes 0-10
#define MAP_COLS 20

#define NUM_PADS   5
#define MAX_LOGS  12
#define MAX_CARS  12

// lily pad positions: map column index (0-indexed)
static const int PAD_COL[NUM_PADS] = { 1, 5, 9, 13, 17 };

// lane types
#define LTYPE_HOME   0
#define LTYPE_WATER  1
#define LTYPE_MEDIAN 2
#define LTYPE_ROAD   3
#define LTYPE_START  4

typedef struct { int type; int dir; float base_spd; int car_col; } LaneCfg;

static const LaneCfg LANE[ROWS] = {
  { LTYPE_HOME,   0,   0.0f, 0            },  // 0  home row
  { LTYPE_WATER, +1,   0.7f, 0            },  // 1  river →
  { LTYPE_WATER, -1,   0.5f, 0            },  // 2  river ←
  { LTYPE_WATER, +1,   0.9f, 0            },  // 3  river →
  { LTYPE_WATER, -1,   0.4f, 0            },  // 4  river ←
  { LTYPE_MEDIAN, 0,   0.0f, 0            },  // 5  safe strip
  { LTYPE_ROAD,  +1,   0.8f, CLR_RED      },  // 6  road → red
  { LTYPE_ROAD,  -1,   1.1f, CLR_YELLOW   },  // 7  road ← yellow
  { LTYPE_ROAD,  +1,   0.6f, CLR_BLUE     },  // 8  road → blue
  { LTYPE_ROAD,  -1,   1.3f, CLR_PINK     },  // 9  road ← pink
  { LTYPE_START,  0,   0.0f, 0            },  // 10 start zone
};

typedef struct { float x; int lane, tiles; } Log;
typedef struct { float x; int lane, w;     } Car;

static Log  logs[MAX_LOGS];
static Car  cars[MAX_CARS];
static int  n_logs, n_cars;

// frog state
static int   fx, fy;       // pixel position (top-left)
static float frac_x;       // sub-pixel accumulator while riding a log

// game state
static int   lives, score, hiscore;
static bool  pads[NUM_PADS];
static int   n_filled;
static float level_spd;
static float dead_t;
static int   gstate;       // 0=title 1=play 2=dead 3=gameover

static int lane_y(int row) { return row * LANE_H; }

static void place_frog() {
  fx = 9 * CELL;       // column 9 — aligns with centre lily pad
  fy = lane_y(10);     // start zone
  frac_x = 0;
}

static void setup_objects() {
  n_logs = 0;
  n_cars = 0;
  #define LOG(x,ln,t) if(n_logs<MAX_LOGS) logs[n_logs++]=(Log){x,ln,t}
  #define CAR(x,ln,w) if(n_cars<MAX_CARS) cars[n_cars++]=(Car){x,ln,w}

  LOG(  0, 1, 3); LOG(130, 1, 3); LOG(240, 1, 3);  // lane 1: 3 short logs →
  LOG(  0, 2, 4); LOG(190, 2, 4);                   // lane 2: 2 long  logs ←
  LOG( 20, 3, 2); LOG(130, 3, 2); LOG(240, 3, 2);  // lane 3: 3 tiny  logs →
  LOG( 30, 4, 4); LOG(210, 4, 4);                   // lane 4: 2 long  logs ←

  CAR(  0, 6, 26); CAR(130, 6, 26); CAR(248, 6, 26); // lane 6: 3 cars →
  CAR( 40, 7, 40); CAR(210, 7, 40);                   // lane 7: 2 trucks ←
  CAR( 10, 8, 26); CAR(140, 8, 26); CAR(264, 8, 26); // lane 8: 3 cars →
  CAR( 60, 9, 26); CAR(210, 9, 26);                   // lane 9: 2 cars ←

  #undef LOG
  #undef CAR
}

static void die() {
  lives--;
  dead_t = now();
  note(36, INSTR_NOISE, 7);
  gstate = (lives <= 0) ? 3 : 2;
  if (lives <= 0 && score > hiscore) { hiscore = score; save(0, hiscore); }
}

static void new_game() {
  lives = 3; score = 0; level_spd = 1.0f;
  for (int i = 0; i < NUM_PADS; i++) pads[i] = false;
  n_filled = 0;
  setup_objects();
  place_frog();
  gstate = 1;
}

void init() {
  hiscore = load(0);
  colorkey(0);
  setup_objects();
  place_frog();
  gstate = 0;
}

void update() {
  if (gstate == 0) {
    if (btnp(0, BTN_A) || btnp(0, BTN_B)) new_game();
    return;
  }
  if (gstate == 3) {
    if (btnp(0, BTN_A) || btnp(0, BTN_B)) new_game();
    return;
  }
  if (gstate == 2) {
    if (now() - dead_t > 1.5f) { place_frog(); gstate = 1; }
    return;
  }

  // ---- hop input (one step per press) ----
  int mx = 0, my = 0;
  if (btnp(0, BTN_UP))    { my = -LANE_H; }
  if (btnp(0, BTN_DOWN))  { my = +LANE_H; }
  if (btnp(0, BTN_LEFT))  { mx = -CELL;   }
  if (btnp(0, BTN_RIGHT)) { mx = +CELL;   }

  if (fy + my < 0)            my = 0;
  if (fy + my > lane_y(10))   my = 0;
  if (fx + mx < 0)            mx = 0;
  if (fx + mx + CELL > SCREEN_W) mx = 0;

  if (mx || my) {
    fx += mx; fy += my;
    frac_x = 0;
    if (my < 0) score += 5;
    note(72, INSTR_SQUARE, 2);
  }

  int flane = fy / LANE_H;

  // ---- move all logs ----
  for (int i = 0; i < n_logs; i++) {
    int ln = logs[i].lane;
    float spd = (float)LANE[ln].dir * LANE[ln].base_spd * level_spd;
    logs[i].x += spd;
    int lw = logs[i].tiles * CELL;
    if (logs[i].x > SCREEN_W)       logs[i].x -= (float)(SCREEN_W + lw);
    if (logs[i].x < -(float)lw)     logs[i].x += (float)(SCREEN_W + lw);
  }

  // ---- ride log / drown check ----
  bool on_log = false;
  if (LANE[flane].type == LTYPE_WATER) {
    float ride_spd = 0;
    for (int i = 0; i < n_logs; i++) {
      if (logs[i].lane != flane) continue;
      int lx = (int)logs[i].x;
      int lw = logs[i].tiles * CELL;
      if (fx + CELL - 2 > lx && fx + 2 < lx + lw) {
        on_log   = true;
        ride_spd = (float)LANE[flane].dir * LANE[flane].base_spd * level_spd;
        break;
      }
    }
    if (on_log) {
      frac_x += ride_spd;
      int imove = (int)frac_x;
      frac_x -= (float)imove;
      fx += imove;
    }
    if (!on_log || fx < 0 || fx + CELL > SCREEN_W) { die(); return; }
  }

  // ---- move all cars ----
  for (int i = 0; i < n_cars; i++) {
    int ln = cars[i].lane;
    float spd = (float)LANE[ln].dir * LANE[ln].base_spd * level_spd;
    cars[i].x += spd;
    if (cars[i].x > SCREEN_W)            cars[i].x -= (float)(SCREEN_W + cars[i].w);
    if (cars[i].x < -(float)cars[i].w)   cars[i].x += (float)(SCREEN_W + cars[i].w);
  }

  // ---- car collision ----
  if (LANE[flane].type == LTYPE_ROAD) {
    for (int i = 0; i < n_cars; i++) {
      if (cars[i].lane != flane) continue;
      if (boxes_touch(fx+1, fy+1, CELL-2, CELL-2,
                      (int)cars[i].x, lane_y(flane)+2, cars[i].w, LANE_H-4)) {
        die(); return;
      }
    }
  }

  // ---- home row landing ----
  if (flane == 0) {
    int frog_col = (fx + CELL / 2) / CELL;  // use center, not top-left (log drift makes fx non-aligned)
    bool on_pad = false;
    for (int i = 0; i < NUM_PADS; i++) {
      if (frog_col == PAD_COL[i]) {
        on_pad = true;
        if (!pads[i]) {
          pads[i] = true; n_filled++;
          score += 50;
          note(60 + n_filled * 7, INSTR_SINE, 5);
        }
        place_frog();
        break;
      }
    }
    if (!on_pad) { die(); return; }

    if (n_filled == NUM_PADS) {
      score += 200;
      level_spd += 0.2f;
      for (int i = 0; i < NUM_PADS; i++) pads[i] = false;
      n_filled = 0;
      note(84, INSTR_SQUARE, 6);
    }
  }
}

void draw() {
  cls(CLR_DARK_BLUE);

  // background tiles (grass, road, lily pads) — 20×11 cells = 320×176 px
  map(0, 0, 0, 0, 20, 11);

  // clip drawing to game area
  clip(0, 0, SCREEN_W, 176);

  // draw logs (slot 6 repeated per tile)
  for (int i = 0; i < n_logs; i++) {
    int lx = (int)logs[i].x;
    int ly = lane_y(logs[i].lane);
    for (int t = 0; t < logs[i].tiles; t++) {
      spr(6, lx + t * CELL, ly);
    }
  }

  // draw cars: body + two windows
  for (int i = 0; i < n_cars; i++) {
    int cx  = (int)cars[i].x;
    int cy  = lane_y(cars[i].lane);
    int cw  = cars[i].w;
    int col = LANE[cars[i].lane].car_col;
    rectfill(cx,       cy+3, cw,   LANE_H-6, col);           // body
    rectfill(cx+3,     cy+5, 7,    LANE_H-11, CLR_DARKER_BLUE); // front window
    rectfill(cx+cw-10, cy+5, 7,    LANE_H-11, CLR_DARKER_BLUE); // rear window
  }

  // draw claimed pad indicators (yellow dot)
  for (int i = 0; i < NUM_PADS; i++) {
    if (pads[i]) {
      int px = PAD_COL[i] * CELL + CELL/2;
      circfill(px, LANE_H/2, 4, CLR_YELLOW);
    }
  }

  // draw frog (blink when just died)
  if (gstate == 1 || (gstate == 2 && frame() % 6 < 3)) {
    spr(4, fx, fy);
  }

  // lane direction arrows (subtle, drawn on each water/road lane edge)
  for (int row = 1; row <= 9; row++) {
    if (LANE[row].type != LTYPE_WATER && LANE[row].type != LTYPE_ROAD) continue;
    int ay = lane_y(row) + LANE_H/2;
    int col = (LANE[row].type == LTYPE_WATER) ? CLR_BLUE_GREEN : CLR_DARK_GREY;
    if (LANE[row].dir > 0) {
      pset(SCREEN_W - 6, ay - 1, col);
      pset(SCREEN_W - 5, ay,     col);
      pset(SCREEN_W - 6, ay + 1, col);
    } else {
      pset(5, ay - 1, col);
      pset(4, ay,     col);
      pset(5, ay + 1, col);
    }
  }

  clip(0, 0, 0, 0);  // reset clip

  // ---- HUD strip (y=176..199) ----
  rectfill(0, 176, SCREEN_W, 24, CLR_BROWNISH_BLACK);
  line(0, 176, SCREEN_W-1, 176, CLR_DARK_GREY);

  // life indicators (small green squares)
  for (int i = 0; i < lives; i++) {
    rectfill(6 + i*14, 184, 10, 10, CLR_GREEN);
  }
  print(str("SCORE %d", score),    60, 184, CLR_YELLOW);
  print_right(str("BEST %d", hiscore), SCREEN_W-4, 184, CLR_LIGHT_GREY);

  // pad progress dots
  for (int i = 0; i < NUM_PADS; i++) {
    int px = SCREEN_W/2 - 20 + i*10;
    circfill(px, 191, 3, pads[i] ? CLR_GREEN : CLR_DARK_GREY);
  }

  // title screen overlay
  if (gstate == 0) {
    rectfill(SCREEN_W/2-80, SCREEN_H/2-38, 160, 70, CLR_BLACK);
    rect    (SCREEN_W/2-80, SCREEN_H/2-38, 160, 70, CLR_GREEN);
    print_centered("FROGGER", SCREEN_W/2, SCREEN_H/2-28, CLR_GREEN);
    print_centered("Hop to the lily pads", SCREEN_W/2, SCREEN_H/2-12, CLR_WHITE);
    print_centered("Ride logs, dodge cars", SCREEN_W/2, SCREEN_H/2+2, CLR_LIGHT_GREY);
    print_centered("Press Z to start", SCREEN_W/2, SCREEN_H/2+16, CLR_YELLOW);
    return;
  }

  // game over overlay
  if (gstate == 3) {
    rectfill(SCREEN_W/2-64, SCREEN_H/2-22, 128, 48, CLR_BLACK);
    rect    (SCREEN_W/2-64, SCREEN_H/2-22, 128, 48, CLR_WHITE);
    print_centered("GAME OVER", SCREEN_W/2, SCREEN_H/2-12, CLR_RED);
    print_centered("Z to try again", SCREEN_W/2, SCREEN_H/2+4, CLR_LIGHT_GREY);
  }
}
