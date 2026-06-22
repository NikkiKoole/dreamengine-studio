#include "studio.h"
#include "ui.h"
#include <math.h>

// EASEL — a Buchla Music Easel: a complete west-coast synthesis voice. Where `lpg` showed the
// low-pass gate alone, this is the whole instrument the gate belongs to — a complex oscillator
// (harmonics made by cross-modulation, not subtraction) → wavefolder → low-pass gate, played by
// a pressure ribbon and shaped by a function generator. (teaches[]/lineage in index.json.)
//
// • COMPLEX OSC (INSTR_FM): RATIO (carrier:mod), TIMBRE (cross-mod depth, sine→clang), all live.
// • FOLD: a wavefolder (DRIVE_FOLD) for glassy/metallic harmonics.
// • LOW-PASS GATE: the final voicing — note_vol + note_cutoff close together (the lpg coupling).
// • FUNCTION GENERATOR: a RISE/FALL slope. PLUCK = tap fires it → a bonk; PRESS = ribbon pressure
//   rides the gate live; CYCLE = it free-runs as an LFO → GATE (tremolo) or TIMBRE (auto sweep).
// • RIBBON: X = pitch (SNAP to scale, or FREE/microtonal), Y = pressure. A–K keys play it too.
// • PULSER: RUN auto-fires the function generator at random scale pitches → it plays itself.
//
//   Drag the ribbon (X pitch, Y pressure). Knobs RATIO/TIMBRE/FOLD/RISE/FALL/RATE. Buttons pick
//   PLUCK/PRESS · CYCLE off/gate/timbre · RUN · SNAP/FREE. Spec: docs/design/easel-spec.md.

#define SL 5

// ── scale / pitch ──
static const int PENTA[5] = { 0, 2, 4, 7, 9 };
#define NSTEP 13                         // ribbon scale steps (~2.5 oct)
#define BASE 45
static const int LKEY[8] = { 'A','S','D','F','G','H','J','K' };
static int step_pitch(int s) { return BASE + 12 * (s / 5) + PENTA[s % 5]; }

// ── the one voice + the gate ──
static int   hV = -1;
static float glvl = 0;                   // LPG level (0..1) — drives vol + cutoff + fold ('gate' is a built-in)
static int   gph = 0;                    // function-gen phase: 0 idle, 1 rising, 2 falling (PLUCK)
static float peak = 1.0f;                // pluck velocity (target of the rise)
static float vpitch = BASE;              // current (glided) pitch
static int   gflash = -100;

// ── function-generator free-run LFO (CYCLE) ──
static float lfo = 0; static int lfodir = 1;

// ── knobs / toggles ──
static float k_ratio = 0.30f, k_timbre = 0.45f, k_fold = 0.35f;
static float k_rise = 0.12f, k_fall = 0.45f, k_rate = 0.4f;
static int   mode = 0;        // 0 = PLUCK, 1 = PRESS
static int   cyc  = 0;        // 0 = off, 1 = →GATE, 2 = →TIMBRE
static int   run  = 0;        // pulser
static int   freetune = 0;    // 0 = SNAP, 1 = FREE
static float puls_t = 0;

// sticky widget-ownership (so a knob drag over the ribbon doesn't play it)
static int own_ids[8], own_n = 0;
static bool is_owned(int id){ for(int i=0;i<own_n;i++) if(own_ids[i]==id) return true; return false; }
static void own(int id){ if(!is_owned(id)&&own_n<8) own_ids[own_n++]=id; }
static void unown(int id){ for(int i=0;i<own_n;i++) if(own_ids[i]==id){own_ids[i]=own_ids[--own_n];return;} }

#define RIB_X 6
#define RIB_Y 128
#define RIB_W 308
#define RIB_H 64

static float rise_per(void) { return (1.0f/60.0f) / (0.01f + k_rise * k_rise * 2.0f); }   // 0.01..2.0s
static float fall_per(void) { return (1.0f/60.0f) / (0.03f + k_fall * k_fall * 3.0f); }   // 0.03..3.0s

static void pluck(int pitch, float vel) { vpitch = pitch; note_pitch(hV, pitch); gph = 1; peak = vel; gflash = frame(); }

void init(void) {
    instrument(SL, INSTR_FM, 2, 0, 9, 120);        // sustaining FM voice — our gate articulates it
    instrument_filter(SL, FILTER_LOW, 8000, 3);    // the gate's lowpass
    instrument_drive_mode(SL, DRIVE_FOLD);         // wavefolder
    instrument_drive(SL, 0.0f);
    hV = note_on(BASE, SL, 0);
    note_glide(hV, 22);                            // legato ribbon slides
    note_harmonics(hV, k_ratio);
    note_timbre(hV, k_timbre);
}

void update(void) {
    // keys: A-K play, SPACE = RUN, number/letter toggles
    for (int i = 0; i < 8; i++) if (keyp(LKEY[i])) pluck(step_pitch(i), 1.0f);   // keys always pluck
    if (keyp(KEY_SPACE)) run = !run;
    if (keyp('Z')) mode = !mode;
    if (keyp('X')) cyc = (cyc + 1) % 3;
    if (keyp('C')) freetune = !freetune;

    // ribbon — skip fingers that own a knob (sticky)
    for (int i = 0; i < touch_count(); i++) if (ui_captured(touch_id(i))) own(touch_id(i));
    for (int i = 0; i < touch_ended_count(); i++) unown(touch_ended_id(i));

    int rib_finger = -1; float rx = 0, ry = 0;
    for (int i = 0; i < touch_count(); i++) {
        if (is_owned(touch_id(i))) continue;
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= RIB_X && tx < RIB_X + RIB_W && ty >= RIB_Y && ty < RIB_Y + RIB_H) {
            rib_finger = touch_id(i);
            rx = clamp((tx - RIB_X) / (float)RIB_W, 0, 1);
            ry = clamp(1.0f - (ty - RIB_Y) / (float)RIB_H, 0, 1);
        }
    }
    // ribbon pitch
    float rpitch = vpitch;
    if (rib_finger != -1) {
        if (freetune) rpitch = BASE + rx * (NSTEP - 1) * 2.0f;          // continuous-ish (microtonal)
        else          rpitch = step_pitch((int)(rx * (NSTEP - 0.001f)));
    }

    // ── ribbon: both modes track pitch by X; PLUCK fires the FG on a NEW touch ──
    static int last_rib = -1;
    if (rib_finger != -1) {
        vpitch = rpitch; note_pitch(hV, (int)(rpitch + 0.5f));
        if (mode == 0 && rib_finger != last_rib) pluck((int)(rpitch + 0.5f), 0.5f + ry * 0.5f);
    }
    last_rib = rib_finger;

    // pulser self-play (fires plucks)
    if (run) {
        puls_t += 1.0f / 60.0f;
        float iv = 0.12f + (1.0f - k_rate) * 0.6f;     // RATE → 0.12..0.72s
        if (puls_t >= iv) { puls_t -= iv; pluck(step_pitch(rnd(NSTEP)), 0.6f + 0.4f * (rnd(100) / 100.0f)); }
    }

    // ── the function generator → gate ──
    if (mode == 1 && rib_finger != -1) {               // PRESS: slew the gate to finger pressure
        float rate = (ry > glvl) ? rise_per() : fall_per();
        glvl += clamp(ry - glvl, -rate, rate); gph = 0;
    } else if (gph == 1) { glvl += rise_per(); if (glvl >= peak) { glvl = peak; gph = 2; } }   // pluck rise
    else if (gph == 2) { glvl -= fall_per(); if (glvl <= 0) { glvl = 0; gph = 0; } }            // pluck fall
    else if (mode == 1) { glvl -= fall_per(); if (glvl < 0) glvl = 0; }                          // press released

    // CYCLE: the FG free-runs as an LFO (rise/fall), routed to GATE or TIMBRE
    lfo += lfodir * ((lfodir > 0) ? rise_per() : fall_per());
    if (lfo >= 1) { lfo = 1; lfodir = -1; } else if (lfo <= 0) { lfo = 0; lfodir = 1; }
    float gout = glvl, tmod = 0;
    if (cyc == 1) gout = glvl * (0.25f + 0.75f * lfo);   // tremolo
    if (cyc == 2) tmod = lfo * 0.5f;                      // auto cross-mod sweep

    // ── apply to the voice: complex-osc macros + fold + low-pass gate ──
    note_harmonics(hV, k_ratio);
    note_timbre(hV, clamp(k_timbre + tmod, 0, 1));
    note_vol(hV, gout * 6.0f);
    note_cutoff(hV, (int)(300.0f + 7700.0f * powf(clamp(gout, 0, 1), 1.6f)));   // highs die with the gate
    note_drive(hV, k_fold * gout);

#ifdef DE_TRACE
    watch("gate", "%.2f", glvl);
    watch("mode", "%s", mode ? "PRESS" : "PLUCK");
    watch("cyc",  "%d", cyc);
    watch("pitch", "%.1f", vpitch);
#endif
}

static const char *NN[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("EASEL", 2, 3, CLR_LIGHT_YELLOW);
    int n = (int)(vpitch + 0.5f);
    print(str("%s%d", NN[n % 12], n / 12 - 1), 52, 3, CLR_LIGHT_GREY);
    print_right("west-coast voice", 318, 3, CLR_DARK_GREY);

    // signal-flow tag + the function-generator slope scope
    font(FONT_SMALL);
    print("OSC>FOLD>LPG", 2, 14, CLR_MEDIUM_GREY);
    int sx = 96, sy = 14, sw = 66, sh = 22;
    rect(sx, sy, sw, sh, CLR_DARKER_GREY);
    int rw = (int)(sw * k_rise / (k_rise + k_fall + 0.001f));    // rise portion width
    line(sx, sy + sh, sx + rw, sy + 2, CLR_DARK_BLUE);           // rise ramp
    line(sx + rw, sy + 2, sx + sw, sy + sh, CLR_DARK_BLUE);      // fall ramp
    int gy = sy + sh - (int)(glvl * (sh - 3));                    // current gate level dot
    circfill(sx + rw, gy, 2, glvl > 0.02f ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    print("FUNC GEN", sx, sy - 1 + sh + 1, CLR_DARK_GREY);
    print(cyc == 1 ? "CYCLE>GATE" : cyc == 2 ? "CYCLE>TIMBRE" : "", 168, 16, CLR_INDIGO);
    font(FONT_NORMAL);

    // ── knob row ──
    ui_begin();
    int ky = 58;
    font(FONT_SMALL);
    ui_knob(&k_ratio,  26,  ky, "RATIO");
    ui_knob(&k_timbre, 74,  ky, "TIMBRE");
    ui_knob(&k_fold,   122, ky, "FOLD");
    ui_knob(&k_rise,   170, ky, "RISE");
    ui_knob(&k_fall,   218, ky, "FALL");
    ui_knob(&k_rate,   266, ky, "RATE");

    // ── toggle buttons ──
    int by = 96;
    if (ui_button(6,   by, 56, 14, mode ? "PRESS" : "PLUCK")) mode = !mode;
    if (ui_button(66,  by, 70, 14, cyc == 1 ? "CYCLE GATE" : cyc == 2 ? "CYCLE TMBR" : "CYCLE OFF")) cyc = (cyc + 1) % 3;
    if (ui_button(140, by, 50, 14, run ? "RUN *" : "RUN")) run = !run;
    if (ui_button(194, by, 60, 14, freetune ? "FREE" : "SNAP")) freetune = !freetune;
    font(FONT_NORMAL);
    ui_end();

    // ── the pressure ribbon ──
    bool playing = glvl > 0.02f;
    rectfill(RIB_X, RIB_Y, RIB_W, RIB_H, playing ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
    // scale guide marks + note labels
    font(FONT_SMALL);
    for (int s = 0; s < NSTEP; s++) {
        int x = RIB_X + (int)((s + 0.5f) / NSTEP * RIB_W);
        line(x, RIB_Y, x, RIB_Y + 4, CLR_DARK_GREY);
        int p = step_pitch(s);
        if (p % 12 == 0) print(NN[0], x - 2, RIB_Y + RIB_H - 7, CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
    rect(RIB_X, RIB_Y, RIB_W, RIB_H, playing ? CLR_WHITE : CLR_MEDIUM_GREY);
    // the playing marker: x = pitch, height = gate (pressure/level)
    int mx = RIB_X + (int)((vpitch - BASE) / ((NSTEP - 1) * 2.0f) * RIB_W);
    mx = mx < RIB_X ? RIB_X : mx > RIB_X + RIB_W ? RIB_X + RIB_W : mx;
    if (playing) {
        int h = (int)(glvl * RIB_H);
        rectfill(mx - 2, RIB_Y + RIB_H - h, 5, h, CLR_LIGHT_YELLOW);
        line(mx, RIB_Y, mx, RIB_Y + RIB_H, CLR_BLUE);
    }
    font(FONT_SMALL);
    print("x = pitch    y = pressure    A-K play", RIB_X + 2, RIB_Y + 1, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
