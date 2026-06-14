// navkit-fx-render.c — render navkit's GENUINE bus effect over a pure sine to a mono WAV, so we can
// A/B one of our ported effects against the real thing (NOT a formula copy — this links navkit's
// actual setBus*/processBusEffects from engines/effects.h via synth.h's real include chain).
//
// Build standalone (no raylib; absolute include paths, so no -I needed):
//   clang -O2 -o /tmp/navfx tools/navkit-fx-render.c -lm
//
// Run (effect name, then its params, then out.wav, duration s, carrier Hz):
//   /tmp/navfx tremolo <rate> <depth> <shape>            out.wav [dur] [hz]   shape 0=sine 1=square 2=tri
//   /tmp/navfx phaser  <rate> <depth> <fb> <mix> <stages> out.wav [dur] [hz]
//
// Then render OURS by exercising the matching API in a tiny cart (tremtest.c / phasertest.c) with
// `play.js … --det --wav`, and compare: tools/wav-modrate.js (rate+depth) or tools/wav-correlate.js
// (sample-level). See docs/guides/debug-harness.md → "A/B a bus EFFECT". Adding a new effect = one
// more `else if` calling its setBus* (the navkit path is identical for every bus effect).
//
// NOTE: the navkit path below is the sibling repo location used throughout this project
// (~/Projects/navkit/soundsystem). Edit the two #includes if yours lives elsewhere.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "/Users/nikkikoole/Projects/navkit/soundsystem/engines/synth.h"
#include "/Users/nikkikoole/Projects/navkit/soundsystem/engines/effects.h"
#include "/Users/nikkikoole/Projects/navkit/soundsystem/engines/dub_loop.h"        // hermiteInterpolate (granular_delay.h needs it)
#include "/Users/nikkikoole/Projects/navkit/soundsystem/engines/granular_delay.h"  // processGranularDelay — its own path, not processBusEffects

static void writeWavMono(const char *path, const short *buf, int n) {
    FILE *f = fopen(path, "wb");
    int sr = SAMPLE_RATE, byteRate = sr * 2, dataBytes = n * 2;
    fwrite("RIFF", 1, 4, f); int riff = 36 + dataBytes; fwrite(&riff, 4, 1, f);
    fwrite("WAVE", 1, 4, f); fwrite("fmt ", 1, 4, f);
    int fmtLen = 16; short fmt = 1, ch = 1, bits = 16, blockAlign = 2;
    fwrite(&fmtLen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&byteRate, 4, 1, f);
    fwrite(&blockAlign, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f);
    fwrite(buf, 2, n, f); fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: navfx <tremolo|phaser> <params...> <out.wav> [dur] [hz]\n"); return 1; }
    const char *effect = argv[1];
    const char *out = "/tmp/navfx.wav";
    float dur = 3.0f, hz = 440.0f;
    _ensureMixerCtx();

    int next;   // index of the out.wav arg, after this effect's positional params
    if (!strcmp(effect, "tremolo")) {
        float rate = atof(argv[2]), depth = atof(argv[3]); int shape = atoi(argv[4]);
        setBusTremolo(0, true, rate, depth, shape);
        next = 5;
    } else if (!strcmp(effect, "phaser")) {
        float rate = atof(argv[2]), depth = atof(argv[3]), fb = atof(argv[4]), mix = atof(argv[5]);
        int stages = atoi(argv[6]);
        setBusPhaser(0, true, rate, depth, mix, fb, stages);
        next = 7;
    } else if (!strcmp(effect, "leslie")) {
        int speed = atoi(argv[2]);
        float drive = atof(argv[3]), balance = atof(argv[4]), doppler = atof(argv[5]), mix = atof(argv[6]);
        setBusLeslie(0, true, speed, drive, balance, doppler, mix);
        next = 7;
    } else if (!strcmp(effect, "grains")) {   // granular delay — its OWN path (processGranularDelay), not processBusEffects
        _ensureFxCtx();
        granDelay.grainSize = atof(argv[2]); granDelay.density = atof(argv[3]);
        granDelay.position  = atof(argv[4]); granDelay.scatter = atof(argv[5]);
        granDelay.feedback  = atof(argv[6]); granDelay.mix     = atof(argv[7]);
        granDelay.freeze = false; granDelay.enabled = true;
        next = 8;
    } else {
        fprintf(stderr, "unknown effect '%s' (add an else-if calling its setBus*)\n", effect);
        return 1;
    }
    if (argc > next)     out = argv[next];
    if (argc > next + 1) dur = atof(argv[next + 1]);
    if (argc > next + 2) hz  = atof(argv[next + 2]);

    int n = (int)(SAMPLE_RATE * dur);
    float dt = 1.0f / (float)SAMPLE_RATE;
    int is_grains = !strcmp(effect, "grains");
    short *buf = malloc(sizeof(short) * n);
    double ph = 0.0, inc = (double)hz / (double)SAMPLE_RATE;
    for (int i = 0; i < n; i++) {
        float s = (float)sin(ph * 2.0 * M_PI) * 0.5f;   // 0.5-amplitude sine
        ph += inc; if (ph >= 1.0) ph -= 1.0;
        float y = is_grains ? processGranularDelay(s, dt)   // navkit's genuine granular DSP
                            : processBusEffects(s, 0, dt);  // navkit's genuine bus DSP
        int v = (int)(y * 32767.0f);
        if (v > 32767) v = 32767; if (v < -32768) v = -32768;
        buf[i] = (short)v;
    }
    writeWavMono(out, buf, n);
    free(buf);
    printf("wrote %s: %d samples @ %dHz, %s, carrier %.2f Hz\n", out, n, SAMPLE_RATE, effect, hz);
    return 0;
}
