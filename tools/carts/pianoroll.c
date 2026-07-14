/* de:meta
{
  "slug": "pianoroll",
  "title": "piano roll",
  "kind": ["instrument", "toy"],
  "teaches": ["song-arrangement", "note-editing"],
  "created": "2026-07-13",
  "lineage": "sampler",
  "description": {
    "summary": "A scrubbable piano roll: play notes in on the keybed, draw & reshape them with the mouse, set velocity, lock to a scale, and save your song.",
    "detail": "Time runs left–right, pitch runs bottom–up. Notes live in BEATS, so tempo and scrubbing are free. Playback is stateless: every frame each note asks 'should I be sounding at the playhead right now?' — so you can drag the playhead anywhere on the ruler and instantly hear the composition at that point, no need to replay from the top. Two mouse TOOLS (toggle with B, or the DRAW/SEL chip): in DRAW, click empty to place a note and drag to size it; in SELECT, drag empty to marquee many notes, then move/duplicate/delete them together. In both, a note's body moves and its edges retime; right-click deletes one. Record live off the keybed too. The bottom panel flips between the KEYBED and a VELOCITY lane. SCALE-lock snaps every pitch into key. Your song auto-saves and reloads.",
    "controls": "keybed / QWERTY: play notes · SPACE: play/stop · R: arm record · B: DRAW/SELECT tool · DRAW: click empty=place, drag=size · SELECT: drag empty=marquee · drag body: move (all selected) · drag edge: resize · right-click: delete one · BACKSPACE: delete selected · , / . : undo / redo (or the < > buttons) · HUD: DUP · VEL/KEYS panel · SCL scale-lock (+root/type) · SAVE / LOAD · snap & tempo · MIDDLE-drag: pan the view around · wheel/↑↓: pitch · ←→: time · drag RULER: scrub (audible)"
  }
}
de:meta */

// piano roll — the composition surface the audio shelf was missing.
//
// sampler chops a SOUND and arranges slices on a step grid; this arranges
// PITCHES over time. It reuses the same live-capture trick sampler uses
// (keybed on_note/on_off callbacks log what you play), but the payload is a
// note {pitch, start, len, vel} instead of a slice trigger.
//
// The one idea that makes the whole thing feel good: playback is STATELESS.
// There's no event queue counting down. Every frame, each note just asks
// "is the playhead inside me right now, and is anything audible?" and turns
// its voice on/off accordingly. Move the playhead anywhere — scrub the ruler,
// loop, jump — and the correct notes start and stop with zero bookkeeping.

#include <stdio.h>     // snprintf — HUD/label text
#include <stdlib.h>    // abs — marquee bounds
#include <string.h>    // memmove/memcpy — undo stacks
#include "studio.h"
#include "keybed.h"

#define SYN       5            // instrument slot the keybed + playback both use
#define MAXNOTES  512
#define SONG_LEN  16.0f        // song length in beats (4 bars of 4/4)

// --- layout -----------------------------------------------------------------
#define HUD_H     10           // toolbar row
#define RULER_Y   HUD_H
#define RULER_H   12
#define ROLL_Y    (RULER_Y + RULER_H)
#define KB_H      50           // bottom panel: keybed manual OR velocity lane
#define KB_Y      (SCREEN_H - KB_H)
#define ROLL_H    (SCREEN_H - KB_H - ROLL_Y)
#define PIANO_W   16           // pitch-key column on the left
#define ROLL_X    PIANO_W
#define ROLL_W    (SCREEN_W - PIANO_W)
#define ROWH      6            // pixels per semitone
#define PXB       28.0f        // pixels per beat
#define EDGE      4            // px grab zone for a note's start/end edge

typedef struct {
    int8_t  pitch;             // MIDI 0..127
    float   start, len;        // in beats
    uint8_t vel;               // 1..7
    uint8_t rec;               // 1 while being recorded live (keybed owns the sound then)
    uint8_t sel;               // 1 if in the current selection
    int     h;                 // live voice handle during playback/scrub, -1 = silent
    float   ostart;            // originals snapshotted at drag-start (for group move)
    int8_t  opitch;
} Note;

// compact note for save files AND undo snapshots (only the musical fields)
typedef struct { int8_t pitch; float start, len; uint8_t vel; } SNote;
typedef struct { int n; SNote notes[MAXNOTES]; } Snap;

static Note  note_[MAXNOTES];
static int   n_notes;

static bool  playing;
static bool  armed;                    // record-arm
static float playhead;                 // beats
static int   tempo = 120;

static int   view_lo = 48;             // lowest visible pitch (C3)
static float view_b0 = 0.0f;           // leftmost visible beat
static float snap  = 0.25f;            // current grid: 1/4 .. 1/32
static bool  velmode;                  // bottom panel shows the velocity lane, not the keybed

static int   rec_map[128];             // midi -> index of the note being recorded, -1 none

// --- scales -----------------------------------------------------------------
static const int  SC_MAJ[]  = { 0, 2, 4, 5, 7, 9, 11 };
static const int  SC_MIN[]  = { 0, 2, 3, 5, 7, 8, 10 };
static const int  SC_PENT[] = { 0, 2, 4, 7, 9 };
static const struct { const char *name; const int *deg; int n; } SCALES[] = {
    { "MAJ", SC_MAJ, 7 }, { "MIN", SC_MIN, 7 }, { "PENT", SC_PENT, 5 },
};
#define NSCALE  ((int)(sizeof SCALES / sizeof SCALES[0]))
static const char *NOTE_NAME[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

static bool scalelock;
static int  scale_root;                // pitch class 0..11
static int  scale_idx;                 // index into SCALES

enum { TOOL_DRAW, TOOL_SELECT };       // DRAW: empty = place/size a note · SELECT: empty = marquee
static int   tool = TOOL_DRAW;

enum { DRAG_NONE, DRAG_MOVE, DRAG_END, DRAG_START, DRAG_SCRUB, DRAG_MARQUEE, DRAG_VEL };
static int   drag;
static int   drag_note;
static float drag_off;                 // beat offset (cursor - grabbed note.start) while moving
static int   mq_x0, mq_y0;             // marquee anchor (px)
static bool  mq_moved;                 // did the press turn into a real drag?
static int   cur_cursor = -1;
static bool  panning;                  // middle-drag: grab & pan the view
static int   pan_mx, pan_my, pan_lo;   // anchors captured at pan-start
static float pan_b0;

// --- helpers ----------------------------------------------------------------
static int   vis_rows(void) { return ROLL_H / ROWH; }
static float vis_beats(void){ return ROLL_W / PXB; }
static bool  is_black(int midi){ int pc = ((midi % 12) + 12) % 12;
    return pc==1||pc==3||pc==6||pc==8||pc==10; }

static float snapf(float b){ float s = roundf(b / snap) * snap; return s < 0 ? 0 : s; }

static int   x_of_beat(float b){ return ROLL_X + (int)((b - view_b0) * PXB); }
static float beat_of_x(int px){ return view_b0 + (px - ROLL_X) / PXB; }
static int   x_of_beat_ruler(float b){ return (int)(b / SONG_LEN * SCREEN_W); }
static float beat_of_x_ruler(int px){ float b = (float)px / SCREEN_W * SONG_LEN;
    return b < 0 ? 0 : b > SONG_LEN ? SONG_LEN : b; }
static int   y_of_pitch(int m){ return ROLL_Y + ROLL_H - (m - view_lo + 1) * ROWH; }
static int   pitch_of_y(int py){ return view_lo + (ROLL_Y + ROLL_H - 1 - py) / ROWH; }

static int vel_vol(int v){ return v < 1 ? 1 : v > 7 ? 7 : v; }

static float max_b0(void){ float m = SONG_LEN - vis_beats(); return m < 0 ? 0 : m; }
static void  clamp_view(void){
    if (view_lo < 12) view_lo = 12;
    if (view_lo > 108 - vis_rows()) view_lo = 108 - vis_rows();
    if (view_b0 < 0) view_b0 = 0;
    if (view_b0 > max_b0()) view_b0 = max_b0();
}

static bool in_scale(int midi){
    int pc = (((midi - scale_root) % 12) + 12) % 12;
    for (int i = 0; i < SCALES[scale_idx].n; i++) if (SCALES[scale_idx].deg[i] == pc) return true;
    return false;
}
// nearest in-key pitch (only when scale-lock is on)
static int scale_snap(int midi){
    if (!scalelock || in_scale(midi)) return midi;
    for (int d = 1; d <= 6; d++){
        if (in_scale(midi - d)) return midi - d;
        if (in_scale(midi + d)) return midi + d;
    }
    return midi;
}

static void all_notes_off(void){
    for (int i = 0; i < n_notes; i++) if (note_[i].h >= 0){ note_off(note_[i].h); note_[i].h = -1; }
}
static void clear_sel(void){ for (int i = 0; i < n_notes; i++) note_[i].sel = 0; }
static int  count_sel(void){ int c = 0; for (int i = 0; i < n_notes; i++) c += note_[i].sel; return c; }

// --- undo / redo: snapshots of the note list --------------------------------
#define UNDO_MAX 32
static Snap ustack[UNDO_MAX]; static int u_len;
static Snap rstack[UNDO_MAX]; static int r_len;

static void snap_take(Snap *s){
    s->n = n_notes;
    for (int i = 0; i < n_notes; i++){
        s->notes[i].pitch = note_[i].pitch; s->notes[i].start = note_[i].start;
        s->notes[i].len   = note_[i].len;   s->notes[i].vel   = note_[i].vel;
    }
}
static bool snap_diff(const Snap *s){          // does the live state differ from snapshot s?
    if (s->n != n_notes) return true;
    for (int i = 0; i < n_notes; i++)
        if (s->notes[i].pitch != note_[i].pitch || s->notes[i].start != note_[i].start ||
            s->notes[i].len   != note_[i].len   || s->notes[i].vel   != note_[i].vel) return true;
    return false;
}
static void snap_apply(const Snap *s){
    for (int i = 0; i < n_notes; i++) if (note_[i].h >= 0){ note_off(note_[i].h); note_[i].h = -1; }
    n_notes = s->n;
    for (int i = 0; i < n_notes; i++){
        note_[i].pitch = s->notes[i].pitch; note_[i].start = s->notes[i].start;
        note_[i].len   = s->notes[i].len;   note_[i].vel   = s->notes[i].vel;
        note_[i].rec = 0; note_[i].sel = 0; note_[i].h = -1;
    }
    for (int m = 0; m < 128; m++) rec_map[m] = -1;
}
static void stack_push(Snap *st, int *len, const Snap *s){
    if (*len == UNDO_MAX){ memmove(st, st + 1, (UNDO_MAX - 1) * sizeof *st); (*len)--; }
    st[(*len)++] = *s;
}
// record an undo point from the CURRENT state, before a discrete mutation (kills redo)
static void mark(void){ Snap s; snap_take(&s); stack_push(ustack, &u_len, &s); r_len = 0; }
// commit an already-captured pre-gesture snapshot, only if the gesture changed anything
static void commit(const Snap *pre){ if (snap_diff(pre)){ stack_push(ustack, &u_len, pre); r_len = 0; } }
static void do_undo(void){ if (!u_len) return; Snap cur; snap_take(&cur); stack_push(rstack, &r_len, &cur); snap_apply(&ustack[--u_len]); }
static void do_redo(void){ if (!r_len) return; Snap cur; snap_take(&cur); stack_push(ustack, &u_len, &cur); snap_apply(&rstack[--r_len]); }

static Snap pregest; static bool gest_open;   // pre-gesture snapshot + "a mouse edit is underway"

// --- live recording ---------------------------------------------------------
static void rec_on(int midi, int vel){
    if (!(playing && armed) || n_notes >= MAXNOTES) return;
    if (midi < 0 || midi > 127) return;
    mark();                            // each recorded note is its own undo step
    midi = scale_snap(midi);
    Note *nt = &note_[n_notes];
    nt->pitch = (int8_t)midi;
    nt->start = playhead;
    nt->len   = snap;
    nt->vel   = (uint8_t)vel_vol(vel > 7 ? vel * 7 / 127 : vel);
    nt->rec   = 1; nt->sel = 0; nt->h = -1;
    rec_map[midi] = n_notes++;
}
static void rec_off(int midi){
    if (midi < 0 || midi > 127) return;
    int i = rec_map[midi];
    if (i >= 0){ note_[i].rec = 0; rec_map[midi] = -1; }
}

// --- stateless playback ------------------------------------------------------
static void audition(void){
    bool audible = playing || drag == DRAG_SCRUB;
    for (int i = 0; i < n_notes; i++){
        Note *nt = &note_[i];
        bool on = audible && !nt->rec &&
                  playhead >= nt->start && playhead < nt->start + nt->len;
        if (on && nt->h < 0)  nt->h = note_on(nt->pitch, SYN, vel_vol(nt->vel));
        else if (!on && nt->h >= 0){ note_off(nt->h); nt->h = -1; }
    }
}

// --- editing ----------------------------------------------------------------
static int hit_note(int px, int py, int *zone){
    for (int i = n_notes - 1; i >= 0; i--){
        Note *nt = &note_[i];
        int y = y_of_pitch(nt->pitch);
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
    if (note_[i].h >= 0) note_off(note_[i].h);
    note_[i] = note_[--n_notes];
    for (int m = 0; m < 128; m++){
        if (rec_map[m] == i) rec_map[m] = -1;
        else if (rec_map[m] == n_notes) rec_map[m] = i;
    }
}
static void del_selected(void){
    for (int i = n_notes - 1; i >= 0; i--) if (note_[i].sel) del_note(i);
}

// duplicate the selection, offset by its own span so copies land right after it
static void dup_selected(void){
    int base = n_notes;
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < base; i++) if (note_[i].sel){
        if (note_[i].start < lo) lo = note_[i].start;
        if (note_[i].start + note_[i].len > hi) hi = note_[i].start + note_[i].len;
    }
    if (hi < lo) return;                        // nothing selected
    float span = hi - lo; if (span < snap) span = snap;
    for (int i = 0; i < base && n_notes < MAXNOTES; i++){
        if (!note_[i].sel) continue;
        Note c = note_[i];
        c.start += span; c.rec = 0; c.h = -1; c.sel = 1;
        note_[i].sel = 0;                       // move selection to the copies
        if (c.start + c.len <= SONG_LEN) note_[n_notes++] = c;
    }
}

// --- persistence ------------------------------------------------------------
typedef struct {
    uint32_t magic; int ver;
    int n, tempo, root, sidx; uint8_t lock; float snap;
    SNote notes[MAXNOTES];
} SaveBlob;
#define SAVE_MAGIC 0x50524C4Cu   // 'PRLL'

static void save_song(void){
    SaveBlob b; b.magic = SAVE_MAGIC; b.ver = 1;
    b.n = n_notes; b.tempo = tempo; b.root = scale_root; b.sidx = scale_idx;
    b.lock = scalelock; b.snap = snap;
    for (int i = 0; i < n_notes; i++){
        b.notes[i].pitch = note_[i].pitch; b.notes[i].start = note_[i].start;
        b.notes[i].len = note_[i].len;     b.notes[i].vel   = note_[i].vel;
    }
    save_bytes(&b, sizeof b);
}
static bool load_song(void){
    SaveBlob b;
    int got = load_bytes(&b, sizeof b);
    if (got < (int)sizeof b || b.magic != SAVE_MAGIC || b.ver != 1) return false;
    if (b.n < 0 || b.n > MAXNOTES) return false;
    all_notes_off();
    n_notes = b.n; tempo = b.tempo; scale_root = b.root % 12; scale_idx = b.sidx % NSCALE;
    scalelock = b.lock; snap = b.snap; if (snap <= 0) snap = 0.25f;
    bpm(tempo);
    for (int i = 0; i < n_notes; i++){
        note_[i].pitch = b.notes[i].pitch; note_[i].start = b.notes[i].start;
        note_[i].len = b.notes[i].len; note_[i].vel = b.notes[i].vel;
        note_[i].rec = 0; note_[i].sel = 0; note_[i].h = -1;
    }
    for (int m = 0; m < 128; m++) rec_map[m] = -1;
    return true;
}

// --- toolbar buttons --------------------------------------------------------
// one table so draw + hit-test never drift. {x, w, label}
enum { B_PLAY, B_REC, B_DUP, B_DEL, B_VEL, B_SCL, B_ROOT, B_TYPE, B_SAVE, B_LOAD, B_SNAP, B_UNDO, B_REDO, B_TOOL, B_N };
static const struct { int x, w; } BTN[B_N] = {
    {  2, 22 }, { 26, 16 }, { 44, 16 }, { 62, 16 }, { 80, 22 }, { 104, 16 },
    { 122, 12 }, { 136, 20 }, { 158, 22 }, { 182, 22 }, { 206, 22 },
    { 246, 14 }, { 262, 14 }, { 286, 32 },
};
static bool btn_click(int b){
    int mx = mouse_x(), my = mouse_y();
    return mouse_pressed(MOUSE_LEFT) &&
           mx >= BTN[b].x && mx < BTN[b].x + BTN[b].w && my >= 1 && my < HUD_H - 1;
}

static void seed_demo(void){
    static const struct { int p; float s, l; } demo[] = {
        {60,0.0f,0.5f},{64,0.5f,0.5f},{67,1.0f,0.5f},{64,1.5f,0.5f},
        {69,2.0f,1.0f},{67,3.0f,1.0f},{65,4.0f,0.5f},{64,4.5f,0.5f},
        {62,5.0f,0.5f},{60,5.5f,1.5f},
    };
    for (int i = 0; i < (int)(sizeof demo / sizeof demo[0]); i++){
        note_[n_notes] = (Note){ .pitch=(int8_t)demo[i].p, .start=demo[i].s,
                                 .len=demo[i].l, .vel=5, .h=-1 };
        n_notes++;
    }
}

void init(void){
    bpm(tempo);
    instrument(SYN, INSTR_SAW, 6, 90, 5, 160);
    keybed_config(SYN, 4, 14);
    keybed_layout(0, KB_Y, SCREEN_W, KB_H);
    keybed_on_note(rec_on);
    keybed_on_off(rec_off);
    for (int m = 0; m < 128; m++) rec_map[m] = -1;
    drag = DRAG_NONE;
    if (!load_song()) seed_demo();          // your song persists; else the demo phrase
}

void update(void){
    if (!velmode) keybed_update();          // keybed owns the bottom panel unless VEL lane is up

    // --- keys (all off the keybed's A–L / W E / T Y U / O P / Z X set) ---
    if (keyp(' ')){ playing = !playing; if (!playing) all_notes_off(); }
    if (keyp('R')) armed = !armed;
    if (keyp('B')) tool = !tool;             // toggle DRAW <-> SELECT (Ableton's draw-mode key)
    if (keyp(',')) do_undo();                // ',' / '.' = the < > keys (off the keybed's letters)
    if (keyp('.')) do_redo();
    if (keyp(KEY_BACKSPACE) && count_sel()){ mark(); del_selected(); }
    if (keyp('[')){ tempo -= 5; if (tempo < 40) tempo = 40; bpm(tempo); }
    if (keyp(']')){ tempo += 5; if (tempo > 240) tempo = 240; bpm(tempo); }

    // --- toolbar ---
    if (btn_click(B_PLAY)){ playing = !playing; if (!playing) all_notes_off(); }
    if (btn_click(B_REC))  armed = !armed;
    if (btn_click(B_DUP) && count_sel()){ mark(); dup_selected(); }
    if (btn_click(B_DEL) && count_sel()){ mark(); del_selected(); }
    if (btn_click(B_VEL))  velmode = !velmode;
    if (btn_click(B_SCL))  scalelock = !scalelock;
    if (btn_click(B_ROOT)) scale_root = (scale_root + 1) % 12;
    if (btn_click(B_TYPE)) scale_idx  = (scale_idx + 1) % NSCALE;
    if (btn_click(B_SAVE)) save_song();
    if (btn_click(B_LOAD)){ Snap pre; snap_take(&pre); if (load_song()) commit(&pre); }
    if (btn_click(B_UNDO)) do_undo();
    if (btn_click(B_REDO)) do_redo();
    if (btn_click(B_SNAP)){                 // cycle 1/4 -> 1/8 -> 1/16 -> 1/32
        snap = (snap <= 0.125f) ? 1.0f : snap * 0.5f;
    }
    if (btn_click(B_TOOL)) tool = !tool;

    // --- scroll ---
    float w = mouse_wheel();
    if (w != 0) view_lo += (w > 0 ? 1 : -1);
    if (keyp(KEY_UP))    view_lo++;
    if (keyp(KEY_DOWN))  view_lo--;
    if (keyp(KEY_LEFT))  view_b0 -= 1;
    if (keyp(KEY_RIGHT)) view_b0 += 1;
    clamp_view();

    // --- advance the playhead ---
    if (playing){
        playhead += dt() * tempo / 60.0f;
        if (playhead >= SONG_LEN) playhead -= SONG_LEN;
        for (int m = 0; m < 128; m++) if (rec_map[m] >= 0){
            Note *nt = &note_[rec_map[m]];
            float len = playhead - nt->start;
            if (len > nt->len) nt->len = len;
        }
        if (playhead < view_b0 || playhead > view_b0 + vis_beats() - 0.5f){
            view_b0 = playhead - 1; clamp_view();
        }
    }

    int mx = mouse_x(), my = mouse_y();
    bool in_roll  = mx >= ROLL_X && my >= ROLL_Y && my < ROLL_Y + ROLL_H;
    bool in_ruler = my >= RULER_Y && my < RULER_Y + RULER_H;
    bool in_vel   = velmode && my >= KB_Y;

    // --- middle-drag: grab & pan the view around (works in any tool) ---
    if (mouse_pressed(MOUSE_MIDDLE)){ panning = true; pan_mx = mx; pan_my = my; pan_b0 = view_b0; pan_lo = view_lo; }
    if (panning && mouse_down(MOUSE_MIDDLE)){
        view_b0 = pan_b0 - (mx - pan_mx) / PXB;      // drag right -> reveal earlier bars (grab-style)
        view_lo = pan_lo + (my - pan_my) / ROWH;     // drag down  -> reveal higher notes
        clamp_view();
    }
    if (!mouse_down(MOUSE_MIDDLE)) panning = false;

    // --- begin an interaction ---
    if (mouse_pressed(MOUSE_LEFT) && (in_roll || in_vel)){   // a note edit may start — snapshot to undo on release
        snap_take(&pregest); gest_open = true;
    }
    if (mouse_pressed(MOUSE_LEFT)){
        if (in_ruler){
            drag = DRAG_SCRUB;
        } else if (in_vel){
            drag = DRAG_VEL;                     // paint velocities in the lane
        } else if (in_roll){
            int zone, i = hit_note(mx, my, &zone);
            if (i >= 0){
                if (zone == DRAG_MOVE){
                    if (!note_[i].sel){ clear_sel(); note_[i].sel = 1; }  // grab picks this note
                    for (int k = 0; k < n_notes; k++)                    // snapshot for group move
                        if (note_[k].sel){ note_[k].ostart = note_[k].start; note_[k].opitch = note_[k].pitch; }
                    drag = DRAG_MOVE; drag_note = i; drag_off = beat_of_x(mx) - note_[i].start;
                } else {                                                 // edge resize = just this note
                    drag = zone; drag_note = i;
                }
            } else if (tool == TOOL_DRAW && n_notes < MAXNOTES){   // DRAW: place a note, drag to size it
                clear_sel();
                int p = scale_snap(pitch_of_y(my));
                note_[n_notes] = (Note){ .pitch=(int8_t)p, .start=snapf(beat_of_x(mx)),
                                         .len=snap, .vel=5, .sel=1, .h=-1 };
                drag = DRAG_END; drag_note = n_notes++;
            } else {                             // SELECT: press starts a marquee; a TAP (no move) deselects
                drag = DRAG_MARQUEE; mq_x0 = mx; mq_y0 = my; mq_moved = false;
            }
        }
    }
    if (mouse_pressed(MOUSE_RIGHT) && in_roll){   // right-click deletes one note
        int i = hit_note(mx, my, 0);
        if (i >= 0){ mark(); del_note(i); }
    }

    // --- continue an interaction ---
    if (mouse_down(MOUSE_LEFT) && drag != DRAG_NONE){
        if (drag == DRAG_SCRUB){
            playhead = beat_of_x_ruler(mx);
        } else if (drag == DRAG_VEL){
            int nv = (int)roundf((float)(SCREEN_H - my) / KB_H * 7.0f);
            nv = vel_vol(nv);
            for (int i = 0; i < n_notes; i++){    // set the note whose start sits under the cursor x
                int nx = x_of_beat(note_[i].start);
                if (mx >= nx - 3 && mx <= nx + 4) note_[i].vel = (uint8_t)nv;
            }
        } else if (drag == DRAG_MARQUEE){
            if (abs(mx - mq_x0) > 3 || abs(my - mq_y0) > 3) mq_moved = true;
        } else if (drag_note >= 0 && drag_note < n_notes){
            Note *g = &note_[drag_note];
            float b = beat_of_x(mx);
            if (drag == DRAG_MOVE){
                float target = snapf(b - drag_off);
                float dstart = target - g->ostart;
                int tp = scale_snap(pitch_of_y(my));
                int dpitch = tp - g->opitch;
                for (int i = 0; i < n_notes; i++) if (note_[i].sel){
                    float s = note_[i].ostart + dstart;
                    if (s < 0) s = 0; if (s + note_[i].len > SONG_LEN) s = SONG_LEN - note_[i].len;
                    note_[i].start = s;
                    int p = note_[i].opitch + dpitch;
                    if (p < 0) p = 0; if (p > 127) p = 127;
                    note_[i].pitch = (int8_t)p;
                }
            } else if (drag == DRAG_END){
                float e = snapf(b); g->len = e - g->start; if (g->len < snap) g->len = snap;
            } else if (drag == DRAG_START){
                float s = snapf(b), end = g->start + g->len;
                if (s > end - snap) s = end - snap; if (s < 0) s = 0;
                g->start = s; g->len = end - s;
            }
        }
    }

    // --- release: finalize marquee (select) or a tap (place) ---
    if (mouse_released(MOUSE_LEFT)){
        if (drag == DRAG_MARQUEE){
            if (!mq_moved){                                // a tap in empty (SELECT mode) = deselect
                clear_sel();
            } else {                                       // a drag = marquee-select
                int x0 = mx < mq_x0 ? mx : mq_x0, x1 = mx > mq_x0 ? mx : mq_x0;
                int y0 = my < mq_y0 ? my : mq_y0, y1 = my > mq_y0 ? my : mq_y0;
                clear_sel();
                for (int i = 0; i < n_notes; i++){
                    int nx0 = x_of_beat(note_[i].start), nx1 = x_of_beat(note_[i].start + note_[i].len);
                    int ny  = y_of_pitch(note_[i].pitch);
                    if (nx1 >= x0 && nx0 <= x1 && ny + ROWH >= y0 && ny <= y1) note_[i].sel = 1;
                }
            }
        }
        if (gest_open){ commit(&pregest); gest_open = false; }   // undo point iff the edit changed notes
        drag = DRAG_NONE;
    }
    if (!mouse_down(MOUSE_LEFT)) drag = DRAG_NONE;

    // --- context cursor ---
    int want = CURSOR_DEFAULT;
    if (panning)                                              want = CURSOR_MOVE;
    else if (drag == DRAG_MOVE)                               want = CURSOR_MOVE;
    else if (drag == DRAG_END || drag == DRAG_START || drag == DRAG_SCRUB) want = CURSOR_RESIZE_H;
    else if (in_ruler)                                        want = CURSOR_RESIZE_H;
    else if (in_vel)                                          want = CURSOR_RESIZE_H;
    else if (in_roll){
        int z, i = hit_note(mx, my, &z);
        if (i >= 0)               want = (z == DRAG_MOVE) ? CURSOR_MOVE : CURSOR_RESIZE_H;
        else if (tool == TOOL_DRAW) want = CURSOR_CROSSHAIR;   // empty + draw = place
        else                        want = CURSOR_DEFAULT;     // empty + select = marquee
    } else if (!velmode && my >= KB_Y)                        want = CURSOR_HAND;
    if (want != cur_cursor){ mouse_cursor(want); cur_cursor = want; }

    audition();

#ifdef DE_TRACE
    watch("playing", "%d", playing);
    watch("armed",   "%d", armed);
    watch("phead",   "%.2f", playhead);
    watch("nnotes",  "%d", n_notes);
    watch("nsel",    "%d", count_sel());
    watch("tool",    "%d", tool);
    watch("ulen",    "%d", u_len);
    watch("rlen",    "%d", r_len);
    watch("velmode", "%d", velmode);
    watch("lock",    "%d", scalelock);
    { int snd = 0; for (int i = 0; i < n_notes; i++) if (note_[i].h >= 0) snd++;
      watch("snd", "%d", snd); }
#endif
}

// --- draw -------------------------------------------------------------------
static void draw_roll(void){
    int rows = vis_rows();
    for (int r = 0; r < rows; r++){
        int m = view_lo + r, y = y_of_pitch(m);
        int bg;
        if (scalelock) bg = in_scale(m) ? CLR_DARKER_GREY : CLR_BLACK;      // in-key lanes lifted
        else           bg = is_black(m) ? CLR_BLACK : CLR_DARKER_GREY;      // piano striping
        rectfill(ROLL_X, y, ROLL_W, ROWH, bg);
        int pc = (((m - scale_root) % 12) + 12) % 12;
        bool rootrow = scalelock ? (pc == 0) : (m % 12 == 0);
        if (rootrow) line(ROLL_X, y + ROWH - 1, SCREEN_W, y + ROWH - 1, CLR_DARK_GREY);
    }
    for (float b = ceilf(view_b0); b < view_b0 + vis_beats(); b += 1){
        int x = x_of_beat(b);
        line(x, ROLL_Y, x, ROLL_Y + ROLL_H, ((int)b % 4 == 0) ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
    }
    for (int i = 0; i < n_notes; i++){
        Note *nt = &note_[i];
        int y = y_of_pitch(nt->pitch);
        if (y + ROWH <= ROLL_Y || y >= ROLL_Y + ROLL_H) continue;
        int x0 = x_of_beat(nt->start), x1 = x_of_beat(nt->start + nt->len);
        if (x1 < ROLL_X || x0 > SCREEN_W) continue;
        if (x0 < ROLL_X) x0 = ROLL_X;
        int w = x1 - x0; if (w < 2) w = 2;
        int fill = nt->rec ? CLR_RED
                 : nt->h >= 0 ? CLR_YELLOW
                 : nt->sel ? CLR_ORANGE
                 : (nt->vel >= 4 ? CLR_GREEN : CLR_DARK_GREEN);   // brighter = louder
        rectfill(x0, y, w, ROWH, fill);
        rect(x0, y, w, ROWH, nt->sel ? CLR_WHITE : CLR_DARK_GREEN);
    }
    if (playhead >= view_b0 && playhead <= view_b0 + vis_beats()){
        int px = x_of_beat(playhead);
        line(px, ROLL_Y, px, ROLL_Y + ROLL_H, CLR_WHITE);
    }
    // marquee rubber-band
    if (drag == DRAG_MARQUEE && mq_moved){
        int mx = mouse_x(), my = mouse_y();
        int x0 = mx < mq_x0 ? mx : mq_x0, y0 = my < mq_y0 ? my : mq_y0;
        rect(x0, y0, abs(mx - mq_x0), abs(my - mq_y0), CLR_WHITE);
    }
}

static void draw_piano_col(void){
    int rows = vis_rows();
    for (int r = 0; r < rows; r++){
        int m = view_lo + r, y = y_of_pitch(m);
        bool held = keybed_held(m);
        int col;
        if (held)           col = CLR_YELLOW;
        else if (scalelock) col = in_scale(m) ? CLR_LIGHT_GREY : CLR_DARK_GREY;
        else                col = is_black(m) ? CLR_DARK_GREY : CLR_LIGHT_GREY;
        rectfill(0, y, PIANO_W - 1, ROWH, col);
        if (m % 12 == 0){
            font(FONT_TINY);
            char s[6]; snprintf(s, sizeof s, "C%d", m / 12 - 1);
            print(s, 1, y, CLR_BLACK);
            font(FONT_NORMAL);
        }
    }
}

static void draw_ruler(void){
    rectfill(0, RULER_Y, SCREEN_W, RULER_H, CLR_DARK_GREY);
    for (int b = 0; b <= (int)SONG_LEN; b++){
        if (b % 4) continue;
        int x = x_of_beat_ruler(b);
        line(x, RULER_Y, x, RULER_Y + RULER_H, CLR_DARKER_GREY);
        font(FONT_TINY); char s[4]; snprintf(s, sizeof s, "%d", b / 4 + 1);
        print(s, x + 2, RULER_Y + 1, CLR_MEDIUM_GREY); font(FONT_NORMAL);
    }
    int wx0 = x_of_beat_ruler(view_b0), wx1 = x_of_beat_ruler(view_b0 + vis_beats());
    rect(wx0, RULER_Y, wx1 - wx0, RULER_H - 1, CLR_MEDIUM_GREY);
    int hx = x_of_beat_ruler(playhead);
    line(hx, RULER_Y, hx, RULER_Y + RULER_H, CLR_WHITE);
    rectfill(hx - 2, RULER_Y, 5, 3, CLR_WHITE);
}

// the velocity lane replaces the keybed in the bottom panel
static void draw_vel_lane(void){
    rectfill(0, KB_Y, SCREEN_W, KB_H, CLR_BLACK);
    line(0, KB_Y, SCREEN_W, KB_Y, CLR_DARK_GREY);
    font(FONT_TINY); print("VELOCITY", 2, KB_Y + 2, CLR_MEDIUM_GREY); font(FONT_NORMAL);
    for (int i = 0; i < n_notes; i++){
        int x = x_of_beat(note_[i].start);
        if (x < ROLL_X || x > SCREEN_W) continue;
        int h = note_[i].vel * (KB_H - 6) / 7;
        int col = note_[i].sel ? CLR_ORANGE : (note_[i].vel >= 4 ? CLR_GREEN : CLR_DARK_GREEN);
        rectfill(x, SCREEN_H - h, 3, h, col);
    }
}

static void hud_btn(int b, const char *label, bool lit, int litcol){
    rectfill(BTN[b].x, 1, BTN[b].w, HUD_H - 2, lit ? litcol : CLR_DARK_GREY);
    print(label, BTN[b].x + 2, 2, lit && litcol == CLR_GREEN ? CLR_BLACK : CLR_WHITE);
}

static void draw_hud(void){
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_SMALL);
    hud_btn(B_PLAY, playing ? "STOP" : "PLAY", playing, CLR_GREEN);
    hud_btn(B_REC,  "REC", armed, CLR_RED);
    hud_btn(B_DUP,  "DUP", false, 0);
    hud_btn(B_DEL,  "DEL", false, 0);
    hud_btn(B_VEL,  velmode ? "KEYS" : "VEL", velmode, CLR_INDIGO);
    hud_btn(B_SCL,  "SCL", scalelock, CLR_INDIGO);
    hud_btn(B_ROOT, NOTE_NAME[scale_root], false, 0);
    hud_btn(B_TYPE, SCALES[scale_idx].name, false, 0);
    hud_btn(B_SAVE, "SAVE", false, 0);
    hud_btn(B_LOAD, "LOAD", false, 0);
    const char *sn = snap >= 1.0f ? "1/4" : snap >= 0.5f ? "1/8" : snap >= 0.25f ? "1/16" : "1/32";
    hud_btn(B_SNAP, sn, false, 0);
    char s[8]; snprintf(s, sizeof s, "%d", tempo);
    print(s, BTN[B_SNAP].x + BTN[B_SNAP].w + 3, 2, CLR_MEDIUM_GREY);
    hud_btn(B_UNDO, "<", u_len > 0, CLR_INDIGO);   // lit when there's something to undo/redo
    hud_btn(B_REDO, ">", r_len > 0, CLR_INDIGO);
    hud_btn(B_TOOL, tool == TOOL_DRAW ? "DRAW" : "SEL", true, CLR_INDIGO);   // always lit = current mode
    font(FONT_NORMAL);
}

void draw(void){
    cls(CLR_BLACK);
    draw_roll();
    draw_piano_col();
    draw_ruler();
    draw_hud();
    if (velmode) draw_vel_lane();
    else         keybed_draw();
}
