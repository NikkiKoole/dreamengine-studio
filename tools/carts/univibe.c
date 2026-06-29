/* de:meta
{
  "title": "univibe",
  "status": "active",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Univibe / Shin-ei Uni-Vibe pedal (Machine Gun, Bridge of Sighs); first cart on mod_optical — the asymmetric photocell LFO distinguishing a univibe from a plain phaser.",
  "description": "The showcase for univibe() - the 60s photocell vibe (Univibe / Shin-ei Uni-Vibe, the Machine Gun / Bridge of Sighs wobble). It's the SAME 4-stage allpass chain as the phaser, but swept by an OPTICAL LFO instead of a sine: a lightbulb glued to a photocell glows slowly bright then snaps dim, so the sweep is asymmetric and liquid where a sine phaser is even and clinical - that asymmetry IS the sound. A slow organ chord loop runs through the vibe; the LAMP in the middle is the live LFO (watch it ease up, then drop fast) and the curve strip draws one cycle of the shape. Press P to A/B OPTICAL (univibe) vs SINE (plain phaser) at the same rate/depth, so you hear only the difference the bulb makes. 1..4 play chords (Am/F/C/G); RATE/DEPTH/MIX sliders ride it live; H for help. The first cart built on the modulation kit (mod_optical) from the boutique-pedals roadmap."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── UNIVIBE ───────────────────────────────────────────────────────────────────
// The showcase for univibe() — the 60s photocell vibe (Univibe / Shin-ei Uni-Vibe,
// the Machine Gun / Bridge of Sighs wobble). It's the SAME 4-stage allpass chain as
// the phaser, but swept by an OPTICAL LFO instead of a sine: a lightbulb glued to a
// photocell glows SLOWLY bright then snaps dim, so the sweep is asymmetric and
// liquid where a sine phaser is even and clinical. That asymmetry is the whole sound.
//
//   A slow ORGAN chord loop runs through the vibe the whole time.
//   the LAMP (center) is the optical LFO — watch it ease up, then drop fast.
//   P        A/B the LFO shape: OPTICAL (univibe) vs SINE (plain phaser) — same
//            rate/depth, so you hear ONLY the difference the bulb makes.
//   1 2 3 4  play chords (Am / F / C / G)
//   sliders  RATE / DEPTH / MIX, live.   H  help.

#define SL_ORG  8

static float k_rate  = 0.45f;   // 0..1 → 0.2..8 Hz
static float k_depth = 0.70f;   // 0..1
static float k_mix   = 0.60f;   // 0..1
static int   sine    = 0;       // P: 0 = optical (univibe), 1 = sine (phaser)
static bool  show_help = false;

static int   chord_h[4] = { -1, -1, -1, -1 };
static int   chord_i = 0;
static float retrig  = 0.0f;
static float lamp_ph = 0.0f;    // local LFO phase for the visual lamp (synced to k_rate)

static float a_rate = -1, a_depth = -1, a_mix = -1; static int a_sine = -1;

static const int CHORD[4][4] = {
    { 57, 60, 64, 69 },   // Am
    { 53, 57, 60, 65 },   // F
    { 48, 55, 60, 64 },   // C
    { 55, 59, 62, 67 },   // G
};
static const char *CHORD_NAME[4] = { "Am", "F", "C", "G" };

static float rate_hz(void) { return 0.2f + k_rate * 7.8f; }

// the optical LFO shape, mirrored from the engine's mod_optical (for the lamp + curve viz)
static float optical(float ph) { return ph < 0.8f ? powf(ph / 0.8f, 0.6f) : 1.0f - (ph - 0.8f) / 0.2f; }
static float lamp_wave(float ph) { return sine ? 0.5f - 0.5f * cosf(ph * 6.2831853f) : optical(ph); }

static void play_chord(int ci) {
    for (int i = 0; i < 4; i++) {
        if (chord_h[i] >= 0) note_off(chord_h[i]);
        chord_h[i] = note_on(CHORD[ci][i], SL_ORG, 5);
    }
    chord_i = ci; retrig = 0.0f;
}

void update(void) {
    float dt = 1.0f / 60.0f;
    if (keyp('1')) play_chord(0);
    if (keyp('2')) play_chord(1);
    if (keyp('3')) play_chord(2);
    if (keyp('4')) play_chord(3);
    if (keyp('P')) sine = !sine;
    if (keyp('H')) show_help = !show_help;

    retrig += dt;
    if (retrig > 4.0f) play_chord((chord_i + 1) & 3);

    lamp_ph += rate_hz() * dt; if (lamp_ph >= 1.0f) lamp_ph -= 1.0f;

    // SET-AND-HOLD: re-push only when a knob or the mode actually moved
    if (k_rate != a_rate || k_depth != a_depth || k_mix != a_mix || sine != a_sine) {
        if (sine) phaser(rate_hz(), k_depth, 0.0f, k_mix, 4);   // sine sweep (no feedback → matches univibe)
        else      univibe(rate_hz(), k_depth, k_mix);            // optical sweep
        a_rate = k_rate; a_depth = k_depth; a_mix = k_mix; a_sine = sine;
    }
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    print("UNIVIBE", 8, 6, CLR_LIGHT_PEACH);
    print(sine ? "SINE (phaser)" : "OPTICAL (vibe)", SCREEN_W - 112, 6, sine ? CLR_DARK_GREY : CLR_PEACH);

    // the LAMP — brightness follows the live LFO (slow bright, fast dim when optical)
    float v = lamp_wave(lamp_ph);
    int cx = SCREEN_W / 2, cy = 70, R = 26;
    for (int r = R; r > 0; r--) {                       // a soft glow halo
        int a = (int)(v * 255 * r / R);
        if (a > 40) circ(cx, cy, r, v > 0.6f ? CLR_LIGHT_PEACH : v > 0.3f ? CLR_PEACH : CLR_DARK_PEACH);
    }
    circfill(cx, cy, 8, v > 0.5f ? CLR_WHITE : CLR_DARK_PEACH);

    // the LFO curve strip — draw one cycle of the current shape (asymmetric vs symmetric)
    int gx = 30, gy = 118, gw = SCREEN_W - 60, gh = 22;
    rect(gx, gy, gw, gh, CLR_DARK_PURPLE);
    int px = gx, py = gy + gh - 1;
    for (int i = 0; i <= gw; i++) {
        float ph = (float)i / gw;
        int yy = gy + gh - 1 - (int)(lamp_wave(ph) * (gh - 2));
        if (i) line(px, py, gx + i, yy, CLR_MAUVE);
        px = gx + i; py = yy;
    }
    int markx = gx + (int)(lamp_ph * gw);               // the live playhead on the curve
    line(markx, gy, markx, gy + gh, CLR_LIGHT_PEACH);
    print(sine ? "even sine sweep" : "slow bright, fast dim", gx, gy - 9, CLR_MEDIUM_GREY);

    char buf[32]; snprintf(buf, sizeof buf, "%s  %.1f Hz", CHORD_NAME[chord_i], rate_hz());
    print(buf, 8, SCREEN_H - 56, CLR_LIGHT_PEACH);

    ui_begin();
    int sy = SCREEN_H - 40, sw = 92;
    ui_slider(&k_rate,  8,            sy, sw, "RATE");
    ui_slider(&k_depth, 8 + sw + 10,  sy, sw, "DEPTH");
    ui_slider(&k_mix,   8 + (sw+10)*2, sy, sw, "MIX");
    ui_end();

    font(FONT_SMALL);
    print("P optical/sine   1-4 chords   H help", 8, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(24, 26, SCREEN_W - 48, 96, CLR_DARK_PURPLE);
        rect(24, 26, SCREEN_W - 48, 96, CLR_MAUVE);
        print("univibe(): 60s photocell vibe", 32, 34, CLR_WHITE);
        print("the phaser, but swept by an", 32, 48, CLR_LIGHT_GREY);
        print("OPTICAL LFO (a lightbulb):", 32, 58, CLR_LIGHT_GREY);
        print("slow bright -> fast dim.", 32, 68, CLR_PEACH);
        print("that asymmetry = the liquid", 32, 82, CLR_LIGHT_GREY);
        print("vibe a sine phaser can't do.", 32, 92, CLR_LIGHT_GREY);
        print("P to A/B it. H to close.", 32, 106, CLR_DARK_GREY);
    }
}

void init(void) {
    instrument(SL_ORG, INSTR_ORGAN, 80, 200, 6, 400);
    play_chord(0);
}
