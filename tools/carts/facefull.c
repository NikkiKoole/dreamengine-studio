/* de:meta
{
  "slug": "facefull",
  "title": "face mock — full paradigm (160x100)",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "probe"
  ],
  "teaches": [],
  "description": "A VIBE MOCKUP at 160x100 x4 that fits the WHOLE device-face paradigm (all five zones) in the candy-toy finish: (1) nav spine — play + view tabs + pattern + bpm, (2) always-live knobs, (3) the context display with flanking soft-keys where the slime MASCOT lives (the soul) beside a mini pattern, (4) the 16-step mode-switched grid with a running playhead, (5) the drum pads. Tests whether the full skeleton survives the harsh half-resolution. Draw-only + now() life (playhead + mascot bop). Mockups-first for vibe work."
}
de:meta */
#include "studio.h"

// FACEFULL — the whole five-zone device-face paradigm at 160x100, candy vibe.
// The experiment: does the full skeleton fit the harsh canvas and stay charming?

static void mnote(int x, int y, int col) { circfill(x, y + 3, 1, col); rectfill(x + 1, y, 1, 4, col); line(x + 2, y, x + 3, y - 1, col); }
static void heart(int cx, int cy, int col) { pset(cx - 1, cy - 1, col); pset(cx + 1, cy - 1, col); trifill(cx - 2, cy, cx + 2, cy, cx, cy + 2, col); }

static void knob(int cx, int cy, int r, float v, const char *label) {
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_PINK);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_PURPLE);
    circ(cx, cy, r, CLR_BROWNISH_BLACK);
    float a = 150 + v * 240;
    line(cx + (int)dx(1, a), cy + (int)dy(1, a), cx + (int)dx(r - 1, a), cy + (int)dy(r - 1, a), CLR_WHITE);
    font(FONT_TINY); print(label, cx - text_width(label) / 2, cy + r + 1, CLR_DARK_BROWN);
}
static void candy_pad(int x, int y, int w, int h, int face, int hi, int lo, const char *label, float env) {
    blend(BLEND_AVG); rrectfill(x + 1, y + 1, w, h, 2, CLR_BLACK); blend_reset();
    rrectfill(x, y, w, h, 2, face);
    if (env > 0.5f) { blend(BLEND_AVG); rrectfill(x + 2, y + 2, w - 4, h - 4, 1, CLR_WHITE); blend_reset(); }
    line(x + 2, y + 1, x + w - 3, y + 1, hi);
    blend(BLEND_AVG); line(x + 2, y + h - 1, x + w - 3, y + h - 1, lo); blend_reset();
    rrect(x, y, w, h, 2, CLR_BROWNISH_BLACK);
    if (label) { font(FONT_TINY); print(label, x + (w - text_width(label)) / 2, y + h / 2 - 2, CLR_BROWNISH_BLACK); }
}
static void chip(int x, int y, int w, const char *s, int on) {   // a soft-key / tab
    rrectfill(x, y, w, 8, 2, on ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    font(FONT_TINY); print(s, x + (w - text_width(s)) / 2, y + 1, on ? CLR_WHITE : CLR_LIGHT_PEACH);
}
static void mascot(int cx, int cy, float t) {
    int bop = (int)(sin_deg(t * 260) * 1.5f); cy += bop;
    ovalfill(cx, cy, 8, 7, CLR_GREEN);
    ovalfill(cx, cy + 1, 7, 5, CLR_LIME_GREEN);
    int blink = sin_deg(t * 90) > 0.85f;
    for (int e = -1; e <= 1; e += 2) { int ex = cx + e * 3, ey = cy - 1;
        if (blink) pset(ex, ey, CLR_BROWNISH_BLACK); else { circfill(ex, ey, 1, CLR_WHITE); pset(ex, ey, CLR_BROWNISH_BLACK); } }
    pset(cx - 4, cy + 1, CLR_PINK); pset(cx + 4, cy + 1, CLR_PINK);
    arc(cx, cy + 1, 2, 20, 160, CLR_DARK_GREEN);
    mnote(cx + 8, cy - 6 - bop, CLR_LIGHT_PEACH);
}

void draw(void) {
    float t = now();
    int step = ((int)(t * 8)) % 16;
    cls(CLR_DARK_PURPLE);
    rrectfill(0, 0, 160, 100, 7, CLR_INDIGO);
    rrectfill(3, 3, 154, 94, 5, CLR_LIGHT_PEACH);
    blend(BLEND_AVG); line(7, 4, 152, 4, CLR_WHITE); blend_reset();

    // ① NAV SPINE — play · view tabs · pattern · bpm · heart
    rrectfill(6, 7, 13, 9, 2, CLR_TRUE_BLUE); trifill(10, 9, 10, 14, 15, 11, CLR_WHITE);   // play
    chip(22, 7, 22, "DRUM", 1); chip(46, 7, 20, "SEQ", 0); chip(68, 7, 20, "MIX", 0);       // view tabs (1px up)
    font(FONT_TINY); print("PAT A", 92, 9, CLR_DARK_BROWN);
    print("124", 118, 9, CLR_DARK_GREEN); print("bpm", 118, 14, CLR_DARK_BROWN);
    heart(150, 11, CLR_RED);

    // ② KNOBS — always-live
    knob(18, 26, 6, 0.4f, "TUN"); knob(46, 26, 6, 0.6f, "DEC");
    knob(74, 26, 6, 0.3f, "TON"); knob(102, 26, 6, 0.7f, "SWG"); knob(130, 26, 6, 0.8f, "VOL");

    // ③ CONTEXT DISPLAY — soft-keys flank BOTH sides of the screen; the mascot lives here (soul)
    chip(6, 38, 16, "SEQ", 1); chip(6, 47, 16, "PAT", 0); chip(6, 56, 16, "MIX", 0);        // left soft-keys
    rrectfill(24, 37, 112, 24, 3, CLR_BROWNISH_BLACK);
    rrectfill(27, 39, 106, 20, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 40; y < 58; y += 2) line(27, y, 132, y, CLR_BROWNISH_BLACK); blend_reset();
    mascot(40, 49, t);                                                                       // the soul
    for (int r = 0; r < 4; r++) for (int s = 0; s < 16; s++)                                 // mini pattern peek
        pset(60 + s * 4, 42 + r * 4, s == step ? CLR_WHITE : (s % (r + 2) == 0) ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN);
    chip(138, 38, 16, "FX", 0); chip(138, 47, 16, "SCP", 0); chip(138, 56, 16, "SNG", 0);    // right soft-keys

    // ④ MODE-SWITCHED GRID — the 16-step row, playhead running
    for (int s = 0; s < 16; s++) {
        int x = 6 + s * 9, on = (s % 4 == 0) || (s == 6) || (s == 10);
        int here = s == step;
        int face = here ? CLR_WHITE : on ? (s % 4 == 0 ? CLR_ORANGE : CLR_LIME_GREEN) : CLR_DARKER_GREY;
        rrectfill(x, 65, 8, 9, 1, face);
        rrect(x, 65, 8, 9, 1, CLR_BROWNISH_BLACK);
    }

    // ⑤ PERFORMANCE SURFACE — the drum pads
    const char *pn[4] = { "KICK", "SNR", "HAT", "CLAP" };
    int pc[4] = { CLR_PINK, CLR_ORANGE, CLR_YELLOW, CLR_TRUE_BLUE };
    int ph[4] = { CLR_LIGHT_PEACH, CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW, CLR_BLUE };
    int pl[4] = { CLR_DARK_PURPLE, CLR_DARK_ORANGE, CLR_DARK_ORANGE, CLR_DARK_BLUE };
    int hitpad = ((step % 4) == 0) ? 0 : (step == 4 || step == 12) ? 1 : (step % 2 == 0) ? 2 : -1;
    for (int i = 0; i < 4; i++)
        candy_pad(6 + i * 37, 77, 34, 16, pc[i], ph[i], pl[i], pn[i], i == hitpad ? 1.0f : 0.0f);

    font(FONT_NORMAL);
}
