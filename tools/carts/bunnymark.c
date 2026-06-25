#include "studio.h"
#include <stdio.h>   // snprintf for the HUD readouts

// ── BUNNYMARK — the classic sprite-throughput benchmark ──────────────────────
// Hold A (or the mouse / a finger) and bunnies pour out of the corner, bounce
// off the gravity floor and the walls, and never stop. The counter climbs; the
// framerate holds at 60 until the machine runs out of blit budget, then sags.
// The number it sags at IS the benchmark — and the "60fps held @ N" line marks
// the high score of the session.
//
// THE POINT (and why it ships beside the software-canvas work): bunnymark is the
// canonical SPRITE-blit stress test — every bunny is one sspr() of a 32×32 sprite,
// pre-tinted into 5 colours baked into the sheet (NOT pal()-recoloured: pal() is
// pathological here — it flushes the GPU batch per sprite and runs a per-pixel
// recolor on the software canvas, so baking the colour keeps the loop a pure
// blit). That's exactly the workload the render-backend toggle swaps. Open
// settings → rendering, flip between "hardware (GPU)" and "software (CPU canvas)"
// and watch the held-bunny count move: the GPU rasterises 32×32 textured quads in
// hardware; the software canvas blits them on the CPU. One honest number each.
//
// The art is a real true-colour PNG (tools/carts/bunnymark_bunny.png) tinted 5
// ways — see bunnymark.cart.js. Transparency is the sprite's own alpha (no
// colorkey). Physics is a FIXED per-frame step (no dt()) — deliberately: a
// benchmark wants the slowdown VISIBLE, so an over-budget frame goes slow-mo
// instead of teleporting, and you SEE the cliff you're measuring.
//
//   HOLD A / mouse / touch — pour bunnies      B — reset
// ─────────────────────────────────────────────────────────────────────────────

#define MAXB     200000           // hard cap (≈4 MB of bunnies; you'll hit the fps wall first)
#define ADD_RATE 200              // bunnies added per frame while held
#define START_N  400              // a lively swarm already bouncing on load (32×32 → fills the screen)
#define GRAVITY  0.30f
#define BOUNCE   0.85f            // floor restitution
#define BUNNY_SZ 32

// each tint is a 32×32 region of the sheet (laid out in bunnymark.cart.js):
//   0 white · 1 yellow · 2 pink · 3 green · 4 orange
static const int BTX[5] = { 0, 32, 64, 96, 0 };
static const int BTY[5] = { 0,  0,  0,  0, 32 };

typedef struct { float x, y, vx, vy; unsigned char tint; } Bunny;
static Bunny bun[MAXB];
static int   n;                   // live bunny count
static int   best_held;           // most bunnies while still holding ~60fps (session high score)

// random float in [0, m)
static float frnd(float m) { return (rnd(10001) / 10000.0f) * m; }

static void spawn_one(void) {
    if (n >= MAXB) return;
    Bunny *b = &bun[n++];
    b->x = 2; b->y = 2;                 // fountain out of the top-left corner (the classic look)
    b->vx = frnd(5.0f);                 // rightward spray
    b->vy = frnd(5.0f) - 2.5f;          // a little up or down
    b->tint = (unsigned char)rnd(5);
}

// initial swarm: scattered across the screen + already moving, so the cart reads
// at a glance the instant it loads (HOLD A then fountains more from the corner).
static void seed_scattered(void) {
    if (n >= MAXB) return;
    Bunny *b = &bun[n++];
    b->x = frnd(SCREEN_W - BUNNY_SZ);
    b->y = 24 + frnd(SCREEN_H - BUNNY_SZ - 24);
    b->vx = frnd(5.0f) - 2.5f;
    b->vy = frnd(5.0f) - 2.5f;
    b->tint = (unsigned char)rnd(5);
}

void init(void) {
    n = 0; best_held = 0;               // transparency is the sprite's alpha — no colorkey() needed
    for (int i = 0; i < START_N; i++) seed_scattered();
}

void update(void) {
    if (btnp(0, BTN_B)) init();                              // reset
    if (btn(0, BTN_A) || mouse_down(0))                      // pour while held
        for (int i = 0; i < ADD_RATE; i++) spawn_one();

    const float floor_y = SCREEN_H - BUNNY_SZ;
    const float wall_x  = SCREEN_W - BUNNY_SZ;
    for (int i = 0; i < n; i++) {
        Bunny *b = &bun[i];
        b->vy += GRAVITY;
        b->x  += b->vx;
        b->y  += b->vy;
        if (b->x < 0)      { b->x = 0;      b->vx = -b->vx; }
        if (b->x > wall_x) { b->x = wall_x; b->vx = -b->vx; }
        if (b->y > floor_y) {
            b->y  = floor_y;
            b->vy = -b->vy * BOUNCE;
            if (rnd(100) < 12) b->vy -= frnd(4.0f);         // the odd extra kick → a lively swarm
        }
        if (b->y < 0) { b->y = 0; b->vy = 0; }
    }

    // session high-water mark: biggest swarm still running smooth (fps is smoothed)
    if (fps() >= 58 && n > best_held) best_held = n;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // bunnies — each is one plain sspr() of its pre-tinted 32×32 sheet region. No
    // pal(): the colour is baked in, so the GPU batches every bunny into one draw
    // call and the software canvas does a straight copy (no per-pixel recolor).
    // This is the whole benchmark — keep it a pure blit.
    for (int i = 0; i < n; i++) {
        int t = bun[i].tint;
        sspr(BTX[t], BTY[t], BUNNY_SZ, BUNNY_SZ, (int)bun[i].x, (int)bun[i].y, BUNNY_SZ, BUNNY_SZ);
    }

    // ── HUD ──────────────────────────────────────────────────────────────
    rectfill(0, 0, SCREEN_W, 22, CLR_BLACK);
    char buf[48];
    snprintf(buf, sizeof buf, "BUNNIES %d", n);
    print(buf, 4, 3, CLR_WHITE);

    int f = fps();
    int ms = f > 0 ? (1000 + f / 2) / f : 0;                // rounded ms/frame
    snprintf(buf, sizeof buf, "%d FPS  %d ms", f, ms);
    print(buf, 4, 12, f >= 55 ? CLR_LIME_GREEN : (f >= 30 ? CLR_YELLOW : CLR_RED));

    snprintf(buf, sizeof buf, "60fps held @ %d", best_held);
    print(buf, SCREEN_W - text_width(buf) - 4, 3, CLR_LIGHT_GREY);

    font(FONT_TINY);
    print("HOLD A/mouse: pour   B: reset   settings>rendering: GPU vs software", 4, SCREEN_H - 7, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
