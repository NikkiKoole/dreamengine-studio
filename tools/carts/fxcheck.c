#include "studio.h"
#include <stdio.h>

// FXCHECK — the engine's EFFECTS stability self-test cart. The twin of tunecheck.c
// (pitch) / the level sweep: it drives a loud sustained chord into the master bus and,
// one effect at a time, configures THAT effect at its DOCUMENTED EXTREME (max feedback /
// resonance / depth — the settings most likely to blow up), so `node tools/fx-check.js`
// can render it and assert the output stays FINITE and BOUNDED — no NaN-collapse, no DC
// runaway, no silent self-oscillation. This is a stability gate, not a character gate:
// whether an effect SOUNDS right is still by ear; this proves it doesn't break.
//
//   node tools/fx-check.js            # render this cart + report peak/rms/dc/clip per effect
//
// The SCHEDULE below is the source of truth. Each test window: at frame 0 reset ALL
// effects off (set-and-hold — once, never per-frame) then enable the one under test, and
// start a sustained chord; release it before a long gap so reverb/echo tails die before
// the next test. Every frame it watch()es the test index + a gate, so the analyzer reads
// ground truth from the trace. Test 0 is DRY (no effect) — the reference the others are
// compared against (an effect that doesn't move the signal off DRY is a dead/unwired fx).
//
// Pure timing harness — nothing to look at; run it headless via fx-check.js.

#define NF   56     // ~0.93s sounding @ 60fps (analyzer measures the stable middle)
#define GAP  28     // ~0.47s silence — long enough for a max-feedback reverb/echo tail to die
#define PER  (NF + GAP)
#define SLOT 5

// the master effects worth fuzzing at extremes. INSERTS process the whole mix (just call
// the config fn); SENDS (reverb/echo) also need a per-slot send level to be fed anything.
#define NTESTS 19   // 0=DRY, 1..12 = one effect each, 13..18 = STACKS (keep in sync with fx-check.js FX_NAMES)

static int fnum = -1;
static int h0 = -1, h1 = -1, h2 = -1;

// reset EVERY effect to its off/neutral state (mix/depth/send = 0). Called once at the
// start of each test window so the effects never bleed into each other.
static void fx_all_off(void) {
    reverb(0, 0);                       instrument_reverb(SLOT, 0);
    echo(1, 0, 0);                      instrument_echo(SLOT, 0);
    chorus(1.5f, 0, 0);
    flanger(0.3f, 0, 0, 0);
    tape(0, 0, 0);
    wah_lfo(2, 0, 0);
    crush(16, 1, 0);
    eq(0, 0, 0);
    tremolo(4, 0, LFO_SHAPE_SINE);
    phaser(1, 0, 0, 0, 4);
    filter(FILTER_OFF, 1000, 0);
    echo_insert(1, 0, 0, 0);            // the reorderable INSERT versions (stacks use these)
    reverb_insert(0, 0, 0);
    drive_insert(0, DRIVE_SOFT, 0);
}

// STACK chains (indices 13+): fx_order(0,…) sets the master insert chain to EXACTLY these kinds
// (omitted effects drop out), so each stack fully controls its own chain + order.
static const int STK_MASTER[]  = { FX_DRIVE, FX_EQ, FX_CRUSH, FX_TAPE };   // lo-fi mastering chain (gain compounding)
static const int STK_COMBS[]   = { FX_FLANGER, FX_PHASER };                // two resonant combs in series (worst-case stability)
static const int STK_DELAYS[]  = { FX_ECHO, FX_REVERB };                   // two long feedback tails stacked
static const int STK_DRY_REV[] = { FX_DRIVE, FX_REVERB };                  // drive → space (order A)
static const int STK_REV_DRY[] = { FX_REVERB, FX_DRIVE };                  // space → drive (order B — same effects, reversed)
static const int STK_KITCHEN[] = { FX_DRIVE, FX_CHORUS, FX_FLANGER, FX_EQ, FX_CRUSH, FX_TAPE, FX_ECHO, FX_REVERB };

// enable exactly ONE effect at its documented EXTREME (the worst case for stability).
static void fx_enable(int t) {
    switch (t) {
        case 1:  reverb(1.0f, 0.0f); instrument_reverb(SLOT, 1.0f);   break; // endless bright hall (max tail)
        case 2:  echo(500, 1.1f, 1.0f); instrument_echo(SLOT, 1.0f);  break; // FEEDBACK > 1.0 — the runaway case
        case 3:  chorus(5.0f, 1.0f, 1.0f);                            break; // max rate/depth/wet
        case 4:  flanger(5.0f, 1.0f, 0.95f, 1.0f);                    break; // max + feedback (jet/metallic)
        case 5:  flanger(0.05f, 1.0f, -0.95f, 1.0f);                  break; // through-zero, max - feedback
        case 6:  tape(1.0f, 1.0f, 1.0f);                              break; // max wow/flutter/saturation
        case 7:  wah_lfo(10.0f, 1.0f, 1.0f);                          break; // max resonance quack
        case 8:  crush(1.0f, 0.05f, 1.0f);                            break; // 1-bit + heavy downsample, full wet
        case 9:  eq(15.0f, 15.0f, 15.0f);                             break; // +15 dB all bands (slams the limiter)
        case 10: tremolo(20.0f, 1.0f, LFO_SHAPE_SQUARE);              break; // max-rate hard chop
        case 11: phaser(10.0f, 1.0f, 0.95f, 1.0f, 8);                 break; // max feedback, 8 stages
        case 12: filter(FILTER_BAND, 300.0f, 0.99f);                  break; // near self-oscillation (resonant scream)
        // ── STACKS — multiple effects chained; each sets its own fx_order ──
        case 13: eq(10, 6, 10); crush(6, 1, 0.6f); tape(0.4f, 0.3f, 0.7f); drive_insert(0.6f, DRIVE_SOFT, 1.0f);
                 fx_order(0, STK_MASTER, 4);                          break; // drive→eq→crush→tape (gain + tone compound)
        case 14: flanger(0.3f, 0.8f, 0.9f, 1.0f); phaser(0.5f, 0.9f, 0.9f, 1.0f, 6);
                 fx_order(0, STK_COMBS, 2);                           break; // two resonant combs in series
        case 15: echo_insert(400, 0.85f, 0.6f, 0.7f); reverb_insert(0.9f, 0.3f, 0.6f);
                 fx_order(0, STK_DELAYS, 2);                          break; // two long feedback tails
        case 16: drive_insert(0.8f, DRIVE_HARD, 1.0f); reverb_insert(0.8f, 0.4f, 0.7f);
                 fx_order(0, STK_DRY_REV, 2);                         break; // drive → reverb (order A)
        case 17: drive_insert(0.8f, DRIVE_HARD, 1.0f); reverb_insert(0.8f, 0.4f, 0.7f);
                 fx_order(0, STK_REV_DRY, 2);                         break; // reverb → drive (order B — must also stay bounded)
        case 18: drive_insert(0.5f, DRIVE_SOFT, 0.8f); chorus(2, 0.5f, 0.5f); flanger(0.3f, 0.5f, 0.5f, 0.5f);
                 eq(4, 2, 4); crush(8, 1, 0.4f); tape(0.3f, 0.2f, 0.5f); echo_insert(300, 0.5f, 0.5f, 0.5f);
                 reverb_insert(0.6f, 0.4f, 0.5f); fx_order(0, STK_KITCHEN, 8); break; // kitchen sink (deep chain)
        default: break;                                                      // 0 = DRY (reference)
    }
}

void init(void) {
    instrument(SLOT, INSTR_SAW, 4, 40, 7, 200);   // bright/broadband + sustained = stresses every effect
}

void update(void) {
    fnum++;
    int idx   = fnum / PER;
    int local = fnum % PER;
    int gate  = (idx < NTESTS) && (local < NF);

    if (idx < NTESTS) {
        if (local == 0) {
            fx_all_off();          // isolate: clear everything, then turn on just this one
            fx_enable(idx);
            h0 = note_on(50, SLOT, 7);   // a loud sustained chord keeps the bus fed (worst case for feedback)
            h1 = note_on(57, SLOT, 6);
            h2 = note_on(62, SLOT, 6);
        } else if (local == NF - 1) {
            if (h0 >= 0) note_off(h0);
            if (h1 >= 0) note_off(h1);
            if (h2 >= 0) note_off(h2);
            h0 = h1 = h2 = -1;
        }
    }

#ifdef DE_TRACE
    watch("fx",   "%d", idx < NTESTS ? idx : -1);   // which effect test (0 = dry)
    watch("gate", "%d", gate);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("FXCHECK - run via tools/fx-check.js", 6, 6, CLR_WHITE);
    char buf[48];
    int idx = fnum < 0 ? 0 : fnum / PER;
    snprintf(buf, sizeof(buf), "test %d / %d", idx, NTESTS);
    print(buf, 6, 20, CLR_LIGHT_GREY);
}
