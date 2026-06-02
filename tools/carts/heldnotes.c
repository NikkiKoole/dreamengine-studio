#include "studio.h"

// HELD NOTES — a theremin. Press and HOLD anywhere on the pad: one sustained voice
// starts (note_on) and rings until you let go (note_off). While you hold, the mouse
// drives it LIVE — X slides the pitch, Y sweeps the filter cutoff. None of this is
// possible with fire-and-forget note(): that plays a fixed blip and forgets it.
//
//   note_on(midi, slot, vol) -> handle   start a sustained voice
//   note_pitch (handle, midi)             slide its pitch (smoothed, no click)
//   note_cutoff(handle, hz)               sweep its filter cutoff
//   note_off   (handle)                   release it (envelope fades out)

#define SLOT 5
#define PAD_X 8
#define PAD_Y 26
#define PAD_W (SCREEN_W - 16)
#define PAD_H (SCREEN_H - 52)

int   voice = -1;          // handle of the held note, or -1 when silent
float lo = 45, hi = 81;    // pitch range across the pad (X)

void init(void) {
    // a warm sawtooth pad with a resonant lowpass + a touch of vibrato, so the
    // live filter sweep (note_cutoff) and pitch glide (note_pitch) are audible.
    instrument(SLOT, INSTR_SAW, 40, 120, 6, 350);
    instrument_filter(SLOT, FILTER_LOW, 1600, 11);
    instrument_lfo(SLOT, 0, LFO_PITCH, 5.0f, 0.18f);   // gentle vibrato
}

float pad_midi(int mx)  { return lo + clamp((float)(mx - PAD_X) / PAD_W, 0, 1) * (hi - lo); }
int   pad_cutoff(int my){ return 200 + (int)((1.0f - clamp((float)(my - PAD_Y) / PAD_H, 0, 1)) * 3600); } // top = bright

void update(void) {
    int mx = mouse_x(), my = mouse_y();
    bool on_pad = mx >= PAD_X && mx < PAD_X + PAD_W && my >= PAD_Y && my < PAD_Y + PAD_H;

    if (mouse_pressed(MOUSE_LEFT) && on_pad && voice < 0)
        voice = note_on((int)pad_midi(mx), SLOT, 5);     // start sustaining

    if (voice >= 0 && mouse_down(MOUSE_LEFT)) {
        note_pitch (voice, pad_midi(mx));                // X → pitch, live
        note_cutoff(voice, pad_cutoff(my));              // Y → filter, live
    }
    if (voice >= 0 && mouse_released(MOUSE_LEFT)) {
        note_off(voice);                                 // let go → release
        voice = -1;
    }
}

void draw(void) {
    int mx = mouse_x(), my = mouse_y();
    bool held = voice >= 0;

    cls(CLR_DARKER_BLUE);
    print("HELD NOTES - theremin", 8, 6, CLR_WHITE);
    print("hold the pad: X = pitch, Y = filter", 8, 15, CLR_INDIGO);

    // pad
    rectfill(PAD_X, PAD_Y, PAD_W, PAD_H, held ? CLR_DARK_PURPLE : CLR_DARKER_PURPLE);
    rect(PAD_X, PAD_Y, PAD_W, PAD_H, held ? CLR_PINK : CLR_INDIGO);
    for (int gx = PAD_X; gx < PAD_X + PAD_W; gx += 24) line(gx, PAD_Y, gx, PAD_Y + PAD_H, CLR_DARKER_GREY);
    for (int gy = PAD_Y; gy < PAD_Y + PAD_H; gy += 24) line(PAD_X, gy, PAD_X + PAD_W, gy, CLR_DARKER_GREY);

    // live crosshair while held
    if (held) {
        int cx = clamp(mx, PAD_X, PAD_X + PAD_W);
        int cy = clamp(my, PAD_Y, PAD_Y + PAD_H);
        line(cx, PAD_Y, cx, PAD_Y + PAD_H, CLR_PINK);
        line(PAD_X, cy, PAD_X + PAD_W, cy, CLR_PINK);
        circfill(cx, cy, 4, CLR_WHITE);
        circ(cx, cy, 6 + (int)(sin_deg(now() * 540) * 2), CLR_YELLOW);
        print(str("note %d   cutoff %d hz", (int)pad_midi(cx), pad_cutoff(cy)), PAD_X + 4, PAD_Y + PAD_H - 10, CLR_LIGHT_YELLOW);
    } else {
        print("press & hold", PAD_X + PAD_W / 2 - 36, PAD_Y + PAD_H / 2 - 3, CLR_MEDIUM_GREY);
    }

    print(held ? "SUSTAINING" : "silent", 8, SCREEN_H - 12, held ? CLR_GREEN : CLR_DARK_GREY);
    print_right("vs note() = a fixed blip", SCREEN_W - 8, SCREEN_H - 12, CLR_DARK_GREY);
}
