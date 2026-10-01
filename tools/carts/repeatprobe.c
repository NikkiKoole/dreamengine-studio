/* de:meta
{
  "slug": "repeatprobe",
  "title": "beat repeat probe",
  "status": "active",
  "created": "2026-10-01",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "granular-synth"
  ],
  "lineage": "Measurement probe for grains_repeat() (docs/design/chompi-harvest.md §3), CHOMPI TEMPO's clock-locked freeze. Rendered headless and read back by its onsets; not played by hand.",
  "description": "At 120 bpm a plucked sine plays an 8th-note line of four pitches. At 4.117 s, deliberately 7 frames AFTER a hit, the tank freezes with grains_repeat(1) and the line stops, so everything heard from then on is the repeat. It must keep landing on the 8th-note grid anyway. At 6 s the repeat drops to half a beat while still frozen (a length change, crossfaded), at 8 s it unfreezes and the line comes back. Render it with `node tools/play.js repeatprobe script /dev/null --headless --frames 600 --wav out.wav`."
}
de:meta */
// repeatprobe — the grains_repeat() measurement. See docs/design/chompi-harvest.md §3.
//
// WHY FREEZE LATE. The claim worth testing is that the repeat stays on the beat however late the
// press is. Freezing exactly on a hit would pass even if the loop simply started at the press, so the
// freeze lands 7 frames (117 ms) after one, and the line STOPS at the freeze, so any onset after it
// can only have come out of the loop.
#include "studio.h"

#define F_FREEZE 247   // 4.117 s: 7 frames after the hit at 240
#define F_HALF   360   // 6 s: repeat 1 beat -> 0.5 beat, still frozen
#define F_THAW   480   // 8 s: back to live

static const int LINE[4] = { 60, 67, 72, 64 };

void draw(void) {
    int f = frame();
    if (f == 1) {
        bpm(120);
        instrument(6, INSTR_SINE, 1, 60, 0, 30);
        grains(100, 10, 1.0f, 0.0f, 0.0f, 1.0f);
        grains_repeat(1.0f);
    }
    bool frozen = f >= F_FREEZE && f < F_THAW;
    if (f == F_FREEZE) grains_freeze(1);
    if (f == F_HALF)   grains_repeat(0.5f);
    if (f == F_THAW)   grains_freeze(0);
    if (f >= 30 && f % 15 == 0 && !frozen) hit(LINE[(f / 15) % 4], 6, 6, 80);   // an 8th at 120 bpm = 15 frames
    cls(CLR_BLACK);
    print(frozen ? (f < F_HALF ? "FROZEN  repeat 1 beat" : "FROZEN  repeat 1/2 beat") : "live", 8, 8, CLR_WHITE);
}
