/* de:meta
{
  "slug": "pitchscope",
  "title": "pitch scope",
  "status": "active",
  "created": "2026-07-17",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "audio-input"
  ],
  "lineage": "A diagnostic for the Tier-1 mic pitch surface (docs/design/mic-and-sampling.md): plot RAW mic_pitch() over time on a log-frequency axis so you can SEE how well (or badly) the estimator follows a voice sliding up and down. The acceptance test for any pitch-detector improvement.",
  "description": {
    "summary": "A scrolling graph of the RAW microphone pitch on a musical (log-frequency) axis, with note reference lines. Hum and slide your voice up and down — a good detector draws a smooth diagonal; a bad one jumps around.",
    "detail": "No smoothing, no scale-snap — it shows mic_pitch() exactly as the engine reports it, so you can judge the detector honestly: does it track a glide, lag, drop out, or octave-jump? Note lines (C2..C6, A440) are drawn for reference and the live reading is shown as Hz + nearest note + a dB level meter.",
    "controls": "CLICK or SPACE: enable/disable the mic. Then hum and slide your pitch up and down."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// PITCH SCOPE — a diagnostic for mic_pitch(). Plots the RAW, unsmoothed estimate over time on a
// log-frequency axis (octaves evenly spaced), so a smooth vocal glide should read as a straight
// diagonal line. Reveals the estimator's real behaviour: tracking, lag, dropouts, octave jumps.

#define FMIN  65.0f      // C2, bottom of the plot
#define FMAX  1050.0f    // ~C6, top

static const char *NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

static float t_hz [SCREEN_W];   // ring of raw pitch (Hz; 0 = no pitch this frame)
static float t_lvl[SCREEN_W];   // ring of level
static int   t_pos = 0;
static float last_hz = 0;       // last non-zero reading, for the big readout

static int fy(float hz) {       // Hz -> y (log scale; high pitch = high on screen)
    if (hz < FMIN) hz = FMIN; if (hz > FMAX) hz = FMAX;
    float f = (log2f(hz) - log2f(FMIN)) / (log2f(FMAX) - log2f(FMIN));
    return (int)((1.0f - f) * (SCREEN_H - 26)) + 20;
}

static void note_of(float hz, char *buf, int cap) {
    if (hz < 20.0f) { snprintf(buf, cap, "--"); return; }
    int m = (int)lroundf(69.0f + 12.0f * log2f(hz / 440.0f));
    snprintf(buf, cap, "%s%d", NOTES[((m % 12) + 12) % 12], m / 12 - 1);
}

void update(void) {
    if (keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT)) {
        if (mic_active()) mic_stop(); else mic_start();
    }
    if (!mic_active()) return;
    float hz = mic_pitch(), lvl = mic_level();
    t_hz[t_pos] = hz; t_lvl[t_pos] = lvl; t_pos = (t_pos + 1) % SCREEN_W;
    if (hz > 20.0f) last_hz = hz;
}

void draw(void) {
    cls(CLR_BLACK);

    if (!mic_active()) {
        print("PITCH SCOPE", SCREEN_W/2 - 40, SCREEN_H/2 - 20, CLR_WHITE);
        print("click / space, then hum", SCREEN_W/2 - 68, SCREEN_H/2, CLR_GREEN);
        print("and slide your voice up + down", SCREEN_W/2 - 88, SCREEN_H/2 + 12, CLR_MEDIUM_GREY);
        return;
    }

    // note reference lines: every C across the range, plus A440
    for (int m = 36; m <= 84; m += 12) {                 // C2..C6
        float f = 440.0f * powf(2.0f, (m - 69) / 12.0f);
        int y = fy(f);
        for (int x = 40; x < SCREEN_W; x += 5) pset(x, y, CLR_DARK_BLUE);
        char lbl[10]; snprintf(lbl, sizeof lbl, "C%d %d", m/12 - 1, (int)f);
        print(lbl, 2, y - 3, CLR_MEDIUM_GREY);
    }
    { int y = fy(440.0f); for (int x = 40; x < SCREEN_W; x += 5) pset(x, y, CLR_INDIGO);
      print("A440", 2, y - 3, CLR_INDIGO); }

    // the raw pitch trace — dots, connected only when both samples are voiced
    int prevx = -1, prevy = 0;
    for (int x = 0; x < SCREEN_W; x++) {
        int idx = (t_pos + x) % SCREEN_W;
        float hz = t_hz[idx];
        if (hz < 20.0f) { prevx = -1; continue; }        // dropout — break the line
        int y = fy(hz);
        int col = t_lvl[idx] > 0.06f ? CLR_GREEN : CLR_DARK_GREEN;
        if (prevx >= 0) line(prevx, prevy, x, y, col); else pset(x, y, col);
        prevx = x; prevy = y;
    }

    // live readout bar (top)
    rectfill(0, 0, SCREEN_W, 16, CLR_DARK_BLUE);
    float lvl = mic_level();
    float db = lvl > 1e-6f ? 20.0f * log10f(lvl) : -99.0f;
    char hzs[12]; snprintf(hzs, sizeof hzs, "%d Hz", (int)(last_hz + 0.5f));
    char nm[8]; note_of(last_hz, nm, sizeof nm);
    print(hzs, 4, 5, CLR_WHITE);
    print(nm, 78, 5, CLR_GREEN);
    // little VU bar on the right
    int bw = 90, bx = SCREEN_W - bw - 4;
    int fill = (int)((db + 60.0f) / 60.0f * bw); if (fill < 0) fill = 0; if (fill > bw) fill = bw;
    rect(bx - 1, 3, bw + 2, 10, CLR_MEDIUM_GREY);
    rectfill(bx, 4, fill, 8, fill > bw*4/5 ? CLR_RED : CLR_GREEN);
}
