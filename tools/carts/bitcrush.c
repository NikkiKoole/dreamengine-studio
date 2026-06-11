// bitcrush — lo-fi quantization (bit-depth + sample-rate reduction), master vs per-instrument.
// A clean PLUCK melody plays over a SAW bass. Crush the WHOLE mix, or just the bass, and hear
// the difference: per-instrument crush grits the bass while the lead stays crystal.
//   ↑ ↓ : bit depth      ← → : sample rate      A : cycle  MASTER -> BASS ONLY -> OFF
#include "studio.h"
#include <stdio.h>
#include <math.h>

#define I_LEAD 0
#define I_BASS 1

static float bits  = 5.0f;
static float rate  = 6.0f;
static int   route = 0;     // 0 = master (whole mix), 1 = per-instrument (bass only), 2 = off
static int   dirty = 1;

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
    if (btn(0, BTN_UP))    { bits += 0.05f; if (bits > 16.0f) bits = 16.0f; dirty = 1; }
    if (btn(0, BTN_DOWN))  { bits -= 0.05f; if (bits < 1.0f)  bits = 1.0f;  dirty = 1; }
    if (btn(0, BTN_RIGHT)) { rate += 0.10f; if (rate > 32.0f) rate = 32.0f; dirty = 1; }
    if (btn(0, BTN_LEFT))  { rate -= 0.10f; if (rate < 1.0f)  rate = 1.0f;  dirty = 1; }
    if (btnp(0, BTN_A))    { route = (route + 1) % 3; dirty = 1; }
    if (tapp(0, 60, SCREEN_W, 70)) { route = (route + 1) % 3; dirty = 1; }
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

    // quantization preview: a sine quantized to `bits` levels, sample-held every `rate`
    int gx = 30, gw = SCREEN_W - 60, gy = 120, gh = 40;
    rect(gx - 2, gy - gh / 2 - 2, gw + 4, gh + 4, CLR_DARKER_GREY);
    float levels = powf(2.0f, bits);
    float hold = 0; int cnt = 999;
    for (int px = 0; px < gw; px++) {
        float t = (px / (float)gw) * 12.566f;          // two cycles
        float s = sinf(t);
        if (++cnt >= (int)rate) { cnt = 0; hold = floorf(s * levels) / levels; }
        pset(gx + px, gy - (int)(hold * (gh * 0.5f - 1.0f)), CLR_LIME_GREEN);
    }

    char buf[32];
    snprintf(buf, sizeof buf, "bits %.1f", bits);
    print(buf, 30, 172, CLR_YELLOW);
    snprintf(buf, sizeof buf, "rate %.0fx", rate);
    print(buf, 150, 172, CLR_YELLOW);
    print_centered("up/down bits   left/right rate   A route", SCREEN_W / 2, 190, CLR_MEDIUM_GREY);
}
