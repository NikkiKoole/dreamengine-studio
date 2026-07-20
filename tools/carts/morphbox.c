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
    "summary": "A four-voice groovebox where every voice is a MORPH, not a preset: KICK/SNARE/HAT are drum voices whose CHARACTER knob slides the 808↔909 voicing (and past it), plus a melodic PLUCK whose CHAR morphs dark↔bright and whose pitch is pentatonic. Focus a voice and it fills two rows of ~10 knobs + a MORPH pad; write a bassline on the pluck by pinning per-step pitch. A step grid plays itself; real summed-bus PUMP off the kick.",
    "detail": "The honest core is runtime/morphdrum.h — an 808 kick and a 909 kick are the SAME synthesis structure at different numbers, so each voice is one parametric model with a full knob panel. Tap a voice name (left of the grid) to FOCUS it; the two knob rows up top edit that voice. CHAR morphs the STRUCTURAL 808↔909 voicing (click brightness, noise mode, FM metal floor); the rest are absolute controls with ranges that reach beyond both (sub-deep TUNE, endless DEC, PUNCH pitch-sweep, DRIVE into distortion). SWG shuffles the whole grid; PUMP is a real master sidechain keyed off the kick. Per-step automation, two idioms (acidcandy's lesson + the Elektron step-first model). A MODE button cycles STEP / PROB / VEL / LOCK. PROB and VEL are CONTOUR LANES — each active step is a pull-down bar you sweep across the row (trig chance, velocity). LOCK is the STEP-FIRST p-lock editor: tap a step to SELECT it (white ring) and the knobs + MORPH pad pin THAT step's params to absolute values — a dot marks a locked knob and a locked step — so every hit can sit at its own point in the 808↔909 space, its own cutoff, decay, drive, sub. No page-per-parameter; the controls you already have become the step's editor. Reflows: roomy 320x200 or the compact 160x100 pocket face.",
    "controls": "STEP mode: tap/drag cells to paint (hat cells cycle off→closed→open). Tap a voice NAME to focus it, drag the top knobs (vertical = value, double-tap = reset). MODE button (header) cycles STEP/PROB/VEL/LOCK. PROB/VEL: vertical-drag a cell to set its bar, drag across to sweep a contour. LOCK: tap a step to select it, then turn any knob or drag the MORPH pad to pin that param for the step (re-tap = deselect, double-tap a step = clear its locks). Top row's 6th knob = SWG, 2nd row's 6th = PUMP. BPM readout, PLAY/STOP top-right."
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
        padx = 6; pady = 28; padw = 80; padh = 78;
        kx[0] = 116; kx[1] = 158; kx[2] = 200; kx[3] = 242; kx[4] = 300;
        ky1 = 48; ky2 = 90;
        gx = 40; gy = 118; sx = 17; sy = 19; cw = 15; ch = 16;   // 4 rows fit in 118..194
    } else {                           // 160x100 — compact pocket face, FONT_SMALL
        hfont = FONT_SMALL;
        padx = 3; pady = 14; padw = 42; padh = 40;
        kx[0] = 56; kx[1] = 78; kx[2] = 100; kx[3] = 122; kx[4] = 144;
        ky1 = 21; ky2 = 43;
        gx = 20; gy = 58; sx = 8; sy = 10; cw = 7; ch = 9;       // 4 rows fit in 58..98
    }
}

static MorphKit kit;
static bool grid[ROWS][STEPS];
static bool hopen[STEPS];        // HAT row: this step is an OPEN hat (long, choked by the next closed)

static const int LIT[ROWS] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN };

// a default groove that shows the box off the moment it loads
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // KICK — four on the floor
    "....x.......x...",   // SNARE — backbeat
    "..x.x.x.x.x.x.x.",   // HAT — running off-beats
    "x.......x...x...",   // PLUCK — a sparse bass root (lock TUNE per step for a line)
};

static int  focus    = 0;        // which voice the top knob rows edit
static int  cur_step = 0;
static int  last_16  = -1;
static int  flash[ROWS];
static bool playing  = true;
static int  g_bpm    = 124;
static float k_swing = 0.0f;     // global shuffle 0..1
static float k_pump  = 0.55f;    // master sidechain depth

// ── per-step data (acidcandy's p-lock lesson, then the Elektron step-first model) ──
// PROB + VEL are CONTOUR LANES — 0..1, default 1, pull-down bars you sweep across the row
// (the rhythmic step attributes with no single voice knob).
enum { PL_PROB, PL_VEL, PL_N };
static float pl[ROWS][STEPS][PL_N];
// PARAM LOCKS are the step-first model: SELECT a step (LOCK mode) and the knobs + MORPH pad
// pin that step's morphdrum params to absolute values (Elektron p-locks). No page per param —
// the controls you already have become the step's editor.
static char  lk[ROWS][STEPS][MD_NPARAM];   // is this param locked on this step?
static float lv[ROWS][STEPS][MD_NPARAM];   // the locked value (0..1)
static int   sel_r = -1, sel_c = -1;       // the selected step in LOCK mode (-1 = none)
static int   tap_key = -1, tap_f = -1000;  // last LOCK-tap (r*STEPS+c) + frame, for double-tap-clear

// grid mode: STEP = on/off · PROB/VEL = contour bars · LOCK = step-first p-lock editor
enum { LN_STEP, LN_PROB, LN_VEL, LN_LOCK, LN_N };
static int  lane = LN_STEP;
static const char *LN_NAME[LN_N] = { "STEP", "PROB", "VEL", "LOCK" };
static const int   LN_PL  [LN_N] = { -1, PL_PROB, PL_VEL, -1 };

typedef struct { int id, paint, lastR, lastC; } Ptr;   // id MUST be first (pointer.h contract)
static Ptr ptr[PTR_MAX];

static bool step_has_lock(int r, int c) {   // any param pinned on this step?
    for (int p = 0; p < MD_NPARAM; p++) if (lk[r][c][p]) return true;
    return false;
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
                // param P-LOCKS: pin the step's locked params to their absolute values, reconfigure
                // the voice (locks reshape the instrument), fire, then restore the knob state.
                if (step_has_lock(r, cur_step)) {
                    float save[MD_NPARAM];
                    for (int p = 0; p < MD_NPARAM; p++) {
                        save[p] = kit.p[r][p];
                        if (lk[r][cur_step][p]) kit.p[r][p] = lv[r][cur_step][p];
                    }
                    morph_apply(&kit, r);
                    fire_row(r, boost, sw);
                    for (int p = 0; p < MD_NPARAM; p++) kit.p[r][p] = save[p];
                    morph_apply(&kit, r);             // back to the knob state for the next fire / display
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

    // MORPH PAD drag: sets CHAR (x) + TUNE (y). In LOCK mode with a step selected it PINS that
    // step's CHAR/TUNE (drop one hit in 808↔909 space); otherwise it drives the focused voice.
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= padx && tx < padx + padw && ty >= pady && ty < pady + padh) {
            float cx = (float)(tx - padx) / (padw - 1), cy = (float)(ty - pady) / (padh - 1);
            cx = cx < 0 ? 0 : cx > 1 ? 1 : cx;
            float tv = 1.0f - (cy < 0 ? 0 : cy > 1 ? 1 : cy);
            if (lane == LN_LOCK && sel_r >= 0) {
                lk[sel_r][sel_c][MD_CHAR] = 1; lv[sel_r][sel_c][MD_CHAR] = cx;
                lk[sel_r][sel_c][MD_TUNE] = 1; lv[sel_r][sel_c][MD_TUNE] = tv;
            } else {
                kit.p[focus][MD_CHAR] = cx; kit.p[focus][MD_TUNE] = tv;
            }
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

        if (lane == LN_LOCK) {                             // LOCK: tap SELECTS a step; double-tap CLEARS
            if (fresh) {
                int key = r * STEPS + c;
                bool dbl = (key == tap_key) && (frame() - tap_f < 18);
                tap_key = key; tap_f = frame();
                if (dbl) {                                 // double-tap → clear this step's p-locks
                    for (int q = 0; q < MD_NPARAM; q++) lk[r][c][q] = 0;
                    sel_r = r; sel_c = c; focus = r;       // keep selected so the knobs snap back to voice
                } else if (sel_r == r && sel_c == c) {
                    sel_r = sel_c = -1;                     // re-tap = deselect
                } else {
                    if (!grid[r][c]) { grid[r][c] = true; hopen[c] = false; }   // place an empty step
                    sel_r = r; sel_c = c; focus = r;
                }
                p->lastR = r; p->lastC = c;
            }
            continue;
        }
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
    // MODE selector: STEP → PROB → VEL → LOCK. PROB/VEL edit contour bars; LOCK = step-first
    // p-locks (tap a step, the knobs + pad pin its params). Leaving LOCK drops the selection.
    if (ui_button(screen_w() - bw - bw - 6, 1, bw, bhh, LN_NAME[lane])) {
        lane = (lane + 1) % LN_N;
        if (lane != LN_LOCK) sel_r = sel_c = -1;
    }

    // ── the MORPH PAD (left): CHAR × TUNE, 808/909 landmarks. In LOCK mode it shows/edits the
    // SELECTED step's CHAR/TUNE (a pinned puck); otherwise the focused voice. ──
    bool locking = (lane == LN_LOCK && sel_r >= 0);
    {
        int x = padx, y = pady, w = padw, h = padh;
        float chv = locking ? (lk[sel_r][sel_c][MD_CHAR] ? lv[sel_r][sel_c][MD_CHAR] : kit.p[sel_r][MD_CHAR]) : kit.p[focus][MD_CHAR];
        float tu  = locking ? (lk[sel_r][sel_c][MD_TUNE] ? lv[sel_r][sel_c][MD_TUNE] : kit.p[sel_r][MD_TUNE]) : kit.p[focus][MD_TUNE];
        int px = x + (int)(chv * (w - 1)), py = y + (int)((1.0f - tu) * (h - 1));
        rectfill(x, y, w, h, CLR_DARKER_GREY);
        rect(x, y, w, h, CLR_MEDIUM_GREY);
        line(x + 1, py, x + w - 2, py, CLR_DARK_GREY);        // crosshair
        line(px, y + 1, px, y + h - 2, CLR_DARK_GREY);
        // X-axis labels are voice-aware: drums travel 808→909, the pluck travels dark→bright.
        int pv = locking ? sel_r : focus;
        const char *xlo = (pv == MD_PLUCK) ? "DARK" : "808";
        const char *xhi = (pv == MD_PLUCK) ? "BRT"  : "909";
        font(hfont);
        print("TUNE", x + 2, y + 1, CLR_MEDIUM_GREY);                          // Y axis (pitch)
        print(xlo, x + 2, y + h - fh - 1, CLR_MEDIUM_GREY);
        print(xhi, x + w - text_width(xhi) - 2, y + h - fh - 1, CLR_MEDIUM_GREY);
        int pr = roomy ? 3 : 2;
        circfill(px, py, pr, LIT[focus]);
        circ(px, py, pr, CLR_WHITE);
    }

    // ── knob rows: the focused voice's 8 panel knobs + SWG/PUMP. In LOCK mode with a step
    // selected, each panel knob edits that STEP's lock (a dot marks a locked knob). ──
    font(FONT_SMALL);            // knob labels stay compact in both modes (short words)
    const MDKnob *panel = MD_PANEL[focus];   // panel[0]=CHAR, [1]=TUNE live on the pad
    static float kdisp[8];       // stable per-slot addresses (ui_knob keys drag-capture on the ptr)
    for (int row = 0; row < 2; row++) {
        int ky = row ? ky2 : ky1;
        for (int i = 0; i < 4; i++) {
            int slot = row * 4 + i, par = panel[2 + slot].param;
            const char *lbl = panel[2 + slot].label;
            if (locking) {
                kdisp[slot] = lk[sel_r][sel_c][par] ? lv[sel_r][sel_c][par] : kit.p[sel_r][par];
                if (ui_knob(&kdisp[slot], kx[i], ky, lbl)) { lv[sel_r][sel_c][par] = kdisp[slot]; lk[sel_r][sel_c][par] = 1; }
                if (lk[sel_r][sel_c][par]) rectfill(kx[i] + UI_KNOB_R - 2, ky - UI_KNOB_R + 1, 2, 2, CLR_WHITE);
            } else {
                ui_knob(&kit.p[focus][par], kx[i], ky, lbl);
            }
        }
    }
    ui_knob(&k_swing, kx[4], ky1, "SWG");
    ui_knob(&k_pump,  kx[4], ky2, "PUMP");

    // LOCK-mode hint (roomy has an empty band above the grid)
    if (roomy && lane == LN_LOCK) {
        font(FONT_SMALL);
        print(sel_r >= 0 ? "LOCK: turn a knob / drag pad to pin - double-tap step to clear"
                         : "LOCK: tap a step to edit its params",
              6, gy - 9, CLR_MEDIUM_GREY);
    }

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
            if (lane == LN_PROB || lane == LN_VEL) {        // contour bars (dim on OFF steps)
                if (grid[r][c]) {
                    int bh = (int)(pl[r][c][LN_PL[lane]] * (ch - 2) + 0.5f);
                    rectfill(x + 1, y + ch - 1 - bh, cw - 2, bh, LIT[r]);
                } else {
                    rectfill(x + 1, y + ch - 2, cw - 2, 1, CLR_DARK_GREY);   // off-step floor tick
                }
            } else {                                        // STEP + LOCK: on/off blocks
                if (grid[r][c]) {
                    rectfill(x + 1, y + 1, cw - 2, ch - 2, LIT[r]);
                    if (r == MD_HAT && hopen[c]) rect(x + 2, y + 2, cw - 4, ch - 4, CLR_WHITE);  // open hat = ringed
                    if (lane == LN_LOCK && step_has_lock(r, c))   // this step carries p-locks
                        rectfill(x + cw - 3, y + 1, 2, 2, CLR_WHITE);
                }
                if (lane == LN_LOCK && r == sel_r && c == sel_c) {   // the SELECTED step (thick ring)
                    rect(x, y, cw, ch, CLR_WHITE); rect(x + 1, y + 1, cw - 2, ch - 2, CLR_WHITE);
                }
            }
            if (c == cur_step && playing) rect(x, y, cw, ch, CLR_WHITE);   // playhead
        }
    }
    ui_end();
}
