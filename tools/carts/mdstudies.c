/* de:meta
{
  "slug": "mdstudies",
  "title": "mega drive style studies",
  "status": "active",
  "created": "2026-11-17",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "step-sequencer",
    "fm-synth"
  ],
  "lineage": "Follow-up to dreamchip: four short ORIGINAL loops, each in the production style of a classic Mega Drive soundtrack family, to hear how far dreamengine's FM4 (alt mapping) plus DAC-style drum samples gets toward that sound. Groundwork for a Mega Drive patch set in other projects.",
  "description": {
    "summary": "Four original loops in classic Mega Drive styles, on FM4 and gritty DAC drums.",
    "detail": "1 CLUB: 909-style DAC drums, rubbery FM bass, house-piano stabs, a sweeping FM pad (the Streets of Rage family). 2 BRIGHT POP: FM brass stabs, an FM marimba line, a bouncy bass (the Sonic-era family). 3 FM ROCK: power chords from heavy operator feedback and drive, a fast FM arpeggio, rock drums with tom fills (the Thunder Force family). 4 AMBIENT: glassy FM pads, a soft bell with echoes made from quieter repeats (the Ecco family). 5 SLOW JAM: a swung 70s soul groove: round fingered FM bass with slides, muted wah guitar, tremolo e-piano, lush strings, soft drums and congas. All compositions are original; only the instruments and production style follow those soundtracks.",
    "controls": "It tours all five. 1-5 (or tap a study) jumps to one and keeps looping it. 0 / SPACE goes back to the tour."
  }
}
de:meta */
// mdstudies — four original loops in classic Mega Drive production styles.
// Built on dreamchip's voicing: FM4 on its alt mapping (harmonics = algorithm,
// MODE_FM4_R0..R3 = operator ratios, timbre = modulation index, morph = feedback),
// a per-note brightness drop for the FM "pluck", and drums as 8-bit samples held at
// the DAC's ~8 kHz like the Mega Drive streams them. No echo/reverb unit: the ambient
// study makes its echoes from quieter repeated notes, as Mega Drive composers did.
#include "studio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// ---------------------------------------------------------------- shared voicing
#define ALG(n) (((n) + 0.5f) / 8.0f)
#ifndef MATCHED_LEVEL
#define MATCHED_LEVEL 1.0f                         // set so both basses sit at the same loudness
#endif
static float R(float ratio) {                  // snap a ratio to FM4's 13-value table
    static const float T[13] = {0.5f, 1, 1.414f, 1.5f, 1.618f, 2, 2.414f, 3, 3.5f, 4, 5, 7, 14};
    int best = 0;
    for (int i = 1; i < 13; i++) if (fabsf(T[i] - ratio) < fabsf(T[best] - ratio)) best = i;
    return (best + 0.5f) / 13.0f;
}
static void fm(int slot, int a, int d, int s, int r, int alg, float r0, float r1, float r2, float r3,
               float bright, float feedback) {
    instrument(slot, INSTR_FM4, a, d, s, r);
    instrument_mode(slot, MODE_FM4_MAP, 1.0f);
    instrument_harmonics(slot, ALG(alg));
    instrument_mode(slot, MODE_FM4_R0, R(r0));
    instrument_mode(slot, MODE_FM4_R1, R(r1));
    instrument_mode(slot, MODE_FM4_R2, R(r2));
    instrument_mode(slot, MODE_FM4_R3, R(r3));
    instrument_timbre(slot, bright);
    instrument_morph(slot, feedback);
    instrument_crush(slot, 10.0f, 1.0f, 1.0f);
    instrument_pan(slot, 0.0f);
    instrument_echo(slot, 0.0f);
    instrument_reverb(slot, 0.0f);
    instrument_drive(slot, 0.0f);
}
static void pluck(int slot, int ms, float amount) { instrument_env(slot, 0, ENV_TIMBRE, 1, ms, amount); }

// ---- DAC-style drums: 8-bit, sample-and-hold at ~8.2 kHz
enum { PK_KICK, PK_SNARE, PK_CLAP, PK_CHAT, PK_OHAT, PK_RKICK, PK_RSNARE, PK_CRASH };
static unsigned rngs = 0x5EED;
static float nz(void) { rngs = rngs * 1664525u + 1013904223u; return (float)(rngs >> 9) / 4194304.0f - 1.0f; }

typedef float (*DrumFn)(float t, int i);
static float k909(float t, int i) { (void)i; static float ph; if (t == 0) ph = 0;
    ph += 6.2832f * (52 + 130 * expf(-t / 0.025f)) / 44100.0f; return tanhf(1.6f * sinf(ph) * expf(-t / 0.14f)) + nz() * expf(-t / 0.002f) * 0.3f; }
static float s909(float t, int i) { (void)i; return 0.6f * sinf(6.2832f * 200 * t) * expf(-t / 0.04f) + nz() * expf(-t / 0.09f) * 0.8f; }
static float clap(float t, int i) { (void)i; float e = 0; for (int k = 0; k < 3; k++) { float tk = t - k * 0.011f; if (tk >= 0) e += expf(-tk / 0.006f); }
    return nz() * (e * 0.7f + expf(-t / 0.08f) * 0.6f); }
static float chat(float t, int i) { (void)i; return nz() * expf(-t / 0.012f); }
static float ohat(float t, int i) { (void)i; return nz() * expf(-t / 0.13f) * 0.8f; }
static float rkick(float t, int i) { (void)i; static float ph; if (t == 0) ph = 0;
    ph += 6.2832f * (60 + 160 * expf(-t / 0.02f)) / 44100.0f; return tanhf(2.2f * sinf(ph) * expf(-t / 0.10f)) + nz() * expf(-t / 0.003f) * 0.5f; }
static float rsnare(float t, int i) { (void)i; return tanhf(1.8f * (0.7f * sinf(6.2832f * 180 * t) * expf(-t / 0.05f) + nz() * expf(-t / 0.12f))); }
static float crash(float t, int i) { (void)i; return nz() * expf(-t / 0.7f) * 0.7f; }

static void make_drum(int slot, DrumFn fn, float secs) {
    const int sr = 44100;
    int n = (int)(secs * sr);
    float *buf = (float *)calloc((size_t)n, sizeof(float));
    float held = 0, next = 0, pk = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / sr;
        float v = fn(t, i);                          // advance any oscillator every sample
        if (t >= next) { held = roundf(fmaxf(-1, fminf(1, v)) * 127) / 127; next += 1.0f / 8191.0f; }
        buf[i] = held;
        if (fabsf(held) > pk) pk = fabsf(held);
    }
    if (pk > 0) for (int i = 0; i < n; i++) buf[i] *= 0.95f / pk;
    sample_load(slot, buf, n);
    free(buf);
}

#define S_DRUM0 40                                // drum instrument slots 40..47
static void drum_slot(int k, float level, float pan) {
    instrument(S_DRUM0 + k, INSTR_SAMPLE, 0, 0, 7, 40);
    instrument_sample(S_DRUM0 + k, k, 60);
    instrument_level(S_DRUM0 + k, level);
    instrument_pan(S_DRUM0 + k, pan);
}

// ---------------------------------------------------------------- the four studies
#define S_BASS 10
#define S_A    11       // per-study lead-ish part (stabs / brass / guitar / bell)
#define S_A2   12       // second voice of that part (guitar fifth, piano right hand...)
#define S_B    13       // second part (pad / marimba / arpeggio)
#define S_C    14       // third part (sub bass / pad)
#define S_D    15       // fourth part (slow jam: strings)

typedef struct { const char *name, *line1, *line2; int bpm, bars; } Study;
#define NSTUDY 5
static const Study STUDY[NSTUDY] = {
    {"1  CLUB", "909-style DAC drums, rubbery FM bass,", "house-piano stabs, a sweeping FM pad", 124, 16},
    {"2  BRIGHT POP", "FM brass stabs, an FM marimba line,", "a bouncy bass, light drums", 140, 16},
    {"3  FM ROCK", "power chords from operator feedback + drive,", "a fast FM arpeggio, rock drums, tom fills", 150, 16},
    {"4  AMBIENT", "glassy FM pads, a soft bell with echoes made", "from quieter repeats, a slow sub bass", 80, 8},
    {"5  SLOW JAM", "swung 70s soul: fingered FM bass with slides,", "wah guitar, tremolo e-piano, strings, congas", 92, 16},
};
#ifndef START_STUDY
#define START_STUDY 0
#endif
#ifndef BASS_VARIANT
#define BASS_VARIANT 0
#endif
static int cur = START_STUDY, tour = 1;
static int bass_variant = BASS_VARIANT;          // 0 = FM4 bass, 1 = the patch matcher's fit to a real YM2612 bass

// The patch matcher's best fit (loss 0.37) to the real Mega Drive bass from Mega Jams,
// recorded through MAME's YM2612: a blown-pipe voice, a low band-pass with a sweeping
// envelope, asymmetric drive, tape and EQ (zxspec/compare). Fitted on A2 only.
static void matched_bass(int slot, float level) {
    instrument(slot, INSTR_PIPE, 0, 1201, 0, 43);
    instrument_harmonics(slot, 0.210f); instrument_timbre(slot, 0.975f); instrument_morph(slot, 0.599f);
    instrument_filter(slot, FILTER_BAND, 42, 6);
    instrument_env(slot, 0, ENV_CUTOFF_OCT, 0, 175, 1.83f);
    instrument_lfo(slot, 1, LFO_VOLUME, 1.90f, 0.027f);
    instrument_drive(slot, 0.312f); instrument_drive_mode(slot, DRIVE_ASYM);
    instrument_tape(slot, 0.677f, 0.475f, 0.349f);
    instrument_tremolo(slot, 10.68f, 0.097f, LFO_SHAPE_SINE);
    instrument_eq(slot, -7.6f, -6.9f, 8.8f);
    instrument_pan(slot, 0.0f);
    instrument_level(slot, level);
}
static double t0;
static long step;
static float lamp[5];                             // bass, A, B, C, drums

static void book(double t, int slot, int midi, int vol, int dur_ms, int lampi) {
    schedule_note_at(t, midi, slot, vol, dur_ms, NULL);
    (void)lampi;
}
static void book_glide(double t, int slot, int midi, int vol, int dur_ms, int glide_ms) {
    NoteOpts o = {0};
    o.glide_ms = glide_ms;                         // slides the still-sounding note to this one
    schedule_note_at(t, midi, slot, vol, dur_ms, &o);
}
static void drum(double t, int k, int vol) { schedule_note_at(t, 60, S_DRUM0 + k, vol, 400, NULL); }
static void drum_at(double t, int k, int vol, int midi) { schedule_note_at(t, midi, S_DRUM0 + k, vol, 400, NULL); }

static void setup_study(int s) {
    note_off_all();
    for (int k = 0; k < 5; k++) lamp[k] = 0;
    switch (s) {
    case 0: // CLUB
        fm(S_BASS, 1, 260, 3, 90, 0, 1, 1, 2, 1, 0.42f, 0.55f); pluck(S_BASS, 120, 0.35f);
        instrument_drive(S_BASS, 0.15f); instrument_level(S_BASS, 0.85f);
        if (bass_variant) matched_bass(S_BASS, MATCHED_LEVEL);
        fm(S_A, 1, 240, 0, 160, 4, 1, 1, 7, 1, 0.48f, 0.12f); pluck(S_A, 70, 0.25f);   // piano stab
        instrument_level(S_A, 0.42f); instrument_pan(S_A, -0.35f);
        fm(S_A2, 1, 240, 0, 160, 4, 1, 1, 7, 1, 0.48f, 0.12f); pluck(S_A2, 70, 0.25f);
        instrument_level(S_A2, 0.42f); instrument_pan(S_A2, 0.35f);
        fm(S_B, 700, 0, 7, 900, 4, 1, 1, 2, 1, 0.30f, 0.25f);                          // sweep pad
        instrument_env(S_B, 0, ENV_TIMBRE, 1800, 1200, 0.30f);
        instrument_level(S_B, 0.28f);
        drum_slot(PK_KICK, 1.0f, 0); drum_slot(PK_CLAP, 0.75f, 0.1f);
        drum_slot(PK_CHAT, 0.30f, 0.35f); drum_slot(PK_OHAT, 0.35f, -0.3f);
        break;
    case 1: // BRIGHT POP
        fm(S_BASS, 1, 200, 3, 80, 0, 1, 1, 1, 1, 0.34f, 0.30f); pluck(S_BASS, 90, 0.25f);
        instrument_level(S_BASS, 0.8f);
        fm(S_A, 45, 220, 5, 140, 1, 1, 1, 1, 1, 0.62f, 0.40f);                          // brass
        instrument_env(S_A, 0, ENV_TIMBRE, 60, 200, 0.20f);
        instrument_level(S_A, 0.40f); instrument_pan(S_A, -0.3f);
        fm(S_A2, 45, 220, 5, 140, 1, 1, 1, 1, 1, 0.62f, 0.40f);
        instrument_env(S_A2, 0, ENV_TIMBRE, 60, 200, 0.20f);
        instrument_level(S_A2, 0.40f); instrument_pan(S_A2, 0.3f);
        fm(S_B, 1, 260, 0, 200, 4, 1, 4, 1, 3, 0.40f, 0.05f); pluck(S_B, 40, 0.30f);   // marimba
        instrument_level(S_B, 0.45f); instrument_pan(S_B, 0.45f);
        drum_slot(PK_RKICK, 0.9f, 0); drum_slot(PK_SNARE, 0.65f, 0);
        drum_slot(PK_CHAT, 0.28f, 0.3f);
        break;
    case 2: // FM ROCK
        fm(S_BASS, 1, 180, 4, 70, 0, 1, 1, 1, 0.5f, 0.38f, 0.45f); instrument_level(S_BASS, 0.75f);
        fm(S_A, 1, 120, 5, 60, 0, 1, 1, 1, 1, 0.70f, 0.95f);                           // guitar root
        instrument_drive(S_A, 0.65f); instrument_drive_mode(S_A, DRIVE_HARD);
        instrument_level(S_A, 0.42f); instrument_pan(S_A, -0.45f);
        fm(S_A2, 1, 120, 5, 60, 0, 1, 1, 1, 1, 0.70f, 0.95f);                          // guitar fifth
        instrument_drive(S_A2, 0.65f); instrument_drive_mode(S_A2, DRIVE_HARD);
        instrument_level(S_A2, 0.42f); instrument_pan(S_A2, 0.45f);
        fm(S_B, 1, 120, 2, 60, 6, 1, 2, 1, 1, 0.55f, 0.35f); pluck(S_B, 50, 0.30f);    // arp synth
        instrument_level(S_B, 0.32f);
        drum_slot(PK_RKICK, 1.0f, 0); drum_slot(PK_RSNARE, 0.8f, 0);
        drum_slot(PK_CHAT, 0.30f, 0.3f); drum_slot(PK_CRASH, 0.40f, -0.2f);
        break;
    case 3: // AMBIENT
        fm(S_B, 900, 0, 7, 2000, 4, 1, 1, 2.414f, 1, 0.22f, 0.10f);                    // glass pad
        instrument_env(S_B, 0, ENV_TIMBRE, 2500, 3000, 0.18f);
        instrument_level(S_B, 0.30f);
        fm(S_A, 2, 1400, 0, 1200, 4, 1, 1.414f, 1, 1.414f, 0.38f, 0.05f);              // bell
        instrument_level(S_A, 0.40f);
        fm(S_C, 400, 0, 7, 1500, 7, 1, 2, 1, 1, 0.05f, 0.0f);                         // sub
        instrument_level(S_C, 0.45f);
        break;
    case 4: // SLOW JAM
        fm(S_BASS, 2, 400, 4, 120, 0, 1, 1, 1, 1, 0.30f, 0.20f);                        // round fingered bass
        pluck(S_BASS, 140, 0.18f); instrument_mono(S_BASS, 1); instrument_level(S_BASS, 0.95f);
        if (bass_variant) { matched_bass(S_BASS, MATCHED_LEVEL); instrument_mono(S_BASS, 1); }
        fm(S_A, 1, 90, 0, 60, 0, 1, 3, 1, 1, 0.45f, 0.50f);                            // muted wah guitar
        instrument_filter(S_A, FILTER_BAND, 650, 9);
        instrument_env(S_A, 1, ENV_CUTOFF, 4, 120, 1900.0f);                          // the wah opens on each stroke
        instrument_level(S_A, 0.50f); instrument_pan(S_A, 0.4f);
        fm(S_B, 3, 900, 3, 700, 4, 1, 1, 14, 1, 0.34f, 0.08f); pluck(S_B, 90, 0.15f);  // e-piano
        instrument_lfo(S_B, 0, LFO_VOLUME, 4.6f, 0.35f);                               // tremolo
        instrument_level(S_B, 0.42f); instrument_pan(S_B, -0.3f);
        fm(S_D, 600, 0, 7, 1100, 1, 1, 1, 2, 1, 0.26f, 0.30f);                         // strings
        instrument_lfo(S_D, 0, LFO_PITCH, 5.2f, 0.06f);                                // a little vibrato
        instrument_level(S_D, 0.26f);
        drum_slot(PK_RKICK, 0.75f, 0); drum_slot(PK_SNARE, 0.45f, 0.05f);
        drum_slot(PK_CHAT, 0.22f, 0.35f); drum_slot(PK_OHAT, 0.22f, 0.35f);
        drum_slot(PK_KICK, 0.30f, -0.45f);                                             // the 909 kick pitched up = congas
        break;
    }
}

// chords as MIDI notes (low to high)
static const int CLUB_CH[4][5] = {{53, 56, 60, 63, 67}, {49, 53, 56, 60, 63}, {51, 56, 58, 63, 65}, {48, 51, 55, 58, 62}};
static const int CLUB_ROOT[4] = {41, 37, 39, 36};
static const int POP_CH[4][3] = {{64, 67, 72}, {64, 69, 72}, {65, 69, 72}, {62, 67, 71}};
static const int POP_ROOT[4] = {36, 33, 41, 43};
static const int POP_MAR[8] = {0, 2, 1, 2, 0, 1, 2, 1};                     // marimba: chord-tone pattern
static const int ROCK_ROOT[4] = {40, 36, 38, 35};                             // E C D B
static const int AMB_CH[4][4] = {{62, 66, 69, 76}, {59, 62, 66, 69}, {55, 62, 66, 73}, {57, 64, 69, 71}};
static const int AMB_ROOT[4] = {38, 35, 31, 33};
// slow jam: Em9 | A13 | Dmaj9 | Bm7 | Cmaj7 | B7#9 (8 bars, twice)
static const int JAM_SEQ[8] = {0, 0, 1, 1, 2, 3, 4, 5};
static const int JAM_ROOT[6] = {40, 45, 38, 47, 36, 47};
static const int JAM_CH[6][4] = {{55, 59, 62, 66}, {55, 61, 66, 71}, {54, 57, 61, 64}, {57, 62, 66, 69},
                                 {52, 55, 59, 62}, {51, 57, 62, 66}};
static const int AMB_BELL[8][2] = {{0, 81}, {6, 78}, {10, 76}, {24, 74}, {32, 81}, {38, 83}, {44, 78}, {56, 76}};   // (16th in 4 bars, note)

static void book_step(long s, double t, double st) {
    const Study *S = &STUDY[cur];
    int bar = (int)(s / 16) % S->bars, i = (int)(s % 16);
    int ms16 = (int)(st * 1000);
    switch (cur) {
    case 0: { // CLUB: chord changes every 2 bars
        int c = (bar / 2) % 4, root = CLUB_ROOT[c];
        static const int bp[16] = {1, 0, 0, 2, 0, 0, 1, 0, 1, 0, 2, 0, 3, 0, 1, 0};   // 1 root 2 octave 3 fifth
        if (bp[i]) { book(t, S_BASS, root + (bp[i] == 2 ? 12 : bp[i] == 3 ? 7 : 0), 6, ms16 * 2 - 20, 0); lamp[0] = 1; }
        if (i == 2 || i == 6 || i == 11 || i == 14) {                                       // piano stabs
            for (int k = 0; k < 3; k++) book(t, S_A, CLUB_CH[c][k] + 12, 5, ms16 * 2, 1);
            for (int k = 3; k < 5; k++) book(t, S_A2, CLUB_CH[c][k] + 12, 5, ms16 * 2, 1);
            lamp[1] = 1;
        }
        if (bar >= 4 && bar % 2 == 0 && i == 0) {                                           // pad
            for (int k = 0; k < 4; k++) book(t, S_B, CLUB_CH[c][k], 4, ms16 * 32 - 60, 2);
            lamp[2] = 1;
        }
        if (i % 4 == 0) drum(t, PK_KICK, 7);
        if (i == 4 || i == 12) drum(t, PK_CLAP, 6);
        if (i % 4 == 2) drum(t, PK_OHAT, 5);
        if (i % 2 == 1) drum(t, PK_CHAT, 4);
        if (i % 4 == 0 || i == 4 || i == 12) lamp[4] = 1;
        break; }
    case 1: { // BRIGHT POP: C Am F G, one bar each
        int c = bar % 4, root = POP_ROOT[c];
        static const int bp[16] = {1, 0, 0, 2, 3, 0, 1, 0, 1, 0, 0, 2, 3, 0, 2, 0};
        if (bp[i]) { book(t, S_BASS, root + (bp[i] == 2 ? 12 : bp[i] == 3 ? 7 : 0), 6, ms16 * 2 - 20, 0); lamp[0] = 1; }
        if (i == 0 || i == 6 || i == 10) {                                                  // brass stabs
            int len = i == 0 ? 3 : i == 6 ? 2 : 4;
            book(t, S_A, POP_CH[c][0], 5, ms16 * len, 1); book(t, S_A, POP_CH[c][1], 5, ms16 * len, 1);
            book(t, S_A2, POP_CH[c][2], 5, ms16 * len, 1);
            lamp[1] = 1;
        }
        if (bar >= 4 && i % 2 == 0) {                                                       // marimba line
            int tone = POP_CH[c][POP_MAR[i / 2]] + 12 + (i == 14 ? 12 : 0);
            book(t, S_B, tone, 5, ms16 * 2, 2); lamp[2] = 1;
        }
        if (i == 0 || i == 7 || i == 8) drum(t, PK_RKICK, 7);
        if (i == 4 || i == 12) drum(t, PK_SNARE, 6);
        if (i % 2 == 0) drum(t, PK_CHAT, 4);
        if (bar % 4 == 3 && i >= 13) drum(t, PK_SNARE, 5);                                  // little fill
        if (i == 0 || i == 4 || i == 8 || i == 12) lamp[4] = 1;
        break; }
    case 2: { // FM ROCK: E C D B, one bar each, chugging 16ths
        int c = bar % 4, root = ROCK_ROOT[c];
        static const int rest[16] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1};
        static const int acc[16] = {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0};
        if (!rest[i]) {
            int v = acc[i] ? 6 : 4, len = acc[i] ? ms16 * 2 : ms16 - 25;                     // accents ring, the rest palm-muted
            book(t, S_A, root + 12, v, len, 1); book(t, S_A2, root + 19, v, len, 1);
            lamp[1] = 1;
        }
        if (i % 2 == 0) { book(t, S_BASS, root + (i % 4 == 2 ? 12 : 0), 6, ms16 * 2 - 20, 0); lamp[0] = 1; }
        if (bar >= 4) {                                                                     // fast arpeggio
            static const int up[16] = {0, 7, 12, -1, 19, -1, 12, 7, 0, 7, 12, -1, 19, -1, 12, 7};   // -1 = the third
            int third = c == 0 ? 3 : 4;                                                     // E minor; C, D, B major
            int tone = root + 36 + (up[i] < 0 ? 12 + third : up[i]);
            book(t, S_B, tone, 4, ms16, 2); lamp[2] = 1;
        }
        if (bar % 4 == 3 && i >= 8) {                                                       // tom fill: the kick pitched up
            if (i % 2 == 0) drum_at(t, PK_RKICK, 6, 72 - (i - 8));
        } else {
            if (i == 0 || i == 2 || i == 8 || i == 10 || i == 11) drum(t, PK_RKICK, 7);
            if (i == 4 || i == 12) drum(t, PK_RSNARE, 7);
            if (i % 2 == 0) drum(t, PK_CHAT, 4);
        }
        if (i == 0 && bar % 4 == 0) drum(t, PK_CRASH, 5);
        if (i % 4 == 0) lamp[4] = 1;
        break; }
    case 3: { // AMBIENT: a chord every 2 bars, a bell phrase over 4 bars with hand-made echoes
        int c = (bar / 2) % 4;
        if (i == 0 && bar % 2 == 0) {
            for (int k = 0; k < 4; k++) book(t, S_B, AMB_CH[c][k], 4, ms16 * 32 - 80, 2);
            book(t, S_C, AMB_ROOT[c], 5, ms16 * 32 - 80, 3);
            lamp[2] = lamp[3] = 1;
        }
        int pos = (bar % 4) * 16 + i;
        for (int k = 0; k < 8; k++) if (AMB_BELL[k][0] == pos) {
            book(t, S_A, AMB_BELL[k][1], 6, 900, 1); lamp[1] = 1;
            for (int e = 1; e <= 3; e++)                                                    // echoes: quieter repeats
                book(t + e * 3 * st, S_A, AMB_BELL[k][1], 6 - e * 2 > 1 ? 6 - e * 2 : 1, 700, 1);
        }
        break; }
    case 4: { // SLOW JAM: swung 16ths, a chord a bar
        int c = JAM_SEQ[bar % 8], root = JAM_ROOT[c];
        double ts = t + (i % 2 ? 0.24 * st : 0.0);                                         // swing
        // bass: (step, offset from root, length in 16ths, volume, slide-in)
        static const int bl[8][5] = {{0, 0, 3, 6, 0}, {3, 0, 1, 3, 0}, {6, 12, 1, 6, 0}, {8, 7, 2, 5, 0},
                                     {10, 10, 2, 5, 0}, {11, 12, 2, 6, 1}, {13, 7, 1, 4, 0}, {14, -1, 2, 5, 0}};
        for (int k = 0; k < 8; k++) if (bl[k][0] == i) {
            int len = (int)(bl[k][2] * ms16) - 15;
            if (bl[k][4]) book_glide(ts, S_BASS, root + bl[k][1], bl[k][3], len, 70);
            else book(ts, S_BASS, root + bl[k][1], bl[k][3], len, 0);
            lamp[0] = 1;
        }
        static const int wah[16] = {0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0};    // muted scratches
        if (wah[i]) {
            book(ts, S_A, JAM_CH[c][1] + 12, (i == 6 || i == 14) ? 6 : 4, 70, 1);
            book(ts, S_A, JAM_CH[c][3] + 12, (i == 6 || i == 14) ? 6 : 4, 70, 1);
            lamp[1] = 1;
        }
        if (i == 0 || i == 6 || i == 14) {                                                  // lazy e-piano comps
            int len = i == 0 ? ms16 * 5 : ms16 * 3;
            for (int k = 0; k < 4; k++) book(ts, S_B, JAM_CH[c][k], i == 0 ? 5 : 4, len, 2);
            lamp[2] = 1;
        }
        if (bar >= 4 && i == 0) {                                                           // strings
            for (int k = 0; k < 4; k++) book(t, S_D, JAM_CH[c][k] + 12, 4, ms16 * 16 - 40, 3);
            lamp[3] = 1;
        }
        if (i == 0 || i == 10) drum(ts, PK_RKICK, 6);
        if (i == 7 && bar % 2) drum(ts, PK_RKICK, 4);
        if (i == 4 || i == 12) drum(ts, PK_SNARE, 5);
        if (i % 2 == 0 && i != 14) drum(ts, PK_CHAT, (i % 4 == 0) ? 4 : 3);
        if (i == 14) drum(ts, PK_OHAT, 4);
        if (i == 3 || i == 9 || i == 11) drum_at(ts, PK_KICK, 4, i == 9 ? 79 : 74);   // congas
        if (i % 4 == 0) lamp[4] = 1;
        break; }
    }
}

static void start_study(int s) {
    cur = s;
    setup_study(s);
    t0 = audio_time() + 0.25;
    step = 0;
}

void update(void) {
    static bool boot = false;
    if (!boot) {
        make_drum(PK_KICK, k909, 0.35f); make_drum(PK_SNARE, s909, 0.25f); make_drum(PK_CLAP, clap, 0.25f);
        make_drum(PK_CHAT, chat, 0.06f); make_drum(PK_OHAT, ohat, 0.35f); make_drum(PK_RKICK, rkick, 0.3f);
        make_drum(PK_RSNARE, rsnare, 0.3f); make_drum(PK_CRASH, crash, 1.2f);
        start_study(START_STUDY);
        boot = true;
    }
    for (int k = 0; k < NSTUDY; k++) if (keyp('1' + k)) { tour = 0; start_study(k); }
    if (keyp('0') || keyp(KEY_SPACE)) { tour = 1; start_study(0); }
    if (keyp('b') || keyp('B')) { bass_variant ^= 1; start_study(cur); }
    if (mouse_pressed(MOUSE_LEFT)) {
        int my = mouse_y();
        for (int k = 0; k < NSTUDY; k++) if (my >= 86 + k * 13 && my < 86 + k * 13 + 12) { tour = 0; start_study(k); }
    }

    const Study *S = &STUDY[cur];
    double st = 60.0 / S->bpm / 4.0;
    double now_t = audio_time();
    long loop = (long)S->bars * 16;
    while (t0 + step * st < now_t + 0.15) {
        if (tour && step == loop) { start_study((cur + 1) % NSTUDY); return; }
        book_step(step % loop, t0 + step * st, st);
        step++;
    }
    for (int k = 0; k < 5; k++) lamp[k] *= 0.9f;
}

void draw(void) {
    camera(0, 0);
    cls(CLR_DARKER_BLUE);
    font(FONT_NORMAL);
    print("MEGA DRIVE STYLE STUDIES", 8, 8, CLR_YELLOW);
    font(FONT_SMALL);
    print("original loops on FM4 + DAC drums", 8, 20, CLR_INDIGO);
    const Study *S = &STUDY[cur];
    font(FONT_NORMAL);
    print(S->name + 3, 8, 40, CLR_WHITE);
    font(FONT_SMALL);
    print(S->line1, 8, 54, CLR_LIGHT_GREY);
    print(S->line2, 8, 62, CLR_LIGHT_GREY);
    char buf[48];
    double st = 60.0 / S->bpm / 4.0;
    long played = (long)((audio_time() - t0) / st);
    if (played < 0) played = 0;
    snprintf(buf, sizeof buf, "%d BPM   BAR %ld/%d   %s", S->bpm, played / 16 % S->bars + 1, S->bars,
             tour ? "TOUR" : "LOOP");
    print(buf, 8, 74, CLR_BLUE);
    print(bass_variant ? "BASS: MATCHED (B)" : "BASS: FM4 (B)", 220, 74, bass_variant ? CLR_ORANGE : CLR_INDIGO);
    for (int k = 0; k < NSTUDY; k++) {
        int y = 86 + k * 13;
        rectfill(6, y - 2, 150, 12, k == cur ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
        print(STUDY[k].name, 10, y + 1, k == cur ? CLR_WHITE : CLR_INDIGO);
    }
    static const char *parts[5] = {"BASS", "A", "B", "C", "DRUM"};
    for (int k = 0; k < 5; k++) {
        int x = 172 + k * 28;
        rectfill(x, 92, 22, 54, CLR_BROWNISH_BLACK);
        int h = (int)(lamp[k] * 50);
        rectfill(x + 2, 144 - h, 18, h, k == 4 ? CLR_YELLOW : k == 0 ? CLR_GREEN : CLR_PINK);
        print(parts[k], x + 1, 148, CLR_INDIGO);
    }
    float sc[300];
    scope_read(sc, 300);
    rectfill(8, 160, 304, 36, CLR_BROWNISH_BLACK);
    for (int x = 1; x < 300; x++) {
        int y0 = 178 - (int)(sc[x - 1] * 40.0f), y1 = 178 - (int)(sc[x] * 40.0f);
        y0 = y0 < 161 ? 161 : y0 > 195 ? 195 : y0; y1 = y1 < 161 ? 161 : y1 > 195 ? 195 : y1;
        line(9 + x, y0, 10 + x, y1, CLR_BLUE);
    }
    font(FONT_NORMAL);
}
