// lay.h — immediate-mode responsive layout, cart-land.
//
// Why this exists (design/responsive-layout.md + design/device-adaptive-layout.md):
// carts hand-compute the same rect arithmetic over and over — "a title bar off
// the top, a keybed off the bottom, the rest is the body", "N buttons in a row
// with a gap", "center a thing", "as many pads as fit". This header is the small
// set of CSS-flavoured helpers that replace that math. Engine deliberately does
// NOT own layout (decision-0006 lane); carts #include this instead.
//
// The whole vocabulary is rect-in / rect-out — a `Box` (float x,y,w,h) goes in,
// a `Box` comes out. There is no retained tree: you call these each frame, like
// every other draw call, so the layout re-solves live as its container changes
// (a resized window, a rotated device). That's what makes it "responsive" for
// free — feed the same helpers a different container and the UI reflows.
//
// Usage (positions relative to whatever container you pass — SCREEN_W/H, or any
// sub-rect):
//
//   #include "studio.h"
//   #include "lay.h"
//   void draw(void) {
//       Box screen = box(0, 0, SCREEN_W, SCREEN_H), body;
//       Box title = lay_split(screen, EDGE_TOP, 12, &body);   // dock a 12px title
//       boxfill(title, CLR_TRUE_BLUE);
//       for (int i = 0; i < 3; i++)                            // 3 equal buttons in a row
//           boxfill(lay_inset(lay_cell(body, 0, 3, i, 4), 1), CLR_DARK_GREY);
//   }
//
// The set: clamp · fluid · pad/inset · at (9-grid anchor) · split (dock a band
// off an edge) · cell (equal flex) · grid (fixed columns) · wrap (auto-fit
// columns) · aspect (contain a ratio). Worked prototypes: tools/carts/respond.c
// (drag a fake screen, watch it reflow), rackfit.c (finger-sized reflow),
// acidfit.c (progressive disclosure). When the engine's SCREEN_W/H graduate to a
// runtime viewport, nothing here changes — you just pass a different container.

#ifndef LAY_H
#define LAY_H

typedef struct { float x, y, w, h; } Box;
static inline Box box(float x, float y, float w, float h) { Box b = {x, y, w, h}; return b; }

// clamp(): keep a value within [lo, hi] (CSS clamp() workhorse).
static inline float lay_clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// fluid(): clamp(lo, pct% of container, hi) — a size that scales SMOOTHLY with
// the container (a margin, a font, a bar height) instead of snapping. CSS clamp+%.
static inline float lay_fluid(float pct, float container, float lo, float hi) { return lay_clamp(pct * container, lo, hi); }

// pad(): per-side inset — CSS padding (top, right, bottom, left). Per-side is
// what an ASYMMETRIC safe-area needs (a notch tops one edge, the home-bar the
// opposite). inset() is the uniform m,m,m,m case.
static inline Box lay_pad(Box c, float t, float r, float b, float l) { return box(c.x + l, c.y + t, c.w - l - r, c.h - t - b); }
static inline Box lay_inset(Box c, float m) { return lay_pad(c, m, m, m, m); }

// at(): pin a w×h box to one of the container's 9 grid spots, inset from the
// edges (CSS position:absolute + inset + env(safe-area-inset)). anchor ∈ L_TL..L_BR.
enum { L_TL, L_T, L_TR, L_L, L_C, L_R, L_BL, L_B, L_BR };
static inline Box lay_at(Box c, int anchor, float w, float h, float inset) {
    int col = anchor % 3, row = anchor / 3;
    float x = col == 0 ? c.x + inset : col == 1 ? c.x + (c.w - w) / 2 : c.x + c.w - w - inset;
    float y = row == 0 ? c.y + inset : row == 1 ? c.y + (c.h - h) / 2 : c.y + c.h - h - inset;
    return box(x, y, w, h);
}

// split(): dock a fixed-size BAND off one edge and return it; the REMAINDER is
// written to *rest (NULL to ignore). CSS flex fixed-basis + a flex:1 sibling —
// the app-chrome workhorse (title / tab / tool bars, keybed). Chain it: dock top,
// then bottom, then *rest is the body. `size` is fixed px OR a fraction (c.h*0.5f).
enum { EDGE_TOP, EDGE_BOTTOM, EDGE_LEFT, EDGE_RIGHT };
static inline Box lay_split(Box c, int edge, float size, Box *rest) {
    Box band = c, rem = c;
    switch (edge) {
        case EDGE_TOP:    band.h = size;                        rem.y += size; rem.h -= size; break;
        case EDGE_BOTTOM: band.y += c.h - size; band.h = size;                rem.h -= size; break;
        case EDGE_LEFT:   band.w = size;                        rem.x += size; rem.w -= size; break;
        case EDGE_RIGHT:  band.x += c.w - size; band.w = size;                rem.w -= size; break;
    }
    if (rest) *rest = rem;
    return band;
}

// split_gap(): split() but leave `gap` px of empty space BETWEEN the docked band and
// the remainder (CSS flex `gap`, for a single fixed-basis dock). split() returns the
// two pieces FLUSH — so a filled remainder (a hero panel, a viewport) touches whatever
// was carved off it, and you re-open breathing room by insetting one side afterward.
// This keeps that gap at the split site. The band keeps `size`; the gap comes out of
// the remainder, on the side facing the band. (Symmetric "pull off ALL neighbours" is
// just lay_inset(); a single-axis gap within a cell is lay_pad() — this is the between-
// two-splits case those can't express in one call.)
static inline Box lay_split_gap(Box c, int edge, float size, float gap, Box *rest) {
    Box band = lay_split(c, edge, size, rest);
    if (rest) switch (edge) {                 // shrink the remainder away from the band
        case EDGE_TOP:    rest->y += gap; rest->h -= gap; break;
        case EDGE_BOTTOM:                 rest->h -= gap; break;
        case EDGE_LEFT:   rest->x += gap; rest->w -= gap; break;
        case EDGE_RIGHT:                  rest->w -= gap; break;
    }
    return band;
}

// cell(): the i-th of n EQUAL flex children along dir (0=row, 1=column) with a
// gap (CSS flex:1 + gap). The reflow primitive — same children, flip dir at a
// breakpoint and they go row↔column.
static inline Box lay_cell(Box c, int dir, int n, int i, float gap) {
    if (n < 1) n = 1;
    if (dir == 0) { float cw = (c.w - gap * (n - 1)) / n; return box(c.x + i * (cw + gap), c.y, cw, c.h); }
    /* col */     { float ch = (c.h - gap * (n - 1)) / n; return box(c.x, c.y + i * (ch + gap), c.w, ch); }
}

// grid(): i-th of n items in a FIXED `cols` grid (CSS grid-template-columns:
// repeat(cols,1fr)). The hand-picked-topology twin of wrap() — a drum machine
// wants a clean 4-wide pad grid, not "as many as happen to fit".
static inline Box lay_grid(Box c, int cols, int n, int i, float gap) {
    if (n < 1) return box(c.x, c.y, 0, 0);   // empty list: rows would be 0 → ch = h/rows is inf/NaN geometry
    if (cols < 1) cols = 1;
    int rows = (n + cols - 1) / cols;
    float cw = (c.w - gap * (cols - 1)) / cols;
    float ch = (c.h - gap * (rows - 1)) / rows;
    return box(c.x + (i % cols) * (cw + gap), c.y + (i / cols) * (ch + gap), cw, ch);
}

// lane(): a shared COLUMN REGISTER — the fix for "two (or more) stacked strips whose
// columns MUST line up" (a step sequencer's roll above its 16-step grid; any per-step
// lane). Capture the x-span + column count ONCE, then place column i of that lane into
// ANY band with lay_lane_cell(): every band bound to the same Lane is column-aligned by
// construction, not by hand-matched maths. The column x-step is always w/n, so left
// edges align across bands even when each band uses a different gap — gap only shrinks
// the drawn width. (Graduates deviceface's g_lane_* / acidwide's grid_lane hack;
// design/responsive-first-device-face.md Layer 2. Keep per-step things on one lane and
// non-per-step controls in bands — never a side-rail that steals the lane's width.)
// (type is `LayLane`, not bare `Lane` — `Lane` is a name carts already use for their
// own domain structs, e.g. acidcandy; the lay_ prefix keeps the register collision-free.)
typedef struct { float x, w; int n; } LayLane;
static inline LayLane lay_lane(Box span, int n) { LayLane L = { span.x, span.w, n < 1 ? 1 : n }; return L; }
static inline Box     lay_lane_cell(LayLane L, Box band, int i, float gap) {
    float step = L.w / (float)L.n;
    return box(L.x + i * step, band.y, step - gap, band.h);
}

// wrap(): i-th of n items in an AUTO-FLOWING grid — as many ~minItem-wide columns
// as fit in c.w, wrapping to new rows (CSS repeat(auto-fit, minmax(minItem,1fr))).
// Unlike grid(), the column count responds to width on its own — feed it a
// finger-sized minItem and the column count IS the density.
static inline int lay_wrap_cols(Box c, int n, float minItem, float gap) {
    int cols = (int)((c.w + gap) / (minItem + gap));
    if (cols < 1) cols = 1; if (cols > n) cols = n; return cols;
}
static inline Box lay_wrap(Box c, int n, int i, float minItem, float gap) {
    if (n < 1) return box(c.x, c.y, 0, 0);   // empty list: cols would clamp to 0 → (n+cols-1)/cols is int div-by-zero (SIGFPE)
    int cols = lay_wrap_cols(c, n, minItem, gap);
    int rows = (n + cols - 1) / cols;
    float cw = (c.w - gap * (cols - 1)) / cols, ch = (c.h - gap * (rows - 1)) / rows;
    return box(c.x + (i % cols) * (cw + gap), c.y + (i / cols) * (ch + gap), cw, ch);
}

// aspect(): fit a `ratio` (w/h) box inside the container, centered — CSS
// aspect-ratio + object-fit:contain. The viewport/preview that letterboxes itself.
static inline Box lay_aspect(Box c, float ratio) {
    if (ratio <= 0) return box(c.x, c.y, c.w, c.h);   // degenerate ratio: w/ratio is inf/NaN → return the container unletterboxed
    float w = c.w, h = w / ratio;
    if (h > c.h) { h = c.h; w = h * ratio; }
    return box(c.x + (c.w - w) / 2, c.y + (c.h - h) / 2, w, h);
}

// ── Box → studio.h drawing sugar (requires studio.h included first) ──────────
static inline void boxfill(Box b, int color) { rectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, color); }
static inline void boxrect(Box b, int color) { rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, color); }
static inline bool binside(Box b, int x, int y) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

#endif // LAY_H
