/* de:meta
{
  "title": "Physics playground",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "verlet-integration",
    "soft-body"
  ],
  "lineage": "Original didactic tech-demo; no sibling cart. Novel: a self-contained, readable Verlet world (points + sticks + relaxation) that demonstrates ropes, cloth, soft-body blobs, and stacked pseudo-rigid boxes from two primitives, with live substep control.",
  "description": "A tiny verlet physics world you can read end to end — the whole engine is three ideas: a point remembers where it was last frame (so velocity is just now-minus-last), a stick keeps two points a fixed distance apart, and each frame you move every point then relax the sticks a few times. From those two primitives the cart builds a whole showcase: a dangling chain, a draping cloth sheet, a wobbly soft-body blob (a ring of points around a hub), pseudo-solid boxes and a triangle that stack and tumble, and bouncy balls — all colliding as circles and against each other's edges. Left/Right change the substep count live (more substeps = fast/heavy things stop tunnelling through thin surfaces). Drag any point to grab and throw it; Z drops a ball, X drops a box, R resets.",
  "todo": [
    "Brainstorm a drawable-palette + touch model: an onscreen toggle to set the active tool (add / move), so touch/mouse always just does that — getting away from always needing a Z/X + A/B pair."
  ]
}
de:meta */
#include "studio.h"

// PHYSICS PLAYGROUND — a tiny verlet world you can read end to end.
//
// The whole engine is three ideas:
//   1. a POINT remembers where it was last frame; velocity is just (now - last).
//   2. a STICK is two points that want to stay a fixed distance apart.
//   3. each frame: move every point, then "relax" the sticks a few times.
// That's it. Ropes, cloth, ragdolls, tumbling boxes and bouncy balls all fall
// out of those three lines. No rotation, no mass matrices, no contact manifolds.
//
// Mouse: grab the nearest point and drag/throw it.  Z: drop a ball.
// X: drop a stick-built box.  R: reset.

#define MAXP   128
#define MAXS   256
#define ITERS    6            // constraint relaxation passes per substep (more = stiffer)
#define GRAV   0.40f          // downward pull, pixels/frame^2 (split across the substeps)
#define DAMP   0.99f          // air drag (velocity kept each frame)
#define REST   0.50f          // wall/floor bounciness (0 = dead, 1 = perfect)
#define FLOOR  (SCREEN_H - 16)

// A point: position (x,y), where it was last frame (px,py), its radius, a colour,
// and w = INVERSE MASS. w = 0 means "immovable" (a pinned anchor, or a grabbed point).
typedef struct { float x, y, px, py, r, w; int col; } Pt;
typedef struct { int a, b; float len; } Stk;   // a stick between points a and b

static Pt  pts[MAXP];  static int npt;
static Stk stks[MAXS]; static int nstk;

static int   grab = -1;     // index of the point held by the mouse (-1 = none)
static float gw;            // its real inverse-mass, saved while we pin it to the cursor
static float pmx, pmy;      // previous-frame mouse, so a release throws at mouse speed
static int   substeps = 3;  // time-slices per frame (←/→ to change). more = less tunneling

static float len2(float x, float y) { return fsqrt(x*x + y*y); }

static int add_pt(float x, float y, float r, bool pinned, int col) {
    if (npt >= MAXP) return npt - 1;
    pts[npt] = (Pt){ x, y, x, y, r, pinned ? 0.0f : 1.0f, col };
    return npt++;
}
static void add_stick(int a, int b) {
    if (nstk >= MAXS) return;
    stks[nstk++] = (Stk){ a, b, len2(pts[b].x - pts[a].x, pts[b].y - pts[a].y) };
}

// --- the readable core ----------------------------------------------------

// move one point forward: velocity = (current - previous), carried over + gravity
static void integrate(Pt *p) {
    if (p->w == 0) return;                 // pinned / held: someone else controls it
    float vx = (p->x - p->px) * DAMP;
    float vy = (p->y - p->py) * DAMP;
    p->px = p->x;  p->py = p->y;
    p->x += vx;    p->y += vy + GRAV / substeps;   // gravity split across substeps → same net fall
}

// pull a stick's two ends back toward their rest length, split by inverse mass
static void relax_stick(Stk *s) {
    Pt *a = &pts[s->a], *b = &pts[s->b];
    float dx = b->x - a->x, dy = b->y - a->y;
    float d  = len2(dx, dy);
    if (d < 0.0001f) return;
    float ws = a->w + b->w;
    if (ws == 0) return;                   // both immovable
    float diff = (d - s->len) / d;         // >0 too long, <0 too short
    a->x += dx * diff * (a->w / ws);  a->y += dy * diff * (a->w / ws);
    b->x -= dx * diff * (b->w / ws);  b->y -= dy * diff * (b->w / ws);
}

// two points are circles — if they overlap, shove them apart (again split by mass)
static void collide(Pt *a, Pt *b) {
    float dx = b->x - a->x, dy = b->y - a->y;
    float d  = len2(dx, dy), min = a->r + b->r;
    if (d >= min || d < 0.0001f) return;
    float ws = a->w + b->w;
    if (ws == 0) return;
    float diff = (min - d) / d;
    a->x -= dx * diff * (a->w / ws);  a->y -= dy * diff * (a->w / ws);
    b->x += dx * diff * (b->w / ws);  b->y += dy * diff * (b->w / ws);
}

// a point (circle) vs a stick (a thin line). Find the closest spot on the segment;
// if the circle overlaps it, push the point out — and push the segment's two ends the
// other way, split by HOW FAR along the segment the contact landed (t). This is what
// makes faces solid: balls bounce off box edges, and boxes rest on each other's sides.
static void collide_seg(Pt *p, Stk *s) {
    Pt *a = &pts[s->a], *b = &pts[s->b];
    if (p == a || p == b) return;                  // don't collide a point with its own stick
    float abx = b->x - a->x, aby = b->y - a->y;
    float ab2 = abx * abx + aby * aby;
    if (ab2 < 0.0001f) return;
    float t = ((p->x - a->x) * abx + (p->y - a->y) * aby) / ab2;
    t = clamp(t, 0.0f, 1.0f);                       // clamp to the segment's ends
    float dx = p->x - (a->x + t * abx);             // from closest point on segment to p
    float dy = p->y - (a->y + t * aby);
    float d  = len2(dx, dy);
    if (d >= p->r || d < 0.0001f) return;
    float wseg = a->w * (1 - t) + b->w * t;         // effective inverse mass at the contact
    float ws   = p->w + wseg;
    if (ws == 0) return;
    float diff = (p->r - d) / d;                    // separation, as a fraction of (dx,dy)
    float pf = p->w / ws, sf = wseg / ws;
    p->x += dx * diff * pf;        p->y += dy * diff * pf;
    a->x -= dx * diff * sf * (1 - t); a->y -= dy * diff * sf * (1 - t);
    b->x -= dx * diff * sf * t;       b->y -= dy * diff * sf * t;
}

// keep a point inside the box, bouncing (and rubbing off speed on the floor)
static void clamp_bounds(Pt *p) {
    if (p->w == 0) return;
    float r = p->r, v;
    if (p->x < r)            { v = p->x - p->px; p->x = r;            p->px = p->x + v * REST; }
    if (p->x > SCREEN_W - r) { v = p->x - p->px; p->x = SCREEN_W - r; p->px = p->x + v * REST; }
    if (p->y < r)            { v = p->y - p->py; p->y = r;            p->py = p->y + v * REST; }
    if (p->y > FLOOR - r)    { v = p->y - p->py; p->y = FLOOR - r;    p->py = p->y + v * REST;
                               float vx = p->x - p->px; p->px = p->x - vx * 0.90f; }  // ground friction
}

static void physics_step(void) {
    // chop the frame into `substeps` smaller moves. Each substep integrates a little and
    // then fully solves — so a fast point can't leap through a thin collider in one jump.
    for (int sub = 0; sub < substeps; sub++) {
        for (int i = 0; i < npt; i++) integrate(&pts[i]);
        for (int it = 0; it < ITERS; it++) {
            for (int s = 0; s < nstk; s++) relax_stick(&stks[s]);
            for (int i = 0; i < npt; i++)
                for (int j = i + 1; j < npt; j++) collide(&pts[i], &pts[j]);
            for (int i = 0; i < npt; i++)
                for (int s = 0; s < nstk; s++) collide_seg(&pts[i], &stks[s]);
            for (int i = 0; i < npt; i++) clamp_bounds(&pts[i]);
        }
    }
}

// --- builders: little assemblies made from the same points + sticks --------

static void build_rope(float x, float y, int segs, int col) {
    int prev = add_pt(x, y, 2, true, CLR_LIGHT_GREY);   // pinned anchor at the top
    for (int i = 1; i <= segs; i++) {
        int p = add_pt(x, y + i * 8, 2, false, col);
        add_stick(prev, p);
        prev = p;
    }
}
// 4 corners + 4 edges + 2 diagonals. The diagonals are the trick: without them the
// square folds flat; with them it holds its shape and tumbles like a solid.
// Returns the base point index (corners are base..base+3, clockwise from top-left).
static int build_box(float cx, float cy, float s, int col) {
    int a = add_pt(cx - s, cy - s, 3, false, col);
    int b = add_pt(cx + s, cy - s, 3, false, col);
    int c = add_pt(cx + s, cy + s, 3, false, col);
    int d = add_pt(cx - s, cy + s, 3, false, col);
    add_stick(a, b); add_stick(b, c); add_stick(c, d); add_stick(d, a);
    add_stick(a, c); add_stick(b, d);
    return a;
}
static void build_triangle(float cx, float cy, float s, int col) {
    int a = add_pt(cx,     cy - s, 3, false, col);
    int b = add_pt(cx + s, cy + s, 3, false, col);
    int c = add_pt(cx - s, cy + s, 3, false, col);
    add_stick(a, b); add_stick(b, c); add_stick(c, a);
}

// CLOTH — a grid of points, each linked to its right + down neighbour. Pin the two top
// corners and it drapes like a hammock. Same point+stick code, arranged in a lattice.
static void build_cloth(float x, float y, int cols, int rows, float sp, int col) {
    int base = npt;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            bool pin = (r == 0 && (c == 0 || c == cols - 1));   // nail just the top corners
            add_pt(x + c * sp, y + r * sp, 1.5f, pin, col);
        }
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            int id = base + r * cols + c;
            if (c < cols - 1) add_stick(id, id + 1);            // link right
            if (r < rows - 1) add_stick(id, id + cols);         // link down
        }
}

// SOFT BODY — a ring of points around a hub. Spokes (hub→rim) keep it round; the rim
// links keep it together. It squashes on impact and springs back: a jelly blob, free.
static void build_blob(float cx, float cy, float rad, int n, int col) {
    int hub = add_pt(cx, cy, 2, false, col);
    int first = -1, prev = -1;
    for (int i = 0; i < n; i++) {
        float a = (float)i / (float)n * 360.0f;
        int p = add_pt(cx + cos_deg(a) * rad, cy + sin_deg(a) * rad, 3, false, col);
        add_stick(hub, p);                       // spoke
        if (prev >= 0) add_stick(prev, p); else first = p;   // rim
        prev = p;
    }
    add_stick(prev, first);                      // close the ring
}
static void spawn_ball(float x, float y) {
    int cols[] = { CLR_ORANGE, CLR_YELLOW, CLR_RED, CLR_PINK };
    add_pt(x, y, 5.0f + rnd(5), false, cols[rnd(4)]);
}

static int dbg_toplow, dbg_bottop;   // corner indices the trace watches (set below)

static void reset_scene(void) {
    npt = 0; nstk = 0; grab = -1;
    build_rope(24, 12, 8, CLR_BLUE);                 // a chain
    build_cloth(52, 12, 6, 5, 11, CLR_LIGHT_GREY);   // a draping sheet
    build_blob(150, 28, 12, 10, CLR_PINK);           // a jelly soft-body
    int bot = build_box(215, 100, 13, CLR_GREEN);    // pseudo-solid box, resting low...
    int top = build_box(215, 45, 13, CLR_LIME_GREEN);// ...and one dropping onto it
    dbg_bottop = bot;        // bottom box, top-left corner
    dbg_toplow = top + 2;    // top box, bottom-right corner
    build_triangle(272, 45, 13, CLR_MAUVE);          // pseudo-solid triangle
    for (int i = 0; i < 4; i++) spawn_ball(120 + i * 55, 16 + rnd(14));
}

void init(void) { reset_scene(); }

void update(void) {
    float mx = (float)mouse_x(), my = (float)mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {                 // grab the nearest point
        float best = 14.0f;
        for (int i = 0; i < npt; i++) {
            float d = len2(mx - pts[i].x, my - pts[i].y);
            if (d < best) { best = d; grab = i; }
        }
        if (grab >= 0) { gw = pts[grab].w; pts[grab].w = 0; }   // make it immovable while dragged
    }
    if (grab >= 0 && mouse_down(MOUSE_LEFT)) {        // pin it to the cursor; remember mouse speed
        pts[grab].x = mx;  pts[grab].y = my;
        pts[grab].px = pmx; pts[grab].py = pmy;
    }
    if (mouse_released(MOUSE_LEFT) && grab >= 0) {    // let go → restore its mass, it flies off
        pts[grab].w = gw; grab = -1;
    }

    if (keyp('Z')) spawn_ball(mx, my);
    if (keyp('X')) build_box(mx, my, 12, CLR_GREEN);
    if (keyp('R')) reset_scene();

    if (keyp(KEY_LEFT)  && substeps > 1) substeps--;   // fewer substeps → watch the box tunnel in
    if (keyp(KEY_RIGHT) && substeps < 8) substeps++;   // more substeps → it lands clean

    physics_step();

#ifdef DE_TRACE
    // the falling (top) box's lower corners vs the resting (bottom) box's upper corners.
    // overlap > 0 means the top box has sunk INTO the bottom one.
    watch("substeps", "%d", substeps);
    watch("topbox_low_y", "%.1f", pts[dbg_toplow].y);
    watch("botbox_top_y", "%.1f", pts[dbg_bottop].y);
    watch("overlap",      "%.1f", pts[dbg_toplow].y - pts[dbg_bottop].y);
#endif

    pmx = mx; pmy = my;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    rectfill(0, FLOOR, SCREEN_W, SCREEN_H - FLOOR, CLR_DARK_GREEN);
    line(0, FLOOR, SCREEN_W - 1, FLOOR, CLR_GREEN);

    for (int s = 0; s < nstk; s++)                   // sticks as lines (box diagonals show too)
        line((int)pts[stks[s].a].x, (int)pts[stks[s].a].y,
             (int)pts[stks[s].b].x, (int)pts[stks[s].b].y, pts[stks[s].a].col);

    for (int i = 0; i < npt; i++) {                  // points as circles; anchors get a marker
        circfill((int)pts[i].x, (int)pts[i].y, (int)pts[i].r, pts[i].col);
        if (pts[i].w == 0 && i != grab)
            rect((int)pts[i].x - 3, (int)pts[i].y - 3, 6, 6, CLR_WHITE);
    }

    font(FONT_SMALL);   // 4x6 font — keeps everything well inside 320px
    print(str("points %d  sticks %d", npt, nstk), 4, 4, CLR_LIGHT_GREY);

    // substeps readout — the star of this build
    print_right(str("SUBSTEPS  %d", substeps), SCREEN_W - 4, 4, CLR_YELLOW);
    print_right("left/right to change", SCREEN_W - 4, 11, CLR_DARK_GREY);

    print("drag: grab/throw   Z: ball   X: box   R: reset", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
