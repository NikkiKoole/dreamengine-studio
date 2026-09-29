/* de:meta
{
  "slug": "radclockcheck",
  "title": "radio clock probe",
  "status": "active",
  "created": "2026-09-29",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "step-sequencer"
  ],
  "lineage": "Measurement probe for radio.h's sample-clock step grid (rad_audio_pos / rad_audio_step / rad_hit), docs/design/radio-arranger-lessons.md phase 0. Sibling of schedcheck: same host, same measurement, but through the radio chassis the 39 stations use. Driven by tools/schedule-check/run.sh, not played by hand.",
  "description": "Runs radio.h's step clock at 60 bpm (a 16th = 0.25 s = 11025 samples) and books one click per step, so the spacing between clicks IS the scheduling error. SC_MODE=1 is the new path every station will migrate to (rad_audio_pos + rad_audio_step + rad_hit, booked on the sample clock): every gap must be exactly one step. SC_MODE=0 is the path the stations use today (rad_clock_step against beat() + schedule_hit(rad_step_dly(...))), the NEGATIVE CONTROL that must swing by a good part of an audio buffer. Driven by `bash tools/schedule-check/run.sh`; not meant to be played."
}
de:meta */
// radclockcheck — the radio-clock probe. See tools/schedule-check/run.sh for the measurement.
#include "studio.h"
#include "radio.h"

#ifndef SC_MODE
#define SC_MODE 1          // 1 = the sample-clock grid (rad_audio_*) · 0 = today's rad_clock_step + schedule_hit (control)
#endif
#define TEMPO 60           // a 16th step = 0.25 s = 11025 samples

static RadioClock clk = { -1, 0, 250.0 };

void update(void) {
    static bool boot = false;
    if (!boot) { instrument(5, INSTR_SQUARE, 0, 0, 7, 1); bpm(TEMPO); boot = true; }
    long st;
#if SC_MODE
    double pos = rad_audio_pos(&clk, TEMPO);
    (void)pos;
    while (rad_audio_step(&clk, &st)) if (st >= 4 && st < 44) rad_hit(&clk, st, 0, 81, 5, 7, 15);
#else
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    clk.stepMs = 60000.0 / (TEMPO * 4);
    while (rad_clock_step(&clk, pos, &st)) if (st >= 4 && st < 44) schedule_hit(rad_step_dly(&clk, st, pos), 81, 5, 7, 15);
#endif
}

void draw(void) { cls(CLR_BLACK); print("radio clock probe", 8, 8, CLR_LIGHT_GREY); }
