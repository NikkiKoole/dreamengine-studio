/* de:meta
{
  "slug": "voxpad",
  "title": "voxpad",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "instrument",
    "probe"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Companion to voxab (continuous-lever probe); novel in being a full phoneme keyboard that covers all 10 table vowels via the diphthong path and all 22 consonants including the navkit table-completion additions, with loop/coda/VC mode articulation.",
  "description": "The PHONEME KEYBOARD for the formant VOICE engine (INSTR_VOICE) — where voxab A/B's the continuous levers, this cart is for HEARING the full articulation set: every vowel and every consonant, including the navkit table-completion phonemes the other voice carts never trigger. Click a VOWEL (all 10 table vowels — ae ah aw uh er are the new ones, reached via the experimental diphthong path vow2+glide). Click a CONSONANT to articulate it: a consonant is a timed event (voice_consonant onset / voice_coda), not a held knob, so a click says 'C + the current vowel' — 'fah', 'shoo', 'chee', 'ngah'. The 14 underlined consonants (ng r w y dh f v z zh th p t k ch) are the ones appended to complete navkit's table. HOLD a drone (SPACE) and consonant clicks re-articulate MID-NOTE (connected, like the say probe); with no drone each click is a one-shot syllable that rings out. V toggles CV (consonant opens the note) vs VC (it closes the note). SIZE and EFFORT sliders push it to giant/child tract and breathy→pressed. LOOP (L) auto-repeats the current syllable — say it, pause, re-say it, over and over (it 'sings' the phoneme), and you can change the vowel/consonant mid-loop. Still drives the engine through the experimental voice_param()/voice_consonant()/voice_coda() path. Click vowel + consonant to say it · SPACE drone · L loop · V mode · Z/X pitch · drag SIZE/EFFORT.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// VOXPAD — the phoneme keyboard for the formant VOICE engine (INSTR_VOICE). voxab A/B's the
// continuous LEVERS; this cart is for HEARING the articulation set — every vowel and every
// consonant, including the navkit table-completion phonemes the other carts never trigger.
//
//   • VOWEL ROW — click to choose the held vowel (all 10 table vowels; ae ah aw uh er are the new
//     ones). Driven through the experimental diphthong path (vow2 + glide=1) so even the extra
//     vowels are reachable as steady vowels.
//   • CONSONANT PAD — click to ARTICULATE. A consonant is a timed event (voice_consonant onset /
//     voice_coda), not a held knob: clicking fires "C + current vowel" ("fah", "shoo", "chee").
//     The 14 on the right of the divider (ng r w y dh f v z zh th p t k ch) are the NEW ones.
//   • HOLD a drone (SPACE) and the consonant clicks re-articulate mid-note (connected, like say);
//     with no drone, each click is a one-shot syllable that rings out.
//   • SIZE / EFFORT sliders make it weird — giant/child tract, breathy→pressed.
//   • LOOP (L) — auto-repeats the current syllable: say it, pause, re-say it, over and over (it
//     'sings' the phoneme). Change the vowel/consonant while it loops and the next pass picks it up.
//
// All still via the experimental note_aux()/voice_consonant()/voice_coda() path.
//
// CONTROLS: click a VOWEL · click a CONSONANT to say it · SPACE hold/release a drone · L loop
// (auto re-say) · V coda-mode (consonant closes the note instead of opening it) · Z/X pitch · drag SIZE/EFFORT.

#define SLOT 5

enum { VP_VOWEL, VP_SIZE, VP_BREATH, VP_OPENQ, VP_TILT, VP_VIBD, VP_VIBR,
       VP_NASAL, VP_OPENF1, VP_FRONTF2, VP_BUZZ, VP_JIT, VP_SHIM, VP_CREAK,
       VP_NASALAF, VP_REDUCE, VP_MEASBW, VP_VOW2, VP_GLIDE };

static const char *VOW_LBL[10] = { "U", "O", "A", "E", "I", "ae", "ah", "aw", "uh", "er" };
// consonant ids 0..21 — mirrors sound.h vox_cons_name. ids 8..21 are the new ones (after the |).
static const char *CONS[22] = { "b","d","g","m","n","l","s","sh",
    "ng","r","w","y","dh","f","v","z","zh","th","p","t","k","ch" };
#define NEW_CONS 8   // ids >= this are the table-completion additions

static int   voice = -1;
static int   drone = 0;
static float pitch = 48;
static int   cur_vowel = 2;     // "A" / "ah"
static int   coda_mode = 0;
static float size_v = 0.33f;
static float effort = 0.5f;
static int   last_cons = -1;    // for the syllable readout
static int   syl_timer = 0;     // one-shot syllable: frames until auto-release
static int   coda_pending = -1; // VC one-shot: consonant to CLOSE on, fired in the back third
static int   loop = 0;          // auto-repeat the current syllable (say · pause · re-say)
static int   loop_gap = 0;      // frames of silence between loop repeats
#define LOOP_GAP 22

static void push_effort(int v) {
    note_aux(v, VP_BREATH, lerp(0.55f, 0.00f, effort));
    note_aux(v, VP_OPENQ,  lerp(0.95f, 0.05f, effort));
    note_aux(v, VP_TILT,   lerp(0.80f, 0.00f, effort));
}

// apply the held vowel + character to a voice (vowel via the diphthong path so all 10 reach)
static void apply_voice(int v) {
    note_aux(v, VP_VOW2, cur_vowel / 9.0f);
    note_aux(v, VP_GLIDE, 1.0f);
    note_aux(v, VP_SIZE, size_v);
    push_effort(v);
}

static void start_drone(void) {
    if (voice >= 0) note_off(voice);
    voice = note_on((int)(pitch + 0.5f), SLOT, 6);
    note_glide(voice, 20);
    apply_voice(voice);
    syl_timer = 0;
}

static void fire_cons(int id) {
    last_cons = id;
    if (drone && voice >= 0) {                  // re-articulate on the held note
        if (coda_mode) voice_coda(voice, id);
        else           voice_consonant(voice, id);
        return;
    }
    // one-shot syllable: let it ring, auto-release. CV opens on the consonant; VC starts clean
    // on the vowel and CLOSES on the consonant later (the coda is fired from update()).
    if (voice >= 0) note_off(voice);
    voice = note_on((int)(pitch + 0.5f), SLOT, 6);
    note_glide(voice, 12);
    apply_voice(voice);
    if (coda_mode) { coda_pending = id; }                       // VC: "ah-m"
    else { voice_consonant(voice, id); coda_pending = -1; }     // CV: "mah"
    syl_timer = 32;
}

// (re)say whatever's currently selected — used by the loop. Vowel-only if no consonant picked yet.
static void say_current(void) {
    if (last_cons >= 0) { fire_cons(last_cons); return; }
    if (voice >= 0) note_off(voice);
    voice = note_on((int)(pitch + 0.5f), SLOT, 6);
    note_glide(voice, 12);
    apply_voice(voice);
    coda_pending = -1;
    syl_timer = 32;
}

void init(void) {
    instrument(SLOT, INSTR_VOICE, 45, 60, 7, 200);
}

void update(void) {
    if (keyp(KEY_SPACE)) {
        drone = !drone;
        if (drone) { loop = 0; start_drone(); }
        else if (voice >= 0) { note_off(voice); voice = -1; }
    }
    if (keyp('L')) { loop = !loop; if (loop) { drone = 0; if (voice >= 0) { note_off(voice); voice = -1; } loop_gap = 0; } }
    if (keyp('V')) coda_mode = !coda_mode;
    if (keyp('Z')) pitch = clamp(pitch - 1, 24, 72);
    if (keyp('X')) pitch = clamp(pitch + 1, 24, 72);

    // one-shot syllable: advance; in VC mode fire the CODA in the back third so it CLOSES on the
    // consonant ("ah-m"); then auto-release and leave a gap before the loop re-says.
    if (!drone && syl_timer > 0) {
        syl_timer--;
        if (coda_pending >= 0 && syl_timer == 13 && voice >= 0) { voice_coda(voice, coda_pending); coda_pending = -1; }
        if (syl_timer == 0 && voice >= 0) { note_off(voice); voice = -1; loop_gap = LOOP_GAP; }
    }

    // loop: say the current syllable, pause, re-say it
    if (loop && voice < 0) { if (loop_gap > 0) loop_gap--; else say_current(); }

    // keep the drone's vowel/character live
    if (drone && voice >= 0) { note_pitch(voice, pitch); apply_voice(voice); }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("VOXPAD", 4, 4, CLR_PEACH);
    print("phoneme keyboard - click to say it", 64, 4, CLR_MEDIUM_GREY);

    ui_begin();

    // ---- vowel row ----
    print("VOWEL", 4, 18, CLR_LIGHT_PEACH);
    for (int i = 0; i < 10; i++)
        if (ui_button(48 + i * 27, 16, 25, 12, VOW_LBL[i])) { cur_vowel = i; if (voice >= 0) apply_voice(voice); }

    // ---- consonant pad (2 rows of 11; | divider after id 7 marks the new ones) ----
    print("SAY", 4, 36, CLR_LIGHT_PEACH);
    print("(ng..ch = new)", 28, 44, CLR_DARKER_GREY);
    for (int i = 0; i < 22; i++) {
        int row = i / 11, col = i % 11;
        int bx = 120 + col * 18, by = 34 + row * 14;
        if (ui_button(bx, by, 16, 12, CONS[i])) fire_cons(i);
    }

    // ---- transport + character ----
    if (ui_button(4, 64, 70, 13, drone ? "DRONE on" : "drone")) {
        drone = !drone;
        if (drone) start_drone(); else if (voice >= 0) { note_off(voice); voice = -1; }
    }
    if (ui_button(78, 64, 18, 13, "-")) pitch = clamp(pitch - 1, 24, 72);
    if (ui_button(120, 64, 18, 13, "+")) pitch = clamp(pitch + 1, 24, 72);
    char pb[5]; int m = (int)(pitch + 0.5f);
    pb[0] = 'm'; pb[1] = '0' + (m / 10) % 10; pb[2] = '0' + m % 10; pb[3] = 0;
    print_centered(pb, 110, 67, CLR_YELLOW);
    if (ui_button(142, 64, 96, 13, coda_mode ? "mode: VC (close)" : "mode: CV (open)")) coda_mode = !coda_mode;
    if (ui_button(240, 64, 74, 13, loop ? "LOOP on" : "loop")) {
        loop = !loop;
        if (loop) { drone = 0; if (voice >= 0) { note_off(voice); voice = -1; } loop_gap = 0; }
    }

    print("SIZE",   4, 84, CLR_LIGHT_PEACH); ui_slider(&size_v, 40, 82, 90, 0);
    print("EFFORT", 140, 84, CLR_LIGHT_PEACH); ui_slider(&effort, 186, 82, 90, 0);

    ui_end();

    // mark the active vowel + the new consonants
    line(48 + cur_vowel * 27, 29, 48 + cur_vowel * 27 + 24, 29, CLR_YELLOW);
    for (int i = NEW_CONS; i < 22; i++) {
        int row = i / 11, col = i % 11, bx = 120 + col * 18, by = 34 + row * 14;
        line(bx, by + 13, bx + 15, by + 13, CLR_LIGHT_PEACH);
    }

    // ---- big syllable readout ----
    char syl[10]; int n = 0;
    if (last_cons >= 0 && !coda_mode) { const char *c = CONS[last_cons]; while (*c) syl[n++] = *c++; }
    const char *vw = VOW_LBL[cur_vowel]; while (*vw) syl[n++] = *vw++;
    if (last_cons >= 0 && coda_mode) { syl[n++] = '-'; const char *c = CONS[last_cons]; while (*c) syl[n++] = *c++; }
    syl[n] = 0;
    rectfill(4, 104, 312, 40, CLR_DARKER_GREY);
    rect(4, 104, 312, 40, CLR_DARK_GREY);
    print("saying:", 10, 108, CLR_MEDIUM_GREY);
    print_centered(syl, 160, 120, CLR_LIGHT_YELLOW);
    print(drone ? "(held drone - clicks re-articulate)"
        : (loop ? "(looping - says it over and over)" : "(one-shot syllables)"), 10, 134, CLR_DARK_GREY);

    print("click VOWEL + CONSONANT to say it   SPACE drone   L loop   V mode", 4, 168, CLR_DARK_GREY);
    print("Z/X pitch   drag SIZE/EFFORT   the underlined consonants are NEW", 4, 178, CLR_DARK_GREY);
    print("plosives b d g p t k . fricatives s sh f v z zh th . nasals m n ng", 4, 190, CLR_DARKER_GREY);
}
