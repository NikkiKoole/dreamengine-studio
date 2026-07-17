/* de:meta
{
  "slug": "bossaface",
  "title": "bossa face (160x100)",
  "status": "active",
  "created": "2026-07-17",
  "kind": [
    "probe"
  ],
  "teaches": [],
  "collection": ["device-face"],
  "lineage": "The vibe mockup for the bossa RACK (docs/design/bossa-rack.md §2b skin) — a WARM bossa take on the candy device-face (candy-style.md / device-face-paradigm.md), the sibling of facemock/tinyface/acidcandy but teak+sand instead of purple+lime, and a sabia bird (the bird of bossa nova) as the mascot instead of the acid slime.",
  "description": "A VIBE MOCKUP (not functional) for the bossa chord-bloom rack's face — the candy device-face grammar with a BOSSA personality: a teak+sand shell instead of the acid purple, a little sabia BIRD on a wire (the bird of bossa) bopping to the clave on the green LCD, a playable AABA chord chart, warm teak knobs (VOICING/FEEL), the diatonic chord palette coloured by harmonic FUNCTION (tonic=gold, subdominant=palm-green, dominant=sunset-orange), and the chord-locked solo strip. Draw-only + now() life. Mockups-first for vibe work; designed FOR the small canvas."
}
de:meta */
#include "studio.h"
#include <string.h>

// BOSSAFACE — a static VIBE mockup for the bossa rack (docs/design/bossa-rack.md).
// Shared candy device-face GRAMMAR (chassis, LCD, chunky bevels, juice) with a
// BOSSA personality: teak + sand instead of purple + lime, and a sabia bird — the
// bird of bossa nova — as the mascot instead of the acid slime. The point is
// CHARM + the warm identity, not function. Tweak here, port the finish to the cart.

// ── little charm primitives ─────────────────────────────────────────────────
static void mnote(int x, int y, int col) {                     // a hummed note
    circfill(x, y + 5, 1, col);
    rectfill(x + 1, y, 1, 6, col);
    line(x + 2, y, x + 4, y - 1, col);
}
static void sparkle(int x, int y, int col, float ph) {
    int s = (int)(1.0f + 1.2f * sin_deg(ph));
    line(x - s, y, x + s, y, col); line(x, y - s, x, y + s, col);
}

// ── the SABIA BIRD — the soul (the bird of bossa nova, "Sabia"); bops, blinks ──
static void bird(int cx, int cy, float t) {
    int bop = (int)(sin_deg(t * 250) * 2);                      // sway to the clave
    cy += bop;
    line(cx - 15, cy + 8, cx + 15, cy + 8, CLR_MEDIUM_GREEN);   // the wire
    // tail + body + lit belly
    trifill(cx + 6, cy - 2, cx + 6, cy + 3, cx + 13, cy + 1, CLR_DARK_BROWN);
    ovalfill(cx, cy, 8, 6, CLR_ORANGE);
    ovalfill(cx + 1, cy + 2, 6, 4, CLR_LIGHT_PEACH);            // pale breast
    // wing
    blend(BLEND_AVG); ovalfill(cx + 1, cy - 1, 5, 3, CLR_DARK_BROWN); blend_reset();
    // head + beak + eye
    circfill(cx - 6, cy - 3, 4, CLR_DARK_ORANGE);
    trifill(cx - 10, cy - 4, cx - 10, cy - 2, cx - 14, cy - 3, CLR_YELLOW);  // beak
    int blink = ((int)(t * 1.4f)) % 4 == 0 && sin_deg(t * 300) > 0.6f;
    if (blink) line(cx - 7, cy - 4, cx - 4, cy - 4, CLR_BROWNISH_BLACK);
    else { pset(cx - 6, cy - 4, CLR_WHITE); pset(cx - 5, cy - 4, CLR_BROWNISH_BLACK); }
    // little legs on the wire
    line(cx - 1, cy + 6, cx - 1, cy + 8, CLR_DARK_ORANGE);
    line(cx + 2, cy + 6, cx + 2, cy + 8, CLR_DARK_ORANGE);
    mnote(cx + 12, cy - 11 - bop, CLR_LIGHT_YELLOW);            // it hums
}

// ── a warm TEAK knob (the candy chunky-knob, bossa-tinted) ───────────────────
static void teak_knob(int cx, int cy, int r, float v, const char *lbl) {
    blend(BLEND_AVG); circfill(cx + 1, cy + 2, r, CLR_BLACK); blend_reset();
    circfill(cx, cy, r, CLR_BROWN);
    ring(cx, cy, r - 2, r, 165, 285, CLR_LIGHT_PEACH);          // top-left glint
    ring(cx, cy, r - 2, r, -15, 105, CLR_DARK_BROWN);           // bottom-right shade
    circ(cx, cy, r, CLR_BROWNISH_BLACK);
    float ang = 150 + v * 240;
    line(cx + (int)dx(r * 0.3f, ang), cy + (int)dy(r * 0.3f, ang),
         cx + (int)dx(r - 2, ang),    cy + (int)dy(r - 2, ang), CLR_WHITE);
    font(FONT_TINY);
    print(lbl, cx - text_width(lbl) / 2, cy + r + 2, CLR_DARK_BROWN);
}

// ── a chord chip — coloured by harmonic FUNCTION (the palette's whole idea) ───
static void chip(int x, int y, int w, int h, int col, int hi, const char *name, int on) {
    blend(BLEND_AVG); rrectfill(x + 1, y + 2, w, h, 3, CLR_BLACK); blend_reset();
    int dy = on ? 1 : 0, hh = on ? h - 1 : h;
    rrectfill(x, y + dy, w, hh, 3, col);
    line(x + 2, y + dy + 1, x + w - 3, y + dy + 1, hi);         // top sheen
    rrect(x, y + dy, w, hh, 3, CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    print(name, x + (w - text_width(name)) / 2, y + dy + (hh - 5) / 2 + 1,
          on ? CLR_WHITE : CLR_BROWNISH_BLACK);
    if (on) { blend(BLEND_AVG); rrectfill(x, y + dy, w, hh, 3, CLR_WHITE); blend_reset(); }
}

void draw(void) {
    float t = now();
    // FUNCTION colours: tonic = gold, subdominant = palm-green, dominant = sunset
    const int C_TON = CLR_YELLOW, C_SUB = CLR_GREEN, C_DOM = CLR_ORANGE;
    const int H_TON = CLR_LIGHT_YELLOW, H_SUB = CLR_LIME_GREEN, H_DOM = CLR_LIGHT_PEACH;

    // ── the case: a teak shell around a warm sand chassis ──
    cls(CLR_BROWNISH_BLACK);
    rrectfill(0, 0, 160, 100, 8, CLR_DARK_BROWN);              // shell shadow edge
    rrectfill(1, 1, 157, 97, 8, CLR_BROWN);                    // teak shell
    rrectfill(5, 5, 150, 90, 6, CLR_LIGHT_PEACH);             // warm sand chassis
    blend(BLEND_AVG); line(9, 6, 150, 6, CLR_WHITE); blend_reset();  // top sheen

    // ── nav spine: play + tabs + readout ──
    trifill(9, 8, 9, 15, 15, 12, CLR_DARK_BROWN);              // play triangle
    font(FONT_TINY);
    static const char *TAB[4] = { "CHORD", "MEL", "MIX", "SONG" };
    int tx = 20;
    for (int i = 0; i < 4; i++) {
        int w = text_width(TAB[i]) + 4;
        if (i == 0) { rrectfill(tx, 8, w, 8, 2, CLR_ORANGE); print(TAB[i], tx + 2, 10, CLR_BROWNISH_BLACK); }
        else print(TAB[i], tx + 2, 10, CLR_DARK_BROWN);
        tx += w + 2;
    }
    print("key C  120", 112, 10, CLR_DARK_BROWN);

    // ── the LCD (context display): the bird + the playable AABA chart ──
    rrectfill(7, 19, 96, 52, 4, CLR_BROWNISH_BLACK);           // bezel
    rrectfill(9, 21, 92, 48, 3, CLR_DARK_GREEN);              // phosphor field
    blend(BLEND_AVG); for (int y = 23; y < 69; y += 2) line(9, y, 100, y, CLR_BROWNISH_BLACK); blend_reset();

    print("Ipanema  Amaj7", 12, 23, CLR_LIME_GREEN);          // song + current chord
    bird(52, 42, t);

    // the AABA chord chart — 8 A-section slots; a playhead cycles (the "chart you play")
    static const char *CH[8] = { "A", "F#", "Bm", "E", "A", "D", "Bm", "E" };
    int play = ((int)(t * 2)) % 8;
    for (int i = 0; i < 8; i++) {
        int sx = 12 + i * 11, sy = 58;
        int on = (i == play);
        rrectfill(sx, sy, 9, 9, 2, on ? CLR_LIME_GREEN : CLR_DARK_GREEN);
        rrect(sx, sy, 9, 9, 2, CLR_MEDIUM_GREEN);
        font(FONT_TINY);
        print(CH[i], sx + (9 - text_width(CH[i])) / 2, sy + 2, on ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREEN);
    }

    // ── two warm teak knobs (the few always-live controls) ──
    teak_knob(120, 30, 9, 0.4f, "VOICE");
    teak_knob(143, 30, 9, 0.65f, "FEEL");
    circfill(120, 49, 2, CLR_ORANGE); pset(119, 48, CLR_WHITE);  // amber LED
    font(FONT_TINY); print("cafe", 134, 47, CLR_DARK_BROWN);

    // ── the chord palette — coloured by FUNCTION (the diatonic 'always-bossa' row) ──
    // I  ii  iii  IV  V  vi  V7/x        tonic gold · subdom green · dom sunset
    struct { const char *n; int col, hi; } PAL[7] = {
        { "Amaj7", C_TON, H_TON }, { "Bm7", C_SUB, H_SUB }, { "C#m7", C_TON, H_TON },
        { "Dmaj7", C_SUB, H_SUB }, { "E7", C_DOM, H_DOM }, { "F#m7", C_TON, H_TON },
        { "C#7", C_DOM, H_DOM },
    };
    int pick = ((int)(t * 1.3f)) % 7;
    for (int i = 0; i < 7; i++)
        chip(8 + i * 21, 74, 19, 11, PAL[i].col, PAL[i].hi, PAL[i].n, i == pick);

    // ── the chord-locked solo strip (the jam surface) ──
    rrectfill(8, 88, 130, 7, 2, CLR_DARK_BROWN);
    for (int i = 0; i < 12; i++) {
        int sx = 12 + i * 11;
        int lit = (i == (int)(t * 6) % 12);
        line(sx, 89, sx, 94, lit ? CLR_LIGHT_YELLOW : CLR_ORANGE);
    }
    font(FONT_TINY); print("SOLO", 141, 90, CLR_DARK_BROWN);

    // a little tropical sparkle for life
    sparkle(150, 22, CLR_LIGHT_YELLOW, t * 200);

    font(FONT_NORMAL);
}
