/* de:meta
{
  "slug": "supersaw",
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
  "lineage": "The showcase cart for instrument_unison (ADR-0030 / design/unison-primitive.md): the primitive's reason to exist, the detune-BLOOM gesture that moog's 3-oscillator fat can't reach. STEREO WIDTH is a cart-land RECIPE here (two panned, mutually-detuned unison stacks), NOT an engine primitive — see the design doc's 'Deferred → width' note for the promotion trigger.",
  "description": {
    "summary": "The Roland JP-8000 Super Saw: SEVEN detuned sawtooth oscillators stacked on ONE key = the fat, wide trance-lead wall. The whole instrument is one gesture — the DETUNE knob BLOOMS a single thin saw open into all seven, and the live output waveform thickens with it. WIDTH then throws the wall across the stereo field. Play a chord and it's enormous.",
    "detail": "The most-unison synth ever made, in one engine call: instrument_unison(slot, 7, detune) renders seven slightly-detuned saws inside a single slot, summed and loudness-normalized. The beating between the copies IS the sound. DETUNE rides LIVE (instrument_unison_detune) — the detune-BLOOM: drag from 0 (one thin, plain saw) up to wide (a shimmering seven-voice wall) and every held note opens at once. VOICES cycles the stack 1->3->5->7 so you hear scarcity build into the wall. CUTOFF sweeps the trance ladder-filter LIVE. WIDTH is a CART-LAND stereo recipe (not an engine feature): a second identical unison stack (slot 6) is panned right while the first pans left, and the two stacks are detuned a few cents AGAINST each other (instrument_tune +/-) so they slowly phase apart — decorrelated, they read as genuine stereo width instead of a re-centred mono sum. That's exactly how a hardware supersaw spreads. The scope up top is the ACTUAL output; the fan below splits into two clusters that pull apart as WIDTH rises. Engine-level unison width stays deliberately deferred (design/unison-primitive.md) until >=2 carts want it.",
    "controls": "Play the keys (touch / mouse / A-K QWERTY / MIDI) — chords sound huge. Drag DETUNE for the bloom (the star). Drag WIDTH to spread the wall across the stereo field (headphones!). Drag CUTOFF for the filter sweep (live — every ringing note follows). Tap VOICES to cycle 1/3/5/7 voices. Tap WAVE to cycle the oscillator (SAW/SQUARE/PULSE/TRI/SINE). Z/X shift octave."
  }
}
de:meta */
//
// supersaw — the JP-8000 Super Saw, the showcase for instrument_unison.
//
// One slot, one call: instrument_unison(LEAD, 7, detune) = seven detuned saws
// summed inside a single voice. The DETUNE knob rides the spread LIVE
// (instrument_unison_detune) — the "bloom" gesture: a thin single saw opening
// into a wall of seven. See docs/design/unison-primitive.md + ADR-0030.
//
// STEREO WIDTH is a CART-LAND RECIPE, not an engine primitive (deliberately —
// engine-level width would refactor the shared per-voice filter path for one
// instrument; deferred until >=2 carts want it). The recipe: a SECOND unison
// stack (WIDE, slot 6) panned right while LEAD pans left, with the two stacks
// TUNED A FEW CENTS APART (instrument_tune +/-). Two IDENTICAL stacks panned
// hard L/R would just re-sum to a centred mono image — it's the mutual detune
// that decorrelates them into real width (exactly how a hardware supersaw's
// stereo spread works). The keybed drives LEAD; keybed_on_note/off mirror every
// press onto WIDE.

#include "studio.h"
#include "keybed.h"
#include "ui.h"

#define LEAD 5           // left stack — the keybed's managed slot
#define WIDE 6           // right stack — the stereo-width mirror (fired via callbacks)

// widgets edit 0..1 floats; we map to musical ranges
static float k_detune = 0.30f;   // THE star — 0..1 -> spread 0 .. UNI_MAX_DETUNE semitones
static float k_cutoff = 0.78f;   // trance ladder cutoff
static float k_width  = 0.45f;   // stereo width — pan spread + inter-stack detune (the recipe)
static int   voices   = 7;       // unison stack (cycles 1/3/5/7)

#define UNI_MAX_DETUNE 0.70f     // widest per-stack spread, semitones (edge voices at +/-this)
#define WIDTH_MAX_TUNE 0.09f     // widest inter-stack detune, semitones per side (~9c each = ~18c apart)

// the WAVE cycle — the wavetable oscillators unison actually fattens (modeled engines can't
// unison, so they're deliberately NOT here). SAW = the classic supersaw; SINE = pure beating.
enum { W_SAW, W_SQUARE, W_PULSE, W_TRI, W_SINE, W_COUNT };
static const char *WAVE_NAME[W_COUNT]  = { "SAW", "SQUARE", "PULSE", "TRI", "SINE" };
static const int   WAVE_INSTR[W_COUNT] = { INSTR_SAW, INSTR_SQUARE, INSTR_SQUARE, INSTR_TRI, INSTR_SINE };
static const float WAVE_DUTY[W_COUNT]  = { 0.5f, 0.5f, 0.22f, 0.5f, 0.5f };   // PULSE = a thin square
static int wave = W_SAW;

static int   r_handle[128];      // WIDE slot's live handle per MIDI note (-1 = silent), mirror of the keybed's
static float scope[512];
static float last_detune = -1, last_cutoff = -1, last_width = -1;
static int   last_voices = -1, last_wave = -1;

static float detune_semis(void) { return k_detune * UNI_MAX_DETUNE; }
static int   cutoff_hz(void)    { return (int)(220 + k_cutoff * k_cutoff * 6800); }   // exp-ish sweep

// program one unison stack from scratch (both slots are identical apart from pan+tune, set by apply_width)
static void program_slot(int slot) {
    instrument(slot, WAVE_INSTR[wave], 4, 0, 7, 260);        // bright sustained lead: instant attack, gentle tail
    instrument_duty(slot, WAVE_DUTY[wave]);
    instrument_filter(slot, FILTER_LADDER, cutoff_hz(), 4);  // the trance 4-pole
    instrument_drive(slot, 0.12f);                           // a touch of warmth
    instrument_unison(slot, voices, detune_semis());         // THE primitive — the wall
}

// WIDTH: pan the two stacks apart AND detune them against each other so they decorrelate into
// real stereo width (identical stacks panned L/R would only re-centre). Pan is note-on-face, so
// sweep ringing notes live too; instrument_tune is read live from the bank, so it bends for free.
static void apply_width(void) {
    float sp = k_width;                      // 0..1 pan spread (0 = centred/mono, 1 = hard L/R)
    float dt = k_width * WIDTH_MAX_TUNE;      // inter-stack detune, semitones per side
    instrument_pan(LEAD, -sp);  instrument_tune(LEAD, -dt);
    instrument_pan(WIDE, +sp);  instrument_tune(WIDE, +dt);
    for (int m = 0; m < 128; m++) if (keybed_held(m)) {
        note_pan(keybed_handle(m), -sp);
        if (r_handle[m] >= 0) note_pan(r_handle[m], +sp);
    }
    last_width = k_width;
}

// mirror every keybed press/release onto the WIDE stack (keybed manages LEAD itself)
static void on_note(int midi, int vel) { r_handle[midi] = note_on(midi, WIDE, vel); }
static void on_off(int midi)           { if (r_handle[midi] >= 0) { note_off(r_handle[midi]); r_handle[midi] = -1; } }

void init(void) {
    for (int m = 0; m < 128; m++) r_handle[m] = -1;
    program_slot(LEAD);
    program_slot(WIDE);
    apply_width();                                           // pan + inter-stack detune from k_width
    keybed_config(LEAD, 4, 14);                              // slot 5, base C4, 14 white keys (2 octaves)
    keybed_velocity(3);                                      // two stacks sum -> halve so it isn't blaring
    keybed_on_note(on_note);                                 // mirror presses onto WIDE
    keybed_on_off(on_off);
    keybed_layout(0, SCREEN_H - 64, SCREEN_W, 64);
}

void update(void) {
    keybed_update();                                         // touch + mouse + QWERTY + MIDI
}

// re-apply engine params only when they CHANGE (avoid spamming the queue every frame) — to BOTH stacks
static void apply_params(void) {
    if (wave != last_wave) {
        program_slot(LEAD); program_slot(WIDE);              // instrument() resets a slot; re-apply the rest
        last_wave = wave;
        last_detune = last_cutoff = -1; last_voices = -1; last_width = -1;   // force filter/unison/width re-apply
    }
    float d = detune_semis();
    if (d != last_detune) {                                  // LIVE bloom (reads the bank per-sample)
        instrument_unison_detune(LEAD, d); instrument_unison_detune(WIDE, d); last_detune = d;
    }
    if (voices != last_voices) {
        instrument_unison(LEAD, voices, d); instrument_unison(WIDE, voices, d); last_voices = voices;
    }
    int c = cutoff_hz();
    if (c != (int)last_cutoff) {
        instrument_filter(LEAD, FILTER_LADDER, c, 4);        // new notes start here
        instrument_filter(WIDE, FILTER_LADDER, c, 4);
        for (int m = 0; m < 128; m++) if (keybed_held(m)) {  // AND sweep every ringing note LIVE (both stacks)
            note_cutoff(keybed_handle(m), c);
            if (r_handle[m] >= 0) note_cutoff(r_handle[m], c);
        }
        last_cutoff = c;
    }
    if (k_width != last_width) apply_width();
}

// the bloom scope: the REAL output waveform, and a fan of the voices spreading into two stereo stacks
static void draw_bloom(int x, int y, int w, int h) {
    rect(x, y, w, h, CLR_DARKER_GREY);
    int cy = y + h / 2;
    line(x + 1, cy, x + w - 2, cy, CLR_DARK_GREY);

    // 1) the actual post-FX output — clean saw when tight, a fuzzy thick band when bloomed
    scope_read(scope, 512);
    int start = 0;                                            // zero-cross trigger so the trace holds still
    for (int i = 1; i < 256; i++) if (scope[i - 1] < 0 && scope[i] >= 0) { start = i; break; }
    int span = w - 4;
    int prevx = x + 2, prevy = cy - (int)(scope[start] * (h * 0.42f));
    for (int i = 1; i < span; i++) {
        int si = start + (i * 220) / span; if (si >= 512) si = 511;
        int px = x + 2 + i;
        int py = cy - (int)(scope[si] * (h * 0.42f));
        line(prevx, prevy, px, py, CLR_LIME_GREEN);
        prevx = px; prevy = py;
    }

    // 2) the fan: `voices` ticks per stack, spread by DETUNE — and the two stacks pull APART with WIDTH
    int fx = x + w / 2, fy = y + h - 6;
    float spread = k_detune;                                  // 0..1 visual detune width
    int   woff   = (int)(k_width * (w * 0.20f));              // L/R stack separation grows with WIDTH
    for (int side = 0; side < 2; side++) {                    // 0 = left stack (blue), 1 = right stack (orange)
        int cx = fx + (side ? woff : -woff);
        int base = side ? CLR_ORANGE : CLR_BLUE;
        for (int u = 0; u < voices; u++) {
            float pos = (voices == 1) ? 0.0f : (-1.0f + 2.0f * (float)u / (float)(voices - 1));
            int tx = cx + (int)(pos * spread * (w * 0.28f));
            line(tx, fy, tx, fy - 5, (u == voices / 2) ? CLR_WHITE : base);
        }
    }
}

void draw(void) {
    cls(CLR_BLACK);

    print("SUPER SAW", 6, 5, CLR_YELLOW);
    print("JP-8000", SCREEN_W - 60, 5, CLR_DARK_GREY);

    draw_bloom(6, 16, SCREEN_W - 12, 52);

    // a one-line soul caption
    print_centered("DETUNE blooms - WIDTH spreads it stereo", SCREEN_W / 2, 71, CLR_MEDIUM_GREY);

    ui_begin();
    // two cycle buttons, side by side
    if (ui_button(SCREEN_W / 2 - 96, 80, 92, 12, str("VOICES %d", voices))) {   // 1/3/5/7 — scarcity building into the wall
        voices += 2; if (voices > 7) voices = 1;
    }
    if (ui_button(SCREEN_W / 2 + 4, 80, 92, 12, str("WAVE %s", WAVE_NAME[wave]))) {   // the waves unison fattens
        wave = (wave + 1) % W_COUNT;
    }

    // three knobs across — value folded into each label (no room for separate readouts)
    int ky = 108;
    ui_knob(&k_detune, 46, ky, str("DET %dc", (int)(detune_semis() * 100)));
    ui_knob(&k_width,  SCREEN_W / 2, ky, str("WIDTH %d", (int)(k_width * 100)));
    ui_knob(&k_cutoff, SCREEN_W - 46, ky, str("CUT %d", cutoff_hz()));
    ui_end();

    apply_params();

    keybed_draw();
}
