/* de:meta
{
  "title": "burger time",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "tilemap-collision",
    "title-play-gameover-loop"
  ],
  "lineage": "Faithful demake of Data East's Burger Time (1982); the cascade mechanic (ingredient drop chain-knocks lower layers) is the novel implementation challenge on top of the standard platform/ladder grid.",
  "genre": "arcade",
  "homage": "BurgerTime (1982)",
  "description": "Faithful demake of the 1982 arcade classic. You're the chef on a lattice of platforms and ladders. Walk across every segment of a burger ingredient to stomp it down a floor — when all four are stomped it drops, landing on the piece below and knocking THAT one loose too (the cascade), all the way to the plate. Build all four burgers to clear the stage while Mr. Hot Dog, Pickle and Egg chase you. Freeze them with a limited shake of pepper, or drop an ingredient on them for big points. Arrows / WASD: walk + climb — Z or X: pepper."
}
de:meta */
#include "studio.h"

// BURGER TIME
// You're the chef. Walk across every segment of a burger ingredient to stomp
// it down a floor — when all four are stomped it drops, landing on the piece
// below and knocking THAT one loose too (the cascade). Build all four burgers
// on the plates to clear the stage.
// Mr. Hot Dog, Pickle and Egg chase you — touch one and you lose a life.
// Your only weapon is a shake of pepper (limited), which freezes them.
// Arrows / WASD: walk + climb   Z or X: throw pepper

#define NF      5            // floors
#define NL      5            // ladders
#define NBURG   4
#define NING    16           // NBURG * 4 ingredients
#define SEG_W   16
#define BURG_X0 32
#define LEFTX   24
#define RIGHTX  296
#define ING_STACK 5          // vertical gap between plated layers
#define MAX_ENEMY 4

static const int floor_y[NF]  = { 32, 68, 104, 140, 176 };
static const int ladder_x[NL] = { 32, 96, 160, 224, 288 };

// ingredient types == tier from the top
enum { BUN_TOP, LETTUCE, PATTY, BUN_BOT };

typedef struct {
    int   col, type, floor;     // floor 0..3 = platform, 99 = plated
    bool  press[4], falling, plated;
    float y;
    int   slot;                 // stack position on the plate
} Ing;

typedef struct {
    float x, y;
    bool  on_ladder;
    int   facing, kind;         // kind 0 hotdog,1 pickle,2 egg
    bool  alive;
    float respawn_at, stun_until;
} Actor;

static Ing   ing[NING];
static int   plated_n[NBURG];
static int   built;            // ingredients fully plated

static Actor chef;
static Actor enemy[MAX_ENEMY];
static int   n_enemy;
static float chef_inv;

// pepper cloud (one at a time)
static float pep_x, pep_y, pep_until;
static int   pepper;

static int state;              // 0 play, 1 game over, 2 win
static int lives, score, hiscore, level;
static float level_start;

// ====================================================================

static float fabsf2(float v) { return v < 0 ? -v : v; }

static int floor_of(float y) {
    for (int f = 0; f < NF; f++) if (fabsf2(y - floor_y[f]) < 5) return f;
    return -1;
}
static int nearest_floor(float y) {
    int b = 0; float bd = 1e9f;
    for (int f = 0; f < NF; f++) { float d = fabsf2(y - floor_y[f]); if (d < bd) { bd = d; b = f; } }
    return b;
}
static int nearest_ladder(float x) {
    for (int l = 0; l < NL; l++) if (fabsf2(x - ladder_x[l]) < 7) return l;
    return -1;
}

static int find_resting(int col, int floor) {
    for (int i = 0; i < NING; i++)
        if (ing[i].col == col && ing[i].floor == floor && !ing[i].falling && !ing[i].plated)
            return i;
    return -1;
}

// stomp complete → fall one floor, pushing whatever rests below (cascade)
static void trigger_fall(int i) {
    Ing *I = &ing[i];
    if (I->falling || I->plated) return;
    I->falling = true;
    for (int s = 0; s < 4; s++) I->press[s] = false;

    int target = I->floor + 1;
    if (target >= 4) {                       // floor 4 is the plate
        I->slot = plated_n[I->col]++;
        I->floor = 99;
    } else {
        int j = find_resting(I->col, target);
        if (j >= 0) trigger_fall(j);
        I->floor = target;
    }
    hit(48, INSTR_SQUARE, 3, 70);
}

static void setup_level() {
    for (int c = 0; c < NBURG; c++) plated_n[c] = 0;
    built = 0;
    for (int c = 0; c < NBURG; c++)
        for (int t = 0; t < 4; t++) {
            Ing *I = &ing[c * 4 + t];
            I->col = c; I->type = t; I->floor = t;
            I->y = floor_y[t]; I->falling = false; I->plated = false;
            for (int s = 0; s < 4; s++) I->press[s] = false;
        }

    chef.x = LEFTX + 8; chef.y = floor_y[4]; chef.on_ladder = false; chef.facing = 1;
    chef_inv = now() + 1.0f;

    n_enemy = min(2 + level, MAX_ENEMY);
    for (int e = 0; e < MAX_ENEMY; e++) {
        enemy[e].kind = e % 3;
        enemy[e].alive = (e < n_enemy);
        enemy[e].on_ladder = false;
        enemy[e].x = (e & 1) ? RIGHTX - 8 : LEFTX + 40;
        enemy[e].y = floor_y[e % 2];
        enemy[e].stun_until = 0;
        enemy[e].respawn_at = 0;
    }
    pep_until = 0;
    level_start = now();
}

static void reset_game() {
    lives = 3; score = 0; level = 1; pepper = 5;
    state = 0;
    setup_level();
}

void init() {
    hiscore = load(0);
    reset_game();
}

static void lose_life() {
    lives--;
    note(34, INSTR_NOISE, 6);
    if (lives <= 0) {
        state = 1;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
    } else {
        chef.x = LEFTX + 8; chef.y = floor_y[4]; chef.on_ladder = false;
        chef_inv = now() + 1.4f;
    }
}

// ====================================================================
// movement shared by chef + enemies
// ====================================================================

static void move_actor(Actor *a, int wx, int wy, float spd) {
    if (a->on_ladder) {
        if (wy != 0) { a->y += wy * spd; a->y = clamp(a->y, floor_y[0], floor_y[NF - 1]); }
        int f = floor_of(a->y);
        if (wx != 0 && f >= 0) { a->on_ladder = false; a->y = floor_y[f]; }
    } else {
        if (wx != 0) { a->x += wx * spd; a->x = clamp(a->x, LEFTX, RIGHTX); a->facing = wx > 0 ? 1 : -1; }
        if (wy != 0) {
            int L = nearest_ladder(a->x);
            if (L >= 0) {
                bool ok = (wy < 0 && a->y > floor_y[0] + 1) || (wy > 0 && a->y < floor_y[NF - 1] - 1);
                if (ok) { a->on_ladder = true; a->x = ladder_x[L]; a->y += wy * spd; }
            }
        }
    }
}

// ====================================================================
// update
// ====================================================================

static void update_chef() {
    int wx = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    int wy = (btn(0, BTN_DOWN)  ? 1 : 0) - (btn(0, BTN_UP)   ? 1 : 0);
    move_actor(&chef, wx, wy, 1.5f);

    // stomp the ingredient under our feet
    if (!chef.on_ladder) {
        int f = floor_of(chef.y);
        if (f >= 0 && f <= 3 && chef.x >= BURG_X0 && chef.x < BURG_X0 + NBURG * SEG_W * 4 / 1) {
            int seg = (int)(chef.x - BURG_X0) / SEG_W;
            if (seg >= 0 && seg < NBURG * 4) {
                int col = seg / 4, sw = seg % 4;
                int i = find_resting(col, f);
                if (i >= 0 && !ing[i].press[sw]) {
                    ing[i].press[sw] = true;
                    hit(60, INSTR_SQUARE, 1, 40);
                    if (ing[i].press[0] && ing[i].press[1] && ing[i].press[2] && ing[i].press[3])
                        trigger_fall(i);
                }
            }
        }
    }

    // throw pepper
    if ((btnp(0, BTN_A) || btnp(0, BTN_B)) && pepper > 0 && now() > pep_until) {
        pepper--;
        pep_x = chef.x + chef.facing * 12;
        pep_y = chef.y - 5;
        pep_until = now() + 0.45f;
        note(72, INSTR_NOISE, 3);
    }
}

static void update_falling() {
    for (int i = 0; i < NING; i++) {
        Ing *I = &ing[i];
        if (!I->falling) continue;
        I->y += 3.0f;
        float dest = (I->floor == 99) ? (floor_y[4] - I->slot * ING_STACK) : floor_y[I->floor];

        // squash any enemy the ingredient sweeps over
        int bx = BURG_X0 + I->col * 64;
        for (int e = 0; e < n_enemy; e++) {
            if (!enemy[e].alive) continue;
            if (boxes_touch(bx, (int)I->y - 5, 64, 8,
                            (int)enemy[e].x - 5, (int)enemy[e].y - 10, 10, 10)) {
                enemy[e].alive = false;
                enemy[e].respawn_at = now() + 2.5f;
                score += 500;
                note(84, INSTR_SQUARE, 4);
            }
        }

        if (I->y >= dest) {
            I->y = dest;
            I->falling = false;
            if (I->floor == 99) {
                I->plated = true;
                built++;
                hit(67, INSTR_SINE, 4, 120);
                if (built >= NING) {
                    state = 2;
                    score += 1000;
                    if (score > hiscore) { hiscore = score; save(0, hiscore); }
                }
            }
        }
    }
}

static void update_enemy(int e) {
    Actor *a = &enemy[e];
    if (!a->alive) {
        if (now() > a->respawn_at) {
            a->alive = true; a->on_ladder = false;
            a->x = (e & 1) ? RIGHTX - 8 : LEFTX + 8;
            a->y = floor_y[0];
        }
        return;
    }
    if (now() < a->stun_until) return;

    // pepper freeze
    if (now() < pep_until &&
        boxes_touch((int)pep_x - 7, (int)pep_y - 7, 14, 14,
                    (int)a->x - 5, (int)a->y - 10, 10, 10)) {
        a->stun_until = now() + 3.0f;
        return;
    }

    int tf = chef.on_ladder ? nearest_floor(chef.y) : floor_of(chef.y);
    if (tf < 0) tf = nearest_floor(chef.y);

    int wx = 0, wy = 0;
    if (a->on_ladder) {
        int f = floor_of(a->y);
        if (f == tf) wx = sgn((int)chef.x - (int)a->x);       // arrive → step off toward chef
        else         wy = (floor_y[tf] < a->y) ? -1 : 1;
    } else {
        int f = floor_of(a->y);
        if (f == tf) {
            wx = sgn((int)chef.x - (int)a->x);
        } else {
            // head to the nearest ladder, then climb toward the chef's floor
            int best = 0; float bd = 1e9f;
            for (int l = 0; l < NL; l++) { float d = fabsf2(a->x - ladder_x[l]); if (d < bd) { bd = d; best = l; } }
            if (fabsf2(a->x - ladder_x[best]) > 3) wx = sgn(ladder_x[best] - (int)a->x);
            else                                   wy = (tf < f) ? -1 : 1;
        }
    }
    move_actor(a, wx, wy, 0.85f + level * 0.05f);

    // caught the chef?
    if (now() > chef_inv &&
        boxes_touch((int)chef.x - 4, (int)chef.y - 10, 8, 10,
                    (int)a->x - 5, (int)a->y - 10, 10, 10))
        lose_life();
}

void update() {
    if (state != 0) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) {
            if (state == 2) { level++; pepper = min(pepper + 3, 9); state = 0; setup_level(); }
            else reset_game();
        }
        return;
    }
    update_chef();
    update_falling();
    for (int e = 0; e < n_enemy; e++) update_enemy(e);
}

// ====================================================================
// drawing
// ====================================================================

static void draw_segment(int sx, int sy, int type) {
    switch (type) {
        case BUN_TOP:
            rectfill(sx + 1, sy - 2, 14, 3, CLR_ORANGE);
            rectfill(sx + 3, sy - 4, 10, 2, CLR_ORANGE);
            pset(sx + 5, sy - 3, CLR_LIGHT_PEACH);
            pset(sx + 9, sy - 4, CLR_LIGHT_PEACH);
            pset(sx + 12, sy - 3, CLR_LIGHT_PEACH);
            break;
        case LETTUCE:
            rectfill(sx, sy - 2, 16, 3, CLR_GREEN);
            for (int k = 0; k < 16; k += 4) trifill(sx + k, sy - 2, sx + k + 2, sy - 5, sx + k + 4, sy - 2, CLR_LIME_GREEN);
            break;
        case PATTY:
            rectfill(sx, sy - 3, 16, 4, CLR_DARK_BROWN);
            line(sx, sy - 3, sx + 15, sy - 3, CLR_BROWN);
            break;
        case BUN_BOT:
            rectfill(sx, sy - 2, 16, 3, CLR_ORANGE);
            break;
    }
}

static void draw_chef(int x, int y, int f, bool flash) {
    if (flash && blink(3)) return;
    rectfill(x - 3, y - 9, 6, 6, CLR_BLUE);        // torso
    rectfill(x - 3, y - 3, 6, 3, CLR_WHITE);       // apron
    rectfill(x - 2, y, 2, 2, CLR_DARK_BLUE);       // legs
    rectfill(x, y, 2, 2, CLR_DARK_BLUE);
    circfill(x, y - 11, 3, CLR_LIGHT_PEACH);       // head
    rectfill(x - 4, y - 14, 8, 3, CLR_WHITE);      // chef hat
    pset(x + f, y - 11, CLR_BLACK);                // eye
}

static void draw_enemy(Actor *a) {
    int x = (int)a->x, y = (int)a->y;
    bool stun = now() < a->stun_until;
    int wob = stun ? (rnd(3) - 1) : 0;
    int body = a->kind == 0 ? CLR_DARK_ORANGE : a->kind == 1 ? CLR_GREEN : CLR_WHITE;
    rectfill(x - 5 + wob, y - 9, 10, 9, body);
    if (a->kind == 0) { rectfill(x - 6 + wob, y - 7, 12, 4, CLR_ORANGE); }  // sausage
    if (a->kind == 1) { for (int k = -4; k <= 4; k += 3) pset(x + k + wob, y - 6, CLR_DARK_GREEN); } // pickle bumps
    // eyes
    pset(x - 2 + wob, y - 6, stun ? CLR_RED : CLR_BLACK);
    pset(x + 2 + wob, y - 6, stun ? CLR_RED : CLR_BLACK);
    // feet
    rectfill(x - 4, y, 2, 2, CLR_PINK);
    rectfill(x + 2, y, 2, 2, CLR_PINK);
}

void draw() {
    cls(CLR_DARKER_BLUE);

    // ladders
    for (int l = 0; l < NL; l++) {
        int lx = ladder_x[l];
        for (int y = floor_y[0]; y < floor_y[NF - 1]; y += 1) {
            pset(lx - 3, y, CLR_TRUE_BLUE);
            pset(lx + 3, y, CLR_TRUE_BLUE);
        }
        for (int y = floor_y[0]; y < floor_y[NF - 1]; y += 5)
            line(lx - 3, y, lx + 3, y, CLR_BLUE);
    }
    // platforms
    for (int f = 0; f < NF; f++)
        rectfill(LEFTX - 8, floor_y[f] + 2, RIGHTX - LEFTX + 16, 2, CLR_MEDIUM_GREY);

    // plates
    for (int c = 0; c < NBURG; c++) {
        int bx = BURG_X0 + c * 64;
        rectfill(bx - 2, floor_y[4] + 4, 68, 3, CLR_LIGHT_GREY);
    }

    // ingredients
    for (int i = 0; i < NING; i++) {
        Ing *I = &ing[i];
        int bx = BURG_X0 + I->col * 64;
        for (int s = 0; s < 4; s++) {
            int sx = bx + s * SEG_W;
            int sy = (int)I->y + (I->press[s] ? 3 : 0);
            draw_segment(sx, sy, I->type);
        }
    }

    // pepper cloud
    if (now() < pep_until)
        for (int k = 0; k < 10; k++)
            pset((int)pep_x + rnd(11) - 5, (int)pep_y + rnd(11) - 5, CLR_WHITE);

    for (int e = 0; e < n_enemy; e++) if (enemy[e].alive) draw_enemy(&enemy[e]);
    draw_chef((int)chef.x, (int)chef.y, chef.facing, now() < chef_inv);

    // HUD
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_right(str("BEST %d", hiscore), SCREEN_W - 4, 3, CLR_YELLOW);
    print_centered(str("LVL %d", level), SCREEN_W/2, 3, CLR_LIGHT_GREY);
    for (int i = 0; i < lives; i++) { int ix = 6 + i * 9; rectfill(ix - 3, 22, 6, 3, CLR_WHITE); }
    print(str("PEPPER %d", pepper), 4, 188, CLR_LIGHT_YELLOW);

    if (now() - level_start < 2.2f)
        print_centered("BUILD THE BURGERS!", SCREEN_W/2, SCREEN_H / 2, CLR_LIGHT_YELLOW);

    if (state == 1) {
        rectfill(SCREEN_W / 2 - 70, SCREEN_H / 2 - 26, 140, 56, CLR_BLACK);
        rect    (SCREEN_W / 2 - 70, SCREEN_H / 2 - 26, 140, 56, CLR_ORANGE);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H / 2 - 16, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 - 2, CLR_YELLOW);
        print_centered("Z to cook again", SCREEN_W/2, SCREEN_H / 2 + 14, CLR_LIGHT_GREY);
    }
    if (state == 2) {
        rectfill(SCREEN_W / 2 - 70, SCREEN_H / 2 - 26, 140, 56, CLR_BLACK);
        rect    (SCREEN_W / 2 - 70, SCREEN_H / 2 - 26, 140, 56, CLR_GREEN);
        print_centered("STAGE CLEAR!", SCREEN_W/2, SCREEN_H / 2 - 16, CLR_GREEN);
        print_centered("burgers served", SCREEN_W/2, SCREEN_H / 2 - 2, CLR_YELLOW);
        print_centered("Z for next stage", SCREEN_W/2, SCREEN_H / 2 + 14, CLR_LIGHT_GREY);
    }
}
