/* de:meta
{
  "slug": "walkmock",
  "title": "walk mock (tiny groovebox UI)",
  "status": "active",
  "created": "2026-11-15",
  "kind": [
    "probe"
  ],
  "teaches": [],
  "description": "A UI MOCKUP (not functional, no sound) for a tiny portrait groovebox (180x320) that takes a sniff of three carts: rootwalk's walking root, chordblossom's chord bloom + voicing cascade, onenote's one-note bass and XY tone. The FOCUS layout: one layer open, the other three folded to a bare lane each, all on one 16-column grid. Press 1 BASS open, 2 CHORD open, 3 LEAD open, 4 a WALK card selected (the bottom row becomes that chord's editor). Left alone it cycles."
}
de:meta */
#include "studio.h"
#include <stdio.h>

// WALKMOCK: draw-only layout sketch, portrait 180x320 (scale 3), fake 100 bpm clock off now().
// The low resolution is on purpose: it forces the cuts. Rules being tested:
//   * one layer OPEN, the rest FOLDED to a bare lane (tab + lane, no words) that still grooves.
//   * TAB = mute (and, when open, the row labels), NAME = sound, LANE = steps.
//   * every lane on ONE 16-column grid, so the playhead is one column down the whole stack.
//   * the BOTTOM ROW edits whatever is selected: KEY, or the selected walk chord.

#define W SCREEN_W
#define H SCREEN_H

#define TX 2       // tab left
#define TW 12      // tab width
#define GX 16      // grid left
#define CS 10      // column stride: 16 * 10 = 160 -> ends at 176
#define CW 9       // cell width
#define GR (GX + 16 * CS - 1)   // grid right edge

static const char *CH_NAME[4] = { "C", "Am", "F", "G7" };
static const char *CH_DEG [4] = { "I", "vi", "IV", "V7" };
static const int   CH_ROOT[4] = { 0, 9, 5, 7 };
static const int   CH_TONES[4][4] = { {0,4,7,-1}, {0,3,7,-1}, {0,4,7,-1}, {0,4,7,10} };
static const char *NOTE[12] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };
static const int   CH_COL[4] = { CLR_PINK, CLR_INDIGO, CLR_ORANGE, CLR_BLUE_GREEN };

static const char DRUM_PAT[3][17] = { "x...x...x...x.x.", "....x.......x...", "x.x.x.x.x.x.x.xx" };
static const int  DRUM_C[3] = { CLR_RED, CLR_ORANGE, CLR_YELLOW };
// chord rhythm with LENGTHS: a digit = a stab starting here that lasts that many steps
static const char CHORD_RHY[17] = "..2...3...1..2..";
static const int  LEAD_PAT[16] = { 4,-1,3,-1, 2,2,-1,0, -1,1,-1,2, 3,-1,-1,4 };
static const char LEAD_WHERE[17] = "x.x.xxxx.x.xx..x";   // where the lead MAY play (it decides the notes)

enum { D_A, D_R, D_5, D_B7, D_8, ND };
static const char *DEG_N[ND] = { "A", "R", "5", "7", "8" };
static const int   DEG_SEMI[ND] = { -1, 0, 7, 10, 12 };
static const int   BASS_FIG[16] = { D_R,-1,D_R,D_8, -1,-1,D_B7,-1, D_R,-1,D_5,-1, -1,D_8,D_B7,D_A };

enum { L_DRUMS, L_BASS, L_CHORD, L_LEAD, NL };
static const char  LCHR [NL] = { 'D', 'B', 'C', 'L' };
static const int   LCOL [NL] = { CLR_ORANGE, CLR_BLUE, CLR_PINK, CLR_GREEN };
static const int   LDARK[NL] = { CLR_BROWNISH_BLACK, CLR_DARKER_BLUE, CLR_DARKER_PURPLE, CLR_BROWNISH_BLACK };

static float mbeat, bloom;  static int step, mbar, view = 0, picked = 0;

// ── primitives ────────────────────────────────────────────────────────────────
static void small(const char *s, int x, int y, int c) { font(FONT_SMALL); if (x < 0) x = W - 3 - text_width(s); print(s, x, y, c); font(FONT_NORMAL); }
static void tiny (const char *s, int x, int y, int c) { font(FONT_TINY); print(s, x, y, c); font(FONT_NORMAL); }
static void csmall(const char *s, int cx, int y, int c) { font(FONT_SMALL); print(s, cx - text_width(s) / 2, y, c); font(FONT_NORMAL); }
static void cprint(const char *s, int cx, int y, int c) { print(s, cx - text_width(s) / 2, y, c); }

static void knob(int cx, int cy, int r, float v, int col) {
    circfill(cx, cy + 1, r, CLR_BLACK);
    circfill(cx, cy, r, CLR_DARKER_GREY);
    circ(cx, cy, r, CLR_DARK_GREY);
    float a = 225.0f - v * 270.0f;
    line(cx, cy, cx + (int)(cos_deg(a) * (r - 2)), cy - (int)(sin_deg(a) * (r - 2)), col);
}
static void chip(int x, int y, int w, int h, const char *lab, int on, int col) {
    rrectfill(x, y + 1, w, h, 2, CLR_BLACK);
    rrectfill(x, y, w, h, 2, on ? col : CLR_DARKER_GREY);
    csmall(lab, x + w / 2, y + h / 2 - 3, on ? CLR_BLACK : CLR_LIGHT_GREY);
}
static void tab(int l, int y, int h) {
    rrectfill(TX, y, TW, h, 2, LCOL[l]);
    char n[2] = { LCHR[l], 0 };
    csmall(n, TX + TW / 2, y + h / 2 - 3, CLR_BLACK);
}
static void shade(int x, int s, int y, int h) { rectfill(x, y, CW, h, s / 4 % 2 ? CLR_DARKER_GREY : CLR_BLACK); }
static void playhead(int y, int h) { rect(GX + step * CS - 1, y - 1, CW + 2, h + 2, CLR_WHITE); }

// the chord rhythm lane: stabs drawn as bars as long as they ring
static void chord_lane(int y, int h) {
    for (int s = 0; s < 16; s++) shade(GX + s * CS, s, y, h);
    for (int s = 0; s < 16; s++) if (CHORD_RHY[s] > '0') {
        int len = CHORD_RHY[s] - '0';
        rectfill(GX + s * CS, y, len * CS - 1, h, CLR_PINK);
        rectfill(GX + s * CS, y, 2, h, CLR_WHITE);            // the attack
    }
}

// chord tones cascaded by the voicing step (chordblossom's trick); semitones above C3
static int voiced(int ch, int shift, int out[5]) {
    int n = CH_TONES[ch][3] < 0 ? 3 : 4, k = 0;
    for (int i = 0; i < n; i++) {
        int idx = i - shift % n, oct = shift / n + (idx < 0 ? 1 : 0);
        out[k++] = CH_ROOT[ch] + CH_TONES[ch][i] + 12 * oct;
    }
    return k;
}

// ── a FOLDED layer: tab + lane, nothing else ─────────────────────────────────
#define FOLD_H 12
static void folded(int l, int y) {
    tab(l, y, FOLD_H);
    if (l == L_CHORD) { chord_lane(y, FOLD_H); playhead(y, FOLD_H); return; }
    for (int s = 0; s < 16; s++) {
        int x = GX + s * CS;
        shade(x, s, y, FOLD_H);
        if (l == L_DRUMS) for (int r = 0; r < 3; r++) if (DRUM_PAT[r][s] == 'x') rectfill(x, y + 9 - r * 4, CW, 3, DRUM_C[r]);
        if (l == L_BASS && BASS_FIG[s] >= 0) rectfill(x, y + 9 - BASS_FIG[s] * 2, CW, 3, CLR_BLUE);
        if (l == L_LEAD && LEAD_PAT[s] >= 0) rectfill(x + 1, y + 9 - LEAD_PAT[s] * 2, CW - 2, 3, CLR_GREEN);
    }
    playhead(y, FOLD_H);
}

// the open-panel frame + header: NAME + "sound v" picker + what it's sounding
static void open_frame(int l, int y, int h, const char *name, const char *snd, const char *now_s) {
    rrectfill(TX, y, W - 2 * TX, h, 3, LDARK[l]);
    rrectfill(TX, y, TW, h, 3, LCOL[l]);
    print(name, GX, y + 3, LCOL[l]);
    small(snd, GX + text_width(name) + 4, y + 4, CLR_LIGHT_GREY);
    print(now_s, GR - text_width(now_s) + 1, y + 3, CLR_WHITE);
}

// ── BASS open ─────────────────────────────────────────────────────────────────
static void open_bass(int y, int h) {
    int root = CH_ROOT[mbar];
    open_frame(L_BASS, y, h, "BASS", "upright v", NOTE[root]);
    int ly = y + 14, rh = 11;
    for (int d = 0; d < ND; d++) {
        int ry = ly + (ND - 1 - d) * rh;
        csmall(DEG_N[d], TX + TW / 2, ry + 3, CLR_BLACK);
        for (int s = 0; s < 16; s++) {
            int x = GX + s * CS;
            shade(x, s, ry, rh - 1);
            if (BASS_FIG[s] == d) {
                rectfill(x, ry, CW, rh - 1, d == D_R ? CLR_BLUE : CLR_TRUE_BLUE);
                int semi = d == D_A ? (CH_ROOT[(mbar + 1) % 4] + 11) % 12 : (root + DEG_SEMI[d]) % 12;
                tiny(NOTE[semi], x + 1, ry + 3, CLR_WHITE);
            }
        }
    }
    playhead(ly, ND * rh - 1);
    int fy = ly + ND * rh + 4;
    static const char *FIG[4] = { "R", "R8", "R87", "+A" };
    for (int i = 0; i < 4; i++) chip(GX + i * 40, fy, 38, 11, FIG[i], i == 2, CLR_BLUE);

    int py = fy + 16, ph = y + h - 4 - py, pw = ph;
    if (pw > 100) pw = ph = 100;
    rectfill(GX, py, pw, ph, CLR_BLACK);
    float tx = 0.35f + 0.15f * sin_deg(now() * 40), ty = 0.6f;
    int dx = GX + (int)(tx * pw), dy = py + (int)((1 - ty) * ph);
    line(dx, py, dx, py + ph - 1, CLR_DARK_BLUE);
    line(GX, dy, GX + pw - 1, dy, CLR_DARK_BLUE);
    circfill(dx, dy, 3, CLR_BLUE); circ(dx, dy, 3, CLR_WHITE);
    tiny("TONE", GX + 2, py + 2, CLR_DARK_BLUE);
    int cx = GX + pw + 5, cw = GR - cx + 1;
    chip(cx, py, cw, 14, "SUB", 1, CLR_BLUE);
    chip(cx, py + 18, cw / 2 - 1, 14, "-", 0, CLR_BLUE);
    chip(cx + cw / 2 + 1, py + 18, cw / 2 - 1, 14, "+", 0, CLR_BLUE);
    csmall("OCT", cx + cw / 2, py + 35, CLR_DARK_GREY);
    if (ph >= 70) { knob(cx + cw / 2, py + ph - 20, 10, 0.5f, CLR_BLUE); csmall("GLIDE", cx + cw / 2, py + ph - 7, CLR_DARK_GREY); }
}

// ── CHORD open: the bloom, by touch ───────────────────────────────────────────
// keys on the grid (16 white keys = 16 columns, so they line up with the steps below),
// the voicing CASCADE as a strip you tap (each column is one voicing, drawn as its notes),
// the rhythm with lengths, and the playing style.
static void open_chord(int y, int h) {
    int sh = (int)(bloom * 5.99f);                         // the voicing step that's picked
    open_frame(L_CHORD, y, h, "CHORD", "rhodes v", CH_NAME[mbar]);
    int v[5], n = voiced(mbar, sh, v);
    int top = v[0]; for (int i = 1; i < n; i++) if (v[i] > top) top = v[i];

    // 1. the keys: 3 octaves from C3 (not on the step grid: a keyboard doesn't need to be),
    //    chord tones lit, the TOP note dotted. Tap a key = make that the top note.
    static const int WHITE_OF[12] = { 0,-1,1,-1,2,3,-1,4,-1,5,-1,6 };
    static const int BLACK_AT[12] = { -1,0,-1,1,-1,-1,3,-1,4,-1,5,-1 };
    int ky = y + 14, kh = 50, ks = 7, kx = GX + 6;            // 21 whites * 7 = 147
    for (int k = 0; k < 21; k++) rectfill(kx + k * ks, ky, ks - 1, kh, CLR_LIGHT_PEACH);
    for (int i = 0; i < n; i++) {
        int pc = v[i] % 12, wi = (v[i] / 12) * 7 + WHITE_OF[pc];
        if (WHITE_OF[pc] >= 0 && wi < 21) {
            rectfill(kx + wi * ks, ky + 30, ks - 1, kh - 30, CLR_PINK);
            if (v[i] == top) rectfill(kx + wi * ks + 1, ky + kh - 6, ks - 3, 3, CLR_WHITE);
        }
    }
    for (int o = 0; o < 3; o++) for (int pc = 0; pc < 12; pc++) if (BLACK_AT[pc] >= 0) {
        int bx = kx + (o * 7 + BLACK_AT[pc]) * ks + 4, lit = 0;
        for (int i = 0; i < n; i++) if (v[i] == o * 12 + pc) lit = 1;
        rectfill(bx, ky, 5, 28, CLR_BLACK);
        if (lit) { rectfill(bx + 1, ky + 12, 3, 15, CLR_PINK); if (o * 12 + pc == top) rectfill(bx + 1, ky + 22, 3, 3, CLR_WHITE); }
    }
    csmall("T", TX + TW / 2, ky + kh - 8, CLR_BLACK);

    // 2. the voicing cascade: 6 voicings side by side, each drawn as its notes (one px per
    //    semitone, low = down). Tap one. This IS the bloom knob, but you see where it goes.
    int cy = ky + kh + 5, chh = 40, cwid = 160 / 6;
    rectfill(GX, cy, 16 * CS - 1, chh, CLR_BLACK);
    for (int c = 0; c < 6; c++) {
        int vv[5], nn = voiced(mbar, c, vv), x = GX + c * cwid;
        if (c == sh) rectfill(x, cy, cwid - 1, chh, CLR_DARK_PURPLE);
        for (int i = 0; i < nn; i++)
            rectfill(x + 4, cy + chh - 4 - vv[i], cwid - 9, 2, c == sh ? CLR_PINK : CLR_MAUVE);
    }
    rect(GX + sh * cwid - 1, cy - 1, cwid + 1, chh + 2, CLR_WHITE);
    csmall("V", TX + TW / 2, cy + chh / 2 - 3, CLR_BLACK);

    // 3. the rhythm, with lengths (drag a stab to stretch it)
    int ry = cy + chh + 5;
    chord_lane(ry, 14);
    playhead(ry, 14);
    csmall("R", TX + TW / 2, ry + 4, CLR_BLACK);

    // 4. how it plays, and voice-leading
    int sy = ry + 20;
    static const char *STY[4] = { "PAD", "STAB", "SKANK", "STRUM" };
    for (int i = 0; i < 4; i++) chip(GX + i * 40, sy, 38, 12, STY[i], i == 1, CLR_PINK);
    chip(GX, sy + 17, 78, 12, "SMOOTH ON", 1, CLR_PINK);
    chip(GX + 82, sy + 17, 78, 12, "+ BASS NOTE", 0, CLR_PINK);
}

// ── LEAD open: it writes the notes, you say where and how much ───────────────
static void open_lead(int y, int h) {
    open_frame(L_LEAD, y, h, "LEAD", "square v", CH_NAME[mbar]);
    // 1. what it's going to play (its pick, shown dim-bright, not hand-edited): 5 rows of the
    //    chord's pentatonic, labelled by degree in the tab
    static const char *PD[5] = { "1", "2", "3", "5", "6" };
    int ly = y + 14, rh = 10;
    for (int r = 0; r < 5; r++) {
        int ry = ly + (4 - r) * rh;
        csmall(PD[r], TX + TW / 2, ry + 2, CLR_BLACK);
        for (int s = 0; s < 16; s++) {
            int x = GX + s * CS;
            shade(x, s, ry, rh - 1);
            if (LEAD_PAT[s] == r) rectfill(x, ry, CW, rh - 1, s <= step ? CLR_GREEN : CLR_MEDIUM_GREEN);
        }
    }
    playhead(ly, 5 * rh - 1);

    // 2. WHERE it may play: the one row you paint
    int wy = ly + 5 * rh + 4;
    for (int s = 0; s < 16; s++) {
        int x = GX + s * CS;
        shade(x, s, wy, 14);
        if (LEAD_WHERE[s] == 'x') rrectfill(x + 1, wy + 2, CW - 2, 10, 2, CLR_LIME_GREEN);
    }
    playhead(wy, 14);
    csmall("?", TX + TW / 2, wy + 4, CLR_BLACK);

    // 3. the dice + keep: reroll a new phrase, or keep this one looping
    int dy = wy + 20;
    rrectfill(GX, dy + 1, 78, 26, 3, CLR_BLACK);
    rrectfill(GX, dy, 78, 26, 3, CLR_GREEN);
    rrectfill(GX + 8, dy + 5, 16, 16, 2, CLR_WHITE);                    // a die
    circfill(GX + 12, dy + 9, 1, CLR_BLACK); circfill(GX + 16, dy + 13, 1, CLR_BLACK); circfill(GX + 20, dy + 17, 1, CLR_BLACK);
    print("NEW", GX + 32, dy + 9, CLR_BLACK);
    chip(GX + 82, dy, 78, 26, "KEEP", 0, CLR_GREEN);

    // 4. how it thinks: mode chips, then BUSY + RANGE
    int my = dy + 32;
    static const char *MODE[3] = { "RIFF", "ANSWER", "SOLO" };
    for (int i = 0; i < 3; i++) chip(GX + i * 54, my, 52, 12, MODE[i], i == 0, CLR_GREEN);
    int ky = my + 18, kr = 11;
    if (y + h - ky > 2 * kr + 12) {
        knob(GX + 40, ky + kr, kr, 0.4f, CLR_GREEN);  csmall("BUSY",  GX + 40,  ky + 2 * kr + 4, CLR_DARK_GREY);
        knob(GX + 120, ky + kr, kr, 0.6f, CLR_GREEN); csmall("RANGE", GX + 120, ky + 2 * kr + 4, CLR_DARK_GREY);
    }
}

// ── top: the WALK row ─────────────────────────────────────────────────────────
static void walk_row(int y, int sel) {
    for (int i = 0; i < 4; i++) {
        int x = 2 + i * 44, on = i == mbar;
        rrectfill(x, y + 1, 42, 22, 2, CLR_BLACK);
        rrectfill(x, y, 42, 22, 2, on ? CH_COL[i] : CLR_DARKER_GREY);
        print(CH_NAME[i], x + 4, y + 4, on ? CLR_BLACK : CH_COL[i]);
        tiny(CH_DEG[i], x + 4, y + 14, on ? CLR_BLACK : CLR_DARK_GREY);
        if (on) rectfill(x + 2, y + 19, (int)(38 * (step + 1) / 16.0f), 1, CLR_WHITE);
        if (i == sel) rrect(x - 1, y - 1, 44, 24, 3, CLR_WHITE);
    }
}

// ── bottom: ONE row that edits what's selected ───────────────────────────────
static void key_row(int y) {
    static const int KEYS[8] = { 0, 2, 3, 5, 7, 9, 10, 12 };
    for (int i = 0; i < 8; i++) {
        int x = 2 + i * 22, on = i == 0;
        rrectfill(x, y + 1, 20, 22, 2, CLR_BLACK);
        rrectfill(x, y, 20, 22, 2, on ? CLR_YELLOW : CLR_DARKER_GREY);
        csmall(NOTE[KEYS[i] % 12], x + 10, y + 8, on ? CLR_BLACK : CLR_LIGHT_GREY);
    }
}
static void chord_row(int y) {
    int c = CH_COL[3];
    static const char *Q[5] = { "MAJ", "MIN", "7", "-7", "SUS" };
    for (int i = 0; i < 5; i++) chip(2 + i * 26, y, 24, 12, Q[i], i == 2, c);
    chip(132, y, 46, 12, "2 BARS", 0, c);
    static const char *ROOTS[7] = { "C", "D", "E", "F", "G", "A", "B" };
    for (int i = 0; i < 7; i++) {
        int x = 2 + i * 25, on = i == 4;
        rrectfill(x, y + 17, 23, 22, 2, CLR_BLACK);
        rrectfill(x, y + 16, 23, 22, 2, on ? c : CLR_DARKER_GREY);
        cprint(ROOTS[i], x + 12, y + 23, on ? CLR_BLACK : CLR_LIGHT_GREY);
    }
}

void draw(void) {
    mbeat = now() * 100.0f / 60.0f;
    step = (int)(mbeat * 4) % 16;
    mbar = (int)(mbeat / 4) % 4;
    bloom = 0.5f + 0.5f * sin_deg(now() * 25);
    for (int k = 0; k < 4; k++) if (keyp('1' + k)) { view = k; picked = 1; }
    if (!picked) view = (int)(now() / 5) % 4;

    int open = view == 1 ? L_CHORD : view == 2 ? L_LEAD : L_BASS;
    int editing = view == 3;

    cls(CLR_BLACK);
    font(FONT_TIC); print("walkbox", 3, 4, CLR_LIGHT_PEACH); font(FONT_NORMAL);
    small(editing ? "chord 4  done" : "100 bpm", 70, 5, editing ? CLR_WHITE : CLR_DARK_GREY);
    rrectfill(W - 22, 2, 20, 11, 2, (int)(mbeat * 2) % 2 ? CLR_GREEN : CLR_DARK_GREEN);
    trifill(W - 15, 4, W - 15, 10, W - 9, 7, CLR_BLACK);

    walk_row(17, editing ? 3 : -1);

    // the stack: folded layers are FOLD_H, the open one takes what's left
    int bottom_h = editing ? 39 : 23;
    int by = H - 3 - bottom_h;
    int top = 45, gap = 4;
    int open_h = (by - 6) - top - 3 * (FOLD_H + gap);
    int y = top;
    for (int l = 0; l < NL; l++) {
        if (l == open) {
            if (l == L_BASS) open_bass(y, open_h);
            if (l == L_CHORD) open_chord(y, open_h);
            if (l == L_LEAD) open_lead(y, open_h);
            y += open_h + gap;
        } else { folded(l, y); y += FOLD_H + gap; }
    }
    if (editing) chord_row(by); else key_row(by);
}
