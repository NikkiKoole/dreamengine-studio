/* de:meta
{
  "slug": "bogie",
  "title": "bogie",
  "status": "active",
  "created": "2026-09-28",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "drum-synthesis",
    "additive-synth"
  ],
  "lineage": "Rows 2 and 3 of docs/design/choochootracker-borrow-list.md, cart-first: Choochootracker's Bogie drum synth (chipnomad_lib/synth/drum_synth_voice.cpp, MIT) hat + cymbal METAL BANK, six square oscillators at inharmonic ratios each with a DIFFERENT decay, so the bank never settles into a pitched square-wave chord as it rings out (our tr808.h hat fires two bank members on ONE slot with ONE decay, which is exactly the chord). Plus its cowbell cross-modulation, which turns out to be an INSTR_MME patch (a square pair through the cross model). Built from stock engine pieces: six INSTR_SQUARE slots per bank with per-slot decays, a highpassed INSTR_NOISE, one INSTR_MME. The shipped tr808.h hat / cymbal / cowbell sit behind a toggle as the reference.",
  "homage": "Choochootracker's Bogie (2026) after the TR-808's six-oscillator metal bank; the cowbell after the 808's two-square circuit with a third oscillator cross-modulating it.",
  "description": {
    "summary": "A hat, an open hat, a cymbal and a cowbell built from six-square metal banks with staggered decays, next to the 808 versions for comparison.",
    "detail": "Four pads. The hats and cymbal are six square oscillators at inharmonic ratios, each on its own slot with its own decay (25..125 ms for the hat, 100..475 ms for the cymbal, both stretched by TONE and the DECAY knob), plus a highpassed noise layer. Because the modes die at different rates the spectrum moves as the sound decays, the way a real cymbal's does; a bank with one decay stays a chord. The cowbell is two squares a sixth apart with a third oscillator cross-modulating them (FM knob): classic at zero, metallic when turned up, and it is the MME engine's cross model doing it. Press 8 to route the same pads to the shipped 808 hat, open hat, cymbal and cowbell from tr808.h, so the two can be judged by ear on the same gesture. Each pad has its own knob set (tone / sweep / noise / fm / decay / hpf).",
    "controls": "A S D F: closed hat, open hat, cymbal, cowbell (or tap the pads) · 1-4: select the pad the knobs edit · drag the knobs (wheel = fine) · LEFT/RIGHT knob, UP/DOWN adjust · 8: BOGIE / 808 reference · M: autoplay pattern"
  },
  "todo": [
    "ear pass: does the staggered bank beat the 808 hat on its own terms (press 8 back and forth); then decide whether it becomes an INSTR_METAL engine (one voice per hit, exponential per-mode decays) or a tr808.h/morphdrum.h option",
    "the amp ADSR decay is LINEAR (sound_adsr_gated); each mode's exponential time constant is mapped to a 3.5x ramp here, the engine version should decay exponentially",
    "stagger DIRECTION: Bogie's top modes live longest (tail brightens); the 808 cymbal's lows do (tail darkens). Worth a knob, measured either way with wav-envelope",
    "a hat hit costs 7 voices (6 modes + noise) against the engine's 32; fine for a kit cart, not for a busy rack"
  ]
}
de:meta */
// bogie — the Choochootracker Bogie metal bank, prototyped from stock engine pieces.
//
// What row 2 is about: an 808 hat is a six-square metal bank. tr808.h fires two members of
// that bank on ONE slot with ONE decay, so the ring-out is a two-note square chord. Bogie
// gives every mode its OWN lifetime (.025 + o*.020 + tone*.070 s for the hat, .10 + o*.075 +
// tone*.70 for the cymbal) so the upper modes die first and the spectrum MOVES while it
// decays, the "spectral migration" Synth Secrets says a real cymbal does. Here that is six
// INSTR_SQUARE slots per bank, each with its own decay, hit together.
//
// Row 3, the cowbell: two squares (808: 540 + 800 Hz) plus a third oscillator that cross-
// modulates them, classic at FM = 0. That is INSTR_MME's CROSS model on a square pair, so
// the cowbell here is one MME slot through the 808's 2.6 kHz bandpass.
//
// The 808 reference: tr808.h is built at slot 27 and key 8 routes the pads to its CH / OH /
// CY / CB, neutral knobs, so both kits answer the same gesture (the modal cart's A/B move).
//
// Approximations, all on purpose for a prototype: mode ratios are rounded to the nearest
// semitone (fractional midi via instrument_tune), the mode decays are LINEAR ramps sized at
// 3.5x Bogie's exponential time constants, and the noise is one slot per bank instead of a
// per-sample brightNoise. The engine version, if the ear says yes, does all three exactly.
//
// MEASURED (wav-envelope, 60 ms windows, single hits, 2026-09-28), so nobody re-derives it:
//   · the 808 open hat's brightness is FLAT for its whole decay (3.97 in every window, centroid
//     pinned at 19 kHz): a static spectrum fading, i.e. the chord. Bogie's open hat and cymbal
//     MOVE (brightness 1.1 → 2.3 across the decay), which is the whole claim of row 2.
//   · the direction is UP: Bogie gives the TOP modes the longest lifetimes (.025 + o*.020), so
//     the lows die first and the tail gets brighter. Synth Secrets' 808 cymbal does the
//     opposite (the LOW band carries the long decay, highs die first). A stagger-direction knob
//     is the obvious next experiment; this cart ports Bogie's direction as-is.
//   · at the closed hat's short DECAY the global exp(-5.5 t/dur) envelope dominates and the
//     six lifetimes collapse to within a few ms of each other (25..30 ms): the stagger is an
//     open-hat / cymbal property, and Bogie's closed hat is a chord too, just a 60 ms one.
//
// controls: A S D F pads · 1-4 select · knobs · LEFT/RIGHT + UP/DOWN · 8 kit · M autoplay

#include "studio.h"
#include "ui.h"
#include "tr808.h"
#include <string.h>

enum { V_CH, V_OH, V_CY, V_CB, NVOICE };
static const char *VNAME[NVOICE] = { "CL HAT", "OP HAT", "CYMBAL", "COWBELL" };
static const char VKEY[NVOICE] = { 'A', 'S', 'D', 'F' };

enum { K_TONE, K_SWEEP, K_NOISE, K_FM, K_DECAY, K_HPF, NKNOB };
static const char *KNAME[NKNOB] = { "tone", "sweep", "noise", "fm", "decay", "hpf" };

#define NMODE    6
#define S_HC     5                     // closed-hat bank 5..10
#define S_HO     11                    // open-hat bank 11..16
#define S_CY     17                    // cymbal bank 17..22
#define S_NZ     23                    // noise: 23 ch, 24 oh, 25 cy
#define S_CB     26                    // cowbell: one INSTR_MME slot
#define S_808    27                    // tr808.h bank 27..42

static const float HAT_R[NMODE] = { 1.18f, 1.56f, 2.17f, 2.87f, 3.73f, 4.61f };
static const float CYM_R[NMODE] = { 1.31f, 1.79f, 2.41f, 3.16f, 4.07f, 5.23f };
#define F_NOTE   110.0f                // Bogie's frequency_ term: the pad plays A2

// per-pad knob sets (0..1). Bogie's own defaults, then a tone/decay per pad.
// per-pad level trim so each pad lands at the 808 reference's peak (measured 2026-09-28 on
// single hits: closed hat was +1.5 dB, cymbal +9 dB, cowbell +4 dB, open hat within 1 dB)
static const float TRIM[NVOICE] = { 0.84f, 1.00f, 0.35f, 0.63f };

static float P[NVOICE][NKNOB] = {
    { 0.45f, 0.10f, 0.55f, 0.20f, 0.18f, 0.35f },   // closed hat
    { 0.45f, 0.10f, 0.55f, 0.20f, 0.62f, 0.35f },   // open hat
    { 0.50f, 0.05f, 0.45f, 0.25f, 0.70f, 0.20f },   // cymbal
    { 0.50f, 0.15f, 0.00f, 0.00f, 0.45f, 0.00f },   // cowbell (noise unused)
};
static int   sel = V_CH, ksel = K_TONE;
static bool  ref808 = false;           // 8: route the pads to tr808.h
static bool  autoplay = true;
static int   pstep = 0;
static float glow[NVOICE];
static float kt[TR_NV], kd[TR_NV], kc[TR_NV];   // the 808's neutral knobs
static bool  dirty[NVOICE];

// ── the bank maths (pure, spec'd) ──────────────────────────────────────────────
static float hz2midi(float hz) { return 69.0f + 12.0f * de_log2f(hz / 440.0f); }

// mode ratio with the FM spread: (o+1) * fm * 0.20 (hat) / 0.18 (cym)
static float mode_ratio(int cym, int o, float fm) {
    return (cym ? CYM_R[o] : HAT_R[o]) + fm * (float)(o + 1) * (cym ? 0.18f : 0.20f);
}
// bank base, Hz: hat 180 + tone*260 + f*.30, cymbal 250 + tone*360 + f*.45
static float bank_base_hz(int cym, float tone) {
    return cym ? 250.0f + tone * 360.0f + F_NOTE * 0.45f : 180.0f + tone * 260.0f + F_NOTE * 0.30f;
}
// Bogie's global duration for a pad: .018 + d² * (hat 1.55 / cymbal 2.0)
static float duration_s(int cym, float decay) { return 0.018f + decay * decay * (cym ? 2.0f : 1.55f); }
// each mode's time constant, combined with the global exp(-5.5 t/dur) envelope:
// 1 / (1/tau_mode + 5.5/dur). The STAGGER is the point: mode o+1 outlives mode o.
static float mode_tau_s(int cym, int o, float tone, float decay) {
    float tm = cym ? 0.10f + (float)o * 0.075f + tone * 0.70f
                   : 0.025f + (float)o * 0.020f + tone * 0.070f;
    float tg = duration_s(cym, decay) / 5.5f;
    return 1.0f / (1.0f / tm + 1.0f / tg);
}
// a LINEAR engine ramp standing in for an exponential of time constant tau: 3.5 tau is where
// the exponential has fallen 30 dB (2.5 cut the cymbal's tail visibly short, measured)
static int tau_to_ms(float tau) { int ms = (int)(tau * 3500.0f + 0.5f); return ms < 4 ? 4 : ms; }
// the onset pitch sweep: base * (1 + sweep * .75 * fast), fast = exp(-t / (.006 + .05 (1 - tone)))
static float sweep_semis(int cym, float sweep) { return 12.0f * de_log2f(1.0f + sweep * (cym ? 0.90f : 0.75f)); }
static int   sweep_ms(float tone) { return (int)((0.006f + 0.050f * (1.0f - tone)) * 3000.0f + 0.5f); }
// noise colour: brightNoise = n - onepole(n, .015 + tone*.25)  →  a highpass at a*sr/2π
static int noise_hpf_hz(float tone) { return (int)((0.015f + tone * 0.25f) * 44100.0f / 6.2831853f + 0.5f); }
// cowbell: B/A = 1.39 + tone*.18 as an MME interval (0.5 = unison, ±2 oct across 0..1)
static float cb_interval(float tone) { return 0.5f + de_log2f(1.39f + tone * 0.18f) * 0.25f; }
static int   hpf_hz(float hpf) { return (int)(200.0f * de_powf(2.0f, hpf * 5.3f) + 0.5f); }

// ── set-and-hold: push one pad's knobs into its slots (only when they changed) ──
static int bank_slot(int v) { return v == V_CH ? S_HC : v == V_OH ? S_HO : S_CY; }
static int bank_midi[NVOICE][NMODE];
static int bank_ms[NVOICE][NMODE];
static int noise_ms[NVOICE];
static int cb_ms = 200, cb_midi = 73;

static void apply_bank(int v) {
    int cym = (v == V_CY);
    const float *p = P[v];
    int s0 = bank_slot(v);
    float base = bank_base_hz(cym, p[K_TONE]);
    for (int o = 0; o < NMODE; o++) {
        float m  = hz2midi(base * mode_ratio(cym, o, p[K_FM]));
        int   mi = (int)(m + 0.5f);
        if (mi < 1) mi = 1; if (mi > 120) mi = 120;
        int ms = tau_to_ms(mode_tau_s(cym, o, p[K_TONE], p[K_DECAY]));
        bank_midi[v][o] = mi; bank_ms[v][o] = ms;
        instrument(s0 + o, INSTR_SQUARE, 0, ms, 0, 6);
        instrument_tune(s0 + o, m - (float)mi);
        instrument_level(s0 + o, TRIM[v] * (0.5f + p[K_FM] * 0.5f));   // (.06 + fm*.06) / .12, trimmed
        instrument_env(s0 + o, 0, ENV_PITCH, 0, sweep_ms(p[K_TONE]), sweep_semis(cym, p[K_SWEEP]));
        instrument_filter(s0 + o, p[K_HPF] > 0.01f ? FILTER_HIGH : FILTER_OFF, hpf_hz(p[K_HPF]), 1);
    }
    int nz = S_NZ + v;
    noise_ms[v] = tau_to_ms(duration_s(cym, p[K_DECAY]) / 5.5f);
    instrument(nz, INSTR_NOISE, 0, noise_ms[v], 0, 6);
    instrument_level(nz, TRIM[v]);
    instrument_filter(nz, FILTER_HIGH, noise_hpf_hz(p[K_TONE]), 1);
}

static void apply_cowbell(void) {
    const float *p = P[V_CB];
    cb_ms = tau_to_ms(duration_s(0, p[K_DECAY]) / 5.5f);
    instrument(S_CB, INSTR_MME, 0, cb_ms, 0, 10);
    instrument_harmonics(S_CB, (2.0f + 0.5f) / 7.0f);              // the CROSS model
    instrument_timbre(S_CB, p[K_FM]);                              // cross-mod depth = "metallic"
    instrument_morph(S_CB, 0.5f);                                  // both squares, equal
    instrument_mode(S_CB, MODE_MME_PAIR, (4.0f + 0.5f) / 5.0f);    // square / square
    instrument_mode(S_CB, MODE_MME_INTERVAL, cb_interval(p[K_TONE]));
    instrument_mode(S_CB, MODE_MME_FEEDBACK, 0.0f);
    instrument_mode(S_CB, MODE_MME_SHAPER, 0.0f);
    instrument_filter(S_CB, FILTER_BAND, 2640, 5);                 // the 808's cowbell bandpass
    instrument_level(S_CB, TRIM[V_CB]);
    instrument_env(S_CB, 0, ENV_PITCH, 0, sweep_ms(p[K_TONE]), 12.0f * de_log2f(1.0f + p[K_SWEEP] * 0.8f));
    cb_midi = 73;                                                  // 554 Hz, the 808's lower square
}

static void apply(int v) { if (v == V_CB) apply_cowbell(); else apply_bank(v); dirty[v] = false; }

// ── fire ────────────────────────────────────────────────────────────────────────
static const int TR_ROLE[NVOICE] = { TR_CH, TR_OH, TR_CY, TR_CB };

static void fire(int v, int delay) {
    glow[v] = 1.0f;
    if (ref808) { tr808_fire(S_808, TR_ROLE[v], 0, delay, kt, kd, kc); return; }
    if (v == V_CB) { schedule_hit(delay, cb_midi, S_CB, 6, cb_ms); return; }
    int s0 = bank_slot(v);
    for (int o = 0; o < NMODE; o++) schedule_hit(delay, bank_midi[v][o], s0 + o, 6, bank_ms[v][o]);
    int nv = (int)(P[v][K_NOISE] * 7.0f + 0.5f);
    if (nv > 0) schedule_hit(delay, 60, S_NZ + v, nv, noise_ms[v]);
}

static void select_pad(int v) { sel = v; }
static void set_ref(bool r) { ref808 = r; }

void init(void) {
    for (int v = 0; v < TR_NV; v++) kt[v] = kd[v] = kc[v] = 0.5f;
    tr808_build(S_808);
    instrument_choke(S_HC, S_HO);                                  // closed hat chokes the open bank's
    for (int o = 1; o < NMODE; o++) instrument_choke(S_HC, S_HO + o);   // every mode (one voice, many slots)
    for (int v = 0; v < NVOICE; v++) { apply(v); glow[v] = 0.0f; }
    bpm(112);
}

// a 16-step pattern: closed hats on 8ths, open on the "and" of 2 and 4, cymbal on 1 every
// other bar, cowbell on 2 and 4 — enough to hear ring-outs overlap
static void pattern_step(int s) {
    int q = 15000 / 112;                                           // one 16th at 112 bpm, ms
    for (int i = 0; i < 4; i++) {
        int st = (s * 4 + i) & 31, d = i * q;
        if ((st & 1) == 0 && st != 6 && st != 14 && st != 22 && st != 30) fire(V_CH, d);
        if (st == 6 || st == 14 || st == 22 || st == 30) fire(V_OH, d);
        if (st == 0) fire(V_CY, d);
        if (st == 4 || st == 12 || st == 20 || st == 28) fire(V_CB, d);
    }
}

void update(void) {
    for (int v = 0; v < NVOICE; v++) {
        if (keyp(VKEY[v])) { fire(v, 0); autoplay = false; }
        if (keyp('1' + v)) select_pad(v);
    }
    if (keyp('8')) set_ref(!ref808);
    if (keyp('M')) autoplay = !autoplay;
    if (keyp(KEY_LEFT))  ksel = (ksel + NKNOB - 1) % NKNOB;
    if (keyp(KEY_RIGHT)) ksel = (ksel + 1) % NKNOB;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        P[sel][ksel] = clamp(P[sel][ksel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        dirty[sel] = true;
        if (frame() % 10 == 0) fire(sel, 0);
    }
    for (int v = 0; v < NVOICE; v++) if (dirty[v]) apply(v);

    if (autoplay && every(1)) { pattern_step(pstep); pstep = (pstep + 1) & 7; }

#ifdef DE_TRACE
    watch("sel", "%d", sel);
    watch("ref808", "%d", ref808 ? 1 : 0);
    watch("tone", "%.2f", P[sel][K_TONE]);
    watch("decay", "%.2f", P[sel][K_DECAY]);
    watch("fm", "%.2f", P[sel][K_FM]);
    watch("auto", "%d", autoplay ? 1 : 0);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();
    print("BOGIE", 6, 4, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("six-square metal banks, staggered decays", 50, 6, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M auto: on" : "M auto: off", SCREEN_W - 6, 6, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);

    // kit toggle
    if (ui_button(6, 16, 96, 14, ref808 ? "8: 808 REF" : "8: BOGIE")) set_ref(!ref808);
    print(ref808 ? "pads -> tr808.h hat/cym/cowbell" : "pads -> the banks", 108, 20, ref808 ? CLR_PEACH : CLR_MEDIUM_GREY);

    // pads
    for (int v = 0; v < NVOICE; v++) {
        int x = 6 + v * 78, y = 36, w = 72, h = 40;
        glow[v] *= 0.90f;
        int bg = glow[v] > 0.4f ? CLR_LIGHT_YELLOW : glow[v] > 0.05f ? CLR_PEACH : (v == sel ? CLR_DARK_BROWN : CLR_DARKER_GREY);
        rectfill(x, y, w, h, bg);
        rect(x, y, w, h, v == sel ? CLR_ORANGE : CLR_DARK_GREY);
        print(VNAME[v], x + 5, y + 6, glow[v] > 0.05f ? CLR_BROWNISH_BLACK : CLR_WHITE);
        print(str("%c  (%d)", VKEY[v], v + 1), x + 5, y + 26, glow[v] > 0.05f ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
        if (tapp(x, y, w, h)) { fire(v, 0); select_pad(v); autoplay = false; }
    }

    // knobs for the selected pad
    print(str("knobs: %s", VNAME[sel]), 6, 84, CLR_YELLOW);
    for (int k = 0; k < NKNOB; k++) {
        int x = 30 + k * 52, y = 112;
        bool dead = (sel == V_CB && (k == K_NOISE || k == K_HPF));
        float before = P[sel][k];
        if (ui_knob(&P[sel][k], x, y, KNAME[k])) { if (!dead && P[sel][k] != before) dirty[sel] = true; }
        if (k == ksel) print("^", x - 2, y + 22, CLR_YELLOW);
        if (dead) print("-", x - 2, y - 2, CLR_DARK_GREY);
    }

    // the stagger, drawn: each mode's decay as a bar (the thing this cart exists to hear)
    if (sel != V_CB) {
        print("mode lifetimes (ms)", 6, 146, CLR_MEDIUM_GREY);
        for (int o = 0; o < NMODE; o++) {
            int ms = bank_ms[sel][o];
            int w = ms / 3; if (w > 200) w = 200;
            rectfill(110, 146 + o * 7, w, 5, o == 0 ? CLR_ORANGE : CLR_BROWN);
            font(FONT_TINY);
            print(str("%d", ms), 112 + w + 2, 146 + o * 7, CLR_DARK_GREY);
            font(FONT_SMALL);
        }
    } else {
        print(str("cowbell: MME cross model, sq/sq, B/A %.2f, fm %.2f", 1.39f + P[V_CB][K_TONE] * 0.18f, P[V_CB][K_FM]), 6, 146, CLR_MEDIUM_GREY);
        print(str("decay %d ms through the 808 bandpass", cb_ms), 6, 156, CLR_DARK_GREY);
    }

    font(FONT_TINY);
    print("A S D F pads  1-4 select  8 kit  M auto  LEFT/RIGHT knob  UP/DOWN adjust", 6, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}

#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    autoplay = false;
    step(1);
    expect(!ref808, "boots on the Bogie banks, not the 808 reference");
    expect_eq(sel, V_CH, "boots editing the closed hat");

    // the point of row 2: every mode outlives the one below it, on both banks, at every tone
    for (int cym = 0; cym < 2; cym++)
        for (int t = 0; t < 3; t++) {
            float tone = 0.5f * (float)t;
            int ok = 1;
            for (int o = 1; o < NMODE; o++)
                if (!(mode_tau_s(cym, o, tone, 0.5f) > mode_tau_s(cym, o - 1, tone, 0.5f))) ok = 0;
            expect(ok, str("%s bank: mode lifetimes are staggered (tone %.1f)", cym ? "cymbal" : "hat", tone));
        }
    expect(tau_to_ms(mode_tau_s(0, 5, 0.45f, 0.18f)) > tau_to_ms(mode_tau_s(0, 0, 0.45f, 0.18f)),
           "closed hat: the top mode's ramp is longer than the bottom one's");
    expect(mode_tau_s(0, 0, 0.5f, 0.9f) > mode_tau_s(0, 0, 0.5f, 0.1f), "DECAY knob lengthens a mode");
    expect(mode_tau_s(0, 2, 0.9f, 0.5f) > mode_tau_s(0, 2, 0.1f, 0.5f), "TONE lengthens a mode (Bogie's tone term)");
    expect(mode_tau_s(1, 0, 0.5f, 0.5f) > mode_tau_s(0, 0, 0.5f, 0.5f), "cymbal modes outlive hat modes");

    // ratios: inharmonic, rising, and FM spreads the upper modes more than the lower
    {
        int ok = 1;
        for (int o = 1; o < NMODE; o++) if (!(mode_ratio(0, o, 0.0f) > mode_ratio(0, o - 1, 0.0f))) ok = 0;
        expect(ok, "hat ratios rise mode by mode");
        expect(mode_ratio(0, 5, 1.0f) - mode_ratio(0, 5, 0.0f) > mode_ratio(0, 0, 1.0f) - mode_ratio(0, 0, 0.0f),
               "FM spreads the top mode more than the bottom one");
    }
    // the applied bank agrees with the maths (apply() ran in init)
    expect(bank_ms[V_CH][5] > bank_ms[V_CH][0], "closed hat slots carry staggered decays");
    expect(bank_ms[V_OH][0] > bank_ms[V_CH][0], "open hat rings longer than closed at the same mode");
    expect(bank_midi[V_CH][5] > bank_midi[V_CH][0], "top mode sits above the bottom mode");
    expect(bank_midi[V_CY][0] > 40 && bank_midi[V_CY][5] < 120, "cymbal bank stays on the keyboard");

    // noise colour and the onset sweep move with tone, the way the source does
    expect(noise_hpf_hz(1.0f) > noise_hpf_hz(0.0f), "brighter tone = higher noise highpass");
    expect(noise_hpf_hz(0.0f) > 90 && noise_hpf_hz(1.0f) < 2000, "noise highpass spans ~105..1860 Hz");
    expect(sweep_ms(0.0f) > sweep_ms(1.0f), "a dark tone sweeps longer");
    expect(spec_close(sweep_semis(0, 1.0f), 9.69f, 0.05f), "full hat sweep starts 75% sharp (9.7 semitones)");
    expect(spec_close(sweep_semis(0, 0.0f), 0.0f, 0.001f), "no sweep at 0");

    // the cowbell as an MME patch: B/A 1.39..1.57 sits inside MME's ±2 octave interval knob
    expect(cb_interval(0.0f) > 0.5f && cb_interval(1.0f) < 1.0f, "cowbell interval is above unison and in range");
    expect(spec_close(de_powf(2.0f, (cb_interval(0.5f) - 0.5f) * 4.0f), 1.48f, 0.01f), "tone 0.5 = the 808's 800/540 ratio");
    expect(hpf_hz(0.0f) == 200 && hpf_hz(1.0f) > 7000, "hpf knob spans 200 Hz .. ~8 kHz");

    // the panel
    spec_tap('3');
    expect_eq(sel, V_CY, "key 3 selects the cymbal");
    spec_tap('8');
    expect(ref808, "8 routes the pads to the 808 reference");
    spec_tap('8');
    expect(!ref808, "8 again brings the banks back");
    spec_tap(KEY_RIGHT);
    expect_eq(ksel, K_SWEEP, "RIGHT moves to the sweep knob");
    float before = P[V_CY][K_SWEEP], other = P[V_CH][K_SWEEP];
    key_down(KEY_UP); step(3); key_up(KEY_UP); step(1);
    expect(P[V_CY][K_SWEEP] > before, "UP raises the selected pad's knob");
    expect(P[V_CH][K_SWEEP] == other, "and leaves the other pads alone");
    expect(!dirty[V_CY], "the change was applied (set-and-hold) by the next update");
}
#endif
