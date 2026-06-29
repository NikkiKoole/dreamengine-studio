/* de:meta
{
  "title": "bitcrush",
  "status": "active",
  "created": "2026-06-11",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "Lo-fi quantization - the crunchy 8-bit / retro grit - shown master vs per-instrument. A clean PLUCK melody plays over a SAW bass through the new crush() bus: drop the BIT DEPTH (floor to 2^bits levels - low = gnarlier steppy quantization noise) and the SAMPLE RATE (sample-and-hold every Nth sample - the downsampled aliasing whine). Cycle the route MASTER (crush the whole mix) -> BASS ONLY (instrument_crush on just the bass, so the lead stays crystal while the low end goes lo-fi - the point of making it a per-instrument bus, not master-only) -> OFF, and watch the live quantization-staircase preview redraw as you turn the knobs. UP/DOWN sets bit depth, LEFT/RIGHT the sample rate, A (or tap the banner) cycles the route."
}
de:meta */
// bitcrush — lo-fi quantization (bit-depth + sample-rate reduction), master vs per-instrument.
// A clean PLUCK melody plays over a SAW bass. Crush the WHOLE mix, or just the bass, and hear
// the difference: per-instrument crush grits the bass while the lead stays crystal.
// The scope IS an XY pad: drag anywhere in it — X = sample rate, Y = bit depth — both at once.
//   drag pad : X rate / Y bits      arrows : fine-tune      A / tap banner : route
#include "studio.h"
#include <stdio.h>
#include <math.h>

#define I_LEAD 0
#define I_BASS 1

// the XY pad / scope rect (shared by update + draw so the mapping + crosshair agree)
#define PAD_X 24
#define PAD_Y 96
#define PAD_W (SCREEN_W - 48)
#define PAD_H 70
#define BITS_MIN 1.0f
#define BITS_MAX 16.0f
#define RATE_MIN 1.0f
#define RATE_MAX 32.0f

static float bits  = 5.0f;
static float rate  = 6.0f;
static int   route = 0;     // 0 = master (whole mix), 1 = per-instrument (bass only), 2 = off
static int   dirty = 1;
static int   padding = 0;   // 1 while a pointer is dragging the pad (for crosshair highlight)

static const char *ROUTE_NAME[3] = { "MASTER (whole mix)", "BASS ONLY (per-instr)", "OFF (clean)" };

static void apply(void) {
    crush(bits, rate, route == 0 ? 1.0f : 0.0f);                   // master
    instrument_crush(I_BASS, bits, rate, route == 1 ? 1.0f : 0.0f); // just the bass
}

void init(void) {
    instrument(I_LEAD, INSTR_PLUCK, 2, 200, 4, 280);
    instrument(I_BASS, INSTR_SAW,   4, 180, 6, 220);
    instrument_filter(I_BASS, FILTER_LOW, 900, 4);
    bpm(120);
    apply();
}

void update(void) {
    // route: tap the banner (its own rect) or press A
    if (btnp(0, BTN_A))                  { route = (route + 1) % 3; dirty = 1; }
    if (tapp(40, 60, SCREEN_W - 80, 28)) { route = (route + 1) % 3; dirty = 1; }

    // XY pad — drag anywhere in the scope with ONE pointer (mouse or touch): X = rate, Y = bits.
    // top = more bits (cleaner), bottom = 1 bit; left = slow resample, right = fast.
    padding = 0;
    int mx = -1, my = -1;
    if (mouse_down(MOUSE_LEFT)) { mx = mouse_x(); my = mouse_y(); }
    if (touch_count() > 0)      { mx = touch_x(0); my = touch_y(0); }   // touch wins if both
    if (mx >= PAD_X && mx < PAD_X + PAD_W && my >= PAD_Y && my < PAD_Y + PAD_H) {
        rate = RATE_MIN + (mx - PAD_X) / (float)PAD_W * (RATE_MAX - RATE_MIN);
        bits = BITS_MIN + (1.0f - (my - PAD_Y) / (float)PAD_H) * (BITS_MAX - BITS_MIN);
        padding = 1; dirty = 1;
    }

    // arrows still fine-tune
    if (btn(0, BTN_UP))    { bits += 0.05f; dirty = 1; }
    if (btn(0, BTN_DOWN))  { bits -= 0.05f; dirty = 1; }
    if (btn(0, BTN_RIGHT)) { rate += 0.10f; dirty = 1; }
    if (btn(0, BTN_LEFT))  { rate -= 0.10f; dirty = 1; }
    if (bits < BITS_MIN) bits = BITS_MIN; if (bits > BITS_MAX) bits = BITS_MAX;
    if (rate < RATE_MIN) rate = RATE_MIN; if (rate > RATE_MAX) rate = RATE_MAX;

    if (dirty) { apply(); dirty = 0; }

    static const int lead[8] = { 72, 76, 79, 76, 74, 77, 81, 77 };
    static const int bass[8] = { 36, 36, 43, 36, 41, 36, 48, 43 };
    if (every(1)) { note(lead[beat() & 7], I_LEAD, 4); note(bass[beat() & 7], I_BASS, 6); }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print_centered("BITCRUSH", SCREEN_W / 2, 12, CLR_WHITE);
    print_centered("lo-fi quantization", SCREEN_W / 2, 26, CLR_MEDIUM_GREY);

    // route banner (tap to cycle)
    int rc = route == 0 ? CLR_ORANGE : (route == 1 ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    rectfill(40, 60, SCREEN_W - 80, 28, rc);
    rect(40, 60, SCREEN_W - 80, 28, CLR_WHITE);
    print_centered(ROUTE_NAME[route], SCREEN_W / 2, 70, CLR_BLACK);

    // the XY pad doubles as the scope: a sine quantized to `bits` levels, sample-held every
    // `rate` samples, drawn across the pad — and a crosshair marks the live (rate, bits) point.
    rect(PAD_X - 1, PAD_Y - 1, PAD_W + 2, PAD_H + 2, padding ? CLR_WHITE : CLR_DARKER_GREY);
    int midY = PAD_Y + PAD_H / 2;
    float levels = powf(2.0f, bits);
    float hold = 0; int cnt = 999;
    for (int px = 0; px < PAD_W; px++) {
        float t = (px / (float)PAD_W) * 12.566f;        // two cycles
        float s = sinf(t);
        if (++cnt >= (int)rate) { cnt = 0; hold = floorf(s * levels) / levels; }
        pset(PAD_X + px, midY - (int)(hold * (PAD_H * 0.42f)), CLR_LIME_GREEN);
    }
    // crosshair at the current setting: X ← rate, Y ← bits (top = max bits)
    int cx = PAD_X + (int)((rate - RATE_MIN) / (RATE_MAX - RATE_MIN) * PAD_W);
    int cy = PAD_Y + (int)((1.0f - (bits - BITS_MIN) / (BITS_MAX - BITS_MIN)) * PAD_H);
    int cc = padding ? CLR_ORANGE : CLR_MAUVE;
    line(cx, PAD_Y, cx, PAD_Y + PAD_H, cc);
    line(PAD_X, cy, PAD_X + PAD_W, cy, cc);
    rectfill(cx - 2, cy - 2, 5, 5, CLR_ORANGE);
    // axis hints
    print("bits", PAD_X + 2, PAD_Y + 2, CLR_DARKER_GREY);
    print_right("rate", PAD_X + PAD_W - 2, PAD_Y + PAD_H - 9, CLR_DARKER_GREY);

    char buf[32];
    snprintf(buf, sizeof buf, "bits %.1f", bits);
    print(buf, 30, 172, CLR_YELLOW);
    snprintf(buf, sizeof buf, "rate %.0fx", rate);
    print(buf, 150, 172, CLR_YELLOW);
    print_centered("drag the pad: X rate / Y bits", SCREEN_W / 2, 190, CLR_MEDIUM_GREY);
}
