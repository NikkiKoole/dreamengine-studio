/* de:meta
{
  "slug": "lfoshapes",
  "title": "lfoshapes",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "algorithm-visualization",
    "sonification"
  ],
  "lineage": "Standalone showcase for the engine's LFO_SHAPE_* vocabulary (STATUS #39); novel in pairing a live trace + phase dot with audible modulation so the learner sees and hears each waveform simultaneously.",
  "description": "The showcase for the unified LFO_SHAPE_* vocabulary - the modulator waveform, shared across the whole engine (voice LFOs via lfo_shape()/note_lfo_shape(), tremolo/autopan, and fx_lfo). One held saw note runs an LFO; press 1..8 to switch its WAVEFORM live - sine / square / tri / saw / ramp / optical (the Univibe bulb throb) / S&H (sample & hold) / random (filtered walk) - and watch the trace redraw while the yellow dot tracks the value right now. D flips the destination between PITCH (vibrato) and CUTOFF (a filter wobble); -/= set the rate. The expressive payoffs: S&H on PITCH is a random-step arpeggio, SQUARE on CUTOFF is a stepped two-state filter. The stateful shapes are deterministically seeded (--det reproducible). H help. (STATUS #39.)"
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// ── LFO SHAPES ──────────────────────────────────────────────────────────────
// The showcase for the unified LFO_SHAPE_* vocabulary (STATUS #39). One held saw
// note runs an LFO; pick its WAVEFORM and hear/see it move. The same eight shapes
// drive voice LFOs (lfo_shape/note_lfo_shape), tremolo/autopan, and fx_lfo.
//
//   1..8   pick the shape  (sine/square/tri/saw/ramp/optical/S&H/random)
//   D      destination: PITCH (vibrato) <-> CUTOFF (filter wobble)
//   - / =  LFO rate
//   the trace draws the live waveform; the dot is the value right now
//
// S&H on PITCH is the random-step arp; SQUARE on CUTOFF is the stepped filter.

#define SL 8

static const char *SHAPE_NAME[8] = { "SINE","SQUARE","TRI","SAW","RAMP","OPTICAL","S&H","RANDOM" };
static int   shape = LFO_SHAPE_SINE;
static int   dest  = LFO_PITCH;        // LFO_PITCH or LFO_CUTOFF
static float rate  = 5.0f;
static int   held  = -1;

// cart-side mirror of the engine's lfo_wave (stateless) for the on-screen trace, -1..1
static float wave_disp(int sh, float ph) {
    switch (sh) {
        case LFO_SHAPE_SQUARE:  return ph < 0.5f ? 1.0f : -1.0f;
        case LFO_SHAPE_TRI:     return ph < 0.5f ? ph*4.0f - 1.0f : 3.0f - ph*4.0f;
        case LFO_SHAPE_SAW:     return 1.0f - 2.0f*ph;
        case LFO_SHAPE_RAMP:    return 2.0f*ph - 1.0f;
        case LFO_SHAPE_OPTICAL: return (ph < 0.8f ? powf(ph/0.8f,0.6f) : 1.0f-(ph-0.8f)/0.2f)*2.0f - 1.0f;
        default:                return sinf(ph * 6.2831853f);
    }
}

// a cart-side S&H / random walk just for the visual (the engine runs its own, deterministically)
static float disp_phase = 0.0f, sh_val = 0.0f, rw_val = 0.0f, rw_tgt = 0.0f;
static unsigned int dseed = 0x1234u;
static float next_rand(void) { dseed = dseed*1103515245u + 12345u; return (float)((dseed>>16)&0xFFFF)/32767.5f - 1.0f; }

static void retrig(void) {
    if (held >= 0) note_off(held);
    held = note_on(45, SL, 5);          // a low-ish saw so vibrato + filter wobble are clearly audible
    note_lfo(held, 0, dest, rate, dest == LFO_PITCH ? 4.0f : 1800.0f);  // depth: semis for pitch, Hz for cutoff
    note_lfo_shape(held, 0, shape);
}

void update(void) {
    float dt = 1.0f/60.0f;
    for (int k = 0; k < 8; k++) if (keyp('1'+k)) { shape = k; note_lfo_shape(held, 0, shape); }
    if (keyp('D')) { dest = (dest == LFO_PITCH) ? LFO_CUTOFF : LFO_PITCH; retrig(); }
    if (keyp('-') && rate > 0.4f)  { rate -= 0.5f; note_lfo(held, 0, dest, rate, dest==LFO_PITCH?4.0f:1800.0f); }
    if (keyp('=') && rate < 16.0f) { rate += 0.5f; note_lfo(held, 0, dest, rate, dest==LFO_PITCH?4.0f:1800.0f); }

    // advance the display LFO (mirrors the engine's stepping for S&H/random visuals)
    float prev = disp_phase;
    disp_phase += rate * dt;
    if (disp_phase >= 1.0f) { disp_phase -= 1.0f; sh_val = next_rand(); rw_tgt = next_rand(); }
    rw_val += (rw_tgt - rw_val) * (4.0f * rate * dt);
    (void)prev;
}

static float disp_value(void) {
    if (shape == LFO_SHAPE_SH)     return sh_val;
    if (shape == LFO_SHAPE_RANDOM) return rw_val;
    return wave_disp(shape, disp_phase);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("LFO SHAPES", 8, 6, CLR_LIGHT_PEACH);

    // shape menu
    for (int k = 0; k < 8; k++) {
        int col = k % 4, row = k / 4;
        int bx = 8 + col * 76, by = 20 + row * 14;
        int on = (k == shape);
        rectfill(bx, by, 72, 12, on ? CLR_INDIGO : CLR_DARK_BLUE);
        char b[16]; snprintf(b, sizeof b, "%d %s", k+1, SHAPE_NAME[k]);
        print(b, bx + 3, by + 2, on ? CLR_WHITE : CLR_MEDIUM_GREY);
    }

    // the waveform trace panel
    int px = 8, py = 56, pw = SCREEN_W - 16, ph = SCREEN_H - 92;
    rect(px, py, pw, ph, CLR_DARK_BLUE);
    line(px, py + ph/2, px + pw, py + ph/2, CLR_DARK_GREY);   // zero line
    int prevy = 0; int havep = 0;
    // for S&H/random, draw a representative preview from a fixed seed (8 steps across the panel)
    unsigned int vseed = 0xBEEFu; float steps[9];
    for (int i = 0; i < 9; i++) { vseed = vseed*1103515245u + 12345u; steps[i] = (float)((vseed>>16)&0xFFFF)/32767.5f - 1.0f; }
    for (int x = 0; x < pw - 2; x++) {
        float ph01 = (float)x / (float)(pw - 2);
        float v;
        if (shape == LFO_SHAPE_SH)          v = steps[(int)(ph01 * 8.0f)];                 // held per step
        else if (shape == LFO_SHAPE_RANDOM) { int i = (int)(ph01*8.0f); float f = ph01*8.0f - i; v = steps[i]*(1-f) + steps[i+1]*f; }  // smoothed
        else                                v = wave_disp(shape, ph01);
        int y = py + ph/2 - (int)(v * (ph/2 - 3));
        if (havep) line(px + 1 + x - 1, prevy, px + 1 + x, y, CLR_GREEN);
        prevy = y; havep = 1;
    }
    // the live value dot (uses the real stepping for S&H/random)
    float lv = disp_value();
    int dotx = px + 1 + (int)(disp_phase * (pw - 3));
    int doty = py + ph/2 - (int)(lv * (ph/2 - 3));
    circfill(dotx, doty, 2, CLR_YELLOW);

    char hud[48];
    snprintf(hud, sizeof hud, "dest: %s   rate: %.1f Hz", dest == LFO_PITCH ? "PITCH (vibrato)" : "CUTOFF (filter)", rate);
    print(hud, 8, SCREEN_H - 30, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("1-8 shape   D dest   -/= rate", 8, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

void init(void) {
    instrument(SL, INSTR_SAW, 200, 0, 8, 1200);
    instrument_filter(SL, FILTER_LOW, 2200, 60);   // a filter so LFO_CUTOFF has something to sweep
    retrig();
}
