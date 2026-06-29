/* de:meta
{
  "title": "Sloppy text",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "Hand-lettered / ransom-note text: instead of one font drawn straight, each glyph picks from a couple of fonts and gets a small tilt + vertical nudge, so a plain string reads like it was scribbled or cut-and-pasted. Pure cart-land — just font() + print_rot() + text_width(), no engine changes. The wobble is deterministic per character (a hash of its index) so it sits still and looks deliberately wonky rather than flickering. Shows four intensities: a barely-there shaky-printer look, a wonkier hand-placed line, a slow live 'drunken sway', and a ransom note that mixes the big COMIC font in among the 8x8 ones, bottom-aligned so the baseline holds. Copy print_sloppy() into any cart that wants the effect."
}
de:meta */
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

// WILD / ransom-note — the size jumble, done so it stays crisp. Big letters come
// from SCALING UP the 8×8 fonts (NORMAL/THIN double cleanly to 16/24px) or from
// dropping in COMIC (already a chunky 20px) — never from scaling the little fonts,
// which just turns to mush. SMALL appears at 1× as the odd tiny accent. All
// bottom-aligned to one baseline. Every glyph both tilts AND scales in one call
// via print_rot_scaled, so even the big letters lean. Per glyph we roll:
//   ~50% normal 8×8   ~20% BIG scaled 8×8 (2-3x)   ~15% COMIC   ~15% small accent
static int print_wild(const char *s, int x, int baseline, int color, float maxang) {
    int cx = x;
    for (int i = 0; s[i]; i++) {
        if (s[i] == ' ') { cx += 10; continue; }
        unsigned h = (unsigned)i * 2654435761u ^ (unsigned)(unsigned char)s[i] * 40503u;
        int roll = h % 20u;
        int fnt, scale, gh;
        if      (roll < 10) { fnt = (h & 1u) ? FONT_THIN : FONT_NORMAL; scale = 1;                 gh = 8;          }
        else if (roll < 14) { fnt = (h & 1u) ? FONT_THIN : FONT_NORMAL; scale = 2 + (int)((h >> 5) & 1u); gh = 8 * scale; }
        else if (roll < 17) { fnt = FONT_COMIC;                         scale = 1;                 gh = 20;         }
        else                { fnt = FONT_SMALL;                         scale = 1;                 gh = 6;          }
        font(fnt);
        char ch[2] = { s[i], 0 };
        int w   = text_width(ch) * scale;
        int top = baseline - gh;                          // bottom-align the (scaled) cell
        float a = (prand(h ^ 0x55u) * 2.0f - 1.0f) * maxang;
        print_rot_scaled(ch, cx, top, a, scale, color, 0);  // tilt AND scale in one call
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
    print("wild (8x8 scaled up + comic):", 6, 144, CLR_INDIGO);
    print_wild("ransom NOTE", 6, 196, CLR_RED, 10.0f);
}
