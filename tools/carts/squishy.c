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
    "detail": "Every stroke is stored as DATA (a path of points + the speed you drew each one at), not painted straight to pixels. Most brushes render that path as a chain of overlapping round stamps whose width = a slow→fat / fast→thin speed curve × an end-taper × a little seeded wobble — ink / pencil / fineliner / marker / chalk are different sets of those numbers (chalk drops stamps for a dry, broken grain). Six brushes render specially: SKETCH (a hairy web of threads, à la Krita's Sketch engine), SPRAY (an airbrush dot-cloud whose spread follows speed), BRISTLE (raked parallel hairs), PAINT (a wide wet brush whose paint runs DOWN in drips from every exposed bottom edge of the stroke — so a serpentine drips off each of its bands, not just the lowest; a run stops when it meets paint below, so inner bands make short runs into the gaps and only the open bottom edge falls long. Run length ∝ the band's thickness. Still a pure function of path+seed, no simulation. It also takes BEVEL for a raised glossy edge; boil wobbles the wet body but the runs stay put — they're 'dried', computed from the stable path), and NIB (a flat calligraphy nib held at a fixed angle: width comes from the ANGLE between the stroke and the nib, not speed — a hairline when you move along the nib, full width across it, so you get true broad-nib thick/thins. Rotate the nib angle with [ and ]; each stroke keeps the angle it was drawn at. The nib is a FAMILY of four sharing one renderer that reads an angle recipe: NIB (fixed angle), BRUSHPEN (angle PLUS a speed swell — swells slow, thins fast), REED (a dry pen whose angle chatters per point, seeded so it's a stable texture that still boils), and TWIST (the angle winds along the stroke); the rim/fill features all take the true nib-ribbon shape), and OIL (thick impasto paint faked for the limited palette: an auto-bevel rim gives the raised, light-catching edge and raked highlight/shadow streaks along the drag read as the ridges and grooves of paint pushed by a knife). OUTLINE draws a fatter silhouette in its own colour UNDER the fill — a black rim + coloured fill is the chunky bubble-lettering look (draw with the fat ink brush, add bevel for a highlight, and you're at the Tiny-Jam logo). BEVEL embosses a stroke into a faux-3D rim; the light is a GLOBAL 'sun' set from an on-screen SUN popover (drag the dial to aim it, tap one of four colours warm peach → golden → neutral → cool moon, toggle auto-spin) or from the keyboard (, and . rotate, / recolours, \\ auto-rotates); every beveled stroke (and the oil-paint rim) relights together, so you sweep and recolour the sun across the whole drawing; BOIL brings a stroke alive in one of two styles (the boil button cycles off → wobble → pulse): WOBBLE = per-point hand-drawn jitter cycling ~7.5fps; PULSE = a subtle smooth grow/shrink breath about the stroke's centre. Almost free since rendering is a pure function of (stroke, seed). Bevel + boil are PER-STROKE — each stroke captures the toggle state when you draw it, so some strokes can be beveled or boiling and others still; the toolbar toggles set the default for NEW strokes (this is the groundwork for a select-and-edit tool). A SELECT toggle on the bar (or the S key) makes this a tiny NON-DESTRUCTIVE vector editor: click a stroke to pick it (accent box), and the bar's controls retarget to that stroke — recolour it, change its dither, toggle bevel/boil, drag its thickness — while a property strip adds bevel-SIZE + boil-INTENSITY + outline-WIDTH sliders and bring-to-FRONT / send-to-BACK ordering. Edit any stroke any time; nothing is baked. A 7-colour pen — black/blue/red/green/cyan/magenta/yellow (cyan is a dark teal; pico32 has no true cyan) — picked from an always-visible palette strip (the seven swatches show their real colours; the active one wears an accent tab); each stroke keeps its colour. The fat brushes can be filled with a dpaint-style dither — a Bayer-ordered density ramp (~12→87% ink via fillp), likewise shown as an always-visible swatch strip — the step before a real flood-fill. A stroke can instead carry a GRADIENT FILL (the G-key popover): its body dithers from the pen colour to a second colour across the stroke, with an ANGLE and a SPREAD knob — a small spread keeps solid ends with a narrow blend band (aaaaa→bfy→zzzzz), spread 1 is an even ramp end-to-end (abc…xyz); it's the dpaint dithered-gradient scoped to a brushstroke (wired for the stamp brushes + the wet-paint body, whose drips run in the from-colour). Any stroke can also cast a DROP SHADOW — the whole silhouette re-rendered dark and offset AWAY from the sun, so it reads as floating above the paper; it's the SAME sun as the bevel, so spinning the sun swings every shadow and every bevel rim together (toggle it in the SUN popover). WIDTH is live on the MOUSE WHEEL: scroll to set the brush size, or scroll mid-stroke to taper/swell the line — it's captured per point, so the change stays where you made it (real pressure, not a uniform rescale). The whole toolbar is ink-on-paper: white buttons, black glyphs; the brush dropdown opens a 2-column icon+name grid. A flood-fill tool + the pixelsnap animated-icon export come next.",
    "controls": "Two-row top panel (ink-on-paper). ROW 1 (brush + effects): a brush dropdown that opens a 2-column icon+name grid (ink/pencil/liner/marker/chalk/sketch/spray/bristle/paint/nib/brushpen/reed/twist/oil), a thickness slider, a BVL toggle, a BOIL cycle (off/wobble/pulse), UNDO, an OUT(line) toggle + an outline-colour ring chip, and a SUN button (opens a light-dial + colour + spin popover + a DROP-SHADOW toggle). ROW 2 (colour + fill): the 7-colour palette, the dither-pattern strip, a GRADIENT button (a ramp glyph — opens the gradient popover: on/off + far colour + spread + angle; the G key does the same), and a SELECT (marquee) toggle. Active swatches/toggles wear an accent tab. Drag to draw; the MOUSE WHEEL sets the width live (scroll mid-stroke to taper/swell it — per-point pressure). Turn SELECT on (button or S key), click a stroke, then edit it via the bar + the bevel-size/boil-amt/outline-width/shadow-distance sliders and the FRONT/BACK ordering buttons. Keys: B bevel, O boil, S select, U undo, C clear, G gradient popover, [ / ] rotate the calligraphy nib angle, , / . rotate the bevel light (the sun), / cycles the sun's colour, \\ auto-rotates the sun."
  },
  "todo": [
    "Polish the tool-icon glyphs — ink/chalk still read a bit muddy at 16px, and the nib-family marks (brushpen comma / reed zigzag / twist coil) are small (sprite-draw.js in squishy.cart.js).",
    "Cache finished strokes + the boil frames to layer buffers instead of re-rendering every stroke every frame.",
    "The pixelsnap animated-icon export: boil frames → pixelsnap → an animated sprite strip.",
    "spec(): same-seed determinism + jitter-bounds.",
    "Feature × drawtool coverage is guarded by `node tools/squishy-features.js` (SQUISHY_MATRIX grid + pixel-diff vs a declared support matrix) — extend the matrix/EXPECT when a new brush or rim feature lands.",
    "Gradient FILL is wired for the stamp brushes + the wet-paint body; extend it to the NIB ribbon (and decide if oil wants it) — then flip those cells to 1 in squishy-features.js EXPECT.",
    "Gradient angle is a 0..360 slider in the G popover; a drag-to-aim dial (dpaint-style, like the sun dial) would feel better."
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
#include <stdlib.h>   // getenv — the SQUISHY_MATRIX feature-coverage self-test
#include <stdio.h>    // snprintf — popover value labels

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

// bevel tones — a lit rim toward the "sun" (global angle) + a dark rim opposite.
#define SHADOW    CLR_BLACK        // the shaded rim, darker than ink
#define BEVEL_OFF 1.0f             // default px the rims peek out past the ink body
#define BEVEL_MAX 4.0f             // max bevel rim size (the select tool's size slider)
#define SHADOW_DIST 5.0f           // default drop-shadow cast distance (px) when the toggle is on
#define SHADOW_MAX  16.0f          // max drop-shadow distance (the select tool's slider)
// the sun's COLOUR — the lit-rim tint, a global scene control (/ cycles it): warm → golden →
// neutral → cool "moon". Default warm peach = the old fixed HILITE, so existing art is unchanged.
static const int LIGHTS[] = { CLR_LIGHT_PEACH, CLR_YELLOW, CLR_WHITE, CLR_LIGHT_GREY };
#define NLIGHTS ((int)(sizeof(LIGHTS) / sizeof(LIGHTS[0])))

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

// 4×4 ordered (Bayer) matrix for the gradient FILL — the same dpaint trick: a pixel takes
// the "to" colour once the ramp position t passes its threshold, so a smooth t → a dithered ramp.
static const int GRAD_BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
#define GRAD_SPREAD_DEF 0.7f   // default blend-band width (0..1)

#define PANEL_H   44               // top tool-bar height (TWO rows); the canvas is below it
#define ROW2_Y    24               // row-2 button y (row 1 = brush+effects, row 2 = colour+fill)
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
enum { K_STAMP, K_CHALK, K_SKETCH, K_SPRAY, K_BRISTLE, K_DRIP, K_NIB, K_IMPASTO };

// where a K_NIB brush's angle comes from (the second signal, alongside speed).
// FIXED = the classic broad nib held at nib_angle; SPEED = a brush-pen that ALSO
// swells slow / thins fast; JITTER = a dry reed pen whose angle chatters per point
// (seeded → still deterministic, boils for free); SPIN = a twist pen whose angle
// winds along the stroke. All four are ONE renderer reading these table fields.
enum { ANG_NONE, ANG_FIXED, ANG_SPEED, ANG_JITTER, ANG_SPIN };

// a tool = a brush recipe. width swings between minw (full speed) and maxw
// (standstill); speedref = px/frame where width bottoms out; noise = per-stamp
// width wobble; taper = how many SAMPLES to taper at each end (a FIXED length,
// not a fraction of the stroke). For spray, maxw = the scatter-cloud radius; for
// bristle, maxw = the hair spread.
// a tool is now PURE DATA — width recipe + kind + an angle recipe + which icon
// slot to show. Add a brush by adding a row; no new renderer for the nib family.
// angle_mode/angle_param only matter for K_NIB (see the ANG_* note above); icon =
// sprite slot for the dropdown (lets several brushes share one glyph).
typedef struct { float minw, maxw, speedref, noise, taper; int kind; const char *name;
                 int angle_mode; float angle_param; int icon; } Brush;
static const Brush BRUSHES[] = {
    { 1.8f, 9.0f,  9.0f, 0.15f, 9.0f, K_STAMP,   "ink",  ANG_NONE,   0.0f,  0 },   // the squish — big swing, hand-inked
    { 1.0f, 2.6f,  6.0f, 0.35f, 5.0f, K_STAMP,   "pen",  ANG_NONE,   0.0f,  1 },   // pencil: thin, scratchy, grainy
    { 1.2f, 1.9f, 12.0f, 0.05f, 4.0f, K_STAMP,   "fin",  ANG_NONE,   0.0f,  2 },   // fineliner: thin, crisp, near-constant
    { 5.0f, 7.5f, 22.0f, 0.05f, 3.0f, K_STAMP,   "mrk",  ANG_NONE,   0.0f,  3 },   // marker: wide, even, flat
    { 2.5f, 5.0f, 10.0f, 0.55f, 4.0f, K_CHALK,   "chk",  ANG_NONE,   0.0f,  4 },   // chalk: dry, grainy, broken edges
    { 2.0f, 2.0f, 12.0f, 0.00f, 1.0f, K_SKETCH,  "skt",  ANG_NONE,   0.0f,  5 },   // sketch: a hairy web of threads
    { 3.0f,13.0f, 11.0f, 0.00f, 3.0f, K_SPRAY,   "spr",  ANG_NONE,   0.0f,  6 },   // airbrush: scattered dots (maxw = cloud)
    { 4.0f, 9.0f, 10.0f, 0.10f, 4.0f, K_BRISTLE, "brs",  ANG_NONE,   0.0f,  7 },   // bristle: raked parallel hairs
    { 6.0f,14.0f, 14.0f, 0.10f, 4.0f, K_DRIP,    "pnt",  ANG_NONE,   0.0f,  8 },   // paint: wide + wet, runs drip DOWN from the wettest spots
    { 2.0f,11.0f, 12.0f, 0.00f, 1.0f, K_NIB,     "nib",  ANG_FIXED,  0.0f,  9 },   // calligraphy: flat nib, width from stroke ANGLE (maxw = nib width)
    { 2.5f,11.0f, 12.0f, 0.00f, 1.0f, K_NIB,     "bpn",  ANG_SPEED,  0.0f, 11 },   // brush pen: broad nib that ALSO swells slow / thins fast
    { 6.0f, 6.0f, 12.0f, 0.00f, 1.0f, K_NIB,     "red",  ANG_JITTER, 22.0f,12 },   // reed pen: dry, angle chatters ±param° per point (seeded)
    { 7.0f, 7.0f, 12.0f, 0.00f, 1.0f, K_NIB,     "twt",  ANG_SPIN,   2.0f, 13 },   // twist nib: angle winds param°/point along the stroke
    { 7.0f,13.0f, 16.0f, 0.15f, 3.0f, K_IMPASTO, "oil",  ANG_NONE,   0.0f, 10 },   // oil/impasto: thick paint — auto-bevel rim + raked knife streaks
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
static const char *TOOL_DISP[] = { "ink", "pencil", "liner", "marker", "chalk", "sketch", "spray", "bristle", "paint", "nib", "brushpen", "reed", "twist", "oil" };
#define NIB_ANGLE_DEF   45.0f  // calligraphy nib angle (deg) — the classic italic slant
#define IMPASTO_RIM     2.0f   // oil/impasto auto-bevel rim size (px) — the raised paint edge
#define IMPASTO_STREAKS 4      // raked ridge/groove streaks across the width (knife texture)
#define OUTLINE_PX      2.0f   // outline rim thickness (px) when the outline toggle is on
#define OUTLINE_MAX     6.0f   // max outline rim (the select tool's outline-width slider)

typedef struct { float x, y, speed, thick; } Sample;   // thick = live PRESSURE at this point (mouse-wheel), varies width ALONG the stroke
typedef struct {
    unsigned seed;          // per-stroke seed → deterministic wobble (boil reseeds this)
    int      tool;          // which brush this stroke was drawn with
    int      color;         // palette colour this stroke was drawn in
    int      pattern;       // dither pattern index (0 = solid) — stamp brushes only
    float    bevel;         // bevel rim size in px (0 = off) — per-stroke, captured/editable
    float    boil;          // boil intensity 0..1 (0 = still) — per-stroke, captured/editable
    int      boil_style;    // BOIL_WOBBLE (jitter) or BOIL_BREATHE (scale pulse)
    float    thick;         // thickness multiplier this stroke was drawn with
    float    nib_angle;     // calligraphy nib angle (deg) — K_NIB only, captured per-stroke
    float    outline;       // outline rim size in px (0 = off) — per-stroke, captured/editable
    int      outline_color; // palette colour of the outline
    float    shadow;        // drop-shadow cast distance in px (0 = off) — per-stroke, direction follows the sun
    int      grad;          // gradient FILL on? (the body dithers from `color` → `grad_color` across the stroke)
    int      grad_color;    // the gradient's far colour (the "to"; `color` is the "from")
    float    grad_angle;    // gradient ramp direction (deg)
    float    grad_spread;   // blend-band width 0..1 (1 = even ramp across the whole stroke; small = solid ends, narrow blend)
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
static float    nib_angle = NIB_ANGLE_DEF;  // calligraphy nib angle DEFAULT (deg); [ / ] rotate it
static int      outline = 0;         // outline DEFAULT for new strokes (on/off toggle)
static int      outsel = 0;          // outline colour DEFAULT (index into COLORS; 0 = INK)
static int      dropshadow = 0;      // drop-shadow DEFAULT for new strokes (on/off toggle)
static int      grad = 0;            // gradient-fill DEFAULT for new strokes (on/off)
static int      gradsel = 2;         // gradient far-colour DEFAULT (index into COLORS; 2 = red)
static float    grad_angle = 0.0f;   // gradient ramp-angle DEFAULT (deg)
static float    grad_spread = GRAD_SPREAD_DEF;  // gradient spread DEFAULT (0..1)
static float    bevel_angle = 225.0f;// GLOBAL bevel light direction (deg, the "sun"); , / . rotate it.
                                     // Shared by every beveled stroke so rotating it relights the scene.
static int      light_sel = 0;       // GLOBAL sun COLOUR (index into LIGHTS); / cycles it (warm..cool)
static int      sun_spin = 0;        // auto-rotate the bevel sun? (\ toggles) — a slowly circling light
static int      sun_open = 0;        // the sun-control popover open? (a bar button opens it)
static int      grad_open = 0;       // the gradient-control popover open? (the G key opens it)
#define SUN_SPIN_SPEED 0.8f          // degrees the sun advances per frame when spinning (~7.5s / turn)
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
static float    pressure = 1.0f;     // live width multiplier driven by the mouse WHEEL — captured per point so scrolling mid-stroke tapers/swells the line
static unsigned seedctr = 0x1234567u;

#define PRESSURE_STEP 0.12f          // width change per wheel notch
#define PRESSURE_MIN  0.30f
#define PRESSURE_MAX  3.00f

static float thickness(void) { return 0.4f + thick01 * 1.6f; }   // 0.4×..2.0×

// integer hash → [0,1); deterministic, no global rnd() state (boil-friendly).
static unsigned hashu(unsigned x){ x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x; }
static float    hashf(unsigned x){ return (hashu(x) & 0xFFFFFF) / (float)0x1000000; }

// width of the brush at sample i — the pure function: speed curve × taper × wobble
// × the per-point wheel PRESSURE captured while drawing.
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
    return wspeed * taper * wobble * s->pts[i].thick;
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
// `color`. (ox,oy) is the bevel rim offset; `grow` widens every stamp (for the
// outline silhouette pass); the boil wobble comes in via `b`.
static void render_stroke(const Stroke *s, float ox, float oy, int color, const Boil *b, float grow) {
    if (s->n == 0) return;
    int chalk = BRUSHES[s->tool].kind == K_CHALK;   // dry, broken: drop ~40% of stamps
    if (s->n == 1) {
        float x = s->pts[0].x, y = s->pts[0].y; boil_pt(s, 0, &x, &y, b);
        stamp(x + ox, y + oy, sample_width(s, 0, b->fseed) + grow, color);
        return;
    }
    for (int i = 0; i < s->n - 1; i++) {
        float x0 = s->pts[i].x,   y0 = s->pts[i].y;
        float x1 = s->pts[i+1].x, y1 = s->pts[i+1].y;
        boil_pt(s, i, &x0, &y0, b); boil_pt(s, i + 1, &x1, &y1, b);
        float w0 = sample_width(s, i, b->fseed) + grow, w1 = sample_width(s, i + 1, b->fseed) + grow;
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

// the optional DROP SHADOW: the whole stroke silhouette re-rendered dark, offset AWAY
// from the "sun" (opposite the bevel's lit side) and UNDER the stroke — so it reads as
// floating above the paper. Distance is per-stroke; the direction is the global sun, so
// spinning the sun swings every shadow (and every bevel rim) together.
static void shadow_pass(const Stroke *s, const Boil *b) {
    if (s->shadow <= 0) return;
    float ox = -cos_deg(bevel_angle) * s->shadow, oy = -sin_deg(bevel_angle) * s->shadow;
    render_stroke(s, ox, oy, SHADOW, b, 0);
}

// the optional raised bevel rim: a SHADOW copy away from the "sun" + a lit copy toward it
// (global angle + colour). Shared by the stamp brushes AND the drip body.
static void bevel_pass(const Stroke *s, const Boil *b) {
    if (s->bevel <= 0) return;
    float hx = cos_deg(bevel_angle) * s->bevel, hy = sin_deg(bevel_angle) * s->bevel;
    render_stroke(s, -hx, -hy, SHADOW, b, 0);
    render_stroke(s,  hx,  hy, LIGHTS[light_sel], b, 0);
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

static void render_gradient(const Stroke *s, const Boil *b);   // fwd: drip's body can be a gradient too

// PAINT: a wide wet body, then runs that drip DOWN from every exposed bottom edge.
// Rasterise the body into drip_cov, then per column find each paint→paper transition
// (a band bottom) and run a drip from it. Pure function of (path, seed) — same soul as
// boil, no simulation, fully re-renderable.
static void render_drip(const Stroke *s, const Boil *b) {
    if (s->outline > 0)   // fatter silhouette UNDER everything (bubble-letter rim), like the stamp brushes
        render_stroke(s, 0, 0, s->outline_color, b, s->outline * 2);
    bevel_pass(s, b);                         // optional raised rim UNDER the wet body
    if (s->grad) render_gradient(s, b);       // gradient wet paint (fills the body, then drips run in `color`)
    else {
        if (s->pattern) fillp(PATTERNS[s->pattern], PAPER);   // dpaint-style dither fills the wet body
        render_stroke(s, 0, 0, s->color, b, 0);   // the wet body (same stamp chain as the fat brushes)
        if (s->pattern) fillp_reset();
    }
    if (s->n < 1) return;

    // Drips are DRIED runs: compute their geometry from the STABLE (un-boiled) path + width, so a
    // boiling body can wobble while the runs stay put instead of dancing/flickering frame to frame.
    Boil still = { 0, 0, s->seed, 0, 0, 0 };

    // painted bounding box (points ± their radius), clamped to screen
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = 0; i < s->n; i++) {
        float x = s->pts[i].x, y = s->pts[i].y;
        float r = sample_width(s, i, still.fseed) * 0.5f;
        if (x - r < minx) minx = x - r; if (x + r > maxx) maxx = x + r;
        if (y - r < miny) miny = y - r; if (y + r > maxy) maxy = y + r;
    }
    int x0 = (int)minx, x1 = (int)maxx, y0 = (int)miny, y1 = (int)maxy;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W - 1) x1 = SCREEN_W - 1; if (y1 > SCREEN_H - 1) y1 = SCREEN_H - 1;
    if (x1 < x0 || y1 < y0) return;

    // rasterise the wet body's coverage (a disk per sample) into the bbox — stable positions
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) drip_cov[y][x] = 0;
    for (int i = 0; i < s->n; i++) {
        float cx = s->pts[i].x, cy = s->pts[i].y;
        float r = sample_width(s, i, still.fseed) * 0.5f; if (r < 0.5f) r = 0.5f;
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

// mark a filled disk into drip_cov, clipped to the bbox (shared by the gradient coverage pass)
static void cov_disk(float cx, float cy, float r, int x0, int x1, int y0, int y1) {
    if (r < 0.5f) r = 0.5f;
    for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
        if (x < x0 || x > x1) continue;
        float dx = x - cx, hh = r * r - dx * dx; if (hh < 0) continue; hh = sqrtf(hh);
        int ya = (int)(cy - hh), yb = (int)(cy + hh);
        if (ya < y0) ya = y0; if (yb > y1) yb = y1;
        for (int y = ya; y <= yb; y++) drip_cov[y][x] = 1;
    }
}

// GRADIENT FILL (the dpaint dithered ramp, scoped to one stroke): rasterise the stroke's
// coverage (disks stepped ALONG each segment, like render_stroke — not one per sample, which
// left gaps/blobs on the thin tapered start) into drip_cov, project every covered pixel onto the axis
// to get t∈[0,1], stretch t by the SPREAD (small spread = solid ends + a narrow blend band;
// 1 = an even ramp end-to-end), then a Bayer threshold picks `color` (t low) vs `grad_color`
// (t high). Same pure-function-of-(path,seed) soul as the rest — boils via boil_pt.
static void render_gradient(const Stroke *s, const Boil *b) {
    if (s->n < 1) return;
    float ca = cos_deg(s->grad_angle), sa = sin_deg(s->grad_angle);
    // bbox of the boiled body + the ramp's projected extent (pad by each sample's radius)
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f, pmin = 1e9f, pmax = -1e9f;
    for (int i = 0; i < s->n; i++) {
        float x = s->pts[i].x, y = s->pts[i].y; boil_pt(s, i, &x, &y, b);
        float r = sample_width(s, i, b->fseed) * 0.5f; if (r < 0.5f) r = 0.5f;
        if (x - r < minx) minx = x - r; if (x + r > maxx) maxx = x + r;
        if (y - r < miny) miny = y - r; if (y + r > maxy) maxy = y + r;
        float p = x * ca + y * sa;
        if (p - r < pmin) pmin = p - r; if (p + r > pmax) pmax = p + r;
    }
    int x0 = (int)minx, x1 = (int)maxx, y0 = (int)miny, y1 = (int)maxy;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W - 1) x1 = SCREEN_W - 1; if (y1 > SCREEN_H - 1) y1 = SCREEN_H - 1;
    if (x1 < x0 || y1 < y0 || pmax - pmin < 0.001f) return;

    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) drip_cov[y][x] = 0;
    if (s->n == 1) {
        float cx = s->pts[0].x, cy = s->pts[0].y; boil_pt(s, 0, &cx, &cy, b);
        cov_disk(cx, cy, sample_width(s, 0, b->fseed) * 0.5f, x0, x1, y0, y1);
    } else for (int i = 0; i < s->n - 1; i++) {   // step ALONG each segment (dense) so coverage is continuous
        float ax = s->pts[i].x, ay = s->pts[i].y, bx = s->pts[i+1].x, by = s->pts[i+1].y;
        boil_pt(s, i, &ax, &ay, b); boil_pt(s, i + 1, &bx, &by, b);
        float w0 = sample_width(s, i, b->fseed) * 0.5f, w1 = sample_width(s, i + 1, b->fseed) * 0.5f;
        float ddx = bx - ax, ddy = by - ay, seg = sqrtf(ddx * ddx + ddy * ddy);
        int steps = (int)(seg / STAMP_SPACING); if (steps < 1) steps = 1;
        for (int k = 0; k <= steps; k++) { float u = (float)k / steps; cov_disk(ax + ddx * u, ay + ddy * u, w0 + (w1 - w0) * u, x0, x1, y0, y1); }
    }
    float sp = s->grad_spread; if (sp < 0.02f) sp = 0.02f;
    float lo = 0.5f - sp * 0.5f, inv = 1.0f / (pmax - pmin);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!drip_cov[y][x]) continue;
            float t = ((x * ca + y * sa) - pmin) * inv;   // 0..1 along the ramp axis
            t = (t - lo) / sp; if (t < 0) t = 0; if (t > 1) t = 1;   // stretch by spread → solid ends
            float thr = (GRAD_BAYER[y & 3][x & 3] + 0.5f) / 16.0f;
            pset(x, y, t > thr ? s->grad_color : s->color);
        }
}

// the half nib-edge vector at sample i — where the nib's angle AND width come from,
// per the brush's angle recipe (all four ANG_* variants funnel through here so the
// renderer below stays one function). JITTER keys off s->seed only (not the boil
// frame) so the dry-pen chatter is a stable texture, while the points still boil.
static void nib_edge(const Stroke *s, int i, float grow, float *nx, float *ny) {
    Brush br = BRUSHES[s->tool];
    float ang = s->nib_angle;
    if (br.angle_mode == ANG_JITTER)
        ang += (hashf(s->seed ^ (unsigned)(i * 2654435761u)) * 2 - 1) * br.angle_param;
    else if (br.angle_mode == ANG_SPIN)
        ang += i * br.angle_param;                              // winds along the stroke
    float pr = s->pts[i].thick;                                 // per-point wheel pressure
    float hw = br.maxw * s->thick * 0.5f * pr;                  // half the nib width
    if (br.angle_mode == ANG_SPEED) {                           // brush pen: swell slow, thin fast
        float sp = s->pts[i].speed / br.speedref; if (sp > 1) sp = 1; if (sp < 0) sp = 0;
        float minhw = br.minw * s->thick * 0.5f * pr;
        hw -= (hw - minhw) * sp;
    }
    hw += grow;                                                 // fatter ribbon for the outline pass
    *nx = cos_deg(ang) * hw; *ny = sin_deg(ang) * hw;
}

// CALLIGRAPHY nib family: a flat broad edge whose width comes from the ANGLE between
// the stroke and the nib — move along the edge and it's a hairline, across it and you
// get the full nib width. Rendered as a ribbon of nib-wide quads between consecutive
// points, with the nib edge stamped at each vertex to close the joints. The nib's base
// angle (`nib_angle`) is captured per stroke; nib_edge() applies the per-brush recipe.
// Parameterised so the SAME ribbon draws the body AND the rim passes: `grow` fattens it
// (outline), (ox,oy) offsets it (shadow / bevel rims), `color` recolours it.
static void render_nib_ex(const Stroke *s, const Boil *b, float grow, float ox, float oy, int color) {
    float nx0, ny0, nx1, ny1;
    if (s->n == 1) {
        float x = s->pts[0].x + ox, y = s->pts[0].y + oy; boil_pt(s, 0, &x, &y, b);
        nib_edge(s, 0, grow, &nx0, &ny0);
        line((int)(x-nx0+.5f),(int)(y-ny0+.5f),(int)(x+nx0+.5f),(int)(y+ny0+.5f), color);
        return;
    }
    for (int i = 0; i < s->n - 1; i++) {
        float x0=s->pts[i].x+ox, y0=s->pts[i].y+oy, x1=s->pts[i+1].x+ox, y1=s->pts[i+1].y+oy;
        boil_pt(s,i,&x0,&y0,b); boil_pt(s,i+1,&x1,&y1,b);
        nib_edge(s, i, grow, &nx0, &ny0); nib_edge(s, i+1, grow, &nx1, &ny1);   // per-point: angle/width can vary along the stroke
        int ax=(int)(x0+nx0+.5f), ay=(int)(y0+ny0+.5f), bx=(int)(x0-nx0+.5f), by=(int)(y0-ny0+.5f);
        int cx=(int)(x1+nx1+.5f), cy=(int)(y1+ny1+.5f), dx=(int)(x1-nx1+.5f), dy=(int)(y1-ny1+.5f);
        trifill(ax, ay, cx, cy, dx, dy, color);                // the nib-wide ribbon quad, as two tris
        trifill(ax, ay, dx, dy, bx, by, color);
        line(ax, ay, bx, by, color);                           // nib edge at the joint (fills convex gaps)
    }
    float xe=s->pts[s->n-1].x+ox, ye=s->pts[s->n-1].y+oy; boil_pt(s,s->n-1,&xe,&ye,b);
    nib_edge(s, s->n-1, grow, &nx1, &ny1);
    line((int)(xe-nx1+.5f),(int)(ye-ny1+.5f),(int)(xe+nx1+.5f),(int)(ye+ny1+.5f), color);   // final nib
}
static void render_nib(const Stroke *s, const Boil *b) { render_nib_ex(s, b, 0, 0, 0, s->color); }

// OIL / IMPASTO: thick paint faked for a limited palette. An auto-bevel rim gives the
// raised, light-catching edge; then raked HILITE/SHADOW streaks along the drag read as
// the ridges + grooves of paint dragged by a stiff brush or knife. Pure f(path, seed).
static void render_impasto(const Stroke *s, const Boil *b) {
    float hx = cos_deg(bevel_angle) * IMPASTO_RIM, hy = sin_deg(bevel_angle) * IMPASTO_RIM;
    render_stroke(s, -hx, -hy, SHADOW, b, 0);                     // raised rim follows the global sun:
    render_stroke(s,  hx,  hy, LIGHTS[light_sel], b, 0);          // hilite toward the light, shadow away
    render_stroke(s, 0, 0, s->color, b, 0);                       // the fat paint body on top
    if (s->n < 2) return;
    for (int i = 0; i < s->n - 1; i++) {                       // raked knife streaks over the body
        float x0=s->pts[i].x,y0=s->pts[i].y,x1=s->pts[i+1].x,y1=s->pts[i+1].y;
        boil_pt(s,i,&x0,&y0,b); boil_pt(s,i+1,&x1,&y1,b);
        float dx=x1-x0, dy=y1-y0, len=sqrtf(dx*dx+dy*dy);
        if (len < 0.01f) continue;
        float px=-dy/len, py=dx/len;                           // perpendicular unit
        float w=(sample_width(s,i,b->fseed)+sample_width(s,i+1,b->fseed))*0.5f;
        for (int k = 0; k < IMPASTO_STREAKS; k++) {
            unsigned hh = hashu(s->seed ^ (unsigned)(k * 2654435761u));
            float j = ((hh & 0xFFFF) / 65535.0f - 0.5f) * 0.3f;
            float off = ((k + 0.5f) / IMPASTO_STREAKS - 0.5f + j) * w * 0.8f;
            int col = (k & 1) ? SHADOW : LIGHTS[light_sel];    // alternate groove / lit ridge
            line((int)(x0+px*off+.5f),(int)(y0+py*off+.5f),
                 (int)(x1+px*off+.5f),(int)(y1+py*off+.5f), col);
        }
    }
}

// draw one stroke. Sketch is its own renderer; the other brushes use the stamp
// chain, with the bevel emboss if it's on: a SHADOW + HILITE copy offset under
// the ink body leave a light rim on the upper-left, a dark rim on the lower-right.
// Pure geometry, no canvas read-back → deterministic. `jitter` propagates the boil
// wobble to all passes so the rim moves with the body.
static void draw_one(const Stroke *s, const Boil *b) {
    int k = BRUSHES[s->tool].kind;
    // drop shadow goes UNDER everything, first. render_stroke's round-chain silhouette
    // matches the solid-body brushes (stamp/chalk/paint/oil); the airy/angle brushes
    // (spray/sketch/bristle/nib) skip it — a cast shadow there wouldn't match their shape.
    if (k == K_STAMP || k == K_CHALK || k == K_DRIP || k == K_IMPASTO) shadow_pass(s, b);
    // NIB family: rim features route through the parameterised ribbon so the outline /
    // shadow / bevel all take the true NIB shape (not a round silhouette). Order matches
    // the stamp path: shadow (back) → outline → bevel → dithered body.
    if (k == K_NIB) {
        if (s->shadow > 0)
            render_nib_ex(s, b, 0, -cos_deg(bevel_angle)*s->shadow, -sin_deg(bevel_angle)*s->shadow, SHADOW);
        if (s->outline > 0)
            render_nib_ex(s, b, s->outline, 0, 0, s->outline_color);
        if (s->bevel > 0) {
            float hx = cos_deg(bevel_angle)*s->bevel, hy = sin_deg(bevel_angle)*s->bevel;
            render_nib_ex(s, b, 0, -hx, -hy, SHADOW);
            render_nib_ex(s, b, 0,  hx,  hy, LIGHTS[light_sel]);
        }
        if (s->pattern) fillp(PATTERNS[s->pattern], PAPER);
        render_nib_ex(s, b, 0, 0, 0, s->color);
        if (s->pattern) fillp_reset();
        return;
    }
    switch (k) {
        case K_SKETCH:  render_sketch(s, b);  return;
        case K_SPRAY:   render_spray(s, b);   return;
        case K_BRISTLE: render_bristle(s, b); return;
        case K_DRIP:    render_drip(s, b);    return;
        case K_IMPASTO: render_impasto(s, b); return;
        default: break;   // K_STAMP / K_CHALK use the stamp chain below
    }
    if (s->outline > 0)   // outline: a fatter silhouette in outline_color, UNDER everything (bubble-letter rim)
        render_stroke(s, 0, 0, s->outline_color, b, s->outline * 2);
    bevel_pass(s, b);     // per-stroke SIZE, GLOBAL sun angle+colour
    if (s->grad) { render_gradient(s, b); return; }       // gradient FILL replaces the flat/patterned body
    if (s->pattern) fillp(PATTERNS[s->pattern], PAPER);   // dpaint-style dither fills the body only
    render_stroke(s, 0, 0, s->color, b, 0);
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

// the sun-control popover: a draggable light dial + the sun-colour swatches + a spin toggle
#define SUN_BTN_X 155                      // the bar button that opens it
#define SUN_PX  92                         // popover top-left
#define SUN_PY  PANEL_H
#define SUN_PW  116
#define SUN_PH  80                         // grew to fit the drop-shadow toggle at the bottom
#define SUN_DCX (SUN_PX + 30)              // dial centre
#define SUN_DCY (SUN_PY + 30)
#define SUN_DR  22                         // dial radius
#define RAD2DEG 57.29578f

// the GRADIENT-fill popover (opened with the G key — no room on the packed bar)
#define GRAD_PX 44
#define GRAD_PY PANEL_H
#define GRAD_PW 232
#define GRAD_PH 74

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
    line(0, ROW2_Y - 2, SCREEN_W, ROW2_Y - 2, CLR_LIGHT_GREY);   // faint divider between the two rows
    line(0, PANEL_H - 1, SCREEN_W, PANEL_H - 1, INK);
    ui_begin();
    // brush dropdown header (the icon identifies the tool; when editing, an ACCENT ring flags it)
    if (ui_spr_button_styled(BRUSHES[tool].icon, DD_X, 2, DD_W, 20, dd_open, TOOLBTN)) dd_open = !dd_open;
    if (editing) rect(DD_X - 1, 1, DD_W + 2, 22, ACCENT);
    font(FONT_SMALL);   // small font for the whole bar

    // thickness — edits the selection's thickness, else the new-stroke default
    if (sel) {
        static float t; t = (sel->thick - 0.4f) / 1.6f;
        if (ui_slider(&t, 26, 4, 34, "thk")) sel->thick = 0.4f + t * 1.6f;
    } else ui_slider(&thick01, 26, 4, 34, "thk");
    rect(26, 5, 34, 6, CLR_LIGHT_GREY);   // outline the track (white-on-white otherwise)

    // bevel toggle
    int bev_on = sel ? (sel->bevel > 0) : bevel;
    if (ui_button(62, 3, 15, 16, "bvl")) { if (sel) sel->bevel = sel->bevel > 0 ? 0 : BEVEL_OFF; else bevel = !bevel; }
    if (bev_on) rectfill(62, 19, 15, 2, ACCENT);

    // boil — cycles off → wobble → pulse (the style); the label shows which
    int bstate = sel ? (sel->boil <= 0 ? 0 : sel->boil_style + 1)
                     : (boil == 0 ? 0 : boil_style + 1);
    if (ui_button(79, 3, 22, 16, bstate == 0 ? "boil" : bstate == 1 ? "wobl" : "puls")) {
        int ns = (bstate + 1) % 3;
        if (sel) { if (ns == 0) sel->boil = 0; else { if (sel->boil <= 0) sel->boil = 1.0f; sel->boil_style = ns - 1; } }
        else     { if (ns == 0) boil = 0;      else { boil = 1; boil_style = ns - 1; } }
    }
    if (bstate) rectfill(79, 19, 22, 2, ACCENT);

    if (ui_button(103, 3, 20, 16, "undo")) do_undo();

    // ---- outline toggle + its own colour chip (bubble-letter rim in outline_color under the fill)
    int out_on = sel ? (sel->outline > 0) : outline;
    if (ui_button(125, 3, 15, 16, "out")) { if (sel) sel->outline = sel->outline > 0 ? 0 : OUTLINE_PX; else outline = !outline; }
    if (out_on) rectfill(125, 19, 15, 2, ACCENT);
    int oci = sel ? color_index(sel->outline_color) : outsel;
    if (ui_button(142, 3, 10, 16, 0)) { int ni = (oci + 1) % NCOLORS; if (sel) sel->outline_color = COLORS[ni]; else outsel = ni; }
    rectfill(142, 3, 10, 16, COLORS[oci]);   // outline-colour chip drawn as a RING (reads as "outline",
    rectfill(144, 5, 6, 12, PAPER);          // not another fill swatch); click cycles the colour
    rect(142, 3, 10, 16, INK);

    // ---- SUN button: opens the light popover (dial + colour + spin). Glyph shows the live sun colour.
    if (ui_button(SUN_BTN_X, 3, 16, 16, 0)) { sun_open = !sun_open; dd_open = 0; }
    circfill(SUN_BTN_X + 8, 11, 3, LIGHTS[light_sel]); circ(SUN_BTN_X + 8, 11, 3, INK);
    for (int a = 0; a < 360; a += 45)
        pset(SUN_BTN_X + 8 + (int)(cos_deg(a) * 6), 11 + (int)(sin_deg(a) * 6), INK);   // rays
    if (sun_open || sun_spin) rectfill(SUN_BTN_X, 19, 16, 2, ACCENT);

    // ===== ROW 2 (colour & fill): palette · dither · gradient · select =====
    int r2tab = ROW2_Y + 16;
    // ---- colour palette (fill): all 7 swatches visible; the active one wears an ACCENT tab
    int ci = sel ? color_index(sel->color) : colsel;
    for (int i = 0; i < NCOLORS; i++) {
        int x = 2 + i * 9;
        if (ui_button(x, ROW2_Y, 9, 16, 0)) { if (sel) sel->color = COLORS[i]; else colsel = i; }
        rectfill(x, ROW2_Y, 9, 16, COLORS[i]);   // overdraw the button with the real colour
        rect(x, ROW2_Y, 9, 16, INK);
        if (i == ci) rectfill(x, r2tab, 9, 2, ACCENT);
    }

    // ---- dither patterns: all 6 visible, each showing its real fill; active wears the tab
    int pat = sel ? sel->pattern : patsel;
    for (int i = 0; i < NPATTERNS; i++) {
        int x = 72 + i * 9;
        if (ui_button(x, ROW2_Y, 9, 16, 0)) { if (sel) sel->pattern = i; else patsel = i; }
        fillp(PATTERNS[i], PAPER); rectfill(x, ROW2_Y, 9, 16, INK); fillp_reset();
        rect(x, ROW2_Y, 9, 16, INK);
        if (i == pat) rectfill(x, r2tab, 9, 2, ACCENT);
    }

    // ---- GRADIENT button: opens the gradient popover (also the G key). A little ramp glyph;
    // ACCENT tab when the fill is on. Sits with the other fill controls.
    int grad_on = sel ? sel->grad : grad;
    if (ui_button(132, ROW2_Y, 16, 16, 0)) { grad_open = !grad_open; dd_open = sun_open = 0; }
    for (int gx = 0; gx < 12; gx++)   // ramp glyph: solid ink at the left → sparse ticks at the right
        if (gx < 4 || (gx & 1) == 0) line(134 + gx, ROW2_Y + 3, 134 + gx, ROW2_Y + 12, INK);
    rect(132, ROW2_Y, 16, 16, INK);
    if (grad_open || grad_on) rectfill(132, r2tab, 16, 2, ACCENT);

    // ---- SELECT toggle: a white marquee box on a black button (turns ACCENT when on)
    if (ui_button(154, ROW2_Y, 18, 16, 0)) selmode = !selmode;
    rectfill(154, ROW2_Y, 18, 16, INK);
    dashed_rect(156, ROW2_Y + 3, 12, 10, selmode ? ACCENT : PAPER);
    if (selmode) rectfill(154, r2tab, 18, 2, ACCENT);

    // contextual property strip: bevel SIZE + boil + OUTLINE width sliders + z-order, for the selection
    if (editing) {
        rectfill(0, PANEL_H, SCREEN_W, PROP_H, PAPER);
        line(0, PANEL_H + PROP_H - 1, SCREEN_W, PANEL_H + PROP_H - 1, INK);
        static float bs; bs = sel->bevel / BEVEL_MAX; if (bs > 1) bs = 1;
        if (ui_slider(&bs, 6, PANEL_H + 3, 58, "bvl")) sel->bevel = bs * BEVEL_MAX;
        rect(6, PANEL_H + 4, 58, 6, CLR_LIGHT_GREY);
        static float bo; bo = sel->boil;
        if (ui_slider(&bo, 68, PANEL_H + 3, 58, "boil")) sel->boil = bo;
        rect(68, PANEL_H + 4, 58, 6, CLR_LIGHT_GREY);
        static float os; os = sel->outline / OUTLINE_MAX; if (os > 1) os = 1;
        if (ui_slider(&os, 130, PANEL_H + 3, 58, "out")) sel->outline = os * OUTLINE_MAX;   // 0 = off
        rect(130, PANEL_H + 4, 58, 6, CLR_LIGHT_GREY);
        if (ui_button(192, PANEL_H + 2, 28, 14, "back"))  to_back();
        if (ui_button(224, PANEL_H + 2, 32, 14, "frnt")) to_front();
        static float sh; sh = sel->shadow / SHADOW_MAX; if (sh > 1) sh = 1;
        if (ui_slider(&sh, 260, PANEL_H + 3, 54, "shd")) sel->shadow = sh * SHADOW_MAX;   // 0 = off
        rect(260, PANEL_H + 4, 54, 6, CLR_LIGHT_GREY);
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
            spr(BRUSHES[i].icon, x + 2, y + (DD_ITEM - 16) / 2);      // brush icon (its own sprite slot)
            print(TOOL_DISP[i], x + 20, y + (DD_ITEM - 6) / 2, INK);  // its name alongside
        }
    }
    // the sun-control popover: a light dial you drag, the 4 sun colours, and a spin toggle
    if (sun_open) {
        rectfill(SUN_PX, SUN_PY, SUN_PW, SUN_PH, PAPER);
        rect(SUN_PX, SUN_PY, SUN_PW, SUN_PH, INK);
        // the dial — drag anywhere inside it to aim the light; the sun dot sits in that direction
        circ(SUN_DCX, SUN_DCY, SUN_DR, CLR_LIGHT_GREY);
        float mdx = mouse_x() - SUN_DCX, mdy = mouse_y() - SUN_DCY;
        if (mouse_down(MOUSE_LEFT) && mdx*mdx + mdy*mdy <= (SUN_DR+4)*(SUN_DR+4))
            bevel_angle = atan2f(mdy, mdx) * RAD2DEG;
        int sx = SUN_DCX + (int)(cos_deg(bevel_angle) * (SUN_DR - 3));
        int sy = SUN_DCY + (int)(sin_deg(bevel_angle) * (SUN_DR - 3));
        line(SUN_DCX, SUN_DCY, sx, sy, CLR_LIGHT_GREY);          // ray to the sun
        circfill(sx, sy, 3, LIGHTS[light_sel]); circ(sx, sy, 3, INK);   // the sun (in its colour)
        // the 4 sun colours (2×2), active one ringed; then a spin toggle
        for (int i = 0; i < NLIGHTS; i++) {
            int x = SUN_PX + 60 + (i % 2) * 14, y = SUN_PY + 6 + (i / 2) * 14;
            if (ui_button(x, y, 12, 12, 0)) light_sel = i;
            rectfill(x, y, 12, 12, LIGHTS[i]); rect(x, y, 12, 12, i == light_sel ? ACCENT : INK);
        }
        // spin toggle — clear on/off; when ON it fills accent + a ↻ mark so it's unmistakable
        int spx = SUN_PX + 58, spy = SUN_PY + 36, spw = 48, sph = 16;
        if (ui_button(spx, spy, spw, sph, 0)) sun_spin = !sun_spin;
        rectfill(spx, spy, spw, sph, sun_spin ? ACCENT : PAPER);
        rect(spx, spy, spw, sph, INK);
        print(sun_spin ? "spin on" : "spin off", spx + 4, spy + 5, sun_spin ? PAPER : INK);
        // drop-shadow toggle — a sun-driven cast shadow (edits the selection, else the new-stroke default)
        int sh_on = sel ? (sel->shadow > 0) : dropshadow;
        int shx = SUN_PX + 6, shy = SUN_PY + 56, shw = SUN_PW - 12, shh = 16;
        if (ui_button(shx, shy, shw, shh, 0)) { if (sel) sel->shadow = sel->shadow > 0 ? 0 : SHADOW_DIST; else dropshadow = !dropshadow; }
        rectfill(shx, shy, shw, shh, sh_on ? ACCENT : PAPER);
        rect(shx, shy, shw, shh, INK);
        print(sh_on ? "drop shadow on" : "drop shadow off", shx + 4, shy + 5, sh_on ? PAPER : INK);
    }

    // the gradient-fill popover (G): on/off, the FAR colour (the fill colour is the "from"),
    // spread + angle. Edits the selection if one is picked, else the new-stroke defaults.
    if (grad_open) {
        rectfill(GRAD_PX, GRAD_PY, GRAD_PW, GRAD_PH, PAPER);
        rect(GRAD_PX, GRAD_PY, GRAD_PW, GRAD_PH, INK);
        int g_on = sel ? sel->grad : grad;
        int tx = GRAD_PX + 6, ty = GRAD_PY + 6;
        if (ui_button(tx, ty, 78, 14, 0)) { if (sel) sel->grad = !sel->grad; else grad = !grad; }
        rectfill(tx, ty, 78, 14, g_on ? ACCENT : PAPER); rect(tx, ty, 78, 14, INK);
        print(g_on ? "gradient on" : "gradient off", tx + 4, ty + 4, g_on ? PAPER : INK);
        print("to", GRAD_PX + 90, ty + 4, INK);
        int gci = sel ? color_index(sel->grad_color) : gradsel;
        for (int i = 0; i < NCOLORS; i++) {
            int x = GRAD_PX + 106 + i * 15, y = ty;
            if (ui_button(x, y, 13, 14, 0)) { if (sel) sel->grad_color = COLORS[i]; else gradsel = i; }
            rectfill(x, y, 13, 14, COLORS[i]); rect(x, y, 13, 14, i == gci ? ACCENT : INK);
        }
        static float gsp; gsp = sel ? sel->grad_spread : grad_spread;
        if (ui_slider(&gsp, GRAD_PX + 6, GRAD_PY + 32, 220, "spread (solid ends <-> even ramp)"))
            { if (sel) sel->grad_spread = gsp; else grad_spread = gsp; }
        rect(GRAD_PX + 6, GRAD_PY + 33, 220, 6, CLR_LIGHT_GREY);
        static float gan; gan = (sel ? sel->grad_angle : grad_angle) / 360.0f;
        if (ui_slider(&gan, GRAD_PX + 6, GRAD_PY + 54, 220, "angle")) {
            float a = gan * 360.0f; if (sel) sel->grad_angle = a; else grad_angle = a;
        }
        rect(GRAD_PX + 6, GRAD_PY + 55, 220, 6, CLR_LIGHT_GREY);
    }

    font(FONT_NORMAL);   // restore (the whole bar drew in FONT_SMALL)
    ui_end();
}

// ── feature × drawtool coverage self-test (SQUISHY_MATRIX=1) ──────────────────────
// A diagnostic scene, not part of the app: renders a GRID of every brush (rows) against
// every toggleable rim/fill feature (cols), each cell the SAME reference stroke with just
// that one feature on (col 0 = none = the baseline). `tools/squishy-features.js` dumps this
// and pixel-diffs each feature cell vs its baseline — a cell identical to baseline means the
// feature is a silent no-op for that brush (that's the bug class that hid drip×dither and
// nib×outline). Rows use TOOL_DISP order; cols are none/bevel/outline/shadow/dither/boil.
static int matrix_mode = 0;
enum { MC_NONE, MC_BEVEL, MC_OUTLINE, MC_SHADOW, MC_PATTERN, MC_GRAD, MC_BOIL, MC_COLS };
#define MTX_LW 40   // left label column width
#define MTX_HH 12   // top header row height

// fill a reusable stroke with a fixed reference path inside a cell (slow → fat, so a rim shows)
static void matrix_stroke(Stroke *st, int tool, int cx, int cy, int cw, int ch) {
    st->tool = tool; st->color = CLR_BLUE; st->pattern = 0;
    st->bevel = 0; st->boil = 0; st->boil_style = BOIL_WOBBLE; st->thick = 1.0f;
    st->nib_angle = NIB_ANGLE_DEF; st->outline = 0; st->outline_color = INK; st->shadow = 0;
    st->grad = 0; st->grad_color = CLR_RED; st->grad_angle = 0; st->grad_spread = GRAD_SPREAD_DEF;
    st->seed = 0x51A5u ^ (unsigned)(tool * 2654435761u);
    int n = 10; st->n = n;
    for (int i = 0; i < n; i++) {
        float t = i / (float)(n - 1);
        st->pts[i].x = cx + 9 + t * (cw - 18);
        st->pts[i].y = cy + ch - 6 - t * (ch - 12);
        st->pts[i].speed = 0.0f;   // standstill → widest the brush gets
        st->pts[i].thick = 1.0f;
    }
}

static void draw_matrix(void) {
    cls(PAPER);
    int cw = (SCREEN_W - MTX_LW) / MC_COLS, ch = (SCREEN_H - MTX_HH) / NTOOLS;
    static const char *fname[MC_COLS] = { "none", "bevl", "outl", "shdw", "dith", "grad", "boil" };
    font(FONT_TINY);
    for (int c = 0; c < MC_COLS; c++) print(fname[c], MTX_LW + c * cw + 2, 3, INK);
    for (int bi = 0; bi < NTOOLS; bi++) {
        int y = MTX_HH + bi * ch;
        print(TOOL_DISP[bi], 2, y + ch / 2 - 2, INK);
        for (int c = 0; c < MC_COLS; c++) {
            int x = MTX_LW + c * cw;
            rect(x, y, cw, ch, CLR_LIGHT_GREY);
            static Stroke ms; matrix_stroke(&ms, bi, x, y, cw, ch);
            unsigned vseed = 0;
            if      (c == MC_BEVEL)   ms.bevel = 2.0f;
            else if (c == MC_OUTLINE) ms.outline = OUTLINE_PX;
            else if (c == MC_SHADOW)  ms.shadow = 3.0f;
            else if (c == MC_PATTERN) ms.pattern = NPATTERNS / 2;
            else if (c == MC_GRAD)  { ms.grad = 1; ms.grad_color = CLR_RED; ms.grad_spread = 0.6f; }
            else if (c == MC_BOIL)  { ms.boil = 1.0f; vseed = VARIANT[1]; }   // a jittered variant so it differs from still
            clip(x + 1, y + 1, cw - 2, ch - 2);   // keep each cell's rim/shadow from spilling into its neighbour
            Boil b = boil_for(&ms, vseed);
            draw_one(&ms, &b);
        }
    }
    clip(0, 0, 0, 0);
    font(FONT_NORMAL);
}

void init(void) {
    matrix_mode = getenv("SQUISHY_MATRIX") != 0;   // coverage self-test scene (diagnostic)
    mouse_hide();               // we draw our own brush-size ring as the cursor
    colorkey(CLR_BLACK);        // icon sprites use black (idx 0) as their bg — key it out so the
                                // ink-on-paper glyphs read on the paper/peach cell (esp. sketch)
}

void update(void) {
    if (matrix_mode) return;   // the coverage self-test drives no input
    float mx = mouse_x(), my = mouse_y();

    // per-frame pointer speed, EMA-smoothed so one jittery frame can't spike width
    float fdx = mx - prevx, fdy = my - prevy;
    float fspeed = sqrtf(fdx * fdx + fdy * fdy);
    ema += (fspeed - ema) * 0.4f;
    prevx = mx; prevy = my;

    // the bevel "sun" can auto-rotate (\ toggles) — advance it every frame, in any mode
    if (keyp('\\')) sun_spin = !sun_spin;
    if (sun_spin) { bevel_angle += SUN_SPIN_SPEED; if (bevel_angle >= 360.0f) bevel_angle -= 360.0f; }

    if (keyp('G')) { grad_open = !grad_open; dd_open = sun_open = 0; }   // G opens the gradient popover

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
    // sun popover is modal too: the dial drag / swatches / spin are driven in draw_panel;
    // here we just block canvas drawing and dismiss on a tap outside the popover.
    if (sun_open) {
        if (mouse_pressed(MOUSE_LEFT)) {
            int in_pop = mx >= SUN_PX && mx < SUN_PX + SUN_PW && my >= SUN_PY && my < SUN_PY + SUN_PH;
            if (my >= PANEL_H && !in_pop) sun_open = 0;      // tap-away dismiss
        }
        return;
    }
    // gradient popover is modal too (widgets driven in draw_panel; here just block + dismiss)
    if (grad_open) {
        if (mouse_pressed(MOUSE_LEFT)) {
            int in_pop = mx >= GRAD_PX && mx < GRAD_PX + GRAD_PW && my >= GRAD_PY && my < GRAD_PY + GRAD_PH;
            if (my >= PANEL_H && !in_pop) grad_open = 0;
        }
        return;
    }

    if (keyp('S')) selmode = !selmode;   // S toggles SELECT mode (also a bar button)

    // SELECT mode: a canvas press picks the nearest stroke (or deselects on empty), never draws.
    // While editing, clicks in the property strip drive its sliders — not a re-pick.
    if (selmode) {
        int in_prop = (selected >= 0 && selected < nstrokes) && my < PANEL_H + PROP_H;
        if (mouse_pressed(MOUSE_LEFT) && my >= PANEL_H && !in_prop) selected = pick_stroke(mx, my);
        if (keyp('U') || mouse_pressed(MOUSE_RIGHT)) do_undo();
        if (keyp('C')) { nstrokes = 0; selected = -1; }
        if ((keyp('[') || keyp(']')) && selected >= 0 && selected < nstrokes)
            strokes[selected].nib_angle += keyp(']') ? 15.0f : -15.0f;   // rotate the picked nib
        if (keyp(',') || keyp('.')) bevel_angle += keyp('.') ? 15.0f : -15.0f;   // rotate the bevel sun
        if (keyp('/')) light_sel = (light_sel + 1) % NLIGHTS;                    // cycle the sun colour
        return;
    }

    // mouse WHEEL rides the width live — scroll to set the brush size, or scroll
    // mid-stroke to taper/swell it (captured per point, so the change stays where you made it).
    float wh = mouse_wheel();
    if (wh != 0) {
        pressure += wh * PRESSURE_STEP;
        if (pressure < PRESSURE_MIN) pressure = PRESSURE_MIN;
        if (pressure > PRESSURE_MAX) pressure = PRESSURE_MAX;
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
        cur.nib_angle = nib_angle;   // capture the calligraphy nib angle (K_NIB uses it)
        cur.outline = outline ? OUTLINE_PX : 0.0f;   // capture the outline default
        cur.outline_color = COLORS[outsel];
        cur.shadow = dropshadow ? SHADOW_DIST : 0.0f;   // capture the drop-shadow default
        cur.grad = grad;                          // capture the gradient-fill default + its colour/angle/spread
        cur.grad_color = COLORS[gradsel];
        cur.grad_angle = grad_angle;
        cur.grad_spread = grad_spread;
        seedctr = seedctr * 1103515245u + 12345u;
        cur.seed = seedctr;
        lastsx = mx; lastsy = my;
        cur.pts[cur.n++] = (Sample){ mx, my, ema, pressure };
    }
    if (drawing && mouse_down(MOUSE_LEFT)) {
        float dsx = mx - lastsx, dsy = my - lastsy;
        if (sqrtf(dsx * dsx + dsy * dsy) >= MIN_SPACING && cur.n < MAX_SAMPLES) {
            cur.pts[cur.n++] = (Sample){ mx, my, ema, pressure };
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
    if (keyp('[') || keyp(']')) nib_angle += keyp(']') ? 15.0f : -15.0f;   // rotate the nib default
    if (keyp(',') || keyp('.')) bevel_angle += keyp('.') ? 15.0f : -15.0f; // rotate the bevel "sun"
    if (keyp('/')) light_sel = (light_sel + 1) % NLIGHTS;                  // cycle the sun colour
}

void draw(void) {
    if (matrix_mode) { draw_matrix(); return; }   // SQUISHY_MATRIX coverage self-test scene
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

    // cursor: hand over the panel or an open modal (dropdown / sun popover), arrow in
    // SELECT mode, our brush ring while drawing.
    int want = (mouse_y() < PANEL_H || dd_open || sun_open || grad_open) ? CURSOR_HAND
             : selmode ? CURSOR_DEFAULT
             : -1;   // -1 = hide OS cursor, draw the brush ring
    if (want != cursor_panel) {
        cursor_panel = want;
        if (want < 0) mouse_hide();
        else { mouse_cursor(want); mouse_show(); }
    }
    if (want < 0) {   // cursor: nib edge for calligraphy, else the brush ring
        int mx = mouse_x(), my = mouse_y();
        if (BRUSHES[tool].kind == K_NIB) {   // preview the nib edge + its angle (wheel pressure folded in)
            float hw = BRUSHES[tool].maxw * thickness() * pressure * 0.5f;
            float nx = cos_deg(nib_angle) * hw, ny = sin_deg(nib_angle) * hw;
            line((int)(mx-nx+.5f),(int)(my-ny+.5f),(int)(mx+nx+.5f),(int)(my+ny+.5f), COLORS[colsel]);
        } else {   // brush ring = the brush's NOMINAL caliber (its fattest, at a standstill), so the
            int r;  // cursor only resizes when YOU change thickness/wheel — not as you move the mouse
            if (BRUSHES[tool].kind == K_SKETCH) r = (int)SKETCH_R;
            else r = (int)(BRUSHES[tool].maxw * thickness() * pressure * 0.5f + 0.5f);
            if (r < 1) r = 1;
            circ(mx, my, r, COLORS[colsel]);
        }
    }

    draw_panel();   // on top of the strokes
}
