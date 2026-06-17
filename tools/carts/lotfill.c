#include "studio.h"
#include <stdio.h>
#include <math.h>

// ============================================================================
// LOTFILL — the street-level CONTENT language + its WORKBENCH.
//
//   ◄ ► ▲ ▼ / WASD   pan        Z / X (or wheel)  zoom        R  new seed
//   TAB  switch atom  1/2/3  peel layers   G  debug grid   O  field overlay
//
// ── what this is ─────────────────────────────────────────────────────────────
// roadnet/procplaces build the PARTITION (roads → blocks → lots, + the rural land
// between cities). This cart fills it. Design: docs/design/streetlevel-content.md.
// The thesis there: every street-level feature is a BOUNDED REGION + a SEEDED
// FILL-RULE, expressed as a pure fn of (region, hash, zone) that answers draw()
// AND the collision/query call from the SAME code. Each fill-rule is an ATOM; this
// cart is the workbench where each atom is a TAB you inspect in isolation — and the
// WORLD tab is the DRIVER that composes them all (same fns, the driver changes).
//
// ── the tabs (TAB to switch) ──────────────────────────────────────────────────
// • world   — the DRIVER (default): a tiny self-contained worldgen. A SELECTOR
//   (world_kind_at) reads terrain + an urbanization field and picks an atom per
//   region — wild→scatter, farmland→rows, town→footprint. "Fill a whole world",
//   no roadnet/worldgen dependency. Layers peel the three domains; G = selector map.
// • scatter — CONTINUOUS-COVER fill-mode (no parcel/identity): forest, meadow. We
//   DON'T enumerate trees; cover_at(x,y) is a field, and the renderer scatters
//   instances per visible JITTERED-GRID cell, each a pure fn of its own cell + seed
//   (lattice instances are local; grown sets tear at chunk borders). Infinite,
//   seam-safe, byte-identical for a seed.
// • rows    — BOUNDED fill-mode: a coarse parcel grid of fields, each planted with
//   rows of a crop + a hedgerow. rows_fill() fills ONE region; the driver tiles it.
// • footprint — buildings: a block grid carves streets, perimeter lots front them
//   by construction, and one footprint_fill() makes house/shop/tower from a ZONE.
// • ruins   — the first MODIFIER PIPELINE (phase 5): footprint → collapse → overgrow.
//   A DECAY field drives two modifiers that READ the maker's output and transform it
//   (collapse invents absence; overgrow creeps growth, scaled by a fertility field).
//   Layers peel the three pipeline stages; G = the decay heatmap. Proves phase 4→5.
// • border / pave / stamp — the small bounded atoms: edge a region (hedge/fence/
//   wall), flat-fill a surface (asphalt/plaza/gravel/court), or drop a composite
//   (fountain/statue/well/obelisk). All tiled by demo_grid() — which IS subdivide.
//
// Two field layers + two scatter layers make it "interesting" with no special cases:
//   • biome field (warped fbm) → cover KIND (bare/grass/meadow/forest)
//   • density field (finer fbm) → clearings in the forest, flower clumps in the meadow
//   • CANOPY scatter (coarse pitch) → trees, gated by forest density (clearings emerge)
//   • GROUND scatter (fine pitch)   → flowers / bushes / tufts (clumped by density)
//
// ── seeded & repeatable (same trick as procplaces) ───────────────────────────
// noise3(x, y, (float)seed) with an INTEGER seed is a fully independent repeatable
// field. Every instance is hash2(cell, seed) — no global pass, no accumulation.
// ============================================================================

#define BIO_FREQ   0.0016f   // biome frequency (smaller = bigger forests/meadows)
#define BIO_WFREQ  0.0011f   // domain-warp frequency (organic cover edges)
#define BIO_WAMP   140.0f    // domain-warp amplitude (px)
#define DENS_FREQ  0.0090f   // clearing/clump frequency (bigger = patchier)
#define ELEV_FREQ  0.0011f   // elevation frequency (smaller = bigger landmasses/lakes)
#define ELEV_SEA   0.30f     // below this = water
#define ELEV_SAND  0.345f    // water..this = beach ring
#define ELEV_ROCK  0.80f     // above this = bare rock (uplands)

#define CANOPY_P   16        // canopy scatter pitch (world px) — tree spacing
#define GROUND_P   6         // ground scatter pitch — flowers/bushes/tufts
#define CLUMP_P    44        // flower-clump pitch (one colour per patch)

// Two stacked fields decide a cover kind: ELEVATION first (water / beach / rock —
// terrain, hard constraints the atoms must respect), then the vegetation BIOME on
// the land between. Eventually elevation comes from worldgen; here it's local.
enum { CV_BARE, CV_GRASS, CV_MEADOW, CV_FOREST, CV_WATER, CV_SAND, CV_ROCK, CV_N };
static const char *CV_NAME[CV_N] = { "bare", "grass", "meadow", "forest", "water", "sand", "rock" };

static int lf_seed = 0;

static unsigned hash2(int a, int b) {                 // per-cell pseudo-random
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static int ifloordiv(int a, int b) {                  // floor division (handles negatives)
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}
static float frac01(unsigned h) { return (h & 4095u) / 4095.0f; }   // 12 bits → [0,1]

// ── the cover field: what grows here, and how densely ────────────────────────
typedef struct { int kind; float dens; } Cover;       // dens ∈ [0,1] within the kind

static void bio_warp(float x, float y, float *ox, float *oy) {
    float z = (float)lf_seed;
    *ox = x + (noise3(x * BIO_WFREQ, y * BIO_WFREQ, z + 11.0f) - 0.5f) * 2.0f * BIO_WAMP;
    *oy = y + (noise3(x * BIO_WFREQ, y * BIO_WFREQ, z + 19.0f) - 0.5f) * 2.0f * BIO_WAMP;
}
static float bio_raw(float x, float y) {              // fbm, 3 octaves
    float z = (float)lf_seed;
    float a = noise3(x * BIO_FREQ,        y * BIO_FREQ,        z);
    float b = noise3(x * BIO_FREQ * 2.3f, y * BIO_FREQ * 2.3f, z + 3.0f);
    float c = noise3(x * BIO_FREQ * 5.1f, y * BIO_FREQ * 5.1f, z + 7.0f);
    return clamp(a * 0.6f + b * 0.28f + c * 0.12f, 0.0f, 1.0f);
}
static float elev_raw(float x, float y) {             // elevation fbm, 2 octaves
    float z = (float)lf_seed;
    float a = noise3(x * ELEV_FREQ,        y * ELEV_FREQ,        z + 50.0f);
    float b = noise3(x * ELEV_FREQ * 2.4f, y * ELEV_FREQ * 2.4f, z + 57.0f);
    return clamp(a * 0.7f + b * 0.3f, 0.0f, 1.0f);
}
// THE shared sampler — every cover-query (draw + probe + future collision) goes here.
// Elevation gates terrain (water/beach/rock); the vegetation biome fills the land.
static Cover cover_at(float x, float y) {
    float e = elev_raw(x, y);
    float d = clamp(noise3(x * DENS_FREQ, y * DENS_FREQ, (float)lf_seed + 5.0f), 0.0f, 1.0f);
    int kind;
    if      (e < ELEV_SEA)  kind = CV_WATER;
    else if (e < ELEV_SAND) kind = CV_SAND;           // thin shoreline ring
    else if (e > ELEV_ROCK) kind = CV_ROCK;
    else {                                            // land — read the vegetation biome
        float wx, wy; bio_warp(x, y, &wx, &wy);
        float b = bio_raw(wx, wy);
        kind = b < 0.34f ? CV_BARE : b < 0.46f ? CV_GRASS : b < 0.60f ? CV_MEADOW : CV_FOREST;
    }
    return (Cover){ kind, d };
}

// ── base ground tint (cheap filled cells, like terrain bands) ─────────────────
static int ground_col(int cx, int cy, const Cover *cv) {
    unsigned h = hash2(cx * 7 + lf_seed * 911, cy * 13);
    switch (cv->kind) {
        case CV_BARE:   return (h & 1) ? CLR_DARK_BROWN    : CLR_BROWN;
        case CV_GRASS:  return (h & 3) ? CLR_DARK_GREEN    : CLR_MEDIUM_GREEN;
        case CV_MEADOW: return (h & 1) ? CLR_MEDIUM_GREEN  : CLR_LIME_GREEN;
        case CV_WATER:  return (h & 3) ? CLR_DARK_BLUE     : CLR_BLUE;        // ripple
        case CV_SAND:   return (h & 1) ? CLR_LIGHT_PEACH   : CLR_PEACH;
        case CV_ROCK:   return (h % 5 == 0) ? CLR_LIGHT_GREY : CLR_DARK_GREY;
        default: {                                                 // forest floor: dappled
            unsigned f = h % 9;
            return f == 0 ? CLR_BROWNISH_BLACK : f < 3 ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN;
        }
    }
}

// ── instance art (drawn in WORLD units; camera_ex scales) ─────────────────────
// tint trios {dark, mid, light}: most trees green, a few pine / autumn / dead.
static void draw_tree(int x, int y, unsigned h, int big) {
    static const int TR[][3] = {
        { CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN },   // broadleaf (common)
        { CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN },
        { CLR_BLUE_GREEN, CLR_DARK_GREEN,   CLR_MEDIUM_GREEN }, // pine
        { CLR_DARK_BROWN, CLR_ORANGE,       CLR_YELLOW },       // autumn (rare)
    };
    unsigned v = h % 8;                                            // broadleaf 5/8, pine 2/8, autumn 1/8
    const int *t = TR[v < 5 ? 0 : v < 7 ? 2 : 3];
    int r = (big ? 5 : 3) + (int)((h >> 6) & 1);                // canopy radius
    rectfill(x, y - 1, 1 + (big ? 1 : 0), 4, CLR_DARK_BROWN);   // trunk
    circfill(x, y - r,     r,     t[0]);                        // shadow base
    circfill(x - 1, y - r - 1, r - 1, t[1]);                    // body
    circfill(x - 2, y - r - 2, (r >= 4 ? 2 : 1), t[2]);         // highlight
}
static void draw_bush(int x, int y, unsigned h) {
    int c = (h & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
    circfill(x, y, 2, c);
    pset(x - 1, y - 1, CLR_LIME_GREEN);
}
static void draw_tuft(int x, int y, unsigned h) {              // grass blades
    int c = (h & 1) ? CLR_MEDIUM_GREEN : CLR_LIME_GREEN;
    line(x,     y, x,     y - 2, c);
    line(x - 1, y, x - 2, y - 2, c);
    line(x + 1, y, x + 2, y - 2, c);
}
static void draw_flower(int x, int y, int col) {
    pset(x, y, CLR_MEDIUM_GREEN);                               // stem dot
    pset(x, y - 2, col);                                       // bloom
}
static int flower_col(int cx, int cy) {                        // one colour per clump
    static const int FC[6] = { CLR_RED, CLR_YELLOW, CLR_WHITE, CLR_ORANGE, CLR_PINK, CLR_MAUVE };
    unsigned h = hash2(ifloordiv(cx * CANOPY_P, CLUMP_P) + lf_seed * 31,
                       ifloordiv(cy * CANOPY_P, CLUMP_P));
    return FC[h % 6];
}
static void draw_palm(int x, int y, unsigned h) {              // beach palm
    rectfill(x, y - 6, 1, 7, CLR_DARK_BROWN);                  // trunk
    int c = (h & 1) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN;
    for (int a = -2; a <= 2; a++) line(x, y - 6, x + a * 2, y - 6 - (2 - (a < 0 ? -a : a)), c);  // fronds
    pset(x, y - 7, CLR_LIME_GREEN);
}
static void draw_cactus(int x, int y, unsigned h) {           // desert/arid
    int hh = 4 + (int)((h >> 4) & 3);
    rectfill(x, y - hh, 2, hh, CLR_DARK_GREEN);
    if (h & 1) { rectfill(x - 2, y - hh + 1, 2, 2, CLR_DARK_GREEN); rectfill(x - 2, y - hh - 1, 1, 3, CLR_DARK_GREEN); }
    pset(x, y - hh, CLR_MEDIUM_GREEN);
}
static void draw_deadtree(int x, int y, unsigned h) {         // wasteland
    line(x, y, x, y - 6, CLR_DARK_BROWN);
    line(x, y - 4, x - 2, y - 6, CLR_DARK_BROWN); line(x, y - 5, x + 2, y - 7, CLR_DARK_BROWN);
    pset(x, y - 6, CLR_MEDIUM_GREY);
}
static void draw_rock(int x, int y, unsigned h) {             // scree/upland
    int r = 2 + (int)((h >> 5) & 1);
    circfill(x, y, r, CLR_DARK_GREY);
    pset(x - 1, y - 1, CLR_LIGHT_GREY);
}

// ── SCATTER FLAVORS — one atom, MANY params (docs/design/streetlevel-content.md:
// "atoms vs biomes vs recipes"). A flavor is the scatter atom's params — WHAT to
// place + how dense — not a new atom. The same scatter loop, pointed at a different
// flavor per cover kind, gives woodland / meadow / beach / scree / wasteland; a new
// biome costs ONE row here + (maybe) one instance art, never a new function.
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
typedef struct {                 // the scatter atom's params for one cover kind
    int   canopy;                // coarse-layer instance (I_*)
    float c_base, c_dens;        // canopy prob = c_base + density * c_dens
    int   ground;                // fine-layer instance (I_FLOWER handled specially)
    float g_base, g_dens;        // ground prob
    float big_at;                // density above which canopy draws "big" (< 0 = never)
} Flavor;
enum { FV_NONE, FV_WOODLAND, FV_MEADOW, FV_GRASS, FV_BEACH, FV_SCREE, FV_WASTE, FV_N };
static const Flavor FLAVORS[FV_N] = {
    [FV_NONE]     = { I_NONE,     0,0,        I_NONE,   0,0,        -1 },
    [FV_WOODLAND] = { I_TREE,     0.25f,0.70f, I_BUSH,  0.12f,0.25f, 0.55f },
    [FV_MEADOW]   = { I_TREE,     0.04f,0,     I_FLOWER,0.10f,0.55f, -1 },
    [FV_GRASS]    = { I_NONE,     0,0,         I_TUFT,  0.18f,0,     -1 },
    [FV_BEACH]    = { I_PALM,     0.03f,0,     I_TUFT,  0.05f,0,     -1 },
    [FV_SCREE]    = { I_ROCK,     0.10f,0,     I_ROCK,  0.06f,0,     -1 },
    [FV_WASTE]    = { I_DEADTREE, 0.05f,0,     I_BUSH,  0.05f,0,     -1 },
};
static int flavor_for(int cover_kind) {          // the per-cover recipe (scatter-level selector)
    switch (cover_kind) {
        case CV_FOREST: return FV_WOODLAND;
        case CV_MEADOW: return FV_MEADOW;
        case CV_GRASS:  return FV_GRASS;
        case CV_SAND:   return FV_BEACH;
        case CV_ROCK:   return FV_SCREE;
        case CV_BARE:   return FV_WASTE;
        default:        return FV_NONE;          // water — nothing scatters
    }
}
// place one jittered instance — shared by the SCATTER tab and the WORLD driver
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
//  THE WORKBENCH — shared inspection state every atom reads
// ════════════════════════════════════════════════════════════════════════════
// The reusability seam (docs/design/streetlevel-content.md → "the atom is a pure
// function, the driver is what changes"): an atom never touches cam/zoom/seed
// globals — it's handed the view rect and reads only these inspection flags. So the
// SAME atom fn runs here (one atom on stage, overlays on) and in the future world
// cart (composed over thousands of regions). What you tune here is what ships.
#define MAX_LAYERS 4
static bool layer_on[MAX_LAYERS] = { true, true, true, true };  // 1/2/3 peel each layer
static bool dbg = false;           // G: draw the lattice grid + cull (reject) markers
static int  ovl = 0;                // O: field overlay — 0 off / 1 cover-kind / 2 density
static int  n_tree, n_ground, n_reject;   // scatter's tallies
static int  cnt[3];                        // generic per-atom live tallies → HUD
static const char *cnt_lbl[3];             // their labels (each atom sets these in draw)

static int dens_ramp(float d) {     // density field as a heat colour (low→high)
    return d < 0.25f ? CLR_DARK_BLUE : d < 0.5f ? CLR_INDIGO : d < 0.75f ? CLR_ORANGE : CLR_RED;
}
static const int KIND_COL[CV_N] = { CLR_DARK_BROWN, CLR_LIME_GREEN, CLR_PINK, CLR_DARK_GREEN,
                                    CLR_BLUE, CLR_PEACH, CLR_LIGHT_GREY };

// ── ATOM: scatter ─────────────────────────────────────────────────────────────
// Layers: 0 base tint · 1 ground (flowers/bushes/tufts) · 2 canopy (trees).
enum { SL_BASE, SL_GROUND, SL_CANOPY };

static void scat_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - 8, Rpx = (int)(cx + hwv) + 8;
    int Tpx = (int)(cy - hhv) - 8, Bpx = (int)(cy + hhv) + 8;
    n_tree = n_ground = n_reject = 0;

    // layer 0 — base ground tint (coarse cells that grow when zoomed out, >= ~3 px)
    if (layer_on[SL_BASE]) {
        int gc = 6, step = 1;
        while ((float)gc * step * zoom < 3.0f && step < 6) step++;
        int cellsz = gc * step;
        int g0x = ifloordiv(Lpx, cellsz), g1x = ifloordiv(Rpx, cellsz);
        int g0y = ifloordiv(Tpx, cellsz), g1y = ifloordiv(Bpx, cellsz);
        for (int gy = g0y; gy <= g1y; gy++)
            for (int gx = g0x; gx <= g1x; gx++) {
                int px = gx * cellsz, py = gy * cellsz;
                Cover cv = cover_at(px + cellsz / 2.0f, py + cellsz / 2.0f);
                rectfill(px, py, cellsz, cellsz, ground_col(gx, gy, &cv));
            }
    }

    // debug — draw the CANOPY lattice the candidates sit on (under the instances)
    if (dbg) {
        int c0x = ifloordiv(Lpx, CANOPY_P), c1x = ifloordiv(Rpx, CANOPY_P) + 1;
        int c0y = ifloordiv(Tpx, CANOPY_P), c1y = ifloordiv(Bpx, CANOPY_P) + 1;
        for (int cxi = c0x; cxi <= c1x; cxi++) line(cxi * CANOPY_P, Tpx, cxi * CANOPY_P, Bpx, CLR_DARKER_GREY);
        for (int cyi = c0y; cyi <= c1y; cyi++) line(Lpx, cyi * CANOPY_P, Rpx, cyi * CANOPY_P, CLR_DARKER_GREY);
    }

    bool zg = zoom >= 0.7f, zc = zoom >= 0.30f;   // LOD draw-gates (real behaviour kept)

    // layer 1 — GROUND scatter (per-flavor: flowers / bushes / tufts / rocks)
    if (layer_on[SL_GROUND] && zg) {
        int c0x = ifloordiv(Lpx, GROUND_P), c1x = ifloordiv(Rpx, GROUND_P);
        int c0y = ifloordiv(Tpx, GROUND_P), c1y = ifloordiv(Bpx, GROUND_P);
        for (int cyi = c0y; cyi <= c1y; cyi++)
            for (int cxi = c0x; cxi <= c1x; cxi++) {
                unsigned h = hash2(cxi + lf_seed * 101, cyi * 3 + 7);
                int wx = cxi * GROUND_P + (int)(frac01(h) * GROUND_P);
                int wy = cyi * GROUND_P + (int)(frac01(h >> 12) * GROUND_P);
                n_ground += scatter_ground(cxi, cyi, wx, wy, h);
            }
    }

    // layer 2 — CANOPY scatter (per-flavor: trees / palms / cacti / dead trees / rocks).
    // cy ascending = north→south painter's sort.
    if (layer_on[SL_CANOPY] && zc) {
        int c0x = ifloordiv(Lpx, CANOPY_P), c1x = ifloordiv(Rpx, CANOPY_P);
        int c0y = ifloordiv(Tpx, CANOPY_P), c1y = ifloordiv(Bpx, CANOPY_P);
        for (int cyi = c0y; cyi <= c1y; cyi++)
            for (int cxi = c0x; cxi <= c1x; cxi++) {
                unsigned h = hash2(cxi * 5 + lf_seed * 17, cyi + 3);
                int wx = cxi * CANOPY_P + (int)(frac01(h) * CANOPY_P);
                int wy = cyi * CANOPY_P + (int)(frac01(h >> 12) * CANOPY_P);
                int placed = scatter_canopy(wx, wy, h);
                n_tree += placed;
                if (!placed && dbg && FLAVORS[flavor_for(cover_at(wx, wy).kind)].canopy != I_NONE) {
                    circ(wx, wy, 1, CLR_DARK_RED); n_reject++;   // culled candidate (a flavor exists here)
                }
            }
    }

    cnt[0] = n_tree;   cnt_lbl[0] = "trees";
    cnt[1] = n_ground; cnt_lbl[1] = "grnd";
    cnt[2] = n_reject; cnt_lbl[2] = dbg ? "cull" : NULL;
}

static const char *scat_probe(float x, float y) {
    static char buf[40];
    Cover cv = cover_at(x, y);
    snprintf(buf, sizeof buf, "%s  dens %.2f", CV_NAME[cv.kind], (double)cv.dens);
    return buf;
}

// ── ATOM: rows ───────────────────────────────────────────────────────────────
// The first BOUNDED-fill atom. Where scatter answers an infinite cover field, rows
// fills a bounded REGION: a coarse PARCEL grid (rural fields), each cell planted
// with rows of one crop + a hedgerow border. rows_fill() is the reusable unit (a
// pure fn of rect + hash + which layers to draw — no globals); rows_draw just tiles
// it. The future world cart calls the SAME rows_fill on the parcels its selector
// marks as farmland. Layers: 0 soil · 1 crop (the rows) · 2 border (hedgerows).
#define FIELD_P 60         // parcel pitch (world px) — one field per cell
enum { RL_SOIL, RL_CROP, RL_BORDER };
enum { CR_PLOUGH, CR_CORN, CR_WHEAT, CR_VINE, CR_N };
static const char *CR_NAME[CR_N] = { "plough", "corn", "wheat", "vineyard" };
static const int   CR_SOIL[CR_N] = { CLR_BROWN, CLR_DARK_BROWN, CLR_ORANGE,      CLR_DARK_GREEN };
static const int   CR_ROW [CR_N] = { CLR_DARK_BROWN, CLR_MEDIUM_GREEN, CLR_YELLOW, CLR_BLUE_GREEN };
static const int   CR_SP  [CR_N] = { 5, 4, 3, 8 };   // row spacing (world px)

typedef struct { int x, y, w, h; } Rect;             // a bounded region (world px)

// fill ONE parcel — the reusable atom. `show[]` = which layers to draw (the driver
// folds layer-peel + LOD into it, so this fn stays a pure fn of its inputs).
static void rows_fill(Rect r, unsigned h, const bool show[3]) {
    int crop = h % CR_N;
    bool vert = (h >> 4) & 1;             // row orientation
    int sp = CR_SP[crop];
    int m = 3;                            // hedgerow margin
    int ix = r.x + m, iy = r.y + m, iw = r.w - 2 * m, ih = r.h - 2 * m;
    if (iw < 2 || ih < 2) return;

    if (show[RL_SOIL]) rectfill(r.x, r.y, r.w, r.h, CR_SOIL[crop]);

    if (show[RL_CROP]) {
        if (vert) {
            for (int x = ix + (int)(h % sp); x < ix + iw; x += sp)
                if (crop == CR_VINE) for (int y = iy; y < iy + ih; y += 4) pset(x, y, CR_ROW[crop]);
                else line(x, iy, x, iy + ih - 1, CR_ROW[crop]);
        } else {
            for (int y = iy + (int)(h % sp); y < iy + ih; y += sp)
                if (crop == CR_VINE) for (int x = ix; x < ix + iw; x += 4) pset(x, y, CR_ROW[crop]);
                else line(ix, y, ix + iw - 1, y, CR_ROW[crop]);
        }
    }

    if (show[RL_BORDER]) {                // hedgerow: bushy 1px dark/medium-green ring
        for (int x = r.x; x < r.x + r.w; x++) {
            int c = ((x + r.y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
            pset(x, r.y, c); pset(x, r.y + r.h - 1, c);
        }
        for (int y = r.y; y < r.y + r.h; y++) {
            int c = ((r.x + y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
            pset(r.x, y, c); pset(r.x + r.w - 1, y, c);
        }
    }
}

static void rows_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - FIELD_P, Rpx = (int)(cx + hwv) + FIELD_P;
    int Tpx = (int)(cy - hhv) - FIELD_P, Bpx = (int)(cy + hhv) + FIELD_P;

    bool show[3] = { layer_on[RL_SOIL],
                     layer_on[RL_CROP] && zoom >= 0.55f,   // LOD: rows only when close
                     layer_on[RL_BORDER] };
    int f0x = ifloordiv(Lpx, FIELD_P), f1x = ifloordiv(Rpx, FIELD_P);
    int f0y = ifloordiv(Tpx, FIELD_P), f1y = ifloordiv(Bpx, FIELD_P);
    int nf = 0;
    for (int fy = f0y; fy <= f1y; fy++)
        for (int fx = f0x; fx <= f1x; fx++) {
            unsigned h = hash2(fx + lf_seed * 257, fy * 7 + 13);
            Rect r = { fx * FIELD_P, fy * FIELD_P, FIELD_P, FIELD_P };
            rows_fill(r, h, show);
            nf++;
        }

    if (dbg) {                            // show the parcel partition + the active field
        for (int fx = f0x; fx <= f1x + 1; fx++) line(fx * FIELD_P, Tpx, fx * FIELD_P, Bpx, CLR_DARK_RED);
        for (int fy = f0y; fy <= f1y + 1; fy++) line(Lpx, fy * FIELD_P, Rpx, fy * FIELD_P, CLR_DARK_RED);
        int afx = ifloordiv((int)cx, FIELD_P), afy = ifloordiv((int)cy, FIELD_P);
        rect(afx * FIELD_P, afy * FIELD_P, FIELD_P, FIELD_P, CLR_YELLOW);
    }
    cnt[0] = nf; cnt_lbl[0] = "fields"; cnt_lbl[1] = NULL; cnt_lbl[2] = NULL;
}

static const char *rows_probe(float x, float y) {
    static char buf[40];
    int fx = ifloordiv((int)x, FIELD_P), fy = ifloordiv((int)y, FIELD_P);
    unsigned h = hash2(fx + lf_seed * 257, fy * 7 + 13);
    snprintf(buf, sizeof buf, "field: %s  rows %s", CR_NAME[h % CR_N], ((h >> 4) & 1) ? "V" : "H");
    return buf;
}

// ── ATOM: footprint ──────────────────────────────────────────────────────────
// Buildings — the GTA street. The thesis (docs/design/streetlevel-content.md): a
// house / shop / tower are the SAME atom with different params, chosen by a ZONE
// field (residential / commercial / downtown) — not enumerated types. A block grid
// carves streets; each block's PERIMETER lots front a street BY CONSTRUCTION (the
// centre stays courtyard/parking, so reachability is a theorem — no orphan
// interiors). footprint_fill() fills ONE lot. Layers: 0 ground (streets + lot base)
// · 1 building (the footprint) · 2 detail (driveway/door). Zone tiles a city
// everywhere here; WHERE cities go is the world driver's selector, not the atom.
#define BLK_P 72         // block pitch (world px)
#define ST_W  9          // street width
enum { FL_GROUND, FL_BUILD, FL_DETAIL };
enum { ZN_RES, ZN_COM, ZN_TOWN, ZN_N };          // downtown = ZN_TOWN
static const char *ZN_NAME[ZN_N] = { "residential", "commercial", "downtown" };

static int posmod(int a, int b) { int m = a % b; return m < 0 ? m + b : m; }

static int zone_at(float x, float y) {
    float i = noise3(x * 0.0016f, y * 0.0016f, (float)lf_seed + 90.0f);
    return i < 0.46f ? ZN_RES : i < 0.72f ? ZN_COM : ZN_TOWN;
}

// The INTACT building rect inside a lot — the massing footprint_fill draws. Factored
// out so the ruin MODIFIERS (collapse/overgrow) operate on the SAME rect: phase 5
// RE-DERIVES phase 4's pure output (no retained buffer needed — it's all pure-fn-of-
// (lot,outward,zone)). Returns 0 if the setback leaves nothing buildable.
static int footprint_body(Rect lot, int outward, int zone, Rect *out) {
    int m  = (zone == ZN_TOWN) ? 1 : 2;                                  // side/back margin
    int fs = (zone == ZN_RES)  ? 5 : (zone == ZN_COM) ? 3 : 1;           // front setback (yard)
    int ix = lot.x + m, iy = lot.y + m, iw = lot.w - 2 * m, ih = lot.h - 2 * m;
    switch (outward) {                                                   // push front edge in by setback
        case 0: iy += fs; ih -= fs; break;
        case 2: ih -= fs;           break;
        case 1: iw -= fs;           break;
        case 3: ix += fs; iw -= fs; break;
    }
    if (iw < 2 || ih < 2) return 0;
    *out = (Rect){ ix, iy, iw, ih };
    return 1;
}

// fill ONE building lot. outward ∈ {0 up,1 right,2 down,3 left} = the street side.
static void footprint_fill(Rect lot, int outward, int zone, unsigned h, const bool show[3]) {
    static const int ROOF_RES [5] = { CLR_RED, CLR_DARK_RED, CLR_BROWN, CLR_PEACH, CLR_DARK_PURPLE };
    static const int ROOF_COM [4] = { CLR_DARK_GREY, CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_INDIGO };
    static const int ROOF_TOWN[3] = { CLR_DARKER_BLUE, CLR_DARKER_GREY, CLR_INDIGO };

    Rect b; if (!footprint_body(lot, outward, zone, &b)) return;
    int ix = b.x, iy = b.y, iw = b.w, ih = b.h;

    if (show[FL_BUILD]) {
        int col = zone == ZN_RES ? ROOF_RES[h % 5] : zone == ZN_COM ? ROOF_COM[h % 4] : ROOF_TOWN[h % 3];
        rectfill(ix, iy, iw, ih, col);
        if (zone == ZN_TOWN) {                                          // fake height: lit top, offset up-left
            rectfill(ix - 1, iy - 1, iw, 1, CLR_LIGHT_GREY);
            for (int yy = iy; yy < iy + ih; yy += 2) pset(ix, yy, CLR_DARKER_GREY);  // window columns
        }
    }
    if (show[FL_DETAIL]) {                                               // driveway/path to the street + door
        int dc = CLR_MEDIUM_GREY, dx = ix + iw / 2, dy = iy + ih / 2;
        switch (outward) {
            case 0: line(dx, lot.y, dx, iy, dc);             pset(dx, iy, CLR_BROWNISH_BLACK); break;
            case 2: line(dx, iy + ih - 1, dx, lot.y + lot.h - 1, dc); pset(dx, iy + ih - 1, CLR_BROWNISH_BLACK); break;
            case 1: line(ix + iw - 1, dy, lot.x + lot.w - 1, dy, dc); pset(ix + iw - 1, dy, CLR_BROWNISH_BLACK); break;
            case 3: line(lot.x, dy, ix, dy, dc);             pset(ix, dy, CLR_BROWNISH_BLACK); break;
        }
    }
}

static void footprint_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - BLK_P, Rpx = (int)(cx + hwv) + BLK_P;
    int Tpx = (int)(cy - hhv) - BLK_P, Bpx = (int)(cy + hhv) + BLK_P;

    bool show[3] = { layer_on[FL_GROUND],
                     layer_on[FL_BUILD]  && zoom >= 0.5f,
                     layer_on[FL_DETAIL] && zoom >= 0.9f };   // driveways only when close
    int b0x = ifloordiv(Lpx, BLK_P), b1x = ifloordiv(Rpx, BLK_P);
    int b0y = ifloordiv(Tpx, BLK_P), b1y = ifloordiv(Bpx, BLK_P);
    int IN = BLK_P - ST_W, nlot = 3, ls = IN / nlot, nb = 0;
    for (int by = b0y; by <= b1y; by++)
        for (int bx = b0x; bx <= b1x; bx++) {
            int ox = bx * BLK_P, oy = by * BLK_P, inx = ox + ST_W, iny = oy + ST_W;
            int zone = zone_at(inx + IN / 2.0f, iny + IN / 2.0f);
            if (show[FL_GROUND]) {
                rectfill(ox, oy, BLK_P, BLK_P, CLR_DARK_GREY);                       // street band (top+left L)
                rectfill(inx, iny, IN, IN, zone == ZN_RES ? CLR_DARK_GREEN : CLR_MEDIUM_GREY);
            }
            for (int lj = 0; lj < nlot; lj++)
                for (int li = 0; li < nlot; li++) {
                    if (li == 1 && lj == 1) {                                        // centre courtyard
                        if (show[FL_GROUND] && zone != ZN_RES)
                            rectfill(inx + li * ls, iny + lj * ls, ls, ls, CLR_DARKER_GREY);
                        continue;
                    }
                    Rect lot = { inx + li * ls, iny + lj * ls, ls, ls };
                    int outward = (lj == 0) ? 0 : (lj == 2) ? 2 : (li == 0) ? 3 : 1; // block-outward = street side
                    unsigned hh = hash2(bx * 97 + li + lf_seed * 7, by * 89 + lj);
                    footprint_fill(lot, outward, zone, hh, show);
                    nb++;
                }
        }

    if (dbg) {                                                                       // block partition + active block
        for (int bx = b0x; bx <= b1x + 1; bx++) line(bx * BLK_P, Tpx, bx * BLK_P, Bpx, CLR_DARK_RED);
        for (int by = b0y; by <= b1y + 1; by++) line(Lpx, by * BLK_P, Rpx, by * BLK_P, CLR_DARK_RED);
        int abx = ifloordiv((int)cx, BLK_P), aby = ifloordiv((int)cy, BLK_P);
        rect(abx * BLK_P, aby * BLK_P, BLK_P, BLK_P, CLR_YELLOW);
    }
    cnt[0] = nb; cnt_lbl[0] = "bldgs"; cnt_lbl[1] = NULL; cnt_lbl[2] = NULL;
}

static const char *footprint_probe(float x, float y) {
    static char buf[40];
    int bx = ifloordiv((int)x, BLK_P), by = ifloordiv((int)y, BLK_P), IN = BLK_P - ST_W;
    int zone = zone_at(bx * BLK_P + ST_W + IN / 2.0f, by * BLK_P + ST_W + IN / 2.0f);
    bool street = posmod((int)x, BLK_P) < ST_W || posmod((int)y, BLK_P) < ST_W;
    snprintf(buf, sizeof buf, "%s  %s", ZN_NAME[zone], street ? "street" : "lot");
    return buf;
}

// ── FIELDS: decay (the era/age dial) + fertility (vitality) ───────────────────
// Two read-only surfaces (docs/design/streetlevel-content.md → "The wilds": a field
// is a dial, not a generator). decay 0→1 = pristine→razed; fertility 0→1 = barren→
// lush. The ruin modifiers read BOTH: decay says how broken, fertility how reclaimed.
#define DECAY_FREQ 0.0052f
#define FERT_FREQ  0.0034f
static float decay_at(float x, float y) {
    float z = (float)lf_seed;
    float a = noise3(x * DECAY_FREQ,        y * DECAY_FREQ,        z + 300.0f);
    float b = noise3(x * DECAY_FREQ * 2.6f, y * DECAY_FREQ * 2.6f, z + 311.0f);
    return clamp(a * 0.7f + b * 0.3f, 0.0f, 1.0f);
}
static float fertility_at(float x, float y) {
    return clamp(noise3(x * FERT_FREQ, y * FERT_FREQ, (float)lf_seed + 400.0f), 0.0f, 1.0f);
}

// ── MODIFIER: collapse — partially destroy a footprint (phase 5) ──────────────
// The first MODIFIER: it reads the maker's INTACT massing (the body rect, re-derived
// by footprint_body) and TRANSFORMS it — punching the roof open to bare foundation
// and spilling rubble around the shell, scaled by decay. Genuinely new because age
// weathers a surface but can't invent ABSENCE: only a modifier that knows the
// original massing knows what's gone. Pure fn of (body, hash, decay) — no globals.
static void collapse_fill(Rect body, unsigned h, float decay) {
    if (decay < 0.30f) return;                                  // intact below the threshold
    float d = (decay - 0.30f) / 0.70f;                          // 0..1 within the ruined band
    int bites = 1 + (int)(d * 5.0f);                            // roof openings (grow with decay)
    for (int i = 0; i < bites; i++) {
        unsigned bh = hash2((int)h + i * 131, (int)(h >> 7) + i * 977);
        int bw = 1 + (int)(frac01(bh)        * (body.w * 0.5f));
        int bhh= 1 + (int)(frac01(bh >> 8)   * (body.h * 0.5f));
        int bx = body.x + (int)(frac01(bh >> 16) * (body.w - bw));
        int by = body.y + (int)(frac01(bh >> 22) * (body.h - bhh));
        rectfill(bx, by, bw, bhh, CLR_BROWNISH_BLACK);          // exposed foundation/interior
    }
    int rub = (int)(d * 16.0f);                                 // rubble spilled around the shell
    for (int i = 0; i < rub; i++) {
        unsigned rh = hash2((int)h * 7 + i, (int)(h >> 3) - i * 53);
        int rx = body.x - 2 + (int)(frac01(rh)       * (body.w + 4));
        int ry = body.y - 2 + (int)(frac01(rh >> 11) * (body.h + 4));
        pset(rx, ry, (rh & 1) ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
    }
}

// ── MODIFIER: overgrow — creep growth into the wreckage (phase 5) ─────────────
// A `spread` aimed at BUILT features (ivy / moss / scrub), not open cover. Density
// scales with decay × the fertility field: a ruin in a fertile trough is swallowed
// green, one on barren scree stays bare. Runs AFTER collapse, so growth lands in the
// holes it opened — the ordered pipeline (footprint → collapse → overgrow) in one fn.
static void overgrow_fill(Rect body, unsigned h, float decay, float fert) {
    float g = decay * (0.30f + 0.70f * fert);
    if (g < 0.08f) return;
    int n = (int)(g * body.w * body.h * 0.09f);
    if (n > 64) n = 64;                                         // cull (perf + readability)
    for (int i = 0; i < n; i++) {
        unsigned gh = hash2((int)h * 13 + i * 31, (int)(h >> 5) + i * 17);
        int gx = body.x + (int)(frac01(gh)       * body.w);
        int gy = body.y + (int)(frac01(gh >> 11) * body.h);
        pset(gx, gy, (gh & 3) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN);
        if ((gh & 7) == 0) pset(gx, gy - 1, CLR_LIME_GREEN);    // a sprig poking up
    }
}

// ── PIPELINE: ruins — footprint → collapse → overgrow (the phase 4→5 proof) ───
// Not a new atom — the first MODIFIER PIPELINE (docs/design/streetlevel-content.md →
// "The wilds": "Ruins = footprint → collapse → overgrow, in that order"). The same
// footprint_fill makes each building; a DECAY field then drives two modifiers that
// transform that output. era-zoning falls out for free — a fresh block in one region,
// a fully-reclaimed one in another, from the same atoms. Layers peel the three STAGES.
enum { RU_BUILD, RU_COLLAPSE, RU_OVERGROW };

static void ruins_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - BLK_P, Rpx = (int)(cx + hwv) + BLK_P;
    int Tpx = (int)(cy - hhv) - BLK_P, Bpx = (int)(cy + hhv) + BLK_P;

    bool zc   = zoom >= 0.5f;
    bool build = layer_on[RU_BUILD]    && zc;
    bool coll  = layer_on[RU_COLLAPSE] && zc;
    bool over  = layer_on[RU_OVERGROW] && zc;
    bool show[3] = { false, build, false };                    // footprint_fill: build layer only
    int b0x = ifloordiv(Lpx, BLK_P), b1x = ifloordiv(Rpx, BLK_P);
    int b0y = ifloordiv(Tpx, BLK_P), b1y = ifloordiv(Bpx, BLK_P);
    int IN = BLK_P - ST_W, nlot = 3, ls = IN / nlot, n_ruin = 0, n_gone = 0;
    for (int by = b0y; by <= b1y; by++)
        for (int bx = b0x; bx <= b1x; bx++) {
            int ox = bx * BLK_P, oy = by * BLK_P, inx = ox + ST_W, iny = oy + ST_W;
            int zone = zone_at(inx + IN / 2.0f, iny + IN / 2.0f);
            rectfill(ox, oy, BLK_P, BLK_P, CLR_DARK_GREY);                       // street band
            rectfill(inx, iny, IN, IN, zone == ZN_RES ? CLR_DARK_GREEN : CLR_MEDIUM_GREY);
            for (int lj = 0; lj < nlot; lj++)
                for (int li = 0; li < nlot; li++) {
                    if (li == 1 && lj == 1) {                                    // centre courtyard
                        if (zone != ZN_RES) rectfill(inx + li * ls, iny + lj * ls, ls, ls, CLR_DARKER_GREY);
                        continue;
                    }
                    Rect lot = { inx + li * ls, iny + lj * ls, ls, ls };
                    int outward = (lj == 0) ? 0 : (lj == 2) ? 2 : (li == 0) ? 3 : 1;
                    unsigned hh = hash2(bx * 97 + li + lf_seed * 7, by * 89 + lj);
                    Rect body; if (!footprint_body(lot, outward, zone, &body)) continue;
                    float fx = body.x + body.w / 2.0f, fy = body.y + body.h / 2.0f;
                    float dec = decay_at(fx, fy), fert = fertility_at(fx, fy);
                    footprint_fill(lot, outward, zone, hh, show);               // stage 1: maker
                    if (coll) collapse_fill(body, hh, dec);                     // stage 2: modifier
                    if (over) overgrow_fill(body, hh, dec, fert);              // stage 3: modifier
                    if (dec >= 0.30f) n_ruin++;
                    if (dec >= 0.85f) n_gone++;
                }
        }

    if (dbg) {                                                                  // the DECAY field heatmap
        int OV = 12; fillp(0xA5A5, -1);
        for (int wy = ifloordiv(Tpx, OV) * OV; wy <= Bpx; wy += OV)
            for (int wx = ifloordiv(Lpx, OV) * OV; wx <= Rpx; wx += OV)
                rectfill(wx, wy, OV, OV, dens_ramp(decay_at(wx + OV / 2.0f, wy + OV / 2.0f)));
        fillp_reset();
    }
    cnt[0] = n_ruin; cnt_lbl[0] = "ruined";
    cnt[1] = n_gone; cnt_lbl[1] = "razed";
    cnt_lbl[2] = NULL;
}

static const char *ruins_probe(float x, float y) {
    static char buf[44];
    float dec = decay_at(x, y), fert = fertility_at(x, y);
    const char *s = dec < 0.30f ? "intact" : dec < 0.60f ? "damaged" : dec < 0.85f ? "ruined" : "razed";
    snprintf(buf, sizeof buf, "%s  decay %.2f  fert %.2f", s, (double)dec, (double)fert);
    return buf;
}

// ── ATOM: world ──────────────────────────────────────────────────────────────
// Not a new fill-rule — the DRIVER. A dead-simple local worldgen that COMPOSES the
// three atoms: a SELECTOR (world_kind_at) reads the terrain + an urbanization field
// and picks, per region, which atom fills it — wild land → scatter, farmland →
// rows, town → footprint. This is the "fill a whole world" half, self-contained
// (no roadnet/worldgen dependency yet): the same scatter/rows_fill/footprint_fill
// run here, gated by the selector. Layers peel the three DOMAINS (nature/farm/city).
#define URB_FREQ 0.0045f         // urbanization frequency (smaller = bigger regions)
enum { WK_NATURE, WK_FARM, WK_CITY };
static const char *WK_NAME[3] = { "wild", "farmland", "city" };

static int world_kind_at(float x, float y) {
    int t = cover_at(x, y).kind;
    if (t == CV_WATER || t == CV_SAND || t == CV_ROCK) return WK_NATURE;   // terrain stays wild
    float u = noise3(x * URB_FREQ, y * URB_FREQ, (float)lf_seed + 200.0f);
    if (u > 0.66f) return WK_CITY;
    if (u > 0.44f) return WK_FARM;
    return WK_NATURE;
}

static void world_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - BLK_P, Rpx = (int)(cx + hwv) + BLK_P;
    int Tpx = (int)(cy - hhv) - BLK_P, Bpx = (int)(cy + hhv) + BLK_P;
    int n_nat = 0, n_farm = 0, n_city = 0;

    // base terrain tint everywhere (coarse cells that grow when zoomed out)
    int gc = 6, step = 1; while ((float)gc * step * zoom < 3.0f && step < 8) step++;
    int cs = gc * step;
    for (int gy = ifloordiv(Tpx, cs); gy <= ifloordiv(Bpx, cs); gy++)
        for (int gx = ifloordiv(Lpx, cs); gx <= ifloordiv(Rpx, cs); gx++) {
            int px = gx * cs, py = gy * cs; Cover cv = cover_at(px + cs / 2.0f, py + cs / 2.0f);
            rectfill(px, py, cs, cs, ground_col(gx, gy, &cv));
        }

    // NATURE — the flavor-driven scatter on wild land (woodland/beach/scree/waste/…)
    if (layer_on[0] && zoom >= 0.30f)
        for (int cyi = ifloordiv(Tpx, CANOPY_P); cyi <= ifloordiv(Bpx, CANOPY_P); cyi++)
            for (int cxi = ifloordiv(Lpx, CANOPY_P); cxi <= ifloordiv(Rpx, CANOPY_P); cxi++) {
                unsigned h = hash2(cxi * 5 + lf_seed * 17, cyi + 3);
                int wx = cxi * CANOPY_P + (int)(frac01(h) * CANOPY_P), wy = cyi * CANOPY_P + (int)(frac01(h >> 12) * CANOPY_P);
                if (world_kind_at(wx, wy) == WK_NATURE) n_nat += scatter_canopy(wx, wy, h);
            }

    // FARM — fields on farmland parcels (reuses rows_fill)
    if (layer_on[1]) {
        bool show[3] = { true, zoom >= 0.45f, true };
        for (int fy = ifloordiv(Tpx, FIELD_P); fy <= ifloordiv(Bpx, FIELD_P); fy++)
            for (int fx = ifloordiv(Lpx, FIELD_P); fx <= ifloordiv(Rpx, FIELD_P); fx++) {
                if (world_kind_at(fx * FIELD_P + FIELD_P / 2.0f, fy * FIELD_P + FIELD_P / 2.0f) != WK_FARM) continue;
                Rect r = { fx * FIELD_P, fy * FIELD_P, FIELD_P, FIELD_P };
                rows_fill(r, hash2(fx + lf_seed * 257, fy * 7 + 13), show); n_farm++;
            }
    }

    // CITY — buildings on town blocks (reuses footprint_fill)
    if (layer_on[2]) {
        bool show[3] = { true, zoom >= 0.45f, zoom >= 0.9f };
        int IN = BLK_P - ST_W, nlot = 3, ls = IN / nlot;
        for (int by = ifloordiv(Tpx, BLK_P); by <= ifloordiv(Bpx, BLK_P); by++)
            for (int bx = ifloordiv(Lpx, BLK_P); bx <= ifloordiv(Rpx, BLK_P); bx++) {
                int ox = bx * BLK_P, oy = by * BLK_P, inx = ox + ST_W, iny = oy + ST_W;
                if (world_kind_at(inx + IN / 2.0f, iny + IN / 2.0f) != WK_CITY) continue;
                int zone = zone_at(inx + IN / 2.0f, iny + IN / 2.0f);
                rectfill(ox, oy, BLK_P, BLK_P, CLR_DARK_GREY);
                rectfill(inx, iny, IN, IN, zone == ZN_RES ? CLR_DARK_GREEN : CLR_MEDIUM_GREY);
                for (int lj = 0; lj < nlot; lj++)
                    for (int li = 0; li < nlot; li++) {
                        if (li == 1 && lj == 1) { if (zone != ZN_RES) rectfill(inx + li * ls, iny + lj * ls, ls, ls, CLR_DARKER_GREY); continue; }
                        Rect lot = { inx + li * ls, iny + lj * ls, ls, ls };
                        int outward = (lj == 0) ? 0 : (lj == 2) ? 2 : (li == 0) ? 3 : 1;
                        footprint_fill(lot, outward, zone, hash2(bx * 97 + li + lf_seed * 7, by * 89 + lj), show);
                    }
                n_city++;
            }
    }

    if (dbg) {                                            // the SELECTOR map: tint each region by kind
        static const int WC[3] = { CLR_DARK_GREEN, CLR_YELLOW, CLR_RED };
        int OV = 12; fillp(0xA5A5, -1);
        for (int wy = ifloordiv(Tpx, OV) * OV; wy <= Bpx; wy += OV)
            for (int wx = ifloordiv(Lpx, OV) * OV; wx <= Rpx; wx += OV)
                rectfill(wx, wy, OV, OV, WC[world_kind_at(wx + OV / 2.0f, wy + OV / 2.0f)]);
        fillp_reset();
    }
    cnt[0] = n_nat;  cnt_lbl[0] = "trees";
    cnt[1] = n_farm; cnt_lbl[1] = "fields";
    cnt[2] = n_city; cnt_lbl[2] = "blocks";
}

static const char *world_probe(float x, float y) {
    static char buf[40];
    snprintf(buf, sizeof buf, "%s / %s", WK_NAME[world_kind_at(x, y)], CV_NAME[cover_at(x, y).kind]);
    return buf;
}

// ── the bounded-atom demo driver (also realises `subdivide`) ──────────────────
// border/pave/stamp all fill ONE bounded region. This tiles a plot grid over the
// view and calls the atom's fill per plot — the grid IS the subdivide primitive.
#define BORDER_P 28
#define PAVE_P   30
#define STAMP_P  34
typedef void (*FillFn)(Rect r, unsigned h, const bool show[3]);
static unsigned plot_hash(int fx, int fy) { return hash2(fx + lf_seed * 149, fy * 131 + 5); }

static void demo_grid(float cam_x, float cam_y, float zoom, int P, FillFn fn, const char *label) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - P, Rpx = (int)(cx + hwv) + P, Tpx = (int)(cy - hhv) - P, Bpx = (int)(cy + hhv) + P;
    bool show[3] = { layer_on[0], layer_on[1], layer_on[2] };
    int n = 0;
    for (int fy = ifloordiv(Tpx, P); fy <= ifloordiv(Bpx, P); fy++)
        for (int fx = ifloordiv(Lpx, P); fx <= ifloordiv(Rpx, P); fx++) {
            Rect r = { fx * P, fy * P, P, P };
            fn(r, plot_hash(fx, fy), show);
            n++;
        }
    if (dbg) {
        for (int fx = ifloordiv(Lpx, P); fx <= ifloordiv(Rpx, P) + 1; fx++) line(fx * P, Tpx, fx * P, Bpx, CLR_DARK_RED);
        for (int fy = ifloordiv(Tpx, P); fy <= ifloordiv(Bpx, P) + 1; fy++) line(Lpx, fy * P, Rpx, fy * P, CLR_DARK_RED);
        rect(ifloordiv((int)cx, P) * P, ifloordiv((int)cy, P) * P, P, P, CLR_YELLOW);
    }
    cnt[0] = n; cnt_lbl[0] = label; cnt_lbl[1] = NULL; cnt_lbl[2] = NULL;
}

// ── ATOM: border — stroke a region edge with a hedge / fence / wall ───────────
// Layers: 0 plot (interior) · 1 edge (the stroke). (Promoted from rows' hedgerow.)
static void border_fill(Rect r, unsigned h, const bool show[3]) {
    int style = h % 3;                                          // 0 hedge · 1 fence · 2 wall
    if (show[0])
        rectfill(r.x, r.y, r.w, r.h, style == 0 ? CLR_DARK_GREEN : style == 1 ? CLR_MEDIUM_GREEN : CLR_DARK_BROWN);
    if (!show[1]) return;
    int x1 = r.x + r.w - 1, y1 = r.y + r.h - 1;
    if (style == 0) {                                          // hedge: bushy 2px ring
        for (int x = r.x; x <= x1; x++) {
            int c = ((x + r.y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
            pset(x, r.y, c); pset(x, r.y + 1, CLR_LIME_GREEN); pset(x, y1, c); pset(x, y1 - 1, CLR_LIME_GREEN);
        }
        for (int y = r.y; y <= y1; y++) {
            int c = ((r.x + y) & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
            pset(r.x, y, c); pset(r.x + 1, y, CLR_LIME_GREEN); pset(x1, y, c); pset(x1 - 1, y, CLR_LIME_GREEN);
        }
    } else if (style == 1) {                                   // fence: rail + posts
        rect(r.x, r.y, r.w, r.h, CLR_BROWN);
        for (int x = r.x; x <= x1; x += 4) { pset(x, r.y, CLR_DARK_BROWN); pset(x, y1, CLR_DARK_BROWN); }
        for (int y = r.y; y <= y1; y += 4) { pset(r.x, y, CLR_DARK_BROWN); pset(x1, y, CLR_DARK_BROWN); }
    } else {                                                   // wall: dithered stone
        for (int x = r.x; x <= x1; x++) { int c = ((x >> 1) & 1) ? CLR_LIGHT_GREY : CLR_DARK_GREY; pset(x, r.y, c); pset(x, y1, c); }
        for (int y = r.y; y <= y1; y++) { int c = ((y >> 1) & 1) ? CLR_LIGHT_GREY : CLR_DARK_GREY; pset(r.x, y, c); pset(x1, y, c); }
    }
}
static void border_draw(float cx, float cy, float z) { demo_grid(cx, cy, z, BORDER_P, border_fill, "plots"); }
static const char *border_probe(float x, float y) {
    static char b[28]; static const char *S[3] = { "hedge", "fence", "wall" };
    snprintf(b, sizeof b, "border: %s", S[plot_hash(ifloordiv((int)x, BORDER_P), ifloordiv((int)y, BORDER_P)) % 3]);
    return b;
}

// ── ATOM: pave — flat surface fill (asphalt / plaza / gravel / court) ─────────
// Layers: 0 surface · 1 markings · 2 curb.
static void pave_fill(Rect r, unsigned h, const bool show[3]) {
    int surf = h % 4;                                          // 0 asphalt · 1 plaza · 2 gravel · 3 court
    int m = 2, ix = r.x + m, iy = r.y + m, iw = r.w - 2 * m, ih = r.h - 2 * m;
    if (show[0]) {
        rectfill(r.x, r.y, r.w, r.h, surf == 0 ? CLR_DARKER_GREY : surf == 1 ? CLR_LIGHT_GREY : surf == 2 ? CLR_DARK_BROWN : CLR_BLUE_GREEN);
        if (surf == 2)                                         // gravel speckle
            for (int y = r.y; y < r.y + r.h; y += 2)
                for (int x = r.x + (y & 1); x < r.x + r.w; x += 2)
                    if ((hash2(x, y + lf_seed) & 3) == 0) pset(x, y, CLR_MEDIUM_GREY);
    }
    if (show[1] && iw > 1 && ih > 1) {
        if (surf == 0) for (int x = ix; x < ix + iw; x += 6) line(x, iy, x, iy + ih - 1, CLR_LIGHT_YELLOW);  // stalls
        else if (surf == 1) {                                  // plaza tile grid
            for (int x = r.x; x < r.x + r.w; x += 4) line(x, r.y, x, r.y + r.h - 1, CLR_MEDIUM_GREY);
            for (int y = r.y; y < r.y + r.h; y += 4) line(r.x, y, r.x + r.w - 1, y, CLR_MEDIUM_GREY);
        } else if (surf == 3) { rect(ix, iy, iw, ih, CLR_WHITE); line(ix, iy + ih / 2, ix + iw - 1, iy + ih / 2, CLR_WHITE); }
    }
    if (show[2]) rect(r.x, r.y, r.w, r.h, CLR_LIGHT_GREY);     // curb
}
static void pave_draw(float cx, float cy, float z) { demo_grid(cx, cy, z, PAVE_P, pave_fill, "plots"); }
static const char *pave_probe(float x, float y) {
    static char b[28]; static const char *S[4] = { "asphalt", "plaza", "gravel", "court" };
    snprintf(b, sizeof b, "pave: %s", S[plot_hash(ifloordiv((int)x, PAVE_P), ifloordiv((int)y, PAVE_P)) % 4]);
    return b;
}

// ── ATOM: stamp — drop one authored composite at the region's anchor ──────────
// Layers: 0 ground · 1 shadow · 2 prop.
static void stamp_fill(Rect r, unsigned h, const bool show[3]) {
    int kind = h % 4;                                          // 0 fountain · 1 statue · 2 well · 3 obelisk
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2, R = (r.w < r.h ? r.w : r.h) / 2 - 3;
    if (R < 2) return;
    if (show[0]) rectfill(r.x, r.y, r.w, r.h, CLR_LIGHT_GREY); // plaza ground
    if (show[1]) circfill(cx + 1, cy + 2, R - 1, CLR_MEDIUM_GREY);  // shadow
    if (!show[2]) return;
    switch (kind) {
        case 0: circfill(cx, cy, R, CLR_LIGHT_GREY); circfill(cx, cy, R - 2, CLR_BLUE); circfill(cx, cy, 1, CLR_WHITE); break;
        case 1: rectfill(cx - 2, cy - 1, 4, R + 1, CLR_DARK_GREY); circfill(cx, cy - R + 2, 2, CLR_LIGHT_GREY); break;
        case 2: circ(cx, cy, R, CLR_DARK_BROWN); circfill(cx, cy, R - 1, CLR_DARKER_GREY); line(cx - R, cy - R, cx + R, cy - R, CLR_BROWN); break;
        case 3: rectfill(cx - 1, cy - R, 3, 2 * R, CLR_MEDIUM_GREY); pset(cx, cy - R - 1, CLR_WHITE); break;
    }
}
static void stamp_draw(float cx, float cy, float z) { demo_grid(cx, cy, z, STAMP_P, stamp_fill, "props"); }
static const char *stamp_probe(float x, float y) {
    static char b[28]; static const char *S[4] = { "fountain", "statue", "well", "obelisk" };
    snprintf(b, sizeof b, "stamp: %s", S[plot_hash(ifloordiv((int)x, STAMP_P), ifloordiv((int)y, STAMP_P)) % 4]);
    return b;
}

// ════════════════════════════════════════════════════════════════════════════
//  THE ATOM REGISTRY — each atom is a tab
// ════════════════════════════════════════════════════════════════════════════
typedef struct {
    const char *name;
    int n_layers;
    const char *layer[MAX_LAYERS];
    void (*draw)(float cam_x, float cam_y, float zoom);
    const char *(*probe)(float wx, float wy);
} Atom;

static Atom atoms[] = {
    { "WORLD",     3, { "nature", "farms", "cities" }, world_draw,     world_probe },
    { "SCATTER",   3, { "base", "ground", "canopy" }, scat_draw,      scat_probe },
    { "ROWS",      3, { "soil", "crop",   "border" }, rows_draw,      rows_probe },
    { "FOOTPRINT", 3, { "ground", "build", "detail" }, footprint_draw, footprint_probe },
    { "RUINS",     3, { "build", "collapse", "overgrow" }, ruins_draw, ruins_probe },
    { "BORDER",    2, { "plot", "edge" },             border_draw,    border_probe },
    { "PAVE",      3, { "surface", "marks", "curb" }, pave_draw,      pave_probe },
    { "STAMP",     3, { "ground", "shadow", "prop" }, stamp_draw,     stamp_probe },
};
#define N_ATOMS ((int)(sizeof atoms / sizeof atoms[0]))

// ════════════════════════════════════════════════════════════════════════════
//  THE SHELL — free-fly explorer over the current atom
// ════════════════════════════════════════════════════════════════════════════
static float cam_x = 250, cam_y = 18;      // open on a forest/farm/town world (seed 1)
static float zoom  = 0.70f;
static int   seed  = 1;
static int   cur   = 0;                   // current atom

void init(void) { lf_seed = seed; }

void update(void) {
    float pan = 5.0f / zoom;
    if (key(KEY_RIGHT) || key('D')) cam_x += pan;
    if (key(KEY_LEFT)  || key('A')) cam_x -= pan;
    if (key(KEY_DOWN)  || key('S')) cam_y += pan;
    if (key(KEY_UP)    || key('W')) cam_y -= pan;
    if (key('Z')) zoom *= 1.04f;
    if (key('X')) zoom /= 1.04f;
    zoom += mouse_wheel() * 0.08f * zoom;
    zoom = clamp(zoom, 0.18f, 6.0f);
    if (keyp('R')) { seed++; lf_seed = seed; }
    if (keyp(KEY_TAB)) { cur = (cur + 1) % N_ATOMS;             // next atom (resets layers)
                         for (int i = 0; i < MAX_LAYERS; i++) layer_on[i] = true; }
    for (int i = 0; i < atoms[cur].n_layers; i++)              // 1/2/3 peel layers
        if (keyp('1' + i)) layer_on[i] = !layer_on[i];
    if (keyp('G')) dbg = !dbg;
    if (keyp('O')) ovl = (ovl + 1) % 3;
}

// the field that drives the atom, drawn as a semi-transparent tint over the result
static void draw_overlay(void) {
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int OV = 12;
    int L = ((int)(cx - hwv) / OV - 1) * OV, R = (int)(cx + hwv) + OV;
    int T = ((int)(cy - hhv) / OV - 1) * OV, B = (int)(cy + hhv) + OV;
    fillp(0xA5A5, -1);
    for (int wy = T; wy <= B; wy += OV)
        for (int wx = L; wx <= R; wx += OV) {
            Cover cv = cover_at(wx + OV / 2.0f, wy + OV / 2.0f);
            rectfill(wx, wy, OV, OV, ovl == 1 ? KIND_COL[cv.kind] : dens_ramp(cv.dens));
        }
    fillp_reset();
}

void draw(void) {
    cls(CLR_BLACK);
    camera_ex((int)cam_x, (int)cam_y, zoom, 0);
    atoms[cur].draw(cam_x, cam_y, zoom);
    if (ovl) draw_overlay();

    camera(0, 0);
    int mx = SCREEN_W / 2, my = SCREEN_H / 2;
    line(mx - 5, my, mx + 5, my, CLR_WHITE);
    line(mx, my - 5, mx, my + 5, CLR_WHITE);

    // top bar — atom + seed/zoom/overlay
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    char buf[72];
    static const char *OVN[3] = { "off", "kind", "dens" };
    int x = print(atoms[cur].name, 3, 1, CLR_YELLOW);
    snprintf(buf, sizeof buf, "  s%d  z%.1f  overlay:%s", seed, (double)zoom, OVN[ovl]);
    print(buf, x, 1, CLR_LIGHT_GREY);

    // layer chips + live counts (small font)
    rectfill(0, 9, SCREEN_W, 8, CLR_BLACK);
    font(FONT_SMALL);
    int lx = 3;
    for (int i = 0; i < atoms[cur].n_layers; i++) {
        snprintf(buf, sizeof buf, "%d:%s ", i + 1, atoms[cur].layer[i]);
        lx = print(buf, lx, 11, layer_on[i] ? CLR_GREEN : CLR_DARK_GREY);
    }
    lx += 6;
    for (int i = 0; i < 3; i++)
        if (cnt_lbl[i]) {
            snprintf(buf, sizeof buf, "%s:%d ", cnt_lbl[i], cnt[i]);
            lx = print(buf, lx, 11, CLR_LIGHT_GREY);
        }

    // bottom — probe + controls
    rectfill(0, SCREEN_H - 16, SCREEN_W, 16, CLR_BLACK);
    font(FONT_NORMAL);
    print(atoms[cur].probe(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f), 3, SCREEN_H - 16, CLR_WHITE);
    font(FONT_SMALL);
    print("WASD/ZX move  R seed  TAB atom  1-3 layers  G grid  O overlay",
          3, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
