/* de:meta
{
  "title": "roadview",
  "status": "active",
  "created": "2026-06-25",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "road-network"
  ],
  "lineage": "Sibling of roadnet but DATA-DRIVEN rather than generative: instead of synthesising a network it loads a real OpenStreetMap export at runtime (de_data_path()/de_dropped_file() + runtime/json.h), then classes and draws the real ways. First cart to read an external data blob at runtime instead of baking geometry into source (cf. data-tools/fmltools/floorwalker).",
  "description": "Drive around REAL road data, loaded at RUNTIME. The network is neither generated nor baked into the cart — it is an OpenStreetMap export (fetched by data-tools/roadview/osm-roads.js, or the synthetic --demo set) read from a JSON file when the cart starts, so the SAME cart shows any town: swap the file, see a different city. This is the data-driven-cart experiment that kills the floorwalker/seinelaan smell of regenerating a cart per dataset (docs/design/external-data-carts.md). Each way is classed HIGHWAY / ARTERIAL / ROAD / TRACK and drawn in its colour at a width sized in real metres (so roads fatten as you zoom in); canals are stroked, water areas filled in blue, parks and woods filled green, and building footprints + individual street trees revealed as LOD-gated detail once you zoom in close (B toggles footprints, T toggles trees) — all beneath the roads, with the bounding box auto-fitting to the screen. Opens on data/demo.json by default — then just DRAG any .json from the data folder onto the window to load that town, or click OPEN (top-right) to reveal the folder in Finder. Built on three tiny EXPERIMENTAL engine hooks: de_data_path() (--data <file> / $DE_DATA), de_dropped_file() (drag & drop) and de_open_path() (reveal folder); the JSON is parsed by runtime/json.h (vendored jsmn). Controls: arrows / WASD pan, left-drag grabs the map, mouse wheel zooms, 0 resets the view, B toggles building footprints, T toggles trees; drag a .json to load it, OPEN reveals the data folder."
}
de:meta */
// ROADVIEW — drive around REAL road data, loaded at runtime.
//
// A broad-strokes map of an actual place. The road network is NOT generated and NOT
// baked into this cart — it's a JSON file fetched from OpenStreetMap (or the synthetic
// --demo set) and loaded when the cart starts, via --data <file>. Swap the file, see a
// different city; the cart never changes. Roads are classed HIGHWAY / ARTERIAL / ROAD /
// TRACK (widths in real metres, so they fatten as you zoom in); rail is a dashed line.
// Underneath sit SimCity-style zoning blocks — residential = green, commercial = blue,
// industrial = yellow, farm = brown, parking = grey — plus parks, water and sand. Building
// footprints get a thin outline so each one stays separable (the JSON also carries the OSM
// building type in `sub`, for a downstream consumer that extrudes individual buildings).
// The experiment: data-driven carts that read a blob at runtime instead of regenerating
// source per dataset (the floorwalker/seinelaan smell).
//
//   DESIGN + KNOWLEDGE → docs/design/external-data-carts.md   (the source of truth for this cart)
//     · session handoff (what works, commits, quick-resume)   · the shared "vector features" schema
//     · the .rvb binary format spec + why (JSON parse was the load bottleneck)
//     · the road hierarchy (which OSM highway=* values map to which tier; what's collapsed/dropped)
//     · open items: hover inspector panel · two-line legend · Breda bbox framing · more road tiers
//
//   make the data:   node data-tools/roadview/osm-roads.js --demo            (writes data/demo.rvb)
//                     node data-tools/roadview/osm-roads.js --place "Delft"   (writes data/delft-*.rvb)
//                     node data-tools/roadview/osm-roads.js --bbox 52.00,4.34,52.02,4.38
//   run it:           roadview loads data/demo.rvb by default. Then just DRAG any data file
//                     (.rvb packed-binary, or .json) from the data folder onto the window to
//                     view it. Real cities are tens of MB of JSON; the .rvb binary loads ~instantly
//                     (no text parsing). The OPEN button (top-right) reveals that folder in Finder.
//                     (--data <file> / $DE_DATA still force a specific file.)
//
//   ◄▲▼► / WASD   pan  ·  left-drag  grab the map  ·  wheel  zoom  ·  0  reset view
//   B / T  toggle building footprints / tree dots (both auto-shown once you zoom in close)
//   drag a .json onto the window → load it   ·   OPEN button → reveal the data folder
//
#include "studio.h"
#include "json.h"      // EXPERIMENTAL cart-land JSON reader (vendored jsmn)
#include <stdlib.h>    // abs

enum { K_HIGHWAY, K_ARTERIAL, K_ROAD, K_TRACK, K_WATER, K_CANAL, K_BUILDING, K_GREEN, K_TREE,
       K_RESIDENTIAL, K_COMMERCIAL, K_INDUSTRIAL, K_FARM, K_PARKING, K_SAND, K_RAIL, K_COAST, K_N };

// per-class style: colour, casing/outline colour, and real-world half-width in metres.
// Area fills (hw_m unused): WATER/BUILDING/GREEN/SAND + the zoning blocks RESIDENTIAL/
// COMMERCIAL/INDUSTRIAL/FARM/PARKING. For BUILDING, `casing` is the footprint OUTLINE
// (drawn 1px around the fill so each footprint stays separable). TREE is a point dot.
// Lines (use hw_m): the road classes, CANAL, COAST, and RAIL (dashed).
typedef struct { int col, casing; float hw_m; const char *label; } Style;
static const Style ST[K_N] = {
    /* HIGHWAY     */ { CLR_ORANGE,       CLR_BROWN,        7.0f, "highway"     },
    /* ARTERIAL    */ { CLR_YELLOW,       CLR_BROWN,        5.0f, "arterial"    },
    /* ROAD        */ { CLR_LIGHT_GREY,   CLR_DARK_GREY,    3.0f, "road"        },
    /* TRACK       */ { CLR_BROWN,        CLR_BROWN,        1.5f, "track"       },
    /* WATER       */ { CLR_BLUE,         CLR_BLUE,         0.0f, "water"       },
    /* CANAL       */ { CLR_BLUE,         CLR_BLUE,         4.0f, "canal"       },
    /* BUILDING    */ { CLR_DARK_GREY,    CLR_LIGHT_GREY,   0.0f, "building"    },  // casing = outline
    /* GREEN       */ { CLR_DARK_GREEN,   CLR_DARK_GREEN,   0.0f, "park"        },
    /* TREE        */ { CLR_GREEN,        CLR_GREEN,        0.0f, "trees"       },
    /* RESIDENTIAL */ { CLR_MEDIUM_GREEN, CLR_MEDIUM_GREEN, 0.0f, "residential" },  // SimCity R = green
    /* COMMERCIAL  */ { CLR_TRUE_BLUE,    CLR_TRUE_BLUE,    0.0f, "commercial"  },  // SimCity C = blue
    /* INDUSTRIAL  */ { CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW, 0.0f, "industrial"  },  // SimCity I = yellow
    /* FARM        */ { CLR_BROWN,        CLR_BROWN,        0.0f, "farm"        },
    /* PARKING     */ { CLR_MEDIUM_GREY,  CLR_MEDIUM_GREY,  0.0f, "parking"     },
    /* SAND        */ { CLR_LIGHT_PEACH,  CLR_LIGHT_PEACH,  0.0f, "sand"        },
    /* RAIL        */ { CLR_DARKER_GREY,  CLR_DARKER_GREY,  2.0f, "rail"        },  // dashed line
    /* COAST       */ { CLR_DARK_BLUE,    CLR_DARK_BLUE,    1.0f, "coast"       },
};

// footprints + tree dots are LOD-gated: only drawn once zoomed in enough that they are a
// few pixels (sub-pixel at fit = wasted draws + clutter). Trees are finer → a higher gate.
#define BUILD_GATE_PPM 0.12f
#define TREE_GATE_PPM  0.25f

#define MAXPTS  5000000                              // a whole big city: Amsterdam is ~4.2M pts / 575k ways
#define MAXPOLY  650000                              // (mostly buildings). ~48MB of static pools — fine on desktop.
#define BTN_W       40                               // "OPEN DATA" button width (top-right HUD)
static float PX[MAXPTS], PY[MAXPTS];                 // shared point pool (local metres, Y north-up)
static struct { int kind, start, count; } ways[MAXPOLY];
static int npoly = 0, nps = 0;
static int kcount[K_N];

static char dname[64] = "";                          // dataset name from the JSON
static char err[160]  = "";                          // non-empty = nothing to draw
static int  loaded_ok = 0, tried = 0, truncated = 0;   // truncated = dataset hit MAXPOLY/MAXPTS

static float bbminx, bbminy, bbmaxx, bbmaxy;         // data bounds (metres)
static float camx, camy;                             // world point at screen centre (metres)
static float ppm = 0.2f, fitppm = 0.2f;              // pixels per metre (zoom)
static int   dragging = 0, drag_px, drag_py;
static int   buildings_on = 1;                       // B toggles footprint fills (still LOD-gated)
static int   trees_on = 1;                           // T toggles tree dots (still LOD-gated)

// world (metres) → screen pixels; Y flips so north is up
static int sx(float wx) { return SCREEN_W / 2 + (int)((wx - camx) * ppm); }
static int sy(float wy) { return SCREEN_H / 2 - (int)((wy - camy) * ppm); }

static void fit(void) {
    float w = bbmaxx - bbminx, h = bbmaxy - bbminy;
    if (w < 1) w = 1; if (h < 1) h = 1;
    float fx = (SCREEN_W * 0.94f) / w, fy = ((SCREEN_H - 22) * 0.94f) / h;
    ppm = fitppm = (fx < fy ? fx : fy);
    camx = (bbminx + bbmaxx) * 0.5f;
    camy = (bbminy + bbmaxy) * 0.5f;
}

static int kind_of(const char *js, const jsmntok_t *t) {
    if (json_eq(js, t, "highway"))  return K_HIGHWAY;
    if (json_eq(js, t, "arterial")) return K_ARTERIAL;
    if (json_eq(js, t, "track"))    return K_TRACK;
    if (json_eq(js, t, "water"))    return K_WATER;
    if (json_eq(js, t, "canal"))    return K_CANAL;
    if (json_eq(js, t, "building")) return K_BUILDING;
    if (json_eq(js, t, "green"))    return K_GREEN;
    if (json_eq(js, t, "tree"))     return K_TREE;
    if (json_eq(js, t, "residential")) return K_RESIDENTIAL;
    if (json_eq(js, t, "commercial"))  return K_COMMERCIAL;
    if (json_eq(js, t, "industrial"))  return K_INDUSTRIAL;
    if (json_eq(js, t, "farm"))        return K_FARM;
    if (json_eq(js, t, "parking"))     return K_PARKING;
    if (json_eq(js, t, "sand"))        return K_SAND;
    if (json_eq(js, t, "rail"))        return K_RAIL;
    if (json_eq(js, t, "coast"))       return K_COAST;
    return K_ROAD;
}

// where the player's data files live. cwd is build/, so ../data == <project>/data.
// The OPEN button reveals this; osm-roads.js writes here by default.
#define DATA_DIR "../data"

static void reset_pools(void) {
    npoly = 0; nps = 0; loaded_ok = 0; truncated = 0; dname[0] = 0; err[0] = 0;
    for (int k = 0; k < K_N; k++) kcount[k] = 0;
}

// Fast path: a packed binary blob (magic "RVB1") from osm-roads.js — no tokenising, no
// strtod, just walk the buffer. Layout (all multi-byte ints little-endian):
//   magic[4] | int32 nfeat | int32 namelen | name bytes | float32 bbox[4]
//   per feature: int32 kind | int32 sublen | sub bytes (ignored) | int32 npts | float32 pts[npts*2]
// `kind` is the K_* index — MUST match KIND_IX in data-tools/roadview/osm-roads.js.
static void load_bin(const char *buf, long len) {
    const char *p = buf + 4, *end = buf + len;             // skip magic
    int nfeat, namelen;
    memcpy(&nfeat, p, 4);   p += 4;
    memcpy(&namelen, p, 4); p += 4;
    int nl = namelen; if (nl < 0) nl = 0; if (nl > (int)sizeof dname - 1) nl = (int)sizeof dname - 1;
    memcpy(dname, p, (size_t)nl); dname[nl] = 0; p += namelen;
    float bb[4]; memcpy(bb, p, 16); p += 16;
    bbminx = bb[0]; bbminy = bb[1]; bbmaxx = bb[2]; bbmaxy = bb[3];
    for (int f = 0; f < nfeat && npoly < MAXPOLY && p + 12 <= end; f++) {
        int kind, sublen, npts;
        memcpy(&kind, p, 4);   p += 4;
        memcpy(&sublen, p, 4); p += 4;
        p += sublen;                                       // cart ignores the building-type `sub`
        memcpy(&npts, p, 4);   p += 4;
        if (kind < 0 || kind >= K_N) kind = K_ROAD;
        int start = nps, got = 0;
        for (int j = 0; j < npts && p + 8 <= end; j++) {
            if (nps < MAXPTS) { float xy[2]; memcpy(xy, p, 8); PX[nps] = xy[0]; PY[nps] = xy[1]; nps++; got++; }
            p += 8;                                        // advance even when the pool is full
        }
        if (got >= 2 || (kind == K_TREE && got >= 1)) {
            ways[npoly].kind = kind; ways[npoly].start = start; ways[npoly].count = got;
            kcount[kind]++; npoly++;
        }
    }
}

static void load_from(const char *path) {
    reset_pools();
    long len; char *js = json_slurp(path, &len);
    if (!js) { snprintf(err, sizeof err, "cannot read %s", path); return; }
    if (len >= 4 && memcmp(js, "RVB1", 4) == 0) {          // binary fast path
        load_bin(js, len);
        free(js);
        if (!npoly) { snprintf(err, sizeof err, "no features in %s", path); return; }
        if (npoly >= MAXPOLY || nps >= MAXPTS) truncated = 1;
        fit(); loaded_ok = 1;
        return;
    }
    jsmntok_t *tok; int nt = json_parse(js, &tok);
    if (nt < 1 || tok[0].type != JSMN_OBJECT) { snprintf(err, sizeof err, "bad json in %s (err %d)", path, nt); free(js); free(tok); return; }

    int ni = json_get(js, tok, 0, "name");
    if (ni >= 0) json_str(dname, sizeof dname, js, &tok[ni]);

    int bi = json_get(js, tok, 0, "bbox");
    if (bi >= 0 && tok[bi].type == JSMN_ARRAY && tok[bi].size >= 4) {
        bbminx = (float)json_num(js, &tok[bi + 1]); bbminy = (float)json_num(js, &tok[bi + 2]);
        bbmaxx = (float)json_num(js, &tok[bi + 3]); bbmaxy = (float)json_num(js, &tok[bi + 4]);
    }

    int fa = json_get(js, tok, 0, "features");
    if (fa >= 0 && tok[fa].type == JSMN_ARRAY) {
        int fi = fa + 1;
        for (int f = 0; f < tok[fa].size && npoly < MAXPOLY; f++) {
            int ki = json_get(js, tok, fi, "kind");
            int pi = json_get(js, tok, fi, "pts");
            int kind = (ki >= 0) ? kind_of(js, &tok[ki]) : K_ROAD;
            if (pi >= 0 && tok[pi].type == JSMN_ARRAY) {
                int cnt = tok[pi].size / 2, start = nps;
                for (int j = 0; j < cnt && nps < MAXPTS; j++) {
                    PX[nps] = (float)json_num(js, &tok[pi + 1 + j * 2]);
                    PY[nps] = (float)json_num(js, &tok[pi + 1 + j * 2 + 1]);
                    nps++;
                }
                int cnt2 = nps - start;                       // trees are a single point; lines/areas need >=2
                if (cnt2 >= 2 || (kind == K_TREE && cnt2 >= 1)) {
                    ways[npoly].kind = kind; ways[npoly].start = start; ways[npoly].count = cnt2;
                    kcount[kind]++; npoly++;
                }
            }
            fi += json_span(tok, fi);
        }
    }
    free(tok); free(js);
    if (!npoly) { snprintf(err, sizeof err, "no roads in %s", path); return; }
    if (npoly >= MAXPOLY || nps >= MAXPTS) truncated = 1;   // dataset bigger than the pools — flag it, don't hide it
    fit();
    loaded_ok = 1;
}

// pick the startup file: --data <file> (or $DE_DATA), else the demo in the data folder.
static void load_data(void) {
    const char *path = de_data_path();
    load_from(path ? path : DATA_DIR "/demo.rvb");
    if (!loaded_ok && !path)
        snprintf(err, sizeof err, "no data -- run: node data-tools/roadview/osm-roads.js --demo  (then drop a file here)");
}

// stroke one polyline as overlapping dots (round caps, scales with zoom). r in px.
static void stroke(int start, int count, int r, int col) {
    if (r < 1) r = 1;
    for (int k = 0; k + 1 < count; k++) {
        int x0 = sx(PX[start + k]),     y0 = sy(PY[start + k]);
        int x1 = sx(PX[start + k + 1]), y1 = sy(PY[start + k + 1]);
        // cull segments fully off-screen (cheap bbox test with margin)
        if ((x0 < -r && x1 < -r) || (x0 > SCREEN_W + r && x1 > SCREEN_W + r) ||
            (y0 < -r && y1 < -r) || (y0 > SCREEN_H + r && y1 > SCREEN_H + r)) continue;
        int dx = x1 - x0, dy = y1 - y0;
        int seg = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
        int steps = seg / r; if (steps < 1) steps = 1;
        for (int s = 0; s <= steps; s++) {
            float t = (float)s / steps;
            circfill(x0 + (int)(dx * t), y0 + (int)(dy * t), r, col);
        }
    }
}

// Filled-area kinds (water, buildings) are polygons, not strokes. Project each to a
// scratch int buffer and polyfill (handles concave); cull any fully off-screen.
#define MAXAREAPTS 4096
static int sxy[MAXAREAPTS * 2];
static void fill_areas(int kind) {
    int col = ST[kind].col;
    for (int i = 0; i < npoly; i++) {
        if (ways[i].kind != kind) continue;
        int c = ways[i].count; if (c < 3) continue; if (c > MAXAREAPTS) c = MAXAREAPTS;
        int st = ways[i].start;
        int minx = 1 << 28, miny = 1 << 28, maxx = -(1 << 28), maxy = -(1 << 28);
        for (int j = 0; j < c; j++) {
            int X = sx(PX[st + j]), Y = sy(PY[st + j]);
            sxy[j * 2] = X; sxy[j * 2 + 1] = Y;
            if (X < minx) minx = X; if (X > maxx) maxx = X;
            if (Y < miny) miny = Y; if (Y > maxy) maxy = Y;
        }
        if (maxx < 0 || minx > SCREEN_W || maxy < 0 || miny > SCREEN_H) continue;
        polyfill(sxy, c, col);
        // outline: when casing differs from fill (buildings), trace the ring 1px so each
        // footprint stays visually separable — matters for downstream "extract buildings".
        int oc = ST[kind].casing;
        if (oc != col) {
            for (int j = 0; j + 1 < c; j++)
                line(sxy[j * 2], sxy[j * 2 + 1], sxy[(j + 1) * 2], sxy[(j + 1) * 2 + 1], oc);
            line(sxy[(c - 1) * 2], sxy[(c - 1) * 2 + 1], sxy[0], sxy[1], oc);   // close the ring
        }
    }
}

// rail: a thin DASHED line (drawn as gapped dots), so it reads as track, not road.
static void stroke_dashed(int start, int count, int r, int col) {
    if (r < 1) r = 1;
    int phase = 0;
    for (int k = 0; k + 1 < count; k++) {
        int x0 = sx(PX[start + k]),     y0 = sy(PY[start + k]);
        int x1 = sx(PX[start + k + 1]), y1 = sy(PY[start + k + 1]);
        if ((x0 < -r && x1 < -r) || (x0 > SCREEN_W + r && x1 > SCREEN_W + r) ||
            (y0 < -r && y1 < -r) || (y0 > SCREEN_H + r && y1 > SCREEN_H + r)) continue;
        int dx = x1 - x0, dy = y1 - y0;
        int seg = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
        int steps = seg / (r * 2); if (steps < 1) steps = 1;
        for (int s = 0; s <= steps; s++) {
            if (phase++ & 1) continue;                       // every other dot → dash
            float t = (float)s / steps;
            circfill(x0 + (int)(dx * t), y0 + (int)(dy * t), r, col);
        }
    }
}
static void draw_rail(void) {
    int r = (int)(ST[K_RAIL].hw_m * ppm + 0.5f); if (r < 1) r = 1; if (r > 2) r = 2;
    for (int i = 0; i < npoly; i++)
        if (ways[i].kind == K_RAIL) stroke_dashed(ways[i].start, ways[i].count, r, ST[K_RAIL].col);
}

// individual trees are point features (one point) → small dots, LOD-gated like footprints.
static void draw_trees(void) {
    int r = (int)(3.0f * ppm + 0.5f); if (r < 1) r = 1; if (r > 3) r = 3;   // ~3 m canopy, capped
    for (int i = 0; i < npoly; i++) {
        if (ways[i].kind != K_TREE) continue;
        int x = sx(PX[ways[i].start]), y = sy(PY[ways[i].start]);
        if (x < -r || x > SCREEN_W + r || y < -r || y > SCREEN_H + r) continue;
        circfill(x, y, r, ST[K_TREE].col);
    }
}

// draw every poly of one class for a phase (0 = casing, 1 = fill)
static void draw_class(int kind, int phase) {
    const Style *s = &ST[kind];
    int rf = (int)(s->hw_m * ppm + 0.5f); if (rf < 1) rf = 1;
    int r   = (phase == 0) ? rf + 1 : rf;
    int col = (phase == 0) ? s->casing : s->col;
    for (int i = 0; i < npoly; i++)
        if (ways[i].kind == kind) stroke(ways[i].start, ways[i].count, r, col);
}

static void legend(void) {
    font(FONT_SMALL);
    rectfill(0, SCREEN_H - 9, SCREEN_W, 9, CLR_BLACK);
    int build_shown = buildings_on && ppm >= BUILD_GATE_PPM;   // only list what's actually drawn
    int tree_shown  = trees_on && ppm >= TREE_GATE_PPM;
    int x = 4;
    for (int k = 0; k < K_N; k++) {
        if (!kcount[k]) continue;
        if (k == K_BUILDING && !build_shown) continue;
        if (k == K_TREE && !tree_shown) continue;
        if (x > SCREEN_W - 24) break;                        // don't run a long legend off-screen
        rectfill(x, SCREEN_H - 7, 6, 5, ST[k].col);
        print(ST[k].label, x + 8, SCREEN_H - 7, CLR_MEDIUM_GREY);
        x += 8 + (int)strlen(ST[k].label) * 4 + 4;
    }
    font(FONT_NORMAL);
}

// the OPEN-DATA button (top-right of the HUD); reveals DATA_DIR in the OS file manager
static void open_button(void) {
    int bx = SCREEN_W - BTN_W;
    int hot = (mouse_y() < 11 && mouse_x() >= bx);
    rectfill(bx, 1, BTN_W - 2, 9, hot ? CLR_BLUE : CLR_DARK_GREY);
    font(FONT_SMALL);
    print("OPEN", bx + 8, 3, CLR_WHITE);
    font(FONT_NORMAL);
}

void update(void) {
    // drag & drop a new data file onto the window → reload (works on the error screen too)
    const char *dropped = de_dropped_file();
    if (dropped) load_from(dropped);

    // OPEN DATA button: reveal the data folder in Finder/Explorer ("where do files go?")
    if (mouse_pressed(MOUSE_LEFT) && mouse_y() < 11 && mouse_x() >= SCREEN_W - BTN_W) {
        de_open_path(DATA_DIR);
        return;
    }
    if (!loaded_ok) return;

    float w = mouse_wheel();
    if (w != 0) {
        ppm *= (w > 0 ? 1.15f : 1.0f / 1.15f);
        float zmax = fitppm * 40.0f; if (zmax < 16.0f) zmax = 16.0f;   // ABSOLUTE ceiling so a big city
        if (ppm < fitppm * 0.25f) ppm = fitppm * 0.25f;                // (tiny fitppm) can still zoom to
        if (ppm > zmax) ppm = zmax;                                    // street/building detail (~16 px/m)
    }
    float pan = 12.0f / ppm;                                  // metres per frame at this zoom
    if (btn(0, BTN_LEFT)  || key('A')) camx -= pan;
    if (btn(0, BTN_RIGHT) || key('D')) camx += pan;
    if (btn(0, BTN_UP)    || key('W')) camy += pan;           // +Y = north = up
    if (btn(0, BTN_DOWN)  || key('S')) camy -= pan;
    if (mouse_pressed(MOUSE_LEFT) && mouse_y() >= 11) { dragging = 1; drag_px = mouse_x(); drag_py = mouse_y(); }
    if (!mouse_down(MOUSE_LEFT)) dragging = 0;
    if (dragging) {
        camx -= (mouse_x() - drag_px) / ppm;
        camy += (mouse_y() - drag_py) / ppm;                  // screen-down = south
        drag_px = mouse_x(); drag_py = mouse_y();
    }
    if (keyp('0')) fit();
    if (keyp('B') || keyp('b')) buildings_on = !buildings_on;
    if (keyp('T') || keyp('t')) trees_on = !trees_on;
}

void draw(void) {
    if (!tried) { tried = 1; load_data(); }
    cls(CLR_DARKER_GREY);

    if (!loaded_ok) {
        font(FONT_NORMAL);
        print_centered("ROADVIEW", SCREEN_W / 2, SCREEN_H / 2 - 12, CLR_LIGHT_GREY);
        font(FONT_SMALL);
        print_centered(err, SCREEN_W / 2, SCREEN_H / 2 + 2, CLR_ORANGE);
        font(FONT_SMALL);
        print_centered("drag a .json onto this window, or click OPEN (top-right) for the folder",
                       SCREEN_W / 2, SCREEN_H / 2 + 14, CLR_DARK_GREY);
        font(FONT_NORMAL);
        rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
        open_button();
        return;
    }

    // painter's order: zoning blocks → green → water → footprints → rail → canals →
    // coast → roads → tree dots on top.
    fill_areas(K_FARM);                                      // zoning area fills (bottom ground layer)
    fill_areas(K_RESIDENTIAL);
    fill_areas(K_COMMERCIAL);
    fill_areas(K_INDUSTRIAL);
    fill_areas(K_PARKING);
    fill_areas(K_SAND);
    fill_areas(K_GREEN);                                     // parks/woods over the zoning tints
    fill_areas(K_WATER);
    if (buildings_on && ppm >= BUILD_GATE_PPM) fill_areas(K_BUILDING);
    draw_rail();                                             // dashed
    draw_class(K_CANAL, 1);
    draw_class(K_COAST, 1);
    draw_class(K_TRACK, 1);
    draw_class(K_ROAD,  1);
    draw_class(K_ARTERIAL, 0); draw_class(K_HIGHWAY, 0);      // casings
    draw_class(K_ARTERIAL, 1); draw_class(K_HIGHWAY, 1);      // fills
    if (trees_on && ppm >= TREE_GATE_PPM) draw_trees();       // finest detail, on top

    // HUD
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("ROADVIEW", 4, 2, CLR_LIGHT_GREY);
    if (dname[0]) print(dname, 78, 2, CLR_ORANGE);
    if (truncated) print("(truncated)", 78 + (int)strlen(dname) * 8 + 6, 2, CLR_RED);
    char z[16]; snprintf(z, sizeof z, "%dm", (int)(SCREEN_W / ppm));     // scale: metres across screen
    font(FONT_SMALL); print(z, SCREEN_W - BTN_W - (int)strlen(z) * 4 - 5, 3, CLR_DARK_GREY); font(FONT_NORMAL);
    open_button();
    legend();
}
