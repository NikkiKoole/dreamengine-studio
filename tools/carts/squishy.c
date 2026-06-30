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
    "summary": "Draw with a velocity-sensitive ink brush — lines swell when you go slow, thin out when you go fast, and taper to a point at each end.",
    "detail": "Every stroke is stored as DATA (a path of points + the speed you drew each one at), not painted straight to pixels. The ink brush renders that path as a chain of overlapping round stamps whose width = a slow→fat / fast→thin speed curve × an end-taper × a little seeded wobble. The bevel toggle embosses each stroke into a faux-3D rim (light from the top-left): a SHADOW + HILITE copy offset under the ink body leave a light rim on the upper-left edge and a dark rim on the lower-right. Storing strokes as data is what makes the planned 'boil' mode (re-render with fresh jitter → a living loop) almost free later. v1 is the ink brush + bevel + a 2-tone ink-on-paper palette; pencil/fineliner/marker and boil come next.",
    "controls": "Drag to ink. B toggles the bevel (faux-3D emboss). Right-click (or U) undoes the last stroke. C clears."
  },
  "todo": [
    "Add the tool palette: pencil / fineliner / marker (the brush table in docs/design/squishy-lines.md).",
    "Add the opt-in boil mode (N cached frames, re-render with per-frame seed, cycle ~6-8fps).",
    "Cache finished strokes to a layer buffer instead of re-rendering every stroke every frame.",
    "spec(): same-seed determinism + jitter-bounds."
  ]
}
de:meta */
#include "studio.h"
#include <math.h>

// ── SQUISHY LINES ─────────────────────────────────────────────────────────
// A velocity-sensitive ink brush. The whole soul of the tool is in one idea:
// a stroke is DATA (a path of points, each carrying the speed you drew it at),
// and rendering is a PURE FUNCTION of (that data + a seed). Draw slow → the
// line swells; flick fast → it thins; lift off → it tapers to a point. A bit
// of seeded per-stamp wobble keeps it from ever looking mechanical.
//
// Because the strokes are data (not baked pixels), the planned "boil" mode —
// re-render every stroke with a fresh per-frame seed and cycle 2–3 of them —
// is almost free. v1 is just the ink brush, in 2-tone ink-on-paper.
//
// See docs/design/squishy-lines.md for the full plan (brush table, bevel
// tool, boil, the draw-big → pixelsnap → animated-sprite pipeline).

// 2-tone: warm cream paper, soft inky brown-black (the tinyjam 1-bit target).
#define PAPER  CLR_WHITE           // #fff1e8
#define INK    CLR_BROWNISH_BLACK  // #291814

// bevel tones — the faux-3D rim. Light comes from the top-LEFT (the DPaint
// convention): a light rim on the upper-left edge, a dark rim on the lower-right.
#define HILITE    CLR_LIGHT_PEACH  // #ffccaa — the lit (top-left) rim
#define SHADOW    CLR_BLACK        // #000000 — the shaded (bottom-right) rim, darker than ink
#define BEVEL_OFF 1.0f             // px the rims peek out past the ink body

#define MAX_STROKES 128
#define MAX_SAMPLES 1500   // a long continuous stroke (a spiral fill, a long contour) fits

// brush feel — the numbers that make the squish. Tune by drawing.
#define MIN_W        1.8f   // width at full speed (the thin flick — still has ink body)
#define MAX_W        9.0f   // width at a standstill (the fat dwell)
#define SPEED_REF    9.0f   // px/frame at which width bottoms out
#define TAPER_FRAC   0.14f  // fraction of the stroke tapered at each end
#define NOISE_AMT    0.15f  // ±15% per-stamp width wobble
#define MIN_SPACING  1.6f   // px between captured path points
#define STAMP_SPACING 0.7f  // px between stamps when rendering a segment (dense = solid)

typedef struct { float x, y, speed; } Sample;
typedef struct {
    unsigned seed;          // per-stroke seed → deterministic wobble (boil reseeds this)
    int      n;
    Sample   pts[MAX_SAMPLES];
} Stroke;

static Stroke   strokes[MAX_STROKES];
static int      nstrokes = 0;
static Stroke   cur;                 // the stroke being drawn right now
static int      drawing = 0;
static int      bevel = 0;           // bevel mode: emboss each stroke into a faux-3D rim
static float    prevx, prevy;        // last frame's pointer (for per-frame speed)
static float    lastsx, lastsy;      // last *captured sample* (for min-spacing)
static float    ema = 0;             // smoothed pointer speed
static unsigned seedctr = 0x1234567u;

// integer hash → [0,1); deterministic, no global rnd() state (boil-friendly).
static unsigned hashu(unsigned x){ x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x; }
static float    hashf(unsigned x){ return (hashu(x) & 0xFFFFFF) / (float)0x1000000; }

// width of the brush at sample i — the pure function: speed curve × taper × wobble.
static float sample_width(const Stroke *s, int i, unsigned fseed) {
    int n = s->n;
    float t = (n > 1) ? (float)i / (n - 1) : 0.5f;           // 0..1 along the stroke
    float taper = fminf(t / TAPER_FRAC, (1.0f - t) / TAPER_FRAC);
    if (taper > 1) taper = 1; if (taper < 0) taper = 0;
    taper = taper * taper * (3 - 2 * taper);                  // smoothstep the ends
    float sp = s->pts[i].speed / SPEED_REF;
    if (sp > 1) sp = 1; if (sp < 0) sp = 0;
    float wspeed = MAX_W - (MAX_W - MIN_W) * sp;              // slow = fat, fast = thin
    float wobble = 1.0f + (hashf(s->seed ^ fseed ^ (unsigned)(i * 2654435761u)) * 2 - 1) * NOISE_AMT;
    return wspeed * taper * wobble;
}

static void stamp(float x, float y, float w, int color) {
    if (w <= 0) return;
    int ix = (int)(x + 0.5f), iy = (int)(y + 0.5f);
    float r = w * 0.5f;
    if (r < 0.75f) pset(ix, iy, color);
    else circfill(ix, iy, (int)(r + 0.5f), color);
}

// render a whole stroke as a chain of overlapping round stamps (stamp-spacing),
// offset by (ox,oy) and in `color` — the offset is what the bevel uses to peek
// a rim out from under the ink body.
static void render_stroke(const Stroke *s, unsigned fseed, float ox, float oy, int color) {
    if (s->n == 0) return;
    if (s->n == 1) { stamp(s->pts[0].x + ox, s->pts[0].y + oy, sample_width(s, 0, fseed), color); return; }
    for (int i = 0; i < s->n - 1; i++) {
        float x0 = s->pts[i].x,   y0 = s->pts[i].y;
        float x1 = s->pts[i+1].x, y1 = s->pts[i+1].y;
        float w0 = sample_width(s, i, fseed), w1 = sample_width(s, i + 1, fseed);
        float dx = x1 - x0, dy = y1 - y0;
        float seg = sqrtf(dx * dx + dy * dy);
        int steps = (int)(seg / STAMP_SPACING);
        if (steps < 1) steps = 1;
        for (int k = 0; k <= steps; k++) {
            float u = (float)k / steps;
            stamp(x0 + dx * u + ox, y0 + dy * u + oy, w0 + (w1 - w0) * u, color);
        }
    }
}

// draw one stroke, with the bevel emboss if it's on. The trick: lay a SHADOW
// copy offset toward the dark side and a HILITE copy offset toward the light,
// then the INK body on top — the ink covers the centre, leaving a 1px light rim
// on the upper-left and a dark rim on the lower-right. Pure geometry, no canvas
// read-back, so it stays deterministic (and boil-ready).
static void draw_one(const Stroke *s, unsigned fseed) {
    if (bevel) {
        render_stroke(s, fseed,  BEVEL_OFF,  BEVEL_OFF, SHADOW);   // lower-right shade
        render_stroke(s, fseed, -BEVEL_OFF, -BEVEL_OFF, HILITE);   // upper-left light
    }
    render_stroke(s, fseed, 0, 0, INK);                            // ink body on top
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

    if (mouse_pressed(MOUSE_LEFT)) {
        drawing = 1;
        cur.n = 0;
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
    if (keyp('U') || mouse_pressed(MOUSE_RIGHT)) { if (nstrokes > 0) nstrokes--; }
    if (keyp('C')) nstrokes = 0;
    if (keyp('B')) bevel = !bevel;
}

void draw(void) {
    cls(PAPER);
    for (int i = 0; i < nstrokes; i++) draw_one(&strokes[i], strokes[i].seed);
    if (drawing) draw_one(&cur, cur.seed);

    // brush-size ring = the cursor; previews the live width (slow = big, fast = small)
    float sp = ema / SPEED_REF; if (sp > 1) sp = 1;
    int r = (int)((MAX_W - (MAX_W - MIN_W) * sp) * 0.5f + 0.5f);
    if (r < 1) r = 1;
    circ(mouse_x(), mouse_y(), r, INK);

    print(bevel ? "drag to ink   B bevel:ON   right-click undo   C clear"
                : "drag to ink   B bevel:off  right-click undo   C clear",
          6, SCREEN_H - 10, INK);
}
