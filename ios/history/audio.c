#include "audio.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int            sr    = 44100;
static double         phase = 0.0;   // accumulated osc phase (continuous → click-free)
static long           t     = 0;     // sample counter
static volatile float level = 0.0f;  // smoothed RMS, read by the canvas VU

void de_audio_init(int sample_rate) { sr = sample_rate; phase = 0.0; t = 0; level = 0.0f; }

float de_audio_level(void) { return level; }

// C-major pentatonic arpeggio up and back: C E G B C B G E …
static double note_hz(long step) {
    static const double tbl[] = { 261.63, 329.63, 392.00, 493.88, 523.25, 493.88, 392.00, 329.63 };
    return tbl[(unsigned long)step % 8];
}

void de_audio_render(float* out, int frames) {
    const int    step_len = sr * 16 / 100;            // 0.16s per note
    double       sumsq    = 0.0;
    for (int i = 0; i < frames; i++) {
        long   step = t / step_len;
        double f    = note_hz(step);
        double pos  = (double)(t % step_len) / step_len;
        double env  = pos < 0.06 ? pos / 0.06         // quick attack
                                 : 1.0 - (pos - 0.06) / 0.94;  // long decay → 0 at note end
        if (env < 0.0) env = 0.0;

        phase += 2.0 * M_PI * f / sr;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;

        float s = (float)(0.18 * env * sin(phase));
        out[i]  = s;
        sumsq  += (double)s * s;
        t++;
    }
    float rms = (float)sqrt(sumsq / (frames > 0 ? frames : 1));
    level += (rms - level) * 0.3f;                    // light smoothing for the meter
}
