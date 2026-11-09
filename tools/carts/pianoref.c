/* de:meta
{
  "slug": "pianoref",
  "title": "piano ref probe",
  "status": "active",
  "created": "2026-11-09",
  "kind": ["probe"],
  "teaches": ["waveguide-synth"],
  "lineage": "INSTR_PIANO's half of an A/B against a real Steinway D: the 13 notes in assets/samples/steinway/ (bitKlavier Grand, Daniel Trueman, Princeton, CC BY 4.0). Same shape as acidref (the 303 vs Open303): play exactly what the reference holds, on a fixed grid, so both drop into the same oracles. openstrings' fifth todo.",
  "description": "Not a toy: a measurement probe. Plays C1, F#1, C2 ... F#6, C7 (MIDI 24 + 6k, the Steinway sample grid) on INSTR_PIANO's grand voicing, each held 3.3 s with a 0.2 s gap, one note every 3.5 s. Compare with node tools/oneshot-compare.js --render pianoref --spec tools/oneshot-specs/pianoref.json. DE_DEFINES: PR_VOL (note_on vol 0-7, default 5 = the samples' layer 12 of 16), PR_HARM / PR_TIMB / PR_MORPH (the macros), PR_STIFF / PR_DECAY / PR_KNOCK (instrument_mode 0..1, 0.5 = the engine default). LISTEN: press S to hear each note twice, ours then the real Steinway, looping (credit: bitKlavier Grand Sample Library, Daniel Trueman, Princeton University, CC BY 4.0)."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <string.h>

#define NZ   13                 // C1 .. C7, every 6 semitones — the Steinway sample grid
#define HOLD 198                // 3.3 s held (the samples are 3.2 s long)
#define PER  210                // + 0.2 s gap
#ifndef PR_VOL
#define PR_VOL 5
#endif
#ifndef PR_HARM
#define PR_HARM 0.08f           // the grand (instrument-recipes.md)
#endif
#ifndef PR_TIMB
#define PR_TIMB 0.5f
#endif
#ifndef PR_MORPH
#define PR_MORPH 1.0f           // a HELD KEY lifts that string's damper: undamped, like the recording
#endif
#ifndef PR_STIFF
#define PR_STIFF 0.5f
#endif
#ifndef PR_DECAY
#define PR_DECAY 0.5f
#endif
#ifndef PR_KNOCK
#define PR_KNOCK 0.5f
#endif

static int f = -1, held = -1;

// ── LISTEN mode (S): the same grid, each note played TWICE, ours then the real Steinway, looping. Off by
// default, so a headless render (the measurement) is unchanged. Samples load at runtime from
// assets/samples/steinway/ (cwd = build/), same as openstrings. Credit: bitKlavier Grand Sample Library,
// Daniel Trueman, Princeton University (CC BY 4.0).
static const char *ZF[NZ] = { "C1", "Fs1", "C2", "Fs2", "C3", "Fs3", "C4", "Fs4", "C5", "Fs5", "C6", "Fs6", "C7" };
static float zbuf[4 * 44100];
static bool listen = false, samples_ok = false;
static int lf = 0;                  // listen-mode frame counter

static int load_wav16(const char *path, float *out, int max) {   // mono 16-bit PCM → -1..1
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    unsigned char h[12]; int n = 0;
    if (fread(h, 1, 12, fp) != 12) { fclose(fp); return 0; }
    for (;;) {
        unsigned char ch[8];
        if (fread(ch, 1, 8, fp) != 8) break;
        unsigned len = ch[4] | ch[5] << 8 | ch[6] << 16 | (unsigned)ch[7] << 24;
        if (memcmp(ch, "data", 4) != 0) { fseek(fp, (long)(len + (len & 1)), SEEK_CUR); continue; }
        for (unsigned i = 0; i < len / 2 && n < max; i++) {
            unsigned char b[2];
            if (fread(b, 1, 2, fp) != 2) break;
            out[n++] = (float)(short)(b[0] | b[1] << 8) / 32768.0f;
        }
        break;
    }
    fclose(fp);
    return n;
}
static void load_samples(void) {
    samples_ok = true;
    for (int z = 0; z < NZ; z++) {
        char path[96];
        snprintf(path, sizeof path, "../assets/samples/steinway/%s.wav", ZF[z]);
        int n = load_wav16(path, zbuf, (int)(sizeof zbuf / sizeof zbuf[0]));
        if (n <= 0) { samples_ok = false; return; }
        sample_load(z, zbuf, n);
        instrument(20 + z, INSTR_SAMPLE, 1, 0, 7, 140);
        instrument_sample(20 + z, z, 24 + 6 * z);
    }
}

void init(void) {
    instrument(5, INSTR_PIANO, 1, 0, 7, 120);   // sustain 7 + a held gate: the VOICE's own decay, not the envelope's
    instrument_harmonics(5, PR_HARM);
    instrument_timbre(5, PR_TIMB);
    instrument_morph(5, PR_MORPH);
    instrument_mode(5, MODE_PIANO_STIFF, PR_STIFF);
    instrument_mode(5, MODE_PIANO_DECAY, PR_DECAY);
    instrument_mode(5, MODE_PIANO_KNOCK, PR_KNOCK);
}

void update(void) {
    if (keyp('S')) {
        listen = !listen; lf = 0;
        if (held >= 0) { note_off(held); held = -1; }
        if (listen && !samples_ok) load_samples();
    }
    if (listen) {
        int k = lf / PER, local = lf % PER;   // k even = ours, odd = Steinway
        int z = (k / 2) % NZ;
        if (local == 0) held = note_on(24 + 6 * z, (k & 1) && samples_ok ? 20 + z : 5, PR_VOL);
        else if (local == HOLD && held >= 0) { note_off(held); held = -1; }
        lf++;
        return;
    }
    f++;
    int z = f / PER, local = f % PER;
    if (z < NZ) {
        if (local == 0) held = note_on(24 + 6 * z, 5, PR_VOL);
        else if (local == HOLD && held >= 0) { note_off(held); held = -1; }
    }
#ifdef DE_TRACE
    watch("zone", "%d", z < NZ ? z : -1);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("piano ref probe", 8, 8, CLR_WHITE);
    char b[64];
    if (listen) {
        int k = lf / PER, z = (k / 2) % NZ;
        snprintf(b, sizeof b, "LISTEN  %s  midi %d", ZF[z], 24 + 6 * z);
        print(b, 8, 24, CLR_LIGHT_GREY);
        if (!samples_ok) print("samples missing: ours only", 8, 40, CLR_RED);
        else print((k & 1) ? "STEINWAY (real)" : "OURS (INSTR_PIANO)", 8, 40, (k & 1) ? CLR_YELLOW : CLR_GREEN);
    } else {
        int z = f / PER;
        snprintf(b, sizeof b, "note %d of %d  midi %d", z + 1, NZ, 24 + 6 * z);
        print(z < NZ ? b : "done", 8, 24, CLR_LIGHT_GREY);
    }
    print("S: listen A/B (ours, then Steinway)", 8, SCREEN_H - 12, CLR_DARK_GREY);
}
