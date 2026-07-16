/* de:meta
{
  "slug": "mictest",
  "title": "mic test",
  "status": "active",
  "created": "2026-07-16",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "audio-input"
  ],
  "lineage": "The first cart to HEAR — the Tier-1 proof for the audio-input frontier (docs/design/mic-and-sampling.md). Reads the engine's new mic_level()/mic_pitch() surface (runtime/mic.h) fed by the host's capture device.",
  "description": {
    "summary": "The engine can HEAR you. Enable the mic and it draws your loudness as a VU meter and your pitch as a musical note — the Tier-1 'mic as a controller' surface.",
    "detail": "Proves the microphone-input seam end to end: mic_start() asks the host to open its capture device (popping the OS permission prompt the first time), then mic_level() (RMS loudness) drives a VU bar with peak-hold and mic_pitch() (a crude zero-crossing estimate) reads out as Hz + nearest note. Loudness is solid; pitch is honestly labelled as octave-noisy on a voice.",
    "controls": "CLICK or SPACE to enable/disable the microphone. Then hum, talk, or clap."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// MIC TEST — the first cart that listens. It exercises the Tier-1 microphone input
// surface (mic_start/mic_stop/mic_active/mic_level/mic_pitch, see runtime/mic.h): the
// host owns the capture device + permission prompt and pushes frames into the engine,
// which analyzes them into a loudness (RMS) and a rough pitch. This cart just draws them.
//
// Loudness (the VU bar) is production-solid. Pitch is a CRUDE zero-crossing estimate —
// it reads octave-low and jittery on a real voice; good as a controller axis, not a tuner.

static const char *NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static float peak = 0.0f;     // VU peak-hold, decays

static void note_of(float hz, char *buf, int cap) {
    if (hz < 20.0f || hz > 5000.0f) { snprintf(buf, cap, "--"); return; }
    float midi = 69.0f + 12.0f * log2f(hz / 440.0f);
    int m = (int)lroundf(midi);
    snprintf(buf, cap, "%s%d", NOTES[((m % 12) + 12) % 12], m / 12 - 1);
}

void update(void) {
    // CLICK or SPACE toggles the mic (the permission prompt attaches to this user action)
    if (keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT)) {
        if (mic_active()) mic_stop();
        else              mic_start();
    }
    float lvl = mic_level();
    if (lvl > peak) peak = lvl;
    peak *= 0.96f;                 // slow peak decay
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    if (!mic_active()) {
        print("MIC TEST", SCREEN_W/2 - 28, 30, CLR_WHITE);
        print("click / space", SCREEN_W/2 - 48, SCREEN_H/2 - 12, CLR_YELLOW);
        print("to enable the mic", SCREEN_W/2 - 64, SCREEN_H/2 + 2, CLR_YELLOW);
        print("(allow the prompt)", SCREEN_W/2 - 68, SCREEN_H/2 + 20, CLR_LIGHT_GREY);
        return;
    }

    float lvl = mic_level();
    float db  = lvl > 1e-6f ? 20.0f * log10f(lvl) : -99.0f;   // dBFS

    // ── VU meter ──────────────────────────────────────────────────────────
    int bx = 24, by = 60, bw = SCREEN_W - 48, bh = 22;
    rect(bx - 1, by - 1, bw + 2, bh + 2, CLR_LIGHT_GREY);
    int fill = (int)((db + 60.0f) / 60.0f * bw);            // -60..0 dB → 0..bw
    if (fill < 0) fill = 0; if (fill > bw) fill = bw;
    // colour ramps green → yellow → red as it fills
    int col = fill < bw/2 ? CLR_GREEN : fill < bw*4/5 ? CLR_YELLOW : CLR_RED;
    rectfill(bx, by, fill, bh, col);
    int px = bx + (int)((20.0f * log10f(peak > 1e-6f ? peak : 1e-6f) + 60.0f) / 60.0f * bw);
    if (px > bx && px < bx + bw) line(px, by, px, by + bh, CLR_WHITE);   // peak-hold tick

    print("LEVEL", bx, by - 12, CLR_WHITE);
    char dbs[16]; snprintf(dbs, sizeof dbs, "%.0f dB", db);
    print(dbs, bx + bw - 40, by - 12, CLR_WHITE);

    // ── pitch readout ─────────────────────────────────────────────────────
    float hz = mic_pitch();
    char note[8]; note_of(hz, note, sizeof note);
    print("PITCH", bx, by + 44, CLR_WHITE);
    char hzs[24]; snprintf(hzs, sizeof hzs, "%.0f Hz  %s", hz, note);
    print(hzs, bx, by + 58, hz > 0 ? CLR_ORANGE : CLR_MEDIUM_GREY);

    // a big note letter so a hum reads at a glance
    if (hz > 0) { font(FONT_COMIC); print(note, SCREEN_W/2 - 12, by + 84, CLR_ORANGE); font(FONT_NORMAL); }

    print("hum / talk / clap", bx, SCREEN_H - 24, CLR_LIGHT_GREY);
    print("space: mic off", bx, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
