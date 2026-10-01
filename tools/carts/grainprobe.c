/* de:meta
{
  "slug": "grainprobe",
  "title": "grain voice probe",
  "status": "active",
  "created": "2026-10-01",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "granular-synth"
  ],
  "lineage": "Measurement probe for INSTR_GRAIN (docs/design/plinky-harvest.md §4.1), Plinky's per-note granular sampler. Rendered headless and read back by pitch and correlation; not played by hand.",
  "description": "A 0.5 s sine sweep (200 → 800 Hz) is loaded as sample 0 with A3 as its root. Five tests of 1.5 s each, separated by 0.25 s of silence: T0 plays it with INSTR_SAMPLE looping (the reference); T1 with INSTR_GRAIN at SPEED 1× on the root note (must match T0); T2 FROZEN in the middle of the sweep (the pitch must stop moving, near 500 Hz); T3 an octave up at 1× (twice the pitch, the SAME sweep timing); T4 with scatter + detune (must stay finite and click-free). Render it with `node tools/play.js grainprobe script /dev/null --headless --frames 500 --wav out.wav`."
}
de:meta */
// grainprobe — the INSTR_GRAIN measurement. See docs/design/plinky-harvest.md §4.1.
//
// WHY A SWEEP. A steady tone can't tell "the playhead moved" from "it didn't": a sweep's pitch IS its
// position. So: T1's pitch track must follow T0's (the voice is transparent at 1×), T2's must sit still
// (frozen), and T3's must be T1's ×2 at the same moments (pitch moved, timing didn't).
#include "studio.h"
#include <math.h>

#define SR    44100
#define N     (SR / 2)
#define ROOT  57          // A3: the sample's original pitch
#define TLEN  90          // frames a test holds
#define TGAP  15          // frames of silence between tests
static float sweep[N];

static int t_of(int f) { return f < 30 ? -1 : (f - 30) / (TLEN + TGAP); }

void init(void) {
    double ph = 0.0;
    for (int i = 0; i < N; i++) {                 // linear 200 → 800 Hz over the half second
        double hz = 200.0 + 600.0 * (double)i / N;
        sweep[i] = 0.5f * (float)sin(ph);
        ph += 6.283185307179586 * hz / SR;
    }
    sample_load(0, sweep, N);
    instrument(5, INSTR_SAMPLE, 1, 0, 7, 20);
    instrument_sample(5, 0, ROOT);
    instrument_sample_mode(5, SAMPLE_LOOP);
    instrument(6, INSTR_GRAIN, 1, 0, 7, 20);
    instrument_sample(6, 0, ROOT);
    instrument_harmonics(6, 0.0f);                // POSITION: from the start of the region
    instrument_timbre(6, 0.5f);                   // SIZE 70 ms
}

static int h = -1;
void update(void) {
    int f = frame(), t = t_of(f), ph = f < 30 ? -1 : (f - 30) % (TLEN + TGAP);
    if (t < 0 || t > 4) return;
    if (ph == 0) {
        instrument_morph(6, t == 2 ? 0.0f : 0.5f);                    // T2 frozen, the rest 1×
        instrument_harmonics(6, t == 2 ? 0.5f : 0.0f);                // T2 freezes MID-sweep (~500 Hz): at 0 a
                                                                      // centred grain straddles the loop seam
        instrument_mode(6, MODE_GRAIN_SCATTER, t == 4 ? 0.6f : 0.0f);
        instrument_mode(6, MODE_GRAIN_DETUNE,  t == 4 ? 0.4f : 0.0f);
        h = note_on(t == 3 ? ROOT + 12 : ROOT, t == 0 ? 5 : 6, 6);
    }
    if (ph == TLEN && h >= 0) { note_off(h); h = -1; }
#ifdef DE_TRACE
    watch("test", "%d", t);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    int t = t_of(frame());
    static const char *const NAME[5] = { "T0 sample loop", "T1 grain 1x", "T2 grain frozen", "T3 grain +oct", "T4 scatter+detune" };
    print(t >= 0 && t <= 4 ? NAME[t] : "", 8, 8, CLR_WHITE);
}
