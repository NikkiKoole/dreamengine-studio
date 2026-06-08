#include "studio.h"
#include "ui.h"
#include <math.h>

// VOXLAB — the fat prototype for the formant VOICE engine (INSTR_VOICE, a navkit
// VoicForm port). The whole point of this cart is to AUDITION by ear: it exposes
// all SEVEN raw voice params on sliders so you can feel which three deserve to
// become the real harmonics/timbre/morph macros. The macro-preset buttons (gen /
// mouth / creat / sing) are candidate 3-axis LAYOUTS — press one, then sweep the
// few sliders it leans on and judge whether that combination is expressive.
//
// This is throwaway scaffolding: voice_param(handle, idx, value) is an EXPERIMENTAL
// by-index poke, not the final API. Once we pick the axes, they get a clean public
// mapping and this cart retires.
//
// CONTROLS: SPACE / PLAY toggles a sustained note · Z / X drop / raise the pitch ·
// drag the sliders · the four preset buttons jump to a candidate macro layout.

#define SLOT 5

enum { P_VOWEL, P_SIZE, P_BREATH, P_OPENQ, P_TILT, P_VIBD, P_VIBR, P_N };

static const char *PNAME[P_N] =
    { "vowel", "size", "breath", "open-q", "tilt", "vib amt", "vib hz" };

static float param[P_N] = { 0.5f, 0.33f, 0.10f, 0.50f, 0.30f, 0.15f, 0.50f };
static int   voice = -1;          // held-note handle, -1 = silent
static float vpitch = 50;         // MIDI (D3 — a comfy vocal register)
static int   last_preset = 0;     // which layout is loaded (for the readout)

// candidate 3-axis LAYOUTS — each is a full param set; the comment names the axes
// you'd actually expose if this layout wins.
static const float PRESET[4][P_N] = {
    //  vowel  size  breath openq  tilt   vibd   vibr
    { 0.50f, 0.33f, 0.10f, 0.50f, 0.30f, 0.15f, 0.50f }, // gen   — vowel / size / effort
    { 0.50f, 0.33f, 0.04f, 0.45f, 0.15f, 0.00f, 0.50f }, // mouth — clean vowel + size (two vowel axes)
    { 0.30f, 0.06f, 0.28f, 0.16f, 0.55f, 0.05f, 0.30f }, // creat — vowel / size(giant) / roughness
    { 0.55f, 0.42f, 0.12f, 0.58f, 0.22f, 0.62f, 0.66f }, // sing  — vowel / effort / vibrato
};
static const char *PRESET_NAME[4] = { "gen", "mouth", "creat", "sing" };
static const char *PRESET_AXES[4] = {
    "vowel / size / effort",
    "vowel x2 (mouth) / size",
    "vowel / size(giant) / growl",
    "vowel / effort / vibrato",
};

// the 5-vowel path the engine morphs along (U->O->A->E->I)
static const char *VOWELS[5] = { "U oo", "O oh", "A ah", "E eh", "I ee" };

static void load_preset(int p) {
    for (int i = 0; i < P_N; i++) param[i] = PRESET[p][i];
    last_preset = p;
}

static void start_voice(void) {
    voice = note_on((int)(vpitch + 0.5f), SLOT, 6);
    note_glide(voice, 30);   // slur between pitches instead of retriggering
}

void init(void) {
    // soft vocal envelope: gentle onset, full sustain, a little release tail
    instrument(SLOT, INSTR_VOICE, 45, 60, 7, 160);
}

void update(void) {
    if (keyp(KEY_SPACE)) {
        if (voice < 0) start_voice();
        else { note_off(voice); voice = -1; }
    }
    if (keyp('Z')) vpitch = clamp(vpitch - 1, 24, 84);
    if (keyp('X')) vpitch = clamp(vpitch + 1, 24, 84);

    // keep the held voice steered every frame
    if (voice >= 0) {
        note_pitch(voice, vpitch);
        for (int i = 0; i < P_N; i++) voice_param(voice, i, param[i]);
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("VOXLAB", 6, 4, CLR_PEACH);
    print("INSTR_VOICE prototype", 60, 4, CLR_MEDIUM_GREY);

    ui_begin();

    // ---- the seven raw param sliders (left column) ----
    for (int i = 0; i < P_N; i++) {
        int y = 20 + i * 21;
        ui_slider(&param[i], 6, y, 132, PNAME[i]);
        // numeric readout to the right of each bar
        char buf[8]; int v = (int)(param[i] * 100 + 0.5f);
        buf[0] = '0' + (v / 100) % 10; buf[1] = '0' + (v / 10) % 10;
        buf[2] = '0' + v % 10; buf[3] = 0;
        print(v >= 100 ? "MAX" : buf + (v < 10 ? 2 : 1), 142, y + 2, CLR_LIGHT_GREY);
    }

    // ---- right column: transport + presets ----
    int rx = 176;
    if (ui_button(rx, 20, 70, 18, voice >= 0 ? "STOP" : "PLAY")) {
        if (voice < 0) start_voice();
        else { note_off(voice); voice = -1; }
    }
    // pitch nudge
    if (ui_button(rx, 42, 20, 16, "-")) vpitch = clamp(vpitch - 1, 24, 84);
    if (ui_button(rx + 50, 42, 20, 16, "+")) vpitch = clamp(vpitch + 1, 24, 84);
    char pb[5]; int m = (int)(vpitch + 0.5f);
    pb[0] = 'm'; pb[1] = '0' + (m / 10) % 10; pb[2] = '0' + m % 10; pb[3] = 0;
    print_centered(pb, rx + 35, 46, CLR_YELLOW);

    // candidate macro layouts (2x2)
    print("layouts:", rx, 66, CLR_MEDIUM_GREY);
    for (int p = 0; p < 4; p++) {
        int bx = rx + (p % 2) * 50;
        int by = 76 + (p / 2) * 20;
        if (ui_button(bx, by, 46, 16, PRESET_NAME[p])) load_preset(p);
    }
    // name the axes of the loaded layout
    print(PRESET_AXES[last_preset], rx, 120, CLR_LIGHT_PEACH);

    ui_end();

    // ---- vowel readout + a little singing mouth ----
    float vw = clamp(param[P_VOWEL], 0, 1) * 4.0f;
    int vi = (int)(vw + 0.5f); if (vi > 4) vi = 4;
    print("vowel:", rx, 138, CLR_MEDIUM_GREY);
    print(VOWELS[vi], rx + 52, 138, CLR_WHITE);

    // mouth: openness ~ how open the vowel is (A widest), size scales the whole head
    int cx = rx + 60, cy = 176;
    float openf = 1.0f - fabsf(param[P_VOWEL] - 0.5f) * 1.6f;  // A (mid) = most open
    if (openf < 0.1f) openf = 0.1f;
    float scale = 0.7f + (1.0f - param[P_SIZE]) * 0.7f;        // small size value = big body
    int rxo = (int)(16 * scale), ryo = (int)((4 + openf * 12) * scale);
    ovalfill(cx, cy, rxo, ryo, voice >= 0 ? CLR_DARK_RED : CLR_DARKER_GREY);
    if (voice >= 0 && openf > 0.4f) ovalfill(cx, cy + ryo / 3, rxo / 3, ryo / 3, CLR_PINK);

    // footer
    print("SPACE play   Z/X pitch   drag sliders", 6, 190, CLR_DARK_GREY);
}
