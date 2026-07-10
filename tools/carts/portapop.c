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
  "lineage": "Phase-3 build of the portapop cassette 4-track (design/portapop.md). Record core (note-on + pitch-stream + note-off) descends from loopstation.c; the bass is a lean pizzicato cut of upright.c, the drum kit after fingerdrums' acoustic kit, the Rhodes the 'stage' INSTR_EPIANO preset from epiano.c. Phase 2 gave it MULTITRACK + a CONSOLE mixer (per-slot level/pan/VU/arm/mute) + overdub monitoring + the TAKE<->CONSOLE mode-flip. Phase 3 makes each track CASTABLE to a different instrument — three of the shelf so far: upright BASS (a melodic fingerboard), a DRUM kit (four pads), and a Rhodes e-piano KEYS (a polyphonic keybed, struck fire-and-forget so chords ring). The remaining shelf voices + ping-pong bounce + the tape glue are still to come.",
  "homage": "Tascam Portastudio",
  "description": {
    "summary": "The bedroom cassette 4-track. Overdub a band alone: cut a bass line, arm the next track and play ALONG, then cast a track to DRUMS and lay a beat under it. Flip to the console to mix — per-track level, pan and bouncing VU. Un-quantized: it loops back exactly as you played it.",
    "detail": "Phase 3 of the portapop multitracker (design/portapop.md). FOUR tracks, each CASTABLE to an instrument — so far upright BASS (a melodic fingerboard), a DRUM kit (four pads), or a Rhodes e-piano (a keybed). Arm a track and RECORD: a one-bar count-in, then it captures your notes/slides (bass), hits (drums) or struck chords (keys) as control events while the OTHER tracks play back so you overdub in time. The longest take sets the song; shorter tracks loop to fill it. Flip to the CONSOLE (TAB) to mix — a level fader, a pan slider and a live VU per track, plus arm/mute and the instrument chip — the per-slot mixer family (instrument_level/instrument_pan) as the show. Three of the shelf-of-8 so far; more voices, ping-pong bounce and the tape glue are still to come.",
    "controls": "CONSOLE view: tap a track's INST chip to cast it (BASS / DRUM / KEYS), drag its LEVEL fader + PAN slider, tap ARM to pick the record track, MUTE to silence it. TAKE view (BASS): press ON a string (E A D G), slide to glide, pull to bend, sweep the gap to pick; TUNE (I) snaps to tune. TAKE view (DRUMS): tap a pad (or A kick / S snare / D hat / F tom). TAKE view (KEYS): tap the Rhodes keys (or A S D F G H J K + W E T Y U black), Z/X octave — struck, so chords ring. Transport (both views): R record (count-in), SPACE play/stop, BACKSPACE clear the armed track, TAB flip views."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// ── PORTAPOP — phase 2: multitrack + console + the mode-flip ──────────────────
// The bedroom cassette 4-track. Overdub one honest take at a time: cut a bass line,
// arm the next track, play ALONG to what's down, stack a band alone. Flip to the
// console to mix — per-track level/pan/VU (the per-slot mixer family as the star).
// One instrument (bass) across all four tracks for now; the shelf of 8 is phase 3.

#define NTRK  4
#define MAXEV 1024
static const int TSLOT[NTRK] = { 5, 6, 7, 8 };     // a track's MELODIC slot (used when cast to BASS)

// the instruments a track can be cast to (phase 3: three of the shelf of 8)
enum { INST_BASS, INST_DRUMS, INST_KEYS, NINST };
static const char *INAME[NINST] = { "BASS", "DRUM", "KEYS" };
#define KEYS_DUR 500                               // a struck Rhodes note rings ~this long then decays

// a little keybed for the KEYS instrument (C3 up, ~1.5 octaves)
static const int KWSEMI[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int KBSEMI[7] = { 1, 3, -1, 6, 8, 10, -1 };
#define KB_BASE 48
#define KB_NW   10
static int white_midi(int i) { return KB_BASE + (i / 7) * 12 + KWSEMI[i % 7]; }
static int black_midi(int i) { int b = KBSEMI[i % 7]; return b < 0 ? -1 : KB_BASE + (i / 7) * 12 + b; }
static int keyflash, keyflash_midi;

// a shared drum kit (percussion is fire-and-forget, so all drum tracks share these voices;
// per-track LEVEL/PAN on a drum track rides the kit — two drum tracks would share that mix)
#define KICK  9
#define SNARE 10
#define HAT   11
#define TOM   12
#define METRO 13
static const int   DSLOT[4] = { KICK, SNARE, HAT, TOM };
static const int   DMIDI[4] = { 36, 52, 84, 45 };
static const int   DDUR [4] = { 130, 90, 30, 170 };
static const char *DNAME[4] = { "KICK", "SNARE", "HAT", "TOM" };
static int padflash[4];

enum { EV_ON, EV_CC, EV_OFF };                     // attack · pitch change (slide/bend) · damp
// lane = the string played (BASS) OR the drum index 0..3 (DRUMS)
typedef struct { float pos, pitch; int kind, vol, lane; } Ev;
typedef struct {
    Ev    ev[MAXEV];
    int   n;
    int   inst;                                    // INST_BASS / INST_DRUMS
    float len;                                     // this track's length in beats (0 = empty)
    int   muted;
    float level, pan;                              // console mix: level 0..1, pan -1..+1
    float applied_level, applied_pan;              // last values pushed (set-and-hold)
    int   vrep;                                    // replay voice handle (BASS only; -1 = none)
    float vu;                                      // meter (decays)
    float g_pitch; int g_lane, g_on;               // replay ghost (BASS fingerboard)
} Track;
static Track trk[NTRK];
static int   armed = 0;                            // the track that records / is played live
static float song_len;                             // = max track len; the loop window
static float rec_last_pitch;

// ── transport ──
enum { TR_STOP, TR_COUNTIN, TR_REC, TR_PLAY };
static int   mode = TR_STOP;
static float tpos, prev_tpos, last_songb;
static int   clickflash;
#define COUNTIN_BEATS 4
#define REC_MAX_BEATS 64.0f
#define BEATS_PER_BAR 4
#define TEMPO 100

// ── view ──
enum { VIEW_TAKE, VIEW_CONSOLE };
static int view = VIEW_CONSOLE;

// ── the bass fingerboard (pizz, fretless, from upright.c) ──
#define TOPBAR   44
#define NECK_X0  30
#define NECK_X1  292
#define SPAN     12
#define NLANE    4
#define LANE_H   ((SCREEN_H - TOPBAR) / NLANE)
static const int   SBASE[NLANE] = { 43, 38, 33, 28 };   // G2 D2 A1 E1
static const char *SLAB [NLANE] = { "G", "D", "A", "E" };
#define GRAB_PX  13
#define BEND_K   0.05f
#define BEND_MAX 2.0f
#define NONE  (-999)
#define KBD   (-2)
#define MOUSE (-3)
enum { GRAB, PICK };
static int   b_handle = -1, b_owner = NONE, b_lane = 3, b_gesture, b_kbsemi = -1;
static float b_midi = 28, b_slide = 28;
static int   b_press_x, b_press_y, b_prevy, b_fx, b_fy, b_sliding;
static float b_wob;

static const char gb_wkey[11]  = "ASDFGHJKL;'";
static const int  gb_wsemi[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
static const char gb_bkey[7]   = { 'W', 'E', 'T', 'Y', 'U', 'O', 'P' };
static const int  gb_bsemi[7]  = { 1, 3, 6, 8, 10, 13, 15 };
#define KB_ROOT 28
static int octv = 1, snap = 0;
static float snap_pitch(float m) { return snap ? roundf(m) : m; }

static int   cy_of(int lane) { return TOPBAR + lane * LANE_H + LANE_H / 2; }
static float pos_at(int x) { return clamp((float)(x - NECK_X0) / (NECK_X1 - NECK_X0), 0, 1) * SPAN; }
static int   midi_lane(float m) { int l = 3; for (int i = 0; i < NLANE; i++) if (SBASE[i] <= m) { l = i; break; } return l; }
static int   midi_x(float m, int lane) { return NECK_X0 + (int)((NECK_X1 - NECK_X0) * (m - SBASE[lane]) / SPAN); }

// console drag capture (one control at a time — no ui.h, avoids pointer-pool cross-talk)
enum { DRAG_NONE, DRAG_FADER, DRAG_PAN };
static int drag_kind = DRAG_NONE, drag_trk = -1;
static const int TCOL[NTRK] = { CLR_ORANGE, CLR_BLUE_GREEN, CLR_PINK, CLR_LIME_GREEN };

// a track's MELODIC slot is (re)configured for its instrument when it's cast
static void cfg_bass(int s) {
    instrument(s, INSTR_BOWED, 4, 0, 7, 460);
    instrument_mode(s, MODE_BOW_PIZZ, 1.0f);
    instrument_filter(s, FILTER_LOW, 1400, 3);
    instrument_harmonics(s, 0.30f); instrument_timbre(s, 0.30f); instrument_morph(s, 0.45f);
    instrument_drive(s, 0.0f); instrument_reverb(s, 0.14f);
}
static void cfg_keys(int s) {                       // the "stage" RHODES, verbatim from epiano.c
    instrument(s, INSTR_EPIANO, 1, 0, 7, 450);
    instrument_harmonics(s, 0.15f);                 // = the tine machine (M_INST[RHODES])
    instrument_timbre(s, 0.30f);                    // stage brightness
    instrument_morph(s, 0.25f);                     // pickup bark
    instrument_drive(s, 0.0f);                      // bark < 0.5 -> no drive (clean stage Rhodes)
    instrument_filter(s, FILTER_OFF, 4000, 0);      // wah off
    instrument_reverb(s, 0.16f);
}
static void configure_slot(int t) {
    if (trk[t].inst == INST_BASS) cfg_bass(TSLOT[t]);
    else if (trk[t].inst == INST_KEYS) cfg_keys(TSLOT[t]);
    // DRUMS uses the shared kit, not TSLOT[t]
}

// ── setup ──
void init(void) {
    bpm(TEMPO);
    for (int t = 0; t < NTRK; t++) {
        trk[t].level = 0.85f; trk[t].pan = 0; trk[t].vrep = -1;
        trk[t].applied_level = -1; trk[t].applied_pan = -2;   // force a first push
        trk[t].inst = INST_BASS;
    }
    trk[1].inst = INST_DRUMS;                       // advertise the pairing: track 2 is the kit
    for (int t = 0; t < NTRK; t++) configure_slot(t);

    // the shared drum kit — a warm acoustic-ish set (values after fingerdrums' acoustic kit)
    instrument(KICK, INSTR_SINE, 0, 300, 0, 80); instrument_filter(KICK, FILTER_LOW, 220, 2);
    instrument_env(KICK, 0, ENV_PITCH, 0, 55, 28.0f); instrument_drive(KICK, 0.15f);
    instrument(SNARE, INSTR_NOISE, 0, 120, 0, 60); instrument_filter(SNARE, FILTER_BAND, 1900, 3);
    instrument(HAT, INSTR_NOISE, 0, 40, 0, 24); instrument_filter(HAT, FILTER_HIGH, 8500, 2);
    instrument(TOM, INSTR_MEMBRANE, 1, 0, 7, 300);
    instrument_harmonics(TOM, 0.28f); instrument_timbre(TOM, 0.18f); instrument_morph(TOM, 0.12f);

    reverb(0.28f, 0.5f);
    instrument(METRO, INSTR_TRI, 0, 30, 0, 20);
}

// ── the recordable seam (records into the ARMED track) ──
static void rec(int kind, float pitch, int vol) {
    Track *T = &trk[armed];
    if (mode == TR_REC && tpos >= 0 && T->n < MAXEV)
        T->ev[T->n++] = (Ev){ tpos, pitch, kind, vol, b_lane };
}
static void pp_on(int owner, int lane, float midi, int vol) {
    if (b_handle >= 0) note_off(b_handle);
    b_owner = owner; b_lane = lane; b_midi = b_slide = midi; b_wob = 0;
    b_handle = note_on((int)(midi + 0.5f), TSLOT[armed], vol);
    note_pitch(b_handle, midi); note_glide(b_handle, 70);
    rec(EV_ON, midi, vol); rec_last_pitch = midi;
    b_fx = midi_x(midi, lane); b_fy = cy_of(lane);
}
static void pp_slide(float midi) {
    if (b_handle < 0) return;
    b_midi = midi; note_pitch(b_handle, midi);
    if (fabsf(midi - rec_last_pitch) > 0.05f) { rec(EV_CC, midi, 0); rec_last_pitch = midi; }
}
static void pp_off(void) {
    if (b_handle < 0) return;
    note_off(b_handle); b_handle = -1;
    rec(EV_OFF, 0, 0);
}
// drums: a fire-and-forget hit; the take stores which drum in `lane` (no pitch stream)
static void pp_hit(int drum) {
    hit(DMIDI[drum], DSLOT[drum], 7, DDUR[drum]);
    padflash[drum] = 6;
    Track *T = &trk[armed];
    if (mode == TR_REC && tpos >= 0 && T->n < MAXEV)
        T->ev[T->n++] = (Ev){ tpos, 0, EV_ON, 7, drum };
}
// keys: a struck Rhodes note, fire-and-forget (rings + decays) — polyphonic for free
static void pp_key(int midi) {
    hit(midi, TSLOT[armed], 6, KEYS_DUR);
    keyflash = 6; keyflash_midi = midi;
    Track *T = &trk[armed];
    if (mode == TR_REC && tpos >= 0 && T->n < MAXEV)
        T->ev[T->n++] = (Ev){ tpos, (float)midi, EV_ON, 6, 0 };
}

// ── replay (per track, on its own slot) ──
static void fire_ev(int t, Ev *e) {
    Track *T = &trk[t];
#ifdef DE_TRACE
    watch("fire", "t%d k%d pos=%.2f midi=%.1f", t, e->kind, e->pos, e->pitch);
#endif
    if (T->inst == INST_DRUMS) {                    // percussion: fire-and-forget, `lane` = drum
        if (e->kind == EV_ON) { int d = e->lane; hit(DMIDI[d], DSLOT[d], e->vol, DDUR[d]); padflash[d] = 6; T->vu = 1.0f; }
        return;
    }
    if (T->inst == INST_KEYS) {                     // struck Rhodes: fire-and-forget hit, `pitch` = note
        if (e->kind == EV_ON) { hit((int)e->pitch, TSLOT[t], e->vol, KEYS_DUR); keyflash = 6; keyflash_midi = (int)e->pitch; T->vu = 1.0f; }
        return;
    }
    if (e->kind == EV_ON) {
        if (T->vrep >= 0) note_off(T->vrep);
        T->vrep = note_on((int)(e->pitch + 0.5f), TSLOT[t], e->vol);
        note_pitch(T->vrep, e->pitch); note_glide(T->vrep, 70);
        T->g_pitch = e->pitch; T->g_lane = e->lane; T->g_on = 1;
        T->vu = 1.0f;
    } else if (e->kind == EV_CC) {
        if (T->vrep >= 0) { note_pitch(T->vrep, e->pitch); T->g_pitch = e->pitch; }
    } else {
        if (T->vrep >= 0) { note_off(T->vrep); T->vrep = -1; }
        T->g_on = 0;
    }
}
// fire track t's events crossed this frame, looping at ITS OWN length (shorter tracks loop to fill)
static void fire_track(int t) {
    Track *T = &trk[t];
    if (T->muted || T->n == 0 || T->len <= 0) return;
    float L = T->len;
    float wp  = fmodf(tpos, L);      if (wp  < 0) wp  += L;
    float pwp = fmodf(prev_tpos, L); if (pwp < 0) pwp += L;
    int wrap = wp < pwp;
    for (int i = 0; i < T->n; i++) {
        float p = T->ev[i].pos;
        int hitnow = wrap ? (p > pwp || p <= wp) : (p > pwp && p <= wp);
        if (hitnow) fire_ev(t, &T->ev[i]);
    }
}
static void play_tracks(int skip) { for (int t = 0; t < NTRK; t++) if (t != skip) fire_track(t); }
static void silence(int t) { if (trk[t].vrep >= 0) { note_off(trk[t].vrep); trk[t].vrep = -1; } trk[t].g_on = 0; }
static void silence_all(void) { for (int t = 0; t < NTRK; t++) silence(t); }

// ── transport control ──
static void recompute_song(void) { song_len = 0; for (int t = 0; t < NTRK; t++) if (trk[t].len > song_len) song_len = trk[t].len; }
static void start_record(void) {
    silence_all();
    trk[armed].n = 0; trk[armed].len = 0;              // a fresh take replaces this track
    mode = TR_COUNTIN; tpos = prev_tpos = -(float)COUNTIN_BEATS;
    view = VIEW_TAKE;
}
static void stop_record(void) {
    Track *T = &trk[armed];
    float last = 0; for (int i = 0; i < T->n; i++) if (T->ev[i].pos > last) last = T->ev[i].pos;
    int bars = (int)roundf(tpos / BEATS_PER_BAR); if (bars < 1) bars = 1;
    if (bars * BEATS_PER_BAR < last) bars = (int)ceilf(last / BEATS_PER_BAR);   // never drop a late note
    T->len = (float)(bars * BEATS_PER_BAR);
    recompute_song();
    if (b_handle >= 0) { note_off(b_handle); b_handle = -1; }
    mode = TR_STOP; tpos = prev_tpos = 0;
    view = VIEW_CONSOLE;
}
static void start_play(void) {
    recompute_song();
    if (song_len <= 0) return;
    silence_all();
    mode = TR_PLAY; tpos = 0; prev_tpos = -0.001f;
    view = VIEW_CONSOLE;
}
static void stop_all(void) { mode = TR_STOP; silence_all(); if (b_handle >= 0) { note_off(b_handle); b_handle = -1; } }

// ── console strip geometry (shared by input + draw) ──
#define STR_INST_Y  56
#define STR_FADE_Y0 72
#define STR_FADE_Y1 140
#define STR_PAN_Y   148
#define STR_BTN_Y   162

static void console_input(void) {
    int mx = mouse_x(), my = mouse_y();
    int SW = SCREEN_W / NTRK;
    int rec = (mode == TR_REC || mode == TR_COUNTIN);
    // faders / pans: capture on press, ride while held, release to let go
    if (mouse_pressed(MOUSE_LEFT)) {
        for (int t = 0; t < NTRK; t++) {
            int x = t * SW;
            if (mx >= x + 24 && mx <= x + 40 && my >= STR_FADE_Y0 && my <= STR_FADE_Y1) { drag_kind = DRAG_FADER; drag_trk = t; break; }
            if (mx >= x + 6  && mx <= x + SW - 6 && my >= STR_PAN_Y && my <= STR_PAN_Y + 10) { drag_kind = DRAG_PAN; drag_trk = t; break; }
        }
    }
    if (mouse_down(MOUSE_LEFT) && drag_trk >= 0) {
        int x = drag_trk * SW;
        if (drag_kind == DRAG_FADER) trk[drag_trk].level = clamp(remap((float)my, STR_FADE_Y0, STR_FADE_Y1, 1, 0), 0, 1);
        else if (drag_kind == DRAG_PAN) trk[drag_trk].pan = clamp(remap((float)mx, x + 6, x + SW - 6, -1, 1), -1, 1);
    }
    if (!mouse_down(MOUSE_LEFT)) { drag_kind = DRAG_NONE; drag_trk = -1; }
    for (int t = 0; t < NTRK; t++) {
        int x = t * SW;
        // INST chip: cycle the track's instrument (clears the take — events reinterpret otherwise)
        if (tapp(x + 4, STR_INST_Y, SW - 8, 12) && !rec) {
            silence(t); trk[t].inst = (trk[t].inst + 1) % NINST;
            configure_slot(t);                                    // re-voice the melodic slot (bass/keys)
            trk[t].n = 0; trk[t].len = 0; recompute_song();
            trk[t].applied_level = -1; trk[t].applied_pan = -2;   // re-push mix to the new slots
        }
        if (tapp(x + 6, STR_BTN_Y, 32, 15) && !rec) armed = t;
        if (tapp(x + 42, STR_BTN_Y, SW - 48, 15)) { trk[t].muted ^= 1; if (trk[t].muted) silence(t); }
    }
}

// ── update ──
void update(void) {
    float songb = beat() + beat_pos();
    float db = songb - last_songb; if (db < 0 || db > 1.0f) db = 0;
    last_songb = songb;
    prev_tpos = tpos;

    // transport (both views)
    if (keyp('R') || tapp(6, 26, 34, 14)) {
        if (mode == TR_REC || mode == TR_COUNTIN) stop_record();
        else start_record();
    }
    if (keyp(KEY_SPACE) || tapp(44, 26, 34, 14)) {
        if (mode == TR_PLAY || mode == TR_REC || mode == TR_COUNTIN) stop_all();
        else start_play();
    }
    if (keyp(KEY_BACKSPACE) || tapp(82, 26, 34, 14)) { silence(armed); trk[armed].n = 0; trk[armed].len = 0; recompute_song(); }
    if (keyp(KEY_TAB)) view ^= 1;

    // advance transport + clicks + replay/monitor
    if (mode != TR_STOP) tpos += db;
    if (mode == TR_COUNTIN || mode == TR_REC) {
        if ((int)floorf(tpos) > (int)floorf(prev_tpos)) {
            int acc = ((((int)floorf(tpos)) % BEATS_PER_BAR) + BEATS_PER_BAR) % BEATS_PER_BAR == 0;
            hit(acc ? 93 : 81, METRO, acc ? 4 : 2, 25); clickflash = 5;
        }
    }
    if (mode == TR_COUNTIN && tpos >= 0) mode = TR_REC;
    if (mode == TR_REC) { if (tpos >= REC_MAX_BEATS) stop_record(); else play_tracks(armed); }  // monitor the others
    if (mode == TR_PLAY) {
        if (tpos >= song_len) { tpos -= song_len; prev_tpos = -0.001f; }
        play_tracks(-1);                                  // mix all
    }

    // push per-slot mix only when a fader/pan actually moved (set-and-hold). A drum track
    // rides all four kit slots (shared kit — see the note at the slot defines).
    for (int t = 0; t < NTRK; t++) {
        if (trk[t].level != trk[t].applied_level) {
            if (trk[t].inst == INST_DRUMS) for (int d = 0; d < 4; d++) instrument_level(DSLOT[d], trk[t].level);
            else instrument_level(TSLOT[t], trk[t].level);
            trk[t].applied_level = trk[t].level;
        }
        if (trk[t].pan != trk[t].applied_pan) {
            if (trk[t].inst == INST_DRUMS) for (int d = 0; d < 4; d++) instrument_pan(DSLOT[d], trk[t].pan);
            else instrument_pan(TSLOT[t], trk[t].pan);
            trk[t].applied_pan = trk[t].pan;
        }
        if (trk[t].vu > 0) trk[t].vu -= 0.05f;
    }
    for (int d = 0; d < 4; d++) if (padflash[d] > 0) padflash[d]--;

    // ── the armed track's play surface (TAKE view): bass fingerboard, or drum pads ──
    if (view == VIEW_TAKE && trk[armed].inst == INST_BASS) {
        int mx = mouse_x(), my = mouse_y();
        if (mouse_pressed(MOUSE_LEFT) && my >= TOPBAR && mx >= NECK_X0 - 6 && mx <= NECK_X1 + 6) {
            int near = 0, nd = 9999;
            for (int l = 0; l < NLANE; l++) { int d = abs(my - cy_of(l)); if (d < nd) { nd = d; near = l; } }
            if (nd <= GRAB_PX) { pp_on(MOUSE, near, snap_pitch(SBASE[near] + pos_at(mx)), 7); b_gesture = GRAB; b_fx = mx; b_fy = my; }
            else { b_owner = MOUSE; b_gesture = PICK; }
            b_press_x = mx; b_press_y = my; b_prevy = my; b_sliding = 0;
        }
        else if (mouse_down(MOUSE_LEFT) && b_owner == MOUSE && b_gesture == GRAB && b_handle >= 0) {
            if (!b_sliding && (abs(mx - b_press_x) > 4 || abs(my - b_press_y) > 4)) b_sliding = 1;
            if (b_sliding) {
                int dpx = b_press_y - my;
                if (abs(dpx) <= 6) b_slide = SBASE[b_lane] + clamp(pos_at(mx), 0, SPAN);
                float vbend = clamp((float)dpx * BEND_K, -BEND_MAX, BEND_MAX);
                pp_slide(b_slide + vbend);
                int maxpx = (int)(BEND_MAX / BEND_K);
                b_fx = midi_x(b_slide, b_lane);
                b_fy = cy_of(b_lane) - (int)clamp((float)dpx, -(float)maxpx, (float)maxpx);
            }
        }
        else if (mouse_down(MOUSE_LEFT) && b_owner == MOUSE && b_gesture == PICK) {
            for (int l = 0; l < NLANE; l++) {
                int cy = cy_of(l);
                if ((b_prevy < cy && my >= cy) || (b_prevy > cy && my <= cy))
                    pp_on(MOUSE, l, snap_pitch(SBASE[l] + clamp(pos_at(mx), 0, SPAN)), 7);
            }
            b_fx = mx; b_fy = my; b_prevy = my;
        }
        if (!mouse_down(MOUSE_LEFT) && b_owner == MOUSE) { if (b_gesture == GRAB) pp_off(); else b_owner = NONE; }

        for (int i = 0; i < 11; i++) {
            if (keyp(gb_wkey[i])) { int m = KB_ROOT + octv * 12 + gb_wsemi[i]; pp_on(KBD, midi_lane(m), (float)m, 7); b_owner = KBD; b_kbsemi = gb_wsemi[i]; }
            if (keyr(gb_wkey[i]) && b_owner == KBD && gb_wsemi[i] == b_kbsemi) pp_off();
        }
        for (int i = 0; i < 7; i++) {
            if (keyp(gb_bkey[i])) { int m = KB_ROOT + octv * 12 + gb_bsemi[i]; pp_on(KBD, midi_lane(m), (float)m, 7); b_owner = KBD; b_kbsemi = gb_bsemi[i]; }
            if (keyr(gb_bkey[i]) && b_owner == KBD && gb_bsemi[i] == b_kbsemi) pp_off();
        }
        if (keyp('Z') && octv > 0) octv--;
        if (keyp('X') && octv < 3) octv++;
        if (keyp('I')) snap ^= 1;
    } else if (view == VIEW_TAKE && trk[armed].inst == INST_DRUMS) {   // four pads
        int PW = SCREEN_W / 2, PH = (SCREEN_H - TOPBAR) / 2;
        for (int d = 0; d < 4; d++)
            if (tapp((d % 2) * PW, TOPBAR + (d / 2) * PH, PW, PH)) pp_hit(d);
        if (keyp('A')) pp_hit(0);                   // kick
        if (keyp('S')) pp_hit(1);                   // snare
        if (keyp('D')) pp_hit(2);                   // hat
        if (keyp('F')) pp_hit(3);                   // tom
    } else if (view == VIEW_TAKE) {                 // armed track is KEYS: a Rhodes keybed
        int ww = SCREEN_W / KB_NW, bh = (int)((SCREEN_H - TOPBAR) * 0.58f);
        for (int i = 0; i < KB_NW; i++) {           // blacks (upper) first
            int bm = black_midi(i); if (bm < 0) continue;
            int bw = 2 * ww / 3, bx = (i + 1) * ww - bw / 2;
            if (tapp(bx, TOPBAR, bw, bh)) pp_key(bm);
        }
        for (int i = 0; i < KB_NW; i++)             // whites (lower)
            if (tapp(i * ww, TOPBAR + bh, ww, SCREEN_H - TOPBAR - bh)) pp_key(white_midi(i));
        for (int i = 0; i < 11; i++) if (keyp(gb_wkey[i])) pp_key(KB_BASE + octv * 12 + gb_wsemi[i]);
        for (int i = 0; i < 7;  i++) if (keyp(gb_bkey[i])) pp_key(KB_BASE + octv * 12 + gb_bsemi[i]);
        if (keyp('Z') && octv > 0) octv--;
        if (keyp('X') && octv < 3) octv++;
    } else {
        console_input();
    }
    if (keyflash > 0) keyflash--;

    b_wob += 0.6f;
    if (clickflash > 0) clickflash--;

#ifdef DE_TRACE
    watch("mode", "%d", mode);
    watch("view", "%d", view);
    watch("tpos", "%.2f", tpos);
    watch("armed", "%d", armed);
    watch("lens", "%.0f/%.0f/%.0f/%.0f", trk[0].len, trk[1].len, trk[2].len, trk[3].len);
    watch("song", "%.1f", song_len);
    watch("ns", "%d/%d/%d/%d", trk[0].n, trk[1].n, trk[2].n, trk[3].n);
#endif
}

// ── draw: shared topbar ──
static void draw_topbar(void) {
    rectfill(0, 0, SCREEN_W, TOPBAR, CLR_DARKER_BLUE);
    line(0, TOPBAR - 1, SCREEN_W, TOPBAR - 1, CLR_BLACK);
    print("PORTAPOP", 6, 5, CLR_LIGHT_PEACH);
    font(FONT_SMALL); print(view == VIEW_TAKE ? "TAKE \x7f trk" : "CONSOLE", 6, 15, CLR_INDIGO);
    if (view == VIEW_TAKE) print(str("%d", armed + 1), 44, 15, TCOL[armed]);
    font(FONT_NORMAL);

    int recon = (mode == TR_REC || mode == TR_COUNTIN);
    rectfill(6,  26, 34, 14, recon ? CLR_RED   : CLR_DARK_BLUE); rect(6,  26, 34, 14, recon ? CLR_WHITE : CLR_DARK_GREY); print("REC", 12, 30, recon ? CLR_WHITE : CLR_RED);
    int plon = (mode == TR_PLAY);
    rectfill(44, 26, 34, 14, plon ? CLR_GREEN : CLR_DARK_BLUE); rect(44, 26, 34, 14, plon ? CLR_WHITE : CLR_DARK_GREY); print("PLY", 50, 30, plon ? CLR_BLACK : CLR_GREEN);
    rectfill(82, 26, 34, 14, CLR_DARK_BLUE); rect(82, 26, 34, 14, CLR_DARK_GREY); print("CLR", 88, 30, CLR_MEDIUM_GREY);

    float shown = tpos < 0 ? 0 : tpos;
    int bar = (int)(shown / BEATS_PER_BAR) + 1, bt = (int)(shown) % BEATS_PER_BAR + 1;
    char buf[24]; snprintf(buf, sizeof buf, "%03d.%d", bar, bt); print(buf, 150, 6, CLR_LIME_GREEN);
    font(FONT_SMALL);
    if (song_len > 0) { snprintf(buf, sizeof buf, "song %d bars", (int)(song_len / BEATS_PER_BAR)); print(buf, 150, 20, CLR_MEDIUM_GREY); }
    else print("empty - arm a track, hit REC", 150, 20, CLR_MEDIUM_GREY);
    print_right(str("%d BPM  TAB view", TEMPO), SCREEN_W - 6, 6, CLR_INDIGO);
    font(FONT_NORMAL);

    if (mode == TR_COUNTIN) { int n = (int)ceilf(-tpos); if (n < 1) n = 1; print_scaled(str("%d", n), SCREEN_W / 2 - 6, 8, 3, clickflash ? CLR_YELLOW : CLR_ORANGE); }
    if (mode == TR_REC && blink(16)) circfill(SCREEN_W - 12, 34, 3, CLR_RED);
}

static void draw_string(int lane, int live, int fx, int fy, int col) {
    int cy = cy_of(lane), thick = lane;
    int peakx = live ? (fx < NECK_X0 + 1 ? NECK_X0 + 1 : (fx > NECK_X1 - 1 ? NECK_X1 - 1 : fx)) : NECK_X0;
    int px = NECK_X0, py = cy;
    for (int x = NECK_X0; x <= NECK_X1; x += 4) {
        int y = cy;
        if (live) {
            float t = x <= peakx ? (float)(x - NECK_X0) / (peakx - NECK_X0) : (float)(NECK_X1 - x) / (NECK_X1 - peakx);
            t = clamp(t, 0, 1);
            y = cy + (int)((fy - cy) * t + sinf(b_wob + x * 0.20f) * t * 1.8f);
        }
        for (int tk = 0; tk <= thick; tk++) line(px, py + tk, x, y + tk, col);
        px = x; py = y;
    }
}

static void draw_take_drums(void) {
    cls(CLR_BROWNISH_BLACK);
    int PW = SCREEN_W / 2, PH = (SCREEN_H - TOPBAR) / 2;
    static const int pcol[4] = { CLR_ORANGE, CLR_PINK, CLR_LIME_GREEN, CLR_BLUE_GREEN };
    for (int d = 0; d < 4; d++) {
        int px = (d % 2) * PW, py = TOPBAR + (d / 2) * PH, on = padflash[d] > 0;
        rectfill(px + 2, py + 2, PW - 4, PH - 4, on ? pcol[d] : CLR_DARKER_BLUE);
        rect(px + 2, py + 2, PW - 4, PH - 4, CLR_DARK_BLUE);
        print(DNAME[d], px + PW / 2 - text_width(DNAME[d]) / 2, py + PH / 2 - 4, on ? CLR_BLACK : pcol[d]);
    }
    draw_topbar();
    font(FONT_SMALL);
    print("tap a pad  (A kick  S snare  D hat  F tom)   TAB console", 6, SCREEN_H - 9, CLR_DARK_PEACH);
    font(FONT_NORMAL);
}

static void draw_take_keys(void) {
    cls(CLR_BLACK);
    int ww = SCREEN_W / KB_NW, bh = (int)((SCREEN_H - TOPBAR) * 0.58f);
    for (int i = 0; i < KB_NW; i++) {               // white keys
        int m = white_midi(i), on = (keyflash > 0 && keyflash_midi == m);
        rectfill(i * ww + 1, TOPBAR + 1, ww - 2, SCREEN_H - TOPBAR - 2, on ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH);
        rect(i * ww, TOPBAR, ww, SCREEN_H - TOPBAR, CLR_DARK_GREY);
    }
    for (int i = 0; i < KB_NW; i++) {               // black keys on top
        int bm = black_midi(i); if (bm < 0) continue;
        int bw = 2 * ww / 3, bx = (i + 1) * ww - bw / 2, on = (keyflash > 0 && keyflash_midi == bm);
        rectfill(bx, TOPBAR, bw, bh, on ? CLR_ORANGE : CLR_BROWNISH_BLACK);
        rect(bx, TOPBAR, bw, bh, CLR_BLACK);
    }
    draw_topbar();
    font(FONT_SMALL);
    print("tap keys / A S D F G H J K  (W E T Y U black)  Z/X octave   TAB console", 6, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

static void draw_take(void) {
    if (trk[armed].inst == INST_DRUMS) { draw_take_drums(); return; }
    if (trk[armed].inst == INST_KEYS)  { draw_take_keys();  return; }
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, TOPBAR, SCREEN_W, SCREEN_H - TOPBAR, CLR_BROWN);
    rectfill(NECK_X0 - 6, TOPBAR, NECK_X1 - NECK_X0 + 12, SCREEN_H - TOPBAR, CLR_BROWNISH_BLACK);
    for (int p = 0; p < 4; p++) circfill(12, TOPBAR + 16 + p * 34, 3, CLR_LIGHT_PEACH);
    rectfill(NECK_X1 + 2, TOPBAR + 4, 4, SCREEN_H - TOPBAR - 8, CLR_LIGHT_PEACH);
    for (int s = 1; s < SPAN; s++) {
        int x = NECK_X0 + (NECK_X1 - NECK_X0) * s / SPAN;
        line(x, TOPBAR + 2, x, SCREEN_H - 2, (s == 5 || s == 7 || s == 12) ? CLR_DARK_PEACH : CLR_DARK_BROWN);
    }
    for (int l = 0; l < NLANE; l++) {
        int live = (b_handle >= 0 && b_lane == l);
        draw_string(l, live, b_fx, b_fy, live ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY);
        print(SLAB[l], 4, cy_of(l) - 3, CLR_LIGHT_PEACH);
    }
    if (b_handle >= 0) {
        int fx = b_fx < NECK_X0 ? NECK_X0 : (b_fx > NECK_X1 ? NECK_X1 : b_fx);
        circfill(fx, b_fy, 4, CLR_YELLOW); circ(fx, b_fy, 4, CLR_WHITE);
    }
    // ghost of the ARMED track's own recording (if it were playing) is not shown while cutting;
    // monitors show as small VU dots in the topbar instead
    for (int t = 0; t < NTRK; t++) if (t != armed && trk[t].vu > 0.02f)
        circfill(120 + t * 8, 34, 2, TCOL[t]);
    draw_topbar();
    font(FONT_SMALL);
    print("press+slide a string / sweep to pick   I tune   TAB console", 6, SCREEN_H - 9, CLR_DARK_PEACH);
    font(FONT_NORMAL);
}

static void draw_console(void) {
    cls(CLR_BLACK);
    int SW = SCREEN_W / NTRK;
    for (int t = 0; t < NTRK; t++) {
        int x = t * SW;
        int armedhere = (t == armed);
        rectfill(x + 1, TOPBAR + 2, SW - 2, SCREEN_H - TOPBAR - 4, CLR_DARKER_BLUE);
        if (armedhere) rect(x + 1, TOPBAR + 2, SW - 2, SCREEN_H - TOPBAR - 4, CLR_RED);

        print(str("%d", t + 1), x + 6, 47, TCOL[t]);
        if (trk[t].len > 0) { font(FONT_SMALL); print(str("%db", (int)(trk[t].len / BEATS_PER_BAR)), x + SW - 20, 48, CLR_MEDIUM_GREY); font(FONT_NORMAL); }

        // INST chip (tap to cast the track's instrument)
        static const int ICOL[NINST] = { CLR_DARK_PURPLE, CLR_DARK_GREEN, CLR_INDIGO };
        rectfill(x + 4, STR_INST_Y, SW - 8, 12, ICOL[trk[t].inst]);
        rect(x + 4, STR_INST_Y, SW - 8, 12, CLR_DARK_BLUE);
        font(FONT_SMALL); print(INAME[trk[t].inst], x + 8, STR_INST_Y + 3, CLR_LIGHT_PEACH); font(FONT_NORMAL);

        // VU meter (fills from the bottom)
        int vx = x + 8, vy0 = STR_FADE_Y0, vh = STR_FADE_Y1 - STR_FADE_Y0;
        rectfill(vx, vy0, 8, vh, CLR_BROWNISH_BLACK);
        int vf = (int)(clamp(trk[t].vu, 0, 1) * vh);
        for (int i = 0; i < vf; i += 3) {
            int yy = vy0 + vh - i - 2;
            int c = i > vh * 0.8f ? CLR_RED : (i > vh * 0.55f ? CLR_ORANGE : CLR_GREEN);
            rectfill(vx + 1, yy, 6, 2, c);
        }

        // LEVEL fader (drag vertical)
        int fx = x + 24, fy0 = STR_FADE_Y0, fh = STR_FADE_Y1 - STR_FADE_Y0;
        rectfill(fx + 5, fy0, 2, fh, CLR_DARK_BLUE);
        int hy = fy0 + (int)((1 - trk[t].level) * fh);
        rectfill(fx, hy - 3, 16, 6, drag_trk == t && drag_kind == DRAG_FADER ? CLR_WHITE : CLR_LIGHT_GREY);
        font(FONT_SMALL); print("LVL", fx - 1, STR_PAN_Y - 6, CLR_DARK_GREY); font(FONT_NORMAL);

        // PAN slider (drag horizontal)
        rectfill(x + 6, STR_PAN_Y + 3, SW - 12, 2, CLR_DARK_BLUE);
        int hx = x + 6 + (int)((trk[t].pan + 1) / 2 * (SW - 12));
        rectfill(hx - 2, STR_PAN_Y, 4, 8, drag_trk == t && drag_kind == DRAG_PAN ? CLR_WHITE : CLR_LIGHT_GREY);

        // ARM / MUTE
        rectfill(x + 6, STR_BTN_Y, 32, 15, armedhere ? CLR_RED : CLR_DARK_BLUE); rect(x + 6, STR_BTN_Y, 32, 15, armedhere ? CLR_WHITE : CLR_DARK_GREY);
        font(FONT_SMALL); print("ARM", x + 10, STR_BTN_Y + 4, armedhere ? CLR_WHITE : CLR_MEDIUM_GREY);
        int mu = trk[t].muted;
        rectfill(x + 42, STR_BTN_Y, SW - 48, 15, mu ? CLR_MEDIUM_GREY : CLR_DARK_BLUE); rect(x + 42, STR_BTN_Y, SW - 48, 15, mu ? CLR_WHITE : CLR_DARK_GREY);
        print("MUTE", x + 46, STR_BTN_Y + 4, mu ? CLR_BLACK : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }
    draw_topbar();
}

void draw(void) {
    if (view == VIEW_TAKE) draw_take();
    else draw_console();
}
