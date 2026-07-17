/* de:meta
{
  "slug": "mictune",
  "title": "mictune (psola auto-tune spike)",
  "status": "active",
  "kind": ["tech-demo"],
  "teaches": ["audio-input"],
  "created": "2026-07-17",
  "lineage": "The offline SPIKE for transparent auto-tune (docs/design/transparent-autotune.md). v1 proved TD-PSOLA keeps FORMANTS still while moving pitch (known-f0 'ah', +5 st). v2 (this) takes on the REAL risk: epochs from a DETECTOR not a known f0 (autocorrelation pitch track + peak-refined glottal marks), a HARD voice-like source (vibrato + drift + breath noise, sitting OFF a scale note), and actual pitch-CORRECTION (time-varying PSOLA snapping each frame to snap_scale — de-wobble + snap onto the scale, the modern-pop vocal). Press R to tune your OWN voice via mic_record. Still cart-land C — sound.h untouched until the algorithm is proven end to end.",
  "description": {
    "summary": "Auto-tune that keeps your voice. A wobbly, slightly-off 'ah' (vibrato + drift) is pitch-corrected onto the scale — the wobble flattens and it snaps in tune, but it still sounds like the same vowel (not a chipmunk). A/B raw vs tuned. Press R to tune YOUR voice from the mic.",
    "detail": "Offline auto-tune spike (docs/design/transparent-autotune.md). init() synthesizes a HARD source: a sawtooth 'ah' (fixed 3-formant bank) whose pitch wobbles (5.5 Hz vibrato + slow drift) around a note deliberately BETWEEN scale notes, plus breath noise. Then, using NO prior knowledge of the pitch: an autocorrelation tracker estimates f0 per hop, glottal epochs are placed from that track and refined to local peaks, and a time-varying TD-PSOLA re-spaces the grains at the TARGET period (snap_scale of the detected pitch) — so the output pitch locks to the scale and stops wobbling, while each grain keeps its formant ringing (the vowel/identity survives). The proof is offline + deterministic: render to WAV and check the tuned f0 is steady on the scale note while the formants match the raw. R records ~2 s of your live voice and runs the exact same pipeline (live-only, ADR-0032).",
    "controls": "A: play RAW (wobbly, off). S: play TUNED (corrected, formants kept). R: record your voice + tune it. Space: A/B raw vs tuned."
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// MICTUNE v2 — offline auto-tune spike (docs/design/transparent-autotune.md). The real-risk test:
// DETECTOR-derived epochs (not a known f0) + a wobbly, breathy, off-pitch source + correct-to-scale.
// Question: does formant-preserving pitch-CORRECTION survive when the pitch must be tracked, not told?

#define SR      44100
#define NLEN    88200            // 2.0 s
#define HOP     256              // pitch-track hop
#define ACW     1024             // autocorrelation window
#define MAXEP   4096             // max epochs

typedef struct { const char *name; int n; int deg[12]; } Scale;
static const Scale SCALES[] = {
    { "minor",  7, {0, 2, 3, 5, 7, 8, 10} },
    { "major",  7, {0, 2, 4, 5, 7, 9, 11} },
    { "penta-", 5, {0, 3, 5, 7, 10} },
    { "chroma", 12,{0,1,2,3,4,5,6,7,8,9,10,11} },
};
static int scale_i = 0;
static float snap_scale(float midi) {                 // nearest note whose pitch-class is in the scale
    const Scale *sc = &SCALES[scale_i];
    int best = (int)lroundf(midi); float bestd = 1e9f;
    for (int oct = -1; oct <= 9; oct++)
        for (int i = 0; i < sc->n; i++) {
            int cand = oct * 12 + sc->deg[i];
            float d = fabsf(midi - cand);
            if (d < bestd) { bestd = d; best = cand; }
        }
    return (float)best;
}
static float hz2midi(float hz) { return hz > 0 ? 69.0f + 12.0f * log2f(hz / 440.0f) : 0.0f; }
static float midi2hz(float m)  { return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f); }

static float raw[NLEN];          // the wobbly 'ah'
static float tuned[NLEN];        // auto-tuned (detector epochs + snap-to-scale)
static float f0trk[NLEN / HOP + 4];
static int   nHop = 0;
static int   ep[MAXEP];          // analysis epoch positions
static int   nEp = 0;

// live mic path
static int   live_state = 0;     // 0 idle, 1 starting mic, 2 recording, 3 ready
static int   liveN = 0;

// ── synth a HARD 'ah': wobbly pitch (vibrato + drift) + fixed formants + breath noise ──
static void synth_wobbly(float *out, int n) {
    static float saw[NLEN];
    float ph = 0.0f, drift = 0.0f; int seed = 12345;
    float baseM = 46.35f;                                 // ~119 Hz — a third of a semitone SHARP of A#2(46): off, but inside A#'s basin
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        seed = seed * 1103515245 + 12345;
        float rnd = ((seed >> 16) & 0xffff) / 32767.5f - 1.0f;
        drift += 0.0004f * rnd; if (drift > 0.2f) drift = 0.2f; if (drift < -0.2f) drift = -0.2f;  // slow ±0.2 st walk (stays in-basin)
        float vib = 0.35f * sinf(2.0f * 3.14159265f * 5.5f * t);                                   // 5.5 Hz vibrato ±0.35 st
        float f0  = midi2hz(baseM + vib + drift);
        saw[i] = 2.0f * ph - 1.0f; ph += f0 / SR; if (ph >= 1.0f) ph -= 1.0f;
    }
    for (int i = 0; i < n; i++) out[i] = 0.0f;
    static float band[NLEN];
    float FC[3] = { 730.0f, 1090.0f, 2440.0f }, GA[3] = { 1.0f, 0.7f, 0.35f };
    for (int b = 0; b < 3; b++) {
        float w0 = 2.0f * 3.14159265f * FC[b] / SR, cw = cosf(w0), sw = sinf(w0), q = 9.0f, alpha = sw / (2 * q);
        float b0 = alpha, b2 = -alpha, a0 = 1 + alpha, a1 = -2 * cw, a2 = 1 - alpha;
        b0 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (int i = 0; i < n; i++) {
            float x = saw[i], y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y; out[i] += GA[b] * y;
        }
    }
    int seed2 = 99;                                         // breath: a little white noise mixed in
    for (int i = 0; i < n; i++) {
        seed2 = seed2 * 1103515245 + 12345;
        float w = ((seed2 >> 16) & 0xffff) / 32767.5f - 1.0f;
        out[i] += 0.05f * w;
    }
    float pk = 1e-6f; for (int i = 0; i < n; i++) { float a = fabsf(out[i]); if (a > pk) pk = a; }
    float g = 0.6f / pk; for (int i = 0; i < n; i++) out[i] *= g;
}

// ── autocorrelation pitch track (per hop): f0 in Hz, 0 = unvoiced ──
static float ac_corr(const float *x, int base, int n, int lag) {
    float s = 0, e0 = 0, e1 = 0;
    for (int i = 0; i < ACW && base + i + lag < n; i++) { float a = x[base + i], b = x[base + i + lag]; s += a * b; e0 += a * a; e1 += b * b; }
    return s / (sqrtf(e0 * e1) + 1e-9f);
}
static void track_pitch(const float *x, int n) {
    nHop = (n - ACW) / HOP;
    int lo = SR / 400, hi = SR / 70;                       // 70..400 Hz
    for (int h = 0; h < nHop; h++) {
        int base = h * HOP;
        float best = 0; int bl = lo;
        for (int lag = lo; lag <= hi; lag++) { float c = ac_corr(x, base, n, lag); if (c > best) { best = c; bl = lag; } }
        if (best < 0.35f) { f0trk[h] = 0.0f; continue; }   // unvoiced / no clear pitch
        float cm = ac_corr(x, base, n, bl - 1), c0 = ac_corr(x, base, n, bl), cp = ac_corr(x, base, n, bl + 1);
        float denom = (cm - 2 * c0 + cp);
        float d = denom != 0 ? 0.5f * (cm - cp) / denom : 0.0f;   // parabolic sub-sample refine
        f0trk[h] = SR / (bl + d);
    }
}
static float f0_at(int pos) {
    int h = pos / HOP; if (h < 0) h = 0; if (h >= nHop) h = nHop - 1;
    return f0trk[h];
}

// ── epoch marking from the f0 track: step by the local period, refine to the local peak ──
static int mark_epochs(const float *x, int n) {
    int m = 0, pos = ACW / 2;
    while (pos < n - 1 && m < MAXEP) {
        ep[m++] = pos;
        float f = f0_at(pos); if (f < 60.0f) f = 110.0f;
        int T = (int)(SR / f);
        int a = pos + (int)(0.72f * T), b = pos + (int)(1.28f * T);   // search ±28% around one period ahead
        if (b >= n) break;
        int pk = a; float pv = -1e9f;
        for (int i = a; i <= b; i++) if (x[i] > pv) { pv = x[i]; pk = i; }
        pos = pk;
    }
    return m;
}

// ── time-varying TD-PSOLA correcting toward snap_scale (de-wobble + snap) ──
static void psola_tune(const float *x, int n, float *out) {
    static float norm[NLEN];
    for (int i = 0; i < n; i++) { out[i] = 0.0f; norm[i] = 0.0f; }
    if (nEp < 3) return;
    int ai = 0;
    for (float op = (float)ep[0]; op < n; ) {
        while (ai + 1 < nEp && ep[ai + 1] <= (int)op) ai++;      // nearest analysis epoch ≤ op
        int a = ep[ai];
        if (ai + 1 < nEp && (int)op - ep[ai] > ep[ai + 1] - (int)op) a = ep[ai + 1];
        int T = (ai + 1 < nEp) ? (ep[ai + 1] - ep[ai]) : (int)(SR / 110.0f);   // local source period
        if (T < 20) T = 20; if (T > 900) T = 900;
        int center = (int)(op + 0.5f);
        for (int j = -T; j <= T; j++) {
            int si = a + j, di = center + j;
            if (si < 0 || si >= n || di < 0 || di >= n) continue;
            float w = 0.5f * (1.0f + cosf(3.14159265f * (float)j / (float)T));
            out[di] += w * x[si]; norm[di] += w;
        }
        float f = f0_at(center); if (f < 60.0f) f = 110.0f;
        float tgt = midi2hz(snap_scale(hz2midi(f)));            // the correction target: nearest scale note
        float Tt = SR / tgt;
        op += Tt;
    }
    for (int i = 0; i < n; i++) if (norm[i] > 1e-6f) out[i] /= norm[i];
}

static void run_pipeline(float *src, float *dst, int n) {      // track → mark → tune
    track_pitch(src, n);
    nEp = mark_epochs(src, n);
    psola_tune(src, n, dst);
}

static int   playing = 0, fc = 0, autocyc = 0;

void init(void) {
    synth_wobbly(raw, NLEN);
    run_pipeline(raw, tuned, NLEN);
    sample_load(0, raw,   NLEN);
    sample_load(1, tuned, NLEN);
    for (int s = 0; s < 2; s++) { instrument(10 + s, INSTR_SAMPLE, 2, 0, 7, 40); instrument_sample(10 + s, s, 57); }
    instrument(12, INSTR_SAMPLE, 2, 0, 7, 40);   // live raw
    instrument(13, INSTR_SAMPLE, 2, 0, 7, 40);   // live tuned
}

static void fire(int which) { playing = which; hit(57, 10 + which, 7, 2000); }

void update(void) {
    if (keyp('A')) fire(0);
    if (keyp('S')) fire(1);

    // live mic: press R → record 2s → tune → play
    if (keyp('R') && live_state == 0) { mic_start(); live_state = 1; }
    if (live_state == 1 && mic_active()) { mic_record(2.0f); live_state = 2; }
    if (live_state == 2 && !mic_recording()) {
        static float lr[NLEN], lt[NLEN];
        liveN = mic_record_read(lr, NLEN);
        if (liveN > ACW * 2) {
            run_pipeline(lr, lt, liveN);
            sample_load(2, lr, liveN); sample_load(3, lt, liveN);
            instrument_sample(12, 2, 57); instrument_sample(13, 3, 57);
        }
        live_state = 3;
    }
    if (live_state == 3) {
        if (keyp('Z')) { playing = 2; hit(57, 12, 7, 2000); }
        if (keyp('X')) { playing = 3; hit(57, 13, 7, 2000); }
    }

    if (autocyc || fc < 260) { if (fc == 10) fire(0); if (fc == 130) fire(1); }
    fc++; if (autocyc && fc > 270) fc = 0;
    if (keyp(KEY_SPACE)) autocyc = !autocyc;

#ifdef DE_TRACE
    watch("playing", "%d", playing);
    watch("nEp", "%d", nEp);
#endif
}

static void draw_wave(const float *buf, int n, int y0, int h, int col) {
    int step = n / SCREEN_W; if (step < 1) step = 1;
    for (int x = 0; x < SCREEN_W; x++) {
        float mn = 1, mx = -1;
        for (int k = 0; k < step; k++) { int i = x * step + k; if (i < n) { float v = buf[i]; if (v < mn) mn = v; if (v > mx) mx = v; } }
        line(x, y0 + (int)(h * 0.5f * (1 - mx)), x, y0 + (int)(h * 0.5f * (1 - mn)), col);
    }
}
// a pitch ribbon: the detected f0 track + its snapped target, over time
static void draw_pitch(int y0, int h) {
    float loM = 43, hiM = 50;
    for (int x = 0; x < SCREEN_W && x < nHop; x++) {
        float f = f0trk[x]; if (f < 60) continue;
        float m = hz2midi(f), sm = snap_scale(m);
        int yr = y0 + (int)(h * (1 - (m  - loM) / (hiM - loM)));
        int yt = y0 + (int)(h * (1 - (sm - loM) / (hiM - loM)));
        pset(x, yr, CLR_ORANGE);          // raw wobbling pitch
        pset(x, yt, CLR_GREEN);           // snapped target (the staircase)
    }
}

void draw(void) {
    cls(CLR_BLACK);
    print("MICTUNE  formant-preserving AUTO-TUNE spike", 6, 5, CLR_INDIGO);
    print("RAW  wobbly 'ah', off-pitch", 6, 22, playing == 0 ? CLR_WHITE : CLR_LIGHT_GREY);
    draw_wave(raw, NLEN, 32, 26, CLR_LIGHT_GREY);
    print("TUNED  snapped to scale, vowel kept", 6, 64, playing == 1 ? CLR_WHITE : CLR_GREEN);
    draw_wave(tuned, NLEN, 74, 26, CLR_GREEN);
    print("PITCH  orange=detected  green=snapped target", 6, 106, CLR_MEDIUM_GREY);
    draw_pitch(116, 40);
    if (live_state == 2) print("recording... sing an 'ah'", 6, SCREEN_H - 26, CLR_ORANGE);
    else if (live_state == 3) print("your voice tuned!  Z raw  X tuned", 6, SCREEN_H - 26, CLR_GREEN);
    print("A raw  S tuned  SPACE a/b  R record your voice", 6, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
