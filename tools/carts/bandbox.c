/* de:meta
{
  "slug": "bandbox",
  "title": "BandBox",
  "status": "wip",
  "created": "2026-07-21",
  "kind": ["instrument"],
  "genre": null,
  "teaches": [],
  "resizable": true,
  "collection": ["device-face"],
  "lineage": "The SEQUENCER sibling of chordwise (the demand-82 harmony analyzer). chordwise proved the chord-chart + genre-band stack on one flat screen and hit a density ceiling; bandbox is the standalone 160x100 device-face sequencer where each cell is a chord WITH ITS OWN params (strum / inversion / voicing) - the per-cell model chordwise deliberately couldn't hold. Reuses the harmony brain (harmony.h) + the genre band maps + rad_bass_to + drumkit.h. Design: docs/design/bossa-rack.md.",
  "description": {
    "summary": "MOCKUP (look pass, not wired): a 160x100 device-face chord SEQUENCER. The resting face is a 4x2 chord CHART + a genre BAND strip; tap a cell to open its EDITOR full-screen (step the chord in-key AND set its own strum / inversion / voicing). Progressive disclosure keeps the small screen roomy - one thing at a time.",
    "detail": "Static layout to iterate the LOOK before wiring logic. Resting face = nav band + a 4x2 chart HERO + a band strip. Tapping a cell opens a full-screen editor overlay (chord spinner + per-cell param chips) that dismisses with BACK - so the editor gets the whole face instead of a cramped sliver. Sample state hardcoded; tap-to-open is the only wired bit.",
    "controls": "Tap a chart cell to open its editor; BACK closes it. (MOCKUP - the rest is static.)"
  }
}
de:meta */
// bandbox — the chord-chart SEQUENCER (a face.h device face), sibling of chordwise.
// LOOK MOCKUP: the resting face (4x2 chart + band) is clean; the per-cell EDITOR is a
// tap-to-open full-screen overlay, so 160x100 stays roomy (progressive disclosure).
#include "studio.h"
#include "lay.h"
#include "ui.h"
#include "face.h"
#include "cursor.h"
#include <stdio.h>

// resting face: nav (top) · keybed (bottom) · and the MIDDLE splits into a left
// VOICE RAIL (the band, one channel per row) + the chord chart (flanking the hero
// is the sanctioned way to get a left column — bands can't be side-rails).
static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.11f, "nav"   },  // KEY · MODE · play
    { FACE_HERO, 0,           0.00f, "body"  },  // [ voice rail | chart ] — split in draw
    { FACE_BAND, EDGE_BOTTOM, 0.19f, "keys"  },  // a tiny keybed — pick a chord root
};
#define NZ    3
#define CELLS 8

// ── hardcoded sample state (mockup) ───────────────────────────────────────
static const char *CH_NAME[CELLS] = { "Cmaj7", "Am7", "Fmaj7", "G7", "", "", "", "" };
static const char *CH_RN[CELLS]   = { "I",     "vi",  "IV",    "V",  "", "", "", "" };
static const int   CH_ON  = 4;
static int   selCell = -1;   // which cell's editor is open (-1 = none, resting face)
static int   playhead = 0;

// the band's voices (the rail = the tracker's row headers). CH chords · BA bass ·
// ME melody · DR drums · PA pad.
#define VOICES 5
static const char *VL[VOICES]  = { "CH", "BA", "ME", "DR", "PA" };
static const int   VON[VOICES] = { 1, 1, 1, 1, 0 };   // pad off by default

static void chip(Box b, const char *lab, const char *val, int accent) {
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, accent ? CLR_DARK_BLUE : CLR_BROWNISH_BLACK);
    rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, accent ? CLR_ORANGE : CLR_DARKER_GREY);
    print(lab, (int)b.x + 3, (int)b.y + 2, CLR_INDIGO);
    print(val, (int)b.x + 3, (int)b.y + 9, accent ? CLR_WHITE : CLR_LIGHT_GREY);
}

static void zone_nav(Box b) {
    Box key = lay_split(b, EDGE_LEFT, b.w * 0.36f, &b);
    rrectfill((int)key.x, (int)key.y, (int)key.w, (int)key.h, 2, CLR_BROWNISH_BLACK);
    print("< KEY C >", (int)key.x + 3, (int)(key.y + key.h / 2 - 2), CLR_YELLOW);
    Box play = lay_split(b, EDGE_RIGHT, lay_clamp(b.h, 14, 22), &b);
    rrectfill((int)play.x, (int)play.y, (int)play.w, (int)play.h, 2, CLR_DARK_BLUE);
    print("\x10", (int)(play.x + play.w / 2 - 2), (int)(play.y + play.h / 2 - 2), CLR_LIME_GREEN);
    rrectfill((int)b.x + 2, (int)b.y, (int)b.w - 2, (int)b.h, 2, CLR_BROWNISH_BLACK);
    print("BOSSA", (int)b.x + 6, (int)(b.y + b.h / 2 - 2), CLR_ORANGE);
}

// GLASS material — a recessed dark display panel with a bezel. Everything on the
// glass is lit/flat (a screen), never chunky (that's the chassis' job).
static Box glass_panel(Box b) {
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 3, CLR_BLACK);   // the dark glass
    rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, CLR_DARK_GREY);       // bezel
    return lay_inset(b, 3);                                           // the drawable interior
}

// mock: which cells are P-LOCKED (a per-cell tweak on top of "follows the chord").
static const int LOCK[VOICES][CELLS] = {
    /* CH */ {0,0,0,0,0,0,0,0},   // chords show a numeral, not a pip
    /* BA */ {0,0,1,0,0,0,0,0},   // a p-locked bass run on bar 3
    /* ME */ {0,0,0,0,0,0,0,0},   // melody all default
    /* DR */ {0,0,0,1,0,0,0,1},   // drum FILLS p-locked on bars 4 & 8
    /* PA */ {0,0,0,0,0,0,0,0},
};
// EXPERIMENT: the full 5-LANE TRACKER — CHORDS/BASS/MEL/DRUMS/PAD stacked, 8 cells
// each, aligned with the rail row headers. Chords = numeral; other voices = a pip
// (grey = follows, orange = p-locked). The whole arrangement at a glance.
static void glass_grid(Box g) {
    for (int v = 0; v < VOICES; v++) {
        Box lane = lay_grid(g, 1, VOICES, v, 1);
        for (int i = 0; i < CELLS; i++) {
            Box c = lay_inset(lay_grid(lane, 8, CELLS, i, 1), 0);
            int on = i < CH_ON, cur = i == playhead, lit = VON[v];
            rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 1, cur ? CLR_DARK_BLUE : CLR_DARKER_BLUE);
            if (v == 0) {   // CHORDS — the numeral (tap opens its editor)
                if (on && ui_button((int)c.x, (int)c.y, (int)c.w, (int)c.h, "")) selCell = i;
                if (on) print(CH_RN[i], (int)(c.x + c.w / 2 - text_width(CH_RN[i]) / 2), (int)(c.y + c.h / 2 - 2),
                              cur ? CLR_WHITE : CLR_LIGHT_GREY);
            } else if (lit && on) {   // other voices — a pip; orange = p-locked
                rectfill((int)(c.x + c.w / 2 - 1), (int)(c.y + c.h / 2 - 1), 3, 3,
                         LOCK[v][i] ? CLR_ORANGE : CLR_MEDIUM_GREY);
            }
            rect((int)c.x, (int)c.y, (int)c.w, (int)c.h, cur ? CLR_LIME_GREEN : CLR_DARKER_GREY);
        }
    }
}

// the VOICE RAIL — the tracker's row headers. Each header is placed at the SAME
// vertical slot as its glass lane (derived from `g`, the glass interior), so the
// rail lines up exactly with the lanes to its right.
static void zone_rail(Box rail, Box g) {
    for (int i = 0; i < VOICES; i++) {
        Box lane = lay_grid(g, 1, VOICES, i, 1);              // the lane's y/h (in the glass)
        Box row = box(rail.x, lane.y, rail.w, lane.h);        // same y/h, in the rail column
        rrectfill((int)row.x, (int)row.y, (int)row.w, (int)row.h, 2, CLR_BROWNISH_BLACK);
        rect((int)row.x, (int)row.y, (int)row.w, (int)row.h, VON[i] ? CLR_ORANGE : CLR_DARKER_GREY);
        print(VL[i], (int)(row.x + row.w / 2 - text_width(VL[i]) / 2), (int)(row.y + row.h / 2 - 2),
              VON[i] ? CLR_WHITE : CLR_DARK_GREY);
        rectfill((int)(row.x + row.w - 4), (int)row.y + 2, 2, 2, VON[i] ? CLR_LIME_GREEN : CLR_DARK_GREY);
    }
}

// a tiny keybed — white keys as a row, black keys overlaid at the piano gaps
// (none after E or B). One root highlighted as "held". The chord-root input.
static void zone_keybed(Box b) {
    int nwhite = 10;                        // ~1.5 octaves, compact
    int rootWhite = 0;                      // mock: C is the "held" root
    float ww = b.w / nwhite;
    for (int i = 0; i < nwhite; i++) {      // white keys
        Box k = box(b.x + i * ww, b.y, ww - 1, b.h);
        rrectfill((int)k.x, (int)k.y, (int)k.w, (int)k.h, 1, i == rootWhite ? CLR_ORANGE : CLR_WHITE);
    }
    for (int i = 0; i < nwhite - 1; i++) {  // black keys (right of C,D,F,G,A → not E,B)
        int s = i % 7;
        if (s == 2 || s == 6) continue;
        rectfill((int)(b.x + (i + 1) * ww - ww * 0.32f), (int)b.y,
                 (int)(ww * 0.64f), (int)(b.h * 0.6f), CLR_BLACK);
    }
}

// the EDITOR, drawn ON the glass (chassis stays put around it): a chord spinner on
// the left + this cell's OWN params on the right. Tapping the glass swaps the chart
// for this — the display morphs, the device buttons don't move.
static void glass_editor(Box g) {
    Box top = lay_split(g, EDGE_TOP, 12, &g);
    char t[16]; snprintf(t, sizeof t, "BAR %d", selCell + 1);
    print(t, (int)top.x + 1, (int)(top.y + 3), CLR_LIGHT_GREY);
    Box back = lay_split(top, EDGE_RIGHT, 34, &top);
    if (ui_button((int)back.x, (int)back.y, (int)back.w, (int)back.h, "BACK")) selCell = -1;

    Box spin = lay_split(g, EDGE_LEFT, g.w * 0.40f, &g);
    Box up = lay_split(spin, EDGE_TOP, 13, &spin);
    Box dn = lay_split(spin, EDGE_BOTTOM, 13, &spin);
    ui_button((int)up.x, (int)up.y, (int)up.w, (int)up.h, "\x1e");   // ▲ next in key
    ui_button((int)dn.x, (int)dn.y, (int)dn.w, (int)dn.h, "\x1f");   // ▼ prev in key
    rrectfill((int)spin.x, (int)spin.y, (int)spin.w, (int)spin.h, 2, CLR_DARKER_BLUE);
    print(CH_NAME[selCell], (int)(spin.x + spin.w / 2 - text_width(CH_NAME[selCell]) / 2), (int)(spin.y + spin.h / 2 - 5), CLR_WHITE);
    print(CH_RN[selCell], (int)(spin.x + spin.w / 2 - text_width(CH_RN[selCell]) / 2), (int)(spin.y + spin.h / 2 + 1), CLR_YELLOW);

    // this cell's OWN params — the whole point of the sequencer (glass chips, stacked)
    static const char *PL[3] = { "STRUM", "INV", "VOICE" };
    static const char *PV[3] = { "DOWN",  "1st", "MID"   };
    Box p = lay_inset(g, 2);
    for (int i = 0; i < 3; i++) {
        Box r = lay_inset(lay_grid(p, 1, 3, i, 2), 0);
        rrectfill((int)r.x, (int)r.y, (int)r.w, (int)r.h, 2, CLR_DARKER_BLUE);
        rect((int)r.x, (int)r.y, (int)r.w, (int)r.h, CLR_DARK_GREY);
        print(PL[i], (int)r.x + 3, (int)(r.y + r.h / 2 - 2), CLR_INDIGO);
        print(PV[i], (int)(r.x + r.w - text_width(PV[i]) - 3), (int)(r.y + r.h / 2 - 2), CLR_WHITE);
    }
}

void draw(void) {
    face_resize();                          // 160×100 chunky canvas
    Box area = face_area(3);
    Face f = face_layout(area, ZONES, NZ, CELLS);

    cls(CLR_DARKER_BLUE);
    rrectfill(0, 0, screen_w(), screen_h(), 6, CLR_INDIGO);   // the molded device BODY (chassis)

    font(FONT_TINY);
    ui_begin();
    zone_nav(f.box[0]);                     // chassis
    Box body = f.box[1];
    Box rail = lay_split(body, EDGE_LEFT, 20, &body);   // left voice soft-keys (chassis) | glass
    Box g = glass_panel(body);              // the GLASS — the whole right region
    zone_rail(rail, g);                     // chassis headers, aligned to the glass lanes
    if (selCell >= 0) glass_editor(g); else glass_grid(g);   // the display morphs; chassis stays
    zone_keybed(f.box[2]);                  // chassis
    font(FONT_NORMAL);
    ui_end();
    cursor_draw(CUR_HAND);
}
