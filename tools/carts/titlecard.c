/* de:meta
{
  "title": "titlecard",
  "status": "active",
  "created": "2026-07-04",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": {
    "summary": "A kinetic pixel-type title card for the trailer builder - white text with a drop shadow that slides in and then BOILS (hand-drawn wobble) while it sits.",
    "detail": "The renderer behind the trailer builder's text cards (docs/design/trailer-builder.md). Content is a list of sized lines - title / sub / body - stacked and centred with lay.h; each line is drawn white with a 1px dark drop shadow (just print() twice, no new API). Two motion layers ride on top: an ENTRANCE that plays once (fade = just appear, or slide in from an edge, eased) and a continuous RESTING boil ported from the squishy cart - every ~8 frames (~7.5fps, the hand-inked-cel cadence) each letter is nudged a pixel by seeded noise, so the type stays alive instead of dead-on-arrival. Deterministic and nearly free: the whole thing is lay.h + print + a hash, zero engine changes. This slice draws its own dark background (a standalone card, a full part in a reel); the overlay case - clearing to a magic colour and colour-keying it over gameplay - is the next slice. Content/style/anim are hard-coded demo values here; wiring them to the .reel grammar (@card ... | title \"...\" | anim slide bottom) comes with the build plumbing.",
    "controls": "Watch it: the card slides up, settles, and keeps boiling. No input."
  }
}
de:meta */
// TITLECARD — kinetic pixel type for the trailer builder's text cards.
//
// Design: docs/design/trailer-builder.md §"Text cards + overlays". Slice 1 =
// standalone card (own dark bg), white text + drop shadow, fade/slide entrance,
// squishy-style resting boil. All existing API — lay.h + print + a hash.

#include "studio.h"
#include "lay.h"

// ── content: an ordered list of sized lines (title/sub/body). Demo values;
//    the .reel grammar feeds these later. ────────────────────────────────────
enum { ROLE_TITLE, ROLE_SUB, ROLE_BODY };
typedef struct { const char *text; int role; } Line;
static const Line lines[] = {
    { "TINY JAM",          ROLE_TITLE },
    { "3 synths, one app", ROLE_SUB   },
};
static const int NLINES = sizeof(lines) / sizeof(lines[0]);

#define GAP 6                 // px between stacked lines
#define BG  CLR_DARKER_BLUE   // standalone card background (deep navy so the white + shadow read)

// role → font · integer scale (print_scaled) · line height. One font (the crisp
// 8×8 normal) scaled up for the hierarchy — chunky blocky pixel type; `body` drops
// to the 4×6 for small taglines/paragraphs.
static int role_font(int r)  { return r == ROLE_BODY ? FONT_SMALL : FONT_NORMAL; }
static int role_scale(int r) { return r == ROLE_TITLE ? 3 : 1; }
static int role_h(int r)     { return r == ROLE_TITLE ? 24 : r == ROLE_SUB ? 8 : 6; }

// ── entrance (plays once) ────────────────────────────────────────────────────
enum { ENTER_FADE, ENTER_SLIDE };
static const int ENTER_KIND  = ENTER_SLIDE;
static const int ENTER_EDGE  = EDGE_BOTTOM;   // slide in FROM this edge
#define ENTER_FRAMES 42                        // ~0.7s at 60fps

// ── resting boil — ported from squishy.c (WOBBLE): 3 seeded variants held ~8
//    frames (~7.5fps), each letter nudged ±BOIL_JIT px. Variant 0 = rest. ──────
#define BOIL_VARIANTS 3
#define BOIL_PERIOD   8
#define BOIL_JIT      1.0f

// cheap deterministic hash → [0,1)
static float hashf(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return (x & 0xffffff) / (float)0x1000000;
}

// draw one line, centred on cx at top y, offset by the entrance slide (dx,dy).
// per-letter boil jitter is added to the draw position only — the baseline
// advance is clean, so letters wobble in place without drifting.
static void draw_line(const char *s, int cx, int y, int scale, float dx, float dy) {
    unsigned variant = (frame() / BOIL_PERIOD) % BOIL_VARIANTS;
    int x = cx - text_width(s) * scale / 2;      // scaled width for centring
    int sh = scale;                              // drop-shadow offset scales with the type
    float jit = BOIL_JIT * scale;                // wobble proportional to glyph size
    for (int i = 0; s[i]; i++) {
        char ch[2] = { s[i], 0 };
        float jx = 0, jy = 0;
        if (variant > 0) {   // variant 0 holds the rest pose
            jx = (hashf((unsigned)i * 2654435761u ^ variant * 40503u ^ 0x11u) * 2 - 1) * jit;
            jy = (hashf((unsigned)i * 2654435761u ^ variant * 40503u ^ 0x22u) * 2 - 1) * jit;
        }
        int px = x + (int)(dx + jx + 0.5f), py = y + (int)(dy + jy + 0.5f);
        print_scaled(ch, px + sh, py + sh, CLR_BLACK, scale);   // drop shadow
        print_scaled(ch, px,      py,      CLR_WHITE, scale);   // ink
        x += text_width(ch) * scale;                            // clean advance (unjittered)
    }
}

void draw(void) {
    cls(BG);

    // stack height → vertical centre
    int total = 0;
    for (int i = 0; i < NLINES; i++) total += role_h(lines[i].role) + (i ? GAP : 0);
    int y = (SCREEN_H - total) / 2;

    // entrance: eased 0→1 over ENTER_FRAMES, then held at 1
    float p = frame() < ENTER_FRAMES ? ease_out((float)frame() / ENTER_FRAMES) : 1.0f;
    float dx = 0, dy = 0;
    if (ENTER_KIND == ENTER_SLIDE) {
        float off = 1.0f - p;   // travel a full screen dimension → the text CLEARLY starts off-screen
        if      (ENTER_EDGE == EDGE_BOTTOM) dy =  off * SCREEN_H;
        else if (ENTER_EDGE == EDGE_TOP)    dy = -off * SCREEN_H;
        else if (ENTER_EDGE == EDGE_LEFT)   dx = -off * SCREEN_W;
        else                                dx =  off * SCREEN_W;
    }
    // (ENTER_FADE has no alpha in the palette → the text just appears + boils;
    //  the reel-level xfade cut supplies the actual dissolve. See the design doc.)

    for (int i = 0; i < NLINES; i++) {
        font(role_font(lines[i].role));
        draw_line(lines[i].text, SCREEN_W / 2, y, role_scale(lines[i].role), dx, dy);
        y += role_h(lines[i].role) + GAP;
    }
}
