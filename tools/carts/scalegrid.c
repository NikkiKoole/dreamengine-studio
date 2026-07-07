/* de:meta
{
  "title": "scalegrid",
  "status": "active",
  "created": "2026-07-07",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "scale-quantize",
    "analog-voice-modeling"
  ],
  "description": {
    "summary": "A scale-locked ISOMORPHIC pad grid you play with your fingers - pick a scale and there are NO WRONG NOTES. Voiced by INSTR_PD (the Casio-CZ phase-distortion engine) on a lush 'wowww' sweep pad, so every tap and chord rings. An on-screen chip bar makes it fully playable on a phone.",
    "detail": "The playable showcase for the scale-grid feature (design/scale-grid.md). Pick a SCALE and a KEY (root) from the on-screen control bar and the grid shows ONLY in-scale notes - so any finger, any chord, any run sounds musical. 11 curated scales (chroma, major, natural minor, minor + major pentatonic, dorian, whole-tone, harmonic minor, hirajoshi, blues, and FOREST - the open/spread 'SoundForest' voicing whose top note reaches past the octave). It is ISOMORPHIC: the ROW OFFSET is a number of scale DEGREES, so a chord SHAPE is identical in every key and octave AND survives a reflow. That offset is a LIVE DIAL (the ADR-0028 knob) - tap ROW to sweep it (ROW:OCT = up an octave per row with roots in a clean left column; ROW:4th / +N = tighter diagonal chord shapes) and hear the anatomy change. Roots are tinted orange. Pads are finger-sized (never crammed): tight screens PACK 1-finger pads, roomy screens CAP the range to 4 octaves and GROW big comfy squares. Fully polyphonic multitouch - every finger holds its own sustained INSTR_PD voice; slide a finger across pads to glide a run. It cold-opens PLAYING ITSELF (a gentle walking arpeggio) so a stranger hears the idea before touching it; the first tap hands control over. The grid maths (scale-lock + isomorphic degree offset + cap-and-grow finger sizing) is lifted from the epianofit mock and kept self-contained here, ready to graduate into a grid.h library.",
    "controls": "TAP / drag pads to play (mouse = one finger, multitouch = many). On-screen chips: SCALE, KEY, ROW (isomorphic row offset), OCT-/OCT+, AUTO (self-play). Keyboard mirrors them: S scale, R key, I row-offset, Z/X octave, M self-play, H help."
  }
}
de:meta */
// scalegrid — a scale-locked isomorphic pad grid, voiced by INSTR_PD (Casio CZ).
//
// The playable showcase for design/scale-grid.md — the feature the epianofit mock
// prototyped SILENTLY. Here it makes sound: pick a scale, and the grid shows only
// in-scale notes (no wrong notes), each pad a sustained phase-distortion pad voice.
//
// WHY ISOMORPHIC: the row offset is a fixed number of scale DEGREES, so a chord/scale
// shape is the SAME in every key and octave — and survives a reflow (a column-wrap
// layout's shapes shift when the grid resizes). ISO-OCT (row up = +1 octave, roots in
// a clean left column) is the legible default; ISO-4TH (row up = +a fourth) gives
// diagonal chord shapes. Per ADR-0028: ship a sensible default, keep the seam.
//
// SIZING (Law: grow → reveal more, never shrink below a finger): tight screens pack
// 1-finger pads; roomy screens cap the range to GRID_MAX_OCT octaves and grow big
// comfy squares. The maths is lifted verbatim from epianofit.c's keybed_grid() and
// kept self-contained, ready to lift into a grid.h library (scale-grid.md §3).
//
// SOUND: INSTR_PD "sweep pad" preset (reso-tri wave, full DCW "wowww" — from the pd
// cart). Fully polyphonic — each finger holds its own note_on() voice until it lifts;
// slide across pads to glide. It self-plays a walking arpeggio on load (the delightful
// cold-open); the first input hands control to the player.

#include "studio.h"

// ── scales (semitone degrees within an octave). CHROMA = all 12. ─────────────
// Our OWN curated set (not Koala's list/order): the standard theory scales every
// tool shares, plus a few high-value colours. Deliberately NOT mirroring Koala's
// niche picks — the "eastern" flavour is HIRAJOSHI (a specifically-named Japanese
// pentatonic), not a generic "JAPANESE" label. FOREST is the SoundForest scale
// (from melodypaint's research, vector-sketch/lib/melody-paint-audio-helper.lua):
// an OPEN VOICING whose top note (16 = a maj3 an octave up) gives it that lush
// ambient ring — a scale value deliberately reaching past the octave.
#define NSCALE 11
static const int SC_N[NSCALE]   = { 12, 7, 7, 5, 7, 5, 6, 7, 5, 6, 6 };
static const int SC[NSCALE][12] = {
    { 0,1,2,3,4,5,6,7,8,9,10,11 },   // chromatic
    { 0,2,4,5,7,9,11 },              // major (ionian)
    { 0,2,3,5,7,8,10 },              // natural minor (aeolian)
    { 0,3,5,7,10 },                  // minor pentatonic
    { 0,2,3,5,7,9,10 },              // dorian
    { 0,2,4,7,9 },                   // major pentatonic — the friendliest "no wrong notes"
    { 0,2,4,6,8,10 },                // whole tone — 6 symmetric notes, dreamy/floating
    { 0,2,3,5,7,8,11 },              // harmonic minor — the dramatic augmented-2nd colour
    { 0,2,3,7,8 },                   // hirajoshi — a Japanese pentatonic (our own naming)
    { 0,3,5,6,7,10 },                // blues — minor pentatonic + the b5 "blue note"
    { 0,2,5,9,11,16 },               // FOREST (SoundForest) — an open/spread voicing (16!)
};
static const char *const SC_NAME[NSCALE] = {
    "CHROMA","MAJOR","MINOR","MIN PENT","DORIAN","MAJ PENT","WHOLE","HARM MIN",
    "HIRAJOSHI","BLUES","FOREST"
};
static const char *const NOTE[12]        = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

#define I_PD         5
#define GRID_MAX_OCT 4
#define HUD_H        20      // on-screen control bar height (px, canvas space)
#define FINGER_PX    22.0f   // baseline pad target (canvas px; ~9mm once scaled)

// player state
static int scale  = 3;       // PENTA — the cold-open where every combo sounds good
static int root   = 0;       // C
static int octave = 4;       // mid register (MIDI 48 = C at octave 4)
static int rowoff = 5;       // the ISOMORPHIC ROW OFFSET in scale DEGREES (1..nsc). This
                             // is the ADR-0028 knob: rowoff == nsc means each row up is
                             // +1 octave (ISO-OCT, the ship default); 3 in a diatonic
                             // scale = up a FOURTH (ISO-4TH); any value is a valid grid —
                             // tap ROW to dial it and hear how the chord shapes change.
static bool show_help = false;

// grid geometry, recomputed each frame (cheap; a city is bounded, so is a grid)
static int   g_cols, g_rows;
static float g_x, g_y, g_pw, g_ph, g_gap;

// per-pad strike glow (transient; decays), indexed by pad idx
#define GLOW_MAX 512
static float glow[GLOW_MAX];

// polyphonic voices — one per finger (+ the mouse) + the self-player
#define MAXV 16
typedef struct { int active, id, pad, handle; } Voice;   // id: touch id, or -2 = mouse
static Voice voice[MAXV];

// self-play (the cold-open arpeggio); any real input switches it off
static bool  selfplay = true;
static int   sp_tick = 0, sp_step = 0, sp_handle = -1, sp_pad = -1;

static float clampf(float v, float lo, float hi){ return v < lo ? lo : v > hi ? hi : v; }

// ── the pad grid geometry — a SEAMLESS isomorphic lattice window ─────────────
// The key invariant (the fix for "chroma is missing a note"): COLUMNS == the row
// offset. In an isomorphic lattice note(row,col) = base + degree(col + row*off);
// if the horizontal span (cols) is smaller than the vertical jump (off) some
// degrees fall in the GAP between rows (chromatic lost A#/B); if larger, some
// REPEAT (pentatonic doubled two columns). Setting cols == off makes it row-major
// exact: every scale degree appears once per octave, no gaps, no repeats — for
// EVERY scale. Rows then fill the height (finger-sized), capped to GRID_MAX_OCT
// octaves; on a roomy screen the pads grow to fill (the cap-and-grow law).
static void compute_grid(void) {
    float aw = SCREEN_W, ah = SCREEN_H - HUD_H;
    g_gap = clampf(FINGER_PX * 0.12f, 1, 4);
    int nsc = SC_N[scale];
    int off = rowoff < 1 ? 1 : (rowoff > nsc ? nsc : rowoff);
    int cols = off;                                              // the seamless-tiling invariant
    int rowsFit = (int)((ah + g_gap) / (FINGER_PX + g_gap)); if (rowsFit < 1) rowsFit = 1;
    int rowsCap = (GRID_MAX_OCT * nsc + cols - 1) / cols;        // rows for GRID_MAX_OCT octaves
    int rows = rowsFit < rowsCap ? rowsFit : rowsCap; if (rows < 1) rows = 1;
    g_cols = cols; g_rows = rows;
    g_pw = (aw - g_gap * (cols - 1)) / cols;
    g_ph = (ah - g_gap * (rows - 1)) / rows;
    g_x = 0; g_y = HUD_H;
}

// per-cell note: ISO fixes the row offset in scale DEGREES (right = +1 degree,
// up = +rowoff degrees) so a chord/scale SHAPE is the same everywhere and survives a
// reflow. Transposed by the KEY root. Returns an absolute MIDI note.
static int pad_midi(int idx) {
    int nsc = SC_N[scale];
    int col = idx % g_cols, row = idx / g_cols, rfb = g_rows - 1 - row;   // rfb: 0 = bottom
    int off = rowoff < 1 ? 1 : (rowoff > nsc ? nsc : rowoff);             // row up = +off degrees
    int degIndex = col + rfb * off;
    int deg = ((degIndex % nsc) + nsc) % nsc, oc = degIndex / nsc;
    return 12 * octave + root + SC[scale][deg] + 12 * oc;
}

// which pad contains a point (canvas space), or -1 (miss / in a gap / in the HUD)
static int pad_at(int px, int py) {
    float lx = px - g_x, ly = py - g_y;
    if (lx < 0 || ly < 0) return -1;
    int col = (int)(lx / (g_pw + g_gap)), row = (int)(ly / (g_ph + g_gap));
    if (col < 0 || col >= g_cols || row < 0 || row >= g_rows) return -1;
    float cx = lx - col * (g_pw + g_gap), cy = ly - row * (g_ph + g_gap);
    if (cx > g_pw || cy > g_ph) return -1;        // fell in the gutter between pads
    return row * g_cols + col;
}

static void set_pad_voice(void) {
    // "sweep pad" preset from the pd cart: reso-tri wave, full DCW "wowww".
    // envs converted from the pd cart's slider→ms formulas.
    instrument(I_PD, INSTR_PD, 76, 627, 5, 755);
    instrument_harmonics(I_PD, 0.69f);   // wavetype: resonant triangle
    instrument_timbre(I_PD, 0.38f);      // distortion / brightness
    instrument_morph(I_PD, 0.95f);       // full DCW envelope sweep — the "wowww"
}

// ── on-screen control bar (touch has no keyboard) — cyclable chips ───────────
// Every parameter the keyboard cycles gets a tappable chip so the cart is fully
// playable on a phone. The row cells are laid out by weight across the bar width.
enum { ACT_NONE, ACT_SCALE, ACT_KEY, ACT_ROW, ACT_OCTDN, ACT_OCTUP, ACT_SELF };
typedef struct { float w; int act; } Chip;
static const Chip CHIP[] = {
    { 1.5f, ACT_SCALE }, { 1.1f, ACT_KEY }, { 1.7f, ACT_ROW },
    { 0.7f, ACT_OCTDN }, { 0.7f, ACT_NONE }, { 0.7f, ACT_OCTUP }, { 1.2f, ACT_SELF },
};
#define NCHIP (int)(sizeof(CHIP) / sizeof(CHIP[0]))
static int bar_x[NCHIP + 1];              // cumulative pixel boundaries of the bar cells
static int bar_prev = -1;                 // chip the latch holds (-1 = released) — click, not hold

static void layout_bar(void) {
    float tot = 0; for (int i = 0; i < NCHIP; i++) tot += CHIP[i].w;
    float x = 0; bar_x[0] = 0;
    for (int i = 0; i < NCHIP; i++) { x += SCREEN_W * CHIP[i].w / tot; bar_x[i + 1] = (int)(x + 0.5f); }
}
static int bar_hit(int px, int py) {      // chip index under a point, or -1
    if (py < 0 || py >= HUD_H) return -1;
    for (int i = 0; i < NCHIP; i++) if (px >= bar_x[i] && px < bar_x[i + 1]) return i;
    return -1;
}
static void stop_selfplay(void) {
    selfplay = false;
    if (sp_handle >= 0) { note_off(sp_handle); sp_handle = -1; sp_pad = -1; }
}
static void do_action(int act) {
    switch (act) {
        // cycling the scale keeps the row offset "stuck to octave" if it was, else clamps
        case ACT_SCALE: { int o = SC_N[scale]; scale = (scale + 1) % NSCALE;
                          int n = SC_N[scale]; if (rowoff == o || rowoff > n) rowoff = n;
                          if (rowoff < 1) rowoff = 1; } break;
        case ACT_KEY:   root = (root + 1) % 12; break;
        case ACT_ROW:   rowoff = rowoff % SC_N[scale] + 1; break;        // dial 1..nsc, wrapping
        case ACT_OCTDN: if (octave > 1) octave--; break;
        case ACT_OCTUP: if (octave < 7) octave++; break;
        case ACT_SELF:  if (selfplay) stop_selfplay(); else selfplay = true; break;
        default: break;
    }
}
// the ROW chip's label — names the musical interval when the offset matches one
static const char *row_label(void) {
    int nsc = SC_N[scale];
    if (rowoff >= nsc)           return "ROW:OCT";
    if (nsc == 7 && rowoff == 3) return "ROW:4th";
    if (nsc == 7 && rowoff == 4) return "ROW:5th";
    return str("ROW:+%d", rowoff);
}

void update(void) {
    compute_grid();
    layout_bar();
    for (int i = 0; i < GLOW_MAX; i++) if (glow[i] > 0) glow[i] = clampf(glow[i] - 0.08f, 0, 1);

    // ── keyboard shortcuts (desktop; each one mirrors an on-screen chip) ──
    if (keyp('h') || keyp('H')) show_help = !show_help;
    if (keyp('s') || keyp('S')) do_action(ACT_SCALE);
    if (keyp('r') || keyp('R')) do_action(ACT_KEY);
    if (keyp('i') || keyp('I')) do_action(ACT_ROW);      // dial the isomorphic row offset
    if (keyp('z') || keyp('Z')) do_action(ACT_OCTDN);
    if (keyp('x') || keyp('X')) do_action(ACT_OCTUP);
    if (keyp('m') || keyp('M')) do_action(ACT_SELF);

    // ── control bar: fire ONCE per press (a CLICK), never repeat while held ──
    // Latched off "is a pointer resting on a chip this frame" — robust to touch ids
    // that churn frame-to-frame (that churn was machine-gunning the chips).
    int bar_chip = -1;
    if (mouse_down(0)) bar_chip = bar_hit(mouse_x(), mouse_y());
    for (int t = 0; t < touch_count() && bar_chip < 0; t++)
        bar_chip = bar_hit(touch_x(t), touch_y(t));
    if (bar_chip >= 0 && bar_prev < 0) do_action(CHIP[bar_chip].act);   // rising edge only
    bar_prev = bar_chip;

    // ── self-play: a gentle walking arpeggio up the scale (the cold-open) ──
    if (selfplay) {
        if (++sp_tick >= 22) {
            sp_tick = 0;
            if (sp_handle >= 0) { note_off(sp_handle); sp_handle = -1; }
            // walk a diagonal-ish path so the ear hears the isomorphic shape
            int total = g_cols * g_rows;
            if (total > 0) {
                int col = sp_step % g_cols;
                int row = g_rows - 1 - ((sp_step / g_cols) % g_rows);
                sp_pad = row * g_cols + col;
                sp_handle = note_on(pad_midi(sp_pad), I_PD, 5);
                if (sp_pad >= 0 && sp_pad < GLOW_MAX) glow[sp_pad] = 1.0f;
                sp_step = (sp_step + 2) % total;
            }
        }
    }

    // ── multitouch: each finger holds its own voice; slide to glide ──
    // 1) release voices whose finger lifted
    for (int e = 0; e < touch_ended_count(); e++) {
        int id = touch_ended_id(e);
        for (int v = 0; v < MAXV; v++)
            if (voice[v].active && voice[v].id == id) { note_off(voice[v].handle); voice[v].active = 0; }
    }
    // 2) update / start voices for each active finger
    for (int t = 0; t < touch_count(); t++) {
        int id = touch_id(t), pad = pad_at(touch_x(t), touch_y(t));
        int vi = -1, free = -1;
        for (int v = 0; v < MAXV; v++) {
            if (voice[v].active && voice[v].id == id) { vi = v; break; }
            if (!voice[v].active && free < 0) free = v;
        }
        if (pad < 0) { if (vi >= 0){ note_off(voice[vi].handle); voice[vi].active = 0; } continue; }
        if (vi < 0) {                                   // new finger
            if (free < 0) continue;
            stop_selfplay();                            // first pad touch hands control over
            voice[free] = (Voice){ 1, id, pad, note_on(pad_midi(pad), I_PD, 6) };
            if (pad < GLOW_MAX) glow[pad] = 1.0f;
        } else if (voice[vi].pad != pad) {              // slid to a new pad — glide (retrigger)
            note_off(voice[vi].handle);
            voice[vi].pad = pad; voice[vi].handle = note_on(pad_midi(pad), I_PD, 6);
            if (pad < GLOW_MAX) glow[pad] = 1.0f;
        }
    }

    // ── mouse as a single finger (desktop) ──
    int mv = -1; for (int v = 0; v < MAXV; v++) if (voice[v].active && voice[v].id == -2) { mv = v; break; }
    if (mouse_down(0)) {
        int pad = pad_at(mouse_x(), mouse_y());
        if (pad < 0) { if (mv >= 0){ note_off(voice[mv].handle); voice[mv].active = 0; } }
        else if (mv < 0) {
            stop_selfplay();                            // first pad click hands control over
            for (int v = 0; v < MAXV; v++) if (!voice[v].active) {
                voice[v] = (Voice){ 1, -2, pad, note_on(pad_midi(pad), I_PD, 6) };
                if (pad < GLOW_MAX) glow[pad] = 1.0f; break;
            }
        } else if (voice[mv].pad != pad) {
            note_off(voice[mv].handle);
            voice[mv].pad = pad; voice[mv].handle = note_on(pad_midi(pad), I_PD, 6);
            if (pad < GLOW_MAX) glow[pad] = 1.0f;
        }
    } else if (mv >= 0) { note_off(voice[mv].handle); voice[mv].active = 0; }
}

// is this pad currently held by a finger/mouse voice?
static bool pad_held(int idx) {
    for (int v = 0; v < MAXV; v++) if (voice[v].active && voice[v].pad == idx) return true;
    return idx == sp_pad;
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // ── the pads ──
    for (int idx = 0; idx < g_cols * g_rows; idx++) {
        int col = idx % g_cols, row = idx / g_cols;
        int midi = pad_midi(idx);
        int pc = ((midi % 12) + 12) % 12, dispOct = midi / 12;
        int isRoot = (((midi - root) % 12 + 12) % 12) == 0;
        int black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);

        float px = g_x + col * (g_pw + g_gap), py = g_y + row * (g_ph + g_gap);
        int x = (int)px, y = (int)py, w = (int)g_pw, h = (int)g_ph;

        int base = isRoot ? CLR_DARK_ORANGE : (black ? CLR_DARKER_PURPLE : CLR_DARKER_BLUE);
        int fill = base;
        if (pad_held(idx))               fill = isRoot ? CLR_ORANGE : CLR_TRUE_BLUE;
        else if (idx < GLOW_MAX && glow[idx] > 0)
            fill = glow[idx] > 0.5f ? (isRoot ? CLR_LIGHT_PEACH : CLR_TRUE_BLUE)
                                    : (isRoot ? CLR_ORANGE : CLR_BLUE);
        rectfill(x, y, w, h, fill);
        rect(x, y, w, h, isRoot ? CLR_LIGHT_PEACH : CLR_DARK_GREY);

        if (h >= 10 && w >= 12) {
            font(FONT_TINY);
            print_centered(str("%s%d", NOTE[pc], dispOct), x + w / 2, y + (h - 5) / 2,
                           (pad_held(idx) || isRoot) ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
        }
    }

    // ── control bar: a tappable chip per parameter (phone has no keyboard) ──
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_TINY);
    for (int i = 0; i < NCHIP; i++) {
        int x = bar_x[i], w = bar_x[i + 1] - bar_x[i];
        const char *lbl = ""; int fg = CLR_LIGHT_PEACH, bg = CLR_DARKER_BLUE, chip = 1;
        switch (CHIP[i].act) {
            case ACT_SCALE: lbl = SC_NAME[scale];             bg = CLR_DARKER_PURPLE; break;
            case ACT_KEY:   lbl = str("KEY %s", NOTE[root]);                          break;
            case ACT_ROW:   lbl = row_label();                bg = CLR_DARK_BLUE;     break;
            case ACT_OCTDN: lbl = "OCT-";                                             break;
            case ACT_OCTUP: lbl = "OCT+";                                             break;
            case ACT_NONE:  lbl = str("O%d", octave); chip = 0; fg = CLR_LIGHT_GREY;  break;   // octave readout
            case ACT_SELF:  lbl = selfplay ? "AUTO" : "PLAY";
                            bg = selfplay ? CLR_LIME_GREEN : CLR_DARKER_BLUE;
                            fg = selfplay ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY;       break;
        }
        if (chip) { rectfill(x + 1, 2, w - 2, HUD_H - 4, bg); rect(x + 1, 2, w - 2, HUD_H - 4, CLR_DARK_GREY); }
        print_centered(lbl, x + w / 2, (HUD_H - 5) / 2, fg);
    }

    if (show_help) {
        int bw = 168, bh = 66, bx = (SCREEN_W - bw) / 2, by = (SCREEN_H - bh) / 2;
        rectfill(bx, by, bw, bh, CLR_BLACK); rect(bx, by, bw, bh, CLR_LIGHT_PEACH);
        font(FONT_SMALL);
        print("scale-locked pad grid", bx + 6, by + 5, CLR_LIGHT_PEACH);
        print("tap/drag pads to play", bx + 6, by + 15, CLR_LIGHT_GREY);
        print("tap the chips up top:", bx + 6, by + 27, CLR_LIGHT_GREY);
        print("SCALE  KEY  ROW  OCT", bx + 6, by + 37, CLR_LIGHT_GREY);
        print("ROW = isomorphic step", bx + 6, by + 47, CLR_LIGHT_GREY);
        print("no wrong notes in-scale", bx + 6, by + 57, CLR_LIME_GREEN);
    }
}
