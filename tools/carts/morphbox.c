/* de:meta
{
  "slug": "morphbox",
  "title": "Morphbox",
  "resizable": true,
  "status": "active",
  "created": "2026-07-19",
  "kind": ["instrument", "tech-demo"],
  "genre": null,
  "teaches": ["step-sequencer"],
  "lineage": "The groovebox transport (self-playing grid + summed-bus sidechain pump) crossed with a NEW honest core: runtime/morphdrum.h, where each of three drum voices is ONE parametric synth and the 808/909 are just two points in it — a CHARACTER knob morphs the 808↔909 STRUCTURE while a deep panel of absolute knobs shapes the sound past both machines (electro→rock). Complements acidcandy (which keeps 808/909 as separate faces); this is the continuum, exposed as knobs.",
  "description": {
    "summary": "A three-voice drum synth where every drum is a MORPH, not a preset: focus KICK, SNARE or HAT and it fills two rows of ~10 knobs — CHARACTER slides the 808↔909 voicing while TUNE/DEC/PUNCH/DRIVE/CUT/SUB… carve the sound well past both machines. A 3-row step grid plays itself; real summed-bus PUMP off the kick.",
    "detail": "The honest core is runtime/morphdrum.h — an 808 kick and a 909 kick are the SAME synthesis structure at different numbers, so each voice is one parametric model with a full knob panel. Tap a voice name (left of the grid) to FOCUS it; the two knob rows up top edit that voice. CHAR morphs the STRUCTURAL 808↔909 voicing (click brightness, noise mode, FM metal floor); the rest are absolute controls with ranges that reach beyond both (sub-deep TUNE, endless DEC, PUNCH pitch-sweep, DRIVE into distortion). SWG shuffles the whole grid; PUMP is a real master sidechain keyed off the kick. Per-step P-LOCK LANES (the acidcandy lesson): a LANE button cycles the grid between STEP (on/off) and PROB / VEL / TUNE / DEC / CHAR — each active step becomes an editable bar (PROB/VEL pull down from full; TUNE/DEC/CHAR are bipolar offsets around the voice knob, so e.g. every hit can sit at a different point in the 808↔909 space). Reflows: roomy 320x200 or the compact 160x100 pocket face.",
    "controls": "Tap/drag grid cells to paint steps (hat cells cycle off→closed→open). Tap a voice NAME to focus it, then drag the top knobs (vertical = value, double-tap = reset). The LANE button (header) cycles STEP/PROB/VEL/TUNE/DEC/CHAR — in a lane, vertical-drag a cell to set its bar, drag across to sweep a contour. Top row's 6th knob = SWG, 2nd row's 6th = PUMP. BPM readout, PLAY/STOP top-right."
  }
}
de:meta */
#include "studio.h"
#define UI_KNOB_R 6              // small candy knobs for the 160x100 face
#define UI_KNOB_DRAG_PX 40       // shorter full-sweep drag to suit the tiny screen
#include "ui.h"
#include "morphdrum.h"   // the honest core: one parametric voice per role, 808↔909 as endpoints
#include "pointer.h"     // the shared multi-finger pool (PTR_ACQUIRE/FIND/CLEAR) — handles the
                         // touch_id bookkeeping right, incl. the negative synthetic-mouse id my
                         // hand-rolled pool mis-treated as a free slot (the paint bug)
#include <stdio.h>       // snprintf (the BPM readout)

// MORPHBOX — a three-voice drum box on the idea that the 808 and the 909 are two ends
// of ONE knob. morphdrum.h owns the synthesis (a deep parametric panel per voice); this
// owns the pattern, the transport, and the humble candy surface.

#define ROWS  MD_NV      // 3 voices
#define STEPS 16

// LAYOUT REFLOWS with the canvas (device-adaptive-layout.md): the cart is `resizable`, so it
// reads screen_w()/screen_h() and picks a COMPACT layout (≤ ~200px wide → the 160x100 pocket
// face) or a ROOMY one (→ 320x200, bigger font + breathing room for text). Same structure both
// ways. All geometry lives in these globals, recomputed each frame by layout() — never fixed
// #defines, so touch coords (raw canvas px) always match what's drawn (no camera scale — that
// would desync ui.h; see CLAUDE.md).
static bool roomy;
static int  gx, gy, sx, sy, cw, ch;    // grid: left/top, column/row stride, cell w/h
static int  padx, pady, padw, padh;    // the MORPH PAD rect
static int  kx[5], ky1, ky2;           // knob columns (4 voice + 1 global) + the two row centres
static int  hfont;                     // font for header / voice names / pad labels

static void layout(void) {
    int w = screen_w();
    roomy = (w >= 240);
    if (roomy) {                       // 320x200 — roomy, FONT_NORMAL text
        hfont = FONT_NORMAL;
        padx = 6; pady = 30; padw = 84; padh = 84;
        kx[0] = 116; kx[1] = 158; kx[2] = 200; kx[3] = 242; kx[4] = 300;
        ky1 = 52; ky2 = 98;
        gx = 40; gy = 128; sx = 17; sy = 22; cw = 15; ch = 20;
    } else {                           // 160x100 — compact pocket face, FONT_SMALL
        hfont = FONT_SMALL;
        padx = 3; pady = 14; padw = 42; padh = 40;
        kx[0] = 56; kx[1] = 78; kx[2] = 100; kx[3] = 122; kx[4] = 144;
        ky1 = 21; ky2 = 43;
        gx = 20; gy = 61; sx = 8; sy = 12; cw = 7; ch = 10;
    }
}

static MorphKit kit;
static bool grid[ROWS][STEPS];
static bool hopen[STEPS];        // HAT row: this step is an OPEN hat (long, choked by the next closed)

static const int LIT[ROWS] = { CLR_RED, CLR_ORANGE, CLR_YELLOW };

// a default groove that shows the box off the moment it loads
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // KICK — four on the floor
    "....x.......x...",   // SNARE — backbeat
    "..x.x.x.x.x.x.x.",   // HAT — running off-beats
};

static int  focus    = 0;        // which voice the top knob rows edit
static int  cur_step = 0;
static int  last_16  = -1;
static int  flash[ROWS];
static bool playing  = true;
static int  g_bpm    = 124;
static float k_swing = 0.0f;     // global shuffle 0..1
static float k_pump  = 0.55f;    // master sidechain depth

// ── per-step LANES (acidcandy's p-lock lesson): a step carries more than on/off ──
// PROB/VEL are 0..1, default 1 (full bar, pull DOWN to reduce). TUNE/DEC/CHAR are bipolar
// offsets around the voice knob (default 0.5 = follow) — wired in increment 2.
enum { PL_PROB, PL_VEL, PL_TUNE, PL_DEC, PL_CHAR, PL_N };
static float pl[ROWS][STEPS][PL_N];
// the grid's edit mode: STEP = on/off; a lane = a bar for that param. PROB/VEL are unipolar
// (pull down from full); TUNE/DEC/CHAR are BIPOLAR per-step offsets around the voice knob
// (centre = follow the knob, up/down = ± that step).
enum { LN_STEP, LN_PROB, LN_VEL, LN_TUNE, LN_DEC, LN_CHAR, LN_N };
static int  lane = LN_STEP;
static const char *LN_NAME[LN_N] = { "STEP", "PROB", "VEL", "TUNE", "DEC", "CHAR" };
static const int   LN_PL  [LN_N] = { -1, PL_PROB, PL_VEL, PL_TUNE, PL_DEC, PL_CHAR };
static const int   LN_BI  [LN_N] = { 0, 0, 0, 1, 1, 1 };   // bipolar (centre = follow knob)?
static const int   LN_MP  [LN_N] = { -1, -1, -1, MD_TUNE, MD_DECAY, MD_CHAR };  // the morphdrum knob it offsets

typedef struct { int id, paint, lastR, lastC; } Ptr;   // id MUST be first (pointer.h contract)
static Ptr ptr[PTR_MAX];

static float plock_off(float base, float o) {   // knob + per-step offset (o=0.5 = follow), clamped
    float e = base + (o - 0.5f);
    return e < 0 ? 0 : e > 1 ? 1 : e;
}

static void fire_row(int r, int boost, int delay) {
    if (r == MD_HAT && hopen[cur_step]) morph_fire_open(&kit, boost, delay);
    else                                morph_fire(&kit, r, boost, delay);
    flash[r] = frame();
}

void init(void) {
    morph_build(&kit, 9);        // voice slots 9..9+MD_NSLOT-1

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++) {
            grid[r][c] = (PRESET[r][c] == 'x');
            pl[r][c][PL_PROB] = 1.0f; pl[r][c][PL_VEL] = 1.0f;   // full = always, full velocity
            pl[r][c][PL_TUNE] = pl[r][c][PL_DEC] = pl[r][c][PL_CHAR] = 0.5f;   // centre = follow knob
        }

    sidechain_key(kit.base + MDS_KICK, 0, 1.0f);   // the kick body IS the pump trigger

    PTR_CLEAR(ptr);
}

static int cell_rc(int x, int y, int *r, int *c) {
    if (x < gx || y < gy) return 0;
    int cc = (x - gx) / sx, rr = (y - gy) / sy;
    if (cc < 0 || cc >= STEPS || rr < 0 || rr >= ROWS) return 0;
    if (x > gx + cc * sx + cw || y > gy + rr * sy + ch) return 0;
    *r = rr; *c = cc; return 1;
}

void update(void) {
    layout();                    // reflow to the current canvas (compact ↔ roomy)
    bpm(g_bpm);
    morph_ride(&kit);            // rebuild only the voices whose knobs moved (set-and-hold)
#ifdef DE_TRACE
    watch("focus", "%d", focus);
#endif

    static float aPump = -1;
    if (k_pump != aPump) { sidechain(0, 0, k_pump, 1, 180); aPump = k_pump; }

    if (playing) {
        float pos16 = beat() * 4 + beat_pos() * 4.0f;
        int sixteenth = (int)pos16;
        if (sixteenth != last_16) {
            last_16  = sixteenth;
            cur_step = sixteenth % STEPS;
            int sw = (cur_step & 1) ? (int)(k_swing * 0.6f * (15000.0f / g_bpm)) : 0;
            for (int r = 0; r < ROWS; r++) {
                if (!grid[r][cur_step]) continue;
                if (rnd(1000) >= (int)(pl[r][cur_step][PL_PROB] * 1000)) continue;   // PROB gate
                int boost = -(int)((1.0f - pl[r][cur_step][PL_VEL]) * 6.0f + 0.5f);  // VEL → velocity
                // param p-LOCKS: offset TUNE/DEC/CHAR around the voice knob for THIS hit, then
                // reconfigure the voice (CHAR/DEC change the instrument), fire, and restore.
                float ot = pl[r][cur_step][PL_TUNE], od = pl[r][cur_step][PL_DEC], oc = pl[r][cur_step][PL_CHAR];
                if (ot != 0.5f || od != 0.5f || oc != 0.5f) {
                    float st = kit.p[r][MD_TUNE], sd = kit.p[r][MD_DECAY], sc = kit.p[r][MD_CHAR];
                    kit.p[r][MD_TUNE] = plock_off(st, ot);
                    kit.p[r][MD_DECAY] = plock_off(sd, od);
                    kit.p[r][MD_CHAR] = plock_off(sc, oc);
                    morph_apply(&kit, r);
                    fire_row(r, boost, sw);
                    kit.p[r][MD_TUNE] = st; kit.p[r][MD_DECAY] = sd; kit.p[r][MD_CHAR] = sc;
                    morph_apply(&kit, r);              // back to the knob state for the next fire / display
                } else {
                    fire_row(r, boost, sw);
                }
            }
        }
    }

    if (btnp(0, BTN_UP))    focus = (focus + ROWS - 1) % ROWS;
    if (btnp(0, BTN_DOWN))  focus = (focus + 1) % ROWS;
    if (btnp(1, BTN_LEFT))  g_bpm = g_bpm > 70  ? g_bpm - 2 : g_bpm;
    if (btnp(1, BTN_RIGHT)) g_bpm = g_bpm < 200 ? g_bpm + 2 : g_bpm;

    // voice focus: ANY finger resting on a name cell picks that voice. Checked every
    // frame (not just on touch-down) so it's robust to the down-frame position lag that
    // a mouse click / quick tap has — the old down-frame-only check missed those.
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= 1 && tx < gx - 2 && ty >= gy && ty < gy + ROWS * sy)
            focus = (ty - gy) / sy;
    }

    // MORPH PAD drag: a finger inside the pad sets the focused voice's CHAR (x) + TUNE (y)
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= padx && tx < padx + padw && ty >= pady && ty < pady + padh) {
            float cx = (float)(tx - padx) / (padw - 1);
            float cy = (float)(ty - pady) / (padh - 1);
            kit.p[focus][MD_CHAR] = cx < 0 ? 0 : cx > 1 ? 1 : cx;
            kit.p[focus][MD_TUNE] = 1.0f - (cy < 0 ? 0 : cy > 1 ? 1 : cy);
        }
    }

    // ── touch/mouse (pointer.h pool). STEP mode: tap toggles (hat cycles off→closed→open),
    // drag paints a run. A LANE mode (PROB/VEL): a finger sets that cell's bar from its Y, and
    // sweeps a contour as it drags across — the acidcandy PROB/PCF-lane feel. ──
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i), r, c;
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                                  // pool full
        bool on = cell_rc(tx, ty, &r, &c);
        if (fresh) {
            if (!on) { p->id = PTR_NONE; continue; }        // landed off the grid — not ours
            p->id = id; p->paint = -1; p->lastR = p->lastC = -1;
        }
        if (!on) continue;                                 // dragged off the grid — hold, do nothing

        if (lane != LN_STEP) {                             // LANE edit: set this cell's bar from Y
            float v = 1.0f - (float)(ty - (gy + r * sy)) / (ch - 1);
            pl[r][c][LN_PL[lane]] = v < 0 ? 0 : v > 1 ? 1 : v;
            p->lastR = r; p->lastC = c;
            continue;
        }
        if (fresh) {                                       // STEP: decide + start the stroke
            if (r == MD_HAT) {                             // hat cell CYCLES: off → closed → open → off
                if (!grid[r][c])    { grid[r][c] = true; hopen[c] = false; }   // → closed
                else if (!hopen[c]) { hopen[c] = true; }                       // → open
                else                { grid[r][c] = false; hopen[c] = false; }  // → off
                p->paint = grid[r][c] ? 1 : 0;
                if (grid[r][c]) {                          // audition what it became
                    if (hopen[c]) morph_fire_open(&kit, 0, 0); else morph_fire(&kit, MD_HAT, 0, 0);
                    flash[r] = frame();
                }
            } else {
                p->paint = !grid[r][c];
                grid[r][c] = p->paint;
                if (p->paint) fire_row(r, 0, 0);
            }
            p->lastR = r; p->lastC = c;
        } else if (p->paint >= 0 && (r != p->lastR || c != p->lastC)) {
            grid[r][c] = p->paint;                         // drag: extend the stroke onto a new cell
            if (r == MD_HAT) hopen[c] = false;             // dragging paints CLOSED hats (tap to cycle open)
            if (p->paint) fire_row(r, 0, 0);
            p->lastR = r; p->lastC = c;
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }
}

void draw(void) {
    layout();                    // reflow to the current canvas (compact ↔ roomy)
    cls(CLR_DARKER_GREY);
    ui_begin();
    int fh = roomy ? 8 : 6;      // header/name font cell height (FONT_NORMAL vs FONT_SMALL)

    // ── header: [title ·] focused voice · BPM · PLAY ──
    font(hfont);
    int hx = 3;
    if (roomy) { print("MORPHBOX", hx, 3, CLR_MEDIUM_GREY); hx = 90; }
    print(MD_NAME[focus], hx, 3, LIT[focus]);
    char bt[16]; snprintf(bt, sizeof bt, "%d BPM", g_bpm);
    print(bt, hx + (roomy ? 44 : 28), 3, CLR_LIGHT_GREY);
    int bw = roomy ? 48 : 32, bhh = roomy ? 13 : 9;
    if (ui_button(screen_w() - bw - 2, 1, bw, bhh, playing ? "STOP" : "PLAY")) playing = !playing;
    // LANE selector: cycles STEP → PROB → VEL. When != STEP the grid edits that lane as bars.
    if (ui_button(screen_w() - bw - bw - 6, 1, bw, bhh, LN_NAME[lane])) lane = (lane + 1) % LN_N;

    // ── the MORPH PAD (left): CHAR × TUNE of the focused voice, 808/909 landmarks ──
    {
        int x = padx, y = pady, w = padw, h = padh;
        float chv = kit.p[focus][MD_CHAR], tu = kit.p[focus][MD_TUNE];
        int px = x + (int)(chv * (w - 1)), py = y + (int)((1.0f - tu) * (h - 1));
        rectfill(x, y, w, h, CLR_DARKER_GREY);
        rect(x, y, w, h, CLR_MEDIUM_GREY);
        line(x + 1, py, x + w - 2, py, CLR_DARK_GREY);        // crosshair
        line(px, y + 1, px, y + h - 2, CLR_DARK_GREY);
        font(hfont);
        print("TUNE", x + 2, y + 1, CLR_MEDIUM_GREY);                          // Y axis
        print("808",  x + 2, y + h - fh - 1, CLR_MEDIUM_GREY);                 // X: char 0 → 808 …
        print("909",  x + w - text_width("909") - 2, y + h - fh - 1, CLR_MEDIUM_GREY);  // … char 1 → 909
        int pr = roomy ? 3 : 2;
        circfill(px, py, pr, LIT[focus]);
        circ(px, py, pr, CLR_WHITE);
    }

    // ── knob rows to the right: the focused voice's 8 remaining panel knobs + SWG/PUMP ──
    font(FONT_SMALL);            // knob labels stay compact in both modes (short words)
    const MDKnob *panel = MD_PANEL[focus];   // panel[0]=CHAR, [1]=TUNE live on the pad
    for (int i = 0; i < 4; i++) ui_knob(&kit.p[focus][panel[2 + i].param], kx[i], ky1, panel[2 + i].label);
    ui_knob(&k_swing, kx[4], ky1, "SWG");
    for (int i = 0; i < 4; i++) ui_knob(&kit.p[focus][panel[6 + i].param], kx[i], ky2, panel[6 + i].label);
    ui_knob(&k_pump,  kx[4], ky2, "PUMP");

    // ── the grid: names on the left (tap to focus, handled in update), 16 steps right ──
    font(hfont);
    for (int r = 0; r < ROWS; r++) {
        int y = gy + r * sy;
        bool foc = (r == focus);
        bool firing = frame() - flash[r] < 4;
        rectfill(1, y, gx - 3, ch, firing ? LIT[r] : (foc ? CLR_DARK_GREY : CLR_DARKER_GREY));
        rect(1, y, gx - 3, ch, foc ? LIT[r] : CLR_DARK_GREY);
        print(MD_NAME[r], 3, y + (ch - fh) / 2, (firing || foc) ? CLR_WHITE : LIT[r]);

        for (int c = 0; c < STEPS; c++) {
            int x = gx + c * sx;
            int bg = (c % 4 == 0) ? CLR_DARK_GREY : CLR_DARKER_GREY;
            rectfill(x, y, cw, ch, bg);
            if (lane == LN_STEP) {                          // on/off blocks
                if (grid[r][c]) {
                    rectfill(x + 1, y + 1, cw - 2, ch - 2, LIT[r]);
                    if (r == MD_HAT && hopen[c]) rect(x + 2, y + 2, cw - 4, ch - 4, CLR_WHITE);  // open hat = ringed
                }
            } else if (grid[r][c]) {                        // LANE: a bar (dim on OFF steps)
                float v = pl[r][c][LN_PL[lane]];
                if (LN_BI[lane]) {                          // bipolar: bar from a centre line
                    int mid = y + ch / 2, off = (int)((v - 0.5f) * (ch - 2));
                    if (off > 0)      rectfill(x + 1, mid - off, cw - 2, off, LIT[r]);
                    else if (off < 0) rectfill(x + 1, mid + 1,   cw - 2, -off, LIT[r]);
                    line(x + 1, mid, x + cw - 2, mid, CLR_MEDIUM_GREY);
                } else {                                    // unipolar: pull-down bar
                    int bh = (int)(v * (ch - 2) + 0.5f);
                    rectfill(x + 1, y + ch - 1 - bh, cw - 2, bh, LIT[r]);
                }
            } else {
                rectfill(x + 1, y + ch - 2, cw - 2, 1, CLR_DARK_GREY);   // off-step floor tick
            }
            if (c == cur_step && playing) rect(x, y, cw, ch, CLR_WHITE);   // playhead
        }
    }
    ui_end();
}
