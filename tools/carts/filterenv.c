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
    "summary": "A Model-D-style dual-envelope rig: a FILTER contour (instrument_env + ENV_CUTOFF) AND a LOUDNESS contour (the amp ADSR) on one saw voice, each with its own sliders and graph.",
    "detail": "Two independent envelopes, like a Minimoog. The top graph is the FILTER sweep (cutoff snaps open on the attack, falls over the decay). The bottom graph is the AMP envelope (attack/decay/sustain/release) — what actually decides how LONG you hear the note: drop the sustain and the note plucks and dies inside the gate; raise it and the note rings until note-off (the red line). Drag the nine sliders and listen.",
    "controls": "A-K play notes - SPACE toggles the auto-arp - drag the FILTER and AMP sliders"
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// DUAL ENV — a Minimoog/Model-D-style two-envelope voice. A saw through a resonant
// lowpass with TWO independent envelopes:
//   FILTER contour (instrument_env + ENV_CUTOFF) — snaps the cutoff open on each note's
//     attack and lets it fall back: the "pew / dwow".
//   LOUDNESS contour (the amp ADSR in instrument()) — attack/decay/SUSTAIN/release: this
//     is what decides how LONG you hear the note. Low sustain → it plucks and dies inside
//     the gate; high sustain → it rings until note-off (the red line in the amp graph).
// Drag the sliders and listen. A–K play notes, SPACE toggles the auto-arp.

#define SLOT 5
#define GATE_MS 450               // how long each note is gated (note-on → note-off)

typedef struct { const char *name; float lo, hi, val; } Knob;
enum { F_ATK, F_DEC, F_AMT, F_CUT, F_RES,   A_ATK, A_DEC, A_SUS, A_REL, NK };
static Knob K[NK] = {
    // FILTER contour
    { "ATK",     0.f,  120.f,    0.f },   // filter-env attack (ms) — plucks want ~0
    { "DEC",    10.f,  600.f,  140.f },   // filter-env decay (ms) — how fast the cutoff closes
    { "AMT",-3500.f, 3500.f, 1800.f },    // env depth on cutoff (Hz). bipolar: + opens, - closes first
    { "CUT",   80.f, 2000.f,  320.f },    // base cutoff the env rides on top of (Hz)
    { "RES",    0.f,   15.f,    9.f },    // filter resonance — the squelch
    // LOUDNESS contour (amp ADSR)
    { "ATK",    0.f,  400.f,    2.f },    // amp attack (ms)
    { "DEC",    0.f,  600.f,  180.f },    // amp decay to sustain (ms)
    { "SUS",    0.f,    7.f,    2.f },    // amp SUSTAIN level (0..7) — THE note-length knob
    { "REL",    0.f,  800.f,  160.f },    // amp release after note-off (ms)
};

static int   active = -1;        // slider being dragged, or -1
static bool  auto_on = true;
static float arp_t = 0.f;
static int   arp_i = 0;
static float last_note_t = -9.f;

// slider layout: two groups (5 filter | 4 amp) in one row
#define SX 8
#define SW 26
#define SP 32
#define GAP 14
#define SY 116
#define SH 42

static int knob_x(int i) { return i < 5 ? SX + i * SP : SX + 5 * SP + GAP + (i - 5) * SP; }

static void apply(void) {
    instrument(SLOT, INSTR_SAW, (int)K[A_ATK].val, (int)K[A_DEC].val, (int)K[A_SUS].val, (int)K[A_REL].val);
    instrument_filter(SLOT, FILTER_LOW, (int)K[F_CUT].val, (int)K[F_RES].val);
    instrument_env(SLOT, 0, ENV_CUTOFF, (int)K[F_ATK].val, (int)K[F_DEC].val, K[F_AMT].val);
}

static void play(int midi) {
    hit(midi, SLOT, 5, GATE_MS);
    last_note_t = now();
}

void init(void) {
    bpm(112);
    apply();
}

void update(void) {
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

    // keyboard A S D F G H J K → pentatonic degrees
    const char keys[8] = { 'A','S','D','F','G','H','J','K' };
    for (int i = 0; i < 8; i++)
        if (keyp(keys[i])) play(degree(SCALE_PENTA, 3, i));
    if (keyp(KEY_SPACE)) auto_on = !auto_on;

    // auto arp so there's always something to judge by ear
    if (auto_on) {
        arp_t += dt();
        if (arp_t >= 0.30f) { arp_t = 0.f; play(degree(SCALE_PENTA, 3, arp_i)); arp_i = (arp_i + 1) % 8; }
    }
}

// the cutoff the FILTER env produces `s` seconds into a note (mirrors sound_ad_env, one-shot AD)
static float env_cutoff_at(float s) {
    float a = K[F_ATK].val / 1000.f, d = K[F_DEC].val / 1000.f, lvl;
    if (a > 0.f && s < a) lvl = s / a;
    else {
        float sd = s - a;
        lvl = (d <= 0.f || sd >= d) ? 0.f : expf(-4.0f * sd / d);
    }
    return K[F_CUT].val + lvl * K[F_AMT].val;
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
    print_right("filter + loudness", 312, 4, CLR_MAUVE);

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
        int gy = 15, gh = 42;
        rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
        rect(gx, gy, gw, gh, CLR_DARKER_GREY);

        float base = K[F_CUT].val, amt = K[F_AMT].val;
        float lo_c = base + (amt < 0.f ? amt : 0.f);
        float hi_c = base + (amt > 0.f ? amt : 0.f);
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
        int gy = 62, gh = 42;
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

    // ── sliders: two groups (FILTER | AMP) ────────────────────────────────────
    font(FONT_SMALL);
    print("FILTER ENV", knob_x(0) + 1, SY - 9, CLR_BLUE);
    print("AMP ENV (loudness)", knob_x(5) + 1, SY - 9, CLR_LIME_GREEN);
    font(FONT_NORMAL);
    for (int i = 0; i < NK; i++) {
        int cx = knob_x(i);
        int col = active == i ? CLR_YELLOW : (i < 5 ? CLR_TRUE_BLUE : CLR_GREEN);
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

    print(auto_on ? "auto-arp ON" : "auto-arp off", 8, SCREEN_H - 9, auto_on ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print_right("A-K play   SPACE arp", 312, SCREEN_H - 9, CLR_LIGHT_GREY);
}
