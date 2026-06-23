#include "studio.h"
#include <stdio.h>
#include <math.h>

// ============================================================================
// CITYPLAN — a whole procedural WORLD you can LIFT THE ROOF off.
//
//   ◄ ► ▲ ▼ / WASD  pan     Z / X (or wheel)  zoom     R  new seed
//   F  force-lift every roof (dollhouse)   O  field overlay   G  debug grid
//
// ── what this is ──────────────────────────────────────────────────────────────
// The full MERGE of lotfill + interiors. lotfill builds the world BETWEEN buildings
// (terrain, forests, farms, blocks, lots, streets); interiors fills the floor-plan
// INSIDE a footprint. They were always the same fill-language at two scales — so
// cityplan stacks the whole stack and you read it all by ZOOMING:
//   • a WORLD selector reads terrain + an urbanization field and picks, per region,
//     WILD land → scatter (forest/meadow/beach/scree/waste), FARMLAND → rows of crops,
//     or CITY → blocks of buildings.
//   • zoom OUT  → the world from above: terrain, woods, fields, and a city of ROOFS.
//   • zoom IN   → each city roof LIFTS to its floor-plan (interiors' BSP rooms).
//   • zoom IN+  → the FURNITURE (beds, sofas, shop shelving, warehouse pallets).
//   • F         → force every roof off at once (the dollhouse / X-ray look).
//
// The reveal is pure LEVEL-OF-DETAIL: a roof lifts only when its building is big
// enough ON SCREEN to read, and the floor-plan is GENERATED LAZILY — the wide world
// never shows a mush of beds, and far buildings cost only a roof rect.
//
// ── seams (inherited from both parents) ─────────────────────────────────────────
// 1. Pure fn of a hash-seed: one block → its lots, one footprint → its whole plan;
//    every scattered tree is hash2(cell,seed) — no grown/accumulated sets.
// 2. Draw == query: room_at/solid_at is the renderer run as a collision query.
// 3. LOD gates DRAWING + GENERATING — far content stays cheap.
// ============================================================================

static int lf_seed = 0;

static unsigned hash2(int a, int b) {
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static unsigned hmix(unsigned h, int k) { return hash2((int)h, k * 2654435761u + 0x9e3779b9u); }
static int ifloordiv(int a, int b) { int q = a / b; if ((a % b) != 0 && ((a < 0) != (b < 0))) q--; return q; }
static int posmod(int a, int b) { int m = a % b; return m < 0 ? m + b : m; }
static float frac01(unsigned h) { return (h & 4095u) / 4095.0f; }

typedef struct { int x, y, w, h; } Rect;

// ════════════════════════════════════════════════════════════════════════════
//  FIELDS — terrain, biome cover, density (lotfill's, scaled to the bigger world)
// ════════════════════════════════════════════════════════════════════════════
#define ELEV_FREQ  0.00042f
#define ELEV_SEA   0.30f
#define ELEV_SAND  0.345f
#define ELEV_ROCK  0.82f
#define BIO_FREQ   0.00052f
#define BIO_WFREQ  0.00036f
#define BIO_WAMP   420.0f
#define DENS_FREQ  0.0028f

enum { CV_BARE, CV_GRASS, CV_MEADOW, CV_FOREST, CV_WATER, CV_SAND, CV_ROCK, CV_N };
static const char *CV_NAME[CV_N] = { "bare", "grass", "meadow", "forest", "water", "sand", "rock" };
typedef struct { int kind; float dens; } Cover;

static void bio_warp(float x, float y, float *ox, float *oy) {
    float z = (float)lf_seed;
    *ox = x + (noise3(x * BIO_WFREQ, y * BIO_WFREQ, z + 11.0f) - 0.5f) * 2.0f * BIO_WAMP;
    *oy = y + (noise3(x * BIO_WFREQ, y * BIO_WFREQ, z + 19.0f) - 0.5f) * 2.0f * BIO_WAMP;
}
static float bio_raw(float x, float y) {
    float z = (float)lf_seed;
    float a = noise3(x * BIO_FREQ,        y * BIO_FREQ,        z);
    float b = noise3(x * BIO_FREQ * 2.3f, y * BIO_FREQ * 2.3f, z + 3.0f);
    float c = noise3(x * BIO_FREQ * 5.1f, y * BIO_FREQ * 5.1f, z + 7.0f);
    return clamp(a * 0.6f + b * 0.28f + c * 0.12f, 0.0f, 1.0f);
}
static float elev_raw(float x, float y) {
    float z = (float)lf_seed;
    float a = noise3(x * ELEV_FREQ,        y * ELEV_FREQ,        z + 50.0f);
    float b = noise3(x * ELEV_FREQ * 2.4f, y * ELEV_FREQ * 2.4f, z + 57.0f);
    return clamp(a * 0.7f + b * 0.3f, 0.0f, 1.0f);
}
static Cover cover_at(float x, float y) {
    float e = elev_raw(x, y);
    float d = clamp(noise3(x * DENS_FREQ, y * DENS_FREQ, (float)lf_seed + 5.0f), 0.0f, 1.0f);
    int kind;
    if      (e < ELEV_SEA)  kind = CV_WATER;
    else if (e < ELEV_SAND) kind = CV_SAND;
    else if (e > ELEV_ROCK) kind = CV_ROCK;
    else { float wx, wy; bio_warp(x, y, &wx, &wy); float b = bio_raw(wx, wy);
           kind = b < 0.34f ? CV_BARE : b < 0.46f ? CV_GRASS : b < 0.60f ? CV_MEADOW : CV_FOREST; }
    return (Cover){ kind, d };
}
static int ground_col(int cx, int cy, const Cover *cv) {
    unsigned h = hash2(cx * 7 + lf_seed * 911, cy * 13);
    switch (cv->kind) {
        case CV_BARE:   return (h & 1) ? CLR_DARK_BROWN    : CLR_BROWN;
        case CV_GRASS:  return (h & 3) ? CLR_DARK_GREEN    : CLR_MEDIUM_GREEN;
        case CV_MEADOW: return (h & 1) ? CLR_MEDIUM_GREEN  : CLR_LIME_GREEN;
        case CV_WATER:  return (h & 3) ? CLR_DARK_BLUE     : CLR_BLUE;
        case CV_SAND:   return (h & 1) ? CLR_LIGHT_PEACH   : CLR_PEACH;
        case CV_ROCK:   return (h % 5 == 0) ? CLR_LIGHT_GREY : CLR_DARK_GREY;
        default: { unsigned f = h % 9; return f == 0 ? CLR_BROWNISH_BLACK : f < 3 ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN; }
    }
}

// ── the WORLD selector: terrain stays wild; urbanization picks farm/city on land ──
#define URB_FREQ 0.0013f
enum { WK_WILD, WK_FARM, WK_CITY };
static const char *WK_NAME[3] = { "wild", "farmland", "city" };
static int world_kind_at(float x, float y) {
    int t = cover_at(x, y).kind;
    if (t == CV_WATER || t == CV_SAND || t == CV_ROCK) return WK_WILD;
    float u = noise3(x * URB_FREQ, y * URB_FREQ, (float)lf_seed + 200.0f);
    if (u > 0.63f) return WK_CITY;
    if (u > 0.43f) return WK_FARM;
    return WK_WILD;
}

// ── city sub-fields: building zone, lot fineness, massing ────────────────────────
enum { ZN_RES, ZN_COM, ZN_IND, ZN_N };
static const char *ZN_NAME[ZN_N] = { "residential", "commercial", "industrial" };
static int zone_at(float x, float y) {
    float i = noise3(x * 0.00075f, y * 0.00075f, (float)lf_seed + 90.0f);
    return i < 0.48f ? ZN_RES : i < 0.76f ? ZN_COM : ZN_IND;
}
static float urban_density_at(float x, float y) {
    float z = (float)lf_seed;
    float a = noise3(x * 0.0024f,        y * 0.0024f,        z + 500.0f);
    float b = noise3(x * 0.0024f * 2.5f, y * 0.0024f * 2.5f, z + 517.0f);
    return clamp(a * 0.62f + b * 0.38f, 0.0f, 1.0f);
}
static int massing_at(float x, float y, int zone, unsigned h) {
    float v = urban_density_at(x, y) + (frac01(h >> 9) - 0.5f) * 0.25f;
    int base = (zone == ZN_IND) ? 1 : (zone == ZN_COM) ? 1 : 0;
    int bump = v > 0.78f ? 2 : v > 0.50f ? 1 : 0;
    int m = base + bump; return m > 3 ? 3 : m;
}

// ── decay / fertility (the RUINS modifier's dials, lotfill's, scaled) ────────────
// decay 0→1 pristine→razed; fertility 0→1 barren→lush. A derelict district is just a
// trough of high decay; a ruin in a fertile trough gets swallowed green.
#define DECAY_FREQ 0.0016f
#define FERT_FREQ  0.0010f
static float decay_at(float x, float y) {
    float z = (float)lf_seed;
    float a = noise3(x * DECAY_FREQ,        y * DECAY_FREQ,        z + 300.0f);
    float b = noise3(x * DECAY_FREQ * 2.6f, y * DECAY_FREQ * 2.6f, z + 311.0f);
    return clamp(a * 0.7f + b * 0.3f, 0.0f, 1.0f);
}
static float fertility_at(float x, float y) { return clamp(noise3(x * FERT_FREQ, y * FERT_FREQ, (float)lf_seed + 400.0f), 0.0f, 1.0f); }

// MODIFIER collapse — punch the roof/massing open to foundation + spill rubble (reads
// phase-4 output, transforms it). MODIFIER overgrow — creep growth into the wreckage,
// scaled by decay × fertility. Both pure fns of (body, hash, fields); run in order.
#define RUIN_TH 0.58f       // only deep-decay troughs go derelict (ruins = districts, not universal)
static void collapse_fill(Rect body, unsigned h, float decay) {
    if (decay < RUIN_TH) return;
    float d = (decay - RUIN_TH) / (1.0f - RUIN_TH);
    int bites = 1 + (int)(d * 5.0f);
    for (int i = 0; i < bites; i++) {
        unsigned bh = hash2((int)h + i * 131, (int)(h >> 7) + i * 977);
        int bw = 1 + (int)(frac01(bh)      * (body.w * 0.5f));
        int bhh= 1 + (int)(frac01(bh >> 8) * (body.h * 0.5f));
        int bx = body.x + (int)(frac01(bh >> 16) * (body.w - bw));
        int by = body.y + (int)(frac01(bh >> 22) * (body.h - bhh));
        rectfill(bx, by, bw, bhh, CLR_BROWNISH_BLACK);
    }
    int rub = (int)(d * 16.0f);
    for (int i = 0; i < rub; i++) {
        unsigned rh = hash2((int)h * 7 + i, (int)(h >> 3) - i * 53);
        int rx = body.x - 2 + (int)(frac01(rh)       * (body.w + 4));
        int ry = body.y - 2 + (int)(frac01(rh >> 11) * (body.h + 4));
        pset(rx, ry, (rh & 1) ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
    }
}
static void overgrow_fill(Rect body, unsigned h, float decay, float fert) {
    float g = decay * (0.30f + 0.70f * fert);
    if (g < 0.08f) return;
    int n = (int)(g * body.w * body.h * 0.09f); if (n > 80) n = 80;
    for (int i = 0; i < n; i++) {
        unsigned gh = hash2((int)h * 13 + i * 31, (int)(h >> 5) + i * 17);
        int gx = body.x + (int)(frac01(gh)       * body.w);
        int gy = body.y + (int)(frac01(gh >> 11) * body.h);
        pset(gx, gy, (gh & 3) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN);
        if ((gh & 7) == 0) pset(gx, gy - 1, CLR_LIME_GREEN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  SCATTER — lotfill's continuous-cover wilderness (flavors + instance art)
// ════════════════════════════════════════════════════════════════════════════
#define CANOPY_P 18
#define GROUND_P 7
#define CLUMP_P  52

static void draw_tree(int x, int y, unsigned h, int big) {
    static const int TR[][3] = {
        { CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN },
        { CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN },
        { CLR_BLUE_GREEN, CLR_DARK_GREEN,   CLR_MEDIUM_GREEN },
        { CLR_DARK_BROWN, CLR_ORANGE,       CLR_YELLOW },
    };
    unsigned v = h % 8;
    const int *t = TR[v < 5 ? 0 : v < 7 ? 2 : 3];
    int r = (big ? 5 : 3) + (int)((h >> 6) & 1);
    rectfill(x, y - 1, 1 + (big ? 1 : 0), 4, CLR_DARK_BROWN);
    circfill(x, y - r, r, t[0]);
    circfill(x - 1, y - r - 1, r - 1, t[1]);
    circfill(x - 2, y - r - 2, (r >= 4 ? 2 : 1), t[2]);
}
static void draw_bush(int x, int y, unsigned h) { circfill(x, y, 2, (h & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN); pset(x - 1, y - 1, CLR_LIME_GREEN); }
static void draw_tuft(int x, int y, unsigned h) { int c = (h & 1) ? CLR_MEDIUM_GREEN : CLR_LIME_GREEN; line(x, y, x, y - 2, c); line(x - 1, y, x - 2, y - 2, c); line(x + 1, y, x + 2, y - 2, c); }
static void draw_flower(int x, int y, int col) { pset(x, y, CLR_MEDIUM_GREEN); pset(x, y - 2, col); }
static int  flower_col(int cx, int cy) {
    static const int FC[6] = { CLR_RED, CLR_YELLOW, CLR_WHITE, CLR_ORANGE, CLR_PINK, CLR_MAUVE };
    unsigned h = hash2(ifloordiv(cx * CANOPY_P, CLUMP_P) + lf_seed * 31, ifloordiv(cy * CANOPY_P, CLUMP_P));
    return FC[h % 6];
}
static void draw_palm(int x, int y, unsigned h) {
    rectfill(x, y - 6, 1, 7, CLR_DARK_BROWN);
    int c = (h & 1) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN;
    for (int a = -2; a <= 2; a++) line(x, y - 6, x + a * 2, y - 6 - (2 - (a < 0 ? -a : a)), c);
    pset(x, y - 7, CLR_LIME_GREEN);
}
static void draw_cactus(int x, int y, unsigned h) {
    int hh = 4 + (int)((h >> 4) & 3);
    rectfill(x, y - hh, 2, hh, CLR_DARK_GREEN);
    if (h & 1) { rectfill(x - 2, y - hh + 1, 2, 2, CLR_DARK_GREEN); rectfill(x - 2, y - hh - 1, 1, 3, CLR_DARK_GREEN); }
    pset(x, y - hh, CLR_MEDIUM_GREEN);
}
static void draw_deadtree(int x, int y, unsigned h) {
    line(x, y, x, y - 6, CLR_DARK_BROWN);
    line(x, y - 4, x - 2, y - 6, CLR_DARK_BROWN); line(x, y - 5, x + 2, y - 7, CLR_DARK_BROWN);
    pset(x, y - 6, CLR_MEDIUM_GREY);
}
static void draw_rock(int x, int y, unsigned h) { int r = 2 + (int)((h >> 5) & 1); circfill(x, y, r, CLR_DARK_GREY); pset(x - 1, y - 1, CLR_LIGHT_GREY); }

enum { I_NONE, I_TREE, I_PALM, I_CACTUS, I_DEADTREE, I_ROCK, I_BUSH, I_TUFT, I_FLOWER };
static void draw_inst(int type, int x, int y, unsigned h, int big) {
    switch (type) {
        case I_TREE:     draw_tree(x, y, h, big); break;
        case I_PALM:     draw_palm(x, y, h);      break;
        case I_CACTUS:   draw_cactus(x, y, h);    break;
        case I_DEADTREE: draw_deadtree(x, y, h);  break;
        case I_ROCK:     draw_rock(x, y, h);      break;
        case I_BUSH:     draw_bush(x, y, h);      break;
        case I_TUFT:     draw_tuft(x, y, h);      break;
    }
}
typedef struct { int canopy; float c_base, c_dens; int ground; float g_base, g_dens; float big_at; } Flavor;
enum { FV_NONE, FV_WOODLAND, FV_MEADOW, FV_GRASS, FV_BEACH, FV_SCREE, FV_WASTE, FV_N };
static const Flavor FLAVORS[FV_N] = {
    [FV_NONE]     = { I_NONE,     0,0,         I_NONE,   0,0,        -1 },
    [FV_WOODLAND] = { I_TREE,     0.25f,0.70f, I_BUSH,   0.12f,0.25f, 0.55f },
    [FV_MEADOW]   = { I_TREE,     0.04f,0,     I_FLOWER, 0.10f,0.55f, -1 },
    [FV_GRASS]    = { I_NONE,     0,0,         I_TUFT,   0.18f,0,     -1 },
    [FV_BEACH]    = { I_PALM,     0.03f,0,     I_TUFT,   0.05f,0,     -1 },
    [FV_SCREE]    = { I_ROCK,     0.10f,0,     I_ROCK,   0.06f,0,     -1 },
    [FV_WASTE]    = { I_DEADTREE, 0.05f,0,     I_BUSH,   0.05f,0,     -1 },
};
static int flavor_for(int cover_kind) {
    switch (cover_kind) {
        case CV_FOREST: return FV_WOODLAND;  case CV_MEADOW: return FV_MEADOW;
        case CV_GRASS:  return FV_GRASS;     case CV_SAND:   return FV_BEACH;
        case CV_ROCK:   return FV_SCREE;     case CV_BARE:   return FV_WASTE;
        default:        return FV_NONE;
    }
}
static int scatter_canopy(int wx, int wy, unsigned h) {
    Cover cv = cover_at(wx, wy);
    const Flavor *f = &FLAVORS[flavor_for(cv.kind)];
    if (f->canopy == I_NONE || frac01(h >> 20) >= f->c_base + cv.dens * f->c_dens) return 0;
    draw_inst(f->canopy, wx, wy, h, f->big_at >= 0 && cv.dens > f->big_at);
    return 1;
}
static int scatter_ground(int cxi, int cyi, int wx, int wy, unsigned h) {
    Cover cv = cover_at(wx, wy);
    const Flavor *f = &FLAVORS[flavor_for(cv.kind)];
    if (f->ground == I_NONE || frac01(h >> 20) >= f->g_base + cv.dens * f->g_dens) return 0;
    if (f->ground == I_FLOWER) draw_flower(wx, wy, flower_col(cxi, cyi));
    else draw_inst(f->ground, wx, wy, h, 0);
    return 1;
}

// ════════════════════════════════════════════════════════════════════════════
//  ROWS — lotfill's bounded farmland parcels (crops + hedgerow)
// ════════════════════════════════════════════════════════════════════════════
#define FIELD_P 96
enum { CR_PLOUGH, CR_CORN, CR_WHEAT, CR_VINE, CR_N };
static const char *CR_NAME[CR_N] = { "plough", "corn", "wheat", "vineyard" };
static const int   CR_SOIL[CR_N] = { CLR_BROWN, CLR_DARK_BROWN, CLR_ORANGE, CLR_DARK_GREEN };
static const int   CR_ROW [CR_N] = { CLR_DARK_BROWN, CLR_MEDIUM_GREEN, CLR_YELLOW, CLR_BLUE_GREEN };
static const int   CR_SP  [CR_N] = { 6, 5, 4, 9 };
static void rows_fill(Rect r, unsigned h, bool soil, bool crop, bool border) {
    int cr = h % CR_N; bool vert = (h >> 4) & 1; int sp = CR_SP[cr], m = 4;
    int ix = r.x + m, iy = r.y + m, iw = r.w - 2 * m, ih = r.h - 2 * m;
    if (iw < 2 || ih < 2) return;
    if (soil) rectfill(r.x, r.y, r.w, r.h, CR_SOIL[cr]);
    if (crop) {
        if (vert) for (int x = ix + (int)(h % sp); x < ix + iw; x += sp)
                      if (cr == CR_VINE) for (int y = iy; y < iy + ih; y += 5) pset(x, y, CR_ROW[cr]);
                      else line(x, iy, x, iy + ih - 1, CR_ROW[cr]);
        else      for (int y = iy + (int)(h % sp); y < iy + ih; y += sp)
                      if (cr == CR_VINE) for (int x = ix; x < ix + iw; x += 5) pset(x, y, CR_ROW[cr]);
                      else line(ix, y, ix + iw - 1, y, CR_ROW[cr]);
    }
    if (border) {
        for (int x = r.x; x < r.x + r.w; x++) { int c = ((x + r.y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN; pset(x, r.y, c); pset(x, r.y + r.h - 1, c); }
        for (int y = r.y; y < r.y + r.h; y++) { int c = ((r.x + y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN; pset(r.x, y, c); pset(r.x + r.w - 1, y, c); }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  CITY — block → ring-of-varied-lots subdivision (lotfill, scaled up)
// ════════════════════════════════════════════════════════════════════════════
#define BLK_P 230
#define ST_W  12

typedef struct { Rect lot; unsigned hash; unsigned char outward, attached, massing; } LotSlot;

static int run_split(int L, int target_w, unsigned h, int *bnd) {
    int k = target_w > 0 ? L / target_w : 1;
    if (k < 1) k = 1; if (k > 6) k = 6;
    float w[6], sum = 0;
    for (int i = 0; i < k; i++) { w[i] = 0.6f + 1.5f * frac01(h >> (i * 3)); sum += w[i]; }
    float acc = 0; bnd[0] = 0;
    for (int i = 0; i < k; i++) { acc += w[i] / sum * (float)L; bnd[i + 1] = (int)(acc + 0.5f); }
    bnd[k] = L; return k;
}
static int block_lots(int bx, int by, int zone, LotSlot *out, int cap) {
    int ox = bx * BLK_P, oy = by * BLK_P, inx = ox + ST_W, iny = oy + ST_W, IN = BLK_P - ST_W;
    unsigned bh = hash2(bx * 131 + lf_seed * 7, by * 89 + 5);
    float ud = urban_density_at(inx + IN / 2.0f, iny + IN / 2.0f);
    int D  = 50 + (int)(frac01(bh >> 3) * 26) + (zone == ZN_IND ? 8 : 0);
    if (D > IN / 2 - 6) D = IN / 2 - 6;
    int tw = (int)(88.0f - 34.0f * ud) + (zone == ZN_IND ? 16 : 0);
    int attach_res = (zone == ZN_RES && ud > 0.50f);
    int n = 0, bnd[7];
    for (int s = 0; s < 4 && n < cap; s++) {
        int horiz = (s < 2), runL = horiz ? IN : (IN - 2 * D);
        if (runL < 16) continue;
        unsigned sh = hash2((int)bh + s * 1009, (int)(bh >> 7) + s * 17);
        int k = run_split(runL, tw, sh, bnd);
        for (int i = 0; i < k && n < cap; i++) {
            int a0 = bnd[i], lw = bnd[i + 1] - a0;
            if (lw < 16) continue;
            Rect r; int outward;
            if      (s == 0) { r = (Rect){ inx + a0,     iny,          lw, D }; outward = 0; }
            else if (s == 1) { r = (Rect){ inx + a0,     iny + IN - D, lw, D }; outward = 2; }
            else if (s == 2) { r = (Rect){ inx,          iny + D + a0, D, lw }; outward = 3; }
            else             { r = (Rect){ inx + IN - D, iny + D + a0, D, lw }; outward = 1; }
            unsigned lh = hash2((int)sh + i * 2399, (int)(sh >> 5) - i * 41);
            out[n].lot = r; out[n].hash = lh; out[n].outward = (unsigned char)outward;
            out[n].attached = (unsigned char)(attach_res && lw <= 64);
            out[n].massing  = (unsigned char)massing_at(r.x + r.w / 2.0f, r.y + r.h / 2.0f, zone, lh);
            n++;
        }
    }
    return n;
}
static int footprint_body(Rect lot, int outward, int zone, int attached, Rect *out) {
    int side = attached ? 0 : (zone == ZN_IND) ? 3 : 4;
    int back = (zone == ZN_IND) ? 3 : 4;
    int fs   = (zone == ZN_RES) ? (attached ? 5 : 12) : (zone == ZN_COM) ? 8 : 5;
    int ix = lot.x, iy = lot.y, iw = lot.w, ih = lot.h;
    switch (outward) {
        case 0: iy += fs;   ih -= fs + back; ix += side; iw -= 2 * side; break;
        case 2: iy += back; ih -= fs + back; ix += side; iw -= 2 * side; break;
        case 1: ix += back; iw -= fs + back; iy += side; ih -= 2 * side; break;
        case 3: ix += fs;   iw -= fs + back; iy += side; ih -= 2 * side; break;
    }
    if (iw < 6 || ih < 6) return 0;
    *out = (Rect){ ix, iy, iw, ih }; return 1;
}

// ════════════════════════════════════════════════════════════════════════════
//  INTERIOR — interiors' BSP floor-plan (one footprint → its rooms)
// ════════════════════════════════════════════════════════════════════════════
enum { RM_HALL, RM_LIVING, RM_BED, RM_KITCHEN, RM_BATH,
       RM_SHOP, RM_BACK, RM_OFFICE, RM_BAY, RM_STORE, RM_N };
static const char *RM_NAME[RM_N] = { "hall", "living", "bedroom", "kitchen", "bath",
                                     "shop floor", "back-of-house", "office", "bay", "storage" };
static const int RM_FLOOR[RM_N] = {
    [RM_HALL]    = CLR_DARK_BROWN,  [RM_LIVING] = CLR_BROWN,      [RM_BED]   = CLR_DARK_PURPLE,
    [RM_KITCHEN] = CLR_DARK_GREY,   [RM_BATH]   = CLR_BLUE_GREEN, [RM_SHOP]  = CLR_LIGHT_GREY,
    [RM_BACK]    = CLR_DARKER_GREY, [RM_OFFICE] = CLR_MEDIUM_GREY,[RM_BAY]   = CLR_DARKER_GREY,
    [RM_STORE]   = CLR_DARK_BROWN,
};
enum { T_N = 1, T_E = 2, T_S = 4, T_W = 8 };
typedef struct { Rect r; int label; int touch; } Room;
typedef struct { int x, y, len, vert, dpos, dlen; } Wall;
#define MAX_ROOMS 32
#define MAX_WALLS 32
typedef struct { Rect shell; int type, outward; Room room[MAX_ROOMS]; int nroom; Wall wall[MAX_WALLS]; int nwall; int egx, egy, entry_room; } Plan;
typedef struct { int min_w, min_h, maxdepth; float bias_lo, bias_hi; } SplitP;
static const SplitP SPLIT[ZN_N] = {
    [ZN_RES] = { 13, 13, 4, 0.34f, 0.66f },
    [ZN_COM] = { 22, 22, 2, 0.34f, 0.66f },
    [ZN_IND] = { 26, 26, 1, 0.70f, 0.84f },
};
static void emit_room(Plan *p, Rect r) {
    if (p->nroom >= MAX_ROOMS) return;
    int t = 0;
    if (r.y <= p->shell.y + 1)                    t |= T_N;
    if (r.y + r.h >= p->shell.y + p->shell.h - 1) t |= T_S;
    if (r.x <= p->shell.x + 1)                    t |= T_W;
    if (r.x + r.w >= p->shell.x + p->shell.w - 1) t |= T_E;
    p->room[p->nroom++] = (Room){ r, RM_BED, t };
}
static void bsp(Plan *p, Rect r, unsigned h, int depth, const SplitP *sp) {
    int canV = r.w >= 2 * sp->min_w, canH = r.h >= 2 * sp->min_h;
    int leaf = (!canV && !canH) || depth >= sp->maxdepth || p->nroom >= MAX_ROOMS - 2;
    if (!leaf && depth > 0 && frac01(hmix(h, 91)) < 0.08f + 0.13f * depth) leaf = 1;
    if (leaf) { emit_room(p, r); return; }
    int vert;
    if (canV && !canH)      vert = 1;
    else if (canH && !canV) vert = 0;
    else { vert = (r.w >= r.h); if (frac01(hmix(h, 55)) < 0.22f) vert = !vert; }
    float bias = sp->bias_lo + frac01(hmix(h, 7)) * (sp->bias_hi - sp->bias_lo);
    if (vert) {
        int lo = r.x + sp->min_w, hi = r.x + r.w - sp->min_w;
        int s = r.x + (int)(bias * r.w); if (s < lo) s = lo; if (s > hi) s = hi;
        int dlen = 9, span = r.h - 2 * 4 - dlen, dpos = r.y + 4 + (span > 0 ? (int)(frac01(hmix(h, 13)) * span) : 0);
        if (p->nwall < MAX_WALLS) p->wall[p->nwall++] = (Wall){ s, r.y, r.h, 1, dpos, dlen };
        bsp(p, (Rect){ r.x, r.y, s - r.x, r.h }, hmix(h, 1), depth + 1, sp);
        bsp(p, (Rect){ s, r.y, r.x + r.w - s, r.h }, hmix(h, 2), depth + 1, sp);
    } else {
        int lo = r.y + sp->min_h, hi = r.y + r.h - sp->min_h;
        int s = r.y + (int)(bias * r.h); if (s < lo) s = lo; if (s > hi) s = hi;
        int dlen = 9, span = r.w - 2 * 4 - dlen, dpos = r.x + 4 + (span > 0 ? (int)(frac01(hmix(h, 13)) * span) : 0);
        if (p->nwall < MAX_WALLS) p->wall[p->nwall++] = (Wall){ r.x, s, r.w, 0, dpos, dlen };
        bsp(p, (Rect){ r.x, r.y, r.w, s - r.y }, hmix(h, 1), depth + 1, sp);
        bsp(p, (Rect){ r.x, s, r.w, r.y + r.h - s }, hmix(h, 2), depth + 1, sp);
    }
}
static int front_bit(int outward) { return outward == 0 ? T_N : outward == 1 ? T_E : outward == 2 ? T_S : T_W; }
static void assign_labels(Plan *p) {
    int n = p->nroom; if (n == 0) return;
    int fb = front_bit(p->outward); long tot = 0;
    for (int i = 0; i < n; i++) { p->room[i].label = RM_BED; tot += (long)p->room[i].r.w * p->room[i].r.h; }
    long avg = tot / n; int big = -1, small = -1;
    for (int i = 0; i < n; i++) {
        long a = (long)p->room[i].r.w * p->room[i].r.h;
        if (big < 0   || a > (long)p->room[big].r.w   * p->room[big].r.h)   big = i;
        if (small < 0 || a < (long)p->room[small].r.w * p->room[small].r.h) small = i;
    }
    int front = -1;
    for (int i = 0; i < n; i++) if (p->room[i].touch & fb)
        if (front < 0 || (long)p->room[i].r.w * p->room[i].r.h > (long)p->room[front].r.w * p->room[front].r.h) front = i;
    if (p->type == ZN_RES) {
        int liv = (front >= 0) ? front : big; p->room[liv].label = RM_LIVING;
        if (n > 1 && small != liv) p->room[small].label = RM_BATH;
        int ki = -1;
        for (int i = 0; i < n; i++) { if (i == liv || p->room[i].label == RM_BATH || !p->room[i].touch) continue;
            if (ki < 0 || (long)p->room[i].r.w * p->room[i].r.h > (long)p->room[ki].r.w * p->room[ki].r.h) ki = i; }
        if (ki >= 0) p->room[ki].label = RM_KITCHEN;
        int e = p->entry_room;
        if (e >= 0 && e != liv && p->room[e].label == RM_BED && (long)p->room[e].r.w * p->room[e].r.h < avg) p->room[e].label = RM_HALL;
    } else if (p->type == ZN_COM) {
        for (int i = 0; i < n; i++) p->room[i].label = RM_BACK;
        int shop = (front >= 0) ? front : big; p->room[shop].label = RM_SHOP;
        if (n > 1 && small != shop) p->room[small].label = RM_OFFICE;
    } else {
        for (int i = 0; i < n; i++) p->room[i].label = RM_STORE;
        p->room[big].label = RM_BAY;
        if (n > 1 && small != big) p->room[small].label = RM_OFFICE;
    }
}
static void plan_build(Plan *p, Rect shell, int type, int outward, unsigned h) {
    p->shell = shell; p->type = type; p->outward = outward; p->nroom = p->nwall = 0; p->entry_room = -1;
    Rect interior = { shell.x + 1, shell.y + 1, shell.w - 2, shell.h - 2 };
    if (interior.w < 4 || interior.h < 4) emit_room(p, interior);
    else bsp(p, interior, h, 0, &SPLIT[type]);
    int cx = shell.x + shell.w / 2, cy = shell.y + shell.h / 2, ix = cx, iy = cy;
    switch (outward) {
        case 0: p->egx = cx; p->egy = shell.y;               ix = cx; iy = shell.y + 2;            break;
        case 1: p->egx = shell.x + shell.w - 1; p->egy = cy; ix = shell.x + shell.w - 3; iy = cy;  break;
        case 2: p->egx = cx; p->egy = shell.y + shell.h - 1; ix = cx; iy = shell.y + shell.h - 3;  break;
        default:p->egx = shell.x; p->egy = cy;               ix = shell.x + 2; iy = cy;            break;
    }
    for (int i = 0; i < p->nroom; i++) { Rect r = p->room[i].r;
        if (ix >= r.x && ix < r.x + r.w && iy >= r.y && iy < r.y + r.h) { p->entry_room = i; break; } }
    assign_labels(p);
}
static int room_at(const Plan *p, int x, int y) {
    for (int i = 0; i < p->nroom; i++) { Rect r = p->room[i].r;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i; }
    return -1;
}
#define WALL_EXT CLR_BROWNISH_BLACK
#define WALL_INT CLR_DARK_GREY
#define WIN_COL  CLR_BLUE
#define DOOR_COL CLR_ORANGE
#define SLAB_COL CLR_DARKER_GREY
typedef struct { bool floors, walls, win, doors, props; } Show;
static void edge(int x0, int y0, int len, int vert, int gap0, int gaplen, int col) {
    for (int i = 0; i < len; i++) { int gx = vert ? y0 + i : x0 + i;
        if (gaplen > 0 && gx >= gap0 && gx < gap0 + gaplen) continue;
        if (vert) pset(x0, y0 + i, col); else pset(x0 + i, y0, col); }
}
static void furnish(const Room *rm) {
    Rect r = rm->r; int ix = r.x + 2, iy = r.y + 2, iw = r.w - 4, ih = r.h - 4;
    if (iw < 4 || ih < 4) return;
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    switch (rm->label) {
        case RM_BED:    rectfill(ix, iy, iw * 3 / 5, ih / 2, CLR_INDIGO); rectfill(ix + 1, iy + 1, (iw * 3 / 5) - 2, 2, CLR_LIGHT_PEACH); break;
        case RM_LIVING: rectfill(ix, iy + ih - 4, iw * 2 / 3, 3, CLR_DARK_RED); rectfill(cx - 3, cy, 6, 3, CLR_DARK_BROWN); break;
        case RM_KITCHEN:rectfill(ix, iy, iw, 2, CLR_LIGHT_GREY); rectfill(ix, iy, 2, ih, CLR_LIGHT_GREY); rectfill(ix + iw - 3, iy + 1, 2, 2, CLR_DARK_GREY); break;
        case RM_BATH:   rectfill(ix, iy, iw / 2 + 1, 3, CLR_BLUE); circfill(ix + iw - 2, iy + ih - 2, 1, CLR_WHITE); break;
        case RM_SHOP:   for (int yy = iy + 1; yy < iy + ih - 3; yy += 3) line(ix, yy, ix + iw - 1, yy, CLR_DARK_BROWN); rectfill(ix, iy + ih - 2, iw, 2, CLR_BROWN); break;
        case RM_OFFICE: rectfill(ix, iy, iw * 2 / 3, 3, CLR_DARK_BROWN); rectfill(ix + 1, iy + 4, 3, 3, CLR_DARK_GREY); break;
        case RM_BAY:    for (int yy = iy + 2; yy < iy + ih - 3; yy += 7) for (int xx = ix + 2; xx < ix + iw - 3; xx += 8) { if (hash2(xx, yy + lf_seed) & 1) continue; rectfill(xx, yy, 4, 4, CLR_BROWN); rect(xx, yy, 4, 4, CLR_DARK_BROWN); } break;
        case RM_STORE: case RM_BACK: for (int yy = iy + 1; yy < iy + ih - 1; yy += 3) line(ix, yy, ix + iw - 1, yy, CLR_DARK_BROWN); break;
        case RM_HALL:   rectfill(cx - 1, iy, 3, ih, CLR_DARK_RED); break;
    }
}
static void plan_draw(const Plan *p, Show s) {
    Rect sh = p->shell;
    rectfill(sh.x + 1, sh.y + 1, sh.w - 2, sh.h - 2, SLAB_COL);
    if (s.floors) for (int i = 0; i < p->nroom; i++) { Rect r = p->room[i].r; rectfill(r.x, r.y, r.w, r.h, RM_FLOOR[p->room[i].label]); }
    if (s.props) for (int i = 0; i < p->nroom; i++) furnish(&p->room[i]);
    if (s.walls) for (int i = 0; i < p->nwall; i++) { Wall w = p->wall[i];
        if (w.vert) { for (int y = w.y; y < w.y + w.len; y++) if (!(y >= w.dpos && y < w.dpos + w.dlen)) pset(w.x, y, WALL_INT); }
        else        { for (int x = w.x; x < w.x + w.len; x++) if (!(x >= w.dpos && x < w.dpos + w.dlen)) pset(x, w.y, WALL_INT); } }
    if (s.doors) for (int i = 0; i < p->nwall; i++) { Wall w = p->wall[i];
        if (w.vert) { pset(w.x, w.dpos, DOOR_COL); pset(w.x, w.dpos + w.dlen - 1, DOOR_COL); }
        else        { pset(w.dpos, w.y, DOOR_COL); pset(w.dpos + w.dlen - 1, w.y, DOOR_COL); } }
    int gx = (p->outward == 0 || p->outward == 2) ? p->egx - 5 : 0;
    int gy = (p->outward == 1 || p->outward == 3) ? p->egy - 5 : 0; int gl = 10;
    edge(sh.x, sh.y, sh.w, 0, p->outward == 0 ? gx : -1, p->outward == 0 ? gl : 0, WALL_EXT);
    edge(sh.x, sh.y + sh.h - 1, sh.w, 0, p->outward == 2 ? gx : -1, p->outward == 2 ? gl : 0, WALL_EXT);
    edge(sh.x, sh.y, sh.h, 1, p->outward == 3 ? gy : -1, p->outward == 3 ? gl : 0, WALL_EXT);
    edge(sh.x + sh.w - 1, sh.y, sh.h, 1, p->outward == 1 ? gy : -1, p->outward == 1 ? gl : 0, WALL_EXT);
    if (s.win) {
        for (int x = sh.x + 6; x < sh.x + sh.w - 6; x += 7) {
            if (!(p->outward == 0 && abs(x - p->egx) < 7)) { pset(x, sh.y, WIN_COL); pset(x + 1, sh.y, WIN_COL); }
            if (!(p->outward == 2 && abs(x - p->egx) < 7)) { pset(x, sh.y + sh.h - 1, WIN_COL); pset(x + 1, sh.y + sh.h - 1, WIN_COL); } }
        for (int y = sh.y + 6; y < sh.y + sh.h - 6; y += 7) {
            if (!(p->outward == 3 && abs(y - p->egy) < 7)) { pset(sh.x, y, WIN_COL); pset(sh.x, y + 1, WIN_COL); }
            if (!(p->outward == 1 && abs(y - p->egy) < 7)) { pset(sh.x + sh.w - 1, y, WIN_COL); pset(sh.x + sh.w - 1, y + 1, WIN_COL); } }
    }
}

// ── the roof (what you see at city zoom) + outdoor yard/driveway ──────────────────
static void draw_roof(Rect b, int zone, int massing, unsigned h) {
    static const int ROOF_RES [5] = { CLR_RED, CLR_DARK_RED, CLR_BROWN, CLR_PEACH, CLR_DARK_PURPLE };
    static const int ROOF_COM [4] = { CLR_DARK_GREY, CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_INDIGO };
    static const int ROOF_IND [3] = { CLR_DARKER_BLUE, CLR_DARKER_GREY, CLR_DARK_GREY };
    int col = zone == ZN_RES ? ROOF_RES[h % 5] : zone == ZN_COM ? ROOF_COM[h % 4] : ROOF_IND[h % 3];
    rectfill(b.x, b.y, b.w, b.h, col);
    rect(b.x, b.y, b.w, b.h, CLR_BROWNISH_BLACK);
    if (massing >= 1) {
        rectfill(b.x, b.y, b.w, 1, CLR_LIGHT_GREY);
        int vstep = massing >= 3 ? 3 : 4;
        for (int yy = b.y + 3; yy < b.y + b.h - 1; yy += vstep) for (int xx = b.x + 2; xx < b.x + b.w - 1; xx += 3) pset(xx, yy, CLR_DARKER_GREY);
    } else {
        if (b.w >= b.h) line(b.x + 1, b.y + b.h / 2, b.x + b.w - 2, b.y + b.h / 2, CLR_LIGHT_PEACH);
        else            line(b.x + b.w / 2, b.y + 1, b.x + b.w / 2, b.y + b.h - 2, CLR_LIGHT_PEACH);
    }
}
static void draw_outdoor(Rect lot, Rect b, int outward, int zone, int attached, unsigned h, bool detail) {
    int dc = CLR_MEDIUM_GREY, dx = b.x + b.w / 2, dy = b.y + b.h / 2;
    switch (outward) {
        case 0: line(dx, lot.y, dx, b.y, dc);                       break;
        case 2: line(dx, b.y + b.h - 1, dx, lot.y + lot.h - 1, dc); break;
        case 1: line(b.x + b.w - 1, dy, lot.x + lot.w - 1, dy, dc); break;
        case 3: line(lot.x, dy, b.x, dy, dc);                       break;
    }
    if (detail && zone == ZN_RES && !attached) {
        int yx, yy;
        switch (outward) {
            case 0:  yx = lot.x + 3 + (int)(frac01(h) * (lot.w > 6 ? lot.w - 6 : 1)); yy = lot.y + 5; break;
            case 2:  yx = lot.x + 3 + (int)(frac01(h) * (lot.w > 6 ? lot.w - 6 : 1)); yy = lot.y + lot.h - 1; break;
            case 1:  yx = lot.x + lot.w - 2; yy = lot.y + 3 + (int)(frac01(h) * (lot.h > 6 ? lot.h - 6 : 1)); break;
            default: yx = lot.x + 3;         yy = lot.y + 3 + (int)(frac01(h) * (lot.h > 6 ? lot.h - 6 : 1)); break;
        }
        draw_tree(yx, yy, h, 0);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  THE DRIVER — compose terrain → wild scatter → farm rows → city (roofs/plans)
// ════════════════════════════════════════════════════════════════════════════
static bool roof_off = false;       // F: force every roof lifted (dollhouse)
static bool dbg      = false;       // G: block + lot grid
static int  ovl      = 0;           // O: field overlay (0 off / 1 world-kind / 2 cover / 3 density)
static int  n_bld, n_open, n_ruin;  // HUD tallies

#define ROOF_LIFT_Z 0.75f
#define MIN_OPEN_PX 26

static void draw_city_block(int bx, int by, float zoom, bool want_peel, bool outdoor_detail) {
    int ox = bx * BLK_P, oy = by * BLK_P, inx = ox + ST_W, iny = oy + ST_W, IN = BLK_P - ST_W;
    int zone = zone_at(inx + IN / 2.0f, iny + IN / 2.0f);
    rectfill(ox, oy, BLK_P, BLK_P, CLR_DARK_GREY);
    rectfill(inx, iny, IN, IN, zone == ZN_RES ? CLR_DARK_GREEN : CLR_MEDIUM_GREY);
    LotSlot slots[40]; int nl = block_lots(bx, by, zone, slots, 40);
    for (int i = 0; i < nl; i++) {
        Rect lot = slots[i].lot, b; int outward = slots[i].outward, att = slots[i].attached;
        if (!footprint_body(lot, outward, zone, att, &b)) continue;
        n_bld++;
        draw_outdoor(lot, b, outward, zone, att, slots[i].hash, outdoor_detail);
        float fcx = b.x + b.w / 2.0f, fcy = b.y + b.h / 2.0f;
        float dec = decay_at(fcx, fcy), fert = fertility_at(fcx, fcy);
        bool peel = want_peel && (b.w * zoom >= MIN_OPEN_PX || b.h * zoom >= MIN_OPEN_PX);
        if (peel) {
            Plan p; plan_build(&p, b, zone, outward, hmix(slots[i].hash, 3));
            Show s = { .floors = true, .walls = true, .doors = zoom >= 0.95f, .win = zoom >= 0.95f, .props = zoom >= 1.5f };
            plan_draw(&p, s); n_open++;
            if (dbg) circ(p.egx, p.egy, 1, CLR_GREEN);
        } else {
            draw_roof(b, zone, slots[i].massing, slots[i].hash);
        }
        if (dec >= RUIN_TH) {                                // RUINS modifier pipeline (over roof OR plan)
            collapse_fill(b, slots[i].hash, dec);
            overgrow_fill(b, slots[i].hash, dec, fert);
            n_ruin++;
        }
    }
    if (dbg) { rect(ox, oy, BLK_P, BLK_P, CLR_DARK_RED);
        for (int i = 0; i < nl; i++) rect(slots[i].lot.x, slots[i].lot.y, slots[i].lot.w, slots[i].lot.h, CLR_YELLOW); }
}

static void tile(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - BLK_P, Rpx = (int)(cx + hwv) + BLK_P;
    int Tpx = (int)(cy - hhv) - BLK_P, Bpx = (int)(cy + hhv) + BLK_P;
    bool want_peel = roof_off || zoom >= ROOF_LIFT_Z;
    bool outdoor_detail = zoom >= 0.32f;
    n_bld = n_open = n_ruin = 0;

    // 1 — terrain ground tint everywhere (coarse cells that grow when zoomed out)
    int gc = 6, step = 1; while ((float)gc * step * zoom < 3.0f && step < 8) step++;
    int cs = gc * step;
    for (int gy = ifloordiv(Tpx, cs); gy <= ifloordiv(Bpx, cs); gy++)
        for (int gx = ifloordiv(Lpx, cs); gx <= ifloordiv(Rpx, cs); gx++) {
            int px = gx * cs, py = gy * cs; Cover cv = cover_at(px + cs / 2.0f, py + cs / 2.0f);
            rectfill(px, py, cs, cs, ground_col(gx, gy, &cv));
        }

    // 2 — WILD ground scatter (flowers / bushes / tufts), then canopy (trees) — gated to wild land
    if (zoom >= 0.6f)
        for (int cyi = ifloordiv(Tpx, GROUND_P); cyi <= ifloordiv(Bpx, GROUND_P); cyi++)
            for (int cxi = ifloordiv(Lpx, GROUND_P); cxi <= ifloordiv(Rpx, GROUND_P); cxi++) {
                unsigned h = hash2(cxi + lf_seed * 101, cyi * 3 + 7);
                int wx = cxi * GROUND_P + (int)(frac01(h) * GROUND_P), wy = cyi * GROUND_P + (int)(frac01(h >> 12) * GROUND_P);
                if (world_kind_at(wx, wy) == WK_WILD) scatter_ground(cxi, cyi, wx, wy, h);
            }
    if (zoom >= 0.30f)
        for (int cyi = ifloordiv(Tpx, CANOPY_P); cyi <= ifloordiv(Bpx, CANOPY_P); cyi++)
            for (int cxi = ifloordiv(Lpx, CANOPY_P); cxi <= ifloordiv(Rpx, CANOPY_P); cxi++) {
                unsigned h = hash2(cxi * 5 + lf_seed * 17, cyi + 3);
                int wx = cxi * CANOPY_P + (int)(frac01(h) * CANOPY_P), wy = cyi * CANOPY_P + (int)(frac01(h >> 12) * CANOPY_P);
                if (world_kind_at(wx, wy) == WK_WILD) scatter_canopy(wx, wy, h);
            }

    // 3 — FARMLAND rows
    bool crop = zoom >= 0.5f;
    for (int fy = ifloordiv(Tpx, FIELD_P); fy <= ifloordiv(Bpx, FIELD_P); fy++)
        for (int fx = ifloordiv(Lpx, FIELD_P); fx <= ifloordiv(Rpx, FIELD_P); fx++) {
            if (world_kind_at(fx * FIELD_P + FIELD_P / 2.0f, fy * FIELD_P + FIELD_P / 2.0f) != WK_FARM) continue;
            Rect r = { fx * FIELD_P, fy * FIELD_P, FIELD_P, FIELD_P };
            rows_fill(r, hash2(fx + lf_seed * 257, fy * 7 + 13), true, crop, true);
        }

    // 4 — CITY blocks (roofs far, floor-plans near)
    for (int by = ifloordiv(Tpx, BLK_P); by <= ifloordiv(Bpx, BLK_P); by++)
        for (int bx = ifloordiv(Lpx, BLK_P); bx <= ifloordiv(Rpx, BLK_P); bx++) {
            int ox = bx * BLK_P, oy = by * BLK_P, IN = BLK_P - ST_W;
            if (world_kind_at(ox + ST_W + IN / 2.0f, oy + ST_W + IN / 2.0f) != WK_CITY) continue;
            draw_city_block(bx, by, zoom, want_peel, outdoor_detail);
        }
}

// ── inspection overlay: the field that drives the world, as a tint ───────────────
static int dens_ramp(float d) { return d < 0.25f ? CLR_DARK_BLUE : d < 0.5f ? CLR_INDIGO : d < 0.75f ? CLR_ORANGE : CLR_RED; }
static const int KIND_COL[CV_N] = { CLR_DARK_BROWN, CLR_LIME_GREEN, CLR_PINK, CLR_DARK_GREEN, CLR_BLUE, CLR_PEACH, CLR_LIGHT_GREY };
static const int WK_COL[3] = { CLR_DARK_GREEN, CLR_YELLOW, CLR_RED };
static void draw_overlay(float cam_x, float cam_y, float zoom) {
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int OV = 14, L = ((int)(cx - hwv) / OV - 1) * OV, R = (int)(cx + hwv) + OV;
    int T = ((int)(cy - hhv) / OV - 1) * OV, B = (int)(cy + hhv) + OV;
    fillp(0xA5A5, -1);
    for (int wy = T; wy <= B; wy += OV)
        for (int wx = L; wx <= R; wx += OV) {
            int col;
            if (ovl == 1)      col = WK_COL[world_kind_at(wx + OV / 2.0f, wy + OV / 2.0f)];
            else if (ovl == 2) col = KIND_COL[cover_at(wx + OV / 2.0f, wy + OV / 2.0f).kind];
            else               col = dens_ramp(cover_at(wx + OV / 2.0f, wy + OV / 2.0f).dens);
            rectfill(wx, wy, OV, OV, col);
        }
    fillp_reset();
}

// ── shared probe ─────────────────────────────────────────────────────────────────
static const char *probe(float fx, float fy) {
    static char buf[56];
    int wk = world_kind_at(fx, fy);
    if (wk == WK_WILD) { snprintf(buf, sizeof buf, "wild / %s", CV_NAME[cover_at(fx, fy).kind]); return buf; }
    if (wk == WK_FARM) { int fxi = ifloordiv((int)fx, FIELD_P), fyi = ifloordiv((int)fy, FIELD_P);
        snprintf(buf, sizeof buf, "farmland / %s", CR_NAME[hash2(fxi + lf_seed * 257, fyi * 7 + 13) % CR_N]); return buf; }
    int bx = ifloordiv((int)fx, BLK_P), by = ifloordiv((int)fy, BLK_P), IN = BLK_P - ST_W;
    int zone = zone_at(bx * BLK_P + ST_W + IN / 2.0f, by * BLK_P + ST_W + IN / 2.0f);
    if (posmod((int)fx, BLK_P) < ST_W || posmod((int)fy, BLK_P) < ST_W) { snprintf(buf, sizeof buf, "%s  street", ZN_NAME[zone]); return buf; }
    LotSlot slots[40]; int nl = block_lots(bx, by, zone, slots, 40); int x = (int)fx, y = (int)fy;
    for (int i = 0; i < nl; i++) {
        Rect b; if (!footprint_body(slots[i].lot, slots[i].outward, zone, slots[i].attached, &b)) continue;
        if (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h) continue;
        Plan p; plan_build(&p, b, zone, slots[i].outward, hmix(slots[i].hash, 3));
        int ri = room_at(&p, x, y);
        snprintf(buf, sizeof buf, "%s / %s", ZN_NAME[zone], ri >= 0 ? RM_NAME[p.room[ri].label] : "wall"); return buf;
    }
    snprintf(buf, sizeof buf, "%s  yard", ZN_NAME[zone]); return buf;
}

// ════════════════════════════════════════════════════════════════════════════
//  SHELL — free-fly explorer
// ════════════════════════════════════════════════════════════════════════════
static float cam_x = 700, cam_y = 520;
static float zoom  = 0.34f;
static int   seed  = 3;

void init(void) { lf_seed = seed; }

void update(void) {
    float pan = 7.0f / zoom;
    if (key(KEY_RIGHT) || key('D')) cam_x += pan;
    if (key(KEY_LEFT)  || key('A')) cam_x -= pan;
    if (key(KEY_DOWN)  || key('S')) cam_y += pan;
    if (key(KEY_UP)    || key('W')) cam_y -= pan;
    if (key('Z')) zoom *= 1.04f;
    if (key('X')) zoom /= 1.04f;
    zoom += mouse_wheel() * 0.08f * zoom;
    zoom = clamp(zoom, 0.12f, 5.0f);
    if (keyp('R')) { seed++; lf_seed = seed; }
    if (keyp('F')) roof_off = !roof_off;
    if (keyp('G')) dbg = !dbg;
    if (keyp('O')) ovl = (ovl + 1) % 4;
}

void draw(void) {
    cls(CLR_BLACK);
    camera_ex((int)cam_x, (int)cam_y, zoom, 0);
    tile(cam_x, cam_y, zoom);
    if (ovl) draw_overlay(cam_x, cam_y, zoom);

    camera(0, 0);
    int mx = SCREEN_W / 2, my = SCREEN_H / 2;
    line(mx - 5, my, mx + 5, my, CLR_WHITE);
    line(mx, my - 5, mx, my + 5, CLR_WHITE);

    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    char buf[88];
    static const char *OVN[4] = { "off", "world", "cover", "dens" };
    const char *mode = roof_off ? "DOLLHOUSE" : zoom >= ROOF_LIFT_Z ? "PEELED" : "ROOFS";
    int x = print("CITYPLAN", 3, 1, CLR_YELLOW);
    snprintf(buf, sizeof buf, "  s%d  z%.2f  %s  open %d/%d  ruin %d  ovl:%s", seed, (double)zoom, mode, n_open, n_bld, n_ruin, OVN[ovl]);
    print(buf, x, 1, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H - 16, SCREEN_W, 16, CLR_BLACK);
    font(FONT_NORMAL);
    print(probe(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f), 3, SCREEN_H - 16, CLR_WHITE);
    font(FONT_SMALL);
    print("WASD/ZX move  R seed  F lift roofs  O overlay  G grid  (zoom in = roofs lift)",
          3, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
