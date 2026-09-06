// patchmatch.h — the ANALYSIS + SEARCH core. Knows nothing about dreamengine:
// give it a target buffer and a render callback and it answers "which parameter
// vector sounds closest". Kept engine-agnostic on purpose so the loss function
// can be self-tested against synthetic signals, with no engine in the picture.
//
// THE LOSS. Comparing waveforms sample by sample is useless — two identical-
// sounding saws a few samples out of phase score terrible. What works is a
// MULTI-RESOLUTION LOG-MEL L1 distance:
//   · four FFT sizes (256..2048), because a short window sees the attack and a
//     long one sees pitch and detune beating, and no single size sees both;
//   · mel-binned, so the top octave's hiss does not outvote the harmonics;
//   · log-scaled, so a quiet partial still counts;
//   · both signals RMS-normalized first, so absolute level is not part of it.
//
// TWO SPEED CHOICES, both because this is 90%+ of the run time (measured):
//   · twiddles precomputed per scale rather than recomputed per butterfly;
//   · each mel band stores only its NON-ZERO bin range. A triangular band
//     touches ~1% of the spectrum, so the naive "loop every bin per band" form
//     spends 99% of its time multiplying by zero.
#ifndef PATCHMATCH_H
#define PATCHMATCH_H

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PM_NSCALES 4
#define PM_BAD     1e9f     // a render that exploded, or is silent

typedef struct {
    int n, hop, nbands, nbins;
    float *win, *tw_re, *tw_im;
    int   *bstart, *boff;        // per band: first bin, offset into bw
    int   *blen;
    float *bw;                   // packed triangular weights
    float  weight;
} PmScale;

static void pm_fft(const PmScale *s, float *re, float *im)
{
    int n = s->n;
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t; t=re[i];re[i]=re[j];re[j]=t; t=im[i];im[i]=im[j];im[j]=t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1, step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0, k = 0; j < half; j++, k += step) {
                float cr = s->tw_re[k], ci = s->tw_im[k];
                float ur = re[i+j], ui = im[i+j];
                float xr = re[i+j+half], xi = im[i+j+half];
                float vr = xr*cr - xi*ci, vi = xr*ci + xi*cr;
                re[i+j] = ur+vr; im[i+j] = ui+vi;
                re[i+j+half] = ur-vr; im[i+j+half] = ui-vi;
            }
        }
    }
}

static float pm_hz2mel(float f) { return 2595.0f * log10f(1.0f + f / 700.0f); }
static float pm_mel2hz(float m) { return 700.0f * (powf(10.0f, m / 2595.0f) - 1.0f); }

static void pm_scale_init(PmScale *s, int n, int nbands, int sr, float weight)
{
    s->n = n; s->hop = n / 4; s->nbands = nbands; s->nbins = n/2 + 1; s->weight = weight;
    s->win = (float*)malloc(sizeof(float) * n);
    for (int i = 0; i < n; i++) s->win[i] = 0.5f - 0.5f * cosf(6.2831853f * i / (float)(n - 1));
    s->tw_re = (float*)malloc(sizeof(float) * (n/2));
    s->tw_im = (float*)malloc(sizeof(float) * (n/2));
    for (int k = 0; k < n/2; k++) { float a = -6.2831853f * k / (float)n; s->tw_re[k] = cosf(a); s->tw_im[k] = sinf(a); }

    s->bstart = (int*)malloc(sizeof(int) * nbands);
    s->blen   = (int*)malloc(sizeof(int) * nbands);
    s->boff   = (int*)malloc(sizeof(int) * nbands);
    float *edge = (float*)malloc(sizeof(float) * (nbands + 2));
    float lo = pm_hz2mel(30.0f), hi = pm_hz2mel(sr * 0.5f);
    for (int i = 0; i < nbands + 2; i++)
        edge[i] = pm_mel2hz(lo + (hi - lo) * i / (float)(nbands + 1)) * n / (float)sr;

    int total = 0;
    for (int b = 0; b < nbands; b++) {
        int l = (int)floorf(edge[b]), r = (int)ceilf(edge[b+2]) + 1;
        if (l < 0) l = 0; if (r > s->nbins) r = s->nbins; if (r < l) r = l;
        s->bstart[b] = l; s->blen[b] = r - l; s->boff[b] = total; total += r - l;
    }
    s->bw = (float*)calloc(total ? total : 1, sizeof(float));
    for (int b = 0; b < nbands; b++) {
        float l = edge[b], c = edge[b+1], r = edge[b+2];
        for (int i = 0; i < s->blen[b]; i++) {
            float f = (float)(s->bstart[b] + i), v = 0.0f;
            if (f > l && f <= c && c > l)      v = (f - l) / (c - l);
            else if (f > c && f < r && r > c)  v = (r - f) / (r - c);
            s->bw[s->boff[b] + i] = v;
        }
    }
    free(edge);
}

static void pm_scale_free(PmScale *s)
{ free(s->win); free(s->tw_re); free(s->tw_im); free(s->bstart); free(s->blen); free(s->boff); free(s->bw); }

static int pm_nframes(const PmScale *s, int len) { int nf = (len - s->n) / s->hop + 1; return nf < 1 ? 1 : nf; }

// log-mel spectrogram into `out` [nframes * nbands]; `re`/`im` are caller scratch of n floats.
static void pm_logmel(const PmScale *s, const float *x, int len, float *out, float *re, float *im)
{
    int nf = pm_nframes(s, len);
    for (int f = 0; f < nf; f++) {
        int off = f * s->hop;
        for (int i = 0; i < s->n; i++) {
            int idx = off + i;
            re[i] = (idx < len ? x[idx] : 0.0f) * s->win[i];
            im[i] = 0.0f;
        }
        pm_fft(s, re, im);
        float *row = out + (size_t)f * s->nbands;
        for (int b = 0; b < s->nbands; b++) {
            const float *w = s->bw + s->boff[b];
            int st = s->bstart[b], L = s->blen[b];
            float acc = 0.0f;
            for (int i = 0; i < L; i++) {
                int k = st + i;
                acc += w[i] * sqrtf(re[k]*re[k] + im[k]*im[k]);
            }
            row[b] = log10f(acc + 1e-5f);
        }
    }
}

typedef struct {
    int len, sr;
    PmScale sc[PM_NSCALES];
    float  *ref[PM_NSCALES];
    int     nf[PM_NSCALES];
    float  *scr_re, *scr_im, *scr_x, *scr_mel;   // reused, so the hot loop never mallocs
} PmTarget;

static void pm_rms_normalize(float *x, int n)
{
    double acc = 0.0;
    for (int i = 0; i < n; i++) acc += (double)x[i] * x[i];
    float r = (float)sqrt(acc / (n ? n : 1));
    if (r < 1e-9f) return;
    float g = 0.1f / r;
    for (int i = 0; i < n; i++) x[i] *= g;
}

static void pm_target_init(PmTarget *t, const float *sample, int len, int sr)
{
    static const int sizes[PM_NSCALES] = { 256, 512, 1024, 2048 };
    static const int bands[PM_NSCALES] = {  32,  40,   48,   48 };
    t->len = len; t->sr = sr;
    float *tmp = (float*)malloc(sizeof(float) * len);
    memcpy(tmp, sample, sizeof(float) * len);
    pm_rms_normalize(tmp, len);
    int maxn = 0, maxmel = 0;
    for (int s = 0; s < PM_NSCALES; s++) {
        pm_scale_init(&t->sc[s], sizes[s], bands[s], sr, 1.0f);
        t->nf[s] = pm_nframes(&t->sc[s], len);
        t->ref[s] = (float*)malloc(sizeof(float) * (size_t)t->nf[s] * bands[s]);
        if (sizes[s] > maxn) maxn = sizes[s];
        int m = t->nf[s] * bands[s]; if (m > maxmel) maxmel = m;
    }
    t->scr_re = (float*)malloc(sizeof(float) * maxn);
    t->scr_im = (float*)malloc(sizeof(float) * maxn);
    t->scr_x  = (float*)malloc(sizeof(float) * len);
    t->scr_mel= (float*)malloc(sizeof(float) * maxmel);
    for (int s = 0; s < PM_NSCALES; s++)
        pm_logmel(&t->sc[s], tmp, len, t->ref[s], t->scr_re, t->scr_im);
    free(tmp);
}

static void pm_target_free(PmTarget *t)
{
    for (int s = 0; s < PM_NSCALES; s++) { pm_scale_free(&t->sc[s]); free(t->ref[s]); }
    free(t->scr_re); free(t->scr_im); free(t->scr_x); free(t->scr_mel);
}

// How far a candidate render is from the target. Non-finite or silent renders get
// PM_BAD rather than a NaN: every comparison against NaN is false, so ONE bad
// candidate can freeze a whole population that never replaces it.
static float pm_distance(PmTarget *t, const float *cand)
{
    double energy = 0.0;
    for (int i = 0; i < t->len; i++) {
        float v = cand[i];
        if (!isfinite(v) || v > 1e6f || v < -1e6f) return PM_BAD;
        energy += (double)v * v;
    }
    if (energy < 1e-12) return PM_BAD;

    memcpy(t->scr_x, cand, sizeof(float) * t->len);
    pm_rms_normalize(t->scr_x, t->len);

    float total = 0.0f, wsum = 0.0f;
    for (int s = 0; s < PM_NSCALES; s++) {
        pm_logmel(&t->sc[s], t->scr_x, t->len, t->scr_mel, t->scr_re, t->scr_im);
        int n = t->nf[s] * t->sc[s].nbands;
        float acc = 0.0f;
        for (int i = 0; i < n; i++) { float d = t->scr_mel[i] - t->ref[s][i]; acc += d < 0 ? -d : d; }
        total += t->sc[s].weight * (n ? acc / (float)n : 0.0f);
        wsum  += t->sc[s].weight;
    }
    return total / wsum;
}

#endif
