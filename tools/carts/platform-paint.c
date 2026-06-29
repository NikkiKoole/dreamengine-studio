/* de:meta
{
  "title": "paint a level",
  "status": "active",
  "created": "2026-06-03",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "tilemap-collision"
  ],
  "lineage": "Same rect-mover as platform-rects; the novel element is using a painted sprite slot as level data — sget() reads each pixel's palette index to classify block type at init, so repainting the sprite changes the level without touching code.",
  "genre": "platformer",
  "description": "The same pixel-perfect platformer mover as 'platform engine (rects)', but the level is painted, not coded. Edit sprite slot 0: 1 pixel = 1 block, and the color is the type — brown solid, light-peach one-way, red hazard, yellow coin, green spawn. At startup the cart reads the sprite back with sget() and builds the world, so repainting the sprite makes a whole new level with no code change. Collect the coins, avoid the red spikes. Arrows/WASD move, Z or up to jump."
}
de:meta */
#include "studio.h"

// PAINT-A-LEVEL — the same pixel-perfect mover as `platform engine (rects)`,
// but the level is DRAWN, not coded. Open the sprites tab, edit slot 0, and
// paint a 16×16 picture where 1 pixel = 1 block and the COLOR is the block type:
//
//     brown        → solid wall / floor      (CLR_BROWN)
//     light peach  → one-way platform        (CLR_LIGHT_PEACH)   jump up through it
//     red          → hazard (touch = respawn)(CLR_RED)
//     yellow       → coin                    (CLR_YELLOW)
//     green        → player spawn            (CLR_GREEN)
//     black/empty  → air
//
// At startup init() walks the sprite with sget() and builds the world from the
// colors. Repaint the sprite, re-run, and you have a new level — no code change.
//
// TWO WAYS TO MAKE A LEVEL: this cart reads its level from a painted sprite
// (level = data); its sibling `platform-rects` hard-codes a Box[] array
// (level = code). Same engine underneath — only the level source differs.
//
// Move: arrows / WASD.   Jump: Z or Up.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING ─────────────────────────────────────────────────────────────────
#define GRAV       0.50f
#define MAX_FALL   7.0f
#define RUN_ACCEL  0.7f
#define RUN_FRIC   0.55f
#define AIR_FRIC   0.90f
#define RUN_MAX    2.4f
#define JUMP_V    -7.6f
#define COYOTE     6
#define BUFFER     6
#define CUT_JUMP   0.40f
#define APEX_GRAV  0.55f
#define APEX_BAND  1.2f

// ── LEVEL LAYOUT ─────────────────────────────────────────────────────────────
#define LV   16        // level is 16×16 blocks (one sprite slot)
#define BLK  12        // pixels per block
#define OX   64        // world origin — centers the 192×192 room on screen
#define OY    4
#define PW    8        // player size (fits through a BLK-wide gap)
#define PH   10

// block types
#define T_SOLID  0
#define T_ONEWAY 1
#define T_HAZARD 2

typedef struct { int x, y, w, h, type; } Box;

#define MAXBLK  (LV * LV)
#define MAXCOIN 64

STATE {
    float px, py, vx, vy;
    int   face;
    bool  grounded;
    int   coyote, buffer;
    float squash;
    int   flash;                  // respawn flash, frames
    int   score;
    int   sx, sy;                 // spawn point (world px)
    // the level — built once in init(), lives in STATE so it survives a reload
    Box   blk[MAXBLK]; int nblk;
    struct { int x, y; bool got; } coin[MAXCOIN]; int ncoin;
};

// ── read the painted sprite → build the world ───────────────────────────────
static void build_level(void) {
    S->nblk = 0; S->ncoin = 0; S->score = 0;
    S->sx = OX + BLK; S->sy = OY + BLK;            // fallback spawn
    for (int gy = 0; gy < LV; gy++)
    for (int gx = 0; gx < LV; gx++) {
        int c  = sget(gx, gy);                     // palette index of the pixel
        int wx = OX + gx * BLK, wy = OY + gy * BLK;
        if      (c == CLR_BROWN)       S->blk[S->nblk++] = (Box){ wx, wy, BLK, BLK, T_SOLID  };
        else if (c == CLR_LIGHT_PEACH) S->blk[S->nblk++] = (Box){ wx, wy, BLK, BLK, T_ONEWAY };
        else if (c == CLR_RED)         S->blk[S->nblk++] = (Box){ wx, wy, BLK, BLK, T_HAZARD };
        else if (c == CLR_YELLOW && S->ncoin < MAXCOIN) {
            S->coin[S->ncoin].x = wx; S->coin[S->ncoin].y = wy; S->coin[S->ncoin].got = false;
            S->ncoin++;
        }
        else if (c == CLR_GREEN)      { S->sx = wx; S->sy = wy; }
    }
}

static void respawn(void) {
    S->px = S->sx; S->py = S->sy;
    S->vx = 0; S->vy = 0;
    S->flash = 5;
    shake(2.0f);
    note(36, INSTR_NOISE, 4);
}

// ═══ THE MOVER (identical to platform-rects, now over S->blk[]) ══════════════
static bool overlap(float ax, float ay, int aw, int ah, Box b) {
    return ax < b.x + b.w && ax + aw > b.x &&
           ay < b.y + b.h && ay + ah > b.y;
}

static void move_x(void) {
    S->px += S->vx;
    for (int i = 0; i < S->nblk; i++) {
        if (S->blk[i].type != T_SOLID) continue;          // only solids block sideways
        if (!overlap(S->px, S->py, PW, PH, S->blk[i])) continue;
        if      (S->vx > 0) S->px = S->blk[i].x - PW;
        else if (S->vx < 0) S->px = S->blk[i].x + S->blk[i].w;
        S->vx = 0;
    }
}

static void move_y(void) {
    float feet_before = S->py + PH;
    S->py += S->vy;
    S->grounded = false;
    for (int i = 0; i < S->nblk; i++) {
        if (S->blk[i].type == T_HAZARD) continue;         // hazards don't physically stop you
        if (!overlap(S->px, S->py, PW, PH, S->blk[i])) continue;
        if (S->blk[i].type == T_ONEWAY) {                 // catch only from above, while falling
            if (S->vy > 0 && feet_before <= S->blk[i].y) {
                S->py = S->blk[i].y - PH;
                S->grounded = true;
                S->vy = 0;
            }
            continue;
        }
        if (S->vy > 0) { S->py = S->blk[i].y - PH; S->grounded = true; }
        else if (S->vy < 0) S->py = S->blk[i].y + S->blk[i].h;
        S->vy = 0;
    }
}
// ═════════════════════════════════════════════════════════════════════════════

void init(void) {
    build_level();
    S->px = S->sx; S->py = S->sy; S->face = 1;
}

void update(void) {
    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    if (dir != 0) {
        S->vx = clamp(S->vx + dir * RUN_ACCEL, -RUN_MAX, RUN_MAX);
        S->face = dir;
    } else {
        S->vx *= S->grounded ? RUN_FRIC : AIR_FRIC;
        if (S->vx > -0.1f && S->vx < 0.1f) S->vx = 0;
    }

    bool was_gr = S->grounded;
    if (was_gr)             S->coyote = COYOTE;
    else if (S->coyote > 0) S->coyote--;

    if (btnp(0, BTN_A) || btnp(0, BTN_UP)) S->buffer = BUFFER;
    else if (S->buffer > 0)                S->buffer--;

    if (S->buffer > 0 && (S->grounded || S->coyote > 0)) {
        S->vy = JUMP_V;
        S->buffer = 0; S->coyote = 0;
        S->squash = -0.7f;
        note(72, INSTR_TRI, 3);
    }
    if ((btnr(0, BTN_A) || btnr(0, BTN_UP)) && S->vy < 0) S->vy *= CUT_JUMP;

    float g = GRAV;
    if (!S->grounded && S->vy > -APEX_BAND && S->vy < APEX_BAND) g *= APEX_GRAV;
    S->vy += g;
    if (S->vy > MAX_FALL) S->vy = MAX_FALL;

    move_x();
    move_y();

    if (!was_gr && S->grounded) {
        S->squash = 1.0f;
        note(48, INSTR_NOISE, 2);
        shake(0.6f);
    }
    S->squash = lerp(S->squash, 0.0f, 0.22f);

    // coins: grab on touch
    for (int i = 0; i < S->ncoin; i++) {
        if (S->coin[i].got) continue;
        if (boxes_touch((int)S->px, (int)S->py, PW, PH, S->coin[i].x + 2, S->coin[i].y + 2, BLK - 4, BLK - 4)) {
            S->coin[i].got = true;
            S->score++;
            note(84 + S->score, INSTR_TRI, 3);    // pitch climbs with each coin
        }
    }
    // hazards: touch = respawn
    for (int i = 0; i < S->nblk; i++)
        if (S->blk[i].type == T_HAZARD &&
            boxes_touch((int)S->px, (int)S->py, PW, PH, S->blk[i].x, S->blk[i].y, S->blk[i].w, S->blk[i].h)) {
            respawn();
            break;
        }

    // fell out of the room → respawn
    if (S->py > SCREEN_H + 40) respawn();

    if (S->flash > 0) S->flash--;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    if (S->flash > 0) { cls(CLR_WHITE); return; }      // respawn flash

    // the painted blocks — drawn in their own colors (what you paint is what you see)
    for (int i = 0; i < S->nblk; i++) {
        int col = (S->blk[i].type == T_SOLID)  ? CLR_BROWN
                : (S->blk[i].type == T_ONEWAY) ? CLR_LIGHT_PEACH
                :                                CLR_RED;
        rectfill(S->blk[i].x, S->blk[i].y, S->blk[i].w, S->blk[i].h, col);
    }
    // coins
    for (int i = 0; i < S->ncoin; i++)
        if (!S->coin[i].got)
            circfill(S->coin[i].x + BLK / 2, S->coin[i].y + BLK / 2, 3, CLR_YELLOW);

    // player with squash/stretch
    int sq = (int)(S->squash * 3.0f);
    int w  = PW + (sq > 0 ? sq : 0);
    int h  = PH - sq;
    int x  = (int)(S->px + 0.5f) - (w - PW) / 2;
    int y  = (int)(S->py + 0.5f) + (PH - h);
    rectfill(x, y, w, h, CLR_GREEN);
    rectfill(x + (S->face > 0 ? w - 3 : 1), y + 2, 2, 2, CLR_BLACK);

    rectfill(0, 0, SCREEN_W, 12, CLR_DARKER_BLUE);
    print("level = sprite slot 0", 4, 3, CLR_WHITE);
    print(str("coins %d/%d", S->score, S->ncoin), SCREEN_W - 76, 3, CLR_YELLOW);
}
