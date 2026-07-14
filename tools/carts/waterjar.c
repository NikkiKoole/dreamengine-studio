/* de:meta
{
  "slug": "waterjar",
  "title": "Water Jar",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "verlet-integration",
    "particle-system",
    "fluid-sim"
  ],
  "lineage": "The PBD/SPH fluid the physics-notes 'Fluids — a third world' section calls for: a code-first Position-Based Fluid (Macklin & Müller / Ten Minute Physics) on runtime/physics.h, deliberately NOT LiquidFun (C++, dead). Same points-and-constraints shape as jelly.c — swaps the blob's edge-links for a per-particle DENSITY constraint (incompressibility) + XSPH viscosity, with a uniform-grid spatial hash so a few hundred particles stay 60fps. No new engine code.",
  "description": {
    "summary": "A jar of water you can tip, slosh, and stir. Hundreds of particles hold together as an incompressible fluid — tilt the jar and it pours to the low corner, settling to a flat surface; drag to stir a whirlpool.",
    "detail": "Position-Based Fluids: each frame the particles fall under gravity, then a density constraint solved over grid-hashed neighbours pushes overpacked water apart until it reads as incompressible (a flat resting surface, not a heap of balls). A dash of XSPH viscosity smooths the slosh. Colour tracks speed — deep still water is dark, fast foam goes white — so the motion reads at a glance.",
    "controls": "Left/Right (or A/D) tilt the jar · drag the mouse to stir · SPACE pours more water · R resets"
  }
}
de:meta */
#include "studio.h"
#include "physics.h"   // the fluid rides the shared verlet toolkit (Layer 0) — PhysPt + integrate/bounds

// WATER JAR — a 2D Position-Based Fluid (PBD/SPH) on physics.h.
//   integrate : particles fall under gravity (verlet, via phys_integrate)
//   neighbours: a uniform-grid spatial hash (cell ≈ smoothing radius H) → O(n), not O(n²)
//   density   : the incompressibility solve — per particle, measure how packed it is vs a
//               rest density and push overpacked water apart. This is what turns "colliding
//               balls" into cohesive water that settles to a FLAT surface.
//   viscosity : a touch of XSPH velocity-smoothing so the slosh is smooth, not spray.
//   walls     : phys_bounds keeps every particle inside the (axis-aligned, in sim space) jar.
// The jar tilts by rotating GRAVITY in the jar's frame; the whole scene is drawn rotated back
// so it reads as tipping a real jar. Nothing here is engine code — just the physics.h verbs.

#define MAXP     1200
#define MAXN     48            // neighbours cached per particle per frame (grid-hashed)
#define H        7.0f          // smoothing radius: particles interact within this
#define H2       (H * H)
#define HCELL    7             // spatial-hash cell size (int, ≈ H) → 3×3 cells cover radius H
#define PR       2.0f          // particle collision radius (for the walls)
#define GRAV     0.14f         // gravity per frame (sim units)
#define DAMP     0.99f         // velocity retention — water keeps its momentum
#define ITERS    4             // density-solve passes per frame (more = stiffer/less squishy)
#define VISC     0.10f         // XSPH viscosity strength (0 = spray, high = honey)
#define VMAX     4.0f          // clamp per-frame speed so a bad frame can't explode the sim
#define EPS      0.0008f       // constraint relaxation (stabilises lambda where gradients vanish)
#define SCORR_K  0.00015f      // artificial pressure: fights particle clumping (surface tension-ish)
#define SCORR_N  4

// jar interior, in sim (untilted) space
#define JX0 44
#define JY0 18
#define JX1 276
#define JY1 190

#define GW ((SCREEN_W / HCELL) + 2)
#define GH ((SCREEN_H / HCELL) + 2)
#define NCELLS (GW * GH)

static PhysPt P[MAXP];
static int    N = 0;

static float lam[MAXP], dpx[MAXP], dpy[MAXP], vlx[MAXP], vly[MAXP];
static int   nbr[MAXP * MAXN], nbrN[MAXP];
static int   cellHead[NCELLS], cnext[MAXP];

static float KP, KS, WDQ;      // kernel normalisers + poly6(dq) for the s_corr denominator
static float rho0 = 0.0f;      // rest density — calibrated once from the initial packing

static float tilt = 0.0f;      // jar tilt in degrees (rotates gravity)
static float pmx, pmy;         // previous mouse (sim space) for stir velocity

static float vlen(float x, float y) { return fsqrt(x * x + y * y); }

// --- SPH kernels (2D) ------------------------------------------------------
static float poly6(float r2) {              // density kernel: smooth, peaks at r=0
    if (r2 >= H2) return 0.0f;
    float d = H2 - r2;
    return KP * d * d * d;
}
// spiky gradient magnitude coefficient: grad W = gcoef(r) * (dx,dy)   (dx,dy = pi - pj)
static float spiky_gcoef(float r) {         // negative → points from j toward i
    if (r >= H || r < 0.0001f) return 0.0f;
    float d = H - r;
    return -KS * d * d / r;
}

// --- spatial hash ----------------------------------------------------------
static int cell_of(float x, float y) {
    int gx = (int)(x / HCELL), gy = (int)(y / HCELL);
    gx = clamp(gx, 0, GW - 1); gy = clamp(gy, 0, GH - 1);
    return gy * GW + gx;
}
static void build_grid(void) {
    for (int c = 0; c < NCELLS; c++) cellHead[c] = -1;
    for (int i = 0; i < N; i++) { int c = cell_of(P[i].x, P[i].y); cnext[i] = cellHead[c]; cellHead[c] = i; }
}
static void build_neighbours(void) {
    for (int i = 0; i < N; i++) {
        int gx = clamp((int)(P[i].x / HCELL), 0, GW - 1);
        int gy = clamp((int)(P[i].y / HCELL), 0, GH - 1);
        int cnt = 0;
        for (int oy = -1; oy <= 1 && cnt < MAXN; oy++)
            for (int ox = -1; ox <= 1 && cnt < MAXN; ox++) {
                int cx = gx + ox, cy = gy + oy;
                if (cx < 0 || cy < 0 || cx >= GW || cy >= GH) continue;
                for (int j = cellHead[cy * GW + cx]; j >= 0 && cnt < MAXN; j = cnext[j]) {
                    if (j == i) continue;
                    float dx = P[i].x - P[j].x, dy = P[i].y - P[j].y;
                    if (dx * dx + dy * dy < H2) nbr[i * MAXN + cnt++] = j;
                }
            }
        nbrN[i] = cnt;
    }
}

// density at particle i (self term included)
static float density_at(int i) {
    float rho = poly6(0.0f);
    for (int k = 0; k < nbrN[i]; k++) {
        int j = nbr[i * MAXN + k];
        float dx = P[i].x - P[j].x, dy = P[i].y - P[j].y;
        rho += poly6(dx * dx + dy * dy);
    }
    return rho;
}

// --- particle setup --------------------------------------------------------
static void add_pt(float x, float y, float vx, float vy) {
    if (N >= MAXP) return;
    phys_pt(&P[N], x, y, 1.0f, PR);
    P[N].px = x - vx; P[N].py = y - vy;     // seed velocity via prev-position
    N++;
}

static void fill_jar(void) {
    N = 0;
    float s = 0.62f * H;                     // initial spacing — a bit under H → well-packed
    // fill the lower ~65% of the jar so it reads as a real pool with headroom to slosh
    float top = JY1 - (JY1 - JY0) * 0.65f;
    for (float y = JY1 - PR - 1; y > top && N < 820; y -= s)
        for (float x = JX0 + PR + 1; x < JX1 - PR - 1 && N < 820; x += s)
            add_pt(x, y, 0, 0);
}

// calibrate the rest density from the initial (settled-ish) packing: use the MAX density found,
// which is an interior particle — a boundary-average would read low and make the fluid puff up.
static void calibrate_rho0(void) {
    build_grid(); build_neighbours();
    float mx = 0.0f;
    for (int i = 0; i < N; i++) { float r = density_at(i); if (r > mx) mx = r; }
    rho0 = mx > 0.0001f ? mx : poly6(0.0f);
}

void init(void) {
    float h5 = H * H * H * H * H, h8 = h5 * H * H * H;
    KP  = 4.0f  / (3.14159265f * h8);
    KS  = 30.0f / (3.14159265f * h5);
    WDQ = poly6((0.2f * H) * (0.2f * H));
    tilt = 0.0f;
    fill_jar();
    calibrate_rho0();
    pmx = pmy = 0.0f;
}

// screen (tilted) → sim (untilted) space, about the jar centre
static void screen_to_sim(float sx, float sy, float *ox, float *oy) {
    float cx = (JX0 + JX1) * 0.5f, cy = (JY0 + JY1) * 0.5f;
    float ct = cos_deg(tilt), st = sin_deg(tilt);
    float dx = sx - cx, dy = sy - cy;
    *ox = cx + dx * ct + dy * st;            // inverse rotation
    *oy = cy - dx * st + dy * ct;
}

void update(void) {
    // --- input: tilt the jar (rotate gravity), ease back to level when let go ---
    float t = 0.0f;
    if (key(KEY_LEFT)  || key('A')) t -= 1.0f;
    if (key(KEY_RIGHT) || key('D')) t += 1.0f;
    if (t != 0.0f) tilt = clamp(tilt + t * 1.6f, -45.0f, 45.0f);
    else           tilt *= 0.94f;            // spring back to level

    if (keyp('R')) init();

    // pour a little stream from the top-centre while SPACE is held
    if (key(KEY_SPACE) && N < MAXP - 6) {
        float cx = (JX0 + JX1) * 0.5f;
        for (int k = -2; k <= 2; k++) add_pt(cx + k * 3.0f, JY0 + 4.0f, 0.0f, 1.2f);
    }

    // mouse stir — push particles near the cursor along the drag direction
    float smx, smy; screen_to_sim((float)mouse_x(), (float)mouse_y(), &smx, &smy);
    if (mouse_down(MOUSE_LEFT)) {
        float dvx = (smx - pmx), dvy = (smy - pmy);
        float mv = vlen(dvx, dvy);
        if (mv > 0.5f) {
            float sr = 22.0f;
            for (int i = 0; i < N; i++) {
                float dx = P[i].x - smx, dy = P[i].y - smy, d = vlen(dx, dy);
                if (d < sr) { float f = (1.0f - d / sr) * 0.6f; phys_kick(&P[i], dvx * f, dvy * f); }
            }
        }
    }
    pmx = smx; pmy = smy;

    // gravity in the jar's frame: tilt rotates it so water pools to the low corner
    float gx = GRAV * sin_deg(tilt), gy = GRAV * cos_deg(tilt);

    // 1) integrate (predict) + clamp runaway speed
    for (int i = 0; i < N; i++) {
        phys_integrate(&P[i], gx, gy, DAMP);
        float vx = P[i].x - P[i].px, vy = P[i].y - P[i].py, sp = vlen(vx, vy);
        if (sp > VMAX) { float s = VMAX / sp; P[i].px = P[i].x - vx * s; P[i].py = P[i].y - vy * s; }
    }

    // 2) neighbours from the predicted positions (once per frame)
    build_grid(); build_neighbours();

    // 3) density constraint — the incompressibility solve
    for (int it = 0; it < ITERS; it++) {
        // 3a) per particle: density → constraint C = rho/rho0 - 1 → lambda
        for (int i = 0; i < N; i++) {
            float rho = density_at(i);
            float C = rho / rho0 - 1.0f;
            float gxi = 0.0f, gyi = 0.0f, sumk = 0.0f;   // grad wrt i, and sum |grad wrt j|²
            for (int k = 0; k < nbrN[i]; k++) {
                int j = nbr[i * MAXN + k];
                float dx = P[i].x - P[j].x, dy = P[i].y - P[j].y, r = vlen(dx, dy);
                float gc = spiky_gcoef(r);
                float gjx = gc * dx / rho0, gjy = gc * dy / rho0;   // grad of C_i wrt p_j = -grad wrt p_i term
                gxi += gjx; gyi += gjy;
                sumk += gjx * gjx + gjy * gjy;
            }
            float sumGrad2 = gxi * gxi + gyi * gyi + sumk;
            lam[i] = -C / (sumGrad2 + EPS);
        }
        // 3b) per particle: position correction from its own + neighbours' lambdas (+ anti-clump)
        for (int i = 0; i < N; i++) {
            float cx = 0.0f, cy = 0.0f;
            for (int k = 0; k < nbrN[i]; k++) {
                int j = nbr[i * MAXN + k];
                float dx = P[i].x - P[j].x, dy = P[i].y - P[j].y, r = vlen(dx, dy);
                float gc = spiky_gcoef(r);
                float w = poly6(r * r) / WDQ;                       // s_corr: (W(r)/W(dq))^n
                float sc = -SCORR_K * w * w * w * w;                // n = 4
                float coef = (lam[i] + lam[j] + sc) / rho0;
                cx += coef * gc * dx; cy += coef * gc * dy;
            }
            dpx[i] = cx; dpy[i] = cy;
        }
        for (int i = 0; i < N; i++) { P[i].x += dpx[i]; P[i].y += dpy[i]; }
        // 3c) walls — keep every particle inside the jar
        for (int i = 0; i < N; i++) phys_bounds(&P[i], JX0, JY0, JX1, JY1, 0.2f, 0.92f);
    }

    // 4) XSPH viscosity — nudge each particle's velocity toward its neighbours' average
    for (int i = 0; i < N; i++) { vlx[i] = P[i].x - P[i].px; vly[i] = P[i].y - P[i].py; }
    for (int i = 0; i < N; i++) {
        float ax = 0.0f, ay = 0.0f;
        for (int k = 0; k < nbrN[i]; k++) {
            int j = nbr[i * MAXN + k];
            float dx = P[i].x - P[j].x, dy = P[i].y - P[j].y;
            float w = poly6(dx * dx + dy * dy) / rho0;
            ax += (vlx[j] - vlx[i]) * w; ay += (vly[j] - vly[i]) * w;
        }
        // apply as a velocity change → move prev-position the other way (verlet)
        P[i].px -= VISC * ax; P[i].py -= VISC * ay;
    }

    // 5) velocity limiter — a corner pinch can eject one particle at a huge speed; cap it so
    //    a single spike can't spark across the jar or feed back into the next frame.
    for (int i = 0; i < N; i++) {
        float vx = P[i].x - P[i].px, vy = P[i].y - P[i].py, sp = vlen(vx, vy);
        if (sp > VMAX) { float s = VMAX / sp; P[i].px = P[i].x - vx * s; P[i].py = P[i].y - vy * s; }
    }

#ifdef DE_TRACE
    float mxsp = 0.0f, sumrho = 0.0f;
    for (int i = 0; i < N; i++) {
        float sp = vlen(P[i].x - P[i].px, P[i].y - P[i].py); if (sp > mxsp) mxsp = sp;
        sumrho += density_at(i);
    }
    watch("N", "%d", N);
    watch("tilt", "%.1f", tilt);
    watch("maxspd", "%.3f", mxsp);
    watch("avgrho", "%.3f", N ? (sumrho / N) / rho0 : 0.0f);   // → 1.0 when incompressible
#endif
}

// speed → colour ramp: still deep water is dark, fast foam goes white
static int speed_colour(float sp) {
    static const int ramp[] = { CLR_TRUE_BLUE, CLR_BLUE, CLR_BLUE, CLR_LIGHT_GREY, CLR_WHITE };
    int n = 5;
    int idx = (int)(sp / (VMAX * 0.6f) * (n - 1));
    if (idx < 0) idx = 0; if (idx > n - 1) idx = n - 1;
    return ramp[idx];
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    float cx = (JX0 + JX1) * 0.5f, cy = (JY0 + JY1) * 0.5f;
    float ct = cos_deg(tilt), st = sin_deg(tilt);
    #define ROTX(x, y) (cx + ((x) - cx) * ct - ((y) - cy) * st)
    #define ROTY(x, y) (cy + ((x) - cx) * st + ((y) - cy) * ct)

    // jar walls (rotated rectangle) — left / bottom / right, open top
    float c0x = ROTX(JX0, JY0), c0y = ROTY(JX0, JY0);
    float c1x = ROTX(JX1, JY0), c1y = ROTY(JX1, JY0);
    float c2x = ROTX(JX1, JY1), c2y = ROTY(JX1, JY1);
    float c3x = ROTX(JX0, JY1), c3y = ROTY(JX0, JY1);
    line((int)c0x, (int)c0y, (int)c3x, (int)c3y, CLR_DARK_GREY);   // left
    line((int)c3x, (int)c3y, (int)c2x, (int)c2y, CLR_DARK_GREY);   // bottom
    line((int)c2x, (int)c2y, (int)c1x, (int)c1y, CLR_DARK_GREY);   // right

    // water: each particle a small circle, coloured by speed → the motion reads
    for (int i = 0; i < N; i++) {
        float sp = vlen(P[i].x - P[i].px, P[i].y - P[i].py);
        float rx = ROTX(P[i].x, P[i].y), ry = ROTY(P[i].x, P[i].y);
        circfill((int)rx, (int)ry, 2, speed_colour(sp));
    }

    font(FONT_SMALL);
    print("water jar - a code-first PBD fluid (physics.h)", 4, 4, CLR_LIGHT_GREY);
    print("< > tilt   drag stir   SPACE pour   R reset", 4, SCREEN_H - 9, CLR_DARK_GREY);
}
