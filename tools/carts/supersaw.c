/* de:meta
{
  "title": "supersaw",
  "status": "active",
  "created": "2026-07-09",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "subtractive-synth"
  ],
  "homage": "Roland JP-8000 Super Saw",
  "lineage": "The showcase cart for instrument_unison (ADR-0030 / design/unison-primitive.md): the primitive's reason to exist, the detune-BLOOM gesture that moog's 3-oscillator fat can't reach.",
  "description": {
    "summary": "The Roland JP-8000 Super Saw: SEVEN detuned sawtooth oscillators stacked on ONE key = the fat, wide trance-lead wall. The whole instrument is one gesture — the DETUNE knob BLOOMS a single thin saw open into all seven, and the live output waveform thickens with it. Play a chord and it's enormous.",
    "detail": "The most-unison synth ever made, in one engine call: instrument_unison(slot, 7, detune) renders seven slightly-detuned saws inside a single slot, summed and loudness-normalized. The beating between the copies IS the sound. DETUNE rides LIVE (instrument_unison_detune) — the detune-BLOOM: drag from 0 (one thin, plain saw) up to wide (a shimmering seven-voice wall) and every held note opens at once. VOICES cycles the stack 1→3→5→7 so you hear scarcity build into the wall. CUTOFF sweeps the trance ladder-filter LIVE — every ringing note follows (note_cutoff on the keybed's held handles). The scope up top is the ACTUAL output (scope_read) — watch the clean saw fuzz out as the wall blooms; the fan of ticks below it is the seven voices spreading in pitch. This is the EXTREME end of unison that moog's 3-osc detune only hints at.",
    "controls": "Play the keys (touch / mouse / A-K QWERTY / MIDI) — chords sound huge. Drag DETUNE for the bloom (the star). Drag CUTOFF for the filter sweep (live — every ringing note follows). Tap VOICES to cycle 1/3/5/7 voices. Tap WAVE to cycle the oscillator (SAW/SQUARE/PULSE/TRI/SINE — the waves unison fattens). Z/X shift octave."
  }
}
de:meta */
//
// supersaw — the JP-8000 Super Saw, the showcase for instrument_unison.
//
// One slot, one call: instrument_unison(LEAD, 7, detune) = seven detuned saws
// summed inside a single voice. The DETUNE knob rides the spread LIVE
// (instrument_unison_detune) — the "bloom" gesture: a thin single saw opening
// into a wall of seven. Before the primitive this needed a hand-managed pool of
// seven slots fired per note (the mt70 multi-slot trick); now it's inert-when-off
// and one line when on. See docs/design/unison-primitive.md + ADR-0030.

#include "studio.h"
#include "keybed.h"
#include "ui.h"

#define LEAD 5

// widgets edit 0..1 floats; we map to musical ranges
static float k_detune = 0.30f;   // THE star — 0..1 → spread 0 .. UNI_MAX_DETUNE semitones
static float k_cutoff = 0.78f;   // trance ladder cutoff
static int   voices   = 7;       // unison stack (cycles 1/3/5/7)

#define UNI_MAX_DETUNE 0.70f     // widest spread, semitones (edge voices at ±this)

// the WAVE cycle — the wavetable oscillators unison actually fattens (modeled engines can't
// unison, so they're deliberately NOT here). SAW = the classic supersaw; SINE = pure beating.
enum { W_SAW, W_SQUARE, W_PULSE, W_TRI, W_SINE, W_COUNT };
static const char *WAVE_NAME[W_COUNT]  = { "SAW", "SQUARE", "PULSE", "TRI", "SINE" };
static const int   WAVE_INSTR[W_COUNT] = { INSTR_SAW, INSTR_SQUARE, INSTR_SQUARE, INSTR_TRI, INSTR_SINE };
static const float WAVE_DUTY[W_COUNT]  = { 0.5f, 0.5f, 0.22f, 0.5f, 0.5f };   // PULSE = a thin square
static int wave = W_SAW;

static float scope[512];
static float last_detune = -1, last_cutoff = -1;
static int   last_voices = -1, last_wave = -1;

static float detune_semis(void) { return k_detune * UNI_MAX_DETUNE; }
static int   cutoff_hz(void)    { return (int)(220 + k_cutoff * k_cutoff * 6800); }   // exp-ish sweep

void init(void) {
    // a bright, sustained lead: instant attack, full sustain, a gentle release tail
    instrument(LEAD, WAVE_INSTR[wave], 4, 0, 7, 260);
    instrument_duty(LEAD, WAVE_DUTY[wave]);
    instrument_filter(LEAD, FILTER_LADDER, cutoff_hz(), 4);   // the trance 4-pole
    instrument_drive(LEAD, 0.12f);                            // a touch of warmth
    instrument_unison(LEAD, voices, detune_semis());          // THE primitive — the whole cart
    keybed_config(LEAD, 4, 14);                               // slot 5, base C4, 14 white keys (2 octaves)
    keybed_layout(0, SCREEN_H - 74, SCREEN_W, 74);
}

void update(void) {
    keybed_update();                                          // touch + mouse + QWERTY + MIDI
}

// re-apply engine params only when they CHANGE (avoid spamming the queue every frame)
static void apply_params(void) {
    // WAVE change re-programs the slot (note-on-face); instrument() resets the slot, so re-apply
    // the rest by forcing the change-checks below to re-fire.
    if (wave != last_wave) {
        instrument(LEAD, WAVE_INSTR[wave], 4, 0, 7, 260);
        instrument_duty(LEAD, WAVE_DUTY[wave]);
        instrument_drive(LEAD, 0.12f);
        last_wave = wave;
        last_detune = last_cutoff = -1; last_voices = -1;      // force filter/unison re-apply
    }
    float d = detune_semis();
    if (d != last_detune)   { instrument_unison_detune(LEAD, d);              last_detune = d; }  // LIVE bloom (reads the bank per-sample)
    if (voices != last_voices) { instrument_unison(LEAD, voices, d);          last_voices = voices; }
    int c = cutoff_hz();
    if (c != (int)last_cutoff) {
        instrument_filter(LEAD, FILTER_LADDER, c, 4);          // new notes start here
        for (int m = 0; m < 128; m++)                          // AND sweep every ringing note LIVE (note_cutoff is a live-ride)
            if (keybed_held(m)) note_cutoff(keybed_handle(m), c);
        last_cutoff = c;
    }
}

// the bloom scope: the REAL output waveform, and a fan of the seven voices' pitches
static void draw_bloom(int x, int y, int w, int h) {
    // frame
    rect(x, y, w, h, CLR_DARKER_GREY);
    int cy = y + h / 2;
    line(x + 1, cy, x + w - 2, cy, CLR_DARK_GREY);

    // 1) the actual post-FX output — clean saw when tight, a fuzzy thick band when bloomed
    scope_read(scope, 512);
    // zero-cross trigger so the trace holds still
    int start = 0;
    for (int i = 1; i < 256; i++) if (scope[i - 1] < 0 && scope[i] >= 0) { start = i; break; }
    int span = w - 4;
    int prevx = x + 2, prevy = cy - (int)(scope[start] * (h * 0.42f));
    for (int i = 1; i < span; i++) {
        int si = start + (i * 220) / span;
        if (si >= 512) si = 511;
        int px = x + 2 + i;
        int py = cy - (int)(scope[si] * (h * 0.42f));
        line(prevx, prevy, px, py, CLR_LIME_GREEN);
        prevx = px; prevy = py;
    }

    // 2) the fan: `voices` ticks spreading apart in pitch as detune widens — the wall, pictured
    int fx = x + w / 2;
    int fy = y + h - 6;
    float spread = k_detune;                                  // 0..1 visual width
    for (int u = 0; u < voices; u++) {
        float pos = (voices == 1) ? 0.0f : (-1.0f + 2.0f * (float)u / (float)(voices - 1));
        int tx = fx + (int)(pos * spread * (w * 0.42f));
        int col = (u == voices / 2) ? CLR_WHITE : CLR_BLUE;
        line(tx, fy, tx, fy - 5, col);
    }
}

void draw(void) {
    cls(CLR_BLACK);

    // title
    print("SUPER SAW", 6, 5, CLR_YELLOW);
    print("JP-8000", SCREEN_W - 60, 5, CLR_DARK_GREY);

    // the bloom scope + voice fan
    draw_bloom(6, 16, SCREEN_W - 12, 52);

    // knobs
    ui_begin();
    int ky = 96;
    ui_knob(&k_detune, 46, ky, "DETUNE");
    ui_knob(&k_cutoff, SCREEN_W - 46, ky, "CUTOFF");

    // two stacked cycle buttons in the center column
    if (ui_button(SCREEN_W / 2 - 48, 82, 96, 15, str("VOICES %d", voices))) {   // 1/3/5/7 — scarcity building into the wall
        voices += 2; if (voices > 7) voices = 1;
    }
    if (ui_button(SCREEN_W / 2 - 48, 100, 96, 15, str("WAVE %s", WAVE_NAME[wave]))) {   // the waves unison fattens
        wave = (wave + 1) % W_COUNT;
    }
    ui_end();

    apply_params();

    // readouts: DETUNE in cents (the number that matters), CUTOFF in Hz
    print_centered(str("%d c", (int)(detune_semis() * 100)), 46, ky + 22, k_detune > 0.02f ? CLR_BLUE : CLR_DARK_GREY);
    print_centered(str("%d Hz", cutoff_hz()), SCREEN_W - 46, ky + 22, CLR_ORANGE);

    // a one-line soul caption (in the gap between the scope and the knobs)
    print_centered("drag DETUNE - the wall blooms", SCREEN_W / 2, 74, CLR_MEDIUM_GREY);

    keybed_draw();
}
