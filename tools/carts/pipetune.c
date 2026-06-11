#include "studio.h"
#include <stdio.h>
#include <math.h>

// PIPE TUNER — an EAR test for the modeled flute (and friends). Sweeps a chromatic
// scale very low → very high and sounds the modeled voice TOGETHER with a pure SINE
// at the same pitch. If the voice is in tune the two lock and sit still; if it drifts
// you hear BEATING / a wobble that speeds up the further out it goes. The classic
// tune-against-a-reference trick — your ears are the meter here (tune-check.js is the
// headless number version).
//
//   PIPE is the point. Its pitch EMERGES from a feedback loop, so the MACROS retune it:
//   the morph (embouchure) macro sets the air-jet length, harmonics (overblow) pushes the
//   octave mode. So each named flute PRESET has its own tuning — flip 1..5 and hear where
//   it goes wrong (recorder/breathy go flat up high; piccolo overblows). docs §18.
//
// CONTROLS
//   1..5         jump to a PIPE preset (flute · pan-pipe · recorder · breathy · piccolo)
//   TAB / E      cycle every voice (the 5 pipes, then REED · BRASS · BOWED · PLUCK)
//   UP / DOWN    nudge embouchure (morph) live off the preset — ±0.05
//   LEFT / RIGHT  play: sweep speed   ·   paused: step ±1 semitone
//   SPACE        play / pause (pause HOLDS the current note so you can judge the beat)
//   R            reference sine on / off (off = hear the raw voice alone)

#define SLOT_TEST 5
#define SLOT_REF  6
#define LO 36          // C2
#define HI 96          // C7

static const char *NOTE_NAMES[12] =
    { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

typedef struct { int id; const char *name; float h, t, m; const char *note; int bad; } Voice;
static const Voice VOICES[] = {
    // the five PIPE presets — the point of the cart. `note` = measured tuning (tune-check.js),
    // `bad` = out of tune (drawn orange). The hollow-embouchure ones (morph <= 0.5) go flat.
    { INSTR_PIPE,  "PIPE flute",    0.00f, 0.38f, 0.70f, "in tune to ~C6",       0 },
    { INSTR_PIPE,  "PIPE pan-pipe", 0.08f, 0.78f, 0.50f, "flat low, 8ve up high", 1 },
    { INSTR_PIPE,  "PIPE recorder", 0.00f, 0.55f, 0.30f, "FLAT ~-20c (m0.30)",   1 },
    { INSTR_PIPE,  "PIPE breathy",  0.00f, 0.90f, 0.42f, "FLAT ~-20c (m0.42)",   1 },
    { INSTR_PIPE,  "PIPE piccolo",  0.55f, 0.28f, 0.82f, "overblows +8ve, in tune", 0 },
    // other engines, vanilla voices — for comparison
    { INSTR_REED,  "REED clarinet", 0.00f, 0.30f, 0.40f, "", 0 },
    { INSTR_BRASS, "BRASS trumpet", 0.15f, 0.60f, 0.42f, "", 0 },
    { INSTR_BOWED, "BOWED violin",  0.45f, 0.30f, 0.70f, "", 0 },
    { INSTR_PLUCK, "PLUCK string",  0.50f, 0.55f, 0.35f, "", 0 },
};
#define NVOX ((int)(sizeof(VOICES) / sizeof(VOICES[0])))

static int   vIdx       = 0;
static float morph      = 0.70f;   // live embouchure; reset from the voice on switch
static bool  refOn      = true;
static bool  frozen     = false;
static int   stepFrames = 30;      // ~0.5s sounding @ 60fps
static int   gapFrames  = 8;

static int  fnum  = -1;
static int  midi  = LO;
static int  hTest = -1, hRef = -1;
static bool sounding = false;

static void define_test(void) {
    const Voice *v = &VOICES[vIdx];
    instrument(SLOT_TEST, v->id, 30, 0, 6, 300);  // held-friendly; PLUCK just decays in the gate
    instrument_harmonics(SLOT_TEST, v->h);
    instrument_timbre(SLOT_TEST, v->t);
    instrument_morph(SLOT_TEST, morph);
}

static void silence(void) {
    if (hTest >= 0) { note_off(hTest); hTest = -1; }
    if (hRef  >= 0) { note_off(hRef);  hRef  = -1; }
    sounding = false;
}

static void sound_current(void) {
    silence();
    instrument_morph(SLOT_TEST, morph);          // pick up live embouchure changes
    hTest = note_on(midi, SLOT_TEST, 5);
    if (refOn) hRef = note_on(midi, SLOT_REF, 4); // a touch quieter — the reference
    sounding = true;
}

static void switch_voice(int i) {
    vIdx  = (i + NVOX) % NVOX;
    morph = VOICES[vIdx].m;        // start from the preset's embouchure
    define_test();
    if (frozen) sound_current();
}

void init(void) {
    define_test();
    instrument(SLOT_REF, INSTR_SINE, 8, 0, 7, 120);
}

void update(void) {
    // ── voice selection ──
    for (int k = 0; k < 5; k++) if (keyp('1' + k)) switch_voice(k);
    if (keyp(KEY_TAB) || keyp('E')) switch_voice(vIdx + 1);

    // ── always-live controls ──
    if (keyp(KEY_SPACE)) { frozen = !frozen; if (frozen) sound_current(); }
    if (keyp(KEY_UP))   { morph += 0.05f; if (morph > 1) morph = 1; define_test(); if (frozen) sound_current(); }
    if (keyp(KEY_DOWN)) { morph -= 0.05f; if (morph < 0) morph = 0; define_test(); if (frozen) sound_current(); }
    if (keyp('R'))      { refOn = !refOn; if (frozen) sound_current(); }

    if (frozen) {
        if (keyp(KEY_LEFT))  { if (midi > LO) midi--; sound_current(); }
        if (keyp(KEY_RIGHT)) { if (midi < HI) midi++; sound_current(); }
        return;   // hold the current note ringing
    }

    if (keyp(KEY_RIGHT)) { stepFrames -= 4; if (stepFrames < 10) stepFrames = 10; }
    if (keyp(KEY_LEFT))  { stepFrames += 4; if (stepFrames > 80) stepFrames = 80; }

    // ── auto ping-pong sweep ──
    fnum++;
    int period = stepFrames + gapFrames;
    int idx    = fnum / period;
    int local  = fnum % period;
    int span   = HI - LO;
    int cyc    = idx % (2 * span);
    midi = (cyc <= span) ? LO + cyc : HI - (cyc - span);

    if (local == 0)                       sound_current();
    else if (local == stepFrames && sounding) silence();

#ifdef DE_TRACE
    watch("midi",  "%d", midi);
    watch("morph", "%.2f", morph);
    watch("vox",   "%d", vIdx);
#endif
}

static void note_name(int m, char *out, int n) {
    snprintf(out, n, "%s%d", NOTE_NAMES[m % 12], m / 12 - 1);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    const Voice *v = &VOICES[vIdx];
    print("PIPE TUNER  -  hear it drift", 8, 6, CLR_WHITE);
    print(refOn ? "voice + SINE ref" : "voice ALONE (ref off)", 8, 16,
          refOn ? CLR_LIGHT_GREY : CLR_ORANGE);

    // big current note
    char nm[8]; note_name(midi, nm, sizeof nm);
    float hz = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
    print(nm, SCREEN_W / 2 - 16, SCREEN_H / 2 - 24, CLR_YELLOW);
    char buf[64];
    snprintf(buf, sizeof buf, "midi %d   %.1f Hz", midi, (double)hz);
    print(buf, SCREEN_W / 2 - 44, SCREEN_H / 2 - 6, CLR_LIGHT_GREY);

    // voice name + its expected-tuning note
    print(v->name, 8, SCREEN_H - 62, CLR_LIME_GREEN);
    if (v->note[0])
        print(v->note, 8, SCREEN_H - 52, v->bad ? CLR_ORANGE : CLR_LIGHT_GREY);

    // live embouchure readout (orange when modified off the preset, or in PIPE's flat zone)
    bool modified = fabsf(morph - v->m) > 0.001f;
    snprintf(buf, sizeof buf, "embouchure (morph) %.2f%s", (double)morph, modified ? "  (modified)" : "");
    print(buf, 8, SCREEN_H - 40, (modified || (v->id == INSTR_PIPE && morph < 0.5f)) ? CLR_ORANGE : CLR_LIGHT_GREY);

    // pitch ladder: where we are LO..HI
    int x0 = SCREEN_W - 14, y0 = 26, h = SCREEN_H - 86;
    rect(x0, y0, 6, h, CLR_INDIGO);
    int py = y0 + h - (midi - LO) * h / (HI - LO);
    rectfill(x0 - 2, py - 1, 10, 3, CLR_YELLOW);

    print(frozen ? "PAUSED  LEFT/RIGHT +-semitone  SPACE play"
                 : "1-5 pipe presets  TAB voice  UP/DN morph  R ref  SPACE pause",
          8, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
