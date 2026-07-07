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
    "summary": "A scale-locked ISOMORPHIC pad grid you play with your fingers - pick a scale and there are NO WRONG NOTES. Voiced by INSTR_PD (the Casio-CZ phase-distortion engine) on a lush 'wowww' sweep pad, so every tap and chord rings.",
    "detail": "The playable showcase for the scale-grid feature (design/scale-grid.md). Pick a SCALE (chroma/major/minor/penta/dorian) and a KEY (root) and the grid shows ONLY in-scale notes - so any finger, any chord, any run sounds musical. It is ISOMORPHIC: the row offset is a fixed number of scale DEGREES, so a chord SHAPE is identical in every key and octave (ISO-OCT = row up is +1 octave with roots in a clean left column; ISO-4TH = row up is +a fourth for diagonal chord shapes). Roots are tinted orange. Pads are finger-sized (never crammed): tight screens PACK 1-finger pads, roomy screens CAP the range to 4 octaves and GROW big comfy squares. Fully polyphonic multitouch - every finger holds its own sustained INSTR_PD voice; slide a finger across pads to glide a run. It cold-opens PLAYING ITSELF (a gentle walking arpeggio) so a stranger hears the idea before touching it; the first tap hands control over. The grid maths (scale-lock + isomorphic degree offset + cap-and-grow finger sizing) is lifted from the epianofit mock and kept self-contained here, ready to graduate into a grid.h library.",
    "controls": "TAP / drag pads to play (mouse = one finger, multitouch = many). S scale, R key/root, I layout (ISO-OCT / ISO-4TH), Z/X octave down/up, M self-play on/off, H help."
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
#include <math.h>

// ── scales (semitone degrees within an octave). CHROMA = all 12. ─────────────
#define NSCALE 5
static const int SC_N[NSCALE]   = { 12, 7, 7, 5, 7 };
static const int SC[NSCALE][12] = {
    { 0,1,2,3,4,5,6,7,8,9,10,11 },   // chromatic
    { 0,2,4,5,7,9,11 },              // major
    { 0,2,3,5,7,8,10 },              // natural minor
    { 0,3,5,7,10 },                  // minor pentatonic
    { 0,2,3,5,7,9,10 },              // dorian
};
static const char *const SC_NAME[NSCALE] = { "CHROMA","MAJOR","MINOR","PENTA","DORIAN" };
static const char *const NOTE[12]        = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const char *const LAYNAME[2]      = { "ISO-4TH","ISO-OCT" };

#define I_PD         5
#define GRID_MAX_OCT 4
#define HUD_H        14      // top status bar height (px, canvas space)
#define FINGER_PX    22.0f   // baseline pad target (canvas px; ~9mm once scaled)

// player state
static int scale = 3;        // PENTA — the cold-open where every combo sounds good
static int root  = 0;        // C
static int octave = 4;       // mid register (MIDI 48 = C at octave 4)
static int isolayout = 1;    // 0 = ISO-4TH, 1 = ISO-OCT (the ship default)
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

// ── the pad grid geometry (lifted from epianofit.c keybed_grid) ──────────────
static void compute_grid(void) {
    float aw = SCREEN_W, ah = SCREEN_H - HUD_H;
    g_gap = clampf(FINGER_PX * 0.12f, 1, 4);
    int nsc = SC_N[scale], maxNotes = GRID_MAX_OCT * nsc;
    int colsMin = (int)((aw + g_gap) / (FINGER_PX + g_gap)); if (colsMin < 1) colsMin = 1;
    int rowsMin = (int)((ah + g_gap) / (FINGER_PX + g_gap)); if (rowsMin < 1) rowsMin = 1;
    int cols, rows;
    if (colsMin * rowsMin <= maxNotes) {          // tight — pack finger-pads
        cols = colsMin; rows = rowsMin;
    } else {                                      // roomy — cap range, grow pads
        rows = (int)(sqrtf((float)maxNotes * ah / aw) + 0.5f); if (rows < 1) rows = 1;
        cols = (maxNotes + rows - 1) / rows;      if (cols < 1) cols = 1;
        if (cols > colsMin) cols = colsMin;       // never shrink a pad below 1 finger
        if (rows > rowsMin) rows = rowsMin;
    }
    g_cols = cols; g_rows = rows;
    g_pw = (aw - g_gap * (cols - 1)) / cols;
    g_ph = (ah - g_gap * (rows - 1)) / rows;
    g_x = 0; g_y = HUD_H;
}

// per-cell note: ISO fixes the row offset in scale DEGREES (right = +1 degree,
// up = +off degrees) so a chord/scale SHAPE is the same everywhere and survives a
// reflow. Transposed by the KEY root. Returns an absolute MIDI note.
static int pad_midi(int idx) {
    int nsc = SC_N[scale];
    int col = idx % g_cols, row = idx / g_cols, rfb = g_rows - 1 - row;   // rfb: 0 = bottom
    int off = (isolayout == 0) ? 3 : nsc;                                 // ISO-4TH / ISO-OCT
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

void update(void) {
    compute_grid();
    for (int i = 0; i < GLOW_MAX; i++) if (glow[i] > 0) glow[i] = clampf(glow[i] - 0.08f, 0, 1);

    // ── controls ──
    if (keyp('h') || keyp('H')) show_help = !show_help;
    if (keyp('s') || keyp('S')) scale = (scale + 1) % NSCALE;
    if (keyp('r') || keyp('R')) root  = (root + 1) % 12;
    if (keyp('i') || keyp('I')) isolayout ^= 1;
    if (keyp('z') || keyp('Z')) { if (octave > 1) octave--; }
    if (keyp('x') || keyp('X')) { if (octave < 7) octave++; }
    if (keyp('m') || keyp('M')) selfplay = !selfplay;

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
                sp_step = (sp_step + 2) % (total > 0 ? total : 1);
            }
        }
    } else if (sp_handle >= 0) { note_off(sp_handle); sp_handle = -1; sp_pad = -1; }

    // any manual input hands control over from the self-player
    bool touched = touch_count() > 0 || mouse_down(0) || mouse_pressed(0);
    if (touched && selfplay) { selfplay = false; if (sp_handle >= 0){ note_off(sp_handle); sp_handle = -1; sp_pad = -1; } }

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

    // ── HUD ──
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_SMALL);
    print(str("%s  KEY %s  %s  OCT%d", SC_NAME[scale], NOTE[root], LAYNAME[isolayout], octave),
          3, 4, CLR_LIGHT_PEACH);
    print_right(selfplay ? "\x0e self-play" : "PLAY", SCREEN_W - 3, 4,
                selfplay ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);

    if (show_help) {
        int bw = 150, bh = 62, bx = (SCREEN_W - bw) / 2, by = (SCREEN_H - bh) / 2;
        rectfill(bx, by, bw, bh, CLR_BLACK); rect(bx, by, bw, bh, CLR_LIGHT_PEACH);
        font(FONT_SMALL);
        print("scale-locked pad grid", bx + 6, by + 5, CLR_LIGHT_PEACH);
        print("tap/drag pads to play", bx + 6, by + 16, CLR_LIGHT_GREY);
        print("S scale  R key  I layout", bx + 6, by + 26, CLR_LIGHT_GREY);
        print("Z/X octave  M self-play", bx + 6, by + 36, CLR_LIGHT_GREY);
        print("no wrong notes in-scale", bx + 6, by + 48, CLR_LIME_GREEN);
    }
}
