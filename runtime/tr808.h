// tr808.h — the shared TR-808 VOICE BANK, extracted so the 808 stops living in
// one cart. tr808.c had grown the definitive voicing (16 voices modeled from the
// reverse-engineered circuit values — the six-oscillator metal bank, the ~50Hz
// bridged-T kick with the decay-knob boom, the 180+330Hz dual-mode snare, the
// three-retrigger handclap, rimshot/claves as one dual bridged-T circuit); when a
// SECOND cart (acidcandy's 808 face) wanted the SAME drums, this became the one
// honest 808 core so the two can never drift (the acid303.h move, for drums).
//
// A cart owns its PATTERN + UI + per-voice KNOBS; this owns the SOUND: the 14-slot
// instrument bank (tr808_build) + the layered, knob-shaped trigger (tr808_fire).
// The three per-voice knobs (tune / decay / colour, floats 0..1, 0.5 = neutral)
// are the cart's arrays — passed in — so a cart can expose all three (tr808), a
// subset, or none (acidcandy uses neutral) without the voice changing.
//
//   #include "studio.h"
//   #include "tr808.h"
//   static float ktune[TR_NV], kdecay[TR_NV], kcolor[TR_NV];   // your knobs (0.5 = neutral)
//   void init(void) {
//       for (int v=0;v<TR_NV;v++){ ktune[v]=kdecay[v]=kcolor[v]=0.5f; }
//       tr808_build(TR808_BASE);                                // build the bank at a free slot base
//   }
//   // trigger a voice at velocity-boost `boost`, `delay` ms from now:
//   tr808_fire(TR808_BASE, TR_BD, 1, 0, ktune, kdecay, kcolor);
//
// The bank occupies `base` .. `base + TR808_NSLOT-1` (14 instrument slots).
// tr808.c uses base 9 (slots 5..8 are user waves). Pick a base that clears your
// other voices (acidcandy's two 303s live at 6/7, so its kit sits at 9+).

#ifndef TR808_H
#define TR808_H
#include "studio.h"
#include <math.h>   // powf

// ── voice roles: the full 16-voice TR-808 palette (index-stable) ─────────────
enum { TR_BD, TR_SD, TR_LT, TR_MT, TR_HT, TR_LC, TR_MC, TR_HC,
       TR_RS, TR_CLV, TR_CP, TR_MA, TR_CB, TR_CY, TR_OH, TR_CH, TR_NV };

static const char *TR808_NAME[TR_NV] = {
    "BASS", "SNRE", "LTOM", "MTOM", "HTOM", "LCGA", "MCGA", "HCGA",
    "RIM",  "CLAV", "CLAP", "MARA", "COWB", "CYMB", "OPHH", "CHHH",
};

// ── instrument-slot layout: 14 slots (some voices share a slot) ──────────────
enum { TRS_BD, TRS_SDB, TRS_SDN, TRS_TOM, TRS_TOMN, TRS_CON, TRS_RS, TRS_CLV,
       TRS_CP, TRS_MAR, TRS_CB, TRS_CYT, TRS_HATO, TRS_HATC };
#define TR808_NSLOT 14
#define TR808_BASE   9        // tr808.c's base (slots 5..8 are user waves)

// ── per-voice knob maths (arrays are the CART's; 0.5 = neutral) ───────────────
static int tr808__midi(const float *ktune, int v, int base) {           // ±12 semitones
    return base + (int)((ktune[v] - 0.5f) * 24.0f + 0.5f);
}
static int tr808__dur(const float *kdecay, int v, int base) {           // 0.25×..4× decay, 1× at 0.5
    float s = powf(4.0f, (kdecay[v] - 0.5f) * 2.0f);
    int d = (int)(base * s + 0.5f);
    return d < 5 ? 5 : d;
}
static int tr808__cv(const float *kcolor, int v, int lo, int hi) {      // crossfade a vol level by colour
    int val = (int)(lo + (hi - lo) * kcolor[v] + 0.5f);
    return val < 0 ? 0 : val > 7 ? 7 : val;
}
static int tr808__vv(int base, int boost) { int v = base + boost; return v < 0 ? 0 : (v > 7 ? 7 : v); }

// ── build the 14-slot voice bank at `base` (call on init) ─────────────────────
static void tr808_build(int base) {
    // kick — the boom: low sine, lowpassed, +26st pitch drop over 50ms, run a
    // little hot (the Miami-bass / trap saturated 808).
    instrument(base + TRS_BD, INSTR_SINE, 0, 480, 0, 60);
    instrument_filter(base + TRS_BD, FILTER_LOW, 250, 3);
    instrument_env(base + TRS_BD, 0, ENV_PITCH, 0, 50, 26.0f);
    instrument_drive(base + TRS_BD, 0.28f);

    // snare body (fired twice: 180Hz + 330Hz modes)
    instrument(base + TRS_SDB, INSTR_SINE, 0, 100, 0, 30);
    instrument_filter(base + TRS_SDB, FILTER_LOW, 1200, 1);
    instrument_env(base + TRS_SDB, 0, ENV_PITCH, 0, 20, 3.0f);
    // snare "snappy" — highpassed noise
    instrument(base + TRS_SDN, INSTR_NOISE, 0, 130, 0, 40);
    instrument_filter(base + TRS_SDN, FILTER_HIGH, 1800, 2);

    // toms — sine with a pitch drop + a separate low noise thud
    instrument(base + TRS_TOM, INSTR_SINE, 0, 260, 0, 50);
    instrument_env(base + TRS_TOM, 0, ENV_PITCH, 0, 80, 6.0f);
    instrument(base + TRS_TOMN, INSTR_NOISE, 0, 28, 0, 12);
    instrument_filter(base + TRS_TOMN, FILTER_BAND, 900, 2);

    // congas — the tom circuit without the noise, shorter
    instrument(base + TRS_CON, INSTR_SINE, 0, 150, 0, 30);
    instrument_env(base + TRS_CON, 0, ENV_PITCH, 0, 25, 4.0f);

    // rimshot — both bridged-T modes through a highpass (keeps 455 AND 1667)
    instrument(base + TRS_RS, INSTR_TRI, 0, 45, 0, 15);
    instrument_filter(base + TRS_RS, FILTER_HIGH, 500, 3);

    // claves — single 2500Hz ping
    instrument(base + TRS_CLV, INSTR_TRI, 0, 40, 0, 14);
    instrument_filter(base + TRS_CLV, FILTER_LOW, 4000, 5);

    // handclap — bandpassed noise; the retriggers in tr808_fire() make the clap
    instrument(base + TRS_CP, INSTR_NOISE, 0, 110, 0, 50);
    instrument_filter(base + TRS_CP, FILTER_BAND, 1100, 5);

    // maracas
    instrument(base + TRS_MAR, INSTR_NOISE, 0, 24, 0, 10);
    instrument_filter(base + TRS_MAR, FILTER_HIGH, 6500, 2);

    // cowbell — square pair through the classic ~2.6kHz bandpass
    instrument(base + TRS_CB, INSTR_SQUARE, 0, 250, 0, 60);
    instrument_filter(base + TRS_CB, FILTER_BAND, 2640, 5);

    // cymbal — bank squares through the 3440Hz region, very long ring
    instrument(base + TRS_CYT, INSTR_SQUARE, 0, 850, 0, 200);
    instrument_filter(base + TRS_CYT, FILTER_HIGH, 3440, 3);

    // hats — bank squares through ~7kHz highpass; open vs closed = decay
    instrument(base + TRS_HATO, INSTR_SQUARE, 0, 340, 0, 90);
    instrument_filter(base + TRS_HATO, FILTER_HIGH, 7000, 3);
    instrument(base + TRS_HATC, INSTR_SQUARE, 0, 42, 0, 16);
    instrument_filter(base + TRS_HATC, FILTER_HIGH, 7000, 3);
    instrument_choke(base + TRS_HATC, base + TRS_HATO);  // closed hat chokes the open hat
}

// fire one voice `delay` ms from now, at velocity-boost `boost`. Layered hits
// follow the schematic: metal voices = members of the six-oscillator bank (MIDI
// 79=800Hz, 73=540, 72=522.7, 66=369.6, 63=304.4, 56=205.3), snare = 180+330Hz
// modes + noise. `ktune/kdecay/kcolor` are the caller's TR_NV knob arrays.
static void tr808_fire(int base, int v, int boost, int delay,
                       const float *ktune, const float *kdecay, const float *kcolor) {
    #define M(bs)     tr808__midi(ktune,  v, (bs))
    #define D(bs)     tr808__dur(kdecay,  v, (bs))
    #define CV(lo,hi) tr808__cv(kcolor,   v, (lo), (hi))
    #define VV(bs)    tr808__vv((bs), boost)
    switch (v) {
    case TR_BD:  // ~50Hz bridged-T with the decay-knob boom
        schedule_hit(delay, M(31), base + TRS_BD, VV(6), D(500));
        break;
    case TR_SD: {  // 180Hz + 330Hz modes; SNPY knob fades body↔noise
        int body = CV(8, 0), snpy = CV(0, 8);
        schedule_hit(delay, M(54), base + TRS_SDB, VV(body), D(110));
        schedule_hit(delay, M(64), base + TRS_SDB, VV(body), D(110));
        schedule_hit(delay, M(60), base + TRS_SDN, VV(snpy), D(140));
        break;
    }
    case TR_LT: case TR_MT: case TR_HT: {  // sine drop + THUD knob controls noise thud level
        int m = v == TR_LT ? 40 : v == TR_MT ? 45 : 52;
        schedule_hit(delay, M(m),  base + TRS_TOM,  VV(4), D(280));
        schedule_hit(delay, M(60), base + TRS_TOMN, VV(CV(0, 5)), D(30));
        break;
    }
    case TR_LC: case TR_MC: case TR_HC: {  // same circuit, cleaner + shorter
        int m = v == TR_LC ? 52 : v == TR_MC ? 57 : 63;
        schedule_hit(delay, M(m), base + TRS_CON, VV(4), D(160));
        break;
    }
    case TR_RS:  // dual bridged-T: 1667Hz (midi 92) + 455Hz (midi 70)
        schedule_hit(delay, M(92), base + TRS_RS, VV(4), D(50));
        schedule_hit(delay, M(70), base + TRS_RS, VV(3), D(50));
        break;
    case TR_CLV: // the rimshot circuit retuned to 2500Hz = midi 99 exactly
        schedule_hit(delay, M(99), base + TRS_CLV, VV(4), D(45));
        break;
    case TR_CP:  // three retriggers ~10ms apart + a soft room tail
        schedule_hit(delay,      M(60), base + TRS_CP, VV(4), 12);
        schedule_hit(delay + 10, M(60), base + TRS_CP, VV(4), 12);
        schedule_hit(delay + 20, M(60), base + TRS_CP, VV(4), 12);
        schedule_hit(delay + 28, M(60), base + TRS_CP, VV(3), D(140));
        break;
    case TR_MA:  schedule_hit(delay, M(90), base + TRS_MAR, VV(3), D(30)); break;
    case TR_CB:  // bank osc 1+2; TONE fades 540Hz↔800Hz emphasis
        schedule_hit(delay, M(73), base + TRS_CB, VV(CV(7, 0)), D(220));
        schedule_hit(delay, M(79), base + TRS_CB, VV(CV(0, 7)), D(220));
        break;
    case TR_CY:  // three bank members; TONE fades warm↔bright
        schedule_hit(delay, M(79), base + TRS_CYT, VV(CV(0, 6)), D(900));
        schedule_hit(delay, M(72), base + TRS_CYT, VV(2), D(900));
        schedule_hit(delay, M(66), base + TRS_CYT, VV(CV(5, 0)), D(900));
        break;
    case TR_OH:  // two bank members; RING fades warm↔bright
        schedule_hit(delay, M(79), base + TRS_HATO, VV(CV(0, 6)), D(360));
        schedule_hit(delay, M(72), base + TRS_HATO, VV(CV(5, 0)), D(360));
        break;
    case TR_CH:  // same two, ~50ms
        schedule_hit(delay, M(79), base + TRS_HATC, VV(3), D(50));
        schedule_hit(delay, M(72), base + TRS_HATC, VV(2), D(50));
        break;
    }
    #undef M
    #undef D
    #undef CV
    #undef VV
}

#endif // TR808_H
