// panmeasure — a throwaway measurement rig for the pan-law A/B (NOT a showcase cart).
// Holds one steady tone and sweeps its pan -1 -> +1 twice: phase 1 under PAN_LINEAR,
// phase 2 under PAN_POWER. Render deterministically with --wav, then read per-channel
// RMS across the sweep to see exactly how the two laws differ (center buildup vs flat).
//
//   node tools/play.js panmeasure script /dev/null --headless --frames 1320 --wav build/panlaw.wav --det
//
// Timeline @60fps: f0..600 LINEAR sweep, f600 switch to POWER, f660..1260 POWER sweep.
#include "studio.h"

#define SL 5
#define PH1_LO  30
#define PH1_HI  600
#define SWITCH  620
#define PH2_LO  660
#define PH2_HI  1260

static int   h;     // held tone handle
static int   t;     // frame counter
static int   law;   // 0 LINEAR, 1 POWER

void init(void) {
    instrument(SL, INSTR_SAW, 10, 0, 7, 200);   // steady, full sustain — no amplitude wobble
    h = note_on(57, SL, 6);                      // A3, constant
}

void update(void) {
    t++;
    if (t == SWITCH) { law = 1; pan_law(PAN_POWER); }

    float p = 0.0f;
    if (t >= PH1_LO && t <= PH1_HI)
        p = -1.0f + 2.0f * (float)(t - PH1_LO) / (PH1_HI - PH1_LO);
    else if (t >= PH2_LO && t <= PH2_HI)
        p = -1.0f + 2.0f * (float)(t - PH2_LO) / (PH2_HI - PH2_LO);
    note_pan(h, p);
}

void draw(void) {
    cls(CLR_BLACK);
    print(law ? "PAN_POWER" : "PAN_LINEAR", 8, 8, CLR_WHITE);
}
