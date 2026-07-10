/* de:meta
{
  "slug": "flyover",
  "title": "flyover",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "noise-terrain",
    "raycasting",
    "parallax",
    "software-rasterizer"
  ],
  "lineage": "Comanche voxel-space engine (y-buffer ray-march) fused with a Mode-7-derived billboard projector and a flat-shaded polygon aircraft; novel in landing, takeoff, and crash on an 8192×8192 named-town continent.",
  "description": "A third-person flight sim over a procedurally generated earth — three engine tricks fused into one sky. The ground is REAL 3-D voxel-space terrain: a noise heightmap ray-marched per column, front-to-back, with a y-buffer and hillshading, so you fly over, around and into actual hills, ridges and snow-capped peaks (sea, beach, grass, forest, rock, snow). Forests and whole cities of windowed buildings are billboards that stand on the terrain, projected far-to-near; clouds are billboards lifted into the sky. Your low-poly plane is a flat-shaded solid-3D model pinned to the foreground — it banks when you turn, pitches when you climb, and casts a dithered shadow on the ground. The world is a vast 8192×8192 continent with ~90 named towns dotting the coast — each a gradient from tall downtown towers out through low suburban houses with gardens to scattered rural farmsteads (barns, farmhouses, silos) and patchwork fields stamped onto the grassland. A scrolling, player-centred minimap shows the land around you, nearby town names, and your heading (props are spatial-gridded so only the ones near you are ever drawn). You can land, take off, and crash: set down in an open field at low speed, taxi, and lift off again — or fly into a tower, a peak or the sea and go up in a fireball (press Z to restart). Sandbox controls: Left/Right turn (the plane banks into it), flight-stick pitch (UP noses down/descends, DOWN climbs — hold UP low and slow to land), Z/X throttle."
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// FLYOVER — a third-person flight sim over a procedurally generated earth.
//
// Three engine idioms fused into one sky:
//   1. VOXEL TERRAIN   — real 3-D ground. A noise heightmap is ray-marched per screen
//      column, front-to-back, drawing vertical strips behind a y-buffer (the Comanche /
//      "voxel space" trick). You fly over, around and into actual hills, ridges and
//      snow-capped peaks — not a flat mode-7 plane. (A ground point at distance z and
//      elevation e lands on row hz + (H-e)*F/z — the mode-7 formula, generalised to e.)
//   2. BILLBOARDS      — forests and whole cities of 3-D-ish buildings scattered across
//      the world, projected with the SAME math and drawn far-to-near (painter's order),
//      so they stand on the terrain and shrink with distance. Clouds are billboards too,
//      just lifted into the sky. (Mountains used to be billboards — now they're terrain.)
//   3. POLYGON PLANE   — a flat-shaded low-poly aircraft (solid3d.c technique:
//      rotate every corner, sort faces, fillp-dither the in-between shades) pinned
//      to the foreground. It banks when you turn and pitches when you climb/dive.
//
// The world is one VAST bounded continent — 8192×8192 flight units. Low-frequency
// noise gives continents, coastlines and mountain ranges; a radial falloff sinks
// the rim into open ocean. ~90 named towns dot the habitable coast. A scrolling,
// player-centred MINIMAP shows the land around you, nearby town names, and your
// heading so you can navigate the distances. Props are bucketed in a spatial grid
// so only the handful near you are ever projected, however big the world gets.
//
// You can also LAND, TAKE OFF, and CRASH. Pitch is flight-stick style (PITCH_INVERT):
// UP noses down/descends, DOWN noses up/climbs. Descend to the deck and hold UP over
// open flat ground at low speed to touch down (approach drag helps bleed speed); taxi
// with L/R, power up with Z, hold DOWN to lift off again. Fly into a tower, a peak,
// water or rough terrain and the plane goes up in a fireball — press Z to restart.
// Collisions reuse the same spatial grid the billboards do, so it stays cheap.
//
// Free-flight sandbox — go find the towns, buzz the rooftops, set her down in a field.
//   Left/Right  bank & turn     UP  dive     DOWN  climb     Z  faster     X  slower
//   (low + hold UP = land · on the ground: Z power up, DOWN take off · crashed: Z restart)

// ── world ────────────────────────────────────────────────────────────────────
#define TEX    1024              // baked colour-grid resolution (the texture)
#define WORLD  8192.0f           // world EXTENT in flight units — 16× the old 512
#define T2W    (TEX / WORLD)     // texture texels per world unit (0.125)
#define HZ     78                // horizon row on screen
#define BS     2                 // ground block size (chunky = faster + retro)
#define F      140.0f            // mode-7 focal length (ground + billboards share it)

static unsigned char world[TEX * TEX];   // baked terrain colour per texel

// sample the baked world at world-space (wx,wy) — off the edge reads as deep ocean
static int sample_world(float wx, float wy) {
    int tx = (int)(wx * T2W), ty = (int)(wy * T2W);
    if (tx < 0 || tx >= TEX || ty < 0 || ty >= TEX) return CLR_DARK_BLUE;
    return world[ty * TEX + tx];
}

// ── elevation: the voxel-terrain renderer extrudes the ground to real height ────
#define PEAK 110.0f                      // height at t==1; ridge crests (t up to 1.4) tower above this
static float hmap[TEX * TEX];            // baked elevation per texel (above sea level)
static float elev_of(float t) {          // terrain value → elevation; sea is flat at 0
    if (t <= 0.46f) return 0.0f;
    float u = (t - 0.46f) / 0.54f;       // 0 at the coast … 1 at t=1 … ~1.7 at a ridge crest
    return powf(u, 0.9f) * PEAK;         // near-linear → strong relief even in the lowlands
}
static float elev_at(float wx, float wy) {
    int tx = (int)(wx * T2W), ty = (int)(wy * T2W);
    if (tx < 0 || tx >= TEX || ty < 0 || ty >= TEX) return 0.0f;   // open sea
    return hmap[ty * TEX + tx];
}

// ── towns (named places, marked on the minimap) ───────────────────────────────
#define MAXCITY 96
static float cityx[MAXCITY], cityy[MAXCITY];
static char  citynamebuf[MAXCITY][16];
static const char *cityname[MAXCITY];
static int ncity = 0;
// names are built from a prefix × suffix grid → 24×15 = 360 distinct town names
static const char *PREFIX[] = {
    "NORTH","SALT","IRON","OAK","DUSK","FAIR","RIVER","GULL","STONE","PINE",
    "RED","HIGH","LAKE","COLD","SUN","WIND","ASH","ELK","BLACK","GOLD",
    "WEST","FROST","BRIAR","HOLLY",
};
static const char *SUFFIX[] = {
    "GATE","REACH","HOLLOW","MOOR","MOUTH","HAVEN","VALE","FORD","DALE","BURG",
    "PORT","CREST","FELL","WICK","BROOK",
};
#define NPREF (int)(sizeof(PREFIX) / sizeof(PREFIX[0]))
#define NSUF  (int)(sizeof(SUFFIX) / sizeof(SUFFIX[0]))

// ── minimap (player-centred, scrolls; sampled live from the world) ─────────────
#define MINI     48              // minimap sample resolution
#define MZOOM    2               // screen pixels per minimap cell → a 96×96 map
#define MAPSPAN  2600.0f         // world units shown across the minimap

// ── props (trees / buildings / mountains / clouds) ─────────────────────────────
#define TREE     0
#define BUILDING 1               // downtown tower (windows, iso sides)
#define MOUNTAIN 2
#define CLOUD    3
#define HOUSE    4               // suburban house (pitched roof)
#define FARM     5               // rural barn / farmhouse / silo
#define MAXP   160000
static float px_[MAXP], py_[MAXP], ph[MAXP];   // world x, world y, height/size/altitude
static unsigned char ptype[MAXP], pv[MAXP];     // type, variant
static int nprop = 0;
static int nstatic = 0;          // props [0,nstatic) are fixed → spatial-gridded; the rest are clouds

// camera / flight state
static float cx, cy, ang, spd, H;       // world pos, heading, speed, altitude
static float bank, pitch;               // visual roll & pitch of the plane model
#define SPD_MAX 16.0f

// flight mode — you can land, take off, and crash into terrain / buildings / mountains
#define ST_FLYING   0
#define ST_LANDED   1
#define ST_CRASHED  2
static int   fly_state = ST_FLYING;
static float crash_t   = 0;             // now() at the moment of the crash (drives the explosion)
#define PITCH_INVERT 1                  // 1: UP dives, DOWN climbs (flight-stick feel). 0: UP climbs.
#define ALT_MAX     200.0f              // ceiling — above the tallest ridge crest plus clearance
#define GROUND_CLR   3.0f               // altitude held above the local terrain when wheels are down
#define LAND_SPD     3.5f               // touchdown must be at or under this speed
#define TAKEOFF_SPD  2.6f               // ground speed needed to rotate & lift off

// layered noise → terrain height 0..1 (same field the ground is baked from).
// Frequencies are 16× lower than the old 512 world, so the continent scales up
// instead of fragmenting into lakes-everywhere. A radial falloff sinks the rim
// into deep ocean, so the land reads as one explorable place with edges.
static float terrain(float x, float y) {
    float h = 0.55f * noise2(x * 0.00034f, y * 0.00034f)   // continents      (period ~2900)
            + 0.28f * noise2(x * 0.0010f,  y * 0.0010f)    // hills & regions (period ~1000)
            + 0.12f * noise2(x * 0.0028f,  y * 0.0028f);   // coastline detail (period ~360)
    // ridged noise → sharp mountain SPINES, so real ranges rise out of the land
    float rg = noise2(x * 0.0006f + 19.0f, y * 0.0006f + 7.0f);
    float ridge = 1.0f - fabsf(2.0f * rg - 1.0f);          // 0..1, ridge-lines where rg≈0.5
    h += powf(ridge, 3.0f) * 0.55f;                        // cubed = narrow, tall crests (not mesas)
    float dx = (x - WORLD * 0.5f) / (WORLD * 0.5f);
    float dy = (y - WORLD * 0.5f) / (WORLD * 0.5f);
    float r  = sqrtf(dx * dx + dy * dy);                   // 0 centre … 1 edge … 1.41 corner
    float coast = clamp(1.22f - r * 1.12f, 0.0f, 1.0f);    // 1 inland → 0 at the rim
    float t = h * (0.34f + 0.66f * coast);                 // rim → deep sea, interior → full height
    return t > 1.4f ? 1.4f : t;                            // soft cap — peaks tower well above PEAK
}

static void add_prop(float x, float y, int type, float h, int variant) {
    if (nprop >= MAXP) return;
    px_[nprop] = x; py_[nprop] = y; ptype[nprop] = type; ph[nprop] = h; pv[nprop] = variant;
    nprop++;
}

// stamp a patchwork field over grass: subdivide a square into colour strips and
// recolour only the texels that are currently grass (so coast/forest stay intact)
static void paint_field(float wx, float wy, float size, unsigned int seed) {
    int tx0 = (int)((wx - size * 0.5f) * T2W), ty0 = (int)((wy - size * 0.5f) * T2W);
    int ts  = (int)(size * T2W); if (ts < 2) ts = 2;
    int strips = 2 + (int)(seed % 3);                  // 2..4 furrow strips
    int horiz  = seed & 1;
    int cols[4] = { CLR_LIME_GREEN, CLR_MEDIUM_GREEN, CLR_BROWN, CLR_LIGHT_YELLOW };
    for (int dy = 0; dy < ts; dy++)
        for (int dx = 0; dx < ts; dx++) {
            int tx = tx0 + dx, ty = ty0 + dy;
            if (tx < 0 || tx >= TEX || ty < 0 || ty >= TEX) continue;
            if (world[ty * TEX + tx] != CLR_DARK_GREEN) continue;   // grass only
            int s = (horiz ? dy : dx) * strips / ts;
            world[ty * TEX + tx] = cols[(s + (seed >> 4)) & 3];
        }
}

// ── spatial grid — buckets the fixed props so a frame only touches nearby ones ──
#define GRID 96                          // 96×96 buckets over the world (~85 units each)
static int bstart[GRID * GRID + 1];      // prefix-sum bucket offsets into porder[]
static int porder[MAXP];                 // prop indices grouped by bucket

static int cell_of(float x, float y) {
    int gx = (int)(x * (GRID / WORLD)), gy = (int)(y * (GRID / WORLD));
    gx = (int)clamp(gx, 0, GRID - 1);
    gy = (int)clamp(gy, 0, GRID - 1);
    return gy * GRID + gx;
}
static void build_grid(int n) {          // counting-sort props [0,n) into buckets
    static int cur[GRID * GRID];
    for (int i = 0; i <= GRID * GRID; i++) bstart[i] = 0;
    for (int i = 0; i < n; i++) bstart[cell_of(px_[i], py_[i]) + 1]++;
    for (int i = 1; i <= GRID * GRID; i++) bstart[i] += bstart[i - 1];
    for (int i = 0; i < GRID * GRID; i++) cur[i] = bstart[i];
    for (int i = 0; i < n; i++) { int c = cell_of(px_[i], py_[i]); porder[cur[c]++] = i; }
}

// ── per-frame visible-prop list (projected billboards, painter-sorted) ─────────
typedef struct { float d; int sx, gy; float sc; int idx; } Vis;
#define VISMAX 4096
static Vis  vis[VISMAX]; static int nvis;
static float g_ca, g_sa, g_cxs; static int g_hz;   // projection frame constants for project()
static int  ybuf[SCREEN_W];                        // voxel terrain silhouette (top y per column)

// project one prop into vis[] (or drop it if behind, too far, or off-screen)
static void project(int i) {
    if (nvis >= VISMAX) return;
    float dxx = px_[i] - cx, dyy = py_[i] - cy;
    float fwd = dxx * g_ca + dyy * g_sa;
    int type = ptype[i];
    float md = type == TREE ? 80 : type == HOUSE ? 130 : type == BUILDING ? 180
             : type == FARM ? 150 : type == MOUNTAIN ? 256 : 200;
    float nd = type == CLOUD ? 42 : 4;
    if (fwd < nd || fwd > md) return;
    float sc = F / fwd;
    int sx = (int)(g_cxs + (dxx * (-g_sa) + dyy * g_ca) * sc);
    if (sx < -90 || sx > SCREEN_W + 90) return;
    float e = elev_at(px_[i], py_[i]);                  // the prop stands on the terrain surface
    int gy = (int)(g_hz + (H - e) * sc);
    vis[nvis].d = fwd; vis[nvis].sx = sx; vis[nvis].sc = sc;
    vis[nvis].gy = gy; vis[nvis].idx = i; nvis++;
}

// is a building or mountain tall enough to hit us within arm's reach? (uses the grid)
static int collide_check(void) {
    int pgx = (int)(cx * (GRID / WORLD)), pgy = (int)(cy * (GRID / WORLD));
    for (int gy = pgy - 1; gy <= pgy + 1; gy++) {
        if (gy < 0 || gy >= GRID) continue;
        for (int gx = pgx - 1; gx <= pgx + 1; gx++) {
            if (gx < 0 || gx >= GRID) continue;
            int b = gy * GRID + gx;
            for (int k = bstart[b]; k < bstart[b + 1]; k++) {
                int i = porder[k], t = ptype[i];
                if (t != BUILDING) continue;                  // mountains are terrain now, not props
                float dx = px_[i] - cx, dy = py_[i] - cy;
                float top = elev_at(px_[i], py_[i]) + ph[i];  // building sits on the terrain
                if (dx * dx + dy * dy < 5.5f * 5.5f && H < top) return 1;
            }
        }
    }
    return 0;
}

static void do_crash(void) {
    if (fly_state == ST_CRASHED) return;
    fly_state = ST_CRASHED; crash_t = now();
    shake(9); hit(30, 2, 6, 700);                 // a low boom + a screen kick
}

static float spawn_cx, spawn_cy, spawn_H;         // scenic start: approaching the tallest peak

static void reset_flight(void) {                  // (re)start the flight at the scenic spawn
    fly_state = ST_FLYING; ang = 0; spd = 1.6f; bank = 0; pitch = 0;   // ang 0 heads +x toward the peak
    cx = spawn_cx; cy = spawn_cy; H = spawn_H;
}

void init(void) {
    touch_controls(true);   // floating stick (left) steers L/R/U/D, A/B (right) = throttle — touch/mouse playable
    // 1. bake the earth: noise → terrain colour bands (texel grid → world coords)
    for (int ty = 0; ty < TEX; ty++)
        for (int tx = 0; tx < TEX; tx++) {
            float wx = tx / T2W, wy = ty / T2W;
            // per-texel hash jitter dithers the band boundaries so the colour
            // thresholds stipple into each other instead of drawing hard contour lines
            unsigned int hh = (unsigned int)(tx * 374761393u + ty * 668265263u);
            hh = (hh ^ (hh >> 13)) * 1274126177u;
            float jitter = ((hh & 1023) / 1023.0f - 0.5f) * 0.05f;
            float t = terrain(wx, wy);
            float h = t + jitter;
            int c;
            if      (h < 0.30f) c = CLR_DARK_BLUE;     // deep sea
            else if (h < 0.42f) c = CLR_TRUE_BLUE;     // sea
            else if (h < 0.47f) c = CLR_BLUE;          // shallows
            else if (h < 0.50f) c = CLR_LIGHT_PEACH;   // beach
            else if (h < 0.60f) c = CLR_DARK_GREEN;    // grass
            else if (h < 0.72f) c = CLR_MEDIUM_GREEN;  // forest
            else if (h < 0.82f) c = CLR_BROWN;         // rock
            else if (h < 0.90f) c = CLR_DARK_GREY;     // crag
            else                c = CLR_WHITE;         // snow
            world[ty * TEX + tx] = c;
            // real height for the voxel renderer: base relief + closer-spaced hills &
            // bumps so the ground visibly rolls as you fly (ramped in from the shoreline)
            float e = elev_of(t);
            float land = clamp((t - 0.50f) / 0.15f, 0.0f, 1.0f);   // 0 at the coast → 1 inland
            e += noise2(wx * 0.004f, wy * 0.004f) * 24.0f * land;  // hills      (period ~250)
            e += noise2(wx * 0.011f, wy * 0.011f) * 9.0f  * land;  // fine bumps (period ~90)
            hmap[ty * TEX + tx] = e;
        }

    // 1b. farmland — patchwork fields stamped over lowland grass, clustered into
    //     agricultural regions by a slow noise mask so they read as real countryside
    for (int i = 0; i < 9000; i++) {
        float wx = rnd_float_between(0, WORLD), wy = rnd_float_between(0, WORLD);
        float h = terrain(wx, wy);
        if (h < 0.50f || h > 0.60f) continue;                        // lowland grass only
        if (noise2(wx * 0.0018f, wy * 0.0018f) < 0.55f) continue;    // only in farming regions
        unsigned int seed = (unsigned int)(wx * 7.0f + wy * 131.0f) + (unsigned int)i * 2654435761u;
        paint_field(wx, wy, rnd_float_between(70, 170), seed);
    }

    nprop = 0;
    ncity = 0;

    // 2. forests — trees scattered through the forest band, all across the continent
    for (int i = 0; i < 500000; i++) {
        float x = rnd_float_between(0, WORLD), y = rnd_float_between(0, WORLD);
        float h = terrain(x, y);
        if (h > 0.58f && h < 0.74f && chance(70))
            add_prop(x, y, TREE, rnd_float_between(1.4f, 2.6f), rnd(2));   // round or pine
    }

    // (mountains are no longer billboards — the voxel terrain renders real peaks)

    // 4. towns — building clusters on habitable coast, each given a unique name
    int s0 = rnd(NPREF * NSUF);                                  // rotate the name grid each flight
    for (int tries = 0; tries < 400000 && ncity < MAXCITY; tries++) {
        float cxx = rnd_float_between(0, WORLD), cyy = rnd_float_between(0, WORLD);
        float h = terrain(cxx, cyy);
        if (h < 0.50f || h > 0.62f) continue;                    // grass / coastal flats only
        int tooclose = 0;                                        // keep towns spread out
        for (int c = 0; c < ncity; c++) {
            float ddx = cityx[c] - cxx, ddy = cityy[c] - cyy;
            if (ddx * ddx + ddy * ddy < 340.0f * 340.0f) { tooclose = 1; break; }
        }
        if (tooclose) continue;
        int combo = (s0 + ncity * 7) % (NPREF * NSUF);           // step 7 is coprime to 360 → distinct
        snprintf(citynamebuf[ncity], sizeof(citynamebuf[0]), "%s%s",
                 PREFIX[combo / NSUF], SUFFIX[combo % NSUF]);
        cityname[ncity] = citynamebuf[ncity];
        cityx[ncity] = cxx; cityy[ncity] = cyy;
        ncity++;

        // downtown — tall towers, tight cluster, tallest dead centre
        int core = rnd_between(18, 42);
        for (int b = 0; b < core; b++) {
            float ox = (rnd_float() + rnd_float() - 1.0f) * 13;  // tight gaussian
            float oy = (rnd_float() + rnd_float() - 1.0f) * 13;
            float bx = cxx + ox, by = cyy + oy;
            if (terrain(bx, by) < 0.48f) continue;               // don't build in the sea
            float dist = length((int)ox, (int)oy);
            float height = remap(clamp(dist, 0, 16), 0, 16, 26, 11) * rnd_float_between(0.8f, 1.2f);
            add_prop(bx, by, BUILDING, clamp(height, 8, 30), rnd(3));
        }
        // suburbs — low houses spread in a much wider ring, with the odd garden tree
        int subs = rnd_between(80, 160);
        for (int b = 0; b < subs; b++) {
            float ox = (rnd_float() + rnd_float() - 1.0f) * 48;  // wide gaussian
            float oy = (rnd_float() + rnd_float() - 1.0f) * 48;
            float bx = cxx + ox, by = cyy + oy;
            if (terrain(bx, by) < 0.49f) continue;
            add_prop(bx, by, HOUSE, rnd_float_between(3.5f, 7.0f), rnd(4));
            if (chance(22))
                add_prop(bx + rnd_float_between(-3, 3), by + rnd_float_between(-3, 3),
                         TREE, rnd_float_between(1.3f, 2.2f), rnd(2));
        }
    }

    // 4b. rural — scattered farmsteads (a farmhouse, often a barn & silo) out on the
    //     lowland grass: the same agricultural land the fields are stamped onto
    for (int i = 0; i < 200000; i++) {
        float wx = rnd_float_between(0, WORLD), wy = rnd_float_between(0, WORLD);
        float h = terrain(wx, wy);
        if (h < 0.50f || h > 0.60f || !chance(6)) continue;
        add_prop(wx, wy, FARM, rnd_float_between(5, 8), 1);                          // farmhouse
        if (chance(60)) add_prop(wx + rnd_float_between(-9, 9), wy + rnd_float_between(-9, 9),
                                 FARM, rnd_float_between(7, 11), 0);                 // barn
        if (chance(30)) add_prop(wx + rnd_float_between(-7, 7), wy + rnd_float_between(-7, 7),
                                 FARM, rnd_float_between(9, 14), 2);                 // silo
    }

    // everything so far is fixed → bucket it for cheap per-frame culling
    nstatic = nprop;
    build_grid(nstatic);

    // 5. clouds — billboards lifted high into the sky, drifting on the wind.
    //    Clouds move, so they are NOT gridded; there are few enough to scan each frame.
    for (int i = 0; i < 4000; i++)
        add_prop(rnd_float_between(0, WORLD), rnd_float_between(0, WORLD), CLOUD,
                 rnd_float_between(24, 64),                       // altitude
                 (int)rnd_float_between(5, 10));                  // puff size

    // scenic spawn — find the spot of strongest LOCAL relief at moderate elevation
    // (hill-and-valley country, not the flat snow massif), so you open on terrain whose
    // shape and shading actually read. Spawn just above it, looking across the relief.
    float bestR = -1; int bx = TEX / 2, by = TEX / 2;
    for (int ty = 24; ty < TEX - 24; ty += 4)
        for (int tx = 24; tx < TEX - 24; tx += 4) {
            float e = hmap[ty * TEX + tx];
            if (e < 14.0f || e > 95.0f) continue;             // skip coast/sea and extreme peaks
            float rel = fabsf(e - hmap[(ty + 14) * TEX + (tx + 14)]);  // slope over ~110 units
            if (rel > bestR) { bestR = rel; bx = tx; by = ty; }
        }
    spawn_cx = bx / T2W; spawn_cy = by / T2W;
    spawn_H = hmap[by * TEX + bx] + 30.0f;                     // ride low over the slope for big parallax
    if (spawn_H > ALT_MAX - 30.0f) spawn_H = ALT_MAX - 30.0f;
    reset_flight();

    // a soft engine drone
    instrument(5, INSTR_TRI, 120, 200, 5, 300);
    instrument_filter(5, FILTER_LOW, 600, 4);
}


void update(void) {
    // drift the clouds on a steady wind (always — the sky lives even on the ground)
    for (int i = nstatic; i < nprop; i++) {
        px_[i] += 0.20f; py_[i] += 0.06f;
        if (px_[i] >= WORLD) px_[i] -= WORLD;
        if (py_[i] >= WORLD) py_[i] -= WORLD;
    }

#ifdef DE_TRACE
    watch("st",   "%d",   fly_state);
    watch("H",    "%.1f", H);
    watch("spd",  "%.2f", spd);
    watch("terr", "%.2f", terrain(cx, cy));
#endif

    // wrecked — wait for a restart
    if (fly_state == ST_CRASHED) {
        if (btnp(0, BTN_A) || btnp(1, BTN_A) || keyp(KEY_SPACE)) reset_flight();
        return;
    }

    float turn = 0;
    if (btn(0, BTN_LEFT)  || btn(1, BTN_LEFT))  { ang -= 1.9f; turn = -1; }
    if (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) { ang += 1.9f; turn =  1; }

    float spdmin = (fly_state == ST_LANDED) ? 0.0f : 0.5f;        // can roll to a stop on the ground
    if (btn(0, BTN_A) || btn(1, BTN_A)) spd = clamp(spd + 0.15f, spdmin, SPD_MAX);  // Z throttle up
    if (btn(0, BTN_B) || btn(1, BTN_B)) spd = clamp(spd - 0.15f, spdmin, SPD_MAX);  // X throttle down

    int upk = btn(0, BTN_UP)   || btn(1, BTN_UP);
    int dnk = btn(0, BTN_DOWN) || btn(1, BTN_DOWN);
    int kClimb   = PITCH_INVERT ? dnk : upk;                      // gain altitude / take off
    int kDescend = PITCH_INVERT ? upk : dnk;                      // lose altitude / land
    float climbDir = 0;

    float groundE = elev_at(cx, cy);                              // terrain height right under us
    float floorH  = groundE + GROUND_CLR;                         // altitude with the wheels on the deck

    if (fly_state == ST_LANDED) {
        H = floorH;                                               // ride the terrain while taxiing
        if (terrain(cx, cy) < 0.46f) do_crash();                  // taxied off into the sea
        if (kClimb && spd >= TAKEOFF_SPD) { fly_state = ST_FLYING; climbDir = -1; }
    } else {                                                      // ST_FLYING
        if (kClimb)   { H = clamp(H + 0.7f, floorH, ALT_MAX); climbDir = -1; }
        if (kDescend) {
            if (H > floorH + 6) { H -= 0.7f; climbDir = 1; }      // ordinary descent toward the deck
            else {                                                // near the ground → commit to a touchdown
                float ahead = elev_at(cx + cos_deg(ang) * 8, cy + sin_deg(ang) * 8);
                float tg = terrain(cx, cy);                       // beach/grass only (not sea, forest or rock)
                int landable = (tg >= 0.47f && tg <= 0.62f && fabsf(ahead - groundE) < 3.0f);
                if (landable && spd <= LAND_SPD && !collide_check()) {
                    H -= 0.5f; climbDir = 1;
                    if (H <= floorH) { H = floorH; fly_state = ST_LANDED; }
                } else {
                    do_crash();                                   // too fast, or over water / slopes / town
                }
            }
        }
        if (H - groundE < 8) { float d = spd * 0.985f; spd = d < 0.5f ? 0.5f : d; }  // approach drag
        if (fly_state == ST_FLYING && collide_check())   do_crash();  // flew into a tower
        if (fly_state == ST_FLYING && H < groundE + 1.5f) do_crash();  // flew into a hillside / peak
    }

    // visual bank / pitch ease toward the input — a plane banks INTO the turn
    bank  = lerp(bank,  turn  * (fly_state == ST_LANDED ? -8 : -30), 0.12f);
    pitch = lerp(pitch, climbDir *  16, 0.10f);

    // move forward (fly, or taxi on the ground)
    cx += cos_deg(ang) * spd;
    cy += sin_deg(ang) * spd;

    // engine drone — pitch rises with throttle (quieter idling on the ground)
    if (frame() % 11 == 0) hit(36 + (int)(spd * 3), 5, fly_state == ST_LANDED ? 1 : 2, 240);
}

// ── billboard drawing — every prop rises from its ground point (sx,gy) and is
//    scaled by `sc` (= F/distance), so far things are small. ph[i] = world size. ──
static void draw_tree(int sx, int gy, float sc, float size, int variant) {
    float r = size * sc;
    if (r < 1) { pset(sx, gy, CLR_DARK_GREEN); return; }
    int tw = max(1, (int)(sc * 0.5f));
    int th = max(1, (int)(size * sc * 0.6f));
    rectfill(sx - tw / 2, gy - th, tw, th, CLR_BROWN);             // trunk
    int cy = gy - th;
    if (variant == 0) {                                            // round, leafy
        circfill(sx, cy - (int)r, (int)r, CLR_DARK_GREEN);
        circfill(sx - (int)(r * 0.3f), cy - (int)(r * 1.2f), (int)(r * 0.7f), CLR_MEDIUM_GREEN);
    } else {                                                       // pine
        trifill(sx, cy - (int)(r * 2.2f), sx - (int)r, cy, sx + (int)r, cy, CLR_DARK_GREEN);
        trifill(sx, cy - (int)(r * 1.4f), sx - (int)(r * 0.8f), cy - (int)(r * 0.4f),
                sx + (int)(r * 0.8f), cy - (int)(r * 0.4f), CLR_MEDIUM_GREEN);
    }
}

static void draw_mountain(int sx, int gy, float sc, float height, int snowy) {
    float sw = (10.0f + height * 0.25f) * sc;
    float sh = height * sc;
    int apexx = sx,            apexy = gy - (int)sh;
    int bl = sx - (int)(sw / 2), br = sx + (int)(sw / 2);
    if (sw < 2) { rectfill(sx, apexy, 1, (int)sh, CLR_DARK_GREY); return; }
    trifill(apexx, apexy, bl, gy, br, gy, CLR_MEDIUM_GREY);                 // lit rock
    trifill(apexx, apexy, sx, gy, br, gy, CLR_DARK_GREY);                   // shaded right face
    if (snowy) {                                                           // snow cap
        int capy = apexy + (int)(sh * 0.32f);
        int capw = (int)(sw * 0.20f);
        trifill(apexx, apexy, apexx - capw, capy, apexx + capw, capy, CLR_WHITE);
    }
}

static void draw_building(int sx, int gy, float sc, float height, int variant) {
    int front, side, top;
    if      (variant == 0) { front = CLR_LIGHT_GREY; side = CLR_DARK_GREY;  top = CLR_WHITE; }
    else if (variant == 1) { front = CLR_TRUE_BLUE;  side = CLR_DARK_BLUE;  top = CLR_BLUE;  }
    else                   { front = CLR_MEDIUM_GREY;side = CLR_DARK_BROWN; top = CLR_LIGHT_PEACH; }

    float sw = 4.5f * sc;
    float sh = height * sc;
    if (sw < 1.2f) { rectfill(sx, gy - (int)sh, 1, (int)sh, side); return; }
    int x0 = sx - (int)(sw / 2);
    int w  = max(1, (int)sw);
    int y0 = gy - (int)sh;
    int hh = max(1, (int)sh);
    int ox = (int)(sw * 0.42f), oy = -(int)(sw * 0.30f);          // iso depth offset (up-right)

    // top face
    trifill(x0, y0, x0 + w, y0, x0 + w + ox, y0 + oy, top);
    trifill(x0, y0, x0 + w + ox, y0 + oy, x0 + ox, y0 + oy, top);
    // right side face
    trifill(x0 + w, y0, x0 + w, gy, x0 + w + ox, gy + oy, side);
    trifill(x0 + w, y0, x0 + w + ox, gy + oy, x0 + w + ox, y0 + oy, side);
    // front face (nearest → drawn on top)
    rectfill(x0, y0, w, hh, front);

    // windows — only when the building is big enough on screen to read
    if (w >= 12 && hh >= 14) {
        unsigned int seed = (unsigned int)(sx * 73 + gy * 31 + w * 17);
        for (int wy = y0 + 4; wy < gy - 4; wy += 6)
            for (int wx = x0 + 3; wx < x0 + w - 3; wx += 5) {
                seed = seed * 1103515245u + 12345u;
                int lit = (seed >> 16) & 7;
                rectfill(wx, wy, 2, 3, lit == 0 ? CLR_YELLOW : (variant == 1 ? CLR_DARKER_BLUE : CLR_DARK_GREY));
            }
    }
}

static void draw_cloud(int sx, int cloudy, float sc, float size) {
    float s = clamp(size * sc, 2, 52);
    ovalfill(sx, cloudy + (int)(s * 0.15f), (int)s, (int)(s * 0.55f), CLR_LIGHT_GREY); // soft underside
    ovalfill(sx - (int)(s * 0.55f), cloudy, (int)(s * 0.6f), (int)(s * 0.45f), CLR_WHITE);
    ovalfill(sx + (int)(s * 0.55f), cloudy, (int)(s * 0.6f), (int)(s * 0.45f), CLR_WHITE);
    ovalfill(sx, cloudy - (int)(s * 0.30f), (int)(s * 0.75f), (int)(s * 0.5f), CLR_WHITE);
}

// suburban house: a small body with a gable roof, a door, and a lit window
static void draw_house(int sx, int gy, float sc, float height, int variant) {
    int wall, roof;
    if      (variant == 0) { wall = CLR_LIGHT_PEACH; roof = CLR_DARK_RED;   }
    else if (variant == 1) { wall = CLR_WHITE;       roof = CLR_BROWN;      }
    else if (variant == 2) { wall = CLR_LIGHT_GREY;  roof = CLR_DARK_GREY;  }
    else                   { wall = CLR_PEACH;       roof = CLR_DARK_BROWN; }
    float sw = 5.0f * sc, sh = height * sc;
    if (sw < 1.5f) { pset(sx, gy - (int)max(1, (int)sh), wall); return; }
    int w = max(2, (int)sw);
    int x0 = sx - w / 2;
    int bodyh = max(1, (int)(sh * 0.62f));
    int y0 = gy - bodyh;
    rectfill(x0, y0, w, bodyh, wall);                               // body
    int roofh = max(1, (int)(sh * 0.5f));
    trifill(x0 - 1, y0, sx, y0 - roofh, x0 + w + 1, y0, roof);      // gable roof
    if (w >= 5 && bodyh >= 4) {
        int dh = max(2, bodyh / 2);
        rectfill(sx - 1, gy - dh, 2, dh, CLR_DARK_BROWN);           // door
        pset(x0 + 1, y0 + 1, CLR_LIGHT_YELLOW);                     // lit window
    }
}

// rural building: red barn (0), farmhouse (1), or grain silo (2)
static void draw_farm(int sx, int gy, float sc, float height, int variant) {
    float sh = height * sc;
    if (variant == 2) {                                            // silo: thin cylinder + domed cap
        float sw = 2.6f * sc; int w = max(1, (int)sw);
        int x0 = sx - w / 2, y0 = gy - (int)sh;
        rectfill(x0, y0, w, (int)sh, CLR_LIGHT_GREY);
        circfill(sx, y0, max(1, w / 2), CLR_MEDIUM_GREY);
        return;
    }
    int wall = variant == 0 ? CLR_DARK_RED       : CLR_LIGHT_PEACH;  // 0 red barn, 1 farmhouse
    int roof = variant == 0 ? CLR_BROWNISH_BLACK : CLR_BROWN;
    float sw = 6.5f * sc;
    if (sw < 1.5f) { rectfill(sx, gy - (int)sh, 1, (int)sh, wall); return; }
    int w = max(2, (int)sw);
    int x0 = sx - w / 2;
    int bodyh = max(1, (int)(sh * 0.62f));
    int y0 = gy - bodyh;
    rectfill(x0, y0, w, bodyh, wall);                              // body
    int roofh = max(1, (int)(sh * 0.5f));
    trifill(x0 - 1, y0, sx, y0 - roofh, x0 + w + 1, y0, roof);     // peaked roof
    if (variant == 0 && w >= 6)                                    // big barn door
        rectfill(sx - max(1, w / 6), gy - bodyh + 1, max(2, w / 3), bodyh - 1, CLR_BROWNISH_BLACK);
}

// ── the plane: a low-poly model, flat-shaded (solid3d technique) ────────────────
// (V3 and zsort are engine helpers now; the plane keeps its own roll-order
//  rotation + custom projection — see studio.h.)
static V3 PV[] = {
    /*0 N */{0,0,3.4f},   /*1 T */{0,0.15f,-2.4f}, /*2 U */{0,0.5f,0.3f}, /*3 D */{0,-0.4f,0.3f},
    /*4 L */{-0.5f,0.05f,0.3f}, /*5 R */{0.5f,0.05f,0.3f},
    /*6 */{-0.45f,0.05f,0.7f}, /*7 */{-0.45f,0.05f,-1.1f}, /*8 */{-3.3f,0.2f,-0.3f}, /*9 */{-3.3f,0.2f,-0.9f},   // L wing
    /*10*/{0.45f,0.05f,0.7f},  /*11*/{0.45f,0.05f,-1.1f},  /*12*/{3.3f,0.2f,-0.3f},  /*13*/{3.3f,0.2f,-0.9f},    // R wing
    /*14*/{0,0.0f,-1.7f}, /*15*/{0,0.15f,-2.4f}, /*16*/{0,1.1f,-2.4f},                                           // tail fin (points UP)
    /*17*/{-1.3f,0.05f,-2.2f}, /*18*/{1.3f,0.05f,-2.2f}, /*19*/{0,0.1f,-1.8f}, /*20*/{0,0.1f,-2.4f},             // tailplane
    /*21*/{-0.25f,0.5f,1.2f}, /*22*/{0.25f,0.5f,1.2f}, /*23*/{0.25f,0.55f,0.2f}, /*24*/{-0.25f,0.55f,0.2f},      // canopy
};
// each face: 3 vertex indices + bright colour + dark colour
static int PF[][5] = {
    {0,2,4, CLR_LIGHT_PEACH, CLR_BROWN}, {0,5,2, CLR_LIGHT_PEACH, CLR_BROWN},     // nose top
    {0,3,5, CLR_LIGHT_GREY,  CLR_DARK_GREY}, {0,4,3, CLR_LIGHT_GREY, CLR_DARK_GREY}, // nose bottom
    {1,2,5, CLR_LIGHT_GREY,  CLR_DARK_GREY}, {1,4,2, CLR_LIGHT_GREY, CLR_DARK_GREY}, // body back top
    {1,5,3, CLR_DARK_GREY,   CLR_DARKER_GREY}, {1,3,4, CLR_DARK_GREY, CLR_DARKER_GREY}, // body back bottom
    {6,8,9, CLR_RED, CLR_DARK_RED}, {6,9,7, CLR_RED, CLR_DARK_RED},               // left wing
    {10,13,12, CLR_RED, CLR_DARK_RED}, {10,11,13, CLR_RED, CLR_DARK_RED},         // right wing
    {14,16,15, CLR_RED, CLR_DARK_RED},                                            // tail fin
    {19,17,20, CLR_RED, CLR_DARK_RED}, {20,18,19, CLR_RED, CLR_DARK_RED},         // tailplane
    {21,22,23, CLR_DARK_BLUE, CLR_DARKER_BLUE}, {21,23,24, CLR_DARK_BLUE, CLR_DARKER_BLUE}, // canopy
};
#define NPF (int)(sizeof(PF) / sizeof(PF[0]))
#define FP_  8.0f
#define PS_  27.0f

static V3 plane_rot(V3 p, float roll, float ptch) {
    // roll about forward (z), then pitch about x (folds in the view tilt)
    float x =  p.x * cos_deg(roll) - p.y * sin_deg(roll);
    float y =  p.x * sin_deg(roll) + p.y * cos_deg(roll);
    float z =  p.z;
    float y2 =  y * cos_deg(ptch) - z * sin_deg(ptch);
    float z2 =  y * sin_deg(ptch) + z * cos_deg(ptch);
    return (V3){ x, y2, z2 };
}

static void draw_plane(void) {
    float ptch = -22 + pitch;         // tilt so we see the plane's TOP, nose up toward the horizon
    float bob  = (fly_state == ST_FLYING) ? sin_deg(now() * 90) * 1.5f : 0;  // settle on the ground
    float pcx = SCREEN_W / 2.0f, pcy = 126 + bob;

    static int sx[32], sy[32]; static V3 rv[32];
    int nv = (int)(sizeof(PV) / sizeof(PV[0]));
    for (int i = 0; i < nv; i++) {
        rv[i] = plane_rot(PV[i], bank, ptch);
        float f = FP_ / (FP_ + rv[i].z);
        sx[i] = (int)(pcx + rv[i].x * f * PS_);
        sy[i] = (int)(pcy - rv[i].y * f * PS_);
    }

    // sort faces far → near (no z-buffer)
    float fkey[NPF]; int forder[NPF];
    for (int i = 0; i < NPF; i++) {
        int *F_ = PF[i];
        fkey[i] = (rv[F_[0]].z + rv[F_[1]].z + rv[F_[2]].z) / 3;
    }
    zsort(fkey, forder, NPF);

    float lx = -0.5f, ly = 0.7f, lz = -0.5f;     // light from upper-left-front
    for (int i = 0; i < NPF; i++) {
        int *F_ = PF[forder[i]];
        int a = F_[0], b = F_[1], c = F_[2];
        V3 ra = rv[a], rb = rv[b], rc = rv[c];
        float ux = rb.x-ra.x, uy = rb.y-ra.y, uz = rb.z-ra.z;
        float vx = rc.x-ra.x, vy = rc.y-ra.y, vz = rc.z-ra.z;
        float nx = uy*vz-uz*vy, ny = uz*vx-ux*vz, nz = ux*vy-uy*vx;
        float len = sqrtf(nx*nx+ny*ny+nz*nz); if (len < 0.0001f) len = 1;
        float lam = fabsf((nx*lx+ny*ly+nz*lz) / len);
        int bright = F_[3], dark = F_[4];
        if (lam > 0.66f) {
            trifill(sx[a],sy[a], sx[b],sy[b], sx[c],sy[c], bright);
        } else if (lam > 0.33f) {
            fillp(FILL_CHECKER, dark);
            trifill(sx[a],sy[a], sx[b],sy[b], sx[c],sy[c], bright);
            fillp_reset();
        } else {
            trifill(sx[a],sy[a], sx[b],sy[b], sx[c],sy[c], dark);
        }
    }
}

// crash: a fireball that expands & fades, then a rising dithered smoke column
static void draw_wreck(void) {
    float e = now() - crash_t;
    int ex = SCREEN_W / 2, ey = 132;
    fillp(FILL_CHECKER, -1);
    for (int s = 0; s < 6; s++) {
        int sy = ey - (int)(e * 26) - s * 9;
        int r  = 9 - s; if (r < 2) r = 2;
        circfill(ex + (int)(sin_deg(e * 90 + s * 50) * 7), sy, r, s < 2 ? CLR_DARK_GREY : CLR_LIGHT_GREY);
    }
    fillp_reset();
    if (e < 0.7f) {                                       // the initial fireball
        int r = (int)(6 + e * 70);
        circfill(ex, ey, r, CLR_ORANGE);
        circfill(ex, ey, (int)(r * 0.6f), CLR_YELLOW);
        circfill(ex, ey, (int)(r * 0.3f), CLR_WHITE);
    }
}

// ── scrolling, player-centred minimap: live-sampled land + nearby town labels ──
static void draw_minimap(void) {
    int mw  = MINI * MZOOM;                                       // 96
    int mx0 = SCREEN_W - mw - 5, my0 = 6;
    int mcx = mx0 + mw / 2, mcy = my0 + mw / 2;
    float ustep = MAPSPAN / MINI;                                 // world units per minimap cell

    rectfill(mx0 - 2, my0 - 2, mw + 4, mw + 4, CLR_DARKER_BLUE);   // backing
    for (int my = 0; my < MINI; my++)
        for (int mx = 0; mx < MINI; mx++) {
            float wx = cx + (mx - MINI * 0.5f + 0.5f) * ustep;
            float wy = cy + (my - MINI * 0.5f + 0.5f) * ustep;
            rectfill(mx0 + mx * MZOOM, my0 + my * MZOOM, MZOOM, MZOOM, sample_world(wx, wy));
        }
    rect(mx0 - 2, my0 - 2, mw + 4, mw + 4, CLR_LIGHT_GREY);        // frame

    // towns within the window: a dot for each, plus a label for the nearest few
    // (placed nearest-first and skipped if it would collide with one already drawn).
    int inwin[MAXCITY], nin = 0;
    for (int i = 0; i < ncity; i++)
        if (fabsf(cityx[i] - cx) < MAPSPAN * 0.5f && fabsf(cityy[i] - cy) < MAPSPAN * 0.5f)
            inwin[nin++] = i;
    for (int a = 0; a < nin; a++)                                  // selection-sort nearest-first
        for (int b = a + 1; b < nin; b++) {
            float da = length((int)(cityx[inwin[a]] - cx), (int)(cityy[inwin[a]] - cy));
            float db = length((int)(cityx[inwin[b]] - cx), (int)(cityy[inwin[b]] - cy));
            if (db < da) { int t = inwin[a]; inwin[a] = inwin[b]; inwin[b] = t; }
        }

    font(FONT_TINY);
    int lx0[16], ly0[16], lx1[16], ly1[16], nlb = 0;              // placed-label boxes
    for (int k = 0; k < nin; k++) {
        int i = inwin[k];
        int dxp = mcx + (int)((cityx[i] - cx) / MAPSPAN * mw);
        int dyp = mcy + (int)((cityy[i] - cy) / MAPSPAN * mw);
        rectfill(dxp - 1, dyp - 1, 3, 3, CLR_YELLOW);
        pset(dxp, dyp, CLR_DARK_RED);
        if (nlb >= 7) continue;                                   // cap labels so it stays readable
        int tw = text_width(cityname[i]);
        int tx = dxp + 3, ty = dyp - 2;
        if (tx + tw > mx0 + mw) tx = dxp - 3 - tw;                 // flip inward at the right edge
        if (tx < mx0) continue;                                   // no room → dot only
        int bx0 = tx - 2, by0 = ty - 2, bx1 = tx + tw + 1, by1 = ty + 6;  // padded box → labels keep clear air
        int hit = 0;                                              // skip if it overlaps a placed label
        for (int j = 0; j < nlb; j++)
            if (bx0 <= lx1[j] && bx1 >= lx0[j] && by0 <= ly1[j] && by1 >= ly0[j]) { hit = 1; break; }
        if (hit) continue;
        lx0[nlb] = bx0; ly0[nlb] = by0; lx1[nlb] = bx1; ly1[nlb] = by1; nlb++;
        print(cityname[i], tx + 1, ty + 1, CLR_BLACK);            // shadow, then label
        print(cityname[i], tx,     ty,     CLR_WHITE);
    }
    font(FONT_NORMAL);

    // you-are-here: heading needle + a red dot, always at the centre of the map
    line(mcx, mcy, mcx + (int)(cos_deg(ang) * 9), mcy + (int)(sin_deg(ang) * 9), CLR_WHITE);
    circfill(mcx, mcy, 2, CLR_RED);
    pset(mcx, mcy, CLR_WHITE);

    // nearest-town callout under the map — where you're headed and how far
    int best = -1; float bestd = 1e9f;
    for (int i = 0; i < ncity; i++) {
        float dd = length((int)(cityx[i] - cx), (int)(cityy[i] - cy));
        if (dd < bestd) { bestd = dd; best = i; }
    }
    if (best >= 0)
        print_right(str("nearest %s  %d", cityname[best], (int)bestd),
                    mx0 + mw, my0 + mw + 5, CLR_LIGHT_YELLOW);
}

void draw(void) {
    // a fixed eye-level horizon; the climb/dive pitch nudges it for feel. Altitude is
    // handled by the voxel projection itself, not by sliding the horizon up and down.
    int hz = (int)clamp(HZ - pitch * 0.6f, 50, 150);

    float ca = cos_deg(ang), sa = sin_deg(ang);
    float cxs = SCREEN_W / 2.0f;

    // --- sky (full height; the terrain overdraws everything below its silhouette) ---
    rectfill(0, 0,  SCREEN_W, 28,            CLR_DARKER_BLUE);
    rectfill(0, 28, SCREEN_W, 24,            CLR_DARK_BLUE);
    rectfill(0, 52, SCREEN_W, 14,            CLR_TRUE_BLUE);
    rectfill(0, 66, SCREEN_W, SCREEN_H - 66, CLR_BLUE);
    circfill(SCREEN_W - 54, 26, 12, CLR_LIGHT_YELLOW);
    circfill(SCREEN_W - 54, 26, 16, CLR_LIGHT_YELLOW);   // soft glow (overdraw)

    // --- VOXEL TERRAIN: ray-march each column over the heightmap, front-to-back,
    //     painting vertical strips behind a y-buffer so near ground hides far ground.
    //     A ground point at distance z, elevation e lands on screen row
    //         sy = hz + (H - e) * F / z      (the old mode-7 formula, generalised to e). ---
    for (int x = 0; x < SCREEN_W; x++) ybuf[x] = SCREEN_H;
    float z = 4.0f, dz = 1.0f;
    while (z < 2000.0f) {
        float lat0  = (0 - cxs) / F * z;
        float wx = cx + ca * z + (-sa) * lat0;
        float wy = cy + sa * z + ( ca) * lat0;
        float stepx = -sa * (z / F) * BS;
        float stepy =  ca * (z / F) * BS;
        float inv = F / z;
        for (int sx = 0; sx < SCREEN_W; sx += BS) {
            int tx = (int)(wx * T2W), ty = (int)(wy * T2W);
            float e; int col;
            if (tx < 0 || tx >= TEX || ty < 0 || ty >= TEX) { e = 0; col = CLR_DARK_BLUE; }
            else { int idx = ty * TEX + tx; e = hmap[idx]; col = world[idx]; }
            int sy = hz + (int)((H - e) * inv);
            if (sy < ybuf[sx]) {
                // hillshade: a slope dropping away from the NW light reads as shadow
                float eL = elev_at(wx - 9.0f, wy - 9.0f);
                if (col != CLR_DARK_BLUE && e < eL - 1.5f) {     // shadowed face (skip flat sea)
                    fillp(FILL_CHECKER, CLR_BLACK);              // dither it darker
                    rectfill(sx, sy, BS, ybuf[sx] - sy, col);
                    fillp_reset();
                } else {
                    rectfill(sx, sy, BS, ybuf[sx] - sy, col);
                }
                for (int k = 0; k < BS && sx + k < SCREEN_W; k++) ybuf[sx + k] = sy;
            }
            wx += stepx; wy += stepy;
        }
        z += dz; dz += 0.06f;                            // LOD: coarser sampling with distance
    }
    // a thin atmospheric haze where the land meets the sky
    fillp(FILL_CHECKER, -1);
    rectfill(0, hz - 2, SCREEN_W, 3, CLR_TRUE_BLUE);
    fillp_reset();

    // --- BILLBOARDS: project only the props near you (spatial grid + clouds), sort, draw ---
    g_ca = ca; g_sa = sa; g_cxs = cxs; g_hz = hz; nvis = 0;
    int pgx = (int)(cx * (GRID / WORLD)), pgy = (int)(cy * (GRID / WORLD));
    const int R = 4;                                   // bucket radius covering the farthest view (mountains)
    for (int gy = pgy - R; gy <= pgy + R; gy++) {
        if (gy < 0 || gy >= GRID) continue;
        for (int gx = pgx - R; gx <= pgx + R; gx++) {
            if (gx < 0 || gx >= GRID) continue;
            int b = gy * GRID + gx;
            for (int k = bstart[b]; k < bstart[b + 1]; k++) project(porder[k]);
        }
    }
    for (int i = nstatic; i < nprop; i++) project(i);  // clouds (drift → not gridded)

    // painter's sort: far first
    static float vkey[VISMAX]; static int vorder[VISMAX];
    for (int i = 0; i < nvis; i++) vkey[i] = vis[i].d;
    zsort(vkey, vorder, nvis);
    for (int k = 0; k < nvis; k++) {
        Vis vv = vis[vorder[k]];
        int i = vv.idx;
        switch (ptype[i]) {
            case TREE:     draw_tree(vv.sx, vv.gy, vv.sc, ph[i], pv[i]); break;
            case MOUNTAIN: draw_mountain(vv.sx, vv.gy, vv.sc, ph[i], pv[i]); break;
            case BUILDING: draw_building(vv.sx, vv.gy, vv.sc, ph[i], pv[i]); break;
            case HOUSE:    draw_house(vv.sx, vv.gy, vv.sc, ph[i], pv[i]); break;
            case FARM:     draw_farm(vv.sx, vv.gy, vv.sc, ph[i], pv[i]); break;
            case CLOUD: {
                int cloudy = vv.gy - (int)(ph[i] * vv.sc);
                draw_cloud(vv.sx, cloudy, vv.sc, pv[i]);
                break;
            }
        }
    }

    // --- plane shadow on the ground (dithered ellipse, fades & shrinks with altitude) ---
    if (fly_state != ST_CRASHED) {
        float shs = clamp(remap(H, 12, 70, 16, 4), 4, 16);
        int   shy = SCREEN_H - 14 - (int)((H - 12) * 0.35f);
        fillp(FILL_CHECKER, -1);
        ovalfill(SCREEN_W / 2 + (int)(bank * 0.4f), shy, (int)shs, (int)(shs * 0.4f), CLR_BLACK);
        fillp_reset();
    }

    // --- the plane (or the wreck) ---
    if (fly_state == ST_CRASHED) draw_wreck();
    else                         draw_plane();

    // --- HUD: flight readouts (top-left) ---
    print(str("ALT %d", (int)H), 6, 6, CLR_WHITE);
    print(str("SPD %d%%", (int)(spd / SPD_MAX * 100)), 6, 16, CLR_LIGHT_GREY);
    int hdg = ((int)ang % 360 + 360) % 360;
    const char *dir[] = { "E", "SE", "S", "SW", "W", "NW", "N", "NE" };
    print(str("HDG %03d %s", hdg, dir[((hdg + 22) / 45) % 8]), 6, 26, CLR_LIGHT_GREY);
    if (fly_state == ST_LANDED) print("LANDED", 6, 36, CLR_LIME_GREEN);

    // --- TEMP touch-stick probe (remove once diagnosed): does the engine see the stick? ---
    print(str("sx %+.2f sy %+.2f", stick_x(), stick_y()), 6, 50, CLR_YELLOW);
    print(str("touch %d  L%d R%d U%d D%d A%d", touch_count(),
              btn(0, BTN_LEFT), btn(0, BTN_RIGHT), btn(0, BTN_UP),
              btn(0, BTN_DOWN), btn(0, BTN_A)), 6, 60, CLR_YELLOW);

    draw_minimap();

    // --- contextual flight prompt (centred) ---
    const char *climbKey = PITCH_INVERT ? "DOWN" : "UP";          // key that gains altitude
    const char *landKey  = PITCH_INVERT ? "UP"   : "DOWN";        // key that descends / lands
    if (fly_state == ST_CRASHED) {
        print_centered("CRASHED", SCREEN_W/2, SCREEN_H / 2 - 14, CLR_RED);
        print_centered("press Z to restart", SCREEN_W/2, SCREEN_H / 2 - 4, CLR_WHITE);
    } else if (fly_state == ST_LANDED) {
        print_centered(str("Z power up   %s take off   L/R taxi", climbKey), SCREEN_W/2, SCREEN_H - 9, CLR_DARK_GREY);
    } else if (H - elev_at(cx, cy) < 8) {
        print_centered(str("hold %s to land  (slow, over open ground)", landKey), SCREEN_W/2, SCREEN_H - 9, CLR_LIGHT_YELLOW);
    } else {
        print_centered(str("L/R turn   %s climb / %s dive   Z/X throttle", climbKey, landKey), SCREEN_W/2, SCREEN_H - 9, CLR_DARK_GREY);
    }
}
