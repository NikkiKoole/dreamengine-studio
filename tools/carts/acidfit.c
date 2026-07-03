/* de:meta
{
  "title": "acidfit",
  "status": "active",
  "created": "2026-07-03",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "The stress test for device-adaptive-layout.md: a FAITHFUL layout mock of the real acidrack (two TB-303s at 7 knobs each, a 909 with 11 voices, an 808 with 9, a 16-step lane, a master strip) - NO audio, NO DSP, just the widgets - laid out against a fake device to answer 'does a genuinely dense, multi-section panel stay legible on a phone?'. It prototypes the PROGRESSIVE-DISCLOSURE rule the plan needs: each section carries a priority + a minimum finger-footprint, and one pass per frame inlines sections by priority until the finger-budget runs out - the overflow collapses into TAB CHIPS you tap to open as an overlay. Same code: iPad inlines all five sections; iPhone shows the sequencer + as many as fit and defers the rest to tabs. Two layers on show - GEOMETRY (lay_split/grid/wrap/at) decides where a shown panel goes; BEHAVIOUR (the budget pass) decides WHETHER it's shown inline or behind a toggle. Presets auto-cycle (iPhone/iPad x portrait/landscape); keys 1-4 lock, 'o' opens the first deferred tab, 'c' closes. When it graduates the fake device + finger become the real viewport + backing scale."
}
de:meta */
// ACIDFIT — the dense-panel stress test + progressive-disclosure prototype for
// docs/design/device-adaptive-layout.md.
//
// rackfit proved the finger-sized EMERGENT reflow on a clean 8-knob rack. This
// throws the REAL acidrack inventory at it (5 heterogeneous sections, ~34
// controls + a step lane) and adds the behaviour layer the plan calls for:
//
//   GEOMETRY layer (lay_*)  — where a panel goes IF shown.
//   BEHAVIOUR layer (below) — WHETHER it's shown inline, or collapsed behind a
//                             tab, decided by the available finger-budget.
//
// The rule (CSS "Priority+" / progressive disclosure): each section declares a
// priority + a minimum footprint in fingers. One pass inlines sections by
// priority until the budget runs out; the overflow becomes tab chips. No device
// is ever named — the finger-budget decides. iPad inlines all 5; iPhone defers.
//
//   Presets auto-cycle. Keys 1-4 lock; 'o' opens the first deferred tab as an
//   overlay, 'c' closes. Watch the SAME code inline on iPad and tab on iPhone.

#include "studio.h"
#include <math.h>

// ───────────────────────── lay: the candidate primitive set (== respond.c) ───
typedef struct { float x, y, w, h; } Box;
static Box box(float x, float y, float w, float h) { Box b = {x, y, w, h}; return b; }
static float lay_clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float lay_fluid(float pct, float container, float lo, float hi) { return lay_clamp(pct * container, lo, hi); }
static Box lay_pad(Box c, float t, float r, float b, float l) { return box(c.x + l, c.y + t, c.w - l - r, c.h - t - b); }
static Box lay_inset(Box c, float m) { return lay_pad(c, m, m, m, m); }

enum { L_TL, L_T, L_TR, L_L, L_C, L_R, L_BL, L_B, L_BR };
static Box lay_at(Box c, int anchor, float w, float h, float inset) {
    int col = anchor % 3, row = anchor / 3;
    float x = col == 0 ? c.x + inset : col == 1 ? c.x + (c.w - w) / 2 : c.x + c.w - w - inset;
    float y = row == 0 ? c.y + inset : row == 1 ? c.y + (c.h - h) / 2 : c.y + c.h - h - inset;
    return box(x, y, w, h);
}
enum { EDGE_TOP, EDGE_BOTTOM, EDGE_LEFT, EDGE_RIGHT };
static Box lay_split(Box c, int edge, float size, Box *rest) {
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
// grid(): i-th of n items in a FIXED `cols` grid (CSS grid-template-columns:
// repeat(cols,1fr)). The hand-picked-topology twin of wrap()'s auto-fit — a drum
// machine wants a clean 4-wide pad grid, not "as many as happen to fit".
static Box lay_grid(Box c, int cols, int n, int i, float gap) {
    if (cols < 1) cols = 1;
    int rows = (n + cols - 1) / cols;
    float cw = (c.w - gap * (cols - 1)) / cols;
    float ch = (c.h - gap * (rows - 1)) / rows;
    int cx = i % cols, cy = i / cols;
    return box(c.x + cx * (cw + gap), c.y + cy * (ch + gap), cw, ch);
}
// wrap(): auto-fit columns (as many ~minItem-wide as fit).
static Box lay_wrap(Box c, int n, int i, float minItem, float gap) {
    int cols = (int)((c.w + gap) / (minItem + gap));
    if (cols < 1) cols = 1; if (cols > n) cols = n;
    int rows = (n + cols - 1) / cols;
    float cw = (c.w - gap * (cols - 1)) / cols, ch = (c.h - gap * (rows - 1)) / rows;
    return box(c.x + (i % cols) * (cw + gap), c.y + (i / cols) * (ch + gap), cw, ch);
}

// ───────────────────────── drawing sugar ─────────────────────────────────────
static void boxfill(Box b, int c) { rectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, c); }
static void boxrect(Box b, int c) { rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, c); }
static bool binside(Box b, int x, int y) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

// ───────────────────────── the rack's real inventory ─────────────────────────
enum { KNOBS, DRUMS };
typedef struct { const char *name; int kind; int n; const char *const *labels; float minW, minH; } Panel;

static const char *const K303[] = { "CUT","RES","ENV","DEC","ACC","DRV","SQL" };
static const char *const KMST[] = { "VOL","TON","VRB" };
static const char *const V909[] = { "BD","SD","LT","MT","HT","RS","CP","CH","OH","CC","RC" };
static const char *const V808[] = { "BD","SD","LT","HT","CP","MA","CB","OH","CH" };

// Priority = array order. Min footprint in FINGERS (a comfortable minimum).
// Comfortable minimums: enough room that the CONTROLS inside stay finger-sized
// (a 303 = 7 finger-knobs + labels; a drum grid = 4-wide finger pads), not just
// "the panel box fits". Setting these too small is what let a phone cram all 5.
static Panel PANELS[] = {
    { "303 A",  KNOBS, 7,  K303, 4.2f, 3.4f },
    { "303 B",  KNOBS, 7,  K303, 4.2f, 3.4f },
    { "909",    DRUMS, 11, V909, 4.2f, 4.0f },
    { "808",    DRUMS, 9,  V808, 4.2f, 4.0f },
    { "MASTER", KNOBS, 3,  KMST, 2.6f, 2.6f },
};
#define NPANEL 5
#define STEPS 16

// devices (logical points; a 44pt finger is ~9mm on all of them)
typedef struct { const char *name; float wpt, hpt; } Device;
static Device DEVS[4] = {
    { "iPhone \x18", 390, 844 }, { "iPhone \x1a", 844, 390 },
    { "iPad \x18",   834, 1194 }, { "iPad \x1a",  1194, 834 },
};
#define FINGER_PT 44.0f
#define TABLET_PT 700.0f

static int tick = 0, locked = -1, open_tab = -1;

// how many panels (by priority) fit in `body` at a uniform max-footprint cell
static int fit_count(Box body, float cellW, float cellH, float gap) {
    int cf = (int)((body.w + gap) / (cellW + gap)); if (cf < 1) cf = 1;
    int rf = (int)((body.h + gap) / (cellH + gap)); if (rf < 1) rf = 1;
    int cap = cf * rf; return NPANEL <= cap ? NPANEL : cap;
}

// draw one knob (body + pointer; angle just varies by index)
static void knob(Box cell, int i, const char *label, float fu) {
    Box dial;
    Box lab = lay_split(cell, EDGE_BOTTOM, lay_clamp(fu * 0.3f, 4, 9), &dial);  // label strip / dial
    float r = lay_clamp((dial.w < dial.h ? dial.w : dial.h) * 0.42f, 3, 40);
    float cx = dial.x + dial.w / 2, cy = dial.y + dial.h / 2;
    circfill((int)cx, (int)cy, (int)r, CLR_MEDIUM_GREY);
    circ((int)cx, (int)cy, (int)r, CLR_DARK_GREY);
    float a = 2.3f + i * 0.7f;
    line((int)cx, (int)cy, (int)(cx + r * 0.8f * cosf(a)), (int)(cy + r * 0.8f * sinf(a)), CLR_ORANGE);
    if (r >= 5) { font(FONT_TINY); print_centered(label, (int)cx, (int)lab.y, CLR_LIGHT_GREY); }
}

// draw a panel's CONTENTS inside `area` (header already drawn by caller)
static void panel_body(Panel *p, Box area, float fu) {
    float gap = lay_clamp(fu * 0.12f, 1, 4);
    if (p->kind == KNOBS) {
        for (int i = 0; i < p->n; i++) {
            Box cell = lay_wrap(area, p->n, i, fu * 0.95f, gap);
            if (cell.w < 5 || cell.h < 6) continue;
            knob(lay_inset(cell, 0.5f), i, p->labels[i], fu);
        }
    } else { // DRUMS — a fixed 4-wide pad grid (lay_grid, not auto-wrap)
        for (int i = 0; i < p->n; i++) {
            Box cell = lay_grid(area, 4, p->n, i, gap);
            if (cell.w < 4 || cell.h < 4) continue;
            Box pad = lay_inset(cell, 0.5f);
            int on = (i % 3 == 0);
            boxfill(pad, on ? CLR_DARK_ORANGE : CLR_DARKER_PURPLE);
            boxrect(pad, CLR_DARKER_BLUE);
            if (pad.h >= 8) { font(FONT_TINY); print_centered(p->labels[i], (int)(pad.x + pad.w / 2), (int)(pad.y + (pad.h - 5) / 2), CLR_LIGHT_PEACH); }
        }
    }
}

// draw a panel (header + body) in `rect`
static void draw_panel(Panel *p, Box rect, float fu, int accent) {
    boxfill(rect, CLR_DARKER_BLUE);
    boxrect(rect, accent ? CLR_TRUE_BLUE : CLR_DARK_GREY);
    Box body;
    Box hdr = lay_split(lay_inset(rect, 1), EDGE_TOP, lay_clamp(fu * 0.4f, 6, 12), &body);
    boxfill(hdr, accent ? CLR_TRUE_BLUE : CLR_DARK_GREY);
    font(FONT_TINY);
    print(p->name, (int)hdr.x + 2, (int)(hdr.y + (hdr.h - 5) / 2), CLR_LIGHT_PEACH);
    panel_body(p, lay_pad(body, 1, 1, 1, 1), fu);
}

void update(void) {
    tick++;
    for (int k = 0; k < 4; k++) if (keyp('1' + k)) locked = k;
    if (keyp('0')) locked = -1;
    if (keyp('O')) open_tab = (open_tab < 0) ? 99 : -1;   // 99 = "first deferred" sentinel
    if (keyp('C')) open_tab = -1;
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    int preset = locked >= 0 ? locked : (tick / 150) % 4;
    Device d = DEVS[preset];

    font(FONT_SMALL);
    print("ACIDFIT - dense real rack + progressive disclosure (device-adaptive-layout.md)", 4, 3, CLR_LIGHT_GREY);

    // fit the device into the canvas, preserving aspect
    float availX = 6, availY = 26, availW = SCREEN_W - 12, availH = SCREEN_H - 34;
    float scale = availW / d.wpt; if (d.hpt * scale > availH) scale = availH / d.hpt;
    Box dev = box(availX + (availW - d.wpt * scale) / 2, availY + (availH - d.hpt * scale) / 2, d.wpt * scale, d.hpt * scale);
    float fu = FINGER_PT * scale;
    bool tablet = (d.wpt < d.hpt ? d.wpt : d.hpt) >= TABLET_PT;
    bool portrait = d.hpt > d.wpt;

    boxfill(lay_inset(dev, -3), CLR_DARK_GREY);
    boxfill(dev, CLR_DARKER_PURPLE);
    float bez = lay_fluid(0.02f, dev.w, 2, 6);
    Box screen = portrait ? lay_pad(dev, bez + fu * 0.35f, bez, bez + fu * 0.2f, bez)
                          : lay_pad(dev, bez, bez + fu * 0.4f, bez, bez + fu * 0.15f);

    float gap = lay_clamp(fu * 0.14f, 1, 5);
    // dock chrome: title (top), sequencer lane (bottom, always inline)
    float titleH = lay_clamp(fu * 0.5f, 8, 22);
    float seqH   = lay_clamp(fu * 1.2f, 10, 60);
    Box afterTitle, afterSeq;
    Box title = lay_split(screen, EDGE_TOP, titleH, &afterTitle);
    Box seq   = lay_split(afterTitle, EDGE_BOTTOM, seqH, &afterSeq);
    Box body0 = lay_pad(afterSeq, gap, 0, gap, 0);

    // BEHAVIOUR PASS: how many panels fit by priority? (uniform max-footprint cell)
    float cellW = 4.2f * fu, cellH = 4.0f * fu;   // the max min-footprint
    int inl0 = fit_count(body0, cellW, cellH, gap);
    float tabH = (NPANEL - inl0 > 0) ? lay_clamp(fu * 0.9f, 9, 34) : 0;
    Box tabstrip = {0}, body = body0;
    if (tabH > 0) tabstrip = lay_split(body0, EDGE_BOTTOM, tabH, &body);
    int inlined = fit_count(body, cellW, cellH, gap);
    int deferred = NPANEL - inlined;

    // grid the inlined panels
    int colsFit = (int)((body.w + gap) / (cellW + gap)); if (colsFit < 1) colsFit = 1;
    int cols = colsFit < inlined ? colsFit : inlined; if (cols < 1) cols = 1;

    // ---- title: label + back-to-root corner keep-out ------------------------
    float homeSz = lay_clamp(fu * 0.75f, 8, 34);
    Box home = lay_at(title, L_TR, homeSz, homeSz, 1);
    boxfill(title, CLR_TRUE_BLUE);
    font(FONT_SMALL);
    print("ACID RACK", (int)title.x + 3, (int)(title.y + (title.h - 6) / 2), CLR_LIGHT_PEACH);
    boxfill(home, CLR_DARKER_BLUE); boxrect(home, CLR_LIGHT_GREY);
    print_centered("<", (int)(home.x + home.w / 2), (int)(home.y + (home.h - 6) / 2), CLR_LIGHT_PEACH);

    // ---- the inlined panels -------------------------------------------------
    for (int i = 0; i < inlined; i++)
        draw_panel(&PANELS[i], lay_inset(lay_grid(body, cols, inlined, i, gap), 0), fu, i < 2);

    // ---- the sequencer lane (always inline) ---------------------------------
    boxfill(seq, CLR_DARKER_BLUE); boxrect(seq, CLR_DARK_GREY);
    Box seqin = lay_inset(seq, 1.5f);
    for (int i = 0; i < STEPS; i++) {
        Box s = lay_wrap(seqin, STEPS, i, fu * 0.42f, gap * 0.5f);
        if (s.w < 2) continue;
        int on = (i % 4 == 0) || (i == 6) || (i == 11);
        boxfill(lay_inset(s, 0.5f), on ? CLR_LIME_GREEN : CLR_DARK_GREY);
    }

    // ---- deferred → tab chips ----------------------------------------------
    int first_deferred = inlined;   // panels [inlined..NPANEL) are behind tabs
    if (deferred > 0) {
        boxfill(tabstrip, CLR_BROWNISH_BLACK);
        for (int j = 0; j < deferred; j++) {
            Box chip = lay_inset(lay_grid(tabstrip, deferred, deferred, j, gap), 1);
            int idx = first_deferred + j;
            int open = (open_tab == idx) || (open_tab == 99 && j == 0);
            boxfill(chip, open ? CLR_TRUE_BLUE : CLR_DARK_GREY);
            boxrect(chip, CLR_MEDIUM_GREY);
            font(FONT_TINY);
            print_centered(PANELS[idx].name, (int)(chip.x + chip.w / 2), (int)(chip.y + (chip.h - 5) / 2), CLR_LIGHT_PEACH);
        }
    }

    // ---- overlay: an opened deferred panel ----------------------------------
    int show = -1;
    if (open_tab == 99 && deferred > 0) show = first_deferred;
    else if (open_tab >= first_deferred && open_tab < NPANEL) show = open_tab;
    if (show >= 0) {
        boxfill(screen, CLR_BROWNISH_BLACK);          // dim the rack
        Box ov = lay_inset(body0, fu * 0.4f);
        draw_panel(&PANELS[show], ov, fu, 1);
        Box x = lay_at(ov, L_TR, homeSz, homeSz, 2);
        boxfill(x, CLR_DARK_RED); boxrect(x, CLR_LIGHT_GREY);
        print_centered("x", (int)(x.x + x.w / 2), (int)(x.y + (x.h - 6) / 2), CLR_LIGHT_PEACH);
    }

    // ---- readout ------------------------------------------------------------
    font(FONT_TINY);
    print(str("%s  %dx%dpt  %s  finger=%dpx (%.0f%%w)  INLINE %d  /  TABS %d%s",
              d.name, (int)d.wpt, (int)d.hpt, tablet ? "TABLET" : "PHONE",
              (int)fu, 100.0f * FINGER_PT / d.wpt, inlined, deferred,
              show >= 0 ? "  [overlay]" : ""),
          4, SCREEN_H - 6, tablet ? CLR_GREEN : CLR_ORANGE);
    print_right(locked >= 0 ? "[1-4 lock  o open  c close]" : "[auto  1-4 lock  o open]", SCREEN_W - 4, 3, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
