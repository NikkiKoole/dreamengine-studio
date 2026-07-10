/* de:meta
{
  "slug": "acidwire",
  "title": "acidwire",
  "status": "active",
  "created": "2026-07-07",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": {
    "summary": "The device-matrix WIREFRAME tool for the acidrack redesign — CLICKABLE/TAPPABLE (touch+mouse): flip acidrack's four-state layout through every device shape and click the controls (mute, patterns, open a strip, focus, tabs) to study the interaction, not just the pixels.",
    "detail": "A design tool for the acidrack redesign (device-adaptive-layout.md Phase 3, brief acidrack-layout-brief.md). The window is fixed (940x700); pressing a key calls de_resize() to shrink the CANVAS to the exact logical @ K=2 size of each device in device-matrix.md §2 (iPhone SE ... iPad 13 landscape). The engine letterboxes it, so you watch acidrack reflow at each TRUE shape - no fake nested device rect (the field-018 honesty win: lay.h + screen_w()/screen_h() see the real size, same path production acidrack uses). Because the canvas is already K=2 logical px, a 44pt finger is a constant 22 logical px - so every control's finger-comfort is honest. It draws the three-state strip model (folded/compact/expanded) and the per-shape arrangements from the brief: roomy=all-compact rack, tall=one expanded + compact + folded, short-wide=tabs. It's the vehicle for the brief's open compact-strip taste calls - tweak the compact layout here and eyeball it across all shapes.",
    "controls": "TAP/CLICK: a strip NAME opens it (compact->expanded->focus) - the M button (un)mutes - a pattern chip selects it - a landscape TAB name opens it + its M mutes without opening - in FOCUS the instrument NAME (‹) is the back button (no X; [M][fx] sit top-right) - the bottom-left label flips to the next device. KEYS mirror it: ]/->/space/` (key left of 1) next shape - [/<- prev - 1-9 jump - w cycle which strip is expanded (also toggles the iPad all-compact rack) - f FOCUS the working strip fullscreen (drum full grid / 303 programmer; f or the X closes) - m (un)mute the working strip - p cycle its pattern (6 per instrument) - s toggle the device SAFE-AREA skin (notch/Dynamic Island/rounded corners/home bar/status bar + the dashed keep-out boundary) - g toggle the 1-FINGER reference grid (44pt cells — anything smaller than a cell is sub-finger) - h hide the label"
  }
}
de:meta */
// ACIDWIRE — the wireframe tool for the acidrack device-adaptive redesign.
// See docs/design/device-matrix.md (§2 shapes) + acidrack-layout-brief.md (the
// three-state strip model this draws). Fixed window; de_resize() reshapes the
// CANVAS to each device's logical @ K=2 size, letterboxed by the engine.

#include "studio.h"
#include <math.h>
#include <string.h>
#include "lay.h"
#include "ui.h"   // real widgets (ui_knob/ui_slider) — per-finger capture + fat hit pads, so the
                  // wireframe's controls actually MOVE (no sound; just honest finger-ergonomics)
#include "disclose.h"   // the shape classify + finger-budget accordion pass (R2 — this cart is its proof)

extern void de_resize(int w, int h);   // engine seam (platform.h): set the active canvas size

#define KEY_GRAVE 96                    // the key left of `1` (a `§` on ISO Mac boards)

// ───────── device matrix (device-matrix.md §2) — logical @ K=2 + safe hardware ─────────
enum { TALL, WIDE, ROOMY };
enum { N_HOME, N_NOTCH, N_ISLAND, N_PAD };   // top hardware: home-button era · notch · Dynamic Island · none
typedef struct { const char *name; int w, h, cls, insT, insB, notch, cornR; } Dev;
// insT/insB = simulated safe-area top/bottom in logical px (status/notch · home bar); notch = the
// top cutout shape; cornR = screen corner radius. acidrack insets top+bottom only (landscape
// side-notch parked — brief §4). All device-honest so the wireframe shows WHERE controls can't go.
static Dev DEV[] = {
    { "iPhone SE",        188, 334, TALL,  10,  0, N_HOME,    3 },   // home button: status bar, square-ish corners
    { "iPhone 13 mini",   188, 406, TALL,  24, 16, N_NOTCH,  11 },
    { "iPhone 16 / 15",   196, 426, TALL,  24, 16, N_ISLAND, 14 },
    { "iPhone 16 Plus",   215, 466, TALL,  24, 16, N_ISLAND, 14 },
    { "iPhone 16 Pro Max", 220, 478, TALL, 26, 16, N_ISLAND, 15 },
    { "iPhone SE land",   334, 188, WIDE,   6,  0, N_HOME,    3 },
    { "iPhone 16 land",   426, 196, WIDE,   8, 12, N_ISLAND, 14 },   // island on the short side; we hint it top-center
    { "iPad mini",        372, 566, ROOMY, 12,  8, N_PAD,     9 },
    { "iPad 11\"",        417, 597, ROOMY, 12,  8, N_PAD,     9 },
    { "iPad 13\"",        516, 688, ROOMY, 12,  8, N_PAD,    10 },
    { "iPad 13\" land",   688, 516, ROOMY, 12,  8, N_PAD,    10 },
};
#define NDEV 11

// ───────── the rack inventory (acidfit's, + a compact middle state) ─────────
enum { KNOBS, DRUMS };
typedef struct { const char *name; int kind; int n; const char *const *labels; const char *const *compact; int nc; int haspat; } Strip;
static const char *const K303[]  = { "CUT","RES","ENV","DEC","ACC","DRV","SQL" };
static const char *const K303c[] = { "CUT","RES","DRV" };                 // §2 guess: the knobs you ride live
static const char *const KMST[]  = { "VOL","TON","VRB" };
static const char *const KMSTc[] = { "DLY","FB","GLU" };
// factual voice selectors (Roland manuals): TR-909 = 11 voices; TR-808 = 11 (toms↔congas,
// RS↔claves, CP↔maracas via a switch; AC/accent is a global track, not in the voice row).
static const char *const V909[]  = { "BD","SD","LT","MT","HT","RS","CP","CH","OH","CC","RC" };
static const char *const V808[]  = { "BD","SD","LT","MT","HT","RS","CP","CB","CY","OH","CH" };
static const char *const VDUMc[] = { "TUNE","DEC" };                      // §2 guess: selected-voice knobs
static Strip STRIP[] = {          // haspat: MASTER is the mixer/FX bus, not a pattern instrument
    { "303 A",  KNOBS, 7,  K303, K303c, 3, 1 },
    { "303 B",  KNOBS, 7,  K303, K303c, 3, 1 },
    { "909",    DRUMS, 11, V909, VDUMc, 2, 1 },
    { "808",    DRUMS, 11, V808, VDUMc, 2, 1 },
    { "MASTER", KNOBS, 3,  KMST, KMSTc, 3, 0 },
};
#define NSTRIP 5
#define STEPS 16

enum { FOLDED, COMPACT, EXPANDED };
#define FU 22.0f                 // a 44pt finger = 22 logical px at K=2 (constant — the point of the matrix)
#define NPAT 6                   // patterns per instrument (maker: back to 6 — 2×4/8 made the header too tall)
#define PATCOLS 6                // ONE row of 6, right-aligned (square cells, spacing kept on the left)

static int cur = 0, applied = -1, work = NSTRIP, showlabel = 1, safehint = 1;
static int g_boxpat = 0;   // pattern selector style this frame: 1 = boxed left panel (iPad), 0 = header row (phone)
static int focused = -1;   // FOCUS/fullscreen: strip index shown full-screen (X closes), or -1 = the rack
static int fingergrid = 0; // 'g': overlay a 1-finger (44pt = FU logical px) reference grid to eyeball fit
static int playing = 0;    // transport play/stop (tap the ▶/■ button)
// work: 0..NSTRIP-1 = that strip expanded; NSTRIP = ALL COMPACT (the iPad §4 headline)
// per-instrument state the main screen must carry: mute + which of the 6 patterns is live
static int muted[NSTRIP] = { 0, 0, 0, 1, 0 };   // 808 muted by default (shows the silenced look)
static int patn[NSTRIP]  = { 0, 2, 1, 3, 0 };   // current pattern 0..NPAT-1
static int pagesel[NSTRIP];                     // knob PAGE index (0..2) — the tiny header page button cycles it
#define NKPAGE 3                                 // placeholder page count until per-page knob sets are wired
static float level[NSTRIP];                      // per-instrument VOLUME — the header level knob (left of mute)
static int   rndseed[NSTRIP];                    // bumped each RND press so the re-roll changes
static int   g_shape = TALL;                     // this frame's device class (TALL/WIDE/ROOMY) — set in draw()

// ───────── immediate-mode click layer (touch + mouse) ─────────
// One pointer press per frame; hit(box) returns true once for the first control it lands in and
// CONSUMES it, so nested controls (M inside a header inside a strip) resolve specific→general by
// draw order. Single-touch taps synthesise the mouse, so this is one path for desktop + device.
static int m_press = 0, m_x = 0, m_y = 0;
static int clicked(Box b) {
    if (m_press && m_x >= b.x && m_x < b.x + b.w && m_y >= b.y && m_y < b.y + b.h) { m_press = 0; return 1; }
    return 0;
}

// ───────── control backing values (no sound — the wireframe just needs them to MOVE so a
// finger can grab and turn one; that's the only way finger-comfort is honest). Persist per
// strip across frames. kv = full/expanded + MASTER knobs; kvc = the compact 2-3 knobs. ─────────
static float kv[NSTRIP][7], kvc[NSTRIP][3], tempo = 0.55f;    // kv = full/expanded + MASTER knobs; kvc = compact 2-3
// ── PARKED IDEA · PAGED KNOBS (motionbox's model — prototyped + parked 2026-07-10) ─────────────────
// A thin page-tab strip that swaps what a fixed set of ~3 knobs controls (MAIN / SHP / FX …), so a
// tight strip reaches every param + fx without a wall of knobs — motionbox's PG_TONE/MOD/OSC/MIX
// applied here. It WORKS, and it's the clean answer to brief §2's "fixed vs per-machine knob count"
// (keep it fixed at 3, add pages). WHY PARKED: the tab strip costs a whole extra ROW of vertical
// budget, and acidrack stacks 5 strips on a phone — that row is too expensive HERE. It's a strong fit
// for a SINGLE dense panel with vertical room to spare; revisit for a cart like that. Same call sank
// a fullscreen [fx] panel (roomy send knobs) — not worth the surface in this wireframe. See
// acidrack-layout-brief.md §2. [fx] is left as an inert button for now.
// step-sequencer backing data — EDITABLE (tap a cell to toggle). Seeded once from the same
// deterministic patterns the wireframe drew before, so the look is unchanged until you edit.
static unsigned char dstep[NSTRIP][11][STEPS];    // drum: strip × voice × step (hit on/off)
static signed char   n303[NSTRIP][STEPS];         // 303: pitch row 0..14 per step
static unsigned char rest303[NSTRIP][STEPS], a303[NSTRIP][STEPS], s303[NSTRIP][STEPS],
                     tie303[NSTRIP][STEPS], len303[NSTRIP][STEPS]; // 303: rest / accent / slide / tie / len(gate)
static int p303_rest(int seed, int i);            // defined below — used here to seed n303/rest303
static int p303_note(int seed, int i);
static int p303_acc(int seed, int i);
static int p303_sld(int seed, int i);
static int uiseeded = 0;
static void wf_seed(void) {   // spread the values once so nothing sits dead at 0
    if (uiseeded) return; uiseeded = 1;
    for (int s = 0; s < NSTRIP; s++) {
        level[s] = 0.8f;                                     // per-instrument volume (near full)
        for (int i = 0; i < 7; i++) kv[s][i]  = 0.2f + 0.1f * ((s + i) % 6);
        for (int i = 0; i < 3; i++) kvc[s][i] = 0.3f + 0.15f * ((s + i) % 4);
        for (int v = 0; v < 11; v++) for (int i = 0; i < STEPS; i++)
            dstep[s][v][i] = (((i + v * 3 + 1) % 4) == 0) || (i == 6 && v % 2 == 0) || (i == 10 && v % 3 == 0);
        for (int i = 0; i < STEPS; i++) {
            n303[s][i]   = (signed char)(p303_note(s, i) % 15);
            rest303[s][i] = p303_rest(s, i); a303[s][i] = p303_acc(s, i); s303[s][i] = p303_sld(s, i);
            tie303[s][i] = (i % 5 == 2); len303[s][i] = (i % 3 != 0);   // placeholder tie/gate patterns
        }
    }
}

// ───────── strip-content wireframe bits ─────────
// A knob is now the REAL ui.h widget: fixed ~1-finger size + hit pad, drag-to-turn. Fixed size is
// the point (acidfit's "comfort threshold, not fit threshold") — if N knobs can't each get a finger
// in the row, they overlap, and THAT overflow is the finding (phone → editor swap, brief §3).
static void wf_knob(Box cell, float *v, const char *label) {
    int cx = (int)(cell.x + cell.w / 2), cy = (int)(cell.y + cell.h / 2);
    ui_knob(v, cx, cy, label);
}
static void wf_knobrow(Box area, const char *const *labels, int n, float *vals) {
    float gap = lay_clamp(FU * 0.12f, 1, 3);
    for (int i = 0; i < n; i++) { Box c = lay_grid(area, n, n, i, gap); if (c.w > 4 && c.h > 6) wf_knob(lay_inset(c, 0.5f), &vals[i], labels[i]); }
}
// the step lane: 16 steps grouped in 4s so downbeats read at a glance (acidrack-ui-research §3 —
// "the single biggest legibility win, costs nothing"). Grouping = a bigger gap between beats
// (Gestalt) + a warm downbeat marker; the ACTIVE steps (lime) stay dominant. mu = muted → grey.
// AUTO-FOLDS to 2×8 when a 1×16 row would push cells below the touch floor — i.e. on phones
// (research §3: naive 1×16 ≈ 23pt/step; portrait folds to 2×8, still chunked in 4s). iPad stays 1×16.
static void wf_steplane(Box area, int sidx, int mu) {
    int voice = 0;   // compact shows the SELECTED voice (selector not yet wired to change it — voice 0 for now)
    float g = 1, G = lay_clamp(FU * 0.18f, 2, 6);          // within-beat gap · between-beat gap (bigger)
    float cw1 = (area.w - 12 * g - 3 * G) / 16.0f;         // width a 1×16 cell would get
    int rows = (cw1 < 11 && area.h >= 16) ? 2 : 1;         // too narrow + room below → fold to 2×8
    int per = 16 / rows;
    float rh = (area.h - (rows - 1) * 2) / (float)rows;
    for (int r = 0; r < rows; r++) {
        int grps = per / 4;
        float cw = (area.w - (per - grps) * g - (grps - 1) * G) / (float)per; if (cw < 1) cw = 1;
        float x = area.x, y = area.y + r * (rh + 2);
        for (int c = 0; c < per; c++) {
            int i = r * per + c;
            Box cell = box(x, y, cw, rh);
            int on = dstep[sidx][voice][i];
            int off = (i % 4 == 0) ? CLR_DARK_BROWN : CLR_DARKER_GREY;   // downbeat gets a warm marker
            boxfill(cell, mu ? (on ? CLR_MEDIUM_GREY : CLR_DARKER_GREY) : (on ? CLR_LIME_GREEN : off));
            if (clicked(cell)) dstep[sidx][voice][i] = !on;
            x += cw + g; if (c % 4 == 3) x += G - g;        // widen the gap after each beat
        }
    }
}
static void wf_minipat(Box area, int sidx, int mu) {   // folded: a row of tiny on/off dots (readout of the live pattern)
    int drum = STRIP[sidx].kind == DRUMS;
    for (int i = 0; i < STEPS; i++) { Box s = lay_wrap(area, STEPS, i, FU * 0.2f, 1); if (s.w < 1) continue;
        int on = drum ? dstep[sidx][0][i] : !rest303[sidx][i];
        boxfill(lay_inset(s, 0.5f), on ? (mu ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_DARKER_BLUE); }
}
// per-instrument PATTERN selector for strip `idx`: NPAT numbered slots, live one lit; tap to select.
// SQUARE pattern cells, RIGHT-ALIGNED in `area` (so the left of the header stays free for the icon).
// Returns the grid's LEFT x (so a caller can tuck controls just left of the patterns).
static float wf_patterns(Box area, int idx, int cols) {
    int cur = patn[idx], mu = muted[idx];
    int rows = (NPAT + cols - 1) / cols;
    float gap = lay_clamp(FU * 0.09f, 1, 3);
    float cell = (area.h - (rows - 1) * gap) / (float)rows;               // square, sized to the height…
    float maxc = area.h * 0.75f; if (cell > maxc) cell = maxc;            // …capped ~75% of header height (maker: small + margins)
    float gw = cols * cell + (cols - 1) * gap;
    if (gw > area.w) { cell = (area.w - (cols - 1) * gap) / (float)cols; gw = area.w; }  // …and never wider than the area
    float gh = rows * cell + (rows - 1) * gap;
    float ox = area.x + area.w - gw;                                      // RIGHT-align the grid
    float oy = area.y + (area.h - gh) / 2;
    for (int i = 0; i < NPAT; i++) {
        int r = i / cols, c = i % cols;
        Box b = box(ox + c * (cell + gap), oy + r * (cell + gap), cell, cell);
        if (b.w < 3) continue;
        int on = (i == cur);
        boxfill(b, on ? (mu ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_DARK_GREY); boxrect(b, CLR_DARKER_GREY);
        if (b.h >= 7) { font(FONT_TINY); print_centered(str("%d", i + 1), (int)(b.x + b.w / 2), (int)(b.y + (b.h - 5) / 2), on ? CLR_BLACK : CLR_MEDIUM_GREY); }
        if (clicked(b)) patn[idx] = i;
    }
    return ox;
}
// the pattern selector as its OWN little box (ReBirth's per-machine PATTERN panel): a titled,
// bordered unit clearly grouped with the instrument. 3×2 grid of the 6 patterns.
static void wf_patbox(Box b, int idx) {
    boxfill(b, CLR_BROWNISH_BLACK); boxrect(b, muted[idx] ? CLR_DARK_RED : CLR_MEDIUM_GREY);
    Box grid; Box lab = lay_split(lay_inset(b, 1), EDGE_TOP, lay_clamp(FU * 0.26f, 4, 7), &grid);
    font(FONT_TINY); print("PAT", (int)lab.x + 1, (int)lab.y, CLR_MEDIUM_GREY);
    wf_patterns(grid, idx, 3);
}
// per-strip ICON placeholder — a coloured ~12px square standing in for a real 12×12 icon (not wired
// yet). Icons replace the "303 A"/"909"/"808" TEXT: a shorter header (more room for the pattern row)
// AND it sidesteps the Roland trademark names. Real icons drop in later; the square is the stand-in.
static const int ICOCLR[NSTRIP] = { CLR_LIME_GREEN, CLR_TRUE_BLUE, CLR_ORANGE, CLR_INDIGO, CLR_LIGHT_GREY };
static void wf_stripicon(Box hdr, int idx, float w) {
    int mu = muted[idx];
    Box ico = box(hdr.x + 2, hdr.y + (hdr.h - w) / 2, w, w);
    boxfill(ico, mu ? CLR_DARK_GREY : ICOCLR[idx]); boxrect(ico, mu ? CLR_RED : CLR_LIGHT_PEACH);
}
// the MUTE button DOUBLES as the per-instrument LEVEL fader (maker's idea, tr808's tap-vs-drag gesture):
// TAP = toggle mute · VERTICAL DRAG = ride volume. Volume shows as a fill behind the "M". Rightmost +
// always drawn, so it's a live channel fader no matter which page/state the strip is in.
static int mdrag = -1, mdrag_y0, mdrag_moved;
static void wf_mute(Box hdr, int idx) {
    int mu = muted[idx];
    float bw = lay_clamp(FU * 0.9f, 14, 22);
    Box m = lay_at(hdr, L_TR, bw, hdr.h - 2, 1);
    boxfill(m, CLR_BROWNISH_BLACK);
    int fh = (int)(level[idx] * (m.h - 2));                       // volume = a bottom-up fill (mini fader)
    if (fh > 0) boxfill(box(m.x + 1, m.y + m.h - 1 - fh, m.w - 2, fh), mu ? CLR_DARK_GREY : CLR_DARK_ORANGE);
    boxrect(m, mu ? CLR_RED : CLR_MEDIUM_GREY);
    font(FONT_TINY); print_centered("M", (int)(m.x + m.w / 2), (int)(m.y + (m.h - 5) / 2), mu ? CLR_RED : CLR_LIGHT_PEACH);
    // claim the press; a >4px vertical move → level drag, else (released without moving) → a mute tap
    if (m_press && m_x >= m.x && m_x < m.x + m.w && m_y >= m.y && m_y < m.y + m.h) {
        mdrag = idx; mdrag_y0 = m_y; mdrag_moved = 0; m_press = 0;
    }
    if (mdrag == idx) {
        if (mouse_down(0)) {
            int dy = mdrag_y0 - mouse_y();
            if (dy > 4 || dy < -4) mdrag_moved = 1;
            if (mdrag_moved) { level[idx] += dy / 40.0f;
                level[idx] = level[idx] < 0 ? 0 : level[idx] > 1 ? 1 : level[idx]; mdrag_y0 = mouse_y(); }
        } else {
            if (!mdrag_moved) muted[idx] = !muted[idx];          // released without dragging → it was a tap
            mdrag = -1;
        }
    }
}

// beat-grouped 16-step columns inside `row` (shared by the drum grid + 303 grid)
static void wf_steprow(Box row, int sidx, int voice, int mu) {
    float g = 1, G = lay_clamp(FU * 0.16f, 2, 5);
    float cw = (row.w - 12 * g - 3 * G) / 16.0f; if (cw < 1) cw = 1;
    float x = row.x;
    for (int i = 0; i < STEPS; i++) {
        Box cell = box(x, row.y, cw, row.h - 1);
        int on = dstep[sidx][voice][i];
        int off = (i % 4 == 0) ? CLR_DARK_BROWN : CLR_DARKER_GREY;
        boxfill(cell, mu ? (on ? CLR_MEDIUM_GREY : CLR_DARKER_GREY) : (on ? CLR_LIME_GREEN : off));
        if (clicked(cell)) dstep[sidx][voice][i] = !on;
        x += cw + g; if (i % 4 == 3) x += G - g;
    }
}
// the FULL drum grid: every voice a row (label + its 16 steps) — the whole pattern at once, the
// overview a phone can only get in focus/fullscreen (acidrack-ui-research §3 "full grid" on roomy).
static void wf_drumgrid(Box area, Strip *s, int mu) {
    int sidx = (int)(s - STRIP);
    float lblW = lay_clamp(FU * 1.4f, 14, 34);
    for (int v = 0; v < s->n; v++) {
        Box row = lay_grid(area, 1, s->n, v, 1);
        Box steps; Box lbl = lay_split(row, EDGE_LEFT, lblW, &steps);
        boxfill(lbl, CLR_DARKER_BLUE);
        font(FONT_TINY); print(s->labels[v], (int)lbl.x + 2, (int)(lbl.y + (lbl.h - 5) / 2), CLR_LIGHT_PEACH);
        wf_steprow(lay_pad(steps, 1, 0, 0, 0), sidx, v, mu);
    }
}
// the 303 step programmer as a mini piano-roll: one note per step over an octave of key-striped rows.
// a 303 pattern is MELODIC, not on/off: per step a note (pitch on a scale, over octaves), a gate
// (note / rest), plus ACCENT and SLIDE (glide) flags. These deterministic per-step generators stand
// in for real data so the editor reads like an acid line.
static int p303_rest(int seed, int i) { return ((i * 3 + seed) % 7) == 5; }
static int p303_note(int seed, int i) { int sc[7] = {0,3,5,7,10,12,15}; return sc[(i*2 + seed) % 7] + ((seed + i) % 3 == 0 ? 12 : 0); } // minor-ish + octave jumps
static int p303_acc (int seed, int i) { return (i % 4 == 0) || ((i * 5 + seed) % 6 == 1); }
static int p303_sld (int seed, int i) { return ((i * 2 + seed) % 5) == 2; }

// the FULL 303 step programmer: a pitch piano-roll (key-striped, spans >1 octave so OCTAVE reads) +
// an ACCENT lane + a SLIDE(glide) lane. The 303's real anatomy (acidrack-ui-research §2: note/accent/
// slide/gate layers), the melodic twin of the drum grid.
// the note ROLL only — piano-key striped; tap a row to place/move the note, tap its own row to rest.
static void wf_303notes(Box notes, int sidx, int mu) {
    int rows = 15;                                     // ~15 semitones → octave range is visible
    float rh = notes.h / (float)rows;
    for (int r = 0; r < rows; r++) {                   // piano-key striping so pitch reads
        int black = (r % 12 == 1 || r % 12 == 3 || r % 12 == 6 || r % 12 == 8 || r % 12 == 10);
        boxfill(box(notes.x, notes.y + r * rh, notes.w, rh - 0.5f), black ? CLR_BROWNISH_BLACK : CLR_DARKER_BLUE);
    }
    float g = 1, G = lay_clamp(FU * 0.16f, 2, 5);
    float cw = (notes.w - 12 * g - 3 * G) / 16.0f; if (cw < 1) cw = 1;
    float x = notes.x; float prevX = -1, prevY = -1;
    for (int i = 0; i < STEPS; i++) {
        Box col = box(x, notes.y, cw, notes.h);
        if (m_press && m_x >= col.x && m_x < col.x + col.w && m_y >= col.y && m_y < col.y + col.h) {
            int rr = rows - 1 - (int)((m_y - notes.y) / rh); if (rr < 0) rr = 0; if (rr >= rows) rr = rows - 1;
            if (!rest303[sidx][i] && n303[sidx][i] == rr) rest303[sidx][i] = 1;   // tap the note again → rest
            else { n303[sidx][i] = (signed char)rr; rest303[sidx][i] = 0; }       // else place/move it here
            m_press = 0;
        }
        if (!rest303[sidx][i]) {
            int note = n303[sidx][i];
            float ny = notes.y + (rows - 1 - note) * rh, ncx = x + cw / 2, ncy = ny + rh / 2;
            if (s303[sidx][i] && prevX >= 0)           // slide: tie a line from the previous note
                line((int)prevX, (int)prevY, (int)ncx, (int)ncy, mu ? CLR_MEDIUM_GREY : CLR_INDIGO);
            boxfill(box(x, ny, cw, rh - 0.5f), mu ? CLR_MEDIUM_GREY : (a303[sidx][i] ? CLR_ORANGE : CLR_LIME_GREEN));
            prevX = ncx; prevY = ncy;
        } else prevX = -1;
        x += cw + g; if (i % 4 == 3) x += G - g;
    }
}
// per-step FLAG lanes, tap a cell to toggle. n = 2 → ACC/SLD (the expanded editor); n = 4 →
// ACC/SLD/TIE/LEN (the compact FLAGS page — the maker's 303 page spec).
static void wf_303flags(Box area, int sidx, int mu, int n) {
    static const char *LN[4]  = { "ACC", "SLD", "TIE", "LEN" };
    static const int   COL[4] = { CLR_ORANGE, CLR_INDIGO, CLR_TRUE_BLUE, CLR_LIGHT_PEACH };
    unsigned char *lane[4] = { a303[sidx], s303[sidx], tie303[sidx], len303[sidx] };
    float g = 1, G = lay_clamp(FU * 0.16f, 2, 5), lblW = lay_clamp(FU * 0.9f, 10, 20);
    for (int L = 0; L < n; L++) {
        Box row = lay_grid(area, 1, n, L, 1);
        Box cells; Box lbl = lay_split(row, EDGE_LEFT, lblW, &cells);
        font(FONT_TINY); print(LN[L], (int)lbl.x + 1, (int)(lbl.y + (lbl.h - 5) / 2), COL[L]);
        float cw = (cells.w - 12 * g - 3 * G) / 16.0f; if (cw < 1) cw = 1; float x = cells.x;
        for (int i = 0; i < STEPS; i++) {
            Box c = box(x, cells.y + 1, cw, cells.h - 2);
            boxfill(c, mu ? CLR_DARKER_GREY : (lane[L][i] ? COL[L] : CLR_DARK_GREY));
            if (clicked(c)) lane[L][i] = !lane[L][i];
            x += cw + g; if (i % 4 == 3) x += G - g;
        }
    }
}
static void wf_303grid(Box area, int sidx, int mu) {   // expanded editor: the roll + ACC/SLD lanes
    Box notes; Box flags = lay_split(area, EDGE_BOTTOM, lay_clamp(FU * 0.9f, 8, 22), &notes);
    wf_303notes(notes, sidx, mu);
    wf_303flags(flags, sidx, mu, 2);
}
// compact 303 preview: the melodic contour — a pitch bar per step (accent = orange, rest = empty).
static void wf_303lane(Box area, int sidx, int mu) {
    float g = 1, G = lay_clamp(FU * 0.16f, 2, 5);
    float cw = (area.w - 12 * g - 3 * G) / 16.0f; if (cw < 1) cw = 1;
    float x = area.x;
    for (int i = 0; i < STEPS; i++) {
        Box cell = box(x, area.y, cw, area.h);            // full-height tap target: tap toggles this step's rest
        if (!rest303[sidx][i]) {
            float f = n303[sidx][i] / 14.0f;              // pitch → bar height
            float h = 2 + f * (area.h - 2);
            boxfill(box(x, area.y + area.h - h, cw, h), mu ? CLR_MEDIUM_GREY : (a303[sidx][i] ? CLR_ORANGE : CLR_LIME_GREEN));
        }
        if (clicked(cell)) rest303[sidx][i] = !rest303[sidx][i];
        x += cw + g; if (i % 4 == 3) x += G - g;
    }
}
// RND re-rolls the pattern, CLR empties it. Content ops (acidrack ships CPY/CLR/RND); wired to the
// wireframe data so they're real. Only shown WHERE YOU EDIT: landscape header + portrait fullscreen.
static void rnd_303(int idx) {
    rndseed[idx]++; int s = idx * 13 + rndseed[idx] * 7 + 1;
    for (int i = 0; i < STEPS; i++) { n303[idx][i] = (signed char)(p303_note(s, i) % 15); rest303[idx][i] = p303_rest(s, i);
        a303[idx][i] = p303_acc(s, i); s303[idx][i] = p303_sld(s, i); tie303[idx][i] = (i % 5 == 2); len303[idx][i] = (i % 3 != 0); }
}
static void clr_303(int idx) { for (int i = 0; i < STEPS; i++) { rest303[idx][i] = 1; a303[idx][i] = s303[idx][i] = tie303[idx][i] = 0; len303[idx][i] = 1; } }
static void rnd_drum(int idx) {
    rndseed[idx]++; int s = idx * 13 + rndseed[idx] * 7 + 1;
    for (int v = 0; v < 11; v++) for (int i = 0; i < STEPS; i++) dstep[idx][v][i] = (((i + v * 3 + s) % 4) == 0) || ((i * 5 + v + s) % 9 == 0);
}
static void clr_drum(int idx) { for (int v = 0; v < 11; v++) for (int i = 0; i < STEPS; i++) dstep[idx][v][i] = 0; }
// RND | CLR — two side-by-side buttons filling `area`, dispatched by instrument kind.
static void wf_ops(Box area, int idx) {
    int drum = STRIP[idx].kind == DRUMS;
    Box r = lay_grid(area, 2, 2, 0, 2), c = lay_grid(area, 2, 2, 1, 2);
    boxfill(r, CLR_DARK_GREY); boxrect(r, CLR_MEDIUM_GREY);
    font(FONT_TINY); print_centered("RND", (int)(r.x + r.w / 2), (int)(r.y + (r.h - 5) / 2), CLR_LIGHT_PEACH);
    if (clicked(r)) { if (drum) rnd_drum(idx); else rnd_303(idx); }
    boxfill(c, CLR_DARK_GREY); boxrect(c, CLR_MEDIUM_GREY);
    print_centered("CLR", (int)(c.x + c.w / 2), (int)(c.y + (c.h - 5) / 2), CLR_LIGHT_PEACH);
    if (clicked(c)) { if (drum) clr_drum(idx); else clr_303(idx); }
}
// the shared strip HEADER — icon · [page (compact-303)] · [ops (landscape)] · patterns (right-aligned
// squares) · mute/fader (rightmost). IDENTICAL in the in-rack strip and the fullscreen FOCUS bar (no
// jump, no leading '<'). Returns 1 if the ICON (or empty header) was tapped — caller opens/leaves.
static int wf_header(Box hdr, int idx, int state, int accent, int hdrops) {
    Strip *s = &STRIP[idx]; int mu = muted[idx];
    boxfill(hdr, mu ? CLR_DARK_RED : (accent ? CLR_TRUE_BLUE : CLR_DARK_GREY));
    float icoW = lay_clamp(FU * 0.6f, 12, 16); if (icoW > hdr.h - 4) icoW = hdr.h - 4;
    wf_stripicon(hdr, idx, icoW);
    wf_mute(hdr, idx);                                         // rightmost (also the level fader) — claims its tap/drag first
    float hleft = hdr.x + icoW + 4;
    if (state == COMPACT && s->kind == KNOBS && s->haspat) {   // N/K/F page button — compact 303 only
        float pgW = icoW; Box pgb = box(hleft, hdr.y + (hdr.h - pgW) / 2, pgW, pgW);
        boxfill(pgb, CLR_DARK_GREY); boxrect(pgb, CLR_MEDIUM_GREY);
        static const char *PGL[NKPAGE] = { "N", "K", "F" };
        font(FONT_TINY); print_centered(PGL[pagesel[idx] % NKPAGE], (int)(pgb.x + pgb.w / 2), (int)(pgb.y + (pgb.h - 5) / 2), CLR_LIGHT_PEACH);
        if (clicked(pgb)) pagesel[idx] = (pagesel[idx] + 1) % NKPAGE;
        hleft = pgb.x + pgW + 4;
    }
    if (s->haspat && (!g_boxpat || state == FOLDED)) {         // patterns: right-aligned row of squares
        float muteW = FU * 1.6f;                               // reserve mute/fader + a margin before it
        Box pb = box(hleft, hdr.y + 1, hdr.x + hdr.w - muteW - hleft, hdr.h - 2);
        float patLeft = wf_patterns(pb, idx, PATCOLS);
        if (hdrops) {   // landscape: RND|CLR tucked just LEFT of the patterns (margin between) — frees the after-icon slot
            float ow = lay_clamp(FU * 1.6f, 24, 40), mg = lay_clamp(FU * 0.7f, 8, 14);
            wf_ops(box(patLeft - mg - ow, hdr.y + 2, ow, hdr.h - 4), idx);
        }
    }
    // FALLBACK: a tap anywhere else on the header (the icon or its empty space) = tapping the icon.
    // Buttons above already consumed their taps, so this only fires on the non-button area.
    return clicked(hdr);
}
// FOCUS / fullscreen: one instrument fills the area under the SAME header (icon tap = leave).
static void draw_focus(Strip *s, Box area, int idx) {
    int mu = muted[idx];
    boxfill(area, CLR_DARKER_BLUE);                          // no outer rect/inset — matches the strip so focus↔rack doesn't jump
    Box body; Box bar = lay_split(lay_inset(area, 1), EDGE_TOP, lay_clamp(FU * 1.0f, 18, 24), &body);
    // ops placement follows ORIENTATION, not focus: landscape (width-rich) → IN the header; portrait → a sub-row.
    if (wf_header(bar, idx, EXPANDED, 1, g_shape == WIDE)) focused = -1;
    if (s->haspat && g_shape != WIDE) {   // portrait fullscreen only — a thin RND|CLR sub-row under the header
        Box ops = lay_split(body, EDGE_TOP, lay_clamp(FU * 0.7f, 9, 16), &body);
        wf_ops(lay_pad(ops, 0, 1, 0, 1), idx);
    }
    body = lay_pad(body, 1, 2, 1, 1);
    if (s->kind == DRUMS) wf_drumgrid(body, s, mu);          // the full voices×steps overview
    else if (s->haspat) { Box grid; Box kn = lay_split(body, EDGE_BOTTOM, FU * 1.6f, &grid); wf_303grid(grid, idx, mu); wf_knobrow(kn, s->labels, s->n, kv[idx]); }
    else                  wf_knobrow(body, s->labels, s->n, kv[idx]); // MASTER: just knobs
}

// draw one strip at `state` inside `rect`
static void draw_strip(Strip *s, Box rect, int state, int accent) {
    int idx = (int)(s - STRIP), mu = muted[idx], pc = patn[idx];
    boxfill(rect, CLR_DARKER_BLUE); boxrect(rect, mu ? CLR_RED : (accent ? CLR_TRUE_BLUE : CLR_DARK_GREY));
    Box body; float hh = lay_clamp(FU * 1.0f, 18, 24);   // a tad bigger than the old header (was 0.85/16–22)
    Box hdr = lay_split(lay_inset(rect, 1), EDGE_TOP, hh, &body);
    // shared header (same in fullscreen); the icon tap opens/expands the strip.
    // landscape's open strip has header room for RND|CLR; portrait/iPad get them via focus's sub-row.
    int hdrops = (g_shape == WIDE && state == EXPANDED);
    if (wf_header(hdr, idx, state, accent, hdrops)) { if (state == EXPANDED) focused = idx; else work = idx; }
    body = lay_pad(body, 1, 1, 1, 1);
    int box_pat = s->haspat && g_boxpat && state != FOLDED;   // dormant (g_boxpat = 0)

    // MASTER (no patterns) has no step sequence either — just its knobs, no lane / preview.
    if (state == FOLDED) { if (s->haspat) wf_minipat(body, idx, mu); return; }

    Box mach = body;
    if (box_pat) {   // iPad: the pattern selector as its own bordered panel on the left
        float boxW = (state == EXPANDED) ? FU * 2.7f : FU * 2.4f;
        Box pbox = lay_split(body, EDGE_LEFT, boxW, &mach);
        wf_patbox(pbox, idx);
        mach = lay_pad(mach, 1, 0, 0, 0);   // gap between the box and the machine
    }

    if (state == COMPACT) {                        // 2-3 knobs (or drum selector) + one step lane
        if (s->kind == KNOBS && s->haspat) {       // 303 COMPACT: page the whole body (maker's spec)
            int pg = pagesel[idx] % NKPAGE;
            if      (pg == 0) wf_303notes(mach, idx, mu);                 // N — just the notes
            else if (pg == 1) wf_knobrow(mach, s->labels, s->n, kv[idx]); // K — just the knobs (all 7)
            else              wf_303flags(mach, idx, mu, 4);              // F — ACC/SLD/TIE/LEN
            return;
        }
        Box lane; Box top = s->haspat ? lay_split(mach, EDGE_TOP, FU * 1.3f, &lane) : mach;
        if (s->kind == KNOBS) wf_knobrow(top, s->compact, s->nc, kvc[idx]);
        else { // drum: voice selector (ALL voices, factual) + selected-voice knobs
            Box kn; Box sel = lay_split(top, EDGE_TOP, FU * 0.7f, &kn);
            float g = 1.5f; for (int i = 0; i < s->n; i++) { Box c = lay_grid(sel, s->n, s->n, i, g); boxfill(lay_inset(c,0.5f), i==0?CLR_DARK_ORANGE:CLR_DARK_GREY);
                if (c.w >= 7) { font(FONT_TINY); print_centered(s->labels[i], (int)(c.x+c.w/2), (int)(c.y+(c.h-5)/2), CLR_LIGHT_GREY); } }
            wf_knobrow(kn, s->compact, s->nc, kvc[idx]);
        }
        if (s->haspat) { if (s->kind == KNOBS) wf_303lane(lay_pad(lane, 0, 1, 0, 1), idx, mu);   // 303: melodic contour
                         else wf_steplane(lay_pad(lane, 0, 1, 0, 1), idx, mu); }                 // drum: selected voice
        return;
    }
    // EXPANDED — the full editor
    if (s->kind == KNOBS) {
        if (s->haspat) { Box lane; Box kn = lay_split(mach, EDGE_BOTTOM, FU * 1.4f, &lane); wf_knobrow(kn, s->labels, s->n, kv[idx]); wf_303grid(lay_pad(lane,0,1,0,1), idx, mu); }   // 303: full programmer
        else wf_knobrow(mach, s->labels, s->n, kv[idx]);   // MASTER: just knobs
    }
    else wf_drumgrid(mach, s, mu);   // DRUMS: the real voices×steps sequencer grid
}

// ───────── device safe-area SKIN (the point of this whole cart) ─────────
#define MINI(a,b) ((a) < (b) ? (a) : (b))
// black the pixels OUTSIDE the corner arc (the rounded screen corner is "off screen")
static void round_corner(int ox, int oy, int r, int sx, int sy) {
    for (int y = 0; y < r; y++) for (int x = 0; x < r; x++) {
        int dx = r - x, dy = r - y;
        if (dx * dx + dy * dy > r * r) pset(ox + sx * x, oy + sy * y, CLR_BLACK);
    }
}
static void hpill(float x, float y, float w, float h, int col) {   // rounded-end horizontal pill
    int r = (int)(h / 2);
    boxfill(box(x + r, y, w - 2 * r, h), col);
    circfill((int)x + r, (int)y + r, r, col); circfill((int)(x + w) - r, (int)y + r, r, col);
}
static void dashrect(Box b, int col) {   // dashed outline = "keep controls inside here"
    int d = 4, g = 3;
    for (int x = (int)b.x; x < b.x + b.w; x += d + g) { line(x, (int)b.y, (int)MINI(x + d, b.x + b.w), (int)b.y, col); line(x, (int)(b.y + b.h) - 1, (int)MINI(x + d, b.x + b.w), (int)(b.y + b.h) - 1, col); }
    for (int y = (int)b.y; y < b.y + b.h; y += d + g) { line((int)b.x, y, (int)b.x, (int)MINI(y + d, b.y + b.h), col); line((int)(b.x + b.w) - 1, y, (int)(b.x + b.w) - 1, (int)MINI(y + d, b.y + b.h), col); }
}
// draw the device hardware over the rendered rack: status band + notch/island + home bar + rounded
// corners + the safe-area boundary. Everything here is a KEEP-OUT the layout must dodge.
static void draw_safe_skin(int W, int H, Dev d) {
    // reserved status/home bands (faint) — iOS draws the clock/battery/home-affordance here
    if (d.insT > 0) boxfill(box(0, 0, W, d.insT), CLR_DARKER_PURPLE);
    if (d.insB > 0) boxfill(box(0, H - d.insB, W, d.insB), CLR_DARKER_PURPLE);
    // status-bar mock (shows the band is real estate the app doesn't own)
    if (d.insT >= 9) { font(FONT_TINY);
        print("9:41", 4, (int)(d.insT - 5) / 2, CLR_LIGHT_GREY);
        boxfill(box(W - 12, (d.insT - 5) / 2, 9, 5), CLR_DARK_GREY); boxfill(box(W - 12, (d.insT - 5) / 2 + 1, 7, 3), CLR_LIME_GREEN); }
    // top cutout hardware (black = a hole in the glass)
    int cx = W / 2;
    if (d.notch == N_NOTCH) {
        int nw = (int)(W * 0.46f), nh = d.insT;
        boxfill(box(cx - nw / 2, 0, nw, nh), CLR_BLACK);
        circfill(cx - nw / 2, nh, 3, CLR_BLACK); circfill(cx + nw / 2, nh, 3, CLR_BLACK);   // rounded bottom
        boxfill(box(cx - nw / 2 - 3, nh - 3, 3, 3), CLR_BLACK); boxfill(box(cx + nw / 2, nh - 3, 3, 3), CLR_BLACK);
    } else if (d.notch == N_ISLAND) {
        int iw = (int)(W * 0.30f), ih = (int)(d.insT * 0.55f); if (ih < 6) ih = 6;
        hpill(cx - iw / 2, 4, iw, ih, CLR_BLACK);
    }
    // home indicator (no home-button era)
    if (d.notch != N_HOME) { int hw = (int)(W * (d.cls == WIDE ? 0.22f : 0.34f));
        hpill(cx - hw / 2, H - 5, hw, 3, CLR_LIGHT_GREY); }
    // rounded screen corners
    int r = d.cornR;
    round_corner(0, 0, r, 1, 1); round_corner(W - 1, 0, r, -1, 1);
    round_corner(0, H - 1, r, 1, -1); round_corner(W - 1, H - 1, r, -1, -1);
    // the safe-area boundary — put every control INSIDE this
    dashrect(box(2, d.insT, W - 4, H - d.insT - d.insB), CLR_ORANGE);
}
// a right-pointing play triangle filling box b (rows narrowing to the tip)
static void play_tri(Box b, int col) {
    int n = (int)b.h; if (n < 1) n = 1;
    for (int i = 0; i < n; i++) {
        float d = fabsf(i - (n - 1) / 2.0f) / ((n - 1) / 2.0f + 0.001f);
        line((int)b.x, (int)b.y + i, (int)b.x + (int)(b.w * (1.0f - d)), (int)b.y + i, col);
    }
}
// 'g' overlay: a 1-finger reference grid (FU = 44pt = 22 logical px cells). Any control smaller than
// one cell is below the comfortable finger target — eyeball fit across shapes without measuring.
static void draw_fingergrid(int W, int H) {
    for (float x = FU; x < W; x += FU) for (int y = 0; y < H; y += 3) pset((int)x, y, CLR_INDIGO);
    for (float y = FU; y < H; y += FU) for (int x = 0; x < W; x += 3) pset(x, (int)y, CLR_INDIGO);
    boxfill(box(3, 3, FU, FU), CLR_INDIGO);   // one solid finger swatch = the target size
    font(FONT_TINY); print("1 finger", 3, (int)FU + 4, CLR_INDIGO);
}

// footprint of a strip in a given state, in logical px (height)
static float strip_h(Strip *s, int state) {
    if (state == FOLDED)  return FU * 1.5f;               // finger-tall header + a slim preview strip
    if (state == COMPACT) return FU * 3.0f;              // header + the paged body
    return (s->kind == DRUMS ? FU * 5.8f : FU * 5.0f);   // EXPANDED
}

void update(void) {
    int i;
    if (keyp(KEY_RIGHT) || keyp(']') || keyp(KEY_SPACE) || keyp(KEY_GRAVE)) cur = (cur + 1) % NDEV;
    if (keyp(KEY_LEFT)  || keyp('[')) cur = (cur + NDEV - 1) % NDEV;
    for (i = 0; i < 9 && i < NDEV; i++) if (keyp('1' + i)) cur = i;
    // NOTE: raylib letter keycodes are UPPERCASE (KEY_W==87), so read 'W' not 'w' — a lowercase
    // literal (119) never matches and the key silently does nothing (this bit: w/f/m/p/s/h were dead).
    if (keyp('W') || keyp(KEY_DOWN)) work = (work + 1) % (NSTRIP + 1);
    if (keyp('H')) showlabel = !showlabel;
    if (keyp('S')) safehint = !safehint;
    if (keyp('G')) fingergrid = !fingergrid;
    // per-instrument controls, acting on the working strip (or 303-A when all-compact)
    int sel = (work < NSTRIP) ? work : 0;
    if (keyp('M')) muted[sel] = !muted[sel];              // (un)mute
    if (keyp('P')) patn[sel] = (patn[sel] + 1) % NPAT;    // cycle its 6 patterns
    if (keyp('F')) focused = (focused == sel) ? -1 : sel; // FOCUS the working strip / X-close
#ifndef DE_RESIZABLE
    if (cur != applied) { de_resize(DEV[cur].w, DEV[cur].h); applied = cur; }   // design tool: force the fake device size
#endif
}

void draw(void) {
    Dev d = DEV[cur];
    int W = screen_w(), H = screen_h();
    // DUAL MODE. DESKTOP (design tool): the canvas is a FAKE device (de_resize'd to DEV[cur]); use its
    // baked class + insets + draw the simulated skin/label. DEVICE (-DDE_RESIZABLE, iOS/real screen):
    // the canvas IS the device — classify the REAL W/H and read the REAL safe-area; no fake skin/flip.
    int cls_, insT, insB;
#ifdef DE_RESIZABLE
    int device_mode = 1;
    cls_ = disclose_shape(W, H);   // TALL/WIDE/ROOMY — same enum values as disclose's (0/1/2)
    { int sx, sy, sw, sh; safe_rect(&sx, &sy, &sw, &sh); insT = sy; insB = H - (sy + sh); }   // top/bottom only (brief §4)
#else
    int device_mode = 0;
    cls_ = d.cls; insT = d.insT; insB = d.insB;
#endif
    g_shape = cls_;   // expose this frame's device class to draw_strip/wf_header (landscape ops etc.)
    g_boxpat = 0;   // patterns live in the HEADER ROW on every device (maker, on iPad glass 2026-07-10) —
                    // preferred over the boxed left PAT panel even on roomy; the box path stays dormant below
    m_press = mouse_pressed(0); m_x = mouse_x(); m_y = mouse_y();   // one pointer press per frame (tap/click)
    cls(CLR_BROWNISH_BLACK);
    wf_seed(); ui_begin();   // ui.h widgets (knobs/slider) live between ui_begin()…ui_end(); the
                             // clicked() tap layer (names/M/patterns) is spatially disjoint, so both coexist

    // safe area (top/bottom insets — simulated on desktop, real on device). The fake device SKIN is
    // drawn last (draw_safe_skin), and only in design-tool mode; on device the real hardware provides it.
    Box full = box(0, 0, W, H);
    Box safe = lay_pad(full, insT, 0, insB, 0);

    // pinned chrome: just the transport (top). No song-chain row + no A/B/C/D banks for now —
    // we're always in LOOP mode (maker, 2026-07-07); the strips get the reclaimed height.
    // landscape (WIDE) has little vertical budget, and the transport carries little — keep it slim there.
    // FLOOR 20px: the multicart Tinyjam HOME hold-pad (build-app.js HOME_HIT_H = 20, top toolbar band) must
    // seat cleanly here — it's a HOLD gesture that shares this band with the transport, so the band must be
    // ≥ its 20px or the pad bleeds into the strips. (Portrait stays taller.)
    float trH = (cls_ == WIDE) ? lay_clamp(FU * 0.95f, 20, 24) : lay_clamp(FU * 1.15f, 12, 30);
    Box afterTr;
    Box transport = lay_split(safe, EDGE_TOP, trH, &afterTr);
    Box bodyarea  = lay_pad(afterTr, 2, 1, 2, 1);

    boxfill(transport, CLR_TRUE_BLUE);
    // play/stop toggle (drawn icon, tappable) + tempo + LOOP mode
    float bs = lay_clamp(trH * 0.7f, 8, 22);
    Box pbtn = box(transport.x + 3, transport.y + (transport.h - bs) / 2, bs, bs);
    boxfill(pbtn, CLR_DARKER_BLUE); boxrect(pbtn, CLR_LIGHT_PEACH);
    if (playing) boxfill(box(pbtn.x + bs * 0.3f, pbtn.y + bs * 0.3f, bs * 0.4f, bs * 0.4f), CLR_LIGHT_PEACH);   // ■ stop
    else         play_tri(lay_inset(pbtn, bs * 0.28f), CLR_LIME_GREEN);                                        // ▶ play
    if (clicked(pbtn)) playing = !playing;
    // TEMPO — a real ui_slider (drag to set) so sliders get finger-tested too; label shows live BPM.
    { font(FONT_TINY);
      int bpm = 90 + (int)(tempo * 90 + 0.5f);                       // 90..180 BPM
      int slw = (int)lay_clamp(FU * 3.2f, 44, 96);
      ui_slider(&tempo, (int)(pbtn.x + bs + 5), (int)(transport.y + (transport.h - UI_SLIDER_H) / 2), slw, str("%d BPM", bpm)); }
    print_right("LOOP", (int)(transport.x + transport.w - 3), (int)(transport.y + (transport.h - 6) / 2), CLR_LIGHT_GREY);

    float gap = lay_clamp(FU * 0.18f, 1, 4);
    const char *mode;

    if (focused >= 0) {
        // ─── FOCUS: one instrument fills the body, X closes back to the rack ───
        mode = "FOCUS (tap name / f to leave)";
        draw_focus(&STRIP[focused], bodyarea, focused);
    } else if (cls_ == WIDE) {
        // ─── short-wide: TABS (accordions degenerate short — acidfit finding) ───
        mode = "tabs";
        int sel = (work >= NSTRIP) ? 0 : work;
        Box panel; Box tabs = lay_split(bodyarea, EDGE_TOP, FU * 0.85f, &panel);   // slim tabs — landscape is tight
        for (int i = 0; i < NSTRIP; i++) {
            Box t = lay_grid(tabs, NSTRIP, NSTRIP, i, gap);
            int m = muted[i];
            boxfill(t, i == sel ? CLR_TRUE_BLUE : (m ? CLR_DARK_RED : CLR_DARK_GREY));
            boxrect(t, m ? CLR_RED : CLR_MEDIUM_GREY);
            // just the NAME now (kept for now; maker will iconize later). NO tab-mute — mute lives in
            // the header of the open strip (a tab-mute would double it). Whole tab taps to open.
            font(FONT_TINY); print_centered(STRIP[i].name, (int)(t.x + t.w / 2), (int)(t.y + (t.h - 5) / 2), CLR_LIGHT_PEACH);
            if (clicked(t)) work = i;
        }
        draw_strip(&STRIP[sel], lay_pad(panel, 0, gap, 0, 0), EXPANDED, 1);

    } else if (cls_ == ROOMY) {
        // ─── iPad (either orientation): 2×2 grid of the 4 pattern machines + a MASTER bar (maker,
        //     2026-07-07). Squarer panels that fill the space in BOTH orientations — no over-wide
        //     lanes (landscape) and no ~40% dead void (portrait's old all-compact stack). ReBirth's
        //     machines-grid + master rail. Every machine shows its full editor at once.
        mode = (W > H) ? "roomy land · 2x2 + master" : "roomy tall · 2x2 + master";
        Box grid; Box mbar = lay_split(bodyarea, EDGE_BOTTOM, FU * 2.0f, &grid);
        for (int i = 0; i < 4; i++)   // 303A · 303B / 909 · 808
            draw_strip(&STRIP[i], lay_grid(grid, 2, 4, i, gap), EXPANDED, i == work);
        draw_strip(&STRIP[4], lay_pad(mbar, 0, gap, 0, 0), EXPANDED, work == 4);   // MASTER: knobs only

    } else {
        // ─── tall (phone portrait): working EXPANDED + compact + folded, by budget ───
        mode = "tall · expand+compact+fold";
        int sel = (work >= NSTRIP) ? 0 : work;
        // the disclosure pass now lives in disclose.h (R2). Declare each strip's per-state footprints +
        // priority (lower index promotes first), let disclose_budget pick states + disclose_stack place them.
        DiscSection sec[NSTRIP];
        for (int i = 0; i < NSTRIP; i++) {
            sec[i].prio = NSTRIP - i; sec[i].pinned = 0;
            sec[i].foot[DISC_FOLDED]   = strip_h(&STRIP[i], FOLDED);
            sec[i].foot[DISC_COMPACT]  = strip_h(&STRIP[i], COMPACT);
            sec[i].foot[DISC_EXPANDED] = strip_h(&STRIP[i], EXPANDED);
        }
        int st[NSTRIP]; disclose_budget(sec, NSTRIP, sel, bodyarea.h, gap, st);
        DiscPlace pl[NSTRIP]; disclose_stack(bodyarea, sec, NSTRIP, st, gap, pl);
        for (int i = 0; i < NSTRIP; i++) draw_strip(&STRIP[i], pl[i].box, st[i], i == sel);
    }

    // ─── device safe-area skin over the rack (notch/island/corners/home bar) — toggle s ───
    if (!device_mode && safehint) draw_safe_skin(W, H, d);
    if (fingergrid) draw_fingergrid(W, H);

    // ─── tool label (inside the device; toggle with h) ───
    if (!device_mode && showlabel) {
        font(FONT_TINY);
        const char *l1 = str("%s  %dx%d  %s", d.name, d.w, d.h, mode);
        const char *l2 = str("%.1fx%.1f fingers  ]/[ shape  w strip  h hide", d.w / FU, d.h / FU);
        int lw = 8 + (int)(strlen(l1) > strlen(l2) ? strlen(l1) : strlen(l2)) * 4;
        Box chip = box(2, H - d.insB - 20, lw < W - 4 ? lw : W - 4, 16);   // bottom-left: keeps transport + notch visible up top
        boxfill(chip, CLR_BLACK); boxrect(chip, CLR_ORANGE);
        print(l1, (int)chip.x + 3, (int)chip.y + 2, CLR_LIGHT_PEACH);
        print(l2, (int)chip.x + 3, (int)chip.y + 9, CLR_ORANGE);
        if (clicked(chip)) cur = (cur + 1) % NDEV;   // tap the label to flip to the next device shape
    }
    ui_end();   // resolve this frame's presses against the widgets drawn above
    font(FONT_NORMAL);
}
