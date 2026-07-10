/* de:meta
{
  "slug": "portapop",
  "title": "portapop",
  "status": "wip",
  "created": "2026-07-10",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "gesture-loop"
  ],
  "lineage": "Phase-1 skeleton of the portapop cassette 4-track (design/portapop.md). The record core descends from loopstation.c — discrete note events PLUS a continuous pitch-stream (EV_ON/EV_CC/EV_OFF) so slides and bends replay verbatim, exactly like loopstation's theremin track. The bass is the full pizzicato upright fingerboard from upright.c (press to grab + slide + bend, sweep to pick, fretless continuous glide). One instrument, one track, TAKE mode only — no console, shelf, or ping-pong yet.",
  "homage": "Tascam Portastudio",
  "description": {
    "summary": "The bedroom cassette 4-track, phase 1: play the upright bass onto tape, one honest take — slides, bends and all. Hit record, count in, play a pass, stop, then play it back and it loops exactly as you played it (no quantize). The slice that proves the machine records itself.",
    "detail": "A skeleton of the portapop multitracker (design/portapop.md). ONE instrument — the full pizzicato upright bass (INSTR_BOWED): press ON a string to grab a note and slide LEFT/RIGHT to glide the pitch continuously, PULL up/down to bend, or start in the open gap and sweep THROUGH a string to pick it. ONE track, TAKE mode only. The transport is the star: RECORD arms a one-bar count-in then captures every note AND every slide as control events; STOP snaps the take to whole bars (that becomes the song); PLAY loops it back verbatim, glides included. Un-quantized on purpose — the rushes and drags are the point. The console, the shelf of instruments, ping-pong bounce and the tape glue all come later.",
    "controls": "Fingerboard: press ON a string (E A D G, low to high) and slide L/R to glide pitch, pull up/down to bend, lift to damp; press in the open gap next to a string and sweep through it to pick. Keyboard: GarageBand map A S D F G H J K (+ W E T Y U black keys), Z/X octave. Transport: R record (count-in), SPACE play/stop, BACKSPACE clear. On-screen REC/PLY/CLR buttons mirror the keys."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// ── PORTAPOP — phase 1 (the proving slice) ───────────────────────────────────
// The bedroom cassette 4-track, cut to the one thing that must work first: a patient
// tape deck you overdub yourself onto. Play the upright bass — with real slides and
// bends — hit record, count in, play a pass, stop, then hit play and hear it loop back
// EXACTLY as you played it. That single loop proves the two load-bearing risks:
//   1. the RECORDABLE SEAM — notes go through pp_on()/pp_slide()/pp_off(), which sound
//      the voice AND (while armed) record a note-on + a pitch STREAM + a note-off, so a
//      glide reconstructs, not just the attack;
//   2. the TAKE MODEL — "play a pass, stop, it loops as-played" (un-quantized).
// Everything else in design/portapop.md (the console, the shelf of 8, ping-pong bounce,
// the tape glue, the mode-flip) is additive onto this core.

#define BASS  5                            // INSTR_BOWED, pizzicato — the one voice
#define CLICK 6                            // the count-in / metronome tick

// ── the take: discrete notes + a continuous pitch stream (loopstation's model) ──
#define MAXEV 2048
enum { EV_ON, EV_CC, EV_OFF };             // note-on (attack) · pitch change (slide/bend) · note-off (damp)
typedef struct { float pos, pitch; int kind, vol; } Ev;   // pos = beats from the top of the take
static Ev    ev[MAXEV];
static int   nev;
static float song_len;                     // take length in beats; 0 = empty
static float rec_last_pitch;               // delta filter for the CC stream

// ── transport ──
enum { TR_STOP, TR_COUNTIN, TR_REC, TR_PLAY };
static int   mode = TR_STOP;
static float tpos, prev_tpos;              // transport position in beats (negative during count-in)
static float last_songb;                   // engine beat clock last frame (to derive a delta)
static int   clickflash;
#define COUNTIN_BEATS 4
#define REC_MAX_BEATS  64.0f               // safety: auto-stop a runaway recording (16 bars)
#define BEATS_PER_BAR  4
#define TEMPO 100

// ── the bass fingerboard (from upright.c — pizz, fretless, mono last-note-wins) ──
#define TOPBAR   44
#define NECK_X0  30
#define NECK_X1  292
#define SPAN     12                        // semitones nut→bridge (one octave)
#define NLANE    4
#define LANE_H   ((SCREEN_H - TOPBAR) / NLANE)
static const int   SBASE[NLANE] = { 43, 38, 33, 28 };   // G2 D2 A1 E1 (high string on top)
static const char *SLAB [NLANE] = { "G", "D", "A", "E" };
#define GRAB_PX  13                        // press within this of a string = grab; else a pick
#define BEND_K   0.05f                     // semitones of bend per pixel pulled
#define BEND_MAX 2.0f

// the one live mono voice
#define NONE  (-999)
#define KBD   (-2)
#define MOUSE (-3)
enum { GRAB, PICK };
static int   b_handle = -1, b_owner = NONE, b_lane = 3, b_gesture, b_kbsemi = -1;
static float b_midi = 28, b_slide = 28;
static int   b_press_x, b_press_y, b_prevy, b_fx, b_fy;
static int   b_sliding;
static float b_wob;

// the replay voice (drives note_pitch just like loopstation's theremin ghost)
static int   vrep = -1;
static float g_pitch;                      // currently-sounding replay pitch (for the ghost dot)
static int   g_on;

// GarageBand musical-typing map — 'A' = the low root
static const char gb_wkey[11]  = "ASDFGHJKL;'";
static const int  gb_wsemi[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
static const char gb_bkey[7]   = { 'W', 'E', 'T', 'Y', 'U', 'O', 'P' };
static const int  gb_bsemi[7]  = { 1, 3, 6, 8, 10, 13, 15 };
#define KB_ROOT 28                         // 'A' = E1 at octave 0
static int octv = 1;

static int   lane_at(int y) { int l = (y - TOPBAR) / LANE_H; return l < 0 ? 0 : (l > 3 ? 3 : l); }
static int   cy_of(int lane) { return TOPBAR + lane * LANE_H + LANE_H / 2; }
static float pos_at(int x) { return clamp((float)(x - NECK_X0) / (NECK_X1 - NECK_X0), 0, 1) * SPAN; }
static int   midi_lane(float m) { int l = 3; for (int i = 0; i < NLANE; i++) if (SBASE[i] <= m) { l = i; break; } return l; }
static int   midi_x(float m, int lane) { return NECK_X0 + (int)((NECK_X1 - NECK_X0) * (m - SBASE[lane]) / SPAN); }

// ── setup ──
void init(void) {
    bpm(TEMPO);
    // the pizzicato upright (from upright.c's setup_pizz, trimmed to the one articulation)
    instrument(BASS, INSTR_BOWED, 4, 0, 7, 460);
    instrument_mode(BASS, MODE_BOW_PIZZ, 1.0f);        // pluck the string, don't bow it
    instrument_filter(BASS, FILTER_LOW, 1400, 3);
    instrument_harmonics(BASS, 0.30f);                 // dark, woody
    instrument_timbre(BASS, 0.30f);
    instrument_morph(BASS, 0.45f);
    reverb(0.28f, 0.5f);
    instrument_reverb(BASS, 0.14f);
    // the count-in / metronome click
    instrument(CLICK, INSTR_TRI, 0, 30, 0, 20);
}

// ── the recordable seam ──────────────────────────────────────────────────────
// Every note + every slide goes through here. Each sounds the live voice AND (while the
// transport is armed-and-rolling) records an event, un-quantized. Wire the whole surface
// to pp_* and recording — glides included — is free.
static void rec(int kind, float pitch, int vol) {
    if (mode == TR_REC && tpos >= 0 && nev < MAXEV)
        ev[nev++] = (Ev){ tpos, pitch, kind, vol };
}
static void pp_on(int owner, int lane, float midi, int vol) {
    if (b_handle >= 0) note_off(b_handle);             // mono: replace what's sounding
    b_owner = owner; b_lane = lane; b_midi = b_slide = midi; b_wob = 0;
    b_handle = note_on((int)(midi + 0.5f), BASS, vol);
    note_pitch(b_handle, midi);
    note_glide(b_handle, 70);                           // slides/bends slur smoothly, both ways
    rec(EV_ON, midi, vol);
    rec_last_pitch = midi;
    b_fx = midi_x(midi, lane); b_fy = cy_of(lane);
}
static void pp_slide(float midi) {                      // continuous pitch move (glide or bend)
    if (b_handle < 0) return;
    b_midi = midi; note_pitch(b_handle, midi);
    if (fabsf(midi - rec_last_pitch) > 0.05f) { rec(EV_CC, midi, 0); rec_last_pitch = midi; }
}
static void pp_off(void) {                              // damp
    if (b_handle < 0) return;
    note_off(b_handle); b_handle = -1;
    rec(EV_OFF, 0, 0);
}

// ── replay ──
static void fire_ev(Ev *e) {
#ifdef DE_TRACE
    watch("fire", "kind=%d pos=%.2f midi=%.1f", e->kind, e->pos, e->pitch);
#endif
    if (e->kind == EV_ON) {
        if (vrep >= 0) note_off(vrep);
        vrep = note_on((int)(e->pitch + 0.5f), BASS, e->vol);
        note_pitch(vrep, e->pitch); note_glide(vrep, 70);
        g_pitch = e->pitch; g_on = 1;
    } else if (e->kind == EV_CC) {
        if (vrep >= 0) { note_pitch(vrep, e->pitch); g_pitch = e->pitch; }
    } else {  // EV_OFF
        if (vrep >= 0) { note_off(vrep); vrep = -1; }
        g_on = 0;
    }
}
// fire every event the playhead crossed since last frame (wrap-aware) — loopstation's logic
static void fire_replay(void) {
    int wrap = tpos < prev_tpos;
    for (int i = 0; i < nev; i++) {
        float p = ev[i].pos;
        int hitnow = wrap ? (p > prev_tpos || p <= tpos)
                          : (p > prev_tpos && p <= tpos);
        if (hitnow) fire_ev(&ev[i]);
    }
}

// ── transport control ──
static void start_record(void) {
    nev = 0; song_len = 0;
    mode = TR_COUNTIN;
    tpos = prev_tpos = -(float)COUNTIN_BEATS;
}
static void stop_record(void) {
    int bars = (int)roundf(tpos / BEATS_PER_BAR); if (bars < 1) bars = 1;
    song_len = (float)(bars * BEATS_PER_BAR);
    mode = TR_STOP; tpos = prev_tpos = 0;
    if (b_handle >= 0) { note_off(b_handle); b_handle = -1; }
}
static void start_play(void) {
    if (song_len <= 0) return;
    mode = TR_PLAY; tpos = 0; prev_tpos = -0.001f;      // so an event at pos 0 fires
}
static void stop_all(void) {
    mode = TR_STOP;
    if (vrep >= 0) { note_off(vrep); vrep = -1; } g_on = 0;
}

// ── update ──
void update(void) {
    float songb = beat() + beat_pos();
    float db = songb - last_songb;
    if (db < 0 || db > 1.0f) db = 0;                    // guard first frame / wrap
    last_songb = songb;
    prev_tpos = tpos;

    // ── transport keys + on-screen buttons ──
    if (keyp('R') || tapp(6, 26, 34, 14)) {
        if (mode == TR_REC || mode == TR_COUNTIN) stop_record();
        else { if (vrep >= 0) { note_off(vrep); vrep = -1; } g_on = 0; start_record(); }
    }
    if (keyp(KEY_SPACE) || tapp(44, 26, 34, 14)) {
        if (mode == TR_PLAY || mode == TR_REC || mode == TR_COUNTIN) stop_all();
        else start_play();
    }
    if (keyp(KEY_BACKSPACE) || tapp(82, 26, 34, 14)) {
        nev = 0; song_len = 0; mode = TR_STOP; tpos = 0;
        if (vrep >= 0) { note_off(vrep); vrep = -1; } g_on = 0;
    }

    // ── advance transport + clicks + replay ──
    if (mode != TR_STOP) tpos += db;
    if (mode == TR_COUNTIN || mode == TR_REC) {
        if ((int)floorf(tpos) > (int)floorf(prev_tpos)) {
            int acc = ((((int)floorf(tpos)) % BEATS_PER_BAR) + BEATS_PER_BAR) % BEATS_PER_BAR == 0;
            hit(acc ? 93 : 81, CLICK, acc ? 4 : 2, 25);
            clickflash = 5;
        }
    }
    if (mode == TR_COUNTIN && tpos >= 0) mode = TR_REC;
    if (mode == TR_REC && tpos >= REC_MAX_BEATS) stop_record();
    if (mode == TR_PLAY) {
        if (tpos >= song_len) { tpos -= song_len; prev_tpos = -0.001f; }
        fire_replay();
    }

    // ── fingerboard: press ON a string to grab+slide+bend, sweep the gap to pick ──
    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT) && my >= TOPBAR && mx >= NECK_X0 - 6 && mx <= NECK_X1 + 6) {
        int near = 0, nd = 9999;
        for (int l = 0; l < NLANE; l++) { int d = abs(my - cy_of(l)); if (d < nd) { nd = d; near = l; } }
        if (nd <= GRAB_PX) {                            // ON a string → grab (fretless: land exactly here)
            pp_on(MOUSE, near, SBASE[near] + pos_at(mx), 7);
            b_gesture = GRAB; b_fx = mx; b_fy = my;
        } else {                                        // open gap → a pick (sweep to pluck)
            b_owner = MOUSE; b_gesture = PICK;
        }
        b_press_x = mx; b_press_y = my; b_prevy = my; b_sliding = 0;
    }
    else if (mouse_down(MOUSE_LEFT) && b_owner == MOUSE && b_gesture == GRAB && b_handle >= 0) {
        if (!b_sliding && (abs(mx - b_press_x) > 4 || abs(my - b_press_y) > 4)) b_sliding = 1;
        if (b_sliding) {
            int dpx = b_press_y - my;                   // vertical pull (+up / -down)
            if (abs(dpx) <= 6) b_slide = SBASE[b_lane] + clamp(pos_at(mx), 0, SPAN);   // walk the frets
            float vbend = clamp((float)dpx * BEND_K, -BEND_MAX, BEND_MAX);              // signed bend
            pp_slide(b_slide + vbend);
            int maxpx = (int)(BEND_MAX / BEND_K);
            b_fx = midi_x(b_slide, b_lane);
            b_fy = cy_of(b_lane) - (int)clamp((float)dpx, -(float)maxpx, (float)maxpx);
        }
    }
    else if (mouse_down(MOUSE_LEFT) && b_owner == MOUSE && b_gesture == PICK) {
        for (int l = 0; l < NLANE; l++) {               // pluck each string the finger sweeps through
            int cy = cy_of(l);
            if ((b_prevy < cy && my >= cy) || (b_prevy > cy && my <= cy))
                pp_on(MOUSE, l, SBASE[l] + clamp(roundf(pos_at(mx)), 0, SPAN), 7);
        }
        b_fx = mx; b_fy = my; b_prevy = my;
    }
    if (!mouse_down(MOUSE_LEFT) && b_owner == MOUSE) {
        if (b_gesture == GRAB) pp_off();                // grabbed note: lift = damp
        else b_owner = NONE;                            // picked notes ring out on their own
    }

    // ── computer keyboard (GarageBand map): discrete grab + damp ──
    for (int i = 0; i < 11; i++) {
        if (keyp(gb_wkey[i])) {
            int midi = KB_ROOT + octv * 12 + gb_wsemi[i];
            pp_on(KBD, midi_lane(midi), (float)midi, 7); b_owner = KBD; b_kbsemi = gb_wsemi[i];
        }
        if (keyr(gb_wkey[i]) && b_owner == KBD && gb_wsemi[i] == b_kbsemi) pp_off();
    }
    for (int i = 0; i < 7; i++) {
        if (keyp(gb_bkey[i])) {
            int midi = KB_ROOT + octv * 12 + gb_bsemi[i];
            pp_on(KBD, midi_lane(midi), (float)midi, 7); b_owner = KBD; b_kbsemi = gb_bsemi[i];
        }
        if (keyr(gb_bkey[i]) && b_owner == KBD && gb_bsemi[i] == b_kbsemi) pp_off();
    }
    if (keyp('Z') && octv > 0) octv--;
    if (keyp('X') && octv < 3) octv++;

    b_wob += 0.6f;
    if (clickflash > 0) clickflash--;

#ifdef DE_TRACE
    watch("mode", "%d", mode);
    watch("tpos", "%.2f", tpos);
    watch("nev", "%d", nev);
    watch("song", "%.1f", song_len);
#endif
}

// ── draw ──
static void draw_topbar(void) {
    rectfill(0, 0, SCREEN_W, TOPBAR, CLR_DARKER_BLUE);
    line(0, TOPBAR - 1, SCREEN_W, TOPBAR - 1, CLR_BLACK);
    print("PORTAPOP", 6, 5, CLR_LIGHT_PEACH);
    font(FONT_SMALL); print("4-track \x7f phase 1", 6, 15, CLR_INDIGO); font(FONT_NORMAL);

    int recon = (mode == TR_REC || mode == TR_COUNTIN);
    rectfill(6,  26, 34, 14, recon ? CLR_RED   : CLR_DARK_BLUE); rect(6,  26, 34, 14, recon ? CLR_WHITE : CLR_DARK_GREY); print("REC", 12, 30, recon ? CLR_WHITE : CLR_RED);
    int plon = (mode == TR_PLAY);
    rectfill(44, 26, 34, 14, plon ? CLR_GREEN : CLR_DARK_BLUE); rect(44, 26, 34, 14, plon ? CLR_WHITE : CLR_DARK_GREY); print("PLY", 50, 30, plon ? CLR_BLACK : CLR_GREEN);
    rectfill(82, 26, 34, 14, CLR_DARK_BLUE); rect(82, 26, 34, 14, CLR_DARK_GREY); print("CLR", 88, 30, CLR_MEDIUM_GREY);

    float shown = tpos < 0 ? 0 : tpos;
    int bar = (int)(shown / BEATS_PER_BAR) + 1, bt = (int)(shown) % BEATS_PER_BAR + 1;
    char buf[24];
    snprintf(buf, sizeof buf, "%03d.%d", bar, bt); print(buf, 150, 6, CLR_LIME_GREEN);
    font(FONT_SMALL);
    if (song_len > 0) { snprintf(buf, sizeof buf, "take %d bars  %d ev", (int)(song_len / BEATS_PER_BAR), nev); print(buf, 150, 20, CLR_MEDIUM_GREY); }
    else print(nev ? "recording..." : "empty - hit REC", 150, 20, CLR_MEDIUM_GREY);
    print_right(str("%d BPM", TEMPO), SCREEN_W - 6, 6, CLR_INDIGO);
    font(FONT_NORMAL);

    if (mode == TR_COUNTIN) {
        int n = (int)ceilf(-tpos); if (n < 1) n = 1;
        print_scaled(str("%d", n), SCREEN_W / 2 - 6, 8, 3, clickflash ? CLR_YELLOW : CLR_ORANGE);
    }
    if (mode == TR_REC && blink(16)) circfill(SCREEN_W - 12, 34, 3, CLR_RED);
}

// one string, pulled toward the finger when live (the tent = the pitch bend), from upright.c
static void draw_string(int lane, int live, int fx, int fy, int col) {
    int cy = cy_of(lane), thick = lane;
    int peakx = live ? (fx < NECK_X0 + 1 ? NECK_X0 + 1 : (fx > NECK_X1 - 1 ? NECK_X1 - 1 : fx)) : NECK_X0;
    int px = NECK_X0, py = cy;
    for (int x = NECK_X0; x <= NECK_X1; x += 4) {
        int y = cy;
        if (live) {
            float t = x <= peakx ? (float)(x - NECK_X0) / (peakx - NECK_X0)
                                 : (float)(NECK_X1 - x) / (NECK_X1 - peakx);
            t = clamp(t, 0, 1);
            y = cy + (int)((fy - cy) * t + sinf(b_wob + x * 0.20f) * t * 1.8f);
        }
        for (int tk = 0; tk <= thick; tk++) line(px, py + tk, x, y + tk, col);
        px = x; py = y;
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, TOPBAR, SCREEN_W, SCREEN_H - TOPBAR, CLR_BROWN);
    rectfill(NECK_X0 - 6, TOPBAR, NECK_X1 - NECK_X0 + 12, SCREEN_H - TOPBAR, CLR_BROWNISH_BLACK);
    for (int p = 0; p < 4; p++) circfill(12, TOPBAR + 16 + p * 34, 3, CLR_LIGHT_PEACH);   // pegs
    rectfill(NECK_X1 + 2, TOPBAR + 4, 4, SCREEN_H - TOPBAR - 8, CLR_LIGHT_PEACH);          // bridge

    for (int s = 1; s < SPAN; s++) {                    // fret guides (fretless — just a guide)
        int x = NECK_X0 + (NECK_X1 - NECK_X0) * s / SPAN;
        line(x, TOPBAR + 2, x, SCREEN_H - 2, (s == 5 || s == 7 || s == 12) ? CLR_DARK_PEACH : CLR_DARK_BROWN);
    }

    for (int l = 0; l < NLANE; l++) {                   // the four strings
        int live = (b_handle >= 0 && b_lane == l);
        draw_string(l, live, b_fx, b_fy, live ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY);
        print(SLAB[l], 4, cy_of(l) - 3, CLR_LIGHT_PEACH);
    }

    // the live finger dot (yellow) and the replay ghost (green)
    if (b_handle >= 0) {
        int fx = b_fx < NECK_X0 ? NECK_X0 : (b_fx > NECK_X1 ? NECK_X1 : b_fx);
        circfill(fx, b_fy, 4, CLR_YELLOW); circ(fx, b_fy, 4, CLR_WHITE);
    }
    if (g_on) {
        int gl = midi_lane(g_pitch);
        int gx = midi_x(g_pitch, gl); gx = gx < NECK_X0 ? NECK_X0 : (gx > NECK_X1 ? NECK_X1 : gx);
        circ(gx, cy_of(gl), 3, CLR_GREEN);
    }

    draw_topbar();
    font(FONT_SMALL);
    print("R rec  SPACE play/stop  BKSP clear   press+slide a string, or sweep to pick", 6, SCREEN_H - 9, CLR_DARK_PEACH);
    font(FONT_NORMAL);
}
