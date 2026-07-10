/* de:meta
{
  "slug": "easel",
  "title": "Easel",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "west-coast-synthesis",
    "fm-synth",
    "wavefolder",
    "lowpass-gate",
    "adsr-envelope"
  ],
  "lineage": "Homage to the Buchla Music Easel (1973); the complete west-coast voice around lpg.c's gate — novel: a complex oscillator (FM cross-mod) + wavefolder feeding a low-pass gate, a triggerable/cycling function generator, and a pressure ribbon (x=pitch, y=pressure), all cart-side.",
  "description": "A complete west-coast synthesis voice, after the Buchla Music Easel (1973) — where the `lpg` cart showed the low-pass gate alone, this is the whole instrument it belongs to. The chain makes brightness ADDITIVELY (the opposite of every subtractive cart): a COMPLEX OSCILLATOR (INSTR_FM — RATIO sets carrier:mod, TIMBRE the cross-mod depth from sine to clang, both live) → a WAVEFOLDER (FOLD, glassy/metallic harmonics) → a LOW-PASS GATE that closes volume AND brightness together (the highs die first — the organic plonk). A FUNCTION GENERATOR (RISE/FALL slope) shapes it: PLUCK fires it on each tap (a bonk), PRESS rides the gate from finger pressure, and CYCLE free-runs it as an LFO routed to the GATE (tremolo) or TIMBRE (auto cross-mod sweep). Played on a pressure RIBBON — x = pitch (SNAP to a pentatonic scale, or FREE/microtonal like the real Easel's capacitive keys), y = pressure (harder = louder + brighter + more folded); A-K play it from the keyboard too. It is POLYPHONIC (4 voices) — chords on A-K, overlapping plucks, and sequence notes ring over each other. RUN starts a PULSER that plays it itself: with SEQ it steps a 5-NOTE SEQUENCER (tap a step cell to set its pitch), or RND fires random scale pitches. Drag the ribbon; knobs RATIO/TIMBRE/FOLD/RISE/FALL/RATE; buttons pick PLUCK/PRESS, CYCLE off/gate/timbre, RUN, SNAP/FREE, SEQ/RND (Z/X/C/V/SPACE keys mirror them). Spec: docs/design/easel-spec.md."
}
de:meta */
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
// • 5-STEP SEQUENCER + PULSER: RUN steps a 5-note sequence (tap a step to set its pitch) — or RND
//   for random pitches. POLYPHONIC (4 voices) so notes ring and chords/sequences overlap.
//
//   Drag the ribbon (X pitch, Y pressure). Knobs RATIO/TIMBRE/FOLD/RISE/FALL/RATE. Buttons pick
//   PLUCK/PRESS · CYCLE off/gate/timbre · RUN · SNAP/FREE · SEQ/RND. Spec: docs/design/easel-spec.md.

#define SL 5
#define NV 4                             // polyphony

// ── scale / pitch ──
static const int PENTA[5] = { 0, 2, 4, 7, 9 };
#define NSTEP 13                         // ribbon scale steps (~2.5 oct)
#define BASE 45
static const int LKEY[8] = { 'A','S','D','F','G','H','J','K' };
static int step_pitch(int s) { return BASE + 12 * (s / 5) + PENTA[s % 5]; }

// ── the voice pool + per-voice gate ──
static int   h[NV];
static float glvl[NV];                   // LPG level 0..1 per voice ('gate' is a built-in)
static int   gph[NV];                    // function-gen phase: 0 idle, 1 rising, 2 falling
static float peak[NV];                   // pluck velocity (rise target)
static float vp[NV];                     // each voice's (glided) pitch
static int   press_v = -1, press_fid = -1;   // voice + finger held in PRESS mode
static float press_y = 0;
static int   gflash = -100;

// ── function-generator free-run LFO (CYCLE) ──
static float lfo = 0; static int lfodir = 1;

// ── 5-step sequencer ──
static int   seq[5] = { 0, 2, 4, 2, 5 };     // scale steps
static int   seqi = 0;
static int   useseq = 1;                     // 1 = SEQ, 0 = RND

// ── knobs / toggles ──
static float k_ratio = 0.30f, k_timbre = 0.45f, k_fold = 0.35f;
static float k_rise = 0.12f, k_fall = 0.45f, k_rate = 0.4f;
static int   mode = 0;        // 0 = PLUCK, 1 = PRESS
static int   cyc  = 0;        // 0 = off, 1 = →GATE, 2 = →TIMBRE
static int   run  = 0;
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
// 5-step sequencer cells (top-right)
#define SQ_X 168
#define SQ_Y 16
#define SQ_W 29
#define SQ_H 26

static float rise_per(void) { return (1.0f/60.0f) / (0.01f + k_rise * k_rise * 2.0f); }   // 0.01..2.0s
static float fall_per(void) { return (1.0f/60.0f) / (0.03f + k_fall * k_fall * 3.0f); }   // 0.03..3.0s

// grab the quietest voice (steal) and start a pluck
static int pluck(int pitch, float vel) {
    int v = 0; float lo = glvl[0];
    for (int i = 1; i < NV; i++) if (glvl[i] < lo) { lo = glvl[i]; v = i; }
    vp[v] = pitch; note_pitch(h[v], pitch); gph[v] = 1; peak[v] = vel; gflash = frame();
    return v;
}

void init(void) {
    instrument(SL, INSTR_FM, 2, 0, 9, 120);        // sustaining FM voice — our gate articulates it
    instrument_filter(SL, FILTER_LOW, 8000, 3);
    instrument_drive_mode(SL, DRIVE_FOLD);
    instrument_drive(SL, 0.0f);
    for (int v = 0; v < NV; v++) {
        h[v] = note_on(BASE, SL, 0);
        note_glide(h[v], 22);
        glvl[v] = 0; gph[v] = 0; peak[v] = 1; vp[v] = BASE;
    }
}

void update(void) {
    for (int i = 0; i < 8; i++) if (keyp(LKEY[i])) pluck(step_pitch(i), 1.0f);   // keys → chords (poly)
    if (keyp(KEY_SPACE)) run = !run;
    if (keyp('Z')) mode = !mode;
    if (keyp('X')) cyc = (cyc + 1) % 3;
    if (keyp('C')) freetune = !freetune;
    if (keyp('V')) useseq = !useseq;

    // sequencer step editing: tap a cell to bump its pitch up the scale
    for (int i = 0; i < 5; i++) if (tapp(SQ_X + i * SQ_W, SQ_Y, SQ_W - 2, SQ_H)) seq[i] = (seq[i] + 1) % NSTEP;

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
    float rpitch = BASE;
    if (rib_finger != -1)
        rpitch = freetune ? BASE + rx * (NSTEP - 1) * 2.0f : step_pitch((int)(rx * (NSTEP - 0.001f)));

    if (mode == 0) {                                   // PLUCK: new ribbon touch fires a voice; slide repitches it
        static int last_rib = -1, last_v = -1;
        if (rib_finger != -1 && rib_finger != last_rib) last_v = pluck((int)(rpitch + 0.5f), 0.7f + ry * 0.3f);
        else if (rib_finger != -1 && last_v >= 0) { vp[last_v] = rpitch; note_pitch(h[last_v], (int)(rpitch + 0.5f)); }
        last_rib = rib_finger;
    } else {                                           // PRESS: a finger grabs one voice; pressure rides its gate
        if (rib_finger != -1) {
            if (rib_finger != press_fid) { press_v = pluck((int)(rpitch + 0.5f), 0.01f); gph[press_v] = 0; press_fid = rib_finger; }
            press_y = ry; vp[press_v] = rpitch; note_pitch(h[press_v], (int)(rpitch + 0.5f));
        } else if (press_fid != -1) { if (press_v >= 0) gph[press_v] = 2; press_v = -1; press_fid = -1; }   // released → fall
    }

    // pulser self-play → SEQ steps the 5-note sequence, or RND
    if (run) {
        puls_t += 1.0f / 60.0f;
        float iv = 0.12f + (1.0f - k_rate) * 0.6f;
        if (puls_t >= iv) {
            puls_t -= iv;
            if (useseq) { pluck(step_pitch(seq[seqi]), 0.85f); seqi = (seqi + 1) % 5; }
            else        pluck(step_pitch(rnd(NSTEP)), 0.6f + 0.4f * (rnd(100) / 100.0f));
        }
    }

    // ── per-voice gate (function generator) ──
    for (int v = 0; v < NV; v++) {
        if (v == press_v) {                            // PRESS-controlled: slew to finger pressure
            float rate = (press_y > glvl[v]) ? rise_per() : fall_per();
            glvl[v] += clamp(press_y - glvl[v], -rate, rate);
        } else if (gph[v] == 1) { glvl[v] += rise_per(); if (glvl[v] >= peak[v]) { glvl[v] = peak[v]; gph[v] = 2; } }
        else if (gph[v] == 2) { glvl[v] -= fall_per(); if (glvl[v] <= 0) { glvl[v] = 0; gph[v] = 0; } }
    }

    // CYCLE: the FG free-runs as an LFO, routed to GATE (tremolo) or TIMBRE (auto sweep)
    lfo += lfodir * ((lfodir > 0) ? rise_per() : fall_per());
    if (lfo >= 1) { lfo = 1; lfodir = -1; } else if (lfo <= 0) { lfo = 0; lfodir = 1; }
    float trem = (cyc == 1) ? (0.25f + 0.75f * lfo) : 1.0f;
    float tmod = (cyc == 2) ? lfo * 0.5f : 0.0f;

    // ── apply to every voice: complex-osc macros + fold + low-pass gate ──
    for (int v = 0; v < NV; v++) {
        note_harmonics(h[v], k_ratio);
        note_timbre(h[v], clamp(k_timbre + tmod, 0, 1));
        float g = glvl[v] * trem;
        note_vol(h[v], g * 5.0f);
        note_cutoff(h[v], (int)(300.0f + 7700.0f * powf(clamp(g, 0, 1), 1.6f)));
        note_drive(h[v], k_fold * g);
    }

#ifdef DE_TRACE
    watch("mode", "%s", mode ? "PRESS" : "PLUCK");
    watch("seq", "%d", useseq ? seqi : -1);
    watch("v0", "%.2f", glvl[0]);
    watch("cyc", "%d", cyc);
#endif
}

static const char *NN[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("EASEL", 2, 3, CLR_LIGHT_YELLOW);
    print_right("west-coast voice", 318, 3, CLR_DARK_GREY);

    // signal-flow tag + the function-generator slope scope
    font(FONT_SMALL);
    print("OSC>FOLD>LPG", 2, 14, CLR_MEDIUM_GREY);
    int sx = 80, sy = 16, sw = 64, sh = 22;
    rect(sx, sy, sw, sh, CLR_DARKER_GREY);
    int rw = (int)(sw * k_rise / (k_rise + k_fall + 0.001f));
    line(sx, sy + sh, sx + rw, sy + 2, CLR_DARK_BLUE);
    line(sx + rw, sy + 2, sx + sw, sy + sh, CLR_DARK_BLUE);
    float g0 = glvl[0]; int gy = sy + sh - (int)(g0 * (sh - 3));
    circfill(sx + rw, gy, 2, g0 > 0.02f ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    print("FUNC GEN", sx, sy + sh + 1, CLR_DARK_GREY);
    print(cyc == 1 ? "CYC>GATE" : cyc == 2 ? "CYC>TMBR" : "", sx + 2, sy - 1, CLR_INDIGO);

    // ── the 5-step sequencer ──
    for (int i = 0; i < 5; i++) {
        int x = SQ_X + i * SQ_W;
        bool cur = (run && useseq && i == seqi);
        rectfill(x, SQ_Y, SQ_W - 2, SQ_H, cur ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
        rect(x, SQ_Y, SQ_W - 2, SQ_H, cur ? CLR_WHITE : CLR_DARK_GREY);
        int bh = 2 + seq[i] * (SQ_H - 4) / (NSTEP - 1);     // pitch as bar height
        rectfill(x + 2, SQ_Y + SQ_H - bh, SQ_W - 6, bh, useseq ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    }
    print("SEQ (tap a step)", SQ_X, SQ_Y + SQ_H + 1, CLR_DARK_GREY);
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
    if (ui_button(4,   by, 50, 14, mode ? "PRESS" : "PLUCK")) mode = !mode;
    if (ui_button(58,  by, 66, 14, cyc == 1 ? "CYCLE GATE" : cyc == 2 ? "CYCLE TMBR" : "CYCLE OFF")) cyc = (cyc + 1) % 3;
    if (ui_button(128, by, 44, 14, run ? "RUN *" : "RUN")) run = !run;
    if (ui_button(176, by, 54, 14, freetune ? "FREE" : "SNAP")) freetune = !freetune;
    if (ui_button(234, by, 60, 14, useseq ? "SEQ" : "RANDOM")) useseq = !useseq;
    font(FONT_NORMAL);
    ui_end();

    // ── the pressure ribbon ──
    bool playing = false; for (int v = 0; v < NV; v++) if (glvl[v] > 0.02f) playing = true;
    rectfill(RIB_X, RIB_Y, RIB_W, RIB_H, playing ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
    font(FONT_SMALL);
    for (int s = 0; s < NSTEP; s++) {
        int x = RIB_X + (int)((s + 0.5f) / NSTEP * RIB_W);
        line(x, RIB_Y, x, RIB_Y + 4, CLR_DARK_GREY);
        if (step_pitch(s) % 12 == 0) print(NN[0], x - 2, RIB_Y + RIB_H - 7, CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
    rect(RIB_X, RIB_Y, RIB_W, RIB_H, playing ? CLR_WHITE : CLR_MEDIUM_GREY);
    // a marker per sounding voice: x = pitch, height = level
    for (int v = 0; v < NV; v++) {
        if (glvl[v] <= 0.02f) continue;
        int mx = RIB_X + (int)((vp[v] - BASE) / ((NSTEP - 1) * 2.0f) * RIB_W);
        mx = mx < RIB_X ? RIB_X : mx > RIB_X + RIB_W - 1 ? RIB_X + RIB_W - 1 : mx;
        int hh = (int)(glvl[v] * RIB_H);
        rectfill(mx - 1, RIB_Y + RIB_H - hh, 3, hh, CLR_LIGHT_YELLOW);
    }
    font(FONT_SMALL);
    print("x = pitch    y = pressure    A-K play", RIB_X + 2, RIB_Y + 1, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
