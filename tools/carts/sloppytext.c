// sloppytext — "hand-lettered" text. Instead of one font drawn straight, each
// glyph picks from a couple of fonts and gets nudged + tilted a touch, so a
// plain string reads like it was scribbled or cut from a ransom note.
//
// Pure cart-land: just font() + print_rot() + text_width(), no engine changes.
// The wobble is DETERMINISTIC per character (a hash of its index), so it looks
// deliberately wonky and sits still — not a flickering mess. One line opts into
// a slow time-wobble to show it can also breathe.
//
//   the knobs: ang (max tilt °), dy (max vertical nudge px), the font set, and
//   an optional live wobble. Copy print_sloppy() into any cart that wants it.
#include "studio.h"
#include <stdio.h>
#include <math.h>

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))
STATE { int frame; };

// cheap deterministic pseudo-random in [0,1) from a 32-bit seed (PCG-ish hash)
static float prand(unsigned seed) {
    seed = seed * 747796405u + 2891336453u;
    seed = ((seed >> ((seed >> 28) + 4)) ^ seed) * 277803737u;
    seed = (seed >> 22) ^ seed;
    return (seed & 0xffffu) / 65536.0f;
}

// SUBTLE sloppy — two same-size 8×8 fonts (NORMAL + THIN), small tilt + nudge.
// Both fonts are 8px tall so the line stays even; the wonk is all tilt/jitter.
//   maxang  max tilt in degrees (± per glyph)
//   maxdy   max vertical nudge in pixels (± per glyph)
//   wob     0 = dead still; >0 adds a slow live wobble (needs `frame`)
static int print_sloppy(const char *s, int x, int y, int color,
                        float maxang, int maxdy, float wob, int frame) {
    static const int fonts[] = { FONT_NORMAL, FONT_THIN };
    int cx = x;
    for (int i = 0; s[i]; i++) {
        if (s[i] == ' ') { cx += 6; continue; }
        unsigned h = (unsigned)i * 2654435761u ^ (unsigned)(unsigned char)s[i];
        font(fonts[(h >> 3) & 1u]);                       // alternate the two fonts, hashed
        float a = (prand(h) * 2.0f - 1.0f) * maxang;        // steady per-glyph tilt
        int   dy = (int)((prand(h ^ 0x9e37u) * 2.0f - 1.0f) * maxdy);
        if (wob > 0.0f) a += sinf(frame * 0.05f + i * 0.7f) * wob;  // optional breathing
        char ch[2] = { s[i], 0 };
        print_rot(ch, cx, y + dy, a, color, 0);           // pivot 0 = top-left anchor
        cx += text_width(ch) + 1;                         // advance by THIS glyph's width
    }
    font(FONT_NORMAL);
    return cx;
}

// WILD / ransom-note — the full jumble: every glyph picks from ALL five fonts
// (TINY 3×5 → COMIC 10×20) AND a random scale, so sizes lurch from tiny to huge.
// All bottom-aligned to one baseline so it still reads as a line. The engine can't
// rotate AND scale in one call (cart-land), so the split is: 1× glyphs tilt
// (print_rot), scaled-up glyphs stand upright and big (print_scaled). The size
// chaos carries it. Scale is capped per font (small fonts blow up most, COMIC
// stays 1× — it's already 20px) so a single letter never runs off the line.
static int print_wild(const char *s, int x, int baseline, int color, float maxang) {
    static const int fonts[]   = { FONT_TINY, FONT_SMALL, FONT_NORMAL, FONT_THIN, FONT_COMIC };
    static const int heights[] = { 5,         6,          8,           8,         20 };
    int cx = x;
    for (int i = 0; s[i]; i++) {
        if (s[i] == ' ') { cx += 8; continue; }
        unsigned h = (unsigned)i * 2654435761u ^ (unsigned)(unsigned char)s[i] * 40503u;
        int fi = (h >> 5) % 5u;
        font(fonts[fi]);
        int want  = 1 + (int)(prand(h) * 3.0f);           // roll 1..3
        int cap   = 26 / heights[fi]; if (cap < 1) cap = 1;  // tiny→5, small→4, normal→3, comic→1
        int scale = want < cap ? want : cap;
        char ch[2] = { s[i], 0 };
        int w   = text_width(ch) * scale;
        int top = baseline - heights[fi] * scale;         // bottom-align the (scaled) cell
        if (scale == 1) print_rot(ch, cx, top, (prand(h ^ 0x55u) * 2.0f - 1.0f) * maxang, color, 0);
        else            print_scaled(ch, cx, top, color, scale);   // big upright pop
        cx += w + 2;
    }
    font(FONT_NORMAL);
    return cx;
}

void update(void) { S->frame++; }

void draw(void) {
    cls(CLR_DARK_BLUE);
    font(FONT_NORMAL);
    print("sloppy text", 6, 6, CLR_DARK_GREY);

    // 1) barely-there: looks like a slightly shaky old printer
    print("subtle (+-3, +-1):", 6, 28, CLR_INDIGO);
    print_sloppy("the quick brown fox", 6, 40, CLR_WHITE, 3.0f, 1, 0.0f, 0);

    // 2) wonkier: clearly hand-placed
    print("wonky (+-9, +-2):", 6, 66, CLR_INDIGO);
    print_sloppy("jumps over the lazy dog", 6, 78, CLR_YELLOW, 9.0f, 2, 0.0f, 0);

    // 3) alive: a slow drunken sway
    print("live wobble:", 6, 104, CLR_INDIGO);
    print_sloppy("wibbly wobbly", 6, 116, CLR_GREEN, 5.0f, 1, 4.0f, S->frame);

    // 4) wild: all five fonts + random scale-up, bottom-aligned. tiny→huge jumble
    print("wild (tiny..big, scaled up):", 6, 144, CLR_INDIGO);
    print_wild("ransom NOTE", 6, 196, CLR_RED, 10.0f);
}
