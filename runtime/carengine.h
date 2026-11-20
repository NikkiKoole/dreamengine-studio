// carengine.h — a car-engine SOUND voice, cart-land (ADR-0006).
//
// Extracted from enginelab (the dyno/workbench, still its tuning bench) when a second consumer
// appeared: backroads drives it from its car physics. The model, from enginelab's header:
// an engine is a FIRING RATE, not a note. A 4-stroke fires (cyl/2) times per rev, so the "engine
// note" is the firing frequency: ~15 Hz of distinct thuds at idle climbing to ~150 Hz of buzz at
// redline. One master number (chug_hz) drives every layer so they stay harmonically locked:
//   BODY    a saw at the firing freq through a resonant lowpass (each edge pings = one puff)
//   FIRING  an LFO_VOLUME tremolo AT chug_hz: the chug that speeds up with the revs
//   RASP    a thin-pulse square an octave up, leaning in ON the throttle (the engine "breathes")
//   INTAKE  air rush / turbo whistle / hum / diesel clatter / EV overtone, per kind
//   DRIVE   saturation that climbs with load (the snarl)
//   LOPE    a slow Perlin wobble on the idle so it loiters
//   BURBLE  lift off the gas at high revs → overrun crackle
// Seven kinds (CE_ELECTRIC … CE_TRACTOR), each one recipe.
//
// CONTRACT
//   CarEngine ce; carengine_init(&ce, first_slot);   // uses instrument slots first_slot .. +3
//   ce.kind = CE_GAS;                                 // switch any time (voices rebuild)
//   every frame: carengine_update(&ce, rpm 0..1, throttle 0..1, audible);   audible 0 = silent
//   ce.layer_on[CE_L_*] toggles a layer (all on after init).
// Uses held voices (note_on/note_pitch/note_vol/note_cutoff/note_drive/note_lfo every frame:
// note_* are built to be ridden live) and hit() for the burble pops (consumes the cart RNG via
// chance()/rnd_between(), in the same order enginelab always did). Include AFTER studio.h.
#ifndef CARENGINE_H
#define CARENGINE_H
#include "studio.h"
#include <math.h>

enum { CE_ELECTRIC, CE_GAS, CE_DIESEL, CE_STEAM, CE_RACE, CE_NUCLEAR, CE_TRACTOR, CE_KINDS };
static const char *CE_NAME[CE_KINDS] = {
    "ELECTRIC", "GAS", "DIESEL", "STEAM", "RACE", "NUCLEAR", "TRACTOR" };
static const char *CE_FEEL[CE_KINDS] = {
    "pure rising whine, no chug",
    "revvy mid, throttle rasp",
    "low clatter, lumpy lope",
    "slow chuff puffs",
    "screaming wail + turbo",
    "steady sci-fi hum",
    "potato-potato grunt" };

enum { CE_L_FIRING, CE_L_RASP, CE_L_INTAKE, CE_L_DRIVE, CE_L_LOPE, CE_L_BURBLE, CE_L_TURBO, CE_L_COUNT };
static const char *CE_L_NAME[CE_L_COUNT] = {
    "FIRING", "RASP", "INTAKE", "DRIVE", "LOPE", "BURBLE", "TURBO" };

// per-kind voice recipe. chugIdle/chugRed: firing Hz at idle vs redline (the master sweep).
typedef struct {
    int   bodyWave, bodyFilt;        // INSTR_* + FILTER_*
    float chugIdle, chugRed;         // firing Hz across the rev range
    int   bodyOct;                   // semitone offset of the body tone
    int   cutBase, cutSpan, res;     // body filter sweep + resonance
    float volIdle;                   // body volume (0..7) at idle — combustion idles audibly; electric is ~silent at rest
    float tremDepth;                 // firing-rate tremolo depth (0 = electric, no chug)
    float driveBase, driveSpan; int driveMode;
    int   raspOn, raspOct; float raspDuty;   // exhaust rasp (square, throttle-led)
    int   intakeMode;                // 0 none · 1 noise rush · 2 turbo whistle · 3 hum/shimmer · 4 clatter · 5 EV overtone whine
    int   burbleOn;                  // overrun crackle on lift
    float lopeAmt;                   // idle-lope wobble strength
    float detune;                    // body slot detune (shimmer/beating); 0 = none
} CERecipe;

static CERecipe carengine_recipe(int k) {
    CERecipe e;
    // shared defaults
    e.bodyWave = INSTR_SAW; e.bodyFilt = FILTER_LOW; e.bodyOct = 0;
    e.res = 6; e.volIdle = 3; e.tremDepth = 0.5f; e.driveBase = 0.1f; e.driveSpan = 0.4f;
    e.driveMode = DRIVE_ASYM; e.raspOn = 0; e.raspOct = 12; e.raspDuty = 0.18f;
    e.intakeMode = 0; e.burbleOn = 0; e.lopeAmt = 0.5f; e.detune = 0;
    switch (k) {
    case CE_ELECTRIC:                                   // no combustion → no chug, no idle, no grit: a clean rising whine
        e.bodyWave = INSTR_SINE; e.bodyOct = 12;        // pure tone — TRI/detune was buzzy & "labored"
        e.chugIdle = 110; e.chugRed = 720;              // a WIDE, effortless zoom up
        e.cutBase = 1200; e.cutSpan = 5000; e.res = 1;  // wide open, almost no resonance → smooth
        e.volIdle = 0;                                  // SILENT at rest (an EV makes no noise stopped); whines up with speed
        e.tremDepth = 0; e.driveBase = 0; e.driveSpan = 0;
        e.intakeMode = 5; e.lopeAmt = 0; e.detune = 0;  // a clean octave-up overtone whine, no beating
        break;
    case CE_DIESEL:                                     // low, hard, clattery, lumpy at idle
        e.chugIdle = 11; e.chugRed = 58;
        e.bodyOct = 0; e.cutBase = 150; e.cutSpan = 560; e.res = 8;
        e.tremDepth = 0.85f; e.driveBase = 0.4f; e.driveSpan = 0.3f; e.driveMode = DRIVE_HARD;
        e.raspOn = 1; e.raspOct = 7; e.raspDuty = 0.12f;
        e.intakeMode = 4; e.lopeAmt = 0.85f;
        break;
    case CE_STEAM:                                      // slow chuff — gated noise puffs
        e.bodyWave = INSTR_NOISE; e.bodyFilt = FILTER_BAND;
        e.chugIdle = 3; e.chugRed = 13;
        e.cutBase = 220; e.cutSpan = 360; e.res = 7;
        e.tremDepth = 1.0f; e.driveBase = 0; e.driveSpan = 0;   // full gate = distinct puffs
        e.intakeMode = 0; e.lopeAmt = 0.35f;
        break;
    case CE_RACE:                                       // wide, bright, screaming + turbo
        e.chugIdle = 26; e.chugRed = 150;
        e.cutBase = 420; e.cutSpan = 2400; e.res = 5;
        e.tremDepth = 0.42f; e.driveBase = 0.3f; e.driveSpan = 0.5f; e.driveMode = DRIVE_ASYM;
        e.raspOn = 1; e.raspOct = 12; e.raspDuty = 0.16f;
        e.intakeMode = 2; e.burbleOn = 1; e.lopeAmt = 0.2f;     // anti-lag crackle
        break;
    case CE_NUCLEAR:                                    // steady mid hum, faint shimmer, no lope
        e.bodyWave = INSTR_TRI;
        e.chugIdle = 44; e.chugRed = 190;
        e.cutBase = 320; e.cutSpan = 980; e.res = 4;
        e.tremDepth = 0.1f; e.driveBase = 0.08f; e.driveSpan = 0.1f; e.driveMode = DRIVE_SOFT;
        e.intakeMode = 3; e.lopeAmt = 0.12f; e.detune = 0.09f;
        break;
    case CE_TRACTOR:                                    // very slow, heavy, max lope (potato-potato)
        e.chugIdle = 6; e.chugRed = 26;
        e.cutBase = 120; e.cutSpan = 440; e.res = 9;
        e.tremDepth = 1.0f; e.driveBase = 0.5f; e.driveSpan = 0.25f; e.driveMode = DRIVE_HARD;
        e.raspOn = 1; e.raspOct = 7; e.raspDuty = 0.1f;
        e.intakeMode = 4; e.lopeAmt = 1.0f;
        break;
    case CE_GAS: default:                               // the revvy everyday mid
        e.chugIdle = 19; e.chugRed = 116;
        e.cutBase = 300; e.cutSpan = 1400; e.res = 6;
        e.tremDepth = 0.55f; e.driveBase = 0.12f; e.driveSpan = 0.45f; e.driveMode = DRIVE_ASYM;
        e.raspOn = 1; e.raspOct = 12; e.raspDuty = 0.18f;
        e.intakeMode = 1; e.burbleOn = 1; e.lopeAmt = 0.5f;
        break;
    }
    return e;
}

typedef struct {
    int   kind;                              // CE_*
    int   slot_body, slot_rasp, slot_intake, slot_pop;
    int   vBody, vRasp, vIntake;             // held voices (-1 = none)
    int   builtKind;                         // which kind the held voices are configured for
    float prev_thr, burble_t, pop_t;         // overrun-crackle state
    int   last_chug_hz;                      // gate note_lfo pushes to when the rate moved
    unsigned char layer_on[CE_L_COUNT];
} CarEngine;

static float carengine_hz_to_midi(float hz) { return 69.0f + 12.0f * log2f(hz / 440.0f); }

static void carengine_init(CarEngine *ce, int first_slot) {
    ce->kind = CE_GAS;
    ce->slot_body = first_slot; ce->slot_rasp = first_slot + 1;
    ce->slot_intake = first_slot + 2; ce->slot_pop = first_slot + 3;
    ce->vBody = ce->vRasp = ce->vIntake = -1;
    ce->builtKind = -1;
    ce->prev_thr = 0; ce->burble_t = 0; ce->pop_t = 0; ce->last_chug_hz = -1;
    for (int i = 0; i < CE_L_COUNT; i++) ce->layer_on[i] = 1;
}

// release the held voices (the next audible update rebuilds them)
static void carengine_kill(CarEngine *ce) {
    if (ce->vBody   >= 0) { note_off(ce->vBody);   ce->vBody   = -1; }
    if (ce->vRasp   >= 0) { note_off(ce->vRasp);   ce->vRasp   = -1; }
    if (ce->vIntake >= 0) { note_off(ce->vIntake); ce->vIntake = -1; }
    ce->builtKind = -1;
}

static void carengine_build(CarEngine *ce, CERecipe e) {
    // BODY — the core firing voice
    instrument(ce->slot_body, e.bodyWave, 30, 0, 7, 220);
    instrument_filter(ce->slot_body, e.bodyFilt, e.cutBase, e.res);
    instrument_drive_mode(ce->slot_body, e.driveMode);
    instrument_tune(ce->slot_body, 0);
    // RASP — thin pulse, an octave up; throttle-led exhaust edge
    instrument(ce->slot_rasp, INSTR_SQUARE, 8, 0, 7, 160);
    instrument_filter(ce->slot_rasp, FILTER_LOW, 1800, 4);
    // INTAKE — noise rush / turbo whistle / hum, depending on the kind
    int iw = (e.intakeMode == 2 || e.intakeMode == 3 || e.intakeMode == 5) ? INSTR_SINE : INSTR_NOISE;
    instrument(ce->slot_intake, iw, 20, 0, 7, 220);
    instrument_filter(ce->slot_intake, FILTER_BAND, 2400, 6);
    // POP — short noise crack for backfire / blow-off
    instrument(ce->slot_pop, INSTR_NOISE, 0, 30, 0, 40);

    ce->vBody   = note_on(40, ce->slot_body, 0);   note_glide(ce->vBody, 60);
    ce->vRasp   = note_on(52, ce->slot_rasp, 0);   note_glide(ce->vRasp, 60);  note_duty(ce->vRasp, e.raspDuty);
    ce->vIntake = note_on(72, ce->slot_intake, 0); note_glide(ce->vIntake, 40);
    ce->last_chug_hz = -1;
    ce->builtKind = ce->kind;
}

// the whole engine voice, driven live every frame
static void carengine_update(CarEngine *ce, float rpm, float throttle, int audible) {
    if (!audible) { carengine_kill(ce); return; }
    CERecipe e = carengine_recipe(ce->kind);
    if (ce->builtKind != ce->kind) { carengine_kill(ce); carengine_build(ce, e); }

    float r = clamp(rpm, 0, 1);

    // master firing frequency — everything hangs off this one number.
    float chug = lerp(e.chugIdle, e.chugRed, r);
    // idle LOPE: a slow Perlin wobble while NOT on the throttle, so idle loiters.
    if (ce->layer_on[CE_L_LOPE] && e.lopeAmt > 0) {
        float w = (noise(now() * 1.7f) - 0.5f) * 2.0f;       // -1..1, slow
        chug *= 1.0f + w * e.lopeAmt * 0.10f * (1.0f - throttle * 0.8f);
    }
    if (chug < 1) chug = 1;

    // BODY — pitch = firing freq + the kind's tonal octave; brightens with revs+gas.
    float bmidi = carengine_hz_to_midi(chug) + e.bodyOct;
    int   bvol  = (int)(lerp(e.volIdle, 6.0f, r) + throttle * 1.2f);  if (bvol > 7) bvol = 7;
    int   bcut  = e.cutBase + (int)((r * 0.7f + throttle * 0.3f) * e.cutSpan);
    note_pitch (ce->vBody, bmidi);
    note_vol   (ce->vBody, bvol);
    note_cutoff(ce->vBody, bcut);
    instrument_tune(ce->slot_body, e.detune);       // cheap shimmer for electric/nuclear

    // DRIVE — saturation climbs with load (rpm + throttle). The snarl.
    float drv = ce->layer_on[CE_L_DRIVE] ? (e.driveBase + (r * 0.5f + throttle * 0.5f) * e.driveSpan) : 0;
    note_drive(ce->vBody, clamp(drv, 0, 1));

    // FIRING tremolo — an LFO_VOLUME AT the firing rate. The chug that speeds up.
    float tdep = (ce->layer_on[CE_L_FIRING]) ? e.tremDepth : 0;
    int chug_i = (int)(chug + 0.5f);
    if (chug_i != ce->last_chug_hz) {           // only re-push when the rate actually moved
        note_lfo(ce->vBody, 0, LFO_VOLUME, chug, tdep);
        ce->last_chug_hz = chug_i;
    } else {
        note_lfo(ce->vBody, 0, LFO_VOLUME, chug, tdep);   // depth may have toggled — cheap, phase kept
    }

    // RASP — thin square an octave up, leans in ON the throttle (engine "breathes").
    if (e.raspOn && ce->layer_on[CE_L_RASP]) {
        note_pitch (ce->vRasp, bmidi + e.raspOct);
        int rvol = (int)(throttle * throttle * 5.0f + r * 1.5f);  if (rvol > 6) rvol = 6;
        note_vol   (ce->vRasp, rvol);
        note_cutoff(ce->vRasp, 900 + (int)((r * 0.6f + throttle * 0.4f) * 3200));
    } else note_vol(ce->vRasp, 0);

    // INTAKE / TURBO — air rush, turbo whistle, or steady hum.
    if (ce->layer_on[CE_L_INTAKE] && e.intakeMode != 0) {
        switch (e.intakeMode) {
        case 1:   // noise air-rush: louder & brighter with load
            note_pitch (ce->vIntake, 60);
            note_vol   (ce->vIntake, (int)(throttle * 3.0f + r * 1.5f));
            note_cutoff(ce->vIntake, 1400 + (int)((r * 0.6f + throttle * 0.4f) * 3000));
            break;
        case 2: {  // turbo whistle (sine), pitch + level rise hard with revs (gated by TURBO toggle)
            int on = ce->layer_on[CE_L_TURBO];
            note_pitch (ce->vIntake, 84 + r * 26);
            note_vol   (ce->vIntake, on ? (int)(r * r * 4.0f + throttle * 1.0f) : 0);
            note_cutoff(ce->vIntake, 8000);
            break; }
        case 3:   // hum/shimmer: high airy bed, barely moves
            note_pitch (ce->vIntake, 72 + r * 6);
            note_vol   (ce->vIntake, (int)(2.0f + r * 2.0f));
            note_cutoff(ce->vIntake, 2200 + (int)(r * 2200));
            break;
        case 5:   // EV overtone whine: a clean octave above the body, rising from silence with speed
            note_pitch (ce->vIntake, bmidi + 12);
            note_vol   (ce->vIntake, (int)(r * r * 4.0f + throttle * 0.5f));   // 0 at rest → whines up
            note_cutoff(ce->vIntake, 9000);
            break;
        case 4:   // diesel/tractor clatter: gritty mid noise pulsing at the firing rate
            note_pitch (ce->vIntake, 50);
            note_vol   (ce->vIntake, (int)(2.0f + r * 2.0f + throttle));
            note_cutoff(ce->vIntake, 1000 + (int)(r * 1400));
            note_lfo   (ce->vIntake, 0, LFO_VOLUME, chug, 0.9f);   // chatters with the body
            break;
        }
    } else note_vol(ce->vIntake, 0);

    // BURBLE — overrun crackle: lift off the gas at high revs → pops for a beat.
    float thr_drop = ce->prev_thr - throttle;
    if (e.burbleOn && ce->layer_on[CE_L_BURBLE] && thr_drop > 0.05f && r > 0.45f)
        ce->burble_t = 0.55f;
    if (ce->burble_t > 0) {
        ce->burble_t -= dt();
        ce->pop_t    -= dt();
        if (ce->pop_t <= 0 && chance(60)) {
            hit(36 + rnd_between(0, 14), ce->slot_pop, 2, 35);    // a dry crack
            ce->pop_t = 0.04f + 0.05f * (rnd_between(0, 100) / 100.0f);
        }
    }
    ce->prev_thr = throttle;
}
#endif
