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
    "summary": "Design a 40 x 40 km island: a warped coast, rolling hills, optional ridged mountains, then water drains, carves valleys and finds the sea.",
    "detail": "The island is a finite 256 x 256 grid (156 m cells). Noise gives the shape: an island mask with a warped coast, layered rolling hills, and ridged mountains that only grow inside a mountain mask (slide it to zero for a low island). Then the whole map drains: a priority flood fills every pit so each cell has a downhill path to the sea, a flow count adds up how much land drains through each cell, and valleys are carved where the flow is large. Draining again gives the rivers and lakes. The panel compares the result to two real islands measured from OpenStreetMap. Design doc: docs/design/island-studio.md.",
    "controls": "Drag the sliders. R or ROLL = new seed. V or VIEW cycles terrain / slope (flat land is where people farm and live) / flow. Hover the map for height and position."
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
static int   seed = 1;
static int   view;              // 0 terrain, 1 slope, 2 flow
static const char *VIEWS[3] = { "terrain", "slope", "flow" };

// ── grids ──────────────────────────────────────────────────────────────────────
static float H[N * N];          // height (m), sea < 0
static float F[N * N];          // filled height (lakes raised to spill level)
static float A[N * N];          // drained area (cells)
static float C[N * N];          // carve scratch
static int   rcv[N * N];        // downstream cell (-1 = sea / outlet)
static int   order[N * N], norder;
static unsigned char col[N * N];
static int   dirty = 1;

// stats
static float st_land, st_peak, st_flat, st_high, st_river;
static int   st_lakes;

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
        if (view == 1) {                                                          // slope
            float s = slope_at(x, y);
            c = s < 0.03f ? CLR_LIME_GREEN : s < 0.08f ? CLR_MEDIUM_GREEN : s < 0.18f ? CLR_DARK_GREEN
              : s < 0.35f ? CLR_BROWN : CLR_DARK_RED;
        } else if (view == 2) {                                                   // flow
            float a = log2f(A[i]);
            c = a < 2 ? CLR_BROWNISH_BLACK : a < 4 ? CLR_DARKER_BLUE : a < 6 ? CLR_DARK_BLUE
              : a < 8 ? CLR_TRUE_BLUE : a < 10 ? CLR_BLUE : CLR_WHITE;
        } else {                                                                  // terrain, NW-lit
            int xm = x > 0 ? x - 1 : x, ym = y > 0 ? y - 1 : y;
            float s = (H[i] - H[ym * N + xm]) / CELL;
            c = h < 3 ? CLR_LIGHT_YELLOW
              : h < 90 ? shade3(LOW, s) : h < 300 ? shade3(MID, s) : h < 650 ? shade3(UP, s)
              : h < 1000 ? shade3(HIGH, s) : shade3(TOP, s);
        }
        if (view != 2 && A[i] >= thr) c = CLR_BLUE;                               // river
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
    recolour();
    dirty = 0;
}

void init(void) { rebuild(); }

void update(void) {
    if (keyp('R')) { seed++; dirty = 1; }
    if (keyp('V')) { view = (view + 1) % 3; recolour(); }
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
        print(str("%.1f, %.1f km   %s %d m", mx * CELL / 1000, my * CELL / 1000,
                  H[i] <= 0 ? "depth" : "height", (int)fabsf(H[i])),
              MAP_X, MAP_Y + N + 6, CLR_LIGHT_GREY);
    }

    int px = PANEL_X, w = 196;
    ui_begin();
    static const char *L[8] = { "island size", "coast warp", "hills", "mountains",
                                "mountain area", "rugged", "valley carving", "rivers shown" };
    float *P[8] = { &k_size, &k_coast, &k_hills, &k_mtn, &k_mreg, &k_rug, &k_ero, &k_riv };
    for (int k = 0; k < 8; k++)
        if (ui_slider(P[k], px, 22 + k * 13, w, L[k])) dirty = 1;
    if (ui_button(px, 132, 60, 13, "ROLL")) { seed++; dirty = 1; }
    if (ui_button(px + 66, 132, 60, 13, "VIEW")) { view = (view + 1) % 3; recolour(); }
    ui_end();
    if (dirty) rebuild();

    int y = 156;
    print("THIS ISLAND", px, y, CLR_LIGHT_PEACH); y += 9;
    print(str("land        %4d km2", (int)st_land), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("peak        %4d m", (int)st_peak), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("flat lowland %3d %%  (farm, towns)", (int)st_flat), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("above 300 m  %3d %%  (empty)", (int)st_high), px, y, CLR_LIGHT_GREY); y += 8;
    print(str("rivers      %4d km   lakes %d", (int)st_river, st_lakes), px, y, CLR_LIGHT_GREY); y += 14;
    print("REAL ISLANDS", px, y, CLR_LIGHT_PEACH); y += 9;
    print("isle of man   572 km2   peak  621 m", px, y, CLR_MEDIUM_GREY); y += 8;
    print("sado          855 km2   peak 1172 m", px, y, CLR_MEDIUM_GREY); y += 8;
    print("40x40 frame  1600 km2", px, y, CLR_MEDIUM_GREY); y += 14;
    print("R roll   V view   hover = height", px, y, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
