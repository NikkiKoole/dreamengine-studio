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
    "summary": "A Wave-Candy-style AUDIO VISUALIZER with four views on the live master mix: a SPAN-style spectrum analyzer, a heat-colored spectrogram waterfall, an oscilloscope, and peak/RMS meters with a clip lamp - all fed by scope_read() and one in-cart 2048-point FFT.",
    "detail": "The audio-visualizer cart: four views (M cycles) over the REAL post-FX master mix. Every frame it pulls the newest 2048 samples with scope_read(), Hann-windows them, and runs an in-cart radix-2 FFT. SPECTRUM is the Voxengo SPAN layout: one-sided magnitude as dB (0..-72) against a log frequency axis (40 Hz..16 kHz), fast-attack/slow-release ballistics, a yellow peak-hold trace, and a cursor reading out the strongest partial. SPECTROGRAM is the Wave Candy waterfall: time scrolls left, frequency runs up the vertical axis, loudness is a heat ramp (black-blue-purple-red-yellow-white), so held notes draw horizontal stripes and the filter sweep carves a moving ridge you can read seconds after it happened. SCOPE draws the waveform itself, frozen at a rising zero crossing. METERS shows peak + RMS bars with meter ballistics, a white peak-hold tick, a latching CLIP lamp, and the crest factor (peak minus RMS - watch it collapse from a spiky saw to steady noise). Test sources make the physics visible: a SINE is one spike/stripe, a SQUARE has odd harmonics only, a SAW is the full comb, a CHORD is three interleaved combs, NOISE is the flat carpet. The missing Wave Candy view is the vectorscope: scope_read() taps the mono mix, so there is no L/R pair to plot.",
    "controls": "M cycles views (spectrum / spectrogram / scope / meters) - 1..5 pick the source (sine / square / saw / chord / noise) - Q toggles silence, F toggles the filter sweep, P clears peak holds + waterfall + clip lamp"
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ── WAVE CANDY — the audio visualizer: 4 views on the master mix ────────────
// All views read the REAL post-FX master via scope_read(); the FFT lives in the
// cart (the engine has no spectral API). The one Wave Candy view we CAN'T do is
// the vectorscope — the tap is mono, no L/R pair to plot.
//   SPECTRUM     SPAN-style: dB (0..-72) vs log frequency, peak hold, readout
//   SPECTROGRAM  the waterfall: time scrolls left, frequency vertical, loudness
//                as heat (black->blue->purple->red->yellow->white) — sweeps and
//                notes leave visible trails
//   SCOPE        the waveform itself, frozen at a rising zero crossing
//   METERS       peak + RMS bars (fast-attack/slow-release), peak hold, a clip
//                lamp, and the crest factor (peak minus RMS = the dynamics)
//
//   M      cycle views
//   1..5   source: SINE / SQUARE / SAW / CHORD / NOISE
//   F      ladder-filter sweep on/off (watch the comb get planed off)
//   Q      silence (analyze the noise floor)
//   P      clear (peak holds / the waterfall / the clip lamp)

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

static const char *SRC_NAME[5]  = { "SINE", "SQUARE", "SAW", "CHORD", "NOISE" };
static const char *VIEW_NAME[4] = { "spectrum", "spectrogram", "scope", "meters" };
static int   view    = 0;
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

// ── meters: peak + RMS over the same 2048-sample window, meter ballistics ────
static float pk_db = DB_FLOOR, rms_db = DB_FLOOR, pk_hold = DB_FLOOR;
static int   clip_t = 0;                // clip lamp latch, frames

// meters run left→right: -72 dB at the left edge, 0 dBFS at the right
static int db_x(float db) { return PX0 + (int)((1.0f - db / DB_FLOOR) * (float)PW + 0.5f); }

static void meter_bar(int y0, int y1, float db, float hold, const char *name) {
    int xe = db_x(db);
    for (int x = PX0; x <= xe; x++) {   // segment colors: green safe, red hot
        float cdb = DB_FLOOR * (1.0f - (float)(x - PX0) / (float)PW);
        int c = cdb > -3.0f ? CLR_RED : cdb > -6.0f ? CLR_ORANGE
              : cdb > -12.0f ? CLR_YELLOW : CLR_MEDIUM_GREEN;
        line(x, y0, x, y1, c);
    }
    if (hold > DB_FLOOR + 0.5f) { int xh = db_x(hold); line(xh, y0, xh, y1, CLR_WHITE); }
    print(name, PX0, y0 - 10, CLR_LIGHT_GREY);
    char num[16]; sprintf(num, "%.1f db", db);
    print(num, PX1 - (int)strlen(num) * 8, y0 - 10, CLR_LIGHT_PEACH);
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
    if (keyp('M')) view = (view + 1) % 4;
    if (keyp('P')) {
        for (int x = 0; x < PW; x++) hold_db[x] = DB_FLOOR;
        memset(sg, CLR_BROWNISH_BLACK, sizeof sg);
        pk_hold = DB_FLOOR; clip_t = 0;
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

    // meters: peak + RMS over the raw window, with meter ballistics
    float pk = 0.0f, sq = 0.0f;
    for (int i = 0; i < FFT_N; i++) {
        float a = fabsf(smp[i]);
        if (a > pk) pk = a;
        sq += smp[i] * smp[i];
    }
    float pdb = to_db(pk), rdb = to_db(sqrtf(sq / (float)FFT_N));
    pk_db  = pdb > pk_db ? pdb : pk_db - 0.5f;    // rise instantly, fall 30 dB/s
    rms_db += (rdb - rms_db) * 0.2f;
    if (pdb > pk_hold) pk_hold = pdb; else pk_hold -= 0.03f;
    if (pk_db < DB_FLOOR) pk_db = DB_FLOOR;
    if (pk_hold < DB_FLOOR) pk_hold = DB_FLOOR;
    if (pk >= 0.999f) clip_t = 90; else if (clip_t > 0) clip_t--;

    // ── draw ──
    cls(CLR_BROWNISH_BLACK);
    char title[40]; sprintf(title, "WAVE CANDY  %s", VIEW_NAME[view]);
    print(title, 8, 5, CLR_LIGHT_PEACH);

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
    } else if (view == 1) {
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
    } else if (view == 2) {
        // the oscilloscope: freeze the trace at a rising zero crossing
        int cy = (PY0 + PY1) / 2, amp = PH / 2 - 4, step = 3;
        int span = FFT_N - PW * step - 1, start = span;
        for (int i = span; i > 0; i--)                      // nearest crossing = freshest window
            if (smp[i-1] <= 0.0f && smp[i] > 0.0f) { start = i; break; }
        line(PX0, cy, PX1, cy, CLR_DARKER_GREY);            // zero line
        line(PX0, cy - amp, PX1, cy - amp, CLR_DARKER_GREY);
        line(PX0, cy + amp, PX1, cy + amp, CLR_DARKER_GREY);
        font(FONT_TINY);
        print("+1", PX0 - 12, cy - amp - 2, CLR_MEDIUM_GREY);
        print("0",  PX0 - 8,  cy - 2,       CLR_MEDIUM_GREY);
        print("-1", PX0 - 12, cy + amp - 2, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        float g = 1.0f;                                     // honest auto-zoom: power of 2, labelled
        while (g < 16.0f && pk * g < 0.5f) g *= 2.0f;
        for (int x = 1; x < PW; x++) {
            float a = smp[start + (x-1)*step] * g, b = smp[start + x*step] * g;
            a = a > 1.0f ? 1.0f : (a < -1.0f ? -1.0f : a);
            b = b > 1.0f ? 1.0f : (b < -1.0f ? -1.0f : b);
            line(PX0 + x - 1, cy - (int)(a * (float)amp), PX0 + x, cy - (int)(b * (float)amp), CLR_LIME_GREEN);
        }
        if (g > 1.0f) {
            char zl[8]; sprintf(zl, "x%d", (int)g);
            print(zl, PX1 - (int)strlen(zl) * 8 - 2, PY0 + 4, CLR_PEACH);
        }
    } else {
        // the meters: peak + RMS bars, peak hold tick, clip lamp, crest factor
        font(FONT_TINY);
        for (int db = 0; db >= (int)DB_FLOOR; db -= 12) {   // vertical dB grid
            int x = db_x((float)db);
            line(x, PY0, x, PY1, CLR_DARKER_GREY);
            char lab[8]; sprintf(lab, "%d", db);
            print(lab, x - (int)strlen(lab) * 2, PY1 + 4, CLR_MEDIUM_GREY);
        }
        font(FONT_NORMAL);
        meter_bar(PY0 + 24, PY0 + 52, pk_db,  pk_hold, "PEAK");
        meter_bar(PY0 + 80, PY0 + 108, rms_db, DB_FLOOR, "RMS");
        char info[48];
        sprintf(info, "crest %.1f db", pk_db - rms_db);     // peak - RMS = the dynamics
        print(info, PX0, PY0 + 120, CLR_LIGHT_GREY);
        rectfill(PX1 - 38, PY0 + 4, 34, 11, clip_t > 0 ? CLR_RED : CLR_DARKER_GREY);
        print("CLIP", PX1 - 36, PY0 + 6, clip_t > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
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
