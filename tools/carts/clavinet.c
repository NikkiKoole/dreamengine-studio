/* de:meta
{
  "slug": "clavinet",
  "title": "clavinet",
  "status": "active",
  "created": "2026-06-11",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling",
    "adsr-envelope"
  ],
  "lineage": "Sixth in the effects-showcase family (spacecho/cathedral/juno/mistress/tapeloop), modelling the Hohner Clavinet D6 through a bus-side envelope-follower auto-wah — novel because the wah had to land on the summed bus, not per-note.",
  "homage": "Hohner Clavinet D6 + auto-wah",
  "description": "Hohner Clavinet D6 (1971) through an AUTO-WAH - the funk machine, and the showcase for wah(). A clavinet is a bright, short, percussive plucked-reed keyboard; run it into an envelope filter and you get the Superstition / Higher Ground talking wakka-wakka: the filter OPENS on each hard stab and quacks shut between them, following how hard you play. This is the realistic auto-wah, which (the scar) had to be a BUS effect - an envelope follower on the summed signal, not a per-note sweep, so it pumps with the groove. Sixth box in the effects family (spacecho=echo, cathedral=reverb, juno=chorus, mistress=flanger, tapeloop=tape, clavinet=auto-wah). A syncopated 16th C-minor funk riff plays itself; the WAH footswitch A/Bs the bare clav vs the talking wah; SENS sets how much your playing opens the filter, RES is the quack, MIX is dry..wet. The on-screen MOUTH opens with the wah (ooo->aah) - harder stabs, wider vowel. WAH footswitch [W], SENS/RES/MIX knobs, SPACE toggles the AUTO riff, 1..6 stab a note by hand, H help."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── CLAVINET ───────────────────────────────────────────────────────────────
// Hohner Clavinet D6 (1971) through an auto-wah — the funk machine. Stevie
// Wonder's "Superstition" / "Higher Ground", every clav-funk record: a short,
// bright, percussive plucked-reed keyboard run into an ENVELOPE filter, so the
// filter OPENS on each hard stab and quacks shut between them. That talking
// "wakka-wakka" is the showcase for wah() — the realistic auto-wah, which had
// to be a BUS effect (an envelope follower on the summed signal, not a per-note
// sweep — the scar; instrument-engines §8.10 / decision 0015). Sixth box in the
// effects family: spacecho · cathedral · juno · mistress · tapeloop · clavinet.
//
//   WAH       footswitch — the auto-wah on/off (hear the clav bare vs talking)
//   SENS      how much your playing opens the filter (the envelope sensitivity)
//   RES       the quack (resonance) · MIX  dry..wet
//   SPACE     AUTO — a syncopated 16th funk riff plays itself
//   1..6      stab a note by hand   ·   H help
//   the MOUTH opens with the wah; the harder the hit, the wider the vowel.

#define CLAV 8

static float k_sens = 0.62f;
static float k_res  = 0.70f;
static float k_mix  = 0.85f;
static bool  on     = true;     // wah engaged at boot — it's the point
static bool  autop  = true;
static bool  show_help;
static int   last16 = -1;
static float mouth  = 0.0f;     // cosmetic: tracks recent stabs → the wah-mouth opening

// a syncopated C-minor funk riff (16 sixteenths) — root/b3/4/5/b7, lots of space
static const int RIFF[16] = { 48,-1,48,51, -1,48,-1,53,  -1,55,-1,58, 55,-1,51,-1 };
// hand-stab pool (1..6): the same minor-pentatonic pool
static const int POOL[6] = { 48, 51, 53, 55, 58, 60 };

static void apply_wah(void) { instrument_wah(CLAV, k_sens, k_res, on ? k_mix : 0.0f); }

static void stab(int midi) {
    hit(midi, CLAV, 6, 150);    // short, percussive clav stab
    mouth = 1.0f;               // pop the mouth open
}

void init(void) {
    bpm(104);                                      // funky mid-tempo
    instrument(CLAV, INSTR_EPIANO, 2, 140, 2, 120);// short percussive plucked-reed
    instrument_harmonics(CLAV, 0.85f);             // clav voicing (epiano/clav recipe)
    instrument_timbre(CLAV, 0.75f);
    instrument_morph(CLAV, 0.55f);
    apply_wah();
}

void update(void) {
    if (keyp(KEY_SPACE)) { autop = !autop; last16 = -1; }
    if (keyp('H')) show_help = !show_help;
    if (keyp('W')) { on = !on; apply_wah(); }
    for (int i = 0; i < 6; i++) if (keyp('1' + i)) stab(POOL[i]);

    if (autop) {
        int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
        if (s16 != last16) {
            last16 = s16;
            int n = RIFF[s16 & 15];
            if (n >= 0) stab(n);
        }
    }
    mouth *= 0.88f;             // the mouth relaxes shut between stabs
}

void draw(void) {
    char buf[44];
    cls(CLR_DARKER_PURPLE);

    print("CLAVINET", 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("hohner D6 + auto-wah  -  wah() showcase", 8, 18, CLR_MAUVE);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(24, 26, 272, 150, CLR_BLACK);
        rect(24, 26, 272, 150, CLR_MAUVE);
        print("CLAVINET", 120, 34, CLR_LIGHT_YELLOW);
        static const char *HL[] = {
            "A bright percussive clav into an auto-",
            "wah: the filter OPENS on each hard stab",
            "and quacks shut between them - the funk",
            "'wakka-wakka' talk.",
            "",
            "WAH   footswitch on/off            [W]",
            "SENS  how much playing opens it",
            "RES   the quack    MIX  dry..wet",
            "SPACE AUTO riff    1..6 stab    H help",
        };
        for (int i = 0; i < 9; i++)
            print(HL[i], 36, 48 + i * 12, i < 4 ? CLR_LIGHT_PEACH : CLR_WHITE);
        ui_begin();
        if (ui_button(130, 162, 60, 14, "CLOSE")) show_help = false;
        ui_end();
        return;
    }

    // ── the wah-mouth: a vowel that opens with the envelope (ooo → aah) ─────
    int cx = 160, cy = 80, ww = 70;
    int oh = 4 + (int)(mouth * (on ? 34.0f : 0.0f));        // mouth height tracks the wah
    for (int i = -ww; i <= ww; i++) {                       // a lens/mouth shape
        float t = (float)i / ww;
        int h = (int)(oh * (1.0f - t * t));
        if (h < 1) h = 1;
        int x = cx + i;
        line(x, cy - h, x, cy + h, i & 1 ? CLR_DARK_RED : CLR_RED);
    }
    rect(cx - ww, cy - oh - 2, ww * 2, (oh + 2) * 2, CLR_MAUVE);
    print(on ? (mouth > 0.5f ? "AAH" : "wah") : "(bypass)", cx - 12, cy - 4, CLR_LIGHT_YELLOW);

    // ── knobs + footswitch ─────────────────────────────────────────────────
    ui_begin();
    bool ch = false;
    if (ui_knob(&k_sens, 60,  150, "SENS")) ch = true;
    if (ui_knob(&k_res,  140, 150, "RES"))  ch = true;
    if (ui_knob(&k_mix,  220, 150, "MIX"))  ch = true;
    if (ch) apply_wah();
    if (ui_button(268, 132, 44, 22, on ? "WAH" : "BYPASS")) { on = !on; apply_wah(); }
    if (ui_button(270, 30, 42, 12, autop ? "AUTO*" : "AUTO")) { autop = !autop; last16 = -1; }
    if (ui_button(270, 46, 42, 12, "HELP")) show_help = true;
    ui_end();

    sprintf(buf, "SENS %d  RES %d  MIX %d", (int)(k_sens * 100), (int)(k_res * 100), (int)(k_mix * 100));
    print(buf, 60, 182, CLR_MAUVE);
}
