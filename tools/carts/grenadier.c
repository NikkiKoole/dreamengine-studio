/* de:meta
{
  "slug": "grenadier",
  "title": "Grenadier",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "subtractive-synth",
    "analog-voice-modeling"
  ],
  "lineage": "A cart-side port of the Grendel RA-99 Grenadier semi-modular filterbank — three parallel resonant filters on stacked held voices, with CMOS randomization drift modeling the hardware's character.",
  "homage": "Grendel RA-99 Grenadier (Rare Waves, 2020)",
  "description": "A triple voltage-controlled FILTERBANK drone/acid synth, after the Grendel RA-99 Grenadier (Rare Waves, 2020) — a CMOS semi-modular box whose whole voice is THREE parallel resonant filters swept together in a 2D space (the TB-303 filter is its tonal cousin). Ported entirely cart-side (no new engine DSP): three held voices on one root, each through its own resonant filter, summed. The XY PAD is the instrument — ALPHA (x) sweeps all three filters ±2 octaves, BETA (y) opens the SPACING from a tight cluster to a wide spread; a live frequency-response strip shows the three peaks move. LAYOUT switches RA-99 (lowpass + 2 bandpass) vs RA-9 (three bandpass). The in-scale KEYBED (A S D F G H J K L) sets the drone ROOT and glides to it. SPACE / Q / MORPH (a trapezoid VCO morphing triangle→square) / RND (per-trigger CMOS drift). GATE pulses the drone in time for the techno chug. Drag the SWEEP pad; ASDFGHJKL = root; ↑/↓ gate rate; ← / → tempo."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// GRENADIER — a triple voltage-controlled FILTERBANK, after the Grendel RA-99 Grenadier
// (Rare Waves, 2020): a CMOS semi-modular drone/acid box whose voice is THREE parallel
// resonant filters swept together in a 2D ALPHA/BETA space (the TB-303 filter is its
// tonal cousin). Ported cart-side — no new engine DSP. Where the hardware splits ONE
// oscillator into three filters, we stack three HELD voices on the same root, each
// through its own resonant filter, and ride their cutoffs live; the mixer sums them.
//
//   • the XY PAD is the instrument: ALPHA (x) sweeps all three filters ±2 octaves,
//     BETA (y) opens the SPACING from a tight cluster to a wide spread. This is the move.
//   • LAYOUT — RA-99 (lowpass + 2 bandpass) or RA-9 (three bandpass), per the two models.
//   • the KEYBED (A S D F G H J K L) sets the drone ROOT, scale-locked; it GLIDES there.
//   • SPACE / Q / MORPH (the trapezoid VCO: triangle→square) / RND (per-trigger CMOS drift).
//   • GATE pulses the drone in time (the techno chug); ↑/↓ rate, ← / → tempo.
//
//   Drag the XY pad with the mouse/finger. The strip up top shows the three filters live.

enum { SL_F1 = 5, SL_F2, SL_F3 };          // the three filterbank voices (same root)
#define NV 3
static int  hv[NV] = { -1, -1, -1 };       // held-note handles

// in-scale roots — minor pentatonic, like onenote's keybed
static const int PENTA[5] = { 0, 3, 5, 7, 10 };
#define NDEG 5
#define NKEYS 9
#define BASE_MIDI 33                        // A1 drone root
static int deg_semi(int d) { return 12 * (d / NDEG) + PENTA[d % NDEG]; }
static const int KB_KEY[NKEYS] = { 'A','S','D','F','G','H','J','K','L' };
#define KB_X 6
#define KB_Y 62
#define KB_W 22
#define KB_H 30

#define XY_X 210
#define XY_Y 62
#define XY_W 102
#define XY_H 118

// frequency-response strip
#define SR_X 6
#define SR_Y 16
#define SR_W 308
#define SR_H 34

static int   cur_deg = 0, pending_deg = 0;
static int   keyflash = -100;
static bool  ra9 = false;                   // false = RA-99 (LP+BP+BP), true = RA-9 (3×BP)
static int   tempo = 120, gate_div = 4;     // gate_div: 2=8th, 4=16th
static bool  gate_on = true;

// pad + knobs (0..1)
static float xy_x = 0.45f, xy_y = 0.40f;    // ALPHA, BETA
static float k_base = 0.45f, k_space = 0.45f, k_q = 0.55f, k_morph = 0.55f, k_rnd = 0.0f;

// per-trigger CMOS randomization (recomputed on each root change)
static unsigned rng = 0x1234567u;
static float frand(void) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; }
static float jbase = 1, jspace = 1, jf[NV] = { 1, 1, 1 }, jq[NV] = { 1, 1, 1 };
static void reroll(void) {                  // ±1 oct base, ±15% space, ±30% Q, per the doc
    float a = k_rnd;
    jbase  = powf(2.0f, (frand() * 2 - 1) * a);
    jspace = 1 + (frand() * 2 - 1) * 0.15f * a;
    for (int i = 0; i < NV; i++) {
        jf[i] = powf(2.0f, (frand() * 2 - 1) * 0.15f * a);
        jq[i] = 1 + (frand() * 2 - 1) * 0.30f * a;
    }
}

// the live filterbank frequencies from ALPHA/BETA (the RA-99 mapping)
static void bank_freqs(float f[NV]) {
    float baseHz   = 80.0f * powf(3000.0f / 80.0f, k_base);          // BASE knob 80..3000 Hz, log
    float base_eff = baseHz * powf(2.0f, (xy_x - 0.5f) * 4.0f) * jbase;   // ALPHA = ±2 oct
    float space    = 1.0f + (1.0f + k_space * 2.5f - 1.0f);          // SPACE knob → 1.0..3.5×
    float space_eff = 1.0f + (space - 1.0f) * (0.3f + xy_y * 0.7f);  // BETA opens the spread
    f[0] = base_eff * jf[0];
    f[1] = base_eff * space_eff * jf[1];
    f[2] = base_eff * space_eff * space_eff * jf[2];
    for (int i = 0; i < NV; i++) f[i] = f[i] < 20 ? 20 : f[i] > 18000 ? 18000 : f[i];
}

// rebuild the trapezoid VCO cycle (triangle → square) into INSTR_USER0
static void set_morph(void) {
    float buf[64], gain = 1.0f / (1.0f - k_morph * 0.99f + 0.01f);
    for (int i = 0; i < 64; i++) {
        float ph = i / 64.0f;
        float tri = (ph < 0.5f) ? (-1.0f + 4.0f * ph) : (3.0f - 4.0f * ph);  // -1..1 triangle
        float v = tri * gain;
        buf[i] = v < -1 ? -1 : v > 1 ? 1 : v;
    }
    wave_set(0, buf, 64);
}

static void set_root(int d) {                  // keybed → QUEUE a root; the drone glides to it
    if (d < 0) d = 0; if (d > NKEYS - 1) d = NKEYS - 1;
    pending_deg = d;
    keyflash = frame();
}

void init() {
    set_morph();
    for (int i = 0; i < NV; i++) {
        instrument(SL_F1 + i, INSTR_USER0, 8, 0, 7, 240);   // a sustained drone voice
        instrument_filter(SL_F1 + i, (i == 0 && !ra9) ? FILTER_LOW : FILTER_BAND, 600, 8);
        instrument_drive(SL_F1 + i, 0.35f);                 // CMOS grit + harmonics for the bandpasses to grab
    }
    reroll();
    for (int i = 0; i < NV; i++) {
        hv[i] = note_on(SL_F1 + i, BASE_MIDI, 3);           // three voices, same root, held
        note_glide(hv[i], 60);                              // portamento between roots
    }
}

void update() {
    if (btnp(0, BTN_LEFT))  tempo = max(70,  tempo - 4);
    if (btnp(0, BTN_RIGHT)) tempo = min(180, tempo + 4);
    if (btnp(0, BTN_UP))    gate_div = (gate_div == 2) ? 4 : (gate_div == 4) ? 8 : 2;
    if (btnp(0, BTN_DOWN))  gate_div = (gate_div == 8) ? 4 : (gate_div == 4) ? 2 : 8;
    bpm(tempo);

    // keybed — type or tap a root (queued; latches on the next gate pulse)
    for (int i = 0; i < NKEYS; i++) if (keyp(KB_KEY[i])) set_root(i);
    for (int i = 0; i < NKEYS; i++) if (tapp(KB_X + i * KB_W, KB_Y, KB_W - 1, KB_H)) set_root(i);

    // XY pad — drag ALPHA/BETA
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= XY_X && tx < XY_X + XY_W && ty >= XY_Y && ty < XY_Y + XY_H) {
            xy_x = clamp((tx - XY_X) / (float)XY_W, 0, 1);
            xy_y = clamp(1.0f - (ty - XY_Y) / (float)XY_H, 0, 1);
        }
    }

    // gate clock — pulse the drone in time; the root latches on each fresh pulse
    float steps = beat() * gate_div + beat_pos() * gate_div;
    static int last_pulse = -1;
    int pulse = (int)steps;
    float env = 1.0f;
    if (gate_on) {
        if (pulse != last_pulse) {                          // a new pulse begins
            last_pulse = pulse;
            if (pending_deg != cur_deg) { cur_deg = pending_deg; reroll(); }   // glide to the queued root
            for (int i = 0; i < NV; i++) note_pitch(hv[i], BASE_MIDI + deg_semi(cur_deg));
        }
        env = expf(-(steps - pulse) * 3.2f);                // a plucky decay each pulse
    } else if (pending_deg != cur_deg) {
        cur_deg = pending_deg; reroll();
        for (int i = 0; i < NV; i++) note_pitch(hv[i], BASE_MIDI + deg_semi(cur_deg));
    }

    // re-shape the trapezoid only when MORPH moved (set-and-hold)
    static float aMorph = -1;
    if (k_morph != aMorph) { set_morph(); aMorph = k_morph; }

    // re-apply filter layout only when the toggle changed
    static int aRa9 = -1;
    if ((int)ra9 != aRa9) {
        for (int i = 0; i < NV; i++)
            note_filter(hv[i], (i == 0 && !ra9) ? FILTER_LOW : FILTER_BAND);
        aRa9 = ra9;
    }

    // DRIVE the bank live — note_cutoff / note_res / note_vol are built to ride every frame
    float f[NV]; bank_freqs(f);
    float Q = 1.0f + k_q * 13.0f;                           // 1..14
    for (int i = 0; i < NV; i++) {
        note_cutoff(hv[i], (int)f[i]);
        note_res(hv[i], Q * jq[i]);
        note_vol(hv[i], 7.0f * env);
    }

#ifdef DE_TRACE
    watch("alpha", "%.2f", xy_x);
    watch("beta",  "%.2f", xy_y);
    watch("f1",    "%d", (int)f[0]);
    watch("f3",    "%d", (int)f[2]);
    watch("root",  "%d", deg_semi(cur_deg));
#endif
}

static const char *NOTE_NAMES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const char *note_name(int m) { return str("%s%d", NOTE_NAMES[m % 12], m / 12 - 1); }

static int freq_x(float hz) {                               // log map 20..18000 Hz → strip x
    float t = logf(hz / 20.0f) / logf(18000.0f / 20.0f);
    return SR_X + (int)(t * SR_W);
}

void draw() {
    cls(CLR_BROWNISH_BLACK);
    print("GRENADIER", 2, 4, CLR_LIGHT_YELLOW);
    print_right(str("%s  %dBPM  1/%d", ra9 ? "RA-9" : "RA-99", tempo, gate_div), 318, 4, CLR_LIGHT_GREY);

    // ── frequency-response strip: the three filters, live ──
    rectfill(SR_X, SR_Y, SR_W, SR_H, CLR_DARKER_GREY);
    for (int o = 0; o < 4; o++) {                           // decade-ish gridlines
        int gx = freq_x(20.0f * powf(10.0f, o));
        if (gx < SR_X + SR_W) line(gx, SR_Y, gx, SR_Y + SR_H, CLR_DARK_GREY);
    }
    float f[NV]; bank_freqs(f);
    float Q = 1.0f + k_q * 13.0f;
    int fcol[NV] = { CLR_ORANGE, CLR_GREEN, CLR_BLUE };
    for (int i = 0; i < NV; i++) {
        int px = freq_x(f[i]);
        int h = (int)(8 + Q / 14.0f * (SR_H - 10));         // taller peak = higher Q
        int w = (int)(14 - Q / 14.0f * 9);                  // higher Q = narrower
        bool lp = (i == 0 && !ra9);
        for (int dx = -w; dx <= w; dx++) {                  // a little resonant bump
            int x = px + dx; if (x < SR_X || x >= SR_X + SR_W) continue;
            int hh = (int)(h * (1.0f - fabsf((float)dx) / (float)(w + 1)));
            if (lp) { for (int xx = SR_X; xx < px; xx++) pset(xx, SR_Y + SR_H - 4, fcol[i]); } // LP shelf hint
            line(x, SR_Y + SR_H - 1, x, SR_Y + SR_H - 1 - hh, fcol[i]);
        }
    }
    rect(SR_X, SR_Y, SR_W, SR_H, CLR_MEDIUM_GREY);
    font(FONT_SMALL);
    print("FILTERBANK", SR_X + 2, SR_Y + 1, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // ── keybed (root) ──
    font(FONT_SMALL);
    print("ROOT", KB_X, KB_Y - 9, CLR_LIGHT_GREY);
    print(note_name(BASE_MIDI + deg_semi(cur_deg)), KB_X + 30, KB_Y - 9, CLR_BLUE);
    if (pending_deg != cur_deg)
        print(str("> %s", note_name(BASE_MIDI + deg_semi(pending_deg))), KB_X + 64, KB_Y - 9, CLR_YELLOW);
    bool kglow = (frame() - keyflash) < 8;
    for (int i = 0; i < NKEYS; i++) {
        int x = KB_X + i * KB_W;
        bool armed = (i == pending_deg), playing = (i == cur_deg);
        int col = armed ? (kglow ? CLR_WHITE : CLR_BLUE) : CLR_DARK_GREY;
        rectfill(x, KB_Y, KB_W - 1, KB_H, col);
        rect(x, KB_Y, KB_W - 1, KB_H, CLR_MEDIUM_GREY);
        if (playing) rectfill(x + 2, KB_Y + 2, KB_W - 5, 4, armed ? CLR_BROWNISH_BLACK : CLR_GREEN);
        char lbl[2] = { (char)KB_KEY[i], 0 };
        print(lbl, x + KB_W / 2 - 2, KB_Y + KB_H - 9, armed ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
    }
    font(FONT_NORMAL);

    // ── the XY pad — ALPHA / BETA ──
    rectfill(XY_X, XY_Y, XY_W, XY_H, CLR_DARKER_GREY);
    for (int g = 1; g < 4; g++) {
        line(XY_X + g * XY_W / 4, XY_Y, XY_X + g * XY_W / 4, XY_Y + XY_H, CLR_DARK_GREY);
        line(XY_X, XY_Y + g * XY_H / 4, XY_X + XY_W, XY_Y + g * XY_H / 4, CLR_DARK_GREY);
    }
    rect(XY_X, XY_Y, XY_W, XY_H, CLR_MEDIUM_GREY);
    int dx = XY_X + (int)(xy_x * XY_W), dy = XY_Y + (int)((1 - xy_y) * XY_H);
    line(XY_X, dy, XY_X + XY_W, dy, CLR_DARKER_BLUE);
    line(dx, XY_Y, dx, XY_Y + XY_H, CLR_DARKER_BLUE);
    circfill(dx, dy, 4, CLR_PINK); circ(dx, dy, 4, CLR_WHITE);
    font(FONT_SMALL);
    print("SWEEP", XY_X, XY_Y - 9, CLR_LIGHT_GREY);
    print("alpha >", XY_X + 2, XY_Y + XY_H + 2, CLR_DARK_GREY);
    print("beta ^", XY_X + XY_W - 28, XY_Y - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ── knobs + toggles ──
    ui_begin();
    font(FONT_SMALL);
    int ky = 124;
    ui_knob(&k_base,  22, ky, "BASE");
    ui_knob(&k_space, 58, ky, "SPACE");
    ui_knob(&k_q,     94, ky, "Q");
    ui_knob(&k_morph, 130, ky, "MORPH");
    ui_knob(&k_rnd,   166, ky, "RND");
    if (ui_button(6,  148, 78, 16, ra9 ? "LAYOUT RA-9" : "LAYOUT RA-99")) ra9 = !ra9;
    if (ui_button(90, 148, 64, 16, gate_on ? "GATE *" : "GATE")) gate_on = !gate_on;
    hint("ASDFGHJKL root  -  drag SWEEP  -  up/dn gate rate  -  L/R tempo");
    font(FONT_NORMAL);
    ui_end();
}
