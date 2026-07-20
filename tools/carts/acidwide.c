/* de:meta
{
  "slug": "acidwide",
  "collection": ["device-face"],
  "title": "Tiny Acid Jam — wide 303 (mockup)",
  "status": "wip",
  "created": "2026-07-20",
  "kind": ["instrument"],
  "genre": null,
  "teaches": [],
  "resizable": true,
  "description": {
    "summary": "Draw-only LAYOUT STUDY: one 303 fully UNHIDDEN on a WIDE screen — everything that pages behind soft-keys on the phone (core knobs CUT/RES/ENV/DEC/ACC + the DF-extras SUB/ADEC/SLDT/TRK + FX DRV/SEND/VERB + DRIFT, and a full piano-roll sequencer with ACC/SLIDE/TIE lanes) shown at ONCE. Two arrangements to compare (keys 1/2): A = control RAIL left + big sequencer screen right; B = full knob ROW on top + one tall screen that absorbs the perform strip. Reflow, not scale.",
    "detail": "The bet: a phone 303 hides most of itself behind SEQ/FLAG/FX/... soft-keys because it only has height. Give it WIDTH and the whole TB setup fits unhidden — the sequencer stops being a 12px bottom strip and becomes the screen (pitch on Y, 16 steps on X, ACC/SLD/TIE lanes right under it). A puts the panel on a left rail (hardware feel); B keeps the familiar top knob-row and lets one tall screen eat the perform strip. Same pieces, two arrangements — the canvas-density-spectrum 'arrangement axis'. Chunky ~240x100 canvas (reflows to the window width).",
    "controls": "1 = arrangement A (rail + screen). 2 = arrangement B (knob-row + tall screen). Draw-only mockup: no audio, fake pattern."
  }
}
de:meta */
#include "studio.h"
#include "lay.h"

extern void de_resize(int w, int h);

// ── fake 303 pattern (16 steps) ──────────────────────────────────────────────
static const int p_on[16]  = { 1,1,0,1, 1,0,1,1, 1,1,0,1, 1,0,1,1 };
static const int p_pit[16] = { 0,12,0,7, 3,0,10,12, 5,8,0,15, 7,0,3,17 };   // semitones over root
static const int p_acc[16] = { 1,0,0,0, 1,0,0,1, 0,0,0,1, 1,0,0,0 };
static const int p_sld[16] = { 0,1,0,0, 0,0,1,0, 0,1,0,0, 0,0,1,0 };
static const int p_tie[16] = { 0,0,0,1, 0,0,0,0, 1,0,0,0, 0,0,0,1 };
#define PITCH_MAX 19

// full knob roster (all unhidden) — core-5, DF-extras, FX, drift
static const char *KN[13] = { "CUT","RES","ENV","DEC","ACC",  "SUB","ADEC","SLDT","TRK",  "DRV","SEND","VERB",  "DRIFT" };
static const float KV[13] = { 0.55,0.70,0.55,0.45,0.55,       0.30,0.40,0.14,0.20,        0.35,0.10,0.20,        0.50 };

static int arr = 0;    // 0 = A (rail), 1 = B (knob-row)
static int g_step = 0;

// ── candy widgets (static — this is a look mockup) ───────────────────────────
static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

static void dknob(int cx, int cy, int r, const char *label, float v) {
    float ang = 150 + v * 240;
    circfill(cx, cy, r, CLR_INDIGO);
    int rim = r / 4; if (rim < 1) rim = 1;
    ring(cx, cy, r - 1 - rim, r - 1, 150, 300, CLR_PINK);
    ring(cx, cy, r - 1 - rim, r - 1, -30, 120, CLR_DARKER_PURPLE);
    circ(cx, cy, r, CLR_BROWNISH_BLACK);
    line(cx + (int)dx(1, ang), cy + (int)dy(1, ang), cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), CLR_WHITE);
    font(FONT_TINY); plabel(label, cx, cy + r + 1, CLR_DARK_BROWN);
}
// a knob sized + centred in a cell (the height-keyed rule from acidcandy)
static void knob_cell(Box c, const char *label, float v) {
    float rh = c.h * 0.30f, rw = c.w * 0.42f;
    int r = (int)lay_clamp(rh < rw ? rh : rw, 4, 11);
    int cy = (int)(c.y + r + 1); if (cy + r + 7 > (int)(c.y + c.h)) cy = (int)(c.y + c.h) - r - 7;
    dknob((int)(c.x + c.w / 2), cy, r, label, v);
}
static void cbtn(Box b, const char *s, int on) {
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, on ? CLR_TRUE_BLUE : CLR_DARK_BROWN);
    rrect((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, CLR_BROWNISH_BLACK);
    font(FONT_TINY); print(s, (int)(b.x + (b.w - text_width(s)) / 2), (int)(b.y + (b.h - 5) / 2), on ? CLR_WHITE : CLR_LIGHT_PEACH);
}
static void lcdbtn(Box b, const char *s, int on) {
    if (on) rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, CLR_MEDIUM_GREEN);
    rrect((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, on ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN);
    font(FONT_TINY); print(s, (int)(b.x + (b.w - text_width(s)) / 2), (int)(b.y + (b.h - 5) / 2), on ? CLR_DARK_GREEN : CLR_LIME_GREEN);
}

// ── the big unhidden SCREEN: piano-roll + ACC/SLD/TIE flag lanes ─────────────
static void screen(Box scr, int with_lanes) {
    rrectfill((int)scr.x, (int)scr.y, (int)scr.w, (int)scr.h, 3, CLR_BROWNISH_BLACK);
    Box glass = lay_inset(scr, 2);
    rrectfill((int)glass.x, (int)glass.y, (int)glass.w, (int)glass.h, 2, CLR_DARK_GREEN);
    blend(BLEND_AVG);
    for (int y = (int)glass.y + 1; y < glass.y + glass.h - 1; y += 2) line((int)glass.x, y, (int)(glass.x + glass.w - 1), y, CLR_BROWNISH_BLACK);
    blend_reset();
    Box gc = lay_inset(glass, 2);

    // roll fills the glass; when with_lanes, the bottom third becomes ACC/SLD/TIE lanes
    Box roll = gc, lanes = gc;
    if (with_lanes) lanes = lay_split_gap(gc, EDGE_BOTTOM, gc.h * 0.34f, 2, &roll);
    float sw = roll.w / 16.0f;
    int base = (int)(roll.y + roll.h - 3);                    // root sits near the roll floor
    float ppx = (roll.h - 6) / (float)PITCH_MAX;              // px per semitone
    for (int s = 0; s < 16; s++) {
        int cx = (int)(roll.x + s * sw), cw = (int)sw - 1; if (cw < 2) cw = 2;
        if (s == g_step) { blend(BLEND_AVG); rectfill(cx, (int)roll.y, cw, (int)roll.h, CLR_MEDIUM_GREEN); blend_reset(); }
        if (!p_on[s]) { pset(cx + cw / 2, base, s % 4 == 0 ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN); continue; }
        int y = base - (int)(p_pit[s] * ppx);
        int col = p_acc[s] ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN;
        int w2 = p_tie[s] ? cw + (int)sw : cw;               // tie = extend into next step
        rectfill(cx, y, w2, 3, col);
        if (p_sld[s] && s < 15 && p_on[s + 1]) {              // slide = ramp to the next note
            int ny = base - (int)(p_pit[s + 1] * ppx);
            line(cx + cw, y + 1, (int)(roll.x + (s + 1) * sw), ny + 1, CLR_LIME_GREEN);
        }
    }
    // flag lanes: ACC / SLD / TIE, one thin row each
    static const char *LN[3] = { "ACC", "SLD", "TIE" };
    const int *LV[3] = { p_acc, p_sld, p_tie };
    const int LC[3] = { CLR_ORANGE, CLR_LIME_GREEN, CLR_LIGHT_YELLOW };
    if (with_lanes) for (int r = 0; r < 3; r++) {
        Box row = lay_cell(lanes, 1, 3, r, 1);
        font(FONT_TINY); print(LN[r], (int)row.x, (int)(row.y + (row.h - 5) / 2), CLR_MEDIUM_GREEN);
        float lsw = (row.w - 16) / 16.0f;
        for (int s = 0; s < 16; s++) {
            int cx = (int)(row.x + 16 + s * lsw), cw = (int)lsw - 1; if (cw < 2) cw = 2;
            if (LV[r][s]) rrectfill(cx, (int)row.y, cw, (int)row.h, 1, LC[r]);
            else pset(cx + cw / 2, (int)(row.y + row.h / 2), s % 4 == 0 ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN);
        }
    }
}

// soft-keys (the RARE pages that stay tucked away even when wide) + pattern chips
static void softkeys(Box c, int vertical) {
    static const char *SK[3] = { "GEN", "KEY", "PAT" };
    for (int i = 0; i < 3; i++) lcdbtn(vertical ? lay_cell(c, 1, 3, i, 2) : lay_cell(c, 0, 3, i, 2), SK[i], 0);
}

// the WAVE + VOICING switches that live at the knob-block's end
static void switches(Box c, int vertical) {
    Box a = vertical ? lay_cell(c, 1, 2, 0, 2) : lay_cell(c, 0, 2, 0, 2);
    Box b = vertical ? lay_cell(c, 1, 2, 1, 2) : lay_cell(c, 0, 2, 1, 2);
    cbtn(a, "SAW", 1); cbtn(b, "DF", 1);
}

static void navstrip(Box nav) {
    rrectfill((int)nav.x, (int)nav.y, (int)nav.w, (int)nav.h, 3, CLR_DARK_PURPLE);
    Box row = lay_inset(nav, 1);
    Box play = lay_split(row, EDGE_LEFT, 14, &row);
    Box home = lay_split(row, EDGE_RIGHT, 12, &row);
    cbtn(play, "", 0); { int cx = (int)(play.x + play.w / 2), cy = (int)(play.y + play.h / 2); trifill(cx - 2, cy - 3, cx - 2, cy + 3, cx + 3, cy, CLR_WHITE); }
    static const char *TAB[5] = { "303a", "303b", "808", "909", "MST" };
    static const int TC[5] = { CLR_PINK, CLR_ORANGE, CLR_TRUE_BLUE, CLR_ORANGE, CLR_GREEN };
    for (int m = 0; m < 5; m++) { Box t = lay_grid(row, 5, 5, m, 2);
        rrectfill((int)t.x, (int)t.y, (int)t.w, (int)t.h, 2, m == 0 ? TC[m] : CLR_DARK_PURPLE);
        rrect((int)t.x, (int)t.y, (int)t.w, (int)t.h, 2, m == 0 ? CLR_WHITE : CLR_BROWNISH_BLACK);
        font(FONT_TINY); plabel(TAB[m], (int)(t.x + t.w / 2), (int)(t.y + (t.h - 5) / 2), m == 0 ? CLR_BROWNISH_BLACK : TC[m]); }
    cbtn(home, "", 0);
}

// ── ARRANGEMENT A — control rail (left) + big sequencer screen (right) ───────
static void draw_A(Box body) {
    Box rail = lay_split_gap(body, EDGE_LEFT, body.w * 0.30f, 2, &body);
    Box scr  = body;
    // rail: 13 knobs in a 2-col grid (top), switches, then soft-keys at the bottom
    Box foot = lay_split_gap(rail, EDGE_BOTTOM, rail.h * 0.12f, 2, &rail);
    Box swr  = lay_split_gap(rail, EDGE_BOTTOM, rail.h * 0.12f, 2, &rail);
    for (int i = 0; i < 13; i++) knob_cell(lay_grid(rail, 3, 15, i, 2), KN[i], KV[i]);   // 3-wide grid (15 cells → last 2 blank)
    switches(swr, 0);
    softkeys(foot, 0);
    screen(scr, 1);
}

// ── ARRANGEMENT B — full knob ROW on top + one tall screen below ─────────────
static void draw_B(Box body) {
    Box krow = lay_split_gap(body, EDGE_TOP, body.h * 0.30f, 2, &body);
    Box scr  = body;                                              // the tall screen absorbs the perform strip
    Box endcol = lay_split_gap(krow, EDGE_RIGHT, krow.w * 0.14f, 2, &krow);   // WAVE/DF + soft-keys stack at the row end
    for (int i = 0; i < 13; i++) knob_cell(lay_grid(krow, 13, 13, i, 2), KN[i], KV[i]);
    Box sw = lay_split_gap(endcol, EDGE_TOP, endcol.h * 0.5f, 2, &endcol);
    switches(sw, 1);
    softkeys(endcol, 1);
    screen(scr, 1);
}

// the chunky always-there 16-cell STEP strip (pitch bar + accent marker + playhead)
static void steprow(Box c) {
    float sw = c.w / 16.0f;
    int base = (int)(c.y + c.h - 2);
    float ppx = (c.h - 6) / (float)PITCH_MAX;
    for (int s = 0; s < 16; s++) {
        int cx = (int)(c.x + s * sw), cw = (int)sw - 1; if (cw < 3) cw = 3;
        int hi = (s == g_step);
        rrectfill(cx, (int)c.y, cw, (int)c.h, 1, (s / 4) % 2 ? CLR_DARK_BROWN : CLR_BROWNISH_BLACK);   // 4-step shading
        if (p_on[s]) {
            int h = 3 + (int)(p_pit[s] * ppx), ty = base - h;
            rrectfill(cx + 1, ty, cw - 2, h, 1, hi ? CLR_WHITE : CLR_PINK);
            if (p_acc[s]) rectfill(cx + 1, ty - 3, cw - 2, 2, CLR_ORANGE);
            if (p_sld[s]) pset(cx + cw - 2, base - 1, CLR_LIME_GREEN);
        }
        rrect(cx, (int)c.y, cw, (int)c.h, 1, hi ? CLR_WHITE : CLR_BROWNISH_BLACK);
    }
}

// ── ARRANGEMENT C/D — knob block + display screen + a chunky 16-cell strip at the BOTTOM ──
// knobrows 1 = C (13 knobs in one row — crowded labels), 2 = D (7-wide grid → 2 rows, roomy labels).
static void draw_C(Box body, int knobrows) {
    float H = body.h;
    Box strip = lay_split_gap(body, EDGE_BOTTOM, H * 0.26f, 2, &body);        // the 16-cell step strip
    Box krow  = lay_split_gap(body, EDGE_TOP, H * (knobrows == 2 ? 0.36f : 0.28f), 2, &body);   // 2 rows need more height
    Box scr   = body;                                                         // the display (piano-roll, no lanes — the strip is the step editor)
    Box endcol = lay_split_gap(krow, EDGE_RIGHT, krow.w * 0.14f, 2, &krow);
    int cols = knobrows == 2 ? 7 : 13;
    for (int i = 0; i < 13; i++) knob_cell(lay_grid(krow, cols, 13, i, 2), KN[i], KV[i]);
    Box sw = lay_split_gap(endcol, EDGE_TOP, endcol.h * 0.5f, 2, &endcol);
    switches(sw, 1);
    softkeys(endcol, 1);
    screen(scr, 0);
    steprow(strip);
}

// the SCREEN as a note-POSITION editor: a pitch×step matrix (grid lines + notes placed by pitch).
// This is where you set the note, so the bottom strip needn't carry pitch as tall bars.
static void notegrid(Box scr) {
    rrectfill((int)scr.x, (int)scr.y, (int)scr.w, (int)scr.h, 3, CLR_BROWNISH_BLACK);
    Box glass = lay_inset(scr, 2);
    rrectfill((int)glass.x, (int)glass.y, (int)glass.w, (int)glass.h, 2, CLR_DARK_GREEN);
    Box inner = lay_inset(glass, 2);
    Box ruler = lay_split_gap(inner, EDGE_LEFT, 7, 1, &inner);   // left = pitch KEYBOARD (the Y axis, made obvious)
    Box gc = inner;
    float sw = gc.w / 16.0f;
    int base = (int)(gc.y + gc.h - 3);
    float ppx = (gc.h - 6) / (float)PITCH_MAX;
    int ph = (int)ppx < 1 ? 1 : (int)ppx;

    // beat-group column shading — every other group of 4 gets a faint band → reads as a step matrix
    blend(BLEND_AVG);
    for (int s = 0; s < 16; s++) if ((s / 4) % 2 == 0) rectfill((int)(gc.x + s * sw), (int)gc.y, (int)sw, (int)gc.h, CLR_BROWNISH_BLACK);
    // octave guide lines + step gridlines
    for (int p = 0; p <= PITCH_MAX; p += 12) { int y = base - (int)(p * ppx); line((int)gc.x, y, (int)(gc.x + gc.w - 1), y, CLR_MEDIUM_GREEN); }
    for (int s = 0; s <= 16; s++) { int gx = (int)(gc.x + s * sw); line(gx, (int)gc.y, gx, (int)(gc.y + gc.h - 1), s % 4 == 0 ? CLR_MEDIUM_GREEN : CLR_DARK_BROWN); }
    blend_reset();

    // left keyboard: white/black keys per semitone → the pitch axis reads at a glance
    rectfill((int)ruler.x, (int)ruler.y, (int)ruler.w, (int)ruler.h, CLR_BROWNISH_BLACK);
    for (int p = 0; p <= PITCH_MAX; p++) {
        int y = base - (int)(p * ppx), cls = p % 12;
        int black = (cls == 1 || cls == 3 || cls == 6 || cls == 8 || cls == 10);
        rectfill((int)ruler.x, y - ph + 1, (int)ruler.w - 1, ph, black ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN);
        if (cls == 0) rectfill((int)ruler.x, y, (int)ruler.w - 1, 1, CLR_LIME_GREEN);   // root-C tick
    }

    // playhead column
    { int cx = (int)(gc.x + g_step * sw); blend(BLEND_AVG); rectfill(cx, (int)gc.y, (int)sw, (int)gc.h, CLR_MEDIUM_GREEN); blend_reset(); }

    for (int s = 0; s < 16; s++) {
        int cx = (int)(gc.x + s * sw), cw = (int)sw - 1; if (cw < 3) cw = 3;
        if (!p_on[s]) { pset(cx + cw / 2, base, s % 4 == 0 ? CLR_MEDIUM_GREEN : CLR_DARK_GREEN); continue; }   // empty step = a faint "tap here" floor dot
        int y = base - (int)(p_pit[s] * ppx), playing = (s == g_step);
        int w2 = p_tie[s] ? cw + (int)sw : cw;
        if (p_sld[s] && s < 15 && p_on[s + 1]) { int ny = base - (int)(p_pit[s + 1] * ppx); line(cx + cw, y, (int)(gc.x + (s + 1) * sw), ny, CLR_LIME_GREEN); }   // slide ramp (under the block)
        if (playing) { blend(BLEND_AVG); rrectfill(cx, y - 3, w2, 7, 1, CLR_WHITE); blend_reset(); }   // playing-note glow
        int col = playing ? CLR_WHITE : p_acc[s] ? CLR_LIME_GREEN : CLR_LIGHT_YELLOW;
        rrectfill(cx + 1, y - 1, w2 - 1, 4, 1, col);                        // chunky note block (an object, not a bar)
        rrect(cx + 1, y - 1, w2 - 1, 4, 1, CLR_BROWNISH_BLACK);
        if (p_acc[s]) pset(cx + cw / 2, y - 3, CLR_ORANGE);                 // accent tick above the note
    }
}

// SHORT compact step strip — on/off + accent only (pitch lives in the note-grid above)
static void stepstrip_compact(Box c) {
    float sw = c.w / 16.0f;
    for (int s = 0; s < 16; s++) {
        int cx = (int)(c.x + s * sw), cw = (int)sw - 1; if (cw < 3) cw = 3;
        int hi = (s == g_step), on = p_on[s];
        rrectfill(cx, (int)c.y, cw, (int)c.h, 1, on ? (hi ? CLR_WHITE : CLR_PINK) : ((s / 4) % 2 ? CLR_DARK_BROWN : CLR_BROWNISH_BLACK));
        if (on && p_acc[s]) rectfill(cx + 1, (int)c.y + 1, cw - 2, 2, CLR_ORANGE);
        rrect(cx, (int)c.y, cw, (int)c.h, 1, hi ? CLR_WHITE : CLR_BROWNISH_BLACK);
    }
}

// ── ARRANGEMENT E — 1 knob row + DF/page nook · FULL-WIDTH note-grid · SHORT step strip ──
static void draw_E(Box body) {
    float H = body.h;
    Box strip = lay_split_gap(body, EDGE_BOTTOM, H * 0.13f, 2, &body);        // short compact cells
    Box krow  = lay_split_gap(body, EDGE_TOP,    H * 0.26f, 2, &body);        // one knob row
    Box scr   = body;                                                         // the full-width note-position editor (the star)
    Box tog   = lay_split_gap(krow, EDGE_RIGHT, krow.w * 0.15f, 2, &krow);    // toggling space (DF / page)
    for (int i = 0; i < 13; i++) knob_cell(lay_grid(krow, 13, 13, i, 2), KN[i], KV[i]);
    Box t1 = lay_split_gap(tog, EDGE_TOP, tog.h * 0.5f, 2, &tog);
    cbtn(t1, "DF", 1); cbtn(tog, "1/2", 0);                                   // voicing + view-page toggles
    notegrid(scr);
    stepstrip_compact(strip);
}

// the FLAG paint-palette — persistent (you always see what a tap/drag will paint). NOTE is armed.
static void flagrail(Box c) {
    static const char *FL[6] = { "NOTE", "ACC", "SLD", "TIE", "OC+", "OC-" };
    for (int i = 0; i < 6; i++) lcdbtn(lay_cell(c, 1, 6, i, 1), FL[i], i == 0);
}
// the RIGHT rail — PAT banks (always visible) over the two genuinely-occasional pages (KEY/GEN) + PERF
static void sidepages(Box c) {
    static const char *PT[4] = { "A", "B", "C", "D" };
    Box pat = lay_split_gap(c, EDGE_TOP, c.h * 0.42f, 2, &c);
    for (int i = 0; i < 4; i++) cbtn(lay_grid(pat, 2, 4, i, 1), PT[i], i == 0);
    static const char *PG[3] = { "KEY", "GEN", "PRF" };
    for (int i = 0; i < 3; i++) lcdbtn(lay_cell(c, 1, 3, i, 1), PG[i], 0);
}

// ── ARRANGEMENT F — the UNPAGED 303: flag palette · note-grid · PAT/pages rail · compact strip ──
static void draw_F(Box body) {
    float H = body.h;
    Box strip = lay_split_gap(body, EDGE_BOTTOM, H * 0.12f, 2, &body);        // compact step strip
    Box krow  = lay_split_gap(body, EDGE_TOP,    H * 0.34f, 2, &body);        // 2-row knobs (roomy labels)
    Box main  = body;
    Box nook  = lay_split_gap(krow, EDGE_RIGHT, krow.w * 0.14f, 2, &krow);
    for (int i = 0; i < 13; i++) knob_cell(lay_grid(krow, 7, 13, i, 2), KN[i], KV[i]);
    Box t1 = lay_split_gap(nook, EDGE_TOP, nook.h * 0.5f, 2, &nook);
    cbtn(t1, "DF", 1); cbtn(nook, "1/2", 0);
    Box frail = lay_split_gap(main, EDGE_LEFT,  main.w * 0.09f, 2, &main);    // paint palette (left)
    Box srail = lay_split_gap(main, EDGE_RIGHT, main.w * 0.14f, 2, &main);    // PAT + tucked pages (right)
    flagrail(frail);
    notegrid(main);
    sidepages(srail);
    stepstrip_compact(strip);
}

void update(void) {
    if (keyp('1')) arr = 0;
    if (keyp('2')) arr = 1;
    if (keyp('3')) arr = 2;
    if (keyp('4')) arr = 3;
    if (keyp('5')) arr = 4;
    if (keyp('6')) arr = 5;
    g_step = (int)(now() * 8) % 16;
}

void draw(void) {
    // chunky wide canvas — height ~100, width follows the window ratio (reflow, not scale)
    { int cw = screen_w(), ch = screen_h();
      if (cw > 0 && ch > 0) { float r = (float)cw / (float)ch;
          int tw = (int)(100.0f * r + 0.5f), th = 100;
          if (tw != cw || th != ch) de_resize(tw, th); } }

    cls(CLR_DARK_PURPLE);
    Box full = box(0, 0, screen_w(), screen_h());
    rrectfill(0, 0, (int)full.w, (int)full.h, 7, CLR_INDIGO);
    Box panel = lay_inset(full, 3);
    rrectfill((int)panel.x, (int)panel.y, (int)panel.w, (int)panel.h, 5, CLR_LIGHT_PEACH);

    Box body;
    Box nav = lay_split_gap(panel, EDGE_TOP, panel.h * 0.11f, 1, &body);
    navstrip(nav);

    // little tag so the render is self-labelling
    static const char *TAG[6] = { "A", "B", "C", "D", "E", "F" };
    font(FONT_TINY); print(TAG[arr], (int)(panel.x + panel.w - 6), (int)panel.y + 1, CLR_DARK_BROWN);

    if (arr == 0)      draw_A(body);
    else if (arr == 1) draw_B(body);
    else if (arr == 2) draw_C(body, 1);
    else if (arr == 3) draw_C(body, 2);
    else if (arr == 4) draw_E(body);
    else               draw_F(body);
}
