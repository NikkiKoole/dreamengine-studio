/* de:meta
{
  "slug": "indicators",
  "title": "indicators",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "The SHOW family from the control vocabulary — output-only indicators in beveled lo-fi: status LEDs (off/dim/on/blink + colours + a chase), stereo VU bargraphs with peak-hold, 7-segment numeric readouts, and an LCD scope with phosphor glow. All driven by one animated signal so they're alive.",
    "detail": "The fourth control-vocabulary study cart. These controls take no input — they DISPLAY — so every one is driven by a shared bouncing signal (a couple of LFOs) to show how it reads a value. Row 1 LEDS: a single lamp in each light state (off/dim/on/blink), the colour vocabulary (red/amber/green/blue/bi-colour), and a chasing activity row. Row 2 METERS: a stereo VU pair (segmented green->amber->red) with decaying PEAK-HOLD caps, plus a horizontal LED bar. Row 3 READOUTS + SCREEN: 7-segment displays (a fixed BPM, a live counter) on a recessed dark inset, beside a bezelled LCD SCOPE — dark phosphor field, scanlines, a bright green trace with a dim glow (the zone-3 'soul' surface). Recessed housings + top-left light, per the pinned lighting rule.",
    "controls": "None — indicators are output. Everything animates from the internal signal; watch the meters bounce, the peak caps hang and fall, the counter tick, and the scope scroll."
  }
}
de:meta */
#include "studio.h"

// INDICATORS — the SHOW family (control-vocabulary.md §3 ▦): output-only
// readouts. LEDs, VU bargraphs, 7-segment numbers, an LCD scope. They take no
// input, so a shared animated signal drives them all — the point of the study
// is how each one READS a value, in beveled lo-fi with the top-left light rule.

static float absf(float v) { return v < 0 ? -v : v; }

// ── a status LED: a recessed lamp that glows when lit (top-left glint) ───────
static void led(int cx, int cy, int r, int on, int col) {
    circfill(cx, cy, r + 1, CLR_BROWNISH_BLACK);          // recessed bezel hole
    if (on) {                                             // soft colour halo
        blend(BLEND_AVG);
        circfill(cx, cy, r + 2, col);
        blend_reset();
    }
    circfill(cx, cy, r, on ? col : CLR_DARKER_GREY);      // lamp
    arc(cx, cy, r, 0, 360, CLR_BLACK);                    // rim
    if (on) pset(cx - (r > 3 ? 1 : 0), cy - (r > 3 ? 1 : 0), CLR_WHITE);  // top-left glint
}

// ── a segmented VU meter: level fills bottom->top, green->amber->red, + peak ─
static void vu_meter(int x, int y, int w, int h, float level, float peak) {
    int n = 12, gap = 1, sh = (h - (n - 1) * gap) / n;    // segment height
    rrectfill(x - 2, y - 2, w + 4, h + 4, 2, CLR_BROWNISH_BLACK);   // recessed housing
    for (int i = 0; i < n; i++) {
        float frac = (i + 1) / (float)n;
        int sy = y + h - (i + 1) * sh - i * gap;
        int col = frac > 0.85f ? CLR_RED : frac > 0.6f ? CLR_YELLOW : CLR_GREEN;
        int lit = level >= (i / (float)n);
        int isPeak = (int)(peak * n) == i;
        if (lit || isPeak) {
            rectfill(x, sy, w, sh, isPeak && !lit ? CLR_WHITE : col);
            line(x, sy, x + w - 1, sy, CLR_WHITE);        // top-left sheen on the lit segment
        } else {
            rectfill(x, sy, w, sh, CLR_DARKER_GREY);      // unlit
            blend(BLEND_AVG); line(x, sy + sh - 1, x + w - 1, sy + sh - 1, CLR_BLACK); blend_reset();
        }
    }
}

// ── a 7-segment digit ────────────────────────────────────────────────────
// segment bits: a=top b=TR c=BR d=bottom e=BL f=TL g=mid
static const unsigned char SEG[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
static void seg7(int x, int y, int w, int h, int digit, int on, int off) {
    int th = 2, m = SEG[digit % 10], hh = h / 2;
    int c;
    c = (m & 0x01) ? on : off; rectfill(x + th, y, w - 2 * th, th, c);                 // a
    c = (m & 0x02) ? on : off; rectfill(x + w - th, y + th, th, hh - th, c);           // b
    c = (m & 0x04) ? on : off; rectfill(x + w - th, y + hh, th, hh - th, c);           // c
    c = (m & 0x08) ? on : off; rectfill(x + th, y + h - th, w - 2 * th, th, c);        // d
    c = (m & 0x10) ? on : off; rectfill(x, y + hh, th, hh - th, c);                    // e
    c = (m & 0x20) ? on : off; rectfill(x, y + th, th, hh - th, c);                    // f
    c = (m & 0x40) ? on : off; rectfill(x + th, y + hh - th / 2, w - 2 * th, th, c);   // g
}
// a right-aligned 7-seg number on its own recessed inset
static void seg7_num(int x, int y, int dw, int dh, int value, int digits, int on, int off) {
    for (int i = 0; i < digits; i++) {
        int d = value; for (int k = 0; k < i; k++) d /= 10;
        seg7(x + (digits - 1 - i) * (dw + 4), y, dw, dh, d % 10, on, off);
    }
}

// ── the LCD scope: bezel + dark phosphor + scanlines + a bright green trace ─
static void scope(int x, int y, int w, int h, float t) {
    rrectfill(x - 3, y - 3, w + 6, h + 6, 3, CLR_DARK_GREY);        // bezel
    line(x - 3, y - 3, x + w + 2, y - 3, CLR_LIGHT_GREY);           // bezel top-left sheen
    line(x - 3, y - 3, x - 3, y + h + 2, CLR_LIGHT_GREY);
    line(x - 3, y + h + 2, x + w + 2, y + h + 2, CLR_BLACK);        // bezel bottom-right shade
    line(x + w + 2, y - 3, x + w + 2, y + h + 2, CLR_BLACK);
    rectfill(x, y, w, h, CLR_BROWNISH_BLACK);                       // phosphor field (near-black)
    blend(BLEND_AVG);
    line(x, y + h / 2, x + w - 1, y + h / 2, CLR_DARK_GREEN);       // graticule centre lines
    line(x + w / 2, y, x + w / 2, y + h - 1, CLR_DARK_GREEN);
    for (int yy = y + 1; yy < y + h; yy += 2) line(x, yy, x + w - 1, yy, CLR_BLACK);  // scanlines
    blend_reset();
    int prev = y + h / 2;
    for (int i = 0; i < w; i++) {                                   // the trace
        float ph = (i / (float)w) * 720 + t * 220;
        float s = sin_deg(ph) * 0.7f + sin_deg(ph * 2 + 40) * 0.3f;
        int sy = y + h / 2 - (int)(s * (h / 2 - 2));
        blend(BLEND_AVG); pset(x + i, sy + 1, CLR_GREEN); blend_reset();   // glow
        if (i) line(x + i - 1, prev, x + i, sy, CLR_LIME_GREEN);
        prev = sy;
    }
}

// ── state (peak-hold decays over time) ───────────────────────────────────
static float peakL = 0, peakR = 0;

void init(void) { bpm(120); }

void update(void) {
#ifdef DE_TRACE
    watch("peakL", "%.2f", peakL);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    print("INDICATORS", 6, 4, CLR_WHITE);
    font(FONT_SMALL);
    print_right("the SHOW family - output only", 314, 6, CLR_MAUVE);

    float t = now();
    // one shared signal driving everything
    float lvlL = absf(sin_deg(t * 70)) * (0.55f + 0.45f * absf(sin_deg(t * 23)));
    float lvlR = absf(sin_deg(t * 61 + 30)) * (0.55f + 0.45f * absf(sin_deg(t * 19)));
    if (lvlL > peakL) peakL = lvlL; else peakL -= 0.006f; if (peakL < 0) peakL = 0;
    if (lvlR > peakR) peakR = lvlR; else peakR -= 0.006f; if (peakR < 0) peakR = 0;

    // ── ROW 1 — LEDs: states, colours, a chase ─────────────────────────────
    print("1 LEDS  off / dim / on / blink   -   colours   -   activity chase", 6, 20, CLR_DARK_GREY);
    rrectfill(4, 28, 312, 46, 4, CLR_DARK_BROWN);
    int blink = ((int)(t * 3)) & 1;
    {
        // the four states (all red)
        const char *sn[4] = { "OFF", "DIM", "ON", "BLINK" };
        int son[4] = { 0, 0, 1, blink };
        int scol[4] = { CLR_RED, CLR_DARK_RED, CLR_RED, CLR_RED };
        for (int i = 0; i < 4; i++) {
            led(24 + i * 30, 44, 5, son[i] || (i == 1), i == 1 ? CLR_DARK_RED : scol[i]);
            print(sn[i], 24 + i * 30 - text_width(sn[i]) / 2, 56, CLR_DARK_GREY);
        }
        // the colour vocabulary
        int cc[5] = { CLR_RED, CLR_ORANGE, CLR_GREEN, CLR_TRUE_BLUE, blink ? CLR_RED : CLR_GREEN };
        const char *cn[5] = { "R", "A", "G", "B", "BI" };
        for (int i = 0; i < 5; i++) {
            led(160 + i * 16, 44, 5, 1, cc[i]);
            print(cn[i], 160 + i * 16 - 1, 56, CLR_DARK_GREY);
        }
        // activity chase — one lit LED runs along the row
        int run = ((int)(t * 8)) % 8;
        for (int i = 0; i < 8; i++)
            led(250 + i * 8, 44, 3, i == run, CLR_LIME_GREEN);
        print("chase", 250 + 12, 56, CLR_DARK_GREY);
    }

    // ── ROW 2 — meters: stereo VU + peak-hold, a horizontal bar ────────────
    print("2 METERS  stereo VU with peak-hold   -   horizontal LED bar", 6, 82, CLR_DARK_GREY);
    rrectfill(4, 90, 312, 82, 4, CLR_DARK_BROWN);
    {
        vu_meter(24, 100, 14, 62, lvlL, peakL);
        vu_meter(44, 100, 14, 62, lvlR, peakR);
        print("L", 27, 164, CLR_LIGHT_GREY); print("R", 47, 164, CLR_LIGHT_GREY);

        // horizontal LED bar (16 cells)
        int bx = 90, by = 120, bw = 210, n = 16, cw = bw / n;
        rrectfill(bx - 2, by - 2, bw + 4, 16, 2, CLR_BROWNISH_BLACK);
        float bl = 0.5f + 0.5f * sin_deg(t * 45);
        for (int i = 0; i < n; i++) {
            float frac = (i + 1) / (float)n;
            int col = frac > 0.85f ? CLR_RED : frac > 0.6f ? CLR_YELLOW : CLR_GREEN;
            int lit = bl >= i / (float)n;
            rectfill(bx + i * cw, by, cw - 1, 12, lit ? col : CLR_DARKER_GREY);
            if (lit) line(bx + i * cw, by, bx + i * cw + cw - 2, by, CLR_WHITE);
        }
        print("SIGNAL", bx, by + 18, CLR_DARK_GREY);
    }

    // ── ROW 3 — readouts + the LCD scope ───────────────────────────────────
    print("3 READOUTS + SCREEN  7-segment displays   -   LCD scope (phosphor)", 6, 180, CLR_DARK_GREY);
    rrectfill(4, 188, 312, 78, 4, CLR_DARK_BROWN);
    {
        // 7-seg on a recessed dark inset
        rrectfill(14, 198, 96, 58, 3, CLR_BROWNISH_BLACK);
        // off-segments are near-invisible (a hair above the inset) so digits stay legible —
        // DARK_RED as a ghost was too close to lit RED and every digit read as "8".
        seg7_num(22, 206, 14, 26, 120, 3, CLR_RED, CLR_DARK_BROWN);         // BPM (fixed)
        print("BPM", 22, 236, CLR_DARK_RED);
        int counter = ((int)(t * 4)) % 1000;
        seg7_num(70, 206, 10, 20, counter, 3, CLR_LIME_GREEN, CLR_DARK_BROWN);   // live counter
        print("STEP", 70, 236, CLR_DARK_GREEN);

        // the LCD scope
        scope(130, 200, 172, 52, t);
    }

    font(FONT_NORMAL);
}
