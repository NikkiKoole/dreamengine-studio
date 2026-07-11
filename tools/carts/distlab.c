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
    "summary": "A distortion PLAYGROUND: a real master insert CHAIN — PRE-EQ -> DRIVE A -> DRIVE B -> CRUSH -> TAPE -> POST-EQ — on an auto-sliding acid drone, with a live transfer curve AND a real oscilloscope of the post-FX output. Rung 3 of a growable 'destruction unit' (design/distortion-lab.md): the tone stack + a two-stage waveshaper CASCADE + bitcrush + tape saturation, all stacked. The go-nuts rung.",
    "detail": "The chain IS fx_order(0, {FX_EQ, FX_DRIVE, FX_INST(FX_DRIVE,1), FX_CRUSH, FX_TAPE, FX_INST(FX_EQ,1)}) — pre-EQ, two drive_insert waveshaper stages, crush() (bit+rate reduction), tape() (wow/flutter/sat), post-EQ. A flat param cursor walks all 16 params: LEFT/RIGHT moves it, UP/DOWN adjusts (or cycles a drive mode); tap a chain block to jump. TRANSFER = the COMPOSED drive-cascade curve (B over A; crush/tape are texture, not a static curve); OUTPUT = a real scope_read() of the post-FX mix. The drone is one held note slid along a riff. Cart-land only — no engine changes.",
    "controls": "LEFT/RIGHT move the param cursor across the chain - UP/DOWN adjust the cursored param (on a DRIVE stage: cycle the waveshaper / ride the amount) - tap a chain block to jump the cursor there - drag the sliders/bars on touch"
  }
}
de:meta */
// distortion lab (Rung 3) — PRE-EQ -> DRIVE A -> DRIVE B -> CRUSH -> TAPE -> POST-EQ.
// The go-nuts rung: stack two waveshapers, then bitcrush, then tape saturation.
// Growable seed; see docs/design/distortion-lab.md.
//   ← → : move cursor    ↑ ↓ : adjust    tap a block to jump    drag sliders
#include "studio.h"
#include <stdio.h>
#include <math.h>

#define I_LEAD 0
#define NSCOPE 256

enum { ST_PRE, ST_DRVA, ST_DRVB, ST_CRUSH, ST_TAPE, ST_POST, NSTAGE };
enum { P_PRE_LO, P_PRE_MID, P_PRE_HI,
       P_DA_MODE, P_DA_AMT,
       P_DB_MODE, P_DB_AMT,
       P_CR_BITS, P_CR_RATE, P_CR_MIX,
       P_TP_WOW, P_TP_FLUT, P_TP_SAT,
       P_POST_LO, P_POST_MID, P_POST_HI, NPARAM };

static const char *MODE_NAME[4] = { "SOFT", "HARD", "FOLD", "ASYM" };
static const char *MODE_DESC[4] = { "tanh \x1a warm tube overdrive",
                                    "hard clip \x1a buzzy digital fuzz",
                                    "sine wavefolder \x1a glassy metal",
                                    "asymmetric \x1a fat even harmonics" };
static const char *BAND[3] = { "LO", "MID", "HI" };

static int   cur      = P_CR_BITS;        // cursor across the whole chain (start on the new CRUSH stage)
static int   modeD[2] = { 0, 2 };         // A = SOFT, B = FOLD (soft overdrive INTO a wavefolder)
static float amtD[2]  = { 0.5f, 0.45f };
static float preq[3]  = { 0, 0, 0 };      // pre-EQ  LO/MID/HI dB (-12..12)
static float postq[3] = { 0, 0, 0 };      // post-EQ LO/MID/HI dB
static float crush_p[3] = { 8, 4, 0.35f };  // bits 1..16, rate 1..64, mix 0..1
static float tape_p[3]  = { 0.3f, 0.15f, 0.35f };  // wow, flutter, saturation 0..1
static int   h = -1;                      // the held drone voice (continuous → the scope always has signal)

static int   param_stage(int p) {
    return p <= P_PRE_HI ? ST_PRE : p <= P_DA_AMT ? ST_DRVA : p <= P_DB_AMT ? ST_DRVB
         : p <= P_CR_MIX ? ST_CRUSH : p <= P_TP_SAT ? ST_TAPE : ST_POST;
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// the SAME waveshaper family the engine uses, re-implemented for the transfer
// curve (representative — the scope is truthful). amt 0 = clean bypass.
static float shape(float x, int m, float a) {
    if (a < 0.001f) return x;
    float g = 1.0f + a * 4.0f, v = x * g;
    switch (m) {
        case 0:  return tanhf(v);
        case 1:  return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
        case 2:  return sinf(v);
        default: return x >= 0.0f ? tanhf(v) : tanhf(v * 0.6f);
    }
}

// ── set-and-hold: re-apply an insert ONLY on change (never per frame) ──
static void apply_pre(void)     { eq(preq[0], preq[1], preq[2]); }
static void apply_post(void)    { eq_inst(1, postq[0], postq[1], postq[2]); }
static void apply_crush(void)   { crush(crush_p[0], crush_p[1], crush_p[2]); }
static void apply_tape(void)    { tape(tape_p[0], tape_p[1], tape_p[2]); }
static void apply_drive(int s)  { if (s == 0) drive_insert(amtD[0], modeD[0], 1.0f);
                                  else        drive_insert_inst(1, amtD[1], modeD[1], 1.0f); }
static void set_mode(int s, int m) { modeD[s] = m & 3; apply_drive(s); }
static void set_amt(int s, float a){ amtD[s] = clampf(a, 0, 1); apply_drive(s); }

void init(void) {
    instrument(I_LEAD, INSTR_SAW, 4, 220, 6, 300);
    instrument_filter(I_LEAD, FILTER_DIODE, 1200, 11);   // a resonant acid peak to drive INTO
    static const int chain[6] = { FX_EQ, FX_DRIVE, FX_INST(FX_DRIVE, 1), FX_CRUSH, FX_TAPE, FX_INST(FX_EQ, 1) };
    fx_order(0, chain, 6);                               // PRE-EQ → A → B → CRUSH → TAPE → POST-EQ
    apply_pre(); apply_drive(0); apply_drive(1); apply_crush(); apply_tape(); apply_post();
    h = note_on(48, I_LEAD, 6);
    note_glide(h, 55);
    bpm(128);
}

// ── layout constants (draw + update share them) ──
#define CH_Y   24
#define CH_H   14
static const int BLK_X[NSTAGE] = { 8, 60, 112, 164, 216, 268 };
#define BLK_W  44
#define EQ_ROW0 150
#define EQ_ROWH 13
#define EQ_BX   44
#define EQ_BW   206
#define AMT_X   60
#define AMT_Y   176
#define AMT_W   190

static int drive_of(int stage) { return stage == ST_DRVA ? 0 : 1; }

static void adjust(int dir) {   // dir = +1 (up) / -1 (down)
    switch (cur) {
        case P_PRE_LO:  preq[0]  = clampf(preq[0]  + dir * 0.5f, -12, 12); apply_pre();  break;
        case P_PRE_MID: preq[1]  = clampf(preq[1]  + dir * 0.5f, -12, 12); apply_pre();  break;
        case P_PRE_HI:  preq[2]  = clampf(preq[2]  + dir * 0.5f, -12, 12); apply_pre();  break;
        case P_DA_AMT:  set_amt(0, amtD[0] + dir * 0.02f);                                break;
        case P_DB_AMT:  set_amt(1, amtD[1] + dir * 0.02f);                                break;
        case P_CR_BITS: crush_p[0] = clampf(crush_p[0] + dir * 0.5f, 1, 16); apply_crush(); break;
        case P_CR_RATE: crush_p[1] = clampf(crush_p[1] + dir * 1.0f, 1, 64); apply_crush(); break;
        case P_CR_MIX:  crush_p[2] = clampf(crush_p[2] + dir * 0.02f, 0, 1); apply_crush(); break;
        case P_TP_WOW:  tape_p[0]  = clampf(tape_p[0]  + dir * 0.02f, 0, 1); apply_tape();  break;
        case P_TP_FLUT: tape_p[1]  = clampf(tape_p[1]  + dir * 0.02f, 0, 1); apply_tape();  break;
        case P_TP_SAT:  tape_p[2]  = clampf(tape_p[2]  + dir * 0.02f, 0, 1); apply_tape();  break;
        case P_POST_LO: postq[0] = clampf(postq[0] + dir * 0.5f, -12, 12); apply_post(); break;
        case P_POST_MID:postq[1] = clampf(postq[1] + dir * 0.5f, -12, 12); apply_post(); break;
        case P_POST_HI: postq[2] = clampf(postq[2] + dir * 0.5f, -12, 12); apply_post(); break;
    }
}

void update(void) {
    if (btnp(0, BTN_LEFT))  cur = (cur + NPARAM - 1) % NPARAM;
    if (btnp(0, BTN_RIGHT)) cur = (cur + 1) % NPARAM;
    if (cur == P_DA_MODE || cur == P_DB_MODE) {   // the mode STEPS, one per press
        int s = cur == P_DA_MODE ? 0 : 1;
        if (btnp(0, BTN_UP))   set_mode(s, modeD[s] + 1);
        if (btnp(0, BTN_DOWN)) set_mode(s, modeD[s] + 3);
    } else {                                      // everything else ramps while held
        if (btn(0, BTN_UP))   adjust(+1);
        if (btn(0, BTN_DOWN)) adjust(-1);
    }

    // tap a chain block → jump the cursor to that stage's first param
    static const int FIRST[NSTAGE] = { P_PRE_LO, P_DA_MODE, P_DB_MODE, P_CR_BITS, P_TP_WOW, P_POST_LO };
    for (int s = 0; s < NSTAGE; s++)
        if (tapp(BLK_X[s], CH_Y, BLK_W, CH_H)) cur = FIRST[s];

    // touch: drag the selected stage's controls
    int st = param_stage(cur);
    if (st == ST_DRVA || st == ST_DRVB) {
        int s = drive_of(st);
        if (tapp(10, EQ_ROW0, 120, EQ_ROWH)) set_mode(s, modeD[s] + 1);
        if (tap(AMT_X - 4, AMT_Y - 6, AMT_W + 8, 16)) set_amt(s, (mouse_x() - AMT_X) / (float)AMT_W);
    } else {
        for (int b = 0; b < 3; b++)
            if (tap(EQ_BX - 4, EQ_ROW0 + b * EQ_ROWH - 2, EQ_BW + 8, EQ_ROWH)) {
                float f = clampf((mouse_x() - EQ_BX) / (float)EQ_BW, 0, 1);
                if (st == ST_PRE)       { preq[b]  = (f * 2 - 1) * 12; cur = P_PRE_LO + b;  apply_pre();  }
                else if (st == ST_POST) { postq[b] = (f * 2 - 1) * 12; cur = P_POST_LO + b; apply_post(); }
                else if (st == ST_CRUSH){ crush_p[b] = b == 0 ? 1 + f * 15 : b == 1 ? 1 + f * 63 : f; cur = P_CR_BITS + b; apply_crush(); }
                else                    { tape_p[b] = f; cur = P_TP_WOW + b; apply_tape(); }
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
    for (int px = 0; px < bw; px++) {                    // the COMPOSED drive-cascade curve (B over A)
        float x = (px / (float)bw) * 2.0f - 1.0f;
        float v = shape(shape(x, modeD[0], amtD[0]), modeD[1], amtD[1]);
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
    int start = 0;
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
    rectfill(BLK_X[s], CH_Y, BLK_W, CH_H, active ? CLR_ORANGE : CLR_INDIGO);
    rect(BLK_X[s], CH_Y, BLK_W, CH_H, active ? CLR_WHITE : CLR_DARK_PURPLE);
    print_centered(label, BLK_X[s] + BLK_W / 2, CH_Y + 5, active ? CLR_BLACK : CLR_WHITE);
    if (s < NSTAGE - 1) print(">", BLK_X[s] + BLK_W + 2, CH_Y + 5, CLR_DARK_GREY);
}

static void draw_lslider(const char *lab, float frac, const char *val, int y, int hot) {
    print(lab, 12, y, hot ? CLR_WHITE : CLR_LIGHT_GREY);
    rect(EQ_BX, y, EQ_BW, 7, hot ? CLR_WHITE : CLR_DARK_PURPLE);
    int w = (int)((EQ_BW - 2) * clampf(frac, 0, 1));
    rectfill(EQ_BX + 1, y + 1, w, 5, CLR_ORANGE);
    print(val, EQ_BX + EQ_BW + 6, y, CLR_WHITE);
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

static void draw_drive_edit(int s) {
    int mp = s == 0 ? P_DA_MODE : P_DB_MODE, ap = s == 0 ? P_DA_AMT : P_DB_AMT;
    print(s == 0 ? "DRIVE A" : "DRIVE B", 12, EQ_ROW0 - 12, CLR_MEDIUM_GREY);
    print(MODE_NAME[modeD[s]], 12, EQ_ROW0, cur == mp ? CLR_WHITE : CLR_LIGHT_GREY);
    print(MODE_DESC[modeD[s]], 62, EQ_ROW0, CLR_YELLOW);
    print("drive", 12, AMT_Y, cur == ap ? CLR_WHITE : CLR_LIGHT_GREY);
    rect(AMT_X, AMT_Y, AMT_W, 8, cur == ap ? CLR_WHITE : CLR_DARK_PURPLE);
    rectfill(AMT_X + 1, AMT_Y + 1, (int)((AMT_W - 2) * amtD[s]), 6, CLR_ORANGE);
    char buf[12]; snprintf(buf, sizeof buf, "%d%%", (int)(amtD[s] * 100 + 0.5f));
    print(buf, AMT_X + AMT_W + 6, AMT_Y, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("DISTORTION LAB", 10, 8, CLR_WHITE);
    font(FONT_SMALL);
    print("EQ>A>B>crush>tape>EQ", 170, 11, CLR_MEDIUM_GREY);

    int st = param_stage(cur);
    char la[12], lb[12];
    snprintf(la, sizeof la, "A:%s", MODE_NAME[modeD[0]]);
    snprintf(lb, sizeof lb, "B:%s", MODE_NAME[modeD[1]]);
    draw_block(ST_PRE,   "PRE",   st == ST_PRE);
    draw_block(ST_DRVA,  la,      st == ST_DRVA);
    draw_block(ST_DRVB,  lb,      st == ST_DRVB);
    draw_block(ST_CRUSH, "CRUSH", st == ST_CRUSH);
    draw_block(ST_TAPE,  "TAPE",  st == ST_TAPE);
    draw_block(ST_POST,  "POST",  st == ST_POST);
    font(FONT_NORMAL);

    draw_curve(10, 46, 140, 92);
    draw_scope(170, 46, 140, 92);

    char v[12];
    if (st == ST_DRVA || st == ST_DRVB) {
        draw_drive_edit(drive_of(st));
    } else if (st == ST_CRUSH) {
        print("CRUSH (bit + rate reduction)", 12, EQ_ROW0 - 12, CLR_MEDIUM_GREY);
        snprintf(v, sizeof v, "%d",  (int)(crush_p[0] + 0.5f)); draw_lslider("BITS", (crush_p[0] - 1) / 15.0f, v, EQ_ROW0,               cur == P_CR_BITS);
        snprintf(v, sizeof v, "%d",  (int)(crush_p[1] + 0.5f)); draw_lslider("RATE", (crush_p[1] - 1) / 63.0f, v, EQ_ROW0 + EQ_ROWH,     cur == P_CR_RATE);
        snprintf(v, sizeof v, "%d%%",(int)(crush_p[2] * 100 + 0.5f)); draw_lslider("MIX", crush_p[2],          v, EQ_ROW0 + 2 * EQ_ROWH, cur == P_CR_MIX);
    } else if (st == ST_TAPE) {
        static const char *TL[3] = { "WOW", "FLUT", "SAT" };
        print("TAPE (wow/flutter + saturation)", 12, EQ_ROW0 - 12, CLR_MEDIUM_GREY);
        for (int b = 0; b < 3; b++) { snprintf(v, sizeof v, "%d%%", (int)(tape_p[b] * 100 + 0.5f)); draw_lslider(TL[b], tape_p[b], v, EQ_ROW0 + b * EQ_ROWH, cur == P_TP_WOW + b); }
    } else {
        float *q = st == ST_PRE ? preq : postq;
        int base = st == ST_PRE ? P_PRE_LO : P_POST_LO;
        print(st == ST_PRE ? "PRE-EQ  (shapes what distorts)" : "POST-EQ (tames the fizz)", 12, EQ_ROW0 - 12, CLR_MEDIUM_GREY);
        for (int b = 0; b < 3; b++) draw_eq_slider(BAND[b], q[b], EQ_ROW0 + b * EQ_ROWH, cur == base + b);
    }

    print_centered("\x1b \x1a cursor   up/down adjust   tap a block", SCREEN_W / 2, 193, CLR_LIGHT_GREY);
}
