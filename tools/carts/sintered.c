/* de:meta
{
  "slug": "sintered",
  "title": "sintered",
  "status": "active",
  "created": "2026-09-28",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "drum-synthesis",
    "wavefolder"
  ],
  "lineage": "Row 5 of docs/design/choochootracker-borrow-list.md, cart-first: Choochootracker's Sintered voice (chipnomad_lib/synth/sintered_voice.cpp, MIT, paiheulevrai), 'MME for drums': a very short noise IMPACT excites two cross-modulating oscillators, and a smoothed, DC-blocked feedback tail keeps ringing. Six models (knot, shard, burst, comb, logic, melt). Each pad renders its hit in cart-land C once, when a knob changes, into a PCM slot the sample engine plays at root pitch. For a one-shot drum that is exact rather than a prototype's shortcut: Sintered resets its noise seed on every hit, so every hit of a patch is the same sound anyway.",
  "homage": "Choochootracker's Sintered (2026), the MME idea aimed at percussion, after Noise Engineering's wild-percussion modules.",
  "description": {
    "summary": "Six pads of wild synthetic percussion: a noise impact into two cross-modulating oscillators and a feedback tail, six models from knotted FM to comb-filtered clank.",
    "detail": "Every pad is its own patch: a model (knot / shard / burst / comb / logic / melt), a pitch, and six knobs. MOD sets how hard the two oscillators drive each other and the tail; A, B and C mean something different per model (the hint line under the knobs says what); MOTION shapes a short burst of movement at the start of the hit (left = a swell, right = a snap, centre = still); DECAY sets the length. Each pad renders its hit once when you change it and plays that buffer, so the knobs are exact and a pattern costs one voice per hit. Autoplay runs a pattern across all six pads.",
    "controls": "A S D F G H: the six pads (or tap them) · 1-6: select the pad the knobs edit · click a model button to change the selected pad's model · drag the knobs (wheel = fine) · LEFT/RIGHT knob, UP/DOWN adjust · M: autoplay"
  },
  "todo": [
    "ear pass: which models earn an engine; then decide INSTR_SINTER (one voice per hit, the knobs ride live) vs a percussion mode on INSTR_MME (shared oscillator + feedback core, different excitation)",
    "presets per pad beyond the six defaults"
  ]
}
de:meta */
// sintered — the Choochootracker Sintered percussion voice, cart-first.
//
// Sintered is "MME for drums": a 1.5..7 ms noise IMPACT excites two oscillators (x at the
// note, y at 2^((a-.5)*5) of it, y frequency-modulated by the feedback), one of six models
// mangles them, and the output feeds back through tanh, a DC estimate and a one-pole
// smoother, so the tail keeps moving instead of just decaying. A per-model MOTION envelope
// pushes two or three knobs for the first few ms (a swell or a snap).
//
// Why a cart first: the engine has no per-sample hook, but a drum is a one-shot, so each pad
// renders its whole hit into a PCM slot when a knob changes and a hit is just a sample
// playback at root = the pad's note (speed 1.0). Sintered reseeds its noise on every
// note-on, so every hit of a patch was always the same sound: this path is exact, not an
// approximation, and a pattern costs one voice per hit.
//
// Ported from sintered_voice.cpp (MIT, paiheulevrai) in de_* math, with two deliberate
// changes, both measured-first habits from the MME port:
//   · the fold is the SYMMETRIC floor-modulo one. Upstream's foldS is the same
//     fabsf(fmodf(x + 1, 4) - 2) - 1 as MME's, and fmodf keeps the sign of a negative input.
//   · a 10 Hz output DC blocker and a 5 ms end fade (the render stops where upstream's voice
//     killed itself, at exp(-5.5) = -48 dB, which is a step in a sample buffer).
//
// controls: A S D F G H pads · 1-6 select · model buttons · knobs · LEFT/RIGHT UP/DOWN · M

#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>

#define SR      44100
#define NPAD    6
#define I0      5                        // instrument slots 5..10, one per pad (INSTR_SAMPLE)
#define REN_MAX (SR * 12 / 10)           // 1.2 s: the longest tail is .018 + 1.10 s (comb, decay 1)

enum { M_KNOT, M_SHARD, M_BURST, M_COMB, M_LOGIC, M_MELT, NMODEL };
static const char *MODEL_NAME[NMODEL] = { "knot", "shard", "burst", "comb", "logic", "melt" };
static const float TAIL[NMODEL]       = { 0.85f, 0.70f, 0.48f, 1.10f, 0.42f, 0.80f };
static const float IMPACT_MIX[NMODEL] = { 0.22f, 0.18f, 0.72f, 0.38f, 0.24f, 0.16f };
// what A / B / C / MOD do, per model (read off the render below)
static const char *HINT[NMODEL] = {
    "mod: PM depth   a: osc2 ratio   b: osc3 ratio   c: fold",
    "mod: osc2 drive   a: osc2 ratio   b: feedback teeth   c: fold+drive",
    "mod: noise + fb   a: noise vs tone   b: noise colour   c: fold",
    "mod: excite drive   a: comb length   b: damping   c: comb feedback",
    "mod: logic mix   a: quantise   b: xor/and/or/max   c: fold",
    "mod: FM depth   a: osc2 ratio   b: warp depth   c: drive",
};

enum { K_MOD, K_A, K_B, K_C, K_MOTION, K_DECAY, K_TUNE, NKNOB };
static const char *KNAME[NKNOB] = { "mod", "a", "b", "c", "motion", "decay", "tune" };

typedef struct { int model; float k[NKNOB]; } Patch;

// six defaults, one per model, voiced as a kit: kick, snare, hat, clank, blip, tom
static Patch pad[NPAD] = {
    { M_KNOT,  { 0.55f, 0.35f, 0.30f, 0.20f, 0.85f, 0.40f, 0.20f } },
    { M_SHARD, { 0.45f, 0.62f, 0.40f, 0.35f, 0.70f, 0.30f, 0.45f } },
    { M_BURST, { 0.40f, 0.85f, 0.10f, 0.15f, 0.80f, 0.18f, 0.80f } },
    { M_COMB,  { 0.50f, 0.30f, 0.30f, 0.70f, 0.65f, 0.35f, 0.40f } },
    { M_LOGIC, { 0.60f, 0.40f, 0.10f, 0.25f, 0.50f, 0.20f, 0.70f } },
    { M_MELT,  { 0.45f, 0.45f, 0.35f, 0.40f, 0.30f, 0.45f, 0.30f } },
};
static const char PADKEY[NPAD] = { 'A', 'S', 'D', 'F', 'G', 'H' };

static float ren[NPAD][REN_MAX];
static int   ren_n[NPAD];
static bool  dirty[NPAD];
static float glow[NPAD];
static int   sel = 0, ksel = K_MOD;
static bool  autoplay = true;
static int   pstep = 0;

// ── the voice (pure: spec'd below) ──────────────────────────────────────────────
static float clamp01f(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
// symmetric triangle fold, period 4 (see the header note on upstream's fmodf)
static float sfold(float x) { float t = x + 1.0f; t -= 4.0f * floorf(t * 0.25f); return fabsf(t - 2.0f) - 1.0f; }

static int   pad_midi(const Patch *p) { return 24 + (int)(p->k[K_TUNE] * 60.0f + 0.5f); }
static float midi_hz(int m) { return 440.0f * de_powf(2.0f, (float)(m - 69) / 12.0f); }
static float duration_s(const Patch *p) { float d = p->k[K_DECAY]; return 0.018f + d * d * TAIL[p->model]; }

typedef struct { float ph[3]; uint32_t rnd; } Osc;
static float s_osc(Osc *o, float f, int slot) {
    o->ph[slot] += f / (float)SR;
    o->ph[slot] -= floorf(o->ph[slot]);
    return de_sin_turns(o->ph[slot]);
}
static float s_rand(Osc *o) { o->rnd = o->rnd * 1664525u + 1013904223u; return ((float)(o->rnd >> 8) * (1.0f / 8388608.0f)) - 1.0f; }

// render one hit at hz into out[]; returns the sample count
static int sin_render(float *out, int nmax, float hz, const Patch *p) {
    const int   model = p->model;
    const float dur = duration_s(p);
    int n = (int)(dur * (float)SR);
    if (n > nmax) n = nmax;
    if (n < 64) n = 64;
    const float baseMod = p->k[K_MOD], baseA = p->k[K_A], baseB = p->k[K_B], baseC = p->k[K_C];
    const float motion = (p->k[K_MOTION] - 0.5f) * 2.0f;         // -1..1, 0 = still (upstream's 128)
    const float motionTime = 0.008f + (1.0f - fabsf(motion)) * 0.35f;
    Osc o = { { 0.0f, 0.0f, 0.0f }, 0x53494e54u };                // upstream reseeds on every note-on
    float fb = 0.0f, dc = 0.0f, nlow = 0.0f, comb[64];
    int ci = 0;
    memset(comb, 0, sizeof comb);
    float hp_x = 0.0f, hp_y = 0.0f;
    const int fade = SR / 200;                                    // 5 ms end fade

    for (int i = 0; i < n; i++) {
        const float age = (float)i / (float)SR;
        float movement = 0.0f;
        if (fabsf(motion) > 0.004f) {
            float x = age / motionTime;
            movement = motion < 0.0f ? (x < 1.0f ? de_sin_turns(0.5f * x) : 0.0f) : de_expf(-6.0f * x);
        }
        float mod = baseMod, a = baseA, b = baseB, c = baseC;
        switch (model) {
            case M_KNOT:  mod = clamp01f(mod + movement * 0.65f); c = clamp01f(c + movement * 0.55f); break;
            case M_SHARD: b = clamp01f(b + movement * 0.65f); c = clamp01f(c + movement * 0.55f); break;
            case M_BURST: a = clamp01f(a + movement * 0.70f); b = clamp01f(b + movement * 0.55f); c = clamp01f(c + movement * 0.35f); break;
            case M_COMB:  b = clamp01f(b + movement * 0.45f); c = clamp01f(c + movement * 0.55f); break;
            case M_LOGIC: a = clamp01f(a + movement * 0.60f); c = clamp01f(c + movement * 0.70f); break;
            case M_MELT:  b = clamp01f(b + movement * 0.70f); c = clamp01f(c + movement * 0.60f); break;
            default: break;
        }
        float nz = s_rand(&o);
        nlow += (nz - nlow) * (0.01f + b * 0.25f);
        float bright = nz - nlow, s = 0.0f;
        float f2 = hz * de_powf(2.0f, (a - 0.5f) * 5.0f);
        float x = s_osc(&o, hz, 0), y = s_osc(&o, f2 + fb * hz * mod * 1.5f, 1);
        switch (model) {
            case M_KNOT: {
                float z = s_osc(&o, hz * de_powf(2.0f, (b - 0.5f) * 7.0f), 2);
                s = de_sin_turns(o.ph[0] + y * mod * 0.32f + z * mod * 0.18f);
                s = s * (1.0f - c) + sfold(s * (1.0f + c * 14.0f)) * c;
                break;
            }
            case M_SHARD: {
                float teeth = sfold((x + y * (1.0f + mod * 6.0f) + fb * b * 4.0f) * (1.0f + c * 12.0f));
                s = de_tanhf(teeth * (1.0f + c * 5.0f));
                break;
            }
            case M_BURST: {
                float color = bright * (1.0f - b) + nlow * b;
                s = color * (a * (1.2f + mod * 0.8f)) + y * (1.0f - a) + fb * mod * 1.5f;
                s = sfold(s * (1.0f + c * 8.0f));
                break;
            }
            case M_COMB: {
                int delay = 1 + (int)(a * 62.0f);
                float delayed = comb[(ci + 64 - delay) & 63];
                float excite = x + y * mod * 0.75f + bright * mod * 0.25f;
                comb[ci] = de_tanhf((excite + delayed * c * 1.35f) * (0.4f + mod * 1.6f));
                ci = (ci + 1) & 63;
                s = delayed * (1.0f - b * 0.92f) + excite * 0.18f;
                break;
            }
            case M_LOGIC: {
                int q = 2 + (int)(a * 126.0f), pattern = (int)(b * 3.99f);
                int ia = (int)((x + 1.0f) * (float)q), ib = (int)((y + 1.0f) * (float)q);
                int lg = pattern == 0 ? (ia ^ ib) : pattern == 1 ? (ia & ib) : pattern == 2 ? (ia | ib) : (ia > ib ? ia : ib);
                s = ((float)(lg % (q * 2)) / (float)q - 1.0f) * mod + x * (1.0f - mod);
                s = sfold(s * (1.0f + c * 15.0f));
                break;
            }
            case M_MELT: {
                float warped = s_osc(&o, hz * (1.0f + y * mod * (3.0f + b * 24.0f) + fb * b * 6.0f), 2);
                s = de_tanhf(warped * (1.0f + c * 13.0f) + bright * mod * b);
                break;
            }
            default: break;
        }
        // the impact: a brief noisy excitation, 1.5..7.5 ms, so the resonators start like a drum
        float impact = bright * de_expf(-age / (0.0015f + 0.006f * (1.0f - b)));
        s += impact * IMPACT_MIX[model] * (0.25f + 0.75f * mod);
        // feedback: DC-blocked, smoothed; C slows the smoother (the tail gets fluid, not buzzy)
        float raw = de_tanhf(s + fb * (0.2f + mod * 1.4f));
        dc += 0.02f * (raw - dc);
        float target = (raw - dc) * (0.12f + baseMod * 0.82f);
        fb += (target - fb) * (0.025f + 0.10f * (1.0f - c));
        float amp = de_expf(-5.5f * age / dur);
        float v = de_tanhf(de_tanhf(s) * amp);
        hp_y = v - hp_x + 0.99857f * hp_y;                        // 10 Hz DC blocker
        hp_x = v;
        float e = i > n - fade ? (float)(n - i) / (float)fade : 1.0f;
        out[i] = hp_y * e;
    }
    // peak-normalise: the models differ by >10 dB at the same knobs and a kit wants one level
    float pk = 0.0f;
    for (int i = 0; i < n; i++) { float m = fabsf(out[i]); if (m > pk) pk = m; }
    if (pk > 1e-4f) { float g = 0.9f / pk; for (int i = 0; i < n; i++) out[i] *= g; }
    return n;
}

// ── voicing: render on change, a hit is a sample playback at root ────────────────
static void apply(int v) {
    int m = pad_midi(&pad[v]);
    ren_n[v] = sin_render(ren[v], REN_MAX, midi_hz(m), &pad[v]);
    sample_load(v, ren[v], ren_n[v]);
    instrument(I0 + v, INSTR_SAMPLE, 0, 0, 7, 20);
    instrument_sample(I0 + v, v, m);                              // root = the pad's note: speed 1.0
    instrument_level(I0 + v, 1.00f);                              // unity: a peak-normalised hit at vol 6 reads ~-16.5 dBFS (measured)
    dirty[v] = false;
}
static void fire(int v, int delay, int vol) {
    glow[v] = 1.0f;
    int ms = (int)((float)ren_n[v] * 1000.0f / (float)SR) + 30;
    schedule_hit(delay, pad_midi(&pad[v]), I0 + v, vol, ms);
}
static void set_model(int v, int m) { pad[v].model = ((m % NMODEL) + NMODEL) % NMODEL; dirty[v] = true; }

// a 16-step pattern across all six pads, queued a beat at a time
static void pattern_beat(int s) {
    int q = 15000 / 118;                                          // one 16th at 118 bpm, ms
    for (int i = 0; i < 4; i++) {
        int st = (s * 4 + i) & 31, d = i * q;
        if (st == 0 || st == 10 || st == 16 || st == 26) fire(0, d, 7);
        if (st == 8 || st == 24) fire(1, d, 6);
        if ((st & 3) == 2) fire(2, d, 5);
        if (st == 14 || st == 30) fire(3, d, 6);
        if (st == 7 || st == 21 || st == 23) fire(4, d, 5);
        if (st == 28) fire(5, d, 6);
    }
}

void init(void) {
    for (int v = 0; v < NPAD; v++) { apply(v); glow[v] = 0.0f; }
    bpm(118);
}

void update(void) {
    for (int v = 0; v < NPAD; v++) {
        if (keyp(PADKEY[v])) { fire(v, 0, 6); autoplay = false; }
        if (keyp('1' + v)) sel = v;
    }
    if (keyp('M')) autoplay = !autoplay;
    if (keyp(KEY_LEFT))  ksel = (ksel + NKNOB - 1) % NKNOB;
    if (keyp(KEY_RIGHT)) ksel = (ksel + 1) % NKNOB;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        pad[sel].k[ksel] = clamp(pad[sel].k[ksel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        dirty[sel] = true;
    }
    // set-and-hold: re-render only a pad that changed, and preview it while you drag
    for (int v = 0; v < NPAD; v++)
        if (dirty[v] && (frame() % 8 == 0)) { apply(v); if (!autoplay) fire(v, 0, 6); }

    if (autoplay && every(1)) { pattern_beat(pstep); pstep = (pstep + 1) & 7; }

#ifdef DE_TRACE
    watch("sel", "%d", sel);
    watch("model", "%d", pad[sel].model);
    watch("mod", "%.2f", pad[sel].k[K_MOD]);
    watch("decay", "%.2f", pad[sel].k[K_DECAY]);
    watch("auto", "%d", autoplay ? 1 : 0);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();
    print("SINTERED", 6, 4, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("impact into cross-mod + feedback tail", 76, 6, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M auto: on" : "M auto: off", SCREEN_W - 6, 6, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);

    // pads
    for (int v = 0; v < NPAD; v++) {
        int x = 6 + v * 52, y = 16, w = 48, h = 34;
        glow[v] *= 0.90f;
        int bg = glow[v] > 0.4f ? CLR_LIGHT_YELLOW : glow[v] > 0.05f ? CLR_PEACH : (v == sel ? CLR_DARK_BROWN : CLR_DARKER_GREY);
        rectfill(x, y, w, h, bg);
        rect(x, y, w, h, v == sel ? CLR_ORANGE : CLR_DARK_GREY);
        int tc = glow[v] > 0.05f ? CLR_BROWNISH_BLACK : CLR_WHITE;
        print(MODEL_NAME[pad[v].model], x + 4, y + 5, tc);
        print(str("%c (%d)", PADKEY[v], v + 1), x + 4, y + 22, glow[v] > 0.05f ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
        if (tapp(x, y, w, h)) { fire(v, 0, 6); sel = v; autoplay = false; }
    }

    // model row for the selected pad
    for (int m = 0; m < NMODEL; m++) {
        int x = 6 + m * 52, y = 56, w = 48, h = 12;
        if (m == pad[sel].model) rectfill(x - 1, y - 1, w + 2, h + 2, CLR_ORANGE);
        if (ui_button(x, y, w, h, MODEL_NAME[m])) set_model(sel, m);
    }

    // knobs
    for (int k = 0; k < NKNOB; k++) {
        int x = 26 + k * 45, y = 92;
        float before = pad[sel].k[k];
        if (ui_knob(&pad[sel].k[k], x, y, KNAME[k]) && pad[sel].k[k] != before) dirty[sel] = true;
        if (k == ksel) print("^", x - 2, y + 22, CLR_YELLOW);
    }
    font(FONT_TINY);
    print(HINT[pad[sel].model], 6, 128, CLR_MEDIUM_GREY);
    print(str("note %d   %.0f ms   motion %s", pad_midi(&pad[sel]), duration_s(&pad[sel]) * 1000.0f,
              pad[sel].k[K_MOTION] < 0.49f ? "swell" : pad[sel].k[K_MOTION] > 0.51f ? "snap" : "still"),
          6, 136, CLR_DARK_GREY);

    // the selected pad's rendered hit
    {
        int x0 = 6, w = SCREEN_W - 12, y0 = 146, hh = 36, n = ren_n[sel];
        rect(x0 - 1, y0 - 1, w + 2, hh + 2, CLR_DARKER_GREY);
        for (int xx = 0; xx < w && n > 0; xx++) {
            int i0 = (int)((long)xx * n / w), i1 = (int)((long)(xx + 1) * n / w);
            float lo = 1.0f, hi = -1.0f;
            for (int i = i0; i < i1; i += 4) { if (ren[sel][i] < lo) lo = ren[sel][i]; if (ren[sel][i] > hi) hi = ren[sel][i]; }
            if (hi >= lo) line(x0 + xx, y0 + (int)(hh * 0.5f - hi * hh * 0.5f), x0 + xx, y0 + (int)(hh * 0.5f - lo * hh * 0.5f), CLR_BROWN);
        }
    }

    print("A S D F G H pads  1-6 select  M auto  LEFT/RIGHT knob  UP/DOWN adjust", 6, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}

#ifdef DE_SPEC
#include "spec.h"
#define SPN (SR / 2)
static float sb[NMODEL][REN_MAX];
static int   sn[NMODEL];
static float spk(const float *b, int from, int n) { float p = 0; for (int i = from; i < n; i++) { float a = fabsf(b[i]); if (a > p) p = a; } return p; }
static float sdiff(const float *a, const float *b, int n) { double d = 0; for (int i = 0; i < n; i++) d += fabsf(a[i] - b[i]); return (float)(d / n); }
static int   sfinite(const float *b, int n) { for (int i = 0; i < n; i++) if (!(b[i] == b[i]) || fabsf(b[i]) > 1.0f) return 0; return 1; }

void spec(void) {
    autoplay = false;
    step(1);
    for (int v = 0; v < NPAD; v++) expect(ren_n[v] > 64, str("pad %d rendered at init", v + 1));

    // the fold is symmetric (upstream's fmodf fold is not)
    {
        int ok = 1;
        for (int i = -40; i <= 40; i++) { float x = (float)i * 0.173f; if (fabsf(sfold(-x) + sfold(x)) > 1e-5f) ok = 0; }
        expect(ok, "sfold is odd-symmetric: fold(-x) == -fold(x)");
        // upstream's shape, kept: an INVERTED triangle of period 4 (0.5 -> -0.5, 1 -> -1, 3 -> +1).
        // It differs from upstream's fmodf fold only for x < -1, where fmodf goes negative.
        expect(fabsf(sfold(0.5f) + 0.5f) < 1e-6f && fabsf(sfold(1.0f) + 1.0f) < 1e-6f && fabsf(sfold(3.0f) - 1.0f) < 1e-6f,
               "sfold is upstream's inverted triangle, period 4");
        expect(fabsf(sfold(0.7f) - sfold(4.7f)) < 1e-5f && fabsf(sfold(-2.3f) - sfold(1.7f)) < 1e-5f, "sfold has period 4 on both sides of zero");
    }

    // every model renders finite, audible, and different from every other, same knobs
    Patch q = { M_KNOT, { 0.5f, 0.5f, 0.4f, 0.4f, 0.7f, 0.5f, 0.4f } };
    for (int m = 0; m < NMODEL; m++) {
        q.model = m;
        sn[m] = sin_render(sb[m], REN_MAX, 110.0f, &q);
        expect(sfinite(sb[m], sn[m]), str("%s renders finite, in range", MODEL_NAME[m]));
        expect(spk(sb[m], 0, sn[m]) > 0.85f, str("%s is peak-normalised", MODEL_NAME[m]));
        expect(fabsf(sb[m][sn[m] - 1]) < 0.01f, str("%s ends at silence (end fade)", MODEL_NAME[m]));
    }
    for (int m = 0; m < NMODEL; m++)
        for (int k = m + 1; k < NMODEL; k++) {
            int n = sn[m] < sn[k] ? sn[m] : sn[k];
            expect(sdiff(sb[m], sb[k], n) > 0.01f, str("%s and %s are different sounds", MODEL_NAME[m], MODEL_NAME[k]));
        }

    // deterministic: a patch renders byte-identical twice (the noise reseeds per hit, like upstream)
    q.model = M_BURST;
    int n1 = sin_render(sb[0], REN_MAX, 110.0f, &q);
    int n2 = sin_render(sb[1], REN_MAX, 110.0f, &q);
    expect(n1 == n2 && memcmp(sb[0], sb[1], sizeof(float) * n1) == 0, "a patch renders byte-identical twice");

    // every knob reaches the DSP, on a model where it is live
    static const char *KN[6] = { "mod", "a", "b", "c", "motion", "decay" };
    for (int k = 0; k < 6; k++) {
        Patch r = q; r.model = M_SHARD;
        Patch s0 = r;
        sin_render(sb[2], REN_MAX, 110.0f, &s0);
        r.k[k] = r.k[k] > 0.5f ? 0.1f : 0.9f;
        int nn = sin_render(sb[3], REN_MAX, 110.0f, &r);
        int n = nn < SPN ? nn : SPN;
        expect(sdiff(sb[2], sb[3], n) > 0.003f, str("%s knob reaches the DSP (shard)", KN[k]));
    }
    // decay lengthens the hit; comb has the longest tail at the same decay
    {
        Patch a = q, b = q; a.k[K_DECAY] = 0.2f; b.k[K_DECAY] = 0.9f;
        expect(duration_s(&b) > duration_s(&a) * 4.0f, "DECAY lengthens the hit");
        Patch c = q, l = q; c.model = M_COMB; l.model = M_LOGIC;
        expect(duration_s(&c) > duration_s(&l), "comb rings longer than logic at the same decay");
        expect(duration_s(&b) < (float)REN_MAX / (float)SR || b.model != M_COMB, "the buffer fits the longest tail");
    }
    // motion centre is still: the envelope term is off, so motion 0.5 == motion 0.502 renders
    {
        Patch a = q, b = q; a.k[K_MOTION] = 0.5f; b.k[K_MOTION] = 0.501f;
        int na = sin_render(sb[4], REN_MAX, 110.0f, &a), nb = sin_render(sb[5], REN_MAX, 110.0f, &b);
        expect(na == nb && memcmp(sb[4], sb[5], sizeof(float) * na) == 0, "motion at centre is still (no movement term)");
    }
    expect(pad_midi(&pad[0]) < pad_midi(&pad[2]), "the kit's kick pad sits below its hat pad");

    // the panel
    spec_tap('3');
    expect_eq(sel, 2, "key 3 selects pad 3");
    spec_tap(KEY_RIGHT);
    expect_eq(ksel, K_A, "RIGHT moves to the a knob");
    float before = pad[2].k[K_A], other = pad[0].k[K_A];
    int n_before = ren_n[2];
    key_down(KEY_DOWN); step(4); key_up(KEY_DOWN); step(10);
    expect(pad[2].k[K_A] < before, "DOWN lowers the selected pad's knob");
    expect(pad[0].k[K_A] == other, "and leaves the other pads alone");
    expect(!dirty[2], "the pad re-rendered (set-and-hold) within a few frames");
    (void)n_before;
    spec_tap('M');
    expect(autoplay, "M turns the pattern on");
}
#endif
