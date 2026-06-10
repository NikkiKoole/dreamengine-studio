#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── TAPELOOP ───────────────────────────────────────────────────────────────
// Frippertronics (Robert Fripp + Brian Eno, mid-'70s): two reel-to-reel decks
// strung into one long tape loop — play a note, it circulates and decays, you
// play over the fading layers, and the music slowly evolves out of its own
// echoes. The showcase for `tape()` — and a nod to the MELLOTRON: the source is a
// faked "recording" of a real instrument (STRINGS / CHOIR / FLUTE), each a drawn
// single-cycle wave (a stack of a few harmonics via wave_set → INSTR_USER0). It
// feeds a LONG echo loop, and the whole circulating wash runs through TAPE — so
// every pass comes back warmer + more warped (wow/flutter pitch-drift + saturation).
// The tape degradation IS the instrument; it turns a clean synth into an aging
// recording. Fifth box in the effects family — spacecho (echo) · cathedral (reverb) ·
// juno (chorus) · mistress (flanger) · tapeloop (tape).
//
//   TIMBRE    the loaded "tape recording" — strings / choir / flute  [T]
//   WOW       slow pitch drift (the seasick wobble)
//   FLUTTER   fast warble
//   SAT       tape saturation/warmth (each loop pass compresses + darkens)
//   FEEDBACK  how long the loop survives (regeneration)
//   SPACE     AUTO — a sparse phrase plays itself into the loop
//   1..6      drop a note in by hand

#define PAD 8

static float k_wow  = 0.45f;
static float k_flut = 0.30f;
static float k_sat  = 0.50f;
static float k_feed = 0.78f;   // → echo feedback 0..~0.92
static bool  autop  = true;
static bool  show_help;
static int   last_step = -1, auto_i = 0;
static float reel = 0.0f;      // reel-rotation phase
static float wob  = 0.0f;      // flutter wobble phase
static float vu   = 0.0f;      // loop level (cosmetic), decays at the feedback rate

// the SOURCE is a faked "recording": three Mellotron-style tape timbres, each a drawn single-cycle
// wave (a stack of a few harmonics) → INSTR_USER0 via wave_set. A real Mellotron plays tape
// recordings of these instruments; we draw the spectrum and let tape() supply the worn-tape wobble.
static int   timbre = 0;       // 0 strings · 1 choir · 2 flute
static const char *TNAME[3] = { "STRINGS", "CHOIR", "FLUTE" };
static float TBL[3][64];

static void build_waves(void) {
    for (int i = 0; i < 64; i++) {
        float x = i / 64.0f * 6.2831853f;
        // STRINGS — dense bowed-ensemble harmonics, rolled-off top
        TBL[0][i] = 0.60f*sinf(x) + 0.42f*sinf(2*x) + 0.30f*sinf(3*x) + 0.20f*sinf(4*x)
                  + 0.14f*sinf(5*x) + 0.10f*sinf(6*x) + 0.06f*sinf(7*x);
        // CHOIR — vocal "aah": soft fundamental + a formant bump around the 3rd–4th harmonic
        TBL[1][i] = 0.45f*sinf(x) + 0.28f*sinf(2*x) + 0.40f*sinf(3*x) + 0.42f*sinf(4*x)
                  + 0.22f*sinf(5*x) + 0.10f*sinf(6*x);
        // FLUTE — nearly pure, a touch of 2nd/3rd (the breath comes from tape + the lowpass)
        TBL[2][i] = 0.88f*sinf(x) + 0.16f*sinf(2*x) + 0.07f*sinf(3*x);
    }
}

// a calm pentatonic pool (C lydian-ish), spread across two octaves
static const int POOL[6] = { 60, 64, 67, 72, 76, 79 };
// a sparse 16-step phrase ('.' = rest, digit = POOL index) — Frippertronics is mostly space
static const char PHRASE[17] = "0...3...1...4...";

static float feedback_v(void) { return k_feed * 0.92f; }

static void apply_tape(void) { tape(k_wow, k_flut, k_sat); }
static void apply_loop(void) { echo(1800, feedback_v(), 0.35f); }   // long, dark Frippertronics loop

static void drop_note(int i) {
    hit(POOL[i], PAD, 5, 900);   // a long soft pad note INTO the loop
    vu += 0.4f; if (vu > 1.0f) vu = 1.0f;
}

void init(void) {
    bpm(40);                                       // slow, drifting
    build_waves();
    wave_set(0, TBL[timbre], 64);                  // the "tape recording" → INSTR_USER0
    instrument(PAD, INSTR_USER0, 220, 0, 5, 1500); // soft swelling pad (the drawn Mellotron timbre)
    instrument_filter(PAD, FILTER_LOW, 1300, 2);   // warm
    instrument_echo(PAD, 0.7f);                    // feed the pad hard into the loop
    apply_loop();
    apply_tape();
}

void update(void) {
    if (keyp(KEY_SPACE)) { autop = !autop; last_step = -1; }
    if (keyp('H')) show_help = !show_help;
    if (keyp('T')) { timbre = (timbre + 1) % 3; wave_set(0, TBL[timbre], 64); }  // cycle tape recording
    for (int i = 0; i < 6; i++) if (keyp('1' + i)) drop_note(i);

    if (autop) {
        int step = beat();
        if (step != last_step) {
            last_step = step;
            char c = PHRASE[(step) & 15];
            if (c != '.') drop_note(c - '0');
        }
    }

    reel += 0.06f; if (reel >= 1.0f) reel -= 1.0f;
    wob  += (0.05f + k_flut * 0.25f);  if (wob >= 6.2831853f) wob -= 6.2831853f;
    // loop level decays at roughly the feedback rate (cosmetic — the wash hanging on)
    vu *= 0.96f + feedback_v() * 0.035f;
    if (vu < 0.0f) vu = 0.0f;
}

// a spinning reel at (cx,cy): a hub + a few spokes turning at the loop speed, wobbling with flutter
static void draw_reel(int cx, int cy, int r, float ph, float wobamt) {
    circ(cx, cy, r, CLR_DARK_GREY);
    circfill(cx, cy, 3, CLR_LIGHT_GREY);
    for (int s = 0; s < 4; s++) {
        float a = (ph + s / 4.0f) * 6.2831853f + wobamt;
        line(cx, cy, cx + (int)(cosf(a) * (r - 2)), cy + (int)(sinf(a) * (r - 2)), CLR_MEDIUM_GREY);
    }
}

void draw(void) {
    char buf[40];
    cls(CLR_BROWNISH_BLACK);

    print("TAPELOOP", 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("frippertronics  -  tape() showcase", 8, 18, CLR_DARK_BROWN);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(24, 26, 272, 150, CLR_BLACK);
        rect(24, 26, 272, 150, CLR_DARK_BROWN);
        print("TAPELOOP", 120, 34, CLR_LIGHT_YELLOW);
        static const char *HL[] = {
            "A pad feeds a LONG echo loop, and the",
            "whole circulating wash runs through TAPE",
            "- so each pass returns warmer + more",
            "warped. The decay IS the music.",
            "",
            "WOW      slow pitch drift",
            "FLUTTER  fast warble",
            "SAT      tape warmth (per loop pass)",
            "FEEDBACK how long the loop survives",
            "TIMBRE   the tape recording [T]:",
            "         strings / choir / flute",
            "SPACE AUTO   1..6 drop a note   H help",
        };
        for (int i = 0; i < 12; i++)
            print(HL[i], 36, 44 + i * 11, i < 4 ? CLR_LIGHT_PEACH : CLR_WHITE);
        ui_begin();
        if (ui_button(130, 160, 60, 14, "CLOSE")) show_help = false;
        ui_end();
        return;
    }

    // ── the tape transport: two reels + the loop of tape sagging between them ──
    int ty = 70, lx = 60, rx = 260;
    float wobamt = sinf(wob) * (0.05f + k_flut * 0.4f);    // flutter jitters the reels
    draw_reel(lx, ty, 26, reel, wobamt);
    draw_reel(rx, ty, 26, reel, -wobamt);
    // the tape spans the reels with a sag that wobbles with wow (slow) + flutter (fast)
    int prev_x = lx + 24, prev_y = ty;
    for (int i = 1; i <= 20; i++) {
        float t = i / 20.0f;
        int x = lx + 24 + (int)((rx - 24 - (lx + 24)) * t);
        float sag = sinf(t * 3.14159f) * (18.0f + sinf(wob * 0.3f) * k_wow * 16.0f + sinf(wob) * k_flut * 6.0f);
        int y = ty + (int)sag + 26;
        line(prev_x, prev_y, x, y, CLR_DARK_BROWN);
        prev_x = x; prev_y = y;
    }

    // loop-level meter (the wash hanging on)
    rectfill(60, 120, 200, 8, CLR_BLACK);
    int w = (int)(vu * 200.0f); if (w > 200) w = 200;
    rectfill(60, 120, w, 8, CLR_DARK_PEACH);
    print("LOOP", 60, 110, CLR_DARK_BROWN);

    // ── knobs ────────────────────────────────────────────────────────────
    ui_begin();
    bool t = false, e = false;
    if (ui_knob(&k_wow,  44,  150, "WOW"))     t = true;
    if (ui_knob(&k_flut, 104, 150, "FLUTTER")) t = true;
    if (ui_knob(&k_sat,  164, 150, "SAT"))     t = true;
    if (ui_knob(&k_feed, 224, 150, "FEEDBACK")) e = true;
    if (t) apply_tape();
    if (e) apply_loop();
    if (ui_button(8, 30, 64, 14, TNAME[timbre])) { timbre = (timbre + 1) % 3; wave_set(0, TBL[timbre], 64); }
    if (ui_button(270, 30, 42, 14, autop ? "AUTO*" : "AUTO")) { autop = !autop; last_step = -1; }
    if (ui_button(270, 48, 42, 14, "HELP")) show_help = true;
    ui_end();
    print("TAPE:", 8, 46, CLR_DARK_BROWN);          // the loaded "recording"

    sprintf(buf, "WOW %d  FLUT %d  SAT %d  FB %d",
            (int)(k_wow * 100), (int)(k_flut * 100), (int)(k_sat * 100), (int)(feedback_v() * 100));
    print(buf, 44, 182, CLR_DARK_BROWN);
}
