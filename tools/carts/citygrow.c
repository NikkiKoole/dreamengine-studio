/* de:meta
{
  "title": "city grow",
  "status": "active",
  "created": "2026-07-06",
  "kind": [
    "tech-demo",
    "generative"
  ],
  "teaches": [
    "noise-terrain",
    "road-network"
  ],
  "lineage": "The worldgen-plan rung 2-5 BENCH (docs/design/worldgen-plan.md §Where the code lives): the settlement/street generation grammar grown in its own knobbed sandbox, over the same worldnet.h spine roadnet2 and sloop share, then extracted (citygen.h) once calibrated. Rung 2 first: the population-density field.",
  "description": "The DENSITY-FIELD bench (worldgen-plan rungs 2-5) - the master input both pillar papers (Parish-Muller, Chen) drive everything from, grown here as a knobbed instrument. RUNG 2, the field: a deterministic population field D(x,y) says WHERE PEOPLE LIVE - regional noise shaped by terrain (people live LOW and FLAT; hills and rock drain it), pulled toward water (coasts and rivers attract harbours and river towns - the same water that is itself uninhabitable), fading out past the ~500 km world rim (one characterful place, not an endless smear). Settlement candidates stay on worldnet.h's suppression lattice (blue-noise spacing survives), but PRESENCE, RANK and SIZE come from the field - cities cluster along coasts and valley floors, hamlets thin toward the wilderness, and a city's EXTENT is a density threshold contour, not a radius. RUNG 3, the arterials: press T over any city and it becomes a CITY VIEW - a per-city TENSOR FIELD (Chen 2008: radial-around-the-core + grid-aligned-to-the-ENTERING-HIGHWAY read live from the worldnet graph + terrain-contour alignment + noise, weights hashed per city) traced as hyperstreamlines of both perpendicular eigen-families with Jobard-Lefer-style separation, wholesale and cached because a city is BOUNDED. The arterials visibly shear along coastlines, wrap around lakes, and orient to their highway gate - every city its own anatomy, same seed same city, always. X exports the traced graph as sndi-check JSON (crossings split, nodes welded) - the first generated network the rung-0 oracle can hold against a real city (first measurement: mean degree 2.65 vs Amersfoort's 2.71; the dead-end rim stubs and missing T-share are rung-5 calibration targets, now measurable). Sliders re-roll everything live; F heat, E extent contours, O the field-vs-hash A/B, B the world rim, SPACE hub-scale hops, R reseed.",
  "todo": [
    "rung 4: district polygons (the arterial graph already partitions the extent) + per-district minor fill via the morph knobs",
    "rung 5: the live SNDi panel + an OSM target beside it; calibration targets already measured: trim dangling rim stubs (44% deg-1), grow T-share (real cities are T-dominant), density-modulated d_sep",
    "true Jobard-Lefer distance separation (the id-grid is coarse; spacing jitters between 0.7x and 1.4x d_sep)",
    "wire the calibrated field into worldnet.h (get_node/get_hub presence/rank from D) so roadnet2 + sloop's world grows organic settlements - the rung-5.5 extraction; arterials become drivable there (the deferred drive-it gate)",
    "bridgehead attractors: crossings concentrate traffic -> density (needs rivers as network edges)",
    "PERF: draw() recomputes the terrain/density noise per-pixel every frame - attach-profiler shows ~89% of cart CPU in noise3 (63.7%) + noise2 (16.4%) + height_at (8.8%), all via draw>density_at>height_at>noise3 and draw>draw_terrain>height_at. Cache height_at/density_at into a buffer that only recomputes on map/seed/pan/zoom change instead of every frame."
  ]
}
de:meta */
#include "studio.h"
#include "worldnet.h"   // the shared spine: terrain, lattice, hash — the field shapes ITS lattice
#include "ui.h"
#include <stdio.h>
#include <math.h>       // powf — the contrast gamma

// ============================================================================
// CITY GROW — the worldgen rung-2..5 BENCH. This file is where the generation
// grammar gets grown and calibrated, per docs/design/worldgen-plan.md §Where
// the code lives: knobbed, seed-rerollable, instant visual feedback, and NEVER
// a second home for world state — everything here is a pure function over the
// same worldnet.h spine the real carts drive. Extraction target: citygen.h
// (rung 5.5), plus the density field wiring INTO worldnet.h's get_node/get_hub.
//
// RUNG 2 (this build): THE POPULATION-DENSITY FIELD — the master input.
//   D(x,y) = regional noise · terrain shaping (low+flat) · water pull · rim fade
// Settlements keep the suppression lattice (blue-noise spacing) but presence,
// rank and size come from D. City extent = the D > extent contour.
//
//   drag sliders   field re-rolls live      ROLL / EXPLORE
//   ◄▲▼► / WASD    pan     wheel zoom       SPACE jump   R new seed
//   F density heat overlay    E extent contours    N settlement dots
//   O settlements: FIELD (rung 2) vs pure-HASH (the old lattice) — the A/B
//   B world-rim ring    G lattice grid    M setup panel    H hide hud
// ============================================================================

#define TILE 4
#define ZMIN 0.0008f
#define ZMAX 0.5f
#define OVERVIEW_ZOOM 0.0012f         // settlement-map zoom (~65 km view) — the rung-2 money view
#define SPAWN_X 40000.0f
#define SPAWN_Y 30000.0f
#define PANEL_W 96

static int   mode = 0;                // 0 = setup panel, 1 = explore
static float camX, camY, jumpN;
static float zoom = OVERVIEW_ZOOM;
static float P = TILE;
static int   vcols, vrows;
static int   dragging = 0, drag_px, drag_py;
static int   show_heat = 1, show_extent = 1, show_dots = 1, show_bound = 0;
static int   show_grid = 0, show_hud = 1;
static int   show_dist = 0;                // rung 4: district faces overlay (K)
static int   use_field = 1;           // 1 = rung-2 field settlements · 0 = old pure-hash (the A/B)

static void view_metrics(void) {
    P = TILE * zoom;
    vcols = (int)(SCREEN_W / P) + 3;
    vrows = (int)(SCREEN_H / P) + 3;
}
static int sxp(float wx) { return (int)((wx - camX) * P + P * 0.5f); }
static int syp(float wy) { return (int)((wy - camY) * P + P * 0.5f); }

// ═════════════════════════════════════════════════════════════════════════════
// THE DENSITY FIELD — rung 2's deliverable, FENCED FOR EXTRACTION into
// worldnet.h once calibrated (it becomes the presence/rank/size input of
// get_node/get_hub — the one-data-model guard says the bench only PROTOTYPES it).
//
// D(x,y) ∈ [0,1], a pure function of (x, y, seedZ, knobs) — locally evaluable
// anywhere, no global pass, seam-true like everything else on the spine.

static float K_region = 0.25f;  // regional blob size          → 20..120 km
static float K_water  = 0.60f;  // water-adjacency pull        → ×1 .. ×2.5 near coast/river
static float K_flat   = 0.65f;  // terrain shaping weight (people live LOW + FLAT)
static float K_dens   = 0.32f;  // presence threshold (left = crowded, right = sparse)
static float K_extent = 0.62f;  // city-extent contour level
static float K_pow    = 0.35f;  // contrast gamma → D^(0.5..3)

// the world bound — worldgen-plan says LOCK AT RUNG 2, leaning ~500 km (the
// float-precision note in roadnet2-plan). Implemented here as a density RIM:
// civilisation fades to wilderness over BOUND_FADE, no wall. Confirm by feel.
#define BOUND_CX 0.0f
#define BOUND_CY 0.0f
#define BOUND_R    250000.0f    // 500 km across
#define BOUND_FADE  60000.0f

static float density_at(float x, float y) {
    float h = height_at(x, y);
    if (h < 0.0f) return 0;                          // water: a constraint (density 0)...
    // regional population noise — an independent z-slice so re-rolling terrain
    // and re-rolling "where the people are" stay one seed but distinct layers
    float sc = 20000.0f + K_region * 100000.0f;      // 20..120 km blobs
    float n = 0.65f * noise3(x / sc, y / sc, seedZ + 7.7f)
            + 0.35f * noise3(x / (sc * 0.27f), y / (sc * 0.27f), seedZ + 7.7f);
    // terrain shaping: LOW is livable (1 at sea level → 0 at the rock line),
    // FLAT is livable (finite-difference slope over a 400 m probe, LAND-clamped so
    // a river channel next door doesn't read as a cliff and speckle the field)
    float lowl = 1.0f - clamp(h / 0.40f, 0, 1);
    float d = 1500.0f;      // baseline chosen to NULL the 3 km roughness octave (sin(kd)=0)
    float he = clamp(height_at(x + d, y), 0, 0.4f), hw2 = clamp(height_at(x - d, y), 0, 0.4f);
    float hs = clamp(height_at(x, y + d), 0, 0.4f), hn = clamp(height_at(x, y - d), 0, 0.4f);
    float gx = (he - hw2) / (2 * d), gy = (hs - hn) / (2 * d);
    float steep = clamp(fsqrt(gx * gx + gy * gy) * 4000.0f, 0, 1);      // ~hill flank → 1
    float shape = lowl * (1.0f - steep);
    float D = n * (1.0f - K_flat + K_flat * shape);
    // ...but water ADJACENCY attracts: real cities sit at coasts, mouths, fords
    // (worldgen-plan §Beyond roads — the harbour attractor; bridgeheads join at rung 3).
    // GRADED by how much water the probe ring sees: a big shore (coast/lake/mouth)
    // pulls harder than a thin river thread — and no binary speckle.
    int wat = 0;
    for (int i = 0; i < 8; i++)
        if (height_at(x + dx(1500.0f, i * 45.0f), y + dy(1500.0f, i * 45.0f)) < 0.0f) wat++;
    D *= 1.0f + K_water * 1.5f * clamp(wat / 4.0f, 0, 1);
    // the rim: one characterful place, thinning to wilderness past ~250 km out
    float bx = x - BOUND_CX, by = y - BOUND_CY;
    float br = fsqrt(bx * bx + by * by);
    if (br > BOUND_R) D *= clamp(1.0f - (br - BOUND_R) / BOUND_FADE, 0, 1);
    return powf(clamp(D, 0, 1), 0.5f + K_pow * 2.5f);
}

// ── FIELD-DRIVEN SETTLEMENTS — the rung-2 swap. Same lattice cell + jitter as
// worldnet's get_node/get_hub (positions identical when wired), same R=1
// priority suppression (blue-noise spacing survives) — but presence, rank and
// size come from D. A hash dither softens the threshold so the presence edge
// sprinkles instead of cutting a hard contour line of towns.

// D at a town cell's jittered spot; -1 = no candidate (water/rock)
static float town_D(int cx, int cy, float *wx, float *wy) {
    unsigned h = wn_hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    int j = jit(NODE_CS);
    float x = cx * NODE_CS + NODE_CS * 0.5f + (float)((int)((h >> 7)  % (2 * j + 1)) - j);
    float y = cy * NODE_CS + NODE_CS * 0.5f + (float)((int)((h >> 15) % (2 * j + 1)) - j);
    if (!passable(height_at(x, y))) return -1;
    *wx = x; *wy = y;
    return density_at(x, y);
}
// presence-eligible strength for suppression: D dithered by the cell hash
static float town_str(int cx, int cy) {
    float x, y, D = town_D(cx, cy, &x, &y);
    if (D < 0) return -1;
    unsigned h = wn_hash2(cx * 741103597u + 43, cy * 1597334677u + 11);
    return D * (0.70f + 0.60f * (float)(h % 1000u) / 1000.0f);   // ±30% soft edge
}
static int fsettle_town(int cx, int cy, float *wx, float *wy, int *rank, float *Dout) {
    float s = town_str(cx, cy);
    if (s < K_dens) return 0;                              // presence FROM the field
    for (int a = cx - 1; a <= cx + 1; a++)                 // R=1 suppression: strongest D wins
        for (int b = cy - 1; b <= cy + 1; b++) {
            if (a == cx && b == cy) continue;
            float t = town_str(a, b);
            if (t > s || (t == s && (a < cx || (a == cx && b < cy)))) {
                if (t >= K_dens) return 0;
            }
        }
    float D = town_D(cx, cy, wx, wy);
    *rank = (D > K_dens * 1.8f) ? RK_TOWN : RK_HAMLET;     // rank FROM the field
    *Dout = D;
    return 1;
}
// hubs (cities/metros) — the coarse lattice, higher bar, de-clustered
static float hub_D(int cx, int cy, float *wx, float *wy) {
    unsigned h = wn_hash2(cx * 374761393u + 5, cy * 668265263u + 3);
    int j = jit(HUB_CS);
    float x = cx * HUB_CS + HUB_CS * 0.5f + (float)((int)((h >> 6)  % (2 * j + 1)) - j);
    float y = cy * HUB_CS + HUB_CS * 0.5f + (float)((int)((h >> 14) % (2 * j + 1)) - j);
    if (!passable(height_at(x, y))) return -1;
    *wx = x; *wy = y;
    return density_at(x, y);
}
static int fsettle_hub(int cx, int cy, float *wx, float *wy, int *rank, float *Dout) {
    float D = hub_D(cx, cy, wx, wy);
    if (D < K_dens * 1.3f) return 0;                       // cities need real density
    for (int a = cx - 1; a <= cx + 1; a++)                 // de-cluster: strongest metro wins
        for (int b = cy - 1; b <= cy + 1; b++) {
            if (a == cx && b == cy) continue;
            float xx, yy, t = hub_D(a, b, &xx, &yy);
            if (t > D) { *rank = RK_CITY; *Dout = D; return 1; }   // demote, keep the node
        }
    *rank = (D > K_dens * 2.4f) ? RK_METRO : RK_CITY;      // rank FROM the field
    *Dout = D;
    return 1;
}
// ═══ end of the fenced field block ═══════════════════════════════════════════

// ═════════════════════════════════════════════════════════════════════════════
// RUNG 3 — TENSOR-FIELD ARTERIALS PER CITY (fenced for extraction). Chen 2008:
// streets are hyperstreamlines of a 2D symmetric tensor field; the field is
// LOCALLY EVALUABLE (the reason it was chosen — worldgen-plan §The method):
//   blend, in double-angle space (tensors have period π, not 2π):
//     radial-around-centre (strong near the core, fading out)
//   + grid-aligned-to-the-ENTERING-HIGHWAY (read from the live worldnet graph)
//   + terrain-contour alignment (perpendicular to the gradient, weighted by slope)
//   + a low-frequency noise wobble
// then trace BOTH eigenvector families (θ and θ+90°) from seeded points with
// Jobard-Lefer-style separation (a per-family id-grid — coarse but deterministic;
// true distance checks are a bench upgrade, not a blocker). A city is BOUNDED,
// so the whole arterial set is traced wholesale on first touch and cached,
// seeded by the hub cell's hash — chunk-safe by construction. Streamlines stop
// where the POPULATION stops (D fades below the extent floor — Parish-Müller's
// "streets stop at zero population"), at water, or at a same-family neighbour.

#define AR_STEP   40.0f      // integration step (m)
#define AR_DSEP   450.0f     // target arterial spacing (m)
#define AR_MAXL   120        // streamlines per city
#define AR_MAXP   220        // points per streamline (≈ 8.8 km)
#define AR_MAXS   512        // seed queue
#define AR_OG     48         // separation id-grid cells per axis

static int   ar_on = 0;                    // city view active
static int   ar_ccx, ar_ccy;               // the city's hub cell — identity + seed
static float ar_cx, ar_cy, ar_R, ar_D;     // centre (m), extent cap, D at centre
static float ar_hw_ang; static int ar_has_hw;   // entering-highway bearing (rad)
static float ar_wrad, ar_wgrid;            // per-city hashed weights
static int   ar_nl; static short ar_np[AR_MAXL];
static unsigned char ar_lfam[AR_MAXL];
static float ar_px[AR_MAXL][AR_MAXP], ar_py[AR_MAXL][AR_MAXP];
static short ar_occ[2][AR_OG][AR_OG];      // separation: line id per cell per family
static float ar_ox, ar_oy, ar_cs;          // id-grid origin + cell size
static int   ar_valid = 0;
static float ar_key[7];                    // (seed + knobs) snapshot → retrace
static float map_camX, map_camY, map_zoom; // where T left the map

// ── Rung 4: the arterial GRAPH + the district faces it encloses ─────────────
// ONE data model (the worldgen-plan non-negotiable — v1 died of two diverging
// street reps): the JSON export (X) AND the district-face extraction (K) are both
// built from THIS graph, never recomputed two different ways. nodes = streamline
// endpoints + family×family crossings (5 m weld); edges = arterial segments
// between them (with provenance back to their streamline, for the JSON pts);
// faces = the planar regions those edges enclose = the districts rung 4 fills.
#define AX_MAXCUT 64          // crossing cuts recorded per streamline
#define GN_MAX    8192        // welded graph nodes (≥ old export capacity)
#define GE_MAX    8192        // graph edges (arterial segments between crossings)
#define FACE_MAX  384         // districts kept
#define FACEV_MAX 64          // stored corners per district (outer face is dropped)
static int   g_valid = 0;
static float gnx[GN_MAX], gny[GN_MAX]; static int g_nn;
static int   gea[GE_MAX], geb[GE_MAX]; static int g_ne;
static short ge_line[GE_MAX]; static float ge_t0[GE_MAX], ge_t1[GE_MAX]; // → JSON pts
static int   fc_n; static short fc_nv[FACE_MAX];
static short fc_v[FACE_MAX][FACEV_MAX];
static float fc_cx[FACE_MAX], fc_cy[FACE_MAX], fc_area[FACE_MAX];
static unsigned char fc_pat[FACE_MAX];      // rung 4.2: the fill pattern chosen per district

// ── Rung 4.2/4.3: the per-district minor-street fill ────────────────────────
// Each district face gets a LOCAL lattice (block-scale) clipped inside the face
// (inset off the bounding arterials), connected by ONE of streetlab's morphs
// chosen per district. streetlab's five patterns are toy-grid code paths; here
// they're re-targeted onto an arbitrary polygon as presets biased by density D:
// dense core → grid, mid → organic/superblock, fringe → cul-de-sac dead-ends.
enum { MP_GRID, MP_ORGANIC, MP_CULDESAC, MP_SUPERBLOCK, MP_N };
#define MS_STEP   95.0f       // minor-street block spacing (m) — 80–110 m real blocks
#define MS_INSET  50.0f       // clearance inside the bounding arterials (half-width + margin)
#define MS_LG     18          // local lattice cap per district axis
#define MS_MAX    12000       // minor nodes over all districts
#define ME_MAX    24000       // minor edges over all districts
static float msx[MS_MAX], msy[MS_MAX]; static int ms_n;
static int   mea[ME_MAX], meb[ME_MAX]; static unsigned char me_pat[ME_MAX]; static int me_n;

static float ar_theta(float x, float y, int fam) {
    float vx = 0, vy = 0;
    float ddx = x - ar_cx, ddy = y - ar_cy;
    float dist = fsqrt(ddx * ddx + ddy * ddy) + 1.0f;
    float thr = atan2f(ddy, ddx);                        // radial family (spokes + rings)
    float wr = ar_wrad / (1.0f + dist / 1500.0f);        // strong near the core
    vx += wr * cosf(2 * thr); vy += wr * sinf(2 * thr);
    if (ar_has_hw) {                                     // the entering highway's grid
        vx += ar_wgrid * cosf(2 * ar_hw_ang);
        vy += ar_wgrid * sinf(2 * ar_hw_ang);
    }
    float d = 1500.0f;                                   // terrain: follow the CONTOURS
    float gx = (clamp(height_at(x + d, y), 0, 0.4f) - clamp(height_at(x - d, y), 0, 0.4f)) / (2 * d);
    float gy = (clamp(height_at(x, y + d), 0, 0.4f) - clamp(height_at(x, y - d), 0, 0.4f)) / (2 * d);
    float steep = clamp(fsqrt(gx * gx + gy * gy) * 4000.0f, 0, 1);
    if (steep > 0.03f) {
        float tht = atan2f(gy, gx) + 1.5708f;            // contour = ⟂ gradient
        float wt = 1.3f * steep;
        vx += wt * cosf(2 * tht); vy += wt * sinf(2 * tht);
    }
    float thn = noise3(x / 9000.0f, y / 9000.0f, seedZ + 3.3f) * 6.2832f;
    vx += 0.15f * cosf(2 * thn); vy += 0.15f * sinf(2 * thn);
    float th = 0.5f * atan2f(vy, vx);
    return fam ? th + 1.5708f : th;
}

static int ar_in_city(float x, float y) {
    float ddx = x - ar_cx, ddy = y - ar_cy;
    if (ddx * ddx + ddy * ddy > ar_R * ar_R) return 0;   // hard extent cap
    if (height_at(x, y) < 0.0f) return 0;                // water: rung 3 has no bridges
    return density_at(x, y) > K_extent * 0.45f;          // streets stop where people stop
}

static int ar_cell(float x, float y, int *r, int *c) {
    int cc = (int)((x - ar_ox) / ar_cs), rr = (int)((y - ar_oy) / ar_cs);
    if (x < ar_ox || y < ar_oy || cc >= AR_OG || rr >= AR_OG) return 0;
    *r = rr; *c = cc;
    return 1;
}

// trace one streamline (both directions from the seed), enforcing separation
static void ar_trace(float sx0, float sy0, int fam) {
    if (ar_nl >= AR_MAXL) return;
    int r, c;
    if (!ar_in_city(sx0, sy0) || !ar_cell(sx0, sy0, &r, &c)) return;
    if (ar_occ[fam][r][c] >= 0) return;                  // someone's here already
    int id = ar_nl;
    float tx[AR_MAXP], ty[AR_MAXP];                      // temp: backward half, reversed later
    int nb = 0, nf = 0;
    for (int dir = 0; dir < 2; dir++) {                  // 0 = backward, 1 = forward
        float x = sx0, y = sy0;
        float th = ar_theta(x, y, fam);
        float hx = cosf(th), hy = sinf(th);
        if (!dir) { hx = -hx; hy = -hy; }
        for (int s = 0; s < AR_MAXP / 2 - 1; s++) {
            th = ar_theta(x, y, fam);
            float dxs = cosf(th), dys = sinf(th);
            if (dxs * hx + dys * hy < 0) { dxs = -dxs; dys = -dys; }   // continuity
            hx = dxs; hy = dys;
            x += dxs * AR_STEP; y += dys * AR_STEP;
            if (!ar_in_city(x, y) || !ar_cell(x, y, &r, &c)) break;
            if (ar_occ[fam][r][c] >= 0 && ar_occ[fam][r][c] != id) break;   // separation
            ar_occ[fam][r][c] = (short)id;
            if (dir) { ar_px[id][AR_MAXP / 2 + nf] = x; ar_py[id][AR_MAXP / 2 + nf] = y; nf++; }
            else     { tx[nb] = x; ty[nb] = y; nb++; }
        }
    }
    if (nb + nf < 6) return;                             // too short to be an arterial
    for (int i = 0; i < nb; i++) {                       // stitch: reversed-back + seed + fwd
        ar_px[id][AR_MAXP / 2 - 1 - nb + i] = tx[nb - 1 - i];
        ar_py[id][AR_MAXP / 2 - 1 - nb + i] = ty[nb - 1 - i];
    }
    ar_px[id][AR_MAXP / 2 - 1] = sx0; ar_py[id][AR_MAXP / 2 - 1] = sy0;
    int start = AR_MAXP / 2 - 1 - nb, count = nb + 1 + nf;
    for (int i = 0; i < count; i++) {                    // compact to the front
        ar_px[id][i] = ar_px[id][start + i];
        ar_py[id][i] = ar_py[id][start + i];
    }
    ar_np[id] = (short)count; ar_lfam[id] = (unsigned char)fam;
    ar_nl++;
}

// the entering highway: nearest big-class edge in the worldnet cache, else the
// bearing toward the nearest worldnet hub, else a hashed angle. Deterministic.
static void ar_find_highway(void) {
    eg_ensure(ar_cx, ar_cy);
    ar_has_hw = 0;
    float best = 1e30f;
    for (int e = 0; e < eg_n; e++) {
        if (eg_e[e].cls > CL_HIGHWAY) continue;
        for (int i = 0; i < LINK_SAMPLES; i++) {
            float mx = (eg_e[e].x[i] + eg_e[e].x[i + 1]) * 0.5f - ar_cx;
            float my = (eg_e[e].y[i] + eg_e[e].y[i + 1]) * 0.5f - ar_cy;
            float dd = mx * mx + my * my;
            if (dd < best && dd < 20000.0f * 20000.0f) {
                best = dd;
                ar_hw_ang = atan2f(eg_e[e].y[i + 1] - eg_e[e].y[i],
                                   eg_e[e].x[i + 1] - eg_e[e].x[i]);
                ar_has_hw = 1;
            }
        }
    }
    if (!ar_has_hw) {
        int hc = wn_ifloor(ar_cx / HUB_CS), hr = wn_ifloor(ar_cy / HUB_CS);
        float bx, by, bd = 1e30f;
        for (int a = hc - 2; a <= hc + 2; a++)
            for (int b = hr - 2; b <= hr + 2; b++) {
                float wx, wy; if (!get_hub(a, b, &wx, &wy)) continue;
                float dd = (wx - ar_cx) * (wx - ar_cx) + (wy - ar_cy) * (wy - ar_cy);
                if (dd > 1 && dd < bd) { bd = dd; bx = wx; by = wy; }
            }
        if (bd < 1e30f) { ar_hw_ang = atan2f(by - ar_cy, bx - ar_cx); ar_has_hw = 1; }
        else { ar_hw_ang = (float)(wn_hash2(ar_ccx, ar_ccy) % 314u) * 0.01f; ar_has_hw = 1; }
    }
}

// trace the whole city: seeds = centre + the highway entry, then Jobard-Lefer
// candidates pushed ±d_sep perpendicular off every traced line (+ a crossing-
// family seed on the line) until the queue drains. Wholesale, cached, hash-seeded.
static void ar_build(void) {
    ar_nl = 0;
    g_valid = 0;                            // the graph + faces derive from the arterials
    for (int f = 0; f < 2; f++)
        for (int rr = 0; rr < AR_OG; rr++)
            for (int cc = 0; cc < AR_OG; cc++) ar_occ[f][rr][cc] = -1;
    ar_ox = ar_cx - 1.2f * ar_R; ar_oy = ar_cy - 1.2f * ar_R;
    ar_cs = (2.4f * ar_R) / AR_OG;
    unsigned h = wn_hash2(ar_ccx * 2246822519u + 3, ar_ccy * 3266489917u + 29);
    ar_wrad  = 0.35f + (float)(h % 100u) / 100.0f * 0.9f;          // hashed per city
    ar_wgrid = 0.45f + (float)((h >> 8) % 100u) / 100.0f * 1.0f;
    ar_find_highway();

    static float qx[AR_MAXS], qy[AR_MAXS]; static unsigned char qf[AR_MAXS];
    int qh = 0, qt = 0;
    qx[qt] = ar_cx; qy[qt] = ar_cy; qf[qt++] = 0;                  // the core, both families
    qx[qt] = ar_cx; qy[qt] = ar_cy; qf[qt++] = 1;
    float ex = ar_cx + cosf(ar_hw_ang) * ar_R * 0.7f;              // the highway gate
    float ey = ar_cy + sinf(ar_hw_ang) * ar_R * 0.7f;
    qx[qt] = ex; qy[qt] = ey; qf[qt++] = 0;
    qx[qt] = ex; qy[qt] = ey; qf[qt++] = 1;
    while (qh < qt && ar_nl < AR_MAXL) {
        float sx0 = qx[qh], sy0 = qy[qh]; int fam = qf[qh]; qh++;
        int before = ar_nl;
        ar_trace(sx0, sy0, fam);
        if (ar_nl == before) continue;
        int id = ar_nl - 1;
        for (int i = 6; i < ar_np[id] - 6 && qt < AR_MAXS - 3; i += 11) {   // every ~d_sep
            float ax = ar_px[id][i], ay = ar_py[id][i];
            float tx2 = ar_px[id][i + 1] - ax, ty2 = ar_py[id][i + 1] - ay;
            float L = fsqrt(tx2 * tx2 + ty2 * ty2); if (L < 1) continue;
            float px2 = -ty2 / L, py2 = tx2 / L;
            qx[qt] = ax + px2 * AR_DSEP; qy[qt] = ay + py2 * AR_DSEP; qf[qt++] = (unsigned char)fam;
            qx[qt] = ax - px2 * AR_DSEP; qy[qt] = ay - py2 * AR_DSEP; qf[qt++] = (unsigned char)fam;
            qx[qt] = ax; qy[qt] = ay; qf[qt++] = (unsigned char)!fam;       // the crossing family
        }
    }
    ar_valid = 1;
}

static void ar_ensure(void) {
    float k[7] = { seedZ, K_extent, K_dens, K_region, K_water, K_flat, K_pow };
    int same = ar_valid;
    for (int i = 0; same && i < 7; i++) if (ar_key[i] != k[i]) same = 0;
    if (same) return;
    for (int i = 0; i < 7; i++) ar_key[i] = k[i];
    ar_build();
}

// enter the city view: the strongest field-hub near the view centre
static int ar_pick_city(void) {
    float vx0 = camX + SCREEN_W * 0.5f / P, vy0 = camY + SCREEN_H * 0.5f / P;
    int hc = wn_ifloor(vx0 / HUB_CS), hr = wn_ifloor(vy0 / HUB_CS);
    float bestD = -1;
    for (int a = hc - 2; a <= hc + 2; a++)
        for (int b = hr - 2; b <= hr + 2; b++) {
            float wx, wy, D; int rk;
            if (!fsettle_hub(a, b, &wx, &wy, &rk, &D)) continue;
            float dd = (wx - vx0) * (wx - vx0) + (wy - vy0) * (wy - vy0);
            float sc = D - fsqrt(dd) * 6e-6f;            // strongest, biased toward the view
            if (sc > bestD) {
                bestD = sc;
                ar_ccx = a; ar_ccy = b; ar_cx = wx; ar_cy = wy; ar_D = D;
            }
        }
    if (bestD < 0) return 0;
    ar_R = 2200.0f + 3200.0f * ar_D;                     // SIZE from the field (rung 2)
    ar_valid = 0;
    return 1;
}

// ── the shared graph builder (feeds BOTH the JSON export and the faces) ──────
// crossings between the two families → welded nodes → arterial segments split at
// every crossing. Edge order + node ids are IDENTICAL to the old inline export
// (same crossing pass, same lid weld pass), so citygrow-graph.json is unchanged.
static void ar_graph(void) {
    static float cutt[AR_MAXL][AX_MAXCUT]; static int ncut[AR_MAXL];
    for (int l = 0; l < ar_nl; l++) ncut[l] = 0;
    for (int a = 0; a < ar_nl; a++)                      // crossings between the two families
        for (int b = a + 1; b < ar_nl; b++) {
            if (ar_lfam[a] == ar_lfam[b]) continue;      // same family never crosses (separation)
            for (int i = 0; i < ar_np[a] - 1; i++)
                for (int j = 0; j < ar_np[b] - 1; j++) {
                    float x1 = ar_px[a][i], y1 = ar_py[a][i], x2 = ar_px[a][i + 1], y2 = ar_py[a][i + 1];
                    float x3 = ar_px[b][j], y3 = ar_py[b][j], x4 = ar_px[b][j + 1], y4 = ar_py[b][j + 1];
                    if ((x1 < x3 - 60 && x2 < x3 - 60 && x1 < x4 - 60 && x2 < x4 - 60) ||
                        (x1 > x3 + 60 && x2 > x3 + 60 && x1 > x4 + 60 && x2 > x4 + 60) ||
                        (y1 < y3 - 60 && y2 < y3 - 60 && y1 < y4 - 60 && y2 < y4 - 60) ||
                        (y1 > y3 + 60 && y2 > y3 + 60 && y1 > y4 + 60 && y2 > y4 + 60)) continue;
                    float den = (x2 - x1) * (y4 - y3) - (y2 - y1) * (x4 - x3);
                    if (den < 1e-6f && den > -1e-6f) continue;
                    float t = ((x3 - x1) * (y4 - y3) - (y3 - y1) * (x4 - x3)) / den;
                    float u = ((x3 - x1) * (y2 - y1) - (y3 - y1) * (x2 - x1)) / den;
                    if (t < 0 || t > 1 || u < 0 || u > 1) continue;
                    if (ncut[a] < AX_MAXCUT) cutt[a][ncut[a]++] = i + t;
                    if (ncut[b] < AX_MAXCUT) cutt[b][ncut[b]++] = j + u;
                }
        }
    g_nn = 0; g_ne = 0;
    #define G_NODE(X, Y, OUT) do {                                             \
        int _k = -1;                                                           \
        for (int _i = 0; _i < g_nn; _i++) {                                    \
            float _dx = gnx[_i] - (X), _dy = gny[_i] - (Y);                    \
            if (_dx * _dx + _dy * _dy < 25.0f) { _k = _i; break; }             \
        }                                                                      \
        if (_k < 0 && g_nn < GN_MAX) { gnx[g_nn] = (X); gny[g_nn] = (Y); _k = g_nn++; } \
        OUT = _k;                                                              \
    } while (0)
    // pass 1: assign node ids along every line (endpoints + sorted cuts)
    static int lid[AR_MAXL][AX_MAXCUT + 2]; static float lt[AR_MAXL][AX_MAXCUT + 2]; static int lnn[AR_MAXL];
    for (int l = 0; l < ar_nl; l++) {
        for (int i = 1; i < ncut[l]; i++)                // insertion sort the cut positions
            for (int j = i; j > 0 && cutt[l][j] < cutt[l][j - 1]; j--) {
                float tmp = cutt[l][j]; cutt[l][j] = cutt[l][j - 1]; cutt[l][j - 1] = tmp;
            }
        int m = 0;
        lt[l][m] = 0;
        for (int i = 0; i < ncut[l]; i++)
            if (cutt[l][i] > lt[l][m] + 0.35f && cutt[l][i] < ar_np[l] - 1.35f) lt[l][++m] = cutt[l][i];
        lt[l][++m] = (float)(ar_np[l] - 1);
        lnn[l] = m + 1;
        for (int i = 0; i < lnn[l]; i++) {
            float tt = lt[l][i]; int i0 = (int)tt; float fr = tt - i0;
            float X = ar_px[l][i0] + (ar_px[l][i0 + (i0 < ar_np[l] - 1)] - ar_px[l][i0]) * fr;
            float Y = ar_py[l][i0] + (ar_py[l][i0 + (i0 < ar_np[l] - 1)] - ar_py[l][i0]) * fr;
            G_NODE(X, Y, lid[l][i]);
        }
    }
    for (int l = 0; l < ar_nl; l++)                      // split each line into segments at its nodes
        for (int s = 0; s + 1 < lnn[l]; s++) {
            if (lid[l][s] == lid[l][s + 1]) continue;
            if (g_ne < GE_MAX) {
                gea[g_ne] = lid[l][s]; geb[g_ne] = lid[l][s + 1];
                ge_line[g_ne] = (short)l; ge_t0[g_ne] = lt[l][s]; ge_t1[g_ne] = lt[l][s + 1];
                g_ne++;
            }
        }
    #undef G_NODE
}

// ── planar face extraction: the districts the arterials enclose ──────────────
// half-edges (darts): dart d → edge e=d>>1, dir=d&1. Walk next-edge always taking
// the dart just CLOCKWISE of the reverse at each arrival node → one bounded face
// per interior region; the unbounded outer face (largest |area|) is dropped.
// Dangling rim stubs (the ~44% dead-ends) fold into the outer walk automatically.
static void ar_faces(void) {
    fc_n = 0;
    int nd = 2 * g_ne;
    if (nd < 6) return;
    static int   dfrom[2 * GE_MAX], dto[2 * GE_MAX]; static float dang[2 * GE_MAX];
    static int   drank[2 * GE_MAX]; static char dvis[2 * GE_MAX];
    static int   nadj[2 * GE_MAX]; static int nstart[GN_MAX + 1], ncnt[GN_MAX];
    for (int d = 0; d < nd; d++) {
        int e = d >> 1, dir = d & 1;
        int a = dir ? geb[e] : gea[e], b = dir ? gea[e] : geb[e];
        dfrom[d] = a; dto[d] = b;
        dang[d] = atan2f(gny[b] - gny[a], gnx[b] - gnx[a]);
        dvis[d] = 0;
    }
    for (int v = 0; v < g_nn; v++) ncnt[v] = 0;
    for (int d = 0; d < nd; d++) ncnt[dfrom[d]]++;
    nstart[0] = 0;
    for (int v = 0; v < g_nn; v++) nstart[v + 1] = nstart[v] + ncnt[v];
    for (int v = 0; v < g_nn; v++) ncnt[v] = 0;          // reused as a fill cursor
    for (int d = 0; d < nd; d++) { int v = dfrom[d]; nadj[nstart[v] + ncnt[v]++] = d; }
    for (int v = 0; v < g_nn; v++) {                     // sort each node's darts CCW by bearing
        int s = nstart[v], n = ncnt[v];
        for (int i = 1; i < n; i++)
            for (int j = i; j > 0 && dang[nadj[s + j]] < dang[nadj[s + j - 1]]; j--) {
                int tmp = nadj[s + j]; nadj[s + j] = nadj[s + j - 1]; nadj[s + j - 1] = tmp;
            }
        for (int i = 0; i < n; i++) drank[nadj[s + i]] = i;
    }
    for (int d0 = 0; d0 < nd; d0++) {                    // walk every unvisited dart into its face
        if (dvis[d0]) continue;
        int seq[FACEV_MAX]; int nv = 0, trunc = 0, steps = 0;
        float area = 0;
        int d = d0;
        do {
            dvis[d] = 1;
            int a = dfrom[d], b = dto[d];
            area += gnx[a] * gny[b] - gnx[b] * gny[a];    // full-cycle shoelace (even if not stored)
            if (nv < FACEV_MAX) seq[nv++] = a; else trunc = 1;
            int v = b, rev = d ^ 1, deg = ncnt[v];
            if (deg == 0) { trunc = 1; break; }
            int pr = (drank[rev] - 1 + deg) % deg;
            d = nadj[nstart[v] + pr];
        } while (d != d0 && ++steps < 2 * GE_MAX);
        area *= 0.5f;
        if (nv < 3) continue;
        if (fc_n < FACE_MAX) {
            fc_nv[fc_n] = trunc ? 0 : (short)nv;         // trunc = the huge outer boundary; keep only its area
            for (int i = 0; i < nv && !trunc; i++) fc_v[fc_n][i] = (short)seq[i];
            float cx = 0, cy = 0;
            for (int i = 0; i < nv; i++) { cx += gnx[seq[i]]; cy += gny[seq[i]]; }
            fc_cx[fc_n] = cx / nv; fc_cy[fc_n] = cy / nv;
            fc_area[fc_n] = area;
            fc_n++;
        }
    }
    int outer = -1; float amax = -1;                     // drop the unbounded outer face
    for (int i = 0; i < fc_n; i++) {
        float m = fc_area[i] < 0 ? -fc_area[i] : fc_area[i];
        if (m > amax) { amax = m; outer = i; }
    }
    if (outer >= 0) {                                    // swap-remove
        fc_n--;
        fc_nv[outer] = fc_nv[fc_n];
        for (int i = 0; i < fc_nv[outer]; i++) fc_v[outer][i] = fc_v[fc_n][i];
        fc_cx[outer] = fc_cx[fc_n]; fc_cy[outer] = fc_cy[fc_n]; fc_area[outer] = fc_area[fc_n];
    }
    // toss any leftover zero-vert (truncated) shells that weren't the dropped outer
    for (int i = 0; i < fc_n; ) {
        if (fc_nv[i] < 3) { fc_n--; fc_nv[i] = fc_nv[fc_n];
                            for (int k = 0; k < fc_nv[i]; k++) fc_v[i][k] = fc_v[fc_n][k];
                            fc_cx[i] = fc_cx[fc_n]; fc_cy[i] = fc_cy[fc_n]; fc_area[i] = fc_area[fc_n]; }
        else i++;
    }
}

// deterministic 0..1 hash (streetlab's hash01 twin, over the spine's wn_hash2)
static float ms_h01(int a, int b, int s) {
    unsigned h = wn_hash2(a * 92821 + s * 68917 + 1, b * 283 + 17);
    return (float)(h & 0xFFFFFF) / (float)0x1000000;
}
// even-odd point-in-face test
static int ms_pip(int fi, float px, float py) {
    int nv = fc_nv[fi], in = 0;
    for (int i = 0, j = nv - 1; i < nv; j = i++) {
        float yi = gny[fc_v[fi][i]], yj = gny[fc_v[fi][j]];
        float xi = gnx[fc_v[fi][i]], xj = gnx[fc_v[fi][j]];
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) in = !in;
    }
    return in;
}
// distance from a point to the nearest face edge (for the arterial-clearance inset)
static float ms_dedge(int fi, float px, float py) {
    int nv = fc_nv[fi]; float best = 1e18f;
    for (int i = 0, j = nv - 1; i < nv; j = i++) {
        float ax = gnx[fc_v[fi][j]], ay = gny[fc_v[fi][j]];
        float bx = gnx[fc_v[fi][i]], by = gny[fc_v[fi][i]];
        float dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
        float t = L2 > 1e-6f ? ((px - ax) * dx + (py - ay) * dy) / L2 : 0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        float qx = ax + t * dx, qy = ay + t * dy;
        float d = (px - qx) * (px - qx) + (py - qy) * (py - qy);
        if (d < best) best = d;
    }
    return fsqrt(best);
}
// the pattern for a district — density-biased preset of streetlab's morphs, hashed for variety
static int ms_pattern(int fi, unsigned *seed) {
    float d = density_at(fc_cx[fi], fc_cy[fi]);
    int cxk = (int)(fc_cx[fi] / MS_STEP), cyk = (int)(fc_cy[fi] / MS_STEP);
    unsigned h = wn_hash2(cxk * 374761393u + 1, cyk * 668265263u + 7);
    *seed = h;
    float r = (float)(h & 0xFFFF) / 65535.0f;
    if (d > 0.72f) return (r < 0.72f) ? MP_GRID : MP_ORGANIC;             // dense core: mostly grid
    if (d > 0.45f) return (r < 0.38f) ? MP_GRID
                        : (r < 0.72f ? MP_ORGANIC : MP_SUPERBLOCK);       // mid ring
    return (r < 0.55f) ? MP_CULDESAC : MP_ORGANIC;                        // fringe: dead-ends
}

static int ms_uf[MS_LG * MS_LG];
static int ms_find(int x) { while (ms_uf[x] != x) x = ms_uf[x] = ms_uf[ms_uf[x]]; return x; }

// fill ONE district: local lattice clipped to the face, connected per its pattern
static void ms_fill_face(int fi) {
    if (fc_nv[fi] < 3) return;
    unsigned seed; int pat = ms_pattern(fi, &seed);
    fc_pat[fi] = (unsigned char)pat;
    // bbox of the face
    float x0 = 1e18f, y0 = 1e18f, x1 = -1e18f, y1 = -1e18f;
    for (int i = 0; i < fc_nv[fi]; i++) {
        float x = gnx[fc_v[fi][i]], y = gny[fc_v[fi][i]];
        if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
    }
    float spanx = x1 - x0, spany = y1 - y0;
    float step = MS_STEP;                                // coarsen so the lattice fits the cap
    if (spanx / step > MS_LG - 1) step = spanx / (MS_LG - 1);
    if (spany / step > MS_LG - 1) step = spany / (MS_LG - 1);
    int cols = (int)(spanx / step) + 1, rows = (int)(spany / step) + 1;
    if (cols < 1) cols = 1; if (rows < 1) rows = 1;
    if (cols > MS_LG) cols = MS_LG; if (rows > MS_LG) rows = MS_LG;
    // lay the lattice, keep interior nodes (inside + clear of arterials)
    static int   lg[MS_LG][MS_LG];
    static float lx[MS_LG][MS_LG], ly[MS_LG][MS_LG];
    int base = ms_n, ln = 0;
    float jit = (pat == MP_ORGANIC) ? 0.40f : (pat == MP_CULDESAC ? 0.22f : 0.0f);
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            lg[r][c] = -1;
            float px = x0 + c * step, py = y0 + r * step;
            float jx = (ms_h01(c, r, (int)seed)      - 0.5f) * step * jit;
            float jy = (ms_h01(c, r, (int)seed + 97) - 0.5f) * step * jit;
            px += jx; py += jy;
            if (!ms_pip(fi, px, py)) continue;
            if (ms_dedge(fi, px, py) < MS_INSET) continue;
            if (ms_n >= MS_MAX) return;
            lg[r][c] = ms_n - base;                      // local id
            msx[ms_n] = px; msy[ms_n] = py; ms_n++;
        }
    ln = ms_n - base;
    if (ln < 2) return;
    #define ADD_EDGE(A, B) do { if (me_n < ME_MAX) { mea[me_n] = base + (A); meb[me_n] = base + (B); \
                                me_pat[me_n] = (unsigned char)pat; me_n++; } } while (0)
    if (pat == MP_GRID || pat == MP_ORGANIC) {           // full local grid
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) {
                if (lg[r][c] < 0) continue;
                if (c + 1 < cols && lg[r][c + 1] >= 0) ADD_EDGE(lg[r][c], lg[r][c + 1]);
                if (r + 1 < rows && lg[r + 1][c] >= 0) ADD_EDGE(lg[r][c], lg[r + 1][c]);
            }
    } else {                                             // cul-de-sac / superblock: Kruskal tree + loops
        int sb = 2 + (int)(ms_h01(0, 0, (int)seed) * 2.0f);   // superblock frame period 2..3
        for (int i = 0; i < ln; i++) ms_uf[i] = i;
        // superblock: the arterial FRAME first (every sb-th line, fully connected)
        if (pat == MP_SUPERBLOCK)
            for (int r = 0; r < rows; r++)
                for (int c = 0; c < cols; c++) {
                    if (lg[r][c] < 0) continue;
                    if (c + 1 < cols && lg[r][c + 1] >= 0 && (r % sb) == 0) {
                        ADD_EDGE(lg[r][c], lg[r][c + 1]); ms_uf[ms_find(lg[r][c])] = ms_find(lg[r][c + 1]); }
                    if (r + 1 < rows && lg[r + 1][c] >= 0 && (c % sb) == 0) {
                        ADD_EDGE(lg[r][c], lg[r + 1][c]); ms_uf[ms_find(lg[r][c])] = ms_find(lg[r + 1][c]); }
                }
        // gather remaining candidate edges + weights, sort, Kruskal
        static int ca[2 * MS_LG * MS_LG], cb[2 * MS_LG * MS_LG]; static float cw[2 * MS_LG * MS_LG];
        int nc = 0;
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) {
                if (lg[r][c] < 0) continue;
                int interior = (pat == MP_CULDESAC) || !((r % sb) == 0);   // superblock skips frame rows
                int interiorV = (pat == MP_CULDESAC) || !((c % sb) == 0);
                if (c + 1 < cols && lg[r][c + 1] >= 0 && interior) {
                    ca[nc] = lg[r][c]; cb[nc] = lg[r][c + 1]; cw[nc] = ms_h01(c, r, (int)seed + 5); nc++; }
                if (r + 1 < rows && lg[r + 1][c] >= 0 && interiorV) {
                    ca[nc] = lg[r][c]; cb[nc] = lg[r + 1][c]; cw[nc] = ms_h01(c, r, (int)seed + 555); nc++; }
            }
        for (int i = 1; i < nc; i++)                     // insertion sort by weight (small n)
            for (int j = i; j > 0 && cw[j] < cw[j - 1]; j--) {
                float tw = cw[j]; cw[j] = cw[j - 1]; cw[j - 1] = tw;
                int ta = ca[j]; ca[j] = ca[j - 1]; ca[j - 1] = ta;
                int tb = cb[j]; cb[j] = cb[j - 1]; cb[j - 1] = tb;
            }
        float loopf = (pat == MP_SUPERBLOCK) ? 0.06f : 0.12f;
        for (int i = 0; i < nc; i++) {
            int ra = ms_find(ca[i]), rb = ms_find(cb[i]);
            if (ra != rb) { ms_uf[ra] = rb; ADD_EDGE(ca[i], cb[i]); }
            else if (ms_h01(ca[i], cb[i], (int)seed + 999) < loopf) ADD_EDGE(ca[i], cb[i]);
        }
    }
    #undef ADD_EDGE
}

static void ms_build(void) {
    ms_n = 0; me_n = 0;
    for (int fi = 0; fi < fc_n; fi++) ms_fill_face(fi);
}

static void ar_graph_ensure(void) {
    ar_ensure();                                         // arterials current for this seed+knobs
    if (g_valid) return;
    ar_graph();
    ar_faces();
    ms_build();
    g_valid = 1;
}

// export the traced arterial graph as sndi-check's generated-graph JSON:
// nodes = streamline endpoints + crossings (5 m weld), edges carry their pts.
// Written to the cwd (build/ under the harness): citygrow-graph.json.
static void ar_export(void) {
    ar_graph_ensure();
    FILE *f = fopen("citygrow-graph.json", "w");
    if (!f) return;
    fprintf(f, "{\"nodes\":[");
    for (int i = 0; i < g_nn; i++) fprintf(f, "%s[%.1f,%.1f]", i ? "," : "", gnx[i], gny[i]);
    fprintf(f, "],\"edges\":[");
    for (int e = 0; e < g_ne; e++) {
        int a = gea[e], b = geb[e], l = ge_line[e];
        // pts BEGIN and END at the welded node coords — sndi-check rebuilds the
        // graph from pts via 1 m vertex welding, so crossing edges must SHARE
        // their endpoint vertex exactly or the crossing doesn't exist to it
        fprintf(f, "%s{\"a\":%d,\"b\":%d,\"pts\":[", e ? "," : "", a, b);
        fprintf(f, "[%.1f,%.1f]", gnx[a], gny[a]);
        int i0 = (int)floorf(ge_t0[e]) + 1, i1 = (int)ceilf(ge_t1[e]) - 1;
        for (int i = i0; i <= i1 && i < ar_np[l]; i++)
            if ((float)i > ge_t0[e] + 0.05f && (float)i < ge_t1[e] - 0.05f)
                fprintf(f, ",[%.1f,%.1f]", ar_px[l][i], ar_py[l][i]);
        fprintf(f, ",[%.1f,%.1f]]}", gnx[b], gny[b]);
    }
    fprintf(f, "]}\n");
    fclose(f);
}
// ═══ end of the fenced rung-3 block ══════════════════════════════════════════

// ── overlays ─────────────────────────────────────────────────────────────────
// With the heat on, terrain drops to a silhouette (dark land / blue water) so the
// field ramp POPS — the bench's money view. Heat off = the normal biome map.
static void draw_terrain(void) {
    int step = 4;
    for (int sy = 0; sy < SCREEN_H; sy += step)
        for (int sx = 0; sx < SCREEN_W; sx += step) {
            float h = height_at(camX + sx / P, camY + sy / P);
            int c = show_heat ? (h < 0.0f ? CLR_DARK_BLUE : CLR_DARKER_GREY) : biome_col(h);
            rectfill(sx, sy, step, step, c);
        }
}

// heat + extent share ONE field-sampling pass (D is ~13 height_at per sample)
static const int HEAT[6] = { CLR_DARK_PURPLE, CLR_DARK_RED, CLR_RED,
                             CLR_ORANGE, CLR_YELLOW, CLR_WHITE };
static void draw_field(void) {
    if (!show_heat && !show_extent) return;
    int step = 8;
    for (int sy = 0; sy < SCREEN_H; sy += step)
        for (int sx = 0; sx < SCREEN_W; sx += step) {
            float D = density_at(camX + sx / P, camY + sy / P);
            if (show_extent && D >= K_extent) {  // the CITY FOOTPRINT: a contour, not a radius
                rectfill(sx, sy, step, step, CLR_WHITE);
                fillp(FILL_CHECKER, -1);
                rectfill(sx, sy, step, step, CLR_ORANGE);
                fillp_reset();
            } else if (show_heat && D > 0.05f) {
                int i = (int)(D * 5.999f);
                rectfill(sx, sy, step, step, HEAT[i]);
            }
        }
}

static void draw_settlements(void) {
    if (!show_dots) return;
    int detail = (zoom >= 0.0009f);   // towns visible at the settlement-map overview
                                      // (continental zoom = hubs only; the field is heavy)
    if (detail) {                                // towns/hamlets (fine lattice)
        int c0 = wn_ifloor(camX / NODE_CS) - 2, c1 = wn_ifloor((camX + vcols) / NODE_CS) + 2;
        int r0 = wn_ifloor(camY / NODE_CS) - 2, r1 = wn_ifloor((camY + vrows) / NODE_CS) + 2;
        for (int cx = c0; cx <= c1; cx++)
            for (int cy = r0; cy <= r1; cy++) {
                float wx, wy, D; int rk;
                if (use_field) { if (!fsettle_town(cx, cy, &wx, &wy, &rk, &D)) continue; }
                else           { if (!get_node(cx, cy, &wx, &wy)) continue;
                                 rk = town_rank(cx, cy); D = 0.5f; }
                int px = sxp(wx), py = syp(wy);
                int town = rk == RK_TOWN;
                int r = town ? 2 : 1;
                if (use_field) r += (int)(D * 1.8f);       // SIZE from the field
                circfill(px, py, r + 2, CLR_BLACK);        // heavy ring: reads on the heat
                circfill(px, py, r, town ? CLR_GREEN : CLR_LIGHT_GREY);
            }
    }
    int h0 = wn_ifloor(camX / HUB_CS) - 2, h1 = wn_ifloor((camX + vcols) / HUB_CS) + 2;
    int v0 = wn_ifloor(camY / HUB_CS) - 2, v1 = wn_ifloor((camY + vrows) / HUB_CS) + 2;
    for (int cx = h0; cx <= h1; cx++)            // cities/metros (coarse lattice)
        for (int cy = v0; cy <= v1; cy++) {
            float wx, wy, D; int rk;
            if (use_field) { if (!fsettle_hub(cx, cy, &wx, &wy, &rk, &D)) continue; }
            else           { if (!get_hub(cx, cy, &wx, &wy)) continue;
                             rk = hub_rank(cx, cy); D = 0.5f; }
            int metro = rk == RK_METRO;
            int rr = metro ? 4 : 3;
            if (use_field) rr += (int)(D * 2.5f);          // SIZE from the field
            if (zoom < 0.0022f) rr = (rr + 1) / 2;         // LOD far out
            int px = sxp(wx), py = syp(wy);
            circfill(px, py, rr + 2, CLR_BLACK);           // heavy ring: reads on the heat
            circfill(px, py, rr, CLR_WHITE);
            if (metro && rr >= 3) circfill(px, py, 2, CLR_DARK_BLUE);
        }
}

// ── the CITY VIEW (rung 3) — the traced arterials over the terrain ───────────
static void ar_draw(void) {
    ar_ensure();
    for (int l = 0; l < ar_nl; l++)                      // casing pass, then centre pass
        for (int i = 0; i + 1 < ar_np[l]; i++) {
            int x0 = sxp(ar_px[l][i]), y0 = syp(ar_py[l][i]);
            int x1 = sxp(ar_px[l][i + 1]), y1 = syp(ar_py[l][i + 1]);
            line(x0, y0 + 1, x1, y1 + 1, CLR_DARKER_GREY);
            line(x0 + 1, y0, x1 + 1, y1, CLR_DARKER_GREY);
        }
    for (int l = 0; l < ar_nl; l++)
        for (int i = 0; i + 1 < ar_np[l]; i++)
            line(sxp(ar_px[l][i]), syp(ar_py[l][i]),
                 sxp(ar_px[l][i + 1]), syp(ar_py[l][i + 1]),
                 ar_lfam[l] ? CLR_LIGHT_GREY : CLR_WHITE);   // the two eigen-families
    int cx = sxp(ar_cx), cy = syp(ar_cy);
    circfill(cx, cy, 3, CLR_BLACK); circfill(cx, cy, 2, CLR_GREEN);   // the core
    if (ar_has_hw) {                                     // the highway-gate bearing
        int gx = sxp(ar_cx + cosf(ar_hw_ang) * ar_R * 0.9f);
        int gy = syp(ar_cy + sinf(ar_hw_ang) * ar_R * 0.9f);
        line(cx, cy, gx, gy, CLR_DARK_RED);
        circfill(gx, gy, 2, CLR_RED);
    }
    circ(cx, cy, (int)(ar_R * P), CLR_DARK_PURPLE);      // the extent cap
}

// ── Rung 4 (K): the DISTRICTS — the planar faces the arterials enclose ───────
// Each face is one district: a coloured dither wash (hashed per district so
// neighbours differ) + a bright outline + a centroid dot. This is the partition
// the per-district minor fill (rung 4.2–4.4) is generated INTO.
static void ar_dist_draw(void) {
    ar_graph_ensure();
    static const int pal[6] = { CLR_RED, CLR_ORANGE, CLR_YELLOW,
                                CLR_GREEN, CLR_BLUE, CLR_PINK };
    for (int fi = 0; fi < fc_n; fi++) {
        int nv = fc_nv[fi]; if (nv < 3 || nv > FACEV_MAX) continue;
        int xy[2 * FACEV_MAX];
        for (int i = 0; i < nv; i++) {
            xy[2 * i]     = sxp(gnx[fc_v[fi][i]]);
            xy[2 * i + 1] = syp(gny[fc_v[fi][i]]);
        }
        unsigned h = wn_hash2((int)(fc_cx[fi] * 0.02f), (int)(fc_cy[fi] * 0.02f));
        int col = pal[h % 6];
        fillp(0x5A5A, -1);                               // ~50% dither: terrain shows through
        polyfill(xy, nv, col);
        fillp_reset();
        poly(xy, nv, col);
        int cx = sxp(fc_cx[fi]), cy = syp(fc_cy[fi]);
        circfill(cx, cy, 1, CLR_WHITE);
    }
}

// ── Rung 4.2/4.3 (K): the MINOR STREETS — coloured by their district's pattern
// so a stranger can SEE neighbouring districts differ (grid vs organic vs
// dead-end vs superblock). This is the "procedural grid dies" view.
static void ms_draw(void) {
    ar_graph_ensure();
    static const int pcol[MP_N] = { CLR_WHITE, CLR_YELLOW, CLR_ORANGE, CLR_PINK };
    for (int e = 0; e < me_n; e++) {
        int a = mea[e], b = meb[e];
        line(sxp(msx[a]), syp(msy[a]), sxp(msx[b]), syp(msy[b]), pcol[me_pat[e]]);
    }
}

static void hud_city(void) {
    char buf[96];
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("CITYGROW", 4, 2, CLR_LIGHT_GREY);
    static const char *dn[4] = { "off", "faces", "minors", "both" };
    snprintf(buf, sizeof buf, "CITY %d,%d  D %.2f  lines %d  districts %d  minor %d/%d  [%s]",
             ar_ccx, ar_ccy, ar_D, ar_nl, show_dist ? fc_n : 0,
             show_dist >= 2 ? ms_n : 0, show_dist >= 2 ? me_n : 0, dn[show_dist]);
    print(buf, 84, 2, CLR_GREEN);
    print_centered("T map   X export JSON   K districts (grid/organic/deadend/superblock)   R reroll",
                   SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
}

static void draw_bound(void) {                   // the world rim (B) — ~500 km across
    if (!show_bound) return;
    int cx = sxp(BOUND_CX), cy = syp(BOUND_CY);
    int r = (int)(BOUND_R * P);
    circ(cx, cy, r, CLR_RED);
    circ(cx, cy, r + (int)(BOUND_FADE * P), CLR_DARK_RED);
}

static void draw_grid_overlay(void) {
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
    char buf[80];
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("CITYGROW", 4, 2, CLR_LIGHT_GREY);
    // D under the mouse — the bench's probe readout
    float mwx = camX + mouse_x() / P, mwy = camY + mouse_y() / P;
    snprintf(buf, sizeof buf, "D %.2f  %s", density_at(mwx, mwy),
             use_field ? "FIELD" : "HASH(old)");
    print(buf, 84, 2, use_field ? CLR_GREEN : CLR_ORANGE);
    print_centered("F heat  E extent  N dots  O field/hash  B rim  \x18\x19\x1a\x1b pan  wheel  SPACE  R  M",
                   SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
}

static void draw_setup_panel(void) {
    char buf[48];
    fillp(FILL_CHECKER, -1); rectfill(0, 0, PANEL_W, SCREEN_H, CLR_BLACK); fillp_reset();
    line(PANEL_W, 0, PANEL_W, SCREEN_H, CLR_DARK_GREY);
    print("CITYGROW", 4, 4, CLR_LIGHT_GREY);

    font(FONT_SMALL);
    ui_begin();
    static const char *L[7] = { "water", "region", "waterpull", "flat", "density",
                                "extent", "contrast" };
    float *pp[7] = { &P_sea, &K_region, &K_water, &K_flat, &K_dens, &K_extent, &K_pow };
    for (int i = 0; i < 7; i++) ui_slider(pp[i], 4, 16 + i * 12, 88, L[i]);
    int roll = ui_button(4, 104, 88, 14, "ROLL");
    int go   = ui_button(4, 122, 88, 16, "EXPLORE \x10");
    ui_end();

    snprintf(buf, sizeof buf, "seed #%u", (unsigned)(seedZ * 1000) % 100000u);
    print(buf, 4, 144, CLR_MEDIUM_GREY);
    print("drag = re-roll live", 4, SCREEN_H - 9, CLR_DARKER_GREY);
    font(FONT_NORMAL);

    if (roll) seedZ += 0.37f;
    if (go || keyp(KEY_ENTER)) mode = 1;
}

void init(void) {
    view_metrics();
    camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f;
}

void update(void) {
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
    float pan = 1.3f / zoom;
    if (btn(0, BTN_LEFT)  || key('A')) camX -= pan;
    if (btn(0, BTN_RIGHT) || key('D')) camX += pan;
    if (btn(0, BTN_UP)    || key('W')) camY -= pan;
    if (btn(0, BTN_DOWN)  || key('S')) camY += pan;
    int pan_ok = (mode == 1) || (mouse_x() > PANEL_W);
    if (mouse_pressed(MOUSE_LEFT) && pan_ok) { dragging = 1; drag_px = mouse_x(); drag_py = mouse_y(); }
    if (mouse_down(MOUSE_LEFT)) {
        if (dragging) { camX -= (mouse_x() - drag_px) / pp; camY -= (mouse_y() - drag_py) / pp;
                        drag_px = mouse_x(); drag_py = mouse_y(); }
    } else dragging = 0;

    if (keyp(KEY_SPACE) && mode == 1) {          // hub-scale hops — a fresh NEIGHBOURHOOD each press,
        jumpN += 1.0f;                           // clamped inside the world rim (there's no one out there)
        unsigned h = wn_hash2((int)(jumpN * 911), 7);
        camX += (float)((int)(h % 300000u) - 150000) + 60000.0f;
        camY += (float)((int)((h >> 11) % 300000u) - 150000) + 60000.0f;
        float ccx = camX + SCREEN_W * 0.5f / pp, ccy = camY + SCREEN_H * 0.5f / pp;
        ccx = clamp(ccx, BOUND_CX - BOUND_R * 0.85f, BOUND_CX + BOUND_R * 0.85f);
        ccy = clamp(ccy, BOUND_CY - BOUND_R * 0.85f, BOUND_CY + BOUND_R * 0.85f);
        camX = ccx - SCREEN_W * 0.5f / pp; camY = ccy - SCREEN_H * 0.5f / pp;
    }
    if (keyp('R')) { zoom = OVERVIEW_ZOOM; view_metrics();
                     camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f;
                     seedZ += 0.37f; jumpN = 0; ar_on = 0; }
    if (keyp('T') && mode == 1) {                // rung 3: the CITY VIEW ↔ the map
        if (ar_on) { ar_on = 0; camX = map_camX; camY = map_camY; zoom = map_zoom; view_metrics(); }
        else if (ar_pick_city()) {
            map_camX = camX; map_camY = camY; map_zoom = zoom;
            ar_on = 1;
            zoom = (SCREEN_H * 0.42f) / (TILE * ar_R);   // the city fills the view
            view_metrics();
            camX = ar_cx - SCREEN_W * 0.5f / (TILE * zoom);
            camY = ar_cy - SCREEN_H * 0.5f / (TILE * zoom);
        }
    }
    if (keyp('X') && ar_on) { ar_ensure(); ar_export(); }
    if (keyp('F')) show_heat   = !show_heat;
    if (keyp('E')) show_extent = !show_extent;
    if (keyp('N')) show_dots   = !show_dots;
    if (keyp('O')) use_field   = !use_field;
    if (keyp('B')) show_bound  = !show_bound;
    if (keyp('G')) show_grid   = !show_grid;
    if (keyp('K')) show_dist   = (show_dist + 1) % 4;  // rung 4: off→faces→minors→both
    if (keyp('H')) show_hud    = !show_hud;
    if (keyp('M')) mode = 0;

#ifdef DE_TRACE
    if (ar_on) {   // rung-4 gate: district count + the per-district pattern mix + minor-street size
        ar_graph_ensure();
        int pc[MP_N] = { 0, 0, 0, 0 };
        for (int i = 0; i < fc_n; i++) pc[fc_pat[i]]++;
        watch("districts", "%d", fc_n);
        watch("pat_grid",  "%d", pc[MP_GRID]);
        watch("pat_org",   "%d", pc[MP_ORGANIC]);
        watch("pat_cul",   "%d", pc[MP_CULDESAC]);
        watch("pat_super", "%d", pc[MP_SUPERBLOCK]);
        watch("minor_n",   "%d", ms_n);
        watch("minor_e",   "%d", me_n);
    }
    // deterministic probes — the rung-2 gate reads these (same seed → same field)
    watch("d_spawn", "%.4f", density_at(SPAWN_X, SPAWN_Y));
    watch("d_far",   "%.4f", density_at(SPAWN_X + 87000.0f, SPAWN_Y - 41000.0f));
    watch("field",   "%d", use_field);
    {   // field statistics over the LAND in view (calibration eyes)
        float dmin = 1, dmax = 0, dsum = 0; int nland = 0;
        for (int gy = 0; gy < 10; gy++)
            for (int gx = 0; gx < 16; gx++) {
                float x = camX + (gx + 0.5f) * vcols / 16.0f;
                float y = camY + (gy + 0.5f) * vrows / 10.0f;
                if (height_at(x, y) < 0) continue;
                float D = density_at(x, y);
                nland++; dsum += D;
                if (D < dmin) dmin = D;
                if (D > dmax) dmax = D;
            }
        watch("d_min",  "%.3f", nland ? dmin : -1.0f);
        watch("d_mean", "%.3f", nland ? dsum / nland : -1.0f);
        watch("d_max",  "%.3f", nland ? dmax : -1.0f);
        watch("nland",  "%d", nland);
    }
    {   // settlement counts over the view — the organic-map gate reads these
        int nt = 0, nh = 0, nhub = 0;
        int c0 = wn_ifloor(camX / NODE_CS), c1 = wn_ifloor((camX + vcols) / NODE_CS);
        for (int cx = c0; cx <= c1; cx++)
            for (int cy = wn_ifloor(camY / NODE_CS); cy <= wn_ifloor((camY + vrows) / NODE_CS); cy++) {
                float wx, wy, D; int rk;
                if (fsettle_town(cx, cy, &wx, &wy, &rk, &D)) { if (rk == RK_TOWN) nt++; else nh++; }
            }
        for (int cx = wn_ifloor(camX / HUB_CS); cx <= wn_ifloor((camX + vcols) / HUB_CS); cx++)
            for (int cy = wn_ifloor(camY / HUB_CS); cy <= wn_ifloor((camY + vrows) / HUB_CS); cy++) {
                float wx, wy, D; int rk;
                if (fsettle_hub(cx, cy, &wx, &wy, &rk, &D)) nhub++;
            }
        watch("towns", "%d", nt); watch("hamlets", "%d", nh); watch("hubs", "%d", nhub);
    }
#endif
}

void draw(void) {
    view_metrics();
    if (ar_on) {                                 // rung 3: one city, its arterials
        int heat = show_heat; show_heat = 0;     // biome terrain under the streets
        draw_terrain(); show_heat = heat;
        ar_draw();
        if (show_dist == 1 || show_dist == 3) ar_dist_draw();   // the partition
        if (show_dist >= 2) ms_draw();                          // the minor fill
        if (show_hud) hud_city();
        return;
    }
    draw_terrain();
    draw_field();
    if (show_grid) draw_grid_overlay();
    draw_settlements();
    draw_bound();
    if (mode == 0) draw_setup_panel();
    else if (show_hud) hud();
}
