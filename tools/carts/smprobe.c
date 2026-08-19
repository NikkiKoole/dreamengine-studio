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
  "lineage": "The measurement bench for runtime/sideman.h (Wurlitzer Side Man, 1959). Fires each of the ten voices in isolation on a fixed frame grid so the spectral tools can read one hit at a time, then a whole-bank stack for headroom and two rolls for click hunting. Also the A/B bench for the BRUSH, the one voice that failed the ear test: round two swept grain and the owner could not tell the three grain settings apart, so round three sweeps the BODY-to-TAIL relationship instead, which is what separates a brushed jazz snare from a drag.",
  "description": {
    "summary": "A deterministic bench for the ten Wurlitzer Side Man voices, and an A/B rig for four BRUSH candidates that span the body-to-tail relationship of a brushed jazz snare, audible solo and on a real backbeat.",
    "detail": "Not a musical cart: it is the render rig behind runtime/sideman.h. Two jobs. First the bank bench, whose timeline is pinned to frame numbers so a headless render puts every voice at a known second, then a whole-bank stack for headroom, a wooden roll, and a full-bank groove. Second the BRUSH A/B, now on its third round. The shipped voice reads as a woosh, which the numbers agreed with (its mid band sits 11.8 dB above both its ends). Round two split the spectrum and swept granular texture; the split was confirmed by ear but the three grain settings were indistinguishable to the listener despite measuring a 2x spread, so grain is now FIXED and the axis has moved. What the owner actually wants is a noise snare that fizzles out, a jazzy snare, and this machine has no snare voice at all, so the brush IS the snare and the wire rattle is central rather than decorative. Candidate 1 is round two winner-by-default as the control. Candidates 2, 3 and 4 are a brushed snare at three points on the body-to-tail axis: a tight tap whose body dominates, a short body under a long bright sizzling tail, and a short body under a two-stage tail that trails away gradually. They differ in gross structure (260, 700 and 950 ms) rather than in parameters, because round two proved parameter nudges are inaudible here. Deliberately DRY, with no cabinet and no outboard chain.",
    "controls": "SPACE starts and stops the groove. 1 2 3 4 pick the brush candidate and swap it live inside the groove, which is the judgement that counts; 0 fires the selected one on its own. Q W E R T Y U I O P play the ten bank voices by hand. It also plays a fixed measurement timeline on its own with no input at all."
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
// ── the brush A/B, round three ────────────────────────────────────────────
// The owner listened to the bank and passed nine voices. The tenth, verbatim:
// "sounds pretty good except the brush that doesn't sound like a brush." So the
// shipped recipe is a CONTROL, not a baseline to improve, and the other nine
// voices are a thing to protect: nothing in this file touches them.
//
// Two rounds of listening have narrowed it, and one of the two results is a
// negative one that is worth more than the positive one:
//
//   ROUND 1 (shipped): "sounds like a woosh." The numbers agreed and said WHY —
//     the shipped voice's mid band sits 11.8 dB ABOVE the quieter of its two
//     ends, so it lives in the nasal region that reads as filtered noise. The
//     fix (split the spectrum into a low body and a high fizz, scoop the middle)
//     is CONFIRMED BY EAR and is kept in every candidate below.
//
//   ROUND 2 (three grain settings): "the other 3 sound very similar." That is a
//     null result on a measured 2x spread, and it is the important finding here:
//     granular texture at 72/120 vs 156/260 Hz, and sample-and-hold vs sine,
//     measured 2.06x / 3.71x / 3.98x envelope modulation with a hard threshold
//     between 90 and 130 Hz, and NONE of it survived contact with an ear on this
//     material. The numbers were right and were not measuring what a listener
//     hears. Grain is therefore FIXED at round two's C setting and is not an axis
//     any more. Do not sweep it again; the finding is written down so nobody
//     re-derives it.
//
//   WHAT HE ACTUALLY WANTS: "a noise snare thing that fizzles out a bit more, a
//     jazzy snare." That is a JAZZ SNARE PLAYED WITH BRUSHES, and it kills the
//     "brushing means a sustained drag" inference that shaped round two. Three
//     structural consequences, and they are why round two's three collapsed into
//     one sound: all three were the same 375 ms gesture with a 63..89 ms attack.
//
//       - it is a HIT WITH A TAIL, not a symmetric drag. A brushed snare has a
//         soft but definite front, 5..20 ms, not 80.
//       - "fizzles out" is about the TAIL, and the tail is the point: a short
//         head body with the WIRES sizzling on underneath it at a lower level
//         for a good while after the body has gone. Two layers, two DIFFERENT
//         decays. This is the house snare shape (cr78's tonal shell + bandpassed
//         rattle, tr808.h/tr909.h's body + snappy), and since this machine has NO
//         snare voice at all, the rattle is central rather than decorative.
//       - his phrasing is ambiguous between "the tail should sizzle on longer"
//         and "it should trail away more gradually", so candidates 3 and 4 cover
//         one reading each rather than asking him.
//
// So the axis is now the BODY-to-TAIL relationship: how short and defined the
// body is, how long and how bright the wires sizzle behind it, and how the two
// levels sit against each other. After round two the steps are STRUCTURAL —
// 260 vs 700 vs 950 ms of total length — because parameter nudges are provably
// inaudible on this voice.
//
//   1  ROUND2 C   the control he has already heard: split body, drag envelope,
//                 fixed grain. Anchors "better than last round?" instead of
//                 re-litigating the woosh.
//   2  TAP        a brushed snare with a TIGHT tail: defined 8 ms front, short
//                 body, wires only a little longer, body-dominant. ~260 ms.
//   3  SIZZLE     short body, LONG BRIGHT wire tail sizzling on well past it,
//                 tail level near the body's. ~700 ms. ("sizzles on longer")
//   4  TRAIL      short body, then a TWO-STAGE tail: a medium rattle plus a very
//                 quiet, very long, very high sizzle, so the decay BENDS instead
//                 of running straight down. ~950 ms. ("trails away gradually")
//
// 2/3/4 also carry the thing that makes an ear say "snare" rather than "noise":
// a quiet tonal SHELL under the noise, two pitches like the house recipe, tuned
// into the bank's own F# (F#3 + C#4) so it belongs to this machine.
//
// One engine detail the grain relies on: the S&H generator is re-seeded per NOTE
// from a global counter (sound.h, lfo_mod[].seed = lfo_seed_ctr ^ ...), so every
// stroke rustles differently and a replay is still bit-identical.
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

// the brush A/B sections. Candidate 4's tail runs ~950 ms, so the solo spacing is
// 1.5s and the measurement windows below are that far apart.
#define CAND_F0    1380
#define CAND_STEP  90
#define CGROOVE_F  1800
#define CG_STEP    7             // frames per sixteenth (~128 BPM)
#define CG_BARS    2
#define CG_LEN     (16 * CG_STEP * CG_BARS)

// ── candidate slots, all above the bank ───────────────────────────────────
#define CB (SIDEMAN_BASE + SIDEMAN_NSLOT)
enum { SP_CLO, SP_CHI,          // 1  round two's C, the control
       SP_SHELL,                // shared: the quiet tonal head under 2/3/4
       SP_2BOD, SP_2WIR,        // 2  TAP     body-dominant, tight tail
       SP_3BOD, SP_3WIR,        // 3  SIZZLE  short body, long bright tail
       SP_4BOD, SP_4WIR, SP_4SIZ,   // 4  TRAIL  two-stage tail that bends
       SP_NSLOT };

// ── candidate 1: round two's C, kept verbatim as the control ──────────────
#define BR_LO_HZ    330          // the head's low body
#define BR_HI_HZ   4600          // the wire tips scraping
#define BR_LO_RES     5          // LOW on the body: noise, not a tone
#define BR_HI_RES    12          // more on the fizz, for the 6 kHz BAND LIMIT
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
#define BR_HI_DRIFT -1400.0f

// ── the GRAIN, now FIXED (round two: three settings, indistinguishable) ───
// A master multiplier so ab-render.js can still render any candidate with its
// modulator off, which is the only honest grain measurement: band-limited noise
// has its own envelope roughness, so candidates cannot be compared to each other.
#define BR_GRAIN_ON 1.0f
#define BR_GR_LO_RATE 156.0f
#define BR_GR_HI_RATE 260.0f
#define BR_GR_LO_DEP    0.45f
#define BR_GR_HI_DEP    0.75f

// ── the SHELL: the quiet tonal head that makes an ear say "snare" ─────────
// Two pitches like the house recipe (cr78 / tr808.h / tr909.h all do this), in the
// bank's own key so it belongs to this machine: F#3 and C#4, the toms' tuning.
#define BR_SHELL_LO   54
#define BR_SHELL_HI   61
#define BR_SHELL_VOL   3

// ── the BODY-to-TAIL axis: three structural points, not three parameter sets ──
// 2 TAP: a defined front and a tail barely longer than the body
#define T2_BOD_A     8
#define T2_BOD_D    85
#define T2_BOD_R    30
#define T2_BOD_HZ 1400
#define T2_WIR_A     4
#define T2_WIR_D   280
#define T2_WIR_R   100
#define T2_WIR_HZ 4600
// 3 SIZZLE: the same short body under a long BRIGHT tail at nearly its level
#define T3_BOD_A     6
#define T3_BOD_D    70
#define T3_BOD_R    25
#define T3_BOD_HZ 1400
#define T3_WIR_A    10
#define T3_WIR_D   440
#define T3_WIR_R   120
#define T3_WIR_HZ 4800
// 4 TRAIL: a medium rattle PLUS a very quiet very long very high sizzle, so the
// decay bends (two summed exponentials) instead of running straight down
#define T4_BOD_A     6
#define T4_BOD_D    65
#define T4_BOD_R    25
#define T4_BOD_HZ 1400
#define T4_WIR_A     8
#define T4_WIR_D   200
#define T4_WIR_R    90
#define T4_WIR_HZ 4600
#define T4_SIZ_A    30
#define T4_SIZ_D   900
#define T4_SIZ_R   420
#define T4_SIZ_HZ 5600

static const char *CAND_NAME[4] = { "1 ROUND2 C", "2 TAP", "3 SIZZLE", "4 TRAIL" };
static const char *CAND_NOTE[4] = {
    "drag 375ms  the control",
    "snare, tight tail  260ms",
    "snare, LOUD sizzle, stops",
    "snare, quiet, goes on",
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

// ── candidate 1: round two's C, verbatim ──────────────────────────────────
static void build_drag(void) {
    int lo = CB + SP_CLO, hi = CB + SP_CHI;
    instrument(lo, INSTR_NOISE, BR_LO_A, BR_LO_D, BR_LO_S, BR_LO_R);
    instrument_filter(lo, FILTER_BAND, BR_LO_HZ, BR_LO_RES);
    instrument_lfo(lo, 0, LFO_VOLUME, BR_GR_LO_RATE, BR_GR_LO_DEP * BR_GRAIN_ON);
    lfo_shape(lo, 0, LFO_SHAPE_SH);
    instrument(hi, INSTR_NOISE, BR_HI_A, BR_HI_D, BR_HI_S, BR_HI_R);
    instrument_filter(hi, FILTER_BAND, BR_HI_HZ, BR_HI_RES);
    instrument_lfo(hi, 0, LFO_VOLUME, BR_GR_HI_RATE, BR_GR_HI_DEP * BR_GRAIN_ON);
    lfo_shape(hi, 0, LFO_SHAPE_SH);
    instrument_env(hi, 0, ENV_CUTOFF, BR_HI_A, 260, BR_HI_DRIFT);
}

// ── the brushed-snare BODY: a struck head. Soft but DEFINITE front (round two's
// 63..89 ms attacks are why its three candidates were one gesture), and a low
// resonance band so it stays noise rather than becoming a tone.
static void build_body(int slot, int a, int d, int r, int hz) {
    instrument(slot, INSTR_NOISE, a, d, 0, r);
    instrument_filter(slot, FILTER_BAND, hz, 7);
}

// ── the WIRES: the sizzle that carries on after the body. Same fixed grain as
// candidate 1, and enough resonance to keep it inside the machine's 6 kHz channel.
static void build_wires(int slot, int a, int d, int r, int hz, int res, float rate, float dep, int shape) {
    instrument(slot, INSTR_NOISE, a, d, 0, r);
    instrument_filter(slot, FILTER_BAND, hz, res);
    instrument_lfo(slot, 0, LFO_VOLUME, rate, dep * BR_GRAIN_ON);
    lfo_shape(slot, 0, shape);
}

static void build_candidates(void) {
    build_drag();

    // the quiet tonal shell under 2/3/4 — the house snare's tonal half, which is
    // what makes an ear hear a drum rather than a burst of noise
    instrument(CB + SP_SHELL, INSTR_SINE, 2, 95, 0, 30);
    instrument_filter(CB + SP_SHELL, FILTER_LOW, 1100, 1);
    instrument_env(CB + SP_SHELL, 0, ENV_PITCH, 0, 18, 3.0f);

    build_body (CB + SP_2BOD, T2_BOD_A, T2_BOD_D, T2_BOD_R, T2_BOD_HZ);
    build_wires(CB + SP_2WIR, T2_WIR_A, T2_WIR_D, T2_WIR_R, T2_WIR_HZ, BR_HI_RES,     BR_GR_HI_RATE, BR_GR_HI_DEP, LFO_SHAPE_SH);
    build_body (CB + SP_3BOD, T3_BOD_A, T3_BOD_D, T3_BOD_R, T3_BOD_HZ);
    build_wires(CB + SP_3WIR, T3_WIR_A, T3_WIR_D, T3_WIR_R, T3_WIR_HZ, BR_HI_RES + 1, BR_GR_HI_RATE, BR_GR_HI_DEP, LFO_SHAPE_SH);
    build_body (CB + SP_4BOD, T4_BOD_A, T4_BOD_D, T4_BOD_R, T4_BOD_HZ);
    build_wires(CB + SP_4WIR, T4_WIR_A, T4_WIR_D, T4_WIR_R, T4_WIR_HZ, BR_HI_RES,     BR_GR_HI_RATE, BR_GR_HI_DEP, LFO_SHAPE_SH);
    // The long quiet trail gets LFO_SHAPE_RANDOM, not sample-and-hold: a smooth
    // filtered random walk is just as irregular but CONTINUOUS, and S&H's instant gain
    // steps measured 6.6x on the splice oracle here (three grainy layers overlapping in
    // the groove), at the bottom of the audible-click band. It is also the right physics
    // for this layer: many wires settling, not a rattle.
    build_wires(CB + SP_4SIZ, T4_SIZ_A, T4_SIZ_D, T4_SIZ_R, T4_SIZ_HZ, BR_HI_RES + 2, BR_GR_HI_RATE * 1.3f, BR_GR_HI_DEP, LFO_SHAPE_RANDOM);

    // the same tube stage the bank runs, so the A/B is about the RECIPE and not
    // about one candidate being the only clean one
    for (int i = 0; i < SP_NSLOT; i++) {
        instrument_drive_mode(CB + i, DRIVE_ASYM);
        instrument_drive(CB + i, SIDEMAN_TUBE * 0.50f);
    }
    // Levels: the four are trimmed to land on the SAME body loudness, measured, so
    // the listen is about structure and not about volume. Inside a candidate the
    // body:tail ratio is the POINT and is deliberately different in each.
    instrument_level(CB + SP_CLO,  0.85f);  instrument_level(CB + SP_CHI, 0.76f);
    instrument_level(CB + SP_SHELL, 0.55f);
    instrument_level(CB + SP_2BOD, 0.80f);  instrument_level(CB + SP_2WIR, 0.34f);
    instrument_level(CB + SP_3BOD, 0.74f);  instrument_level(CB + SP_3WIR, 0.50f);
    instrument_level(CB + SP_4BOD, 0.86f);  instrument_level(CB + SP_4WIR, 0.30f);
    instrument_level(CB + SP_4SIZ, 0.24f);
}

// fire brush candidate `c` (0 = round two's C .. 3 = TRAIL), `delay` ms from now
static void fire_cand(int c, int delay) {
    cand_flash = frames;
    if (c == 0) {
        schedule_hit(delay,     72, CB + SP_CLO, 6, BR_LO_GATE);
        schedule_hit(delay + 4, 72, CB + SP_CHI, 6, BR_HI_GATE);
        return;
    }
    // the shared tonal shell, two pitches, quiet
    schedule_hit(delay, BR_SHELL_LO, CB + SP_SHELL, BR_SHELL_VOL, 110);
    schedule_hit(delay, BR_SHELL_HI, CB + SP_SHELL, BR_SHELL_VOL - 1, 110);
    switch (c) {
        case 1:
            schedule_hit(delay,     72, CB + SP_2BOD, 6, 110);
            schedule_hit(delay + 2, 72, CB + SP_2WIR, 6, 320);
            break;
        case 2:
            schedule_hit(delay,     72, CB + SP_3BOD, 6,  95);
            schedule_hit(delay + 3, 72, CB + SP_3WIR, 6, 470);
            break;
        default:
            schedule_hit(delay,      72, CB + SP_4BOD, 6,  90);
            schedule_hit(delay + 2,  72, CB + SP_4WIR, 6, 300);
            schedule_hit(delay + 10, 72, CB + SP_4SIZ, 6, 950);
            break;
    }
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
    print("BRUSH A/B - the body-to-tail axis", 8, 22, CLR_LIGHT_PEACH);
    for (int c = 0; c < 4; c++) {
        int y = 34 + c * 13;
        int sel = (c == cand);
        int lit = sel && (frames - cand_flash < 10);
        rectfill(6, y - 2, 306, 11, sel ? CLR_DARK_PURPLE : CLR_DARK_BLUE);
        print(CAND_NAME[c], 10, y, sel ? CLR_WHITE : CLR_MEDIUM_GREY);
        print(CAND_NOTE[c], 96, y, sel ? CLR_LIGHT_GREY : CLR_DARK_GREY);
        if (lit) rectfill(86, y, 6, 7, CLR_ORANGE);
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
