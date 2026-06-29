/* de:meta
{
  "title": "vox",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "instrument",
    "probe"
  ],
  "teaches": [
    "analog-voice-modeling",
    "generative-melody"
  ],
  "lineage": "Playable instrument built on the INSTR_VOICE formant engine; distills voxlab's parameter space to three macro axes (vowel/size/effort) and adds a 2D jam-pad that auto-generates random consonant+vowel syllables while singing pentatonic pitches.",
  "description": "The formant VOICE engine (INSTR_VOICE — navkit VoicForm port) on the THREE axes the voxlab probe landed on (VOWEL = U→O→A→E→I morph · SIZE = formant shift / vocal-tract length, giant→child · EFFORT = one macro of breath + glottal open-quotient + spectral tilt, breathy/dark/relaxed → pressed/bright/clean), with TWO ways to play. The SLIDERS + SPACE hold a sustained drone you tweak by hand. The JAM PAD is a scale-locked 2D strip (like the radio jam bar): drag across it to sing — X picks the note (major pentatonic, always in tune), Y is the second dimension = effort (up = louder/pressed), and every new note grabs a RANDOM consonant + vowel so dragging auto-scats ('ba-doo-la-see'). The SHAPE toggle (S) flips where the consonant goes: CV 'b·a' = a consonant ONSET (voice_consonant), the note starts on it ('bah'/'mah'); VC 'a·m' = a consonant CODA (voice_coda), the note closes on it ('ahh-m'/'oo-d') as it releases. Consonants are timed onsets/codas, vibrato is the external pitch LFO (V toggle) — neither is an axis. Still drives the engine through the experimental voice_param()/voice_consonant()/voice_coda() path pending the final wiring. Drag the JAM PAD to sing · SPACE = drone · Z/X shift the scale root · V vibrato · S syllable shape (CV/VC) · drag the three sliders for the base voice."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// VOX — the formant voice (INSTR_VOICE) on the THREE axes the voxlab probe landed on
// (VOWEL / SIZE / EFFORT), with two ways to play and two syllable shapes.
//
//   • SLIDERS + SPACE — hold a sustained drone and shape the base voice by hand.
//   • JAM PAD — a scale-locked 2D strip (like the radio jam bar): drag to sing. X picks the
//     note (major pentatonic, always in tune); Y = EFFORT (up = louder/pressed). Each new note
//     grabs a RANDOM consonant + vowel, so dragging auto-scats.
//
//   SHAPE toggle — where the consonant goes:
//     CV "b·a"  = consonant ONSET (voice_consonant): the note STARTS on it — "bah", "mah".
//     VC "a·m"  = consonant CODA  (voice_coda):       the note ENDS on it — "ahh-m", "oo-d".
//                 (clean vowel attack; a voiced coda morphs in as the note releases.)
//
// EFFORT is one macro (breath + open-q + tilt). Vibrato is the external pitch LFO (V), not an
// axis. Consonants are timed onsets/codas, not an axis. All still via the experimental
// note_aux()/voice_consonant()/voice_coda() path pending the final wiring.
//
// CONTROLS: drag the JAM PAD to sing · SPACE = drone · Z/X shift the scale root · V vibrato ·
// S toggles syllable shape (CV/VC) · drag the three sliders for the base voice.

#define SLOT 5

enum { VP_VOWEL, VP_SIZE, VP_BREATH, VP_OPENQ, VP_TILT, VP_VIBD, VP_VIBR };

#define PADX 8
#define PADY 98
#define PADW 304
#define PADH 70
#define NSTEPS 10
static const int PENT[NSTEPS] = { 0, 2, 4, 7, 9, 12, 14, 16, 19, 21 }; // major pentatonic, 2 oct

static float vowel  = 0.5f;
static float size   = 0.33f;
static float effort = 0.5f;
static int   voice  = -1;
static float vpitch = 48;     // scale root (also the drone pitch)
static int   vibrato = 0;
static int   codaMode = 0;    // 0 = CV (consonant onset), 1 = VC (consonant coda)

static int   jam     = 0;
static int   padstep = -1;
static float jvowel  = 0.5f;
static int   jcons   = -1;    // the onset consonant this note used (CV mode)
static float jeff    = 0.5f;

static const char *VOWELS[5] = { "U  oo", "O  oh", "A  ah", "E  eh", "I  ee" };
static const char *CONS[9]   = { "-", "b", "d", "g", "m", "n", "l", "s", "sh" };
// random pools (ids; mellow, voiced — no hissy s/sh). onset pool may pick "none" (-1).
static const int JAM_CONS[]  = { -1, 0, 1, 2, 3, 4, 5 };
static const int CODA_CONS[] = { 3, 4, 5, 3, 4, 5, 0, 1, 2 };   // m n l (weighted) + b d g
#define JAM_NCONS  ((int)(sizeof(JAM_CONS)  / sizeof(JAM_CONS[0])))
#define CODA_NCONS ((int)(sizeof(CODA_CONS) / sizeof(CODA_CONS[0])))

static void push_effort(int v, float e) {   // the locked EFFORT curve (matches vox_apply_macros)
    note_aux(v, VP_BREATH, lerp(0.22f, 0.00f, e));   // light breath — not the old whispery 0.55
    note_aux(v, VP_OPENQ,  lerp(0.85f, 0.20f, e));
    note_aux(v, VP_TILT,   lerp(0.70f, 0.05f, e));
}

static void apply_vibrato(int v) {
    note_lfo(v, 0, LFO_PITCH, 5.5f, vibrato ? 0.18f : 0.0f);
}

// (re)start the held note at midi, beginning with consonant onset `c` (-1 = clean vowel attack)
static void start_note(int midi, int c) {
    if (voice >= 0) note_off(voice);
    voice = note_on(midi, SLOT, 6);
    note_glide(voice, 25);
    note_aux(voice, VP_VIBD, 0.0f);   // engine vibrato off — the LFO owns it
    apply_vibrato(voice);
    voice_consonant(voice, c);
}

// end the current note — in VC mode it CLOSES on a random voiced coda ("ahh-m")
static void end_note(void) {
    if (voice < 0) return;
    if (codaMode) voice_coda(voice, CODA_CONS[rnd(CODA_NCONS)]);
    note_off(voice);
    voice = -1;
}

void init(void) {
    instrument(SLOT, INSTR_VOICE, 45, 60, 7, 200);   // a little release tail for the coda to ring in
}

void update(void) {
    int mx = mouse_x(), my = mouse_y();
    int inpad = mx >= PADX && mx < PADX + PADW && my >= PADY && my < PADY + PADH;

    // ---- JAM PAD ----
    if (mouse_pressed(MOUSE_LEFT) && inpad) { jam = 1; padstep = -1; }
    if (jam && mouse_down(MOUSE_LEFT)) {
        int step = (mx - PADX) * NSTEPS / PADW; step = (int)clamp(step, 0, NSTEPS - 1);
        jeff = clamp(1.0f - (float)(my - PADY) / PADH, 0, 1);
        if (step != padstep) {                          // new scale step = new sung note
            padstep = step;
            jvowel = rnd(5) * 0.25f;                    // random vowel
            jcons  = codaMode ? -1 : JAM_CONS[rnd(JAM_NCONS)];   // CV: random onset · VC: clean start
            start_note((int)(vpitch + 0.5f) + PENT[step], jcons);
        }
        if (voice >= 0) {
            note_aux(voice, VP_VOWEL, jvowel);
            note_aux(voice, VP_SIZE, size);
            push_effort(voice, jeff);
        }
    } else if (jam) {                                   // released the pad → close the note (coda in VC)
        jam = 0; padstep = -1;
        end_note();
    }

    // ---- SPACE drone (only when not jamming) ----
    if (!jam) {
        if (keyp(KEY_SPACE)) {
            if (voice < 0) start_note((int)(vpitch + 0.5f), -1);   // clean vowel
            else end_note();                                       // VC mode closes on a consonant
        }
        if (voice >= 0) {
            note_pitch(voice, vpitch);
            note_aux(voice, VP_VOWEL, vowel);
            note_aux(voice, VP_SIZE, size);
            push_effort(voice, effort);
        }
    }

    if (keyp('Z')) vpitch = clamp(vpitch - 1, 24, 72);
    if (keyp('X')) vpitch = clamp(vpitch + 1, 24, 72);
    if (keyp('V')) { vibrato = !vibrato; if (voice >= 0) apply_vibrato(voice); }
    if (keyp('S')) codaMode = !codaMode;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("VOX", 8, 4, CLR_PEACH);
    print("formant voice + jam", 44, 4, CLR_MEDIUM_GREY);

    ui_begin();

    // ---- the three axes (base voice) ----
    print("VOWEL",  10, 16, CLR_LIGHT_PEACH); ui_slider(&vowel,  44, 16, 102, 0);
    print("SIZE",   10, 34, CLR_LIGHT_PEACH); ui_slider(&size,   44, 34, 102, 0);
    print("EFFORT", 10, 52, CLR_LIGHT_PEACH); ui_slider(&effort, 64, 52, 82, 0);

    // ---- transport (right) ----
    int rx = 158;
    if (ui_button(rx, 12, 90, 14, voice >= 0 ? "STOP" : "DRONE")) {
        if (voice < 0) start_note((int)(vpitch + 0.5f), -1); else end_note();
    }
    if (ui_button(rx, 28, 20, 14, "-")) vpitch = clamp(vpitch - 1, 24, 72);
    if (ui_button(rx + 70, 28, 20, 14, "+")) vpitch = clamp(vpitch + 1, 24, 72);
    char pb[5]; int m = (int)(vpitch + 0.5f);
    pb[0] = 'm'; pb[1] = '0' + (m / 10) % 10; pb[2] = '0' + m % 10; pb[3] = 0;
    print_centered(pb, rx + 45, 32, CLR_YELLOW);
    if (ui_button(rx, 44, 90, 14, vibrato ? "vibrato ON" : "vibrato off"))
        { vibrato = !vibrato; if (voice >= 0) apply_vibrato(voice); }
    if (ui_button(rx, 60, 90, 14, codaMode ? "shape: VC a.m" : "shape: CV b.a"))
        codaMode = !codaMode;

    ui_end();

    // ---- the last sung syllable ----
    print("singing:", 158, 78, CLR_MEDIUM_GREY);
    if (jam && padstep >= 0) {
        int vi = (int)(jvowel * 4.0f + 0.5f); if (vi > 4) vi = 4;
        char syl[8]; int n = 0;
        char v = VOWELS[vi][0] + 32;
        if (codaMode) { syl[n++] = v; syl[n++] = '-'; syl[n++] = '?'; }   // VC: vowel then mystery coda
        else { if (jcons >= 0) { const char *c = CONS[jcons + 1]; syl[n++] = c[0]; if (c[1]) syl[n++] = c[1]; } syl[n++] = v; }
        syl[n] = 0;
        print(syl, 222, 78, CLR_LIGHT_YELLOW);
    } else {
        print("--", 222, 78, CLR_DARK_GREY);
    }

    // ---- JAM PAD ----
    rectfill(PADX, PADY, PADW, PADH, CLR_DARKER_GREY);
    for (int s = 0; s < NSTEPS; s++) {
        int x = PADX + s * PADW / NSTEPS;
        if (s) line(x, PADY, x, PADY + PADH, CLR_DARK_GREY);
        if (PENT[s] % 12 == 0) rectfill(x + 1, PADY + 1, PADW / NSTEPS - 1, PADH - 2, CLR_DARKER_PURPLE);
    }
    rect(PADX, PADY, PADW, PADH, CLR_DARK_GREY);
    print("JAM PAD - drag to sing   (up = louder)", PADX + 4, PADY + 3, CLR_MEDIUM_GREY);
    if (jam && padstep >= 0) {
        int colx = PADX + padstep * PADW / NSTEPS;
        rectfill(colx + 1, PADY + 1, PADW / NSTEPS - 1, PADH - 2, CLR_DARK_RED);
        int my = (int)clamp(mouse_y(), PADY + 3, PADY + PADH - 3);
        int cxp = colx + (PADW / NSTEPS) / 2;
        circfill(cxp, my, 4, CLR_YELLOW);
        circfill(cxp, my, 2, CLR_WHITE);
    }

    print("drag pad: sing  SPACE: drone  Z/X: key  V: vib  S: shape", 8, 190, CLR_DARK_GREY);
}
