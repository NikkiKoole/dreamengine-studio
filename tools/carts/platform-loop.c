/* de:meta
{
  "title": "rail loop (sonic-style)",
  "status": "active",
  "created": "2026-06-03",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "genre": "platformer",
  "description": "Curved rails and edge attributes — the follow-up to 'platform rails'. The rail no longer has to be a straight girder: sample any curve into a list of points and the player's position becomes a float INDEX into that list, so a Bézier hill and a full LOOP are the same code. A Sonic loop 'just works' because the player is glued to the track (no free-fall on a rail) — running it is only advancing the index around the circle, upside-down at the top and all. The orange stretch is a CONVEYOR: an edge attribute that carries you forward on its own (same idea as a one-way ladder — behaviour attached to part of the graph). Roll over the hill, through the loop, across the conveyor, to the flag. Arrows/WASD move, Z to restart."
}
de:meta */
#include "studio.h"

// RAIL LOOP — curved rails + edge attributes, the Sonic idea made literal.
//
// platform-rails used STRAIGHT girders (y = y0 + slope*x). But the rail can be
// ANY curve. The trick: sample the whole rail into a list of points once, then
// the player's position is just a FLOAT INDEX into that list. Running = move the
// index; the player is drawn at points[index]. A line, a Bézier hill, and a full
// LOOP are all the same code — because position is parametric (an index), not a
// height function of x.
//
// Why a loop "just works": in the rail model the player is GLUED to the track —
// no free-fall while on a rail — so running a loop is only "advance the index
// around the circle". No centripetal physics. The ball even goes upside-down at
// the top, because that's just where those points are.
//
// EDGE ATTRIBUTE: the orange stretch is a CONVEYOR — a range of the rail that
// carries you forward on its own. Same idea as a one-way ladder: behaviour
// attached to part of the graph, not the player.
//
// Move: arrows / WASD (you need a little speed to roll the loop).   Restart: Z.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── TUNING — edit + hot-reload (rail rebuilds every frame from these) ────────
#define RUN_SPD   0.95f    // index-steps per frame
#define CONV_SPD  0.85f    // extra steps/frame while on the conveyor
#define SPIN_K    14.0f    // how fast the ball visibly spins
#define PR        5        // player ball radius

#define GROUND_Y  168
#define LOOP_CX   180      // loop centre x
#define LOOP_R    34       // loop radius (top reaches GROUND_Y - 2*R)
#define CONV_X0   232      // conveyor start x
#define CONV_X1   286      // conveyor end x

#define RAIL_MAX 512
typedef struct { float x, y; } Pt;

STATE { float t; float dist; bool win; };

// ── the rail, rebuilt each frame into a points list ──────────────────────────
static Pt  rail[RAIL_MAX];
static int rail_n;
static int conv_lo, conv_hi;   // index range that behaves as a conveyor

static float qbez(float a, float c, float b, float u) {   // quadratic Bézier
    float v = 1 - u;
    return v * v * a + 2 * v * u * c + u * u * b;
}
static void add_pt(float x, float y) {
    if (rail_n >= RAIL_MAX) return;
    if (rail_n > 0) {                                      // keep points ~evenly spaced
        float dx = x - rail[rail_n - 1].x, dy = y - rail[rail_n - 1].y;
        if (dx * dx + dy * dy < 2.2f * 2.2f) return;
    }
    rail[rail_n++] = (Pt){ x, y };
}
static void build_rail(void) {
    rail_n = 0;
    // 1) a Bézier hill — a curved rail that's NOT a loop
    for (int i = 0; i <= 80; i++) { float u = i / 80.0f; add_pt(qbez(18, 78, 138, u), qbez(170, 96, 170, u)); }
    // 2) flat run-up into the loop
    for (int x = 138; x <= LOOP_CX; x += 2) add_pt(x, GROUND_Y);
    // 3) the LOOP — a full circle, bottom sitting on the ground at (LOOP_CX, GROUND_Y)
    for (int i = 0; i <= 80; i++) {
        float deg = 360.0f * i / 80.0f;
        add_pt(LOOP_CX + LOOP_R * sin_deg(deg), (GROUND_Y - LOOP_R) + LOOP_R * cos_deg(deg));
    }
    // 4) flat out of the loop, up to the conveyor
    for (int x = LOOP_CX; x <= CONV_X0; x += 2) add_pt(x, GROUND_Y);
    // 5) the CONVEYOR stretch (an edge attribute)
    conv_lo = rail_n;
    for (int x = CONV_X0; x <= CONV_X1; x += 2) add_pt(x, GROUND_Y);
    conv_hi = rail_n;
    // 6) final stretch to the flag
    for (int x = CONV_X1; x <= 306; x += 2) add_pt(x, GROUND_Y);
}

// tangent-based outward normal so the ball sits ON the track (and inside the loop)
static void normal_at(int i, float *nx, float *ny) {
    int a = i - 1 < 0 ? 0 : i - 1, b = i + 1 >= rail_n ? rail_n - 1 : i + 1;
    float tx = rail[b].x - rail[a].x, ty = rail[b].y - rail[a].y;
    float len = fsqrt(tx * tx + ty * ty); if (len < 0.001f) len = 1;
    *nx = ty / len; *ny = -tx / len;       // rotate tangent 90° → points to track interior
}

void init(void) { S->t = 0; S->dist = 0; S->win = false; }

void update(void) {
    build_rail();
#ifdef DE_TRACE
    watch("p", "t=%.0f/%d x=%.0f y=%.0f conv=%d win=%d", S->t, rail_n,
          rail[(int)S->t].x, rail[(int)S->t].y,
          ((int)S->t >= conv_lo && (int)S->t < conv_hi), S->win);
#endif
    if (S->win) { if (btnp(0, BTN_A)) { S->t = 0; S->dist = 0; S->win = false; } return; }

    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    float step = dir * RUN_SPD;
    bool on_conv = (int)S->t >= conv_lo && (int)S->t < conv_hi;
    if (on_conv) step += CONV_SPD;                         // the conveyor carries you

    S->t = clamp(S->t + step, 0, rail_n - 1);
    float moved = step < 0 ? -step : step;
    S->dist += moved;

    if ((int)S->t >= rail_n - 2) { S->win = true; strum(72, CHORD_MAJ, INSTR_TRI, 4, 40); }
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // the rail: a 2px beam, orange where it's a conveyor
    for (int i = 1; i < rail_n; i++) {
        bool conv = (i >= conv_lo && i < conv_hi);
        int col = conv ? CLR_ORANGE : CLR_RED;
        line((int)rail[i - 1].x, (int)rail[i - 1].y, (int)rail[i].x, (int)rail[i].y, col);
        line((int)rail[i - 1].x, (int)rail[i - 1].y + 1, (int)rail[i].x, (int)rail[i].y + 1,
             conv ? CLR_DARK_ORANGE : CLR_DARK_RED);
    }
    // conveyor chevrons (pointing the way it carries you)
    for (int i = conv_lo; i < conv_hi; i += 5) {
        int x = (int)rail[i].x, y = (int)rail[i].y - 4;
        line(x - 2, y - 2, x + 1, y, CLR_LIGHT_YELLOW);
        line(x + 1, y, x - 2, y + 2, CLR_LIGHT_YELLOW);
    }

    // goal flag at the end
    int fx = (int)rail[rail_n - 1].x, fy = (int)rail[rail_n - 1].y;
    line(fx, fy - 16, fx, fy, CLR_LIGHT_GREY);
    rectfill(fx + 1, fy - 16, 9, 6, blink(20) ? CLR_GREEN : CLR_YELLOW);

    // the player: a spinning ball that sits on the track via the surface normal
    int i = (int)S->t;
    float nx, ny; normal_at(i, &nx, &ny);
    int cx = (int)(rail[i].x + nx * PR), cy = (int)(rail[i].y + ny * PR);
    circfill(cx, cy, PR, CLR_BLUE);
    circ(cx, cy, PR, CLR_DARK_BLUE);
    float a = S->dist * SPIN_K;                            // spin with distance travelled
    line(cx - (int)(PR * cos_deg(a)), cy - (int)(PR * sin_deg(a)),
         cx + (int)(PR * cos_deg(a)), cy + (int)(PR * sin_deg(a)), CLR_DARK_BLUE);
    pset(cx + (int)(2 * nx), cy + (int)(2 * ny), CLR_WHITE);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("roll the hill + loop to the flag", 4, 2, CLR_WHITE);
    if (S->win) {
        print_centered("LOOP CLEARED!", SCREEN_W / 2, SCREEN_H / 2 - 6, CLR_YELLOW);
        print_centered("press Z to play again", SCREEN_W / 2, SCREEN_H / 2 + 4, CLR_WHITE);
    }
}
