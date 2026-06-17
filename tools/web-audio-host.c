// web-audio-host.c — the offline AUDIO-ONLY host for the web-parity gate (Axis 1).
// It includes the real engine (sound.h) but NO graphics/raylib (a shim satisfies the one
// type sound.h needs), runs a fixed deterministic sequence — every modeled engine + the
// master effects — in synth mode (sound_synth_mode), pumps sound_callback() 735 samples/frame,
// and writes a stereo 16-bit WAV. Compiled BOTH ways by tools/web-audio-check.js:
//   native:  clang  -O2  → the reference
//   wasm:    emcc   -O2  → run under Node (NODERAWFS)
// Byte/near-byte diffing the two WAVs isolates ONE variable — does emscripten's compiled DSP
// math match native clang? — i.e. float/codegen/libm determinism. Same source, same host, same
// sequence; only the compiler differs. See docs/design/web-audio-parity.md.

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "studio.h"   // constants (INSTR_*/FILTER_*/…) + the audio API prototypes (self-contained)

// sound.h logs warnings via printh() (normally studio.c's). Forward to stderr so a queue
// overflow etc. is visible but stays out of the WAV path.
void printh(const char *fmt, ...) { va_list a; va_start(a, fmt); vfprintf(stderr, fmt, a); va_end(a); fputc('\n', stderr); }

// sound.h's humanization (tone/chance) calls rnd() — normally studio.c's GetRandomValue. We give
// it a fixed-seed integer LCG: deterministic and bit-identical under both compilers (pure integer
// math, no libm), so it can't itself introduce native-vs-wasm divergence.
static unsigned int de_rng = 0x2545F491u;
int rnd(int n) { if (n <= 0) return 0; de_rng = de_rng * 1664525u + 1013904223u; return (int)((de_rng >> 9) % (unsigned)n); }

#include "sound.h"    // the engine + the audio API DEFINITIONS (raylib.h via the shim, synth mode)

// ── stereo 16-bit WAV writer, byte-for-byte the same format as studio.c's --wav ──
static FILE *wav;
static int   wav_samples;   // int16 count (2/frame)
static void wav_open(const char *path) {
    wav = fopen(path, "wb");
    if (!wav) { fprintf(stderr, "web-audio-host: cannot open %s\n", path); exit(2); }
    int z = 0, sr = SOUND_SAMPLE_RATE, byterate = sr * 4, fmtlen = 16;
    short fmt = 1, ch = 2, block = 4, bits = 16;
    fwrite("RIFF", 1, 4, wav); fwrite(&z, 4, 1, wav); fwrite("WAVE", 1, 4, wav);
    fwrite("fmt ", 1, 4, wav); fwrite(&fmtlen, 4, 1, wav);
    fwrite(&fmt, 2, 1, wav); fwrite(&ch, 2, 1, wav);
    fwrite(&sr, 4, 1, wav); fwrite(&byterate, 4, 1, wav);
    fwrite(&block, 2, 1, wav); fwrite(&bits, 2, 1, wav);
    fwrite("data", 1, 4, wav); fwrite(&z, 4, 1, wav);
}
static void wav_pump(void) {
    float scratch[735 * 2];
    short pcm[735 * 2];
    sound_callback(scratch, 735);
    for (int i = 0; i < 735 * 2; i++) pcm[i] = (short)(scratch[i] * 32767.0f);   // truncation — identical to studio.c
    fwrite(pcm, 2, 735 * 2, wav);
    wav_samples += 735 * 2;
}
static void wav_close(void) {
    int data_bytes = wav_samples * 2, riff = 36 + data_bytes;
    fseek(wav, 4, SEEK_SET);  fwrite(&riff, 4, 1, wav);
    fseek(wav, 40, SEEK_SET); fwrite(&data_bytes, 4, 1, wav);
    fclose(wav);
}

// ── the deterministic sequence: every modeled engine, through the master effects ──
// One instrument per engine on slots 5+, a rotating one-shot note schedule, plus a sustained
// pair to keep long feedback tails ringing (where libm/FMA ULP differences compound the most).
static const int ENGINES[] = {
    INSTR_PLUCK, INSTR_MALLET, INSTR_FM,    INSTR_ORGAN, INSTR_EPIANO, INSTR_PD,
    INSTR_REED,  INSTR_PIPE,   INSTR_GUITAR, INSTR_PIANO, INSTR_BOWED, INSTR_BRASS,
};
#define NENG ((int)(sizeof(ENGINES) / sizeof(ENGINES[0])))
static const int MIDIS[] = { 45, 52, 57, 60, 64, 69, 72, 67 };
#define NMIDI ((int)(sizeof(MIDIS) / sizeof(MIDIS[0])))
#define SLOT0 5

int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "out.wav";
    int frames      = argc > 2 ? atoi(argv[2]) : 540;   // ~9s
    // argv[3]: engine id to render ALONE, no effects (isolates one engine's determinism).
    //          omit / -1 = the full mixed sequence (all engines + master effects).
    int solo_engine = argc > 3 ? atoi(argv[3]) : -1;

    sound_synth_mode = true;     // no device stream; we pump the callback ourselves
    sound_init();

    if (solo_engine >= 0) {
        // ── single-engine, dry: a sustained note + a few one-shots, no effects ──
        instrument(SLOT0, solo_engine, 4, 60, 5, 400);
        wav_open(out);
        int held = note_on(57, SLOT0, 6);   // sustained A3 for the whole render
        for (int f = 0; f < frames; f++) {
            if (f % 60 == 0 && f > 0) note(MIDIS[(f / 60) % NMIDI], SLOT0, 6);
            (void)held;
            wav_pump();
        }
        wav_close();
        return 0;
    }

    for (int e = 0; e < NENG; e++) instrument(SLOT0 + e, ENGINES[e], 4, 60, 5, 300);

    // master effects: sends (reverb/echo — feedback) + inserts (chorus/eq/tape — sinf/expf LFOs),
    // configured ONCE (set-and-hold). The sends need a per-instrument feed.
    reverb(0.72f, 0.30f);
    echo(300, 0.55f, 0.5f);
    chorus(1.5f, 0.4f, 0.4f);
    eq(3.0f, 0.0f, 4.0f);
    tape(0.3f, 0.2f, 0.4f);
    for (int e = 0; e < NENG; e++) { instrument_reverb(SLOT0 + e, 0.30f); instrument_echo(SLOT0 + e, 0.18f); }

    int held = -1, held_eng = 0;
    wav_open(out);
    for (int f = 0; f < frames; f++) {
        if (f % 7 == 0) {                                  // a rotating one-shot
            int e = (f / 7) % NENG, m = MIDIS[(f / 7) % NMIDI];
            note(m, SLOT0 + e, 6);
        }
        if (f % 90 == 0) {                                 // a sustained note (long tail) on a rotating engine
            if (held >= 0) note_off(held);
            held_eng = (held_eng + 1) % NENG;
            held = note_on(40 + (held_eng % 7), SLOT0 + held_eng, 6);
        }
        wav_pump();
    }
    wav_close();
    return 0;
}
