// Book illustration (ch.4): a row of dots riding a sine wave that DRIFTS sideways
// on its own — driven by frame(). Loops seamlessly every 120 frames (phase 0..360).
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    line(0, 100, 320, 100, CLR_DARK_GREY);          // the calm baseline
    int bright[] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_INDIGO };
    float phase = frame() * 3.0f;                   // the drift — one full loop per 120 frames
    // faint continuous curve behind the dots
    for (int x = 0; x < 320; x++) {
        int y = 100 + (int)(sin_deg(x * 2.4f + phase) * 46);
        pset(x, y, CLR_DARK_GREEN);
    }
    // the dots themselves, spaced out and coloured along the ramp
    for (int i = 0; i < 16; i++) {
        int x = 14 + i * 20;
        int y = 100 + (int)(sin_deg(x * 2.4f + phase) * 46);
        circfill(x, y, 6, bright[i % 6]);
    }
}
