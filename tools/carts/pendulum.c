/* de:meta
{
  "title": "double pendulum",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "Two rods swinging from each other — and pure chaos. Solving the real equations of motion in tiny steps, the tip traces a glowing trail. The smallest change in the start sends it on a completely different path, which is what chaos means. Z gives a new random start to watch it diverge, X toggles the trail."
}
de:meta */
#include "studio.h"
#include <math.h>

// DOUBLE PENDULUM — two rods, swinging from each other, and pure chaos.
//
// A single pendulum is predictable. Hang a second one off the end and the
// physics becomes chaotic: the tiniest change in the start sends it on a
// totally different path. We solve the real equations of motion in tiny time
// steps and trace the tip — the glowing trail is the proof that no two runs
// ever repeat.
//
//   Z   new random start (watch it diverge)      X   toggle the trail

#define PI 3.14159265f
#define PX 160
#define PY 64
#define TRAIL 256

static float a1, a2, w1, w2;
static const float l1 = 58, l2 = 58, m1 = 1, m2 = 1, g = 1.0f;
static float tx[TRAIL], ty[TRAIL];
static int   tn, thead;
static bool  show_trail = true;

static void sim_step(void) {
    float dt = 0.0028f;
    for (int s = 0; s < 14; s++) {
        float d = a1 - a2;
        float den = 2 * m1 + m2 - m2 * cosf(2 * a1 - 2 * a2);
        float n1 = -g * (2 * m1 + m2) * sinf(a1) - m2 * g * sinf(a1 - 2 * a2)
                   - 2 * sinf(d) * m2 * (w2 * w2 * l2 + w1 * w1 * l1 * cosf(d));
        float n2 = 2 * sinf(d) * (w1 * w1 * l1 * (m1 + m2) + g * (m1 + m2) * cosf(a1)
                   + w2 * w2 * l2 * m2 * cosf(d));
        w1 += (n1 / (l1 * den)) * dt;
        w2 += (n2 / (l2 * den)) * dt;
        a1 += w1 * dt; a2 += w2 * dt;
        w1 *= 0.99996f; w2 *= 0.99996f;   // a whisper of damping for numerical safety
    }
    float x1 = PX + l1 * sinf(a1), y1 = PY + l1 * cosf(a1);
    float x2 = x1 + l2 * sinf(a2), y2 = y1 + l2 * cosf(a2);
    tx[thead] = x2; ty[thead] = y2;
    thead = (thead + 1) % TRAIL;
    if (tn < TRAIL) tn++;
}

static void reset(void) {
    a1 = PI * 0.7f + rnd_float() * 0.6f;
    a2 = PI * 0.5f + rnd_float() * 1.0f;
    w1 = w2 = 0; tn = thead = 0;
}

void init(void) { reset(); for (int i = 0; i < 200; i++) sim_step(); }

void update(void) {
    if (btnp(0, BTN_A)) reset();
    if (btnp(0, BTN_B)) show_trail = !show_trail;
    sim_step();
}

void draw(void) {
    cls(CLR_BLACK);
    if (show_trail)
        for (int i = 0; i < tn; i++) {
            int idx = (thead - tn + i + TRAIL * 2) % TRAIL;
            int r = tn - 1 - i;   // 0 = newest
            int col = r < 30 ? CLR_WHITE : r < 90 ? CLR_BLUE : r < 170 ? CLR_TRUE_BLUE : CLR_DARKER_BLUE;
            pset((int)tx[idx], (int)ty[idx], col);
        }
    float x1 = PX + l1 * sinf(a1), y1 = PY + l1 * cosf(a1);
    float x2 = x1 + l2 * sinf(a2), y2 = y1 + l2 * cosf(a2);
    line(PX, PY, (int)x1, (int)y1, CLR_LIGHT_GREY);
    line((int)x1, (int)y1, (int)x2, (int)y2, CLR_LIGHT_GREY);
    circfill(PX, PY, 2, CLR_DARK_GREY);
    circfill((int)x1, (int)y1, 5, CLR_RED);
    circfill((int)x2, (int)y2, 5, CLR_YELLOW);

    print("DOUBLE PENDULUM", 4, 4, CLR_LIGHT_GREY);
    print("Z: new random start    X: toggle trail", 4, SCREEN_H - 9, CLR_DARK_GREY);
}
