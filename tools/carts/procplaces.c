#include "studio.h"
#include <stdio.h>
#include <math.h>

// ============================================================================
// PROCEDURAL PLACES — a testbed for procedural-generation.
//
//   ◄ ► ▲ ▼ / WASD   pan the camera
//   Z / X            zoom in / out   (mouse wheel too)
//   R                new seed  (a fresh, repeatable world)
//   1 / 2            switch generator
//   TAB              field overlay (tint the world by its underlying field)
//
// ── what this is ─────────────────────────────────────────────────────────────
// One free-fly explorer shell that hosts PLUGGABLE generators. Each generator is
// a SECTION below exposing the same little vtable — draw / reseed / probe /
// field_col — so the shell never knows or cares what it's drawing. Ships with two
// structurally-opposite generators to prove the seam is real:
//
//   1. ROADS & CITIES — TWO orthogonal fields (SimCity-2000 model): a LAND-USE
//      field (residential/commercial/industrial/green) crossed with an INTENSITY
//      field (wild/light/medium/heavy). The pair picks what a block looks like;
//      a nested road lattice is promoted by class (local→avenue→arterial→highway)
//      and culled by local intensity. Both fields are domain-warped so districts
//      are organic blobs, not bullseyes.
//   2. TERRAIN        — a pure scalar FIELD: fbm elevation rendered as coloured
//      bands (sea → beach → grass → forest → rock → snow). No structure.
//
// Adding forest / dungeon / cave is just another section + a row in gens[].
//
// ── why one cart, not runtime/ headers (yet) ─────────────────────────────────
// These generators COULD be cart-land library headers (ADR-0006), but that ships
// per the second-customer rule — a real second cart using each. Until one exists
// (e.g. sloop consuming the road field for collision), they live here as in-file
// sections: cleaner to tweak, and honest about reuse. EXTRACT WHEN NEEDED — lift a
// section into runtime/<name>.h (make every fn `static`) the day a second cart
// includes it.  Design notes: docs/design/procgen-places.md.
//
// ── seeded & repeatable ──────────────────────────────────────────────────────
// Everything is a pure function of world position + seed. The engine's noise is
// VALUE noise (not Perlin) and noise/noise2 take no seed — but noise2(x,y) ==
// noise3(x,y,0), so noise3(x, y, (float)seed) with an INTEGER seed is a fully
// independent, repeatable field (the z-slice hashes through noise_hash on its own;
// integer z = no interpolation bleed). Same seed → byte-identical world every run.
// ============================================================================

// ════════════════════════════════════════════════════════════════════════════
//  GENERATOR 1 — ROADS & CITIES   (two orthogonal fields + a classed lattice)
// ════════════════════════════════════════════════════════════════════════════
// THE WRONG TURN TO AVOID (docs/design/procgen-places.md): draw() and road_at()
// must read the SAME geometry, so all the sampling/visibility logic lives in two
// shared helpers — roads_zone_at() (what's here) and roads_line_render() (is this
// lattice line drawn, and how wide). Both the renderer and the query call them, so
// a future consumer (sloop's collision) can never disagree with what's on screen.
#define ROADS_BASE 100        // finest lattice pitch (px) = one city block
#define ROADS_IFREQ 0.00045f  // intensity-field frequency (smaller = bigger cities)
#define ROADS_LFREQ 0.00080f  // land-use-field frequency (smaller = bigger districts)
#define ROADS_WFREQ 0.00060f  // domain-warp frequency
#define ROADS_WAMP  210.0f    // domain-warp amplitude (px) — how organic the blobs

enum { LU_GREEN, LU_RES, LU_COM, LU_IND, LU_N };          // land use
enum { LV_WILD, LV_LIGHT, LV_MED, LV_HEAVY };             // intensity level
enum { RC_LOCAL, RC_AVENUE, RC_ARTERIAL, RC_HIGHWAY };    // road class (by lattice level)

static const char *LU_NAME[LU_N] = { "GREEN", "RES", "COM", "IND" };
static const char *LV_NAME[4]    = { "wild", "light", "med", "heavy" };

static int roads_seed = 0;
static void roads_reseed(int seed) { roads_seed = seed; }

typedef struct { int lu; int level; float intensity; } Zone;   // "what kind of place"
typedef struct { Zone z; bool on_road; int road_class; } Place; // + "is there a road here"

static int roads_ifloordiv(int a, int b) {        // floor division (handles negatives)
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}
static unsigned roads_hash2(int a, int b) {       // per-cell pseudo-random (houses, fields)
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

// ── the two fields, domain-warped ────────────────────────────────────────────
// warp bends the SAMPLE point; the lattice geometry stays straight, so roads keep
// their nesting (no tearing) while the ZONES they pass through go wavy & organic.
static void roads_warp(float x, float y, float *ox, float *oy) {
    float z = (float)roads_seed;
    *ox = x + (noise3(x * ROADS_WFREQ, y * ROADS_WFREQ, z + 30.0f) - 0.5f) * 2.0f * ROADS_WAMP;
    *oy = y + (noise3(x * ROADS_WFREQ, y * ROADS_WFREQ, z + 40.0f) - 0.5f) * 2.0f * ROADS_WAMP;
}
static float roads_intensity_raw(float x, float y) {     // fbm, 2 octaves
    float z = (float)roads_seed;
    float a = noise3(x * ROADS_IFREQ,        y * ROADS_IFREQ,        z);
    float b = noise3(x * ROADS_IFREQ * 2.7f, y * ROADS_IFREQ * 2.7f, z + 1.0f);
    return clamp(a * 0.68f + b * 0.32f, 0.0f, 1.0f);
}
static int roads_level_of(float i) {
    if (i < 0.42f) return LV_WILD;
    if (i < 0.60f) return LV_LIGHT;
    if (i < 0.77f) return LV_MED;
    return LV_HEAVY;
}
static int roads_landuse_of(float x, float y) {
    float r = noise3(x * ROADS_LFREQ, y * ROADS_LFREQ, (float)roads_seed + 70.0f);
    if (r < 0.30f) return LU_RES;
    if (r < 0.56f) return LU_COM;
    if (r < 0.80f) return LU_IND;
    return LU_GREEN;                                       // parks even inside a city
}

// THE shared sampler — every place-query goes through here (warp once, read both).
static Zone roads_zone_at(float x, float y) {
    float wx, wy;
    roads_warp(x, y, &wx, &wy);
    float in = roads_intensity_raw(wx, wy);
    int lv = roads_level_of(in);
    int lu = (lv == LV_WILD) ? LU_GREEN : roads_landuse_of(wx, wy);  // countryside = green
    return (Zone){ lu, lv, in };
}

// ── the classed lattice ──────────────────────────────────────────────────────
// A line's CLASS is a pure function of its index — nested pitch 1/3/9/27. So a
// highway is a highway wherever it runs, independent of the zone it crosses.
static int roads_class_of(int k) {
    if (k % 27 == 0) return RC_HIGHWAY;
    if (k % 9  == 0) return RC_ARTERIAL;
    if (k % 3  == 0) return RC_AVENUE;
    return RC_LOCAL;
}
// Shared visibility+width: is the line of class `cls` drawn in zone `z`, how wide,
// and is it an unpaved dirt track? Both draw and road_at call this — never inline.
static bool roads_line_render(int cls, const Zone *z, int *hw, bool *dirt) {
    *dirt = false;
    switch (cls) {
        case RC_HIGHWAY:  *hw = 44; return true;                     // skeleton: everywhere
        case RC_ARTERIAL: *hw = 20; return z->level >= LV_LIGHT;
        case RC_AVENUE:
            if (z->level >= LV_MED) { *hw = 11; return true; }
            if (z->level >= LV_LIGHT && z->lu == LU_GREEN) { *hw = 3; *dirt = true; return true; }
            return false;                                            // farm dirt lane
        default:          *hw = 7;  return z->level >= LV_HEAVY;     // local streets
    }
}

// ── block interiors (land use × intensity = the look) ────────────────────────
static void roads_houses(int wx, int wy, int span, int n, const int *roofs, int nroof) {
    int cw = span / n, ch = span / n;
    if (cw < 4 || ch < 4) return;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            unsigned h = roads_hash2(wx + i * 137 + roads_seed * 911, wy + j * 137);
            if ((h & 7) == 0) continue;                              // empty lots / yards
            int hx = wx + i * cw, hy = wy + j * ch;
            rectfill(hx + 1, hy + 1, cw - 2, ch - 2, roofs[h % nroof]);
            rect(hx + 1, hy + 1, cw - 2, ch - 2, CLR_BROWNISH_BLACK);
        }
}
static void roads_towers(int wx, int wy, int span) {                 // downtown: big lit slabs
    int slab[4] = { CLR_DARKER_GREY, CLR_DARK_GREY, CLR_DARKER_BLUE, CLR_INDIGO };
    int cells = 2, cw = span / cells;
    for (int i = 0; i < cells; i++)
        for (int j = 0; j < cells; j++) {
            unsigned h = roads_hash2(wx + i * 211 + roads_seed * 7, wy + j * 211);
            int hx = wx + i * cw + 2, hy = wy + j * cw + 2, w = cw - 4, hh = cw - 4;
            rectfill(hx, hy, w, hh, slab[h & 3]);
            rect(hx, hy, w, hh, CLR_BLACK);
            for (int wyy = hy + 2; wyy < hy + hh - 2; wyy += 4)        // lit windows
                for (int wxx = hx + 2; wxx < hx + w - 2; wxx += 3)
                    if ((roads_hash2(wxx, wyy + (int)h) & 3) == 0)
                        pset(wxx, wyy, CLR_LIGHT_YELLOW);
        }
}
static void roads_factory(int wx, int wy, int span) {                // industrial: big footprints
    int body[3] = { CLR_DARKER_GREY, CLR_DARK_BROWN, CLR_DARKER_PURPLE };
    unsigned h = roads_hash2(wx + roads_seed * 13, wy);
    rectfill(wx + 3, wy + 3, span - 6, span - 6, body[h % 3]);
    rect(wx + 3, wy + 3, span - 6, span - 6, CLR_BROWNISH_BLACK);
    for (int s = wx + 8; s < wx + span - 8; s += 9)                   // sawtooth roof / vents
        line(s, wy + 6, s, wy + span - 7, CLR_BROWNISH_BLACK);
    if (h & 1) circfill(wx + span - 12, wy + 11, 4, CLR_MEDIUM_GREY); // tank
}
static void roads_park(int wx, int wy, int span, int level) {        // green: farm or park
    if (level == LV_WILD) {                                          // wilderness: scattered trees
        for (int i = 0; i < 5; i++) {
            unsigned h = roads_hash2(wx + i * 53 + roads_seed, wy - i * 91);
            if (h & 1) continue;
            int tx = wx + 8 + (int)(h % (span - 16)), ty = wy + 8 + (int)((h >> 8) % (span - 16));
            circfill(tx, ty, 3 + (h & 1), CLR_MEDIUM_GREEN);
        }
        return;
    }
    unsigned hf = roads_hash2(wx + roads_seed, wy);                   // farm patchwork / park lawn
    int g = (hf & 2) ? CLR_MEDIUM_GREEN : CLR_LIME_GREEN;
    rectfill(wx + 4, wy + 4, span - 8, span - 8, g);
    for (int i = 0; i < 3; i++) {
        unsigned h = roads_hash2(wx + i * 47, wy + i * 71 + roads_seed);
        circfill(wx + 10 + (int)(h % (span - 20)), wy + 10 + (int)((h >> 7) % (span - 20)),
                 3, CLR_DARK_GREEN);
    }
}

static void roads_fill_block(int wx, int wy, int span, const Zone *z) {
    static const int RES_ROOF[5] = { CLR_BROWN, CLR_RED, CLR_DARK_RED, CLR_DARK_PURPLE, CLR_PEACH };
    static const int COM_ROOF[4] = { CLR_DARK_GREY, CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_INDIGO };
    int in = wx + 4, iw = span - 8;                                  // 4px gutter; roads overpaint
    switch (z->lu) {
        case LU_RES:
            roads_houses(in, in - wx + wy, iw, z->level == LV_LIGHT ? 2 : z->level == LV_MED ? 3 : 4,
                         RES_ROOF, 5);
            break;
        case LU_COM:
            if (z->level >= LV_HEAVY) roads_towers(wx, wy, span);
            else roads_houses(in, in - wx + wy, iw, z->level == LV_LIGHT ? 2 : 3, COM_ROOF, 4);
            break;
        case LU_IND:
            roads_factory(wx, wy, span);
            break;
        default:  // LU_GREEN
            roads_park(wx, wy, span, z->level);
            break;
    }
}

// dashed centre line down a vertical span / across a horizontal span
static void roads_dashes_v(int x, int t, int b, int col) {
    for (int y = roads_ifloordiv(t, 24) * 24; y < b; y += 24) line(x, y, x, y + 11, col);
}
static void roads_dashes_h(int y, int l, int r, int col) {
    for (int x = roads_ifloordiv(l, 24) * 24; x < r; x += 24) line(x, y, x + 11, y, col);
}

// draw the vertical lattice line owned by this cell (its LEFT edge), top→bottom of cell
static void roads_road_v(int wx, int wy, int kx, const Zone *z) {
    int hw; bool dirt;
    int cls = roads_class_of(kx);
    if (!roads_line_render(cls, z, &hw, &dirt)) return;
    int col = dirt ? CLR_DARK_BROWN : (cls == RC_HIGHWAY ? CLR_BROWNISH_BLACK : CLR_DARK_GREY);
    rectfill(wx - hw, wy, hw * 2, ROADS_BASE, col);
    if (z->lu != LU_GREEN && z->level >= LV_MED && cls <= RC_ARTERIAL && !dirt) {   // pavement
        rectfill(wx - hw - 2, wy, 2, ROADS_BASE, CLR_MEDIUM_GREY);
        rectfill(wx + hw,     wy, 2, ROADS_BASE, CLR_MEDIUM_GREY);
    }
    if (cls == RC_AVENUE && !dirt) {
        line(wx - hw, wy, wx - hw, wy + ROADS_BASE, CLR_LIGHT_GREY);
        line(wx + hw, wy, wx + hw, wy + ROADS_BASE, CLR_LIGHT_GREY);
    } else if (cls >= RC_ARTERIAL) {
        roads_dashes_v(wx, wy, wy + ROADS_BASE, CLR_YELLOW);
        if (cls == RC_HIGHWAY) {
            line(wx - hw + 3, wy, wx - hw + 3, wy + ROADS_BASE, CLR_LIGHT_GREY);
            line(wx + hw - 3, wy, wx + hw - 3, wy + ROADS_BASE, CLR_LIGHT_GREY);
        }
    }
}
static void roads_road_h(int wx, int wy, int ky, const Zone *z) {
    int hw; bool dirt;
    int cls = roads_class_of(ky);
    if (!roads_line_render(cls, z, &hw, &dirt)) return;
    int col = dirt ? CLR_DARK_BROWN : (cls == RC_HIGHWAY ? CLR_BROWNISH_BLACK : CLR_DARK_GREY);
    rectfill(wx, wy - hw, ROADS_BASE, hw * 2, col);
    if (z->lu != LU_GREEN && z->level >= LV_MED && cls <= RC_ARTERIAL && !dirt) {   // pavement
        rectfill(wx, wy - hw - 2, ROADS_BASE, 2, CLR_MEDIUM_GREY);
        rectfill(wx, wy + hw,     ROADS_BASE, 2, CLR_MEDIUM_GREY);
    }
    if (cls == RC_AVENUE && !dirt) {
        line(wx, wy - hw, wx + ROADS_BASE, wy - hw, CLR_LIGHT_GREY);
        line(wx, wy + hw, wx + ROADS_BASE, wy + hw, CLR_LIGHT_GREY);
    } else if (cls >= RC_ARTERIAL) {
        roads_dashes_h(wy, wx, wx + ROADS_BASE, CLR_YELLOW);
        if (cls == RC_HIGHWAY) {
            line(wx, wy - hw + 3, wx + ROADS_BASE, wy - hw + 3, CLR_LIGHT_GREY);
            line(wx, wy + hw - 3, wx + ROADS_BASE, wy + hw - 3, CLR_LIGHT_GREY);
        }
    }
}

// cam/zoom match the shell's camera_ex. Enumerates the visible-world rect (wider
// than the screen when zoomed out) and draws at world coords; camera_ex scales.
static void roads_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int L = (int)(cx - hwv), R = (int)(cx + hwv), T = (int)(cy - hhv), B = (int)(cy + hhv);

    int B0 = ROADS_BASE;
    int kx0 = roads_ifloordiv(L, B0) - 1, kx1 = roads_ifloordiv(R, B0) + 1;
    int ky0 = roads_ifloordiv(T, B0) - 1, ky1 = roads_ifloordiv(B, B0) + 1;

    rectfill(L - B0, T - B0, (R - L) + 2 * B0, (B - T) + 2 * B0, CLR_DARK_GREEN);  // grass backdrop

    // pass A — block interiors
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++) {
            int wx = kx * B0, wy = ky * B0;
            Zone z = roads_zone_at(wx + B0 / 2.0f, wy + B0 / 2.0f);
            roads_fill_block(wx, wy, B0, &z);
        }
    // pass B — roads on top (so they front the buildings; markings read at crossings)
    for (int kx = kx0; kx <= kx1; kx++)
        for (int ky = ky0; ky <= ky1; ky++) {
            int wx = kx * B0, wy = ky * B0;
            Zone z = roads_zone_at(wx + B0 / 2.0f, wy + B0 / 2.0f);
            roads_road_v(wx, wy, kx, &z);
            roads_road_h(wx, wy, ky, &z);
        }
}

// query (this is what a consumer cart — sloop rung 3 — would call once extracted)
static Place road_at(float x, float y) {
    Zone z = roads_zone_at(x, y);
    int kxn = (int)lroundf(x / ROADS_BASE), kyn = (int)lroundf(y / ROADS_BASE);
    bool on = false; int best = RC_LOCAL;
    int hw; bool dirt;
    int cv = roads_class_of(kxn);
    if (roads_line_render(cv, &z, &hw, &dirt) && fabsf(x - kxn * (float)ROADS_BASE) <= hw) { on = true; best = cv; }
    int ch = roads_class_of(kyn);
    if (roads_line_render(ch, &z, &hw, &dirt) && fabsf(y - kyn * (float)ROADS_BASE) <= hw && ch > best) {
        on = true; best = ch;
    } else if (roads_line_render(ch, &z, &hw, &dirt) && fabsf(y - kyn * (float)ROADS_BASE) <= hw) {
        on = true;
    }
    return (Place){ z, on, best };
}
static int roads_field_col(float x, float y) {       // overlay tint: colour by land use
    static const int lc[LU_N] = { CLR_LIME_GREEN, CLR_ORANGE, CLR_BLUE, CLR_MAUVE };
    Zone z = roads_zone_at(x, y);
    if (z.level == LV_WILD) return CLR_DARK_GREEN;
    return lc[z.lu];
}
static const char *roads_probe(float x, float y) {
    static char buf[48];
    Place p = road_at(x, y);
    static const char *RC_NAME[4] = { "local", "avenue", "arterial", "highway" };
    snprintf(buf, sizeof buf, "%s %s %s i%.2f", LU_NAME[p.z.lu], LV_NAME[p.z.level],
             p.on_road ? RC_NAME[p.road_class] : "lot", p.z.intensity);
    return buf;
}

// ════════════════════════════════════════════════════════════════════════════
//  GENERATOR 2 — TERRAIN   (pure fbm elevation field, no structure)
// ════════════════════════════════════════════════════════════════════════════
// Structurally the opposite of roads — proves the plug-in seam. fbm elevation
// thresholds into bands, drawn as filled cells so zooming out shows whole coasts.
#define TERRAIN_CELL 8        // world px per filled cell (smaller = finer coastline)
#define TERRAIN_FREQ 0.0016f  // base elevation frequency (smaller = bigger continents)

enum { TB_DEEP, TB_SEA, TB_BEACH, TB_GRASS, TB_FOREST, TB_ROCK, TB_SNOW, TB_N };
static const float TERRA_MAX[TB_N]  = { 0.34f, 0.42f, 0.46f, 0.58f, 0.72f, 0.86f, 1.01f }; // upper elev of each band
static const int   TERRA_COL[TB_N]  = { CLR_DARK_BLUE, CLR_BLUE, CLR_LIGHT_PEACH,
                                         CLR_LIME_GREEN, CLR_DARK_GREEN, CLR_MEDIUM_GREY, CLR_WHITE };
static const char *TERRA_NAME[TB_N] = { "DEEP SEA", "SEA", "BEACH", "GRASS", "FOREST", "ROCK", "SNOW" };

static int terrain_seed = 0;
static void terrain_reseed(int seed) { terrain_seed = seed; }

typedef struct { float elev; int band; } Terrain;

static float terrain_elev(float x, float y) {
    float z = (float)terrain_seed;
    float f = TERRAIN_FREQ, amp = 1.0f, sum = 0.0f, norm = 0.0f;
    for (int o = 0; o < 4; o++) {                       // fbm: 4 octaves
        sum  += amp * noise3(x * f, y * f, z + (float)o * 5.0f);
        norm += amp;
        f    *= 2.0f;
        amp  *= 0.5f;
    }
    return clamp(sum / norm, 0.0f, 1.0f);
}
static int terrain_band_of(float e) {
    for (int b = 0; b < TB_N; b++) if (e <= TERRA_MAX[b]) return b;
    return TB_SNOW;
}
static Terrain terrain_at(float x, float y) {
    float e = terrain_elev(x, y);
    return (Terrain){ e, terrain_band_of(e) };
}
static void terrain_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int C = TERRAIN_CELL;
    int L = ((int)(cx - hwv) / C - 1) * C, R = (int)(cx + hwv) + C;
    int T = ((int)(cy - hhv) / C - 1) * C, B = (int)(cy + hhv) + C;
    for (int wy = T; wy <= B; wy += C)
        for (int wx = L; wx <= R; wx += C)
            rectfill(wx, wy, C, C, TERRA_COL[terrain_band_of(terrain_elev(wx + C / 2.0f, wy + C / 2.0f))]);
}
static int terrain_field_col(float x, float y) {      // overlay tint: the band IS the field
    return TERRA_COL[terrain_band_of(terrain_elev(x, y))];
}
static const char *terrain_probe(float x, float y) {
    static char buf[32];
    Terrain t = terrain_at(x, y);
    snprintf(buf, sizeof buf, "%s e%.2f", TERRA_NAME[t.band], t.elev);
    return buf;
}

// ════════════════════════════════════════════════════════════════════════════
//  THE SHELL — free-fly explorer + generator registry
// ════════════════════════════════════════════════════════════════════════════
typedef struct {
    const char *name;
    void  (*draw)(float cam_x, float cam_y, float zoom);
    void  (*reseed)(int seed);
    const char *(*probe)(float wx, float wy);   // 1-line "what's under here"
    int   (*field_col)(float wx, float wy);     // palette colour of the field (overlay)
} Generator;

static Generator gens[] = {
    { "ROADS & CITIES", roads_draw,   roads_reseed,   roads_probe,   roads_field_col   },
    { "TERRAIN",        terrain_draw, terrain_reseed, terrain_probe, terrain_field_col },
};
#define N_GENS ((int)(sizeof gens / sizeof gens[0]))

static float cam_x = 3887, cam_y = -100;   // world coord of screen top-left at zoom 1
static float zoom  = 0.55f;                // open zoomed-out over a varied district (seed 1)
static int   seed  = 1;
static int   cur   = 0;
static bool  overlay = false;

static void reseed_all(int s) {
    for (int i = 0; i < N_GENS; i++) gens[i].reseed(s);
}

void init(void) {
    reseed_all(seed);
}

void update(void) {
    float pan = 5.0f / zoom;                       // constant on-screen pan speed
    if (key(KEY_RIGHT) || key('D')) cam_x += pan;
    if (key(KEY_LEFT)  || key('A')) cam_x -= pan;
    if (key(KEY_DOWN)  || key('S')) cam_y += pan;
    if (key(KEY_UP)    || key('W')) cam_y -= pan;

    if (key('Z')) zoom *= 1.04f;
    if (key('X')) zoom /= 1.04f;
    zoom += mouse_wheel() * 0.08f * zoom;
    zoom = clamp(zoom, 0.18f, 6.0f);

    if (keyp('R')) { seed++; reseed_all(seed); }
    if (keyp(KEY_TAB)) overlay = !overlay;
    for (int i = 0; i < N_GENS; i++) if (keyp('1' + i)) cur = i;
}

// fill the visible-world rect with the active field's colours (coarse, dithered)
static void draw_overlay(void) {
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int OV = 16;
    int L = ((int)(cx - hwv) / OV - 1) * OV, R = (int)(cx + hwv) + OV;
    int T = ((int)(cy - hhv) / OV - 1) * OV, B = (int)(cy + hhv) + OV;
    fillp(0xA5A5, -1);                       // checker → semi-transparent tint
    for (int wy = T; wy <= B; wy += OV)
        for (int wx = L; wx <= R; wx += OV)
            rectfill(wx, wy, OV, OV, gens[cur].field_col(wx + OV / 2.0f, wy + OV / 2.0f));
    fillp_reset();
}

void draw(void) {
    cls(CLR_BLACK);

    camera_ex((int)cam_x, (int)cam_y, zoom, 0);
    gens[cur].draw(cam_x, cam_y, zoom);
    if (overlay) draw_overlay();

    // HUD in screen space
    camera(0, 0);
    int mx = SCREEN_W / 2, my = SCREEN_H / 2;
    line(mx - 5, my, mx + 5, my, CLR_WHITE);          // centre crosshair = probe point
    line(mx, my - 5, mx, my + 5, CLR_WHITE);

    rectfill(0, 0, SCREEN_W, 10, CLR_BLACK);
    char buf[64];
    int x = print(gens[cur].name, 3, 2, CLR_YELLOW);
    snprintf(buf, sizeof buf, "  seed %d  zoom %.2f", seed, (double)zoom);
    print(buf, x, 2, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H - 18, SCREEN_W, 18, CLR_BLACK);
    print(gens[cur].probe(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f),
          3, SCREEN_H - 16, CLR_WHITE);
    print("WASD pan  Z/X zoom  R seed  1/2 gen  TAB overlay",
          3, SCREEN_H - 8, CLR_MEDIUM_GREY);
}
