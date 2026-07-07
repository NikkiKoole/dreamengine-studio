/* de:meta
{
  "title": "epianofit",
  "status": "active",
  "created": "2026-07-07",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "The layout mock for epiano's responsive redesign - the step-4 artifact of the responsive-instrument-ui.md playbook, applied to a SINGLE-instrument keybed cart (vs acidfit's 5-strip rack). NO audio, just the widgets, laid out against a fake device. epiano's real inventory: a KEYBED (the pinned hero), a 3-way machine selector (RHODES/WURLI/CLAV), two ride-live macros (BRITE/BARK), 3 FX pedals, a preset browser, a drawer of raw macros. The keybed is pinned bottom in EVERY shape; the SOUND panel above it discloses (inline on tablet, compact strip + FX tab on phone). The keybed itself EDITOR-SWAPS: real piano keys where there's width + chromatic, else a scale-locked PAD GRID that packs more range into less space and (when a scale is picked) shows only in-scale notes. A dedicated keybed bar owns NOTE input: a prominent OCT-/OCT+ pair with a root readout + a SCALE selector (CHROMA/MAJOR/MINOR/PENTA/DORIAN). Surfaces the EP truth that playable range scales with width - and shows the grid beating tall piano keys for range on a phone. Presets auto-cycle across 5 device shapes; 1-5 lock, 0 auto, 'm' machine, 'f' fx, 's' scale, 'z'/'x' octave, 'g' force piano/grid."
}
de:meta */
// EPIANOFIT — epiano's layout mock, built by the responsive-instrument-ui.md
// playbook (step 4: prototype the arrangements in a fit-cart with lay.h).
//
// acidfit stress-tested a dense 5-strip RACK. epiano is the other shape: ONE
// instrument whose identity is a big KEYBED. Two ideas on show:
//
//  1. THE SOUND PANEL DISCLOSES (like acidfit, but the keybed is pinned):
//       roomy (tablet) → INLINE: selector + macros + pedals + preset + raw.
//       tight  (phone) → COMPACT strip; pedals + raw one tap away behind FX.
//
//  2. THE KEYBED EDITOR-SWAPS by shape + scale (design-system §8.3 / brief §3):
//       real PIANO keys where there's width AND scale = chromatic;
//       else a scale-locked PAD GRID — packs more octaves into the same box,
//       and when a scale is picked shows ONLY in-scale notes (roots tinted),
//       so there are no wrong notes and a phone gets real range.
//     A dedicated keybed BAR owns note input: a prominent OCT-/OCT+ pair + a
//     root readout + a SCALE selector. (Sound stays in the panel; notes here.)
//
//   Presets auto-cycle. 1-5 lock a device, 0 auto. 'm' machine, 'f' fx,
//   's' scale, 'z'/'x' octave down/up, 'g' force piano/grid (else auto).

#include "studio.h"
#include <math.h>
#include <stdio.h>   // snprintf for the metric readout
#include <string.h>  // strstr for the cramped-flag colour

#include "lay.h"   // Box + the lay_* layout vocabulary

// ───────────────────────── epiano's real inventory ──────────────────────────
static const char *const MACH[3]   = { "RHODES", "WURLI", "CLAV" };
static const char *const MACRO2[2] = { "BRITE", "BARK" };
static const char *const RAW3[3]   = { "HARM", "TIMB", "MORPH" };
static const char *const PED[3]    = { "TREM", "DYNO", "SWIRL" };
static const char *const PRESET[5] = { "Mk I", "Suitc", "Dyno", "Bell", "Brite" };
#define NPRESET 5

// scales (semitone degrees within an octave). CHROMA = all 12.
static const int SC_N[5]     = { 12, 7, 7, 5, 7 };
static const int SC[5][12]   = {
    { 0,1,2,3,4,5,6,7,8,9,10,11 },   // chromatic
    { 0,2,4,5,7,9,11 },              // major
    { 0,2,3,5,7,8,10 },              // natural minor
    { 0,3,5,7,10 },                  // minor pentatonic
    { 0,2,3,5,7,9,10 },              // dorian
};
static const char *const SC_NAME[5] = { "CHROMA", "MAJOR", "MINOR", "PENTA", "DORIAN" };
static const char *const NOTE[12]   = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

// devices (logical points; a 44pt finger is ~9mm on all of them)
typedef struct { const char *name; float wpt, hpt; } Device;
static Device DEVS[5] = {
    { "iPhone SE \x18", 320, 568 },
    { "iPhone \x18",    390, 844 }, { "iPhone \x1a", 844, 390 },
    { "iPad \x18",      834, 1194 }, { "iPad \x1a",  1194, 834 },
};
#define NDEV 5
#define FINGER_PT 44.0f
#define TABLET_PT 700.0f

static int tick = 0, locked = -1, machine = 0, curpreset = 0;
static int scale = 0, octave = 3, editor = 0;   // scale idx (0=chromatic) · base octave · editor(0 auto,1 piano,2 grid)
static bool fx_open = false;

// ── a wireframe knob: dial + pointer + label (angle varies by index) ─────────
static void knob(Box cell, int i, const char *label, int accent, float fu) {
    Box dial;
    Box lab = lay_split(cell, EDGE_BOTTOM, lay_clamp(fu * 0.3f, 4, 9), &dial);
    float r = lay_clamp((dial.w < dial.h ? dial.w : dial.h) * 0.42f, 3, 40);
    float cx = dial.x + dial.w / 2, cy = dial.y + dial.h / 2;
    circfill((int)cx, (int)cy, (int)r, CLR_MEDIUM_GREY);
    circ((int)cx, (int)cy, (int)r, CLR_DARK_GREY);
    float a = 2.3f + i * 0.9f;
    line((int)cx, (int)cy, (int)(cx + r * 0.8f * cosf(a)), (int)(cy + r * 0.8f * sinf(a)),
         accent ? CLR_ORANGE : CLR_LIGHT_GREY);
    if (r >= 5) { font(FONT_TINY); print_centered(label, (int)cx, (int)lab.y, CLR_LIGHT_GREY); }
}

static void selector(Box area, float fu) {           // machine tabs, one lit
    float gap = lay_clamp(fu * 0.1f, 1, 3);
    for (int i = 0; i < 3; i++) {
        Box seg = lay_cell(area, 0, 3, i, gap);
        int on = (i == machine);
        boxfill(seg, on ? CLR_ORANGE : CLR_DARKER_BLUE);
        boxrect(seg, on ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
        if (seg.h >= 7) { font(FONT_TINY); print_centered(MACH[i], (int)(seg.x + seg.w / 2),
                          (int)(seg.y + (seg.h - 5) / 2), on ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY); }
    }
}

static void pedal(Box rect, const char *name, int i, float fu) {
    boxfill(rect, CLR_DARKER_PURPLE); boxrect(rect, CLR_DARK_GREY);
    Box body = lay_inset(rect, 1.5f), knb;
    Box top = lay_split(body, EDGE_TOP, body.h * 0.32f, &knb);
    circfill((int)(top.x + top.w / 2), (int)(top.y + top.h / 2),
             (int)lay_clamp(top.h * 0.4f, 2, 8), CLR_DARK_ORANGE);
    knob(knb, i + 3, name, 1, fu);
}

static void preset_grid(Box area, float fu) {
    float gap = lay_clamp(fu * 0.1f, 1, 3);
    for (int i = 0; i < NPRESET; i++) {
        Box chip = lay_wrap(area, NPRESET, i, fu * 1.4f, gap);
        if (chip.w < 6 || chip.h < 6) continue;
        int on = (i == curpreset);
        boxfill(lay_inset(chip, 0.5f), on ? CLR_TRUE_BLUE : CLR_DARKER_BLUE);
        boxrect(lay_inset(chip, 0.5f), on ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
        if (chip.h >= 8) { font(FONT_TINY); print_centered(PRESET[i], (int)(chip.x + chip.w / 2),
                           (int)(chip.y + (chip.h - 5) / 2), CLR_LIGHT_PEACH); }
    }
}
static void preset_strip(Box area, float fu) {
    boxfill(area, CLR_DARKER_BLUE); boxrect(area, CLR_DARK_GREY);
    font(FONT_TINY);
    print("\x1b", (int)area.x + 2, (int)(area.y + (area.h - 5) / 2), CLR_LIGHT_PEACH);
    print_right("\x1a", (int)(area.x + area.w - 2), (int)(area.y + (area.h - 5) / 2), CLR_LIGHT_PEACH);
    print_centered(PRESET[curpreset], (int)(area.x + area.w / 2), (int)(area.y + (area.h - 5) / 2), CLR_LIGHT_GREY);
}

// ── EDITOR A · real piano keys (as many finger-wide whites as fit) ───────────
// KEY_MIN_F = the comfortable touch minimum in FINGERS (1 finger = 44pt, Apple
// HIG). A white key is a tall target so we allow a hair under a full finger;
// count derives from it, so keys are never sized below the floor.
#define KEY_MIN_F 0.9f
static void keybed_piano(Box area, float fu, int *nWhite_out, float *keyW_out) {
    float gap = 1, minKeyW = lay_clamp(fu * KEY_MIN_F, 6, 200);
    int nWhite = (int)((area.w + gap) / (minKeyW + gap)); if (nWhite < 3) nWhite = 3;
    float kw = (area.w - gap * (nWhite - 1)) / nWhite;
    *keyW_out = kw; *nWhite_out = nWhite;
    for (int i = 0; i < nWhite; i++)
        { Box k = box(area.x + i * (kw + gap), area.y, kw, area.h);
          boxfill(k, CLR_LIGHT_PEACH); boxrect(k, CLR_DARK_GREY); }
    float bw = kw * 0.6f, bh = area.h * 0.6f; int blackAfter[5] = { 0,1,3,4,5 };
    for (int i = 0; i < nWhite - 1; i++) {
        int pos = i % 7, has = 0;
        for (int b = 0; b < 5; b++) if (blackAfter[b] == pos) has = 1;
        if (!has) continue;
        Box bk = box(area.x + (i + 1) * (kw + gap) - bw / 2 - gap / 2, area.y, bw, bh);
        boxfill(bk, CLR_BROWNISH_BLACK); boxrect(bk, CLR_DARK_GREY);
    }
}

// ── EDITOR B · scale-locked pad grid — the phone/scale-mode swap ─────────────
// Fills the box with pads, ascending bottom-left; when the scale is not
// chromatic only in-scale notes appear (no wrong notes). Roots tinted.
// TWO sizing regimes, so Law 2 ("grow → reveal more, don't shrink/tile forever")
// holds both ways:
//   tight  (phone) — pack as many 1-FINGER pads as fit; range is precious.
//   roomy  (iPad)  — cap the range to GRID_MAX_OCT octaves and GROW the pads to
//                    fill (big comfy squares); OCT-/OCT+ window the range.
// Never below 1 finger either way. Returns pad count + octaves exposed.
#define GRID_MAX_OCT 4
static void keybed_grid(Box area, float fu, int *pads_out, float *oct_out, float *padW_out) {
    float gap = lay_clamp(fu * 0.12f, 1, 4);
    int nsc = SC_N[scale], maxNotes = GRID_MAX_OCT * nsc;
    int colsMin = (int)((area.w + gap) / (fu + gap)); if (colsMin < 1) colsMin = 1;
    int rowsMin = (int)((area.h + gap) / (fu + gap)); if (rowsMin < 1) rowsMin = 1;
    int cols, rows;
    if (colsMin * rowsMin <= maxNotes) {          // tight — pack finger-pads
        cols = colsMin; rows = rowsMin;
    } else {                                      // roomy — cap range, grow pads
        rows = (int)(sqrtf((float)maxNotes * area.h / area.w) + 0.5f); if (rows < 1) rows = 1;
        cols = (maxNotes + rows - 1) / rows;      if (cols < 1) cols = 1;
        if (cols > colsMin) cols = colsMin;       // never shrink a pad below 1 finger
        if (rows > rowsMin) rows = rowsMin;
    }
    float pw = (area.w - gap * (cols - 1)) / cols, ph = (area.h - gap * (rows - 1)) / rows;
    int total = cols * rows;
    *pads_out = total; *oct_out = (float)total / nsc; *padW_out = pw;
    for (int idx = 0; idx < total; idx++) {
        int col = idx % cols, row = idx / cols;
        int order = (rows - 1 - row) * cols + col;    // bottom-left = lowest
        int deg = order % nsc, oc = order / nsc;
        int pc = SC[scale][deg] % 12;
        int midi = 12 * octave + 12 * oc + SC[scale][deg];
        Box pad = lay_inset(box(area.x + col * (pw + gap), area.y + row * (ph + gap), pw, ph), 0.5f);
        int black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        int fill = (deg == 0) ? CLR_ORANGE : (black ? CLR_DARKER_PURPLE : CLR_DARKER_BLUE);
        boxfill(pad, fill); boxrect(pad, (deg == 0) ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
        if (pad.h >= 8 && pad.w >= 10) {
            font(FONT_TINY);
            print_centered(str("%s%d", NOTE[pc], octave + oc),
                           (int)(pad.x + pad.w / 2), (int)(pad.y + (pad.h - 5) / 2),
                           (deg == 0) ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
        }
        (void)midi;
    }
}

void update(void) {
    tick++;
    for (int k = 0; k < NDEV; k++) if (keyp('1' + k)) locked = k;
    if (keyp('0')) locked = -1;
    if (keyp('m') || keyp('M')) machine = (machine + 1) % 3;
    if (keyp('f') || keyp('F')) fx_open = !fx_open;
    if (keyp('s') || keyp('S')) scale = (scale + 1) % 5;
    if (keyp('g') || keyp('G')) editor = (editor + 1) % 3;
    if (keyp('z') || keyp('Z')) { if (octave > 0) octave--; }
    if (keyp('x') || keyp('X')) { if (octave < 8) octave++; }
    curpreset = (tick / 90) % NPRESET;
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    int preset = locked >= 0 ? locked : (tick / 150) % NDEV;
    Device d = DEVS[preset];

    font(FONT_SMALL);
    print("EPIANOFIT - epiano's layout mock (responsive-instrument-ui.md step 4)", 4, 3, CLR_LIGHT_GREY);

    float availX = 6, availY = 26, availW = SCREEN_W - 12, availH = SCREEN_H - 34;
    float scl = availW / d.wpt; if (d.hpt * scl > availH) scl = availH / d.hpt;
    Box dev = box(availX + (availW - d.wpt * scl) / 2, availY + (availH - d.hpt * scl) / 2,
                  d.wpt * scl, d.hpt * scl);
    float fu = FINGER_PT * scl;
    bool tablet   = (d.wpt < d.hpt ? d.wpt : d.hpt) >= TABLET_PT;
    bool portrait = d.hpt > d.wpt;

    boxfill(lay_inset(dev, -3), CLR_DARK_GREY);
    boxfill(dev, CLR_DARKER_PURPLE);
    float bez = lay_fluid(0.02f, dev.w, 2, 6);
    Box screen = portrait ? lay_pad(dev, bez + fu * 0.35f, bez, bez + fu * 0.2f, bez)
                          : lay_pad(dev, bez, bez + fu * 0.4f, bez, bez + fu * 0.15f);

    float gap = lay_clamp(fu * 0.14f, 1, 5);
    float titleH = lay_clamp(fu * 0.5f, 8, 20);
    Box afterTitle, afterPanel;
    Box title = lay_split(screen, EDGE_TOP, titleH, &afterTitle);

    bool roomy = tablet;
    float wantPanel = roomy ? fu * 5.8f : fu * 2.7f;
    float maxPanel  = afterTitle.h - fu * 3.4f;
    float panelH = lay_clamp(wantPanel, fu * 2.2f, maxPanel < fu * 2.2f ? fu * 2.2f : maxPanel);
    Box panel = lay_split(afterTitle, EDGE_TOP, panelH, &afterPanel);
    Box keyzone = lay_pad(afterPanel, gap, gap, gap, gap);

    // title bar + back-to-root keep-out
    float homeSz = lay_clamp(fu * 0.7f, 8, 30);
    Box home = lay_at(title, L_TR, homeSz, homeSz, 1);
    boxfill(title, CLR_TRUE_BLUE);
    font(FONT_SMALL);
    print("EPIANO", (int)title.x + 3, (int)(title.y + (title.h - 6) / 2), CLR_LIGHT_PEACH);
    boxfill(home, CLR_DARKER_BLUE); boxrect(home, CLR_LIGHT_GREY);
    print_centered("<", (int)(home.x + home.w / 2), (int)(home.y + (home.h - 6) / 2), CLR_LIGHT_PEACH);

    // ── SOUND panel (disclosure) ──────────────────────────────────────────────
    boxfill(panel, CLR_DARKER_BLUE);
    Box pin = lay_pad(panel, gap, gap, gap, gap), selBar, pbody;
    selBar = lay_split(pin, EDGE_TOP, lay_clamp(fu * 0.8f, 8, 20), &pbody);
    pbody = lay_pad(pbody, gap, 0, 0, 0);
    const char *mode;
    if (roomy) {
        mode = "INLINE";
        selector(selBar, fu);
        Box c0 = lay_cell(pbody, 0, 3, 0, gap), c1 = lay_cell(pbody, 0, 3, 1, gap), c2 = lay_cell(pbody, 0, 3, 2, gap);
        for (int i = 0; i < 2; i++) knob(lay_cell(c0, 0, 2, i, gap), i, MACRO2[i], 1, fu);
        for (int i = 0; i < 3; i++) pedal(lay_cell(c1, 0, 3, i, gap), PED[i], i, fu);
        Box praw, pre = lay_split(c2, EDGE_TOP, c2.h * 0.5f, &praw);
        preset_grid(pre, fu);
        for (int i = 0; i < 3; i++) knob(lay_cell(praw, 0, 3, i, gap), i, RAW3[i], 0, fu);
    } else {
        mode = "COMPACT";
        selector(selBar, fu);
        Box b0 = lay_cell(pbody, 0, 4, 0, gap), b1 = lay_cell(pbody, 0, 4, 1, gap),
            b2 = lay_cell(pbody, 0, 4, 2, gap), b3 = lay_cell(pbody, 0, 4, 3, gap);
        knob(b0, 0, MACRO2[0], 1, fu); knob(b1, 1, MACRO2[1], 1, fu);
        preset_strip(b2, fu);
        boxfill(b3, fx_open ? CLR_ORANGE : CLR_DARKER_PURPLE); boxrect(b3, CLR_LIGHT_GREY);
        font(FONT_TINY);
        print_centered("FX \x1a", (int)(b3.x + b3.w / 2), (int)(b3.y + (b3.h - 5) / 2),
                       fx_open ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH);
    }

    // ── the KEYBED bar (owns NOTE input) — scale selector + prominent octave ──
    float barH = lay_clamp(fu * 0.85f, 11, 22);
    Box kbody, kbar = lay_split(keyzone, EDGE_TOP, barH, &kbody);
    kbody = lay_pad(kbody, 0, 0, 0, 0);
    float octW = lay_clamp(fu * 3.4f, 42, 190);
    Box sclbar; Box octbar = lay_split(kbar, EDGE_RIGHT, octW, &sclbar);
    // SCALE selector (left)
    Box rest2; Box sclchip = lay_split(sclbar, EDGE_LEFT, lay_clamp(fu * 4.2f, 46, 220), &rest2);
    boxfill(sclchip, CLR_DARKER_BLUE); boxrect(sclchip, CLR_ORANGE);
    font(FONT_TINY);
    print(str("SCALE \x1a %s", SC_NAME[scale]), (int)sclchip.x + 2, (int)(sclchip.y + (sclchip.h - 5) / 2), CLR_LIGHT_PEACH);
    // prominent OCT- [root] OCT+ (right)
    float og = lay_clamp(fu * 0.08f, 1, 2);
    Box om = lay_cell(octbar, 0, 3, 0, og), ol = lay_cell(octbar, 0, 3, 1, og), op = lay_cell(octbar, 0, 3, 2, og);
    boxfill(om, CLR_DARKER_PURPLE); boxrect(om, CLR_LIGHT_PEACH);
    boxfill(op, CLR_DARKER_PURPLE); boxrect(op, CLR_LIGHT_PEACH);
    print_centered("OCT-", (int)(om.x + om.w / 2), (int)(om.y + (om.h - 5) / 2), CLR_LIGHT_PEACH);
    print_centered("OCT+", (int)(op.x + op.w / 2), (int)(op.y + (op.h - 5) / 2), CLR_LIGHT_PEACH);
    boxfill(ol, CLR_BROWNISH_BLACK); boxrect(ol, CLR_DARK_GREY);
    print_centered(str("C%d", octave), (int)(ol.x + ol.w / 2), (int)(ol.y + (ol.h - 5) / 2), CLR_ORANGE);

    // editor swap: piano where there's width AND chromatic; else the pad grid.
    bool auto_grid = (scale != 0) || (portrait && !tablet);
    bool use_grid  = (editor == 2) || (editor == 0 && auto_grid);
    if (editor == 1) use_grid = false;

    char metric[110];
    if (use_grid) {
        int pads; float goct, pw;
        keybed_grid(kbody, fu, &pads, &goct, &pw);
        float f = pw / fu;                                   // pad width in FINGERS
        snprintf(metric, sizeof metric, "GRID %s: %d pads / %.1f oct @ %.2f finger %s",
                 SC_NAME[scale], pads, goct, f, f < 0.9f ? "CRAMPED" : "ok");
    } else {
        int nw; float kw;
        keybed_piano(kbody, fu, &nw, &kw);
        float f = kw / fu;                                   // white-key width in FINGERS
        snprintf(metric, sizeof metric, "PIANO: %d white / %.1f oct @ %.2f finger %s",
                 nw, nw / 7.0f, f, f < 0.8f ? "CRAMPED" : "ok");
    }

    // ── FX tab overlay (tight shapes only) ────────────────────────────────────
    if (!roomy && fx_open) {
        Box sheet = lay_pad(afterPanel, gap * 2, gap * 2, gap * 2, gap * 2);
        boxfill(lay_inset(sheet, -2), CLR_BROWNISH_BLACK);
        boxfill(sheet, CLR_DARKER_BLUE); boxrect(sheet, CLR_ORANGE);
        Box sin = lay_pad(sheet, gap, gap, gap, gap), sb;
        sb = lay_split(sin, EDGE_TOP, lay_clamp(fu * 0.5f, 8, 16), &sin);
        font(FONT_TINY);
        print("FX  (f closes)", (int)sb.x + 2, (int)(sb.y + (sb.h - 5) / 2), CLR_LIGHT_PEACH);
        Box praw, pedrow = lay_split(sin, EDGE_TOP, sin.h * 0.58f, &praw);
        for (int i = 0; i < 3; i++) pedal(lay_cell(pedrow, 0, 3, i, gap), PED[i], i, fu);
        for (int i = 0; i < 3; i++) knob(lay_cell(praw, 0, 3, i, gap), i, RAW3[i], 0, fu);
    }

    // ── readout ────────────────────────────────────────────────────────────────
    bool cramped = (strstr(metric, "CRAMPED") != NULL);
    font(FONT_TINY);
    print(str("%s  %dx%dpt  %s | %s | panel:%s | %s",
              d.name, (int)d.wpt, (int)d.hpt, tablet ? "TABLET" : "PHONE", metric, mode, MACH[machine]),
          4, SCREEN_H - 6, cramped ? CLR_ORANGE : (use_grid ? CLR_LIME_GREEN : CLR_GREEN));
    print_right(locked >= 0 ? "[1-5 dev 0 auto  m f  s scale  z/x oct  g editor]" : "[auto 1-5  m f s z x g]",
                SCREEN_W - 4, 3, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
