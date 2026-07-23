/* de:meta
{
  "slug": "walkgrab",
  "title": "Walk-Grab (mockup)",
  "status": "active",
  "created": "2026-07-18",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer"
  ],
  "lineage": "A DRAW-ONLY vibe mockup for the walkbox redesign: instead of a focus/zoom edit mode, each note in the piano-roll carries dedicated little GRABBER handles you pick up in place — right edge = length/ring, a top knob on a stem = bend (the 'ding'), a tip nub = slide to the next note, a bottom corner = scoop. Grabbers appear on the note you pick, so the overview stays uncluttered.",
  "description": {
    "summary": "Static mockup: a walking-bass note as a piano-roll capsule with dedicated GRABBERS you pick up in place — right edge = length, top knob = bend (pull up for the 'ding'), tip nub = slide to next, corner = scoop. Top shows the context roll (grabbers pop up on the picked note); bottom is that note up close, labelled. No interaction yet.",
    "detail": "The answer to 'how do you edit a note without a zoom mode': the note carries its own little handles. DRAG THE BODY = move it (pitch x time). RIGHT EDGE tab = length / how long it rings. TOP KNOB on a stem = bend — pull it up (or down) for the expressive 'ding' / scoop / vibrato, the upright's continuous GRAB+pull; the knob's height IS the bend, and the pitch line follows. TIP NUB = fling to the next note to make a slide/slur. BOTTOM CORNER = scoop into the attack. Only the picked note shows its grabbers, so the 16-step overview stays chunky and readable. This is the interaction model for the walkbox redesign (docs/design/walkbox.md).",
    "controls": "None — draw-only mockup for eyeballing the grabber handles."
  }
}
de:meta */

// Walk-Grab — a draw-only VIBE MOCKUP for the walkbox redesign.
// A note carries dedicated GRABBER handles you pick up in place (no zoom mode):
// right edge = length, top knob = bend, tip = slide, corner = scoop.
// See docs/design/walkbox.md.

#include "studio.h"

// ── a grabber knob: a bright pickable dot with a dark ring ──
static void knob(int x, int y, int col) { circfill(x, y, 3, CLR_BROWNISH_BLACK); circfill(x, y, 2, col); pset(x - 1, y - 1, CLR_WHITE); }
static void tab_grip(int x, int y, int h) { for (int i = 1; i < h - 1; i += 2) { pset(x, y + i, CLR_BROWNISH_BLACK); pset(x + 2, y + i, CLR_BROWNISH_BLACK); } }

// ── the context mini-roll (grabbers pop up on the PICKED note) ──
#define MGX 74
#define MGY 34
#define MCW 18
#define MRH 7
#define MROW 6
static int mx_(int c) { return MGX + c * MCW; }
static int my_(int r) { return MGY + (MROW - 1 - r) * MRH; }

static void mini_cap(int c, int r, int len, int sel) {
    int x = mx_(c) + 1, y = my_(r) + 1, h = MRH - 2;
    for (int i = 0; i < len; i++)
        rectfill(mx_(c + i) + 1, y, MCW - (i == len - 1 ? 2 : 0), h, i == 0 ? CLR_PEACH : i == 1 ? CLR_DARK_PEACH : CLR_BROWN);
    rrect(x, y, len * MCW - 2, h, 1, sel ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK);
    if (sel) {                                   // the picked note shows its little grabbers
        int rx = mx_(c + len) - 2, cy = y + h / 2;
        knob(mx_(c) + (len * MCW) / 2, y - 6, CLR_LIGHT_YELLOW);   // bend knob (top stem)
        line(mx_(c) + (len * MCW) / 2, y, mx_(c) + (len * MCW) / 2, y - 4, CLR_LIGHT_YELLOW);
        rectfill(rx, y, 2, h, CLR_LIGHT_PEACH);                    // length tab (right edge)
        knob(rx + 6, cy, CLR_ORANGE);                             // slide nub (tip)
    }
}

// ── the anatomy note (that same note, up close) ──
#define NX 60
#define NY 124
#define NW 150
#define NH 24

static void leader(int x0, int y0, int x1, int y1) { line(x0, y0, x1, y1, CLR_DARK_PEACH); }

void draw(void) {
    cls(CLR_DARK_BROWN);

    // ── title ──
    rectfill(0, 0, SCREEN_W, 22, CLR_BROWNISH_BLACK);
    print("WALK-GRAB", 6, 3, CLR_LIGHT_PEACH);
    font(FONT_SMALL); print("grabbers on the note", 6, 13, CLR_DARK_PEACH); font(FONT_NORMAL);

    // ── context mini-roll ──
    font(FONT_SMALL); print("pick a note - grabbers pop up:", 74, 25, CLR_DARK_PEACH); font(FONT_NORMAL);
    rectfill(MGX - 2, MGY - 6, 8 * MCW + 4, MROW * MRH + 10, CLR_BROWNISH_BLACK);
    for (int r = 0; r < MROW; r++)
        rectfill(MGX, my_(r), 8 * MCW, MRH, (r & 1) ? CLR_DARK_BROWN : CLR_BROWNISH_BLACK);
    mini_cap(0, 1, 2, 0);
    mini_cap(2, 4, 1, 0);
    mini_cap(4, 3, 3, 1);          // ← the picked note
    mini_cap(7, 2, 1, 0);

    // ── anatomy: the picked note, up close, with every grabber labelled ──
    int cy = NY + NH / 2;
    // body: ring-out fade head -> tail
    for (int i = 0; i < 5; i++)
        rectfill(NX + i * (NW / 5), NY, NW / 5, NH, i == 0 ? CLR_PEACH : i == 1 ? CLR_DARK_PEACH : i == 2 ? CLR_BROWN : CLR_BROWNISH_BLACK);
    rrect(NX, NY, NW, NH, 2, CLR_LIGHT_YELLOW);
    rectfill(NX, NY, 3, NH, CLR_LIGHT_PEACH);       // bright attack edge

    // (1) BEND knob on a stem — pull up = the "ding"; the pitch line follows
    int bkx = NX + NW / 2, bky = NY - 22;
    line(bkx, NY, bkx, bky + 3, CLR_LIGHT_YELLOW);
    knob(bkx, bky, CLR_LIGHT_YELLOW);
    // the resulting pitch line inside the note (rises toward under the knob, then eases)
    { int px = NX + 4, py = cy; for (int x = NX + 5; x < NX + NW - 4; x++) {
        float t = (float)(x - NX) / NW; float off = t < 0.5f ? t * 2.0f : 1.0f - (t - 0.5f); // up then ease
        int y = cy - (int)(off * (NH * 0.7f)); line(px, py, x, y, CLR_ORANGE); px = x; py = y; } }

    // (2) LENGTH tab — right edge
    int rx = NX + NW - 3;
    rectfill(rx, NY, 3, NH, CLR_WHITE); tab_grip(rx, NY, NH);

    // (3) SLIDE nub — tip, flung to the next (ghost) note
    int snx = NX + NW + 10;
    knob(snx, cy, CLR_ORANGE);
    for (int x = snx + 4; x < snx + 30; x += 4) pset(x, cy - (x - snx) / 4, CLR_ORANGE);   // dashed toward…
    rrect(snx + 30, NY - 8, 22, NH, 2, CLR_DARK_PEACH);                                     // …a ghost next note

    // (4) SCOOP nub — bottom-left corner
    knob(NX, NY + NH, CLR_LIGHT_PEACH);

    // ── labels + leaders ──
    font(FONT_SMALL);
    leader(bkx - 6, bky + 1, 54, bky + 2);                         // bend knob → left
    print("knob = bend", 4, bky - 3, CLR_LIGHT_YELLOW);
    print("(pull up = ding)", 4, bky + 4, CLR_LIGHT_YELLOW);
    leader(rx, NY, 200, NY - 6);                                   // length tab → up-right
    print("edge = length/ring", 152, NY - 12, CLR_LIGHT_PEACH);
    leader(snx, cy + 4, snx - 2, cy + 14);                         // slide nub → down
    print("tip = slide to next", 210, cy + 15, CLR_ORANGE);
    leader(NX, NY + NH + 3, NX - 4, NY + NH + 11);                 // scoop corner → down-left
    print("corner = scoop in", 4, NY + NH + 12, CLR_LIGHT_PEACH);
    print("drag body = move (pitch x time)", 96, NY + NH + 24, CLR_DARK_PEACH);
    font(FONT_NORMAL);
}
