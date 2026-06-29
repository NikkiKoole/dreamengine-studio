/* de:meta
{
  "title": "drivemodes",
  "status": "active",
  "created": "2026-06-11",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "The four flavours of the drive() grit knob, side by side on one growling bass. instrument_drive_mode() picks the waveshaper - SOFT (tanh, warm tube overdrive), HARD (hard clip, buzzy digital fuzz), FOLD (sine wavefolder, metallic and glassy), ASYM (asymmetric tube, adds the fat even harmonics) - while instrument_drive() stays the amount. An auto-riff keeps the bass going so you hear the mode flip live, with a representative waveshape drawn under each panel. All four are a true bypass at drive 0 and hold full-scale loudness, so they change character not volume. LEFT/RIGHT (or tap a panel) picks the mode, UP/DOWN rides the drive amount, Z plucks."
}
de:meta */
// drivemodes — the four flavours of drive() on one growling bass note.
// instrument_drive_mode() picks the waveshaper; instrument_drive() is the amount.
//   ← → : pick mode      ↑ ↓ : drive amount      Z : pluck now
// An auto-riff keeps the note going so you can hear the mode change live.
#include "studio.h"
#include <stdio.h>
#include <math.h>

#define I_BASS 0

static const char *MODE_NAME[4] = { "SOFT",  "HARD",  "FOLD",  "ASYM"  };
static const char *MODE_DESC[4] = { "tanh \x1a warm tube overdrive",
                                    "hard clip \x1a buzzy digital fuzz",
                                    "sine wavefolder \x1a metallic",
                                    "asymmetric \x1a even-harmonic tube" };

static int   mode = 0;
static float amt  = 0.6f;

void init(void) {
    instrument(I_BASS, INSTR_SAW, 4, 220, 6, 260);
    instrument_filter(I_BASS, FILTER_LOW, 1400, 7);   // a resonant peak to drive INTO
    instrument_drive(I_BASS, amt);
    instrument_drive_mode(I_BASS, mode);
    bpm(132);
}

void update(void) {
    if (btnp(0, BTN_LEFT))  { mode = (mode + 3) & 3; instrument_drive_mode(I_BASS, mode); }
    if (btnp(0, BTN_RIGHT)) { mode = (mode + 1) & 3; instrument_drive_mode(I_BASS, mode); }
    if (btn(0, BTN_UP))   { amt += 0.015f; if (amt > 1.0f) amt = 1.0f; instrument_drive(I_BASS, amt); }
    if (btn(0, BTN_DOWN)) { amt -= 0.015f; if (amt < 0.0f) amt = 0.0f; instrument_drive(I_BASS, amt); }
    if (btnp(0, BTN_A)) note(36, I_BASS, 6);

    // tap the four panels on a phone
    int pw = SCREEN_W / 4;
    for (int i = 0; i < 4; i++)
        if (tapp(i * pw, 60, pw, 70)) { mode = i; instrument_drive_mode(I_BASS, mode); }

    // auto-riff: a low octave-jumping bassline
    static const int riff[8] = { 36, 36, 43, 36, 41, 36, 48, 43 };
    if (every(1)) note(riff[beat() & 7], I_BASS, 6);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print_centered("DRIVE MODES", SCREEN_W / 2, 12, CLR_WHITE);
    print_centered("the four flavours of grit", SCREEN_W / 2, 26, CLR_LIGHT_GREY);

    int pw = SCREEN_W / 4;
    for (int i = 0; i < 4; i++) {
        int x = i * pw, on = (i == mode);
        rectfill(x + 4, 60, pw - 8, 70, on ? CLR_ORANGE : CLR_INDIGO);
        rect(x + 4, 60, pw - 8, 70, on ? CLR_WHITE : CLR_DARK_PURPLE);
        print_centered(MODE_NAME[i], x + pw / 2, 90, on ? CLR_BLACK : CLR_WHITE);
        // a representative waveshape glyph per mode (a sine pushed through that shaper)
        int cy = 116, cx0 = x + 12, ww = pw - 24;
        int col = on ? CLR_BLACK : CLR_LIGHT_GREY;
        for (int px = 0; px < ww; px++) {
            float t = (px / (float)ww) * 6.2832f, s = sinf(t), v;
            if      (i == 0) v = tanhf(s * 2.2f);                            // soft
            else if (i == 1) v = s > 0.5f ? 1.0f : (s < -0.5f ? -1.0f : s * 2.0f);  // hard
            else if (i == 2) v = sinf(s * 4.0f);                             // fold
            else             v = s >= 0 ? tanhf(s * 2.2f) : tanhf(s * 1.1f) * 0.8f; // asym
            pset(cx0 + px, cy - (int)(v * 11.0f), col);
        }
    }

    print(MODE_DESC[mode], 18, 150, CLR_YELLOW);

    // drive amount bar
    print("drive", 18, 170, CLR_WHITE);
    rect(70, 170, 180, 8, CLR_DARK_PURPLE);
    rectfill(71, 171, (int)(178 * amt), 6, CLR_ORANGE);
    char buf[16]; snprintf(buf, sizeof buf, "%d%%", (int)(amt * 100 + 0.5f));
    print(buf, 256, 170, CLR_WHITE);

    print_centered("\x1b \x1a mode    up/down drive    Z pluck", SCREEN_W / 2, 188, CLR_LIGHT_GREY);
}
