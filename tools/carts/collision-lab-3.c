#include "studio.h"

// COLLISION LAB 3 — the world. Labs 1 and 2 tested two OBJECTS against each
// other. This one is about colliding with the ENVIRONMENT, which comes in
// three flavors, cheapest first:
//
//  • EDGES — pure math on your coordinates. The screen border needs no data at
//    all: clamp (stop at the wall), bounce (flip the velocity), or wrap (come
//    in the other side). Press X to cycle all three and feel the difference.
//
//  • TILES — ask the grid. The brown blocks live in the tile MAP; the ball's
//    bounding box is tested with touching_map(), which just checks the handful
//    of cells the box overlaps (highlighted live). Cheap, the workhorse of
//    most 2D games. tile_at(mouse) reads the cell under your cursor.
//
//  • PIXELS — ask the canvas. The green blob is NOT data — it's just pixels
//    drawn on screen — yet touching_color() collides with it by reading the
//    canvas back. Slow but completely flexible: anything you can draw, you can
//    collide with (see pget under your cursor). This is how scorched-earth /
//    Worms terrain works.
//
// Fly: arrows accelerate the ball (it keeps momentum, lab-2 style).
// X cycles the edge behavior. Z resets.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

#define ACCEL 0.18f
#define DRAG  0.985f
#define BR    5            // ball radius (its box is BR*2 square)
#define HUDH  12

STATE {
    float px, py, vx, vy;
    int   edge_mode;       // 0 clamp, 1 bounce, 2 wrap
};

static void reset_game(void) {
    S->px = 60; S->py = 130; S->vx = 0; S->vy = 0;
    S->edge_mode = 1;
}

void init(void) {
    reset_game();
    // build the tile obstacles once: a plus and a wall column in the MAP
    mset(6, 5, 1); mset(7, 5, 1); mset(8, 5, 1); mset(7, 4, 1); mset(7, 6, 1);
    for (int r = 2; r <= 7; r++) mset(13, r, 1);
}

void update(void) {
    if (btnp(0, BTN_A)) reset_game();
    if (btnp(0, BTN_B)) S->edge_mode = (S->edge_mode + 1) % 3;

    if (btn(0, BTN_LEFT))  S->vx -= ACCEL;
    if (btn(0, BTN_RIGHT)) S->vx += ACCEL;
    if (btn(0, BTN_UP))    S->vy -= ACCEL;
    if (btn(0, BTN_DOWN))  S->vy += ACCEL;
    S->vx *= DRAG; S->vy *= DRAG;

    float prevx = S->px, prevy = S->py;
    S->px += S->vx; S->py += S->vy;

    // ── flavor 1: EDGES — three behaviors, same wall ─────────────────────────
    if (S->edge_mode == 0) {                       // clamp: stop dead
        if (S->px < BR)            { S->px = BR;            S->vx = 0; }
        if (S->px > SCREEN_W - BR) { S->px = SCREEN_W - BR; S->vx = 0; }
        if (S->py < HUDH + BR)     { S->py = HUDH + BR;     S->vy = 0; }
        if (S->py > SCREEN_H - BR) { S->py = SCREEN_H - BR; S->vy = 0; }
    } else if (S->edge_mode == 1) {                // bounce: flip the velocity
        if (S->px < BR)            { S->px = BR;            S->vx = -S->vx; }
        if (S->px > SCREEN_W - BR) { S->px = SCREEN_W - BR; S->vx = -S->vx; }
        if (S->py < HUDH + BR)     { S->py = HUDH + BR;     S->vy = -S->vy; }
        if (S->py > SCREEN_H - BR) { S->py = SCREEN_H - BR; S->vy = -S->vy; }
    } else {                                       // wrap: out one side, in the other
        if (S->px < 0) S->px += SCREEN_W;  if (S->px >= SCREEN_W) S->px -= SCREEN_W;
        if (S->py < HUDH) S->py += SCREEN_H - HUDH; if (S->py >= SCREEN_H) S->py -= SCREEN_H - HUDH;
    }

    // ── flavor 2: TILES — detect via the grid, crude bounce on hit ──────────
    // (deliberately crude: restore + flip. A real mover resolves per-axis —
    // see platform-tiles for the proper version.)
    if (touching_map((int)(S->px - BR), (int)(S->py - BR), BR * 2, BR * 2)) {
        S->px = prevx; S->py = prevy;
        S->vx = -S->vx * 0.7f; S->vy = -S->vy * 0.7f;
        note(45, INSTR_NOISE, 2);
    }

#ifdef DE_TRACE
    watch("l", "tmap=%d tcol=%d mode=%d px=%.0f py=%.0f",
          touching_map((int)(S->px - BR), (int)(S->py - BR), BR * 2, BR * 2),
          touching_color((int)(S->px - BR), (int)(S->py - BR), BR * 2, BR * 2, CLR_LIME_GREEN),
          S->edge_mode, S->px, S->py);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    int bx = (int)S->px, by = (int)S->py;

    // ── flavor 3 target: the PIXEL blob — no data, just drawing ─────────────
    circfill(258, 80, 20, CLR_LIME_GREEN);
    circfill(243, 95, 12, CLR_LIME_GREEN);
    circfill(272, 100, 10, CLR_LIME_GREEN);
    // touching_color reads these pixels straight off the canvas
    bool tcol = touching_color(bx - BR, by - BR, BR * 2, BR * 2, CLR_LIME_GREEN);

    // ── the tile obstacles, drawn by reading the map back ───────────────────
    for (int cy = 0; cy < 12; cy++)
        for (int cx = 0; cx < 20; cx++)
            if (mget(cx, cy)) {
                rectfill(cx * 16, cy * 16, 16, 16, CLR_BROWN);
                rect(cx * 16, cy * 16, 16, 16, CLR_DARK_BROWN);
            }
    // highlight the cells the ball's box currently overlaps — what
    // touching_map actually looks at
    int c0 = (bx - BR) / 16, c1 = (bx + BR - 1) / 16;
    int r0 = (by - BR) / 16, r1 = (by + BR - 1) / 16;
    for (int cy = r0; cy <= r1; cy++)
        for (int cx = c0; cx <= c1; cx++)
            rect(cx * 16, cy * 16, 16, 16, mget(cx, cy) ? CLR_RED : CLR_DARK_GREY);
    bool tmap = touching_map(bx - BR, by - BR, BR * 2, BR * 2);

    // ── the ball + its bounding box ──────────────────────────────────────────
    rect(bx - BR, by - BR, BR * 2, BR * 2, CLR_DARKER_GREY);
    circfill(bx, by, BR, tcol ? CLR_LIME_GREEN : (tmap ? CLR_RED : CLR_WHITE));
    circ(bx, by, BR, CLR_LIGHT_GREY);

    // ── zone labels: what to DO with each demo ───────────────────────────────
    font(FONT_SMALL);
    print("TILES: bump into me", 84, 116, CLR_PEACH);
    print("PIXELS: touch me", 228, 118, CLR_LIME_GREEN);
    print_right("EDGES: fly off the screen", SCREEN_W - 4, 154, CLR_LIGHT_YELLOW);

    // ── readouts ─────────────────────────────────────────────────────────────
    int mx = mouse_x(), my = mouse_y();
    static const char *mode_name[3] = { "clamp", "bounce", "wrap" };
    print(str("edges: %s (X cycles)", mode_name[S->edge_mode]), 4, 170, CLR_LIGHT_YELLOW);
    print(str("touching_map(ball)   = %s", tmap ? "true" : "false"), 4, 178, tmap ? CLR_RED : CLR_LIGHT_GREY);
    print(str("touching_color(ball, GREEN) = %s", tcol ? "true" : "false"), 4, 186, tcol ? CLR_GREEN : CLR_LIGHT_GREY);
    print(str("tile_at(mouse) = %d", tile_at(mx, my)), 200, 178, CLR_LIGHT_GREY);
    print(str("pget(mouse)    = %d", pget(mx, my)), 200, 186, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // the mouse marks the CELL it is over (what tile_at reads) — no fake cursor,
    // your real pointer is the pointer
    if (my > HUDH && my < 164)
        rect((mx / 16) * 16, (my / 16) * 16, 16, 16, CLR_INDIGO);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("collision lab 3: the world", 4, 2, CLR_WHITE);
    font(FONT_SMALL);
    print("arrows fly the ball   X: edge mode   Z: reset", 4, SCREEN_H - 6, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
