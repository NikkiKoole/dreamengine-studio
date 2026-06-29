/* de:meta
{
  "title": "voxlab",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "instrument",
    "probe"
  ],
  "teaches": [],
  "description": "The FAT prototype for the formant VOICE engine (INSTR_VOICE — a navkit VoicForm port: a glottal pulse through four vowel formants). A probe: it exposes all SEVEN raw voice params on sliders (vowel U→I morph · size/vocal-tract length · breath · glottal open-quotient · spectral tilt · vibrato depth · vibrato rate) so you can AUDITION by ear which three deserve to become the public harmonics/timbre/morph macros. The four 'layout' buttons (gen / mouth / creat / sing) load candidate 3-axis combos — press one, sweep the sliders it leans on, judge whether that combination feels expressive. Drives the engine through voice_param(handle, idx, value), an EXPERIMENTAL by-index poke that retires once the axes are chosen. SPACE plays a sustained note · Z/X drop/raise pitch · drag the sliders."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// VOXLAB — the fat prototype for the formant VOICE engine (INSTR_VOICE, a navkit
// VoicForm port). AUDITION by ear: it exposes the raw voice params so you can feel
// which deserve to become real macros — including the experimental NASALITY slider
// and the 2D-VOWEL XY PAD (open/F1 on Y, front/F2 on X) we're trying out before
// committing the API. The preset buttons (gen / mouth / creat / sing) are candidate
// 3-axis LAYOUTS — press one, sweep what it leans on, judge if it's expressive.
//
// Throwaway scaffolding: note_aux(handle, idx, value) is EXPERIMENTAL, not the
// final API. Once we pick the axes they get a clean mapping and this cart retires.
//
// CONTROLS: SPACE / PLAY toggles a note · Z / X pitch · drag the sliders · drag the
// XY PAD to sweep the vowel plane (F1 x F2) · preset buttons load a candidate layout.

#define SLOT 5

enum { P_VOWEL, P_SIZE, P_BREATH, P_OPENQ, P_TILT, P_VIBD, P_VIBR,
       P_NASAL, P_OPEN, P_FRONT, P_N };

static const char *PNAME[P_N] =
    { "vowel", "size", "breath", "open-q", "tilt", "vib amt", "vib hz",
      "nasal", "open F1", "front F2" };

static float param[P_N] = { 0.5f, 0.33f, 0.10f, 0.50f, 0.30f, 0.15f, 0.50f,
                            0.0f, 0.5f, 0.5f };   // nasal off · open/front neutral (x1.0)
static int   voice = -1;
static float vpitch = 50;
static int   last_preset = 0;
static int   padgrab = 0;          // dragging the XY pad?

// the open F1 / front F2 sliders are REPLACED by the XY pad; sliders cover 0..P_NASAL
#define N_SLIDERS P_OPEN           // = 8 (vowel..nasal)

// XY pad rect (vowel plane: X = front/F2, Y = open/F1)
#define XPADX 168
#define XPADY 126
#define XPADW 144
#define XPADH 58

static const float PRESET[4][P_N] = {
    //  vowel  size  breath openq  tilt   vibd   vibr  nasal open  front
    { 0.50f, 0.33f, 0.10f, 0.50f, 0.30f, 0.15f, 0.50f, 0.0f, 0.5f, 0.5f }, // gen   — vowel / size / effort
    { 0.50f, 0.33f, 0.04f, 0.45f, 0.15f, 0.00f, 0.50f, 0.0f, 0.5f, 0.5f }, // mouth — clean vowel + size (two vowel axes)
    { 0.30f, 0.06f, 0.28f, 0.16f, 0.55f, 0.05f, 0.30f, 0.0f, 0.5f, 0.5f }, // creat — vowel / size(giant) / roughness
    { 0.55f, 0.42f, 0.12f, 0.58f, 0.22f, 0.62f, 0.66f, 0.0f, 0.5f, 0.5f }, // sing  — vowel / effort / vibrato
};
static const char *PRESET_NAME[4] = { "gen", "mouth", "creat", "sing" };

static const char *VOWELS[5] = { "U oo", "O oh", "A ah", "E eh", "I ee" };

static void load_preset(int p) {
    for (int i = 0; i < P_N; i++) param[i] = PRESET[p][i];
    last_preset = p;
}

static void start_voice(void) {
    voice = note_on((int)(vpitch + 0.5f), SLOT, 6);
    note_glide(voice, 30);
}

void init(void) {
    instrument(SLOT, INSTR_VOICE, 45, 60, 7, 160);
}

void update(void) {
    if (keyp(KEY_SPACE)) {
        if (voice < 0) start_voice();
        else { note_off(voice); voice = -1; }
    }
    if (keyp('Z')) vpitch = clamp(vpitch - 1, 24, 84);
    if (keyp('X')) vpitch = clamp(vpitch + 1, 24, 84);

    // XY pad: drag to sweep F2 (X) and F1 (Y, up = more open). grab only if the press
    // started inside the pad (so slider drags never get hijacked).
    int mx = mouse_x(), my = mouse_y();
    int over = mx >= XPADX && mx < XPADX + XPADW && my >= XPADY && my < XPADY + XPADH;
    if (mouse_pressed(MOUSE_LEFT) && over) padgrab = 1;
    if (!mouse_down(MOUSE_LEFT)) padgrab = 0;
    if (padgrab) {
        param[P_FRONT] = clamp((float)(mx - XPADX) / XPADW, 0, 1);
        param[P_OPEN]  = clamp(1.0f - (float)(my - XPADY) / XPADH, 0, 1);
    }

    if (voice >= 0) {
        note_pitch(voice, vpitch);
        for (int i = 0; i < P_N; i++) note_aux(voice, i, param[i]);
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("VOXLAB", 6, 4, CLR_PEACH);
    print("INSTR_VOICE prototype", 60, 4, CLR_MEDIUM_GREY);

    ui_begin();

    // ---- raw param sliders (vowel..nasal) ----
    for (int i = 0; i < N_SLIDERS; i++) {
        int y = 16 + i * 18;
        ui_slider(&param[i], 6, y, 126, PNAME[i]);
        char buf[8]; int v = (int)(param[i] * 100 + 0.5f);
        buf[0] = '0' + (v / 100) % 10; buf[1] = '0' + (v / 10) % 10;
        buf[2] = '0' + v % 10; buf[3] = 0;
        print(v >= 100 ? "MAX" : buf + (v < 10 ? 2 : 1), 136, y + 1, CLR_LIGHT_GREY);
    }

    // ---- transport + presets (right column) ----
    int rx = 168;
    if (ui_button(rx, 16, 84, 16, voice >= 0 ? "STOP" : "PLAY")) {
        if (voice < 0) start_voice();
        else { note_off(voice); voice = -1; }
    }
    if (ui_button(rx, 36, 20, 14, "-")) vpitch = clamp(vpitch - 1, 24, 84);
    if (ui_button(rx + 64, 36, 20, 14, "+")) vpitch = clamp(vpitch + 1, 24, 84);
    char pb[5]; int m = (int)(vpitch + 0.5f);
    pb[0] = 'm'; pb[1] = '0' + (m / 10) % 10; pb[2] = '0' + m % 10; pb[3] = 0;
    print_centered(pb, rx + 42, 39, CLR_YELLOW);
    print("layouts:", rx, 54, CLR_MEDIUM_GREY);
    for (int p = 0; p < 4; p++) {
        int bx = rx + (p % 2) * 44, by = 64 + (p / 2) * 18;
        if (ui_button(bx, by, 40, 16, PRESET_NAME[p])) load_preset(p);
    }

    ui_end();

    // ---- the 2D vowel-plane XY pad (X = front/F2, Y = open/F1) ----
    print("vowel plane  F1 v  F2 >", rx, 116, CLR_LIGHT_PEACH);
    rectfill(XPADX, XPADY, XPADW, XPADH, CLR_DARKER_GREY);
    line(XPADX + XPADW / 2, XPADY, XPADX + XPADW / 2, XPADY + XPADH, CLR_DARK_GREY);  // neutral cross
    line(XPADX, XPADY + XPADH / 2, XPADX + XPADW, XPADY + XPADH / 2, CLR_DARK_GREY);
    rect(XPADX, XPADY, XPADW, XPADH, CLR_DARK_GREY);
    int mkx = XPADX + (int)(param[P_FRONT] * XPADW);
    int mky = XPADY + (int)((1.0f - param[P_OPEN]) * XPADH);
    mkx = (int)clamp(mkx, XPADX + 2, XPADX + XPADW - 2);
    mky = (int)clamp(mky, XPADY + 2, XPADY + XPADH - 2);
    circfill(mkx, mky, 4, CLR_YELLOW);
    circfill(mkx, mky, 2, CLR_WHITE);

    // ---- readouts (bottom-left) ----
    float vw = clamp(param[P_VOWEL], 0, 1) * 4.0f;
    int vi = (int)(vw + 0.5f); if (vi > 4) vi = 4;
    print("vowel:", 6, 162, CLR_MEDIUM_GREY); print(VOWELS[vi], 56, 162, CLR_WHITE);
    print("F1", 6, 176, CLR_MEDIUM_GREY);
    print((int)(param[P_OPEN] * 100 + 0.5f) >= 50 ? "open" : "closed", 28, 176, CLR_LIGHT_GREY);
    print("F2", 86, 176, CLR_MEDIUM_GREY);
    print((int)(param[P_FRONT] * 100 + 0.5f) >= 50 ? "front" : "back", 108, 176, CLR_LIGHT_GREY);

    print("SPACE play  Z/X pitch  drag sliders + XY pad", 6, 190, CLR_DARK_GREY);
}
