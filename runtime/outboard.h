// outboard.h — the shared ANALOG OUTBOARD CHAIN: a mix-bus output stage as a voicing table.
//
// The ampcab.h move (docs/design/effects-bus-architecture.md §E), applied to the MASTER bus
// instead of a guitar cabinet: an outboard unit is a PRESET BUNDLE of effects the engine already
// ships, NOT new DSP (decision 0015). This header is the ONE source of truth for the four stages,
// so every cart and app that pins "the analog output chain" agrees on what that sounds like.
// Full honesty ledger (what of the classic-outboard promise we can and cannot back):
// docs/design/analog-outboard-chain.md. Include AFTER studio.h.
//
//   EQ     the console EQ        eq_inst(0,…)   + FX_EQ                three fixed bands, ±12 dB
//   IRON   the saturation stage  drive_insert() + FX_DRIVE             DRIVE_ASYM = odd AND even harmonics
//   COMP   the FET bus comp      eq_inst(1,…) input gain + glue()      ratio = a bundle, see below
//   PLATE  the plate send        reverb() + instrument_reverb(slot,…)  parallel, not in the chain
//
// TWO things about this that are easy to get wrong, both recorded in the doc:
//
//  1. THE COMP IS PINNED LAST. sc_apply() runs on bus 0 AFTER the whole fx_order chain and before
//     the soft-clip, so the achievable order is EQ → IRON → COMP, never comp-first. That is a real
//     console order (EQ into the bus comp), but do not write copy claiming the other one.
//  2. A FET COMP HAS NO THRESHOLD, so `glue` having none is not the gap it looks like. You compress
//     an 1176 by driving its INPUT. That is what ObRatio + the instance-1 EQ boost are: INPUT and a
//     ratio BUTTON, which is the whole front panel. But be precise about what glue then DOES:
//     MEASURED on the `outboard` cart's loop, the ALL ratio drops RMS by ~3.8 dB while the PEAK
//     stays at the ceiling and the crest factor goes UP. glue is a fast-attack, slow-release
//     DUCKER: it squashes the body and lets the transient through, which is the pumping "breathes
//     as one lump" character it was built for, and it is NOT peak control. It also has no makeup
//     gain, so switching COMP in costs level you cannot get back. Do not sell it as a limiter.
//
// SET-AND-HOLD is the CALLER's job: call outboard_apply() only when a toggle or knob actually
// CHANGED, never every frame (reconfiguring eq/drive/glue per frame churns the audio thread into a
// stutter — tools/lint-fx-frame.js).
//
// Every stage bypasses to a BYTE-IDENTICAL null (eq 0/0/0, drive amount 0, glue amount 0, sends 0),
// which is what makes a cart's stage toggles a real A/B instead of an approximate one. MEASURED,
// not assumed (two renders of the `outboard` cart, diffed sample by sample): EQ, IRON and COMP
// return to bit-exact on the SAME SAMPLE the switch flips; PLATE returns to bit-exact ~3.5s later
// at the default size, because a reverb tail is real and does not vanish when you stop sending to
// it. One trap found by that measurement: fx_order() must be set UNCONDITIONALLY (see below). A
// chain assembled from only the ENABLED stages measured a 2.1s trailing divergence after bypass.

#ifndef DE_OUTBOARD_H
#define DE_OUTBOARD_H

#include "studio.h"

// ── the FET comp's RATIO buttons ──────────────────────────────────────────────
// One button position = one (amount, attack, release, dirt) tuple. VOICED after the device, not
// derived from it: higher ratios squash harder, grab faster, recover faster and run dirtier, which
// is the direction a real unit moves (all-buttons-in is its dirty setting too). `dirt` rides the
// IRON stage up so the comp brings its own colour even with IRON's own knob low.
typedef struct {
    const char *name;
    float amount;      // glue amount 0..1
    int   atk_ms;
    int   rel_ms;
    float dirt;        // extra drive_insert amount this ratio adds
} ObRatio;

#define OB_RATIO_N 4
static const ObRatio OB_RATIO[OB_RATIO_N] = {
    { "4:1",  0.28f, 14, 280, 0.03f },   // gentle glue, the mix breathes as one lump
    { "8:1",  0.44f,  7, 190, 0.06f },
    { "12:1", 0.60f,  3, 120, 0.10f },
    { "ALL",  0.84f,  1,  55, 0.17f },   // all-buttons-in: squashed, fast, dirty
};

// ── the console EQ curves ─────────────────────────────────────────────────────
// Base curve in dB for the three FIXED bands (<80 Hz / 80 Hz-6 kHz / >6 kHz). The stage's own knob
// scales the whole curve, so 0 = flat = byte-identical bypass and 1 = the full voicing.
typedef struct { const char *name; float lo, mid, hi; } ObCurve;

#define OB_CURVE_N 3
static const ObCurve OB_CURVE[OB_CURVE_N] = {
    { "WARM",  5.0f, -2.0f,  2.0f },   // weight down low, mids out of the way, a little sheen
    { "AIR",   1.0f, -1.0f,  7.0f },   // the top-end lift
    { "SCOOP", 3.0f, -7.0f,  3.0f },   // the smiley curve: ends up, middle gone
};

// ── the plate voicing ─────────────────────────────────────────────────────────
// A plate is dense, bright and has no room geometry, so it wants LOW damping (a bright tail) and a
// mid-to-long size. NOTE: the tank is MONO today, which is the one place this chain cannot deliver
// the "lush" of a real plate — see the doc §6 for the reverb_plate() sibling that would.
#define OB_PLATE_DAMP 0.30f
#define OB_PLATE_SIZE_MIN 0.45f
#define OB_PLATE_SIZE_RANGE 0.40f

// which slots feed the plate, and how hard. Reverb is a SEND bus, so the caller names its own
// voices; a drum machine typically sends the snare/clap a lot and the kick none at all.
typedef struct { int slot; float send; } ObSend;

// ── the rack's state ──────────────────────────────────────────────────────────
// Four stage on/off flags plus one 0..1 knob each, and the two selector positions. A cart owns one
// of these (put it in its persistent state) and hands it to outboard_apply() on every change.
typedef struct {
    int   eq_on;      float eq_amt;    int curve;   // 0..OB_CURVE_N-1
    int   iron_on;    float iron_amt;
    int   comp_on;    float comp_in;   int ratio;   // comp_in 0..1 → up to +12 dB into the comp
    int   plate_on;   float plate_amt;
} Outboard;

// sensible defaults: everything OFF, so a fresh rack is bit-identical to no rack at all.
static Outboard outboard_default(void) {
    Outboard o;
    o.eq_on = 0;    o.eq_amt    = 0.80f;  o.curve = 0;
    o.iron_on = 0;  o.iron_amt  = 0.42f;
    o.comp_on = 0;  o.comp_in   = 0.40f;  o.ratio = 1;
    o.plate_on = 0; o.plate_amt = 0.55f;
    return o;
}

// Apply the whole rack. `sends` names the slots that feed the plate (may be NULL/0 for none).
// Call ONLY on change. Bypassed stages are both dropped from the chain AND written to their null
// values, so a toggle is a true byte-identical A/B.
static void outboard_apply(const Outboard *o, const ObSend *sends, int n_sends) {
    const ObRatio *r = &OB_RATIO[(o->ratio < 0 || o->ratio >= OB_RATIO_N) ? 0 : o->ratio];
    const ObCurve *c = &OB_CURVE[(o->curve < 0 || o->curve >= OB_CURVE_N) ? 0 : o->curve];

    // EQ — instance 0 is the console curve, scaled by its own knob.
    if (o->eq_on) eq_inst(0, c->lo * o->eq_amt, c->mid * o->eq_amt, c->hi * o->eq_amt);
    else          eq_inst(0, 0.0f, 0.0f, 0.0f);

    // IRON — the asymmetric saturation stage. The comp's ratio adds its own dirt on top, so the
    // FET's colour arrives with the FET and not only when you reach for this knob.
    float dirt = (o->iron_on ? o->iron_amt * 0.55f : 0.0f) + (o->comp_on ? r->dirt : 0.0f);
    if (dirt > 0.0005f) drive_insert(dirt, DRIVE_ASYM, 1.0f);
    else                drive_insert(0.0f, DRIVE_ASYM, 0.0f);

    // COMP — instance 1 EQ is the INPUT knob (flat boost, the only way to gain up), then the pinned
    // glue stage. No threshold, by design: this is how a FET unit is driven.
    if (o->comp_on) {
        float g = o->comp_in * 12.0f;            // 0..+12 dB into the comp
        eq_inst(1, g, g, g);
        glue(0, r->amount, r->atk_ms, r->rel_ms);
    } else {
        eq_inst(1, 0.0f, 0.0f, 0.0f);
        glue(0, 0.0f, 8, 150);
    }

    // The chain: EQ → IRON → the comp's input gain, then the pinned comp, then the soft-clip.
    // Set UNCONDITIONALLY, with every stage listed whether it is in or out, because a bypassed
    // stage is already nulled and a stable chain is what makes the A/B a true one. NOTE that this
    // means outboard_apply() OWNS the master bus's insert order: a cart that also wants (say) a
    // chorus on the master must place it itself, in a chain that includes these three.
    static const int chain[3] = { FX_EQ, FX_DRIVE, FX_INST(FX_EQ, 1) };
    fx_order(0, chain, 3);

    // PLATE — a parallel send, so the stage is the per-slot send amounts, not a chain slot.
    if (o->plate_on) reverb(OB_PLATE_SIZE_MIN + OB_PLATE_SIZE_RANGE * o->plate_amt, OB_PLATE_DAMP);
    for (int i = 0; i < n_sends; i++)
        instrument_reverb(sends[i].slot, o->plate_on ? sends[i].send * o->plate_amt : 0.0f);
}

// one-line stage name/label helpers for a UI that wants to stay in step with the table above
static const char *ob_ratio_name(int i) { return OB_RATIO[(i < 0 || i >= OB_RATIO_N) ? 0 : i].name; }
static const char *ob_curve_name(int i) { return OB_CURVE[(i < 0 || i >= OB_CURVE_N) ? 0 : i].name; }

#endif
