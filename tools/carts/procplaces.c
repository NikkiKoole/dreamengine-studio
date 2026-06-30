/* de:meta
{
  "title": "Procedural Places",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "tech-demo",
    "generative"
  ],
  "teaches": [
    "noise-terrain"
  ],
  "lineage": "Original testbed for pluggable world generators; the roads-and-cities generator applies a SimCity-2000-style dual noise field (land-use × intensity) to a nested road lattice, while the terrain generator is a straight fbm elevation showcase.",
  "description": "A testbed for procedural generation: one free-fly explorer that hosts PLUGGABLE generators. Ships with two structurally-opposite ones - ROADS & CITIES (a noise density field zones the world into city/town/rural/highway and a nested road lattice is promoted per zone; cities cluster on the noise peaks, organic not a bullseye) and TERRAIN (fbm elevation as colour bands: sea/beach/grass/forest/rock/snow). Same seed = byte-identical world every run (engine value noise, seeded via noise3's z-slice). Forest/dungeon/cave gens slot in as more headers. Arrows/WASD pan, Z/X (or wheel) zoom, R new seed, 1/2 switch generator, TAB field overlay.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
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
//      and culled by local intensity. RENDERED ON A TILE GRID (tile = sloop's CELL):
//      roads are runs of tiles, so crossings connect by construction, one surface
//      colour means no seam, and everything snaps to the grid (no sub-pixel curbs).
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
//  GENERATOR 1 — ROADS & CITIES   (two orthogonal fields, rendered on a TILE GRID)
// ════════════════════════════════════════════════════════════════════════════
// THE WRONG TURN TO AVOID (docs/design/procgen-places.md): draw() and road_at()
// must read the SAME geometry. They do — both classify a TILE through the exact
// same helpers (roads_zone_at + roads_axis_v/h), so a consumer (sloop's collision)
// can never disagree with what's on screen.
//
// ── the tile scheme — THESE are the knobs ────────────────────────────────────
// Change these and everything below re-derives (road widths, lattice, collision,
// render). Tile == sloop's CELL (7px), so "the car is 3-4 tiles wide" is literally
// true in this grid. A lane fits the widest car with room. Off by one? Edit LANE.
#define TILE     7        // px per tile  (== sloop CELL — a car is ~3-4 tiles wide)
#define LANE     5        // tiles per lane (a 4-tile-wide car fits with room)
#define SIDEWALK 1        // tiles of sidewalk each side (built-up, non-highway)
#define BLOCK_T  24       // finest lattice pitch, in tiles
#define BLOCK_PX (BLOCK_T * TILE)     // 168 px — finest block (local-street spacing)
#define LOT_T    4        // building-lot size, in tiles (the "footprint" grid)

#define ROADS_IFREQ 0.00030f  // intensity-field frequency (smaller = bigger cities)
#define ROADS_LFREQ 0.00052f  // land-use-field frequency (smaller = bigger districts)
#define ROADS_WFREQ 0.00040f  // domain-warp frequency
#define ROADS_WAMP  320.0f    // domain-warp amplitude (px) — how organic the blobs

enum { LU_GREEN, LU_RES, LU_COM, LU_IND, LU_N };          // land use
enum { LV_WILD, LV_LIGHT, LV_MED, LV_HEAVY };             // intensity level
enum { RC_LOCAL, RC_AVENUE, RC_ARTERIAL, RC_HIGHWAY };    // road class (by lattice level)

static const char *LU_NAME[LU_N] = { "GREEN", "RES", "COM", "IND" };
static const char *LV_NAME[4]    = { "wild", "light", "med", "heavy" };
static const int   ROAD_LANES[4] = { 1, 2, 4, 6 };    // lanes per class (RC_LOCAL..RC_HIGHWAY)
static const int   ROAD_STEP[4]  = { 1, 3, 9, 27 };   // pitch multiple per class (nested lattice)

static int roads_seed = 0;
static void roads_reseed(int seed) { roads_seed = seed; }

typedef struct { int lu; int level; float intensity; } Zone;   // "what kind of place"
typedef struct { Zone z; bool on_road; int road_class; } Place; // + "is there a road here"

static int roads_ifloordiv(int a, int b) {        // floor division (handles negatives)
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}
static unsigned roads_hash2(int a, int b) {       // per-cell pseudo-random (lots, trees)
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

// A line's CLASS is a pure function of its index — nested pitch 1/3/9/27. So a
// highway is a highway wherever it runs, independent of the zone it crosses.
static int roads_class_of(int k) {
    if (k % 27 == 0) return RC_HIGHWAY;
    if (k % 9  == 0) return RC_ARTERIAL;
    if (k % 3  == 0) return RC_AVENUE;
    return RC_LOCAL;
}

// ── road band: visible? how many tiles of surface + sidewalk, and dirt? ───────
// Sampled once per line at a pitch-bucket (so a line's width/visibility is CONSTANT
// over its whole run — roads can't chunk or jog). Shared by draw + road_at.
static bool roads_band(int cls, const Zone *z, int *surf_t, int *side_t, bool *dirt) {
    *dirt = false; *side_t = 0; *surf_t = 0;
    bool vis;
    switch (cls) {
        case RC_HIGHWAY:  vis = true;                 break;   // skeleton: everywhere
        case RC_ARTERIAL: vis = z->level >= LV_LIGHT; break;
        case RC_AVENUE:
            if (z->level >= LV_MED) vis = true;
            else if (z->level >= LV_LIGHT && z->lu == LU_GREEN) { vis = true; *dirt = true; }
            else vis = false;                                  // farm dirt lane
            break;
        default:          vis = z->level >= LV_HEAVY; break;   // LOCAL streets
    }
    if (!vis) return false;
    if (*dirt) { *surf_t = LANE; return true; }                // dirt = 1 lane, no sidewalk
    *surf_t = ROAD_LANES[cls] * LANE;
    if (cls != RC_HIGHWAY && z->lu != LU_GREEN && z->level >= LV_MED) *side_t = SIDEWALK;
    return true;
}

// What a tile is, with respect to ONE axis's nearest lattice line. `vis`+`surf`+`off`
// are set whenever a road exists nearby (even for kind-0 tiles just off the band), so
// callers can measure distance to a crossing for zebras / lights.
typedef struct { int kind; int cls; int off; int surf; int line; bool vis; bool dirt; } Band;
static Band roads_axis(int along, int across, bool vertical) {
    // `along`  = tile coord parallel to the line  (varies the bucket)
    // `across` = tile coord perpendicular         (decides surface/sidewalk/none)
    Band b = { 0, 0, 0, 0, 0, false, false };
    int k = (int)lroundf(across / (float)BLOCK_T);
    int cls = roads_class_of(k);
    int pitch = BLOCK_T * ROAD_STEP[cls];                      // bucket cadence (tiles)
    int bz = roads_ifloordiv(along, pitch) * pitch + pitch / 2;
    int line_px = k * BLOCK_PX, along_px = bz * TILE + TILE / 2;   // line coord, bucket centre (px)
    Zone z = roads_zone_at(vertical ? (float)line_px : (float)along_px,
                           vertical ? (float)along_px : (float)line_px);
    int surf, side; bool dirt;
    if (!roads_band(cls, &z, &surf, &side, &dirt)) return b;
    int half = surf / 2;
    int off = across - k * BLOCK_T;                            // tile offset from line column
    b.cls = cls; b.dirt = dirt; b.off = off; b.surf = surf; b.line = k; b.vis = true;
    if (off >= -half && off <= surf - 1 - half) { b.kind = 1; return b; }
    if (side > 0 && ((off >= -half - side && off < -half) ||
                     (off > surf - 1 - half && off <= surf - 1 - half + side)))
        b.kind = 2;
    return b;
}
static Band roads_axis_v(int tx, int ty) { return roads_axis(ty, tx, true);  }  // vertical line
static Band roads_axis_h(int tx, int ty) { return roads_axis(tx, ty, false); }  // horizontal line

// building / ground colour for a non-road tile (lots grid the footprints)
static int roads_lot_color(int tx, int ty, const Zone *z) {
    int lotx = roads_ifloordiv(tx, LOT_T), loty = roads_ifloordiv(ty, LOT_T);
    unsigned h = roads_hash2(lotx + roads_seed * 911, loty);
    switch (z->lu) {
        case LU_GREEN:
            if (z->level == LV_WILD)
                return (roads_hash2(tx * 7, ty * 13 + roads_seed) % 19 == 0) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN;
            return (h & 1) ? CLR_MEDIUM_GREEN : CLR_LIME_GREEN;        // park / farm
        case LU_RES: {
            static const int roof[5] = { CLR_BROWN, CLR_RED, CLR_DARK_RED, CLR_DARK_PURPLE, CLR_PEACH };
            if ((h & 7) == 0) return CLR_DARK_GREEN;                  // yard / empty lot
            return roof[h % 5];
        }
        case LU_COM: {
            static const int roofL[4] = { CLR_DARK_GREY, CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_INDIGO };
            static const int roofH[3] = { CLR_DARKER_BLUE, CLR_DARKER_GREY, CLR_INDIGO };
            return (z->level >= LV_HEAVY) ? roofH[h % 3] : roofL[h % 4];
        }
        default: {                                                    // LU_IND
            static const int body[3] = { CLR_DARKER_GREY, CLR_DARK_BROWN, CLR_DARKER_PURPLE };
            return body[h % 3];
        }
    }
}

// ── v1.5 tile polish: zebra crossings, traffic lights, parking ───────────────
// zebra "rungs" across a road, on the approach to a bigger crossing
static void roads_zebra_tile(int px, int py, bool stripes_vertical) {
    for (int i = 1; i < TILE; i += 3)
        if (stripes_vertical) line(px + i, py, px + i, py + TILE - 1, CLR_WHITE);
        else                  line(px, py + i, px + TILE - 1, py + i, CLR_WHITE);
}
// a signal head at a major-crossing corner; the lit lamp cycles red→amber→green on
// now() (per-crossing phase so the grid isn't synchronised)
static void roads_light_tile(int px, int py, unsigned phase) {
    int lamp[3] = { CLR_RED, CLR_YELLOW, CLR_GREEN };
    int s = ((int)(now() * 0.8f) + (int)phase) % 3;
    rectfill(px + 1, py + 1, 4, 4, CLR_BLACK);              // housing for contrast
    rectfill(px + 2, py + 2, 2, 2, lamp[s]);               // the lit lamp
}
// some built-up commercial / industrial lots are parking instead of a building
static bool roads_is_parking(int tx, int ty, const Zone *z) {
    if ((z->lu != LU_COM && z->lu != LU_IND) || z->level < LV_MED) return false;
    int lotx = roads_ifloordiv(tx, LOT_T), loty = roads_ifloordiv(ty, LOT_T);
    return (roads_hash2(lotx * 3 + roads_seed * 17, loty * 3) % 5) == 0;
}

// cam/zoom match the shell's camera_ex. Enumerates visible TILES and draws each as
// one filled cell at tile-snapped world coords; camera_ex scales. LOD: when zoomed
// out, draw bigger meta-cells (keeps the frame cheap AND stops thin features
// shimmering — only fine markings/edges draw when truly close).
static void roads_draw(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv), Rpx = (int)(cx + hwv);
    int Tpx = (int)(cy - hhv), Bpx = (int)(cy + hhv);

    int step = 1;
    while ((float)TILE * step * zoom < 3.0f && step < 8) step++;   // cells >= ~3 screen px
    bool detail = (step == 1) && (zoom >= 0.9f);                    // dashes/edges only when close
    int sz = TILE * step, c = step / 2;

    int t0x = roads_ifloordiv(Lpx, TILE) - 1, t1x = roads_ifloordiv(Rpx, TILE) + 1;
    int t0y = roads_ifloordiv(Tpx, TILE) - 1, t1y = roads_ifloordiv(Bpx, TILE) + 1;
    t0x -= ((t0x % step) + step) % step;                           // align to the step grid
    t0y -= ((t0y % step) + step) % step;

    for (int ty = t0y; ty <= t1y; ty += step)
        for (int tx = t0x; tx <= t1x; tx += step) {
            int px = tx * TILE, py = ty * TILE;
            Band v = roads_axis_v(tx + c, ty + c), h = roads_axis_h(tx + c, ty + c);
            if (v.kind == 1 || h.kind == 1) {                       // road surface
                bool dirt = (v.kind == 1 && v.dirt) || (h.kind == 1 && h.dirt);
                rectfill(px, py, sz, sz, dirt ? CLR_DARK_BROWN : CLR_DARK_GREY);
                if (detail && !dirt) {
                    bool cross = (v.kind == 1 && h.kind == 1);
                    bool zebra = false;
                    // zebra on the approach tile just outside a crossing (avenue+)
                    if (v.kind == 1 && !cross && h.vis && h.cls >= RC_AVENUE) {
                        int lo = -h.surf / 2, hi = h.surf - 1 - h.surf / 2;
                        if (h.off == lo - 1 || h.off == hi + 1) { roads_zebra_tile(px, py, true); zebra = true; }
                    }
                    if (h.kind == 1 && !cross && v.vis && v.cls >= RC_AVENUE) {
                        int lo = -v.surf / 2, hi = v.surf - 1 - v.surf / 2;
                        if (v.off == lo - 1 || v.off == hi + 1) { roads_zebra_tile(px, py, false); zebra = true; }
                    }
                    // centre dashes (skip through crossings and zebra tiles)
                    if (!cross && !zebra) {
                        if (v.kind == 1 && v.cls >= RC_AVENUE && v.off == 0 && (ty & 1))
                            line(px + TILE / 2, py, px + TILE / 2, py + TILE - 1, CLR_YELLOW);
                        if (h.kind == 1 && h.cls >= RC_AVENUE && h.off == 0 && (tx & 1))
                            line(px, py + TILE / 2, px + TILE - 1, py + TILE / 2, CLR_YELLOW);
                    }
                    // signal heads at all four corners of a major (avenue+) crossing
                    if (cross && v.cls >= RC_AVENUE && h.cls >= RC_AVENUE) {
                        bool vc = (v.off == -v.surf / 2) || (v.off == v.surf - 1 - v.surf / 2);
                        bool hc = (h.off == -h.surf / 2) || (h.off == h.surf - 1 - h.surf / 2);
                        if (vc && hc) roads_light_tile(px, py, roads_hash2(v.line, h.line));
                    }
                }
            } else if (v.kind == 2 || h.kind == 2) {                // sidewalk
                rectfill(px, py, sz, sz, CLR_MEDIUM_GREY);
            } else {                                                // building / ground lot
                Zone z = roads_zone_at(px + sz / 2.0f, py + sz / 2.0f);
                if (roads_is_parking(tx + c, ty + c, &z)) {                  // parking lot
                    rectfill(px, py, sz, sz, CLR_DARKER_GREY);
                    if (detail) {
                        int ex = ((tx % LOT_T) + LOT_T) % LOT_T;
                        if (ex == 0)          line(px, py, px, py + TILE - 1, CLR_BROWNISH_BLACK);
                        else if (ex % 2 == 1) line(px, py + 1, px, py + TILE - 2, CLR_LIGHT_GREY); // bay lines
                    }
                } else {
                    int col = roads_lot_color(tx + c, ty + c, &z);
                    rectfill(px, py, sz, sz, col);
                    if (detail && z.lu != LU_GREEN && col != CLR_DARK_GREEN) {   // building footprints
                        int ex = ((tx % LOT_T) + LOT_T) % LOT_T, ey = ((ty % LOT_T) + LOT_T) % LOT_T;
                        if (ex == 0) line(px, py, px, py + TILE - 1, CLR_BROWNISH_BLACK);
                        if (ey == 0) line(px, py, px + TILE - 1, py, CLR_BROWNISH_BLACK);
                        if (z.lu == LU_COM && ex == LOT_T / 2 && ey == LOT_T / 2)
                            pset(px + TILE / 2, py + TILE / 2, CLR_LIGHT_YELLOW);
                    }
                }
            }
        }
}

// query (this is what a consumer cart — sloop rung 3 — would call once extracted).
// Same classifier the renderer uses, so collision == what's drawn, always.
static Place road_at(float x, float y) {
    Zone z = roads_zone_at(x, y);                          // the LOT's zone (lu/level report)
    int tx = roads_ifloordiv((int)x, TILE), ty = roads_ifloordiv((int)y, TILE);
    Band v = roads_axis_v(tx, ty), h = roads_axis_h(tx, ty);
    bool on = (v.kind == 1) || (h.kind == 1);             // drivable surface (sidewalk doesn't count)
    int cls = -1;
    if (v.kind == 1) cls = v.cls;
    if (h.kind == 1 && h.cls > cls) cls = h.cls;
    if (cls < 0) cls = RC_LOCAL;
    return (Place){ z, on, cls };
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

static float cam_x = 2360, cam_y = 404;    // world coord of screen top-left at zoom 1
static float zoom  = 1.20f;                // open on a signalised avenue crossing (seed 1)
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
