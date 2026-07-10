// citygen.h — the calibrated procedural CITY GRAMMAR, cart-land (ADR-0006).
//
// Extracted from citygrow.c (worldgen-plan rung 5.5) once the generator passed
// rung-5 SNDi calibration (`sndi-check --compare` PASSES vs real Rotterdam).
// citygrow stays the tuning BENCH (sliders, rendering, export); THIS header is
// the shared MODEL both the bench and the spine read:
//   · rung 2  density_at(x,y)  — the population field (WHERE people live)
//   · rung 3  ar_build()       — per-city tensor-field arterials (Chen 2008)
//   · rung 4  ar_graph()/ar_faces()/ms_build() — districts + minor-street fill
//   · rung 5  the MS_LOOPF / MS_STITCHF calibration toward real Dutch cities
//
// ONE data model (the worldgen-plan non-negotiable): the SAME typed graph the
// spine's road_at consumes. Pure over worldnet.h's terrain/lattice/hash + seedZ
// + the K_* / MS_* knobs. A city is BOUNDED → generate one wholesale, and the
// CALLER caches it (per hub cell). State here is file-static: one city at a time.
//
// CONTRACT: include AFTER studio.h. Sets the current city via ar_cx/ar_cy/ar_R/
// ar_D/ar_ccx/ar_ccy (a picker like citygrow's ar_pick_city fills them), then
// ar_graph_ensure() traces + graphs + fills + caches for that (seed, knobs).
#ifndef CITYGEN_H
#define CITYGEN_H
#include "studio.h"
#include "worldnet.h"   // the spine: terrain, ranked lattice, hash, seedZ
#include <math.h>       // powf — the contrast gamma
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
#define MP_STITCH MP_N        // rung 4.4: a local street T-ing onto a bounding arterial
#define MS_STEP   155.0f      // minor-street block spacing (m) — rung-5 calibrated (int/km²)
#define MS_INSET  50.0f       // clearance inside the bounding arterials (half-width + margin)
#define MS_STITCHF 0.66f      // rung 5: fraction of perimeter locals that T onto an arterial;
                              // the rest stay real DEAD-ENDS (raises deg-1 + dendricity + circuity)
// rung 5: the DENDRICITY knob — per pattern, the fraction of non-tree (loop) edges
// added back after a spanning tree. Low = tree-like/dead-ends, high = grid. This one
// knob moves deg-1 / deg-4+ / mean-degree / circuity / dendricity together.
static const float MS_LOOPF[MP_N] = { 0.50f, 0.32f, 0.10f, 0.06f };  // grid/organic/culdesac/super
#define MS_LG     18          // local lattice cap per district axis
#define MS_MAX    12000       // minor nodes over all districts
#define ME_MAX    24000       // minor edges over all districts
static float msx[MS_MAX], msy[MS_MAX]; static int ms_n;
static int   mea[ME_MAX], meb[ME_MAX]; static unsigned char me_pat[ME_MAX]; static int me_n;
static char  art_used[AR_MAXL][AR_MAXP];    // rung 4.4: arterial vertex already has a stitch stub

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
    float r = (float)(h & 0xFFFF) / 65535.0f;   // rung-5 mix — even the core is T-dominant, not a mesh
    if (d > 0.72f) return (r < 0.40f) ? MP_GRID
                        : (r < 0.78f ? MP_ORGANIC : MP_SUPERBLOCK);       // dense core: grid + organic
    if (d > 0.45f) return (r < 0.22f) ? MP_GRID
                        : (r < 0.60f ? MP_ORGANIC
                        : (r < 0.80f ? MP_SUPERBLOCK : MP_CULDESAC));     // mid ring
    return (r < 0.60f) ? MP_CULDESAC : MP_ORGANIC;                        // fringe: dead-ends
}

static int ms_uf[MS_LG * MS_LG];
static int ms_ldeg[MS_LG * MS_LG];      // rung 5: local degree, to cap loop-created crossings at T
static int ms_find(int x) { while (ms_uf[x] != x) x = ms_uf[x] = ms_uf[ms_uf[x]]; return x; }

// fill ONE district: local lattice clipped to the face, connected per its pattern
static void ms_fill_face(int fi) {
    if (fc_nv[fi] < 3) return;
    unsigned seed; int pat = ms_pattern(fi, &seed);
    fc_pat[fi] = (unsigned char)pat;
    // a per-district ROTATED lattice, centred on the centroid, sized to the face
    // radius. The hashed orientation (rung 5) breaks the world-axis grid alignment
    // → raises orientation entropy toward real cities + reads far less mechanical.
    float cx = fc_cx[fi], cy = fc_cy[fi], rad = 0;
    for (int i = 0; i < fc_nv[fi]; i++) {
        float dx = gnx[fc_v[fi][i]] - cx, dy = gny[fc_v[fi][i]] - cy, dd = dx * dx + dy * dy;
        if (dd > rad) rad = dd;
    }
    rad = fsqrt(rad);
    float step = MS_STEP;
    int half = (int)(rad / step) + 1;
    if (2 * half + 1 > MS_LG) { half = (MS_LG - 1) / 2; step = rad / half; }
    int cols = 2 * half + 1, rows = cols;
    float ang = ms_h01((int)(cx * 0.01f), (int)(cy * 0.01f), (int)seed + 13) * 3.14159265f;
    float ca_ = cosf(ang), sa = sinf(ang);
    // lay the lattice, keep interior nodes (inside + clear of arterials)
    static int   lg[MS_LG][MS_LG];
    int base = ms_n, ln = 0;
    float jit = (pat == MP_ORGANIC) ? 0.40f : (pat == MP_CULDESAC ? 0.22f : 0.0f);
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            lg[r][c] = -1;
            float lxr = (c - half) * step, lyr = (r - half) * step;
            float px = cx + lxr * ca_ - lyr * sa;
            float py = cy + lxr * sa + lyr * ca_;
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
    for (int i = 0; i < ln; i++) ms_ldeg[i] = 0;
    #define ADD_EDGE(A, B) do { if (me_n < ME_MAX) { mea[me_n] = base + (A); meb[me_n] = base + (B); \
                                me_pat[me_n] = (unsigned char)pat; me_n++; ms_ldeg[A]++; ms_ldeg[B]++; } } while (0)
    // unified fill (rung 5): a spanning tree for connectivity + a per-pattern
    // loop-add-back fraction (MS_LOOPF = the dendricity knob). Even "grid" leaves
    // a fraction of edges out → real dead-ends + Ts, not a perfect degree-4 mesh.
    int sb = 2 + (int)(ms_h01(0, 0, (int)seed) * 2.0f);   // superblock frame period 2..3
    for (int i = 0; i < ln; i++) ms_uf[i] = i;
    if (pat == MP_SUPERBLOCK)                             // the continuous arterial FRAME first
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) {
                if (lg[r][c] < 0) continue;
                if (c + 1 < cols && lg[r][c + 1] >= 0 && (r % sb) == 0) {
                    ADD_EDGE(lg[r][c], lg[r][c + 1]); ms_uf[ms_find(lg[r][c])] = ms_find(lg[r][c + 1]); }
                if (r + 1 < rows && lg[r + 1][c] >= 0 && (c % sb) == 0) {
                    ADD_EDGE(lg[r][c], lg[r + 1][c]); ms_uf[ms_find(lg[r][c])] = ms_find(lg[r + 1][c]); }
            }
    static int ca[2 * MS_LG * MS_LG], cb[2 * MS_LG * MS_LG]; static float cw[2 * MS_LG * MS_LG];
    int nc = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            if (lg[r][c] < 0) continue;
            int skipH = (pat == MP_SUPERBLOCK) && ((r % sb) == 0);   // frame edges already laid
            int skipV = (pat == MP_SUPERBLOCK) && ((c % sb) == 0);
            if (c + 1 < cols && lg[r][c + 1] >= 0 && !skipH) {
                ca[nc] = lg[r][c]; cb[nc] = lg[r][c + 1]; cw[nc] = ms_h01(c, r, (int)seed + 5); nc++; }
            if (r + 1 < rows && lg[r + 1][c] >= 0 && !skipV) {
                ca[nc] = lg[r][c]; cb[nc] = lg[r + 1][c]; cw[nc] = ms_h01(c, r, (int)seed + 555); nc++; }
        }
    for (int i = 1; i < nc; i++)                          // insertion sort by weight (small n)
        for (int j = i; j > 0 && cw[j] < cw[j - 1]; j--) {
            float tw = cw[j]; cw[j] = cw[j - 1]; cw[j - 1] = tw;
            int ta = ca[j]; ca[j] = ca[j - 1]; ca[j - 1] = ta;
            int tb = cb[j]; cb[j] = cb[j - 1]; cb[j - 1] = tb;
        }
    float loopf = MS_LOOPF[pat];
    for (int i = 0; i < nc; i++) {
        int ra = ms_find(ca[i]), rb = ms_find(cb[i]);
        if (ra != rb) { ms_uf[ra] = rb; ADD_EDGE(ca[i], cb[i]); }         // tree edge (connectivity)
        else if (ms_h01(ca[i], cb[i], (int)seed + 999) < loopf            // a loop — but cap most
                 && ms_ldeg[ca[i]] < 3 && ms_ldeg[cb[i]] < 3)             // junctions at 3 arms (T, not X)
            ADD_EDGE(ca[i], cb[i]);
    }
    #undef ADD_EDGE
    // ── stitch (continuity tenet): perimeter locals T onto the bounding arterial ──
    // snap a stub onto the NEAREST arterial vertex; on export that vertex is shared
    // (sndi welds ≤1 m) so the arterial gains a real T-junction. One stub per vertex.
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            if (lg[r][c] < 0) continue;
            int li = base + lg[r][c];
            float px = msx[li], py = msy[li];
            if (ms_dedge(fi, px, py) > MS_INSET + step * 0.75f) continue;   // interior node, skip
            if (ms_h01(c, r, (int)seed + 41) > MS_STITCHF) continue;        // the rest stay dead-ends
            float bestd = (MS_INSET + step * 1.3f); bestd *= bestd;
            int bl = -1, bi = -1;
            for (int l = 0; l < ar_nl; l++)
                for (int i = 0; i < ar_np[l]; i++) {
                    float dx = ar_px[l][i] - px, dy = ar_py[l][i] - py, d = dx * dx + dy * dy;
                    if (d < bestd) { bestd = d; bl = l; bi = i; }
                }
            if (bl < 0 || art_used[bl][bi]) continue;
            if (ms_n >= MS_MAX || me_n >= ME_MAX) continue;
            art_used[bl][bi] = 1;
            int sidx = ms_n;
            msx[ms_n] = ar_px[bl][bi]; msy[ms_n] = ar_py[bl][bi]; ms_n++;
            mea[me_n] = li; meb[me_n] = sidx; me_pat[me_n] = MP_STITCH; me_n++;
        }
}

static void ms_build(void) {
    ms_n = 0; me_n = 0;
    for (int l = 0; l < AR_MAXL; l++)
        for (int i = 0; i < AR_MAXP; i++) art_used[l][i] = 0;
    for (int fi = 0; fi < fc_n; fi++) ms_fill_face(fi);
}

// ── street half-widths + the point→segment distance the query AND the lots share
#define CG_ART_HW   12.0f     // arterial carriageway half-width (m)
#define CG_MINOR_HW  5.0f     // minor-street half-width (m)
typedef struct { int on_road; float dist; int minor; } CityHit;

static float cg_seg_d2(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
    float t = L2 > 1e-6f ? ((px - ax) * dx + (py - ay) * dy) / L2 : 0;
    if (t < 0) t = 0; if (t > 1) t = 1;
    float qx = ax + t * dx, qy = ay + t * dy;
    return (px - qx) * (px - qx) + (py - qy) * (py - qy);
}

// ── Rung 7: BUILDINGS — one terrace box lining each minor-street frontage (a
// Dutch-style block face), set back off the carriageway and kept clear of the
// arterials. Oriented boxes in metres, cached with the city; the caller (sloop)
// maps them to world-px and emits them as solid OB_HOUSE obstacles.
#define CGB_MAX    4000
#define CGB_DEPTH  14.0f      // building depth (m)
#define CGB_SET     3.0f      // setback from the carriageway edge (m)
static float cgb_mx[CGB_MAX], cgb_my[CGB_MAX], cgb_w[CGB_MAX], cgb_h[CGB_MAX], cgb_ang[CGB_MAX];
static int   cgb_n;

static void cg_lots(void) {
    cgb_n = 0;
    float off = CG_MINOR_HW + CGB_SET + CGB_DEPTH * 0.5f;
    float clr2 = (CG_ART_HW + 4.0f) * (CG_ART_HW + 4.0f);
    for (int e = 0; e < me_n && cgb_n < CGB_MAX; e++) {
        float ax = msx[mea[e]], ay = msy[mea[e]], bx = msx[meb[e]], by = msy[meb[e]];
        float dx = bx - ax, dy = by - ay, len = fsqrt(dx * dx + dy * dy);
        if (len < 26.0f) continue;                       // stubs too short to line
        float px = -dy / len, py = dx / len;             // perpendicular to the street
        float mx = (ax + bx) * 0.5f, my = (ay + by) * 0.5f;
        float blen = len * 0.8f; if (blen > 45.0f) blen = 45.0f;   // cap so alleys read between blocks
        float ang = atan2f(dy, dx) * 57.29578f;
        for (int s = -1; s <= 1 && cgb_n < CGB_MAX; s += 2) {      // one terrace each side
            float cx = mx + px * off * s, cy = my + py * off * s;
            if (height_at(cx, cy) < 0.0f) continue;      // water
            float dmin = 1e18f;                          // keep off the arterials (the driven roads)
            for (int l = 0; l < ar_nl; l++)
                for (int i = 0; i + 1 < ar_np[l]; i++) {
                    float d = cg_seg_d2(cx, cy, ar_px[l][i], ar_py[l][i], ar_px[l][i + 1], ar_py[l][i + 1]);
                    if (d < dmin) dmin = d;
                }
            if (dmin < clr2) continue;
            cgb_mx[cgb_n] = cx; cgb_my[cgb_n] = cy;
            cgb_w[cgb_n] = blen; cgb_h[cgb_n] = CGB_DEPTH; cgb_ang[cgb_n] = ang;
            cgb_n++;
        }
    }
}

static void ar_graph_ensure(void) {
    ar_ensure();                                         // arterials current for this seed+knobs
    if (g_valid) return;
    ar_graph();
    ar_faces();
    ms_build();
    g_valid = 1;                                         // set BEFORE cg_lots (it reads the built city)
    cg_lots();
}

// ── the QUERY SEAM (rung 5.5b) — the twin of worldnet's wn_road_at, over the
// generated city. On_road when within a class half-width of any street; the same
// segments ar_draw/ms_draw render (screen == query). Brute-force over the current
// cached city (a city is bounded → thousands of segments).
static CityHit citygen_road_at(float x, float y) {
    ar_graph_ensure();
    float bestA = 1e18f, bestM = 1e18f;
    for (int l = 0; l < ar_nl; l++)                      // arterials (the traced polylines)
        for (int i = 0; i + 1 < ar_np[l]; i++) {
            float d = cg_seg_d2(x, y, ar_px[l][i], ar_py[l][i], ar_px[l][i + 1], ar_py[l][i + 1]);
            if (d < bestA) bestA = d;
        }
    for (int e = 0; e < me_n; e++) {                     // minor streets + stitches
        float d = cg_seg_d2(x, y, msx[mea[e]], msy[mea[e]], msx[meb[e]], msy[meb[e]]);
        if (d < bestM) bestM = d;
    }
    CityHit h;
    float dA = fsqrt(bestA), dM = fsqrt(bestM);
    int on_a = dA <= CG_ART_HW, on_m = dM <= CG_MINOR_HW;
    h.minor = on_m && (!on_a || dM < dA);                // nearest that we're actually ON
    h.on_road = on_a || on_m;
    h.dist = h.minor ? dM : dA;
    return h;
}

// ── pick a city near a world point (the pure twin of citygrow's ar_pick_city —
// no camera) → sets ar_cx/ar_cy/ar_R/ar_D/ar_ccx/ar_ccy; ar_graph_ensure() then
// builds it. The caller (sloop) uses this to choose which city to drop into.
static int citygen_pick_city(float wx, float wy) {
    int hc = wn_ifloor(wx / HUB_CS), hr = wn_ifloor(wy / HUB_CS);
    float bestD = -1;
    for (int a = hc - 2; a <= hc + 2; a++)
        for (int b = hr - 2; b <= hr + 2; b++) {
            float x, y, D; int rk;
            if (!fsettle_hub(a, b, &x, &y, &rk, &D)) continue;
            float dd = (x - wx) * (x - wx) + (y - wy) * (y - wy);
            float sc = D - fsqrt(dd) * 6e-6f;
            if (sc > bestD) { bestD = sc; ar_ccx = a; ar_ccy = b; ar_cx = x; ar_cy = y; ar_D = D; }
        }
    if (bestD < 0) return 0;
    ar_R = 2200.0f + 3200.0f * ar_D;
    ar_valid = 0;
    return 1;
}

// nearest street POINT + tangent to (wx,wy) over the current city — for spawning
// the rig ON a road, facing along it (the twin of wn_nearest_road_point).
static int citygen_nearest_street(float wx, float wy, float *rx, float *ry, float *rang) {
    ar_graph_ensure();
    float best = 1e18f, bx = wx, by = wy, bang = 0;
    #define CG_NEAR(ax, ay, bx2, by2) do {                                       \
        float dx = (bx2) - (ax), dy = (by2) - (ay), L2 = dx * dx + dy * dy;       \
        float t = L2 > 1e-6f ? ((wx - (ax)) * dx + (wy - (ay)) * dy) / L2 : 0;    \
        if (t < 0) t = 0; if (t > 1) t = 1;                                       \
        float qx = (ax) + t * dx, qy = (ay) + t * dy;                             \
        float d = (wx - qx) * (wx - qx) + (wy - qy) * (wy - qy);                  \
        if (d < best) { best = d; bx = qx; by = qy; bang = atan2f(dy, dx); }      \
    } while (0)
    for (int l = 0; l < ar_nl; l++)
        for (int i = 0; i + 1 < ar_np[l]; i++)
            CG_NEAR(ar_px[l][i], ar_py[l][i], ar_px[l][i + 1], ar_py[l][i + 1]);
    for (int e = 0; e < me_n; e++)
        CG_NEAR(msx[mea[e]], msy[mea[e]], msx[meb[e]], msy[meb[e]]);
    #undef CG_NEAR
    if (best > 1e17f) return 0;
    *rx = bx; *ry = by; *rang = bang;
    return 1;
}

#endif // CITYGEN_H
