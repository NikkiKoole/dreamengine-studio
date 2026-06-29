/* de:meta
{
  "title": "touch piano",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "instrument"
  ],
  "teaches": [],
  "description": "A two-octave multi-finger piano - the touch-release API's first customer (touch_ended_count/id/x/y). Every finger landing on a key starts a sustained note; THAT finger lifting releases exactly its own note, even with four other fingers still playing - impossible before the release API, since a lifted finger has no touch index. Slide a finger across keys for glissando, hold up to 5-finger chords on iPhone (Safari's touch ceiling), ~10 on iPad. Lift ripples mark where each finger let go (touch_ended_x/y). On desktop the mouse is one finger."
}
de:meta */
// touch piano — the multitouch keybed, now on the shared keybed.h
//
// Hold keys to sound them; slide a finger across for glissando; lift to release.
// Each held key glows in the colour of the finger holding it, and a ripple
// spawns where a finger lets go. Two octaves of white + black keys, plus the
// QWERTY home row / row above and a plugged-in MIDI keyboard for free.
//
// All the key plumbing (per-finger capture, glissando, voices) lives in
// keybed.h; this cart just adds the rainbow-finger colouring + lift ripples.
#include "studio.h"
#include "keybed.h"
#include <stdio.h>

#define SLOT     5
#define KEY_TOP  56

// lift ripples — drawn at the released finger's last position
typedef struct { int x, y, life; } Rip;
static Rip rips[16];

static const int FCOLS[6] = { CLR_RED, CLR_BLUE, CLR_GREEN, CLR_ORANGE, CLR_PINK, CLR_LIME_GREEN };
static int id_color(int id) { return FCOLS[(id < 0 ? -id : id) % 6]; }

void init(void) {
    instrument(SLOT, INSTR_SAW, 8, 140, 6, 260);   // snappy synth-piano
    keybed_config(SLOT, 4, 14);                     // base C4, two octaves of white keys
    keybed_layout(0, KEY_TOP, SCREEN_W, SCREEN_H - KEY_TOP);
}

void update(void) {
    keybed_update();
    // a ripple wherever a finger lifted this frame
    for (int i = 0; i < touch_ended_count(); i++)
        for (int r = 0; r < 16; r++)
            if (rips[r].life <= 0) { rips[r] = (Rip){ touch_ended_x(i), touch_ended_y(i), 14 }; break; }
    for (int r = 0; r < 16; r++) if (rips[r].life > 0) rips[r].life--;
}

void draw(void) {
    cls(CLR_BLACK);

    int nw = keybed_white_count();
    // white keys — held ones glow in the holding finger's colour
    for (int k = 0; k < nw; k++) {
        int x, y, w, h; keybed_white_rect(k, &x, &y, &w, &h);
        int midi = keybed_white_midi(k);
        int finger = keybed_finger(midi);
        int col = keybed_held(midi) ? (finger >= 0 ? id_color(finger) : CLR_LIME_GREEN) : CLR_WHITE;
        rectfill(x + 1, y, w - 2, h, col);
        rect(x, y, w, h, CLR_DARK_GREY);
        if (k % 7 == 0) print(str("C%d", keybed_octave() + k / 7), x + 4, y + h - 12, CLR_MEDIUM_GREY);
    }
    // black keys
    for (int k = 0; k < nw; k++) {
        int x, y, w, h, midi; if (!keybed_black_rect(k, &x, &y, &w, &h, &midi)) continue;
        int finger = keybed_finger(midi);
        int col = keybed_held(midi) ? (finger >= 0 ? id_color(finger) : CLR_LIME_GREEN) : CLR_DARK_GREY;
        rectfill(x, y, w, h, col);
        rect(x, y, w, h, CLR_BLACK);
    }

    // lift ripples
    for (int r = 0; r < 16; r++)
        if (rips[r].life > 0) circ(rips[r].x, rips[r].y, 16 - rips[r].life, CLR_LIGHT_GREY);

    // header
    int held = 0;
    for (int m = 0; m < 128; m++) if (keybed_held(m)) held++;
    char buf[48];
    snprintf(buf, sizeof buf, "TOUCH PIANO   fingers:%d  notes:%d", touch_count(), held);
    print(buf, 4, 6, CLR_WHITE);
    font(FONT_SMALL);
    print("hold/slide keys - QWERTY or MIDI too - Z X octave", 4, 20, CLR_LIGHT_GREY);
    if (midi_present()) print("MIDI keyboard connected", 4, 28, CLR_LIME_GREEN);
    font(FONT_NORMAL);
}
