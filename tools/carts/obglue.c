/* de:meta
{
  "slug": "obglue",
  "title": "glue probe",
  "status": "active",
  "created": "2026-08-19",
  "kind": [
    "probe"
  ],
  "teaches": [
    "drum-synthesis"
  ],
  "lineage": "docs/design/analog-outboard-chain.md §2b measured glue() as a fast-attack slow-release DUCKER: at the hardest ratio it takes 3.8 dB of RMS while the peak does not move, so the crest factor RISES. §5.2 called the missing makeup the cheapest honesty win on the list, because without it switching the comp in makes the mix smaller and a level-matched A/B is impossible; §2 called program-dependent release the last honest piece of 'snappy'. This is the bench both were built and measured on: a loop with a SPARSE half and a WALL half, so the program dependence has something to depend on.",
  "description": {
    "summary": "A drum loop that plays four bars sparse and four bars as a wall, with the bus compressor switchable in and out. The meter reads RMS, because RMS is the number that moves: a peak meter shows this stage doing nothing at all.",
    "detail": "Two facts about a self-keyed bus compressor that a peak meter hides. First, it is not a limiter: the transient outruns even a 1 ms attack and passes through, and the follower then squashes the BODY behind it, so the peak stays where it was and the crest factor goes UP. That is the mix breathing as one lump, which is what a bus comp is for, and it is why the meter here is RMS with a peak-hold tick. Second, its recovery is program-dependent, which needs two kinds of program to see: the sparse half is single hits with air between them and recovers at the release time you set, while the wall half builds a second, slower follower and takes about four times as long to let go. The makeup is automatic: the stage averages the gain it is applying over about a second and a half and puts exactly that back, so switching it in no longer costs you level and the A/B is a fair one. The rack runs at about -7 dBFS peak on purpose. Feed a mastering stage a mix that already peaks at 0 dBFS and the master soft-clip absorbs everything it does, which is how you conclude a working stage is inert.",
    "controls": "SPACE switches the comp in and out, LEFT/RIGHT step the ratio (which sets amount, attack and release together), W cycles the program (sparse-then-wall / wall only / sparse only / the release bench); it plays itself otherwise"
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// GLUE PROBE — the bench glue()'s makeup and program-dependent release were measured on.
//
// MEASURE IT:
//   node tools/ab-render.js obglue --set glue_amt=0.0f,0.84f --frames 600 --keep
//   node tools/wav-analyze.js <each wav>        # peak / RMS / crest: RMS is the number that moves
// The claim to check is that the two RMS figures now MATCH (the makeup put the level back). Before
// the makeup leg they differed by 3.8 dB at this setting, and that gap is why an A/B of this stage
// used to be a comparison of two different volumes rather than of two different sounds.

#define K 5     // kick
#define S 6     // snare
#define H 7     // hat
#define B 8     // bass
#define T 9     // the RELEASE BENCH's steady tone (prog 3) — a quiet held sine under the bursts

static float glue_amt  = 0.44f;   // ab-render flips THIS. outboard.h's ratio table: 0.28/0.44/0.60/0.84
static int   glue_atk  = 7;
static int   glue_rel  = 190;
static int   ratio     = 1;
static int   comp_on   = 1;
// THE BYPASS CONTROL: 0 = never call glue() at all, which is the only way to prove that amount 0 is
// byte-identical to a build with no comp in it. A sha cannot answer that; it only says "differs".
static int   glue_wired = 1;
// 0 = four bars sparse then four bars wall (the A/B), 1 = wall only, 2 = sparse only,
// 3 = THE RELEASE BENCH. The program is a PARAMETER of this bench because the release is
// program-DEPENDENT: measuring it needs two different programs, not two different settings.
// Program 3 is the one that puts a NUMBER on a release: a quiet steady tone holds all the way
// through while a loud burst lands every two seconds, so the tone's amplitude IS the gain recovery
// curve and `node tools/wav-envelope.js` reads the recovery time straight off it. Aggregate RMS is
// the wrong measure for a release change (it moves by hundredths of a dB); the SHAPE is the claim.
static int   prog = 0;
static int   tone_h = -1;
static int   last_step = -1;
static int   step = 0;
static float meter = 0.0f, hold = 0.0f;

#define IN_DB 7.0f   // the rack's input drive into the comp, in dB (flat eq_inst boost)

static const float RATIO_AMT[4] = { 0.28f, 0.44f, 0.60f, 0.84f };
static const int   RATIO_ATK[4] = { 14, 7, 3, 1 };
static const int   RATIO_REL[4] = { 280, 190, 120, 55 };
static const char *RATIO_NAME[4] = { "4:1", "8:1", "12:1", "ALL" };

// SET-AND-HOLD: fired by a change only, never per frame (tools/lint-fx-frame.js).
static void apply_comp(void) {
    if (!glue_wired) return;
    glue(0, comp_on ? glue_amt : 0.0f, glue_atk, glue_rel);
}

void init(void) {
    bpm(112);
    instrument(K, INSTR_MEMBRANE, 1, 260, 0, 90);
    instrument(S, INSTR_NOISE,    1, 140, 0, 60);
    instrument(H, INSTR_NOISE,    1,  40, 0, 20);
    instrument(B, INSTR_SAW,      4, 200, 5, 120);
    instrument(T, INSTR_SINE,     8,   0, 7,  80);
    instrument_filter(B, FILTER_LOW, 900, 4);
    // HEADROOM (analog-outboard-chain.md §2c): a mastering stage on an already-clipped mix measures
    // nothing, because the always-on master soft-clip has eaten the difference first. The target is
    // about -7 dBFS peak on the rack out.
    instrument_level(K, 0.90f);
    instrument_level(S, 0.55f);
    instrument_level(H, 0.26f);
    instrument_level(B, 0.70f);
    instrument_level(T, 0.10f);   // the release bench's probe tone: quiet, so it never drives the comp itself
    // THE INPUT STAGE, and it is not optional: glue() keys off ABSOLUTE level (a FET comp has no
    // threshold, so you compress it by driving its input — see the doc §2). A quiet mix therefore
    // barely compresses at all, and the first version of this bench sat at -36 dBFS RMS where every
    // ratio measured the same. A flat eq_inst boost is the only gain-above-unity the engine has.
    // It stays IN for both sides of the A/B, so switching the comp is a test of the compressor and
    // not of an input knob.
    eq_inst(1, IN_DB, IN_DB, IN_DB);
    static const int chain[1] = { FX_INST(FX_EQ, 1) };
    fx_order(0, chain, 1);
    apply_comp();
}

// the program: 16 steps a bar, 8 bars. Bars 0-3 SPARSE (single hits with air between them, so the
// recovery is the fast one), bars 4-7 a WALL (something on every step, so the slow follower builds).
static void play_step(int st) {
    int bar = (st / 16) % 8;
    int s16 = st % 16;
    if (prog == 3) {                       // THE RELEASE BENCH: a steady tone + a loud burst
        if (tone_h < 0) tone_h = note_on(69, T, 7);
        if ((st % 16) == 0) { note(36, K, 7); note(62, S, 7); }
        return;
    }
    if (tone_h >= 0) { note_off(tone_h); tone_h = -1; }
    int wall = (prog == 1) || (prog == 0 && bar >= 4);
    if (s16 == 0 || (wall && s16 == 8)) note(36, K, 7);
    if (s16 == 8 || (wall && (s16 == 4 || s16 == 12))) note(62, S, 5);
    if (wall && (s16 % 2) == 0) note(84, H, 3);
    if (wall) { if ((s16 % 4) == 0) note(40 + (bar % 3) * 3, B, 6); }
    else      { if (s16 == 0)      note(40, B, 6); }
}

void update(void) {
    int st = (int)(beat() * 4 + beat_pos() * 4.0f);
    if (st != last_step) { last_step = st; step = st; play_step(st); }

    if (keyp(' ')) { comp_on = !comp_on; apply_comp(); }
    if (keyp('W') || keyp('w')) prog = (prog + 1) % 4;
    int r = ratio;
    if (keyp(KEY_LEFT)  && r > 0) r--;
    if (keyp(KEY_RIGHT) && r < 3) r++;
    if (r != ratio) {                      // SET-AND-HOLD: only the change reconfigures the stage
        ratio = r;
        glue_amt = RATIO_AMT[r]; glue_atk = RATIO_ATK[r]; glue_rel = RATIO_REL[r];
        apply_comp();
    }
#ifdef DE_TRACE
    watch("comp", "%d", comp_on);
    watch("amount", "%.2f", comp_on ? glue_amt : 0.0f);
    watch("wall", "%d", ((prog == 1) || (prog == 0 && ((step / 16) % 8) >= 4)) ? 1 : 0);
#endif
}

#define NS 512
static float sc[NS];

void draw(void) {
    cls(CLR_BLACK);
    scope_read(sc, NS);

    // THE RMS METER, with a peak-hold tick. §2b: a peak meter shows this stage doing NOTHING, which
    // is how you would wrongly conclude it is inert. RMS is the number that moves.
    float sum = 0.0f, pk = 0.0f;
    for (int i = 0; i < NS; i++) { sum += sc[i] * sc[i]; float a = fabsf(sc[i]); if (a > pk) pk = a; }
    float rms = sqrtf(sum / NS);
    meter += (rms - meter) * 0.20f;
    if (pk > hold) hold = pk; else hold -= 0.004f;
    if (hold < 0.0f) hold = 0.0f;

    int mx = 24, my = 40, mw = 26, mh = 108;
    rect(mx - 1, my - 1, mw + 2, mh + 2, CLR_DARK_GREY);
    int rh = (int)(meter * 3.2f * mh); if (rh > mh) rh = mh;
    rectfill(mx, my + mh - rh, mw, rh, comp_on ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
    int ph = (int)(hold * 1.0f * mh); if (ph > mh) ph = mh;
    line(mx, my + mh - ph, mx + mw - 1, my + mh - ph, CLR_ORANGE);
    font(FONT_SMALL);
    print("RMS", mx + 3, my + mh + 4, CLR_LIGHT_GREY);
    print("peak", mx + 1, my - 10, CLR_ORANGE);
    font(FONT_NORMAL);

    // the waveform, so the squashed body is visible next to the untouched transient
    int wx = 74, wy = 94, ww = SCREEN_W - wx - 12;
    line(wx, wy, wx + ww, wy, CLR_DARKER_GREY);
    for (int x = 1; x < ww; x++) {
        int i0 = (x - 1) * NS / ww, i1 = x * NS / ww;
        line(wx + x - 1, wy - (int)(sc[i0] * 42.0f), wx + x, wy - (int)(sc[i1] * 42.0f),
             comp_on ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
    }

    int px = 74, py = 18;
    print("GLUE PROBE", px, py, CLR_WHITE); py += 14;
    char s[64];
    snprintf(s, sizeof s, "comp %s   ratio %s", comp_on ? "IN " : "OUT", RATIO_NAME[ratio]);
    print(s, px, py, comp_on ? CLR_LIME_GREEN : CLR_MEDIUM_GREY); py += 10;
    snprintf(s, sizeof s, "amount %.2f  atk %dms  rel %dms", glue_amt, glue_atk, glue_rel);
    font(FONT_SMALL);
    print(s, px, py, CLR_LIGHT_GREY); py += 10;
    int wall = (prog == 1) || (prog == 0 && ((step / 16) % 8) >= 4);
    print(prog == 3 ? "program: RELEASE BENCH  (tone + burst)"
                    : (wall ? "program: WALL  (slow recovery)" : "program: SPARSE  (fast recovery)"),
          px, py, prog == 3 ? CLR_YELLOW : (wall ? CLR_ORANGE : CLR_BLUE));

    font(FONT_SMALL);
    print("the transient goes through; the BODY", 74, SCREEN_H - 34, CLR_LIGHT_GREY);
    print("gets squashed -- so watch RMS, not peak", 74, SCREEN_H - 26, CLR_LIGHT_GREY);
    print("SPACE comp   <> ratio   W program", 74, SCREEN_H - 14, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
