// pan — the stereo showcase (stereo.md step 2). Three ways to place a sound in the
// field: instrument_pan (static, per slot), note_pan (live, follows the ball — positional
// audio), and LFO_PAN (a hands-off auto-pan drone). Headphones recommended.
//
// A ball bounces left↔right; a held pad note pans to wherever it is (note_pan). Tap A for a
// hard-LEFT pluck, B for a hard-RIGHT pluck (instrument_pan on two slots). The bottom drone
// auto-pans on its own (LFO_PAN).
//
// MOUSE: a sustained tone follows the pointer — pan from cursor X (left↔right), pitch from
// cursor Y (up = higher). Hands-on positional audio: aim it anywhere in the field by ear.
#include "studio.h"

#define SL_PAD   5
#define SL_LEFT  6
#define SL_RIGHT 7
#define SL_DRONE 8
#define SL_MOUSE 9

typedef struct {
    float bx, bdir;     // ball x + direction
    int   pad;          // held pad handle (follows the ball)
    int   drone;        // held auto-pan drone handle
    int   mouse;        // held mouse-follow handle (pan = cursor x, pitch = cursor y)
    int   law;          // current pan law: 0 = PAN_LINEAR, 1 = PAN_POWER (SPACE toggles)
    int   started;
    int   tick;         // frame counter (tap throttle)
    float litL, litR;   // decaying flash on the L/R hit bars
} St;

void init(void) {
    instrument(SL_PAD,   INSTR_TRI,   40, 0, 6, 300);
    instrument(SL_LEFT,  INSTR_PLUCK,  2, 200, 0, 400);
    instrument(SL_RIGHT, INSTR_PLUCK,  2, 200, 0, 400);
    instrument(SL_DRONE, INSTR_SAW,  200, 0, 7, 400);
    instrument(SL_MOUSE, INSTR_TRI,  20, 0, 6, 300);

    instrument_pan(SL_LEFT,  -1.0f);   // hard left
    instrument_pan(SL_RIGHT, +1.0f);   // hard right

    // the mouse-follow tone: a gentle filter so the y→pitch sweep stays smooth
    instrument_filter(SL_MOUSE, FILTER_LOW, 1600, 4);

    // the auto-pan drone: a slow, full-width LFO sweep across the field
    instrument_filter(SL_DRONE, FILTER_LOW, 700, 6);
    instrument_lfo(SL_DRONE, 0, LFO_PAN, 0.4f, 1.0f);
}

void update(void) {
    St *s = (St *)de_state(sizeof(St));
    if (!s->started) {
        s->started = 1;
        s->bx = SCREEN_W / 2.0f;
        s->bdir = 1.0f;
        s->pad   = note_on(57, SL_PAD, 5);     // A3 pad, follows the ball
        s->drone = note_on(33, SL_DRONE, 4);   // low auto-pan drone
        s->mouse = note_on(60, SL_MOUSE, 4);   // mouse-follow tone (pitch set live below)
        note_glide(s->mouse, 60);              // smooth the y→pitch slides
    }
    s->tick++;

    // SPACE toggles the pan law live — A/B the two by ear (LINEAR center buildup vs
    // POWER equal-loudness sweep). LINEAR is the default; POWER is the opt-in.
    if (keyp(KEY_SPACE)) {
        s->law = !s->law;
        pan_law(s->law ? PAN_POWER : PAN_LINEAR);
    }

    // bounce the ball
    s->bx += s->bdir * 1.6f;
    if (s->bx < 8)            { s->bx = 8;            s->bdir =  1.0f; }
    if (s->bx > SCREEN_W - 8) { s->bx = SCREEN_W - 8; s->bdir = -1.0f; }

    // positional audio: pan the pad to the ball's screen position (-1 left .. +1 right)
    note_pan(s->pad, (s->bx - SCREEN_W / 2.0f) / (SCREEN_W / 2.0f));

    // mouse-follow tone: pan from cursor x, pitch from cursor y (top = high, bottom = low)
    note_pan(s->mouse, (mouse_x() - SCREEN_W / 2.0f) / (SCREEN_W / 2.0f));
    note_pitch(s->mouse, 48.0f + (1.0f - (float)mouse_y() / SCREEN_H) * 24.0f);  // 48..72, 2 octaves

    if (btn(0, BTN_A) && (s->tick % 18 == 0)) { note(69, SL_LEFT, 6);  s->litL = 1.0f; }
    if (btn(0, BTN_B) && (s->tick % 18 == 0)) { note(72, SL_RIGHT, 6); s->litR = 1.0f; }

    s->litL *= 0.90f;
    s->litR *= 0.90f;
}

void draw(void) {
    St *s = (St *)de_state(sizeof(St));
    cls(CLR_DARK_BLUE);

    line(SCREEN_W / 2, 0, SCREEN_W / 2, SCREEN_H, CLR_DARK_GREY);
    print("L", 6, 6, CLR_LIGHT_GREY);
    print("R", SCREEN_W - 12, 6, CLR_LIGHT_GREY);

    int by = SCREEN_H / 2;
    circfill((int)s->bx, by, 6, CLR_YELLOW);
    circ((int)s->bx, by, 6, CLR_WHITE);

    int lh = (int)(s->litL * 60);
    int rh = (int)(s->litR * 60);
    rectfill(10, SCREEN_H - 10 - lh, 12, lh, CLR_RED);
    rectfill(SCREEN_W - 22, SCREEN_H - 10 - rh, 12, rh, CLR_GREEN);

    // mouse cursor: crosshair marks where the mouse-follow tone is placed in the field
    int mx = mouse_x(), my = mouse_y();
    line(mx - 5, my, mx + 5, my, CLR_ORANGE);
    line(mx, my - 5, mx, my + 5, CLR_ORANGE);
    circ(mx, my, 3, CLR_ORANGE);

    print("ball pans the pad (note_pan)", SCREEN_W / 2 - 70, by + 16, CLR_LIGHT_GREY);
    print("mouse: x=pan  y=pitch", SCREEN_W / 2 - 56, by + 26, CLR_ORANGE);

    // pan-law readout — SPACE flips between the two
    print("SPACE toggles pan law:", 6, 18, CLR_MEDIUM_GREY);
    print(s->law ? "PAN_POWER (equal loudness)" : "PAN_LINEAR (center buildup)",
          6, 28, s->law ? CLR_GREEN : CLR_LIGHT_GREY);

    print("A=hard left  B=hard right", SCREEN_W / 2 - 64, SCREEN_H - 22, CLR_LIGHT_GREY);
    print("drone auto-pans (LFO_PAN)", SCREEN_W / 2 - 64, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
