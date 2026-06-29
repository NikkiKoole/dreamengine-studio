/* de:meta
{
  "title": "what is code?",
  "status": "active",
  "created": "2026-06-07",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "The very first lesson, before variables or loops: a program is a LIST of instructions the computer runs top to bottom, doing exactly what each one says. The code sits on the left, 'the screen' on the right - press NEXT (or SPACE) to run ONE line at a time and watch its mark appear. Eight lines of real studio.h calls (cls, rectfill, circfill, trifill, rect, print) build a little house, one step at a time, so you SEE the 1:1 mapping: this line -> this mark, in order, accumulating. The done lines get a green check, the current line a yellow arrow, and each shows its plain-English meaning ('a triangle - the roof'). Runs in a roomy 480x300 canvas so the code is readable at full size. Fully playable by button-tap or keyboard. R / OVER starts over and watches it build again."
}
de:meta */
// what is code? — the very first lesson, before variables or loops.
//
// THE ONE IDEA: a program is a list of instructions. The computer runs them
// top to bottom, in order, and does *exactly* what each one says — leaving a
// mark and moving on. It has no idea you're drawing a "house"; it just follows
// steps. We make that visible: the code sits on the left, "the screen" on the
// right, and you run ONE line at a time and watch its mark appear. Eight lines
// of code build a little house — and you saw every one of them do its job.
//
//   NEXT (button) / SPACE / Z  → run the next line
//   OVER (button) / R          → start over and watch it build again
//
// Runs in a 480×300 canvas (see whatiscode.cart.js) so the code listing can use
// the readable 8px font instead of being crammed into 320px.
#include "studio.h"
#include <stdio.h>

// the program the student watches run. `code` is shown in the listing; `say`
// is the plain-English meaning; the matching mark is drawn in draw_step().
typedef struct { const char *code, *say; } Line;
static const Line prog[] = {
    { "cls(CLR_BLUE);",                  "fill the screen blue - the sky" },
    { "rectfill(0,108,164,42,GREEN);",   "a green rectangle - the grass" },
    { "circfill(134,26,13,YELLOW);",     "a yellow circle - the sun" },
    { "rectfill(40,64,66,50,ORANGE);",   "a box - the house wall" },
    { "trifill(34,64,73,34,112,64,RED);","a triangle - the roof" },
    { "rectfill(62,90,18,24,BROWN);",    "a small box - the door" },
    { "rect(48,74,16,14,WHITE);",        "an outline - the window" },
    { "print(\"HOME\",116,120,WHITE);",  "letters on the screen" },
};
#define NLINE 8

#define BOX_X 306
#define BOX_Y 58
#define BOX_W 166
#define BOX_H 200
#define B(x) (BOX_X + (x))          // box-local x → screen x
#define BY(y) (BOX_Y + (y))         //          y → screen y

#define PTR_X  14                   // run-pointer column
#define NUM_X  26                   // line-number column
#define CODE_X 44                   // code-text column
#define LIST_Y 62                   // first listing line
#define LINE_H 14                   // listing line pitch

static int pc;        // how many lines have run (0..NLINE)
static int flash;     // frames since the last line ran — for the "pop"

// draw the mark that line i makes, in box-local coordinates
static void draw_step(int i) {
    switch (i) {
        case 0: rectfill(B(0), BY(0), BOX_W, BOX_H, CLR_BLUE); break;
        case 1: rectfill(B(0), BY(108), 164, 42, CLR_GREEN); break;
        case 2: circfill(B(134), BY(26), 13, CLR_YELLOW); break;
        case 3: rectfill(B(40), BY(64), 66, 50, CLR_ORANGE); break;
        case 4: trifill(B(34), BY(64), B(73), BY(34), B(112), BY(64), CLR_RED); break;
        case 5: rectfill(B(62), BY(90), 18, 24, CLR_BROWN); break;
        case 6: rect(B(48), BY(74), 16, 14, CLR_WHITE); break;   // exactly the listed call — one rect, one mark
        case 7: print("HOME", B(116), BY(120), CLR_WHITE); break;
    }
}

static void advance(void) {     // run the next line — or loop back when done
    if (pc < NLINE) { pc++; flash = 0; hit(58 + pc * 2, INSTR_SQUARE, 3, 45); }
    else            { pc = 0; flash = 0; }
}
static void restart(void) { pc = 0; flash = 0; }

// a tappable button; returns true the frame it's pressed (touch). keys are read
// in update() so they're not double-counted.
static bool button(int x, int y, int w, int h, const char *label, bool lit) {
    bool hit = tapp(x, y, w, h);
    rectfill(x, y, w, h, lit ? CLR_DARK_GREEN : CLR_DARKER_GREY);
    rect(x, y, w, h, CLR_LIGHT_GREY);
    print_centered(label, x + w / 2, y + h / 2 - 3, CLR_WHITE);
    return hit;
}

void update(void) {
    if (flash < 60) flash++;
    if (keyp(KEY_SPACE) || btnp(0, BTN_A)) advance();   // touch taps → buttons in draw()
    if (keyp('R')) restart();
#ifdef DE_TRACE
    watch("pc", "%d", pc);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    print_scaled("WHAT IS CODE?", 14, 10, CLR_LIGHT_YELLOW, 2);
    font(FONT_SMALL);
    print("a program is a LIST of steps. the computer runs them one at a time,", 16, 34, CLR_MEDIUM_GREY);
    print("top to bottom, doing exactly what each line says.", 16, 43, CLR_MEDIUM_GREY);

    // ── left: the code listing (readable 8px font) ──
    font(FONT_NORMAL);
    for (int i = 0; i < NLINE; i++) {
        int ly = LIST_Y + i * LINE_H;
        bool done = i < pc, cur = (i == pc);
        int col = cur ? CLR_LIGHT_YELLOW : done ? CLR_LIME_GREEN : CLR_DARK_GREY;
        if (cur) rectfill(10, ly - 2, 286, LINE_H, CLR_DARKER_GREY);
        print(cur ? ">" : done ? "+" : " ", PTR_X, ly, col);
        char ln[6]; snprintf(ln, sizeof ln, "%d", i + 1);
        print(ln, NUM_X, ly, CLR_DARK_GREY);
        print(prog[i].code, CODE_X, ly, col);
    }

    // the plain-English meaning of the line that just ran
    if (pc > 0) {
        print("just ran:", 14, 184, CLR_DARK_GREY);
        print(prog[pc - 1].say, 92, 184, CLR_WHITE);
    }

    // ── buttons ──
    bool atEnd = pc >= NLINE;
    if (button(14, 208, 120, 26, atEnd ? "AGAIN" : "NEXT", !atEnd)) advance();
    if (button(146, 208, 84, 26, "OVER", false)) restart();

    font(FONT_SMALL);
    char p[24]; snprintf(p, sizeof p, "line %d of %d", pc, NLINE);
    print(p, 14, 246, CLR_LIGHT_GREY);
    print("SPACE / NEXT to run a line   -   R to start over", 14, 256, CLR_DARK_GREY);

    // ── right: "the screen" the code draws on ──
    font(FONT_SMALL);
    print("the screen", BOX_X, BOX_Y - 10, CLR_MEDIUM_GREY);
    clip(BOX_X, BOX_Y, BOX_W, BOX_H);
    rectfill(BOX_X, BOX_Y, BOX_W, BOX_H, CLR_DARK_BLUE);   // blank screen before any code
    for (int i = 0; i < pc; i++) draw_step(i);             // replay every mark so far
    if (pc > 0 && flash < 8)                               // a brief frame-flash: "something happened"
        rect(BOX_X + flash, BOX_Y + flash, BOX_W - flash * 2, BOX_H - flash * 2, CLR_WHITE);
    clip(0, 0, SCREEN_W, SCREEN_H);
    rect(BOX_X - 1, BOX_Y - 1, BOX_W + 2, BOX_H + 2, CLR_LIGHT_GREY);

    if (atEnd) {
        font(FONT_SMALL);
        print_centered("8 lines of code drew a house!", BOX_X + BOX_W / 2, BOX_Y + BOX_H + 6,
                       blink(20) ? CLR_LIME_GREEN : CLR_GREEN);
    }
}
