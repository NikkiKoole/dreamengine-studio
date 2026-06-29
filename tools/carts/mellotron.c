/* de:meta
{
  "title": "mellotron",
  "status": "active",
  "created": "2026-06-11",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "additive-synth",
    "analog-voice-modeling"
  ],
  "lineage": "Emulates the 1963 Mellotron M400 without sample playback — reconstructs tape-onset, ensemble shimmer, and the finite-tape quirk via additive wave_set stacking, pitch mod-envelope, chorus, and a per-note tape-drain timer.",
  "homage": "Mellotron M400 (1970)",
  "description": "The 1963 tape-replay keyboard, faked in pure synthesis. A real Mellotron has NO oscillators - each key drags a strip of pre-recorded tape (a real strings/choir/flute/brass section) across a playback head. We have no sample playback (code-first synthesis, on purpose), so the cart reconstructs the CUES the ear uses to believe a recording: each VOICE is a drawn single-cycle wave (harmonics stacked via wave_set into INSTR_USER0, the tapeloop trick); every key-press makes a soft airy breath of noise as the tape-head engages and the note settles UP to pitch over ~90ms (the capstan spinning to speed - the real Mellotron onset, modelled with a pitch mod-envelope rather than a synthy click); chorus() turns one wave into an ENSEMBLE; a gentle vibrato + slow filter drift keep the timbre from freezing; then tape() (wow/flutter/saturation) and reverb() add the worn tape and the room. The headline is the signature Mellotron quirk: THE ~8-SECOND TAPE LIMIT - each key's tape is finite, so a held note runs out and stops on its own even with the key still down (lift and re-press to rewind). Each held key shows its tape draining from the top in the voice colour, then goes dark when it runs out. LAYERING - a real Mellotron tape frame holds three tracks, so you can blend them: tap the VOICE buttons (STRINGS / CHOIR / FLUTE / BRASS) to stack timbres into the loaded tape (the drawn single-cycle waves are summed - additive, still one voice per note), or hit a COMBI preset (STR+CH, STR+FL, CHO+FL, FULL). Dressed as an M400 cabinet: wooden end-cheeks, a cream control panel, a logo plate, the voice/combi selectors and TONE + TAPE (loop length 2-8s) knobs. Hold keys to play - computer keyboard uses the GarageBand musical-typing map (white keys on the home row A S D F G H J K L ; ' with A = C, black keys on the row above W E T Y U O P - all labelled on the keys; Z / X shift the octave), or mouse = one finger, or touch = chords up to ~5 fingers and slide for glissando."
}
de:meta */
#include "studio.h"
#include "ui.h"
#define KEYBED_WHITE_KEYS "ASDFGHJKL;'"   // GarageBand musical-typing: home row = whites (A = C)
#define KEYBED_BLACK_KEYS "WE TYU OP"     // the row above = blacks
#include "keybed.h"
#include <math.h>
#include <stdio.h>

// ── MELLOTRON ────────────────────────────────────────────────────────────────
// The real thing IS tape: every key drags a strip of pre-recorded tape across a
// playback head — a real strings/choir/flute/brass section, captured once in 1963.
// We have NO sample playback (code-first synthesis, on purpose), so we FAKE the
// recording: each VOICE is a drawn single-cycle wave (a stack of harmonics via
// wave_set -> INSTR_USER0, the tapeloop trick), and we reconstruct the *cues* the
// ear uses to believe a recording —
//   * THE TAPE ENGAGE   a soft breath of noise + a pitch that settles UP to the note
//                        over ~90 ms (the capstan spinning to speed — not a click)
//   * ENSEMBLE shimmer   chorus() turns one wave into a whole section
//   * MOVEMENT           gentle vibrato + a warm lowpass so it isn't frozen
//   * TAPE + ROOM        tape() wow/flutter/saturation, then reverb()
// and the signature quirk that makes a Mellotron a Mellotron:
//   * THE ~8s TAPE LIMIT each key's tape is FINITE — hold a note and it runs out
//     and stops, even with the key still down. Lift + re-press rewinds the tape.
// LAYERING — a real Mellotron tape frame holds 3 tracks; blend them. Tap the VOICE
// buttons to stack timbres (we SUM the drawn waves into one cycle — additive, one
// voice per note), or hit a COMBI preset. (design: docs/design/recorded-timbres.md.)
//
// Play with the COMPUTER KEYBOARD (GarageBand musical-typing map): white keys on
// the home row A S D F G H J K L ; '  (A = C), black keys on the row above
// W E T Y U O P. Z / X shift the octave. Or use the mouse / multitouch keybed.

#define WOOD     12                       // the wooden end-cheeks of the cabinet
#define KX0      WOOD
#define NWHITE   14                        // two octaves of white keys
#define WKW      ((SCREEN_W - 2 * WOOD) / NWHITE)
#define KEY_TOP  72
#define BLACK_W  14
#define BLACK_H  76

#define SLOT   5                           // the "tape recording" — INSTR_USER0
#define CHIFF  6                           // the attack transient — INSTR_NOISE

// keybed.h owns the layout, per-finger capture, glissando, QWERTY, MIDI + octave.
// This cart keeps only the *tape* lifecycle on top of it (chiff onset, the 8s limit).
// Per-note tape state, keyed by MIDI note (keybed's voice identity).
static float s_start[128];                 // now() when this note's tape started rolling
static bool  s_spent[128];                 // tape ran out — silent until the key lifts

// ── the four drawn "recordings" + the active LAYER blend ──
static const char *VNAME[4] = { "STRINGS", "CHOIR", "FLUTE", "BRASS" };
static const int   VCLR [4] = { CLR_BLUE, CLR_PINK, CLR_GREEN, CLR_ORANGE };
static float TBL[4][64];
static float ACT[64];
static int   layer = 1;                    // bitmask of active voices (default = STRINGS)

static const struct { const char *name; int mask; } COMBI[4] = {
    { "STR+CH", 1 | 2 }, { "STR+FL", 1 | 4 }, { "CHO+FL", 2 | 4 }, { "FULL", 1 | 2 | 4 | 8 },
};

static float k_tone = 0.55f;               // TONE knob -> lowpass cutoff
static float k_len  = 1.00f;               // TAPE knob -> loop length (default = 8s)

static float tape_len(void) { return 2.0f + k_len * 6.0f; }     // 2..8 seconds
static int   tone_hz (void) { return 700 + (int)(k_tone * 3300); }  // 700..4000 Hz
static int   first_voice(void) { for (int v = 0; v < 4; v++) if (layer & (1 << v)) return v; return 0; }

static void build_waves(void) {
    for (int i = 0; i < 64; i++) {
        float x = i / 64.0f * 6.2831853f;
        // STRINGS — dense bowed-ensemble harmonics, rolled-off top
        TBL[0][i] = 0.60f*sinf(x) + 0.42f*sinf(2*x) + 0.30f*sinf(3*x) + 0.20f*sinf(4*x)
                  + 0.14f*sinf(5*x) + 0.10f*sinf(6*x) + 0.06f*sinf(7*x);
        // CHOIR — vocal "aah": soft fundamental + a formant bump around the 3rd-4th harmonic
        TBL[1][i] = 0.45f*sinf(x) + 0.28f*sinf(2*x) + 0.40f*sinf(3*x) + 0.42f*sinf(4*x)
                  + 0.22f*sinf(5*x) + 0.10f*sinf(6*x);
        // FLUTE — nearly pure, a touch of 2nd/3rd (breath comes from the chiff + lowpass)
        TBL[2][i] = 0.88f*sinf(x) + 0.16f*sinf(2*x) + 0.07f*sinf(3*x);
        // BRASS — bright sawtooth-ish stack, a formant lift in the upper mids (the blare)
        TBL[3][i] = 0.50f*sinf(x) + 0.40f*sinf(2*x) + 0.34f*sinf(3*x) + 0.30f*sinf(4*x)
                  + 0.26f*sinf(5*x) + 0.20f*sinf(6*x) + 0.14f*sinf(7*x) + 0.10f*sinf(8*x);
    }
}

// blend the active voices into ONE drawn cycle (additive layering) and load it live
static void build_active_wave(void) {
    float peak = 0.0001f;
    for (int i = 0; i < 64; i++) {
        float s = 0;
        for (int v = 0; v < 4; v++) if (layer & (1 << v)) s += TBL[v][i];
        ACT[i] = s;
        float a = s < 0 ? -s : s;
        if (a > peak) peak = a;
    }
    float g = 0.95f / peak;                            // normalize so a blend isn't louder
    for (int i = 0; i < 64; i++) ACT[i] *= g;
    wave_set(0, ACT, 64);
}

void kb_prep(int midi, int vel);   // the per-note tape onset (defined below)

void init(void) {
    build_waves();
    keybed_config(SLOT, 4, NWHITE);                    // base C4, two octaves of white keys
    keybed_layout(KX0, KEY_TOP, NWHITE * WKW, SCREEN_H - KEY_TOP);
    keybed_on_note(kb_prep);                           // chiff onset + start the tape clock at each note-on

    // the sustaining tape voice: a soft swelling pad (bloom in, long release out)
    instrument(SLOT, INSTR_USER0, 140, 0, 6, 600);
    instrument_filter(SLOT, FILTER_LOW, tone_hz(), 2);     // warm; TONE knob sweeps this
    instrument_lfo(SLOT, 0, LFO_PITCH, 5.2f, 0.10f);       // gentle player vibrato
    instrument_lfo(SLOT, 1, LFO_CUTOFF, 0.30f, 220.0f);    // slow drift so the timbre isn't frozen
    // the capstan spin-up: each note starts ~18 cents FLAT and settles up to pitch over
    // ~90 ms — the tape getting up to speed. The real Mellotron onset (not a click).
    instrument_env(SLOT, 2, ENV_PITCH, 0, 90, -0.18f);
    instrument_reverb(SLOT, 0.28f);                        // the room it was recorded in
    build_active_wave();

    // the attack "engage": a soft, airy breath of noise (the tape-head pressing on),
    // lowpassed dark so it reads as mechanism, not a percussive click.
    instrument(CHIFF, INSTR_NOISE, 4, 45, 0, 30);
    instrument_filter(CHIFF, FILTER_LOW, 1200, 1);

    chorus(0.55f, 0.50f, 0.45f);           // ENSEMBLE — one wave reads as a section
    reverb(0.55f, 0.45f);
    tape(0.65f, 0.45f, 0.50f);             // worn-tape wobble (wow / flutter / saturation) — pronounced
}

// keybed.h fires this just BEFORE the sustain note_on, for ANY source (touch/QWERTY/MIDI):
// roll a fresh tape — the chiff onset + reset this note's tape clock. (keybed owns the
// sustain voice itself; lifting + re-pressing re-fires this, which "rewinds" the tape.)
void kb_prep(int midi, int vel) {
    (void)vel;
    hit(midi, CHIFF, 2, 36);                        // soft breath as the tape-head engages
    s_start[midi] = now();
    s_spent[midi] = false;
}

void update(void) {
    keybed_update();          // touch + glissando + QWERTY (GarageBand map) + MIDI + Z/X octave

    // THE 8-SECOND TAPE LIMIT — a held note runs out of tape and stops on its own. We kill
    // keybed's voice early but leave the key logically HELD (keybed's held-state is the latch),
    // so it stays silent until you lift + re-press, which re-fires kb_prep and rewinds the tape.
    float len = tape_len();
    for (int m = 0; m < 128; m++)
        if (keybed_held(m) && !s_spent[m] && now() - s_start[m] >= len) {
            note_off(keybed_handle(m));
            s_spent[m] = true;
        }
}

static void set_tone(void) {
    int hz = tone_hz();
    instrument_filter(SLOT, FILTER_LOW, hz, 2);
    for (int m = 0; m < 128; m++)                   // sweep the ringing notes too
        if (keybed_held(m) && !s_spent[m]) note_cutoff(keybed_handle(m), hz);
}

// draw one key with its remaining-tape fill (drains from the top as tape is used)
static void draw_key(int x, int y, int w, int h, int midi, char label, bool black) {
    bool held = keybed_held(midi);
    int base = black ? CLR_DARK_GREY : CLR_WHITE;
    if (!held) {
        rectfill(x, y, w, h, base);
    } else if (s_spent[midi]) {
        rectfill(x, y, w, h, CLR_BROWNISH_BLACK);   // tape ran out
    } else {
        float used = (now() - s_start[midi]) / tape_len();
        if (used < 0) used = 0; if (used > 1) used = 1;
        rectfill(x, y, w, h, VCLR[first_voice()]);  // remaining tape = the (lead) voice colour
        rectfill(x, y, w, (int)(used * h), CLR_BROWNISH_BLACK);  // used tape spools away
    }
    rect(x, y, w, h, CLR_DARK_BROWN);
    if (label) {                                    // the computer key that plays this note
        char t[2] = { label, 0 };
        int col = held ? CLR_LIGHT_PEACH : (black ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY);
        print(t, x + (w - text_width(t)) / 2, y + h - (black ? 11 : 20), col);
    }
}

void draw(void) {
    cls(CLR_DARK_BROWN);                            // the wooden cabinet

    // ── wooden end-cheeks ──
    rectfill(0, 0, WOOD, SCREEN_H, CLR_BROWN);
    rectfill(SCREEN_W - WOOD, 0, WOOD, SCREEN_H, CLR_BROWN);
    line(WOOD - 1, 0, WOOD - 1, SCREEN_H, CLR_BROWNISH_BLACK);
    line(SCREEN_W - WOOD, 0, SCREEN_W - WOOD, SCREEN_H, CLR_BROWNISH_BLACK);

    // ── cream control panel ──
    rectfill(WOOD, 0, SCREEN_W - 2 * WOOD, KEY_TOP, CLR_LIGHT_PEACH);
    line(WOOD, KEY_TOP - 1, SCREEN_W - WOOD, KEY_TOP - 1, CLR_DARK_PEACH);

    // logo plate
    rectfill(16, 4, 92, 12, CLR_BROWNISH_BLACK);
    print("MELLOTRON", 20, 6, CLR_LIGHT_PEACH);

    ui_begin();
    // ── VOICE toggles (row 1) — tap to stack timbres into the blend ──
    static const int vx[4] = { 16, 72, 120, 168 }, vw[4] = { 52, 44, 44, 44 };
    for (int i = 0; i < 4; i++) {
        if (ui_button(vx[i], 18, vw[i], 14, VNAME[i])) {
            layer ^= (1 << i);
            if (layer == 0) layer = (1 << i);       // never empty
            build_active_wave();
        }
        if (layer & (1 << i)) rectfill(vx[i], 33, vw[i], 2, VCLR[i]);  // active marker
    }
    // ── COMBI presets (row 2) ──
    static const int cx[4] = { 16, 70, 124, 178 }, cw[4] = { 50, 50, 50, 40 };
    for (int i = 0; i < 4; i++)
        if (ui_button(cx[i], 38, cw[i], 13, COMBI[i].name)) { layer = COMBI[i].mask; build_active_wave(); }

    // ── TONE / TAPE knobs (top-right) ──
    font(FONT_SMALL);
    if (ui_knob(&k_tone, 256, 18, "TONE")) set_tone();
    ui_knob(&k_len, 292, 18, "TAPE");
    char buf[16];
    snprintf(buf, sizeof buf, "%.0fs", tape_len());
    print(buf, 292 - text_width(buf) / 2, 38, CLR_DARK_BROWN);
    print("A S D F.. = keys   Z X = octave", 16, 60, CLR_DARK_PEACH);
    font(FONT_NORMAL);
    ui_end();

    // ── keyboard (keybed.h owns layout/voices; we draw the tape-drain fill) ──
    int nw = keybed_white_count();
    for (int k = 0; k < nw; k++) {                  // white keys
        int x, y, w, h; keybed_white_rect(k, &x, &y, &w, &h);
        char label = (k < (int)sizeof(KEYBED_WHITE_KEYS) - 1) ? KEYBED_WHITE_KEYS[k] : 0;
        draw_key(x + 1, y, w - 1, h, keybed_white_midi(k), label, false);
        if (k % 7 == 0) {
            char lbl[5]; snprintf(lbl, sizeof lbl, "C%d", keybed_octave() + k / 7);
            print(lbl, x + 4, SCREEN_H - 11, CLR_MEDIUM_GREY);
        }
    }
    for (int k = 0; k < nw; k++) {                  // black keys
        int x, y, w, h, midi; if (!keybed_black_rect(k, &x, &y, &w, &h, &midi)) continue;
        char label = (k < (int)sizeof(KEYBED_BLACK_KEYS) - 1 && KEYBED_BLACK_KEYS[k] != ' ') ? KEYBED_BLACK_KEYS[k] : 0;
        draw_key(x, y, w, h, midi, label, true);
    }
}
