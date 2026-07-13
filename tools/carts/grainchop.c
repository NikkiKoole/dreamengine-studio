/* de:meta
{
  "slug": "grainchop",
  "title": "grainchop",
  "status": "active",
  "created": "2026-07-13",
  "kind": [
    "instrument",
    "probe"
  ],
  "genre": null,
  "teaches": [
    "granular-synth",
    "gesture-loop"
  ],
  "lineage": "A zero-engine-work SPIKE toward the PCM-sampler question (docs/design/mic-and-sampling.md): does 'chop what I just played' feel good? Built on grains()/grains_freeze()/grains_pitch() (grains.c is the showcase) + keybed.h (the shared polyphonic keybed epiano/moog/touchpiano carry) + an engine-select bank borrowed from motionbox's OSC page. Proves the FEEL of freeze-and-chop before committing to a real INSTR_SAMPLE voice + a mix-capture path.",
  "description": {
    "summary": "Play a synth on the keybed, FREEZE the last few seconds of what you played into the granular buffer, then CHOP it: 8 slice pads jump to points in your own frozen sound and the keys re-pitch it. A fantasy-console stand-in for a sampler — no PCM, no mic; it grabs the console's OWN synthesis and lets you play with the recording.",
    "detail": "The sampler workflow, faked with a granular cloud so it needs ZERO new engine work. LIVE: the keybed feeds grains() the whole time (dry synth + a trailing cloud). Hit FREEZE and the buffer stops capturing and loops forever — now you're playing with the recording. The 8 SLICE pads scrub the read POSITION to eight points across the frozen buffer (tap = jump to that slice); , and . transpose the cloud a semitone; REV plays the grains backwards. SIZE / DENS / FB shape the cloud (grain length, density, self-feedback). ENGINE cycles the voice under the keybed (SAW / FM / PLUCK / EPIANO / MALLET / PIANO / ORGAN / BRASS) so you can chop many different sounds. It is granular, not clean slices — that difference is exactly the question the spike answers.",
    "controls": "Keybed: A-K white keys, W-E-T-Y-U-O-P black, Z/X octave down/up (+ mouse/touch/MIDI). SPACE or F = FREEZE toggle. 1-8 or the pads = jump to a slice. , . = pitch down/up. R or the REV button = reverse. [ ] or the ENGINE button = cycle the voice. SIZE/DENS/FB knobs shape the cloud."
  }
}
de:meta */
#include "studio.h"
#include "keybed.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>

// ── GRAINCHOP ────────────────────────────────────────────────────────────────
// A spike toward the PCM sampler (docs/design/mic-and-sampling.md): does chopping
// what you just played actually feel good? Faked with a granular cloud so it costs
// no engine work. Play the keybed → it feeds grains() → FREEZE grabs the last few
// seconds and loops it → the 8 slice pads scrub the read position through your own
// frozen sound while the keys re-pitch it. The source is the console's OWN synth,
// never a mic — the honest-loop angle mic-and-sampling.md didn't fully walk.
//
//   SPACE/F  freeze the buffer (stop capturing, loop what's there)
//   1-8      jump the read position to slice i (the "chop")
//   , .      transpose the cloud down/up a semitone
//   R        reverse the grains
//   [ ]      cycle the engine under the keybed
//   SIZE/DENS/FB  shape the cloud

#define SLOT     5
#define NSLICE   8

// ── the voice bank under the keybed (borrowed from motionbox's OSC MODEL list) ──
typedef struct { int instr; const char *name; int a, d, s, r; } Voice;   // env: attack/decay ms, sustain 0-7, release ms
static const Voice VOICES[] = {
    { INSTR_SAW,    "SAW",   8, 140, 6, 220 },
    { INSTR_FM,     "FM",    4, 200, 5, 300 },
    { INSTR_PLUCK,  "PLUCK", 2,   0, 0, 600 },
    { INSTR_EPIANO, "EPIANO",2,   0, 0, 500 },
    { INSTR_MALLET, "MALLET",1,   0, 0, 400 },
    { INSTR_PIANO,  "PIANO", 2,   0, 0, 650 },
    { INSTR_ORGAN,  "ORGAN", 6,  40, 7, 120 },
    { INSTR_BRASS,  "BRASS",30, 100, 6, 220 },
};
#define NVOICE ((int)(sizeof VOICES / sizeof *VOICES))

// ── cloud state (knobs 0..1; set-and-hold below) ──
static float k_size = 0.35f;   // → grain 40..400ms
static float k_dens = 0.60f;   // → 5..60 grains/sec
static float k_fb   = 0.25f;   // → 0..0.85 feedback
static float pos    = 0.85f;   // read position 0..1 (0 = deep past, 1 = live edge)
static int   slice  = NSLICE - 1;   // active slice (highlight)
static int   pitch  = 0;       // grain transpose, semitones
static bool  reverse = false;
static bool  frozen  = false;
static int   eng     = 0;      // index into VOICES

// last-applied (only re-push grains when a value moves — set-and-hold)
static float a_gm = -1, a_de = -1, a_po = -1, a_fb = -1;
static int   a_pitch = -999; static bool a_rev = false; static int a_frozen = -1;

static void apply_voice(void) {
    const Voice *v = &VOICES[eng];
    instrument(SLOT, v->instr, v->a, v->d, v->s, v->r);
}

void init(void) {
    apply_voice();
    keybed_config(SLOT, 4, 14);                 // slot 5, base C4, 14 white keys (2 octaves)
    keybed_layout(0, 96, SCREEN_W, SCREEN_H - 96);
    grains(40 + k_size * 360, 5 + k_dens * 55, pos, 0.12f, k_fb * 0.85f, 0.6f);
    grains_pitch(0, 0.15f, 0);
}

static void set_slice(int i) {
    if (i < 0) i = 0; if (i > NSLICE - 1) i = NSLICE - 1;
    slice = i;
    pos   = (float)i / (float)(NSLICE - 1);     // 8 evenly-spaced read points
}

void update(void) {
    keybed_update();                            // touch + mouse + QWERTY + MIDI → the synth

    if (keyp(' ') || keyp('F')) frozen = !frozen;
    if (keyp('R')) reverse = !reverse;
    if (keyp('.') && pitch <  24) pitch++;
    if (keyp(',') && pitch > -24) pitch--;
    if (keyp(']')) eng = (eng + 1) % NVOICE, apply_voice();
    if (keyp('[')) eng = (eng + NVOICE - 1) % NVOICE, apply_voice();
    for (int i = 0; i < NSLICE; i++)            // number keys 1..8 = slice pads
        if (keyp('1' + i)) set_slice(i);

    // ── SET-AND-HOLD: reconfigure the cloud only when something moved ──
    float gm = 40 + k_size * 360, de = 5 + k_dens * 55, fb = k_fb * 0.85f;
    if (gm != a_gm || de != a_de || pos != a_po || fb != a_fb) {
        grains(gm, de, pos, 0.12f, fb, 0.6f);
        a_gm = gm; a_de = de; a_po = pos; a_fb = fb;
    }
    if (pitch != a_pitch || reverse != a_rev) {
        grains_pitch((float)pitch, 0.15f, reverse ? 1 : 0);
        a_pitch = pitch; a_rev = reverse;
    }
    if ((frozen ? 1 : 0) != a_frozen) {
        grains_freeze(frozen ? 1 : 0);
        a_frozen = frozen ? 1 : 0;
    }
}

void draw(void) {
    cls(CLR_BLACK);
    char buf[96];

    // ── header ──
    snprintf(buf, sizeof buf, "GRAINCHOP  %s", VOICES[eng].name);
    print(buf, 4, 2, CLR_WHITE);
    snprintf(buf, sizeof buf, "%s  pit%+d%s", frozen ? "FROZEN" : "LIVE", pitch, reverse ? " REV" : "");
    print(buf, SCREEN_W - text_width(buf) - 4, 2, frozen ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);

    ui_begin();

    // ── slice strip = the 8 chop pads (buffer viz + pads in one row) ──
    int sx = 4, sy = 16, sw = (SCREEN_W - 8) / NSLICE, sh = 26;
    for (int i = 0; i < NSLICE; i++) {
        char lab[4]; snprintf(lab, sizeof lab, "%d", i + 1);
        if (ui_button(sx + i * sw, sy, sw - 2, sh, lab)) set_slice(i);
    }

    // ── controls: FREEZE hero + ENGINE/REV + cloud knobs ──
    if (ui_button(4, 50, 80, 34, frozen ? "FROZEN" : "FREEZE")) frozen = !frozen;
    if (ui_button(90, 50, 70, 16, VOICES[eng].name)) eng = (eng + 1) % NVOICE, apply_voice();
    if (ui_button(90, 68, 70, 16, reverse ? "REV ON" : "REV"))  reverse = !reverse;
    ui_knob(&k_size, 192, 66, "SIZE");
    ui_knob(&k_dens, 228, 66, "DENS");
    ui_knob(&k_fb,   264, 66, "FB");

    ui_end();

    // ── overlays (after ui_end so state is resolved) ──
    // active-slice highlight on the strip
    int hx = sx + slice * sw;
    rect(hx, sy, sw - 2, sh, frozen ? CLR_LIME_GREEN : CLR_YELLOW);
    // read-position playhead across the strip
    int px = sx + (int)(pos * (NSLICE * sw - 2));
    line(px, sy - 2, px, sy + sh + 1, CLR_ORANGE);
    // LIVE = the buffer is being overwritten (a sweeping capture bar); FROZEN = held
    if (!frozen) {
        int cx = sx + (int)(fabsf(sinf(now() * 1.4f)) * (NSLICE * sw - 2));
        line(cx, sy - 2, cx, sy + sh + 1, CLR_RED);
        print("REC", sx + 2, sy + sh - 8, CLR_RED);
    } else {
        print("CHOP: tap a slice", sx + 2, sy + sh - 8, CLR_LIME_GREEN);
    }

    // ── the keybed (owns the bottom, custom-drawn manual) ──
    keybed_draw();
}
