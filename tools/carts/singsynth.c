/* de:meta
{
  "slug": "singsynth",
  "title": "sing synth",
  "status": "active",
  "kind": ["instrument"],
  "genre": null,
  "teaches": ["audio-input"],
  "created": "2026-07-17",
  "lineage": "Voice->instrument, vein 3 of the audio-input frontier (docs/design/audio-input-frontier.md): the Casio SK-1 sampling keyboard, honestly. Hold a vowel, the engine records it and LOOPS the sustained middle (INSTR_SAMPLE + SAMPLE_LOOP) so keybed.h plays YOUR voice across the whole keyboard, polyphonically. (First cut tried a single wave_set() cycle — too little data to carry a voice; it just sounded like a buzzy sine. Looping real PCM keeps your formants + texture, the sampling doc's Tier-4.) Sibling of humtheremin/humseq (which read your PITCH); this keeps your TIMBRE. Capture-then-freeze, so the buffer is deterministic data — unlike the live voxbox/vocoder.",
  "homage": "The Casio SK-1 (1985) — the toy sampling keyboard a generation played their own 'aaah' on, pitched across the keys.",
  "description": {
    "summary": "Say a vowel and play your own voice as a synth. Tap REC and hold an 'aaah'; the engine loops it into a keyboard instrument you play polyphonically — capture four different vowels and switch between them.",
    "detail": "Voice sampler. Tap the REC button (it opens the mic the first time) and hold a steady vowel; ~1.2s is captured, normalized, loaded into a sample slot, and a held LOOP is set over the sustained middle (instrument_sample + SAMPLE_LOOP) so the note rings your actual voice — formants, texture and all — for as long as you hold a key. The loop region defaults to the middle but you DRAG its two green markers on the waveform to pick exactly which slice of your recording sustains (the sounding note re-loops live as you drag). The keybed (keybed.h: touch + mouse + QWERTY A-K + MIDI) plays it across two octaves, polyphonically: press a chord and it's three of your own vowels in harmony. The note you sang plays at original pitch; higher keys pitch it up. The four slot buttons pick the voice to play AND the next capture target, so you can bank an 'aaah', an 'eee', an 'ooo' and switch. The buffer is plain PCM DATA (deterministic, saveable), the clean side of the doctrine — only the act of singing is live. Not the vocoder (voxbox) or a pitch-follower (humtheremin): this freezes your VOICE into a playable, loopable instrument.",
    "controls": "REC button: open the mic (first tap), then hold a vowel to capture the current slot. Drag the two GREEN markers on the waveform to set the loop start/end. Slot buttons 1-4: pick the voice to play + capture into. A-K / click / touch / MIDI: play the keybed. Z/X: octave down/up."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "keybed.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// SING SYNTH — voice sampler. One REC button: tap, hold a vowel; the engine LOOPS the sustained
// middle as an INSTR_SAMPLE voice, and keybed.h plays YOUR voice polyphonically across the keys.
// Looping real PCM (not a single wave_set cycle) is what keeps a voice sounding like a voice.
// Capture-then-freeze: the buffer is deterministic data (see de:meta).

#define REC_SECS  1.2f
#define BUFMAX    (44100 * 2)
#define NSLOT     4                // 4 vowels; sample slots 0..3, instrument slots 10..13
#define ISLOT     10               // first INSTR_SAMPLE instrument slot
#define DEF_A     0.34f            // default loop region — draggable per slot (grab the green marks)
#define DEF_B     0.68f
// layout
#define WBX 4
#define WBY 22
#define WBW (SCREEN_W - 8)
#define WBH 26
#define SB_Y 52
#define SB_H 15
#define TOP_H 72                   // keybed fills below this

enum { ST_OFF, ST_WAIT, ST_REC, ST_PLAY };

static const char *NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

static int   state = ST_OFF;
static int   cur   = 0;
static int   filled[NSLOT];
static int   root_midi[NSLOT];
static float loop_a[NSLOT], loop_b[NSLOT];   // per-slot loop region (fractions 0..1), draggable
static int   grabbed = -1;         // dragging a loop marker: 0 = start, 1 = end, -1 = none
static int   pending_rec = 0;      // waiting on mic permission, then auto-record

static float buf[BUFMAX];
static float cap_best_lvl = 0.0f;
static float cap_hz = 0.0f;
static int   rec_frames = 0;
static int   fail = 0;

static float wf_lo[SCREEN_W], wf_hi[SCREEN_W];

static void note_name(int m, char *b, int cap) {
    snprintf(b, cap, "%s%d", NOTES[((m % 12) + 12) % 12], m / 12 - 1);
}

static void begin_record(void) {
    note_off_all();
    cap_best_lvl = 0.0f; cap_hz = 0.0f; rec_frames = 0; fail = 0;
    mic_record(REC_SECS);
    state = ST_REC;
}

// the single REC button's action: first tap opens the mic (permission), then auto-records once live;
// later taps record straight away.
static void rec_pressed(void) {
    if (state == ST_REC) return;
    if (!mic_active()) { mic_start(); pending_rec = 1; if (state == ST_OFF) state = ST_WAIT; }
    else begin_record();
}

static void finish_capture(void) {
    int sr = mic_record_rate(); if (sr < 8000) sr = 44100;
    int n  = mic_record_read(buf, BUFMAX);
    if (n < sr / 4) { fail = 1; state = ST_PLAY; return; }

    float pk = 0.0f; for (int i = 0; i < n; i++) { float a = fabsf(buf[i]); if (a > pk) pk = a; }
    if (pk < 0.02f) { fail = 1; state = ST_PLAY; return; }
    float g = 0.95f / pk; for (int i = 0; i < n; i++) buf[i] *= g;

    int root = (cap_hz > 60.0f && cap_hz < 1000.0f)
             ? (int)lroundf(69.0f + 12.0f * log2f(cap_hz / 440.0f)) : 60;

    sample_load(cur, buf, n);
    loop_a[cur] = DEF_A; loop_b[cur] = DEF_B;                // fresh take → reset the loop to the middle
    instrument(ISLOT + cur, INSTR_SAMPLE, 6, 0, 7, 160);
    instrument_sample(ISLOT + cur, cur, root);
    instrument_sample_region(ISLOT + cur, loop_a[cur], loop_b[cur]);
    instrument_sample_mode(ISLOT + cur, SAMPLE_LOOP);
    filled[cur] = 1; root_midi[cur] = root; fail = 0;
    state = ST_PLAY;
}

static void select_slot(int s) {
    cur = s;
    keybed_config(ISLOT + cur, 4, 14);
    keybed_layout(0, TOP_H, SCREEN_W, SCREEN_H - TOP_H);
}

// drag the loop start/end markers on the waveform — the sustaining loop follows live
static void edit_markers(void) {
    int mx = mouse_x(), my = mouse_y();
    int inpanel = (mx >= WBX && mx <= WBX + WBW && my >= WBY - 4 && my <= WBY + WBH + 4);
    if (mouse_pressed(MOUSE_LEFT) && inpanel && grabbed < 0) {
        int ax = WBX + (int)(loop_a[cur] * WBW), bx = WBX + (int)(loop_b[cur] * WBW);
        int da = mx > ax ? mx - ax : ax - mx, db = mx > bx ? mx - bx : bx - mx;
        grabbed = (da <= db) ? 0 : 1;                       // grab the nearer marker
    }
    if (grabbed >= 0) {
        float f = (mx - WBX) / (float)WBW;
        if (f < 0) f = 0; if (f > 1) f = 1;
        if (grabbed == 0) { if (f > loop_b[cur] - 0.05f) f = loop_b[cur] - 0.05f; loop_a[cur] = f; }
        else              { if (f < loop_a[cur] + 0.05f) f = loop_a[cur] + 0.05f; loop_b[cur] = f; }
        instrument_sample_region(ISLOT + cur, loop_a[cur], loop_b[cur]);   // LIVE — held notes re-loop
        if (!mouse_down(MOUSE_LEFT)) grabbed = -1;
    }
}

void init(void) {
    for (int i = 0; i < NSLOT; i++) { filled[i] = 0; root_midi[i] = 60; loop_a[i] = DEF_A; loop_b[i] = DEF_B; }
    select_slot(0);
}

void update(void) {
    if (pending_rec && mic_active()) { pending_rec = 0; begin_record(); }

    if (state == ST_REC) {
        float lvl = mic_level(), hz = mic_pitch();
        if (lvl > cap_best_lvl && hz > 60.0f && hz < 1000.0f) { cap_best_lvl = lvl; cap_hz = hz; }
        rec_frames++;
        if (rec_frames > 3 && !mic_recording()) finish_capture();
    }
    else if (state == ST_PLAY && filled[cur]) {
        edit_markers();
        keybed_update();
    }
}

static void draw_wave_big(int slot) {
    int mid = WBY + WBH / 2, amp = WBH / 2 - 2;
    int la = WBX + (int)(loop_a[slot] * WBW), lb = WBX + (int)(loop_b[slot] * WBW);
    rectfill(la, WBY, lb - la, WBH, CLR_DARKER_BLUE);          // lit loop region
    rect(WBX, WBY, WBW, WBH, CLR_DARK_GREY);
    for (int x = WBX; x < WBX + WBW; x += 6) pset(x, mid, CLR_DARK_BLUE);
    if (filled[slot]) {
        int len = sample_peaks(slot, wf_lo, wf_hi, WBW);
        if (len > 0) for (int i = 0; i < WBW; i++) {
            int x = WBX + i, inloop = (x >= la && x <= lb);
            line(x, mid - (int)(wf_hi[i] * amp), x, mid - (int)(wf_lo[i] * amp),
                 inloop ? CLR_ORANGE : CLR_INDIGO);
        }
        line(la, WBY, la, WBY + WBH, CLR_GREEN); rectfill(la - 2, WBY, 5, 4, CLR_GREEN);   // grab handles
        line(lb, WBY, lb, WBY + WBH, CLR_GREEN); rectfill(lb - 3, WBY, 5, 4, CLR_GREEN);
        char nm[8]; note_name(root_midi[slot], nm, sizeof nm);
        char b[28]; snprintf(b, sizeof b, "drag loop \x07 root %s", nm);
        print(b, WBX + 3, WBY + WBH - 8, CLR_GREEN);
    } else {
        print("hold a vowel + tap REC", WBX + 6, mid - 3, CLR_MEDIUM_GREY);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();

    print("SING SYNTH", 6, 7, CLR_ORANGE);

    // REC button (top-right) — or, mid-take, a progress bar in its place
    int bx = SCREEN_W - 66, by = 3, bw = 62, bh = 16;
    if (state == ST_REC) {
        rect(bx, by, bw, bh, CLR_RED);
        int pw = (int)(mic_record_progress() * (bw - 2));
        rectfill(bx + 1, by + 1, pw, bh - 2, CLR_RED);
        print("REC", bx + bw / 2 - 12, by + 5, CLR_WHITE);
    } else {
        if (ui_button(bx, by, bw, bh, mic_active() ? "\x07 REC" : "ENABLE")) rec_pressed();
    }

    // waveform (or, mid-take, a live level meter)
    if (state == ST_REC) {
        float lvl = mic_level(); if (lvl > 1) lvl = 1;
        rect(WBX, WBY, WBW, WBH, CLR_DARK_GREY);
        rectfill(WBX + 2, WBY + WBH / 2 - 4, (int)(lvl * (WBW - 4)), 8, CLR_ORANGE);
        print("listening — hold the vowel", WBX + 4, WBY + 2, CLR_RED);
    } else {
        draw_wave_big(cur);
    }

    // slot buttons (tappable) — pick the voice to play + capture into
    int w = 42, gap = 6, total = NSLOT * w + (NSLOT - 1) * gap, x0 = (SCREEN_W - total) / 2;
    for (int s = 0; s < NSLOT; s++) {
        int x = x0 + s * (w + gap);
        char lab[8]; snprintf(lab, sizeof lab, "%d%s", s + 1, filled[s] ? " \x07" : "");
        if (ui_button(x, SB_Y, w, SB_H, lab) && state != ST_REC) select_slot(s);
        if (s == cur) rect(x - 2, SB_Y - 2, w + 4, SB_H + 4, CLR_WHITE);
    }

    // keybed (once the current slot has a voice)
    if (state != ST_REC) {
        if (filled[cur]) keybed_draw();
        else print("capture a vowel to play it here", 6, TOP_H + 12, CLR_MEDIUM_GREY);
    }

    if (fail) print("nothing caught — hold a louder, steadier vowel", 6, SCREEN_H - 12, CLR_RED);

    ui_end();
}
