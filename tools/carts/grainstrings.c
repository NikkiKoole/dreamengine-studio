/* de:meta
{
  "slug": "grainstrings",
  "title": "grain strings",
  "status": "active",
  "created": "2026-10-01",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "granular-synth",
    "scale-quantize"
  ],
  "resizable": true,
  "lineage": "The demo for INSTR_GRAIN and docs/design/plinky-harvest.md §4.1: the sampler of Plinky (plinkysynth/plinky_public, MIT, Alex Evans), where each of eight touch strips plays a granular voice and your finger's place on the strip is your place in the sound. Not a Plinky clone and not named like one.",
  "description": {
    "summary": "The console sings a chord, records itself, and hands you the recording on eight strings. Where you touch a string is where you are in the sound; drag to move through it.",
    "detail": "Each string plays the same recording at a different note of a pentatonic scale, so any fingers you hold make a chord. Height is time: the bottom of a string is the start of the sound, the top is the end, and the line on each string shows where its grains are coming from. Drag a finger up or down to scrub through the sound. SPEED sets how fast a held note travels on its own: all the way down and it FREEZES on one moment, so you can hold a single instant of a sung chord as long as you like. SIZE sets the grain length (short = buzzy, long = smooth), SCATTER smears the grains into a cloud, DETUNE scatters their pitch upward into a shimmer, REVERSE plays everything backward. RESAMPLE makes the console sing a new phrase; MIC records three seconds of you instead.",
    "controls": "Hold and drag on the strings (multi-touch), or A S D F G H J K at the middle of the sound. Knobs: SIZE SPEED SCATTER DETUNE. REVERSE, RESAMPLE, MIC (records 3 s)."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "face.h"
#include "pointer.h"
#include <math.h>

// GRAIN STRINGS — INSTR_GRAIN (Plinky's per-note granular sampler) on eight strings.
//
//   the source    at boot the console sings (a VOICE choir chord + a MALLET line), then record_grab()
//                 takes its own output into sample 0. MIC swaps in three seconds of the microphone
//   the strings   8 columns, each its own INSTR_GRAIN slot (so instrument_playhead() can show each one),
//                 tuned to a pentatonic so every chord works. A finger's HEIGHT is its POSITION in the
//                 sound: set on the slot before note-on, then ridden live with note_harmonics(handle)
//   the knobs     SIZE / SPEED are live macros on all eight slots; SCATTER / DETUNE / REVERSE are the
//                 aux channel, read at the next note-on
//
// Every sound setting is SET-AND-HOLD: apply_sound() re-sends a value only when it changed.

#define NCOL      8
#define SL0       5           // slots 5..12 = the strings
#define SL_CHOIR  20          // the source phrase
#define SL_BELL   21
#define ROOT      60          // the recording's root: column 2 (C4) plays it at its own pitch
#define DESIGN_W  200
#define DESIGN_H  320
#define SRC_SECS  2.4f

static const int PITCH[NCOL] = { 55, 57, 60, 62, 64, 67, 69, 72 };   // G A C D E G A C
static float pos_of(float y01) { return y01 < 0 ? 0 : y01 > 1 ? 1 : y01; }

// ── the sound ──
static float k_size = 0.55f, k_speed = 0.12f, k_scatter = 0.2f, k_detune = 0.0f;
static int   reverse;
static float a_size = -1, a_speed = -1, a_scatter = -1, a_detune = -1;
static int   a_rev = -1;
static void apply_sound(void) {
    for (int c = 0; c < NCOL; c++) {
        if (k_size    != a_size)    instrument_timbre(SL0 + c, k_size);
        if (k_speed   != a_speed)   instrument_morph(SL0 + c, k_speed);
        if (k_scatter != a_scatter) instrument_mode(SL0 + c, MODE_GRAIN_SCATTER, k_scatter);
        if (k_detune  != a_detune)  instrument_mode(SL0 + c, MODE_GRAIN_DETUNE, k_detune);
        if (reverse   != a_rev)     instrument_mode(SL0 + c, MODE_GRAIN_REVERSE, reverse ? 1.0f : 0.0f);
    }
    a_size = k_size; a_speed = k_speed; a_scatter = k_scatter; a_detune = k_detune; a_rev = reverse;
}

// ── the source: the console sings, then records itself ──
enum { SRC_SINGING, SRC_READY, SRC_MIC_WAIT, SRC_MIC_REC, SRC_EMPTY };
static int src = SRC_SINGING, src_frame, phrase;
static int src_len;                                   // samples in sample 0
#define MIC_MAX (44100 * 3)
static float micbuf[MIC_MAX];
static const int CHORD[3][3] = { { 60, 64, 67 }, { 57, 60, 64 }, { 62, 65, 69 } };
static const int BELL[3][4]  = { { 72, 76, 79, 84 }, { 69, 72, 76, 81 }, { 74, 77, 81, 86 } };
static int choir_h[3] = { -1, -1, -1 };
static void sing(void) {                              // called every frame while SRC_SINGING
    int f = src_frame++, p = phrase % 3;
    if (f == 0) for (int i = 0; i < 3; i++) choir_h[i] = note_on(CHORD[p][i] - 12, SL_CHOIR, 5);
    if (f > 0 && f % 30 == 10 && f < 130) hit(BELL[p][(f / 30) % 4], SL_BELL, 4, 600);
    if (f == 120) for (int i = 0; i < 3; i++) if (choir_h[i] >= 0) { note_off(choir_h[i]); choir_h[i] = -1; }
    if (f == (int)(SRC_SECS * 60) + 10) {
        src_len = record_grab(0, SRC_SECS + 0.2f);
        src = src_len > 1000 ? SRC_READY : SRC_EMPTY;
    }
}
static void resample(void) { phrase++; src = SRC_SINGING; src_frame = 0; note_off_all(); }

static void mic_poll(void) {
    if (src == SRC_MIC_WAIT && mic_active()) { mic_record(3.0f); src = SRC_MIC_REC; }
    if (src == SRC_MIC_REC && !mic_recording()) {
        int n = mic_record_read(micbuf, MIC_MAX);
        float pk = 0.0f;
        for (int i = 0; i < n; i++) pk = fmaxf(pk, fabsf(micbuf[i]));
        if (n > 1000 && pk > 0.01f) {
            for (int i = 0; i < n; i++) micbuf[i] *= 0.9f / pk;        // the take, peak-normalised like record_grab
            sample_load(0, micbuf, n);
            src_len = n; src = SRC_READY;
        } else src = src_len > 1000 ? SRC_READY : SRC_EMPTY;          // a silent take keeps the old sound
    }
}

// ── the strings: one voice per string, owned by the finger that touched it last ──
typedef struct { int id, col; } Ptr;                 // id FIRST (pointer.h's contract)
static Ptr ptr[PTR_MAX];
static int col_h[NCOL], col_owner[NCOL];
static float col_pos[NCOL];

static void string_down(int c, float p, int owner) {
    if (src != SRC_READY) return;
    col_pos[c] = p; col_owner[c] = owner;
    if (col_h[c] >= 0) { note_harmonics(col_h[c], p); return; }
    instrument_harmonics(SL0 + c, p);                 // the new note's grains START here…
    col_h[c] = note_on(PITCH[c], SL0 + c, 6);
}
static void string_move(int c, float p, int owner) {
    if (col_owner[c] != owner || col_h[c] < 0) return;
    if (fabsf(p - col_pos[c]) > 0.002f) { note_harmonics(col_h[c], p); col_pos[c] = p; }   // …and follow the finger
}
static void string_up(int c, int owner) {
    if (col_owner[c] != owner || col_h[c] < 0) return;
    note_off(col_h[c]); col_h[c] = -1; col_owner[c] = -1;
}

// ── layout ──
static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.07f, "nav"     },
    { FACE_HERO, 0,           0.00f, "strings" },
    { FACE_BAND, EDGE_BOTTOM, 0.13f, "knobs"   },
    { FACE_BAND, EDGE_BOTTOM, 0.08f, "buttons" },
};
#define NZ 4
static Box z_nav, z_str, z_knobs, z_btn;
static void relayout(void) {
    face_resize_to(DESIGN_W, DESIGN_H);
    Face f = face_layout(face_area(3), ZONES, NZ, NCOL);
    z_nav = f.box[0]; z_str = f.box[1]; z_knobs = f.box[2]; z_btn = f.box[3];
}
static Box string_box(int c) {
    float gap = 3.0f, w = (z_str.w - gap * (NCOL + 1)) / NCOL;
    return box(z_str.x + gap + c * (w + gap), z_str.y + 2, w, z_str.h - 4);
}
static int string_at(int x, int y, float *p) {
    for (int c = 0; c < NCOL; c++) {
        Box b = string_box(c);
        if (x >= b.x - 1 && x < b.x + b.w + 2 && y >= b.y && y < b.y + b.h) { *p = pos_of(1.0f - (y - b.y) / b.h); return c; }
    }
    return -1;
}
static float y_pos(int c, int y) { Box b = string_box(c); return pos_of(1.0f - (y - b.y) / b.h); }

// ── the self-play: once the sound is ready, hold a slow-moving frozen chord until someone touches it ──
static bool demo = true;
static int  demo_frames, demo_h[3] = { -1, -1, -1 };
static const signed char DCH[4][3] = { { 2, 4, 6 }, { 1, 3, 5 }, { 0, 4, 7 }, { 2, 5, 6 } };
static void demo_stop_notes(void) { for (int i = 0; i < 3; i++) if (demo_h[i] >= 0) { note_off(demo_h[i]); demo_h[i] = -1; } }
static void run_demo(void) {
    if (!demo || src != SRC_READY) return;
    int f = demo_frames++, k = (f / 150) % 4;
    if (f % 150 == 0) {
        demo_stop_notes();
        for (int i = 0; i < 3; i++) {
            int c = DCH[k][i];
            instrument_harmonics(SL0 + c, 0.15f + 0.2f * i);
            demo_h[i] = note_on(PITCH[c], SL0 + c, 5);
        }
    }
    for (int i = 0; i < 3; i++) if (demo_h[i] >= 0 && f % 6 == 0)      // drift up the sound, slowly
        note_harmonics(demo_h[i], pos_of(0.15f + 0.2f * i + (f % 150) * 0.002f));
}
static void stop_demo(void) { if (demo) { demo = false; demo_stop_notes(); } }

static const char KEYS[NCOL] = { 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K' };

void init(void) {
    record_arm();
    instrument(SL_CHOIR, INSTR_VOICE, 220, 0, 7, 700);
    instrument_harmonics(SL_CHOIR, 0.45f);            // "ah"
    instrument(SL_BELL, INSTR_MALLET, 1, 0, 7, 900);
    instrument_harmonics(SL_BELL, 0.75f);
    reverb(0.6f, 0.4f);
    for (int c = 0; c < NCOL; c++) {
        instrument(SL0 + c, INSTR_GRAIN, 40, 0, 7, 600);
        instrument_sample(SL0 + c, 0, ROOT);
        col_h[c] = -1; col_owner[c] = -1;
    }
    PTR_CLEAR(ptr);
    apply_sound();
}

void update(void) {
    relayout();
    if (src == SRC_SINGING) sing();
    mic_poll();
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {                                  // a finger that lands OFF the strings (a knob) stays inert
            float pp; int c = string_at(tx, ty, &pp);
            *p = (Ptr){ id, c };
            if (c >= 0) { stop_demo(); string_down(c, pp, id); }
        } else if (p->col >= 0) string_move(p->col, y_pos(p->col, ty), id);   // a string keeps its finger
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) { if (p->col >= 0) string_up(p->col, p->id); p->id = PTR_NONE; }
    }
    for (int c = 0; c < NCOL; c++) {
        if (keyp(KEYS[c])) { stop_demo(); string_down(c, 0.5f, 1000 + c); }
        if (keyr(KEYS[c])) string_up(c, 1000 + c);
    }
    run_demo();
    apply_sound();
#ifdef DE_TRACE
    watch("src", "%d", src);
    watch("len", "%d", src_len);
    watch("demo", "%d", demo);
    int held = 0; for (int c = 0; c < NCOL; c++) held += col_h[c] >= 0;
    watch("held", "%d", held);
#endif
}

// each string draws the recording running UP it (bottom = start), and its own playhead
static float wlo[160], whi[160];
static void draw_strings(void) {
    int rows = (int)string_box(0).h; if (rows > 160) rows = 160;
    int have = src_len > 0 && sample_peaks(0, wlo, whi, rows) > 0;
    for (int c = 0; c < NCOL; c++) {
        Box b = string_box(c);
        bool on = col_h[c] >= 0 || (demo && src == SRC_READY && (demo_h[0] >= 0) &&
                  (DCH[(demo_frames / 150) % 4][0] == c || DCH[(demo_frames / 150) % 4][1] == c || DCH[(demo_frames / 150) % 4][2] == c));
        rectfill((int)b.x, (int)b.y, (int)b.w, (int)b.h, on ? CLR_DARK_BLUE : CLR_DARKER_PURPLE);
        int cx = (int)(b.x + b.w / 2);
        if (have) for (int r = 0; r < rows; r++) {
            float a = fmaxf(fabsf(wlo[r]), fabsf(whi[r]));
            int hw = (int)(a * (b.w / 2 - 1));
            int y = (int)(b.y + b.h) - 1 - (int)(r * b.h / rows);
            if (hw > 0) line(cx - hw, y, cx + hw, y, on ? CLR_INDIGO : CLR_DARK_PURPLE);
        }
        line(cx, (int)b.y, cx, (int)(b.y + b.h) - 1, on ? CLR_LIGHT_GREY : CLR_DARK_GREY);   // the string
        float ph = instrument_playhead(SL0 + c);
        if (ph >= 0.0f) {
            int y = (int)(b.y + b.h) - 1 - (int)(ph * (b.h - 1));
            rectfill((int)b.x, y - 1, (int)b.w, 3, CLR_PEACH);
            if (col_h[c] >= 0) {                      // where the finger is (it moves the playhead's origin)
                int fy = (int)(b.y + b.h) - 1 - (int)(col_pos[c] * (b.h - 1));
                rect((int)b.x, fy - 2, (int)b.w, 5, CLR_WHITE);
            }
        }
    }
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    rect(1, 1, screen_w() - 2, screen_h() - 2, CLR_DARK_GREY);
    font(FONT_SMALL);
    print("GRAIN STRINGS", (int)z_nav.x + 4, (int)z_nav.y + 3, CLR_LIGHT_YELLOW);
    const char *st = src == SRC_SINGING ? "LISTEN..." : src == SRC_MIC_WAIT ? "MIC?" : src == SRC_MIC_REC ? "REC" :
                     src == SRC_EMPTY ? "NO SOUND" : demo ? "PLAYING ITSELF" : "";
    print(st, (int)(z_nav.x + z_nav.w) - 4 - text_width(st), (int)z_nav.y + 3, src == SRC_MIC_REC ? CLR_RED : CLR_LIGHT_GREY);
    draw_strings();
    if (src == SRC_SINGING) {
        font(FONT_SMALL);
        print("the console is singing", (int)z_str.x + 8, (int)(z_str.y + z_str.h / 2) - 8, CLR_WHITE);
        print("into its own sampler",   (int)z_str.x + 8, (int)(z_str.y + z_str.h / 2) + 2, CLR_WHITE);
    }
    if (src == SRC_MIC_REC) {
        Box b = z_str;
        rectfill((int)b.x, (int)(b.y + b.h) - 3, (int)(b.w * mic_record_progress()), 3, CLR_RED);
    }
    ui_begin();
    Box kr = lay_inset(z_knobs, 1);
    ui_knob_cell(lay_cell(kr, 0, 4, 0, 2), &k_size,    "SIZE");
    ui_knob_cell(lay_cell(kr, 0, 4, 1, 2), &k_speed,   k_speed < 0.01f ? "FROZEN" : "SPEED");
    ui_knob_cell(lay_cell(kr, 0, 4, 2, 2), &k_scatter, "SCATTER");
    ui_knob_cell(lay_cell(kr, 0, 4, 3, 2), &k_detune,  "DETUNE");
    Box br = lay_inset(z_btn, 2);
    if (ui_button_cell(lay_cell(br, 0, 3, 0, 3), reverse ? "REVERSED" : "REVERSE")) reverse = !reverse;
    if (ui_button_cell(lay_cell(br, 0, 3, 1, 3), "RESAMPLE") && src != SRC_SINGING) { stop_demo(); resample(); }
    if (ui_button_cell(lay_cell(br, 0, 3, 2, 3), "MIC") && src == SRC_READY) { stop_demo(); mic_start(); src = SRC_MIC_WAIT; }
    ui_end();
}

#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    // every string is a note of the C major pentatonic: the "any chord works" promise
    static const int PENTA[5] = { 0, 2, 4, 7, 9 };
    int bad = 0;
    for (int c = 0; c < NCOL; c++) { bool in = false; for (int i = 0; i < 5; i++) in |= PITCH[c] % 12 == PENTA[i]; bad += !in; }
    expect_eq(bad, 0, "every string is in the pentatonic");
    for (int c = 1; c < NCOL; c++) expect(PITCH[c] > PITCH[c - 1], "strings rise left to right");
    expect_eq(PITCH[2], ROOT, "string 2 plays the recording at its own pitch");
    // height is time: bottom = the start of the sound, top = the end, clamped
    expect(pos_of(-0.2f) == 0.0f && pos_of(1.3f) == 1.0f, "position clamps to the sound");
    // the self-play only ever holds pentatonic strings, three at a time
    for (int k = 0; k < 4; k++) for (int i = 0; i < 3; i++) expect(DCH[k][i] >= 0 && DCH[k][i] < NCOL, "demo chords are real strings");
}
#endif
