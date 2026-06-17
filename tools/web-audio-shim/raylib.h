// Minimal raylib.h SHIM for the offline audio-parity harness (tools/web-audio-host.c).
// sound.h does `#include "raylib.h"` only for the AudioStream type + 5 stream functions, and
// in synth mode (sound_synth_mode = true) those functions are never CALLED — they just need to
// exist to compile/link. This shim shadows the real raylib (via -I tools/web-audio-shim) so the
// harness pulls in ZERO of raylib's graphics surface: the comparison is sound.h's DSP and nothing
// else. NOT a real raylib — only what sound.h touches. See docs/design/web-audio-parity.md.
#ifndef RAYLIB_H_SHIM
#define RAYLIB_H_SHIM

typedef struct AudioStream {
    void *buffer; void *processor;
    unsigned int sampleRate, sampleSize, channels;
} AudioStream;

typedef struct Wave {
    unsigned int frameCount, sampleRate, sampleSize, channels;
    void *data;
} Wave;

typedef void (*AudioCallback)(void *bufferData, unsigned int frames);

static inline AudioStream LoadAudioStream(unsigned int sr, unsigned int sz, unsigned int ch) {
    (void)sr; (void)sz; (void)ch; AudioStream s = {0}; return s;
}
static inline void UnloadAudioStream(AudioStream s)               { (void)s; }
static inline void PlayAudioStream(AudioStream s)                 { (void)s; }
static inline void SetAudioStreamCallback(AudioStream s, AudioCallback cb) { (void)s; (void)cb; }
static inline void SetAudioStreamBufferSizeDefault(int n)         { (void)n; }

#endif
