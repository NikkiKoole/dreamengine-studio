#include "studio.h"

// MODRACK — a tiny modular synth, built in steps (see docs/design/modular-synth.md).
//
// STEP 1: the generative chain is HARDCODED in C — no cables, no editing yet. Wiring it
// by hand first is the point: you see exactly what each module does before any cable
// hides it. The classic Berlin-school patch:
//
//   CLOCK ──tick──► S&H ◄──samples── LFO
//                    └──value──► QUANT ──pitch──► VOICE
//   LFO ───────────────────────────────────────► VOICE filter cutoff (live, every frame)
//
// A slow LFO wanders; the S&H freezes it on each clock step; QUANT snaps that to a scale
// so it's always in key; VOICE plays it as a held note whose filter the same LFO sweeps
// live. Endless in-key melody, zero composition. Later steps draw real module strips and
// make the cables editable.

#define SLOT 5

const char *NOTES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

int   tempo    = 112;
int   scale    = SCALE_PENTA;   // major pentatonic — always pretty
int   root_oct = 4;
float lfo_phase = 0, lfo_rate = 0.37f;   // Hz — deliberately not clock-locked → it wanders
float lfo_out  = 0;             // LFO output 0..1
float sh       = 0;             // sample & hold: the LFO value frozen at the last tick
int   cur_midi = 60;            // QUANT output (MIDI note)
int   voice    = -1;            // VOICE: held-note handle
int   last_step = -1;           // for clock-edge detection
int   tick_flash = 99;          // frames since the last clock tick (LED)
float cutoff   = 600;

void init(void) {
    instrument(SLOT, INSTR_SAW, 4, 90, 3, 260);     // a plucky saw with some sustain
    instrument_filter(SLOT, FILTER_LOW, 600, 10);   // resonant lowpass for the LFO to sweep
    bpm(tempo);
}

void update(void) {
    bpm(tempo);

    // CLOCK — 8th-note steps (a tick = a rising gate edge)
    int step8 = beat() * 2 + (int)(beat_pos() * 2.0f);
    bool tick = step8 != last_step;
    if (tick) { last_step = step8; tick_flash = 0; } else tick_flash++;

    // LFO — a slow wandering 0..1
    lfo_phase += lfo_rate * dt();
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
    lfo_out = (sin_deg(lfo_phase * 360.0f) + 1.0f) * 0.5f;

    // on each tick: S&H freezes the LFO → QUANT snaps to a scale → VOICE retriggers
    if (tick) {
        sh = lfo_out;                              // sample & hold
        int n = (int)(sh * 7.999f);                // 0..7
        cur_midi = degree(scale, root_oct, n);     // quantize to the scale
        if (voice >= 0) note_off(voice);
        voice = note_on(cur_midi, SLOT, 5);        // (re)trigger the held voice
    }

    // VOICE filter CV — the LFO sweeps the cutoff of the RINGING note, live every frame
    // (this is the held-note move; plain note() couldn't touch a sounding voice)
    cutoff = 300.0f + lfo_out * 2600.0f;
    if (voice >= 0) note_cutoff(voice, (int)cutoff);
}

// a vertical 0..1 meter inside a module box
void meter(int x, int y, int w, int h, float v, int col) {
    int fill = (int)(clamp(v, 0, 1) * h);
    rectfill(x, y + h - fill, w, fill, col);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("MODRACK", 8, 5, CLR_WHITE);
    print("step1: hardcoded chain", 92, 6, CLR_INDIGO);

    int bx[5]      = { 8, 66, 124, 182, 240 };
    const char *nm[5] = { "CLOCK", "LFO", "S&H", "QUANT", "VOICE" };
    int col[5]     = { CLR_ORANGE, CLR_PINK, CLR_YELLOW, CLR_GREEN, CLR_BLUE };
    int w = 50, y = 36, h = 110;

    for (int i = 0; i < 5; i++) {
        int x = bx[i];
        bool lit = (i == 0 && tick_flash < 5) || (i == 4 && tick_flash < 8);
        rectfill(x, y, w, h, CLR_DARKER_PURPLE);
        rect(x, y, w, h, lit ? CLR_WHITE : col[i]);
        print(nm[i], x + 3, y + 3, col[i]);
        if (i < 4) print(">", x + w + 1, y + h / 2 - 3, CLR_MEDIUM_GREY);   // signal-flow arrow

        int ix = x + 6, iy = y + 16, iw = w - 12, ih = h - 40;
        switch (i) {
            case 0:  // CLOCK — pulsing LED + BPM
                circfill(x + w / 2, y + 40, 6, tick_flash < 5 ? CLR_WHITE : CLR_DARK_ORANGE);
                print(str("%d", tempo), x + 14, y + h - 16, CLR_LIGHT_GREY);
                print("bpm", x + 12, y + h - 8, CLR_DARK_GREY);
                break;
            case 1:  // LFO — live wander
                meter(ix, iy, iw, ih, lfo_out, CLR_PINK);
                break;
            case 2:  // S&H — frozen between ticks
                meter(ix, iy, iw, ih, sh, CLR_YELLOW);
                break;
            case 3:  // QUANT — note name
                print(NOTES[cur_midi % 12], x + 18, y + 44, CLR_WHITE);
                print(str("midi %d", cur_midi), x + 6, y + h - 10, CLR_MEDIUM_GREY);
                break;
            case 4:  // VOICE — filter cutoff the LFO is sweeping
                meter(ix, iy, iw, ih, (cutoff - 300) / 2600.0f, CLR_BLUE);
                if (tick_flash < 8) circ(x + w / 2, y + 40, 8 - tick_flash, CLR_LIGHT_PEACH);
                break;
        }
    }

    print("clock -> lfo -> sample&hold -> quantize -> voice", 8, 162, CLR_DARK_GREY);
    print("hardcoded patch. next: draw the strips, then make cables editable.", 8, 176, CLR_DARKER_GREY);
    print(str("cutoff %d hz   note %s%d", (int)cutoff, NOTES[cur_midi % 12], cur_midi / 12 - 1), 8, 188, CLR_BLUE_GREEN);
}
