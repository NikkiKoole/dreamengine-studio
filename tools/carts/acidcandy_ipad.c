/* de:meta
{
  "title": "Tiny Acid Jam — iPad all-in-one (mockup)",
  "slug": "acidcandy_ipad",
  "kind": ["tech-demo"],
  "teaches": [],
  "status": "wip",
  "created": "2026-07-22",
  "lineage": "acidcandy",
  "description": {
    "summary": "A ground-up iPad ROOMY layout for acidcandy: all five machines on screen at once, no device-face nav. Two narrow 303 knob-strips (303a mirrored so its dials clear the left border) + a narrow MASTER knob-strip bracket a big shared SCREEN; the 808 (16 voices) and 909 (11) sit as pad-bank strips stacked at the bottom (808 under 909). Draw-only VIBES mockup, no interaction — the design lock before wiring.",
    "detail": "The model this session settled on: (1) LAYOUT — transport bar; middle band [303a|303b|SCREEN|MST], columns = header(tap-to-focus)+5 acid knobs+FX trio(DRV/SND/VRB, same size as the drum FX)+CL/DF+KEY; drums stacked bottom. (2) STICKY FOCUS — tapping a machine's nameplate puts it on the shared screen (white ring on the plate, screen border+tag recolour, soft-keys swap); focus only routes the DEEP editor, play stays live for ALL machines. (3) THE SCREEN per focus — 303 = tb303 NOTE GRID (scale-degree rows x 16 steps; colour=accent, SHAPE=^v octave / line=slide / stretched cell=tie); drum = 2D VOICE GRID (voices x 16 steps); MST = PCF/CRU/GAT automation lanes. (4) DRUMS have no column, so the bottom strip = voice PADS (a cell is an instrument, not a step; lit=in pattern, white pulse=firing) + the 3 FX knobs; both machines use the 808 pad width so the 909's spare room holds ONE SHARED 4-knob context panel (TUN/DEC/ATK/SNP) that follows the last-picked voice on EITHER machine and rings in that machine's colour (blue=808 voice, yellow=909). Colours match the rack: 303a pink, 303b orange, 808 blue, 909 amber, MST green.",
    "controls": "None — static mockup; the demo auto-cycles sticky FOCUS through the 5 machines and alternates the selected drum voice (808 blue / 909 yellow) so every screen + the context-panel colour flip are visible. OPEN before wiring: drum per-voice VCE home (screen vs the shared panel), 303 grid pitch-gutter labels, 808 grid cell size (16 rows ~5px). Sibling prior art: roomyface (device-face ROOMY tiles), acidwire (interactive wireframe). Next = lock this + wire the real cart (data shapes are concrete: per-step degree/acc/slide/oct/tie for 303s, voice x step for drums)."
  }
}
de:meta */

// Tiny Acid Jam — iPad ROOMY layout mockup.
// The phone rack shows ONE machine at a time (device-face nav). On the iPad we have
// the pixels to show them ALL: 808/909 as horizontal strips top & bottom, the two 303s
// as tall side columns (their full knob+settings stack always visible), master in the
// transport bar, and the big shared screen in the freed middle space.
//
// This is a DRAW-ONLY mockup (mockups-first-for-vibes) — no widgets, no sound.
// A slow playhead sweeps so it feels alive.

#include "studio.h"
#include <math.h>
#include <string.h>

// ── machine palette (matches tools/carts/acidcandy.c rack[]) ─────────────────
#define A_HI  CLR_PINK        // 303a bass
#define A_LO  CLR_DARK_PURPLE
#define B_HI  CLR_ORANGE      // 303b lead
#define B_LO  CLR_DARK_ORANGE
#define D8_HI CLR_TRUE_BLUE   // 808
#define D8_LO CLR_DARK_BLUE
#define D9_HI CLR_YELLOW      // 909
#define D9_LO CLR_DARK_BROWN
#define M_HI  CLR_GREEN       // MST
#define M_LO  CLR_DARK_GREEN

#define CHASSIS   CLR_BROWNISH_BLACK
#define PANEL     CLR_DARKER_GREY
#define INK       CLR_LIGHT_GREY
#define DIM       CLR_MEDIUM_GREY
#define LCD_BG    CLR_DARK_GREEN
#define LCD_INK   CLR_LIME_GREEN

static int playstep = 0;   // animated playhead (0..15)
static int focus = 0;      // which machine owns the shared screen: 0=303a 1=303b 2=808 3=909 4=MST
static int sel_mach = 2;   // the last-picked DRUM machine (2=808 / 3=909) — the shared context knobs follow it
static int sel_voice = 0;  // and which voice within it

// the STICKY-FOCUS ring: a bright white outline on the focused machine's nameplate
static void focus_ring(int x, int y, int w, int h) {
    rect(x - 1, y - 1, w + 2, h + 2, CLR_WHITE);
    rect(x - 2, y - 2, w + 4, h + 4, CLR_BLACK);
}

// just the dial graphics: filled body + a tick pointing at `v` (0..1)
static void dial_only(int cx, int cy, int r, float v, int accent) {
    circfill(cx, cy, r, CLR_BLACK);
    circ(cx, cy, r, accent);
    float a = (-3.14159f * 1.25f) + v * (3.14159f * 1.5f);   // 7-o'clock..5-o'clock sweep
    int ex = cx + (int)(cosf(a) * (r - 1));
    int ey = cy + (int)(sinf(a) * (r - 1));
    line(cx, cy, ex, ey, CLR_WHITE);
}

// small labelled dial: dial + a label/value to its RIGHT
static void dial(int cx, int cy, int r, float v, int accent, const char *lab, const char *val) {
    dial_only(cx, cy, r, v, accent);
    font(FONT_TINY);
    print(lab, cx + r + 3, cy - 5, INK);
    if (val) print(val, cx + r + 3, cy + 1, accent);
    font(FONT_NORMAL);
}

// mirrored labelled dial: label/value to the LEFT, dial to the RIGHT (for the
// left-most column, so the knob sits off the window border, not jammed against it)
static void dial_l(int cx, int cy, int r, float v, int accent, const char *lab, const char *val) {
    dial_only(cx, cy, r, v, accent);
    font(FONT_TINY);
    int lw = 0; for (const char *p = lab; *p; p++) lw += 4;   // tiny font ~4px/char
    print(lab, cx - r - 3 - lw, cy - 5, INK);
    if (val) { int vw = 0; for (const char *p = val; *p; p++) vw += 4;
               print(val, cx - r - 3 - vw, cy + 1, accent); }
    font(FONT_NORMAL);
}

// a machine tag chip — name centred, mute LED tucked in the TOP-RIGHT corner
// (out of the text row, so a wide label like "808" fits without the LED clipping it)
static void chip(int x, int y, int w, int h, int hi, int lo, const char *name, int led_on) {
    rectfill(x, y, w, h, lo);
    rect(x, y, w, h, hi);
    print(name, x + 3, y + (h - 5) / 2, hi);
    circfill(x + w - 4, y + 4, 2, led_on ? hi : CLR_DARK_GREY);
}

// drum voice rosters (2-char), in tr808.h / tr909.h order — 808 = 16, 909 = 11
static const char *V808[16] = { "KI","SN","LT","MT","HT","LC","MC","HC",
                                "RS","CV","CP","MA","CB","CY","OH","CH" };
static const char *V909[11] = { "KI","SN","LT","MT","HT","RS","CP","CH","OH","CC","RC" };

// ONE demo beat, shared by the bottom pads + the screen grid so they always agree
static int drum_hit(int v, int i) {
    if (v == 0) return (i % 4) == 0;           // kick: four on the floor
    if (v == 1) return (i == 4 || i == 12);    // snare: backbeat
    return ((i * 5 + v * 7) % 13) < 2;         // everything else: scattered fills
}
static int drum_acc(int v, int i) { return v <= 1 && (i % 8) == 0; }
static int drum_active(int v) {                // any hit in the bar? (for the pad glow)
    for (int i = 0; i < 16; i++) if (drum_hit(v, i)) return 1; return 0;
}

// one horizontal drum strip: a PAD per voice + 3 fx knobs on the right. Both machines
// use the SAME pad width (sized for 16), so the 909's fewer voices leave a gap that holds
// a SHARED 4-knob context panel — it edits whichever voice was last picked, on EITHER
// machine, and takes that machine's colour (blue = an 808 voice, yellow = a 909 voice).
static void drum_strip(int x, int y, int w, int h, int hi, int lo, const char *name,
                       int nv, const char **vn, int mach, int sel_mach, int sel_voice,
                       int sel_hi, const char *sel_name, int focused) {
    rectfill(x, y, w, h, PANEL);
    rect(x, y, w, h, lo);

    // left tag block — tap it to focus this machine onto the screen (wider = label fits)
    chip(x + 2, y + 2, 28, h - 4, hi, lo, name, 1);
    if (focused) focus_ring(x + 2, y + 2, 28, h - 4);

    // fx knobs on the right
    int kw = 3 * 20 + 4;
    int kx = x + w - kw;
    dial(kx + 6,  y + 11, 5, 0.55f, hi, "DST", NULL);
    dial(kx + 26, y + 11, 5, 0.35f, hi, "SND", NULL);
    dial(kx + 46, y + 11, 5, 0.20f, hi, "VRB", NULL);

    // the VOICE PADS: one per drum voice (a cell = an instrument, not a step). tap to
    // finger-drum / select; lit = in the pattern, dimmed = muted; it pulses when it fires.
    int px0 = x + 33, px1 = kx - 4;
    int step = (px1 - px0) / 16;                  // fixed width (16-voice reference)
    int pw = step - 1, ph = h - 8, py = y + 4;
    for (int v = 0; v < nv; v++) {
        int cx = px0 + v * step;
        int active = drum_active(v);
        int firing = drum_hit(v, playstep);          // pulses on its trigger
        int col = firing ? CLR_WHITE : (active ? hi : CLR_DARK_GREY);
        rectfill(cx, py, pw, ph, active ? lo : CLR_BLACK);
        rect(cx, py, pw, ph, col);
        if (mach == sel_mach && v == sel_voice)      // the selected voice (white = always visible)
            rect(cx - 1, py - 1, pw + 2, ph + 2, CLR_WHITE);
        font(FONT_TINY);
        print(vn[v], cx + (pw - 8) / 2, py + (ph - 5) / 2, col);
        font(FONT_NORMAL);
    }

    // the SHARED context panel lives in the freed space (the strip with nv < 16 = 909)
    if (nv < 16) {
        int fx0 = px0 + nv * step + 5;
        int fw = px1 - fx0;
        rect(fx0, y + 1, fw - 2, h - 2, sel_hi);                 // ring in the selected machine's colour
        font(FONT_TINY); print(sel_name, fx0 + 2, y + 2, sel_hi); font(FONT_NORMAL);
        const char *kn[4] = { "TUN", "DEC", "ATK", "SNP" };
        float kv[4] = { 0.5f, 0.6f, 0.35f, 0.7f };
        int kstep = (fw - 4) / 4;
        for (int i = 0; i < 4; i++) {
            int cx = fx0 + 2 + kstep / 2 + i * kstep;
            font(FONT_TINY); print(kn[i], cx - 6, y + 8, DIM); font(FONT_NORMAL);
            dial_only(cx, y + 17, 4, kv[i], sel_hi);
        }
    }
}

// a narrow 303 column: just a strip of knobs + the CL/DF meta toggle + a KEY label.
// (the note-entry surface lives on the big screen now — this is only the sound.)
static void col303(int x, int y, int w, int h, int hi, int lo,
                   const char *tag, const char *role, const char *key, int muted,
                   int focused, int mirror) {
    (void)role;
    rectfill(x, y, w, h, PANEL);
    rect(x, y, w, h, lo);

    // header: tag + mute LED (tap to focus this 303 onto the shared screen)
    rectfill(x + 2, y + 2, w - 4, 12, lo);
    rect(x + 2, y + 2, w - 4, 12, hi);
    print(tag, x + 4, y + 5, hi);
    circfill(x + w - 6, y + 8, 2, muted ? CLR_DARK_GREY : hi);
    if (focused) focus_ring(x + 2, y + 2, w - 4, 12);

    // 5 acid knobs stacked (spacing scales with height). mirror = knob on the RIGHT,
    // label on the LEFT — so the left-most column's dials clear the window border.
    const char *names[5] = { "CUT", "RES", "ENV", "DEC", "ACC" };
    const char *vals[5]  = { "72",  "48",  "60",  "35",  "80" };
    float vv[5] = { 0.72f, 0.48f, 0.60f, 0.35f, 0.80f };
    int fy = y + h - 46;                       // start of the FX + meta + key section
    int ky = y + 22, kgap = (fy - ky) / 5;
    for (int i = 0; i < 5; i++) {
        if (mirror) dial_l(x + w - 12, ky + i * kgap, 5, vv[i], hi, names[i], vals[i]);
        else        dial  (x + 11,     ky + i * kgap, 5, vv[i], hi, names[i], vals[i]);
    }

    // FX sub-section: DRV / SND / VRB as a horizontal 3-knob row — a different
    // pattern from the vertical acid stack, so it reads as "the FX", not more core.
    line(x + 3, fy, x + w - 3, fy, lo);
    font(FONT_TINY); print("FX", x + 4, fy + 1, DIM); font(FONT_NORMAL);
    const char *fxn[3] = { "DRV", "SND", "VRB" };
    float fxv[3] = { 0.30f, 0.25f, 0.40f };
    for (int i = 0; i < 3; i++) {
        int cx = x + 8 + i * 15;
        dial_only(cx, fy + 12, 5, fxv[i], hi);   // r5 = same size as the drum FX knobs
        font(FONT_TINY); print(fxn[i], cx - 6, fy + 19, DIM); font(FONT_NORMAL);
    }

    // CL / DF voicing meta toggle + KEY
    int sy = fy + 24;
    int sw = (w - 6) / 2;
    rectfill(x + 3, sy, sw, 9, hi);           // CL selected
    rect(x + 3, sy, sw, 9, hi);
    rect(x + 3 + sw, sy, sw, 9, lo);
    font(FONT_TINY);
    print("CL", x + 6, sy + 2, CLR_BLACK);
    print("DF", x + 6 + sw, sy + 2, hi);
    print("KEY", x + 4, sy + 11, DIM);
    print(key, x + 22, sy + 11, hi);
    font(FONT_NORMAL);
}

static int FHI[5] = { A_HI, B_HI, D8_HI, D9_HI, M_HI };
static int FLO[5] = { A_LO, B_LO, D8_LO, D9_LO, M_LO };
static const char *FNAME[5] = { "303a", "303b", "808", "909", "MST" };
static const char *FROLE[5] = { "NOTE GRID", "NOTE GRID", "16 VOICES", "11 VOICES", "MIX+FX" };

// the soft-key row at the bottom of the screen (labels change per focused machine)
static void softkeys(int x, int w, int yrow, const char **keys, int n, int accent) {
    int kw = (w - 10) / n;
    for (int i = 0; i < n; i++) {
        int cx = x + 5 + i * kw;
        int sel = (i == 0);
        rect(cx, yrow, kw - 2, 9, LCD_INK);
        if (sel) rectfill(cx, yrow, kw - 2, 9, accent);
        font(FONT_TINY);
        print(keys[i], cx + 3, yrow + 2, sel ? CLR_WHITE : LCD_INK);
        font(FONT_NORMAL);
    }
}

// screen content: the 303 NOTE GRID (rows = scale degrees, cols = 16 steps) — the
// tb303-style draggable grid. colour carries loudness (accent), SHAPE carries what
// happens between notes: ^/v = octave up/down, a line = slide, a stretched cell = tie.
static void screen303(int x, int cy0, int w, int ch, int focus) {
    int hi = (focus == 0) ? A_HI : B_HI;
    // per step: degree 0..7 (-1 = rest), accent, slide-to-next, octave(+1/0/-1), tie
    static const int degA[16] = { 0,0,-1,2, 0,1,-1,3, 0,0,-1,4, 2,0,1,-1 };
    static const int accA[16] = { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,1,0 };
    static const int sldA[16] = { 0,0,0,0, 0,1,0,0, 0,0,0,1, 0,0,0,0 };
    static const int octA[16] = { 0,0,0,0, 0,0,0,1, 0,0,0,0,-1,0,0,0 };
    static const int tieA[16] = { 0,1,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0 };
    static const int degB[16] = { 4,5,3,6, 5,7,-1,2, 4,6,5,7, 3,5,7,4 };
    static const int accB[16] = { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0 };
    static const int sldB[16] = { 0,1,0,0, 0,1,0,0, 0,1,0,0, 0,1,0,0 };
    static const int octB[16] = { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,1 };
    static const int tieB[16] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
    const int *deg = (focus == 0) ? degA : degB;
    const int *acc = (focus == 0) ? accA : accB;
    const int *sld = (focus == 0) ? sldA : sldB;
    const int *oct = (focus == 0) ? octA : octB;
    const int *tie = (focus == 0) ? tieA : tieB;
    const int NR = 8;                     // scale-degree rows

    int gut = 8, gx = x + gut, gw = w - gut - 2, step = gw / 16;
    int gy = cy0, gh = ch - 6, rh = gh / NR;   // reserve 6px for the legend

    // faint grid + playhead column
    for (int i = 0; i < 16; i++) {
        int cx = gx + i * step;
        if (i % 4 == 0) line(cx, gy, cx, gy + rh * NR, CLR_DARK_GREEN);
        if (i == playstep) rectfill(cx, gy, step - 1, rh * NR, CLR_DARK_GREEN);
    }
    for (int r = 0; r <= NR; r++) line(gx, gy + r * rh, gx + gw, gy + r * rh, CLR_BROWNISH_BLACK);

    // the notes
    for (int i = 0; i < 16; i++) {
        if (deg[i] < 0) continue;
        int row = NR - 1 - deg[i];
        int ry = gy + row * rh;
        int cx = gx + i * step;
        int cw = step - 1 + (tie[i] ? step : 0);      // TIE = a stretched cell (shape)
        int c = acc[i] ? CLR_WHITE : hi;              // ACCENT = colour
        rectfill(cx + 1, ry + 1, cw - 1, rh - 1, c);
        if (oct[i] > 0) {                             // OCTAVE UP = a ^ (shape)
            line(cx + 2, ry + 4, cx + 4, ry + 1, CLR_BLACK);
            line(cx + 4, ry + 1, cx + 6, ry + 4, CLR_BLACK);
        } else if (oct[i] < 0) {                      // OCTAVE DOWN = a v
            line(cx + 2, ry + rh - 5, cx + 4, ry + rh - 2, CLR_BLACK);
            line(cx + 4, ry + rh - 2, cx + 6, ry + rh - 5, CLR_BLACK);
        }
        if (sld[i] && deg[(i + 1) % 16] >= 0) {       // SLIDE = a line to the next note
            int r2 = NR - 1 - deg[(i + 1) % 16];
            line(cx + step, ry + rh / 2, gx + (i + 1) * step, gy + r2 * rh + rh / 2, CLR_LIGHT_PEACH);
        }
    }
    // legend
    font(FONT_TINY);
    print("wht=ACC  ^v=OCT  \x7f=SLIDE  []=TIE", gx, gy + rh * NR + 1, CLR_MEDIUM_GREEN);
    font(FONT_NORMAL);
}

// screen content: the drum 2D GRID (rows = voices, cols = 16 steps) — the whole beat
// at a glance. left gutter names the voices; the selected row is lit.
static void screendrum(int x, int cy0, int w, int ch, int focus) {
    int hi = FHI[focus];
    int nv = (focus == 2) ? 16 : 11;
    const char **vn = (focus == 2) ? V808 : V909;
    int sel = 0;                          // selected voice (kick) — its row is highlit

    int gut = 13, gx = x + gut, gw = w - gut - 2, step = gw / 16;
    int gy = cy0, gh = ch, rh = gh / nv;

    // playhead column behind the cells
    rectfill(gx + playstep * step, gy, step - 1, rh * nv, CLR_DARK_GREEN);

    for (int v = 0; v < nv; v++) {
        int ry = gy + v * rh;
        if (v == sel) rectfill(x + 1, ry, gut - 1, rh - 1, CLR_DARK_GREEN);
        font(FONT_TINY);
        print(vn[v], x + 2, ry + (rh - 5) / 2, (v == sel) ? CLR_WHITE : CLR_MEDIUM_GREEN);
        font(FONT_NORMAL);
        for (int i = 0; i < 16; i++) {
            int cx = gx + i * step;
            if (drum_hit(v, i)) {
                rectfill(cx + 1, ry + 1, step - 2, rh - 2, drum_acc(v, i) ? CLR_WHITE : hi);
            } else {
                pset(cx + step / 2, ry + rh / 2, (i % 4 == 0) ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK);
            }
        }
    }
}

// screen content: the master automation lanes (PCF / CRUSH / GATE)
static void screenmst(int x, int cy0, int w, int ch) {
    const char *lab[3] = { "PCF", "CRU", "GAT" };
    const int col[3] = { CLR_GREEN, CLR_ORANGE, CLR_PINK };
    int lh = ch / 3, gx = x + 22, step = (w - 26) / 16;
    for (int L = 0; L < 3; L++) {
        int ly = cy0 + L * lh;
        font(FONT_TINY); print(lab[L], x + 4, ly + lh / 2 - 2, col[L]); font(FONT_NORMAL);
        for (int i = 0; i < 16; i++) {
            int cx = gx + i * step;
            int v = (int)((0.5f + 0.45f * sinf(i * 0.6f + L)) * (lh - 4));   // a wavy automation
            if (i == playstep) rectfill(cx, ly, step - 1, lh - 2, CLR_DARK_GREEN);
            rectfill(cx, ly + (lh - 2) - v, step - 2, v, col[L]);
        }
    }
}

// the big shared middle screen — shows whichever machine is FOCUSED
static void bigscreen(int x, int y, int w, int h, int focus) {
    rectfill(x, y, w, h, LCD_BG);
    rect(x, y, w, h, FLO[focus]);              // border recolours to the focused machine
    rect(x + 1, y + 1, w - 2, h - 2, CLR_BLACK);

    // top tag row — names who owns the screen right now
    font(FONT_TINY);
    print(FNAME[focus], x + 4, y + 3, FHI[focus]);
    print(FROLE[focus], x + 30, y + 3, CLR_MEDIUM_GREEN);
    print("132 BPM", x + w - 34, y + 3, LCD_INK);
    font(FONT_NORMAL);

    int cy0 = y + 13, ch = h - 26;
    const char *k303[5] = { "SEQ", "FLAG", "FX", "PERF", "GEN" };
    const char *kdrm[5] = { "VCE", "FLAG", "KIT", "PERF", "GEN" };
    const char *kmst[4] = { "MIX", "PCF", "CRU", "GAT" };
    if (focus <= 1) {
        screen303(x, cy0, w, ch, focus);
        softkeys(x, w, y + h - 11, k303, 5, FLO[focus]);
    } else if (focus <= 3) {
        screendrum(x, cy0, w, ch, focus);
        softkeys(x, w, y + h - 11, kdrm, 5, FLO[focus]);
    } else {
        screenmst(x, cy0, w, ch);
        softkeys(x, w, y + h - 11, kmst, 4, FLO[focus]);
    }
}

// the MASTER column — same narrow knob-strip shape as a 303: header + master
// knobs + a mini mixer + the delay division
static void colMST(int x, int y, int w, int h, int focused) {
    rectfill(x, y, w, h, PANEL);
    rect(x, y, w, h, M_LO);

    // header — tap to focus master onto the screen
    rectfill(x + 2, y + 2, w - 4, 12, M_LO);
    rect(x + 2, y + 2, w - 4, 12, M_HI);
    print("MST", x + 4, y + 5, M_HI);
    circfill(x + w - 6, y + 8, 2, M_HI);
    if (focused) focus_ring(x + 2, y + 2, w - 4, 12);

    // 5 master knobs stacked (matches the 303 columns' spacing)
    const char *names[5] = { "TMP", "SWG", "GLU", "FLT", "PMP" };
    const char *vals[5]  = { "132", "18",  "50",  "75",  "40" };
    float vv[5] = { 0.66f, 0.18f, 0.50f, 0.75f, 0.40f };
    int ky = y + 24, kgap = (h - 46) / 5;
    for (int i = 0; i < 5; i++)
        dial(x + 11, ky + i * kgap, 6, vv[i], M_HI, names[i], vals[i]);

    // bottom: a 4-channel mini mixer + the delay division
    int sy = ky + 5 * kgap + 1;
    const int lv[4] = { 9, 6, 10, 8 };
    const int mc[4] = { A_HI, B_HI, D8_HI, D9_HI };
    for (int i = 0; i < 4; i++) {
        int cx = x + 4 + i * 8;
        rectfill(cx, sy, 6, 10, CLR_BLACK);
        rectfill(cx, sy + 10 - lv[i], 6, lv[i], mc[i]);
    }
    font(FONT_TINY);
    print("DLY 1/16", x + 4, sy + 12, M_HI);
    font(FONT_NORMAL);
}

// the transport bar across the very top — now just PLAY + title + clock
static void transport(int x, int y, int w, int h) {
    rectfill(x, y, w, h, CHASSIS);

    // PLAY button
    rectfill(x + 2, y + 1, 26, h - 2, M_LO);
    rect(x + 2, y + 1, 26, h - 2, M_HI);
    for (int i = 0; i < 6; i++) line(x + 9, y + 3 + i, x + 9, y + h - 4 - i, CLR_WHITE);
    line(x + 9, y + 3, x + 18, y + h / 2, CLR_WHITE);
    line(x + 9, y + h - 4, x + 18, y + h / 2, CLR_WHITE);

    print("TINY ACID JAM", x + 32, y + 2, INK);
    font(FONT_TINY);
    print("iPad \x7f everything at once", x + 32, y + 8, DIM);
    font(FONT_NORMAL);

    // BPM clock, right-aligned
    print("132", x + w - 58, y + 2, M_HI);
    font(FONT_TINY);
    print("BPM", x + w - 34, y + 3, DIM);
    print("A-min pent", x + w - 34, y + 9, DIM);
    font(FONT_NORMAL);
}

void update(void) {
    // slow playhead sweep, ~8 steps/sec feel
    playstep = ((int)(now() * 6.0f)) % 16;
    // demo the STICKY focus: hold each machine on the screen for ~3s, then advance
    // (in the real cart you tap a nameplate; here it cycles so you see every state)
    focus = ((int)(now() / 3.0f)) % 5;
    // demo the SHARED context knobs: alternate picking an 808 (blue) vs 909 (yellow)
    // voice, so the panel's ring flips colour to match the selected machine
    int sd = ((int)(now() / 2.0f)) % 4;
    if (sd < 2) { sel_mach = 2; sel_voice = (sd == 0) ? 0 : 10; }   // 808 voices → blue
    else        { sel_mach = 3; sel_voice = (sd == 2) ? 1 : 7;  }   // 909 voices → yellow
}

void draw(void) {
    cls(CHASSIS);

    // ── layout bands ────────────────────────────────────────────────────────
    // narrow knob-strips + shorter drum strips give the screen the freed space.
    // 0   transport                          (h14)
    // 15  [303a|303b| BIG SCREEN |MST]        (h123) — narrow columns (+FX row), big screen
    // 140 909 strip                          (h24)  — drum section, even shorter cells,
    // 165 808 strip                          (h24)    stacked at the bottom (808 under 909)
    // 191 legend                             (h9)
    transport(0, 0, SCREEN_W, 14);

    // middle band: two narrow 303 knob-strips, the big screen, the master knob-strip
    int midY = 15, midH = 123;
    col303(0,   midY, 46, midH, A_HI, A_LO, "303a", "BASS", "Am", 0, focus == 0, 1);  // mirrored: dials off the left border
    col303(48,  midY, 46, midH, B_HI, B_LO, "303b", "LEAD", "Am", 0, focus == 1, 0);
    bigscreen(96, midY, 176, midH, focus);
    colMST(274, midY, 46, midH, focus == 4);

    // drum section, stacked at the bottom (909 on top of 808) — one PAD per voice; the
    // 909's spare room holds the SHARED context knobs, coloured for the selected machine
    int sel_hi = (sel_mach == 2) ? D8_HI : D9_HI;
    const char *sel_name = (sel_mach == 2) ? V808[sel_voice] : V909[sel_voice];
    drum_strip(0, 140, SCREEN_W, 24, D9_HI, D9_LO, "909", 11, V909, 3, sel_mach, sel_voice, sel_hi, sel_name, focus == 3);
    drum_strip(0, 165, SCREEN_W, 24, D8_HI, D8_LO, "808", 16, V808, 2, sel_mach, sel_voice, sel_hi, sel_name, focus == 2);

    // legend
    font(FONT_TINY);
    print("STICKY FOCUS \x7f tap a nameplate to put that machine on the big screen; play stays live for all", 4, 192, DIM);
    font(FONT_NORMAL);
}
