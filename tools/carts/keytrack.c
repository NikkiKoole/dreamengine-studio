/* de:meta
{
  "slug": "keytrack",
  "title": "keytrack — the filter follows the keyboard",
  "status": "active",
  "created": "2026-07-29",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "subtractive-synth"
  ],
  "lineage": "Synth Secrets Part 6 (keyboard tracking) and Part 63's Juno 60 trick of playing a self-oscillating filter as an extra oscillator; the engine side is instrument_keytrack(), added for audit §B2.",
  "description": "KEYBOARD TRACKING, the thing every real subtractive synth has and this engine did not until now. A filter cutoff set in Hz is only correct at ONE pitch: play two octaves up and the same patch is a different sound, because the harmonics moved and the filter did not. instrument_keytrack(slot, amount) makes the cutoff FOLLOW the note - 0 = fixed Hz (how everything behaved before), 1 = the cutoff doubles per octave, and 0.93 is the classic musical setting Reid pins as '190 percent per octave'. With tracking on, the cutoff you pass is the value at C4 and scales from there. Press SPACE to hear it: a scale walks four octaves up and back while the graph draws the note pitch as dots and the CUTOFF as a line, so you watch the line either sit flat (fixed) or climb with the notes (tracking). Measured on this patch, a fixed cutoff moves the spectral centroid only 391 to 686 Hz across four octaves, while full tracking moves it 259 to 1535 Hz - the difference between a patch voiced for one register and one that holds its character everywhere. And the marquee, Reid's Juno 60 move: crank RESONANCE with tracking at 1.0 and the filter's own whistle becomes a playable pitched voice, a third oscillator out of a filter. 1/2/3 set tracking 0 / 0.93 / 1.0, UP/DOWN resonance, LEFT/RIGHT the C4 cutoff, SPACE the scale, A..K play by hand."
}
de:meta */
// keytrack — what keyboard tracking IS, in one screen (audit §B2 / Synth Secrets Part 6).
//
// The complaint: `instrument_filter(slot, FILTER_LOW, 400, r)` means 400 Hz at EVERY pitch. Play a bass
// note and the filter is above most of its harmonics (bright); play three octaves up and it is below the
// fundamental (muffled). So a patch is only voiced correctly in one register — which is why real synths
// all carry a "VCF Kybd" slider and this engine's patches quietly did not.
//
// The fix is one multiply at note-on: cutoff *= 2^(amount * (midi - 60)/12). amount 1 = the cutoff doubles
// per octave (true 1V/oct); 0.93 is Reid's musically-nice "190 percent"; 0 is the old behaviour.
//
//   1 / 2 / 3   tracking 0.00 / 0.93 / 1.00        SPACE   run the four-octave scale
//   UP / DOWN   resonance (crank it for the whistle)  LEFT/RIGHT  the cutoff AT C4
//   A..K        play by hand
#include "studio.h"
#define KEYBED_WHITE_KEYS "ASDFGHJK"
#define KEYBED_BLACK_KEYS "WE TYU"
#include "keybed.h"

#define SL 5
#define NKT 3
static const float KT[NKT]      = { 0.0f, 0.93f, 1.0f };
static const char *KT_NAME[NKT] = { "0.00  fixed Hz", "0.93  musical", "1.00  full 1V/oct" };
static int   kt_i  = 0;
static int   cut4  = 400;        // the cutoff AT C4, in Hz
static int   res   = 9;
static bool  running = true;
static int   pos = 0;           // scale position
static float last_beat = -1.0f;
static int   lit_midi = -1;      // the note currently sounding (for the graph)
static float hist_m[64], hist_c[64];   // rolling history: note pitch + the cutoff it produced
static int   hist_n = 0;

// A scale walking four octaves up and back — the RANGE is the whole point. Named PHRASE, not SCALE:
// SCALE is a -D compile flag (the window scale factor), so `int SCALE[30]` expands to `int 4[30]`.
#define NSTEP 30
static const int PHRASE[NSTEP] = {
    36,38,40,43,45, 48,50,52,55,57, 60,62,64,67,69, 72,74,76,79,81,
    84,81,79,76,74, 72,69,67,64,60
};

static void apply(void) {
    instrument(SL, INSTR_SAW, 2, 0, 7, 90);
    instrument_filter(SL, FILTER_LADDER, cut4, res);
    instrument_keytrack(SL, KT[kt_i]);
}

// what the engine will use for this note — the same maths as sound_setup_note, so the graph cannot lie
static float cutoff_for(int midi) {
    return cut4 * powf(2.0f, KT[kt_i] * (midi - 60) / 12.0f);
}

static void strike(int midi) {
    hit(midi, SL, 6, 420);
    lit_midi = midi;
    if (hist_n < 64) hist_n++;
    for (int i = 63; i > 0; i--) { hist_m[i] = hist_m[i-1]; hist_c[i] = hist_c[i-1]; }
    hist_m[0] = (float)midi; hist_c[0] = cutoff_for(midi);
}

void kb_strike(int midi, int vel) { (void)vel; strike(midi); }

void init(void) {
    apply();
    keybed_config(SL, 4, 8);
    keybed_layout(8, 150, SCREEN_W - 16, 44);
    keybed_manage_voices(false);
    keybed_on_note(kb_strike);
    bpm(150);
}

void update(void) {
    keybed_update();
    for (int i = 0; i < NKT; i++)
        if (keyp('1' + i) || tapp(8 + i * 96, 30, 92, 13)) { kt_i = i; apply(); }
    if (keyp(KEY_UP))    { res  = res  < 15 ? res + 1 : 15;   apply(); }
    if (keyp(KEY_DOWN))  { res  = res  > 0  ? res - 1 : 0;    apply(); }
    if (keyp(KEY_RIGHT)) { cut4 = cut4 < 4000 ? cut4 + 100 : 4000; apply(); }
    if (keyp(KEY_LEFT))  { cut4 = cut4 >  100 ? cut4 - 100 :  100; apply(); }
    if (keyp(KEY_SPACE)) running = !running;

    if (running) {                      // one note per 8th
        float b = beat() * 2.0f;
        if (last_beat < 0 || (int)b != (int)last_beat) {
            strike(PHRASE[pos % NSTEP]);
            pos++;
        }
        last_beat = b;
    }
}

// ---- drawing ----------------------------------------------------------------
#define GX 8
#define GY 46
#define GW (SCREEN_W - 16)
#define GH 96

static int y_for_hz(float hz) {          // log frequency, 50Hz..8kHz
    float t = (logf(hz) - logf(50.0f)) / (logf(8000.0f) - logf(50.0f));
    if (t < 0) t = 0; else if (t > 1) t = 1;
    return GY + GH - 1 - (int)(t * (GH - 2));
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("KEYTRACK", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("the filter follows the keyboard", 84, 9, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    for (int i = 0; i < NKT; i++) {      // the three tracking buttons
        bool on = (i == kt_i);
        rectfill(8 + i * 96, 30, 92, 13, on ? CLR_DARK_BLUE : CLR_DARKER_GREY);
        rect(8 + i * 96, 30, 92, 13, on ? CLR_BLUE : CLR_DARK_GREY);
        font(FONT_SMALL);
        print(str("%d %s", i + 1, KT_NAME[i]), 12 + i * 96, 33, on ? CLR_WHITE : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }

    rectfill(GX, GY, GW, GH, CLR_BLACK);
    rect(GX, GY, GW, GH, CLR_DARK_GREY);
    font(FONT_TINY);
    print("8k", GX + 2, GY + 2, CLR_DARKER_GREY);
    print("50", GX + 2, GY + GH - 8, CLR_DARKER_GREY);
    font(FONT_NORMAL);

    // history, newest on the right: the note's pitch as a dot, the CUTOFF as a line
    int n = hist_n < 40 ? hist_n : 40;
    for (int i = 0; i < n; i++) {
        int x = GX + GW - 6 - i * ((GW - 12) / 40);
        float f = 440.0f * powf(2.0f, (hist_m[i] - 69) / 12.0f);
        pset(x, y_for_hz(f), CLR_LIGHT_PEACH);                 // the note
        int cy = y_for_hz(hist_c[i]);
        rectfill(x - 1, cy, 3, 2, CLR_LIME_GREEN);             // the cutoff it produced
        if (i > 0) {                                           // join the cutoffs so tracking READS as a slope
            int px = x + ((GW - 12) / 40), py = y_for_hz(hist_c[i-1]);
            line(x, cy, px, py, CLR_DARK_GREEN);
        }
    }
    font(FONT_SMALL);
    print("dots = note pitch", GX + 26, GY + GH - 18, CLR_LIGHT_PEACH);
    print("line = cutoff", GX + 26, GY + GH - 10, CLR_LIME_GREEN);
    print(str("cutoff@C4 %d Hz   res %d   %s", cut4, res, running ? "SCALE" : "held"),
          GX + 110, GY + GH - 10, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    keybed_draw();
    hint("1/2/3 tracking   UP/DOWN res   LEFT/RIGHT cutoff   SPACE scale   A..K play");
}
