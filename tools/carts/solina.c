#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── SOLINA ─────────────────────────────────────────────────────────────────
// The ARP / Eminent Solina String Ensemble (1974): the string machine. Not a
// synth with voices to steal — a divide-down ORGAN. Every key you hold sounds
// (fully polyphonic, paraphonic), each one a stack of sawtooths tapped at
// different FOOTAGES: Contrabass 16', Cello/Viola 8', Violin 4'. Bare, those
// stacked saws are static and buzzy. The magic is the ENSEMBLE — a triple BBD
// chorus (three modulated delays, LFOs 120 deg apart) that smears the static
// saws into the lush, breathing, unmistakable string wash heard on every '70s
// record. Flip ENSEMBLE off: a buzzy organ. Flip it on: a Solina. This is the
// chorus() companion to JUNO-6 — there the chorus widens a synth; HERE the
// chorus IS the instrument's entire identity.
//
//   TABS     six voice tabs - tap to toggle. Strings (blue) 16'/8'/8'/4',
//            brass (orange) 8'/4'. Enabled tabs all sound = the divide-down mix.
//   PLAY     bottom 2 octaves - touch, or QWERTY (Z..M lower, Q..U upper).
//            HOLD a key: it swells in (CRESCENDO) and rings out slowly on lift.
//   CRESCENDO  the swell-in (attack)      SUSTAIN  the ring-out (release)
//   BRIGHT     master tone (all footages) ENSEMBLE off / I / II   [E]
//   SPACE      AUTO - a slow string progression plays itself.

#define NTAB      6
#define SLOT0     8                 // tab slots 8..13
#define BASE_MIDI 48                // C3; keyboard spans 2 octaves up
#define NSEMI     24
#define NWHITE    14                // 2 octaves of white keys
#define KEY_TOP   132
#define WKW       (SCREEN_W / NWHITE)
#define BLACK_W   16
#define BLACK_H   42
#define NOFINGER  (-999)

typedef struct {
    const char *name, *foot;
    int oct;        // footage transpose (semitones): 16'=-12, 8'=0, 4'=+12
    int cut;        // base lowpass cutoff (Hz) — the tab's brightness
    int brass;      // brass tabs get a touch of drive + a different colour
} Tab;

static const Tab TAB[NTAB] = {
    { "CONTRA", "16'", -12,  900, 0 },   // Contrabass — dark sub-octave bed
    { "CELLO",  "8'",    0, 1500, 0 },   // Cello — warm 8'
    { "VIOLA",  "8'",    0, 2400, 0 },   // Viola — mid 8'
    { "VIOLIN", "4'",   12, 3600, 0 },   // Violin — bright octave-up
    { "TRMPT",  "8'",    0, 3200, 1 },   // Trumpet — brassy 8'
    { "HORN",   "4'",   12, 2600, 1 },   // Horn — brassy octave-up
};
static int tab_on[NTAB] = { 0, 1, 1, 1, 0, 0 };   // classic string patch: Cello+Viola+Violin

// knobs (ui.h floats 0..1)
static float k_cresc  = 0.40f;     // attack swell
static float k_sust   = 0.45f;     // release tail
static float k_bright = 0.50f;     // master tone

static int   ens   = 1;            // ENSEMBLE: 0 off, 1 = I (lush), 2 = II (deep). On at boot — it's a Solina.
static bool  autop = true;         // plays itself on load — a string machine wants to swell
static bool  show_help;
static int   last_step = -1, auto_i = 0;

// per-semitone polyphony: a note handle per (key, tab), -1 = none
static int  h_note[NSEMI][NTAB];
static bool kbd[NSEMI];            // held by a computer key
static int  tfinger[NSEMI];        // touch id holding this key, or NOFINGER

// keyboard geometry
static const int wsemi[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int bsemi[7] = { 1, 3, -1, 6, 8, 10, -1 };

// QWERTY → semitone (tracker layout, two octaves)
typedef struct { int code, semi; } KMap;
static const KMap KEYS[] = {
    {'Z',0},{'S',1},{'X',2},{'D',3},{'C',4},{'V',5},{'G',6},{'B',7},{'H',8},{'N',9},{'J',10},{'M',11},
    {'Q',12},{'2',13},{'W',14},{'3',15},{'E',16},{'R',17},{'5',18},{'T',19},{'6',20},{'Y',21},{'7',22},{'U',23},
};
#define NKEYS ((int)(sizeof(KEYS)/sizeof(KEYS[0])))

static int attack_ms(void)  { return 60  + (int)(k_cresc * 1340.0f); }   // 60..1400
static int release_ms(void) { return 300 + (int)(k_sust  * 3200.0f); }   // 300..3500
static float bright_mul(void){ return 0.5f + k_bright * 1.1f; }          // 0.5..1.6

static void apply_voices(void) {
    for (int t = 0; t < NTAB; t++) {
        int s = SLOT0 + t;
        instrument(s, INSTR_SAW, attack_ms(), 300, 6, release_ms());
        int cut = (int)(TAB[t].cut * bright_mul());
        instrument_filter(s, FILTER_LOW, cut, 2);
        instrument_tune(s, 0.05f + t * 0.005f);              // per-tab detune spread = divide-down beating
        instrument_lfo(s, 0, LFO_PITCH, 0.16f, 0.04f);       // slow tape-style wow under the ensemble
        instrument_drive(s, TAB[t].brass ? 0.20f : 0.0f);    // brass tabs bite a little
    }
}

static void apply_ensemble(void) {
    // The triple-BBD ensemble. Off = mix 0 (bare divide-down organ).
    if (ens == 0)      chorus(1.0f, 0.40f, 0.0f);
    else if (ens == 1) chorus(0.9f, 0.50f, 0.60f);           // I  — the classic lush swirl
    else               chorus(1.6f, 0.70f, 0.72f);           // II — deeper, faster, drenched
}

static bool sounding(int k) { return kbd[k] || tfinger[k] != NOFINGER; }

static void voice_start(int k) {
    for (int t = 0; t < NTAB; t++) {
        if (h_note[k][t] != -1) continue;                    // already on
        if (!tab_on[t]) continue;
        h_note[k][t] = note_on(BASE_MIDI + k + TAB[t].oct, SLOT0 + t, 5);
    }
}
static void voice_stop(int k) {
    if (sounding(k)) return;                                  // still held by the other source
    for (int t = 0; t < NTAB; t++)
        if (h_note[k][t] != -1) { note_off(h_note[k][t]); h_note[k][t] = -1; }
}

// which semitone (0..23) is under canvas point x,y? -1 = none
static int semi_at(int x, int y) {
    if (y < KEY_TOP || y >= SCREEN_H) return -1;
    int wk = x / WKW;
    if (wk < 0) wk = 0;
    if (wk >= NWHITE) wk = NWHITE - 1;
    if (y < KEY_TOP + BLACK_H) {
        for (int k = wk - 1; k <= wk; k++) {
            if (k < 0 || k >= NWHITE) continue;
            int pos = k % 7;
            if (bsemi[pos] < 0) continue;
            int bx = (k + 1) * WKW - BLACK_W / 2;
            if (x >= bx && x < bx + BLACK_W) return (k / 7) * 12 + bsemi[pos];
        }
    }
    return (wk / 7) * 12 + wsemi[wk % 7];
}

static void play_chord_auto(const int *notes, int n, int dur) {
    for (int i = 0; i < n; i++)
        for (int t = 0; t < NTAB; t++)
            if (tab_on[t]) hit(notes[i] + TAB[t].oct, SLOT0 + t, 4, dur);
}

void init(void) {
    bpm(50);
    for (int k = 0; k < NSEMI; k++) { tfinger[k] = NOFINGER; for (int t = 0; t < NTAB; t++) h_note[k][t] = -1; }
    apply_voices();
    apply_ensemble();
}

void update(void) {
    if (keyp(KEY_SPACE)) { autop = !autop; last_step = -1; if (!autop) note_off_all(); }
    if (keyp(KEY_TAB) || keyp('/')) show_help = !show_help;

    // computer keyboard — press = note_on, release = note_off (true holds)
    for (int i = 0; i < NKEYS; i++) {
        int k = KEYS[i].semi;
        if (keyp(KEYS[i].code) && !kbd[k]) { kbd[k] = true;  voice_start(k); }
        if (keyr(KEYS[i].code) &&  kbd[k]) { kbd[k] = false; voice_stop(k);  }
    }

    // touch — claim keys, glissando on slide, release on lift
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        int s  = semi_at(touch_x(i), touch_y(i));
        int cur = -1;
        for (int k = 0; k < NSEMI; k++) if (tfinger[k] == id) { cur = k; break; }
        if (s == cur) continue;
        if (cur >= 0) { tfinger[cur] = NOFINGER; voice_stop(cur); }
        if (s >= 0 && tfinger[s] == NOFINGER) { tfinger[s] = id; voice_start(s); }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        int id = touch_ended_id(i);
        for (int k = 0; k < NSEMI; k++)
            if (tfinger[k] == id) { tfinger[k] = NOFINGER; voice_stop(k); }
    }

    // AUTO — a slow I–vi–IV–V string progression (Am: Am F C G), fully sustained
    if (autop) {
        static const int PROG[4][3] = {
            { 57, 60, 64 },   // Am
            { 53, 57, 60 },   // F
            { 48, 55, 60 },   // C
            { 55, 59, 62 },   // G
        };
        int step = beat();
        if (step != last_step) {
            last_step = step;
            if ((step & 3) == 0) { play_chord_auto(PROG[auto_i], 3, 3400); auto_i = (auto_i + 1) % 4; }
        }
    }
}

static void draw_help(void) {
    rectfill(20, 22, 280, 168, CLR_BROWNISH_BLACK);
    rect(20, 22, 280, 168, CLR_DARK_ORANGE);
    print("SOLINA  string ensemble", 96, 30, CLR_LIGHT_YELLOW);
    static const char *HL[] = {
        "A divide-down ORGAN, not a synth: every",
        "key sounds, each a stack of sawtooths at",
        "footages (16'/8'/4'). Bare = a buzzy organ.",
        "",
        "The ENSEMBLE is a triple BBD chorus that",
        "smears those saws into THE string wash.",
        "Flip it OFF then I/II - that is the Solina.",
        "",
        "TABS    tap to mix the footages",
        "PLAY    touch / QWERTY (Z..M, Q..U); HOLD",
        "        a key to hear it swell in + ring out",
        "ENSEMBLE off/I/II [E]   SPACE  AUTO",
    };
    for (int i = 0; i < 12; i++)
        print(HL[i], 30, 44 + i * 11, (i == 4 || i == 5 || i == 6) ? CLR_LIGHT_PEACH : CLR_WHITE);
    ui_begin();
    if (ui_button(130, 172, 60, 14, "CLOSE")) show_help = false;
    ui_end();
}

void draw(void) {
    char buf[48];
    cls(CLR_BROWNISH_BLACK);

    // ── wood-and-cream chassis ───────────────────────────────────────────────
    rectfill(0, 0, SCREEN_W, KEY_TOP, CLR_MEDIUM_GREY);       // warm beige panel
    rectfill(0, 0, 8, KEY_TOP, CLR_DARK_BROWN);               // wood end-cheeks
    rectfill(SCREEN_W - 8, 0, 8, KEY_TOP, CLR_DARK_BROWN);

    print("SOLINA", 14, 6, CLR_BROWNISH_BLACK);
    font(FONT_SMALL);
    print("STRING ENSEMBLE", 70, 9, CLR_DARK_BROWN);
    font(FONT_NORMAL);

    if (show_help) { draw_help(); return; }

    // ── voice tabs (rocker switches) ─────────────────────────────────────────
    int tw = 49, tx0 = 12, ty = 22, th = 30;
    for (int t = 0; t < NTAB; t++) {
        int x = tx0 + t * tw;
        if (tapp(x, ty, tw - 3, th)) { tab_on[t] = !tab_on[t]; }
        int base = TAB[t].brass ? CLR_DARK_RED : CLR_DARKER_BLUE;
        int lit  = TAB[t].brass ? CLR_DARK_ORANGE : CLR_TRUE_BLUE;
        rectfill(x, ty, tw - 3, th, CLR_BROWNISH_BLACK);
        rectfill(x + 1, ty + 1, tw - 5, th - 2, tab_on[t] ? lit : base);
        rect(x, ty, tw - 3, th, CLR_BROWNISH_BLACK);
        // lit highlight strip across the top of an engaged tab
        if (tab_on[t]) rectfill(x + 2, ty + 2, tw - 7, 3, CLR_LIGHT_PEACH);
        font(FONT_SMALL);
        int nc = (int)strlen(TAB[t].name);
        print(TAB[t].name, x + (tw - 3) / 2 - nc * 2, ty + 11, tab_on[t] ? CLR_WHITE : CLR_MEDIUM_GREY);
        print(TAB[t].foot, x + (tw - 3) / 2 - (int)strlen(TAB[t].foot) * 2, ty + 20,
              tab_on[t] ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
        font(FONT_NORMAL);
    }

    // ── ENSEMBLE switch — the star ───────────────────────────────────────────
    int ex = 14, ey = 62;
    print("ENSEMBLE", ex, ey, CLR_BROWNISH_BLACK);
    static const char *EL[3] = { "OFF", "I", "II" };
    if (tapp(ex, ey + 12, 86, 18)) { ens = (ens + 1) % 3; apply_ensemble(); }
    for (int m = 0; m < 3; m++) {
        bool on = (ens == m);
        int x = ex + m * 28;
        rectfill(x, ey + 14, 24, 14, on ? (m ? CLR_DARK_ORANGE : CLR_DARK_GREY) : CLR_DARKER_GREY);
        rect(x, ey + 14, 24, 14, CLR_BROWNISH_BLACK);
        print(EL[m], x + 12 - (int)strlen(EL[m]) * 4, ey + 17, on ? CLR_BLACK : CLR_MEDIUM_GREY);
    }
    font(FONT_SMALL);
    print(ens ? "the BBD string wash" : "bare divide-down", ex, ey + 31, CLR_DARK_BROWN);
    font(FONT_NORMAL);

    // ── knobs + buttons ──────────────────────────────────────────────────────
    ui_begin();
    bool v = false;
    if (ui_knob(&k_cresc,  150, 86, "CRESC")) v = true;
    if (ui_knob(&k_sust,   200, 86, "SUST"))  v = true;
    if (ui_knob(&k_bright, 250, 86, "TONE"))  v = true;
    if (v) apply_voices();
    if (ui_button(276, 64, 32, 14, autop ? "AUTO*" : "AUTO")) { autop = !autop; last_step = -1; if (!autop) note_off_all(); }
    if (ui_button(276, 82, 32, 14, "HELP")) show_help = true;
    ui_end();

    font(FONT_SMALL);
    sprintf(buf, "ATK %dms  REL %dms", attack_ms(), release_ms());
    print(buf, 150, 118, CLR_DARK_BROWN);
    font(FONT_NORMAL);

    // ── keyboard ─────────────────────────────────────────────────────────────
    for (int wk = 0; wk < NWHITE; wk++) {
        int s = (wk / 7) * 12 + wsemi[wk % 7];
        int x = wk * WKW;
        bool on = sounding(s);
        rectfill(x, KEY_TOP, WKW - 1, SCREEN_H - KEY_TOP, on ? CLR_LIGHT_YELLOW : CLR_WHITE);
        rect(x, KEY_TOP, WKW - 1, SCREEN_H - KEY_TOP, CLR_DARK_GREY);
    }
    for (int wk = 0; wk < NWHITE; wk++) {
        int pos = wk % 7;
        if (bsemi[pos] < 0) continue;
        int s = (wk / 7) * 12 + bsemi[pos];
        int bx = (wk + 1) * WKW - BLACK_W / 2;
        bool on = sounding(s);
        rectfill(bx, KEY_TOP, BLACK_W, BLACK_H, on ? CLR_DARK_ORANGE : CLR_BROWNISH_BLACK);
        rect(bx, KEY_TOP, BLACK_W, BLACK_H, CLR_BLACK);
    }
}
