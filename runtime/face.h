// face.h — the device-face GRAMMAR (Layer 3 of design/responsive-first-device-face.md),
// cart-land (decision-0006 lane): carts #include this instead of the engine owning it.
//
// Why this exists. Every device face (acidcandy, chipjam, dubjam, deviceface) re-copies
// the same boilerplate — the chunky-canvas resize, the chassis + safe-area content area,
// carving the five zones, wiring the shared per-step register — and re-derives the
// arrangement rules the `acidwide` A–H mockup study found (often getting them wrong: the
// "F" face broke the column register with a side-rail). This header lets a cart DECLARE
// its zones and gets those rules by default:
//
//   · per-step zones share ONE full-width column lane (lay.h's LayLane register)
//   · bands dock TOP/BOTTOM only — a band can NEVER become a width-stealing side-rail
//     (the single rule that would have stopped the "F" mistake automatically)
//   · the HERO is always the remainder (so a taller device's extra height flows into it)
//   · REFLOW, never camera-scale (touch stays 1:1 — the ui.h desync gotcha)
//
// Scaffold, NOT straitjacket (the one thing to protect): every zone comes back as a plain
// lay.h `Box` you draw into with raw lay.h/ui.h, and a bespoke layout just doesn't call
// face_layout() — drop to raw lay.h anywhere. Opinionated by default, escapable by choice.
//
// Usage:
//   #include "studio.h"
//   #include "face.h"
//   static FaceZone ZONES[] = {                      // declared VISUALLY top→bottom
//       { FACE_BAND, EDGE_TOP,    0.12f, "nav"     }, // ① thin transport / view tabs
//       { FACE_BAND, EDGE_TOP,    0.26f, "knobs"   }, // ② the live continuous controls
//       { FACE_HERO, 0,           0.00f, "display" }, // ③ the touchable screen (the remainder)
//       { FACE_LANE, EDGE_BOTTOM, 0.22f, "steps"   }, // ④ per-step grid — bound to the register
//       { FACE_BAND, EDGE_BOTTOM, 0.20f, "perform" }, // ⑤ keybed / pads
//   };
//   void draw(void) {
//       face_resize();                               // request the chunky canvas (route 2)
//       Box area = face_area(3);                      // content = whole screen inset 3, ∩ safe-rect
//       Face f = face_layout(area, ZONES, 5, 16);     // carve + enforce + build a 16-col register
//       // ... draw your chassis to box(0,0,screen_w(),screen_h()) ...
//       draw_nav(f.box[0]); draw_knobs(f.box[1]); draw_display(f.box[2]);
//       for (int i = 0; i < 16; i++) draw_step(face_col(&f, f.box[3], i, 2), i);  // ④ via the lane
//       draw_keys(f.box[4]);
//   }
// The five-zone model + per-step/band vocabulary: design/device-face-paradigm.md.

#ifndef FACE_H
#define FACE_H

#include "studio.h"
#include "lay.h"

extern void de_resize(int w, int h);   // engine seam (studio.c): set the active canvas — cart-drivable

#ifndef FACE_MAX_ZONES
#define FACE_MAX_ZONES 10
#endif

// zone kinds. BAND = a docked full-width strip; LANE = a docked strip that is PER-STEP
// (bind its columns to the shared register with face_col); HERO = the remainder.
enum { FACE_BAND, FACE_LANE, FACE_HERO };

typedef struct {
    int   kind;         // FACE_BAND · FACE_LANE · FACE_HERO
    int   edge;         // EDGE_TOP | EDGE_BOTTOM (BAND/LANE only — LEFT/RIGHT are refused: no side-rails)
    float frac;         // height as a fraction of the content area (BAND/LANE); ignored for HERO
    const char *name;   // for debugging / self-documentation
} FaceZone;

typedef struct {
    Box     box[FACE_MAX_ZONES];   // resolved rect per zone, in DECLARATION order (== f.box[i] for ZONES[i])
    int     n;
    Box     area;                  // the content area passed in
    LayLane lane;                  // the shared per-step register (spans the FULL content width)
} Face;

// face_resize — request the CHUNKY canvas (route 2 of canvas-density-spectrum.md): match
// the DEVICE ratio at the DESIGN's density, keep the fitting axis at the design value,
// extend the other to match the ratio (the reflow spreads the design in — no bars). The
// ratio is invariant under our own resize, so the target is a stable fixed point.
static inline void face_resize(void) {
    int cw = screen_w(), ch = screen_h();
    if (cw <= 0 || ch <= 0) return;
    float r = (float)cw / (float)ch;
    int tw, th;
    if (r >= 1.6f) { th = 100; tw = (int)(100.0f * r + 0.5f); }   // wide → keep height, extend width
    else           { tw = 160; th = (int)(160.0f / r + 0.5f); }   // tall → keep width, extend height
    if (tw != cw || th != ch) de_resize(tw, th);
}

// face_area — the content area = the whole screen inset by `chassis` px, then intersected
// with the safe rect (notch / home-bar; == whole canvas on desktop). Draw your chassis to
// the FULL screen so it bleeds to every edge; the controls stay inside this.
static inline Box face_area(int chassis) {
    Box panel = lay_inset(box(0, 0, screen_w(), screen_h()), (float)chassis);
    int sx, sy, sw, sh; safe_rect(&sx, &sy, &sw, &sh);
    float x0 = panel.x > sx ? panel.x : sx, y0 = panel.y > sy ? panel.y : sy;
    float x1 = (panel.x + panel.w) < (sx + sw) ? (panel.x + panel.w) : (float)(sx + sw);
    float y1 = (panel.y + panel.h) < (sy + sh) ? (panel.y + panel.h) : (float)(sy + sh);
    return box(x0, y0, x1 - x0, y1 - y0);
}

// face_layout — THE grammar. Carve the declared zones out of `area` and build the shared
// register. Zones are docked so DECLARATION order reads visually TOP→BOTTOM: top-edge
// bands from the top in order, bottom-edge bands from the bottom in REVERSE order (so the
// last-declared bottom band lands bottom-most), the HERO takes whatever is left.
// `lane_cols` sizes the full-width register every LANE/HERO per-step overlay shares.
static inline Face face_layout(Box area, FaceZone *z, int n, int lane_cols) {
    Face f;
    f.n = n < FACE_MAX_ZONES ? n : FACE_MAX_ZONES;
    f.area = area;
    f.lane = lay_lane(area, lane_cols);   // FULL width — the grammar never lets a band narrow it
    Box body = area;
    int hero = -1;
    // top-edge bands, declaration order (dock from the top down)
    for (int i = 0; i < f.n; i++) {
        if (z[i].kind == FACE_HERO) { hero = i; continue; }
        if (z[i].edge != EDGE_BOTTOM)
            f.box[i] = lay_split(body, EDGE_TOP, area.h * z[i].frac, &body);
    }
    // bottom-edge bands, REVERSE declaration order (dock from the bottom up)
    for (int i = f.n - 1; i >= 0; i--) {
        if (z[i].kind != FACE_HERO && z[i].edge == EDGE_BOTTOM)
            f.box[i] = lay_split(body, EDGE_BOTTOM, area.h * z[i].frac, &body);
    }
    if (hero >= 0) f.box[hero] = body;   // the remainder (absorbs the extra height)
    return f;
}

// face_col — column `i` of the shared register, occupying `band`'s vertical extent. Use it
// for BOTH a LANE zone AND the hero's per-step overlay, and they line up by construction.
// The register is anchored to the FULL content width (it takes only the band's y/h, never
// its x) — that's what guarantees alignment. So per-step content that must be INDENTED
// (e.g. a drum grid sitting after a left name-gutter) can't use face_col; drop to lay.h and
// build a LOCAL lane on the sub-box: `LayLane g = lay_lane(sub, cols);
// lay_lane_cell(g, band, i, gap)`. (Escape hatch — surfaced converting chipjam; scaffold,
// not straitjacket.)
static inline Box face_col(Face *f, Box band, int i, float gap) {
    return lay_lane_cell(f->lane, band, i, gap);
}

// face_zone — find a zone's rect by name (a convenience so draw code doesn't hardcode
// indices; returns a zero Box if not found). Cheap — a handful of strcmp per frame.
static inline Box face_zone(Face *f, const char *name, FaceZone *z) {
    for (int i = 0; i < f->n; i++) {
        const char *a = z[i].name, *b = name;
        while (*a && *a == *b) { a++; b++; }
        if (*a == *b) return f->box[i];
    }
    return box(0, 0, 0, 0);
}

#endif
