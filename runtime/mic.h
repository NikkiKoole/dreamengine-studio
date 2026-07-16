#ifndef DE_MIC_H
#define DE_MIC_H
// ============================================================================
// mic.h — microphone INPUT analysis for dreamengine (Tier-1, the audio-input
// frontier: docs/design/mic-and-sampling.md → §"the live-throughput dimension").
//
// DEVICE-FREE BY DESIGN. The engine core NEVER opens a capture device. Each
// platform HOST owns its mic and PUSHES captured mono frames in through the
// platform.h §4 seam (de_audio_input); this header only ANALYZES them. That is
// the exact mirror of the audio OUTPUT seam, inverted — output is PULLED from the
// engine (de_audio_render), input is PUSHED into it — and it is why the mic needs
// no cross-platform capture library inside the engine (notably: no second copy of
// the miniaudio that raylib already bundles, so there is no symbol clash and no
// version coupling). The per-platform capture + permission plumbing lives in the
// hosts: CoreAudio/AudioQueue (desktop), AVAudioSession (iOS), getUserMedia (web).
//
// The cart reads two surfaces (declared in studio.h, implemented in studio.c):
//   mic_level() — RMS loudness 0..1, the beatbox/envelope-follower input.
//   mic_pitch() — a CRUDE zero-crossing pitch estimate in Hz. Honest caveat: it
//                 reads octave-low and jittery on a real voice (harmonics + noise
//                 add crossings). Fine as a controller axis; a real hum->pitch
//                 feature wants autocorrelation/FFT. Kept crude + labelled for v1.
//
// mic_start()/mic_stop() raise a "wanted" flag the host polls (de_mic_wanted) so it
// opens/closes the device lazily and only prompts for permission when a cart asks.
// ============================================================================

#include <math.h>

#define MIC_WIN 2048   // analysis window (samples) ≈ 46ms @44.1k — long enough to resolve a low hum's period

// Published readings: the pushing host thread WRITES, the main (game) thread READS.
// Single-word float/int loads+stores are atomic enough on arm64/x86 for a meter
// (same call the mic-spike made). A finer surface could go lock-free later.
static volatile float mic_g_rms    = 0.0f;   // 0..1 loudness
static volatile float mic_g_pitch  = 0.0f;   // Hz (0 = no clear pitch)
static volatile int   mic_g_wanted = 0;      // a cart called mic_start(); host opens its device
static volatile int   mic_g_active = 0;      // host reports capture is live + permission granted

// Accumulation window — touched ONLY by the pushing host thread.
static float mic_acc[MIC_WIN];
static int   mic_acc_n = 0;

// Host → engine: feed captured MONO frames at `sr` Hz from the capture callback
// (the host's audio thread). Accumulates a MIC_WIN window, then republishes the
// RMS + a zero-crossing pitch. Carrying `sr` keeps the pitch Hz correct whatever
// rate the device runs at (desktop mics are usually 48k, the engine mixes at 44.1k).
static inline void mic_input_push(const float *s, int n, int sr) {
    if (!s || n <= 0 || sr <= 0) return;
    for (int i = 0; i < n; i++) {
        mic_acc[mic_acc_n++] = s[i];
        if (mic_acc_n >= MIC_WIN) {
            double sum = 0.0;
            for (int j = 0; j < MIC_WIN; j++) sum += (double)mic_acc[j] * mic_acc[j];
            float rms = (float)sqrt(sum / MIC_WIN);
            mic_g_rms = rms;
            if (rms > 0.01f) {                        // only trust pitch when the block is loud enough
                int zc = 0;
                for (int j = 1; j < MIC_WIN; j++)
                    if ((mic_acc[j-1] < 0.0f) != (mic_acc[j] < 0.0f)) zc++;   // sign change
                float secs = (float)MIC_WIN / (float)sr;
                mic_g_pitch = (zc * 0.5f) / secs;     // 2 crossings per cycle
            } else {
                mic_g_pitch = 0.0f;
            }
            mic_acc_n = 0;
        }
    }
}

#endif // DE_MIC_H
