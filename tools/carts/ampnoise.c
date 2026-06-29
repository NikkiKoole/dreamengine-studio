/* de:meta
{
  "title": "amp noise",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Boutique-pedals roadmap; models tube-amp hiss + 50/60 Hz mains hum as an opt-in rig noise floor, paired with a visual 12AX7 tube-amp face.",
  "description": "The showcase for amp_noise() - the OPTIONAL rig-noise floor. An electric guitar through a tube amp is never truly silent: a bed of HISS (tube/thermal noise) plus a 50/60 Hz mains HUM (the single-coil buzz) sits under everything, even with no note playing. amp_noise() models that floor - but it is entirely opt-in (a fantasy console is pristine by default), so the headline is the A/B: press N (or SPACE) to toggle the FLOOR on/off and hear the rig breathe versus dead-silent. The HISS and HUM sliders dial the floor (0,0 = off); M switches mains 50 Hz (EU) / 60 Hz (US); 1..4 play chords that sit ON the floor. A tube-amp face glows its 12AX7 power tube and dusts hiss speckle over the grille while the floor is on. Some tracks want this grime, some want the silence - that is the point, it is a choice not a default. Built on the boutique-pedals roadmap; pairs with a noise gate (next) to clamp the floor between notes."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── AMP NOISE ───────────────────────────────────────────────────────────────
// The showcase for amp_noise() — the OPTIONAL rig-noise floor. An electric guitar
// through a tube amp is never truly silent: a bed of HISS + a 50/60 Hz mains HUM
// sits under everything. amp_noise() models that floor — but it's entirely opt-in
// (a fantasy console is pristine by default), so the headline here is the A/B:
//
//   N / SPACE   toggle the FLOOR on/off — hear the rig "breathe" vs dead-silent
//   HISS / HUM  sliders dial the floor (0,0 = off)
//   M           mains 50 Hz (EU) ↔ 60 Hz (US)
//   1 2 3 4     play chords (the music sits ON the floor)   H help
//
// Some tracks want this grime; some want the silence. That's the point — it's a
// choice, not a default.

#define SL_GTR  8

static float k_hiss = 0.40f;
static float k_hum  = 0.30f;
static int   mains  = 60;
static bool  floor_on = true;
static bool  show_help = false;

static int   chord_h[4] = { -1, -1, -1, -1 };
static int   chord_i = 0;
static float retrig  = 0.0f;
static float hum_ph  = 0.0f;   // local mains phase for the swaying VU needle

static float a_hiss = -1, a_hum = -1; static int a_mains = -1, a_on = -1;

static const int CHORD[4][4] = {
    { 52, 55, 59, 64 },   // Em
    { 48, 55, 60, 64 },   // C
    { 50, 57, 62, 65 },   // D
    { 47, 55, 59, 62 },   // G/B-ish
};
static const char *CHORD_NAME[4] = { "Em", "C", "D", "G" };

static void play_chord(int ci) {
    for (int i = 0; i < 4; i++) {
        if (chord_h[i] >= 0) note_off(chord_h[i]);
        chord_h[i] = note_on(CHORD[ci][i], SL_GTR, 4);
    }
    chord_i = ci; retrig = 0.0f;
}

void update(void) {
    float dt = 1.0f / 60.0f;
    if (keyp('1')) play_chord(0);
    if (keyp('2')) play_chord(1);
    if (keyp('3')) play_chord(2);
    if (keyp('4')) play_chord(3);
    if (keyp('N') || keyp(KEY_SPACE)) floor_on = !floor_on;
    if (keyp('M')) mains = (mains == 60) ? 50 : 60;
    if (keyp('H')) show_help = !show_help;

    retrig += dt;
    if (retrig > 3.0f) play_chord((chord_i + 1) & 3);

    hum_ph += (mains == 60 ? 4.0f : 3.3f) * dt; if (hum_ph >= 1.0f) hum_ph -= 1.0f;   // visual sway (scaled down)

    // SET-AND-HOLD: re-push only when a knob / the toggle moved
    int on = floor_on ? 1 : 0;
    if (k_hiss != a_hiss || k_hum != a_hum || mains != a_mains || on != a_on) {
        if (floor_on) amp_noise(k_hiss, k_hum, mains);
        else          amp_noise(0.0f, 0.0f, mains);     // OFF → true silence
        a_hiss = k_hiss; a_hum = k_hum; a_mains = mains; a_on = on;
    }
}

void draw(void) {
    cls(CLR_DARK_BROWN);
    print("AMP NOISE", 8, 6, CLR_LIGHT_PEACH);
    print_right(floor_on ? "FLOOR: ON" : "FLOOR: OFF", SCREEN_W - 8, 6, floor_on ? CLR_ORANGE : CLR_DARK_GREY);

    // the amp face: a grille + a glowing power tube that flickers with the (visualised) floor
    int gx = 16, gy = 22, gw = SCREEN_W - 32, gh = 64;
    rrectfill(gx, gy, gw, gh, 5, CLR_BROWNISH_BLACK);
    rrect(gx, gy, gw, gh, 5, CLR_DARK_PEACH);
    for (int yy = gy + 5; yy < gy + gh - 5; yy += 3)         // speaker grille cloth
        line(gx + 6, yy, gx + gw - 90, yy, CLR_DARKER_GREY);
    // hiss speckle over the grille (only when on) — a dusting that tracks HISS
    if (floor_on) {
        unsigned int s = (unsigned int)(now() * 997.0f);
        int dots = (int)(k_hiss * 60.0f);
        for (int i = 0; i < dots; i++) {
            s = s * 1103515245u + 12345u;
            int px = gx + 6 + (int)((s >> 16) % (gw - 96));
            s = s * 1103515245u + 12345u;
            int py = gy + 5 + (int)((s >> 16) % (gh - 10));
            pset(px, py, CLR_MEDIUM_GREY);
        }
    }
    // the power tube — glows; flickers with HUM when the floor is on
    int tx = gx + gw - 44, ty = gy + gh / 2;
    float glow = floor_on ? 0.55f + 0.45f * sinf(hum_ph * 6.2831853f) * k_hum : 0.0f;
    for (int r = 18; r > 0; r -= 3) circ(tx, ty, r, glow > 0.5f ? CLR_ORANGE : glow > 0.15f ? CLR_DARK_PEACH : CLR_BROWNISH_BLACK);
    circfill(tx, ty, 5, floor_on ? (glow > 0.5f ? CLR_LIGHT_PEACH : CLR_DARK_PEACH) : CLR_DARKER_GREY);
    font(FONT_TINY); print_centered("12AX7", tx, ty + 20, CLR_DARK_GREY); font(FONT_NORMAL);

    char buf[40]; snprintf(buf, sizeof buf, "%s   mains %d Hz", CHORD_NAME[chord_i], mains);
    print(buf, 8, SCREEN_H - 54, CLR_LIGHT_PEACH);

    ui_begin();
    int sy = SCREEN_H - 40, sw = 120;
    ui_slider(&k_hiss, 8,           sy, sw, "HISS");
    ui_slider(&k_hum,  8 + sw + 12, sy, sw, "HUM");
    ui_end();

    font(FONT_SMALL);
    print("N floor on/off   M 50/60Hz   1-4 chords   H help", 8, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(22, 24, SCREEN_W - 44, 96, CLR_BROWNISH_BLACK);
        rect(22, 24, SCREEN_W - 44, 96, CLR_ORANGE);
        print("amp_noise(): the OPTIONAL floor", 30, 32, CLR_WHITE);
        print("a tube amp hisses + hums even", 30, 46, CLR_LIGHT_PEACH);
        print("with no note playing. great for", 30, 56, CLR_LIGHT_PEACH);
        print("lo-fi life -- or turn it OFF for", 30, 66, CLR_LIGHT_PEACH);
        print("a pristine, dead-silent console.", 30, 76, CLR_LIGHT_PEACH);
        print("N toggles it. HISS+HUM dial it.", 30, 90, CLR_ORANGE);
        print("H to close", 30, 104, CLR_DARK_GREY);
    }
}

void init(void) {
    instrument(SL_GTR, INSTR_GUITAR, 2, 0, 5, 500);
    play_chord(0);
}
