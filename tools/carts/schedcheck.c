/* de:meta
{
  "slug": "schedcheck",
  "title": "schedule clock probe",
  "status": "active",
  "created": "2026-09-28",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "step-sequencer"
  ],
  "lineage": "Measurement probe for schedule_at() / audio_time() (docs/design/audio-timing.md): the notes a sequencer books must land on their samples however the frames and the audio callbacks interleave. Driven by tools/schedule-check/run.sh, not played by hand. Born from loficity, whose grooves wobbled on native playback.",
  "description": "Books a short click every 0.25 s and nothing else, so the spacing between clicks in the render IS the scheduling error. Built two ways: SC_MODE=1 books on the sound clock with schedule_at(audio_time() + ...), which must land every click exactly 11025 samples apart; SC_MODE=0 is the NEGATIVE CONTROL, the old way - a delay from a frame clock through schedule_hit - which must show the swing of an audio buffer, or the gate cannot tell a working scheduler from a broken one. Driven by `bash tools/schedule-check/run.sh`; not meant to be played."
}
de:meta */
// schedcheck — the schedule-clock probe. See tools/schedule-check/run.sh for the measurement.
//
// WHY IT IS THIS EMPTY: the measurement is the gap between click onsets, so anything else that makes
// a sound would be contamination. One square-wave slot, one click per step, no effects.
#include "studio.h"

#ifndef SC_MODE
#define SC_MODE 1          // 1 = schedule_at on audio_time (the fix) · 0 = schedule_hit from a frame clock (control)
#endif
#define SC_STEP 0.25       // seconds between clicks = 11025 samples
#define SC_N    40
#define SC_LOOK 0.1        // book this far ahead, like a sequencer does

void update(void) {
    static bool boot = false;
    static double t0 = 0, fclk = 0;
    static int n = 0;
    if (!boot) {
        instrument(5, INSTR_SQUARE, 0, 0, 7, 1);
#if SC_MODE
        t0 = audio_time() + 0.2;
#else
        t0 = 0.2;
#endif
        boot = true;
    }
    fclk += dt();
#if SC_MODE
    double clock = audio_time();
#else
    double clock = fclk;          // the old way: a frame clock, and a DELAY from it
#endif
    while (n < SC_N && t0 + n * SC_STEP < clock + SC_LOOK) {
        double t = t0 + n * SC_STEP;
#if SC_MODE
        schedule_at(t, 81, 5, 7, 15);
#else
        schedule_hit((int)((t - clock) * 1000.0), 81, 5, 7, 15);
#endif
        n++;
    }
}

void draw(void) { cls(CLR_BLACK); print("schedule clock probe", 8, 8, CLR_LIGHT_GREY); }
