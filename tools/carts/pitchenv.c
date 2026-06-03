#include "studio.h"
#include <math.h>

// PITCH ENV — punch & zap. A one-shot envelope (instrument_env + ENV_PITCH) that
// starts the note AMOUNT semitones off its real pitch and lets it settle back over
// the decay. Big amount + fast decay on a low note = the "donk" of a kick drum;
// medium settings = lasers and zaps; a tiny amount = an attack "chiff" on any sound.
// Still no instrument engine — this is the same mod-envelope, pointed at pitch.
// Drag the sliders, W cycles the waveform, A-K play, SPACE toggles the auto-pulse.

#define SLOT 5

typedef struct { const char *name; float lo, hi, val; } Knob;
enum { K_ATK, K_DECAY, K_AMOUNT, K_NOTE, K_LEN, NK };
static Knob K[NK] = {
    { "ATK",     0.f,  120.f,   0.f },   // env attack (ms) — drums want ~0 (instant offset)
    { "DECAY",   5.f,  400.f,  55.f },   // env decay (ms) — how fast it settles to pitch
    { "AMOUNT",-48.f,   48.f,  36.f },   // pitch offset at the strike (semitones, bipolar)
    { "NOTE",   24.f,   72.f,  36.f },   // the pitch it settles TO (MIDI)
    { "LEN",    60.f,  600.f, 220.f },   // note length / amp body (ms)
};

static const int   WAVES[4]      = { INSTR_SINE, INSTR_TRI, INSTR_SAW, INSTR_SQUARE };
static const char *WAVE_NAME[4]  = { "SINE", "TRI", "SAW", "SQUARE" };
static int   wave_i = 1;          // TRI — good body for a kick

static int   active = -1;
static bool  auto_on = true;
static float arp_t = 0.f;
static float last_note_t = -9.f;

#define SX 12
#define SPITCH 60
#define SW 44
#define SY 110
#define SH 56

static void apply(void) {
    int len = (int)K[K_LEN].val;
    instrument(SLOT, WAVES[wave_i], 1, len, 0, 25);                              // punchy body that decays over the note
    instrument_env(SLOT, 0, ENV_PITCH, (int)K[K_ATK].val, (int)K[K_DECAY].val, K[K_AMOUNT].val);
}

static void play(int midi) {
    hit(midi, SLOT, 6, (int)K[K_LEN].val);
    last_note_t = now();
}

void init(void) {
    bpm(120);
    apply();
}

void update(void) {
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

    // A S D F G H J K play the NOTE root + pentatonic offsets (melodic zaps)
    const char keys[8] = { 'A','S','D','F','G','H','J','K' };
    const int  off[8]  = { 0, 3, 5, 7, 10, 12, 15, 19 };
    for (int i = 0; i < 8; i++)
        if (keyp(keys[i])) play((int)K[K_NOTE].val + off[i]);
    if (keyp('W')) { wave_i = (wave_i + 1) % 4; apply(); }
    if (keyp(KEY_SPACE)) auto_on = !auto_on;

    if (auto_on) {
        arp_t += dt();
        if (arp_t >= 0.45f) { arp_t = 0.f; play((int)K[K_NOTE].val); }
    }
}

// the pitch OFFSET (semitones) the env produces `s` seconds into a note
static float env_off_at(float s) {
    float a = K[K_ATK].val / 1000.f, d = K[K_DECAY].val / 1000.f, lvl;
    if (a > 0.f && s < a) lvl = s / a;
    else { float sd = s - a; lvl = (d <= 0.f || sd >= d) ? 0.f : expf(-4.0f * sd / d); }
    return lvl * K[K_AMOUNT].val;
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    print("PITCH ENV", 8, 5, CLR_LIGHT_PEACH);
    print_right("punch & zap", 312, 5, CLR_MAUVE);

    // graph — pitch offset over time
    int gx = 8, gy = 20, gw = 304, gh = 80;
    rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
    rect(gx, gy, gw, gh, CLR_DARKER_GREY);

    float span = (K[K_ATK].val + K[K_DECAY].val) / 1000.f;
    if (span < 0.04f) span = 0.04f;
    span *= 1.2f;
    float amt = K[K_AMOUNT].val;
    float lo_o = amt < 0.f ? amt : 0.f, hi_o = amt > 0.f ? amt : 0.f;
    if (hi_o - lo_o < 2.f) hi_o = lo_o + 2.f;
    #define OY(o) (gy + gh - 1 - (int)((clamp((o), lo_o, hi_o) - lo_o) / (hi_o - lo_o) * (gh - 2)))

    // the settled-pitch (offset 0) line
    int zy = OY(0.f);
    for (int x = gx + 2; x < gx + gw - 2; x += 6) pset(x, zy, CLR_DARK_GREY);

    int px = -1, py = -1;
    for (int x = 0; x < gw; x++) {
        int yy = OY(env_off_at((x / (float)gw) * span));
        if (px >= 0) line(px, py, gx + x, yy, amt < 0.f ? CLR_BLUE : CLR_DARK_ORANGE);
        px = gx + x; py = yy;
    }
    float since = now() - last_note_t;
    if (since >= 0.f && since < span) circfill(gx + (int)((since / span) * gw), OY(env_off_at(since)), 2, CLR_YELLOW);
    #undef OY
    print("pitch", gx + 3, gy + 2, CLR_DARK_GREY);
    print(str("%+d st", (int)amt), gx + 3, gy + 10, CLR_DARK_GREY);
    print_right(str("wave: %s", WAVE_NAME[wave_i]), gx + gw - 4, gy + 2, CLR_MEDIUM_GREY);

    // sliders
    for (int i = 0; i < NK; i++) {
        int cx = SX + i * SPITCH, col = active == i ? CLR_YELLOW : CLR_PINK;
        rect(cx, SY, SW, SH, CLR_DARKER_GREY);
        float t = (K[i].val - K[i].lo) / (K[i].hi - K[i].lo);
        int vy = SY + SH - 1 - (int)(t * (SH - 2));
        if (K[i].lo < 0.f) {                                 // bipolar knob fills from its zero line
            float t0 = (0.f - K[i].lo) / (K[i].hi - K[i].lo);
            int zzy = SY + SH - 1 - (int)(t0 * (SH - 2));
            int top = vy < zzy ? vy : zzy, h = (vy < zzy ? zzy - vy : vy - zzy) + 1;
            rectfill(cx + 1, top, SW - 2, h, col);
            line(cx + 1, zzy, cx + SW - 2, zzy, CLR_LIGHT_GREY);
        } else {
            int fh = (int)(t * (SH - 2));
            rectfill(cx + 1, SY + SH - 1 - fh, SW - 2, fh, col);
        }
        print(K[i].name, cx + 2, SY + SH + 3, CLR_LIGHT_GREY);
        print(str("%d", (int)K[i].val), cx + 2, SY + SH + 11, CLR_WHITE);
    }

    print(auto_on ? "pulse ON" : "pulse off", 8, SCREEN_H - 18, auto_on ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print("drag   A-K play   W wave   SPACE pulse", 8, SCREEN_H - 9, CLR_LIGHT_GREY);
}
