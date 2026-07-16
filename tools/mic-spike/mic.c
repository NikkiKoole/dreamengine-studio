// mic.c — Tier-1 microphone SPIKE (docs/design/mic-and-sampling.md §"the live-throughput
// dimension"): prove the engine CAN hear on this Mac desktop before touching the hot
// runtime files. Uses miniaudio (the same lib raylib links internally) so this is exactly
// the cross-platform path the doc names — mac now, WASM getUserMedia / iOS later.
//
// It captures the default mic and, ~5×/second, prints the two surfaces a cart would read:
//   mic_level()  — RMS loudness (a VU meter + dBFS), the beatbox/envelope-follower input
//   mic_pitch()  — a crude zero-crossing pitch estimate in Hz, the hum→theremin input
// No storage, no playback — the Tier-1 "mic as CONTROLLER" shape. Runs ~6s then exits.
//
// build+run:  zsh tools/mic-spike/run.sh
// If the meter stays flat at silence, it's almost certainly macOS mic PERMISSION (TCC) —
// see run.sh's note; grant the terminal app mic access (or run it yourself via `! `).

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#define SR        48000
#define CHANNELS  1

// shared state written by the audio thread, read by main (single float writes are atomic
// enough for a spike meter; a real engine surface would use a proper ring/lockfree read)
static volatile float g_rms   = 0.0f;   // 0..1
static volatile float g_pitch = 0.0f;   // Hz, 0 = no clear pitch

static void on_capture(ma_device* dev, void* out, const void* in, ma_uint32 n) {
    (void)dev; (void)out;
    const float* s = (const float*)in;
    if (!s || n == 0) return;

    // RMS loudness
    double sum = 0.0;
    for (ma_uint32 i = 0; i < n; i++) sum += (double)s[i] * s[i];
    g_rms = (float)sqrt(sum / n);

    // crude pitch: count zero-crossings, only trust it when the block is loud enough
    if (g_rms > 0.01f) {
        int zc = 0;
        for (ma_uint32 i = 1; i < n; i++)
            if ((s[i-1] < 0.0f && s[i] >= 0.0f) || (s[i-1] >= 0.0f && s[i] < 0.0f)) zc++;
        float secs = (float)n / (float)SR;
        g_pitch = (zc * 0.5f) / secs;   // 2 crossings per cycle
    } else {
        g_pitch = 0.0f;
    }
}

// nearest note name for a frequency (so a hummed pitch reads legibly)
static const char* NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static void note_of(float hz, char* buf, int cap) {
    if (hz < 20.0f || hz > 5000.0f) { snprintf(buf, cap, "  -"); return; }
    float midi = 69.0f + 12.0f * log2f(hz / 440.0f);
    int m = (int)lroundf(midi);
    snprintf(buf, cap, "%2s%d", NOTES[((m % 12) + 12) % 12], m / 12 - 1);
}

int main(void) {
    // enumerate capture devices and prefer a PHYSICAL mic — the system default here is often a
    // virtual loopback ("BlackHole"), which delivers silence. Skip virtual drivers.
    ma_context ctx;
    if (ma_context_init(NULL, 0, NULL, &ctx) != MA_SUCCESS) {
        fprintf(stderr, "mic-spike: ma_context_init failed\n"); return 1;
    }
    ma_device_info* caps; ma_uint32 nCaps;
    ma_context_get_devices(&ctx, NULL, NULL, &caps, &nCaps);
    printf("mic-spike: %u capture device(s):\n", nCaps);
    int pick = -1;
    for (ma_uint32 i = 0; i < nCaps; i++) {
        int virtualish = strstr(caps[i].name, "BlackHole") || strstr(caps[i].name, "Aggregate")
                      || strstr(caps[i].name, "Multi-Output") || strstr(caps[i].name, "Soundflower");
        printf("   [%u] %-30s%s%s\n", i, caps[i].name,
               caps[i].isDefault ? "  (default)" : "", virtualish ? "  (virtual — skip)" : "");
        if (pick < 0 && !virtualish) pick = (int)i;   // first physical device
    }
    if (pick < 0) pick = 0;   // fall back to whatever exists
    printf("   → using [%d] %s\n", pick, caps[pick].name);

    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID = &caps[pick].id;
    cfg.capture.format   = ma_format_f32;
    cfg.capture.channels = CHANNELS;
    cfg.sampleRate       = SR;
    cfg.dataCallback     = on_capture;

    ma_device dev;
    if (ma_device_init(&ctx, &cfg, &dev) != MA_SUCCESS) {
        fprintf(stderr, "mic-spike: FAILED to init capture device (no mic? backend issue?)\n");
        return 1;
    }
    printf("mic-spike: capturing from \"%s\" @ %dHz — hum or clap; ~6s\n\n",
           dev.capture.name, SR);

    if (ma_device_start(&dev) != MA_SUCCESS) {
        fprintf(stderr, "mic-spike: FAILED to start device\n");
        ma_device_uninit(&dev);
        return 1;
    }

    float peak = 0.0f;
    for (int t = 0; t < 30; t++) {          // 30 × 200ms ≈ 6s
        usleep(200 * 1000);
        float rms = g_rms, pitch = g_pitch;
        if (rms > peak) peak = rms;

        // VU bar from dBFS (-60..0)
        float db = rms > 1e-6f ? 20.0f * log10f(rms) : -99.0f;
        int bars = (int)((db + 60.0f) / 60.0f * 40.0f);
        if (bars < 0) bars = 0; if (bars > 40) bars = 40;
        char meter[41]; memset(meter, ' ', 40); meter[40] = 0;
        for (int i = 0; i < bars; i++) meter[i] = '#';

        char note[8]; note_of(pitch, note, sizeof note);
        printf("\r[%s] %5.1f dB  pitch %6.1f Hz (%s)   ", meter, db, pitch, note);
        fflush(stdout);
    }

    ma_device_stop(&dev);
    ma_device_uninit(&dev);
    ma_context_uninit(&ctx);

    printf("\n\nmic-spike: done. peak RMS = %.4f  (%.1f dBFS)\n",
           peak, peak > 1e-6f ? 20.0f * log10f(peak) : -99.0f);
    int onlyVirtual = strstr(caps[pick].name, "BlackHole") || strstr(caps[pick].name, "Aggregate");
    if (peak < 1e-4f) {
        printf("  >> ~silence.");
        if (onlyVirtual)
            printf(" And only a VIRTUAL device enumerated — macOS is HIDING the physical\n"
                   "     mics because this process lacks Microphone permission (TCC). The mic isn't\n"
                   "     missing; the grant is. Run this from YOUR terminal so the dialog attaches:\n"
                   "       ! zsh tools/mic-spike/run.sh\n"
                   "     click Allow, then hum — the meter moves and pitch locks to a note.\n");
        else
            printf(" A physical device opened but delivered no signal — check input gain /\n"
                   "     the mic isn't muted, then re-run.\n");
    } else {
        printf("  >> the engine CAN hear (peak %.1f dBFS). Tier-1 mic_level()/mic_pitch() is\n"
               "     PROVEN feasible on this desktop.\n", 20.0f * log10f(peak));
    }
    return 0;
}
