/* de:meta
{
  "slug": "lsystem",
  "title": "l-system plants",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "l-system"
  ],
  "lineage": "Classic Lindenmayer string-rewrite turtle plants (fern/bush/tree) with a push/pop branch stack; adds animated wind and auto-fit bounding box.",
  "description": "Watch a recursive rule grow into a tree. Start with a tiny string, rewrite every letter over and over (F becomes FF, and so on), then read the result as turtle commands: F draws, +/- turn, and [ ] push/pop a stack to remember and return to each branch fork. That stack IS the recursion. Up/down apply the rule more or fewer times to see detail emerge, left/right bend the angle, Z cycles plants (fern/bush/tree/weed), X toggles the wind.",
  "todo": [
    "Growth feels backward: each iteration adds branches but the plant shrinks vertically — it should grow from small/few to bigger/more.",
    "Changing the angle sometimes changes the plant's size for no clear reason.",
    "Add cute pixel buttons for the various plant algorithms."
  ]
}
de:meta */
#include "studio.h"

// L-SYSTEM PLANTS — how a recursive rule grows into a tree.
//
// Start with a tiny string (the "axiom"). Then, over and over, REWRITE every
// letter using a rule — e.g. F → FF. The string explodes in length, and when
// you read it as drawing instructions a plant appears:
//
//   F  draw forward      [  remember where I am  (push onto a stack)
//   +  turn right        ]  jump back there      (pop the stack)
//   -  turn left         X  a growth bud (draws nothing)
//
// The [ ] stack IS the recursion: every branch remembers its fork and returns
// to it. Press up/down to apply the rule more times and watch detail emerge.
//
//   up / down   grow / ungrow (apply the rule more or fewer times)
//   left / right   bend the branch angle
//   Z   next plant        X   toggle the wind

#define MAXLEN 200000

typedef struct {
    const char *name, *axiom, *pF, *pX;   // pX = NULL means X has no rule
    float angle;
    int   def_iter, max_iter;
} LSystem;

static const LSystem PRE[] = {
    { "FERN",   "X", "FF", "F+[[X]-X]-F[-FX]+X",       25, 5, 6 },
    { "BUSH",   "F", "FF+[+F-F-F]-[-F+F+F]", 0,     22, 3, 4 },
    { "TREE",   "F", "F[+F]F[-F]F",          0,     25, 4, 5 },
    { "WEED",   "F", "F[+F]F[-F][F]",        0,     20, 4, 5 },
};
#define N_PRE 4

static char  A[MAXLEN], B[MAXLEN];
static char *cur;
static int   preset = 0, iters, wind_on = 1;
static float angle;
static int   need_expand = 1, need_measure = 1;
static float scale, ox, oy;
static float bbminx, bbmaxx, bbminy, bbmaxy;

static void load_preset(int n) {
    preset = n;
    angle  = PRE[n].angle;
    iters  = PRE[n].def_iter;
    need_expand = need_measure = 1;
}

void init(void) { load_preset(0); }

// rewrite the string `iters` times
static void expand(void) {
    const LSystem *L = &PRE[preset];
    int n = 0;                          // copy axiom into A
    for (const char *p = L->axiom; *p && n < MAXLEN - 1; p++) A[n++] = *p;
    A[n] = 0;
    char *src = A, *dst = B;
    for (int i = 0; i < iters; i++) {
        n = 0;
        for (char *c = src; *c && n < MAXLEN - 400; c++) {
            const char *r = (*c == 'F') ? L->pF : (*c == 'X') ? L->pX : 0;
            if (r) for (const char *p = r; *p && n < MAXLEN - 1; p++) dst[n++] = *p;
            else dst[n++] = *c;
        }
        dst[n] = 0;
        char *t = src; src = dst; dst = t;   // swap buffers
    }
    cur = src;
}

static float wind(int depth) {
    if (!wind_on) return 0;
    return 5.0f * sin_deg(now() * 80 + depth * 35);   // deeper branches sway out of phase
}

// walk the string. measure==true just grows the bounding box; otherwise draws.
static void interp(bool measure) {
    static struct { float x, y, a; } st[200];
    float x = 0, y = 0, a = -90;   // start facing up
    int sp = 0;
    if (measure) bbminx = bbmaxx = bbminy = bbmaxy = 0;
    for (const char *c = cur; *c; c++) {
        switch (*c) {
            case 'F': {
                float nx = x + cos_deg(a), ny = y + sin_deg(a);
                if (measure) {
                    if (nx < bbminx) bbminx = nx; if (nx > bbmaxx) bbmaxx = nx;
                    if (ny < bbminy) bbminy = ny; if (ny > bbmaxy) bbmaxy = ny;
                } else {
                    int col = sp >= 3 ? CLR_GREEN : sp == 2 ? CLR_MEDIUM_GREEN
                            : sp == 1 ? CLR_DARK_GREEN : CLR_BROWN;
                    line((int)(ox + x * scale), (int)(oy + y * scale),
                         (int)(ox + nx * scale), (int)(oy + ny * scale), col);
                }
                x = nx; y = ny;
            } break;
            case '+': a += angle + wind(sp); break;
            case '-': a -= angle + wind(sp); break;
            case '[': if (sp < 200) { st[sp].x = x; st[sp].y = y; st[sp].a = a; sp++; } break;
            case ']':
                if (!measure) circfill((int)(ox + x * scale), (int)(oy + y * scale), 1,
                                       (sp & 1) ? CLR_PINK : CLR_GREEN);   // a leaf / blossom
                if (sp > 0) { sp--; x = st[sp].x; y = st[sp].y; a = st[sp].a; }
                break;
        }
    }
}

static void recompute(void) {
    if (need_expand) { expand(); need_measure = 1; need_expand = 0; }
    if (need_measure) {
        interp(true);   // fill bbox (windless, so the fit stays stable)
        float w = bbmaxx - bbminx, h = bbmaxy - bbminy;
        if (w < 1) w = 1; if (h < 1) h = 1;
        scale = min(260.0f / w, 150.0f / h);
        ox = 160 - (bbminx + bbmaxx) / 2 * scale;
        oy = 188 - bbmaxy * scale;          // sit the base on the ground
        need_measure = 0;
    }
}

void update(void) {
    const LSystem *L = &PRE[preset];
    if (btnp(0, BTN_UP))    { iters = min(L->max_iter, iters + 1); need_expand = 1; }
    if (btnp(0, BTN_DOWN))  { iters = max(0, iters - 1);          need_expand = 1; }
    if (btnp(0, BTN_LEFT))  { angle -= 1; need_measure = 1; }
    if (btnp(0, BTN_RIGHT)) { angle += 1; need_measure = 1; }
    if (btnp(0, BTN_A))     load_preset((preset + 1) % N_PRE);
    if (btnp(0, BTN_B))     wind_on = !wind_on;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    recompute();
    rectfill(0, 189, SCREEN_W, SCREEN_H - 189, CLR_DARK_BROWN);   // soil
    interp(false);

    const LSystem *L = &PRE[preset];
    print(str("%s   iter %d   angle %d", L->name, iters, (int)angle), 4, 4, CLR_WHITE);
    print(str("F -> %s", L->pF), 4, 14, CLR_LIME_GREEN);
    if (L->pX) print(str("X -> %s", L->pX), 4, 23, CLR_LIME_GREEN);

    hint("up/down grow   L/R angle   Z plant   X wind");
}
