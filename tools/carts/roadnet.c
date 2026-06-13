#include "studio.h"
#include "ui.h"
#include <stdio.h>

// ============================================================================
// ROADNET  —  a SPLINE arterial network over an infinite heightmap.   (rungs 1-3)
//
//   intro panel      drag the SLIDERS (world re-rolls live behind the panel),
//                    ROLL = fresh seed, EXPLORE / ENTER = dismiss the panel
//   ◄▲▼►  /  WASD   pan across the (infinite) world
//   left-drag        grab + pan the world with the mouse
//   mouse wheel      zoom in / out (a bit), pivoting on the screen centre
//   SPACE           deterministic jump to fresh scenery
//   R               new seed (a fresh, repeatable world)
//   M               re-open the setup panel
//   G               toggle the CELL-BORDER overlay  (the seam test)
//   H               hide HUD (clean screenshot)
//
// ── what this is ─────────────────────────────────────────────────────────────
// The thing procgen-places.md parked as "the wall": ARCED, non-axis-aligned roads
// in a deterministic, infinite world. The crack (see docs/design/roadnet.md): a
// NODE-LATTICE spline is local even though a *grown* spline isn't. Each coarse cell
// hashes to at most one POI node (worldgen's trick); a link between two nodes is a
// Catmull-Rom whose 4 control points are node(C-d), node(C), node(C+d), node(C+2d)
// — every one a PURE FUNCTION of cell coords. So any chunk recomputes the identical
// curve with no global pass: infinite + deterministic + seam-true.
//
// CONNECTED by HIERARCHY (rung 1.5): a flat node lattice fragments into clumps (a
// link needs BOTH adjacent cells to hold a node → a sub-percolation random graph).
// Real road networks are connected, so we split into two tiers — the "Main Rib":
//   • HIGHWAYS — hubs on a COARSE lattice (HUB_CS), each linked to its neighbour
//     hubs. A lattice graph is connected by construction → one spine spans the
//     world (short water crossed by bridges; only WIDE open water breaks it).
//   • MINOR ROADS — every town links to its nearest hub (a feeder onto the spine)
//     AND to its nearest other town (a local road). So clusters chain together and
//     a town far from a hub still connects via town→town→…→hub — not everything
//     needs a highway. Degree ~1 each → no criss-cross.
// All tiers are the same pure-function-of-cell-coords spline, so still seam-true.
//
// Borrowed VERBATIM from worldgen.c (the gtascii port): height_at / biome_col /
// passable / hash2 / ifloor. New here: the node tangents + the Hermite ribbon.
//
// Rung 1 = THE SEAM TEST (pan across a cell border, toggle G; the same curve draws
// from either side). Rung 2 = TERRAIN-AWARE ROUTING: link_path() tries straight →
// BRIDGE a short water gap (river/strait, ≤ MAX_BRIDGE; drawn as a distinct deck) →
// BEND around the obstacle on land; drops only if none works. Roads never draw over
// water *except* a tagged bridge span. Tunnels / valley-following stay v2.
// Rung 3 = RANK + ROAD CLASS: each node hashes to a rank (hamlet/town/city/metro,
// high ranks rare → cities feel spaced out), and a link's class is f(its endpoints'
// ranks) — motorway/highway/arterial/street/dirt — driving width, colour, and a
// dashed centre-line on the big roads. Markers size by rank too. (Landmarks = later.)
// ============================================================================

#define TILE 4                                  // screen px per world tile at zoom 1
#define ZMIN 0.12f                               // mousewheel zoom range — far out
#define ZMAX 2.5f                                 // (terrain is sampled per screen
                                                  //  pixel, so there's no buffer ceiling)
#define SPAWN_X 312.0f
#define SPAWN_Y 8.0f

#define ROAD_R    2                  // road ribbon radius (px) at this zoom
#define PANEL_W   96                 // intro-panel width (px); left-drag pans only right of it

// ── tweakable world params — all 0..1 (driven by the intro-panel sliders) ─────
// Getters map each to its real range; the NODE_CS / HUB_CS macros below call them
// so every existing call site stays unchanged. Defaults reproduce the tuned look.
static float P_hub_space  = 0.167f;  // hub lattice spacing  → 12..60 tiles
static float P_town_space = 0.167f;  // town lattice spacing → 6..30 tiles
static float P_hub_dens   = 0.75f;   // hub presence         → 40..100 %
static float P_town_dens  = 0.444f;  // town presence        → 0..90 %
static float P_jitter     = 0.6f;    // node wiggle off-grid → 0..~0.45·cell
static float P_diag       = 0.25f;   // diagonal shortcuts   → 0..100% of quads (no X)
static float P_sea        = 0.5f;    // sea level            → subtract 0.30..0.60

static int   hub_cs(void)   { return 12 + (int)(P_hub_space  * 48); }   // 12..60
static int   node_cs(void)  { return 6  + (int)(P_town_space * 24); }   // 6..30
static int   hub_pct(void)  { return 40 + (int)(P_hub_dens   * 60); }   // 40..100
static int   town_pct(void) { return (int)(P_town_dens * 90); }         // 0..90
static int   jit(int cs)    { int j = (int)(P_jitter * cs * 0.45f); return j < 1 ? 1 : j; }
static int   diag_pct(void) { return (int)(P_diag * 100); }             // 0..100
static float sea_sub(void)  { return 0.30f + P_sea * 0.30f; }           // 0.30..0.60
#define NODE_CS  node_cs()
#define HUB_CS   hub_cs()

static int mode = 0;                 // 0 = intro/setup panel, 1 = explore

static float camX, camY;             // top-left of view, in world-tile coords
static float seedZ;                  // noise z-slice = the world's "seed"
static float jumpN;                  // SPACE counter → deterministic far jumps
static int   show_grid = 0;          // cell-border overlay (seam test)
static int   show_hud  = 1;
static float zoom = 1.0f;            // mousewheel zoom (ZMIN..ZMAX)
static float P    = TILE;            // pixels per tile = TILE * zoom (set per frame)
static int   vcols, vrows;           // visible world-tile span this frame (road bounds)
static int   dragging = 0, drag_px, drag_py;  // left-drag-to-pan ("grab the map")

static void view_metrics(void) {     // recompute P + visible tile span from zoom
    P = TILE * zoom;
    vcols = (int)(SCREEN_W / P) + 3;             // world-tile span (road-loop bounds)
    vrows = (int)(SCREEN_H / P) + 3;
}

// ── deterministic per-cell hash (worldgen's) ─────────────────────────────────
static unsigned hash2(int a, int b) {
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static int ifloor(float v) { int i = (int)v; return (v < 0 && (float)i != v) ? i - 1 : i; }

// ── HEIGHTMAP + RIVERS — worldgen's createZoomedinWorld(), verbatim (scalein 1) ─
static float height_at(float i, float j) {
    float x1 = noise3(i / 100.0f, j / 100.0f, seedZ);   // big continents
    float x2 = noise3(i / 10.0f,  j / 10.0f,  seedZ);   // fine roughness
    float x3 = noise3(i / 30.0f,  j / 30.0f,  seedZ);   // medium hills
    float x = 0.6f * x1 + 0.3f * x3 + 0.1f * x2;
    x = x - sea_sub();                                   // drop sea level (water slider)
    float xr = noise2(i / 40.0f, j / 40.0f); xr = xr * xr;
    if (xr > 0.3f && xr < 0.3f + (0.5f * 0.3f) * 0.5f && x > 0.0f) x = -0.5f;
    xr = noise2(i / 20.0f, j / 20.0f);
    if (xr > 0.3f && xr < 0.3f + (0.05f * 0.3f) * 0.5f && x > 0.0f) x = -0.5f;
    return x;
}
static int biome_col(float x) {
    if (x == -0.5f)  return CLR_DARK_BLUE;     // deep water / river
    if (x <  0.00f)  return CLR_BLUE;          // shallow water
    if (x <  0.01f)  return CLR_YELLOW;        // beach
    if (x <  0.05f)  return CLR_ORANGE;        // dunes
    if (x <  0.10f)  return CLR_PINK;          // scrub
    if (x <  0.20f)  return CLR_GREEN;         // grass
    if (x <  0.30f)  return CLR_DARK_GREEN;    // forest
    if (x <  0.40f)  return CLR_BROWN;         // hills
    if (x <  0.45f)  return CLR_DARK_GREY;     // rock
    return CLR_LIGHT_GREY;                      // peaks
}
static int passable(float x) { return x >= 0.0f && x < 0.4f; }   // road LOS test

// ── POI NODES — at most one per coarse cell, hashed + jittered, on habitable land ─
// Pure function of (cx,cy): same node falls out wherever/whenever you visit. This is
// worldgen's city_at(), kept as the network's node placement.
static int get_node(int cx, int cy, float *wx, float *wy) {
    unsigned h = hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    if (h % 100u >= (unsigned)town_pct()) return 0;      // town-density slider
    int j = jit(NODE_CS);                                // wiggle slider × cell
    float jx = (float)((int)((h >> 7)  % (2*j+1)) - j);
    float jy = (float)((int)((h >> 15) % (2*j+1)) - j);
    float x = cx * NODE_CS + NODE_CS * 0.5f + jx;
    float y = cy * NODE_CS + NODE_CS * 0.5f + jy;
    if (!passable(height_at(x, y))) return 0;            // habitable land only
    *wx = x; *wy = y;
    return 1;
}

// ── HUBS — the highway backbone, on a coarse lattice. Independent hash layer from
// towns (different constants), denser (~85%) so the lattice graph stays connected. ─
static int get_hub(int cx, int cy, float *wx, float *wy) {
    unsigned h = hash2(cx * 374761393u + 5, cy * 668265263u + 3);
    if (h % 100u >= (unsigned)hub_pct()) return 0;       // hub-density slider
    int j = jit(HUB_CS);                                 // wiggle slider × cell
    float jx = (float)((int)((h >> 6)  % (2*j+1)) - j);
    float jy = (float)((int)((h >> 14) % (2*j+1)) - j);
    float x = cx * HUB_CS + HUB_CS * 0.5f + jx;
    float y = cy * HUB_CS + HUB_CS * 0.5f + jy;
    if (!passable(height_at(x, y))) return 0;            // habitable land only
    *wx = x; *wy = y;
    return 1;
}

// ── RANK (rung 3) — every node gets a deterministic rank; the rare high ranks are
// what make big cities feel spaced out (rarity ≈ spacing, no Poisson pass needed).
// A road's CLASS is then just f(its endpoints' ranks) — pure functions both ends, so
// the class is identical from either chunk → still seam-true. ─────────────────────
enum { RK_HAMLET, RK_TOWN, RK_CITY, RK_METRO };
enum { CL_MOTORWAY, CL_HIGHWAY, CL_ARTERIAL, CL_STREET, CL_DIRT };

static int hub_rank(int cx, int cy) {                    // backbone node: city / metro
    unsigned h = hash2(cx * 915488749u + 31, cy * 147645773u + 7);
    return (h % 100u < 15u) ? RK_METRO : RK_CITY;        // metros are rare
}
static int town_rank(int cx, int cy) {                   // feeder node: town / hamlet
    unsigned h = hash2(cx * 1900385059u + 61, cy * 2120969693u + 17);
    return (h % 100u < 35u) ? RK_TOWN : RK_HAMLET;
}

// style per road class: ribbon radius, casing colour, centre colour, has centre-line
typedef struct { int r, casing, centre, mark; } RStyle;
static const RStyle RS[5] = {
    /* MOTORWAY */ { 4, CLR_DARKER_GREY, CLR_LIGHT_GREY,  1 },
    /* HIGHWAY  */ { 3, CLR_DARKER_GREY, CLR_LIGHT_GREY,  1 },
    /* ARTERIAL */ { 2, CLR_DARK_GREY,   CLR_LIGHT_GREY,  0 },
    /* STREET   */ { 2, CLR_DARK_GREY,   CLR_MEDIUM_GREY, 0 },
    /* DIRT     */ { 1, CLR_BROWN,       CLR_BROWN,       0 },
};

// world tile → screen px (centre of tile) — P = TILE*zoom, set per frame
static int sxp(float wx) { return (int)((wx - camX) * P + P * 0.5f); }
static int syp(float wy) { return (int)((wy - camY) * P + P * 0.5f); }

// Catmull-Rom basis through (p0,p1,p2,p3); p1->p2 is the drawn span, t in [0,1].
// (The geometry SEAM is link_path() below — this is just its base-curve helper.)
static void catmull(float p0,float p1,float p2,float p3,float t,float *out){
    float t2=t*t, t3=t2*t;
    *out = 0.5f*( (2*p1) + (-p0+p2)*t
                + (2*p0-5*p1+4*p2-p3)*t2
                + (-p0+3*p1-3*p2+p3)*t3 );
}

// The 4 forward link directions (the reverse 4 are owned by the neighbour cell, so
// each physical link is enumerated exactly once → no double-draw, no seam fight).
static const int DIR[4][2] = {{1,0},{0,1},{1,1},{1,-1}};

#define LINK_SAMPLES 20             // world-space samples per link (collision-ready)
#define MAXBEND      26.0f          // furthest a road will detour sideways (tiles)
#define MAX_BRIDGE   10.0f          // longest WATER span a road will bridge (tiles);
                                    // wider water = a real barrier (road drops)
static float lp_x[LINK_SAMPLES + 1], lp_y[LINK_SAMPLES + 1];   // last link_path() result
static int   lp_br[LINK_SAMPLES + 1];   // per-sample: 1 = this span is a BRIDGE (over water)

static int passable_at(float x, float y) { return passable(height_at(x, y)); }

// Geometry only: sample the base Catmull-Rom (c0,a,b,c3) into lp_, plus a perpendicular
// BOW of size `bend` peaking at the middle (sin window) — the terrain detour.
static void build_curve(float c0x, float c0y, float ax, float ay, float bx, float by,
                        float c3x, float c3y, float bend, float perpx, float perpy) {
    for (int i = 0; i <= LINK_SAMPLES; i++) {
        float t = (float)i / LINK_SAMPLES, bxv, byv;
        catmull(c0x, ax, bx, c3x, t, &bxv);
        catmull(c0y, ay, by, c3y, t, &byv);
        float w = sin_deg(t * 180.0f);               // 0 at ends, 1 at the middle
        lp_x[i] = bxv + perpx * bend * w;
        lp_y[i] = byv + perpy * bend * w;
    }
}

// Scan the current lp_ curve. *peak = any impassable HIGH ground (rock/peak — can't
// bridge those). *maxwater = longest run of WATER samples (bridgeable). Returns 1 if
// the whole curve is on passable land (no blockage at all).
static int classify_curve(int *peak, int *maxwater) {
    *peak = 0; *maxwater = 0; int run = 0, blocked = 0;
    for (int i = 0; i <= LINK_SAMPLES; i++) {
        float h = height_at(lp_x[i], lp_y[i]);
        if (passable(h)) { run = 0; continue; }
        blocked++;
        if (h >= 0.4f) *peak = 1;                    // rock/peak → not bridgeable
        else { run++; if (run > *maxwater) *maxwater = run; }   // water run
    }
    return blocked == 0;
}

// THE SEAM — the ONE place road geometry is produced. Fills lp_ (+ lp_br bridge flags)
// with the drivable path A→B (shaped by c0/c3). Tries, in order: straight; BRIDGE a
// short water gap; BEND around the obstacle on land. Returns the sample count, or 0 if
// none works (drop). Both the renderer and the future sloop road_at() read lp_.
static int link_path(float c0x, float c0y, float ax, float ay,
                     float bx, float by, float c3x, float c3y) {
    float dx = bx - ax, dy = by - ay, len = fsqrt(dx*dx + dy*dy);
    if (len < 0.5f) return 0;
    float ux = dx/len, uy = dy/len, perpx = -uy, perpy = ux;
    int peak, maxw;

    // 1. straight-ish base curve already clear? (most links)
    build_curve(c0x, c0y, ax, ay, bx, by, c3x, c3y, 0, perpx, perpy);
    if (classify_curve(&peak, &maxw)) {
        for (int i = 0; i <= LINK_SAMPLES; i++) lp_br[i] = 0;
        return LINK_SAMPLES + 1;
    }

    // 2. blocked only by a SHORT stretch of water (a river/strait)? → BRIDGE it.
    float spacing = len / LINK_SAMPLES;
    if (!peak && maxw * spacing <= MAX_BRIDGE) {
        for (int i = 0; i <= LINK_SAMPLES; i++)
            lp_br[i] = (height_at(lp_x[i], lp_y[i]) < 0.0f);   // tag the over-water spans
        return LINK_SAMPLES + 1;
    }

    // 3. peak, or water too wide to bridge → try to BEND around it on land.
    int steps = (int)(len / 1.5f); if (steps < 2) steps = 2;
    float sumt = 0; int nb = 0;
    for (int s = 1; s < steps; s++) {
        float t = (float)s / steps;
        if (!passable_at(ax + dx*t, ay + dy*t)) { sumt += t; nb++; }
    }
    if (!nb) return 0;
    float tc = sumt / nb, ox = ax + dx*tc, oy = ay + dy*tc;
    float clr = 0; int sgn = 0;
    for (float off = 1.5f; off <= MAXBEND; off += 1.5f) {
        if (passable_at(ox + perpx*off, oy + perpy*off)) { clr = off; sgn =  1; break; }
        if (passable_at(ox - perpx*off, oy - perpy*off)) { clr = off; sgn = -1; break; }
    }
    if (!sgn) return 0;                              // boxed in → drop
    for (float k = 1.0f; k <= 3.0f; k += 0.5f) {
        float bend = sgn * (clr * k + 2.0f);
        float ab = bend < 0 ? -bend : bend; if (ab > MAXBEND * 1.5f) break;
        build_curve(c0x, c0y, ax, ay, bx, by, c3x, c3y, bend, perpx, perpy);
        if (classify_curve(&peak, &maxw)) {
            for (int i = 0; i <= LINK_SAMPLES; i++) lp_br[i] = 0;
            return LINK_SAMPLES + 1;
        }
    }
    return 0;
}

// Stroke the path in lp_ as overlapping circles, fine enough at any zoom. Spans tagged
// as bridges (lp_br) draw in `bcol` so a road over water reads as a bridge.
static void stroke_path(int n, int r, int col, int bcol) {
    for (int i = 0; i + 1 < n; i++) {
        int c = (lp_br[i] || lp_br[i+1]) ? bcol : col;     // a span touching water = bridge
        int x0 = sxp(lp_x[i]), y0 = syp(lp_y[i]), x1 = sxp(lp_x[i+1]), y1 = syp(lp_y[i+1]);
        int dx = x1 - x0, dy = y1 - y0;
        int seg = (int)fsqrt((float)(dx*dx + dy*dy));
        int steps = seg / (r > 0 ? r : 1); if (steps < 1) steps = 1;
        for (int s = 0; s <= steps; s++) {
            float t = (float)s / steps;
            circfill(x0 + (int)(dx*t), y0 + (int)(dy*t), r, c);
        }
    }
}

// Dashed yellow centre-line along lp_ (highways/motorways). Walks the path in screen
// pixels so the dash period is constant at any zoom; skipped over bridge spans.
static void stroke_dashes(int n) {
    int acc = 0;
    for (int i = 0; i + 1 < n; i++) {
        int x0 = sxp(lp_x[i]), y0 = syp(lp_y[i]), x1 = sxp(lp_x[i+1]), y1 = syp(lp_y[i+1]);
        int dx = x1 - x0, dy = y1 - y0;
        int seg = (int)fsqrt((float)(dx*dx + dy*dy));
        int bridge = lp_br[i] || lp_br[i+1];
        for (int s = 0; s <= seg; s++) {
            if (!bridge && acc % 7 < 2) {                // 2 on / 5 off
                float t = seg ? (float)s / seg : 0;
                pset(x0 + (int)(dx*t), y0 + (int)(dy*t), CLR_YELLOW);
            }
            acc++;
        }
    }
}

// Draw one classified link for a phase: 0 = casing, 1 = centre (+ centre-line markings).
static void draw_link(int n, int cls, int phase) {
    const RStyle *s = &RS[cls];
    if (phase == 0) {
        stroke_path(n, s->r, s->casing, CLR_BROWN);      // bridge deck casing = brown
    } else {
        int cr = s->r - 1;
        if (cr >= 1) stroke_path(n, cr, s->centre, CLR_WHITE);   // bridge deck = white
        if (s->mark && zoom >= 0.7f) stroke_dashes(n);
    }
}

// HIGHWAYS — the connected backbone. Hub lattice, forward-only links, Catmull-Rom
// shaped by the hubs beyond each span (reflect when absent). phase 0=casing, 1=centre.
static void draw_highways(int phase) {
    int c0 = ifloor(camX / HUB_CS) - 2, c1 = ifloor((camX + vcols) / HUB_CS) + 2;
    int r0 = ifloor(camY / HUB_CS) - 2, r1 = ifloor((camY + vrows) / HUB_CS) + 2;
    for (int cx = c0; cx <= c1; cx++)
        for (int cy = r0; cy <= r1; cy++) {
            float p1x, p1y; if (!get_hub(cx, cy, &p1x, &p1y)) continue;
            for (int d = 0; d < 4; d++) {
                int dx = DIR[d][0], dy = DIR[d][1];
                if (d >= 2) {                                           // diagonal: AT MOST one per quad (no X)
                    int qx = cx, qy = (dy == 1) ? cy : cy - 1;          // the quad this diagonal crosses
                    unsigned qh = hash2(qx * 2246822519u + 7, qy * 3266489917u + 13);
                    if (qh % 100u >= (unsigned)diag_pct()) continue;    // this quad has no diagonal
                    if ((dy == 1) != (int)((qh >> 9) & 1)) continue;    // and it picked the OTHER diagonal
                }
                float p2x, p2y;
                if (!get_hub(cx + dx, cy + dy, &p2x, &p2y)) continue;
                float p0x, p0y, p3x, p3y;
                if (!get_hub(cx - dx, cy - dy, &p0x, &p0y)) { p0x = 2*p1x - p2x; p0y = 2*p1y - p2y; }
                if (!get_hub(cx + 2*dx, cy + 2*dy, &p3x, &p3y)) { p3x = 2*p2x - p1x; p3y = 2*p2y - p1y; }
                // class from the two hubs' ranks: a metro endpoint makes it a motorway
                int cls = (hub_rank(cx, cy) == RK_METRO || hub_rank(cx + dx, cy + dy) == RK_METRO)
                          ? CL_MOTORWAY : CL_HIGHWAY;
                int np = link_path(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y);  // bends/bridges terrain
                if (np) draw_link(np, cls, phase);                           // 0 = no route
            }
        }
}

// nearest present hub to a town, searched over the 5x5 hub cells around it (pure fn).
// Wide search so a town near a hub-gap still finds the backbone instead of orphaning.
static int nearest_hub(float tx, float ty, float *hx, float *hy) {
    int hc = ifloor(tx / HUB_CS), hr = ifloor(ty / HUB_CS);
    float best = 1e18f; int found = 0;
    for (int a = hc - 2; a <= hc + 2; a++)
        for (int b = hr - 2; b <= hr + 2; b++) {
            float wx, wy; if (!get_hub(a, b, &wx, &wy)) continue;
            float dx = wx - tx, dy = wy - ty, dd = dx*dx + dy*dy;
            if (dd < best) { best = dd; *hx = wx; *hy = wy; found = 1; }
        }
    return found;
}

// nearest OTHER town, over the 5x5 town cells around it (pure fn, self excluded)
static int nearest_town(float tx, float ty, float *nx, float *ny) {
    int tc = ifloor(tx / NODE_CS), tr = ifloor(ty / NODE_CS);
    float best = 1e18f; int found = 0;
    for (int a = tc - 2; a <= tc + 2; a++)
        for (int b = tr - 2; b <= tr + 2; b++) {
            float wx, wy; if (!get_node(a, b, &wx, &wy)) continue;
            if (wx == tx && wy == ty) continue;          // self
            float dx = wx - tx, dy = wy - ty, dd = dx*dx + dy*dy;
            if (dd < best) { best = dd; *nx = wx; *ny = wy; found = 1; }
        }
    return found;
}

// MINOR ROADS — each town links to its nearest hub (feeder onto the backbone) AND to
// its nearest other town (a local road). The local roads chain clusters together so a
// town far from any hub still connects via town→town→…→hub — not everything needs a
// highway. Degree ~1 each, so no criss-cross. Owned by the town; mutual nearest pairs
// double-draw harmlessly (identical pixels).
static void draw_feeders(int phase) {
    int m = 5;
    int c0 = ifloor(camX / NODE_CS) - m, c1 = ifloor((camX + vcols) / NODE_CS) + m;
    int r0 = ifloor(camY / NODE_CS) - m, r1 = ifloor((camY + vrows) / NODE_CS) + m;
    for (int cx = c0; cx <= c1; cx++)
        for (int cy = r0; cy <= r1; cy++) {
            float tx, ty; if (!get_node(cx, cy, &tx, &ty)) continue;
            int tr = town_rank(cx, cy);
            // feeder onto the backbone: a TOWN gets an arterial, a hamlet a street
            float hx, hy; int np;
            if (nearest_hub(tx, ty, &hx, &hy)) {
                int cls = (tr == RK_TOWN) ? CL_ARTERIAL : CL_STREET;
                np = link_path(2*tx - hx, 2*ty - hy, tx, ty, hx, hy, 2*hx - tx, 2*hy - ty);
                if (np) draw_link(np, cls, phase);
            }
            // local road to the nearest town: street if a town is involved, else a dirt track
            float nx, ny;
            if (nearest_town(tx, ty, &nx, &ny)) {
                int nr = town_rank(ifloor(nx / NODE_CS), ifloor(ny / NODE_CS));
                int cls = (tr == RK_TOWN || nr == RK_TOWN) ? CL_STREET : CL_DIRT;
                np = link_path(2*tx - nx, 2*ty - ny, tx, ty, nx, ny, 2*nx - tx, 2*ny - ty);
                if (np) draw_link(np, cls, phase);
            }
        }
}

static void draw_nodes(int detail) {
    // towns (feeder tier) — orange = town, small red = hamlet; hidden far out (LOD)
    if (detail) {
        int c0 = ifloor(camX / NODE_CS) - 2, c1 = ifloor((camX + vcols) / NODE_CS) + 2;
        int r0 = ifloor(camY / NODE_CS) - 2, r1 = ifloor((camY + vrows) / NODE_CS) + 2;
        for (int cx = c0; cx <= c1; cx++)
            for (int cy = r0; cy <= r1; cy++) {
                float wx, wy; if (!get_node(cx, cy, &wx, &wy)) continue;
                int px = sxp(wx), py = syp(wy);
                int town = town_rank(cx, cy) == RK_TOWN;
                circfill(px, py, town ? 3 : 2, CLR_BLACK);
                circfill(px, py, town ? 2 : 1, town ? CLR_ORANGE : CLR_RED);
            }
    }
    // hubs (backbone tier) — metros bigger than cities. Shrink with zoom so they don't
    // swarm into a polka-dot field that buries the highways when far out.
    int h0 = ifloor(camX / HUB_CS) - 2, h1 = ifloor((camX + vcols) / HUB_CS) + 2;
    int v0 = ifloor(camY / HUB_CS) - 2, v1 = ifloor((camY + vrows) / HUB_CS) + 2;
    for (int cx = h0; cx <= h1; cx++)
        for (int cy = v0; cy <= v1; cy++) {
            float wx, wy; if (!get_hub(cx, cy, &wx, &wy)) continue;
            int metro = hub_rank(cx, cy) == RK_METRO;
            int rr = (metro ? 4 : 3);
            if (zoom < 0.45f) rr = metro ? 2 : 1;        // LOD: tiny dots far out
            else if (zoom < 0.22f) rr = 1;
            int px = sxp(wx), py = syp(wy);
            circfill(px, py, rr + 1, CLR_BLACK);
            circfill(px, py, rr, CLR_WHITE);
            if (metro && rr >= 3) circfill(px, py, 1, CLR_RED);   // metro pip
        }
}

static void draw_grid_overlay(void) {       // the seam test aid: NODE_CS cell borders
    for (int cx = ifloor(camX / NODE_CS); cx * NODE_CS < camX + vcols; cx++) {
        int x = (int)((cx * NODE_CS - camX) * P);
        line(x, 0, x, SCREEN_H, CLR_DARK_PURPLE);
    }
    for (int cy = ifloor(camY / NODE_CS); cy * NODE_CS < camY + vrows; cy++) {
        int y = (int)((cy * NODE_CS - camY) * P);
        line(0, y, SCREEN_W, y, CLR_DARK_PURPLE);
    }
}

static void hud(void) {
    char buf[64];
    float cx = camX + vcols / 2.0f, cy = camY + vrows / 2.0f;
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("ROADNET", 4, 2, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "x %d  y %d", (int)cx, (int)cy);
    print(buf, 80, 2, CLR_MEDIUM_GREY);
    if (show_grid) print("[grid]", 160, 2, CLR_DARK_PURPLE);
    print_centered("drag/\x18\x19\x1a\x1b pan   wheel zoom   SPACE jump   R seed   M setup   G grid",
                   SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
}

// ── INTRO / SETUP PANEL — a glass strip of sliders over the LIVE world preview.
// The world is drawn full-screen behind this (draw() runs draw_world first), and
// every slider edits a P_* the generators read, so the world re-rolls live as you
// drag. No separate preview renderer — the whole right of the screen IS the preview.
static void draw_setup_panel(void) {
    char buf[48];
    int PW = PANEL_W;
    fillp(FILL_CHECKER, -1); rectfill(0, 0, PW, SCREEN_H, CLR_BLACK); fillp_reset();
    line(PW, 0, PW, SCREEN_H, CLR_DARK_GREY);
    print("ROADNET", 4, 4, CLR_LIGHT_GREY);

    font(FONT_SMALL);
    ui_begin();
    static const char *L[7] = { "hub gap", "town gap", "hub dens", "town dens",
                                "wiggle", "diag", "water" };
    float *pp[7] = { &P_hub_space, &P_town_space, &P_hub_dens, &P_town_dens,
                     &P_jitter, &P_diag, &P_sea };
    for (int i = 0; i < 7; i++) ui_slider(pp[i], 4, 16 + i * 12, 88, L[i]);
    int roll = ui_button(4, 104, 88, 14, "ROLL");
    int go   = ui_button(4, 122, 88, 16, "EXPLORE \x10");
    ui_end();

    snprintf(buf, sizeof buf, "seed #%u", (unsigned)(seedZ * 1000) % 100000u);
    print(buf, 4, 144, CLR_MEDIUM_GREY);
    print("drag = re-roll live", 4, SCREEN_H - 9, CLR_DARKER_GREY);
    font(FONT_NORMAL);

    if (roll) seedZ += 0.37f;                    // fresh world, same params
    if (go || keyp(KEY_ENTER)) mode = 1;
}

static void draw_world(void) {
    view_metrics();                                  // P + visible span from zoom
    // terrain — sampled per SCREEN cell, so cost is FIXED at any zoom (no per-tile
    // buffer → no zoom-out ceiling). seed + water are read live inside height_at.
    int step = 2;
    for (int sy = 0; sy < SCREEN_H; sy += step)
        for (int sx = 0; sx < SCREEN_W; sx += step)
            rectfill(sx, sy, step, step, biome_col(height_at(camX + sx / P, camY + sy / P)));
    if (show_grid) draw_grid_overlay();
    // LOD: zoomed far out, drop the minor-road/town tier — show only the highway
    // skeleton (keeps it legible + the road counts sane across a huge view span).
    int detail = (zoom >= 0.45f);
    draw_highways(0); if (detail) draw_feeders(0);    // casings first...
    draw_highways(1); if (detail) draw_feeders(1);    // ...then centres
    draw_nodes(detail);
}

void init(void) {
    zoom = 1.0f; view_metrics();
    camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f;
}

void update(void) {
    // mousewheel zoom, pivoting on the screen centre (clamped ZMIN..ZMAX)
    float w = mouse_wheel();
    if (w != 0) {
        float oldp = TILE * zoom;
        float ccx = camX + SCREEN_W * 0.5f / oldp, ccy = camY + SCREEN_H * 0.5f / oldp;
        zoom += (w > 0 ? 1 : -1) * 0.15f * zoom;
        if (zoom < ZMIN) zoom = ZMIN;
        if (zoom > ZMAX) zoom = ZMAX;
        float np = TILE * zoom;
        camX = ccx - SCREEN_W * 0.5f / np; camY = ccy - SCREEN_H * 0.5f / np;
    }

    float pan = 1.3f / zoom;                     // constant on-screen pan speed
    if (btn(0, BTN_LEFT)  || key('A')) camX -= pan;
    if (btn(0, BTN_RIGHT) || key('D')) camX += pan;
    if (btn(0, BTN_UP)    || key('W')) camY -= pan;
    if (btn(0, BTN_DOWN)  || key('S')) camY += pan;

    // left-drag to pan ("grab the map"). In setup mode only right of the panel, so
    // the sliders keep the left edge. Convert screen-px delta → world tiles via P.
    int pan_ok = (mode == 1) || (mouse_x() > PANEL_W);
    if (mouse_pressed(MOUSE_LEFT) && pan_ok) { dragging = 1; drag_px = mouse_x(); drag_py = mouse_y(); }
    if (!mouse_down(MOUSE_LEFT)) dragging = 0;
    if (dragging) {
        float pp = TILE * zoom;
        camX -= (mouse_x() - drag_px) / pp;
        camY -= (mouse_y() - drag_py) / pp;
        drag_px = mouse_x(); drag_py = mouse_y();
    }

    if (keyp(KEY_SPACE)) {                       // deterministic jump to fresh scenery
        jumpN += 1.0f;
        unsigned h = hash2((int)(jumpN * 911), 7);
        camX += (float)((int)(h % 4000u) - 2000) + 800.0f;
        camY += (float)((int)((h >> 11) % 4000u) - 2000) + 800.0f;
    }
    if (keyp('R')) { zoom = 1.0f; view_metrics(); camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f; seedZ += 0.37f; jumpN = 0; }
    if (keyp('G')) show_grid = !show_grid;
    if (keyp('H')) show_hud  = !show_hud;
    if (keyp('M')) mode = 0;                     // re-open the setup panel
}

void draw(void) {
    draw_world();                                // the live preview, always
    if (mode == 0) draw_setup_panel();
    else if (show_hud) hud();
}
