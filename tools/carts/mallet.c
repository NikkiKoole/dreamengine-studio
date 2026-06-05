// mallet — INSTR_MALLET showcase: eight bars + the three engine macros.
//
// The second modeled ENGINE: every note strikes a little bar simulation (four decaying
// sine modes — the bar's vibration modes — plus the mallet's contact click), so what you
// hear is BEHAVIOR: the bar rings down and its partials fade at their own rates.
// Every engine answers the same three 0..1 macro knobs — the API never grows:
//   harmonics = bar material  (0 wood/marimba .. 1 metal/bell — the partials gliss between)
//   timbre    = mallet        (0 soft yarn thump .. 1 hard brass, bright + click)
//   morph     = ring length   (0 dry xylophone tick .. 1 vibraphone sustain + motor)
//
// The named instruments are nothing but KNOB POSITIONS (audio-notes §8.1: presets are
// baked macro positions) — that's what makes this cart the engine's tuning rig: if
// pressing 4 doesn't sound like a vibraphone, the macro MAPPING is wrong, not the preset.
//
// controls: A S D F G H J K  strike a bar (or click one; drag across = glissando)
//           1..5  presets: marimba / xylophone / celesta / vibraphone / glockenspiel
//           drag a slider with the mouse (auditions as you drag), or
//           LEFT/RIGHT pick a knob + UP/DOWN turn it
//           SPACE glissando run   ·   M autoplay on/off

#include "studio.h"
#include <math.h>

#define I_BAR 5
#define NBAR  8

static const char STRKEY[NBAR] = { 'A','S','D','F','G','H','J','K' };
static const char *KNOB_NAME[3] = { "harmonics (bar)", "timbre (mallet)", "morph (ring)" };
static const char *KNOB_LO[3]   = { "wood",  "yarn",  "tick" };
static const char *KNOB_HI[3]   = { "bell",  "brass", "vibe" };

// the presets: knob positions with a hardware name. STARTING GUESSES — this cart exists
// to tune them (and the mapping behind them) by ear.
typedef struct { const char *name; float h, t, m; } Preset;
static const Preset PRESET[5] = {
    { "marimba", 0.00f, 0.45f, 0.35f },
    { "xylo",    0.15f, 0.80f, 0.15f },
    { "celesta", 0.50f, 0.55f, 0.45f },
    { "vibes",   0.25f, 0.50f, 0.90f },   // morph's top = the motor
    { "glocken", 0.90f, 0.85f, 0.60f },
};

static int   midi_of[NBAR];
static float glow[NBAR];       // visual strike flash, decays each frame
static int   pend[NBAR];       // frames until a scheduled gliss note visually lands
static float knob[3] = { 0.25f, 0.5f, 0.5f };
static int   sel = 0;
static int   drag_k = -1;      // slider currently grabbed by the mouse, -1 = none
static bool  autoplay = true;
static int   cur_preset = -1;  // last pressed preset (display only; knob moves clear it)
static int   apos = 0;
static bool  sweeping = false; // mouse drag across the bars = glissando
static int   prevX = 0;
static float motor_ph = 0;     // the spinning fan, drawn when the motor is on

// bar geometry — a mallet keyboard: low long bars left, high short bars right
#define BAR_W    26
#define BAR_GAP  6
#define BAR_X(b) (14 + (b) * (BAR_W + BAR_GAP))
#define BAR_Y    34
#define BAR_H(b) (78 - (b) * 6)

// slider geometry — shared by draw() and the mouse hit-test
#define KNOB_W   88
#define KNOB_Y   (SCREEN_H - 38)
#define KNOB_X(k) (14 + (k) * 102)

static void apply_knobs(void) {
    instrument_harmonics(I_BAR, knob[0]);
    instrument_timbre(I_BAR, knob[1]);
    instrument_morph(I_BAR, knob[2]);
}

// gate scales with the ring knob — a tick frees its voice fast, a vibe ring isn't chopped
static int gate_ms(void) { return 500 + (int)(knob[2] * knob[2] * 9000.0f); }

static void strike(int b, int vol) {
    hit(midi_of[b], I_BAR, vol, gate_ms());
    glow[b] = 1.0f;
}

static void gliss(void) {
    for (int b = 0; b < NBAR; b++) {
        schedule_hit(b * 55, midi_of[b], I_BAR, 5, gate_ms());
        pend[b] = 1 + (b * 55 * 60) / 1000;   // matching visual flash, frames
    }
}

static void set_preset(int p) {
    knob[0] = PRESET[p].h; knob[1] = PRESET[p].t; knob[2] = PRESET[p].m;
    cur_preset = p;
    apply_knobs();
    strike(4, 6);                              // audition the new bar immediately
}

void init(void) {
    // the engine id is just a waveform — wrap it in a slot like any wave.
    // long release: the gate ending should never chop a ringing bar
    instrument(I_BAR, INSTR_MALLET, 1, 0, 7, 1200);
    apply_knobs();
    for (int b = 0; b < NBAR; b++) midi_of[b] = degree(SCALE_PENTA, 4, b + 1);
    bpm(92);
    glow[2] = 0.6f; glow[5] = 0.9f;            // a lively first frame
}

void update(void) {
    for (int b = 0; b < NBAR; b++) {
        if (keyp(STRKEY[b])) strike(b, 6);
        if (pend[b] > 0 && --pend[b] == 0) glow[b] = 1.0f;
    }
    for (int p = 0; p < 5; p++)
        if (keyp('1' + p)) set_preset(p);

    // knobs: pick with LEFT/RIGHT, turn with UP/DOWN (held); audition every 12 frames
    if (keyp(KEY_LEFT))  sel = (sel + 2) % 3;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 3;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_knobs();
        if (frame() % 12 == 0) strike(4, 5);
    }

    if (keyp(KEY_SPACE)) gliss();
    if (keyp('M')) autoplay = !autoplay;

    // mouse: grab a slider...
    if (mouse_pressed(MOUSE_LEFT)) {
        for (int k = 0; k < 3; k++)
            if (point_in_box(mouse_x(), mouse_y(), KNOB_X(k) - 2, KNOB_Y - 6, KNOB_W + 4, 18)) {
                drag_k = sel = k;
            }
        // ...or strike a bar; holding and sweeping across the keyboard = glissando
        if (drag_k < 0 && mouse_y() >= BAR_Y - 4 && mouse_y() < KNOB_Y - 14) {
            for (int b = 0; b < NBAR; b++)
                if (point_in_box(mouse_x(), mouse_y(), BAR_X(b), BAR_Y, BAR_W, BAR_H(b)))
                    strike(b, 6);
            sweeping = true;
            prevX = mouse_x();
        }
    }
    if (drag_k >= 0) {
        if (mouse_down(MOUSE_LEFT)) {
            knob[drag_k] = clamp((float)(mouse_x() - KNOB_X(drag_k)) / (float)KNOB_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_knobs();
            if (frame() % 12 == 0) strike(4, 5);   // audition while dragging
        } else drag_k = -1;
    }
    if (sweeping) {
        if (mouse_down(MOUSE_LEFT)) {
            int mx = mouse_x();
            for (int b = 0; b < NBAR; b++) {       // strike each bar the mallet crosses
                int cx = BAR_X(b) + BAR_W / 2;
                if ((prevX < cx && mx >= cx) || (prevX > cx && mx <= cx)) strike(b, 5);
            }
            prevX = mx;
        } else sweeping = false;
    }

    // autoplay: a music-box noodle; a glissando at the top of every 16 beats,
    // and now and then a ROLL (the mallet tremolo — rapid repeated strikes)
    if (autoplay && every(1)) {
        static const int seq[16] = { 0,4,2,5, 7,4,2,0, 3,5,4,1, 2,4,0,5 };
        if (beat() % 16 == 0) gliss();
        else if (chance(12)) {                     // the roll: 6 fast hits on one bar
            int b = seq[apos % 16];
            for (int r = 0; r < 6; r++) schedule_hit(r * 70, midi_of[b], I_BAR, 3 + (r & 1), gate_ms());
            pend[b] = 1;
        } else {
            strike(seq[apos % 16], 5);
            if (chance(30)) schedule_hit(330, midi_of[(seq[apos % 16] + 4) % NBAR], I_BAR, 3, gate_ms());
        }
        apos++;
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("preset", "%d", cur_preset);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("MALLET", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("modal struck-bar engine", 64, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    bool motor = knob[2] > 0.65f;
    if (motor) motor_ph += 0.04f + 0.10f * (knob[2] - 0.65f) / 0.35f;

    // the bars + their resonator tubes
    for (int b = 0; b < NBAR; b++) {
        int x = BAR_X(b), h = BAR_H(b);
        glow[b] *= 0.92f;
        int wood = knob[0] < 0.5f;
        int col  = glow[b] > 0.5f ? CLR_LIGHT_YELLOW
                 : glow[b] > 0.1f ? (wood ? CLR_PEACH : CLR_LIGHT_GREY)
                                  : (wood ? CLR_DARK_BROWN : CLR_DARKER_GREY);
        int bump = (int)(glow[b] * 3.0f);          // struck bars bounce up a touch
        rectfill(x, BAR_Y - bump, BAR_W, h, col);
        rect(x, BAR_Y - bump, BAR_W, h, glow[b] > 0.1f ? CLR_WHITE : CLR_DARK_GREY);
        // resonator tube
        int ty = BAR_Y + h + 4, th = 18 + (NBAR - b) * 2;
        rectfill(x + 8, ty, BAR_W - 16, th, CLR_DARKER_GREY);
        // the vibe motor: a little fan spins in each tube when morph's top third is in
        if (motor) {
            int cx = x + BAR_W / 2, cy = ty + th / 2;
            float a = motor_ph + b * 0.4f;
            line(cx - (int)(cosf(a) * 4), cy - (int)(sinf(a) * 4),
                 cx + (int)(cosf(a) * 4), cy + (int)(sinf(a) * 4), CLR_LIGHT_GREY);
        }
        print(str("%c", STRKEY[b]), x + BAR_W / 2 - 2, BAR_Y + h - 11,
              glow[b] > 0.1f ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
    }

    // preset row — knob positions with hardware names
    font(FONT_SMALL);
    for (int p = 0; p < 5; p++) {
        int x = 14 + p * 60;
        bool on = (p == cur_preset);
        print(str("%d %s", p + 1, PRESET[p].name), x, KNOB_Y - 26, on ? CLR_YELLOW : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);

    // the three macro knobs
    for (int k = 0; k < 3; k++) {
        int x = KNOB_X(k), y = KNOB_Y;
        bool on = (k == sel);
        font(FONT_SMALL);
        print(KNOB_NAME[k], x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 7, knob[k], on ? CLR_ORANGE : CLR_BROWN, CLR_DARKER_GREY);
        font(FONT_TINY);
        print(KNOB_LO[k], x, y + 9, CLR_DARK_GREY);
        print_right(KNOB_HI[k], x + KNOB_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    print("A..K strike   click/sweep the bars   1..5 presets   SPACE gliss", 10, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
