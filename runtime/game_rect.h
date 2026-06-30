// game_rect — where the SCREEN_W×SCREEN_H canvas sits inside the window, and the ONE
// window↔canvas coordinate transform built on it. This is the chokepoint from
// docs/design/touch-controls.md "De-risking principle": every touch/mouse read and the
// present blit go through here, so responsive placement (Phase 2) is just "set game_rect to
// a different value" — the silent, library-wide coordinate rewrite is retired before the
// feature that needs it. PURE math (no raylib types), so a det-probe can test the round-trip
// off-device (tools/det-probes/placetest.c).
//
// Phase 1.5 pins game_rect to the full window (origin 0,0; scale = the native SCALE), an
// identity transform: gr_win_to_canvas reduces to wx/SCALE, byte-identical to the old code.
#ifndef GAME_RECT_H
#define GAME_RECT_H

typedef enum { PLACE_OVERLAY = 0, PLACE_DECK, PLACE_RAILS } PlaceMode;

// the canvas's on-screen origin (window px) + px-per-canvas-px. With {0,0,SCALE} the game
// fills its scaled size at the window's top-left, exactly as before placement existed.
typedef struct { float x, y, scale; } GameRect;

typedef struct {
    PlaceMode mode;
    GameRect  game;                              // where the canvas blits
    int       band_x, band_y, band_w, band_h;    // control band (window px); full window in overlay
} Placement;

// window pixel → canvas pixel (inverse of the blit). int truncation matches the old (int)(w/SCALE).
static inline int gr_win_to_canvas_x(GameRect gr, float wx) { return (int)((wx - gr.x) / gr.scale); }
static inline int gr_win_to_canvas_y(GameRect gr, float wy) { return (int)((wy - gr.y) / gr.scale); }

// canvas pixel → window pixel (the blit's forward map).
static inline float gr_canvas_to_win_x(GameRect gr, int cx) { return gr.x + cx * gr.scale; }
static inline float gr_canvas_to_win_y(GameRect gr, int cy) { return gr.y + cy * gr.scale; }

// a band narrower/shorter than this (window px) isn't worth stealing from the game → overlay.
// ~2× a 44px thumb target so a stick + a button row fit with margin.
#define GR_MIN_BAND 90

// the placement decision: given the window + canvas sizes, where does the game sit and where do
// the controls go? PURE — renders nothing, so it's deterministically testable over a size matrix.
//
//   fit SCREEN_W×SCREEN_H into the window at the largest INTEGER scale (crisp pixels) → leftover
//     leftover BELOW ≥ GR_MIN_BAND and taller than the sides → DECK  (game top, band below)
//     leftover on the SIDES ≥ GR_MIN_BAND                    → RAILS (game centred, side rails)
//     neither (matched aspect, game ≈ fills window)          → OVERLAY (corner, the fallback)
//
// On desktop the window IS the game size (SCREEN_W*SCALE), so scale = SCALE, leftover = 0 →
// OVERLAY with game at (0,0): an identity GameRect, byte-identical to no placement at all. The
// deck/rails branches only fire when the window aspect differs from the game (i.e. on a phone).
static inline Placement gr_place(int win_w, int win_h, int screen_w, int screen_h) {
    int sx = win_w / screen_w, sy = win_h / screen_h;     // largest integer scale that fits
    int s  = sx < sy ? sx : sy;
    if (s < 1) s = 1;
    int gw = screen_w * s, gh = screen_h * s;
    int extra_w = win_w - gw, extra_h = win_h - gh;       // leftover letterbox (window px)
    int cx = extra_w / 2, cy = extra_h / 2;               // pixel-aligned centre offset (floored)

    Placement p;
    p.game.scale = (float)s;
    if (extra_h >= extra_w && extra_h >= GR_MIN_BAND) {
        p.mode = PLACE_DECK;                              // game pinned to top, centred across
        p.game.x = (float)cx; p.game.y = 0.0f;
        p.band_x = 0; p.band_y = gh; p.band_w = win_w; p.band_h = extra_h;
    } else if (extra_w >= GR_MIN_BAND) {
        p.mode = PLACE_RAILS;                             // game centred, both side rails are the band
        p.game.x = (float)cx; p.game.y = (float)cy;
        p.band_x = 0; p.band_y = 0; p.band_w = win_w; p.band_h = win_h;
    } else {
        p.mode = PLACE_OVERLAY;                           // matched aspect → corner overlay (no band)
        p.game.x = (float)cx; p.game.y = (float)cy;
        p.band_x = 0; p.band_y = 0; p.band_w = win_w; p.band_h = win_h;
    }
    return p;
}

#endif
