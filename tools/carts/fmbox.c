/* de:meta
{
  "title": "fm box",
  "status": "active",
  "created": "2026-07-10",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "step-sequencer",
    "fm-synth",
    "drum-synthesis"
  ],
  "lineage": "The Elektron Model:Cycles reimagined — an all-FM percussion groovebox. Chassis from drummachine.c (grid + beat() playhead + pointer paint); voices are six INSTR_FM machines instead of the analog kit. Adds the thing no cart has: per-step PARAMETER LOCKS. Design: docs/design/fmbox-blind-brief.md.",
  "description": {
    "summary": "An all-FM drum box: six machines on ONE FM engine, plus per-step parameter locks.",
    "detail": "Every 808/909/CR-78 makes analog drums; this makes METALLIC, glassy FM drums — six 'machines' (KICK/SNARE/METAL/PERC/TONE/CHORD) that are all the same 2-op FM engine reconfigured by six macro knobs (PIT pitch, DEC decay, COL colour/brightness, SHP shape/ratio, SWP pitch-sweep, CON contour/feedback). Turn SHP and one machine walks from wood to glass to metal. The headline is the p-LOCK: hold a lit step and drag up to lock the SELECTED macro to a value for THAT step only — a snare that brightens across the bar, a tone that walks in pitch — the loop evolves without you touching it. Locked steps show the value as a fill-height bar inside the cell (the height IS the locked value); fling a cell down to clear its lock.",
    "controls": "TAP a cell = toggle. HOLD a lit cell + DRAG UP = p-lock the selected macro to that step (fling down to clear). Tap a machine name to select it; tap a knob (or arrows UP/DOWN) to select a macro, DRAG the knob to set the machine's base value. WASD move cursor, Z toggle, X clear grid. Arrows LEFT/RIGHT tempo. BPM buttons top-right."
  }
}
de:meta */
#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND

// FM BOX — the Elektron Model:Cycles reimagined as a dreamengine cart.
//
// One idea, two layers. LAYER 1 is a plain step grid (drummachine's chassis):
// six rows, sixteen columns, a playhead sweeping off beat(); a lit cell fires
// that row's drum. LAYER 2 is what makes it a Cycles and not just "drummachine
// with FM voices":
//
//   * every row is the SAME two-op FM engine (INSTR_FM), reconfigured into a
//     different "machine" by six macro knobs. Kick, metal and chord are the
//     same oscillator pair — only the macros differ. That unity is the toy.
//   * PARAMETER LOCKS: any step can carry its own macro value. Hold a lit cell
//     and drag up to lock the *selected* macro just for that step. The loop
//     then evolves on its own — a snare that opens across the bar, a tone that
//     walks. The lock reads as a fill-height bar inside the cell.
//
// The six macros map straight onto our FM engine's controls:
//   PIT pitch (base note)     DEC decay (gate length)    COL colour  = timbre
//   SHP shape (ratio) = harmonics   SWP sweep = pitch-env   CON contour = morph
//
// A p-lock is applied the honest way: right before a step fires, the slot's
// macros are set to that step's locked values (or the machine's base). That's a
// per-STEP write, never per-frame, so it's not the effects-frame trap.

#define ROWS  6
#define STEPS 16
#define NMAC  6         // PIT DEC COL SHP SWP CON
#define SLOT0 5         // FM machines live in slots 5..10
#define SNOISE 11       // the snare's noise crack layer

// macro indices — named per CLAUDE.md's data-driven rule (never raw numbers)
enum { K_PIT, K_DEC, K_COL, K_SHP, K_SWP, K_CON };
enum { M_KICK, M_SNARE, M_METAL, M_PERC, M_TONE, M_CHORD };

static const char *LABEL[ROWS] = { "KICK", "SNARE", "METAL", "PERC", "TONE", "CHORD" };
static const char *MNAME[NMAC] = { "PITCH", "DECAY", "COLOR", "SHAPE", "SWEEP", "CONTOUR" };
static const int   LIT  [ROWS] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_INDIGO };

// the note range each machine's PIT macro sweeps (lo..hi MIDI)
static const int MIDI_LO[ROWS] = { 28, 48, 60, 55, 33, 48 };
static const int MIDI_HI[ROWS] = { 46, 64, 84, 79, 57, 67 };
// amp release per machine (ms) — METAL rings, the rest are tight
static const int REL[ROWS]     = { 60, 50, 320, 80, 140, 180 };

// base macro values per machine (0..1) — the electro-FM kit you hear on load
static float mac[ROWS][NMAC] = {
    /* KICK  */ { 0.20f, 0.35f, 0.15f, 0.02f, 0.75f, 0.10f },
    /* SNARE */ { 0.45f, 0.22f, 0.80f, 0.45f, 0.15f, 0.55f },
    /* METAL */ { 0.50f, 0.70f, 0.85f, 0.62f, 0.00f, 0.20f },
    /* PERC  */ { 0.55f, 0.25f, 0.55f, 0.40f, 0.35f, 0.10f },
    /* TONE  */ { 0.25f, 0.45f, 0.45f, 0.15f, 0.10f, 0.20f },
    /* CHORD */ { 0.40f, 0.50f, 0.50f, 0.15f, 0.00f, 0.15f },
};

// a starter groove so it sounds alive the moment it loads
static const char *PRESET[ROWS] = {
    "x.....x...x....x",   // KICK  — electro thump
    "....x.......x...",   // SNARE — backbeat
    "..x...x...x...x.",   // METAL — ringing accents
    "..x..x..x..x..x.",   // PERC  — offbeat blips
    "x..x..x..x..x..x",   // TONE  — bassline
    "............x...",   // CHORD — one stab before the loop
};

// p-locks: per step, per macro. -1 = no lock; 0..100 = value*100 for that step.
static signed char plock[ROWS][STEPS][NMAC];

static bool grid[ROWS][STEPS];
static int  curR = 0, curC = 0;       // keyboard cursor cell
static int  selRow = 0;               // the machine whose knobs show
static int  selMac = K_COL;           // the macro the grid p-locks + shows
static int  cur_step = 0;             // playhead column
static int  last_16 = -1;             // last sixteenth we triggered
static int  flash[ROWS];              // frame() each row last fired
static int  tempo = 124;

static void play_machine(int r, int step);   // fwd — set_cell auditions through it

static float clampf(float v, float lo, float hi){ return v < lo ? lo : v > hi ? hi : v; }

// ── layout ──
#define GX 40    // grid left
#define GY 22    // grid top
#define SX 16    // column stride
#define SY 16    // row stride
#define CW 14    // cell w
#define CH 14    // cell h
#define KY  144  // knob row top
#define KH  34   // knob bar height
#define KX  16   // first knob left
#define KSTRIDE 50

// per-finger drag state
typedef struct {
    int id, kind, r, c, mac, startY;   // id MUST be first (pointer.h)
    float startVal;
    bool  moved;
} Ptr;
enum { KIND_NONE, KIND_CELL, KIND_KNOB };
static Ptr ptr[PTR_MAX];

// transport buttons (top-right BPM) — geometry shared by update() hit-test + draw()
enum { B_BPMDN, B_BPMUP, NBTN };
static const struct { int x, y, w, h; const char *label; } BTN[NBTN] = {
    { 274, 2, 14, 11, "-" },
    { 302, 2, 14, 11, "+" },
};

// ── hit-tests ──
static int cell_rc(int x, int y, int *r, int *c) {
    if (x < GX || y < GY) return 0;
    int cc = (x - GX) / SX, rr = (y - GY) / SY;
    if (cc < 0 || cc >= STEPS || rr < 0 || rr >= ROWS) return 0;
    *r = rr; *c = cc; return 1;
}
static int label_row(int x, int y) {          // left column, over a row → that machine
    if (x >= GX || y < GY) return -1;
    int rr = (y - GY) / SY;
    return (rr >= 0 && rr < ROWS) ? rr : -1;
}
static int knob_idx(int x, int y) {           // which macro knob is under (x,y)?
    if (y < KY || y > KY + KH) return -1;
    for (int k = 0; k < NMAC; k++)
        if (x >= KX + k * KSTRIDE && x < KX + k * KSTRIDE + 22) return k;
    return -1;
}

static void clear_grid(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++) { grid[r][c] = false;
            for (int k = 0; k < NMAC; k++) plock[r][c][k] = -1; }
}

// the value a macro takes on a given step (locked value, else machine base)
static float step_val(int r, int step, int k) {
    signed char pl = plock[r][step][k];
    return (pl < 0) ? mac[r][k] : pl / 100.0f;
}

// fire one machine for one step — applies that step's macros, then strikes
static void play_machine(int r, int step) {
    int slot = SLOT0 + r;
    float m[NMAC];
    for (int k = 0; k < NMAC; k++) m[k] = step_val(r, step, k);

    int dur = 24 + (int)(m[K_DEC] * m[K_DEC] * 560.0f);   // DEC → gate length
    instrument_timbre  (slot, m[K_COL]);                   // COL → brightness
    instrument_harmonics(slot, m[K_SHP]);                  // SHP → carrier:mod ratio
    instrument_morph   (slot, m[K_CON]);                   // CON → feedback grit
    // SWP → downward pitch blip (starts sharp, settles) — the donk / zap
    instrument_env(slot, 1, ENV_PITCH, 0, dur < 140 ? dur : 140, m[K_SWP] * 36.0f);

    int midi = MIDI_LO[r] + (int)(m[K_PIT] * (MIDI_HI[r] - MIDI_LO[r]));
    if (r == M_CHORD) {                                     // one polyphonic machine
        hit(midi,     slot, 3, dur);
        hit(midi + 3, slot, 3, dur);
        hit(midi + 7, slot, 3, dur);
    } else if (r == M_SNARE) {
        hit(midi, slot, 4, dur);                           // FM body...
        hit(60, SNOISE, 5, dur < 90 ? dur : 90);           // ...over a noise crack
    } else {
        hit(midi, slot, 5, dur);
    }
    flash[r] = frame();
}

static void set_cell(int r, int c, bool on) {
    grid[r][c] = on;
    curR = r; curC = c; selRow = r;
    if (on) play_machine(r, c);                            // audition on turn-on
}

void init() {
    // six machines = six INSTR_FM slots. The macros (set per step in play_machine)
    // are what make one engine sound like six drums; here we just set the amp shape.
    for (int r = 0; r < ROWS; r++)
        instrument(SLOT0 + r, INSTR_FM, 0, 120, 0, REL[r]);
    instrument(SNOISE, INSTR_NOISE, 0, 90, 0, 40);         // snare crack
    instrument_filter(SNOISE, FILTER_BAND, 1900, 3);

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++) {
            grid[r][c] = (PRESET[r][c] == 'x');
            for (int k = 0; k < NMAC; k++) plock[r][c][k] = -1;
        }

    // seed a couple of p-locks so the headline is legible on load:
    // METAL brightens (COL rises) across its four hits; TONE walks up in pitch.
    plock[M_METAL][2 ][K_COL] = 40; plock[M_METAL][6 ][K_COL] = 60;
    plock[M_METAL][10][K_COL] = 80; plock[M_METAL][14][K_COL] = 99;
    plock[M_TONE][0 ][K_PIT] = 20; plock[M_TONE][3 ][K_PIT] = 30;
    plock[M_TONE][6 ][K_PIT] = 45; plock[M_TONE][9 ][K_PIT] = 35;
    plock[M_TONE][12][K_PIT] = 55; plock[M_TONE][15][K_PIT] = 65;

    PTR_CLEAR(ptr);
}

void update() {
    bpm(tempo);

    // ── transport: advance the playhead off the synth clock ──
    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int   sixteenth = (int)pos16;
    if (sixteenth != last_16) {
        last_16  = sixteenth;
        cur_step = sixteenth % STEPS;
        for (int r = 0; r < ROWS; r++)
            if (grid[r][cur_step]) play_machine(r, cur_step);
    }

    // ── keyboard: WASD cursor, Z toggle, X clear, arrows tempo + macro select ──
    if (btnp(0, BTN_UP))    { curR = (curR + ROWS - 1) % ROWS; selRow = curR; }
    if (btnp(0, BTN_DOWN))  { curR = (curR + 1) % ROWS;        selRow = curR; }
    if (btnp(0, BTN_LEFT))  curC = (curC + STEPS - 1) % STEPS;
    if (btnp(0, BTN_RIGHT)) curC = (curC + 1) % STEPS;
    if (btnp(0, BTN_A))     set_cell(curR, curC, !grid[curR][curC]);
    if (btnp(0, BTN_B))     clear_grid();

    if (btnp(1, BTN_LEFT)  || tapp(BTN[B_BPMDN].x, BTN[B_BPMDN].y, BTN[B_BPMDN].w, BTN[B_BPMDN].h)) tempo = max(60,  tempo - 4);
    if (btnp(1, BTN_RIGHT) || tapp(BTN[B_BPMUP].x, BTN[B_BPMUP].y, BTN[B_BPMUP].w, BTN[B_BPMUP].h)) tempo = min(220, tempo + 4);
    if (btnp(1, BTN_UP))    selMac = (selMac + NMAC - 1) % NMAC;
    if (btnp(1, BTN_DOWN))  selMac = (selMac + 1) % NMAC;

    // ── touch / mouse ──
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i), r, c, k;
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {
            if (tapp(BTN[B_BPMDN].x, BTN[B_BPMDN].y, BTN[B_BPMDN].w, BTN[B_BPMDN].h) ||
                tapp(BTN[B_BPMUP].x, BTN[B_BPMUP].y, BTN[B_BPMUP].w, BTN[B_BPMUP].h)) {
                p->id = PTR_NONE; continue;                // BPM handled by tapp above
            }
            if ((k = knob_idx(tx, ty)) >= 0) {             // grab a macro knob
                selMac = k;
                *p = (Ptr){ id, KIND_KNOB, 0, 0, k, ty, mac[selRow][k], false };
            } else if (cell_rc(tx, ty, &r, &c)) {          // grab a grid cell
                *p = (Ptr){ id, KIND_CELL, r, c, 0, ty, step_val(r, c, selMac), false };
                selRow = r;
            } else if ((r = label_row(tx, ty)) >= 0) {     // tap a machine name
                selRow = curR = r; p->id = PTR_NONE;
            } else {
                p->id = PTR_NONE;
            }
        } else if (p->kind == KIND_KNOB) {
            // absolute fader: the value tracks the finger's position within the bar,
            // so you never have to drag past the slider to reach the top. A clean tap
            // (no move) just selects the macro without changing it.
            if (abs(ty - p->startY) > 3) p->moved = true;
            if (p->moved)
                mac[selRow][p->mac] = clampf((float)(KY + KH - ty) / (KH - 2), 0, 1);
        } else if (p->kind == KIND_CELL) {
            if (abs(ty - p->startY) > 4) p->moved = true;
            if (p->moved) {
                // fling well below the cell → clear the lock; else set it (drag up = more)
                int cellBot = GY + p->r * SY + CH;
                if (ty > cellBot + 26) {
                    plock[p->r][p->c][selMac] = -1;
                } else {
                    float v = clampf(p->startVal + (p->startY - ty) / 45.0f, 0, 1);
                    plock[p->r][p->c][selMac] = (signed char)(v * 100.0f);
                    grid[p->r][p->c] = true;               // a locked step is a lit step
                }
            }
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (!p) continue;
        if (p->kind == KIND_CELL && !p->moved)             // a clean tap = toggle
            set_cell(p->r, p->c, !grid[p->r][p->c]);
        p->id = PTR_NONE;
    }

#ifdef DE_TRACE
    int lit = 0, locks = 0;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < STEPS; c++) {
        lit += grid[r][c];
        for (int k = 0; k < NMAC; k++) locks += (plock[r][c][k] >= 0);
    }
    watch("tempo",  "%d", tempo);
    watch("selRow", "%d", selRow);
    watch("selMac", "%d", selMac);
    watch("lit",    "%d", lit);
    watch("locks",  "%d", locks);
#endif
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    print("FM BOX", 2, 3, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("six machines - one FM engine - p-locks", 46, 5, CLR_DARK_GREY);
    font(FONT_NORMAL);
    print_right(str("%d", tempo), BTN[B_BPMDN].x - 4, 3, CLR_WHITE);
    for (int b = 0; b < NBTN; b++) {
        rectfill(BTN[b].x, BTN[b].y, BTN[b].w, BTN[b].h, CLR_DARK_GREY);
        rect(BTN[b].x, BTN[b].y, BTN[b].w, BTN[b].h, CLR_MEDIUM_GREY);
        print(BTN[b].label, BTN[b].x + 4, BTN[b].y + 2, CLR_LIGHT_GREY);
    }

    // playhead marker
    rectfill(GX + cur_step * SX, GY - 4, CW, 2, CLR_WHITE);

    for (int r = 0; r < ROWS; r++) {
        bool firing = (frame() - flash[r]) < 5;
        int lcol = (r == selRow) ? CLR_WHITE : firing ? CLR_WHITE : LIT[r];
        print(LABEL[r], 2, GY + r * SY + 4, lcol);
        if (r == selRow) rectfill(0, GY + r * SY + 12, GX - 4, 1, CLR_WHITE);

        for (int c = 0; c < STEPS; c++) {
            int x = GX + c * SX, y = GY + r * SY;
            if (grid[r][c]) {
                rectfill(x, y, CW, CH, LIT[r]);
                // p-lock of the SELECTED macro → a fill-height bar (the height IS the value)
                signed char pl = plock[r][c][selMac];
                if (pl >= 0) {
                    int h = 1 + (pl * (CH - 2)) / 100;
                    rectfill(x + 1, y + CH - 1 - h, CW - 2, h, CLR_WHITE);
                }
                // any lock at all → a corner tick, so other-macro locks stay visible
                bool anylock = false;
                for (int k = 0; k < NMAC; k++) anylock |= (plock[r][c][k] >= 0);
                if (anylock) rectfill(x + CW - 3, y + 1, 2, 2, CLR_BLACK);
                if (c == cur_step) rect(x, y, CW, CH, CLR_WHITE);
            } else {
                int bg = (c == cur_step) ? CLR_DARK_GREY
                       : (c % 4 == 0)    ? CLR_DARKER_BLUE
                                         : CLR_DARKER_GREY;
                rectfill(x, y, CW, CH, bg);
            }
        }
    }
    // cursor outline
    rect(GX + curC * SX - 1, GY + curR * SY - 1, CW + 2, CH + 2, CLR_GREEN);

    // ── knob row: the selected machine's six macros ──
    print_right(str("%s", LABEL[selRow]), 316, KY - 12, LIT[selRow]);
    print(str("edit: %s", LABEL[selRow]), 4, KY - 12, CLR_LIGHT_GREY);
    for (int k = 0; k < NMAC; k++) {
        int x = KX + k * KSTRIDE;
        bool sel = (k == selMac);
        // bar track
        rectfill(x, KY, 12, KH, CLR_DARKER_GREY);
        int h = (int)(mac[selRow][k] * (KH - 2));
        rectfill(x + 1, KY + KH - 1 - h, 10, h, sel ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY);
        if (sel) rect(x - 1, KY - 1, 14, KH + 2, CLR_WHITE);
        font(FONT_SMALL);
        int tw = text_width(MNAME[k]);
        print(MNAME[k], x + 6 - tw / 2, KY + KH + 2, sel ? CLR_WHITE : CLR_LIGHT_GREY);
        font(FONT_NORMAL);
    }

    font(FONT_SMALL);
    print("tap cell=toggle - hold+drag up=lock selected macro - fling down=clear", 4, 192, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
