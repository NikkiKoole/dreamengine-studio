/* de:meta
{
  "slug": "bloom",
  "title": "Bloom",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "l-system"
  ],
  "lineage": "Stand-alone L-system turtle garden; the pocket turtle (push/pop stack + heading + line()) is derived from the 16-spirograph idiom, extended with branching state and day-stepped iterative rewriting across three species.",
  "description": "A calm L-system turtle garden at twilight: click the soil to plant a seed (cycles fern/bush/tree), then press space or X to advance a day and watch every plant rewrite its L-system one iteration further — sprouts branch into ferns, bushes, and trees that sway in the wind. Controls: click or Z to plant, space or X to advance the day.",
  "todo": [
    "New plants only appear around the middle of the screen, never at the start or end.",
    "Add cute onscreen A/B buttons."
  ]
}
de:meta */
#include "studio.h"
#include <stddef.h>

// BLOOM — an L-system turtle garden.
//
// Click anywhere on the soil to plant a seed. Each in-game DAY advances every
// plant's L-system one iteration, so seedlings branch and grow a little more
// each day — watch a sprout become a fern, a bush, or a tree.
//
// An L-system starts with a tiny string (the "axiom") and REWRITES every
// letter by a rule (F -> FF...). The string explodes, and read as turtle
// instructions a plant appears:
//
//   F  draw forward      [  push turtle state (remember this fork)
//   +  turn right        ]  pop  turtle state (return to the fork)
//   -  turn left         X  a growth bud (draws nothing, just rewrites)
//
// The [ ] stack IS the branching: every limb remembers its fork and returns.
// There is NO engine turtle — the little turtle below lives in this cart,
// built from dx/dy + line(), exactly like 16-spirograph.c.
//
//   click / Z   plant a seed (cycles species: fern, bush, tree)
//   X / space   advance one day  (every plant grows one iteration)

// ------------------------------------------------------------
// species — each is a different L-system rule set => a different shape
// ------------------------------------------------------------
typedef struct {
    const char *name, *axiom, *pF, *pX;  // pX == NULL => X has no rule
    float angle;       // base turn in degrees
    float step;        // pixels per F at iteration 0 (shrinks as it grows)
    int   max_iter;    // don't grow past this (keeps strings bounded)
    int   trunk, limb, leaf;  // colors: trunk / mid branch / leaf-blossom
} Species;

static const Species SP[] = {
    // FERN — feathery, leans, lots of little fronds
    { "fern", "X", "FF", "F-[[X]+X]+F[+FX]-X",
      24.0f, 7.0f, 4, CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN },
    // BUSH — dense, symmetric, rounded
    { "bush", "F", "FF-[-F+F+F]+[+F-F-F]", NULL,
      22.0f, 6.0f, 3, CLR_DARK_BROWN, CLR_DARK_GREEN, CLR_GREEN },
    // TREE — sparse, tall, clear trunk
    { "tree", "F", "FF[+F]F[-F][F]", NULL,
      26.0f, 7.0f, 4, CLR_BROWN, CLR_DARK_GREEN, CLR_PINK },
};
#define N_SP 3

// ------------------------------------------------------------
// the garden
// ------------------------------------------------------------
#define MAX_PLANTS 16
#define STRBUF     8192   // per-plant working buffers

typedef struct {
    bool  active;
    int   species;
    int   gx;           // ground x (planted column)
    int   iter;         // current L-system iterations (= age in grown days)
    int   planted_day;  // the day this seed went in
    float seed;         // per-plant random phase (wind + leaf jitter)
    int   leafcol;      // this plant's blossom color tint
} Plant;

static Plant plants[MAX_PLANTS];
static int   day = 1;
static int   next_species = 0;   // which species the next click plants
static float day_flash = 0.0f;   // brief glow when a day passes

// blossom palette — plants pick one so the garden isn't monochrome
static const int BLOSSOMS[6] = {
    CLR_PINK, CLR_LIGHT_PEACH, CLR_YELLOW, CLR_LIME_GREEN, CLR_PEACH, CLR_WHITE
};

// ------------------------------------------------------------
// a pocket turtle — position + heading, drawn with dx/dy + line().
// (copied idiom from 16-spirograph.c; the [ ] stack is added here.)
// ------------------------------------------------------------
typedef struct { float x, y, heading; int depth; } TurtleState;

static float       t_x, t_y, t_heading;
static int         t_depth;
static TurtleState t_stack[64];
static int         t_sp;

static void turtle_set(float x, float y, float heading) {
    t_x = x; t_y = y; t_heading = heading; t_depth = 0; t_sp = 0;
}
static void turtle_turn(float d) { t_heading += d; }
static void turtle_push(void) {
    if (t_sp < 64) {
        t_stack[t_sp].x = t_x; t_stack[t_sp].y = t_y;
        t_stack[t_sp].heading = t_heading; t_stack[t_sp].depth = t_depth;
        t_sp++;
    }
    t_depth++;
}
static void turtle_pop(void) {
    if (t_sp > 0) {
        t_sp--;
        t_x = t_stack[t_sp].x; t_y = t_stack[t_sp].y;
        t_heading = t_stack[t_sp].heading; t_depth = t_stack[t_sp].depth;
    }
}
static void turtle_forward(float steps, int color) {
    float nx = t_x + dx(steps, t_heading);
    float ny = t_y + dy(steps, t_heading);
    line((int)t_x, (int)t_y, (int)nx, (int)ny, color);
    t_x = nx; t_y = ny;
}

// ------------------------------------------------------------
// L-system expansion — rewrite `axiom` `iters` times into `out`.
// Bounded: stops rewriting once the buffer is nearly full, so a plant
// just stops getting more detailed instead of overflowing.
// ------------------------------------------------------------
static char bufA[STRBUF], bufB[STRBUF];

static char *expand(const Species *s, int iters) {
    char *src = bufA, *dst = bufB;
    int n = 0;
    for (const char *p = s->axiom; *p && n < STRBUF - 1; p++) src[n++] = *p;
    src[n] = 0;

    for (int i = 0; i < iters; i++) {
        n = 0;
        bool full = false;
        for (char *c = src; *c; c++) {
            const char *r = (*c == 'F') ? s->pF : (*c == 'X') ? s->pX : NULL;
            if (r) {
                for (const char *p = r; *p; p++) {
                    if (n >= STRBUF - 1) { full = true; break; }
                    dst[n++] = *p;
                }
            } else {
                if (n >= STRBUF - 1) { full = true; break; }
                dst[n++] = *c;
            }
            if (full) break;
        }
        dst[n] = 0;
        char *tmp = src; src = dst; dst = tmp;   // swap
        if (full) break;   // no room to grow further; keep what we have
    }
    return src;
}

// gentle per-plant wind — deeper branches sway wider and out of phase
static float wind(int depth, float seed) {
    return (1.2f + depth * 0.7f) * sin_deg(now() * 38.0f + seed * 360.0f + depth * 40.0f);
}

// ------------------------------------------------------------
// render one plant by running its L-system string through the turtle
// ------------------------------------------------------------
static void draw_plant(Plant *pl) {
    const Species *s = &SP[pl->species];

    if (pl->iter <= 0) {
        // a freshly planted seed — a little mound + sprout nub
        ovalfill(pl->gx, 190, 3, 2, CLR_DARK_BROWN);
        line(pl->gx, 190, pl->gx, 187, s->trunk);
        circfill(pl->gx, 186, 1, s->leaf);
        return;
    }

    char *cmd = expand(s, pl->iter);

    // each grown iteration shrinks the step so the plant fits as it branches
    float step = s->step / (1.0f + pl->iter * 0.45f);

    turtle_set((float)pl->gx, 190.0f, -90.0f);   // -90 = facing up

    for (char *c = cmd; *c; c++) {
        switch (*c) {
            case 'F': {
                // color by branch depth: trunk near the ground, greener out
                int col = (t_depth == 0) ? s->trunk
                        : (t_depth == 1) ? s->limb
                        : s->leaf;
                turtle_forward(step, col);
            } break;
            case '+': turtle_turn( s->angle + wind(t_depth, pl->seed)); break;
            case '-': turtle_turn(-s->angle + wind(t_depth, pl->seed)); break;
            case '[': turtle_push(); break;
            case ']':
                // a leaf / blossom at the tip of each finished branch
                if (t_depth >= 2)
                    circfill((int)t_x, (int)t_y, 1, pl->leafcol);
                turtle_pop();
                break;
            default: break;   // X and anything else: no drawing
        }
    }
}

// ------------------------------------------------------------
// planting + days
// ------------------------------------------------------------
static void plant_seed(int x) {
    if (x < 6) x = 6;
    if (x > SCREEN_W - 6) x = SCREEN_W - 6;
    for (int i = 0; i < MAX_PLANTS; i++) {
        if (!plants[i].active) {
            plants[i].active      = true;
            plants[i].species     = next_species;
            plants[i].gx          = x;
            plants[i].iter        = 0;          // starts as a seed
            plants[i].planted_day = day;
            plants[i].seed        = rnd_float();
            plants[i].leafcol     = BLOSSOMS[rnd(6)];
            next_species = (next_species + 1) % N_SP;   // cycle species per plant
            return;
        }
    }
}

static void advance_day(void) {
    day++;
    day_flash = 1.0f;
    for (int i = 0; i < MAX_PLANTS; i++) {
        if (plants[i].active) {
            int cap = SP[plants[i].species].max_iter;
            if (plants[i].iter < cap) plants[i].iter++;
        }
    }
}

void init(void) {
    bpm(70);
    day = 1;
    next_species = 0;
    day_flash = 0.0f;
    for (int i = 0; i < MAX_PLANTS; i++) plants[i].active = false;

    // start with a small mature row so the garden greets you grown
    int xs[3] = { 70, 160, 250 };
    for (int i = 0; i < 3; i++) {
        plant_seed(xs[i]);
        plants[i].iter = SP[plants[i].species].max_iter - 1;
    }
    day = 4;
}

void update(void) {
    if (day_flash > 0.0f) {
        day_flash -= dt() * 1.6f;
        if (day_flash < 0.0f) day_flash = 0.0f;
    }

    // plant on click (or Z) — at the mouse column, on the soil
    if (mouse_pressed(MOUSE_LEFT)) plant_seed(mouse_x());
    if (btnp(0, BTN_A))           plant_seed(SCREEN_W / 2 + rnd_between(-60, 60));

    // advance a day — every plant grows one iteration
    if (keyp(KEY_SPACE) || btnp(0, BTN_B)) advance_day();
}

// soft horizon glow + dithered sky
static void draw_sky(void) {
    cls(CLR_DARKER_PURPLE);
    // banded twilight sky with dither between bands
    rectfill(0, 0,   SCREEN_W, 70,  CLR_DARKER_BLUE);
    fillp(FILL_CHECKER, CLR_DARKER_BLUE);
    rectfill(0, 70,  SCREEN_W, 24,  CLR_DARK_BLUE);
    fillp_reset();
    rectfill(0, 94,  SCREEN_W, 60,  CLR_DARK_BLUE);
    fillp(FILL_CHECKER, CLR_DARK_BLUE);
    rectfill(0, 150, SCREEN_W, 22,  CLR_MAUVE);
    fillp_reset();

    // a low warm sun
    int sy = 132;
    circfill(SCREEN_W - 54, sy, 11, CLR_DARK_ORANGE);
    circfill(SCREEN_W - 54, sy,  8, CLR_ORANGE);
    circfill(SCREEN_W - 54, sy,  4, CLR_LIGHT_YELLOW);

    // a few stars up high
    for (int i = 0; i < 18; i++) {
        int sx2 = (int)(noise(i * 7.3f) * SCREEN_W);
        int sy2 = (int)(noise(i * 7.3f + 30.0f) * 60);
        if (sin_deg(now() * 60.0f + i * 50.0f) > 0.3f)
            pset(sx2, sy2, CLR_LIGHT_GREY);
    }
}

void draw(void) {
    draw_sky();

    // soil
    rectfill(0, 172, SCREEN_W, SCREEN_H - 172, CLR_DARK_BROWN);
    fillp(FILL_DOTS, -1);
    rectfill(0, 172, SCREEN_W, 8, CLR_BROWN);
    fillp_reset();
    rectfill(0, 188, SCREEN_W, 4, CLR_BROWNISH_BLACK);

    // swaying grass tufts along the soil line
    for (int x = 2; x < SCREEN_W; x += 5) {
        int h    = 3 + (int)(noise(x * 0.12f) * 5);
        int sway = (int)(sin_deg(now() * 34.0f + x * 5.0f) * 1.5f);
        line(x, 190, x + sway, 190 - h, CLR_DARK_GREEN);
    }

    // the plants (draw left-to-right so nearer ones overlap naturally)
    for (int i = 0; i < MAX_PLANTS; i++)
        if (plants[i].active) draw_plant(&plants[i]);

    // ---- HUD ----
    rectfill(0, 0, SCREEN_W, 11, CLR_BROWNISH_BLACK);
    int dcol = day_flash > 0.0f ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH;
    print(str("day %d", day), 4, 2, dcol);

    int count = 0;
    for (int i = 0; i < MAX_PLANTS; i++) if (plants[i].active) count++;
    print_right(str("plants %d/%d", count, MAX_PLANTS), SCREEN_W - 4, 2, CLR_MEDIUM_GREY);

    // a swatch showing which species the next seed will be
    print_centered(str("next: %s", SP[next_species].name), SCREEN_W/2, 2, SP[next_species].leaf);

    print("click/Z plant   space/X advance day", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
