/* de:meta
{
  "slug": "mictune",
  "title": "mictune (psola spike)",
  "status": "active",
  "kind": ["tech-demo"],
  "teaches": ["audio-input"],
  "created": "2026-07-17",
  "lineage": "The offline SPIKE for transparent auto-tune (docs/design/transparent-autotune.md): can TD-PSOLA move a voice's PITCH while keeping its FORMANTS still? A synthetic 'ah' vowel (sawtooth glottal source × fixed formant bank, KNOWN f0 so epochs are exact) is pitch-shifted up a fourth THREE ways: RAW (untouched), PSOLA (formant-preserving overlap-add), and a NAIVE resample control (varispeed-style — pitch AND formants move → the chipmunk). If PSOLA's spectral centroid stays with RAW while its pitch jumps to the control's, formant preservation works and the primitive is real. Prototyped in cart-land C — sound.h untouched until proven.",
  "description": {
    "summary": "A spike: does formant-preserving pitch-shift actually work? A synthetic 'ah' vowel is shifted up a fourth three ways — RAW, PSOLA (keeps the vowel), and a NAIVE resample (chipmunks it). A/B them: PSOLA should sound higher but still 'ah'; the naive one sounds squeaky. The WAV oracle is the real judge.",
    "detail": "Offline TD-PSOLA spike for transparent auto-tune. init() synthesizes a known-f0 (110 Hz) vowel: a sawtooth through a fixed 3-formant bandpass bank ('ah'). It builds three PCM buffers — RAW, PSOLA-shifted up +5 semitones (epochs at the known period, Hann grains re-spaced at the target period, overlap-add — so each grain keeps its formant ringing while the spacing carries the new pitch), and a NAIVE linear-resample control (reads faster → pitch and formants both rise). Plays them into three INSTR_SAMPLE slots at 1:1 so no engine resampling confounds the test. The proof is offline: render to WAV and check that PSOLA's pitch matches the control's (it moved) while its spectral centroid matches RAW's (the formants didn't). A/B by ear too.",
    "controls": "A: play RAW. S: play PSOLA (shifted, formant-kept). D: play NAIVE resample (chipmunk control). Space: auto-cycle A/S/D."
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// MICTUNE — offline spike for transparent auto-tune (docs/design/transparent-autotune.md).
// Question: can TD-PSOLA raise a voice's PITCH while its FORMANTS (its vowel/identity) stay put?
// Method: synthesize an 'ah' at a KNOWN f0, shift it +5 semitones two ways — PSOLA (should keep the
// vowel) and a naive resample (should chipmunk it) — and let the WAV oracle judge the centroid.

#define SR      44100
#define NLEN    66150            // 1.5 s of source
#define F0      110.0f           // A2 — the KNOWN source pitch (so epochs are exact, no detector error)
#define SEMIS   5.0f             // shift up a fourth — big enough that a naive resample's centroid MOVES obviously
#define RTMIDI  57               // A3 = MIDI 57; the sample root note (played 1:1)

static float raw[NLEN];          // the untouched 'ah'
static float psola[NLEN];        // PSOLA-shifted (formant-preserving)
static int   ctrlN = 0;
static float ctrl[NLEN];         // naive linear-resample control (varispeed-style — the WRONG way)

static int   playing = 0;        // which buffer last fired (0 raw / 1 psola / 2 ctrl), for the HUD
static int   autocyc = 0, fc = 0;

// synthesize an 'ah': a sawtooth glottal source shaped by a FIXED 3-formant bank (envelope is
// independent of f0 — that's the whole point of a formant model, and what PSOLA must preserve).
static void synth_vowel(float *out, int n) {
    float ph = 0.0f, inc = F0 / SR;
    static float saw[NLEN];
    for (int i = 0; i < n; i++) { saw[i] = 2.0f * ph - 1.0f; ph += inc; if (ph >= 1.0f) ph -= 1.0f; }
    for (int i = 0; i < n; i++) out[i] = 0.0f;
    // 'ah' formants (Hz): F1 730, F2 1090, F3 2440 — the classic open-vowel triad
    static float band[NLEN];
    float FC[3] = { 730.0f, 1090.0f, 2440.0f }, GA[3] = { 1.0f, 0.7f, 0.35f };
    for (int b = 0; b < 3; b++) {
        for (int i = 0; i < n; i++) band[i] = saw[i];
        // run one bandpass on a copy, add into out
        float w0 = 2.0f * 3.14159265f * FC[b] / SR, cw = cosf(w0), sw = sinf(w0), q = 9.0f;
        float alpha = sw / (2.0f * q);
        float b0 = alpha, b2 = -alpha, a0 = 1 + alpha, a1 = -2 * cw, a2 = 1 - alpha;
        b0 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (int i = 0; i < n; i++) {
            float x = band[i];
            float y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            out[i] += GA[b] * y;
        }
    }
    // normalize to ~0.6 peak
    float pk = 1e-6f; for (int i = 0; i < n; i++) { float a = fabsf(out[i]); if (a > pk) pk = a; }
    float g = 0.6f / pk; for (int i = 0; i < n; i++) out[i] *= g;
}

// TD-PSOLA pitch-shift, DURATION-PRESERVING. Known f0 → analysis epochs at k*T. For each output
// position (advanced by the TARGET period T/ratio), copy the Hann-windowed grain from the NEAREST
// analysis epoch and overlap-add. Grain length is pinned to the source period → the formant ringing
// inside each grain is untouched; only the grain SPACING changes → pitch moves, timbre stays.
static void psola_shift(const float *in, float *out, int n, float ratio) {
    float T  = (float)SR / F0;       // source period (samples) — keep FLOAT everywhere (no epoch drift)
    float Tt = T / ratio;            // target period (closer together = higher pitch)
    int   H  = (int)T;               // half-grain = one period → grain is 2T, 50%+ overlap
    static float norm[NLEN];
    for (int i = 0; i < n; i++) { out[i] = 0.0f; norm[i] = 0.0f; }
    for (float op = 0.0f; op < n; op += Tt) {
        int center = (int)(op + 0.5f);
        int a = (int)(roundf((float)center / T) * T + 0.5f);   // nearest analysis epoch k*T (float T → no drift)
        for (int j = -H; j <= H; j++) {
            int si = a + j, di = center + j;
            if (si < 0 || si >= n || di < 0 || di >= n) continue;
            float w = 0.5f * (1.0f + cosf(3.14159265f * (float)j / (float)H));  // Hann over [-H,H]
            out[di]  += w * in[si];
            norm[di] += w;
        }
    }
    for (int i = 0; i < n; i++) if (norm[i] > 1e-6f) out[i] /= norm[i];   // overlap-add gain normalize
}

// naive linear resample by `ratio` (varispeed / the WRONG way — pitch AND formants rise). Fewer output
// samples; that's fine, the oracle reads its centroid in Hz.
static int naive_resample(const float *in, float *out, int n, float ratio) {
    int m = 0;
    for (float rp = 0.0f; rp < n - 1 && m < n; rp += ratio, m++) {
        int i = (int)rp; float fr = rp - i;
        out[m] = in[i] * (1.0f - fr) + in[i + 1] * fr;
    }
    return m;
}

void init(void) {
    float ratio = powf(2.0f, SEMIS / 12.0f);   // +5 semitones ≈ 1.335
    synth_vowel(raw, NLEN);
    psola_shift(raw, psola, NLEN, ratio);
    ctrlN = naive_resample(raw, ctrl, NLEN, ratio);

    sample_load(0, raw,   NLEN);
    sample_load(1, psola, NLEN);
    sample_load(2, ctrl,  ctrlN);
    for (int s = 0; s < 3; s++) {
        instrument(10 + s, INSTR_SAMPLE, 2, 0, 7, 40);   // fast attack, full sustain — clean body
        instrument_sample(10 + s, s, RTMIDI);            // play at RTMIDI = 1:1 (no engine resample)
    }
}

static void fire(int which) { playing = which; hit(RTMIDI, 10 + which, 7, 1500); }

void update(void) {
    if (keyp('A')) fire(0);
    if (keyp('S')) fire(1);
    if (keyp('D')) fire(2);
    if (keyp(KEY_SPACE)) autocyc = !autocyc;

    // deterministic auto-sequence (also what the WAV oracle renders): raw → psola → ctrl, ~100 frames each
    if (autocyc || fc < 320) {
        if (fc == 10)  fire(0);
        if (fc == 120) fire(1);
        if (fc == 230) fire(2);
    }
    fc++;
    if (autocyc && fc > 330) fc = 0;

#ifdef DE_TRACE
    watch("playing", "%d", playing);
    watch("frame", "%d", fc);
#endif
}

static void draw_wave(const float *buf, int n, int y0, int h, int col) {
    int step = n / SCREEN_W; if (step < 1) step = 1;
    for (int x = 0; x < SCREEN_W; x++) {
        float mn = 1, mx = -1;
        for (int k = 0; k < step; k++) { int i = x * step + k; if (i < n) { float v = buf[i]; if (v < mn) mn = v; if (v > mx) mx = v; } }
        int ya = y0 + (int)(h * 0.5f * (1 - mx)), yb = y0 + (int)(h * 0.5f * (1 - mn));
        line(x, ya, x, yb, col);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    print("MICTUNE  formant-preserving pitch-shift spike (+5 semitones)", 6, 6, CLR_INDIGO);

    const char *nm[3] = { "RAW  'ah' @110Hz", "PSOLA  shifted, formants KEPT", "NAIVE resample  (chipmunk control)" };
    int col[3] = { CLR_LIGHT_GREY, CLR_GREEN, CLR_ORANGE };
    const float *bufs[3] = { raw, psola, ctrl };
    int ns[3] = { NLEN, NLEN, ctrlN };
    for (int s = 0; s < 3; s++) {
        int y0 = 26 + s * 52;
        print(nm[s], 6, y0, playing == s ? CLR_WHITE : col[s]);
        draw_wave(bufs[s], ns[s], y0 + 10, 34, col[s]);
    }
    print("A raw   S psola   D naive   SPACE autocycle", 6, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
