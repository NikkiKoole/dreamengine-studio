/* de:meta
{
  "slug": "roadnet2",
  "collection": ["road"],
  "title": "roadnet 2",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "tech-demo",
    "generative"
  ],
  "teaches": [
    "noise-terrain",
    "generative-melody"
  ],
  "lineage": "A graph-first rebuild of roadnet (v1), keeping its Catmull-Rom node-lattice spline highways verbatim but replacing the dual field+graph street representation with a single edge-graph — cleaner foundation for the collector/access/building fill-in to come.",
  "description": "A VECTOR-NATIVE rebuild of roadnet, started from roadnet's clean rung-1..3 baseline: the same elegant spline-highway core (terrain-aware Catmull-Rom roads between ranked hub/town cities over an infinite deterministic heightmap) with the rest deliberately UNFILLED — the clean foundation. The deeper-road fill-in (collectors / access / cul-de-sacs / buildings) is being redone graph-first this time: ONE representation (spline edges in a graph), ONE query (road_at = nearest edge within its class half-width, generalising arterial_at downward), buildings on edges, warped-grid curvy deeper roads, and NO modular street field — the dual field+graph representation is what made v1 messy. WORLDGEN RUNG 1 IS IN: the unified wn_road_at() — a nearest-edge query + spatial index over the SAME spline edges the renderer strokes, materialized around the car once per cell crossing, with the network/edge-type field (road/rail/water) pinned into the edge model from day one. The car reads the surface (off the tarmac it drags to ~60 km/h), the HUD names the road class under you, C spawns the car snapped ONTO the nearest road facing along it, and G at drive zoom overlays the cached graph over the strokes — the two must coincide (screen == query). Plan + build order: docs/design/roadnet2-plan.md; the rung ladder: docs/design/worldgen-plan.md. Controls (L0 core, same as roadnet): drag the panel sliders, ROLL / EXPLORE; then arrows / WASD pan, mouse wheel zooms, SPACE jumps to fresh scenery, R new seed, M reopens the panel, C drop/spawn the car, G graph/cell overlay, H hides the HUD."
}
de:meta */
#include "studio.h"
#include "worldnet.h"   // the shared WORLD SPINE (terrain + lattice + spline links + the
                        // wn_road_at() graph query) — extracted from this cart at rung 1
                        // when sloop became the second consumer; this cart stays its home
#include "ui.h"
#include <stdio.h>

// ============================================================================
// ROADNET 2  —  a VECTOR-NATIVE rebuild of roadnet. Same elegant spline highway
// core (this file = roadnet's clean rung-1..3 baseline, commit c16c9a2, verbatim),
// but the deeper-road FILL-IN will be done graph-first this time: ONE representation
// (spline edges in a graph), ONE query (road_at = nearest edge within its half-width,
// generalising arterial_at downward), buildings on edges, warped-grid deeper roads.
// NO modular street FIELD (grid_at/street_axis) and NO field render — that dual-
// representation is what made roadnet v1 messy. Full plan + build order + the lessons
// carried over: docs/design/roadnet2-plan.md.
//
// STATUS: L0 core + WORLDGEN RUNG 1 (the unified wn_road_at() graph query — see the
// fenced block below; ladder: docs/design/worldgen-plan.md). The vectorland fill-in
// (collectors → access → cul-de-sacs → buildings) is the work to come, per the plan.
//
// ---- original roadnet rung-1..3 header (the L0 core, unchanged) ----------------
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
// Rung 3 = RANK + ROAD CLASS: each node hashes to a rank (hamlet/town/city/metro),
// and a link's class is f(its endpoints' ranks) — motorway/highway/arterial/street/
// dirt — driving width, colour, and a dashed centre-line on the big roads; markers
// size by rank. SPACING: priority suppression (a stronger town/metro candidate within
// R suppresses/demotes its neighbours) → blue-noise distribution, still pure-hash and
// seam-true. VALLEY-BIAS: a clear link bows toward the lower side at its midpoint, so
// roads sag into valleys / hug coasts (reverts if it would hit water). (Landmarks = later.)
// ============================================================================

// ── METRE RE-BASE: 1 world unit = 1 METRE. TILE = px per metre at zoom 1 (so zoom 1 ≈ 4 px/m
// is a drive zoom; zoom ~0.012 is a town overview; ~0.0008 is continental). WS (worldnet.h) =
// the world scale applied to the OLD tile-based design — terrain features + the bend/bridge/
// valley distances all stretch by WS so the look is preserved while the lattice is in real km.
#define TILE 4                                   // screen px per metre at zoom 1
#define ZMIN 0.0008f                             // far out — see a continent of cities
#define ZMAX 3.0f                                // tight — the DRIVE follow-cam
#define OVERVIEW_ZOOM 0.012f                     // town-network overview (setup preview / MAP default)
#define SPAWN_X 40000.0f                         // a land spot at metre scale (old 312,8 was ocean now)
#define SPAWN_Y 30000.0f

#define ROAD_R    2                  // road ribbon radius (px) at this zoom
#define PANEL_W   96                 // intro-panel width (px); left-drag pans only right of it

// the tweakable world params (P_*), their range getters, and NODE_CS/HUB_CS now
// live in worldnet.h — the intro-panel sliders write the P_* globals directly.

static int mode = 0;                 // 0 = intro/setup panel, 1 = explore

static float camX, camY;             // top-left of view, in world-tile coords
static float jumpN;                  // SPACE counter → deterministic far jumps
static int   show_grid = 0;          // cell-border overlay (seam test)
static int   show_hud  = 1;
static float zoom = 1.0f;            // mousewheel zoom (ZMIN..ZMAX)
static float P    = TILE;            // pixels per tile = TILE * zoom (set per frame)

// ── CAR (build step 2: the SCALE measuring instrument) — left-click the map to drop it,
// arrows/WASD drive, camera follows; C toggles it off (free-cam). Physics ported from
// steer.c (speed-scaled steering + grip). Speed reads out in km/h via M_PER_UNIT — the
// SCALE KNOB you tune by feel. NOTE: the car is unit-scaled + fixed-pixel for now so it's
// visible at the map's zoom; a metre-sized, metre-accurate car wants the L0 metre re-base
// (lattice in km + wider zoom) — the deliberate next step. See roadnet2-plan.md.
#define M_PER_UNIT  1.0f     // 1 world unit = 1 METRE (the re-base) — km/h now reads true
#define CAR_ACCEL   0.020f   // gas, m/frame² along the heading
#define CAR_BRAKE   0.040f
#define CAR_REVMAX -0.15f    // ~32 km/h reverse
#define CAR_FRIC    0.97f    // engine speed kept per frame
#define CAR_MAXSPD  0.60f    // m/frame → ~130 km/h (0.6·60·3.6)
#define CAR_STEER   3.0f     // deg/frame at full speed (scales down with speed)
#define CAR_GRIP    0.22f    // velocity→heading lerp
static int   car_on = 0;
static float car_x, car_y, car_vx, car_vy, car_spd, car_ang, car_odo;  // odo in metres
static int   click_px, click_py;     // press anchor — click (tiny move) = place car, else = drag-pan
static void car_spawn(float wx, float wy) {
    car_x = wx; car_y = wy; car_vx = car_vy = car_spd = 0; car_ang = -90; car_odo = 0; car_on = 1;
}

// ── TWO CAMERAS (the driving harness) — DRIVE follows the car at a tight zoom with a metre
// reference grid + a GPS minimap; MAP is the overview you pop to (click the GPS) and fast-travel
// from (click a spot → car teleports there → back to DRIVE). One world, two cameras. See
// roadnet2-plan.md → "Two cameras + GPS".
enum { V_MAP, V_DRIVE };
static int   vmode = V_MAP;
#define DRIVE_ZOOM 1.5f            // tight follow-cam (~6 px/m → ~53 m view, car ≈ 27 px)
#define MAP_ZOOM   OVERVIEW_ZOOM   // overview zoom when you pop to the map
#define GRID_M     20.0f          // reference-grid spacing, metres
#define GPS_SZ     78             // GPS minimap box size (px)
#define GPS_ZOOM   0.004f         // the minimap's own zoom (~5 km box — shows the local town network)
#define GPS_X      (SCREEN_W - GPS_SZ - 3)
#define GPS_Y      (SCREEN_H - GPS_SZ - 3)
static int   vcols, vrows;           // visible world-tile span this frame (road bounds)
static int   dragging = 0, drag_px, drag_py;  // left-drag-to-pan ("grab the map")

static void view_metrics(void) {     // recompute P + visible tile span from zoom
    P = TILE * zoom;
    vcols = (int)(SCREEN_W / P) + 3;             // world-tile span (road-loop bounds)
    vrows = (int)(SCREEN_H / P) + 3;
}

// hash/terrain/ranks/lattice (hash2 → wn_hash2, height_at, biome_col, passable,
// RK_*/CL_*, town_*/hub_rank, get_node/get_hub) now live in worldnet.h.

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

// catmull / build_curve / classify_curve / link_path (THE geometry seam) + the
// LINK_SAMPLES..BEND_STEP tuning now live in worldnet.h; the strokers below read lp_.

// Stroke the path in lp_ as overlapping circles, fine enough at any zoom. Spans tagged
// as bridges (lp_br) draw in `bcol` so a road over water reads as a bridge.
static void stroke_path(int n, int r, int col, int bcol) {
    for (int i = 0; i + 1 < n; i++) {
        int c = (lp_br[i] || lp_br[i+1]) ? bcol : col;     // a span touching water = bridge
        int x0 = sxp(lp_x[i]), y0 = syp(lp_y[i]), x1 = sxp(lp_x[i+1]), y1 = syp(lp_y[i+1]);
        int M = r + 4;                                     // skip fully off-screen spans
        if ((x0<-M&&x1<-M)||(x0>SCREEN_W+M&&x1>SCREEN_W+M)||
            (y0<-M&&y1<-M)||(y0>SCREEN_H+M&&y1>SCREEN_H+M)) continue;
        // draw the span as a CLEAN ribbon: a quad (centre-line ± perpendicular·r) + a joint
        // circle to round the corner. (Was a chain of circles → scalloped "blobs" when wide.)
        float ddx = x1 - x0, ddy = y1 - y0, L = fsqrt(ddx*ddx + ddy*ddy);
        line(x0, y0, x1, y1, c);                                 // continuous centre-line — keeps thin
                                                                 // roads SOLID when zoomed out (else the
                                                                 // quad's perp rounds to 0 → dotted)
        if (r >= 2 && L >= 1.0f) {                               // wide enough → fill the ribbon + round joint
            int px = (int)(-ddy / L * r), py = (int)(ddx / L * r);
            int xy[8] = { x0+px, y0+py, x1+px, y1+py, x1-px, y1-py, x0-px, y0-py };
            polyfill(xy, 4, c);
            circfill(x1, y1, r, c);
        }
    }
}

// Dashed yellow centre-line along lp_ (highways/motorways). Walks the path in screen
// pixels so the dash period is constant at any zoom; skipped over bridge spans.
static void stroke_dashes(int n) {
    int acc = 0;
    for (int i = 0; i + 1 < n; i++) {
        int x0 = sxp(lp_x[i]), y0 = syp(lp_y[i]), x1 = sxp(lp_x[i+1]), y1 = syp(lp_y[i+1]);
        if ((x0<-4&&x1<-4)||(x0>SCREEN_W+4&&x1>SCREEN_W+4)||   // skip off-screen spans (see stroke_path)
            (y0<-4&&y1<-4)||(y0>SCREEN_H+4&&y1>SCREEN_H+4)) continue;
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
// ROAD_HW_M (real carriageway half-widths, metres — worldnet.h) drawn at hw·P pixels so a
// road is true-width when you drive (car sits in its lane) yet floors to 1px on the overview map.
static void draw_link(int n, int cls, int phase) {
    const RStyle *s = &RS[cls];
    int r = (int)(ROAD_HW_M[cls] * P); if (r < 1) r = 1;     // metre half-width → px (floored)
    if (phase == 0) {
        stroke_path(n, r, s->casing, CLR_BROWN);             // casing/full width (brown over water = bridge)
    } else {
        int sh = (int)(1.5f * P); if (sh < 1) sh = 1;        // shoulder/kerb inset
        int cr = r - sh;
        if (cr >= 1) stroke_path(n, cr, s->centre, CLR_WHITE);   // carriageway (white over water = bridge)
        if (s->mark && zoom >= 0.7f) stroke_dashes(n);       // centre dashes (drive zoom)
    }
}

// HIGHWAYS — the connected backbone. Hub lattice, forward-only links, Catmull-Rom
// shaped by the hubs beyond each span (reflect when absent). phase 0=casing, 1=centre.
// The backbone is a PROXIMITY GRAPH over the hubs (β-skeleton), not a lattice: a link A–B
// exists only if no third hub C lies in the forbidden lune (|CA|²+|CB|² < β·|AB|²). β=1 is the
// Gabriel graph (looped, "highway map"); β→1.8 thins toward the RNG (tree-like). This gives a
// sparse, branching corridor network with low-degree junctions — real highways, not a grid of
// at-grade knots. Local + deterministic: gather the hubs near the view (margin so every on-screen
// edge's lune is fully covered → no pop on pan), then it's pure geometry over that set.
#define HW_MAX 512
static void draw_highways(int phase) {
    int c0 = wn_ifloor(camX / HUB_CS) - 4, c1 = wn_ifloor((camX + vcols) / HUB_CS) + 4;
    int r0 = wn_ifloor(camY / HUB_CS) - 4, r1 = wn_ifloor((camY + vrows) / HUB_CS) + 4;
    static float hx[HW_MAX], hy[HW_MAX]; static int hrk[HW_MAX]; int N = 0;
    for (int cx = c0; cx <= c1; cx++)
        for (int cy = r0; cy <= r1 && N < HW_MAX; cy++) {
            float x, y; if (!get_hub(cx, cy, &x, &y)) continue;
            hx[N]=x; hy[N]=y; hrk[N]=hub_rank(cx, cy); N++;
        }
    float beta = hw_beta();
    float maxd2 = (3.2f * HUB_CS) * (3.2f * HUB_CS);          // only consider nearby pairs
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++) {
            float ax=hx[i], ay=hy[i], bx=hx[j], by=hy[j];
            float ab2 = (bx-ax)*(bx-ax) + (by-ay)*(by-ay);
            if (ab2 > maxd2 || ab2 < 1) continue;
            int linked = 1;                                  // β-skeleton test against every other hub
            for (int k = 0; k < N; k++) {
                if (k==i || k==j) continue;
                float ca2 = (hx[k]-ax)*(hx[k]-ax) + (hy[k]-ay)*(hy[k]-ay);
                float cb2 = (hx[k]-bx)*(hx[k]-bx) + (hy[k]-by)*(hy[k]-by);
                if (ca2 + cb2 < beta * ab2) { linked = 0; break; }
            }
            if (!linked) continue;
            float p0x=2*ax-bx, p0y=2*ay-by, p3x=2*bx-ax, p3y=2*by-ay;   // straight base; link_path bends for terrain
            int cls = (hrk[i]==RK_METRO || hrk[j]==RK_METRO) ? CL_MOTORWAY : CL_HIGHWAY;
            int np = link_path(p0x,p0y, ax,ay, bx,by, p3x,p3y);
            if (np) draw_link(np, cls, phase);
        }
}

// nearest_hub / nearest_town + THE GRAPH + wn_road_at() (the rung-1 fenced block:
// NET_* edge types, REdge, WnHit, the eg_ edge cache + spatial hash + query) were
// extracted VERBATIM to worldnet.h when sloop became the second consumer.

// rung-1 car wiring: the surface drives the car (the graph query, not pixels)
#define OFFROAD_DRAG 0.96f   // extra per-frame speed keep off the tarmac (top ≈ 63 km/h)
static WnHit car_hit;        // this frame's query at the car — HUD + grip + trace read it
static const char *CLS_NAME[5] = { "MOTORWAY", "HIGHWAY", "ARTERIAL", "STREET", "DIRT" };

// MINOR ROADS — each town links to its nearest hub (feeder onto the backbone) AND to
// its nearest other town (a local road). The local roads chain clusters together so a
// town far from any hub still connects via town→town→…→hub — not everything needs a
// highway. Degree ~1 each, so no criss-cross. Owned by the town; mutual nearest pairs
// double-draw harmlessly (identical pixels).
static void draw_feeders(int phase) {
    int m = 5;
    int c0 = wn_ifloor(camX / NODE_CS) - m, c1 = wn_ifloor((camX + vcols) / NODE_CS) + m;
    int r0 = wn_ifloor(camY / NODE_CS) - m, r1 = wn_ifloor((camY + vrows) / NODE_CS) + m;
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
                int nr = town_rank(wn_ifloor(nx / NODE_CS), wn_ifloor(ny / NODE_CS));
                int cls = (tr == RK_TOWN || nr == RK_TOWN) ? CL_STREET : CL_DIRT;
                np = link_path(2*tx - nx, 2*ty - ny, tx, ty, nx, ny, 2*nx - tx, 2*ny - ty);
                if (np) draw_link(np, cls, phase);
            }
        }
}

static void draw_nodes(int detail) {
    // towns (feeder tier) — orange = town, small red = hamlet; hidden far out (LOD)
    if (detail) {
        int c0 = wn_ifloor(camX / NODE_CS) - 2, c1 = wn_ifloor((camX + vcols) / NODE_CS) + 2;
        int r0 = wn_ifloor(camY / NODE_CS) - 2, r1 = wn_ifloor((camY + vrows) / NODE_CS) + 2;
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
    int h0 = wn_ifloor(camX / HUB_CS) - 2, h1 = wn_ifloor((camX + vcols) / HUB_CS) + 2;
    int v0 = wn_ifloor(camY / HUB_CS) - 2, v1 = wn_ifloor((camY + vrows) / HUB_CS) + 2;
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
    for (int cx = wn_ifloor(camX / NODE_CS); cx * NODE_CS < camX + vcols; cx++) {
        int x = (int)((cx * NODE_CS - camX) * P);
        line(x, 0, x, SCREEN_H, CLR_DARK_PURPLE);
    }
    for (int cy = wn_ifloor(camY / NODE_CS); cy * NODE_CS < camY + vrows; cy++) {
        int y = (int)((cy * NODE_CS - camY) * P);
        line(0, y, SCREEN_W, y, CLR_DARK_PURPLE);
    }
}

static void hud(void) {
    char buf[64];
    float cx = camX + vcols / 2.0f, cy = camY + vrows / 2.0f;
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("ROADNET2", 4, 2, CLR_LIGHT_GREY);
    if (vmode == V_DRIVE && car_on) {            // DRIVE: speed readout = the scale instrument
        float kmh = (car_spd < 0 ? -car_spd : car_spd) * M_PER_UNIT * 60.0f * 3.6f;
        snprintf(buf, sizeof buf, "%3d km/h   %.2f km", (int)kmh, car_odo / 1000.0f);
        print(buf, 84, 2, CLR_YELLOW);
        print(car_hit.on_road ? CLS_NAME[car_hit.cls] : "OFF-ROAD", 232, 2,   // rung 1: what
              car_hit.on_road ? CLR_GREEN : CLR_ORANGE);                      // wn_road_at() says
        print_centered("\x18\x19\x1a\x1b/WASD drive   click GPS = map   C drop car",
                       SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
    } else {                                     // MAP overview
        snprintf(buf, sizeof buf, "x %d  y %d", (int)cx, (int)cy);
        print(buf, 84, 2, CLR_MEDIUM_GREY);
        if (show_grid) print("[grid]", 170, 2, CLR_DARK_PURPLE);
        print_centered("click = drive there   drag/\x18\x19\x1a\x1b pan   wheel zoom   SPACE jump   M setup",
                       SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
    }
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
                                "wiggle", "web", "water" };
    float *pp[7] = { &P_hub_space, &P_town_space, &P_hub_dens, &P_town_dens,
                     &P_jitter, &P_web, &P_sea };
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
    int detail = (zoom >= 0.0035f);              // metre scale: towns/feeders from the overview up; only highways continental
    draw_highways(0); if (detail) draw_feeders(0);    // casings first...
    draw_highways(1); if (detail) draw_feeders(1);    // ...then centres
    draw_nodes(detail);
}

void init(void) {
    zoom = OVERVIEW_ZOOM; view_metrics();        // open zoomed to the town network (metre scale)
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

    float pp = TILE * zoom;

    if (vmode == V_DRIVE && car_on) {            // ══ DRIVE — follow the car, arrows/WASD steer ══
        float turn = (btn(0,BTN_RIGHT)||key('D') ? 1.0f:0) - (btn(0,BTN_LEFT)||key('A') ? 1.0f:0);
        float sc = car_spd / CAR_MAXSPD; if (sc < 0) sc = -sc;
        if (sc > 0.04f && sc < 0.5f) sc = 0.5f;  // responsive even at low speed (but a parked car can't turn)
        car_ang += turn * CAR_STEER * sc;        // steering scales with speed
        if (btn(0,BTN_UP)  ||key('W')) car_spd += CAR_ACCEL;
        if (btn(0,BTN_DOWN)||key('S')) car_spd -= CAR_BRAKE;
        car_spd *= CAR_FRIC; car_spd = clamp(car_spd, CAR_REVMAX, CAR_MAXSPD);
        // rung 1: the SURFACE drives the car — wn_road_at() at the car, off-road drags
        car_hit = wn_road_at(car_x, car_y);
        if (!car_hit.on_road) car_spd *= OFFROAD_DRAG;
#ifdef DE_TRACE
        watch("on_road", "%d", car_hit.on_road);
        watch("cls", "%d", car_hit.cls);
        watch("rdist", "%.1f", car_hit.dist);
        watch("eg_n", "%d", eg_n);
        watch("eg_ov", "%d", eg_overflow);
        watch("cx", "%.1f", car_x);
        watch("cy", "%.1f", car_y);
#endif
        car_vx = lerp(car_vx, dx(car_spd, car_ang), CAR_GRIP);    // velocity chases the heading
        car_vy = lerp(car_vy, dy(car_spd, car_ang), CAR_GRIP);
        car_x += car_vx; car_y += car_vy;
        car_odo += fsqrt(car_vx*car_vx + car_vy*car_vy) * M_PER_UNIT;
        camX = car_x - SCREEN_W * 0.5f / pp; camY = car_y - SCREEN_H * 0.5f / pp;  // camera follows
        if (mouse_pressed(MOUSE_LEFT) && mouse_x() >= GPS_X && mouse_y() >= GPS_Y) {   // click GPS → MAP
            vmode = V_MAP; zoom = MAP_ZOOM; view_metrics();
            camX = car_x - SCREEN_W*0.5f/(TILE*zoom); camY = car_y - SCREEN_H*0.5f/(TILE*zoom);
        }
    } else {                                     // ══ MAP — overview: pan + click to fast-travel ══
        float pan = 1.3f / zoom;
        if (btn(0, BTN_LEFT)  || key('A')) camX -= pan;
        if (btn(0, BTN_RIGHT) || key('D')) camX += pan;
        if (btn(0, BTN_UP)    || key('W')) camY -= pan;
        if (btn(0, BTN_DOWN)  || key('S')) camY += pan;
        int pan_ok = (mode == 1) || (mouse_x() > PANEL_W);
        if (mouse_pressed(MOUSE_LEFT) && pan_ok) { dragging = 1; drag_px = click_px = mouse_x(); drag_py = click_py = mouse_y(); }
        if (mouse_down(MOUSE_LEFT)) {
            if (dragging) { camX -= (mouse_x()-drag_px)/pp; camY -= (mouse_y()-drag_py)/pp; drag_px=mouse_x(); drag_py=mouse_y(); }
        } else {
            if (dragging && mode == 1) {         // released as a CLICK (tiny move) → drop/teleport car → DRIVE
                int ddx = mouse_x()-click_px, ddy = mouse_y()-click_py;
                if (ddx*ddx + ddy*ddy < 16) { car_spawn(camX + mouse_x()/pp, camY + mouse_y()/pp);
                                              vmode = V_DRIVE; zoom = DRIVE_ZOOM; view_metrics(); }
            }
            dragging = 0;
        }
    }

    if (keyp('C')) {                             // C toggles the car: drop it, or (in explore)
        if (car_on) { car_on = 0; vmode = V_MAP; }         // spawn ON the road nearest the view
        else if (mode == 1) {                              // centre, aligned to it → DRIVE — the
            float p2 = TILE * zoom;                        // deterministic harness entry
            float wx = camX + SCREEN_W * 0.5f / p2, wy = camY + SCREEN_H * 0.5f / p2;
            float rang;
            if (wn_nearest_road_point(wx, wy, &wx, &wy, &rang)) { car_spawn(wx, wy); car_ang = rang; }
            else car_spawn(wx, wy);
            vmode = V_DRIVE; zoom = DRIVE_ZOOM; view_metrics();
        }
    }
    if (keyp(KEY_SPACE) && vmode == V_MAP) {     // deterministic jump to fresh scenery
        jumpN += 1.0f;
        unsigned h = wn_hash2((int)(jumpN * 911), 7);
        camX += (float)((int)(h % 4000u) - 2000) + 800.0f;
        camY += (float)((int)((h >> 11) % 4000u) - 2000) + 800.0f;
    }
    if (keyp('R')) { zoom = MAP_ZOOM; view_metrics(); camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f; seedZ += 0.37f; jumpN = 0; car_on = 0; vmode = V_MAP; }
    if (keyp('G')) show_grid = !show_grid;
    if (keyp('H')) show_hud  = !show_hud;
    if (keyp('M')) { mode = 0; car_on = 0; vmode = V_MAP; zoom = OVERVIEW_ZOOM; view_metrics(); }  // setup panel
}

static void draw_car(void) {                     // metre-sized now (1u = 1m)
    int sx = sxp(car_x), sy = syp(car_y);
    int r = (int)(0.9f * P); if (r < 2) r = 2;   // ~1.8 m body
    circfill(sx, sy, r, CLR_RED); circ(sx, sy, r, CLR_BLACK);
    line(sx, sy, sx + (int)dx(3.0f*P, car_ang), sy + (int)dy(3.0f*P, car_ang), CLR_WHITE);  // ~3 m nose
}
// faint world-space reference grid every GRID_M metres — the SCALE device (watch the metres go by)
static void draw_grid_ref(void) {
    float gm = GRID_M / M_PER_UNIT;              // grid spacing in world units
    if (gm <= 0.001f) return;
    for (int i = wn_ifloor(camX/gm); i <= wn_ifloor((camX+vcols)/gm); i++) {
        int x = sxp(i*gm); line(x, 11, x, SCREEN_H, CLR_DARK_GREY);
    }
    for (int j = wn_ifloor(camY/gm); j <= wn_ifloor((camY+vrows)/gm); j++) {
        int y = syp(j*gm); line(0, y, SCREEN_W, y, CLR_DARK_GREY);
    }
}
// GPS minimap — the same world rendered at an overview zoom into a corner box, car at centre.
static void draw_gps(void) {
    float ocamX = camX, ocamY = camY, oz = zoom;
    rectfill(GPS_X-1, GPS_Y-1, GPS_SZ+2, GPS_SZ+2, CLR_WHITE);    // frame
    clip(GPS_X, GPS_Y, GPS_SZ, GPS_SZ);
    zoom = GPS_ZOOM; view_metrics();
    camX = car_x - (GPS_X + GPS_SZ*0.5f)/(TILE*zoom);            // centre the car in the box
    camY = car_y - (GPS_Y + GPS_SZ*0.5f)/(TILE*zoom);
    draw_world();                                               // terrain + roads, clipped to the box
    circfill(sxp(car_x), syp(car_y), 2, CLR_RED);                // you-are-here
    clip(0, 0, 0, 0);
    camX = ocamX; camY = ocamY; zoom = oz; view_metrics();
    print("GPS", GPS_X+2, GPS_Y+2, CLR_WHITE);
}
// G in DRIVE: overlay the CACHED graph the query answers from, on top of the strokes —
// the two must coincide (screen == query), eyeballable. Pink = road, white = bridge span.
static void draw_graph_overlay(void) {
    for (int e = 0; e < eg_n; e++)
        for (int i = 0; i < LINK_SAMPLES; i++)
            line(sxp(eg_e[e].x[i]), syp(eg_e[e].y[i]),
                 sxp(eg_e[e].x[i+1]), syp(eg_e[e].y[i+1]),
                 eg_e[e].br[i] ? CLR_WHITE : CLR_PINK);
}
void draw(void) {
    draw_world();                                // terrain + roads at the current camera
    if (vmode == V_DRIVE && car_on) {
        if (show_grid) draw_graph_overlay();     // rung 1: the graph over the strokes
        draw_grid_ref();                         // metre grid (scale)
        draw_car();
        draw_gps();                              // minimap inset
    } else if (car_on) {                         // MAP: show where the car is parked
        int x = sxp(car_x), y = syp(car_y);
        circfill(x, y, 3, CLR_RED); circ(x, y, 3, CLR_WHITE);
    }
    if (mode == 0) draw_setup_panel();
    else if (show_hud) hud();
}
