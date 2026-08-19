/* de:meta
{
  "slug": "smprobe",
  "title": "side man probe bench",
  "status": "active",
  "created": "2026-08-19",
  "kind": [
    "probe"
  ],
  "teaches": [
    "drum-synthesis",
    "analog-voice-modeling"
  ],
  "lineage": "The measurement bench for runtime/sideman.h (Wurlitzer Side Man, 1959). Fires each of the ten voices in isolation on a fixed frame grid so the spectral tools can read one hit at a time, then a whole-bank stack for headroom and two rolls for click hunting.",
  "description": {
    "summary": "A deterministic bench that fires all ten Wurlitzer Side Man voices in turn, then stacks them, then rolls them, so the audio oracles can measure the bank.",
    "detail": "Not a musical cart: it is the render rig behind runtime/sideman.h. The timeline is pinned to frame numbers so a headless render puts every voice at a known second: ten solo hits 0.75s apart, one whole-bank stack (the headroom test), a wooden roll of the four struck-wood voices, then a full-bank groove. Render it with play.js --wav and read the regions with harmonic-spec / inharm-spec / wav-envelope / click-check. Deliberately DRY, with no cabinet and no outboard chain, because the header's claim is that the fullness comes from the tube stage and the band limit rather than from a box.",
    "controls": "Keys 1-9 and 0 fire the ten voices by hand; SPACE stacks the whole bank; R runs the wooden roll. It plays its fixed timeline on its own with no input at all."
  }
}
de:meta */
#include "studio.h"
#include "sideman.h"
#include <stdio.h>

// ── SIDE MAN PROBE BENCH ──────────────────────────────────────────────────
// The measurement rig for runtime/sideman.h. Every event sits on a fixed frame
// so a headless render is a MAP: the analysis windows below are the contract
// between this cart and the numbers recorded in docs/design/sideman-voices.md.
//
//   frame       second   what
//   60+i*45     1.00 +   voice i alone, i = 0..9 (BASS..CYMBAL), 0.75s apart
//   570         9.50     the whole bank at once (peak / headroom)
//   630         10.50    wooden roll: WOOD/TEMP1/TEMP2/CLAVES 16ths, 24 hits
//   810         13.50    full-bank groove, 64 sixteenths
//
// Run:  node tools/play.js smprobe script /dev/null --headless --frames 1260 --wav out.wav

#define SOLO_F0   60
#define SOLO_STEP 45
#define STACK_F   570
#define WROLL_F   630
#define WROLL_N   24
#define WROLL_STEP 6
#define GROOVE_F  810
#define GROOVE_N  64
#define GROOVE_STEP 6

static int frames = 0;
static int last_voice = -1;
static int last_frame = -1000;

// the wooden roll, in order: block, temple I, temple II, claves
static const int WROLL[4] = { SM_WOOD, SM_TEMP1, SM_TEMP2, SM_CLAVES };

// a plain 4/4 the way a rhythm disc would read it, one row per voice, 16 steps
static const char *GROOVE[SM_NV] = {
    /* BASS    */ "x.......x.......",
    /* TOM I   */ "............x...",
    /* TOM II  */ "..........x.....",
    /* WOOD    */ "....x.......x...",
    /* TEMP I  */ "..x.......x.....",
    /* TEMP II */ "......x.....x...",
    /* CLAVES  */ "x..x..x...x..x..",
    /* BRUSH   */ "..x...x...x...x.",
    /* MARACAS */ "x.x.x.x.x.x.x.x.",
    /* CYMBAL  */ "x...............",
};

void init(void) {
    sideman_build(SIDEMAN_BASE);
}

static void fire(int v, int delay) {
    sideman_fire(SIDEMAN_BASE, v, 0, delay);
    last_voice = v;
    last_frame = frames;
}

void update(void) {
    // ── the fixed timeline ────────────────────────────────────────────────
    int f = frames;
    if (f >= SOLO_F0 && f <= SOLO_F0 + 9 * SOLO_STEP && (f - SOLO_F0) % SOLO_STEP == 0)
        fire((f - SOLO_F0) / SOLO_STEP, 0);

    if (f == STACK_F)
        for (int v = 0; v < SM_NV; v++) fire(v, 0);

    if (f >= WROLL_F && f < WROLL_F + WROLL_N * WROLL_STEP && (f - WROLL_F) % WROLL_STEP == 0)
        fire(WROLL[((f - WROLL_F) / WROLL_STEP) % 4], 0);

    if (f >= GROOVE_F && f < GROOVE_F + GROOVE_N * GROOVE_STEP && (f - GROOVE_F) % GROOVE_STEP == 0) {
        int step = ((f - GROOVE_F) / GROOVE_STEP) % 16;
        for (int v = 0; v < SM_NV; v++)
            if (GROOVE[v][step] == 'x') fire(v, 0);
    }

    // ── hand play, so a human can audition it in the editor ───────────────
    static const int KEYS[SM_NV] = { '1','2','3','4','5','6','7','8','9','0' };
    for (int v = 0; v < SM_NV; v++) if (keyp(KEYS[v])) fire(v, 0);
    if (keyp(' ')) for (int v = 0; v < SM_NV; v++) fire(v, 0);
    if (keyp('R') || keyp('r'))
        for (int i = 0; i < WROLL_N; i++) sideman_fire(SIDEMAN_BASE, WROLL[i % 4], 0, i * 100);

    frames++;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("SIDE MAN PROBE BENCH", 8, 8, CLR_ORANGE);
    char buf[64];
    snprintf(buf, sizeof buf, "frame %d  t %.2fs", frames, frames / 60.0f);
    print(buf, 8, 20, CLR_LIGHT_GREY);

    for (int v = 0; v < SM_NV; v++) {
        int lit = (v == last_voice && frames - last_frame < 8);
        snprintf(buf, sizeof buf, "%d %-16s", (v + 1) % 10, SIDEMAN_NAME[v]);
        print(buf, 8, 40 + v * 12, lit ? CLR_WHITE : CLR_DARK_GREY);
        if (lit) rectfill(150, 42 + v * 12, 40, 6, CLR_ORANGE);
    }
    print("1-0 voice  SPACE stack  R roll", 8, 176, CLR_MEDIUM_GREY);
}
