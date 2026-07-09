// worldnet.h — the infinite deterministic WORLD SPINE, cart-land (ADR-0006).
//
// Extracted VERBATIM from roadnet2.c's fenced rung-1 block (worldgen-plan rung 1 /
// driving-world-program A1) the moment sloop became the second consumer — the roadkit
// trigger discipline. roadnet2 stays the spine's home cart (rendering, cameras, sliders);
// this header is the shared MODEL: terrain, the ranked hub/town lattice, terrain-aware
// spline links, and the ONE query — wn_road_at() = nearest edge within its class
// half-width, over a per-anchor edge cache + spatial hash. ONE data model (the
// worldgen-plan guard): every consumer reads the same typed edge graph; the edge-type
// field (NET_ROAD / NET_RAIL / NET_WATER) is pinned so rivers + rails land without
// breaking the seam.
//
// CONTRACT
//   · include AFTER studio.h (uses noise2/noise3, fsqrt, sin_deg, angle_to)
//   · units: 1 world unit = 1 METRE, y-down; everything is a pure function of
//     (cell coords, the P_* params, seedZ) → infinite, deterministic, seam-true
//   · params: the P_* globals below (defaults = roadnet2's tuned look) + seedZ;
//     change them any time — the edge cache watches and rebuilds itself
//   · query: WnHit wn_road_at(x, y) → on_road (within the class half-width),
//     on_pave (+EG_PAVE m — the walk strip), cls (CL_*), zone (land-use; = cls
//     until the content rung), net (NET_*), dist (m to the nearest centre-line)
//   · spawn: wn_nearest_road_point(qx, qy, &x, &y, &ang) → drop a car ON the net
//   · rendering: consumers may read the cache directly after wn_road_at()/
//     eg_ensure() — eg_e[0..eg_n): sampled polylines + cls/net/bridge flags —
//     or re-enumerate with get_hub/get_node/link_path (same pure functions).
//   · cost: the cache rebuilds once per node-cell crossing (≈ one draw-frame's
//     worth of work); a query is a 3×3 bucket scan. Overflow → brute, never wrong.
//
// Consumers: roadnet2 (the spine cart), sloop (the P1 seam's procedural backend).
#ifndef WORLDNET_H
#define WORLDNET_H

// ── METRE BASE: WS stretches the old tile-based terrain design to metre scale ─
#define WS   300.0f

// ── tweakable world params — all 0..1 (roadnet2's sliders drive these) ────────
static float P_hub_space  = 0.167f;  // hub lattice spacing  → 25..60 km
static float P_town_space = 0.167f;  // town lattice spacing → 3..8 km
static float P_hub_dens   = 0.75f;   // hub presence         → 40..100 %
static float P_town_dens  = 0.444f;  // town presence        → 0..90 %
static float P_jitter     = 0.6f;    // node wiggle off-grid → 0..~0.45·cell
static float P_web        = 0.5f;    // highway WEB density (β-skeleton): 0 = sparse/tree, 1 = looped/mesh
static float P_sea        = 0.5f;    // sea level            → subtract 0.30..0.60
static float seedZ        = 0.0f;    // noise z-slice = the world's "seed"

static int   hub_cs(void)   { return 25000 + (int)(P_hub_space  * 35000); }  // 25..60 km (cities)
static int   node_cs(void)  { return 3000  + (int)(P_town_space * 5000);  }  // 3..8 km (towns)
static int   hub_pct(void)  { return 40 + (int)(P_hub_dens   * 60); }   // 40..100
static int   town_pct(void) { return (int)(P_town_dens * 90); }         // 0..90
static int   jit(int cs)    { int j = (int)(P_jitter * cs * 0.45f); return j < 1 ? 1 : j; }
static float hw_beta(void)  { return 1.8f - P_web * 1.0f; }             // β-skeleton param: 1.8 sparse(RNG) .. 0.8 dense
static float sea_sub(void)  { return 0.30f + P_sea * 0.30f; }           // 0.30..0.60
#define NODE_CS  node_cs()
#define HUB_CS   hub_cs()

// ── deterministic per-cell hash (worldgen's) ─────────────────────────────────
static unsigned wn_hash2(int a, int b) {
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static int wn_ifloor(float v) { int i = (int)v; return (v < 0 && (float)i != v) ? i - 1 : i; }

// ── HEIGHTMAP + RIVERS — worldgen's createZoomedinWorld(), verbatim (scalein 1) ─
static float height_at(float i, float j) {
    i /= WS; j /= WS;                                   // metre re-base: stretch terrain features ×WS
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
// priority suppression give blue-noise spacing. All pure functions of cell coords
// → seam-true. A road's CLASS is f(endpoint ranks). ─────────────────────────────
enum { RK_HAMLET, RK_TOWN, RK_CITY, RK_METRO };
enum { CL_MOTORWAY, CL_HIGHWAY, CL_ARTERIAL, CL_STREET, CL_DIRT };

static int town_rank(int cx, int cy) {                   // feeder node: town / hamlet
    unsigned h = wn_hash2(cx * 1900385059u + 61, cy * 2120969693u + 17);
    return (h % 100u < 35u) ? RK_TOWN : RK_HAMLET;
}
// a town CANDIDATE exists here (density gate only — no terrain, so suppression is cheap)
static int town_present(int cx, int cy) {
    unsigned h = wn_hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    return (h % 100u) < (unsigned)town_pct();
}
// suppression priority: higher rank wins, hash breaks ties (cell folded in → no ties)
static unsigned town_prio(int cx, int cy) {
    unsigned h = wn_hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    return ((unsigned)town_rank(cx, cy) << 28)
         | ((h ^ ((unsigned)cx * 2246822519u + (unsigned)cy * 3266489917u)) & 0x0fffffffu);
}

// METRO de-clustering: a metro candidate demotes to a city if a higher-priority metro
// candidate sits within R cells — so big cities never bunch up.
static int hub_rank(int cx, int cy) {
    unsigned h = wn_hash2(cx * 915488749u + 31, cy * 147645773u + 7);
    if (h % 100u >= 15u) return RK_CITY;                 // not even a metro candidate
    for (int a = cx - 2; a <= cx + 2; a++)
        for (int b = cy - 2; b <= cy + 2; b++) {
            if (a == cx && b == cy) continue;
            unsigned g = wn_hash2(a * 915488749u + 31, b * 147645773u + 7);
            if (g % 100u < 15u && g > h) return RK_CITY; // a stronger metro nearby → demote
        }
    return RK_METRO;
}

// ── POI NODES — one per cell, with PRIORITY SUPPRESSION for blue-noise spacing ──
static int get_node(int cx, int cy, float *wx, float *wy) {
    if (!town_present(cx, cy)) return 0;                  // density slider
    unsigned p = town_prio(cx, cy);
    for (int a = cx - 1; a <= cx + 1; a++)               // min-distance spacing (R=1)
        for (int b = cy - 1; b <= cy + 1; b++) {
            if (a == cx && b == cy) continue;
            if (town_present(a, b) && town_prio(a, b) > p) return 0;   // a stronger neighbour wins
        }
    unsigned h = wn_hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    int j = jit(NODE_CS);                                // wiggle slider × cell
    float jx = (float)((int)((h >> 7)  % (2*j+1)) - j);
    float jy = (float)((int)((h >> 15) % (2*j+1)) - j);
    float x = cx * NODE_CS + NODE_CS * 0.5f + jx;
    float y = cy * NODE_CS + NODE_CS * 0.5f + jy;
    if (!passable(height_at(x, y))) return 0;            // habitable land only
    *wx = x; *wy = y;
    return 1;
}

// ── HUBS — the highway backbone, on a coarse lattice. Independent hash layer. ──
static int get_hub(int cx, int cy, float *wx, float *wy) {
    unsigned h = wn_hash2(cx * 374761393u + 5, cy * 668265263u + 3);
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

// Catmull-Rom basis through (p0,p1,p2,p3); p1->p2 is the drawn span, t in [0,1].
static void catmull(float p0,float p1,float p2,float p3,float t,float *out){
    float t2=t*t, t3=t2*t;
    *out = 0.5f*( (2*p1) + (-p0+p2)*t
                + (2*p0-5*p1+4*p2-p3)*t2
                + (-p0+3*p1-3*p2+p3)*t3 );
}

#define LINK_SAMPLES 40             // world-space samples per link
#define MAXBEND      (26.0f*WS)     // furthest a road will detour sideways (metres)
#define MAX_BRIDGE   (10.0f*WS)     // longest WATER span a road will bridge (metres)
#define VALLEY_D     (6.0f*WS)      // valley-bias: how far sideways to probe (metres)
#define VALLEY_K     (40.0f*WS)     // ...how strongly height diff pulls the road
#define VALLEY_MAX   (5.0f*WS)      // ...capped detour toward lower ground (metres)
#define BEND_STEP    (1.5f*WS)      // probe step for the bend search (metres)
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

// Scan the current lp_ curve. *peak = any impassable HIGH ground. *maxwater = longest
// run of WATER samples (bridgeable). Returns 1 if the whole curve is on passable land.
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
// short water gap; BEND around the obstacle on land. Returns the sample count, or 0.
static int link_path(float c0x, float c0y, float ax, float ay,
                     float bx, float by, float c3x, float c3y) {
    float dx = bx - ax, dy = by - ay, len = fsqrt(dx*dx + dy*dy);
    if (len < 0.5f) return 0;
    float ux = dx/len, uy = dy/len, perpx = -uy, perpy = ux;
    int peak, maxw;

    // 1. base curve clear? then add a gentle VALLEY bias toward the lower side.
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

    // 2. blocked only by a SHORT stretch of water? → BRIDGE it.
    float spacing = len / LINK_SAMPLES;
    if (!peak && maxw * spacing <= MAX_BRIDGE) {
        for (int i = 0; i <= LINK_SAMPLES; i++)
            lp_br[i] = (height_at(lp_x[i], lp_y[i]) < 0.0f);   // tag the over-water spans
        return LINK_SAMPLES + 1;
    }

    // 3. peak, or water too wide to bridge → try to BEND around it on land.
    int steps = (int)(len / BEND_STEP); if (steps < 2) steps = 2; if (steps > 256) steps = 256;
    float sumt = 0; int nb = 0;
    for (int s = 1; s < steps; s++) {
        float t = (float)s / steps;
        if (!passable_at(ax + dx*t, ay + dy*t)) { sumt += t; nb++; }
    }
    if (!nb) return 0;
    float tc = sumt / nb, ox = ax + dx*tc, oy = ay + dy*tc;
    float clr = 0; int sgn = 0;
    for (float off = BEND_STEP; off <= MAXBEND; off += BEND_STEP) {
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

// real carriageway HALF-widths in METRES, by class (motorway→dirt)
static const float ROAD_HW_M[5] = { 12.0f, 7.0f, 5.0f, 3.5f, 2.5f };

// nearest present hub to a town, searched over the 5x5 hub cells around it (pure fn).
static int nearest_hub(float tx, float ty, float *hx, float *hy) {
    int hc = wn_ifloor(tx / HUB_CS), hr = wn_ifloor(ty / HUB_CS);
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
    int tc = wn_ifloor(tx / NODE_CS), tr = wn_ifloor(ty / NODE_CS);
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

// ═════════════════════════════════════════════════════════════════════════════
// THE GRAPH + wn_road_at() — worldgen-plan RUNG 1: the unified nearest-edge query
// + spatial index over the SAME pure-function edges the renderer strokes.
//
// Edges near an ANCHOR (the car) are materialized once per node-cell crossing,
// using the renderer's own ownership rules (β-skeleton pairs; a town's two
// feeders) so screen == query by construction. Each edge's spans are walked
// into a fixed bucket grid over the coverage box; wn_road_at(x,y) tests only the
// 3×3 buckets around the point. A bucket/pool overflow falls back to a brute
// scan — slower, never wrong.

enum { NET_ROAD, NET_RAIL, NET_WATER };  // edge-TYPE field — pinned into the data model
                                         // BEFORE rung 1 freezes it (worldgen-plan §Beyond
                                         // roads: rivers + rails are later NET_* consumers)

typedef struct {
    float x[LINK_SAMPLES + 1], y[LINK_SAMPLES + 1];  // sampled polyline, world metres
    unsigned char br[LINK_SAMPLES + 1];              // per-sample bridge flag (over water)
    unsigned char cls, net;                          // CL_* + NET_* (all NET_ROAD today)
} REdge;

// WnHit — the P1 seam contract (mirrors sloop's / roadnet v1's RoadHit, so the swap
// is a body change only). zone = land-use; stubbed to cls until the content rung.
// dist = metres to the nearest centre-line — lane-keeping falls out for free.
typedef struct { int on_road, on_pave, cls, zone, net; float dist; } WnHit;

#define EG_MAX   160     // edges cached around the anchor cell
#define EG_GRID  24      // spatial-hash buckets per axis over the coverage box
#define EG_BKT   18      // segment refs per bucket
#define EG_PAVE  2.0f    // pavement strip beside the carriageway (m) — the walk seam
#define EG_MARGIN 200.0f // coverage slack beyond the anchor node cell (m)

static REdge eg_e[EG_MAX];
static int   eg_n, eg_overflow;                    // overflow → brute scan (correct, slower)
static short eg_bkt[EG_GRID][EG_GRID][EG_BKT];     // ref = (edge << 6) | sample
static unsigned char eg_bn[EG_GRID][EG_GRID];
static float eg_x0, eg_y0, eg_bs;                  // coverage-box origin + bucket size
static int   eg_valid = 0;
static struct { float p[7], seed; int cx, cy; } eg_key;   // rebuild trigger snapshot

static float seg_d2(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay;
    float c1 = (px - ax) * vx + (py - ay) * vy;
    if (c1 <= 0) { float dx = px - ax, dy = py - ay; return dx*dx + dy*dy; }
    float c2 = vx*vx + vy*vy;
    if (c2 <= c1) { float dx = px - bx, dy = py - by; return dx*dx + dy*dy; }
    float t = c1 / c2, dx = px - (ax + t*vx), dy = py - (ay + t*vy);
    return dx*dx + dy*dy;
}

// could this straight link (bent up to `infl` sideways) reach the coverage box?
static int eg_reach(float ax, float ay, float bx, float by, float infl) {
    float cxh = eg_x0 + eg_bs * EG_GRID, cyh = eg_y0 + eg_bs * EG_GRID;
    float lox = (ax < bx ? ax : bx) - infl, hix = (ax > bx ? ax : bx) + infl;
    float loy = (ay < by ? ay : by) - infl, hiy = (ay > by ? ay : by) + infl;
    return !(hix < eg_x0 || lox > cxh || hiy < eg_y0 || loy > cyh);
}

// append the current lp_ curve to the pool + walk its spans into the buckets
// (half-bucket strides; only cells inside the coverage box are marked)
static void eg_add(int cls) {
    if (eg_n >= EG_MAX) { eg_overflow = 1; return; }
    REdge *e = &eg_e[eg_n];
    for (int i = 0; i <= LINK_SAMPLES; i++) {
        e->x[i] = lp_x[i]; e->y[i] = lp_y[i]; e->br[i] = (unsigned char)lp_br[i];
    }
    e->cls = (unsigned char)cls; e->net = NET_ROAD;
    for (int i = 0; i < LINK_SAMPLES; i++) {
        float ax = e->x[i], ay = e->y[i], dx = e->x[i+1] - ax, dy = e->y[i+1] - ay;
        float len = fsqrt(dx*dx + dy*dy);
        int steps = (int)(len / (eg_bs * 0.5f)) + 1;
        int lastc = -1, lastr = -1;
        for (int s = 0; s <= steps; s++) {
            float t = (float)s / steps, px = ax + dx*t, py = ay + dy*t;
            if (px < eg_x0 || py < eg_y0) { lastc = -1; continue; }
            int c = (int)((px - eg_x0) / eg_bs), r = (int)((py - eg_y0) / eg_bs);
            if (c >= EG_GRID || r >= EG_GRID) { lastc = -1; continue; }
            if (c == lastc && r == lastr) continue;
            lastc = c; lastr = r;
            if (eg_bn[r][c] >= EG_BKT) { eg_overflow = 1; continue; }
            eg_bkt[r][c][eg_bn[r][c]++] = (short)((eg_n << 6) | i);
        }
    }
    eg_n++;
}

// rebuild the pool around (qx,qy): the renderer's two enumerations, anchored on the
// query instead of the view. Highways gather hubs ±5 hub cells (lune coverage for any
// pair near the anchor); feeders gather towns ±6 node cells (the draw margin). Links
// that can't reach the coverage box are skipped before link_path (the expensive part).
#define EG_HWN 160
static void eg_build(float qx, float qy) {
    eg_n = 0; eg_overflow = 0;
    for (int r = 0; r < EG_GRID; r++) for (int c = 0; c < EG_GRID; c++) eg_bn[r][c] = 0;
    int cs = NODE_CS;
    int acx = wn_ifloor(qx / cs), acy = wn_ifloor(qy / cs);
    eg_x0 = acx * cs - EG_MARGIN; eg_y0 = acy * cs - EG_MARGIN;
    eg_bs = (cs + 2 * EG_MARGIN) / EG_GRID;

    // 1. HIGHWAYS — β-skeleton over hubs around the anchor (draw_highways' rule)
    static float hx[EG_HWN], hy[EG_HWN]; static int hrk[EG_HWN]; int N = 0;
    int h0 = wn_ifloor(qx / HUB_CS) - 5, h1 = wn_ifloor(qx / HUB_CS) + 5;
    int v0 = wn_ifloor(qy / HUB_CS) - 5, v1 = wn_ifloor(qy / HUB_CS) + 5;
    for (int cx = h0; cx <= h1; cx++)
        for (int cy = v0; cy <= v1 && N < EG_HWN; cy++) {
            float x, y; if (!get_hub(cx, cy, &x, &y)) continue;
            hx[N] = x; hy[N] = y; hrk[N] = hub_rank(cx, cy); N++;
        }
    float beta = hw_beta(), maxd2 = (3.2f * HUB_CS) * (3.2f * HUB_CS);
    float infl = MAXBEND * 1.5f + 60.0f;
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++) {
            float ax = hx[i], ay = hy[i], bx = hx[j], by = hy[j];
            float ab2 = (bx-ax)*(bx-ax) + (by-ay)*(by-ay);
            if (ab2 > maxd2 || ab2 < 1) continue;
            if (!eg_reach(ax, ay, bx, by, infl)) continue;
            int linked = 1;
            for (int k = 0; k < N; k++) {
                if (k == i || k == j) continue;
                float ca2 = (hx[k]-ax)*(hx[k]-ax) + (hy[k]-ay)*(hy[k]-ay);
                float cb2 = (hx[k]-bx)*(hx[k]-bx) + (hy[k]-by)*(hy[k]-by);
                if (ca2 + cb2 < beta * ab2) { linked = 0; break; }
            }
            if (!linked) continue;
            int cls = (hrk[i] == RK_METRO || hrk[j] == RK_METRO) ? CL_MOTORWAY : CL_HIGHWAY;
            if (link_path(2*ax-bx, 2*ay-by, ax, ay, bx, by, 2*bx-ax, 2*by-ay)) eg_add(cls);
        }

    // 2. FEEDERS — the towns' two links each (draw_feeders' rule)
    int m = 6;
    for (int cx = acx - m; cx <= acx + m; cx++)
        for (int cy = acy - m; cy <= acy + m; cy++) {
            float tx, ty; if (!get_node(cx, cy, &tx, &ty)) continue;
            int tr = town_rank(cx, cy);
            float hx2, hy2, nx, ny;
            if (nearest_hub(tx, ty, &hx2, &hy2) && eg_reach(tx, ty, hx2, hy2, infl)) {
                int cls = (tr == RK_TOWN) ? CL_ARTERIAL : CL_STREET;
                if (link_path(2*tx-hx2, 2*ty-hy2, tx, ty, hx2, hy2, 2*hx2-tx, 2*hy2-ty)) eg_add(cls);
            }
            if (nearest_town(tx, ty, &nx, &ny) && eg_reach(tx, ty, nx, ny, infl)) {
                int nr = town_rank(wn_ifloor(nx / NODE_CS), wn_ifloor(ny / NODE_CS));
                int cls = (tr == RK_TOWN || nr == RK_TOWN) ? CL_STREET : CL_DIRT;
                if (link_path(2*tx-nx, 2*ty-ny, tx, ty, nx, ny, 2*nx-tx, 2*ny-ty)) eg_add(cls);
            }
        }
}

static void eg_ensure(float qx, float qy) {
    int cx = wn_ifloor(qx / NODE_CS), cy = wn_ifloor(qy / NODE_CS);
    float p[7] = { P_hub_space, P_town_space, P_hub_dens, P_town_dens, P_jitter, P_web, P_sea };
    int same = eg_valid && cx == eg_key.cx && cy == eg_key.cy && eg_key.seed == seedZ;
    for (int i = 0; same && i < 7; i++) if (eg_key.p[i] != p[i]) same = 0;
    if (same) return;
    for (int i = 0; i < 7; i++) eg_key.p[i] = p[i];
    eg_key.seed = seedZ; eg_key.cx = cx; eg_key.cy = cy; eg_valid = 1;
    eg_build(qx, qy);
}

typedef struct { float bw, ba; int cw, ca, pave; } EgBest;   // within-hw best · any best
static void eg_test(int e, int i, float x, float y, EgBest *b) {
    const REdge *ed = &eg_e[e];
    float d2 = seg_d2(x, y, ed->x[i], ed->y[i], ed->x[i+1], ed->y[i+1]);
    float hw = ROAD_HW_M[ed->cls];
    if (d2 < b->ba) { b->ba = d2; b->ca = ed->cls; }
    if (d2 <= hw * hw && d2 < b->bw) { b->bw = d2; b->cw = ed->cls; }
    if (d2 <= (hw + EG_PAVE) * (hw + EG_PAVE)) b->pave = 1;
}

// THE QUERY — one call answers surface + class + collision for every tier.
static WnHit wn_road_at(float x, float y) {
    WnHit r; r.on_road = 0; r.on_pave = 0; r.cls = CL_DIRT; r.zone = 0;
    r.net = NET_ROAD; r.dist = 1e9f;
    eg_ensure(x, y);
    EgBest b = { 1e30f, 1e30f, -1, -1, 0 };
    int qc = (int)((x - eg_x0) / eg_bs), qr = (int)((y - eg_y0) / eg_bs);
    if (!eg_overflow && x >= eg_x0 && y >= eg_y0 && qc < EG_GRID && qr < EG_GRID) {
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                int c = qc + dc, rr = qr + dr;
                if (c < 0 || rr < 0 || c >= EG_GRID || rr >= EG_GRID) continue;
                for (int k = 0; k < eg_bn[rr][c]; k++) {
                    short ref = eg_bkt[rr][c][k];
                    eg_test(ref >> 6, ref & 63, x, y, &b);
                }
            }
    } else {                                          // overflow / off-box → brute (still correct)
        for (int e = 0; e < eg_n; e++)
            for (int i = 0; i < LINK_SAMPLES; i++) eg_test(e, i, x, y, &b);
    }
    if (b.cw >= 0) { r.on_road = 1; r.cls = b.cw; }
    else if (b.ca >= 0) r.cls = b.ca;
    r.on_pave = b.pave;
    r.zone = r.cls;                                   // land-use stub until the content rung
    if (b.ba < 1e30f) r.dist = fsqrt(b.ba);
    return r;
}

// SPAWN helper: the nearest cached road SAMPLE to (qx,qy) + the road's heading there —
// drop a car ON the network, facing along it. Returns 0 if no edge is in reach.
static int wn_nearest_road_point(float qx, float qy, float *ox, float *oy, float *oang) {
    eg_ensure(qx, qy);
    float best = 1e30f; int be = -1, bi = 0;
    for (int e = 0; e < eg_n; e++)
        for (int i = 0; i <= LINK_SAMPLES; i++) {
            float ddx = eg_e[e].x[i] - qx, ddy = eg_e[e].y[i] - qy;
            float dd = ddx*ddx + ddy*ddy;
            if (dd < best) { best = dd; be = e; bi = i; }
        }
    if (be < 0) return 0;
    *ox = eg_e[be].x[bi]; *oy = eg_e[be].y[bi];
    int i0 = bi < LINK_SAMPLES ? bi : bi - 1;
    *oang = angle_to((int)eg_e[be].x[i0], (int)eg_e[be].y[i0],
                     (int)eg_e[be].x[i0+1], (int)eg_e[be].y[i0+1]);
    return 1;
}

#endif // WORLDNET_H
