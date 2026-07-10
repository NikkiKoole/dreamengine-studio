/* de:meta
{
  "slug": "bunnymark",
  "title": "bunnymark",
  "status": "active",
  "created": "2026-06-25",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "lineage": "The canonical Flash/Pixi.js bunnymark (Iain Lobb / Goodboy Digital), ported as the worked sprite-throughput demonstrator for the DE_SOFTWARE_CANVAS render-backend toggle — GPU immediate-mode vs the CPU software canvas, the same spr() blits each way.",
  "description": "The classic sprite-throughput benchmark. Hold A (or the mouse / a finger) and bunnies pour out of the corner, bounce off a gravity floor and the walls, and never stop while a live counter climbs and the FPS readout holds at 60 — until the machine runs out of blit budget and the whole swarm slides into slow-motion. The number it sags at IS the benchmark, and a \"60fps held @ N\" line keeps the session's high score. Every bunny is one sspr() of a 32×32 sprite — a hand bunny posterised into the pico32 palette and tinted five ways via palette-index ramps (NOT pal()-recoloured: pal() flushes the GPU sprite batch per call and runs a per-pixel recolor on the software canvas, so baking the tint into the sheet keeps the loop a pure blit), making it a clean test of the sprite-blit path — exactly the workload the render-backend toggle swaps: open settings → rendering and flip between hardware (GPU immediate-mode) and software (CPU canvas) to watch the held-bunny count move between the two engines. Physics is a fixed per-frame step on purpose, so the slowdown shows as honest slow-motion instead of teleporting. HOLD A/mouse/touch to pour, B to reset."
}
de:meta */
#include "studio.h"
#include "ui.h"      // ui_spr_button — clickable sprite-icon toggle buttons
#include <stdio.h>   // snprintf for the HUD readouts
#include <math.h>    // sinf for the scale pulse

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
// The art is bunnymark_bunny.png posterized into the pico32 palette and tinted 5
// ways via palette-index ramps — see bunnymark.cart.js. Index 0 is transparent
// (colorkey). Physics is a FIXED per-frame step (no dt()) — deliberately: a
// benchmark wants the slowdown VISIBLE, so an over-budget frame goes slow-mo
// instead of teleporting, and you SEE the cliff you're measuring.
//
// X spins the bunnies and C pulse-scales them — both drop the sprite OFF the
// axis-aligned blit fast path: rotation routes through the inverse-map rasterizer
// (de_cpu_sspr_rot) and scale through the general per-pixel loop. On the GPU
// they're ~free (the hardware transforms the quad either way); on the software
// canvas they cost several× more per sprite. So the two toggles turn bunnymark
// into a two-axis demonstrator: flip render mode AND spin/scale to feel the gap.
//
//   HOLD A / mouse / touch — pour      X — spin      C — pulse-scale      R — reset
// ─────────────────────────────────────────────────────────────────────────────

#define MAXB     200000           // hard cap (≈4 MB of bunnies; you'll hit the fps wall first)
#define ADD_RATE 200              // bunnies added per frame while held
#define START_N  400              // a lively swarm already bouncing on load (32×32 → fills the screen)
#define GRAVITY  0.30f
#define BOUNCE   0.85f            // floor restitution
#define BUNNY_SZ 32

// HUD sprite buttons — ui_spr_button (ui.h): a 16×16 icon centred in the rect, with
// ui.h's capture/hit-pad/focus + state colouring. Icons in bunnymark.cart.js slots 18/19.
#define BTN_ROW_Y       3
#define BTN_SZ      18            // 18 = 16px icon + 1px padding each side
#define BTN_SPIN_X  112
#define BTN_SCALE_X 132
#define ICON_SPIN   18
#define ICON_SCALE  19

// each tint is a 32×32 region of the sheet (laid out in bunnymark.cart.js):
//   0 white · 1 yellow · 2 pink · 3 green · 4 orange
static const int BTX[5] = { 0, 32, 64, 96, 0 };
static const int BTY[5] = { 0,  0,  0,  0, 32 };

typedef struct { float x, y, vx, vy; unsigned char tint; } Bunny;
static Bunny bun[MAXB];
static int   n;                   // live bunny count
static int   best_held;           // most bunnies while still holding ~60fps (session high score)
static bool  spin_on;             // X — rotate each bunny (off the blit fast path → rotated rasterizer)
static bool  scale_on;            // C — pulse the bunny size (off the fast path → scaled blit)

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
    colorkey(0);                        // index 0 = transparent (background of the posterized bunny)
    n = 0; best_held = 0;
    for (int i = 0; i < START_N; i++) seed_scattered();
}

void update(void) {
    if (keyp('R')) init();                                   // reset (keeps spin/scale modes)
    if (keyp('X')) spin_on  = !spin_on;                      // keyboard mirrors the buttons
    if (keyp('C')) scale_on = !scale_on;
    // pour while held — but not when the pointer is over a HUD button (that press is a
    // button click, toggled by ui_spr_button in draw()).
    int mx = mouse_x(), my = mouse_y();
    int on_btn = (mx >= BTN_SPIN_X  && mx < BTN_SPIN_X  + BTN_SZ && my >= BTN_ROW_Y && my < BTN_ROW_Y + BTN_SZ) ||
                 (mx >= BTN_SCALE_X && mx < BTN_SCALE_X + BTN_SZ && my >= BTN_ROW_Y && my < BTN_ROW_Y + BTN_SZ);
    if (btn(0, BTN_A) || (mouse_down(0) && !on_btn))
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
    ui_begin();                          // ui.h: snapshot contacts/focus before any widgets

    // bunnies. Plain (no spin/scale): one sspr() of the pre-tinted 32×32 sheet
    // region — the axis-aligned blit fast path (GPU batches all into ~1 draw call;
    // software does a straight row copy). With spin or scale on, each bunny goes
    // through sspr_ex (rotate + scale): ~free on the GPU, but off the fast path on
    // the software canvas (rotated rasterizer / per-pixel scaled blit). That gap is
    // the whole point of the two toggles.
    int f = frame();
    for (int i = 0; i < n; i++) {
        int t = bun[i].tint;
        int x = (int)bun[i].x, y = (int)bun[i].y;
        if (spin_on || scale_on) {
            int   sz  = BUNNY_SZ;
            if (scale_on) { float s = 1.0f + 0.4f * sinf(f * 0.06f + i * 0.5f); sz = (int)(BUNNY_SZ * s); if (sz < 2) sz = 2; }
            float deg = spin_on ? (f * 3.0f + i * 37.0f) : 0.0f;
            sspr_ex(BTX[t], BTY[t], BUNNY_SZ, BUNNY_SZ, x, y, sz, sz, deg, sz / 2, sz / 2);
        } else {
            sspr(BTX[t], BTY[t], BUNNY_SZ, BUNNY_SZ, x, y, BUNNY_SZ, BUNNY_SZ);
        }
    }

    // ── HUD ──────────────────────────────────────────────────────────────
    rectfill(0, 0, SCREEN_W, 22, CLR_BLACK);
    char buf[48];
    snprintf(buf, sizeof buf, "BUNNIES %d", n);
    print(buf, 4, 3, CLR_WHITE);

    int fp = fps();
    int ms = fp > 0 ? (1000 + fp / 2) / fp : 0;             // rounded ms/frame
    snprintf(buf, sizeof buf, "%d FPS  %d ms", fp, ms);
    print(buf, 4, 12, fp >= 55 ? CLR_LIME_GREEN : (fp >= 30 ? CLR_YELLOW : CLR_RED));

    // SPIN / SCALE as cute clickable sprite buttons (lit when on) — both push the
    // bunnies OFF the blit fast path. Keyboard X / C mirror them. (ui.h sprite buttons.)
    if (ui_spr_button(ICON_SPIN,  BTN_SPIN_X,  BTN_ROW_Y, BTN_SZ, BTN_SZ, spin_on))  spin_on  = !spin_on;
    if (ui_spr_button(ICON_SCALE, BTN_SCALE_X, BTN_ROW_Y, BTN_SZ, BTN_SZ, scale_on)) scale_on = !scale_on;

    snprintf(buf, sizeof buf, "60fps held @ %d", best_held);
    print(buf, SCREEN_W - text_width(buf) - 4, 3, CLR_LIGHT_GREY);

    font(FONT_TINY);
    print("A/mouse pour   X spin   C scale   R reset   settings>rendering: GPU vs software", 4, SCREEN_H - 7, CLR_DARK_GREY);
    font(FONT_NORMAL);

    ui_end();                            // ui.h: resolve clicks against the buttons drawn above
}
