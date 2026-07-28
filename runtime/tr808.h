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

// ── instrument-slot layout: 16 slots (some voices share a slot) ──────────────
enum { TRS_BD, TRS_SDB, TRS_SDN, TRS_TOM, TRS_TOMN, TRS_CON, TRS_RS, TRS_CLV,
       TRS_CP, TRS_MAR, TRS_CB, TRS_CYT, TRS_HATO, TRS_HATC,
       TRS_CYM, TRS_CYH };          // cymbal MID + HIGH bands (see tr808_cym3) — appended, so no
                                    // existing TRS_* value moves
#define TR808_NSLOT 16
#define TR808_BASE   9        // tr808.c's base (slots 5..8 are user waves)

// ── CYMBAL: one band, or the schematic's THREE? (Synth Secrets plan 1.2 / audit §J5) ──────────
// Part 39 dissects the real TR-808 cymbal: the six enharmonic square oscillators are mixed, then
// "split into two bands by a pair of band-pass filters"; the LOWER band gets a VCA + AR contour (the
// panel DECAY rides this one), and "the upper band is further split into two signal paths that pass
// through independent VCAs controlled by their own contour generators. The upper of the two … has the
// shortest Decay." Three bands, three highpasses, three UNEQUAL decays, recombined by a mixer — the
// panel TONE control. And the payoff sentence: "this inequality of decay times allows the TR808 to
// change the mix of lower-, mid- and higher-frequency components AS THE SOUND PROGRESSES", which is
// exactly the spectral migration Part 37 says a real cymbal does (energy climbs, then the highs die
// first and the mids dominate the tail).
//
// RUNTIME, not a #define, and deliberately so: a compile-time gate can't be A/B'd by ear in the editor,
// and the whole point of this item is a judgement only ears can make. The two extra slots are always
// BUILT (they cost nothing unheard) and `tr808_cym3` only decides whether a crash FIRES them, so a cart
// can flip it mid-run — instrument_level is read live at mix, so even the balance can be swept live.
//   0 = as this header shipped — one slot, one highpass, one decay, so the timbre is static as it rings.
//   1 = the schematic's three bands. Costs 3× the voices per crash (9, from 3 bank members × 3 bands).
// Unequal decays need per-slot `decay_ms`; a per-hit gate length can't do it, because with sustain 0 the
// slot's decay governs the ring and the gate only ends an already-silent note. Hence real slots.
//
// ✅ DEFAULT IS NOW 1 — the owner's ear picked the three-band cymbal (2026-07-28), A/B'd against the stock
// voice as rendered WAVs. 0 is KEPT reachable (key C in the tr808 cart), not removed: it is the sound the
// cart shipped with for a year and a byte-identical render proves it is still exactly that.
static int tr808_cym3 = 1;

// The two colour bands' centre frequencies. Hoisted out of the build so they can be swept with
// ab-render, because where the TOP band sits is NOT a free choice. Swept on an isolated crash:
//     hi_hz   whole-file peak   centroid at strike     (low band alone reads 11846 Hz)
//     5000    -3.5 dBFS         11496 Hz   — no lift at all, the band is under the low one
//     6200    -3.5              12106
//     7000    -3.5              13758      ← sane; 7100 (the MEASURED 808 value) sits right here
//     7800    -0.0              15796      — runs away; see the aliasing note in the build below
// -3.5 dBFS is the boot KICK in that clip, i.e. at 7000 and below the cymbal stays under the loudest
// drum in the kit, which is where it should sit. 7800 pushes the crash past it and pins the ceiling.
//
// 7100 is not a tuning choice — it's the reverse-engineered 808's own upper cymbal bandpass, already
// recorded in tr808.c's docblock ("bandpasses at 7100/3440Hz") and never implemented until now. The
// sweep independently found ~7000 to be the highest corner that doesn't fold, which is a pleasing check.
//
// WHERE THIS DEPARTS FROM THE SCHEMATIC, on purpose: Reid has ONE upper bandpass (7100) feeding TWO
// VCAs with different decays, and a separate highpass after each VCA. A slot here carries exactly one
// filter, so the post-VCA highpasses get folded into the band frequency instead — two corners rather
// than one corner and two highpasses. Same three-unequal-decay structure, and it moves the spectrum
// MORE than stacking both upper paths on one corner would.
static int tr808_cym3_mid_hz = 5200;
static int tr808_cym3_hi_hz  = 7100;

// How far under the low band the two colour bands sit, in VELOCITY steps (tr808__vv clamps 0..7). This
// is the balance lever — deliberately velocity and not instrument_level, because acidcandy and dubjam
// use instrument_level as their per-slot MIXER (looping `i < TR808_NSLOT` from one fader) and would
// silently overwrite any level a band set for itself. Velocity rides on the hit, so nothing can clobber it.
// It is a CLIFF, not a fader, and that is the honest limit of this implementation. tr808__vv clamps
// 0..7, and the cymbal already fires at ~2, so with boost 1 an offset of -4 lands at -1 → clamped to
// 0 → silent. Swept at the final band settings (stock crash = -14.0 dBFS, flat at ~11850 Hz):
//     vel    strike peak    centroid walk over the first 300ms
//     -3     -7.2 dBFS      14895 → 12929 → 11844 Hz   ← chosen: the quietest setting that still sounds
//     -4     -14.0          11512 → 11882 → 11824      — bit-identical to stock: the bands are SILENT
//     -5/-6  identical to -4 (still clamped)
// So the usable range is 0..-3 and the three-band crash cannot be fully level-matched: it lands ~6.8dB
// hotter at the strike than the one-band version. Both other levers are dead ends — instrument_level
// collides with the carts' per-slot mixers (see the build note) and velocity runs out after 4 steps. If
// the ear likes this cymbal, the proper fix is to make acidcandy/dubjam's mixer loops scale relative to
// a per-slot base instead of setting it absolutely, and then trim these two slots with instrument_level.
// Not done yet on purpose: that touches three carts' mixers for a feature still defaulting to OFF.
static int tr808_cym3_vel = -3;

// How many of the three bank members each COLOUR band gets (1 or 3). The low band always gets all three,
// as it always did. 3 is the default because it is the version the owner's ear approved (2026-07-28).
//
// 1 is a real alternative and measured equivalent, kept because voice economy is a live concern in this
// cart (tr808.c's docblock: it fires "2-3 squares per hit (full six would eat the 8-voice polyphony)"):
//     members / vel    strike peak    centroid walk over the first 300ms      voices per crash
//     3 / -3           -7.2 dBFS      14895 → 12929 → 11844 Hz                9   ← shipped (approved)
//     1 / -1           -6.8           14843 → 12906 → 11825                   5
//     1 /  0           -4.6           15619 → 13248 → 11811                   5   (widest walk)
//     1 / -2           -9.7           13584 → 12466 → 11833                   5
// So 1/-1 matches the shipped level and walk within 0.4dB and ~30Hz for 4 fewer voices. It is NOT the
// default anyway, because the numbers are equal but the sound is not identical: with one member the
// colour bands are a single pitch instead of three enharmonic ones beating against each other, and
// substituting that for the take that was actually listened to would be a silent change of the verdict.
// Switch to 1 if voice pressure ever bites. Note vel bottoms out one step earlier here (-3 goes silent,
// since a lone member fires at VV(2) and the clamp is at 0).
static int tr808_cym3_members = 3;

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
    // …plus the schematic's upper two bands (§J5). Higher corner ⇒ shorter decay: the highs die first,
    // so the surviving spectrum walks downward through the tail instead of holding still. Always built;
    // tr808_cym3 decides whether a crash fires them.
    // Resonance 3, same as the low band. Do NOT lower it thinking that tames the peak: this filter
    // DRAINS level as resonance rises (the documented per-res bass drain, tools/filter-spec.js), so
    // dropping these to res 1 made the strike LOUDER, not quieter — the opposite of the intent.
    // FILTER_BAND, not FILTER_HIGH, and this is the one decision here worth reading twice. Reid's text
    // says the mix is "split into two bands by a pair of BAND-PASS filters", and taking him literally
    // also fixes a real artifact: a HIGHPASS at 7800 passes everything above 7800, which on INSTR_SQUARE
    // means it passes the oscillator's ALIASING. Measured as a stem (play.js --solo-slot 24) that band
    // came out at -0.0 dBFS — 14dB louder than the low band it was supposed to colour, clipping on its
    // own, with a spectral centroid of 21942 Hz. Nyquist is 22050. It was not a cymbal band at all, it
    // was a fold-over amplifier. A bandpass rolls off above the corner too, so the junk near Nyquist
    // stays out and the band lands at a sane level with no trim.
    //
    // That is also why there is no instrument_level trim here, which was the first fix attempted and is
    // REJECTED on top of being unnecessary: acidcandy and dubjam use instrument_level as their per-slot
    // MIXER, looping `i < TR808_NSLOT` to drive every slot from one fader, which would silently overwrite
    // any internal balance a band set for itself. Velocity can't carry it either (tr808__vv is an int
    // 0..7 and the cymbal already fires at ~2). The band levels have to come out right by construction.
    instrument(base + TRS_CYM, INSTR_SQUARE, 3, 420, 0, 120);   // mid — "near the centre of the range"
    instrument_filter(base + TRS_CYM, FILTER_BAND, tr808_cym3_mid_hz, 3);
    instrument(base + TRS_CYH, INSTR_SQUARE, 2, 150, 0, 50);    // high — "the shortest Decay"
    instrument_filter(base + TRS_CYH, FILTER_BAND, tr808_cym3_hi_hz, 3);
    // The low band is left exactly as it shipped: it alone survives past ~1.2s, so it IS the tail, and
    // touching it (a 0.62 trim on the first attempt) quietened the shipped tail by 4.2dB — an audible
    // change to the part of the sound this item is not about.

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
        if (tr808_cym3) {
            // the SAME members into the upper two bands — the migration comes from one source decaying
            // at three rates, not from three different tones (which would just be three sounds)
            int cvq = tr808_cym3_vel;   // how far the colour bands sit UNDER the low band, in velocity steps
            schedule_hit(delay, M(72), base + TRS_CYM, VV(2 + cvq), D(450));
            schedule_hit(delay, M(72), base + TRS_CYH, VV(2 + cvq), D(170));
            if (tr808_cym3_members == 3) {
                schedule_hit(delay, M(79), base + TRS_CYM, VV(CV(0, 6) + cvq), D(450));
                schedule_hit(delay, M(66), base + TRS_CYM, VV(CV(5, 0) + cvq), D(450));
                schedule_hit(delay, M(79), base + TRS_CYH, VV(CV(0, 6) + cvq), D(170));
                schedule_hit(delay, M(66), base + TRS_CYH, VV(CV(5, 0) + cvq), D(170));
            }
        }
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

// set voice v's stereo PAN (-1 L .. 0 centre .. +1 R) by panning EVERY output slot it
// fires. Toms (LT/MT/HT) and congas (LC/MC/HC) each share one slot, so those voices pan
// as a GROUP — hardware-honest (the real 808's tom/conga voice circuit is shared). SEAM:
// independent tom/conga pan would need the 14-slot bank split into per-voice slots.
static void tr808_pan(int base, int v, float pan) {
    switch (v) {
    case TR_BD:  instrument_pan(base + TRS_BD,  pan); break;
    case TR_SD:  instrument_pan(base + TRS_SDB, pan); instrument_pan(base + TRS_SDN,  pan); break;
    case TR_LT: case TR_MT: case TR_HT:
                 instrument_pan(base + TRS_TOM, pan); instrument_pan(base + TRS_TOMN, pan); break;
    case TR_LC: case TR_MC: case TR_HC:
                 instrument_pan(base + TRS_CON, pan); break;
    case TR_RS:  instrument_pan(base + TRS_RS,  pan); break;
    case TR_CLV: instrument_pan(base + TRS_CLV, pan); break;
    case TR_CP:  instrument_pan(base + TRS_CP,  pan); break;
    case TR_MA:  instrument_pan(base + TRS_MAR, pan); break;
    case TR_CB:  instrument_pan(base + TRS_CB,  pan); break;
    case TR_CY:  instrument_pan(base + TRS_CYT, pan); break;
    case TR_OH:  instrument_pan(base + TRS_HATO, pan); break;
    case TR_CH:  instrument_pan(base + TRS_HATC, pan); break;
    }
}

// FINE-tune voice v by `semis` (fractional semitones, live) across its output slots — the
// microtune trim that lets two kicks be nulled against each other. Same slot map as tr808_pan;
// toms/congas share a slot so they fine-tune as a GROUP.
static void tr808_tune(int base, int v, float semis) {
    switch (v) {
    case TR_BD:  instrument_tune(base + TRS_BD,  semis); break;
    case TR_SD:  instrument_tune(base + TRS_SDB, semis); instrument_tune(base + TRS_SDN,  semis); break;
    case TR_LT: case TR_MT: case TR_HT:
                 instrument_tune(base + TRS_TOM, semis); instrument_tune(base + TRS_TOMN, semis); break;
    case TR_LC: case TR_MC: case TR_HC:
                 instrument_tune(base + TRS_CON, semis); break;
    case TR_RS:  instrument_tune(base + TRS_RS,  semis); break;
    case TR_CLV: instrument_tune(base + TRS_CLV, semis); break;
    case TR_CP:  instrument_tune(base + TRS_CP,  semis); break;
    case TR_MA:  instrument_tune(base + TRS_MAR, semis); break;
    case TR_CB:  instrument_tune(base + TRS_CB,  semis); break;
    case TR_CY:  instrument_tune(base + TRS_CYT, semis); break;
    case TR_OH:  instrument_tune(base + TRS_HATO, semis); break;
    case TR_CH:  instrument_tune(base + TRS_HATC, semis); break;
    }
}

#endif // TR808_H
