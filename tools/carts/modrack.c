#include "studio.h"
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
       MOD_SEQ, MOD_VIBRATO, MOD_CHANCE, NTYPE };
enum { FMT_INT, FMT_F1, FMT_SCALE, FMT_NOTE, FMT_MS, FMT_LOGIC, FMT_WAVE, FMT_FILTER, FMT_DEST };

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
                   1, {{"bpm",60,240,112,30,28,FMT_INT}} },
    [MOD_LFO]   = { "LFO", CLR_PINK, 4, 6, 1, {{2,true,24,60,"cv"}},
                   1, {{"rate",0.1f,8,0.37f,24,28,FMT_F1}} },
    [MOD_SH]    = { "S&H", CLR_YELLOW, 4, 6, 3, {{2,false,8,60,"in"},{0,false,24,60,"clk"},{2,true,40,60,"cv"}}, 0, {} },
    [MOD_QUANT] = { "QUANT", CLR_GREEN, 5, 6, 2, {{2,false,18,60,"in"},{1,true,42,60,"pit"}},
                   2, {{"scl",0,5.99f,SCALE_PENTA,18,28,FMT_SCALE},{"root",0,11.99f,0,42,28,FMT_NOTE}} },
    [MOD_VOICE] = { "VOICE", CLR_BLUE, 6, 8, 7, {{0,false,3,84,"g"},{1,false,13,84,"p"},{2,false,23,84,"f"},{2,false,33,84,"r"},{2,false,43,84,"w"},{2,false,53,84,"a"},{0,false,63,84,"vb"}},
                   7, {{"cut",200,2200,700,20,28,FMT_INT},{"res",0,15,6,52,28,FMT_INT},
                       {"pw",0.05f,0.95f,0.5f,12,46,FMT_F1},{"wav",0,8.99f,0,36,46,FMT_WAVE},{"flt",0,3.99f,0,58,46,FMT_FILTER},
                       {"fenv",0,3000,0,20,64,FMT_INT},{"penv",-48,48,0,52,64,FMT_INT}} },   // vb = VIBE patch point; a = amp/VCA CV; flt = filter mode
    [MOD_EUCLID]= { "EUCLID", CLR_RED, 5, 6, 2, {{0,false,18,60,"clk"},{0,true,42,60,"g"}},
                   2, {{"h",1,8.99f,4,18,26,FMT_INT},{"s",2,16.99f,8,42,26,FMT_INT}} },
    [MOD_ENV]   = { "ENV", CLR_MEDIUM_GREEN, 4, 6, 2, {{0,false,14,60,"g"},{2,true,34,60,"cv"}},
                   2, {{"atk",0.005f,0.5f,0.01f,14,32,FMT_MS},{"dec",0.02f,1,0.25f,34,32,FMT_MS}} },
    [MOD_DRUM]  = { "DRUM", CLR_DARK_ORANGE, 4, 5, 3, {{0,false,8,48,"k"},{0,false,24,48,"s"},{0,false,40,48,"h"}}, 0, {} },
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
                     3, {{"rate",0.5f,14,5.5f,8,28,FMT_F1},{"dpt",0,1,0.3f,24,28,FMT_F1},{"dst",0,2.99f,0,40,28,FMT_DEST}} },
    [MOD_CHANCE]  = { "CHANCE", CLR_BROWN, 4, 5, 2, {{0,false,14,48,"in"},{0,true,34,48,"out"}},
                     1, {{"prob",0,1,0.5f,24,28,FMT_F1}} },
};
int tw(int type) { return TYPES[type].cw * CELL; }   // module pixel width/height
int th(int type) { return TYPES[type].ch * CELL; }

// one-screen help per module kind (click the module's ? to show it)
const char *HELP[NTYPE][3] = {
    [MOD_CLOCK]  = { "Master clock. bpm knob sets the tempo.", "Gate outs: /1 every step, /2 half,", "/4 quarter. Patch into trigger inputs." },
    [MOD_LFO]    = { "Low-freq oscillator: a slow 0..1 wave.", "rate sets speed. Patch the cv out into", "any cv input to modulate it." },
    [MOD_SH]     = { "Sample & Hold. On each clk pulse it", "grabs the cv at 'in' and holds it until", "the next clk -> a stepped, random-ish cv." },
    [MOD_QUANT]  = { "Quantizer. Snaps any cv to the nearest", "note of a scale (scl/root) so it's always", "in key. cv in -> pitch out." },
    [MOD_VOICE]  = { "Voice. g=gate p=pitch; f/r/w/a CV =", "cutoff/res/pw/amp. Patch an ENV into 'a' for", "a percussive VCA. fenv/penv = filter+pitch punch." },
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
    [MOD_VIBRATO]= { "Audio-rate vibrato via note_lfo(). rate=Hz,", "dpt=depth, dst=pit/cut/pw. 'g' unpatched=", "always on; patched=gate-enable." },
    [MOD_CHANCE] = { "Gate filter: lets incoming gates through", "with prob chance (0=never 1=always).", "Thins patterns without changing the rhythm." },
};

// ── module instances + cables ──
typedef struct { int type, x, y; float param[8], state[24], jackval[4]; } Module;   // param[] up to 8 knobs; state[] holds SCOPE history
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

int  spawn(int type, int x, int y) {
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
}
void preset_acid(void) {         // euclid-gated squelch bass, slewed pitch + LFO wah
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), eu = spawn(MOD_EUCLID, bayx(1), bayy(1));
    int lf = spawn(MOD_LFO, bayx(2), bayy(2)), sh = spawn(MOD_SH, bayx(3), bayy(3));
    int sl = spawn(MOD_SLEW, bayx(4), bayy(4)), qt = spawn(MOD_QUANT, bayx(5), bayy(5)), vo = spawn(MOD_VOICE, bayx(6), bayy(6));
    mod[vo].param[0] = 420; mod[vo].param[1] = 11; mod[vo].param[3] = 1;   // low cutoff, squelchy res, square wave
    mod[qt].param[0] = SCALE_PENTA_MIN;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, vo, 0);
    add_cable(ck, 0, sh, 1); add_cable(lf, 0, sh, 0);
    add_cable(sh, 2, sl, 0); add_cable(sl, 1, qt, 0); add_cable(qt, 1, vo, 1);
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
    mod[vo].param[3] = 2;   // triangle — mellow, rounded; a warm played pluck distinct from the buzzier presets
    add_cable(kb, 0, vo, 0); add_cable(kb, 1, vo, 1); add_cable(kb, 0, en, 0); add_cable(en, 1, vo, 2);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}
void preset_pwmpad(void) {       // square voice with a slow LFO sweeping its pulse width (PWM)
    note_off_all(); nmod = 0; ncable = 0; palette_scroll = 0;
    int ck = spawn(MOD_CLOCK, bayx(0), bayy(0)), lf = spawn(MOD_LFO, bayx(1), bayy(1));
    int sh = spawn(MOD_SH, bayx(2), bayy(2)), qt = spawn(MOD_QUANT, bayx(3), bayy(3)), vo = spawn(MOD_VOICE, bayx(4), bayy(4));
    mod[ck].param[0] = 90; mod[lf].param[0] = 0.25f; mod[qt].param[0] = SCALE_PENTA_MIN;
    mod[vo].param[0] = 1100; mod[vo].param[1] = 3; mod[vo].param[2] = 0.5f; mod[vo].param[3] = 1;   // cut/res/pw + wav=square
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
    mod[vo].param[0] = 500;                                                   // low cutoff so the sweep opens it
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
    mod[vo].param[0] = 280; mod[vo].param[1] = 10; mod[vo].param[3] = 0;   // low cut, squelchy res, saw
    mod[vo].param[4] = 2400;                                              // FILTER ENV: snaps wide open on each note
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
    mod[vo].param[0] = 1500; mod[vo].param[3] = 1;   // bright, square
    mod[vo].param[4] = 700;                          // a touch of filter pluck
    mod[vo].param[5] = 12;                           // PITCH ENV: +12 st — each note zaps up an octave into the note
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
    mod[vo].param[0] = 480; mod[vo].param[1] = 3; mod[vo].param[3] = 3;   // sine, low-ish cut
    mod[vo].param[5] = 36;                                           // PITCH ENV: +36 st = the kick "donk"
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
    mod[vo].param[0] = 900; mod[vo].param[1] = 4; mod[vo].param[3] = 2;   // tri wave, open filter
    mod[vo].param[4] = 400;                                  // light filter blip on attack
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
    mod[vo].param[0] = 500; mod[vo].param[1] = 11; mod[vo].param[3] = 0;  // saw, high Q
    mod[vo].param[6] = 2;                                    // BAND-PASS
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
    mod[vo].param[0] = 800; mod[vo].param[1] = 13; mod[vo].param[3] = 1;   // sqr, sharp notch Q
    mod[vo].param[6] = 3;                                    // NOTCH
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
    mod[vo].param[0] = 650; mod[vo].param[1] = 6; mod[vo].param[3] = 0; mod[vo].param[4] = 900;
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
    mod[vo].param[0] = 1200; mod[vo].param[3] = 2;      // tri — smooth, vibrato clearly audible
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
    mod[vo].param[0] = 700; mod[vo].param[1] = 7; mod[vo].param[3] = 1; mod[vo].param[4] = 900;
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, ch, 0); add_cable(ch, 1, vo, 0);
    add_cable(ck, 0, tm, 0); add_cable(tm, 1, qt, 0); add_cable(qt, 1, vo, 1);
    add_cable(ck, 0, dr, 0); add_cable(ck, 0, dr, 2);
}

const char *PRESET_NAMES[] = { "Empty", "Generative", "Acid bass", "Beats", "Keys synth", "PWM pad", "Turing", "Grids beat", "Marbles", "Maths sweep", "Env pluck", "Zap lead", "Punch (VCA)", "Glide", "BP acid", "Notch phaser", "Seq melody", "Vibrato", "Chance gates" };
void (*PRESET_FN[])(void) = { preset_empty, preset_generative, preset_acid, preset_beats, preset_keys, preset_pwmpad, preset_turing, preset_grids, preset_marbles, preset_maths, preset_envpluck, preset_zaplead, preset_punch, preset_glide, preset_bpacid, preset_notchphaser, preset_seq, preset_vibe, preset_chance };
#define NPRESET 19

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
void load_patch(void) {
    Patch p;
    if (load_bytes(&p, sizeof p) == (int)sizeof p) {
        nmod = p.nmod < 0 ? 0 : p.nmod > MAX_MOD ? MAX_MOD : p.nmod;
        for (int i = 0; i < nmod; i++) { mod[i] = (Module){ p.m[i].type, p.m[i].x, p.m[i].y, {0}, {0}, {0} }; for (int k = 0; k < 8; k++) mod[i].param[k] = p.m[i].param[k]; }
        ncable = p.ncable < 0 ? 0 : p.ncable > MAXCABLE ? MAXCABLE : p.ncable;
        for (int c = 0; c < ncable; c++) cable[c] = p.cable[c];
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
    preset_generative();
}

void eval_mod(int mi) {
    Module *m = &mod[mi];
    switch (m->type) {
        case MOD_CLOCK: {
            // each CLOCK runs its OWN tempo off dt() — NOT the global bpm() — so multiple
            // clocks are independent and only drive what they're patched to. 8th-note steps
            // (2 per beat); state[0]=phase, state[1]=last step, state[2]=LED flash timer.
            m->state[0] += dt() * (m->param[0] / 60.0f) * 2.0f;
            int s = (int)m->state[0];
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
            m->jackval[0] = (sin_deg(m->state[0] * 360.0f) + 1.0f) * 0.5f;
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
            int slot = (amp_cv ? 14 : 5) + (int)m->param[3];   // wav picks the wave (9 incl. user waves); VCA uses the flat-envelope bank (14-22)
            if (gate > 0.5f && m->state[0] <= 0.5f) {
                int mm = (int)pitch; if (mm < 1) mm = 48;
                int h = (int)m->state[1]; if (h > 0) note_off(h);
                h = note_on(mm, slot, amp_cv ? 0 : 5);   // VCA-driven → start silent, the amp CV opens it
                m->state[1] = h;
                // onboard mod-envelopes — fire per note: filter "pew" + pitch punch (fixed
                // snappy decays; the knobs set the depth, 0 = off). This is the audio-rate
                // upgrade over patching the control-rate ENV module into 'f'.
                note_env(h, 0, ENV_CUTOFF, 0, 130, m->param[5]);   // fenv → filter sweep   (param 5 = the fenv knob)
                note_env(h, 1, ENV_PITCH,  0,  45, m->param[6]);   // penv → pitch blip     (param 6 = the penv knob)
                m->state[3] = 0;   // trigger flash
            }
            m->state[0] = gate; m->state[3] += 1;
            m->state[2] = m->param[0] + clamp(fcv, 0, 1) * 1800.0f;   // cutoff = base knob + 'f' CV
            int h = (int)m->state[1];
            if (h > 0) {
                note_pitch(h, pitch < 1 ? 48.0f : pitch);                                     // track pitch CV every frame (enables live vibrato, bends)
                note_filter(h, 1 + (int)clamp(m->param[4], 0, 3));                           // lp/hp/bp/nt   (param 4 = the flt knob)
                note_cutoff(h, (int)m->state[2]);
                note_res(h, (int)clamp(m->param[1] + read_in(mi, 3) * 15.0f, 0, 15));
                note_duty(h, clamp(m->param[2] + read_in(mi, 4) * 0.5f, 0.05f, 0.95f));
                if (amp_cv) note_vol(h, (int)(clamp(read_in(mi, 5), 0, 1) * 7.0f + 0.5f));
                // vb jack (6): if patched to a VIBE, read its params and apply audio-rate LFO
                int vc = cable_into(mi, 6);
                if (vc >= 0 && mod[cable[vc].sm].type == MOD_VIBRATO && mod[cable[vc].sm].jackval[1] > 0.5f) {
                    Module *vb = &mod[cable[vc].sm];
                    int dst = (int)clamp(vb->param[2], 0, 2);
                    int dests[] = { LFO_PITCH, LFO_CUTOFF, LFO_DUTY };
                    float d = vb->param[1];
                    float dep[] = { d * 2.5f, d * 800.0f, d * 0.35f };
                    note_lfo(h, 0, dests[dst], vb->param[0], dep[dst]);
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
        case MOD_DRUM: {
            float k = read_in(mi, 0), s = read_in(mi, 1), h = read_in(mi, 2);
            if (k > 0.5f && m->state[0] <= 0.5f) { hit(72, INSTR_NOISE, 3, 18); hit(43, INSTR_TRI, 7, 90); m->state[3] = 0; } m->state[0] = k; m->state[3] += 1;
            if (s > 0.5f && m->state[1] <= 0.5f) { hit(58, INSTR_NOISE, 5, 90); m->state[4] = 0; } m->state[1] = s; m->state[4] += 1;
            if (h > 0.5f && m->state[2] <= 0.5f) { hit(92, INSTR_NOISE, 3, 22); m->state[5] = 0; } m->state[2] = h; m->state[5] += 1;
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
    }
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
int sig_col(int t) { return t == 0 ? CLR_GREEN : t == 1 ? CLR_YELLOW : CLR_BLUE; }

int near_col(int x, int y) {
    float d = distance(wmx, wmy, x, y);
    return d < 12 ? CLR_WHITE : d < 26 ? CLR_LIGHT_GREY : d < 48 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY;
}

const char *knob_str(int fmt, float v) {
    if (fmt == FMT_LOGIC) { const char *L[3] = { "AND", "OR", "XOR" }; return L[(int)v]; }
    if (fmt == FMT_WAVE)  { const char *W[9] = { "saw", "sqr", "tri", "sin", "noi", "org", "vox", "bel", "fld" }; return W[(int)v]; }   // org/vox/bel/fld = drawn user waves
    if (fmt == FMT_FILTER){ const char *F[4] = { "lp",  "hp",  "bp",  "nt"  }; return F[(int)clamp(v, 0, 3)]; }
    if (fmt == FMT_DEST)  { const char *D[3] = { "pit", "cut", "pw"  }; return D[(int)clamp(v, 0, 2)]; }
    if (fmt == FMT_SCALE) return SCALES[(int)v];
    if (fmt == FMT_NOTE)  return NOTES[(int)v];
    if (fmt == FMT_F1)    return str("%.1f", v);
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
    if (mod[mi].type == MOD_VOICE && (int)mod[mi].state[1] > 0) note_off((int)mod[mi].state[1]);   // don't leave a stuck note
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
            const char *sig = jd.type == 0 ? "gate" : jd.type == 1 ? "pit" : "cv";
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

void draw(void) {
    // ---- pan (left-drag empty space) + zoom (wheel), in screen-delta terms ----
    if (panning && mouse_down(MOUSE_LEFT)) { cam_x -= (mouse_x() - pan_px) / zoom; cam_y -= (mouse_y() - pan_py) / zoom; }
    pan_px = mouse_x(); pan_py = mouse_y();
    if (!mouse_down(MOUSE_LEFT)) { panning = 0; held_knob = 0; }
    bool over_side = mouse_x() < SIDEBAR_W;
    float wh = mouse_wheel();
    if (wh != 0) {
        if (over_side) {   // wheel over the palette → scroll the module list
            int maxsc = 14 + NTYPE * 16 - SCREEN_H + 6; if (maxsc < 0) maxsc = 0;
            palette_scroll = (int)clamp(palette_scroll - wh * 16, 0, maxsc);
        } else {           // wheel over the canvas → step to the next crisp zoom stop (point under cursor stays fixed)
            int zi = 0; for (int i = 0; i < NZOOM; i++) if (zoom >= ZOOMS[i] - 0.01f) zi = i;
            zi = (int)clamp(zi + (wh > 0 ? 1 : -1), 0, NZOOM - 1);
            float z0 = zoom, z1 = ZOOMS[zi];
            if (z1 != z0) {
                cam_x += (mouse_x() - SCREEN_W / 2.0f) * (1.0f / z0 - 1.0f / z1);
                cam_y += (mouse_y() - SCREEN_H / 2.0f) * (1.0f / z0 - 1.0f / z1);
                zoom = z1;
            }
        }
    }

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
        if (hot && mouse_pressed(MOUSE_LEFT) && palette_drag < 0 && nmod < MAX_MOD && help_type < 0 && !preset_open) palette_drag = t;
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
        int rows = (NPRESET + 1) / 2;
        for (int i = 0; i < NPRESET; i++) {
            int col = i / rows, row = i % rows;
            int ix = pbx + col * pbw, iy = 14 + row * 12;
            bool ih = mouse_x() >= ix && mouse_x() < ix + pbw && mouse_y() >= iy && mouse_y() < iy + 12;
            rectfill(ix, iy, pbw, 12, ih ? CLR_DARK_GREY : CLR_BLACK);
            rect(ix, iy, pbw, 12, CLR_DARKER_GREY);
            print(PRESET_NAMES[i], ix + 4, iy + 4, CLR_LIGHT_GREY);
            if (ih && mouse_pressed(MOUSE_LEFT)) { PRESET_FN[i](); preset_open = 0; }
        }
        bool inside = mouse_x() >= pbx && mouse_x() < pbx + pbw * 2 && mouse_y() < 14 + rows * 12;
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
}
