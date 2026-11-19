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
    "detail": "The island is a finite 256 x 256 grid (156 m cells). Noise gives the shape: an island mask with a warped coast, layered rolling hills, and ridged mountains that only grow inside a mountain mask (slide it to zero for a low island). Then the whole map drains: a priority flood fills every pit so each cell has a downhill path to the sea, a flow count adds up how much land drains through each cell, and valleys are carved where the flow is large. Draining again gives the rivers and lakes. A prevailing wind brings rain that the mountains wring out, leaving a dry lee. Natural cover follows from height, slope, rain and water (forest, grassland, scrub, moor, rock, marsh, beach), and settlements claim the flattest, lowest, best-watered, most fertile land, spaced apart, with the biggest few becoming towns. Roads are routed across the terrain by A*, most important first, with a discount for reusing an existing road so routes merge into trunks; traffic sets the class (main road, secondary, lane), a ring road follows the coast where the land allows, an expressway joins the two biggest towns when they are far apart, scattered farmsteads add tracks and through-lanes, and forests and moors get dead-end tracks. Rivers are smooth lines and roads cross them on bridges. The panel compares the result to two real islands measured from OpenStreetMap. Design doc: docs/design/island-studio.md.",
    "controls": "Drag the sliders. R or ROLL = new seed. V or VIEW cycles cover / terrain / slope / suitability / flow. Hover the map for height, cover, rain and suitability. Red squares are towns, white villages, dots hamlets; the arrow shows the wind. The tree line slider goes from bare hills to wooded mountains. TAB opens a full-screen zoom (drag to pan, wheel to zoom) with roads at their real widths."
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
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define N        256
#define WORLD_M  40000.0f
#define CELL     (WORLD_M / N)               // 156.25 m
#define MAP_X    8
#define MAP_Y    22
#define PANEL_X  276

// ── knobs (0..1 sliders) ───────────────────────────────────────────────────────
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
static float k_vill  = 0.50f;   // settlement density (spacing 4.5 km .. 1.5 km)
static float k_farm  = 0.70f;   // farmstead density (spacing ~1.2 km .. 450 m)
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
static int    nset;

// ── grids ──────────────────────────────────────────────────────────────────────
static float H[N * N];          // height (m), sea < 0
static float F[N * N];          // filled height (lakes raised to spill level)
static float A[N * N];          // drained area (cells)
static float C[N * N];          // carve scratch
static int   rcv[N * N];        // downstream cell (-1 = sea / outlet)
static int   order[N * N], norder;
static unsigned char col[N * N];
static float M[N * N];          // moisture 0..1 (rain the cell gets)
static float DW[N * N];         // distance to fresh water (river / lake), metres
static float DS[N * N];         // distance to the sea, metres
static float SU[N * N];         // settlement suitability 0..1
static unsigned char CV[N * N]; // cover class
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
static int   hp_i[N * N * 2];
static float hp_k[N * N * 2];
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
    static unsigned char seen[N * N];
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

static float river_thr(void) { return 40.0f + (1.0f - k_riv) * 900.0f; }   // cells drained

#define RIVER_CELLS 300.0f            // drained cells that count as a real river (~7 km²)
#define LAKE_DEPTH 3.0f                 // shallower filled pits are damp ground, not lakes
static int is_lake(int i) { return H[i] > 0 && F[i] - H[i] > LAKE_DEPTH; }
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
        for (int k = 1; k <= 60; k++) {
            int sx = (int)(x + ux * k + 0.5f), sy = (int)(y + uy * k + 0.5f);
            if (sx < 0 || sy < 0 || sx >= N || sy >= N) break;
            float h = fmaxf(H[sy * N + sx], 0);
            if (k <= 4 && prev > h) local += prev - h;            // air climbing toward us right here
            prev = h;
            ridge = fmaxf(ridge, h);                              // highest ground the air crossed
            if (H[sy * N + sx] <= 0 && k > 2) break;             // reached the sea: air was full
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
    static float T[N * N];
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
            float sm = 0; int n = 0;
            for (int k = -2; k <= 2; k++) for (int l = -2; l <= 2; l++) {
                int xx = x + l, yy = y + k; if (xx < 0 || yy < 0 || xx >= N || yy >= N) continue;
                sm += M[yy * N + xx]; n++;
            }
            T[y * N + x] = sm / n;
        }
        for (int i = 0; i < N * N; i++) M[i] = H[i] > 0 ? T[i] : 1;
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

// ── 7: natural cover ─────────────────────────────────────────────────────────────
static void cover(void) {
    float treeline = 400.0f + k_tree * 900.0f;              // metres, at moderate rain
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int i = y * N + x; float h = H[i], m = M[i], sl = slope_at(x, y);
        int c;
        if (h <= 0)                                          c = CV_SEA;
        else if (is_lake(i))                                 c = CV_LAKE;
        else if (h < 4 && DS[i] < 450)                       c = CV_BEACH;
        else if (sl < 0.03f && h < 25 && m > 0.30f && (DW[i] < 350 || (DS[i] < 600 && A[i] > 20)))
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
static void settle(void) {
    static float raw[N * N];
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
        float s = 0; int n = 0;
        for (int k = -3; k <= 3; k++) for (int l = -3; l <= 3; l++) {
            int xx = x + l, yy = y + k; if (xx < 0 || yy < 0 || xx >= N || yy >= N) continue;
            s += raw[yy * N + xx]; n++;
        }
        SU[y * N + x] = raw[y * N + x] > 0 ? clamp(0.5f * raw[y * N + x] + 0.5f * s / n, 0, 1) : 0;
    }
    float mx = 1e-6f; for (int i = 0; i < N * N; i++) mx = fmaxf(mx, SU[i]);
    for (int i = 0; i < N * N; i++) SU[i] /= mx;            // relative: the island's best spot = 1
    // greedy placement: best cells first, each keeps a spacing
    static int cand[N * N]; int nc = 0;
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
        float size = 0; int R = 16;                           // good land within 2.5 km
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
static unsigned char ER[N * N][8];   // road class on edge (cell, dir k); stored both ways
static float TR[N * N];              // traffic through a cell
static unsigned char RING[N * N];    // on the ring road
static int   roads_dirty = 1;
static float st_roads_ms;          // how long the last routing took
enum { RM_LINK, RM_RING, RM_EXPRESS, RM_TRACK };

static int river_cell(int i) { return H[i] > 0 && A[i] >= RIVER_CELLS * 0.5f; }

static float step_cost(int a, int b, int k, int mode) {
    if (H[b] <= 0 || is_lake(b)) return -1;
    float len = k < 4 ? CELL : CELL * 1.41421f;
    float g = fabsf(H[b] - H[a]) / len;                        // grade ALONG the step: contours are cheap
    float m = mode == RM_EXPRESS
        ? 1 + (g / 0.035f) * (g / 0.035f) * 5 + (g > 0.07f ? (g - 0.07f) * 400 : 0)
        : 1 + (g / 0.07f) * (g / 0.07f) * 2.5f + (g > 0.14f ? (g - 0.14f) * 150 : 0);
    int c = CV[b];
    m *= c == CV_MARSH ? 3.0f : c == CV_ROCK ? 2.5f : c == CV_BEACH ? 2.0f
       : c == CV_FOREST ? 1.2f : c == CV_MOOR ? 1.15f : 1.0f;
    // old roads wander: they followed field edges and whoever owned what. A per-cell ±20% cost
    // jitter (fixed per seed) bends the straight lines across flat land; expressways ignore it.
    if (mode != RM_EXPRESS) {
        unsigned hb = (unsigned)b * 2654435761u ^ (unsigned)seed * 97531u; hb ^= hb >> 13; hb *= 0x5bd1e995u; hb ^= hb >> 15;
        m *= 0.8f + 0.4f * (float)(hb & 1023) / 1023.0f;
    }
    if (mode == RM_RING) m *= DS[b] < 160 ? 1.8f : DS[b] < 2200 ? 0.8f : 1.35f;   // hug the coast, off the beach
    float extra = 0;
    int bx = b % N, by = b / N, ax = a % N, ay = a / N;
    int cross = river_cell(b) && !river_cell(a);
    if (k >= 4 && !river_cell(a) && !river_cell(b)                // diagonal step slipping between two river cells
        && river_cell(ay * N + bx) && river_cell(by * N + ax)) cross = 1;
    if (cross) {                                               // a bridge: cost grows with the river
        float span = clamp(sqrtf(fmaxf(A[b], A[ay * N + bx])) / 40.0f, 0.4f, 3.0f);
        extra = (mode == RM_EXPRESS ? 2.0f : 5.0f) * CELL * span;
    }
    if (river_cell(a) && river_cell(b)) m *= 6;                // don't drive along the river bed
    if (mode != RM_EXPRESS) {
        if (ER[a][k] == RC_EXPRESS) m *= 1.0f;           // access-controlled: cross it, never ride it
        else if (ER[a][k]) m *= ER[a][k] >= RC_MAIN ? 0.3f : ER[a][k] == RC_TRACK ? 0.7f : 0.4f;   // follow a road
        else if (TR[b] > 0) m *= 0.75f;                        // join one
    }
    return len * m + extra;
}

static float ag[N * N];
static int   af[N * N], astamp[N * N], aclosed[N * N], agen;
static int   pbuf[N * N];
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
#define MAXPL 3000
#define MAXPT 160000
typedef struct { int start, n, cls; } Poly;
static Poly  pl[MAXPL]; static int npl;
static float plx[MAXPT], ply[MAXPT]; static int npt;
static float st_road[NRC];           // km of road per class
static int   st_express, st_ringgaps;

static int link_a[4096], link_b[4096]; static float link_w[4096];
static int link_cmp_idx[4096];
static int link_cmp(const void *x, const void *y) {
    float a = link_w[*(const int *)x], b = link_w[*(const int *)y];
    return a < b ? 1 : a > b ? -1 : 0;
}
static int is_node(int c) {
    int deg = 0, cl[2] = { 0, 0 };
    for (int k = 0; k < 8; k++) if (ER[c][k]) { if (deg < 2) cl[deg] = ER[c][k]; deg++; }
    if (deg != 2 || cl[0] != cl[1]) return 1;
    for (int s = 0; s < nset; s++) if (set[s].y * N + set[s].x == c) return 1;
    return 0;
}
static float sm_x[8192], sm_y[8192];
// cell path → smooth curve in metres (into sm_x/sm_y); returns the point count
static int smooth_path(const int *cells, int n) {
    // 1. Douglas-Peucker: keep only the cell-path points that matter (tolerance ~0.6 cell),
    //    which throws away the 8-direction staircase; 2. Catmull-Rom through what's left,
    //    one point every ~20 m: a road that curves instead of kinking.
    float *ax = sm_x, *ay = sm_y; static float kx[1024], ky[1024];
    static unsigned char keep[1024];
    int m0 = n < 1000 ? n : 1000;
    for (int i = 0; i < m0; i++) { kx[i] = (cells[i] % N + 0.5f) * CELL; ky[i] = (cells[i] / N + 0.5f) * CELL; keep[i] = 0; }
    keep[0] = keep[m0 - 1] = 1;
    { static int st[2048]; int sp = 0; st[sp++] = 0; st[sp++] = m0 - 1;
      float tol = CELL * 0.6f;
      while (sp > 0) {
          int b = st[--sp], a = st[--sp];
          float dx = kx[b] - kx[a], dy = ky[b] - ky[a], L = sqrtf(dx * dx + dy * dy) + 1e-6f;
          int worst = -1; float wd = tol;
          for (int i = a + 1; i < b; i++) {
              float d = fabsf((kx[i] - kx[a]) * dy - (ky[i] - ky[a]) * dx) / L;
              if (d > wd) { wd = d; worst = i; }
          }
          if (worst >= 0 && sp + 4 < 2048) { keep[worst] = 1; st[sp++] = a; st[sp++] = worst; st[sp++] = worst; st[sp++] = b; }
      } }
    static float qx[1024], qy[1024]; int q = 0;
    for (int i = 0; i < m0; i++) if (keep[i]) { qx[q] = kx[i]; qy[q] = ky[i]; q++; }
    int m = 0;
    for (int i = 0; i + 1 < q; i++) {
        float p0x = qx[i > 0 ? i - 1 : i], p0y = qy[i > 0 ? i - 1 : i];
        float p1x = qx[i], p1y = qy[i], p2x = qx[i + 1], p2y = qy[i + 1];
        float p3x = qx[i + 2 < q ? i + 2 : i + 1], p3y = qy[i + 2 < q ? i + 2 : i + 1];
        float seg = sqrtf((p2x - p1x) * (p2x - p1x) + (p2y - p1y) * (p2y - p1y));
        int steps = (int)(seg / 20.0f) + 1;
        for (int k = 0; k < steps && m < 8000; k++) {
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
    if (n < 2 || npl >= MAXPL) return;
    int m = smooth_path(cells, n);
    float *ax = sm_x, *ay = sm_y;
    if (npt + m >= MAXPT) return;
    pl[npl].start = npt; pl[npl].n = m; pl[npl].cls = cls; npl++;
    float L = 0;
    for (int i = 0; i < m; i++) {
        plx[npt + i] = ax[i]; ply[npt + i] = ay[i];
        if (i) L += sqrtf((ax[i] - ax[i - 1]) * (ax[i] - ax[i - 1]) + (ay[i] - ay[i - 1]) * (ay[i] - ay[i - 1]));
    }
    npt += m; st_road[cls] += L / 1000.0f;
}
static void build_polys(void) {
    static unsigned char vis[N * N];
    static int cells[N * 4];
    npl = 0; npt = 0;
    for (int k = 0; k < NRC; k++) st_road[k] = 0;
    for (int i = 0; i < N * N; i++) vis[i] = 0;
    for (int pass = 0; pass < 2; pass++)                       // pass 0 from nodes, pass 1 leftover loops
        for (int c0 = 0; c0 < N * N; c0++) {
            if (pass == 0 && !is_node(c0)) continue;
            for (int k0 = 0; k0 < 8; k0++) {
                if (!ER[c0][k0] || (vis[c0] >> k0 & 1)) continue;
                int cls = ER[c0][k0], n = 0, cur = c0, k = k0;
                cells[n++] = cur;
                for (;;) {
                    int nx = cur % N + DX[k], ny = cur / N + DY[k], nxt = ny * N + nx;
                    vis[cur] |= 1 << k; vis[nxt] |= 1 << OPP[k];
                    cur = nxt; if (n < N * 4) cells[n++] = cur;
                    if (cur == c0 || is_node(cur)) break;
                    int nk = -1;
                    for (int j = 0; j < 8; j++) if (ER[cur][j] && !(vis[cur] >> j & 1)) { nk = j; break; }
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
#define MAXFARM 2500
static int farm[MAXFARM], nfarm;
static unsigned hsh(unsigned a) { a ^= a >> 16; a *= 0x7feb352dU; a ^= a >> 15; a *= 0x846ca68bU; a ^= a >> 16; return a; }
static int cn_away = -1;            // when >=0: only accept hits away from this cell (a 2nd way out)
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
                if (ax * bx + ay * by > 0 || bx * bx + by * by < 16) on = 0;
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
    int g = (int)(k_farm_sp() / CELL); if (g < 2) g = 2;
    for (int gy = 0; gy < N; gy += g) for (int gx = 0; gx < N; gx += g) {
        unsigned h = hsh((unsigned)(gx * 73856093) ^ (unsigned)(gy * 19349663) ^ (unsigned)(seed * 83492791));
        int x = gx + (int)(h % (unsigned)g), y = gy + (int)((h >> 8) % (unsigned)g);
        if (x >= N || y >= N) continue;
        int i = y * N + x;
        if (H[i] <= 0 || SU[i] <= 0.05f || river_cell(i) || TR[i] > 0) continue;
        if ((float)((h >> 16) & 1023) / 1023.0f > SU[i] * 2.6f) continue;   // keep with p ∝ suitability
        int near = 0;
        for (int s = 0; s < nset && !near; s++) {
            float dx = set[s].x - x, dy = set[s].y - y; if (dx * dx + dy * dy < (450 / CELL) * (450 / CELL)) near = 1;
        }
        if (near || nfarm >= MAXFARM) continue;
        farm[nfarm++] = i;
    }
    for (int f = 0; f < nfarm; f++) {
        int n;
        float cst = connect_to_network(farm[f], pbuf, &n, 4500);
        if (cst < 0) continue;
        float len = (n - 1) * CELL;
        mark(pbuf, n, len > 900 ? RC_LANE : RC_TRACK, 0, 0, 1);
        // a second way out for ~40% of farms: through-lanes and loops instead of a tree of dead ends
        unsigned h2 = hsh((unsigned)farm[f] * 31u + (unsigned)seed);
        if ((h2 & 1023) < 410) {
            cn_away = pbuf[0];                           // pbuf[0] = where the first connection hit
            if (connect_to_network(farm[f], pbuf, &n, 5500) >= 0) mark(pbuf, n, RC_LANE, 0, 0, 1);
            cn_away = -1;
        }
    }
}

// ── 5 (v2): carve valleys from the SMOOTHED river lines, not per cell. Per-cell carving left a
// staircase along every diagonal river (an 8-direction chain touches only at corners, so its
// neighbours stayed high): sawtooth walls, rock teeth, diagonal bands in the hillshade. Now each
// cell is lowered by its distance to the nearest river line, with a valley whose depth and
// half-width grow with the area drained: diagonal valleys are as smooth as straight ones. ──────
#define CARVE_CELLS 80.0f      // ~2 km²: below this a stream is too small to cut a valley (D8 streams on an even slope run parallel at 45°; carving each made stripes)
static void carve_lines(void) {
    static unsigned char up[N * N];
    static int cells[N * 4];
    float km2 = CELL * CELL / 1e6f;
    for (int i = 0; i < N * N; i++) { C[i] = 0; up[i] = 0; }
    #define CARVE_OK(i) (H[i] > 0 && A[i] >= CARVE_CELLS)
    for (int i = 0; i < N * N; i++) if (CARVE_OK(i) && rcv[i] >= 0 && CARVE_OK(rcv[i]) && up[rcv[i]] < 255) up[rcv[i]]++;
    for (int i = 0; i < N * N; i++) {
        if (!CARVE_OK(i) || up[i] == 1) continue;
        int n = 0, c = i;
        for (;;) {
            if (n < N * 4) cells[n++] = c;
            int nx = rcv[c];
            if (nx < 0) break;
            if (!CARVE_OK(nx) || up[nx] != 1) { if (n < N * 4) cells[n++] = nx; break; }
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
static float river_w(float a) { return clamp(sqrtf(a) * 0.35f, 1.5f, 30.0f); }   // metres
static int stream_cell(int i) { return H[i] > 0 && !is_lake(i) && A[i] >= STREAM_CELLS; }
static void build_rivers(void) {
    static unsigned char up[N * N];
    static int cells[N * 4];
    nrv = 0; nrpt = 0;
    for (int i = 0; i < N * N; i++) up[i] = 0;
    for (int i = 0; i < N * N; i++) if (stream_cell(i) && rcv[i] >= 0 && stream_cell(rcv[i]) && up[rcv[i]] < 255) up[rcv[i]]++;
    for (int i = 0; i < N * N; i++) {
        if (!stream_cell(i) || up[i] == 1) continue;           // chains start at sources and confluences
        int n = 0, c = i;
        for (;;) {
            if (n < N * 4) cells[n++] = c;
            int nx = rcv[c];
            if (nx < 0) break;
            if (!stream_cell(nx)) { if (n < N * 4) cells[n++] = nx; break; }   // into the sea / a lake
            if (up[nx] != 1) { if (n < N * 4) cells[n++] = nx; break; }        // a confluence ends the chain
            c = nx;
        }
        if (n < 2 || nrv >= MAXRV) continue;
        int m = smooth_path(cells, n);
        if (nrpt + m >= MAXRPT) break;
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
#define MAXBR 800
#define BG 64                                  // bucket grid over the world for river segments
typedef struct { float x, y, dx, dy, len; int cls; } Bridge;
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
                    if (dup || nbr >= MAXBR) continue;
                    float rdx = bx - ax, rdy = by - ay, rl = sqrtf(rdx * rdx + rdy * rdy) + 1e-6f; rdx /= rl; rdy /= rl;
                    float wdx = rvx[j] - rvx[j - 1], wdy = rvy[j] - rvy[j - 1], wl = sqrtf(wdx * wdx + wdy * wdy) + 1e-6f;
                    float sn = fabsf(rdx * wdy / wl - rdy * wdx / wl);   // |sin| of the crossing angle
                    br[nbr].x = ix; br[nbr].y = iy; br[nbr].dx = rdx; br[nbr].dy = rdy;
                    br[nbr].len = fminf(w / fmaxf(sn, 0.35f) + 10.0f, 160.0f); br[nbr].cls = pl[p].cls; nbr++;
                }
            }
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

static void build_roads(void) {
    clock_t t0 = clock();
    for (int i = 0; i < N * N; i++) { TR[i] = 0; RING[i] = 0; for (int k = 0; k < 8; k++) ER[i][k] = 0; }
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
    for (int j = 0; j < nl; j++) {
        int i = link_cmp_idx[j], a = link_a[i], b = link_b[i];
        float d = set_dist(a, b);
        float cst = astar(set[a].y * N + set[a].x, set[b].y * N + set[b].x, RM_LINK, pbuf, &n, d * 3.5f + 2500);
        if (cst < 0) continue;
        mark(pbuf, n, RC_LANE, link_w[i], 0, 0);
    }
    // 4. classes from traffic: quantiles over road cells; the ring is at least a main road
    static float tv[N * N]; int nt = 0;
    for (int i = 0; i < N * N; i++) if (TR[i] > 0) tv[nt++] = TR[i];
    for (int i = 1; i < nt; i++) { float v = tv[i]; int j = i - 1; while (j >= 0 && tv[j] > v) { tv[j + 1] = tv[j]; j--; } tv[j + 1] = v; }
    float t_main = nt ? tv[(int)(nt * 0.78f)] : 1, t_sec = nt ? tv[(int)(nt * 0.35f)] : 1;
    for (int c = 0; c < N * N; c++) for (int k = 0; k < 8; k++) {
        if (ER[c][k] != RC_LANE) continue;
        int nx = c % N + DX[k], ny = c / N + DY[k], o = ny * N + nx;
        float t = fminf(TR[c], TR[o]);
        int cls = t >= t_main ? RC_MAIN : t >= t_sec ? RC_SEC : RC_LANE;
        if (RING[c] && RING[o] && cls < RC_MAIN) cls = RC_MAIN;
        ER[c][k] = (unsigned char)cls;
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
        if (astar(set[t1].y * N + set[t1].x, set[t2].y * N + set[t2].x, RM_EXPRESS, pbuf, &n, d * 3.0f) >= 0) {
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
    farms();
    upland_tracks();
    build_polys();
    build_rivers();
    build_bridges();
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
            float a = log2f(A[i]);
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

static void rebuild(void) {
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
    cover();
    for (int k = 0; k < NCV; k++) st_cov[k] = 0;
    for (int i = 0; i < N * N; i++) if (H[i] > 0) st_cov[CV[i]] += 1;
    for (int k = 0; k < NCV; k++) st_cov[k] = land ? 100.0f * st_cov[k] / land : 0;
    settle();
    st_rank[0] = st_rank[1] = st_rank[2] = 0; st_coastal = 0;
    for (int k = 0; k < nset; k++) if (DS[set[k].y * N + set[k].x] < 1000) st_coastal++;
    st_coastal = nset ? 100 * st_coastal / nset : 0;
    for (int k = 0; k < nset; k++) st_rank[set[k].rank]++;
    recolour();
    dirty = 0; roads_dirty = 1; zdirty = 1;
}

void init(void) { rebuild(); build_roads(); }

void update(void) {
#ifdef DE_TRACE
    watch("seed", "%d", seed); watch("nset", "%d", nset); watch("coastal", "%d", st_coastal);
    watch("moor", "%d", (int)st_cov[CV_MOOR]); watch("forest", "%d", (int)st_cov[CV_FOREST]);
    watch("land", "%d", (int)st_land); watch("npl", "%d", npl); watch("express", "%d", st_express);
    watch("ringgaps", "%d", st_ringgaps); watch("bridges", "%d", nbr); watch("lakes", "%d", st_lakes); { int bb = -1; for (int b = 0; b < nbr; b++) if (bb < 0 || br[b].len > br[bb].len) bb = b;
      if (bb >= 0) { watch("bigbridge_x", "%.0f", br[bb].x); watch("bigbridge_y", "%.0f", br[bb].y); watch("bigbridge_len", "%.0f", br[bb].len); } } watch("rivers", "%d", nrv);
    watch("forest_tracks", "%d", nforest); watch("upland_tracks", "%d", nupland); watch("roads_ms", "%.0f", st_roads_ms);
    for (int k = RC_TRACK; k < NRC; k++) watch(RCN[k], "%.0f", st_road[k]);
#endif
    if (keyp(KEY_TAB)) {
        zoom_on = !zoom_on;
        int mx = mouse_x() - MAP_X, my = mouse_y() - MAP_Y;
        if (zoom_on && mx >= 0 && my >= 0 && mx < N && my < N) { zx = (mx + 0.5f) * CELL; zy = (my + 0.5f) * CELL; zdirty = 1; }
    }
    if (zoom_on) { zoom_input(); return; }
    if (keyp('R')) { seed++; dirty = 1; }
    if (keyp('V')) { view = (view + 1) % NVIEW; recolour(); }
}

// ── roads on the overview: smoothed polylines, minor first ────────────────────
static void draw_roads_overview(void) {
    if (roads_dirty) return;
    for (int cls = RC_TRACK; cls < NRC; cls++)
        for (int p = 0; p < npl; p++) {
            if (pl[p].cls != cls) continue;
            for (int i = pl[p].start + 1; i < pl[p].start + pl[p].n; i++)
                line(MAP_X + (int)(plx[i - 1] / CELL), MAP_Y + (int)(ply[i - 1] / CELL),
                     MAP_X + (int)(plx[i] / CELL), MAP_Y + (int)(ply[i] / CELL), RCOL[cls]);
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
            float jx = x + (nz(x / 90.0f, y / 90.0f, 71.0f) - 0.5f) * CELL * 0.9f;   // soften cell edges
            float jy = y + (nz(x / 90.0f, y / 90.0f, 73.0f) - 0.5f) * CELL * 0.9f;
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
        float rm = set[s].rank == ST_TOWN ? 650 : set[s].rank == ST_VILLAGE ? 260 : 120;
        circ(sx, sy, (int)(rm / zm), set[s].rank == ST_TOWN ? CLR_RED : CLR_WHITE);
        print(RN[set[s].rank], sx + 4, sy - 3, CLR_WHITE);
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
    for (int y = 0; y < N; y++) {                       // horizontal runs: one rectfill per run
        int x = 0;
        while (x < N) {
            int c = col[y * N + x], x0 = x;
            while (x < N && col[y * N + x] == c) x++;
            rectfill(MAP_X + x0, MAP_Y + y, x - x0, 1, c);
        }
    }
    rect(MAP_X - 1, MAP_Y - 1, N + 2, N + 2, CLR_DARK_GREY);
    if (view != V_FLOW) { draw_roads_overview(); if (!roads_dirty) for (int f = 0; f < nfarm; f++) pset(MAP_X + farm[f] % N, MAP_Y + farm[f] / N, CLR_DARK_PEACH); }
    if (view != V_FLOW)
        for (int r = ST_HAMLET; r <= ST_TOWN; r++)               // small first, towns on top
            for (int k = 0; k < nset; k++) {
                if (set[k].rank != r) continue;
                int sx = MAP_X + set[k].x, sy = MAP_Y + set[k].y;
                if (r == ST_TOWN)        { rectfill(sx - 3, sy - 3, 7, 7, CLR_BROWNISH_BLACK); rectfill(sx - 2, sy - 2, 5, 5, CLR_RED); }
                else if (r == ST_VILLAGE){ rectfill(sx - 2, sy - 2, 5, 5, CLR_BROWNISH_BLACK); rectfill(sx - 1, sy - 1, 3, 3, CLR_WHITE); }
                else                     { rectfill(sx - 1, sy - 1, 3, 3, CLR_BROWNISH_BLACK); pset(sx, sy, CLR_LIGHT_PEACH); }
            }
    // wind: an arrow in the top-right corner, blowing FROM k_wind
    {
        float a = k_wind * 6.2831853f; int cx = MAP_X + N - 16, cy = MAP_Y + 16;
        float dx = -cosf(a), dy = -sinf(a);                      // downwind
        circfill(cx, cy, 11, CLR_DARKER_BLUE);
        int hx = cx + (int)(dx * 8), hy = cy + (int)(dy * 8), tx = cx - (int)(dx * 8), ty = cy - (int)(dy * 8);
        line(tx, ty, hx, hy, CLR_WHITE);
        line(hx, hy, hx - (int)(dx * 4 - dy * 3), hy - (int)(dy * 4 + dx * 3), CLR_WHITE);
        line(hx, hy, hx - (int)(dx * 4 + dy * 3), hy - (int)(dy * 4 - dx * 3), CLR_WHITE);
    }
    int kb = (int)(10000 / CELL);                       // 10 km bar
    rectfill(MAP_X + 4, MAP_Y + N - 7, kb, 2, CLR_WHITE);
    print("10 km", MAP_X + 8 + kb, MAP_Y + N - 10, CLR_WHITE);
}

void draw(void) {
    if (dirty) rebuild();
    if (roads_dirty && !mouse_down(MOUSE_LEFT)) build_roads();   // routing waits for the slider release
    if (zoom_on) { cls(CLR_DARKER_BLUE); zoom_draw(); return; }
    cls(CLR_BROWNISH_BLACK);
    print("ISLAND STUDIO", MAP_X, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print(str("40 x 40 km   seed %d   view: %s", seed, VIEWS[view]), MAP_X + 112, 8, CLR_MEDIUM_GREY);
    draw_map();

    int mx = mouse_x() - MAP_X, my = mouse_y() - MAP_Y;
    if (mx >= 0 && my >= 0 && mx < N && my < N) {
        int i = my * N + mx;
        print(str("%.1f, %.1f km  %s %d m  %s  rain %d%%  suit %d%%", mx * CELL / 1000, my * CELL / 1000,
                  H[i] <= 0 ? "depth" : "height", (int)fabsf(H[i]), CVN[CV[i]],
                  (int)(M[i] * 100), (int)(SU[i] * 100)),
              MAP_X, MAP_Y + N + 6, CLR_LIGHT_GREY);
    }

    int px = PANEL_X, w = 196;
    ui_begin();
    static const char *L[13] = { "island size", "coast warp", "hills", "mountains",
                                 "mountain area", "rugged", "valley carving", "rivers shown",
                                 "wind from", "rainfall", "settlements", "tree line", "farmsteads" };
    float *P[13] = { &k_size, &k_coast, &k_hills, &k_mtn, &k_mreg, &k_rug, &k_ero, &k_riv,
                     &k_wind, &k_rain, &k_vill, &k_tree, &k_farm };
    for (int k = 0; k < 13; k++)
        if (ui_slider(P[k], px, 20 + k * 12, w, L[k])) dirty = 1;
    if (ui_button(px, 178, 60, 12, "ROLL")) { seed++; dirty = 1; }
    if (ui_button(px + 66, 178, 60, 12, "VIEW")) { view = (view + 1) % NVIEW; recolour(); }
    ui_end();

    int y = 193;
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
    print("R roll  V view  TAB zoom  hover info", px, y, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
