#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── GRAIN CLOUDS ──────────────────────────────────────────────────────────────
// The showcase for grains() — the granular delay ported from navkit. It captures
// the live signal into a buffer, then sprays scattered, overlapping windowed
// GRAINS of the recent past back out as an evolving cloud. The headline move is
// FREEZE: stop capturing, loop what's there forever — play a chord, freeze it
// into an infinite shimmering pad, then play OVER your own frozen self.
//
//   A slow PAD chord feeds the master grains() cloud the whole time.
//   1 2 3 4   play four chords into the cloud (Cmaj7 Am7 Fmaj7 G)
//   F / SPACE FREEZE the cloud — captured audio loops; the pad keeps playing on top
//   the SLIDERS drag grain SIZE / DENSITY / POSITION / SCATTER / FEEDBACK / MIX live
//   H         toggle this help
//
// Watch the cloud: each dot is a grain reading the buffer. Frozen = the swarm
// keeps swirling but no fresh audio is captured (the bar at the bottom stops).

#define SL_PAD   8

static float k_size  = 0.18f;   // 0..1 → grain length (mapped to ~5..400ms)
static float k_dens  = 0.30f;   // 0..1 → density 1..60 grains/sec
static float k_pos   = 0.80f;   // 0..1 → position (0 deep past, 1 live edge)
static float k_scat  = 0.35f;   // 0..1 → scatter
static float k_fb    = 0.25f;   // 0..1 → feedback 0..0.9
static float k_mix   = 0.55f;   // 0..1 → dry/wet

static bool  frozen     = false;
static bool  show_help  = false;
static int   pad_h[4]   = { -1, -1, -1, -1 };   // held pad note handles
static int   chord_i    = 0;
static float retrig     = 0.0f;

// last-applied grain params (SET-AND-HOLD: only re-push grains() when a value moves)
static float a_size = -1, a_dens = -1, a_pos = -1, a_scat = -1, a_fb = -1, a_mix = -1;

// the four chords (MIDI), a soft jazzy pad loop
static const int CHORD[4][4] = {
    { 60, 64, 67, 71 },   // Cmaj7
    { 57, 60, 64, 67 },   // Am7
    { 53, 57, 60, 64 },   // Fmaj7
    { 55, 59, 62, 65 },   // G
};
static const char *CHORD_NAME[4] = { "Cmaj7", "Am7", "Fmaj7", "G" };

// grain particles — purely visual, mirrors the density/scatter knobs
#define NGRAIN 64
typedef struct { float x, y, vx, vy, age, life; bool on; } Grain;
static Grain gr[NGRAIN];
static float spawn_acc = 0.0f;

static float grain_ms(void)  { return 5.0f  + k_size * 395.0f; }     // 5..400ms
static float density(void)   { return 1.0f  + k_dens * 59.0f;  }     // 1..60/s
static float feedback(void)  { return k_fb  * 0.9f; }               // 0..0.9

static void play_chord(int ci) {
    for (int i = 0; i < 4; i++) {
        if (pad_h[i] >= 0) note_off(pad_h[i]);
        pad_h[i] = note_on(CHORD[ci][i], SL_PAD, 5);
    }
    chord_i = ci;
    retrig  = 0.0f;
}

static void spawn_grain(void) {
    for (int i = 0; i < NGRAIN; i++) if (!gr[i].on) {
        // position knob → which part of the cloud the grain reads from (x), scatter spreads it
        float base = 40.0f + k_pos * (SCREEN_W - 80.0f);
        float sx   = (float)((i * 2654435761u) >> 8 & 1023) / 1023.0f - 0.5f;
        gr[i].x    = base + sx * k_scat * (SCREEN_W * 0.5f);
        gr[i].y    = 60.0f + (float)((i * 40503u) >> 4 & 127);
        gr[i].vx   = sx * 8.0f;
        gr[i].vy   = -6.0f - k_size * 18.0f;
        gr[i].age  = 0.0f;
        gr[i].life = 0.3f + grain_ms() / 400.0f;   // longer grains live longer
        gr[i].on   = true;
        return;
    }
}

void update(void) {
    float dt = 1.0f / 60.0f;

    // ── keys ──
    if (keyp('1')) play_chord(0);
    if (keyp('2')) play_chord(1);
    if (keyp('3')) play_chord(2);
    if (keyp('4')) play_chord(3);
    if (keyp('H')) show_help = !show_help;
    if (keyp('F') || keyp(KEY_SPACE)) {
        frozen = !frozen;
        grains_freeze(frozen ? 1 : 0);
    }

    // ── auto re-trigger the pad so the cloud always has fresh material (unless frozen) ──
    retrig += dt;
    if (retrig > 4.0f && !frozen) play_chord((chord_i + 1) & 3);

    // ── SET-AND-HOLD: reconfigure grains() only when a knob actually moved ──
    float gm = grain_ms(), de = density(), po = k_pos, sc = k_scat, fb = feedback(), mx = k_mix;
    if (gm != a_size || de != a_dens || po != a_pos || sc != a_scat || fb != a_fb || mx != a_mix) {
        grains(gm, de, po, sc, fb, mx);
        a_size = gm; a_dens = de; a_pos = po; a_scat = sc; a_fb = fb; a_mix = mx;
    }

    // ── visual grain swarm ──
    spawn_acc += density() * dt * 0.25f;
    while (spawn_acc >= 1.0f) { spawn_acc -= 1.0f; spawn_grain(); }
    for (int i = 0; i < NGRAIN; i++) if (gr[i].on) {
        gr[i].age += dt;
        gr[i].x += gr[i].vx * dt;
        gr[i].y += gr[i].vy * dt;
        if (gr[i].age >= gr[i].life) {
            if (frozen) gr[i].age = 0.0f, gr[i].y = 60.0f + (float)((i * 40503u) >> 4 & 127);  // frozen → loop
            else gr[i].on = false;
        }
    }
}

void draw(void) {
    cls(CLR_BLACK);
    print("GRAIN CLOUDS", 8, 6, CLR_MAUVE);
    print(frozen ? "FROZEN" : "LIVE", SCREEN_W - 56, 6, frozen ? CLR_YELLOW : CLR_DARK_GREY);

    // the grain swarm
    for (int i = 0; i < NGRAIN; i++) if (gr[i].on) {
        float t = 1.0f - gr[i].age / gr[i].life;          // brightness fades over the grain's life (the Hanning window)
        int c = t > 0.66f ? CLR_WHITE : t > 0.33f ? CLR_MAUVE : CLR_INDIGO;
        circfill((int)gr[i].x, (int)gr[i].y, t > 0.5f ? 2 : 1, c);
    }

    // current chord
    char buf[48];
    snprintf(buf, sizeof buf, "feeding: %s", CHORD_NAME[chord_i]);
    print(buf, 8, SCREEN_H - 64, CLR_MEDIUM_GREY);

    // capture bar — sweeps while LIVE, stops when FROZEN (you SEE the write head halt)
    int bw = SCREEN_W - 16;
    rect(8, SCREEN_H - 52, bw, 6, CLR_DARKER_GREY);
    if (!frozen) {
        int wx = (int)((now() * 0.4f - floorf(now() * 0.4f)) * (bw - 2));
        rectfill(9 + wx, SCREEN_H - 51, 2, 4, CLR_GREEN);
    }

    // ── sliders ──
    ui_begin();
    int sy = SCREEN_H - 40, sw = 78;
    ui_slider(&k_size, 8,           sy, sw, "SIZE");
    ui_slider(&k_dens, 8 + sw + 8,  sy, sw, "DENSITY");
    ui_slider(&k_pos,  8 + (sw+8)*2, sy, sw, "POSITION");
    ui_slider(&k_scat, 8,           sy + 18, sw, "SCATTER");
    ui_slider(&k_fb,   8 + sw + 8,  sy + 18, sw, "FEEDBACK");
    ui_slider(&k_mix,  8 + (sw+8)*2, sy + 18, sw, "MIX");
    ui_end();

    print("1-4 chords   F/SPACE freeze   H help", 8, SCREEN_H - 8, CLR_DARK_GREY);

    if (show_help) {
        rectfill(20, 24, SCREEN_W - 40, 102, CLR_DARK_BLUE);
        rect(20, 24, SCREEN_W - 40, 102, CLR_MAUVE);
        print("grains(): granular delay", 28, 32, CLR_WHITE);
        print("(ported from navkit)", 28, 42, CLR_DARK_GREY);
        print("captures audio, replays it as", 28, 56, CLR_LIGHT_GREY);
        print("scattered grains -> a cloud.", 28, 66, CLR_LIGHT_GREY);
        print("FREEZE = loop it forever:", 28, 80, CLR_YELLOW);
        print("chord, freeze, solo over it.", 28, 90, CLR_LIGHT_GREY);
        print("SCATTER = evolving. FEEDBACK", 28, 104, CLR_LIGHT_GREY);
        print("= self-feeding cloud.", 28, 114, CLR_LIGHT_GREY);
    }
}

void init(void) {
    // a soft pad: slow attack, long release — lots of sustain for the cloud to chew on
    instrument(SL_PAD, INSTR_TRI, 220, 400, 6, 900);
    play_chord(0);
}
