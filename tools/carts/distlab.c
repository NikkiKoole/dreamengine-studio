/* de:meta
{
  "slug": "distlab",
  "title": "distortion lab",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": {
    "summary": "A distortion PLAYGROUND, Rung 0: one drive stage on an auto-riffing acid line, with a live transfer curve AND a real oscilloscope of the post-FX output side by side. The seed of a growable 'destruction unit' (design/distortion-lab.md) — a single [DRIVE] block today, room in the chain for more stages later.",
    "detail": "instrument_drive_mode() picks the waveshaper (SOFT tanh tube / HARD clip fuzz / FOLD sine wavefolder / ASYM even-harmonic tube) and instrument_drive() is the amount. LEFT panel = a representative TRANSFER CURVE (input->output; the identity diagonal is ghosted so you see the bend), redrawn as you turn drive up. RIGHT panel = a real OSCILLOSCOPE via scope_read() — the actual post-FX mix, zero-crossing-frozen so it holds still. The drone runs a resonant saw so the grit has a peak to scream into. Cart-land only, no engine changes (scope_read exposes the output).",
    "controls": "LEFT/RIGHT pick the drive mode (or tap the [DRIVE] block) - UP/DOWN ride the drive amount (or drag the amount bar) - Z pluck the note now"
  }
}
de:meta */
// distortion lab (Rung 0) — one drive stage + a transfer curve + a live scope.
// The seed of a growable destruction unit; see docs/design/distortion-lab.md.
//   ← → : drive mode    ↑ ↓ : drive amount    Z : pluck
#include "studio.h"
#include <stdio.h>
#include <math.h>

#define I_LEAD 0
#define NSCOPE 256

static const char *MODE_NAME[4] = { "SOFT", "HARD", "FOLD", "ASYM" };
static const char *MODE_DESC[4] = { "tanh \x1a warm tube overdrive",
                                    "hard clip \x1a buzzy digital fuzz",
                                    "sine wavefolder \x1a glassy metal",
                                    "asymmetric \x1a fat even harmonics" };

static int   mode = 2;          // start on FOLD — the fun one
static float amt  = 0.55f;
static int   h    = -1;         // the held drone voice (continuous → the scope always has signal)

// the SAME waveshaper family the engine uses, re-implemented for the curve
// display (representative, not the engine's exact gain — the scope is the
// truthful readout). g = pre-gain rising with the drive amount.
static float shape(float x, int m, float a) {
    float g = 1.0f + a * 4.0f, v = x * g;
    switch (m) {
        case 0:  return tanhf(v);
        case 1:  return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
        case 2:  return sinf(v);
        default: return x >= 0.0f ? tanhf(v) : tanhf(v * 0.6f);
    }
}

static void set_mode(int m) { mode = m & 3; instrument_drive_mode(I_LEAD, mode); if (h >= 0) note_drive_mode(h, mode); }
static void set_amt(float a) { amt = a < 0 ? 0 : a > 1 ? 1 : a; instrument_drive(I_LEAD, amt); if (h >= 0) note_drive(h, amt); }

void init(void) {
    instrument(I_LEAD, INSTR_SAW, 4, 220, 6, 300);
    instrument_filter(I_LEAD, FILTER_DIODE, 1200, 11);   // a resonant acid peak to drive INTO
    h = note_on(48, I_LEAD, 6);                          // one held drone…
    note_glide(h, 55);                                   // …that SLIDES between riff notes (303-style)
    set_amt(amt);
    set_mode(mode);
    bpm(128);
}

// ── the chain block + the amount bar, so update() can hit-test them ──
#define STAGE_X 10
#define STAGE_Y 26
#define STAGE_W 96
#define STAGE_H 16
#define BAR_X 60
#define BAR_Y 176
#define BAR_W 190

void update(void) {
    if (btnp(0, BTN_LEFT))  set_mode(mode + 3);
    if (btnp(0, BTN_RIGHT)) set_mode(mode + 1);
    if (btn(0, BTN_UP))   set_amt(amt + 0.015f);
    if (btn(0, BTN_DOWN)) set_amt(amt - 0.015f);
    if (btnp(0, BTN_A) && h >= 0) note_vol(h, 6);        // Z re-swells the drone if you've faded it

    // tap the [DRIVE] block cycles the mode (phone)
    if (tapp(STAGE_X, STAGE_Y, STAGE_W, STAGE_H)) set_mode(mode + 1);
    // drag the amount bar
    if (tap(BAR_X - 4, BAR_Y - 6, BAR_W + 8, 16)) {
        float f = (mouse_x() - BAR_X) / (float)BAR_W;
        set_amt(f);
    }

    // auto-riff: SLIDE the held drone along a low acid line — continuous tone,
    // so the grit is always audible and the scope always has signal.
    static const int riff[8] = { 36, 48, 36, 43, 36, 48, 41, 43 };
    if (every(1) && h >= 0) note_pitch(h, riff[beat() & 7] + 12);
}

static void draw_curve(int bx, int by, int bw, int bh) {
    rectfill(bx, by, bw, bh, CLR_BLACK);
    rect(bx, by, bw, bh, CLR_DARK_PURPLE);
    int mx = bx + bw / 2, my = by + bh / 2;
    line(bx, my, bx + bw, my, CLR_DARKER_GREY);          // axes
    line(mx, by, mx, by + bh, CLR_DARKER_GREY);
    for (int px = 0; px < bw; px++) {                    // identity diagonal (ghost)
        float x = (px / (float)bw) * 2.0f - 1.0f;
        pset(bx + px, my - (int)(x * (bh / 2 - 2)), CLR_DARK_GREY);
    }
    for (int px = 0; px < bw; px++) {                    // the actual transfer curve
        float x = (px / (float)bw) * 2.0f - 1.0f;
        float v = shape(x, mode, amt);
        if (v > 1) v = 1; if (v < -1) v = -1;
        pset(bx + px, my - (int)(v * (bh / 2 - 2)), CLR_ORANGE);
    }
    print("TRANSFER", bx + 3, by + 2, CLR_LIGHT_GREY);
}

static void draw_scope(int bx, int by, int bw, int bh) {
    rectfill(bx, by, bw, bh, CLR_BLACK);
    rect(bx, by, bw, bh, CLR_DARK_PURPLE);
    int my = by + bh / 2;
    line(bx, my, bx + bw, my, CLR_DARKER_GREY);
    static float sc[NSCOPE];
    scope_read(sc, NSCOPE);
    int start = 0;                                       // freeze on a rising zero-crossing
    for (int i = 1; i < NSCOPE / 2; i++)
        if (sc[i - 1] < 0.0f && sc[i] >= 0.0f) { start = i; break; }
    int lim = bh / 2 - 2, prev = my;
    for (int px = 0; px < bw; px++) {
        int si = start + px; if (si >= NSCOPE) break;
        int a = (int)(sc[si] * lim * 2.5f);              // gain the trace so it fills the box
        if (a > lim) a = lim; if (a < -lim) a = -lim;    // …clamped so a hot mix doesn't spill
        int y = my - a;
        if (px) line(bx + px - 1, prev, bx + px, y, CLR_GREEN);
        prev = y;
    }
    print("OUTPUT", bx + 3, by + 2, CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("DISTORTION LAB", 10, 8, CLR_WHITE);
    font(FONT_SMALL);
    print("one stage - room to grow", 150, 11, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the chain row: [ DRIVE:MODE ]  →  + → + (ghosted future stages)
    rectfill(STAGE_X, STAGE_Y, STAGE_W, STAGE_H, CLR_ORANGE);
    rect(STAGE_X, STAGE_Y, STAGE_W, STAGE_H, CLR_WHITE);
    char blk[24]; snprintf(blk, sizeof blk, "DRIVE:%s", MODE_NAME[mode]);
    print(blk, STAGE_X + 8, STAGE_Y + 5, CLR_BLACK);
    print("\x1a", STAGE_X + STAGE_W + 6, STAGE_Y + 5, CLR_DARK_GREY);
    for (int i = 0; i < 3; i++) {
        int gx = STAGE_X + STAGE_W + 16 + i * 30;
        rect(gx, STAGE_Y, 22, STAGE_H, CLR_DARKER_GREY);
        print("+", gx + 9, STAGE_Y + 5, CLR_DARK_GREY);
    }

    draw_curve(10, 48, 140, 118);
    draw_scope(170, 48, 140, 118);

    print(MODE_DESC[mode], 10, 170, CLR_YELLOW);

    // drive amount bar
    print("drive", 10, BAR_Y, CLR_WHITE);
    rect(BAR_X, BAR_Y, BAR_W, 8, CLR_DARK_PURPLE);
    rectfill(BAR_X + 1, BAR_Y + 1, (int)((BAR_W - 2) * amt), 6, CLR_ORANGE);
    char buf[16]; snprintf(buf, sizeof buf, "%d%%", (int)(amt * 100 + 0.5f));
    print(buf, BAR_X + BAR_W + 6, BAR_Y, CLR_WHITE);

    print_centered("\x1b \x1a mode    up/down drive    Z pluck", SCREEN_W / 2, 192, CLR_LIGHT_GREY);
}
