/* de:meta
{
  "title": "acid rack",
  "status": "active",
  "created": "2026-07-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "subtractive-synth",
    "drum-synthesis"
  ],
  "lineage": "Propellerhead ReBirth RB-338 (1997) — the 2×TB-303 + drum-machine acid rack — rebuilt as one cart from the shipped machine carts (tb303 ×2 on the new FILTER_DIODE, tr909 voices), with Reason-style rack-fold accordion panels and pattern banks + a chain row = song mode. Increments 1-3 of docs/design/rebirth-classic.md.",
  "homage": "Propellerhead ReBirth RB-338 (1997)",
  "todo": [
    "increment 4: seeded generator + song code + WAV export",
    "touch stroke entry: right-click cycles 909 strokes — needs a long-press path on phones",
    "per-voice tune/decay/color knobs (the standalone carts' depth) — the rack bakes center values for now",
    "303 lines don't shuffle (held voice can't schedule ahead) — revisit if straight-303-vs-swung-drums clashes at high swing",
    "maker naming pass: 'acid rack' is a working title"
  ],
  "description": {
    "summary": "Two 303s, the full 909, the curated 808 and the master FX rack in one cart — pattern banks A-D chained into a real song. The ReBirth move: everything runs together, you edit while it plays.",
    "detail": "The RB-338 homage, increment 2: two full TB-303 voices (the engine's FILTER_DIODE diode-ladder squelch, authentic non-retriggering slide, accent into the filter env, live CUT/RES/DRV knobs per machine) + the COMPLETE tr909 kit (11 voices — analog kick/snare/toms/rim/clap, FM-clang metal, the stroke family: right-click a cell for flam/drag/ratchet, the METAL-FILTER XY pad riding all five metal highpasses, TOTAL ACCENT row) + the curated tr808 (9 voices: boom kick, snare, 2 toms, clap, maracas, cowbell, hats — congas/clave/rim/cymbal cut per the rack slot budget), all clocked off one transport with the 909's period-correct even-16th SHUFFLE (one master knob + Z/X). Effects are PER-DEVICE like the real RB-338: every machine strip has an [fx] view (the header button swaps the grid for that machine's effects) with DIST (per-voice drive on that machine only — drums scream while the rest stays clean) and SEND (its level into THE one shared tempo-synced delay unit, per-device routing like the hardware; the 909's fx view also hosts the METAL pad and SHUFFLE). The MASTER strip keeps what genuinely needs a bus: the delay unit's TIME (snaps 1/16, 1/8, dotted-8th, 1/4) and FB, GLU (the glue compressor that tames the drop), and the PCF: a pattern-controlled filter whose 16-step level lane is drawn per BANK, so the arrangement itself rides the master lowpass — the demo's build bank is literally a drawn ramp that opens the filter over one bar. (True per-device PCF/comp waits on machine buses — effects-bus-architecture.md Increment G.) The rack is an ACCORDION: each machine is a slim strip showing its name, a live 16-tick mini pattern with the playhead, and a MUTE — tap a strip to expand its full editor (piano roll + flag rows + knobs for a 303, trigger grid for the drums). Sound never depends on what's open. Patterns live in four BANKS (A-D, the transport buttons); the SONG row at the bottom chains up to 64 bars of banks into a real track — tap a cell to cycle A→B→C→D→empty. Everything autosaves.",
    "controls": "SPACE run/stop · tap a strip header to expand · A-D buttons pick the edit bank (also LEFT/RIGHT) · SONG button toggles chain playback · tap SONG-row cells to write the arrangement · UP/DOWN tempo · Z/X shuffle · roll: tap/drag paints notes, tap a note to erase · OCT/ACC/SLD rows toggle per step · drums: tap cells, right-click a 909 cell for flam/drag/ratchet, AC row = accent, drag the METAL pad for hat tone · [fx] button on a machine strip: its DIST + delay SEND (909: also METAL pad + SHUF) · MASTER strip: delay TIME/FB, GLU, PCF/RES + drag the lane to draw the filter pattern"
  }
}
de:meta */
#include "studio.h"
#include "ui.h"          // widgets: ui_button/ui_knob — per-finger capture, focus ring
#include <stdio.h>
#include <math.h>
#include <string.h>

// ── ACID RACK ─────────────────────────────────────────────────────────────
// ReBirth RB-338 (1997) put "acid techno in a box": two TB-303s and Roland
// drum machines wired to one transport, pattern banks, a song chain. This
// cart rebuilds that shape from the machines this console already ships:
// the tb303 cart's voice (upgraded to FILTER_DIODE — audio-notes §25) twice,
// and the tr909 cart's kick/clap/hats as the starter kit (the full curated
// 808 + 909 panels are increment 2 — see docs/design/rebirth-classic.md).
//
// The two design invariants (from the design doc):
//   1. sound NEVER depends on what's expanded — the sequencer runs globally;
//      the accordion is pure view. Fold the drums mid-roll, nothing changes.
//   2. folded ≠ blind — every strip header carries a live 16-tick mini
//      pattern with the playhead, so the whole rack visibly breathes.
//
// Data model: a PATTERN is a whole-rack snapshot (both 303 lines + the drum
// grid). Four pattern BANKS (A-D); the SONG row chains up to 64 bank picks,
// one per bar. The generator (increment 4) will fill A sparse → D drop.

#define STEPS   16
#define NBANK   4
#define CHAINN  64
#define BASE    36            // C2 — bottom row of the 303 roll

// instrument slots — the rack's slot plan (rebirth-classic §1): the full set
// needs 25 of the 27 definable slots, so the rack uses the whole 5..31 range
// (self-contained cart — nothing else defines slots here).
// 303s: 5,6 · 909 complete: 7..19 (13) · 808 curated: 20..29 (10) · 30,31 free
#define SLOT_A    5           // 303-A
#define SLOT_B    6           // 303-B
// tr909 (complete kit — recipes verbatim from tr909.c)
#define SL9_BD    7
#define SL9_BDC   8           // kick click layer (the ATTACK knob sound)
#define SL9_SDB   9
#define SL9_SDN   10
#define SL9_TOM   11
#define SL9_TOMC  12
#define SL9_RS    13
#define SL9_CP    14
#define SL9_HHC   15
#define SL9_HHO   16
#define SL9_CC    17
#define SL9_CCN   18
#define SL9_RC    19
// tr808 (curated — congas/clave/rim/cymbal cut per the design doc slot math)
#define SL8_BD    20
#define SL8_SDB   21
#define SL8_SDN   22
#define SL8_TOM   23
#define SL8_TOMN  24
#define SL8_CP    25
#define SL8_MAR   26
#define SL8_CB    27
#define SL8_HATO  28
#define SL8_HATC  29

// drum voice tables
enum { V9_BD, V9_SD, V9_LT, V9_MT, V9_HT, V9_RS, V9_CP, V9_CH, V9_OH, V9_CC, V9_RC, N909 };
enum { V8_BD, V8_SD, V8_LT, V8_HT, V8_CP, V8_MA, V8_CB, V8_OH, V8_CH, N808 };
static const char *NAME909[N909] = { "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CH", "OH", "CC", "RC" };
static const char *NAME808[N808] = { "BD", "SD", "LT", "HT", "CP", "MA", "CB", "OH", "CH" };
enum { ST_PLAIN, ST_FLAM, ST_DRAG, ST_RATCHET, NSTROKE };   // 909 stroke family
#define FLAM_MS 22

// ── pattern data ──────────────────────────────────────────────────────────
typedef struct {                       // one 303 line in one bank
    unsigned char  pitch[STEPS];       // semitone 0..12 above BASE
    unsigned short on, oct, acc, sld;  // per-step bitmasks
} Line;
typedef struct {                       // one bank = a whole-rack snapshot
    Line           ln[2];              // [0]=303-A [1]=303-B
    unsigned short d909[N909];         // 909 trigger masks
    unsigned char  st909[N909][STEPS]; // 909 stroke per cell (ST_*)
    unsigned short acc909;             // 909 TOTAL ACCENT row
    unsigned short d808[N808];         // 808 trigger masks
    unsigned short acc808;             // 808 accent row
    unsigned char  pcf[STEPS];         // pattern-controlled-filter lane, level 0..7
} Pattern;

static Pattern bank[NBANK];
static unsigned char chain[CHAINN];    // bank index per bar; 0xFF = empty (song ends)
static int  chainN     = 0;            // bars in the song (first empty cell)

// ── transport / play state ────────────────────────────────────────────────
static int  tempo = 130;
static bool running = true;
static bool songmode = true;           // play the chain (else: loop the edit bank)
static int  editBank = 0;              // which bank A-D the panels edit
static int  playBank = 0;              // which bank is sounding this bar
static int  barpos = 0;                // position in the chain
static int  last16 = -1, playhead = 0;
static int  swing = 50;                // master shuffle 50..66 — 909-style, even 16ths drag
static float swingf = 0.0f;            // the SHUF slider's 0..1 view of it

// ── the machines ──────────────────────────────────────────────────────────
enum { K_CUT, K_RES, K_ENV, K_DEC, K_ACC, K_DRV, K_SQL, NK };
static const char *KNAME[NK] = { "CUT", "RES", "ENV", "DEC", "ACC", "DRV", "SQL" };

typedef struct {
    int   slot;
    const char *name;
    float knob[NK];         // 0..1 (ui_knob's range)
    int   wave;             // INSTR_SAW / INSTR_SQUARE
    int   h;                // held note handle (-1 = none)
    bool  prev_slide;
} M303;
static M303 m[2] = {
    { SLOT_A, "303-A", { 0.45f, 0.70f, 0.60f, 0.40f, 0.60f, 0.35f, 0.33f }, INSTR_SAW,    -1, false },
    { SLOT_B, "303-B", { 0.38f, 0.75f, 0.55f, 0.45f, 0.60f, 0.45f, 0.33f }, INSTR_SQUARE, -1, false },
};

// knob value mappings — same curves as tb303.c, knobs normalized 0..1
static int   cut_hz(M303 *s)  { return (int)(60.0f * powf(2.0f, s->knob[K_CUT] * 6.0f)); } // 60..3840
static int   res_q(M303 *s)   { return (int)(s->knob[K_RES] * 15.0f); }
static float env_hz(M303 *s)  { return s->knob[K_ENV] * 3000.0f; }
static int   dec_ms(M303 *s)  { return 30 + (int)(s->knob[K_DEC] * 500.0f); }
static float acc_mul(M303 *s) { return 1.0f + s->knob[K_ACC] * 1.5f; }
static float sq_mul(M303 *s)  { return 1.0f + s->knob[K_SQL] * 2.0f; }

// the 303 voice — tb303.c's recipe verbatim, on the diode ladder
static void define_303(M303 *s) {
    instrument(s->slot, s->wave, 2, 60, 6, 25);
    instrument_duty(s->slot, 0.48f);
    instrument_filter(s->slot, FILTER_DIODE, cut_hz(s), res_q(s));
    instrument_drive(s->slot, s->knob[K_DRV]);
}
static void knob_changed_303(M303 *s, int k) {
    if (k == K_CUT || k == K_RES) {
        instrument_filter(s->slot, FILTER_DIODE, cut_hz(s), res_q(s));
        if (s->h >= 0) { note_cutoff(s->h, cut_hz(s)); note_res(s->h, (float)res_q(s)); }
    } else if (k == K_DRV) {
        instrument_drive(s->slot, s->knob[K_DRV]);
        if (s->h >= 0) note_drive(s->h, s->knob[K_DRV]);
    }                                                // ENV/DEC/ACC/SQL apply at next trigger
}
static void off_303(M303 *s) { if (s->h >= 0) { note_off(s->h); s->h = -1; } s->prev_slide = false; }

// step trigger — the authentic circuit, from tb303.c: a slid step does NOT
// retrigger (glide carries the pitch, the filter env keeps decaying); accent
// is louder AND a harder env kick; non-slid gates drop at ~70% of the step.
static void step_303(M303 *s, Line *ln, int st) {
    int b = 1 << st;
    if (ln->on & b) {
        int midi = BASE + ln->pitch[st] + ((ln->oct & b) ? 12 : 0);
        int vol  = (ln->acc & b) ? 7 : 5;
        if (s->h >= 0 && s->prev_slide) {
            note_glide(s->h, 60);
            note_pitch(s->h, (float)midi);
            note_vol(s->h, vol);                     // env does NOT refire — authentic
        } else {
            if (s->h >= 0) note_off(s->h);
            float e = env_hz(s) * sq_mul(s) * ((ln->acc & b) ? acc_mul(s) : 1.0f);
            instrument_env(s->slot, 0, ENV_CUTOFF, 0, dec_ms(s), e);
            s->h = note_on(midi, s->slot, vol);
            note_glide(s->h, 0);
        }
        s->prev_slide = (ln->sld & b) != 0;
    } else {
        off_303(s);
    }
}
static void gate_303(M303 *s, Line *ln, int st) {   // staccato release mid-step
    int b = 1 << st;
    if (s->h >= 0 && (ln->on & b) && !(ln->sld & b)) {
        float f = beat_pos() * 4.0f; f -= (int)f;
        if (f > 0.7f) { note_off(s->h); s->h = -1; }
    }
}

// ── the drum machines — tr909.c / tr808.c recipes verbatim ───────────────
// Per-voice tune/decay/color knobs are the standalone carts' depth; the rack
// bakes their center values (a knob at 0.5 is a no-op in both source carts).
static int vv(int base, int boost) { int v = base + boost; return v < 0 ? 0 : (v > 7 ? 7 : v); }

// 909 metal-filter XY pad state — X scales the five metal slots' highpass
// cutoffs, Y is their shared resonance (tr909's admitted impurity, kept:
// the FM stand-ins land bright and hissy without a tone control)
static float mcut = 0.40f, mres = 0.33f;
static void apply_metal_filter(void) {
    static const int msl[5]   = { SL9_HHC, SL9_HHO, SL9_CC, SL9_CCN, SL9_RC };
    static const int mbase[5] = { 8500, 8000, 4500, 5200, 5000 };
    float sc = 0.3f + 1.4f * mcut;
    int   rq = (int)(mres * 12.0f);
    for (int i = 0; i < 5; i++)
        instrument_filter(msl[i], FILTER_HIGH, (int)(mbase[i] * sc), rq);
}

static void define_909(void) {
    // kick — sine body, +30st sweep over 35ms (punch, not boom), run hot
    instrument(SL9_BD, INSTR_SINE, 0, 300, 0, 50);
    instrument_filter(SL9_BD, FILTER_LOW, 380, 2);
    instrument_env(SL9_BD, 0, ENV_PITCH, 0, 35, 30.0f);
    instrument_drive(SL9_BD, 0.35f);
    instrument(SL9_BDC, INSTR_NOISE, 0, 9, 0, 4);         // the ATTACK click
    instrument_filter(SL9_BDC, FILTER_HIGH, 2500, 2);
    // snare — 185+330Hz body modes + bright noise
    instrument(SL9_SDB, INSTR_SINE, 0, 90, 0, 25);
    instrument_filter(SL9_SDB, FILTER_LOW, 1400, 1);
    instrument_env(SL9_SDB, 0, ENV_PITCH, 0, 15, 4.0f);
    instrument(SL9_SDN, INSTR_NOISE, 0, 170, 0, 50);
    instrument_filter(SL9_SDN, FILTER_HIGH, 1400, 2);
    // toms — sine drop + a short click at the attack
    instrument(SL9_TOM, INSTR_SINE, 0, 220, 0, 45);
    instrument_env(SL9_TOM, 0, ENV_PITCH, 0, 60, 7.0f);
    instrument(SL9_TOMC, INSTR_NOISE, 0, 12, 0, 5);
    instrument_filter(SL9_TOMC, FILTER_BAND, 2000, 3);
    // rim — short woody dual ping
    instrument(SL9_RS, INSTR_TRI, 0, 28, 0, 10);
    instrument_filter(SL9_RS, FILTER_HIGH, 400, 3);
    // clap — bandpassed noise; retriggers in fire make the clap
    instrument(SL9_CP, INSTR_NOISE, 0, 100, 0, 45);
    instrument_filter(SL9_CP, FILTER_BAND, 1200, 5);
    // hats/cymbals — the FM-clang ROM stand-ins: 3.5 detent, feedback high,
    // closing highpass (negative ENV_CUTOFF) = the sampled sizzle
    instrument(SL9_HHC, INSTR_FM, 0, 40, 0, 16);
    instrument_harmonics(SL9_HHC, 0.55f);
    instrument_timbre(SL9_HHC, 0.90f);
    instrument_morph(SL9_HHC, 0.85f);
    instrument_env(SL9_HHC, 0, ENV_CUTOFF, 0, 28, -5000.0f);
    instrument(SL9_HHO, INSTR_FM, 0, 380, 0, 110);
    instrument_harmonics(SL9_HHO, 0.55f);
    instrument_timbre(SL9_HHO, 0.90f);
    instrument_morph(SL9_HHO, 0.85f);
    instrument_env(SL9_HHO, 0, ENV_CUTOFF, 0, 220, -4500.0f);
    instrument_choke(SL9_HHC, SL9_HHO);
    instrument(SL9_CC, INSTR_FM, 0, 1000, 0, 280);        // crash: clang + wash
    instrument_harmonics(SL9_CC, 0.55f);
    instrument_timbre(SL9_CC, 1.00f);
    instrument_morph(SL9_CC, 0.90f);
    instrument(SL9_CCN, INSTR_NOISE, 0, 1200, 0, 320);
    instrument(SL9_RC, INSTR_FM, 0, 550, 0, 160);         // ride: cleaner ping
    instrument_harmonics(SL9_RC, 0.55f);
    instrument_timbre(SL9_RC, 0.72f);
    instrument_morph(SL9_RC, 0.58f);
    apply_metal_filter();                                  // the pad owns the metal highpasses
}

static void define_808(void) {
    // kick — the boom: low sine, +26st drop over 50ms, run a little hot
    instrument(SL8_BD, INSTR_SINE, 0, 480, 0, 60);
    instrument_filter(SL8_BD, FILTER_LOW, 250, 3);
    instrument_env(SL8_BD, 0, ENV_PITCH, 0, 50, 26.0f);
    instrument_drive(SL8_BD, 0.28f);
    // snare — 180+330Hz modes + snappy noise
    instrument(SL8_SDB, INSTR_SINE, 0, 100, 0, 30);
    instrument_filter(SL8_SDB, FILTER_LOW, 1200, 1);
    instrument_env(SL8_SDB, 0, ENV_PITCH, 0, 20, 3.0f);
    instrument(SL8_SDN, INSTR_NOISE, 0, 130, 0, 40);
    instrument_filter(SL8_SDN, FILTER_HIGH, 1800, 2);
    // toms — sine drop + a low noise thud
    instrument(SL8_TOM, INSTR_SINE, 0, 260, 0, 50);
    instrument_env(SL8_TOM, 0, ENV_PITCH, 0, 80, 6.0f);
    instrument(SL8_TOMN, INSTR_NOISE, 0, 28, 0, 12);
    instrument_filter(SL8_TOMN, FILTER_BAND, 900, 2);
    // clap
    instrument(SL8_CP, INSTR_NOISE, 0, 110, 0, 50);
    instrument_filter(SL8_CP, FILTER_BAND, 1100, 5);
    // maracas
    instrument(SL8_MAR, INSTR_NOISE, 0, 24, 0, 10);
    instrument_filter(SL8_MAR, FILTER_HIGH, 6500, 2);
    // cowbell — square pair through the classic ~2.6kHz bandpass
    instrument(SL8_CB, INSTR_SQUARE, 0, 250, 0, 60);
    instrument_filter(SL8_CB, FILTER_BAND, 2640, 5);
    // hats — bank squares through ~7kHz highpass; open vs closed = decay
    instrument(SL8_HATO, INSTR_SQUARE, 0, 340, 0, 90);
    instrument_filter(SL8_HATO, FILTER_HIGH, 7000, 3);
    instrument(SL8_HATC, INSTR_SQUARE, 0, 42, 0, 16);
    instrument_filter(SL8_HATC, FILTER_HIGH, 7000, 3);
    instrument_choke(SL8_HATC, SL8_HATO);
}

static int flash909[N909], flash808[N808];    // header/panel activity flashes

static void fire909(int v, int boost, int delay) {
    switch (v) {
    case V9_BD:
        schedule_hit(delay, 32, SL9_BD, vv(6, boost), 320);
        schedule_hit(delay, 60, SL9_BDC, vv(3, boost), 10);
        break;
    case V9_SD:
        schedule_hit(delay, 54, SL9_SDB, vv(4, boost), 95);
        schedule_hit(delay, 64, SL9_SDB, vv(3, boost), 95);
        schedule_hit(delay, 60, SL9_SDN, vv(4, boost), 180);
        break;
    case V9_LT: case V9_MT: case V9_HT: {
        int mm = v == V9_LT ? 42 : v == V9_MT ? 47 : 54;
        schedule_hit(delay, mm, SL9_TOM, vv(5, boost), 240);
        schedule_hit(delay, 70, SL9_TOMC, vv(2, boost), 14);
        break;
    }
    case V9_RS:
        schedule_hit(delay, 76, SL9_RS, vv(5, boost), 30);
        schedule_hit(delay, 64, SL9_RS, vv(3, boost), 30);
        break;
    case V9_CP:
        schedule_hit(delay,      60, SL9_CP, vv(5, boost), 11);
        schedule_hit(delay + 9,  60, SL9_CP, vv(5, boost), 11);
        schedule_hit(delay + 18, 60, SL9_CP, vv(5, boost), 11);
        schedule_hit(delay + 26, 60, SL9_CP, vv(3, boost), 130);
        break;
    case V9_CH: schedule_hit(delay, 97, SL9_HHC, vv(4, boost), 40);  break;
    case V9_OH: schedule_hit(delay, 97, SL9_HHO, vv(4, boost), 400); break;
    case V9_CC:
        schedule_hit(delay, 84, SL9_CC,  vv(4, boost), 1100);
        schedule_hit(delay, 60, SL9_CCN, vv(3, boost), 1300);
        break;
    case V9_RC:
        schedule_hit(delay, 76, SL9_RC, vv(4, boost), 600);
        schedule_hit(delay, 83, SL9_RC, vv(2, boost), 600);
        break;
    }
    flash909[v] = 6;
}

// the 909 stroke family: flam = one quiet grace 22ms early, drag = two,
// ratchet = four even hits chopped across the step (tr909.c fire_stroke)
static void fire_stroke909(int v, int st, int boost, int delay, int step_ms) {
    switch (st) {
    case ST_RATCHET:
        for (int k = 0; k < 4; k++) fire909(v, k ? boost - 2 : boost, delay + k * step_ms / 4);
        return;
    case ST_DRAG: fire909(v, boost - 4, delay - 2 * FLAM_MS); /* fall through */
    case ST_FLAM: fire909(v, boost - 3, delay - FLAM_MS);     /* fall through */
    default:      fire909(v, boost, delay);
    }
}

static void fire808(int v, int boost, int delay) {
    switch (v) {
    case V8_BD: schedule_hit(delay, 31, SL8_BD, vv(6, boost), 500); break;
    case V8_SD:
        schedule_hit(delay, 54, SL8_SDB, vv(4, boost), 110);
        schedule_hit(delay, 64, SL8_SDB, vv(4, boost), 110);
        schedule_hit(delay, 60, SL8_SDN, vv(4, boost), 140);
        break;
    case V8_LT: case V8_HT: {
        int mm = v == V8_LT ? 40 : 52;
        schedule_hit(delay, mm, SL8_TOM, vv(4, boost), 280);
        schedule_hit(delay, 60, SL8_TOMN, vv(3, boost), 30);
        break;
    }
    case V8_CP:
        schedule_hit(delay,      60, SL8_CP, vv(4, boost), 12);
        schedule_hit(delay + 10, 60, SL8_CP, vv(4, boost), 12);
        schedule_hit(delay + 20, 60, SL8_CP, vv(4, boost), 12);
        schedule_hit(delay + 28, 60, SL8_CP, vv(3, boost), 140);
        break;
    case V8_MA: schedule_hit(delay, 90, SL8_MAR, vv(3, boost), 30); break;
    case V8_CB:  // bank osc pair, 540Hz + 800Hz
        schedule_hit(delay, 73, SL8_CB, vv(4, boost), 220);
        schedule_hit(delay, 79, SL8_CB, vv(4, boost), 220);
        break;
    case V8_OH:
        schedule_hit(delay, 79, SL8_HATO, vv(3, boost), 360);
        schedule_hit(delay, 72, SL8_HATO, vv(3, boost), 360);
        break;
    case V8_CH:
        schedule_hit(delay, 79, SL8_HATC, vv(3, boost), 50);
        schedule_hit(delay, 72, SL8_HATC, vv(2, boost), 50);
        break;
    }
    flash808[v] = 6;
}

// ── the accordion ─────────────────────────────────────────────────────────
enum { STRIP_A, STRIP_B, STRIP_909, STRIP_808, STRIP_MST, NSTRIP };
static const char *SNAME[NSTRIP] = { "303-A", "303-B", "909", "808", "MASTER" };
static int  expanded = STRIP_A;
static bool mute[NSTRIP];
static bool fxview[NSTRIP];            // machine strips: panel shows FX instead of the grid

// layout — transport bar / strips / song chain row
// 24 + 5×18 + 114 = 228 = CHAIN_Y, chain ends 240: exactly flush
#define TB_H    22
#define HDR_H   18
#define PANEL_H 114
#define CHAIN_Y 228
static int strip_y(int i) {            // header top of strip i
    int y = TB_H + 2;
    for (int k = 0; k < i; k++) y += HDR_H + (k == expanded ? PANEL_H : 0);
    return y;
}

// ── the FX rack — per-DEVICE like the RB-338, master where it must be ─────
// Each machine strip has an [FX] view: DIST = per-voice instrument_drive on
// that machine's slots (no bus grab), SEND = its level into THE one shared
// delay unit (instrument_echo — ReBirth had one delay unit + per-device
// routing too). The MASTER strip keeps what genuinely needs a bus today:
// the delay unit's TIME/FB, the GLUE comp, and the PCF + its lane. True
// per-device PCF/comp needs machine/group buses — banked as Increment G in
// effects-bus-architecture.md (maker thinking on it, 2026-07-02).
enum { F_TIME, F_FB, F_GLU, F_PCF, F_RES, NFX };   // the MASTER strip's knobs
static const char *FXNAME[NFX] = { "TIME", "FB", "GLU", "PCF", "RES" };
static float fxk[NFX] = { 0.50f, 0.35f, 0.30f, 0.40f, 0.35f };
static float send[4] = { 0.17f, 0.17f, 0.00f, 0.00f };   // per-machine delay send (A, B, 909, 808)
static float dist9 = 0.0f, dist8 = 0.0f;                 // per-drum-machine drive
static bool  pcf_on = false;           // is the master filter currently engaged

static const int SLOTS909[13] = { SL9_BD, SL9_BDC, SL9_SDB, SL9_SDN, SL9_TOM, SL9_TOMC,
                                  SL9_RS, SL9_CP, SL9_HHC, SL9_HHO, SL9_CC, SL9_CCN, SL9_RC };
static const int SLOTS808[10] = { SL8_BD, SL8_SDB, SL8_SDN, SL8_TOM, SL8_TOMN,
                                  SL8_CP, SL8_MAR, SL8_CB, SL8_HATO, SL8_HATC };

// re-apply set-and-hold effects ONLY when a value moved (groovebox idiom)
static void apply_fx(void) {
    static float aTime = -1, aFb = -1, aGlue = -1, aS[4] = { -1, -1, -1, -1 }, aD9 = -1, aD8 = -1;
    static int   aTempo = -1;
    // THE delay unit — the shared echo SEND bus (one unit, per-device routing,
    // like the hardware). fb capped ≤0.72: near-unity + heavy sends made a
    // sustained wall that held the glue in full gain-reduction (2026-07-02)
    if (fxk[F_TIME] != aTime || fxk[F_FB] != aFb || tempo != aTempo) {
        static const float DIV[4] = { 0.25f, 0.5f, 0.75f, 1.0f };   // 1/16 · 1/8 · dotted · 1/4
        int ms = (int)(60000.0f / tempo * DIV[(int)(fxk[F_TIME] * 3.999f)]);
        echo(ms, fxk[F_FB] * 0.72f, 0.45f);
        aTime = fxk[F_TIME]; aFb = fxk[F_FB]; aTempo = tempo;
    }
    // per-machine sends into it
    // sends capped ×0.6 (like the drums): a full-wet mono 303 into fb 0.72
    // sustains a ~3.5× energy loop that slams the master limiter
    if (send[0] != aS[0]) { instrument_echo(SLOT_A, send[0] * 0.6f); aS[0] = send[0]; }
    if (send[1] != aS[1]) { instrument_echo(SLOT_B, send[1] * 0.6f); aS[1] = send[1]; }
    if (send[2] != aS[2]) { for (int i = 0; i < 13; i++) instrument_echo(SLOTS909[i], send[2] * 0.6f); aS[2] = send[2]; }
    if (send[3] != aS[3]) { for (int i = 0; i < 10; i++) instrument_echo(SLOTS808[i], send[3] * 0.6f); aS[3] = send[3]; }
    // per-drum-machine DIST — per-voice saturation layered over the baked
    // kick drives (909 BD 0.35, 808 BD 0.28), no bus involved at all
    if (dist9 != aD9) {
        for (int i = 0; i < 13; i++) {
            float baked = SLOTS909[i] == SL9_BD ? 0.35f : 0.0f;
            instrument_drive(SLOTS909[i], baked + dist9 * (0.85f - baked));
        }
        aD9 = dist9;
    }
    if (dist8 != aD8) {
        for (int i = 0; i < 10; i++) {
            float baked = SLOTS808[i] == SL8_BD ? 0.28f : 0.0f;
            instrument_drive(SLOTS808[i], baked + dist8 * (0.85f - baked));
        }
        aD8 = dist8;
    }
    // GLUE — master (needs a bus; Increment G would make it per-device).
    // knob maxes at HEAVY GLUE, never a mute (amount 1.0 could duck the mix
    // to silence under a sustained loud tail — measured 2026-07-02)
    float glu = mute[STRIP_MST] ? 0.0f : fxk[F_GLU];
    if (glu != aGlue) {
        glue(0, glu < 0.02f ? 0.0f : glu * 0.55f, 8, 150);
        aGlue = glu;
    }
}

// the PCF — master filter() ridden from the playing bank's lane (ride-safe;
// only the OFF↔ON mode flip is gated). Per-device PCF waits on Increment G.
static void ride_pcf(void) {
    float depth = mute[STRIP_MST] ? 0.0f : fxk[F_PCF];
    if (depth < 0.02f) {
        if (pcf_on) { filter(FILTER_OFF, 0.0f, 0.0f); pcf_on = false; }
        return;
    }
    int lvl = bank[playBank].pcf[playhead];
    float cut = 250.0f * powf(2.0f, (lvl / 7.0f) * depth * 4.5f);   // 250Hz .. ~5.6kHz at full
    filter(FILTER_LOW, cut, 0.15f + 0.75f * fxk[F_RES]);
    pcf_on = true;
}

// ── authored demo patterns (the generator fills these in increment 4) ─────
// 303 lines as tb303-style strings: nt '.'=rest '0'-'9','A'-'C'=semitone;
// oc/ac/sl = 'x' masks. Drum rows use tr909's stroke chars: 'x' hit,
// 'f' flam, 'd' drag, 'r' ratchet (strokes are 909-only, like the panel).
typedef struct { const char *nt, *oc, *ac, *sl; } LineSrc;
typedef struct {
    LineSrc a, b;
    const char *r909[N909]; const char *ac9;   // NULL row = silent
    const char *r808[N808]; const char *ac8;
    const char *pcf;                           // digits 0-7, '.' = 0, NULL = flat 0
} PatSrc;
static const PatSrc DEMO[NBANK] = {
    { // A — sparse intro: one 303 murmurs over the kick
      { "0...3...0...7...", "................", "x...........x...", "................" },
      { "................", "................", "................", "................" },
      { [V9_BD] = "x...x...x...x..." }, "x...............",
      { 0 }, 0,
      "3333333333333333" },   // PCF: the intro sits dark
    { // B — the groove: clap lands, hats breathe, the cowbell answers
      { "0.C03.7A0.C0537A", "................", "x...x...x...x...", "......x.......x." },
      { "0...............", "x...............", "x...............", "................" },
      { [V9_BD] = "x...x...x...x...", [V9_CP] = "....x.......x...",
        [V9_CH] = "..x...x...x...x." }, "x.......x.......",
      { [V8_CB] = "x..x..x.x..x..x." }, 0,
      "5656565656565656" },   // PCF: gentle pumping
    { // C — the build: A climbs an octave, kick doubles into the turn,
      //     ratcheted hats (the Hardfloor move), 808 shaker under it all
      { "0.C03.7A0.C0537A", "........xxxxxxxx", "x...x...x...x...", "......x.......x." },
      { "0.0.0.0.0.0.0.0.", "................", "x...x...x...x...", "................" },
      { [V9_BD] = "x...x...x...x.xx", [V9_CP] = "....x.......x...",
        [V9_CH] = "x.x.x.x.x.x.x.xr", [V9_LT] = "............x...",
        [V9_HT] = "..............x." }, "x.......x.......",
      { [V8_MA] = "xxxxxxxxxxxxxxxx" }, 0,
      "0112233445566777" },   // PCF: the RAMP — the whole build opens over one bar
    { // D — the drop: crash on the one, open hats, ride, snare flam,
      //     both boxes accented and sliding, 808 shaker + cowbell on top
      { "0.C03.7A0.C0537A", "x.......x.......", "x.x.x.x.x.x.x.x.", "..x...x...x...x." },
      { "0..7..A.0..3..C.", ".........x......", "x.......x.......", "..x.....x......." },
      { [V9_BD] = "x...x...x...x...", [V9_CP] = "....x.......f...",
        [V9_CH] = "x...x...x...x...", [V9_OH] = "..x...x...x...x.",
        [V9_RC] = "x.......x.......", [V9_CC] = "x..............." }, "x...x...x...x...",
      { [V8_MA] = "xxxxxxxxxxxxxxxx", [V8_CB] = "x..x..x.x..x..x." }, 0,
      "7777777777777777" },   // PCF: the drop is wide open
};
static const char *DEMO_CHAIN = "AABBAABBCCCDBBDD";

static void decode_line(Line *ln, const LineSrc *src) {
    memset(ln, 0, sizeof *ln);
    for (int s = 0; s < STEPS; s++) {
        char c = src->nt[s];
        if (c != '.') {
            ln->on |= 1 << s;
            ln->pitch[s] = (c >= '0' && c <= '9') ? c - '0' : c - 'A' + 10;
        }
        if (src->oc[s] == 'x') ln->oct |= 1 << s;
        if (src->ac[s] == 'x') ln->acc |= 1 << s;
        if (src->sl[s] == 'x') ln->sld |= 1 << s;
    }
}
static unsigned short decode_mask(const char *row) {
    unsigned short mk = 0;
    if (row) for (int s = 0; s < STEPS; s++) if (row[s] != '.') mk |= 1 << s;
    return mk;
}
static void load_demo(void) {
    memset(bank, 0, sizeof bank);
    for (int bk = 0; bk < NBANK; bk++) {
        Pattern *P = &bank[bk];
        const PatSrc *S = &DEMO[bk];
        decode_line(&P->ln[0], &S->a);
        decode_line(&P->ln[1], &S->b);
        for (int v = 0; v < N909; v++) {
            P->d909[v] = decode_mask(S->r909[v]);
            if (S->r909[v]) for (int s = 0; s < STEPS; s++)
                P->st909[v][s] = S->r909[v][s] == 'f' ? ST_FLAM
                               : S->r909[v][s] == 'd' ? ST_DRAG
                               : S->r909[v][s] == 'r' ? ST_RATCHET : ST_PLAIN;
        }
        P->acc909 = decode_mask(S->ac9);
        for (int v = 0; v < N808; v++) P->d808[v] = decode_mask(S->r808[v]);
        P->acc808 = decode_mask(S->ac8);
        if (S->pcf) for (int st = 0; st < STEPS; st++)
            P->pcf[st] = (S->pcf[st] >= '0' && S->pcf[st] <= '7') ? S->pcf[st] - '0' : 0;
    }
    chainN = 0;
    memset(chain, 0xFF, sizeof chain);
    for (const char *c = DEMO_CHAIN; *c && chainN < CHAINN; c++)
        chain[chainN++] = *c - 'A';
}

// ── save / load (autosaves the whole song) ────────────────────────────────
#define SAVE_MAGIC 0xAC1D0004u   // v4: per-device FX (sends + drum dist), MASTER strip knobs
typedef struct {
    unsigned magic;
    Pattern  bank[NBANK];
    unsigned char chain[CHAINN];
    int      chainN, tempo, editBank, swing;
    float    knob[2][NK], mcut, mres, fxk[NFX], send[4], dist9, dist8;
    int      wave[2];
    bool     songmode, mute[NSTRIP];
} SaveBlob;
static int  save_cooldown = 0;         // >0 → a save is pending
static void mark_dirty(void) { save_cooldown = 45; }
static void save_song(void) {
    SaveBlob sb; memset(&sb, 0, sizeof sb);
    sb.magic = SAVE_MAGIC;
    memcpy(sb.bank, bank, sizeof bank);
    memcpy(sb.chain, chain, sizeof chain);
    sb.chainN = chainN; sb.tempo = tempo; sb.editBank = editBank; sb.swing = swing;
    sb.mcut = mcut; sb.mres = mres;
    memcpy(sb.fxk, fxk, sizeof fxk);
    memcpy(sb.send, send, sizeof send); sb.dist9 = dist9; sb.dist8 = dist8;
    for (int i = 0; i < 2; i++) { memcpy(sb.knob[i], m[i].knob, sizeof m[i].knob); sb.wave[i] = m[i].wave; }
    sb.songmode = songmode;
    memcpy(sb.mute, mute, sizeof mute);
    save_bytes(&sb, sizeof sb);
}
static bool load_song(void) {
    SaveBlob sb;
    if (load_bytes(&sb, sizeof sb) != sizeof sb || sb.magic != SAVE_MAGIC) return false;
    memcpy(bank, sb.bank, sizeof bank);
    memcpy(chain, sb.chain, sizeof chain);
    chainN = sb.chainN; tempo = sb.tempo; editBank = sb.editBank; swing = sb.swing;
    swingf = (swing - 50) / 16.0f;
    mcut = sb.mcut; mres = sb.mres;
    memcpy(fxk, sb.fxk, sizeof fxk);
    memcpy(send, sb.send, sizeof send); dist9 = sb.dist9; dist8 = sb.dist8;
    for (int i = 0; i < 2; i++) { memcpy(m[i].knob, sb.knob[i], sizeof m[i].knob); m[i].wave = sb.wave[i]; }
    songmode = sb.songmode;
    memcpy(mute, sb.mute, sizeof sb.mute);
    return true;
}

// ── init ──────────────────────────────────────────────────────────────────
void init(void) {
    if (!load_song()) load_demo();
    define_303(&m[0]);
    define_303(&m[1]);
    define_909();
    define_808();
    bpm(tempo);
    // master insert chain — fx_order REPLACES the chain, so every pedal the
    // rack uses must be listed. Only the PCF filter lives on master now;
    // dist + delay are per-device (apply_fx configures the delay UNIT + sends)
    static const int kinds[1] = { FX_FILTER };
    fx_order(0, kinds, 1);
    apply_fx();
}

// ── per-finger surface routing (the dubdesk fix, dubdesk.c:97) ────────────
// owned = the finger was captured by a ui.h widget → raw surfaces skip it
// until it lifts. seen = the finger existed last frame → !seen = a fresh tap.
#define NFING 12
static int  own_ids[NFING], own_n = 0;
static int  seen_ids[NFING], seen_n = 0;
static bool is_owned(int id)  { for (int i = 0; i < own_n; i++)  if (own_ids[i]  == id) return true; return false; }
static bool was_seen(int id)  { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) return true; return false; }
static void own_add(int id)   { if (!is_owned(id) && own_n < NFING) own_ids[own_n++] = id; }
static void seen_add(int id)  { if (!was_seen(id) && seen_n < NFING) seen_ids[seen_n++] = id; }
static void own_drop(int id)  { for (int i = 0; i < own_n; i++)  if (own_ids[i]  == id) { own_ids[i]  = own_ids[--own_n];   return; } }
static void seen_drop(int id) { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) { seen_ids[i] = seen_ids[--seen_n]; return; } }

// ── update ────────────────────────────────────────────────────────────────
static void stop_all(void) { off_303(&m[0]); off_303(&m[1]); }

void update(void) {
    // keys
    if (keyp(KEY_SPACE)) { running = !running; last16 = -1; if (!running) stop_all(); }
    if (keyp(KEY_UP))    { tempo += 2; if (tempo > 200) tempo = 200; bpm(tempo); mark_dirty(); }
    if (keyp(KEY_DOWN))  { tempo -= 2; if (tempo < 60)  tempo = 60;  bpm(tempo); mark_dirty(); }
    if (keyp(KEY_LEFT))  { editBank = (editBank + NBANK - 1) % NBANK; }
    if (keyp(KEY_RIGHT)) { editBank = (editBank + 1) % NBANK; }
    if (keyp('S'))       { songmode = !songmode; mark_dirty(); }
    if (keyp('Z'))       { swing -= 2; if (swing < 50) swing = 50; swingf = (swing - 50) / 16.0f; mark_dirty(); }
    if (keyp('X'))       { swing += 2; if (swing > 66) swing = 66; swingf = (swing - 50) / 16.0f; mark_dirty(); }

    // ── raw-tap surfaces (roll / flags / grid / chain / headers) ──────────
    // sticky widget-ownership (the dubdesk fix): once ui.h captures a finger
    // (a knob/button grab) it stays owned until it LIFTS, so a knob drag that
    // wanders down over the roll never paints it. Capture is visible from the
    // frame after the press — geometry covers the press frame (no ui widget
    // sits on a raw surface). Every unowned finger paints independently.
    for (int i = 0; i < touch_count(); i++) if (ui_captured(touch_id(i))) own_add(touch_id(i));
    for (int i = 0; i < touch_ended_count(); i++) { own_drop(touch_ended_id(i)); seen_drop(touch_ended_id(i)); }

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (is_owned(id)) continue;            // finger belongs to a knob/button
        int px = touch_x(i), py = touch_y(i);
        bool tap = !was_seen(id);              // first frame of this finger
        seen_add(id);
        Pattern *P = &bank[editBank];

        // strip headers: tap to expand (press only, and not on the MUTE zone)
        if (tap) for (int i = 0; i < NSTRIP; i++) {
            int y = strip_y(i);
            if (py >= y && py < y + HDR_H && px < 250) { expanded = i; break; }
        }

        // song chain row: tap a cell to cycle A→B→C→D→empty
        if (tap && py >= CHAIN_Y && py < CHAIN_Y + 14 && px >= 34 && px < 34 + CHAINN * 4) {
            int c = (px - 34) / 4;
            chain[c] = (chain[c] == 0xFF) ? 0 : (chain[c] + 1 > 3 ? 0xFF : chain[c] + 1);
            chainN = 0; while (chainN < CHAINN && chain[chainN] != 0xFF) chainN++;
            mark_dirty();
        }

        // expanded 303 panel: roll + flag rows (paint on drag, erase on tap)
        if ((expanded == STRIP_A || expanded == STRIP_B) && !fxview[expanded]) {
            Line *ln = &P->ln[expanded];
            int y0 = strip_y(expanded) + HDR_H;
            int rx = 40, ry = y0 + 26;
            if (px >= rx && px < rx + STEPS * 15 && py >= ry && py < ry + 13 * 5) {
                int st = (px - rx) / 15, row = 12 - (py - ry) / 5, b = 1 << st;
                if (tap && (ln->on & b) && ln->pitch[st] == row) ln->on &= ~b;   // tap the note = erase
                else { ln->on |= b; ln->pitch[st] = (unsigned char)row; }
                mark_dirty();
            }
            int fy = ry + 13 * 5 + 2;
            if (tap && px >= rx && px < rx + STEPS * 15 && py >= fy && py < fy + 21) {
                int st = (px - rx) / 15, row = (py - fy) / 7, b = 1 << st;
                if (row == 0) ln->oct ^= b; else if (row == 1) ln->acc ^= b; else ln->sld ^= b;
                mark_dirty();
            }
        }

        // expanded 909 panel: trigger grid + accent row (SEQ view) or the
        // metal XY pad (FX view — a drag surface)
        if (expanded == STRIP_909 && !fxview[STRIP_909]) {
            int y0 = strip_y(STRIP_909) + HDR_H;
            int gx = 36, gy = y0 + 4;
            if (tap && px >= gx && px < gx + STEPS * 13 && py >= gy && py < gy + N909 * 9) {
                int st = (px - gx) / 13, v = (py - gy) / 9;
                P->d909[v] ^= 1 << st;
                if (!(P->d909[v] & (1 << st))) P->st909[v][st] = ST_PLAIN;
                mark_dirty();
            }
            if (tap && px >= gx && px < gx + STEPS * 13 && py >= gy + N909 * 9 + 2 && py < gy + N909 * 9 + 10) {
                P->acc909 ^= 1 << ((px - gx) / 13);
                mark_dirty();
            }
        }
        if (expanded == STRIP_909 && fxview[STRIP_909]) {
            int y0 = strip_y(STRIP_909) + HDR_H;
            if (px >= 190 && px < 250 && py >= y0 + 30 && py < y0 + 70) {
                mcut = (px - 190) / 60.0f;
                mres = 1.0f - (py - (y0 + 30)) / 40.0f;
                apply_metal_filter();
                mark_dirty();
            }
        }

        // expanded MASTER panel: drag the PCF lane (bar-graph levels per step)
        if (expanded == STRIP_MST) {
            int y0 = strip_y(STRIP_MST) + HDR_H;
            int gx = 36, ly = y0 + 34, lh = 72;
            if (px >= gx && px < gx + STEPS * 13 && py >= ly && py < ly + lh) {
                int st = (px - gx) / 13;
                int lvl = 7 - (py - ly) * 8 / lh; if (lvl < 0) lvl = 0; if (lvl > 7) lvl = 7;
                P->pcf[st] = (unsigned char)lvl;
                mark_dirty();
            }
        }

        // expanded 808 panel: trigger grid + accent row
        if (expanded == STRIP_808 && !fxview[STRIP_808]) {
            int y0 = strip_y(STRIP_808) + HDR_H;
            int gx = 36, gy = y0 + 6;
            if (tap && px >= gx && px < gx + STEPS * 13 && py >= gy && py < gy + N808 * 10) {
                int st = (px - gx) / 13, v = (py - gy) / 10;
                P->d808[v] ^= 1 << st;
                mark_dirty();
            }
            if (tap && px >= gx && px < gx + STEPS * 13 && py >= gy + N808 * 10 + 2 && py < gy + N808 * 10 + 10) {
                P->acc808 ^= 1 << ((px - gx) / 13);
                mark_dirty();
            }
        }
    }

    // right-click cycles a 909 cell through the stroke family (off cells join
    // at FLAM, like tr909) — desktop-only for now; long-press is the touch todo
    if (mouse_pressed(MOUSE_RIGHT) && expanded == STRIP_909 && !fxview[STRIP_909]) {
        Pattern *P = &bank[editBank];
        int y0 = strip_y(STRIP_909) + HDR_H;
        int gx = 36, gy = y0 + 4;
        int px = mouse_x(), py = mouse_y();
        if (px >= gx && px < gx + STEPS * 13 && py >= gy && py < gy + N909 * 9) {
            int st = (px - gx) / 13, v = (py - gy) / 9;
            if (!(P->d909[v] & (1 << st))) { P->d909[v] |= 1 << st; P->st909[v][st] = ST_FLAM; }
            else P->st909[v][st] = (unsigned char)((P->st909[v][st] + 1) % NSTROKE);
            mark_dirty();
        }
    }

    // deferred autosave (don't write the file on every painted cell)
    if (save_cooldown > 0 && --save_cooldown == 0) save_song();

    apply_fx();     // all gated on-change inside — cheap to call every frame
    ride_pcf();     // filter() is the one effect built to be ridden

#ifdef DE_TRACE
    watch("step", "%d", playhead);
    watch("bar",  "%d", barpos);
    watch("bank", "%d", playBank);
#endif

    // ── the clock — sixteenths off beat(), like tb303/drummachine ─────────
    if (!running) return;
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    int st  = s16 & 15;
    if (s16 != last16) {
        bool first_flip = (last16 < 0);
        bool newbar = (st == 0 && !first_flip) || first_flip;
        last16 = s16;
        playhead = st;
        if (newbar) {                              // pick the bar's bank
            if (songmode && chainN > 0) {
                playBank = chain[barpos % chainN];
                barpos = (barpos + 1) % (chainN > 0 ? chainN : 1);
            } else playBank = editBank;
        }
        Pattern *P = &bank[playBank];
        // 303s trigger on the flip (a held voice can't be scheduled ahead;
        // they also don't shuffle — noted in the design doc, revisit if the
        // straight-303-against-swung-drums clash bothers the ears at 60+)
        if (!mute[STRIP_A]) step_303(&m[0], &P->ln[0], st); else off_303(&m[0]);
        if (!mute[STRIP_B]) step_303(&m[1], &P->ln[1], st); else off_303(&m[1]);

        // drums: schedule the NEXT step sample-accurate (the tr909 pattern) —
        // that's what makes shuffle and flam graces land between the frames
        float f = beat_pos() * 4.0f; f -= (int)f;
        int step_ms = 15000 / tempo;
        int delay   = (int)((1.0f - f) * step_ms);
        int nx      = (s16 + 1) & 15;
        if (nx & 1) delay += (swing - 50) * 2 * step_ms / 100;   // even 16ths drag
        // the next step's bank: at a bar seam, peek the chain
        Pattern *N = P;
        if (nx == 0 && songmode && chainN > 0) N = &bank[chain[barpos % chainN]];
        else if (nx == 0 && !songmode) N = &bank[editBank];
        if (!mute[STRIP_909]) {
            int boost = (N->acc909 >> nx) & 1 ? 2 : 0;
            for (int v = 0; v < N909; v++)
                if (N->d909[v] & (1 << nx)) fire_stroke909(v, N->st909[v][nx], boost, delay, step_ms);
        }
        if (!mute[STRIP_808]) {
            int boost = (N->acc808 >> nx) & 1 ? 2 : 0;
            for (int v = 0; v < N808; v++)
                if (N->d808[v] & (1 << nx)) fire808(v, boost, delay);
        }
        if (first_flip) {   // fresh start: also sound the step we're already on
            int b9 = (P->acc909 >> st) & 1 ? 2 : 0, b8 = (P->acc808 >> st) & 1 ? 2 : 0;
            if (!mute[STRIP_909]) for (int v = 0; v < N909; v++)
                if (P->d909[v] & (1 << st)) fire909(v, b9, 0);
            if (!mute[STRIP_808]) for (int v = 0; v < N808; v++)
                if (P->d808[v] & (1 << st)) fire808(v, b8, 0);
        }
    } else {
        Pattern *P = &bank[playBank];
        gate_303(&m[0], &P->ln[0], st);
        gate_303(&m[1], &P->ln[1], st);
    }
}

// ── draw ──────────────────────────────────────────────────────────────────
static const int BANKCLR[NBANK] = { CLR_ORANGE, CLR_GREEN, CLR_BLUE, CLR_RED };

static void draw_header(int i, int y) {
    bool open = (i == expanded);
    rectfill(2, y, 316, HDR_H - 2, open ? CLR_DARKER_GREY : CLR_DARK_GREY);
    print(open ? "v" : ">", 8, y + 5, CLR_LIGHT_GREY);
    print(SNAME[i], 18, y + 5, mute[i] ? CLR_MEDIUM_GREY : CLR_WHITE);
    // live mini pattern — the "folded isn't blind" ticks
    Pattern *P = &bank[playBank];
    for (int s = 0; s < STEPS; s++) {
        int tx = 96 + s * 6;
        bool onn = false;
        if      (i == STRIP_909) { for (int d = 0; d < N909; d++) if (P->d909[d] & (1 << s)) { onn = true; break; } }
        else if (i == STRIP_808) { for (int d = 0; d < N808; d++) if (P->d808[d] & (1 << s)) { onn = true; break; } }
        else if (i == STRIP_MST) onn = P->pcf[s] > 0;
        else onn = (P->ln[i].on >> s) & 1;
        int c = onn ? (mute[i] ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_DARKER_GREY;
        if (running && s == playhead) c = CLR_WHITE;
        rectfill(tx, y + 6, 4, 6, c);
    }
    // activity LED
    bool lit = false;
    if      (i == STRIP_909) { for (int d = 0; d < N909; d++) if (flash909[d] > 0) lit = true; }
    else if (i == STRIP_808) { for (int d = 0; d < N808; d++) if (flash808[d] > 0) lit = true; }
    else if (i == STRIP_MST) lit = pcf_on || fxk[F_GLU] > 0.02f;
    else lit = (m[i].h >= 0);
    circfill(206, y + 9, 2, lit && !mute[i] ? CLR_GREEN : CLR_DARKER_GREY);
    // view + MUTE toggles (ui.h buttons — per-finger capture, focus ring)
    if (i != STRIP_MST && ui_button(216, y + 2, 32, HDR_H - 4, fxview[i] ? "seq" : "fx"))
        fxview[i] = !fxview[i];
    if (ui_button(252, y + 2, 34, HDR_H - 4, mute[i] ? "MUTED" : "mute")) { mute[i] = !mute[i]; mark_dirty(); }
}

static void draw_303_panel(int i, int y0) {
    M303 *s = &m[i];
    Line *ln = &bank[editBank].ln[i];
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);

    // knob row
    for (int k = 0; k < NK; k++)
        if (ui_knob(&s->knob[k], 26 + k * 38, y0 + 12, KNAME[k])) { knob_changed_303(s, k); mark_dirty(); }
    if (ui_button(288, y0 + 4, 28, 16, s->wave == INSTR_SAW ? "SAW" : "SQR")) {
        s->wave = (s->wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW;
        define_303(s); mark_dirty();
    }

    // piano roll: 13 rows (root..octave), playhead column, root rows tinted
    int rx = 40, ry = y0 + 26;
    for (int st = 0; st < STEPS; st++) {
        int cx = rx + st * 15;
        int bg = (running && st == playhead) ? CLR_DARKER_GREY : CLR_BLACK;
        rectfill(cx, ry, 14, 13 * 5 - 1, bg);
        for (int r = 0; r <= 12; r += 12) {  // root + octave guide rows
            rectfill(cx, ry + (12 - r) * 5, 14, 4, bg == CLR_BLACK ? CLR_DARKER_GREY : CLR_DARK_GREY);
        }
        if ((ln->on >> st) & 1) {
            int row = ln->pitch[st];
            int c = ((ln->acc >> st) & 1) ? CLR_RED : BANKCLR[editBank];
            rectfill(cx, ry + (12 - row) * 5, 14, 4, c);
            if ((ln->sld >> st) & 1) rectfill(cx + 12, ry + (12 - row) * 5 + 1, 4, 2, CLR_WHITE);
        }
    }
    font(FONT_SMALL);
    print("C3", 2, ry + 4, CLR_MEDIUM_GREY);
    print("C2", 2, ry + 12 * 5 - 5, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // flag rows
    static const char *FLAG[3] = { "OCT", "ACC", "SLD" };
    int fy = ry + 13 * 5 + 2;
    font(FONT_SMALL);
    for (int r = 0; r < 3; r++) {
        print(FLAG[r], 14, fy + r * 7, CLR_MEDIUM_GREY);
        unsigned short mask = r == 0 ? ln->oct : r == 1 ? ln->acc : ln->sld;
        for (int st = 0; st < STEPS; st++)
            rectfill(rx + st * 15, fy + r * 7, 14, 6,
                     (mask >> st) & 1 ? (r == 1 ? CLR_RED : CLR_YELLOW) : CLR_DARKER_GREY);
    }
    font(FONT_NORMAL);
}

// one grid implementation, two voice tables — the 808/909 shared skeleton.
// strokes draws tr909's tick marks; row pitch rh differs (11 vs 9 voices).
static void draw_drum_grid(const char **names, int nv, unsigned short *rows,
                           unsigned char st_rows[][STEPS], unsigned short accmask,
                           int *flash, int gx, int gy, int rh) {
    font(FONT_SMALL);
    for (int v = 0; v < nv; v++) {
        print(names[v], 12, gy + v * rh + 2, flash[v] > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
        for (int st = 0; st < STEPS; st++) {
            int cx = gx + st * 13;
            bool onn = (rows[v] >> st) & 1;
            int c = onn ? BANKCLR[editBank] : ((st & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
            if (running && st == playhead && onn) c = CLR_WHITE;
            rectfill(cx, gy + v * rh, 12, rh - 1, c);
            if (onn && st_rows && st_rows[v][st] != ST_PLAIN)   // stroke ticks
                for (int t = 0; t <= (int)st_rows[v][st] - ST_FLAM; t++)
                    rectfill(cx + 2 + t * 3, gy + v * rh + rh - 3, 2, 2, CLR_BLACK);
        }
    }
    // TOTAL ACCENT row
    print("AC", 12, gy + nv * rh + 3, CLR_RED);
    for (int st = 0; st < STEPS; st++)
        rectfill(gx + st * 13, gy + nv * rh + 2, 12, 6,
                 (accmask >> st) & 1 ? CLR_RED : CLR_DARKER_GREY);
    font(FONT_NORMAL);
}

static void draw_909_panel(int y0) {
    Pattern *P = &bank[editBank];
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    draw_drum_grid(NAME909, N909, P->d909, P->st909, P->acc909, flash909, 36, y0 + 4, 9);
    font(FONT_SMALL);
    print("rclick cell", 252, y0 + 86, CLR_DARK_GREY);
    print("= strokes", 252, y0 + 94, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

// ── the per-machine FX views (the [fx] header button) — RB-338 style: ─────
// each device owns its dist + its send into THE shared delay unit
static void draw_303_fx(int i, int y0) {
    M303 *s = &m[i];
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    if (ui_knob(&s->knob[K_DRV], 26, y0 + 12, "DIST")) { knob_changed_303(s, K_DRV); mark_dirty(); }
    if (ui_knob(&send[i], 64, y0 + 12, "SEND")) mark_dirty();
    font(FONT_SMALL);
    print("DIST = this box's drive (same as DRV on the seq view)", 12, y0 + 40, CLR_DARK_GREY);
    print("SEND = its level into the shared delay unit", 12, y0 + 50, CLR_DARK_GREY);
    print("delay TIME/FB live on the MASTER strip", 12, y0 + 60, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
static void draw_909_fx(int y0) {
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    if (ui_knob(&dist9, 26, y0 + 12, "DIST")) mark_dirty();
    if (ui_knob(&send[2], 64, y0 + 12, "SEND")) mark_dirty();
    // the metal-filter XY pad (X = five metal highpass cutoffs, Y = resonance)
    int padx = 190, pady = y0 + 30;
    rectfill(padx, pady, 60, 40, CLR_DARKER_GREY);
    rect(padx, pady, 60, 40, CLR_DARK_GREY);
    circfill(padx + (int)(mcut * 60), pady + (int)((1.0f - mres) * 40), 2, CLR_YELLOW);
    // master shuffle — the 909 is where Roland shipped it (Z/X keys too)
    if (ui_slider(&swingf, 254, y0 + 44, 56, "SHUF")) { swing = 50 + (int)(swingf * 16.0f); mark_dirty(); }
    font(FONT_SMALL);
    print("METAL", padx + 18, pady + 42, CLR_MEDIUM_GREY);
    print("DIST rides every 909 voice; SEND feeds the delay", 12, y0 + 84, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
static void draw_808_fx(int y0) {
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    if (ui_knob(&dist8, 26, y0 + 12, "DIST")) mark_dirty();
    if (ui_knob(&send[3], 64, y0 + 12, "SEND")) mark_dirty();
    font(FONT_SMALL);
    print("DIST rides every 808 voice; SEND feeds the delay", 12, y0 + 84, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

static void draw_808_panel(int y0) {
    Pattern *P = &bank[editBank];
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    draw_drum_grid(NAME808, N808, P->d808, NULL, P->acc808, flash808, 36, y0 + 6, 10);
    font(FONT_SMALL);
    print("the boom box", 254, y0 + 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

static void draw_master_panel(int y0) {
    Pattern *P = &bank[editBank];
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    // knob row: TIME/FB (the shared delay unit) · GLU · PCF/RES
    for (int k = 0; k < NFX; k++)
        if (ui_knob(&fxk[k], 26 + k * 38, y0 + 12, FXNAME[k])) mark_dirty();
    // the PCF lane — bar-graph levels, one per step, per BANK (it's part of
    // the arrangement: the demo song's build is a ramp drawn here)
    int gx = 36, ly = y0 + 34, lh = 72;
    for (int st = 0; st < STEPS; st++) {
        int cx = gx + st * 13;
        rectfill(cx, ly, 12, lh, (st & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
        int lvl = P->pcf[st];
        int bh = lvl * lh / 7;
        int c = (running && st == playhead) ? CLR_WHITE : BANKCLR[editBank];
        if (bh > 0) rectfill(cx, ly + lh - bh, 12, bh, c);
    }
    font(FONT_SMALL);
    print("PCF", 12, ly + 2, CLR_MEDIUM_GREY);
    print("lane", 12, ly + 10, CLR_MEDIUM_GREY);
    print("sends live in each", 240, y0 + 92, CLR_DARK_GREY);
    print("machine's fx view", 240, y0 + 100, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    ui_begin();

    // ── transport bar ─────────────────────────────────────────────────────
    rectfill(0, 0, 320, TB_H, CLR_BLACK);
    if (ui_button(4, 2, 36, 18, running ? "STOP" : "RUN")) {
        running = !running; last16 = -1; if (!running) stop_all();
    }
    char buf[24];
    snprintf(buf, sizeof buf, "%d", tempo);
    print("BPM", 46, 8, CLR_MEDIUM_GREY);
    print(buf, 70, 8, CLR_WHITE);
    if (ui_button(94, 2, 12, 18, "-")) { tempo -= 2; if (tempo < 60) tempo = 60; bpm(tempo); mark_dirty(); }
    if (ui_button(108, 2, 12, 18, "+")) { tempo += 2; if (tempo > 200) tempo = 200; bpm(tempo); mark_dirty(); }
    for (int bk = 0; bk < NBANK; bk++) {
        char nm[2] = { (char)('A' + bk), 0 };
        int x = 136 + bk * 20;
        if (ui_button(x, 2, 17, 18, nm)) editBank = bk;
        if (bk == editBank) rect(x, 2, 17, 18, BANKCLR[bk]);
        if (running && bk == playBank) rectfill(x + 6, 17, 5, 2, CLR_WHITE);
    }
    if (ui_button(226, 2, 42, 18, songmode ? "SONG" : "LOOP")) { songmode = !songmode; barpos = 0; mark_dirty(); }
    font(FONT_SMALL);
    print("acid rack", 272, 9, CLR_INDIGO);
    font(FONT_NORMAL);

    // ── strips ────────────────────────────────────────────────────────────
    for (int i = 0; i < NSTRIP; i++) {
        int y = strip_y(i);
        draw_header(i, y);
        if (i == expanded) {
            if      (i == STRIP_MST) draw_master_panel(y + HDR_H);
            else if (fxview[i]) {
                if      (i == STRIP_909) draw_909_fx(y + HDR_H);
                else if (i == STRIP_808) draw_808_fx(y + HDR_H);
                else                     draw_303_fx(i, y + HDR_H);
            }
            else if (i == STRIP_909) draw_909_panel(y + HDR_H);
            else if (i == STRIP_808) draw_808_panel(y + HDR_H);
            else draw_303_panel(i, y + HDR_H);
        }
    }

    // ── song chain row ────────────────────────────────────────────────────
    print("SONG", 4, CHAIN_Y + 3, songmode ? CLR_WHITE : CLR_MEDIUM_GREY);
    for (int c = 0; c < CHAINN; c++) {
        int x = 34 + c * 4;
        int bk = chain[c];
        int col = (bk == 0xFF) ? CLR_DARK_GREY : BANKCLR[bk];
        rectfill(x, CHAIN_Y + 2, 3, 10, col);
        if (songmode && running && chainN > 0 && c == (barpos + chainN - 1) % chainN)
            rectfill(x, CHAIN_Y, 3, 2, CLR_WHITE);
    }

    for (int d = 0; d < N909; d++) if (flash909[d] > 0) flash909[d]--;
    for (int d = 0; d < N808; d++) if (flash808[d] > 0) flash808[d]--;
    ui_end();
}
