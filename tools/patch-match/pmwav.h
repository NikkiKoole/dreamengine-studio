// pmwav.h — turning a file on disk into something comparable with an engine render.
//
// The target and the candidate must differ ONLY in how they were made, so every
// difference that is not about sound gets removed here first:
//   · stereo -> mono, because the loss reads one channel and a mono downmix of a
//     wide sample is what a mono render can hope to match;
//   · 48k (what these files are) -> 44100 (what the engine renders at);
//   · leading silence trimmed, so the attack lands at sample 0 in both. A target
//     whose note starts 80ms late scores every candidate on its silence;
//   · the PITCH detected, so the search plays the right note instead of spending
//     its dimensions rediscovering one.
#ifndef PMWAV_H
#define PMWAV_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static unsigned pmw_u32(const unsigned char *p) { return p[0] | (p[1]<<8) | (p[2]<<16) | ((unsigned)p[3]<<24); }
static unsigned pmw_u16(const unsigned char *p) { return p[0] | (p[1]<<8); }

// Read any ordinary WAV to mono float. Returns samples, or 0 (and *err set).
static int pmw_load(const char *path, float **out, int *sr, const char **err)
{
    *err = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) { *err = "cannot open"; return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    if (size < 44) { fclose(f); *err = "too short to be a WAV"; return 0; }
    unsigned char *buf = (unsigned char*)malloc(size);
    if (fread(buf, 1, size, f) != (size_t)size) { fclose(f); free(buf); *err = "short read"; return 0; }
    fclose(f);
    if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) { free(buf); *err = "not a RIFF/WAVE file"; return 0; }

    int fmt = 0, ch = 0, bits = 0; long dpos = 0; unsigned dlen = 0;
    long p = 12;
    while (p + 8 <= size) {
        unsigned id0 = *(unsigned*)(buf + p);
        unsigned len = pmw_u32(buf + p + 4);
        if (!memcmp(buf + p, "fmt ", 4)) {
            fmt = pmw_u16(buf + p + 8); ch = pmw_u16(buf + p + 10);
            *sr = (int)pmw_u32(buf + p + 12); bits = pmw_u16(buf + p + 22);
        } else if (!memcmp(buf + p, "data", 4)) {
            dpos = p + 8; dlen = len;
            if (dpos + (long)dlen > size) dlen = (unsigned)(size - dpos);   // truncated file: take what is there
        }
        (void)id0;
        p += 8 + len + (len & 1);
    }
    if (!ch || !dpos) { free(buf); *err = "no fmt/data chunk"; return 0; }
    if (fmt != 1 && fmt != 3 && fmt != 0xFFFE) { free(buf); *err = "compressed WAV (need PCM or float)"; return 0; }

    int bytes = bits / 8, n = (int)(dlen / (bytes * ch));
    if (n <= 0) { free(buf); *err = "no audio frames"; return 0; }
    float *mono = (float*)malloc(sizeof(float) * n);
    const unsigned char *d = buf + dpos;
    for (int i = 0; i < n; i++) {
        float acc = 0.0f;
        for (int c = 0; c < ch; c++) {
            const unsigned char *s = d + (size_t)(i * ch + c) * bytes;
            float v = 0.0f;
            if (fmt == 3 && bits == 32)      v = *(const float*)s;
            else if (bits == 16)             v = (float)(short)(s[0] | (s[1] << 8)) / 32768.0f;
            else if (bits == 24)             v = (float)(int)((s[0]<<8 | s[1]<<16 | (unsigned)s[2]<<24)) / 2147483648.0f;
            else if (bits == 32)             v = (float)(int)pmw_u32(s) / 2147483648.0f;
            else if (bits == 8)              v = ((float)s[0] - 128.0f) / 128.0f;
            acc += v;
        }
        mono[i] = acc / (float)ch;
    }
    free(buf);
    *out = mono;
    return n;
}

// Catmull-Rom resample. Only ever runs once, on the target, so it is not on any hot path.
static int pmw_resample(const float *in, int n, int from_sr, int to_sr, float **out)
{
    if (from_sr == to_sr) { *out = (float*)malloc(sizeof(float)*n); memcpy(*out, in, sizeof(float)*n); return n; }
    double ratio = (double)to_sr / (double)from_sr;
    int m = (int)(n * ratio);
    float *o = (float*)malloc(sizeof(float) * m);
    for (int i = 0; i < m; i++) {
        double sp = i / ratio; int k = (int)sp; float t = (float)(sp - k);
        float p0 = in[k > 0 ? k-1 : 0], p1 = in[k < n ? k : n-1];
        float p2 = in[k+1 < n ? k+1 : n-1], p3 = in[k+2 < n ? k+2 : n-1];
        o[i] = 0.5f*((2*p1) + (-p0+p2)*t + (2*p0-5*p1+4*p2-p3)*t*t + (-p0+3*p1-3*p2+p3)*t*t*t);
    }
    *out = o;
    return m;
}

// First sample worth calling the start of the note: the first crossing of 1% of
// peak, backed off 3ms so the attack transient is not clipped off.
static int pmw_onset(const float *x, int n, int sr)
{
    float pk = 0.0f;
    for (int i = 0; i < n; i++) { float a = fabsf(x[i]); if (a > pk) pk = a; }
    float thr = pk * 0.01f;
    int i = 0;
    while (i < n && fabsf(x[i]) < thr) i++;
    i -= sr / 333;
    return i < 0 ? 0 : i;
}

// f0 by normalized autocorrelation over a stable stretch just after the onset.
// Returns Hz, or 0 when nothing is clearly pitched (a bell-ish or noisy target).
static float pmw_pitch(const float *x, int n, int sr)
{
    int win = sr / 4; if (win > n) win = n;
    if (win < 512) return 0.0f;
    int lo = sr / 2000, hi = sr / 50; if (hi > win / 2) hi = win / 2;
    double e0 = 0.0;
    for (int i = 0; i < win; i++) e0 += (double)x[i] * x[i];
    if (e0 < 1e-12) return 0.0f;
    float best = 0.0f; int bestlag = 0;
    for (int lag = lo; lag <= hi; lag++) {
        double num = 0.0, den = 0.0;
        for (int i = 0; i + lag < win; i++) { num += (double)x[i]*x[i+lag]; den += (double)x[i+lag]*x[i+lag]; }
        float r = (den > 1e-12) ? (float)(num / sqrt(den * e0)) : 0.0f;
        if (r > best) { best = r; bestlag = lag; }
    }
    if (best < 0.35f || !bestlag) return 0.0f;      // honest 0 beats a confident wrong octave
    // parabolic refine on the lag axis
    float lag = (float)bestlag;
    if (bestlag > lo && bestlag < hi) {
        double a = 0, b = 0, c = 0;
        for (int d = -1; d <= 1; d++) {
            int L = bestlag + d; double num = 0, den = 0;
            for (int i = 0; i + L < win; i++) { num += (double)x[i]*x[i+L]; den += (double)x[i+L]*x[i+L]; }
            double r = (den > 1e-12) ? num / sqrt(den * e0) : 0;
            if (d == -1) a = r; else if (d == 0) b = r; else c = r;
        }
        double denom = a - 2*b + c;
        if (fabs(denom) > 1e-12) lag += (float)(0.5 * (a - c) / denom);
    }
    return (float)sr / lag;
}

static int pmw_midi_of(float hz) { return (int)lrintf(69.0f + 12.0f * log2f(hz / 440.0f)); }
static float pmw_cents_off(float hz, int midi)
{ return 1200.0f * log2f(hz / (440.0f * powf(2.0f, (midi - 69) / 12.0f))); }

#endif
