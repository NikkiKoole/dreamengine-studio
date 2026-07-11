/* de:meta
{
  "slug": "distlab",
  "title": "distortion lab",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": {
    "summary": "A distortion PLAYGROUND: a real master insert CHAIN — PRE-EQ -> DRIVE -> POST-EQ — on an auto-sliding acid drone, with a live transfer curve AND a real oscilloscope of the post-FX output. Rung 1 of a growable 'destruction unit' (design/distortion-lab.md): the tone stack (EQ before AND after the clipper) now wraps the drive stage, with room in the chain for more.",
    "detail": "The chain IS fx_order(0, {FX_EQ, FX_DRIVE, FX_INST(FX_EQ,1)}) — pre-EQ (instance 0) shapes what gets distorted, drive_insert() is the dirt (SOFT/HARD/FOLD/ASYM), post-EQ (instance 1) tames the fizz. A flat param cursor walks the whole chain: LEFT/RIGHT moves it, UP/DOWN adjusts (or cycles the drive mode); the stage holding the cursor highlights. TRANSFER = the drive stage's input->output curve (identity ghosted); OUTPUT = a real scope_read() of the post-FX mix, zero-crossing-frozen. The drone is one held note slid along a riff (note_pitch) so the sound is continuous. Cart-land only — no engine changes.",
    "controls": "LEFT/RIGHT move the param cursor across the chain - UP/DOWN adjust the cursored param (on DRIVE: cycle the waveshaper / ride the amount) - tap a chain block to jump the cursor there - drag the sliders/bars on touch"
  }
}
de:meta */
// distortion lab (Rung 1) — a PRE-EQ -> DRIVE -> POST-EQ master insert chain
// on a held acid drone, + a transfer curve + a live scope.
// The tone stack: EQ before the clipper picks what distorts, EQ after tames it.
// Growable seed; see docs/design/distortion-lab.md.
//   ← → : move cursor    ↑ ↓ : adjust    tap a block to jump    drag sliders
#include "studio.h"
#include <stdio.h>
#include <math.h>

#define I_LEAD 0
#define NSCOPE 256

enum { ST_PRE, ST_DRIVE, ST_POST, NSTAGE };
enum { P_PRE_LO, P_PRE_MID, P_PRE_HI, P_DRV_MODE, P_DRV_AMT, P_POST_LO, P_POST_MID, P_POST_HI, NPARAM };

static const char *MODE_NAME[4] = { "SOFT", "HARD", "FOLD", "ASYM" };
static const char *MODE_DESC[4] = { "tanh \x1a warm tube overdrive",
                                    "hard clip \x1a buzzy digital fuzz",
                                    "sine wavefolder \x1a glassy metal",
                                    "asymmetric \x1a fat even harmonics" };
static const char *BAND[3] = { "LO", "MID", "HI" };

static int   cur  = P_DRV_MODE;   // the flat param cursor across the whole chain
static int   mode = 2;            // start on FOLD — the fun one
static float amt  = 0.55f;
static float preq[3]  = { 0, 0, 0 };   // pre-EQ  LO/MID/HI dB (-12..12)
static float postq[3] = { 0, 0, 0 };   // post-EQ LO/MID/HI dB
static int   h = -1;              // the held drone voice (continuous → the scope always has signal)

static int   param_stage(int p) { return p <= P_PRE_HI ? ST_PRE : p <= P_DRV_AMT ? ST_DRIVE : ST_POST; }
static float clampdb(float v)    { return v < -12 ? -12 : v > 12 ? 12 : v; }

// the SAME waveshaper family the engine uses, re-implemented for the transfer
// curve (representative — the scope is the truthful readout). g = pre-gain.
static float shape(float x, int m, float a) {
    float g = 1.0f + a * 4.0f, v = x * g;
    switch (m) {
        case 0:  return tanhf(v);
        case 1:  return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
        case 2:  return sinf(v);
        default: return x >= 0.0f ? tanhf(v) : tanhf(v * 0.6f);
    }
}

// ── set-and-hold: re-apply an insert ONLY on change (never per frame) ──
static void apply_pre(void)   { eq(preq[0], preq[1], preq[2]); }
static void apply_post(void)  { eq_inst(1, postq[0], postq[1], postq[2]); }
static void apply_drive(void) { drive_insert(amt, mode, 1.0f); }
static void set_mode(int m)   { mode = m & 3; apply_drive(); }
static void set_amt(float a)  { amt = a < 0 ? 0 : a > 1 ? 1 : a; apply_drive(); }

void init(void) {
    instrument(I_LEAD, INSTR_SAW, 4, 220, 6, 300);
    instrument_filter(I_LEAD, FILTER_DIODE, 1200, 11);   // a resonant acid peak to drive INTO
    static const int chain[3] = { FX_EQ, FX_DRIVE, FX_INST(FX_EQ, 1) };  // PRE-EQ → DRIVE → POST-EQ
    fx_order(0, chain, 3);
    apply_pre(); apply_drive(); apply_post();
    h = note_on(48, I_LEAD, 6);                          // one held drone…
    note_glide(h, 55);                                   // …that SLIDES between riff notes (303-style)
    bpm(128);
}

// ── layout constants (draw + update share them) ──
#define CH_Y   24
#define CH_H   16
static const int BLK_X[NSTAGE] = { 10, 96, 214 };
static const int BLK_W[NSTAGE] = { 78, 108, 96 };
#define EQ_ROW0 150
#define EQ_ROWH 13
#define EQ_BX   44
#define EQ_BW   210
#define AMT_X   60
#define AMT_Y   176
#define AMT_W   190

static void adjust(int dir) {   // dir = +1 (up) / -1 (down)
    switch (cur) {
        case P_PRE_LO:  preq[0]  = clampdb(preq[0]  + dir * 0.5f); apply_pre();  break;
        case P_PRE_MID: preq[1]  = clampdb(preq[1]  + dir * 0.5f); apply_pre();  break;
        case P_PRE_HI:  preq[2]  = clampdb(preq[2]  + dir * 0.5f); apply_pre();  break;
        case P_DRV_AMT: set_amt(amt + dir * 0.02f);                              break;
        case P_POST_LO: postq[0] = clampdb(postq[0] + dir * 0.5f); apply_post(); break;
        case P_POST_MID:postq[1] = clampdb(postq[1] + dir * 0.5f); apply_post(); break;
        case P_POST_HI: postq[2] = clampdb(postq[2] + dir * 0.5f); apply_post(); break;
    }
}

void update(void) {
    if (btnp(0, BTN_LEFT))  cur = (cur + NPARAM - 1) % NPARAM;
    if (btnp(0, BTN_RIGHT)) cur = (cur + 1) % NPARAM;
    if (cur == P_DRV_MODE) {                    // the mode STEPS, one per press
        if (btnp(0, BTN_UP))   set_mode(mode + 1);
        if (btnp(0, BTN_DOWN)) set_mode(mode + 3);
    } else {                                    // everything else ramps while held
        if (btn(0, BTN_UP))   adjust(+1);
        if (btn(0, BTN_DOWN)) adjust(-1);
    }

    // tap a chain block → jump the cursor to that stage's first param
    static const int FIRST[NSTAGE] = { P_PRE_LO, P_DRV_MODE, P_POST_LO };
    for (int s = 0; s < NSTAGE; s++)
        if (tapp(BLK_X[s], CH_Y, BLK_W[s], CH_H)) cur = FIRST[s];

    // touch: drag the selected stage's controls
    int st = param_stage(cur);
    if (st == ST_DRIVE) {
        if (tapp(10, EQ_ROW0, 120, EQ_ROWH)) set_mode(mode + 1);   // tap the mode row cycles
        if (tap(AMT_X - 4, AMT_Y - 6, AMT_W + 8, 16)) set_amt((mouse_x() - AMT_X) / (float)AMT_W);
    } else {
        float *q = st == ST_PRE ? preq : postq;
        for (int b = 0; b < 3; b++)
            if (tap(EQ_BX - 4, EQ_ROW0 + b * EQ_ROWH - 2, EQ_BW + 8, EQ_ROWH)) {
                q[b] = clampdb(((mouse_x() - (EQ_BX + EQ_BW / 2)) / (float)(EQ_BW / 2)) * 12.0f);
                cur = (st == ST_PRE ? P_PRE_LO : P_POST_LO) + b;
                if (st == ST_PRE) apply_pre(); else apply_post();
            }
    }

    // auto-riff: SLIDE the held drone along a low acid line — continuous tone.
    static const int riff[8] = { 36, 48, 36, 43, 36, 48, 41, 43 };
    if (every(1) && h >= 0) note_pitch(h, riff[beat() & 7] + 12);
}

static void draw_curve(int bx, int by, int bw, int bh) {
    rectfill(bx, by, bw, bh, CLR_BLACK);
    rect(bx, by, bw, bh, CLR_DARK_PURPLE);
    int mx = bx + bw / 2, my = by + bh / 2, lim = bh / 2 - 2;
    line(bx, my, bx + bw, my, CLR_DARKER_GREY);
    line(mx, by, mx, by + bh, CLR_DARKER_GREY);
    for (int px = 0; px < bw; px++) {                    // identity diagonal (ghost)
        float x = (px / (float)bw) * 2.0f - 1.0f;
        pset(bx + px, my - (int)(x * lim), CLR_DARK_GREY);
    }
    for (int px = 0; px < bw; px++) {                    // the drive stage's transfer curve
        float x = (px / (float)bw) * 2.0f - 1.0f, v = shape(x, mode, amt);
        if (v > 1) v = 1; if (v < -1) v = -1;
        pset(bx + px, my - (int)(v * lim), CLR_ORANGE);
    }
    print("TRANSFER", bx + 3, by + 2, CLR_LIGHT_GREY);
}

static void draw_scope(int bx, int by, int bw, int bh) {
    rectfill(bx, by, bw, bh, CLR_BLACK);
    rect(bx, by, bw, bh, CLR_DARK_PURPLE);
    int my = by + bh / 2, lim = bh / 2 - 2;
    line(bx, my, bx + bw, my, CLR_DARKER_GREY);
    static float sc[NSCOPE];
    scope_read(sc, NSCOPE);
    int start = 0;                                       // freeze on a rising zero-crossing
    for (int i = 1; i < NSCOPE / 2; i++)
        if (sc[i - 1] < 0.0f && sc[i] >= 0.0f) { start = i; break; }
    int prev = my;
    for (int px = 0; px < bw; px++) {
        int si = start + px; if (si >= NSCOPE) break;
        int a = (int)(sc[si] * lim * 2.5f);
        if (a > lim) a = lim; if (a < -lim) a = -lim;
        int y = my - a;
        if (px) line(bx + px - 1, prev, bx + px, y, CLR_GREEN);
        prev = y;
    }
    print("OUTPUT", bx + 3, by + 2, CLR_LIGHT_GREY);
}

static void draw_block(int s, const char *label, int active) {
    rectfill(BLK_X[s], CH_Y, BLK_W[s], CH_H, active ? CLR_ORANGE : CLR_INDIGO);
    rect(BLK_X[s], CH_Y, BLK_W[s], CH_H, active ? CLR_WHITE : CLR_DARK_PURPLE);
    print(label, BLK_X[s] + 6, CH_Y + 5, active ? CLR_BLACK : CLR_WHITE);
    if (s < NSTAGE - 1) print("\x1a", BLK_X[s] + BLK_W[s] + 2, CH_Y + 5, CLR_DARK_GREY);
}

static void draw_eq_slider(const char *lab, float v, int y, int hot) {
    print(lab, 12, y, hot ? CLR_WHITE : CLR_LIGHT_GREY);
    int cx = EQ_BX + EQ_BW / 2, half = EQ_BW / 2 - 1;
    rect(EQ_BX, y, EQ_BW, 7, hot ? CLR_WHITE : CLR_DARK_PURPLE);
    line(cx, y, cx, y + 7, CLR_DARKER_GREY);
    int px = (int)(v / 12.0f * half);
    if (px >= 0) rectfill(cx, y + 1, px, 5, CLR_ORANGE);
    else         rectfill(cx + px, y + 1, -px, 5, CLR_ORANGE);
    char b[10]; snprintf(b, sizeof b, "%+d", (int)(v + (v < 0 ? -0.5f : 0.5f)));
    print(b, EQ_BX + EQ_BW + 6, y, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("DISTORTION LAB", 10, 8, CLR_WHITE);
    font(FONT_SMALL);
    print("tone stack: EQ -> drive -> EQ", 150, 11, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    int st = param_stage(cur);
    char blk[24]; snprintf(blk, sizeof blk, "DRIVE:%s", MODE_NAME[mode]);
    draw_block(ST_PRE,   "PRE-EQ",  st == ST_PRE);
    draw_block(ST_DRIVE, blk,       st == ST_DRIVE);
    draw_block(ST_POST,  "POST-EQ", st == ST_POST);

    draw_curve(10, 46, 140, 92);
    draw_scope(170, 46, 140, 92);

    // the edit panel — the stage holding the cursor
    if (st == ST_DRIVE) {
        print(MODE_NAME[mode], 12, EQ_ROW0, cur == P_DRV_MODE ? CLR_WHITE : CLR_LIGHT_GREY);
        print(MODE_DESC[mode], 60, EQ_ROW0, CLR_YELLOW);
        print("drive", 12, AMT_Y, cur == P_DRV_AMT ? CLR_WHITE : CLR_LIGHT_GREY);
        rect(AMT_X, AMT_Y, AMT_W, 8, cur == P_DRV_AMT ? CLR_WHITE : CLR_DARK_PURPLE);
        rectfill(AMT_X + 1, AMT_Y + 1, (int)((AMT_W - 2) * amt), 6, CLR_ORANGE);
        char buf[12]; snprintf(buf, sizeof buf, "%d%%", (int)(amt * 100 + 0.5f));
        print(buf, AMT_X + AMT_W + 6, AMT_Y, CLR_WHITE);
    } else {
        float *q = st == ST_PRE ? preq : postq;
        int base = st == ST_PRE ? P_PRE_LO : P_POST_LO;
        print(st == ST_PRE ? "PRE-EQ  (shapes what distorts)" : "POST-EQ (tames the fizz)", 12, EQ_ROW0 - 12, CLR_MEDIUM_GREY);
        for (int b = 0; b < 3; b++)
            draw_eq_slider(BAND[b], q[b], EQ_ROW0 + b * EQ_ROWH, cur == base + b);
    }

    print_centered("\x1b \x1a cursor   up/down adjust   tap a block", SCREEN_W / 2, 193, CLR_LIGHT_GREY);
}
