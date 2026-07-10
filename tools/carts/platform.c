/* de:meta
{
  "slug": "platform",
  "title": "platform game",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "game"
  ],
  "teaches": [
    "tilemap-collision",
    "camera-follow",
    "title-play-gameover-loop"
  ],
  "lineage": "Canonical reference platformer for the engine; straightforward Mario-style stomp loop built to demonstrate tilemap collision and multi-tile scrolling with no notable novel technique.",
  "genre": "platformer",
  "description": "Side-scrolling platformer with sprites and a tile map. Stomp red enemies from above, collect coins, watch your lives. Left/right to move, Z or up to jump."
}
de:meta */
#include "studio.h"

// PLATFORM GAME
// left/right to move, Z to jump, stomp enemies from above

#define TILE_BRICK  1
#define TILE_GRASS  2
#define TILE_COIN   3

#define PW          14    // player pixel width
#define PH          15    // player pixel height
#define GRAVITY     0.32f
#define JUMP_VY    -5.8f
#define WALK_SPD    2.2f

#define WORLD_W     (80 * CELL_W)
#define WORLD_H     (19 * CELL_H)

// ---- player ----
static float px, py, pvx, pvy;
static bool  on_ground, facing_right;
static bool  p_dead, gameover;
static float dead_t;
static int   lives, coins, score;

// ---- enemies ----
#define MAX_EN  8
static float   ex[MAX_EN], ey[MAX_EN], evx[MAX_EN];
static bool    eon[MAX_EN];

// ---- solid tile check ----

static bool solid_tile(int cx, int cy) {
    int t = mget(cx, cy);
    return t == TILE_BRICK || t == TILE_GRASS;
}

static bool solid_at(int x, int y, int w, int h) {
    int x2 = x + w - 1, y2 = y + h - 1;
    return solid_tile(x/CELL_W,  y/CELL_H)  ||
           solid_tile(x2/CELL_W, y/CELL_H)  ||
           solid_tile(x/CELL_W,  y2/CELL_H) ||
           solid_tile(x2/CELL_W, y2/CELL_H);
}

// ---- collect coins near player ----

static void collect_coins() {
    // check the 4 cells the player might overlap
    int x1 = (int)px / CELL_W,     y1 = (int)py / CELL_H;
    int x2 = ((int)px+PW) / CELL_W, y2 = ((int)py+PH) / CELL_H;
    int xs[2] = { x1, x2 }, ys[2] = { y1, y2 };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            if (mget(xs[i], ys[j]) == TILE_COIN) {
                mset(xs[i], ys[j], 0);
                coins++;  score += 10;
                note(72 + (coins % 8) * 2, INSTR_SINE, 3);
            }
}

// ---- setup ----

static void place_enemies() {
    // pairs of (pixel_x, map_row) — enemy stands on that row's platform
    int starts[MAX_EN][2] = {
        { 96,  6}, {340,  6}, {570,  6}, {740,  6},
        {200, 12}, {480, 12}, {700,  9}, {900,  9},
    };
    for (int i = 0; i < MAX_EN; i++) {
        ex[i]  = starts[i][0];
        ey[i]  = starts[i][1] * CELL_H - PH;   // stand on top of that row
        evx[i] = (i % 2 == 0) ? 0.9f : -0.9f;
        eon[i] = true;
    }
}

static void reset_player() {
    px = 48; py = 16 * CELL_H - (PH - 1);  // bottom pixel at tile 16 top
    pvx = pvy = 0;
    p_dead = false;
    facing_right = true;
}

void init() {
    lives = 3; coins = 0; score = 0; gameover = false;
    colorkey(0);
    place_enemies();
    reset_player();
}

// ---- update ----

void update() {
    if (gameover) {
        if (btnp(0,BTN_A)||btnp(0,BTN_B)) {
            lives=3; coins=0; score=0; gameover=false;
            place_enemies(); reset_player();
        }
        return;
    }
    if (p_dead) {
        if (now() - dead_t > 1.5f) {
            if (--lives <= 0) gameover = true;
            else { place_enemies(); reset_player(); }
        }
        return;
    }

    // horizontal input
    pvx = 0;
    if (btn(0,BTN_LEFT))  { pvx = -WALK_SPD; facing_right = false; }
    if (btn(0,BTN_RIGHT)) { pvx =  WALK_SPD; facing_right = true;  }

    // jump
    if ((btnp(0,BTN_A) || btnp(0,BTN_UP)) && on_ground) {
        pvy = JUMP_VY;
        note(62, INSTR_SQUARE, 3);
    }

    // gravity
    pvy = clamp(pvy + GRAVITY, -12.0f, 9.0f);

    // move X then resolve — use PH-1 to exclude the foot pixel (which sits on the ground tile)
    px += pvx;
    px  = clamp(px, 0, WORLD_W - PW);
    if (solid_at((int)px, (int)py, PW, PH - 1)) {
        px -= pvx;
        pvx = 0;
    }

    // move Y then resolve
    py += pvy;
    if (solid_at((int)px, (int)py, PW, PH)) {
        if (pvy >= 0) {
            // snap bottom pixel to tile top — -(PH-1) keeps the foot pixel inside the tile
            py = (float)((int)(py + PH - 1) / CELL_H * CELL_H) - (PH - 1);
        } else {
            py = (float)((int)(py / CELL_H) * CELL_H + CELL_H);
        }
        pvy = 0;
    }
    // probe 1px below feet — true every frame while standing, not just on collision frame
    on_ground = solid_at((int)px, (int)py + 1, PW, PH);

    // fall off world → die
    if (py > WORLD_H + 32) {
        p_dead = true; dead_t = now();
        note(36, INSTR_NOISE, 6);
        return;
    }

    collect_coins();

    // enemies
    for (int i = 0; i < MAX_EN; i++) {
        if (!eon[i]) continue;

        ex[i] += evx[i];

        // turn around at map edges or walls
        bool hit_wall = solid_at((int)ex[i] - 1, (int)ey[i], 1, PH) ||
                        solid_at((int)ex[i] + PW, (int)ey[i], 1, PH);
        if (hit_wall || ex[i] < 0 || ex[i] > WORLD_W - PW)
            evx[i] = -evx[i];

        // stomp or hurt
        if (boxes_touch((int)px,(int)py,PW,PH, (int)ex[i],(int)ey[i],PW,PH)) {
            bool stomp = pvy > 0 && py + PH < ey[i] + PH/2;
            if (stomp) {
                eon[i] = false;
                pvy = -4.0f;
                score += 100;
                note(50, INSTR_SQUARE, 4);
            } else {
                p_dead = true; dead_t = now();
                note(36, INSTR_NOISE, 6);
                return;
            }
        }
    }
}

// ---- draw ----

void draw() {
    cls(CLR_DARK_BLUE);

    // background clouds (parallax: move at half player speed)
    camera(0, 0);
    int cam_x = (int)clamp(px + PW/2 - SCREEN_W/2, 0, WORLD_W - SCREEN_W);
    int cam_y = (int)clamp(py + PH/2 - SCREEN_H/2, 0, WORLD_H - SCREEN_H);
    for (int i = 0; i < 5; i++) {
        int cx2 = ((i * 137 + 20) % WORLD_W - cam_x/2) % SCREEN_W;
        int cy2 = 20 + (i * 31) % 40;
        rectfill(cx2,    cy2,   28, 10, CLR_WHITE);
        rectfill(cx2+4,  cy2-5, 20,  9, CLR_WHITE);
        rectfill(cx2+8,  cy2-9, 14,  7, CLR_WHITE);
    }

    // world — follow resets camera for map + sprites
    follow((int)px + PW/2, (int)py + PH/2, WORLD_W, WORLD_H);
    map(0, 0, 0, 0, 80, 19);

    // enemies
    for (int i = 0; i < MAX_EN; i++) {
        if (!eon[i]) continue;
        int fr = anim(2, 5, (float)i * 0.3f);
        sprf(6 + fr, (int)ex[i], (int)ey[i], evx[i] > 0, false);
    }

    // player
    if (!p_dead) {
        int fr = (pvx != 0 && on_ground) ? anim(2, 8, 0) : 0;
        sprf(4 + fr, (int)px, (int)py, !facing_right, false);
    } else {
        // flash
        if (frame() % 4 < 2)
            sprf(4, (int)px, (int)py, !facing_right, false);
    }

    // HUD (screen-space, reset camera first)
    camera(0, 0);

    rectfill(0, 0, SCREEN_W, 12, CLR_BLACK);
    print(str("x%d", lives), 4, 2, CLR_RED);
    print(str("COINS %d", coins), 28, 2, CLR_YELLOW);
    print_right(str("SCORE %d", score), SCREEN_W - 4, 2, CLR_WHITE);

    if (gameover) {
        rectfill(SCREEN_W/2-64, SCREEN_H/2-20, 128, 50, CLR_BLACK);
        rect    (SCREEN_W/2-64, SCREEN_H/2-20, 128, 50, CLR_WHITE);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H/2-12, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H/2, CLR_YELLOW);
        print_centered("Z to restart", SCREEN_W/2, SCREEN_H/2+12, CLR_LIGHT_GREY);
    }
}
