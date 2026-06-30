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
    "detail": "Every stroke is stored as DATA (a path of points + the speed you drew each one at), not painted straight to pixels. Most brushes render that path as a chain of overlapping round stamps whose width = a slow→fat / fast→thin speed curve × an end-taper × a little seeded wobble — ink / pencil / fineliner / marker / chalk are different sets of those numbers (chalk drops stamps for a dry, broken grain). Three brushes render specially: SKETCH (a hairy web of threads, à la Krita's Sketch engine), SPRAY (an airbrush dot-cloud whose spread follows speed), and BRISTLE (raked parallel hairs). The bevel toggle embosses strokes into a faux-3D rim (light from the top-left). And because rendering is a pure function of (stroke, seed), the BOIL toggle re-renders through a few jittered seeds and cycles them ~7.5fps → the whole drawing breathes, hand-drawn-animation style, for almost free. 2-tone ink-on-paper; a richer palette + the pixelsnap animated-icon export come next.",
    "controls": "Top panel: open the tool dropdown (ink/pen/fineliner/marker/chalk/sketch/spray/bristle), drag the thickness slider, toggle BVL (bevel) and BOIL (living wobble), tap UNDO. Then drag on the canvas to draw. Keys: B bevel, O boil, U undo, C clear."
  },
  "todo": [
    "Swap the text tool-buttons for code-drawn glyph sprites (sprite-draw.js) per docs/design/squishy-lines.md.",
    "Cache finished strokes + the boil frames to layer buffers instead of re-rendering every stroke every frame.",
    "The pixelsnap animated-icon export: boil frames → pixelsnap → an animated sprite strip.",
    "spec(): same-seed determinism + jitter-bounds."
  ]
}
de:meta */
#include "studio.h"
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
#define BEVEL_OFF 1.0f             // px the rims peek out past the ink body

#define PANEL_H   24               // top tool-bar height; the canvas is below it

// boil: the opt-in living loop. Re-render the committed strokes through a few
// jittered variants and cycle them slowly — the classic hand-drawn wobble.
#define BOIL_FRAMES 3              // how many jittered variants the boil cycles
#define BOIL_PERIOD 8              // display frames each variant is held (~7.5fps)
#define BOIL_JIT    1.2f           // px of per-point wobble when boiling

#define MAX_STROKES 128
#define MAX_SAMPLES 1500           // a long continuous stroke (spiral fill, contour) fits

#define MIN_SPACING   1.6f         // px between captured path points
#define STAMP_SPACING 0.7f         // px between stamps when rendering a segment (dense = solid)

// how a tool draws its stored path:
enum { K_STAMP, K_CHALK, K_SKETCH, K_SPRAY, K_BRISTLE };

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
};
#define NTOOLS ((int)(sizeof(BRUSHES) / sizeof(BRUSHES[0])))
#define SKETCH_R     22.0f     // px: a thread links to prior points within this reach (à la Krita Sketch)
#define SKETCH_R2    (SKETCH_R * SKETCH_R)
#define SKETCH_TRIES 5         // random prior points tried per sample → web density
#define SPRAY_DOTS   7         // dots scattered per path point (airbrush)
#define BRISTLE_HAIRS 5        // parallel hairs across the bristle width

typedef struct { float x, y, speed; } Sample;
typedef struct {
    unsigned seed;          // per-stroke seed → deterministic wobble (boil reseeds this)
    int      tool;          // which brush this stroke was drawn with
    float    thick;         // thickness multiplier this stroke was drawn with
    int      n;
    Sample   pts[MAX_SAMPLES];
} Stroke;

static const unsigned VARIANT[BOIL_FRAMES] = { 0u, 0x9e3779b9u, 0x85ebca6bu };

static Stroke   strokes[MAX_STROKES];
static int      nstrokes = 0;
static Stroke   cur;                 // the stroke being drawn right now
static int      drawing = 0;
static int      bevel = 0;           // bevel mode: emboss each stroke into a faux-3D rim
static int      boil = 0;            // boil mode: cycle jittered variants → a living loop
static int      cursor_panel = -1;   // tracks which cursor is shown (hand on bar / ring on canvas)
static int      tool = 0;            // selected brush
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

// render a stroke as a chain of overlapping round stamps, offset by (ox,oy) in
// `color` — (ox,oy) is the bevel rim offset; `jitter` adds the boil wobble.
static void render_stroke(const Stroke *s, unsigned fseed, float ox, float oy, int color, int jitter) {
    if (s->n == 0) return;
    int chalk = BRUSHES[s->tool].kind == K_CHALK;   // dry, broken: drop ~40% of stamps
    if (s->n == 1) {
        float jx = jitter ? boil_off(s->seed, 0, fseed, 0x11) : 0;
        float jy = jitter ? boil_off(s->seed, 0, fseed, 0x22) : 0;
        stamp(s->pts[0].x + ox + jx, s->pts[0].y + oy + jy, sample_width(s, 0, fseed), color);
        return;
    }
    for (int i = 0; i < s->n - 1; i++) {
        float x0 = s->pts[i].x,   y0 = s->pts[i].y;
        float x1 = s->pts[i+1].x, y1 = s->pts[i+1].y;
        if (jitter) {
            x0 += boil_off(s->seed, i,   fseed, 0x11); y0 += boil_off(s->seed, i,   fseed, 0x22);
            x1 += boil_off(s->seed, i+1, fseed, 0x11); y1 += boil_off(s->seed, i+1, fseed, 0x22);
        }
        float w0 = sample_width(s, i, fseed), w1 = sample_width(s, i + 1, fseed);
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
static void render_spray(const Stroke *s, unsigned fseed, int jitter) {
    for (int i = 0; i < s->n; i++) {
        float x = s->pts[i].x, y = s->pts[i].y;
        if (jitter) { x += boil_off(s->seed, i, fseed, 0x11); y += boil_off(s->seed, i, fseed, 0x22); }
        float rad = sample_width(s, i, fseed) * 0.5f + 1.0f;
        for (int k = 0; k < SPRAY_DOTS; k++) {
            unsigned h = hashu(s->seed ^ (unsigned)(i * 2654435761u) ^ (unsigned)(k * 0x9E3779B1u));
            float a = (h & 0xFFFF) / 65535.0f * 360.0f;               // angle, degrees
            float d = sqrtf(((h >> 16) & 0xFFFF) / 65535.0f) * rad;   // uniform over the disk
            pset((int)(x + cos_deg(a) * d + .5f), (int)(y + sin_deg(a) * d + .5f), INK);
        }
    }
}

// bristle: BRISTLE_HAIRS thin lines raked across the brush width (perpendicular
// to the stroke), each nudged a little so they're not mechanically parallel.
static void render_bristle(const Stroke *s, unsigned fseed, int jitter) {
    if (s->n < 2) return;
    for (int i = 0; i < s->n - 1; i++) {
        float x0 = s->pts[i].x, y0 = s->pts[i].y, x1 = s->pts[i+1].x, y1 = s->pts[i+1].y;
        if (jitter) {
            x0 += boil_off(s->seed, i,   fseed, 0x11); y0 += boil_off(s->seed, i,   fseed, 0x22);
            x1 += boil_off(s->seed, i+1, fseed, 0x11); y1 += boil_off(s->seed, i+1, fseed, 0x22);
        }
        float dx = x1 - x0, dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.01f) continue;
        float px = -dy / len, py = dx / len;     // perpendicular unit
        float w = (sample_width(s, i, fseed) + sample_width(s, i + 1, fseed)) * 0.5f;
        for (int h = 0; h < BRISTLE_HAIRS; h++) {
            unsigned hh = hashu(s->seed ^ (unsigned)(h * 2654435761u));
            float j = ((hh & 0xFFFF) / 65535.0f - 0.5f) * 0.35f;
            float off = ((h + 0.5f) / BRISTLE_HAIRS - 0.5f + j) * w;
            line((int)(x0 + px * off + .5f), (int)(y0 + py * off + .5f),
                 (int)(x1 + px * off + .5f), (int)(y1 + py * off + .5f), INK);
        }
    }
}

// the SKETCH brush (à la Krita's Sketch engine): a thin spine plus a hairy WEB —
// each point threads a 1px line to a few seeded-random prior points within reach.
// The web topology is keyed by the stroke seed ONLY (stable), while boil jitters
// the point positions (fseed) — so the whole tangle wiggles without re-wiring.
static void render_sketch(const Stroke *s, unsigned fseed, int jitter) {
    if (s->n < 2) return;
    for (int i = 1; i < s->n; i++) {
        float xi = s->pts[i].x, yi = s->pts[i].y;
        float xp = s->pts[i-1].x, yp = s->pts[i-1].y;
        if (jitter) {
            xi += boil_off(s->seed, i,   fseed, 0x11); yi += boil_off(s->seed, i,   fseed, 0x22);
            xp += boil_off(s->seed, i-1, fseed, 0x11); yp += boil_off(s->seed, i-1, fseed, 0x22);
        }
        line((int)(xi + .5f), (int)(yi + .5f), (int)(xp + .5f), (int)(yp + .5f), INK);  // spine
        for (int k = 0; k < SKETCH_TRIES; k++) {
            unsigned h = hashu(s->seed ^ (unsigned)(i * 2654435761u) ^ (unsigned)(k * 0x9E3779B1u));
            int j = (int)(h % (unsigned)i);                 // a prior point (topology = seed only)
            float xj = s->pts[j].x, yj = s->pts[j].y;
            if (jitter) { xj += boil_off(s->seed, j, fseed, 0x11); yj += boil_off(s->seed, j, fseed, 0x22); }
            float dx = xi - xj, dy = yi - yj;
            if (dx * dx + dy * dy < SKETCH_R2)
                line((int)(xi + .5f), (int)(yi + .5f), (int)(xj + .5f), (int)(yj + .5f), INK);
        }
    }
}

// draw one stroke. Sketch is its own renderer; the other brushes use the stamp
// chain, with the bevel emboss if it's on: a SHADOW + HILITE copy offset under
// the ink body leave a light rim on the upper-left, a dark rim on the lower-right.
// Pure geometry, no canvas read-back → deterministic. `jitter` propagates the boil
// wobble to all passes so the rim moves with the body.
static void draw_one(const Stroke *s, unsigned fseed, int jitter) {
    switch (BRUSHES[s->tool].kind) {
        case K_SKETCH:  render_sketch(s, fseed, jitter);  return;
        case K_SPRAY:   render_spray(s, fseed, jitter);   return;
        case K_BRISTLE: render_bristle(s, fseed, jitter); return;
        default: break;   // K_STAMP / K_CHALK use the stamp chain below
    }
    if (bevel) {
        render_stroke(s, fseed,  BEVEL_OFF,  BEVEL_OFF, SHADOW, jitter);
        render_stroke(s, fseed, -BEVEL_OFF, -BEVEL_OFF, HILITE, jitter);
    }
    render_stroke(s, fseed, 0, 0, INK, jitter);
}

static void do_undo(void) { if (nstrokes > 0) nstrokes--; }

#define DD_X   2
#define DD_W   50
#define DD_ITEM 16             // dropdown row height

// the top tool-bar: a tool dropdown, thickness, bevel, boil, undo.
static void draw_panel(void) {
    rectfill(0, 0, SCREEN_W, PANEL_H, PAPER);
    line(0, PANEL_H - 1, SCREEN_W, PANEL_H - 1, INK);
    ui_begin();
    // tool dropdown header (name of the current tool + a down caret)
    if (ui_button(DD_X, 3, DD_W, 16, str("%s v", BRUSHES[tool].name))) dd_open = !dd_open;
    ui_slider(&thick01, 58, 4, 48, "thk");          // thickness 0.4×..2.0×
    if (ui_button(110, 3, 30, 16, "bvl")) bevel = !bevel;
    if (bevel) rectfill(110, 19, 30, 2, ACCENT);
    if (ui_button(144, 3, 40, 16, "boil")) boil = !boil;
    if (boil) rectfill(144, 19, 40, 2, ACCENT);
    if (ui_button(188, 3, 40, 16, "undo")) do_undo();
    // the open dropdown list, over the canvas
    if (dd_open) {
        for (int i = 0; i < NTOOLS; i++) {
            int y = PANEL_H + i * DD_ITEM;
            if (ui_button(DD_X, y, DD_W, DD_ITEM, BRUSHES[i].name)) { tool = i; dd_open = 0; }
            if (i == tool) rectfill(DD_X, y, 2, DD_ITEM, ACCENT);   // mark the current tool
        }
    }
    ui_end();
}

void init(void) {
    mouse_hide();   // we draw our own brush-size ring as the cursor
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
            int in_list = mx >= DD_X && mx < DD_X + DD_W && my >= PANEL_H && my < PANEL_H + NTOOLS * DD_ITEM;
            if (my >= PANEL_H && !in_list) dd_open = 0;     // tap-away dismiss
        }
        return;                                             // no drawing while open
    }

    // a press that lands in the panel drives the panel, never the canvas
    if (mouse_pressed(MOUSE_LEFT) && my >= PANEL_H) {
        drawing = 1;
        cur.n = 0;
        cur.tool = tool;
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
    if (keyp('C')) nstrokes = 0;
    if (keyp('B')) bevel = !bevel;
    if (keyp('O')) boil = !boil;
}

void draw(void) {
    cls(PAPER);
    // boil: pick this beat's jittered variant; committed strokes wobble, the
    // stroke you're actively drawing stays put.
    unsigned vseed = boil ? VARIANT[(frame() / BOIL_PERIOD) % BOIL_FRAMES] : 0u;
    for (int i = 0; i < nstrokes; i++) draw_one(&strokes[i], strokes[i].seed ^ vseed, boil);
    if (drawing) draw_one(&cur, cur.seed, 0);

    // cursor: an OS hand over the tool-bar (so it's clearly clickable), our own
    // brush-size ring over the canvas. Only toggle on the transition (no flicker).
    int on_panel = mouse_y() < PANEL_H || dd_open;   // dropdown is modal → keep the hand cursor
    if (on_panel != cursor_panel) {
        cursor_panel = on_panel;
        if (on_panel) { mouse_cursor(CURSOR_HAND); mouse_show(); }
        else mouse_hide();
    }
    if (!on_panel) {   // brush ring previews the reach (sketch) or live width (slow = big, fast = small)
        int r;
        if (BRUSHES[tool].kind == K_SKETCH) r = (int)SKETCH_R;
        else {
            Brush b = BRUSHES[tool];
            float sp = ema / b.speedref; if (sp > 1) sp = 1;
            r = (int)((b.maxw - (b.maxw - b.minw) * sp) * thickness() * 0.5f + 0.5f);
        }
        if (r < 1) r = 1;
        circ(mouse_x(), mouse_y(), r, INK);
    }

    draw_panel();   // on top of the strokes
}
