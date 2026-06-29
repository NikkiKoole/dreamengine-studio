/* de:meta
{
  "title": "Fonts",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "Showcases the three built-in fonts (FONT_NORMAL 8×8, FONT_SMALL 4×6, FONT_TINY 3×5), print_shadow, print_outline, and the chained print return value."
}
de:meta */
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);

    // ── title ──────────────────────────────────────────────
    print_outline("FONT SHOWCASE", (SCREEN_W - text_width("FONT SHOWCASE")) / 2, 6, CLR_YELLOW, CLR_DARK_BROWN);

    // ── FONT_NORMAL (8x8) ──────────────────────────────────
    font(FONT_NORMAL);
    print("FONT_NORMAL  (8x8)", 8, 26, CLR_WHITE);
    print("drop shadow", 9, 37, CLR_BLACK);
    print("drop shadow", 8, 36, CLR_LIGHT_GREY);
    print_outline("outline text", 8, 46, CLR_WHITE, CLR_DARK_PURPLE);

    // chained print — shows return-x usage
    int x = print("chain: ", 8, 58, CLR_LIGHT_GREY);
    x = print("red ", x, 58, CLR_RED);
    x = print("green ", x, 58, CLR_GREEN);
    print("blue", x, 58, CLR_BLUE);

    // ── FONT_SMALL (4x6) ──────────────────────────────────
    font(FONT_SMALL);
    print("FONT_SMALL  (4x6) — fits ~64 chars across 320px", 8, 76, CLR_ORANGE);
    print("ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 !?.,;:", 8, 84, CLR_WHITE);
    print("shadow on small font", 9, 93, CLR_BLACK);
    print("shadow on small font", 8, 92, CLR_LIGHT_GREY);

    // show text_width respects active font
    const char *sw = "text_width uses active font";
    int sw_x = (SCREEN_W - text_width(sw)) / 2;
    print(sw, sw_x, 102, CLR_LIGHT_PEACH);

    // ── FONT_TINY (3x5) ──────────────────────────────────
    font(FONT_TINY);
    print("FONT_TINY  (3x5) — fits ~80 chars across 320px", 8, 116, CLR_LIME_GREEN);
    print("ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 !?.,;:+-*/=", 8, 124, CLR_WHITE);
    print("shadow on tiny font", 9, 133, CLR_BLACK);
    print("shadow on tiny font", 8, 132, CLR_LIGHT_GREY);
    print_outline("outline on tiny", 8, 141, CLR_YELLOW, CLR_DARK_GREEN);

    // pack a lot of info into tiny space
    print("hp:100/100  mp:80/80  xp:2400  gp:9999  lvl:12  str:18  int:14", 8, 152, CLR_MEDIUM_GREY);

    // ── reset + footer ────────────────────────────────────
    font(FONT_NORMAL);
    print(str("frame %d", frame()), 9, SCREEN_H - 9, CLR_BLACK);
    print(str("frame %d", frame()), 8, SCREEN_H - 10, CLR_DARK_GREY);
    print_right("font(FONT_NORMAL) resets", SCREEN_W - 4, SCREEN_H - 10, CLR_DARK_GREY);
}
