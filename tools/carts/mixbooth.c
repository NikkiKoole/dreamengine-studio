/* de:meta
{
  "title": "mixing booth",
  "status": "active",
  "created": "2026-07-02",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "per-instrument-fx-bus"
  ],
  "lineage": "docs/design/teaching-gaps.md Gap type 3 flagged 8 instrument_* per-slot effect variants as zero-cart -- undersold, not under-taught. groovebox.c owns the SUMMED-BUS family (many voices, one shared rack) and pedalboard.c/combo.c own the SERIAL-chain family (one guitar, many pedals); this is the third sibling -- the PER-INSTRUMENT family, where each voice gets its OWN private effect and the rest of the mix stays untouched. Also retrofits ampcab.h (previously only combo/pedalboard/afrobeat/wba) onto a sixth track, and clears strum_notes (an explicit voicing, not a CHORD_* type) on the guitar's re-strum.",
  "description": {
    "summary": "A tiny 6-track band loops itself so both hands are free to flip each track's OWN private effect on and off -- proof that instrument_tape()/instrument_gate()/instrument_ringmod()/instrument_univibe()/instrument_shallow()/instrument_grains_freeze()/instrument_grains_pitch()/instrument_fx_mod() touch only the one instrument slot they're given, never the rest of the mix.",
    "detail": "Six channel strips, one per track (DRUMS/BASS/LEAD/ORGAN/PAD/GTR), each wired to a different per-instrument effect nobody else in the library demos: DRUMS gets instrument_tape() (wow/flutter/saturation), BASS gets instrument_gate() (choppy noise-gate chop), LEAD gets instrument_ringmod() (a metallic carrier), ORGAN gets instrument_univibe() (the liquid photocell wobble), PAD cycles OFF -> WARP (instrument_shallow) -> FREEZE (instrument_grains_freeze + instrument_grains_pitch, a frozen granular drone), and GTR retrofits the underused ampcab.h guitar-amp bundle (5 voicings via instrument_drive/instrument_drive_mode/instrument_eq/glue) plus a BREATHE toggle that rides instrument_fx_mod(FXMOD_DRIVE) every frame for a slow gain pulse. The guitar re-strums each chord change via strum_notes() -- an explicit 3-note array, not a CHORD_* type. Each strip's AMOUNT knob controls its effect's main parameter; MUTE/SOLO isolate a track so the 'clean everywhere else' claim is audible, not just asserted. Every instrument_* call is SET-AND-HOLD -- it only fires on the button/knob that actually changed, never every frame.",
    "controls": "tap a track's switch to cycle its effect (off/on, or off/WARP/FREEZE for PAD); drag/wheel the AMOUNT knob under it; MUTE/SOLO isolate one track's effect"
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "ampcab.h"     // the shared amp/cab voicing table (also used by combo/pedalboard)
#include <math.h>

// MIXING BOOTH -- the per-instrument-effect family's showcase (docs/design/teaching-gaps.md
// Gap type 3). groovebox.c already owns the SUMMED-BUS effects (one rack, many voices) and
// pedalboard.c/combo.c own the SERIAL-chain family (one guitar, a pedal chain); the missing
// third sibling is the PER-INSTRUMENT family -- instrument_X(slot, ...) applies an effect to
// ONE voice's own private FX bus while everything else stays dry. A tiny band plays itself so
// both hands are free to flip each track's switch and hear the isolation directly.
//
//   DRUMS  instrument_tape()          -- wow/flutter/saturation, this track only
//   BASS   instrument_gate()          -- choke the tail between hits, this track only
//   LEAD   instrument_ringmod()       -- a metallic sine carrier, this track only
//   ORGAN  instrument_univibe()       -- the liquid photocell wobble, this track only
//   PAD    instrument_shallow() / instrument_grains_freeze()+instrument_grains_pitch()
//          -- WARP (watery warble) or FREEZE (a frozen granular drone), this track only
//   GTR    the ampcab.h amp/cab bundle (drive+eq+glue, 5 voicings) + instrument_fx_mod()
//          riding FXMOD_DRIVE every frame for a slow breathing pulse
//
// MUTE/SOLO make the isolation audible, not just asserted -- solo a track and everything
// else drops out, so "this effect touches ONLY this voice" is something you hear, not read.

#define NTRACK 6
enum { I_DRUMS = 0, I_BASS, I_LEAD, I_ORGAN, I_PAD, I_GUITAR };
enum { SL_DRUMS = 5, SL_BASS, SL_LEAD, SL_ORGAN, SL_PAD, SL_GUITAR };

static const char *TLABEL[NTRACK] = { "DRUMS", "BASS", "LEAD", "ORGAN", "PAD", "GTR" };
static const int   TCOL[NTRACK]   = { CLR_RED, CLR_BLUE, CLR_YELLOW, CLR_ORANGE, CLR_PINK, CLR_GREEN };

static const char *DESCR[NTRACK] = {
    "instrument_tape() -- warps only the drums",
    "instrument_gate() -- chokes only the bass",
    "instrument_ringmod() -- clangs only the lead",
    "instrument_univibe() -- vibes only the organ",
    "shallow()/grains_freeze()+grains_pitch() -- pad only",
    "ampcab.h voicing + instrument_fx_mod() -- guitar only",
};

#define COLW 50
#define X0   8
#define LABEL_Y 12
#define ROW_Y   22
#define BTN_W   44
#define BTN_H   14
#define KNOB_CY 58
#define MUTE_Y  82
#define SOLO_Y  82

static bool  fx_on[NTRACK]  = { false, false, false, false, false, false };
static float fx_amt[NTRACK] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.4f };  // GUITAR's knob = voicing 0..1
static bool  mute[NTRACK], solo[NTRACK];
static int   pad_mode = 0;     // 0 off, 1 WARP, 2 FREEZE
static int   voicing  = 2;     // CRUNCH, matches the effects-recipes anchor
static int   last_touched = -1;

// a I-vi-IV-V loop (C Am F G), root position around octave 3; bass sits an octave down,
// organ a further octave down from the shared chord table
static const int CHORD[4][3]  = { { 48, 52, 55 }, { 45, 48, 52 }, { 41, 45, 48 }, { 43, 47, 50 } };
static const int BASS_ROOT[4] = { 36, 33, 29, 31 };
static const char *DRUM_PAT = "x...x...x...x...";
static const char *BASS_PAT = "..x...x...x...x.";
static const char *LEAD_PAT = "x.x.x.x.x.x.x.x.";

static int   organH[3] = { -1, -1, -1 }, padH[3] = { -1, -1, -1 };
static int   chordIdx = 0, lastStep16 = -1, leadIdx = 0;
static float breatheT = 0;

// mute/solo aware volume for a track (solo, if any is active, silences every non-soloed track)
static int track_vol(int i, int base) {
    bool anySolo = false;
    for (int k = 0; k < NTRACK; k++) if (solo[k]) anySolo = true;
    if (anySolo) return solo[i] ? base : 0;
    return mute[i] ? 0 : base;
}

// mute/solo can change without the note re-triggering (ORGAN/PAD are HELD notes), so push the
// new volume onto whatever's currently ringing
static void refresh_held_vols(void) {
    for (int k = 0; k < 3; k++) {
        if (organH[k] >= 0) note_vol(organH[k], (float)track_vol(I_ORGAN, 5));
        if (padH[k]   >= 0) note_vol(padH[k],   (float)track_vol(I_PAD,   4));
    }
}

// ── per-track effects, SET-AND-HOLD: called only from the button/knob that changed ──
static void apply_track(int i) {
    float a = fx_amt[i];
    switch (i) {
    case I_DRUMS:
        if (fx_on[i]) instrument_tape(SL_DRUMS, 0.15f + 0.5f * a, 0.10f + 0.4f * a, 0.3f + 0.6f * a);
        else          instrument_tape(SL_DRUMS, 0, 0, 0);
        break;
    case I_BASS:
        instrument_gate(SL_BASS, fx_on[i] ? 0.1f + 0.6f * a : 0.0f, 3, 30 + (int)(150.0f * a));
        break;
    case I_LEAD:
        instrument_ringmod(SL_LEAD, 60.0f + 900.0f * a, fx_on[i] ? 0.65f : 0.0f);
        break;
    case I_ORGAN:
        instrument_univibe(SL_ORGAN, 1.5f + 6.0f * a, 0.75f, fx_on[i] ? 0.6f : 0.0f);
        break;
    }
}

static void pad_set_amt(void) {
    if (pad_mode == 1)      instrument_shallow(SL_PAD, 0.4f + 1.6f * fx_amt[I_PAD], 0.65f, 0.6f);
    else if (pad_mode == 2) instrument_grains_pitch(SL_PAD, (fx_amt[I_PAD] - 0.5f) * 24.0f, 0.2f, 0);
}

static void pad_set_mode(int m) {
    pad_mode = m;
    if (m == 0) {                                          // OFF -- clean pad
        instrument_shallow(SL_PAD, 1.0f, 0.6f, 0.0f);
        instrument_grains_freeze(SL_PAD, 0);
    } else if (m == 1) {                                    // WARP -- watery warble
        instrument_grains_freeze(SL_PAD, 0);
        pad_set_amt();
    } else {                                                // FREEZE -- a frozen granular drone
        instrument_shallow(SL_PAD, 1.0f, 0.6f, 0.0f);
        instrument_grains(SL_PAD, 180.0f, 14.0f, 0.7f, 0.35f, 0.3f, 0.6f);  // allocate the cloud ONCE
        instrument_grains_freeze(SL_PAD, 1);
        pad_set_amt();
    }
}

static void guitar_set_voicing(void) {
    voicing = (int)(fx_amt[I_GUITAR] * 4.999f);
    const AmpVoicing *v = &AMP_VC[voicing];
    ampcab_apply(SL_GUITAR, voicing, 0.6f, v->lo, v->mid, v->hi, 0.3f);
}

static void on_track_button(int i) {
    if (i == I_PAD)         { pad_set_mode((pad_mode + 1) % 3); }
    else                    { fx_on[i] = !fx_on[i]; if (i != I_GUITAR) apply_track(i); }
}

static void on_track_knob(int i) {
    if (i == I_PAD)         pad_set_amt();
    else if (i == I_GUITAR) guitar_set_voicing();
    else if (fx_on[i])      apply_track(i);
}

static const char *track_button_label(int i) {
    switch (i) {
    case I_DRUMS:  return fx_on[i] ? "TAPE" : "off";
    case I_BASS:   return fx_on[i] ? "GATE" : "off";
    case I_LEAD:   return fx_on[i] ? "RING" : "off";
    case I_ORGAN:  return fx_on[i] ? "VIBE" : "off";
    case I_PAD:    return pad_mode == 0 ? "off" : pad_mode == 1 ? "WARP" : "FRZ";
    case I_GUITAR: return fx_on[i] ? "BRTH" : "off";
    }
    return "?";
}

void init(void) {
    bpm(96);

    instrument(SL_DRUMS,  INSTR_MEMBRANE, 0, 0, 0, 40);
    instrument(SL_BASS,   INSTR_SAW, 2, 160, 3, 120);
    instrument_filter(SL_BASS, FILTER_LOW, 480, 3);
    instrument(SL_LEAD,   INSTR_SAW, 1, 90, 0, 180);
    instrument_filter(SL_LEAD, FILTER_LOW, 4000, 4);
    instrument(SL_ORGAN,  INSTR_ORGAN, 1, 0, 7, 200);
    instrument(SL_PAD,    INSTR_SINE, 250, 0, 7, 500);
    instrument(SL_GUITAR, INSTR_GUITAR, 1, 0, 7, 1200);

    for (int i = 0; i < NTRACK; i++) apply_track(i);   // everything starts a true bypass
    pad_set_mode(0);
    guitar_set_voicing();

    for (int k = 0; k < 3; k++) {
        organH[k] = note_on(CHORD[0][k] - 12, SL_ORGAN, 5);
        padH[k]   = note_on(CHORD[0][k],      SL_PAD,   4);
    }
}

void update(void) {
    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int   sixteenth = (int)pos16;
    if (sixteenth != lastStep16) {
        lastStep16 = sixteenth;
        int step = sixteenth % 16;
        int bar  = sixteenth / 16;
        int ci   = (bar / 2) % 4;              // a new chord every 2 bars

        if (ci != chordIdx) {
            chordIdx = ci;
            for (int k = 0; k < 3; k++) {
                note_pitch(organH[k], CHORD[ci][k] - 12);
                note_pitch(padH[k],   CHORD[ci][k]);
            }
            int notes[3] = { CHORD[ci][0] - 12, CHORD[ci][1] - 12, CHORD[ci][2] - 12 };
            strum_notes(notes, 3, SL_GUITAR, track_vol(I_GUITAR, 5), 30);   // re-strum the new chord
        }

        if (DRUM_PAT[step] == 'x') hit(38, SL_DRUMS, track_vol(I_DRUMS, 6), 220);
        if (BASS_PAT[step] == 'x') hit(BASS_ROOT[chordIdx], SL_BASS, track_vol(I_BASS, 5), 180);
        if (LEAD_PAT[step] == 'x') {
            hit(CHORD[chordIdx][leadIdx % 3] + 12, SL_LEAD, track_vol(I_LEAD, 4), 160);
            leadIdx++;
        }
    }

    // GUITAR's BREATHE -- a slow drive pulse ridden LIVE (instrument_fx_mod is designed to be
    // called every frame, unlike the set-and-hold instrument_tape/gate/ringmod/univibe above)
    breatheT += dt();
    if (fx_on[I_GUITAR])
        instrument_fx_mod(SL_GUITAR, FXMOD_DRIVE, 0.5f + 0.45f * sinf(breatheT * 6.283f * 0.25f));
}

void draw(void) {
    cls(CLR_DARK_GREY);
    print("MIXING BOOTH", (SCREEN_W - text_width("MIXING BOOTH")) / 2, 2, CLR_WHITE);

    ui_begin();
    for (int i = 0; i < NTRACK; i++) {
        int x = X0 + i * COLW;
        print(TLABEL[i], x, LABEL_Y, TCOL[i]);

        if (ui_button(x, ROW_Y, BTN_W, BTN_H, track_button_label(i))) {
            on_track_button(i);
            last_touched = i;
        }
        if (ui_knob(&fx_amt[i], x + BTN_W / 2, KNOB_CY, i == I_GUITAR ? "VOICE" : "AMT")) {
            on_track_knob(i);
            last_touched = i;
        }
        if (ui_button(x, MUTE_Y, 21, 12, mute[i] ? "MU" : "..")) { mute[i] = !mute[i]; refresh_held_vols(); }
        if (ui_button(x + 23, SOLO_Y, 21, 12, solo[i] ? "SO" : "..")) { solo[i] = !solo[i]; refresh_held_vols(); }
    }
    ui_end();

    font(FONT_SMALL);
    const char *cap = last_touched >= 0 ? DESCR[last_touched]
        : "each switch touches ONLY its own track -- the rest stays clean";
    print(cap, 6, 188, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}
