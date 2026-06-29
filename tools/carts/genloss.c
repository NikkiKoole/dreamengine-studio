/* de:meta
{
  "title": "generation loss",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "VHS generation-loss / dubbing degradation chain (crush → tape → dropout); showcases the new dropout() effect from the boutique-pedals roadmap, with a visual VHS tape transport.",
  "description": "The showcase for dropout() - the VHS / Generation-Loss FAILURE knob. A self-playing wistful EP loop runs through the full degraded-cassette chain: crush() (bit + sample-rate reduction) into tape() (wow/flutter/saturation) into dropout(). The new piece is dropout: a sample-and-hold clock randomly TRIGGERS momentary tape-catches where the whole mix stumbles - drops in level AND goes dull from HF loss - then recovers fast, exactly like a tape dubbed one too many times. Three sliders: FAILURE (how often the tape catches), DEPTH (how hard each catch drops), DEGRADE (the bit-crush + tape grime baseline). A VHS tape-transport display turns its reels and tears its tracking when a catch fires. SPACE pauses the loop; H for help. Built on dropout() + the modulation kit's sample-and-hold (mod_sh) from the boutique-pedals roadmap. Part of the lo-fi / effects family."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── GENERATION LOSS ─────────────────────────────────────────────────────────
// The showcase for dropout() — the VHS / Generation-Loss "FAILURE" knob. A self-
// playing lo-fi loop runs through the full degraded-cassette chain:
//      crush()  (bit + sample-rate reduction)  →  tape()  (wow/flutter/sat)  →  dropout()
// dropout() is the new piece: a sample-&-hold clock randomly TRIGGERS momentary
// tape-catches where the whole mix stumbles — drops in level AND goes dull — then
// recovers. The other three were already ours; the catches are what make it sound
// like a tape that's been dubbed one too many times.
//
//   FAILURE  how OFTEN the tape catches (dropout amount)
//   DEPTH    how HARD each catch drops (level + HF loss)
//   DEGRADE  bit-crush + tape wow/flutter amount (the baseline grime)
//   SPACE    pause/resume the loop      H  help

#define SL_KEYS  8

static float k_fail   = 0.45f;
static float k_depth  = 0.70f;
static float k_degr   = 0.45f;
static bool  playing  = true;
static bool  show_help = false;

static float a_fail = -1, a_depth = -1, a_degr = -1;

// a wistful minor loop (A minor pentatonic-ish), played as a gentle EP arp
static const int SEQ[16] = { 57, 60, 64, 67, 64, 60, 62, 57, 55, 60, 64, 69, 67, 64, 60, 55 };
static int   step = 0;
static float steptmr = 0.0f;

// VHS visual: a local LCG drives decorative tracking glitches (visual only, frame-stepped)
static unsigned int vseed = 0x9E3779B9u;
static float reel = 0.0f, glitch = 0.0f, track_y = 0.0f;
static float vrand(void) { vseed = vseed * 1103515245u + 12345u; return (float)((vseed >> 16) & 0xFFFF) / 65535.0f; }

void update(void) {
    float dt = 1.0f / 60.0f;
    if (keyp(KEY_SPACE)) playing = !playing;
    if (keyp('H')) show_help = !show_help;

    // SET-AND-HOLD: re-push the chain only when a knob moved
    if (k_fail != a_fail || k_depth != a_depth || k_degr != a_degr) {
        crush(16.0f - k_degr * 10.0f, 1.0f + k_degr * 6.0f, 0.35f + k_degr * 0.4f);   // 16→6 bits + downsample
        tape(k_degr * 0.6f, k_degr * 0.45f, 0.2f + k_degr * 0.4f);                     // wow/flutter/sat
        dropout(k_fail, k_depth);                                                       // the FAILURE knob
        a_fail = k_fail; a_depth = k_depth; a_degr = k_degr;
    }

    // self-playing arp
    if (playing) {
        steptmr += dt;
        if (steptmr >= 0.18f) {
            steptmr -= 0.18f;
            hit(SEQ[step], SL_KEYS, 5, 300);
            step = (step + 1) & 15;
        }
    }

    reel += dt * 2.2f;
    glitch *= 0.85f;                         // glitch flash decays
    if (vrand() < 0.04f + k_fail * 0.10f) {  // FAILURE feeds the visual catch-rate too
        glitch = 0.6f + vrand() * 0.4f;
        track_y = vrand();
    }
}

static void reel_at(int cx, int cy) {
    circ(cx, cy, 11, CLR_MEDIUM_GREY);
    circfill(cx, cy, 4, CLR_LIGHT_GREY);
    float a = reel;
    for (int i = 0; i < 3; i++, a += 2.094f)
        line(cx, cy, cx + (int)(cosf(a) * 9), cy + (int)(sinf(a) * 9), CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // the "tape" window — scanlines + a wandering tracking band + glitch tear
    int wx = 18, wy = 22, ww = SCREEN_W - 36, wh = 70;
    rectfill(wx, wy, ww, wh, CLR_BLACK);
    for (int y = wy; y < wy + wh; y += 2) {
        int off = (glitch > 0.2f && y > wy + (int)(track_y * wh) && y < wy + (int)(track_y * wh) + 10)
                  ? (int)((vrand() - 0.5f) * glitch * 30) : 0;   // a torn slice during a catch
        line(wx + off, y, wx + ww + off, y, y % 4 ? CLR_DARK_BLUE : CLR_DARKER_GREY);
    }
    // a tracking-noise band
    int ty = wy + (int)(track_y * (wh - 6));
    rectfill(wx, ty, ww, 3, glitch > 0.3f ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    reel_at(wx + 30, wy + wh / 2);
    reel_at(wx + ww - 30, wy + wh / 2);
    print_centered(glitch > 0.4f ? "TRACKING" : "GEN LOSS", SCREEN_W / 2, wy + wh / 2 - 4, glitch > 0.4f ? CLR_PEACH : CLR_LIGHT_PEACH);
    print(playing ? "PLAY" : "PAUSE", wx + 4, wy + 3, CLR_MEDIUM_GREY);

    ui_begin();
    int sy = SCREEN_H - 40, sw = 92;
    ui_slider(&k_fail,  8,            sy, sw, "FAILURE");
    ui_slider(&k_depth, 8 + sw + 10,  sy, sw, "DEPTH");
    ui_slider(&k_degr,  8 + (sw+10)*2, sy, sw, "DEGRADE");
    ui_end();

    font(FONT_SMALL);
    print("crush -> tape -> DROPOUT (the FAILURE knob)", 8, SCREEN_H - 52, CLR_MEDIUM_GREY);
    print("SPACE play/pause   H help", 8, SCREEN_H - 7, CLR_DARK_GREY);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(24, 26, SCREEN_W - 48, 92, CLR_BLACK);
        rect(24, 26, SCREEN_W - 48, 92, CLR_PEACH);
        print("dropout(): the FAILURE knob", 32, 34, CLR_WHITE);
        print("random tape-CATCHES: the mix", 32, 48, CLR_LIGHT_GREY);
        print("stumbles (level + HF drop),", 32, 58, CLR_LIGHT_GREY);
        print("then recovers. crush + tape", 32, 68, CLR_LIGHT_GREY);
        print("+ dropout = a worn cassette.", 32, 78, CLR_LIGHT_GREY);
        print("FAILURE=how often DEPTH=how hard", 32, 92, CLR_PEACH);
        print("H to close", 32, 106, CLR_DARK_GREY);
    }
}

void init(void) {
    instrument(SL_KEYS, INSTR_EPIANO, 2, 180, 4, 260);
    bpm(96);
}
