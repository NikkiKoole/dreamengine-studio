// tr909.h — the shared TR-909 VOICE BANK, extracted so the 909 stops living in
// one cart (the tr808.h move, again). tr909.c grew the definitive voicing: the
// fast-sweep house KICK with its ATTACK-click layer, the 185+330Hz snare, sine
// toms with a click, the woody rimshot, the three-retrigger clap, and the FM-clang
// ROM stand-ins for the metal (hats/crash/ride) riding a shared highpass. When a
// SECOND cart (acidcandy's 909 face) wanted the SAME kit, this became the one
// honest 909 core so the two can never drift.
//
// A cart owns its PATTERN + UI + per-voice KNOBS; this owns the SOUND: the 13-slot
// bank (tr909_build), the metal-highpass XY (tr909_metal), the knob-shaped trigger
// (tr909_fire), and the stroke family flam/drag/ratchet (tr909_fire_stroke). The
// three per-voice knobs (tune/decay/colour, floats 0..1, 0.5 = neutral) are the
// cart's arrays, passed in.
//
//   #include "studio.h"
//   #include "tr909.h"
//   static float ktune[TR9_NV], kdecay[TR9_NV], kcolor[TR9_NV];
//   static float mcut = 0.40f, mres = 0.33f;              // the metal-filter XY
//   void init(void) {
//       for (int v=0;v<TR9_NV;v++){ ktune[v]=kdecay[v]=kcolor[v]=0.5f; }
//       tr909_build(TR909_BASE);
//       tr909_metal(TR909_BASE, mcut, mres);              // aim the 5 metal highpasses
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
static int tr909__midi(const float *kt, int v, int base) { return base + (int)((kt[v] - 0.5f) * 24.0f + 0.5f); }
static int tr909__dur(const float *kd, int v, int base) {
    float s = powf(4.0f, (kd[v] - 0.5f) * 2.0f);
    int d = (int)(base * s + 0.5f);
    return d < 5 ? 5 : d;
}
static int tr909__cv(const float *kc, int v, int lo, int hi) {
    int val = (int)(lo + (hi - lo) * kc[v] + 0.5f);
    return val < 0 ? 0 : val > 7 ? 7 : val;
}
static int tr909__vv(int base, int boost) { int v = base + boost; return v < 0 ? 0 : (v > 7 ? 7 : v); }

// ── build the 13-slot bank at `base` (call on init; then tr909_metal) ─────────
static void tr909_build(int base) {
    // kick — sine body, lowpassed, +30st sweep over 35ms (punch, not boom); run hot
    instrument(base + TR9S_BD, INSTR_SINE, 0, 300, 0, 50);
    instrument_filter(base + TR9S_BD, FILTER_LOW, 380, 2);
    instrument_env(base + TR9S_BD, 0, ENV_PITCH, 0, 35, 30.0f);
    instrument_drive(base + TR9S_BD, 0.35f);
    // kick click — the ATTACK knob layer: a 10ms highpassed noise tick
    instrument(base + TR9S_BDC, INSTR_NOISE, 0, 9, 0, 4);
    instrument_filter(base + TR9S_BDC, FILTER_HIGH, 2500, 2);

    // snare body (185Hz + 330Hz modes), quick settle sweep
    instrument(base + TR9S_SDB, INSTR_SINE, 0, 90, 0, 25);
    instrument_filter(base + TR9S_SDB, FILTER_LOW, 1400, 1);
    instrument_env(base + TR9S_SDB, 0, ENV_PITCH, 0, 15, 4.0f);
    // snare noise — brighter + longer than the 808's
    instrument(base + TR9S_SDN, INSTR_NOISE, 0, 170, 0, 50);
    instrument_filter(base + TR9S_SDN, FILTER_HIGH, 1400, 2);

    // toms — sine pitch drop + a short noise click
    instrument(base + TR9S_TOM, INSTR_SINE, 0, 220, 0, 45);
    instrument_env(base + TR9S_TOM, 0, ENV_PITCH, 0, 60, 7.0f);
    instrument(base + TR9S_TOMC, INSTR_NOISE, 0, 12, 0, 5);
    instrument_filter(base + TR9S_TOMC, FILTER_BAND, 2000, 3);

    // rimshot — short woody dual ping through a low highpass
    instrument(base + TR9S_RS, INSTR_TRI, 0, 28, 0, 10);
    instrument_filter(base + TR9S_RS, FILTER_HIGH, 400, 3);

    // handclap — bandpassed noise; the retriggers in tr909_fire() make the clap
    instrument(base + TR9S_CP, INSTR_NOISE, 0, 100, 0, 45);
    instrument_filter(base + TR9S_CP, FILTER_BAND, 1200, 5);

    // ── the ROM-sample stand-ins: FM-clang metal (hats/crash/ride) ──────────
    instrument(base + TR9S_HHC, INSTR_FM, 0, 40, 0, 16);
    instrument_harmonics(base + TR9S_HHC, 0.55f);
    instrument_timbre(base + TR9S_HHC, 0.90f);
    instrument_morph(base + TR9S_HHC, 0.85f);
    instrument_env(base + TR9S_HHC, 0, ENV_CUTOFF, 0, 28, -5000.0f);

    instrument(base + TR9S_HHO, INSTR_FM, 0, 380, 0, 110);
    instrument_harmonics(base + TR9S_HHO, 0.55f);
    instrument_timbre(base + TR9S_HHO, 0.90f);
    instrument_morph(base + TR9S_HHO, 0.85f);
    instrument_env(base + TR9S_HHO, 0, ENV_CUTOFF, 0, 220, -4500.0f);
    instrument_choke(base + TR9S_HHC, base + TR9S_HHO);   // closed hat chokes the open hat

    // crash — FM clang + a separate long noise wash (TONE crossfades)
    instrument(base + TR9S_CC, INSTR_FM, 0, 1000, 0, 280);
    instrument_harmonics(base + TR9S_CC, 0.55f);
    instrument_timbre(base + TR9S_CC, 1.00f);
    instrument_morph(base + TR9S_CC, 0.90f);
    instrument(base + TR9S_CCN, INSTR_NOISE, 0, 1200, 0, 320);

    // ride — same metal, less feedback = a cleaner ringing ping
    instrument(base + TR9S_RC, INSTR_FM, 0, 550, 0, 160);
    instrument_harmonics(base + TR9S_RC, 0.55f);
    instrument_timbre(base + TR9S_RC, 0.72f);
    instrument_morph(base + TR9S_RC, 0.58f);
}

// aim the shared metal highpass on all five metal slots from the XY pad (mcut/mres 0..1)
static void tr909_metal(int base, float mcut, float mres) {
    float m = 0.3f + 1.4f * mcut;
    int   r = (int)(mres * 12.0f + 0.5f);
    instrument_filter(base + TR9S_HHC, FILTER_HIGH, (int)(8500 * m), r);
    instrument_filter(base + TR9S_HHO, FILTER_HIGH, (int)(8000 * m), r);
    instrument_filter(base + TR9S_CC,  FILTER_HIGH, (int)(4500 * m), r);
    instrument_filter(base + TR9S_CCN, FILTER_HIGH, (int)(5200 * m), r);
    instrument_filter(base + TR9S_RC,  FILTER_HIGH, (int)(5000 * m), r);
}

// fire one voice `delay` ms from now. Analog voices per the schematic; hats/cymbals
// are the FM-clang ROM stand-ins. kt/kd/kc are the caller's TR9_NV knob arrays.
static void tr909_fire(int base, int v, int boost, int delay,
                       const float *kt, const float *kd, const float *kc) {
    #define M(bs)     tr909__midi(kt, v, (bs))
    #define D(bs)     tr909__dur(kd, v, (bs))
    #define CV(lo,hi) tr909__cv(kc, v, (lo), (hi))
    #define VV(bs)    tr909__vv((bs), boost)
    switch (v) {
    case TR9_BD:  // house kick: fast sweep body + ATTK click layer
        schedule_hit(delay, M(32), base + TR9S_BD, VV(6), D(320));
        schedule_hit(delay, 60, base + TR9S_BDC, VV(CV(0, 6)), 10);
        break;
    case TR9_SD: {  // 185Hz + 330Hz modes; SNPY fades body↔noise
        int body = CV(8, 1), snpy = CV(1, 8);
        schedule_hit(delay, M(54), base + TR9S_SDB, VV(body), D(95));
        schedule_hit(delay, M(64), base + TR9S_SDB, VV(body - 1), D(95));
        schedule_hit(delay, M(60), base + TR9S_SDN, VV(snpy), D(180));
        break;
    }
    case TR9_LT: case TR9_MT: case TR9_HT: {  // sine drop + CLIK attack noise
        int m = v == TR9_LT ? 42 : v == TR9_MT ? 47 : 54;
        schedule_hit(delay, M(m),  base + TR9S_TOM,  VV(5), D(240));
        schedule_hit(delay, 70, base + TR9S_TOMC, VV(CV(0, 5)), 14);
        break;
    }
    case TR9_RS:  // woody dual ping
        schedule_hit(delay, M(76), base + TR9S_RS, VV(5), D(30));
        schedule_hit(delay, M(64), base + TR9S_RS, VV(3), D(30));
        break;
    case TR9_CP:  // three retriggers ~9ms apart + a soft room tail
        schedule_hit(delay,      M(60), base + TR9S_CP, VV(5), 11);
        schedule_hit(delay + 9,  M(60), base + TR9S_CP, VV(5), 11);
        schedule_hit(delay + 18, M(60), base + TR9S_CP, VV(5), 11);
        schedule_hit(delay + 26, M(60), base + TR9S_CP, VV(3), D(130));
        break;
    case TR9_CH:  // FM clang through the closing highpass
        schedule_hit(delay, M(97), base + TR9S_HHC, VV(4), D(40));
        break;
    case TR9_OH:  // same metal, long burst; choked by a closed hit
        schedule_hit(delay, M(97), base + TR9S_HHO, VV(4), D(400));
        break;
    case TR9_CC:  // TONE fades FM clang ↔ noise wash
        schedule_hit(delay, M(84), base + TR9S_CC,  VV(CV(6, 1)), D(1100));
        schedule_hit(delay, M(60), base + TR9S_CCN, VV(CV(1, 6)), D(1300));
        break;
    case TR9_RC:  // cleaner FM ping — bell + a quieter edge a fifth up
        schedule_hit(delay, M(76), base + TR9S_RC, VV(4), D(600));
        schedule_hit(delay, M(83), base + TR9S_RC, VV(2), D(600));
        break;
    }
    #undef M
    #undef D
    #undef CV
    #undef VV
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

#endif // TR909_H
