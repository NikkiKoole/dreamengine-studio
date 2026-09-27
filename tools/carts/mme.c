/* de:meta
{
  "slug": "mme",
  "title": "mme",
  "status": "active",
  "created": "2026-09-27",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "wavefolder",
    "vocoder"
  ],
  "lineage": "Cart-first prototype of the #1 row in docs/design/choochootracker-borrow-list.md: Choochootracker's MME voice (chipnomad_lib/synth/mme_voice.cpp, MIT, itself a tracker-sized take on Mutable Warps' ring/fold/XOR/vocoder algorithms after Noise Engineering's Loquelic Iteritas). The engine has no per-sample hook for cart code, so each key press RENDERS the note in cart-land C and hands it to a PCM slot (sample_load + instrument_sample at root = the pressed note, so nothing is repitched). That is the prototype's trick, not its sound: once the mapping settles this becomes an INSTR_* engine and the cart keeps its panel.",
  "homage": "Noise Engineering Loquelic Iteritas (the two-osc cross-modulation voice) by way of Mutable Instruments Warps (the diode ring mod + the filter-bank vocoder).",
  "description": {
    "summary": "Two oscillators fighting through seven cross-modulation models: diode ring, fold, cross, VPM, sync, XOR logic, vocoder. Aggressive by design.",
    "detail": "A prototype of the MME (Multi Modulation Engine) voice, played from a keybed. Two oscillators (five wave pairs, B tuned up to two octaves off A) run through one of seven models, then a saturate-into-fold shaper, then a feedback path that reinjects only the AC part of the output (a DC-blocked feedback state), so the wild settings stay turbulent instead of collapsing to silence or a flat line. Every key press renders the note in cart-land C into a PCM slot the sample engine plays back at root pitch: the sound is exact, the knobs re-render the next note rather than riding the held one. Autoplay walks a bass line while stepping the models so the panel is never silent.",
    "controls": "A-K / W-P / click / touch / MIDI: keybed (Z/X octave) · 1-7 or LEFT/RIGHT: model · UP/DOWN: wave pair · drag the knobs (amount / flow / feedback / shaper / interval), wheel = fine · M: autoplay"
  },
  "todo": [
    "ear-settle the macro mapping for the engine port (proposed: harmonics = model, timbre = amount, morph = flow, feedback on a MODE_ aux)",
    "per-note peak normalisation hides how much louder ring is than vocode; the engine port needs a per-model trim table instead",
    "a held note cannot ride a knob mid-note (sample-slot prototype); the engine port fixes that"
  ]
}
de:meta */
// mme — the Choochootracker MME voice, prototyped as a cart before it becomes an engine.
//
// Why a cart: the engine has no per-sample hook for cart code, but a cart CAN render its
// own audio and hand it to a PCM slot. So a key press renders REN_SEC of the voice at the
// pressed pitch into ren[], sample_load()s it into one of NV slots (round-robin), binds
// the matching INSTR_SAMPLE instrument at root = that note (speed 1.0, no resampling) and
// note_on()s it. The engine's ADSR still gates the release. Knob changes re-render the
// NEXT note (and fire a short preview strike while you drag), they do not ride a held one.
//
// The DSP is a straight port of mme_voice.cpp (MIT, paiheulevrai), in de_* math so a
// spec() render is the same bits on every platform:
//   two oscs (pair table below, B = A * 2^(±2 oct)) → model → shaper (tanh into a fold
//   past 55%) → feedback: raw = tanh(fb·(.1+g) + out·(.45+1.9·fc)), DC-blocked, back in.
//
// Measured while porting (2026-09-27), so nobody re-derives them:
//   · upstream's fold uses fmodf, which keeps the sign of a negative input; on the sq/sq pair
//     the fold model rendered a FLAT LINE at any feedback (ac rms 0.003). mme_fold() below is
//     the symmetric floor-modulo fold. Worth sending upstream.
//   · a folder at full feedback carries ~0.2 of DC (the feedback state is DC-blocked, the
//     output is not), so the render ends in a 10 Hz DC blocker.
//   · click-check flags the cross/vpm models (64 events on a 4 s keybed take) but the largest
//     sample step is 0.057 inside a smooth slope, and ring has no step above 0.02: that is
//     the tool's documented false-positive shape (a steep slope against a quiet step-rms),
//     not a splice. Trust the step dump, not the count.
//   · a single voice plays at -12 dBFS peak, in family with the engine's own baseline (-14).
//
// controls: keybed (A-K whites, W-P blacks, Z/X octave) · 1-7 / LEFT RIGHT model ·
//           UP DOWN wave pair · knobs (drag, wheel) · M autoplay

#include "studio.h"
#include "ui.h"
#include "keybed.h"
#include <math.h>
#include <string.h>

#define I0       5                    // instrument slots I0..I0+NV-1, one per PCM slot
#define NV       6                    // polyphony = PCM slots used (engine has 8)
#define SR       44100                // SOUND_SAMPLE_RATE — the rate sample_load() assumes
#define REN_SEC  2.0f
#define REN_N    (SR * 2)                // = SR * REN_SEC, kept an integer constant expression
#define NBAND    20

enum { M_RING, M_FOLD, M_CROSS, M_VPM, M_SYNC, M_LOGIC, M_VOCODE, NMODEL };
static const char *MODEL_NAME[NMODEL] = { "ring", "fold", "cross", "vpm", "sync", "logic", "vocode" };
#define NPAIR 5
static const char *PAIR_NAME[NPAIR] = { "sin/sin", "tri/sin", "saw/tri", "sin/sq", "sq/sq" };
static const int PAIR_A[NPAIR] = { 0, 1, 2, 0, 3 };   // wave shapes: 0 sin 1 tri 2 saw 3 square
static const int PAIR_B[NPAIR] = { 0, 0, 1, 2, 3 };

typedef struct {
    int   model, pair;
    float interval;     // 0..1, 0.5 = unison, ±2 octaves
    float amount, flow, feedback, shaper;
} MmeParams;

static MmeParams P = { M_RING, 0, 0.5f, 0.55f, 0.5f, 0.25f, 0.0f };

static float ren[REN_N];
static int   handle[128];                  // keybed notes: one live voice per MIDI note
static int   vhandle[NV];                  // the voice on each PCM slot (the walk repeats notes,
static int   rr = 0;                       // round-robin PCM slot                 so it keys by SLOT)
static bool  autoplay = true;
static int   apos = 0;
static int   preview_left = 0;             // frames until the preview note releases
static int   preview_v = -1;               // its PCM slot
static bool  dirty = false;                // a knob moved since the last preview strike
static int   auto_left[NV], auto_v[NV];    // autoplay note-offs, by slot
static int   last_render_n = 0;

// ── the voice ──────────────────────────────────────────────────────────────────
static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static float wrapf(float x) { return x - floorf(x); }

static float mme_osc(float phase, int shape) {
    phase = wrapf(phase);
    switch (shape) {
        case 1: return 1.0f - 4.0f * fabsf(phase - 0.5f);
        case 2: return 2.0f * phase - 1.0f;
        case 3: return phase < 0.5f ? 1.0f : -1.0f;
        case 4: { float s = de_sin_turns(phase); return s * s; }
        default: return de_sin_turns(phase);
    }
}

// Warps' diode ring-modulator model (MIT): a dead zone, then a square law.
static float mme_diode(float x) {
    float sign = x > 0.0f ? 1.0f : -1.0f;
    float dead = fabsf(x) - 0.667f;
    dead += fabsf(dead);
    return 0.043247658f * dead * dead * sign;
}

// Triangle wavefolder, period 4, symmetric. Upstream writes fabsf(fmodf(x + 1, 4) - 2) - 1,
// and C's fmodf keeps the SIGN of a negative input, so every negative lobe folded to a
// large positive value and the shaper then pinned both halves of a square pair to the
// same rail: the fold model at the sq/sq pair rendered a flat line (measured: ac rms
// 0.003 at any feedback). A floor-based modulo is the fold that was meant.
static float mme_fold(float x) {
    float t = x + 1.0f;
    t -= 4.0f * floorf(t * 0.25f);
    return fabsf(t - 2.0f) - 1.0f;
}

static float mme_shaper(float x, float amount) {
    if (amount <= 0.0f) return x;
    float sat  = de_tanhf(x * (1.0f + amount * 5.0f));
    float fold = mme_fold(sat * (1.0f + amount * 3.0f));
    float mix  = clampf((amount - 0.55f) / 0.45f, 0.0f, 1.0f);
    return sat + (fold - sat) * mix;
}

// 20-band envelope vocoder after Warps' filter-bank one: each band is the difference of
// two cascaded one-poles (a cheap bandpass), the modulator band's envelope gates the
// carrier band. Coefficients depend on flow + amount only, so they are hoisted per render.
typedef struct {
    float mlo[NBAND], mhi[NBAND], clo[NBAND], chi[NBAND], env[NBAND], a[NBAND];
    float release;
} Voc;

static void voc_init(Voc *v, float flow, float amount) {
    memset(v, 0, sizeof *v);
    for (int i = 0; i < NBAND; i++) {
        float t  = (float)i / (float)(NBAND - 1);
        float u  = clampf(t + (flow - 0.5f) * 0.26f, 0.0f, 1.0f);
        float hz = 90.0f * de_powf(42.0f, u);
        v->a[i]  = clampf(6.2831853f * hz / (float)SR, 0.0001f, 0.45f);
    }
    v->release = 0.003f + (1.0f - amount) * 0.08f;
}

static float voc_run(Voc *v, float mod, float car) {
    float out = 0.0f;
    for (int i = 0; i < NBAND; i++) {
        float a = v->a[i];
        v->mlo[i] += a * (mod - v->mlo[i]);
        v->mhi[i] += a * (v->mlo[i] - v->mhi[i]);
        v->clo[i] += a * (car - v->clo[i]);
        v->chi[i] += a * (v->clo[i] - v->chi[i]);
        float env  = fabsf(v->mlo[i] - v->mhi[i]);
        float rate = env > v->env[i] ? 0.18f : v->release;
        v->env[i] += rate * (env - v->env[i]);
        out += (v->clo[i] - v->chi[i]) * clampf(v->env[i] * 6.0f, 0.0f, 2.0f);
    }
    return out * 0.18f;
}

// Render n samples of the voice at hz into out[]: 4 ms attack, hold, 200 ms fade at the
// end so a one-shot never clicks off. The engine's instrument ADSR gates the release.
static void mme_render(float *out, int n, float hz, const MmeParams *p) {
    const float ratio = de_powf(2.0f, (p->interval - 0.5f) * 4.0f);
    const int   sa = PAIR_A[p->pair], sb = PAIR_B[p->pair];
    const float amount = p->amount, flow = p->flow, fc = p->feedback;
    const float fbg = fc * fc * 2.5f;                  // civil below 2/3, ugly past it
    const float stepA = hz / (float)SR, stepB = hz * ratio / (float)SR;
    const int   atk = SR * 4 / 1000, tail = SR / 5;
    float phA = 0.0f, phB = 0.0f, fb = 0.0f, fbdc = 0.0f;
    float hp_x = 0.0f, hp_y = 0.0f;                    // 10 Hz output DC blocker (a folder at full
    Voc voc;                                           // feedback carries ~0.2 of DC; the sample engine would thump it)
    if (p->model == M_VOCODE) voc_init(&voc, flow, amount);

    for (int i = 0; i < n; i++) {
        if (p->model == M_SYNC && phA + stepA >= 1.0f)
            phB += (flow - phB) * amount;                // amount: free-running → hard reset at phase = flow
        const float inj = fb * fbg;
        float a = de_tanhf(mme_osc(phA, sa) + inj * (0.35f + 0.75f * flow));
        float b = de_tanhf(mme_osc(phB, sb) - inj * (1.10f - 0.50f * flow));
        float s = 0.0f;
        switch (p->model) {
            case M_RING: {
                float analog  = de_tanhf((mme_diode(a + b * amount * 2.0f) + mme_diode(a - b * amount * 2.0f)) * 12.0f);
                float digital = 4.0f * a * b * amount;
                digital /= 1.0f + fabsf(digital);
                s = analog + (digital - analog) * flow;
                break;
            }
            case M_FOLD: {
                float sum = (a + b * (0.15f + amount) + a * b * 0.25f) * (0.02f + amount * 1.4f);
                s = mme_fold(sum);
                break;
            }
            case M_CROSS: {
                float ab = mme_osc(phA + (b + fb) * amount * 0.28f, sa);
                float ba = mme_osc(phB + (a + fb) * amount * 0.28f, sb);
                s = ab + (ba - ab) * flow;
                break;
            }
            case M_VPM: {
                float ab = mme_osc(phA + (b + fb) * amount * 0.45f, sa);
                float ba = mme_osc(phB + (a + fb) * amount * 0.45f, sb);
                s = ab + (ba - ab) * flow;
                break;
            }
            case M_SYNC:
                s = mme_osc(phB, sb) + a * (amount * 0.18f);
                break;
            case M_LOGIC: {
                int ia = (int)(a * 32767.0f), ib = (int)(b * 32767.0f);
                float x = (float)((short)ia ^ (short)ib) / 32768.0f;
                float cmp = fabsf(a) > fabsf(b) ? a : b;
                s = (a + b) * (1.0f - amount) * 0.5f + (x + (cmp - x) * flow) * amount;
                break;
            }
            case M_VOCODE:
                s = voc_run(&voc, b * amount, a);
                break;
            default: break;
        }
        s = de_tanhf(s + inj * 1.2f);
        s = mme_shaper(s, p->shaper);
        // feedback: keep the turbulence, reinject only the AC part (a recursive DC offset
        // turns a wild patch into silence or a flat line)
        float raw = de_tanhf(fb * (0.10f + fbg) + s * (0.45f + fc * 1.9f));
        fbdc += 0.025f * (raw - fbdc);
        fb = (raw - fbdc) * (0.72f + fc * 0.22f);

        hp_y = s - hp_x + 0.99857f * hp_y;             // 1 - 2π·10/44100
        hp_x = s;

        float env = 1.0f;
        if (i < atk) env = (float)i / (float)atk;
        if (i > n - tail) env *= (float)(n - i) / (float)tail;
        out[i] = hp_y * env;

        phA = wrapf(phA + stepA);
        phB = wrapf(phB + stepB);
    }
    // peak-normalise to 0.9 like record_grab() does: the models differ by ~15 dB at the same
    // knobs (vocode's 0.18 sum vs ring's tanh rails) and a keyboard wants one level
    float pk = 0.0f;
    for (int i = 0; i < n; i++) { float a = fabsf(out[i]); if (a > pk) pk = a; }
    if (pk > 1e-4f) { float g = 0.9f / pk; for (int i = 0; i < n; i++) out[i] *= g; }
}

// ── voicing: one render per press, into a round-robin PCM slot ─────────────────
static float midi_hz(int midi) { return 440.0f * de_powf(2.0f, (float)(midi - 69) / 12.0f); }

// Render + load + start one voice; returns the PCM slot it took. The slot's previous
// voice is released first so a reloaded buffer is never read by a live voice.
static int play(int midi, int vel) {
    if (midi < 0 || midi > 127) return -1;
    int v = rr; rr = (rr + 1) % NV;
    if (vhandle[v] >= 0) { note_off(vhandle[v]); vhandle[v] = -1; }
    mme_render(ren, REN_N, midi_hz(midi), &P);
    last_render_n = REN_N;
    sample_load(v, ren, REN_N);
    instrument_sample(I0 + v, v, midi);            // root = the note: speed 1.0, no resampling
    vhandle[v] = note_on(midi, I0 + v, vel);
    kb_glow[midi] = 1.0f;                          // light the key for cart-fired notes too (same TU as keybed.h)
    return v;
}
static void release_slot(int v) {
    if (v < 0 || v >= NV || vhandle[v] < 0) return;
    note_off(vhandle[v]);
    vhandle[v] = -1;
}

// keybed: one voice per MIDI note (keybed.h refcounts sources, so a note is pressed once)
static void on_note(int midi, int vel) {
    autoplay = false;
    if (handle[midi] >= 0) release_slot(handle[midi]);
    handle[midi] = play(midi, vel);
}
static void on_off(int midi) {
    if (midi < 0 || midi > 127 || handle[midi] < 0) return;
    release_slot(handle[midi]);
    handle[midi] = -1;
}

static void preview(void) {
    if (preview_v >= 0) release_slot(preview_v);
    preview_v = play(57, 5);
    preview_left = 18;
}

static void set_model(int m) {
    P.model = ((m % NMODEL) + NMODEL) % NMODEL;
    dirty = true;
}
static void set_pair(int p) {
    P.pair = ((p % NPAIR) + NPAIR) % NPAIR;
    dirty = true;
}

// The autoplay walk: a bass line, each bar a new model + wave pair, amount/flow/feedback
// swept across the bar so the thumbnail and a --wav render cover the whole panel.
static void walk_step(void) {
    static const int walk[8] = { 45, 45, 52, 48, 45, 57, 55, 52 };
    int i = apos & 7;
    if (i == 0) { P.model = (apos / 8) % NMODEL; P.pair = (apos / 8) % NPAIR; }
    float u = (float)i / 7.0f;
    P.amount   = 0.25f + 0.65f * u;
    P.flow     = 0.15f + 0.70f * (1.0f - u);
    P.feedback = 0.15f + 0.45f * u;
    int k = apos % NV;
    if (auto_v[k] >= 0) release_slot(auto_v[k]);
    auto_v[k] = play(walk[i], 6); auto_left[k] = 28;
    apos++;
}

void init(void) {
    for (int v = 0; v < NV; v++) {
        instrument(I0 + v, INSTR_SAMPLE, 3, 0, 7, 220);   // 3 ms declick, release gates the tail
        auto_left[v] = 0; auto_v[v] = -1; vhandle[v] = -1;
    }
    for (int m = 0; m < 128; m++) handle[m] = -1;
    keybed_config(I0, 3, 14);                              // slot unused (managed off), C3, 2 octaves
    keybed_layout(0, 104, SCREEN_W, SCREEN_H - 104);
    keybed_manage_voices(false);
    keybed_on_note(on_note);
    keybed_on_off(on_off);
    bpm(100);
}

void update(void) {
    for (int m = 0; m < NMODEL; m++) if (keyp('1' + m)) set_model(m);
    if (keyp(KEY_LEFT))  set_model(P.model - 1);
    if (keyp(KEY_RIGHT)) set_model(P.model + 1);
    if (keyp(KEY_UP))    set_pair(P.pair + 1);
    if (keyp(KEY_DOWN))  set_pair(P.pair - 1);
    if (keyp('M')) autoplay = !autoplay;

    keybed_update();

    if (dirty && !autoplay && frame() % 10 == 0) { dirty = false; preview(); }
    if (preview_left > 0 && --preview_left == 0) { release_slot(preview_v); preview_v = -1; }

    for (int k = 0; k < NV; k++)
        if (auto_left[k] > 0 && --auto_left[k] == 0) { release_slot(auto_v[k]); auto_v[k] = -1; }

    if (autoplay && every(1)) walk_step();

#ifdef DE_TRACE
    watch("model", "%d", P.model);
    watch("pair", "%d", P.pair);
    watch("amount", "%.2f", P.amount);
    watch("flow", "%.2f", P.flow);
    watch("fb", "%.2f", P.feedback);
    watch("shaper", "%.2f", P.shaper);
    watch("auto", "%d", autoplay ? 1 : 0);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();

    print("MME", 6, 4, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("multi modulation engine", 38, 6, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M auto: walk" : "M auto: off", SCREEN_W - 6, 6,
                autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);

    // model row
    for (int m = 0; m < NMODEL; m++) {
        int x = 4 + m * 45, y = 16, w = 42, h = 13;
        if (m == P.model) { rectfill(x - 1, y - 1, w + 2, h + 2, CLR_ORANGE); }
        if (ui_button(x, y, w, h, MODEL_NAME[m])) set_model(m);
    }
    // wave pair row
    for (int p = 0; p < NPAIR; p++) {
        int x = 4 + p * 63, y = 33, w = 60, h = 11;
        if (p == P.pair) { rectfill(x - 1, y - 1, w + 2, h + 2, CLR_PEACH); }
        if (ui_button(x, y, w, h, PAIR_NAME[p])) set_pair(p);
    }

    // knobs
    int ky = 66;
    if (ui_knob(&P.amount,   34,  ky, "amount"))   dirty = true;
    if (ui_knob(&P.flow,     97,  ky, "flow"))     dirty = true;
    if (ui_knob(&P.feedback, 160, ky, "feedback")) dirty = true;
    if (ui_knob(&P.shaper,   223, ky, "shaper"))   dirty = true;
    if (ui_knob(&P.interval, 286, ky, "interval")) dirty = true;
    font(FONT_TINY);
    {
        float semis = (P.interval - 0.5f) * 48.0f;
        print(str("B %+.0f st", semis), 262, ky + 22, CLR_DARK_GREY);
    }
    print("1-7 model  UP/DN pair  Z/X oct", 4, 96, CLR_DARK_GREY);

    // last render, as a strip behind the keybed edge
    if (last_render_n > 0) {
        int x0 = 190, w = 124, y0 = 91, hh = 10;
        for (int x = 0; x < w; x++) {
            int i0 = (int)((long)x * last_render_n / w), i1 = (int)((long)(x + 1) * last_render_n / w);
            float lo = 1.0f, hi = -1.0f;
            for (int i = i0; i < i1 && i < last_render_n; i += 8) { if (ren[i] < lo) lo = ren[i]; if (ren[i] > hi) hi = ren[i]; }
            if (hi >= lo) line(x0 + x, y0 + (int)(hh * 0.5f - hi * hh * 0.5f), x0 + x, y0 + (int)(hh * 0.5f - lo * hh * 0.5f), CLR_DARK_BROWN);
        }
    }

    font(FONT_NORMAL);
    keybed_draw();
    ui_end();
}

#ifdef DE_SPEC
#include "spec.h"
#define SPEC_N (SR / 4)
static float spec_buf[NMODEL][SPEC_N];
static float spec_peak(const float *b, int n) { float p = 0; for (int i = 0; i < n; i++) { float a = fabsf(b[i]); if (a > p) p = a; } return p; }
static float spec_diff(const float *a, const float *b, int n) { float d = 0; for (int i = 0; i < n; i++) d += fabsf(a[i] - b[i]); return d / n; }
static int   spec_finite(const float *b, int n) { for (int i = 0; i < n; i++) if (!(b[i] == b[i]) || fabsf(b[i]) > 4.0f) return 0; return 1; }
static float spec_mean(const float *b, int from, int n) { double s = 0; for (int i = from; i < n; i++) s += b[i]; return (float)(s / (n - from)); }

void spec(void) {
    autoplay = false;
    step(1);
    expect_eq(P.model, M_RING, "boots on the ring model");

    // every model renders: finite, audible, and a different sound from every other model
    MmeParams q = { M_RING, 2, 0.5f, 0.6f, 0.5f, 0.3f, 0.2f };
    for (int m = 0; m < NMODEL; m++) {
        q.model = m;
        mme_render(spec_buf[m], SPEC_N, 110.0f, &q);
        expect(spec_finite(spec_buf[m], SPEC_N), str("%s renders finite samples", MODEL_NAME[m]));
        expect(spec_peak(spec_buf[m] + SR / 50, SPEC_N - SR / 50) > 0.05f, str("%s is audible (peak > 0.05)", MODEL_NAME[m]));
    }
    for (int m = 0; m < NMODEL; m++)
        for (int k = m + 1; k < NMODEL; k++)
            expect(spec_diff(spec_buf[m], spec_buf[k], SPEC_N) > 0.01f,
                   str("%s and %s are different sounds", MODEL_NAME[m], MODEL_NAME[k]));

    // the same patch twice is the same bits (de_* math; the render is deterministic)
    q.model = M_RING;
    mme_render(spec_buf[0], SPEC_N, 110.0f, &q);
    mme_render(spec_buf[1], SPEC_N, 110.0f, &q);
    expect(memcmp(spec_buf[0], spec_buf[1], sizeof spec_buf[0]) == 0, "a patch renders byte-identical twice");

    // knobs reach the DSP: amount / flow / feedback / shaper / interval each change the render
    static const char *KN[5] = { "amount", "flow", "feedback", "shaper", "interval" };
    for (int k = 0; k < 5; k++) {
        MmeParams r = q;
        float *f = k == 0 ? &r.amount : k == 1 ? &r.flow : k == 2 ? &r.feedback : k == 3 ? &r.shaper : &r.interval;
        *f = 0.9f;
        mme_render(spec_buf[1], SPEC_N, 110.0f, &r);
        expect(spec_diff(spec_buf[0], spec_buf[1], SPEC_N) > 0.005f, str("%s knob reaches the DSP", KN[k]));
    }

    // full feedback on the fold model stays turbulent, not DC: the AC-only reinjection
    MmeParams w = { M_FOLD, 4, 0.5f, 1.0f, 0.5f, 1.0f, 0.8f };
    mme_render(spec_buf[2], SPEC_N, 110.0f, &w);
    expect(spec_finite(spec_buf[2], SPEC_N), "max feedback fold is finite");
    {
        float mean = spec_mean(spec_buf[2], SPEC_N / 2, SPEC_N);
        double ac = 0; for (int i = SPEC_N / 2; i < SPEC_N; i++) { float d = spec_buf[2][i] - mean; ac += d * d; }
        ac = sqrt(ac / (SPEC_N / 2));
        expect(ac > 0.05, str("max feedback stays turbulent, not a flat line (ac rms %.3f)", ac));
        expect(fabsf(mean) < 0.02f, str("the output DC blocker holds (mean %.3f)", mean));
    }
    expect(spec_peak(spec_buf[2] + SPEC_N / 2, SPEC_N / 2) > 0.05f, "max feedback does not collapse to silence");

    // sync at amount 0 is B free-running: identical to sync at amount 0 with a different flow
    MmeParams s0 = { M_SYNC, 0, 0.7f, 0.0f, 0.2f, 0.0f, 0.0f }, s1 = s0; s1.flow = 0.8f;
    mme_render(spec_buf[3], SPEC_N, 110.0f, &s0);
    mme_render(spec_buf[4], SPEC_N, 110.0f, &s1);
    expect(memcmp(spec_buf[3], spec_buf[4], sizeof spec_buf[0]) == 0, "sync at amount 0 ignores the reset phase (B free-runs)");

    // the panel: keys pick models and pairs, M toggles the walk
    spec_tap('3');
    expect_eq(P.model, M_CROSS, "key 3 picks cross");
    spec_tap(KEY_RIGHT);
    expect_eq(P.model, M_VPM, "RIGHT steps to vpm");
    spec_tap(KEY_LEFT); spec_tap(KEY_LEFT); spec_tap(KEY_LEFT);
    expect_eq(P.model, M_RING, "three LEFTs walk back to ring");
    spec_tap(KEY_LEFT);
    expect_eq(P.model, M_VOCODE, "LEFT wraps around from ring to vocode");
    spec_tap(KEY_DOWN);
    expect_eq(P.pair, NPAIR - 1, "DOWN wraps the wave pair");
    spec_tap('M');
    expect(autoplay, "M turns the walk on");
    // every() rides the audio clock, which step() does not run — drive the walk directly
    last_render_n = 0;
    int m0 = P.model;
    for (int i = 0; i < 8; i++) walk_step();
    expect(last_render_n == REN_N, "a walk step renders a full note");
    expect(P.model != m0, "the next bar steps to another model");
}
#endif
