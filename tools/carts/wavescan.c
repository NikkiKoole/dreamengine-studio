/* de:meta
{
  "slug": "wavescan",
  "title": "wavescan",
  "status": "active",
  "created": "2026-10-01",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "west-coast-synthesis",
    "scale-quantize"
  ],
  "resizable": true,
  "lineage": "The demo for INSTR_WAVESCAN and docs/design/plinky-harvest.md: the voice of Plinky (plinkysynth/plinky_public, MIT, Alex Evans), an 8-voice touch-strip synth. Its scanned wavetable bank and its low-pass gate are ported; the strings-a-fifth-apart layout is its stride() algorithm. Not a Plinky clone and not named like one: the name and panel art are not ours to use.",
  "description": {
    "summary": "Eight strings, a fifth apart, locked to a scale: you cannot play a wrong note. SCAN sweeps the wave from a pure sine to a buzz; GATE makes each note darken as it fades.",
    "detail": "Each column is a string and each pad on it is a note of C major, so any two fingers make a chord. Hold a pad to sound it, slide along the string to change the note. SCAN moves through twelve band-limited single cycles, drawn above the strings: sine, folded sines, FM, square, saw, noisy folds, a buzz, the two neighbours of the cursor crossfading. SPREAD detunes the voice's four oscillators apart. GATE is a low-pass gate: the filter's cutoff IS the volume envelope, so a note is bright when struck and darkens exactly as it fades, the wooden 'plonk' of west-coast synths. RES puts a peak on that cutoff, NOISE adds breath to the strike, INTERVAL stacks a second pair of oscillators a fifth or an octave away. It plays itself until you touch it; DEMO starts it again.",
    "controls": "Hold pads (multi-touch) or A S D F G H J K; Z/X move the keyboard row. Knobs: SCAN SPREAD GATE (live), RES NOISE INTERVAL (next note). DEMO toggles the self-play."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "face.h"
#include "pointer.h"
#include "wavescan_data.h"   // the engine's own table bank: the cycles drawn above the strings ARE what plays
#include <math.h>

// WAVESCAN — INSTR_WAVESCAN (Plinky's voice) on Plinky's surface.
//
//   the strings   8 columns × 8 pads, each column one voice. Column c starts STRIDE[c] scale steps
//                 up: Plinky's stride() for C major and a fifth (7 semitones), which quantises the
//                 fifth to the scale and avoids repeating a degree, so the strings tune 0 4 8 … 28
//   the voice     one INSTR_WAVESCAN slot. SCAN/SPREAD/GATE are its three macros and ride live;
//                 RES/NOISE/INTERVAL are its aux channel, read at the next note-on
//   the bank      drawn from wavescan_data.h (level 3, 64 samples per cycle), the scan cursor
//                 between the two shapes it is crossfading
//
// Every sound setting is SET-AND-HOLD: apply_sound() re-sends a value only when it changed.

#define SLOT      5
#define NCOL      8
#define NROW      8
#define ROOT      36          // C2 at the bottom of string 0
#define DESIGN_W  200
#define DESIGN_H  320

static const int MAJOR[7] = { 0, 2, 4, 5, 7, 9, 11 };
static int STRIDE[NCOL];      // scale steps up from ROOT where each string starts

// Plinky's stride() (plinky.c), for one scale: walk up `semis` per string, snap each landing to the
// nearest scale step, and break ties toward the step used least so far.
static void build_stride(int semis) {
    int pos = 0, used[7] = { 1, 0, 0, 0, 0, 0, 0 };
    STRIDE[0] = 0;
    for (int fi = 1; fi < NCOL; fi++) {
        pos += semis;
        int step = pos % 12, best = 0, bestdist = 0, bestscore = 9999;
        for (int i = 0; i < 7; i++) {
            int dist = MAJOR[i] - step;
            if (dist < -6) dist += 12; else if (dist > 6) dist -= 12;
            int score = abs(dist) * 16 + used[i];
            if (score < bestscore) { bestscore = score; best = i; bestdist = dist; }
        }
        used[best]++;
        pos += bestdist;
        STRIDE[fi] = best + (pos / 12) * 7;
    }
}
static int degree_midi(int deg) { return ROOT + 12 * (deg / 7) + MAJOR[deg % 7]; }
static int pad_midi(int col, int row) { return degree_midi(STRIDE[col] + row); }

// ── the sound ──
static float k_scan = 0.40f, k_spread = 0.35f, k_gate = 0.85f;
static float k_res = 0.25f, k_noise = 0.15f, k_iv = 0.5f;
// INTERVAL snaps to the musical ones so a sweep never lands between them
static const int   IV_SEMI[5] = { -12, -5, 0, 7, 12 };
static const char *const IV_NAME[5] = { "-OCT", "-4TH", "UNISON", "+5TH", "+OCT" };
static int iv_index(void) { int i = (int)(k_iv * 4.999f); return i < 0 ? 0 : i > 4 ? 4 : i; }

static float a_scan = -1, a_spread = -1, a_gate = -1, a_res = -1, a_noise = -1;
static int   a_iv = -99;
static void apply_sound(void) {
    if (k_scan   != a_scan)   { instrument_harmonics(SLOT, k_scan); a_scan = k_scan; }
    if (k_spread != a_spread) { instrument_timbre(SLOT, k_spread);  a_spread = k_spread; }
    if (k_gate   != a_gate)   { instrument_morph(SLOT, k_gate);     a_gate = k_gate; }
    if (k_res    != a_res)    { instrument_mode(SLOT, MODE_WAVESCAN_RES, k_res);     a_res = k_res; }
    if (k_noise  != a_noise)  { instrument_mode(SLOT, MODE_WAVESCAN_NOISE, k_noise); a_noise = k_noise; }
    int iv = IV_SEMI[iv_index()];
    if (iv != a_iv) { instrument_mode(SLOT, MODE_WAVESCAN_INTERVAL, 0.5f + iv / 24.0f); a_iv = iv; }
}

// ── the strings: one voice per column, owned by the finger that touched it last ──
typedef struct { int id, col, row; } Ptr;            // id FIRST (pointer.h's contract)
static Ptr ptr[PTR_MAX];
static int col_h[NCOL];                              // held note handle per string, -1 = silent
static int col_row[NCOL];                            // the pad it sounds
static int col_owner[NCOL];                          // touch id (or 1000+key) holding it
static float glow[NCOL][NROW];

static void string_down(int c, int r, int owner) {
    if (col_h[c] >= 0) note_pitch(col_h[c], (float)pad_midi(c, r));   // a second finger on a string retunes it
    else col_h[c] = note_on(pad_midi(c, r), SLOT, 6);
    col_row[c] = r; col_owner[c] = owner;
    glow[c][r] = 1.0f;
}
static void string_slide(int c, int r, int owner) {
    if (col_owner[c] != owner || col_row[c] == r || col_h[c] < 0) return;
    note_pitch(col_h[c], (float)pad_midi(c, r));
    col_row[c] = r; glow[c][r] = 1.0f;
}
static void string_up(int c, int owner) {
    if (col_owner[c] != owner || col_h[c] < 0) return;
    note_off(col_h[c]); col_h[c] = -1; col_owner[c] = -1;
}

// ── layout ──
static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.07f, "nav"   },
    { FACE_BAND, EDGE_TOP,    0.15f, "bank"  },
    { FACE_HERO, 0,           0.00f, "pads"  },
    { FACE_BAND, EDGE_BOTTOM, 0.13f, "live"  },
    { FACE_BAND, EDGE_BOTTOM, 0.13f, "note"  },
};
#define NZ 5
static Box z_nav, z_bank, z_pads, z_live, z_note;
static void relayout(void) {
    face_resize_to(DESIGN_W, DESIGN_H);
    Face f = face_layout(face_area(3), ZONES, NZ, NCOL);
    z_nav = f.box[0]; z_bank = f.box[1]; z_pads = f.box[2]; z_live = f.box[3]; z_note = f.box[4];
}
static Box pad_box(int c, int r) {                   // row 0 at the BOTTOM: up the string = up in pitch
    float gap = 2.0f, pw = (z_pads.w - gap * (NCOL + 1)) / NCOL, ph = (z_pads.h - gap * (NROW + 1)) / NROW;
    return box(z_pads.x + gap + c * (pw + gap), z_pads.y + z_pads.h - gap - (r + 1) * (ph + gap) + gap, pw, ph);
}
static bool pad_at(int x, int y, int *c, int *r) {
    for (int cc = 0; cc < NCOL; cc++) for (int rr = 0; rr < NROW; rr++) {
        Box b = pad_box(cc, rr);
        if (x >= b.x - 1 && x < b.x + b.w + 1 && y >= b.y - 1 && y < b.y + b.h + 1) { *c = cc; *r = rr; return true; }
    }
    return false;
}

// ── the self-play: a slow arpeggio across the strings while SCAN sweeps, until someone touches it ──
static bool demo = true;
static int  demo_step, demo_frames;
static const signed char DEMO[16][2] = {   // (string, pad)
    {0,0},{2,1},{4,1},{3,2}, {1,2},{3,3},{5,2},{6,2}, {0,1},{2,2},{4,3},{5,4}, {7,2},{5,3},{3,4},{1,3},
};
static void run_demo(void) {
    if (!demo) return;
    if (demo_frames++ % 15 == 0) {                  // ~1/8 notes at 120 bpm
        int s = demo_step++ % 16, c = DEMO[s][0], r = DEMO[s][1];
        hit(pad_midi(c, r), SLOT, 6, 180);
        glow[c][r] = 1.0f;
    }
    k_scan = 0.5f - 0.48f * cosf(demo_frames * (6.2831853f / 960.0f));   // one full sweep every 16 s
}
static void stop_demo(void) { demo = false; }

static const char KEYS[NCOL] = { 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K' };
static int kb_row = 2;

void init(void) {
    build_stride(7);
    instrument(SLOT, INSTR_WAVESCAN, 2, 420, 2, 520);   // short decay, low sustain: room for the gate's plonk
    instrument_glide(SLOT, 25);                          // a slide along a string glides, as a finger would
    reverb(0.55f, 0.45f);
    for (int c = 0; c < NCOL; c++) { col_h[c] = -1; col_owner[c] = -1; }
    PTR_CLEAR(ptr);
    apply_sound();
}

void update(void) {
    relayout();
    // fingers: only a finger that LANDS on a pad joins the pool, so the knobs keep theirs
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i), c, r;
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {                                 // a finger that lands OFF the pads (a knob) stays inert
            if (!pad_at(tx, ty, &c, &r)) { *p = (Ptr){ id, -1, -1 }; continue; }
            *p = (Ptr){ id, c, r };
            stop_demo();
            string_down(c, r, id);
        } else if (p->col >= 0 && pad_at(tx, ty, &c, &r) && c == p->col && r != p->row) {   // slide along its own string
            p->row = r;
            string_slide(p->col, r, id);
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) { if (p->col >= 0) string_up(p->col, p->id); p->id = PTR_NONE; }
    }
    for (int c = 0; c < NCOL; c++) {
        if (keyp(KEYS[c])) { stop_demo(); string_down(c, kb_row, 1000 + c); }
        if (keyr(KEYS[c])) string_up(c, 1000 + c);
    }
    if (keyp('Z') && kb_row > 0) kb_row--;
    if (keyp('X') && kb_row < NROW - 1) kb_row++;
    run_demo();
    apply_sound();
    for (int c = 0; c < NCOL; c++) for (int r = 0; r < NROW; r++) glow[c][r] *= 0.88f;
#ifdef DE_TRACE
    watch("scan", "%.3f", k_scan);
    watch("gate", "%.2f", k_gate);
    watch("demo", "%d", demo);
    int held = 0; for (int c = 0; c < NCOL; c++) held += col_h[c] >= 0;
    watch("held", "%d", held);
#endif
}

// the bank: twelve cycles, the two under the cursor drawn bright, the cursor between them
static const char *const SHAPE[WAVESCAN_NSHAPE] = {
    "SINE", "SINE+3", "FOLD", "FM", "SQUARE", "SAW", "GRIT", "FOLD2", "FOLD3", "GRIT2", "GRIT3", "BUZZ" };
static void draw_bank(void) {
    Box z = z_bank;
    rectfill((int)z.x, (int)z.y, (int)z.w, (int)z.h, CLR_BLACK);
    rect((int)z.x, (int)z.y, (int)z.w, (int)z.h, CLR_INDIGO);
    float pos = k_scan * (WAVESCAN_NSHAPE - 1);
    int sa = (int)pos; if (sa > WAVESCAN_NSHAPE - 2) sa = WAVESCAN_NSHAPE - 2;
    float sf = pos - sa;
    float cw = (z.w - 4) / WAVESCAN_NSHAPE;
    int top = (int)z.y + 3, h = (int)z.h - 14;
    const short *lv;
    for (int k = 0; k < WAVESCAN_NSHAPE; k++) {
        lv = WAVESCAN_TABLE[k] + WAVESCAN_LEVEL_OFF[3];             // 64 samples: enough for a thumbnail
        float wgt = k == sa ? 1.0f - sf : k == sa + 1 ? sf : 0.0f;
        int col = wgt > 0.5f ? CLR_WHITE : wgt > 0.0f ? CLR_PEACH : CLR_DARK_PURPLE;
        int x0 = (int)(z.x + 2 + k * cw), px = -1, py = 0;
        for (int i = 0; i <= 64; i += 2) {
            int x = x0 + 1 + (int)(i * (cw - 2) / 64.0f);
            int y = top + h / 2 - (int)(lv[i < 64 ? i : 0] * (float)(h / 2 - 1) / 19000.0f);
            if (px >= 0) line(px, py, x, y, col);
            px = x; py = y;
        }
    }
    int cx = (int)(z.x + 2 + (pos + 0.5f) * cw);
    line(cx, top, cx, top + h, CLR_YELLOW);
    font(FONT_TINY);
    const char *nm = sf < 0.5f ? SHAPE[sa] : SHAPE[sa + 1];
    print(nm, (int)z.x + 4, (int)(z.y + z.h) - 8, CLR_YELLOW);
}

static void draw_pads(void) {
    for (int c = 0; c < NCOL; c++) for (int r = 0; r < NROW; r++) {
        Box b = pad_box(c, r);
        int deg = STRIDE[c] + r, root = deg % 7 == 0;
        bool on = col_h[c] >= 0 && col_row[c] == r;
        int base = root ? CLR_DARK_BLUE : CLR_DARKER_PURPLE;
        rectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, on ? CLR_PEACH : base);
        float g = glow[c][r];
        if (g > 0.05f && !on) rect((int)b.x, (int)b.y, (int)b.w, (int)b.h, g > 0.5f ? CLR_WHITE : CLR_PINK);
        if (r == kb_row && !demo) pset((int)b.x + 1, (int)b.y + 1, CLR_DARK_GREY);
    }
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    rect(1, 1, screen_w() - 2, screen_h() - 2, CLR_DARK_GREY);
    font(FONT_SMALL);
    print("WAVESCAN", (int)z_nav.x + 4, (int)z_nav.y + 3, CLR_LIGHT_YELLOW);
    // the live output between the title and DEMO: watch the gate darken (round off) a note as it fades
    static float sc[128];
    scope_read(sc, 128);
    int sx0 = (int)z_nav.x + 50, sx1 = (int)(z_nav.x + z_nav.w) - 46, cy = (int)(z_nav.y + z_nav.h / 2);
    int amp = (int)(z_nav.h / 2) - 1, w = sx1 - sx0;
    if (w > 8) for (int i = 1; i < w; i++) {
        int a = (int)(sc[(i - 1) * 127 / w] * amp * 2), b = (int)(sc[i * 127 / w] * amp * 2);
        line(sx0 + i - 1, cy - a, sx0 + i, cy - b, CLR_GREEN);
    }
    draw_bank();
    draw_pads();
    ui_begin();
    Box db = box(z_nav.x + z_nav.w - 40, z_nav.y + 1, 38, z_nav.h - 2);
    if (ui_button_cell(db, demo ? "DEMO ON" : "DEMO")) { demo = !demo; demo_frames = 0; }
    Box lr = lay_inset(z_live, 1);
    ui_knob_cell(lay_cell(lr, 0, 3, 0, 2), &k_scan,   "SCAN");
    ui_knob_cell(lay_cell(lr, 0, 3, 1, 2), &k_spread, "SPREAD");
    ui_knob_cell(lay_cell(lr, 0, 3, 2, 2), &k_gate,   "GATE");
    Box nr = lay_inset(z_note, 1);
    ui_knob_cell(lay_cell(nr, 0, 3, 0, 2), &k_res,   "RES");
    ui_knob_cell(lay_cell(nr, 0, 3, 1, 2), &k_noise, "NOISE");
    ui_knob_cell(lay_cell(nr, 0, 3, 2, 2), &k_iv,    IV_NAME[iv_index()]);
    ui_end();
}

#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    // Plinky's stride() for C major + a fifth: the strings land on diatonic fifths, and string 6 takes
    // F (not F#, which is not in the scale): worked by hand from the upstream algorithm
    build_stride(7);
    int want[NCOL] = { 0, 4, 8, 12, 16, 20, 24, 28 };
    for (int c = 0; c < NCOL; c++) expect_eq(STRIDE[c], want[c], "strings start a diatonic fifth apart");
    expect_eq(pad_midi(0, 0), 36, "string 0 pad 0 is C2");
    expect_eq(pad_midi(1, 0), 43, "string 1 starts on G2, a fifth up");
    expect_eq(pad_midi(6, 0), 77, "string 6 starts on F5 (the fifth above B, snapped into the scale)");
    expect_eq(pad_midi(7, 7), 96, "the top pad is C7");
    // every pad is in C major: the 'no wrong notes' promise
    int bad = 0;
    for (int c = 0; c < NCOL; c++) for (int r = 0; r < NROW; r++) {
        int pc = pad_midi(c, r) % 12; bool in = false;
        for (int i = 0; i < 7; i++) in |= MAJOR[i] == pc;
        bad += !in;
    }
    expect_eq(bad, 0, "every pad is a note of the scale");
    // adjacent pads on a string climb by exactly one scale step
    for (int r = 0; r < NROW - 1; r++) expect(pad_midi(3, r + 1) > pad_midi(3, r), "up the string = up in pitch");
    // INTERVAL snaps: the extremes and the middle land on the named intervals
    k_iv = 0.0f; expect_eq(IV_SEMI[iv_index()], -12, "INTERVAL bottom = an octave down");
    k_iv = 0.5f; expect_eq(IV_SEMI[iv_index()], 0, "INTERVAL centre = unison");
    k_iv = 1.0f; expect_eq(IV_SEMI[iv_index()], 12, "INTERVAL top = an octave up");
    k_iv = 0.5f;
}
#endif
