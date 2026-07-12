// ampcab.h — the shared guitar amp/cabinet VOICING table (cart-land, like fxicons.h).
//
// An amp/cab is a PRESET BUNDLE of effects we already ship, NOT new DSP
// (docs/design/effects-bus-architecture.md §E): instrument_drive + a DRIVE_* waveshaper +
// instrument_eq + glue, pinned as the output stage like leslie. This header is the ONE source of
// truth for the five voicings so combo.c (the standalone amp) and pedalboard.c (the pinned CABINET
// slot) can't drift apart. Include AFTER studio.h (it uses DRIVE_*/CLR_*/instrument_*/glue).

#ifndef DE_AMPCAB_H
#define DE_AMPCAB_H

typedef struct {
    const char *name;   // voicing name (UI)
    int   mode;         // DRIVE_* waveshaper
    float drive;        // base drive amount — GAIN scales this
    float lo, mid, hi;  // base EQ curve in dB — tone knobs offset this
    float glue;         // base power-amp sag — SAG scales this
    float timbre;       // INSTR_GUITAR string brightness for this voicing
    int   col;          // UI accent colour
} AmpVoicing;

#define AMPCAB_N 5
static const AmpVoicing AMP_VC[AMPCAB_N] = {
    { "CLEAN",   DRIVE_SOFT, 0.08f,  2,-2, 3, 0.15f, 0.75f, CLR_LIGHT_GREY   },
    { "CHIME",   DRIVE_ASYM, 0.30f,  0, 2, 4, 0.30f, 0.65f, CLR_LIGHT_YELLOW },
    { "CRUNCH",  DRIVE_ASYM, 0.55f,  3, 2,-2, 0.40f, 0.55f, CLR_ORANGE       },   // the effects-recipes anchor
    { "HI-GAIN", DRIVE_HARD, 0.80f,  4,-4, 2, 0.60f, 0.45f, CLR_RED          },
    { "LO-FI",   DRIVE_FOLD, 0.70f, -3, 4,-6, 0.50f, 0.30f, CLR_MAUVE        },
};

// Apply the full amp bundle to `slot`. gain01 scales the voicing's drive; eqLo/Mid/Hi are the FINAL
// dB the caller wants (combo adds its BASS/MID/TREBLE offset; pedalboard passes the voicing's base
// curve); sag01 scales the voicing's glue (master bus 0). mode + timbre come from the voicing.
// SET-AND-HOLD is the caller's job — call this only when something CHANGED, never every frame
// (reconfiguring eq/glue per frame churns the audio thread → stutter).
static void ampcab_apply(int slot, int v, float gain01, float eqLo, float eqMid, float eqHi, float sag01) {
    if (v < 0 || v >= AMPCAB_N) v = 0;   // clamp voicing index into the AMP_VC[] table
    const AmpVoicing *a = &AMP_VC[v];
    instrument_drive_mode(slot, a->mode);
    instrument_timbre(slot, a->timbre);
    instrument_drive(slot, a->drive * (0.30f + 1.4f * gain01));
    instrument_eq(slot, eqLo, eqMid, eqHi);
    glue(0, a->glue * sag01, 8, 120);
}

#endif
