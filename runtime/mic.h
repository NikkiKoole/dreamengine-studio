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
//   mic_pitch() — pitch in Hz via a YIN detector (autocorrelation family). Tracks a
//                 hummed/sung voice cleanly and is OCTAVE-SAFE (no octave jumps), and
//                 reports 0 rather than guessing when the input is unvoiced. ~21
//                 readings/sec — a melody/controller axis, not a sample-accurate tuner.
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

// ── mic RECORD buffer (capture-then-freeze) ──────────────────────────────────
// While armed (mic_rec_cap > 0) the push copies raw mic frames here so a cart can grab a few
// seconds of input (a beatboxed loop, a sung phrase), then sample_load() it. The cart reads it
// out only AFTER recording finishes (mic_recording() == 0), so the audio thread is the sole writer.
#define MIC_REC_MAX (44100 * 8)      // up to 8 seconds of mono capture (~1.4 MB .bss)
static float          mic_rec[MIC_REC_MAX];
static volatile int   mic_rec_cap = 0;   // target sample count (0 = idle / not armed)
static volatile int   mic_rec_n   = 0;   // samples captured so far
static volatile int   mic_rec_sr  = 44100;

// ── pitch via YIN (de Cheveigné & Kawahara 2002) ─────────────────────────────
// Autocorrelation-family, but with the cumulative-mean-normalized difference that makes YIN
// ROBUST for voice — it rejects the octave errors a raw zero-crossing (or plain autocorrelation)
// falls for on a harmonic-rich signal like a hum. Runs once per filled window on the host thread.
#define MIC_TAU_MIN 32     // shortest lag searched → ~1378 Hz max @44.1k
#define MIC_TAU_MAX 700    // longest lag searched  → ~63 Hz min @44.1k (covers the vocal range)
#define MIC_YIN_THRESH 0.15f   // aperiodicity cutoff: no lag below this ⇒ "no clear pitch"
static float mic_yin_d[MIC_TAU_MAX + 2];

static float mic_pitch_yin(const float *x, int n, int sr) {
    int tau_max = MIC_TAU_MAX; if (tau_max > n / 2) tau_max = n / 2;
    int W = n - tau_max;                          // fixed correlation length → comparable across lags
    // cumulative-mean-normalized difference function d'(tau)
    mic_yin_d[0] = 1.0f;
    float running = 0.0f;
    for (int tau = 1; tau <= tau_max; tau++) {
        float sum = 0.0f;
        for (int j = 0; j < W; j++) { float diff = x[j] - x[j + tau]; sum += diff * diff; }
        running += sum;
        mic_yin_d[tau] = (running > 0.0f) ? (sum * tau / running) : 1.0f;
    }
    // first lag that dips below the threshold, walked down to its local minimum (the true period,
    // not a later harmonic) — this step is what kills the octave jumps
    int tau = -1;
    for (int t = MIC_TAU_MIN; t < tau_max; t++) {
        if (mic_yin_d[t] < MIC_YIN_THRESH) {
            while (t + 1 <= tau_max && mic_yin_d[t + 1] < mic_yin_d[t]) t++;
            tau = t; break;
        }
    }
    if (tau < 0) return 0.0f;                     // nothing periodic enough → no clear pitch
    // parabolic interpolation around the minimum for sub-sample (sub-cent) accuracy
    float est = (float)tau;
    if (tau > MIC_TAU_MIN && tau < tau_max) {
        float a = mic_yin_d[tau - 1], b = mic_yin_d[tau], c = mic_yin_d[tau + 1];
        float denom = a + c - 2.0f * b;
        if (denom != 0.0f) est = tau + (a - c) / (2.0f * denom);
    }
    return (est > 0.0f) ? (float)sr / est : 0.0f;
}

// Host → engine: feed captured MONO frames at `sr` Hz from the capture callback
// (the host's audio thread). Accumulates a MIC_WIN window, then republishes the RMS + a YIN
// pitch. Carrying `sr` keeps the pitch Hz correct whatever rate the device runs at (desktop
// mics are usually 48k, the engine mixes at 44.1k).
static inline void mic_input_push(const float *s, int n, int sr) {
    if (!s || n <= 0 || sr <= 0) return;
    mic_rec_sr = sr;
    if (mic_rec_cap > 0 && mic_rec_n < mic_rec_cap) {   // recording: append raw frames
        int room = mic_rec_cap - mic_rec_n;
        int take = n < room ? n : room;
        for (int i = 0; i < take; i++) mic_rec[mic_rec_n + i] = s[i];
        mic_rec_n += take;
    }
    if (extin_on) for (int i = 0; i < n; i++) sound_extin_write(s[i]);   // Phase 2: feed the live vocoder/pedal ring (sound.h)
    for (int i = 0; i < n; i++) {
        mic_acc[mic_acc_n++] = s[i];
        if (mic_acc_n >= MIC_WIN) {
            double sum = 0.0;
            for (int j = 0; j < MIC_WIN; j++) sum += (double)mic_acc[j] * mic_acc[j];
            float rms = (float)sqrt(sum / MIC_WIN);
            mic_g_rms = rms;
            mic_g_pitch = (rms > 0.01f) ? mic_pitch_yin(mic_acc, MIC_WIN, sr) : 0.0f;
            mic_acc_n = 0;
        }
    }
}

#endif // DE_MIC_H
