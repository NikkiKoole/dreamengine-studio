/* de:meta
{
  "slug": "text-fx",
  "title": "text fx",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "genre": "lab",
  "description": "Animated text — the dialogue/title/HUD toolkit, eight effects each a 5-15 line technique you can lift straight into a game: typewriter (with the tick sound that sells it — X toggles), per-char wave, shine sweep, wipe reveal, damage-number shake, rainbow, and an outlined bounce. Two tools do most of the work: print() returns the x after what it drew, so printing one character at a time gives every char its own offset/color; and clip() turns into a reveal machine — a growing window wipes text in, a narrow moving window sweeps a shine across it. All effects loop forever; watch, then steal the one you need."
}
de:meta */
#include "studio.h"

// TEXT FX — animated text, the dialogue/title/HUD toolkit. Plain print() is
// the baseline; every row below it is a tiny technique (5–15 lines each) you
// can lift straight into a game. Two tools do most of the work:
//
//  • PER-CHARACTER PRINTING — print() returns the x after what it drew, so
//    printing one character at a time lets every char have its own y offset
//    (wave), color (rainbow), or jitter (shake).
//
//  • CLIP() AS A REVEAL — clip(x,y,w,h) limits drawing to a window. Print the
//    text inside a GROWING window and it wipes in; print a bright copy inside
//    a narrow MOVING window on top of a dim copy and a shine sweeps across.
//    clip(0,0,0,0) resets.
//
// The typewriter also shows the sound half: a tiny tick per letter sells the
// effect more than the visual does (X toggles it).
//
// All effects loop forever — watch, then steal the one you need.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

STATE { bool sound_on; int last_n; };

void init(void) { S->sound_on = true; }

// print one char, return the advance — the building block for per-char effects
static int print_ch(char c, int x, int y, int col) {
    char b[2] = { c, 0 };
    return print(b, x, y, col);
}

void update(void) {
    if (btnp(0, BTN_B)) S->sound_on = !S->sound_on;

    // typewriter tick — fires once per revealed letter (see draw for n)
    const char *tw = "an old man gives you a sword...";
    int len = 0; while (tw[len]) len++;
    int n = (frame() / 5 + 12) % (len + 30);       // +12 phase: never starts blank
    if (n != S->last_n && n > S->last_n && n <= len && S->sound_on)
        hit(78, INSTR_NOISE, 1, 20);
    S->last_n = n;
}

void draw(void) {
    cls(CLR_BLACK);
    int f = frame();
    int lx = 6, tx = 96;                           // label column, effect column

    font(FONT_SMALL);

    // ── 0: plain — the baseline everything else is measured against ─────────
    int y = 22;
    print("plain", lx, y + 1, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    print("HELLO, WORLD", tx, y, CLR_LIGHT_GREY);
    font(FONT_SMALL);

    // ── 1: typewriter — show only the first n characters ────────────────────
    y += 21;
    print("typewriter", lx, y + 1, CLR_MEDIUM_GREY);
    {
        const char *t = "an old man gives you a sword...";
        int len = 0; while (t[len]) len++;
        int n = (f / 5 + 12) % (len + 30); if (n > len) n = len;
        char buf[64];
        for (int i = 0; i < n; i++) buf[i] = t[i];
        buf[n] = 0;
        font(FONT_NORMAL);
        int xe = print(buf, tx, y, CLR_WHITE);
        if (n < len && (f / 8) % 2) print("_", xe, y, CLR_WHITE);   // blinking caret
        font(FONT_SMALL);
    }

    // ── 2: wave — each char bobs on its own sine phase ───────────────────────
    y += 21;
    print("wave", lx, y + 1, CLR_MEDIUM_GREY);
    {
        const char *t = "~ under the sea ~";
        font(FONT_NORMAL);
        int x = tx;
        for (int i = 0; t[i]; i++)
            x = print_ch(t[i], x, y + (int)(sin_deg(f * 6 + i * 38) * 3), CLR_BLUE);
        font(FONT_SMALL);
    }

    // ── 3: shine — a bright copy inside a narrow moving clip window ─────────
    y += 21;
    print("shine", lx, y + 1, CLR_MEDIUM_GREY);
    {
        const char *t = "PRESS START";
        font(FONT_NORMAL);
        int w = text_width(t);
        print(t, tx, y, CLR_DARK_GREY);                         // dim base
        int sx = (f * 2) % (w + 80) - 40;                        // the sweep
        clip(tx + sx, y, 16, 10);
        print(t, tx, y, CLR_WHITE);                              // bright copy
        clip(0, 0, 0, 0);
        font(FONT_SMALL);
    }

    // ── 4: wipe — reveal inside a growing clip window ────────────────────────
    y += 21;
    print("wipe", lx, y + 1, CLR_MEDIUM_GREY);
    {
        const char *t = "CHAPTER ONE";
        font(FONT_NORMAL);
        int w = text_width(t);
        int reveal = (f * 2) % (w + 90);                         // +90 = hold, then loop
        clip(tx, y, reveal, 10);
        print(t, tx, y, CLR_LIGHT_PEACH);
        clip(0, 0, 0, 0);
        font(FONT_SMALL);
    }

    // ── 5: shake — per-char jitter; the damage-number look ──────────────────
    y += 21;
    print("shake", lx, y + 1, CLR_MEDIUM_GREY);
    {
        const char *t = "-42 CRITICAL!";
        font(FONT_NORMAL);
        int x = tx;
        for (int i = 0; t[i]; i++)
            x = print_ch(t[i], x + rnd_between(-1, 2) - 0,       // tiny x wobble
                         y + rnd_between(-1, 2), CLR_RED);
        font(FONT_SMALL);
    }

    // ── 6: rainbow — per-char color, phase shifted by index ─────────────────
    y += 21;
    print("rainbow", lx, y + 1, CLR_MEDIUM_GREY);
    {
        const char *t = "RAINBOW ROAD";
        static const int cols[6] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_PINK };
        font(FONT_NORMAL);
        int x = tx;
        for (int i = 0; t[i]; i++)
            x = print_ch(t[i], x, y, cols[(i + f / 6) % 6]);
        font(FONT_SMALL);
    }

    // ── 7: bounce — the whole line hops on a rectified sine arc ─────────────
    y += 21;
    print("bounce", lx, y + 1, CLR_MEDIUM_GREY);
    {
        float s = sin_deg(f * 4);
        int hop = (int)((s < 0 ? -s : s) * 5);                   // |sin| = bounce arc
        font(FONT_NORMAL);
        print_outline("LEVEL UP!", tx, y - hop, CLR_YELLOW, CLR_DARK_BROWN);
        font(FONT_SMALL);
    }

    font(FONT_NORMAL);
    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("text fx", 4, 2, CLR_WHITE);
    print(S->sound_on ? "tick ON (X)" : "tick OFF(X)", SCREEN_W - 92, 2,
          S->sound_on ? CLR_GREEN : CLR_LIGHT_GREY);
}
