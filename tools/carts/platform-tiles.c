/* de:meta
{
  "slug": "platform-tiles",
  "title": "tilemap platformer",
  "status": "active",
  "created": "2026-06-03",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "tilemap-collision",
    "camera-follow",
    "screen-shake-juice"
  ],
  "lineage": "Third in the platform-engine trio (after platform-rects and platform-paint); same polished mover (coyote time, jump buffering, variable height, squash-stretch) now driven by mget() tile data with a scrolling camera.",
  "genre": "platformer",
  "description": "A great-feeling scrolling platformer where the world is a tile map (drawn as colored rects by tile type). The same polished mover as 'platform engine (rects)' — per-axis resolution with flush tile snapping, sub-pixel motion, coyote time, jump buffering, variable jump height, floaty apex, squash/stretch and landing dust — but on tile collision with a camera that scrolls a world bigger than the screen. Collect coins, stomp the purple critters from above, dodge the red spikes, reach the green flag. Arrows/WASD move, Z or up to jump."
}
de:meta */
#include "studio.h"

// TILEMAP PLATFORMER — a great-feeling scrolling platformer where the world is
// a tile map (mget), drawn as colored rects by tile type. It's the third level-
// source in the trio: platform-rects (level as a Box[] array), platform-paint
// (level read from a painted sprite), and this one (level as a tile map you
// paint in the map editor).
//
// The point: the SAME polished mover as platform-rects — per-axis resolution
// with flush tile snapping, sub-pixel float motion, coyote time, jump buffering,
// variable jump height, a floaty apex, squash/stretch and landing dust — but on
// tile collision, with a camera that scrolls a world bigger than the screen.
// (The older `platform` cart felt stiff because it had the collision but none of
// this feel layer: instant velocity, no coyote/buffer, fixed-height jumps.)
//
// Collect coins, stomp the purple critters from above, dodge the red spikes,
// reach the green flag.   Move: arrows / WASD.   Jump: Z or Up.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING ─────────────────────────────────────────────────────────────────
#define GRAV       0.50f
#define MAX_FALL   7.0f
#define RUN_ACCEL  0.7f
#define RUN_FRIC   0.55f
#define AIR_FRIC   0.90f
#define RUN_MAX    2.6f
#define JUMP_V    -8.2f
#define COYOTE     6
#define BUFFER     6
#define CUT_JUMP   0.40f
#define APEX_GRAV  0.55f
#define APEX_BAND  1.2f
#define STOMP_V   -5.0f       // bounce after stomping an enemy

// ── tile ids (must match the tiles map in platform-tiles.cart.js) ───────────
#define T_SOLID   1
#define T_ONEWAY  2
#define T_COIN    3
#define T_SPIKE   4
#define T_GOAL    5
#define T_SPAWN   6
#define T_ENEMY   7

// ── world ────────────────────────────────────────────────────────────────────
#define LEVEL_W 56
#define LEVEL_H 13
#define WORLD_W (LEVEL_W * CELL_W)
#define WORLD_H (LEVEL_H * CELL_H)
#define PW 10
#define PH 14
#define EW 12
#define EH 12

#define MAXEN   16
#define MAXDUST 24

typedef struct { float x, y, vx; bool on; } Enemy;
typedef struct { float x, y, vx, vy; int life, col; } Dust;

STATE {
    float px, py, vx, vy;
    int   face;
    bool  grounded;
    int   coyote, buffer;
    float squash;
    int   coins, flash;
    bool  win;
    int   sx, sy;                  // spawn (world px)
    Enemy en[MAXEN]; int nen;
    Dust  dust[MAXDUST];
};

// ── tiles ────────────────────────────────────────────────────────────────────
static bool tile_solid(int t) { return t == T_SOLID; }

static void spawn_dust(float x, float y, int n) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < MAXDUST; i++)
            if (S->dust[i].life <= 0) {
                S->dust[i] = (Dust){ x + rnd_between(-4, 5), y,
                    rnd_float_between(-1.2f, 1.2f), rnd_float_between(-1.6f, -0.1f),
                    rnd_between(8, 14), (k & 1) ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY };
                break;
            }
}

static void respawn(void) {
    S->px = S->sx; S->py = S->sy; S->vx = 0; S->vy = 0;
    S->flash = 5;
    shake(2.0f);
    note(36, INSTR_NOISE, 4);
}

// ═══ THE MOVER — per-axis resolution against the tile grid, snapping flush ════
// NOTE: cast the WHOLE edge expression — (int)(py + PH), not (int)py + PH — so a
// fractional py doesn't shave a pixel off the foot/edge row and drop the player
// through a (one-way) tile.
static void move_x(void) {
    S->px += S->vx;
    if (S->px < 0)            { S->px = 0; S->vx = 0; }
    if (S->px > WORLD_W - PW) { S->px = WORLD_W - PW; S->vx = 0; }
    int top = (int)S->py / CELL_H, bot = ((int)(S->py + PH) - 1) / CELL_H;
    if (S->vx > 0) {
        int col = ((int)(S->px + PW) - 1) / CELL_W;
        for (int r = top; r <= bot; r++)
            if (tile_solid(mget(col, r))) { S->px = col * CELL_W - PW; S->vx = 0; break; }
    } else if (S->vx < 0) {
        int col = (int)S->px / CELL_W;
        for (int r = top; r <= bot; r++)
            if (tile_solid(mget(col, r))) { S->px = (col + 1) * CELL_W; S->vx = 0; break; }
    }
}

static void move_y(void) {
    float feet_before = S->py + PH;
    S->py += S->vy;
    S->grounded = false;
    int left = (int)S->px / CELL_W, right = ((int)(S->px + PW) - 1) / CELL_W;
    if (S->vy >= 0) {
        int row = (int)(S->py + PH) / CELL_H;          // the surface row just under the feet
        for (int c = left; c <= right; c++) {
            int t = mget(c, row);
            if (tile_solid(t) || (t == T_ONEWAY && feet_before <= row * CELL_H)) {
                S->py = row * CELL_H - PH; S->grounded = true; S->vy = 0; break;
            }
        }
    } else {
        int row = (int)S->py / CELL_H;                 // head row
        for (int c = left; c <= right; c++)
            if (tile_solid(mget(c, row))) { S->py = (row + 1) * CELL_H; S->vy = 0; break; }
    }
}
// ═════════════════════════════════════════════════════════════════════════════

void init(void) {
    S->sx = CELL_W; S->sy = CELL_H;                 // fallback spawn
    S->nen = 0;
    for (int r = 0; r < LEVEL_H; r++)
        for (int c = 0; c < LEVEL_W; c++) {
            int t = mget(c, r);
            if (t == T_SPAWN) {
                S->sx = c * CELL_W; S->sy = (r + 1) * CELL_H - PH;
                mset(c, r, 0);
            } else if (t == T_ENEMY && S->nen < MAXEN) {
                S->en[S->nen++] = (Enemy){ c * CELL_W + (CELL_W - EW) / 2.0f,
                                           (r + 1) * CELL_H - EH, 0.6f, true };
                mset(c, r, 0);
            }
        }
    S->px = S->sx; S->py = S->sy; S->face = 1;
}

void update(void) {
#ifdef DE_TRACE
    watch("p", "px=%.1f py=%.1f vx=%.2f vy=%.2f gnd=%d", S->px, S->py, S->vx, S->vy, S->grounded);
#endif
    if (S->flash > 0) S->flash--;

    if (S->win) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) respawn(), S->win = false;
        return;
    }

    // horizontal: accelerate toward input, else glide to a stop
    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    if (dir != 0) { S->vx = clamp(S->vx + dir * RUN_ACCEL, -RUN_MAX, RUN_MAX); S->face = dir; }
    else { S->vx *= S->grounded ? RUN_FRIC : AIR_FRIC; if (S->vx > -0.1f && S->vx < 0.1f) S->vx = 0; }

    // jump feel: coyote + buffer + variable height
    bool was_gr = S->grounded;
    if (was_gr)             S->coyote = COYOTE;
    else if (S->coyote > 0) S->coyote--;
    if (btnp(0, BTN_A) || btnp(0, BTN_UP)) S->buffer = BUFFER;
    else if (S->buffer > 0)                S->buffer--;
    if (S->buffer > 0 && (S->grounded || S->coyote > 0)) {
        S->vy = JUMP_V; S->buffer = 0; S->coyote = 0; S->squash = -0.7f;
        spawn_dust(S->px + PW / 2, S->py + PH, 4);
        note(72, INSTR_TRI, 3);
    }
    if ((btnr(0, BTN_A) || btnr(0, BTN_UP)) && S->vy < 0) S->vy *= CUT_JUMP;

    float gg = GRAV;
    if (!S->grounded && S->vy > -APEX_BAND && S->vy < APEX_BAND) gg *= APEX_GRAV;
    S->vy += gg;
    if (S->vy > MAX_FALL) S->vy = MAX_FALL;

    move_x();
    move_y();

    if (!was_gr && S->grounded) {
        S->squash = 1.0f;
        spawn_dust(S->px + PW / 2, S->py + PH, 5);
        note(48, INSTR_NOISE, 2);
        shake(0.6f);
    }
    S->squash = lerp(S->squash, 0.0f, 0.22f);

    // tile triggers under the player (coins / spikes / goal)
    int l = (int)S->px / CELL_W, r = ((int)S->px + PW - 1) / CELL_W;
    int tp = (int)S->py / CELL_H, bt = ((int)S->py + PH - 1) / CELL_H;
    for (int c = l; c <= r; c++)
        for (int row = tp; row <= bt; row++) {
            int t = mget(c, row);
            if (t == T_COIN) { mset(c, row, 0); S->coins++; note(84 + S->coins, INSTR_TRI, 3); }
            else if (t == T_SPIKE) { respawn(); return; }
            else if (t == T_GOAL)  { S->win = true; note(84, INSTR_TRI, 5); return; }
        }

    if (S->py > WORLD_H + 40) { respawn(); return; }   // fell in a pit

    // enemies: patrol, stomp from above, hurt on contact
    for (int i = 0; i < S->nen; i++) {
        Enemy *e = &S->en[i];
        if (!e->on) continue;
        e->x += e->vx;
        // turn at a wall ahead or at a ledge (no ground in front)
        int ahead = (e->vx > 0) ? (int)(e->x + EW) / CELL_W : (int)(e->x - 1) / CELL_W;
        int midr  = (int)(e->y + EH / 2) / CELL_H;
        int footr = (int)(e->y + EH) / CELL_H;
        if (tile_solid(mget(ahead, midr)) || !tile_solid(mget(ahead, footr))) e->vx = -e->vx;

        if (boxes_touch((int)S->px, (int)S->py, PW, PH, (int)e->x, (int)e->y, EW, EH)) {
            if (S->vy > 0 && S->py + PH <= e->y + EH / 2 + 2) {   // stomp
                e->on = false;
                S->vy = STOMP_V;
                S->coins += 1;
                spawn_dust(e->x + EW / 2, e->y + EH, 6);
                shake(1.0f);
                note(60, INSTR_SQUARE, 3);
            } else {
                respawn(); return;
            }
        }
    }

    // tick dust
    for (int i = 0; i < MAXDUST; i++)
        if (S->dust[i].life > 0) {
            S->dust[i].x += S->dust[i].vx; S->dust[i].y += S->dust[i].vy;
            S->dust[i].vy += 0.12f; S->dust[i].life--;
        }
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    if (S->flash > 0) { cls(CLR_WHITE); }   // respawn flash (still scroll-follow below)

    follow((int)S->px + PW / 2, (int)S->py + PH / 2, WORLD_W, WORLD_H);

    // tiles as colored rects
    for (int r = 0; r < LEVEL_H; r++)
        for (int c = 0; c < LEVEL_W; c++) {
            int t = mget(c, r), x = c * CELL_W, y = r * CELL_H;
            if (t == T_SOLID)       rectfill(x, y, CELL_W, CELL_H, CLR_BROWN);
            else if (t == T_ONEWAY) rectfill(x, y, CELL_W, 5, CLR_LIGHT_PEACH);
            else if (t == T_COIN)   circfill(x + CELL_W / 2, y + CELL_H / 2, 3, CLR_YELLOW);
            else if (t == T_SPIKE)  trifill(x, y + CELL_H, x + CELL_W / 2, y + 2, x + CELL_W, y + CELL_H, CLR_RED);
            else if (t == T_GOAL) { rectfill(x + CELL_W / 2, y - CELL_H, 2, CELL_H * 2, CLR_LIGHT_GREY);
                                    rectfill(x + CELL_W / 2 + 2, y - CELL_H, 8, 6, CLR_GREEN); }
        }

    // enemies
    for (int i = 0; i < S->nen; i++) {
        Enemy *e = &S->en[i];
        if (!e->on) continue;
        rectfill((int)e->x, (int)e->y, EW, EH, CLR_MAUVE);
        int eye = (e->vx > 0) ? EW - 4 : 2;
        rectfill((int)e->x + eye, (int)e->y + 3, 2, 2, CLR_WHITE);
    }

    // dust
    for (int i = 0; i < MAXDUST; i++)
        if (S->dust[i].life > 0) pset((int)S->dust[i].x, (int)S->dust[i].y, S->dust[i].col);

    // player with squash/stretch
    int sq = (int)(S->squash * 3.0f);
    int w  = PW + (sq > 0 ? sq : 0);
    int h  = PH - sq;
    int x  = (int)(S->px + 0.5f) - (w - PW) / 2;
    int y  = (int)(S->py + 0.5f) + (PH - h);
    rectfill(x, y, w, h, CLR_GREEN);
    rectfill(x + (S->face > 0 ? w - 3 : 1), y + 2, 2, 2, CLR_BLACK);

    // HUD (screen space)
    camera(0, 0);
    rectfill(0, 0, SCREEN_W, 12, CLR_DARKER_BLUE);
    print("reach the flag", 4, 3, CLR_WHITE);
    print(str("coins %d", S->coins), SCREEN_W - 64, 3, CLR_YELLOW);
    if (S->win) {
        print_centered("YOU WIN!", SCREEN_W / 2, SCREEN_H / 2 - 8, CLR_YELLOW);
        print_centered("press Z to play again", SCREEN_W / 2, SCREEN_H / 2 + 4, CLR_WHITE);
    }
}
