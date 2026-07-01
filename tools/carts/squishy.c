/* de:meta
{
  "title": "Squishy Lines",
  "status": "active",
  "created": "2026-06-30",
  "kind": [
    "tool"
  ],
  "teaches": [
    "velocity-brush"
  ],
  "description": {
    "summary": "Draw with a velocity-sensitive ink brush — lines swell when you go slow, thin out when you go fast, and taper to a point at each end. Pick a tool, thickness, and bevel in the top panel.",
    "detail": "Every stroke is stored as DATA (a path of points + the speed you drew each one at), not painted straight to pixels. Most brushes render that path as a chain of overlapping round stamps whose width = a slow→fat / fast→thin speed curve × an end-taper × a little seeded wobble — ink / pencil / fineliner / marker / chalk are different sets of those numbers (chalk drops stamps for a dry, broken grain). Four brushes render specially: SKETCH (a hairy web of threads, à la Krita's Sketch engine), SPRAY (an airbrush dot-cloud whose spread follows speed), BRISTLE (raked parallel hairs), and PAINT (a wide wet brush whose paint runs DOWN in drips from every exposed bottom edge of the stroke — so a serpentine drips off each of its bands, not just the lowest; a run stops when it meets paint below, so inner bands make short runs into the gaps and only the open bottom edge falls long. Run length ∝ the band's thickness. Still a pure function of path+seed, no simulation). BEVEL embosses a stroke into a faux-3D rim (light from the top-left); BOIL brings a stroke alive in one of two styles (the boil button cycles off → wobble → pulse): WOBBLE = per-point hand-drawn jitter cycling ~7.5fps; PULSE = a subtle smooth grow/shrink breath about the stroke's centre. Almost free since rendering is a pure function of (stroke, seed). Bevel + boil are PER-STROKE — each stroke captures the toggle state when you draw it, so some strokes can be beveled or boiling and others still; the toolbar toggles set the default for NEW strokes (this is the groundwork for a select-and-edit tool). A SELECT toggle on the bar (or the S key) makes this a tiny NON-DESTRUCTIVE vector editor: click a stroke to pick it (accent box), and the bar's controls retarget to that stroke — recolour it, change its dither, toggle bevel/boil, drag its thickness — while a property strip adds bevel-SIZE + boil-INTENSITY sliders and bring-to-FRONT / send-to-BACK ordering. Edit any stroke any time; nothing is baked. A 7-colour pen — black/blue/red/green/cyan/magenta/yellow (cyan is a dark teal; pico32 has no true cyan) — picked from an always-visible palette strip (the seven swatches show their real colours; the active one wears an accent tab); each stroke keeps its colour. The fat brushes can be filled with a dpaint-style dither — a Bayer-ordered density ramp (~12→87% ink via fillp), likewise shown as an always-visible swatch strip — the step before a real flood-fill. The whole toolbar is ink-on-paper: white buttons, black glyphs; the brush dropdown opens a 2-column icon+name grid. A flood-fill tool + the pixelsnap animated-icon export come next.",
    "controls": "Top panel (ink-on-paper): a brush dropdown that opens a 2-column icon+name grid (ink/pencil/liner/marker/chalk/sketch/spray/bristle/paint), thickness slider, a BVL toggle, a BOIL cycle (off/wobble/pulse), UNDO, an always-visible dither-pattern strip, an always-visible 7-colour palette strip, and a SELECT (marquee) toggle — active swatches/toggles wear an accent tab. Drag to draw. Turn SELECT on (button or S key), click a stroke, then edit it via the bar + the bevel-size/boil-amt sliders and the FRONT/BACK ordering buttons. Keys: B bevel, O boil, S select, U undo, C clear."
  },
  "todo": [
    "Polish the ink/chalk tool-icon glyphs — still read a bit muddy at 16px (sprite-draw.js in squishy.cart.js). Sketch now reads since black is keyed out (colorkey).",
    "Cache finished strokes + the boil frames to layer buffers instead of re-rendering every stroke every frame.",
    "The pixelsnap animated-icon export: boil frames → pixelsnap → an animated sprite strip.",
    "spec(): same-seed determinism + jitter-bounds."
  ]
}
de:meta */
#include "studio.h"
// ink-on-paper UI theme: override ui.h's dark-navy defaults so every ui_button / ui_slider
// reads as black-on-white on the cream panel. MUST precede ui.h (its UI_COL_* are #ifndef-guarded),
// and it's per-cart, so no other cart is affected.
#define UI_COL_BG        CLR_WHITE           // button / slider-track background (paper)
#define UI_COL_FILL      CLR_LIGHT_PEACH     // slider filled portion
#define UI_COL_FILL_HOT  CLR_LIGHT_PEACH     // pressed / hot fill
#define UI_COL_FRAME     CLR_LIGHT_GREY      // 1px chip border
#define UI_COL_TEXT      CLR_BROWNISH_BLACK  // label ink (the "black line" in each button)
#define UI_COL_TEXT_HOT  CLR_BROWNISH_BLACK  // keep ink on hover (white would vanish on paper)
#define UI_COL_FOCUS     CLR_RED             // focus ring = accent
#include "ui.h"
#include <math.h>

// ── SQUISHY LINES ─────────────────────────────────────────────────────────
// A velocity-sensitive drawing tool. The soul is one idea: a stroke is DATA (a
// path of points, each carrying the speed you drew it at), and rendering is a
// PURE FUNCTION of (that data + a seed). Draw slow → the line swells; flick fast
// → it thins; lift off → it tapers. Seeded per-stamp wobble keeps it hand-inked.
//
// Each TOOL is just a different set of brush numbers (width range, speed
// sensitivity, wobble, taper). The BEVEL toggle embosses strokes into faux-3D.
// Because strokes are data (not baked pixels), the planned "boil" mode is almost
// free. See docs/design/squishy-lines.md for the full plan.

// 2-tone: warm cream paper, soft inky brown-black (the tinyjam 1-bit target).
#define PAPER  CLR_WHITE           // #fff1e8
#define INK    CLR_BROWNISH_BLACK  // #291814
#define ACCENT CLR_RED             // selection / on-state marker in the panel

// bevel tones — light from the top-LEFT (DPaint convention): light rim on the
// upper-left edge, dark rim on the lower-right.
#define HILITE    CLR_LIGHT_PEACH  // the lit (top-left) rim
#define SHADOW    CLR_BLACK        // the shaded (bottom-right) rim, darker than ink
#define BEVEL_OFF 1.0f             // default px the rims peek out past the ink body
#define BEVEL_MAX 4.0f             // max bevel rim size (the select tool's size slider)

// the pen palette — black/blue/red/green + (sort-of) cyan/magenta/yellow. pico32 has no true
// cyan, so cyan ≈ CLR_BLUE_GREEN (dark teal); swatch sprites 8..14 in the .cart.js mirror this.
#define NCOLORS 7
static const int COLORS[NCOLORS] = { INK, CLR_BLUE, CLR_RED, CLR_DARK_GREEN, CLR_BLUE_GREEN, CLR_PINK, CLR_YELLOW };
static int color_index(int c) { for (int i = 0; i < NCOLORS; i++) if (COLORS[i] == c) return i; return 0; }

// dither fill patterns for the fat stamp brushes (à la dpaint) — a Bayer-ordered
// DENSITY ramp: index 0 = solid (no fillp), then ~12 / 25 / 50 / 75 / 87% ink.
// Holes show PAPER → a screen-printed stroke. Swatch sprites 12.. in the .cart.js
// mirror this list exactly (PAT_MASKS), so the cycle button shows the real fill.
#define NPATTERNS 6
static const int PATTERNS[NPATTERNS] = { 0x0000, 0x7FDF, 0x5F5F, 0x5A5A, 0x0A0A, 0x0208 };

#define PANEL_H   24               // top tool-bar height; the canvas is below it
#define PROP_H    18               // contextual property strip (shown under the bar when editing a selection)

// boil: the opt-in living loop. Re-render the committed strokes through a few
// jittered variants and cycle them slowly — the classic hand-drawn wobble.
#define BOIL_FRAMES 3              // how many jittered variants the boil cycles
#define BOIL_PERIOD 8              // display frames each variant is held (~7.5fps)
#define BOIL_JIT    1.2f           // px of per-point wobble when boiling (WOBBLE style)
// boil STYLES — different ways a stroke can come alive:
enum { BOIL_WOBBLE, BOIL_BREATHE };
#define BREATHE_AMT   0.07f        // PULSE style: ±7% scale about the stroke's centre
#define BREATHE_SPEED 2.5f         // degrees of breath phase per frame (~2.4s per breath)

#define MAX_STROKES 128
#define MAX_SAMPLES 1500           // a long continuous stroke (spiral fill, contour) fits

#define MIN_SPACING   1.6f         // px between captured path points
#define STAMP_SPACING 0.7f         // px between stamps when rendering a segment (dense = solid)

// how a tool draws its stored path:
enum { K_STAMP, K_CHALK, K_SKETCH, K_SPRAY, K_BRISTLE, K_DRIP };

// a tool = a brush recipe. width swings between minw (full speed) and maxw
// (standstill); speedref = px/frame where width bottoms out; noise = per-stamp
// width wobble; taper = how many SAMPLES to taper at each end (a FIXED length,
// not a fraction of the stroke). For spray, maxw = the scatter-cloud radius; for
// bristle, maxw = the hair spread.
typedef struct { float minw, maxw, speedref, noise, taper; int kind; const char *name; } Brush;
static const Brush BRUSHES[] = {
    { 1.8f, 9.0f,  9.0f, 0.15f, 9.0f, K_STAMP,   "ink" },   // the squish — big swing, hand-inked
    { 1.0f, 2.6f,  6.0f, 0.35f, 5.0f, K_STAMP,   "pen" },   // pencil: thin, scratchy, grainy
    { 1.2f, 1.9f, 12.0f, 0.05f, 4.0f, K_STAMP,   "fin" },   // fineliner: thin, crisp, near-constant
    { 5.0f, 7.5f, 22.0f, 0.05f, 3.0f, K_STAMP,   "mrk" },   // marker: wide, even, flat
    { 2.5f, 5.0f, 10.0f, 0.55f, 4.0f, K_CHALK,   "chk" },   // chalk: dry, grainy, broken edges
    { 2.0f, 2.0f, 12.0f, 0.00f, 1.0f, K_SKETCH,  "skt" },   // sketch: a hairy web of threads
    { 3.0f,13.0f, 11.0f, 0.00f, 3.0f, K_SPRAY,   "spr" },   // airbrush: scattered dots (maxw = cloud)
    { 4.0f, 9.0f, 10.0f, 0.10f, 4.0f, K_BRISTLE, "brs" },   // bristle: raked parallel hairs
    { 6.0f,14.0f, 14.0f, 0.10f, 4.0f, K_DRIP,    "pnt" },   // paint: wide + wet, runs drip DOWN from the wettest spots
};
#define NTOOLS ((int)(sizeof(BRUSHES) / sizeof(BRUSHES[0])))   // brush icons are sprite slots 0..7
#define SKETCH_R     22.0f     // px: a thread links to prior points within this reach (à la Krita Sketch)
#define SKETCH_R2    (SKETCH_R * SKETCH_R)
#define SKETCH_TRIES 5         // random prior points tried per sample → web density
#define SPRAY_DOTS   7         // dots scattered per path point (airbrush)
#define BRISTLE_HAIRS 5        // parallel hairs across the bristle width
// paint drips: runs fall DOWN from every EXPOSED bottom edge of the wet paint (a
// painted cell with paper just below it) — so a serpentine stroke drips off each of
// its bands, not only the lowest. A run stops when it meets paint again below (merges
// into a lower band), so inner bands make short runs into the gaps and only the truly
// open bottom edge runs long. Run length ∝ the band's thickness there (more paint =
// longer). Pure function of (path, seed) — re-renders identically.
#define DRIP_STEP      4       // column stride between drip candidates (px)
#define DRIP_MIN_THK   6       // min band thickness (px) at a column before it can run
#define DRIP_CHANCE    0.5f    // fraction of candidate edges that actually run
#define DRIP_LEN_SCALE 1.8f    // run length per px of band thickness (before the random factor)
#define DRIP_MAX_LEN   140.0f  // cap so a free-falling run can't go forever

// readable names for the header label (BRUSHES[].name is the 3-char button id)
static const char *TOOL_DISP[] = { "ink", "pencil", "liner", "marker", "chalk", "sketch", "spray", "bristle", "paint" };

typedef struct { float x, y, speed; } Sample;
typedef struct {
    unsigned seed;          // per-stroke seed → deterministic wobble (boil reseeds this)
    int      tool;          // which brush this stroke was drawn with
    int      color;         // palette colour this stroke was drawn in
    int      pattern;       // dither pattern index (0 = solid) — stamp brushes only
    float    bevel;         // bevel rim size in px (0 = off) — per-stroke, captured/editable
    float    boil;          // boil intensity 0..1 (0 = still) — per-stroke, captured/editable
    int      boil_style;    // BOIL_WOBBLE (jitter) or BOIL_BREATHE (scale pulse)
    float    thick;         // thickness multiplier this stroke was drawn with
    int      n;
    Sample   pts[MAX_SAMPLES];
} Stroke;

static const unsigned VARIANT[BOIL_FRAMES] = { 0u, 0x9e3779b9u, 0x85ebca6bu };

static Stroke   strokes[MAX_STROKES];
static int      nstrokes = 0;
static Stroke   cur;                 // the stroke being drawn right now
static int      drawing = 0;
static int      bevel = 0;           // bevel DEFAULT for new strokes (each stroke captures it)
static int      boil = 0;            // boil DEFAULT for new strokes (captured as intensity 0/1)
static int      boil_style = BOIL_WOBBLE;   // boil style DEFAULT for new strokes
static int      cursor_panel = -1;   // tracks which cursor is shown (hand on bar / ring on canvas)
static int      tool = 0;            // selected brush
static int      selmode = 0;         // SELECT mode (a top-bar toggle): clicks pick/edit, not draw
static int      selected = -1;       // index of the selected stroke (-1 = none)
static int      colsel = 0;          // selected pen colour (index into COLORS)
static int      patsel = 0;          // selected dither pattern (index into PATTERNS; 0 = solid)
static int      dd_open = 0;         // tool dropdown open?
static float    thick01 = 0.375f;    // thickness slider (0..1) → ~1.0× by default
static float    prevx, prevy;        // last frame's pointer (for per-frame speed)
static float    lastsx, lastsy;      // last *captured sample* (for min-spacing)
static float    ema = 0;             // smoothed pointer speed
static unsigned seedctr = 0x1234567u;

static float thickness(void) { return 0.4f + thick01 * 1.6f; }   // 0.4×..2.0×

// integer hash → [0,1); deterministic, no global rnd() state (boil-friendly).
static unsigned hashu(unsigned x){ x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x; }
static float    hashf(unsigned x){ return (hashu(x) & 0xFFFFFF) / (float)0x1000000; }

// width of the brush at sample i — the pure function: speed curve × taper × wobble.
static float sample_width(const Stroke *s, int i, unsigned fseed) {
    Brush b = BRUSHES[s->tool];
    float maxw = b.maxw * s->thick, minw = b.minw * s->thick;
    int n = s->n;
    // taper a FIXED number of samples (b.taper) at each end — distance from the
    // nearer endpoint, NOT a fraction of length, so the start stays put as the
    // line grows. (A single-point dab keeps full width.)
    float taper = 1.0f;
    if (n > 1) {
        float ts = fminf((float)i, (float)(n - 1 - i)) / b.taper;
        if (ts > 1) ts = 1; if (ts < 0) ts = 0;
        taper = ts * ts * (3 - 2 * ts);                       // smoothstep the ends
    }
    float sp = s->pts[i].speed / b.speedref;
    if (sp > 1) sp = 1; if (sp < 0) sp = 0;
    float wspeed = maxw - (maxw - minw) * sp;                 // slow = fat, fast = thin
    float wobble = 1.0f + (hashf(s->seed ^ fseed ^ (unsigned)(i * 2654435761u)) * 2 - 1) * b.noise;
    return wspeed * taper * wobble;
}

static void stamp(float x, float y, float w, int color) {
    if (w <= 0) return;
    int ix = (int)(x + 0.5f), iy = (int)(y + 0.5f);
    float r = w * 0.5f;
    if (r < 0.75f) pset(ix, iy, color);
    else circfill(ix, iy, (int)(r + 0.5f), color);
}

// per-point boil wobble — a small offset keyed by (stroke seed, point, frame
// seed). Coherent per sample (the whole point shifts), so the line wiggles
// rather than fizzing. Zero unless `jitter` is set (boil on).
static float boil_off(unsigned seed, int i, unsigned fseed, unsigned salt) {
    return (hashf(seed ^ fseed ^ (unsigned)(i * 2246822519u) ^ salt) * 2 - 1) * BOIL_JIT;
}

// the live boil state for one stroke this frame: amount (0 = still), style, the
// frame-seed (drives WOBBLE jitter + the width wobble), and for BREATHE the
// stroke centre + breath phase. draw() fills this per stroke; the renderers call
// boil_pt() on each point.
typedef struct { float amt; int style; unsigned fseed; float cx, cy, phase; } Boil;

static void boil_pt(const Stroke *s, int i, float *x, float *y, const Boil *b) {
    if (b->amt <= 0) return;
    if (b->style == BOIL_BREATHE) {                       // subtle grow/shrink about the centre
        float sc = 1.0f + sin_deg(b->phase) * BREATHE_AMT * b->amt;
        *x = b->cx + (*x - b->cx) * sc;
        *y = b->cy + (*y - b->cy) * sc;
    } else {                                              // WOBBLE: per-point jitter
        *x += boil_off(s->seed, i, b->fseed, 0x11) * b->amt;
        *y += boil_off(s->seed, i, b->fseed, 0x22) * b->amt;
    }
}

// render a stroke as a chain of overlapping round stamps, offset by (ox,oy) in
// `color` — (ox,oy) is the bevel rim offset; `jitter` adds the boil wobble.
static void render_stroke(const Stroke *s, float ox, float oy, int color, const Boil *b) {
    if (s->n == 0) return;
    int chalk = BRUSHES[s->tool].kind == K_CHALK;   // dry, broken: drop ~40% of stamps
    if (s->n == 1) {
        float x = s->pts[0].x, y = s->pts[0].y; boil_pt(s, 0, &x, &y, b);
        stamp(x + ox, y + oy, sample_width(s, 0, b->fseed), color);
        return;
    }
    for (int i = 0; i < s->n - 1; i++) {
        float x0 = s->pts[i].x,   y0 = s->pts[i].y;
        float x1 = s->pts[i+1].x, y1 = s->pts[i+1].y;
        boil_pt(s, i, &x0, &y0, b); boil_pt(s, i + 1, &x1, &y1, b);
        float w0 = sample_width(s, i, b->fseed), w1 = sample_width(s, i + 1, b->fseed);
        float dx = x1 - x0, dy = y1 - y0;
        float seg = sqrtf(dx * dx + dy * dy);
        int steps = (int)(seg / STAMP_SPACING);
        if (steps < 1) steps = 1;
        for (int k = 0; k <= steps; k++) {
            if (chalk && hashf(s->seed ^ (unsigned)(i * 131u + k * 977u)) < 0.4f) continue;  // dry gaps
            float u = (float)k / steps;
            stamp(x0 + dx * u + ox, y0 + dy * u + oy, w0 + (w1 - w0) * u, color);
        }
    }
}

// airbrush: scatter SPRAY_DOTS dots per path point inside a cloud whose radius
// follows the speed curve (slow = wide, fast = tight). Density builds on overlap.
static void render_spray(const Stroke *s, const Boil *b) {
    for (int i = 0; i < s->n; i++) {
        float x = s->pts[i].x, y = s->pts[i].y; boil_pt(s, i, &x, &y, b);
        float rad = sample_width(s, i, b->fseed) * 0.5f + 1.0f;
        for (int k = 0; k < SPRAY_DOTS; k++) {
            unsigned h = hashu(s->seed ^ (unsigned)(i * 2654435761u) ^ (unsigned)(k * 0x9E3779B1u));
            float a = (h & 0xFFFF) / 65535.0f * 360.0f;               // angle, degrees
            float d = sqrtf(((h >> 16) & 0xFFFF) / 65535.0f) * rad;   // uniform over the disk
            pset((int)(x + cos_deg(a) * d + .5f), (int)(y + sin_deg(a) * d + .5f), s->color);
        }
    }
}

// bristle: BRISTLE_HAIRS thin lines raked across the brush width (perpendicular
// to the stroke), each nudged a little so they're not mechanically parallel.
static void render_bristle(const Stroke *s, const Boil *b) {
    if (s->n < 2) return;
    for (int i = 0; i < s->n - 1; i++) {
        float x0 = s->pts[i].x, y0 = s->pts[i].y, x1 = s->pts[i+1].x, y1 = s->pts[i+1].y;
        boil_pt(s, i, &x0, &y0, b); boil_pt(s, i + 1, &x1, &y1, b);
        float dx = x1 - x0, dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.01f) continue;
        float px = -dy / len, py = dx / len;     // perpendicular unit
        float w = (sample_width(s, i, b->fseed) + sample_width(s, i + 1, b->fseed)) * 0.5f;
        for (int h = 0; h < BRISTLE_HAIRS; h++) {
            unsigned hh = hashu(s->seed ^ (unsigned)(h * 2654435761u));
            float j = ((hh & 0xFFFF) / 65535.0f - 0.5f) * 0.35f;
            float off = ((h + 0.5f) / BRISTLE_HAIRS - 0.5f + j) * w;
            line((int)(x0 + px * off + .5f), (int)(y0 + py * off + .5f),
                 (int)(x1 + px * off + .5f), (int)(y1 + py * off + .5f), s->color);
        }
    }
}

// the SKETCH brush (à la Krita's Sketch engine): a thin spine plus a hairy WEB —
// each point threads a 1px line to a few seeded-random prior points within reach.
// The web topology is keyed by the stroke seed ONLY (stable), while boil jitters
// the point positions (fseed) — so the whole tangle wiggles without re-wiring.
static void render_sketch(const Stroke *s, const Boil *b) {
    if (s->n < 2) return;
    for (int i = 1; i < s->n; i++) {
        float xi = s->pts[i].x, yi = s->pts[i].y;
        float xp = s->pts[i-1].x, yp = s->pts[i-1].y;
        boil_pt(s, i, &xi, &yi, b); boil_pt(s, i - 1, &xp, &yp, b);
        line((int)(xi + .5f), (int)(yi + .5f), (int)(xp + .5f), (int)(yp + .5f), s->color);  // spine
        for (int k = 0; k < SKETCH_TRIES; k++) {
            unsigned h = hashu(s->seed ^ (unsigned)(i * 2654435761u) ^ (unsigned)(k * 0x9E3779B1u));
            int j = (int)(h % (unsigned)i);                 // a prior point (topology = seed only)
            float xj = s->pts[j].x, yj = s->pts[j].y; boil_pt(s, j, &xj, &yj, b);
            float dx = xi - xj, dy = yi - yj;
            if (dx * dx + dy * dy < SKETCH_R2)
                line((int)(xi + .5f), (int)(yi + .5f), (int)(xj + .5f), (int)(yj + .5f), s->color);
        }
    }
}

// per-call coverage grid for the paint body — used to find bottom edges + stop runs.
static unsigned char drip_cov[SCREEN_H][SCREEN_W];

// draw ONE run down from a band bottom edge at (x, yedge). `y1` = lowest painted row of
// this stroke; below it there's no paint so a run there falls free. Stops on paint below.
static void emit_drip(int x, int yedge, int thk, int y1, const Stroke *s) {
    if (thk < DRIP_MIN_THK) return;
    unsigned h = hashu(s->seed ^ (unsigned)(x * 2654435761u) ^ (unsigned)(yedge * 40503u));
    if (hashf(h) > DRIP_CHANCE) return;                       // not every edge runs
    // long-tailed length: u*u biases toward short stubs with the odd long runner,
    // so drips don't all bottom out at the same depth.
    float u = hashf(h ^ 0x55u);
    float len = thk * DRIP_LEN_SCALE * (0.15f + u * u * 2.4f);
    if (len > DRIP_MAX_LEN) len = DRIP_MAX_LEN;
    // each run gets its own gentle sideways drift + wobble so they're not ruler-straight
    // parallels — some lean left, some right, some run true.
    float lean = (hashf(h ^ 0xA3u) * 2 - 1) * 0.06f;          // px of drift per row (±)
    float wob  = 0.5f + hashf(h ^ 0x7Bu) * 0.8f;              // wobble amplitude
    float ph   = (float)(h % 360u);
    int d, xx = x;
    for (d = 1; d < (int)len; d++) {
        int yy = yedge + d;
        if (yy >= SCREEN_H) break;
        xx = (int)(x + lean * d + sin_deg(d * 6.0f + ph) * wob + 0.5f);
        if (xx < 0 || xx >= SCREEN_W) break;
        if (yy <= y1 && drip_cov[yy][xx]) break;              // met a lower band → merge/stop
        float t = d / len;
        stamp((float)xx, (float)yy, 2.0f * (1 - t) + 0.6f, s->color);   // thin tapering streak
    }
    stamp((float)xx, (float)(yedge + d), 2.2f, s->color);     // the heavy bead where it ends
    if ((h & 7u) == 0) pset(xx, yedge + d + 2 + (int)(h % 3u), s->color);   // occasional spatter dot
}

// PAINT: a wide wet body, then runs that drip DOWN from every exposed bottom edge.
// Rasterise the body into drip_cov, then per column find each paint→paper transition
// (a band bottom) and run a drip from it. Pure function of (path, seed) — same soul as
// boil, no simulation, fully re-renderable.
static void render_drip(const Stroke *s, const Boil *b) {
    render_stroke(s, 0, 0, s->color, b);   // the wet body (same stamp chain as the fat brushes)
    if (s->n < 1) return;

    // painted bounding box (points ± their radius), clamped to screen
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = 0; i < s->n; i++) {
        float x = s->pts[i].x, y = s->pts[i].y; boil_pt(s, i, &x, &y, b);
        float r = sample_width(s, i, b->fseed) * 0.5f;
        if (x - r < minx) minx = x - r; if (x + r > maxx) maxx = x + r;
        if (y - r < miny) miny = y - r; if (y + r > maxy) maxy = y + r;
    }
    int x0 = (int)minx, x1 = (int)maxx, y0 = (int)miny, y1 = (int)maxy;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W - 1) x1 = SCREEN_W - 1; if (y1 > SCREEN_H - 1) y1 = SCREEN_H - 1;
    if (x1 < x0 || y1 < y0) return;

    // rasterise the wet body's coverage (a disk per sample) into the bbox
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) drip_cov[y][x] = 0;
    for (int i = 0; i < s->n; i++) {
        float cx = s->pts[i].x, cy = s->pts[i].y; boil_pt(s, i, &cx, &cy, b);
        float r = sample_width(s, i, b->fseed) * 0.5f; if (r < 0.5f) r = 0.5f;
        for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
            if (x < x0 || x > x1) continue;
            float dx = x - cx, hh = r * r - dx * dx; if (hh < 0) continue; hh = sqrtf(hh);
            int ya = (int)(cy - hh), yb = (int)(cy + hh);
            if (ya < y0) ya = y0; if (yb > y1) yb = y1;
            for (int y = ya; y <= yb; y++) drip_cov[y][x] = 1;
        }
    }

    // per column: every paint→paper transition is a band bottom that can drip
    for (int x = x0; x <= x1; x += DRIP_STEP) {
        int run = 0;
        for (int y = y0; y <= y1; y++) {
            if (drip_cov[y][x]) { run++; continue; }
            if (run) emit_drip(x, y - 1, run, y1, s);   // y-1 is a band bottom exposed to paper
            run = 0;
        }
        if (run) emit_drip(x, y1, run, y1, s);           // a band reaching the bbox floor also runs
    }
}

// draw one stroke. Sketch is its own renderer; the other brushes use the stamp
// chain, with the bevel emboss if it's on: a SHADOW + HILITE copy offset under
// the ink body leave a light rim on the upper-left, a dark rim on the lower-right.
// Pure geometry, no canvas read-back → deterministic. `jitter` propagates the boil
// wobble to all passes so the rim moves with the body.
static void draw_one(const Stroke *s, const Boil *b) {
    switch (BRUSHES[s->tool].kind) {
        case K_SKETCH:  render_sketch(s, b);  return;
        case K_SPRAY:   render_spray(s, b);   return;
        case K_BRISTLE: render_bristle(s, b); return;
        case K_DRIP:    render_drip(s, b);    return;
        default: break;   // K_STAMP / K_CHALK use the stamp chain below
    }
    if (s->bevel > 0) {   // per-stroke bevel, s->bevel = rim size in px
        render_stroke(s,  s->bevel,  s->bevel, SHADOW, b);   // rims stay solid
        render_stroke(s, -s->bevel, -s->bevel, HILITE, b);
    }
    if (s->pattern) fillp(PATTERNS[s->pattern], PAPER);   // dpaint-style dither fills the body only
    render_stroke(s, 0, 0, s->color, b);
    if (s->pattern) fillp_reset();
}

// build this stroke's live boil state for the current frame
static Boil boil_for(const Stroke *s, unsigned vseed) {
    Boil b = { s->boil, s->boil_style, s->seed, 0, 0, 0 };
    if (b.amt <= 0) return b;
    if (b.style == BOIL_WOBBLE) {
        b.fseed = s->seed ^ vseed;                 // jitter + width cycle per beat
    } else {                                       // BREATHE: stable width, smooth scale pulse
        float sx = 0, sy = 0;
        for (int k = 0; k < s->n; k++) { sx += s->pts[k].x; sy += s->pts[k].y; }
        b.cx = sx / s->n; b.cy = sy / s->n;
        b.phase = frame() * BREATHE_SPEED + (float)(s->seed % 360u);   // per-stroke offset
    }
    return b;
}

static void do_undo(void) { if (nstrokes > 0) nstrokes--; if (selected >= nstrokes) selected = -1; }

// z-order: strokes draw in array order (later = on top). Move the selected stroke
// to the front (top) or back (bottom) and keep `selected` pointing at it.
static void to_front(void) {
    if (selected < 0 || selected >= nstrokes - 1) return;
    Stroke t = strokes[selected];
    for (int i = selected; i < nstrokes - 1; i++) strokes[i] = strokes[i + 1];
    strokes[nstrokes - 1] = t; selected = nstrokes - 1;
}
static void to_back(void) {
    if (selected <= 0 || selected >= nstrokes) return;
    Stroke t = strokes[selected];
    for (int i = selected; i > 0; i--) strokes[i] = strokes[i - 1];
    strokes[0] = t; selected = 0;
}

// distance² from point (px,py) to segment a-b
static float seg_dist2(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    float t = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0;
    if (t < 0) t = 0; if (t > 1) t = 1;
    float ex = px - (ax + t * dx), ey = py - (ay + t * dy);
    return ex * ex + ey * ey;
}

// nearest stroke to (px,py) within a pick threshold (its half-width + slack) → index, or -1
static int pick_stroke(float px, float py) {
    int best = -1; float bestd = 1e9f;
    for (int s = 0; s < nstrokes; s++) {
        const Stroke *st = &strokes[s];
        float pad = st->thick * BRUSHES[st->tool].maxw * 0.5f + 6.0f;
        float mind = 1e9f;
        if (st->n == 1) { float dx = px - st->pts[0].x, dy = py - st->pts[0].y; mind = dx * dx + dy * dy; }
        for (int i = 0; i < st->n - 1; i++) {
            float d = seg_dist2(px, py, st->pts[i].x, st->pts[i].y, st->pts[i+1].x, st->pts[i+1].y);
            if (d < mind) mind = d;
        }
        if (mind < pad * pad && mind < bestd) { bestd = mind; best = s; }
    }
    return best;
}

#define DD_X    2
#define DD_W    22             // dropdown HEADER width (the collapsed icon button in the bar)
#define DD_ITEM 20             // dropdown row height
// the open list is a 2-column grid of icon+name cells — rows grow as tools are added,
// so there's room to spare for the next batch of drawing tools.
#define DD_COLS    2
#define DD_ITEM_W  64                                   // one cell: icon (16) + name
#define DD_ROWS    ((NTOOLS + DD_COLS - 1) / DD_COLS)   // ceil(NTOOLS / cols)
#define DD_PANEL_W (DD_COLS * DD_ITEM_W)
#define DD_PANEL_H (DD_ROWS * DD_ITEM)

// ink-on-paper tool buttons: paper bg so dark glyphs read; red frame = selected.
// fields: { bg, bg_sel, frame, frame_hot, frame_sel, halo_sel }
static const UiSprStyle TOOLBTN = { PAPER, CLR_LIGHT_PEACH, CLR_LIGHT_GREY, INK, ACCENT, -1 };

// a dashed 1px rectangle (marquee look) — dots every 2px round the perimeter
static void dashed_rect(int x, int y, int w, int h, int col) {
    for (int i = 0; i < w; i += 2) { pset(x + i, y, col); pset(x + i, y + h - 1, col); }
    for (int i = 0; i < h; i += 2) { pset(x, y + i, col); pset(x + w - 1, y + i, col); }
}

// the top tool-bar. When the SELECT tool has a stroke picked, the value controls
// EDIT that stroke (and reflect its values) instead of setting new-stroke defaults,
// and a contextual property strip (bevel size / boil intensity) drops under the bar.
static void draw_panel(void) {
    int editing = (selmode && selected >= 0 && selected < nstrokes);
    Stroke *sel = editing ? &strokes[selected] : 0;

    rectfill(0, 0, SCREEN_W, PANEL_H, PAPER);
    line(0, PANEL_H - 1, SCREEN_W, PANEL_H - 1, INK);
    ui_begin();
    // brush dropdown header
    if (ui_spr_button_styled(tool, DD_X, 2, DD_W, 20, dd_open, TOOLBTN)) dd_open = !dd_open;
    font(FONT_SMALL); print(editing ? "edit" : TOOL_DISP[tool], 26, 9, INK);   // small font for the whole bar

    // thickness — edits the selection's thickness, else the new-stroke default
    if (sel) {
        static float t; t = (sel->thick - 0.4f) / 1.6f;
        if (ui_slider(&t, 56, 4, 38, "thk")) sel->thick = 0.4f + t * 1.6f;
    } else ui_slider(&thick01, 56, 4, 38, "thk");
    rect(56, 5, 38, 6, CLR_LIGHT_GREY);   // outline the track (white-on-white otherwise)

    // bevel toggle
    int bev_on = sel ? (sel->bevel > 0) : bevel;
    if (ui_button(96, 3, 16, 16, "bvl")) { if (sel) sel->bevel = sel->bevel > 0 ? 0 : BEVEL_OFF; else bevel = !bevel; }
    if (bev_on) rectfill(96, 19, 16, 2, ACCENT);

    // boil — cycles off → wobble → pulse (the style); the label shows which
    int bstate = sel ? (sel->boil <= 0 ? 0 : sel->boil_style + 1)
                     : (boil == 0 ? 0 : boil_style + 1);
    if (ui_button(114, 3, 22, 16, bstate == 0 ? "boil" : bstate == 1 ? "wobl" : "puls")) {
        int ns = (bstate + 1) % 3;
        if (sel) { if (ns == 0) sel->boil = 0; else { if (sel->boil <= 0) sel->boil = 1.0f; sel->boil_style = ns - 1; } }
        else     { if (ns == 0) boil = 0;      else { boil = 1; boil_style = ns - 1; } }
    }
    if (bstate) rectfill(114, 19, 22, 2, ACCENT);

    if (ui_button(138, 3, 22, 16, "undo")) do_undo();

    // ---- colour palette: all 7 swatches visible; the active one wears an ACCENT tab
    // (matches the bvl/boil on-state bar). Edits the selection's colour, else the default.
    int ci = sel ? color_index(sel->color) : colsel;
    for (int i = 0; i < NCOLORS; i++) {
        int x = 166 + i * 10;
        if (ui_button(x, 3, 10, 16, 0)) { if (sel) sel->color = COLORS[i]; else colsel = i; }
        rectfill(x, 3, 10, 16, COLORS[i]);   // overdraw the button with the real colour
        rect(x, 3, 10, 16, INK);
        if (i == ci) rectfill(x, 19, 10, 2, ACCENT);
    }

    // ---- dither patterns: all 6 visible, each showing its real fill; active wears the tab
    int pat = sel ? sel->pattern : patsel;
    for (int i = 0; i < NPATTERNS; i++) {
        int x = 240 + i * 9;
        if (ui_button(x, 3, 9, 16, 0)) { if (sel) sel->pattern = i; else patsel = i; }
        fillp(PATTERNS[i], PAPER); rectfill(x, 3, 9, 16, INK); fillp_reset();
        rect(x, 3, 9, 16, INK);
        if (i == pat) rectfill(x, 19, 9, 2, ACCENT);
    }

    // ---- SELECT toggle: a white marquee box on a black button (turns ACCENT when on)
    if (ui_button(298, 3, 18, 16, 0)) selmode = !selmode;
    rectfill(298, 3, 18, 16, INK);
    dashed_rect(301, 6, 12, 10, selmode ? ACCENT : PAPER);
    if (selmode) rectfill(298, 19, 18, 2, ACCENT);

    // contextual property strip: bevel SIZE + boil INTENSITY sliders + z-order, for the selection
    if (editing) {
        rectfill(0, PANEL_H, SCREEN_W, PROP_H, PAPER);
        line(0, PANEL_H + PROP_H - 1, SCREEN_W, PANEL_H + PROP_H - 1, INK);
        static float bs; bs = sel->bevel / BEVEL_MAX; if (bs > 1) bs = 1;
        if (ui_slider(&bs, 6, PANEL_H + 3, 92, "bevel")) sel->bevel = bs * BEVEL_MAX;
        rect(6, PANEL_H + 4, 92, 6, CLR_LIGHT_GREY);
        static float bo; bo = sel->boil;
        if (ui_slider(&bo, 104, PANEL_H + 3, 92, "boil")) sel->boil = bo;
        rect(104, PANEL_H + 4, 92, 6, CLR_LIGHT_GREY);
        if (ui_button(202, PANEL_H + 2, 34, 14, "back"))  to_back();
        if (ui_button(240, PANEL_H + 2, 44, 14, "front")) to_front();
    }

    // the open dropdown list: a 2-column grid of brush icon + name cells over the canvas
    if (dd_open) {
        rectfill(DD_X, PANEL_H, DD_PANEL_W, DD_PANEL_H, PAPER);
        rect(DD_X, PANEL_H, DD_PANEL_W, DD_PANEL_H, INK);
        for (int i = 0; i < NTOOLS; i++) {
            int col = i % DD_COLS, row = i / DD_COLS;
            int x = DD_X + col * DD_ITEM_W, y = PANEL_H + row * DD_ITEM;
            int on = (i == tool);
            if (ui_button(x, y, DD_ITEM_W, DD_ITEM, 0)) { tool = i; dd_open = 0; selmode = 0; }  // picking exits select mode
            rectfill(x + 1, y + 1, DD_ITEM_W - 2, DD_ITEM - 2, on ? CLR_LIGHT_PEACH : PAPER);
            if (on) rect(x + 1, y + 1, DD_ITEM_W - 2, DD_ITEM - 2, ACCENT);
            spr(i, x + 2, y + (DD_ITEM - 16) / 2);                    // brush icon (sprite slot i)
            print(TOOL_DISP[i], x + 20, y + (DD_ITEM - 6) / 2, INK);  // its name alongside
        }
    }
    font(FONT_NORMAL);   // restore (the whole bar drew in FONT_SMALL)
    ui_end();
}

void init(void) {
    mouse_hide();               // we draw our own brush-size ring as the cursor
    colorkey(CLR_BLACK);        // icon sprites use black (idx 0) as their bg — key it out so the
                                // ink-on-paper glyphs read on the paper/peach cell (esp. sketch)
}

void update(void) {
    float mx = mouse_x(), my = mouse_y();

    // per-frame pointer speed, EMA-smoothed so one jittery frame can't spike width
    float fdx = mx - prevx, fdy = my - prevy;
    float fspeed = sqrtf(fdx * fdx + fdy * fdy);
    ema += (fspeed - ema) * 0.4f;
    prevx = mx; prevy = my;

    // while the tool dropdown is open it's modal: clicks pick a tool (resolved in
    // the panel) and a tap on the bare canvas just dismisses it — never draws.
    if (dd_open) {
        if (mouse_pressed(MOUSE_LEFT)) {
            int right = DD_X + DD_PANEL_W, bottom = PANEL_H + DD_PANEL_H;
            int in_list = mx >= DD_X && mx < right && my >= PANEL_H && my < bottom;
            if (my >= PANEL_H && !in_list) dd_open = 0;     // tap-away dismiss
        }
        return;                                             // no drawing while open
    }

    if (keyp('S')) selmode = !selmode;   // S toggles SELECT mode (also a bar button)

    // SELECT mode: a canvas press picks the nearest stroke (or deselects on empty), never draws.
    // While editing, clicks in the property strip drive its sliders — not a re-pick.
    if (selmode) {
        int in_prop = (selected >= 0 && selected < nstrokes) && my < PANEL_H + PROP_H;
        if (mouse_pressed(MOUSE_LEFT) && my >= PANEL_H && !in_prop) selected = pick_stroke(mx, my);
        if (keyp('U') || mouse_pressed(MOUSE_RIGHT)) do_undo();
        if (keyp('C')) { nstrokes = 0; selected = -1; }
        return;
    }

    // a press that lands in the panel drives the panel, never the canvas
    if (mouse_pressed(MOUSE_LEFT) && my >= PANEL_H) {
        drawing = 1;
        cur.n = 0;
        cur.tool = tool;
        cur.color = COLORS[colsel];
        cur.pattern = patsel;
        cur.bevel = bevel ? BEVEL_OFF : 0.0f;   // capture the current bevel/boil defaults
        cur.boil = boil ? 1.0f : 0.0f;
        cur.boil_style = boil_style;
        cur.thick = thickness();
        seedctr = seedctr * 1103515245u + 12345u;
        cur.seed = seedctr;
        lastsx = mx; lastsy = my;
        cur.pts[cur.n++] = (Sample){ mx, my, ema };
    }
    if (drawing && mouse_down(MOUSE_LEFT)) {
        float dsx = mx - lastsx, dsy = my - lastsy;
        if (sqrtf(dsx * dsx + dsy * dsy) >= MIN_SPACING && cur.n < MAX_SAMPLES) {
            cur.pts[cur.n++] = (Sample){ mx, my, ema };
            lastsx = mx; lastsy = my;
        }
    }
    if (drawing && mouse_released(MOUSE_LEFT)) {
        drawing = 0;
        if (cur.n > 0 && nstrokes < MAX_STROKES) strokes[nstrokes++] = cur;
        cur.n = 0;
    }

    // NB: raylib letter keycodes are UPPERCASE — keyp('u') (lowercase, 117) never fires.
    if (keyp('U') || mouse_pressed(MOUSE_RIGHT)) do_undo();
    if (keyp('C')) { nstrokes = 0; selected = -1; }
    if (keyp('B')) bevel = !bevel;
    if (keyp('O')) boil = !boil;
}

void draw(void) {
    cls(PAPER);
    // boil: pick this beat's jittered variant; committed strokes wobble, the
    // stroke you're actively drawing stays put.
    // boil is now PER-STROKE: each stroke animates by its own intensity. The variant
    // (which jitter this beat) is global timing; a still stroke (boil 0) uses a stable seed.
    unsigned vseed = VARIANT[(frame() / BOIL_PERIOD) % BOIL_FRAMES];
    for (int i = 0; i < nstrokes; i++) {
        Boil b = boil_for(&strokes[i], vseed);
        draw_one(&strokes[i], &b);
    }
    if (drawing) { Boil nb = { 0, 0, cur.seed, 0, 0, 0 }; draw_one(&cur, &nb); }   // active stroke stays still

    // cursor: an OS hand over the tool-bar (so it's clearly clickable), our own
    // brush-size ring over the canvas. Only toggle on the transition (no flicker).
    // selection highlight (only in SELECT mode): an accent bounding box around the picked stroke
    if (selmode && selected >= 0 && selected < nstrokes) {
        const Stroke *st = &strokes[selected];
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (int i = 0; i < st->n; i++) {
            float x = st->pts[i].x, y = st->pts[i].y;
            if (x < minx) minx = x; if (y < miny) miny = y;
            if (x > maxx) maxx = x; if (y > maxy) maxy = y;
        }
        int pad = (int)(st->thick * BRUSHES[st->tool].maxw * 0.5f + 4.0f);
        rect((int)minx - pad, (int)miny - pad, (int)(maxx - minx) + 2 * pad, (int)(maxy - miny) + 2 * pad, ACCENT);
    }

    // cursor: hand over the panel, arrow in SELECT mode, our brush ring while drawing.
    int want = (mouse_y() < PANEL_H || dd_open) ? CURSOR_HAND
             : selmode ? CURSOR_DEFAULT
             : -1;   // -1 = hide OS cursor, draw the brush ring
    if (want != cursor_panel) {
        cursor_panel = want;
        if (want < 0) mouse_hide();
        else { mouse_cursor(want); mouse_show(); }
    }
    if (want < 0) {   // brush ring previews the reach (sketch) or live width (slow = big, fast = small)
        int r;
        if (BRUSHES[tool].kind == K_SKETCH) r = (int)SKETCH_R;
        else {
            Brush b = BRUSHES[tool];
            float sp = ema / b.speedref; if (sp > 1) sp = 1;
            r = (int)((b.maxw - (b.maxw - b.minw) * sp) * thickness() * 0.5f + 0.5f);
        }
        if (r < 1) r = 1;
        circ(mouse_x(), mouse_y(), r, COLORS[colsel]);
    }

    draw_panel();   // on top of the strokes
}
