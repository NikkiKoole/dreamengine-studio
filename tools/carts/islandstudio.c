/* de:meta
{
  "slug": "islandstudio",
  "title": "island studio",
  "status": "wip",
  "created": "2026-11-19",
  "kind": ["tech-demo", "generative"],
  "teaches": ["noise-terrain"],
  "lineage": "A bench for backroads' finite island: worldnet-style layered noise for the shape, plus what an endless world can't do, a whole-map priority-flood drainage pass (Barnes 2014) and stream-power valley carving, so rivers run to the sea and valleys exist for roads to follow.",
  "description": {
    "summary": "Grow a 40 x 40 km island: coast, hills and mountains, then water drains and carves valleys, the wind brings rain and a rain shadow, cover grows, and people settle the best land.",
    "detail": "The island is a finite 256 x 256 grid (156 m cells). Noise gives the shape: an island mask with a warped coast, layered rolling hills, and ridged mountains that only grow inside a mountain mask (slide it to zero for a low island). Then the whole map drains: a priority flood fills every pit so each cell has a downhill path to the sea, a flow count adds up how much land drains through each cell, and valleys are carved where the flow is large. Draining again gives the rivers and lakes. A prevailing wind brings rain that the mountains wring out, leaving a dry lee. Natural cover follows from height, slope, rain and water (forest, grassland, scrub, moor, rock, marsh, beach), and settlements claim the flattest, lowest, best-watered, most fertile land, spaced apart, with the biggest few becoming towns. The panel compares the result to two real islands measured from OpenStreetMap. Design doc: docs/design/island-studio.md.",
    "controls": "Drag the sliders. R or ROLL = new seed. V or VIEW cycles cover / terrain / slope / suitability / flow. Hover the map for height, cover, rain and suitability. Red squares are towns, white villages, dots hamlets; the arrow shows the wind. The tree line slider goes from bare hills to wooded mountains."
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

// stats
static float st_land, st_peak, st_flat, st_high, st_river, st_cov[NCV];
static int   st_lakes, st_rank[3], st_coastal;   // st_coastal: % of settlements within 1 km of the sea

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
static void carve(void) {
    float km2 = CELL * CELL / 1e6f;                     // area of one cell, km²
    for (int i = 0; i < N * N; i++)
        C[i] = H[i] > 0 ? fminf(k_ero * 55.0f * sqrtf(A[i] * km2), H[i] * 0.7f) : 0;
    for (int pass = 0; pass < 2; pass++) {              // widen: a valley, not a slot
        static float T[N * N];
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
            float s = 0, w = 0;
            for (int k = -1; k <= 1; k++) for (int l = -1; l <= 1; l++) {
                int xx = x + l, yy = y + k; if (xx < 0 || yy < 0 || xx >= N || yy >= N) continue;
                float c = C[yy * N + xx]; float ww = (k == 0 && l == 0) ? 2.0f : 1.0f;
                s += fmaxf(c * 0.85f, 0) * ww; w += ww;
            }
            T[y * N + x] = fmaxf(C[y * N + x], s / w);
        }
        for (int i = 0; i < N * N; i++) C[i] = T[i];
    }
    for (int i = 0; i < N * N; i++) if (H[i] > 0) H[i] = fmaxf(H[i] - C[i], 0.5f);
}

static float slope_at(int x, int y) {                   // rise over run (m/m)
    int x0 = x > 0 ? x - 1 : x, x1 = x < N - 1 ? x + 1 : x;
    int y0 = y > 0 ? y - 1 : y, y1 = y < N - 1 ? y + 1 : y;
    float gx = (H[y * N + x1] - H[y * N + x0]) / ((x1 - x0) * CELL);
    float gy = (H[y1 * N + x] - H[y0 * N + x]) / ((y1 - y0) * CELL);
    return sqrtf(gx * gx + gy * gy);
}

static float river_thr(void) { return 40.0f + (1.0f - k_riv) * 900.0f; }   // cells drained

#define RIVER_CELLS 300.0f            // drained cells that count as a real river (~7 km²)
static int is_lake(int i) { return H[i] > 0 && F[i] - H[i] > 1.5f; }
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
        else if (sl < 0.03f && h < 25 && m > 0.30f && (DW[i] < 500 || (DS[i] < 600 && A[i] > 20)))
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
        if (F[i] - h > 1.5f) { col[i] = CLR_BLUE; continue; }                    // lake
        int xm = x > 0 ? x - 1 : x, ym = y > 0 ? y - 1 : y;
        float shd = (H[i] - H[ym * N + xm]) / CELL;                              // NW-lit hillshade
        if (view == V_COVER) {
            static const int CC[NCV][3] = {
                { 0, 0, 0 }, { 0, 0, 0 },
                { CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW },                 // beach
                { CLR_BLUE_GREEN, CLR_BLUE_GREEN, CLR_TRUE_BLUE },                        // marsh
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
    if (k_ero > 0.01f) { carve(); drain(); }
    // stats
    float km2 = CELL * CELL / 1e6f, thr = river_thr();
    int land = 0, flat = 0, high = 0, riv = 0, lakes = 0; st_peak = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int i = y * N + x; if (H[i] <= 0) continue;
        land++; if (H[i] > st_peak) st_peak = H[i];
        if (H[i] > 300) high++;
        if (slope_at(x, y) < 0.05f && H[i] < 150) flat++;
        if (A[i] >= thr) riv++;
        if (F[i] - H[i] > 1.5f) lakes++;
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
    dirty = 0;
}

void init(void) { rebuild(); }

void update(void) {
#ifdef DE_TRACE
    watch("seed", "%d", seed); watch("nset", "%d", nset); watch("coastal", "%d", st_coastal);
    watch("moor", "%d", (int)st_cov[CV_MOOR]); watch("forest", "%d", (int)st_cov[CV_FOREST]);
#endif
    if (keyp('R')) { seed++; dirty = 1; }
    if (keyp('V')) { view = (view + 1) % NVIEW; recolour(); }
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
    static const char *L[12] = { "island size", "coast warp", "hills", "mountains",
                                 "mountain area", "rugged", "valley carving", "rivers shown",
                                 "wind from", "rainfall", "settlements", "tree line" };
    float *P[12] = { &k_size, &k_coast, &k_hills, &k_mtn, &k_mreg, &k_rug, &k_ero, &k_riv,
                     &k_wind, &k_rain, &k_vill, &k_tree };
    for (int k = 0; k < 12; k++)
        if (ui_slider(P[k], px, 20 + k * 12, w, L[k])) dirty = 1;
    if (ui_button(px, 166, 60, 12, "ROLL")) { seed++; dirty = 1; }
    if (ui_button(px + 66, 166, 60, 12, "VIEW")) { view = (view + 1) % NVIEW; recolour(); }
    ui_end();
    if (dirty) rebuild();

    int y = 182;
    print("THIS ISLAND", px, y, CLR_LIGHT_PEACH); y += 9;
    print(str("land %4d km2   peak %4d m", (int)st_land, (int)st_peak), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("flat lowland %2d%%   above 300 m %2d%%", (int)st_flat, (int)st_high), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("forest %2d%%  grass %2d%%  scrub %2d%%", (int)st_cov[CV_FOREST], (int)st_cov[CV_GRASS], (int)st_cov[CV_SCRUB]), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("moor %2d%%  rock %2d%%  marsh %2d%%", (int)st_cov[CV_MOOR], (int)st_cov[CV_ROCK], (int)st_cov[CV_MARSH]), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("towns %d  villages %d  hamlets %d  coastal %d%%", st_rank[ST_TOWN], st_rank[ST_VILLAGE], st_rank[ST_HAMLET], st_coastal), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("rivers %d km   lakes %d", (int)st_river, st_lakes), px, y, CLR_LIGHT_GREY); y += 12;
    print("REAL ISLANDS", px, y, CLR_LIGHT_PEACH); y += 9;
    print("isle of man   572 km2   peak  621 m", px, y, CLR_MEDIUM_GREY); y += 8;
    print("sado          855 km2   peak 1172 m", px, y, CLR_MEDIUM_GREY); y += 12;
    print("R roll   V view   hover = details", px, y, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
