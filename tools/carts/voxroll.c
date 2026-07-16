/* de:meta
{
  "slug": "voxroll",
  "title": "Voxroll",
  "status": "active",
  "created": "2026-07-16",
  "kind": [
    "instrument",
    "toy"
  ],
  "teaches": [
    "note-editing",
    "analog-voice-modeling"
  ],
  "lineage": "pianoroll (the note-editing model) + vox4 (the INSTR_VOICE formant voice) — a Melodyne-style vocal roll where each note is a sung vowel-BLOB you sculpt.",
  "description": {
    "summary": "A Melodyne-style vocal piano roll: draw sung vowel-blobs, drag them for pitch, tug the ✱ tab to shift formant WITHOUT moving pitch, set each note's vowel & vibrato. Touching notes glide.",
    "detail": "One voice (INSTR_VOICE). Time runs left–right, pitch bottom–up. The white line IS the sung pitch — it glides smoothly through notes that touch (legato) and breaks across gaps (loose). Draw a blob in empty space; drag its body to move pitch/time, its edges to retime. Select a blob and the bottom panel sculpts it: VOWEL chips (oo/oh/ah/eh/ee), a VIB (vibrato) slider, and FORMANT — the Melodyne signature — set either with the slider or by dragging the ✱ tab above the blob, which shifts the vocal-tract SIZE (giant→child) while the PITCH stays put. Playback is stateless/scrubbable: drag the ruler and hear the melody at any point. Auto-saves.",
    "controls": "SPACE play/stop · drag RULER: scrub (audible) · click empty: draw a blob (drag to size) · drag blob body: pitch+time · drag blob edge: resize · click blob: select · drag ✱ tab (above selected): formant, pitch stays · bottom panel: VOWEL chips / VIB slider / FORMANT slider · right-click or BACKSPACE: delete · , / . : undo/redo · SAVE / LOAD · wheel / ↑↓: scroll pitch · ←→: scroll time · MIDDLE-drag: pan"
  }
}
de:meta */

// VOXROLL — the "melodyne" cart. pianoroll's editing surface, but every note is a sung vowel-BLOB
// on the INSTR_VOICE formant engine, and the two Melodyne signatures are first-class:
//   (1) the flowing PITCH LINE glides through touching notes and breaks across gaps, and
//   (2) FORMANT (vocal-tract size) is decoupled from PITCH — drag the ✱ tab and the vowel colour
//       shifts giant→child while the note stays on the same pitch.
// Playback is MONOPHONIC (one singer): a single voice follows the contour, gliding within a
// legato run and re-attacking after a gap. The drawn white line is literally the pitch you hear.

#include <stdio.h>     // snprintf
#include <string.h>    // memmove
#include <stdint.h>    // int8_t / uint8_t / uint32_t
#include <math.h>
#include "studio.h"

#define SLOT      5
#define MAXNOTES  256
#define SONG_LEN  16.0f
#define CONNECT   0.16f          // gap (beats) at/under which two blobs GLIDE (legato)

// --- layout -----------------------------------------------------------------
#define HUD_H    10
#define RULER_Y  HUD_H
#define RULER_H  10
#define ROLL_Y   (RULER_Y + RULER_H)
#define INSP_H   46
#define INSP_Y   (SCREEN_H - INSP_H)
#define ROLL_H   (INSP_Y - ROLL_Y)
#define PIANO_W  16
#define ROLL_X   PIANO_W
#define ROWH     9               // px per semitone
#define PXB      30.0f           // px per beat
#define EDGE     4               // grab zone for a blob edge

static const char *VOWELS[5] = { "oo", "oh", "ah", "eh", "ee" };
static const char *SIZES[3]  = { "giant", "adult", "child" };
static const char *NOTES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

typedef struct {
    int8_t  pitch;             // MIDI
    float   start, len;        // beats
    uint8_t vowel;             // 0..4
    float   vib;               // 0..1 vibrato depth
    float   size;              // 0..1 formant / vocal-tract length (giant..child) — decoupled from pitch
    int     h;                 // (unused per-note; kept 0)
    float   ostart, osize;     // drag-start snapshots
    int8_t  opitch;
} Note;

typedef struct { int8_t pitch; float start, len; uint8_t vowel; float vib, size; } SNote;
typedef struct { int n; SNote notes[MAXNOTES]; } Snap;

static Note  note_[MAXNOTES];
static int   n_notes;
static int   sel_i = -1;                // single selection, -1 = none

static bool  playing;
static float playhead;                  // beats
static int   tempo = 96;

static int   view_lo = 55;              // lowest visible pitch
static float view_b0 = 0.0f;            // leftmost visible beat
static float snap = 0.25f;

static int   perf_h = -1;               // the one performance voice (monophonic singer)
static int   prev_h = -1;               // preview voice while editing (heard even when stopped)

enum { DRAG_NONE, DRAG_MOVE, DRAG_END, DRAG_START, DRAG_SCRUB, DRAG_FORMANT, DRAG_VIB, DRAG_FRM };
static int   drag;
static int   drag_note = -1;
static float drag_off;                  // cursor-minus-note offset (beats) while moving
static int   grab_my;                   // y at grab (formant tab)
static bool  panning;
static int   pan_mx, pan_my, pan_lo;
static float pan_b0;
static int   cur_cursor = -1;

// --- geometry helpers -------------------------------------------------------
static inline int   vis_rows(void)  { return ROLL_H / ROWH; }
static inline float vis_beats(void) { return (SCREEN_W - ROLL_X) / PXB; }
static inline float snapf(float b)  { float s = roundf(b / snap) * snap; return s < 0 ? 0 : s; }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

static int   x_of_beat(float b)  { return ROLL_X + (int)((b - view_b0) * PXB); }
static float beat_of_x(int px)   { return view_b0 + (px - ROLL_X) / PXB; }
static int   x_ruler(float b)    { return (int)(b / SONG_LEN * SCREEN_W); }
static float beat_ruler(int px)  { float b = (float)px / SCREEN_W * SONG_LEN;
                                   return b < 0 ? 0 : b > SONG_LEN ? SONG_LEN : b; }
static int   y_top(int m)        { return ROLL_Y + ROLL_H - (m - view_lo + 1) * ROWH; }  // blob TOP
static int   yf(float pitch)     { return ROLL_Y + ROLL_H - (int)((pitch - view_lo + 0.5f) * ROWH); }
static int   pitch_of_y(int py)  { return view_lo + (ROLL_Y + ROLL_H - 1 - py) / ROWH; }

static bool is_black(int m){ int pc = ((m % 12) + 12) % 12;
    return pc==1||pc==3||pc==6||pc==8||pc==10; }

static void note_name(int midi, char *buf){
    const char *n = NOTES[((midi % 12) + 12) % 12];
    int oct = midi / 12 - 1, i = 0;
    buf[i++] = n[0]; if (n[1]) buf[i++] = n[1];
    buf[i++] = '0' + oct; buf[i] = 0;
}
static void clamp_view(void){
    if (view_lo < 24) view_lo = 24;
    if (view_lo > 96 - vis_rows()) view_lo = 96 - vis_rows();
    if (view_b0 < 0) view_b0 = 0;
    float mx = SONG_LEN - vis_beats(); if (mx < 0) mx = 0;
    if (view_b0 > mx) view_b0 = mx;
}

// --- the flowing contour: one pitch line through the whole phrase ------------
// A note is CONNECTED to its successor when their gap <= CONNECT (touching gap=0 counts, that's the
// common case). Connected notes GLIDE: a portamento of GLIDE beats arrives at the next pitch right
// at its onset (a singer sliding into the note), and the line stays down across the join = legato.
// A larger gap breaks the line: pen up, silence, and the next note re-attacks.
//   down=0 -> silence.  note>=0 -> flat inside that note (draw its vibrato wobble).  note==-1 & down -> a glide.
#define GLIDE  0.14f
typedef struct { int down, note; float pitch, vowel, size, vib; } Samp;

// the note whose onset is the smallest start strictly after note i's start (its successor in time)
static int succ_of(int i){
    int j = -1;
    for (int k = 0; k < n_notes; k++)
        if (note_[k].start > note_[i].start && (j < 0 || note_[k].start < note_[j].start)) j = k;
    return j;
}
static void ramp(Samp *s, int a, int b, float t){    // glide from note a into note b
    t = smooth(t < 0 ? 0 : t > 1 ? 1 : t);
    s->down = 1; s->note = -1;
    s->pitch = lerpf(note_[a].pitch, note_[b].pitch, t);
    s->vowel = lerpf(note_[a].vowel, note_[b].vowel, t);
    s->size  = lerpf(note_[a].size,  note_[b].size,  t);
    s->vib   = 0;
}

static Samp sample_at(float beat){
    Samp s = { 0, -1, 0, 0, 0, 0 };
    // home = the note containing this beat (latest onset wins if they overlap)
    int home = -1;
    for (int i = 0; i < n_notes; i++)
        if (beat >= note_[i].start && beat < note_[i].start + note_[i].len)
            if (home < 0 || note_[i].start > note_[home].start) home = i;

    if (home >= 0){
        int nx = succ_of(home);
        if (nx >= 0 && note_[nx].start - (note_[home].start + note_[home].len) <= CONNECT
                    && beat >= note_[nx].start - GLIDE){          // gliding into the next note
            ramp(&s, home, nx, (beat - (note_[nx].start - GLIDE)) / GLIDE);
        } else {                                                  // flat, inside the note
            s.down = 1; s.note = home; s.pitch = note_[home].pitch;
            s.vowel = note_[home].vowel; s.size = note_[home].size; s.vib = note_[home].vib;
        }
        return s;
    }
    // in a gap: hold the previous note, then glide into the next — if they're connected
    int prev = -1, next = -1;
    for (int i = 0; i < n_notes; i++){
        float e = note_[i].start + note_[i].len;
        if (e <= beat && (prev < 0 || e > note_[prev].start + note_[prev].len)) prev = i;
        if (note_[i].start > beat && (next < 0 || note_[i].start < note_[next].start)) next = i;
    }
    if (prev >= 0 && next >= 0 && note_[next].start - (note_[prev].start + note_[prev].len) <= CONNECT){
        if (beat >= note_[next].start - GLIDE)
            ramp(&s, prev, next, (beat - (note_[next].start - GLIDE)) / GLIDE);
        else {                                                    // hold prev's pitch across the gap
            s.down = 1; s.note = -1; s.pitch = note_[prev].pitch;
            s.vowel = note_[prev].vowel; s.size = note_[prev].size; s.vib = 0;
        }
    }
    return s;
}

// --- sound ------------------------------------------------------------------
static void voice_apply(int h, float vowel, float size, float vib){
    note_harmonics(h, vowel / 4.0f);   // VOWEL
    note_timbre(h, size);              // SIZE  (formant / vocal-tract length)
    note_morph(h, 0.5f);               // EFFORT (fixed, relaxed)
    note_lfo(h, 0, LFO_PITCH, 5.5f, vib * 0.22f);
}

// monophonic performer — follows the contour: glides within a legato run, re-attacks after a gap
static void perform(void){
    bool audible = playing || drag == DRAG_SCRUB;
    if (!audible){ if (perf_h >= 0){ note_off(perf_h); perf_h = -1; } return; }
    Samp s = sample_at(playhead);
    if (!s.down){ if (perf_h >= 0){ note_off(perf_h); perf_h = -1; } return; }
    if (perf_h < 0){                                    // phrase start: a fresh attack
        perf_h = note_on((int)(s.pitch + 0.5f), SLOT, 6);
        note_glide(perf_h, 45);
    }
    note_pitch(perf_h, s.pitch);                        // the drawn line == the heard pitch
    voice_apply(perf_h, s.vowel, s.size, s.vib);
}

// preview a blob while you shape it (only when stopped, so it isn't fighting the performer)
static void preview(Note *nt){
    if (playing){ if (prev_h >= 0){ note_off(prev_h); prev_h = -1; } return; }
    if (prev_h < 0){ prev_h = note_on(nt->pitch, SLOT, 6); note_glide(prev_h, 30); }
    note_pitch(prev_h, nt->pitch);
    voice_apply(prev_h, nt->vowel, nt->size, nt->vib);
}
static void preview_stop(void){ if (prev_h >= 0){ note_off(prev_h); prev_h = -1; } }

// --- undo / redo ------------------------------------------------------------
#define UNDO_MAX 32
static Snap ustack[UNDO_MAX]; static int u_len;
static Snap rstack[UNDO_MAX]; static int r_len;

static void snap_take(Snap *s){
    s->n = n_notes;
    for (int i = 0; i < n_notes; i++){
        s->notes[i].pitch = note_[i].pitch; s->notes[i].start = note_[i].start;
        s->notes[i].len = note_[i].len; s->notes[i].vowel = note_[i].vowel;
        s->notes[i].vib = note_[i].vib; s->notes[i].size = note_[i].size;
    }
}
static bool snap_diff(const Snap *s){
    if (s->n != n_notes) return true;
    for (int i = 0; i < n_notes; i++)
        if (s->notes[i].pitch != note_[i].pitch || s->notes[i].start != note_[i].start ||
            s->notes[i].len != note_[i].len || s->notes[i].vowel != note_[i].vowel ||
            s->notes[i].vib != note_[i].vib || s->notes[i].size != note_[i].size) return true;
    return false;
}
static void snap_apply(const Snap *s){
    n_notes = s->n; sel_i = -1;
    for (int i = 0; i < n_notes; i++){
        note_[i].pitch = s->notes[i].pitch; note_[i].start = s->notes[i].start;
        note_[i].len = s->notes[i].len; note_[i].vowel = s->notes[i].vowel;
        note_[i].vib = s->notes[i].vib; note_[i].size = s->notes[i].size; note_[i].h = 0;
    }
}
static void stack_push(Snap *st, int *len, const Snap *s){
    if (*len == UNDO_MAX){ memmove(st, st + 1, (UNDO_MAX - 1) * sizeof *st); (*len)--; }
    st[(*len)++] = *s;
}
static void mark(void){ Snap s; snap_take(&s); stack_push(ustack, &u_len, &s); r_len = 0; }
static void commit(const Snap *pre){ if (snap_diff(pre)){ stack_push(ustack, &u_len, pre); r_len = 0; } }
static void do_undo(void){ if (!u_len) return; Snap c; snap_take(&c); stack_push(rstack, &r_len, &c); snap_apply(&ustack[--u_len]); }
static void do_redo(void){ if (!r_len) return; Snap c; snap_take(&c); stack_push(ustack, &u_len, &c); snap_apply(&rstack[--r_len]); }
static Snap pregest; static bool gest_open;

// --- editing ----------------------------------------------------------------
static int hit_note(int px, int py, int *zone){
    for (int i = n_notes - 1; i >= 0; i--){
        Note *nt = &note_[i];
        int y = y_top(nt->pitch);
        if (py < y || py >= y + ROWH) continue;
        int x0 = x_of_beat(nt->start), x1 = x_of_beat(nt->start + nt->len);
        if (px < x0 - 1 || px > x1 + 1) continue;
        if (zone){
            int e = (x1 - x0) / 3; if (e > EDGE) e = EDGE; if (e < 1) e = 1;
            if (px >= x1 - e)      *zone = DRAG_END;
            else if (px <= x0 + e) *zone = DRAG_START;
            else                   *zone = DRAG_MOVE;
        }
        return i;
    }
    return -1;
}
static void del_note(int i){
    if (i < 0 || i >= n_notes) return;
    note_[i] = note_[--n_notes];
    if (sel_i == i) sel_i = -1; else if (sel_i == n_notes) sel_i = i;
}

// formant tab: a small grab above the top-centre of the SELECTED blob
static bool in_formant_tab(int px, int py){
    if (sel_i < 0) return false;
    Note *nt = &note_[sel_i];
    int cx = (x_of_beat(nt->start) + x_of_beat(nt->start + nt->len)) / 2;
    int ty = y_top(nt->pitch);
    return px >= cx - 5 && px <= cx + 5 && py >= ty - 9 && py < ty;
}

// --- persistence ------------------------------------------------------------
typedef struct { uint32_t magic; int ver, n, tempo; float snap; SNote notes[MAXNOTES]; } SaveBlob;
#define SAVE_MAGIC 0x564F5852u   // 'VOXR'
static void save_song(void){
    SaveBlob b; b.magic = SAVE_MAGIC; b.ver = 1; b.n = n_notes; b.tempo = tempo; b.snap = snap;
    for (int i = 0; i < n_notes; i++){
        b.notes[i].pitch = note_[i].pitch; b.notes[i].start = note_[i].start;
        b.notes[i].len = note_[i].len; b.notes[i].vowel = note_[i].vowel;
        b.notes[i].vib = note_[i].vib; b.notes[i].size = note_[i].size;
    }
    save_bytes(&b, sizeof b);
}
static bool load_song(void){
    SaveBlob b; int got = load_bytes(&b, sizeof b);
    if (got < (int)sizeof b || b.magic != SAVE_MAGIC || b.ver != 1) return false;
    if (b.n < 0 || b.n > MAXNOTES) return false;
    n_notes = b.n; tempo = b.tempo; snap = b.snap; if (snap <= 0) snap = 0.25f; bpm(tempo);
    sel_i = -1;
    for (int i = 0; i < n_notes; i++){
        note_[i].pitch = b.notes[i].pitch; note_[i].start = b.notes[i].start;
        note_[i].len = b.notes[i].len; note_[i].vowel = b.notes[i].vowel;
        note_[i].vib = b.notes[i].vib; note_[i].size = b.notes[i].size; note_[i].h = 0;
    }
    return true;
}

// --- HUD buttons ------------------------------------------------------------
enum { H_PLAY, H_UNDO, H_REDO, H_SNAP, H_SAVE, H_LOAD, H_N };
static const struct { int x, w; } HB[H_N] = {
    { 2, 30 }, { 34, 12 }, { 48, 12 }, { 64, 26 }, { 250, 32 }, { 286, 32 },
};
static bool hb_click(int b){
    int mx = mouse_x(), my = mouse_y();
    return mouse_pressed(MOUSE_LEFT) && mx >= HB[b].x && mx < HB[b].x + HB[b].w && my >= 1 && my < HUD_H - 1;
}

// inspector chip/slider rects
static const struct { int x, w; } VCHIP[5] = { {4,22},{28,22},{52,22},{76,22},{100,22} };
#define VCHIP_Y  (INSP_Y + 12)
#define VCHIP_H  12
#define SLD_X    158
#define SLD_W    60
#define VIB_Y    (INSP_Y + 14)
#define FRM_Y    (INSP_Y + 28)

// --- seed -------------------------------------------------------------------
static void seed(void){
    static const struct { float s, l; int p, v; float vib, sz; } demo[] = {
        {0.00f,0.75f,60,2,0.10f,0.45f},{0.75f,0.75f,62,2,0.10f,0.45f},{1.50f,1.00f,64,3,0.20f,0.45f},
        {2.50f,0.50f,65,1,0.12f,0.45f},{3.00f,1.50f,67,2,0.35f,0.30f},{4.75f,0.75f,65,4,0.15f,0.55f},
        {5.50f,0.75f,64,3,0.15f,0.55f},{6.25f,1.75f,62,0,0.55f,0.65f},
    };
    for (int i = 0; i < (int)(sizeof demo / sizeof demo[0]); i++){
        note_[n_notes++] = (Note){ .pitch=(int8_t)demo[i].p, .start=demo[i].s, .len=demo[i].l,
            .vowel=(uint8_t)demo[i].v, .vib=demo[i].vib, .size=demo[i].sz, .h=0 };
    }
}

void init(void){
    bpm(tempo);
    instrument(SLOT, INSTR_VOICE, 40, 80, 7, 220);
    drag = DRAG_NONE;
    if (!load_song()) seed();
}

void update(void){
    if (keyp(' ')){ playing = !playing; if (!playing && perf_h >= 0){ note_off(perf_h); perf_h = -1; } }
    if (keyp(',')) do_undo();
    if (keyp('.')) do_redo();
    if (keyp('[')){ tempo -= 4; if (tempo < 40) tempo = 40; bpm(tempo); }
    if (keyp(']')){ tempo += 4; if (tempo > 220) tempo = 220; bpm(tempo); }
    if (keyp(KEY_BACKSPACE) && sel_i >= 0){ mark(); del_note(sel_i); }

    if (hb_click(H_PLAY)){ playing = !playing; if (!playing && perf_h >= 0){ note_off(perf_h); perf_h = -1; } }
    if (hb_click(H_UNDO)) do_undo();
    if (hb_click(H_REDO)) do_redo();
    if (hb_click(H_SNAP)) snap = (snap <= 0.125f) ? 1.0f : snap * 0.5f;
    if (hb_click(H_SAVE)) save_song();
    if (hb_click(H_LOAD)){ Snap pre; snap_take(&pre); if (load_song()) commit(&pre); }

    // scroll
    float w = mouse_wheel();
    if (w != 0) view_lo += (w > 0 ? 1 : -1);
    if (keyp(KEY_UP)) view_lo++;
    if (keyp(KEY_DOWN)) view_lo--;
    if (keyp(KEY_LEFT)) view_b0 -= 1;
    if (keyp(KEY_RIGHT)) view_b0 += 1;
    clamp_view();

    // advance playhead
    if (playing){
        playhead += dt() * tempo / 60.0f;
        if (playhead >= SONG_LEN) playhead -= SONG_LEN;
        if (playhead < view_b0 || playhead > view_b0 + vis_beats() - 0.5f){ view_b0 = playhead - 1; clamp_view(); }
    }

    int mx = mouse_x(), my = mouse_y();
    bool in_roll  = mx >= ROLL_X && my >= ROLL_Y && my < ROLL_Y + ROLL_H;
    bool in_ruler = my >= RULER_Y && my < RULER_Y + RULER_H;
    bool in_insp  = my >= INSP_Y;

    // middle-drag pan
    if (mouse_pressed(MOUSE_MIDDLE)){ panning = true; pan_mx = mx; pan_my = my; pan_b0 = view_b0; pan_lo = view_lo; }
    if (panning && mouse_down(MOUSE_MIDDLE)){
        view_b0 = pan_b0 - (mx - pan_mx) / PXB;
        view_lo = pan_lo + (my - pan_my) / ROWH;
        clamp_view();
    }
    if (!mouse_down(MOUSE_MIDDLE)) panning = false;

    // begin: snapshot for undo on release
    if (mouse_pressed(MOUSE_LEFT) && (in_roll || in_insp || in_ruler)){ snap_take(&pregest); gest_open = true; }

    if (mouse_pressed(MOUSE_LEFT)){
        if (in_ruler){
            drag = DRAG_SCRUB;
        } else if (in_insp){
            // vowel chips
            for (int v = 0; v < 5; v++)
                if (sel_i >= 0 && mx >= VCHIP[v].x && mx < VCHIP[v].x + VCHIP[v].w &&
                    my >= VCHIP_Y && my < VCHIP_Y + VCHIP_H){ note_[sel_i].vowel = v; preview(&note_[sel_i]); }
            // sliders (drag continues below)
            if (sel_i >= 0 && mx >= SLD_X && mx < SLD_X + SLD_W){
                if (my >= VIB_Y - 3 && my < VIB_Y + 7) drag = DRAG_VIB;
                else if (my >= FRM_Y - 3 && my < FRM_Y + 7) drag = DRAG_FRM;
            }
        } else if (in_formant_tab(mx, my)){
            drag = DRAG_FORMANT; drag_note = sel_i; grab_my = my; note_[sel_i].osize = note_[sel_i].size;
            preview(&note_[sel_i]);
        } else if (in_roll){
            int zone, i = hit_note(mx, my, &zone);
            if (i >= 0){
                sel_i = i; drag = zone; drag_note = i;
                note_[i].ostart = note_[i].start; note_[i].opitch = note_[i].pitch;
                if (zone == DRAG_MOVE) drag_off = beat_of_x(mx) - note_[i].start;
                preview(&note_[i]);
            } else if (n_notes < MAXNOTES){                 // empty -> draw a blob, drag to size
                int p = pitch_of_y(my);
                note_[n_notes] = (Note){ .pitch=(int8_t)p, .start=snapf(beat_of_x(mx)),
                    .len=snap, .vowel=2, .vib=0.15f, .size=0.5f, .h=0 };
                sel_i = n_notes; drag = DRAG_END; drag_note = n_notes++;
                preview(&note_[sel_i]);
            }
        }
    }
    if (mouse_pressed(MOUSE_RIGHT) && in_roll){
        int i = hit_note(mx, my, 0);
        if (i >= 0){ mark(); del_note(i); }
    }

    // continue
    if (mouse_down(MOUSE_LEFT) && drag != DRAG_NONE){
        if (drag == DRAG_SCRUB){
            playhead = beat_ruler(mx);
        } else if (drag == DRAG_VIB && sel_i >= 0){
            float t = (float)(mx - SLD_X) / (SLD_W - 1); if (t < 0) t = 0; if (t > 1) t = 1;
            note_[sel_i].vib = t; preview(&note_[sel_i]);
        } else if (drag == DRAG_FRM && sel_i >= 0){
            float t = (float)(mx - SLD_X) / (SLD_W - 1); if (t < 0) t = 0; if (t > 1) t = 1;
            note_[sel_i].size = t; preview(&note_[sel_i]);
        } else if (drag == DRAG_FORMANT && drag_note >= 0){
            Note *g = &note_[drag_note];
            float sz = g->osize + (grab_my - my) / 70.0f;      // drag UP = larger size = child
            if (sz < 0) sz = 0; if (sz > 1) sz = 1;
            g->size = sz; preview(g);
        } else if (drag_note >= 0 && drag_note < n_notes){
            Note *g = &note_[drag_note];
            float b = beat_of_x(mx);
            if (drag == DRAG_MOVE){
                float s = snapf(b - drag_off);
                if (s < 0) s = 0; if (s + g->len > SONG_LEN) s = SONG_LEN - g->len;
                g->start = s;
                int p = pitch_of_y(my); if (p < 0) p = 0; if (p > 127) p = 127;
                g->pitch = (int8_t)p; preview(g);
            } else if (drag == DRAG_END){
                float e = snapf(b); g->len = e - g->start; if (g->len < snap) g->len = snap;
            } else if (drag == DRAG_START){
                float s = snapf(b), end = g->start + g->len;
                if (s > end - snap) s = end - snap; if (s < 0) s = 0;
                g->start = s; g->len = end - s;
            }
        }
    }

    if (mouse_released(MOUSE_LEFT)){
        preview_stop();
        if (gest_open){ commit(&pregest); gest_open = false; }
        drag = DRAG_NONE; drag_note = -1;
    }
    if (!mouse_down(MOUSE_LEFT)) drag = DRAG_NONE;

    // cursor
    int want = CURSOR_DEFAULT;
    if (panning || drag == DRAG_MOVE) want = CURSOR_MOVE;
    else if (drag == DRAG_END || drag == DRAG_START || drag == DRAG_SCRUB || in_ruler) want = CURSOR_RESIZE_H;
    else if (in_formant_tab(mx, my)) want = CURSOR_MOVE;
    else if (in_roll){
        int z, i = hit_note(mx, my, &z);
        if (i >= 0) want = (z == DRAG_MOVE) ? CURSOR_MOVE : CURSOR_RESIZE_H;
        else want = CURSOR_CROSSHAIR;
    }
    if (want != cur_cursor){ mouse_cursor(want); cur_cursor = want; }

    perform();

#ifdef DE_TRACE
    watch("playing", "%d", playing);
    watch("phead",   "%.2f", playhead);
    watch("nnotes",  "%d", n_notes);
    watch("sel",     "%d", sel_i);
    watch("perf_on", "%d", perf_h >= 0);
    { Samp s = sample_at(playhead); watch("cpitch", "%.2f", s.pitch); watch("down", "%d", s.down); }
#endif
}

// --- draw -------------------------------------------------------------------
static void draw_roll_bg(void){
    int rows = vis_rows();
    for (int r = 0; r < rows; r++){
        int m = view_lo + r, y = y_top(m);
        int bg = (m % 12 == 0) ? CLR_DARK_PURPLE : (is_black(m) ? CLR_DARKER_BLUE : CLR_DARK_BLUE);
        rectfill(ROLL_X, y, SCREEN_W - ROLL_X, ROWH, bg);
        // piano key column
        rectfill(0, y, PIANO_W - 1, ROWH, is_black(m) ? CLR_BLACK : CLR_LIGHT_GREY);
        if (m % 12 == 0){ char nb[5]; note_name(m, nb); font(FONT_TINY); print(nb, 1, y + 1, CLR_DARK_GREY); font(FONT_NORMAL); }
    }
    for (float bb = ceilf(view_b0); bb < view_b0 + vis_beats(); bb += 1){
        int x = x_of_beat(bb);
        line(x, ROLL_Y, x, ROLL_Y + ROLL_H, ((int)bb % 4 == 0) ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
    }
}

static void draw_blob(int i){
    Note *bl = &note_[i];
    int x  = x_of_beat(bl->start), x2 = x_of_beat(bl->start + bl->len);
    if (x2 < ROLL_X || x > SCREEN_W) return;
    int y = y_top(bl->pitch);
    if (y + ROWH <= ROLL_Y || y >= ROLL_Y + ROLL_H) return;
    int w = x2 - x - 1; if (w < 6) w = 6;
    int h = ROWH - 1;
    bool sel = (i == sel_i);
    bool active = (playing || drag == DRAG_SCRUB) && playhead >= bl->start && playhead < bl->start + bl->len;

    int body = active ? CLR_LIGHT_PEACH : CLR_PEACH;
    int top  = active ? CLR_WHITE : CLR_LIGHT_PEACH;
    if (sel){ body = active ? CLR_LIGHT_YELLOW : CLR_YELLOW; }

    rrectfill(x, y, w, h, 3, body);
    int fband = 2 + (int)(bl->size * (h - 3));          // lighter top band reads the formant SIZE
    rrectfill(x, y, w, fband, 3, top);
    font(FONT_SMALL);
    print(VOWELS[bl->vowel], x + 2, y + h / 2 - 3, sel ? CLR_BLACK : CLR_DARK_PURPLE);
    font(FONT_NORMAL);

    if (sel){
        rrect(x - 1, y - 1, w + 2, h + 2, 4, CLR_WHITE);
        int cx = (x + x2) / 2;
        line(cx, y - 8, cx, y - 1, CLR_WHITE);          // stem
        rrectfill(cx - 4, y - 9, 9, 5, 2, CLR_WHITE);   // the ✱ grab tab
        print("\x0f", cx - 3, y - 10, CLR_BLACK);
    }
}

static void draw_contour(void){
    int have = 0, px = 0, pyv = 0;
    for (int x = ROLL_X; x <= SCREEN_W; x++){
        Samp s = sample_at(beat_of_x(x));
        if (s.down){
            float p = s.pitch;
            if (s.note >= 0){                            // vibrato wobble lives inside notes only
                float local = beat_of_x(x) - note_[s.note].start;
                p += sinf(local * 7.0f + note_[s.note].start * 3.1f) * (s.vib * 0.85f);
            }
            int y = yf(p);
            if (have){ line(px, pyv + 1, x, y + 1, CLR_DARK_PEACH); line(px, pyv, x, y, CLR_WHITE); }
            px = x; pyv = y; have = 1;
        } else have = 0;
    }
}

static void draw_ruler(void){
    rectfill(0, RULER_Y, SCREEN_W, RULER_H, CLR_DARK_GREY);
    for (int b = 0; b <= (int)SONG_LEN; b += 4){
        int x = x_ruler(b);
        line(x, RULER_Y, x, RULER_Y + RULER_H, CLR_DARKER_GREY);
        font(FONT_TINY); char s[4]; snprintf(s, sizeof s, "%d", b / 4 + 1);
        print(s, x + 2, RULER_Y + 1, CLR_MEDIUM_GREY); font(FONT_NORMAL);
    }
    int wx0 = x_ruler(view_b0), wx1 = x_ruler(view_b0 + vis_beats());
    rect(wx0, RULER_Y, wx1 - wx0, RULER_H - 1, CLR_MEDIUM_GREY);
    int hx = x_ruler(playhead);
    line(hx, RULER_Y, hx, RULER_Y + RULER_H, CLR_WHITE);
    rectfill(hx - 2, RULER_Y, 5, 3, CLR_WHITE);
}

static void hbtn(int b, const char *label, bool lit, int col){
    rectfill(HB[b].x, 1, HB[b].w, HUD_H - 2, lit ? col : CLR_DARK_GREY);
    print(label, HB[b].x + 2, 2, CLR_WHITE);
}
static void draw_hud(void){
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_SMALL);
    hbtn(H_PLAY, playing ? "STOP" : "PLAY", playing, CLR_GREEN);
    hbtn(H_UNDO, "<", u_len > 0, CLR_INDIGO);
    hbtn(H_REDO, ">", r_len > 0, CLR_INDIGO);
    const char *sn = snap >= 1.0f ? "1/4" : snap >= 0.5f ? "1/8" : snap >= 0.25f ? "1/16" : "1/32";
    hbtn(H_SNAP, sn, false, 0);
    char tb[10]; snprintf(tb, sizeof tb, "%d bpm", tempo);
    print(tb, HB[H_SNAP].x + HB[H_SNAP].w + 4, 2, CLR_MEDIUM_GREY);
    print("VOXROLL", 150, 2, CLR_LIGHT_PEACH);
    hbtn(H_SAVE, "SAVE", false, 0);
    hbtn(H_LOAD, "LOAD", false, 0);
    font(FONT_NORMAL);
}

static void draw_slider(int x, int y, int w, float v, int col){
    rectfill(x, y, w, 4, CLR_DARK_GREY);
    int fx = x + (int)(v * (w - 1));
    rectfill(x, y, fx - x + 1, 4, col);
    rectfill(fx - 1, y - 2, 3, 8, CLR_WHITE);
}
static void draw_inspector(void){
    rectfill(0, INSP_Y, SCREEN_W, INSP_H, CLR_BROWNISH_BLACK);
    line(0, INSP_Y, SCREEN_W, INSP_Y, CLR_DARK_GREY);
    font(FONT_SMALL);
    if (sel_i < 0){
        print("click a blob to sculpt it   drag empty space to draw one", 4, INSP_Y + 18, CLR_MEDIUM_GREY);
        print("touching notes GLIDE (legato) - the white line is the sung pitch", 4, INSP_Y + 32, CLR_DARK_GREY);
        font(FONT_NORMAL);
        return;
    }
    Note *s = &note_[sel_i];
    char nb[5]; note_name(s->pitch, nb);
    int szi = s->size < 0.4f ? 0 : (s->size > 0.6f ? 2 : 1);
    char rd[48]; snprintf(rd, sizeof rd, "%s   %s   %s   vib %d%%", nb, VOWELS[s->vowel], SIZES[szi], (int)(s->vib * 100));
    print(rd, 4, INSP_Y + 2, CLR_WHITE);

    for (int v = 0; v < 5; v++){
        bool on = (v == s->vowel);
        rrectfill(VCHIP[v].x, VCHIP_Y, VCHIP[v].w, VCHIP_H, 2, on ? CLR_YELLOW : CLR_DARK_GREY);
        print(VOWELS[v], VCHIP[v].x + 5, VCHIP_Y + 3, on ? CLR_BLACK : CLR_LIGHT_GREY);
    }
    print("VIB", 132, VIB_Y - 3, CLR_LIGHT_PEACH);   draw_slider(SLD_X, VIB_Y, SLD_W, s->vib,  CLR_LIGHT_PEACH);
    print("FRM", 132, FRM_Y - 3, CLR_LIGHT_YELLOW);  draw_slider(SLD_X, FRM_Y, SLD_W, s->size, CLR_LIGHT_YELLOW);
    print(SIZES[szi], SLD_X + SLD_W + 4, FRM_Y - 3, CLR_LIGHT_GREY);
    print("drag \x0f tab = formant (pitch stays)", 4, INSP_Y + VCHIP_H + 20, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw(void){
    cls(CLR_BROWNISH_BLACK);
    draw_roll_bg();
    for (int i = 0; i < n_notes; i++) if (i != sel_i) draw_blob(i);
    if (sel_i >= 0) draw_blob(sel_i);            // selected on top (its tab + ring)
    draw_contour();
    if (playhead >= view_b0 && playhead <= view_b0 + vis_beats()){
        int px = x_of_beat(playhead);
        line(px, ROLL_Y, px, ROLL_Y + ROLL_H, CLR_WHITE);
    }
    draw_ruler();
    draw_hud();
    draw_inspector();
}
