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
  "lineage": "Phase-1 skeleton of the portapop cassette 4-track (design/portapop.md). Record core (Ev/rec_ev/fire_replay) descends from loopstation.c; the bass voice + fingerboard surface are a lean cut of upright.c. This slice proves the recordable seam + the take model with ONE instrument, ONE track, TAKE mode only — no console, no shelf, no ping-pong yet.",
  "homage": "Tascam Portastudio",
  "description": {
    "summary": "The bedroom cassette 4-track, phase 1: play the upright bass onto tape, one honest take. Hit record, count in, play a pass, stop — then play it back and it loops as you played it (no quantize). The smallest slice that proves the machine records itself.",
    "detail": "A skeleton of the portapop multitracker (design/portapop.md). ONE instrument (a pizzicato upright bass, INSTR_BOWED), ONE track, TAKE mode only. The transport is the star: RECORD arms a one-bar count-in then captures every note you play as a control event; STOP snaps the take to whole bars and that becomes the song; PLAY loops the take back verbatim. Recording is un-quantized on purpose — the little rushes and drags are the point. This is the proving slice: the console, the shelf of instruments, ping-pong bounce, and the tape glue all come later.",
    "controls": "Fingerboard: tap/click a string (E A D G, low to high) left-to-right for pitch. Keyboard: GarageBand map A S D F G H J K (+ W E T Y U black keys), Z/X octave. Transport: R record (with count-in), SPACE play/stop, BACKSPACE clear the take. On-screen REC/PLAY/CLR buttons do the same."
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// ── PORTAPOP — phase 1 (the proving slice) ───────────────────────────────────
// The bedroom cassette 4-track, cut down to the one thing that has to work first:
// a patient tape deck you overdub yourself onto. Play the upright bass, hit record,
// count in, play a pass, stop — then hit play and hear it loop back exactly as you
// played it. That single loop proves the two load-bearing risks of the whole cart:
//   1. the RECORDABLE SEAM — every note goes through pp_note(), which sounds the
//      voice AND (while armed) records it as an event, so replay reconstructs the take;
//   2. the TAKE MODEL — "play a pass, stop, it loops as-played" (un-quantized), the
//      first take setting the song length.
// Everything else in design/portapop.md (the console, the shelf of 8 instruments,
// ping-pong bounce, the tape glue, the mode-flip) is additive onto this core.
//
// Deliberately OUT of phase 1: no console/mixer, no other instruments, no second
// track, no bounce, no tape()/varispeed glue, no cassette chrome, no save.

#include <stdio.h>

#define BASS   5                 // INSTR_BOWED, pizzicato — the one voice
#define CLICK  6                 // the count-in / metronome tick

// ── the take (one track, straight from loopstation's model) ──
#define MAXEV 512
typedef struct { float pos; int midi, vol, dur; } Ev;   // pos in beats from the top of the take
static Ev    ev[MAXEV];
static int   nev;
static float song_len;           // take length in beats; 0 = empty (no take yet)

// ── transport ──
enum { TR_STOP, TR_COUNTIN, TR_REC, TR_PLAY };
static int   mode = TR_STOP;
static float tpos, prev_tpos;    // transport position in beats (negative during count-in)
static float last_songb;         // engine beat clock last frame (to derive a delta)
static int   clickflash;
#define COUNTIN_BEATS 4          // one bar of 4/4 count-in
#define REC_MAX_BEATS 64.0f      // safety: auto-stop a runaway recording (16 bars)
#define BEATS_PER_BAR 4
#define TEMPO 100

// ── the bass fingerboard (a lean cut of upright.c) ──
#define TOPBAR   44
#define NECK_X0  30
#define NECK_X1  292
#define SPAN     12              // semitones nut→bridge (one octave)
#define NLANE    4
#define LANE_H   ((SCREEN_H - TOPBAR) / NLANE)
// lanes top→bottom: G D A E (high string on top), matching upright
static const int   SBASE[NLANE] = { 43, 38, 33, 28 };   // G2 D2 A1 E1
static const char *SLAB [NLANE] = { "G", "D", "A", "E" };

// GarageBand musical-typing map (house layout) — 'A' = the low root
static const char gb_wkey[11]  = "ASDFGHJKL;'";
static const int  gb_wsemi[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
static const char gb_bkey[7]   = { 'W', 'E', 'T', 'Y', 'U', 'O', 'P' };
static const int  gb_bsemi[7]  = { 1, 3, 6, 8, 10, 13, 15 };
#define KB_ROOT 28               // 'A' = E1 at octave 0
static int octv = 1;

// last-played feedback (a glowing dot on the string)
static int   last_lane = -1, last_flash;
static float last_x;

// ── setup ──
void init(void) {
    bpm(TEMPO);

    // the pizzicato upright — INSTR_BOWED plucked (from upright.c's setup_pizz, trimmed)
    instrument(BASS, INSTR_BOWED, 4, 0, 7, 460);
    instrument_mode(BASS, MODE_BOW_PIZZ, 1.0f);          // pluck the string, don't bow it
    instrument_filter(BASS, FILTER_LOW, 1400, 3);
    instrument_harmonics(BASS, 0.30f);                   // dark, woody
    instrument_timbre(BASS, 0.30f);
    instrument_morph(BASS, 0.45f);
    reverb(0.28f, 0.5f);
    instrument_reverb(BASS, 0.14f);

    // the count-in / metronome click
    instrument(CLICK, INSTR_TRI, 0, 30, 0, 20);
}

// ── the recordable seam ──────────────────────────────────────────────────────
// EVERY note the player triggers goes through here. It sounds the voice, and while
// the transport is armed-and-rolling it records the note as an event. Wire the whole
// cart to call pp_note() and recording is free.
static void pp_note(int midi, int vol, int dur) {
    hit(midi, BASS, vol, dur);
    if (mode == TR_REC && tpos >= 0 && nev < MAXEV)
        ev[nev++] = (Ev){ tpos, midi, vol, dur };        // as-played — NO quantize
    // feedback: light the string lane this pitch sits on
    for (int l = 0; l < NLANE; l++) if (SBASE[l] <= midi) { last_lane = l; break; }
    last_x = NECK_X0 + (NECK_X1 - NECK_X0) * (float)(midi - SBASE[last_lane < 0 ? 3 : last_lane]) / SPAN;
    last_flash = 6;
}

// fire every recorded event the playhead crossed since last frame (wrap-aware),
// exactly loopstation's fire_replay logic on our own take window
static void fire_replay(void) {
    int wrap = tpos < prev_tpos;
    for (int i = 0; i < nev; i++) {
        float p = ev[i].pos;
        int hitnow = wrap ? (p > prev_tpos || p <= tpos)
                          : (p > prev_tpos && p <= tpos);
        if (!hitnow) continue;
        pp_note(ev[i].midi, ev[i].vol, ev[i].dur);       // safe: mode==TR_PLAY so it won't re-record
#ifdef DE_TRACE
        watch("fire", "pos=%.2f midi=%d", p, ev[i].midi);
#endif
    }
}

// ── transport control ──
static void start_record(void) {
    nev = 0; song_len = 0;                               // a fresh take replaces the old
    mode = TR_COUNTIN;
    tpos = prev_tpos = -(float)COUNTIN_BEATS;            // count in, then cross zero into REC
}
static void stop_record(void) {
    // first take sets the length — snap to whole bars (round, min one bar)
    int bars = (int)roundf(tpos / BEATS_PER_BAR);
    if (bars < 1) bars = 1;
    song_len = (float)(bars * BEATS_PER_BAR);
    mode = TR_STOP; tpos = prev_tpos = 0;
}
static void start_play(void) {
    if (song_len <= 0) return;
    mode = TR_PLAY; tpos = 0; prev_tpos = -0.001f;       // so an event at pos 0 fires
}
static void stop_all(void) { mode = TR_STOP; }

// ── update ──
void update(void) {
    // derive a transport delta from the engine's beat clock (respects bpm)
    float songb = beat() + beat_pos();
    float db = songb - last_songb;
    if (db < 0 || db > 1.0f) db = 0;                     // guard first frame / any wrap
    last_songb = songb;

    prev_tpos = tpos;

    // ── transport keys + on-screen buttons ──
    if (keyp('R') || tapp(6, 26, 34, 14)) {              // REC
        if (mode == TR_REC || mode == TR_COUNTIN) stop_record();
        else start_record();
    }
    if (keyp(KEY_SPACE) || tapp(44, 26, 34, 14)) {       // PLAY / STOP
        if (mode == TR_PLAY || mode == TR_REC || mode == TR_COUNTIN) stop_all();
        else start_play();
    }
    if (keyp(KEY_BACKSPACE) || tapp(82, 26, 34, 14)) {   // CLR
        nev = 0; song_len = 0; mode = TR_STOP; tpos = 0;
    }

    // ── advance the transport + fire clicks / replay ──
    if (mode != TR_STOP) tpos += db;
    if (mode == TR_COUNTIN || mode == TR_REC) {          // metronome on every beat
        if ((int)floorf(tpos) > (int)floorf(prev_tpos)) {
            int acc = ((((int)floorf(tpos)) % BEATS_PER_BAR) + BEATS_PER_BAR) % BEATS_PER_BAR == 0;
            hit(acc ? 93 : 81, CLICK, acc ? 4 : 2, 25);
            clickflash = 5;
        }
    }
    if (mode == TR_COUNTIN && tpos >= 0) { mode = TR_REC; }   // count-in done → recording
    if (mode == TR_REC && tpos >= REC_MAX_BEATS) stop_record();
    if (mode == TR_PLAY) {
        if (tpos >= song_len) { tpos -= song_len; prev_tpos = -0.001f; }  // loop the take
        fire_replay();
    }

    // ── fingerboard: tap/click a string to pluck a note (discrete, monophonic) ──
    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT) && my >= TOPBAR && mx >= NECK_X0 - 6 && mx <= NECK_X1 + 6) {
        int lane = (my - TOPBAR) / LANE_H; lane = lane < 0 ? 0 : (lane > 3 ? 3 : lane);
        float pos = clamp((float)(mx - NECK_X0) / (NECK_X1 - NECK_X0), 0, 1) * SPAN;
        pp_note(SBASE[lane] + (int)roundf(pos), 7, 240);
    }

    // ── computer keyboard (GarageBand map), discrete notes ──
    for (int i = 0; i < 11; i++) if (keyp(gb_wkey[i])) {
        pp_note(KB_ROOT + octv * 12 + gb_wsemi[i], 7, 240);
    }
    for (int i = 0; i < 7; i++) if (keyp(gb_bkey[i])) {
        pp_note(KB_ROOT + octv * 12 + gb_bsemi[i], 7, 240);
    }
    if (keyp('Z') && octv > 0) octv--;
    if (keyp('X') && octv < 3) octv++;

    if (last_flash > 0) last_flash--;
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
    font(FONT_SMALL);
    print("4-track \x7f phase 1", 6, 15, CLR_INDIGO);
    font(FONT_NORMAL);

    // transport buttons: REC / PLAY / CLR
    int recon = (mode == TR_REC || mode == TR_COUNTIN);
    rectfill(6,  26, 34, 14, recon ? CLR_RED : CLR_DARK_BLUE);
    rect(6, 26, 34, 14, recon ? CLR_WHITE : CLR_DARK_GREY);
    print("REC", 12, 30, recon ? CLR_WHITE : CLR_RED);

    int plon = (mode == TR_PLAY);
    rectfill(44, 26, 34, 14, plon ? CLR_GREEN : CLR_DARK_BLUE);
    rect(44, 26, 34, 14, plon ? CLR_WHITE : CLR_DARK_GREY);
    print("PLY", 50, 30, plon ? CLR_BLACK : CLR_GREEN);

    rectfill(82, 26, 34, 14, CLR_DARK_BLUE);
    rect(82, 26, 34, 14, CLR_DARK_GREY);
    print("CLR", 88, 30, CLR_MEDIUM_GREY);

    // the tape counter: bar.beat, plus song length + event count
    float shown = tpos < 0 ? 0 : tpos;
    int bar  = (int)(shown / BEATS_PER_BAR) + 1;
    int bt   = (int)(shown) % BEATS_PER_BAR + 1;
    char buf[24];
    snprintf(buf, sizeof buf, "%03d.%d", bar, bt);
    print(buf, 150, 6, CLR_LIME_GREEN);

    font(FONT_SMALL);
    if (song_len > 0) {
        snprintf(buf, sizeof buf, "take %d bars  %d notes", (int)(song_len / BEATS_PER_BAR), nev);
        print(buf, 150, 20, CLR_MEDIUM_GREY);
    } else {
        print(nev ? "recording..." : "empty - hit REC", 150, 20, CLR_MEDIUM_GREY);
    }
    print_right(str("%d BPM", TEMPO), SCREEN_W - 6, 6, CLR_INDIGO);
    font(FONT_NORMAL);

    // count-in numbers (4..3..2..1) big in the middle
    if (mode == TR_COUNTIN) {
        int n = (int)ceilf(-tpos);
        if (n < 1) n = 1;
        print_scaled(str("%d", n), SCREEN_W / 2 - 6, 8, 3, clickflash ? CLR_YELLOW : CLR_ORANGE);
    }
    // REC lamp
    if ((mode == TR_REC) && blink(16)) circfill(SCREEN_W - 12, 34, 3, CLR_RED);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // ── the fingerboard ──
    rectfill(0, TOPBAR, SCREEN_W, SCREEN_H - TOPBAR, CLR_BROWN);
    rectfill(NECK_X0 - 6, TOPBAR, NECK_X1 - NECK_X0 + 12, SCREEN_H - TOPBAR, CLR_BROWNISH_BLACK);
    // scroll/pegs left, bridge right
    for (int p = 0; p < 4; p++) circfill(12, TOPBAR + 16 + p * 34, 3, CLR_LIGHT_PEACH);
    rectfill(NECK_X1 + 2, TOPBAR + 4, 4, SCREEN_H - TOPBAR - 8, CLR_LIGHT_PEACH);

    // faint semitone guides (fretless — just a guide for the eye)
    for (int s = 1; s < SPAN; s++) {
        int x = NECK_X0 + (NECK_X1 - NECK_X0) * s / SPAN;
        int c = (s == 5 || s == 7 || s == 12) ? CLR_DARK_PEACH : CLR_DARK_BROWN;
        line(x, TOPBAR + 2, x, SCREEN_H - 2, c);
    }

    // the four strings (E thickest at the bottom)
    for (int l = 0; l < NLANE; l++) {
        int cy = TOPBAR + l * LANE_H + LANE_H / 2;
        int thick = l;                                   // lane 3 (E) thickest, lane 0 (G) thinnest
        int lit = (last_flash > 0 && last_lane == l);
        int col = lit ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY;
        for (int tk = 0; tk <= thick; tk++) line(NECK_X0, cy + tk, NECK_X1, cy + tk, col);
        print(SLAB[l], 4, cy - 3, CLR_LIGHT_PEACH);
    }

    // the pluck: a glowing dot where the last note sounded
    if (last_flash > 0 && last_lane >= 0) {
        int fx = (int)clamp(last_x, NECK_X0, NECK_X1);
        int fy = TOPBAR + last_lane * LANE_H + LANE_H / 2;
        circfill(fx, fy, 4, CLR_YELLOW);
        circ(fx, fy, 4, CLR_WHITE);
    }

    draw_topbar();

    // one-line hint
    font(FONT_SMALL);
    print("R rec  SPACE play/stop  BKSP clear   A-K / tap a string", 6, SCREEN_H - 9, CLR_DARK_PEACH);
    font(FONT_NORMAL);
}
