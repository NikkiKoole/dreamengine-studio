// keybed.h — a polyphonic chromatic keyboard, cart-land.
//
// Why this exists (docs/design/midi-and-keybed.md): ~38 sound carts hand-roll
// the same keybed — white/black key tables, a per-finger touch pool, glissando,
// QWERTY rows, and note_on/note_off bookkeeping (epiano, moog, touchpiano,
// mellotron all carry ~120 near-identical lines of it). This header owns that
// once, and folds in a plugged-in MIDI keyboard (engine midi_get()) as just
// another note source — so a cart that #includes it gets touch + mouse + QWERTY
// + MIDI, with chords, glissando, and velocity, for free.
//
// Voices are keyed by absolute MIDI note (0..127), which is what makes MIDI
// input trivial and lets every source coexist. Engine deliberately does NOT own
// this (decision-0006 lane); it sits BESIDE solo.h (the radio scale-locked
// strip) and is NOT for continuous-pitch ribbons (martenot/heldnotes) — those
// share only the engine-level midi_get() layer, not this.
//
// Usage — minimal (the built-in drawn manual draws AND hit-tests):
//   #include "keybed.h"
//   void init(void){ instrument(5, INSTR_SAW, 8, 140, 6, 260);
//                     keybed_config(5, 4, 2);                 // slot 5, base octave C4, 2 octaves
//                     keybed_layout(0, 56, SCREEN_W, SCREEN_H - 56); }
//   void update(void){ keybed_update(); }                    // drains touch + QWERTY + MIDI
//   void draw(void){ cls(CLR_BLACK); keybed_draw(); }
//
// Usage — custom-drawn keys (cart owns the look, e.g. epiano): keybed_config()
//   + keybed_layout() + keybed_update() as above, but draw your own keys —
//   iterate with keybed_white_count() / keybed_white_rect() / keybed_black_rect()
//   and colour them from keybed_held(midi) / keybed_glow(midi).
//
// Tunables (#define before the include):
//   KEYBED_OCT_DOWN / KEYBED_OCT_UP   QWERTY octave-shift keys (default Z / X; 0 = off)
//   KEYBED_WHITE_KEYS / KEYBED_BLACK_KEYS   QWERTY layout (default GarageBand musical typing)

#ifndef DE_KEYBED_H
#define DE_KEYBED_H

#include <stdint.h>
#include <stdbool.h>

#ifndef KEYBED_OCT_DOWN
#define KEYBED_OCT_DOWN 'Z'
#endif
#ifndef KEYBED_OCT_UP
#define KEYBED_OCT_UP 'X'
#endif
// home row = white keys (left→right), the QWERTY row above = black keys (gaps where
// a piano has none). GarageBand "musical typing" map; covers ~2 octaves of whites.
#ifndef KEYBED_WHITE_KEYS
#define KEYBED_WHITE_KEYS "ASDFGHJKL"
#endif
#ifndef KEYBED_BLACK_KEYS
#define KEYBED_BLACK_KEYS "WE TYU OP"   // aligned to whites: C# D# (gap) F# G# A# (gap) C# D#
#endif

// the piano pattern within an octave
static const int KB_WSEMI[7] = { 0, 2, 4, 5, 7, 9, 11 };       // white-key semitones
static const int KB_BSEMI[7] = { 1, 3, -1, 6, 8, 10, -1 };     // black key right of white i (-1 = none)

#define KB_TOUCH  1
#define KB_QWERTY 2
#define KB_MIDI   4

static int      kb_slot      = 5;
static int      kb_base_oct  = 4;      // leftmost octave (4 → C4 = MIDI 60)
static int      kb_nwhite    = 14;     // number of white keys (7 = one octave; epiano 8, moog 11, 2-oct 14)
static int      kb_vel       = 6;      // 0..7 velocity for touch/QWERTY (MIDI carries its own)

static int      kb_handle[128];        // live voice handle per MIDI note, -1 = silent
static uint8_t  kb_src[128];           // bitmask of sources holding each note
static float    kb_glow[128];          // 1.0 on press, decays in keybed_draw / read it yourself

// per-finger key capture (multitouch + glissando)
#define KB_NPTR 12
typedef struct { int id, midi; } KbPtr;   // midi = key this finger currently holds, -1 = off-keys
static KbPtr    kb_ptr[KB_NPTR];
static int      kb_prev_ids[KB_NPTR];     // touch ids seen last frame (for new-finger detection)
static int      kb_nprev = 0;

// drawn-manual geometry (set by keybed_layout; keybed_draw reuses it)
static int      kb_x = 0, kb_y = 0, kb_w = 0, kb_h = 0;

// ── config ──
static void keybed_config(int slot, int base_octave, int nwhite) {
    kb_slot = slot; kb_base_oct = base_octave; kb_nwhite = nwhite < 1 ? 1 : nwhite;
    for (int m = 0; m < 128; m++) { kb_handle[m] = -1; kb_src[m] = 0; kb_glow[m] = 0; }
    for (int i = 0; i < KB_NPTR; i++) { kb_ptr[i].id = -999; kb_ptr[i].midi = -1; }
}
static void keybed_layout(int x, int y, int w, int h) { kb_x = x; kb_y = y; kb_w = w; kb_h = h; }
static void keybed_velocity(int v) { kb_vel = v < 0 ? 0 : v > 7 ? 7 : v; }

static int keybed_white_count(void) { return kb_nwhite; }
static int keybed_base_midi(void)   { return (kb_base_oct + 1) * 12; }
static int keybed_octave(void)      { return kb_base_oct; }   // current leftmost octave (for display)
// a black key sits after white i only if i isn't the last white (needs a white to its right)
static bool kb_has_black(int i) { return i >= 0 && i < kb_nwhite - 1 && KB_BSEMI[i % 7] >= 0; }

static bool keybed_held(int midi) { return midi >= 0 && midi < 128 && kb_src[midi] != 0; }
static float keybed_glow(int midi) { return (midi >= 0 && midi < 128) ? kb_glow[midi] : 0.0f; }
static int  keybed_handle(int midi) { return (midi >= 0 && midi < 128) ? kb_handle[midi] : -1; }  // live voice handle, or -1 — for per-note modulation

// ── voice management (refcounted across sources so they never stomp) ──
static void kb_press(int midi, int src, int vel) {
    if (midi < 0 || midi > 127) return;
    if (kb_src[midi] == 0) { kb_handle[midi] = note_on(midi, kb_slot, vel); kb_glow[midi] = 1.0f; }
    kb_src[midi] |= src;
}
static void kb_release(int midi, int src) {
    if (midi < 0 || midi > 127 || !(kb_src[midi] & src)) return;
    kb_src[midi] &= ~src;
    if (kb_src[midi] == 0 && kb_handle[midi] >= 0) { note_off(kb_handle[midi]); kb_handle[midi] = -1; }
}
// release everything (octave shift / panic) — keeps sources consistent
static void keybed_all_off(void) {
    for (int m = 0; m < 128; m++) if (kb_src[m]) { if (kb_handle[m] >= 0) note_off(kb_handle[m]); kb_handle[m] = -1; kb_src[m] = 0; }
    for (int i = 0; i < KB_NPTR; i++) kb_ptr[i].midi = -1;
}
static void keybed_octave_shift(int d) {
    int n = kb_base_oct + d;
    if (n < 0 || n > 9) return;
    keybed_all_off();
    kb_base_oct = n;
}

// ── geometry: which MIDI note is under canvas point (x,y)? -1 = none ──
static int keybed_midi_at(int px, int py) {
    if (kb_w <= 0 || kb_h <= 0) return -1;
    if (px < kb_x || px >= kb_x + kb_w || py < kb_y || py >= kb_y + kb_h) return -1;
    int nwhite = keybed_white_count();
    int wkw = kb_w / nwhite; if (wkw < 1) wkw = 1;
    int blackH = kb_h * 60 / 100, blackW = wkw * 7 / 12; if (blackW < 1) blackW = 1;
    int rel = px - kb_x;
    int wk = rel / wkw; if (wk < 0) wk = 0; if (wk >= nwhite) wk = nwhite - 1;
    if (py - kb_y < blackH) {                              // black row: test this white + its left neighbour
        for (int k = wk - 1; k <= wk; k++) {
            if (!kb_has_black(k)) continue;
            int bx = kb_x + (k + 1) * wkw - blackW / 2;     // straddles the white-key boundary
            if (px >= bx && px < bx + blackW) return keybed_base_midi() + (k / 7) * 12 + KB_BSEMI[k % 7];
        }
    }
    return keybed_base_midi() + (wk / 7) * 12 + KB_WSEMI[wk % 7];
}

// rect of white key i (0..white_count-1) — for custom drawing
static void keybed_white_rect(int i, int *x, int *y, int *w, int *h) {
    int nwhite = keybed_white_count(); int wkw = kb_w / nwhite;
    if (x) *x = kb_x + i * wkw; if (y) *y = kb_y; if (w) *w = wkw; if (h) *h = kb_h;
}
static int keybed_white_midi(int i) { return keybed_base_midi() + (i / 7) * 12 + KB_WSEMI[i % 7]; }
// black key right of white i, or returns false if none
static bool keybed_black_rect(int i, int *x, int *y, int *w, int *h, int *midi) {
    int nwhite = keybed_white_count(); if (!kb_has_black(i)) return false;
    int wkw = kb_w / nwhite, blackH = kb_h * 60 / 100, blackW = wkw * 7 / 12;
    if (x) *x = kb_x + (i + 1) * wkw - blackW / 2; if (y) *y = kb_y; if (w) *w = blackW; if (h) *h = blackH;
    if (midi) *midi = keybed_base_midi() + (i / 7) * 12 + KB_BSEMI[i % 7];
    return true;
}

// ── input ──
static bool kb_id_was_prev(int id) { for (int i = 0; i < kb_nprev; i++) if (kb_prev_ids[i] == id) return true; return false; }
static KbPtr *kb_find_ptr(int id) { for (int i = 0; i < KB_NPTR; i++) if (kb_ptr[i].id == id) return &kb_ptr[i]; return 0; }
static bool kb_note_touch_free(int midi) { return midi >= 0 && midi < 128 && !(kb_src[midi] & KB_TOUCH); }

// QWERTY note for the n-th white/black char of the layout strings
static int kb_qwerty_midi(char c) {
    const char *W = KEYBED_WHITE_KEYS, *B = KEYBED_BLACK_KEYS;
    for (int i = 0; W[i]; i++) if (W[i] == c) return keybed_base_midi() + (i / 7) * 12 + KB_WSEMI[i % 7];
    for (int i = 0; B[i]; i++) if (B[i] == c && B[i] != ' ' && KB_BSEMI[i % 7] >= 0)
        return keybed_base_midi() + (i / 7) * 12 + KB_BSEMI[i % 7];
    return -1;
}

// call once per frame (in update(), after keybed_layout has been set at least once)
static void keybed_update(void) {
    for (int k = 0; k < 128; k++) if (kb_glow[k] > 0) kb_glow[k] *= 0.88f;   // decay here so custom-draw carts get it too

    // 1. MIDI keyboard — absolute notes + velocity
    int mn, mv, t;
    while ((t = midi_get(&mn, &mv)) != 0) {
        if (t > 0) { int v = (mv * 7) / 127; kb_press(mn, KB_MIDI, v < 1 ? 1 : v); }
        else         kb_release(mn, KB_MIDI);
    }

    // 2. QWERTY — octave shift + the two key rows
    if (KEYBED_OCT_DOWN && keyp(KEYBED_OCT_DOWN)) keybed_octave_shift(-1);
    if (KEYBED_OCT_UP   && keyp(KEYBED_OCT_UP))   keybed_octave_shift(+1);
    for (const char *p = KEYBED_WHITE_KEYS; *p; p++) { if (keyp(*p)) kb_press(kb_qwerty_midi(*p), KB_QWERTY, kb_vel); if (keyr(*p)) kb_release(kb_qwerty_midi(*p), KB_QWERTY); }
    for (const char *p = KEYBED_BLACK_KEYS; *p; p++) { if (*p == ' ') continue; if (keyp(*p)) kb_press(kb_qwerty_midi(*p), KB_QWERTY, kb_vel); if (keyr(*p)) kb_release(kb_qwerty_midi(*p), KB_QWERTY); }

    // 3. touch / mouse — per-finger capture + glissando. A finger only CLAIMS a key
    //    on the frame it newly touches down over the keys (so a drag from a knob into
    //    the keys never starts a note); once owned, it glides across keys until lift.
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), midi = keybed_midi_at(touch_x(i), touch_y(i));
        KbPtr *p = kb_find_ptr(id);
        if (p) {
            if (midi != p->midi) {
                if (p->midi >= 0) kb_release(p->midi, KB_TOUCH);
                if (midi >= 0 && kb_note_touch_free(midi)) { kb_press(midi, KB_TOUCH, kb_vel); p->midi = midi; }
                else p->midi = -1;
            }
        } else if (!kb_id_was_prev(id) && midi >= 0 && kb_note_touch_free(midi)) {  // newly down over a key
            for (int j = 0; j < KB_NPTR; j++) if (kb_ptr[j].id == -999) { kb_ptr[j].id = id; kb_ptr[j].midi = midi; kb_press(midi, KB_TOUCH, kb_vel); break; }
        }
    }
    // lifted fingers → release their key + free the slot
    for (int i = 0; i < touch_ended_count(); i++) {
        int id = touch_ended_id(i);
        KbPtr *p = kb_find_ptr(id);
        if (p) { if (p->midi >= 0) kb_release(p->midi, KB_TOUCH); p->id = -999; p->midi = -1; }
    }

    // snapshot this frame's live ids for next frame's new-finger test
    kb_nprev = 0;
    for (int i = 0; i < touch_count() && kb_nprev < KB_NPTR; i++) kb_prev_ids[kb_nprev++] = touch_id(i);
}

// ── the built-in drawn manual (optional) ──
static void keybed_draw(void) {
    int nwhite = keybed_white_count();
    int wkw = kb_w / nwhite, blackH = kb_h * 60 / 100, blackW = wkw * 7 / 12;
    // white keys
    for (int k = 0; k < nwhite; k++) {
        int x = kb_x + k * wkw, midi = keybed_white_midi(k);
        bool down = keybed_held(midi);
        int col = down ? CLR_LIME_GREEN : kb_glow[midi] > 0.1f ? CLR_PEACH : CLR_WHITE;
        rectfill(x + 1, kb_y, wkw - 2, kb_h, col);
        rect(x, kb_y, wkw, kb_h, CLR_DARK_GREY);
        if (k % 7 == 0) print(str("C%d", kb_base_oct + k / 7), x + 3, kb_y + kb_h - 10, CLR_MEDIUM_GREY);
    }
    // black keys
    for (int k = 0; k < nwhite; k++) {
        if (!kb_has_black(k)) continue;
        int x = kb_x + (k + 1) * wkw - blackW / 2, midi = keybed_base_midi() + (k / 7) * 12 + KB_BSEMI[k % 7];
        bool down = keybed_held(midi);
        rectfill(x, kb_y, blackW, blackH, down ? CLR_ORANGE : kb_glow[midi] > 0.1f ? CLR_DARK_ORANGE : CLR_DARKER_GREY);
        rect(x, kb_y, blackW, blackH, CLR_BLACK);
    }
}

#endif // DE_KEYBED_H
