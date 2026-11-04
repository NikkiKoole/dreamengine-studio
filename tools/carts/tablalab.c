/* de:meta
{
  "slug": "tablalab",
  "title": "tablalab",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "additive-synth",
    "drum-synthesis"
  ],
  "lineage": "A measuring bench for the tabla's STROKES, made after tablahand sounded right in its playing and wrong in its sound. Recipes come from measured single strokes: Freesound CC0 pack 'tabla bols' by mmiron (pack 8162) and the CC BY 4.0 pack by ajaysm (pack 10737), used for measurement only, not shipped. The physics it tests is C.V. Raman's (1920, 1934): the syahi tunes the dayan's overtones into a near-harmonic series, and a stroke is mostly a choice of which overtones the fingers let ring.",
  "description": {
    "summary": "Each tabla stroke built by hand from its measured overtones: which ones ring, how loud, how long each lasts. Na is the 3rd overtone over the 2nd with the fundamental damped; tun is the fundamental alone; tin is the 2nd; the bayan's boom is a pitch falling five semitones. TAB A/Bs every stroke against INSTR_MEMBRANE.",
    "detail": "Five sine voices are the dayan's overtones (the lowest mode at 314 Hz, then 2, 3, 4, 5 times a 279 Hz series: the measured tuning, where the lowest mode sits above the series). Each stroke sets each voice's loudness and decay from the recordings, so the 2nd overtone can outlast the fundamental as it does on a real drum (t60 2.1 s against 1.3 s), which the membrane engine cannot. Closed strokes are a short low thud, not a quiet ring. The bayan is three voices whose pitch falls about five semitones over 0.2 s after the strike, the measured boom. Probe build (DE_DEFINES=PROBE_BOL=n) renders one stroke for the A/B against the recordings.",
    "controls": "A tun · S na · D na-open · F tin · G te · H ge · J ke · K dha · L dhin · TAB lab / membrane · SPACE theka on/off"
  }
}
de:meta */
// tablalab — tabla strokes as recipes of measured overtones.
//
// Why additive: the measured strokes differ mostly in WHICH overtones ring and HOW LONG each
// lasts (na: 3rd over 2nd, fundamental damped; tun: fundamental alone; tas/tin: the 2nd). The
// membrane engine has one strike-position knob that tilts all six modes together and always
// makes higher modes die faster, so every stroke comes out the same sound, brighter or darker.
// One sine per overtone lets each stroke be exactly its recipe. If the recipes hold, they are
// the spec for a per-stroke "touch" control on INSTR_MEMBRANE.
//
// Each partial's decay comes from the slot's ADSR decay (sustain 0), set just before booking.

#include "studio.h"
#include <math.h>
#include <string.h>

// dayan partial voices, bayan partial voices, the thuds, the click, the old membrane pair
#define DP0  10          // 10..14 dayan overtones
#define NDP  5
#define BP0  15          // 15..17 bayan overtones
#define NBP  3
#define D_THUD 18
#define B_THUD 19
#define CLICK  20
#define M_D    21        // INSTR_MEMBRANE dayan (the old sound, for A/B)
#define M_B    22

// measured tuning (mmiron's drum): lowest mode 314 Hz, then a 279 Hz harmonic series
static const float DAYAN_HZ[NDP] = { 314.0f, 558.0f, 837.0f, 1116.0f, 1395.0f };
// bayan (ajaysm ga/dhin): ~117 Hz settling to ~89 after the fall; partials ~1, 2, 3
#define BAYAN_HZ 89.0f
static const float BAYAN_RATIO[NBP] = { 1.0f, 1.97f, 2.95f };

typedef struct {
    const char *name;
    float dg[NDP], dt[NDP];   // dayan partial gain (dB, -99 = off) and t60 (s)
    float thud;               // closed-stroke thud on the dayan 0..1
    float click;              // contact snap 0..1
    int   bayan;              // 0 none, 1 open ge, 2 closed ke
} Stroke;

#define OFF -99
static const Stroke STROKES[] = {
    //  measured: onset level (dB, the first 50 ms) and t60 (s) per overtone, fitted to the recordings
    { "tun",  {   0, -27, -23, -40, -36 }, { 2.2f, 1.8f, 0.46f, 0.4f, 0.22f }, 0, 0.15f, 0 },
    { "na",   { -30, -23,   0, -10, -10 }, { 0.16f, 1.3f, 0.42f, 0.33f, 0.13f }, 0, 0.5f, 0 },
    { "naopen",{  -1, -18,   0, -10,  -8 }, { 1.3f, 2.4f, 0.38f, 0.4f, 0.12f }, 0, 0.35f, 0 },
    { "tin",  {  -4,   0,  -8,  -5, -16 }, { 0.18f, 1.6f, 0.24f, 0.6f, 0.5f }, 0, 0.3f, 0 },
    { "te",   { -24, -24, -18, OFF, OFF }, { 0.05f, 0.05f, 0.05f, 0.05f, 0.05f }, 1.0f, 0.6f, 0 },
    { "ge",   { OFF, OFF, OFF, OFF, OFF }, { 0,0,0,0,0 }, 0, 0.1f, 1 },
    { "ke",   { OFF, OFF, OFF, OFF, OFF }, { 0,0,0,0,0 }, 0, 0.4f, 2 },
    { "dha",  { -30, -23,   0, -10, -10 }, { 0.16f, 1.3f, 0.42f, 0.33f, 0.13f }, 0, 0.5f, 1 },
    { "dhin", {  -4,   0,  -8,  -5, -16 }, { 0.18f, 1.6f, 0.24f, 0.6f, 0.5f }, 0, 0.3f, 1 },
};
#define NSTROKE ((int)(sizeof STROKES / sizeof STROKES[0]))
static const char KEYS[NSTROKE] = { 'A','S','D','F','G','H','J','K','L' };

static bool lab = true, theka = false;
static double t_beat;
static int    tbeat;
static float  flash[NSTROKE];

static float db2g(float db) { return db <= OFF ? 0 : powf(10.0f, db / 20.0f); }

static void setup_sound(void) {
    for (int i = 0; i < NDP; i++) instrument_pan(DP0 + i, 0.15f);
    for (int i = 0; i < NBP; i++) instrument_pan(BP0 + i, -0.15f);
    // the closed dayan stroke: a short low thud (measured cluster ~120-370 Hz, ~80-180 ms)
    instrument(D_THUD, INSTR_MEMBRANE, 1, 0, 7, 40);
    instrument_harmonics(D_THUD, 0.9f); instrument_timbre(D_THUD, 0.35f); instrument_morph(D_THUD, 0.1f);
    instrument_pan(D_THUD, 0.15f);
    // the closed bayan slap: lower, shorter (ke ~40 ms, 60-230 Hz)
    instrument(B_THUD, INSTR_MEMBRANE, 1, 0, 7, 30);
    instrument_harmonics(B_THUD, 0.95f); instrument_timbre(B_THUD, 0.5f); instrument_morph(B_THUD, 0.0f);
    instrument_pan(B_THUD, -0.15f);
    // the contact snap: a few ms of bright noise
    instrument(CLICK, INSTR_NOISE, 0, 6, 0, 4);
    instrument_filter(CLICK, FILTER_HIGH, 2500, 2);
    // the old sound, as tablahand voices it
    instrument(M_D, INSTR_MEMBRANE, 1, 0, 7, 260);
    instrument_harmonics(M_D, 0.10f); instrument_timbre(M_D, 0.45f); instrument_morph(M_D, 0.12f);
    instrument(M_B, INSTR_MEMBRANE, 1, 0, 7, 420);
    instrument_harmonics(M_B, 0.22f); instrument_timbre(M_B, 0.30f); instrument_morph(M_B, 0.30f);
    for (int s = DP0; s <= M_B; s++) instrument_reverb(s, 0.14f);
    reverb(0.35f, 0.5f);
}

static void sine_partial(int slot, float hz, double t, float gain, float t60) {
    if (gain < 0.0015f) return;
    // The ADSR decay is LINEAR in amplitude (measured), a real head decays exponentially. A linear
    // ramp is down 20 dB at 90% of its length, an exponential with this t60 at a third of it, so
    // 0.37 * t60 matches the first 20-30 dB, the part the ear judges. Sustain 0, long gate.
    instrument(slot, INSTR_SINE, 1, (int)(t60 * 370.0f), 0, 60);
    float midi = 69.0f + 12.0f * log2f(hz / 440.0f);
    int m = (int)floorf(midi + 0.5f);
    instrument_tune(slot, midi - m);
    NoteOpts o = { .gain = gain };
    schedule_note_at(t, m, slot, 7, (int)(t60 * 1000.0f) + 100, &o);
}

static void play_lab(int k, double t, float force) {
    const Stroke *s = &STROKES[k];
    for (int i = 0; i < NDP; i++)
        sine_partial(DP0 + i, DAYAN_HZ[i], t, db2g(s->dg[i]) * force * 0.55f, s->dt[i]);
    if (s->thud > 0) {
        NoteOpts o = { .gain = s->thud * force * 0.9f };
        schedule_note_at(t, 55, D_THUD, 7, 60, &o);              // ~196 Hz
    }
    if (s->click > 0) {
        NoteOpts o = { .gain = s->click * force * 0.25f };
        schedule_note_at(t, 60, CLICK, 7, 8, &o);
    }
    if (s->bayan == 1) {
        // the boom: three partials whose pitch falls ~5 semitones over ~0.2 s (measured 117 -> 89 Hz)
        static const float BG[NBP] = { 1.0f, 0.22f, 0.1f }, BT[NBP] = { 1.4f, 0.45f, 0.25f };
        for (int i = 0; i < NBP; i++) {
            instrument_env(BP0 + i, 0, ENV_PITCH, 0, 300, 5.0f);   // measured: HOLDS ~90 ms, then drops; one AD env cannot hold, so a longer fall
            sine_partial(BP0 + i, BAYAN_HZ * BAYAN_RATIO[i], t, BG[i] * force * 0.8f, BT[i]);
        }
    } else if (s->bayan == 2) {
        NoteOpts o = { .gain = force * 0.9f };
        schedule_note_at(t, 38, B_THUD, 7, 40, &o);
    }
}

// the same stroke on INSTR_MEMBRANE, roughly as tablahand plays it
static void play_membrane(int k, double t, float force) {
    const Stroke *s = &STROKES[k];
    bool right = s->dg[0] > OFF || s->dg[1] > OFF;
    if (right) {
        float pos = (k == 1 || k == 2 || k == 7) ? 0.38f : (k == 4 ? -0.15f : 0);
        NoteOpts o = { .gain = force, .timbre = pos };
        schedule_note_at(t, 62, s->thud > 0 ? D_THUD : M_D, 7, s->thud > 0 ? 40 : 900, &o);
    }
    if (s->bayan == 1) { NoteOpts o = { .gain = force }; schedule_note_at(t, 38, M_B, 7, 1100, &o); }
    if (s->bayan == 2) { NoteOpts o = { .gain = force * 0.8f }; schedule_note_at(t, 45, B_THUD, 7, 30, &o); }
}

static void play(int k, double t, float force) {
    if (lab) play_lab(k, t, force); else play_membrane(k, t, force);
    flash[k] = 1;
}

// teental, plain (the bench is about the SOUND; tablahand is about the playing)
static const int THEKA[16] = { 7,8,8,7, 7,8,8,7, 7,3,3,1, 1,8,8,7 };   // dha dhin dhin dha ... tin tin na(ta)

void init(void) {
    setup_sound();
#ifdef PROBE_THEKA   // DE_DEFINES=PROBE_THEKA[,PROBE_MEMBRANE]: start in theka, for A/B renders
    theka = true; t_beat = audio_time() + 0.2;
#ifdef PROBE_MEMBRANE
    lab = false;
#endif
#endif
#ifdef PROBE_BOL     // DE_DEFINES=PROBE_BOL=<n>[,PROBE_MEMBRANE]: one stroke, for the A/B with a recording
#ifdef PROBE_MEMBRANE
    lab = false;
#endif
    play(PROBE_BOL, audio_time() + 0.2, 0.9f);
#endif
}

void update(void) {
#ifdef PROBE_BOL
    return;
#endif
    if (keyp(KEY_TAB)) lab = !lab;
    if (keyp(KEY_SPACE)) { theka = !theka; t_beat = audio_time() + 0.1; tbeat = 0; }
    for (int k = 0; k < NSTROKE; k++) if (keyp(KEYS[k])) play(k, audio_time() + 0.03, 0.9f);
    if (theka) {
        double len = 60.0 / 96;
        while (t_beat < audio_time() + 0.12) {
            play(THEKA[tbeat], t_beat, tbeat == 0 ? 1.0f : tbeat % 4 == 0 ? 0.85f : 0.7f);
            t_beat += len; tbeat = (tbeat + 1) % 16;
        }
    }
    for (int k = 0; k < NSTROKE; k++) flash[k] *= 0.9f;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print(lab ? "LAB (measured overtones)" : "MEMBRANE (the old sound)", 4, 4, lab ? CLR_GREEN : CLR_ORANGE);
    // each stroke: its recipe as five bars (loudness = height, decay = width)
    for (int k = 0; k < NSTROKE; k++) {
        int x = 6 + (k % 5) * 63, y = 30 + (k / 5) * 78;
        const Stroke *s = &STROKES[k];
        rect(x - 2, y - 2, 60, 66, flash[k] > 0.1f ? CLR_YELLOW : CLR_DARK_GREY);
        print(str("%c %s", KEYS[k], s->name), x, y, CLR_WHITE);
        for (int i = 0; i < NDP; i++) {
            float g = db2g(s->dg[i]);
            int h = (int)(g * 34);
            int w = 2 + (int)(s->dt[i] * 3);
            if (h > 0) rectfill(x + 2 + i * 11, y + 50 - h, w > 9 ? 9 : w, h, i == 0 ? CLR_PINK : CLR_LIGHT_GREY);
        }
        if (s->thud > 0) print("thud", x + 2, y + 52, CLR_ORANGE);
        if (s->bayan == 1) print("+boom", x + 26, y + 52, CLR_BLUE);
        if (s->bayan == 2) print("+slap", x + 26, y + 52, CLR_BLUE);
    }
    print("bars: overtone 1-5, height = loud, width = how long", 4, 170, CLR_LIGHT_GREY);
    print(str("TAB lab/membrane   SPACE theka %s", theka ? "on" : "off"), 4, 184, CLR_LIGHT_GREY);
}
