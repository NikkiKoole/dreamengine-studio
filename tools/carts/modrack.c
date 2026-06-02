#include "studio.h"

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
       MOD_SLEW, MOD_ATTN, MOD_LOGIC, MOD_SCOPE, MOD_KEYS, NTYPE };
enum { FMT_INT, FMT_F1, FMT_SCALE, FMT_NOTE, FMT_MS, FMT_LOGIC, FMT_WAVE };

typedef struct { int type; bool out; int dx, dy; const char *label; } JackDef;   // type: 0 gate/1 pitch/2 cv
typedef struct { const char *label; float lo, hi, def; int dx, dy, fmt; } KnobDef;
typedef struct {
    const char *name; int col, cw, ch;   // size in 12px cells
    int njack; JackDef jack[6];
    int nknob; KnobDef knob[4];
} ModType;

ModType TYPES[NTYPE] = {
    // ── big modules (6x7 cells = 72x84) ──
    [MOD_CLOCK] = { "CLOCK", CLR_ORANGE, 6, 7, 3, {{0,true,16,72,"1"},{0,true,36,72,"2"},{0,true,56,72,"4"}},
                   1, {{"bpm",60,240,112,40,34,FMT_INT}} },
    [MOD_LFO]   = { "LFO", CLR_PINK, 6, 7, 1, {{2,true,36,72,"cv"}},
                   1, {{"rate",0.1f,8,0.37f,24,36,FMT_F1}} },
    [MOD_SH]    = { "S&H", CLR_YELLOW, 6, 7, 3, {{2,false,24,16,"in"},{0,false,48,16,"clk"},{2,true,36,72,"cv"}}, 0, {} },
    [MOD_QUANT] = { "QUANT", CLR_GREEN, 6, 7, 2, {{2,false,36,16,"in"},{1,true,36,72,"pit"}},
                   2, {{"scl",0,5.99f,SCALE_PENTA,22,40,FMT_SCALE},{"root",0,11.99f,0,50,40,FMT_NOTE}} },
    [MOD_VOICE] = { "VOICE", CLR_BLUE, 6, 7, 5, {{0,false,7,16,"g"},{1,false,21,16,"p"},{2,false,35,16,"f"},{2,false,49,16,"r"},{2,false,63,16,"w"}},
                   4, {{"cut",200,2200,700,20,40,FMT_INT},{"res",0,15,6,52,40,FMT_INT},{"pw",0.05f,0.95f,0.5f,20,62,FMT_F1},{"wav",0,4.99f,0,52,62,FMT_WAVE}} },
    [MOD_EUCLID]= { "EUCLID", CLR_RED, 6, 7, 2, {{0,false,36,16,"clk"},{0,true,36,72,"g"}},
                   2, {{"h",1,8.99f,4,22,40,FMT_INT},{"s",2,16.99f,8,50,40,FMT_INT}} },
    [MOD_ENV]   = { "ENV", CLR_MEDIUM_GREEN, 6, 7, 2, {{0,false,36,16,"g"},{2,true,36,72,"cv"}},
                   2, {{"atk",0.005f,0.5f,0.01f,26,40,FMT_MS},{"dec",0.02f,1,0.25f,52,40,FMT_MS}} },
    [MOD_DRUM]  = { "DRUM", CLR_DARK_ORANGE, 6, 7, 3, {{0,false,18,16,"k"},{0,false,36,16,"s"},{0,false,54,16,"h"}}, 0, {} },
    // ── tiny utilities (3x5 = 36x60, LOGIC 4x5 = 48x60) ──
    [MOD_SLEW]  = { "SLEW", CLR_INDIGO, 3, 5, 2, {{2,false,18,12,"in"},{2,true,18,52,"out"}},
                   1, {{"ms",1,500,80,18,32,FMT_INT}} },
    [MOD_ATTN]  = { "ATTN", CLR_MAUVE, 3, 5, 2, {{2,false,18,12,"in"},{2,true,18,52,"out"}},
                   1, {{"amt",0,1,1,18,32,FMT_F1}} },
    [MOD_LOGIC] = { "LOGIC", CLR_LIGHT_YELLOW, 4, 5, 3, {{0,false,14,14,"a"},{0,false,34,14,"b"},{0,true,24,52,"o"}},
                   1, {{"mod",0,2.99f,0,24,34,FMT_LOGIC}} },
    [MOD_SCOPE] = { "SCOPE", CLR_LIGHT_GREY, 6, 4, 1, {{2,false,8,13,"in"}}, 0, {} },
    [MOD_KEYS]  = { "KEYS", CLR_LIGHT_PEACH, 6, 5, 2, {{0,true,24,52,"g"},{1,true,48,52,"p"}}, 0, {} },
};
int tw(int type) { return TYPES[type].cw * CELL; }   // module pixel width/height
int th(int type) { return TYPES[type].ch * CELL; }

// one-screen help per module kind (click the module's ? to show it)
const char *HELP[NTYPE][3] = {
    [MOD_CLOCK]  = { "Master clock. bpm knob sets the tempo.", "Gate outs: /1 every step, /2 half,", "/4 quarter. Patch into trigger inputs." },
    [MOD_LFO]    = { "Low-freq oscillator: a slow 0..1 wave.", "rate sets speed. Patch the cv out into", "any cv input to modulate it." },
    [MOD_SH]     = { "Sample & Hold. On each clk pulse it", "grabs the cv at 'in' and holds it until", "the next clk -> a stepped, random-ish cv." },
    [MOD_QUANT]  = { "Quantizer. Snaps any cv to the nearest", "note of a scale (scl/root) so it's always", "in key. cv in -> pitch out." },
    [MOD_VOICE]  = { "The voice. g=gate p=pitch; f/r/w CV sweep", "cutoff/res/pulse-width. Knobs set the base", "+ wav picks the waveform (saw/sqr/tri/sin/noi)." },
    [MOD_EUCLID] = { "Euclidean rhythm: h hits spread evenly", "over s steps. Advances on clk, fires a", "gate on each hit. Patch 'o' into DRUM." },
    [MOD_ENV]    = { "AD envelope. A gate makes a 0->1->0 cv", "ramp (atk up, dec down). Patch cv into a", "filter for a pluck on every note." },
    [MOD_DRUM]   = { "Three drum voices. A gate at k/s/h", "fires kick / snare / hat. Patch gates", "(from EUCLID or CLOCK) into them." },
    [MOD_SLEW]   = { "Smooths a cv toward its input over 'ms'.", "Feed it stepped pitch -> glide / lag.", "" },
    [MOD_ATTN]   = { "Attenuator: scales a cv by amt (0..1).", "Sets how much a modulator affects its", "target -- i.e. the depth." },
    [MOD_LOGIC]  = { "Combines two gates: AND / OR / XOR (mod).", "AND two EUCLIDs for a pattern neither", "has alone -- emergent rhythm." },
    [MOD_SCOPE]  = { "Oscilloscope. Patch a cv into 'in' to see", "it drawn as a moving trace -- a probe for", "watching what a signal is actually doing." },
    [MOD_KEYS]   = { "Play it! Click the keys or use computer", "keys A S D F G H J. Outputs gate (g) +", "pitch (p) -- patch them into a VOICE." },
};

// ── module instances + cables ──
typedef struct { int type, x, y; float param[4], state[24], jackval[4]; } Module;   // param[] up to 4 knobs; state[] holds SCOPE history
#define MAX_MOD 24
Module mod[MAX_MOD];
int    nmod = 0;

typedef struct { int sm, sj, dm, dj; } Cable;
#define MAXCABLE 48
Cable cable[MAXCABLE];
int   ncable = 0;

// global transport + ui state
int   g_step = 0, g_newstep = 0, last_step = -1, tick_flash = 99;
int   held_knob = 0, drag_y = 0, drag_jack = -1, msg_flash = 0, dbg_midi = 60;
const char *msg = "";

// ── canvas (pan/zoom) + palette state ──
#define SIDEBAR_W 38
float cam_x = -28, cam_y = -2, zoom = 0.82f;     // pannable/zoomable view of the stage
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
    mod[vo].param[0] = 420; mod[qt].param[0] = SCALE_PENTA_MIN;
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
    add_cable(kb, 0, vo, 0); add_cable(kb, 1, vo, 1); add_cable(kb, 0, en, 0); add_cable(en, 1, vo, 2);
    add_cable(ck, 0, eu, 0); add_cable(eu, 1, dr, 0); add_cable(ck, 0, dr, 2);
}
const char *PRESET_NAMES[] = { "Generative", "Acid bass", "Beats", "Keys synth" };
void (*PRESET_FN[])(void) = { preset_generative, preset_acid, preset_beats, preset_keys };
#define NPRESET 4

// ── persistence ──
typedef struct { int type, x, y; float param[4]; } SaveMod;
typedef struct { int nmod; SaveMod m[MAX_MOD]; int ncable; Cable cable[MAXCABLE]; } Patch;

void save_patch(void) {
    Patch p; p.nmod = nmod; p.ncable = ncable;
    for (int i = 0; i < nmod; i++) { p.m[i].type = mod[i].type; p.m[i].x = mod[i].x; p.m[i].y = mod[i].y; for (int k = 0; k < 4; k++) p.m[i].param[k] = mod[i].param[k]; }
    for (int c = 0; c < ncable; c++) p.cable[c] = cable[c];
    save_bytes(&p, sizeof p);
    msg = "SAVED"; msg_flash = 40;
}
void load_patch(void) {
    Patch p;
    if (load_bytes(&p, sizeof p) == (int)sizeof p) {
        nmod = p.nmod < 0 ? 0 : p.nmod > MAX_MOD ? MAX_MOD : p.nmod;
        for (int i = 0; i < nmod; i++) { mod[i] = (Module){ p.m[i].type, p.m[i].x, p.m[i].y, {0}, {0}, {0} }; for (int k = 0; k < 4; k++) mod[i].param[k] = p.m[i].param[k]; }
        ncable = p.ncable < 0 ? 0 : p.ncable > MAXCABLE ? MAXCABLE : p.ncable;
        for (int c = 0; c < ncable; c++) cable[c] = p.cable[c];
        msg = "LOADED";
    } else msg = "no save";
    msg_flash = 40;
}

void init(void) {
    // five voice slots (5-9) that differ ONLY in waveform, so VOICE's wav knob is a pure
    // waveshape selector (same ADSR + filter; cutoff/res/pw are driven live per note)
    int waves[5] = { INSTR_SAW, INSTR_SQUARE, INSTR_TRI, INSTR_SINE, INSTR_NOISE };
    for (int i = 0; i < 5; i++) { instrument(5 + i, waves[i], 6, 120, 4, 300); instrument_filter(5 + i, FILTER_LOW, 800, 8); }
    instrument_duty(6, 0.5f);   // square's pw knob overrides this live
    preset_generative();
}

void eval_mod(int mi) {
    Module *m = &mod[mi];
    switch (m->type) {
        case MOD_CLOCK:
            bpm((int)m->param[0]);
            m->jackval[0] = g_newstep ? 1 : 0;
            m->jackval[1] = (g_newstep && g_step % 2 == 0) ? 1 : 0;
            m->jackval[2] = (g_newstep && g_step % 4 == 0) ? 1 : 0;
            break;
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
            int slot = 5 + (int)m->param[3];   // wav knob picks the waveform (slots 5-9)
            float gate = read_in(mi, 0), pitch = read_in(mi, 1), fcv = read_in(mi, 2);
            if (gate > 0.5f && m->state[0] <= 0.5f) {
                int mm = (int)pitch; if (mm < 1) mm = 48;
                int h = (int)m->state[1]; if (h > 0) note_off(h);
                m->state[1] = note_on(mm, slot, 5);
                m->state[3] = 0;   // trigger flash
            }
            m->state[0] = gate; m->state[3] += 1;
            m->state[2] = m->param[0] + clamp(fcv, 0, 1) * 1800.0f;   // cutoff = base knob + 'f' CV
            int h = (int)m->state[1];
            if (h > 0) {
                note_cutoff(h, (int)m->state[2]);
                note_res(h, (int)clamp(m->param[1] + read_in(mi, 3) * 15.0f, 0, 15));        // resonance = res knob + 'r' CV
                note_duty(h, clamp(m->param[2] + read_in(mi, 4) * 0.5f, 0.05f, 0.95f));      // pulse width = pw knob + 'w' CV
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
    if (fmt == FMT_WAVE)  { const char *W[5] = { "saw", "sqr", "tri", "sin", "noi" }; return W[(int)v]; }
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
            if (distance(mx, my, jackpos_x(mi, j), jackpos_y(mi, j)) < 6) return mi * 4 + j;
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
    if (palette_drag < 0 && !panning && help_type < 0 && drag_mod < 0 && !preset_open && mouse_pressed(MOUSE_LEFT) && distance(wmx, wmy, cx, cy) < 7) { held_knob = id; drag_y = wmy; }
    if (held_knob == id && mouse_down(MOUSE_LEFT)) { v = clamp(v + (drag_y - wmy) * (hi - lo) / 120.0f, lo, hi); drag_y = wmy; }
    bool hot = held_knob == id || distance(wmx, wmy, cx, cy) < 7;
    circfill(cx, cy, 5, CLR_DARKER_GREY);
    circ(cx, cy, 5, hot ? CLR_WHITE : CLR_MEDIUM_GREY);
    float a = 135.0f + clamp((v - lo) / (hi - lo), 0, 1) * 270.0f;
    line(cx, cy, cx + (int)(cos_deg(a) * 4), cy + (int)(sin_deg(a) * 4), CLR_WHITE);
    print(name, cx - text_width(name) / 2, cy + 7, near_col(cx, cy));
    if (hot) print(val, cx - text_width(val) / 2, cy + 13, CLR_WHITE);
    return v;
}

// per-type extra visuals (meters, LEDs, dots) drawn after the generic frame
void draw_extras(int mi) {
    Module *m = &mod[mi]; int x = m->x, y = m->y, cx = x + GW / 2;
    bool gate_lit = tick_flash < 5;
    switch (m->type) {
        case MOD_CLOCK: circfill(x + 12, y + 14, 3, gate_lit ? CLR_WHITE : CLR_DARK_ORANGE); break;
        case MOD_LFO:   meter(x + 48, y + 18, 6, 46, m->jackval[0], CLR_PINK); break;
        case MOD_SH:    meter(x + 10, y + 30, 6, 36, m->state[1], CLR_YELLOW); print(str("%d%%", (int)(m->state[1] * 99)), x + 26, y + 40, CLR_DARK_GREY); break;
        case MOD_QUANT: print(NOTES[((int)m->jackval[1]) % 12], cx - text_width(NOTES[((int)m->jackval[1]) % 12]) / 2, y + 54, CLR_WHITE); break;
        case MOD_VOICE: if (m->state[3] < 8) circfill(x + GW / 2, y + 30, 5 - (int)m->state[3] / 2, CLR_LIGHT_PEACH); break;   // trigger pulse
        case MOD_EUCLID: {
            int hits = (int)m->param[0], steps = (int)m->param[1];
            for (int s = 0; s < steps; s++) {
                int dx = x + 8 + (int)(s * (56.0f / steps));
                bool on = euclid(hits, steps, s);
                circfill(dx, y + 56, on ? 2 : 1, on ? CLR_RED : CLR_DARK_GREY);
                if (s == (((int)m->state[1] % steps) + steps) % steps && m->state[2] < 6) circ(dx, y + 56, 3, CLR_WHITE);
            }
            break; }
        case MOD_ENV:   meter(x + 8, y + 30, 6, 36, m->state[3], CLR_MEDIUM_GREEN); break;
        case MOD_DRUM:
            circfill(x + 18, y + 48, 5, m->state[3] < 5 ? CLR_WHITE : CLR_DARK_RED);
            circfill(x + 36, y + 48, 4, m->state[4] < 5 ? CLR_WHITE : CLR_BROWN);
            circfill(x + 54, y + 48, 3, m->state[5] < 5 ? CLR_WHITE : CLR_DARK_GREY);
            print("kik sn hat", x + 8, y + 64, CLR_DARK_GREY);
            break;
        case MOD_SCOPE: {   // moving trace of the recorded cv history
            int head = (int)m->state[0];
            rectfill(x + 3, y + 16, GW - 6, 28, CLR_BLACK);
            for (int i = 0; i < 20; i++) {
                int idx = (head - i + 20) % 20;
                pset(x + GW - 5 - i * 3, y + 42 - (int)(m->state[2 + idx] * 24), CLR_LIME_GREEN);
            }
            break; }
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
    print(t->name, x + 3, y + 2, t->col);
    print("?", x + W - 16, y + 2, distance(wmx, wmy, x + W - 15, y + 4) < 5 ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    print("x", x + W - 8, y + 2, distance(wmx, wmy, x + W - 6, y + 4) < 5 ? CLR_RED : CLR_DARK_GREY);

    draw_extras(mi);

    for (int k = 0; k < t->nknob; k++) {
        KnobDef kd = t->knob[k];
        m->param[k] = knob_dial(mi * 4 + k + 1, x + kd.dx, y + kd.dy, m->param[k], kd.lo, kd.hi, kd.label, knob_str(kd.fmt, m->param[k]));
    }
    for (int j = 0; j < t->njack; j++) {
        JackDef jd = t->jack[j];
        int jx = x + jd.dx, jy = y + jd.dy;
        float v = jd.out ? m->jackval[j] : read_in(mi, j);
        bool lit = jd.type == 0 && v > 0.5f;
        int c = sig_col(jd.type);
        if (jd.out) { circ(jx, jy, 3, c); if (lit) circfill(jx, jy, 1, CLR_WHITE); }
        else          circfill(jx, jy, 3, lit ? CLR_WHITE : c);
        print(jd.label, jx - text_width(jd.label) / 2, jy + 5, near_col(jx, jy));
    }
}

void edit_cables(int mx, int my) {
    if (mouse_pressed(MOUSE_LEFT)) {
        int h = jack_at(mx, my);
        if (h >= 0) {
            int mi = h / 4, j = h % 4;
            if (TYPES[mod[mi].type].jack[j].out) drag_jack = h;
            else { int c = cable_into(mi, j); if (c >= 0) { drag_jack = cable[c].sm * 4 + cable[c].sj; remove_cable(c); } }
        }
    }
    if (mouse_released(MOUSE_LEFT)) {
        if (drag_jack >= 0) {
            int h = jack_at(mx, my), sm = drag_jack / 4, sj = drag_jack % 4;
            if (h >= 0) {
                int dm = h / 4, dj = h % 4;
                if (!TYPES[mod[dm].type].jack[dj].out && TYPES[mod[dm].type].jack[dj].type == TYPES[mod[sm].type].jack[sj].type)
                    add_cable(sm, sj, dm, dj);
            }
        }
        drag_jack = -1;
    }
    if (mouse_pressed(MOUSE_RIGHT)) {
        int h = jack_at(mx, my);
        if (h >= 0) { int mi = h / 4, j = h % 4; for (int c = ncable - 1; c >= 0; c--) if ((cable[c].sm == mi && cable[c].sj == j) || (cable[c].dm == mi && cable[c].dj == j)) remove_cable(c); }
    }
}

void draw_cable_between(int sx, int sy, int dx, int dy, int c, bool pulse) {
    int mx = (sx + dx) / 2, my = max(sy, dy) + 14;
    bezier(sx, sy, mx, my, dx, dy, c);
    if (pulse && tick_flash < 10) {
        float t = tick_flash / 10.0f, u = 1.0f - t;
        circfill((int)(u * u * sx + 2 * u * t * mx + t * t * dx), (int)(u * u * sy + 2 * u * t * my + t * t * dy), 2, CLR_WHITE);
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
        } else {           // wheel over the canvas → zoom toward the cursor (world point under mouse stays fixed)
            float z0 = zoom, z1 = clamp(zoom * (1 + wh * 0.12f), 0.4f, 3.0f);
            cam_x += (mouse_x() - SCREEN_W / 2.0f) * (1.0f / z0 - 1.0f / z1);
            cam_y += (mouse_y() - SCREEN_H / 2.0f) * (1.0f / z0 - 1.0f / z1);
            zoom = z1;
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

    if (drag_jack >= 0) {
        int sm = drag_jack / 4, sj = drag_jack % 4, c = sig_col(TYPES[mod[sm].type].jack[sj].type);
        draw_cable_between(jackpos_x(sm, sj), jackpos_y(sm, sj), wmx, wmy, c, false);
        for (int mi = 0; mi < nmod; mi++)
            for (int j = 0; j < TYPES[mod[mi].type].njack; j++)
                if (!TYPES[mod[mi].type].jack[j].out && TYPES[mod[mi].type].jack[j].type == TYPES[mod[sm].type].jack[sj].type)
                    circ(jackpos_x(mi, j), jackpos_y(mi, j), 5, CLR_WHITE);
    }
    if (palette_drag >= 0) {   // ghost snaps to the cell grid where it'll land
        int W = tw(palette_drag), H = th(palette_drag);
        int gx = snap12(wmx - W / 2), gy = snap12(wmy - H / 2);
        rect(gx, gy, W, H, TYPES[palette_drag].col);
        print(TYPES[palette_drag].name, gx + 3, gy + 2, TYPES[palette_drag].col);
    }

    // ===== SCREEN SPACE — palette sidebar + HUD =====
    camera(0, 0);
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
        for (int i = 0; i < NPRESET; i++) {
            int iy = 14 + i * 12;
            bool ih = mouse_x() >= pbx && mouse_x() < pbx + pbw && mouse_y() >= iy && mouse_y() < iy + 12;
            rectfill(pbx, iy, pbw, 12, ih ? CLR_DARK_GREY : CLR_BLACK);
            rect(pbx, iy, pbw, 12, CLR_DARKER_GREY);
            print(PRESET_NAMES[i], pbx + 4, iy + 4, CLR_LIGHT_GREY);
            if (ih && mouse_pressed(MOUSE_LEFT)) { PRESET_FN[i](); preset_open = 0; }
        }
        bool inside = mouse_x() >= pbx && mouse_x() < pbx + pbw && mouse_y() < 14 + NPRESET * 12;
        if (mouse_pressed(MOUSE_LEFT) && !inside) preset_open = 0;   // click away to close
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
