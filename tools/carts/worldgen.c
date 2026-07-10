/* de:meta
{
  "slug": "worldgen",
  "title": "worldgen",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "noise-terrain",
    "camera-follow"
  ],
  "lineage": "Direct C port of gtascii (github.com/NikkiKoole/gtascii), a Lua/LOVE2D ASCII GTA world; novel in converting the fixed-grid scan to a per-cell hash so the same world streams infinitely around the camera.",
  "homage": "gtascii (github.com/NikkiKoole/gtascii)",
  "description": "A faithful C port of the procedural world from my old gtascii project (an ASCII GTA), rebuilt as an endless, deterministic top-down map you pan across. The recipe is gtascii's, line for line: a layered value-noise heightmap (three octaves summed 0.6 / 0.3 / 0.1) thresholded into a terrain colour ladder — deep water, shallows, beach, dunes, scrub, grass, forest, hills, rock, peaks; meandering RIVERS carved by a second squared-noise field; CITIES dropped on habitable lowland (black footprints with a street grid), and ROADS that connect nearby cities only where the land between them is passable. Where gtascii scanned one fixed grid, this hashes each map cell so the same world streams around the camera with no global pass — effectively infinite. A live legend shows which biome the centre sits on. This is the world sloop's build-a-vehicle rig will later drive across (docs/design/sloop.md → rung 3, biomes). Controls: arrows / WASD pan, Z / X zoom the terrain in / out about the centre (gtascii's scalein 1/16/128), SPACE jumps to a fresh region, R recentres."
}
de:meta */
#include "studio.h"
#include <stdio.h>

// ============================================================================
// WORLDGEN  —  a faithful C port of gtascii's procedural world.
//              (github.com/NikkiKoole/gtascii → worldgen.lua, Lua/LÖVE)
//
//   ◄▲▼►  /  WASD   pan across the (infinite) world
//   Z / X           zoom the terrain detail in / out  (gtascii's `scalein` 1/16/128)
//   SPACE           jump to a fresh region (the world is endless + deterministic)
//   R               recentre at the origin
//
// ── the recipe, ported one-for-one from gtascii ──────────────────────────────
// gtascii built a FIXED grid; sloop's world must STREAM around the camera. The
// only change is that — every formula below is the original, line for line:
//
//   1. HEIGHTMAP — layered value noise, three octaves summed at gtascii's exact
//      weights (createZoomedinWorld):   x = 0.6·N(/100) + 0.3·N(/30) + 0.1·N(/10)
//      then a fine-detail octave folded in when zoomed (scalein 16/128), and the
//      sea level dropped by 0.45.  N() is the engine's noise (love.math.noise's twin).
//   2. TERRAIN BANDS — that single height `x` thresholded into the gtascii colour
//      ladder: deep water → shallow → beach → … → forest → hills → rock → peaks.
//   3. RIVERS — two passes of a SECOND noise field, squared, carved into land
//      wherever it lands in a narrow value band (the meandering-channel trick).
//   4. CITIES — placed on habitable lowland over a coarse cell-grid, jittered.
//      gtascii scanned the whole map; we hash each cell so the SAME cities fall
//      out wherever you are, with no global pass (the streaming-world adaptation).
//   5. ROADS — connect nearby cities with a straight run IF the land between them
//      is passable (gtascii's Bresenham line-of-sight over open terrain).
//
// Nothing here is sloop's drive core — this is the world the rig will later cross
// (docs/design/sloop.md → "rung 3: biomes + traction"). Built standalone first to
// prove the gtascii look, exactly like orbit shipped its integrator before the bin.
// ============================================================================

#define TILE 4                                  // screen px per world tile
#define COLS (SCREEN_W / TILE + 2)              // visible tile columns (+margin)
#define ROWS (SCREEN_H / TILE + 2)              // visible tile rows
#define SPAWN_X 312.0f                           // open on a scenic region (river + cities +
#define SPAWN_Y 8.0f                             // forest), not the open ocean at the origin

// ── camera: top-left of the view, in world-tile coords (float = smooth scroll) ─
static float camX, camY;
static int   scalein = 1;                       // gtascii zoom: 1 overview / 16 / 128 closeup
static float seedZ;                             // noise z-slice (the world's "seed")
static float jumpN;                             // SPACE counter → deterministic far jumps

// ── deterministic per-tile hash (sloop's, for jitter + city cells) ────────────
static unsigned hash2(int a, int b) {
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static int ifloor(float v) { int i = (int)v; return (v < 0 && (float)i != v) ? i - 1 : i; }

// ── 1+3. HEIGHTMAP — gtascii createZoomedinWorld(), verbatim ──────────────────
// Returns the terrain height `x` at world-tile (i,j). The river carve sets it to
// the sentinel -0.5 (deep water) exactly as gtascii does. noise3's z carries the seed.
static float height_at(float i, float j) {
    float scale = 1.0f / scalein;
    float x1 = noise3(i / 100.0f * scale, j / 100.0f * scale, seedZ);   // big continents
    float x2 = noise3(i / 10.0f  * scale, j / 10.0f  * scale, seedZ);   // fine roughness
    float x3 = noise3(i / 30.0f  * scale, j / 30.0f  * scale, seedZ);   // medium hills
    float x4 = noise3(i / 0.3f   * scale, j / 0.3f   * scale, seedZ);   // detail (zoomed only)
    float x5 = noise3(i / 0.2f   * scale, j / 0.2f   * scale, seedZ);

    float x = 0.6f * x1 + 0.3f * x3 + 0.1f * x2;
    if (scalein == 16 || scalein == 128) x = x + x4 / 500.0f;           // "nice!!"
    if (scalein == 128)                  x = x + x5 / 2000.0f;
    x = x - 0.45f;                                                      // drop sea level

    // rivers — pass 1 (wide): a squared noise field, carved where it falls in a band
    float xr = noise2(i / 40.0f * scale, j / 40.0f * scale);
    xr = xr * xr;
    if (xr > 0.3f && xr < 0.3f + (0.5f * 0.3f) * 0.5f && x > 0.0f) x = -0.5f;
    // rivers — pass 2 (narrow tributaries)
    xr = noise2(i / 20.0f * scale, j / 20.0f * scale);
    if (xr > 0.3f && xr < 0.3f + (0.05f * 0.3f) * 0.5f && x > 0.0f) x = -0.5f;

    return x;
}

// ── 2. TERRAIN BANDS — gtascii's colour ladder (PICO palette equivalents) ─────
// The two blues are the only judgement calls: gtascii's `blue` (deep/river) → the
// dark navy, `new_blue` (shallow) → the bright cyan-blue. The rest map one-to-one.
static int biome_col(float x) {
    if (x == -0.5f)     return CLR_DARK_BLUE;     // deep water / river channel
    if (x <  0.00f)     return CLR_BLUE;          // shallow water  (gtascii new_blue)
    if (x <  0.01f)     return CLR_YELLOW;        // beach / sand
    if (x <  0.05f)     return CLR_ORANGE;        // dunes
    if (x <  0.10f)     return CLR_PINK;          // scrub
    if (x <  0.20f)     return CLR_GREEN;         // grass / lowland
    if (x <  0.30f)     return CLR_DARK_GREEN;    // forest
    if (x <  0.40f)     return CLR_BROWN;         // hills
    if (x <  0.45f)     return CLR_DARK_GREY;     // rock
    return CLR_LIGHT_GREY;                        // peaks (gtascii x<0.8)
}
static int passable(float x) { return x >= 0.0f && x < 0.4f; }   // gtascii road LOS test

// ── cached colour grid: only regenerated when the base tile / zoom / seed moves ─
static int colgrid[ROWS][COLS];
static int cacheX = 0x7fffffff, cacheY, cacheScale; static float cacheSeed;
static void regen_if_needed(void) {
    int bx = ifloor(camX), by = ifloor(camY);
    if (bx == cacheX && by == cacheY && scalein == cacheScale && seedZ == cacheSeed) return;
    cacheX = bx; cacheY = by; cacheScale = scalein; cacheSeed = seedZ;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            colgrid[r][c] = biome_col(height_at((float)(bx + c), (float)(by + r)));
}

// ── 4. CITIES — deterministic per coarse cell (the streaming-world adaptation) ─
// gtascii placed a city where a 16×16 block averaged out as habitable lowland and a
// dice roll passed. We do the same per cell, but hash the CELL so the result is the
// same no matter when we visit it — no global scan. Returns 1 + the (jittered) world
// tile position if cell (cx,cy) holds a city on land that isn't water or mountain.
#define CITY_CS  16                  // city cell size (world tiles) — gtascii's cs
#define CITY_R    3                  // drawn city footprint radius (world tiles)
static int city_at(int cx, int cy, float *wx, float *wy) {
    unsigned h = hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    if (h % 100u >= 40u) return 0;                       // ~40% of cells (gtascii <0.4)
    float jx = (float)((int)((h >> 7) % 17u) - 8);       // jitter ±8 (gtascii rndOffset(8))
    float jy = (float)((int)((h >> 15) % 17u) - 8);
    float x = cx * CITY_CS + CITY_CS * 0.5f + jx;
    float y = cy * CITY_CS + CITY_CS * 0.5f + jy;
    float ht = height_at(x, y);
    if (ht <= 0.0f || ht >= 0.4f) return 0;              // habitable land only
    *wx = x; *wy = y;
    return 1;
}

// world tile → screen px (top-left of the tile)
static int sxp(float wx) { return (int)((wx - camX) * TILE); }
static int syp(float wy) { return (int)((wy - camY) * TILE); }

// ── 5. ROADS — straight run between two cities if the land between is passable ──
#define ROAD_MAX 30.0f               // gtascii: connect cities closer than 30 tiles
static int road_clear(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    float len = fsqrt(dx * dx + dy * dy);
    if (len < 0.5f) return 0;
    int steps = (int)len;                                // sample ~every world tile
    for (int s = 1; s < steps; s++) {
        float t = (float)s / steps;
        if (!passable(height_at(ax + dx * t, ay + dy * t))) return 0;
    }
    return 1;
}
static void thick_line(int x0, int y0, int x1, int y1, int col, int half) {
    for (int o = -half; o <= half; o++) {
        line(x0 + o, y0, x1 + o, y1, col);               // a fat-ish band (good enough at this zoom)
        line(x0, y0 + o, x1, y1 + o, col);
    }
}
static void draw_roads_and_cities(void) {
    int c0 = ifloor(camX / CITY_CS) - 2, c1 = ifloor((camX + COLS) / CITY_CS) + 2;
    int r0 = ifloor(camY / CITY_CS) - 2, r1 = ifloor((camY + ROWS) / CITY_CS) + 2;
    // roads first (under the city blocks) — each city links forward to avoid drawing twice
    for (int cx = c0; cx <= c1; cx++)
        for (int cy = r0; cy <= r1; cy++) {
            float ax, ay; if (!city_at(cx, cy, &ax, &ay)) continue;
            for (int nx = cx - 2; nx <= cx + 2; nx++)
                for (int ny = cy - 2; ny <= cy + 2; ny++) {
                    if (ny < cy || (ny == cy && nx <= cx)) continue;   // forward half only
                    float bx, by; if (!city_at(nx, ny, &bx, &by)) continue;
                    float dx = bx - ax, dy = by - ay;
                    if (dx * dx + dy * dy > ROAD_MAX * ROAD_MAX) continue;
                    if (road_clear(ax, ay, bx, by))
                        thick_line(sxp(ax) + TILE / 2, syp(ay) + TILE / 2,
                                   sxp(bx) + TILE / 2, syp(by) + TILE / 2, CLR_DARK_GREY, 1);
                }
        }
    // city blocks: a black footprint with a street grid + a coloured ring (gtascii's
    // intersection-score reds/purples) so a city reads as a city from the overview.
    for (int cx = c0; cx <= c1; cx++)
        for (int cy = r0; cy <= r1; cy++) {
            float ax, ay; if (!city_at(cx, cy, &ax, &ay)) continue;
            int px = sxp(ax - CITY_R), py = syp(ay - CITY_R), s = CITY_R * 2 * TILE;
            rectfill(px, py, s, s, CLR_BLACK);
            for (int g = TILE; g < s; g += TILE) {       // faint street grid inside
                line(px + g, py, px + g, py + s, CLR_DARKER_GREY);
                line(px, py + g, px + s, py + g, CLR_DARKER_GREY);
            }
            rect(px, py, s, s, (hash2(cx, cy) & 1) ? CLR_DARK_PURPLE : CLR_DARK_RED);
        }
}

// ── HUD: a biome legend + the live readout (which band the camera centre sits on) ─
static const char *BAND_NAME[] = {
    "deep", "water", "beach", "dunes", "scrub", "grass", "forest", "hills", "rock", "peaks"
};
static int band_idx(float x) {
    if (x == -0.5f) return 0; if (x < 0.0f) return 1; if (x < 0.01f) return 2;
    if (x < 0.05f) return 3;  if (x < 0.1f) return 4; if (x < 0.2f) return 5;
    if (x < 0.3f) return 6;   if (x < 0.4f) return 7; if (x < 0.45f) return 8; return 9;
}
static void hud(void) {
    char buf[64];
    float cx = camX + COLS / 2.0f, cy = camY + ROWS / 2.0f;
    float ch = height_at(cx, cy);
    // a left legend swatch column (over a dim panel so it reads on busy terrain)
    font(FONT_SMALL);
    fillp(FILL_CHECKER, -1); rectfill(2, 14, 44, 82, CLR_BLACK); fillp_reset();
    int bands[] = { CLR_DARK_BLUE, CLR_BLUE, CLR_YELLOW, CLR_ORANGE, CLR_PINK,
                    CLR_GREEN, CLR_DARK_GREEN, CLR_BROWN, CLR_DARK_GREY, CLR_LIGHT_GREY };
    for (int i = 0; i < 10; i++) {
        rectfill(4, 16 + i * 8, 6, 6, bands[i]);
        print(BAND_NAME[i], 13, 17 + i * 8, band_idx(ch) == i ? CLR_WHITE : CLR_MEDIUM_GREY);
    }
    font(FONT_NORMAL);
    // top bar
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("WORLDGEN", 4, 2, CLR_LIGHT_GREY);
    snprintf(buf, sizeof buf, "zoom %d", scalein);
    print(buf, 90, 2, CLR_YELLOW);
    snprintf(buf, sizeof buf, "x %d  y %d", (int)cx, (int)cy);
    print(buf, 150, 2, CLR_MEDIUM_GREY);
    print_centered("\x18\x19\x1a\x1b pan   Z/X zoom   SPACE jump   R origin",
                   SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
}

void init(void) {
    camX = SPAWN_X - COLS / 2.0f; camY = SPAWN_Y - ROWS / 2.0f;
}

void update(void) {
    float pan = 1.3f;                                // ~1.3 world-tiles/frame
    if (btn(0, BTN_LEFT)  || key('A')) camX -= pan;
    if (btn(0, BTN_RIGHT) || key('D')) camX += pan;
    if (btn(0, BTN_UP)    || key('W')) camY -= pan;
    if (btn(0, BTN_DOWN)  || key('S')) camY += pan;

    // zoom the terrain detail. Z=in, X=out (the gtascii scalein 1→16→128 chain). Note
    // BTN_A/BTN_B alias the Z/X keys, so we bind ONLY the key — no btn() double-fire. We
    // zoom ABOUT THE SCREEN CENTRE: noise samples at tile/scalein, so scaling the camera
    // by the ratio keeps the SAME terrain under the centre, just magnified (a true zoom,
    // not a teleport into a different noise locale).
    if (keyp('X') || keyp('Z')) {
        int old = scalein;
        if (keyp('X')) scalein = scalein == 1 ? 16 : scalein == 16 ? 128 : 1;
        else           scalein = scalein == 128 ? 16 : scalein == 16 ? 1 : 128;
        float ratio = (float)scalein / old;
        float ccx = (camX + COLS / 2.0f) * ratio, ccy = (camY + ROWS / 2.0f) * ratio;
        camX = ccx - COLS / 2.0f; camY = ccy - ROWS / 2.0f;
    }

    if (keyp(KEY_SPACE)) {                           // deterministic jump to fresh scenery
        jumpN += 1.0f;
        unsigned h = hash2((int)(jumpN * 911), 7);
        camX += (float)((int)(h % 4000u) - 2000) + 800.0f;
        camY += (float)((int)((h >> 11) % 4000u) - 2000) + 800.0f;
        seedZ += 0.37f;                              // also nudge the noise slice
    }
    if (keyp('R')) { camX = SPAWN_X - COLS / 2.0f; camY = SPAWN_Y - ROWS / 2.0f; seedZ = 0; jumpN = 0; scalein = 1; }
}

void draw(void) {
    regen_if_needed();
    int fracx = (int)((camX - ifloor(camX)) * TILE);
    int fracy = (int)((camY - ifloor(camY)) * TILE);
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            rectfill(c * TILE - fracx, r * TILE - fracy, TILE, TILE, colgrid[r][c]);
    draw_roads_and_cities();
    hud();
}
