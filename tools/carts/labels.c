/* de:meta
{
  "slug": "labels",
  "title": "labels",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "A labeling study for the control vocabulary — how to name a knob/button/fader when there's barely room: the five in-game fonts + scaling, text effects (shadow / outline / emboss / inverted chip), a handwritten 'boiling' vibe (FONT_COMIC + per-char jitter), rotated labels for tight vertical columns, and the classic Dymo labelprinter look in both colorways (white-on-dark tape, dark-on-white paper).",
    "detail": "The controls all use bare print() today; this cart works out the label LANGUAGE that pairs with them. Row 1 FONTS: the same word in TINY (3x5) / SMALL (4x6) / NORMAL (8x8) / THIN (CGA) / COMIC (handwriting), plus print_rot_scaled for 2x/3x. Row 2 EFFECTS: drop-shadow, 4-way outline, an inverted chip (light on a dark pill), and handwritten — FONT_COMIC with a slow per-character jitter so the text gently boils like ink. Row 3 ROTATED: labels turned 90 degrees (reading up + down) for a narrow fader column, and a 45-degree tag — via print_rot. Row 4 LABELPRINTER: embossed Dymo-tape labels (rounded tape + punched hole + a raised-letter emboss) in the two classic colorways. These become the label helpers the device-face carts pull from.",
    "controls": "None — a showcase. The handwritten line boils over time; everything else is static."
  }
}
de:meta */
#include "studio.h"

// LABELS — the labeling language for the control vocabulary. Fonts + scale,
// text effects, rotation for tight spaces, a handwritten boil, and the Dymo
// labelprinter look (both colorways). Reusable label_* helpers live here.

static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

// drop-shadow: the word offset (1,1) in a dark tone, then the ink on top
static void label_shadow(const char *s, int x, int y, int ink, int shadow) {
    print(s, x + 1, y + 1, shadow);
    print(s, x, y, ink);
}

// 4-way outline: the word in the edge colour nudged N/E/S/W, then the ink centred
static void label_outline(const char *s, int x, int y, int ink, int edge) {
    print(s, x - 1, y, edge); print(s, x + 1, y, edge);
    print(s, x, y - 1, edge); print(s, x, y + 1, edge);
    print(s, x, y, ink);
}

// an inverted chip: light text on a dark rounded pill (a tag)
static void label_chip(const char *s, int x, int y, int pill, int ink) {
    int w = text_width(s) + 8;
    rrectfill(x, y - 2, w, 11, 4, pill);
    print(s, x + 4, y + 1, ink);
}

// handwritten: FONT_COMIC drawn char-by-char with a slow per-char jitter so it
// gently "boils" like wet ink (the deterministic hand-drawn wobble).
static void label_hand(const char *s, int x, int y, int col, float t) {
    font(FONT_COMIC);
    int cx = x;
    for (int i = 0; s[i]; i++) {
        char ch[2] = { s[i], 0 };
        int jx = (int)(cos_deg(i * 130 + t * 90) * 0.9f);
        int jy = (int)(sin_deg(i * 80 + t * 120) * 1.4f);
        print(ch, cx + jx, y + jy, col);
        cx += text_width(ch);
    }
    font(FONT_NORMAL);
}

// a Dymo-style embossed tape label. paper = 1 → dark ink on white paper;
// paper = 0 → the classic raised WHITE letters on a dark/coloured tape.
// returns the tape width so you can lay several in a row.
static int label_tape(const char *s, int x, int y, int tapecol, int paper) {
    int w = text_width(s) + 16, h = 13;
    int face = paper ? CLR_WHITE : tapecol;
    int ink  = paper ? CLR_BROWNISH_BLACK : CLR_WHITE;
    int emb  = paper ? CLR_LIGHT_GREY : CLR_BLACK;   // emboss shadow tone
    rrectfill(x, y, w, h, 5, face);                  // the rounded tape
    circfill(x + 6, y + h / 2, 2, CLR_BROWNISH_BLACK);   // punched hole (bg shows through)
    int tx = x + 12, ty = y + 3;
    print(s, tx + 1, ty + 1, emb);                   // raised-letter emboss shadow
    print(s, tx, ty, ink);
    rrect(x, y, w, h, 5, paper ? CLR_LIGHT_GREY : CLR_BLACK);
    return w;
}

void init(void) {}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    float t = now();

    print("LABELS", 6, 3, CLR_WHITE);
    font(FONT_SMALL);
    print_right("naming controls when there's no room", 314, 5, CLR_MAUVE);

    // ── ROW 1 — the font family + scaling ──────────────────────────────────
    print("1 FONTS  tiny / small / normal / thin / comic  + scale", 6, 18, CLR_DARK_GREY);
    rrectfill(4, 26, 312, 52, 4, CLR_DARK_BROWN);
    {
        int y = 36;
        font(FONT_TINY);   print("CUTOFF", 12, y + 1, CLR_LIGHT_PEACH); print("tiny", 12, y + 8, CLR_DARK_GREY);
        font(FONT_SMALL);  print("CUTOFF", 60, y, CLR_LIGHT_PEACH);     print("small", 60, y + 8, CLR_DARK_GREY);
        font(FONT_NORMAL); print("CUTOFF", 118, y, CLR_LIGHT_PEACH);
        font(FONT_SMALL);  print("normal", 118, y + 10, CLR_DARK_GREY);
        font(FONT_THIN);   print("CUTOFF", 176, y, CLR_LIGHT_PEACH);
        font(FONT_SMALL);  print("thin", 176, y + 10, CLR_DARK_GREY);
        font(FONT_COMIC);  print("Cut", 236, y - 3, CLR_LIGHT_PEACH);
        font(FONT_SMALL);  print("comic", 236, y + 12, CLR_DARK_GREY);
        // scaling
        font(FONT_NORMAL);
        print_rot_scaled("MIX", 12, y + 20, 0, 2, CLR_YELLOW, 0);   // 2x
        print_rot_scaled("FX", 70, y + 20, 0, 3, CLR_YELLOW, 0);    // 3x
        font(FONT_SMALL); print("print_rot_scaled 2x / 3x", 130, y + 26, CLR_DARK_GREY);
    }

    // ── ROW 2 — text effects ───────────────────────────────────────────────
    font(FONT_SMALL);
    print("2 EFFECTS  shadow / outline / chip / handwritten (boils)", 6, 84, CLR_DARK_GREY);
    rrectfill(4, 92, 312, 46, 4, CLR_DARK_BROWN);
    {
        font(FONT_NORMAL);
        label_shadow("SHADOW", 12, 102, CLR_WHITE, CLR_BLACK);
        label_outline("OUTLINE", 110, 102, CLR_YELLOW, CLR_BROWNISH_BLACK);
        font(FONT_SMALL);
        label_chip("SOLO", 214, 101, CLR_RED, CLR_WHITE);
        label_chip("MUTE", 252, 101, CLR_DARK_GREEN, CLR_WHITE);
        label_hand("handwritten", 12, 124, CLR_LIGHT_PEACH, t);   // FONT_COMIC + boil
    }

    // ── ROW 3 — rotation for tight columns ─────────────────────────────────
    font(FONT_SMALL);
    print("3 ROTATED  90 for a narrow fader column / 45 tag  (print_rot)", 6, 146, CLR_DARK_GREY);
    rrectfill(4, 154, 312, 56, 4, CLR_DARK_BROWN);
    {
        // a mock vertical fader with a rotated label beside it
        rrectfill(30, 162, 5, 40, 2, CLR_BROWNISH_BLACK);
        rrectfill(24, 176, 16, 7, 2, CLR_MEDIUM_GREY);
        font(FONT_SMALL);
        print_rot("CUTOFF", 18, 202, 270, CLR_LIGHT_PEACH, 0);     // reads bottom-to-top
        print_rot("CUTOFF", 52, 162, 90, CLR_DARK_GREY, 0);        // reads top-to-bottom
        // a diagonal tag
        print_rot("NEW!", 210, 196, 315, CLR_YELLOW, 1);
        font(FONT_NORMAL);
        print_rot("RES", 120, 168, 0, CLR_LIGHT_GREY, 0);
        print_rot("RES", 120, 184, 90, CLR_MAUVE, 0);
        print_rot("RES", 160, 184, 180, CLR_MAUVE, 0);
        print_rot("RES", 160, 168, 270, CLR_MAUVE, 0);
    }

    // ── ROW 4 — the labelprinter (Dymo) look, both colorways ───────────────
    font(FONT_SMALL);
    print("4 LABELPRINTER  embossed tape - white-on-dark / dark-on-white", 6, 216, CLR_DARK_GREY);
    rrectfill(4, 224, 312, 40, 4, CLR_DARK_BROWN);
    {
        font(FONT_NORMAL);
        int x = 14;
        x += label_tape("VOLUME", x, 232, CLR_BROWNISH_BLACK, 0) + 6;   // classic white-on-black
        x += label_tape("BASS", x, 232, CLR_DARK_RED, 0) + 6;          // white-on-red tape
        x += label_tape("MIX", x, 232, 0, 1) + 6;                      // dark-on-white paper
        label_tape("FILTER", 14, 248, CLR_DARK_BLUE, 0);               // white-on-blue
        label_tape("TAPE", 90, 248, 0, 1);                            // dark-on-white
    }

    font(FONT_NORMAL);
}
