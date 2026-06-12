#include "studio.h"
#include "keybed.h"

// KEYBEDTEST — throwaway check for keybed.h (step 2 of docs/design/midi-and-keybed.md).
// The built-in drawn manual, played by touch / mouse / QWERTY (home row + the row
// above) / a plugged-in MIDI keyboard, all at once. Z/X shift octave.

void init(void) {
    instrument(5, INSTR_SAW, 8, 140, 6, 260);
    keybed_config(5, 4, 14);                         // slot 5, base C4, 14 white keys (2 octaves)
    keybed_layout(0, 40, SCREEN_W, SCREEN_H - 40);
}

void update(void) { keybed_update(); }

void draw(void) {
    cls(CLR_BLACK);
    keybed_draw();
    print("KEYBED.H  touch / ASDF.. / MIDI   Z X = octave", 6, 6, CLR_WHITE);
    print(midi_present() ? "midi: CONNECTED" : "midi: none", 6, 16, midi_present() ? CLR_LIME_GREEN : CLR_DARK_GREY);
    int n = 0; for (int m = 0; m < 128; m++) if (keybed_held(m)) n++;
    print(str("notes: %d", n), 6, 26, CLR_LIGHT_GREY);
}
