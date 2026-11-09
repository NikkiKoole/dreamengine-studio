/* de:meta
{
  "slug": "grandkeys",
  "title": "grand keys",
  "status": "wip",
  "created": "2026-11-09",
  "kind": ["instrument"],
  "teaches": ["waveguide-synth"],
  "lineage": "The playable face of steinway.h: a real Steinway D (bitKlavier Grand, Daniel Trueman, Princeton, CC BY 4.0) on INSTR_PIANO's hybrid, measured note by note in pianoref and first heard in openstrings. keybed.h does the keys.",
  "description": {
    "summary": "Play a real Steinway grand yourself. Every key starts with a recording of the real piano, and when you hold it (or the pedal) past a few seconds the note carries on as a modelled string, so it rings as long as a real one.",
    "detail": "There are 13 recordings, one every half octave; every other key plays its nearest one a little faster or slower. At 2.6 s each note fades from the recording into a waveguide string tuned to match it, which is why a held chord keeps ringing past the 3.2 s the recordings last. P switches the recordings off so you hear the modelled piano alone. Piano: bitKlavier Grand Sample Library, Daniel Trueman, Princeton University (CC BY 4.0).",
    "controls": "keys: A-L white, W E T Y U O P black (Z/X octave) · mouse/touch on the keys · a MIDI keyboard works too · SPACE (or a MIDI sustain pedal) = sustain pedal · UP/DOWN loudness · P Steinway / modelled piano"
  }
}
de:meta */
// grand keys — a real Steinway on a keybed.
//
// THE POINT. steinway.h gives INSTR_PIANO a real recording per note that fades into the modelled
// string at 2.6 s (the hybrid). Hold a note, or the pedal, and you hear the hand-over happen. The
// SUSTAIN PEDAL is the reason this cart owns its voices instead of letting keybed.h do it: a key let
// go while the pedal is down keeps ringing until the pedal comes up.
#include "studio.h"
#include "keybed.h"
#include "steinway.h"
#include <stdio.h>

#define SLOT    5
#define KEY_TOP 76

static bool  ok = false, real = true;  // recordings loaded · playing them (P) or the model alone
static bool  pedal = false;
static int   vel = 5;                  // 5 = the loudness the recordings were made at
static int   hnd[128];                 // the voice sounding on each key, -1 = none
static bool  sus[128];                 // let go while the pedal was down: still ringing

static void key_down(int m, int v) {
    if (hnd[m] >= 0) note_off(hnd[m]);           // the same key struck again: a fresh strike
    hnd[m] = note_on(m, SLOT, v);
    sus[m] = false;
}
static void key_up(int m) {
    if (pedal) { sus[m] = true; return; }        // the pedal holds the damper off
    if (hnd[m] >= 0) note_off(hnd[m]);
    hnd[m] = -1;
}
static void pedal_up(void) {
    for (int m = 0; m < 128; m++)
        if (sus[m]) { if (hnd[m] >= 0) note_off(hnd[m]); hnd[m] = -1; sus[m] = false; }
}

void init(void) {
    for (int m = 0; m < 128; m++) hnd[m] = -1;
    instrument(SLOT, INSTR_PIANO, 1, 0, 7, 150);  // held while the key is down; 150 ms damper on release
    instrument_harmonics(SLOT, 0.08f);            // the grand voicing
    instrument_timbre(SLOT, 0.5f);
    instrument_morph(SLOT, 1.0f);                 // an undamped string while held, like the recordings
    instrument_reverb(SLOT, 0.22f);
    reverb(0.5f, 0.4f);
    ok = steinway_load(0) == STEINWAY_ZONES;
    instrument_mode(SLOT, MODE_PIANO_MODEL, 1.0f);
    if (ok) steinway_use(SLOT, 0);
    keybed_config(SLOT, 3, 21);                   // C3 .. B5, three octaves; Z / X shift
    keybed_layout(0, KEY_TOP, SCREEN_W, SCREEN_H - KEY_TOP);
    keybed_velocity(vel);
    keybed_manage_voices(false);                  // the cart owns the voices (the pedal)
    keybed_on_note(key_down);
    keybed_on_off(key_up);
}

void update(void) {
    bool p = key(KEY_SPACE) || midi_cc(0, 64) >= 64;
    if (pedal && !p) { pedal = false; pedal_up(); }
    pedal = p;
    if (keyp(KEY_UP))   { if (vel < 7) vel++; keybed_velocity(vel); }
    if (keyp(KEY_DOWN)) { if (vel > 1) vel--; keybed_velocity(vel); }
    if (keyp('P') && ok) {
        real = !real;
        if (real) steinway_use(SLOT, 0); else steinway_detach(SLOT);
    }
    keybed_update();
#ifdef DE_TRACE
    int n = 0; for (int m = 0; m < 128; m++) if (hnd[m] >= 0) n++;
    watch("sounding", "%d", n);
    watch("pedal", "%d", pedal ? 1 : 0);
#endif
}

static int key_col(int m, int base, int held_col) {
    if (keybed_held(m)) return held_col;
    if (sus[m]) return CLR_PEACH;                 // ringing on the pedal
    return base;
}

void draw(void) {
    cls(CLR_BLACK);
    // the fallboard: a strip of dark wood with the name on it
    rectfill(0, 0, SCREEN_W, KEY_TOP - 6, CLR_DARK_BROWN);
    rectfill(0, KEY_TOP - 6, SCREEN_W, 6, CLR_BROWNISH_BLACK);
    print("grand keys", 8, 8, CLR_WHITE);
    print(real && ok ? "P steinway" : ok ? "P model" : "(no recordings)", SCREEN_W - 96, 8,
          real && ok ? CLR_YELLOW : CLR_LIGHT_GREY);
    char b[48];
    snprintf(b, sizeof b, "loud %d  oct C%d", vel, keybed_octave());
    print(b, 8, 24, CLR_LIGHT_GREY);
    // the pedal
    int px = SCREEN_W - 44, py = 30;
    rectfill(px, py, 28, 10, pedal ? CLR_YELLOW : CLR_DARK_GREY);
    print("pedal", px - 44, py + 1, pedal ? CLR_YELLOW : CLR_MEDIUM_GREY);
    print("SPACE pedal  UP/DOWN loud  Z/X octave", 8, 44, CLR_MEDIUM_GREY);
    if (ok && real) {                             // CC BY 4.0: the credit shows while it plays
        font(FONT_TINY);
        print("piano: bitKlavier Grand, D. Trueman, Princeton (CC BY 4.0)", 8, 58, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }
    // the keys: white, then black on top
    for (int k = 0; k < keybed_white_count(); k++) {
        int x, y, w, h; keybed_white_rect(k, &x, &y, &w, &h);
        int m = keybed_white_midi(k);
        rectfill(x, y, w - 1, h, key_col(m, CLR_WHITE, CLR_LIGHT_PEACH));
        if (k % 7 == 0) print("C", x + w / 2 - 3, y + h - 12, CLR_MEDIUM_GREY);
    }
    for (int k = 0; k < keybed_white_count(); k++) {
        int x, y, w, h, m; if (!keybed_black_rect(k, &x, &y, &w, &h, &m)) continue;
        rectfill(x, y, w, h, key_col(m, CLR_BROWNISH_BLACK, CLR_BROWN));
    }
}
