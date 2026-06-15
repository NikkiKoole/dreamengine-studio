#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── SHIMMER ───────────────────────────────────────────────────────────────────
// The showcase for shimmer() — a reverb with an OCTAVE-UP pitch-shifter wired into
// its feedback loop. Each pass through the reverb, the wet tail is tapped, pitched
// up an octave, and fed back in — so a held chord doesn't just bloom, it CLIMBS,
// rising into a glassy, crystalline pad (the Strymon BlueSky / Brian Eno sound).
//
//   A held chord feeds the shimmer the whole time.
//   1 2 3 4   change chord (Cmaj / Am / Fmaj / G)
//   SHIMMER   the climb — low = a plain reverb bloom, high = an endless ascent
//   SIZE/DAMP/MIX  decay length / tail darkness / dry-wet
//   H help
//
// The rising sparks ARE the octave-up tail: more SHIMMER = more of them, climbing
// faster and brighter as the energy ascends through the octaves.

#define SL_PAD  8

static float k_size = 0.80f;
static float k_damp = 0.40f;
static float k_amt  = 0.65f;   // shimmer amount (the climb)
static float k_mix  = 0.55f;
static bool  show_help = false;

static int   pad_h[4] = { -1, -1, -1, -1 };
static int   chord_i = 0;
static float retrig = 0.0f;

static float a_size = -1, a_damp = -1, a_amt = -1, a_mix = -1;

static const int CHORD[4][4] = {
    { 48, 55, 60, 64 },   // Cmaj
    { 45, 52, 57, 60 },   // Am
    { 41, 53, 57, 60 },   // Fmaj
    { 43, 50, 55, 59 },   // G
};
static const char *CHORD_NAME[4] = { "Cmaj", "Am", "Fmaj", "G" };

// rising sparks — the ascending octave tail, visualised
#define NSPARK 56
typedef struct { float x, y, vy, life, age; bool on; } Spark;
static Spark spark[NSPARK];
static float spawn_acc = 0.0f;

static void play_chord(int ci) {
    for (int i = 0; i < 4; i++) {
        if (pad_h[i] >= 0) note_off(pad_h[i]);
        pad_h[i] = note_on(CHORD[ci][i], SL_PAD, 5);
    }
    chord_i = ci; retrig = 0.0f;
}

void update(void) {
    float dt = 1.0f / 60.0f;
    if (keyp('1')) play_chord(0);
    if (keyp('2')) play_chord(1);
    if (keyp('3')) play_chord(2);
    if (keyp('4')) play_chord(3);
    if (keyp('H')) show_help = !show_help;

    retrig += dt;
    if (retrig > 5.0f) play_chord((chord_i + 1) & 3);   // hold each chord long enough to climb

    if (k_size != a_size || k_damp != a_damp || k_amt != a_amt || k_mix != a_mix) {
        shimmer(k_size, k_damp, k_amt, k_mix);
        a_size = k_size; a_damp = k_damp; a_amt = k_amt; a_mix = k_mix;
    }

    // rising sparks: spawn rate + speed track the shimmer amount
    spawn_acc += (2.0f + k_amt * 28.0f) * dt;
    while (spawn_acc >= 1.0f) {
        spawn_acc -= 1.0f;
        for (int i = 0; i < NSPARK; i++) if (!spark[i].on) {
            unsigned int s = (unsigned int)(now() * 9301.0f) + i * 49297u;
            spark[i].x = 16 + (s >> 8) % (SCREEN_W - 32);
            spark[i].y = SCREEN_H - 52;
            spark[i].vy = -(14.0f + k_amt * 46.0f);     // faster climb at higher shimmer
            spark[i].life = 1.2f + k_size * 2.5f;
            spark[i].age = 0.0f; spark[i].on = true;
            break;
        }
    }
    for (int i = 0; i < NSPARK; i++) if (spark[i].on) {
        spark[i].age += dt;
        spark[i].y += spark[i].vy * dt;
        spark[i].vy *= 0.992f;                            // ease as it rises
        if (spark[i].age >= spark[i].life || spark[i].y < 14) spark[i].on = false;
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("SHIMMER", 8, 6, CLR_LIGHT_PEACH);

    // the rising sparks — warm at the bottom, bright/white as they climb (ascending octaves)
    for (int i = 0; i < NSPARK; i++) if (spark[i].on) {
        float t = spark[i].y / (float)SCREEN_H;          // 1 at bottom → 0 at top
        int c = t < 0.35f ? CLR_WHITE : t < 0.6f ? CLR_LIGHT_PEACH : t < 0.8f ? CLR_PEACH : CLR_INDIGO;
        float fade = 1.0f - spark[i].age / spark[i].life;
        circfill((int)spark[i].x, (int)spark[i].y, fade > 0.5f ? 1 : 0, c);
        if (fade > 0.7f) pset((int)spark[i].x, (int)spark[i].y - 1, c);
    }

    char buf[40]; snprintf(buf, sizeof buf, "chord: %s", CHORD_NAME[chord_i]);
    print(buf, 8, SCREEN_H - 58, CLR_MEDIUM_GREY);

    ui_begin();
    int sy = SCREEN_H - 40, sw = 70;
    ui_slider(&k_size, 8,            sy, sw, "SIZE");
    ui_slider(&k_damp, 8 + (sw+6),   sy, sw, "DAMP");
    ui_slider(&k_amt,  8 + (sw+6)*2, sy, sw, "SHIMMER");
    ui_slider(&k_mix,  8 + (sw+6)*3, sy, sw, "MIX");
    ui_end();

    font(FONT_SMALL);
    print("1-4 chords   SHIMMER = the climb   H help", 8, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(22, 24, SCREEN_W - 44, 96, CLR_DARK_BLUE);
        rect(22, 24, SCREEN_W - 44, 96, CLR_INDIGO);
        print("shimmer(): octave-up reverb", 30, 32, CLR_WHITE);
        print("the wet tail is pitched UP an", 30, 46, CLR_LIGHT_PEACH);
        print("octave and fed back in, so a", 30, 56, CLR_LIGHT_PEACH);
        print("held chord CLIMBS into a glassy", 30, 66, CLR_LIGHT_PEACH);
        print("crystalline pad (Eno/Strymon).", 30, 76, CLR_LIGHT_PEACH);
        print("SHIMMER low=bloom  high=ascend", 30, 90, CLR_INDIGO);
        print("H to close", 30, 104, CLR_DARK_GREY);
    }
}

void init(void) {
    instrument(SL_PAD, INSTR_ORGAN, 120, 0, 7, 600);
    play_chord(0);
}
