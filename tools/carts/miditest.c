#include "studio.h"
#include <stdio.h>

// MIDITEST — throwaway probe for the engine-level MIDI input layer (step 1 of
// docs/design/midi-and-keybed.md). Plug in a USB MIDI keyboard: every key you
// press plays a note AND scrolls into a log; held keys light a 128-note grid;
// the pitch-bend wheel moves a bar. Proves the KeyStep reaches C before keybed.h.

#define SLOT 5

static char  logbuf[8][32];
static int   logn = 0;

static void logline(const char *s) {
    if (logn < 8) { snprintf(logbuf[logn], 32, "%s", s); logn++; }
    else { for (int i = 0; i < 7; i++) snprintf(logbuf[i], 32, "%s", logbuf[i + 1]); snprintf(logbuf[7], 32, "%s", s); }
}

void init(void) {
    instrument(SLOT, INSTR_SAW, 8, 120, 5, 250);
    instrument_filter(SLOT, FILTER_LOW, 2200, 8);
}

void update(void) {
    int mnote, vel, t;
    while ((t = midi_get(&mnote, &vel)) != 0) {
        if (t > 0) { int v = vel >> 4; note(mnote, SLOT, v ? v : 1); logline(str("on  %3d v%3d", mnote, vel)); }
        else       { logline(str("off %3d", mnote)); }
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("MIDI TEST", 8, 6, CLR_WHITE);
    print(midi_present() ? str("device: %s", midi_name()[0] ? midi_name() : "CONNECTED") : "device: none (plug in USB MIDI)",
          8, 16, midi_present() ? CLR_LIME_GREEN : CLR_DARK_GREY);

    // 128-note held grid (16 x 8)
    for (int n = 0; n < 128; n++) {
        int gx = 8 + (n % 16) * 9, gy = 30 + (n / 16) * 9;
        bool down = midi_held(n);
        rectfill(gx, gy, 8, 8, down ? CLR_PINK : CLR_DARKER_GREY);
    }

    // pitch-bend bar
    int b = midi_bend();                       // -8192..8191
    int cx = SCREEN_W / 2, bw = 120;
    rect(cx - bw / 2, 108, bw, 8, CLR_DARK_GREY);
    int bx = cx + (b * (bw / 2)) / 8192;
    rectfill(bx - 1, 106, 3, 12, CLR_YELLOW);
    print("bend", cx - 12, 118, CLR_INDIGO);

    // event log
    font(FONT_SMALL);
    for (int i = 0; i < logn; i++) print(logbuf[i], 8, 132 + i * 8, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}
