/* de:meta
{
  "slug": "outboard",
  "title": "outboard rack",
  "status": "active",
  "created": "2026-08-19",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "docs/design/analog-outboard-chain.md -- the honesty ledger for the classic 'we modeled a FET compressor, a British EQ and a German plate' output-chain promise. ampcab.h did this for a guitar cabinet (effects-bus-architecture.md 'E'); runtime/outboard.h does it for the MASTER bus, and this cart is the bench that plays it. Sibling of mixbooth (the per-instrument family) and groovebox (the summed-bus family): this one is the OUTPUT-STAGE family, four pinned stages you A/B against a loop.",
  "description": {
    "summary": "A mastering rack you can hear working. Three programs play themselves so both hands stay on the four stages: console EQ, asymmetric IRON saturation, a FET-style bus compressor and a plate send. The meter reads RMS, PEAK and CREST side by side, because the compressor moves the first and RAISES the third, and a peak meter alone says the stage is inert. A HEADROOM switch flips the mix to unity levels, which is what a cart sounds like when nobody trimmed anything, and the rack measurably loses 2.8 dB of its effect.",
    "detail": "Four stages, all bundles of effects the engine already ships (decision 0015), owned by the shared runtime/outboard.h table: EQ is eq_inst(0) with three fixed bands and a curve preset, IRON is drive_insert(DRIVE_ASYM) which is the even-harmonic shaper (the one line in the classic outboard pitch that is fully true here), COMP is a flat eq_inst(1) INPUT boost feeding glue() with the ratio button selecting an (amount, attack, release, dirt) tuple, and PLATE is reverb() plus per-slot instrument_reverb sends. Four things it makes measurable rather than claimed. (1) HEADROOM: HOT is not 'louder', it is UNITY on every slot, which is the default a cart gets if nobody sets instrument_level. Measured on the GROOVE loop, going from -6.6 dBFS peak to -0.05 dBFS halves what the EQ does (+0.55 dB of RMS becomes +0.27 dB), costs the EQ+IRON+COMP chain 2.8 dB of its lift (+10.30 becomes +7.53) and collapses the crest it leaves from 14.7 dB to 7.7 dB: the peak is pinned before the rack starts, so what you hear is mostly the soft-clip. The switch is on the panel because hearing that is the lesson. (2) The meter shows RMS, PEAK and CREST plus a live 'vs dry' delta, because COMP at the ALL ratio drops RMS about 4 dB while the peak stays at the ceiling and the crest factor RISES: glue() is a fast-attack slow-release DUCKER, not a peak limiter. (3) Three programs, each voiced to expose one stage: a steady groove, a loud-bar/quiet-bar PUMP that makes the comp's recovery audible, and a SPARSE pattern whose gaps the plate tail fills. (4) The bit-exact bypass, now a committed oracle (tools/bypass-check.js): EQ and IRON return on the same sample the switch flips, COMP within one LSB inside 17 ms, PLATE about 3.4 s later once its tail has decayed. With the PLATE in circuit every A/B smears for about a second, so the cart says so on the panel rather than letting you draw the wrong conclusion. Order note: the comp is PINNED after the whole insert chain, so the achievable path is EQ then IRON then COMP, never comp-first. Every outboard_apply() is SET-AND-HOLD, fired only by the control that actually changed.",
    "controls": "SPACE runs the loop; keys 1 2 3 4 switch EQ / IRON / COMP / PLATE in and out (or tap a name bar); drag a knob; C cycles the EQ curve, V the compression ratio; N cycles the presets (DRY / GLUE / SQUASH / AIR / FULL); P cycles the program; H flips HEADROOM between ROOM and HOT; B bypasses the whole rack; F auto-flips the rack in and out every two bars, hands-free"
  }
}
de:meta */
#include "studio.h"
#include "ui.h"           // widgets + the Box helpers (ui.h pulls in lay.h)
#include "fxicons.h"      // the shared effect glyphs + pedal palette
#include "drumkit.h"      // the shared playable kit (DK_ELECTRO)
#include "outboard.h"     // the shared four-stage output-chain voicing table
#include <math.h>
#include <stdio.h>    // snprintf — ui.h only pulls stdio in DEBUG builds, so the bake needs it named here
#ifdef DE_SPEC
#include "spec.h"
#endif

// OUTBOARD RACK -- the bench for runtime/outboard.h.
//
// The promise every "analog console" plug-in makes is a signal path: a snappy FET compressor, a
// warm console EQ, a lush plate. docs/design/analog-outboard-chain.md is the ledger of what of
// that this engine can honestly back; this cart is where you HEAR it, and the four things it
// MEASURES instead of asserting are the reason it exists:
//
//   1. HEADROOM. Not a mixing nicety, the precondition. HOT is not "louder": it is UNITY on every
//      slot, which is what a cart sounds like when nobody set instrument_level at all, because 1.0
//      is the default. MEASURED on the GROOVE programme (dry vs the stage held in, play.js --wav +
//      wav-analyze), ROOM at -6.6 dBFS peak against HOT at -0.05 dBFS peak:
//        the EQ's lift               +0.55 dB  →  +0.27 dB   (halved)
//        EQ+IRON+COMP's lift        +10.30 dB  →  +7.53 dB   (2.8 dB of the rack gone)
//        the crest it leaves         14.68 dB  →   7.72 dB
//      So the honest claim is not "the stages go inert", it is that the peak is pinned BEFORE the
//      rack starts, so what you hear at HOT is mostly the soft-clip and not the rack. The switch is
//      ON THE PANEL because hearing that is the lesson.
//   2. RMS, PEAK and CREST side by side, plus a live "vs dry" delta. COMP at the ALL ratio takes
//      about 4 dB of RMS while the PEAK stays at the ceiling, so the crest factor goes UP: glue()
//      is a fast-attack slow-release DUCKER, not a peak limiter. Watch the peak alone and you
//      conclude, wrongly, that the stage is inert. That is why there are three numbers.
//   3. Three PROGRAMS, each voiced to expose one stage: a steady groove; a loud-bar/quiet-bar PUMP
//      where the comp's RECOVERY is the audible event; and a SPARSE pattern whose gaps the plate
//      fills. A mastering chain shown on one static loop shows one thing.
//   4. The scope: the real post-FX mix, triggered so the wave stands still. Watch IRON flatten one
//      side of the wave harder than the other, and watch the soft-clip square it off at HOT.
//
// The bit-exact bypass is no longer an assertion in a comment: `node tools/bypass-check.js` is the
// committed oracle. EQ and IRON return on the sample the switch flips; COMP within one LSB inside
// 17 ms (a nulled eq_inst is an ALGEBRAIC null, not a float-exact one); PLATE 3.4 s later, because
// a reverb tail is real. Which is why the chain strip warns you: with the PLATE in, every stage's
// A/B smears for about a second, so park it out to hear a switch cleanly.

#define SL_BASS  5
#define DK_BASE  20
#define STEPS    16
#define SCOPE_N  2048   // ~46ms, a slow timebase: enough cycles of a 55 Hz kick to see a WAVE

// ── the programs ──────────────────────────────────────────────────────────────
// Three loops, each chosen for what it makes AUDIBLE. A chain like this wants transients and
// dynamics; a steady 16th-note bed hides the comp entirely, which is how a bus compressor gets
// written off as inert. `dyn` alternates a loud bar with a quiet one, which is the whole point of
// the PUMP program: the interesting sound is the RECOVERY, and a recovery is only audible if the
// programme gets out of the way for a bar.
typedef struct {
    const char *name;
    const char *kick, *snare, *hat, *clap;
    const signed char *bass;
    int dyn;                 // 1 = every other bar plays soft, so the comp's release is audible
    const char *listen;      // what this programme is FOR — printed under the step lanes
} Program;

// minor basslines; -1 = rest
static const signed char BASS_GROOVE[STEPS] = {
    33, -1, 33, 45,  36, -1, 40, -1,  33, -1, 33, 43,  41, -1, 40, 38,
};
static const signed char BASS_PUMP[STEPS] = {
    33, -1, -1, -1,  33, -1, -1, 45,  36, -1, -1, -1,  40, -1, 38, -1,
};
static const signed char BASS_SPARSE[STEPS] = {
    33, -1, -1, -1,  -1, -1, -1, -1,  36, -1, -1, -1,  -1, -1, -1, -1,
};

#define PROG_N 3
static const Program PROG[PROG_N] = {
    { "GROOVE", "X...X..X..X.X...", "....X.......X...", "..X...X...X...X.", "............X..X",
      BASS_GROOVE, 0, "a steady bed: A/B any stage against it" },
    { "PUMP",   "X.......X.......", "....X.......X...", "..............X.", "............X...",
      BASS_PUMP,   1, "loud bar / quiet bar: hear COMP breathe and recover" },
    { "SPARSE", "X...............", "........X.......", "................", "..............X.",
      BASS_SPARSE, 0, "big gaps: hear the PLATE tail fill them" },
};

// ── the presets ───────────────────────────────────────────────────────────────
// A rack with four stages and six controls has no entry point without these. Voiced after what you
// would actually reach for, and named after the SOUND, never after a device: nothing here models
// anything (decision 0015, and the doc's §7 rule that no named unit goes in the copy).
typedef struct { const char *name; Outboard ob; const char *note; } ObPreset;

#define PRESET_N 5
static const ObPreset PRESET[PRESET_N] = {
    //           eq_on  eq_amt curve   iron_on iron_amt  comp_on comp_in ratio   plate_on plate_amt
    { "DRY",    { 0,    0.80f, 0,      0,      0.42f,    0,      0.40f,  1,      0,       0.55f },
      "the rack out: the reference every A/B is measured against" },
    { "GLUE",   { 1,    0.35f, 0,      1,      0.22f,    1,      0.30f,  0,      0,       0.55f },
      "gentle: the whole mix breathes as one lump" },
    { "SQUASH", { 1,    0.60f, 2,      1,      0.70f,    1,      0.65f,  3,      0,       0.55f },
      "all buttons in: pumping and dirty, and the CREST goes UP" },
    { "AIR",    { 1,    0.85f, 1,      0,      0.42f,    0,      0.40f,  1,      1,       0.62f },
      "top lift plus a plate on the snare: no dynamics at all" },
    { "FULL",   { 1,    0.80f, 0,      1,      0.42f,    1,      0.40f,  1,      1,       0.55f },
      "all four stages, the whole rack in circuit" },
};

// which voices feed the plate. A kick in a plate just smears, so it sends nothing.
static const ObSend SENDS[] = {
    { DK_BASE + DK_SNARE, 0.55f },
    { DK_BASE + DK_CLAP,  0.65f },
    { DK_BASE + DK_HHC,   0.30f },
    { SL_BASS,            0.12f },
};
#define N_SENDS ((int)(sizeof SENDS / sizeof SENDS[0]))

// ── state ─────────────────────────────────────────────────────────────────────
static Outboard ob;
static int   playing  = 1;
static int   bypass   = 0;          // the whole-rack A/B
static int   hot      = 0;          // HEADROOM: 0 = ROOM (~-7 dBFS peak), 1 = HOT (into the clip)
static int   prog     = 0;
static int   preset   = 4;          // boots on FULL: the demo IS the A/B, and B gives you dry
static int   autoflip = 0;          // hands-free A/B: flip the whole rack every two bars
static int   last_s16 = -1;
static int   cur_step = 0;   // NOT `step`: spec.h declares void step(int) for the harness
static int   bars     = 0;   // NOT `bar`: studio.h has a bar() draw call
static float scope[SCOPE_N];
static float peak_hold   = 0.0f;    // slow-falling peak-hold tick on the meter
static float rms_smooth  = 0.0f;    // the level that MOVES when a stage goes in
static float peak_smooth = 0.0f;    // the level that mostly does NOT — which is the lesson
static float rms_dry     = 0.0f;    // learned whenever nothing is in circuit: the A/B reference

// ── stage identity ────────────────────────────────────────────────────────────
// The glyph and the pedal colours come from the SHARED language (fxicons.h) so these read like the
// pedalboard's stompboxes instead of like a fifth private palette. COMP is the one stage with no
// FX_* kind, because glue() is not a reorderable insert: it is PINNED after the chain. So it gets a
// hand-drawn VU needle and the near-black / VU-yellow pairing fxicons gives FX_MULTIBAND, borrowed
// deliberately (those two are the engine's only dynamics stages) and not by pretending it is one.
enum { ST_EQ, ST_IRON, ST_COMP, ST_PLATE };
static const char *ST_NAME[4] = { "EQ", "IRON", "COMP", "PLATE" };
static const int   ST_KIND[4] = { FX_EQ, FX_DRIVE, -1, FX_REVERB };
static const char *ST_SUB[4]  = { "eq_inst(0)", "DRIVE_ASYM", "glue()+IN", "reverb send" };
static const char *ST_FOOT[4] = { "3 fixed bands", "even + odd", "no threshold", "MONO tank" };
static int st_body(int i)   { return i == ST_COMP ? CLR_BROWNISH_BLACK : fx_body(ST_KIND[i]); }
static int st_accent(int i) { return i == ST_COMP ? CLR_YELLOW         : fx_accent(ST_KIND[i]); }

// ── SET-AND-HOLD ──────────────────────────────────────────────────────────────
// apply() is the ONLY place the rack is (re)configured, and it is called from a control CHANGE,
// never from a frame: eq/drive/glue/reverb rebuild bus DSP, and firing them 60x/s is a stutter, not
// a crash (tools/lint-fx-frame.js).
static void apply(void) {
    if (bypass) {
        Outboard off = outboard_default();
        outboard_apply(&off, SENDS, N_SENDS);
    } else {
        outboard_apply(&ob, SENDS, N_SENDS);
    }
}

// HEADROOM, the finding this cart exists to make visible. Both states are set from ONE place so
// they differ by nothing except the gain: a demo where the two halves drift is not a demo.
static void set_levels(void) {
    // ROOM peaks at about -7 dBFS: the trims that leave the rack room to work.
    // HOT is UNITY on every slot, which is not an arbitrary "louder" — it is what a cart sounds like
    // when nobody set instrument_level at all, because 1.0 is the default. So the switch is exactly
    // the mistake §2c is about: not "someone turned it up", but "someone never turned it down".
    if (hot) {
        instrument_level(SL_BASS,            1.0f);
        instrument_level(DK_BASE + DK_KICK,  1.0f);
        instrument_level(DK_BASE + DK_SNARE, 1.0f);
        instrument_level(DK_BASE + DK_HHC,   1.0f);
        instrument_level(DK_BASE + DK_CLAP,  1.0f);
    } else {
        instrument_level(SL_BASS,            0.335f);
        instrument_level(DK_BASE + DK_KICK,  0.370f);
        instrument_level(DK_BASE + DK_SNARE, 0.335f);
        instrument_level(DK_BASE + DK_HHC,   0.255f);
        instrument_level(DK_BASE + DK_CLAP,  0.300f);
    }
}

static void load_preset(int p) {
    preset = (p % PRESET_N + PRESET_N) % PRESET_N;
    ob     = PRESET[preset].ob;
    bypass = 0;
    apply();
}

// toggle one stage in/out by index. The keyboard path exists so the A/B is SCRIPTABLE: it is what
// tools/bypass-check.js drives (a mouse tap is an absolute canvas coordinate that does not survive a
// relayout; a key is position-free).
static void toggle(int i) {
    if      (i == ST_EQ)   ob.eq_on    = !ob.eq_on;
    else if (i == ST_IRON) ob.iron_on  = !ob.iron_on;
    else if (i == ST_COMP) ob.comp_on  = !ob.comp_on;
    else                   ob.plate_on = !ob.plate_on;
    bypass = 0;
    apply();
}

static int rack_in(void) {
    return !bypass && (ob.eq_on || ob.iron_on || ob.comp_on || ob.plate_on);
}

static void fire(int s) {
    const Program *p = &PROG[prog];
    // the PUMP program's every-other bar plays soft. That gap is where a bus comp's release lives.
    int soft = (p->dyn && (bars & 1));
    int d = soft ? 3 : 0;
    if (p->kick[s]  == 'X') dk_fire(DK_KICK,  0, 7 - d);
    if (p->snare[s] == 'X') dk_fire(DK_SNARE, 0, 6 - d);
    if (p->hat[s]   == 'X') dk_fire(DK_HHC,   0, 4 - (soft ? 2 : 0));
    if (p->clap[s]  == 'X') dk_fire(DK_CLAP,  0, 5 - d);
    if (p->bass[s] > 0)     hit(p->bass[s], SL_BASS, soft ? 3 : 6, 150);
}

void init(void) {
    bpm(104);
    dk_use(&DK_ELECTRO, DK_BASE);

    instrument(SL_BASS, INSTR_SAW, 2, 220, 4, 90);
    instrument_filter(SL_BASS, FILTER_LOW, 620, 7);
    instrument_env(SL_BASS, 0, ENV_CUTOFF, 0, 130, 1400.0f);

    set_levels();       // ROOM: about -7 dBFS peak, which is what leaves the rack room to work
    load_preset(4);     // FULL
}

void update(void) {
    if (keyp(' ')) playing = !playing;
    if (keyp('B') || keyp('b')) { bypass = !bypass; apply(); }
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) toggle(i);
    if (keyp('C') || keyp('c')) { ob.curve = (ob.curve + 1) % OB_CURVE_N; apply(); }
    if (keyp('V') || keyp('v')) { ob.ratio = (ob.ratio + 1) % OB_RATIO_N; apply(); }
    if (keyp('N') || keyp('n')) load_preset(preset + 1);
    if (keyp('P') || keyp('p')) prog = (prog + 1) % PROG_N;
    if (keyp('H') || keyp('h')) { hot = !hot; set_levels(); }
    if (keyp('F') || keyp('f')) autoflip = !autoflip;

    if (playing) {
        int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
        if (s16 != last_s16) {
            last_s16 = s16;
            int prev = cur_step;
            cur_step = s16 % STEPS;
            if (cur_step < prev) {
                bars++;
                // hands-free A/B: two bars in, two bars out. A stranger who never presses a key
                // still hears the rack arrive and leave, which is the only way a demo demonstrates.
                if (autoflip && (bars & 1)) { bypass = !bypass; apply(); }
            }
            fire(cur_step);
        }
    }

    // Meter the REAL post-FX mix, and meter it THREE ways, because no one of them is the answer.
    // RMS is what MOVES for every stage here. PEAK mostly does not, which is the trap: a peak meter
    // shows the comp doing nothing. CREST (peak minus RMS) is the number that exposes what glue()
    // actually is: at the ALL ratio it takes ~4 dB of RMS with the peak pinned, so the crest RISES.
    // A compressor is supposed to do the opposite. glue is a fast-attack slow-release DUCKER with no
    // threshold (you drive it with INPUT, which is the whole FET front panel), and that is a
    // character, not level control.
    scope_read(scope, SCOPE_N);
    float pk = 0.0f, sq = 0.0f;
    for (int i = 0; i < SCOPE_N; i++) {
        float a = fabsf(scope[i]); if (a > pk) pk = a;
        sq += scope[i] * scope[i];
    }
    float rms = sqrtf(sq / SCOPE_N);
    peak_hold    = (pk > peak_hold) ? pk : peak_hold * 0.965f;
    peak_smooth += (pk  - peak_smooth) * 0.20f;
    rms_smooth  += (rms - rms_smooth)  * 0.12f;
    // The "vs dry" reference LEARNS ITSELF whenever nothing is in circuit, so the delta is against
    // THIS programme at THIS headroom rather than against a number baked in at build time.
    if (!rack_in() && rms_smooth > 0.0005f)
        rms_dry += (rms_smooth - rms_dry) * (rms_dry > 0.0f ? 0.05f : 1.0f);

#ifdef DE_TRACE
    watch("step",   "%d", cur_step);
    watch("bypass", "%d", bypass);
    watch("hot",    "%d", hot);
    watch("peak",   "%.3f", peak_hold);
    watch("rms",    "%.4f", rms_smooth);
    watch("stages", "%d%d%d%d", ob.eq_on, ob.iron_on, ob.comp_on, ob.plate_on);
#endif
}

// is stage `i` actually in circuit right now?
static int stage_on(int i) {
    if (bypass) return 0;
    return i == ST_EQ ? ob.eq_on : i == ST_IRON ? ob.iron_on
         : i == ST_COMP ? ob.comp_on : ob.plate_on;
}

static float db_of(float lin) { return 20.0f * log10f(lin > 1e-5f ? lin : 1e-5f); }

// ── the stage panel ───────────────────────────────────────────────────────────
// Laid out from its own Box (lay.h), so the panel reads as a stack of bands rather than as a column
// of hand-added pixel offsets: name bar, glyph, subtitle, knob, selector, footer.
static void panel(Box c, int i) {
    int on = stage_on(i);
    int body   = on ? st_body(i)   : CLR_DARKER_GREY;
    int accent = on ? st_accent(i) : CLR_DARK_GREY;

    boxfill(c, body);
    boxrect(c, accent);

    Box rest;
    Box name = lay_split(c, EDGE_TOP, 11, &rest);
    // the name bar IS the switch. The widget goes down FIRST and the label on top of it: ui_button
    // draws its own chrome, so painting the bar before the button hides the bar (it did).
    if (ui_button_cell(name, "")) toggle(i);
    boxfill(name, on ? accent : CLR_DARKER_GREY);
    print(ST_NAME[i], (int)name.x + 4, (int)name.y + 2, on ? CLR_BLACK : CLR_MEDIUM_GREY);
    font(FONT_SMALL);
    const char *sw = on ? "IN" : "OUT";
    print(sw, (int)(name.x + name.w) - 4 - text_width(sw), (int)name.y + 3,
          on ? CLR_BLACK : CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the shared glyph, so a stranger recognises the stage before reading a word of it
    Box glyph = lay_split(rest, EDGE_TOP, 20, &rest);
    // the glyph is drawn in a LIGHT ink rather than in the pedal's own accent: fxicons' accents are
    // chosen to sit on a black background, and indigo-on-dark-blue (the reverb pair) is invisible at
    // 20px. The shared language is carried by the SHAPE plus the chassis and name-bar colours.
    int ink = on ? CLR_WHITE : CLR_DARK_GREY;
    int gx = (int)(glyph.x + glyph.w / 2), gy = (int)(glyph.y + glyph.h / 2);
    if (i == ST_COMP) {
        // a VU needle. glue() has no FX_* kind because it is not a reorderable insert, so there is
        // no shared icon to reuse — draw the METER, which is what a bus comp's face actually is.
        circ(gx, gy + 1, 6, ink);
        line(gx, gy + 1, gx - 4, gy - 3, ink);
        pset(gx, gy + 1, ink);
    } else {
        fx_icon(ST_KIND[i], gx, gy, ink, body);
    }

    font(FONT_SMALL);
    Box sub = lay_split(rest, EDGE_TOP, 8, &rest);
    print(ST_SUB[i], (int)(sub.x + sub.w / 2) - text_width(ST_SUB[i]) / 2, (int)sub.y,
          on ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    font(FONT_NORMAL);

    Box foot = lay_split(rest, EDGE_BOTTOM, 8, &rest);
    Box selb = lay_split(rest, EDGE_BOTTOM, 13, &rest);

    float *v = (i == ST_EQ)   ? &ob.eq_amt
             : (i == ST_IRON) ? &ob.iron_amt
             : (i == ST_COMP) ? &ob.comp_in
                              : &ob.plate_amt;
    const char *klab = (i == ST_COMP) ? "INPUT" : (i == ST_PLATE) ? "SIZE" : "AMOUNT";
    if (ui_knob_cell(rest, v, klab)) apply();

    if (i == ST_EQ || i == ST_COMP) {
        const char *sel = (i == ST_EQ) ? ob_curve_name(ob.curve) : ob_ratio_name(ob.ratio);
        if (ui_button_cell(lay_pad(selb, 0, 5, 1, 5), sel)) {
            if (i == ST_EQ) ob.curve = (ob.curve + 1) % OB_CURVE_N;
            else            ob.ratio = (ob.ratio + 1) % OB_RATIO_N;
            apply();
        }
    } else {
        char buf[16];
        snprintf(buf, sizeof buf, "%d%%", (int)(*v * 100.0f + 0.5f));
        print(buf, (int)(selb.x + selb.w / 2) - text_width(buf) / 2, (int)selb.y + 2,
              on ? CLR_WHITE : CLR_DARK_GREY);
    }

    font(FONT_SMALL);
    print(ST_FOOT[i], (int)(foot.x + foot.w / 2) - text_width(ST_FOOT[i]) / 2, (int)foot.y,
          on ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
    font(FONT_NORMAL);
}

// ── the meter block ───────────────────────────────────────────────────────────
// THREE numbers, on purpose (see update()): an RMS bar with a slow peak-hold tick, then RMS, PEAK,
// CREST and the live delta against the dry reference the cart learns for itself.
static void meters(Box c) {
    float rdb = db_of(rms_smooth), pdb = db_of(peak_smooth);
    float crest = pdb - rdb;
    char buf[48];

    Box row = lay_split(c, EDGE_TOP, 9, &c);
    print("RMS", (int)row.x, (int)row.y, CLR_LIGHT_GREY);
    int bx = (int)row.x + 24, bw = 104;
    rect(bx, (int)row.y - 1, bw, 8, CLR_DARKER_GREY);
    float fill = (rdb + 60.0f) / 60.0f;                       // -60 dBFS = empty
    if (fill < 0.0f) fill = 0.0f;
    if (fill > 1.0f) fill = 1.0f;
    rectfill(bx + 1, (int)row.y, (int)(fill * (bw - 2)), 6,
             rdb > -6.0f ? CLR_ORANGE : CLR_LIME_GREEN);
    int tick = bx + 1 + (int)(peak_hold * (bw - 2));
    if (tick > bx + bw - 2) tick = bx + bw - 2;
    line(tick, (int)row.y, tick, (int)row.y + 5, peak_hold > 0.98f ? CLR_RED : CLR_WHITE);
    snprintf(buf, sizeof buf, "%.1f dB", rdb);
    print(buf, bx + bw + 6, (int)row.y, CLR_WHITE);

    font(FONT_SMALL);
    Box row2 = lay_split(c, EDGE_TOP, 8, &c);
    snprintf(buf, sizeof buf, "PEAK %.1f", pdb);
    print(buf, (int)row2.x, (int)row2.y, peak_hold > 0.98f ? CLR_RED : CLR_MEDIUM_GREY);
    snprintf(buf, sizeof buf, "CREST %.1f", crest);
    print(buf, (int)row2.x + 66, (int)row2.y, CLR_LIGHT_GREY);
    // the honest cost of a bus comp with no makeup gain: the delta against the DRY reference. Until
    // there IS one, the readout says how to get one, which is also how you learn what B is for.
    if (rms_dry <= 0.0f)
        print("vs dry: press B for the reference", (int)row2.x + 140, (int)row2.y, CLR_DARK_GREY);
    else if (!rack_in())
        print("vs dry: this IS dry", (int)row2.x + 140, (int)row2.y, CLR_DARK_GREY);
    else {
        float d = rdb - db_of(rms_dry);
        snprintf(buf, sizeof buf, "vs dry %+.1f dB", d);
        print(buf, (int)row2.x + 140, (int)row2.y, d < -0.5f ? CLR_ORANGE : CLR_LIME_GREEN);
    }

    Box row3 = lay_split(c, EDGE_TOP, 8, &c);
    if (hot)
        print("HOT: unity levels, peak pinned before the rack starts -- it loses 2.8 dB of its effect",
              (int)row3.x, (int)row3.y, CLR_RED);
    else
        print("a peak meter alone says the comp is inert -- watch RMS and CREST instead",
              (int)row3.x, (int)row3.y, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

// the signal path, drawn in the order it actually RUNS: the comp is pinned after the inserts, so
// EQ then IRON then COMP is the only order available on the master bus.
static void chain_strip(int x0, int y) {
    static const int ORDER[3] = { ST_EQ, ST_IRON, ST_COMP };
    int x = x0;
    font(FONT_SMALL);
    print("MIX", x, y, CLR_MEDIUM_GREY); x += text_width("MIX") + 3;
    for (int k = 0; k < 3; k++) {
        int i = ORDER[k];
        print(">", x, y, CLR_DARKER_GREY);  x += text_width(">") + 3;
        print(ST_NAME[i], x, y, stage_on(i) ? st_accent(i) : CLR_DARKER_GREY);
        x += text_width(ST_NAME[i]) + 3;
    }
    print("> CLIP > OUT", x, y, CLR_MEDIUM_GREY);
    x += text_width("> CLIP > OUT") + 8;
    print("+ PLATE parallel", x, y, stage_on(ST_PLATE) ? st_accent(ST_PLATE) : CLR_DARKER_GREY);
    x += text_width("+ PLATE parallel") + 8;
    // The measured corollary, on the panel instead of only in the doc: with the plate in circuit a
    // stage switch does not reconverge until the tail has carried the difference away. Not saying so
    // is how you conclude a bypass is leaky when it is a reverb being a reverb.
    if (stage_on(ST_PLATE))
        print("plate in: an A/B smears ~1s", x, y, CLR_YELLOW);
    font(FONT_NORMAL);
}

void draw(void) {
    ui_begin();
    cls(CLR_BLACK);
    Box rest = lay_inset(box(0, 0, SCREEN_W, SCREEN_H), 4);

    // ── header ────────────────────────────────────────────────────────────────
    Box head = lay_split(rest, EDGE_TOP, 12, &rest);
    print("OUTBOARD RACK", (int)head.x, (int)head.y, CLR_WHITE);
    font(FONT_SMALL);
    print("hear the bus work", (int)head.x + 112, (int)head.y + 3, CLR_DARK_GREY);
    font(FONT_NORMAL);
    int hr = (int)(head.x + head.w);
    if (ui_button(hr - 190, (int)head.y, 36, 11, playing ? "STOP" : "PLAY")) playing = !playing;
    if (ui_button(hr - 150, (int)head.y, 46, 11, PROG[prog].name)) prog = (prog + 1) % PROG_N;
    if (ui_button(hr - 100, (int)head.y, 42, 11, hot ? "HOT" : "ROOM")) { hot = !hot; set_levels(); }
    if (ui_button(hr - 54,  (int)head.y, 54, 11, bypass ? "RACK IN" : "BYPASS")) {
        bypass = !bypass; apply();
    }

    // ── the scope: the ACTUAL post-FX master mix ───────────────────────────────
    Box sc = lay_split(rest, EDGE_TOP, 32, &rest);
    boxrect(sc, CLR_DARKER_GREY);
    int mid = (int)(sc.y + sc.h / 2);
    int sx = (int)sc.x + 1, sw = (int)sc.w - 2, amp = (int)(sc.h / 2) - 2;
    line(sx, mid, sx + sw - 1, mid, CLR_DARKER_GREY);
    // a TRIGGERED trace: start at the first rising zero-crossing so the wave stands still instead of
    // scrolling, on a slow ~46ms timebase so the low end reads as a WAVE and not as one half-cycle.
    // This is where you WATCH IRON flatten one side harder than the other, and watch the soft-clip
    // square the whole thing off at HOT.
    int trig = 0;
    for (int i = 1; i < 256; i++)
        if (scope[i - 1] <= 0.0f && scope[i] > 0.0f) { trig = i; break; }
    int span = SCOPE_N - trig - 1, prev = mid;
    for (int cx = 0; cx < sw; cx++) {
        int i = trig + cx * span / sw;
        int yv = mid - (int)(scope[i] * amp);
        if (cx) line(sx + cx - 1, prev, sx + cx, yv, bypass ? CLR_MEDIUM_GREY : CLR_LIME_GREEN);
        prev = yv;
    }
    font(FONT_SMALL);
    print("post-FX mix", (int)(sc.x + sc.w) - 58, (int)sc.y + 2, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── meters ────────────────────────────────────────────────────────────────
    lay_split(rest, EDGE_TOP, 2, &rest);
    meters(lay_split(rest, EDGE_TOP, 26, &rest));

    // ── presets ───────────────────────────────────────────────────────────────
    Box pre = lay_split(rest, EDGE_TOP, 13, &rest);
    for (int i = 0; i < PRESET_N; i++) {
        Box b = lay_cell(pre, 0, PRESET_N + 1, i, 3);   // dir 0 = ROW; lay_cell is NOT EDGE_*
        if (ui_button_cell(b, PRESET[i].name)) load_preset(i);
        if (i == preset && !bypass) boxrect(b, CLR_WHITE);   // DRY counts: rack_in() is false for it
    }
    {
        Box b = lay_cell(pre, 0, PRESET_N + 1, PRESET_N, 3);
        if (ui_button_cell(b, autoflip ? "FLIP ON" : "AUTO A/B")) autoflip = !autoflip;
    }
    font(FONT_SMALL);
    Box note = lay_split(rest, EDGE_TOP, 8, &rest);
    print(PRESET[preset].note, (int)note.x, (int)note.y, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // ── the four stages ───────────────────────────────────────────────────────
    lay_split(rest, EDGE_TOP, 2, &rest);
    Box row = lay_split(rest, EDGE_TOP, 96, &rest);
    for (int i = 0; i < 4; i++) panel(lay_cell(row, 0, 4, i, 4), i);

    // ── the chain, then the loop ──────────────────────────────────────────────
    Box chain = lay_split(rest, EDGE_TOP, 11, &rest);
    chain_strip((int)chain.x, (int)chain.y + 2);

    Box seq = lay_split(rest, EDGE_TOP, 18, &rest);
    const Program *p = &PROG[prog];
    int soft = (p->dyn && (bars & 1));
    for (int s = 0; s < STEPS; s++) {
        Box b = lay_cell(seq, 0, STEPS, s, 2);
        int on = (s == cur_step && playing);
        int drum = (p->kick[s] == 'X') || (p->snare[s] == 'X') || (p->clap[s] == 'X');
        rectfill((int)b.x, (int)seq.y, (int)b.w, 10,
                 on ? CLR_WHITE : drum ? (soft ? CLR_DARK_GREY : CLR_MEDIUM_GREY) : CLR_DARKER_GREY);
        if (p->bass[s] > 0)
            rectfill((int)b.x, (int)seq.y + 12, (int)b.w, 5, on ? CLR_YELLOW : CLR_BROWN);
    }

    font(FONT_SMALL);
    Box foot = lay_split(rest, EDGE_BOTTOM, 8, &rest);
    print(soft ? "PUMP: the quiet bar -- this is where you hear the comp let go" : PROG[prog].listen,
          (int)rest.x, (int)rest.y + 1, soft ? CLR_YELLOW : CLR_MEDIUM_GREY);
    print("1-4 stage  C curve  V ratio  N preset  P prog  H room  B bypass  F flip",
          (int)foot.x, (int)foot.y, CLR_DARK_GREY);
    font(FONT_NORMAL);

    ui_end();
}

#ifdef DE_SPEC
// The logic worth gating on a rack is not the DSP (the audio gates own that, and the bypass claim is
// gated for real by tools/bypass-check.js, which renders audio) but the WIRING: a preset that
// cross-wires a knob, a stage key that flips the wrong stage, a bypass that forgets what it left, a
// headroom switch that changes nothing. All four are silent, all four are one-line mistakes, and
// this half runs in a second.
void spec(void) {
    step(2);

    // the null rack: a fresh Outboard must be every stage OUT, because that is what the whole-rack
    // BYPASS relies on (it applies outboard_default() and nothing else)
    Outboard d = outboard_default();
    expect(!d.eq_on && !d.iron_on && !d.comp_on && !d.plate_on,
           "outboard_default() is every stage OUT");

    // the presets are what they say they are
    expect(!PRESET[0].ob.eq_on && !PRESET[0].ob.iron_on &&
           !PRESET[0].ob.comp_on && !PRESET[0].ob.plate_on,
           "the DRY preset has no stage in circuit");
    expect(PRESET[4].ob.eq_on && PRESET[4].ob.iron_on &&
           PRESET[4].ob.comp_on && PRESET[4].ob.plate_on,
           "the FULL preset has all four stages in circuit");
    expect_eq(PRESET[2].ob.ratio, OB_RATIO_N - 1,
              "SQUASH selects the hardest ratio, which is its whole point");
    expect(PRESET[3].ob.plate_on && !PRESET[3].ob.comp_on,
           "AIR is a plate voicing with no dynamics at all");

    // every preset is reachable by the N key, in order, and lands on its own table row
    load_preset(0);
    for (int i = 0; i < PRESET_N; i++) {
        expect_eq(preset, i, "N cycles to the next preset");
        expect(ob.eq_on == PRESET[i].ob.eq_on && ob.plate_on == PRESET[i].ob.plate_on,
               "the loaded rack matches its preset row");
        spec_tap('N');
    }
    expect_eq(preset, 0, "N wraps back round to DRY");

    // a stage key flips EXACTLY its own stage. Inserting a knob or a stage mid-list has silently
    // cross-wired an index in this repo before, which is why this is asserted per stage.
    load_preset(0);
    static const char keys[4] = { '1', '2', '3', '4' };
    for (int i = 0; i < 4; i++) {
        spec_tap(keys[i]);
        int flags[4] = { ob.eq_on, ob.iron_on, ob.comp_on, ob.plate_on };
        expect_eq(flags[0] + flags[1] + flags[2] + flags[3], 1,
                  "one stage key switches exactly one stage in");
        expect_eq(flags[i], 1, "and it is the stage that key names");
        spec_tap(keys[i]);
        expect(!ob.eq_on && !ob.iron_on && !ob.comp_on && !ob.plate_on,
               "pressing it again switches that stage back out");
    }

    // the selectors wrap instead of running off the end of their tables
    load_preset(4);
    for (int i = 0; i < OB_CURVE_N + 1; i++) spec_tap('C');
    expect_eq(ob.curve, 1, "C cycles the EQ curve and wraps");
    for (int i = 0; i < OB_RATIO_N; i++) spec_tap('V');
    expect_eq(ob.ratio, 1, "V cycles the ratio all the way round");
    expect(ob_curve_name(-1) == ob_curve_name(0) && ob_ratio_name(999) == ob_ratio_name(0),
           "the name helpers clamp an out-of-range selector instead of reading past the table");

    // HEADROOM is really two states, and must not disturb the rack
    load_preset(4);
    int was = hot;
    spec_tap('H');
    expect(hot != was, "H flips HEADROOM");
    expect(ob.eq_on && ob.comp_on, "and leaves the rack configuration alone");
    spec_tap('H');
    expect_eq(hot, was, "H flips it back");

    // BYPASS is the whole-rack A/B: it must not clear the stage flags, or RACK IN would come back to
    // a different rack than the one you left
    load_preset(4);
    spec_tap('B');
    expect(bypass && !rack_in(), "B takes the whole rack out of circuit");
    expect(ob.eq_on && ob.iron_on && ob.comp_on && ob.plate_on,
           "and REMEMBERS every stage, so RACK IN restores what you had");
    spec_tap('B');
    expect(!bypass && rack_in(), "B puts it back");

    // touching a stage while bypassed must land you IN circuit, never in a state where the panel
    // says IN and the bus says OUT
    spec_tap('B');
    expect(bypass, "bypassed again");
    toggle(ST_EQ);
    expect(!bypass, "touching a stage while bypassed drops the bypass");

    // the programmes: every pattern must be the declared length, or fire() reads past the end
    for (int i = 0; i < PROG_N; i++) {
        int n = 0; while (PROG[i].kick[n])  n++;  expect_eq(n, STEPS, "the kick pattern is STEPS long");
        n = 0;     while (PROG[i].snare[n]) n++;  expect_eq(n, STEPS, "the snare pattern is STEPS long");
        n = 0;     while (PROG[i].hat[n])   n++;  expect_eq(n, STEPS, "the hat pattern is STEPS long");
        n = 0;     while (PROG[i].clap[n])  n++;  expect_eq(n, STEPS, "the clap pattern is STEPS long");
    }
    expect_eq(PROG[1].dyn, 1, "PUMP is the dynamic programme (loud bar / quiet bar)");
    expect(PROG[0].dyn == 0 && PROG[2].dyn == 0, "the other two programmes are steady");
}
#endif
