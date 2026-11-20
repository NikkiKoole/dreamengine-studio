/* de:meta
{
  "slug": "enginelab",
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
  "description": "A car-engine sound DYNO. Combustion engines now run INSTR_MOTOR, a voice built from the engine itself: each cylinder fires at its real crank angle in the firing order (inline-6, crossplane V8 burble, flat-4 off-beat, a big single), every firing a little different so the engine rumbles instead of buzzing, into exhaust pipes whose resonances stay put while the revs sweep through them. Sixteen kinds. Petrol: gas (a 1970s straight-six), hatch (revvy inline-4), triple (inline-3), warble (inline-5 + turbo), V6, flat6, boxer (flat-4), muscle (crossplane V8), race (flatplane V8 on a straight pipe + turbo), vtwin (a 45-degree motorcycle twin). Diesel, with the block's KNOCK clattering through the muffler: diesel (inline-4), truck (big turbo-diesel six), tractor (one big cylinder). And the ones without cylinders: steam (a locomotive: four steam blasts per wheel turn, a limp in the beat, standing still it only hisses), electric (a modern EV: motor and gear whine), train (a 1990s inverter that sings up a scale as it pulls away), and the fantasy nuclear hum (still the old synth). N flips any combustion kind back to the OLD synth layers (a saw through a resonant filter with a tremolo at the firing rate) so you can hear what changed. Hold UP/Z/SPACE to rev, LEFT/RIGHT switch kinds, 1-7 toggle layers (over the cylinder voice only turbo, burble, lope and intake apply), I ignition, L free-rev vs under-load, M auto-rev demo."
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
//   N                     A/B: the cylinder voice (INSTR_MOTOR) vs the old synth layers
// ============================================================================

// ── the engine voice itself lives in runtime/carengine.h (extracted when backroads became its
// second consumer); this cart stays its tuning bench: kinds, layers, revs, the dyno display.
#include "carengine.h"
#define EK_KINDS CE_KINDS
#define EK_NAME  CE_NAME
#define EK_FEEL  CE_FEEL
#define L_COUNT  CE_L_COUNT
#define L_NAME   CE_L_NAME
static CarEngine ce;              // slots 5..8 (body, rasp, intake, pop) — as before

// ── live state ────────────────────────────────────────────────────────────────
static int   engine_on = 1;
static float rpm       = 0;       // 0..1 normalised revs
static float throttle  = 0;       // 0..1 ramped pedal
static int   loaded    = 0;       // 0 = free-rev (snappy), 1 = under load (lazy wind-up)
static int   autorev   = 1;       // demo self-revving (alive at rest / for the bake)
static float ar_phase  = 0;       // autorev throttle phase

// firing-pulse scope: a ring of recent blip x-positions scrolling left
#define SCOPE_N 64
static float scope_phase = 0;     // accumulates at chug_hz → emits a blip each cycle
static int   scope_hit;           // 1 the frame a firing pulse fired (draw a fat blip)

// ── input + rev physics ───────────────────────────────────────────────────────
static void update_revs(void) {
    if (keyp(KEY_LEFT))  { ce.kind = (ce.kind + EK_KINDS - 1) % EK_KINDS; }
    if (keyp(KEY_RIGHT)) { ce.kind = (ce.kind + 1) % EK_KINDS; }
    if (keyp('I')) {
        engine_on = !engine_on;
        if (engine_on) hit(33, INSTR_SAW, 3, 90);            // a crank chirp
        else           hit(28, INSTR_NOISE, 3, 160);         // the cough as it dies
    }
    if (keyp('L')) loaded  = !loaded;
    if (keyp('M')) autorev = !autorev;
    if (keyp('N')) ce.legacy = !ce.legacy;                    // A/B: the cylinder voice vs the old synth layers
    for (int i = 0; i < L_COUNT; i++)
        if (keyp('1' + i)) ce.layer_on[i] = !ce.layer_on[i];

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
    if (!ready) { carengine_init(&ce, 5); ready = 1; }
    update_revs();
    carengine_update(&ce, rpm, throttle, engine_on);

    // firing-pulse scope: accumulate at chug_hz, flag a blip each cycle
    scope_hit = 0;
    if (engine_on) {
        CERecipe er = carengine_recipe(ce.kind);
        float chug = lerp(er.chugIdle, er.chugRed, clamp(rpm, 0, 1));
        if (carengine_is_motor(&ce, er)) {                    // the motor's real firing rate: rpm/60 × cylinders/2
            chug = carengine_rpm(&ce, rpm) / 60.0f * CE_MOT_CYL[er.motorLayout] * 0.5f;
        }
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
    int shown = ((int)(carengine_rpm(&ce, rpm) + 50) / 100) * 100;
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
    print(EK_NAME[ce.kind], X, 24, CLR_WHITE);
    font(FONT_SMALL);
    print(EK_FEEL[ce.kind], X, 34, CLR_LIGHT_GREY);
    {
        int motor = carengine_is_motor(&ce, carengine_recipe(ce.kind));
        int has   = carengine_recipe(ce.kind).motorLayout >= 0;
        print(motor ? "voice: CYLINDERS" : has ? "voice: OLD SYNTH" : "voice: synth", X, 42, motor ? CLR_GREEN : CLR_ORANGE);
        print(has ? "<> kind   N a/b" : "<> kind", X, 50, CLR_DARK_GREY);
    }
    font(FONT_NORMAL);

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
        // over the cylinder voice only these layers still do anything
        int live = !carengine_is_motor(&ce, carengine_recipe(ce.kind)) ||
                   i == CE_L_TURBO || i == CE_L_BURBLE || i == CE_L_LOPE || i == CE_L_INTAKE;
        print(b, X, yy, ce.layer_on[i] && live ? CLR_WHITE : CLR_DARK_GREY);
        print(ce.layer_on[i] ? "on" : "off", X + 70, yy, ce.layer_on[i] ? CLR_GREEN : CLR_DARK_GREY);
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
