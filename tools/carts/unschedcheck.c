/* de:meta
{
  "slug": "unschedcheck",
  "title": "unschedule probe",
  "status": "active",
  "created": "2026-11-03",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "step-sequencer"
  ],
  "lineage": "Measurement probe for unschedule() (asked for by Playtime's stingers: a part that rests while a stinger plays on its instrument, after its notes were already booked a bar ahead). Driven by tools/schedule-check/run.sh, not played by hand. Sibling of schedcheck.",
  "description": "Books every click of two slots up front, then takes some back. Slot 6 loses a WINDOW (clicks 5..9) half a second after booking; slot 5 loses everything from click 15 on, unbooked in the SAME frame it was booked, which is the FIFO claim (an unschedule sent right after its bookings must still see them). UC_MODE=0 is the NEGATIVE CONTROL: the same run with no unschedule, where every click must be heard, or the gate could not tell a dropped note from a click it simply failed to detect. Driven by `bash tools/schedule-check/run.sh`; not meant to be played."
}
de:meta */
// unschedcheck: the unschedule() probe. See tools/schedule-check/unsched.c for the measurement.
// Slot 5 clicks on the step, slot 6 half a step later, so every onset says which slot it was.
#include "studio.h"

#ifndef UC_MODE
#define UC_MODE 1          // 1 = take notes back · 0 = NEGATIVE CONTROL: book everything, take nothing back
#endif
#define UC_STEP 0.25       // seconds between a slot's clicks = 11025 samples
#define UC_N    20         // clicks per slot (2 x 20 = 40 booked notes, inside the engine's 64-note pen)

void update(void) {
    static int frame_n = 0;
    static double t0 = 0;
    if (frame_n == 0) {
        instrument(5, INSTR_SQUARE, 0, 0, 7, 1);
        instrument(6, INSTR_SQUARE, 0, 0, 7, 1);
        t0 = audio_time() + 0.3;
        for (int k = 0; k < UC_N; k++) {
            schedule_at(t0 + k * UC_STEP, 81, 5, 7, 15);
            schedule_at(t0 + k * UC_STEP + UC_STEP / 2, 69, 6, 7, 15);
        }
#if UC_MODE
        unschedule(5, t0 + 15 * UC_STEP - 0.01, 0);                  // open-ended, in the booking frame
#endif
    }
#if UC_MODE
    if (frame_n == 30)                                              // half a second later: a window
        unschedule(6, t0 + 5 * UC_STEP, t0 + 10 * UC_STEP);
#endif
    frame_n++;
}

void draw(void) { cls(CLR_BLACK); print("unschedule probe", 8, 8, CLR_LIGHT_GREY); }
