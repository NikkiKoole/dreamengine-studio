/* de:meta
{
  "slug": "roomyface",
  "collection": ["device-face", "responsive"],
  "title": "iPad spread mockup — B tile vs C unhide",
  "status": "wip",
  "created": "2026-07-20",
  "kind": ["instrument"],
  "genre": null,
  "teaches": [],
  "resizable": true,
  "description": {
    "summary": "Draw-only LAYOUT STUDY for the tablet/iPad-Pro question in responsive-first-device-face.md: how a device face should spread onto a much larger screen. Two arrangements to vibe-check (keys 1/2): B = SHOW MORE — tile the whole rack (four machine faces at once, 2x2); C = UNHIDE THE DEPTH — one machine using the whole screen with everything that pages behind soft-keys on a phone shown at once. Both are built on the SAME face.h grammar (face_layout per cell for B; one fuller FaceZone[] for C), proving the seam needs no engine work — only a device_class()==ROOMY branch.",
    "detail": "The two axes of canvas-density-spectrum made concrete on an iPad-Pro-shaped canvas (4:3). B keeps each face at ~phone density but gives the canvas room for FOUR — the paradigm's 'show more, not rearrange'; best for a multi-machine rack (acidcandy). C spends the room on ONE machine unhidden — the acidwide A-H bet at tablet scale; best for a single deep instrument. face.h supports both for free: B calls face_layout() on each of a 2x2 lay_grid's cells; C calls it once with a wider zone table. No audio, fake data — this is a LOOK study to pick the direction before touching acidcandy.",
    "controls": "1 = arrangement B (tile the rack). 2 = arrangement C (unhide one machine). Draw-only mockup."
  }
}
de:meta */
#include "studio.h"
#include "lay.h"
#include "face.h"

extern void de_resize(int w, int h);

// iPad-Pro spread study — B (tile the rack) vs C (unhide one). Draw-only.
static int arr = 0;   // 0 = B tiled, 1 = C unhidden

// fake pattern data (shared by the mockup faces)
static const int P_ON[16]  = { 1,0,1,0, 1,1,0,1, 1,0,1,0, 0,1,1,0 };
static const int P_PIT[16] = { 0,0,7,0, 3,10,0,12, 5,0,8,0, 0,15,3,0 };
static const int P_ACC[16] = { 1,0,0,0, 1,0,0,1, 0,0,1,0, 0,1,0,0 };
static const int P_SLD[16] = { 0,0,1,0, 0,1,0,0, 0,0,1,0, 0,0,1,0 };
static const int P_TIE[16] = { 0,0,0,1, 0,0,0,0, 1,0,0,0, 0,0,0,1 };
#define PMAX 17

static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// a static candy knob sized to its cell (draw-only)
static void mknob(Box c, const char *label, float v, int accent) {
    float rh = c.h * 0.30f, rw = c.w * 0.40f;
    int r = (int)lay_clamp(rh < rw ? rh : rw, 4, 11);
    int cx = (int)(c.x + c.w / 2), cy = (int)(c.y + r + 1);
    if (cy + r + 6 > (int)(c.y + c.h)) cy = (int)(c.y + c.h) - r - 6;
    float ang = 150 + v * 240;
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 1, r, 150, ang, accent);
    circ(cx, cy, r, CLR_BROWNISH_BLACK);
    line(cx, cy, cx + (int)dx(r - 1, ang), cy + (int)dy(r - 1, ang), CLR_WHITE);
    font(FONT_TINY); if (label) plabel(label, cx, cy + r + 1, CLR_LIGHT_GREY);
}

static int roll_y(Box rb, int pit) { return (int)(rb.y + rb.h - 2 - (pit / (float)(PMAX + 1)) * (rb.h - 4)); }

// ── one COMPACT machine face (for the B tiles) ───────────────────────────────
static FaceZone MINI[] = {
    { FACE_BAND, EDGE_TOP,    0.15f, "nav"   },
    { FACE_BAND, EDGE_TOP,    0.24f, "knobs" },
    { FACE_HERO, 0,           0.00f, "scr"   },
    { FACE_BAND, EDGE_BOTTOM, 0.22f, "step"  },
};
static void mini_face(Box cell, const char *name, int accent, int lo) {
    rrectfill((int)cell.x, (int)cell.y, (int)cell.w, (int)cell.h, 5, CLR_INDIGO);
    Box panel = lay_inset(cell, 3);
    rrectfill((int)panel.x, (int)panel.y, (int)panel.w, (int)panel.h, 4, CLR_DARK_PURPLE);
    Face f = face_layout(lay_inset(panel, 3), MINI, 4, 16);
    // ① nav: name + transport dot
    Box nav = f.box[0];
    rrectfill((int)nav.x, (int)nav.y, (int)nav.w, (int)nav.h, 3, lo);
    circfill((int)nav.x + 6, (int)(nav.y + nav.h / 2), 3, CLR_LIME_GREEN);
    print(name, (int)nav.x + 13, (int)(nav.y + nav.h / 2 - 3), CLR_WHITE);
    // ② knobs (4)
    const char *K[4] = { "CUT", "RES", "ENV", "DEC" };
    float V[4] = { 0.6f, 0.7f, 0.45f, 0.5f };
    for (int i = 0; i < 4; i++) mknob(lay_grid(f.box[1], 4, 4, i, 2), K[i], V[i], accent);
    // ③ screen: a small piano-roll on the register
    Box scr = f.box[2];
    rrectfill((int)scr.x, (int)scr.y, (int)scr.w, (int)scr.h, 2, CLR_DARKER_GREY);
    Box si = lay_inset(scr, 2);
    for (int s = 0; s < 16; s++) {
        if (!P_ON[s]) continue;
        Box c = face_col(&f, si, s, 0);
        rectfill((int)c.x, roll_y(si, P_PIT[s]), (int)c.w - 1, 2, P_ACC[s] ? CLR_WHITE : accent);
    }
    // ④ step lane
    Box st = f.box[3];
    for (int s = 0; s < 16; s++) {
        Box c = lay_inset(face_col(&f, st, s, 1), 0);
        rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 1, P_ON[s] ? accent : CLR_DARK_BLUE);
    }
}

// ── B: tile the whole rack (four machines at once) ───────────────────────────
static void draw_tiled(Box area) {
    const char *MN[4] = { "303 A", "303 B", "808", "909" };
    int MC[4] = { CLR_PINK, CLR_ORANGE, CLR_TRUE_BLUE, CLR_LIME_GREEN };
    int ML[4] = { CLR_DARK_PURPLE, CLR_DARK_ORANGE, CLR_DARK_BLUE, CLR_DARK_GREEN };
    for (int m = 0; m < 4; m++)
        mini_face(lay_inset(lay_grid(area, 2, 4, m, 6), 2), MN[m], MC[m], ML[m]);
}

// ── C: unhide ONE machine — the whole screen, everything shown at once ────────
static FaceZone WIDE[] = {
    { FACE_BAND, EDGE_TOP,    0.09f, "nav"    },
    { FACE_BAND, EDGE_TOP,    0.20f, "knobs"  },
    { FACE_HERO, 0,           0.00f, "screen" },
    { FACE_BAND, EDGE_BOTTOM, 0.10f, "step"   },
    { FACE_BAND, EDGE_BOTTOM, 0.16f, "keys"   },
};
static void lane_row(Face *f, Box band, const int *flag, int accent, const char *label) {
    for (int s = 0; s < 16; s++) {
        Box c = face_col(f, band, s, 1);
        rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 1, flag[s] ? accent : CLR_DARK_BLUE);
    }
    font(FONT_TINY); print(label, (int)band.x - 1, (int)band.y - 5, CLR_LIGHT_GREY);
}
static void draw_unhidden(Box area) {
    rrectfill((int)area.x, (int)area.y, (int)area.w, (int)area.h, 6, CLR_INDIGO);
    Box panel = lay_inset(area, 4);
    rrectfill((int)panel.x, (int)panel.y, (int)panel.w, (int)panel.h, 5, CLR_DARK_PURPLE);
    Face f = face_layout(lay_inset(panel, 4), WIDE, 5, 16);

    // ① nav — name + transport + PAT banks (all visible)
    Box nav = f.box[0];
    rrectfill((int)nav.x, (int)nav.y, (int)nav.w, (int)nav.h, 3, CLR_DARK_PURPLE);
    circfill((int)nav.x + 8, (int)(nav.y + nav.h / 2), 4, CLR_LIME_GREEN);
    print("303 A  \x10  UNHIDDEN", (int)nav.x + 18, (int)(nav.y + nav.h / 2 - 3), CLR_WHITE);
    const char *PB[4] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; i++) { Box b = lay_grid(lay_split(nav, EDGE_RIGHT, nav.w * 0.28f, &nav), 4, 4, i, 3);
        rrectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, 2, i == 0 ? CLR_PINK : CLR_DARK_BLUE);
        plabel(PB[i], (int)(b.x + b.w / 2), (int)(b.y + b.h / 2 - 3), CLR_WHITE); }

    // ② the FULL knob roster — everything that pages behind soft-keys on a phone
    const char *KN[13] = { "CUT","RES","ENV","DEC","ACC","SUB","ADEC","SLDT","TRK","DRV","SEND","VERB","DRIFT" };
    float KV[13] = { .55f,.70f,.55f,.45f,.55f,.30f,.40f,.14f,.20f,.35f,.10f,.20f,.50f };
    for (int i = 0; i < 13; i++) mknob(lay_grid(f.box[1], 13, 13, i, 3), KN[i], KV[i], CLR_PINK);

    // ③ the big SCREEN — piano-roll + ACC/SLD/TIE lanes, all full-width on the register
    Box scr = f.box[2];
    rrectfill((int)scr.x, (int)scr.y, (int)scr.w, (int)scr.h, 3, CLR_DARKER_GREY);
    Box si = lay_inset(scr, 4);
    Box lanes = lay_split(si, EDGE_BOTTOM, si.h * 0.34f, &si);              // reserve 3 flag lanes at the bottom
    // the roll (pitch × step)
    for (int s = 0; s < 16; s++) {
        Box c = face_col(&f, si, s, 0);
        if (s == 7) { blend(BLEND_AVG); rectfill((int)c.x, (int)si.y, (int)c.w, (int)si.h, CLR_MEDIUM_GREY); blend_reset(); }  // fake playhead
        if (!P_ON[s]) continue;
        int y = roll_y(si, P_PIT[s]);
        rectfill((int)c.x + 1, y, (int)c.w - 2, 3, P_ACC[s] ? CLR_WHITE : CLR_PINK);
    }
    Box acc = lay_split(lanes, EDGE_TOP, lanes.h / 3.0f, &lanes);
    Box sld = lay_split(lanes, EDGE_TOP, lanes.h / 2.0f, &lanes);
    lane_row(&f, lay_inset(acc, 1), P_ACC, CLR_ORANGE,     "ACC");
    lane_row(&f, lay_inset(sld, 1), P_SLD, CLR_LIME_GREEN, "SLD");
    lane_row(&f, lay_inset(lanes, 1), P_TIE, CLR_TRUE_BLUE, "TIE");

    // ④ step lane
    Box st = f.box[3];
    for (int s = 0; s < 16; s++) {
        Box c = lay_inset(face_col(&f, st, s, 2), 0);
        rrectfill((int)c.x, (int)c.y, (int)c.w, (int)c.h, 2, P_ON[s] ? (P_ACC[s] ? CLR_ORANGE : CLR_PINK) : CLR_DARK_BLUE);
    }
    // ⑤ keybed — a full chromatic octave+ across the whole width
    Box kb = f.box[4];
    int WK = 14;
    for (int i = 0; i < WK; i++) { Box k = lay_grid(kb, WK, WK, i, 1);
        rrectfill((int)k.x, (int)k.y, (int)k.w, (int)k.h, 2, CLR_WHITE); }
    for (int i = 0; i < WK - 1; i++) { if (i % 7 == 2 || i % 7 == 6) continue;
        Box w = lay_grid(kb, WK, WK, i, 1);
        int bw = (int)(w.w * 0.62f);
        rectfill((int)(w.x + w.w - bw / 2), (int)kb.y, bw, (int)(kb.h * 0.62f), CLR_BLACK); }
}

void draw(void) {
    de_resize(420, 315);                       // force an iPad-Pro-ish 4:3 canvas for the study
    if (keyp('1')) arr = 0;
    if (keyp('2')) arr = 1;

    cls(CLR_BROWNISH_BLACK);
    Box scr = box(0, 0, screen_w(), screen_h());
    Box area = lay_split(scr, EDGE_TOP, 12, &scr);        // a thin caption bar
    font(FONT_SMALL);
    print(arr == 0 ? "B \x1a SHOW MORE: tile the rack (best for a multi-machine rack)"
                   : "C \x1a UNHIDE THE DEPTH: one machine, everything at once (best for a single deep instrument)",
          3, 3, CLR_LIGHT_GREY);
    print("1 / 2", screen_w() - 30, 3, CLR_DARK_GREY);

    if (arr == 0) draw_tiled(lay_inset(scr, 4));
    else          draw_unhidden(lay_inset(scr, 2));
}
