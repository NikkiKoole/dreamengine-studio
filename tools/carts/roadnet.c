#include "studio.h"
#include "ui.h"
#include <stdio.h>

// ============================================================================
// ROADNET  —  a SPLINE arterial network over an infinite heightmap.   (rungs 1-3)
//   DOCS: start at docs/design/roadnet-handoff.md (orientation + next steps);
//   design in roadnet.md, the L3 street level in roadnet-streetlevel.md.
//
//   intro panel      drag the SLIDERS (world re-rolls live behind the panel),
//                    ROLL = fresh seed, EXPLORE / ENTER = dismiss the panel
//   ◄▲▼►  /  WASD   pan across the (infinite) world
//   left-drag        grab + pan the world with the mouse
//   mouse wheel      zoom in / out (a bit), pivoting on the screen centre
//   SPACE           deterministic jump to fresh scenery
//   R               new seed (a fresh, repeatable world)
//   L               toggle the MAGNIFIER — an inset window into the street level below
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
// Rung 3 = RANK + ROAD CLASS: each node hashes to a rank (hamlet/town/city/metro),
// and a link's class is f(its endpoints' ranks) — motorway/highway/arterial/street/
// dirt — driving width, colour, and a dashed centre-line on the big roads; markers
// size by rank. SPACING: priority suppression (a stronger town/metro candidate within
// R suppresses/demotes its neighbours) → blue-noise distribution, still pure-hash and
// seam-true. VALLEY-BIAS: a clear link bows toward the lower side at its midpoint, so
// roads sag into valleys / hug coasts (reverts if it would hit water). (Landmarks = later.)
// ============================================================================

#define TILE 4                                  // screen px per world tile at zoom 1
#define ZMIN 0.12f                               // mousewheel zoom range — far out
#define ZMAX 12.0f                                // up from 2.5 → the main camera can now fly
                                                  // down to street level (the GRAPH/sloop view).
                                                  // (terrain is sampled per screen
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
// L2 (street-level, seen in the magnifier):
static float P_citysize   = 0.333f;  // city radius          → 0.5..2.0×
static float P_downtown   = 0.375f;  // downtown core size
static float P_farms      = 0.5f;    // farmland density
static float P_blocks     = 0.25f;   // city-block size (street spacing)
static float P_align      = 0.6f;    // districts aligned to the artery vs world-axis (0=all axis, 1=all aligned)
static float P_roadw      = 0.25f;   // arterial road width vs blocks (street level) → 0.35..1.5×
static float P_field      = 0.4f;    // farm field size (Voronoi cell pitch)

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
enum { VIEW_MAP, VIEW_GRAPH, VIEW_COUNT };   // TAB cycles these in explore
static int view = VIEW_MAP;          // MAP = painterly world; GRAPH = vector road-network debug

static float camX, camY;             // top-left of view, in world-tile coords
static float seedZ;                  // noise z-slice = the world's "seed"
static float jumpN;                  // SPACE counter → deterministic far jumps
static int   show_grid = 0;          // cell-border overlay (seam test)
static int   show_hud  = 1;
static int   show_loupe = 1;         // magnifier inset → the street level below
static float zoom = 1.0f;            // mousewheel zoom (ZMIN..ZMAX)
// Lens zoom levels (declared here so the street grid can use LOUPE2_ZOOM as the L3
// draw-gate; the matching LOUPE*_SZ pixel sizes live in the magnifier block below).
#define LOUPE_ZOOM  4.0f             // STREET lens — ~7 world tiles across (a district close-up)
#define LOUPE2_ZOOM 9.0f             // BLOCK  lens — ~3 world tiles across (lots/access streets)
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

// ── RANK + SPACING — every node gets a deterministic rank; rare high ranks PLUS
// priority suppression give blue-noise spacing (no two strong nodes adjacent). All
// pure functions of cell coords → seam-true. A road's CLASS is f(endpoint ranks). ──
enum { RK_HAMLET, RK_TOWN, RK_CITY, RK_METRO };
enum { CL_MOTORWAY, CL_HIGHWAY, CL_ARTERIAL, CL_STREET, CL_DIRT, CL_ACCESS };
// CL_ACCESS = the finest tier (L3 residential lanes carved by the street grid, never the
// arterial cache) — kept last so it stays > CL_ARTERIAL (no centre-line) without renumbering.

static int town_rank(int cx, int cy) {                   // feeder node: town / hamlet
    unsigned h = hash2(cx * 1900385059u + 61, cy * 2120969693u + 17);
    return (h % 100u < 35u) ? RK_TOWN : RK_HAMLET;
}
// a town CANDIDATE exists here (density gate only — no terrain, so suppression is cheap)
static int town_present(int cx, int cy) {
    unsigned h = hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    return (h % 100u) < (unsigned)town_pct();
}
// suppression priority: higher rank wins, hash breaks ties (cell folded in → no ties)
static unsigned town_prio(int cx, int cy) {
    unsigned h = hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    return ((unsigned)town_rank(cx, cy) << 28)
         | ((h ^ ((unsigned)cx * 2246822519u + (unsigned)cy * 3266489917u)) & 0x0fffffffu);
}

// METRO de-clustering: a metro candidate demotes to a city if a higher-priority metro
// candidate sits within R cells — so big cities never bunch up. Keeps the node (the
// backbone stays dense/connected); only the *rank* changes.
static int hub_rank(int cx, int cy) {
    unsigned h = hash2(cx * 915488749u + 31, cy * 147645773u + 7);
    if (h % 100u >= 15u) return RK_CITY;                 // not even a metro candidate
    for (int a = cx - 2; a <= cx + 2; a++)
        for (int b = cy - 2; b <= cy + 2; b++) {
            if (a == cx && b == cy) continue;
            unsigned g = hash2(a * 915488749u + 31, b * 147645773u + 7);
            if (g % 100u < 15u && g > h) return RK_CITY; // a stronger metro nearby → demote
        }
    return RK_METRO;
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

// ── POI NODES — one per cell, with PRIORITY SUPPRESSION for blue-noise spacing (a
// town drops if a higher-priority town candidate is adjacent — R=1 min distance).
// Pure function of (cx,cy); the suppression scan is hash-only (cheap), one height_at.
static int get_node(int cx, int cy, float *wx, float *wy) {
    if (!town_present(cx, cy)) return 0;                  // density slider
    unsigned p = town_prio(cx, cy);
    for (int a = cx - 1; a <= cx + 1; a++)               // min-distance spacing (R=1)
        for (int b = cy - 1; b <= cy + 1; b++) {
            if (a == cx && b == cy) continue;
            if (town_present(a, b) && town_prio(a, b) > p) return 0;   // a stronger neighbour wins
        }
    unsigned h = hash2(cx * 2654435761u + 17, cy * 40503u + 91);
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
#define VALLEY_D     6.0f           // valley-bias: how far sideways to probe (tiles)
#define VALLEY_K     40.0f          // ...how strongly height diff pulls the road
#define VALLEY_MAX   5.0f           // ...capped detour toward lower ground (tiles)
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

    // 1. base curve clear? then add a gentle VALLEY bias — bend toward the lower of the
    //    two sides at the midpoint, so roads sag into valleys / hug coasts instead of
    //    climbing. Pure fn of terrain → seam-true; reverts if the bias hits water/peak.
    build_curve(c0x, c0y, ax, ay, bx, by, c3x, c3y, 0, perpx, perpy);
    if (classify_curve(&peak, &maxw)) {
        float mx, my;
        catmull(c0x, ax, bx, c3x, 0.5f, &mx);
        catmull(c0y, ay, by, c3y, 0.5f, &my);
        float hp = height_at(mx + perpx*VALLEY_D, my + perpy*VALLEY_D);
        float hn = height_at(mx - perpx*VALLEY_D, my - perpy*VALLEY_D);
        float vb = (hn - hp) * VALLEY_K;                 // + = the +perp side is lower
        if (vb >  VALLEY_MAX) vb =  VALLEY_MAX;
        if (vb < -VALLEY_MAX) vb = -VALLEY_MAX;
        if (vb > 0.5f || vb < -0.5f) {
            build_curve(c0x, c0y, ax, ay, bx, by, c3x, c3y, vb, perpx, perpy);
            if (!classify_curve(&peak, &maxw))           // biased curve hit terrain → revert
                build_curve(c0x, c0y, ax, ay, bx, by, c3x, c3y, 0, perpx, perpy);
        }
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
    print(view == VIEW_GRAPH ? "GRAPH" : "MAP",            // current view (TAB cycles)
          SCREEN_W - 40, 2, view == VIEW_GRAPH ? CLR_ORANGE : CLR_MEDIUM_GREY);
    print_centered("drag/\x18\x19\x1a\x1b pan   wheel zoom   TAB view   SPACE jump   R seed   L loupe   M setup",
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
    for (int i = 0; i < 7; i++) ui_slider(pp[i], 4, 16 + i * 11, 88, L[i]);
    print("CITY (loupe)", 4, 92, CLR_MEDIUM_GREY);             // L2 knobs (street level)
    static const char *L2[7] = { "city size", "downtown", "farms", "blocks", "align", "road w", "field" };
    float *pp2[7] = { &P_citysize, &P_downtown, &P_farms, &P_blocks, &P_align, &P_roadw, &P_field };
    for (int i = 0; i < 7; i++) ui_slider(pp2[i], 4, 99 + i * 10, 88, L2[i]);
    int roll = ui_button(4,  172, 42, 13, "ROLL");
    int go   = ui_button(50, 172, 42, 13, "GO \x10");
    ui_end();

    snprintf(buf, sizeof buf, "seed #%u", (unsigned)(seedZ * 1000) % 100000u);
    print(buf, 4, 189, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    if (roll) seedZ += 0.37f;                    // fresh world, same params
    if (go || keyp(KEY_ENTER)) mode = 1;
}

// terrain — sampled per SCREEN cell, so cost is FIXED at any zoom (no per-tile buffer
// → no zoom-out ceiling). seed + water read live inside height_at.
static void render_terrain(void) {
    int step = 2;
    for (int sy = 0; sy < SCREEN_H; sy += step)
        for (int sx = 0; sx < SCREEN_W; sx += step)
            rectfill(sx, sy, step, step, biome_col(height_at(camX + sx / P, camY + sy / P)));
}

// the road network layer (grid overlay + highways + feeders + node markers)
static void render_roads(int detail) {
    if (show_grid) draw_grid_overlay();
    draw_highways(0); if (detail) draw_feeders(0);    // casings first...
    draw_highways(1); if (detail) draw_feeders(1);    // ...then centres
    draw_nodes(detail);
}

// ── STREET LEVEL — the "next level down", seen only through the magnifier. ─────────
// The L2 ZONE MODEL (RCI, anchored to roadnet's cities):
//   • URBANITY U(p) ∈ 0..1 — "how built-up". A field of bumps: U = max over nearby
//     city nodes of weight(rank)·falloff(dist/radius(rank)). Highest downtown, fading
//     to countryside at the edge. Metros = tall/wide bumps, hamlets = small ones.
//   • LAND USE from U, in concentric rings (real-city layout), jittered by noise:
//     wild(terrain) → FARM (cultivated ring) → RES (suburb) → COM (downtown core),
//     with INDUSTRIAL on the fringe / drawn to WATER (= HARBOR), and PARK pockets
//     carved out of any built-up zone. All pure fns of world coords → aligns with the
//     highways running through.
//   • BLOCK CONTENTS (done): built-up zones grow a nested STREET GRID (local + avenue
//     lattice, on_street/street_axis) that carves them into BLOCKS, and each block fills
//     with building LOTS (lot_color). Density emerges from U — downtown gets the full
//     local grid, the suburb only its avenues. (Farm fields / parks-as-content = next.)
enum { Z_FARM, Z_RES, Z_COM, Z_IND, Z_HARBOR, Z_PARK };
static const int ZONE_COL[6] = {
    CLR_YELLOW,      // FARM   — golden fields
    CLR_PEACH,       // RES    — warm rooftops
    CLR_LIGHT_GREY,  // COM    — downtown concrete/glass
    CLR_BROWN,       // IND    — factories
    CLR_DARK_GREY,   // HARBOR — docks on the water
    CLR_DARK_GREEN,  // PARK   — green space (the football field lives here, block stage)
};
#define U_FARM  0.12f                // U thresholds: below = wild countryside (terrain)
#define U_LIGHT 0.30f                // farm ring ends / suburb begins
#define U_MED   0.55f                // industrial-fringe ceiling

// L2 panel knobs (defaults reproduce the tuned look):
static float citysize_mul(void) { return 0.5f + P_citysize * 1.5f; }   // 0.5..2.0× radius
static float u_com(void)        { return 0.90f - P_downtown * 0.40f; } // downtown core size (low = big)
static float farm_frac(void)    { return 0.30f + P_farms * 0.55f; }    // fraction of fields cultivated (vs fallow)
static float field_sp(void)     { return 1.0f + P_field * 2.0f; }      // 1..3 world tiles per farm field
static float block_sp(void)     { return 0.7f + P_blocks * 1.6f; }     // 0.7..2.3 world tiles per city block
static float road_w_mul(void)   { return 0.35f + P_roadw * 1.15f; }    // 0.35..1.5× arterial carve width

static float rank_weight(int r) { return r==RK_METRO?1.0f : r==RK_CITY?0.78f : r==RK_TOWN?0.52f : 0.34f; }
static float rank_cityR(int r)  { float b = r==RK_METRO?22.f : r==RK_CITY?14.f : r==RK_TOWN?8.f : 4.f; return b * citysize_mul(); }

// nearby city nodes gathered once per loupe frame (so urbanity() is a short loop)
#define MAXCITY 160
static float gnx[MAXCITY], gny[MAXCITY], gnw[MAXCITY], gnr[MAXCITY]; static int gn;
static float gux[MAXCITY], guy[MAXCITY];   // each city's primary axis (unit dir of the road that enters it)
static unsigned gid[MAXCITY];              // STABLE per-city id (hash of its origin cell) — NOT the
                                           // array index, which renumbers as cities scroll in/out of
                                           // gather_cities(). District orientation must key off this so
                                           // the grid is a pure fn of world pos (else it re-rolls on pan).
static int   dom_i = -1;                   // index of the city that dominates the last urbanity() query

// The road that ENTERS a city → the axis its street grid aligns to. Town: toward its
// nearest hub (the feeder). Hub: toward its nearest neighbour hub (the highway). Pure
// fn of position → deterministic + seam-true. Returns a unit vector (defaults to +x).
static void city_dir(float wx, float wy, int is_hub, float *ux, float *uy) {
    float bx = wx + 1, by = wy, best = 1e18f; int found = 0;
    if (is_hub) {                                        // nearest OTHER hub (self excluded)
        int hc = ifloor(wx/HUB_CS), hr = ifloor(wy/HUB_CS);
        for (int a = hc-2; a <= hc+2; a++) for (int b = hr-2; b <= hr+2; b++) {
            float ox, oy; if (!get_hub(a, b, &ox, &oy)) continue;
            if (ox == wx && oy == wy) continue;
            float dx = ox-wx, dy = oy-wy, dd = dx*dx + dy*dy;
            if (dd < best) { best = dd; bx = ox; by = oy; found = 1; }
        }
    } else {
        found = nearest_hub(wx, wy, &bx, &by);           // the feeder road
    }
    float dx = bx - wx, dy = by - wy, L = fsqrt(dx*dx + dy*dy);
    if (!found || L < 1e-4f) { *ux = 1; *uy = 0; return; }
    *ux = dx/L; *uy = dy/L;
}
static void gather_cities(void) {
    gn = 0;
    int m = 6;                                       // margin in cells to catch big bumps
    int hc0 = ifloor(camX/HUB_CS)-m, hc1 = ifloor((camX+vcols)/HUB_CS)+m;
    int hr0 = ifloor(camY/HUB_CS)-m, hr1 = ifloor((camY+vrows)/HUB_CS)+m;
    for (int cx=hc0; cx<=hc1 && gn<MAXCITY; cx++) for (int cy=hr0; cy<=hr1 && gn<MAXCITY; cy++) {
        float wx,wy; if (!get_hub(cx,cy,&wx,&wy)) continue;
        gnx[gn]=wx; gny[gn]=wy; gnw[gn]=rank_weight(hub_rank(cx,cy)); gnr[gn]=rank_cityR(hub_rank(cx,cy));
        gid[gn]=hash2(cx*131+1, cy*131+1);          // stable id from the hub's cell (salt 1)
        city_dir(wx,wy,1,&gux[gn],&guy[gn]); gn++;
    }
    int tc0 = ifloor(camX/NODE_CS)-m, tc1 = ifloor((camX+vcols)/NODE_CS)+m;
    int tr0 = ifloor(camY/NODE_CS)-m, tr1 = ifloor((camY+vrows)/NODE_CS)+m;
    for (int cx=tc0; cx<=tc1 && gn<MAXCITY; cx++) for (int cy=tr0; cy<=tr1 && gn<MAXCITY; cy++) {
        float wx,wy; if (!get_node(cx,cy,&wx,&wy)) continue;
        gnx[gn]=wx; gny[gn]=wy; gnw[gn]=rank_weight(town_rank(cx,cy)); gnr[gn]=rank_cityR(town_rank(cx,cy));
        gid[gn]=hash2(cx*131+2, cy*131+2);          // stable id from the town's cell (salt 2)
        city_dir(wx,wy,0,&gux[gn],&guy[gn]); gn++;
    }
}
static float urbanity(float wx, float wy) {          // 0..1, max bump over nearby cities
    // DOMAIN WARP — perturb the query point so the bumps (and every zone band) come out
    // as organic blobs, not perfect concentric circles.
    float ax = wx + (noise2(wx*0.045f + 11, wy*0.045f + 11) - 0.5f) * 16.0f;
    float ay = wy + (noise2(wx*0.045f + 37, wy*0.045f + 37) - 0.5f) * 16.0f;
    float u = 0; dom_i = -1;
    for (int i=0; i<gn; i++) {
        float dx=ax-gnx[i], dy=ay-gny[i], d=fsqrt(dx*dx+dy*dy)/gnr[i];
        if (d < 1.0f) { float f=(1.0f-d); f=f*f*gnw[i]; if (f>u) { u=f; dom_i=i; } }
    }
    return u;
}
static int water_near(float wx, float wy) {          // any water within ~2 tiles (harbor test)
    return height_at(wx+2,wy)<0 || height_at(wx-2,wy)<0 || height_at(wx,wy+2)<0 || height_at(wx,wy-2)<0;
}
#define Z_NONE (-1)                  // "leave the terrain showing" (countryside gap)
static int zone_of(float wx, float wy, float U) {    // assumes U >= U_FARM (built/cultivated)
    if (U >= U_LIGHT && noise2(wx*0.5f+seedZ*3, wy*0.5f+seedZ*3) > 0.80f) return Z_PARK;
    if (noise2(wx*0.12f+seedZ*5, wy*0.12f+seedZ*5) > 0.62f) {     // industrial noise
        int nw = water_near(wx, wy);
        if (U < U_MED || nw) return nw ? Z_HARBOR : Z_IND;        // fringe industry / harbour
    }
    if (U < U_LIGHT) return Z_FARM;                              // transition band — occupancy + Voronoi
                                                                 // fields decide its content downstream
    float j = (noise2(wx*0.08f-seedZ, wy*0.08f-seedZ) - 0.5f) * 0.18f;   // jitter the core edge
    return (U + j >= u_com()) ? Z_COM : Z_RES;
}
// ── L2 STREET GRID — a nested local lattice that carves a built-up zone into BLOCKS.
// Line k sits at k·block_sp (world tiles); class + visibility are pure functions of k
// and the local urbanity, so it's seam-true and the future road_at() reads the same
// geometry. Block DENSITY falls out of U: downtown gets the full local grid, the suburb
// only its avenues — block size emerges, it isn't a flag.
// Tier order = priority (ACCESS narrowest .. AVENUE widest); ST_NONE=0 < ST_ACCESS.
enum { ST_NONE, ST_ACCESS, ST_LOCAL, ST_AVENUE };
// want_access separates GENERATION from the DRAW gate: callers set it to admit the L3 access
// tier. The renderer sets it from zoom (a draw gate — access only at L3 detail); the road
// GRAPH extractor sets it 1 always (the access lanes EXIST regardless of view zoom — the
// car must route on them). Default 0, so a bare road_at() is conservative.
static int want_access = 0;
static int street_axis(float coord, float U, float *adist) {
    float sp = block_sp();
    int k = ifloor(coord / sp + 0.5f);                    // nearest line index
    float d = coord - k * sp; *adist = d < 0 ? -d : d;
    if (k % 4 == 0) return (U >= U_LIGHT) ? ST_AVENUE : ST_NONE;   // the grid's bigger streets
    if (U >= U_MED) return ST_LOCAL;                               // locals only in the dense core
    // L3 ACCESS tier — where locals are suppressed (the suburb) the blocks are huge and
    // interior lots have no frontage. Bisect them with half-pitch residential lanes so every
    // lot fronts a street. Gated on want_access (set from zoom for the render, =1 for the graph).
    if (want_access && U >= U_LIGHT) {
        float sp2 = sp * 0.5f;
        int k2 = ifloor(coord / sp2 + 0.5f);
        float d2 = coord - k2 * sp2; if (d2 < 0) d2 = -d2;
        if (k2 & 1) { *adist = d2; return ST_ACCESS; }    // odd half-line = mid-block access lane
    }
    return ST_NONE;
}
enum { GR_NONE, GR_ROAD, GR_WALK, GR_ACCESS };            // a built-up tile: lot / street / sidewalk / access lane
static int grid_at(float gx, float gy, float U) {
    float dv, dh;
    int cv = street_axis(gx, U, &dv), ch = street_axis(gy, U, &dh);
    float hwv = (cv == ST_AVENUE) ? 0.22f : (cv == ST_ACCESS) ? 0.09f : 0.13f;  // half-width (tiles)
    float hwh = (ch == ST_AVENUE) ? 0.22f : (ch == ST_ACCESS) ? 0.09f : 0.13f;
    float sw = 0.07f;                                     // sidewalk band just outside the kerb
    int rv = (cv != ST_NONE && dv < hwv);                 // on the vertical street?
    int rh = (ch != ST_NONE && dh < hwh);                 // on the horizontal street?
    if (rv || rh) {                                       // any local/avenue here = STREET; access-only = ACCESS
        int wide = (rv && cv > ST_ACCESS) || (rh && ch > ST_ACCESS);
        return wide ? GR_ROAD : GR_ACCESS;
    }
    // sidewalks border the wider streets only (access lanes are bare residential frontage)
    if ((cv > ST_ACCESS && dv < hwv+sw) || (ch > ST_ACCESS && dh < hwh+sw)) return GR_WALK;
    return GR_NONE;
}
// roof / ground colour for one building lot inside a block (sub-block hash grid)
static int lot_color(float wx, float wy, int z) {
    float lp = block_sp() * 0.5f;                         // ~4 lots per block
    unsigned h = hash2(ifloor(wx/lp) + (int)(seedZ*131.0f), ifloor(wy/lp)*7 + (int)(seedZ*57.0f));
    if (z == Z_RES) {
        static const int roof[5] = { CLR_BROWN, CLR_RED, CLR_DARK_RED, CLR_DARK_PURPLE, CLR_PEACH };
        return ((h & 7u) == 0) ? CLR_DARK_GREEN : roof[h % 5u];    // 1-in-8 lot = yard
    }
    if (z == Z_COM) {
        static const int roof[4] = { CLR_LIGHT_GREY, CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_INDIGO };
        return roof[h % 4u];
    }
    static const int body[3] = { CLR_DARKER_GREY, CLR_DARK_BROWN, CLR_DARKER_PURPLE };  // IND / HARBOR
    return body[h % 3u];
}
// ── DISTRICT ORIENTATION — a city is a PATCHWORK, not one monolithic grid. A fraction
// P_align of a city's districts rotate into its artery frame (main street follows the
// road in); the rest grid to the world axes ("bigger roads connect however"). Both
// frames anchored at the node, so it stays one coherent city. Keyed on gid[] (the STABLE
// per-city id), NOT dom_i — so the orientation is a pure fn of world pos and doesn't
// re-roll as cities renumber in gather_cities() (the drag-flip bug).
#define DISTRICT_BLK 5               // district size in blocks (orientation constant within one)
static void city_grid_coords(float wx, float wy, float *gx, float *gy) {
    float ex = wx - gnx[dom_i], ey = wy - gny[dom_i];        // node-local, world-axis frame
    float dsp = block_sp() * DISTRICT_BLK;
    unsigned g = gid[dom_i];
    unsigned hd = hash2(ifloor(ex/dsp) + (int)(g & 0xffffu), ifloor(ey/dsp)*7 - (int)((g>>8) & 0xffffu));
    if ((hd % 1000u) < (unsigned)(P_align * 1000.0f)) {      // this district aligns to the artery
        float ux = gux[dom_i], uy = guy[dom_i];
        *gx =  ex*ux + ey*uy;                                // along the entering road
        *gy = -ex*uy + ey*ux;                                // perpendicular
    } else {                                                 // ...the rest grid to the world axes
        *gx = ex; *gy = ey;
    }
}

// ── ARTERIAL POINT-QUERY — "is this world point on a roadnet road, and which class?"
// The renderer STROKES arterials in screen px; collision needs the same roads in WORLD
// space. So once per frame we gather the visible links' polylines (the SAME link_path()
// geometry the renderer draws → agree by construction) into a segment cache, then test
// point-to-segment distance against a per-class WORLD half-width. This is the seam the
// drop-in world exposes: render_streetlevel + the future sloop road_at() both read it.
//
// NB: gather_arterials() MIRRORS the enumeration in draw_highways()/draw_feeders() (which
// hubs/towns link to which). The GEOMETRY is shared (link_path); only the *enumeration*
// is duplicated — keep the two in sync if the linking rules ever change (rungs 1-3 frozen).
#define MAXSEG 4096
static float sgx0[MAXSEG], sgy0[MAXSEG], sgx1[MAXSEG], sgy1[MAXSEG];
static int   sgcls[MAXSEG]; static int nseg;
static float road_hw(int cls) {              // arterial road HALF-width (world tiles), scaled live
    float w;
    switch (cls) {
        case CL_MOTORWAY: w = 0.75f; break;
        case CL_HIGHWAY:  w = 0.60f; break;
        case CL_ARTERIAL: w = 0.45f; break;
        case CL_STREET:   w = 0.32f; break;
        default:          w = 0.22f; break;  // dirt
    }
    return w * road_w_mul();
}
static void push_link(int np, int cls) {
    for (int i = 0; i + 1 < np && nseg < MAXSEG; i++) {
        sgx0[nseg]=lp_x[i]; sgy0[nseg]=lp_y[i]; sgx1[nseg]=lp_x[i+1]; sgy1[nseg]=lp_y[i+1];
        sgcls[nseg]=cls; nseg++;
    }
}
static void gather_arterials(void) {
    nseg = 0;
    int c0 = ifloor(camX/HUB_CS)-2, c1 = ifloor((camX+vcols)/HUB_CS)+2;    // mirror draw_highways
    int r0 = ifloor(camY/HUB_CS)-2, r1 = ifloor((camY+vrows)/HUB_CS)+2;
    for (int cx=c0; cx<=c1; cx++) for (int cy=r0; cy<=r1; cy++) {
        float p1x,p1y; if (!get_hub(cx,cy,&p1x,&p1y)) continue;
        for (int d=0; d<4; d++) {
            int dx=DIR[d][0], dy=DIR[d][1];
            if (d>=2) { int qx=cx, qy=(dy==1)?cy:cy-1;
                unsigned qh=hash2(qx*2246822519u+7, qy*3266489917u+13);
                if (qh%100u >= (unsigned)diag_pct()) continue;
                if ((dy==1) != (int)((qh>>9)&1)) continue; }
            float p2x,p2y; if (!get_hub(cx+dx,cy+dy,&p2x,&p2y)) continue;
            float p0x,p0y,p3x,p3y;
            if (!get_hub(cx-dx,cy-dy,&p0x,&p0y)) { p0x=2*p1x-p2x; p0y=2*p1y-p2y; }
            if (!get_hub(cx+2*dx,cy+2*dy,&p3x,&p3y)) { p3x=2*p2x-p1x; p3y=2*p2y-p1y; }
            int cls=(hub_rank(cx,cy)==RK_METRO||hub_rank(cx+dx,cy+dy)==RK_METRO)?CL_MOTORWAY:CL_HIGHWAY;
            int np=link_path(p0x,p0y,p1x,p1y,p2x,p2y,p3x,p3y);
            if (np) push_link(np, cls);
        }
    }
    int m=5;                                                               // mirror draw_feeders
    int fc0=ifloor(camX/NODE_CS)-m, fc1=ifloor((camX+vcols)/NODE_CS)+m;
    int fr0=ifloor(camY/NODE_CS)-m, fr1=ifloor((camY+vrows)/NODE_CS)+m;
    for (int cx=fc0; cx<=fc1; cx++) for (int cy=fr0; cy<=fr1; cy++) {
        float tx,ty; if (!get_node(cx,cy,&tx,&ty)) continue;
        int tr=town_rank(cx,cy);
        float hx,hy;
        if (nearest_hub(tx,ty,&hx,&hy)) {
            int cls=(tr==RK_TOWN)?CL_ARTERIAL:CL_STREET;
            int np=link_path(2*tx-hx,2*ty-hy,tx,ty,hx,hy,2*hx-tx,2*hy-ty);
            if (np) push_link(np, cls);
        }
        float nx,ny;
        if (nearest_town(tx,ty,&nx,&ny)) {
            int nr=town_rank(ifloor(nx/NODE_CS),ifloor(ny/NODE_CS));
            int cls=(tr==RK_TOWN||nr==RK_TOWN)?CL_STREET:CL_DIRT;
            int np=link_path(2*tx-nx,2*ty-ny,tx,ty,nx,ny,2*nx-tx,2*ny-ty);
            if (np) push_link(np, cls);
        }
    }
}
static float seg_dist2(float px,float py, float x0,float y0,float x1,float y1) {
    float vx=x1-x0, vy=y1-y0, wx=px-x0, wy=py-y0;
    float c1=vx*wx+vy*wy; if (c1<=0) return wx*wx+wy*wy;
    float c2=vx*vx+vy*vy; if (c2<=c1) { float ex=px-x1,ey=py-y1; return ex*ex+ey*ey; }
    float t=c1/c2, ex=px-(x0+t*vx), ey=py-(y0+t*vy); return ex*ex+ey*ey;
}
// nearest road over the cache; returns 1 + the WIDEST (lowest-idx) class we're inside,
// and *outoff = distance to that road's centre-line (tiles) for street-scale markings.
static int arterial_at(float wx, float wy, int *outcls, float *outoff) {
    int best = 99; float bestd2 = 1e18f;
    for (int i=0; i<nseg; i++) {
        if (sgcls[i] > best) continue;                           // already have a bigger road here
        float hw = road_hw(sgcls[i]);
        float d2 = seg_dist2(wx,wy, sgx0[i],sgy0[i],sgx1[i],sgy1[i]);
        if (d2 < hw*hw) {
            if (sgcls[i] < best) { best = sgcls[i]; bestd2 = d2; }   // wider road wins
            else if (d2 < bestd2) bestd2 = d2;                       // same class, nearer centre-line
        }
    }
    if (best < 99) { *outcls = best; *outoff = fsqrt(bestd2); return 1; }
    return 0;
}

// distance (tiles) to the nearest cached arterial — for ribbon development (houses string
// along the roads out of town). Uncapped; the occupancy ramp clamps it.
static float road_dist(float wx, float wy) {
    float best = 1e18f;
    for (int i=0; i<nseg; i++) {
        float d2 = seg_dist2(wx,wy, sgx0[i],sgy0[i],sgx1[i],sgy1[i]);
        if (d2 < best) best = d2;
    }
    return fsqrt(best);
}

// ── THE OUTSKIRTS GRADIENT — the concentric core (COM/IND, high U) stays fully built;
// below U_CORE, lot OCCUPANCY ramps down so the suburb THINS toward the edge, with a
// RIBBON boost near roads (houses follow the arterials out of town). Unbuilt land becomes
// a Voronoi FIELD. So city → thinning houses → fields + ribbon → countryside, no blob edge.
#define U_CORE       0.55f           // at/above this U the suburb is fully built
#define RIBBON_REACH 3.0f            // tiles; houses string this far either side of a road
static float occupancy(float U, float rd) {
    float base = (U - U_FARM) / (U_CORE - U_FARM);   // 0 at the farm edge .. 1 at the dense core
    if (base < 0) base = 0; if (base > 1) base = 1;
    float ribbon = 1.0f - rd / RIBBON_REACH; if (ribbon < 0) ribbon = 0;
    float occ = base + (1.0f - base) * ribbon * 0.9f;   // ribbon fills lots in along the roads
    return occ > 1 ? 1 : occ;
}
static int lot_built(float gx, float gy, float occ) {    // is this lot developed? (per-lot hash vs occ)
    float lp = block_sp() * 0.5f;
    unsigned h = hash2(ifloor(gx/lp)*5 + 1, ifloor(gy/lp)*7 - 3);
    return (h % 1000u) < (unsigned)(occ * 1000.0f);
}

// ── VORONOI FARM FIELDS — a jittered-grid (Worley) partition into discrete parcels: each
// cell hashes to one seed; the nearest seed's cell is the FIELD (its hash → crop), and where
// the two nearest seeds are ~equidistant we're on a HEDGEROW. Local + pure fn → seam-true.
static unsigned field_at(float wx, float wy, int *edge) {
    float fp = field_sp();
    int cx = ifloor(wx/fp), cy = ifloor(wy/fp);
    float best = 1e18f, best2 = 1e18f; unsigned bid = 0;
    for (int a=cx-1; a<=cx+1; a++) for (int b=cy-1; b<=cy+1; b++) {
        unsigned h = hash2(a + (int)(seedZ*71.0f), b*3 - (int)(seedZ*29.0f));
        float sx = (a + (h & 255u)/255.0f) * fp;             // jittered seed inside the cell
        float sy = (b + ((h>>8) & 255u)/255.0f) * fp;
        float dx = wx-sx, dy = wy-sy, d = dx*dx + dy*dy;
        if (d < best) { best2 = best; best = d; bid = hash2(a*131+7, b*197+3); }
        else if (d < best2) best2 = d;
    }
    *edge = (fsqrt(best2) - fsqrt(best)) < fp*0.10f;         // near-equidistant → hedgerow/track
    return bid;
}
static int field_color(unsigned fid, int edge) {
    if (edge) return CLR_DARK_GREEN;                          // hedgerow / treeline between fields
    if ((fid % 100u) >= (unsigned)(farm_frac()*100.0f)) return CLR_MEDIUM_GREEN;   // fallow meadow
    static const int crop[4] = { CLR_YELLOW, CLR_LIME_GREEN, CLR_BROWN, CLR_MEDIUM_GREEN };
    return crop[fid % 4u];
}

// ── THE ROAD GRAPH (the background data layer the car sim will navigate) ─────────────
// road_at()/arterial_at() answer "is this point on tarmac" — a FIELD, enough for keeping a
// car on the surface + building collision. The car ALSO wants the network as STRUCTURE:
// NODES (intersections) + EDGES (segments with class + endpoint node ids = adjacency), to
// snap to a road, route A→B, and spawn traffic that follows lanes. The graph is built from
// the SAME geometry the field reads (graph == field == screen, by construction):
//   • ARTERIALS — the link_path segment cache, exact (no nodes yet → n0=n1=-1).
//   • GRID + ACCESS streets — vectorised by graph_add_grid() (below road_at), per city /
//     per district / in the district's own (rotated) frame, every candidate validated
//     against road_at() so graph ⊆ field. Edges carry n0/n1 → routable adjacency.
// Still ⛔: stitching the grid graph onto the arterial backbone at city entries, and
// collapsing degree-2 chains (the grid is finely noded). See roadnet-streetlevel.md.
typedef struct { float x0,y0,x1,y1; int cls; int n0,n1; } RoadEdge;  // n0,n1 = node ids (-1 = none)
typedef struct { float x,y; } RoadNode;
#define MAXGEDGE 12000
#define MAXGNODE 6000
static RoadEdge gedge[MAXGEDGE]; static int nedge;
static RoadNode gnode[MAXGNODE]; static int nnode;
static int g_deg[MAXGNODE];          // grid-node degree (incident grid edges) — filled by the collapse;
static int g_inc[MAXGNODE][4];       // ...also its incident edge indices (≤4: ±gx, ±gy). Read by the
                                     // node-draw (only deg≠2 nodes are real junctions/dead-ends).
static void push_gedge(float x0,float y0,float x1,float y1,int cls,int n0,int n1) {
    if (nedge >= MAXGEDGE) return;
    gedge[nedge].x0=x0; gedge[nedge].y0=y0; gedge[nedge].x1=x1; gedge[nedge].y1=y1;
    gedge[nedge].cls=cls; gedge[nedge].n0=n0; gedge[nedge].n1=n1; nedge++;
}
static void build_graph(void) {        // call AFTER gather_arterials() (it fills the sg* cache)
    nedge = 0; nnode = 0;
    for (int i=0; i<nseg && nedge<MAXGEDGE; i++)
        push_gedge(sgx0[i],sgy0[i],sgx1[i],sgy1[i], sgcls[i], -1,-1);   // arterials: no nodes yet
}

// THE WORLD QUERY SEAM (for the drop-in consumer, e.g. sloop). Assumes gather_cities() +
// gather_arterials() have run for a region covering (wx,wy) this frame (the renderer does
// this). on_road = drivable surface; cls = its class; zone = land use; built = a building
// stands here (vs open field); coff = arterial centre-line distance (-1 if not on one).
typedef struct { int on_road, cls, zone, built; float urb, coff; } RoadHit;
static RoadHit road_at(float wx, float wy) {
    RoadHit r = { 0, CL_STREET, Z_NONE, 0, 0, -1 };
    r.urb = urbanity(wx, wy);                                    // sets dom_i
    if (!passable(height_at(wx, wy))) return r;                  // water/peak
    int acls; float aoff;
    if (arterial_at(wx, wy, &acls, &aoff)) { r.on_road = 1; r.cls = acls; r.coff = aoff; }
    if (r.urb < U_FARM) return r;                                // wild countryside
    r.zone = zone_of(wx, wy, r.urb);
    // local street grid (built-up zones only)
    if (!r.on_road && (r.zone==Z_RES||r.zone==Z_COM||r.zone==Z_IND||r.zone==Z_HARBOR)) {
        float gx, gy; city_grid_coords(wx, wy, &gx, &gy);
        int g = grid_at(gx, gy, r.urb);
        if      (g == GR_ROAD)   { r.on_road = 1; r.cls = CL_STREET; }
        else if (g == GR_ACCESS) { r.on_road = 1; r.cls = CL_ACCESS; }   // L3 residential lane
    }
    if (r.on_road) return r;
    // built vs open: COM/IND are the dense core (always built); RES/FARM thin via occupancy
    if (r.zone==Z_COM || r.zone==Z_IND || r.zone==Z_HARBOR) r.built = 1;
    else if (r.zone==Z_RES || r.zone==Z_FARM) {
        float gx, gy; city_grid_coords(wx, wy, &gx, &gy);
        r.built = lot_built(gx, gy, occupancy(r.urb, road_dist(wx, wy)));
    }
    return r;
}

// is this world point on a GRID/ACCESS street (not an arterial, not open lot)? → its class.
static int is_grid_road(float wx, float wy, int *cls) {
    RoadHit h = road_at(wx, wy);
    if (h.on_road && (h.cls == CL_STREET || h.cls == CL_ACCESS)) { *cls = h.cls; return 1; }
    return 0;
}
// ── DEGREE-2 COLLAPSE — the grid is finely noded (a node at every half-block crossing), so a
// straight lane is a chain of degree-2 nodes + many tiny edges. Walk each chain from a real
// junction (degree ≠ 2) through colinear degree-2 nodes and emit ONE merged edge; arterials
// (n0<0) pass through untouched. Leaves only junctions/dead-ends/corners as nodes and turns
// straight runs into single clean edges. Cleaner to look at AND a tidier graph for the car sim.
static int collapse_other(int e, int cur) { return gedge[e].n0==cur ? gedge[e].n1 : gedge[e].n0; }
static void graph_collapse_grid(void) {
    for (int i=0;i<nnode;i++) g_deg[i]=0;
    for (int e=0;e<nedge;e++) {                          // degree + incidence over GRID edges only
        if (gedge[e].n0 < 0) continue;                   // arterial
        int a=gedge[e].n0, b=gedge[e].n1;
        if (g_deg[a]<4) g_inc[a][g_deg[a]]=e;  g_deg[a]++;
        if (g_deg[b]<4) g_inc[b][g_deg[b]]=e;  g_deg[b]++;
    }
    static int used[MAXGEDGE]; for (int e=0;e<nedge;e++) used[e]=0;
    static RoadEdge out[MAXGEDGE]; int nout=0;
    for (int e=0;e<nedge && nout<MAXGEDGE;e++)            // arterials verbatim
        if (gedge[e].n0 < 0) out[nout++]=gedge[e];
    for (int j=0;j<nnode;j++) {
        if (g_deg[j]==2) continue;                       // chains start at junctions / dead-ends only
        int nd = g_deg[j]<4 ? g_deg[j] : 4;
        for (int d=0; d<nd; d++) {
            int e=g_inc[j][d]; if (used[e]) continue;
            int cur=j, cls=gedge[e].cls, k;
            float sx=gnode[j].x, sy=gnode[j].y;
            for (;;) {
                used[e]=1; k=collapse_other(e,cur);
                if (g_deg[k]!=2) break;                   // reached another junction → stop
                int e2=-1, knd=g_deg[k]<4?g_deg[k]:4;     // the other edge at this degree-2 node
                for (int t=0;t<knd;t++){ int ee=g_inc[k][t]; if (ee!=e && !used[ee]){ e2=ee; break; } }
                if (e2<0) break;
                int nx=collapse_other(e2,k);
                float d1x=gnode[k].x-gnode[cur].x, d1y=gnode[k].y-gnode[cur].y;
                float d2x=gnode[nx].x-gnode[k].x,  d2y=gnode[nx].y-gnode[k].y;
                float cross=d1x*d2y-d1y*d2x, dot=d1x*d2x+d1y*d2y;
                if (dot<=0 || cross*cross > 1e-4f*(d1x*d1x+d1y*d1y)*(d2x*d2x+d2y*d2y)) break;  // corner
                cur=k; e=e2;                              // colinear → keep walking
            }
            if (nout<MAXGEDGE) {
                out[nout].x0=sx; out[nout].y0=sy; out[nout].x1=gnode[k].x; out[nout].y1=gnode[k].y;
                out[nout].cls=cls; out[nout].n0=j; out[nout].n1=k; nout++;
            }
        }
    }
    for (int e=0;e<nedge && nout<MAXGEDGE;e++)            // leftover degree-2 loops (no junction) — keep
        if (gedge[e].n0>=0 && !used[e]) { out[nout++]=gedge[e]; used[e]=1; }
    nedge=nout;
    for (int i=0;i<nedge;i++) gedge[i]=out[i];
}
// ── VECTORISE THE INTRA-CITY GRID into gedge[]/gnode[] over the VISIBLE region (LOD: street
// zoom only; far out the grid is sub-pixel and meaningless). The grid is rotated PER DISTRICT
// (city_grid_coords), so we can't lay one global lattice — we go per gathered city, per
// district, in that district's own frame. At each grid-line crossing we place a NODE if it
// validates as road; adjacent crossings joined by a road span become an EDGE carrying both
// node ids (= adjacency). road_at() validates every candidate, so the graph is a strict
// subset of the field AND the per-district frame auto-clips at boundaries (a candidate that
// strays into a neighbour district's territory simply fails to validate). want_access=1 so
// the access tier is part of the graph regardless of the view zoom.
#define GRID_DI 24                       // max grid-line indices per district axis (incl. access half-pitch)
static void graph_add_grid(void) {
    float sp = block_sp(), dsp = sp * DISTRICT_BLK, hp = sp * 0.5f;   // hp = access half-pitch
    int saved = want_access; want_access = 1;
    float wx0 = camX, wy0 = camY, wx1 = camX + vcols, wy1 = camY + vrows;
    for (int ci = 0; ci < gn && nedge < MAXGEDGE; ci++) {
        float nx = gnx[ci], ny = gny[ci], R = gnr[ci] * 1.4f;
        if (nx + R < wx0 || nx - R > wx1 || ny + R < wy0 || ny - R > wy1) continue;  // can't reach view
        int di0 = ifloor((wx0 - nx)/dsp), di1 = ifloor((wx1 - nx)/dsp);              // districts over view
        int dj0 = ifloor((wy0 - ny)/dsp), dj1 = ifloor((wy1 - ny)/dsp);
        for (int di = di0; di <= di1 && nedge < MAXGEDGE; di++)
        for (int dj = dj0; dj <= dj1 && nedge < MAXGEDGE; dj++) {
            unsigned g = gid[ci];                                                    // == city_grid_coords
            unsigned hd = hash2(di + (int)(g & 0xffffu), dj*7 - (int)((g>>8) & 0xffffu));
            int aligned = ((hd % 1000u) < (unsigned)(P_align * 1000.0f));
            float ux = aligned ? gux[ci] : 1.0f, uy = aligned ? guy[ci] : 0.0f;
            float ex0 = di*dsp, ex1 = ex0 + dsp, ey0 = dj*dsp, ey1 = ey0 + dsp;      // node-local box
            float cxx[4] = { ex0,ex1,ex0,ex1 }, cyy[4] = { ey0,ey0,ey1,ey1 };        // grid coords at corners
            float gxmin=1e9f,gxmax=-1e9f,gymin=1e9f,gymax=-1e9f;
            for (int c=0;c<4;c++) {
                float gx =  cxx[c]*ux + cyy[c]*uy, gy = -cxx[c]*uy + cyy[c]*ux;
                if (gx<gxmin)gxmin=gx; if (gx>gxmax)gxmax=gx;
                if (gy<gymin)gymin=gy; if (gy>gymax)gymax=gy;
            }
            int ka = ifloor(gxmin/hp), ma = ifloor(gymin/hp);
            int nk = ifloor(gxmax/hp) - ka + 1, nm = ifloor(gymax/hp) - ma + 1;
            if (nk > GRID_DI) nk = GRID_DI; if (nm > GRID_DI) nm = GRID_DI;
            static int nid[GRID_DI][GRID_DI];
            // pass 1: node at each (k,m) crossing inside the box that validates as grid road
            for (int a=0;a<nk;a++) for (int b=0;b<nm;b++) {
                nid[a][b] = -1;
                float gx = (ka+a)*hp, gy = (ma+b)*hp;
                float ex = gx*ux - gy*uy, ey = gx*uy + gy*ux;        // inverse rotate → node-local
                if (ex<ex0||ex>ex1||ey<ey0||ey>ey1) continue;        // belongs to a neighbour district
                float wx = nx+ex, wy = ny+ey; int cls;
                if (!is_grid_road(wx, wy, &cls)) continue;
                if (nnode < MAXGNODE) { gnode[nnode].x=wx; gnode[nnode].y=wy; nid[a][b]=nnode++; }
            }
            // pass 2: join adjacent nodes whose connecting span is road → edge (with adjacency)
            for (int a=0;a<nk;a++) for (int b=0;b<nm && nedge<MAXGEDGE;b++) {
                if (nid[a][b] < 0) continue;
                float x0 = gnode[nid[a][b]].x, y0 = gnode[nid[a][b]].y;
                if (a+1<nk && nid[a+1][b] >= 0) {                    // neighbour along +gx
                    float x1 = gnode[nid[a+1][b]].x, y1 = gnode[nid[a+1][b]].y; int cm;
                    if (is_grid_road((x0+x1)*0.5f,(y0+y1)*0.5f,&cm)) push_gedge(x0,y0,x1,y1,cm,nid[a][b],nid[a+1][b]);
                }
                if (b+1<nm && nid[a][b+1] >= 0) {                    // neighbour along +gy
                    float x1 = gnode[nid[a][b+1]].x, y1 = gnode[nid[a][b+1]].y; int cm;
                    if (is_grid_road((x0+x1)*0.5f,(y0+y1)*0.5f,&cm)) push_gedge(x0,y0,x1,y1,cm,nid[a][b],nid[a][b+1]);
                }
            }
        }
    }
    want_access = saved;
    graph_collapse_grid();                                       // merge straight runs → clean edges
}
static void render_streetlevel(int bx, int by, int sz) {        // bounds = the loupe box (cheap)
    gather_cities();
    gather_arterials();                                          // the road point-query cache
    want_access = (zoom >= LOUPE2_ZOOM);                         // access tier = a DRAW gate here
    int step = 2;
    int x0 = bx - (bx % step), y0 = by - (by % step);
    for (int sy=y0; sy<by+sz; sy+=step)
        for (int sx=x0; sx<bx+sz; sx+=step) {
            float wx = camX + sx/P, wy = camY + sy/P;
            RoadHit h = road_at(wx, wy);                          // SAME query the consumer calls
            int col;
            if (h.on_road) {                                     // class-aware road surface
                col = (h.cls == CL_DIRT)   ? CLR_BROWN          // unpaved inter-town road
                    : (h.cls == CL_ACCESS) ? CLR_DARKER_GREY    // narrow residential lane (L3)
                    :                        CLR_DARK_GREY;     // paved street/arterial
                if (h.cls <= CL_ARTERIAL && h.coff >= 0 && h.coff < 0.08f)  // big roads get a centre-line
                    col = CLR_YELLOW;
            }
            else if (h.zone == Z_NONE) continue;                // water / wild → terrain
            else if (h.zone==Z_COM || h.zone==Z_IND || h.zone==Z_HARBOR) {  // dense core, unchanged
                float gx, gy; city_grid_coords(wx, wy, &gx, &gy);
                col = (grid_at(gx, gy, h.urb) == GR_WALK) ? CLR_MEDIUM_GREY  // sidewalk vs building lot
                                                          : lot_color(gx, gy, h.zone);
            }
            else if (h.zone==Z_RES || h.zone==Z_FARM) {          // THE GRADIENT: houses thin into fields
                float gx, gy; city_grid_coords(wx, wy, &gx, &gy);
                if (h.zone==Z_RES && grid_at(gx, gy, h.urb) == GR_WALK) col = CLR_MEDIUM_GREY;  // sidewalk
                else if (h.built) col = lot_color(gx, gy, Z_RES);            // a house
                else { int e; unsigned fid = field_at(wx, wy, &e); col = field_color(fid, e); }  // a field
            }
            else col = ZONE_COL[h.zone];                         // PARK tint
            rectfill(sx, sy, step, step, col);
        }
}

static void draw_world(void) {
    view_metrics();
    render_terrain();
    render_roads(zoom >= 0.45f);                       // LOD: drop minor tier far out
}

// ── GRAPH (debug) VIEW — the road network as the pure VECTOR graph: crisp class-coloured
// centre-lines + intersection-node dots at ANY zoom (so the small roads finally read — a
// field paint aliases away when small), over dimmed terrain. TAB toggles it; the main camera
// now zooms all the way down (ZMAX raised), so this doubles as the eventual sloop street-
// camera. It renders the SAME gedge[]/gnode[] the car sim navigates ("use the graph now").
static const int GEDGE_COL[6] = {
    CLR_RED,          // MOTORWAY
    CLR_ORANGE,       // HIGHWAY
    CLR_YELLOW,       // ARTERIAL
    CLR_LIME_GREEN,   // STREET  (collector / city local)
    CLR_DARK_ORANGE,  // DIRT
    CLR_PINK,         // ACCESS  (residential lane)
};
#define GRAPH_STREET_ZOOM 3.0f         // at/above this the intra-city grid is vectorised in too (LOD)
static void draw_graph_view(void) {
    view_metrics();
    gather_cities();
    gather_arterials();                                  // fills the sg* cache...
    build_graph();                                       // ...arterials packed into gedge[]
    if (zoom >= GRAPH_STREET_ZOOM) graph_add_grid();     // + the intra-city grid (nodes + edges)
    render_terrain();
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();  // dim
    for (int i = 0; i < nedge; i++) {                    // EDGES — vector centre-lines, class-coloured
        int x0 = sxp(gedge[i].x0), y0 = syp(gedge[i].y0);
        int x1 = sxp(gedge[i].x1), y1 = syp(gedge[i].y1);
        int col = GEDGE_COL[gedge[i].cls];
        line(x0, y0, x1, y1, col);
        if (gedge[i].cls <= CL_HIGHWAY) line(x0, y0 + 1, x1, y1 + 1, col);   // fatten the big roads
    }
    for (int i = 0; i < nnode; i++)                      // NODES — only real junctions/dead-ends
        if (g_deg[i] != 2 && g_deg[i] != 0)              // (deg-2 mid-lane nodes were collapsed away)
            pset(sxp(gnode[i].x), syp(gnode[i].y), CLR_WHITE);
    for (int i = 0; i < gn; i++) {                       // city anchors (the macro nodes)
        int x = sxp(gnx[i]), y = syp(gny[i]);
        circfill(x, y, 2, CLR_WHITE); circ(x, y, 2, CLR_BLACK);
    }
}

// ── MAGNIFIERS — insets that re-render the screen-centre point a few zoom levels deeper,
// WITH the street level layered in. "Same data, another zoom": terrain + the very same
// highways (aligned) + the interior streets/zones the map is too coarse to show. Two
// nested lenses now: STREET = L2 district view, BLOCK = the deeper L3 harness (where the
// access streets + footprints + sloop's car will live — see roadnet-streetlevel.md).
#define LOUPE_SZ    116            // L2 district lens   (LOUPE_ZOOM  defined up top)
#define LOUPE2_SZ   78             // L3 block lens      (LOUPE2_ZOOM defined up top)
static void draw_loupe_at(int bx, int by, int sz, float lz, const char *label) {
    float ocamX = camX, ocamY = camY, oz = zoom;      // save the current transform
    float cw = camX + SCREEN_W * 0.5f / P;            // inspected point = screen centre
    float ch = camY + SCREEN_H * 0.5f / P;
    zoom = lz; view_metrics();                        // deep zoom; P recomputed
    camX = cw - (bx + sz * 0.5f) / P;                 // centre the inspected point in the box
    camY = ch - (by + sz * 0.5f) / P;
    rectfill(bx - 1, by - 1, sz + 2, sz + 2, CLR_WHITE);   // frame
    clip(bx, by, sz, sz);
    render_terrain();
    render_streetlevel(bx, by, sz);                   // owns the road surface in the lens
    draw_nodes(1);                                    // + just the city markers
    clip(0, 0, 0, 0);
    camX = ocamX; camY = ocamY; zoom = oz; view_metrics();   // restore
    print(label, bx + 3, by + 3, CLR_WHITE);
}
static void draw_loupe(void) {
    line(SCREEN_W/2 - 4, SCREEN_H/2, SCREEN_W/2 + 4, SCREEN_H/2, CLR_WHITE);   // map crosshair
    line(SCREEN_W/2, SCREEN_H/2 - 4, SCREEN_W/2, SCREEN_H/2 + 4, CLR_WHITE);
    draw_loupe_at(SCREEN_W - LOUPE_SZ - 3, SCREEN_H - LOUPE_SZ - 3,            // STREET, bottom-right
                  LOUPE_SZ, LOUPE_ZOOM, "STREET");
    draw_loupe_at(SCREEN_W - LOUPE2_SZ - 3, 3, LOUPE2_SZ, LOUPE2_ZOOM, "BLOCK"); // BLOCK, top-right
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
    if (keyp(KEY_TAB)) view = (view + 1) % VIEW_COUNT;   // cycle MAP ↔ GRAPH (vector network)
    if (keyp('G')) show_grid = !show_grid;
    if (keyp('H')) show_hud  = !show_hud;
    if (keyp('L')) show_loupe = !show_loupe;     // magnifier into the street level
    if (keyp('M')) mode = 0;                     // re-open the setup panel
}

void draw(void) {
    if (mode == 1 && view == VIEW_GRAPH) draw_graph_view();   // the vector road-network debug view
    else draw_world();                           // the painterly map (always, incl. setup preview)
    if (show_loupe && view == VIEW_MAP) draw_loupe();   // lenses only make sense over the map view
    if (mode == 0) draw_setup_panel();
    else if (show_hud) hud();
}
