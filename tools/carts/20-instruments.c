/* de:meta
{
  "slug": "20-instruments",
  "title": "instruments",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "An instrument is a waveform + an ADSR envelope (+ pulse duty), defined once with instrument() then played like any wave: note(midi, slot, vol). Defines four — PLUCK, PAD, LEAD, STAB — and draws each one's volume-over-time envelope so you can SEE the difference a fast vs slow attack and a short vs long release makes. An arpeggio cycles through all four so you hear them. 1-4 select, Z plays the selected one.",
  "todo": [
    "Add an onscreen toggle to stop/start the auto-play.",
    "Make the four instrument blocks clickable — play when clicked."
  ]
}
de:meta */
#include "studio.h"

// Instruments: a slot = a waveform + an ADSR envelope (+ pulse duty).
// Define a slot once with instrument(), then play it like any wave.
// This cart defines four and shows each one's envelope shape on screen.

#define PLUCK 5
#define PAD   6
#define LEAD  7
#define STAB  8

// the same numbers we pass to instrument(), kept so we can also DRAW the envelope
typedef struct { const char *name; int wave; int a, d, sus, r; float duty; int col; } Patch;
Patch patches[4];
int   sel = 0;        // which instrument the Z key plays
float flash[4];       // brief highlight when an instrument fires
int   ri = 0;         // arpeggio position

void init() {
    //                 name      wave          A    D  sus   R    duty   colour
    patches[0] = (Patch){ "PLUCK", INSTR_SAW,      2,  90,  1, 120, 0.50f, CLR_YELLOW };
    patches[1] = (Patch){ "PAD",   INSTR_SINE,   300,   0,  7, 500, 0.50f, CLR_BLUE   };
    patches[2] = (Patch){ "LEAD",  INSTR_SQUARE,   6,  40,  5, 150, 0.12f, CLR_GREEN  };
    patches[3] = (Patch){ "STAB",  INSTR_TRI,      2, 220,  0,  60, 0.50f, CLR_RED    };

    for (int i = 0; i < 4; i++) {
        instrument(PLUCK + i, patches[i].wave, patches[i].a, patches[i].d, patches[i].sus, patches[i].r);
        instrument_duty(PLUCK + i, patches[i].duty);
    }
    bpm(150);
}

void update() {
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) sel = i;

    // auto-arpeggio: one note per beat, cycling through the four instruments so you hear each
    if (every(1)) {
        int riff[] = { 0, 2, 4, 7, 4, 2 };
        note(degree(SCALE_PENTA, 4, riff[ri % 6]), PLUCK + (ri % 4), 6);
        flash[ri % 4] = 1.0f;
        ri++;
    }

    // play the selected instrument by hand
    if (btnp(0, BTN_A)) { note(degree(SCALE_PENTA, 4, 0), PLUCK + sel, 6); flash[sel] = 1.0f; }

    for (int i = 0; i < 4; i++) if (flash[i] > 0) flash[i] -= dt() * 3.0f;
}

// draw an ADSR envelope as a little line graph inside the given box
void draw_env(Patch p, int x, int y, int w, int h) {
    int   hold  = 120;                        // pretend-sustain length, just for the picture
    float total = p.a + p.d + hold + p.r;
    if (total < 1) total = 1;
    float sus = p.sus / 7.0f;
    int ax = x + (int)(w * (p.a / total));
    int dx = ax + (int)(w * (p.d / total));
    int hx = dx + (int)(w * (hold / total));
    int ex = hx + (int)(w * (p.r / total));
    int by = y + h;                           // baseline (silent)
    int sy = y + (int)(h * (1.0f - sus));     // sustain level
    line(x,  by, ax, y,  p.col);              // attack: 0 -> 1
    line(ax, y,  dx, sy, p.col);              // decay:  1 -> sustain
    line(dx, sy, hx, sy, p.col);              // sustain hold
    line(hx, sy, ex, by, p.col);              // release: sustain -> 0
}

void draw() {
    cls(CLR_DARK_BLUE);
    print("INSTRUMENT = wave + ADSR + duty", 8, 6, CLR_WHITE);
    print("1-4 select   Z plays selected", 8, 18, CLR_LIGHT_GREY);

    for (int i = 0; i < 4; i++) {
        Patch p = patches[i];
        int x = 8 + i * 76, y = 34, w = 68, h = 132;
        rectfill(x, y, w, h, flash[i] > 0 ? CLR_DARK_GREY : CLR_BLACK);
        rect(x, y, w, h, i == sel ? CLR_WHITE : CLR_DARK_GREY);
        print(p.name, x + 6, y + 6, p.col);
        draw_env(p, x + 6, y + 22, w - 12, 70);
        print(str("A%d", p.a), x + 4, y + h - 26, CLR_LIGHT_GREY);
        print(str("R%d", p.r), x + 4, y + h - 14, CLR_LIGHT_GREY);
    }

    print("each shape is its volume over time", 8, 188, CLR_DARK_GREY);
}
