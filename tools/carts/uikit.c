/* de:meta
{
  "slug": "uikit",
  "title": "ui kit",
  "status": "active",
  "created": "2026-06-07",
  "kind": [
    "probe",
    "tech-demo"
  ],
  "teaches": [],
  "description": "The ui.h showcase + device probe: button, slider, knob driven by mouse, touch, AND keyboard at once. Knobs play a sound (BLIP fires it, LOOP repeats on the beat), sliders drive the ball pit, so every widget audibly/visibly does something. Every finger is its own pointer - ride PIT and CUT with two fingers at the same time (the per-contact capture table at work). Mouse hover highlights, wheel fine-tunes a hovered knob. Press FOCUS (or F) for pointer-less play: arrows move the marching ring, LEFT/RIGHT adjust the focused value, Z presses a focused button.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// UI KIT — the ui.h showcase + device probe. Every v1 widget live on one
// screen, all three input modes at once:
//   POINTER  drag knobs (vertical, grab anywhere) and sliders (absolute);
//            every finger is its own pointer — ride PIT and CUT with two
//            fingers at the same time (the capture-table proof).
//   MOUSE    hover highlights; wheel fine-tunes a hovered knob.
//   KEYS     hit FOCUS (or F): arrows move the marching ring, LEFT/RIGHT
//            adjust the focused value (hold to accelerate), Z presses a
//            focused button. Off by default — that's ui.h's opt-in rule.
// The knobs play a sound (BLIP fires it, LOOP repeats it on the beat) and
// the sliders drive the ball pit below, so every widget visibly/audibly
// does something real.

#define SLOT 5

static float pit = 0.5f, cut = 0.6f, vol = 0.8f;     // knob values 0..1
static float spd = 0.4f, siz = 0.4f, cnt = 0.5f;     // slider values 0..1

static int  looping = 0;
static char evt[32] = "";                            // last grab/release event
static int  evt_t = 0;

#define NBALL 24
static float bx[NBALL], by[NBALL], bvx[NBALL], bvy[NBALL];

// pit area
#define PX 8
#define PY 108
#define PW 304
#define PH 84

static int midi_of(void)  { return 45 + (int)(pit * 30); }       // A2..D5
static int cut_of(void)   { return (int)(200.0f * powf(2.0f, cut * 4.5f)); } // 200..4500 Hz
static int vol_of(void)   { int v = (int)(vol * 7.99f); return v ? v : 1; }

static void apply_filter(void) { instrument_filter(SLOT, FILTER_LOW, cut_of(), 6); }
static void blip(void)         { hit(midi_of(), SLOT, vol_of(), 140); }

static void say(const char *what, const char *who) {
    int i = 0;
    while (what[i]) { evt[i] = what[i]; i++; }
    evt[i++] = ' ';
    int j = 0;
    while (who[j] && i < 30) evt[i++] = who[j++];
    evt[i] = 0;
    evt_t = 90;
}

void init(void) {
    instrument(SLOT, INSTR_SQUARE, 2, 110, 0, 60);
    apply_filter();
    bpm(120);
    for (int i = 0; i < NBALL; i++) {
        bx[i] = rnd_float_between(PX + 6, PX + PW - 6);
        by[i] = rnd_float_between(PY + 6, PY + PH - 6);
        bvx[i] = rnd_float_between(-1, 1);
        bvy[i] = rnd_float_between(-1, 1);
    }
}

void update(void) {
    if (looping && every(1)) blip();
    if (evt_t > 0) evt_t--;

    int n = 1 + (int)(cnt * (NBALL - 1));
    float v = 0.3f + spd * 2.5f;
    for (int i = 0; i < n; i++) {
        bx[i] += bvx[i] * v;
        by[i] += bvy[i] * v;
        float r = 1 + siz * 6;
        if (bx[i] < PX + r)      { bx[i] = PX + r;      bvx[i] = -bvx[i]; }
        if (bx[i] > PX + PW - r) { bx[i] = PX + PW - r; bvx[i] = -bvx[i]; }
        if (by[i] < PY + r)      { by[i] = PY + r;      bvy[i] = -bvy[i]; }
        if (by[i] > PY + PH - r) { by[i] = PY + PH - r; bvy[i] = -bvy[i]; }
    }

#ifdef DE_TRACE
    watch("pit", "%.2f", pit); watch("cut", "%.2f", cut); watch("vol", "%.2f", vol);
    watch("spd", "%.2f", spd); watch("caps", "%d", ui_cap_n);
    watch("loop", "%d", looping);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();

    print("UI KIT", 8, 4, CLR_WHITE);
    font(FONT_SMALL);
    print_right("ui.h probe: mouse + touch + keys", 312, 6, CLR_MAUVE);

    // ── knobs: the sound (two fingers = two knobs, try PIT + CUT live) ──
    if (ui_knob(&pit, 36, 52, "pit")) { /* next blip picks it up */ }
    if (ui_knob(&cut, 84, 52, "cut")) apply_filter();
    ui_knob(&vol, 132, 52, "vol");

    // ── buttons ──
    if (ui_button(8, 80, 44, 14, "BLIP")) blip();
    if (ui_button(60, 80, 44, 14, looping ? "LOOP*" : "LOOP")) looping = !looping;
    if (ui_button(112, 80, 52, 14, ui_focus_on ? "FOCUS*" : "FOCUS") || keyp('F'))
        ui_focus(!ui_focus_on);

    // ── sliders: the ball pit ──
    ui_slider(&spd, 184, 38, 128, "speed");
    ui_slider(&siz, 184, 52, 128, "size");
    ui_slider(&cnt, 184, 66, 128, "count");

    // grab/release events surfaced (the undo/commit hook points)
    if (ui_grabbed(&pit)) say("grabbed", "pit");
    if (ui_grabbed(&cut)) say("grabbed", "cut");
    if (ui_grabbed(&vol)) say("grabbed", "vol");
    if (ui_released(&pit)) say("released", "pit");
    if (ui_released(&cut)) say("released", "cut");
    if (ui_released(&vol)) say("released", "vol");
    if (ui_released(&spd)) say("released", "speed");
    if (ui_released(&siz)) say("released", "size");
    if (ui_released(&cnt)) say("released", "count");

    // ── ball pit ──
    rect(PX, PY, PW, PH, CLR_DARKER_GREY);
    int n = 1 + (int)(cnt * (NBALL - 1));
    for (int i = 0; i < n; i++)
        circfill((int)bx[i], (int)by[i], (int)(1 + siz * 6),
                 i % 2 ? CLR_BLUE : CLR_MAUVE);

    if (evt_t > 0) print(evt, PX + 4, PY + 4, CLR_LIGHT_PEACH);
    print(ui_focus_on ? "FOCUS: arrows move ring, LEFT/RIGHT adjust, Z press"
                      : "drag anything - two fingers = two knobs. F = key focus",
          8, 195, CLR_DARK_GREY);
    font(FONT_NORMAL);

    ui_end();
}
