#include "studio.h"

// ALLEY CAT
// Climb the apartment wall, dodge the birds, and leap into a LIT window.
// Inside: gobble every mouse before the broom sweeps you out.
// Left/Right: move   Z or Up: jump   (in a room: arrows/WASD to prowl)
// You've got nine lives. Best score is saved.

// ---- building layout ----
#define COLS       4
#define ROWS       4
#define WIN_W      46
#define WIN_H      24
#define GROUND_Y   188
#define CAT_W      14
#define CAT_H      12

// ---- physics ----
#define MOVE       2.0f
#define GRAV       0.38f
#define JUMP_V     -6.7f
#define VMAX       7.0f
#define RSPD       2.0f      // room prowl speed

#define MAX_BIRDS  6
#define MAX_MICE   8
#define ROOM_TIME  9.0f
#define BROOM_SPD  2.3f

typedef enum { CLIMB, ROOM, GAMEOVER } State;

typedef struct { float x, y, vx; bool on; } Bird;
typedef struct { float x, y, vx, vy; bool on; } Mouse;

// ---- cat ----
static float cx, cy, vx, vy;
static bool  grounded;
static int   facing;
static float inv_until;     // invincibility window after a hit

// ---- climb scene ----
static bool  win_open[ROWS][COLS];
static Bird  birds[MAX_BIRDS];
static float dogx, dogvx;
static float level_start;

// ---- room scene ----
static Mouse mice[MAX_MICE];
static int   mice_left;
static float broom_x, broom_dir;

// ---- game ----
static State state;
static int   lives, score, hiscore, level;

// ---- geometry helpers ----
static int col_x(int c)  { return 22 + c * 70; }
static int sill_y(int r) { return 54 + r * 36; }          // top of the ledge
static int win_y(int r)  { return sill_y(r) - WIN_H; }    // top of the glass

// ====================================================================

static void setup_climb() {
    // open a couple of windows on the upper two rows — you must climb to them
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) win_open[r][c] = false;
    int opened = 0;
    while (opened < 2) {
        int r = rnd(2), c = rnd(COLS);
        if (!win_open[r][c]) { win_open[r][c] = true; opened++; }
    }

    int nb = min(2 + level, MAX_BIRDS);
    for (int i = 0; i < MAX_BIRDS; i++) {
        if (i < nb) {
            birds[i].on = true;
            birds[i].y  = 70 + (i % 3) * 36;                       // in the gaps between rows
            float sp    = (1.1f + level * 0.2f);
            if (sp > 3.0f) sp = 3.0f;
            birds[i].vx = (i & 1) ? sp : -sp;
            birds[i].x  = rnd(SCREEN_W);
        } else birds[i].on = false;
    }

    dogx = 40; dogvx = 0.9f;
    cx = SCREEN_W / 2 - CAT_W / 2;
    cy = GROUND_Y - CAT_H;
    vx = vy = 0; grounded = true; facing = 1;
    inv_until = now() + 1.0f;
    level_start = now();
}

static void setup_room() {
    state = ROOM;
    mice_left = min(4 + level, MAX_MICE);
    for (int i = 0; i < MAX_MICE; i++) {
        mice[i].on = (i < mice_left);
        mice[i].x  = rnd_between(40, SCREEN_W - 40);
        mice[i].y  = rnd_between(45, 165);
        float a = rnd(360);
        mice[i].vx = dx(0.5f, a);
        mice[i].vy = dy(0.5f, a);
    }
    broom_x = 30; broom_dir = 1;
    cx = SCREEN_W / 2 - CAT_W / 2;
    cy = 168;
    timer_reset();
}

static void reset_game() {
    lives = 9; score = 0; level = 1;
    state = CLIMB;
    setup_climb();
}

void init() {
    hiscore = load(0);
    reset_game();
}

static void lose_life() {
    lives--;
    note(40, INSTR_NOISE, 6);
    if (lives <= 0) {
        state = GAMEOVER;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
    } else {
        // respawn at the bottom with a moment of grace
        cx = SCREEN_W / 2 - CAT_W / 2;
        cy = GROUND_Y - CAT_H;
        vx = vy = 0; grounded = true;
        inv_until = now() + 1.2f;
    }
}

// ====================================================================
// CLIMB
// ====================================================================

static void update_climb() {
    vx = 0;
    if (btn(0, BTN_LEFT))  { vx = -MOVE; facing = -1; }
    if (btn(0, BTN_RIGHT)) { vx =  MOVE; facing =  1; }
    cx += vx;
    cx = clamp(cx, 0, SCREEN_W - CAT_W);

    if (grounded && (btnp(0, BTN_A) || btnp(0, BTN_UP))) {
        vy = JUMP_V; grounded = false;
        hit(64, INSTR_SQUARE, 2, 60);
    }

    float oldFeet = cy + CAT_H;
    vy += GRAV;
    if (vy > VMAX) vy = VMAX;
    cy += vy;
    float newFeet = cy + CAT_H;

    // one-way platform landing — only while falling
    grounded = false;
    if (vy > 0) {
        float bestTop = 1e9f;
        // ground
        if (oldFeet <= GROUND_Y && newFeet >= GROUND_Y) bestTop = GROUND_Y;
        // window sills
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                int top = sill_y(r);
                int px  = col_x(c) - 2, pw = WIN_W + 4;
                if (oldFeet <= top && newFeet >= top + 0.01f &&
                    cx + CAT_W > px && cx < px + pw && top < bestTop)
                    bestTop = top;
            }
        }
        if (bestTop < 1e9f) { cy = bestTop - CAT_H; vy = 0; grounded = true; }
    }

    // birds
    for (int i = 0; i < MAX_BIRDS; i++) {
        if (!birds[i].on) continue;
        birds[i].x += birds[i].vx;
        if (birds[i].x < -16)            birds[i].x = SCREEN_W + 8;
        if (birds[i].x > SCREEN_W + 16)  birds[i].x = -8;
        if (now() > inv_until &&
            boxes_touch((int)cx, (int)cy, CAT_W, CAT_H, (int)birds[i].x, (int)birds[i].y, 12, 6)) {
            lose_life();
            return;
        }
    }

    // dog on the sidewalk
    dogx += dogvx;
    if (dogx < 10 || dogx > SCREEN_W - 30) dogvx = -dogvx;
    bool onGround = grounded && cy + CAT_H >= GROUND_Y - 1;
    if (onGround && now() > inv_until &&
        boxes_touch((int)cx, (int)cy, CAT_W, CAT_H, (int)dogx, GROUND_Y - 11, 20, 11)) {
        lose_life();
        return;
    }

    // dive into a lit window
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (!win_open[r][c]) continue;
            if (boxes_touch((int)cx, (int)cy, CAT_W, CAT_H, col_x(c) + 4, win_y(r), WIN_W - 8, WIN_H)) {
                score += 150 + level * 50;
                hit(72, INSTR_SINE, 4, 120);
                schedule(120, 79, INSTR_SINE, 4);
                setup_room();
                return;
            }
        }
    }
}

// ====================================================================
// ROOM (bonus)
// ====================================================================

static void exit_room(bool cleared) {
    if (cleared) {
        score += 300 + level * 100;
        if ((level % 3) == 0 && lives < 9) lives++;   // an occasional extra life
    }
    level++;
    state = CLIMB;
    setup_climb();
}

static void update_room() {
    if (timer() > ROOM_TIME) { exit_room(false); return; }

    vx = vy = 0;
    if (btn(0, BTN_LEFT))  { vx = -RSPD; facing = -1; }
    if (btn(0, BTN_RIGHT)) { vx =  RSPD; facing =  1; }
    if (btn(0, BTN_UP))      vy = -RSPD;
    if (btn(0, BTN_DOWN))    vy =  RSPD;
    cx = clamp(cx + vx, 26, SCREEN_W - 26 - CAT_W);
    cy = clamp(cy + vy, 34, 172 - CAT_H);

    // mice scurry, flee a little when the cat is close, bounce off walls
    for (int i = 0; i < MAX_MICE; i++) {
        if (!mice[i].on) continue;
        if (near((int)mice[i].x, (int)mice[i].y, (int)cx + CAT_W / 2, (int)cy + CAT_H / 2, 34)) {
            float a = angle_to((int)cx, (int)cy, (int)mice[i].x, (int)mice[i].y);
            mice[i].vx = dx(0.9f, a);
            mice[i].vy = dy(0.9f, a);
        }
        mice[i].x += mice[i].vx;
        mice[i].y += mice[i].vy;
        if (mice[i].x < 28 || mice[i].x > SCREEN_W - 28) mice[i].vx = -mice[i].vx;
        if (mice[i].y < 36 || mice[i].y > 170)           mice[i].vy = -mice[i].vy;

        if (boxes_touch((int)cx, (int)cy, CAT_W, CAT_H, (int)mice[i].x - 4, (int)mice[i].y - 3, 8, 6)) {
            mice[i].on = false;
            mice_left--;
            score += 75;
            hit(76 + rnd(6), INSTR_SQUARE, 3, 70);
            if (mice_left <= 0) { exit_room(true); return; }
        }
    }

    // the broom sweeps — touch it and you're tossed back outside
    broom_x += broom_dir * BROOM_SPD;
    if (broom_x < 30 || broom_x > SCREEN_W - 30) broom_dir = -broom_dir;
    if (boxes_touch((int)cx, (int)cy, CAT_W, CAT_H, (int)broom_x - 5, 50, 10, 100)) {
        note(36, INSTR_NOISE, 5);
        exit_room(false);
    }
}

void update() {
    if (state == GAMEOVER) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) reset_game();
        return;
    }
    if (state == CLIMB) update_climb();
    else                update_room();
}

// ====================================================================
// drawing
// ====================================================================

static void draw_cat(int x, int y, int f, bool flash) {
    if (flash && (frame() / 3) % 2 == 0) return;   // blink while invincible
    int hx = (f > 0) ? x + 11 : x + 3;             // head (front)
    // tail
    int tx = (f > 0) ? x + 1 : x + 13;
    line(tx, y + 7, tx + (f > 0 ? -3 : 3), y + 2, CLR_ORANGE);
    // body
    rectfill(x + 3, y + 5, 9, 6, CLR_ORANGE);
    pset(x + 5, y + 5, CLR_DARK_ORANGE);
    pset(x + 8, y + 5, CLR_DARK_ORANGE);
    // legs
    rectfill(x + 4, y + 11, 2, 2, CLR_DARK_ORANGE);
    rectfill(x + 8, y + 11, 2, 2, CLR_DARK_ORANGE);
    // head + ears
    circfill(hx, y + 5, 4, CLR_ORANGE);
    trifill(hx - 3, y + 2, hx - 2, y - 3, hx,     y + 1, CLR_ORANGE);
    trifill(hx,     y + 1, hx + 2, y - 3, hx + 3, y + 2, CLR_ORANGE);
    pset(hx + (f > 0 ? 2 : -2), y + 4, CLR_GREEN);   // eye
}

static void draw_climb() {
    // brick wall
    cls(CLR_DARK_BROWN);
    for (int y = 8; y < GROUND_Y; y += 10) {
        line(0, y, SCREEN_W, y, CLR_BROWNISH_BLACK);
        int off = ((y / 10) & 1) ? 16 : 0;
        for (int x = off; x < SCREEN_W; x += 32) line(x, y, x, y + 10, CLR_BROWNISH_BLACK);
    }

    // windows
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int x = col_x(c), y = win_y(r);
            rectfill(x - 2, y - 2, WIN_W + 4, WIN_H + 4, CLR_DARKER_GREY);   // frame
            if (win_open[r][c]) {
                rectfill(x, y, WIN_W, WIN_H, CLR_LIGHT_YELLOW);             // lit interior
                rectfill(x, y, 5, WIN_H, CLR_RED);                          // curtains
                rectfill(x + WIN_W - 5, y, 5, WIN_H, CLR_RED);
                if ((frame() / 20) % 2) print("in!", x + WIN_W / 2 - 6, y + 8, CLR_DARK_BROWN);
            } else {
                rectfill(x, y, WIN_W, WIN_H, CLR_DARKER_BLUE);              // dark glass
                line(x + WIN_W / 2, y, x + WIN_W / 2, y + WIN_H, CLR_DARK_BLUE);
                line(x, y + WIN_H / 2, x + WIN_W, y + WIN_H / 2, CLR_DARK_BLUE);
            }
            rectfill(x - 3, sill_y(r), WIN_W + 6, 3, CLR_LIGHT_GREY);       // sill / ledge
        }
    }

    // sidewalk
    rectfill(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, CLR_DARK_GREY);
    // trash cans
    rectfill(6, GROUND_Y - 12, 14, 12, CLR_LIGHT_GREY);
    rectfill(4, GROUND_Y - 13, 18, 3, CLR_MEDIUM_GREY);
    rectfill(SCREEN_W - 22, GROUND_Y - 12, 14, 12, CLR_LIGHT_GREY);
    rectfill(SCREEN_W - 24, GROUND_Y - 13, 18, 3, CLR_MEDIUM_GREY);

    // dog
    int dx_ = (int)dogx;
    rectfill(dx_, GROUND_Y - 8, 18, 8, CLR_BROWN);            // body
    rectfill(dx_ + (dogvx > 0 ? 15 : -3), GROUND_Y - 11, 6, 6, CLR_BROWN); // head
    rectfill(dx_ + 2, GROUND_Y, 3, 2, CLR_DARK_BROWN);
    rectfill(dx_ + 12, GROUND_Y, 3, 2, CLR_DARK_BROWN);

    // birds
    for (int i = 0; i < MAX_BIRDS; i++) {
        if (!birds[i].on) continue;
        int bx = (int)birds[i].x, by = (int)birds[i].y;
        int flap = ((frame() / 5) % 2) ? 3 : 0;
        line(bx, by, bx + 5, by - 3 + flap, CLR_DARKER_PURPLE);
        line(bx + 5, by - 3 + flap, bx + 10, by, CLR_DARKER_PURPLE);
        line(bx + 4, by, bx + 6, by, CLR_DARK_PURPLE);
    }

    draw_cat((int)cx, (int)cy, facing, now() < inv_until);

    // HUD
    print(str("SCORE %d", score), 4, 3, CLR_WHITE);
    print_right(str("BEST %d", hiscore), SCREEN_W - 4, 3, CLR_YELLOW);
    // lives as little cat heads
    for (int i = 0; i < lives && i < 9; i++) {
        int ix = 6 + i * 12, iy = 14;
        circfill(ix, iy, 3, CLR_ORANGE);
        trifill(ix - 3, iy - 1, ix - 2, iy - 4, ix, iy - 1, CLR_ORANGE);
        trifill(ix, iy - 1, ix + 2, iy - 4, ix + 3, iy - 1, CLR_ORANGE);
    }

    if (now() - level_start < 2.4f)
        print_centered("REACH A LIT WINDOW!", SCREEN_H / 2, CLR_LIGHT_YELLOW);
}

static void draw_room() {
    // wallpapered room
    cls(CLR_DARKER_PURPLE);
    rectfill(24, 30, SCREEN_W - 48, 150, CLR_MAUVE);
    for (int x = 30; x < SCREEN_W - 30; x += 14)
        for (int y = 36; y < 176; y += 14)
            pset(x, y, CLR_DARK_PURPLE);
    rectfill(24, 172, SCREEN_W - 48, 8, CLR_DARK_BROWN);   // skirting / floor

    // mouse hole
    trifill(SCREEN_W - 34, 180, SCREEN_W - 28, 172, SCREEN_W - 22, 180, CLR_BLACK);

    // mice
    for (int i = 0; i < MAX_MICE; i++) {
        if (!mice[i].on) continue;
        int mx = (int)mice[i].x, my = (int)mice[i].y;
        circfill(mx, my, 3, CLR_LIGHT_GREY);
        circfill(mx + (mice[i].vx > 0 ? 2 : -2), my - 1, 2, CLR_LIGHT_GREY);  // head
        line(mx - (mice[i].vx > 0 ? 3 : -3), my, mx - (mice[i].vx > 0 ? 7 : -7), my + 1, CLR_MEDIUM_GREY); // tail
        pset(mx + (mice[i].vx > 0 ? 3 : -3), my - 1, CLR_BLACK);
    }

    // broom
    int brx = (int)broom_x;
    line(brx + 3, 44, brx - 3, 150, CLR_BROWN);            // handle
    rectfill(brx - 6, 148, 12, 16, CLR_LIGHT_YELLOW);      // bristles
    for (int b = -5; b <= 5; b += 2) line(brx + b, 150, brx + b, 168, CLR_DARK_ORANGE);

    draw_cat((int)cx, (int)cy, facing, false);

    // HUD + timer bar
    print(str("MICE %d", mice_left), 6, 4, CLR_WHITE);
    print_right(str("SCORE %d", score), SCREEN_W - 6, 4, CLR_WHITE);
    print_centered("GOBBLE THE MICE!", 16, CLR_LIGHT_YELLOW);
    int barw = (int)((ROOM_TIME - timer()) / ROOM_TIME * (SCREEN_W - 12));
    if (barw < 0) barw = 0;
    rectfill(6, 26, barw, 2, barw < 60 ? CLR_RED : CLR_GREEN);
}

void draw() {
    if (state == ROOM) draw_room();
    else               draw_climb();

    if (state == GAMEOVER) {
        rectfill(SCREEN_W / 2 - 70, SCREEN_H / 2 - 26, 140, 56, CLR_BLACK);
        rect    (SCREEN_W / 2 - 70, SCREEN_H / 2 - 26, 140, 56, CLR_ORANGE);
        print_centered("OUT OF LIVES",          SCREEN_H / 2 - 16, CLR_RED);
        print_centered(str("SCORE %d", score),  SCREEN_H / 2 - 2,  CLR_YELLOW);
        print_centered("Z to prowl again",      SCREEN_H / 2 + 14, CLR_LIGHT_GREY);
    }
}
