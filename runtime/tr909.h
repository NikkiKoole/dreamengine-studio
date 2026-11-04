// tr909.h — the shared TR-909 VOICE BANK, extracted so the 909 stops living in
// one cart (the tr808.h move, again). tr909.c grew the definitive voicing: the
// fast-sweep house KICK with its ATTACK-click layer, the 185+330Hz snare, sine
// toms with a click, the woody rimshot, the three-retrigger clap, and the METAL-bank
// ROM stand-ins for the metal (hats/crash/ride) riding a shared lowpass (FM + highpass until 2026-11-04). When a
// SECOND cart (acidcandy's 909 face) wanted the SAME kit, this became the one
// honest 909 core so the two can never drift.
//
// A cart owns its PATTERN + UI + per-voice KNOBS; this owns the SOUND: the 13-slot
// bank (tr909_build), the metal-lowpass XY (tr909_metal), the knob-shaped trigger
// (tr909_fire), and the stroke family flam/drag/ratchet (tr909_fire_stroke). The
// three per-voice knobs (tune/decay/colour, floats 0..1, 0.5 = neutral) are the
// cart's arrays, passed in.
//
//   #include "studio.h"
#include "cart_ctx.h"   // per-instance state seam (docs/design/engine-context.md)
//   #include "tr909.h"
//   static float ktune[TR9_NV], kdecay[TR9_NV], kcolor[TR9_NV];
//   static float mcut = 0.40f, mres = 0.33f;              // the metal-filter XY
//   void init(void) {
//       for (int v=0;v<TR9_NV;v++){ ktune[v]=kdecay[v]=kcolor[v]=0.5f; }
//       tr909_build(TR909_BASE);
//       tr909_metal(TR909_BASE, mcut, mres);              // aim the 5 metal lowpasses
//   }
//   tr909_fire(TR909_BASE, TR9_BD, 1, 0, ktune, kdecay, kcolor);
//   tr909_fire_stroke(TR909_BASE, v, stroke, boost, delay, step_ms, ktune, kdecay, kcolor);
//
// The bank occupies `base` .. `base + TR909_NSLOT-1` (13 slots).

#ifndef TR909_H
#define TR909_H
#include "studio.h"
#include <math.h>   // powf

// ── voice roles: the 11-voice TR-909 roster ──────────────────────────────────
enum { TR9_BD, TR9_SD, TR9_LT, TR9_MT, TR9_HT, TR9_RS, TR9_CP, TR9_CH, TR9_OH, TR9_CC, TR9_RC, TR9_NV };
static const char *TR909_NAME[TR9_NV] = {
    "BASS", "SNRE", "LTOM", "MTOM", "HTOM", "RIM", "CLAP", "CHHH", "OPHH", "CRSH", "RIDE",
};

// ── instrument-slot layout: 13 slots (kick/snare/tom/crash layer two each) ───
enum { TR9S_BD, TR9S_BDC, TR9S_SDB, TR9S_SDN, TR9S_TOM, TR9S_TOMC, TR9S_RS,
       TR9S_CP, TR9S_HHC, TR9S_HHO, TR9S_CC, TR9S_CCN, TR9S_RC };
#define TR909_NSLOT 13
#define TR909_BASE   9
#define TR909_FLAM_MS 22        // grace-note lead — the panel's fixed flam spacing

// stroke family (per-cell humanize)
enum { TR9_ST_PLAIN, TR9_ST_FLAM, TR9_ST_DRAG, TR9_ST_RATCHET, TR9_NSTROKE };

// ── per-voice knob maths (arrays are the CART's; 0.5 = neutral) ───────────────
// ── measured against a real TR-909 (2026-11-04): the "TR909 - all" set in a vintage-drums collection,
// every voice at its knob marks 0 / 3 / 7 / 10 (tools/oneshot-compare.js). Two knobs did not behave:
// TUNE swept two OCTAVES where a real tom / snare / kick tune spans ~10 semitones (and unevenly: most
// of it above the middle), and DECAY stopped growing because the slot's own decay capped the gate. The
// tables below are the real knob, read at its marks; between marks, linear.
static float tr909__knob4(float k, const float t[4]) {          // marks at 0, .3, .7, 1
    if (k <= 0.0f) return t[0];
    if (k >= 1.0f) return t[3];
    if (k < 0.3f)  return t[0] + (t[1] - t[0]) * (k / 0.3f);
    if (k < 0.7f)  return t[1] + (t[2] - t[1]) * ((k - 0.3f) / 0.4f);
    return t[2] + (t[3] - t[2]) * ((k - 0.7f) / 0.3f);
}
// semitones from the voice's pitch AT MARK 3, measured: kick 54/54/65/97 Hz, snare 129/161/215/248,
// toms (all three alike) e.g. low 75/86/118/129
static const float TR909_TUNE_BD[4]  = { 0.0f, 0.0f, 3.2f, 10.1f };
static const float TR909_TUNE_SD[4]  = { -3.8f, 0.0f, 5.0f, 7.5f };
static const float TR909_TUNE_TOM[4] = { -2.5f, 0.0f, 5.4f, 7.4f };
static int tr909__midi(const float *kt, int v, int base) {
    const float *t = v == TR9_BD ? TR909_TUNE_BD : v == TR9_SD ? TR909_TUNE_SD
                   : (v == TR9_LT || v == TR9_MT || v == TR9_HT) ? TR909_TUNE_TOM : 0;
    if (!t) return base + (int)((kt[v] - 0.5f) * 24.0f + 0.5f);   // rim / clap / metal: ±12 as before
    float st = tr909__knob4(kt[v], t);
    return base + (int)(st + (st >= 0.0f ? 0.5f : -0.5f));        // base = the pitch AT MARK 3
}
// -20 dB times, measured at the decay marks 0 / 3 / 7 / 10
static const float TR909_DEC_BD[4] = { 85, 115, 170, 220 };
static const float TR909_DEC_LT[4] = { 170, 190, 220, 265 };
static const float TR909_DEC_MT[4] = { 115, 120, 125, 145 };
static const float TR909_DEC_HT[4] = { 120, 130, 160, 160 };
static int tr909__decay_slot(float t20) { return (int)(t20 / 0.9f * 1.1f + 0.5f); }   // a linear decay crosses -20 dB at 0.9
// hats: the DECAY knob at marks 0 / .25 / .5 / .75 / 1 (the set has 0, 2, 4, 6, 8, 10)
static const float TR909_DEC_CH[5] = { 25, 55, 80, 93, 95 };
static const float TR909_DEC_OH[5] = { 145, 185, 190, 235, 260 };
static float tr909__knob5(float k, const float t[5]) {
    if (k <= 0.0f) return t[0];
    if (k >= 1.0f) return t[4];
    float x = k * 4.0f; int i = (int)x; float f = x - (float)i;
    return t[i] + (t[i + 1] - t[i]) * f;
}
// crash / ride TUNE on a real 909 is the sample's PLAYBACK RATE: across the knob the crash rises ~an
// octave (5.1 -> 9.6 kHz) and gets SHORTER by the same factor (845 -> 430 ms); the ride ~9 semitones,
// 695 -> 380 ms. So the knob moves pitch AND length together: ±6 semitones, length / rate
static int tr909__cym_semi(float k) { return (int)((k - 0.5f) * 12.0f + (k >= 0.5f ? 0.5f : -0.5f)); }
static int tr909__cym_len(float k, int ms) { return (int)((float)ms / de_powf(2.0f, (k - 0.5f)) + 0.5f); }
static int tr909__dur(const float *kd, int v, int base) {
    float s = de_powf(4.0f, (kd[v] - 0.5f) * 2.0f);
    int d = (int)(base * s + 0.5f);
    return d < 5 ? 5 : d;
}
static int tr909__cv(const float *kc, int v, int lo, int hi) {
    int val = (int)(lo + (hi - lo) * kc[v] + 0.5f);
    return val < 0 ? 0 : val > 7 ? 7 : val;
}
static int tr909__vv(int base, int boost) { int v = base + boost; return v < 0 ? 0 : (v > 7 ? 7 : v); }

// ── SNARE: does a harder hit read NOISIER, or just louder? (audit §J9 · Synth Secrets Part 35) ──
// The 909's half of the same idea; the reasoning is written out once in tr808.h next to
// `tr808_snare_dyn` — read that. Short version: Part 35 says harder strikes make "the spectrum become
// more noise-like", but `boost` was added to the body and noise layers equally, so an accent was just
// the same snare louder. This tips them in opposite directions instead: noise up, body down.
//
// DUPLICATED here rather than shared, deliberately, on two grounds. tr909.h has no dependency on
// tr808.h and taking one on for four lines of integer maths would be a bad trade; and this pair's
// existing convention is already per-header copies of the small helpers (tr909__vv / __cv / __dur are
// each byte-identical twins of the 808's). Keeping them separate also keeps the two machines tunable
// apart, which is the standing rule for this pair — an 808 snare and a 909 snare are not the same drum.
// If you change the CURVE here, change it in tr808.h too, or the two will drift.
//   0 = as shipped (one fixed balance) · 1 = one step per unit of boost · 2 = double.
#define TR909_STATE(X) \
    X(int,      tr909_snare_dyn,   ,              1)

#ifndef DE_CART_CTX
DE_CTX_STATICS(TR909_STATE)
#else
DE_CTX_BLOCK_SAVED(tr909, Tr909, TR909_STATE)   // voicing config a cart changes → session state
// defined BEFORE the access macros below: after them, `c->name` would expand to
// `c->(ctx_()->name)`, because the member names are macros from that point on.
static void tr909_ctx_init_(Tr909Ctx *c) {
    c->tr909_snare_dyn = 1;   // the only non-zero default; de_state_for zero-fills the rest
}
#define tr909_snare_dyn (tr909_ctx_()->tr909_snare_dyn)
#endif

// Linear in boost, NOT `(boost*dyn+1)/2` — boost only ever spans 0..2 here, so that rounding collapsed
// boost 1 and boost 2 onto the same tilt and the balance stopped varying with velocity at all. See the
// longer note in tr808.h; keep the two curves identical.
static int tr909__snare_tilt(int boost) {   // velocity steps to tip: + for noise, - for body
    if (tr909_snare_dyn <= 0 || boost <= 0) return 0;
    return boost * tr909_snare_dyn;
}

// ── build the 13-slot bank at `base` (call on init; then tr909_metal) ─────────
static void tr909_build(int base) {
    // kick — sine body, lowpassed, +30st sweep over 35ms (punch, not boom); run hot
    instrument(base + TR9S_BD, INSTR_SINE, 0, 300, 0, 50);
    instrument_filter(base + TR9S_BD, FILTER_LOW, 380, 2);
    instrument_env(base + TR9S_BD, 0, ENV_PITCH, 0, 35, 30.0f);
    instrument_drive(base + TR9S_BD, 0.22f);   // was 0.35: its 2nd harmonic sat 13 dB under the body, a real 909's 27-30
    // kick click — the ATTACK knob layer: a 10ms highpassed noise tick
    instrument(base + TR9S_BDC, INSTR_NOISE, 0, 9, 0, 4);
    instrument_filter(base + TR9S_BDC, FILTER_HIGH, 2500, 2);

    // snare body (185Hz + 330Hz modes), quick settle sweep
    instrument(base + TR9S_SDB, INSTR_SINE, 0, 90, 0, 25);
    instrument_filter(base + TR9S_SDB, FILTER_LOW, 1400, 1);
    instrument_env(base + TR9S_SDB, 0, ENV_PITCH, 0, 15, 4.0f);
    // snare noise — brighter + longer than the 808's
    instrument(base + TR9S_SDN, INSTR_NOISE, 0, 170, 0, 50);
    instrument_filter(base + TR9S_SDN, FILTER_LOW, 8500, 4);   // real 909 snare tops out ~8.2-9 kHz (a highpass ran to 20)

    // toms — sine pitch drop + a short noise click
    instrument(base + TR9S_TOM, INSTR_SINE, 0, 220, 0, 45);
    instrument_env(base + TR9S_TOM, 0, ENV_PITCH, 0, 60, 7.0f);
    instrument(base + TR9S_TOMC, INSTR_NOISE, 0, 12, 0, 5);
    instrument_filter(base + TR9S_TOMC, FILTER_BAND, 2000, 3);

    // rimshot — short woody dual ping through a low highpass
    instrument(base + TR9S_RS, INSTR_TRI, 0, 28, 0, 10);
    instrument_filter(base + TR9S_RS, FILTER_HIGH, 150, 2);   // 400 Hz cut the real body (215 Hz)

    // handclap — bandpassed noise; the retriggers in tr909_fire() make the clap
    instrument(base + TR9S_CP, INSTR_NOISE, 0, 100, 0, 45);
    instrument_filter(base + TR9S_CP, FILTER_BAND, 1200, 5);

    // ── the ROM-sample stand-ins: METAL banks (hats/crash/ride) ─────────────
    // A real 909's hats and cymbals are 6-bit SAMPLES of real cymbals: noisy, energy 3.3 / 7.5 / 14 kHz
    // (open hat, 10/50/90%), the crash 2.8 / 6.0 / 10.7, the ride 4.3 / 9.5 / 14. These were FM clang,
    // and FM's sidebands + aliasing put them at 8 / 15.7 / 20.8 kHz whatever the filter did (measured
    // 2026-11-04 against the "TR909 - all" set). The METAL bank lowpassed lands there; the XY pad
    // (tr909_metal) now rides that LOWPASS: right = brighter. Lengths come from tr909_fire per hit.
    instrument(base + TR9S_HHC, INSTR_METAL, 0, 40, 0, 16);
    instrument_harmonics(base + TR9S_HHC, 0.70f); instrument_timbre(base + TR9S_HHC, 0.85f); instrument_morph(base + TR9S_HHC, 0.85f);
    instrument_mode(base + TR9S_HHC, MODE_METAL_DECAY, 0.40f); instrument_mode(base + TR9S_HHC, MODE_METAL_SPREAD, 0.20f);

    instrument(base + TR9S_HHO, INSTR_METAL, 0, 380, 0, 110);
    instrument_harmonics(base + TR9S_HHO, 0.70f); instrument_timbre(base + TR9S_HHO, 0.85f); instrument_morph(base + TR9S_HHO, 0.85f);
    instrument_mode(base + TR9S_HHO, MODE_METAL_DECAY, 0.66f); instrument_mode(base + TR9S_HHO, MODE_METAL_SPREAD, 0.20f);
    instrument_choke(base + TR9S_HHC, base + TR9S_HHO);   // closed hat chokes the open hat

    // crash — the bank + a separate long noise wash (TONE crossfades)
    instrument(base + TR9S_CC, INSTR_METAL, 0, 1000, 0, 280);
    instrument_harmonics(base + TR9S_CC, 0.55f); instrument_timbre(base + TR9S_CC, 0.30f); instrument_morph(base + TR9S_CC, 0.30f);
    instrument_mode(base + TR9S_CC, MODE_METAL_DECAY, 0.80f); instrument_mode(base + TR9S_CC, MODE_METAL_SPREAD, 0.15f);
    instrument(base + TR9S_CCN, INSTR_NOISE, 0, 1200, 0, 320);

    // ride — the bank, cleaner (less noise), a bright ping
    instrument(base + TR9S_RC, INSTR_METAL, 0, 550, 0, 160);
    instrument_harmonics(base + TR9S_RC, 0.62f); instrument_timbre(base + TR9S_RC, 0.35f); instrument_morph(base + TR9S_RC, 0.30f);
    instrument_mode(base + TR9S_RC, MODE_METAL_DECAY, 0.80f); instrument_mode(base + TR9S_RC, MODE_METAL_SPREAD, 0.20f);
}

// aim the shared metal LOWPASS on all five metal slots from the XY pad (mcut/mres 0..1). Since
// 2026-11-04 a lowpass (was a highpass on FM): right = BRIGHTER. At the carts' default (mcut 0.40) the
// corners land where a real 909's metal tops out (hats ~12.5 kHz, crash ~9.5, ride ~13).
static void tr909_metal(int base, float mcut, float mres) {
    float m = 0.3f + 1.4f * mcut;
    int   r = (int)(mres * 12.0f + 0.5f);
    #define TR909_LPC(hz) ((int)((hz) * m) > 18000 ? 18000 : (int)((hz) * m))
    instrument_filter(base + TR9S_HHC, FILTER_LOW, TR909_LPC(14500), r);
    instrument_filter(base + TR9S_HHO, FILTER_LOW, TR909_LPC(14500), r);
    instrument_filter(base + TR9S_CC,  FILTER_LOW, TR909_LPC(11000), r);
    instrument_filter(base + TR9S_CCN, FILTER_LOW, TR909_LPC(10000), r);
    instrument_filter(base + TR9S_RC,  FILTER_LOW, TR909_LPC(15000), r);
    #undef TR909_LPC
}

// fire one voice `delay` ms from now. Analog voices per the schematic; hats/cymbals
// are the FM-clang ROM stand-ins. kt/kd/kc are the caller's TR9_NV knob arrays.
// …and the keyboard's trigger: the same voice with an EXTRA semitone offset on every layer, so one
// drum can be played chromatically past what the ±12-semitone TUNE knob can reach. Clamped to a real
// MIDI note (a keyboard can ask for the kick 60 semitones down; a knob never could). Byte-identical
// for semi == 0. See tr808_fire_semi for the full rationale — same move, same reason.
static int tr909__midi_semi(const float *kt, int v, int base, int semi) {
    int m = tr909__midi(kt, v, base) + semi;
    return m < 0 ? 0 : m > 127 ? 127 : m;
}
static void tr909_fire_semi(int base, int v, int boost, int delay,
                            const float *kt, const float *kd, const float *kc, int semi) {
    #define M(bs)     tr909__midi_semi(kt, v, (bs), semi)
    #define D(bs)     tr909__dur(kd, v, (bs))
    #define CV(lo,hi) tr909__cv(kc, v, (lo), (hi))
    #define VV(bs)    tr909__vv((bs), boost)
    switch (v) {
    case TR9_BD:  // house kick: fast sweep body + ATTK click layer
        {   int dd = tr909__decay_slot(tr909__knob4(kd[v], TR909_DEC_BD));
            instrument(base + TR9S_BD, INSTR_SINE, 0, dd, 0, 50);
            schedule_hit(delay, M(33), base + TR9S_BD, VV(6), dd);   // 54 Hz at mark 3, as measured
        }
        schedule_hit(delay, 60, base + TR9S_BDC, VV(CV(0, 3)), 10);   // the real ATTACK knob barely moves the spectrum
        break;
    case TR9_SD: {  // 185Hz + 330Hz modes; SNPY fades body↔noise
        int body = CV(8, 1), snpy = CV(1, 8) - 2;   // -2: the real snare's energy is mostly its body
        int tilt = tr909__snare_tilt(boost);   // §J9: a harder hit tips body→noise, not just louder
        schedule_hit(delay, M(51), base + TR9S_SDB, VV(body - tilt), D(95));       // 161 Hz at mark 3
        schedule_hit(delay, M(57), base + TR9S_SDB, VV(body - 2 - tilt), D(95));   // its 2nd mode, ~1.4x (real 161 + 226, 5-9 dB under)
        schedule_hit(delay, M(60), base + TR9S_SDN, VV(snpy + tilt), D(180));
        break;
    }
    case TR9_LT: case TR9_MT: case TR9_HT: {  // sine drop + CLIK attack noise
        // pitch AT MARK 3 measured 86 / 108 / 129 Hz; a strong 2nd mode ~2x up (6-12 dB under), which the
        // sine alone does not have: booked as a quieter octave
        int m = v == TR9_LT ? 40 : v == TR9_MT ? 44 : 47;
        const float *dt = v == TR9_LT ? TR909_DEC_LT : v == TR9_MT ? TR909_DEC_MT : TR909_DEC_HT;
        int dd = tr909__decay_slot(tr909__knob4(kd[v], dt));
        instrument(base + TR9S_TOM, INSTR_SINE, 0, dd, 0, 45);
        schedule_hit(delay, M(m),      base + TR9S_TOM,  VV(5), dd);
        schedule_hit(delay, M(m) + 12, base + TR9S_TOM,  VV(2), dd * 2 / 3);
        schedule_hit(delay, 70, base + TR9S_TOMC, VV(CV(0, 5)), 14);
        break;
    }
    case TR9_RS:  // woody dual ping
        // a real 909 rim: 215 Hz, its 463 Hz partial ~8 dB under (was 659 / 330)
        schedule_hit(delay, M(57), base + TR9S_RS, VV(5), D(30));
        schedule_hit(delay, M(70), base + TR9S_RS, VV(2), D(30));
        break;
    case TR9_CP:  // three retriggers ~9ms apart + a soft room tail
        schedule_hit(delay,      M(60), base + TR9S_CP, VV(5), 11);
        schedule_hit(delay + 9,  M(60), base + TR9S_CP, VV(5), 11);
        schedule_hit(delay + 18, M(60), base + TR9S_CP, VV(5), 11);
        schedule_hit(delay + 26, M(60), base + TR9S_CP, VV(3), D(130));
        break;
    case TR9_CH: {  // the metal bank, short; DECAY from the real knob
        int dd = tr909__decay_slot(tr909__knob5(kd[v], TR909_DEC_CH));
        instrument(base + TR9S_HHC, INSTR_METAL, 0, dd, 0, 16);
        schedule_hit(delay, 84 + semi, base + TR9S_HHC, VV(4), dd);
        break;
    }
    case TR9_OH: {  // same metal, long burst; choked by a closed hit
        int dd = tr909__decay_slot(tr909__knob5(kd[v], TR909_DEC_OH));
        instrument(base + TR9S_HHO, INSTR_METAL, 0, dd, 0, 110);
        schedule_hit(delay, 84 + semi, base + TR9S_HHO, VV(4), dd);
        break;
    }
    case TR9_CC: {  // TONE fades the bank ↔ noise wash; TUNE = playback rate (pitch + length)
        // real: -20 dB in 845 / 715 / 495 / 430 ms across the knob, ~660 at its middle
        int st = tr909__cym_semi(kt[v]) + semi;
        int dd = tr909__decay_slot((float)tr909__cym_len(kt[v], D(660)));
        instrument(base + TR9S_CC,  INSTR_METAL, 0, dd, 0, 280);
        instrument(base + TR9S_CCN, INSTR_NOISE, 0, dd, 0, 320);
        schedule_hit(delay, 100 + st, base + TR9S_CC,  VV(CV(6, 1)), dd);
        schedule_hit(delay, 60 + st,  base + TR9S_CCN, VV(CV(1, 6)), dd);
        break;
    }
    case TR9_RC: {  // a cleaner ping; TUNE = playback rate. Real: 695 / 480 / 440 / 380 ms, ~470 at the middle
        int st = tr909__cym_semi(kt[v]) + semi;
        int dd = tr909__decay_slot((float)tr909__cym_len(kt[v], D(470)));
        instrument(base + TR9S_RC, INSTR_METAL, 0, dd, 0, 160);
        schedule_hit(delay, 104 + st, base + TR9S_RC, VV(4), dd);
        break;
    }
    }
    #undef M
    #undef D
    #undef CV
    #undef VV
}
// the machine's own trigger: the voice at the pitch its knob says. Everything that is not a keyboard
// comes through here, including tr909_fire_stroke below, so the two bodies can never drift.
static void tr909_fire(int base, int v, int boost, int delay,
                       const float *kt, const float *kd, const float *kc) {
    tr909_fire_semi(base, v, boost, delay, kt, kd, kc, 0);
}

// fire a step WITH its stroke. Flam/drag lead the main hit with 1/2 quiet grace
// notes; a ratchet replaces it with four decaying hits across the step.
static void tr909_fire_stroke(int base, int v, int st, int boost, int delay, int step_ms,
                              const float *kt, const float *kd, const float *kc) {
    int g;
    switch (st) {
    case TR9_ST_DRAG:
        g = delay - 2 * TR909_FLAM_MS; tr909_fire(base, v, boost - 4, g < 0 ? 0 : g, kt, kd, kc);
        /* fall through — a drag is a flam with one more grace */
    case TR9_ST_FLAM:
        g = delay - TR909_FLAM_MS;     tr909_fire(base, v, boost - 3, g < 0 ? 0 : g, kt, kd, kc);
        tr909_fire(base, v, boost, delay, kt, kd, kc);
        break;
    case TR9_ST_RATCHET:
        for (int k = 0; k < 4; k++) tr909_fire(base, v, boost - k, delay + k * step_ms / 4, kt, kd, kc);
        break;
    default:
        tr909_fire(base, v, boost, delay, kt, kd, kc);
    }
}

// set voice v's stereo PAN (-1 L .. 0 centre .. +1 R) across all its output slots. Toms
// (LT/MT/HT) share one slot → they pan as a GROUP (see tr808_pan). SEAM: independent tom
// pan = splitting the bank.
static void tr909_pan(int base, int v, float pan) {
    switch (v) {
    case TR9_BD: instrument_pan(base + TR9S_BD,  pan); instrument_pan(base + TR9S_BDC, pan); break;
    case TR9_SD: instrument_pan(base + TR9S_SDB, pan); instrument_pan(base + TR9S_SDN, pan); break;
    case TR9_LT: case TR9_MT: case TR9_HT:
                 instrument_pan(base + TR9S_TOM, pan); instrument_pan(base + TR9S_TOMC, pan); break;
    case TR9_RS: instrument_pan(base + TR9S_RS,  pan); break;
    case TR9_CP: instrument_pan(base + TR9S_CP,  pan); break;
    case TR9_CH: instrument_pan(base + TR9S_HHC, pan); break;
    case TR9_OH: instrument_pan(base + TR9S_HHO, pan); break;
    case TR9_CC: instrument_pan(base + TR9S_CC,  pan); instrument_pan(base + TR9S_CCN, pan); break;
    case TR9_RC: instrument_pan(base + TR9S_RC,  pan); break;
    }
}

// FINE-tune voice v by `semis` (fractional semitones, live) across its slots — see tr808_tune.
static void tr909_tune(int base, int v, float semis) {
    switch (v) {
    case TR9_BD: instrument_tune(base + TR9S_BD,  semis); instrument_tune(base + TR9S_BDC, semis); break;
    case TR9_SD: instrument_tune(base + TR9S_SDB, semis); instrument_tune(base + TR9S_SDN, semis); break;
    case TR9_LT: case TR9_MT: case TR9_HT:
                 instrument_tune(base + TR9S_TOM, semis); instrument_tune(base + TR9S_TOMC, semis); break;
    case TR9_RS: instrument_tune(base + TR9S_RS,  semis); break;
    case TR9_CP: instrument_tune(base + TR9S_CP,  semis); break;
    case TR9_CH: instrument_tune(base + TR9S_HHC, semis); break;
    case TR9_OH: instrument_tune(base + TR9S_HHO, semis); break;
    case TR9_CC: instrument_tune(base + TR9S_CC,  semis); instrument_tune(base + TR9S_CCN, semis); break;
    case TR9_RC: instrument_tune(base + TR9S_RC,  semis); break;
    }
}

// ── self-check — the spec.h "specs on an includeable" pattern ────────────────
// The 909's half; the long reasoning is in tr808.h next to its twin. Short version: the snare tilt curve
// must actually depend on VELOCITY, and it once silently didn't — `(boost * dyn + 1) / 2` collapsed boost 1
// and boost 2 onto the same tilt because boost only ever spans 0..2 here, so the feature measured and
// A/B'd as real while no longer being velocity-dependent. Since the two headers keep DUPLICATE copies of
// this curve on purpose, each needs its own assertions or one can drift without the other noticing.
#ifdef DE_SPEC
#include "spec.h"
static inline void tr909_selfcheck(void) {
    int save = tr909_snare_dyn;

    int soft_ok = 1;
    for (int d = 0; d <= 4; d++) { tr909_snare_dyn = d; soft_ok &= tr909__snare_tilt(0) == 0; }
    expect(soft_ok, "tr909 snare: boost 0 never tilts (soft hits stay exactly as shipped)");

    tr909_snare_dyn = 0;
    expect(tr909__snare_tilt(1) == 0 && tr909__snare_tilt(2) == 0,
           "tr909 snare: dyn 0 is a true no-op (the shipped fixed balance)");

    int dep_ok = 1;
    for (int d = 1; d <= 2; d++) {
        tr909_snare_dyn = d;
        dep_ok &= tr909__snare_tilt(2) > tr909__snare_tilt(1);
        dep_ok &= tr909__snare_tilt(1) > tr909__snare_tilt(0);
    }
    expect(dep_ok, "tr909 snare: tilt is STRICTLY increasing in boost (velocity-dependent, not just on)");

    tr909_snare_dyn = save;

    // The two headers deliberately carry separate copies of this curve, so DRIFT between them is the risk
    // the duplication buys. Each header's own check above only proves its OWN curve is sane — one could be
    // changed to `boost*dyn*2`, still pass, and silently no longer match its twin. This catches that, but
    // only when both headers are in the same translation unit, hence the guard: tr909.h does not include
    // tr808.h and must not start. It activates by itself in any cart that includes both.
    //
    // No cart runs it TODAY. `acidcandy` is the only one that includes both, and it cannot host a spec()
    // at all: it has a global named `step`, which spec.h's `void step(int n)` shadows, so merely including
    // spec.h breaks the cart's own transport code. That trap is generic to sequencer carts — see
    // docs/design/spec-harness.md → "reserved names".
#ifdef TR808_H
    int agree = 1, s8 = tr808_snare_dyn;
    for (int d = 0; d <= 2; d++)
        for (int b = 0; b <= 2; b++) {
            tr909_snare_dyn = d; tr808_snare_dyn = d;
            agree &= tr909__snare_tilt(b) == tr808__snare_tilt(b);
        }
    tr909_snare_dyn = save; tr808_snare_dyn = s8;
    expect(agree, "the 909 and 808 snare tilt curves still agree (duplicated on purpose, must not drift)");
#endif
}
#endif

#endif // TR909_H
