/* de:meta
{
  "slug": "warbleprobe",
  "title": "tape warble probe",
  "status": "active",
  "created": "2026-10-01",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "lineage": "Measurement probe for tape_warble() (docs/design/chompi-harvest.md §2), ported from CHOMPI's Warble.h. Rendered headless and read back by its pitch track; not played by hand.",
  "description": "One sustained A440 sine through a transparent tape() (no wow, no flutter, no saturation), so the only thing that can move its pitch is the warble. 0-4 s warble off (the control: the pitch must be dead flat), 4-12 s tape_warble(0.5) (the pitch must sit still and then sag at random moments), 12-16 s off again (the head glides home, then flat). Render it with `node tools/play.js warbleprobe script /dev/null --headless --frames 960 --wav out.wav`."
}
de:meta */
// warbleprobe — the tape_warble() measurement. See docs/design/chompi-harvest.md §2.
//
// WHY A BARE SINE THROUGH A TRANSPARENT TAPE. A pitch tracker on a sine has nothing to confuse it,
// and tape(0, 0, 0) runs the insert with every OTHER source of pitch movement switched off, so any
// wobble in the render is the warble's. Sections are on a fixed frame timeline so the reader knows
// where the control ends without being told.
#include "studio.h"

#define F_ON   240   // 4 s: warble on
#define F_OFF  720   // 12 s: warble off (glide home)

void draw(void) {
    int f = frame();
    if (f == 1) {
        instrument(5, INSTR_SINE, 5, 0, 7, 50);
        tape(0.0f, 0.0f, 0.0f);
        note_on(69, 5, 5);
    }
    if (f == F_ON)  tape_warble(0.5f);
    if (f == F_OFF) tape_warble(0.0f);
    cls(CLR_BLACK);
    print(f < F_ON ? "warble OFF (control)" : f < F_OFF ? "warble 0.5" : "warble OFF (gliding home)", 8, 8, CLR_WHITE);
}
