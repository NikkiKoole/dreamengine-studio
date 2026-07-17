/* de:meta
{
  "slug": "livetune",
  "title": "livetune (live auto-tune spike)",
  "status": "active",
  "kind": ["instrument", "tech-demo"],
  "teaches": ["audio-input"],
  "created": "2026-07-18",
  "lineage": "The SPIKE for LIVE real-time auto-tune (docs/design/transparent-autotune.md §the live real-time path). The offline sample_autotune() shipped + was confirmed; this tests the hard follow-on: streaming formant-preserving pitch correction on the AUDIO THREAD (autotune_mic) — sing and hear yourself snapped in tune NOW, not record-then-play. The whole question is latency + streaming-epoch quality; the deliverable is a FEEL test, not an oracle number (there is no frozen output). NOT hardtune (robot vocoder) — this keeps your voice.",
  "description": {
    "summary": "Sing and hear yourself auto-tuned in REAL TIME. Press SPACE, allow the mic, and sing an 'ah' — your pitch snaps onto the scale live while you still sound like you. The ribbon scrolls your pitch (orange) snapping to the notes (green). A spike: the point is whether the latency feels live.",
    "detail": "Front-end for autotune_mic(root, scale, amount) — streaming TD-PSOLA on the audio thread (docs/design/transparent-autotune.md §live). SPACE opens the mic and turns on live correction: the engine reads the mic ring, tracks the pitch, and re-spaces glottal grains at the snapped target period so your voice comes back in tune, formants kept. A scrolling ribbon shows your live pitch vs where it snaps; a level meter shows it's hearing you; the latency note is the inherent buffering. S cycles the scale; [ / ] ride the correction amount. LIVE — does not replay deterministically (ADR-0032). This is a spike: if it lags or gargles, that's the finding.",
    "controls": "SPACE: mic + live auto-tune on/off. S: scale. [ / ]: correction amount down/up. Sing an 'ah'."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// LIVETUNE — the live real-time auto-tune spike (docs/design/transparent-autotune.md §live).
// Sing and hear yourself corrected NOW via autotune_mic() (streaming PSOLA on the audio thread).
// The question is latency + quality; judge it by ear. Ribbon shows your pitch snapping live.

#define ROOT 0
#define HIST 260                 // scrolling pitch-history columns

static int   live = 0;
static int   scale_i = SCALE_MINOR;
static float amount = 1.0f;
static float hist[HIST];         // recent pitch, MIDI (0 = unvoiced), scrolls left
static int   hw = 0;

static const int SLEN[6] = { 7, 7, 5, 5, 6, 12 };
static const int SDEG[6][12] = {
    { 0,2,4,5,7,9,11 }, { 0,2,3,5,7,8,10 }, { 0,2,4,7,9 },
    { 0,3,5,7,10 }, { 0,3,5,6,7,10 }, { 0,1,2,3,4,5,6,7,8,9,10,11 },
};
static const char *SNAME[6] = { "major", "minor", "penta", "penta-", "blues", "chroma" };
static const char *NOTE[12] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };
static float vsnap(float m) {
    int len = SLEN[scale_i]; float best = m, bd = 1e9f;
    for (int o = 0; o < 11; o++) for (int i = 0; i < len; i++) {
        float c = 12.0f * o + ROOT + SDEG[scale_i][i], d = fabsf(m - c);
        if (d < bd) { bd = d; best = c; }
    }
    return best;
}

void update(void) {
    if (keyp(KEY_SPACE)) {
        live = !live;
        if (live) { mic_start(); autotune_mic(ROOT, scale_i, amount); }
        else      { autotune_mic(ROOT, scale_i, 0.0f); }
    }
    if (keyp('S')) { scale_i = (scale_i + 1) % 6; if (live) autotune_mic(ROOT, scale_i, amount); }
    if (keyp(']')) { amount += 0.1f; if (amount > 1.0f) amount = 1.0f; if (live) autotune_mic(ROOT, scale_i, amount); }
    if (keyp('[')) { amount -= 0.1f; if (amount < 0.0f) amount = 0.0f; if (live) autotune_mic(ROOT, scale_i, amount); }

    if (live) {                                  // scroll the pitch history
        float hz = mic_pitch();
        hist[hw % HIST] = hz > 0 ? 69.0f + 12.0f * log2f(hz / 440.0f) : 0.0f;
        hw++;
    }

#ifdef DE_TRACE
    watch("live", "%d", live);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_COMIC); print("livetune", 8, 5, CLR_INDIGO); font(FONT_NORMAL);
    print("live auto-tune", 108, 9, CLR_MEDIUM_GREY);

    if (!live) {
        print("Press SPACE, then sing an 'ah'", SCREEN_W/2 - 116, 76, CLR_WHITE);
        print("hear yourself snap in tune, live", SCREEN_W/2 - 124, 96, CLR_LIGHT_GREY);
        print("SPACE start   S scale   [ ] amount", 8, SCREEN_H - 12, CLR_MEDIUM_GREY);
        return;
    }

    int ready = mic_active();
    print(ready ? "\x07 LIVE  sing an 'ah'" : "opening mic... allow the prompt", 8, 28, ready ? CLR_GREEN : CLR_ORANGE);

    // level meter (are we hearing you?)
    float lv = mic_level(); int mw = (int)(lv * 3.0f * 120.0f); if (mw > 120) mw = 120;
    print("in", 8, 44, CLR_DARK_GREY); rect(26, 44, 120, 8, CLR_DARKER_GREY); rectfill(26, 44, mw, 8, CLR_GREEN);

    // scrolling pitch ribbon: your pitch (orange) snapping to the scale grid (green)
    int rx = 20, ry = 60, rw = SCREEN_W - 30, rh = 96;
    float mlo = 45, mhi = 65;                        // A2..F4 window
    for (int mm = (int)mlo; mm <= (int)mhi; mm++) {
        if (fabsf(vsnap((float)mm) - mm) > 0.01f) continue;
        int y = ry + rh - (int)((mm - mlo) / (mhi - mlo) * rh);
        line(rx, y, rx + rw, y, CLR_DARKER_GREY);
        print(NOTE[((mm % 12) + 12) % 12], rx - 16, y - 3, CLR_DARK_GREY);
    }
    for (int i = 0; i < HIST && i < rw; i++) {
        float m = hist[(hw - HIST + i + HIST * 4) % HIST];   // oldest → newest, left → right
        if (m < 1) continue;
        int x = rx + i * rw / HIST;
        int yr = ry + rh - (int)((m        - mlo) / (mhi - mlo) * rh);
        int yt = ry + rh - (int)((vsnap(m) - mlo) / (mhi - mlo) * rh);
        if (yr >= ry && yr < ry + rh) pset(x, yr, CLR_ORANGE);
        if (yt >= ry && yt < ry + rh) pset(x, yt, CLR_GREEN);
    }
    print("orange = you    green = snapped", rx, ry + rh + 4, CLR_DARK_GREY);

    char buf[64];
    snprintf(buf, sizeof buf, "SPACE stop   S %s   amount %.0f%%   ~%dms lag", SNAME[scale_i], amount * 100.0f, 27);
    print(buf, 8, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
