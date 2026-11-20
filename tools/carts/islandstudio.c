/* de:meta
{
  "slug": "islandstudio",
  "title": "island studio",
  "status": "wip",
  "created": "2026-11-19",
  "kind": ["tech-demo", "generative"],
  "teaches": ["noise-terrain", "pathfinding"],
  "lineage": "A bench for backroads' finite island: worldnet-style layered noise for the shape, plus what an endless world can't do, a whole-map priority-flood drainage pass (Barnes 2014) and stream-power valley carving, so rivers run to the sea and valleys exist for roads to follow.",
  "description": {
    "summary": "Grow a 40 x 40 km island: coast, hills and mountains, water that drains and carves valleys, wind, rain and a rain shadow, cover, settlements, farms, and the roads between them.",
    "detail": "The island is a finite grid: 256 x 256 (156 m cells) while you play with the sliders, 512 or 1024 (39 m) for a final bake. Noise gives the shape: an island mask with a warped coast, layered rolling hills, and ridged mountains that only grow inside a mountain mask (slide it to zero for a low island). Then the whole map drains: a priority flood fills every pit so each cell has a downhill path to the sea, a flow count adds up how much land drains through each cell, and valleys are carved where the flow is large. Draining again gives the rivers and lakes. A prevailing wind brings rain that the mountains wring out, leaving a dry lee. Natural cover follows from height, slope, rain and water (forest, grassland, scrub, moor, rock, marsh, beach), and settlements claim the flattest, lowest, best-watered, most fertile land, spaced apart, with the biggest few becoming towns. Roads are routed across the terrain by A*, most important first, with a discount for reusing an existing road so routes merge into trunks; traffic sets the class (main road, secondary, lane), a ring road follows the coast where the land allows, an expressway joins the two biggest towns when they are far apart, scattered farmsteads add tracks and through-lanes, and forests and moors get dead-end tracks. Rivers are smooth lines and roads cross them on bridges. The panel compares the result to two real islands measured from OpenStreetMap. Design doc: docs/design/island-studio.md.",
    "controls": "Drag the sliders. R or ROLL = new seed. V or VIEW cycles cover / terrain / slope / suitability / flow. Hover the map for height, cover, rain and suitability. Red squares are towns (named), white villages, dots hamlets; hover one for its name. Every island and place gets a name from a Norse, Gaelic or English flavour picked by the seed. The arrow shows the wind. The tree line slider goes from bare hills to wooded mountains. Town size sets how far towns and villages grow their street grids (only the roads re-route). Bridging sets how readily roads bridge open water (lakes, sea inlets): 0 never, 1 crossings up to 2 km. TAB opens a full-screen zoom (drag to pan, wheel to zoom) with roads at their real widths and, close in, every building. G or RES switches the grid between 256 (live), 512 and 1024 (a ~2.5 s bake, much finer roads). X exports the island (roads, cover, heights) to data/islandstudio-<seed>.json (roadview's vector format; measure with tools/road-density.js) plus the packed .rvb that backroads drives."
  }
}
de:meta */
// island studio — a bench for a finite, drivable island (backroads' world).
//
// Pipeline (all on one N×N grid, rebuilt when a slider moves):
//   1. SHAPE    island mask: distance from the centre through a domain warp, so the coast
//               gets bays and headlands. Negative = sea.
//   2. RELIEF   rolling hills (3-octave fbm) + RIDGED mountains masked to one or two regions.
//   3. DRAIN    priority flood (Barnes 2014): grow inward from the sea, lowest cell first. Every
//               land cell is reached from a neighbour that is lower-or-equal → that neighbour is
//               its downstream ("rcv"). Pits get raised to their spill level (lakes).
//   4. FLOW     visit cells in reverse flood order, add each cell's water to its downstream.
//   5. CARVE    valleys: lower each cell by ~sqrt(drained area), blurred so valleys are wider than
//               one cell, then DRAIN + FLOW again for the final rivers.
//   6. CLIMATE  a prevailing wind: air starts wet over the sea and rains out as it climbs, so
//               windward slopes are wet and the lee of the mountains is a rain shadow.
//   7. COVER    natural cover from height, slope, moisture and distance to water: forest, grass,
//               scrub, moor, bare rock, marsh, beach. (Before people: farmland comes later.)
//   8. SETTLE   suitability = flat × low × near fresh water or coast × fertile. The best spots,
//               spaced apart, become settlements; the good land around each sets its size, and
//               the biggest few (also spaced) become towns. Empty and full fall out of the land.
// World: 1 cell = CELL metres; heights in metres.
#include "studio.h"
#include "ui.h"
#include "roadkit.h"
#include "buildingkit.h"  // the shared building catalog (OSM vocabulary): we place, backroads draws   // ramp splines (scurve_spline): the shared interchange grammar from roadlab
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#define MAXN     1024                        // arrays are sized for the finest bake
static int N = 256;                          // grid cells per side: 256 live, 512 / 1024 to bake
#define AREA_K   ((float)N * N / (256.0f * 256.0f))   // cell-count thresholds were tuned at 256: scale them
#define MAP_PX   256                         // the overview is always drawn 256 px across
#define MAPCELL  (WORLD_M / MAP_PX)          // metres per overview pixel
#define WORLD_M  40000.0f
#define CELL     (WORLD_M / N)               // 156.25 m
#define MAP_X    8
#define MAP_Y    22
#define PANEL_X  276

// ── knobs (0..1 sliders) ───────────────────────────────────────────────────────
// slider start values can be overridden at build time for sweeps: DE_DEFINES=K_BRIDGE0=1.0f node tools/play.js islandstudio ...
#ifndef K_BRIDGE0
#define K_BRIDGE0 0.35f
#endif
#ifndef K_TOWN0
#define K_TOWN0 0.25f
#endif
static float k_size  = 0.55f;   // island radius
static float k_coast = 0.50f;   // coast warp (bays, headlands)
static float k_hills = 0.45f;   // rolling-hill height
static float k_mtn   = 0.55f;   // mountain height (0 = a low island)
static float k_mreg  = 0.45f;   // how much of the island the mountains cover
static float k_rug   = 0.50f;   // ridge sharpness / fine detail
static float k_ero   = 0.50f;   // valley carving
static float k_riv   = 0.45f;   // river threshold (lower = more rivers drawn)
static float k_wind  = 0.62f;   // prevailing wind, the direction it blows FROM (0..1 = 0..360°)
static float k_rain  = 0.50f;   // how wet the climate is overall
static float k_vill  = 0.60f;   // settlement density (spacing 4.5 km .. 1.5 km)
static float k_farm  = 0.93f;   // farmstead density (spacing ~1.2 km .. 450 m)
static float k_town  = K_TOWN0;   // how far towns grow: town street radius 300 m .. 1.5 km (villages ~0.38 of it)
static float k_bridge = K_BRIDGE0;  // appetite for bridging open water (lakes, inlets): 0 = never .. 1 = spans up to 2 km
static float k_tree  = 0.45f;   // tree line: 0 = bare Scottish hills (~400 m), 1 = wooded to ~1300 m
static int   seed = 1;
static int   view;
enum { V_COVER, V_TERRAIN, V_SLOPE, V_SUIT, V_FLOW, NVIEW };
static const char *VIEWS[NVIEW] = { "cover", "terrain", "slope", "suitability", "flow" };

// natural cover classes
enum { CV_SEA, CV_LAKE, CV_BEACH, CV_MARSH, CV_GRASS, CV_SCRUB, CV_FOREST, CV_MOOR, CV_ROCK, NCV };
static const char *CVN[NCV] = { "sea", "lake", "beach", "marsh", "grassland", "scrub", "forest", "moor", "bare rock" };

// settlements
enum { ST_HAMLET, ST_VILLAGE, ST_TOWN };
#define MAXSET 400
typedef struct { int x, y, rank; float size; } Settle;
static Settle set[MAXSET];
static float  set_r[MAXSET];   // built-up radius per settlement (m), from the town-size slider: streets, farms keep out, export
static int    nset;

// ── grids ──────────────────────────────────────────────────────────────────────
static float H[MAXN * MAXN];          // height (m), sea < 0
static float F[MAXN * MAXN];          // filled height (lakes raised to spill level)
static float A[MAXN * MAXN];          // drained area (cells)
static float C[MAXN * MAXN];          // carve scratch
static int   rcv[MAXN * MAXN];        // downstream cell (-1 = sea / outlet)
static int   order[MAXN * MAXN], norder;
static unsigned char col[MAXN * MAXN];
static float M[MAXN * MAXN];          // moisture 0..1 (rain the cell gets)
static float DW[MAXN * MAXN];         // distance to fresh water (river / lake), metres
static float DS[MAXN * MAXN];         // distance to the sea, metres
static float SU[MAXN * MAXN];         // settlement suitability 0..1
static unsigned char CV[MAXN * MAXN]; // cover class
static int   dirty = 1;
// zoom view (TAB) state
#define ZW 480
#define ZH 300
static int   zoom_on;
static float zx = 20000, zy = 20000, zm = 6;          // centre (m), metres per pixel
static int   zdirty = 1, zdrag, zmx, zmy;
static void zoom_input(void);

// stats
static float st_land, st_peak, st_flat, st_high, st_river, st_cov[NCV];
static int   st_lakes, st_rank[3], st_coastal;   // st_coastal: % of settlements within 1 km of the sea

static float k_farm_sp(void) { return 1200.0f - k_farm * 750.0f; }
static float nz(float x, float y, float o) { return noise3(x, y, seed * 7.31f + o); }
static float smooth01(float a, float b, float x) {
    float t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t);
}

// ── 1 + 2: shape and relief ────────────────────────────────────────────────────
static float height_at(float x, float y) {          // x, y in metres, 0..WORLD_M
    float kx = x / 1000.0f, ky = y / 1000.0f;       // km
    float wa = 2.0f + k_coast * 9.0f;               // coast warp, km
    float wx = kx + wa * (nz(kx / 14.0f, ky / 14.0f, 1.3f) - 0.5f) * 2
                  + wa * 0.35f * (nz(kx / 4.0f, ky / 4.0f, 2.1f) - 0.5f) * 2;
    float wy = ky + wa * (nz(kx / 14.0f, ky / 14.0f, 5.7f) - 0.5f) * 2
                  + wa * 0.35f * (nz(kx / 4.0f, ky / 4.0f, 8.4f) - 0.5f) * 2;
    float R = 9.0f + k_size * 9.0f;                 // island radius, km
    // the radius itself wanders with direction (noise on the unit circle, so it wraps):
    // lobes and long headlands, not a disc
    float ang = atan2f(wy - 20, wx - 20);
    float ca = cosf(ang), sa = sinf(ang);
    R *= 1.0f + (0.25f + k_coast * 0.55f) * (nz(ca * 1.1f + 3, sa * 1.1f + 3, 41.0f) - 0.5f) * 2
              + 0.20f * (nz(ca * 2.7f + 9, sa * 2.7f + 9, 43.0f) - 0.5f) * 2;
    float d = sqrtf((wx - 20) * (wx - 20) + (wy - 20) * (wy - 20)) / R;
    float e = 1.0f - d;                             // >0 land, <0 sea (before relief)
    float edge = fminf(fminf(wx, wy), fminf(40 - wx, 40 - wy));   // km to the border (warped: no ruler coast)
    e = fminf(e, (edge - 2.5f) / 5.0f);             // always sea at the frame: it's an ISLAND
    float redge = fminf(fminf(kx, ky), fminf(40 - kx, 40 - ky));
    e = fminf(e, (redge - 1.0f) / 2.0f);            // backstop: the warp can't push land off the map
    float inland = smooth01(0.0f, 0.35f, e);        // 0 at the coast → 1 inland

    // rolling hills: 3 octaves, ~8 km features
    float f = 0, amp = 1, fr = 1.0f / 8.0f, tot = 0;
    for (int o = 0; o < 3; o++) { f += amp * nz(kx * fr, ky * fr, 11.0f + o); tot += amp; amp *= 0.5f; fr *= 2.1f; }
    f /= tot;
    float hills = (f - 0.25f) * (60.0f + k_hills * 760.0f);

    // mountains: ridged noise, inside a ~20 km mask that covers k_mreg of the land
    float mm = nz(kx / 16.0f, ky / 16.0f, 21.0f) * 0.8f + clamp(e, 0, 1) * 0.42f;   // interior-biased
    float th = 0.75f - k_mreg * 0.45f;
    float mmask = smooth01(th - 0.05f, th + 0.25f, mm);   // wide: foothills, not walls
    float r = 0; amp = 1; fr = 1.0f / 7.0f; tot = 0;
    float sharp = 1.0f + k_rug * 2.0f;
    for (int o = 0; o < 4; o++) {
        float n = 1.0f - fabsf(2.0f * nz(kx * fr, ky * fr, 31.0f + o) - 1.0f);
        r += amp * powf(n, sharp); tot += amp; amp *= 0.45f + k_rug * 0.15f; fr *= 2.0f;
    }
    r /= tot;
    float mtn = mmask * r * k_mtn * 1600.0f;

    return e * 260.0f + inland * (fmaxf(hills, 0) * 0.85f + hills * 0.15f + mtn);
}

// ── 3 + 4: priority-flood drainage + flow accumulation ─────────────────────────
static int   hp_i[MAXN * MAXN * 2];
static float hp_k[MAXN * MAXN * 2];
static int   hp_n;
static void hp_push(int i, float k) {
    int c = hp_n++;
    while (c > 0) { int p = (c - 1) / 2; if (hp_k[p] <= k) break; hp_i[c] = hp_i[p]; hp_k[c] = hp_k[p]; c = p; }
    hp_i[c] = i; hp_k[c] = k;
}
static int hp_pop(void) {
    int top = hp_i[0]; int li = hp_i[--hp_n]; float lk = hp_k[hp_n]; int c = 0;
    for (;;) {
        int a = 2 * c + 1; if (a >= hp_n) break;
        if (a + 1 < hp_n && hp_k[a + 1] < hp_k[a]) a++;
        if (hp_k[a] >= lk) break;
        hp_i[c] = hp_i[a]; hp_k[c] = hp_k[a]; c = a;
    }
    hp_i[c] = li; hp_k[c] = lk;
    return top;
}
static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

static void drain(void) {
    static unsigned char seen[MAXN * MAXN];
    hp_n = 0; norder = 0;
    for (int i = 0; i < N * N; i++) { seen[i] = 0; rcv[i] = -1; A[i] = 1; }
    // seeds: every sea cell (and the map border) is an outlet
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int i = y * N + x;
        if (H[i] <= 0 || x == 0 || y == 0 || x == N - 1 || y == N - 1) {
            seen[i] = 1; F[i] = H[i]; hp_push(i, F[i]);
        }
    }
    while (hp_n > 0) {
        int c = hp_pop(); order[norder++] = c;
        int cx = c % N, cy = c / N;
        for (int k = 0; k < 8; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            int n = ny * N + nx; if (seen[n]) continue;
            seen[n] = 1;
            F[n] = fmaxf(H[n], F[c] + 0.01f);            // pits fill to the spill level
            rcv[n] = c;
            hp_push(n, F[n]);
        }
    }
    // flow: reverse flood order = upstream before downstream
    for (int j = norder - 1; j >= 0; j--) {
        int c = order[j];
        if (rcv[c] >= 0 && H[c] > 0) A[rcv[c]] += A[c];
    }
}

// ── 5: carve valleys ───────────────────────────────────────────────────────────
static float slope_at(int x, int y) {                   // rise over run (m/m)
    int x0 = x > 0 ? x - 1 : x, x1 = x < N - 1 ? x + 1 : x;
    int y0 = y > 0 ? y - 1 : y, y1 = y < N - 1 ? y + 1 : y;
    float gx = (H[y * N + x1] - H[y * N + x0]) / ((x1 - x0) * CELL);
    float gy = (H[y1 * N + x] - H[y0 * N + x]) / ((y1 - y0) * CELL);
    return sqrtf(gx * gx + gy * gy);
}

static float river_thr(void) { return (40.0f + (1.0f - k_riv) * 900.0f) * AREA_K; }   // cells drained

#define RIVER_CELLS (300.0f * AREA_K)  // drained cells that count as a real river (~7 km²)
#define LAKE_DEPTH 3.0f                 // shallower filled pits are damp ground, not lakes
static int is_lake(int i) { return H[i] > 0 && F[i] - H[i] > LAKE_DEPTH; }
static int is_dry(int i) { return H[i] > 0 && !is_lake(i); }
static int is_fresh(int i) { return H[i] > 0 && (A[i] >= RIVER_CELLS || is_lake(i)); }

// ── 6: climate. March UPWIND from each cell; every metre the air has climbed since it left the
// sea wrings rain out of it. What's left (plus a bonus for climbing right here) is this cell's
// rain. ~9 km of fetch, nearest-cell steps: cheap, and the rain shadow is unmistakable. ─────────
static void climate(void) {
    float a = k_wind * 6.2831853f;
    float ux = cosf(a), uy = sinf(a);                       // unit vector pointing UPWIND
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int i = y * N + x;
        if (H[i] <= 0) { M[i] = 1; continue; }
        float ridge = H[i], prev = H[i], local = 0;
        int kmax = (int)(9375.0f / CELL), kloc = (int)(625.0f / CELL + 0.5f), kmin = (int)(312.0f / CELL + 0.5f);
        for (int k = 1; k <= kmax; k++) {
            int sx = (int)(x + ux * k + 0.5f), sy = (int)(y + uy * k + 0.5f);
            if (sx < 0 || sy < 0 || sx >= N || sy >= N) break;
            float h = fmaxf(H[sy * N + sx], 0);
            if (k <= kloc && prev > h) local += prev - h;            // air climbing toward us right here
            prev = h;
            ridge = fmaxf(ridge, h);                              // highest ground the air crossed
            if (H[sy * N + sx] <= 0 && k > kmin) break;             // reached the sea: air was full
        }
        // rain SHADOW: air that crossed a ridge higher than this cell has rained out and sunk.
        // Windward slopes and summits have nothing higher upwind, so they stay wet.
        float shadow = ridge - H[i];
        float wet = expf(-shadow / (150.0f + k_rain * 450.0f));
        float oro = clamp(local / 120.0f, 0, 1) * 0.35f;         // windward slope: rain falls HERE
        wet += clamp(H[i] / 1500.0f, 0, 0.25f);                  // high ground catches cloud
        M[i] = clamp((0.16f + k_rain * 0.44f) * (wet + oro), 0, 1);
    }
    // blur: the nearest-cell upwind march leaves diagonal streaks; weather is smooth anyway
    static float T[MAXN * MAXN];
    int rb = (int)(312.0f / CELL + 0.5f); if (rb < 1) rb = 1;   // ~±310 m, separable (cheap at 1024)
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
            float sm = 0; int n = 0;
            for (int l = -rb; l <= rb; l++) { int xx = x + l; if (xx < 0 || xx >= N) continue; sm += M[y * N + xx]; n++; }
            T[y * N + x] = sm / n;
        }
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
            float sm = 0; int n = 0;
            for (int k = -rb; k <= rb; k++) { int yy = y + k; if (yy < 0 || yy >= N) continue; sm += T[yy * N + x]; n++; }
            M[y * N + x] = H[y * N + x] > 0 ? sm / n : 1;
        }
    }
}

// two-pass chamfer distance transform (metres) from every cell where src(i) is true
static void dist_from(float *D, int (*src)(int)) {
    const float d1 = CELL, d2 = CELL * 1.41421f;
    for (int i = 0; i < N * N; i++) D[i] = src(i) ? 0 : 1e9f;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        float *d = &D[y * N + x];
        if (x > 0)               *d = fminf(*d, D[y * N + x - 1] + d1);
        if (y > 0)               *d = fminf(*d, D[(y - 1) * N + x] + d1);
        if (x > 0 && y > 0)      *d = fminf(*d, D[(y - 1) * N + x - 1] + d2);
        if (x < N - 1 && y > 0)  *d = fminf(*d, D[(y - 1) * N + x + 1] + d2);
    }
    for (int y = N - 1; y >= 0; y--) for (int x = N - 1; x >= 0; x--) {
        float *d = &D[y * N + x];
        if (x < N - 1)               *d = fminf(*d, D[y * N + x + 1] + d1);
        if (y < N - 1)               *d = fminf(*d, D[(y + 1) * N + x] + d1);
        if (x < N - 1 && y < N - 1)  *d = fminf(*d, D[(y + 1) * N + x + 1] + d2);
        if (x > 0 && y < N - 1)      *d = fminf(*d, D[(y + 1) * N + x - 1] + d2);
    }
}
static int is_sea(int i) { return H[i] <= 0; }
static float DL[MAXN * MAXN];          // water cells: distance to dry land (m); bridges may span 2x this

// ── 7: natural cover ─────────────────────────────────────────────────────────────
static void cover(void) {
    float treeline = 400.0f + k_tree * 900.0f;              // metres, at moderate rain
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int i = y * N + x; float h = H[i], m = M[i], sl = slope_at(x, y);
        int c;
        if (h <= 0)                                          c = CV_SEA;
        else if (is_lake(i))                                 c = CV_LAKE;
        else if (h < 4 && DS[i] < 450)                       c = CV_BEACH;
        else if (sl < 0.03f && h < 25 && m > 0.30f && (DW[i] < 350 || (DS[i] < 600 && A[i] > 20 * AREA_K)))
                                                             c = CV_MARSH;
        else if (sl > 0.45f || (h > 850 && sl > 0.22f))      c = CV_ROCK;
        else if (h > treeline - m * 220 || (h > treeline * 0.55f && m < 0.30f)) c = CV_MOOR;   // above the tree line
        else if (m > 0.48f || (m > 0.32f && sl > 0.07f) || (DW[i] < 300 && m > 0.28f && sl > 0.03f))
                                                             c = CV_FOREST;
        else if (m < 0.30f)                                  c = CV_SCRUB;
        else                                                 c = CV_GRASS;
        CV[i] = (unsigned char)c;
    }
}

// ── 8: settlements ───────────────────────────────────────────────────────────────
static float *su_sort_key;
static int su_cmp(const void *a, const void *b) {
    float ka = su_sort_key[*(const int *)a], kb = su_sort_key[*(const int *)b];
    return ka < kb ? 1 : ka > kb ? -1 : 0;
}
// RESOLUTION-STABLE PLACES: settlements are always chosen on the 256 grid (the one the sliders are
// tuned on); a 512 / 1024 bake first runs a quick 256 pass (~70 ms) for the plan (place, rank, size),
// then snaps each place to the most suitable buildable cell within ~300 m on its own grid. Placing
// per cell at each grid made seed 3 have 87 places at 256 and 95 at 1024, only 42 of them within
// 400 m of each other and different towns: what you previewed wasn't what you exported.
static int river_cell(int i);
static int   plan_mode, nplan;
static float plan_x[MAXSET], plan_y[MAXSET], plan_size[MAXSET]; static int plan_rank[MAXSET], set_plan[MAXSET];
static void place_from_plan(void) {
    static unsigned char taken[MAXN * MAXN];
    for (int i = 0; i < N * N; i++) taken[i] = 0;
    nset = 0;
    int r = (int)(300.0f / CELL + 0.5f);
    for (int p = 0; p < nplan; p++) {
        int cx = (int)(plan_x[p] / CELL), cy = (int)(plan_y[p] / CELL), best = -1; float bs = -1e9f;
        for (int y = cy - r; y <= cy + r; y++) for (int x = cx - r; x <= cx + r; x++) {
            if (x < 0 || y < 0 || x >= N || y >= N) continue;
            int i = y * N + x; if (taken[i]) continue;
            float d = hypotf((float)(x - cx), (float)(y - cy)) / (r + 1);
            float sc = SU[i] > 0 ? SU[i] - 0.25f * d                       // suitable land, near the plan
                     : (H[i] > 0 && !river_cell(i) && !is_lake(i)) ? -1.0f - d : -1e9f;   // else any dry land
            if (sc > bs) { bs = sc; best = i; }
        }
        if (best < 0) continue;
        taken[best] = 1;
        set[nset].x = best % N; set[nset].y = best / N; set[nset].rank = plan_rank[p]; set[nset].size = plan_size[p];
        set_plan[nset] = p; nset++;
    }
}
static void settle(void) {
    static float raw[MAXN * MAXN];
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int i = y * N + x, c = CV[i];
        if (c == CV_SEA || c == CV_LAKE || c == CV_MARSH || c == CV_ROCK || c == CV_BEACH
            || A[i] >= fminf(RIVER_CELLS, river_thr())) { raw[i] = 0; continue; }   // never ON water
        float flat  = 1.0f - clamp(slope_at(x, y) / 0.09f, 0, 1);
        float low   = 1.0f - clamp(H[i] / 650.0f, 0, 1);
        float water = fmaxf(expf(-DW[i] / 1300.0f), 0.55f * expf(-DS[i] / 1000.0f));   // a river or lake beats the sea
        float fert  = clamp((M[i] - 0.15f) / 0.45f, 0, 1) * (c == CV_MOOR ? 0.3f : 1.0f);
        raw[i] = flat * low * (0.25f + 0.75f * water) * (0.35f + 0.65f * fert);
        // where a river meets the sea: the classic harbour town spot
        if (A[i] >= RIVER_CELLS * 0.5f && DS[i] < 700) raw[i] *= 1.25f;
    }
    // SU = good land in the neighbourhood (~1 km box), not just this cell
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        float s = 0; int n = 0; int r1 = (int)(470.0f / CELL + 0.5f);
        for (int k = -r1; k <= r1; k++) for (int l = -r1; l <= r1; l++) {
            int xx = x + l, yy = y + k; if (xx < 0 || yy < 0 || xx >= N || yy >= N) continue;
            s += raw[yy * N + xx]; n++;
        }
        SU[y * N + x] = raw[y * N + x] > 0 ? clamp(0.5f * raw[y * N + x] + 0.5f * s / n, 0, 1) : 0;
    }
    float mx = 1e-6f; for (int i = 0; i < N * N; i++) mx = fmaxf(mx, SU[i]);
    for (int i = 0; i < N * N; i++) SU[i] /= mx;            // relative: the island's best spot = 1
    if (plan_mode) { place_from_plan(); return; }
    // greedy placement: best cells first, each keeps a spacing
    static int cand[MAXN * MAXN]; int nc = 0;
    for (int i = 0; i < N * N; i++) if (SU[i] > 0.08f) cand[nc++] = i;
    su_sort_key = SU; qsort(cand, nc, sizeof(int), su_cmp);
    float sp = (4.5f - k_vill * 3.0f) * 1000.0f / CELL;      // spacing in cells
    nset = 0;
    for (int j = 0; j < nc && nset < MAXSET; j++) {
        int i = cand[j], x = i % N, y = i / N, ok = 1;
        for (int k = 0; k < nset && ok; k++) {
            float dx = set[k].x - x, dy = set[k].y - y;
            if (dx * dx + dy * dy < sp * sp) ok = 0;
        }
        if (!ok) continue;
        float size = 0; int R = (int)(2500.0f / CELL + 0.5f); // good land within 2.5 km
        for (int k = -R; k <= R; k++) for (int l = -R; l <= R; l++) {
            int xx = x + l, yy = y + k; if (xx < 0 || yy < 0 || xx >= N || yy >= N || k * k + l * l > R * R) continue;
            size += raw[yy * N + xx];
        }
        set[nset].x = x; set[nset].y = y; set[nset].size = size; set[nset].rank = ST_HAMLET; nset++;
    }
    // rank: the biggest few (spaced 10 km apart) are towns, the upper half villages
    static int ix[MAXSET]; for (int k = 0; k < nset; k++) ix[k] = k;
    for (int a = 0; a < nset; a++) for (int b = a + 1; b < nset; b++)
        if (set[ix[b]].size > set[ix[a]].size) { int t = ix[a]; ix[a] = ix[b]; ix[b] = t; }
    int ntown = 0, maxtown = 1 + (int)(st_land / 300.0f);    // ~1 town per 300 km²
    float tsp = 10000.0f / CELL;
    for (int a = 0; a < nset && ntown < maxtown; a++) {
        Settle *s = &set[ix[a]]; int ok = 1;
        for (int b = 0; b < a; b++) {
            Settle *o = &set[ix[b]]; if (o->rank != ST_TOWN) continue;
            float dx = o->x - s->x, dy = o->y - s->y; if (dx * dx + dy * dy < tsp * tsp) ok = 0;
        }
        if (ok) { s->rank = ST_TOWN; ntown++; }
    }
    for (int a = 0; a < nset * 45 / 100; a++) if (set[ix[a]].rank == ST_HAMLET) set[ix[a]].rank = ST_VILLAGE;
}

// ── 9: roads ───────────────────────────────────────────────────────────────────
// WHERE roads go is routed on the coarse grid (A* over a terrain cost). Order matters: the
// important connections are routed first, and every later route gets a discount for running on
// an existing road, so routes MERGE into shared trunks instead of running side by side. Each
// route adds its traffic (gravity: size × size / distance) to the cells it uses, and traffic
// sets the class. WHAT roads are (smooth lines in metres) comes from walking the road graph.
enum { RC_NONE, RC_TRACK, RC_LANE, RC_SEC, RC_MAIN, RC_EXPRESS, NRC };
static const char *RCN[NRC] = { "", "track", "lane", "secondary", "main road", "expressway" };
static const float RHW[NRC] = { 0, 2.0f, 2.8f, 3.8f, 5.0f, 10.0f };       // half-width, metres
static const int   RCOL[NRC] = { 0, CLR_LIGHT_PEACH, CLR_WHITE, CLR_YELLOW, CLR_ORANGE, CLR_RED };
static const int OPP[8] = { 1, 0, 3, 2, 7, 6, 5, 4 };                    // opposite of DX/DY dir k
static unsigned char ER[MAXN * MAXN][8];   // road class on edge (cell, dir k); stored both ways
static float TR[MAXN * MAXN];              // traffic through a cell
static unsigned char RING[MAXN * MAXN];    // on the ring road
static int   roads_dirty = 1;
static float st_roads_ms;          // how long the last routing took
enum { RM_LINK, RM_RING, RM_EXPRESS, RM_TRACK };

static unsigned char JN[MAXN * MAXN];   // road edges meeting at a cell (>= 3 = a junction)
static int river_cell(int i) { return H[i] > 0 && A[i] >= RIVER_CELLS * 0.5f; }

static int no_water;                              // set while re-routing a route whose bridge was too long
static float max_span(void) { return 150.0f + k_bridge * 1850.0f; }   // longest bridge the slider allows (m)
static float step_cost(int a, int b, int k, int mode) {
    float len = k < 4 ? CELL : CELL * 1.41421f;
    // OPEN WATER (sea inlets, lakes): bridged only as far as the bridging slider allows. A water cell
    // more than half the longest allowed span from land is out of reach; each metre over water costs
    // like many metres of road, so a bridge only wins against a long way round. Farm tracks never bridge.
    if (!is_dry(b)) {
        if (mode == RM_TRACK || mode == RM_RING || no_water || k_bridge < 0.02f) return -1;   // only links + the expressway bridge
        float maxspan = max_span();
        if (DL[b] > maxspan * 0.5f) return -1;
        // dearer the further from shore: a bridge CROSSES water at its narrows; without this, full
        // appetite made 3 km "bridges" running along a channel
        return len * (1.2f + 30.0f * (1.0f - k_bridge) * (1.0f - k_bridge)) * (1.0f + DL[b] / 400.0f);
    }
    float g = fabsf(H[b] - H[a]) / len;                        // grade ALONG the step: contours are cheap
    float m = mode == RM_EXPRESS
        ? 1 + (g / 0.035f) * (g / 0.035f) * 5 + (g > 0.07f ? (g - 0.07f) * 400 : 0)
        : 1 + (g / 0.07f) * (g / 0.07f) * 2.5f + (g > 0.14f ? (g - 0.14f) * 150 : 0);
    int c = CV[b];
    m *= c == CV_MARSH ? 3.0f : c == CV_ROCK ? 2.5f : c == CV_BEACH ? 2.0f
       : c == CV_FOREST ? 1.2f : c == CV_MOOR ? 1.15f : 1.0f;
    // old roads wander: they followed field edges and whoever owned what. A SMOOTH ±20% cost field
    // (~300 m features) bends the straight lines across flat land; expressways ignore it.
    // (It was random per cell: gentle at 156 m cells, but at 39 m it made main roads zigzag between
    //  cheap and dear cells: 62-76 sharp kinks per 100 km vs the Isle of Man's 7-8.)
    if (mode != RM_EXPRESS) {
        float wx = (b % N + 0.5f) * CELL, wy = (b / N + 0.5f) * CELL;
        m *= 0.8f + 0.4f * clamp((nz(wx / 300.0f, wy / 300.0f, 91.0f) - 0.5f) * 2.2f + 0.5f, 0, 1);
    }
    if (mode == RM_RING) m *= DS[b] < 160 ? 1.8f : DS[b] < 2200 ? 0.8f : 1.35f;   // hug the coast, off the beach
    float extra = 0;
    int bx = b % N, by = b / N, ax = a % N, ay = a / N;
    int cross = river_cell(b) && !river_cell(a);
    if (k >= 4 && !river_cell(a) && !river_cell(b)                // diagonal step slipping between two river cells
        && river_cell(ay * N + bx) && river_cell(by * N + ax)) cross = 1;
    if (cross) {                                               // a bridge: cost grows with the river
        float span = clamp(sqrtf(fmaxf(A[b], A[ay * N + bx]) / AREA_K) / 40.0f, 0.4f, 3.0f);
        extra = (mode == RM_EXPRESS ? 2.0f : 5.0f) * CELL * span * (1.35f - k_bridge);   // x1 at the default appetite
    }
    if (river_cell(a) && river_cell(b)) m *= 6;                // don't drive along the river bed
    if (mode != RM_EXPRESS) {
        if (ER[a][k] == RC_EXPRESS) m *= 1.0f;           // access-controlled: cross it, never ride it
        else if (ER[a][k]) m *= ER[a][k] >= RC_MAIN ? 0.55f : ER[a][k] == RC_TRACK ? 0.8f : 0.65f;   // follow a road
        else if (TR[b] > 0) m *= 0.85f;                        // join one
        if (JN[b] >= 3) m *= 1.0f;                           // (a junction discount made routes detour: off)
        // JOIN IT OR KEEP AWAY: a cell within ~60 m of a road but not on it costs extra, so a route
        // merges onto the road or keeps a real distance. At 39 m cells, running alongside made a
        // second road 39 m away that jogged into the first (teeth, hooks: 56 kinks/100 km on main
        // roads vs the Isle of Man's 7). Below 60 m/cell (256 grid) this is off: calibration holds.
        if (!TR[b] && !JN[b]) {
            int rr = (int)(60.0f / CELL);
            if (rr >= 1) {
                int bx2 = b % N, by2 = b / N, near = 0;
                for (int yy = by2 - rr; yy <= by2 + rr && !near; yy++) for (int xx = bx2 - rr; xx <= bx2 + rr; xx++) {
                    if (xx < 0 || yy < 0 || xx >= N || yy >= N) continue;
                    int c2 = yy * N + xx; if (TR[c2] > 0 || JN[c2]) { near = 1; break; }
                }
                if (near) m *= 1.6f;
            }
        }
        // (calibrated 2026-11-19: 0.3/0.4 collapsed every route onto a trunk, so roads merged into
        //  T junctions instead of crossing: 1 % crossroads and 50 % dead ends vs the Isle of Man's 5 % / 35 %)
    }
    return len * m + extra;
}

static float ag[MAXN * MAXN];
static int   af[MAXN * MAXN], astamp[MAXN * MAXN], aclosed[MAXN * MAXN], agen;
static int   pbuf[MAXN * MAXN];
// A* from s to t; writes the cell path into out. Returns its cost, or -1 (no path / over budget).
static float astar(int s, int t, int mode, int *out, int *nout, float budget) {
    agen++; hp_n = 0;
    int tx = t % N, ty = t / N;
    float hw = mode == RM_TRACK ? 1.0f : 0.8f;                 // heuristic weight (slightly greedy = fast)
    ag[s] = 0; astamp[s] = agen; af[s] = -1;
    hp_push(s, 0);
    while (hp_n > 0) {
        int c = hp_pop();
        if (aclosed[c] == agen) continue;
        aclosed[c] = agen;
        if (c == t) break;
        if (ag[c] > budget) return -1;
        int cx = c % N, cy = c / N;
        for (int k = 0; k < 8; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            int n = ny * N + nx;
            if (aclosed[n] == agen) continue;
            float sc = step_cost(c, n, k, mode); if (sc < 0) continue;
            float ng = ag[c] + sc;
            if (astamp[n] != agen || ng < ag[n]) {
                astamp[n] = agen; ag[n] = ng; af[n] = c;
                float dx = nx - tx, dy = ny - ty;
                if (hp_n < N * N * 2 - 1) hp_push(n, ng + hw * CELL * sqrtf(dx * dx + dy * dy));
            }
        }
    }
    if (aclosed[t] != agen) return -1;
    int n = 0; for (int c = t; c != -1; c = af[c]) out[n++] = c;
    for (int i = 0; i < n / 2; i++) { int tmp = out[i]; out[i] = out[n - 1 - i]; out[n - 1 - i] = tmp; }
    *nout = n; return ag[t];
}

static int dir_to(int a, int b) {
    int dx = b % N - a % N, dy = b / N - a / N;
    for (int k = 0; k < 8; k++) if (DX[k] == dx && DY[k] == dy) return k;
    return -1;
}
static void mark(const int *p, int n, int cls, float traffic, int ring, int only_new) {
    for (int i = 0; i + 1 < n; i++) {
        int a = p[i], b = p[i + 1], k = dir_to(a, b); if (k < 0) continue;
        if (only_new && ER[a][k]) continue;
        if (!ER[a][k]) { if (JN[a] < 255) JN[a]++; if (JN[b] < 255) JN[b]++; }
        if (cls > ER[a][k]) { ER[a][k] = (unsigned char)cls; ER[b][OPP[k]] = (unsigned char)cls; }
    }
    for (int i = 0; i < n; i++) { TR[p[i]] += traffic; if (ring) RING[p[i]] = 1; }
}
static float set_dist(int a, int b) {
    float dx = set[a].x - set[b].x, dy = set[a].y - set[b].y; return sqrtf(dx * dx + dy * dy) * CELL;
}
static float gravity(int a, int b) {
    float d = fmaxf(set_dist(a, b) / 1000.0f, 1.5f);
    return set[a].size * set[b].size / powf(d, 1.5f);
}

// road polylines (smoothed, metres)
#define MAXPL 16000
#define MAXPT 600000
static int st_overflow;               // road lines dropped for lack of room: must stay 0 (it silently ate main roads once)
typedef struct { int start, n, cls; } Poly;
static Poly  pl[MAXPL]; static int npl;
static unsigned char pl_ramp[MAXPL];   // an interchange slip road (unnumbered)
static float plx[MAXPT], ply[MAXPT]; static int npt;
static float st_road[NRC];           // km of road per class
static int   st_express, st_ringgaps, st_link_try[3], st_link_ok[3];   // links tried/routed by floor (lane/sec/main)

static int link_a[4096], link_b[4096]; static float link_w[4096];
static int link_cmp_idx[4096];
static int link_cmp(const void *x, const void *y) {
    float a = link_w[*(const int *)x], b = link_w[*(const int *)y];
    return a < b ? 1 : a > b ? -1 : 0;
}
static int fcmp_up(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }
static unsigned char IS_SET[MAXN * MAXN];   // a settlement sits on this cell (is_node ran nset per cell: slow at 1024)
// GRADE SEPARATION: the expressway is its own LAYER. Where it crosses another road the two run
// through each other (no junction): node-ness is counted per layer, and the expressway gets a bridge
// span over the crossing (build_flyovers). It still meets the network at grade where it ENDS.
static int layer_of(int cls) { return cls == RC_EXPRESS; }
static int is_node_l(int c, int L) {
    int deg = 0, cl[2] = { 0, 0 };
    for (int k = 0; k < 8; k++) if (ER[c][k] && layer_of(ER[c][k]) == L) { if (deg < 2) cl[deg] = ER[c][k]; deg++; }
    if (deg == 0) return 0;
    if (deg != 2 || cl[0] != cl[1]) return 1;
    return IS_SET[c];
}
static int is_node(int c) { return is_node_l(c, 0) || is_node_l(c, 1); }
#define SM_MAX 24000
static float sm_x[SM_MAX], sm_y[SM_MAX];
static float sm_tol_min;   // simplification tolerance floor in metres (emit_poly sets it per road class; 0 = rivers)
// cell path → smooth curve in metres (into sm_x/sm_y); returns the point count
static int smooth_path(const int *cells, int n) {
    // 1. Douglas-Peucker: keep only the cell-path points that matter (tolerance ~0.6 cell),
    //    which throws away the 8-direction staircase; 2. Catmull-Rom through what's left,
    //    one point every ~20 m: a road that curves instead of kinking.
    float *ax = sm_x, *ay = sm_y; static float kx[MAXN * 4], ky[MAXN * 4];
    static unsigned char keep[MAXN * 4];
    int m0 = n < MAXN * 4 ? n : MAXN * 4;
    for (int i = 0; i < m0; i++) { kx[i] = (cells[i] % N + 0.5f) * CELL; ky[i] = (cells[i] / N + 0.5f) * CELL; keep[i] = 0; }
    keep[0] = keep[m0 - 1] = 1;
    { static int st[MAXN * 16]; int sp = 0; st[sp++] = 0; st[sp++] = m0 - 1;
      float tol = fmaxf(CELL * 0.6f, sm_tol_min);
      while (sp > 0) {
          int b = st[--sp], a = st[--sp];
          float dx = kx[b] - kx[a], dy = ky[b] - ky[a], L = sqrtf(dx * dx + dy * dy) + 1e-6f;
          int worst = -1; float wd = tol;
          for (int i = a + 1; i < b; i++) {
              float d = fabsf((kx[i] - kx[a]) * dy - (ky[i] - ky[a]) * dx) / L;
              if (d > wd) { wd = d; worst = i; }
          }
          if (worst >= 0 && sp + 4 < MAXN * 16) { keep[worst] = 1; st[sp++] = a; st[sp++] = worst; st[sp++] = worst; st[sp++] = b; }
      } }
    static float qx[MAXN * 4], qy[MAXN * 4]; int q = 0;
    for (int i = 0; i < m0; i++) if (keep[i]) { qx[q] = kx[i]; qy[q] = ky[i]; q++; }
    int m = 0;
    for (int i = 0; i + 1 < q; i++) {
        float p0x = qx[i > 0 ? i - 1 : i], p0y = qy[i > 0 ? i - 1 : i];
        float p1x = qx[i], p1y = qy[i], p2x = qx[i + 1], p2y = qy[i + 1];
        float p3x = qx[i + 2 < q ? i + 2 : i + 1], p3y = qy[i + 2 < q ? i + 2 : i + 1];
        float seg = sqrtf((p2x - p1x) * (p2x - p1x) + (p2y - p1y) * (p2y - p1y));
        int steps = (int)(seg / 20.0f) + 1;
        for (int k = 0; k < steps && m < SM_MAX - 2; k++) {
            float t = k / (float)steps, t2 = t * t, t3 = t2 * t;
            ax[m] = 0.5f * (2 * p1x + (-p0x + p2x) * t + (2 * p0x - 5 * p1x + 4 * p2x - p3x) * t2 + (-p0x + 3 * p1x - 3 * p2x + p3x) * t3);
            ay[m] = 0.5f * (2 * p1y + (-p0y + p2y) * t + (2 * p0y - 5 * p1y + 4 * p2y - p3y) * t2 + (-p0y + 3 * p1y - 3 * p2y + p3y) * t3);
            m++;
        }
    }
    ax[m] = qx[q - 1]; ay[m] = qy[q - 1]; m++;
    return m;
}
static void emit_poly(const int *cells, int n, int cls) {
    if (n < 2) return;
    if (npl >= MAXPL) { st_overflow++; return; }
    // MAIN ROADS SIMPLIFY LOOSER: at 39 m cells a one-cell sideways jog (39 m) beat the 0.6-cell
    // tolerance (23 m), so it stayed and the spline drew it as a hook (26-30 sharp kinks/100 km on
    // main roads vs the Isle of Man's 7). Big roads are laid out in long lines; a 45 m tolerance lets
    // them. Below ~75 m cells (the 256 grid) 0.6 cell is already larger: unchanged there.
    sm_tol_min = cls >= RC_MAIN ? 45.0f : cls == RC_SEC ? 35.0f : 0.0f;
    int m = smooth_path(cells, n);
    sm_tol_min = 0;
    float *ax = sm_x, *ay = sm_y;
    if (npt + m >= MAXPT) { st_overflow++; return; }
    pl[npl].start = npt; pl[npl].n = m; pl[npl].cls = cls; pl_ramp[npl] = 0; npl++;
    float L = 0;
    for (int i = 0; i < m; i++) {
        plx[npt + i] = ax[i]; ply[npt + i] = ay[i];
        if (i) L += sqrtf((ax[i] - ax[i - 1]) * (ax[i] - ax[i - 1]) + (ay[i] - ay[i - 1]) * (ay[i] - ay[i - 1]));
    }
    npt += m; st_road[cls] += L / 1000.0f;
}
// Two roads crossing DIAGONALLY between cells (one NW-SE, one NE-SW through the same 2x2 block)
// share no cell, so the graph had no junction: they passed through each other like a flyover with
// no bridge. Reroute one around the corner so they meet in a real crossroads.
static int st_diagx;
static void fix_diagonal_crossings(void) {
    st_diagx = 0;
    for (int y = 0; y + 1 < N; y++) for (int x = 0; x + 1 < N; x++) {
        int c = y * N + x, b = y * N + x + 1, d = (y + 1) * N + x + 1;
        int cls = ER[c][4];                              // c -> (x+1, y+1)
        if (!cls || !ER[b][6]) continue;                 // b -> (x, y+1): the other diagonal
        ER[c][4] = 0; ER[d][7] = 0;                      // c-d now goes c -> b -> d
        if (cls > ER[c][0]) { ER[c][0] = (unsigned char)cls; ER[b][1] = (unsigned char)cls; }
        if (cls > ER[b][2]) { ER[b][2] = (unsigned char)cls; ER[d][3] = (unsigned char)cls; }
        st_diagx++;
    }
}
// CORNER CUT: a road that goes a -> b -> c, where b is a plain bend (exactly two road edges, same
// class, no settlement) and a, c are neighbours, goes a -> c instead. Irons out right-angle steps
// (incl. the diagonal-crossing reroute) that the smoother keeps as kinks. Repeated until stable.
static int st_cuts;
static void cut_corners(void) {
    st_cuts = 0;
    for (int pass = 0; pass < 3; pass++) {
        int changed = 0;
        for (int b = 0; b < N * N; b++) {
            if (IS_SET[b]) continue;
            int k1 = -1, k2 = -1, deg = 0;
            for (int k = 0; k < 8; k++) if (ER[b][k]) { deg++; if (k1 < 0) k1 = k; else k2 = k; }
            if (deg != 2 || ER[b][k1] != ER[b][k2]) continue;
            int bx = b % N, by = b / N;
            int ax = bx + DX[k1], ay = by + DY[k1], cx2 = bx + DX[k2], cy2 = by + DY[k2];
            int ddx = cx2 - ax, ddy = cy2 - ay;
            if (abs(ddx) > 1 || abs(ddy) > 1 || (ddx == 0 && ddy == 0)) continue;   // a, c not neighbours
            int a = ay * N + ax, kc = -1;
            for (int k = 0; k < 8; k++) if (DX[k] == ddx && DY[k] == ddy) kc = k;
            if (kc < 0) continue;
            int cls = ER[b][k1];
            ER[b][k1] = 0; ER[a][OPP[k1]] = 0; ER[b][k2] = 0; ER[cy2 * N + cx2][OPP[k2]] = 0;
            if (cls > ER[a][kc]) { ER[a][kc] = (unsigned char)cls; ER[cy2 * N + cx2][OPP[kc]] = (unsigned char)cls; }
            changed++; st_cuts++;
        }
        if (!changed) break;
    }
}
// JUNCTION CONSOLIDATION (fine grids): two junctions a few metres apart on the same road (side roads
// that met "almost opposite", or arrived a cell apart) become ONE: the side roads of junction b move
// onto the nearby junction a, so staggered T pairs become crossroads, the sliver of field between two
// near junctions disappears, and b becomes a plain bend. Off on coarse grids (> 100 m cells), where a
// cell apart is a real staggered junction and calibration was done.
static int st_consol;
static int edeg(int c) { int d = 0; for (int k = 0; k < 8; k++) if (ER[c][k]) d++; return d; }
static void line_cells(int a, int c, int cls) {            // 8-connected straight run of road edges a -> c
    int x = a % N, y = a / N, tx = c % N, ty = c / N;
    while (x != tx || y != ty) {
        int sx = (tx > x) - (tx < x), sy = (ty > y) - (ty < y), k = -1;
        for (int q = 0; q < 8; q++) if (DX[q] == sx && DY[q] == sy) k = q;
        int from = y * N + x; x += sx; y += sy; int to = y * N + x;
        if (cls > ER[from][k]) { ER[from][k] = (unsigned char)cls; ER[to][OPP[k]] = (unsigned char)cls; }
    }
}
static void consolidate_junctions(void) {
    st_consol = 0;
    if (CELL > 100.0f) return;
    for (int b = 0; b < N * N; b++) {
        if (IS_SET[b] || edeg(b) < 3) continue;
        int bx = b % N, by = b / N;
        // a junction within three road steps of b (and ~90 m)
        int a = -1, via = -1, k1a = -1;
        for (int k1 = 0; k1 < 8 && a < 0; k1++) {
            if (!ER[b][k1] || ER[b][k1] == RC_EXPRESS) continue;
            int c1 = (by + DY[k1]) * N + bx + DX[k1];
            if (edeg(c1) >= 3 && c1 != b) { a = c1; via = c1; k1a = k1; break; }
            for (int k2 = 0; k2 < 8 && a < 0; k2++) {
                if (!ER[c1][k2] || OPP[k2] == k1) continue;
                int c2 = (c1 / N + DY[k2]) * N + c1 % N + DX[k2];
                if (c2 == b) continue;
                float dx = (c2 % N - bx) * CELL, dy = (c2 / N - by) * CELL;
                if (edeg(c2) >= 3 && dx * dx + dy * dy <= 90.0f * 90.0f) { a = c2; via = c1; k1a = k1; break; }
                if (edeg(c2) != 2) continue;
                for (int k3 = 0; k3 < 8; k3++) {              // a third step (tiny triangles: corners ~3 cells apart)
                    if (!ER[c2][k3] || OPP[k3] == k2) continue;
                    int c3 = (c2 / N + DY[k3]) * N + c2 % N + DX[k3];
                    if (c3 == b || c3 == c1 || edeg(c3) < 3) continue;
                    float ex = (c3 % N - bx) * CELL, ey = (c3 / N - by) * CELL;
                    if (ex * ex + ey * ey > 90.0f * 90.0f) continue;
                    a = c3; via = c1; k1a = k1; break;
                }
            }
        }
        if (a < 0) continue;
        int ax = a % N, ay = a / N, moved = 0;
        for (int k = 0; k < 8; k++) {
            if (!ER[b][k] || k == k1a) continue;              // keep the road toward a
            int cls = ER[b][k];
            if (cls == RC_EXPRESS) continue;
            int c = (by + DY[k]) * N + bx + DX[k];
            if (c == via || c == a) continue;
            // don't create a narrow fork at a: the moved arm must leave >= 35° from a's existing arms
            float ux = (float)(c % N - ax), uy = (float)(c / N - ay), ul = sqrtf(ux * ux + uy * uy);
            if (ul < 0.5f) continue;
            int ok = 1;
            for (int q = 0; q < 8 && ok; q++) if (ER[a][q]) {
                float c35 = (ux * DX[q] + uy * DY[q]) / (ul * sqrtf((float)(DX[q] * DX[q] + DY[q] * DY[q])));
                if (c35 > 0.819f) ok = 0;
            }
            if (!ok) continue;
            ER[b][k] = 0; ER[c][OPP[k]] = 0;                  // detach the side road from b ...
            line_cells(a, c, cls);                            // ... and attach it to a
            moved++;
        }
        if (moved) st_consol++;
    }
}
static void build_polys(void) {
    static unsigned char vis[MAXN * MAXN];
    static int cells[MAXN * 4];
    npl = 0; npt = 0;
    for (int k = 0; k < NRC; k++) st_road[k] = 0;
    for (int i = 0; i < N * N; i++) vis[i] = 0;
    for (int pass = 0; pass < 2; pass++)                       // pass 0 from nodes, pass 1 leftover loops
        for (int c0 = 0; c0 < N * N; c0++) {
            for (int k0 = 0; k0 < 8; k0++) {
                if (!ER[c0][k0] || (vis[c0] >> k0 & 1)) continue;
                int L = layer_of(ER[c0][k0]);
                if (pass == 0 && !is_node_l(c0, L)) continue;
                int cls = ER[c0][k0], n = 0, cur = c0, k = k0;
                cells[n++] = cur;
                for (;;) {
                    int nx = cur % N + DX[k], ny = cur / N + DY[k], nxt = ny * N + nx;
                    vis[cur] |= 1 << k; vis[nxt] |= 1 << OPP[k];
                    cur = nxt; if (n < MAXN * 4) cells[n++] = cur;
                    if (cur == c0 || is_node_l(cur, L)) break;
                    int nk = -1;
                    for (int j = 0; j < 8; j++) if (ER[cur][j] && layer_of(ER[cur][j]) == L && !(vis[cur] >> j & 1)) { nk = j; break; }
                    if (nk < 0) break;
                    k = nk;
                }
                emit_poly(cells, n, cls);
            }
        }
}

// ── FARMSTEADS: most rural road exists to reach a farm, not a village. Farms scatter over
// fertile land on a jittered grid (~700 m), kept with probability = suitability, so the plains
// fill up and the hills stay empty. Each one is joined to the NEAREST road by a short
// least-cost search (Dijkstra until it touches the network): short = a farm track, long = a lane.
#define MAXFARM 6000
static int farm[MAXFARM], nfarm;
static unsigned hsh(unsigned a) { a ^= a >> 16; a *= 0x7feb352dU; a ^= a >> 15; a *= 0x846ca68bU; a ^= a >> 16; return a; }
// ── PLACE NAMES: every settlement gets a name, and the island one too. Each island picks a naming
// flavour from its seed (Norse-ish, Gaelic-ish or English-ish; a quarter of places borrow the next
// one, as real islands layer languages), and the ending follows the place: a harbour on the coast,
// a ford on a river, a dale in the hills. Keyed on seed + location (a 2 km grid), so a place keeps its name across grid sizes where the place itself does; unique on the island, never the island's own name. The names
// travel in the export as the residential area's sub ("place=village;name=Holmby") + JSON "name".
static char set_name[MAXSET][24], set_sub[MAXSET][48], island_name[24];
static const char *NM_FIRST[3][16] = {
    { "Kirk", "Thor", "Grim", "Ulf", "Ask", "Hol", "Sel", "Brek", "Rams", "Gun", "Lax", "Skel", "Hav", "Orm", "Swin", "Ey" },
    { "Bally", "Kil", "Lis", "Tulla", "Drum", "Clon", "Rath", "Carn", "Bal", "Kin", "Ard", "Dun", "Glen", "Ros", "Aber", "Inver" },
    { "Ash", "Brad", "Stan", "Wil", "Ald", "Hather", "Mel", "Thorn", "Cal", "Wood", "Brom", "Chal", "Pen", "Elm", "Hart", "Wick" } };
// endings: [style][where: 0 plain, 1 coast, 2 river, 3 hill][6]
static const char *NM_END[3][4][6] = {
    { { "by", "thorpe", "stead", "toft", "garth", "by" }, { "wick", "ness", "vik", "sey", "haven", "wick" },
      { "beck", "ford", "os", "beck", "ford", "by" }, { "dale", "fell", "haugh", "thwaite", "dale", "scar" } },
    { { "more", "beg", "lagh", "dara", "kee", "fin" }, { "ross", "port", "carrig", "mara", "inish", "ross" },
      { "ath", "aun", "glas", "ford", "linn", "ath" }, { "dow", "knock", "ard", "slieve", "dun", "beg" } },
    { { "ton", "ham", "bury", "ley", "worth", "field" }, { "mouth", "haven", "port", "sea", "cliff", "mouth" },
      { "ford", "bridge", "bourne", "brook", "ford", "wade" }, { "down", "hill", "combe", "ridge", "edge", "low" } } };
static void make_name(char *out, int n, unsigned h, int style, int where) {
    const char *a = NM_FIRST[style][h & 15], *b = NM_END[style][where][(h >> 4) % 6];
    // no doubled joins like "Hol"+"low" -> keep, but "Kirk"+"kee" style letter clashes read badly: skip a repeat
    int la = (int)strlen(a);
    if (la && a[la - 1] == b[0]) snprintf(out, n, "%s%s", a, b + 1); else snprintf(out, n, "%s%s", a, b);
}
static void name_places(void) {
    int style = (int)(hsh((unsigned)seed * 7919u + 3u) % 3u);
    // the island's name first, from the seed alone (so it's the same at every grid size); it ends like a coast
    make_name(island_name, sizeof island_name, hsh((unsigned)seed * 31337u + 11u), style, 1);
    for (int s = 0; s < nset; s++) {
        int i = set[s].y * N + set[s].x;
        int where = DS[i] < 900 ? 1 : DW[i] < 350 ? 2 : H[i] > 140 ? 3 : 0;
        for (int tries = 0; tries < 40; tries++) {
            // keyed on WHERE the place is (a 2 km grid), not its index: 1024 places a few more
            // settlements than 256, so index-keyed names changed between preview and export
            int qx = (int)((set[s].x + 0.5f) * CELL / 2000.0f), qy = (int)((set[s].y + 0.5f) * CELL / 2000.0f);
            unsigned h = hsh((unsigned)qx * 73856093u ^ (unsigned)qy * 19349663u ^ (unsigned)seed * 40503u ^ (unsigned)tries * 2654435761u);
            int st = ((h >> 12) & 3) == 0 ? (style + 1) % 3 : style;      // a quarter borrow the next flavour
            make_name(set_name[s], sizeof set_name[s], h, st, tries > 20 ? 0 : where);
            int dup = !strcmp(set_name[s], island_name); for (int o = 0; o < s && !dup; o++) if (!strcmp(set_name[o], set_name[s])) dup = 1;
            if (!dup) break;
        }
        static const char *PL[3] = { "hamlet", "village", "town" };
        snprintf(set_sub[s], sizeof set_sub[s], "place=%s;name=%s", PL[set[s].rank], set_name[s]);
    }
}
static int cn_away = -1;            // when >=0: only accept hits away from this cell (a 2nd way out)
// A* that keeps bridges honest: the per-cell distance-to-land gate can't stop a route running ALONG
// the water (11 km "bridges" hugging a coast at full appetite). If the found route has a water stretch
// longer than the slider's max span, it is routed again with no water at all.
static float astar_b(int s, int t, int mode, int *out, int *nout, float budget) {
    float c = astar(s, t, mode, out, nout, budget);
    if (c < 0) return c;
    // each water stretch must be a CROSSING: no longer than the slider's max span, and no longer than
    // ~2.5x the water's width there (2 x its furthest point from shore). A road running ALONG a coast
    // or a lake channel is many times longer than that (seen: 1.3 km in the shallows, an expressway
    // hopping down a lake chain).
    float run = 0, deep = 0; int bad = 0;
    for (int i = 1; i <= *nout && !bad; i++) {
        if (i == *nout || is_dry(out[i])) {
            if (run > 0 && (run > max_span() || run > 2.5f * (2.0f * deep + CELL))) bad = 1;
            run = 0; deep = 0; continue;
        }
        run += (out[i] % N != out[i - 1] % N && out[i] / N != out[i - 1] / N) ? CELL * 1.41421f : CELL;
        if (DL[out[i]] > deep) deep = DL[out[i]];
    }
    if (!bad) return c;
    no_water = 1; c = astar(s, t, mode, out, nout, budget); no_water = 0;
    return c;
}
static float connect_to_network(int s, int *out, int *nout, float budget) {
    agen++; hp_n = 0;
    ag[s] = 0; astamp[s] = agen; af[s] = -1; hp_push(s, 0);
    int hit = -1;
    while (hp_n > 0) {
        int c = hp_pop();
        if (aclosed[c] == agen) continue;
        aclosed[c] = agen;
        if (ag[c] > budget) break;
        if (c != s) {
            int on = TR[c] > 0; for (int k = 0; k < 8 && !on; k++) if (ER[c][k] && ER[c][k] != RC_EXPRESS) on = 1;   // no farm gates onto the expressway
            if (on && cn_away >= 0) {                    // must leave in another direction, and not too close
                float fx = s % N, fy = s / N, ax = cn_away % N - fx, ay = cn_away / N - fy, bx = c % N - fx, by = c / N - fy;
                if (ax * bx + ay * by > 0 || bx * bx + by * by < (625.0f / CELL) * (625.0f / CELL)) on = 0;
            }
            if (on) { hit = c; break; }
        }
        int cx = c % N, cy = c / N;
        for (int k = 0; k < 8; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            int n = ny * N + nx; if (aclosed[n] == agen) continue;
            float sc = step_cost(c, n, k, RM_TRACK); if (sc < 0) continue;
            float ng = ag[c] + sc;
            if (astamp[n] != agen || ng < ag[n]) { astamp[n] = agen; ag[n] = ng; af[n] = c; if (hp_n < N * N * 2 - 1) hp_push(n, ng); }
        }
    }
    if (hit < 0) return -1;
    int n = 0; for (int c = hit; c != -1; c = af[c]) out[n++] = c;
    *nout = n; return ag[hit];
}
static void farms(void) {
    nfarm = 0;
    // a jittered grid with a FRACTIONAL spacing (in cells): whole-cell spacing made the slider jump
    // between ~310 m and ~470 m with nothing between, and road density with it
    float g = fmaxf(k_farm_sp() / CELL, 1.5f);
    int ng = (int)(N / g) + 1;
    for (int jy = 0; jy < ng; jy++) for (int jx = 0; jx < ng; jx++) {
        unsigned h = hsh((unsigned)(jx * 73856093) ^ (unsigned)(jy * 19349663) ^ (unsigned)(seed * 83492791));
        int x = (int)((jx + (h & 255) / 256.0f) * g), y = (int)((jy + ((h >> 8) & 255) / 256.0f) * g);
        if (x >= N || y >= N) continue;
        int i = y * N + x;
        if (H[i] <= 0 || SU[i] <= 0.05f || river_cell(i) || TR[i] > 0) continue;
        if ((float)((h >> 16) & 1023) / 1023.0f > SU[i] * 2.6f) continue;   // keep with p ∝ suitability
        int near = 0;
        for (int s = 0; s < nset && !near; s++) {
            float dx = set[s].x - x, dy = set[s].y - y, rr = fmaxf(450.0f, set_r[s] + 150.0f) / CELL;
            if (dx * dx + dy * dy < rr * rr) near = 1;
        }
        if (near) continue;
        if (nfarm >= MAXFARM) { st_overflow++; continue; }
        farm[nfarm++] = i;
    }
    for (int f = 0; f < nfarm; f++) {
        int n;
        float cst = connect_to_network(farm[f], pbuf, &n, 4500);
        if (cst < 0) continue;
        mark(pbuf, n, RC_TRACK, 0, 0, 1);               // one way out: a private access track
        // a second way out for ~45% of farms: then both legs are a public through-lane, and the
        // tree of dead ends becomes loops
        unsigned h2 = hsh((unsigned)farm[f] * 31u + (unsigned)seed);
        if ((h2 & 1023) < 460) {
            static int first[MAXN * 4]; int n1 = n < MAXN * 4 ? n : MAXN * 4;
            for (int q = 0; q < n1; q++) first[q] = pbuf[q];
            cn_away = pbuf[0];                           // pbuf[0] = where the first connection hit
            if (connect_to_network(farm[f], pbuf, &n, 5500) >= 0) {
                mark(pbuf, n, RC_LANE, 0, 0, 0);
                mark(first, n1, RC_LANE, 0, 0, 0);       // raises the track to a lane (never lowers a road)
            }
            cn_away = -1;
        }
    }
    // FIELD TRACKS: every farm also has a dead-end track out into its own fields (300-650 m
    // toward the best nearby land not already on a road): much of real track length is this
    for (int f = 0; f < nfarm; f++) {
        int fx = farm[f] % N, fy = farm[f] / N, best = -1; float bs = 0;
        int rr = (int)(650.0f / CELL + 0.5f); float d2lo = (300.0f / CELL) * (300.0f / CELL), d2hi = (650.0f / CELL) * (650.0f / CELL);
        for (int y = fy - rr; y <= fy + rr; y++) for (int x = fx - rr; x <= fx + rr; x++) {
            if (x < 0 || y < 0 || x >= N || y >= N) continue;
            int d2 = (x - fx) * (x - fx) + (y - fy) * (y - fy);
            if (d2 < d2lo || d2 > d2hi) continue;            // 300..650 m out
            int i = y * N + x;
            if (H[i] <= 0 || TR[i] > 0 || river_cell(i)) continue;
            int on = 0; for (int k = 0; k < 8; k++) if (ER[i][k]) on = 1;
            if (on) continue;
            float sc = SU[i] + 0.15f * (float)(hsh((unsigned)i * 13u + (unsigned)seed) & 255) / 255.0f;
            if (sc > bs) { bs = sc; best = i; }
        }
        int n;
        if (best >= 0 && bs > 0.1f && astar(farm[f], best, RM_TRACK, pbuf, &n, 2000) >= 0) mark(pbuf, n, RC_TRACK, 0, 0, 1);
    }
}

// ── TOWN STREETS: towns and villages get a street grid instead of being a star of routes that all
// end at one centre cell. citygen.h's method (rung 4, calibrated to real cities), re-done on the
// road GRID so streets go through the same smoothing, junction consolidation, splitting and export
// as every other road: a rotated lattice (~130 m blocks) aligned to the main road in, clipped to a
// ragged disc on buildable land; nodes near an existing road snap onto it (the routes in become
// the town's main streets); a spanning tree for connectivity plus a fraction of loops (more in the
// core, fewer at the edge: a grid centre, cul-de-sacs outside). citygen.h itself isn't included:
// it reads worldnet.h's terrain, not this island's.
#define TS_LG 25
static int st_streets;                      // street segments laid (trace)
// the town-size slider sets the biggest town's radius; other places scale with sqrt(their size), so a
// market town spreads further than a small one. Villages ~0.38 of a town (230 m at the default).
static void settle_radii(void) {
    float R = 300.0f + k_town * 1200.0f, mx = 1e-6f;
    for (int s = 0; s < nset; s++) if (set[s].size > mx) mx = set[s].size;
    for (int s = 0; s < nset; s++) {
        float f = 0.7f + 0.3f * sqrtf(set[s].size / mx);
        set_r[s] = set[s].rank == ST_TOWN ? R * f : set[s].rank == ST_VILLAGE ? R * 0.38f * f : 120.0f;
    }
}
static int ts_find(int *uf, int x) { while (uf[x] != x) x = uf[x] = uf[uf[x]]; return x; }
static int ts_road(int c) { for (int k = 0; k < 8; k++) if (ER[c][k] && ER[c][k] != RC_EXPRESS) return 1; return 0; }
static int ts_land(int c) {
    if (H[c] <= 0 || is_lake(c) || river_cell(c)) return 0;
    for (int k = 0; k < 8; k++) if (ER[c][k] == RC_EXPRESS) return 0;   // no street onto the expressway
    return 1;
}
// the straight cell run a -> b (as line_cells walks it) into out; 0 if it leaves buildable land or is too steep
static int ts_path(int a, int b, int *out, int *n) {
    // Bresenham, 8-connected: the cells hug the true line, so the smoother draws a straight street
    // (walking diagonal-first then straight made an L the smoother turned into an S-wiggle)
    int x = a % N, y = a / N, tx = b % N, ty = b / N; *n = 0;
    int dx = abs(tx - x), dy = -abs(ty - y), sx = tx > x ? 1 : -1, sy = ty > y ? 1 : -1, err = dx + dy;
    out[(*n)++] = a;
    while (x != tx || y != ty) {
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
        int c = y * N + x; if (!ts_land(c)) return 0;
        out[(*n)++] = c;
    }
    float len = hypotf((float)(a % N - b % N), (float)(a / N - b / N)) * CELL;
    return fabsf(H[a] - H[b]) / fmaxf(len, 1) < 0.12f;
}
static void town_streets(void) {
    st_streets = 0;
    static int lg[TS_LG][TS_LG], uf[TS_LG * TS_LG], deg[TS_LG * TS_LG], onr[TS_LG * TS_LG], cellof[TS_LG * TS_LG];
    static float rad[TS_LG * TS_LG];
    static int ea[2 * TS_LG * TS_LG], eb[2 * TS_LG * TS_LG]; static float ew[2 * TS_LG * TS_LG];
    static int path[MAXN * 2];
    for (int s = 0; s < nset; s++) {
        if (set[s].rank == ST_HAMLET) continue;
        float R = set_r[s];
        float step = fmaxf(130.0f, CELL);
        int c0 = set[s].y * N + set[s].x;
        float cx = (set[s].x + 0.5f) * CELL, cy = (set[s].y + 0.5f) * CELL;
        unsigned hs = hsh((unsigned)s * 2654435761u ^ (unsigned)seed * 97u);
        // orientation: along the biggest road into the centre (walked ~250 m out), else hashed
        float ang = (hs & 1023) / 1023.0f * 3.14159f; int best = 0;
        for (int k = 0; k < 8; k++) if (ER[c0][k] > best && ER[c0][k] != RC_EXPRESS) {
            best = ER[c0][k]; int cur = c0, kk = k, steps = (int)(250.0f / CELL) + 1;
            for (int q = 0; q < steps; q++) {
                int nx = cur % N + DX[kk], ny = cur / N + DY[kk]; cur = ny * N + nx;
                int nk = -1; for (int j = 0; j < 8; j++) if (ER[cur][j] && j != OPP[kk]) { nk = j; break; }
                if (nk < 0) break; kk = nk;
            }
            ang = atan2f((float)(cur / N - set[s].y), (float)(cur % N - set[s].x));
        }
        float ca = cosf(ang), sa = sinf(ang);
        int half = (int)(R / step) + 1; if (2 * half + 1 > TS_LG) half = (TS_LG - 1) / 2;
        int n = 0;
        for (int r = -half; r <= half; r++) for (int c = -half; c <= half; c++) {
            int *slot = &lg[r + half][c + half]; *slot = -1;
            unsigned h = hsh(hs ^ (unsigned)((r + 31) * 131 + (c + 31)));
            float d = hypotf((float)r, (float)c) * step;
            float jit = d < R * 0.45f ? 0.08f : 0.30f;            // a regular core, a looser edge
            float lx = c * step + ((h & 255) / 255.0f - 0.5f) * step * jit;
            float ly = r * step + (((h >> 8) & 255) / 255.0f - 0.5f) * step * jit;
            float px = cx + lx * ca - ly * sa, py = cy + lx * sa + ly * ca;
            float rr = hypotf(px - cx, py - cy);
            if (rr > R * (0.8f + 0.35f * ((h >> 16) & 255) / 255.0f)) continue;   // ragged edge
            int gx = (int)(px / CELL), gy = (int)(py / CELL);
            if (gx < 1 || gy < 1 || gx >= N - 1 || gy >= N - 1) continue;
            int cell = (r == 0 && c == 0) ? c0 : gy * N + gx, on = r == 0 && c == 0;
            if (!on) {                                            // snap onto a road nearby: it becomes a street
                int sr = (int)(fmaxf(CELL, step * 0.3f) / CELL + 0.5f); float bd = 1e9f;
                for (int yy = gy - sr; yy <= gy + sr; yy++) for (int xx = gx - sr; xx <= gx + sr; xx++) {
                    int q = yy * N + xx; if (!ts_road(q)) continue;
                    float dd = (float)((xx - gx) * (xx - gx) + (yy - gy) * (yy - gy));
                    if (dd < bd) { bd = dd; cell = q; on = 1; }
                }
                if (!on && (!ts_land(cell) || slope_at(cell % N, cell / N) > 0.15f)) continue;
            }
            int dup = 0; for (int q = 0; q < n; q++) if (cellof[q] == cell) dup = 1;
            if (dup) continue;
            *slot = n; cellof[n] = cell; onr[n] = on; rad[n] = rr; uf[n] = n; deg[n] = 0; n++;
        }
        if (n < 2) continue;
        int root = -1;                                            // everything on a road is already connected
        for (int q = 0; q < n; q++) if (onr[q]) { if (root < 0) root = q; else uf[ts_find(uf, q)] = ts_find(uf, root); }
        int ne = 0;
        for (int r = 0; r < 2 * half + 1; r++) for (int c = 0; c < 2 * half + 1; c++) {
            int a = lg[r][c]; if (a < 0) continue;
            int nb[2] = { c + 1 < 2 * half + 1 ? lg[r][c + 1] : -1, r + 1 < 2 * half + 1 ? lg[r + 1][c] : -1 };
            for (int e = 0; e < 2; e++) if (nb[e] >= 0) {
                ea[ne] = a; eb[ne] = nb[e];
                ew[ne] = (hsh(hs ^ (unsigned)(a * 977 + nb[e] * 31 + e)) & 65535) / 65535.0f; ne++;
            }
        }
        for (int i = 1; i < ne; i++) for (int j = i; j > 0 && ew[j] < ew[j - 1]; j--) {
            float tw = ew[j]; ew[j] = ew[j - 1]; ew[j - 1] = tw;
            int t = ea[j]; ea[j] = ea[j - 1]; ea[j - 1] = t; t = eb[j]; eb[j] = eb[j - 1]; eb[j - 1] = t;
        }
        for (int i = 0; i < ne; i++) {
            int a = ea[i], b = eb[i], np;
            if (onr[a] && onr[b]) {                               // both on roads: skip if that road already joins them
                if (!ts_path(cellof[a], cellof[b], path, &np)) continue;
                int all = 1; for (int q = 0; q < np && all; q++) if (!ts_road(path[q])) all = 0;
                if (all) continue;
            }
            int ra = ts_find(uf, a), rb = ts_find(uf, b);
            float d = (rad[a] + rad[b]) * 0.5f;
            float loopf = d < R * 0.5f ? 0.85f : 0.4f;           // closed blocks in the core, cul-de-sacs outside          // grid core, tree-like edge
            int cap = d < R * 0.5f ? 4 : 3;                      // crossroads in the core, Ts outside
            int tree = ra != rb;
            if (!tree && !((hsh(hs ^ (unsigned)(i * 7919)) & 1023) / 1023.0f < loopf && deg[a] < cap && deg[b] < cap)) continue;
            if (!ts_path(cellof[a], cellof[b], path, &np)) continue;
            mark(path, np, RC_LANE, 0, 0, 0);
            deg[a]++; deg[b]++; st_streets++;
            if (tree) uf[ra] = rb;
        }
        // pieces cut off by water or slope: join each to the town by the cheapest route, if short
        int rr0 = root >= 0 ? ts_find(uf, root) : -1;
        for (int q = 0; q < n; q++) {
            int rq = ts_find(uf, q); if (rq == rr0 || rq != q) continue;
            int bestq = -1; float bd = 1e9f;
            for (int p = 0; p < n; p++) if (ts_find(uf, p) == rq && rad[p] < bd) { bd = rad[p]; bestq = p; }
            int np2;
            if (astar_b(cellof[bestq], c0, RM_LINK, pbuf, &np2, step * 4) >= 0) mark(pbuf, np2, RC_LANE, 0, 0, 0);
            uf[rq] = rr0 >= 0 ? rr0 : rq;
        }
    }
}

// ── 5 (v2): carve valleys from the SMOOTHED river lines, not per cell. Per-cell carving left a
// staircase along every diagonal river (an 8-direction chain touches only at corners, so its
// neighbours stayed high): sawtooth walls, rock teeth, diagonal bands in the hillshade. Now each
// cell is lowered by its distance to the nearest river line, with a valley whose depth and
// half-width grow with the area drained: diagonal valleys are as smooth as straight ones. ──────
#define CARVE_CELLS (80.0f * AREA_K)      // ~2 km²: below this a stream is too small to cut a valley (D8 streams on an even slope run parallel at 45°; carving each made stripes)
static void carve_lines(void) {
    static unsigned char up[MAXN * MAXN];
    static int cells[MAXN * 4];
    float km2 = CELL * CELL / 1e6f;
    for (int i = 0; i < N * N; i++) { C[i] = 0; up[i] = 0; }
    #define CARVE_OK(i) (H[i] > 0 && A[i] >= CARVE_CELLS)
    for (int i = 0; i < N * N; i++) if (CARVE_OK(i) && rcv[i] >= 0 && CARVE_OK(rcv[i]) && up[rcv[i]] < 255) up[rcv[i]]++;
    for (int i = 0; i < N * N; i++) {
        if (!CARVE_OK(i) || up[i] == 1) continue;
        int n = 0, c = i;
        for (;;) {
            if (n < MAXN * 4) cells[n++] = c;
            int nx = rcv[c];
            if (nx < 0) break;
            if (!CARVE_OK(nx) || up[nx] != 1) { if (n < MAXN * 4) cells[n++] = nx; break; }
            c = nx;
        }
        if (n < 2) continue;
        int m = smooth_path(cells, n);
        for (int k = 1; k < m; k++) {
            int ci = cells[(int)((float)k / (m - 1) * (n - 1) + 0.5f)];
            if (H[ci] <= 0 && k > 1) ci = cells[(int)((float)(k - 1) / (m - 1) * (n - 1))];
            float ak = sqrtf(fmaxf(A[ci], 1) * km2);
            float D = k_ero * 55.0f * ak, W = 110.0f + 95.0f * ak;          // depth, half-width (m)
            float ax = sm_x[k - 1], ay = sm_y[k - 1], bx = sm_x[k], by = sm_y[k];
            int x0 = (int)((fminf(ax, bx) - W) / CELL), x1 = (int)((fmaxf(ax, bx) + W) / CELL);
            int y0 = (int)((fminf(ay, by) - W) / CELL), y1 = (int)((fmaxf(ay, by) + W) / CELL);
            if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 >= N) x1 = N - 1; if (y1 >= N) y1 = N - 1;
            float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy + 1e-6f;
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
                float px = (x + 0.5f) * CELL, py = (y + 0.5f) * CELL;
                float t = clamp(((px - ax) * dx + (py - ay) * dy) / l2, 0, 1);
                float ex = ax + dx * t - px, ey = ay + dy * t - py;
                float d = sqrtf(ex * ex + ey * ey) / W;
                if (d >= 1) continue;
                float v = D * (1 - d * d * (3 - 2 * d));                     // smooth U-shaped profile
                if (v > C[y * N + x]) C[y * N + x] = v;
            }
        }
    }
    #undef CARVE_OK
    for (int i = 0; i < N * N; i++) if (H[i] > 0) H[i] = fmaxf(H[i] - fminf(C[i], H[i] * 0.7f), 0.5f);
}

// BREACH: carving is capped at 70 % of local height, so near the low coast a river bed could end
// up running uphill, and every such dip filled as a lake. Walk each river upstream → downstream
// (reverse flood order) and lower any bed cell that sits above the one feeding it, unless the cut
// would exceed 25 m: those stay as real lakes. Rivers cut down to the sea, like real ones.
static void breach(void) {
    for (int j = norder - 1; j >= 0; j--) {
        int i = order[j];
        if (H[i] <= 0 || A[i] < CARVE_CELLS) continue;
        int r = rcv[i]; if (r < 0 || H[r] <= 0) continue;
        float want = H[i] - 0.05f;
        if (H[r] > want && H[r] - want < 25.0f) H[r] = fmaxf(want, 0.3f);
    }
}

// ── RIVERS as smooth lines (zoom view): each chain runs from a source or confluence down to
// the next confluence or the sea, smoothed like a road, width from the area it drains. ─────
#define STREAM_CELLS (RIVER_CELLS * 0.25f)
#define MAXRV  2000
#define MAXRPT 60000
typedef struct { int start, n; } RPoly;
static RPoly rv[MAXRV]; static int nrv;
static float rvx[MAXRPT], rvy[MAXRPT], rvw[MAXRPT]; static int nrpt;
static float river_w(float a) { return clamp(sqrtf(a / AREA_K) * 0.35f, 1.5f, 30.0f); }   // metres
static int stream_cell(int i) { return H[i] > 0 && !is_lake(i) && A[i] >= STREAM_CELLS; }
static void build_rivers(void) {
    static unsigned char up[MAXN * MAXN];
    static int cells[MAXN * 4];
    nrv = 0; nrpt = 0;
    for (int i = 0; i < N * N; i++) up[i] = 0;
    for (int i = 0; i < N * N; i++) if (stream_cell(i) && rcv[i] >= 0 && stream_cell(rcv[i]) && up[rcv[i]] < 255) up[rcv[i]]++;
    for (int i = 0; i < N * N; i++) {
        if (!stream_cell(i) || up[i] == 1) continue;           // chains start at sources and confluences
        int n = 0, c = i;
        for (;;) {
            if (n < MAXN * 4) cells[n++] = c;
            int nx = rcv[c];
            if (nx < 0) break;
            if (!stream_cell(nx)) { if (n < MAXN * 4) cells[n++] = nx; break; }   // into the sea / a lake
            if (up[nx] != 1) { if (n < MAXN * 4) cells[n++] = nx; break; }        // a confluence ends the chain
            c = nx;
        }
        if (n < 2) continue;
        if (nrv >= MAXRV) { st_overflow++; continue; }
        int m = smooth_path(cells, n);
        if (nrpt + m >= MAXRPT) { st_overflow++; break; }
        rv[nrv].start = nrpt; rv[nrv].n = m; nrv++;
        for (int k = 0; k < m; k++) {
            int ci = cells[(int)((float)k / (m - 1 > 0 ? m - 1 : 1) * (n - 1) + 0.5f)];
            rvx[nrpt + k] = sm_x[k]; rvy[nrpt + k] = sm_y[k];
            rvw[nrpt + k] = river_w(H[ci] > 0 ? A[ci] : A[cells[n > 1 ? n - 2 : 0]]);
        }
        nrpt += m;
    }
}

// ── BRIDGES: wherever a road line crosses a river line wider than a culvert ─────────────
#define MAXBR 2000
#define BG 64                                  // bucket grid over the world for river segments
typedef struct { float x, y, dx, dy, len; int cls, p, i; float wl; } Bridge;   // wl: water level under a water span (m), -1 = not over open water   // p, i: the road poly + point index it sits on
static Bridge br[MAXBR]; static int nbr;
static int seg_x(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy, float *ix, float *iy) {
    float rx = bx - ax, ry = by - ay, sx = dx - cx, sy = dy - cy, den = rx * sy - ry * sx;
    if (fabsf(den) < 1e-6f) return 0;
    float t = ((cx - ax) * sy - (cy - ay) * sx) / den, u = ((cx - ax) * ry - (cy - ay) * rx) / den;
    if (t < 0 || t > 1 || u < 0 || u > 1) return 0;
    *ix = ax + rx * t; *iy = ay + ry * t; return 1;
}
static void build_bridges(void) {
    static int bh[BG * BG], bnext[MAXRPT];
    float bs = WORLD_M / BG;
    nbr = 0;
    for (int i = 0; i < BG * BG; i++) bh[i] = -1;
    for (int r = 0; r < nrv; r++)                              // bucket every river segment by its midpoint
        for (int i = rv[r].start + 1; i < rv[r].start + rv[r].n; i++) {
            int bx = (int)((rvx[i] + rvx[i - 1]) * 0.5f / bs), by = (int)((rvy[i] + rvy[i - 1]) * 0.5f / bs);
            if (bx < 0 || by < 0 || bx >= BG || by >= BG) continue;
            bnext[i] = bh[by * BG + bx]; bh[by * BG + bx] = i;
        }
    for (int p = 0; p < npl; p++)
        for (int i = pl[p].start + 1; i < pl[p].start + pl[p].n; i++) {
            float ax = plx[i - 1], ay = ply[i - 1], bx = plx[i], by = ply[i];
            int gx = (int)((ax + bx) * 0.5f / bs), gy = (int)((ay + by) * 0.5f / bs);
            for (int qy = gy - 1; qy <= gy + 1; qy++) for (int qx = gx - 1; qx <= gx + 1; qx++) {
                if (qx < 0 || qy < 0 || qx >= BG || qy >= BG) continue;
                for (int j = bh[qy * BG + qx]; j >= 0; j = bnext[j]) {
                    float ix, iy;
                    if (!seg_x(ax, ay, bx, by, rvx[j - 1], rvy[j - 1], rvx[j], rvy[j], &ix, &iy)) continue;
                    float w = rvw[j];
                    if (w < 3.5f) continue;                    // a culvert, not a bridge
                    int dup = 0;
                    for (int k = 0; k < nbr && !dup; k++) if (fabsf(br[k].x - ix) < 25 && fabsf(br[k].y - iy) < 25) dup = 1;
                    if (dup) continue;
                    if (nbr >= MAXBR) { st_overflow++; continue; }
                    float rdx = bx - ax, rdy = by - ay, rl = sqrtf(rdx * rdx + rdy * rdy) + 1e-6f; rdx /= rl; rdy /= rl;
                    float wdx = rvx[j] - rvx[j - 1], wdy = rvy[j] - rvy[j - 1], wl = sqrtf(wdx * wdx + wdy * wdy) + 1e-6f;
                    float sn = fabsf(rdx * wdy / wl - rdy * wdx / wl);   // |sin| of the crossing angle
                    br[nbr].x = ix; br[nbr].y = iy; br[nbr].dx = rdx; br[nbr].dy = rdy;
                    br[nbr].len = fminf(w / fmaxf(sn, 0.35f) + 10.0f, 160.0f); br[nbr].cls = pl[p].cls; br[nbr].p = p; br[nbr].i = i; br[nbr].wl = -1; nbr++;
                }
            }
        }
}

// WATER BRIDGES: where a road runs over open water (a lake or a sea inlet the bridging slider let it
// cross), the whole run becomes one bridge span (+20 m abutments each side), tagged B1 like a river
// bridge; any river bridge inside that run is folded into it.
static int st_waterbr; static float st_waterbr_max;
static float H_bilinear(float x, float y);
// open water under a road point: a lake cell, or the SMOOTH terrain below sea level (cell lookup on
// 156 m cells called a coast road skimming the shoreline a 240 m bridge)
static int water_at(float x, float y) {
    int c = (int)(y / CELL) * N + (int)(x / CELL);
    if (c < 0 || c >= N * N) return 0;
    return is_lake(c) || H_bilinear(x, y) < -1.0f;
}
static void build_water_bridges(void) {
    st_waterbr = 0; st_waterbr_max = 0;
    for (int p = 0; p < npl; p++) {
        int s0 = pl[p].start, e0 = s0 + pl[p].n - 1;
        for (int i = s0; i <= e0; i++) {
            if (!water_at(plx[i], ply[i])) continue;
            int j = i, routed = 0; float L = 0;
            for (int q = i;; q++) {                           // did the ROUTE go through water here? (else a
                int c = (int)(ply[q] / CELL) * N + (int)(plx[q] / CELL);   // smoothed line just clips a shore)
                if (c >= 0 && c < N * N && !is_dry(c)) for (int k = 0; k < 8; k++) if (ER[c][k]) routed = 1;
                if (q >= e0 || !water_at(plx[q + 1], ply[q + 1])) break;
                L += hypotf(plx[q + 1] - plx[q], ply[q + 1] - ply[q]); j = q + 1;
            }
            if (routed && L > 30.0f) {                                  // shorter = a smoothed road clipping a cell corner of the shore
                float half = L * 0.5f, acc = 0; int mid = i;
                while (mid < j && acc + hypotf(plx[mid + 1] - plx[mid], ply[mid + 1] - ply[mid]) < half) { acc += hypotf(plx[mid + 1] - plx[mid], ply[mid + 1] - ply[mid]); mid++; }
                if (mid == s0) mid++;
                for (int b = 0; b < nbr; b++)                     // fold river bridges inside the run into it
                    if (br[b].p == p && br[b].i >= i && br[b].i <= j + 1) { br[b] = br[--nbr]; b--; }
                if (nbr >= MAXBR) { st_overflow++; break; }
                float dx = plx[mid] - plx[mid - 1], dy = ply[mid] - ply[mid - 1], l = sqrtf(dx * dx + dy * dy) + 1e-6f;
                br[nbr].x = plx[mid]; br[nbr].y = ply[mid]; br[nbr].dx = dx / l; br[nbr].dy = dy / l;
                br[nbr].len = L + 40.0f; br[nbr].cls = pl[p].cls; br[nbr].p = p; br[nbr].i = mid;
                { int c = (int)(ply[mid] / CELL) * N + (int)(plx[mid] / CELL); br[nbr].wl = (c >= 0 && c < N * N && is_lake(c)) ? F[c] : 0.0f; }
                nbr++;
                st_waterbr++; if (L > st_waterbr_max) st_waterbr_max = L;
            }
            i = j;
        }
    }
}

// FLYOVERS: every cell where the expressway and another road share a cell (and it isn't a town the
// expressway ends in) is a crossing: the expressway gets a 260 m bridge span centred on it, which the
// export tags like a river bridge and backroads raises over the road underneath.
static int st_flyovers;
static void build_flyovers(void) {
    st_flyovers = 0;
    for (int c = 0; c < N * N; c++) {
        if (IS_SET[c]) continue;
        int e = 0, o = 0;
        for (int k = 0; k < 8; k++) if (ER[c][k]) { if (ER[c][k] == RC_EXPRESS) e++; else o++; }
        if (!e || o < 2) continue;                       // the road must pass THROUGH (a dead end touching the expressway isn't a crossing)
        float cx = (c % N + 0.5f) * CELL, cy = (c / N + 0.5f) * CELL;
        int bp = -1, bi = -1; float bd = 1e30f;                       // nearest expressway point
        for (int p = 0; p < npl; p++) {
            if (pl[p].cls != RC_EXPRESS) continue;
            for (int i = pl[p].start + 1; i < pl[p].start + pl[p].n; i++) {
                float d = (plx[i] - cx) * (plx[i] - cx) + (ply[i] - cy) * (ply[i] - cy);
                if (d < bd) { bd = d; bp = p; bi = i; }
            }
        }
        if (bp < 0 || bd > CELL * CELL * 4) continue;
        int dup = 0;
        for (int k = 0; k < nbr && !dup; k++) if (br[k].p == bp && fabsf(br[k].x - plx[bi]) < 100 && fabsf(br[k].y - ply[bi]) < 100) dup = 1;
        if (dup || nbr >= MAXBR) continue;
        float dx = plx[bi] - plx[bi - 1], dy = ply[bi] - ply[bi - 1], l = sqrtf(dx * dx + dy * dy) + 1e-6f;
        br[nbr].x = plx[bi]; br[nbr].y = ply[bi]; br[nbr].dx = dx / l; br[nbr].dy = dy / l;
        br[nbr].len = 260.0f; br[nbr].cls = RC_EXPRESS; br[nbr].p = bp; br[nbr].i = bi; br[nbr].wl = -1; nbr++;
        st_flyovers++;
    }
}

// ── INTERCHANGES: a DIAMOND at selected flyovers (the crossing road a public road: lane or better, ≥ 3 km apart
// along the expressway, ≥ 1.5 km from its ends). Four slip roads, drive-on-the-right: per direction an
// exit ~320 m before the bridge and an entrance after it, on that direction's right, each an S-curve
// (roadkit's scurve_spline, roadlab's flyover biarc) from the expressway's edge to the crossing road
// ~110 m out, where it meets it at grade. The expressway is up on its deck there; the ramps stay down.
static int st_interchanges;
static int ex_nearest(int cls_express, float x, float y, int *op, int *oi) {   // nearest poly point of a class set
    float bd = 1e30f; *op = -1; *oi = -1;
    for (int p = 0; p < npl; p++) {
        if ((pl[p].cls == RC_EXPRESS) != cls_express) continue;
        if (!cls_express && pl[p].cls < RC_LANE) continue;  // the crossing road: a public road (lane or better), not a track
        for (int i = pl[p].start + 1; i < pl[p].start + pl[p].n; i++) {
            float d = (plx[i] - x) * (plx[i] - x) + (ply[i] - y) * (ply[i] - y);
            if (d < bd) { bd = d; *op = p; *oi = i; }
        }
    }
    return bd < 60.0f * 60.0f;
}
// snap a ramp end onto the crossing road and SPLIT that road there, so the ramps and the road share a
// point: backroads then finds a real junction (curb corners) instead of two overlapping strips
static int snap_find(float x, float y, int *op, int *oi) {   // nearest crossing-road vertex within 40 m
    int bp = -1, bi = -1; float bd = 40.0f * 40.0f;
    for (int p = 0; p < npl; p++) {
        if (pl[p].cls == RC_EXPRESS || pl[p].cls < RC_LANE) continue;
        for (int i = pl[p].start; i < pl[p].start + pl[p].n; i++) {
            float d = (plx[i] - x) * (plx[i] - x) + (ply[i] - y) * (ply[i] - y);
            if (d < bd) { bd = d; bp = p; bi = i; }
        }
    }
    *op = bp; *oi = bi; return bp >= 0;
}
static void snap_split(float *x, float *y) {
    int bp = -1, bi = -1; float bd = 40.0f * 40.0f;
    for (int p = 0; p < npl; p++) {
        if (pl[p].cls == RC_EXPRESS || pl[p].cls < RC_LANE) continue;
        for (int i = pl[p].start; i < pl[p].start + pl[p].n; i++) {
            float d = (plx[i] - *x) * (plx[i] - *x) + (ply[i] - *y) * (ply[i] - *y);
            if (d < bd) { bd = d; bp = p; bi = i; }
        }
    }
    if (bp < 0) return;
    *x = plx[bi]; *y = ply[bi];
    int a = pl[bp].start, e = a + pl[bp].n - 1;
    if (bi <= a || bi >= e || npl >= MAXPL) return;          // already an end: nothing to split
    pl[npl].start = bi; pl[npl].n = e - bi + 1; pl[npl].cls = pl[bp].cls; pl_ramp[npl] = pl_ramp[bp];   // the tail, sharing point bi
    pl[bp].n = bi - a + 1;
    for (int k = 0; k < nbr; k++) if (br[k].p == bp && br[k].i > bi) br[k].p = npl;   // bridges on the tail
    npl++;
}
static void add_ramp(Port a, Port b) {
    static float xs[256], ys[256];
    int n = scurve_spline(a, b, xs, ys);
    if (n < 2 || npl >= MAXPL || npt + n >= MAXPT) { st_overflow++; return; }
    pl[npl].start = npt; pl[npl].n = n; pl[npl].cls = RC_SEC; pl_ramp[npl] = 1; npl++;
    float L = 0;
    for (int i = 0; i < n; i++) { plx[npt + i] = xs[i]; ply[npt + i] = ys[i]; if (i) L += hypotf(xs[i] - xs[i-1], ys[i] - ys[i-1]); }
    npt += n; st_road[RC_SEC] += L / 1000.0f;
}
static void build_interchanges(void) {
    st_interchanges = 0;
    static float icx[64], icy[64]; int nic = 0;
    float ex0x = 0, ex0y = 0, ex1x = 0, ex1y = 0; int have_ends = 0;   // the expressway's two ends
    for (int p = 0; p < npl; p++) if (pl[p].cls == RC_EXPRESS) {
        int a = pl[p].start, e = a + pl[p].n - 1;
        if (!have_ends) { ex0x = plx[a]; ex0y = ply[a]; ex1x = plx[e]; ex1y = ply[e]; have_ends = 1; }
        else {                                                // keep the two points farthest apart
            float cand[2][2] = { { plx[a], ply[a] }, { plx[e], ply[e] } };
            for (int c = 0; c < 2; c++) {
                float d0 = hypotf(cand[c][0] - ex1x, cand[c][1] - ex1y), dc = hypotf(ex0x - ex1x, ex0y - ex1y);
                float d1 = hypotf(cand[c][0] - ex0x, cand[c][1] - ex0y);
                if (d0 > dc && d0 >= d1) { ex0x = cand[c][0]; ex0y = cand[c][1]; }
                else if (d1 > dc) { ex1x = cand[c][0]; ex1y = cand[c][1]; }
            }
        }
    }
    for (int b = 0; b < nbr && nic < 64; b++) {
        if (br[b].cls != RC_EXPRESS) continue;
        float X = br[b].x, Y = br[b].y;
        if (hypotf(X - ex0x, Y - ex0y) < 1500 || hypotf(X - ex1x, Y - ex1y) < 1500) continue;
        int far = 1; for (int k = 0; k < nic; k++) if (hypotf(icx[k] - X, icy[k] - Y) < 3000) far = 0;
        if (!far) continue;
        int cp, ci;                                           // the crossing road: secondary or main
        if (!ex_nearest(0, X, Y, &cp, &ci)) continue;
        int c0 = ci > pl[cp].start ? ci - 1 : ci, c1 = ci + 1 < pl[cp].start + pl[cp].n ? ci + 1 : ci;
        float cx = plx[c1] - plx[c0], cy = ply[c1] - ply[c0], cl = hypotf(cx, cy); if (cl < 1) continue;
        cx /= cl; cy /= cl;
        float ex = br[b].dx, ey = br[b].dy;                   // expressway direction
        if (fabsf(ex * cx + ey * cy) > 0.8f) continue;        // too shallow a crossing for a diamond
        float rx = -ey, ry = ex;                              // right of +e (y down: rotate +90°)
        if (cx * rx + cy * ry < 0) { cx = -cx; cy = -cy; }    // c now points to the right side of +e
        float E = atan2f(ey, ex) * RK_R2D, Ld = 320.0f, Wc = 110.0f, off = RHW[RC_EXPRESS];
        // +e: exit before (right), entrance after (right); -e: exit "before" in its own sense = after (left), entrance before (left)
        Port a, q;
        float Rx = X + cx * Wc, Ry = Y + cy * Wc, Lx = X - cx * Wc, Ly = Y - cy * Wc;   // where ramps meet the road
        int sp1, si1, sp2, si2;                               // the road must be there on BOTH sides
        if (!snap_find(Rx, Ry, &sp1, &si1) || !snap_find(Lx, Ly, &sp2, &si2)) continue;
        snap_split(&Rx, &Ry); snap_split(&Lx, &Ly);
        a = (Port){ X - ex * Ld + rx * off, Y - ey * Ld + ry * off, E, 0 };  q = (Port){ Rx, Ry, E, 0 };                                add_ramp(a, q);
        a = (Port){ Rx, Ry, E, 0 };                                         q = (Port){ X + ex * Ld + rx * off, Y + ey * Ld + ry * off, E, 0 }; add_ramp(a, q);
        a = (Port){ X + ex * Ld - rx * off, Y + ey * Ld - ry * off, E + 180, 0 }; q = (Port){ Lx, Ly, E + 180, 0 };                      add_ramp(a, q);
        a = (Port){ Lx, Ly, E + 180, 0 };                                   q = (Port){ X - ex * Ld - rx * off, Y - ey * Ld - ry * off, E + 180, 0 }; add_ramp(a, q);
        icx[nic] = X; icy[nic] = Y; nic++; st_interchanges++;
    }
}

// ── FORESTRY + UPLAND TRACKS: forests are worked, uplands are grazed. Points on a jittered grid
// inside forest (~850 m) and on moor / high scrub (~1.2 km, sparser), each joined to the nearest
// road as a track: dead ends that climb into the woods and onto the hill. ────────────────────
static int nforest, nupland;
static void upland_tracks(void) {
    nforest = nupland = 0;
    for (int pass = 0; pass < 2; pass++) {
        float sp = pass == 0 ? 850.0f : 1200.0f;
        int g = (int)(sp / CELL);
        for (int gy = 0; gy < N; gy += g) for (int gx = 0; gx < N; gx += g) {
            unsigned h = hsh((unsigned)(gx * 19349663) ^ (unsigned)(gy * 73856093) ^ (unsigned)(seed * 2654435761u) ^ (unsigned)(pass * 977));
            int x = gx + (int)(h % (unsigned)g), y = gy + (int)((h >> 8) % (unsigned)g);
            if (x >= N || y >= N) continue;
            int i = y * N + x, c = CV[i];
            int ok = pass == 0 ? c == CV_FOREST : (c == CV_MOOR || (c == CV_SCRUB && H[i] > 250));
            if (!ok || TR[i] > 0 || river_cell(i)) continue;
            if ((h >> 20 & 1023) > (pass == 0 ? 660u : 560u)) continue;
            int n;
            if (connect_to_network(i, pbuf, &n, pass == 0 ? 5000 : 14000) < 0) continue;   // the hill is far and steep: a long budget
            mark(pbuf, n, RC_TRACK, 0, 0, 1);
            if (pass == 0) nforest++; else nupland++;
        }
    }
}

// ── HOUSES: islandstudio decides WHERE buildings go, backroads how they look; the types are the shared
// catalog in runtime/buildingkit.h (OpenStreetMap's vocabulary, docs/design/island-buildings.md).
// Exported exactly as osm-roads.js writes a real city: kind "building", sub = the OSM building=
// value (+ ";key=value"), height field = metres (storeys x 3), one closed rectangle each. Along every lane / secondary / main road inside a settlement's built-up
// radius, both sides, front to the road, set back 4-8 m: terraces + shops in a town's core, detached
// houses further out and in villages, a sparse few in hamlets; one church per town and village near
// the centre; a farmhouse + one or two barns at every farm. Never on a road (every corner keeps
// clear of every road's edge), in water, on steep ground, or on another building.
#define MAXHOUSE 24000
typedef struct { float x, y, ux, uy, w, d; unsigned char h, t; } House;   // centre, frontage dir, frontage w, depth d, h storeys, t = BK_* row
static House hs[MAXHOUSE]; static int nhs, st_htype[BK_N];
#define RSG 625                                   // road-segment buckets (64 m) for clearance tests
static int rs_head[RSG * RSG], rs_next[MAXPT]; static unsigned char rs_cls[MAXPT];
static int hs_head[RSG * RSG], hs_next[MAXHOUSE];
static short own[MAXN * MAXN];                    // settlement owning a cell (built-up radius), -1 none
static float hs_rand(unsigned *st) { *st = hsh(*st + 0x9e3779b9u); return (*st & 0xFFFFFF) / (float)0x1000000; }
static void hs_corners(const House *q, float *cx, float *cy) {
    float vx = -q->uy, vy = q->ux;                // depth direction
    float a[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    for (int k = 0; k < 4; k++) { cx[k] = q->x + q->ux * a[k][0] * q->w * 0.5f + vx * a[k][1] * q->d * 0.5f;
                                  cy[k] = q->y + q->uy * a[k][0] * q->w * 0.5f + vy * a[k][1] * q->d * 0.5f; }
}
static float road_clear(float x, float y) {      // distance from (x,y) to the nearest road EDGE (m)
    int bx = (int)(x / 64.0f), by = (int)(y / 64.0f); float best = 1e9f;
    for (int gy = by - 1; gy <= by + 1; gy++) for (int gx = bx - 1; gx <= bx + 1; gx++) {
        if (gx < 0 || gy < 0 || gx >= RSG || gy >= RSG) continue;
        for (int i = rs_head[gy * RSG + gx]; i >= 0; i = rs_next[i]) {
            float ax = plx[i - 1], ay = ply[i - 1], dx = plx[i] - ax, dy = ply[i] - ay, L2 = dx * dx + dy * dy;
            float t = L2 > 1e-6f ? ((x - ax) * dx + (y - ay) * dy) / L2 : 0; t = t < 0 ? 0 : t > 1 ? 1 : t;
            float ex = ax + t * dx - x, ey = ay + t * dy - y;
            float d = sqrtf(ex * ex + ey * ey) - RHW[rs_cls[i]];
            if (d < best) best = d;
        }
    }
    return best;
}
static float hs_slope_max = 0.2f;                 // landmarks on headlands and hilltops may sit steeper
static int buildable(float x, float y) {
    if (x < 0 || y < 0 || x >= WORLD_M || y >= WORLD_M) return 0;
    int c = (int)(y / CELL) * N + (int)(x / CELL);
    return is_dry(c) && H_bilinear(x, y) > 0.5f && !river_cell(c) && slope_at(c % N, c / N) < hs_slope_max;
}
static int try_house(House q) {
    if (nhs >= MAXHOUSE) { st_overflow++; return 0; }
    float cx[4], cy[4]; hs_corners(&q, cx, cy);
    if (!buildable(q.x, q.y) || road_clear(q.x, q.y) < 2.0f) return 0;
    for (int k = 0; k < 4; k++) if (!buildable(cx[k], cy[k]) || road_clear(cx[k], cy[k]) < 1.5f) return 0;
    for (int k = 0; k < 4; k++) {                 // edge midpoints too (a long terrace can straddle a lane)
        float mx = (cx[k] + cx[(k + 1) & 3]) * 0.5f, my = (cy[k] + cy[(k + 1) & 3]) * 0.5f;
        if (road_clear(mx, my) < 1.5f) return 0;
    }
    float r = 0.5f * sqrtf(q.w * q.w + q.d * q.d);   // vs other buildings: circles, a little generous
    int bx = (int)(q.x / 64.0f), by = (int)(q.y / 64.0f);
    for (int gy = by - 1; gy <= by + 1; gy++) for (int gx = bx - 1; gx <= bx + 1; gx++) {
        if (gx < 0 || gy < 0 || gx >= RSG || gy >= RSG) continue;
        for (int j = hs_head[gy * RSG + gx]; j >= 0; j = hs_next[j]) {
            float r2 = 0.5f * sqrtf(hs[j].w * hs[j].w + hs[j].d * hs[j].d);
            float dx = hs[j].x - q.x, dy = hs[j].y - q.y;
            if (dx * dx + dy * dy < (r + r2) * 0.85f * (r + r2) * 0.85f) return 0;
        }
    }
    hs[nhs] = q; hs_next[nhs] = hs_head[by * RSG + bx]; hs_head[by * RSG + bx] = nhs; nhs++;
    st_htype[q.t]++;
    return 1;
}
// ── services + landmarks (after the churches, before ordinary houses, so they get first pick) ──
// a building of type t facing the road at road point i (side sd), set back `back` m from the edge
static int hs_at_road(int i, int sd, int t, float w, float d, float back, int lv) {
    float dx = plx[i] - plx[i - 1], dy = ply[i] - ply[i - 1], l = sqrtf(dx * dx + dy * dy) + 1e-6f;
    House q = { 0 }; q.ux = dx / l; q.uy = dy / l; q.w = w; q.d = d; q.t = (unsigned char)t; q.h = (unsigned char)lv;
    float off = RHW[rs_cls[i]] + back + d * 0.5f;
    q.x = plx[i] - q.uy * off * sd; q.y = ply[i] + q.ux * off * sd;
    return try_house(q);
}
// road points (class >= cmin, never the expressway) within rad of (tx,ty), nearest to distance `want` first
static int hs_road_pts(float tx, float ty, float rad, int cmin, float want, int *out, int max) {
    static float key[8192]; int n = 0, r = (int)(rad / 64.0f) + 1, bx = (int)(tx / 64.0f), by = (int)(ty / 64.0f);
    if (max > 8192) max = 8192;
    for (int gy = by - r; gy <= by + r; gy++) for (int gx = bx - r; gx <= bx + r; gx++) {
        if (gx < 0 || gy < 0 || gx >= RSG || gy >= RSG) continue;
        for (int i = rs_head[gy * RSG + gx]; i >= 0 && n < max; i = rs_next[i]) {
            if (rs_cls[i] < cmin || rs_cls[i] == RC_EXPRESS) continue;
            float d = hypotf(plx[i] - tx, ply[i] - ty); if (d > rad) continue;
            out[n] = i; key[n] = fabsf(d - want); n++;
        }
    }
    for (int a = 1; a < n; a++) for (int b = a; b > 0 && key[b] < key[b - 1]; b--) {
        float t = key[b]; key[b] = key[b - 1]; key[b - 1] = t; int u = out[b]; out[b] = out[b - 1]; out[b - 1] = u; }
    return n;
}
static int hs_fuel_at(int i, int sd, unsigned *st) {     // canopy by the road, kiosk behind it
    const BkType *C = &BK[BK_FUEL_CANOPY], *K = &BK[BK_FUEL];
    float cw = C->w0 + hs_rand(st) * (C->w1 - C->w0), cd = C->d0 + hs_rand(st) * (C->d1 - C->d0);
    int n0 = nhs;
    if (!hs_at_road(i, sd, BK_FUEL_CANOPY, cw, cd, 3.0f, 1)) return 0;
    if (!hs_at_road(i, sd, BK_FUEL, K->w0 + hs_rand(st) * (K->w1 - K->w0), K->d0 + hs_rand(st) * (K->d1 - K->d0), 3.0f + cd + 7.0f, 1)) {   // 7 m forecourt behind the canopy (the overlap test is circles)
        nhs = n0; st_htype[BK_FUEL_CANOPY]--;            // no room for the kiosk: take the canopy back
        int bx = (int)(hs[n0].x / 64.0f), by = (int)(hs[n0].y / 64.0f); hs_head[by * RSG + bx] = hs_next[n0];
        return 0;
    }
    return 1;
}
// a free-standing landmark at the best of `ncand` scored cells, `spacing` m apart, up to `count`
static int hs_landmark(int t, const int *cand, int ncand, float spacing, int count) {
    int placed = 0; static float px[16], py[16];
    const BkType *B = &BK[t];
    hs_slope_max = 0.45f;
    for (int k = 0; k < ncand && placed < count; k++) {
        float x = (cand[k] % N + 0.5f) * CELL, y = (cand[k] / N + 0.5f) * CELL, ok = 1;
        for (int j = 0; j < placed; j++) if (hypotf(px[j] - x, py[j] - y) < spacing) ok = 0;
        if (!ok) continue;
        unsigned st = hsh((unsigned)cand[k] * 31u + (unsigned)t);
        for (int tr = 0; tr < 8; tr++) {
            float a = hs_rand(&st) * 6.2831853f, r = tr * 12.0f;
            House q = { 0 }; q.ux = cosf(a); q.uy = sinf(a);
            q.w = B->w0 + hs_rand(&st) * (B->w1 - B->w0); q.d = B->d0 + hs_rand(&st) * (B->d1 - B->d0);
            q.h = B->lv0; q.t = (unsigned char)t; q.x = x + cosf(a) * r; q.y = y + sinf(a) * r;
            if (try_house(q)) { px[placed] = x; py[placed] = y; placed++; break; }
        }
    }
    hs_slope_max = 0.2f;
    return placed;
}
static float hs_ring_h(int x, int y, float r) {          // mean height on a ring (for prominence)
    float s = 0; int n = 0;
    for (int k = 0; k < 8; k++) {
        int xx = x + (int)(cosf(k * 0.785398f) * r / CELL), yy = y + (int)(sinf(k * 0.785398f) * r / CELL);
        if (xx < 0 || yy < 0 || xx >= N || yy >= N) continue;
        s += fmaxf(H[yy * N + xx], 0); n++;
    }
    return n ? s / n : 0;
}
static float *hs_sort_key;
static int hs_cmp(const void *a, const void *b) { float x = hs_sort_key[*(const int *)a], y = hs_sort_key[*(const int *)b]; return x < y ? 1 : x > y ? -1 : 0; }
// LANDMARK SITES, chosen BEFORE the road network is final so each gets an ACCESS ROAD: a track to the
// nearest road (a lane up to the castle), routed like a farm's. The building goes on the site after
// the roads (place_services_landmarks), beside the track's end.
static int lm_site[BK_N][8], lm_n[BK_N];
static int lm_pick(int t, const int *cand, int ncand, float spacing, int count) {
    lm_n[t] = 0;
    for (int k = 0; k < ncand && lm_n[t] < count && lm_n[t] < 8; k++) {
        int c = cand[k], ok = slope_at(c % N, c / N) < 0.45f && is_dry(c) && !river_cell(c);
        for (int j = 0; j < lm_n[t] && ok; j++)
            if (hypotf((float)(lm_site[t][j] % N - c % N), (float)(lm_site[t][j] / N - c / N)) * CELL < spacing) ok = 0;
        if (ok) lm_site[t][lm_n[t]++] = c;
    }
    return lm_n[t];
}
static int st_lm_roads;
static void landmark_sites(void) {
    for (int t = 0; t < BK_N; t++) lm_n[t] = 0;
    static int cand[MAXN * 8]; static float sc[MAXN * MAXN]; int nc;
    // LIGHTHOUSES: land within 150 m of the sea, scored by how much sea surrounds it (a headland)
    nc = 0;
    for (int i = 0; i < N * N; i++) {
        sc[i] = 0;
        if (H[i] <= 0 || DS[i] > fmaxf(200.0f, CELL * 1.5f) || nc >= MAXN * 8) continue;   // on the shore (one cell is 156 m at 256)
        int x = i % N, y = i / N, sea = 0, tot = 0;
        for (int k = 0; k < 16; k++) for (int rr = 1; rr <= 3; rr++) {
            int xx = x + (int)(cosf(k * 0.3927f) * rr * 400.0f / CELL), yy = y + (int)(sinf(k * 0.3927f) * rr * 400.0f / CELL);
            tot++; if (xx < 0 || yy < 0 || xx >= N || yy >= N || H[yy * N + xx] <= 0) sea++;
        }
        sc[i] = (float)sea / tot + H[i] / 2000.0f;        // more sea around = more of a headland; a little height helps
        if (sc[i] > 0.55f) cand[nc++] = i;
    }
    hs_sort_key = sc; qsort(cand, nc, sizeof(int), hs_cmp);
    lm_pick(BK_LIGHTHOUSE, cand, nc, 8000.0f, 1 + (int)(st_land / 400.0f) > 3 ? 3 : 1 + (int)(st_land / 400.0f));
    // WINDMILLS: farmland (grass) hilltops that stand out from the land around them
    nc = 0;
    for (int i = 0; i < N * N; i++) {
        if (CV[i] != CV_GRASS || H[i] < 20 || H[i] > 300 || nc >= MAXN * 8) continue;
        float pr = H[i] - hs_ring_h(i % N, i / N, 700.0f);
        if (pr < 15.0f) continue;
        sc[i] = pr; cand[nc++] = i;
    }
    qsort(cand, nc, sizeof(int), hs_cmp);
    { int cnt = 2 + (int)(st_land / 250.0f); lm_pick(BK_WINDMILL, cand, nc, 5000.0f, cnt > 5 ? 5 : cnt); }
    // MAST: the highest peak
    nc = 0; { int best = -1; for (int i = 0; i < N * N; i++) if (H[i] > 0 && (best < 0 || H[i] > H[best])) best = i;
              if (best >= 0) { cand[nc++] = best; lm_pick(BK_MAST, cand, nc, 0, 1); } }
    // CASTLE: a hill above the biggest town, within 3 km, at least 40 m higher
    {
        int bt = -1; for (int s = 0; s < nset; s++) if (set[s].rank == ST_TOWN && (bt < 0 || set[s].size > set[bt].size)) bt = s;
        if (bt >= 0) {
            int tx = set[bt].x, ty = set[bt].y, r = (int)(3000.0f / CELL); float th = H[ty * N + tx];
            nc = 0;
            for (int y = ty - r; y <= ty + r; y++) for (int x = tx - r; x <= tx + r; x++) {
                if (x < 0 || y < 0 || x >= N || y >= N || nc >= MAXN * 8) continue;
                int i = y * N + x; float d = hypotf((float)(x - tx), (float)(y - ty)) * CELL;
                if (d > 3000.0f || d < set_r[bt] * 0.6f || H[i] < th + 40.0f) continue;
                sc[i] = (H[i] - hs_ring_h(x, y, 500.0f)) + (H[i] - th) * 0.3f - d / 100.0f; cand[nc++] = i;
            }
            qsort(cand, nc, sizeof(int), hs_cmp);
            lm_pick(BK_CASTLE, cand, nc, 0, 1);
        }
    }
    st_lm_roads = 0;
    for (int t = 0; t < BK_N; t++) for (int k = 0; k < lm_n[t]; k++) {
        int n; cn_away = -1;
        if (connect_to_network(lm_site[t][k], pbuf, &n, t == BK_MAST ? 60000 : 9000) >= 0) {   // a summit mast has a long service road
            mark(pbuf, n, t == BK_CASTLE ? RC_LANE : RC_TRACK, 0, 0, 1); st_lm_roads++;
        }
    }
}
static void place_services_landmarks(void) {
    static int pts[8192];
    // PUBS: every town (by the centre), ~80 % of villages
    for (int s = 0; s < nset; s++) {
        if (set[s].rank == ST_HAMLET) continue;
        unsigned st = hsh((unsigned)s * 977u + (unsigned)seed * 13u);
        if (set[s].rank == ST_VILLAGE && hs_rand(&st) > 0.8f) continue;
        float sx = (set[s].x + 0.5f) * CELL, sy = (set[s].y + 0.5f) * CELL;
        int n = hs_road_pts(sx, sy, set[s].rank == ST_TOWN ? 300 : 200, RC_LANE, 40, pts, 8192);
        const BkType *B = &BK[BK_PUB];
        for (int k = 0, done = 0; k < n && !done; k++)
            for (int sd = -1; sd <= 1 && !done; sd += 2)
                done = hs_at_road(pts[k], sd, BK_PUB, B->w0 + hs_rand(&st) * (B->w1 - B->w0), B->d0 + hs_rand(&st) * (B->d1 - B->d0), 4.0f, 2);
    }
    // FUEL: every town, on a secondary/main road near its edge; plus one per ~12 km of main road in open country
    for (int s = 0; s < nset; s++) {
        if (set[s].rank != ST_TOWN) continue;
        unsigned st = hsh((unsigned)s * 4241u + (unsigned)seed);
        float sx = (set[s].x + 0.5f) * CELL, sy = (set[s].y + 0.5f) * CELL;
        int n = hs_road_pts(sx, sy, set_r[s] * 1.3f, RC_SEC, set_r[s] * 0.85f, pts, 8192);
        for (int k = 0, done = 0; k < n && !done; k++) for (int sd = -1; sd <= 1 && !done; sd += 2) done = hs_fuel_at(pts[k], sd, &st);
    }
    {
        float acc = 6000.0f;                              // the first one ~6 km in
        for (int p = 0; p < npl; p++) {
            if (pl[p].cls < RC_MAIN || pl[p].cls == RC_EXPRESS) continue;
            unsigned st = hsh((unsigned)p * 7u + (unsigned)seed * 101u);
            for (int i = pl[p].start + 3; i < pl[p].start + pl[p].n - 3; i++) {
                acc += hypotf(plx[i] - plx[i - 1], ply[i] - ply[i - 1]);
                int c = (int)(ply[i] / CELL) * N + (int)(plx[i] / CELL);
                if (acc < 12000.0f || (c >= 0 && c < N * N && own[c] >= 0)) continue;
                if (hs_fuel_at(i, hs_rand(&st) < 0.5f ? -1 : 1, &st)) acc = 0;
            }
        }
    }
    // LANDMARKS at the sites landmark_sites() chose before the roads (each already has an access track)
    for (int t = 0; t < BK_N; t++) if (lm_n[t]) hs_landmark(t, lm_site[t], lm_n[t], 0, lm_n[t]);
}
static void build_houses(void) {
    nhs = 0; for (int k = 0; k < BK_N; k++) st_htype[k] = 0;
    for (int i = 0; i < RSG * RSG; i++) { rs_head[i] = -1; hs_head[i] = -1; }
    for (int p = 0; p < npl; p++) for (int i = pl[p].start + 1; i < pl[p].start + pl[p].n; i++) {
        rs_cls[i] = (unsigned char)pl[p].cls;
        int bx = (int)((plx[i] + plx[i - 1]) * 0.5f / 64.0f), by = (int)((ply[i] + ply[i - 1]) * 0.5f / 64.0f);
        if (bx < 0 || by < 0 || bx >= RSG || by >= RSG) continue;
        rs_next[i] = rs_head[by * RSG + bx]; rs_head[by * RSG + bx] = i;
    }
    for (int i = 0; i < N * N; i++) own[i] = -1;
    for (int s = 0; s < nset; s++) {
        int r = (int)(set_r[s] / CELL) + 1;
        for (int y = set[s].y - r; y <= set[s].y + r; y++) for (int x = set[s].x - r; x <= set[s].x + r; x++) {
            if (x < 0 || y < 0 || x >= N || y >= N) continue;
            if (hypotf((float)(x - set[s].x), (float)(y - set[s].y)) * CELL <= set_r[s]) own[y * N + x] = (short)s;
        }
    }
    // 1. a CHURCH per town and village: the first spot that fits along a road near the centre
    for (int s = 0; s < nset; s++) {
        if (set[s].rank == ST_HAMLET) continue;
        float sx = (set[s].x + 0.5f) * CELL, sy = (set[s].y + 0.5f) * CELL;
        static int ci[4096]; static float cd[4096]; int nc = 0;           // road points within 220 m, nearest first
        int bx = (int)(sx / 64.0f), by = (int)(sy / 64.0f);
        for (int gy = by - 4; gy <= by + 4; gy++) for (int gx = bx - 4; gx <= bx + 4; gx++) {
            if (gx < 0 || gy < 0 || gx >= RSG || gy >= RSG) continue;
            for (int i = rs_head[gy * RSG + gx]; i >= 0 && nc < 4096; i = rs_next[i]) {
                if (rs_cls[i] < RC_LANE || rs_cls[i] == RC_EXPRESS) continue;
                float d = hypotf(plx[i] - sx, ply[i] - sy); if (d > 220) continue;
                ci[nc] = i; cd[nc] = d; nc++;
            }
        }
        for (int a = 1; a < nc; a++) for (int b = a; b > 0 && cd[b] < cd[b - 1]; b--) {
            float t = cd[b]; cd[b] = cd[b - 1]; cd[b - 1] = t; int u = ci[b]; ci[b] = ci[b - 1]; ci[b - 1] = u; }
        int done = 0;
        for (int k = 0; k < nc && !done; k++) {
            int i = ci[k];
            float dx = plx[i] - plx[i - 1], dy = ply[i] - ply[i - 1], l = sqrtf(dx * dx + dy * dy) + 1e-6f;
            for (int sd = -1; sd <= 1 && !done; sd += 2) {
                House q = { 0 }; q.ux = dx / l; q.uy = dy / l; q.w = 12; q.d = 26;
                float off = RHW[rs_cls[i]] + 8 + q.d * 0.5f;
                q.x = plx[i] - q.uy * off * sd; q.y = ply[i] + q.ux * off * sd; q.h = 2; q.t = BK_CHURCH;
                done = try_house(q);
            }
        }
    }
    place_services_landmarks();
    // 2. houses along the roads inside each settlement
    for (int p = 0; p < npl; p++) {
        int cls = pl[p].cls; if (cls < RC_LANE || cls == RC_EXPRESS) continue;
        int s0 = pl[p].start, n = pl[p].n;
        static float cum[SM_MAX * 4]; if (n > SM_MAX * 4) continue;
        cum[0] = 0; for (int i = 1; i < n; i++) cum[i] = cum[i - 1] + hypotf(plx[s0 + i] - plx[s0 + i - 1], ply[s0 + i] - ply[s0 + i - 1]);
        float L = cum[n - 1];
        for (int sd = -1; sd <= 1; sd += 2) {
            unsigned st = hsh((unsigned)p * 7919u + (unsigned)(sd + 2) * 104729u + (unsigned)seed);
            float s = 12.0f + hs_rand(&st) * 8.0f; int i = 0;          // clear of the junction at each end
            while (s < L - 12.0f) {
                while (i < n - 2 && cum[i + 1] < s) i++;
                float t = (s - cum[i]) / fmaxf(cum[i + 1] - cum[i], 1e-3f);
                float px = plx[s0 + i] + (plx[s0 + i + 1] - plx[s0 + i]) * t, py = ply[s0 + i] + (ply[s0 + i + 1] - ply[s0 + i]) * t;
                float ux = plx[s0 + i + 1] - plx[s0 + i], uy = ply[s0 + i + 1] - ply[s0 + i], ul = sqrtf(ux * ux + uy * uy) + 1e-6f; ux /= ul; uy /= ul;
                int c = (int)(py / CELL) * N + (int)(px / CELL);
                int so = (c >= 0 && c < N * N) ? own[c] : -1;
                if (so < 0) { s += 15.0f; continue; }
                int rank = set[so].rank;
                float dc = hypotf(px - (set[so].x + 0.5f) * CELL, py - (set[so].y + 0.5f) * CELL) / fmaxf(set_r[so], 1);
                int core = rank == ST_TOWN && dc < 0.5f;
                float keep = rank == ST_TOWN ? (core ? 0.95f : 0.8f) : rank == ST_VILLAGE ? 0.7f : 0.45f;
                House q = { 0 }; q.ux = ux; q.uy = uy;
                if (core && hs_rand(&st) < 0.55f) { q.t = BK_TERRACE; q.w = 18 + hs_rand(&st) * 18; q.d = 9 + hs_rand(&st) * 3; q.h = 2 + (hs_rand(&st) < 0.4f); }
                else if (core && cls >= RC_SEC && hs_rand(&st) < 0.5f) { q.t = BK_RETAIL; q.w = 8 + hs_rand(&st) * 6; q.d = 10 + hs_rand(&st) * 4; q.h = 2 + (hs_rand(&st) < 0.5f); }
                else {
                    // a MIX of homes so a street isn't one house repeated: detached (some big), semi-
                    // detached pairs, bungalows; the town core stays 2-3-storey detached + terraces
                    float r = hs_rand(&st);
                    q.t = core ? BK_HOUSE
                        : rank == ST_TOWN    ? (r < 0.35f ? BK_SEMI : r < 0.50f ? BK_BUNGALOW : BK_HOUSE)
                        : rank == ST_VILLAGE ? (r < 0.20f ? BK_SEMI : r < 0.45f ? BK_BUNGALOW : BK_HOUSE)
                        :                      (r < 0.30f ? BK_BUNGALOW : BK_HOUSE);
                    const BkType *B = &BK[q.t];
                    q.w = B->w0 + hs_rand(&st) * (B->w1 - B->w0); q.d = B->d0 + hs_rand(&st) * (B->d1 - B->d0);
                    if (q.t == BK_HOUSE && !core && hs_rand(&st) < 0.15f) { q.w += 4; q.d += 2; }   // the odd big detached house
                    q.h = q.t == BK_HOUSE ? (rank == ST_TOWN ? 2 + (core && hs_rand(&st) < 0.3f) : 1 + (hs_rand(&st) < (rank == ST_VILLAGE ? 0.6f : 0.4f)))
                        : B->lv0 + (int)(hs_rand(&st) * (B->lv1 - B->lv0 + 0.999f));
                    if (!core) {                                  // not ruler-straight: +-4 degrees
                        float a = (hs_rand(&st) - 0.5f) * 0.14f, ca = cosf(a), sa = sinf(a);
                        float nx = q.ux * ca - q.uy * sa, ny = q.ux * sa + q.uy * ca; q.ux = nx; q.uy = ny;
                    }
                }
                float gap = q.t == BK_TERRACE ? 3 + hs_rand(&st) * 4
                          : rank == ST_TOWN ? (core ? 2 + hs_rand(&st) * 4 : 5 + hs_rand(&st) * 8)
                          : rank == ST_VILLAGE ? 6 + hs_rand(&st) * 14 : 14 + hs_rand(&st) * 30;
                float off = RHW[cls] + (core ? 3 + hs_rand(&st) * 2 : 4 + hs_rand(&st) * 4) + q.d * 0.5f;
                q.x = px - uy * off * sd + ux * q.w * 0.5f; q.y = py + ux * off * sd + uy * q.w * 0.5f;
                if (hs_rand(&st) < keep && try_house(q)) {
                    s += q.w + gap;
                    // a GARAGE beside or a SHED behind some homes outside the core
                    if (!core && q.t != BK_TERRACE && q.t != BK_RETAIL) {
                        float ax = -uy * sd, ay = ux * sd;          // away from the road
                        float g = hs_rand(&st);
                        if (g < 0.30f) {
                            const BkType *G = &BK[BK_GARAGE]; House gq = { 0 };
                            gq.t = BK_GARAGE; gq.h = 1; gq.ux = ux; gq.uy = uy;
                            gq.w = G->w0 + hs_rand(&st) * (G->w1 - G->w0); gq.d = G->d0 + hs_rand(&st) * (G->d1 - G->d0);
                            float along = q.w * 0.5f + 1.5f + gq.w * 0.5f, back = (gq.d - q.d) * 0.5f + 3.0f;
                            gq.x = q.x + ux * along + ax * back; gq.y = q.y + uy * along + ay * back;
                            if (try_house(gq)) s += gq.w + 1.5f;
                        } else if (g < 0.55f) {
                            const BkType *H2 = &BK[BK_SHED]; House sq = { 0 };
                            sq.t = BK_SHED; sq.h = 1; sq.ux = ux; sq.uy = uy;
                            sq.w = H2->w0 + hs_rand(&st) * (H2->w1 - H2->w0); sq.d = H2->d0 + hs_rand(&st) * (H2->d1 - H2->d0);
                            float back = q.d * 0.5f + 5.0f + sq.d * 0.5f, along = (hs_rand(&st) - 0.5f) * q.w * 0.6f;
                            sq.x = q.x + ux * along + ax * back; sq.y = q.y + uy * along + ay * back;
                            try_house(sq);
                        }
                    }
                }
                else s += 4.0f + (hs_rand(&st) < keep ? 0 : gap);
            }
        }
    }
    // 3. FARMS: a farmhouse on the farm, one or two barns 22-40 m off it
    for (int f = 0; f < nfarm; f++) {
        unsigned st = hsh((unsigned)farm[f] * 2654435761u ^ (unsigned)seed);
        float fx = (farm[f] % N + 0.5f) * CELL, fy = (farm[f] / N + 0.5f) * CELL, a = hs_rand(&st) * 6.2831853f;
        House q = { 0 }; q.ux = cosf(a); q.uy = sinf(a); q.w = 9 + hs_rand(&st) * 4; q.d = 8 + hs_rand(&st) * 3; q.h = 2; q.t = BK_FARM;
        int ok = 0;
        for (int k = 0; k < 6 && !ok; k++) { q.x = fx + (hs_rand(&st) - 0.5f) * 40; q.y = fy + (hs_rand(&st) - 0.5f) * 40; ok = try_house(q); }
        if (!ok) continue;
        int nb = 1 + (hs_rand(&st) < 0.5f);
        for (int b = 0; b < nb; b++) {
            House g = { 0 }; float ga = a + (hs_rand(&st) < 0.5f ? 0 : 1.5707963f);
            g.ux = cosf(ga); g.uy = sinf(ga); g.w = 18 + hs_rand(&st) * 14; g.d = 12 + hs_rand(&st) * 8; g.h = 1; g.t = BK_BARN;
            for (int k = 0; k < 6; k++) {
                float ba = hs_rand(&st) * 6.2831853f, bd = 22 + hs_rand(&st) * 18;
                g.x = q.x + cosf(ba) * bd; g.y = q.y + sinf(ba) * bd;
                if (try_house(g)) break;
            }
        }
    }
}

// ── ROUTE NUMBERS + SIGNPOSTS (roadmap item 1, for backroads' way-finding) ───────────────────────
// A ROAD GRAPH over the final lines (lanes and up; tracks left out): nodes = line ends, which meet
// exactly at junctions. ROUTES: at every node, same-class lines are paired straight-on (turn < 60°,
// straightest first) and chained; each chain is one route, numbered longest first: main = A, secondary
// = B, the expressway M1; lanes and slip roads unnumbered. Export: road sub token "ref=A3".
// SIGNPOSTS (kind "guidepost", one 20 m line per ARM from the junction along it): every junction a
// secondary-or-bigger road meets, plus town exits. Each arm carries "ref=A3;to=Holbeck:4.2,Selness:11.0":
// the next village and the next town that way (one entry if they're the same), km along the road.
#define RG_MAXN 32768
static int rg_nn, rg_head[RG_MAXN], rg_deg[RG_MAXN];
static float rg_x[RG_MAXN], rg_y[RG_MAXN];
static int pl_a[MAXPL], pl_b[MAXPL], pl_next_a[MAXPL], pl_next_b[MAXPL];   // end nodes + adjacency links
static float pl_len[MAXPL];
static unsigned char pl_in[MAXPL];                                         // in the graph
static char pl_ref[MAXPL][8];                                              // "A3", "" = unnumbered
static int rg_set[RG_MAXN];                                                // settlement at a node, -1
static int st_routes[3], st_guideposts;
#define RG_H 65536
static int rg_hash[RG_H];
static int rg_node(float x, float y) {
    int qx = (int)lroundf(x * 2), qy = (int)lroundf(y * 2);
    unsigned h = hsh((unsigned)qx * 73856093u ^ (unsigned)qy * 19349663u) & (RG_H - 1);
    for (;;) {
        int i = rg_hash[h];
        if (i < 0) break;
        if ((int)lroundf(rg_x[i] * 2) == qx && (int)lroundf(rg_y[i] * 2) == qy) return i;
        h = (h + 1) & (RG_H - 1);
    }
    if (rg_nn >= RG_MAXN) { st_overflow++; return -1; }
    rg_x[rg_nn] = x; rg_y[rg_nn] = y; rg_head[rg_nn] = -1; rg_deg[rg_nn] = 0; rg_set[rg_nn] = -1;
    rg_hash[h] = rg_nn; return rg_nn++;
}
// unit direction leaving node `end` (0 = start of the line, 1 = its end) along line p, `dist` metres in
static void pl_dir(int p, int end, float dist, float *dx, float *dy, float *px, float *py) {
    int s0 = pl[p].start, n = pl[p].n, i0 = end ? s0 + n - 1 : s0, st = end ? -1 : 1;
    float acc = 0; int i = i0;
    while (i + st >= s0 && i + st < s0 + n && acc < dist) { acc += hypotf(plx[i + st] - plx[i], ply[i + st] - ply[i]); i += st; }
    float ex = plx[i] - plx[i0], ey = ply[i] - ply[i0], l = sqrtf(ex * ex + ey * ey) + 1e-6f;
    *dx = ex / l; *dy = ey / l; if (px) { *px = plx[i]; *py = ply[i]; }
}
// place names in signs: never the separators
static void sign_name(char *out, int n, const char *name) {
    int k = 0; for (const char *c = name; *c && k < n - 1; c++) if (*c != ':' && *c != ',' && *c != ';') out[k++] = *c;
    out[k] = 0;
}
#define MAXGP 12000
typedef struct { float x0, y0, x1, y1; char sub[96]; } Guidepost;
static Guidepost gp[MAXGP]; static int ngp;
// DESTINATIONS FOLLOW THE ROUTE (a shortest-path search made every arm of a looped junction converge
// on the same nearest place: 32 posts said the same on every arm). From the junction, walk out along
// the arm and keep to the same road at every node: the same route number, else straight on onto the
// biggest class. The first place whose built-up area the walk enters is the first destination, the
// next town on the same walk the second; km along the walk. Never the place the sign stands in.
static int walk_arm(int from, int p, float d0, int here, int *v1, float *d1, int *t1, float *dt1) {
    *v1 = *t1 = -1; *d1 = *dt1 = 0;
    int cur = (pl_a[p] == from) ? pl_b[p] : pl_a[p], prev = p; float d = d0;
    static int seen_stamp[RG_MAXN], gen; gen++;
    seen_stamp[from] = gen;
    for (int steps = 0; steps < 4000 && d < 40000.0f; steps++) {
        if (seen_stamp[cur] == gen) break;               // looped back
        seen_stamp[cur] = gen;
        int c = (int)(rg_y[cur] / CELL) * N + (int)(rg_x[cur] / CELL);
        int so = (c >= 0 && c < N * N) ? own[c] : -1;
        if (so < 0) so = rg_set[cur];
        if (so >= 0 && so != here && set[so].rank >= ST_VILLAGE) {
            if (*v1 < 0) { *v1 = so; *d1 = d; }
            if (set[so].rank == ST_TOWN && so != *v1) { *t1 = so; *dt1 = d; break; }
            if (set[so].rank == ST_TOWN && *t1 < 0 && so == *v1) { /* first is a town: look on for another */ }
        }
        // which way on: same ref first, else the biggest class, straightest
        float ix, iy; pl_dir(prev, pl_b[prev] == cur, 30, &ix, &iy, 0, 0);   // pointing back along where we came from
        int best = -1; float bs = -1e9f;
        for (int e = rg_head[cur]; e >= 0; e = (e & 1) ? pl_next_b[e >> 1] : pl_next_a[e >> 1]) {
            int q = e >> 1; if (q == prev) continue;
            float ox, oy; pl_dir(q, e & 1, 30, &ox, &oy, 0, 0);
            float straight = -(ox * ix + oy * iy);          // 1 = straight on
            float sc = straight + pl[q].cls * 0.6f;
            if (pl_ref[prev][0] && !strcmp(pl_ref[q], pl_ref[prev])) sc += 10.0f;
            if (straight < -0.3f) sc -= 5.0f;               // never a hairpin back
            if (sc > bs) { bs = sc; best = q; }
        }
        if (best < 0) break;                              // dead end
        d += pl_len[best];
        cur = (pl_a[best] == cur) ? pl_b[best] : pl_a[best]; prev = best;
    }
    return *v1 >= 0;
}
typedef struct { float x0, y0, dx, dy; char ref[8]; int v, t; float dv, dt; } Arm;
static Arm arms[16]; static int narms;
static void arm_add(float x0, float y0, float dx, float dy, const char *ref, int from, int p, float d0, int here) {
    if (narms >= 16) return;
    Arm *a = &arms[narms];
    if (!walk_arm(from, p, d0, here, &a->v, &a->dv, &a->t, &a->dt)) return;
    a->x0 = x0; a->y0 = y0; a->dx = dx; a->dy = dy; memcpy(a->ref, ref, 8); narms++;
}
// one place named on several arms stays only on the arm where it's nearest; empty arms go
static void arms_dedup(void) {
    for (int i = 0; i < narms; i++) for (int j = 0; j < narms; j++) {
        if (i == j) continue;
        int *pi[2] = { &arms[i].v, &arms[i].t }; float *di[2] = { &arms[i].dv, &arms[i].dt };
        int *pj[2] = { &arms[j].v, &arms[j].t }; float *dj[2] = { &arms[j].dv, &arms[j].dt };
        for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++)
            if (*pi[a] >= 0 && *pi[a] == *pj[b] && (*di[a] > *dj[b] || (*di[a] == *dj[b] && i > j))) *pi[a] = -1;
    }
    int k = 0;
    for (int i = 0; i < narms; i++) {
        Arm *a = &arms[i];
        if (a->v < 0 && a->t >= 0) { a->v = a->t; a->dv = a->dt; a->t = -1; }
        if (a->t == a->v) a->t = -1;
        if (a->v >= 0) arms[k++] = *a;
    }
    narms = k;
}
static void arms_commit(void) {
    for (int i = 0; i < narms; i++) {
        if (ngp >= MAXGP) { st_overflow++; return; }
        Arm *a = &arms[i]; Guidepost *g = &gp[ngp++];
        g->x0 = a->x0; g->y0 = a->y0; g->x1 = a->x0 + a->dx * 20.0f; g->y1 = a->y0 + a->dy * 20.0f;
        int k = a->ref[0] ? snprintf(g->sub, sizeof g->sub, "ref=%s;to=", a->ref) : snprintf(g->sub, sizeof g->sub, "to=");
        char nm[24];
        sign_name(nm, sizeof nm, set_name[a->v]); k += snprintf(g->sub + k, sizeof g->sub - k, "%s:%.1f", nm, a->dv / 1000.0f);
        if (a->t >= 0) { sign_name(nm, sizeof nm, set_name[a->t]); snprintf(g->sub + k, sizeof g->sub - k, ",%s:%.1f", nm, a->dt / 1000.0f); }
    }
}
static void build_routes_signs(void) {
    rg_nn = 0; ngp = 0; st_routes[0] = st_routes[1] = st_routes[2] = 0;
    for (int i = 0; i < RG_H; i++) rg_hash[i] = -1;
    for (int p = 0; p < npl; p++) {
        pl_in[p] = pl[p].cls >= RC_LANE; pl_ref[p][0] = 0;
        if (!pl_in[p]) continue;
        int s0 = pl[p].start, e0 = s0 + pl[p].n - 1;
        pl_a[p] = rg_node(plx[s0], ply[s0]); pl_b[p] = rg_node(plx[e0], ply[e0]);
        if (pl_a[p] < 0 || pl_b[p] < 0 || pl_a[p] == pl_b[p]) { pl_in[p] = 0; continue; }
        float L = 0; for (int i = s0 + 1; i <= e0; i++) L += hypotf(plx[i] - plx[i - 1], ply[i] - ply[i - 1]);
        pl_len[p] = L;
        // adjacency: edge id e = p*2 + side (side 0: leaving from a, 1: leaving from b)
        pl_next_a[p] = rg_head[pl_a[p]]; rg_head[pl_a[p]] = p * 2;     rg_deg[pl_a[p]]++;
        pl_next_b[p] = rg_head[pl_b[p]]; rg_head[pl_b[p]] = p * 2 + 1; rg_deg[pl_b[p]]++;
    }
    for (int s = 0; s < nset; s++) {
        int v = rg_node((set[s].x + 0.5f) * CELL, (set[s].y + 0.5f) * CELL);
        if (v >= 0 && rg_head[v] >= 0) rg_set[v] = s;   // only if a road reaches the centre
    }
    // ROUTES: union lines paired straight-on at each node, per class
    static int uf[MAXPL]; static float clen[MAXPL]; static int order[MAXPL];
    for (int p = 0; p < npl; p++) uf[p] = p;
    #define UF(x) ({ int _r = (x); while (uf[_r] != _r) _r = uf[_r] = uf[uf[_r]]; _r; })
    for (int v = 0; v < rg_nn; v++) {
        int es[16], ne = 0;
        for (int e = rg_head[v]; e >= 0 && ne < 16; e = (e & 1) ? pl_next_b[e >> 1] : pl_next_a[e >> 1]) {
            int p = e >> 1; if (pl[p].cls >= RC_SEC && !pl_ramp[p]) es[ne++] = e;
        }
        static unsigned char used[16]; for (int i = 0; i < ne; i++) used[i] = 0;
        for (int round = 0; round < 8; round++) {          // straightest same-class pair first
            int bi = -1, bj = -1; float bd = -0.2f;         // straight on = a turn under ~78°
            for (int i = 0; i < ne; i++) for (int j = i + 1; j < ne; j++) {
                if (used[i] || used[j] || pl[es[i] >> 1].cls != pl[es[j] >> 1].cls) continue;
                float ax, ay, bx, by; pl_dir(es[i] >> 1, es[i] & 1, 40, &ax, &ay, 0, 0); pl_dir(es[j] >> 1, es[j] & 1, 40, &bx, &by, 0, 0);
                float dd = ax * bx + ay * by;            // -1 = straight on
                if (dd < bd) { bd = dd; bi = i; bj = j; }
            }
            if (bi < 0) break;
            used[bi] = used[bj] = 1;
            int ra = UF(es[bi] >> 1), rb = UF(es[bj] >> 1); if (ra != rb) uf[ra] = rb;
        }
    }
    for (int p = 0; p < npl; p++) clen[p] = 0;
    for (int p = 0; p < npl; p++) if (pl_in[p] && pl[p].cls >= RC_SEC && !pl_ramp[p]) clen[UF(p)] += pl_len[p];
    // fragments (< 2 km: a bend through a town centre, a class flip for a few hundred metres) join
    // the longest same-class route they touch, so an island has a dozen A roads, not a hundred
    for (int pass = 0; pass < 3; pass++)
        for (int p = 0; p < npl; p++) {
            if (!pl_in[p] || pl[p].cls < RC_SEC || pl_ramp[p]) continue;
            int r = UF(p); if (clen[r] >= 2000.0f) continue;
            int best = -1; float bl = 0;
            for (int side = 0; side < 2; side++) {
                int v = side ? pl_b[p] : pl_a[p];
                for (int e = rg_head[v]; e >= 0; e = (e & 1) ? pl_next_b[e >> 1] : pl_next_a[e >> 1]) {
                    int q = e >> 1; if (pl[q].cls != pl[p].cls || pl_ramp[q]) continue;
                    int rq = UF(q); if (rq != r && clen[rq] > bl) { bl = clen[rq]; best = rq; }
                }
            }
            if (best >= 0) { uf[r] = best; clen[best] += clen[r]; clen[r] = 0; }
        }
    int nr = 0; for (int p = 0; p < npl; p++) if (clen[p] >= 2000.0f) order[nr++] = p;   // only real routes get a number
    hs_sort_key = clen; qsort(order, nr, sizeof(int), hs_cmp);    // longest first
    static int num[MAXPL]; static float plen_route[MAXPL];
    for (int p = 0; p < npl; p++) num[p] = 0;
    int cnt[NRC] = { 0 };
    for (int k = 0; k < nr; k++) { int r = order[k], c = pl[r].cls; num[r] = ++cnt[c]; }
    for (int p = 0; p < npl; p++) {
        plen_route[p] = 0;
        if (!pl_in[p] || pl[p].cls < RC_SEC || pl_ramp[p]) continue;
        int r = UF(p), c = pl[p].cls;
        if (!num[r]) continue;
        plen_route[p] = clen[r];
        if (c == RC_EXPRESS) snprintf(pl_ref[p], sizeof pl_ref[p], "M%d", num[r]);
        else snprintf(pl_ref[p], sizeof pl_ref[p], "%c%d", c == RC_MAIN ? 'A' : 'B', num[r]);
    }
    // short unnumbered pieces of a numbered class carry on the number of the longest route they touch
    for (int pass = 0; pass < 4; pass++)
        for (int p = 0; p < npl; p++) {
            if (!pl_in[p] || pl[p].cls < RC_SEC || pl_ramp[p] || pl_ref[p][0]) continue;
            int best = -1; float bl = 0;
            for (int side = 0; side < 2; side++) {
                int v = side ? pl_b[p] : pl_a[p];
                for (int e = rg_head[v]; e >= 0; e = (e & 1) ? pl_next_b[e >> 1] : pl_next_a[e >> 1]) {
                    int q = e >> 1; if (q == p || !pl_ref[q][0] || pl_ref[q][0] == 'M') continue;
                    if (plen_route[q] > bl) { bl = plen_route[q]; best = q; }
                }
            }
            if (best >= 0) { memcpy(pl_ref[p], pl_ref[best], sizeof pl_ref[p]); plen_route[p] = plen_route[best]; }
        }
    st_routes[0] = cnt[RC_EXPRESS]; st_routes[1] = cnt[RC_MAIN]; st_routes[2] = cnt[RC_SEC];
    #undef UF
    // SIGNPOSTS at junctions a secondary-or-bigger road meets (not on the expressway itself)
    for (int v = 0; v < rg_nn; v++) {
        if (rg_deg[v] < 3) continue;
        int big = 0, expr = 0;
        for (int e = rg_head[v]; e >= 0; e = (e & 1) ? pl_next_b[e >> 1] : pl_next_a[e >> 1]) {
            int c = pl[e >> 1].cls; if (c >= RC_SEC) big = 1; if (c == RC_EXPRESS) expr = 1;
        }
        if (!big || expr) continue;
        // a post only where it tells you something: two different routes meet, or (after the
        // de-duplication) an unnumbered arm still names somewhere of its own
        int here = rg_set[v]; { int c = (int)(rg_y[v] / CELL) * N + (int)(rg_x[v] / CELL); if (c >= 0 && c < N * N && own[c] >= 0) here = own[c]; }
        narms = 0;
        for (int e = rg_head[v]; e >= 0; e = (e & 1) ? pl_next_b[e >> 1] : pl_next_a[e >> 1]) {
            int p = e >> 1; float dx, dy; pl_dir(p, e & 1, 20, &dx, &dy, 0, 0);
            arm_add(rg_x[v], rg_y[v], dx, dy, pl_ref[p], v, p, pl_len[p], here);
        }
        arms_dedup();
        int nref = 0, lane = 0; char refs[16][8];
        for (int i = 0; i < narms; i++) {
            if (!arms[i].ref[0]) { lane = 1; continue; }
            int seen = 0; for (int k = 0; k < nref; k++) if (!strcmp(refs[k], arms[i].ref)) seen = 1;
            if (!seen) strcpy(refs[nref++], arms[i].ref);
        }
        if (narms >= 2 && (nref >= 2 || lane)) arms_commit();
    }
    // TOWN EXITS: where a secondary-or-bigger road leaves a town's built-up radius, one arm outward
    for (int s = 0; s < nset; s++) {
        if (set[s].rank != ST_TOWN) continue;
        float cx = (set[s].x + 0.5f) * CELL, cy = (set[s].y + 0.5f) * CELL, R = set_r[s];
        for (int p = 0; p < npl; p++) {
            if (!pl_in[p] || pl[p].cls < RC_SEC || pl[p].cls == RC_EXPRESS || pl_ramp[p]) continue;
            int s0 = pl[p].start, n = pl[p].n; float acc = 0;
            for (int i = s0 + 1; i < s0 + n; i++) {
                float da = hypotf(plx[i - 1] - cx, ply[i - 1] - cy), db = hypotf(plx[i] - cx, ply[i] - cy);
                float seg = hypotf(plx[i] - plx[i - 1], ply[i] - ply[i - 1]); acc += seg;
                if ((da < R) == (db < R)) continue;
                int outward_forward = db >= R;            // leaving the town in the line's direction?
                float dx = plx[i] - plx[i - 1], dy = ply[i] - ply[i - 1], l = seg + 1e-6f; dx /= l; dy /= l;
                if (!outward_forward) { dx = -dx; dy = -dy; }
                int near = outward_forward ? pl_a[p] : pl_b[p];
                float rest = outward_forward ? pl_len[p] - acc : acc - seg;
                narms = 0;
                arm_add(plx[i], ply[i], dx, dy, pl_ref[p], near, p, rest, s);
                arms_commit();
            }
        }
    }
    st_guideposts = ngp;
}

// ── EXPORT (X): the island as roadview's vector features (the shared road model:
// data-tools/roadview/osm-roads.js's IR). Roads map onto the OSM ladder the real islands were
// measured with; bridge spans are split out with sub "B1"; rivers = canal, coastline = coast,
// settlements = residential areas, farms = buildings. Local metres, origin SW, Y north-up.
// Writes BOTH data/islandstudio-<seed>.json (readable IR, road-density.js) and the packed
// data/islandstudio-<seed>.rvb backroads/roadview/citydrive load, byte-identical to what
// `osm-roads.js --convert` makes from that JSON (no manual convert step). Features are recorded
// once (coords rounded exactly as the JSON prints them), then serialized twice.
static const char *EXPORT_KIND[NRC] = { "", "track", "road", "secondary", "arterial", "highway" };
static char export_msg[160]; static float export_msg_t;
// kind name -> .rvb index: MUST match KIND_IX in osm-roads.js (= the K_* enum in roadview.c)
static int ex_kind_ix(const char *k) {
    static const char *NM[] = { "highway", "arterial", "road", "track", "water", "canal", "building",
        "green", "tree", "residential", "commercial", "industrial", "farm", "parking", "sand", "rail", "coast",
        "secondary", "tertiary", "service", "cycleway", "footway", "other_area", "other_line",
        "crossing", "give_way", "stop", "traffic_signals", "calming", "guidepost" };
    for (int i = 0; i < (int)(sizeof NM / sizeof NM[0]); i++) if (!strcmp(NM[i], k)) return i;
    return 2;   // road (osm-roads.js's fallback)
}
typedef struct { const char *kind, *sub; int start, n; float h; } ExFeat;   // h: building height (m), 0 = none
static ExFeat *exf; static int exn, excap;
static double *expt; static int exnp, expcap;   // x,y pairs
static double ex_r(float v, int dec) { char b[32]; snprintf(b, sizeof b, dec ? "%.1f" : "%.0f", v); return strtod(b, NULL); }
static void ex_begin(const char *kind, const char *sub) {
    if (exn == excap) { excap = excap ? excap * 2 : 4096; exf = realloc(exf, excap * sizeof *exf); }
    exf[exn++] = (ExFeat){ kind, sub, exnp, 0, 0 };
}
static void ex_pt(double x, double y) {
    if (exnp + 2 > expcap) { expcap = expcap ? expcap * 2 : 1 << 16; expt = realloc(expt, expcap * sizeof *expt); }
    expt[exnp++] = x; expt[exnp++] = y; exf[exn - 1].n++;
}
static void ex_line(const char *kind, const char *sub, const float *xs, const float *ys, int a, int b) {
    if (b <= a) return;
    ex_begin(kind, sub);
    for (int i = a; i <= b; i++) ex_pt(ex_r(xs[i], 1), ex_r(WORLD_M - ys[i], 1));
}
static void ex_poly(const char *kind, const char *sub, float cx, float cy, float r, int n) {
    ex_begin(kind, sub);
    for (int k = 0; k <= n; k++) {
        float a = k * 6.2831853f / n;
        ex_pt(ex_r(cx + cosf(a) * r, 1), ex_r(WORLD_M - (cy + sinf(a) * r), 1));
    }
}
static void ex_w32(FILE *f, int v) { unsigned char b[4] = { v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >> 24) & 255 }; fwrite(b, 1, 4, f); }
static void ex_wf(FILE *f, double d) { float v = (float)d; unsigned u; memcpy(&u, &v, 4); ex_w32(f, (int)u); }
static float H_bilinear(float x, float y);
static void export_island(void) {
    exn = 0; exnp = 0;
    // LAND COVER as areas (first, so they lie under everything): runs of one class per row on a
    // 128 grid (312 m), each run one rectangle. A block whose sample point is water takes the block's
    // most common LAND class instead (a whole-water block is left out): water goes out separately,
    // finer, below, so a 312 m sample can't put a lake beside the bridge that crosses it.
    {
        const int R = 128; float cs = WORLD_M / R;
        static const char *KIND[NCV] = { "water", "water", "sand", "other_area", "farm", "other_area", "green", "other_area", "other_area" };
        static const char *SUB[NCV]  = { NULL, NULL, NULL, "natural=wetland", NULL, "natural=scrub", "natural=wood", "natural=heath", "natural=bare_rock" };
        static unsigned char blk[128 * 128];
        int B = N / R;                                        // cells per block side (2 / 4 / 8)
        for (int by = 0; by < R; by++) for (int bx = 0; bx < R; bx++) {
            int c = CV[(by * B) * N + bx * B];
            if (c == CV_SEA || c == CV_LAKE) {
                int cnt[NCV] = { 0 }, best = -1;
                for (int y = by * B; y < (by + 1) * B; y++) for (int x = bx * B; x < (bx + 1) * B; x++) cnt[CV[y * N + x]]++;
                for (int k = CV_BEACH; k < NCV; k++) if (cnt[k] && (best < 0 || cnt[k] > cnt[best])) best = k;
                c = best < 0 ? 255 : best;
            }
            blk[by * R + bx] = (unsigned char)c;
        }
        for (int ry = 0; ry < R; ry++) {
            int rx = 0;
            while (rx < R) {
                int c = blk[ry * R + rx], x0 = rx;
                while (rx < R && blk[ry * R + rx] == c) rx++;
                if (c == 255) continue;
                float X0 = x0 * cs, X1 = rx * cs, Y0 = WORLD_M - ry * cs, Y1 = WORLD_M - (ry + 1) * cs;
                float rx5[5] = { X0, X1, X1, X0, X0 }, ry5[5] = { Y1, Y1, Y0, Y0, Y1 };
                ex_begin(KIND[c], SUB[c]);
                for (int k = 0; k < 5; k++) ex_pt(ex_r(rx5[k], 0), ex_r(ry5[k], 0));
            }
        }
        // WATER (sea + lakes) on a finer grid: 512 (78 m), or the island's own grid if coarser
        const int RW = N < 512 ? N : 512; float ws = WORLD_M / RW; int S = N / RW;
        for (int ry = 0; ry < RW; ry++) {
            int rx = 0;
            while (rx < RW) {
                int c = CV[(ry * S) * N + rx * S], wet = c == CV_SEA || c == CV_LAKE, x0 = rx;
                while (rx < RW) { int c2 = CV[(ry * S) * N + rx * S]; if ((c2 == CV_SEA || c2 == CV_LAKE) != wet) break; rx++; }
                if (!wet) continue;
                float X0 = x0 * ws, X1 = rx * ws, Y0 = WORLD_M - ry * ws, Y1 = WORLD_M - (ry + 1) * ws;
                float rx5[5] = { X0, X1, X1, X0, X0 }, ry5[5] = { Y1, Y1, Y0, Y0, Y1 };
                ex_begin("water", NULL);
                for (int k = 0; k < 5; k++) ex_pt(ex_r(rx5[k], 1), ex_r(ry5[k], 1));
            }
        }
    }
    // roads, split at bridges
    for (int p = 0; p < npl; p++) {
        int s0 = pl[p].start, e0 = pl[p].start + pl[p].n - 1, cur = s0;
        // bridges on this poly, in order along it
        static int bi[64]; int nb = 0;
        for (int b = 0; b < nbr && nb < 64; b++) if (br[b].p == p) bi[nb++] = b;
        for (int a = 0; a < nb; a++) for (int c = a + 1; c < nb; c++) if (br[bi[c]].i < br[bi[a]].i) { int t = bi[a]; bi[a] = bi[c]; bi[c] = t; }
        // the route number rides on every piece: "ref=A3", and on a bridge "B1[;water=..];ref=A3"
        static char rsub[MAXPL][16];
        const char *rs = NULL;
        if (pl_ref[p][0]) { snprintf(rsub[p], sizeof rsub[p], "ref=%s", pl_ref[p]); rs = rsub[p]; }
        for (int a = 0; a < nb; a++) {
            Bridge *q = &br[bi[a]];
            int k0 = q->i - 1, k1 = q->i;                         // widen to cover the deck length
            while (k0 > cur && hypotf(plx[k0] - q->x, ply[k0] - q->y) < q->len * 0.5f) k0--;
            while (k1 < e0 && hypotf(plx[k1] - q->x, ply[k1] - q->y) < q->len * 0.5f) k1++;
            if (k0 < cur) continue;
            ex_line(EXPORT_KIND[pl[p].cls], rs, plx, ply, cur, k0);
            static char bsub[MAXBR][40];                      // water spans say what they cross: "B1;water=<level m>"
            int bl = snprintf(bsub[bi[a]], sizeof bsub[0], "B1");
            if (q->wl >= 0) bl += snprintf(bsub[bi[a]] + bl, sizeof bsub[0] - bl, ";water=%.1f", q->wl);
            if (rs) snprintf(bsub[bi[a]] + bl, sizeof bsub[0] - bl, ";%s", rs);
            ex_line(EXPORT_KIND[pl[p].cls], bsub[bi[a]], plx, ply, k0, k1);
            cur = k1;
        }
        ex_line(EXPORT_KIND[pl[p].cls], rs, plx, ply, cur, e0);
    }
    for (int r = 0; r < nrv; r++) ex_line("canal", NULL, rvx, rvy, rv[r].start, rv[r].start + rv[r].n - 1);
    // SIGNPOSTS: one "guidepost" line per arm, junction -> 20 m along the arm (see build_routes_signs)
    for (int g = 0; g < ngp; g++) {
        float xs[2] = { gp[g].x0, gp[g].x1 }, ys[2] = { gp[g].y0, gp[g].y1 };
        ex_line("guidepost", gp[g].sub, xs, ys, 0, 1);
    }
    // coastline: marching squares on H = 0, one short segment per crossing cell
    for (int y = 0; y + 1 < N; y++) for (int x = 0; x + 1 < N; x++) {
        float v[4] = { H[y * N + x], H[y * N + x + 1], H[(y + 1) * N + x + 1], H[(y + 1) * N + x] };
        float cx[4] = { 0, 1, 1, 0 }, cy[4] = { 0, 0, 1, 1 }, px[4], py[4]; int np = 0;
        for (int e = 0; e < 4; e++) {
            float a = v[e], b = v[(e + 1) & 3];
            if ((a > 0) == (b > 0)) continue;
            float t = a / (a - b);
            px[np] = (x + 0.5f + cx[e] + (cx[(e + 1) & 3] - cx[e]) * t) * CELL;
            py[np] = (y + 0.5f + cy[e] + (cy[(e + 1) & 3] - cy[e]) * t) * CELL; np++;
        }
        if (np >= 2) ex_line("coast", NULL, px, py, 0, 1);
        if (np == 4) ex_line("coast", NULL, px, py, 2, 3);
    }
    for (int s = 0; s < nset; s++)
        ex_poly("residential", set_sub[s], (set[s].x + 0.5f) * CELL, (set[s].y + 0.5f) * CELL,
                set[s].rank == ST_TOWN ? fmaxf(set_r[s] + 50.0f, 460.0f)
                : set[s].rank == ST_VILLAGE ? clamp(set_r[s] + 30.0f, 190.0f, 440.0f) : 120.0f, 16);
    // BUILDINGS, as osm-roads.js writes a real city: sub = OSM building= (+ ";key=value"), h = metres.
    // (The old unlabelled 12 m farm diamonds are gone: real OSM's untyped buildings also have an
    // empty sub, so "no sub = farm" couldn't last. Farmsteads are the "farm" footprints.)
    for (int k = 0; k < nhs; k++) {
        float cx[4], cy[4]; hs_corners(&hs[k], cx, cy);
        const char *sb = bk_sub(hs[k].t);
        ex_begin("building", *sb ? sb : NULL);
        exf[exn - 1].h = hs[k].h * 3.0f;
        for (int q = 0; q <= 4; q++) ex_pt(ex_r(cx[q & 3], 1), ex_r(WORLD_M - cy[q & 3], 1));
    }
    // HEIGHTFIELD (RVB3; backroads/citydrive drape the scene over it): G x G metres,
    // row i = south -> north, col j = west -> east, sea clamped to 0
    int G = N < 256 ? N : 256;
    double *hz = malloc((size_t)G * G * sizeof *hz);
    if (!hz) { snprintf(export_msg, sizeof export_msg, "export failed (memory)"); export_msg_t = 3; return; }
    for (int i = 0; i < G; i++) for (int j = 0; j < G; j++) {
        float wx = WORLD_M * j / (G - 1), wy = WORLD_M - WORLD_M * i / (G - 1);
        int cc = (int)(fminf(wy, WORLD_M - 1) / CELL) * N + (int)(fminf(wx, WORLD_M - 1) / CELL);
        hz[i * G + j] = ex_r(is_lake(cc) ? F[cc] : fmaxf(H_bilinear(wx, wy), 0), 1);   // a lake is its flat surface, not its bed
    }
    char name[48]; snprintf(name, sizeof name, "%s (seed %d)", island_name, seed);
    // 1. JSON (the readable IR)
    char path[128];
    snprintf(path, sizeof path, "../data/islandstudio-%d.json", seed);
    FILE *f = fopen(path, "w");
    if (!f) { snprintf(path, sizeof path, "islandstudio-%d.json", seed); f = fopen(path, "w"); }
    if (!f) { free(hz); snprintf(export_msg, sizeof export_msg, "export failed"); export_msg_t = 3; return; }
    fprintf(f, "{\"source\":\"islandstudio\",\"name\":\"%s\",\"land_km2\":%.1f,\"bbox\":[0,0,%.0f,%.0f],\"features\":[", name, st_land, WORLD_M, WORLD_M);
    for (int q = 0; q < exn; q++) {
        ExFeat *e = &exf[q];
        fprintf(f, "%s\n{\"kind\":\"%s\"", q ? "," : "", e->kind);
        if (e->sub) fprintf(f, ",\"sub\":\"%s\"", e->sub);
        if (e->h > 0) fprintf(f, ",\"h\":%g", e->h);
        if (e->sub && !strncmp(e->sub, "place=", 6)) fprintf(f, ",\"name\":\"%s\"", strstr(e->sub, "name=") + 5);
        fprintf(f, ",\"pts\":[");
        for (int k = 0; k < e->n * 2; k++) fprintf(f, "%s%.1f", k ? "," : "", expt[e->start + k]);
        fprintf(f, "]}");
    }
    fprintf(f, "\n],\n\"heightfield\":{\"g\":%d,\"z\":[", G);
    for (int k = 0; k < G * G; k++) fprintf(f, "%s%.1f", k ? "," : "", hz[k]);
    fprintf(f, "]}}\n");
    fclose(f);
    // 2. RVB3 (osm-roads.js writeBin layout, all LE): "RVB3" | nfeat | namelen | name | bbox[4] |
    //    per feature: kind | float h | sublen | sub | npts | pts[npts*2] | G | float grid[G*G]
    char rpath[128]; snprintf(rpath, sizeof rpath, "%.*s.rvb", (int)strlen(path) - 5, path);
    FILE *r = fopen(rpath, "wb");
    if (r) {
        fwrite("RVB3", 1, 4, r); ex_w32(r, exn); ex_w32(r, (int)strlen(name)); fwrite(name, 1, strlen(name), r);
        ex_wf(r, 0); ex_wf(r, 0); ex_wf(r, WORLD_M); ex_wf(r, WORLD_M);
        for (int q = 0; q < exn; q++) {
            ExFeat *e = &exf[q]; int sl = e->sub ? (int)strlen(e->sub) : 0;
            ex_w32(r, ex_kind_ix(e->kind)); ex_wf(r, e->h); ex_w32(r, sl); if (sl) fwrite(e->sub, 1, sl, r);
            ex_w32(r, e->n);
            for (int k = 0; k < e->n * 2; k++) ex_wf(r, expt[e->start + k]);
        }
        ex_w32(r, G); for (int k = 0; k < G * G; k++) ex_wf(r, hz[k]);
        fclose(r);
        snprintf(export_msg, sizeof export_msg, "exported %s (+ .rvb)", path);
    } else snprintf(export_msg, sizeof export_msg, "exported %s (.rvb failed)", path);
    free(hz);
    export_msg_t = 4;
}

static void build_roads(void) {
    clock_t t0 = clock();
    st_overflow = 0;
    for (int i = 0; i < N * N; i++) { TR[i] = 0; RING[i] = 0; IS_SET[i] = 0; JN[i] = 0; for (int k = 0; k < 8; k++) ER[i][k] = 0; }
    for (int s = 0; s < nset; s++) IS_SET[set[s].y * N + set[s].x] = 1;
    int n; float maxw = 1e-6f;
    // 1. candidate links: the GABRIEL graph (two places link if no third sits in the circle
    //    between them), capped in length, plus every town pair (the trunks)
    int nl = 0;
    float sp = (4.5f - k_vill * 3.0f) * 1000.0f, maxd = fmaxf(3.4f * sp, 7000.0f);
    for (int a = 0; a < nset; a++) for (int b = a + 1; b < nset && nl < 4096; b++) {
        float d = set_dist(a, b);
        int towns = set[a].rank == ST_TOWN && set[b].rank == ST_TOWN;
        if (!towns) {
            if (d > maxd) continue;
            float mx = (set[a].x + set[b].x) * 0.5f, my = (set[a].y + set[b].y) * 0.5f, r2 = (d / CELL) * (d / CELL) * 0.25f;
            int ok = 1;
            for (int c = 0; c < nset && ok; c++) {
                if (c == a || c == b) continue;
                float dx = set[c].x - mx, dy = set[c].y - my; if (dx * dx + dy * dy < r2) ok = 0;
            }
            if (!ok) continue;
        }
        link_a[nl] = a; link_b[nl] = b; link_w[nl] = gravity(a, b) * (towns ? 4.0f : 1.0f);
        if (link_w[nl] > maxw) maxw = link_w[nl];
        nl++;
    }
    // the VILLAGE NETWORK: the same Gabriel rule over villages + towns only, as if the hamlets
    // weren't there. A road between two villages stays a B-road when it passes a hamlet; without
    // this every such road was split into two lane-class halves (main density ~0.3 vs 0.7-0.9).
    // These routes mostly follow existing roads (cheaper) and raise them to secondary.
    int nbase = nl;
    for (int a = 0; a < nset; a++) for (int b = a + 1; b < nset && nl < 4096; b++) {
        if (set[a].rank < ST_VILLAGE || set[b].rank < ST_VILLAGE) continue;
        if (set[a].rank == ST_TOWN && set[b].rank == ST_TOWN) continue;   // already linked
        float d = set_dist(a, b); if (d > maxd * 1.8f) continue;
        float mx = (set[a].x + set[b].x) * 0.5f, my = (set[a].y + set[b].y) * 0.5f, r2 = (d / CELL) * (d / CELL) * 0.25f;
        int ok = 1;
        for (int c = 0; c < nset && ok; c++) {
            if (c == a || c == b || set[c].rank < ST_VILLAGE) continue;
            float dx = set[c].x - mx, dy = set[c].y - my; if (dx * dx + dy * dy < r2) ok = 0;
        }
        for (int q = 0; q < nbase && ok; q++) if ((link_a[q] == a && link_b[q] == b) || (link_a[q] == b && link_b[q] == a)) ok = 0;
        if (!ok) continue;
        link_a[nl] = a; link_b[nl] = b; link_w[nl] = gravity(a, b);
        if (link_w[nl] > maxw) maxw = link_w[nl];
        nl++;
    }
    for (int i = 0; i < nl; i++) { link_cmp_idx[i] = i; link_w[i] /= maxw; }
    qsort(link_cmp_idx, nl, sizeof(int), link_cmp);

    // 2. the RING: coastal places in angular order around the island's centre, consecutive
    //    pairs joined with a coast-hugging cost. A leg that would cost too much is left out:
    //    the ring breaks there (cliffs or a mountain to the sea) and the island has a far side.
    static int cs[MAXSET]; static float ca[MAXSET]; int nc = 0;
    float cx = 0, cy = 0, cn = 0;
    for (int i = 0; i < N * N; i++) if (H[i] > 0) { cx += i % N; cy += i / N; cn++; }
    if (cn > 0) { cx /= cn; cy /= cn; }
    for (int s = 0; s < nset; s++) if (DS[set[s].y * N + set[s].x] < 1800) {
        cs[nc] = s; ca[nc] = atan2f(set[s].y - cy, set[s].x - cx); nc++;
    }
    for (int i = 0; i < nc; i++) for (int j = i + 1; j < nc; j++)
        if (ca[j] < ca[i]) { float t = ca[i]; ca[i] = ca[j]; ca[j] = t; int u = cs[i]; cs[i] = cs[j]; cs[j] = u; }
    st_ringgaps = 0;
    for (int i = 0; i < nc && nc >= 3; i++) {
        int a = cs[i], b = cs[(i + 1) % nc];
        float d = set_dist(a, b);
        float cst = astar(set[a].y * N + set[a].x, set[b].y * N + set[b].x, RM_RING, pbuf, &n, d * 4.0f + 3000);
        if (cst < 0) { st_ringgaps++; continue; }
        mark(pbuf, n, RC_LANE, 0.25f, 1, 0);
    }
    // 3. links, most important first (so the rest can merge onto them)
    for (int q = 0; q < 3; q++) st_link_try[q] = st_link_ok[q] = 0;
    for (int j = 0; j < nl; j++) {
        int i = link_cmp_idx[j], a = link_a[i], b = link_b[i];
        int ra = set[a].rank, rb = set[b].rank;          // a floor from what it joins; traffic can raise it
        // town-town = main; anything touching a village or town = secondary (B/C roads, OSM tertiary+,
        // which the real islands' 'main' figure includes); only hamlet-to-hamlet is a lane
        int floor = (ra == ST_TOWN && rb == ST_TOWN) ? RC_MAIN : (ra >= ST_VILLAGE || rb >= ST_VILLAGE) ? RC_SEC : RC_LANE;
        st_link_try[floor - RC_LANE]++;
        float d = set_dist(a, b);
        float cst = astar_b(set[a].y * N + set[a].x, set[b].y * N + set[b].x, RM_LINK, pbuf, &n, d * 3.5f + 2500);
        if (cst < 0) continue;
        st_link_ok[floor - RC_LANE]++;
        mark(pbuf, n, floor, link_w[i], 0, 0);
    }
    // 4. classes from traffic: quantiles over road cells; the ring is at least a main road
    static float tv[MAXN * MAXN]; int nt = 0;
    for (int i = 0; i < N * N; i++) if (TR[i] > 0) tv[nt++] = TR[i];
    qsort(tv, nt, sizeof(float), fcmp_up);
    float t_main = nt ? tv[(int)(nt * 0.78f)] : 1, t_sec = nt ? tv[(int)(nt * 0.35f)] : 1;
    for (int c = 0; c < N * N; c++) for (int k = 0; k < 8; k++) {
        if (ER[c][k] < RC_LANE || ER[c][k] > RC_MAIN) continue;
        int nx = c % N + DX[k], ny = c / N + DY[k], o = ny * N + nx;
        float t = fminf(TR[c], TR[o]);
        int cls = t >= t_main ? RC_MAIN : t >= t_sec ? RC_SEC : RC_LANE;
        if (RING[c] && RING[o] && cls < RC_MAIN) cls = RC_MAIN;
        if (cls > ER[c][k]) ER[c][k] = (unsigned char)cls;
    }
    // 5. the EXPRESSWAY, if it makes sense: the two biggest towns, more than 10 km apart.
    //    Gentle grades, big bridges allowed, no merging onto old roads (a new alignment).
    st_express = 0;
    int t1 = -1, t2 = -1;
    for (int s = 0; s < nset; s++) if (set[s].rank == ST_TOWN) {
        if (t1 < 0 || set[s].size > set[t1].size) { t2 = t1; t1 = s; }
        else if (t2 < 0 || set[s].size > set[t2].size) t2 = s;
    }
    if (t1 >= 0 && t2 >= 0 && set_dist(t1, t2) > 10000) {
        float d = set_dist(t1, t2);
        if (astar_b(set[t1].y * N + set[t1].x, set[t2].y * N + set[t2].x, RM_EXPRESS, pbuf, &n, d * 3.0f) >= 0) {
            mark(pbuf, n, RC_EXPRESS, 0, 0, 0); st_express = 1;
        }
    }
    // 6. TRACKS: dead-end spurs from villages and hamlets out into their best farmland
    for (int s = 0; s < nset; s++) {
        if (set[s].rank == ST_TOWN) continue;
        int nsp = set[s].rank == ST_VILLAGE ? 3 : 2;
        for (int q = 0; q < nsp; q++) {
            float ang = (q + 0.37f * s) * 6.2831853f / nsp;
            int best = -1; float bs = 0;
            for (float r = 600; r <= 1700; r += CELL) {
                int x = set[s].x + (int)(cosf(ang) * r / CELL), y = set[s].y + (int)(sinf(ang) * r / CELL);
                if (x < 0 || y < 0 || x >= N || y >= N) break;
                int i = y * N + x; if (H[i] <= 0) break;
                if (TR[i] > 0 || river_cell(i)) continue;
                if (SU[i] > bs) { bs = SU[i]; best = i; }
            }
            if (best < 0 || bs < 0.15f) continue;
            if (astar(set[s].y * N + set[s].x, best, RM_TRACK, pbuf, &n, 2600 * 2.5f) >= 0)
                mark(pbuf, n, RC_TRACK, 0, 0, 1);
        }
    }
    settle_radii();
    town_streets();
    farms();
    upland_tracks();
    landmark_sites();
    fix_diagonal_crossings();
    cut_corners();
    consolidate_junctions();
    cut_corners();
    build_polys();
    build_rivers();
    build_bridges();
    build_water_bridges();
    build_flyovers();
    build_interchanges();
    build_houses();
    build_routes_signs();
    roads_dirty = 0;
    st_roads_ms = (clock() - t0) * 1000.0f / CLOCKS_PER_SEC;
}

// ── colour ─────────────────────────────────────────────────────────────────────
static int shade3(const int *r, float s) { return s < -0.04f ? r[0] : s > 0.04f ? r[2] : r[1]; }
static void recolour(void) {
    static const int SEA_D[1] = { CLR_DARKER_BLUE };
    static const int LOW[3]  = { CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_GREEN };
    static const int MID[3]  = { CLR_BLUE_GREEN, CLR_DARK_GREEN, CLR_MEDIUM_GREEN };
    static const int UP[3]   = { CLR_DARK_BROWN, CLR_BROWN, CLR_MEDIUM_GREY };
    static const int HIGH[3] = { CLR_DARK_GREY, CLR_MEDIUM_GREY, CLR_LIGHT_GREY };
    static const int TOP[3]  = { CLR_MEDIUM_GREY, CLR_LIGHT_GREY, CLR_WHITE };
    float thr = river_thr();
    (void)SEA_D;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int i = y * N + x; float h = H[i]; int c;
        if (h <= 0) { c = h < -40 ? CLR_DARKER_BLUE : h < -12 ? CLR_DARK_BLUE : CLR_TRUE_BLUE; col[i] = c; continue; }
        if (F[i] - h > LAKE_DEPTH) { col[i] = CLR_BLUE; continue; }                    // lake
        int xm = x > 0 ? x - 1 : x, ym = y > 0 ? y - 1 : y;
        float shd = (H[i] - H[ym * N + xm]) / CELL;                              // NW-lit hillshade
        if (view == V_COVER) {
            static const int CC[NCV][3] = {
                { 0, 0, 0 }, { 0, 0, 0 },
                { CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW },                 // beach
                { CLR_BLUE_GREEN, CLR_BLUE_GREEN, CLR_LIME_GREEN },                        // marsh
                { CLR_MEDIUM_GREEN, CLR_GREEN, CLR_LIME_GREEN },                          // grass
                { CLR_DARK_BROWN, CLR_BROWN, CLR_PEACH },                                 // scrub
                { CLR_BLUE_GREEN, CLR_DARK_GREEN, CLR_MEDIUM_GREEN },                     // forest
                { CLR_DARKER_PURPLE, CLR_MAUVE, CLR_INDIGO },                             // moor
                { CLR_DARK_GREY, CLR_MEDIUM_GREY, CLR_LIGHT_GREY },                       // rock
            };
            c = shade3(CC[CV[i]], shd);
        } else if (view == V_SUIT) {
            float u = SU[i];
            c = u <= 0 ? CLR_DARKER_GREY : u < 0.15f ? CLR_DARKER_PURPLE : u < 0.3f ? CLR_DARK_PURPLE
              : u < 0.45f ? CLR_DARK_RED : u < 0.6f ? CLR_DARK_ORANGE : u < 0.75f ? CLR_ORANGE : CLR_YELLOW;
        } else if (view == V_SLOPE) {
            float s = slope_at(x, y);
            c = s < 0.03f ? CLR_LIME_GREEN : s < 0.08f ? CLR_MEDIUM_GREEN : s < 0.18f ? CLR_DARK_GREEN
              : s < 0.35f ? CLR_BROWN : CLR_DARK_RED;
        } else if (view == V_FLOW) {
            float a = log2f(A[i] / AREA_K);
            c = a < 2 ? CLR_BROWNISH_BLACK : a < 4 ? CLR_DARKER_BLUE : a < 6 ? CLR_DARK_BLUE
              : a < 8 ? CLR_TRUE_BLUE : a < 10 ? CLR_BLUE : CLR_WHITE;
        } else {                                                                  // terrain
            float s = shd;
            c = h < 3 ? CLR_LIGHT_YELLOW
              : h < 90 ? shade3(LOW, s) : h < 300 ? shade3(MID, s) : h < 650 ? shade3(UP, s)
              : h < 1000 ? shade3(HIGH, s) : shade3(TOP, s);
        }
        if (view != V_FLOW && A[i] >= thr) c = CLR_BLUE;                               // river
        col[i] = c;
    }
}

static float st_rebuild_ms;
static void terrain_pass(void) {
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)
        H[y * N + x] = height_at((x + 0.5f) * CELL, (y + 0.5f) * CELL);
    drain();
    if (k_ero > 0.01f) { carve_lines(); drain(); breach(); drain(); }
    // stats
    float km2 = CELL * CELL / 1e6f, thr = river_thr();
    int land = 0, flat = 0, high = 0, riv = 0, lakes = 0; st_peak = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int i = y * N + x; if (H[i] <= 0) continue;
        land++; if (H[i] > st_peak) st_peak = H[i];
        if (H[i] > 300) high++;
        if (slope_at(x, y) < 0.05f && H[i] < 150) flat++;
        if (A[i] >= thr) riv++;
        if (F[i] - H[i] > LAKE_DEPTH) lakes++;
    }
    st_land = land * km2; st_flat = land ? 100.0f * flat / land : 0; st_high = land ? 100.0f * high / land : 0;
    st_river = riv * CELL / 1000.0f; st_lakes = lakes;
    climate();
    dist_from(DW, is_fresh);
    dist_from(DS, is_sea);
    dist_from(DL, is_dry);
    cover();
    for (int k = 0; k < NCV; k++) st_cov[k] = 0;
    for (int i = 0; i < N * N; i++) if (H[i] > 0) st_cov[CV[i]] += 1;
    for (int k = 0; k < NCV; k++) st_cov[k] = land ? 100.0f * st_cov[k] / land : 0;
}
static void rebuild(void) {
    clock_t t0 = clock();
    if (N != 256) {                                   // the plan: places chosen on the 256 grid
        int keep = N; N = 256;
        terrain_pass(); plan_mode = 0; settle(); name_places();   // names too: the same at every grid
        nplan = nset;
        for (int k = 0; k < nset; k++) {
            plan_x[k] = (set[k].x + 0.5f) * CELL; plan_y[k] = (set[k].y + 0.5f) * CELL;
            plan_rank[k] = set[k].rank; plan_size[k] = set[k].size;
        }
        N = keep;
    }
    terrain_pass();
    plan_mode = N != 256; settle();
    if (plan_mode) {                                  // carry the plan's names (island_name is still set)
        static char pn[MAXSET][24], ps[MAXSET][48];
        for (int k = 0; k < nplan; k++) { memcpy(pn[k], set_name[k], 24); memcpy(ps[k], set_sub[k], 48); }
        for (int k = 0; k < nset; k++) { memcpy(set_name[k], pn[set_plan[k]], 24); memcpy(set_sub[k], ps[set_plan[k]], 48); }
    } else name_places();
    plan_mode = 0;
    st_rank[0] = st_rank[1] = st_rank[2] = 0; st_coastal = 0;
    for (int k = 0; k < nset; k++) if (DS[set[k].y * N + set[k].x] < 1000) st_coastal++;
    st_coastal = nset ? 100 * st_coastal / nset : 0;
    for (int k = 0; k < nset; k++) st_rank[set[k].rank]++;
    recolour();
    dirty = 0; roads_dirty = 1; zdirty = 1;
    st_rebuild_ms = (clock() - t0) * 1000.0f / CLOCKS_PER_SEC;
}

void init(void) { rebuild(); build_roads(); }

void update(void) {
#ifdef DE_TRACE
    watch("seed", "%d", seed); watch("nset", "%d", nset); watch("coastal", "%d", st_coastal);
    watch("moor", "%d", (int)st_cov[CV_MOOR]); watch("forest", "%d", (int)st_cov[CV_FOREST]);
    watch("land", "%d", (int)st_land); watch("npl", "%d", npl); watch("express", "%d", st_express);
    watch("ringgaps", "%d", st_ringgaps); watch("bridges", "%d", nbr); watch("lakes", "%d", st_lakes); watch("N", "%d", N); watch("diagx", "%d", st_diagx); watch("cuts", "%d", st_cuts); watch("consol", "%d", st_consol); watch("streets", "%d", st_streets); watch("routes", "M %d A %d B %d", st_routes[0], st_routes[1], st_routes[2]); watch("guideposts", "%d", st_guideposts); watch("lm_roads", "%d", st_lm_roads); watch("houses", "%d", nhs); for (int t = 0; t < BK_N; t++) if (st_htype[t]) watch(str("b_%s%s%s", BK[t].building, BK[t].tag ? "_" : "", BK[t].tag ? strchr(BK[t].tag, '=') + 1 : ""), "%d", st_htype[t]); watch("waterbr", "%d", st_waterbr); watch("waterbr_max", "%.0f", st_waterbr_max); watch("flyovers", "%d", st_flyovers); watch("interchanges", "%d", st_interchanges); watch("rebuild_ms", "%.0f", st_rebuild_ms); watch("overflow", "%d", st_overflow); watch("links", "lane %d/%d sec %d/%d main %d/%d", st_link_ok[0], st_link_try[0], st_link_ok[1], st_link_try[1], st_link_ok[2], st_link_try[2]); { int bb = -1; for (int b = 0; b < nbr; b++) if (bb < 0 || br[b].len > br[bb].len) bb = b;
      if (bb >= 0) { watch("bigbridge_x", "%.0f", br[bb].x); watch("bigbridge_y", "%.0f", br[bb].y); watch("bigbridge_len", "%.0f", br[bb].len); } } watch("rivers", "%d", nrv);
    watch("forest_tracks", "%d", nforest); watch("upland_tracks", "%d", nupland); watch("roads_ms", "%.0f", st_roads_ms);
    for (int k = RC_TRACK; k < NRC; k++) watch(RCN[k], "%.0f", st_road[k]);
#endif
    if (keyp(KEY_TAB)) {
        zoom_on = !zoom_on;
        int mx = mouse_x() - MAP_X, my = mouse_y() - MAP_Y;
        if (zoom_on && mx >= 0 && my >= 0 && mx < MAP_PX && my < MAP_PX) { zx = (mx + 0.5f) * MAPCELL; zy = (my + 0.5f) * MAPCELL; zdirty = 1; }
    }
    if (zoom_on) { zoom_input(); return; }
    if (keyp('X') && !roads_dirty) export_island();
    if (export_msg_t > 0) export_msg_t -= dt();
    if (keyp('R')) { seed++; dirty = 1; }
    if (keyp('G')) { N = N == 256 ? 512 : N == 512 ? 1024 : 256; dirty = 1; }
    if (keyp('V')) { view = (view + 1) % NVIEW; recolour(); }
}

// ── roads on the overview: smoothed polylines, minor first ────────────────────
static void draw_roads_overview(void) {
    if (roads_dirty) return;
    for (int cls = RC_TRACK; cls < NRC; cls++)
        for (int p = 0; p < npl; p++) {
            if (pl[p].cls != cls) continue;
            for (int i = pl[p].start + 1; i < pl[p].start + pl[p].n; i++)
                line(MAP_X + (int)(plx[i - 1] / MAPCELL), MAP_Y + (int)(ply[i - 1] / MAPCELL),
                     MAP_X + (int)(plx[i] / MAPCELL), MAP_Y + (int)(ply[i] / MAPCELL), RCOL[cls]);
        }
}

// ── ZOOM (TAB): full screen, a few metres per pixel. Terrain is resampled finely (bilinear
// heights + a little fine noise, cover boundaries jittered so cells don't show), rivers and
// roads are drawn at their real widths. Drag pans, wheel zooms. ─────────────────────────────
static unsigned char zcol[ZW * ZH];
static float zh[ZW * ZH];

// bicubic (Catmull-Rom) height between grid points: bilinear made flat triangular facets that
// the hillshade outlined, a visible diagonal grid at zoom
static float Hg(int x, int y) { x = x < 0 ? 0 : x >= N ? N - 1 : x; y = y < 0 ? 0 : y >= N ? N - 1 : y; return H[y * N + x]; }
static float cr1(float a, float b, float c, float d, float t) {
    return 0.5f * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (-a + 3 * b - 3 * c + d) * t * t * t);
}
static float H_bilinear(float x, float y) {
    float fx = x / CELL - 0.5f, fy = y / CELL - 0.5f;
    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    float tx = fx - x0, ty = fy - y0, r[4];
    for (int j = 0; j < 4; j++)
        r[j] = cr1(Hg(x0 - 1, y0 - 1 + j), Hg(x0, y0 - 1 + j), Hg(x0 + 1, y0 - 1 + j), Hg(x0 + 2, y0 - 1 + j), tx);
    return cr1(r[0], r[1], r[2], r[3], ty);
}
static void zoom_render(void) {
    static const int CC[NCV][3] = {
        { 0, 0, 0 }, { 0, 0, 0 },
        { CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW },
        { CLR_BLUE_GREEN, CLR_BLUE_GREEN, CLR_LIME_GREEN },
        { CLR_MEDIUM_GREEN, CLR_GREEN, CLR_LIME_GREEN },
        { CLR_DARK_BROWN, CLR_BROWN, CLR_PEACH },
        { CLR_BLUE_GREEN, CLR_DARK_GREEN, CLR_MEDIUM_GREEN },
        { CLR_DARKER_PURPLE, CLR_MAUVE, CLR_INDIGO },
        { CLR_DARK_GREY, CLR_MEDIUM_GREY, CLR_LIGHT_GREY },
    };
    for (int py = 0; py < ZH; py++) for (int px = 0; px < ZW; px++) {
        float x = zx + (px - ZW / 2) * zm, y = zy + (py - ZH / 2) * zm;
        float h = H_bilinear(x, y);
        if (h > 0) h += (nz(x / 140.0f, y / 140.0f, 61.0f) - 0.5f) * 8.0f;
        zh[py * ZW + px] = h;
    }
    for (int py = 0; py < ZH; py++) for (int px = 0; px < ZW; px++) {
        float x = zx + (px - ZW / 2) * zm, y = zy + (py - ZH / 2) * zm, h = zh[py * ZW + px];
        int c;
        if (h <= 0) c = h < -40 ? CLR_DARKER_BLUE : h < -12 ? CLR_DARK_BLUE : CLR_TRUE_BLUE;
        else {
            // soften cell edges by a FIXED distance (it was ~1 cell: fine at 156 m, stair-steps at 39 m)
            float jx = x + (nz(x / 90.0f, y / 90.0f, 71.0f) - 0.5f) * 140.0f + (nz(x / 25.0f, y / 25.0f, 75.0f) - 0.5f) * 30.0f;
            float jy = y + (nz(x / 90.0f, y / 90.0f, 73.0f) - 0.5f) * 140.0f + (nz(x / 25.0f, y / 25.0f, 77.0f) - 0.5f) * 30.0f;
            int cxi = (int)(jx / CELL), cyi = (int)(jy / CELL);
            cxi = cxi < 0 ? 0 : cxi >= N ? N - 1 : cxi; cyi = cyi < 0 ? 0 : cyi >= N ? N - 1 : cyi;
            int cv = CV[cyi * N + cxi];
            if (cv == CV_SEA) cv = CV_BEACH;
            if (cv == CV_LAKE) c = CLR_BLUE;
            else {
                float hl = (px > 0 && py > 0) ? zh[(py - 1) * ZW + px - 1] : h;
                float s = (h - hl) / (zm * 1.414f);
                c = shade3(CC[cv], s * 1.5f);
            }
        }
        zcol[py * ZW + px] = (unsigned char)c;
    }
    zdirty = 0;
}
static int zsx(float x) { return (int)floorf((x - zx) / zm + ZW / 2); }
static int zsy(float y) { return (int)floorf((y - zy) / zm + ZH / 2); }
// a thick segment: a quad (two triangles) + round caps
static void thick(float x0, float y0, float x1, float y1, float r, int c) {
    if (r < 0.75f) { line((int)x0, (int)y0, (int)x1, (int)y1, c); return; }
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
    if (l > 0.01f) {
        float nx = -dy / l * r, ny = dx / l * r;
        trifill((int)(x0 + nx), (int)(y0 + ny), (int)(x1 + nx), (int)(y1 + ny), (int)(x1 - nx), (int)(y1 - ny), c);
        trifill((int)(x0 + nx), (int)(y0 + ny), (int)(x1 - nx), (int)(y1 - ny), (int)(x0 - nx), (int)(y0 - ny), c);
    }
    if (r >= 1.2f) circfill((int)x1, (int)y1, (int)r, c);
}
static void zoom_draw(void) {
    if (zdirty) zoom_render();
    for (int y = 0; y < ZH; y++) {
        int x = 0;
        while (x < ZW) { int c = zcol[y * ZW + x], x0 = x; while (x < ZW && zcol[y * ZW + x] == c) x++; rectfill(x0, y, x - x0, 1, c); }
    }
    float x0w = zx - ZW / 2 * zm - 200, x1w = zx + ZW / 2 * zm + 200, y0w = zy - ZH / 2 * zm - 200, y1w = zy + ZH / 2 * zm + 200;
    // rivers: smooth chains, width from the area drained
    for (int r = 0; r < nrv; r++)
        for (int i = rv[r].start + 1; i < rv[r].start + rv[r].n; i++) {
            float ax = rvx[i - 1], ay = rvy[i - 1], bx = rvx[i], by = rvy[i];
            if (fmaxf(ax, bx) < x0w || fminf(ax, bx) > x1w || fmaxf(ay, by) < y0w || fminf(ay, by) > y1w) continue;
            thick(zsx(ax), zsy(ay), zsx(bx), zsy(by), rvw[i] * 0.5f / zm, CLR_TRUE_BLUE);
        }
    // roads: all casings, then fills by class
    for (int pass = 0; pass < 2; pass++)
        for (int cls = RC_TRACK; cls < NRC; cls++)
            for (int p = 0; p < npl; p++) {
                if (pl[p].cls != cls) continue;
                float r = RHW[cls] / zm + (pass == 0 ? fmaxf(1.0f, 1.5f / zm) : 0);
                int col2 = pass == 0 ? CLR_BROWNISH_BLACK : RCOL[cls];
                for (int i = pl[p].start + 1; i < pl[p].start + pl[p].n; i++) {
                    float ax = plx[i - 1], ay = ply[i - 1], bx = plx[i], by = ply[i];
                    if (fmaxf(ax, bx) < x0w || fminf(ax, bx) > x1w || fmaxf(ay, by) < y0w || fminf(ay, by) > y1w) continue;
                    thick(zsx(ax), zsy(ay), zsx(bx), zsy(by), r, col2);
                }
            }
    // bridges: a deck wider than the road, the road over it, parapets on both sides
    for (int b = 0; b < nbr; b++) {
        Bridge *q = &br[b];
        if (q->x < x0w || q->x > x1w || q->y < y0w || q->y > y1w) continue;
        float hl = q->len * 0.5f, hw = RHW[q->cls];
        float ax = q->x - q->dx * hl, ay = q->y - q->dy * hl, bx = q->x + q->dx * hl, by = q->y + q->dy * hl;
        float nx = -q->dy, ny = q->dx;
        thick(zsx(ax), zsy(ay), zsx(bx), zsy(by), (hw + 2.0f) / zm, CLR_DARK_GREY);
        thick(zsx(ax), zsy(ay), zsx(bx), zsy(by), hw / zm, RCOL[q->cls]);
        for (int sd = -1; sd <= 1; sd += 2) {
            float o = (hw + 1.6f) * sd;
            line(zsx(ax + nx * o), zsy(ay + ny * o), zsx(bx + nx * o), zsy(by + ny * o), CLR_WHITE);
        }
    }
    // houses (the exported footprints)
    if (zm < 6.0f) for (int k = 0; k < nhs; k++) {
        const House *q = &hs[k];
        if (q->x < x0w || q->x > x1w || q->y < y0w || q->y > y1w) continue;
        float cx[4], cy[4]; hs_corners(q, cx, cy);
        int hc = BK[q->t].landmark ? CLR_WHITE : BK[q->t].sign ? CLR_PINK : q->t == BK_TERRACE ? CLR_PEACH
               : BK[q->t].wall == BK_WALL_TIMBER ? CLR_BROWN : CLR_LIGHT_PEACH;
        trifill(zsx(cx[0]), zsy(cy[0]), zsx(cx[1]), zsy(cy[1]), zsx(cx[2]), zsy(cy[2]), hc);
        trifill(zsx(cx[0]), zsy(cy[0]), zsx(cx[2]), zsy(cy[2]), zsx(cx[3]), zsy(cy[3]), hc);
    }
    // farmsteads: a house and a barn
    for (int f = 0; f < nfarm; f++) {
        int sx = zsx((farm[f] % N + 0.5f) * CELL), sy = zsy((farm[f] / N + 0.5f) * CELL);
        if (sx < -10 || sy < -10 || sx > ZW + 10 || sy > ZH + 10) continue;
        int hs = (int)fmaxf(2, 12 / zm), bs = (int)fmaxf(2, 18 / zm);
        rectfill(sx - hs, sy - hs, hs * 2, hs * 2, CLR_DARK_RED);
        rectfill(sx + hs + 1, sy - bs / 2, bs * 2, bs, CLR_DARK_BROWN);
    }
    // settlements (placeholder until towns get streets): a ring sized by the place + its rank
    font(FONT_SMALL);
    static const char *RN[3] = { "hamlet", "village", "TOWN" };
    for (int s = 0; s < nset; s++) {
        float x = (set[s].x + 0.5f) * CELL, y = (set[s].y + 0.5f) * CELL;
        int sx = zsx(x), sy = zsy(y);
        if (sx < -60 || sy < -60 || sx > ZW + 60 || sy > ZH + 60) continue;
        circ(sx, sy, (int)(set_r[s] / zm), set[s].rank == ST_TOWN ? CLR_RED : CLR_WHITE);
        print(str("%s %s", set_name[s], RN[set[s].rank]), sx + 4, sy - 3, CLR_WHITE);
    }
    // legend + scale
    rectfill(0, 0, ZW, 11, CLR_BROWNISH_BLACK);
    print(str("ZOOM  %.1f m/px   TAB back   drag pan   wheel zoom", zm), 4, 3, CLR_LIGHT_PEACH);
    int lx = 250;
    for (int c = RC_EXPRESS; c >= RC_TRACK; c--) {
        rectfill(lx, 4, 6, 3, RCOL[c]); print(RCN[c], lx + 8, 3, CLR_LIGHT_GREY); lx += text_width(RCN[c]) + 14;
    }
    float bar_m = zm < 3 ? 100 : zm < 12 ? 500 : 1000; int bpx = (int)(bar_m / zm);
    rectfill(6, ZH - 8, bpx, 2, CLR_WHITE);
    print(bar_m >= 1000 ? "1 km" : str("%d m", (int)bar_m), 10 + bpx, ZH - 11, CLR_WHITE);
    font(FONT_NORMAL);
}
static void zoom_input(void) {
    int mx = mouse_x(), my = mouse_y();
    float w = mouse_wheel();
    if (w != 0) {                                             // zoom about the mouse
        float wx = zx + (mx - ZW / 2) * zm, wy = zy + (my - ZH / 2) * zm;
        zm = clamp(zm * (w > 0 ? 0.8f : 1.25f), 1.0f, 60.0f);
        zx = wx - (mx - ZW / 2) * zm; zy = wy - (my - ZH / 2) * zm; zdirty = 1;
    }
    if (mouse_pressed(MOUSE_LEFT)) { zdrag = 1; zmx = mx; zmy = my; }
    if (zdrag) {
        if (!mouse_down(MOUSE_LEFT)) zdrag = 0;
        else if (mx != zmx || my != zmy) { zx -= (mx - zmx) * zm; zy -= (my - zmy) * zm; zmx = mx; zmy = my; zdirty = 1; }
    }
}

static void draw_map(void) {
    // always MAP_PX across: sample the grid (N may be 256, 512 or 1024); horizontal runs
    #define MCOL(px, py) col[((py) * N / MAP_PX) * N + (px) * N / MAP_PX]
    for (int y = 0; y < MAP_PX; y++) {
        int x = 0;
        while (x < MAP_PX) {
            int c = MCOL(x, y), x0 = x;
            while (x < MAP_PX && MCOL(x, y) == c) x++;
            rectfill(MAP_X + x0, MAP_Y + y, x - x0, 1, c);
        }
    }
    #undef MCOL
    rect(MAP_X - 1, MAP_Y - 1, MAP_PX + 2, MAP_PX + 2, CLR_DARK_GREY);
    if (view != V_FLOW) { draw_roads_overview(); if (!roads_dirty) for (int f = 0; f < nfarm; f++) pset(MAP_X + farm[f] % N * MAP_PX / N, MAP_Y + farm[f] / N * MAP_PX / N, CLR_DARK_PEACH); }
    if (view != V_FLOW)
        for (int r = ST_HAMLET; r <= ST_TOWN; r++)               // small first, towns on top
            for (int k = 0; k < nset; k++) {
                if (set[k].rank != r) continue;
                int sx = MAP_X + set[k].x * MAP_PX / N, sy = MAP_Y + set[k].y * MAP_PX / N;
                if (r == ST_TOWN)        { rectfill(sx - 3, sy - 3, 7, 7, CLR_BROWNISH_BLACK); rectfill(sx - 2, sy - 2, 5, 5, CLR_RED); }
                else if (r == ST_VILLAGE){ rectfill(sx - 2, sy - 2, 5, 5, CLR_BROWNISH_BLACK); rectfill(sx - 1, sy - 1, 3, 3, CLR_WHITE); }
                else                     { rectfill(sx - 1, sy - 1, 3, 3, CLR_BROWNISH_BLACK); pset(sx, sy, CLR_LIGHT_PEACH); }
            }
    if (view != V_FLOW) {                                       // town names (villages show on hover)
        font(FONT_TINY);
        for (int k = 0; k < nset; k++) if (set[k].rank == ST_TOWN) {
            int sx = MAP_X + set[k].x * MAP_PX / N, sy = MAP_Y + set[k].y * MAP_PX / N;
            int tw = (int)strlen(set_name[k]) * 4, tx = sx + 5 + tw > MAP_X + MAP_PX ? sx - 5 - tw : sx + 5;
            rectfill(tx - 1, sy - 3, tw + 1, 7, CLR_BROWNISH_BLACK);
            print(set_name[k], tx, sy - 2, CLR_WHITE);
        }
        font(FONT_SMALL);
    }
    // wind: an arrow in the top-right corner, blowing FROM k_wind
    {
        float a = k_wind * 6.2831853f; int cx = MAP_X + MAP_PX - 16, cy = MAP_Y + 16;
        float dx = -cosf(a), dy = -sinf(a);                      // downwind
        circfill(cx, cy, 11, CLR_DARKER_BLUE);
        int hx = cx + (int)(dx * 8), hy = cy + (int)(dy * 8), tx = cx - (int)(dx * 8), ty = cy - (int)(dy * 8);
        line(tx, ty, hx, hy, CLR_WHITE);
        line(hx, hy, hx - (int)(dx * 4 - dy * 3), hy - (int)(dy * 4 + dx * 3), CLR_WHITE);
        line(hx, hy, hx - (int)(dx * 4 + dy * 3), hy - (int)(dy * 4 - dx * 3), CLR_WHITE);
    }
    int kb = (int)(10000 / MAPCELL);                    // 10 km bar
    rectfill(MAP_X + 4, MAP_Y + MAP_PX - 7, kb, 2, CLR_WHITE);
    print("10 km", MAP_X + 8 + kb, MAP_Y + MAP_PX - 10, CLR_WHITE);
}

void draw(void) {
    if (dirty) rebuild();
    if (roads_dirty && !mouse_down(MOUSE_LEFT)) build_roads();   // routing waits for the slider release
    if (zoom_on) { cls(CLR_DARKER_BLUE); zoom_draw(); return; }
    cls(CLR_BROWNISH_BLACK);
    print("ISLAND STUDIO", MAP_X, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print(str("%s   seed %d   %dx%d (%.0f m)   %s", island_name, seed, N, N, CELL, VIEWS[view]), MAP_X + 112, 8, CLR_MEDIUM_GREY);
    draw_map();

    int mx = mouse_x() - MAP_X, my = mouse_y() - MAP_Y;
    if (mx >= 0 && my >= 0 && mx < MAP_PX && my < MAP_PX) {
        int i = (my * N / MAP_PX) * N + mx * N / MAP_PX;
        print(str("%.1f, %.1f km  %s %d m  %s  rain %d%%  suit %d%%", mx * MAPCELL / 1000, my * MAPCELL / 1000,
                  H[i] <= 0 ? "depth" : "height", (int)fabsf(H[i]), CVN[CV[i]],
                  (int)(M[i] * 100), (int)(SU[i] * 100)),
              MAP_X, MAP_Y + MAP_PX + 6, CLR_LIGHT_GREY);
        int best = -1; float bd = 7 * 7;                        // the place under the mouse, by name
        for (int k = 0; k < nset; k++) {
            float dx = set[k].x * MAP_PX / (float)N - mx, dy = set[k].y * MAP_PX / (float)N - my;
            if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = k; }
        }
        if (best >= 0) {
            static const char *RK[3] = { "hamlet", "village", "town" };
            print(str("%s (%s)", set_name[best], RK[set[best].rank]), MAP_X + 2, MAP_Y + 2, CLR_WHITE);
        }
    }

    int px = PANEL_X, w = 196;
    ui_begin();
    static const char *L[15] = { "island size", "coast warp", "hills", "mountains",
                                 "mountain area", "rugged", "valley carving", "rivers shown",
                                 "wind from", "rainfall", "settlements", "tree line", "farmsteads", "town size", "bridging" };
    float *P[15] = { &k_size, &k_coast, &k_hills, &k_mtn, &k_mreg, &k_rug, &k_ero, &k_riv,
                     &k_wind, &k_rain, &k_vill, &k_tree, &k_farm, &k_town, &k_bridge };
    for (int k = 0; k < 15; k++)
        if (ui_slider(P[k], px, 20 + k * 11, w, L[k])) { if (P[k] == &k_town || P[k] == &k_bridge) roads_dirty = 1; else dirty = 1; }
    if (ui_button(px, 186, 60, 12, "ROLL")) { seed++; dirty = 1; }
    if (ui_button(px + 66, 186, 60, 12, "VIEW")) { view = (view + 1) % NVIEW; recolour(); }
    if (ui_button(px + 132, 186, 64, 12, str("RES %d", N))) { N = N == 256 ? 512 : N == 512 ? 1024 : 256; dirty = 1; }
    ui_end();

    int y = 201;
    print("THIS ISLAND", px, y, CLR_LIGHT_PEACH); y += 8;
    print(str("land %4d km2   peak %4d m", (int)st_land, (int)st_peak), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("flat lowland %2d%%   above 300 m %2d%%", (int)st_flat, (int)st_high), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("forest %2d%%  grass %2d%%  scrub %2d%%", (int)st_cov[CV_FOREST], (int)st_cov[CV_GRASS], (int)st_cov[CV_SCRUB]), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("moor %2d%%  rock %2d%%  marsh %2d%%", (int)st_cov[CV_MOOR], (int)st_cov[CV_ROCK], (int)st_cov[CV_MARSH]), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("towns %d villages %d hamlets %d coast %d%%", st_rank[ST_TOWN], st_rank[ST_VILLAGE], st_rank[ST_HAMLET], st_coastal), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("rivers %d km  lakes %d  farms %d", (int)st_river, st_lakes, nfarm), px, y, CLR_LIGHT_GREY); y += 10;
    print("ROADS km/km2      main   minor  track", px, y, CLR_LIGHT_PEACH); y += 9;
    if (roads_dirty) print("this island       (routing...)", px, y, CLR_LIGHT_GREY);
    else {
        float L = fmaxf(st_land, 1);
        print(str("this island       %.2f   %.2f   %.2f", (st_road[RC_EXPRESS] + st_road[RC_MAIN] + st_road[RC_SEC]) / L,
                  st_road[RC_LANE] / L, st_road[RC_TRACK] / L), px, y, CLR_LIGHT_GREY);
    }
    y += 8;
    print("isle of man       0.92   0.83   1.03", px, y, CLR_MEDIUM_GREY); y += 8;
    print("sado              0.69   1.91   1.65", px, y, CLR_MEDIUM_GREY); y += 9;
    if (st_overflow) print(str("! %d items dropped: raise MAX* limits", st_overflow), px, y, CLR_RED);
    else print(export_msg_t > 0 ? export_msg : "R roll  V view  TAB zoom  X export", px, y, export_msg_t > 0 ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    font(FONT_NORMAL);
}
