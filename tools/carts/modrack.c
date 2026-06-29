/* de:meta
{
  "title": "modrack",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "instrument",
    "probe"
  ],
  "teaches": [
    "generative-melody",
    "step-sequencer",
    "euclidean-rhythm",
    "algorithm-visualization"
  ],
  "lineage": "Berlin-school modular synthesis (Turing Machine, Grids, Marbles, Maths clones) implemented as a data-driven module registry; novel in making the patch graph editable and type-checked in a PICO-8-scale cart.",
  "description": "A tiny modular synth, built in steps. STEP 1: the generative Berlin-school chain is hardcoded in C — CLOCK ticks a SAMPLE & HOLD that freezes a wandering LFO, a QUANTIZER snaps it to a scale (always in key), and a VOICE plays it as a held note whose filter the same LFO sweeps live. Endless in-key melody, zero composition. Later steps draw editable module strips and patch cables. Built on the held-note API (note_on/note_pitch/note_cutoff)."
}
de:meta */
#include "studio.h"
#include "gestures.h"   // pinch_scale() — two-finger zoom on touch (the wheel has no finger)
#include "cursor.h"     // pixel mouse cursor (grab while dragging a knob/cable/module/canvas)
#include <math.h>   // sinf — bakes the org/vox/bel/fld user-wave tables in init()

// MODRACK — a tiny modular synth (see docs/design/modular-synth.md).
//
// STEP 1 of the patcher rebuild: modules are now DATA, not hardcoded. A ModType registry
// describes each kind's jacks + knobs; Module instances hold a type, a position, and their
// own param/state. Eval and draw are generic (a switch per kind). Same default patch, same
// look & sound — but now adding a module is a registry entry, and you can have several of
// the same kind. Sets up the palette + drag + zoom steps next.
//
// Patch: drag an output jack to an input (type-checked green=gate/blue=cv/yellow=pitch),
// click an occupied input to rewire, right-click a jack to clear. Knobs drag. S/L/R = save/load/reset.

#define ROOT_OCT 4

// everything is on a 12px cell grid. modules are a whole number of cells; the current
// kinds are all 6x7 cells (72x84). drag-placement snaps to this grid.
#define CELL 12
#define GW  72   // 6 cells wide
#define GH  84   // 7 cells tall
#define GX  12
#define GSP 84   // 7 cells (module + 1 gap)
#define GY  12
#define GRP 96   // 8 cells
#define bayx(i) (GX + ((i) % 4) * GSP)
#define bayy(i) (GY + ((i) / 4) * GRP)
int snap12(int v) { return ((v < 0 ? v - CELL / 2 : v + CELL / 2) / CELL) * CELL; }

const char *NOTES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
const char *SCALES[6] = { "maj","min","pen","pnm","blu","chr" };

// ── module type registry ──
enum { MOD_CLOCK, MOD_LFO, MOD_SH, MOD_QUANT, MOD_VOICE, MOD_EUCLID, MOD_ENV, MOD_DRUM,
       MOD_SLEW, MOD_ATTN, MOD_LOGIC, MOD_SCOPE, MOD_KEYS, MOD_TURING, MOD_GRIDS, MOD_MARBLES, MOD_MATHS,
       MOD_SEQ, MOD_VIBRATO, MOD_CHANCE, MOD_MACRO, MOD_XPOSE, MOD_MIX, MOD_CMP, MOD_DIV, MOD_ADSR,
       MOD_RVB, MOD_ECHO, MOD_DRIVE, MOD_CRUSH, MOD_SAT, MOD_WAH, MOD_VOWEL, MOD_EQ, MOD_FILT, MOD_GRAINS,
       MOD_TIDES, NTYPE };   // RVB..GRAINS = FX (see apply_master_fx). TIDES = a morphing LFO / function gen (CV)
enum { FMT_INT, FMT_F1, FMT_SCALE, FMT_NOTE, FMT_MS, FMT_LOGIC, FMT_WAVE, FMT_FILTER, FMT_DEST, FMT_ENGINE, FMT_OCT, FMT_DIV, FMT_PCT, FMT_DRIVE, FMT_LFOSHAPE };

typedef struct { int type; bool out; int dx, dy; const char *label; } JackDef;   // type: 0 gate/1 pitch/2 cv
typedef struct { const char *label; float lo, hi, def; int dx, dy, fmt; } KnobDef;
typedef struct {
    const char *name; int col, cw, ch;   // size in 12px cells
    int njack; JackDef jack[8];
    int nknob; KnobDef knob[8];
} ModType;

ModType TYPES[NTYPE] = {
    // 4-cell (48px): LFO S&H ENV DRUM  |  5-cell (60px): CLOCK QUANT EUCLID TURING MARBLES MATHS  |  6-cell (72px): VOICE KEYS SCOPE GRIDS
    [MOD_CLOCK] = { "CLOCK", CLR_ORANGE, 5, 6, 3, {{0,true,12,60,"1"},{0,true,30,60,"2"},{0,true,48,60,"4"}},
                   2, {{"bpm",60,240,112,18,30,FMT_INT},{"swg",0,0.6f,0,44,30,FMT_PCT}} },
    [MOD_LFO]   = { "LFO", CLR_PINK, 4, 6, 1, {{2,true,24,60,"cv"}},
                   2, {{"rate",0.1f,8,0.37f,24,26,FMT_F1},{"shp",0,5.99f,0,24,44,FMT_LFOSHAPE}} },
    [MOD_SH]    = { "S&H", CLR_YELLOW, 4, 6, 3, {{2,false,8,60,"in"},{0,false,24,60,"clk"},{2,true,40,60,"cv"}}, 0, {} },
    [MOD_QUANT] = { "QUANT", CLR_GREEN, 5, 6, 2, {{2,false,18,60,"in"},{1,true,42,60,"pit"}},
                   2, {{"scl",0,5.99f,SCALE_PENTA,18,28,FMT_SCALE},{"root",0,11.99f,0,42,28,FMT_NOTE}} },
    [MOD_VOICE] = { "VOICE", CLR_BLUE, 6, 8, 8, {{0,false,3,84,"g"},{1,false,13,84,"p"},{2,false,23,84,"f"},{2,false,33,84,"r"},{2,false,43,84,"w"},{2,false,53,84,"a"},{0,false,63,84,"vb"},{3,true,70,18,"~"}},   // jack 7 = audio out (pink) → patch into an FX module to effect just this voice
                   8, {{"cut",200,2200,700,20,28,FMT_INT},{"res",0,15,6,52,28,FMT_INT},
                       {"pw",0.05f,0.95f,0.5f,12,46,FMT_F1},{"wav",0,8.99f,0,36,46,FMT_WAVE},{"flt",0,3.99f,0,58,46,FMT_FILTER},
                       {"fenv",0,3000,0,14,64,FMT_INT},{"penv",-48,48,0,36,64,FMT_INT},{"denv",0,1,0,58,64,FMT_F1}} },   // vb = VIBE patch point; a = amp/VCA CV; flt = filter mode; denv = duty (PWM) env, square/pulse only
    [MOD_EUCLID]= { "EUCLID", CLR_RED, 5, 6, 2, {{0,false,18,60,"clk"},{0,true,42,60,"g"}},
                   2, {{"h",1,8.99f,4,18,26,FMT_INT},{"s",2,16.99f,8,42,26,FMT_INT}} },
    [MOD_ENV]   = { "ENV", CLR_MEDIUM_GREEN, 4, 6, 2, {{0,false,14,60,"g"},{2,true,34,60,"cv"}},
                   2, {{"atk",0.005f,0.5f,0.01f,14,32,FMT_MS},{"dec",0.02f,1,0.25f,34,32,FMT_MS}} },
    [MOD_DRUM]  = { "DRUM", CLR_DARK_ORANGE, 5, 5, 4, {{0,false,8,48,"k"},{0,false,24,48,"s"},{0,false,40,48,"h"},{3,true,56,30,"~"}}, 0, {} },   // jack 3 = audio out (all 3 drum voices)
    // ── tiny utilities (3x5 = 36x60, LOGIC 4x5 = 48x60) ──
    [MOD_SLEW]  = { "SLEW", CLR_INDIGO, 3, 5, 2, {{2,false,9,48,"in"},{2,true,27,48,"out"}},
                   1, {{"ms",1,500,80,18,32,FMT_INT}} },
    [MOD_ATTN]  = { "ATTN", CLR_MAUVE, 3, 5, 2, {{2,false,9,48,"in"},{2,true,27,48,"out"}},
                   1, {{"amt",0,1,1,18,32,FMT_F1}} },
    [MOD_LOGIC] = { "LOGIC", CLR_LIGHT_YELLOW, 4, 5, 3, {{0,false,10,48,"a"},{0,false,24,48,"b"},{0,true,38,48,"o"}},
                   1, {{"mod",0,2.99f,0,24,26,FMT_LOGIC}} },
    [MOD_SCOPE] = { "SCOPE", CLR_LIGHT_GREY, 6, 4, 1, {{2,false,36,38,"in"}}, 0, {} },
    [MOD_KEYS]  = { "KEYS", CLR_LIGHT_PEACH, 6, 5, 2, {{0,true,24,52,"g"},{1,true,48,52,"p"}}, 0, {} },
    // ── famous-module-inspired ──
    [MOD_TURING]= { "TURING", CLR_TRUE_BLUE, 5, 6, 2, {{0,false,18,60,"clk"},{2,true,42,60,"cv"}},
                   2, {{"rnd",0,1,0.4f,18,26,FMT_F1},{"len",2,16.99f,8,42,26,FMT_INT}} },
    [MOD_GRIDS] = { "GRIDS", CLR_DARK_ORANGE, 6, 6, 4, {{0,false,10,60,"clk"},{0,true,26,60,"k"},{0,true,44,60,"s"},{0,true,62,60,"h"}},
                   2, {{"map",0,1,0.3f,22,26,FMT_F1},{"fill",0,1,0.5f,50,26,FMT_F1}} },
    [MOD_MARBLES]={ "MARBLES", CLR_LIME_GREEN, 5, 7, 3, {{0,false,10,72,"clk"},{0,true,30,72,"g"},{2,true,50,72,"cv"}},
                   2, {{"dens",0,1,0.6f,16,40,FMT_F1},{"sprd",0,1,0.7f,44,40,FMT_F1}} },
    [MOD_MATHS] = { "MATHS", CLR_BLUE_GREEN, 5, 6, 3, {{0,false,10,60,"t"},{2,true,30,60,"cv"},{0,true,50,60,"eoc"}},
                   3, {{"rise",0.005f,2,0.1f,12,26,FMT_MS},{"fall",0.005f,2,0.3f,30,26,FMT_MS},{"cyc",0,1,0,48,26,FMT_F1}} },
    // ── new modules ──
    [MOD_SEQ]     = { "SEQ", CLR_PEACH, 6, 7, 3, {{0,false,14,72,"clk"},{2,true,36,72,"cv"},{0,true,58,72,"g"}},
                     8, {{"1",0,1,0,    8,26,FMT_F1},{"2",0,1,0.14f,24,26,FMT_F1},{"3",0,1,0.28f,40,26,FMT_F1},{"4",0,1,0.43f,56,26,FMT_F1},
                         {"5",0,1,0.57f,8,42,FMT_F1},{"6",0,1,0.71f,24,42,FMT_F1},{"7",0,1,0.85f,40,42,FMT_F1},{"8",0,1,1,    56,42,FMT_F1}} },
    [MOD_VIBRATO] = { "VIBE", CLR_DARK_BLUE, 4, 5, 2, {{0,false,9,48,"g"},{0,true,27,48,"out"}},
                     3, {{"rate",0.5f,14,5.5f,8,28,FMT_F1},{"dpt",0,1,0.3f,24,28,FMT_F1},{"dst",0,6.99f,0,40,28,FMT_DEST}} },
    [MOD_CHANCE]  = { "CHANCE", CLR_BROWN, 4, 5, 2, {{0,false,14,48,"in"},{0,true,34,48,"out"}},
                     1, {{"prob",0,1,0.5f,24,28,FMT_F1}} },
    // Plaits-style macro voice (audio-notes §8): eng picks a modeled engine, the three
    // 0..1 macros (har/tmb/mor) mean something different per engine; h/t/m CV inlets ADD
    // to their knobs (full range — patch an ATTN before the inlet to set depth)
    [MOD_MACRO]   = { "MACRO", CLR_DARK_PURPLE, 6, 8, 7, {{0,false,4,86,"g"},{1,false,16,86,"p"},{2,false,28,86,"h"},{2,false,40,86,"t"},{2,false,52,86,"m"},{0,false,64,86,"vb"},{3,true,70,18,"~"}},   // jack 6 = audio out (pink)
                     7, {{"eng",0,13.99f,0,20,28,FMT_ENGINE},{"har",0,1,0.5f,52,28,FMT_F1},
                         {"tmb",0,1,0.5f,20,48,FMT_F1},{"mor",0,1,0.5f,52,48,FMT_F1},
                         {"drv",0,1,0,14,68,FMT_F1},{"eko",0,1,0,36,68,FMT_F1},{"tun",-12,12,0,58,68,FMT_F1}} },
    // pitch utility: a precision adder — shifts any pitch line by whole octaves (the only
    // way to reach the bass register; QUANT's root knob is semitones-up within ROOT_OCT)
    [MOD_XPOSE]   = { "XPOSE", CLR_DARK_PEACH, 3, 5, 2, {{1,false,9,48,"in"},{1,true,27,48,"out"}},
                     1, {{"oct",-2,2,0,18,32,FMT_OCT}} },
    // cv mixer with attenuverters — the only way to COMBINE two modulators into one input
    // (every input jack holds a single cable). a/b go -1..+1 so it also inverts; off is the
    // base level an inverted signal swings down from (a=-1, off=1 -> an upside-down LFO).
    [MOD_MIX]     = { "MIX", CLR_MEDIUM_GREY, 5, 5, 3, {{2,false,12,48,"a"},{2,false,30,48,"b"},{2,true,48,48,"out"}},
                     3, {{"a",-1,1,1,12,26,FMT_F1},{"b",-1,1,1,30,26,FMT_F1},{"off",0,1,0,48,26,FMT_F1}} },
    // comparator — the cv→gate conversion: gate high while in > thr. An LFO through it is a
    // clock whose pulse width IS the threshold; ramps become rhythms, randomness becomes triggers
    [MOD_CMP]     = { "CMP", CLR_DARK_GREEN, 3, 5, 2, {{2,false,9,48,"in"},{0,true,27,48,"out"}},
                     1, {{"thr",0,1,0.5f,18,32,FMT_F1}} },
    // clock divider — passes every Nth gate; the /3 /8 /16 that CLOCK's fixed /1 /2 /4 can't do
    [MOD_DIV]     = { "DIV", CLR_ORANGE, 3, 5, 2, {{0,false,9,48,"in"},{0,true,27,48,"out"}},
                     1, {{"div",2,16.99f,4,18,32,FMT_DIV}} },
    // full envelope with a SUSTAIN stage — unlike AD-only ENV, a held gate HOLDS at sus and
    // gate-fall starts the release. Patch into VOICE 'a' and held notes finally breathe.
    [MOD_ADSR]    = { "ADSR", CLR_DARK_RED, 5, 7, 2, {{0,false,14,72,"g"},{2,true,40,72,"cv"}},
                     4, {{"atk",0.005f,1,0.05f,15,28,FMT_MS},{"dec",0.02f,1,0.2f,43,28,FMT_MS},
                         {"sus",0,1,0.7f,15,50,FMT_F1},{"rel",0.02f,2,0.5f,43,50,FMT_MS}} },
    // ── master-bus FX (no output jack — they're SINKS that dirty up the whole rack). The cv
    // inlet ADDS to the wet/amount knob, so an LFO/ENV/MATHS can sweep a pedal. Applied
    // globally in apply_master_fx() (NOT eval_mod): only the first module of a kind drives
    // the bus; the rest sit dormant. drive is per-voice (slots 5..22), the rest are whole-mix.
    [MOD_RVB]   = { "VERB", CLR_INDIGO, 4, 6, 2, {{2,false,24,60,"cv"},{3,false,4,16,"in"}},
                    2, {{"size",0.1f,0.95f,0.6f,14,30,FMT_F1},{"mix",0,1,0.35f,34,30,FMT_F1}} },
    [MOD_ECHO]  = { "ECHO", CLR_BLUE_GREEN, 5, 6, 2, {{2,false,30,60,"cv"},{3,false,4,16,"in"}},
                    3, {{"time",40,800,300,12,30,FMT_INT},{"fb",0,0.85f,0.4f,30,30,FMT_F1},{"mix",0,1,0.35f,48,30,FMT_F1}} },
    [MOD_DRIVE] = { "DRIVE", CLR_BROWN, 4, 6, 2, {{2,false,24,60,"cv"},{3,false,4,16,"in"}},
                    2, {{"amt",0,1,0.4f,14,30,FMT_F1},{"mode",0,3.99f,0,34,30,FMT_DRIVE}} },
    [MOD_CRUSH] = { "CRUSH", CLR_MAUVE, 4, 6, 2, {{2,false,24,60,"cv"},{3,false,4,16,"in"}},
                    2, {{"bits",2,16,8,14,30,FMT_INT},{"mix",0,1,0.5f,34,30,FMT_F1}} },
    // SAT = whole-MIX saturation (drive_insert / FX_DRIVE) — unlike DRIVE (per-VOICE, post-filter
    // acid scream), this drives the SUMMED bus so drums + MACRO grit up too: tube glue low, lo-fi
    // wall high. amt + mode + dry/wet mix; cv adds to amt.
    [MOD_SAT]   = { "SAT", CLR_ORANGE, 5, 6, 1, {{2,false,30,60,"cv"}},
                    3, {{"amt",0,1,0.4f,12,30,FMT_F1},{"mode",0,3.99f,3,30,30,FMT_DRIVE},{"mix",0,1,0.8f,48,30,FMT_F1}} },
    // WAH = LFO auto-wah (rhythmic "wah-wah"). VOWEL = formant/vowel filter. Both dual-mode:
    // unpatched = whole mix; pink ~ in = just that part (private bus). cv → mix (WAH) / vowel (VOWEL).
    [MOD_WAH]   = { "WAH", CLR_LIME_GREEN, 5, 6, 2, {{2,false,30,60,"cv"},{3,false,4,16,"in"}},
                    3, {{"rate",0.5f,10,4,12,30,FMT_F1},{"res",0,1,0.6f,30,30,FMT_F1},{"mix",0,1,0.7f,48,30,FMT_F1}} },
    [MOD_VOWEL] = { "VOWL", CLR_DARK_PEACH, 5, 6, 2, {{2,false,30,60,"cv"},{3,false,4,16,"in"}},
                    3, {{"vow",0,1,0.3f,12,30,FMT_F1},{"q",0,1,0.6f,30,30,FMT_F1},{"mix",0,1,0.7f,48,30,FMT_F1}} },
    // EQ = 3-band tone (the only BOOST in the rack), gains in dB ±12, 0/0/0 = flat. Dual-mode; cv→hi
    // (brightness automation). Patch a source ~ into 'in' to EQ just that part.
    [MOD_EQ]    = { "EQ", CLR_LIGHT_GREY, 5, 6, 2, {{2,false,30,60,"cv"},{3,false,4,16,"in"}},
                    3, {{"lo",-12,12,0,12,30,FMT_INT},{"mid",-12,12,0,30,30,FMT_INT},{"hi",-12,12,0,48,30,FMT_INT}} },
    // FILT = the DJ filter (master-only — its per-part form would fight VOICE's own filter). The cv
    // inlet SWEEPS the cutoff, so patch an LFO/ENV → a filter sweep on the whole mix. mode lp/hp/bp/nt.
    [MOD_FILT]  = { "FILT", CLR_TRUE_BLUE, 5, 6, 1, {{2,false,30,60,"cv"}},
                    3, {{"mode",0,3.99f,0,12,30,FMT_FILTER},{"cut",100,12000,1200,30,30,FMT_INT},{"res",0,1,0.3f,48,30,FMT_F1}} },
    // GRAINS = granular cloud (capture + scatter the recent past). Dual-mode. size=grain length,
    // dens=grains/sec, mix=wet; position/scatter/feedback fixed to an evolving-cloud voicing. cv→mix.
    // POOL-LIMITED: master + one part at a time (a 2nd patched CLOUD won't get a bus).
    [MOD_GRAINS]= { "CLOUD", CLR_LIGHT_PEACH, 5, 6, 2, {{2,false,30,60,"cv"},{3,false,4,16,"in"}},
                    3, {{"size",20,400,120,12,30,FMT_INT},{"dens",4,60,16,30,30,FMT_INT},{"mix",0,1,0.5f,48,30,FMT_F1}} },
    // TIDES — a morphing LFO/function generator (Mutable Tides). freq=rate, slope=peak position
    // (ramp-down → triangle → ramp-up), shape=curve (exp→lin→log). 'clk' patched = a one-shot
    // (shaped AD env per trigger); unpatched = free-running LFO. cv out + an eoc gate at cycle end.
    [MOD_TIDES] = { "TIDES", CLR_MEDIUM_GREEN, 5, 6, 3, {{0,false,10,60,"clk"},{2,true,30,60,"cv"},{0,true,50,60,"eoc"}},
                    3, {{"freq",0.05f,10,1,12,26,FMT_F1},{"slope",0,1,0.5f,30,26,FMT_F1},{"shape",0,1,0.5f,48,26,FMT_F1}} },
};
// knob-index names for the multi-knob sound modules — MUST stay in the same order as the
// knob arrays in TYPES[] above. Use these instead of raw numbers in eval/presets: a
// reordered or inserted knob then fails loudly at the compiler instead of silently
// cross-wiring (the fenv/flt/penv mixup of 2026-06-04).
enum { CK_BPM, CK_SWG };                                            // MOD_CLOCK knobs
enum { VK_CUT, VK_RES, VK_PW, VK_WAV, VK_FLT, VK_FENV, VK_PENV, VK_DENV };   // MOD_VOICE knobs
enum { BK_RATE, BK_DEPTH, BK_DEST };                                // MOD_VIBRATO knobs
enum { MK_ENG, MK_HARM, MK_TIMB, MK_MORPH, MK_DRV, MK_EKO, MK_TUN };  // MOD_MACRO knobs
enum { MX_A, MX_B, MX_OFF };                                        // MOD_MIX knobs
enum { AK_ATK, AK_DEC, AK_SUS, AK_REL };                            // MOD_ADSR knobs
enum { RVK_SIZE, RVK_MIX };                                         // MOD_RVB knobs
enum { ECK_TIME, ECK_FB, ECK_MIX };                                 // MOD_ECHO knobs
enum { DRK_AMT, DRK_MODE };                                         // MOD_DRIVE knobs
enum { CRK_BITS, CRK_MIX };                                         // MOD_CRUSH knobs
enum { SATK_AMT, SATK_MODE, SATK_MIX };                            // MOD_SAT knobs
enum { WHK_RATE, WHK_RES, WHK_MIX };                               // MOD_WAH knobs
enum { VWK_VOW, VWK_Q, VWK_MIX };                                  // MOD_VOWEL knobs
enum { EQK_LO, EQK_MID, EQK_HI };                                  // MOD_EQ knobs
enum { FLK_MODE, FLK_CUT, FLK_RES };                               // MOD_FILT knobs
enum { GRK_SIZE, GRK_DENS, GRK_MIX };                              // MOD_GRAINS knobs
enum { TDK_FREQ, TDK_SLOPE, TDK_SHAPE };                           // MOD_TIDES knobs

int tw(int type) { return TYPES[type].cw * CELL; }   // module pixel width/height
int th(int type) { return TYPES[type].ch * CELL; }

// one-screen help per module kind (click the module's ? to show it)
const char *HELP[NTYPE][3] = {
    [MOD_CLOCK]  = { "Master clock. bpm sets tempo; swg drags", "every 2nd step (swing). Gate outs: /1", "every step, /2 half, /4 quarter." },
    [MOD_LFO]    = { "Low-freq oscillator: a slow 0..1 wave.", "rate=speed, shp=shape (sin/sqr/tri/saw/", "rmp/opt). Patch cv out -> any cv in. (S&H=MOD_SH.)" },
    [MOD_SH]     = { "Sample & Hold. On each clk pulse it", "grabs the cv at 'in' and holds it until", "the next clk -> a stepped, random-ish cv." },
    [MOD_QUANT]  = { "Quantizer. Snaps any cv to the nearest", "note of a scale (scl/root) so it's always", "in key. cv in -> pitch out." },
    [MOD_VOICE]  = { "Voice. g=gate p=pitch; f/r/w/a CV =", "cutoff/res/pw/amp. Patch an ENV into 'a' for", "a percussive VCA. fenv/penv/denv = flt/pitch/PWM punch." },
    [MOD_EUCLID] = { "Euclidean rhythm: h hits spread evenly", "over s steps. Advances on clk, fires a", "gate on each hit. Patch 'o' into DRUM." },
    [MOD_ENV]    = { "AD envelope. A gate makes a 0->1->0 cv", "ramp (atk up, dec down). Patch cv into a", "filter for a pluck on every note." },
    [MOD_DRUM]   = { "Three drum voices. A gate at k/s/h", "fires kick / snare / hat. Patch gates", "(from EUCLID or CLOCK) into them." },
    [MOD_SLEW]   = { "Smooths a cv toward its input over 'ms'.", "Feed it stepped pitch -> glide / lag.", "" },
    [MOD_ATTN]   = { "Attenuator: scales a cv by amt (0..1).", "Sets how much a modulator affects its", "target -- i.e. the depth." },
    [MOD_LOGIC]  = { "Combines two gates: AND / OR / XOR (mod).", "AND two EUCLIDs for a pattern neither", "has alone -- emergent rhythm." },
    [MOD_SCOPE]  = { "Oscilloscope. Patch a cv into 'in' to see", "it drawn as a moving trace -- a probe for", "watching what a signal is actually doing." },
    [MOD_KEYS]   = { "Play it! Click the keys or use computer", "keys A S D F G H J. Outputs gate (g) +", "pitch (p) -- patch them into a VOICE." },
    [MOD_TURING] = { "Turing Machine: a looping random melody.", "rnd=0 locks the loop, rnd=1 = chaos, in", "between it evolves. cv out -> QUANT -> VOICE." },
    [MOD_GRIDS]  = { "Drum-pattern generator. map morphs the", "groove, fill sets density. k/s/h gate outs", "-> patch into a DRUM. Clock it from CLOCK." },
    [MOD_MARBLES]= { "Shaped randomness. dens = how often the", "random gate (g) fires; sprd = how wide the", "random cv swings. Clock it; cv -> QUANT." },
    [MOD_MATHS]  = { "Function generator (Make Noise Maths).", "rise/fall shape a cv ramp: 't' triggers a", "one-shot; cyc>0 loops it (an LFO). eoc pulses at end." },
    [MOD_SEQ]    = { "8-step CV sequencer. Knobs set step values", "0-1 (patch cv→QUANT for pitched melody).", "Advances on clk, fires gate on each step." },
    [MOD_VIBRATO]= { "Audio-rate LFO via note_lfo(). rate=Hz, dpt=depth,", "dst=pit/cut/pw/vol + har/tmb/mor (MACRO macros).", "Patch 'out'->VOICE/MACRO 'vb'. 'g' off=always on." },
    [MOD_CHANCE] = { "Gate filter: lets incoming gates through", "with prob chance (0=never 1=always).", "Thins patterns without changing the rhythm." },
    [MOD_MACRO]  = { "Modeled voice, 14 engines (eng): plk/mlt/fm/org/ep/", "pd/mem/reed/pipe/vox/gtr/pno/bow/brs. har/tmb/mor shape", "each (+CV inlets); drv/eko/tun=grit/echo/detune." },
    [MOD_XPOSE]  = { "Octave shifter. Patch a pitch line through", "it (QUANT/KEYS -> VOICE) and oct moves it", "by -2..+2 whole octaves. Basses live here." },
    [MOD_MIX]    = { "CV mixer: out = a*ka + b*kb + off. Knobs go", "-1..+1 (attenuvert: negative = inverted).", "Sum an LFO + ENV into one filter cutoff." },
    [MOD_CMP]    = { "Comparator: gate high while cv in > thr.", "An LFO through it becomes a clock -- thr", "sets the pulse width. CV -> rhythm, no CLOCK." },
    [MOD_DIV]    = { "Clock divider: passes every Nth gate.", "CLOCK only speaks /1 /2 /4 -- DIV adds /3,", "/8, /16... two DIVs = instant polymeter." },
    [MOD_ADSR]   = { "Envelope with SUSTAIN: gate up -> attack,", "decay, then HOLD at sus; gate down ->", "release. Into VOICE 'a' = pads that breathe." },
    [MOD_RVB]    = { "Reverb. size=room, mix=wet. Patch a source's", "pink ~ (audio) into 'in' to verb just THAT part;", "unpatched = the whole mix. cv adds to mix." },
    [MOD_ECHO]   = { "Delay. time=gap, fb=echoes, mix=wet. Patch a", "source's ~ into 'in' = delay just that part;", "unpatched = whole mix. (cv adds to mix.)" },
    [MOD_DRIVE]  = { "Overdrive (post-filter acid scream). amt=grit,", "mode=sft/hrd/fld/asy. Patch a source ~ into 'in'", "= drive that part; unpatched = all synth voices." },
    [MOD_CRUSH]  = { "Bitcrush lo-fi grunge. bits=res, mix=blend.", "Patch a source's ~ into 'in' = crush just that", "part; unpatched = the whole mix. cv adds to mix." },
    [MOD_SAT]    = { "Mix-bus SATURATION: drives the WHOLE mix", "(drums + MACRO too, unlike DRIVE). amt=grit,", "mode=sft/hrd/fld/asy, mix=dry/wet. cv->amt." },
    [MOD_WAH]    = { "Auto-wah: an LFO rocks a resonant band", "(rhythmic wah-wah). rate=Hz, res=quack, mix=wet.", "Patch a source ~ into 'in' = wah just that part." },
    [MOD_VOWEL]  = { "Vowel/formant filter: makes it 'talk'. vow", "morphs a->e->i->o->u, q=sharpness, mix=wet.", "cv->vow (LFO it = wow-wee-wow). ~ in = per part." },
    [MOD_EQ]     = { "3-band EQ (tone). lo/mid/hi = boost or cut", "in dB (the only BOOST in the rack). cv->hi", "(brightness). Patch a ~ in = EQ just that part." },
    [MOD_FILT]   = { "DJ filter: sweep the WHOLE mix. mode=lp/hp/", "bp/nt, cut=cutoff, res=peak. Patch an LFO/ENV", "into cv to SWEEP the cutoff (the build/drop)." },
    [MOD_GRAINS] = { "Granular CLOUD: sprays grains of the recent", "past. size=grain len, dens=grains/sec, mix=wet.", "~ in = cloud one part (pool: master + 1 part)." },
    [MOD_TIDES]  = { "Tidal mod: a morphing LFO. freq=rate, slope=", "ramp<->saw shape, shape=exp/lin/log curve. Patch", "clk = one-shot env per trigger; eoc fires at end." },
};

// ── module instances + cables ──
typedef struct { int type, x, y; float param[8], state[24], jackval[8]; } Module;   // param[]/jackval[] sized to the 8-jack/8-knob registry max — jackval MUST cover every jack index (an out jack at index 4+ used to silently corrupt the neighbouring Module when it was [4])
#define MAX_MOD 24
Module mod[MAX_MOD];
int    nmod = 0;

typedef struct { int sm, sj, dm, dj; } Cable;
#define MAXCABLE 48
Cable cable[MAXCABLE];
int   ncable = 0;

// global transport + ui state
int   g_step = 0, g_newstep = 0, last_step = -1, tick_flash = 99;
int   held_knob = 0, drag_y = 0, drag_x = 0, drag_jack = -1, msg_flash = 0, dbg_midi = 60;
const char *msg = "";

// ── canvas (pan/zoom) + palette state ──
#define SIDEBAR_W 38
float cam_x = -32, cam_y = -2, zoom = 1.0f;      // pannable/zoomable view of the stage
// zoom snaps to these crisp stops — pixel art only looks clean at integer scales (and a
// uniform 0.5), so the wheel steps between them instead of landing on a muddy fraction.
const float ZOOMS[] = { 0.5f, 1.0f, 2.0f, 3.0f };
#define NZOOM 4
int   wmx = 0, wmy = 0;                           // mouse in world space (valid while the stage camera is active)
int   panning = 0, pan_px = 0, pan_py = 0, palette_drag = -1, help_type = -1;
int   drag_mod = -1, grab_dx = 0, grab_dy = 0;    // module being dragged around the canvas
int   palette_scroll = 0, preset_open = 0;         // sidebar scroll offset (px); presets dropdown open?
int   palette_pend = -1, pend_my = 0, pal_scrolling = 0;   // deferred palette pick-up: press → drag right = place, drag up/down = scroll (touch has no wheel)
float pinch_acc = 1.0f;                            // accumulated two-finger pinch factor (touch zoom)

int  spawn(int type, int x, int y) {
    if (nmod >= MAX_MOD) return MAX_MOD - 1;   // rack full — callers should check, but never write past mod[]
    Module *m = &mod[nmod];
    *m = (Module){ type, snap12(x), snap12(y), {0}, {0}, {0} };   // snap to the cell grid
    for (int k = 0; k < TYPES[type].nknob; k++) m->param[k] = TYPES[type].knob[k].def;
    return nmod++;
}
int  jackpos_x(int mi, int j) { return mod[mi].x + TYPES[mod[mi].type].jack[j].dx; }
int  jackpos_y(int mi, int j) { return mod[mi].y + TYPES[mod[mi].type].jack[j].dy; }
int  cable_into(int dm, int dj) { for (int c = 0; c < ncable; c++) if (cable[c].dm == dm && cable[c].dj == dj) return c; return -1; }
void remove_cable(int i) { for (int k = i; k < ncable - 1; k++) cable[k] = cable[k + 1]; ncable--; }
void add_cable(int sm, int sj, int dm, int dj) {
    int e = cable_into(dm, dj); if (e >= 0) remove_cable(e);
    if (ncable < MAXCABLE) cable[ncable++] = (Cable){ sm, sj, dm, dj };
}
float read_in(int mi, int j) { int c = cable_into(mi, j); return c >= 0 ? mod[cable[c].sm].jackval[cable[c].sj] : 0.0f; }

void wire_default(void) {
    ncable = 0;
    add_cable(0, 0, 2, 1); add_cable(0, 0, 4, 0); add_cable(0, 0, 5, 0);   // CLOCK/1 → S&H clk, VOICE g, EUCLID clk
    add_cable(1, 0, 2, 0); add_cable(2, 2, 3, 0); add_cable(3, 1, 4, 1);   // LFO → S&H in, S&H → QUANT, QUANT → VOICE p
    add_cable(1, 0, 4, 2);                                                  // LFO → VOICE f
    add_cable(5, 1, 7, 0); add_cable(0, 0, 7, 2); add_cable(0, 2, 7, 1);   // EUCLID → DRUM k, CLOCK/1 → hat, CLOCK/4 → snare
    add_cable(0, 0, 6, 0);                                                  // CLOCK/1 → ENV g
}

// ── preset patches (the top "PRESETS" dropdown) ──
void preset_empty(void) {        // blank canvas
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
}
void preset_generative(void) {   // the default: in-key melody over a euclidean beat
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    for (int i = 0; i < 8; i++) spawn(i, bayx(i), bayy(i));
    wire_default();
    int rv = spawn(MOD_RVB, bayx(8), bayy(8));               // a touch of space so the default isn't bone-dry
    mod[rv].param[RVK_SIZE] = 0.55f; mod[rv].param[RVK_MIX] = 0.24f;
}
void preset_acid(void) {         // euclid-gated squelch bass, slewed pitch + LFO wah — XPOSE -1 puts it in the bass register
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int lf = spawn(MOD_LFO, bayx(2), bayy(2)), sh = spawn(MOD_SH, bayx(3), bayy(3));
    int sl = spawn(MOD_SLEW, bayx(4), bayy(4)), qt = spawn(MOD_QUANT, bayx(5), bayy(5)), vo = spawn(MOD_VOICE, bayx(6), bayy(6));
    int xp = spawn(MOD_XPOSE, bayx(7), bayy(7));
    mod[vo].param[VK_CUT] = 420; mod[vo].param[VK_RES] = 11; mod[vo].param[VK_WAV] = 1;   // low cutoff, squelchy res, square wave
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[xp].param[0] = -1;                                                  // down an octave — where acid bass lives
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0);
    add_cable(ck, 0, sh, 1); add_cable(lf, 0, sh, 0);
    add_cable(sh, 2, sl, 0); add_cable(sl, 1, qt, 0);
    add_cable(qt, 1, xp, 0); add_cable(xp, 1, vo, 1);                       // QUANT → XPOSE → VOICE pitch
    add_cable(lf, 0, vo, 2);
}
void preset_beats(void) {        // two euclidean rhythms → a drum kit
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), e1 = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int e2 = spawn(MOD_EUCLID, bayx(2), bayy(2)), dr = spawn(MOD_DRUM, bayx(3), bayy(3));
    mod[e1].param[0] = 4; mod[e1].param[1] = 8; mod[e2].param[0] = 3; mod[e2].param[1] = 8;
    add_cable(ck, 0, e1, 0); add_cable(ck, 0, e2, 0);
    add_cable(e1, 1, dr, 0); add_cable(e2, 1, dr, 1); add_cable(ck, 0, dr, 2);
}
void preset_keys(void) {         // play KEYS into a voice with an envelope-plucked filter, over a beat
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int kb = spawn(MOD_KEYS, bayx(0), bayy(0)), en = spawn(MOD_ENV, bayx(1), bayy(1)), vo = spawn(MOD_VOICE, bayx(2), bayy(2));
    int ck = spawn(MOD_CLOCK, bayx(4), bayy(4)), eu = spawn(MOD_EUCLID, bayx(5), bayy(5)), dr = spawn(MOD_DRUM, bayx(6), bayy(6));
    mod[vo].param[VK_WAV] = 2;   // triangle — mellow, rounded; a warm played pluck distinct from the buzzier presets
    add_cable(kb, 0, vo, 0); add_cable(kb, 1, vo, 1); add_cable(kb, 0, en, 0); add_cable(en, 1, vo, 2);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_pwmpad(void) {       // square voice with a slow LFO sweeping its pulse width (PWM)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), lf = spawn(MOD_LFO, bayx(1), bayy(1));
    int sh = spawn(MOD_SH, bayx(2), bayy(2)), qt = spawn(MOD_QUANT, bayx(3), bayy(3)), vo = spawn(MOD_VOICE, bayx(4), bayy(4));
    mod[ck].param[0] = 90; mod[lf].param[0] = 0.25f; mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 1100; mod[vo].param[VK_RES] = 3; mod[vo].param[VK_PW] = 0.5f; mod[vo].param[VK_WAV] = 1;   // cut/res/pw + wav=square
    add_cable(ck, 0, sh, 1); add_cable(ck, 0, vo, 0);
    add_cable(lf, 0, sh, 0); add_cable(sh, 2, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(lf, 0, vo, 4);     // LFO → 'w' = the PWM shimmer
}
void preset_turing(void) {       // looping random melody (Turing) + a euclidean beat
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), tm = spawn(MOD_TURING, bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT, bayx(2), bayy(2)), vo = spawn(MOD_VOICE, bayx(3), bayy(3));
    int eu = spawn(MOD_EUCLID, bayx(4), bayy(4)), dr = spawn(MOD_DRUM, bayx(5), bayy(5));
    mod[tm].param[0] = 0.3f;   // mostly-locked, slowly evolving loop
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1); add_cable(ck, 0, vo, 0);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_grids(void) {        // Grids drum-pattern generator → a full kit
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), gr = spawn(MOD_GRIDS, bayx(1), bayy(1)), dr = spawn(MOD_DRUM, bayx(2), bayy(2));
    add_cable(ck, 0, gr, 0);
    add_cable(gr, 1, dr, 0); add_cable(gr, 2, dr, 1); add_cable(gr, 3, dr, 2);
}
void preset_marbles(void) {      // shaped randomness drives both rhythm (gate) and melody (cv)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), mb = spawn(MOD_MARBLES, bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT, bayx(2), bayy(2)), vo = spawn(MOD_VOICE, bayx(3), bayy(3));
    add_cable(ck, 0, mb, 0); add_cable(mb, 1, vo, 0); add_cable(mb, 2, qt, 0); add_cable(qt, 1, vo, 1);
}
void preset_maths(void) {        // MATHS cycling as a slow asymmetric filter sweep over a generative line
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), lf = spawn(MOD_LFO, bayx(1), bayy(1)), sh = spawn(MOD_SH, bayx(2), bayy(2)), qt = spawn(MOD_QUANT, bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE, bayx(4), bayy(4)), ma = spawn(MOD_MATHS, bayx(5), bayy(5)), eu = spawn(MOD_EUCLID, bayx(6), bayy(6)), dr = spawn(MOD_DRUM, bayx(7), bayy(7));
    mod[ma].param[0] = 0.4f; mod[ma].param[1] = 1.2f; mod[ma].param[2] = 1;   // slow rise, slower fall, cycling
    mod[vo].param[VK_CUT] = 500;                                                   // low cutoff so the sweep opens it
    add_cable(ck, 0, sh, 1); add_cable(ck, 0, vo, 0); add_cable(ck, 0, eu, 0);
    add_cable(lf, 0, sh, 0); add_cable(sh, 2, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ma, 1, vo, 2);   // MATHS cv → VOICE filter = the slow sweep
    add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_envpluck(void) {     // VOICE's onboard FILTER env = a punchy pluck bass — no separate ENV module needed
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int tm = spawn(MOD_TURING, bayx(2), bayy(2)), qt = spawn(MOD_QUANT, bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE, bayx(4), bayy(4)), e2 = spawn(MOD_EUCLID, bayx(5), bayy(5)), dr = spawn(MOD_DRUM, bayx(6), bayy(6));
    mod[vo].param[VK_CUT] = 280; mod[vo].param[VK_RES] = 10; mod[vo].param[VK_WAV] = 0;   // low cut, squelchy res, saw
    mod[vo].param[VK_FENV] = 2400;                                              // FILTER ENV: snaps wide open on each note
    mod[qt].param[0] = SCALE_PENTA_MIN; mod[tm].param[0] = 0.35f;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0);                     // euclid gates the bass
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);  // turing melody → quant → pitch
    mod[e2].param[0] = 4; mod[e2].param[1] = 8;
    add_cable(ck, 0, e2, 0); add_cable(e2, 1, dr, 0); add_cable(ck, 0, dr, 2);  // a beat under it
}
void preset_zaplead(void) {      // VOICE's onboard PITCH env = a zappy lead (each note swoops up into pitch)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), tm = spawn(MOD_TURING, bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT, bayx(2), bayy(2)), vo = spawn(MOD_VOICE, bayx(3), bayy(3));
    int eu = spawn(MOD_EUCLID, bayx(4), bayy(4)), dr = spawn(MOD_DRUM, bayx(5), bayy(5));
    mod[vo].param[VK_CUT] = 1500; mod[vo].param[VK_WAV] = 1;   // bright, square
    mod[vo].param[VK_FENV] = 700;                          // a touch of filter pluck
    mod[vo].param[VK_PENV] = 12;                           // PITCH ENV: +12 st — each note zaps up an octave into the note
    mod[tm].param[0] = 0.4f;
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1); add_cable(ck, 0, vo, 0);
    mod[eu].param[0] = 4; mod[eu].param[1] = 8;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_punch(void) {        // ENV -> VCA (amp) + a big pitch env = the kick/zap the VOICE couldn't make before
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int en = spawn(MOD_ENV, bayx(2), bayy(2)), vo = spawn(MOD_VOICE, bayx(3), bayy(3));
    int e2 = spawn(MOD_EUCLID, bayx(4), bayy(4)), dr = spawn(MOD_DRUM, bayx(5), bayy(5));
    mod[ck].param[0] = 120;
    mod[en].param[0] = 0.005f; mod[en].param[1] = 0.13f;             // fast attack, short decay = a percussive amp
    mod[vo].param[VK_CUT] = 480; mod[vo].param[VK_RES] = 3; mod[vo].param[VK_WAV] = 3;   // sine, low-ish cut
    mod[vo].param[VK_PENV] = 36;                                           // PITCH ENV: +36 st = the kick "donk"
    mod[eu].param[0] = 4; mod[eu].param[1] = 8;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0); add_cable(eu, 1, en, 0);   // one gate fires VOICE + ENV together
    add_cable(en, 1, vo, 5);                                                      // ENV -> 'a' (VCA): the percussive punch
    mod[e2].param[0] = 3; mod[e2].param[1] = 8;
    add_cable(ck, 0, e2, 0); add_cable(e2, 1, dr, 2); add_cable(ck, 0, dr, 1);   // hats + snare on top
}
void preset_glide(void) {        // live pitch tracking: sparse gates + LFO walks scale while note is held
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), lf = spawn(MOD_LFO, bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT, bayx(2), bayy(2)), eu = spawn(MOD_EUCLID, bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE, bayx(4), bayy(4)), en = spawn(MOD_ENV, bayx(5), bayy(5));
    int dr = spawn(MOD_DRUM, bayx(6), bayy(6));
    mod[ck].param[0] = 88;
    mod[lf].param[0] = 0.35f;                               // slow LFO drifts through scale degrees
    mod[eu].param[0] = 2; mod[eu].param[1] = 16;            // 2/16 = sparse; long held notes
    mod[qt].param[0] = SCALE_MAJOR;
    mod[vo].param[VK_CUT] = 900; mod[vo].param[VK_RES] = 4; mod[vo].param[VK_WAV] = 2;   // tri wave, open filter
    mod[vo].param[VK_FENV] = 400;                                  // light filter blip on attack
    mod[en].param[1] = 0.3f;
    add_cable(lf, 0, qt, 0); add_cable(qt, 1, vo, 1);       // LFO → QUANT → pitch (walks while held)
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0);
    add_cable(eu, 1, en, 0); add_cable(en, 1, vo, 2);        // ENV → filter CV for each attack
    add_cable(ck, 0, dr, 1); add_cable(ck, 0, dr, 2);
}
void preset_bpacid(void) {       // band-pass filter: Turing loop + LFO cutoff sweep = nasal wah
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), tm = spawn(MOD_TURING, bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT, bayx(2), bayy(2)), eu = spawn(MOD_EUCLID, bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE, bayx(4), bayy(4)), lf = spawn(MOD_LFO, bayx(5), bayy(5));
    int dr = spawn(MOD_DRUM, bayx(6), bayy(6));
    mod[ck].param[0] = 124;
    mod[tm].param[0] = 0.2f; mod[tm].param[1] = 8;
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[eu].param[0] = 5; mod[eu].param[1] = 8;
    mod[lf].param[0] = 0.7f;
    mod[vo].param[VK_CUT] = 500; mod[vo].param[VK_RES] = 11; mod[vo].param[VK_WAV] = 0;  // saw, high Q
    mod[vo].param[VK_FLT] = 2;                                    // BAND-PASS
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0);
    add_cable(lf, 0, vo, 2);                                 // LFO sweeps BP cutoff
    add_cable(ck, 0, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_notchphaser(void) {  // notch filter: MATHS cycling slowly sweeps notch freq = phaser
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), lf = spawn(MOD_LFO, bayx(1), bayy(1));
    int sh = spawn(MOD_SH, bayx(2), bayy(2)), qt = spawn(MOD_QUANT, bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE, bayx(4), bayy(4)), ma = spawn(MOD_MATHS, bayx(5), bayy(5));
    int dr = spawn(MOD_DRUM, bayx(6), bayy(6));
    mod[ck].param[0] = 95;
    mod[lf].param[0] = 0.5f;
    mod[ma].param[0] = 0.4f; mod[ma].param[1] = 1.8f; mod[ma].param[2] = 1; // cycling ramp = slow notch sweep
    mod[qt].param[0] = SCALE_PENTA;
    mod[vo].param[VK_CUT] = 800; mod[vo].param[VK_RES] = 13; mod[vo].param[VK_WAV] = 1;   // sqr, sharp notch Q
    mod[vo].param[VK_FLT] = 3;                                    // NOTCH
    add_cable(ck, 0, sh, 1); add_cable(lf, 0, sh, 0);
    add_cable(sh, 2, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ck, 0, vo, 0);
    add_cable(ma, 1, vo, 2);                                 // MATHS → notch freq sweep
    add_cable(ck, 0, dr, 0); add_cable(ck, 0, dr, 2);
}

void preset_seq(void) {          // step sequencer: C pentatonic minor phrase → QUANT → VOICE
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), sq = spawn(MOD_SEQ,    bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT,  bayx(2), bayy(2)), vo = spawn(MOD_VOICE,  bayx(3), bayy(3));
    int eu = spawn(MOD_EUCLID, bayx(4), bayy(4)), dr = spawn(MOD_DRUM,   bayx(5), bayy(5));
    mod[ck].param[0] = 105;
    mod[sq].param[0]=0; mod[sq].param[1]=0.125f; mod[sq].param[2]=0.375f; mod[sq].param[3]=0.5f;
    mod[sq].param[4]=0.375f; mod[sq].param[5]=0.25f; mod[sq].param[6]=0.125f; mod[sq].param[7]=0;
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 650; mod[vo].param[VK_RES] = 6; mod[vo].param[VK_WAV] = 0; mod[vo].param[VK_FENV] = 900;
    mod[eu].param[0] = 3; mod[eu].param[1] = 8;
    add_cable(ck, 0, sq, 0); add_cable(sq, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(sq, 2, vo, 0);                             // SEQ gate fires every step
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_vibe(void) {         // audio-rate vibrato: sparse gates hold notes long, VIBE always on
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,   bayx(0), bayy(0)), eu = spawn(MOD_EUCLID,  bayx(1), bayy(1));
    int tm = spawn(MOD_TURING,  bayx(2), bayy(2)), qt = spawn(MOD_QUANT,   bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE,   bayx(4), bayy(4)), vb = spawn(MOD_VIBRATO, bayx(5), bayy(5));
    int dr = spawn(MOD_DRUM,    bayx(6), bayy(6));
    mod[ck].param[0] = 78;
    mod[eu].param[0] = 2; mod[eu].param[1] = 16;        // very sparse → long held notes
    mod[tm].param[0] = 0.2f;
    mod[qt].param[0] = SCALE_MAJOR;
    mod[vo].param[VK_CUT] = 1200; mod[vo].param[VK_WAV] = 2;      // tri — smooth, vibrato clearly audible
    mod[vb].param[0] = 5.5f; mod[vb].param[1] = 0.6f;  // 5.5 Hz, moderate depth, dst=pit
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0);
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(vb, 1, vo, 6);                             // VIBE out → VOICE vb (explicit patch)
    add_cable(ck, 0, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_chance(void) {       // probabilistic gates: dense euclid filtered at 60% by CHANCE
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int ch = spawn(MOD_CHANCE, bayx(2), bayy(2)), tm = spawn(MOD_TURING, bayx(3), bayy(3));
    int qt = spawn(MOD_QUANT,  bayx(4), bayy(4)), vo = spawn(MOD_VOICE,  bayx(5), bayy(5));
    int dr = spawn(MOD_DRUM,   bayx(6), bayy(6));
    mod[ck].param[0] = 118;
    mod[eu].param[0] = 6; mod[eu].param[1] = 8;         // dense 6/8
    mod[ch].param[0] = 0.6f;                             // 60% pass rate
    mod[tm].param[0] = 0.3f;
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 700; mod[vo].param[VK_RES] = 7; mod[vo].param[VK_WAV] = 1; mod[vo].param[VK_FENV] = 900;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, ch, 0); add_cable(ch, 1, vo, 0);
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ck, 0, dr, 0); add_cable(ck, 0, dr, 2);
}

void preset_macro(void) {        // Plaits-style MACRO voice on the PD (Casio CZ) engine: a VIBE
                                 // sweeps the morph macro at AUDIO RATE — the DCW "wowww" pulsing
                                 // (showcases the macros-as-LFO-destinations feature, dst=mor)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,   bayx(0), bayy(0)), tm = spawn(MOD_TURING,  bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT,   bayx(2), bayy(2)), mc = spawn(MOD_MACRO,   bayx(4), bayy(4));
    int vb = spawn(MOD_VIBRATO, bayx(5), bayy(5)), eu = spawn(MOD_EUCLID,  bayx(6), bayy(6));
    int dr = spawn(MOD_DRUM,    bayx(3), bayy(3));   // MACRO front-and-centre (bay 4); drums take the edge bay
    mod[ck].param[0] = 96;
    mod[tm].param[0] = 0.25f;                            // mostly-locked melody loop
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[mc].param[MK_ENG] = 5;                           // PD (Casio CZ) engine
    mod[mc].param[MK_HARM] = 0.94f; mod[mc].param[MK_TIMB] = 0.5f; mod[mc].param[MK_MORPH] = 0.3f;   // resonant wavetype, mid distortion, some DCW
    mod[vb].param[BK_RATE] = 3.0f; mod[vb].param[BK_DEPTH] = 0.5f; mod[vb].param[BK_DEST] = 6;        // dst=mor: LFO the DCW sweep (the new macro dest)
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, mc, 1);
    add_cable(ck, 1, mc, 0);                             // /2 gates — give each strike room to ring
    add_cable(vb, 1, mc, 5);                             // VIBE out → MACRO vb: audio-rate LFO on morph (the new feature)
    mod[eu].param[0] = 3; mod[eu].param[1] = 8;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}

void preset_mix(void) {          // THE classic synth patch: slow LFO wah + per-note ENV pluck SUMMED into one cutoff via MIX
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), tm = spawn(MOD_TURING, bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT,  bayx(2), bayy(2)), lf = spawn(MOD_LFO,    bayx(3), bayy(3));
    int en = spawn(MOD_ENV,    bayx(4), bayy(4)), mx = spawn(MOD_MIX,    bayx(5), bayy(5));
    int vo = spawn(MOD_VOICE,  bayx(6), bayy(6)), dr = spawn(MOD_DRUM,   bayx(7), bayy(7));
    mod[ck].param[0] = 110;
    mod[tm].param[0] = 0.3f;
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[lf].param[0] = 0.25f;                            // the slow wah half
    mod[en].param[1] = 0.18f;                            // the per-note pluck half
    mod[mx].param[MX_A] = 0.5f; mod[mx].param[MX_B] = 1.0f;   // wah underneath, pluck on top
    mod[vo].param[VK_CUT] = 320; mod[vo].param[VK_RES] = 9;   // low base cutoff so the summed cv does the talking
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ck, 0, vo, 0); add_cable(ck, 0, en, 0);
    add_cable(lf, 0, mx, 0); add_cable(en, 1, mx, 1); add_cable(mx, 2, vo, 2);   // LFO + ENV → MIX → cutoff
    add_cable(ck, 2, dr, 0); add_cable(ck, 0, dr, 2);
}

void preset_clockless(void) {    // NO CLOCK: two free-running LFOs through two CMPs make the whole groove
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int l1 = spawn(MOD_LFO, bayx(0), bayy(0)), l2 = spawn(MOD_LFO, bayx(1), bayy(1));
    int c1 = spawn(MOD_CMP, bayx(2), bayy(2)), c2 = spawn(MOD_CMP, bayx(2) + 48, bayy(2));   // two slim CMPs share a bay
    int qt = spawn(MOD_QUANT, bayx(4), bayy(4)), vo = spawn(MOD_VOICE, bayx(5), bayy(5));
    int dr = spawn(MOD_DRUM,  bayx(6), bayy(6));
    mod[l1].param[0] = 0.6f; mod[l2].param[0] = 1.7f;    // incommensurate rates — the rhythm never quite repeats
    mod[c1].param[0] = 0.6f;                             // thr = pulse width: notes get medium gates
    mod[c2].param[0] = 0.85f;                            // hats only on the LFO's crest — tight ticks
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 700; mod[vo].param[VK_RES] = 6; mod[vo].param[VK_FENV] = 800;
    add_cable(l1, 0, c1, 0); add_cable(c1, 1, vo, 0); add_cable(c1, 1, dr, 0);   // LFO1 rhythm → note + kick
    add_cable(l2, 0, c2, 0); add_cable(c2, 1, dr, 2);                             // LFO2 rhythm → hats
    add_cable(l2, 0, qt, 0); add_cable(qt, 1, vo, 1);                             // pitch rides the OTHER lfo — cross-rhythm melody
}

void preset_polymeter(void) {    // DIV 4-against-3: kick every 4 steps, snare+bass every 3 — the accents drift and realign
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0));
    int d1 = spawn(MOD_DIV, bayx(1), bayy(1)), d2 = spawn(MOD_DIV, bayx(1) + 48, bayy(1));   // two slim DIVs share a bay
    int dr = spawn(MOD_DRUM, bayx(2), bayy(2));
    int tm = spawn(MOD_TURING, bayx(4), bayy(4)), qt = spawn(MOD_QUANT, bayx(5), bayy(5)), vo = spawn(MOD_VOICE, bayx(6), bayy(6));
    mod[ck].param[0] = 132;
    mod[d1].param[0] = 4; mod[d2].param[0] = 3;          // the polymeter: 4 against 3
    mod[tm].param[0] = 0.3f; mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 380; mod[vo].param[VK_RES] = 8; mod[vo].param[VK_FENV] = 1200;
    add_cable(ck, 0, d1, 0); add_cable(ck, 0, d2, 0);
    add_cable(ck, 0, dr, 2);                             // hats every step
    add_cable(d1, 1, dr, 0);                             // kick on the 4-cycle
    add_cable(d2, 1, dr, 1);                             // snare on the 3-cycle — drifts against the kick
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(d2, 1, vo, 0);                             // bass rides the 3 too
}
void preset_adsrpad(void) {      // ADSR pad: wide CMP gates + a sustaining VCA — notes swell, hold, and breathe out
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int l1 = spawn(MOD_LFO, bayx(0), bayy(0)), c1 = spawn(MOD_CMP, bayx(1), bayy(1));
    int ad = spawn(MOD_ADSR, bayx(2), bayy(2));
    int l2 = spawn(MOD_LFO, bayx(4), bayy(4)), qt = spawn(MOD_QUANT, bayx(5), bayy(5)), vo = spawn(MOD_VOICE, bayx(6), bayy(6));
    mod[l1].param[0] = 0.22f;                            // slow breath: each cycle is one long note
    mod[c1].param[0] = 0.35f;                            // low threshold = wide gates for the sustain to live in
    mod[ad].param[AK_ATK] = 0.35f; mod[ad].param[AK_DEC] = 0.3f; mod[ad].param[AK_SUS] = 0.65f; mod[ad].param[AK_REL] = 1.0f;
    mod[l2].param[0] = 0.13f;                            // pitch drifts independently of the breathing
    mod[qt].param[0] = SCALE_MAJOR;
    mod[vo].param[VK_CUT] = 1000; mod[vo].param[VK_RES] = 3; mod[vo].param[VK_WAV] = 2;   // tri — soft pad tone
    add_cable(l1, 0, c1, 0); add_cable(c1, 1, vo, 0); add_cable(c1, 1, ad, 0);
    add_cable(ad, 1, vo, 5);                             // ADSR → 'a': the VCA with a real sustain + release
    add_cable(l2, 0, qt, 0); add_cable(qt, 1, vo, 1);    // pitch wanders the scale while the note holds
}

void preset_pdreso(void) {       // PD on a resonant wavetype: a VIBE sweeps TIMBRE = a resonant
                                 // filter sweep with NO filter (+ drive for grit). The CZ recipe,
                                 // and the standout new sound from macros-as-LFO-destinations.
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,   bayx(0), bayy(0)), tm = spawn(MOD_TURING,  bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT,   bayx(2), bayy(2)), mc = spawn(MOD_MACRO,   bayx(4), bayy(4));
    int vb = spawn(MOD_VIBRATO, bayx(5), bayy(5)), dr = spawn(MOD_DRUM,    bayx(6), bayy(6));
    mod[ck].param[0] = 124;
    mod[tm].param[0] = 0.3f;
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[mc].param[MK_ENG] = 5;                           // PD (Casio CZ)
    mod[mc].param[MK_HARM] = 0.94f; mod[mc].param[MK_TIMB] = 0.35f; mod[mc].param[MK_MORPH] = 0.2f;  // reso wavetype, low base distortion
    mod[mc].param[MK_DRV] = 0.4f;                        // drive = grit into the resonance
    mod[vb].param[BK_RATE] = 1.2f; mod[vb].param[BK_DEPTH] = 0.55f; mod[vb].param[BK_DEST] = 5;       // dst=tmb: the resonant sweep
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, mc, 1);
    add_cable(ck, 0, mc, 0);
    add_cable(vb, 1, mc, 5);                             // VIBE out → MACRO vb: audio-rate timbre sweep
    add_cable(ck, 0, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_organ(void) {        // ORGAN engine: harmonics = drawbar registration, a VIBE sweeps
                                 // MORPH = animated chorus/percussion. SAME routing as the PD
                                 // presets, different engine — the engine-agnostic-modulation point.
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,   bayx(0), bayy(0)), tm = spawn(MOD_TURING,  bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT,   bayx(2), bayy(2)), mc = spawn(MOD_MACRO,   bayx(4), bayy(4));
    int vb = spawn(MOD_VIBRATO, bayx(5), bayy(5)), eu = spawn(MOD_EUCLID,  bayx(6), bayy(6));
    int dr = spawn(MOD_DRUM,    bayx(3), bayy(3));
    mod[ck].param[0] = 100;
    mod[tm].param[0] = 0.2f;
    mod[qt].param[0] = SCALE_MAJOR;
    mod[mc].param[MK_ENG] = 3;                           // ORGAN (tonewheel)
    mod[mc].param[MK_HARM] = 0.45f; mod[mc].param[MK_TIMB] = 0.55f; mod[mc].param[MK_MORPH] = 0.5f;  // jazz-ish drawbars, chorus on
    mod[mc].param[MK_EKO] = 0.2f;                        // a little echo-bus space
    mod[vb].param[BK_RATE] = 0.8f; mod[vb].param[BK_DEPTH] = 0.4f; mod[vb].param[BK_DEST] = 6;        // dst=mor: slow chorus swell
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, mc, 1);
    add_cable(ck, 1, mc, 0);                             // /2 gates — organ holds while gated
    add_cable(vb, 1, mc, 5);
    mod[eu].param[0] = 3; mod[eu].param[1] = 8;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}

void preset_spacedub(void) {     // FX showcase: a sparse generative line drenched in delay + reverb,
                                 // an LFO sweeping the echo MIX so the repeats throw in and out (dub)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), tm = spawn(MOD_TURING, bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT,  bayx(2), bayy(2)), vo = spawn(MOD_VOICE,  bayx(3), bayy(3));
    int eu = spawn(MOD_EUCLID, bayx(4), bayy(4)), dr = spawn(MOD_DRUM,   bayx(5), bayy(5));
    int ec = spawn(MOD_ECHO,   bayx(6), bayy(6)), rv = spawn(MOD_RVB,    bayx(7), bayy(7));
    int lf = spawn(MOD_LFO,    bayx(8), bayy(8));
    mod[ck].param[0] = 92;
    mod[tm].param[0] = 0.25f;   mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 950; mod[vo].param[VK_RES] = 5; mod[vo].param[VK_WAV] = 2; mod[vo].param[VK_FENV] = 700;
    mod[eu].param[0] = 3; mod[eu].param[1] = 8;
    mod[ec].param[ECK_TIME] = 375; mod[ec].param[ECK_FB] = 0.5f; mod[ec].param[ECK_MIX] = 0.30f;
    mod[rv].param[RVK_SIZE] = 0.72f; mod[rv].param[RVK_MIX] = 0.42f;
    mod[lf].param[0] = 0.12f;                            // slow swell
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1); add_cable(ck, 0, vo, 0);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
    add_cable(lf, 0, ec, 0);                             // LFO -> ECHO cv: dub throws bloom and fade
}

void preset_satbus(void) {       // FX showcase: a full beat + acid bass run through the mix-bus
                                 // SATURATOR (SAT) — the DRUMS grit up too, what per-voice DRIVE can't do
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int tm = spawn(MOD_TURING, bayx(2), bayy(2)), qt = spawn(MOD_QUANT,  bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE,  bayx(4), bayy(4)), dr = spawn(MOD_DRUM,   bayx(5), bayy(5));
    int sa = spawn(MOD_SAT,    bayx(6), bayy(6));
    mod[ck].param[0] = 124;
    mod[eu].param[0] = 4; mod[eu].param[1] = 8;
    mod[tm].param[0] = 0.3f; mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 500; mod[vo].param[VK_RES] = 8; mod[vo].param[VK_WAV] = 0; mod[vo].param[VK_FENV] = 1000;
    mod[sa].param[SATK_AMT] = 0.5f; mod[sa].param[SATK_MODE] = 3; mod[sa].param[SATK_MIX] = 0.85f;   // asym tube glue on the whole bus
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0);
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ck, 0, dr, 0); add_cable(ck, 2, dr, 2);
}

void preset_splitfx(void) {      // PINK AUDIO CABLES: bitcrush on JUST the drums, reverb on JUST the
                                 // voice — two effects, two parts, patched independently (the routing demo)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int tm = spawn(MOD_TURING, bayx(2), bayy(2)), qt = spawn(MOD_QUANT,  bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE,  bayx(4), bayy(4)), dr = spawn(MOD_DRUM,   bayx(5), bayy(5));
    int rv = spawn(MOD_RVB,    bayx(6), bayy(6)), cr = spawn(MOD_CRUSH,  bayx(7), bayy(7));
    mod[ck].param[0] = 100; mod[eu].param[0] = 3; mod[eu].param[1] = 8;
    mod[tm].param[0] = 0.3f; mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 900; mod[vo].param[VK_RES] = 5; mod[vo].param[VK_WAV] = 2; mod[vo].param[VK_FENV] = 600;
    mod[rv].param[RVK_SIZE] = 0.78f; mod[rv].param[RVK_MIX] = 0.6f;   // big room, as a SEND on the voice only
    mod[cr].param[CRK_BITS] = 5;     mod[cr].param[CRK_MIX] = 0.7f;    // gritty drums only
    // gate/pitch patch (blue/green/yellow)
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1); add_cable(ck, 0, vo, 0);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
    // AUDIO patch (pink): VOICE audio-out (jack 7) → VERB in (jack 1); DRUM audio-out (jack 3) → CRUSH in
    add_cable(vo, 7, rv, 1);
    add_cable(dr, 3, cr, 1);
}

void preset_filterjam(void) {    // the DJ FILTER swept by an LFO over the whole mix (the build/drop)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int tm = spawn(MOD_TURING, bayx(2), bayy(2)), qt = spawn(MOD_QUANT,  bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE,  bayx(4), bayy(4)), dr = spawn(MOD_DRUM,   bayx(5), bayy(5));
    int lf = spawn(MOD_LFO,    bayx(6), bayy(6)), fl = spawn(MOD_FILT,   bayx(7), bayy(7));
    mod[ck].param[0] = 120; mod[eu].param[0] = 4; mod[eu].param[1] = 8;
    mod[tm].param[0] = 0.3f; mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 2000; mod[vo].param[VK_WAV] = 0;   // bright source so the master filter has something to cut
    mod[lf].param[0] = 0.2f;                                   // slow sweep
    mod[fl].param[FLK_MODE] = 0; mod[fl].param[FLK_CUT] = 350; mod[fl].param[FLK_RES] = 0.5f;   // low-pass, resonant
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1); add_cable(ck, 0, vo, 0);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
    add_cable(lf, 0, fl, 0);   // LFO → FILT cv = the cutoff sweep on the whole mix
}
void preset_tides(void) {        // TIDES as a SHAPEABLE filter envelope: euclid triggers a one-shot,
                                 // slope/shape morph the pluck (snappy exp pluck → slow log swell)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int td = spawn(MOD_TIDES,  bayx(2), bayy(2)), tm = spawn(MOD_TURING, bayx(3), bayy(3));
    int qt = spawn(MOD_QUANT,  bayx(4), bayy(4)), vo = spawn(MOD_VOICE,  bayx(5), bayy(5));
    int dr = spawn(MOD_DRUM,   bayx(6), bayy(6));
    mod[ck].param[0] = 110; mod[eu].param[0] = 4; mod[eu].param[1] = 8;
    mod[td].param[TDK_FREQ] = 3.0f; mod[td].param[TDK_SLOPE] = 0.15f; mod[td].param[TDK_SHAPE] = 0.3f;  // fast, snappy rise, exp = a pluck
    mod[tm].param[0] = 0.3f; mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[VK_CUT] = 350; mod[vo].param[VK_RES] = 7; mod[vo].param[VK_WAV] = 0;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0); add_cable(eu, 1, td, 0);   // euclid gates the note + triggers TIDES
    add_cable(td, 1, vo, 2);                                                      // TIDES cv → VOICE filter = the shaped pluck
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ck, 0, dr, 0); add_cable(ck, 2, dr, 2);
}

void preset_electronium(void) {  // Raymond Scott homage — his Electronium was a SELF-COMPOSING machine
                                 // you steer, not play. Here: TURING evolves the melody, MARBLES adds
                                 // shaped randomness, TIDES morphs the filter — never quite repeating —
                                 // all run dirty through SAT (warm DRIVE_ASYM tube saturation = analog).
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,   bayx(0), bayy(0)), tm = spawn(MOD_TURING,  bayx(1), bayy(1));
    int qt = spawn(MOD_QUANT,   bayx(2), bayy(2)), td = spawn(MOD_TIDES,   bayx(3), bayy(3));
    int vo = spawn(MOD_VOICE,   bayx(4), bayy(4)), eu = spawn(MOD_EUCLID,  bayx(5), bayy(5));
    int mb = spawn(MOD_MARBLES, bayx(6), bayy(6)), dr = spawn(MOD_DRUM,    bayx(7), bayy(7));
    int sa = spawn(MOD_SAT,     bayx(8), bayy(8));
    mod[ck].param[CK_BPM] = 104; mod[ck].param[CK_SWG] = 0.10f;          // motoric, a touch of swing (less machine-rigid)
    mod[tm].param[0] = 0.35f; mod[tm].param[1] = 12;                     // rnd 0.35 = slowly mutating loop; 12-step
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[td].param[TDK_FREQ] = 0.13f; mod[td].param[TDK_SLOPE] = 0.5f; mod[td].param[TDK_SHAPE] = 0.5f;   // slow free-run = the morphing filter
    mod[vo].param[VK_CUT] = 480; mod[vo].param[VK_RES] = 8; mod[vo].param[VK_WAV] = 0; mod[vo].param[VK_FENV] = 900;  // resonant saw
    mod[eu].param[0] = 5; mod[eu].param[1] = 8;                          // busy motoric gate
    mod[mb].param[0] = 0.55f; mod[mb].param[1] = 0.6f;                   // dens / spread
    mod[sa].param[SATK_AMT] = 0.45f; mod[sa].param[SATK_MODE] = 3; mod[sa].param[SATK_MIX] = 0.8f;   // ASYM tube glue on the whole mix
    // the melody: TURING → QUANT → VOICE, gated by EUCLID
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0);
    add_cable(td, 1, vo, 2);                                             // TIDES (free-run LFO) → VOICE cutoff = the slow morph
    // shaped randomness drives variation: MARBLES gate → snare accents, cv → resonance wobble
    add_cable(ck, 0, mb, 0); add_cable(mb, 1, dr, 1); add_cable(mb, 2, vo, 3);
    add_cable(ck, 1, dr, 0); add_cable(ck, 0, dr, 2);                    // kick on /2, hat every step
}
void preset_bandito(void) {      // Raymond Scott's drum machine was named "Bandito the Bongo Artist".
                                 // GRIDS is the pattern brain → the kit; a MACRO MALLET plays tuned
                                 // hand-percussion on the snare pattern; a TURING→XPOSE bassline locks
                                 // to the kick. Run dirty through SAT. A little rhythm-bot.
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), gr = spawn(MOD_GRIDS,  bayx(1), bayy(1));
    int dr = spawn(MOD_DRUM,  bayx(2), bayy(2)), tm = spawn(MOD_TURING, bayx(3), bayy(3));
    int qt = spawn(MOD_QUANT, bayx(4), bayy(4)), xp = spawn(MOD_XPOSE,  bayx(5), bayy(5));
    int vo = spawn(MOD_VOICE, bayx(6), bayy(6)), mc = spawn(MOD_MACRO,  bayx(7), bayy(7));
    int sa = spawn(MOD_SAT,   bayx(8), bayy(8));
    mod[ck].param[CK_BPM] = 100; mod[ck].param[CK_SWG] = 0.12f;          // shuffle for a hand-played feel
    mod[gr].param[0] = 0.3f; mod[gr].param[1] = 0.62f;                   // map / fill — a busy-ish kit
    mod[tm].param[0] = 0.3f; mod[tm].param[1] = 8;
    mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[xp].param[0] = -1;                                              // bass an octave down
    mod[vo].param[VK_CUT] = 350; mod[vo].param[VK_RES] = 6; mod[vo].param[VK_WAV] = 0; mod[vo].param[VK_FENV] = 1000;  // punchy pluck bass
    mod[mc].param[MK_ENG] = 6;                                          // MEMBRANE = struck drumhead = REAL bongos/congas
    mod[mc].param[MK_HARM] = 0.5f; mod[mc].param[MK_TIMB] = 0.6f; mod[mc].param[MK_MORPH] = 0.1f; mod[mc].param[MK_DRV] = 0.2f;  // head between tabla/djembe, edge slap, ~flat
    mod[sa].param[SATK_AMT] = 0.4f; mod[sa].param[SATK_MODE] = 3; mod[sa].param[SATK_MIX] = 0.8f;   // dirty analog glue
    // GRIDS pattern → the kit
    add_cable(ck, 0, gr, 0); add_cable(gr, 1, dr, 0); add_cable(gr, 2, dr, 1); add_cable(gr, 3, dr, 2);
    add_cable(gr, 2, mc, 0);                                            // snare pattern also fires the bongos
    add_cable(gr, 1, vo, 0);                                            // bass locks to the kick
    // melody source → bass (down an octave) + bongo pitch (mid register)
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0);
    add_cable(qt, 1, xp, 0); add_cable(xp, 1, vo, 1);
    add_cable(qt, 1, mc, 1);
}
void preset_chamber(void) {      // shows off the new modeled engines: a BOWED string (violin/cello)
                                 // on slow sparse bows, drenched in hall reverb. Sustaining-engine demo.
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK,  bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int tm = spawn(MOD_TURING, bayx(2), bayy(2)), qt = spawn(MOD_QUANT,  bayx(3), bayy(3));
    int mc = spawn(MOD_MACRO,  bayx(4), bayy(4)), rv = spawn(MOD_RVB,    bayx(5), bayy(5));
    mod[ck].param[CK_BPM] = 68;
    mod[eu].param[0] = 2; mod[eu].param[1] = 16;                        // very sparse → long held bows
    mod[tm].param[0] = 0.2f; mod[qt].param[0] = SCALE_MAJOR;
    mod[mc].param[MK_ENG] = 12;                                         // BOWED — violin/cello
    mod[mc].param[MK_HARM] = 0.4f; mod[mc].param[MK_TIMB] = 0.4f; mod[mc].param[MK_MORPH] = 0.55f;  // bow pos/pressure/swell
    mod[rv].param[RVK_SIZE] = 0.75f; mod[rv].param[RVK_MIX] = 0.4f;     // concert hall (master reverb)
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, mc, 1);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, mc, 0);                   // sparse gates hold long bows
}

const char *PRESET_NAMES[] = { "Empty", "Generative", "Acid bass", "Beats", "Keys synth", "PWM pad", "Turing", "Grids beat", "Marbles", "Maths sweep", "Env pluck", "Zap lead", "Punch (VCA)", "Glide", "BP acid", "Notch phaser", "Seq melody", "Vibrato", "Chance gates", "Macro voice", "Mix mod", "Clockless", "Polymeter", "ADSR pad", "PD reso", "Organ jam", "Space dub", "Sat bus", "Split FX", "Filter jam", "Tides", "Electronium", "Bandito", "Chamber" };
void (*PRESET_FN[])(void) = { preset_empty, preset_generative, preset_acid, preset_beats, preset_keys, preset_pwmpad, preset_turing, preset_grids, preset_marbles, preset_maths, preset_envpluck, preset_zaplead, preset_punch, preset_glide, preset_bpacid, preset_notchphaser, preset_seq, preset_vibe, preset_chance, preset_macro, preset_mix, preset_clockless, preset_polymeter, preset_adsrpad, preset_pdreso, preset_organ, preset_spacedub, preset_satbus, preset_splitfx, preset_filterjam, preset_tides, preset_electronium, preset_bandito, preset_chamber };
#define NPRESET 34

// ── persistence ──
typedef struct { int type, x, y; float param[8]; } SaveMod;
typedef struct { int nmod; SaveMod m[MAX_MOD]; int ncable; Cable cable[MAXCABLE]; } Patch;

void save_patch(void) {
    Patch p; p.nmod = nmod; p.ncable = ncable;
    for (int i = 0; i < nmod; i++) { p.m[i].type = mod[i].type; p.m[i].x = mod[i].x; p.m[i].y = mod[i].y; for (int k = 0; k < 8; k++) p.m[i].param[k] = mod[i].param[k]; }
    for (int c = 0; c < ncable; c++) p.cable[c] = cable[c];
    save_bytes(&p, sizeof p);
    msg = "SAVED"; msg_flash = 40;
}
// Loading VALIDATES, never trusts: the blob is raw struct bytes from disk (size-checked
// only), so a stale/corrupt/foreign save can carry any type/index/float. Out-of-range
// module types become SCOPEs, out-of-range params snap to the knob's default (this also
// kills NaNs and a div-by-zero in DIV's eval), and cables are dropped unless both ends
// exist, point at real jacks, and run out → in.
void load_patch(void) {
    Patch p;
    if (load_bytes(&p, sizeof p) == (int)sizeof p) {
        note_off_all();   // presets do this; load must too, or ringing voices orphan into drones
        nmod = p.nmod < 0 ? 0 : p.nmod > MAX_MOD ? MAX_MOD : p.nmod;
        for (int i = 0; i < nmod; i++) {
            int t = p.m[i].type;
            if (t < 0 || t >= NTYPE) t = MOD_SCOPE;   // unknown kind (corrupt or future save) → harmless placeholder
            mod[i] = (Module){ t, p.m[i].x, p.m[i].y, {0}, {0}, {0} };
            for (int k = 0; k < TYPES[t].nknob; k++) {
                KnobDef kd = TYPES[t].knob[k];
                float v = p.m[i].param[k];
                mod[i].param[k] = (v >= kd.lo && v <= kd.hi) ? v : kd.def;   // NaN fails both → def
            }
        }
        ncable = 0;
        int want = p.ncable < 0 ? 0 : p.ncable > MAXCABLE ? MAXCABLE : p.ncable;
        for (int c = 0; c < want; c++) {
            Cable cb = p.cable[c];
            if (cb.sm < 0 || cb.sm >= nmod || cb.dm < 0 || cb.dm >= nmod) continue;
            ModType *st = &TYPES[mod[cb.sm].type], *dt = &TYPES[mod[cb.dm].type];
            if (cb.sj < 0 || cb.sj >= st->njack || cb.dj < 0 || cb.dj >= dt->njack) continue;
            if (!st->jack[cb.sj].out || dt->jack[cb.dj].out) continue;                  // must run out → in
            if (st->jack[cb.sj].type != dt->jack[cb.dj].type) continue;                 // same type-check the UI enforces
            cable[ncable++] = cb;
        }
        msg = "LOADED";
    } else msg = "no save";
    msg_flash = 40;
}

void init(void) {
    // four DRAWN timbres (wave_set → INSTR_USER0..3) for the wav knob's org/vox/bel/fld
    // positions — single-cycle tables, integer harmonics only so the cycle wraps cleanly
    float tbl[64];
    for (int i = 0; i < 64; i++) { float x = i / 64.0f * 6.2831853f;             // ORG — drawbar-organ sum
        tbl[i] = 0.55f * sinf(x) + 0.28f * sinf(2 * x) + 0.18f * sinf(3 * x) + 0.12f * sinf(4 * x); }
    wave_set(0, tbl, 64);
    for (int i = 0; i < 64; i++) { float x = i / 64.0f * 6.2831853f;             // VOX — hollow odd partials
        tbl[i] = 0.45f * sinf(x) + 0.30f * sinf(3 * x) + 0.18f * sinf(5 * x) + 0.07f * sinf(7 * x); }
    wave_set(1, tbl, 64);
    for (int i = 0; i < 64; i++) { float x = i / 64.0f * 6.2831853f;             // BEL — sparse high partials, metallic
        tbl[i] = 0.60f * sinf(x) + 0.35f * sinf(4 * x) + 0.25f * sinf(7 * x) + 0.15f * sinf(10 * x); }
    wave_set(2, tbl, 64);
    for (int i = 0; i < 64; i++) { float x = i / 64.0f * 6.2831853f;             // FLD — wavefolded sine, gnarly
        tbl[i] = sinf(2.5f * sinf(x)); }
    wave_set(3, tbl, 64);

    // nine voice slots (5-13) that differ ONLY in waveform, so VOICE's wav knob is a pure
    // waveshape selector (same ADSR + filter; cutoff/res/pw are driven live per note)
    int waves[9] = { INSTR_SAW, INSTR_SQUARE, INSTR_TRI, INSTR_SINE, INSTR_NOISE,
                     INSTR_USER0, INSTR_USER1, INSTR_USER2, INSTR_USER3 };
    for (int i = 0; i < 9; i++) { instrument(5 + i, waves[i], 6, 120, 4, 300); instrument_filter(5 + i, FILTER_LOW, 800, 8); }
    instrument_duty(6, 0.5f);   // square's pw knob overrides this live
    // a second bank (14-22): same waves but a FLAT envelope (full sustain, no baked decay).
    // The VOICE plays from this bank when its amp jack is patched, so the patched ENV fully
    // shapes the amplitude (a true VCA) instead of fighting the slot's own 120ms decay.
    for (int i = 0; i < 9; i++) { instrument(14 + i, waves[i], 2, 0, 7, 8); instrument_filter(14 + i, FILTER_LOW, 800, 8); }
    // MACRO voice's three modeled engines, one slot each (23-25). PLUCK/MALLET decay on
    // their own — the long release just lets them ring out after the gate drops; FM gets
    // the epiano-ish DX strike (decaying brightness) from the fm cart's tuning.
    instrument(23, INSTR_PLUCK,  1,   0, 7, 1200);
    instrument(24, INSTR_MALLET, 1,   0, 7, 1200);
    instrument(25, INSTR_FM,     2, 700, 3,  350);
    // the three newer modeled engines — slots 29-31 (ENG_SLOT in eval MUST match). ORGAN/PD
    // hold while gated (sustain 7); EPIANO is struck and rings out on the long release.
    instrument(29, INSTR_ORGAN,  1,   0, 7,  200);
    instrument(30, INSTR_EPIANO, 1,   0, 7, 1200);
    instrument(31, INSTR_PD,     2, 500, 6,  400);
    // the eight remaining modeled engines — slots 32-39 (ENG_SLOT in eval MUST match), unlocked by
    // SOUND_INSTR_SLOTS 32→48. Ring-out engines (MEMBRANE/GUITAR/PIANO) get a long release like
    // PLUCK; the self-oscillating/blown ones (REED/PIPE/VOICE/BOWED/BRASS) hold while gated (sus 7).
    instrument(32, INSTR_MEMBRANE, 1, 0, 7, 1200);   // struck drumhead — tabla/conga/bongo (rings)
    instrument(33, INSTR_REED,     2, 0, 7,  300);   // clarinet/sax (holds)
    instrument(34, INSTR_PIPE,     2, 0, 7,  300);   // flute/recorder (holds)
    instrument(35, INSTR_VOICE,    2, 0, 7,  300);   // formant voice — sings/speaks (holds)
    instrument(36, INSTR_GUITAR,   1, 0, 7, 1200);   // string + body — nylon/banjo/uke (rings)
    instrument(37, INSTR_PIANO,    1, 0, 7, 1200);   // stiff-string — grand/harpsichord (rings)
    instrument(38, INSTR_BOWED,    2, 0, 7,  300);   // bowed string — violin/cello (holds)
    instrument(39, INSTR_BRASS,    2, 0, 7,  300);   // lip-reed brass — trumpet→tuba (holds)
    // DRUM voices (26-28) — the §17 "darker drums" pass. The kick is the punch-preset
    // recipe baked in: a LONG sine whose pitch env does the donk (909 boom), not a 90ms
    // tri tick. Snare = band-passed noise over a tonal body; hat = high-passed noise.
    instrument(26, INSTR_SINE,  0, 280, 0, 60);                // kick body
    instrument_env(26, 1, ENV_PITCH, 0, 55, 30);               // starts +30 st, settles fast = punch
    instrument(27, INSTR_NOISE, 0, 130, 0, 50);                // snare rattle
    instrument_filter(27, FILTER_BAND, 1400, 3);
    instrument(28, INSTR_NOISE, 0, 25, 0, 15);                 // hat
    instrument_filter(28, FILTER_HIGH, 6500, 2);

    // registry tripwire: every loop in the cart indexes jack[8]/knob[8]/param[8]/jackval[8].
    // A ModType declaring more than 8 of either reads/writes garbage — fail LOUDLY at boot
    // instead of corrupting memory quietly. (Hit this check? Grow the arrays together.)
    for (int t = 0; t < NTYPE; t++)
        if (TYPES[t].njack > 8 || TYPES[t].nknob > 8) {
            msg = "BAD ModType registry: >8 jacks/knobs"; msg_flash = 99999;
        }

    // master FX chain: the SAT/CRUSH/ECHO/VERB modules drive these master inserts (in this order
    // — saturate → bitcrush → delay → space). All idle at mix 0 until a module is present, so a
    // rack with no FX module is byte-identical clean. (Per-voice DRIVE isn't a bus insert.)
    int mfx[] = { FX_FILTER, FX_FORMANT, FX_WAH, FX_EQ, FX_DRIVE, FX_CRUSH, FX_GRAINS, FX_ECHO, FX_REVERB };
    fx_order(0, mfx, 9);

    preset_generative();
}

void eval_mod(int mi) {
    Module *m = &mod[mi];
    switch (m->type) {
        case MOD_CLOCK: {
            // each CLOCK runs its OWN tempo off dt() — NOT the global bpm() — so multiple
            // clocks are independent and only drive what they're patched to. 8th-note steps
            // (2 per beat); state[0]=phase, state[1]=last step, state[2]=LED flash timer.
            // swg delays every ODD step by that fraction of a step (the off-beat drags —
            // swing); /2 and /4 fire on even steps only, so the on-beats stay straight.
            m->state[0] += dt() * (m->param[CK_BPM] / 60.0f) * 2.0f;
            int   n  = (int)m->state[0];
            float fr = m->state[0] - n;
            int s = (n % 2 == 1 && fr < m->param[CK_SWG]) ? n - 1 : n;   // odd onset not reached yet
            bool ns = s != (int)m->state[1];
            m->state[1] = s;
            m->state[2] = ns ? 0 : m->state[2] + 1;
            m->jackval[0] = ns ? 1 : 0;                       // /1 — every step
            m->jackval[1] = (ns && s % 2 == 0) ? 1 : 0;       // /2
            m->jackval[2] = (ns && s % 4 == 0) ? 1 : 0;       // /4
            break; }
        case MOD_LFO:
            m->state[0] += m->param[0] * dt();
            if (m->state[0] >= 1.0f) m->state[0] -= 1.0f;
            // shp picks the waveform via the engine's own dispatcher (sin/sqr/tri/saw/rmp/opt);
            // S&H lives in the dedicated MOD_SH module. -1..1 → 0..1 CV.
            m->jackval[0] = (lfo_value((int)m->param[1], m->state[0]) + 1.0f) * 0.5f;
            break;
        case MOD_SH: {
            float clk = read_in(mi, 1);
            if (clk > 0.5f && m->state[0] <= 0.5f) m->state[1] = read_in(mi, 0);
            m->state[0] = clk;
            m->jackval[2] = m->state[1];
            break; }
        case MOD_QUANT: {
            int n = (int)(clamp(read_in(mi, 0), 0, 1) * 7.999f);
            m->jackval[1] = degree((int)m->param[0], ROOT_OCT, n) + (int)m->param[1];
            dbg_midi = (int)m->jackval[1];
            break; }
        case MOD_VOICE: {
            if (cable_into(mi, 0) < 0) {   // gate input unpatched → release (no dangling drone)
                int h = (int)m->state[1]; if (h > 0) { note_off(h); m->state[1] = 0; }
            }
            float gate = read_in(mi, 0), pitch = read_in(mi, 1), fcv = read_in(mi, 2);
            bool amp_cv = cable_into(mi, 5) >= 0;   // 'a' patched → ENV shapes the amplitude (a VCA)
            int slot = (amp_cv ? 14 : 5) + (int)m->param[VK_WAV];   // wav picks the wave (9 incl. user waves); VCA uses the flat-envelope bank (14-22)
            if (gate > 0.5f && m->state[0] <= 0.5f) {
                int mm = (int)pitch; if (mm < 1) mm = 48;
                int h = (int)m->state[1]; if (h > 0) note_off(h);
                h = note_on(mm, slot, amp_cv ? 0 : 5);   // VCA-driven → start silent, the amp CV opens it
                m->state[1] = h;
                // onboard mod-envelopes — fire per note: filter "pew" + pitch punch (fixed
                // snappy decays; the knobs set the depth, 0 = off). This is the audio-rate
                // upgrade over patching the control-rate ENV module into 'f'.
                note_env(h, 0, ENV_CUTOFF, 0, 130, m->param[VK_FENV]);   // fenv → filter sweep
                note_env(h, 1, ENV_PITCH,  0,  45, m->param[VK_PENV]);   // penv → pitch blip
                note_env(h, 2, ENV_DUTY,   0, 130, m->param[VK_DENV]);   // denv → PWM sweep (square/pulse only)
                m->state[3] = 0;   // trigger flash
            }
            m->state[0] = gate; m->state[3] += 1;
            m->state[2] = m->param[VK_CUT] + clamp(fcv, 0, 1) * 1800.0f;   // cutoff = base knob + 'f' CV
            int h = (int)m->state[1];
            if (h > 0) {
                note_pitch(h, pitch < 1 ? 48.0f : pitch);                                     // track pitch CV every frame (enables live vibrato, bends)
                note_filter(h, 1 + (int)clamp(m->param[VK_FLT], 0, 3));                      // lp/hp/bp/nt
                note_cutoff(h, (int)m->state[2]);
                note_res(h, clamp(m->param[VK_RES] + read_in(mi, 3) * 15.0f, 0, 15));
                note_duty(h, clamp(m->param[VK_PW] + read_in(mi, 4) * 0.5f, 0.05f, 0.95f));
                if (amp_cv) note_vol(h, clamp(read_in(mi, 5), 0, 1) * 7.0f);
                // vb jack (6): if patched to a VIBE, read its params and apply audio-rate LFO
                int vc = cable_into(mi, 6);
                if (vc >= 0 && mod[cable[vc].sm].type == MOD_VIBRATO && mod[cable[vc].sm].jackval[1] > 0.5f) {
                    Module *vb = &mod[cable[vc].sm];
                    int dst = (int)clamp(vb->param[BK_DEST], 0, 6);
                    int dests[] = { LFO_PITCH, LFO_CUTOFF, LFO_DUTY, LFO_VOLUME, LFO_HARMONICS, LFO_TIMBRE, LFO_MORPH };
                    float d = vb->param[BK_DEPTH];
                    float dep[] = { d * 2.5f, d * 800.0f, d * 0.35f, d, d, d, d };   // vol = tremolo; har/tmb/mor = macro swing (no-op on a wavetable VOICE)
                    note_lfo(h, 0, dests[dst], vb->param[BK_RATE], dep[dst]);
                }
            }
            break; }
        case MOD_EUCLID: {
            float clk = read_in(mi, 0);
            m->jackval[1] = 0;
            if (clk > 0.5f && m->state[0] <= 0.5f) {
                m->state[1] += 1;
                if (euclid((int)m->param[0], (int)m->param[1], (int)m->state[1])) { m->jackval[1] = 1; m->state[2] = 0; }
            }
            m->state[0] = clk; m->state[2] += 1;
            break; }
        case MOD_ENV: {
            float g = read_in(mi, 0);
            if (g > 0.5f && m->state[0] <= 0.5f) { m->state[1] = 0; m->state[2] = 1; }
            m->state[0] = g;
            if (m->state[2] > 0.5f) {
                m->state[1] += dt();
                float a = m->param[0], d = m->param[1];
                if      (m->state[1] < a)     m->state[3] = m->state[1] / a;
                else if (m->state[1] < a + d) m->state[3] = 1.0f - (m->state[1] - a) / d;
                else { m->state[3] = 0; m->state[2] = 0; }
            }
            m->jackval[1] = clamp(m->state[3], 0, 1);
            break; }
        case MOD_DRUM: {   // slots 26-28 defined in init() — see the "darker drums" block there
            float k = read_in(mi, 0), s = read_in(mi, 1), h = read_in(mi, 2);
            if (k > 0.5f && m->state[0] <= 0.5f) { hit(72, INSTR_NOISE, 2, 12); hit(34, 26, 7, 250); m->state[3] = 0; } m->state[0] = k; m->state[3] += 1;
            if (s > 0.5f && m->state[1] <= 0.5f) { hit(58, 27, 5, 110); hit(53, INSTR_TRI, 3, 45); m->state[4] = 0; } m->state[1] = s; m->state[4] += 1;
            if (h > 0.5f && m->state[2] <= 0.5f) { hit(92, 28, 3, 24); m->state[5] = 0; } m->state[2] = h; m->state[5] += 1;
            break; }
        case MOD_SLEW: {   // smooth a CV toward its input → glide / lag
            float k = clamp(16.67f / m->param[0], 0.02f, 1.0f);
            m->state[0] += (read_in(mi, 0) - m->state[0]) * k;
            m->jackval[1] = m->state[0];
            break; }
        case MOD_ATTN:     // scale a CV (depth)
            m->jackval[1] = clamp(read_in(mi, 0) * m->param[0], 0, 1);
            break;
        case MOD_LOGIC: {  // combine two gates
            bool a = read_in(mi, 0) > 0.5f, b = read_in(mi, 1) > 0.5f; int md = (int)m->param[0];
            m->jackval[2] = (md == 0 ? (a && b) : md == 1 ? (a || b) : (a != b)) ? 1 : 0;
            break; }
        case MOD_SCOPE: {  // record a 20-sample history of the cv input (drawn in draw_extras)
            int head = ((int)m->state[0] + 1) % 20; m->state[0] = head;
            m->state[2 + head] = clamp(read_in(mi, 0), 0, 1);
            break; }
        case MOD_KEYS: {   // playable: computer keys A S D F G H J or clicking the on-screen keys
            const char KK[7] = { 'A','S','D','F','G','H','J' }; const int OFF[7] = { 0,2,4,5,7,9,11 };
            int base = 48, pitch = -1;
            for (int i = 0; i < 7; i++) if (key(KK[i])) pitch = base + OFF[i];
            if (mouse_down(MOUSE_LEFT) && drag_mod < 0 && palette_drag < 0 && help_type < 0)
                for (int i = 0; i < 7; i++) { int kx = m->x + 3 + i * 10; if (wmx >= kx && wmx < kx + 9 && wmy >= m->y + 14 && wmy < m->y + 42) pitch = base + OFF[i]; }
            m->state[0] = pitch >= 0 ? 1 : 0;            // gate
            if (pitch >= 0) m->state[1] = pitch;          // pitch (holds last)
            m->jackval[0] = m->state[0];
            m->jackval[1] = m->state[1];
            break; }
        case MOD_TURING: {   // looping shift-register: head cycles a ring; each new step mutates with prob 'rnd'
            if (m->state[18] < 0.5f) { for (int i = 0; i < 16; i++) m->state[2 + i] = rnd_float(); m->state[18] = 1; }   // seed the loop
            int len = (int)m->param[1]; if (len < 2) len = 2; if (len > 16) len = 16;
            float clk = read_in(mi, 0);
            if (clk > 0.5f && m->state[0] <= 0.5f) {
                int head = ((int)m->state[1] + 1) % len;
                if (rnd_float() < m->param[0]) m->state[2 + head] = rnd_float();   // mutate (rnd=0 locks the loop)
                m->state[1] = head;
            }
            m->state[0] = clk;
            m->jackval[1] = m->state[2 + (int)m->state[1]];
            break; }
        case MOD_GRIDS: {    // drum-pattern generator → kick/snare/hat gates
            static const float STR[3][16] = {
                { 1.0f,.1f,.3f,.1f, .8f,.1f,.4f,.2f, .9f,.1f,.3f,.5f, .7f,.1f,.4f,.6f },   // kick
                { .1f,.1f,.4f,.2f, 1.0f,.1f,.3f,.4f, .2f,.3f,.5f,.2f, .9f,.2f,.6f,.5f },   // snare
                { .9f,.4f,.8f,.4f, .85f,.4f,.8f,.5f, .9f,.4f,.8f,.5f, .85f,.4f,.8f,.6f } }; // hat
            float clk = read_in(mi, 0);
            m->jackval[1] = m->jackval[2] = m->jackval[3] = 0;
            if (clk > 0.5f && m->state[0] <= 0.5f) {
                int step = ((int)m->state[1] + (int)(m->param[0] * 16)) % 16;     // map = pattern rotation
                for (int d = 0; d < 3; d++)
                    if (STR[d][step] >= 1.0f - m->param[1]) { m->jackval[1 + d] = 1; m->state[2 + d] = 0; }   // fill = threshold
                m->state[1] += 1;
            }
            m->state[0] = clk; for (int d = 0; d < 3; d++) m->state[2 + d] += 1;
            break; }
        case MOD_MARBLES: {  // shaped randomness: a random gate (density) + a held random cv (spread)
            float clk = read_in(mi, 0);
            m->jackval[1] = 0;
            if (clk > 0.5f && m->state[0] <= 0.5f) {
                if (rnd_float() < m->param[0]) { m->jackval[1] = 1; m->state[3] = 0; }                         // gate fires with prob 'dens'
                m->state[1] = clamp(0.5f + (rnd_float() - 0.5f) * m->param[1], 0, 1);                          // held cv, width 'sprd'
            }
            m->state[0] = clk; m->state[3] += 1;
            m->jackval[2] = m->state[1];
            break; }
        case MOD_MATHS: {    // rise/fall function gen: env when triggered, LFO when cycling, + EOC
            float trig = read_in(mi, 0);
            bool cyc = m->param[2] > 0.5f;
            m->jackval[2] = 0;
            if ((trig > 0.5f && m->state[0] <= 0.5f) || (cyc && m->state[1] < 0.5f)) m->state[1] = 1;   // start rising
            m->state[0] = trig;
            if (m->state[1] > 1.5f) {            // falling
                m->state[3] -= dt() / (m->param[1] < 0.002f ? 0.002f : m->param[1]);
                if (m->state[3] <= 0) { m->state[3] = 0; m->state[1] = 0; m->jackval[2] = 1; m->state[4] = 0; }   // end-of-cycle pulse
            } else if (m->state[1] > 0.5f) {     // rising
                m->state[3] += dt() / (m->param[0] < 0.002f ? 0.002f : m->param[0]);
                if (m->state[3] >= 1) { m->state[3] = 1; m->state[1] = 2; }
            }
            m->jackval[1] = clamp(m->state[3], 0, 1);
            m->state[4] += 1;
            break; }
        case MOD_SEQ: {
            float clk = read_in(mi, 0);
            m->jackval[2] = 0;
            if (clk > 0.5f && m->state[1] <= 0.5f) {
                m->state[0] = ((int)m->state[0] + 1) % 8;
                m->jackval[2] = 1;
                m->state[2] = 0;
            }
            m->state[1] = clk; m->state[2] += 1;
            m->jackval[1] = m->param[(int)m->state[0]];   // current step cv
            break; }
        case MOD_VIBRATO: {
            bool en = cable_into(mi, 0) < 0 || read_in(mi, 0) > 0.5f;
            m->jackval[1] = en ? 1.0f : 0.0f;   // output enable signal; VOICE reads params via cable
            break; }
        case MOD_CHANCE: {
            float g = read_in(mi, 0);
            m->jackval[1] = 0;
            if (g > 0.5f && m->state[0] <= 0.5f)
                if (rnd_float() < m->param[0]) { m->jackval[1] = 1; m->state[1] = 0; }
            m->state[0] = g; m->state[1] += 1;
            break; }
        case MOD_MACRO: {
            if (cable_into(mi, 0) < 0) {   // gate input unpatched → release (no dangling drone)
                int h = (int)m->state[1]; if (h > 0) { note_off(h); m->state[1] = 0; }
            }
            // eng → slot: plk/mlt/fm live at 23-25; the three newer engines at 29-31
            // (26-28 are the drum voices, so this can't be plain 23+eng). MUST match init().
            static const int ENG_SLOT[14] = { 23, 24, 25, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39 };
            int slot = ENG_SLOT[(int)clamp(m->param[MK_ENG], 0, 13)];
            float gate = read_in(mi, 0), pitch = read_in(mi, 1);
            float hv = clamp(m->param[MK_HARM]  + read_in(mi, 2), 0, 1);   // macro = knob + CV inlet
            float tv = clamp(m->param[MK_TIMB]  + read_in(mi, 3), 0, 1);
            float mv = clamp(m->param[MK_MORPH] + read_in(mi, 4), 0, 1);
            // per-voice character (knobs, no CV inlets): drive = gnarl, eko = echo-bus send,
            // tun = detune. Set on the slot every frame — instrument_tune is live, drive/echo
            // reach a sounding voice via note_* below; new notes pick them all up.
            float dv = clamp(m->param[MK_DRV], 0, 1), ev = clamp(m->param[MK_EKO], 0, 1), tn = m->param[MK_TUN];
            instrument_drive(slot, dv); instrument_echo(slot, ev); instrument_tune(slot, tn);
            if (gate > 0.5f && m->state[0] <= 0.5f) {
                // strike-shaping macros (pluck pick, mallet hardness) are read at excitation
                // time — push them onto the slot BEFORE note_on (the request queue keeps order)
                instrument_harmonics(slot, hv); instrument_timbre(slot, tv); instrument_morph(slot, mv);
                int mm = (int)pitch; if (mm < 1) mm = 48;
                int h = (int)m->state[1]; if (h > 0) note_off(h);
                m->state[1] = note_on(mm, slot, 5);
                m->state[3] = 0;   // trigger flash
            } else if (gate <= 0.5f && m->state[0] > 0.5f) {
                // gate fall = let go: the engine rings through its release (a pluck's whole
                // point). Drop the handle — the tail keeps its baked pitch/macros.
                int h = (int)m->state[1]; if (h > 0) { note_off(h); m->state[1] = 0; }
            }
            m->state[0] = gate; m->state[3] += 1;
            m->state[5] = hv; m->state[6] = tv; m->state[7] = mv;   // live bars in draw_extras
            int h = (int)m->state[1];
            if (h > 0) {   // live macros reach the sounding voice, slewed (FM brightness, mallet ring, pluck damping)
                note_pitch(h, pitch < 1 ? 48.0f : pitch);
                note_harmonics(h, hv); note_timbre(h, tv); note_morph(h, mv);
                note_drive(h, dv); note_echo(h, ev);   // tune is already live via instrument_tune
                // vb jack (5): a patched VIBE drives an audio-rate LFO on a macro (or any dest) —
                // har/tmb/mor are the new macro destinations (e.g. LFO the PD DCW sweep)
                int vc = cable_into(mi, 5);
                if (vc >= 0 && mod[cable[vc].sm].type == MOD_VIBRATO && mod[cable[vc].sm].jackval[1] > 0.5f) {
                    Module *vb = &mod[cable[vc].sm];
                    int dst = (int)clamp(vb->param[BK_DEST], 0, 6);
                    int dests[] = { LFO_PITCH, LFO_CUTOFF, LFO_DUTY, LFO_VOLUME, LFO_HARMONICS, LFO_TIMBRE, LFO_MORPH };
                    float d = vb->param[BK_DEPTH];
                    float dep[] = { d * 2.5f, d * 800.0f, d * 0.35f, d, d, d, d };
                    note_lfo(h, 0, dests[dst], vb->param[BK_RATE], dep[dst]);
                }
            }
            break; }
        case MOD_XPOSE: {  // precision adder for pitch lines: out = in + oct*12 (whole octaves, snapped)
            int oct = (int)floorf(m->param[0] + 0.5f);
            float p = read_in(mi, 0);
            m->jackval[1] = p < 1 ? 0 : clamp(p + oct * 12, 1, 127);   // no input → no pitch (downstream default kicks in)
            break; }
        case MOD_MIX:      // attenuverting mixer: combine two modulators into one cv (invert via negative knobs)
            m->jackval[2] = clamp(read_in(mi, 0) * m->param[MX_A] + read_in(mi, 1) * m->param[MX_B] + m->param[MX_OFF], 0, 1);
            break;
        case MOD_CMP:      // comparator: cv → gate (high while above threshold — pulse width is the knob)
            m->jackval[1] = read_in(mi, 0) > m->param[0] ? 1 : 0;
            break;
        case MOD_DIV: {    // clock divider: one gate out every Nth input pulse
            float clk = read_in(mi, 0);
            m->jackval[1] = 0;
            if (clk > 0.5f && m->state[0] <= 0.5f) {
                m->state[1] += 1;
                if ((int)m->state[1] % (int)m->param[0] == 0) { m->jackval[1] = 1; m->state[2] = 0; }
            }
            m->state[0] = clk; m->state[2] += 1;
            break; }
        case MOD_ADSR: {   // gate-length-aware envelope: attack → decay → HOLD at sus → release
            float g = read_in(mi, 0);
            bool gh = g > 0.5f;
            if (gh && m->state[0] <= 0.5f) m->state[1] = 1;                     // (re)trigger → attack (from current level: legato)
            if (!gh && m->state[1] > 0.5f && m->state[1] < 2.5f) m->state[1] = 3;   // gate fell mid-env → release
            if (m->state[1] == 1) {            // attack: rise to 1
                m->state[2] += dt() / (m->param[AK_ATK] > 0.002f ? m->param[AK_ATK] : 0.002f);
                if (m->state[2] >= 1) { m->state[2] = 1; m->state[1] = 2; }
            } else if (m->state[1] == 2) {     // decay toward sus, then HOLD there while the gate stays up
                float sus = m->param[AK_SUS];
                if (m->state[2] > sus) {
                    m->state[2] -= dt() / (m->param[AK_DEC] > 0.002f ? m->param[AK_DEC] : 0.002f);
                    if (m->state[2] < sus) m->state[2] = sus;
                } else m->state[2] = sus;
            } else if (m->state[1] == 3) {     // release: fall to 0
                m->state[2] -= dt() / (m->param[AK_REL] > 0.002f ? m->param[AK_REL] : 0.002f);
                if (m->state[2] <= 0) { m->state[2] = 0; m->state[1] = 0; }
            }
            m->state[0] = g;
            m->jackval[1] = clamp(m->state[2], 0, 1);
            break; }
        case MOD_TIDES: {   // morphing LFO / one-shot function gen (Mutable Tides)
            float freq  = m->param[TDK_FREQ];
            float slope = clamp(m->param[TDK_SLOPE], 0.02f, 0.98f);   // peak position in the cycle
            float e     = powf(4.0f, (0.5f - m->param[TDK_SHAPE]) * 2.0f);   // shape: exp(>1)..lin(1)..log(<1)
            bool  trig_patched = cable_into(mi, 0) >= 0;
            float trig = read_in(mi, 0);
            m->jackval[2] = 0;   // eoc low by default
            if (trig_patched) {  // ONE-SHOT: a rising edge fires a single shaped cycle, then idle
                if (trig > 0.5f && m->state[2] <= 0.5f) { m->state[0] = 0; m->state[1] = 1; }
                m->state[2] = trig;
                if (m->state[1] > 0.5f) {
                    m->state[0] += freq * dt();
                    if (m->state[0] >= 1.0f) { m->state[0] = 0; m->state[1] = 0; m->jackval[2] = 1; m->state[3] = 0; }   // EOC → idle
                }
            } else {             // FREE-RUN LFO: phase loops, eoc pulses each wrap
                m->state[0] += freq * dt();
                if (m->state[0] >= 1.0f) { m->state[0] -= 1.0f; m->jackval[2] = 1; m->state[3] = 0; }
                m->state[1] = 1;
            }
            float ph = m->state[0], v;
            if (m->state[1] < 0.5f)   v = 0;                                      // idle (one-shot done)
            else if (ph < slope)      v = powf(ph / slope, e);                    // rise 0→1
            else                      v = powf(1.0f - (ph - slope) / (1.0f - slope), e);   // fall 1→0
            m->jackval[1] = clamp(v, 0, 1);
            m->state[3] += 1;
            break; }
    }
}

// ── FX modules — applied once per frame (after the eval loop, so cv jacks are fresh).
// Two routings, decided by each FX module's AUDIO-IN (pink, jack 1):
//   • UNPATCHED → it drives the GLOBAL master bus (reverb_insert/echo_insert/crush/drive_insert).
//     Only the first unpatched module of a kind drives it; absent kind → pushed OFF once.
//   • PATCHED from a VOICE/MACRO/DRUM audio-out → it effects JUST that part, via the per-instrument
//     API (instrument_reverb/echo/drive/crush on the source's slot[s]).
// Either way SET-AND-HOLD: re-apply only when a value moves, quantized so a cv-swept knob can't
// flood the request queue (the groovebox apply_fx() pattern). cv inlet (jack 0) adds to wet/amount.
static int q64(float v) { return (int)(clamp(v, 0, 1) * 64 + 0.5f); }
static int primary_of(int type) { for (int i = 0; i < nmod; i++) if (mod[i].type == type) return i; return -1; }

#define FX_AUDIO_IN 1   // FX modules: jack 0 = cv, jack 1 = audio-in (pink)
static int kind_index(int t) { return t == MOD_RVB ? 0 : t == MOD_ECHO ? 1 : t == MOD_DRIVE ? 2 : t == MOD_CRUSH ? 3
                                     : t == MOD_WAH ? 4 : t == MOD_VOWEL ? 5 : t == MOD_EQ ? 6 : t == MOD_GRAINS ? 7 : -1; }   // FILT/SAT master-only
#define NPFX 8   // per-part-capable FX kinds (0..7)
// first module of `type` whose audio-in is NOT patched — that one drives the MASTER bus
static int primary_master(int type) {
    for (int i = 0; i < nmod; i++) if (mod[i].type == type && cable_into(i, FX_AUDIO_IN) < 0) return i;
    return -1;
}
// the instrument slot(s) a source module feeds: VOICE/MACRO play one slot (its wav/eng knob picks
// it), DRUM plays the three drum slots. This is how a per-part effect finds its target.
static int source_slots(int mi, int *slots) {
    int t = mod[mi].type;
    if (t == MOD_VOICE) { bool amp = cable_into(mi, 5) >= 0; slots[0] = (amp ? 14 : 5) + (int)mod[mi].param[VK_WAV]; return 1; }
    if (t == MOD_MACRO) { static const int ES[14] = { 23, 24, 25, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39 }; slots[0] = ES[(int)clamp(mod[mi].param[MK_ENG], 0, 13)]; return 1; }
    if (t == MOD_DRUM)  { slots[0] = 26; slots[1] = 27; slots[2] = 28; return 3; }
    return 0;
}
// configure / clear one per-part effect on one slot (ki: 0 VERB 1 ECHO 2 DRIVE 3 CRUSH). VERB/ECHO
// are SENDS to shared buses (size/time are shared across the rack); DRIVE/CRUSH get private per-slot
// buses (fully independent). reverb()/echo() set the shared bus, instrument_* sets this slot's send.
static void pfx_apply(int ki, int slot, float a, float b, float c) {
    switch (ki) {
        case 0: reverb(a, 0.42f);      instrument_reverb(slot, c); break;   // a=size (shared room), c=send
        case 1: echo((int)a, b, 0.5f); instrument_echo(slot, c);   break;   // a=time (shared), b=fb, c=send
        case 2: instrument_drive_mode(slot, (int)b); instrument_drive(slot, a); break;  // a=amt, b=mode
        case 3: instrument_crush(slot, a, 6.0f, c); break;                  // a=bits, c=mix (private bus)
        case 4: instrument_wah_lfo(slot, a, b, c); break;                   // a=rate, b=res, c=mix
        case 5: instrument_formant(slot, a, b, c); break;                   // a=vowel, b=q, c=mix
        case 6: instrument_eq(slot, a, b, c); break;                        // a=lo, b=mid, c=hi (dB)
        default:instrument_grains(slot, a, b, 0.85f, 0.4f, 0.5f, c); break; // a=size, b=dens, c=mix (pos/scat/fb fixed)
    }
}
static void pfx_off(int ki, int slot) {
    switch (ki) {
        case 0: instrument_reverb(slot, 0); break;
        case 1: instrument_echo(slot, 0);   break;
        case 2: instrument_drive(slot, 0);  break;
        case 3: instrument_crush(slot, 8, 1, 0); break;
        case 4: instrument_wah_lfo(slot, 2, 0, 0); break;
        case 5: instrument_formant(slot, 0.3f, 0.5f, 0); break;
        case 6: instrument_eq(slot, 0, 0, 0); break;
        default:instrument_grains(slot, 120, 12, 0.85f, 0.4f, 0, 0); break;
    }
}

void apply_master_fx(void) {
    static int ap_rvb_sz = -1, ap_rvb_mix = -1;
    static int ap_ec_t = -1, ap_ec_fb = -1, ap_ec_mix = -1;
    static int ap_dr_amt = -1, ap_dr_mode = -1;
    static int ap_cr_bits = -1, ap_cr_mix = -1;
    static int ap_sat_amt = -1, ap_sat_mode = -1, ap_sat_mix = -1;
    static int ap_wah_rate = -1, ap_wah_res = -1, ap_wah_mix = -1;
    static int ap_vow_v = -1, ap_vow_q = -1, ap_vow_mix = -1;
    static int ap_eq_lo = -999, ap_eq_mid = -999, ap_eq_hi = -999;
    static int ap_fl_mode = -1, ap_fl_cut = -1, ap_fl_res = -1;
    static int ap_gr_size = -1, ap_gr_dens = -1, ap_gr_mix = -1;
    // per-part (pink-cable) state: applied last frame? + last quantized a/b/c, per (kind, slot)
    static unsigned char fx_on[NPFX][32];
    static int fx_qa[NPFX][32], fx_qb[NPFX][32], fx_qc[NPFX][32];
    unsigned char now[NPFX][32]; for (int k = 0; k < NPFX; k++) for (int s = 0; s < 32; s++) now[k][s] = 0;
    int mi;

    // ===== PER-PART: every FX module whose audio-in is patched effects that source's slot(s) =====
    for (int i = 0; i < nmod; i++) {
        int ki = kind_index(mod[i].type); if (ki < 0) continue;
        int ac = cable_into(i, FX_AUDIO_IN); if (ac < 0) continue;
        int slots[3], ns = source_slots(cable[ac].sm, slots); if (ns == 0) continue;
        float cv = read_in(i, 0);   // cv inlet still adds to the wet/amount
        float a, b, c;
        switch (ki) {
            case 0:  a = mod[i].param[RVK_SIZE]; b = 0; c = clamp(mod[i].param[RVK_MIX] + cv, 0, 1); break;
            case 1:  a = mod[i].param[ECK_TIME]; b = mod[i].param[ECK_FB]; c = clamp(mod[i].param[ECK_MIX] + cv, 0, 1); break;
            case 2:  a = clamp(mod[i].param[DRK_AMT] + cv, 0, 1); b = (int)clamp(mod[i].param[DRK_MODE], 0, 3); c = 0; break;
            case 3:  a = mod[i].param[CRK_BITS]; b = 0; c = clamp(mod[i].param[CRK_MIX] + cv, 0, 1); break;
            case 4:  a = mod[i].param[WHK_RATE]; b = mod[i].param[WHK_RES]; c = clamp(mod[i].param[WHK_MIX] + cv, 0, 1); break;  // WAH
            case 5:  a = clamp(mod[i].param[VWK_VOW] + cv, 0, 1); b = mod[i].param[VWK_Q]; c = mod[i].param[VWK_MIX]; break;     // VOWEL: cv→vowel
            case 6:  a = mod[i].param[EQK_LO]; b = mod[i].param[EQK_MID]; c = clamp(mod[i].param[EQK_HI] + cv * 6.0f, -12, 12); break;  // EQ: cv→hi (brightness)
            default: a = mod[i].param[GRK_SIZE]; b = mod[i].param[GRK_DENS]; c = clamp(mod[i].param[GRK_MIX] + cv, 0, 1); break;        // GRAINS: cv→mix
        }
        int qa = (int)(a * 100), qb = (int)(b * 100), qc = (int)(c * 100);
        for (int j = 0; j < ns; j++) {
            int s = slots[j]; if (s < 0 || s >= 32) continue;
            now[ki][s] = 1;
            if (!fx_on[ki][s] || fx_qa[ki][s] != qa || fx_qb[ki][s] != qb || fx_qc[ki][s] != qc) {
                pfx_apply(ki, s, a, b, c);
                fx_qa[ki][s] = qa; fx_qb[ki][s] = qb; fx_qc[ki][s] = qc;
            }
        }
    }
    // sweep: a (kind, slot) on last frame but not now (cable pulled / module deleted / re-pointed)
    // gets turned OFF once, so a part never keeps an orphaned effect.
    for (int k = 0; k < NPFX; k++) for (int s = 0; s < 32; s++) {
        if (fx_on[k][s] && !now[k][s]) { pfx_off(k, s); fx_qa[k][s] = fx_qb[k][s] = fx_qc[k][s] = -99999; }
        fx_on[k][s] = now[k][s];
    }

    // ===== MASTER: an UNPATCHED FX module of each kind drives the whole mix (as before) =====
    // VERB — master reverb insert (FX_REVERB in the init() chain; mix 0 = bypass dry)
    if ((mi = primary_master(MOD_RVB)) >= 0) {
        float size = mod[mi].param[RVK_SIZE];
        float mix  = clamp(mod[mi].param[RVK_MIX] + read_in(mi, 0), 0, 1);
        int sq = q64(size), mq = q64(mix);
        if (sq != ap_rvb_sz || mq != ap_rvb_mix) { reverb_insert(size, 0.42f, mix); ap_rvb_sz = sq; ap_rvb_mix = mq; }
    } else if (ap_rvb_mix != -1) { reverb_insert(0.5f, 0.42f, 0); ap_rvb_sz = ap_rvb_mix = -1; }

    // ECHO — master delay insert. tone fixed 0.5; cv adds to mix.
    if ((mi = primary_master(MOD_ECHO)) >= 0) {
        int   tms = (int)mod[mi].param[ECK_TIME];
        float fb  = mod[mi].param[ECK_FB];
        float mix = clamp(mod[mi].param[ECK_MIX] + read_in(mi, 0), 0, 1);
        int tq = tms / 8, fq = q64(fb), mq = q64(mix);
        if (tq != ap_ec_t || fq != ap_ec_fb || mq != ap_ec_mix) { echo_insert(tms, fb, 0.5f, mix); ap_ec_t = tq; ap_ec_fb = fq; ap_ec_mix = mq; }
    } else if (ap_ec_mix != -1) { echo_insert(200, 0, 0.5f, 0); ap_ec_t = ap_ec_fb = ap_ec_mix = -1; }

    // DRIVE — master = all subtractive VOICE slots (5..22), post-filter (the 303/acid scream). NOT
    // drums or MACRO. Skips any slot a per-part DRIVE already claims this frame. amt coarse-quantized
    // to 16 so a cv sweep over 18 slots stays easy on the queue; held VOICE notes update live.
    if ((mi = primary_master(MOD_DRIVE)) >= 0) {
        float amt = clamp(mod[mi].param[DRK_AMT] + read_in(mi, 0), 0, 1);
        int mode = (int)clamp(mod[mi].param[DRK_MODE], 0, 3);
        int aq = (int)(amt * 16 + 0.5f);
        if (aq != ap_dr_amt || mode != ap_dr_mode) {
            for (int s = 5; s <= 22; s++) { if (now[2][s]) continue; instrument_drive(s, amt); instrument_drive_mode(s, mode); }
            for (int v = 0; v < nmod; v++)
                if (mod[v].type == MOD_VOICE) { int h = (int)mod[v].state[1]; if (h > 0) { note_drive(h, amt); note_drive_mode(h, mode); } }
            ap_dr_amt = aq; ap_dr_mode = mode;
        }
    } else if (ap_dr_amt != -1) { for (int s = 5; s <= 22; s++) if (!now[2][s]) instrument_drive(s, 0); ap_dr_amt = ap_dr_mode = -1; }

    // CRUSH — master bitcrush on the whole mix. rate fixed 6 (lo-fi downsample); cv adds to mix.
    if ((mi = primary_master(MOD_CRUSH)) >= 0) {
        float bits = mod[mi].param[CRK_BITS];
        float mix  = clamp(mod[mi].param[CRK_MIX] + read_in(mi, 0), 0, 1);
        int bq = (int)bits, mq = q64(mix);
        if (bq != ap_cr_bits || mq != ap_cr_mix) { crush(bits, 6.0f, mix); ap_cr_bits = bq; ap_cr_mix = mq; }
    } else if (ap_cr_mix != -1) { crush(8, 6, 0); ap_cr_bits = ap_cr_mix = -1; }

    // SAT — whole-mix saturation insert (drive_insert / FX_DRIVE). Always master (no audio-in): the
    // drums grit up too — the difference from the per-voice DRIVE module. cv adds to amt.
    if ((mi = primary_of(MOD_SAT)) >= 0) {
        float amt = clamp(mod[mi].param[SATK_AMT] + read_in(mi, 0), 0, 1);
        int mode  = (int)clamp(mod[mi].param[SATK_MODE], 0, 3);
        float mix = mod[mi].param[SATK_MIX];
        int aq = q64(amt), mq = q64(mix);
        if (aq != ap_sat_amt || mode != ap_sat_mode || mq != ap_sat_mix) { drive_insert(amt, mode, mix); ap_sat_amt = aq; ap_sat_mode = mode; ap_sat_mix = mq; }
    } else if (ap_sat_amt != -1) { drive_insert(0, 0, 0); ap_sat_amt = ap_sat_mode = ap_sat_mix = -1; }

    // WAH — master LFO auto-wah on the whole mix. cv adds to mix.
    if ((mi = primary_master(MOD_WAH)) >= 0) {
        float rate = mod[mi].param[WHK_RATE], res = mod[mi].param[WHK_RES];
        float mix = clamp(mod[mi].param[WHK_MIX] + read_in(mi, 0), 0, 1);
        int rq = (int)(rate * 10), sq = q64(res), mq = q64(mix);
        if (rq != ap_wah_rate || sq != ap_wah_res || mq != ap_wah_mix) { wah_lfo(rate, res, mix); ap_wah_rate = rq; ap_wah_res = sq; ap_wah_mix = mq; }
    } else if (ap_wah_mix != -1) { wah_lfo(2, 0, 0); ap_wah_rate = ap_wah_res = ap_wah_mix = -1; }

    // VOWEL — master formant/vowel filter on the whole mix. cv sweeps the vowel (the talking filter).
    if ((mi = primary_master(MOD_VOWEL)) >= 0) {
        float vow = clamp(mod[mi].param[VWK_VOW] + read_in(mi, 0), 0, 1);
        float q = mod[mi].param[VWK_Q], mix = mod[mi].param[VWK_MIX];
        int vq = q64(vow), qq = q64(q), mq = q64(mix);
        if (vq != ap_vow_v || qq != ap_vow_q || mq != ap_vow_mix) { formant(vow, q, mix); ap_vow_v = vq; ap_vow_q = qq; ap_vow_mix = mq; }
    } else if (ap_vow_mix != -1) { formant(0.3f, 0.5f, 0); ap_vow_v = ap_vow_q = ap_vow_mix = -1; }

    // EQ — master 3-band tone on the whole mix (the only BOOST). cv adds to the high band (brightness).
    if ((mi = primary_master(MOD_EQ)) >= 0) {
        float lo = mod[mi].param[EQK_LO], mid = mod[mi].param[EQK_MID];
        float hi = clamp(mod[mi].param[EQK_HI] + read_in(mi, 0) * 6.0f, -12, 12);
        int lq = (int)lo, mq = (int)mid, hq = (int)hi;
        if (lq != ap_eq_lo || mq != ap_eq_mid || hq != ap_eq_hi) { eq(lo, mid, hi); ap_eq_lo = lq; ap_eq_mid = mq; ap_eq_hi = hq; }
    } else if (ap_eq_lo != -999) { eq(0, 0, 0); ap_eq_lo = ap_eq_mid = ap_eq_hi = -999; }

    // FILT — the DJ filter on the whole mix (master-only; cheap to sweep live). The cv inlet SWEEPS
    // the cutoff (patch an LFO/ENV). mode knob 0..3 = lp/hp/bp/nt → filter() wants 1..4 (+1).
    if ((mi = primary_of(MOD_FILT)) >= 0) {
        int mode = (int)clamp(mod[mi].param[FLK_MODE], 0, 3) + 1;   // FILTER_LOW..NOTCH (1..4)
        float cut = clamp(mod[mi].param[FLK_CUT] + read_in(mi, 0) * 8000.0f, 20, 13000);
        float res = mod[mi].param[FLK_RES];
        int cq = (int)(cut / 80), rq = q64(res);   // cutoff quantized to ~80Hz steps
        if (mode != ap_fl_mode || cq != ap_fl_cut || rq != ap_fl_res) { filter(mode, cut, res); ap_fl_mode = mode; ap_fl_cut = cq; ap_fl_res = rq; }
    } else if (ap_fl_mode != -1) { filter(0, 1000, 0); ap_fl_mode = ap_fl_cut = ap_fl_res = -1; }   // FILTER_OFF

    // GRAINS — master granular cloud on the whole mix (pos/scat/fb fixed to an evolving voicing). cv→mix.
    if ((mi = primary_master(MOD_GRAINS)) >= 0) {
        float size = mod[mi].param[GRK_SIZE], dens = mod[mi].param[GRK_DENS];
        float mix = clamp(mod[mi].param[GRK_MIX] + read_in(mi, 0), 0, 1);
        int zq = (int)(size / 8), dq = (int)dens, mq = q64(mix);
        if (zq != ap_gr_size || dq != ap_gr_dens || mq != ap_gr_mix) { grains(size, dens, 0.85f, 0.4f, 0.5f, mix); ap_gr_size = zq; ap_gr_dens = dq; ap_gr_mix = mq; }
    } else if (ap_gr_mix != -1) { grains(120, 12, 0.85f, 0.4f, 0, 0); ap_gr_size = ap_gr_dens = ap_gr_mix = -1; }
}

void update(void) {
    if (keyp('S')) save_patch();
    if (keyp('L')) load_patch();
    if (keyp('R')) preset_generative();
    if (msg_flash > 0) msg_flash--;

    int step8 = beat() * 2 + (int)(beat_pos() * 2.0f);
    g_newstep = step8 != last_step;
    if (g_newstep) { last_step = step8; tick_flash = 0; } else tick_flash++;
    g_step = step8;

    for (int i = 0; i < nmod; i++) eval_mod(i);
    apply_master_fx();   // configure the master bus / drive from any FX modules (set-and-hold)

#ifdef DE_TRACE
    watch("nmod", "%d", nmod);
    watch("cables", "%d", ncable);
    watch("midi", "%d", dbg_midi);
    watch("zoom", "%d", (int)(zoom * 100));
    watch("lastx", "%d", nmod > 0 ? mod[nmod - 1].x : -1);
    watch("help", "%d", help_type);
    watch("vh", "%d", nmod > 4 && mod[4].type == MOD_VOICE ? (int)mod[4].state[1] : -1);
#endif
}

// ── drawing ──
int sig_col(int t) { return t == 0 ? CLR_GREEN : t == 1 ? CLR_YELLOW : t == 3 ? CLR_PINK : CLR_BLUE; }   // 3 = audio (pink)

int near_col(int x, int y) {
    float d = distance(wmx, wmy, x, y);
    return d < 12 ? CLR_WHITE : d < 26 ? CLR_LIGHT_GREY : d < 48 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
}

const char *knob_str(int fmt, float v) {
    if (fmt == FMT_LOGIC) { const char *L[3] = { "AND", "OR", "XOR" }; return L[(int)v]; }
    if (fmt == FMT_WAVE)  { const char *W[9] = { "saw", "sqr", "tri", "sin", "noi", "org", "vox", "bel", "fld" }; return W[(int)v]; }   // org/vox/bel/fld = drawn user waves
    if (fmt == FMT_FILTER){ const char *F[4] = { "lp",  "hp",  "bp",  "nt"  }; return F[(int)clamp(v, 0, 3)]; }
    if (fmt == FMT_DEST)  { const char *D[7] = { "pit", "cut", "pw", "vol", "har", "tmb", "mor" }; return D[(int)clamp(v, 0, 6)]; }   // har/tmb/mor = engine macros (MACRO voice only)
    if (fmt == FMT_ENGINE){ const char *E[14] = { "plk", "mlt", "fm", "org", "ep", "pd", "mem", "reed", "pipe", "vox", "gtr", "pno", "bow", "brs" }; return E[(int)clamp(v, 0, 13)]; }   // all 14 modeled engines
    if (fmt == FMT_DRIVE) { const char *D[4] = { "sft", "hrd", "fld", "asy" }; return D[(int)clamp(v, 0, 3)]; }   // DRIVE_SOFT/HARD/FOLD/ASYM waveshaper
    if (fmt == FMT_LFOSHAPE) { const char *S[6] = { "sin", "sqr", "tri", "saw", "rmp", "opt" }; return S[(int)clamp(v, 0, 5)]; }   // LFO_SHAPE_* (stateless; S&H = the MOD_SH module)
    if (fmt == FMT_OCT)   { int o = (int)floorf(v + 0.5f); return o == 0 ? "0" : str("%+d", o); }          // snapped whole octaves
    if (fmt == FMT_DIV)   return str("/%d", (int)v);                                                       // clock division
    if (fmt == FMT_SCALE) return SCALES[(int)v];
    if (fmt == FMT_NOTE)  return NOTES[(int)v];
    if (fmt == FMT_F1)    return str("%.1f", v);
    if (fmt == FMT_PCT)   return str("%d%%", (int)(v * 100 + 0.5f));
    if (fmt == FMT_MS)    return str("%dms", (int)(v * 1000));
    return str("%d", (int)v);
}

// jack hit-test → packed mi*4 + j, or -1
int jack_at(int mx, int my) {
    for (int mi = 0; mi < nmod; mi++)
        for (int j = 0; j < TYPES[mod[mi].type].njack; j++)
            if (distance(mx, my, jackpos_x(mi, j), jackpos_y(mi, j)) < (zoom < 1.0f ? 6.0f / zoom : 6.0f)) return mi * 8 + j;
    return -1;
}
int knob_at(int mx, int my) {   // any knob under (mx,my)? (used to keep panning off knobs)
    for (int mi = 0; mi < nmod; mi++)
        for (int k = 0; k < TYPES[mod[mi].type].nknob; k++)
            if (distance(mx, my, mod[mi].x + TYPES[mod[mi].type].knob[k].dx, mod[mi].y + TYPES[mod[mi].type].knob[k].dy) < 7) return mi;
    return -1;
}
int delx_at(int mx, int my) {   // the ✕ delete corner of a module
    for (int mi = 0; mi < nmod; mi++)
        if (distance(mx, my, mod[mi].x + tw(mod[mi].type) - 6, mod[mi].y + 4) < 5) return mi;
    return -1;
}
int qmark_at(int mx, int my) {  // the ? help corner of a module
    for (int mi = 0; mi < nmod; mi++)
        if (distance(mx, my, mod[mi].x + tw(mod[mi].type) - 15, mod[mi].y + 4) < 5) return mi;
    return -1;
}
int module_at(int mx, int my) {  // topmost module whose body contains (mx,my)
    int r = -1;
    for (int mi = 0; mi < nmod; mi++)
        if (mx >= mod[mi].x && mx < mod[mi].x + tw(mod[mi].type) && my >= mod[mi].y && my < mod[mi].y + th(mod[mi].type)) r = mi;
    return r;
}
int handle_at(int mx, int my) {  // the title-bar drag handle (top strip, minus the ?/x corner)
    for (int mi = 0; mi < nmod; mi++)
        if (mx >= mod[mi].x && mx < mod[mi].x + tw(mod[mi].type) - 18 && my >= mod[mi].y && my < mod[mi].y + 9) return mi;
    return -1;
}
void delete_mod(int mi) {
    if ((mod[mi].type == MOD_VOICE || mod[mi].type == MOD_MACRO) && (int)mod[mi].state[1] > 0) note_off((int)mod[mi].state[1]);   // don't leave a stuck note
    for (int c = ncable - 1; c >= 0; c--) if (cable[c].sm == mi || cable[c].dm == mi) remove_cable(c);
    for (int k = mi; k < nmod - 1; k++) mod[k] = mod[k + 1];
    nmod--;
    for (int c = 0; c < ncable; c++) { if (cable[c].sm > mi) cable[c].sm--; if (cable[c].dm > mi) cable[c].dm--; }
}

void meter(int x, int y, int w, int h, float v, int col) {
    rectfill(x, y, w, h, CLR_BLACK);
    int fill = (int)(clamp(v, 0, 1) * h);
    rectfill(x, y + h - fill, w, fill, col);
}

float knob_dial(int id, int cx, int cy, float v, float lo, float hi, const char *name, const char *val) {
    float hit_r = zoom < 1.0f ? 7.0f / zoom : 7.0f;
    if (palette_drag < 0 && !panning && help_type < 0 && drag_mod < 0 && !preset_open && mouse_pressed(MOUSE_LEFT) && distance(wmx, wmy, cx, cy) < hit_r)
        { held_knob = id; drag_y = mouse_y(); drag_x = mouse_x(); }
    if (held_knob == id && mouse_down(MOUSE_LEFT)) {
        float dy = drag_y - mouse_y();   // up = positive = increase (coarse)
        float dx = mouse_x() - drag_x;  // right = positive = increase (fine)
        v = clamp(v + dy * (hi - lo) / 80.0f + dx * (hi - lo) / 600.0f, lo, hi);
        drag_y = mouse_y();
        drag_x = mouse_x();
    }
    bool hot = held_knob == id || distance(wmx, wmy, cx, cy) < hit_r;
    circfill(cx, cy, 5, hot ? CLR_WHITE : CLR_MEDIUM_GREY);
    circfill(cx, cy, 3, CLR_DARKER_GREY);
    float a = 135.0f + clamp((v - lo) / (hi - lo), 0, 1) * 270.0f;
    int nx = cx + (int)(cos_deg(a) * 3), ny = cy + (int)(sin_deg(a) * 3);
    rectfill(nx, ny, 2, 2, CLR_WHITE);
    print(name, cx - text_width(name) / 2, cy + 7, near_col(cx, cy));
    if (hot) print(val, cx - text_width(val) / 2, cy + 13, CLR_WHITE);
    return v;
}

// per-type extra visuals (meters, LEDs, dots) drawn after the generic frame
void draw_extras(int mi) {
    Module *m = &mod[mi]; int x = m->x, y = m->y; int W = TYPES[m->type].cw * CELL, cx = x + W / 2;
    switch (m->type) {
        case MOD_CLOCK: circfill(x + 12, y + 14, 3, m->state[2] < 5 ? CLR_WHITE : CLR_DARK_ORANGE); break;   // per-clock step flash
        case MOD_LFO:   meter(x + W - 14, y + 16, 6, 36, m->jackval[0], CLR_PINK); break;
        case MOD_SH:    meter(x + 10, y + 18, 6, 34, m->state[1], CLR_YELLOW); print(str("%d%%", (int)(m->state[1] * 99)), x + 26, y + 34, CLR_DARK_GREY); break;
        case MOD_QUANT: print(NOTES[((int)m->jackval[1]) % 12], cx - text_width(NOTES[((int)m->jackval[1]) % 12]) / 2, y + 44, CLR_WHITE); break;
        case MOD_VOICE: if (m->state[3] < 8) circfill(cx, y + 14, 5 - (int)m->state[3] / 2, CLR_LIGHT_PEACH); break;   // trigger pulse
        case MOD_EUCLID: {
            int hits = (int)m->param[0], steps = (int)m->param[1];
            for (int s = 0; s < steps; s++) {
                int dx = x + 6 + (int)(s * ((W - 12.0f) / steps));
                bool on = euclid(hits, steps, s);
                circfill(dx, y + 48, on ? 2 : 1, on ? CLR_RED : CLR_DARK_GREY);
                if (s == (((int)m->state[1] % steps) + steps) % steps && m->state[2] < 6) circ(dx, y + 48, 3, CLR_WHITE);
            }
            break; }
        case MOD_ENV:   meter(x + 2, y + 20, 4, 30, m->state[3], CLR_MEDIUM_GREEN); break;
        case MOD_DRUM:
            circfill(x + 8,  y + 20, 5, m->state[3] < 5 ? CLR_WHITE : CLR_DARK_RED);
            circfill(x + 24, y + 20, 4, m->state[4] < 5 ? CLR_WHITE : CLR_BROWN);
            circfill(x + 40, y + 20, 3, m->state[5] < 5 ? CLR_WHITE : CLR_DARK_GREY);
            print("kik sn hat", x + 4, y + 32, CLR_DARK_GREY);
            break;
        case MOD_SCOPE: {   // moving trace of the recorded cv history
            int head = (int)m->state[0];
            rectfill(x + 3, y + 13, W - 6, 20, CLR_BLACK);
            for (int i = 0; i < 20; i++) {
                int idx = (head - i + 20) % 20;
                pset(x + W - 5 - i * 3, y + 31 - (int)(m->state[2 + idx] * 16), CLR_LIME_GREEN);
            }
            break; }
        case MOD_TURING: {  // the looping ring: dot brightness = value, current head ringed
            int len = (int)m->param[1]; if (len < 2) len = 2; if (len > 16) len = 16;
            for (int i = 0; i < len; i++) {
                int dx = x + 4 + (int)(i * ((W - 8.0f) / len));
                circfill(dx, y + 48, 2, m->state[2 + i] > 0.5f ? CLR_TRUE_BLUE : CLR_DARKER_GREY);
                if (i == (int)m->state[1]) circ(dx, y + 48, 3, CLR_WHITE);
            }
            break; }
        case MOD_GRIDS:     // 3 drum LEDs flashing on their hits — aligned with k/s/h jacks
            circfill(x + 22, y + 44, 4, m->state[2] < 5 ? CLR_WHITE : CLR_DARK_RED);
            circfill(x + 40, y + 44, 3, m->state[3] < 5 ? CLR_WHITE : CLR_BROWN);
            circfill(x + 58, y + 44, 2, m->state[4] < 5 ? CLR_WHITE : CLR_DARK_GREY);
            break;
        case MOD_MARBLES:   // gate flash + cv meter
            circfill(x + 26, y + 56, 4, m->state[3] < 5 ? CLR_WHITE : CLR_DARK_GREEN);
            meter(x + 30, y + 30, 6, 30, m->state[1], CLR_LIME_GREEN);
            break;
        case MOD_MATHS:     // level bar + EOC flash
            rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK);
            rectfill(x + 8, y + 46, (int)(clamp(m->state[3], 0, 1) * (W - 16)), 3, CLR_BLUE_GREEN);
            if (m->state[4] < 5) circfill(x + 46, y + 40, 3, CLR_WHITE);
            break;
        case MOD_SEQ: {   // 8 position dots + mini bars showing step values
            int cur = ((int)m->state[0]) % 8;
            for (int s = 0; s < 8; s++) {
                int ddx = x + 4 + s * 9;
                int bh  = (int)(m->param[s] * 14);
                rectfill(ddx - 2, y + 62 - bh, 4, bh, s == cur ? CLR_PEACH : CLR_DARKER_GREY);
                circfill(ddx, y + 64, s == cur && m->state[2] < 8 ? 3 : 2,
                         s == cur ? CLR_WHITE : CLR_DARK_GREY);
            }
            break; }
        case MOD_VIBRATO:
            circfill(cx, y + 14, 3, m->jackval[1] > 0.5f ? CLR_DARK_BLUE : CLR_DARKER_GREY);
            break;
        case MOD_CHANCE:
            circfill(cx, y + 18, 4, m->state[1] < 6 ? CLR_WHITE : CLR_BROWN);
            break;
        case MOD_MACRO: {   // trigger LED + live har/tmb/mor bars — CV modulation made visible
            circfill(x + 10, y + 17, 4, m->state[3] < 6 ? CLR_WHITE : CLR_DARK_PURPLE);
            for (int b = 0; b < 3; b++) {
                rectfill(x + 22, y + 13 + b * 4, 42, 2, CLR_BLACK);
                rectfill(x + 22, y + 13 + b * 4, (int)(clamp(m->state[5 + b], 0, 1) * 42), 2, CLR_PINK);
            }
            break; }
        case MOD_XPOSE: {   // the snapped octave reads at a glance (it's a switch, not a sweep)
            const char *s = knob_str(FMT_OCT, m->param[0]);
            print(s, cx - text_width(s) / 2, y + 14, CLR_WHITE);
            break; }
        case MOD_MIX:       // the summed cv, live — watch two modulators braid into one
            meter(x + 45, y + 12, 6, 10, m->jackval[2], CLR_WHITE);
            break;
        case MOD_CMP:       // gate LED — high while the cv sits above the threshold
            circfill(cx, y + 16, 4, m->jackval[1] > 0.5f ? CLR_GREEN : CLR_DARKER_GREY);
            break;
        case MOD_DIV: {     // the division readout flashes white each time it lets a pulse through
            const char *s = knob_str(FMT_DIV, m->param[0]);
            print(s, cx - text_width(s) / 2, y + 14, m->state[2] < 6 ? CLR_WHITE : CLR_MEDIUM_GREY);
            break; }
        case MOD_ADSR:      // envelope level, live — watch it hold at sus while the gate is up
            meter(x + 53, y + 16, 5, 48, m->state[2], CLR_DARK_RED);
            break;
        case MOD_KEYS: {    // 7 white keys; lit while held
            const char KK[7] = { 'A','S','D','F','G','H','J' }; const int OFF[7] = { 0,2,4,5,7,9,11 };
            for (int i = 0; i < 7; i++) {
                int kx = x + 3 + i * 10;
                bool dn = key(KK[i]) || (m->state[0] > 0.5f && (int)m->state[1] == 48 + OFF[i]);
                rectfill(kx, y + 14, 9, 26, dn ? CLR_YELLOW : CLR_WHITE);
                rect(kx, y + 14, 9, 26, CLR_DARK_GREY);
                print(str("%c", KK[i]), kx + 2, y + 42, CLR_DARK_GREY);
            }
            break; }
        // FX modules: a wet/amount bar (knob + live cv) so modulation is visible at a glance
        case MOD_RVB:   { float w = clamp(m->param[RVK_MIX] + read_in(mi, 0), 0, 1);
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(w * (W - 16)), 3, CLR_INDIGO); break; }
        case MOD_ECHO:  { float w = clamp(m->param[ECK_MIX] + read_in(mi, 0), 0, 1);
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(w * (W - 16)), 3, CLR_BLUE_GREEN); break; }
        case MOD_DRIVE: { float w = clamp(m->param[DRK_AMT] + read_in(mi, 0), 0, 1);
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(w * (W - 16)), 3, CLR_DARK_ORANGE); break; }
        case MOD_CRUSH: { float w = clamp(m->param[CRK_MIX] + read_in(mi, 0), 0, 1);
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(w * (W - 16)), 3, CLR_MAUVE); break; }
        case MOD_SAT:   { float w = clamp(m->param[SATK_AMT] + read_in(mi, 0), 0, 1);
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(w * (W - 16)), 3, CLR_ORANGE); break; }
        case MOD_WAH:   { float w = clamp(m->param[WHK_MIX] + read_in(mi, 0), 0, 1);
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(w * (W - 16)), 3, CLR_LIME_GREEN); break; }
        case MOD_VOWEL: { float v = clamp(m->param[VWK_VOW] + read_in(mi, 0), 0, 1);   // bar tracks the vowel (cv sweep)
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(v * (W - 16)), 3, CLR_DARK_PEACH); break; }
        case MOD_FILT:  { float c = clamp((m->param[FLK_CUT] + read_in(mi, 0) * 8000.0f) / 13000.0f, 0, 1);   // bar tracks the cutoff sweep
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(c * (W - 16)), 3, CLR_TRUE_BLUE); break; }
        case MOD_GRAINS:{ float w = clamp(m->param[GRK_MIX] + read_in(mi, 0), 0, 1);
                          rectfill(x + 8, y + 46, W - 16, 3, CLR_BLACK); rectfill(x + 8, y + 46, (int)(w * (W - 16)), 3, CLR_LIGHT_PEACH); break; }
        case MOD_TIDES: meter(x + W - 12, y + 14, 6, 34, m->jackval[1], CLR_MEDIUM_GREEN);   // live output level
                        if (m->state[3] < 5) circfill(x + 50, y + 50, 3, CLR_WHITE);          // eoc flash
                        break;
    }
}

void draw_module(int mi) {
    Module *m = &mod[mi];
    ModType *t = &TYPES[m->type];
    int x = m->x, y = m->y, W = t->cw * CELL, H = t->ch * CELL;
    rectfill(x, y, W, H, CLR_DARKER_PURPLE);
    bool hhot = drag_mod == mi || (wmx >= x && wmx < x + W - 18 && wmy >= y && wmy < y + 9);
    rectfill(x, y, W, 9, hhot ? CLR_DARK_GREY : CLR_DARKER_GREY);   // title bar = drag handle (lights up on hover)
    rect(x, y, W, H, t->col);
    font(FONT_NORMAL);
    print(t->name, x + 3, y + 2, t->col);
    font(FONT_SMALL);
    print("?", x + W - 16, y + 2, distance(wmx, wmy, x + W - 15, y + 4) < 5 ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    print("x", x + W - 8, y + 2, distance(wmx, wmy, x + W - 6, y + 4) < 5 ? CLR_RED : CLR_DARK_GREY);

    draw_extras(mi);

    for (int k = 0; k < t->nknob; k++) {
        KnobDef kd = t->knob[k];
        m->param[k] = knob_dial(mi * 16 + k + 1, x + kd.dx, y + kd.dy, m->param[k], kd.lo, kd.hi, kd.label, knob_str(kd.fmt, m->param[k]));
    }
    for (int j = 0; j < t->njack; j++) {
        JackDef jd = t->jack[j];
        int jx = x + jd.dx, jy = y + jd.dy;
        bool hint = m->type == MOD_VOICE && j == 5 && cable_into(mi, j) < 0;
        int pc = blink(18) ? CLR_LIGHT_PEACH : CLR_DARK_PEACH;
        const char *lbl = hint ? "env" : jd.label;
        print(lbl, jx - text_width(lbl) / 2, jy + 5, hint ? pc : near_col(jx, jy));
    }
}

void edit_cables(int mx, int my) {
    if (mouse_pressed(MOUSE_LEFT)) {
        int h = jack_at(mx, my);
        if (h >= 0) {
            int mi = h / 8, j = h % 8;
            if (TYPES[mod[mi].type].jack[j].out) drag_jack = h;
            else { int c = cable_into(mi, j); if (c >= 0) { drag_jack = cable[c].sm * 8 + cable[c].sj; remove_cable(c); } }
        }
    }
    if (mouse_released(MOUSE_LEFT)) {
        if (drag_jack >= 0) {
            int h = jack_at(mx, my), sm = drag_jack / 8, sj = drag_jack % 8;
            if (h >= 0) {
                int dm = h / 8, dj = h % 8;
                if (!TYPES[mod[dm].type].jack[dj].out && TYPES[mod[dm].type].jack[dj].type == TYPES[mod[sm].type].jack[sj].type)
                    add_cable(sm, sj, dm, dj);
            }
        }
        drag_jack = -1;
    }
    if (mouse_pressed(MOUSE_RIGHT)) {
        int h = jack_at(mx, my);
        if (h >= 0) { int mi = h / 8, j = h % 8; for (int c = ncable - 1; c >= 0; c--) if ((cable[c].sm == mi && cable[c].sj == j) || (cable[c].dm == mi && cable[c].dj == j)) remove_cable(c); }
    }
}

void draw_cable_between(int sx, int sy, int dx, int dy, int c, bool pulse) {
    int mx = (sx + dx) / 2, my = max(sy, dy) + 14;
    // Draw the curve as a chain of tiny filled rects, NOT bezier()'s 1px GL line: only
    // rectfill (DrawRectangle) scales solidly under the camera, so the cable thickens with
    // zoom (1px @1x -> a solid stroke @2x/3x) instead of staying a hairline. (line/circfill
    // would stay 1px or break into dots when zoomed in.)
    int n = (int)(distance(sx, sy, mx, my) + distance(mx, my, dx, dy));
    if (n < 8) n = 8; else if (n > 160) n = 160;
    int px = sx, py = sy;
    for (int i = 1; i <= n; i++) {
        float t = i / (float)n, u = 1.0f - t;
        int nx = (int)(u * u * sx + 2 * u * t * mx + t * t * dx);
        int ny = (int)(u * u * sy + 2 * u * t * my + t * t * dy);
        int rx = px < nx ? px : nx, ry = py < ny ? py : ny;
        rectfill(rx, ry, (px < nx ? nx - px : px - nx) + 1, (py < ny ? ny - py : py - ny) + 1, c);
        px = nx; py = ny;
    }
    if (pulse && tick_flash < 10) {
        float t = tick_flash / 10.0f, u = 1.0f - t;
        int hx = (int)(u * u * sx + 2 * u * t * mx + t * t * dx), hy = (int)(u * u * sy + 2 * u * t * my + t * t * dy);
        int pd = zoom < 1.0f ? (int)(1.0f / zoom) : 1;
        rectfill(hx - pd, hy - pd, pd * 2 + 1, pd * 2 + 1, CLR_WHITE);
    }
}

// world coord → screen pixel (matches camera_ex centred at SCREEN_W/2, SCREEN_H/2)
int wsx(float wx) { return (int)((wx - cam_x - SCREEN_W * 0.5f) * zoom + SCREEN_W * 0.5f); }
int wsy(float wy) { return (int)((wy - cam_y - SCREEN_H * 0.5f) * zoom + SCREEN_H * 0.5f); }

// draws jacks and hover tooltip in screen space — called after camera(0,0)
void draw_jacks_ss(void) {
    int jr = 3;
    for (int i = 0; i < nmod; i++) {
        Module *m = &mod[i];
        ModType *t = &TYPES[m->type];
        for (int j = 0; j < t->njack; j++) {
            JackDef jd = t->jack[j];
            int sx = wsx(m->x + jd.dx), sy = wsy(m->y + jd.dy);
            if (sx < SIDEBAR_W || sx >= SCREEN_W || sy < 0 || sy >= SCREEN_H) continue;
            float v = jd.out ? m->jackval[j] : read_in(i, j);
            bool lit = jd.type == 0 && v > 0.5f;
            int c = sig_col(jd.type);
            bool hint = m->type == MOD_VOICE && j == 5 && cable_into(i, j) < 0;
            int pc = blink(18) ? CLR_LIGHT_PEACH : CLR_DARK_PEACH;
            if (jd.out) {
                circfill(sx, sy, jr, c);
                circfill(sx, sy, jr - 2, CLR_DARKER_GREY);
                if (lit) circfill(sx, sy, 1, CLR_WHITE);
            } else {
                circfill(sx, sy, jr, hint ? pc : (lit ? CLR_WHITE : c));
            }
            if (hint) circ(sx, sy, jr + 2, pc);
        }
    }
    // compatible-jack highlight rings while dragging a cable
    if (drag_jack >= 0) {
        int sm = drag_jack / 8, sj = drag_jack % 8;
        for (int mi = 0; mi < nmod; mi++)
            for (int j = 0; j < TYPES[mod[mi].type].njack; j++)
                if (!TYPES[mod[mi].type].jack[j].out && TYPES[mod[mi].type].jack[j].type == TYPES[mod[sm].type].jack[sj].type) {
                    int sx = wsx(jackpos_x(mi, j)), sy = wsy(jackpos_y(mi, j));
                    if (sx >= SIDEBAR_W && sx < SCREEN_W && sy >= 0 && sy < SCREEN_H)
                        circ(sx, sy, jr + 2, CLR_WHITE);
                }
    }
    // screen-space tooltip when zoomed out — unreadable world text replaced with a legible label
    if (zoom < 1.0f && !preset_open && help_type < 0 && mouse_x() >= SIDEBAR_W) {
        const char *tip = 0;
        int jh = jack_at(wmx, wmy);
        if (jh >= 0) {
            int mi = jh / 8, j2 = jh % 8;
            JackDef jd = TYPES[mod[mi].type].jack[j2];
            const char *sig = jd.type == 0 ? "gate" : jd.type == 1 ? "pit" : jd.type == 3 ? "aud" : "cv";
            tip = str("%s %s %s", TYPES[mod[mi].type].name, jd.label, sig);
        } else {
            int kk = -1, km = -1;
            for (int mi = 0; mi < nmod && km < 0; mi++)
                for (int k = 0; k < TYPES[mod[mi].type].nknob; k++)
                    if (distance(wmx, wmy, mod[mi].x + TYPES[mod[mi].type].knob[k].dx, mod[mi].y + TYPES[mod[mi].type].knob[k].dy) < 7.0f / zoom)
                        { km = mi; kk = k; break; }
            if (km >= 0) {
                KnobDef kd = TYPES[mod[km].type].knob[kk];
                tip = str("%s %s: %s", TYPES[mod[km].type].name, kd.label, knob_str(kd.fmt, mod[km].param[kk]));
            } else {
                int mm = module_at(wmx, wmy);
                if (mm >= 0) tip = TYPES[mod[mm].type].name;
            }
        }
        if (tip) {
            font(FONT_SMALL);
            int tw = text_width(tip) + 6, tx = mouse_x() + 8, ty = mouse_y() - 14;
            if (tx + tw > SCREEN_W) tx = mouse_x() - tw - 2;
            if (ty < 14) ty = mouse_y() + 6;
            rectfill(tx - 2, ty - 1, tw, 8, CLR_BLACK);
            rect(tx - 2, ty - 1, tw, 8, CLR_DARKER_GREY);
            print(tip, tx, ty, CLR_WHITE);
        }
    }
}

// step to the next crisp zoom stop, keeping the point under (ax,ay) fixed —
// shared by the mouse wheel and the touch pinch
void zoom_step(int dir, float ax, float ay) {
    int zi = 0; for (int i = 0; i < NZOOM; i++) if (zoom >= ZOOMS[i] - 0.01f) zi = i;
    zi = (int)clamp(zi + dir, 0, NZOOM - 1);
    float z0 = zoom, z1 = ZOOMS[zi];
    if (z1 == z0) return;
    cam_x += (ax - SCREEN_W / 2.0f) * (1.0f / z0 - 1.0f / z1);
    cam_y += (ay - SCREEN_H / 2.0f) * (1.0f / z0 - 1.0f / z1);
    zoom = z1;
}

void draw(void) {
    // ---- pan (left-drag empty space) + zoom (wheel / pinch), in screen-delta terms ----
    if (panning && mouse_down(MOUSE_LEFT)) { cam_x -= (mouse_x() - pan_px) / zoom; cam_y -= (mouse_y() - pan_py) / zoom; }
    pan_px = mouse_x(); pan_py = mouse_y();
    if (!mouse_down(MOUSE_LEFT)) { panning = 0; held_knob = 0; }
    bool over_side = mouse_x() < SIDEBAR_W;
    float wh = mouse_wheel();
    int maxsc = 14 + NTYPE * 16 - SCREEN_H + 6; if (maxsc < 0) maxsc = 0;
    if (wh != 0) {
        if (over_side) palette_scroll = (int)clamp(palette_scroll - wh * 16, 0, maxsc);   // wheel over the palette → scroll the list
        else           zoom_step(wh > 0 ? 1 : -1, mouse_x(), mouse_y());                  // wheel over the canvas → zoom (point under cursor stays fixed)
    }
    // touch: a second finger means PINCH — cancel the one-finger gestures so the zoom
    // doesn't also pan/turn/drag, accumulate the per-frame pinch factor, and step one
    // crisp zoom stop each time it crosses ±~30%, anchored at the pinch midpoint.
    float pz = pinch_scale();   // call every frame — it tracks the finger pair internally
    if (touch_count() >= 2) {
        panning = 0; held_knob = 0; drag_mod = -1; palette_drag = -1; palette_pend = -1; pal_scrolling = 0;
        pinch_acc *= pz;
        if (pinch_acc > 1.30f || pinch_acc < 0.77f) {
            zoom_step(pinch_acc > 1.0f ? 1 : -1, (touch_x(0) + touch_x(1)) * 0.5f, (touch_y(0) + touch_y(1)) * 0.5f);
            pinch_acc = 1.0f;
        }
    } else pinch_acc = 1.0f;

    cls(CLR_DARKER_BLUE);

    // ===== STAGE — world space (pan/zoom) =====
    camera_ex((int)cam_x, (int)cam_y, zoom, 0);
    wmx = mouse_world_x(); wmy = mouse_world_y();

    if (help_type >= 0) {
        if (mouse_pressed(MOUSE_LEFT)) help_type = -1;          // any click dismisses the help panel
    } else if (!over_side && !preset_open && mouse_y() >= 14 && palette_drag < 0 && !panning && drag_mod < 0 && mouse_pressed(MOUSE_LEFT)) {
        int q = qmark_at(wmx, wmy), dm = delx_at(wmx, wmy);
        if (q >= 0) help_type = mod[q].type;
        else if (dm >= 0) delete_mod(dm);
        else if (jack_at(wmx, wmy) < 0 && knob_at(wmx, wmy) < 0) {
            int hm = handle_at(wmx, wmy);                       // grab the title-bar handle to move
            if (hm >= 0) { drag_mod = hm; grab_dx = wmx - mod[hm].x; grab_dy = wmy - mod[hm].y; }
            else if (module_at(wmx, wmy) < 0) { panning = 1; pan_px = mouse_x(); pan_py = mouse_y(); }  // empty canvas → pan
        }
    }
    if (drag_mod >= 0) {   // move the grabbed module (snaps to the cell grid; its cables follow)
        if (mouse_down(MOUSE_LEFT)) { mod[drag_mod].x = snap12(wmx - grab_dx); mod[drag_mod].y = snap12(wmy - grab_dy); }
        else drag_mod = -1;
    }
    if (help_type < 0 && !preset_open && !over_side && palette_drag < 0 && !panning && drag_mod < 0) edit_cables(wmx, wmy);

    font(FONT_SMALL);   // module labels (titles, ? / x, jacks, knobs) use the small font

    if (palette_drag >= 0) {   // faint 12px cell grid, shown only while placing a module
        int wl = (int)((0 - 160) / zoom + cam_x + 160), wr = (int)((SCREEN_W - 160) / zoom + cam_x + 160);
        int wt = (int)((0 - 100) / zoom + cam_y + 100), wb = (int)((SCREEN_H - 100) / zoom + cam_y + 100);
        for (int gx = snap12(wl); gx <= wr; gx += CELL) line(gx, wt, gx, wb, CLR_DARKER_GREY);
        for (int gy = snap12(wt); gy <= wb; gy += CELL) line(wl, gy, wr, gy, CLR_DARKER_GREY);
    }

    for (int i = 0; i < nmod; i++) draw_module(i);

    for (int c = 0; c < ncable; c++)
        draw_cable_between(jackpos_x(cable[c].sm, cable[c].sj), jackpos_y(cable[c].sm, cable[c].sj),
                           jackpos_x(cable[c].dm, cable[c].dj), jackpos_y(cable[c].dm, cable[c].dj),
                           sig_col(TYPES[mod[cable[c].sm].type].jack[cable[c].sj].type), true);

    // highlight cables connected to the hovered jack — redraw on top in white so they stand out
    int hj = jack_at(wmx, wmy);
    if (hj >= 0) {
        int hmi = hj / 8, hjj = hj % 8;
        for (int c = 0; c < ncable; c++)
            if ((cable[c].sm == hmi && cable[c].sj == hjj) || (cable[c].dm == hmi && cable[c].dj == hjj))
                draw_cable_between(jackpos_x(cable[c].sm, cable[c].sj), jackpos_y(cable[c].sm, cable[c].sj),
                                   jackpos_x(cable[c].dm, cable[c].dj), jackpos_y(cable[c].dm, cable[c].dj),
                                   CLR_WHITE, false);
    }

    if (drag_jack >= 0) {
        int sm = drag_jack / 8, sj = drag_jack % 8, c = sig_col(TYPES[mod[sm].type].jack[sj].type);
        draw_cable_between(jackpos_x(sm, sj), jackpos_y(sm, sj), wmx, wmy, c, false);
    }
    if (palette_drag >= 0) {   // ghost snaps to the cell grid where it'll land
        int W = tw(palette_drag), H = th(palette_drag);
        int gx = snap12(wmx - W / 2), gy = snap12(wmy - H / 2);
        rect(gx, gy, W, H, TYPES[palette_drag].col);
        print(TYPES[palette_drag].name, gx + 3, gy + 2, TYPES[palette_drag].col);
    }

    // ===== SCREEN SPACE — palette sidebar + HUD =====
    camera(0, 0);
    draw_jacks_ss();
    rectfill(0, 0, SIDEBAR_W, SCREEN_H, CLR_BROWNISH_BLACK);
    font(FONT_SMALL);
    print("ADD", 8, 4, CLR_MEDIUM_GREY);
    for (int t = 0; t < NTYPE; t++) {
        int by = 14 + t * 16 - palette_scroll;
        if (by < 12 || by > SCREEN_H - 4) continue;   // scrolled out of view
        bool hot = mouse_x() < SIDEBAR_W && mouse_y() >= by && mouse_y() < by + 15;
        rectfill(3, by, SIDEBAR_W - 6, 14, CLR_DARKER_PURPLE);
        rect(3, by, SIDEBAR_W - 6, 14, hot ? CLR_WHITE : TYPES[t].col);
        print(TYPES[t].name, 6, by + 4, TYPES[t].col);
        if (hot && mouse_pressed(MOUSE_LEFT) && palette_drag < 0 && palette_pend < 0 && !pal_scrolling && nmod < MAX_MOD && help_type < 0 && !preset_open)
            { palette_pend = t; pend_my = mouse_y(); held_knob = 0; panning = 0; }   // deferred: not a drag yet (see below)
    }
    // pressing empty sidebar space (no item caught it) → drag-scroll the list directly
    if (mouse_pressed(MOUSE_LEFT) && over_side && palette_pend < 0 && palette_drag < 0 && !pal_scrolling && help_type < 0 && !preset_open)
        { pal_scrolling = 1; pend_my = mouse_y(); held_knob = 0; panning = 0; }
    // deferred pick-up: a press on a palette item isn't a module-drag yet — drag RIGHT onto
    // the canvas to place it, drag UP/DOWN to scroll the list (the touch path: no wheel).
    if (palette_pend >= 0) {
        int dy = mouse_y() - pend_my;
        if      (!mouse_down(MOUSE_LEFT))  palette_pend = -1;
        else if (mouse_x() >= SIDEBAR_W) { palette_drag = palette_pend; palette_pend = -1; }
        else if (dy > 8 || dy < -8)      { pal_scrolling = 1; pend_my = mouse_y(); palette_pend = -1; }
    }
    if (pal_scrolling) {   // list follows the finger/cursor
        if (mouse_down(MOUSE_LEFT)) { palette_scroll = (int)clamp(palette_scroll - (mouse_y() - pend_my), 0, maxsc); pend_my = mouse_y(); }
        else pal_scrolling = 0;
    }
    if (mouse_released(MOUSE_LEFT) && palette_drag >= 0) {
        if (mouse_x() >= SIDEBAR_W && nmod < MAX_MOD) spawn(palette_drag, wmx - tw(palette_drag) / 2, wmy - th(palette_drag) / 2);
        palette_drag = -1;
    }

    font(FONT_NORMAL);
    print("MODRACK", SIDEBAR_W + 4, 3, CLR_WHITE);
    font(FONT_SMALL);

    // PRESETS dropdown — pick a ready-made patch
    int pbx = SIDEBAR_W + 64, pbw = 56;
    bool pbh = mouse_x() >= pbx && mouse_x() < pbx + pbw && mouse_y() < 13;
    rectfill(pbx, 2, pbw, 11, pbh || preset_open ? CLR_BLUE : CLR_DARKER_PURPLE);
    rect(pbx, 2, pbw, 11, CLR_LIGHT_GREY);
    print("PRESETS", pbx + 5, 4, CLR_WHITE);
    if (pbh && mouse_pressed(MOUSE_LEFT)) preset_open = !preset_open;
    if (preset_open) {
        int ncol = 3;                          // 3 columns so the (growing) list fits on screen
        int rows = (NPRESET + ncol - 1) / ncol;
        for (int i = 0; i < NPRESET; i++) {
            int col = i / rows, row = i % rows;
            int ix = pbx + col * pbw, iy = 14 + row * 12;
            bool ih = mouse_x() >= ix && mouse_x() < ix + pbw && mouse_y() >= iy && mouse_y() < iy + 12;
            rectfill(ix, iy, pbw, 12, ih ? CLR_DARK_GREY : CLR_BLACK);
            rect(ix, iy, pbw, 12, CLR_DARKER_GREY);
            print(PRESET_NAMES[i], ix + 4, iy + 4, CLR_LIGHT_GREY);
            if (ih && mouse_pressed(MOUSE_LEFT)) { PRESET_FN[i](); preset_open = 0; }
        }
        bool inside = mouse_x() >= pbx && mouse_x() < pbx + pbw * ncol && mouse_y() < 14 + rows * 12;
        if (mouse_pressed(MOUSE_LEFT) && !inside) preset_open = 0;
    }
    if (msg_flash > 0) print(msg, pbx + pbw + 8, 5, CLR_LIGHT_PEACH);

    // 1:1 zoom-reset button (top-right)
    int zbx = SCREEN_W - 30; bool zbh = mouse_x() >= zbx && mouse_y() < 14;
    rectfill(zbx, 2, 28, 11, zbh ? CLR_BLUE : CLR_DARKER_PURPLE);
    rect(zbx, 2, 28, 11, CLR_LIGHT_GREY);
    print("1:1", zbx + 7, 4, CLR_WHITE);
    if (zbh && mouse_pressed(MOUSE_LEFT) && help_type < 0 && !preset_open) { zoom = 1.0f; cam_x = -32; cam_y = -2; }
    print("patch: out->in   rclick clears   ? help   x del   S/L/R", SIDEBAR_W + 4, 192, CLR_DARKER_GREY);

    if (help_type >= 0) {   // module help panel (modal; click to close)
        int px = 56, py = 60, pw = 212, ph = 66;
        rectfill(px, py, pw, ph, CLR_BLACK);
        rect(px, py, pw, ph, TYPES[help_type].col);
        font(FONT_NORMAL); print(TYPES[help_type].name, px + 8, py + 6, TYPES[help_type].col); font(FONT_SMALL);
        for (int l = 0; l < 3; l++) if (HELP[help_type][l] && HELP[help_type][l][0]) print(HELP[help_type][l], px + 8, py + 22 + l * 10, CLR_LIGHT_GREY);
        print("click to close", px + pw - 58, py + ph - 9, CLR_MEDIUM_GREY);
    }

    font(FONT_NORMAL);

    // pixel cursor (screen space — camera was reset above): a closed fist while
    // dragging (knob / cable / module / canvas), a pointing finger when hovering
    // something grabbable (knob, jack, module handle), else the plain arrow.
    int grabbing = held_knob || drag_jack >= 0 || drag_mod >= 0 || panning || palette_drag >= 0;
    int hover_grab = !over_side && (knob_at(wmx, wmy) >= 0 || jack_at(wmx, wmy) >= 0 || handle_at(wmx, wmy) >= 0);
    cursor_draw(grabbing ? CUR_GRAB : hover_grab ? CUR_HAND : CUR_ARROW);
}
