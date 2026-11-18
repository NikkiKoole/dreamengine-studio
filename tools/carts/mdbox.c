/* de:meta
{
  "slug": "mdbox",
  "title": "md groovebox",
  "status": "active",
  "created": "2026-11-18",
  "kind": [
    "instrument",
    "toy"
  ],
  "teaches": [
    "step-sequencer",
    "fm-synth"
  ],
  "lineage": "A little groovebox on the Mega Drive voicing worked out in dreamchip and mdstudies: FM4 on its alt mapping for e-piano and pad, the patch-matched 'grit' bass (or the FM bass), and DAC-style 8 kHz drum samples. The sounds were chosen by blind listening tests against a real YM2612 (see mdstudies / mdref).",
  "description": {
    "summary": "A 16-step Mega Drive groovebox: DAC drums, grit or FM bass, FM e-piano stabs and pads.",
    "detail": "Seven rows of 16 steps: kick, clap, closed hat, open hat (8-bit samples held at ~8 kHz, like the Mega Drive's DAC), bass (each step is the root, octave, fifth or seventh of the bar's chord), e-piano stabs (the chord) and a pad (holds the chord). The pattern is one bar; the chords move through a 4-bar cycle from a preset: club F minor, pop C major, 70s soul E minor, or the retro tour's A minor. Swing, tempo, and the bass voice (the grit bass fitted to a real YM2612, or the FM bass) are one tap away.",
    "controls": "Tap a cell to toggle it (bass cells cycle root / octave / fifth / seventh / off). SPACE play/stop, LEFT/RIGHT tempo, S swing, B bass voice, P chord preset, C clear, D demo pattern. The buttons on top do the same."
  }
}
de:meta */
// mdbox — a Mega Drive groovebox. Sounds: see mdstudies.c (same helpers, same patches).
#include "studio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- sounds
#define ALG(n) (((n) + 0.5f) / 8.0f)
static float R(float ratio) {
    static const float T[13] = {0.5f, 1, 1.414f, 1.5f, 1.618f, 2, 2.414f, 3, 3.5f, 4, 5, 7, 14};
    int best = 0;
    for (int i = 1; i < 13; i++) if (fabsf(T[i] - ratio) < fabsf(T[best] - ratio)) best = i;
    return (best + 0.5f) / 13.0f;
}
static void fm(int slot, int a, int d, int s, int r, int alg, float r0, float r1, float r2, float r3,
               float bright, float feedback) {
    instrument(slot, INSTR_FM4, a, d, s, r);
    instrument_mode(slot, MODE_FM4_MAP, 1.0f);
    instrument_harmonics(slot, ALG(alg));
    instrument_mode(slot, MODE_FM4_R0, R(r0)); instrument_mode(slot, MODE_FM4_R1, R(r1));
    instrument_mode(slot, MODE_FM4_R2, R(r2)); instrument_mode(slot, MODE_FM4_R3, R(r3));
    instrument_timbre(slot, bright);
    instrument_morph(slot, feedback);
    instrument_crush(slot, 10.0f, 1.0f, 1.0f);
}

static unsigned rngs = 0x5EED;
static float nz(void) { rngs = rngs * 1664525u + 1013904223u; return (float)(rngs >> 9) / 4194304.0f - 1.0f; }
typedef float (*DrumFn)(float t);
static float ph_k;
static float k909(float t) { if (t == 0) ph_k = 0; ph_k += 6.2832f * (52 + 130 * expf(-t / 0.025f)) / 44100.0f;
    return tanhf(1.6f * sinf(ph_k) * expf(-t / 0.14f)) + nz() * expf(-t / 0.002f) * 0.3f; }
static float clap(float t) { float e = 0; for (int k = 0; k < 3; k++) { float tk = t - k * 0.011f; if (tk >= 0) e += expf(-tk / 0.006f); }
    return nz() * (e * 0.7f + expf(-t / 0.08f) * 0.6f); }
static float chat(float t) { return nz() * expf(-t / 0.012f); }
static float ohat(float t) { return nz() * expf(-t / 0.13f) * 0.8f; }

static void make_drum(int sample_slot, DrumFn fn, float secs) {     // 8-bit, held at ~8.2 kHz: the DAC
    const int sr = 44100;
    int n = (int)(secs * sr);
    float *buf = (float *)calloc((size_t)n, sizeof(float));
    float held = 0, next = 0, pk = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / sr, v = fn(t);
        if (t >= next) { held = roundf(fmaxf(-1, fminf(1, v)) * 127) / 127; next += 1.0f / 8191.0f; }
        buf[i] = held;
        if (fabsf(held) > pk) pk = fabsf(held);
    }
    if (pk > 0) for (int i = 0; i < n; i++) buf[i] *= 0.95f / pk;
    sample_load(sample_slot, buf, n);
    free(buf);
}

enum { R_KICK, R_CLAP, R_CHAT, R_OHAT, R_BASS, R_STAB, R_PAD, NROWS };
static const char *ROW_NAME[NROWS] = {"KICK", "CLAP", "C.HAT", "O.HAT", "BASS", "STAB", "PAD"};
#define S_DRUM0 40                                // drum slots 40..43, sample slots 0..3
#define S_BASS  10
#define S_STAB  11
#define S_PAD   12

static int grit = 1;                              // bass voice: 1 = matched grit bass, 0 = FM bass
static void set_bass(void) {
    if (grit) {                                   // patch-matcher fit to a real YM2612 bass (mdstudies)
        instrument(S_BASS, INSTR_PIPE, 0, 1201, 0, 43);
        instrument_harmonics(S_BASS, 0.210f); instrument_timbre(S_BASS, 0.975f); instrument_morph(S_BASS, 0.599f);
        instrument_filter(S_BASS, FILTER_BAND, 42, 6);
        instrument_env(S_BASS, 0, ENV_CUTOFF_OCT, 0, 175, 1.83f);
        instrument_lfo(S_BASS, 1, LFO_VOLUME, 1.90f, 0.027f);
        instrument_drive(S_BASS, 0.312f); instrument_drive_mode(S_BASS, DRIVE_ASYM);
        instrument_tape(S_BASS, 0.677f, 0.475f, 0.349f);
        instrument_tremolo(S_BASS, 10.68f, 0.097f, LFO_SHAPE_SINE);
        instrument_eq(S_BASS, -7.6f, -6.9f, 8.8f);
        instrument_level(S_BASS, 1.0f);
    } else {
        fm(S_BASS, 1, 280, 4, 90, 0, 1, 1, 2, 1, 0.40f, 0.50f);
        instrument_env(S_BASS, 0, ENV_TIMBRE, 1, 110, 0.30f);
        instrument_drive(S_BASS, 0.10f); instrument_level(S_BASS, 0.85f);
    }
}

static void setup_sounds(void) {
    make_drum(0, k909, 0.35f); make_drum(1, clap, 0.25f); make_drum(2, chat, 0.06f); make_drum(3, ohat, 0.35f);
    const float lvl[4] = {1.0f, 0.75f, 0.30f, 0.35f}, pan[4] = {0, 0.1f, 0.35f, -0.3f};
    for (int k = 0; k < 4; k++) {
        instrument(S_DRUM0 + k, INSTR_SAMPLE, 0, 0, 7, 40);
        instrument_sample(S_DRUM0 + k, k, 60);
        instrument_level(S_DRUM0 + k, lvl[k]);
        instrument_pan(S_DRUM0 + k, pan[k]);
    }
    instrument_choke(S_DRUM0 + 2, S_DRUM0 + 3);   // a closed hat cuts the open one
    set_bass();
    fm(S_STAB, 2, 420, 3, 500, 4, 1, 1, 14, 1, 0.42f, 0.18f);           // the FM e-piano
    instrument_env(S_STAB, 0, ENV_TIMBRE, 1, 60, 0.20f);
    instrument_level(S_STAB, 0.45f);
    fm(S_PAD, 520, 0, 7, 700, 4, 1, 1, 2, 1, 0.28f, 0.20f);             // the FM pad
    instrument_analog(S_PAD, 0.5f);
    instrument_level(S_PAD, 0.30f);
}

// ---------------------------------------------------------------- chords + pattern
typedef struct { const char *name; int tempo; const char *chord[4]; int root[4]; int notes[4][4]; } Preset;
static const Preset PRESET[4] = {
    {"CLUB Fm", 124, {"Fm9", "Dbmaj7", "Ebsus", "Cm7"}, {41, 37, 39, 36},
     {{56, 60, 63, 67}, {53, 56, 60, 63}, {56, 58, 63, 65}, {51, 55, 58, 62}}},
    {"POP C", 128, {"C", "Am", "F", "G"}, {36, 45, 41, 43},
     {{55, 60, 64, 67}, {57, 60, 64, 69}, {57, 60, 65, 69}, {55, 59, 62, 67}}},
    {"SOUL Em", 96, {"Em9", "A13", "Dmaj9", "Cmaj7"}, {40, 45, 38, 36},
     {{55, 59, 62, 66}, {55, 61, 66, 71}, {54, 57, 61, 64}, {52, 55, 59, 62}}},
    {"TOUR Am", 128, {"Am", "F", "C", "G"}, {45, 41, 48, 43},
     {{57, 60, 64, 69}, {57, 60, 65, 69}, {55, 60, 64, 67}, {55, 59, 62, 67}}},
};
static const int BASS_IV[5] = {0, 0, 12, 7, 10};  // cell value -> interval: root, octave, fifth, seventh
static const char *BASS_TXT[5] = {"", "1", "8", "5", "7"};

static unsigned char grid[NROWS][16];
static int preset = 0, tempo = 124, swing = 0, playing = 1;
static double t0;
static long step;
static int play_step = -1;
static float cell_flash[NROWS][16], flash;

static void load_demo(void) {
    static const char *demo[NROWS] = {
        "x...x...x...x...", "....x.......x...", ".x.x.x.x.x.x.x.x", "..x...x...x...x.",
        "1..2..1.1.2.3.1.", "..x...x....x..x.", "x...............",
    };
    for (int r = 0; r < NROWS; r++)
        for (int s = 0; s < 16; s++) {
            char c = demo[r][s];
            grid[r][s] = c == '.' ? 0 : r == R_BASS ? (unsigned char)(c - '0') : 1;
        }
}

static void start(void) { t0 = audio_time() + 0.15; step = 0; }

static void book_step(long s, double t, double st) {
    int i = (int)(s % 16), bar = (int)((s / 16) % 4);
    const Preset *P = &PRESET[preset];
    double ts = t + ((swing && (i & 1)) ? 0.22 * st : 0.0);
    int ms16 = (int)(st * 1000);
    for (int k = 0; k < 4; k++)
        if (grid[k][i]) schedule_at(ts, 60, S_DRUM0 + k, k == R_CHAT ? 4 : 6, 400);
    if (grid[R_BASS][i]) schedule_at(ts, P->root[bar] + BASS_IV[grid[R_BASS][i]], S_BASS, 6, ms16 * 2 - 20);
    if (grid[R_STAB][i])
        for (int k = 0; k < 4; k++) schedule_at(ts, P->notes[bar][k] + 12, S_STAB, 5, ms16 * 2);
    if (grid[R_PAD][i]) {                          // hold until the next pad step (or the bar's end)
        int len = 1;
        while (i + len < 16 && !grid[R_PAD][i + len]) len++;
        for (int k = 0; k < 4; k++) schedule_at(t, P->notes[bar][k], S_PAD, 4, ms16 * len - 40);
    }
}

// ---------------------------------------------------------------- input + layout
typedef struct { int x, w; const char *id; } Button;
static char lbl_play[8], lbl_bpm[12], lbl_swing[12], lbl_bass[12], lbl_chords[16];
#define BAR_Y 18
#define BAR_H 11
#define GRID_X 56
#define GRID_Y 36
#define CELL_W 16
#define ROW_H 14

static Button BTN[] = {
    {4, 28, "play"}, {36, 10, "slower"}, {48, 34, "tempo"}, {84, 10, "faster"}, {98, 40, "swing"},
    {142, 50, "bass"}, {196, 64, "chords"}, {264, 26, "clear"}, {294, 22, "demo"},
};
#define NBTN (int)(sizeof BTN / sizeof BTN[0])

static void press(const char *id) {
    if (!strcmp(id, "play")) { playing = !playing; if (playing) start(); else note_off_all(); }
    else if (!strcmp(id, "slower")) { if (tempo > 60) tempo -= 2; }
    else if (!strcmp(id, "faster")) { if (tempo < 180) tempo += 2; }
    else if (!strcmp(id, "swing")) swing = !swing;
    else if (!strcmp(id, "bass")) { grit = !grit; set_bass(); }
    else if (!strcmp(id, "chords")) { preset = (preset + 1) % 4; tempo = PRESET[preset].tempo; }
    else if (!strcmp(id, "clear")) memset(grid, 0, sizeof grid);
    else if (!strcmp(id, "demo")) load_demo();
}

static void on_tap(int mx, int my) {
    if (my >= BAR_Y && my < BAR_Y + BAR_H)
        for (int b = 0; b < NBTN; b++) if (mx >= BTN[b].x && mx < BTN[b].x + BTN[b].w) { press(BTN[b].id); return; }
    int r = (my - GRID_Y) / ROW_H, s = (mx - GRID_X) / CELL_W;
    if (my >= GRID_Y && r >= 0 && r < NROWS && mx >= GRID_X && s >= 0 && s < 16) {
        if (r == R_BASS) grid[r][s] = (unsigned char)((grid[r][s] + 1) % 5);
        else grid[r][s] = !grid[r][s];
    }
}

void update(void) {
    static bool boot = false;
    if (!boot) { setup_sounds(); load_demo(); start(); boot = true; }
    if (keyp(KEY_SPACE)) press("play");
    if (keyp(KEY_LEFT)) press("slower");
    if (keyp(KEY_RIGHT)) press("faster");
    if (keyp('s') || keyp('S')) press("swing");
    if (keyp('b') || keyp('B')) press("bass");
    if (keyp('p') || keyp('P')) press("chords");
    if (keyp('c') || keyp('C')) press("clear");
    if (keyp('d') || keyp('D')) press("demo");
    if (mouse_pressed(MOUSE_LEFT)) on_tap(mouse_x(), mouse_y());

    double st = 60.0 / tempo / 4.0;
    if (playing) {
        double now_t = audio_time();
        while (t0 + step * st < now_t + 0.12) { book_step(step, t0 + step * st, st); step++; }
        long cur = (long)floor((now_t - t0) / st);        // what is sounding now
        if (cur >= 0 && (int)(cur % 16) != play_step) {
            play_step = (int)(cur % 16);
            for (int r = 0; r < NROWS; r++) if (grid[r][play_step]) cell_flash[r][play_step] = 1;
            if (grid[R_KICK][play_step]) flash = 1;
        }
    }
    for (int r = 0; r < NROWS; r++) for (int s = 0; s < 16; s++) cell_flash[r][s] *= 0.85f;
    flash *= 0.85f;
}

// ---------------------------------------------------------------- draw
static const int ROW_COL[NROWS] = {CLR_YELLOW, CLR_ORANGE, CLR_LIGHT_YELLOW, CLR_PEACH, CLR_GREEN, CLR_PINK, CLR_BLUE};

void draw(void) {
    camera(0, 0);
    cls(flash > 0.5f ? CLR_DARKER_PURPLE : CLR_DARKER_BLUE);
    font(FONT_NORMAL);
    print("MD GROOVEBOX", 4, 5, CLR_YELLOW);
    font(FONT_SMALL);
    print("dac drums - grit/fm bass - fm stabs + pads", 112, 7, CLR_INDIGO);

    snprintf(lbl_play, sizeof lbl_play, "%s", playing ? "STOP" : "PLAY");
    snprintf(lbl_bpm, sizeof lbl_bpm, "%d BPM", tempo);
    snprintf(lbl_swing, sizeof lbl_swing, "SWING %s", swing ? "ON" : "OFF");
    snprintf(lbl_bass, sizeof lbl_bass, "BASS %s", grit ? "GRIT" : "FM");
    snprintf(lbl_chords, sizeof lbl_chords, "%s", PRESET[preset].name);
    const char *txt[NBTN] = {lbl_play, "-", lbl_bpm, "+", lbl_swing, lbl_bass, lbl_chords, "CLEAR", "DEMO"};
    for (int b = 0; b < NBTN; b++) {
        int active = (b == 4 && swing) || (b == 5 && grit) || (b == 0 && playing);
        rectfill(BTN[b].x, BAR_Y, BTN[b].w, BAR_H, active ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
        print_centered(txt[b], BTN[b].x + BTN[b].w / 2, BAR_Y + 3, CLR_WHITE);
    }

    for (int r = 0; r < NROWS; r++) {                 // the grid
        int y = GRID_Y + r * ROW_H;
        print(ROW_NAME[r], 6, y + 4, ROW_COL[r]);
        for (int s = 0; s < 16; s++) {
            int x = GRID_X + s * CELL_W;
            int base = (s / 4) % 2 ? CLR_DARK_BLUE : CLR_DARKER_GREY;
            int on = grid[r][s];
            int col = on ? (cell_flash[r][s] > 0.3f ? CLR_WHITE : ROW_COL[r]) : base;
            rectfill(x, y, CELL_W - 2, ROW_H - 2, col);
            if (r == R_BASS && on) print_centered(BASS_TXT[on], x + 7, y + 4, CLR_BLACK);
        }
    }
    if (playing && play_step >= 0)                    // playhead
        rect(GRID_X + play_step * CELL_W - 1, GRID_Y - 1, CELL_W, NROWS * ROW_H, CLR_WHITE);

    const Preset *P = &PRESET[preset];                // the 4-bar chord cycle
    double st = 60.0 / tempo / 4.0;
    int bar = playing ? (int)(((long)floor((audio_time() - t0) / st) / 16) % 4) : -1;
    for (int k = 0; k < 4; k++) {
        int x = GRID_X + k * 64, y = GRID_Y + NROWS * ROW_H + 2;
        rectfill(x, y, 62, 10, k == bar ? CLR_TRUE_BLUE : CLR_DARK_BLUE);
        print_centered(P->chord[k], x + 31, y + 2, k == bar ? CLR_WHITE : CLR_INDIGO);
    }
    print("CHORDS", 6, GRID_Y + NROWS * ROW_H + 4, CLR_INDIGO);

    float sc[256];                                    // scope
    scope_read(sc, 256);
    int sy = 162;
    rectfill(GRID_X, sy, 256, 34, CLR_BROWNISH_BLACK);
    for (int x = 1; x < 256; x++) {
        int y0 = sy + 17 - (int)(sc[x - 1] * 40), y1 = sy + 17 - (int)(sc[x] * 40);
        y0 = y0 < sy + 1 ? sy + 1 : y0 > sy + 33 ? sy + 33 : y0;
        y1 = y1 < sy + 1 ? sy + 1 : y1 > sy + 33 ? sy + 33 : y1;
        line(GRID_X + x - 1, y0, GRID_X + x, y1, CLR_BLUE);
    }
    print("tap cells", 6, sy + 4, CLR_INDIGO);
    print("SPACE B P", 6, sy + 14, CLR_INDIGO);
    print("S C D", 6, sy + 24, CLR_INDIGO);
    font(FONT_NORMAL);
}
