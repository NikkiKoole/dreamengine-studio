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
  "description": "The layout mock for epiano's responsive redesign - the step-4 artifact of the responsive-instrument-ui.md playbook, applied to a SINGLE-instrument keybed cart (vs acidfit's 5-strip rack). NO audio, just the widgets, laid out against a fake device to answer 'does epiano stay playable from an iPhone SE to an iPad?'. epiano's real inventory: a KEYBED (the pinned hero), a 3-way machine selector (RHODES/WURLI/CLAV), two ride-live macros (BRITE/BARK), 3 FX pedals, a preset browser, and a drawer of raw macros (HARM/TIMB/MORPH). The keybed is pinned bottom in EVERY shape; the panel above it DISCLOSES - inline-all on a tablet, a compact strip + an FX tab on a phone. Surfaces the EP-specific responsive truth: playable OCTAVES scale with width (the keybed twin of otafit's ribbon-length metric) - a phone portrait keybed is cramped, landscape is the win. Presets auto-cycle across 5 device shapes; 1-5 lock, 0 auto, 'm' cycles the machine, 'f' opens the FX tab on tight shapes."
}
de:meta */
// EPIANOFIT — epiano's layout mock, built by the responsive-instrument-ui.md
// playbook (step 4: prototype the arrangements in a fit-cart with lay.h).
//
// acidfit stress-tested a dense 5-strip RACK. epiano is the other shape: ONE
// instrument whose identity is a big KEYBED. So the disclosure model differs —
// the keybed is the pinned performing surface (never folds), and only the
// control PANEL above it discloses:
//
//   roomy (tablet)     → INLINE: selector + macros + all 3 pedals + preset grid
//                        + raw macros, all open above a generous keybed.
//   tight  (phone)     → COMPACT strip: selector + BRITE/BARK + preset prev/next,
//                        the pedals + raw macros one tap away behind an FX tab.
//
// The EP-specific lesson it makes visible: a keybed's playability IS its width
// (px per white key → octaves shown). Portrait crams few/narrow keys; landscape
// is the keybed's friend — the same "this rack may want lock-landscape" call
// otafit surfaced for ribbons.
//
//   Presets auto-cycle. 1-5 lock a device, 0 auto. 'm' cycles RHODES/WURLI/CLAV,
//   'f' opens the FX tab on the tight shapes.

#include "studio.h"
#include <math.h>

#include "lay.h"   // Box + the lay_* layout vocabulary

// ───────────────────────── epiano's real inventory ──────────────────────────
static const char *const MACH[3]   = { "RHODES", "WURLI", "CLAV" };
static const char *const MACRO2[2] = { "BRITE", "BARK" };
static const char *const RAW3[3]   = { "HARM", "TIMB", "MORPH" };
// each machine carries 3 FX pedals (icon + knob(s)); generic names for the mock
static const char *const PED[3]    = { "TREM", "DYNO", "SWIRL" };
static const char *const PRESET[5] = { "Mk I", "Suitc", "Dyno", "Bell", "Brite" };
#define NPRESET 5

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

// ── the machine selector: 3 segmented tabs, one lit ──────────────────────────
static void selector(Box area, float fu) {
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

// ── one FX pedal: icon (top) + a knob + label ────────────────────────────────
static void pedal(Box rect, const char *name, int i, float fu) {
    boxfill(rect, CLR_DARKER_PURPLE); boxrect(rect, CLR_DARK_GREY);
    Box body = lay_inset(rect, 1.5f);
    Box knb;
    Box top = lay_split(body, EDGE_TOP, body.h * 0.32f, &knb);
    // icon: a little tine/lamp
    float ir = lay_clamp(top.h * 0.4f, 2, 8);
    circfill((int)(top.x + top.w / 2), (int)(top.y + top.h / 2), (int)ir, CLR_DARK_ORANGE);
    knob(knb, i + 3, name, 1, fu);
}

// ── the preset browser: a grid (roomy) or prev/next (compact) ────────────────
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
static void preset_strip(Box area, float fu) {         // ◀ name ▶
    boxfill(area, CLR_DARKER_BLUE); boxrect(area, CLR_DARK_GREY);
    font(FONT_TINY);
    print("\x1b", (int)area.x + 2, (int)(area.y + (area.h - 5) / 2), CLR_LIGHT_PEACH);
    print_right("\x1a", (int)(area.x + area.w - 2), (int)(area.y + (area.h - 5) / 2), CLR_LIGHT_PEACH);
    print_centered(PRESET[curpreset], (int)(area.x + area.w / 2), (int)(area.y + (area.h - 5) / 2), CLR_LIGHT_GREY);
}

// ── the KEYBED: as many finger-wide white keys as fit + blacks. returns nWhite,
//    writes the achieved white-key width so the readout can flag a cramped bed.
static int keybed(Box area, float fu, float *keyW_out) {
    float gap = 1;
    float minKeyW = lay_clamp(fu * 0.62f, 6, 64);          // comfortable min white key
    int nWhite = (int)((area.w + gap) / (minKeyW + gap)); if (nWhite < 3) nWhite = 3;
    float kw = (area.w - gap * (nWhite - 1)) / nWhite;
    *keyW_out = kw;
    for (int i = 0; i < nWhite; i++) {
        Box k = box(area.x + i * (kw + gap), area.y, kw, area.h);
        boxfill(k, CLR_LIGHT_PEACH); boxrect(k, CLR_DARK_GREY);
    }
    // black keys: within each octave (7 whites C..B) after white 0,1,3,4,5
    float bw = kw * 0.6f, bh = area.h * 0.6f;
    int blackAfter[5] = { 0, 1, 3, 4, 5 };
    for (int i = 0; i < nWhite - 1; i++) {
        int pos = i % 7, has = 0;
        for (int b = 0; b < 5; b++) if (blackAfter[b] == pos) has = 1;
        if (!has) continue;
        float cx = area.x + (i + 1) * (kw + gap) - bw / 2 - gap / 2;
        Box bk = box(cx, area.y, bw, bh);
        boxfill(bk, CLR_BROWNISH_BLACK); boxrect(bk, CLR_DARK_GREY);
    }
    return nWhite;
}

void update(void) {
    tick++;
    for (int k = 0; k < NDEV; k++) if (keyp('1' + k)) locked = k;
    if (keyp('0')) locked = -1;
    if (keyp('m') || keyp('M')) machine = (machine + 1) % 3;
    if (keyp('f') || keyp('F')) fx_open = !fx_open;
    curpreset = (tick / 90) % NPRESET;      // gently auto-browse presets for the mock
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    int preset = locked >= 0 ? locked : (tick / 150) % NDEV;
    Device d = DEVS[preset];

    font(FONT_SMALL);
    print("EPIANOFIT - epiano's layout mock (responsive-instrument-ui.md step 4)", 4, 3, CLR_LIGHT_GREY);

    // fit the device into the canvas, preserving aspect
    float availX = 6, availY = 26, availW = SCREEN_W - 12, availH = SCREEN_H - 34;
    float scale = availW / d.wpt; if (d.hpt * scale > availH) scale = availH / d.hpt;
    Box dev = box(availX + (availW - d.wpt * scale) / 2, availY + (availH - d.hpt * scale) / 2,
                  d.wpt * scale, d.hpt * scale);
    float fu = FINGER_PT * scale;
    bool tablet   = (d.wpt < d.hpt ? d.wpt : d.hpt) >= TABLET_PT;
    bool portrait = d.hpt > d.wpt;

    boxfill(lay_inset(dev, -3), CLR_DARK_GREY);
    boxfill(dev, CLR_DARKER_PURPLE);
    float bez = lay_fluid(0.02f, dev.w, 2, 6);
    Box screen = portrait ? lay_pad(dev, bez + fu * 0.35f, bez, bez + fu * 0.2f, bez)
                          : lay_pad(dev, bez, bez + fu * 0.4f, bez, bez + fu * 0.15f);

    float gap = lay_clamp(fu * 0.14f, 1, 5);

    // ── the arrangement: title (top) · keybed (bottom, PINNED) · panel (middle)
    float titleH = lay_clamp(fu * 0.5f, 8, 20);
    Box afterTitle, afterPanel;
    Box title = lay_split(screen, EDGE_TOP, titleH, &afterTitle);

    // panel gets a small compact budget or a generous inline budget; the keybed
    // (the hero) takes the rest — but never less than ~3 fingers of height.
    bool roomy = tablet;
    float wantPanel = roomy ? fu * 5.8f : fu * 2.7f;
    float maxPanel  = afterTitle.h - fu * 3.0f;          // guarantee the keybed
    float panelH = lay_clamp(wantPanel, fu * 2.2f, maxPanel < fu * 2.2f ? fu * 2.2f : maxPanel);
    Box panel = lay_split(afterTitle, EDGE_TOP, panelH, &afterPanel);
    Box keyzone = lay_pad(afterPanel, gap, gap, gap, gap);

    // ── title bar + back-to-root keep-out (matches the app chrome) ────────────
    float homeSz = lay_clamp(fu * 0.7f, 8, 30);
    Box home = lay_at(title, L_TR, homeSz, homeSz, 1);
    boxfill(title, CLR_TRUE_BLUE);
    font(FONT_SMALL);
    print("EPIANO", (int)title.x + 3, (int)(title.y + (title.h - 6) / 2), CLR_LIGHT_PEACH);
    boxfill(home, CLR_DARKER_BLUE); boxrect(home, CLR_LIGHT_GREY);
    print_centered("<", (int)(home.x + home.w / 2), (int)(home.y + (home.h - 6) / 2), CLR_LIGHT_PEACH);

    // ── the PANEL (disclosure) ────────────────────────────────────────────────
    boxfill(panel, CLR_DARKER_BLUE);
    Box pin = lay_pad(panel, gap, gap, gap, gap);
    Box selBar, pbody;
    selBar = lay_split(pin, EDGE_TOP, lay_clamp(fu * 0.8f, 8, 20), &pbody);
    pbody = lay_pad(pbody, gap, 0, 0, 0);

    const char *mode;
    if (roomy) {
        // INLINE-ALL — selector on top, then macros | pedals | preset+raw columns
        mode = "INLINE";
        selector(selBar, fu);
        Box c0 = lay_cell(pbody, 0, 3, 0, gap);          // macros
        Box c1 = lay_cell(pbody, 0, 3, 1, gap);          // pedals
        Box c2 = lay_cell(pbody, 0, 3, 2, gap);          // preset + raw macros
        for (int i = 0; i < 2; i++) knob(lay_cell(c0, 0, 2, i, gap), i, MACRO2[i], 1, fu);
        for (int i = 0; i < 3; i++) pedal(lay_cell(c1, 0, 3, i, gap), PED[i], i, fu);
        Box praw, pre = lay_split(c2, EDGE_TOP, c2.h * 0.5f, &praw);
        preset_grid(pre, fu);
        for (int i = 0; i < 3; i++) knob(lay_cell(praw, 0, 3, i, gap), i, RAW3[i], 0, fu);
    } else {
        // COMPACT — selector + [BRITE][BARK][◀preset▶][FX ▸] ; pedals/raw behind FX
        mode = "COMPACT";
        selector(selBar, fu);
        Box b0 = lay_cell(pbody, 0, 4, 0, gap);
        Box b1 = lay_cell(pbody, 0, 4, 1, gap);
        Box b2 = lay_cell(pbody, 0, 4, 2, gap);
        Box b3 = lay_cell(pbody, 0, 4, 3, gap);
        knob(b0, 0, MACRO2[0], 1, fu);
        knob(b1, 1, MACRO2[1], 1, fu);
        preset_strip(b2, fu);
        boxfill(b3, fx_open ? CLR_ORANGE : CLR_DARKER_PURPLE);
        boxrect(b3, CLR_LIGHT_GREY);
        font(FONT_TINY);
        print_centered("FX \x1a", (int)(b3.x + b3.w / 2), (int)(b3.y + (b3.h - 5) / 2),
                       fx_open ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH);
    }

    // ── the KEYBED (pinned hero, every shape) ────────────────────────────────
    // a slim header carries octave shift + the playability metric
    Box kbody, khdr = lay_split(keyzone, EDGE_TOP, lay_clamp(fu * 0.42f, 7, 14), &kbody);
    Box octl = lay_at(khdr, L_TR, lay_clamp(fu * 0.7f, 9, 26), khdr.h, 0);
    Box octr = lay_at(khdr, L_TR, lay_clamp(fu * 0.7f, 9, 26), khdr.h, lay_clamp(fu * 0.7f, 9, 26) + 1);
    font(FONT_TINY);
    print("KEYBED", (int)khdr.x + 1, (int)(khdr.y + (khdr.h - 5) / 2), CLR_DARK_GREY);
    boxfill(octr, CLR_DARKER_BLUE); boxrect(octr, CLR_DARK_GREY);
    boxfill(octl, CLR_DARKER_BLUE); boxrect(octl, CLR_DARK_GREY);
    print_centered("\x1b", (int)(octl.x + octl.w / 2), (int)(octl.y + (octl.h - 5) / 2), CLR_LIGHT_GREY);
    print_centered("\x1a", (int)(octr.x + octr.w / 2), (int)(octr.y + (octr.h - 5) / 2), CLR_LIGHT_GREY);

    float keyW;
    int nWhite = keybed(kbody, fu, &keyW);
    bool cramped = keyW < fu * 0.6f;

    // ── FX tab overlay (tight shapes only) ────────────────────────────────────
    if (!roomy && fx_open) {
        Box sheet = lay_pad(afterPanel, gap * 2, gap * 2, gap * 2, gap * 2);
        boxfill(lay_inset(sheet, -2), CLR_BROWNISH_BLACK);
        boxfill(sheet, CLR_DARKER_BLUE); boxrect(sheet, CLR_ORANGE);
        Box sin = lay_pad(sheet, gap, gap, gap, gap);
        Box shdr, sb = lay_split(sin, EDGE_TOP, lay_clamp(fu * 0.5f, 8, 16), &sin);
        (void)shdr;
        font(FONT_TINY);
        print("FX  (f closes)", (int)sb.x + 2, (int)(sb.y + (sb.h - 5) / 2), CLR_LIGHT_PEACH);
        Box praw, pedrow = lay_split(sin, EDGE_TOP, sin.h * 0.58f, &praw);
        for (int i = 0; i < 3; i++) pedal(lay_cell(pedrow, 0, 3, i, gap), PED[i], i, fu);
        for (int i = 0; i < 3; i++) knob(lay_cell(praw, 0, 3, i, gap), i, RAW3[i], 0, fu);
    }

    // ── readout ────────────────────────────────────────────────────────────────
    float octaves = nWhite / 7.0f;
    font(FONT_TINY);
    print(str("%s  %dx%dpt  %s  fu=%dpx | keys: %d white / %.1f oct @ %dpx %s | panel: %s | %s",
              d.name, (int)d.wpt, (int)d.hpt, tablet ? "TABLET" : "PHONE", (int)fu,
              nWhite, octaves, (int)keyW, cramped ? "CRAMPED" : "ok", mode, MACH[machine]),
          4, SCREEN_H - 6, cramped ? CLR_ORANGE : (tablet ? CLR_GREEN : CLR_LIGHT_GREY));
    print_right(locked >= 0 ? "[1-5 lock  0 auto  m machine  f fx]" : "[auto  1-5 lock  m  f]",
                SCREEN_W - 4, 3, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
