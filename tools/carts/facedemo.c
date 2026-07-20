/* de:meta
{
  "slug": "facedemo",
  "collection": ["device-face", "responsive"],
  "title": "Face GRAMMAR demo (face.h)",
  "status": "wip",
  "created": "2026-07-20",
  "kind": ["instrument"],
  "genre": null,
  "teaches": [],
  "resizable": true,
  "description": {
    "summary": "Layer 3 of responsive-first-device-face.md: the same five-zone device face as the deviceface STARTER, but DECLARED through face.h's grammar instead of hand-carved. The whole layout is a 5-row FaceZone[] table; face.h owns the chunky-canvas resize, the safe-area content box, carving the zones, and the shared 16-column register — and ENFORCES the arrangement rules (bands dock top/bottom only, so no side-rail can steal the per-step lane's width; the hero is always the remainder).",
    "detail": "Where deviceface teaches the mechanism (raw lay.h), this teaches the GRAMMAR. draw() shrinks to: face_resize(); face_area(3); face_layout(area, ZONES, 5, 16); then draw into f.box[0..4]. The per-step SCOPE overlay (hero) and the step grid (a FACE_LANE zone) both place columns through face_col(), so they align by construction. The device_class() hook picks the zone table per breakpoint (TALL/WIDE share the compact table; ROOMY gives the hero more room) — the arrangement axis, one branch. Scaffold not straitjacket: every zone is a plain Box; a bespoke face just doesn't call face_layout(). Silent (teaches the layout).",
    "controls": "Tap [WAVE]/[SEQ]/[MIX] to page the display. Tap the 16 steps to toggle. Drag the 8 knobs. Tap the pads. Resize / rotate — face.h reflows it, never scales."
  }
}
de:meta */
#include "studio.h"
#include "lay.h"
#include "ui.h"
#include "face.h"
#include "cursor.h"

// ── THE WHOLE LAYOUT — one declarative table (read it top→bottom on screen) ───
// Compare draw() below to deviceface's hand-carved version: the arrangement lives
// HERE, and face.h enforces the principles so this table can't express a bad one
// (no LEFT/RIGHT edge = no width-stealing side-rail).
static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.12f, "nav"     },   // ① transport + view tabs
    { FACE_BAND, EDGE_TOP,    0.30f, "knobs"   },   // ② 8 continuous controls
    { FACE_HERO, 0,           0.00f, "display" },   // ③ the touchable screen (remainder)
    { FACE_LANE, EDGE_BOTTOM, 0.20f, "steps"   },   // ④ 16 per-step cells (the register)
    { FACE_BAND, EDGE_BOTTOM, 0.22f, "perform" },   // ⑤ pads
};
// ROOMY (tablet) variant — the arrangement axis: same zones, the hero keeps more.
static FaceZone ZONES_ROOMY[] = {
    { FACE_BAND, EDGE_TOP,    0.10f, "nav"     },
    { FACE_BAND, EDGE_TOP,    0.22f, "knobs"   },
    { FACE_HERO, 0,           0.00f, "display" },
    { FACE_LANE, EDGE_BOTTOM, 0.16f, "steps"   },
    { FACE_BAND, EDGE_BOTTOM, 0.18f, "perform" },
};
#define NZ 5
#define STEPS 16

static float knobs[8] = { 0.6f, 0.4f, 0.7f, 0.3f, 0.5f, 0.55f, 0.25f, 0.8f };
static const char *KLAB[8] = { "OSC", "CUT", "RES", "ENV", "LFO", "FX", "DLY", "VOL" };
static int   tab, playing = 1, g_frame;
static int   step_on[STEPS] = { 1,0,1,0, 1,1,0,0, 1,0,1,0, 0,1,1,0 };

static int tab_btn(Box b, const char *s, int on) {
    int hit = ui_button((int)b.x, (int)b.y, (int)b.w, (int)b.h, "");
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, on ? CLR_ORANGE : CLR_DARK_BLUE);
    print(s, (int)(b.x + b.w / 2 - text_width(s) / 2), (int)(b.y + b.h / 2 - 3), on ? CLR_BLACK : CLR_LIGHT_GREY);
    return hit;
}

static void zone_nav(Box nav) {
    Box play = lay_split(nav, EDGE_LEFT, lay_clamp(nav.h, 12, 24), &nav);
    if (ui_button((int)play.x, (int)play.y, (int)play.w, (int)play.h, "")) playing = !playing;
    rrectfill((int)play.x, (int)play.y, (int)play.w, (int)play.h, 2, CLR_DARK_BLUE);
    print(playing ? "\x10" : "\xdb", (int)(play.x + play.w / 2 - 2), (int)(play.y + play.h / 2 - 3), CLR_LIME_GREEN);
    Box name = lay_split(nav, EDGE_RIGHT, nav.w * 0.24f, &nav);
    print("GRAMMAR", (int)(name.x + 2), (int)(name.y + name.h / 2 - 3), CLR_LIGHT_GREY);
    const char *T[3] = { "WAVE", "SEQ", "MIX" };
    for (int i = 0; i < 3; i++)
        if (tab_btn(lay_grid(lay_inset(nav, 1), 3, 3, i, 2), T[i], tab == i)) tab = i;
}

// ③ the display — content swaps by tab; the per-step overlay shares the register.
static void zone_display(Box scr, Face *f) {
    rrectfill((int)scr.x, (int)scr.y, (int)scr.w, (int)scr.h, 3, CLR_DARKER_GREY);
    Box pad = lay_inset(scr, 3);
    int head = (g_frame / 8) % STEPS;
    if (tab == 0) {                                  // WAVE — a scope, with per-step tick marks (via face_col)
        int cy = (int)(pad.y + pad.h / 2), prev = cy;
        for (int x = 0; x < (int)pad.w; x++) {
            float ph = (g_frame * 0.05f) + x * 0.20f;
            int y = cy + (int)(sin_deg(ph * 12.0f) * pad.h * 0.32f * (0.5f + 0.5f * knobs[0]));
            if (x) line((int)pad.x + x - 1, prev, (int)pad.x + x, y, CLR_LIME_GREEN);
            prev = y;
        }
        for (int i = 0; i < STEPS; i++) {            // step ticks — SAME lane as zone ④ below
            Box col = face_col(f, pad, i, 0);
            int c = step_on[i] ? (playing && i == head ? CLR_WHITE : CLR_ORANGE) : CLR_DARK_GREY;
            pset((int)col.x, (int)(pad.y + pad.h - 1), c);
            pset((int)col.x, (int)pad.y, c);
        }
    } else if (tab == 1) {                           // SEQ — the piano-roll-ish bars, on the register
        for (int i = 0; i < STEPS; i++) {
            Box col = face_col(f, pad, i, 0);
            if (step_on[i]) {
                Box bar = lay_inset(col, 1);
                rectfill((int)bar.x, (int)(bar.y + bar.h * 0.35f), (int)bar.w, (int)(bar.h * 0.6f),
                         (playing && i == head) ? CLR_WHITE : CLR_ORANGE);
            }
        }
    } else {                                         // MIX — level bars
        const char *ML[4] = { "OSC", "SUB", "FX", "OUT" };
        for (int i = 0; i < 4; i++) {
            Box c = lay_grid(pad, 4, 4, i, 4);
            Box bar = lay_split(c, EDGE_BOTTOM, c.h * (0.35f + 0.55f * knobs[i]), &c);
            rectfill((int)bar.x, (int)bar.y, (int)bar.w, (int)bar.h, CLR_LIME_GREEN);
            print(ML[i], (int)bar.x, (int)(c.y + c.h - 6), CLR_LIGHT_GREY);
        }
    }
}

// ④ the step grid — a FACE_LANE zone, columns from the SAME register as the display.
static void zone_steps(Box row, Face *f) {
    int head = (g_frame / 8) % STEPS;
    for (int i = 0; i < STEPS; i++) {
        Box c = face_col(f, row, i, 2);
        if (ui_button((int)c.x, (int)c.y, (int)c.w, (int)c.h, "")) step_on[i] = !step_on[i];
        int col = step_on[i] ? CLR_ORANGE : CLR_DARK_BLUE;
        if (playing && i == head) col = step_on[i] ? CLR_WHITE : CLR_MEDIUM_GREY;
        rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 2, col);
    }
}

// ⑤ pads (draw-only)
static void zone_perform(Box b) {
    for (int i = 0; i < 8; i++) {
        Box p = lay_inset(lay_grid(b, 8, 8, i, 2), 1);
        rrectfill((int)p.x, (int)p.y, (int)p.w, (int)p.h, 2, CLR_BLUE_GREEN);
    }
}

void draw(void) {
    g_frame++;
    face_resize();                                   // ① chunky canvas
    Box area = face_area(3);                          // ② content = screen inset 3, ∩ safe-rect
    FaceZone *zt = (device_class() == 2) ? ZONES_ROOMY : ZONES;   // arrangement axis: table per breakpoint
    Face f = face_layout(area, zt, NZ, STEPS);        // ③ carve + enforce + build the register

    cls(CLR_DARK_PURPLE);
    Box full = box(0, 0, screen_w(), screen_h());     // chassis bleeds to every edge
    rrectfill((int)full.x, (int)full.y, (int)full.w, (int)full.h, 6, CLR_DARK_BLUE);
    rrectfill((int)area.x - 2, (int)area.y - 2, (int)area.w + 4, (int)area.h + 4, 4, CLR_INDIGO);

    font(FONT_SMALL);
    ui_begin();
    zone_nav(f.box[0]);
    for (int i = 0; i < 8; i++)                       // ② knobs — cell-sized (ui.h), in a BAND
        ui_knob_cell(lay_grid(f.box[1], 8, 8, i, 2), &knobs[i], KLAB[i]);
    zone_display(f.box[2], &f);
    zone_steps(f.box[3], &f);
    zone_perform(f.box[4]);
    font(FONT_NORMAL);
    ui_end();
    cursor_draw(mouse_down(MOUSE_LEFT) ? CUR_GRAB : CUR_HAND);
}
