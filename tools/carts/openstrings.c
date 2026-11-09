/* de:meta
{
  "slug": "openstrings",
  "title": "openstrings",
  "status": "wip",
  "created": "2026-11-09",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "waveguide-synth",
    "chord-voicing"
  ],
  "lineage": "The first cart on resonator(): a SYMPATHETIC RESONATOR, a bank of tuned strings on a send. The idea comes from Flyfish/Tuna on the M-VAVE FM-1 (GPL-3.0; the idea only, no code), where 8 voices cannot hold a pedalled arpeggio, so each note is short and a resonator tuned to the harmony makes the pedal. docs/design/sympathetic-resonator.md",
  "description": {
    "summary": "A rolling piano arpeggio of SHORT, dry notes, and a bank of eight open strings tuned to each chord that rings under it like a held sustain pedal. R switches the strings off and on, so you hear what they do.",
    "detail": "Every piano note here is cut short (the damper falls after 180 ms), so on its own it sounds like a dry, plinky piano. The pedal comes from the resonator: eight delay-loop strings, retuned at each chord change to the bass, its fifth and the chord tones over three octaves. The piano's sound excites them, and they ring on at those notes. Strings that keep their note across a change ring on; the rest are damped for 25 ms and retuned, like a pedal change. The strings on screen show which notes are tuned and ring with the chord.",
    "controls": "R strings on/off · P synth piano / Steinway samples · UP/DOWN ring time · LEFT/RIGHT brightness · SPACE stop/play. Piano samples: bitKlavier Grand Sample Library, Daniel Trueman, Princeton University (CC BY 4.0)"
  },
  "todo": [
    "BAKE the Steinway into the cart (web/app builds cannot read assets/ at runtime), shrunk: per-register length 3.1 s → 0.7 s, 22 kHz, maybe ADPCM (3.5 MB → ~0.25-1 MB). STATUS Open 'SAMPLED STEINWAY'.",
    "Replace the 13 instrument slots (one per zone) with a real MULTISAMPLE instrument: one slot, zones by key (engine work).",
    "VELOCITY layers: one layer today (v12 of 16), so soft notes are only quieter, not darker. Take more layers from Princeton's original, keep the CC BY credit.",
    "LISTEN: resonator level 0.6 and the 25 ms retune damping — too much, too little? Then decide whether pianohand's hand-built halo voice gives way to resonator().",
    "MODELLED PIANO vs THE STEINWAY: the measuring pass landed as an opt-in, MODE_PIANO_MODEL 1 (this cart uses it): felt hammer, decay in dB/s, onset ramp, knock off; measured with the pianoref probe (S = ours then the real note). By ear the tail is good, the onset is not yet. NEXT: the HYBRID (commuted synthesis), the real Steinway first ~80 ms cross-faded into the modelled tail, as a further MODE_PIANO_MODEL value. Watch the splice (click-check.js) and match pitch + partial levels at the joint."
  ]
}
de:meta */
// openstrings — the sympathetic resonator, heard against a dry piano.
//
// THE POINT. A pedalled arpeggio wants every note to ring on, which costs a voice per note. Here
// each note is SHORT (180 ms) and the ring comes from ONE shared bank of strings tuned to the
// harmony: resonator() / resonator_tune() / instrument_resonator(). R is the A/B.
#include "studio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SL_PNO 10
#define SL_BAS 11
#define BPM_   84
#define STEPS_PER_CHORD 16                 // one bar of sixteenths per chord
#define LOOKAHEAD 0.12                     // seconds of notes booked ahead

// the progression: bass note + the chord's pitch classes (root first)
typedef struct { int bass; int pc[4]; int n; const char *name; } Chord;
static const Chord PROG[] = {
    { 36, { 0, 4, 7, 11 }, 4, "Cmaj7" },
    { 33, { 9, 0, 4, 7 },  4, "Am7"   },
    { 29, { 5, 9, 0, 4 },  4, "Fmaj7" },
    { 31, { 7, 11, 2, 5 }, 4, "G7"    },
    { 28, { 4, 7, 0, 11 }, 4, "C/E"   },
    { 26, { 2, 5, 9, 0 },  4, "Dm7"   },
    { 31, { 7, 11, 2, 5 }, 4, "G7"    },
    { 36, { 0, 4, 7 },     3, "C"     },
};
#define NPROG (int)(sizeof PROG / sizeof PROG[0])

static bool   playing = true;
static bool   strings_on = true;
static float  ring_len = 0.70f, bright = 0.45f;
static double next_t = 0.0;                // audio time of the next sixteenth to book
static int    step = 0;                    // sixteenths since start
static int    cur_chord = 0;               // the chord the strings are tuned to (what sounds now)
static double tune_at[4];                  // pending retunes: audio time + chord (a small queue)
static int    tune_ch[4], tune_n = 0;
static int    tuned[8], ntuned = 0;
static float  string_glow[8];

// ── SAMPLED PIANO (P): 13 Steinway notes, one every 6 semitones, from the bitKlavier Grand
// (Daniel Trueman, Princeton, CC BY 4.0), loaded at RUNTIME from assets/samples/steinway/ (credit +
// licence there; a web/app build would need them baked in). One instrument slot per zone; a note
// plays on the zone with the nearest root, so no note is shifted more than 3 semitones.
#define SL_ZONE 20
#define NZONE   13                     // C1 .. C7 every 6 semitones: no note moves more than 3
static const char *ZONE_FILE[NZONE] = { "C1", "Fs1", "C2", "Fs2", "C3", "Fs3", "C4", "Fs4", "C5", "Fs5", "C6", "Fs6", "C7" };
static bool zones_ok = false, sampled = false;
static float zbuf[8 * 44100];

static int load_wav16(const char *path, float *out, int max) {   // mono 16-bit PCM → -1..1
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char h[12]; int n = 0;
    if (fread(h, 1, 12, f) != 12) { fclose(f); return 0; }
    for (;;) {
        unsigned char ch[8];
        if (fread(ch, 1, 8, f) != 8) break;
        unsigned len = ch[4] | ch[5] << 8 | ch[6] << 16 | (unsigned)ch[7] << 24;
        if (memcmp(ch, "data", 4) != 0) { fseek(f, (long)(len + (len & 1)), SEEK_CUR); continue; }
        for (unsigned i = 0; i < len / 2 && n < max; i++) {
            unsigned char b[2];
            if (fread(b, 1, 2, f) != 2) break;
            out[n++] = (float)(short)(b[0] | b[1] << 8) / 32768.0f;
        }
        break;
    }
    fclose(f);
    return n;
}
static void load_zones(void) {
    zones_ok = true;
    for (int z = 0; z < NZONE; z++) {
        char path[96];
        snprintf(path, sizeof path, "../assets/samples/steinway/%s.wav", ZONE_FILE[z]);   // (cwd = build/)
        int n = load_wav16(path, zbuf, (int)(sizeof zbuf / sizeof zbuf[0]));
        if (n <= 0) { zones_ok = false; return; }
        sample_load(z, zbuf, n);
        int sl = SL_ZONE + z;
        instrument(sl, INSTR_SAMPLE, 1, 0, 7, 140);
        instrument_sample(sl, z, 24 + 6 * z);
        instrument_reverb(sl, 0.18f);
        instrument_resonator(sl, 0.6f);
    }
}
static int zone_slot(int midi) {
    int z = (midi - 24 + 3) / 6;                                   // the nearest root (24 + 6z)
    if (midi < 24) z = 0;
    if (z >= NZONE) z = NZONE - 1;
    return SL_ZONE + z;
}
static int pno(int midi, int synth_slot) { return sampled ? zone_slot(midi) : synth_slot; }

static void apply_resonator(void) { resonator(strings_on ? 0.6f : 0.0f, ring_len, bright); }

// the strings for one chord: the bass and its fifth, the root an octave up, the chord tones in
// octave 4, the root on top — the notes a held pedal would leave ringing
static int strings_for(const Chord *c, int *out) {
    int n = 0, r = c->pc[0];
    out[n++] = c->bass;
    out[n++] = c->bass + 7;
    out[n++] = 48 + r;
    for (int i = 0; i < c->n && n < 7; i++) out[n++] = 60 + c->pc[i];
    out[n++] = 72 + r;
    return n;
}

// the right hand's figure for step s within a chord: up two octaves through the chord tones, then down
static int figure_note(const Chord *c, int s) {
    int up[8], m = 0;
    for (int o = 0; o < 2; o++) for (int i = 0; i < c->n && m < 8; i++) up[m++] = 60 + 12 * o + c->pc[i] - (c->pc[i] > 9 ? 12 : 0);
    int k = s % 16, idx = k < 8 ? k : 15 - k;
    if (idx >= m) idx = m - 1;
    return up[idx];
}

void init(void) {
    bpm(BPM_);
    instrument(SL_PNO, INSTR_PIANO, 1, 0, 7, 120);
    instrument_harmonics(SL_PNO, 0.08f);   // the grand voicing (instrument-recipes.md)
    instrument_timbre(SL_PNO, 0.45f);
    instrument_morph(SL_PNO, 0.15f);       // a damped string: the ring is NOT the piano's own
    instrument_mode(SL_PNO, MODE_PIANO_MODEL, 1.0f);   // the Steinway-matched grand (MODE_PIANO_MODEL)
    instrument_reverb(SL_PNO, 0.18f);
    instrument_resonator(SL_PNO, 0.6f);
    instrument(SL_BAS, INSTR_PIANO, 1, 0, 7, 160);
    instrument_harmonics(SL_BAS, 0.08f);
    instrument_timbre(SL_BAS, 0.40f);
    instrument_morph(SL_BAS, 0.15f);
    instrument_mode(SL_BAS, MODE_PIANO_MODEL, 1.0f);   // the Steinway-matched grand (MODE_PIANO_MODEL)
    instrument_reverb(SL_BAS, 0.18f);
    instrument_resonator(SL_BAS, 0.6f);
    reverb(0.55f, 0.55f);
    apply_resonator();
    load_zones();
    next_t = audio_time() + 0.1;
}

void update(void) {
    if (keyp('R'))     { strings_on = !strings_on; apply_resonator(); }
    if (keyp(KEY_UP))    { ring_len   = fminf(1.0f, ring_len + 0.1f);   apply_resonator(); }
    if (keyp(KEY_DOWN))  { ring_len   = fmaxf(0.0f, ring_len - 0.1f);   apply_resonator(); }
    if (keyp(KEY_RIGHT)) { bright = fminf(1.0f, bright + 0.1f); apply_resonator(); }
    if (keyp(KEY_LEFT))  { bright = fmaxf(0.0f, bright - 0.1f); apply_resonator(); }
    if (keyp('P') && zones_ok) sampled = !sampled;
    if (keyp(KEY_SPACE)) { playing = !playing; if (playing) next_t = audio_time() + 0.1; }

    double tnow = audio_time(), sixteenth = 60.0 / BPM_ / 4.0;
    while (playing && next_t < tnow + LOOKAHEAD) {
        int ci = (step / STEPS_PER_CHORD) % NPROG, s = step % STEPS_PER_CHORD;
        const Chord *c = &PROG[ci];
        if (s == 0) {
            schedule_at(next_t, c->bass, pno(c->bass, SL_BAS), 6, 600);                 // the bass, a little longer
            if (tune_n < 4) { tune_at[tune_n] = next_t; tune_ch[tune_n] = ci; tune_n++; }
        }
        int vol = (s == 0 || s == 8) ? 6 : (s % 4 == 0 ? 5 : 4);         // a lean on each group's first note
        { int m = figure_note(c, s); schedule_at(next_t, m, pno(m, SL_PNO), vol, 180); }         // SHORT: the damper falls at once
        next_t += sixteenth;
        step++;
    }
    // retune when the chord SOUNDS, not when it is booked (a control lands at once)
    while (tune_n > 0 && tnow >= tune_at[0]) {
        cur_chord = tune_ch[0];
        ntuned = strings_for(&PROG[cur_chord], tuned);
        resonator_tune(tuned, ntuned);
        for (int k = 0; k < 8; k++) string_glow[k] = 1.0f;
        for (int i = 1; i < tune_n; i++) { tune_at[i - 1] = tune_at[i]; tune_ch[i - 1] = tune_ch[i]; }
        tune_n--;
    }
    float fall = 1.0f / (60.0f * (0.3f + 3.0f * ring_len));
    for (int k = 0; k < 8; k++) string_glow[k] = fmaxf(0.0f, string_glow[k] - fall);
#ifdef DE_TRACE
    watch("chord", "%d", cur_chord);
    watch("strings", "%d", strings_on ? 1 : 0);
    watch("sampled", "%d", sampled ? 1 : 0);
#endif
}

static const char *NOTE_NAMES[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

void draw(void) {
    cls(CLR_DARK_BLUE);
    int x0 = 40, x1 = SCREEN_W - 20, top = 40, gap = 15;
    print("openstrings", 8, 8, CLR_WHITE);
    print(PROG[cur_chord].name, SCREEN_W - 8 - 8 * (int)strlen(PROG[cur_chord].name), 8, CLR_YELLOW);
    for (int k = 0; k < 8; k++) {
        int y = top + k * gap;
        bool on = strings_on && k < ntuned;
        if (on) {
            int n = tuned[k];
            print(NOTE_NAMES[n % 12], 8, y - 3, CLR_LIGHT_GREY);
            // a vibrating string: amplitude from the glow, wavelength from the pitch
            float amp = 4.0f * string_glow[k] * string_glow[k];
            float wl = 6.0f + 0.25f * (float)(96 - n);
            int px = x0, py = y;
            for (int x = x0 + 2; x <= x1; x += 2) {
                float env = sinf(3.14159f * (float)(x - x0) / (float)(x1 - x0));
                int yy = y + (int)(amp * env * sinf((float)x / wl * 6.28f + now() * 30.0f));
                line(px, py, x, yy, string_glow[k] > 0.05f ? CLR_PEACH : CLR_LIGHT_GREY);
                px = x; py = yy;
            }
        } else {
            line(x0, y, x1, y, CLR_DARK_GREY);
        }
        circfill(x0, y, 1, CLR_LIGHT_GREY);
        circfill(x1, y, 1, CLR_LIGHT_GREY);
    }
    int y = SCREEN_H - 30;
    print(strings_on ? "R strings ON" : "R strings OFF", 8, y, strings_on ? CLR_GREEN : CLR_RED);
    char buf[48];
    snprintf(buf, sizeof buf, "ring %d  bright %d", (int)(ring_len * 10 + 0.5f), (int)(bright * 10 + 0.5f));
    print(buf, 8, y + 10, CLR_LIGHT_GREY);
    print(playing ? "" : "SPACE play", SCREEN_W - 88, y, CLR_WHITE);
    print(!zones_ok ? "P (no samples)" : sampled ? "P steinway" : "P synth", SCREEN_W - 120, y + 10,
          sampled ? CLR_YELLOW : CLR_LIGHT_GREY);
}
