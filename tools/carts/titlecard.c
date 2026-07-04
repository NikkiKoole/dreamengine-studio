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
#include <stdlib.h>   // getenv, atoi
#include <stdio.h>    // fopen/fgets
#include <string.h>   // str*

// ── content: an ordered list of sized lines (title/sub/body). Filled from a
//    params file named by $TITLECARD_PARAMS (written by the reel bake); falls
//    back to demo content when unset, so running it standalone still works. ────
enum { ROLE_TITLE, ROLE_SUB, ROLE_BODY };
#define MAXLINES 6
#define MAXTEXT  96
static struct { char text[MAXTEXT]; int role; } lines[MAXLINES];
static int NLINES = 0;

#define GAP 6                         // px between stacked lines
#define MARGIN 12                     // inset from the edge when pinned top/bottom (overlays)
#define MAGIC  CLR_GREEN              // the reserved key colour for overlays (bg magic) — #00e436,
                                      // far from the white ink + black shadow, colour-keyed at compose
enum { POS_TOP, POS_CENTER, POS_BOTTOM };
static int   card_bg  = CLR_DARKER_BLUE;   // standalone card background (or MAGIC for an overlay)
static int   card_pos = POS_CENTER;        // where the text stack sits
static float boil_amt = 1.0f;              // resting wobble intensity (0 = off), a ×BOIL_JIT multiplier
static float breathe_amt = 0.0f;           // resting grow/shrink intensity (0 = off), ×BREATHE_AMT
static int   loaded   = 0;
#define BREATHE_SPEED 2.5f                 // degrees of breath phase per frame (~2.4s per breath)
#define BREATHE_AMT   0.07f                // full-strength ±7% layout scale about the card centre

// role → font · integer scale (print_scaled) · line height. One font (the crisp
// 8×8 normal) scaled up for the hierarchy — chunky blocky pixel type; `body` drops
// to the 4×6 for small taglines/paragraphs.
static int role_font(int r)  { return r == ROLE_BODY ? FONT_SMALL : FONT_NORMAL; }
static int role_scale(int r) { return r == ROLE_TITLE ? 3 : 1; }
static int role_h(int r)     { return r == ROLE_TITLE ? 24 : r == ROLE_SUB ? 8 : 6; }

// ── in / hold / out timeline ─────────────────────────────────────────────────
// The card plays an IN transition (take `in_dur` s), rests (boil/breathe) for the
// middle, then an OUT transition (last `out_dur` s). Each end has its own effect:
// SLIDE = real motion from/to an edge; FADE = no positional move (the reel's xfade
// cut supplies the dissolve — there's no per-pixel alpha in the palette).
enum { ANIM_FADE, ANIM_SLIDE };
static float total_dur = 0.0f;                       // total on-screen secs (0 = unknown → no out)
static int   in_kind  = ANIM_SLIDE, in_edge  = EDGE_BOTTOM;  static float in_dur  = 0.5f;
static int   out_kind = ANIM_FADE,  out_edge = EDGE_TOP;     static float out_dur = 0.0f;   // 0 = no out

// "fade" | "slide <edge>" → kind + edge (edge = enters-FROM for in, exits-TO for out)
static void parse_effect(const char *v, int *kind, int *edge) {
    if (strncmp(v, "fade", 4) == 0) { *kind = ANIM_FADE; return; }
    *kind = ANIM_SLIDE;
    *edge = strstr(v, "top") ? EDGE_TOP : strstr(v, "left") ? EDGE_LEFT : strstr(v, "right") ? EDGE_RIGHT : EDGE_BOTTOM;
}
// add the off-screen offset for a slide at fraction `off` (0 = in place, 1 = fully off toward edge)
static void slide_off(int kind, int edge, float off, float *dx, float *dy) {
    if (kind != ANIM_SLIDE) return;
    if      (edge == EDGE_BOTTOM) *dy += off * SCREEN_H;
    else if (edge == EDGE_TOP)    *dy -= off * SCREEN_H;
    else if (edge == EDGE_LEFT)   *dx -= off * SCREEN_W;
    else                          *dx += off * SCREEN_W;
}

// add one content line (copied — the params buffer is reused)
static void add_line(int role, const char *text) {
    if (NLINES >= MAXLINES) return;
    strncpy(lines[NLINES].text, text, MAXTEXT - 1);
    lines[NLINES].text[MAXTEXT - 1] = 0;
    lines[NLINES].role = role;
    NLINES++;
}
static void demo_content(void) {
    NLINES = 0;
    add_line(ROLE_TITLE, "TINY JAM");
    add_line(ROLE_SUB,   "3 synths, one app");
}
// parse the params file ($TITLECARD_PARAMS): one `key value` per line —
//   title/sub/body <text> · anim slide <edge> | fade · bg <palette index>
static void load_params(void) {
    loaded = 1;
    const char *p = getenv("TITLECARD_PARAMS");
    FILE *f = p ? fopen(p, "r") : NULL;
    if (!f) { demo_content(); return; }
    NLINES = 0;
    char raw[256];
    while (fgets(raw, sizeof raw, f)) {
        raw[strcspn(raw, "\r\n")] = 0;
        if (!raw[0]) continue;
        char *sp = strchr(raw, ' ');
        const char *val = sp ? sp + 1 : "";
        int klen = sp ? (int)(sp - raw) : (int)strlen(raw);
        #define KEY(k) (klen == (int)sizeof(k) - 1 && strncmp(raw, k, klen) == 0)
        if      (KEY("title")) add_line(ROLE_TITLE, val);
        else if (KEY("sub"))   add_line(ROLE_SUB,   val);
        else if (KEY("body"))  add_line(ROLE_BODY,  val);
        else if (KEY("bg"))      card_bg = strcmp(val, "magic") == 0 ? MAGIC : atoi(val);
        else if (KEY("pos"))     card_pos = strstr(val, "top") ? POS_TOP : strstr(val, "bottom") ? POS_BOTTOM : POS_CENTER;
        else if (KEY("boil"))    boil_amt = (float)atof(val);
        else if (KEY("breathe")) breathe_amt = (float)atof(val);
        else if (KEY("dur"))     total_dur = (float)atof(val);
        else if (KEY("anim"))    parse_effect(val, &in_kind, &in_edge);   // back-compat: in effect
        else if (KEY("in"))  { in_dur  = (float)atof(val);  const char *e = strchr(val, ' '); if (e) parse_effect(e + 1, &in_kind,  &in_edge); }
        else if (KEY("out")) { out_dur = (float)atof(val);  const char *e = strchr(val, ' '); if (e) parse_effect(e + 1, &out_kind, &out_edge); }
        #undef KEY
    }
    fclose(f);
    if (NLINES == 0) demo_content();
}

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
// draw one line, centred on cx at top y, offset by the entrance slide (dx,dy). bf =
// breathe factor: letters' offset from cx is scaled by it (the horizontal half of the
// grow/shrink). per-letter boil jitter is added to the draw position only — the baseline
// advance is clean, so letters wobble in place without drifting.
static void draw_line(const char *s, int cx, int y, int scale, float dx, float dy, float bf) {
    unsigned variant = (frame() / BOIL_PERIOD) % BOIL_VARIANTS;
    int x = cx - text_width(s) * scale / 2;      // scaled width for centring
    int sh = scale;                              // drop-shadow offset scales with the type
    float jit = BOIL_JIT * scale * boil_amt;     // wobble proportional to glyph size × intensity
    for (int i = 0; s[i]; i++) {
        char ch[2] = { s[i], 0 };
        float jx = 0, jy = 0;
        if (variant > 0 && boil_amt > 0) {   // variant 0 holds the rest pose
            jx = (hashf((unsigned)i * 2654435761u ^ variant * 40503u ^ 0x11u) * 2 - 1) * jit;
            jy = (hashf((unsigned)i * 2654435761u ^ variant * 40503u ^ 0x22u) * 2 - 1) * jit;
        }
        int px = (int)(cx + (x - cx) * bf + dx + jx + 0.5f), py = y + (int)(dy + jy + 0.5f);
        print_scaled(ch, px + sh, py + sh, CLR_BLACK, scale);   // drop shadow
        print_scaled(ch, px,      py,      CLR_WHITE, scale);   // ink
        x += text_width(ch) * scale;                            // clean advance (unjittered)
    }
}

void draw(void) {
    if (!loaded) load_params();
    cls(card_bg);

    // stack height → placed top / centre / bottom (bottom+top are the overlay lower/upper-third)
    int total = 0;
    for (int i = 0; i < NLINES; i++) total += role_h(lines[i].role) + (i ? GAP : 0);
    int y = card_pos == POS_TOP    ? MARGIN
          : card_pos == POS_BOTTOM ? SCREEN_H - total - MARGIN
          :                          (SCREEN_H - total) / 2;

    // in / hold / out — offset the whole stack off-screen at each end (slide), rest in the middle
    float t = frame() / 60.0f;   // the bake runs the cart at 60fps
    float dx = 0, dy = 0;
    if (in_dur > 0 && t < in_dur) {                                   // IN: edge → place
        slide_off(in_kind, in_edge, 1.0f - ease_out(t / in_dur), &dx, &dy);
    } else if (out_dur > 0 && total_dur > 0 && t > total_dur - out_dur) {   // OUT: place → edge
        float left = (total_dur - t) / out_dur;                       // 1 → 0 across the out window
        slide_off(out_kind, out_edge, 1.0f - ease_out(left < 0 ? 0 : left), &dx, &dy);
    }
    // (FADE has no alpha in the palette → the text just appears/vanishes; the reel-level
    //  xfade cut at the part boundary supplies the actual dissolve. See the design doc.)

    // breathe: a slow ±BREATHE_AMT layout scale about the stack's vertical centre. Glyphs stay
    // integer-scaled (crisp); the block spreads/contracts — the cheap bitmap take on a scale pulse.
    float bf = 1.0f + sin_deg(frame() * BREATHE_SPEED) * BREATHE_AMT * breathe_amt;
    int cy = y + total / 2;
    for (int i = 0; i < NLINES; i++) {
        font(role_font(lines[i].role));
        int yb = (int)(cy + (y - cy) * bf + 0.5f);
        draw_line(lines[i].text, SCREEN_W / 2, yb, role_scale(lines[i].role), dx, dy, bf);
        y += role_h(lines[i].role) + GAP;
    }
}
