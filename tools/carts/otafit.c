/* de:meta
{
  "title": "otafit",
  "status": "active",
  "created": "2026-07-03",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "The third device-adaptive-layout.md prototype, and the one that grounds the orientation-lock case in a NUMBER. A layout mock of the ota family (four drag-to-play fretless ribbons - daddy/mommy/junior/dog - a gesture loop bar up top, two step rows below), laid out with the promoted runtime/lay.h. It tests two things the knob racks couldn't: (1) does the lay_* toolkit handle CONTINUOUS drag surfaces (a ribbon is just a Box you hit-test and map finger-x -> pitch), not only discrete tap widgets - and (2) the orientation question, measured: a fretless ribbon's playability IS its length (pitch resolution = px per semitone), so it reports, per device shape, how many semitones a finger covers and how much LONGER the ribbon is in landscape. Wide screens give a ribbon 1.4x (iPad) to 2.2x (iPhone) more pitch resolution -> this is a rack that PREFERS lock-landscape, and now that preference is a threshold a layout-check oracle could read, not a hunch. Presets auto-cycle (iPhone/iPad x portrait/landscape); keys 1-4 lock. No audio - layout + measurement only."
}
de:meta */
// OTAFIT — the ribbon-family layout mock + the orientation-lock MEASUREMENT.
//
// respond/rackfit/acidfit were all discrete tap-widgets (knobs, pads, steps).
// otafamily is different: four CONTINUOUS drag ribbons. This proves two things:
//   1. lay.h handles a drag surface fine — a ribbon is just a Box you place with
//      the toolkit, then hit-test + map finger-x → pitch. No new primitive.
//   2. The orientation question is MEASURABLE for this shape: a fretless ribbon's
//      playability = its LENGTH (pitch resolution). We report px-per-semitone and
//      how many semitones a finger covers, per device shape, plus the landscape
//      advantage ratio. Wide wins big → this rack PREFERS lock-landscape, and
//      that's now a number a layout-check oracle can key on (device-adaptive-
//      layout.md §orientation policy), not a hunch.
//
//   Presets auto-cycle. Keys 1-4 lock a device, 0 resumes.

#include "studio.h"
#include <math.h>
#include <stddef.h>   // NULL
#include "lay.h"   // the promoted layout vocabulary

// ── the family ───────────────────────────────────────────────────────────────
enum { M_DAD, M_MOM, M_KID, M_DOG, NM };
static const char *const MNAME[NM] = { "DAD", "MOM", "KID", "DOG" };
static const int         MCOL[NM]  = { CLR_TRUE_BLUE, CLR_PINK, CLR_YELLOW, CLR_BROWN };
#define SEMIS 24        // a ribbon spans two octaves
#define PLAYABLE_SEMI_PER_FINGER 2.0f   // a finger covering ≤2 semitones = precise enough

// devices (logical points; a 44pt finger ≈ 9mm on all)
typedef struct { const char *name; float wpt, hpt; } Device;
static Device DEVS[4] = {
    { "iPhone \x18", 390, 844 }, { "iPhone \x1a", 844, 390 },
    { "iPad \x18",   834, 1194 }, { "iPad \x1a",  1194, 834 },
};
#define FINGER_PT 44.0f
#define TABLET_PT 700.0f

static int tick = 0, locked = -1;

void update(void) {
    tick++;
    for (int k = 0; k < 4; k++) if (keyp('1' + k)) locked = k;
    if (keyp('0')) locked = -1;
}

// a little otamatone face in `f`
static void face(Box f, int i) {
    boxfill(f, MCOL[i]); boxrect(f, CLR_DARKER_BLUE);
    if (f.w < 10) return;
    float ex = f.x + f.w * 0.3f, ey = f.y + f.h * 0.36f, er = lay_clamp(f.w * 0.06f, 1, 3);
    circfill((int)ex, (int)ey, (int)er, CLR_BROWNISH_BLACK);
    circfill((int)(f.x + f.w * 0.7f), (int)ey, (int)er, CLR_BROWNISH_BLACK);
    line((int)(f.x + f.w * 0.3f), (int)(f.y + f.h * 0.66f), (int)(f.x + f.w * 0.7f), (int)(f.y + f.h * 0.66f), CLR_BROWNISH_BLACK);
}

// draw one ribbon row; returns the ribbon (drag surface) WIDTH in px
static float ribbon_row(Box row, int i, float fu, float gap) {
    Box rec  = lay_split(row, EDGE_RIGHT, lay_clamp(fu * 0.8f, 7, 34), &row);
    Box fc   = lay_split(row, EDGE_LEFT,  lay_clamp(fu * 1.0f, 9, 44), &row);
    Box rib  = lay_pad(row, 1, 2, 1, 2);
    face(fc, i);

    // the ribbon: a drag surface — fill + semitone ticks (octave lines brighter)
    boxfill(rib, CLR_DARKER_PURPLE); boxrect(rib, MCOL[i]);
    for (int s = 0; s <= SEMIS; s++) {
        int x = (int)(rib.x + s * rib.w / SEMIS);
        line(x, (int)rib.y, x, (int)(rib.y + rib.h), s % 12 == 0 ? MCOL[i] : CLR_DARK_GREY);
    }
    // the playhead (where a finger "is" — animated so it reads as alive)
    float pos = 0.5f + 0.42f * sinf(tick * 0.03f + i * 1.7f);
    circfill((int)(rib.x + pos * rib.w), (int)(rib.y + rib.h / 2), (int)lay_clamp(rib.h * 0.3f, 2, 10), CLR_WHITE);

    // REC toggle (decorative)
    boxfill(rec, CLR_DARK_GREY); boxrect(rec, CLR_MEDIUM_GREY);
    if (rec.w >= 9) { font(FONT_TINY); print_centered("R", (int)(rec.x + rec.w / 2), (int)(rec.y + (rec.h - 5) / 2), CLR_RED); }
    return rib.w;
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    int preset = locked >= 0 ? locked : (tick / 150) % 4;
    Device d = DEVS[preset];

    font(FONT_SMALL);
    print("OTAFIT - ribbon family: does the ribbon PLAY? (device-adaptive-layout.md orientation case)", 4, 3, CLR_LIGHT_GREY);

    // fit the device into the canvas, preserving aspect
    float availX = 6, availY = 26, availW = SCREEN_W - 12, availH = SCREEN_H - 34;
    float scale = availW / d.wpt; if (d.hpt * scale > availH) scale = availH / d.hpt;
    Box dev = box(availX + (availW - d.wpt * scale) / 2, availY + (availH - d.hpt * scale) / 2, d.wpt * scale, d.hpt * scale);
    float fu = FINGER_PT * scale;
    bool tablet = (d.wpt < d.hpt ? d.wpt : d.hpt) >= TABLET_PT;
    bool portrait = d.hpt > d.wpt;

    boxfill(lay_inset(dev, -3), CLR_DARK_GREY);
    boxfill(dev, CLR_DARKER_BLUE);
    float bez = lay_fluid(0.02f, dev.w, 2, 6);
    Box screen = portrait ? lay_pad(dev, bez + fu * 0.35f, bez, bez + fu * 0.2f, bez)
                          : lay_pad(dev, bez, bez + fu * 0.4f, bez, bez + fu * 0.15f);

    float gap = lay_clamp(fu * 0.14f, 1, 5);
    // dock chrome: title (top), gesture loop bar (top), 2 step rows (bottom)
    float titleH = lay_clamp(fu * 0.5f, 8, 22);
    float loopH  = lay_clamp(fu * 0.7f, 8, 30);
    float seqH   = lay_clamp(fu * 0.7f, 8, 30);
    Box afterTitle, afterLoop, afterSeq;
    Box title = lay_split(screen, EDGE_TOP, titleH, &afterTitle);
    Box loop  = lay_split(afterTitle, EDGE_TOP, loopH, &afterLoop);
    Box seq   = lay_split(afterLoop, EDGE_BOTTOM, seqH * 2 + gap, &afterSeq);
    Box body  = lay_pad(afterSeq, gap, 0, gap, 0);

    // title + back-to-root corner keep-out
    float homeSz = lay_clamp(fu * 0.75f, 8, 34);
    Box home = lay_at(title, L_TR, homeSz, homeSz, 1);
    boxfill(title, CLR_TRUE_BLUE); font(FONT_SMALL);
    print("OTA FAMILY", (int)title.x + 3, (int)(title.y + (title.h - 6) / 2), CLR_LIGHT_PEACH);
    boxfill(home, CLR_DARKER_BLUE); boxrect(home, CLR_LIGHT_GREY);
    print_centered("<", (int)(home.x + home.w / 2), (int)(home.y + (home.h - 6) / 2), CLR_LIGHT_PEACH);

    // loop bar: a gesture track per member (decorative dots)
    boxfill(loop, CLR_DARKER_PURPLE);
    for (int i = 0; i < NM; i++) {
        Box tr = lay_cell(lay_inset(loop, 1), 1, NM, i, 0);
        for (int d2 = 0; d2 < 6; d2++) {
            float t = (d2 + 1) / 7.0f;
            circfill((int)(tr.x + t * tr.w), (int)(tr.y + tr.h / 2), 1, MCOL[i]);
        }
    }

    // the four ribbons — stacked full-width rows (each wants LENGTH)
    float ribLen = 0;
    for (int i = 0; i < NM; i++)
        ribLen = ribbon_row(lay_cell(body, 1, NM, i, gap), i, fu, gap);

    // two step rows (BOOM / TICK), pinned bottom
    const char *sr[2] = { "BOOM", "TICK" };
    for (int r = 0; r < 2; r++) {
        Box row = lay_split(lay_pad(seq, r == 0 ? 0 : gap, 0, 0, 0), EDGE_TOP, seqH, NULL);
        if (r == 1) row = box(seq.x, seq.y + seqH + gap, seq.w, seqH);
        Box lab = lay_split(row, EDGE_LEFT, lay_clamp(fu * 0.9f, 10, 40), &row);
        boxfill(lab, CLR_DARK_GREY); font(FONT_TINY);
        print_centered(sr[r], (int)(lab.x + lab.w / 2), (int)(lab.y + (lab.h - 5) / 2), CLR_LIGHT_PEACH);
        for (int s = 0; s < 16; s++) {
            Box c = lay_inset(lay_grid(row, 16, 16, s, gap * 0.4f), 0.5f);
            if (c.w < 2) continue;
            int on = (r == 0) ? (s % 4 == 0) : (s % 4 == 2);
            boxfill(c, on ? CLR_LIME_GREEN : CLR_DARK_GREY);
        }
    }

    // ── the MEASUREMENT: is the ribbon long enough to PLAY? ──────────────────
    float pxPerSemi = ribLen / SEMIS;
    float fingerSemi = pxPerSemi > 0 ? fu / pxPerSemi : 99;   // semitones a finger covers
    bool playable = fingerSemi <= PLAYABLE_SEMI_PER_FINGER;
    float landscapeGain = (d.wpt > d.hpt ? d.wpt / d.hpt : d.hpt / d.wpt);  // × longer when wide

    font(FONT_TINY);
    print(str("%s  %s  ribbon=%dpx  finger covers %.1f semis  %s   landscape = %.1fx longer -> PREFERS LANDSCAPE",
              d.name, tablet ? "TABLET" : "PHONE", (int)ribLen, fingerSemi,
              playable ? "PLAYABLE" : "CRAMPED", landscapeGain),
          4, SCREEN_H - 6, playable ? CLR_GREEN : CLR_ORANGE);
    print_right(locked >= 0 ? "[1-4 lock  0 auto]" : "[auto-cycle  1-4 lock]", SCREEN_W - 4, 3, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
