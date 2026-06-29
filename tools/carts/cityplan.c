/* de:meta
{
  "title": "Cityplan — a world you can lift the roof off",
  "status": "active",
  "created": "2026-06-23",
  "kind": [
    "tech-demo",
    "generative"
  ],
  "teaches": [
    "dungeon-generation",
    "noise-terrain"
  ],
  "lineage": "Merges lotfill (terrain/scatter/rows + block-to-varied-lot subdivision, terraces, yards, ruins, border/pave/stamp) and interiors (BSP floor-plans, emergent room labels, door circulation) into one world cart with a zoom-banded level-of-detail roof-lift: world of roofs far, floor-plans near, lazily generated; novel in unifying the two sibling fill-languages across scale behind a single LOD seam.",
  "description": "The full merge of Lotfill + Interiors: the same fill-language at two scales, stacked so you read the whole world by ZOOMING. A WORLD selector reads terrain + an urbanization field and picks per region — WILD land gets scatter wilderness (woodland/meadow/beach/scree/wasteland + all the instance art), FARMLAND gets rows of crops + hedgerows, CITY gets blocks SUBDIVIDED into a street-fronting ring of VARIED lots (dense residential rows TERRACED into shared-wall runs, detached houses with a fenced/hedged yard + driveway). Zoom OUT and you see the world from above: terrain, woods, fields, and a city of ROOFS. Zoom IN and each city roof LIFTS to reveal that footprint's floor-plan (a recursive BSP room tree with emergent labels — living/kitchen/bath, shop/back/office, bay/storage fall out of area/frontage/adjacency rules, never a type list — doors through every wall, windows on the outside). Zoom IN further and the FURNITURE appears. A decay field ruins derelict DISTRICTS (collapse punches the roof/massing open, overgrow creeps it green). Commercial/industrial block interiors are paved (asphalt/plaza/gravel + parking), and ~1 in 5 courtyards gets a fountain/statue/well/obelisk centrepiece. The reveal is pure LEVEL-OF-DETAIL: a roof lifts only once its building is big enough on screen to read (no mush of beds), and the floor-plan is generated LAZILY (far buildings cost a roof rect). F force-lifts every roof (dollhouse); O cycles a field overlay (world-kind/cover/density); G the block+lot grid. Inherits both parents' seams: pure fn of a hash-seed, draw == query (room_at/world_kind_at run as a collision/interaction query). WASD/ZX move, R new seed."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <math.h>

// ============================================================================
// CITYPLAN — a whole procedural WORLD you can LIFT THE ROOF off.
//
//   ◄ ► ▲ ▼ / WASD / left-drag  pan     Z / X (or wheel)  zoom     R  new seed
//   F  force-lift every roof (dollhouse)   O  field overlay (…/value)   G  debug grid
//   a custom cursor + hover panel name what's under the pointer (house / room / furniture)
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
// ── the dwellings layer (a land-VALUE field drives character) ───────────────────
// value_at() composes amenity/density/decay into a per-parcel value; that picks a RES
// archetype (tenement/cottage/family/mansion) which fills rooms from a richer vocabulary
// (dining/kids/study/garage/utility) + one signature feature, and tunes roof/massing/yard
// + furniture density. A tenement footprint becomes several small flats off a shared
// corridor. Furniture is DATA (furnish_items → Furn list) so drawing and the hover probe
// share one definition. Full writeup: docs/design/cityplan.md (Stage D).
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

// ── land value: a COMPOSED field (no new seed) — how desirable a parcel is.
// waterfront/low-lying amenity + central density + green surroundings, minus dereliction.
// Pure fn of the fields above → emergent "nice vs rough" districts, nothing hand-placed.
static float value_at(float x, float y) {
    float e = elev_raw(x, y);
    float shore = clamp(1.0f - (e - ELEV_SAND) / 0.16f, 0.0f, 1.0f);   // near sea level ≈ near water → premium
    float v = 0.50f                                                    // every term centred so the field sits at ~0.5, not dragged low
            + 0.20f * shore                                            // waterfront amenity (one-sided bonus)
            + 0.45f * (urban_density_at(x, y) - 0.50f)                 // central/urban land
            + 0.25f * (fertility_at(x, y)     - 0.50f)                 // green amenity
            - 0.45f * (decay_at(x, y)         - 0.50f);                // dereliction — but MID decay is neutral, not a penalty
    v = 0.50f + (v - 0.50f) * 1.30f;                                   // gentle contrast around mid so districts read
    return clamp(v, 0.0f, 1.0f);
}

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
            int m = massing_at(r.x + r.w / 2.0f, r.y + r.h / 2.0f, zone, lh);
            if (zone == ZN_RES && m < 3 && value_at(r.x + r.w / 2.0f, r.y + r.h / 2.0f) > 0.72f) m++;  // prime lots build bulkier
            out[n].massing  = (unsigned char)m;
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
       RM_SHOP, RM_BACK, RM_OFFICE, RM_BAY, RM_STORE,
       RM_DINING, RM_KIDS, RM_STUDY, RM_GARAGE, RM_UTILITY, RM_N };
static const char *RM_NAME[RM_N] = { "hall", "living", "bedroom", "kitchen", "bath",
                                     "shop floor", "back-of-house", "office", "bay", "storage",
                                     "dining", "kids room", "study", "garage", "utility" };
static const int RM_FLOOR[RM_N] = {
    [RM_HALL]    = CLR_DARK_BROWN,  [RM_LIVING] = CLR_BROWN,      [RM_BED]   = CLR_DARK_PURPLE,
    [RM_KITCHEN] = CLR_DARK_GREY,   [RM_BATH]   = CLR_BLUE_GREEN, [RM_SHOP]  = CLR_LIGHT_GREY,
    [RM_BACK]    = CLR_DARKER_GREY, [RM_OFFICE] = CLR_MEDIUM_GREY,[RM_BAY]   = CLR_DARKER_GREY,
    [RM_STORE]   = CLR_DARK_BROWN,  [RM_DINING] = CLR_BROWN,      [RM_KIDS]  = CLR_DARK_PURPLE,
    [RM_STUDY]   = CLR_DARK_BROWN,  [RM_GARAGE] = CLR_DARKER_GREY,[RM_UTILITY] = CLR_DARK_GREY,
};
enum { T_N = 1, T_E = 2, T_S = 4, T_W = 8 };
typedef struct { Rect r; int label; int touch; } Room;
typedef struct { int x, y, len, vert, dpos, dlen, party; } Wall;   // party = solid divider between dwellings (no door)
#define MAX_ROOMS 32
#define MAX_WALLS 32
typedef struct { Rect shell; int type, outward; float value; Room room[MAX_ROOMS]; int nroom; Wall wall[MAX_WALLS]; int nwall; int egx, egy, entry_room; int sig_kind, sig_room; } Plan;
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
        if (p->nwall < MAX_WALLS) p->wall[p->nwall++] = (Wall){ s, r.y, r.h, 1, dpos, dlen, 0 };
        bsp(p, (Rect){ r.x, r.y, s - r.x, r.h }, hmix(h, 1), depth + 1, sp);
        bsp(p, (Rect){ s, r.y, r.x + r.w - s, r.h }, hmix(h, 2), depth + 1, sp);
    } else {
        int lo = r.y + sp->min_h, hi = r.y + r.h - sp->min_h;
        int s = r.y + (int)(bias * r.h); if (s < lo) s = lo; if (s > hi) s = hi;
        int dlen = 9, span = r.w - 2 * 4 - dlen, dpos = r.x + 4 + (span > 0 ? (int)(frac01(hmix(h, 13)) * span) : 0);
        if (p->nwall < MAX_WALLS) p->wall[p->nwall++] = (Wall){ r.x, s, r.w, 0, dpos, dlen, 0 };
        bsp(p, (Rect){ r.x, r.y, r.w, s - r.y }, hmix(h, 1), depth + 1, sp);
        bsp(p, (Rect){ r.x, s, r.w, r.y + r.h - s }, hmix(h, 2), depth + 1, sp);
    }
}
static int front_bit(int outward) { return outward == 0 ? T_N : outward == 1 ? T_E : outward == 2 ? T_S : T_W; }
// ── residential archetypes — a home's "personality", emergent from value + size ──
enum { ARCH_TENEMENT, ARCH_COTTAGE, ARCH_FAMILY, ARCH_MANSION, ARCH_N };
static const char *ARCH_NAME[ARCH_N] = { "tenement", "cottage", "family home", "mansion" };
static int arch_of(float value, int nroom) {
    if (value > 0.70f && nroom >= 5) return ARCH_MANSION;
    if (value < 0.35f)               return ARCH_TENEMENT;   // cramped, whatever the size
    if (value > 0.45f && nroom >= 4) return ARCH_FAMILY;
    return ARCH_COTTAGE;
}
// what the spare rooms become, biggest-first — this is where each archetype's character lives
static const int ARCH_FILL[ARCH_N][6] = {
    [ARCH_TENEMENT] = { RM_BED,    RM_BED,   RM_BED,  RM_BED,  RM_BED, RM_BED },   // subdivided rental: all beds
    [ARCH_COTTAGE]  = { RM_BED,    RM_DINING,RM_BED,  RM_BED,  RM_BED, RM_BED },
    [ARCH_FAMILY]   = { RM_DINING, RM_KIDS,  RM_BED,  RM_STUDY,RM_BED, RM_BED },
    [ARCH_MANSION]  = { RM_DINING, RM_STUDY, RM_BATH, RM_KIDS, RM_BED, RM_BED },
};
enum { SIG_NONE, SIG_FIREPLACE, SIG_PIANO, SIG_POOL, SIG_LIBRARY };   // memorable per-home feature
static void assign_labels(Plan *p, unsigned h) {
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
        // archetype fills the spare bedrooms with character rooms (dining, kids, study, …)
        int arch = arch_of(p->value, n);
        int lo[MAX_ROOMS], m = 0;
        for (int i = 0; i < n; i++) if (p->room[i].label == RM_BED) lo[m++] = i;
        for (int a = 0; a < m; a++) for (int b = a + 1; b < m; b++)               // sort biggest-first
            if ((long)p->room[lo[b]].r.w * p->room[lo[b]].r.h > (long)p->room[lo[a]].r.w * p->room[lo[a]].r.h) { int t = lo[a]; lo[a] = lo[b]; lo[b] = t; }
        int gi = (arch >= ARCH_FAMILY && m >= 3 && frac01(hmix(h, 71)) < 0.40f) ? 0     : -1;  // biggest spare → garage
        int ui = (m >= 2 && frac01(hmix(h, 83)) < 0.45f)                        ? m - 1 : -1;  // smallest spare → laundry
        int fi = 0;
        for (int k = 0; k < m; k++) {
            if      (k == gi) p->room[lo[k]].label = RM_GARAGE;
            else if (k == ui) p->room[lo[k]].label = RM_UTILITY;
            else              p->room[lo[k]].label = ARCH_FILL[arch][fi < 6 ? fi++ : 5];
        }
        // one signature feature — the thing you remember the house by (chance rises with wealth)
        unsigned sg = hmix(h, 53);
        float chance = arch == ARCH_MANSION ? 0.85f : arch == ARCH_FAMILY ? 0.45f : arch == ARCH_COTTAGE ? 0.20f : 0.0f;
        if (frac01(sg) < chance) {
            int studyi = -1; for (int i = 0; i < n; i++) if (p->room[i].label == RM_STUDY) studyi = i;
            if (studyi >= 0 && (sg & 4)) { p->sig_kind = SIG_LIBRARY; p->sig_room = studyi; }
            else { int pick = (sg >> 3) % (arch == ARCH_MANSION ? 3 : 2);                 // pool table is mansion-only
                   p->sig_kind = (int[]){ SIG_FIREPLACE, SIG_PIANO, SIG_POOL }[pick]; p->sig_room = liv; }
        }
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
// ── multi-unit footprints — poor/tenement land packs several small flats behind party walls ──
static int unit_count(int type, float value, Rect in) {
    if (type != ZN_RES || value >= 0.35f) return 1;          // only tenement-grade land subdivides into flats
    long area = (long)in.w * in.h; int u = (int)(area / 1100);   // ~one flat per ~33×33
    if (u < 2) u = (area > 700) ? 2 : 1;
    return u > 6 ? 6 : u;
}
static void label_dwelling(Plan *p, int s, int e) {         // label rooms [s,e) as one little flat
    if (e <= s) return;
    int big = s, small = s;
    for (int i = s; i < e; i++) { long a = (long)p->room[i].r.w * p->room[i].r.h;
        if (a > (long)p->room[big].r.w   * p->room[big].r.h)   big = i;
        if (a < (long)p->room[small].r.w * p->room[small].r.h) small = i; }
    for (int i = s; i < e; i++) p->room[i].label = RM_BED;
    p->room[big].label = RM_LIVING;
    if (e - s >= 3 && small != big) p->room[small].label = RM_BATH;
}
static void unit_flat(Plan *p, Rect f, unsigned h, int wx, int wy, int wlen, int wvert) {
    int s = p->nroom; SplitP us = { 9, 9, 2, 0.40f, 0.60f };
    bsp(p, f, h, 0, &us); label_dwelling(p, s, p->nroom);
    if (p->nwall < MAX_WALLS) { int dl = 4, dpos = (wvert ? wy : wx) + wlen / 2 - dl / 2;
        p->wall[p->nwall++] = (Wall){ wx, wy, wlen, wvert, dpos, dl, 0 }; }   // the flat's front door onto the shared corridor
}
static void build_units(Plan *p, Rect in, int units, unsigned h) {
    int cw = 5, per = (units + 1) / 2; if (per < 1) per = 1;
    if (p->outward == 0 || p->outward == 2) {               // corridor runs in vertically from a top/bottom entry
        int cx0 = p->egx - cw / 2;
        if (cx0 < in.x + 11) cx0 = in.x + 11;
        if (cx0 + cw > in.x + in.w - 11) cx0 = in.x + in.w - cw - 11;
        if (cx0 < in.x + 4 || cx0 + cw > in.x + in.w - 4) { int s = p->nroom; bsp(p, in, h, 0, &SPLIT[ZN_RES]); label_dwelling(p, s, p->nroom); return; }
        if (p->nroom < MAX_ROOMS) p->room[p->nroom++] = (Room){ (Rect){ cx0, in.y, cw, in.h }, RM_HALL, 0 };
        for (int sd = 0; sd < 2; sd++) {
            int rx = sd ? cx0 + cw : in.x, rw = sd ? (in.x + in.w) - (cx0 + cw) : cx0 - in.x, wx = sd ? cx0 + cw : cx0, y = in.y;
            for (int r = 0; r < per; r++) { int rh = (in.y + in.h - y) / (per - r);
                unit_flat(p, (Rect){ rx, y, rw, rh }, hmix(h, sd * 9 + r + 1), wx, y, rh, 1);
                if (r > 0 && p->nwall < MAX_WALLS) p->wall[p->nwall++] = (Wall){ rx, y, rw, 0, 0, 0, 1 };   // party wall between stacked flats
                y += rh; }
        }
    } else {                                                // corridor runs in horizontally from a left/right entry
        int cy0 = p->egy - cw / 2;
        if (cy0 < in.y + 11) cy0 = in.y + 11;
        if (cy0 + cw > in.y + in.h - 11) cy0 = in.y + in.h - cw - 11;
        if (cy0 < in.y + 4 || cy0 + cw > in.y + in.h - 4) { int s = p->nroom; bsp(p, in, h, 0, &SPLIT[ZN_RES]); label_dwelling(p, s, p->nroom); return; }
        if (p->nroom < MAX_ROOMS) p->room[p->nroom++] = (Room){ (Rect){ in.x, cy0, in.w, cw }, RM_HALL, 0 };
        for (int sd = 0; sd < 2; sd++) {
            int ry = sd ? cy0 + cw : in.y, rh = sd ? (in.y + in.h) - (cy0 + cw) : cy0 - in.y, wy = sd ? cy0 + cw : cy0, x = in.x;
            for (int c = 0; c < per; c++) { int rw = (in.x + in.w - x) / (per - c);
                unit_flat(p, (Rect){ x, ry, rw, rh }, hmix(h, sd * 9 + c + 5), x, wy, rw, 0);
                if (c > 0 && p->nwall < MAX_WALLS) p->wall[p->nwall++] = (Wall){ x, ry, rh, 1, 0, 0, 1 };   // party wall between flats
                x += rw; }
        }
    }
}
static void plan_build(Plan *p, Rect shell, int type, int outward, unsigned h, float value) {
    p->shell = shell; p->type = type; p->outward = outward; p->value = value; p->nroom = p->nwall = 0; p->entry_room = -1;
    p->sig_kind = 0; p->sig_room = -1;
    Rect interior = { shell.x + 1, shell.y + 1, shell.w - 2, shell.h - 2 };
    int units = unit_count(type, value, interior);
    if (interior.w < 4 || interior.h < 4) { emit_room(p, interior); units = 1; }
    else if (units > 1) build_units(p, interior, units, h);  // tenement: a row of small flats behind party walls
    else {
        SplitP sp = SPLIT[type];
        if (type == ZN_RES) {                                // value drives how finely a single home subdivides
            sp.maxdepth += value > 0.66f ? 1 : value < 0.33f ? -1 : 0;
            int bigger = (int)((1.0f - value) * 5.0f);       // poorer homes: larger min room → fewer, cruder rooms
            sp.min_w += bigger; sp.min_h += bigger;
        }
        bsp(p, interior, h, 0, &sp);
    }
    int cx = shell.x + shell.w / 2, cy = shell.y + shell.h / 2, ix = cx, iy = cy;
    switch (outward) {
        case 0: p->egx = cx; p->egy = shell.y;               ix = cx; iy = shell.y + 2;            break;
        case 1: p->egx = shell.x + shell.w - 1; p->egy = cy; ix = shell.x + shell.w - 3; iy = cy;  break;
        case 2: p->egx = cx; p->egy = shell.y + shell.h - 1; ix = cx; iy = shell.y + shell.h - 3;  break;
        default:p->egx = shell.x; p->egy = cy;               ix = shell.x + 2; iy = cy;            break;
    }
    for (int i = 0; i < p->nroom; i++) { Rect r = p->room[i].r;
        if (ix >= r.x && ix < r.x + r.w && iy >= r.y && iy < r.y + r.h) { p->entry_room = i; break; } }
    if (units <= 1) assign_labels(p, h);                     // multi-unit flats are labelled per-unit in build_units
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
// ── furniture as data: each piece is a named rect+style so the SAME list both draws the
//    room and answers "what's under the cursor" — no parallel geometry to drift ──
enum { FX_FILL, FX_RECT, FX_CIRC };
typedef struct { int x, y, w, h, col, style; const char *name; } Furn;
static void draw_furn(const Furn *f) {
    if (f->style == FX_FILL)      rectfill(f->x, f->y, f->w, f->h, f->col);
    else if (f->style == FX_RECT) rect(f->x, f->y, f->w, f->h, f->col);
    else                          circfill(f->x, f->y, f->w, f->col);          // CIRC: (x,y)=centre, w=radius
}
static bool furn_hit(const Furn *f, int x, int y) {
    if (f->style == FX_CIRC) { int dx = x - f->x, dy = y - f->y, rr = f->w + 1; return dx * dx + dy * dy <= rr * rr; }
    return x >= f->x && x < f->x + f->w && y >= f->y && y < f->y + f->h;
}
static int furnish_items(const Room *rm, float value, int sig, Furn *out, int cap) {
    int n = 0; Rect r = rm->r; int ix = r.x + 2, iy = r.y + 2, iw = r.w - 4, ih = r.h - 4;
    if (iw < 4 || ih < 4) return 0;
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2; bool rich = value > 0.50f, lux = value > 0.78f;
    #define PUT(nm, sx, sy, sw, sh, c, st) do { if (n < cap) out[n++] = (Furn){ sx, sy, sw, sh, c, st, nm }; } while (0)
    switch (rm->label) {
        case RM_BED: { int bw = iw * 3 / 5;
            PUT("bed", ix, iy, bw, ih / 2, CLR_INDIGO, FX_FILL); PUT("bed", ix + 1, iy + 1, bw - 2, 2, CLR_LIGHT_PEACH, FX_FILL);
            if (iw >= 9)                     PUT("nightstand", ix + bw + 1, iy, 2, 2, CLR_DARK_BROWN, FX_FILL);
            if (rich && iw >= 9 && ih >= 10) PUT("rug", ix + 1, iy + ih - 4, iw - 2, 3, CLR_DARK_RED, FX_RECT);
            if (lux && iw >= 11)             PUT("wardrobe", ix + iw - 3, iy + ih - 5, 3, 5, CLR_DARK_BROWN, FX_FILL);
            break; }
        case RM_LIVING:
            PUT("sofa", ix, iy + ih - 4, iw * 2 / 3, 3, CLR_DARK_RED, FX_FILL);
            PUT("coffee table", cx - 3, cy, 6, 3, CLR_DARK_BROWN, FX_FILL);
            if (iw >= 9 && ih >= 9) PUT("rug", ix + 1, iy + 1, iw - 2, ih - 6, CLR_BROWN, FX_RECT);
            if (rich && iw >= 8)    PUT("TV console", ix, iy, iw / 2, 2, CLR_DARKER_GREY, FX_FILL);
            if (lux && iw >= 11)    PUT("armchair", ix + iw - 4, iy + ih - 4, 4, 3, CLR_DARK_RED, FX_FILL);
            break;
        case RM_KITCHEN:
            PUT("counter", ix, iy, iw, 2, CLR_LIGHT_GREY, FX_FILL); PUT("counter", ix, iy, 2, ih, CLR_LIGHT_GREY, FX_FILL);
            PUT("fridge", ix + iw - 3, iy + 1, 2, 2, CLR_DARK_GREY, FX_FILL);
            if (rich && iw >= 9 && ih >= 9) PUT("island", cx - 2, cy, 4, 3, CLR_DARK_BROWN, FX_FILL);
            break;
        case RM_BATH:
            PUT("bathtub", ix, iy, iw / 2 + 1, 3, CLR_BLUE, FX_FILL);
            PUT("sink", ix + iw - 2, iy + ih - 2, 1, 0, CLR_WHITE, FX_CIRC);
            if (iw >= 7) PUT("toilet", ix + iw - 3, iy, 2, 2, CLR_WHITE, FX_FILL);
            break;
        case RM_OFFICE:
            PUT("desk", ix, iy, iw * 2 / 3, 3, CLR_DARK_BROWN, FX_FILL); PUT("chair", ix + 1, iy + 4, 3, 3, CLR_DARK_GREY, FX_FILL);
            if (rich && ih >= 8) PUT("bookshelf", ix + iw - 2, iy, 2, ih - 1, CLR_DARK_BROWN, FX_FILL);
            break;
        case RM_HALL: PUT("runner", cx - 1, iy, 3, ih, CLR_DARK_RED, FX_FILL); break;
        case RM_DINING: { int tw = iw / 2 < 4 ? 4 : iw / 2, th = ih / 3 < 3 ? 3 : ih / 3;
            PUT("dining table", cx - tw / 2, cy - th / 2, tw, th, CLR_DARK_BROWN, FX_FILL);
            PUT("chair", cx - tw / 2 - 1, cy, 1, 1, CLR_DARKER_GREY, FX_FILL); PUT("chair", cx + tw / 2, cy, 1, 1, CLR_DARKER_GREY, FX_FILL);
            PUT("chair", cx, cy - th / 2 - 1, 1, 1, CLR_DARKER_GREY, FX_FILL); PUT("chair", cx, cy + th / 2, 1, 1, CLR_DARKER_GREY, FX_FILL);
            break; }
        case RM_KIDS:
            PUT("bed", ix, iy, iw / 2, ih / 3 + 1, CLR_BLUE, FX_FILL); PUT("bed", ix + 1, iy + 1, iw / 2 - 2, 1, CLR_LIGHT_PEACH, FX_FILL);
            if (iw >= 7) PUT("toy chest", ix + iw - 3, iy + ih - 3, 2, 2, CLR_RED, FX_FILL);
            break;
        case RM_STUDY:
            PUT("desk", ix, iy, iw * 2 / 3, 3, CLR_DARK_BROWN, FX_FILL);
            if (ih >= 7) PUT("bookshelf", ix + iw - 2, iy, 2, ih - 1, CLR_DARK_BROWN, FX_FILL);
            PUT("chair", ix + 2, iy + 4, 1, 1, CLR_DARKER_GREY, FX_FILL);
            break;
        case RM_GARAGE:
            if (iw >= 6 && ih >= 6) { PUT("car", ix + 1, iy + 1, iw - 4, ih - 3, CLR_TRUE_BLUE, FX_FILL); PUT("car", ix + 2, iy + 2, iw - 6, 2, CLR_BLUE, FX_FILL); }
            PUT("workbench", ix, iy + ih - 1, iw, 1, CLR_DARK_GREY, FX_FILL);
            break;
        case RM_UTILITY:
            PUT("washer", ix, iy, 3, 3, CLR_LIGHT_GREY, FX_FILL);
            if (iw >= 8) PUT("dryer", ix + 4, iy, 3, 3, CLR_LIGHT_GREY, FX_FILL);
            break;
    }
    if (iw >= 5 && ih >= 5) switch (sig) {                          // signature feature, drawn on top
        case SIG_FIREPLACE: PUT("fireplace", cx - 3, r.y + 1, 6, 2, CLR_DARKER_GREY, FX_FILL); PUT("fireplace", cx - 1, r.y + 1, 2, 1, CLR_ORANGE, FX_FILL); break;
        case SIG_PIANO:     PUT("grand piano", ix + iw - 6, iy, 5, 4, CLR_BLACK, FX_FILL); PUT("grand piano", ix + iw - 6, iy, 5, 1, CLR_WHITE, FX_FILL); break;
        case SIG_POOL:      PUT("pool table", cx - 4, r.y + r.h / 2 - 2, 8, 4, CLR_DARK_GREEN, FX_FILL); PUT("pool table", cx - 4, r.y + r.h / 2 - 2, 8, 4, CLR_DARK_BROWN, FX_RECT); break;
        case SIG_LIBRARY:   PUT("library", r.x + 1, r.y + 1, r.w - 2, 1, CLR_DARK_BROWN, FX_FILL); PUT("library", r.x + 1, r.y + r.h - 2, r.w - 2, 1, CLR_DARK_BROWN, FX_FILL);
                            PUT("library", r.x + 1, r.y + 1, 1, r.h - 2, CLR_DARK_BROWN, FX_FILL); PUT("library", r.x + r.w - 2, r.y + 1, 1, r.h - 2, CLR_DARK_BROWN, FX_FILL); break;
    }
    #undef PUT
    return n;
}
static void furnish(const Room *rm, float value, int sig) {
    Rect r = rm->r; int ix = r.x + 2, iy = r.y + 2, iw = r.w - 4, ih = r.h - 4;
    if (iw < 4 || ih < 4) return;
    int lb = rm->label;
    if (lb == RM_LIVING || lb == RM_DINING || lb == RM_STUDY || lb == RM_BED || lb == RM_KIDS) {   // background clutter (under everything)
        int dens = value > 0.50f ? 3 : 5;                   // keep ~1/dens of the grid cells
        for (int yy = iy + 1; yy < iy + ih - 2; yy += 6)
            for (int xx = ix + 1; xx < ix + iw - 2; xx += 6) {
                unsigned g = hash2(xx * 5 + (int)(value * 97), yy * 9 + lf_seed);
                int dx0 = xx - ix, dx1 = (ix + iw) - xx, dy0 = yy - iy, dy1 = (iy + ih) - yy;   // dist to nearest wall
                int dw = dx0; if (dx1 < dw) dw = dx1; if (dy0 < dw) dw = dy0; if (dy1 < dw) dw = dy1;
                if (g % (dw <= 3 ? dens : dens * 3)) continue;  // hug the walls; leave the centre walkable
                int col = (lb == RM_KIDS) ? (int[]){ CLR_RED, CLR_YELLOW, CLR_BLUE }[(g >> 4) % 3]
                                          : (int[]){ CLR_DARK_BROWN, CLR_DARKER_GREY, CLR_DARK_BROWN, CLR_DARK_GREEN }[(g >> 4) % 4];
                rectfill(xx, yy, 2, 2, col);
            }
    }
    if (lb == RM_SHOP)  { for (int yy = iy + 1; yy < iy + ih - 3; yy += 3) line(ix, yy, ix + iw - 1, yy, CLR_DARK_BROWN); rectfill(ix, iy + ih - 2, iw, 2, CLR_BROWN); return; }
    if (lb == RM_BAY)   { for (int yy = iy + 2; yy < iy + ih - 3; yy += 7) for (int xx = ix + 2; xx < ix + iw - 3; xx += 8) { if (hash2(xx, yy + lf_seed) & 1) continue; rectfill(xx, yy, 4, 4, CLR_BROWN); rect(xx, yy, 4, 4, CLR_DARK_BROWN); } return; }
    if (lb == RM_STORE || lb == RM_BACK) { for (int yy = iy + 1; yy < iy + ih - 1; yy += 3) line(ix, yy, ix + iw - 1, yy, CLR_DARK_BROWN); return; }
    Furn items[28]; int ni = furnish_items(rm, value, sig, items, 28);   // discrete named pieces + signature
    for (int i = 0; i < ni; i++) draw_furn(&items[i]);
}
static void plan_draw(const Plan *p, Show s) {
    Rect sh = p->shell;
    rectfill(sh.x + 1, sh.y + 1, sh.w - 2, sh.h - 2, SLAB_COL);
    if (s.floors) for (int i = 0; i < p->nroom; i++) { Rect r = p->room[i].r; rectfill(r.x, r.y, r.w, r.h, RM_FLOOR[p->room[i].label]); }
    if (s.props) for (int i = 0; i < p->nroom; i++) furnish(&p->room[i], p->value, i == p->sig_room ? p->sig_kind : SIG_NONE);
    if (s.walls) for (int i = 0; i < p->nwall; i++) { Wall w = p->wall[i]; int wc = w.party ? WALL_EXT : WALL_INT;
        if (w.vert) { for (int y = w.y; y < w.y + w.len; y++) if (!(w.dlen > 0 && y >= w.dpos && y < w.dpos + w.dlen)) pset(w.x, y, wc); }
        else        { for (int x = w.x; x < w.x + w.len; x++) if (!(w.dlen > 0 && x >= w.dpos && x < w.dpos + w.dlen)) pset(x, w.y, wc); } }
    if (s.doors) for (int i = 0; i < p->nwall; i++) { Wall w = p->wall[i]; if (w.party || w.dlen <= 0) continue;
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
static void draw_roof(Rect b, int zone, int massing, unsigned h, float value) {
    static const int ROOF_RES [5] = { CLR_DARK_PURPLE, CLR_DARK_RED, CLR_BROWN, CLR_RED, CLR_PEACH };  // drab→warm, low→high value
    static const int ROOF_COM [4] = { CLR_DARK_GREY, CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_INDIGO };
    static const int ROOF_IND [3] = { CLR_DARKER_BLUE, CLR_DARKER_GREY, CLR_DARK_GREY };
    int col;
    if (zone == ZN_RES) {
        int vi = (int)(value * 4.999f) + (h % 4 == 0 ? ((h >> 2) & 1 ? 1 : -1) : 0);  // value sets the tier; occasional ±1 jitter for texture
        col = ROOF_RES[vi < 0 ? 0 : vi > 4 ? 4 : vi];
    } else col = zone == ZN_COM ? ROOF_COM[h % 4] : ROOF_IND[h % 3];
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
static void border_edge(Rect r, int style);                  // defined with pave/stamp below
static void draw_outdoor(Rect lot, Rect b, int outward, int zone, int attached, unsigned h, bool detail, float value) {
    int dc = CLR_MEDIUM_GREY, dx = b.x + b.w / 2, dy = b.y + b.h / 2;
    switch (outward) {
        case 0: line(dx, lot.y, dx, b.y, dc);                       break;
        case 2: line(dx, b.y + b.h - 1, dx, lot.y + lot.h - 1, dc); break;
        case 1: line(b.x + b.w - 1, dy, lot.x + lot.w - 1, dy, dc); break;
        case 3: line(lot.x, dy, b.x, dy, dc);                       break;
    }
    if (detail && zone == ZN_RES && !attached) {
        border_edge(lot, value > 0.66f ? 2 : value > 0.42f ? 0 : 1);   // wall (affluent) / hedge / picket fence
        if (value >= 0.34f) {                                          // poorest lots stay bare
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
}

// ── lotfill's last three atoms, brought in CONTEXTUALLY (not as tabs) ────────────
// border — stroke a lot edge: hedge / fence / wall (around detached RES lots).
static void border_edge(Rect r, int style) {
    int x1 = r.x + r.w - 1, y1 = r.y + r.h - 1;
    if (style == 0) {                                          // hedge
        for (int x = r.x; x <= x1; x++) { int c = ((x + r.y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN; pset(x, r.y, c); pset(x, y1, c); }
        for (int y = r.y; y <= y1; y++) { int c = ((r.x + y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN; pset(r.x, y, c); pset(x1, y, c); }
    } else if (style == 1) {                                   // picket fence
        rect(r.x, r.y, r.w, r.h, CLR_BROWN);
        for (int x = r.x; x <= x1; x += 4) { pset(x, r.y, CLR_DARK_BROWN); pset(x, y1, CLR_DARK_BROWN); }
        for (int y = r.y; y <= y1; y += 4) { pset(r.x, y, CLR_DARK_BROWN); pset(x1, y, CLR_DARK_BROWN); }
    } else {                                                   // stone wall
        for (int x = r.x; x <= x1; x++) { int c = ((x >> 1) & 1) ? CLR_LIGHT_GREY : CLR_DARK_GREY; pset(x, r.y, c); pset(x, y1, c); }
        for (int y = r.y; y <= y1; y++) { int c = ((y >> 1) & 1) ? CLR_LIGHT_GREY : CLR_DARK_GREY; pset(r.x, y, c); pset(x1, y, c); }
    }
}
// pave — flat surface fill for a COM/IND block interior: asphalt / plaza / gravel.
static void pave_interior(Rect r, int surf, bool marks) {
    int base = surf == 0 ? CLR_DARKER_GREY : surf == 1 ? CLR_LIGHT_GREY : CLR_DARK_BROWN;
    rectfill(r.x, r.y, r.w, r.h, base);
    if (!marks) return;
    if (surf == 0) for (int x = r.x + 8; x < r.x + r.w; x += 11) line(x, r.y, x, r.y + r.h - 1, CLR_MEDIUM_GREY);  // faint parking stalls
    else if (surf == 1) {                                                                                          // plaza tiles
        for (int x = r.x; x < r.x + r.w; x += 8) line(x, r.y, x, r.y + r.h - 1, CLR_MEDIUM_GREY);
        for (int y = r.y; y < r.y + r.h; y += 8) line(r.x, y, r.x + r.w - 1, y, CLR_MEDIUM_GREY);
    } else for (int y = r.y; y < r.y + r.h; y += 2)                                                               // gravel speckle
        for (int x = r.x + (y & 1); x < r.x + r.w; x += 2) if ((hash2(x, y + lf_seed) & 3) == 0) pset(x, y, CLR_MEDIUM_GREY);
}
// stamp — one authored composite at a plaza/green anchor: fountain / statue / well / obelisk.
static void stamp_prop(int cx, int cy, int R, int kind) {
    if (R < 2) return;
    circfill(cx + 1, cy + 2, R - 1, CLR_MEDIUM_GREY);          // shadow
    switch (kind) {
        case 0: circfill(cx, cy, R, CLR_LIGHT_GREY); circfill(cx, cy, R - 2, CLR_BLUE); circfill(cx, cy, 1, CLR_WHITE); break;       // fountain
        case 1: rectfill(cx - 2, cy - 1, 4, R + 1, CLR_DARK_GREY); circfill(cx, cy - R + 2, 2, CLR_LIGHT_GREY); break;               // statue
        case 2: circ(cx, cy, R, CLR_DARK_BROWN); circfill(cx, cy, R - 1, CLR_DARKER_GREY); line(cx - R, cy - R, cx + R, cy - R, CLR_BROWN); break;  // well
        default:rectfill(cx - 1, cy - R, 3, 2 * R, CLR_MEDIUM_GREY); pset(cx, cy - R - 1, CLR_WHITE); break;                          // obelisk
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
    unsigned bhash = hash2(bx * 131 + lf_seed * 7, by * 89 + 5);
    rectfill(ox, oy, BLK_P, BLK_P, CLR_DARK_GREY);                                 // street band
    if (zone == ZN_RES) rectfill(inx, iny, IN, IN, CLR_DARK_GREEN);                // gardens/greens
    else {                                                                         // commercial/industrial: paved
        int surf = (zone == ZN_COM) ? (int)(bhash & 1)        // asphalt / plaza
                                    : ((bhash & 1) ? 2 : 0);  // gravel / asphalt
        pave_interior((Rect){ inx, iny, IN, IN }, surf, zoom >= 0.5f);
    }
    LotSlot slots[40]; int nl = block_lots(bx, by, zone, slots, 40);
    for (int i = 0; i < nl; i++) {
        Rect lot = slots[i].lot, b; int outward = slots[i].outward, att = slots[i].attached;
        if (!footprint_body(lot, outward, zone, att, &b)) continue;
        n_bld++;
        float fcx = b.x + b.w / 2.0f, fcy = b.y + b.h / 2.0f;
        float dec = decay_at(fcx, fcy), fert = fertility_at(fcx, fcy), val = value_at(fcx, fcy);
        draw_outdoor(lot, b, outward, zone, att, slots[i].hash, outdoor_detail, val);
        bool peel = want_peel && (b.w * zoom >= MIN_OPEN_PX || b.h * zoom >= MIN_OPEN_PX);
        if (peel) {
            Plan p; plan_build(&p, b, zone, outward, hmix(slots[i].hash, 3), val);
            Show s = { .floors = true, .walls = true, .doors = zoom >= 0.95f, .win = zoom >= 0.95f, .props = zoom >= 1.5f };
            plan_draw(&p, s); n_open++;
            if (dbg) circ(p.egx, p.egy, 1, CLR_GREEN);
        } else {
            draw_roof(b, zone, slots[i].massing, slots[i].hash, val);
        }
        if (dec >= RUIN_TH) {                                // RUINS modifier — roof view only; a peeled plan shows the intact layout
            if (!peel) { collapse_fill(b, slots[i].hash, dec); overgrow_fill(b, slots[i].hash, dec, fert); }
            n_ruin++;
        }
    }
    if (outdoor_detail && bhash % 5 == 0)                    // ~1 in 5 blocks: a courtyard/green centrepiece
        stamp_prop(inx + IN / 2, iny + IN / 2, 7, (int)((bhash >> 8) % 4));

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
static int value_ramp(float v) { return v < 0.30f ? CLR_DARK_RED : v < 0.48f ? CLR_ORANGE : v < 0.64f ? CLR_YELLOW : v < 0.80f ? CLR_LIME_GREEN : CLR_GREEN; }
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
            else if (ovl == 3) col = dens_ramp(cover_at(wx + OV / 2.0f, wy + OV / 2.0f).dens);
            else               col = value_ramp(value_at(wx + OV / 2.0f, wy + OV / 2.0f));
            rectfill(wx, wy, OV, OV, col);
        }
    fillp_reset();
    if (ovl == 4) {                              // value mode: annotate with the numeric field (screen-space so it stays legible at any zoom)
        camera(0, 0);
        font(FONT_TINY);
        char vb[8];
        for (int sy = 12; sy < SCREEN_H - 18; sy += 22)
            for (int sx = 2; sx < SCREEN_W - 12; sx += 24) {
                float wx = (sx - SCREEN_W / 2.0f) / zoom + cam_x + SCREEN_W / 2.0f;
                float wy = (sy - SCREEN_H / 2.0f) / zoom + cam_y + SCREEN_H / 2.0f;
                snprintf(vb, sizeof vb, "%.2f", (double)value_at(wx, wy));
                print(vb, sx, sy, CLR_BLACK);
            }
        font(FONT_NORMAL);
    }
}

// ── shared probe — l1 = container (house/farm/wild), l2 = room/detail, l3 = furniture (or "") ──
static void hover_at(float fx, float fy, char *l1, char *l2, char *l3, int cap) {
    l3[0] = 0;
    int bx = ifloordiv((int)fx, BLK_P), by = ifloordiv((int)fy, BLK_P), IN = BLK_P - ST_W;
    float bcx = bx * BLK_P + ST_W + IN / 2.0f, bcy = by * BLK_P + ST_W + IN / 2.0f;
    if (world_kind_at(bcx, bcy) == WK_CITY) {                   // classify like the renderer: per-BLOCK, not per-point
        int zone = zone_at(bcx, bcy);
        if (posmod((int)fx, BLK_P) < ST_W || posmod((int)fy, BLK_P) < ST_W) { snprintf(l1, cap, "%s", ZN_NAME[zone]); snprintf(l2, cap, "street"); return; }
        LotSlot slots[40]; int nl = block_lots(bx, by, zone, slots, 40); int x = (int)fx, y = (int)fy;
        for (int i = 0; i < nl; i++) {
            Rect b; if (!footprint_body(slots[i].lot, slots[i].outward, zone, slots[i].attached, &b)) continue;
            if (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h) continue;
            Plan p; plan_build(&p, b, zone, slots[i].outward, hmix(slots[i].hash, 3), value_at(b.x + b.w / 2.0f, b.y + b.h / 2.0f));
            int ri = room_at(&p, x, y);
            snprintf(l1, cap, "%s", (zone == ZN_RES) ? ARCH_NAME[arch_of(p.value, p.nroom)] : ZN_NAME[zone]);   // RES → archetype
            snprintf(l2, cap, "%s", ri >= 0 ? RM_NAME[p.room[ri].label] : "wall");
            if (ri >= 0) {                                      // name the furniture piece under the cursor (topmost wins)
                Furn it[28]; int ni = furnish_items(&p.room[ri], p.value, ri == p.sig_room ? p.sig_kind : SIG_NONE, it, 28);
                const char *fn = 0;
                for (int k = 0; k < ni; k++) if (furn_hit(&it[k], x, y)) fn = it[k].name;
                if (fn) snprintf(l3, cap, "%s", fn);
            }
            return;
        }
        snprintf(l1, cap, "%s", ZN_NAME[zone]); snprintf(l2, cap, "yard"); return;
    }
    int fxi = ifloordiv((int)fx, FIELD_P), fyi = ifloordiv((int)fy, FIELD_P);   // farm is per-FIELD-tile, also like the renderer
    if (world_kind_at(fxi * FIELD_P + FIELD_P / 2.0f, fyi * FIELD_P + FIELD_P / 2.0f) == WK_FARM) {
        snprintf(l1, cap, "farmland"); snprintf(l2, cap, "%s", CR_NAME[hash2(fxi + lf_seed * 257, fyi * 7 + 13) % CR_N]); return; }
    snprintf(l1, cap, "wilderness"); snprintf(l2, cap, "%s", CV_NAME[cover_at(fx, fy).kind]);
}
static const char *probe(float fx, float fy) {
    static char buf[80], a[28], b[28], c[28];
    hover_at(fx, fy, a, b, c, sizeof a);
    if (c[0]) snprintf(buf, sizeof buf, "%s / %s / %s", a, b, c);
    else      snprintf(buf, sizeof buf, "%s / %s", a, b);
    return buf;
}

// ════════════════════════════════════════════════════════════════════════════
//  SHELL — free-fly explorer
// ════════════════════════════════════════════════════════════════════════════
static float cam_x = 700, cam_y = 520;
static float zoom  = 0.34f;
static int   seed  = 3;
static int   drag_on = 0, drag_mx, drag_my;       // left-drag to pan
static float drag_cx, drag_cy;

void init(void) { lf_seed = seed; mouse_hide(); }   // we draw our own cursor

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
    if (mouse_pressed(MOUSE_LEFT)) { drag_on = 1; drag_mx = mouse_x(); drag_my = mouse_y(); drag_cx = cam_x; drag_cy = cam_y; }
    if (mouse_released(MOUSE_LEFT)) drag_on = 0;
    if (drag_on && mouse_down(MOUSE_LEFT)) {          // grab-the-map: world point under the cursor stays put
        cam_x = drag_cx - (mouse_x() - drag_mx) / zoom;
        cam_y = drag_cy - (mouse_y() - drag_my) / zoom;
    }
    if (keyp('R')) { seed++; lf_seed = seed; }
    if (keyp('F')) roof_off = !roof_off;
    if (keyp('G')) dbg = !dbg;
    if (keyp('O')) ovl = (ovl + 1) % 5;
}

// custom cursor + a little inspector panel naming what's under the pointer
static void draw_cursor_and_hover(void) {
    int mx = mouse_x(), my = mouse_y();
    if (mx <= 0 && my <= 0) return;                       // no real pointer (e.g. headless bake) → draw nothing
    if (!drag_on) {                                       // inspector panel (hidden while panning)
        float wx = (mx - SCREEN_W / 2.0f) / zoom + cam_x + SCREEN_W / 2.0f;
        float wy = (my - SCREEN_H / 2.0f) / zoom + cam_y + SCREEN_H / 2.0f;
        char l1[28], l2[28], l3[28]; hover_at(wx, wy, l1, l2, l3, sizeof l1);
        font(FONT_SMALL);
        int n1 = 0; while (l1[n1]) n1++; int n2 = 0; while (l2[n2]) n2++; int n3 = 0; while (l3[n3]) n3++;
        int mw = n1; if (n2 > mw) mw = n2; if (n3 > mw) mw = n3;
        int lines = l3[0] ? 3 : 2;
        int w = mw * 5 + 7, h = 3 + lines * 7, px = mx + 9, py = my + 6;
        if (px + w > SCREEN_W) px = mx - w - 3;
        if (py + h > SCREEN_H) py = my - h - 3;
        if (px < 0) px = 0; if (py < 0) py = 0;
        rectfill(px, py, w, h, CLR_BLACK); rect(px, py, w, h, CLR_LIGHT_GREY);
        print(l1, px + 3, py + 2, CLR_WHITE);
        print(l2, px + 3, py + 9, CLR_LIGHT_GREY);
        if (l3[0]) print(l3, px + 3, py + 16, CLR_YELLOW);
        font(FONT_NORMAL);
    }
    for (int r = 0; r <= 7; r++) line(mx, my + r, mx + r, my + r, CLR_WHITE);   // arrow, tip at (mx,my)
    line(mx, my, mx, my + 7, CLR_BLACK); line(mx, my + 7, mx + 7, my + 7, CLR_BLACK); line(mx, my, mx + 7, my + 7, CLR_BLACK);
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
    static const char *OVN[5] = { "off", "world", "cover", "dens", "value" };
    const char *mode = roof_off ? "DOLLHOUSE" : zoom >= ROOF_LIFT_Z ? "PEELED" : "ROOFS";
    int x = print("CITYPLAN", 3, 1, CLR_YELLOW);
    snprintf(buf, sizeof buf, "  s%d  z%.2f  %s  open %d/%d  ruin %d  ovl:%s", seed, (double)zoom, mode, n_open, n_bld, n_ruin, OVN[ovl]);
    print(buf, x, 1, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H - 16, SCREEN_W, 16, CLR_BLACK);
    font(FONT_NORMAL);
    print(probe(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f), 3, SCREEN_H - 16, CLR_WHITE);
    font(FONT_SMALL);
    print("WASD/ZX or drag move  wheel zoom  R seed  F roofs  O overlay  G grid  (zoom in = roofs lift)",
          3, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    draw_cursor_and_hover();
}
