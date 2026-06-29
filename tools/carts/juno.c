/* de:meta
{
  "title": "juno-6",
  "status": "active",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling",
    "chord-voicing"
  ],
  "lineage": "Roland Juno-6 (1982) recreation; sibling of sh101 but the BBD bucket-brigade chorus is the whole lesson — the dry poly pad is unremarkable, the chorus IS the instrument.",
  "homage": "Roland Juno-6 (1982)",
  "description": "Roland Juno-6 (1982) - the showcase for chorus(), the engines master BBD chorus. On paper a plain 6-voice poly synth (one saw + sub oscillator, one resonant lowpass) - sh101 with more voices. What makes it a JUNO is the bucket-brigade CHORUS: flip the famous OFF/I/II switch and the dry mono synth smears into a lush, wide, shimmering stereo wash (a centered mono chord literally fans out across the stereo field - the Junos two amps). Tap the CHORUS strip (or press C) to A/B it; the WIDTH meter opens as the chorus widens the signal. RATE/DEPTH/MIX ride the chorus live, CUTOFF/RES shape the resonant lowpass. Tap a chord pad or press 1..4 (Cmaj7/Am7/Fmaj7/G7); SPACE toggles AUTO, a slow self-playing progression; H help."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── JUNO-6 ─────────────────────────────────────────────────────────────────
// Roland's Juno-6 (1982): a 6-voice poly synth — one saw/pulse osc + a sub
// oscillator, one resonant lowpass — that on paper is "sh101 with more voices."
// What made it a Juno was the BBD CHORUS: a bucket-brigade modulated delay on
// the output that smeared the dry mono synth into a lush, wide, shimmering
// stereo wash. Flip it off and you hear the (perfectly nice) bare synth; flip
// it on (I or II) and it's unmistakably a Juno. This cart is the showcase for
// chorus() — the first use of the engine's shared modulated-delay buffer
// (instrument-engines §8.10 / decision 0015): the effect IS the instrument.
//
//   CHORUS   off / I (slow, lush) / II (faster, deeper) — the famous switch.
//            I/II snap RATE+DEPTH to the classic presets; ride them yourself.
//   RATE     chorus LFO speed       DEPTH  sweep amount     MIX  dry..wet
//   CUTOFF / RES   the resonant lowpass — shape the pad
//   PADS     four lush chords; tap one (or press 1..4) to play it
//   SPACE    AUTO — a slow chord progression plays itself
//   the WIDTH meter opens as the chorus widens the mono synth into stereo.

#define SLOT 8

// knobs (ui.h floats 0..1)
static float k_cut  = 0.45f;   // cutoff
static float k_res  = 0.30f;   // resonance
static float k_rate = 0.10f;   // chorus rate  (I preset)
static float k_dep  = 0.45f;   // chorus depth (I preset)
static float k_mix  = 0.50f;   // chorus dry/wet

static int   chmode = 1;       // 0 off, 1 = CHORUS I, 2 = CHORUS II (on at boot — it's a Juno)
static bool  autop  = true;
static bool  show_help;
static int   last_step = -1, auto_i = 0;
static float width = 0.0f;     // cosmetic stereo-width meter

// four lush Juno chords (maj7 / min7 voicings around C4)
static const int CHORD[4][4] = {
    { 60, 64, 67, 71 },   // Cmaj7
    { 57, 60, 64, 67 },   // Am7
    { 53, 57, 60, 64 },   // Fmaj7
    { 55, 59, 62, 65 },   // G7
};
static const char *CNAME[4] = { "Cmaj7", "Am7", "Fmaj7", "G7" };

static int cut_hz(void) { return 200 + (int)(k_cut * k_cut * 5800.0f); }   // 200..6000, squared feel
static int res_v(void)  { return (int)(k_res * 12.0f); }                    // 0..12

static void apply_voice(void) {
    instrument(SLOT, INSTR_SAW, 24, 0, 6, 460);      // warm poly pad
    instrument_filter(SLOT, FILTER_LOW, cut_hz(), res_v());
}
static void apply_chorus(void) {
    float rate = 0.1f + k_rate * 4.9f;               // 0.1..5 Hz
    chorus(rate, k_dep, chmode ? k_mix : 0.0f);      // mode 0 → mix 0 = bypass
}
static void set_mode(int m) {
    chmode = m;
    if (m == 1) { k_rate = 0.10f; k_dep = 0.45f; }   // I — slow, lush
    if (m == 2) { k_rate = 0.22f; k_dep = 0.62f; }   // II — faster, deeper
    apply_chorus();
}

static void play_chord(int i) {
    for (int n = 0; n < 4; n++) hit(CHORD[i][n], SLOT, 5, 620);
    hit(CHORD[i][0] - 12, SLOT, 3, 620);             // sub-oscillator (octave down, root)
}

void init(void) {
    bpm(56);
    apply_voice();
    apply_chorus();
}

void update(void) {
    if (keyp(KEY_SPACE)) { autop = !autop; last_step = -1; }
    if (keyp('H')) show_help = !show_help;
    if (keyp('C')) set_mode((chmode + 1) % 3);                 // cycle the chorus switch
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) play_chord(i);

    if (autop) {
        int step = beat();
        if (step != last_step) {
            last_step = step;
            if ((step & 1) == 0) { play_chord(auto_i); auto_i = (auto_i + 1) % 4; }
        }
    }

    // cosmetic width meter: opens toward the chorus depth when engaged
    float target = chmode ? k_dep * k_mix : 0.0f;
    width += (target - width) * 0.08f;
}

void draw(void) {
    char buf[48];
    cls(CLR_DARKER_BLUE);

    // title
    print("JUNO-6", 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("polyphonic synthesizer  -  chorus showcase", 8, 18, CLR_INDIGO);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(24, 26, 272, 150, CLR_BLACK);
        rect(24, 26, 272, 150, CLR_INDIGO);
        print("JUNO-6", 130, 34, CLR_LIGHT_YELLOW);
        static const char *HL[] = {
            "A plain saw+sub+lowpass poly synth -",
            "until the CHORUS. Flip it OFF: nice,",
            "bare, mono. Flip it I/II: lush, wide,",
            "shimmering - THAT is the Juno.",
            "",
            "CHORUS  off / I (slow) / II (deep)  [C]",
            "RATE DEPTH MIX   the chorus itself",
            "CUTOFF RES       the lowpass",
            "1..4    play a chord    SPACE  AUTO",
        };
        for (int i = 0; i < 9; i++)
            print(HL[i], 36, 48 + i * 12, i < 4 ? CLR_LIGHT_PEACH : CLR_WHITE);
        ui_begin();
        if (ui_button(130, 158, 60, 14, "CLOSE")) show_help = false;
        ui_end();
        return;
    }

    // ── control panel ──────────────────────────────────────────────────────
    rectfill(8, 30, 304, 70, CLR_DARK_PURPLE);
    rect(8, 30, 304, 70, CLR_INDIGO);

    // the CHORUS switch — three LEDs (off / I / II); tap the strip to cycle (or press C)
    print("CHORUS", 16, 36, CLR_LIGHT_PEACH);
    static const char *ML[3] = { "OFF", "I", "II" };
    if (tapp(14, 48, 80, 18)) set_mode((chmode + 1) % 3);
    for (int m = 0; m < 3; m++) {
        bool on = (chmode == m);
        int x = 16 + m * 26;
        rectfill(x, 50, 22, 14, on ? (m ? CLR_ORANGE : CLR_DARK_GREY) : CLR_BROWNISH_BLACK);
        rect(x, 50, 22, 14, CLR_INDIGO);
        print(ML[m], x + 11 - (int)strlen(ML[m]) * 4, 53, on ? CLR_BLACK : CLR_INDIGO);
    }
    print("tap / [C]", 16, 70, CLR_INDIGO);

    // ── width meter — the mono synth fanning out to stereo ─────────────────
    int wx = 16, wy = 84, ww = 78;
    print("WIDTH", wx, 84, CLR_INDIGO);
    rectfill(wx, wy + 8, ww, 6, CLR_BROWNISH_BLACK);
    int wfill = (int)(width * 3.0f * ww); if (wfill > ww) wfill = ww;
    rectfill(wx, wy + 8, wfill, 6, CLR_BLUE);

    ui_begin();
    bool ch = false;
    if (ui_knob(&k_cut,  120, 58, "CUT"))   apply_voice();
    if (ui_knob(&k_res,  160, 58, "RES"))   apply_voice();
    if (ui_knob(&k_rate, 212, 58, "RATE"))  ch = true;
    if (ui_knob(&k_dep,  256, 58, "DEPTH")) ch = true;
    if (ui_knob(&k_mix,  298, 58, "MIX"))   ch = true;
    if (ch) apply_chorus();
    if (ui_button(248, 32, 30, 12, autop ? "AUTO*" : "AUTO")) { autop = !autop; last_step = -1; }
    if (ui_button(282, 32, 30, 12, "HELP")) show_help = true;
    ui_end();

    // ── chord pads ─────────────────────────────────────────────────────────
    for (int i = 0; i < 4; i++) {
        int x = 8 + i * 78, y = 150, w = 74, h = 40;
        if (tapp(x, y, w, h)) play_chord(i);
        rectfill(x, y, w, h, CLR_INDIGO);
        rect(x, y, w, h, CLR_BLUE);
        print(CNAME[i], x + w / 2 - (int)strlen(CNAME[i]) * 4, y + h / 2 - 4, CLR_LIGHT_PEACH);
        font(FONT_SMALL);
        sprintf(buf, "%d", i + 1);
        print(buf, x + 3, y + 3, CLR_BLUE);
        font(FONT_NORMAL);
    }

    sprintf(buf, "CUT %dHz  RES %d", cut_hz(), res_v());
    print(buf, 130, 86, CLR_INDIGO);
}
