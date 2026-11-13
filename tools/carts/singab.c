/* de:meta
{
  "slug": "singab",
  "title": "sing A/B",
  "status": "active",
  "created": "2026-11-13",
  "kind": ["probe"],
  "teaches": ["generative-melody"],
  "lineage": "A/B probe against JIANT FM's VOICE/WORDS engine (M-VAVE FM-1 firmware, GPL-3.0; ideas only, no code). Sings the same lyric on the same melody and timing as JIANT's hostsim render, through INSTR_VOICE with hand-phonemized syllables.",
  "description": {
    "summary": "Sings 'hello my darling stay with me tonight' through INSTR_VOICE on the exact melody and timing of the JIANT FM comparison render, so the two engines can be heard side by side.",
    "detail": "Each note is one syllable: an onset consonant (voice_consonant), a vowel (note_aux VP_VOWEL), an optional diphthong glide over the note (aux 17/18), and a coda consonant fired just before release (voice_coda). The syllables are phonemized by hand, which is exactly the layer JIANT automates (text to phonemes, syllables advanced by the key gate). R toggles the reverb send.",
    "controls": "R = reverb on/off"
  }
}
de:meta */
#include "studio.h"

// SING A/B: the same lyric, melody and timing as the JIANT hostsim clip
// (STEPS=60,0,62,0,64,0,65,0,67,0,0,0,65,0,64,0,62,0,60,0,0,0,0,0 at BPM 90, 16th steps).
// At 60 fps a 16th at 90 BPM is exactly 10 frames. A note sounds for one step, then a rest step.
//
// What INSTR_VOICE lacks that this cart has to fake (the gaps the A/B is meant to expose):
//   - no 'h' (hello starts on a bare vowel), no onset clusters (stay = s only, the t is lost)
//   - no text layer: every syllable below is hand-phonemized
//   - no language-specific aspiration, no burst/closure stages: a consonant is one timed morph

#define SLOT 5
#define STEP_FR 10
enum { VP_VOWEL = 0, VP_SIZE = 1, VP_BREATH = 2, VP_OPENQ = 3, VP_TILT = 4, VP_VIBD = 5, VP_VIBR = 6,
       VP_JIT = 11, VP_SHIM = 12, VP_REDUCE = 15, VP_VOW2 = 17, VP_GLIDE = 18 };
enum { C_NONE = -1, C_B, C_D, C_G, C_M, C_N, C_L, C_S, C_SH, C_NG, C_R, C_W, C_Y, C_DH, C_F, C_V, C_Z,
       C_ZH, C_TH, C_P, C_T, C_K, C_CH };
// vowels: path U O A E I = 0 .25 .5 .75 1 (VP_VOWEL); table rows for VP_VOW2: 0 U 1 O 2 A 3 E 4 I 5 AE 6 AH 7 AW 8 UH 9 ER
#define V_U 0.0f
#define V_O 0.25f
#define V_A 0.5f
#define V_E 0.75f
#define V_I 1.0f

typedef struct { const char *txt; int onset; float vowel; int vow2; int coda; } Syl;   // vow2 -1 = none
static const Syl LYR[] = {
    {"hel",   C_NONE, V_E, -1, C_L },
    {"lo",    C_L,    V_O,  8, C_NONE},   // o -> u (go)
    {"my",    C_M,    V_A,  4, C_NONE},   // a -> i
    {"dar",   C_D,    V_A, -1, C_R },
    {"ling",  C_L,    V_I, -1, C_NG},
    {"stay",  C_S,    V_E,  4, C_NONE},   // e -> i
    {"with",  C_W,    V_I, -1, C_TH},
    {"me",    C_M,    V_I, -1, C_NONE},
    {"to",    C_T,    V_A,  6, C_NONE},   // reduced: sits on AH
    {"night", C_N,    V_A,  4, C_T },     // a -> i
};
#define NLYR ((int)(sizeof LYR / sizeof LYR[0]))
static const int MEL[24] = {60,0,62,0,64,0,65,0,67,0,0,0,65,0,64,0,62,0,60,0,0,0,0,0};

static int fr = 0, syl = 0, voice = -1, cur = 0, wet = 1;

static void apply_wet(void) { instrument_reverb(SLOT, wet ? 0.3f : 0.0f); }

void init(void) {
    instrument(SLOT, INSTR_VOICE, 5, 60, 7, 190);   // ATK ~5 ms, REL ~190 ms: JIANT's singer presets
    reverb(0.45f, 0.5f);
    apply_wet();
}

void update(void) {
    if (keyp('R')) { wet = !wet; apply_wet(); }
    int step = (fr / STEP_FR) % 24, sub = fr % STEP_FR;
    if (sub == 0 && MEL[step]) {                      // a note: the next syllable
        const Syl *s = &LYR[syl % NLYR];
        cur = syl % NLYR;
        syl++;
        voice = note_on(MEL[step], SLOT, 6);
        note_aux(voice, VP_SIZE, 0.33f);
        note_aux(voice, VP_BREATH, 0.10f);
        note_aux(voice, VP_OPENQ, 0.50f);
        note_aux(voice, VP_TILT, 0.30f);
        note_aux(voice, VP_JIT, 0.15f);
        note_aux(voice, VP_SHIM, 0.15f);
        note_aux(voice, VP_VOWEL, s->vowel);
        if (s->vow2 >= 0) note_aux(voice, VP_VOW2, s->vow2 / 9.0f);
        note_aux(voice, VP_GLIDE, (s->vow2 == 6) ? 1.0f : 0.0f);
        voice_coda(voice, -1);
        voice_consonant(voice, s->onset);
    }
    if (voice >= 0) {
        const Syl *s = &LYR[cur];
        if (s->vow2 >= 0 && s->vow2 != 6)            // diphthong: glide across the back of the note
            note_aux(voice, VP_GLIDE, clamp((sub - 3) / 6.0f, 0, 1));
        if (sub == STEP_FR - 4 && s->coda >= 0) voice_coda(voice, s->coda);
        if (sub == STEP_FR - 1) { note_off(voice); voice = -1; }
    }
    fr++;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("SING A/B  (INSTR_VOICE)", 8, 8, CLR_WHITE);
    for (int i = 0; i < NLYR; i++)
        print(LYR[i].txt, 8 + (i % 5) * 60, 40 + (i / 5) * 14, i == cur ? CLR_YELLOW : CLR_LIGHT_GREY);
    print(wet ? "R: reverb ON" : "R: reverb OFF", 8, 180, CLR_INDIGO);
}
