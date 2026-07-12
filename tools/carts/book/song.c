// Chapter-7 illustration (a VIDEO — it has sound): a tiny step sequencer. Sixteen
// steps sweep left to right; each lit block plays its note on a plucked string.
// Pentatonic, so it can't sound wrong. Visualised as a piano-roll you can hear.
#include "studio.h"

#define SL     5
#define STEPF  9                                  // frames per step
static const int mel[16] = { 72,69,67,64, 67,64,62,60, 64,67,69,72, 69,67,64,-1 };
static int last = -1;

void init(void) { instrument(SL, INSTR_PLUCK, 2, 0, 7, 500); }

void update(void) {
    int step = (frame() / STEPF) % 16;
    if (step != last) {                            // just landed on a new step
        last = step;
        if (mel[step] >= 0) note(mel[step], SL, 5);
    }
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    int step = (frame() / STEPF) % 16, colw = SCREEN_W / 16;
    for (int i = 0; i < 16; i++) {
        if (mel[i] < 0) continue;
        int h = 20 + (mel[i] - 58) * 7;            // higher note → taller block
        int y = 172 - h;
        int col = 8 + (mel[i] - 60 + 60) % 6;      // colour by pitch
        int lit = (i == step);
        rectfill(i * colw + 3, y, colw - 6, h, lit ? CLR_WHITE : col);
        if (lit) {
            rect(i * colw + 3, y, colw - 6, h, CLR_YELLOW);
            circfill(i * colw + colw / 2, y - 11, 5, CLR_YELLOW);   // it's singing
        }
    }
    line(step * colw, 22, step * colw, 176, CLR_YELLOW);   // the playhead
    line(0, 176, SCREEN_W, 176, CLR_DARK_GREY);
    print("\x0e", 8, 8, CLR_LIGHT_GREY);           // a little note glyph, if present
}
