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
  "lineage": "The measurement bench for runtime/sideman.h (Wurlitzer Side Man, 1959). Fires each of the ten voices in isolation on a fixed frame grid so the spectral tools can read one hit at a time, then a whole-bank stack for headroom and two rolls for click hunting. Now also the A/B bench for the BRUSH, the one voice that failed the ear test: four candidates sweeping the granular axis, solo and inside the real default rhythm.",
  "description": {
    "summary": "A deterministic bench for the ten Wurlitzer Side Man voices, and an A/B rig for four competing BRUSH recipes that sweep grain from a light rustle to a buzz, audible solo and inside a real pattern.",
    "detail": "Not a musical cart: it is the render rig behind runtime/sideman.h. Two jobs. First the bank bench, whose timeline is pinned to frame numbers so a headless render puts every voice at a known second, then a whole-bank stack for headroom, a wooden roll, and a full-bank groove. Second the BRUSH A/B: nine voices passed a listen and the brush did not, and the word the owner reached for was BRUSHING, so what is missing is the texture of many wire strands dragged across a head. That texture is GRAIN, and no amount of smooth filtered noise under a smooth envelope can make it, so grain is the axis the candidates sweep rather than a feature one of them has. A is the shipped recipe, kept as the control so different stays distinguishable from better. B, C and D share one body (a low head thump and a high tip fizz with the nasal middle scooped, at low resonance because a brush is close to pitchless) and one envelope (a long sustained drag with a plateau rather than a softened hit), and differ only in the modulation: B is a light slow rustle, C is heavy and fast up near the boundary where texture becomes buzz, and D is C's exact rate and depth with a sine instead of sample-and-hold, so the shape question gets its own answer. Deliberately DRY, with no cabinet and no outboard chain.",
    "controls": "SPACE starts and stops the groove. 1 2 3 4 pick the brush candidate and swap it live inside the groove, which is the judgement that matters; 0 fires the selected one on its own. Q W E R T Y U I O P play the ten bank voices by hand. It also plays a fixed measurement timeline on its own with no input at all."
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
//   frame        second   what
//   60+i*45      1.00 +   voice i alone, i = 0..9 (BASS..CYMBAL), 0.75s apart
//   570          9.50     the whole bank at once (peak / headroom)
//   630          10.50    wooden roll: WOOD/TEMP1/TEMP2/CLAVES 16ths, 24 hits
//   810          13.50    full-bank groove, 64 sixteenths
//   1380+i*75    23.00 +  BRUSH CANDIDATE i alone, i = 0..3 (A..D), 1.25s apart
//   1740+i*224   29.00 +  the default rhythm, 2 bars, candidate i on the brush row
//
// Run:  node tools/play.js smprobe script /dev/null --headless --frames 2680 --wav out.wav
//
// ── the brush A/B ─────────────────────────────────────────────────────────
// The owner listened to the bank and passed nine voices. The tenth: "sounds
// pretty good except the brush that doesn't sound like a brush", and then, asked
// what he expected: "yeah im expecting a kind of brushing sound indeed."
// So the shipped recipe is the CONTROL here, not a baseline to improve, and the
// other nine voices are a thing to protect: nothing in this file touches them.
//
// BRUSHING is the word that decides the design. It rules out the snare reading
// (a brush-shaped tap on the backbeat) and it names what is actually missing:
//
//   GRAIN. A brush is dozens of individual wire strands each making its own tiny
//   scrape across a surface, and smooth filtered noise under a smooth envelope
//   physically cannot produce that. The engine already does this trick — cr78's
//   guiro chops bandpassed noise with LFO_VOLUME at 36 Hz for a ratchet — and a
//   brush wants the same mechanism faster and shallower so it reads as texture
//   rather than as a ratchet. Which means grain is the AXIS the candidates sweep,
//   not a feature one of them has.
//
// Two supporting changes B/C/D all share, so the sweep is the only variable:
//
//   the SPECTRUM is split. A brush on a head is spectrally extreme: a low body
//   where the head is displaced and a high fizz where the tips scrape, with the
//   middle comparatively scooped. The shipped voice lives at 1150 Hz, which is
//   the nasal region that reads as "filtered noise" to an ear — so its whole
//   measured 1812->855 Hz arc travels inside the wrong place. Resonance is LOW
//   here (5, against the bank's 11-13) because high resonance makes noise tonal
//   and a brush is close to pitchless.
//
//   the ENVELOPE is a DRAG, not a softened hit. 26 ms into a 155 ms body is still
//   percussion shape. These sustain: a slow rise, a plateau that lasts while the
//   stroke is happening, and a slow fall — about 380 ms end to end.
//
// The four:
//   A  SHIPPED   the control, untouched
//   B  RUSTLE    split body + drag envelope + grain at 72/120 Hz S&H — MEASURED as
//                the milder pole: the metric's response to rate has a knee between
//                90 and 130 Hz, which is very likely the perceptual boundary too
//                (below it an amplitude S&H flutters, above it it roughens)
//   C  GRAIN     the same at 156/260 Hz S&H — the far side, deliberately past the
//                knee, where texture starts becoming a buzz
//   D  SINE      C's exact rate and depth with a SINE shape, because regular
//                modulation buzzes where irregular modulation rustles, and that
//                difference likely matters more than band placement does
//
// One engine detail that makes this work: the S&H generator is re-seeded per NOTE
// from a global counter (sound.h, lfo_mod[].seed = lfo_seed_ctr ^ ...), so every
// stroke rustles DIFFERENTLY and a replay is still bit-identical. A brush whose
// every stroke was the same grain would read as a machine.
//
// Candidate slots start above the bank (base 9 + SIDEMAN_NSLOT), so the bank is
// untouched and a candidate can be lifted into sideman.h verbatim once one wins.

#define SOLO_F0   60
#define SOLO_STEP 45
#define STACK_F   570
#define WROLL_F   630
#define WROLL_N   24
#define WROLL_STEP 6
#define GROOVE_F  810
#define GROOVE_N  64
#define GROOVE_STEP 6

// the brush A/B sections. A drag is ~380 ms, so the solo spacing is wider than
// the bank's and the measurement windows below are 1.25s apart.
#define CAND_F0    1380
#define CAND_STEP  75
#define CGROOVE_F  1740
#define CG_STEP    7             // frames per sixteenth (~128 BPM)
#define CG_BARS    2
#define CG_LEN     (16 * CG_STEP * CG_BARS)

// ── candidate slots, all above the bank ───────────────────────────────────
#define CB (SIDEMAN_BASE + SIDEMAN_NSLOT)
enum { SP_BLO, SP_BHI,     // B  light slow grain
       SP_CLO, SP_CHI,     // C  heavy fast grain
       SP_DLO, SP_DHI,     // D  the same, sine-shaped
       SP_NSLOT };

// ── the shared body, and the grain axis ──────────────────────────────────
// Named so ab-render.js can sweep any one of them without touching the rest.
#define BR_LO_HZ    330          // the head's low body
#define BR_HI_HZ   4600          // the wire tips scraping
// LOW resonance on the body, because high resonance makes noise tonal and a brush
// is close to pitchless. The FIZZ needs more, not for tone but for the BAND LIMIT:
// this machine's channel stops at 6 kHz, and a wide band at 5.2 kHz leaks 77% of its
// energy above that, which is hiss and not a brush. Same lesson the maracas and
// cymbal already taught this bank.
#define BR_LO_RES     5
#define BR_HI_RES    12
// the drag envelope: rise, plateau, fall (sustain > 0 = it HOLDS while gated)
#define BR_LO_A      70
#define BR_LO_D      90
#define BR_LO_S       4
#define BR_LO_R     170
#define BR_HI_A      45
#define BR_HI_D     110
#define BR_HI_S       3
#define BR_HI_R     130
#define BR_LO_GATE  220
#define BR_HI_GATE  140
// the grain: B light and slow, C heavy and fast, D = C with a sine
#define BR_B_RATE    120.0f
#define BR_B_LO_RATE  (BR_B_RATE * 0.6f)
#define BR_B_HI_RATE  BR_B_RATE
#define BR_B_LO_DEP    0.55f
#define BR_B_HI_DEP    0.65f
#define BR_C_RATE    260.0f
#define BR_C_LO_RATE (BR_C_RATE * 0.6f)
#define BR_C_HI_RATE BR_C_RATE
#define BR_C_LO_DEP    0.45f
#define BR_C_HI_DEP    0.75f
// A master multiplier on every grain depth, so ab-render.js can render the SAME
// candidate with its modulator off. That A/B is the only honest grain measurement:
// band-limited noise has its own envelope roughness (candidate A, which has no
// modulator at all, reads 0.49 on the depth metric), so comparing candidates to
// each other measures bandwidth as much as texture. Compare a candidate to itself.
#define BR_GRAIN_ON 1.0f
// the tips slow down as the brush lifts, so the fizz band drifts DOWN over the
// stroke — the drag's travel, without an impact to supply it
#define BR_HI_DRIFT -1400.0f

static const char *CAND_NAME[4] = { "A SHIPPED", "B RUSTLE", "C GRAIN", "D SINE" };
static const char *CAND_NOTE[4] = {
    "1150+3100, no grain",
    "split+drag, 72/120 S&H",
    "split+drag, 156/260 S&H",
    "C's rate+depth, SINE",
};

static int frames = 0;
static int cand = 0;              // which candidate is selected
static int groove_on = 0;
static int groove_f0 = 0;
static int last_voice = -1;
static int last_frame = -1000;
static int cand_flash = -1000;

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

// FOXTROT 4 BEAT, the cart's default rhythm and the one the brush has to work in.
// The brush row is played by the SELECTED CANDIDATE, everything else by the bank.
static const char *FOX4_BASS   = "x...x...x...x...";
static const char *FOX4_BRUSH  = "....x.......x...";
static const char *FOX4_WOOD   = "..x...x...x...x.";
static const char *FOX4_CYMBAL = "x...............";

// one candidate = a LOW body slot + a HIGH fizz slot, identical except for the
// grain on each. Written once so the three differ only in what is passed in.
static void build_pair(int lo, int hi, float loRate, float loDep,
                       float hiRate, float hiDep, int shape) {
    instrument(lo, INSTR_NOISE, BR_LO_A, BR_LO_D, BR_LO_S, BR_LO_R);
    instrument_filter(lo, FILTER_BAND, BR_LO_HZ, BR_LO_RES);
    instrument_lfo(lo, 0, LFO_VOLUME, loRate, loDep * BR_GRAIN_ON);
    lfo_shape(lo, 0, shape);

    instrument(hi, INSTR_NOISE, BR_HI_A, BR_HI_D, BR_HI_S, BR_HI_R);
    instrument_filter(hi, FILTER_BAND, BR_HI_HZ, BR_HI_RES);
    instrument_lfo(hi, 0, LFO_VOLUME, hiRate, hiDep * BR_GRAIN_ON);
    lfo_shape(hi, 0, shape);
    instrument_env(hi, 0, ENV_CUTOFF, BR_HI_A, 260, BR_HI_DRIFT);
}

static void build_candidates(void) {
    build_pair(CB + SP_BLO, CB + SP_BHI, BR_B_LO_RATE, BR_B_LO_DEP,
               BR_B_HI_RATE, BR_B_HI_DEP, LFO_SHAPE_SH);
    build_pair(CB + SP_CLO, CB + SP_CHI, BR_C_LO_RATE, BR_C_LO_DEP,
               BR_C_HI_RATE, BR_C_HI_DEP, LFO_SHAPE_SH);
    build_pair(CB + SP_DLO, CB + SP_DHI, BR_C_LO_RATE, BR_C_LO_DEP,
               BR_C_HI_RATE, BR_C_HI_DEP, LFO_SHAPE_SINE);

    // the same tube stage the bank runs, so the A/B is about the RECIPE and not
    // about one candidate being the only clean one
    for (int i = 0; i < SP_NSLOT; i++) {
        instrument_drive_mode(CB + i, DRIVE_ASYM);
        instrument_drive(CB + i, SIDEMAN_TUBE * 0.50f);
    }
    // A chopped voice loses average level, so the grainy ones are trimmed UP to
    // land on the shipped brush's loudness: the A/B is only fair if the four are
    // the same volume (measured, see docs/design/sideman-voices.md).
    instrument_level(CB + SP_BLO, 0.86f);  instrument_level(CB + SP_BHI, 0.71f);
    instrument_level(CB + SP_CLO, 0.85f);  instrument_level(CB + SP_CHI, 0.76f);
    instrument_level(CB + SP_DLO, 0.90f);  instrument_level(CB + SP_DHI, 0.79f);
}

// fire brush candidate `c` (0=A shipped .. 3=D sine), `delay` ms from now
static void fire_cand(int c, int delay) {
    if (c == 0) { sideman_fire(SIDEMAN_BASE, SM_BRUSH, 0, delay); cand_flash = frames; return; }
    int lo = CB + SP_BLO + (c - 1) * 2;
    schedule_hit(delay,     72, lo,     6, BR_LO_GATE);
    schedule_hit(delay + 4, 72, lo + 1, 6, BR_HI_GATE);
    cand_flash = frames;
}

void init(void) {
    sideman_build(SIDEMAN_BASE);
    build_candidates();
}

static void fire(int v, int delay) {
    sideman_fire(SIDEMAN_BASE, v, 0, delay);
    last_voice = v;
    last_frame = frames;
}

// one sixteenth of FOXTROT 4 BEAT, brush row played by candidate `c`
static void fox4_step(int step, int c) {
    if (FOX4_BASS[step]   == 'x') fire(SM_BASS, 0);
    if (FOX4_WOOD[step]   == 'x') fire(SM_WOOD, 0);
    if (FOX4_CYMBAL[step] == 'x') fire(SM_CYMBAL, 0);
    if (FOX4_BRUSH[step]  == 'x') fire_cand(c, 0);
}

void update(void) {
    int f = frames;

    // ── the fixed measurement timeline ────────────────────────────────────
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

    // the four candidates alone, 1.25s apart (a drag is ~380 ms)
    if (f >= CAND_F0 && f <= CAND_F0 + 3 * CAND_STEP && (f - CAND_F0) % CAND_STEP == 0) {
        cand = (f - CAND_F0) / CAND_STEP;
        fire_cand(cand, 0);
    }

    // the four candidates inside the real rhythm, two bars each
    if (f >= CGROOVE_F && f < CGROOVE_F + 4 * CG_LEN) {
        int rel = f - CGROOVE_F, c = rel / CG_LEN, in = rel % CG_LEN;
        if (in % CG_STEP == 0) { cand = c; fox4_step((in / CG_STEP) % 16, c); }
    }

    // ── hand play, so the owner can A/B it in the editor ──────────────────
    static const int CKEY[4]      = { '1', '2', '3', '4' };
    static const int VKEY[SM_NV]  = { 'Q','W','E','R','T','Y','U','I','O','P' };
    for (int c = 0; c < 4; c++)
        if (keyp(CKEY[c])) { cand = c; if (!groove_on) fire_cand(c, 0); }
    if (keyp('0')) fire_cand(cand, 0);
    for (int v = 0; v < SM_NV; v++) if (keyp(VKEY[v])) fire(v, 0);
    if (keyp(KEY_SPACE)) { groove_on = !groove_on; groove_f0 = frames + 1; }

    // the live groove: the same FOXTROT 4 BEAT, so switching candidate mid-loop
    // is the judgement that matters (a brush inside a pattern, not alone)
    if (groove_on) {
        int rel = frames - groove_f0;
        if (rel >= 0 && rel % CG_STEP == 0) fox4_step((rel / CG_STEP) % 16, cand);
    }

    frames++;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("SIDE MAN PROBE BENCH", 8, 6, CLR_ORANGE);
    char buf[64];
    snprintf(buf, sizeof buf, "f %d  t %.2fs", frames, frames / 60.0f);
    print(buf, 232, 6, CLR_DARK_GREY);

    // ── the brush A/B, the point of this cart right now ───────────────────
    print("BRUSH A/B - the grain axis", 8, 22, CLR_LIGHT_PEACH);
    for (int c = 0; c < 4; c++) {
        int y = 34 + c * 13;
        int sel = (c == cand);
        int lit = sel && (frames - cand_flash < 10);
        rectfill(6, y - 2, 306, 11, sel ? CLR_DARK_PURPLE : CLR_DARK_BLUE);
        snprintf(buf, sizeof buf, "%d %s", c + 1, CAND_NAME[c]);
        print(buf, 10, y, sel ? CLR_WHITE : CLR_MEDIUM_GREY);
        print(CAND_NOTE[c], 108, y, sel ? CLR_LIGHT_GREY : CLR_DARK_GREY);
        if (lit) rectfill(98, y, 6, 7, CLR_ORANGE);
    }
    print(groove_on ? "GROOVE RUNNING - swap 1-4 while it plays"
                    : "SPACE starts the groove   0 = alone",
          8, 90, groove_on ? CLR_GREEN : CLR_MEDIUM_GREY);

    // ── the bank, on QWERTYUIOP ───────────────────────────────────────────
    print("THE BANK - these nine passed a listen", 8, 106, CLR_LIGHT_PEACH);
    static const char *KL = "QWERTYUIOP";
    for (int v = 0; v < SM_NV; v++) {
        int col = v / 5, row = v % 5;
        int x = 10 + col * 150, y = 120 + row * 12;
        int lit = (v == last_voice && frames - last_frame < 8);
        snprintf(buf, sizeof buf, "%c %s", KL[v], SIDEMAN_SHORT[v]);
        print(buf, x, y, lit ? CLR_WHITE : CLR_DARK_GREY);
        if (lit) rectfill(x + 62, y, 22, 7, CLR_ORANGE);
    }
    print("1-4 BRUSH   SPACE GROOVE   0 SOLO", 8, 186, CLR_MEDIUM_GREY);
}
