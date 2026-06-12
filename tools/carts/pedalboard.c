// pedalboard — an electric guitar you PLAY, through a CHAIN of stompboxes you BUILD. The showcase
// for fx_order(): the order pedals sit in the chain is the order the engine runs them, so moving
// a pedal actually changes the tone.
//
// THE PEDALBOARD (top): a horizontal signal chain, left→right. Tap "≡ PEDALS" (top-left) to open
// the PALETTE in the lower half — a tray of every effect, drawn as a little icon + name. Then:
//   • DRAG a palette chip UP into the chain to ADD it.
//   • DRAG a chain pedal by its LABEL sideways to REORDER it (the sound reorders with it).
//   • DRAG a chain pedal DOWN out of the chain to REMOVE it (it returns to the palette).
//   • drag a KNOB to dial it; tap the FOOTSWITCH to toggle on/off; 1..9 toggle by position.
// The chain can outgrow the screen — a scrollbar appears; drag it to pan. (REVERB is in the chain
// like any pedal, but it's a SEND, so its POSITION is cosmetic for now — Step C / multiple reverb
// tanks makes it a real insert. See docs/design/effects-bus-architecture.md.)
//
// THE GUITAR (lower half, when the palette is closed):
//   FRETTING HAND — ROOT row (Z X C V B N M) moves up the neck (E F G A B C D); SHAPE row (A S D
//                   F G) sets the chord shape (5 / maj / min / sus4 / 7).
//   STRUMMING HAND — sweep across the strings over the body (the STRUM zone) to strum; tap a string
//                    on the neck to pick one; SPACE strums. M-row toggles autoplay.
//
// Mouse + touch both work — every contact is its own pointer. The mouse is merged in explicitly.

#include "studio.h"
#include <math.h>

#define I_GTR  5
#define I_MUTE 6      // a choked, muted voice for picking the short nut-side string segment
#define NSTR   6
#define MAXK   4
#define NSHAPE 5
#define NROOT  7

// ── the effect catalog: every pedal you can drag into the chain ──────────────────────────────
// kind = the engine FX_* insert kind (its slot in the reorderable chain), or -1 for a SEND (reverb),
// whose chain position is cosmetic until Step C.
enum { C_BIT, C_EQ, C_CHO, C_PHA, C_FLG, C_TAP, C_TRM, C_WAH, C_RVB, NCAT };
typedef struct {
    const char *name; int body, accent, kind, nk;
    const char *klabel[MAXK]; float kdef[MAXK];
} FxDef;
static const FxDef CAT[NCAT] = {
    { "BITCRUSH", CLR_DARK_BROWN,    CLR_DARK_ORANGE,  FX_CRUSH,   3, { "BIT","RTE","MIX" },   { 0.50f, 0.40f, 0.60f } },
    { "EQ",       CLR_DARKER_BLUE,   CLR_BLUE,         FX_EQ,      3, { "LO","MID","HI" },     { 0.50f, 0.50f, 0.50f } },
    { "CHORUS",   CLR_DARKER_PURPLE, CLR_PINK,         FX_CHORUS,  3, { "RTE","DEP","MIX" },   { 0.40f, 0.55f, 0.55f } },
    { "PHASER",   CLR_DARK_GREEN,    CLR_LIME_GREEN,   FX_PHASER,  4, { "RTE","DEP","FB","MX" },{ 0.30f, 0.70f, 0.65f, 0.55f } },
    { "FLANGER",  CLR_BLUE_GREEN,    CLR_MEDIUM_GREEN, FX_FLANGER, 4, { "RT","DP","FB","MX" }, { 0.20f, 0.70f, 0.60f, 0.50f } },
    { "TAPE",     CLR_DARK_RED,      CLR_PEACH,        FX_TAPE,    3, { "WOW","FLT","SAT" },   { 0.35f, 0.25f, 0.45f } },
    { "TREMOLO",  CLR_DARKER_GREY,   CLR_LIGHT_YELLOW, FX_TREM,    3, { "SPD","DEP","WAV" },   { 0.45f, 0.60f, 0.0f } },
    { "WAH",      CLR_DARK_PURPLE,   CLR_MAUVE,        FX_WAH,     3, { "SNS","RES","MIX" },   { 0.50f, 0.55f, 0.70f } },
    { "REVERB",   CLR_DARK_BLUE,     CLR_INDIGO,       -1,         3, { "SIZ","DMP","MIX" },   { 0.70f, 0.40f, 0.45f } },
};

// ── the chain: an ordered list of DISTINCT catalog ids, each with its own knobs + on-state ──
typedef struct { int cat; float k[MAXK]; bool on; } Slot;
static Slot  chain[NCAT];
static int   chain_n   = 0;
static float scroll_x  = 0.0f;     // horizontal pan of the chain view
static bool  palette_open = false;
static int   dirty     = 1;

// ── the fretting hand: real guitar tab ──  standard tuning, E-shape MOVEABLE chords.
static const int OPEN[NSTR] = { 40, 45, 50, 55, 59, 64 };   // E A D G B E (low→high)
static const int SHAPE_F[NSHAPE][NSTR] = {
    { 0, 2, 2, -1, -1, -1 },   // 5    power — finger root/5th/octave; high strings ring open
    { 0, 2, 2,  1,  0,  0 },   // maj  E-shape major
    { 0, 2, 2,  0,  0,  0 },   // min  E-shape minor
    { 0, 2, 2,  2,  0,  0 },   // sus4 suspended fourth
    { 0, 2, 0,  1,  0,  0 },   // 7    E-shape dominant 7
};
static const char *SHAPE_NAME[NSHAPE] = { "5", "maj", "min", "sus4", "7" };
static const char  SHAPE_KEY[NSHAPE]  = { 'A', 'S', 'D', 'F', 'G' };
static const int   ROOT_FRET[NROOT]   = { 0, 1, 3, 5, 7, 8, 10 };         // barre fret for E F G A B C D
static const char *ROOT_NAME[NROOT]   = { "E", "F", "G", "A", "B", "C", "D" };
static const char  ROOT_KEY[NROOT]    = { 'Z', 'X', 'C', 'V', 'B', 'N', 'M' };
#define FRET_W 7
static int  sel_shape = 0;
static int  sel_root  = 0;
static int  str_midi[NSTR];

static float amp[NSTR];
static float vib_ph[NSTR];
static int   pend[NSTR];

static bool  autoplay = true;
static int   apos = 0;

// ── geometry ──
#define PED_Y 14
#define PED_H 72
#define PED_W 48
#define PITCH 52
#define CHAIN_X0 4
#define VIEW_W  312                  // chain viewport: x 4..316
#define VIEW_R  (CHAIN_X0 + VIEW_W)
#define SB_Y    (PED_Y + PED_H)      // scrollbar track (only drawn on overflow)
#define KNOB_CY (PED_Y + 44)
#define ILLU_CY (PED_Y + 22)
#define PAL_Y   88                   // palette panel top (when open)
#define SX0   22                     // nut (neck end)
#define SX1   302                    // bridge (body end)
#define STRUMX 196                   // strum zone starts here (right side, over the body)
#define STR_Y0 96
#define STR_DY 7
#define STR_Y(s) (STR_Y0 + (s) * STR_DY)
#define SHAPE_Y 143
#define SHAPE_W 56
#define SHAPE_X(i) (12 + (i) * 60)
#define ROOT_Y  159
#define ROOT_W  40
#define ROOT_X(i) (11 + (i) * 43)
#define ROW_H 14

static int knob_cx_at(int px, int j, int nk) { return px + 4 + (PED_W - 8) * (2 * j + 1) / (2 * nk); }
static int knob_slot_w(int nk)                { return (PED_W - 8) / nk; }
static int gate_ms(void) { return 1800; }

static int str_fret(int s) { return SHAPE_F[sel_shape][s] < 0 ? 0 : ROOT_FRET[sel_root] + SHAPE_F[sel_shape][s]; }
static void build_strings(void) {
    for (int s = 0; s < NSTR; s++) str_midi[s] = OPEN[s] + str_fret(s);
}

// ── chain helpers ──
static int  chain_index(int cat) { for (int i = 0; i < chain_n; i++) if (chain[i].cat == cat) return i; return -1; }
static int  content_w(void)      { return chain_n * PITCH; }
static float max_scroll(void)    { float m = (float)(content_w() - VIEW_W); return m < 0 ? 0 : m; }
static void clamp_scroll(void)   { float m = max_scroll(); if (scroll_x < 0) scroll_x = 0; if (scroll_x > m) scroll_x = m; }
static void chain_insert(int cat, int at) {
    if (chain_n >= NCAT || chain_index(cat) >= 0) return;
    if (at < 0) at = 0; if (at > chain_n) at = chain_n;
    for (int i = chain_n; i > at; i--) chain[i] = chain[i - 1];
    chain[at].cat = cat; chain[at].on = true;
    for (int j = 0; j < MAXK; j++) chain[at].k[j] = CAT[cat].kdef[j];
    chain_n++;
}
static void chain_remove(int idx) {
    if (idx < 0 || idx >= chain_n) return;
    for (int i = idx; i < chain_n - 1; i++) chain[i] = chain[i + 1];
    chain_n--;
}
static void chain_move(int from, int to) {
    if (from < 0 || from >= chain_n) return;
    Slot s = chain[from];
    for (int i = from; i < chain_n - 1; i++) chain[i] = chain[i + 1];
    chain_n--;
    if (to > from) to--;
    if (to < 0) to = 0; if (to > chain_n) to = chain_n;
    for (int i = chain_n; i > to; i--) chain[i] = chain[i - 1];
    chain[to] = s; chain_n++;
}

// push every effect's state to the engine, then set the INSERT ORDER from the chain order.
// An effect not in the chain (or off) is pushed dry; reverb (a send) is driven by its on-state,
// its chain POSITION ignored (cosmetic until Step C).
static void apply_fx(void) {
    for (int c = 0; c < NCAT; c++) {
        int idx = chain_index(c);
        bool act = (idx >= 0) && chain[idx].on;
        const float *k = (idx >= 0) ? chain[idx].k : CAT[c].kdef;
        switch (c) {
            case C_BIT: crush(16.0f - k[0] * 14.0f, 1.0f + k[1] * 15.0f, act ? k[2] : 0.0f); break;
            case C_EQ:  if (act) eq((k[0]-0.5f)*24.0f, (k[1]-0.5f)*24.0f, (k[2]-0.5f)*24.0f); else eq(0.0f, 0.0f, 0.0f); break;
            case C_CHO: chorus(0.1f + k[0] * 4.9f, k[1], act ? k[2] : 0.0f); break;
            case C_PHA: phaser(0.1f + k[0] * 9.9f, k[1], (k[2]-0.5f) * 1.8f, act ? k[3] : 0.0f, 4); break;
            case C_FLG: flanger(0.05f + k[0] * 4.95f, k[1], k[2] * 0.95f, act ? k[3] : 0.0f); break;
            case C_TAP: tape(act ? k[0] : 0.0f, act ? k[1] : 0.0f, act ? k[2] : 0.0f); break;
            case C_TRM: tremolo(0.5f + k[0] * 11.5f, act ? k[1] : 0.0f, (int)(k[2] * 2.99f)); break;
            case C_WAH: wah(k[0], k[1], act ? k[2] : 0.0f); break;
            case C_RVB: reverb(0.2f + k[0] * 0.78f, k[1]); instrument_reverb(I_GTR, act ? k[2] : 0.0f); break;
        }
    }
    int kinds[NCAT], n = 0;
    for (int i = 0; i < chain_n; i++) { int kd = CAT[chain[i].cat].kind; if (kd >= 0) kinds[n++] = kd; }
    fx_order(0, kinds, n);   // the chain order IS the insert order (Increment A)
}

void init(void) {
    instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 1200);
    instrument_harmonics(I_GTR, 0.55f);
    instrument_timbre(I_GTR, 0.85f);
    instrument_morph(I_GTR, 0.15f);
    instrument_drive_mode(I_GTR, DRIVE_SOFT);
    instrument_drive(I_GTR, 0.18f);
    instrument(I_MUTE, INSTR_GUITAR, 1, 0, 2, 180);
    instrument_harmonics(I_MUTE, 0.5f);
    instrument_timbre(I_MUTE, 0.95f);
    instrument_morph(I_MUTE, 0.92f);
    build_strings();
    bpm(100);
    // a tasteful starting chain (CHORUS + TREMOLO ringing); the rest wait in the palette
    chain_insert(C_BIT, 0); chain[0].on = false;
    chain_insert(C_EQ, 1);  chain[1].on = false;
    chain_insert(C_CHO, 2);
    chain_insert(C_TRM, 3);
    chain_insert(C_RVB, 4); chain[4].on = false;
    apply_fx();
    amp[0] = 0.8f; amp[2] = 1.0f; amp[4] = 0.6f;
}

static void pluck_str(int s, int vol) {
    if (s < 0 || s >= NSTR) return;
    hit(str_midi[s], I_GTR, vol, gate_ms());
    amp[s] = 1.0f; vib_ph[s] = 0.0f;
}
static void strum_down(void) {
    for (int s = 0; s < NSTR; s++) {
        schedule_hit(s * 28, str_midi[s], I_GTR, 5, gate_ms());
        pend[s] = 1 + (s * 28 * 60) / 1000;
    }
}
static void set_shape(int sh) { sel_shape = sh; build_strings(); autoplay = false; }
static void set_root(int r)   { sel_root  = r;  build_strings(); autoplay = false; }
static int  near_string(int ty) { int s = (ty - STR_Y0 + STR_DY / 2) / STR_DY; return s < 0 ? 0 : s >= NSTR ? NSTR - 1 : s; }
static int dot_x(int s) {
    int f = str_fret(s);
    if (f == 0) return SX0 + 2;
    int dx = SX0 + 6 + f * FRET_W;
    return dx > STRUMX - 16 ? STRUMX - 16 : dx;
}
static void pick_string(int s, int px) {
    if (s < 0 || s >= NSTR) return;
    int f = str_fret(s);
    if (f > 0 && px < dot_x(s) - 1) {
        float seg = 1.0f - powf(2.0f, -f / 12.0f);
        int hi = (str_midi[s] - f) + (int)(12.0f * log2f(1.0f / seg) + 0.5f);
        if (hi > 103) hi = 103;
        hit(hi, I_MUTE, 4, 130);
        amp[s] = 0.7f; vib_ph[s] = 0.0f;
    } else {
        pluck_str(s, 6);
    }
}

// ── per-contact pointer pool ──
#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_KNOB, PTR_PICK, PTR_DRAGSLOT, PTR_DRAGPAL, PTR_SCROLL };
typedef struct { int id, mode, slot, knob, cat, prevY, x, y; } Ptr;
static Ptr ptr[NPTR];

// screen x of chain pedal i (may be off-screen); the slot under a point, or -1
static int ped_screen_x(int i) { return CHAIN_X0 + i * PITCH - (int)scroll_x; }
static int slot_under(int tx) {
    int i = (tx + (int)scroll_x - CHAIN_X0) / PITCH;
    if (i < 0 || i >= chain_n) return -1;
    int px = ped_screen_x(i);
    return (tx >= px && tx < px + PED_W) ? i : -1;
}

// build the list of palette-available cats (not in the chain) + the chip rect for the a-th of them
#define PAL_COLS 4
#define PAL_CW   72
#define PAL_CH   26
static int pal_avail(int *out) { int n = 0; for (int c = 0; c < NCAT; c++) if (chain_index(c) < 0) out[n++] = c; return n; }
static void pal_chip_rect(int a, int *x, int *y) { *x = 6 + (a % PAL_COLS) * 78; *y = PAL_Y + 16 + (a / PAL_COLS) * (PAL_CH + 2); }

static void commit_drop(Ptr *p) {
    if (p->mode == PTR_DRAGSLOT) {
        int idx = chain_index(p->cat);
        if (idx < 0) return;
        if (p->y >= PED_Y + PED_H) { chain_remove(idx); }          // dropped below the chain → remove
        else {                                                     // dropped in the chain → reorder
            int tgt = (p->x + (int)scroll_x - CHAIN_X0 + PITCH / 2) / PITCH;
            if (tgt < 0) tgt = 0; if (tgt > chain_n - 1) tgt = chain_n - 1;
            chain_move(idx, tgt);
        }
        clamp_scroll(); dirty = 1;
    } else if (p->mode == PTR_DRAGPAL) {
        if (p->y < PED_Y + PED_H && chain_index(p->cat) < 0 && chain_n < NCAT) {   // dropped into the chain → add
            int tgt = (p->x + (int)scroll_x - CHAIN_X0 + PITCH / 2) / PITCH;
            chain_insert(p->cat, tgt);
            clamp_scroll(); dirty = 1;
        }
    }
}

void update(void) {
    for (int i = 0; i < NSHAPE; i++) if (keyp(SHAPE_KEY[i])) set_shape(i);
    for (int i = 0; i < NROOT;  i++) if (keyp(ROOT_KEY[i]))  set_root(i);
    for (int i = 0; i < chain_n; i++) if (keyp('1' + i)) { chain[i].on = !chain[i].on; dirty = 1; }
    if (keyp(KEY_SPACE)) { strum_down(); autoplay = false; }

    if (tapp(4, 2, 56, 11))           palette_open = !palette_open;
    if (tapp(SCREEN_W - 70, 4, 66, 10)) autoplay = !autoplay;

    bool overflow = max_scroll() > 0;

    int cid[NPTR], cxp[NPTR], cyp[NPTR], nc = 0;
    for (int i = 0; i < touch_count() && nc < NPTR; i++) { cid[nc] = touch_id(i); cxp[nc] = touch_x(i); cyp[nc] = touch_y(i); nc++; }
    if (mouse_down(MOUSE_LEFT) && nc < NPTR) {
        int mxp = mouse_x(), myp = mouse_y(), dup = 0;
        for (int i = 0; i < nc; i++) { int dx = cxp[i] - mxp, dy = cyp[i] - myp; if (dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2) dup = 1; }
        if (!dup) { cid[nc] = -100; cxp[nc] = mxp; cyp[nc] = myp; nc++; }
    }

    for (int i = 0; i < nc; i++) {
        int id = cid[i], tx = cxp[i], ty = cyp[i];
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1, -1, -1, ty, tx, ty };

            // 1. the chain (always live, palette open or not)
            if (ty >= PED_Y && ty < PED_Y + PED_H) {
                int s = slot_under(tx);
                if (s >= 0) {
                    int px = ped_screen_x(s); int nk = CAT[chain[s].cat].nk;
                    if (point_in_box(tx, ty, px + 8, PED_Y + 57, PED_W - 16, 14)) { chain[s].on = !chain[s].on; dirty = 1; }
                    else if (point_in_box(tx, ty, px, KNOB_CY - 11, PED_W, 22)) {
                        int sw = knob_slot_w(nk);
                        for (int j = 0; j < nk; j++)
                            if (tx >= knob_cx_at(px, j, nk) - sw / 2 && tx < knob_cx_at(px, j, nk) + sw / 2) { p->mode = PTR_KNOB; p->slot = s; p->knob = j; }
                    } else { p->mode = PTR_DRAGSLOT; p->slot = s; p->cat = chain[s].cat; }   // label = drag handle
                }
            }
            // 2. scrollbar
            if (p->mode == PTR_IDLE && overflow && ty >= SB_Y && ty <= SB_Y + 6) p->mode = PTR_SCROLL;
            // 3. palette chips (only when open)
            if (p->mode == PTR_IDLE && palette_open && ty >= PAL_Y) {
                int avail[NCAT], na = pal_avail(avail);
                for (int a = 0; a < na; a++) { int cx2, cy2; pal_chip_rect(a, &cx2, &cy2);
                    if (point_in_box(tx, ty, cx2, cy2, PAL_CW, PAL_CH)) { p->mode = PTR_DRAGPAL; p->cat = avail[a]; } }
            }
            // 4. the guitar (only when palette closed)
            if (p->mode == PTR_IDLE && !palette_open) {
                for (int i2 = 0; i2 < NSHAPE; i2++) if (point_in_box(tx, ty, SHAPE_X(i2), SHAPE_Y, SHAPE_W, ROW_H)) set_shape(i2);
                if (p->mode == PTR_IDLE)
                    for (int i2 = 0; i2 < NROOT; i2++) if (point_in_box(tx, ty, ROOT_X(i2), ROOT_Y, ROOT_W, ROW_H)) set_root(i2);
                if (p->mode == PTR_IDLE && ty >= STR_Y0 - 9 && ty <= STR_Y(NSTR - 1) + 9 && tx >= SX0 - 8 && tx <= SX1 + 8) {
                    p->mode = PTR_PICK; autoplay = false;
                    pick_string(near_string(ty), tx);
                    p->prevY = ty;
                }
            }
        } else if (p->mode == PTR_KNOB) {
            if (p->slot < chain_n) { chain[p->slot].k[p->knob] = clamp(chain[p->slot].k[p->knob] + (p->prevY - ty) * 0.012f, 0.0f, 1.0f); dirty = 1; }
            p->prevY = ty;
        } else if (p->mode == PTR_PICK) {
            for (int s = 0; s < NSTR; s++) { int ys = STR_Y(s); if ((p->prevY < ys && ty >= ys) || (p->prevY > ys && ty <= ys)) pick_string(s, tx); }
            p->prevY = ty;
        } else if (p->mode == PTR_SCROLL) {
            scroll_x += (tx - p->x) * (content_w() / (float)VIEW_W); clamp_scroll();
        }
        p->x = tx; p->y = ty;
    }
    for (int j = 0; j < NPTR; j++) {
        if (ptr[j].id == NOID) continue;
        int present = 0;
        for (int i = 0; i < nc; i++) if (cid[i] == ptr[j].id) { present = 1; break; }
        if (!present) { commit_drop(&ptr[j]); ptr[j].id = NOID; }
    }

    if (dirty) { apply_fx(); dirty = 0; }

    if (autoplay && every(1)) {
        static const int prog[8] = { 0, 2, 6, 3, 0, 5, 3, 2 };
        if (beat() % 4 == 0) { sel_shape = 0; sel_root = prog[apos % 8]; build_strings(); strum_down(); apos++; }
    }
    for (int s = 0; s < NSTR; s++) if (pend[s] > 0 && --pend[s] == 0) { amp[s] = 1.0f; vib_ph[s] = 0.0f; }

#ifdef DE_TRACE
    watch("chain_n", "%d", chain_n); watch("pal", "%d", palette_open);
#endif
}

// the little face graphic for an effect, centered at (cx,cy) — reused by the chain pedal AND the
// (smaller) palette chip.
static void draw_illu(int cat, int cx, int cy, int col, int bg) {
    if (cat == C_BIT) {                                 // 8-bit critter
        int ix = cx - 6, iy = cy - 5;
        rectfill(ix + 2, iy, 8, 2, col); rectfill(ix, iy + 2, 12, 4, col);
        rectfill(ix, iy + 6, 3, 3, col); rectfill(ix + 5, iy + 6, 2, 2, col); rectfill(ix + 9, iy + 6, 3, 3, col);
        rectfill(ix + 3, iy + 3, 2, 2, bg); rectfill(ix + 7, iy + 3, 2, 2, bg);
    } else if (cat == C_EQ) {                            // equalizer spectrum
        static const int hh[5] = { 5, 11, 7, 13, 8 };
        for (int i = 0; i < 5; i++) rectfill(cx - 10 + i * 5, cy + 6 - hh[i], 3, hh[i], col);
    } else if (cat == C_CHO) {                            // shimmer waves (two offset)
        for (int o = 0; o < 2; o++) {
            int base = cy + (o ? 3 : -3), px = cx - 15, py = base;
            for (int xx = cx - 15; xx <= cx + 15; xx += 2) { int wy = base + (int)(sinf((xx - cx) * 0.42f + o * 1.6f) * 3.0f); line(px, py, xx, wy, col); px = xx; py = wy; }
        }
    } else if (cat == C_PHA) {                            // phaser — one slow swirl + a notch dot
        int px = cx - 14, py = cy;
        for (int xx = cx - 14; xx <= cx + 14; xx++) { int wy = cy + (int)(sinf((xx - cx) * 0.26f) * 5.0f); line(px, py, xx, wy, col); px = xx; py = wy; }
        circfill(cx, cy, 1, bg);
    } else if (cat == C_FLG) {                            // a jet
        int jx = cx + 7, jy = cy;
        trifill(jx - 14, jy - 2, jx - 14, jy + 2, jx, jy, col);
        trifill(jx - 10, jy, jx - 15, jy - 6, jx - 5, jy, col);
        trifill(jx - 10, jy, jx - 15, jy + 6, jx - 5, jy, col);
        for (int i = 0; i < 3; i++) line(cx - 16, jy - 3 + i * 3, cx - 9, jy - 3 + i * 3, col);
    } else if (cat == C_TAP) {                            // two tape reels
        circ(cx - 7, cy, 4, col); circfill(cx - 7, cy, 1, col);
        circ(cx + 7, cy, 4, col); circfill(cx + 7, cy, 1, col);
        line(cx - 7, cy - 5, cx + 7, cy - 5, col); line(cx - 7, cy + 5, cx + 7, cy + 5, col);
    } else if (cat == C_TRM) {                            // tremolo — carrier sine inside an AM bulge
        int px = cx - 15, py = cy;
        for (int xx = cx - 15; xx <= cx + 15; xx++) {
            float u = (float)(xx - (cx - 15)) / 30.0f; float env = sinf(u * 3.14159f);
            int wy = cy + (int)(sinf((xx - cx) * 1.1f) * env * 6.0f); line(px, py, xx, wy, col); px = xx; py = wy;
        }
    } else if (cat == C_WAH) {                            // wah — a resonant bandpass peak
        line(cx - 12, cy + 5, cx - 2, cy - 6, col); line(cx - 2, cy - 6, cx + 2, cy - 6, col);
        line(cx + 2, cy - 6, cx + 12, cy + 5, col); line(cx - 13, cy + 5, cx + 13, cy + 5, col);
    } else {                                             // REVERB — expanding rings (the bloom)
        for (int i = 1; i <= 3; i++) circ(cx, cy, i * 3, col);
        pset(cx, cy, col);
    }
}

static void draw_chain_pedal(int i, int x) {
    Slot *sl = &chain[i]; const FxDef *d = &CAT[sl->cat];
    int cx = x + PED_W / 2;
    rrectfill(x, PED_Y, PED_W, PED_H, 4, d->body);
    rrect(x, PED_Y, PED_W, PED_H, 4, sl->on ? d->accent : CLR_DARKER_GREY);
    font(FONT_SMALL);
    print_centered(d->name, cx, PED_Y + 3, sl->on ? CLR_WHITE : CLR_MEDIUM_GREY);
    draw_illu(sl->cat, cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY, d->body);
    int kr = d->nk >= 4 ? 4 : 5;
    for (int j = 0; j < d->nk; j++) {
        int kx = knob_cx_at(x, j, d->nk);
        circfill(kx, KNOB_CY, kr, CLR_BROWNISH_BLACK);
        circ(kx, KNOB_CY, kr, sl->on ? d->accent : CLR_DARK_GREY);
        float a = (-135.0f + sl->k[j] * 270.0f) * 0.0174533f;
        line(kx, KNOB_CY, kx + (int)(sinf(a) * (kr - 1)), KNOB_CY - (int)(cosf(a) * (kr - 1)), sl->on ? CLR_WHITE : CLR_MEDIUM_GREY);
        font(FONT_TINY);
        print_centered(d->klabel[j], kx, KNOB_CY + kr + 1, sl->on ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
    circfill(x + 7, PED_Y + 63, 2, sl->on ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    rrectfill(x + 12, PED_Y + 58, PED_W - 20, 12, 2, CLR_BROWNISH_BLACK);
    rrect(x + 12, PED_Y + 58, PED_W - 20, 12, 2, CLR_DARK_GREY);
}

// a small palette chip / drag-ghost: the icon + a name, no knobs.
static void draw_chip(int cat, int x, int y, int w, int h, bool ghost) {
    const FxDef *d = &CAT[cat];
    rrectfill(x, y, w, h, 3, d->body);
    rrect(x, y, w, h, 3, ghost ? CLR_WHITE : d->accent);
    draw_illu(cat, x + w / 2, y + 9, d->accent, d->body);
    font(FONT_TINY); print_centered(d->name, x + w / 2, y + h - 6, CLR_WHITE); font(FONT_NORMAL);
}

static void draw_palette(void) {
    rectfill(0, PAL_Y, SCREEN_W, SCREEN_H - PAL_Y, CLR_BROWNISH_BLACK);
    line(0, PAL_Y, SCREEN_W, PAL_Y, CLR_DARK_GREY);
    font(FONT_TINY);
    print_centered("drag a pedal UP into the chain  ·  drag a chain pedal DOWN here to remove", SCREEN_W / 2, PAL_Y + 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    int avail[NCAT], na = pal_avail(avail);
    for (int a = 0; a < na; a++) { int cx2, cy2; pal_chip_rect(a, &cx2, &cy2); draw_chip(avail[a], cx2, cy2, PAL_CW, PAL_CH, false); }
}

static void draw_guitar(void) {
    int by = STR_Y0 - 8, bh = (STR_Y(NSTR - 1) + 8) - by;
    rrectfill(6, by, SCREEN_W - 12, bh, 6, CLR_BLUE_GREEN);
    rrect(6, by, SCREEN_W - 12, bh, 6, CLR_BLUE);
    rrectfill(SX0 - 8, by + 3, SX1 - SX0 + 28, bh - 6, 4, CLR_LIGHT_PEACH);
    rectfill(STRUMX, by + 3, SX1 - STRUMX + 4, bh - 6, CLR_PEACH);
    rectfill(SX0 + 60, by + 3, 5, bh - 6, CLR_DARKER_GREY);
    rectfill(STRUMX - 8, by + 3, 5, bh - 6, CLR_DARKER_GREY);
    rectfill(SX0 - 4, by + 2, 3, bh - 4, CLR_MEDIUM_GREY);
    for (int s = 0; s < NSTR; s++) {
        int y = STR_Y(s);
        amp[s] *= 0.93f; vib_ph[s] += 0.6f;
        int col = amp[s] > 0.5f ? CLR_LIGHT_YELLOW : amp[s] > 0.1f ? CLR_DARK_ORANGE : CLR_MEDIUM_GREY;
        int px = SX0, py = y;
        for (int xx = SX0 + 8; xx <= SX1; xx += 8) {
            float t  = (float)(xx - SX0) / (float)(SX1 - SX0);
            int   wy = y + (int)(amp[s] * 4.0f * sinf(t * 9.42f + vib_ph[s]) * sinf(t * 3.14f));
            line(px, py, xx, wy, col); px = xx; py = wy;
        }
    }
    for (int s = 0; s < NSTR; s++) {
        int f = str_fret(s), y = STR_Y(s);
        if (f == 0) { circ(SX0 + 2, y, 2, CLR_DARK_RED); }
        else { int dx = SX0 + 6 + f * FRET_W; if (dx > STRUMX - 16) dx = STRUMX - 16; circfill(dx, y, 2, CLR_DARK_RED); pset(dx - 1, y - 1, CLR_PEACH); }
    }
    font(FONT_TINY); print_centered("STRUM", (STRUMX + SX1) / 2, by + bh - 7, CLR_DARK_BROWN); font(FONT_NORMAL);
    for (int j = 0; j < NPTR; j++)
        if (ptr[j].id != NOID && ptr[j].mode == PTR_PICK)
            trifill(ptr[j].x - 3, ptr[j].y - 4, ptr[j].x + 3, ptr[j].y - 4, ptr[j].x, ptr[j].y + 4, CLR_WHITE);
    for (int i = 0; i < NSHAPE; i++) {
        int x = SHAPE_X(i); bool on = (i == sel_shape);
        rrectfill(x, SHAPE_Y, SHAPE_W, ROW_H, 3, on ? CLR_ORANGE : CLR_DARKER_GREY);
        rrect(x, SHAPE_Y, SHAPE_W, ROW_H, 3, on ? CLR_WHITE : CLR_DARK_GREY);
        print_centered(SHAPE_NAME[i], x + SHAPE_W / 2, SHAPE_Y + 4, on ? CLR_BLACK : CLR_MEDIUM_GREY);
        font(FONT_TINY); print(str("%c", SHAPE_KEY[i]), x + 2, SHAPE_Y + 1, on ? CLR_BLACK : CLR_DARK_GREY); font(FONT_NORMAL);
    }
    for (int i = 0; i < NROOT; i++) {
        int x = ROOT_X(i); bool on = (i == sel_root);
        rrectfill(x, ROOT_Y, ROOT_W, ROW_H, 3, on ? CLR_LIME_GREEN : CLR_DARKER_GREY);
        rrect(x, ROOT_Y, ROOT_W, ROW_H, 3, on ? CLR_WHITE : CLR_DARK_GREY);
        print_centered(ROOT_NAME[i], x + ROOT_W / 2, ROOT_Y + 4, on ? CLR_BLACK : CLR_MEDIUM_GREY);
        font(FONT_TINY); print(str("%c", ROOT_KEY[i]), x + 2, ROOT_Y + 1, on ? CLR_BLACK : CLR_DARK_GREY); font(FONT_NORMAL);
    }
    font(FONT_TINY);
    print_centered("ZXCVBNM = neck   ASDFG = shape   strum the body = chord, near the nut = high ghosts", SCREEN_W / 2, ROOT_Y + ROW_H + 2, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // top bar — palette toggle (left), AUTO (right)
    rrectfill(4, 2, 56, 11, 2, palette_open ? CLR_INDIGO : CLR_DARKER_GREY);
    rrect(4, 2, 56, 11, 2, palette_open ? CLR_WHITE : CLR_DARK_GREY);
    font(FONT_SMALL); print(palette_open ? "x CLOSE" : "= PEDALS", 9, 4, CLR_WHITE);
    print_right(autoplay ? "AUTO: on" : "AUTO: off", SCREEN_W - 6, 5, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // the chain (clipped to its viewport so half-scrolled pedals don't bleed over the bars)
    clip(CHAIN_X0, PED_Y, VIEW_W, PED_H);
    for (int i = 0; i < chain_n; i++) {
        int x = ped_screen_x(i);
        if (x > VIEW_R || x + PED_W < CHAIN_X0) continue;
        draw_chain_pedal(i, x);
        if (i < chain_n - 1) print(">", x + PED_W, KNOB_CY - 3, CLR_DARK_GREY);
    }
    clip(0, 0, 0, 0);
    if (chain_n == 0) { font(FONT_SMALL); print_centered("open PEDALS, drag effects in →", SCREEN_W / 2, PED_Y + 32, CLR_DARK_GREY); font(FONT_NORMAL); }

    // scrollbar (only on overflow)
    if (max_scroll() > 0) {
        rectfill(CHAIN_X0, SB_Y + 1, VIEW_W, 3, CLR_DARKER_GREY);
        int tw = VIEW_W * VIEW_W / content_w();
        int tx = CHAIN_X0 + (int)(scroll_x / max_scroll() * (VIEW_W - tw));
        rectfill(tx, SB_Y + 1, tw, 3, CLR_LIGHT_GREY);
    }

    if (palette_open) draw_palette();
    else              draw_guitar();

    // drag ghost (on top of everything)
    for (int j = 0; j < NPTR; j++)
        if (ptr[j].id != NOID && (ptr[j].mode == PTR_DRAGSLOT || ptr[j].mode == PTR_DRAGPAL))
            draw_chip(ptr[j].cat, ptr[j].x - 22, ptr[j].y - 13, 44, 26, true);
}
