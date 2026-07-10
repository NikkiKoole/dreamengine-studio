/* de:meta
{
  "slug": "voxab",
  "title": "voxab",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "instrument",
    "probe"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "A/B comparator tool for the INSTR_VOICE formant engine — adds instant A/B flip, solo parameter sweep, and auto-retrig on top of the voxlab explorer to isolate subtle lever contributions (buzz, creak, nasality, measured-BW).",
  "description": "The A/B COMPARATOR for the formant VOICE engine (INSTR_VOICE). Where voxlab explores the space, this cart decides BY EAR whether a subtle lever earns its place — the listening voxlab can't do. Holds TWO full param states A and B; TAB snaps the LIVE sustained tone between them INSTANTLY, so the difference lives in the flip, not in slider-hunting. SOLO-SWEEP (W) auto-ramps the SELECTED param 0→1→0 with everything else frozen, so you hear ONLY that param. An optional auto-RETRIG (G) re-articulates the held tone so you also hear attacks. The preset A/B PAIRS load a base differing in ONE lever for a direct flip-test: BW (derived vs navkit measured bandwidths) · NASAL (formant-config morph vs anti-formant notch — the two nasality models) · BUZZ (full glottal pulse vs smooth sine) · CREAK (clean vs vocal fry) · REDUCE (full vowel vs schwa). These exercise the EXPERIMENTAL navkit completeness-audit levers (voice_param indices 10-16: buzz/jitter/shimmer/creak/anti-formant nasality/schwa-reduce/measured-BW). Click a param row or use [ ] to select; drag the bottom slider to edit it in the live slot; C copies live→other. SPACE play · TAB flip A/B · Z/X pitch.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// VOXAB — the A/B comparator for the formant VOICE engine (INSTR_VOICE). voxlab is for
// EXPLORING the space; this cart is for DECIDING by ear whether a lever earns its place.
// Three things voxlab can't do that matter for subtle levers (measured-BW, buzz, creak,
// the two nasality models):
//
//   • A/B FLIP — two full param states A and B; TAB snaps the LIVE held tone between them
//     instantly. The difference lives in the flip, not in slider-hunting. (Preset PAIRS set
//     A and B to a base differing in ONE lever, so you press a pair, flip, and hear it.)
//   • SOLO SWEEP — auto-ramp the SELECTED param 0→1→0 on the live tone, everything else
//     frozen, so you hear ONLY that param's contribution.
//   • HANDS-FREE TONE — a sustained drone (optional auto-RETRIG so you also hear attacks),
//     so you just sit and listen.
//
// All still via the experimental note_aux() path (indices 0..16, mirrors sound.h) — no
// API commitment. The new levers are the navkit completeness-audit finds
// (docs/design/voice-engine.md): buzz / jitter / shimmer / creak / anti-formant nasality /
// schwa-reduce / measured-bandwidth toggle.
//
// CONTROLS: SPACE play/stop · TAB flip A/B · [ ] select param · click a row to select ·
// drag the bottom slider to edit the selected param in the LIVE slot · C copy live→other ·
// W solo-sweep · G retrig · Z/X pitch · the pair buttons load an A/B contrast · the VOWEL
// STEPPER row (bottom) sings any of the 10 table vowels in one click — ae/ah/aw/uh/er are the
// navkit table-completion vowels, 'off' returns to the primary vowel slider.

#define SLOT 5

// param indices — MUST mirror sound.h VOX_NPARAM order (0..16)
enum { P_VOWEL, P_SIZE, P_BREATH, P_OPENQ, P_TILT, P_VIBD, P_VIBR,
       P_NASAL, P_OPENF1, P_FRONTF2, P_BUZZ, P_JIT, P_SHIM, P_CREAK,
       P_NASALAF, P_REDUCE, P_MEASBW, P_VOW2, P_GLIDE, NP };

static const char *PNAME[NP] = {
    "vowel", "size", "breath", "open-q", "tilt", "vib amt", "vib hz",
    "nasal", "openF1", "frontF2", "buzz", "jitter", "shimmer", "creak",
    "nasalAF", "reduce", "measBW", "vow2", "glide" };

// neutral "ah" base — matches sound.h sound_voice_start() defaults
static const float DEF[NP] = { 0.5f, 0.33f, 0.10f, 0.5f, 0.30f, 0.15f, 0.5f,
                               0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f,
                               0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

static float A[NP], B[NP];      // the two stored states
static int   active = 0;        // which slot is LIVE (0 = A, 1 = B)
static int   sel = P_VOWEL;     // selected param (edited / swept)
static int   voice = -1;
static int   playing = 0;
static float pitch = 48;
static int   retrig = 0;
static int   retrig_t = 0;
#define RETRIG_FRAMES 110       // ~1.8s between re-articulations
static int   sweep = 0;
static float sweep_ph = 0;      // 0..1, triangle → sweepval
static float sweepval = 0;

// preset A/B PAIRS — each loads a base, differing in one lever, for a direct flip-test
enum { PR_BW, PR_NASAL, PR_BUZZ, PR_CREAK, PR_REDUCE, PR_VOWEL2, NPAIR };
static const char *PRNAME[NPAIR] = { "BW", "NASAL", "BUZZ", "CREAK", "REDUCE", "VOWEL2" };

// the 10 table vowels (rows 0..9). 0..4 are the slider path; 5..9 (ae..er) are the new
// table-completion vowels. The stepper row sets vow2 + glide=1 so a click SINGS the vowel.
static const char *VOW_LBL[10] = { "U", "O", "A", "E", "I", "ae", "ah", "aw", "uh", "er" };

static void load_pair(int p) {
    for (int i = 0; i < NP; i++) A[i] = B[i] = DEF[i];
    switch (p) {
        case PR_BW:     A[P_MEASBW] = 0.0f; B[P_MEASBW] = 1.0f;          break; // derived vs measured BW
        case PR_NASAL:  A[P_NASAL]  = 0.8f; B[P_NASALAF] = 0.8f;         break; // config-morph vs anti-formant
        case PR_BUZZ:   A[P_BUZZ]   = 1.0f; B[P_BUZZ]   = 0.0f;          break; // glottal vs smooth sine
        case PR_CREAK:  A[P_CREAK]  = 0.0f; B[P_CREAK]  = 0.7f;          break; // clean vs vocal fry
        case PR_REDUCE: A[P_REDUCE] = 0.0f; B[P_REDUCE] = 0.85f;         break; // full vowel vs schwa
        case PR_VOWEL2: A[P_VOW2]   = B[P_VOW2] = 0.556f;                       // vow2 = AE (row 5/9)
                        A[P_GLIDE]  = 0.0f; B[P_GLIDE] = 1.0f;          break; // "ah" vs the new "ae" vowel
    }
}

static float *live(void) { return active ? B : A; }

static void start_voice(void) {
    if (voice >= 0) note_off(voice);
    voice = note_on((int)(pitch + 0.5f), SLOT, 6);
    note_glide(voice, 20);
    retrig_t = 0;
}

static void stop_voice(void) {
    if (voice >= 0) note_off(voice);
    voice = -1;
}

static void apply_live(void) {
    if (voice < 0) return;
    note_pitch(voice, pitch);
    float *s = live();
    for (int i = 0; i < NP; i++)
        note_aux(voice, i, (sweep && i == sel) ? sweepval : s[i]);
}

void init(void) {
    instrument(SLOT, INSTR_VOICE, 45, 60, 7, 160);
    load_pair(PR_BW);
}

void update(void) {
    if (keyp(KEY_SPACE)) { if (playing) { stop_voice(); playing = 0; } else { start_voice(); playing = 1; } }
    if (keyp(KEY_TAB))   active = !active;
    if (keyp('['))       sel = (sel + NP - 1) % NP;
    if (keyp(']'))       sel = (sel + 1) % NP;
    if (keyp('C'))       { float *s = live(), *o = active ? A : B; for (int i = 0; i < NP; i++) o[i] = s[i]; }
    if (keyp('W'))       { sweep = !sweep; sweep_ph = 0; }
    if (keyp('G'))       retrig = !retrig;
    if (keyp('Z'))       pitch = clamp(pitch - 1, 24, 72);
    if (keyp('X'))       pitch = clamp(pitch + 1, 24, 72);

    // click a param row to select it
    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT) && mx >= 4 && mx < 148 && my >= 16 && my < 16 + NP * 8) {
        int row = (my - 16) / 8;
        if (row >= 0 && row < NP) sel = row;
    }

    // solo sweep: triangle 0→1→0 over ~3s
    if (sweep) {
        sweep_ph += 1.0f / 180.0f; if (sweep_ph >= 1.0f) sweep_ph -= 1.0f;
        sweepval = sweep_ph < 0.5f ? sweep_ph * 2.0f : 2.0f - sweep_ph * 2.0f;
    }

    // auto-retrig so attacks are heard too
    if (playing && retrig && ++retrig_t >= RETRIG_FRAMES) start_voice();

    apply_live();
}

static void pnum(float v, int x, int y, int col) {
    int n = (int)(v * 100 + 0.5f);
    if (n >= 100) { print("MX", x, y, col); return; }
    char b[3]; int i = 0;
    if (n >= 10) b[i++] = '0' + (n / 10) % 10;
    b[i++] = '0' + n % 10; b[i] = 0;
    print(b, x, y, col);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("VOXAB", 4, 4, CLR_PEACH);
    print("A/B + solo-sweep audition", 52, 4, CLR_MEDIUM_GREY);

    // ---- param list (name + A value + B value); LIVE slot's column is highlighted ----
    print("A", 100, 6, active ? CLR_DARK_GREY : CLR_YELLOW);
    print("B", 126, 6, active ? CLR_YELLOW : CLR_DARK_GREY);
    for (int i = 0; i < NP; i++) {
        int y = 16 + i * 8;
        if (i == sel) rectfill(2, y - 1, 146, 8, CLR_DARKER_GREY);
        int nc = (i == sel) ? CLR_WHITE : (i >= P_BUZZ ? CLR_LIGHT_PEACH : CLR_LIGHT_GREY);
        print(PNAME[i], 6, y, nc);
        pnum(A[i], 100, y, active ? CLR_MEDIUM_GREY : CLR_WHITE);
        pnum(B[i], 126, y, active ? CLR_WHITE : CLR_MEDIUM_GREY);
    }

    ui_begin();
    int rx = 152;

    // transport + flip
    char fl[10]; fl[0]='L';fl[1]='I';fl[2]='V';fl[3]='E';fl[4]=':';fl[5]=' ';fl[6]=active?'B':'A';fl[7]=0;
    if (ui_button(rx, 16, 80, 13, fl)) active = !active;
    if (ui_button(rx + 84, 16, 80, 13, playing ? "STOP" : "PLAY")) {
        if (playing) { stop_voice(); playing = 0; } else { start_voice(); playing = 1; }
    }

    if (ui_button(rx, 32, 18, 13, "-")) pitch = clamp(pitch - 1, 24, 72);
    if (ui_button(rx + 62, 32, 18, 13, "+")) pitch = clamp(pitch + 1, 24, 72);
    char pb[5]; int m = (int)(pitch + 0.5f);
    pb[0] = 'm'; pb[1] = '0' + (m / 10) % 10; pb[2] = '0' + m % 10; pb[3] = 0;
    print_centered(pb, rx + 40, 35, CLR_YELLOW);
    if (ui_button(rx + 84, 32, 80, 13, retrig ? "retrig ON" : "retrig off")) retrig = !retrig;

    if (ui_button(rx, 48, 164, 13, sweep ? "SOLO-SWEEP on" : "solo-sweep off")) { sweep = !sweep; sweep_ph = 0; }
    if (ui_button(rx, 64, 80, 13, "copy live>")) { float *s = live(), *o = active ? A : B; for (int i = 0; i < NP; i++) o[i] = s[i]; }
    if (ui_button(rx + 84, 64, 80, 13, "reset pair")) load_pair(PR_BW);

    // preset A/B pairs
    print("A/B pairs (flip to compare):", rx, 82, CLR_MEDIUM_GREY);
    for (int p = 0; p < NPAIR; p++) {
        int bx = rx + (p % 3) * 56, by = 90 + (p / 3) * 15;
        if (ui_button(bx, by, 52, 13, PRNAME[p])) load_pair(p);
    }

    // ---- the selected-param editor (edits the LIVE slot) ----
    print("edit", rx, 122, CLR_LIGHT_PEACH);
    print(PNAME[sel], rx + 30, 122, CLR_WHITE);
    print(active ? "in B" : "in A", rx + 120, 122, CLR_YELLOW);
    ui_slider(&live()[sel], rx, 132, 130, 0);
    pnum(live()[sel], rx + 138, 133, CLR_LIGHT_GREY);

    // ---- one-click VOWEL STEPPER: click to sing any table vowel (sets vow2 + glide=1 in the
    //      live slot — no slider math). ae ah aw uh er are the navkit table-completion vowels;
    //      'off' hands control back to the primary vowel slider. ----
    for (int i = 0; i < 10; i++)
        if (ui_button(4 + i * 28, 170, 26, 11, VOW_LBL[i])) { live()[P_VOW2] = i / 9.0f; live()[P_GLIDE] = 1.0f; }
    if (ui_button(284, 170, 30, 11, "off")) live()[P_GLIDE] = 0.0f;

    ui_end();

    // mark the vowel currently being sung (glide engaged)
    if (live()[P_GLIDE] > 0.5f) {
        int av = (int)(live()[P_VOW2] * 9.0f + 0.5f); if (av > 9) av = 9; if (av < 0) av = 0;
        line(4 + av * 28, 181, 4 + av * 28 + 25, 181, CLR_YELLOW);
    }

    // sweep readout
    if (sweep) { print("sweep:", rx, 148, CLR_MEDIUM_GREY); pnum(sweepval, rx + 50, 148, CLR_LIGHT_YELLOW); }

    print("vowel row: click=sing (ae-er=new)  off=use slider", 4, 184, CLR_DARK_GREY);
    print("SPACE play  TAB flip  [ ] pick  W sweep  Z/X pitch", 4, 192, CLR_DARKER_GREY);
}
