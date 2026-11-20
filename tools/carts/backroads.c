/* de:meta
{
  "slug": "backroads",
  "title": "backroads",
  "status": "wip",
  "created": "2026-11-19",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "noise-terrain",
    "software-rasterizer",
    "dithering-gradient",
    "procedural-mesh",
    "road-network"
  ],
  "lineage": "Test Drive's cockpit view over worldnet.h's endless terrain; flat-shaded fillp ramps from solid3d; novel: LOD rings drawn back-to-front by grid order (no sort, no z-buffer), roads clipped into the terrain cells they lie on so painter's order still holds, and a cart-local near-plane clip.",
  "description": {
    "summary": "A 1970s cockpit-view drive (work in progress): flat-shaded, dithered countryside from the endless worldnet world, roads you can follow from town to town or leave for the fields.",
    "detail": "The ground is a grid of triangles sampled from worldnet's heightmap in three rings around the car: 16 m cells close by, 64 m further out, 256 m out to the fog. There is no z-buffer and no sort: a heightfield seen from inside it can be painted back-to-front just by walking the grid rows and columns from the far edge toward the camera, outer ring first. Roads are worldnet's own spline edges, cut into pieces and clipped into the exact terrain cell (and triangle) they lie on, then drawn right after that cell, so the same order hides a road behind a hill. Centre dashes and edge posts are placed by distance along the road, so they stay put as you drive past them. Leaving the road changes everything: plough and woods drag, grass lets the tail slide, and four wheels sampling the ground tilt the view over crests and ditches. Design doc: docs/design/backroads.md.",
    "controls": "UP gas, DOWN brake/reverse, LEFT/RIGHT steer. - / = big hills, [ / ] small hills, H toggles the debug readout, TAB opens the map (drag to pan, wheel zooms, centre button snaps back to the car). Drives an islandstudio island when one is exported (data/islandstudio-3.rvb or -1, --data, or drop a .rvb on the window); otherwise the endless worldnet world."
  }
}
de:meta */
#include "studio.h"
#include "worldnet.h"
#include "roadkit.h"     // curb_return: the shared at-grade junction geometry (streetlab, citydrive)
#include <math.h>
#include <stdlib.h>

// BACKROADS — rung 2: roads + ground feel.
//
//   world   worldnet.h, 1 unit = 1 m, y-down. height = height_at()*vscale + cart detail.
//   rings   three square vertex grids around the car (fine → coarse). Each ring skips the
//           square the next-finer ring covers; squares align to the next-coarser grid so
//           they tile exactly, and the finer ring's border heights are lerped from the
//           coarse corners so there are no cracks.
//   order   outer ring first; inside a ring, rows by |row - camera row| descending, then
//           columns the same way. For a heightfield seen from inside the grid that is a
//           correct painter's order: whatever hides a cell is nearer in both axes.
//   roads   worldnet's edge polylines → Catmull-Rom sub-pieces near the car → each piece
//           binned into every ring cell it touches, CLIPPED to that cell and to the cell's
//           triangle, and drawn on that exact plane right after the cell. So roads follow
//           the drawn ground exactly and a nearer hill still covers them.
//   clip    triangles crossing the near plane are clipped in camera space (cart-local;
//           evidence for ADR-0036's engine clip, not a replacement for it).

// ── view ───────────────────────────────────────────────────────────────────────
#define VIEW_H   134            // 3D viewport height; the dash covers the rest
#define HZ0      56             // horizon row at zero pitch
#define FOCAL    200.0f         // pixels; ~77° horizontal field of view at 320 wide
#define NEAR     0.35f          // near plane (m)
#define EYE      1.2f           // eye height above the ground (m)
#define FOG0     900.0f         // fog starts (m)
#define FOG1     2900.0f        // fully fogged (m)
#define D2R      0.0174533f

#define HAZE     CLR_MEDIUM_GREY

// ── terrain rings ──────────────────────────────────────────────────────────────
#define NRING 3
#define RMAX  33                // max vertices per ring side
static const float RS[NRING] = { 16.0f, 64.0f, 256.0f };   // cell size (m)
static const int   RN[NRING] = { 32, 32, 24 };              // cells per side
static float rx0[NRING], ry0[NRING];                        // ring origin (world m)
static float vcx[NRING][RMAX * RMAX], vcy[NRING][RMAX * RMAX], vcz[NRING][RMAX * RMAX];
static float vh[NRING][RMAX * RMAX], vt[NRING][RMAX * RMAX];

// ── state ──────────────────────────────────────────────────────────────────────
static float px, py;            // car position (m)
static float vx, vy;            // car velocity (m/s, world)
static float head = 0;          // heading (deg, atan2 frame: 0 = east, 90 = south)
static float steer = 0;         // -1..1 smoothed
static float pitch = 0, roll = 0;   // body attitude (rad); camera rides it
static float camh;              // eye height (m)
static float dash_shake = 0;         // dash shake (px), from surface roughness × speed
static float vscale = 900.0f;   // metres per height_at() unit — tune by eye
static float detail = 90.0f;    // metres of cart-local swell (see detail_at) — tune by eye
static int   show_map;            // TAB: north-up map instead of the cockpit
static float map_mpp = 60.0f;    // map zoom, metres per pixel (wheel)
static int   map_follow = 1;      // 1 = map centred on the car; a drag frees it, the button re-centres
static float map_cx, map_cy;      // map centre (world m) while not following
static int   map_drag, map_mx, map_my;                  // drag in progress + last mouse px
#define MAP_BTN_X (SCREEN_W - 50)                        // "centre" button, top-right of the map
#define MAP_BTN_Y 14
#define MAP_BTN_W 44
#define MAP_BTN_H 11
static int   dbg = 1;
static int   drone;               // dev view (O): camera 25 m up, tilted down, to read junction shapes
static int   ntri;              // triangles drawn this frame
static int   cur_surf;          // surface under the car

static float cfx, cfy, crx, cry, cpc, cps, crc, crs;      // camera basis

// ── ISLAND MODE: drive an islandstudio export instead of worldnet's endless world ────────
// docs/design/island-studio.md. The island is a .rvb (roadview's packed vector format, written by
// islandstudio X + data-tools/roadview/osm-roads.js --convert): a heightfield, land cover as area
// runs, roads on the OSM ladder (bridge spans tagged B1), rivers, settlements, farms. Loaded from
// --data / $DE_DATA, else ../data/islandstudio-3.rvb (then -1), or dropped on the window. No file = worldnet.
// The file's y is north-up; here y is down (south), so every y is flipped on load.
#include <stdio.h>
#include <string.h>
static int   ISL;                                 // 1 = an island is loaded
static char  isl_name[64];
static float isl_W = 40000, isl_H = 40000;        // extent (m)
#define IHF_MAX 256
static float ihf[IHF_MAX * IHF_MAX]; static int ihg;   // heights (m), row 0 = SOUTH (file order)
#define ICG 128
static unsigned char icv[ICG * ICG];              // cover per ~312 m cell, row 0 = NORTH (flipped)
enum { IC_LAND, IC_WATER, IC_SAND, IC_FARM, IC_WOOD, IC_HEATH, IC_SCRUB, IC_ROCK, IC_WET };
#define IPL_MAX 24000
#define IPT_MAX 320000
typedef struct { int start, n; unsigned char cls, bridge, river; } IPoly;
static IPoly ipl[IPL_MAX]; static int nipl;
static float ipx[IPT_MAX], ipy[IPT_MAX], ipS[IPT_MAX];   // points + arc length along their poly
static int   ip_poly[IPT_MAX], ib_next[IPT_MAX];
static int   nipt;
#define IBG 160                                   // 250 m buckets of segments (keyed by end point)
static int   ib_head[IBG * IBG];
typedef struct { float x, y, r; int rank; } ISet; // rank: 0 hamlet, 1 village, 2 town
#define ISET_MAX 800
static ISet  iset[ISET_MAX]; static int niset;
#define IFARM_MAX 8000
static float ifx[IFARM_MAX], ify[IFARM_MAX]; static int nifarm;

static float ihf_at(int i, int j) {               // i = column (west→east), j = row (south→north)
    i = i < 0 ? 0 : i >= ihg ? ihg - 1 : i; j = j < 0 ? 0 : j >= ihg ? ihg - 1 : j;
    return ihf[j * ihg + i];
}
static float cr1(float a, float b, float c, float d, float t) {
    return 0.5f * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (-a + 3 * b - 3 * c + d) * t * t * t);
}
static float isl_h(float x, float y) {            // bicubic: 156 m heightfield cells, 16 m drive cells
    if (!ihg) return 0;
    float fu = clamp(x / isl_W, 0, 1) * (ihg - 1), fv = clamp((isl_H - y) / isl_H, 0, 1) * (ihg - 1);
    int i0 = (int)floorf(fu), j0 = (int)floorf(fv);
    float tu = fu - i0, tv = fv - j0, r[4];
    for (int k = 0; k < 4; k++)
        r[k] = cr1(ihf_at(i0 - 1, j0 - 1 + k), ihf_at(i0, j0 - 1 + k), ihf_at(i0 + 1, j0 - 1 + k), ihf_at(i0 + 2, j0 - 1 + k), tu);
    return fmaxf(cr1(r[0], r[1], r[2], r[3], tv), 0);
}
static int isl_cover(float x, float y) {          // cell edges jittered so the 312 m grid never shows
    float jx = x + (noise2(x / 90.0f + 3.1f, y / 90.0f + 7.7f) - 0.5f) * 160.0f;
    float jy = y + (noise2(x / 90.0f + 9.4f, y / 90.0f + 1.3f) - 0.5f) * 160.0f;
    int i = (int)(jx / isl_W * ICG), j = (int)(jy / isl_H * ICG);
    if (i < 0 || j < 0 || i >= ICG || j >= ICG) return IC_WATER;
    return icv[j * ICG + i];
}
static int ib_of(float v, float ext) { int b = (int)(v / ext * IBG); return b < 0 ? 0 : b >= IBG ? IBG - 1 : b; }

// nearest road (or river) segment to (x,y) within ~250 m: distance, class, bridge, point, tangent
enum { NK_ROAD, NK_RIVER, NK_BRIDGE, NK_ANYROAD };    // what isl_nearest looks for
typedef struct { int ok, poly; float dist, qx, qy, tx, ty, s; } IHit;   // s = arc length along the poly
static IHit isl_nearest(float x, float y, int want, float maxd) {
    IHit h = { 0, -1, 1e30f, 0, 0, 1, 0, 0 };
    int bx = ib_of(x, isl_W), by = ib_of(y, isl_H), rb = (int)(maxd / (isl_W / IBG)) + 1;
    for (int j = by - rb; j <= by + rb; j++) for (int i = bx - rb; i <= bx + rb; i++) {
        if (i < 0 || j < 0 || i >= IBG || j >= IBG) continue;
        for (int k = ib_head[j * IBG + i]; k >= 0; k = ib_next[k]) {
            const IPoly *p = &ipl[ip_poly[k]];
            int kind = p->river ? NK_RIVER : p->bridge ? NK_BRIDGE : NK_ROAD;
            if (want == NK_ANYROAD ? kind == NK_RIVER : kind != want) continue;
            float ax = ipx[k - 1], ay = ipy[k - 1], dx = ipx[k] - ax, dy = ipy[k] - ay, l2 = dx * dx + dy * dy + 1e-6f;
            float t = clamp(((x - ax) * dx + (y - ay) * dy) / l2, 0, 1);
            float qx = ax + dx * t, qy = ay + dy * t, d = fsqrt((x - qx) * (x - qx) + (y - qy) * (y - qy));
            if (d < h.dist) { float l = fsqrt(l2); h.ok = 1; h.poly = ip_poly[k]; h.dist = d; h.qx = qx; h.qy = qy; h.tx = dx / l; h.ty = dy / l; h.s = ipS[k - 1] + t * l; }
        }
    }
    if (h.dist > maxd) h.ok = 0;
    return h;
}

static int isl_load(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len < 32) { fclose(f); return 0; }
    unsigned char *buf = (unsigned char *)malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(buf); return 0; }
    fclose(f);
    unsigned char *p = buf, *end = buf + len;
    #define RD(T, v) do { if (p + sizeof(T) > end) goto bad; memcpy(&(v), p, sizeof(T)); p += sizeof(T); } while (0)
    if (memcmp(p, "RVB", 3) != 0) goto bad;
    int ver = p[3] - '0'; p += 4;
    int nfeat, nlen; RD(int, nfeat); RD(int, nlen);
    if (nlen < 0 || p + nlen > end) goto bad;
    { int c = nlen < 63 ? nlen : 63; memcpy(isl_name, p, (size_t)c); isl_name[c] = 0; } p += nlen;
    float bb[4]; for (int k = 0; k < 4; k++) RD(float, bb[k]);
    isl_W = fmaxf(bb[2], 1000); isl_H = fmaxf(bb[3], 1000);
    nipl = nipt = niset = nifarm = 0; ihg = 0;
    memset(icv, IC_LAND, sizeof icv);
    for (int i = 0; i < IBG * IBG; i++) ib_head[i] = -1;
    for (int fi = 0; fi < nfeat; fi++) {
        int kind, sl, np; float fh = 0; char sub[48] = { 0 };
        RD(int, kind); if (ver >= 2) RD(float, fh);
        RD(int, sl); if (sl < 0 || p + sl > end) goto bad;
        memcpy(sub, p, (size_t)(sl < 47 ? sl : 47)); p += sl;
        RD(int, np); if (np < 0 || p + (long)np * 8 > end) goto bad;
        float *pts = (float *)malloc((size_t)(np > 0 ? np : 1) * 8);
        memcpy(pts, p, (size_t)np * 8); p += (long)np * 8;
        for (int k = 0; k < np; k++) pts[k * 2 + 1] = isl_H - pts[k * 2 + 1];   // flip: y down
        // kinds = osm-roads.js KIND_IX (append-only)
        int cls = kind == 0 ? CL_MOTORWAY : kind == 1 ? CL_HIGHWAY : (kind == 17 || kind == 18) ? CL_ARTERIAL
                : (kind == 2 || kind == 19) ? CL_STREET : kind == 3 ? CL_DIRT : -1;
        if ((cls >= 0 || kind == 5) && np >= 2 && nipl < IPL_MAX && nipt + np < IPT_MAX) {   // road or river
            IPoly *q = &ipl[nipl];
            q->start = nipt; q->n = np; q->cls = (unsigned char)(cls >= 0 ? cls : CL_DIRT);
            q->bridge = sub[0] == 'B'; q->river = kind == 5;
            float S = 0;
            for (int k = 0; k < np; k++) {
                int g = nipt + k;
                ipx[g] = pts[k * 2]; ipy[g] = pts[k * 2 + 1]; ip_poly[g] = nipl;
                if (k) S += fsqrt((ipx[g] - ipx[g - 1]) * (ipx[g] - ipx[g - 1]) + (ipy[g] - ipy[g - 1]) * (ipy[g] - ipy[g - 1]));
                ipS[g] = S; ib_next[g] = -1;
                if (k) {                                   // bucket the segment (g-1, g) by its midpoint
                    int b = ib_of((ipx[g] + ipx[g - 1]) * 0.5f, isl_W) + ib_of((ipy[g] + ipy[g - 1]) * 0.5f, isl_H) * IBG;
                    ib_next[g] = ib_head[b]; ib_head[b] = g;
                }
            }
            nipt += np; nipl++;
        } else if (np >= 3 && (kind == 4 || kind == 7 || kind == 12 || kind == 14 || kind == 22)) {   // cover areas
            int c = kind == 4 ? IC_WATER : kind == 7 ? IC_WOOD : kind == 12 ? IC_FARM : kind == 14 ? IC_SAND
                  : strstr(sub, "wetland") ? IC_WET : strstr(sub, "scrub") ? IC_SCRUB
                  : strstr(sub, "heath") ? IC_HEATH : strstr(sub, "rock") ? IC_ROCK : IC_LAND;
            float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
            for (int k = 0; k < np; k++) { x0 = fminf(x0, pts[k*2]); x1 = fmaxf(x1, pts[k*2]); y0 = fminf(y0, pts[k*2+1]); y1 = fmaxf(y1, pts[k*2+1]); }
            for (int j = 0; j < ICG; j++) {                // islandstudio writes axis-aligned runs: fill by cell centre
                float cy = (j + 0.5f) * isl_H / ICG; if (cy < y0 || cy > y1) continue;
                for (int i = 0; i < ICG; i++) { float cx = (i + 0.5f) * isl_W / ICG; if (cx >= x0 && cx <= x1) icv[j * ICG + i] = (unsigned char)c; }
            }
        } else if (kind == 9 && np >= 3 && niset < ISET_MAX) {                  // residential: a settlement
            float cx = 0, cy = 0, r = 0;
            for (int k = 0; k < np; k++) { cx += pts[k*2]; cy += pts[k*2+1]; } cx /= np; cy /= np;
            for (int k = 0; k < np; k++) r = fmaxf(r, fsqrt((pts[k*2]-cx)*(pts[k*2]-cx) + (pts[k*2+1]-cy)*(pts[k*2+1]-cy)));
            iset[niset].x = cx; iset[niset].y = cy; iset[niset].r = r;
            iset[niset].rank = r > 450 ? 2 : r > 180 ? 1 : 0; niset++;
        } else if (kind == 6 && np >= 1 && nifarm < IFARM_MAX) {                // building: a farmstead
            float cx = 0, cy = 0; for (int k = 0; k < np; k++) { cx += pts[k*2]; cy += pts[k*2+1]; }
            ifx[nifarm] = cx / np; ify[nifarm] = cy / np; nifarm++;
        }
        free(pts);
    }
    if (ver >= 3 && p + 4 <= end) {
        int g; memcpy(&g, p, 4); p += 4;
        if (g > 1 && g <= IHF_MAX && p + (long)g * g * 4 <= end) { ihg = g; memcpy(ihf, p, (size_t)g * g * 4); }
    }
    #undef RD
    free(buf);
    return ihg > 0 && nipl > 0;
bad:
    free(buf);
    return 0;
}

static float raw_at(float x, float y) {
    if (ISL) return isl_cover(x, y) == IC_WATER ? -1.0f : isl_h(x, y) / 1000.0f + 0.02f;   // >0 land, <0 water
    return height_at(x, y);
}
// worldnet's terrain is MACRO truth: its smallest features are ~3 km across, flat from
// behind a wheel. The cart adds driving-scale DETAIL (pure fn of x,y — as deterministic
// as the spine): ~700 m swells + ~180 m bumps, faded out at the coast.
static float detail_at(float x, float y, float t) {
    float k = clamp(t / 0.06f, 0, 1);
    float a = noise2(x / 700.0f + 31.7f, y / 700.0f + 5.3f) - 0.5f;
    float b = noise2(x / 180.0f + 2.1f,  y / 180.0f + 77.0f) - 0.5f;
    return k * (detail * a + detail * 0.18f * b);
}
// Rivers: worldnet stamps them as a flat -0.5 (a step down to sea level, a 200 m slot canyon
// from a cockpit). Bed them RIVER_CUT metres below the land they cut through instead.
#define RIVER_CUT 3.0f
static float hgt_xy(float x, float y, float t) {
    if (ISL) {                                     // the island has real relief: only a few metres of
        float h = isl_h(x, y);                     // driving-scale bumps on top ([ ] scales them)
        if (t > 0) h += detail * 0.08f * ((noise2(x / 180.0f + 2.1f, y / 180.0f + 77.0f) - 0.5f)
                                        + 0.4f * (noise2(x / 45.0f + 5.3f, y / 45.0f + 9.1f) - 0.5f));
        return h;
    }
    if (t == -0.5f) {
        float b = wn_height_base(x, y);
        if (b > 0) return fmaxf(0, b * vscale + detail_at(x, y, b) - RIVER_CUT);
    }
    return t > 0 ? t * vscale + detail_at(x, y, t) : 0;
}
static float ground(float x, float y) { return hgt_xy(x, y, raw_at(x, y)); }

// ── shading ramps: dark → light anchors; checker blends sit between them ────────
typedef struct { int n; int c[5]; } Ramp;
enum { B_WATER, B_SAND, B_SCRUB, B_GRASS, B_FOREST, B_HILL, B_ROCK, B_PEAK,
       B_PLOUGH, B_PASTURE, B_HAY, B_ASPHALT, B_GRAVEL, NBAND };
static const Ramp RAMP[NBAND] = {
    { 3, { CLR_DARKER_BLUE, CLR_DARK_BLUE, CLR_TRUE_BLUE } },
    { 3, { CLR_DARK_BROWN, CLR_BROWN, CLR_MEDIUM_GREY } },
    { 4, { CLR_BROWNISH_BLACK, CLR_DARK_BROWN, CLR_BROWN, CLR_MEDIUM_GREY } },
    { 4, { CLR_BROWNISH_BLACK, CLR_BLUE_GREEN, CLR_DARK_GREEN, CLR_MEDIUM_GREY } },
    { 3, { CLR_BROWNISH_BLACK, CLR_BLUE_GREEN, CLR_DARK_GREEN } },
    { 4, { CLR_BROWNISH_BLACK, CLR_DARK_BROWN, CLR_BROWN, CLR_MEDIUM_GREY } },
    { 4, { CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_DARK_GREY, CLR_MEDIUM_GREY } },
    { 4, { CLR_DARKER_GREY, CLR_DARK_GREY, CLR_MEDIUM_GREY, CLR_LIGHT_GREY } },
    // lowland patchwork: flat ground (lum ≈ 0.6) lands on a DIFFERENT anchor per kind
    { 4, { CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_DARK_BROWN, CLR_BROWN } },          // plough
    { 4, { CLR_BROWNISH_BLACK, CLR_DARKER_BLUE, CLR_BLUE_GREEN, CLR_DARK_GREEN } },     // pasture
    { 4, { CLR_DARK_BROWN, CLR_BROWN, CLR_MEDIUM_GREY, CLR_LIGHT_PEACH } },             // hay
    // road surfaces
    { 4, { CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_DARK_GREY, CLR_MEDIUM_GREY } },     // asphalt
    { 4, { CLR_BROWNISH_BLACK, CLR_DARK_BROWN, CLR_MAUVE, CLR_MEDIUM_GREY } },          // gravel
};
// countryside, not a map legend: elevation picks the broad band, then woods (a noise blob)
// and a 120 m patchwork of parcels — pure functions of position, same every visit.
static int band_at(float t, float x, float y) {
    if (ISL) {                                     // the island's own cover; farmland gets the parcels
        switch (isl_cover(x, y)) {
            case IC_WATER: return B_WATER;
            case IC_SAND:  return B_SAND;
            case IC_WOOD:  return B_FOREST;
            case IC_HEATH: return B_HILL;
            case IC_SCRUB: return B_SCRUB;
            case IC_ROCK:  return isl_h(x, y) > 900 ? B_PEAK : B_ROCK;
            case IC_WET:   return B_PASTURE;
            default: break;
        }
    } else {
        if (t <  0.00f) return B_WATER;
        if (t <  0.012f) return B_SAND;
        if (t >= 0.45f) return B_PEAK;
        if (t >= 0.40f) return B_ROCK;
        float w = noise2(x / 900.0f + 13.0f, y / 900.0f + 41.0f);
        if (w > 0.66f - (t > 0.3f ? 0.12f : 0)) return B_FOREST;
        if (t >= 0.32f) return B_HILL;
    }
    unsigned h = wn_hash2(wn_ifloor(x / 120.0f) * 7 + 3, wn_ifloor(y / 120.0f) * 13 + 1);
    int p = (int)(h % 10u);
    return p < 3 ? B_GRASS : p < 6 ? B_PASTURE : p < 8 ? B_PLOUGH : B_HAY;
}

// ── how each surface drives: grip (sideways hold), roll (drag ∝ speed), rough (dash_shake)
typedef struct { float grip, roll, rough; const char *name; } Surf;
static const Surf SURF[NBAND] = {
    [B_WATER]   = { 0.2f, 2.00f, 1.0f, "water"   },
    [B_SAND]    = { 0.35f, 0.35f, 0.5f, "sand"   },
    [B_SCRUB]   = { 0.50f, 0.20f, 0.8f, "scrub"  },
    [B_GRASS]   = { 0.55f, 0.15f, 0.5f, "grass"  },
    [B_FOREST]  = { 0.45f, 0.60f, 1.0f, "woods"  },
    [B_HILL]    = { 0.50f, 0.22f, 0.8f, "heath"  },
    [B_ROCK]    = { 0.60f, 0.30f, 1.0f, "rock"   },
    [B_PEAK]    = { 0.40f, 0.40f, 1.0f, "scree"  },
    [B_PLOUGH]  = { 0.40f, 0.40f, 1.0f, "plough" },
    [B_PASTURE] = { 0.55f, 0.15f, 0.5f, "pasture"},
    [B_HAY]     = { 0.50f, 0.12f, 0.6f, "stubble"},
    [B_ASPHALT] = { 1.00f, 0.015f, 0.05f, "asphalt"},
    [B_GRAVEL]  = { 0.70f, 0.04f, 0.35f, "gravel" },
};

static const float SUNX = -0.68f, SUNY = 0.48f, SUNZ = 0.55f;   // low SW sun, so swells read
static const int FOGPAT[3] = { 0xA0A0, 0xA5A5, 0x5F5F };        // 25 / 50 / 75 % haze

static int g_outline;          // 1 = also stroke the polygon's edges (seals road T-junction cracks)
static void fill_shaded(int *xy, int n, int band, float lum, float fog) {
    const Ramp *r = &RAMP[band];
    int steps = r->n * 2 - 1;
    int k = (int)(clamp(lum, 0, 0.999f) * steps);
    if (g_outline) k = ((k + 1) / 2) * 2 < steps ? ((k + 1) / 2) * 2 : steps - 1;  // roads: solid shades only
    int a = r->c[k / 2], b = (k & 1) ? r->c[k / 2 + 1] : a;
    int fl = (int)(clamp(fog, 0, 1) * 4.0f);
    if (fl >= 4) { a = b = HAZE; fl = 0; }
    if (fl > 0)      fillp(FOGPAT[fl - 1], HAZE);
    else if (a != b) fillp(FILL_CHECKER, b);
    for (int i = 1; i + 1 < n; i++)
        trifill(xy[0], xy[1], xy[2*i], xy[2*i+1], xy[2*i+2], xy[2*i+3], a);
    if (g_outline && fl == 0)          // line() ignores fillp, so only seal solid (unfogged) fills
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            line(xy[2*i], xy[2*i+1], xy[2*j], xy[2*j+1], a);
            // ...and once more one pixel UP: the next (nearer) cell's terrain triangle can cover the
            // row just above its edge, which its own road fragment, rounded differently, then missed:
            // a 1-px strip of ground through the road (the maker's "horizontal glitchy lines")
            line(xy[2*i], xy[2*i+1] - 1, xy[2*j], xy[2*j+1] - 1, a);
        }
    fillp_reset();
    ntri++;
}

// ── camera ─────────────────────────────────────────────────────────────────────
static void to_cam(float wx, float wy, float h, float *ox, float *oy, float *oz) {
    float dx = wx - px, dy = wy - py, dz = h - camh;
    float x = dx * crx + dy * cry;
    float z = dx * cfx + dy * cfy;
    float y = dz * cpc - z * cps;                 // pitch
    *oz = dz * cps + z * cpc;
    *ox = x * crc - y * crs;                      // roll
    *oy = x * crs + y * crc;
}
// project a camera-space polygon (near-clipped) and fill it
#define PMAX 12
static void fill_cam(float *X, float *Y, float *Z, int n, int band, float lum) {
    float PX[PMAX], PY[PMAX], PZ[PMAX]; int m = 0;
    int anyIn = 0;
    for (int i = 0; i < n; i++) if (Z[i] >= NEAR) anyIn = 1;
    if (!anyIn) return;
    float zs = 0;
    for (int i = 0; i < n && m < PMAX - 1; i++) {          // Sutherland–Hodgman, z = NEAR
        int j = (i + 1) % n;
        int inI = Z[i] >= NEAR, inJ = Z[j] >= NEAR;
        if (inI) { PX[m] = X[i]; PY[m] = Y[i]; PZ[m] = Z[i]; m++; }
        if (inI != inJ) {
            float u = (NEAR - Z[i]) / (Z[j] - Z[i]);
            PX[m] = X[i] + (X[j] - X[i]) * u; PY[m] = Y[i] + (Y[j] - Y[i]) * u; PZ[m] = NEAR; m++;
        }
    }
    if (m < 3) return;
    // side reject AFTER the near clip (a point behind the camera has no meaningful side)
    float k = 160.0f / FOCAL;
    int allL = 1, allR = 1;
    for (int i = 0; i < m; i++) {
        if (!(PX[i] >  PZ[i] * k + 0.5f)) allR = 0;
        if (!(PX[i] < -PZ[i] * k - 0.5f)) allL = 0;
    }
    if (allL || allR) return;
    int xy[PMAX * 2];
    for (int i = 0; i < m; i++) {
        xy[2*i]   = (int)floorf(clamp(160.0f + PX[i] * FOCAL / PZ[i], -4000, 4000) + 0.5f);
        xy[2*i+1] = (int)floorf(clamp(HZ0    - PY[i] * FOCAL / PZ[i], -4000, 4000) + 0.5f);
        zs += PZ[i];
    }
    fill_shaded(xy, m, band, lum, (zs / m - FOG0) / (FOG1 - FOG0));
}

// lighting of one ring triangle (vertex ids) → lum, plus its up-ness for the cliff rule
static float tri_lum(int r, int a, int b, int c, float *nzo) {
    int n1 = RN[r] + 1; float s = RS[r];
    float ax = (a % n1) * s, ay = (a / n1) * s, bx = (b % n1) * s, by = (b / n1) * s;
    float cx = (c % n1) * s, cy = (c / n1) * s;
    float e1x = bx - ax, e1y = by - ay, e1z = vh[r][b] - vh[r][a];
    float e2x = cx - ax, e2y = cy - ay, e2z = vh[r][c] - vh[r][a];
    float nx = e1y * e2z - e1z * e2y, ny = e1z * e2x - e1x * e2z, nz = e1x * e2y - e1y * e2x;
    if (nz < 0) { nx = -nx; ny = -ny; nz = -nz; }
    float nl = fsqrt(nx*nx + ny*ny + nz*nz); if (nl <= 0) nl = 1;
    nx /= nl; ny /= nl; nz /= nl;
    if (nzo) *nzo = nz;
    float d = nx * SUNX + ny * SUNY + nz * SUNZ;
    return 0.08f + 0.95f * (d > 0 ? d : 0);
}

static void draw_tri(int r, int a, int b, int c) {
    float nz, lum = tri_lum(r, a, b, c, &nz);
    int n1 = RN[r] + 1; float s = RS[r];
    float t = (vt[r][a] + vt[r][b] + vt[r][c]) / 3.0f;
    float wx = rx0[r] + ((a % n1) + (b % n1) + (c % n1)) * s / 3.0f;
    float wy = ry0[r] + ((a / n1) + (b / n1) + (c / n1)) * s / 3.0f;
    int band = band_at(t, wx, wy);
    if (band != B_WATER && nz < 0.72f) band = B_ROCK;      // cliffs and gorge walls
    if (band == B_WATER) lum = 0.55f;
    float X[3] = { vcx[r][a], vcx[r][b], vcx[r][c] };
    float Y[3] = { vcy[r][a], vcy[r][b], vcy[r][c] };
    float Z[3] = { vcz[r][a], vcz[r][b], vcz[r][c] };
    fill_cam(X, Y, Z, 3, band, lum);
}

// ── rings ──────────────────────────────────────────────────────────────────────
static float snapf(float v, float s) { return floorf(v / s) * s; }

static void build_ring(int r) {
    int n = RN[r], n1 = n + 1;
    float s = RS[r];
    float align = r + 1 < NRING ? RS[r + 1] : s;
    rx0[r] = snapf(px, align) - (float)(n / 2) * s;
    ry0[r] = snapf(py, align) - (float)(n / 2) * s;
    int step = r + 1 < NRING ? (int)(RS[r + 1] / s) : 1;
    for (int j = 0; j <= n; j++)
        for (int i = 0; i <= n; i++) {
            int id = j * n1 + i;
            float wx = rx0[r] + i * s, wy = ry0[r] + j * s, t;
            int onx = (j == 0 || j == n), ony = (i == 0 || i == n);
            if (step > 1 && onx && i % step) {              // border: lerp the coarse edge
                int i0 = i - i % step;
                float xa = rx0[r] + i0 * s, xb = xa + step * s, u = (float)(i % step) / step;
                float t0 = raw_at(xa, wy), t1 = raw_at(xb, wy);
                float h0 = hgt_xy(xa, wy, t0), h1 = hgt_xy(xb, wy, t1);
                t = t0 + (t1 - t0) * u; vh[r][id] = h0 + (h1 - h0) * u;
            } else if (step > 1 && ony && j % step) {
                int j0 = j - j % step;
                float ya = ry0[r] + j0 * s, yb = ya + step * s, u = (float)(j % step) / step;
                float t0 = raw_at(wx, ya), t1 = raw_at(wx, yb);
                float h0 = hgt_xy(wx, ya, t0), h1 = hgt_xy(wx, yb, t1);
                t = t0 + (t1 - t0) * u; vh[r][id] = h0 + (h1 - h0) * u;
            } else {
                t = raw_at(wx, wy); vh[r][id] = hgt_xy(wx, wy, t);
            }
            vt[r][id] = t;
            to_cam(wx, wy, vh[r][id], &vcx[r][id], &vcy[r][id], &vcz[r][id]);
        }
}
static int in_finer(int r, float wx, float wy, float s) {   // cell fully inside ring r-1?
    if (r == 0) return 0;
    float fx0 = rx0[r - 1], fy0 = ry0[r - 1], fs = RN[r - 1] * RS[r - 1];
    return wx >= fx0 && wx + s <= fx0 + fs && wy >= fy0 && wy + s <= fy0 + fs;
}

// ── road pieces: polygons in world xy, binned into the ring cells they touch ────
#define ROAD_RINGS 2            // roads drawn in rings 0..1 (≈ 1 km); beyond is sub-pixel
#define PC_MAX   20000
#define BIN_MAX  60000
static int pc_drop;              // pieces/bins dropped for lack of room this frame: must stay 0
#define PST_MAX  600
enum { PK_ROAD, PK_DASH, PK_RAIL, PK_FASCIA };   // RAIL / FASCIA: vertical strips along a deck's sides
typedef struct { float x[4], y[4]; unsigned char kind, band, deck; float z0, z1, ax, ay, bx, by; } Piece;   // deck: heights z0..z1 along axis a→b
static Piece pc[PC_MAX]; static int npc;
static int   bin_head[ROAD_RINGS][32 * 32], bin_next[BIN_MAX], bin_pc[BIN_MAX], nbin;
typedef struct { float x, y; } Post;
static Post  pst[PST_MAX]; static int npst;
static int   pst_head[32 * 32], pst_next[PST_MAX];         // posts: ring 0 cells only

static void bin_piece(int id) {
    Piece *p = &pc[id];
    float mnx = p->x[0], mxx = mnx, mny = p->y[0], mxy = mny;
    for (int i = 1; i < 4; i++) {
        mnx = fminf(mnx, p->x[i]); mxx = fmaxf(mxx, p->x[i]);
        mny = fminf(mny, p->y[i]); mxy = fmaxf(mxy, p->y[i]);
    }
    for (int r = 0; r < ROAD_RINGS; r++) {
        float s = RS[r]; int n = RN[r];
        int i0 = (int)floorf((mnx - rx0[r]) / s), i1 = (int)floorf((mxx - rx0[r]) / s);
        int j0 = (int)floorf((mny - ry0[r]) / s), j1 = (int)floorf((mxy - ry0[r]) / s);
        if (i1 < 0 || j1 < 0 || i0 >= n || j0 >= n) continue;
        if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0; if (i1 >= n) i1 = n - 1; if (j1 >= n) j1 = n - 1;
        for (int j = j0; j <= j1; j++)
            for (int i = i0; i <= i1; i++) {
                if (in_finer(r, rx0[r] + i * s, ry0[r] + j * s, s)) continue;
                if (nbin >= BIN_MAX) { pc_drop++; return; }
                int c = j * n + i;
                bin_pc[nbin] = id; bin_next[nbin] = bin_head[r][c]; bin_head[r][c] = nbin++;
            }
    }
}
static void add_piece(float ax, float ay, float bx, float by, float cx, float cy,
                      float dx, float dy, int kind, int band) {
    if (npc >= PC_MAX) { pc_drop++; return; }
    Piece *p = &pc[npc];
    p->x[0] = ax; p->y[0] = ay; p->x[1] = bx; p->y[1] = by;
    p->x[2] = cx; p->y[2] = cy; p->x[3] = dx; p->y[3] = dy;
    p->kind = (unsigned char)kind; p->band = (unsigned char)band; p->deck = 0;
    bin_piece(npc++);
}

// Catmull-Rom point on edge e at sample-segment i, param t (clamped neighbours)
static void edge_pt(const REdge *e, int i, float t, float *ox, float *oy) {
    int i0 = i > 0 ? i - 1 : 0, i3 = i + 2 <= LINK_SAMPLES ? i + 2 : LINK_SAMPLES;
    catmull(e->x[i0], e->x[i], e->x[i + 1], e->x[i3], t, ox);
    catmull(e->y[i0], e->y[i], e->y[i + 1], e->y[i3], t, oy);
}

// the cart's OWN carriageway half-widths (m): worldnet's (12/7/5/3.5/2.5) read as runways
// from a 1.2 m eye height. These are 1970s country roads: two narrow lanes, a single track.
static const float HW[5] = { 7.0f, 4.5f, 3.6f, 3.0f, 2.0f };   // motorway → dirt
#define ROAD_NEAR  1150.0f      // build road pieces within this range (ring 1 reach)
// ── JUNCTIONS: where three or more road ends meet (the export splits roads at every junction, so
// arms end exactly on the node). Each node keeps its arms: bearing out of the node, half-width,
// class. Built once on load; drawn as a curb-return patch (roadkit's geometry) in isl_build_roads.
#define IJN_MAX 6000
#define IJ_ARM  8
typedef struct { float x, y; int n; float brg[IJ_ARM], hw[IJ_ARM]; unsigned char cls[IJ_ARM]; int poly[IJ_ARM], atend[IJ_ARM]; } IJunc;
static IJunc ijn[IJN_MAX]; static int nijn;
static float ipl_j0[IPL_MAX], ipl_j1[IPL_MAX];   // per road poly: junction clearance at its start / end (m)
static void isl_find_junctions(void) {
    nijn = 0;
    for (int q = 0; q < nipl; q++) ipl_j0[q] = ipl_j1[q] = 0;
    for (int q = 0; q < nipl; q++) {
        if (ipl[q].river) continue;
        for (int e = 0; e < 2; e++) {
            int i = e == 0 ? ipl[q].start : ipl[q].start + ipl[q].n - 1;
            float x = ipx[i], y = ipy[i];
            int k = 0;                                     // existing node within 1.5 m?
            for (; k < nijn; k++) if (fabsf(ijn[k].x - x) < 1.5f && fabsf(ijn[k].y - y) < 1.5f) break;
            if (k == nijn) { if (nijn >= IJN_MAX) continue; ijn[nijn].x = x; ijn[nijn].y = y; ijn[nijn].n = 0; nijn++; }
            IJunc *J = &ijn[k];
            if (J->n >= IJ_ARM) continue;
            // bearing: toward the poly point ~12 m from the node (smooths the first kink)
            int dir = e == 0 ? 1 : -1, j = i;
            while (j + dir >= ipl[q].start && j + dir < ipl[q].start + ipl[q].n && fabsf(ipS[j] - ipS[i]) < 12.0f) j += dir;
            J->brg[J->n] = atan2f(ipy[j] - y, ipx[j] - x) / D2R;
            J->hw[J->n] = HW[ipl[q].cls]; J->cls[J->n] = ipl[q].cls; J->poly[J->n] = q; J->atend[J->n] = e;
            J->n++;
        }
    }
    // keep junctions (3+ arms) AND 2-arm joints where the road bends (a class change splits a road
    // into two lines mid-bend; without a patch the outside of the bend showed a wedge of field)
    int w = 0;
    for (int k = 0; k < nijn; k++) {
        IJunc *J = &ijn[k];
        if (J->n >= 3) { ijn[w++] = *J; continue; }
        if (J->n == 2) {
            float g = fmodf(J->brg[1] - J->brg[0] + 720.0f, 360.0f); if (g > 180) g = 360 - g;
            if (g < 172.0f) ijn[w++] = *J;                 // a real bend (not straight on)
        }
    }
    nijn = w;
    for (int k = 0; k < nijn; k++) {                       // markings + posts keep clear of the junction
        if (ijn[k].n < 3) continue;                        // (a plain 2-arm joint keeps its markings)
        float mh = 0; for (int a = 0; a < ijn[k].n; a++) mh = fmaxf(mh, ijn[k].hw[a]);
        float clear = mh + 3.0f + 2.0f * mh + 2.0f;         // ≈ widest arm + its curb radius + a margin
        for (int a = 0; a < ijn[k].n; a++) {
            int q = ijn[k].poly[a];
            if (ijn[k].atend[a]) ipl_j1[q] = fmaxf(ipl_j1[q], clear); else ipl_j0[q] = fmaxf(ipl_j0[q], clear);
        }
    }
}

#define DASH_NEAR  160.0f
#define POST_NEAR  420.0f
#define DASH_LEN   4.0f
#define DASH_GAP   12.0f
#define POST_GAP   50.0f

static void add_post(float ox, float oy) {
    int ci = (int)floorf((ox - rx0[0]) / RS[0]), cj = (int)floorf((oy - ry0[0]) / RS[0]);
    if (ci < 0 || cj < 0 || ci >= RN[0] || cj >= RN[0] || npst >= PST_MAX) return;
    int c = cj * RN[0] + ci;
    pst[npst].x = ox; pst[npst].y = oy;
    pst_next[npst] = pst_head[c]; pst_head[c] = npst++;
}
// ── BRIDGE DECKS: every bridge span (the export's B1: rivers, and the expressway's flyovers) gets a
// deck profile instead of following the ground: level between the ground at its two ends, and where
// it passes OVER another road it rises to 6.5 m clearance on 6 % ramps. Pieces of a bridge carry
// these heights; the car rides the deck when it's nearer the deck than the ground beneath.
#define IBR_MAX 2000
typedef struct { int q; float g0, g1, L; int nc; float sc[4], gc[4]; } IBr;
static IBr ibr[IBR_MAX]; static int nibr;
static short ipl_br[IPL_MAX];                     // per poly: its deck index, -1 = not a bridge
static int seg_cross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy, float *t) {
    float rx = bx - ax, ry = by - ay, sx = dx - cx, sy = dy - cy, den = rx * sy - ry * sx;
    if (fabsf(den) < 1e-6f) return 0;
    float u = ((cx - ax) * ry - (cy - ay) * rx) / den; *t = ((cx - ax) * sy - (cy - ay) * sx) / den;
    // strictly INSIDE both segments: a road that merely touches the deck at an end (the road the
    // bridge connects to) is not passing underneath (counting it lifted whole decks 6.5 m off the ground)
    return *t > 0.02f && *t < 0.98f && u > 0.02f && u < 0.98f;
}
static void isl_build_decks(void) {
    nibr = 0;
    for (int q = 0; q < nipl; q++) ipl_br[q] = -1;
    for (int q = 0; q < nipl && nibr < IBR_MAX; q++) {
        if (!ipl[q].bridge || ipl[q].river) continue;
        IBr *B = &ibr[nibr]; int a = ipl[q].start, e = a + ipl[q].n - 1;
        B->q = q; B->L = ipS[e] - ipS[a]; B->nc = 0;
        B->g0 = ground(ipx[a], ipy[a]); B->g1 = ground(ipx[e], ipy[e]);
        for (int k = a + 1; k <= e; k++) {                // roads passing underneath
            int bx = ib_of((ipx[k] + ipx[k - 1]) * 0.5f, isl_W), by = ib_of((ipy[k] + ipy[k - 1]) * 0.5f, isl_H);
            for (int j = by - 1; j <= by + 1; j++) for (int i = bx - 1; i <= bx + 1; i++) {
                if (i < 0 || j < 0 || i >= IBG || j >= IBG) continue;
                for (int g = ib_head[j * IBG + i]; g >= 0; g = ib_next[g]) {
                    const IPoly *o = &ipl[ip_poly[g]];
                    if (o->river || o->bridge || ip_poly[g] == q) continue;
                    float t;
                    if (!seg_cross(ipx[k - 1], ipy[k - 1], ipx[k], ipy[k], ipx[g - 1], ipy[g - 1], ipx[g], ipy[g], &t)) continue;
                    float sc = ipS[k - 1] - ipS[a] + t * (ipS[k] - ipS[k - 1]);
                    int dup = 0; for (int c = 0; c < B->nc; c++) if (fabsf(B->sc[c] - sc) < 20) dup = 1;
                    if (dup || B->nc >= 4) continue;
                    B->sc[B->nc] = sc;
                    B->gc[B->nc] = ground(ipx[k - 1] + (ipx[k] - ipx[k - 1]) * t, ipy[k - 1] + (ipy[k] - ipy[k - 1]) * t);
                    B->nc++;
                }
            }
        }
        ipl_br[q] = (short)nibr++;
    }
}
static float deck_h(int q, float s) {             // s = arc length along the WHOLE poly list (ipS)
    const IBr *B = &ibr[ipl_br[q]];
    float sr = clamp(s - ipS[ipl[q].start], 0, B->L);
    float base = B->g0 + (B->g1 - B->g0) * (B->L > 0 ? sr / B->L : 0), z = base;
    for (int c = 0; c < B->nc; c++) z = fmaxf(z, B->gc[c] + 6.5f - 0.06f * fabsf(sr - B->sc[c]));
    // the deck MUST meet the ground at both ends: taper the lift to 0 over the last 40 m (a crossing on
    // higher ground than the span's ends left a 3.7 m step the wheels refused, so cars drove through)
    float taper = clamp(fminf(sr, B->L - sr) / 40.0f, 0, 1);
    return base + (z - base) * taper * taper * (3 - 2 * taper);
}
// JUNCTION PATCH: rounded curb corners between neighbouring arms (roadkit's curb_return, the same
// fillet geometry streetlab and citydrive draw), plus a small hub disc, as extra road pieces. The
// corner K is where the two FACING road edges cross; roadkit's edge_corner assumes one width, so it
// is computed here for two (a lane meeting a main road).
static float junc_R(float hwA, float hwB) { return 3.0f + 2.0f * fminf(hwA, hwB); }   // curb radius (m)
static void isl_junction_pieces(const IJunc *J) {
    int n = J->n, ord[IJ_ARM];
    for (int i = 0; i < n; i++) ord[i] = i;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (J->brg[ord[j]] < J->brg[ord[i]]) { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    int dirt = 1; float maxhw = 0;
    for (int i = 0; i < n; i++) { if (J->cls[i] != CL_DIRT) dirt = 0; maxhw = fmaxf(maxhw, J->hw[i]); }
    int band = dirt ? B_GRAVEL : B_ASPHALT;
    // hub disc: closes the wedge where arms end at an angle
    for (int k = 0; k < 10; k++) {
        float a0 = k * 6.2831853f / 10, a1 = (k + 1) * 6.2831853f / 10;
        add_piece(J->x, J->y, J->x + cosf(a0) * maxhw, J->y + sinf(a0) * maxhw,
                  J->x + cosf(a1) * maxhw, J->y + sinf(a1) * maxhw, J->x + cosf(a1) * maxhw, J->y + sinf(a1) * maxhw, PK_ROAD, band);
    }
    for (int q = 0; q < n; q++) {
        int ia = ord[q], ib = ord[(q + 1) % n];
        float bA = J->brg[ia], bB = J->brg[ib], gap = fmodf(bB - bA + 720.0f, 360.0f);
        if (gap <= 1.0f || gap >= 175.0f) continue;          // straight on: no corner to round
        float ax = cosf(bA * D2R), ay = sinf(bA * D2R), bx = cosf(bB * D2R), by = sinf(bB * D2R);
        float hA = J->hw[ia], hB = J->hw[ib];
        // A's edge facing B: offset +90° from A; B's edge facing A: offset -90° from B
        float pax = J->x - ay * hA, pay = J->y + ax * hA, pbx = J->x + by * hB, pby = J->y - bx * hB;
        float det = ax * (-by) - ay * (-bx);                 // solve pa + a t = pb + b s
        if (fabsf(det) < 1e-4f) continue;
        float dx = pbx - pax, dy = pby - pay;
        float t = (dx * (-by) - dy * (-bx)) / det, s = (ax * dy - ay * dx) / det;
        if (t < 0 || s < 0) continue;
        float kx = pax + ax * t, ky = pay + ay * t;
        float half = gap * 0.5f * D2R;
        float R = fminf(junc_R(hA, hB), 22.0f * tanf(half)); // acute corners: keep the fillet short
        if (R < 0.5f) continue;
        CurbReturn c = curb_return(kx, ky, bA, bB, R);
        float a0 = atan2f(c.t1y - c.oy, c.t1x - c.ox), a1 = atan2f(c.t2y - c.oy, c.t2x - c.ox);
        float d = a1 - a0; while (d > 3.14159265f) d -= 6.2831853f; while (d < -3.14159265f) d += 6.2831853f;
        float px0 = c.t1x, py0 = c.t1y;
        for (int k = 1; k <= 8; k++) {                       // fan from K over the arc
            float a = a0 + d * k / 8, x1 = c.ox + cosf(a) * R, y1 = c.oy + sinf(a) * R;
            add_piece(kx, ky, px0, py0, x1, y1, x1, y1, PK_ROAD, band);
            px0 = x1; py0 = y1;
        }
    }
}
// island roads: the file's polylines are already smooth (~20 m points), so segments are straight;
// each end's normal comes from its neighbours, so consecutive segments join without gaps
static void isl_build_roads(void) {
    int bx0 = ib_of(px - ROAD_NEAR, isl_W), bx1 = ib_of(px + ROAD_NEAR, isl_W);
    int by0 = ib_of(py - ROAD_NEAR, isl_H), by1 = ib_of(py + ROAD_NEAR, isl_H);
    for (int k = 0; k < nijn; k++) {                       // junction patches first (any order works: same band)
        float dx = ijn[k].x - px, dy = ijn[k].y - py;
        if (dx * dx + dy * dy < 900.0f * 900.0f) isl_junction_pieces(&ijn[k]);
    }
    for (int pass = 0; pass < 2; pass++)           // roads, then rivers: added last = drawn first (under)
        for (int bj = by0; bj <= by1; bj++) for (int bi = bx0; bi <= bx1; bi++)
            for (int b = ib_head[bj * IBG + bi]; b >= 0; b = ib_next[b]) {
                const IPoly *p = &ipl[ip_poly[b]];
                if (p->river != pass) continue;
                int a = b - 1, s0 = p->start, s1 = p->start + p->n - 1;
                float ax = ipx[a], ay = ipy[a], bx = ipx[b], by = ipy[b];
                float S = ipS[a], L = ipS[b] - ipS[a];
                float d = fsqrt(seg_d2(px, py, ax, ay, bx, by));
                if (L < 0.01f || d > (pass ? 700.0f : ROAD_NEAR)) continue;
                float hw = pass ? 3.0f : HW[p->cls];
                int band = pass ? B_WATER : p->cls == CL_DIRT ? B_GRAVEL : B_ASPHALT;
                int marked = !pass && p->cls != CL_DIRT;
                int a0 = a > s0 ? a - 1 : a, b1 = b < s1 ? b + 1 : b;
                float tax = ipx[b] - ipx[a0], tay = ipy[b] - ipy[a0], la = fsqrt(tax * tax + tay * tay) + 1e-6f;
                float tbx = ipx[b1] - ipx[a], tby = ipy[b1] - ipy[a], lb = fsqrt(tbx * tbx + tby * tby) + 1e-6f;
                float nax = -tay / la * hw, nay = tax / la * hw, nbx = -tby / lb * hw, nby = tbx / lb * hw;
                float step = d < 300 ? 6.0f : d < 700 ? 16.0f : 32.0f;
                int ns = (int)ceilf(L / step); if (ns < 1) ns = 1;
                int dq = (!pass && p->bridge && ipl_br[ip_poly[b]] >= 0) ? ip_poly[b] : -1;   // a deck?
                for (int k = 0; k < ns; k++) {
                    float t0 = (float)k / ns, t1 = (float)(k + 1) / ns;
                    float x0 = ax + (bx - ax) * t0, y0 = ay + (by - ay) * t0, x1 = ax + (bx - ax) * t1, y1 = ay + (by - ay) * t1;
                    float n0x = nax + (nbx - nax) * t0, n0y = nay + (nby - nay) * t0, n1x = nax + (nbx - nax) * t1, n1y = nay + (nby - nay) * t1;
                    int before = npc;
                    add_piece(x0 + n0x, y0 + n0y, x1 + n1x, y1 + n1y, x1 - n1x, y1 - n1y, x0 - n0x, y0 - n0y, PK_ROAD, band);
                    if (dq >= 0 && npc > before) {
                        Piece *pp = &pc[npc - 1];
                        float z0 = deck_h(dq, S + L * t0), z1 = deck_h(dq, S + L * t1);
                        pp->deck = 1; pp->z0 = z0; pp->z1 = z1; pp->ax = x0; pp->ay = y0; pp->bx = x1; pp->by = y1;
                        for (int side = -1; side <= 1; side += 2)  // parapet + slab edge on both sides
                            for (int kind = PK_RAIL; kind <= PK_FASCIA; kind++) {
                                float o = side * (1.0f + 0.25f / hw);
                                int b2 = npc;
                                add_piece(x0 + n0x * o, y0 + n0y * o, x1 + n1x * o, y1 + n1y * o, x1 + n1x * o, y1 + n1y * o, x0 + n0x * o, y0 + n0y * o, kind, band);
                                if (npc > b2) { Piece *sp = &pc[npc - 1]; sp->deck = 1; sp->z0 = z0; sp->z1 = z1; }
                            }
                    }
                }
                float ux = (bx - ax) / L, uy = (by - ay) / L;
                float Ltot = ipS[s1], c0 = pass ? 0 : ipl_j0[ip_poly[b]], c1 = pass ? 0 : ipl_j1[ip_poly[b]];
                if (marked && d < DASH_NEAR)
                    for (float s0d = ceilf(S / DASH_GAP) * DASH_GAP; s0d < S + L; s0d += DASH_GAP) {
                        if (s0d - ipS[s0] < c0 || Ltot - s0d < c1 + DASH_LEN) continue;   // clear of junctions
                        float t0 = s0d - S, t1 = fminf(s0d + DASH_LEN - S, L);
                        float x0 = ax + ux * t0, y0 = ay + uy * t0, x1 = ax + ux * t1, y1 = ay + uy * t1;
                        float nx = -uy * 0.12f, ny = ux * 0.12f;
                        int before = npc;
                        add_piece(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, PK_DASH, band);
                        if (dq >= 0 && npc > before) {
                            Piece *pp = &pc[npc - 1];
                            pp->deck = 1; pp->z0 = deck_h(dq, s0d); pp->z1 = deck_h(dq, s0d + (t1 - t0));
                            pp->ax = x0; pp->ay = y0; pp->bx = x1; pp->by = y1;
                        }
                    }
                if (marked && !p->bridge && d < POST_NEAR)
                    for (float s0p = ceilf(S / POST_GAP) * POST_GAP; s0p < S + L; s0p += POST_GAP) {
                        if (s0p - ipS[s0] < c0 + 4.0f || Ltot - s0p < c1 + 4.0f) continue;
                        float x0 = ax + ux * (s0p - S), y0 = ay + uy * (s0p - S);
                        for (int side = -1; side <= 1; side += 2)
                            add_post(x0 - uy * (hw + 1.5f) * side, y0 + ux * (hw + 1.5f) * side);
                    }
            }
}
static void build_roads(void) {
    npc = 0; nbin = 0; npst = 0; pc_drop = 0;
    for (int r = 0; r < ROAD_RINGS; r++)
        for (int c = 0; c < 32 * 32; c++) bin_head[r][c] = -1;
    for (int c = 0; c < 32 * 32; c++) pst_head[c] = -1;
    if (ISL) { isl_build_roads(); return; }
    eg_ensure(px, py);
    for (int e = 0; e < eg_n; e++) {
        const REdge *ed = &eg_e[e];
        if (ed->net != NET_ROAD) continue;
        float hw = HW[ed->cls];
        int band = ed->cls == CL_DIRT ? B_GRAVEL : B_ASPHALT;
        int marked = ed->cls != CL_DIRT;
        float S = 0;                                       // arc length at segment start
        for (int i = 0; i < LINK_SAMPLES; i++) {
            float ax = ed->x[i], ay = ed->y[i], bx = ed->x[i + 1], by = ed->y[i + 1];
            float L = fsqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
            float d = fsqrt(seg_d2(px, py, ax, ay, bx, by));
            if (L < 0.01f || d > ROAD_NEAR) { S += L; continue; }
            // sub-step: fine near, coarse far
            float step = d < 300 ? 6.0f : d < 700 ? 16.0f : 32.0f;
            int ns = (int)ceilf(L / step); if (ns < 1) ns = 1; if (ns > 200) ns = 200;
            float qx[201], qy[201];
            for (int k = 0; k <= ns; k++) edge_pt(ed, i, (float)k / ns, &qx[k], &qy[k]);
            float plx = 0, ply = 0, prx = 0, pry = 0;
            for (int k = 0; k <= ns; k++) {
                int ka = k > 0 ? k - 1 : 0, kb = k < ns ? k + 1 : ns;
                float tx = qx[kb] - qx[ka], ty = qy[kb] - qy[ka];
                float tl = fsqrt(tx * tx + ty * ty); if (tl <= 0) tl = 1;
                float nx = -ty / tl * hw, ny = tx / tl * hw;
                float lx = qx[k] + nx, ly = qy[k] + ny, rx = qx[k] - nx, ry = qy[k] - ny;
                if (k > 0) add_piece(plx, ply, lx, ly, rx, ry, prx, pry, PK_ROAD, band);
                plx = lx; ply = ly; prx = rx; pry = ry;
            }
            // dashes + posts, placed by ARC LENGTH so they stay put as you drive
            if (marked && d < DASH_NEAR) {
                for (float s0 = ceilf(S / DASH_GAP) * DASH_GAP; s0 < S + L; s0 += DASH_GAP) {
                    float t0 = (s0 - S) / L, t1 = fminf((s0 + DASH_LEN - S) / L, 1.0f);
                    float x0, y0, x1, y1; edge_pt(ed, i, t0, &x0, &y0); edge_pt(ed, i, t1, &x1, &y1);
                    float tx = x1 - x0, ty = y1 - y0, tl = fsqrt(tx * tx + ty * ty);
                    if (tl <= 0) continue;
                    float nx = -ty / tl * 0.12f, ny = tx / tl * 0.12f;
                    add_piece(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny,
                              PK_DASH, band);
                }
            }
            if (marked && d < POST_NEAR) {
                for (float s0 = ceilf(S / POST_GAP) * POST_GAP; s0 < S + L; s0 += POST_GAP) {
                    float t0 = (s0 - S) / L, x0, y0, x1, y1;
                    edge_pt(ed, i, t0, &x0, &y0); edge_pt(ed, i, fminf(t0 + 0.01f, 1), &x1, &y1);
                    float tx = x1 - x0, ty = y1 - y0, tl = fsqrt(tx * tx + ty * ty);
                    if (tl <= 0) continue;
                    for (int side = -1; side <= 1; side += 2) {
                        float ox = x0 - ty / tl * (hw + 1.5f) * side, oy = y0 + tx / tl * (hw + 1.5f) * side;
                        int ci = (int)floorf((ox - rx0[0]) / RS[0]), cj = (int)floorf((oy - ry0[0]) / RS[0]);
                        if (ci < 0 || cj < 0 || ci >= RN[0] || cj >= RN[0] || npst >= PST_MAX) continue;
                        int c = cj * RN[0] + ci;
                        pst[npst].x = ox; pst[npst].y = oy;
                        pst_next[npst] = pst_head[c]; pst_head[c] = npst++;
                    }
                }
            }
            S += L;
        }
    }
}

// the DRAWN ground height at a world point: ring r's triangulated surface. Every caller
// on a shared border picks the same cell (by floor) → identical heights → no cracks.
static float ring_h(int r, float x, float y) {
    int n = RN[r], n1 = n + 1; float s = RS[r];
    float fu = (x - rx0[r]) / s, fv = (y - ry0[r]) / s;
    int i = (int)floorf(fu), j = (int)floorf(fv);
    if (i < 0) i = 0; if (i > n - 1) i = n - 1;
    if (j < 0) j = 0; if (j > n - 1) j = n - 1;
    float u = fu - i, v = fv - j;
    int v00 = j * n1 + i;
    float h00 = vh[r][v00], h10 = vh[r][v00 + 1], h01 = vh[r][v00 + n1], h11 = vh[r][v00 + n1 + 1];
    return u >= v ? h00 + u * (h10 - h00) + v * (h11 - h10)
                  : h00 + v * (h01 - h00) + u * (h11 - h01);
}
// clip a polygon (WORLD x,y) against a half-plane a*x + b*y + c >= 0. Clipping in world
// coords makes a border point bit-identical from both cells that share it.
static int clip_half(float *u, float *v, int n, float a, float b, float c, float *ou, float *ov) {
    int m = 0;
    for (int i = 0; i < n && m < PMAX - 1; i++) {
        int j = (i + 1) % n;
        float di = a * u[i] + b * v[i] + c, dj = a * u[j] + b * v[j] + c;
        if (di >= 0) { ou[m] = u[i]; ov[m] = v[i]; m++; }
        if ((di >= 0) != (dj >= 0)) {
            float t = di / (di - dj);
            ou[m] = u[i] + (u[j] - u[i]) * t; ov[m] = v[i] + (v[j] - v[i]) * t; m++;
        }
    }
    return m;
}
// draw one piece, clipped to cell (i,j) of ring r and to each of its two triangles
static void draw_piece_in_cell(int r, int i, int j, const Piece *p, float lumA, float lumB) {
    float s = RS[r];
    float x0 = rx0[r] + i * s, y0 = ry0[r] + j * s, x1 = x0 + s, y1 = y0 + s;
    float u[PMAX], v[PMAX], u2[PMAX], v2[PMAX];
    for (int k = 0; k < 4; k++) { u[k] = p->x[k]; v[k] = p->y[k]; }
    int n = 4;
    n = clip_half(u, v, n,   1, 0, -x0, u2, v2); if (n < 3) return;
    n = clip_half(u2, v2, n, -1, 0,  x1, u, v);  if (n < 3) return;
    n = clip_half(u, v, n,   0, 1, -y0, u2, v2); if (n < 3) return;
    n = clip_half(u2, v2, n, 0, -1,  y1, u, v);  if (n < 3) return;
    float lift = p->kind == PK_DASH ? 0.02f : 0.0f;
    float dc = y0 - x0;                                     // diagonal: (x - x0) - (y - y0) = 0
    for (int side = 0; side < 2; side++) {                  // A: below the diagonal, B: above
        float cu[PMAX], cv[PMAX];
        int m = side == 0 ? clip_half(u, v, n, 1, -1, dc, cu, cv) : clip_half(u, v, n, -1, 1, -dc, cu, cv);
        if (m < 3) continue;
        float X[PMAX], Y[PMAX], Z[PMAX];
        for (int k = 0; k < m; k++)
        {
            float z;
            if (p->deck) {                                   // on a bridge deck: its own height along the axis
                float dx = p->bx - p->ax, dy = p->by - p->ay, l2 = dx * dx + dy * dy + 1e-6f;
                float t = clamp(((cu[k] - p->ax) * dx + (cv[k] - p->ay) * dy) / l2, 0, 1);
                z = p->z0 + (p->z1 - p->z0) * t;
            } else z = ring_h(r, cu[k], cv[k]);
            to_cam(cu[k], cv[k], z + lift, &X[k], &Y[k], &Z[k]);
        }
        g_outline = 1;
        if (p->kind == PK_DASH) fill_cam(X, Y, Z, m, B_PEAK, 0.999f);   // pale line
        else fill_cam(X, Y, Z, m, p->band, side == 0 ? lumA : lumB);
        g_outline = 0;
    }
}
static void draw_side(const Piece *p) {           // a vertical strip along a deck edge
    float lo0, hi0, lo1, hi1;
    if (p->kind == PK_RAIL) { lo0 = p->z0; hi0 = p->z0 + 0.7f; lo1 = p->z1; hi1 = p->z1 + 0.7f; }
    else                    { lo0 = p->z0 - 1.3f; hi0 = p->z0; lo1 = p->z1 - 1.3f; hi1 = p->z1; }
    float X[4], Y[4], Z[4];
    to_cam(p->x[0], p->y[0], lo0, &X[0], &Y[0], &Z[0]); to_cam(p->x[1], p->y[1], lo1, &X[1], &Y[1], &Z[1]);
    to_cam(p->x[1], p->y[1], hi1, &X[2], &Y[2], &Z[2]); to_cam(p->x[0], p->y[0], hi0, &X[3], &Y[3], &Z[3]);
    g_outline = 1;
    fill_cam(X, Y, Z, 4, B_ROCK, p->kind == PK_RAIL ? 0.9f : 0.15f);    // parapet: medium grey (asphalt is dark grey, white glared); slab edge dark
    g_outline = 0;
}
static void draw_post(const Post *q) {
    float h = ground(q->x, q->y), bx, by, bz, tx, ty, tz;
    to_cam(q->x, q->y, h, &bx, &by, &bz);
    to_cam(q->x, q->y, h + 1.0f, &tx, &ty, &tz);
    if (bz < 1.0f || tz < 1.0f) return;
    float sx0 = 160 + bx * FOCAL / bz, sy0 = HZ0 - by * FOCAL / bz;
    float sx1 = 160 + tx * FOCAL / tz, sy1 = HZ0 - ty * FOCAL / tz;
    int w = (int)(0.14f * FOCAL / bz); if (w < 1) w = 1;
    int cb = (int)(sy1 + (sy0 - sy1) * 0.2f);              // dark cap: the top fifth
    int fog = bz > 300;
    // posts are vertical in the world; under roll they lean on screen, so draw a thin quad
    float lx = (float)w * 0.5f;
    fillp(fog ? FILL_CHECKER : FILL_SOLID, HAZE);
    trifill((int)(sx0 - lx), (int)sy0, (int)(sx0 + lx), (int)sy0, (int)(sx1 + lx), cb, CLR_LIGHT_GREY);
    trifill((int)(sx0 - lx), (int)sy0, (int)(sx1 + lx), cb, (int)(sx1 - lx), cb, CLR_LIGHT_GREY);
    trifill((int)(sx1 - lx), cb, (int)(sx1 + lx), cb, (int)(sx1 + lx), (int)sy1, CLR_BROWNISH_BLACK);
    trifill((int)(sx1 - lx), cb, (int)(sx1 + lx), (int)sy1, (int)(sx1 - lx), (int)sy1, CLR_BROWNISH_BLACK);
    fillp_reset();
}

// ── draw one cell: its triangles, then its road pieces, then its posts ──────────
static void draw_cell(int r, int i, int j) {
    int n1 = RN[r] + 1;
    if (in_finer(r, rx0[r] + i * RS[r], ry0[r] + j * RS[r], RS[r])) return;
    int v00 = j * n1 + i, v10 = v00 + 1, v01 = v00 + n1, v11 = v01 + 1;
    float za = vcz[r][v00] + vcz[r][v10] + vcz[r][v11];
    float zb = vcz[r][v00] + vcz[r][v11] + vcz[r][v01];
    if (za > zb) { draw_tri(r, v00, v10, v11); draw_tri(r, v00, v11, v01); }
    else         { draw_tri(r, v00, v11, v01); draw_tri(r, v00, v10, v11); }
    if (r < ROAD_RINGS) {
        int c = j * RN[r] + i;
        if (bin_head[r][c] >= 0) {
            float lumA = tri_lum(r, v00, v10, v11, 0), lumB = tri_lum(r, v00, v11, v01, 0);
            // ground road, ground markings, the deck's slab edge (below the deck, so the deck covers it
            // from above), the deck, its markings, then the parapets on top
            for (int pass = 0; pass < 6; pass++)
                for (int b = bin_head[r][c]; b >= 0; b = bin_next[b]) {
                    const Piece *p = &pc[bin_pc[b]];
                    int key = p->kind == PK_ROAD ? (p->deck ? 3 : 0) : p->kind == PK_DASH ? (p->deck ? 4 : 1)
                            : p->kind == PK_FASCIA ? 2 : 5;
                    if (key != pass) continue;
                    if (key == 2 || key == 5) draw_side(p); else draw_piece_in_cell(r, i, j, p, lumA, lumB);
                }
        }
        if (r == 0)
            for (int q = pst_head[c]; q >= 0; q = pst_next[q]) draw_post(&pst[q]);
    }
}

static void draw_ring(int r) {
    int n = RN[r];
    float s = RS[r];
    int ci = (int)floorf((px - rx0[r]) / s), cj = (int)floorf((py - ry0[r]) / s);
    if (ci < 0) ci = 0; if (ci > n - 1) ci = n - 1;
    if (cj < 0) cj = 0; if (cj > n - 1) cj = n - 1;
    int far_j = cj > n - 1 - cj ? cj : n - 1 - cj;
    int far_i = ci > n - 1 - ci ? ci : n - 1 - ci;
    for (int dj = far_j; dj >= 0; dj--)
        for (int sj = -1; sj <= 1; sj += 2) {
            if (dj == 0 && sj > 0) continue;
            int j = cj + sj * dj;
            if (j < 0 || j >= n) continue;
            for (int di = far_i; di >= 0; di--)
                for (int si = -1; si <= 1; si += 2) {
                    if (di == 0 && si > 0) continue;
                    int i = ci + si * di;
                    if (i < 0 || i >= n) continue;
                    draw_cell(r, i, j);
                }
        }
}

// ── sky + horizon silhouette, built unrolled then turned by the camera roll ────
static void scr_roll(float sx, float sy, int *ox, int *oy) {
    float u = sx - 160.0f, w = HZ0 - sy;
    *ox = (int)(160.0f + u * crc - w * crs);
    *oy = (int)(HZ0 - (u * crs + w * crc));
}
static void quad_roll(float x0, float y0, float x1, float y1, int col) {   // axis rect, rolled
    int ax, ay, bx, by, cx, cy, dx, dy;
    scr_roll(x0, y0, &ax, &ay); scr_roll(x1, y0, &bx, &by);
    scr_roll(x1, y1, &cx, &cy); scr_roll(x0, y1, &dx, &dy);
    trifill(ax, ay, bx, by, cx, cy, col); trifill(ax, ay, cx, cy, dx, dy, col);
}
// THE ISLAND'S REAL SKYLINE (island mode): per screen column, march a ray across the island from
// where the drawn terrain fades (~2.9 km) to the far coast, keeping the highest elevation angle the
// land reaches (minus the Earth's curvature) in three distance bands. Far bands are paler and bluer
// (aerial perspective) and drawn first; nearer ridges overlap them; the near terrain covers it all.
// Heights are exaggerated with distance (see below): physically true, far mountains were 3-6 px.
#define SKY_COLS ((SCREEN_W + 80) / 2 + 2)
static void island_skyline(float hz) {
    static const float D0[3] = { 2900.0f, 8000.0f, 16000.0f }, D1[3] = { 8000.0f, 16000.0f, 45000.0f };
    static const int   COL[3] = { CLR_BLUE_GREEN, CLR_DARK_GREY, CLR_INDIGO };   // near → far: land teal, grey, lavender (must contrast with the dusk sky)
    static float ang[3][SKY_COLS];
    int nc = 0;
    for (int x = -40; x < SCREEN_W + 40 && nc < SKY_COLS; x += 2, nc++) {
        float a = head * D2R + atanf((x - 160.0f) / FOCAL), ca = cosf(a), sa = sinf(a);
        for (int b = 0; b < 3; b++) ang[b][nc] = -1e9f;
        int b = 0;
        for (float d = D0[0]; d < D1[2]; d *= 1.045f) {
            while (b < 2 && d >= D1[b]) b++;
            float sx = px + ca * d, sy = py + sa * d;
            if (sx < 0 || sy < 0 || sx > isl_W || sy > isl_H) break;    // off the island: open sea
            float h = isl_h(sx, sy); if (h <= 0.5f) continue;
            float e = (h - camh - d * d / 12742000.0f) / d;              // the Earth's curvature
            // exaggerated with distance, Test Drive style: true at the haze edge (so it meets the drawn
            // terrain), ~x3 beyond 16 km, so a 600 m mountain 20 km off reads as a landmark, not 5 px
            e *= 1.0f + 2.0f * clamp((d - 2900.0f) / 13000.0f, 0, 1);
            if (e > ang[b][nc]) ang[b][nc] = e;
        }
    }
#ifdef DE_TRACE
    { float m[3] = { -9, -9, -9 }; for (int b = 0; b < 3; b++) for (int c = 0; c < nc; c++) if (ang[b][c] > m[b]) m[b] = ang[b][c];
      watch("sky_px", "%.0f %.0f %.0f", m[0] * FOCAL, m[1] * FOCAL, m[2] * FOCAL); watch("hz", "%.0f", hz); }
#endif
    for (int b = 2; b >= 0; b--) {                        // far first
        for (int c = 0; c + 1 < nc; c++) {
            float e0 = ang[b][c], e1 = ang[b][c + 1];
            if (e0 < -1e8f && e1 < -1e8f) continue;
            float h0 = e0 < -1e8f ? 0 : e0 * FOCAL, h1 = e1 < -1e8f ? 0 : e1 * FOCAL;
            if (h0 < -2 && h1 < -2) continue;              // below the horizon: hidden by haze anyway
            float x0 = -40 + c * 2.0f, x1 = x0 + 2.0f;
            int ax, ay, bx, by, cx, cy, dx, dy;
            scr_roll(x0, hz - h0, &ax, &ay); scr_roll(x1, hz - h1, &bx, &by);
            scr_roll(x1, hz + 1, &cx, &cy); scr_roll(x0, hz + 1, &dx, &dy);
            trifill(ax, ay, bx, by, cx, cy, COL[b]); trifill(ax, ay, cx, cy, dx, dy, COL[b]);
        }
    }
}
static void draw_sky(float hz) {
    static const int SKY[4] = { CLR_DARKER_BLUE, CLR_DARKER_PURPLE, CLR_MAUVE, HAZE };
    rectfill(0, 0, SCREEN_W, VIEW_H, HAZE);
    float top = hz - 70, bh = 10.0f;                       // 7 bands of 10 px
    quad_roll(-200, top - 400, 520, top, SKY[0]);
    for (int k = 0; k < 7; k++) {
        int a = SKY[k / 2], b = (k & 1) ? SKY[k / 2 + 1] : a;
        if (a != b) fillp(FILL_CHECKER, b);
        quad_roll(-200, top + k * bh, 520, top + (k + 1) * bh + 0.5f, a);
        fillp_reset();
    }
    if (ISL) { island_skyline(hz); return; }
    for (int x = -80; x < SCREEN_W + 80; x += 4) {         // far ridge, 4-px slabs
        float h[2];
        for (int e = 0; e < 2; e++) {
            float a = (head + atanf((x + e * 4 - 160.0f) / FOCAL) * 57.29578f) * D2R;
            float r1 = noise2(cosf(a) * 2.2f + 11.0f, sinf(a) * 2.2f + 3.0f);
            float r2 = noise2(cosf(a) * 9.0f + 40.0f, sinf(a) * 9.0f + 7.0f);
            h[e] = 3 + 16 * r1 * r1 + 3 * r2;
        }
        int ax, ay, bx, by, cx, cy, dx, dy;
        scr_roll(x, hz - h[0], &ax, &ay); scr_roll(x + 4, hz - h[1], &bx, &by);
        scr_roll(x + 4, hz + 1, &cx, &cy); scr_roll(x, hz + 1, &dx, &dy);
        trifill(ax, ay, bx, by, cx, cy, CLR_INDIGO); trifill(ax, ay, cx, cy, dx, dy, CLR_INDIGO);
    }
}

// ── the car ────────────────────────────────────────────────────────────────────
#define WB   2.6f               // wheelbase (m)
#define TRK  1.5f               // track (m)
static int on_road_at(float x, float y) {
    if (ISL) { IHit r = isl_nearest(x, y, NK_ANYROAD, 15); return r.ok && r.dist <= HW[ipl[r.poly].cls] + 0.3f; }
    return wn_road_at(x, y).on_road;
}
static int surf_at(float x, float y) {
    if (ISL) {
        IHit r = isl_nearest(x, y, NK_ANYROAD, 15);
        if (r.ok && r.dist <= HW[ipl[r.poly].cls] + 0.3f) return ipl[r.poly].cls == CL_DIRT ? B_GRAVEL : B_ASPHALT;
        IHit w = isl_nearest(x, y, NK_RIVER, 10);
        if (w.ok && w.dist < 3.0f) return B_WATER;           // a ford: a river with no bridge
        return band_at(raw_at(x, y), x, y);
    }
    WnHit h = wn_road_at(x, y);
    if (h.dist <= HW[h.cls] + 0.3f) return h.cls == CL_DIRT ? B_GRAVEL : B_ASPHALT;
    float t = raw_at(x, y);
    return band_at(t, x, y);
}
// what a wheel feels: the ground + a little surface-texture bump (felt, not drawn)
static float wheel_h(float x, float y, float rough) {
    float t = raw_at(x, y), g = hgt_xy(x, y, t);
    if (ISL) {                                          // on a deck? ride it if the car is nearer it than the ground
        IHit r = isl_nearest(x, y, NK_BRIDGE, 12);
        if (r.ok && ipl_br[r.poly] >= 0 && r.dist <= HW[ipl[r.poly].cls] + 0.6f) {
            float zd = deck_h(r.poly, r.s), here = camh - EYE;
            if (fabsf(zd - here) < fabsf(g - here) + 0.5f) g = zd;
        }
    }
    if (t == -0.5f && wn_road_at(x, y).on_road) {         // bridge: ride the deck, not the river bed
        float b = wn_height_base(x, y);
        if (b > 0) g = b * vscale + detail_at(x, y, b);
    }
    return g + rough * 0.12f * (noise2(x / 2.5f, y / 2.5f) - 0.5f);
}
#ifdef DE_TRACE
// TEST-ONLY autopilot (harness builds; key P): steer at a point ~25 m ahead on the nearest
// drawn road centre-line, one lane to the right. Lets a script prove "follow a road for km".
static int autop;
static float autopilot_steer(void) {
    float fx = cosf(head * D2R), fy = sinf(head * D2R);
    float ax = px + fx * 25, ay = py + fy * 25, best = 1e30f, bx = ax, by = ay, tx = fx, ty = fy;
    if (ISL) {
        IHit h = isl_nearest(ax, ay, NK_ANYROAD, 60);
        if (h.ok) { bx = h.qx; by = h.qy; tx = h.tx; ty = h.ty; }
    } else {
    eg_ensure(px, py);
    for (int e = 0; e < eg_n; e++) {
        const REdge *ed = &eg_e[e];
        for (int i = 0; i < LINK_SAMPLES; i++) {
            if (seg_d2(ax, ay, ed->x[i], ed->y[i], ed->x[i+1], ed->y[i+1]) > 900) continue;
            for (int k = 0; k <= 24; k++) {
                float qx, qy, rx, ry; edge_pt(ed, i, k / 24.0f, &qx, &qy);
                edge_pt(ed, i, fminf(k / 24.0f + 0.02f, 1), &rx, &ry);
                float dd = (qx - ax) * (qx - ax) + (qy - ay) * (qy - ay);
                if (dd < best) { best = dd; bx = qx; by = qy; tx = rx - qx; ty = ry - qy; }
            }
        }
    }
    }
    float tl = fsqrt(tx * tx + ty * ty); if (tl <= 0) return 0;
    tx /= tl; ty /= tl;
    if (tx * fx + ty * fy < 0) { tx = -tx; ty = -ty; }       // travel direction
    bx += -ty * 1.8f; by += tx * 1.8f;                        // keep right
    float want = atan2f(by - py, bx - px) / D2R, diff = want - head;
    while (diff > 180) diff -= 360; while (diff < -180) diff += 360;
    return clamp(diff / 8.0f, -1, 1);
}
#endif
static void drive(float d) {
    int gas = btn(0, BTN_UP), brk = btn(0, BTN_DOWN);
    float st = (btn(0, BTN_RIGHT) ? 1.0f : 0) - (btn(0, BTN_LEFT) ? 1.0f : 0);
#ifdef DE_TRACE
    if (keyp('P')) autop = !autop;
    if (autop) { st = autopilot_steer(); float v = fsqrt(vx*vx + vy*vy); gas = v < 22; brk = v > 26; }
#endif
    steer += (st - steer) * clamp(d * 6.0f, 0, 1);

    cur_surf = surf_at(px, py);
    const Surf *S = &SURF[cur_surf];
    float fx = cosf(head * D2R), fy = sinf(head * D2R), lx = -fy, ly = fx;
    float vf = vx * fx + vy * fy, vl = vx * lx + vy * ly;

    // four wheels → pitch + roll + ride height
    float hfl = wheel_h(px + fx * WB/2 - lx * TRK/2, py + fy * WB/2 - ly * TRK/2, S->rough);
    float hfr = wheel_h(px + fx * WB/2 + lx * TRK/2, py + fy * WB/2 + ly * TRK/2, S->rough);
    float hrl = wheel_h(px - fx * WB/2 - lx * TRK/2, py - fy * WB/2 - ly * TRK/2, S->rough);
    float hrr = wheel_h(px - fx * WB/2 + lx * TRK/2, py - fy * WB/2 + ly * TRK/2, S->rough);
    float grade = ((hfl + hfr) - (hrl + hrr)) / (2 * WB);
    float bank  = ((hfl + hrl) - (hfr + hrr)) / (2 * TRK);   // + = left side high

    // longitudinal: power-limited drive, brakes, gravity, surface drag + air
    float acc = 0;
    if (gas) acc += 7.5f * (1.0f - fabsf(vf) / 60.0f);
    if (brk) acc -= vf > 0.5f ? 11.0f * (0.5f + 0.5f * S->grip) : 3.0f;
    acc -= 9.8f * grade / fsqrt(1 + grade * grade);
    acc -= vf * (0.06f * fabsf(vf) / 30.0f + S->roll);
    vf += acc * d;
    if (vf < -6) vf = -6;
    if (!gas && !brk && fabsf(vf) < 0.3f) vf = 0;
    // lateral: grip bleeds sideways velocity — low grip = the tail drifts
    vl *= expf(-S->grip * 9.0f * d);

    // steering: less lock at speed, and less bite on loose ground
    float maxst = 32.0f / (1.0f + fabsf(vf) / 10.0f);
    head += steer * maxst * vf / 6.0f * (0.45f + 0.55f * S->grip) * d;

    vx = fx * vf + lx * vl; vy = fy * vf + ly * vl;
    float nx = px + vx * d, ny = py + vy * d;
    if (raw_at(nx, ny) < 0 && !on_road_at(nx, ny)) { vx = vy = 0; }   // water (bridges ok)
    else { px = nx; py = ny; }

    float spring = clamp(d * 9.0f, 0, 1);
    pitch += (atanf(grade) - pitch) * spring;
    roll  += (atanf(bank)  - roll)  * spring;
    camh = (hfl + hfr + hrl + hrr) * 0.25f + EYE;
    float spd = fsqrt(vx * vx + vy * vy);
    dash_shake = S->rough * clamp(spd / 20.0f, 0, 1.5f) * 2.0f;
}

static void spawn(void) {
    vx = vy = 0; steer = 0;
    if (ISL) {                                          // on a road at the biggest town
        int best = -1;
        for (int s = 0; s < niset; s++) if (best < 0 || iset[s].r > iset[best].r) best = s;
        float sx = best >= 0 ? iset[best].x : isl_W * 0.5f, sy = best >= 0 ? iset[best].y : isl_H * 0.5f;
        IHit h = isl_nearest(sx, sy, NK_ROAD, 2500);
        if (h.ok) {
            head = atan2f(h.ty, h.tx) / D2R;
            px = h.qx - h.ty * 1.8f; py = h.qy + h.tx * 1.8f;   // keep right: one lane over
        } else { px = sx; py = sy; }
    } else {
        float rx, ry, ra;
        px = 40000; py = 30000;
        if (wn_nearest_road_point(px, py, &rx, &ry, &ra)) {
            head = ra;                                      // keep right: one lane over
            px = rx - sinf(ra * D2R) * 1.8f; py = ry + cosf(ra * D2R) * 1.8f;
        }
    }
    camh = ground(px, py) + EYE;
}
void init(void) {
    const char *path = de_data_path();
    // default island: seed 3 (it has every road type, the expressway and its flyovers included),
    // then seed 1; with neither, the endless worldnet world
    ISL = path ? isl_load(path) : (isl_load("../data/islandstudio-3.rvb") || isl_load("../data/islandstudio-1.rvb"));
    if (ISL) { isl_find_junctions(); isl_build_decks(); }
    spawn();
}

void update(void) {
    float d = dt();
    { const char *f = de_dropped_file(); if (f && isl_load(f)) { ISL = 1; isl_find_junctions(); isl_build_decks(); spawn(); } }   // drop an island .rvb
    if (ISL && keyp('K') && nibr > 0) {               // dev: jump 60 m before the next bridge, facing it
        static int bcur = -1; bcur = (bcur + 1) % nibr;    // every bridge in turn (river bridges and flyovers)
        const IPoly *q = &ipl[ibr[bcur].q]; int a = q->start, e = a + q->n - 1;
        float dx = ipx[e] - ipx[a], dy = ipy[e] - ipy[a], l = fsqrt(dx * dx + dy * dy) + 1e-6f;
        px = ipx[a] - dx / l * 60.0f; py = ipy[a] - dy / l * 60.0f; head = atan2f(dy, dx) / D2R;
        vx = vy = 0; camh = ground(px, py) + EYE;
    }
    if (ISL && keyp('J') && nijn > 0) {               // dev: jump 40 m before the next junction, facing it
        static int jcur = -1;
        for (int tries = 0; tries < nijn; tries++) { jcur = (jcur + 1) % nijn; if (ijn[jcur].n >= 3) break; }
        IJunc *J = &ijn[jcur]; float b = J->brg[0] * D2R;
        px = J->x + cosf(b) * 40.0f; py = J->y + sinf(b) * 40.0f; head = J->brg[0] + 180.0f;
        vx = vy = 0; camh = ground(px, py) + EYE;
    }
    if (keyp('H')) dbg = !dbg;
    if (keyp('O')) drone = !drone;
    if (keyp(KEY_TAB)) show_map = !show_map;
    if (show_map) {
        float w = mouse_wheel();
        if (w != 0) map_mpp = clamp(map_mpp * (w > 0 ? 0.8f : 1.25f), 4, 120);
        int mx = mouse_x(), my = mouse_y();
        int on_btn = !map_follow && mx >= MAP_BTN_X && mx < MAP_BTN_X + MAP_BTN_W
                                 && my >= MAP_BTN_Y && my < MAP_BTN_Y + MAP_BTN_H;
        if (mouse_pressed(MOUSE_LEFT)) {
            if (on_btn) map_follow = 1;                  // re-centre: follow the car again
            else if (my < VIEW_H) {
                if (map_follow) { map_cx = px; map_cy = py; map_follow = 0; }
                map_drag = 1; map_mx = mx; map_my = my;
            }
        }
        if (map_drag) {
            if (!mouse_down(MOUSE_LEFT)) map_drag = 0;
            else {                                       // grab-and-pull: the world moves with the mouse
                map_cx -= (mx - map_mx) * map_mpp; map_cy -= (my - map_my) * map_mpp;
                map_mx = mx; map_my = my;
            }
        }
    }
    if (key('-')) vscale = clamp(vscale - 300 * d, 100, 4000);
    if (key('=')) vscale = clamp(vscale + 300 * d, 100, 4000);
    if (key('[')) detail = clamp(detail - 40 * d, 0, 400);
    if (key(']')) detail = clamp(detail + 40 * d, 0, 400);
    drive(d);
#ifdef DE_TRACE
    watch("x", "%.0f", px); watch("y", "%.0f", py); watch("head", "%.1f", head);
    watch("spd", "%.1f", fsqrt(vx * vx + vy * vy)); watch("camh", "%.1f", camh);
    watch("ntri", "%d", ntri); watch("surf", "%s", SURF[cur_surf].name);
    if (ISL) { IHit r = isl_nearest(px, py, NK_BRIDGE, 60);
        watch("brd", "%.1f", r.ok ? r.dist : -1.0f); watch("deckz", "%.1f", (r.ok && ipl_br[r.poly] >= 0) ? deck_h(r.poly, r.s) : -1.0f);
        IHit a2 = isl_nearest(px, py, NK_ANYROAD, 60); watch("road_cls", "%d", a2.ok ? ipl[a2.poly].cls : -1); watch("road_br", "%d", a2.ok ? ipl[a2.poly].bridge : -1); }
    watch("njunc", "%d", ISL ? nijn : 0); watch("nbridge", "%d", ISL ? nibr : 0); watch("pc_drop", "%d", pc_drop); watch("nbin", "%d", nbin);
    watch("pitch", "%.3f", pitch); watch("roll", "%.3f", roll); watch("npc", "%d", npc);
#endif
}

static void draw_dash(void) {
    int sy = (int)(dash_shake * (noise(now() * 23.0f) - 0.5f) * 2.0f);
    camera(0, sy);
    rectfill(0, VIEW_H - 2, SCREEN_W, SCREEN_H - VIEW_H + 6, CLR_BROWNISH_BLACK);
    line(0, VIEW_H - 2, SCREEN_W, VIEW_H - 2, CLR_DARK_BROWN);
    int cx = 70, cy = 172, r = 20;
    circfill(cx, cy, r, CLR_DARKER_GREY);
    circ(cx, cy, r, CLR_DARK_BROWN);
    float kmh = fsqrt(vx * vx + vy * vy) * 3.6f;
    float a = (135 + clamp(kmh / 180.0f, 0, 1) * 270) * D2R;
    line(cx, cy, cx + (int)(cosf(a) * (r - 3)), cy + (int)(sinf(a) * (r - 3)), CLR_DARK_PEACH);
    print(str("%3d", (int)kmh), cx - 12, cy + 8, CLR_MEDIUM_GREY);
    if (dbg) {
        float hdg = head + 90; while (hdg < 0) hdg += 360; while (hdg >= 360) hdg -= 360;
        font(FONT_SMALL);
        print(str("hdg %3d  alt %4dm  %s", (int)hdg, (int)(camh - EYE), SURF[cur_surf].name), 120, 146, CLR_MEDIUM_GREY);
        print(str("hills x%d +%dm  tris %d  road %d", (int)vscale, (int)detail, ntri, npc), 120, 156, CLR_DARK_GREY);
        print(str("pos %d,%d", (int)px, (int)py), 120, 166, CLR_DARK_GREY);
        print(ISL ? str("island: %s", isl_name) : "world: worldnet (endless)", 120, 176, CLR_DARK_GREY);
        print("arrows drive -/= hills [/] bumps H", 120, 184, CLR_DARKER_GREY);
        font(FONT_NORMAL);
    }
    camera(0, 0);
}

// ── TAB map: north-up, car at the centre. Terrain sampled per 2 px block (fixed cost, only
// while open); roads are the cached worldnet edges (whole links around the car, so the
// next towns show); towns from the node lattice. Wheel zooms. ───────────────────────────
static int map_col(float x, float y) {
    float t = raw_at(x, y);
    if (t == -0.5f) return CLR_TRUE_BLUE;                // river
    if (t < 0)      return CLR_DARK_BLUE;
    int b = band_at(t, x, y);
    switch (b) {
        case B_SAND:   return CLR_BROWN;
        case B_FOREST: return CLR_BLUE_GREEN;
        case B_HILL:   return CLR_DARK_BROWN;
        case B_ROCK:   return CLR_DARK_GREY;
        case B_PEAK:   return CLR_MEDIUM_GREY;
        default:       return map_mpp < 12 ? RAMP[b].c[2] : CLR_DARK_GREEN;   // parcels only up close
    }
}
static void draw_map(void) {
    int cx = SCREEN_W / 2, cy = VIEW_H / 2, st = 2;
    float m = map_mpp;
    float ox = map_follow ? px : map_cx, oy = map_follow ? py : map_cy;   // world point at the centre
    clip(0, 0, SCREEN_W, VIEW_H);
    for (int sy = 0; sy < VIEW_H; sy += st)
        for (int sx = 0; sx < SCREEN_W; sx += st)
            rectfill(sx, sy, st, st, map_col(ox + (sx + 1 - cx) * m, oy + (sy + 1 - cy) * m));
    // roads: casings, then centres, minor classes first so highways sit on top
    static const int RC[5] = { CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_PEACH, CLR_WHITE, CLR_MAUVE };
    if (ISL) {
        float x0w = ox - cx * m, x1w = ox + cx * m, y0w = oy - cy * m, y1w = oy + cy * m;
        for (int pass = 0; pass < 2; pass++)            // rivers, then roads minor → major
            for (int cls = CL_DIRT; cls >= CL_MOTORWAY; cls--)
                for (int q = 0; q < nipl; q++) {
                    const IPoly *pp = &ipl[q];
                    if (pp->river != (pass == 0) || (pass == 1 && pp->cls != cls) || (pass == 0 && cls != CL_DIRT)) continue;
                    for (int i = pp->start + 1; i < pp->start + pp->n; i++) {
                        float ax = ipx[i - 1], ay = ipy[i - 1], bx = ipx[i], by = ipy[i];
                        if (fmaxf(ax, bx) < x0w || fminf(ax, bx) > x1w || fmaxf(ay, by) < y0w || fminf(ay, by) > y1w) continue;
                        int sx0 = cx + (int)((ax - ox) / m), sy0 = cy + (int)((ay - oy) / m);
                        int sx1 = cx + (int)((bx - ox) / m), sy1 = cy + (int)((by - oy) / m);
                        line(sx0, sy0, sx1, sy1, pass == 0 ? CLR_TRUE_BLUE : pp->bridge ? CLR_LIGHT_GREY : RC[cls]);
                    }
                }
        for (int k = 0; k < nifarm; k++) pset(cx + (int)((ifx[k] - ox) / m), cy + (int)((ify[k] - oy) / m), CLR_DARK_RED);
        font(FONT_SMALL);
        for (int k = 0; k < niset; k++) {
            int sx = cx + (int)((iset[k].x - ox) / m), sy = cy + (int)((iset[k].y - oy) / m), big = iset[k].rank;
            rectfill(sx - 1 - big, sy - 1 - big, 3 + 2 * big, 3 + 2 * big, CLR_BROWNISH_BLACK);
            rectfill(sx - big, sy - big, 1 + 2 * big, 1 + 2 * big, big == 2 ? CLR_RED : big ? CLR_PINK : CLR_PEACH);
        }
    } else {
    eg_ensure(px, py);
    for (int pass = 0; pass < 2; pass++)
        for (int cls = CL_DIRT; cls >= CL_MOTORWAY; cls--)
            for (int e = 0; e < eg_n; e++) {
                const REdge *ed = &eg_e[e];
                if (ed->cls != cls) continue;
                for (int i = 0; i < LINK_SAMPLES; i++) {
                    int x0 = cx + (int)((ed->x[i] - ox) / m),   y0 = cy + (int)((ed->y[i] - oy) / m);
                    int x1 = cx + (int)((ed->x[i+1] - ox) / m), y1 = cy + (int)((ed->y[i+1] - oy) / m);
                    if (pass == 0) {
                        if (cls <= CL_ARTERIAL) { line(x0 + 1, y0, x1 + 1, y1, CLR_BROWNISH_BLACK); line(x0, y0 + 1, x1, y1 + 1, CLR_BROWNISH_BLACK); }
                    } else line(x0, y0, x1, y1, ed->br[i] ? CLR_LIGHT_GREY : RC[cls]);
                }
            }
    // towns
    int cs = NODE_CS, hw = (int)(SCREEN_W * m / 2 / cs) + 1, hh = (int)(VIEW_H * m / 2 / cs) + 1;
    int ax = wn_ifloor(ox / cs), ay = wn_ifloor(oy / cs);
    font(FONT_SMALL);
    for (int gx = ax - hw; gx <= ax + hw; gx++)
        for (int gy = ay - hh; gy <= ay + hh; gy++) {
            float tx, ty; if (!get_node(gx, gy, &tx, &ty)) continue;
            int sx = cx + (int)((tx - ox) / m), sy = cy + (int)((ty - oy) / m);
            int big = town_rank(gx, gy) == RK_TOWN;
            rectfill(sx - 1 - big, sy - 1 - big, 3 + 2 * big, 3 + 2 * big, CLR_BROWNISH_BLACK);
            rectfill(sx - big, sy - big, 1 + 2 * big, 1 + 2 * big, big ? CLR_PINK : CLR_PEACH);
        }
    }
    // the car: an arrow along the heading
    float fx = cosf(head * D2R), fy = sinf(head * D2R);
    int ax0 = cx + (int)((px - ox) / m), ay0 = cy + (int)((py - oy) / m);   // car on the map
    int nx = ax0 + (int)(fx * 6), ny = ay0 + (int)(fy * 6);
    int lx = ax0 - (int)(fx * 3 - fy * 3), ly = ay0 - (int)(fy * 3 + fx * 3);
    int rx = ax0 - (int)(fx * 3 + fy * 3), ry = ay0 - (int)(fy * 3 - fx * 3);
    trifill(nx, ny, lx, ly, rx, ry, CLR_RED);
    line(nx, ny, lx, ly, CLR_BROWNISH_BLACK); line(nx, ny, rx, ry, CLR_BROWNISH_BLACK);
    line(lx, ly, rx, ry, CLR_BROWNISH_BLACK);
    // scale bar: 1 km
    int kb = (int)(1000 / m);
    rectfill(6, VIEW_H - 10, kb, 2, CLR_WHITE);
    print("1 km", 8 + kb, VIEW_H - 12, CLR_WHITE);
    print("N", SCREEN_W - 10, 4, CLR_WHITE);
    print("TAB close  drag pan  wheel zoom", 6, 4, CLR_WHITE);
    if (!map_follow) {                                   // re-centre button: crosshair + label
        int hot = mouse_x() >= MAP_BTN_X && mouse_x() < MAP_BTN_X + MAP_BTN_W
               && mouse_y() >= MAP_BTN_Y && mouse_y() < MAP_BTN_Y + MAP_BTN_H;
        rectfill(MAP_BTN_X, MAP_BTN_Y, MAP_BTN_W, MAP_BTN_H, hot ? CLR_DARK_PEACH : CLR_BROWNISH_BLACK);
        rect(MAP_BTN_X, MAP_BTN_Y, MAP_BTN_W, MAP_BTN_H, CLR_WHITE);
        int bx = MAP_BTN_X + 6, by = MAP_BTN_Y + 5;
        circ(bx, by, 3, CLR_RED); pset(bx, by, CLR_RED);
        print("centre", MAP_BTN_X + 13, MAP_BTN_Y + 3, CLR_WHITE);
    }
    font(FONT_NORMAL);
    clip(0, 0, 0, 0);
}

void draw(void) {
    if (show_map) { draw_map(); draw_dash(); return; }
    float save_camh = camh, save_pitch = pitch, save_px = px, save_py = py;
    if (drone) {                                        // lift + pull back + look down; restored below
        camh += 25.0f; pitch = -0.6f;
        px -= cosf(head * D2R) * 18.0f; py -= sinf(head * D2R) * 18.0f;
    }
    float h = head * D2R;
    cfx = cosf(h); cfy = sinf(h);
    crx = -sinf(h); cry = cosf(h);
    cpc = cosf(pitch); cps = sinf(pitch);
    crc = cosf(roll);  crs = sinf(roll);
    float hz = HZ0 + tanf(pitch) * FOCAL;

    clip(0, 0, SCREEN_W, VIEW_H);
    draw_sky(hz);
    ntri = 0;
    for (int r = NRING - 1; r >= 0; r--) build_ring(r);
    build_roads();
    for (int r = NRING - 1; r >= 0; r--) draw_ring(r);
    clip(0, 0, 0, 0);
    camh = save_camh; pitch = save_pitch; px = save_px; py = save_py;
    draw_dash();
}
