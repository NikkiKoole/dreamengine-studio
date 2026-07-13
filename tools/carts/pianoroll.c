/* de:meta
{
  "slug": "pianoroll",
  "title": "piano roll",
  "kind": ["instrument", "toy"],
  "teaches": ["song-arrangement", "note-editing"],
  "created": "2026-07-13",
  "lineage": "sampler",
  "description": {
    "summary": "A scrubbable piano roll: play notes in on the keybed, or draw & reshape them with the mouse.",
    "detail": "Time runs left–right, pitch runs bottom–up. Notes live in BEATS, so tempo and scrubbing are free. Playback is stateless: every frame each note asks 'should I be sounding at the playhead right now?' — so you can drag the playhead anywhere on the ruler and instantly hear the composition at that point, no need to replay from the top. Three ways in, one note list: RECORD live off the keybed, CLICK an empty cell to place, or DRAG a note's body (move) / edges (retime) to reshape it.",
    "controls": "keybed / QWERTY: play notes · SPACE: play/stop · R: arm record · drag the top RULER: scrub (you hear it) · left-click empty: place note (drag out to size) · drag note body: move · drag note edge: resize · right-click: delete · wheel / ↑↓: scroll pitch · ←→: scroll time · [ ]: tempo · X: clear all"
  }
}
de:meta */

// piano roll — the composition surface the audio shelf was missing.
//
// sampler chops a SOUND and arranges slices on a step grid; this arranges
// PITCHES over time. It reuses the same live-capture trick sampler uses
// (keybed on_note/on_off callbacks log what you play), but the payload is a
// note {pitch, start, len} instead of a slice trigger.
//
// The one idea that makes the whole thing feel good: playback is STATELESS.
// There's no event queue counting down. Every frame, each note just asks
// "is the playhead inside me right now, and is anything audible?" and turns
// its voice on/off accordingly. Move the playhead anywhere — scrub the ruler,
// loop, jump — and the correct notes start and stop with zero bookkeeping.
// That's what makes "drag to hear 10s in" free.

#include <stdio.h>     // snprintf — HUD/label text
#include "studio.h"
#include "keybed.h"

#define SYN       5            // instrument slot the keybed + playback both use
#define MAXNOTES  512
#define SONG_LEN  16.0f        // song length in beats (4 bars of 4/4)
#define SNAP      0.25f        // grid = 1/16 note

// --- layout -----------------------------------------------------------------
#define HUD_H     10           // transport buttons row
#define RULER_Y   HUD_H
#define RULER_H   12
#define ROLL_Y    (RULER_Y + RULER_H)
#define KB_H      50           // keybed manual at the bottom
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
    int     h;                 // live voice handle during playback/scrub, -1 = silent
} Note;

static Note  note_[MAXNOTES];
static int   n_notes;

static bool  playing;
static bool  armed;                    // record-arm
static float playhead;                 // beats
static int   tempo = 120;

static int   view_lo = 48;             // lowest visible pitch (C3)
static float view_b0 = 0.0f;           // leftmost visible beat

static int   rec_map[128];             // midi -> index of the note being recorded, -1 none

enum { DRAG_NONE, DRAG_MOVE, DRAG_END, DRAG_START, DRAG_SCRUB };
static int   drag;
static int   drag_note;
static float drag_off;                 // beat offset (cursor - note.start) while moving

// --- helpers ----------------------------------------------------------------
static int   vis_rows(void) { return ROLL_H / ROWH; }
static float vis_beats(void){ return ROLL_W / PXB; }
static bool  is_black(int midi){ int pc = ((midi % 12) + 12) % 12;
    return pc==1||pc==3||pc==6||pc==8||pc==10; }

static float snapf(float b){ float s = roundf(b / SNAP) * SNAP; return s < 0 ? 0 : s; }

// pixel <-> data mappings
static int   x_of_beat(float b){ return ROLL_X + (int)((b - view_b0) * PXB); }
static float beat_of_x(int px){ return view_b0 + (px - ROLL_X) / PXB; }
// the ruler shows the WHOLE song across the full width (an overview + scrub bar)
static int   x_of_beat_ruler(float b){ return (int)(b / SONG_LEN * SCREEN_W); }
static float beat_of_x_ruler(int px){ float b = (float)px / SCREEN_W * SONG_LEN;
    return b < 0 ? 0 : b > SONG_LEN ? SONG_LEN : b; }
static int   y_of_pitch(int m){ return ROLL_Y + ROLL_H - (m - view_lo + 1) * ROWH; }
// exact inverse of y_of_pitch (the -1 keeps a click and its hit-test on the SAME row)
static int   pitch_of_y(int py){ return view_lo + (ROLL_Y + ROLL_H - 1 - py) / ROWH; }

static int vel_vol(int v){ return v < 1 ? 1 : v > 7 ? 7 : v; }

// silence every voice we own (playback handles + any half-recorded notes)
static void all_notes_off(void){
    for (int i = 0; i < n_notes; i++) if (note_[i].h >= 0){ note_off(note_[i].h); note_[i].h = -1; }
}

// --- live recording: keybed calls these on every fresh press / release -------
static void rec_on(int midi, int vel){
    if (!(playing && armed) || n_notes >= MAXNOTES) return;
    if (midi < 0 || midi > 127) return;
    Note *nt = &note_[n_notes];
    nt->pitch = (int8_t)midi;
    nt->start = playhead;
    nt->len   = SNAP;
    nt->vel   = (uint8_t)vel_vol(vel > 7 ? vel * 7 / 127 : vel);
    nt->rec   = 1;
    nt->h     = -1;                    // keybed makes the sound while recording; we don't
    rec_map[midi] = n_notes++;
}
static void rec_off(int midi){
    if (midi < 0 || midi > 127) return;
    int i = rec_map[midi];
    if (i >= 0){ note_[i].rec = 0; rec_map[midi] = -1; }
}

// --- stateless playback: does each note want to sound at the playhead? --------
static void audition(void){
    bool audible = playing || drag == DRAG_SCRUB;   // scrubbing is audible too
    for (int i = 0; i < n_notes; i++){
        Note *nt = &note_[i];
        bool on = audible && !nt->rec &&
                  playhead >= nt->start && playhead < nt->start + nt->len;
        if (on && nt->h < 0)  nt->h = note_on(nt->pitch, SYN, vel_vol(nt->vel));
        else if (!on && nt->h >= 0){ note_off(nt->h); nt->h = -1; }
    }
}

// --- editing ----------------------------------------------------------------
// topmost note whose body is under (px,py); returns index or -1, and the zone.
static int hit_note(int px, int py, int *zone){
    for (int i = n_notes - 1; i >= 0; i--){       // last drawn = on top
        Note *nt = &note_[i];
        int y = y_of_pitch(nt->pitch);
        if (py < y || py >= y + ROWH) continue;
        int x0 = x_of_beat(nt->start), x1 = x_of_beat(nt->start + nt->len);
        if (px < x0 - 1 || px > x1 + 1) continue;
        if (zone){
            // edge grab-zones shrink with the note so a tiny note keeps a movable body
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
    note_[i] = note_[--n_notes];                  // swap-remove
    // any recording pointer to the moved slot must be re-aimed / dropped
    for (int m = 0; m < 128; m++){
        if (rec_map[m] == i) rec_map[m] = -1;
        else if (rec_map[m] == n_notes) rec_map[m] = i;
    }
}

static void clear_all(void){
    all_notes_off();
    n_notes = 0;
    for (int m = 0; m < 128; m++) rec_map[m] = -1;
}

// --- transport buttons ------------------------------------------------------
// returns true if a fresh left-click landed inside (x,y,w,h)
static bool click(int x, int y, int w, int h){
    int mx = mouse_x(), my = mouse_y();
    return mouse_pressed(MOUSE_LEFT) && mx >= x && mx < x + w && my >= y && my < y + h;
}

// a gentle two-bar phrase so the cart opens with something to hear & see (X clears it)
static void seed_demo(void){
    static const struct { int p; float s, l; } demo[] = {
        {60,0.0f,0.5f},{64,0.5f,0.5f},{67,1.0f,0.5f},{64,1.5f,0.5f},
        {69,2.0f,1.0f},{67,3.0f,1.0f},{65,4.0f,0.5f},{64,4.5f,0.5f},
        {62,5.0f,0.5f},{60,5.5f,1.5f},
    };
    for (int i = 0; i < (int)(sizeof demo / sizeof demo[0]); i++){
        note_[n_notes].pitch = (int8_t)demo[i].p;
        note_[n_notes].start = demo[i].s;
        note_[n_notes].len   = demo[i].l;
        note_[n_notes].vel   = 5; note_[n_notes].rec = 0; note_[n_notes].h = -1;
        n_notes++;
    }
}

void init(void){
    bpm(tempo);
    instrument(SYN, INSTR_SAW, 6, 90, 5, 160);    // a mellow synth lead
    keybed_config(SYN, 4, 14);                     // slot, base octave C4, 14 white keys (2 oct)
    keybed_layout(0, SCREEN_H - KB_H, SCREEN_W, KB_H);
    keybed_on_note(rec_on);
    keybed_on_off(rec_off);
    for (int m = 0; m < 128; m++) rec_map[m] = -1;
    drag = DRAG_NONE;
    seed_demo();
}

void update(void){
    keybed_update();                               // drains touch + mouse-on-keys + QWERTY + MIDI

    // --- transport keys ---
    if (keyp(' ')){ playing = !playing; if (!playing) all_notes_off(); }
    if (keyp('R')) armed = !armed;
    if (keyp('X')) clear_all();
    if (keyp('[')){ tempo -= 5; if (tempo < 40) tempo = 40; bpm(tempo); }
    if (keyp(']')){ tempo += 5; if (tempo > 240) tempo = 240; bpm(tempo); }

    // --- transport buttons (clickable, touch-friendly) ---
    if (click(2, 1, 22, HUD_H - 2)){ playing = !playing; if (!playing) all_notes_off(); }
    if (click(26, 1, 26, HUD_H - 2)) armed = !armed;

    // --- scroll ---
    float w = mouse_wheel();
    if (w != 0){ view_lo += (w > 0 ? 1 : -1); }
    if (keyp(KEY_UP))   view_lo++;
    if (keyp(KEY_DOWN)) view_lo--;
    if (keyp(KEY_LEFT))  view_b0 -= 1;
    if (keyp(KEY_RIGHT)) view_b0 += 1;
    if (view_lo < 12) view_lo = 12;
    if (view_lo > 108 - vis_rows()) view_lo = 108 - vis_rows();
    if (view_b0 < 0) view_b0 = 0;
    float maxb0 = SONG_LEN - vis_beats(); if (maxb0 < 0) maxb0 = 0;
    if (view_b0 > maxb0) view_b0 = maxb0;

    // --- advance the playhead ---
    if (playing){
        playhead += dt() * tempo / 60.0f;
        if (playhead >= SONG_LEN) playhead -= SONG_LEN;     // loop
        // grow any note still being recorded
        for (int m = 0; m < 128; m++) if (rec_map[m] >= 0){
            Note *nt = &note_[rec_map[m]];
            float len = playhead - nt->start;
            if (len > nt->len) nt->len = len;               // ignores the loop wrap; fine
        }
        // auto-scroll to keep the playhead in view
        if (playhead < view_b0 || playhead > view_b0 + vis_beats() - 0.5f){
            view_b0 = playhead - 1; if (view_b0 < 0) view_b0 = 0;
            if (view_b0 > maxb0) view_b0 = maxb0;
        }
    }

    int mx = mouse_x(), my = mouse_y();

    // --- begin a drag ---
    if (mouse_pressed(MOUSE_LEFT)){
        if (my >= RULER_Y && my < RULER_Y + RULER_H){       // scrub the ruler
            drag = DRAG_SCRUB;
        } else if (mx >= ROLL_X && my >= ROLL_Y && my < ROLL_Y + ROLL_H){
            int zone; int i = hit_note(mx, my, &zone);
            if (i >= 0){
                drag = zone; drag_note = i;
                drag_off = beat_of_x(mx) - note_[i].start;
            } else if (n_notes < MAXNOTES){                 // place a fresh note
                Note *nt = &note_[n_notes];
                nt->pitch = (int8_t)pitch_of_y(my);
                nt->start = snapf(beat_of_x(mx));
                nt->len   = SNAP;
                nt->vel   = 5;
                nt->rec   = 0; nt->h = -1;
                drag = DRAG_END; drag_note = n_notes++;      // drag right away to size it
            }
        }
    }
    // right-click deletes
    if (mouse_pressed(MOUSE_RIGHT)){
        int i = hit_note(mx, my, 0);
        if (i >= 0) del_note(i);
    }

    // --- continue a drag ---
    if (mouse_down(MOUSE_LEFT) && drag != DRAG_NONE){
        if (drag == DRAG_SCRUB){
            playhead = beat_of_x_ruler(mx);
        } else if (drag_note >= 0 && drag_note < n_notes){
            Note *nt = &note_[drag_note];
            float b = beat_of_x(mx);
            if (drag == DRAG_MOVE){
                nt->start = snapf(b - drag_off);
                if (nt->start + nt->len > SONG_LEN) nt->start = SONG_LEN - nt->len;
                if (nt->start < 0) nt->start = 0;
                nt->pitch = (int8_t)pitch_of_y(my);
            } else if (drag == DRAG_END){
                float e = snapf(b);
                nt->len = e - nt->start; if (nt->len < SNAP) nt->len = SNAP;
            } else if (drag == DRAG_START){
                float s = snapf(b), end = nt->start + nt->len;
                if (s > end - SNAP) s = end - SNAP; if (s < 0) s = 0;
                nt->start = s; nt->len = end - s;
            }
        }
    } else drag = DRAG_NONE;

    audition();

#ifdef DE_TRACE
    watch("playing", "%d", playing);
    watch("armed",   "%d", armed);
    watch("phead",   "%.2f", playhead);
    watch("nnotes",  "%d", n_notes);
    { int snd = 0; for (int i = 0; i < n_notes; i++) if (note_[i].h >= 0) snd++;
      watch("snd", "%d", snd); }         // voices ringing right now (proves scrub/playback fire)
#endif
}

// --- draw -------------------------------------------------------------------
static void draw_roll(void){
    int rows = vis_rows();
    // pitch-lane striping: naturals a touch lighter than the black-key lanes
    for (int r = 0; r < rows; r++){
        int m = view_lo + r, y = y_of_pitch(m);
        rectfill(ROLL_X, y, ROLL_W, ROWH, is_black(m) ? CLR_BLACK : CLR_DARKER_GREY);
        if (m % 12 == 0) line(ROLL_X, y + ROWH - 1, SCREEN_W, y + ROWH - 1, CLR_DARK_GREY); // C line
    }
    // beat / bar gridlines
    for (float b = ceilf(view_b0); b < view_b0 + vis_beats(); b += 1){
        int x = x_of_beat(b);
        line(x, ROLL_Y, x, ROLL_Y + ROLL_H, ((int)b % 4 == 0) ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
    }
    // notes
    for (int i = 0; i < n_notes; i++){
        Note *nt = &note_[i];
        int y = y_of_pitch(nt->pitch);
        if (y + ROWH <= ROLL_Y || y >= ROLL_Y + ROLL_H) continue;   // off-screen pitch
        int x0 = x_of_beat(nt->start), x1 = x_of_beat(nt->start + nt->len);
        if (x1 < ROLL_X || x0 > SCREEN_W) continue;
        if (x0 < ROLL_X) x0 = ROLL_X;
        int w = x1 - x0; if (w < 2) w = 2;
        int col = nt->rec ? CLR_RED : (nt->h >= 0 ? CLR_YELLOW : CLR_GREEN);
        rectfill(x0, y, w, ROWH, col);
        rect(x0, y, w, ROWH, CLR_DARK_GREEN);
    }
    // playhead (only when it's inside the scrolled window)
    if (playhead >= view_b0 && playhead <= view_b0 + vis_beats()){
        int px = x_of_beat(playhead);
        line(px, ROLL_Y, px, ROLL_Y + ROLL_H, CLR_WHITE);
    }
}

static void draw_piano_col(void){
    int rows = vis_rows();
    for (int r = 0; r < rows; r++){
        int m = view_lo + r, y = y_of_pitch(m);
        bool blk = is_black(m), held = keybed_held(m);
        int col = held ? CLR_YELLOW : (blk ? CLR_DARK_GREY : CLR_LIGHT_GREY);
        rectfill(0, y, PIANO_W - 1, ROWH, col);
        if (m % 12 == 0){                       // label each C with its octave
            font(FONT_TINY);
            char s[6]; snprintf(s, sizeof s, "C%d", m / 12 - 1);
            print(s, 1, y, CLR_BLACK);
            font(FONT_NORMAL);
        }
    }
}

static void draw_ruler(void){
    rectfill(0, RULER_Y, SCREEN_W, RULER_H, CLR_DARK_GREY);
    for (int b = 0; b <= (int)SONG_LEN; b++){          // bar ticks
        if (b % 4) continue;
        int x = x_of_beat_ruler(b);
        line(x, RULER_Y, x, RULER_Y + RULER_H, CLR_DARKER_GREY);
        font(FONT_TINY); char s[4]; snprintf(s, sizeof s, "%d", b / 4 + 1);
        print(s, x + 2, RULER_Y + 1, CLR_MEDIUM_GREY); font(FONT_NORMAL);
    }
    // the roll's visible window, as a bracket on the overview
    int wx0 = x_of_beat_ruler(view_b0), wx1 = x_of_beat_ruler(view_b0 + vis_beats());
    rect(wx0, RULER_Y, wx1 - wx0, RULER_H - 1, CLR_MEDIUM_GREY);
    // playhead marker
    int hx = x_of_beat_ruler(playhead);
    line(hx, RULER_Y, hx, RULER_Y + RULER_H, CLR_WHITE);
    rectfill(hx - 2, RULER_Y, 5, 3, CLR_WHITE);
}

static void draw_hud(void){
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_SMALL);
    // play / stop
    rectfill(2, 1, 22, HUD_H - 2, playing ? CLR_GREEN : CLR_DARK_GREY);
    print(playing ? "STOP" : "PLAY", 4, 2, playing ? CLR_BLACK : CLR_WHITE);
    // record arm
    rectfill(26, 1, 26, HUD_H - 2, armed ? CLR_RED : CLR_DARK_GREY);
    print("REC", 28, 2, CLR_WHITE);
    // tempo + hint
    char s[16]; snprintf(s, sizeof s, "%d BPM", tempo);
    print(s, 56, 2, CLR_MEDIUM_GREY);
    print("SPACE play  R rec  drag ruler=scrub", 96, 2, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw(void){
    cls(CLR_BLACK);
    draw_roll();
    draw_piano_col();
    draw_ruler();
    draw_hud();
    keybed_draw();
}
