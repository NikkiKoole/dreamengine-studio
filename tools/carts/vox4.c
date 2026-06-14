#include "studio.h"
#include "ui.h"
#include <math.h>

// VOX4 — the voice on its LOCKED public API (no experimental note_aux here). The 3 standard
// engine macros ARE the voice, plus the one earned 4th knob:
//
//   harmonics = VOWEL  — what's said (U → O → A → E → I)
//   timbre    = SIZE   — who says it (vocal-tract length, giant → child)
//   morph     = EFFORT — how it's said (soft/dark/relaxed → bright/pressed)
//   voice_nasal()      = NASAL — open → hummed/nasal (the honk, the Tibetan-monk chant)
//
// Driven live with note_harmonics/timbre/morph + voice_nasal on the held note — the same 3-macro
// muscle memory as every other engine. Vibrato is the universal note_lfo; consonants are the
// voice_consonant()/voice_coda() events (see voxpad). For the monk drone: low pitch + NASAL up +
// EFFORT soft + vowel oh/oo.
//
// CONTROLS: drag the 4 sliders · SPACE hold a drone · Z/X pitch · V vibrato.

#define SLOT 5

static const char *VOWELS[5] = { "oo", "oh", "ah", "eh", "ee" };

static float vowel  = 0.5f;
static float size_v = 0.33f;
static float effort = 0.5f;
static float nasal  = 0.0f;
static int   voice  = -1;
static int   drone  = 0;
static float pitch  = 48;
static int   vibrato = 0;

// drive the held voice through the PUBLIC API — the 3 macros + the nasal knob
static void apply_voice(int v) {
    note_harmonics(v, vowel);    // VOWEL
    note_timbre(v, size_v);      // SIZE
    note_morph(v, effort);       // EFFORT (the engine expands it to breath/open-q/tilt)
    voice_nasal(v, nasal);       // NASAL
}

static void apply_vibrato(int v) { note_lfo(v, 0, LFO_PITCH, 5.5f, vibrato ? 0.18f : 0.0f); }

static void start_drone(void) {
    if (voice >= 0) note_off(voice);
    instrument_harmonics(SLOT, vowel);   // seed the macros so the note speaks right from frame 0
    instrument_timbre(SLOT, size_v);
    instrument_morph(SLOT, effort);
    voice = note_on((int)(pitch + 0.5f), SLOT, 6);
    note_glide(voice, 25);
    apply_voice(voice);
    apply_vibrato(voice);
}

void init(void) {
    instrument(SLOT, INSTR_VOICE, 45, 60, 7, 160);
}

void update(void) {
    if (keyp(KEY_SPACE)) {
        drone = !drone;
        if (drone) start_drone();
        else if (voice >= 0) { note_off(voice); voice = -1; }
    }
    if (keyp('Z')) pitch = clamp(pitch - 1, 24, 72);
    if (keyp('X')) pitch = clamp(pitch + 1, 24, 72);
    if (keyp('V')) { vibrato = !vibrato; if (voice >= 0) apply_vibrato(voice); }

    if (drone && voice >= 0) { note_pitch(voice, pitch); apply_voice(voice); }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("VOX4", 6, 4, CLR_PEACH);
    print("the voice - locked API", 50, 4, CLR_MEDIUM_GREY);

    ui_begin();

    print("VOWEL",  6, 28, CLR_LIGHT_PEACH); ui_slider(&vowel,  64, 28, 160, 0);
    print("SIZE",   6, 48, CLR_LIGHT_PEACH); ui_slider(&size_v, 64, 48, 160, 0);
    print("EFFORT", 6, 68, CLR_LIGHT_PEACH); ui_slider(&effort, 64, 68, 160, 0);
    print("NASAL",  6, 88, CLR_LIGHT_YELLOW); ui_slider(&nasal, 64, 88, 160, 0);

    if (ui_button(6, 112, 76, 16, drone ? "DRONE on" : "drone")) {
        drone = !drone;
        if (drone) start_drone(); else if (voice >= 0) { note_off(voice); voice = -1; }
    }
    if (ui_button(90, 112, 20, 16, "-")) pitch = clamp(pitch - 1, 24, 72);
    if (ui_button(140, 112, 20, 16, "+")) pitch = clamp(pitch + 1, 24, 72);
    if (ui_button(170, 112, 90, 16, vibrato ? "vibrato ON" : "vibrato off"))
        { vibrato = !vibrato; if (voice >= 0) apply_vibrato(voice); }

    ui_end();

    int vi = (int)(vowel * 4.0f + 0.5f); if (vi > 4) vi = 4; if (vi < 0) vi = 0;
    print(VOWELS[vi], 232, 28, CLR_WHITE);
    print(size_v < 0.3f ? "giant" : (size_v > 0.6f ? "child" : "adult"), 232, 48, CLR_LIGHT_GREY);
    print(effort < 0.4f ? "soft" : (effort > 0.7f ? "bright" : "mid"), 232, 68, CLR_LIGHT_GREY);
    print(nasal > 0.5f ? "hummed" : (nasal > 0.05f ? "nasal" : "open"), 232, 88, CLR_LIGHT_GREY);

    char pb[5]; int m = (int)(pitch + 0.5f);
    pb[0] = 'm'; pb[1] = '0' + (m / 10) % 10; pb[2] = '0' + m % 10; pb[3] = 0;
    print_centered(pb, 125, 116, CLR_YELLOW);

    print("macros: harmonics=VOWEL  timbre=SIZE  morph=EFFORT  + voice_nasal", 6, 150, CLR_DARK_GREY);
    print("monk drone: low pitch (Z) + NASAL up + EFFORT soft + vowel oh/oo", 6, 166, CLR_DARKER_GREY);
    print("SPACE drone   Z/X pitch   V vibrato   drag the sliders", 6, 184, CLR_DARK_GREY);
}
