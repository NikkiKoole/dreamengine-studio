/* de:meta
{
  "slug": "humseq",
  "title": "hum sequencer",
  "status": "active",
  "kind": ["instrument"],
  "genre": null,
  "teaches": ["audio-input", "step-sequencer"],
  "created": "2026-07-17",
  "lineage": "hum->MIDI: the voice-as-sequencer sibling of `humtheremin`. Where the theremin plays your pitch LIVE and continuous, humseq CAPTURES a hummed melody, freezes it, quantizes it to notes on a scale (and optionally a grid), and LOOPS it back on any instrument. The first cart to turn a voice into a playable, deterministic note track — the standalone proof of the hum->MIDI frontier (docs/design/audio-input-frontier.md) before it drops into portapop as 'sing a part'.",
  "homage": "Every 'sing your melody and hear it as an instrument' toy — Sonic Pi's live loops, the Jam Origin MIDI Guitar, a beatboxer looping into a pedal — but the input is a hum and the output is a scale-locked piano roll.",
  "description": {
    "summary": "Hum a melody and hear it played back on a synth — a voice-driven step sequencer. Your hum is captured, snapped to a scale, and looped as a clean piano-roll of notes you can re-voice on any instrument.",
    "detail": "hum->MIDI, capture-then-freeze. SPACE opens the mic, SPACE again records a 4-second window while your pitch (mic_pitch, YIN) draws a live ribbon; when the window fills, the contour is SEGMENTED by a hysteresis note-tracker — it commits to a note and only switches when your pitch moves clearly past it and holds, so a wobbling/gliding hum reads as clean notes instead of chatter; breaths become rests. Each note is snapped to the chosen SCALE (S cycles major/minor pentatonic, major, natural minor, or CHROMATIC = nearest semitone, the most faithful) so a stranger's hum lands musical. The frozen track then LOOPS: a playhead sweeps the roll and fires the notes on the chosen instrument. TAB toggles FREE timing (keep the hum's own rhythm) vs GRID (snap note edges to a 16-step grid). Both S and TAB re-quantize the SAME capture live — no re-record. [ / ] cycle the voice (pluck / sine / epiano / saw / square). R re-records. Determinism boundary: the ACT of humming is live input (like portapop recording control events), but the captured note track is plain data — it loops and would save/replay deterministically.",
    "controls": "SPACE: enable mic, then start a 4s recording. R: re-record. S: cycle scale (incl. chromatic). TAB: FREE vs GRID timing. [ / ]: cycle the playback instrument. Then just hum a short melody."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

// HUM SEQUENCER — hum->MIDI, the capture-then-freeze sibling of humtheremin.
// Pitch front-end (hz->MIDI, pentatonic snap, octave-glitch guard) is lifted from humtheremin;
// what's new is CAPTURE (a per-frame pitch contour over a fixed window) -> SEGMENT (runs of steady
// snapped pitch -> notes, gaps -> rests) -> LOOP-PLAY (a playhead fires the notes on one instrument).
// The captured track is deterministic DATA; only the act of humming is live (see de:meta).

#define MIDI_LO   45.0f            // lowest note shown/played (~A2)
#define MIDI_HI   84.0f            // highest (~C6)
#define GATE      0.012f           // below this loudness = silence (breaths between notes -> rests)
#define FPS       60               // capture is frame-indexed; the harness runs a fixed step
#define REC_SECS  4
#define CAP       (REC_SECS * FPS) // captured frames (= loop length)
#define STEPS     16               // grid divisions across the window (GRID mode)
#define STEP_F    (CAP / STEPS)    // frames per grid step
#define MIN_LEN   7                // shortest kept note in frames (~116ms) — drops blips/breaths
#define HYST      0.6f             // semitones your pitch must move PAST a note before we switch
#define CONFIRM   3                // frames a new pitch must hold before it becomes a new note
#define GAP       6                // unvoiced frames we ride through (breaths) before ending a note
#define MAXNOTES  96
#define BAR_H     20               // top control-bar height (the buttons live here)

enum { ST_OFF, ST_WAIT, ST_REC, ST_PLAY };

static const char *NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

// scales as pitch-class sets (rooted at C). CHROMATIC (all 12) = snap to the nearest semitone =
// the most faithful to your hum; the narrower a scale, the more forgiving (every snap sounds good).
typedef struct { const char *name; int n; int deg[12]; } Scale;
static const Scale SCALES[] = {
    { "penta+", 5, {0, 2, 4, 7, 9} },                 // major pentatonic — the safest default
    { "penta-", 5, {0, 3, 5, 7, 10} },                // minor pentatonic
    { "major",  7, {0, 2, 4, 5, 7, 9, 11} },
    { "minor",  7, {0, 2, 3, 5, 7, 8, 10} },          // natural minor
    { "chroma", 12,{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11} },   // no scale-lock, nearest semitone
};
#define NSCALES ((int)(sizeof(SCALES) / sizeof(SCALES[0])))
static int scale_i = 0;                               // current scale (index into SCALES)

static const int   INSTRS[5] = {INSTR_PLUCK, INSTR_SINE, INSTR_EPIANO, INSTR_SAW, INSTR_SQUARE};
static const char *INAMES[5] = {"pluck", "sine", "epiano", "saw", "square"};

static int   state   = ST_OFF;
static int   instr_i = 0;
static int   grid    = 0;          // 0 = FREE timing, 1 = GRID (snap to STEPS)

// capture: one continuous-MIDI pitch per frame, or -1 for a rest (unvoiced)
static float cap_raw[CAP];
static int   cap_pos = 0;
static float last_m  = -1.0f;      // last confident pitch (MIDI), held through the glitch guard
static float pending = -999.0f;    // a big-jump candidate awaiting a repeat

// frozen note track (built from cap_raw on stop; rebuilt on TAB)
typedef struct { int midi, start, len; } Note;
static Note notes[MAXNOTES];
static int  n_notes = 0;

// playback
static int  ph      = 0;           // playhead frame
static int  voice   = -1;
static int  cur_note = -1;         // note index currently sounding (-1 = none)

static float snap_scale(float midi) {                 // nearest note whose pitch-class is in the scale
    const Scale *sc = &SCALES[scale_i];
    int best = (int)lroundf(midi); float bestd = 1e9f;
    for (int oct = -1; oct <= 8; oct++)
        for (int i = 0; i < sc->n; i++) {
            int cand = oct * 12 + sc->deg[i];
            float d = fabsf(midi - cand);
            if (d < bestd) { bestd = d; best = cand; }
        }
    return (float)best;
}

static int pitch_y(float midi) {
    float f = (midi - MIDI_LO) / (MIDI_HI - MIDI_LO);
    if (f < 0) f = 0; if (f > 1) f = 1;
    return BAR_H + 2 + (int)((1.0f - f) * (SCREEN_H - BAR_H - 12));   // high pitch = high on screen
}
static int frame_x(int f) { return f * SCREEN_W / CAP; }

static void note_name(int m, char *buf, int cap) {
    snprintf(buf, cap, "%s%d", NOTES[((m % 12) + 12) % 12], m / 12 - 1);
}

static void push_note(int m, int start, int len) {
    if (len < MIN_LEN || n_notes >= MAXNOTES) return;
    // merge with the previous note if it's the SAME pitch a short gap away (de-fragment)
    if (n_notes > 0 && notes[n_notes - 1].midi == m &&
        start - (notes[n_notes - 1].start + notes[n_notes - 1].len) <= GAP) {
        notes[n_notes - 1].len = start + len - notes[n_notes - 1].start;
        return;
    }
    notes[n_notes++] = (Note){ m, start, len };
}

// SEGMENT the captured contour into notes — a monophonic note TRACKER with hysteresis, so a
// wobbling/gliding hum doesn't chatter into many notes. We COMMIT to a note and only switch when
// the raw pitch moves clearly PAST it (HYST) toward another scale degree AND holds there (CONFIRM);
// short unvoiced dropouts (breaths) are ridden through (GAP) instead of ending the note. GRID mode
// then snaps each note's edges to the nearest step so the loop sits on the beat.
static void build_notes(void) {
    n_notes = 0;
    int   open = 0, cur_m = 0, start = 0, end = 0, gap = 0;
    int   cand_m = -1, cand_start = 0, cand_cnt = 0;

    for (int f = 0; f < CAP; f++) {
        float r = cap_raw[f];
        if (r < 0) {                                   // unvoiced
            if (open && ++gap > GAP) { push_note(cur_m, start, end - start); open = 0; cand_cnt = 0; cand_m = -1; }
            continue;
        }
        int s = (int)snap_scale(r);
        if (!open) { open = 1; cur_m = s; start = f; end = f + 1; gap = 0; cand_m = -1; cand_cnt = 0; continue; }
        gap = 0;
        if (s == cur_m) { end = f + 1; cand_m = -1; cand_cnt = 0; continue; }
        // s differs — only a move that clears the hysteresis band counts as leaving the note
        float sep = fabsf(r - (float)cur_m) - fabsf(r - (float)s);
        if (sep > HYST) {
            if (s == cand_m) cand_cnt++; else { cand_m = s; cand_start = f; cand_cnt = 1; }
            if (cand_cnt >= CONFIRM) {                 // the new pitch stuck → close here, open the new note
                push_note(cur_m, start, cand_start - start);
                cur_m = cand_m; start = cand_start; end = f + 1; cand_m = -1; cand_cnt = 0;
            } else end = f + 1;                        // tentatively extend; may snap back
        } else { end = f + 1; cand_m = -1; cand_cnt = 0; }   // within the band → still this note
    }
    if (open) push_note(cur_m, start, end - start);

    if (grid) for (int i = 0; i < n_notes; i++) {      // snap edges to the step grid
        int s = (notes[i].start + STEP_F / 2) / STEP_F * STEP_F;
        int e = (notes[i].start + notes[i].len + STEP_F / 2) / STEP_F * STEP_F;
        if (e <= s) e = s + STEP_F;
        if (e > CAP) e = CAP;
        notes[i].start = s; notes[i].len = e - s;
    }
}

static int note_at(int f) {                    // which note (index) covers frame f, else -1
    for (int i = 0; i < n_notes; i++)
        if (f >= notes[i].start && f < notes[i].start + notes[i].len) return i;
    return -1;
}

static void kill_voice(void) {
    if (voice >= 0) { note_off(voice); voice = -1; }
    cur_note = -1;
}

static void start_record(void) {
    kill_voice();
    for (int i = 0; i < CAP; i++) cap_raw[i] = -1.0f;
    cap_pos = 0; last_m = -1.0f; pending = -999.0f;
    n_notes = 0; ph = 0;
    state = ST_REC;
}

static int pending_rec = 0;                        // waiting on mic permission, then auto-record
static void rec_pressed(void) {                    // the single REC button's action
    if (state == ST_REC) return;
    if (!mic_active()) { mic_start(); pending_rec = 1; if (state == ST_OFF) state = ST_WAIT; }
    else start_record();
}
static void requantize(void) { if (state == ST_PLAY) { build_notes(); kill_voice(); } }

void init(void) {
    bpm(120);                                  // a tempo for the grid + a future portapop handoff
    for (int i = 0; i < CAP; i++) cap_raw[i] = -1.0f;
}

void update(void) {
    // keyboard shortcuts (the on-screen buttons in draw() are the primary controls)
    if (keyp(KEY_SPACE)) rec_pressed();
    if (keyp('R') && state == ST_PLAY) start_record();
    if (keyp('[')) instr_i = (instr_i + 4) % 5;
    if (keyp(']')) instr_i = (instr_i + 1) % 5;
    int requant = 0;
    if (keyp(KEY_TAB) && (state == ST_PLAY || state == ST_REC)) { grid = !grid; requant = 1; }
    if (keyp('S')     && (state == ST_PLAY || state == ST_REC)) { scale_i = (scale_i + 1) % NSCALES; requant = 1; }
    if (requant) requantize();

    if (pending_rec && mic_active()) { pending_rec = 0; start_record(); }

    if (state == ST_REC) {
        float lvl = mic_level(), hz = mic_pitch();
        int voiced = lvl > GATE && hz > 70.0f && hz < 1100.0f;
        if (voiced) {
            float target = 69.0f + 12.0f * log2f(hz / 440.0f);
            if (target < MIDI_LO) target = MIDI_LO; if (target > MIDI_HI) target = MIDI_HI;
            // trust a small move instantly; a big jump must repeat once (reject lone octave blips)
            if      (last_m < 0)                          last_m = target, pending = -999.0f;
            else if (fabsf(target - last_m)  <= 7.0f)     last_m = target, pending = -999.0f;
            else if (fabsf(target - pending) <= 1.5f)     last_m = target, pending = -999.0f;
            else                                          pending = target;
            cap_raw[cap_pos] = last_m;
        } else {
            cap_raw[cap_pos] = -1.0f;                    // rest
        }
        if (++cap_pos >= CAP) { build_notes(); ph = 0; cur_note = -1; state = ST_PLAY; }
    }
    else if (state == ST_PLAY) {
        ph = (ph + 1) % CAP;
        int idx = note_at(ph);
        if (idx != cur_note) {                           // note boundary -> retrigger (monophonic)
            kill_voice();
            if (idx >= 0) voice = note_on(notes[idx].midi, INSTRS[instr_i], 5);
            cur_note = idx;
        }
    }
    else if (state != ST_REC && voice >= 0) kill_voice();
}

static void draw_roll_bg(void) {
    // horizontal octave guides (each C)
    for (float m = 48; m <= MIDI_HI; m += 12) {
        int y = pitch_y(m);
        for (int x = 0; x < SCREEN_W; x += 6) pset(x, y, CLR_DARK_BLUE);
    }
    // vertical grid steps (GRID mode)
    if (grid) for (int s = 0; s <= STEPS; s++) {
        int x = s * SCREEN_W / STEPS;
        for (int y = BAR_H + 2; y < SCREEN_H - 8; y += 5) pset(x, y, CLR_DARK_BLUE);
    }
}

static void draw_contour_ghost(int upto) {         // the raw hum as a faint ribbon
    int prevx = -1, prevy = 0;
    for (int f = 0; f < upto; f++) {
        if (cap_raw[f] < 0) { prevx = -1; continue; }
        int x = frame_x(f), y = pitch_y(cap_raw[f]);
        pset(x, y, CLR_INDIGO);
        if (prevx >= 0) line(prevx, prevy, x, y, CLR_DARK_BLUE);
        prevx = x; prevy = y;
    }
}

// the top control bar — tappable SCALE / TIMING / VOICE (left) + REC (right). SCALE and TIMING
// re-quantize the same capture live; VOICE just re-voices playback.
static void draw_bar(void) {
    if (ui_button(4,   2, 52, 15, SCALES[scale_i].name))   { scale_i = (scale_i + 1) % NSCALES; requantize(); }
    if (ui_button(58,  2, 44, 15, grid ? "GRID" : "FREE")) { grid = !grid; requantize(); }
    if (ui_button(104, 2, 56, 15, INAMES[instr_i]))        { instr_i = (instr_i + 1) % 5; }

    int bx = SCREEN_W - 64, by = 2, bw = 60, bh = 15;
    if (state == ST_REC) {                                 // a progress bar in the REC button's place
        rect(bx, by, bw, bh, CLR_RED);
        rectfill(bx + 1, by + 1, (int)(mic_record_progress() * (bw - 2)), bh - 2, CLR_RED);
        print("REC", bx + bw / 2 - 12, by + 5, CLR_WHITE);
    } else if (ui_button(bx, by, bw, bh, mic_active() ? "\x07 REC" : "ENABLE")) {
        rec_pressed();
    }
}

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    draw_bar();

    if (state == ST_OFF) {
        font(FONT_COMIC); print("hum sequencer", SCREEN_W/2 - 74, SCREEN_H/2 - 20, CLR_ORANGE); font(FONT_NORMAL);
        print("hum a melody, hear it play back", SCREEN_W/2 - 92, SCREEN_H/2 + 6, CLR_INDIGO);
        print("tap ENABLE, then REC, then hum", SCREEN_W/2 - 92, SCREEN_H/2 + 22, CLR_MEDIUM_GREY);
        ui_end(); return;
    }

    draw_roll_bg();

    if (state == ST_WAIT) {
        draw_contour_ghost(CAP);
        font(FONT_COMIC); print("tap REC + hum", SCREEN_W/2 - 66, SCREEN_H/2 - 8, CLR_WHITE); font(FONT_NORMAL);
        print("a 4-second window", SCREEN_W/2 - 52, SCREEN_H/2 + 12, CLR_MEDIUM_GREY);
        ui_end(); return;
    }

    if (state == ST_REC) {
        draw_contour_ghost(cap_pos);
        int hx = frame_x(cap_pos);
        line(hx, BAR_H + 2, hx, SCREEN_H - 8, CLR_RED);
        if (cap_pos > 0 && cap_raw[cap_pos - 1] >= 0) circfill(hx, pitch_y(cap_raw[cap_pos - 1]), 3, CLR_WHITE);
        print("hum a melody...", 6, SCREEN_H - 12, CLR_RED);
        ui_end(); return;
    }

    // ST_PLAY — the frozen note track, looping
    draw_contour_ghost(CAP);
    for (int i = 0; i < n_notes; i++) {
        int x0 = frame_x(notes[i].start), x1 = frame_x(notes[i].start + notes[i].len);
        int y = pitch_y((float)notes[i].midi), on = (i == cur_note), w = x1 - x0;
        if (w < 1) w = 1;
        rectfill(x0, y - 2, w, 5, on ? CLR_WHITE : CLR_ORANGE);
        rect(x0, y - 2, w, 5, on ? CLR_YELLOW : CLR_DARK_GREY);
    }
    line(frame_x(ph), BAR_H + 2, frame_x(ph), SCREEN_H - 8, CLR_GREEN);
    if (n_notes == 0) print("no clear pitch — tap REC and hum again", 6, SCREEN_H - 12, CLR_MEDIUM_GREY);
    ui_end();
}
