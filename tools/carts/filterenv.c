/* de:meta
{
  "title": "dual env",
  "status": "active",
  "created": "2026-06-03",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "adsr-envelope",
    "subtractive-synth"
  ],
  "lineage": "Sibling of pitchenv — the same mod-envelope system pointed at filter cutoff; the canonical tuning rig for the resonant-lowpass sweep (the pluck 'pew').",
  "description": {
    "summary": "A Minimoog/Model-D-style bass rig: TWO detuned saws through the Moog ladder filter, with a FILTER contour, a cutoff LFO, a LOUDNESS ADSR, plus DETUNE + DRIVE + a sub octave for the fat low-end 'humpf'. Each control has its own slider and a live graph.",
    "detail": "Subtractive-synth fattening, demonstrated. The voice is two sawtooths (instrument_tune detunes the second a few cents → they beat = thick) through FILTER_LADDER (the Moog 4-pole), warmed by instrument_drive in DRIVE_ASYM (even harmonics = round grit). Graph 1: the FILTER cutoff — the one-shot ENV snaps it open on the attack, the LFO wobbles it continuously. Graph 2: the AMP envelope — what decides how LONG you hear the note (low sustain plucks and dies, high sustain rings to note-off, the red line). Graph 3: a STILL wave-shape picture — one frozen cycle of the saw, ROUNDED by the filter (lower CUT) and clipped on one side by the DRIVE (asymmetric). It's a diagram, not a moving scope: drag CUT or DRV and watch the corners round off / the top flatten. DETUNE/DRIVE and the SUB OSC -1 toggle are where the humpf lives.",
    "controls": "A-K play notes - SPACE toggles the auto-arp - SUB OSC button top-right - drag the FILTER / LFO / AMP / VOICE sliders"
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// DUAL ENV — a Minimoog/Model-D-style bass voice + filter rig. Where the "humpf" comes from:
//   TWO detuned saws (SLOT + SLOT2, instrument_tune) — they beat against each other = thick.
//   FILTER_LADDER — the Moog 4-pole transistor ladder (24 dB/oct), creamier than a 2-pole.
//   DRIVE (DRIVE_ASYM) — even-harmonic saturation = round, fat grit.
//   SUB OSC -1 — the same wave an octave down for low-end body (the Osc-3-as-sub trick).
// On top of that, three modulators:
//   FILTER contour (instrument_env + ENV_CUTOFF) — snaps the cutoff open on the attack.
//   cutoff LFO (instrument_lfo + LFO_CUTOFF) — the continuous wobble.
//   LOUDNESS contour (the amp ADSR) — its SUSTAIN decides how LONG you hear the note.
// Drag the sliders and listen. A–K play (GarageBand layout), Z/X octave, SPACE toggles the arp.

#define SLOT  5
#define SLOT2 6                   // the 2nd, detuned saw — the other half of the fat
#define GATE_MS 450               // how long each note is gated (note-on → note-off)

typedef struct { const char *name; float lo, hi, val; } Knob;
enum { F_ATK, F_DEC, F_AMT, F_CUT, F_RES,   L_RATE, L_DEPTH,   A_ATK, A_DEC, A_SUS, A_REL,   V_DTN, V_DRV, NK };
static Knob K[NK] = {
    // FILTER contour
    { "ATK",     0.f,  120.f,    0.f },   // filter-env attack (ms) — plucks want ~0
    { "DEC",    10.f,  600.f,  140.f },   // filter-env decay (ms) — how fast the cutoff closes
    { "AMT",-3500.f, 3500.f, 1800.f },    // env depth on cutoff (Hz). bipolar: + opens, - closes first
    { "CUT",   80.f, 2000.f,  320.f },    // base cutoff the env rides on top of (Hz)
    { "RES",    0.f,   15.f,    9.f },    // filter resonance — the squelch
    // LFO on cutoff — the continuous wobble (per-voice: resets to phase 0 on each note-on)
    { "RTE",    0.f,   12.f,    5.f },    // LFO rate (Hz) — wobble speed
    { "DEP",    0.f, 3000.f,  600.f },    // LFO depth (Hz) — wobble amount on cutoff. 0 = off
    // LOUDNESS contour (amp ADSR)
    { "ATK",    0.f,  400.f,    2.f },    // amp attack (ms)
    { "DEC",    0.f,  600.f,  180.f },    // amp decay to sustain (ms)
    { "SUS",    0.f,    7.f,    2.f },    // amp SUSTAIN level (0..7) — THE note-length knob
    { "REL",    0.f,  800.f,  160.f },    // amp release after note-off (ms)
    // VOICE — the "humpf": detune the 2nd saw + saturate
    { "DTN",    0.f,   30.f,   12.f },    // detune of the 2nd saw (cents) — beating = fat. 0 = unison
    { "DRV",    0.f,  100.f,   25.f },    // drive (DRIVE_ASYM, %) — even-harmonic warmth/grit
};

static int   active = -1;        // slider being dragged, or -1
static bool  auto_on = true;
static bool  sub_on = false;     // sub-oscillator: same wave one octave down
static int   oct = 0;            // octave shift from Z/X (GarageBand-style)
static float arp_t = 0.f;
static int   arp_i = 0;
static float last_note_t = -9.f;

// sub-osc toggle button (top-right)
#define SUBX 252
#define SUBY 3
#define SUBW 60
#define SUBH 11

// slider layout: four groups (5 filter | 2 lfo | 4 amp | 2 voice) in one row
#define SX 2
#define SW 20
#define SP 23
#define GAP 6
#define SY 116
#define SH 42

static int knob_x(int i) {
    if (i < 5)  return SX + i * SP;                              // filter group
    if (i < 7)  return SX + 5 * SP + GAP + (i - 5) * SP;         // lfo group
    if (i < 11) return SX + 7 * SP + 2 * GAP + (i - 7) * SP;     // amp group
    return SX + 11 * SP + 3 * GAP + (i - 11) * SP;              // voice group
}

static void apply(void) {
    float det = K[V_DTN].val / 100.f;     // cents → semitones (instrument_tune is in semitones)
    float drv = K[V_DRV].val / 100.f;     // % → 0..1
    int slots[2] = { SLOT, SLOT2 };
    for (int s = 0; s < 2; s++) {         // two saws, the 2nd detuned → they beat = Moog-fat
        int sl = slots[s];
        instrument(sl, INSTR_SAW, (int)K[A_ATK].val, (int)K[A_DEC].val, (int)K[A_SUS].val, (int)K[A_REL].val);
        instrument_filter(sl, FILTER_LADDER, (int)K[F_CUT].val, (int)K[F_RES].val);   // the Moog 4-pole
        instrument_env(sl, 0, ENV_CUTOFF, (int)K[F_ATK].val, (int)K[F_DEC].val, K[F_AMT].val);
        instrument_lfo(sl, 0, LFO_CUTOFF, K[L_RATE].val, K[L_DEPTH].val);             // wobble rides on top
        instrument_drive_mode(sl, DRIVE_ASYM);                                        // even harmonics = fat
        instrument_drive(sl, drv);
        instrument_tune(sl, s == 0 ? 0.f : det);                                      // detune only the partner
    }
}

static void play(int midi) {
    hit(midi, SLOT,  4, GATE_MS);
    hit(midi, SLOT2, 4, GATE_MS);                                    // the detuned partner saw
    if (sub_on && midi - 12 >= 0) hit(midi - 12, SLOT, 3, GATE_MS);  // sub osc: same wave, 1 octave down
    last_note_t = now();
}

void init(void) {
    bpm(112);
    apply();
}

void update(void) {
    // sub-osc toggle button (top-right) — separate region from the sliders
    if (mouse_pressed(MOUSE_LEFT) && mouse_x() >= SUBX && mouse_x() < SUBX + SUBW
        && mouse_y() >= SUBY && mouse_y() < SUBY + SUBH)
        sub_on = !sub_on;

    // sliders (drag vertically)
    if (mouse_pressed(MOUSE_LEFT)) {
        active = -1;
        for (int i = 0; i < NK; i++) {
            int cx = knob_x(i);
            if (mouse_x() >= cx && mouse_x() < cx + SW && mouse_y() >= SY - 8 && mouse_y() <= SY + SH + 8)
                active = i;
        }
    }
    if (!mouse_down(MOUSE_LEFT)) active = -1;
    if (active >= 0) {
        float t = 1.0f - clamp((float)(mouse_y() - SY) / SH, 0.f, 1.f);
        K[active].val = K[active].lo + t * (K[active].hi - K[active].lo);
        apply();
    }

    // GarageBand "musical typing" layout: A=C, W=C#, S=D, E=D#, D=E, F=F, T=F#, G=G,
    // Y=G#, H=A, U=A#, J=B, K=C, O=C#, L=D, P=D# — a chromatic octave-and-a-bit. Z/X shift octave.
    static const char gbk[16] = { 'A','W','S','E','D','F','T','G','Y','H','U','J','K','O','L','P' };
    for (int i = 0; i < 16; i++)
        if (keyp(gbk[i])) play(60 + oct * 12 + i);
    if (keyp('Z') && oct > -3) oct--;
    if (keyp('X') && oct <  3) oct++;
    if (keyp(KEY_SPACE)) auto_on = !auto_on;

    // auto arp so there's always something to judge by ear (follows the Z/X octave)
    if (auto_on) {
        arp_t += dt();
        if (arp_t >= 0.30f) { arp_t = 0.f; play(degree(SCALE_PENTA, 3 + oct, arp_i)); arp_i = (arp_i + 1) % 8; }
    }
}

// the cutoff `s` seconds into a note: base + one-shot ENV (mirrors sound_ad_env) + the LFO wobble
// (sine, phase 0 at note-on — matches our per-voice retrigger), all summed onto the cutoff
static float env_cutoff_at(float s) {
    float a = K[F_ATK].val / 1000.f, d = K[F_DEC].val / 1000.f, lvl;
    if (a > 0.f && s < a) lvl = s / a;
    else {
        float sd = s - a;
        lvl = (d <= 0.f || sd >= d) ? 0.f : expf(-4.0f * sd / d);
    }
    float lfo = sinf(6.2831853f * K[L_RATE].val * s) * K[L_DEPTH].val;
    return K[F_CUT].val + lvl * K[F_AMT].val + lfo;
}

// the AMP loudness 0..1 `s` seconds into a note, gated at GATE_MS (ADSR)
static float amp_at(float s) {
    float a = K[A_ATK].val / 1000.f, d = K[A_DEC].val / 1000.f;
    float sus = K[A_SUS].val / 7.f, r = K[A_REL].val / 1000.f, gate = GATE_MS / 1000.f;
    float t = s < gate ? s : gate, lvl;            // ADS portion runs only while the key is held
    if (a > 0.f && t < a)            lvl = t / a;
    else if (d > 0.f && t < a + d)   lvl = sus + (1.f - sus) * expf(-4.0f * (t - a) / d);
    else                             lvl = sus;
    if (s > gate) lvl = (r <= 0.f) ? 0.f : lvl * expf(-4.0f * (s - gate) / r);   // release after note-off
    return lvl < 0.f ? 0.f : (lvl > 1.f ? 1.f : lvl);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("DUAL ENV", 8, 4, CLR_LIGHT_PEACH);

    // sub-osc toggle (top-right): layers the same wave one octave down
    rectfill(SUBX, SUBY, SUBW, SUBH, sub_on ? CLR_GREEN : CLR_BROWNISH_BLACK);
    rect(SUBX, SUBY, SUBW, SUBH, CLR_DARKER_GREY);
    font(FONT_SMALL);
    print("SUB OSC -1", SUBX + 5, SUBY + 3, sub_on ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // shared time axis: cover the whole note — gate + release tail (and at least the filter sweep)
    float gate = GATE_MS / 1000.f;
    float win = gate + K[A_REL].val / 1000.f + 0.04f;
    float fspan = (K[F_ATK].val + K[F_DEC].val) / 1000.f * 1.15f;
    if (win < fspan) win = fspan;
    if (win < 0.1f) win = 0.1f;
    int offx;                                          // note-off x, shared between strips

    int gx = 8, gw = 304;

    // ── strip 1: FILTER cutoff sweep ──────────────────────────────────────────
    {
        int gy = 15, gh = 28;
        rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
        rect(gx, gy, gw, gh, CLR_DARKER_GREY);

        float base = K[F_CUT].val, amt = K[F_AMT].val, dep = K[L_DEPTH].val;
        float lo_c = base + (amt < 0.f ? amt : 0.f) - dep;   // widen the range for the LFO swing
        float hi_c = base + (amt > 0.f ? amt : 0.f) + dep;
        if (lo_c < 20.f) lo_c = 20.f;
        if (hi_c - lo_c < 50.f) hi_c = lo_c + 50.f;
        #define CY(c) (gy + gh - 1 - (int)((clamp((c), lo_c, hi_c) - lo_c) / (hi_c - lo_c) * (gh - 2)))

        int by = CY(base);                                    // faint base-cutoff rest line
        for (int x = gx + 2; x < gx + gw - 2; x += 6) pset(x, by, CLR_DARK_GREY);

        int px = -1, py = -1;
        for (int x = 0; x < gw; x++) {
            int yy = CY(env_cutoff_at((x / (float)gw) * win));
            if (px >= 0) line(px, py, gx + x, yy, amt < 0.f ? CLR_ORANGE : CLR_BLUE);
            px = gx + x; py = yy;
        }
        offx = gx + (int)((gate / win) * gw);
        for (int y = gy + 1; y < gy + gh - 1; y += 4) pset(offx, y, CLR_RED);   // note-off marker

        float since = now() - last_note_t;
        if (since >= 0.f && since < win)
            circfill(gx + (int)((since / win) * gw), CY(env_cutoff_at(since)), 2, CLR_YELLOW);
        #undef CY

        font(FONT_SMALL);
        print("FILTER cutoff", gx + 3, gy + 2, CLR_BLUE);
        print(str("%d->%d Hz", (int)base, (int)(base + amt < 20.f ? 20.f : base + amt)), gx + 3, gy + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
    }

    // ── strip 2: AMP loudness contour (ADSR) ──────────────────────────────────
    {
        int gy = 46, gh = 28;
        rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
        rect(gx, gy, gw, gh, CLR_DARKER_GREY);
        #define AY(l) (gy + gh - 1 - (int)(clamp((l), 0.f, 1.f) * (gh - 2)))

        int sy = AY(K[A_SUS].val / 7.f);                      // faint sustain-level line
        for (int x = gx + 2; x < offx; x += 6) pset(x, sy, CLR_DARK_GREY);

        int px = -1, py = -1;
        for (int x = 0; x < gw; x++) {
            int yy = AY(amp_at((x / (float)gw) * win));
            if (px >= 0) line(px, py, gx + x, yy, CLR_LIME_GREEN);
            px = gx + x; py = yy;
        }
        for (int y = gy + 1; y < gy + gh - 1; y += 4) pset(offx, y, CLR_RED);   // note-off marker

        float since = now() - last_note_t;
        if (since >= 0.f && since < win)
            circfill(gx + (int)((since / win) * gw), AY(amp_at(since)), 2, CLR_YELLOW);
        #undef AY

        font(FONT_SMALL);
        print("AMP loudness", gx + 3, gy + 2, CLR_LIME_GREEN);
        print("note off", offx + 2 > gx + gw - 36 ? offx - 34 : offx + 2, gy + 2, CLR_RED);
        font(FONT_NORMAL);
    }

    // ── strip 3: WAVE SHAPE — one frozen cycle (a STILL picture, not a live scope) ──
    // a rising saw ROUNDED by the filter (lower CUT = more rounded) and clipped on ONE
    // side by the drive (DRV, asymmetric). Pure function of CUT + DRV, so it sits still
    // and only redraws when you drag those two — the wave we keep talking about.
    {
        int gy = 76, gh = 28, mid = gy + gh / 2;
        rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
        rect(gx, gy, gw, gh, CLR_DARKER_GREY);
        for (int x = gx + 2; x < gx + gw - 2; x += 6) pset(x, mid, CLR_DARK_GREY);   // zero line

        float bright = clamp((K[F_CUT].val - 80.f) / (2000.f - 80.f), 0.f, 1.f);
        float a = 0.05f + bright * 0.55f;                 // one-pole coeff: open filter → sharper (less rounding)
        float drv = K[V_DRV].val / 100.f, g = 1.f + drv * 6.f, bias = drv * 0.8f;
        int M = (gw - 2) / 2, N = M * 2;                  // two cycles across the panel
        static float w[512];
        float y = 0.f;
        for (int c = 0; c < 4; c++)                       // 2 warm-up cycles settle the filter, then keep 2
            for (int i = 0; i < M; i++) {
                float saw = 2.f * (i / (float)M) - 1.f;   // rising saw
                y += a * (saw - y);                       // one-pole lowpass = rounded corners (the filter)
                float s = tanhf(g * y + bias) - tanhf(bias);   // asymmetric drive = flatten one side
                if (c >= 2) w[(c - 2) * M + i] = s;
            }
        float pk = 0.001f;                                // normalize height so the SHAPE always reads
        for (int i = 0; i < N; i++) { float v = w[i] < 0 ? -w[i] : w[i]; if (v > pk) pk = v; }
        float lim = (float)(gh / 2 - 2), amp = (float)(gh / 2 - 3) / pk;
        int px = -1, py = -1;
        for (int i = 0; i < N; i++) {
            int xx = gx + 1 + i;
            int yy = mid - (int)(clamp(w[i] * amp, -lim, lim));
            if (px >= 0) line(px, py, xx, yy, CLR_YELLOW);
            px = xx; py = yy;
        }
        font(FONT_SMALL);
        print("WAVE SHAPE  (CUT rounds - DRV clips a side)", gx + 3, gy + 2, CLR_YELLOW);
        font(FONT_NORMAL);
    }

    // ── sliders: four groups (FILTER | LFO | AMP | VOICE) ──────────────────────
    font(FONT_SMALL);
    print("FILTER ENV", knob_x(0) + 1, SY - 9, CLR_BLUE);
    print("LFO", knob_x(5) + 1, SY - 9, CLR_MAUVE);
    print("AMP ENV", knob_x(7) + 1, SY - 9, CLR_LIME_GREEN);
    print("VOICE", knob_x(11) + 1, SY - 9, CLR_ORANGE);
    font(FONT_NORMAL);
    for (int i = 0; i < NK; i++) {
        int cx = knob_x(i);
        int col = active == i ? CLR_YELLOW
                : i < 5 ? CLR_TRUE_BLUE : i < 7 ? CLR_MAUVE : i < 11 ? CLR_GREEN : CLR_ORANGE;
        rect(cx, SY, SW, SH, CLR_DARKER_GREY);
        float t = (K[i].val - K[i].lo) / (K[i].hi - K[i].lo);
        int vy = SY + SH - 1 - (int)(t * (SH - 2));
        if (K[i].lo < 0.f) {                                 // bipolar knob: fill from the zero line
            float t0 = (0.f - K[i].lo) / (K[i].hi - K[i].lo);
            int zy = SY + SH - 1 - (int)(t0 * (SH - 2));
            int top = vy < zy ? vy : zy, h = (vy < zy ? zy - vy : vy - zy) + 1;
            rectfill(cx + 1, top, SW - 2, h, col);
            line(cx + 1, zy, cx + SW - 2, zy, CLR_LIGHT_GREY);
        } else {
            int fh = (int)(t * (SH - 2));
            rectfill(cx + 1, SY + SH - 1 - fh, SW - 2, fh, col);
        }
        font(FONT_SMALL);
        print(K[i].name, cx + 2, SY + SH + 2, CLR_LIGHT_GREY);
        print(str("%d", (int)K[i].val), cx + 2, SY + SH + 9, CLR_WHITE);
        font(FONT_NORMAL);
    }

    font(FONT_SMALL);
    print(auto_on ? "auto-arp ON" : "auto-arp off", 8, SCREEN_H - 7, auto_on ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print(str("oct %+d", oct), 72, SCREEN_H - 7, CLR_MAUVE);
    print_right("GarageBand keys  Z/X oct  SPACE arp", 312, SCREEN_H - 7, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}
