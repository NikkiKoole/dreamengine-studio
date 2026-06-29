/* de:meta
{
  "title": "enginelab · dyno",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "subtractive-synth",
    "analog-voice-modeling",
    "sonification"
  ],
  "lineage": "A dyno workbench grown from sloop.c's single-voice engine sound; novel in breaking every synthesis layer (firing-rate tremolo, rasp, intake, drive, lope, burble) onto a live toggle for A/B auditioning.",
  "description": "A car-engine sound DYNO and synthesis workbench (the next-gen voice for sloop). An engine isn't a note - it's a FIRING RATE: ~15 Hz of distinct thuds at idle climbing to ~150 Hz of buzzing tone at redline, and that sweep is the whole illusion. One master number (chug_hz) drives every layer so they stay harmonically locked: a saw BODY pinging a resonant lowpass (each edge = one combustion puff), a FIRING tremolo at the chug rate that speeds up with the revs (sloop's is stuck at 9 Hz - the #1 thing that reads as 'synth' not 'engine'), a throttle-led exhaust RASP, an INTAKE/turbo-whistle bed, DRIVE saturation that snarls under load, an idle LOPE wobble, and an overrun BURBLE crackle when you lift. Seven engine kinds (electric whine, gas, diesel clatter, steam chuff, screaming race + turbo, nuclear hum, potato-potato tractor), each a distinct recipe. Hold UP/Z/SPACE to rev, LEFT/RIGHT switch kinds, 1-7 toggle each layer live to hear what it adds, I ignition, L free-rev vs under-load, M auto-rev demo."
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// ============================================================================
// ENGINELAB  —  a car-engine sound DYNO. Rev it, switch engine kinds, and toggle
//               each synthesis LAYER on/off to hear what it contributes.
//
//   ── why this exists ──────────────────────────────────────────────────────
//   sloop's engine is ONE held saw through a resonant lowpass, pitch tracking
//   the revs (sloop.c → engine_sound()). Good bones, but flat: no snarl under
//   load, no firing-rate "chug", a single sterile voice, the same character for
//   every engine kind. This cart is the workbench for the NEXT version — every
//   technique broken out on a live toggle so you can A/B it by ear, then port
//   the winners back into sloop. Nothing here is faked: it's all held voices
//   driven frame-by-frame, the same model sloop already uses.
//
//   ── the model: an engine is a FIRING RATE, not a note ─────────────────────
//   A 4-stroke fires (cyl/2) times per rev, so the "engine note" is really the
//   firing frequency: ~15 Hz of distinct thuds at idle, climbing to ~150 Hz of
//   buzzing tone at redline. THAT sweep is the whole illusion. Everything below
//   derives from one master number — chug_hz — so the layers stay harmonically
//   locked instead of fighting each other:
//     • BODY    a saw at the firing freq through a resonant lowpass — each saw
//               edge "pings" the resonance = one combustion puff. Distinct thuds
//               low, a merged tone high. The core voice.
//     • FIRING  an LFO_VOLUME tremolo AT chug_hz — the lope/chug. Speeds up with
//               revs (sloop's tremolo is stuck at 9 Hz — the #1 thing that reads
//               as "synth" not "engine"). Strong at idle, a fast buzz at redline.
//     • RASP    a thin-pulse square an octave up — the bright exhaust edge that
//               only leans in when you're ON the throttle (off-throttle it backs
//               off → the engine "breathes").
//     • INTAKE  a noise/whistle bed — air rush, or a turbo whistle (RACE), or
//               diesel clatter. Rises with revs + load.
//     • DRIVE   waveshaper saturation that climbs with load → the snarl. ASYM for
//               a fat single-ended-amp grit, HARD for a diesel rattle.
//     • LOPE    a slow Perlin wobble on the idle so it loiters, never sterile.
//     • BURBLE  lift off the throttle at high revs → overrun crackle (the pop-pop).
//
//   ── controls ──────────────────────────────────────────────────────────────
//   ▲ / Z / SPACE (hold)  throttle (rev it)        ◄ / ►   switch engine kind
//   1..7                  toggle a synthesis layer  I       ignition on/off
//   L                     LOAD: free-rev ↔ under-load (slower to wind up)
//   M                     auto-rev demo (self-blips the throttle)
// ============================================================================

// ── engine kinds (mirrors sloop's EK_*, so a tuned voice ports back 1:1) ──────
enum { EK_ELECTRIC, EK_GAS, EK_DIESEL, EK_STEAM, EK_RACE, EK_NUCLEAR, EK_TRACTOR, EK_KINDS };
static const char *EK_NAME[EK_KINDS] = {
    "ELECTRIC", "GAS", "DIESEL", "STEAM", "RACE", "NUCLEAR", "TRACTOR" };
static const char *EK_FEEL[EK_KINDS] = {
    "pure rising whine, no chug",
    "revvy mid, throttle rasp",
    "low clatter, lumpy lope",
    "slow chuff puffs",
    "screaming wail + turbo",
    "steady sci-fi hum",
    "potato-potato grunt" };

// ── synthesis layers (each on a live toggle, juice.c style) ───────────────────
enum { L_FIRING, L_RASP, L_INTAKE, L_DRIVE, L_LOPE, L_BURBLE, L_TURBO, L_COUNT };
static const char *L_NAME[L_COUNT] = {
    "FIRING", "RASP", "INTAKE", "DRIVE", "LOPE", "BURBLE", "TURBO" };
static int layer_on[L_COUNT];   // all default 1 (set in init)

// ── per-kind voice recipe — ONE struct, filled per frame from the kind ────────
// chugIdle/chugRed: firing Hz at idle vs redline (the master pitch sweep).
// bodyOct: semitones added to the firing pitch for the body's tonal growl.
// bodyWave/bodyFilt: oscillator + filter mode. tremDepth: firing-lope strength.
// drive*: saturation that climbs with load. rasp*/intake*: the extra layers.
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
} ERecipe;

static ERecipe recipe(int k) {
    ERecipe e;
    // shared defaults
    e.bodyWave = INSTR_SAW; e.bodyFilt = FILTER_LOW; e.bodyOct = 0;
    e.res = 6; e.volIdle = 3; e.tremDepth = 0.5f; e.driveBase = 0.1f; e.driveSpan = 0.4f;
    e.driveMode = DRIVE_ASYM; e.raspOn = 0; e.raspOct = 12; e.raspDuty = 0.18f;
    e.intakeMode = 0; e.burbleOn = 0; e.lopeAmt = 0.5f; e.detune = 0;
    switch (k) {
    case EK_ELECTRIC:                                   // no combustion → no chug, no idle, no grit: a clean rising whine
        e.bodyWave = INSTR_SINE; e.bodyOct = 12;        // pure tone — TRI/detune was buzzy & "labored"
        e.chugIdle = 110; e.chugRed = 720;              // a WIDE, effortless zoom up
        e.cutBase = 1200; e.cutSpan = 5000; e.res = 1;  // wide open, almost no resonance → smooth
        e.volIdle = 0;                                  // SILENT at rest (an EV makes no noise stopped); whines up with speed
        e.tremDepth = 0; e.driveBase = 0; e.driveSpan = 0;
        e.intakeMode = 5; e.lopeAmt = 0; e.detune = 0;  // a clean octave-up overtone whine, no beating
        break;
    case EK_DIESEL:                                     // low, hard, clattery, lumpy at idle
        e.chugIdle = 11; e.chugRed = 58;
        e.bodyOct = 0; e.cutBase = 150; e.cutSpan = 560; e.res = 8;
        e.tremDepth = 0.85f; e.driveBase = 0.4f; e.driveSpan = 0.3f; e.driveMode = DRIVE_HARD;
        e.raspOn = 1; e.raspOct = 7; e.raspDuty = 0.12f;
        e.intakeMode = 4; e.lopeAmt = 0.85f;
        break;
    case EK_STEAM:                                      // slow chuff — gated noise puffs
        e.bodyWave = INSTR_NOISE; e.bodyFilt = FILTER_BAND;
        e.chugIdle = 3; e.chugRed = 13;
        e.cutBase = 220; e.cutSpan = 360; e.res = 7;
        e.tremDepth = 1.0f; e.driveBase = 0; e.driveSpan = 0;   // full gate = distinct puffs
        e.intakeMode = 0; e.lopeAmt = 0.35f;
        break;
    case EK_RACE:                                       // wide, bright, screaming + turbo
        e.chugIdle = 26; e.chugRed = 150;
        e.cutBase = 420; e.cutSpan = 2400; e.res = 5;
        e.tremDepth = 0.42f; e.driveBase = 0.3f; e.driveSpan = 0.5f; e.driveMode = DRIVE_ASYM;
        e.raspOn = 1; e.raspOct = 12; e.raspDuty = 0.16f;
        e.intakeMode = 2; e.burbleOn = 1; e.lopeAmt = 0.2f;     // anti-lag crackle
        break;
    case EK_NUCLEAR:                                    // steady mid hum, faint shimmer, no lope
        e.bodyWave = INSTR_TRI;
        e.chugIdle = 44; e.chugRed = 190;
        e.cutBase = 320; e.cutSpan = 980; e.res = 4;
        e.tremDepth = 0.1f; e.driveBase = 0.08f; e.driveSpan = 0.1f; e.driveMode = DRIVE_SOFT;
        e.intakeMode = 3; e.lopeAmt = 0.12f; e.detune = 0.09f;
        break;
    case EK_TRACTOR:                                    // very slow, heavy, max lope (potato-potato)
        e.chugIdle = 6; e.chugRed = 26;
        e.cutBase = 120; e.cutSpan = 440; e.res = 9;
        e.tremDepth = 1.0f; e.driveBase = 0.5f; e.driveSpan = 0.25f; e.driveMode = DRIVE_HARD;
        e.raspOn = 1; e.raspOct = 7; e.raspDuty = 0.1f;
        e.intakeMode = 4; e.lopeAmt = 1.0f;
        break;
    case EK_GAS: default:                               // the revvy everyday mid
        e.chugIdle = 19; e.chugRed = 116;
        e.cutBase = 300; e.cutSpan = 1400; e.res = 6;
        e.tremDepth = 0.55f; e.driveBase = 0.12f; e.driveSpan = 0.45f; e.driveMode = DRIVE_ASYM;
        e.raspOn = 1; e.raspOct = 12; e.raspDuty = 0.18f;
        e.intakeMode = 1; e.burbleOn = 1; e.lopeAmt = 0.5f;
        break;
    }
    return e;
}

// ── slots & held voices ───────────────────────────────────────────────────────
#define SLOT_BODY   5
#define SLOT_RASP   6
#define SLOT_INTAKE 7
#define SLOT_POP    8     // transient backfire/blow-off (re-triggered hits)
static int vBody = -1, vRasp = -1, vIntake = -1;
static int builtKind = -1;        // which kind the held voices are configured for

// ── live state ────────────────────────────────────────────────────────────────
static int   eng_kind  = EK_GAS;
static int   engine_on = 1;
static float rpm       = 0;       // 0..1 normalised revs
static float throttle  = 0;       // 0..1 ramped pedal
static float prev_thr  = 0;
static int   loaded    = 0;       // 0 = free-rev (snappy), 1 = under load (lazy wind-up)
static int   autorev   = 1;       // demo self-revving (alive at rest / for the bake)
static float burble_t  = 0;       // overrun-crackle countdown after a lift
static float pop_t     = 0;       // burble pop spacing
static int   last_chug_hz = -1;   // gate note_lfo pushes to when the rate moved
static float ar_phase  = 0;       // autorev throttle phase

// firing-pulse scope: a ring of recent blip x-positions scrolling left
#define SCOPE_N 64
static float scope_phase = 0;     // accumulates at chug_hz → emits a blip each cycle
static int   scope_hit;           // 1 the frame a firing pulse fired (draw a fat blip)

static float hz_to_midi(float hz) { return 69.0f + 12.0f * log2f(hz / 440.0f); }

// ── (re)build the three held voices for the current kind ──────────────────────
static void engine_kill(void) {
    if (vBody   >= 0) { note_off(vBody);   vBody   = -1; }
    if (vRasp   >= 0) { note_off(vRasp);   vRasp   = -1; }
    if (vIntake >= 0) { note_off(vIntake); vIntake = -1; }
    builtKind = -1;
}

static void engine_build(ERecipe e) {
    // BODY — the core firing voice
    instrument(SLOT_BODY, e.bodyWave, 30, 0, 7, 220);
    instrument_filter(SLOT_BODY, e.bodyFilt, e.cutBase, e.res);
    instrument_drive_mode(SLOT_BODY, e.driveMode);
    instrument_tune(SLOT_BODY, 0);
    // RASP — thin pulse, an octave up; throttle-led exhaust edge
    instrument(SLOT_RASP, INSTR_SQUARE, 8, 0, 7, 160);
    instrument_filter(SLOT_RASP, FILTER_LOW, 1800, 4);
    // INTAKE — noise rush / turbo whistle / hum, depending on the kind
    int iw = (e.intakeMode == 2 || e.intakeMode == 3 || e.intakeMode == 5) ? INSTR_SINE : INSTR_NOISE;
    instrument(SLOT_INTAKE, iw, 20, 0, 7, 220);
    instrument_filter(SLOT_INTAKE, FILTER_BAND, 2400, 6);
    // POP — short noise crack for backfire / blow-off
    instrument(SLOT_POP, INSTR_NOISE, 0, 30, 0, 40);

    vBody   = note_on(40, SLOT_BODY, 0);   note_glide(vBody, 60);
    vRasp   = note_on(52, SLOT_RASP, 0);   note_glide(vRasp, 60);  note_duty(vRasp, e.raspDuty);
    vIntake = note_on(72, SLOT_INTAKE, 0); note_glide(vIntake, 40);
    last_chug_hz = -1;
    builtKind = eng_kind;
}

// ── the whole engine voice, driven live every frame ───────────────────────────
static void engine_sound(int audible) {
    if (!audible) { engine_kill(); return; }
    ERecipe e = recipe(eng_kind);
    if (builtKind != eng_kind) { engine_kill(); engine_build(e); }

    float r = clamp(rpm, 0, 1);

    // master firing frequency — everything hangs off this one number.
    float chug = lerp(e.chugIdle, e.chugRed, r);
    // idle LOPE: a slow Perlin wobble while NOT on the throttle, so idle loiters.
    if (layer_on[L_LOPE] && e.lopeAmt > 0) {
        float w = (noise(now() * 1.7f) - 0.5f) * 2.0f;       // -1..1, slow
        chug *= 1.0f + w * e.lopeAmt * 0.10f * (1.0f - throttle * 0.8f);
    }
    if (chug < 1) chug = 1;

    // BODY — pitch = firing freq + the kind's tonal octave; brightens with revs+gas.
    float bmidi = hz_to_midi(chug) + e.bodyOct;
    int   bvol  = (int)(lerp(e.volIdle, 6.0f, r) + throttle * 1.2f);  if (bvol > 7) bvol = 7;
    int   bcut  = e.cutBase + (int)((r * 0.7f + throttle * 0.3f) * e.cutSpan);
    note_pitch (vBody, bmidi);
    note_vol   (vBody, bvol);
    note_cutoff(vBody, bcut);
    instrument_tune(SLOT_BODY, e.detune);       // cheap shimmer for electric/nuclear

    // DRIVE — saturation climbs with load (rpm + throttle). The snarl.
    float drv = layer_on[L_DRIVE] ? (e.driveBase + (r * 0.5f + throttle * 0.5f) * e.driveSpan) : 0;
    note_drive(vBody, clamp(drv, 0, 1));

    // FIRING tremolo — an LFO_VOLUME AT the firing rate. The chug that speeds up.
    float tdep = (layer_on[L_FIRING]) ? e.tremDepth : 0;
    int chug_i = (int)(chug + 0.5f);
    if (chug_i != last_chug_hz) {               // only re-push when the rate actually moved
        note_lfo(vBody, 0, LFO_VOLUME, chug, tdep);
        last_chug_hz = chug_i;
    } else {
        note_lfo(vBody, 0, LFO_VOLUME, chug, tdep);   // depth may have toggled — cheap, phase kept
    }

    // RASP — thin square an octave up, leans in ON the throttle (engine "breathes").
    if (e.raspOn && layer_on[L_RASP]) {
        note_pitch (vRasp, bmidi + e.raspOct);
        int rvol = (int)(throttle * throttle * 5.0f + r * 1.5f);  if (rvol > 6) rvol = 6;
        note_vol   (vRasp, rvol);
        note_cutoff(vRasp, 900 + (int)((r * 0.6f + throttle * 0.4f) * 3200));
    } else note_vol(vRasp, 0);

    // INTAKE / TURBO — air rush, turbo whistle, or steady hum.
    if (layer_on[L_INTAKE] && e.intakeMode != 0) {
        switch (e.intakeMode) {
        case 1:   // noise air-rush: louder & brighter with load
            note_pitch (vIntake, 60);
            note_vol   (vIntake, (int)(throttle * 3.0f + r * 1.5f));
            note_cutoff(vIntake, 1400 + (int)((r * 0.6f + throttle * 0.4f) * 3000));
            break;
        case 2: {  // turbo whistle (sine), pitch + level rise hard with revs (gated by TURBO toggle)
            int on = layer_on[L_TURBO];
            note_pitch (vIntake, 84 + r * 26);
            note_vol   (vIntake, on ? (int)(r * r * 4.0f + throttle * 1.0f) : 0);
            note_cutoff(vIntake, 8000);
            break; }
        case 3:   // hum/shimmer: high airy bed, barely moves
            note_pitch (vIntake, 72 + r * 6);
            note_vol   (vIntake, (int)(2.0f + r * 2.0f));
            note_cutoff(vIntake, 2200 + (int)(r * 2200));
            break;
        case 5:   // EV overtone whine: a clean octave above the body, rising from silence with speed
            note_pitch (vIntake, bmidi + 12);
            note_vol   (vIntake, (int)(r * r * 4.0f + throttle * 0.5f));   // 0 at rest → whines up
            note_cutoff(vIntake, 9000);
            break;
        case 4:   // diesel/tractor clatter: gritty mid noise pulsing at the firing rate
            note_pitch (vIntake, 50);
            note_vol   (vIntake, (int)(2.0f + r * 2.0f + throttle));
            note_cutoff(vIntake, 1000 + (int)(r * 1400));
            note_lfo   (vIntake, 0, LFO_VOLUME, chug, 0.9f);   // chatters with the body
            break;
        }
    } else note_vol(vIntake, 0);

    // BURBLE — overrun crackle: lift off the gas at high revs → pops for a beat.
    float thr_drop = prev_thr - throttle;
    if (e.burbleOn && layer_on[L_BURBLE] && thr_drop > 0.05f && r > 0.45f)
        burble_t = 0.55f;
    if (burble_t > 0) {
        burble_t -= dt();
        pop_t    -= dt();
        if (pop_t <= 0 && chance(60)) {
            hit(36 + rnd_between(0, 14), SLOT_POP, 2, 35);    // a dry crack
            pop_t = 0.04f + 0.05f * (rnd_between(0, 100) / 100.0f);
        }
    }
    prev_thr = throttle;
}

// ── input + rev physics ───────────────────────────────────────────────────────
static void update_revs(void) {
    if (keyp(KEY_LEFT))  { eng_kind = (eng_kind + EK_KINDS - 1) % EK_KINDS; }
    if (keyp(KEY_RIGHT)) { eng_kind = (eng_kind + 1) % EK_KINDS; }
    if (keyp('I')) {
        engine_on = !engine_on;
        if (engine_on) hit(33, INSTR_SAW, 3, 90);            // a crank chirp
        else           hit(28, INSTR_NOISE, 3, 160);         // the cough as it dies
    }
    if (keyp('L')) loaded  = !loaded;
    if (keyp('M')) autorev = !autorev;
    for (int i = 0; i < L_COUNT; i++)
        if (keyp('1' + i)) layer_on[i] = !layer_on[i];

    // throttle: held keys, OR the autorev demo blipping it rhythmically.
    float want;
    if (autorev) {
        ar_phase += dt() * 0.55f;
        float s = sinf(ar_phase) * sinf(ar_phase * 0.37f + 1.1f);   // wandering blips
        want = clamp(s * 1.6f, 0, 1);
    } else {
        want = (key(KEY_UP) || key('Z') || key(KEY_SPACE)) ? 1.0f : 0.0f;
    }
    if (!engine_on) want = 0;
    throttle = lerp(throttle, want, 0.18f);

    // revs chase the throttle; free-rev is snappy, loaded winds up lazily, and the
    // engine always idles a touch above zero when running (so it never goes silent).
    float idle = engine_on ? 0.08f : 0;
    float target = engine_on ? clamp(idle + throttle * (1.0f - idle), 0, 1) : 0;
    float rate = (target > rpm) ? (loaded ? 1.4f : 4.5f) : 2.6f;   // wind-up vs engine-braking
    rpm = lerp(rpm, target, clamp(rate * dt(), 0, 1));
}

void update(void) {
    static int ready = 0;
    if (!ready) { for (int i = 0; i < L_COUNT; i++) layer_on[i] = 1; ready = 1; }
    update_revs();
    engine_sound(engine_on);

    // firing-pulse scope: accumulate at chug_hz, flag a blip each cycle
    scope_hit = 0;
    if (engine_on) {
        float chug = lerp(recipe(eng_kind).chugIdle, recipe(eng_kind).chugRed, clamp(rpm, 0, 1));
        scope_phase += chug * dt();
        if (scope_phase >= 1.0f) { scope_phase -= 1.0f; scope_hit = 1; }
    }
}

// ── drawing ─────────────────────────────────────────────────────────────────
#define TACH_CX 96
#define TACH_CY 118
#define TACH_R  78

static void draw_tach(void) {
    // sweep 225° total: from 135° (lower-left) clockwise to -45° (lower-right)
    circ(TACH_CX, TACH_CY, TACH_R, CLR_DARK_GREY);
    circ(TACH_CX, TACH_CY, TACH_R - 1, CLR_DARK_GREY);
    // tick marks + redline arc
    for (int i = 0; i <= 10; i++) {
        float t = i / 10.0f;
        float a = 135.0f - t * 270.0f;               // degrees
        int redzone = (t > 0.82f);
        int c = redzone ? CLR_RED : CLR_LIGHT_GREY;
        int x0 = TACH_CX + (int)(cos_deg(a) * (TACH_R - 10));
        int y0 = TACH_CY - (int)(sin_deg(a) * (TACH_R - 10));
        int x1 = TACH_CX + (int)(cos_deg(a) * (TACH_R - 2));
        int y1 = TACH_CY - (int)(sin_deg(a) * (TACH_R - 2));
        line(x0, y0, x1, y1, c);
    }
    // the needle
    float a = 135.0f - clamp(rpm, 0, 1) * 270.0f;
    int nx = TACH_CX + (int)(cos_deg(a) * (TACH_R - 14));
    int ny = TACH_CY - (int)(sin_deg(a) * (TACH_R - 14));
    int needcol = (rpm > 0.82f) ? CLR_RED : CLR_YELLOW;
    line(TACH_CX, TACH_CY, nx, ny, needcol);
    circfill(TACH_CX, TACH_CY, 4, CLR_WHITE);
    // rpm digits
    char buf[16];
    int shown = (int)(rpm * 8000 + 50);
    snprintf(buf, sizeof buf, "%d", shown);
    print(buf, TACH_CX - 14, TACH_CY + 24, needcol);
    print("rpm", TACH_CX - 10, TACH_CY + 34, CLR_DARK_GREY);
}

static void draw_scope(void) {
    int y = 224, x0 = 8, x1 = SCREEN_W - 8;
    line(x0, y, x1, y, CLR_DARK_BLUE);
    // a marker pulse that "fires" at chug_hz — fattens the moment a pulse fires
    int px = x0 + (int)(scope_phase * (x1 - x0));
    int h = scope_hit ? 10 : 4;
    line(px, y - h, px, y + h, scope_hit ? CLR_YELLOW : CLR_LIGHT_GREY);
}

static void draw_panel(void) {
    int X = 188;
    // engine kind
    print("ENGINE", X, 14, CLR_DARK_GREY);
    print(EK_NAME[eng_kind], X, 24, CLR_WHITE);
    print(EK_FEEL[eng_kind], X, 34, CLR_LIGHT_GREY);
    print("\x11 \x10 switch", X, 44, CLR_DARK_GREY);

    // ignition + load + autorev state
    print(engine_on ? "IGN: ON" : "IGN: OFF", X, 60, engine_on ? CLR_GREEN : CLR_RED);
    print(loaded ? "LOAD: under load" : "LOAD: free-rev", X, 70, CLR_LIGHT_GREY);
    if (autorev) print("AUTO-REV (M)", X, 80, CLR_ORANGE);
    else         print("hold \x18/Z/SPACE", X, 80, CLR_DARK_GREY);

    // throttle bar
    int tbx = X, tby = 94, tbw = SCREEN_W - X - 8, tbh = 7;
    rect(tbx, tby, tbw, tbh, CLR_DARK_GREY);
    rectfill(tbx + 1, tby + 1, (int)((tbw - 2) * throttle), tbh - 2, CLR_GREEN);
    print("throttle", tbx, tby + 10, CLR_DARK_GREY);

    // layer toggles
    int ly = 118;
    print("LAYERS  (1-7)", X, ly, CLR_DARK_GREY);
    for (int i = 0; i < L_COUNT; i++) {
        int yy = ly + 11 + i * 9;
        char b[24];
        snprintf(b, sizeof b, "%d %s", i + 1, L_NAME[i]);
        print(b, X, yy, layer_on[i] ? CLR_WHITE : CLR_DARK_GREY);
        print(layer_on[i] ? "on" : "off", X + 70, yy, layer_on[i] ? CLR_GREEN : CLR_DARK_GREY);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    // a faint heat glow behind the tach as the revs climb
    if (rpm > 0.6f) circfill(TACH_CX, TACH_CY, TACH_R + 4, CLR_DARK_PURPLE);
    draw_tach();
    draw_panel();
    draw_scope();
    print("ENGINELAB \x07 dyno", 8, 6, CLR_DARK_GREY);
}
