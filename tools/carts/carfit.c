/* de:meta
{
  "slug": "carfit",
  "title": "Car Fit — sprite → poly + deform mesh",
  "status": "active",
  "created": "2026-07-23",
  "kind": ["tool"],
  "teaches": ["procedural-mesh", "marching-squares", "verlet-integration", "soft-body"],
  "lineage": "The auto-mesh half of the deforming-texture + Box2D spike: reuses jelly.c's verlet+tritex skin, but instead of a hand-authored rim ring it DERIVES both the collision outline and the deform mesh from an arbitrary sprite's alpha at runtime (sget → Moore trace → RDP → grid-clip triangulation).",
  "description": {
    "summary": "Feed it an arbitrary car sprite; it auto-traces the silhouette into a simplified collision polygon (what you'd hand Box2D) AND a triangulated interior mesh (UV'd by rest position) that you can grab and squish — the paint job deforms with it. No hand-authored geometry: outline + mesh are both read from the sprite's alpha.",
    "detail": "Two problems that share one input. THE POLY: scan the sprite region with sget() into an alpha mask (index 0 = outside), Moore-neighbour boundary-trace the silhouette, then Ramer-Douglas-Peucker-simplify it to a handful of verts — that loop is the convex-decomposable hull you'd feed b2MakePolygon. THE TEXTURE: overlay a grid on the mask, keep vertices whose pixel is opaque, and emit two triangles per fully-inside cell; each vertex's UV is just its rest pixel position, so tritex paints the sprite onto the mesh with zero seams. The mesh vertices are runtime/physics.h verlet points linked to their grid neighbours (structural + shear), so grabbing one squishes the whole car and it springs back — the sprite's paint deforming along with the geometry. Box2D isn't wired here: this is the geometry-extraction spike that proves the auto pipeline before the rigid chassis+wheels split goes on top.",
    "controls": "Drag any point of the car to squish/throw it. [O] toggle the detected collision outline, [W] mesh wireframe, [T] texture on/off, [R] reset."
  },
  "todo": [
    "Semantic split: Hough-circle the wheels out of the mask so chassis + 2 wheels become separate Box2D bodies on revolute joints (this spike is one soft blob).",
    "Convex-decompose the RDP outline (Bayazit) into ≤8-vert hulls and actually hand them to b2MakePolygon (buggy cart's world).",
    "Skin the outline to the deformed mesh so the drawn silhouette follows the squish (currently the outline is the rest-space diagram)."
  ]
}
de:meta */
#include "studio.h"
#include "physics.h"
#include <math.h>
#include <stdio.h>   // snprintf — HUD labels

static inline float vlen(float x, float y) { return sqrtf(x*x + y*y); }

// ── CAR FIT ────────────────────────────────────────────────────────────────
// One input (a sprite's alpha), two outputs:
//   1. a simplified OUTLINE polygon  → the collision hull (Box2D-bound)
//   2. a triangulated INTERIOR mesh  → a deformable tritex skin (UV = rest px)
// Both are derived at runtime from sget() — nothing about the geometry is
// hand-authored, so any sprite dropped in the sheet region gets the same
// treatment. See jelly.c for the verlet+tritex skin this reuses.

#define CARW   64            // sprite region on the sheet: (0,0)-(CARW-1,CARH-1)
#define CARH   32
#define OX      0            // sheet origin of the car region (also the UV origin)
#define OY      0

#define VIEW    3.0f         // rest-px → screen-px zoom for the playable car
#define STEP    4            // grid spacing (rest px) for the deform mesh
#define GW     (CARW/STEP+1)  // grid columns
#define GH     (CARH/STEP+1)  // grid rows
#define MAXV   (GW*GH)
#define MAXT   (GW*GH*2)
#define MAXOUT  512          // raw traced boundary pixels
#define EPS     1.5f         // RDP tolerance (rest px)

#define FLOORY  (SCREEN_H - 18)

static bool alpha[CARH][CARW];        // the mask: true = opaque = inside the car

// ---- deform mesh ------------------------------------------------------------
static PhysPt pt[MAXV];               // one verlet point per active grid vertex
static float  rx[MAXV], ry[MAXV];     // rest position (rest px) → also the UV
static int    vid[GH][GW];            // grid cell → vertex index (-1 = outside)
static int    nv;
static int    mtri[MAXT][3];           // triangle vertex indices
static int    nt;
// structural + shear links, precomputed once
static int    lnk[MAXV*4][2]; static float lnkRest[MAXV*4]; static int nl;

// ---- detected outline -------------------------------------------------------
static int    outX[MAXOUT], outY[MAXOUT];   // simplified collision polygon (rest px)
static int    nout;

static int    grab = -1;                     // grabbed vertex
static float  pmx, pmy;                       // previous mouse (for throw velocity)
static bool   showOutline = true, showWire = false, showTex = true;

// ── mask ─────────────────────────────────────────────────────────────────────
static inline bool M(int x, int y) {
    if (x < 0 || y < 0 || x >= CARW || y >= CARH) return false;
    return alpha[y][x];
}

static void read_mask(void) {
    for (int y = 0; y < CARH; y++)
        for (int x = 0; x < CARW; x++)
            alpha[y][x] = sget(OX + x, OY + y) != 0;   // index 0 == transparent == outside
}

// ── Moore-neighbour boundary trace ───────────────────────────────────────────
// Walk the silhouette clockwise; produces the raw boundary pixel loop.
// Neighbour offsets in CW order (screen y-down): E,SE,S,SW,W,NW,N,NE.
static const int NX[8] = { 1, 1, 0,-1,-1,-1, 0, 1};
static const int NY[8] = { 0, 1, 1, 1, 0,-1,-1,-1};

static int trace(int px[], int py[], int max) {
    int sx = -1, sy = -1;
    for (int y = 0; y < CARH && sy < 0; y++)          // first opaque pixel (top-left-most)
        for (int x = 0; x < CARW; x++)
            if (M(x, y)) { sx = x; sy = y; break; }
    if (sy < 0) return 0;

    int n = 0;
    int cx = sx, cy = sy;                              // current boundary pixel
    int bx = sx - 1, by = sy;                          // backtrack (the empty pixel we came from = west)
    px[n] = cx; py[n] = cy; n++;

    for (int guard = 0; guard < max * 4; guard++) {
        int d0 = 0;                                    // index of backtrack around current
        for (int d = 0; d < 8; d++)
            if (cx + NX[d] == bx && cy + NY[d] == by) { d0 = d; break; }

        bool found = false;
        for (int i = 1; i <= 8; i++) {                 // search CW from just past the backtrack
            int d  = (d0 + i) & 7;
            int nx = cx + NX[d], ny = cy + NY[d];
            if (M(nx, ny)) {
                bx = cx + NX[(d + 7) & 7];             // new backtrack = the cell checked just before
                by = cy + NY[(d + 7) & 7];
                cx = nx; cy = ny; found = true; break;
            }
        }
        if (!found) break;                             // isolated pixel
        if (cx == sx && cy == sy) break;               // closed the loop
        if (n >= max) break;
        px[n] = cx; py[n] = cy; n++;
    }
    return n;
}

// ── Ramer-Douglas-Peucker (iterative, keep[] marks survivors) ────────────────
static int perp2(int ax, int ay, int bx, int by, int cx, int cy) {  // dist² of C to line AB × |AB|²
    int dx = bx - ax, dy = by - ay;
    int cross = (cx - ax) * dy - (cy - ay) * dx;
    return cross * cross;   // compared against EPS² * |AB|² by the caller
}

static void rdp(const int px[], const int py[], int n, int keep[]) {
    // stack of [lo,hi] index ranges
    static int st[MAXOUT][2]; int sp = 0;
    keep[0] = keep[n - 1] = 1;
    st[sp][0] = 0; st[sp][1] = n - 1; sp++;
    while (sp > 0) {
        int lo = st[--sp][0], hi = st[sp][1];
        if (hi <= lo + 1) continue;
        int ax = px[lo], ay = py[lo], bx = px[hi], by = py[hi];
        int abLen2 = (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
        long best = -1; int bi = -1;
        for (int k = lo + 1; k < hi; k++) {
            long d2 = perp2(ax, ay, bx, by, px[k], py[k]);
            if (d2 > best) { best = d2; bi = k; }
        }
        // keep bi if its perpendicular distance > EPS  ⇔  best > EPS² * |AB|²
        if (bi >= 0 && (double)best > (double)EPS * EPS * (abLen2 ? abLen2 : 1)) {
            keep[bi] = 1;
            st[sp][0] = lo; st[sp][1] = bi; sp++;
            st[sp][0] = bi; st[sp][1] = hi; sp++;
        }
    }
}

static void build_outline(void) {
    static int px[MAXOUT], py[MAXOUT], keep[MAXOUT];
    int n = trace(px, py, MAXOUT);
    for (int i = 0; i < n; i++) keep[i] = 0;
    if (n >= 2) rdp(px, py, n, keep);
    nout = 0;
    for (int i = 0; i < n && nout < MAXOUT; i++)
        if (keep[i]) { outX[nout] = px[i]; outY[nout] = py[i]; nout++; }
}

// ── grid-clip triangulation → the deform mesh ────────────────────────────────
static float wox, woy;   // world offset the car spawns / resets at

static void place_rest(int i) {   // vertex i to its rest world position, zero velocity
    phys_place(&pt[i], wox + rx[i] * VIEW, woy + ry[i] * VIEW);
}

static void add_link(int a, int b) {
    if (a < 0 || b < 0) return;
    lnk[nl][0] = a; lnk[nl][1] = b;
    lnkRest[nl] = vlen(rx[a] - rx[b], ry[a] - ry[b]) * VIEW;
    nl++;
}

static void build_mesh(void) {
    nv = nt = nl = 0;
    for (int gy = 0; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++) {
            int sx = gx * STEP; if (sx >= CARW) sx = CARW - 1;
            int sy = gy * STEP; if (sy >= CARH) sy = CARH - 1;
            if (M(sx, sy)) {
                vid[gy][gx] = nv;
                rx[nv] = sx; ry[nv] = sy;
                phys_pt(&pt[nv], 0, 0, 1.0f, 2.0f);
                nv++;
            } else vid[gy][gx] = -1;
        }
    // triangles: any grid cell whose four corners are all inside
    for (int gy = 0; gy < GH - 1; gy++)
        for (int gx = 0; gx < GW - 1; gx++) {
            int a = vid[gy][gx], b = vid[gy][gx+1], c = vid[gy+1][gx+1], d = vid[gy+1][gx];
            if (a >= 0 && b >= 0 && c >= 0 && d >= 0) {
                mtri[nt][0]=a; mtri[nt][1]=b; mtri[nt][2]=c; nt++;
                mtri[nt][0]=a; mtri[nt][1]=c; mtri[nt][2]=d; nt++;
            }
        }
    // links: structural (right, down) + shear (both diagonals) for a stiff soft body
    for (int gy = 0; gy < GH; gy++)
        for (int gx = 0; gx < GW; gx++) {
            int v = vid[gy][gx]; if (v < 0) continue;
            if (gx+1 < GW) add_link(v, vid[gy][gx+1]);
            if (gy+1 < GH) add_link(v, vid[gy+1][gx]);
            if (gx+1 < GW && gy+1 < GH) add_link(v, vid[gy+1][gx+1]);
            if (gx-1 >= 0 && gy+1 < GH) add_link(v, vid[gy+1][gx-1]);
        }
}

static void reset_pose(void) { for (int i = 0; i < nv; i++) place_rest(i); grab = -1; }

static bool inited = false;
static void setup(void) {
    read_mask();
    build_outline();
    wox = SCREEN_W * 0.5f - CARW * VIEW * 0.5f;
    woy = 30.0f;
    build_mesh();
    reset_pose();
    inited = true;
}

void update(void) {
    if (!inited) setup();
    float mx = (float)mouse_x(), my = (float)mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {                 // grab nearest vertex
        float best = 24.0f;
        for (int i = 0; i < nv; i++) {
            float d = vlen(mx - pt[i].x, my - pt[i].y);
            if (d < best) { best = d; grab = i; }
        }
        if (grab >= 0) pt[grab].w = 0;               // pinned while held
    }
    if (grab >= 0 && mouse_down(MOUSE_LEFT)) phys_grab(&pt[grab], mx, my, pmx, pmy);
    if (mouse_released(MOUSE_LEFT) && grab >= 0) { pt[grab].w = 1.0f; grab = -1; }

    if (keyp('R')) reset_pose();
    if (keyp('O')) showOutline = !showOutline;
    if (keyp('W')) showWire    = !showWire;
    if (keyp('T')) showTex     = !showTex;

    const int SUB = 3, ITER = 6;
    for (int s = 0; s < SUB; s++) {
        for (int i = 0; i < nv; i++) phys_integrate(&pt[i], 0.0f, 0.35f / SUB, 0.99f);
        for (int it = 0; it < ITER; it++) {
            for (int l = 0; l < nl; l++) phys_link(&pt[lnk[l][0]], &pt[lnk[l][1]], lnkRest[l]);
            for (int i = 0; i < nv; i++) phys_bounds(&pt[i], 0, 0, SCREEN_W, FLOORY, 0.3f, 0.7f);
        }
    }

#ifdef DE_TRACE
    watch("verts",   "%d", nv);
    watch("tris",    "%d", nt);
    watch("outline", "%d", nout);
#endif
    pmx = mx; pmy = my;
}

// draw the detected collision outline as a diagram on the raw sprite (top-left inset)
static void draw_inset(void) {
    int s = 2, ix = 6, iy = 6;
    sspr(OX, OY, CARW, CARH, ix, iy, CARW * s, CARH * s);
    for (int i = 0; i < nout; i++) {
        int j = (i + 1) % nout;
        line(ix + outX[i]*s, iy + outY[i]*s, ix + outX[j]*s, iy + outY[j]*s, CLR_WHITE);
        rectfill(ix + outX[i]*s - 1, iy + outY[i]*s - 1, 3, 3, CLR_YELLOW);
    }
    font(FONT_SMALL);
    print("detected hull -> Box2D", ix, iy + CARH*s + 2, CLR_LIGHT_GREY);
    char b[48]; snprintf(b, sizeof b, "%d verts", nout);
    print(b, ix, iy + CARH*s + 10, CLR_YELLOW);
}

void draw(void) {
    if (!inited) setup();
    cls(CLR_DARK_BLUE);
    rectfill(0, FLOORY, SCREEN_W, SCREEN_H - FLOORY, CLR_DARK_GREEN);

    if (showTex)
        for (int t = 0; t < nt; t++) {
            int a = mtri[t][0], b = mtri[t][1], c = mtri[t][2];
            tritex((int)pt[a].x, (int)pt[a].y, OX + rx[a], OY + ry[a],
                   (int)pt[b].x, (int)pt[b].y, OX + rx[b], OY + ry[b],
                   (int)pt[c].x, (int)pt[c].y, OX + rx[c], OY + ry[c]);
        }

    if (showWire)
        for (int l = 0; l < nl; l++)
            line((int)pt[lnk[l][0]].x, (int)pt[lnk[l][0]].y,
                 (int)pt[lnk[l][1]].x, (int)pt[lnk[l][1]].y, CLR_DARK_GREY);

    if (showOutline)                                   // rest-space hull, skinned to the mesh bbox drift
        for (int i = 0; i < nout; i++) {
            int j = (i + 1) % nout;
            // map rest px → the mesh's grid vertex nearest that rest point, for a live-ish overlay
            int gi = ((int)(outY[i]/(float)STEP+0.5f)); int gxi = ((int)(outX[i]/(float)STEP+0.5f));
            int gj = ((int)(outY[j]/(float)STEP+0.5f)); int gxj = ((int)(outX[j]/(float)STEP+0.5f));
            if (gi<0)gi=0; if(gi>=GH)gi=GH-1; if(gxi<0)gxi=0; if(gxi>=GW)gxi=GW-1;
            if (gj<0)gj=0; if(gj>=GH)gj=GH-1; if(gxj<0)gxj=0; if(gxj>=GW)gxj=GW-1;
            int va = vid[gi][gxi], vb = vid[gj][gxj];
            if (va>=0 && vb>=0)
                line((int)pt[va].x,(int)pt[va].y,(int)pt[vb].x,(int)pt[vb].y, CLR_YELLOW);
        }

    draw_inset();

    font(FONT_SMALL);
    char b[96];
    snprintf(b, sizeof b, "auto: sprite alpha -> %d-vert hull + %d-tri deform mesh   drag to squish", nout, nt);
    print(b, 6, SCREEN_H - 12, CLR_WHITE);
    print("O outline  W wire  T texture  R reset", SCREEN_W - 168, 6, CLR_LIGHT_GREY);
}
