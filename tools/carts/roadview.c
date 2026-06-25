// ROADVIEW — drive around REAL road data, loaded at runtime.
//
// A broad-strokes map of an actual place. The road network is NOT generated and NOT
// baked into this cart — it's a JSON file fetched from OpenStreetMap (or the synthetic
// --demo set) and loaded when the cart starts, via --data <file>. Swap the file, see a
// different city; the cart never changes. Each way is classed HIGHWAY / ARTERIAL / ROAD
// / TRACK and drawn in its colour, widths sized in real metres so they fatten as you
// zoom in. The experiment: data-driven carts that read a blob at runtime instead of
// regenerating source per dataset (the floorwalker/seinelaan smell). See
// docs/design/external-data-carts.md.
//
//   make the data:   node tools/osm-roads.js --demo            (writes data/demo.json)
//                     node tools/osm-roads.js --place "Delft"   (writes data/delft.json)
//                     node tools/osm-roads.js --bbox 52.00,4.34,52.02,4.38
//   run it:           roadview loads data/demo.json by default. Then just DRAG any .json
//                     from the data folder onto the window to view it. The OPEN button
//                     (top-right) reveals that folder in Finder so you can find your files.
//                     (--data <file> / $DE_DATA still force a specific file.)
//
//   ◄▲▼► / WASD   pan  ·  left-drag  grab the map  ·  wheel  zoom  ·  0  reset view
//   B  toggle building footprints (filled blocks; auto-shown once you zoom in close)
//   drag a .json onto the window → load it   ·   OPEN button → reveal the data folder
//
#include "studio.h"
#include "json.h"      // EXPERIMENTAL cart-land JSON reader (vendored jsmn)
#include <stdlib.h>    // abs

enum { K_HIGHWAY, K_ARTERIAL, K_ROAD, K_TRACK, K_WATER, K_CANAL, K_BUILDING, K_N };

// per-class style: colour, casing colour, and real-world half-width in metres.
// WATER and BUILDING are filled areas (hw_m unused); the rest are polylines.
typedef struct { int col, casing; float hw_m; const char *label; } Style;
static const Style ST[K_N] = {
    /* HIGHWAY  */ { CLR_ORANGE,     CLR_BROWN,     7.0f, "highway"  },
    /* ARTERIAL */ { CLR_YELLOW,     CLR_BROWN,     5.0f, "arterial" },
    /* ROAD     */ { CLR_LIGHT_GREY, CLR_DARK_GREY, 3.0f, "road"     },
    /* TRACK    */ { CLR_BROWN,      CLR_BROWN,     1.5f, "track"    },
    /* WATER    */ { CLR_BLUE,       CLR_BLUE,      0.0f, "water"    },
    /* CANAL    */ { CLR_BLUE,       CLR_BLUE,      4.0f, "canal"    },
    /* BUILDING */ { CLR_DARK_GREY,  CLR_DARK_GREY, 0.0f, "building" },
};

// buildings (footprints) are LOD-gated: only filled once you've zoomed in enough that a
// footprint is a few pixels — keeps the wide view a clean road network, reveals blocks up close.
#define BUILD_GATE_PPM 0.12f

#define MAXPTS  800000                               // higher than roads alone need — buildings are dense
#define MAXPOLY  80000
#define BTN_W       40                               // "OPEN DATA" button width (top-right HUD)
static float PX[MAXPTS], PY[MAXPTS];                 // shared point pool (local metres, Y north-up)
static struct { int kind, start, count; } ways[MAXPOLY];
static int npoly = 0, nps = 0;
static int kcount[K_N];

static char dname[64] = "";                          // dataset name from the JSON
static char err[160]  = "";                          // non-empty = nothing to draw
static int  loaded_ok = 0, tried = 0;

static float bbminx, bbminy, bbmaxx, bbmaxy;         // data bounds (metres)
static float camx, camy;                             // world point at screen centre (metres)
static float ppm = 0.2f, fitppm = 0.2f;              // pixels per metre (zoom)
static int   dragging = 0, drag_px, drag_py;
static int   buildings_on = 1;                       // B toggles footprint fills (still LOD-gated)

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
    return K_ROAD;
}

// where the player's data files live. cwd is build/, so ../data == <project>/data.
// The OPEN button reveals this; osm-roads.js writes here by default.
#define DATA_DIR "../data"

static void reset_pools(void) {
    npoly = 0; nps = 0; loaded_ok = 0; dname[0] = 0; err[0] = 0;
    for (int k = 0; k < K_N; k++) kcount[k] = 0;
}

static void load_from(const char *path) {
    reset_pools();
    long len; char *js = json_slurp(path, &len);
    if (!js) { snprintf(err, sizeof err, "cannot read %s", path); return; }
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
                if (nps - start >= 2) {
                    ways[npoly].kind = kind; ways[npoly].start = start; ways[npoly].count = nps - start;
                    kcount[kind]++; npoly++;
                }
            }
            fi += json_span(tok, fi);
        }
    }
    free(tok); free(js);
    if (!npoly) { snprintf(err, sizeof err, "no roads in %s", path); return; }
    fit();
    loaded_ok = 1;
}

// pick the startup file: --data <file> (or $DE_DATA), else the demo in the data folder.
static void load_data(void) {
    const char *path = de_data_path();
    load_from(path ? path : DATA_DIR "/demo.json");
    if (!loaded_ok && !path)
        snprintf(err, sizeof err, "no data -- run: node tools/osm-roads.js --demo  (then drop a .json here)");
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
    int x = 4;
    for (int k = 0; k < K_N; k++) {
        if (!kcount[k]) continue;
        if (k == K_BUILDING && !build_shown) continue;
        rectfill(x, SCREEN_H - 7, 6, 5, ST[k].col);
        print(ST[k].label, x + 8, SCREEN_H - 7, CLR_MEDIUM_GREY);
        x += 8 + (int)strlen(ST[k].label) * 4 + 8;
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
    if (w != 0) { ppm *= (w > 0 ? 1.15f : 1.0f / 1.15f); if (ppm < fitppm * 0.25f) ppm = fitppm * 0.25f; if (ppm > fitppm * 40) ppm = fitppm * 40; }
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

    // painter's order: water, then building footprints (LOD-gated), then roads on top
    fill_areas(K_WATER);
    if (buildings_on && ppm >= BUILD_GATE_PPM) fill_areas(K_BUILDING);
    draw_class(K_CANAL, 1);
    draw_class(K_TRACK, 1);
    draw_class(K_ROAD,  1);
    draw_class(K_ARTERIAL, 0); draw_class(K_HIGHWAY, 0);      // casings
    draw_class(K_ARTERIAL, 1); draw_class(K_HIGHWAY, 1);      // fills

    // HUD
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("ROADVIEW", 4, 2, CLR_LIGHT_GREY);
    if (dname[0]) print(dname, 78, 2, CLR_ORANGE);
    char z[16]; snprintf(z, sizeof z, "%dm", (int)(SCREEN_W / ppm));     // scale: metres across screen
    font(FONT_SMALL); print(z, SCREEN_W - BTN_W - (int)strlen(z) * 4 - 5, 3, CLR_DARK_GREY); font(FONT_NORMAL);
    open_button();
    legend();
}
