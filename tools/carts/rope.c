/* de:meta
{
  "slug": "rope",
  "title": "verlet rope",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "verlet-integration"
  ],
  "description": "How games fake rope and cloth, cheaply. The rope is a line of points; each frame every point keeps moving the way it was already going (Verlet integration stores the previous position) and gravity tugs it down. Then the links are relaxed a few times — nudging each pair back to the right segment length — and believable rope falls out, no springs or forces. Arrows swing the anchor, Z flicks the tail, X flips gravity.",
  "todo": [
    "Mouse: let the player position the root, and make the red anchor draggable.",
    "Add a knob to change rope length.",
    "Replace the Z/X actions with cute pixel buttons."
  ]
}
de:meta */
#include "studio.h"
#include <math.h>

// VERLET ROPE — how games fake rope and cloth, cheaply.
//
// The rope is just a line of points. Each frame, every point keeps moving the
// way it was already going (we store its previous spot and let the difference
// be its velocity — that's "Verlet" integration) and gravity tugs it down.
// Then we RELAX the links a few times: for each pair, nudge the two points back
// to the rope's segment length. Do that over and over and a believable rope
// falls out — no springs, no forces, just "keep the links the right length".
//
//   arrows: swing the anchor    Z: flick the tail    X: flip gravity

#define N   24
#define SEG 7.0f

static float px[N], py[N], ox[N], oy[N];
static float anx = 160, any = 28;
static int   gdir = 1;

static void make_rope(void) {
    for (int i = 0; i < N; i++) { px[i] = anx + i * 4; py[i] = any; ox[i] = px[i]; oy[i] = py[i]; }
}

static void step(void) {
    px[0] = anx; py[0] = any; ox[0] = anx; oy[0] = any;   // pin the anchor
    for (int i = 1; i < N; i++) {
        float vx = (px[i] - ox[i]) * 0.99f, vy = (py[i] - oy[i]) * 0.99f;
        ox[i] = px[i]; oy[i] = py[i];
        px[i] += vx; py[i] += vy + 0.5f * gdir;            // inertia + gravity
    }
    for (int it = 0; it < 6; it++) {                       // relax the links
        for (int i = 0; i < N - 1; i++) {
            float dx = px[i + 1] - px[i], dy = py[i + 1] - py[i];
            float d = sqrtf(dx * dx + dy * dy); if (d < 0.001f) d = 0.001f;
            float k = (SEG - d) / d * 0.5f, fx = dx * k, fy = dy * k;
            if (i != 0) { px[i] -= fx; py[i] -= fy; }
            px[i + 1] += fx; py[i + 1] += fy;
        }
        px[0] = anx; py[0] = any;
    }
}

void init(void) { make_rope(); for (int i = 0; i < 14; i++) step(); }   // caught mid-swing

void update(void) {
    if (btn(0, BTN_LEFT))  anx -= 3;
    if (btn(0, BTN_RIGHT)) anx += 3;
    if (btn(0, BTN_UP))    any -= 3;
    if (btn(0, BTN_DOWN))  any += 3;
    anx = clamp(anx, 10, SCREEN_W - 10); any = clamp(any, 10, 150);
    if (btnp(0, BTN_A)) px[N - 1] += rnd(50) - 25;    // flick the tail
    if (btnp(0, BTN_B)) gdir = -gdir;
    step();
}

void draw(void) {
    cls(CLR_BLACK);
    for (int i = 0; i < N - 1; i++) {
        int col = i < 8 ? CLR_BROWN : i < 16 ? CLR_DARK_ORANGE : CLR_ORANGE;
        line((int)px[i], (int)py[i], (int)px[i + 1], (int)py[i + 1], col);
        line((int)px[i], (int)py[i] + 1, (int)px[i + 1], (int)py[i + 1] + 1, col);
    }
    circfill((int)anx, (int)any, 4, CLR_LIGHT_GREY);          // anchor
    circfill((int)px[N - 1], (int)py[N - 1], 6, CLR_RED);     // weight
    print("VERLET ROPE", 4, 4, CLR_LIGHT_GREY);
    print("arrows: swing   Z: flick   X: gravity", 4, SCREEN_H - 9, CLR_DARK_GREY);
}
