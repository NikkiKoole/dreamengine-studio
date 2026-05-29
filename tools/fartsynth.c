#include "studio.h"

// LE PÉTOMANE — a fart synthesizer, and a lesson in sound design.
//
// A single note is boring. Every interesting sound is really MANY simple notes
// strung together, changing over time. This toy builds a "fart" out of exactly
// that: while it plays, it fires a fast stream of short notes whose PITCH slides
// (BEND) and wobbles (WOBBLE), through a chosen WAVEFORM, fading out at the end
// (the envelope). That is how chip music fakes a glissando — no fancy API, just
// lots of hit() calls in a row.
//
// Tweak the six knobs, hit it, and listen to what each one does.
//
//   W / S   pick a knob          Z   let it rip
//   A / D   turn the knob        X   next preset

enum { P_PITCH, P_LEN, P_WAVE, P_BEND, P_WOBBLE, P_VOL, N_PARAMS };

typedef struct { const char *name; int lo, hi, step; } Param;
static Param P[N_PARAMS] = {
    { "PITCH",   20, 72,   2 },
    { "LENGTH", 150, 1200, 50 },
    { "WAVE",     0, 4,    1 },
    { "BEND",   -24, 24,   2 },
    { "WOBBLE",   0, 12,   1 },
    { "VOLUME",   1, 7,    1 },
};
static int pv[N_PARAMS];
static int sel = 0;

static const char *WAVE_NAME[5] = { "SQUARE", "SAW", "TRIANGLE", "NOISE", "SINE" };

// presets: pitch, len, wave, bend, wobble, vol
static const char *PRESET_NAME[6] = {
    "THE RUMBLER", "THE SQUEAKER", "THE TRUMPET", "THE WET ONE", "SILENT-BUT-DEADLY", "THE MACHINE GUN"
};
static const int PRESET[6][N_PARAMS] = {
    { 28,  800, INSTR_SAW,    -10,  3, 6 },
    { 60,  250, INSTR_SQUARE,   6,  8, 5 },
    { 44,  500, INSTR_SAW,       0,  6, 6 },
    { 32,  600, INSTR_NOISE,    -8, 10, 6 },
    { 30, 1000, INSTR_SINE,     -4,  1, 2 },
    { 40,  450, INSTR_SQUARE,   -2, 12, 7 },
};
static int preset = 0;

static bool  playing = false;
static int   play_frame = 0;
static float play_frac = 0;   // 0..1 progress, for the visuals

static void apply_preset(int n) {
    for (int i = 0; i < N_PARAMS; i++) pv[i] = PRESET[n][i];
}

void init(void) { apply_preset(0); }

void update(void) {
    if (btnp(0, BTN_UP))   sel = (sel + N_PARAMS - 1) % N_PARAMS;
    if (btnp(0, BTN_DOWN)) sel = (sel + 1) % N_PARAMS;
    if (btnp(0, BTN_LEFT))  pv[sel] = max(P[sel].lo, pv[sel] - P[sel].step);
    if (btnp(0, BTN_RIGHT)) pv[sel] = min(P[sel].hi, pv[sel] + P[sel].step);

    if (btnp(0, BTN_B)) { preset = (preset + 1) % 6; apply_preset(preset); }

    if (btnp(0, BTN_A)) { playing = true; play_frame = 0; }

    if (playing) {
        int len = pv[P_LEN];
        int elapsed = play_frame * 1000 / 60;
        play_frac = (float)elapsed / len;
        if (elapsed >= len) {
            playing = false;
        } else if (play_frame % 2 == 0) {
            // build the swept, wobbling pitch for this instant
            float t   = play_frac;
            float vib = pv[P_WOBBLE] * sin_deg(elapsed * 0.9f);
            int   midi = mid(16, pv[P_PITCH] + (int)(pv[P_BEND] * t) + (int)vib, 108);
            // envelope: fade out over the last quarter
            int vol = pv[P_VOL];
            if (t > 0.75f) vol = (int)(vol * (1.0f - t) / 0.25f);
            if (vol < 1) vol = 1;
            hit(midi, pv[P_WAVE], vol, 45);
        }
        play_frame++;
    }
}

// draw the selected waveform as an oscilloscope trace (2 cycles)
static void draw_wave(int bx, int by, int bw, int bh) {
    rectfill(bx, by, bw, bh, CLR_BLACK);
    rect(bx, by, bw, bh, CLR_DARK_GREY);
    int midY = by + bh / 2, amp = bh / 2 - 3;
    int px = 0, py = 0;
    for (int i = 0; i <= bw - 4; i += 2) {
        float ph = (float)i / (bw - 4) * 2.0f;   // 0..2 cycles
        float f  = ph - (int)ph;                  // fractional part of the cycle
        float s;
        switch (pv[P_WAVE]) {
            case INSTR_SQUARE: s = (f < 0.5f) ? 1 : -1; break;
            case INSTR_SAW:    s = 1 - 2 * f; break;
            case INSTR_TRI:    s = (f < 0.5f) ? (f * 4 - 1) : (3 - 4 * f); break;
            case INSTR_NOISE:  s = noise(i * 0.4f + frame() * 0.2f) * 2 - 1; break;
            default:           s = sin_deg(ph * 360); break;  // SINE
        }
        int x = bx + 2 + i, y = midY - (int)(s * amp);
        if (i > 0) line(px, py, x, y, CLR_GREEN);
        px = x; py = y;
    }
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    print("LE PETOMANE", 6, 4, CLR_LIGHT_PEACH);
    print_right("fart synth", 314, 4, CLR_MAUVE);

    // ── knobs ──
    for (int i = 0; i < N_PARAMS; i++) {
        int y = 24 + i * 20;
        bool on = (i == sel);
        if (on) print(">", 2, y, CLR_YELLOW);
        print(P[i].name, 10, y, on ? CLR_WHITE : CLR_LIGHT_GREY);

        // slider bar filled to the current value
        int bx = 74, bw = 70;
        rect(bx, y, bw, 7, CLR_DARK_GREY);
        int fill = (int)((float)(pv[i] - P[i].lo) / (P[i].hi - P[i].lo) * (bw - 2));
        rectfill(bx + 1, y + 1, fill, 5, on ? CLR_YELLOW : CLR_MAUVE);

        // value readout
        const char *v = (i == P_WAVE)   ? WAVE_NAME[pv[i]]
                      : (i == P_LEN)    ? str("%dms", pv[i])
                      : (i == P_BEND)   ? str("%+d", pv[i])
                                        : str("%d", pv[i]);
        print(v, 150, y, on ? CLR_WHITE : CLR_LIGHT_GREY);
    }

    // ── waveform scope ──
    print("WAVEFORM", 232, 22, CLR_LIGHT_GREY);
    draw_wave(232, 32, 82, 40);

    // ── whoopee cushion (deflates as it plays) ──
    int cx = 268, cy = 130;
    float infl = playing ? clamp(1.0f - play_frac * 0.55f, 0.45f, 1.0f) : 1.0f;
    int r = (int)(26 * infl);
    rectfill(cx - r, cy - r * 6 / 10, 2 * r, r * 12 / 10, CLR_RED);
    circfill(cx - r, cy, r * 6 / 10, CLR_RED);
    circfill(cx + r, cy, r * 6 / 10, CLR_RED);
    circfill(cx - r / 2, cy - r / 3, 4, CLR_LIGHT_PEACH);   // highlight
    rectfill(cx + r, cy - 3, 11, 6, CLR_DARK_RED);          // nozzle
    if (playing) {
        // little gust lines + a caption when it's ripping
        for (int i = 0; i < 3; i++)
            line(cx + r + 12, cy - 4 + i * 4, cx + r + 12 + 8 + (frame() % 6), cy - 4 + i * 4, CLR_LIGHT_YELLOW);
        print("PFFFT!", cx + r + 6, cy - 18, CLR_LIGHT_YELLOW);
    }

    print(PRESET_NAME[preset], 232, 156, CLR_PINK);
    print("W/S pick  A/D turn  Z rip  X preset", 6, SCREEN_H - 9, CLR_LIGHT_GREY);
}
