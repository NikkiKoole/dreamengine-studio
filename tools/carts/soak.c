#include "studio.h"
#include <stdio.h>

// SOAK — the engine's LONG-RUN stability self-test cart. It cycles a STRESS phase (rapid dense
// one-shot notes fired through a big reverb + feedback-echo tail, to fill the voice pool and the
// effect feedback buffers) and an IDLE phase (silence — the tails decay), ~24 times over ~64s.
// `node tools/soak-check.js` renders the whole run and asserts the things a short test can't see:
//
//   • IDLE windows return to TRUE SILENCE — no voice leak (a note_off that never frees its slot),
//     no tail that rings forever, no slow energy accumulation in a feedback loop.
//   • STRESS windows stay LEVEL across all cycles — late cycles ≈ early ones. A slow gain drift or
//     a progressive voice-pool starvation (from a leak) shows up as the level creeping cycle to cycle.
//   • Nothing blows up over the long haul — peak stays bounded, no NaN-collapse to silence.
//
//   node tools/soak-check.js            # render this cart (long) + the stability report
//
// Pairs with the denormal flush-to-zero in sound.h: this proves the tails decay to silence (the
// audible side); FTZ handles the audio-thread CPU side of a tail decaying into the denormal range.
// Pure timing harness — nothing to look at; run it headless via soak-check.js.

#define STRESS 60               // ~1.0s of dense note-firing
#define IDLE   100              // ~1.7s of silence — long enough for the reverb+echo tails to die
#define CYCLE  (STRESS + IDLE)
#define NCYC   24               // ~64s total
#define SLOT   5

// a deterministic pitch walk (no RNG — the run must be reproducible under --det)
static const int WALK[] = { 48, 52, 55, 59, 60, 64, 67, 71, 72, 67, 64, 60, 55, 52 };
#define NWALK ((int)(sizeof(WALK) / sizeof(WALK[0])))

static int fnum = -1;
static int nc = 0;              // note counter (drives the deterministic pitch walk)

void init(void) {
    instrument(SLOT, INSTR_PLUCK, 3, 30, 4, 600);   // a ringing pluck — a real decaying tail
    reverb(0.88f, 0.25f);   instrument_reverb(SLOT, 0.6f);   // big bright hall → long decay (set once)
    echo(360, 0.72f, 0.5f); instrument_echo(SLOT, 0.55f);    // feedback delay → long repeats (set once)
}

void update(void) {
    fnum++;
    int cyc    = fnum / CYCLE;
    int local  = fnum % CYCLE;
    int stress = (cyc < NCYC) && (local < STRESS);

    if (stress && (local % 3 == 0)) {           // ~20 notes/sec → overlap fills the 32-voice pool
        note(WALK[nc % NWALK], SLOT, 6);          // one-shot (250ms, ADSR releases itself — fire-and-forget)
        nc++;
    }

#ifdef DE_TRACE
    watch("cyc",   "%d", cyc);
    watch("phase", "%d", stress ? 1 : 0);        // 1 = stressing, 0 = idle/decay (the analyzer windows on this)
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    print("SOAK - run via tools/soak-check.js", 6, 6, CLR_WHITE);
    char buf[48];
    int cyc = fnum < 0 ? 0 : fnum / CYCLE;
    snprintf(buf, sizeof(buf), "cycle %d / %d", cyc < NCYC ? cyc : NCYC, NCYC);
    print(buf, 6, 20, CLR_LIGHT_GREY);
}
