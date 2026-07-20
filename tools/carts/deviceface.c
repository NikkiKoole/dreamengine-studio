/* de:meta
{
  "slug": "deviceface",
  "collection": ["device-face", "responsive"],
  "title": "Device-face STARTER (responsive-first template)",
  "status": "wip",
  "created": "2026-07-20",
  "kind": ["instrument"],
  "genre": null,
  "teaches": [],
  "resizable": true,
  "description": {
    "summary": "The COPY-ME skeleton for a responsive-first device face. It starts responsive-and-opinionated instead of being retrofitted: the chunky-canvas resize is wired, the five zones (nav · knobs · display · step-lane · perform) are stacked, and the per-step 16-column REGISTER is shared between the display's roll and the step grid so alignment is structural, not hand-matched. Fork this, don't re-derive it.",
    "detail": "Layer 1 of responsive-first-device-face.md — the 'device-face starter'. Encodes the four constraints the acidwide A-H study surfaced: (1) the column register — per-step things share one full-width 16-column x-lane; (2) per-step vs band — knobs/nav/transport go in horizontal BANDS, never a side-rail that steals the grid's width; (3) paging is fine — the nav tabs page the display, the step lane is permanent; (4) widgets size to their CELL (ui_knob_cell sizes to the cell). It drives the Layer-2 primitives it motivated: lay.h's LayLane register (lay_lane / lay_lane_cell) and ui.h's ui_knob_cell / ui_button_cell. Silent on purpose: it teaches the LAYOUT, you drop your instrument into the marked seams. The escape hatch: every zone is a plain lay.h Box, so drop to raw lay.h anywhere.",
    "controls": "Tap ▶ transport. Tap [SEQ]/[SND]/[MIX] to page the display. Tap the 16 step cells to toggle the pattern (watch the playhead sweep). Drag the knobs. Resize the window / rotate the device — it reflows, never scales."
  }
}
de:meta */
#include "studio.h"
#include "lay.h"
#include "ui.h"
#include "cursor.h"

extern void de_resize(int w, int h);   // engine seam (studio.c): set the active canvas — cart-drivable

// ─────────────────────────────────────────────────────────────────────────────
//  DEVICE-FACE STARTER — the responsive-first template.
//  Read top-to-bottom; the structure IS the lesson. See
//  docs/design/responsive-first-device-face.md (Layer 1) and
//  docs/design/device-face-paradigm.md (the five zones).
// ─────────────────────────────────────────────────────────────────────────────

#define STEPS 16

static float knobs[6] = { 0.55f, 0.70f, 0.40f, 0.55f, 0.30f, 0.20f };
static const char *KLAB[6] = { "CUT", "RES", "ENV", "DEC", "DRV", "VERB" };

static int   tab;                 // which page the DISPLAY shows: 0 SEQ · 1 SND · 2 MIX (the screen owns modes)
static int   playing = 1;
static int   step_on[STEPS] = { 1,0,0,1, 1,0,1,0, 1,0,0,1, 0,1,0,0 };
static int   g_frame;

// ── THE REGISTER (constraint #1) ─────────────────────────────────────────────
// Per-step things share ONE 16-column x-lane (lay.h's `Lane`) so a step reads as
// one vertical slice. We build the lane ONCE per frame from the full content width;
// BOTH the display's roll (zone 3) and the step grid (zone 4) place their columns
// through lay_lane_cell() — alignment is guaranteed, not hand-matched. (This was
// hand-rolled g_lane_x/g_lane_w here; Layer 2 graduated it into lay.h.)
static LayLane g_lane;   // set in draw()

// Widgets size to their CELL (constraint #4) via ui.h's `ui_knob_cell(Box, *v, label)`
// — no hand-tuned radius. (Also once a local helper here; graduated into ui.h in Layer 2.)

static int tab_btn(Box b, const char *s, int on) {
    int hit = ui_button((int)b.x, (int)b.y, (int)b.w, (int)b.h, "");
    rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, on ? CLR_PINK : CLR_DARK_BLUE);
    print(s, (int)(b.x + b.w / 2 - text_width(s) / 2), (int)(b.y + b.h / 2 - 3),
          on ? CLR_WHITE : CLR_LIGHT_GREY);
    return hit;
}

// ── ZONE 1 · NAV SPINE (a BAND — constraint #2) ──────────────────────────────
// transport + the coarse VIEW tabs + a name. Tabs PAGE the display (constraint #3:
// paging is fine — you don't need every control on screen at once).
static void zone_nav(Box nav) {
    Box play = lay_split(nav, EDGE_LEFT, lay_clamp(nav.h, 12, 24), &nav);
    if (ui_button((int)play.x, (int)play.y, (int)play.w, (int)play.h, "")) playing = !playing;
    rrectfill((int)play.x, (int)play.y, (int)play.w, (int)play.h, 2, CLR_DARK_BLUE);
    print(playing ? "\x10" : "\xdb", (int)(play.x + play.w / 2 - 2), (int)(play.y + play.h / 2 - 3), CLR_GREEN);

    Box name = lay_split(nav, EDGE_RIGHT, nav.w * 0.28f, &nav);
    print("STARTER", (int)(name.x + 2), (int)(name.y + name.h / 2 - 3), CLR_LIGHT_GREY);

    const char *T[3] = { "SEQ", "SND", "MIX" };
    for (int i = 0; i < 3; i++)
        if (tab_btn(lay_grid(lay_inset(nav, 1), 3, 3, i, 2), T[i], tab == i)) tab = i;
}

// ── ZONE 3 · THE CONTEXT DISPLAY (the hero — the remainder) ───────────────────
// A touchable screen whose content swaps by the nav tab. In SEQ it renders the
// per-step roll THROUGH the register, so it lines up with the zone-4 grid below.
static void zone_display(Box scr) {
    rrectfill((int)scr.x, (int)scr.y, (int)scr.w, (int)scr.h, 3, CLR_DARKER_GREY);
    Box pad = lay_inset(scr, 3);

    if (tab == 0) {                                    // SEQ — the per-step roll (bound to the register)
        int head = (g_frame / 8) % STEPS;
        for (int i = 0; i < STEPS; i++) {
            Box col = lay_lane_cell(g_lane, pad, i, 0); // ← same lane as the step grid → aligned
            if (step_on[i]) {
                Box bar = lay_inset(col, 1);
                rectfill((int)bar.x, (int)(bar.y + bar.h * 0.35f), (int)bar.w, (int)(bar.h * 0.6f),
                         (playing && i == head) ? CLR_WHITE : CLR_PINK);
            }
            if (playing && i == head)
                line((int)col.x, (int)pad.y, (int)col.x, (int)(pad.y + pad.h), CLR_DARK_GREY);
        }
    } else if (tab == 1) {                             // SND — deeper params (paged in, not always on)
        for (int i = 0; i < 6; i++) ui_knob_cell(lay_grid(pad, 6, 6, i, 3), &knobs[i], KLAB[i]);
    } else {                                           // MIX — a few level bars
        const char *ML[4] = { "VCE", "DRM", "FX", "OUT" };
        for (int i = 0; i < 4; i++) {
            Box c = lay_grid(pad, 4, 4, i, 4);
            Box bar = lay_split(c, EDGE_BOTTOM, c.h * (0.4f + 0.5f * knobs[i % 6]), &c);
            rectfill((int)bar.x, (int)bar.y, (int)bar.w, (int)bar.h, CLR_GREEN);
            print(ML[i], (int)(bar.x), (int)(c.y + c.h - 6), CLR_LIGHT_GREY);
        }
    }
}

// ── ZONE 4 · MODE-SWITCHED GRID (per-step — THE register) ────────────────────
static void zone_steps(Box row) {
    int head = (g_frame / 8) % STEPS;
    for (int i = 0; i < STEPS; i++) {
        Box c = lay_lane_cell(g_lane, row, i, 2);      // ← same lane as the display roll (left edges align)
        if (ui_button((int)c.x, (int)c.y, (int)c.w, (int)c.h, "")) step_on[i] = !step_on[i];
        int col = step_on[i] ? CLR_PINK : CLR_DARK_BLUE;
        if (playing && i == head) col = step_on[i] ? CLR_WHITE : CLR_MEDIUM_GREY;
        rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 2, col);
    }
}

// ── ZONE 5 · PERFORMANCE SURFACE (a BAND — draw-only keybed here) ─────────────
static void zone_perform(Box kb) {
    const int WK = 10;                                 // ten white keys across
    for (int i = 0; i < WK; i++) {
        Box k = lay_grid(kb, WK, WK, i, 1);
        rrectfill((int)k.x, (int)k.y, (int)k.w, (int)k.h, 2, CLR_WHITE);
    }
    for (int i = 0; i < WK - 1; i++) {                 // black keys, skipping E-F / B-C gaps
        if (i % 7 == 2 || i % 7 == 6) continue;
        Box w = lay_grid(kb, WK, WK, i, 1);
        int bw = (int)(w.w * 0.6f);
        rectfill((int)(w.x + w.w - bw / 2), (int)kb.y, bw, (int)(kb.h * 0.6f), CLR_BLACK);
    }
}

void draw(void) {
    g_frame++;

    // ── CHUNKY CANVAS (route 2 of canvas-density-spectrum.md): match the DEVICE
    // ratio at the DESIGN's density — keep the fitting axis at the design value,
    // extend the other just enough to match the ratio, and let the reflow spread
    // the design into the leftover (no bars). REFLOW, never camera-scale (that
    // desyncs ui.h touch). The ratio is invariant under our resize → a fixed point.
    { int cw = screen_w(), ch = screen_h();
      if (cw > 0 && ch > 0) {
          float r = (float)cw / (float)ch;
          int tw, th;
          if (r >= 1.6f) { th = 100; tw = (int)(100.0f * r + 0.5f); }   // wide → keep height
          else           { tw = 160; th = (int)(160.0f / r + 0.5f); }   // tall → keep width
          if (tw != cw || th != ch) de_resize(tw, th);
      } }

    cls(CLR_DARK_PURPLE);
    font(FONT_SMALL);
    ui_begin();

    // chassis bleeds to EVERY edge (no margins); controls stay inside the safe area.
    Box full = box(0, 0, screen_w(), screen_h());
    rrectfill((int)full.x, (int)full.y, (int)full.w, (int)full.h, 6, CLR_INDIGO);
    Box panel = lay_inset(full, 3);
    rrectfill((int)panel.x, (int)panel.y, (int)panel.w, (int)panel.h, 4, CLR_DARK_BLUE);

    int sx, sy, sw, sh; safe_rect(&sx, &sy, &sw, &sh);   // rescaled to our chunky canvas
    float ax0 = panel.x > sx ? panel.x : sx, ay0 = panel.y > sy ? panel.y : sy;
    float ax1 = (panel.x + panel.w) < (sx + sw) ? (panel.x + panel.w) : (float)(sx + sw);
    float ay1 = (panel.y + panel.h) < (sy + sh) ? (panel.y + panel.h) : (float)(sy + sh);
    Box area = lay_inset(box(ax0, ay0, ax1 - ax0, ay1 - ay0), 2);

    // ── THE FIVE-ZONE STACK ──────────────────────────────────────────────────
    // Carve the BANDS off the edges; the DISPLAY is whatever's left (the hero
    // absorbs the extra height a taller device gives). Bands are sized as FRACTIONS
    // of the canvas (design proportions), not finger_px — that's the route-2 rule.
    Box body = area;
    Box nav     = lay_split(body, EDGE_TOP,    body.h * 0.12f, &body);   // ① nav spine
    Box knobrow = lay_split(body, EDGE_TOP,    body.h * 0.26f, &body);   // ② continuous controls
    Box perform = lay_split(body, EDGE_BOTTOM, body.h * 0.20f, &body);   // ⑤ perform surface
    Box steps   = lay_split(body, EDGE_BOTTOM, body.h * 0.22f, &body);   // ④ step grid (per-step)
    Box display = body;                                                  // ③ the hero (remainder)

    // Build the REGISTER from the FULL content width (constraint #2: no side-rail
    // steals the per-step lane's width — knobs/nav are bands ABOVE, not rails).
    g_lane = lay_lane(area, STEPS);

    // ARRANGEMENT SEAM (canvas-density-spectrum.md axis 2): today one focused face,
    // reflowed by the chunky canvas above — good on phone AND tablet. When a tablet
    // should show MORE (several faces at once), branch here on device_class():
    //   if (device_class() == 2) { draw_roomy(area); ui_end(); return; }  // 0 TALL · 1 WIDE · 2 ROOMY
    // Every zone already takes a Box, so that's a clean ADD, not a rewrite.

    zone_nav(nav);
    for (int i = 0; i < 6; i++)                                          // ② knobs — cell-sized, in a BAND
        ui_knob_cell(lay_grid(knobrow, 6, 6, i, 3), &knobs[i], KLAB[i]);
    zone_display(display);                                               // ③
    zone_steps(steps);                                                  // ④
    zone_perform(perform);                                              // ⑤

    // ── DROP YOUR INSTRUMENT IN HERE ─────────────────────────────────────────
    // This starter is SILENT — it teaches the layout, not the sound. Wire your
    // voice off `playing` + `(g_frame/8)%STEPS` + `step_on[]`, and give real
    // meaning to the knobs / display / keybed. See docs/guides/instrument-carts.md.

    font(FONT_NORMAL);
    ui_end();
    cursor_draw(mouse_down(MOUSE_LEFT) ? CUR_GRAB : CUR_HAND);
}
