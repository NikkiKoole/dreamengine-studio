#include "studio.h"
#include <stdio.h>

// TUNECHECK — the sound engine's TUNING self-test cart. Plays a fixed sweep of
// (engine × pitch) sustained notes with silence gaps, so `node tools/tune-check.js`
// can measure each note's ACTUAL fundamental and report how many cents sharp/flat it
// is. The modeled/physical engines (bowed, piped, brass, …) can drift out of tune in
// ways the level-only wav-analyze.js can't see; this cart + that analyzer catch it.
//
//   node tools/tune-check.js            # render this cart + report cents-of-detune
//
// The SCHEDULE below is the source of truth. Every frame it watch()es the current
// engine id, the EXPECTED midi, and a gate flag — the analyzer reads ground truth
// straight from the trace, so the report can never drift out of sync with this table.
// SINE is first as a control: it's mathematically exact, so a non-zero cents reading on
// SINE means the MEASUREMENT is off, not the engine.
//
// Pure timing harness — nothing to look at; run it headless via tune-check.js.

#define NOTE_FRAMES 48     // ~0.8s sounding @ 60fps (analyzer measures the stable middle)
#define GAP_FRAMES  14     // ~0.23s silence between notes (lets releases die)
#define PERIOD      (NOTE_FRAMES + GAP_FRAMES)

// the non-standard / pitched engines worth tuning-checking. SINE = control.
// (NOISE is unpitched; MEMBRANE is an inharmonic drum; VOICE is formant-shaped —
//  all three are deliberately left out of the default tuning sweep.)
static const int ENGINES[] = {
    INSTR_SINE,   INSTR_PLUCK,  INSTR_MALLET, INSTR_FM,    INSTR_ORGAN,
    INSTR_EPIANO, INSTR_PD,     INSTR_REED,   INSTR_PIPE,  INSTR_GUITAR,
    INSTR_PIANO,  INSTR_BOWED,  INSTR_BRASS,
    INSTR_PIANO,   // ← the DIFFERENTIAL pass; see ET_ENTRY. Keep this LAST.
};
#define NENG ((int)(sizeof(ENGINES) / sizeof(ENGINES[0])))

// THE DIFFERENTIAL PASS. The last entry is PIANO again with MODE_PIANO_STRETCH forced to 0, i.e.
// plain equal temperament. PIANO deliberately does NOT tune to ET (it stretches its fundamentals,
// bass-flat/treble-sharp — the Railsback curve), and the deviation that creates is smaller than
// tune-check's warn threshold, so an absolute-pitch check printed a tick whether the stretch worked
// fully, half-worked, or did nothing at all — which is exactly how audit §I4c survived. Rendering
// the SAME engine with the stretch off, in the SAME pass and the same measurement window, lets the
// analyzer assert the DIFFERENCE between the two passes against the intended curve. The difference
// cancels every constant error the loop carries (§I4d's uncompensated delay, and the way it drifts
// within a note as the brightness bloom moves `ksb`), so there is nothing to bless and nothing to
// re-bless when a window changes. Needs the stretch to be a RUNTIME parameter — that is what
// MODE_PIANO_STRETCH is for. Plan §2.3(a).
#define ET_ENTRY (NENG - 1)

// four octaves of A (A4 = 69 = 440Hz exactly) — wide enough to expose pitch-dependent
// detuning and octave-tracking bugs (a flageolet/overblow jumping the wrong way, etc.)
static const int PITCHES[] = { 45, 57, 69, 81 };   // A2 110 · A3 220 · A4 440 · A5 880
#define NPIT ((int)(sizeof(PITCHES) / sizeof(PITCHES[0])))

#define NNOTES (NENG * NPIT)
#define SLOT0  5                      // engine e lives on instrument slot SLOT0+e

static int fnum = -1;
static int held = -1;

void init(void) {
    // one slot per engine, all defined up front so nothing is redefined mid-render.
    // sustain 7 holds the held engines wide open; the decaying engines (pluck/mallet/
    // fm/epiano/guitar/piano) ring down naturally inside the gate. No macros set on
    // purpose: we test each engine's AS-SHIPPED default voice (what note() gives you).
    for (int e = 0; e < NENG; e++)
        instrument(SLOT0 + e, ENGINES[e], 4, 60, 7, 140);
    instrument_mode(SLOT0 + ET_ENTRY, MODE_PIANO_STRETCH, 0.0f);   // the differential pass: stretch OFF
    // ⚠ INHARMONICITY OFF FOR BOTH PIANO PASSES, and this is a correctness fix, not a convenience.
    // PIANO ships stiff-string dispersion (MODE_PIANO_STIFF, audit §I4b), which makes the waveform
    // genuinely NON-PERIODIC — the partials are no longer integer multiples. tune-check detects pitch
    // with YIN, an autocorrelation-family method, and autocorrelation on an inharmonic signal locks onto
    // a shorter lag pulled by the stretched upper partials: it read A2 as +26¢ SHARP with confidence
    // falling to 0.65, while a spectral-peak measurement of the same render puts the fundamental exactly
    // where it belongs. So the sharp reading was the DETECTOR, not the engine.
    // Turning dispersion off here keeps this sweep measuring the one thing it is for — TUNING, including
    // the stretched-tuning differential — on a signal its detector can actually track. Inharmonicity has
    // its own oracle (tools/inharm-spec.js, which uses Goertzel peaks and is immune to this), and the
    // evidence that the stiff knob does NOT move pitch lives in plan §2.3(a).
    // FOLLOW-UP: an automated "pitch is invariant across MODE_PIANO_STIFF" assertion belongs in
    // inharm-spec, not here, precisely because it needs the spectral method.
    for (int e = 0; e < NENG; e++)
        if (ENGINES[e] == INSTR_PIANO) instrument_mode(SLOT0 + e, MODE_PIANO_STIFF, 0.0f);
}

void update(void) {
    fnum++;
    int idx   = fnum / PERIOD;           // which note in the flattened sweep
    int local = fnum % PERIOD;           // frame within this note's slot
    int gate  = (idx < NNOTES) && (local < NOTE_FRAMES);

    int eng = -1, midi = -1;
    if (idx < NNOTES) {
        eng  = idx / NPIT;               // engine index 0..NENG-1
        midi = PITCHES[idx % NPIT];
        if (local == 0)                  // note-on: start the held note
            held = note_on(midi, SLOT0 + eng, 7);
        else if (local == NOTE_FRAMES - 1 && held >= 0) {  // release before the gap
            note_off(held);
            held = -1;
        }
    }

#ifdef DE_TRACE
    // ground truth for tools/tune-check.js (read straight from the trace):
    watch("note", "%d", idx);
    watch("eng",  "%d", eng < 0 ? -1 : ENGINES[eng]);   // the INSTR_* id, not the index
    watch("emidi", "%d", midi);                          // expected MIDI note (-1 = silence)
    watch("gate", "%d", gate);
    // 1 = this is the stretch-OFF differential pass (same INSTR_* id as the normal one, so the
    // analyzer cannot tell them apart from `eng` alone). See ET_ENTRY.
    watch("et", "%d", eng == ET_ENTRY ? 1 : 0);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    int idx = fnum < 0 ? -1 : fnum / PERIOD;
    print("TUNECHECK - run via tools/tune-check.js", 6, 6, CLR_WHITE);
    char buf[48];
    snprintf(buf, sizeof(buf), "note %d / %d", idx < 0 ? 0 : idx, NNOTES);
    print(buf, 6, 20, CLR_LIGHT_GREY);
}
