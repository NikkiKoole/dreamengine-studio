/* de:meta
{
  "slug": "inslat",
  "title": "insert latency probe",
  "status": "active",
  "created": "2026-08-17",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "audio-input"
  ],
  "lineage": "Measurement probe for the aumf (audio-effect plug-in) question in docs/design/auv3-plugin-types.md §4.1: the mic ring was built for ANALYSIS, so its insert latency was unmeasured. Driven by tools/insert-latency.js, not played by hand.",
  "description": "Measures how long a sample takes to get from de_audio_input() to the output — the number that decides whether a cart can be an INSERT EFFECT (a guitar pedal cares; an envelope follower does not). Deliberately the emptiest cart that can answer it: input_monitor(1.0) and NOTHING else makes a sound, so every sample in the render came from the input ring. No effects either, so the figure is the RING, not the pedals. Feed it impulses with DE_MIC_WAV and the output offset minus the input offset is the latency; several impulses spaced apart say whether it is CONSTANT (declarable to a host via the AU's latency property) or drifts. Driven by `node tools/insert-latency.js`; not meant to be played."
}
de:meta */
// inslat — the insert-latency probe. See tools/insert-latency.js for the measurement.
//
// WHY THE CART IS THIS EMPTY. The question is the RING's latency, so anything else that makes a
// sound is contamination: a single voice, a reverb tail, even a UI click would land in the render
// and be indistinguishable from the impulse we are looking for. So: no notes, no effects, no
// widgets. Every non-zero sample in the output WAV came through de_audio_input().
//
// The harness path this rides (runtime/studio.c, DE_MIC_WAV) pushes one frame's worth of input
// samples and then renders one frame's worth of output, which is EXACTLY the shape an AUv3 render
// block has — the host hands you N input samples and asks for N output samples in the same
// callback. That is what makes a headless number meaningful for the plug-in case rather than just
// a fact about our mic.
#include "studio.h"

static int armed = 0;

void init(void) {
    // SET-AND-HOLD, once. input_monitor() is a live setter but re-issuing it every frame rebuilds
    // bus DSP 60x/s (CLAUDE.md's "effects are set-and-hold" rule; lint-fx-frame gates it).
    mic_start();            // raises the "wanted" flag; DE_MIC_WAV calls de_mic_set_active(1) itself
    input_monitor(1.0f);    // UNITY, so the output amplitude is also a gain measurement
    armed = 1;
}

void draw(void) {
    cls(CLR_BLACK);
    print("insert latency probe", 8, 8, CLR_WHITE);
    print(armed ? "monitor: on (unity)" : "monitor: OFF", 8, 20, armed ? CLR_LIGHT_GREY : CLR_RED);
    print("drive with tools/insert-latency.js", 8, 40, CLR_DARK_GREY);
    print("mic_active():", 8, 60, CLR_DARK_GREY);
    print(mic_active() ? "yes" : "no", 112, 60, mic_active() ? CLR_LIME_GREEN : CLR_DARK_ORANGE);
#ifdef DE_TRACE
    watch("mic_active", "%d", mic_active());
    watch("mic_level", "%.4f", mic_level());
#endif
}
