/* de:meta
{
  "slug": "22-filter",
  "title": "filter",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "A resonant filter sculpts an instrument's tone — instrument_filter(slot, mode, cutoff_hz, resonance). The same buzzy saw through all four modes: FILTER_LOW (warm), FILTER_HIGH (thin), FILTER_BAND (vowel), FILTER_NOTCH (hollow). resonance 0-15 adds a whistly peak at the cutoff, and an LFO on LFO_CUTOFF sweeps the cutoff for a wah. Each panel draws its frequency-response shape with the cutoff line sweeping live, and holds a long note so you hear the filter move. 1-4 select, Z plays the selected one."
}
de:meta */
#include "studio.h"
#include <math.h>

// A filter sculpts an instrument's tone. Four flavours, all on the same buzzy saw:
//   FILTER_LOW  warm    FILTER_HIGH  thin
//   FILTER_BAND vowel   FILTER_NOTCH hollow
// resonance (0..15) adds a whistly peak at the cutoff; an LFO on LFO_CUTOFF sweeps it (wah).
// Press S to change the SWEEP SHAPE (sine / square / S&H / random) — the same filter, moved
// by a different LFO waveform: smooth wah, hard two-step, random jumps, organic drift.

#define LP 5
#define HP 6
#define BP 7
#define NF 8

typedef struct { const char *name; int slot; int mode; float rate; float depth; int base; int col; } Flt;
Flt flts[4];
int cur = 0;

static const int   SHP[4]      = { LFO_SHAPE_SINE, LFO_SHAPE_SQUARE, LFO_SHAPE_SH, LFO_SHAPE_RANDOM };
static const char *SHP_NAME[4] = { "SINE", "SQUARE", "S&H", "RANDOM" };
static int shape_i = 0;

// cart-side mirror of the engine LFO, per filter, so the drawn cutoff matches what you hear
static float vphase[4], vheld[4], vrw[4], vtgt[4];
static unsigned int vseed = 0x55u;
static float nrnd(void) { vseed = vseed*1103515245u + 12345u; return (float)((vseed>>16)&0xFFFF)/32767.5f - 1.0f; }

static void apply_shape(void) { for (int i = 0; i < 4; i++) lfo_shape(flts[i].slot, 0, SHP[shape_i]); }

void init() {
    for (int i = 0; i < 4; i++) instrument(LP + i, INSTR_SAW, 10, 0, 7, 250);

    instrument_filter(LP, FILTER_LOW,   900, 11);
    instrument_filter(HP, FILTER_HIGH, 1100, 11);
    instrument_filter(BP, FILTER_BAND, 1000, 13);
    instrument_filter(NF, FILTER_NOTCH, 900,  8);

    // sweep each cutoff up and down so you HEAR the filter move (wah)
    instrument_lfo(LP, 0, LFO_CUTOFF, 0.5f, 750);
    instrument_lfo(HP, 0, LFO_CUTOFF, 0.5f, 800);
    instrument_lfo(BP, 0, LFO_CUTOFF, 0.4f, 800);
    instrument_lfo(NF, 0, LFO_CUTOFF, 0.4f, 700);

    flts[0] = (Flt){ "LOWPASS",  LP, FILTER_LOW,   0.5f, 750, 900,  CLR_YELLOW };
    flts[1] = (Flt){ "HIGHPASS", HP, FILTER_HIGH,  0.5f, 800, 1100, CLR_GREEN  };
    flts[2] = (Flt){ "BANDPASS", BP, FILTER_BAND,  0.4f, 800, 1000, CLR_BLUE   };
    flts[3] = (Flt){ "NOTCH",    NF, FILTER_NOTCH, 0.4f, 700, 900,  CLR_PINK   };
    apply_shape();   // start on SINE (default), but now switchable
    bpm(80);
}

// -1..1 sweep value mirroring the engine's chosen shape (for the viz)
static float vwave(int i) {
    switch (SHP[shape_i]) {
        case LFO_SHAPE_SQUARE: return vphase[i] < 0.5f ? 1.0f : -1.0f;
        case LFO_SHAPE_SH:     return vheld[i];
        case LFO_SHAPE_RANDOM: return vrw[i];
        default:               return sinf(vphase[i] * 6.2831853f);
    }
}

void update() {
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) cur = i;
    if (keyp('S')) { shape_i = (shape_i + 1) & 3; apply_shape(); }

    // advance the per-filter viz LFO mirror
    for (int i = 0; i < 4; i++) {
        vphase[i] += flts[i].rate * dt;
        if (vphase[i] >= 1.0f) { vphase[i] -= 1.0f; vheld[i] = nrnd(); vtgt[i] = nrnd(); }
        vrw[i] += (vtgt[i] - vrw[i]) * (4.0f * flts[i].rate * dt);
    }

    if (every(2)) {
        cur = (beat() / 2) % 4;
        hit(degree(SCALE_PENTA, 3, 2), flts[cur].slot, 6, 1500);
    }
    if (btnp(0, BTN_A)) hit(degree(SCALE_PENTA, 3, 2), flts[cur].slot, 6, 1500);
}

// a stylised frequency response with the current cutoff marked
void draw_resp(int mode, int x, int y, int w, int h, float cf, int col) {
    int top = y, bot = y + h;
    int cx = x + (int)(w * cf);
    if (cx < x + 4)     cx = x + 4;
    if (cx > x + w - 4) cx = x + w - 4;
    if (mode == FILTER_LOW) {
        line(x, top, cx, top, col);
        line(cx, top, x + w, bot, col);
    } else if (mode == FILTER_HIGH) {
        line(x, bot, cx, bot, col);
        line(cx, bot, x + w, top, col);
    } else if (mode == FILTER_BAND) {
        line(x, bot, cx, top, col);
        line(cx, top, x + w, bot, col);
    } else { // notch
        line(x, top, cx - 6, top, col);
        line(cx - 6, top, cx, bot, col);
        line(cx, bot, cx + 6, top, col);
        line(cx + 6, top, x + w, top, col);
    }
    line(cx, top - 3, cx, bot + 3, CLR_DARK_GREY);   // the sweeping cutoff line
}

void draw() {
    cls(CLR_DARK_BLUE);
    print("FILTER: sculpt the tone", 8, 6, CLR_WHITE);
    print(str("1-4 select  Z plays  S shape: %s", SHP_NAME[shape_i]), 8, 18, CLR_LIGHT_GREY);

    for (int i = 0; i < 4; i++) {
        Flt f = flts[i];
        int x = 8 + i * 76, y = 36, w = 68, h = 132;
        rect(x, y, w, h, i == cur ? CLR_WHITE : CLR_DARK_GREY);
        print(f.name, x + 5, y + 6, f.col);

        // cutoff right now (mirrors the engine LFO shape), mapped 0..2000 Hz across the box
        float cutoff = f.base + vwave(i) * f.depth;
        float frac = cutoff / 2000.0f;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        draw_resp(f.mode, x + 6, y + 24, w - 12, 68, frac, f.col);

        print(str("%dhz", (int)cutoff), x + 5, y + h - 14, CLR_LIGHT_GREY);
    }
    print("freq ->  (low ... high)", 8, 188, CLR_DARK_GREY);
}
