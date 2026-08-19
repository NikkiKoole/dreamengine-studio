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

// the brush A/B sections. The winner's tail runs ~285 ms, the RING variant's shell
// ~260 ms, so 1.5s of solo spacing leaves every tail room to finish.
#define CAND_F0    1380
#define CAND_STEP  90
#define CGROOVE_F  1800
#define CG_STEP    7             // frames per sixteenth (~128 BPM)
#define CG_BARS    2
#define CG_LEN     (16 * CG_STEP * CG_BARS)

// ── candidate slots, all above the bank ───────────────────────────────────
#define CB (SIDEMAN_BASE + SIDEMAN_NSLOT)
enum { SP_V2SHELL, SP_V2BOD, SP_V2WIR,     // the PROMOTION PROOF: round three's
                                           // candidate 2 verbatim, in the probe's own
                                           // slots, so it can be diffed against the
                                           // shipped voice under identical conditions
       // the shell variants each get their OWN body and wires, not the bank's, so the
       // whole voice can be brought back down as the shell comes up. Otherwise the
       // shell just makes it LOUDER, and round two proved that is what gets heard.
       SP_S2SH, SP_S2BOD, SP_S2WIR,
       SP_S3SH, SP_S3BOD, SP_S3WIR,
       SP_S4SH, SP_S4BOD, SP_S4WIR,
       SP_NSLOT };

// ── the winner, verbatim, for the byte-identity proof ─────────────────────
// These are the numbers as the owner heard them. sideman.h now carries the same set;
// BRUSH_VERIFY swaps which of the two the timeline plays, and ab-render.js flipping it
// must produce a BYTE-IDENTICAL window or the promotion changed something.
#define BRUSH_VERIFY 0
#define V2_BOD_A     8
#define V2_BOD_D    85
#define V2_BOD_R    30
#define V2_BOD_HZ 1400
#define V2_BOD_RES   7
#define V2_WIR_A     4
#define V2_WIR_D   280
#define V2_WIR_R   100
#define V2_WIR_HZ 4600
#define V2_WIR_RES  12
#define V2_GR_RATE 260.0f        // the grain, FROZEN (round two: a null on three settings)
#define V2_GR_DEP    0.75f
#define V2_SHELL_LO  54
#define V2_SHELL_HI  61
#define V2_SHELL_VOL  3

// ── the one lever never swept: how present the tonal SHELL is ─────────────
// The shell is the difference between "noise with a tail" and "a drum being brushed".
// The shipped voice keeps it deliberately quiet (vol 3, level 0.55). These three are
// structural steps up from that, not nudges, because round two proved nudges are
// inaudible on this voice. The body and the wires come from the SHIPPED bank slots in
// all three, so the shell is genuinely the only variable.
// The axis is the shell-to-NOISE ratio, expressed by holding the shell where it ships
// and pulling the noise DOWN, then a per-variant makeup so all four end up the same
// loudness. MAKEUP is measured, not guessed (see the doc's table).
#define SH_LEVEL    0.55f        // the shipped shell level, unchanged in all three
#define SH2_NOISE   0.63f        // -4 dB of noise  = the shell +4 dB relative
#define SH2_MAKEUP  1.24f
#define SH3_NOISE   0.40f        // -8 dB of noise
#define SH3_MAKEUP  1.41f
#define SH4_NOISE   0.40f
#define SH4_MAKEUP  0.93f
#define SH4_DECAY 190            // 4 also RINGS: the drum pitch carries under the tail
#define SH4_REL    70

static const char *CAND_NAME[4] = { "1 SHIPPED", "2 SHELL+", "3 SHELL++", "4 RING" };
static const char *CAND_NOTE[4] = {
    "the one you picked",
    "shell +4dB vs the noise",
    "shell +8dB vs the noise",
    "shell +8dB, and rings",
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

// the PROMOTION PROOF copy: round three's candidate 2, exactly as it was heard
static void build_verify(void) {
    instrument(CB + SP_V2BOD, INSTR_NOISE, V2_BOD_A, V2_BOD_D, 0, V2_BOD_R);
    instrument_filter(CB + SP_V2BOD, FILTER_BAND, V2_BOD_HZ, V2_BOD_RES);
    instrument(CB + SP_V2WIR, INSTR_NOISE, V2_WIR_A, V2_WIR_D, 0, V2_WIR_R);
    instrument_filter(CB + SP_V2WIR, FILTER_BAND, V2_WIR_HZ, V2_WIR_RES);
    instrument_lfo(CB + SP_V2WIR, 0, LFO_VOLUME, V2_GR_RATE, V2_GR_DEP);
    lfo_shape(CB + SP_V2WIR, 0, LFO_SHAPE_SH);
    instrument(CB + SP_V2SHELL, INSTR_SINE, 2, 95, 0, 30);
    instrument_filter(CB + SP_V2SHELL, FILTER_LOW, 1100, 1);
    instrument_env(CB + SP_V2SHELL, 0, ENV_PITCH, 0, 18, 3.0f);
    instrument_level(CB + SP_V2BOD, 0.80f);
    instrument_level(CB + SP_V2WIR, 0.34f);
    instrument_level(CB + SP_V2SHELL, 0.55f);
}

// a shell VARIANT: its own shell + body + wires, identical to the shipped recipe except
// that the noise is scaled DOWN (that is the axis) and the whole thing is scaled by a
// makeup gain (so loudness is not the axis).
static void build_variant(int sh, int bod, int wir, float noise, float makeup, int decay, int rel) {
    instrument(sh, INSTR_SINE, 2, decay, 0, rel);
    instrument_filter(sh, FILTER_LOW, 1100, 1);
    instrument_env(sh, 0, ENV_PITCH, 0, 18, 3.0f);
    instrument_level(sh, SH_LEVEL * makeup);

    instrument(bod, INSTR_NOISE, V2_BOD_A, V2_BOD_D, 0, V2_BOD_R);
    instrument_filter(bod, FILTER_BAND, V2_BOD_HZ, V2_BOD_RES);
    instrument_level(bod, 0.80f * noise * makeup);

    instrument(wir, INSTR_NOISE, V2_WIR_A, V2_WIR_D, 0, V2_WIR_R);
    instrument_filter(wir, FILTER_BAND, V2_WIR_HZ, V2_WIR_RES);
    instrument_lfo(wir, 0, LFO_VOLUME, V2_GR_RATE, V2_GR_DEP);
    lfo_shape(wir, 0, LFO_SHAPE_SH);
    instrument_level(wir, 0.34f * noise * makeup);
}

static void build_candidates(void) {
    build_verify();
    build_variant(CB + SP_S2SH, CB + SP_S2BOD, CB + SP_S2WIR, SH2_NOISE, SH2_MAKEUP, 95, 30);
    build_variant(CB + SP_S3SH, CB + SP_S3BOD, CB + SP_S3WIR, SH3_NOISE, SH3_MAKEUP, 95, 30);
    build_variant(CB + SP_S4SH, CB + SP_S4BOD, CB + SP_S4WIR, SH4_NOISE, SH4_MAKEUP, SH4_DECAY, SH4_REL);
    // the same tube stage the bank runs on the brush, so a variant is not the only
    // clean one
    for (int i = 0; i < SP_NSLOT; i++) {
        instrument_drive_mode(CB + i, DRIVE_ASYM);
        instrument_drive(CB + i, SIDEMAN_TUBE * 0.50f);
    }
}

// fire brush candidate `c`: 0 = the SHIPPED voice, 1..3 = the shell variants, which
// reuse the shipped BODY and WIRES and swap only the shell. The call order matches
// sideman_fire exactly, because the engine seeds each note's sample-and-hold from a
// global counter and re-ordering the layers re-rolls the wire grain.
static void fire_cand(int c, int delay) {
    cand_flash = frames;
    if (c == 0) {
#if BRUSH_VERIFY
        schedule_hit(delay, V2_SHELL_LO, CB + SP_V2SHELL, V2_SHELL_VOL, 110);
        schedule_hit(delay, V2_SHELL_HI, CB + SP_V2SHELL, V2_SHELL_VOL - 1, 110);
        schedule_hit(delay,     72, CB + SP_V2BOD, 6, 110);
        schedule_hit(delay + 2, 72, CB + SP_V2WIR, 6, 320);
#else
        sideman_fire(SIDEMAN_BASE, SM_BRUSH, 0, delay);
#endif
        return;
    }
    int base3 = CB + SP_S2SH + (c - 1) * 3;
    schedule_hit(delay, SIDEMAN_SHELL_LO, base3, SIDEMAN_SHELL_VOL, SIDEMAN_SHELL_GATE);
    schedule_hit(delay, SIDEMAN_SHELL_HI, base3, SIDEMAN_SHELL_VOL - 1, SIDEMAN_SHELL_GATE);
    schedule_hit(delay,     72, base3 + 1, 6, 110);
    schedule_hit(delay + SIDEMAN_WIRE_MS, 72, base3 + 2, 6, SIDEMAN_WIRE_GATE);
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
    print("BRUSH A/B - how present is the shell", 8, 22, CLR_LIGHT_PEACH);
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
