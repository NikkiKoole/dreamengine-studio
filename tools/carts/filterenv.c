/* de:meta
{
  "title": "filter env",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "adsr-envelope",
    "subtractive-synth"
  ],
  "lineage": "Sibling of pitchenv — the same mod-envelope system pointed at filter cutoff; the canonical tuning rig for the resonant-lowpass sweep (the pluck 'pew').",
  "description": "The pluck \"pew\" — a saw run through a resonant lowpass with a one-shot FILTER envelope (instrument_env + ENV_CUTOFF) that snaps the cutoff open on each note's attack and lets it fall back over the decay. The graph shows the cutoff sweep; drag the five sliders (attack, decay, amount, base cutoff, resonance) and listen. A tuning rig for the new modulation envelopes. A-K play notes, SPACE toggles the auto-arp."
}
de:meta */
#include "studio.h"
#include <math.h>

// FILTER ENV — the pluck "pew". A saw run through a resonant lowpass, with a one-shot
// filter envelope (instrument_env + ENV_CUTOFF) that snaps the cutoff OPEN on every
// note's attack and lets it fall back over the decay. That sweep is the whole "pew /
// dwow" of a plucky synth bass. Drag the sliders and listen — this is the tuning rig
// for the new mod-envelope. A–K play notes, SPACE toggles the auto-arp.

#define SLOT 5

typedef struct { const char *name; float lo, hi, val; } Knob;
enum { K_ATK, K_DECAY, K_AMOUNT, K_CUTOFF, K_RESO, NK };
static Knob K[NK] = {
    { "ATK",     0.f,  120.f,    0.f },   // filter-env attack (ms) — plucks want ~0
    { "DECAY",  10.f,  600.f,  140.f },   // filter-env decay (ms) — how fast it closes
    { "AMOUNT",-3500.f, 3500.f, 1800.f }, // env depth on cutoff (Hz). bipolar: + opens, - closes first
    { "CUTOFF", 80.f, 2000.f,  320.f },   // base cutoff the env rides on top of (Hz)
    { "RESO",    0.f,   15.f,    9.f },   // filter resonance — the squelch
};

static int   active = -1;        // slider being dragged, or -1
static bool  auto_on = true;
static float arp_t = 0.f;
static int   arp_i = 0;
static float last_note_t = -9.f;

// slider layout: 5 columns across the lower half
#define SX 12
#define SPITCH 60
#define SW 44
#define SY 110
#define SH 56

static void apply(void) {
    instrument(SLOT, INSTR_SAW, 2, 0, 6, 160);                                  // bright, sustains through the gate
    instrument_filter(SLOT, FILTER_LOW, (int)K[K_CUTOFF].val, (int)K[K_RESO].val);
    instrument_env(SLOT, 0, ENV_CUTOFF, (int)K[K_ATK].val, (int)K[K_DECAY].val, K[K_AMOUNT].val);
}

static void play(int midi) {
    hit(midi, SLOT, 5, 450);
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
            int cx = SX + i * SPITCH;
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

// the cutoff the filter env produces `s` seconds into a note (mirrors sound_ad_env)
static float env_cutoff_at(float s) {
    float a = K[K_ATK].val / 1000.f, d = K[K_DECAY].val / 1000.f, lvl;
    if (a > 0.f && s < a) lvl = s / a;
    else {
        float sd = s - a;
        lvl = (d <= 0.f || sd >= d) ? 0.f : expf(-4.0f * sd / d);
    }
    return K[K_CUTOFF].val + lvl * K[K_AMOUNT].val;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("FILTER ENV", 8, 5, CLR_LIGHT_PEACH);
    print_right("the pluck \"pew\"", 312, 5, CLR_MAUVE);

    // envelope graph — cutoff over time
    int gx = 8, gy = 20, gw = 304, gh = 80;
    rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
    rect(gx, gy, gw, gh, CLR_DARKER_GREY);

    float span = (K[K_ATK].val + K[K_DECAY].val) / 1000.f;
    if (span < 0.05f) span = 0.05f;
    span *= 1.15f;

    // vertical range spans the whole sweep — with a negative amount the cutoff dips
    // BELOW the base, so the range runs base+amount .. base (the engine floors it at 20 Hz)
    float base = K[K_CUTOFF].val, amt = K[K_AMOUNT].val;
    float lo_c = base + (amt < 0.f ? amt : 0.f);
    float hi_c = base + (amt > 0.f ? amt : 0.f);
    if (lo_c < 20.f) lo_c = 20.f;
    if (hi_c - lo_c < 50.f) hi_c = lo_c + 50.f;
    #define CY(c) (gy + gh - 1 - (int)((clamp((c), lo_c, hi_c) - lo_c) / (hi_c - lo_c) * (gh - 2)))

    // faint base-cutoff line (where the cutoff rests between notes)
    int by = CY(base);
    for (int x = gx + 2; x < gx + gw - 2; x += 6) pset(x, by, CLR_DARK_GREY);

    // the sweep curve
    int px = -1, py = -1;
    for (int x = 0; x < gw; x++) {
        int yy = CY(env_cutoff_at((x / (float)gw) * span));
        if (px >= 0) line(px, py, gx + x, yy, amt < 0.f ? CLR_ORANGE : CLR_BLUE);
        px = gx + x; py = yy;
    }

    // playhead riding the curve since the last note
    float since = now() - last_note_t;
    if (since >= 0.f && since < span) {
        int hx = gx + (int)((since / span) * gw);
        circfill(hx, CY(env_cutoff_at(since)), 2, CLR_YELLOW);
    }
    #undef CY
    print("cutoff", gx + 3, gy + 2, CLR_DARK_GREY);
    print(str("%d -> %d Hz", (int)base, (int)(base + amt < 20.f ? 20.f : base + amt)), gx + 3, gy + 10, CLR_DARK_GREY);

    // sliders
    for (int i = 0; i < NK; i++) {
        int cx = SX + i * SPITCH, col = active == i ? CLR_YELLOW : CLR_TRUE_BLUE;
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
        print(K[i].name, cx + 2, SY + SH + 3, CLR_LIGHT_GREY);
        print(str("%d", (int)K[i].val), cx + 2, SY + SH + 11, CLR_WHITE);
    }

    print(auto_on ? "auto-arp ON" : "auto-arp off", 8, SCREEN_H - 18, auto_on ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print("drag sliders   A-K play   SPACE arp", 8, SCREEN_H - 9, CLR_LIGHT_GREY);
}
