/* de:meta
{
  "title": "wavecandy",
  "status": "active",
  "created": "2026-07-10",
  "kind": [
    "tool",
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization",
    "additive-synth"
  ],
  "homage": "Voxengo SPAN (the spectrum view) + FL Studio Wave Candy (the multi-view visualizer, source of the spectrogram)",
  "description": {
    "summary": "A SPAN-style SPECTRUM ANALYZER plus a Wave-Candy-style SPECTROGRAM waterfall over the live master mix: scope_read() feeds a 2048-point FFT, drawn as dB-vs-log-frequency with peak hold, or as scrolling heat-colored trails - watch a saw's harmonic comb, a square's missing even partials, a ladder sweep planing the top off.",
    "detail": "The frequency-domain twin of the oscilloscope carts, with two views on the same in-cart FFT. Every frame the cart pulls the newest 2048 post-FX master samples with scope_read(), Hann-windows them, runs a radix-2 FFT, and shows the result two ways (M switches). SPECTRUM is the Voxengo SPAN layout: one-sided magnitude as dB (0..-72) against a log frequency axis (40 Hz..16 kHz), fast-attack/slow-release ballistics, a yellow peak-hold trace, and a cursor reading out the strongest partial. SPECTROGRAM is the Wave Candy waterfall: time scrolls left, frequency runs up the vertical axis, loudness is a heat ramp (black-blue-purple-red-yellow-white), so held notes draw horizontal stripes and the filter sweep carves a moving ridge you can read seconds after it happened. Test sources make the physics visible: a SINE is one spike/stripe, a SQUARE has odd harmonics only, a SAW is the full comb, a CHORD is three interleaved combs, NOISE is the flat carpet. Toggle the ladder-filter sweep and watch the comb's top get planed off and pushed back - the same picture SPAN shows a producer EQing a mix.",
    "controls": "M switches view (spectrum / spectrogram) - 1..5 pick the source (sine / square / saw / chord / noise) - Q toggles silence, F toggles the filter sweep, P clears peak hold + waterfall"
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ── WAVE CANDY — spectrum analyzer + spectrogram ────────────────────────────
// Two views over the REAL post-FX master mix (scope_read; the FFT lives in the
// cart — the engine has no spectral API):
//   SPECTRUM     SPAN-style: dB (0..-72) vs log frequency, peak hold, readout
//   SPECTROGRAM  the waterfall: time scrolls left, frequency vertical, loudness
//                as heat (black->blue->purple->red->yellow->white) — sweeps and
//                notes leave visible trails
//
//   M      switch view
//   1..5   source: SINE / SQUARE / SAW / CHORD / NOISE
//   F      ladder-filter sweep on/off (watch the comb get planed off)
//   Q      silence (analyze the noise floor)
//   P      clear (peak hold / the waterfall)

#define SL      8                       // our instrument slot
#define FFT_N   2048                    // scope_read's max window
#define BIN_HZ  (44100.0f / FFT_N)      // ~21.5 Hz per bin

// plot area
#define PX0 26
#define PX1 312
#define PY0 18
#define PY1 158
#define PW  (PX1 - PX0)
#define PH  (PY1 - PY0)
#define F_LO   40.0f
#define F_HI   16000.0f
#define DB_FLOOR (-72.0f)

static float smp[FFT_N], fre[FFT_N], fim[FFT_N], hann[FFT_N];
static float mag[FFT_N / 2];            // one-sided linear magnitude (1.0 = 0 dBFS sine)
static float disp_db[PW];               // smoothed curve, one column per pixel
static float hold_db[PW];               // peak-hold trace

static const char *SRC_NAME[5] = { "SINE", "SQUARE", "SAW", "CHORD", "NOISE" };
static int   view    = 0;               // 0 = spectrum, 1 = spectrogram
static int   source  = 2;               // start on SAW: the full harmonic comb
static int   muted   = 0;
static int   sweep   = 0;
static float swp_ph  = 0.0f;
static int   cutoff  = 16000;
static int   hnd[3]  = { -1, -1, -1 };  // up to 3 held notes (the chord)

// in-place iterative radix-2 FFT (the textbook one; 2048 points is nothing at 60fps)
static void fft(float *re, float *im, int n) {
    for (int i = 1, j = 0; i < n; i++) {          // bit-reversal permutation
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t;
            t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -6.2831853f / (float)len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float vr = re[i+k+len/2]*cr - im[i+k+len/2]*ci;
                float vi = re[i+k+len/2]*ci + im[i+k+len/2]*cr;
                re[i+k+len/2] = re[i+k] - vr;  im[i+k+len/2] = im[i+k] - vi;
                re[i+k]      += vr;            im[i+k]      += vi;
                float t = cr*wr - ci*wi; ci = cr*wi + ci*wr; cr = t;
            }
        }
    }
}

// column x (0..PW-1) → frequency, log-spaced F_LO..F_HI
static float col_freq(float x) { return F_LO * powf(F_HI / F_LO, x / (float)(PW - 1)); }

// linear magnitude at an arbitrary frequency, lerped between bins
static float mag_at(float hz) {
    float p = hz / BIN_HZ;
    int   i = (int)p;
    if (i < 1) i = 1;
    if (i >= FFT_N/2 - 1) return mag[FFT_N/2 - 1];
    float fr = p - (float)i;
    return mag[i] + (mag[i+1] - mag[i]) * fr;
}

static float to_db(float m) {
    float db = 20.0f * log10f(m + 1e-9f);
    return db < DB_FLOOR ? DB_FLOOR : (db > 0.0f ? 0.0f : db);
}

static int db_y(float db) { return PY0 + (int)((-db / -DB_FLOOR) * (float)PH + 0.5f); }

// loudest dB across a frequency band — many bins per band: take the max (keeps
// narrow peaks honest); band thinner than a bin: lerp between neighbours
static float band_db(float flo, float fhi, float fmid) {
    int b0 = (int)(flo / BIN_HZ) + 1, b1 = (int)(fhi / BIN_HZ);
    float m;
    if (b1 >= b0) {
        m = 0.0f;
        if (b0 < 1) b0 = 1;
        if (b1 > FFT_N/2 - 1) b1 = FFT_N/2 - 1;
        for (int b = b0; b <= b1; b++) if (mag[b] > m) m = mag[b];
    } else {
        m = mag_at(fmid);
    }
    return to_db(m);
}

// ── spectrogram: a ring buffer of heat-colored columns, newest at the right ──
static unsigned char sg[PW * PH];       // palette indices, column-major
static int sg_w = 0;                    // ring write head (slot for the next column)

// spectrogram row (0 = top) → frequency: same log axis, high frequencies up
static float row_freq(float y) { return F_LO * powf(F_HI / F_LO, ((float)(PH - 1) - y) / (float)(PH - 1)); }

static const unsigned char HEAT[12] = {  // quiet → loud, 6 dB per step
    CLR_BROWNISH_BLACK, CLR_DARKER_BLUE, CLR_DARK_BLUE, CLR_DARKER_PURPLE,
    CLR_DARK_PURPLE, CLR_DARK_RED, CLR_RED, CLR_DARK_ORANGE,
    CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_WHITE
};
static int heat_of(float db) {
    int i = (int)((db - DB_FLOOR) / -DB_FLOOR * 12.0f);
    return HEAT[i < 0 ? 0 : (i > 11 ? 11 : i)];
}

static void stop_all(void) {
    for (int i = 0; i < 3; i++) if (hnd[i] >= 0) { note_off(hnd[i]); hnd[i] = -1; }
}

static void start_source(void) {
    stop_all();
    if (muted) return;
    int wave = INSTR_SAW;
    if (source == 0) wave = INSTR_SINE;
    if (source == 1) wave = INSTR_SQUARE;
    if (source == 4) wave = INSTR_NOISE;
    instrument(SL, wave, 5, 0, 7, 80);            // sustained pad: hold forever, quick fades
    instrument_filter(SL, FILTER_LADDER, cutoff, 2);
    if (source == 3) {                            // CHORD: A major triad, three interleaved combs
        hnd[0] = note_on(45, SL, 4);              // A2 110 Hz
        hnd[1] = note_on(49, SL, 4);              // C#3
        hnd[2] = note_on(52, SL, 4);              // E3
    } else {
        hnd[0] = note_on(45, SL, 5);              // A2: harmonics land every 110 Hz
    }
}

void update(void) {
    for (int k = 0; k < 5; k++) if (keyp('1' + k)) { source = k; start_source(); }
    if (keyp('Q')) { muted = !muted; start_source(); }
    if (keyp('F')) { sweep = !sweep; if (!sweep) { cutoff = 16000; for (int i=0;i<3;i++) if (hnd[i]>=0) note_cutoff(hnd[i], cutoff); } }
    if (keyp('M')) view = !view;
    if (keyp('P')) {
        for (int x = 0; x < PW; x++) hold_db[x] = DB_FLOOR;
        memset(sg, CLR_BROWNISH_BLACK, sizeof sg);
    }

    if (sweep) {                                  // slow exponential triangle, 200 Hz .. 12 kHz
        swp_ph += 0.12f / 60.0f;
        if (swp_ph >= 1.0f) swp_ph -= 1.0f;
        float tri = swp_ph < 0.5f ? swp_ph * 2.0f : 2.0f - swp_ph * 2.0f;
        cutoff = (int)(200.0f * powf(60.0f, tri));
        for (int i = 0; i < 3; i++) if (hnd[i] >= 0) note_cutoff(hnd[i], cutoff);  // note_* is built to be ridden live
    }
}

void draw(void) {
    // ── analyze: newest 2048 master samples → Hann window → FFT → magnitudes ──
    scope_read(smp, FFT_N);
    for (int i = 0; i < FFT_N; i++) { fre[i] = smp[i] * hann[i]; fim[i] = 0.0f; }
    fft(fre, fim, FFT_N);
    for (int i = 0; i < FFT_N / 2; i++)           // ×4/N: one-sided ×2, Hann coherent gain ×2
        mag[i] = sqrtf(fre[i]*fre[i] + fim[i]*fim[i]) * (4.0f / (float)FFT_N);

    // per-column target with SPAN-style ballistics: jump up fast, fall back slow
    for (int x = 0; x < PW; x++) {
        float db = band_db(col_freq((float)x - 0.5f), col_freq((float)x + 0.5f), col_freq((float)x));
        disp_db[x] += (db - disp_db[x]) * (db > disp_db[x] ? 0.6f : 0.10f);
        if (disp_db[x] > hold_db[x]) hold_db[x] = disp_db[x];
        else                         hold_db[x] -= 0.06f;   // peak hold decays gently
        if (hold_db[x] < DB_FLOOR)   hold_db[x] = DB_FLOOR;
    }

    // append one spectrogram column every frame (even in spectrum view, so
    // toggling shows history instead of a gap)
    unsigned char *newest = &sg[sg_w * PH];
    for (int y = 0; y < PH; y++)
        newest[y] = (unsigned char)heat_of(band_db(row_freq((float)y + 0.5f), row_freq((float)y - 0.5f), row_freq((float)y)));
    sg_w = (sg_w + 1) % PW;

    // ── draw ──
    cls(CLR_BROWNISH_BLACK);
    print(view == 0 ? "WAVE CANDY  spectrum" : "WAVE CANDY  spectrogram", 8, 5, CLR_LIGHT_PEACH);

    static const float GRID_HZ[8] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    static const char *GRID_LAB[8] = { 0, "100", 0, "500", "1K", 0, "5K", "10K" };

    if (view == 0) {
        font(FONT_TINY);
        for (int db = 0; db >= (int)DB_FLOOR; db -= 12) {   // horizontal dB grid
            int y = db_y((float)db);
            line(PX0, y, PX1, y, CLR_DARKER_GREY);
            char lab[8]; sprintf(lab, "%d", db);
            print(lab, PX0 - 4 - (int)strlen(lab) * 4, y - 2, CLR_MEDIUM_GREY);
        }
        for (int g = 0; g < 8; g++) {                       // vertical frequency grid
            int x = PX0 + (int)(logf(GRID_HZ[g] / F_LO) / logf(F_HI / F_LO) * (float)(PW - 1) + 0.5f);
            line(x, PY0, x, PY1, CLR_DARKER_GREY);
            if (GRID_LAB[g]) print(GRID_LAB[g], x - (int)strlen(GRID_LAB[g]) * 2, PY1 + 4, CLR_MEDIUM_GREY);
        }
        font(FONT_NORMAL);

        for (int x = 0; x < PW; x++) {                      // filled spectrum + peak hold
            int y  = db_y(disp_db[x]);
            int yh = db_y(hold_db[x]);
            if (y < PY1) line(PX0 + x, y, PX0 + x, PY1, CLR_DARK_GREEN);
            if (yh < PY1) pset(PX0 + x, yh, CLR_YELLOW);
        }
        for (int x = 1; x < PW; x++)                        // bright top edge
            line(PX0 + x - 1, db_y(disp_db[x-1]), PX0 + x, db_y(disp_db[x]), CLR_LIME_GREEN);
    } else {
        // the waterfall: oldest column at the left, newest at the right
        for (int x = 0; x < PW; x++) {
            const unsigned char *c = &sg[((sg_w + x) % PW) * PH];
            for (int y = 0; y < PH; y++)
                if (c[y] != CLR_BROWNISH_BLACK) pset(PX0 + x, PY0 + y, c[y]);
        }
        font(FONT_TINY);
        for (int g = 0; g < 8; g++) {                       // frequency labels, high at the top
            if (!GRID_LAB[g]) continue;
            int y = PY0 + (int)((1.0f - logf(GRID_HZ[g] / F_LO) / logf(F_HI / F_LO)) * (float)(PH - 1) + 0.5f);
            print(GRID_LAB[g], PX0 - 4 - (int)strlen(GRID_LAB[g]) * 4, y - 2, CLR_MEDIUM_GREY);
        }
        font(FONT_NORMAL);
    }
    rect(PX0, PY0, PW + 1, PH + 1, CLR_DARK_GREY);

    // cursor readout: the strongest partial on screen
    int best = 1;
    for (int b = 2; b < FFT_N / 2; b++) if (mag[b] > mag[best]) best = b;
    char info[64];
    if (to_db(mag[best]) > DB_FLOOR + 1.0f) {
        sprintf(info, "peak %d hz  %.1f db", (int)(best * BIN_HZ + 0.5f), to_db(mag[best]));
        print(info, PX0, 168, CLR_LIGHT_GREY);
    }
    sprintf(info, "%s%s%s", muted ? "MUTED" : SRC_NAME[source],
            sweep ? "  sweep " : "", sweep ? "" : "");
    if (sweep) sprintf(info + strlen(info), "%d hz", cutoff);
    print(info, PX0, 178, CLR_PEACH);
    font(FONT_SMALL);
    print("M VIEW  1-5 SOURCE  F SWEEP  Q MUTE  P CLEAR", PX0, 190, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

void init(void) {
    for (int i = 0; i < FFT_N; i++)
        hann[i] = 0.5f - 0.5f * cosf(6.2831853f * (float)i / (float)(FFT_N - 1));
    for (int x = 0; x < PW; x++) { disp_db[x] = DB_FLOOR; hold_db[x] = DB_FLOOR; }
    memset(sg, CLR_BROWNISH_BLACK, sizeof sg);
    start_source();
}
