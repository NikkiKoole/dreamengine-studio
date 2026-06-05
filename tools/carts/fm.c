// fm — INSTR_FM showcase: two operators, three macros, one scope.
//
// The third modeled ENGINE: a carrier sine you hear, and an inaudible modulator sine
// that bends the carrier's phase — the DX-synth recipe. Brightness DECAYS within each
// note (bright strike → mellow tail), which is what separates a DX epiano from a cheap
// organ. Every engine answers the same three 0..1 macro knobs — the API never grows:
//   harmonics = carrier:mod RATIO (snapped detents — integers musical, offs = bells, 14 = tine)
//   timbre    = brightness        (FM amount; decays per note like a real DX strike)
//   morph     = feedback          (0 clean .. saw-edge growl .. noisy clang)
//
// The named presets are nothing but KNOB POSITIONS (audio-notes §8.1) — the tuning rig
// contract: if pressing 1 doesn't sound like an electric piano, the MAPPING is wrong.
// (brass is the deliberate stress test: classic FM brass wants the index to RISE on
// attack, our bake decays it — §8.8.3's open question, answered by ear here.)
//
// The scope draws the engine's actual formula, including the per-note brightness decay —
// strike a key and WATCH the wave mellow as it rings.
//
// controls: A S D F G H J K  play (major scale)   ·   1..5 presets:
//           epiano / bell / bass / brass / clang
//           drag a slider with the mouse (auditions as you drag), or
//           LEFT/RIGHT pick a knob + UP/DOWN turn it
//           SPACE chord   ·   M autoplay on/off

#include "studio.h"
#include <math.h>

#define I_FM  5
#define NKEY  8

static const char STRKEY[NKEY] = { 'A','S','D','F','G','H','J','K' };
static const char *KNOB_NAME[3] = { "harmonics (ratio)", "timbre (bright)", "morph (feedback)" };
static const char *KNOB_LO[3]   = { "sub",   "mellow", "clean" };
static const char *KNOB_HI[3]   = { "tine",  "bright", "clang" };

// MUST match the engine's snapped table (sound.h sound_fm_sample) — display only
static const float RATIO[10] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 3.5f, 4.0f, 5.0f, 7.0f, 14.0f };

// the presets: knob positions with a hardware name. STARTING GUESSES — tune by ear here.
typedef struct { const char *name; float h, t, m; } Preset;
static const Preset PRESET[5] = {
    { "epiano", 0.15f, 0.45f, 0.10f },
    { "bell",   0.55f, 0.60f, 0.15f },
    { "bass",   0.00f, 0.75f, 0.30f },
    { "brass",  0.15f, 0.90f, 0.50f },   // the stress test (see header)
    { "clang",  0.95f, 0.80f, 0.90f },
};

static int   midi_of[NKEY];
static float glow[NKEY];
static float knob[3] = { 0.15f, 0.45f, 0.1f };
static int   sel = 0;
static int   drag_k = -1;
static bool  autoplay = true;
static int   cur_preset = 0;
static int   apos = 0;
static int   scope_age = 9999;   // frames since the last strike — drives the scope's decay

// key + slider geometry
#define KEY_W    34
#define KEY_X(b) (10 + (b) * (KEY_W + 4))
#define KEY_Y    102
#define KEY_H    30
#define KNOB_W   88
#define KNOB_Y   (SCREEN_H - 32)
#define KNOB_X(k) (14 + (k) * 102)

static void apply_knobs(void) {
    instrument_harmonics(I_FM, knob[0]);
    instrument_timbre(I_FM, knob[1]);
    instrument_morph(I_FM, knob[2]);
}

static void play_key(int b, int vol) {
    note(midi_of[b], I_FM, vol);
    glow[b] = 1.0f;
    scope_age = 0;
}

static void play_chord(void) {
    chord(midi_of[0], CHORD_MAJ7, I_FM, 5);
    glow[0] = glow[2] = glow[4] = 1.0f;
    scope_age = 0;
}

static void set_preset(int p) {
    knob[0] = PRESET[p].h; knob[1] = PRESET[p].t; knob[2] = PRESET[p].m;
    cur_preset = p;
    apply_knobs();
    play_key(2, 6);
}

void init(void) {
    // the engine doesn't decay on its own (unlike PLUCK/MALLET) — a normal ADSR shapes it.
    // one fixed piano-ish envelope for every preset; brass strains against it on purpose.
    instrument(I_FM, INSTR_FM, 2, 700, 3, 350);
    apply_knobs();
    for (int b = 0; b < NKEY; b++) midi_of[b] = degree(SCALE_MAJOR, 4, b);
    bpm(96);
}

void update(void) {
    for (int b = 0; b < NKEY; b++)
        if (keyp(STRKEY[b])) play_key(b, 6);
    for (int p = 0; p < 5; p++)
        if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + 2) % 3;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 3;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_knobs();
        if (frame() % 12 == 0) play_key(2, 5);
    }

    if (keyp(KEY_SPACE)) play_chord();
    if (keyp('M')) autoplay = !autoplay;

    if (mouse_pressed(MOUSE_LEFT)) {
        for (int k = 0; k < 3; k++)
            if (point_in_box(mouse_x(), mouse_y(), KNOB_X(k) - 2, KNOB_Y - 6, KNOB_W + 4, 18))
                drag_k = sel = k;
        if (drag_k < 0)
            for (int b = 0; b < NKEY; b++)
                if (point_in_box(mouse_x(), mouse_y(), KEY_X(b), KEY_Y, KEY_W, KEY_H))
                    play_key(b, 6);
    }
    if (drag_k >= 0) {
        if (mouse_down(MOUSE_LEFT)) {
            knob[drag_k] = clamp((float)(mouse_x() - KNOB_X(drag_k)) / (float)KNOB_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_knobs();
            if (frame() % 12 == 0) play_key(2, 5);
        } else drag_k = -1;
    }

    // autoplay: epiano comping — a maj7 chord at the top, a noodle between
    if (autoplay && every(1)) {
        static const int seq[16] = { 2,4,5,7, 4,2,0,4, 5,7,6,4, 2,0,1,2 };
        if (beat() % 8 == 0) play_chord();
        else {
            play_key(seq[apos % 16] % NKEY, 4);
            if (chance(25)) schedule_hit(280, midi_of[(seq[apos % 16] + 2) % NKEY], I_FM, 3, 250);
        }
        apos++;
    }
    scope_age++;

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("preset", "%d", cur_preset);
#endif
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("FM", 8, 6, CLR_BLUE);
    font(FONT_SMALL);
    print("two-operator fm engine", 32, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // the engine's parameters, derived exactly like sound_fm_sample does
    int   ri    = (int)(knob[0] * 9.999f); if (ri > 9) ri = 9;
    float ratio = RATIO[ri];
    float settle = 0.25f + 0.75f * expf(-(float)scope_age / 54.0f);   // ~0.9s at 60fps
    float beta  = knob[1] * knob[1] * 12.0f * settle;

    // the scope: the actual FM formula, two carrier cycles, feedback iterated
    int sy = 58, sx0 = 10, sx1 = SCREEN_W - 10, prev_y = sy;
    rectfill(sx0 - 2, 24, sx1 - sx0 + 4, 70, CLR_DARK_BLUE);
    line(sx0, sy, sx1, sy, CLR_DARKER_GREY);
    float fb_s = 0.0f;
    for (int x = sx0; x <= sx1; x++) {
        float t = (float)(x - sx0) / (float)(sx1 - sx0) * 2.0f;       // 2 carrier cycles
        float m = sinf(t * ratio * 6.2831853f + knob[2] * 1.3f * fb_s * 3.14159265f);
        fb_s = m;
        int y = sy - (int)(sinf(t * 6.2831853f + m * beta) * 28.0f);
        if (x > sx0) line(x - 1, prev_y, x, y, CLR_BLUE);
        prev_y = y;
    }
    font(FONT_TINY);
    print(str("ratio 1:%.1f   brightness %.1f   feedback %.2f", ratio, beta, knob[2] * 1.3f),
          sx0, 26, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the keys
    for (int b = 0; b < NKEY; b++) {
        int x = KEY_X(b);
        glow[b] *= 0.90f;
        int col = glow[b] > 0.5f ? CLR_LIGHT_YELLOW : glow[b] > 0.1f ? CLR_BLUE : CLR_DARK_BLUE;
        rectfill(x, KEY_Y, KEY_W, KEY_H, col);
        rect(x, KEY_Y, KEY_W, KEY_H, glow[b] > 0.1f ? CLR_WHITE : CLR_DARK_GREY);
        print(str("%c", STRKEY[b]), x + KEY_W / 2 - 3, KEY_Y + KEY_H - 11,
              glow[b] > 0.5f ? CLR_DARKER_BLUE : CLR_MEDIUM_GREY);
    }

    // preset row
    font(FONT_SMALL);
    for (int p = 0; p < 5; p++) {
        int x = 14 + p * 58;
        bool on = (p == cur_preset);
        print(str("%d %s", p + 1, PRESET[p].name), x, KNOB_Y - 24, on ? CLR_YELLOW : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);

    // the three macro knobs
    for (int k = 0; k < 3; k++) {
        int x = KNOB_X(k), y = KNOB_Y;
        bool on = (k == sel);
        font(FONT_SMALL);
        print(KNOB_NAME[k], x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 7, knob[k], on ? CLR_ORANGE : CLR_TRUE_BLUE, CLR_DARK_BLUE);
        font(FONT_TINY);
        print(KNOB_LO[k], x, y + 9, CLR_DARK_GREY);
        print_right(KNOB_HI[k], x + KNOB_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    print("A..K play   1..5 presets   SPACE chord   watch the wave mellow after a strike", 10, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
