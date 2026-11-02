/* de:meta
{
  "slug": "widthprobe",
  "title": "stereo width probe",
  "status": "hidden",
  "created": "2026-11-02",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "Measurement probe for stereo_width() (issue #47: a 78 is mono, and nothing could narrow a mix). Rendered headless and read back with tools/stereo-check.js; not played by hand.",
  "description": "Measurement probe, not a playable cart (status hidden). Two notes hard-panned left and right and a plate reverb (whose tail is itself stereo), so the mix is wide in two different ways. VARIANT 0 never calls width; 1 calls stereo_width(0) at boot (must be mono, tail included); 2 calls stereo_width(1) (must be byte-identical to 0); 3 calls stereo_width(2); 4 goes stereo_width(0) and back to stereo_width(1) after two seconds (must end byte-identical again). Render it with `node tools/play.js widthprobe script /dev/null --headless --frames 360 --wav out.wav`."
}
de:meta */
// widthprobe — the stereo_width() measurement (issue #47).
#include "studio.h"

static int VARIANT = 0;   // 0 never · 1 stereo_width(0) · 2 stereo_width(1) · 3 stereo_width(2) · 4 stereo_width(0) then back to 1 at frame 120

static int f = 0;

void init(void) {
    instrument(5, INSTR_SAW, 5, 100, 5, 300);  instrument_pan(5, -1.0f);  instrument_reverb(5, 0.4f);
    instrument(6, INSTR_SQUARE, 5, 100, 5, 300); instrument_pan(6, 1.0f); instrument_reverb(6, 0.4f);
    reverb(0.7f, 0.4f);  reverb_plate(1.0f);           // a plate's tail comes back WIDE (two pickups)
    if (VARIANT == 1 || VARIANT == 4) stereo_width(0.0f);
    if (VARIANT == 2) stereo_width(1.0f);
    if (VARIANT == 3) stereo_width(2.0f);
}

void update(void) {
    if (f % 30 == 0) { hit(48 + (f / 30) % 5, 5, 6, 200); hit(60 + (f / 30) % 3, 6, 5, 200); }
    if (VARIANT == 4 && f == 120) stereo_width(1.0f);
    f++;
#ifdef DE_TRACE
    watch("variant", "%d", VARIANT);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("widthprobe", 4, 4, CLR_WHITE);
}
