#ifndef FXICONS_H
#define FXICONS_H
// ── fxicons.h — the shared VISUAL LANGUAGE for the engine effects ────────────────────────────
// One little icon + a body/accent colour per FX_* effect, so every cart's "pedals" look the same:
// the pedalboard's stompboxes, the epiano's mode toggles, any future effect-toy. Write the icon
// once, place it many ways (ADR-0006 cart-land library headers; the same discipline as ui.h).
//
// Keyed by the engine FX_* kinds (studio.h), so it's stable across carts. Include AFTER studio.h:
//     #include "studio.h"
//     #include "fxicons.h"
// All static, no state. Uses the studio.h draw API + CLR_*/FX_* constants.
#include <math.h>

// per-effect colours (the pedal's body + its lit accent) — the canonical effect palette
static int fx_body(int kind) {
    switch (kind) {
        case FX_CRUSH:   return CLR_DARK_BROWN;
        case FX_EQ:      return CLR_DARKER_BLUE;
        case FX_CHORUS:  return CLR_DARKER_PURPLE;
        case FX_PHASER:  return CLR_DARK_GREEN;
        case FX_FLANGER: return CLR_BLUE_GREEN;
        case FX_TAPE:    return CLR_DARK_RED;
        case FX_TREM:    return CLR_DARKER_GREY;
        case FX_WAH:     return CLR_DARK_PURPLE;
        case FX_REVERB:  return CLR_DARK_BLUE;
        case FX_FORMANT: return CLR_BROWN;
        case FX_PAN:     return CLR_DARK_GREY;
        case FX_FILTER:  return CLR_TRUE_BLUE;
        default:         return CLR_DARKER_GREY;
    }
}
static int fx_accent(int kind) {
    switch (kind) {
        case FX_CRUSH:   return CLR_DARK_ORANGE;
        case FX_EQ:      return CLR_BLUE;
        case FX_CHORUS:  return CLR_PINK;
        case FX_PHASER:  return CLR_LIME_GREEN;
        case FX_FLANGER: return CLR_MEDIUM_GREEN;
        case FX_TAPE:    return CLR_PEACH;
        case FX_TREM:    return CLR_LIGHT_YELLOW;
        case FX_WAH:     return CLR_MAUVE;
        case FX_REVERB:  return CLR_INDIGO;
        case FX_FORMANT: return CLR_LIGHT_PEACH;
        case FX_PAN:     return CLR_LIGHT_YELLOW;
        case FX_FILTER:  return CLR_BLUE;
        default:         return CLR_LIGHT_GREY;
    }
}
static const char *fx_name(int kind) {
    switch (kind) {
        case FX_CRUSH:   return "BITCRUSH";
        case FX_EQ:      return "EQ";
        case FX_CHORUS:  return "CHORUS";
        case FX_PHASER:  return "PHASER";
        case FX_FLANGER: return "FLANGER";
        case FX_TAPE:    return "TAPE";
        case FX_TREM:    return "TREMOLO";
        case FX_WAH:     return "WAH";
        case FX_REVERB:  return "REVERB";
        case FX_FORMANT: return "VOWEL";
        case FX_PAN:     return "AUTOPAN";
        case FX_FILTER:  return "FILTER";
        default:         return "FX";
    }
}

// draw the effect's icon centered at (cx,cy), in `col` over background `bg` (bg used for cutouts).
static void fx_icon(int kind, int cx, int cy, int col, int bg) {
    if (kind == FX_CRUSH) {                                  // 8-bit critter
        int ix = cx - 6, iy = cy - 5;
        rectfill(ix + 2, iy, 8, 2, col); rectfill(ix, iy + 2, 12, 4, col);
        rectfill(ix, iy + 6, 3, 3, col); rectfill(ix + 5, iy + 6, 2, 2, col); rectfill(ix + 9, iy + 6, 3, 3, col);
        rectfill(ix + 3, iy + 3, 2, 2, bg); rectfill(ix + 7, iy + 3, 2, 2, bg);
    } else if (kind == FX_EQ) {                              // equalizer spectrum
        static const int hh[5] = { 5, 11, 7, 13, 8 };
        for (int i = 0; i < 5; i++) rectfill(cx - 10 + i * 5, cy + 6 - hh[i], 3, hh[i], col);
    } else if (kind == FX_CHORUS) {                          // shimmer waves (two offset)
        for (int o = 0; o < 2; o++) {
            int base = cy + (o ? 3 : -3), px = cx - 15, py = base;
            for (int xx = cx - 15; xx <= cx + 15; xx += 2) { int wy = base + (int)(sinf((xx - cx) * 0.42f + o * 1.6f) * 3.0f); line(px, py, xx, wy, col); px = xx; py = wy; }
        }
    } else if (kind == FX_PHASER) {                          // one slow swirl + a notch dot
        int px = cx - 14, py = cy;
        for (int xx = cx - 14; xx <= cx + 14; xx++) { int wy = cy + (int)(sinf((xx - cx) * 0.26f) * 5.0f); line(px, py, xx, wy, col); px = xx; py = wy; }
        circfill(cx, cy, 1, bg);
    } else if (kind == FX_FLANGER) {                         // a jet
        int jx = cx + 7, jy = cy;
        trifill(jx - 14, jy - 2, jx - 14, jy + 2, jx, jy, col);
        trifill(jx - 10, jy, jx - 15, jy - 6, jx - 5, jy, col);
        trifill(jx - 10, jy, jx - 15, jy + 6, jx - 5, jy, col);
        for (int i = 0; i < 3; i++) line(cx - 16, jy - 3 + i * 3, cx - 9, jy - 3 + i * 3, col);
    } else if (kind == FX_TAPE) {                            // two tape reels
        circ(cx - 7, cy, 4, col); circfill(cx - 7, cy, 1, col);
        circ(cx + 7, cy, 4, col); circfill(cx + 7, cy, 1, col);
        line(cx - 7, cy - 5, cx + 7, cy - 5, col); line(cx - 7, cy + 5, cx + 7, cy + 5, col);
    } else if (kind == FX_TREM) {                            // carrier sine inside an AM bulge
        int px = cx - 15, py = cy;
        for (int xx = cx - 15; xx <= cx + 15; xx++) {
            float u = (float)(xx - (cx - 15)) / 30.0f; float env = sinf(u * 3.14159f);
            int wy = cy + (int)(sinf((xx - cx) * 1.1f) * env * 6.0f); line(px, py, xx, wy, col); px = xx; py = wy;
        }
    } else if (kind == FX_WAH) {                             // a resonant bandpass peak
        line(cx - 12, cy + 5, cx - 2, cy - 6, col); line(cx - 2, cy - 6, cx + 2, cy - 6, col);
        line(cx + 2, cy - 6, cx + 12, cy + 5, col); line(cx - 13, cy + 5, cx + 13, cy + 5, col);
    } else if (kind == FX_FORMANT) {                         // an open vowel mouth (the talkbox)
        int ww = 11, oh = 6;
        for (int dx = -ww; dx <= ww; dx++) {
            float t = (float)dx / (float)ww;
            int h = (int)(oh * (1.0f - t * t)); if (h < 1) h = 1;
            line(cx + dx, cy - h, cx + dx, cy + h, col);
        }
        circ(cx, cy, 2, bg);                                 // a darker opening so it reads as a mouth
    } else if (kind == FX_PAN) {                             // a double-headed L↔R sweep arrow
        line(cx - 13, cy, cx + 13, cy, col);                 // the pan axis
        line(cx - 13, cy, cx - 8, cy - 4, col); line(cx - 13, cy, cx - 8, cy + 4, col);   // ◄ left head
        line(cx + 13, cy, cx + 8, cy - 4, col); line(cx + 13, cy, cx + 8, cy + 4, col);   // ► right head
        circfill(cx, cy, 2, col);                            // the sound, sweeping along it
    } else if (kind == FX_FILTER) {                          // a lowpass curve with a resonant corner bump
        line(cx - 13, cy - 3, cx - 3, cy - 3, col);          // flat passband
        line(cx - 3,  cy - 3, cx,     cy - 8, col);          // up to the resonant peak
        line(cx,      cy - 8, cx + 5, cy + 6, col);          // steep rolloff past the corner
        line(cx + 5,  cy + 6, cx + 13, cy + 6, col);         // settled stopband
    } else {                                                 // REVERB — expanding rings (the bloom)
        for (int i = 1; i <= 3; i++) circ(cx, cy, i * 3, col);
        pset(cx, cy, col);
    }
}

#endif // FXICONS_H
