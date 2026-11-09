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
    "detail": "There are 13 recordings, one every half octave; every other key plays its nearest one a little faster or slower. At 2.6 s each note fades from the recording into a waveguide string tuned to match it, which is why a held chord keeps ringing past the 3.2 s the recordings last. 1-4 pick a sound: the Steinway, a PLEYEL (the same recordings voiced like the 1840s piano Chopin played: darker, a quicker decay, a lighter touch), the modelled piano alone, and SINE KEY, two sines 6 cents apart with chorus and delay (Felucca's preset, MIT, as Flyfish plays it). Piano: bitKlavier Grand Sample Library, Daniel Trueman, Princeton University (CC BY 4.0).",
    "controls": "keys: A-L white, W E T Y U O P black (Z/X octave) · mouse/touch on the keys · a MIDI keyboard works too · SPACE (or MIDI CC64, or the on-screen pedal) = sustain · C (or CC67, or the on-screen pedal) = soft pedal, una corda: notes struck with it down are quieter and darker · UP/DOWN loudness · 1 Steinway / 2 Pleyel / 3 modelled piano / 4 sine key"
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
#include <math.h>

#define SLOT    5
#define SL_SINE 6
#define KEY_TOP 76

enum { S_STEINWAY, S_PLEYEL, S_MODEL, S_SINE, S_N };
static const char *SOUND_NAME[S_N] = { "1 steinway", "2 pleyel", "3 model", "4 sine key" };
static bool  ok = false;               // recordings loaded
static int   sound = S_STEINWAY;       // 1-4 pick it
static bool  pedal = false, soft = false;
static int   vel = 5;                  // 5 = the loudness the recordings were made at
static int   hnd[128];                 // the voice sounding on each key, -1 = none
static bool  sus[128];                 // let go while the pedal was down: still ringing
static float vol0[128];                // the note's volume at the strike (the Pleyel fade rides it down)
static int   age[128];                 // frames since the strike

// ── PLEYEL: the same Steinway recordings, voiced after the piano Chopin played (Flyfish's idea; our own
// numbers). An 1840s Pleyel is lighter-built than a modern grand: leather-and-felt hammers, lighter strings,
// a smaller sound that dies sooner. So: DARKER (a low-pass that follows the note, its overtones above
// ~7x the fundamental cut, as a softer hammer would), a QUICKER DECAY (6 dB/s on top of the Steinway's
// own), and a LIGHTER touch (a little quieter at the same key velocity).
#define PL_HARM   7.0f                 // the low-pass sits at this many times the fundamental ...
#define PL_LO     900.0f               // ... but never below / above these
#define PL_HI     5000.0f
#define PL_FADE   6.0f                 // extra dB a second
#define PL_TOUCH  0.85f
// ── SOFT PEDAL (una corda): the action shifts so the hammer hits fewer strings with a less-worn part of
// its felt, so a note struck with it down is QUIETER and DARKER. It acts at the strike, as on a piano:
// pressing it under a sounding note changes nothing.
#define SOFT_VOL  0.65f                // ~ -3.7 dB
#define SOFT_HARM 9.0f                 // the low-pass at 9x the fundamental (1200 .. 6000 Hz) ...
#define SOFT_CUT  0.6f                 // ... or lower still under a Pleyel, whichever is darker
#define OPEN_HZ   18000                // at or above this a note plays with its filter OFF

static float midi_hz(int m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }

static void key_down(int m, int v) {
    if (hnd[m] >= 0) note_off(hnd[m]);           // the same key struck again: a fresh strike
    float vv = (float)v;
    float cut = OPEN_HZ;
    if (sound == S_PLEYEL) {
        vv *= PL_TOUCH;
        cut = fminf(PL_HI, fmaxf(PL_LO, midi_hz(m) * PL_HARM));
    }
    if (soft) {
        vv *= SOFT_VOL;
        cut = fminf(cut * SOFT_CUT, fminf(6000.0f, fmaxf(1200.0f, midi_hz(m) * SOFT_HARM)));
        if (cut < 400.0f) cut = 400.0f;
    }
    hnd[m] = note_on(m, sound == S_SINE ? SL_SINE : SLOT, v);
    if (sound != S_SINE) {                       // a note that needs no darkening gets NO filter: even an
        if (cut >= OPEN_HZ) note_filter(hnd[m], FILTER_OFF);   // "open" 18 kHz low-pass took 2.4 dB off the top
        else note_cutoff(hnd[m], (int)cut);
    }
    note_vol(hnd[m], vv);
    vol0[m] = vv; age[m] = 0;
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

static void set_sound(int s) {
    sound = s;
    if (ok) { if (s == S_STEINWAY || s == S_PLEYEL) steinway_use(SLOT, 0); else steinway_detach(SLOT); }
}

// the two pedals on the fallboard, soft (left) and sustain (right): a finger or the mouse can hold them
static const int PED_Y = 22, PED_W = 30, PED_H = 12;
static int ped_x(int i) { return SCREEN_W - 80 + i * 40; }
static bool ped_touched(int i) {
    for (int t = 0; t < touch_count(); t++) {
        int x = touch_x(t), y = touch_y(t);
        if (x >= ped_x(i) && x < ped_x(i) + PED_W && y >= PED_Y && y < PED_Y + PED_H) return true;
    }
    return false;
}

void init(void) {
    for (int m = 0; m < 128; m++) hnd[m] = -1;
    instrument(SLOT, INSTR_PIANO, 1, 0, 7, 150);  // held while the key is down; 150 ms damper on release
    instrument_harmonics(SLOT, 0.08f);            // the grand voicing
    instrument_timbre(SLOT, 0.5f);
    instrument_morph(SLOT, 1.0f);                 // an undamped string while held, like the recordings
    instrument_filter(SLOT, FILTER_LOW, OPEN_HZ, 0);   // per note: the Pleyel + the soft pedal set it (note_cutoff)
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
    if (ok) steinway_use(SLOT, 0); else sound = S_MODEL;
    keybed_config(SLOT, 3, 21);                   // C3 .. B5, three octaves; Z / X shift
    keybed_layout(0, KEY_TOP, SCREEN_W, SCREEN_H - KEY_TOP);
    keybed_velocity(vel);
    keybed_manage_voices(false);                  // the cart owns the voices (the pedals)
    keybed_on_note(key_down);
    keybed_on_off(key_up);
}

void update(void) {
    bool p = key(KEY_SPACE) || midi_cc(0, 64) >= 64 || ped_touched(1);
    if (pedal && !p) { pedal = false; pedal_up(); }
    pedal = p;
    soft = key('C') || midi_cc(0, 67) >= 64 || ped_touched(0);
    if (keyp(KEY_UP))   { if (vel < 7) vel++; keybed_velocity(vel); }
    if (keyp(KEY_DOWN)) { if (vel > 1) vel--; keybed_velocity(vel); }
    for (int k = 0; k < S_N; k++)                 // 1-4 pick the sound (not P: P is a black key on the keybed)
        if (keyp('1' + k) && (ok || (k != S_STEINWAY && k != S_PLEYEL)) && k != sound) set_sound(k);
    keybed_update();
    // the Pleyel's quicker decay: ride every sounding note down, PL_FADE dB a second past the Steinway's own
    if (sound == S_PLEYEL)
        for (int m = 0; m < 128; m++) if (hnd[m] >= 0) {
            age[m]++;
            note_vol(hnd[m], vol0[m] * powf(10.0f, -PL_FADE * (float)age[m] / 60.0f / 20.0f));
        }
#ifdef DE_TRACE
    int n = 0; for (int m = 0; m < 128; m++) if (hnd[m] >= 0) n++;
    watch("sounding", "%d", n);
    watch("pedal", "%d", pedal ? 1 : 0);
    watch("soft", "%d", soft ? 1 : 0);
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
    print(SOUND_NAME[sound], SCREEN_W - 8 - 8 * (int)strlen(SOUND_NAME[sound]), 8, sound <= S_PLEYEL ? CLR_YELLOW : CLR_LIGHT_GREY);
    char b[48];
    snprintf(b, sizeof b, "loud %d (up/dn)  oct C%d (z/x)", vel, keybed_octave());
    print(b, 8, 24, CLR_LIGHT_GREY);
    // the pedals: soft (left), sustain (right)
    for (int i = 0; i < 2; i++) {
        bool on = i ? pedal : soft;
        rectfill(ped_x(i), PED_Y, PED_W, PED_H, on ? CLR_YELLOW : CLR_DARK_GREY);
        print(i ? "sus" : "soft", ped_x(i) + (i ? 3 : 0), PED_Y + PED_H + 3, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
    }
    print("SPACE sus  C soft  1-4 sound", 8, 40, CLR_MEDIUM_GREY);
    if (ok && (sound == S_STEINWAY || sound == S_PLEYEL)) {                             // CC BY 4.0: the credit shows while it plays
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
