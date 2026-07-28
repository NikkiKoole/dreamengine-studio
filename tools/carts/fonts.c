/* de:meta
{
  "slug": "fonts",
  "title": "Fonts",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "Showcases all six built-in fonts (FONT_NORMAL 8×8, FONT_THIN 8×8, FONT_TIC 6×6, FONT_SMALL 4×6, FONT_TINY 3×5, FONT_COMIC 10×20) — each line printed in the font it names — plus drop shadow, print_outline, text_width, and the chained print return value."
}
de:meta */
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);

    // ── title ──────────────────────────────────────────────
    font(FONT_NORMAL);
    print_outline("FONT SHOWCASE", (SCREEN_W - text_width("FONT SHOWCASE")) / 2, 4, CLR_YELLOW, CLR_DARK_BROWN);

    // ── the family: each line printed IN the font it names ─
    // so you read the name and see the letterforms at once. Keep each line
    // inside that font's budget across 320px: 40 chars at 8px, 53 at 6px,
    // 64 at 5px, 80 at 4px, 32 at 10px.
    font(FONT_NORMAL);
    print("FONT_NORMAL 8x8  ABC abc 0123 !?", 8, 18, CLR_WHITE);

    font(FONT_THIN);
    print("FONT_THIN 8x8    ABC abc 0123 !?", 8, 29, CLR_LIGHT_GREY);

    font(FONT_TIC);
    print("FONT_TIC 6x6     ABC abc 0123 !?  small but bold", 8, 40, CLR_LIGHT_PEACH);

    font(FONT_SMALL);
    print("FONT_SMALL 4x6   ABC abc 0123 !?  ~64 chars across 320px", 8, 50, CLR_ORANGE);

    font(FONT_TINY);
    print("FONT_TINY 3x5    ABC abc 0123 !?  ~80 chars across 320px", 8, 60, CLR_LIME_GREEN);

    font(FONT_COMIC);
    print("FONT_COMIC 10x20", 8, 68, CLR_PINK);

    line(8, 96, SCREEN_W - 8, 96, CLR_DARK_PURPLE);

    // ── the print_* effects, in the default font ───────────
    font(FONT_NORMAL);
    print("drop shadow", 9, 105, CLR_BLACK);
    print("drop shadow", 8, 104, CLR_LIGHT_GREY);
    print_outline("outline text", 8, 116, CLR_WHITE, CLR_DARK_PURPLE);

    // chained print — each call returns the x after its last char
    int x = print("chain: ", 8, 128, CLR_LIGHT_GREY);
    x = print("red ", x, 128, CLR_RED);
    x = print("green ", x, 128, CLR_GREEN);
    print("blue", x, 128, CLR_BLUE);

    // text_width measures in the ACTIVE font — centering follows the switch
    font(FONT_TIC);
    const char *sw = "text_width measures in the active font";
    print(sw, (SCREEN_W - text_width(sw)) / 2, 143, CLR_LIGHT_PEACH);

    line(8, 156, SCREEN_W - 8, 156, CLR_DARK_PURPLE);

    // ── what the small fonts are FOR: density ──────────────
    font(FONT_TINY);
    print("hp:100/100  mp:80/80  xp:2400  gp:9999  lvl:12  str:18  int:14", 8, 164, CLR_MEDIUM_GREY);
    print_outline("outline works in every font", 8, 175, CLR_YELLOW, CLR_DARK_GREEN);

    // ── reset + footer ────────────────────────────────────
    font(FONT_NORMAL);
    print(str("frame %d", frame()), 9, SCREEN_H - 9, CLR_BLACK);
    print(str("frame %d", frame()), 8, SCREEN_H - 10, CLR_DARK_GREY);
    print_right("font(FONT_NORMAL) resets", SCREEN_W - 4, SCREEN_H - 10, CLR_DARK_GREY);
}
