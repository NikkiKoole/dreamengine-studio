/* de:meta
{
  "title": "vowel",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "subtractive-synth",
    "analog-voice-modeling"
  ],
  "lineage": "Talkbox / formant-filter family (not a vocoder); drives a saw-chord drone through four bandpasses tuned to human vowel frequencies, with the mouth morphing visually.",
  "description": "The FORMANT FILTER as the instrument - the showcase for formant(): four bandpasses parked at the human vowel frequencies, so a plain buzzing synth suddenly SAYS a vowel. A held saw-chord drone runs into the filter; sweep VOWEL and the chord talks OO->OH->AH->EH->EE. This is the single-input vowel filter (the talkbox family, played with a knob instead of your mouth) - NOT a vocoder, which needs a separate carrier x modulator input. The BUS side of the vowel story (colour ANY sound); INSTR_VOICE is the instrument side (a synth that sings on its own). Newest box in the effects family (spacecho/cathedral/juno/mistress/tapeloop/clavinet/eq/bitcrush/organ/vowel). The on-screen MOUTH morphs with the vowel and a ruler tracks OO..EE. VOWEL the talking knob (1..5 jump straight to a vowel), Q how pronounced/nasal, MIX dry..wet; AUTO [A] sweeps it on its own, BYPASS [Z] A/Bs the raw saw chord, H help."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── VOWEL ────────────────────────────────────────────────────────────────────
// The FORMANT FILTER as the instrument — push a plain synth through a "voice".
// A held saw-chord drone (no words of its own, just a buzz) runs into formant():
// four bandpasses parked at the human vowel frequencies, so the buzz suddenly
// says OOH / OH / AAH / EH / EEE depending on where the VOWEL knob sits. Sweep it
// and the chord talks. This is the single-input vowel filter (the talkbox family,
// played with a knob instead of your mouth) — NOT a vocoder, which needs a second
// carrier x modulator input (a future effect, once the sidechain path lands).
// The BUS side of the vowel story (color ANY sound); INSTR_VOICE is the
// instrument side (a synth that sings on its own). Newest box in the effects
// family: spacecho · cathedral · juno · mistress · tapeloop · clavinet · eq ·
// bitcrush · organ(leslie) · vowel.
//
//   VOWEL   the knob that talks:  OO -> OH -> AH -> EH -> EE
//   Q       how pronounced/nasal the vowel is (peak narrowness)
//   MIX     dry..wet (0 = the bare buzzing chord)
//   AUTO    a slow vowel sweep plays itself  ([A])
//   BYPASS  hear the raw saw chord vs the talking one  ([Z])
//   1..5    jump straight to a vowel   ·   H help

#define PAD 9               // the drone slot

static float k_vowel = 0.5f;    // 0..1 along OO->OH->AH->EH->EE  (start at AH)
static float k_q     = 0.6f;    // peak narrowness
static float k_mix   = 0.95f;   // wet — the point
static bool  on      = true;    // formant engaged at boot
static bool  autop   = true;    // auto vowel-sweep
static bool  show_help;
static float sweep_ph;          // auto-sweep phase
static int   drone[3];          // held-chord handles

// a fat, open voicing (saw chord) — lots of harmonics for the formants to carve
static const int CHORD[3] = { 40, 47, 52 };   // E2 B2 E3
static const char *VNAME[5] = { "OO", "OH", "AH", "EH", "EE" };

// set-and-hold: only re-push formant() when something moved (cheap coeff update,
// but no point spamming the queue when idle — the groovebox apply_fx() discipline).
static float last_v = -1, last_q = -1, last_mix = -1;
static void apply_formant(void) {
    float m = on ? k_mix : 0.0f;
    if (k_vowel == last_v && k_q == last_q && m == last_mix) return;
    formant(k_vowel, k_q, m);
    last_v = k_vowel; last_q = k_q; last_mix = m;
}

void init(void) {
    // a slow pad: soft attack, full sustain, long release → a continuous drone
    instrument(PAD, INSTR_SAW, 40, 200, 6, 600);
    for (int i = 0; i < 3; i++) drone[i] = note_on(CHORD[i], PAD, 5);
    apply_formant();
}

void update(void) {
    if (keyp('H')) show_help = !show_help;
    if (keyp('A')) autop = !autop;
    if (keyp('Z')) on = !on;
    for (int i = 0; i < 5; i++) if (keyp('1' + i)) { k_vowel = i / 4.0f; autop = false; }

    if (autop) {                                  // a slow vowel sweep — the chord "talks"
        sweep_ph += 0.0045f;
        if (sweep_ph >= 1.0f) sweep_ph -= 1.0f;
        k_vowel = 0.5f - 0.5f * cosf(sweep_ph * 6.2831853f);   // 0..1..0 ease
    }
    apply_formant();   // a sweeping vowel is the intended motion (cheap coeff update, no buffer churn)
}

// draw a mouth whose shape reads the current vowel: rounded/small for OO,
// wide for AH, a flat slit for EE — purely cosmetic vowel feedback.
static void draw_mouth(int cx, int cy) {
    float v = k_vowel;                            // 0..1
    int   ww = 26 + (int)(v * 44.0f);             // OO narrow -> EE wide
    float openf = 0.5f - 0.5f * cosf((v < 0.5f ? v * 2.0f : (1.0f - v) * 2.0f) * 3.14159f);
    int   oh = 6 + (int)(openf * 30.0f);          // AH (middle) opens widest, OO/EE close
    if (!on) { oh = 3; }
    for (int i = -ww; i <= ww; i++) {
        float t = (float)i / ww;
        int h = (int)(oh * (1.0f - t * t));
        if (h < 1) h = 1;
        int x = cx + i;
        line(x, cy - h, x, cy + h, (i & 1) ? CLR_DARK_RED : CLR_RED);
    }
    rect(cx - ww, cy - oh - 3, ww * 2, (oh + 3) * 2, CLR_MAUVE);
}

void draw(void) {
    char buf[48];
    cls(CLR_DARKER_BLUE);

    print("VOWEL", 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("a saw chord pushed through formant() - the vowel filter", 8, 18, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(20, 26, 280, 156, CLR_BLACK);
        rect(20, 26, 280, 156, CLR_MAUVE);
        print("VOWEL FILTER", 110, 34, CLR_LIGHT_YELLOW);
        static const char *HL[] = {
            "Four bandpasses parked at the human",
            "vowel frequencies make a plain saw",
            "chord SAY a vowel. Sweep VOWEL and the",
            "chord talks: OO-OH-AH-EH-EE.",
            "",
            "VOWEL  the talking knob       1..5 jump",
            "Q      pronounced/nasal",
            "MIX    dry..wet",
            "AUTO sweep [A]   BYPASS [Z]   H help",
        };
        for (int i = 0; i < 9; i++)
            print(HL[i], 32, 48 + i * 12, i < 4 ? CLR_LIGHT_PEACH : CLR_WHITE);
        ui_begin();
        if (ui_button(130, 166, 60, 14, "CLOSE")) show_help = false;
        ui_end();
        return;
    }

    draw_mouth(160, 78);
    int vi = (int)(k_vowel * 4.0f + 0.5f); if (vi < 0) vi = 0; if (vi > 4) vi = 4;
    print(on ? VNAME[vi] : "(bypass)", 160 - 12, 74, CLR_LIGHT_YELLOW);

    // a little vowel-path ruler under the mouth
    for (int i = 0; i < 5; i++) {
        int x = 70 + i * 45;
        print(VNAME[i], x, 120, (i == vi && on) ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    }
    int px = 70 + (int)(k_vowel * 4.0f * 45.0f) + 4;
    if (on) line(px, 132, px, 138, CLR_LIGHT_YELLOW);

    ui_begin();
    bool ch = false;
    if (ui_knob(&k_vowel, 60,  152, "VOWEL")) { ch = true; autop = false; }
    if (ui_knob(&k_q,     140, 152, "Q"))     ch = true;
    if (ui_knob(&k_mix,   220, 152, "MIX"))   ch = true;
    if (ch) apply_formant();
    if (ui_button(268, 134, 44, 22, on ? "ON" : "BYPASS")) { on = !on; apply_formant(); }
    if (ui_button(270, 30, 42, 12, autop ? "AUTO*" : "AUTO")) autop = !autop;
    if (ui_button(270, 46, 42, 12, "HELP")) show_help = true;
    ui_end();

    sprintf(buf, "VOWEL %d  Q %d  MIX %d", (int)(k_vowel * 100), (int)(k_q * 100), (int)(k_mix * 100));
    print(buf, 60, 184, CLR_MAUVE);
}
