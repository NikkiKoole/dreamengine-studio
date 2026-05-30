#include "studio.h"

// An LFO is one slow sine wave that wobbles a single knob of an instrument.
// Same call, three destinations:
//   LFO_PITCH  -> vibrato      LFO_VOLUME -> tremolo      LFO_DUTY -> PWM shimmer
// This cart defines one of each, holds a long note on it, and animates what moves.

#define VIB 5
#define TRM 6
#define PWM 7

typedef struct { const char *name; int slot; int dest; float rate; float depth; int col; } Mod;
Mod mods[3];
int cur = 0;

void init() {
    instrument(VIB, INSTR_SINE,   40, 0, 7, 300);
    instrument(TRM, INSTR_SAW,    20, 0, 7, 200);
    instrument(PWM, INSTR_SQUARE, 20, 0, 7, 200);

    instrument_lfo(VIB, 0, LFO_PITCH,  6.0f, 0.5f);   // bob the pitch +/- half a semitone
    instrument_lfo(TRM, 0, LFO_VOLUME, 9.0f, 0.9f);   // pump the volume up and down
    instrument_lfo(PWM, 0, LFO_DUTY,   2.5f, 0.35f);  // sweep the pulse width

    mods[0] = (Mod){ "VIBRATO", VIB, LFO_PITCH,  6.0f, 0.5f,  CLR_YELLOW };
    mods[1] = (Mod){ "TREMOLO", TRM, LFO_VOLUME, 9.0f, 0.9f,  CLR_GREEN  };
    mods[2] = (Mod){ "PWM",     PWM, LFO_DUTY,   2.5f, 0.35f, CLR_BLUE   };
    bpm(90);
}

void update() {
    for (int i = 0; i < 3; i++) if (keyp('1' + i)) cur = i;

    // every 2 beats, the next instrument holds a long note so you hear its LFO breathe
    if (every(2)) {
        cur = (beat() / 2) % 3;
        hit(degree(SCALE_PENTA, 4, 2), mods[cur].slot, 6, 1400);
    }
    if (btnp(0, BTN_A)) hit(degree(SCALE_PENTA, 4, 2), mods[cur].slot, 6, 1400);
}

void draw() {
    cls(CLR_DARK_BLUE);
    print("LFO: one sine wobbling one knob", 8, 6, CLR_WHITE);
    print("1-3 select   Z plays selected", 8, 18, CLR_LIGHT_GREY);

    for (int i = 0; i < 3; i++) {
        Mod m = mods[i];
        int x = 8 + i * 102, y = 34, w = 96, h = 132;
        rect(x, y, w, h, i == cur ? CLR_WHITE : CLR_DARK_GREY);
        print(m.name, x + 6, y + 6, m.col);

        float v  = sin_deg(now() * m.rate * 360.0f);   // -1..1, same speed as the audio LFO
        int   cx = x + w / 2, cy = y + 72;

        if (m.dest == LFO_PITCH) {                     // a note marker riding a sine
            for (int px = 0; px < w - 16; px += 3) {
                float vv = sin_deg((now() + px * 0.012f) * m.rate * 360.0f);
                pset(x + 8 + px, cy - (int)(vv * 26), CLR_DARK_GREY);
            }
            circfill(cx, cy - (int)(v * 26), 5, m.col);
        } else if (m.dest == LFO_VOLUME) {             // a bar that pumps up and down
            int bh = (int)((0.5f + 0.5f * v) * 56);
            rectfill(cx - 16, cy + 28 - bh, 32, bh, m.col);
            rect(cx - 16, cy - 28, 32, 56, CLR_DARK_GREY);
        } else {                                       // pulse drawn at the current width
            float duty = 0.5f + v * m.depth;
            int cw = 24, top = cy - 24, bot = cy + 24;
            for (int c = 0; c < 3; c++) {
                int sx = x + 10 + c * cw;
                int hw = (int)(cw * duty);
                line(sx, bot, sx, top, m.col);
                line(sx, top, sx + hw, top, m.col);
                line(sx + hw, top, sx + hw, bot, m.col);
                line(sx + hw, bot, sx + cw, bot, m.col);
            }
        }
        print(str("%dHz", (int)m.rate), x + 6, y + h - 14, CLR_LIGHT_GREY);
    }
}
