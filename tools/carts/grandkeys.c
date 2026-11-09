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
    "detail": "There are 13 recordings, one every half octave; every other key plays its nearest one a little faster or slower. At 2.6 s each note fades from the recording into a waveguide string tuned to match it, which is why a held chord keeps ringing past the 3.2 s the recordings last. 1 2 3 pick one of three sounds: the Steinway, the modelled piano alone, and SINE KEY, two sines 6 cents apart with chorus and delay (Felucca's preset, MIT, as Flyfish plays it). Piano: bitKlavier Grand Sample Library, Daniel Trueman, Princeton University (CC BY 4.0).",
    "controls": "keys: A-L white, W E T Y U O P black (Z/X octave) · mouse/touch on the keys · a MIDI keyboard works too · SPACE (or a MIDI sustain pedal) = sustain pedal · UP/DOWN loudness · 1 Steinway / 2 modelled piano / 3 sine key"
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
#include <string.h>

#define SLOT    5
#define SL_SINE 6
#define KEY_TOP 76

static bool  ok = false;               // recordings loaded
static int   sound = 0;                // 1 2 3: 0 the Steinway · 1 the modelled piano alone · 2 SINE KEY
static const char *SOUND_NAME[3] = { "1 steinway", "2 model", "3 sine key" };
static bool  pedal = false;
static int   vel = 5;                  // 5 = the loudness the recordings were made at
static int   hnd[128];                 // the voice sounding on each key, -1 = none
static bool  sus[128];                 // let go while the pedal was down: still ringing

static void key_down(int m, int v) {
    if (hnd[m] >= 0) note_off(hnd[m]);           // the same key struck again: a fresh strike
    hnd[m] = note_on(m, sound == 2 ? SL_SINE : SLOT, v);
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
    // SINE KEY: Felucca's preset of that name (Felucca-Engines src/eng_analog.c, MIT, (c) 2026 Leo Kuroshita,
    // Hugelton Instruments), as Flyfish plays it. Two sines 6 cents apart (the slow beat is the shimmer), the
    // filter open, ATK 2 / DEC 80 / SUS 30 / REL 70 on Felucca's 1 ms .. 10 s curve = 1 ms, 330 ms, 24 %, 160 ms,
    // and sends chorus 30 / delay 25 / reverb 40 of 127.
    instrument(SL_SINE, INSTR_SINE, 1, 330, 2, 160);
    instrument_unison(SL_SINE, 2, 0.06f);
    instrument_chorus(SL_SINE, 1.0f, 0.4f, 0.24f);
    instrument_echo(SL_SINE, 0.2f);
    instrument_reverb(SL_SINE, 0.31f);
    instrument_level(SL_SINE, 0.5f);             // measured: 6 dB over the piano at the same loudness
    echo(300, 0.35f, 0.5f);
    ok = steinway_load(0) == STEINWAY_ZONES;
    instrument_mode(SLOT, MODE_PIANO_MODEL, 1.0f);
    if (ok) steinway_use(SLOT, 0); else sound = 1;
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
    for (int k = 0; k < 3; k++)                   // 1 2 3 pick the sound (not P: P is a black key on the keybed)
        if (keyp('1' + k) && (k != 0 || ok) && k != sound) {
            sound = k;
            if (ok) { if (sound == 0) steinway_use(SLOT, 0); else steinway_detach(SLOT); }
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
    print(SOUND_NAME[sound], SCREEN_W - 8 - 8 * (int)strlen(SOUND_NAME[sound]), 8, sound == 0 ? CLR_YELLOW : CLR_LIGHT_GREY);
    char b[48];
    snprintf(b, sizeof b, "loud %d  oct C%d", vel, keybed_octave());
    print(b, 8, 24, CLR_LIGHT_GREY);
    // the pedal
    int px = SCREEN_W - 44, py = 30;
    rectfill(px, py, 28, 10, pedal ? CLR_YELLOW : CLR_DARK_GREY);
    print("pedal", px - 44, py + 1, pedal ? CLR_YELLOW : CLR_MEDIUM_GREY);
    print("SPACE pedal  UP/DOWN loud  Z/X octave", 8, 44, CLR_MEDIUM_GREY);
    if (ok && sound == 0) {                             // CC BY 4.0: the credit shows while it plays
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
