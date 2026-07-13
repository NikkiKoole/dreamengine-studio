/* de:meta
{
  "slug": "verlet",
  "title": "Verlet toolkit demo",
  "status": "active",
  "created": "2026-07-13",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "verlet-integration",
    "soft-body"
  ],
  "lineage": "Twin of the `physics` playground, but built on the shared runtime/physics.h toolkit instead of an inline engine — the proof that the extracted primitives (phys_integrate/phys_link/phys_collide/phys_bounds/phys_grab) reproduce the hand-rolled verlet loop. Where `physics` teaches the engine inline, this one shows the library in use.",
  "description": "A rope, a pile of bouncy balls, and a wobbly blob — all driven by runtime/physics.h (the shared verlet toolkit), not by inline math. Drag any point to grab and throw it; Z drops a ball; R resets. Same feel as the Physics playground, a fraction of the code, because the point/stick/collision verbs live in the header now."
}
de:meta */
#include "studio.h"
#include "physics.h"

#define MAXP 128
#define MAXS 256
#define GRAV   0.40f
#define DAMP   0.99f
#define REST   0.50f
#define ITERS  6
#define FLOOR  (SCREEN_H - 16)

typedef struct { int a, b; float len; } Stk;   /* a stick between points a and b */
static PhysPt pts[MAXP];  static int pcol[MAXP];  static int npt;
static Stk stks[MAXS]; static int nstk;

static int   substeps = 3;
static int   grab = -1;
static float gw, pmx, pmy;

static int add_pt(float x, float y, float r, bool pinned, int col) {
    if (npt >= MAXP) return npt - 1;
    phys_pt(&pts[npt], x, y, pinned ? 0.0f : 1.0f, r);
    pcol[npt] = col;
    return npt++;
}
static void add_stick(int a, int b) {
    if (nstk >= MAXS) return;
    float dx = pts[b].x - pts[a].x, dy = pts[b].y - pts[a].y;
    stks[nstk++] = (Stk){ a, b, fsqrt(dx*dx + dy*dy) };
}

static void build_rope(float x, float y, int segs, int col) {
    int prev = add_pt(x, y, 2, true, CLR_LIGHT_GREY);
    for (int i = 1; i <= segs; i++) { int p = add_pt(x, y + i*8, 2, false, col); add_stick(prev, p); prev = p; }
}
static void build_blob(float cx, float cy, float rad, int n, int col) {
    int hub = add_pt(cx, cy, 2, false, col);
    int first = -1, prev = -1;
    for (int i = 0; i < n; i++) {
        float a = (float)i / (float)n * 360.0f;
        int p = add_pt(cx + cos_deg(a)*rad, cy + sin_deg(a)*rad, 3, false, col);
        add_stick(hub, p);
        if (prev >= 0) add_stick(prev, p); else first = p;
        prev = p;
    }
    add_stick(prev, first);
}
static void spawn_ball(float x, float y) {
    int cols[] = { CLR_ORANGE, CLR_YELLOW, CLR_RED, CLR_PINK };
    add_pt(x, y, 5.0f + rnd(5), false, cols[rnd(4)]);
}

static void reset_scene(void) {
    npt = 0; nstk = 0; grab = -1;
    build_rope(40, 12, 10, CLR_BLUE);
    build_blob(150, 40, 14, 10, CLR_PINK);
    for (int i = 0; i < 5; i++) spawn_ball(90 + i*40, 16 + rnd(14));
}

void init(void) { reset_scene(); }

void update(void) {
    float mx = (float)mouse_x(), my = (float)mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {
        float best = 14.0f;
        for (int i = 0; i < npt; i++) {
            float d = fsqrt((mx-pts[i].x)*(mx-pts[i].x) + (my-pts[i].y)*(my-pts[i].y));
            if (d < best) { best = d; grab = i; }
        }
        if (grab >= 0) { gw = pts[grab].w; pts[grab].w = 0; }
    }
    if (grab >= 0 && mouse_down(MOUSE_LEFT)) phys_grab(&pts[grab], mx, my, pmx, pmy);
    if (mouse_released(MOUSE_LEFT) && grab >= 0) { pts[grab].w = gw; grab = -1; }

    if (keyp('Z')) spawn_ball(mx, my);
    if (keyp('R')) reset_scene();

    for (int sub = 0; sub < substeps; sub++) {
        for (int i = 0; i < npt; i++) phys_integrate(&pts[i], 0.0f, GRAV / substeps, DAMP);
        for (int it = 0; it < ITERS; it++) {
            for (int s = 0; s < nstk; s++) phys_link(&pts[stks[s].a], &pts[stks[s].b], stks[s].len);
            for (int i = 0; i < npt; i++)
                for (int j = i + 1; j < npt; j++) phys_collide(&pts[i], &pts[j]);
            for (int i = 0; i < npt; i++) phys_bounds(&pts[i], 0, 0, SCREEN_W, FLOOR, REST, 0.90f);
        }
    }
    pmx = mx; pmy = my;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    rectfill(0, FLOOR, SCREEN_W, SCREEN_H - FLOOR, CLR_DARK_GREEN);
    line(0, FLOOR, SCREEN_W - 1, FLOOR, CLR_GREEN);

    for (int s = 0; s < nstk; s++)
        line((int)pts[stks[s].a].x, (int)pts[stks[s].a].y,
             (int)pts[stks[s].b].x, (int)pts[stks[s].b].y, pcol[stks[s].a]);
    for (int i = 0; i < npt; i++) {
        circfill((int)pts[i].x, (int)pts[i].y, (int)pts[i].r, pcol[i]);
        if (pts[i].w == 0 && i != grab) rect((int)pts[i].x - 3, (int)pts[i].y - 3, 6, 6, CLR_WHITE);
    }

    font(FONT_SMALL);
    print("runtime/physics.h  —  drag: grab/throw   Z: ball   R: reset", 4, 4, CLR_LIGHT_GREY);
    print(str("points %d  sticks %d", npt, nstk), 4, SCREEN_H - 9, CLR_DARK_GREY);
}
