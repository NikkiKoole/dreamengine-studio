/* de:meta
{
  "title": "space invaders",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop"
  ],
  "lineage": "Space Invaders (1978) clone; whole-formation grid march with edge-detect descent, speed-up as ranks thin.",
  "genre": "shooter",
  "homage": "Space Invaders (1978)",
  "description": "Classic arcade shooter. 8×3 alien grid that speeds up as you clear it, 3 enemy bullets, lives. Left/right to move, A to shoot."
}
de:meta */
#include "studio.h"

// space invaders
// left/right to move, A to shoot

#define COLS   8
#define ROWS   3
#define CW     18    // cell width
#define CH     14    // cell height
#define GX     ((SCREEN_W - COLS * CW) / 2)
#define GY     22

// --- state ---
static int   alive[ROWS][COLS];
static float off_x, off_y, spd;
static int   dir;
static int   n_alive;

static float px;
static int   lives, score;

static float pbx, pby;       // player bullet (-1 = inactive)

#define EB 3
static float ebx[EB], eby[EB];   // enemy bullets

static bool  game_over, won;
static int   flash;              // invincibility frames after being hit

// --- helpers ---

static void reset(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            alive[r][c] = 1;
    off_x = 0; off_y = 0; dir = 1;
    n_alive = ROWS * COLS;
    spd = 0.3f;
    px = SCREEN_W / 2.0f;
    lives = 3; score = 0;
    pbx = pby = -1;
    for (int i = 0; i < EB; i++) eby[i] = -1;
    game_over = won = false;
    flash = 0;
}

static int left_col(void) {
    for (int c = 0; c < COLS; c++)
        for (int r = 0; r < ROWS; r++)
            if (alive[r][c]) return c;
    return 0;
}

static int right_col(void) {
    for (int c = COLS-1; c >= 0; c--)
        for (int r = 0; r < ROWS; r++)
            if (alive[r][c]) return c;
    return COLS-1;
}

static void draw_alien(int x, int y, int row) {
    int c  = row == 0 ? CLR_PINK : row == 1 ? CLR_YELLOW : CLR_GREEN;
    int f  = (frame() / 15) % 2;   // leg animation frame
    rectfill(x+2, y+3, 8, 5, c);   // body
    rectfill(x+4, y+1, 4, 3, c);   // head
    rectfill(x,   y+4, 3, 3, c);   // left arm
    rectfill(x+9, y+4, 3, 3, c);   // right arm
    pset(x+4, y+4, CLR_BLACK);     // eyes
    pset(x+7, y+4, CLR_BLACK);
    pset(x + (f ? 3 : 2), y+8, c); // legs (alternate each frame)
    pset(x + (f ? 8 : 9), y+8, c);
}

static void draw_ship(int x) {
    trifill(x, SCREEN_H-10, x+12, SCREEN_H-10, x+6, SCREEN_H-18, CLR_LIGHT_GREY);
    rectfill(x+5, SCREEN_H-22, 2, 5, CLR_LIGHT_GREY);
}

// --- game loop ---

void init(void) { reset(); }

void update(void) {
    if (game_over || won) {
        if (btnp(0, BTN_A)) reset();
        return;
    }

    // move player
    if (btn(0, BTN_LEFT))  px -= 2;
    if (btn(0, BTN_RIGHT)) px += 2;
    px = clamp(px, 0, SCREEN_W - 13);

    // player shoots
    if (btnp(0, BTN_A) && pby < 0) {
        pbx = px + 5;
        pby = SCREEN_H - 24;
    }

    // move player bullet and check hits
    if (pby > 0) {
        pby -= 5;
        if (pby <= 0) {
            pbx = pby = -1;
        } else {
            bool hit = false;
            for (int r = 0; r < ROWS && !hit; r++) {
                for (int c = 0; c < COLS && !hit; c++) {
                    if (!alive[r][c]) continue;
                    int ax = GX + c*CW + (int)off_x;
                    int ay = GY + r*CH + (int)off_y;
                    if (pbx >= ax && pbx <= ax+11 &&
                        pby >= ay && pby <= ay+9) {
                        alive[r][c] = 0;
                        n_alive--;
                        score += (ROWS - r) * 10;
                        pbx = pby = -1;
                        // speed up as aliens die
                        spd = 0.3f + 0.9f * (1.0f - (float)n_alive / (ROWS*COLS));
                        if (n_alive == 0) won = true;
                        hit = true;
                    }
                }
            }
        }
    }

    // move alien grid
    off_x += spd * dir;
    int lx = GX + left_col()  * CW       + (int)off_x;
    int rx = GX + right_col() * CW + 11  + (int)off_x;
    if (dir > 0 && rx >= SCREEN_W - 4) { dir = -1; off_y += 8; }
    if (dir < 0 && lx <= 4)            { dir =  1; off_y += 8; }

    // aliens reached the player?
    for (int r = ROWS-1; r >= 0; r--)
        for (int c = 0; c < COLS; c++)
            if (alive[r][c] &&
                GY + r*CH + (int)off_y + CH >= SCREEN_H - 24)
                { game_over = true; return; }

    // enemy fires occasionally
    if (rnd(60) == 0) {
        for (int i = 0; i < EB; i++) {
            if (eby[i] >= 0) continue;
            int col = rnd(COLS);
            for (int r = ROWS-1; r >= 0; r--) {
                if (alive[r][col]) {
                    ebx[i] = GX + col*CW + 5 + off_x;
                    eby[i] = GY + r*CH  + 9 + off_y;
                    break;
                }
            }
            break;
        }
    }

    // move enemy bullets and check player hit
    if (flash > 0) flash--;
    for (int i = 0; i < EB; i++) {
        if (eby[i] < 0) continue;
        eby[i] += 3;
        if (eby[i] > SCREEN_H) { eby[i] = -1; continue; }
        if (flash == 0 &&
            ebx[i] >= px && ebx[i] <= px+12 &&
            eby[i] >= SCREEN_H-22 && eby[i] <= SCREEN_H-8) {
            eby[i] = -1;
            flash = 90;
            if (--lives == 0) game_over = true;
        }
    }
}

void draw(void) {
    cls(CLR_BLACK);

    if (game_over || won) {
        print_centered(won ? "YOU WIN!" : "GAME OVER", SCREEN_W/2, SCREEN_H/2 - 10, won ? CLR_GREEN : CLR_RED);
        print_centered(str("score: %d", score), SCREEN_W/2, SCREEN_H/2 + 4, CLR_WHITE);
        print_centered("press A to play again", SCREEN_W/2, SCREEN_H/2 + 18, CLR_DARK_GREY);
        return;
    }

    // aliens
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (alive[r][c])
                draw_alien(GX + c*CW + (int)off_x,
                           GY + r*CH + (int)off_y, r);

    // player bullet
    if (pby > 0) rectfill((int)pbx, (int)pby, 2, 7, CLR_YELLOW);

    // enemy bullets
    for (int i = 0; i < EB; i++)
        if (eby[i] > 0)
            rectfill((int)ebx[i], (int)eby[i], 2, 5, CLR_RED);

    // player — flashes while invincible
    if (flash == 0 || (flash / 6) % 2 == 0)
        draw_ship((int)px);

    // ground line
    line(0, SCREEN_H-6, SCREEN_W, SCREEN_H-6, CLR_GREEN);

    // HUD: score left, ship-icons for lives right
    print(str("score %d", score), 4, 2, CLR_WHITE);
    for (int i = 0; i < lives; i++) {
        int lx = SCREEN_W - 14 - i * 14;
        trifill(lx, 7, lx+8, 7, lx+4, 2, CLR_LIGHT_GREY);
    }
}
