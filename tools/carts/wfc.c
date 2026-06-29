/* de:meta
{
  "title": "wfc terrain",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "tech-demo",
    "generative"
  ],
  "teaches": [
    "wave-function-collapse",
    "marching-squares",
    "autotiling"
  ],
  "lineage": "Purpose-built tech demo for WFC terrain generation paired with marching-squares autotiling, showing the two techniques as a live before/after toggle.",
  "description": "Wave Function Collapse (simple tiled model) generating terrain, then auto-tiling for smooth transitions. Every cell starts in superposition; OBSERVE collapses the lowest-entropy cell, PROPAGATE ripples the constraint out (a level may only touch levels within +/-1, so water never borders forest -> beaches must appear). A neighbour-agreement bonus grows coherent islands & bays instead of noise. Renders two ways: BANDS (flat colour per cell -- the hard-edged worldgen look) and SMOOTH (marching-squares auto-tiling: each level's coastline drawn over the level below). B toggles between them = a live before/after of generation (WFC) vs transitions (auto-tiling). Opens on a finished map; SPACE step, A auto, R regenerate (watch it collapse)."
}
de:meta */
#include "studio.h"

// WFC — Wave Function Collapse (simple tiled model), terrain edition.
// Every cell starts in SUPERPOSITION (could be any terrain level). Repeatedly:
//   1. OBSERVE  — find the cell with the fewest options left (lowest entropy),
//                 collapse it to one level (weighted-random).
//   2. PROPAGATE— that choice forbids some neighbours; ripple the constraint out
//                 until the grid settles (or hits a contradiction -> restart).
// The adjacency rule IS the terrain logic: a level may only touch levels within
// ±1 of itself, so water never borders forest — beaches/edges must appear. That
// gradient is what makes the output read as land.
//
// Then RENDER two ways (toggle B):
//   * BANDS  — one flat colour per cell (the worldgen/roadnet look: hard steps).
//   * SMOOTH — AUTO-TILING: each level's coastline drawn with marching-squares
//              corner tiles over the level below, so seams blend. THIS is the fix
//              for worldgen's banding — generation (WFC) vs transitions (autotile).
//
// SPACE step   A auto on/off   R regenerate   B bands<->smooth

#define GW 32
#define GH 18
#define P  10           // px per cell  (GW*P=320, GH*P=180; HUD below)
#define HUD_Y (GH * P)
#define NLV 6
#define ALL ((1 << NLV) - 1)

enum { DEEP, SEA, SAND, GRASS, FOREST, ROCK };
static const int LVCOL[NLV] = { CLR_DARK_BLUE, CLR_BLUE, CLR_YELLOW,
                                CLR_GREEN, CLR_DARK_GREEN, CLR_LIGHT_GREY };
static const int LVW[NLV]   = { 2, 3, 3, 7, 4, 2 };   // collapse weights (favour grass -> land with bays)

static int  opts[GH][GW];     // bitmask of still-possible levels per cell
static bool done, autorun;
static int  rmode = 2;        // render: 0 flat bands, 1 diagonal (marching squares), 2 rounded (RPG-Maker corners)
static const char *RNAME[3] = { "flat bands", "auto-tiled (diagonal)", "auto-tiled (rounded)" };
static int  steps, restarts;

// propagation stack — a cell is pushed at most NLV times (one per bit removed)
static int stkx[GW * GH * NLV], stky[GW * GH * NLV], sp;

static int popc(int v) { return __builtin_popcount(v); }
static int low(int v)  { return __builtin_ctz(v); }   // index of lowest set bit

// levels a neighbour may take, given this cell's options: each option L allows L-1,L,L+1
static int spread(int o) {
    int m = 0;
    for (int L = 0; L < NLV; L++) if (o & (1 << L)) {
        m |= 1 << L;
        if (L > 0)       m |= 1 << (L - 1);
        if (L < NLV - 1) m |= 1 << (L + 1);
    }
    return m;
}

static void wfc_reset(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) opts[y][x] = ALL;
    done = false; steps = 0; sp = 0;
}

// arc-consistency ripple from the stack; false on contradiction (a cell emptied)
static bool propagate(void) {
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    while (sp > 0) {
        sp--; int x = stkx[sp], y = stky[sp];
        int allow = spread(opts[y][x]);
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || nx >= GW || ny < 0 || ny >= GH) continue;
            int no = opts[ny][nx] & allow;
            if (no != opts[ny][nx]) {
                if (no == 0) return false;
                opts[ny][nx] = no;
                stkx[sp] = nx; stky[sp] = ny; sp++;
            }
        }
    }
    return true;
}

// collapse the lowest-entropy cell; returns false on contradiction, sets done if none left
static bool observe(void) {
    int best = 99;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        int e = popc(opts[y][x]);
        if (e > 1 && e < best) best = e;
    }
    if (best == 99) { done = true; return true; }
    // reservoir-pick a random cell among those at min entropy (organic, not scan-biased)
    int bx = -1, by = -1, seen = 0;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if (popc(opts[y][x]) == best) { seen++; if (rnd(seen) == 0) { bx = x; by = y; } }
    // weighted choice of a level among this cell's options. Base weight + a strong
    // bonus for matching an already-collapsed neighbour -> regions GROW into blobs
    // (islands & coastlines) instead of high-frequency noise.
    int o = opts[by][bx], w[NLV], total = 0;
    for (int L = 0; L < NLV; L++) w[L] = (o & (1 << L)) ? LVW[L] : 0;
    static const int ndx[4] = { 1, -1, 0, 0 }, ndy[4] = { 0, 0, 1, -1 };
    for (int k = 0; k < 4; k++) {
        int nx = bx + ndx[k], ny = by + ndy[k];
        if (nx < 0 || nx >= GW || ny < 0 || ny >= GH || popc(opts[ny][nx]) != 1) continue;
        int nl = low(opts[ny][nx]);
        for (int L = 0; L < NLV; L++) if (w[L]) {
            if (L == nl) w[L] += 7; else if (L == nl - 1 || L == nl + 1) w[L] += 2;
        }
    }
    for (int L = 0; L < NLV; L++) total += w[L];
    int r = rnd(total), pick = low(o);
    for (int L = 0; L < NLV; L++) if (w[L]) { r -= w[L]; if (r < 0) { pick = L; break; } }
    opts[by][bx] = 1 << pick;
    sp = 0; stkx[sp] = bx; stky[sp] = by; sp++;
    steps++;
    return propagate();
}

static void step(void) {
    if (done) return;
    if (!observe()) { restarts++; wfc_reset(); }   // contradiction -> start over
}

static void solve_all(void) { int g = 0; while (!done && g++ < 5000) step(); }

// open on a finished map (instant payoff + a good thumbnail); R replays the collapse
void init(void) { wfc_reset(); solve_all(); }

void update(void) {
    if (keyp('R')) { restarts = 0; wfc_reset(); autorun = true; }
    if (btnp(0, BTN_A) || keyp('A')) autorun = !autorun;
    if (keyp('B')) rmode = (rmode + 1) % 3;
    if (keyp(KEY_SPACE)) { autorun = false; step(); }
    if (autorun && !done) for (int i = 0; i < 6; i++) step();   // ~6 collapses/frame

#ifdef DE_TRACE
    int rem = 0; for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) if (popc(opts[y][x]) > 1) rem++;
    watch("done", "%d", done); watch("steps", "%d", steps);
    watch("restarts", "%d", restarts); watch("uncollapsed", "%d", rem);
#endif
}

// ---------------------------------------------------------------- render
// fan-fill a small convex polygon
static void fan(int *xs, int *ys, int n, int col) {
    for (int i = 1; i + 1 < n; i++) trifill(xs[0], ys[0], xs[i], ys[i], xs[i + 1], ys[i + 1], col);
}
// marching-squares: fill the "inside" region of one dual-grid tile. Corner bits:
// 1=TL 2=TR 4=BR 8=BL. Tile spans (X,Y)..(X+P,Y+P); mids at the edge centres.
static void fill_case(int X, int Y, int c, int col) {
    int h = P / 2;
    int TLx=X,TLy=Y, TRx=X+P,TRy=Y, BRx=X+P,BRy=Y+P, BLx=X,BLy=Y+P;
    int Tx=X+h,Ty=Y, Rx=X+P,Ry=Y+h, Bx=X+h,By=Y+P, Lx=X,Ly=Y+h;
    int xs[6], ys[6], n = 0;
    #define PT(px,py) do{ xs[n]=px; ys[n]=py; n++; }while(0)
    switch (c) {
        case 0:  return;
        case 15: rectfill(X, Y, P, P, col); return;
        case 1:  PT(TLx,TLy);PT(Tx,Ty);PT(Lx,Ly); break;                       // TL
        case 2:  PT(Tx,Ty);PT(TRx,TRy);PT(Rx,Ry); break;                       // TR
        case 4:  PT(Rx,Ry);PT(BRx,BRy);PT(Bx,By); break;                       // BR
        case 8:  PT(Lx,Ly);PT(Bx,By);PT(BLx,BLy); break;                       // BL
        case 3:  PT(TLx,TLy);PT(TRx,TRy);PT(Rx,Ry);PT(Lx,Ly); break;           // top
        case 6:  PT(Tx,Ty);PT(TRx,TRy);PT(BRx,BRy);PT(Bx,By); break;           // right
        case 12: PT(Lx,Ly);PT(Rx,Ry);PT(BRx,BRy);PT(BLx,BLy); break;          // bottom
        case 9:  PT(TLx,TLy);PT(Tx,Ty);PT(Bx,By);PT(BLx,BLy); break;           // left
        case 7:  PT(TLx,TLy);PT(TRx,TRy);PT(BRx,BRy);PT(Bx,By);PT(Lx,Ly); break;   // -BL
        case 11: PT(TLx,TLy);PT(TRx,TRy);PT(Rx,Ry);PT(Bx,By);PT(BLx,BLy); break;   // -BR
        case 13: PT(TLx,TLy);PT(Tx,Ty);PT(Rx,Ry);PT(BRx,BRy);PT(BLx,BLy); break;   // -TR
        case 14: PT(Tx,Ty);PT(TRx,TRy);PT(BRx,BRy);PT(BLx,BLy);PT(Lx,Ly); break;   // -TL
        case 5:  { int ax[3]={TLx,Tx,Lx},ay[3]={TLy,Ty,Ly}; fan(ax,ay,3,col);      // TL+BR saddle
                   int bx[3]={Rx,BRx,Bx},by[3]={Ry,BRy,By}; fan(bx,by,3,col); return; }
        case 10: { int ax[3]={Tx,TRx,Rx},ay[3]={Ty,TRy,Ry}; fan(ax,ay,3,col);      // TR+BL saddle
                   int bx[3]={Lx,Bx,BLx},by[3]={Ly,By,BLy}; fan(bx,by,3,col); return; }
    }
    #undef PT
    fan(xs, ys, n, col);
}

static int level_at(int x, int y) { return low(opts[y][x]); }   // valid once collapsed

// is cell (x,y) part of layer L? (out of bounds = not, so map edges round off)
static bool fill_at(int x, int y, int L) {
    return x >= 0 && x < GW && y >= 0 && y < GH && level_at(x, y) >= L;
}
// RPG-Maker 4-corner autotiling: draw each cell as 4 quadrants. A quadrant whose
// two edge-neighbours are BOTH empty is an outer corner -> a rounded quarter-disc;
// otherwise it's a straight edge / interior -> a full square. No diagonals.
static void quad(int qx, int qy, int ccx, int ccy, bool oa, bool ob, int col) {
    if (!oa && !ob) {                       // outer convex corner -> quarter circle toward cell centre
        clip(qx, qy, P / 2, P / 2);
        circfill(ccx, ccy, P / 2, col);
        clip(0, 0, 0, 0);
    } else {
        rectfill(qx, qy, P / 2, P / 2, col);
    }
}
static void render_rounded(void) {
    int h = P / 2;
    rectfill(0, 0, SCREEN_W, HUD_Y, LVCOL[DEEP]);
    for (int L = 1; L < NLV; L++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                if (level_at(x, y) < L) continue;
                int cx0 = x * P, cy0 = y * P, ccx = cx0 + h, ccy = cy0 + h, col = LVCOL[L];
                bool N = fill_at(x, y-1, L), S = fill_at(x, y+1, L),
                     W = fill_at(x-1, y, L), E = fill_at(x+1, y, L);
                quad(cx0,     cy0,     ccx, ccy, W, N, col);   // NW
                quad(cx0 + h, cy0,     ccx, ccy, E, N, col);   // NE
                quad(cx0,     cy0 + h, ccx, ccy, W, S, col);   // SW
                quad(cx0 + h, cy0 + h, ccx, ccy, E, S, col);   // SE
            }
}

void draw(void) {
    if (rmode == 2 && done) {
        render_rounded();                       // RPG-Maker corner autotiling (rounded, no diagonals)
    } else if (rmode == 1 && done) {
        // marching-squares auto-tiling: paint sea, then each level's coast over the one below
        rectfill(0, 0, SCREEN_W, HUD_Y, LVCOL[DEEP]);
        for (int L = 1; L < NLV; L++)
            for (int j = 0; j < GH - 1; j++)
                for (int i = 0; i < GW - 1; i++) {
                    int c = (level_at(i, j)     >= L ? 1 : 0)
                          | (level_at(i + 1, j) >= L ? 2 : 0)
                          | (level_at(i + 1, j + 1) >= L ? 4 : 0)
                          | (level_at(i, j + 1) >= L ? 8 : 0);
                    if (c) fill_case(i * P + P / 2, j * P + P / 2, c, LVCOL[L]);
                }
    } else {
        // BANDS / entropy view: one flat block per cell (hard edges, like worldgen)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                int e = popc(opts[y][x]);
                int col = e == 1 ? LVCOL[level_at(x, y)]
                        : e <= 3 ? CLR_MEDIUM_GREY
                        : e <= 4 ? CLR_DARK_GREY : CLR_DARKER_GREY;
                rectfill(x * P, y * P, P, P, col);
            }
    }

    // HUD
    rectfill(0, HUD_Y, SCREEN_W, SCREEN_H - HUD_Y, CLR_BLACK);
    int hx = print("WFC ", 4, HUD_Y + 2, CLR_WHITE);
    print(done ? "done" : "collapsing", hx, HUD_Y + 2, done ? CLR_GREEN : CLR_YELLOW);
    print_right(RNAME[rmode], SCREEN_W - 4, HUD_Y + 2, CLR_BLUE);
    font(FONT_SMALL);
    print(str("SPACE step   A auto   R regenerate   B render mode     steps %d  restarts %d", steps, restarts),
          4, HUD_Y + 13, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
