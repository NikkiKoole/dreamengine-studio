/* de:meta
{
  "title": "tr-606 drumatix",
  "status": "active",
  "created": "2026-07-06",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "subtractive-synth",
    "additive-synth",
    "drum-synthesis",
    "analog-voice-modeling"
  ],
  "lineage": "Roland TR-606 (1981), the TB-303's silver sibling — completes the Roland classic-machine family (cr78/tr808/tr909/tb303/sh101). Metal bank + dual-bandpass cymbal modeled from circuit-analysis MEASURED values; kick/snare/toms are the documented circuit topologies (double twin-T + TONE, single twin-T + snappy, resonator + rumble) ear-tuned to reference. Chassis copied from tr808.c.",
  "homage": "Roland TR-606 Drumatix (1981)",
  "description": "The tin insect — Roland's TR-606 Drumatix (1981), sold as the TB-303's matching silver sibling (\"your rhythm section in two boxes\"). Thin snappy kick, papery snare, and the jewel-box hats half of electro and all of Aphex Twin ran on. Built from the real circuit: the six-oscillator metal bank at the circuit-analysis frequencies (246.4/308/367/418.2/440.4/627.2 Hz — note the beating 418+440 pair), hats and cymbal shimmer through the schematic's 7.1kHz BANDPASS (not the 808's highpass) with the cymbal body through its own 3.44kHz path, the kick as the schematic's DOUBLE twin-T (two detuned resonators beating, TONE knob crossfades them — no 808 boom), single-mode twin-T snare under bright snappy noise, toms with the low rumble burst, and two documented quirks: the open hat's decay is TEMPO-LINKED (faster song = shorter ring, like the hardware), and the closed hat fires the shut-off circuit (chokes the open hat). Stock panel is austere like the real thing — volume and ACCENT were the only controls — so the per-voice TUNE/DECAY/character knobs hide behind MODS, the drilled-pots mod culture as a toggle. Six presets (Drumatix, Acid Pair, Skitter, Electro, Tom Dance, Sparse), accent row, anachronistic swing. Q-U play voices, LEFT/RIGHT preset, UP/DOWN tempo, SPACE start/stop, Z/X swing, M mods, MOUSE edits the grid / accent strip / knobs."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── TR-606 DRUMATIX ───────────────────────────────────────────────────────
// The TB-303's silver sibling (1981) — sold as a DIN-sync'd pair, "your bass
// player and drummer in two boxes." Where the 808 is round and boomy the 606
// is thin, wiry and fast: a tin insect that thinks it's a drummer. Detroit
// electro, the acid revival, and Aphex Twin ran on it. Sibling cart to
// tr808.c (same editable grid + swing knob); all-analog, and the metal side
// has been circuit-analyzed to component level, so those values are MEASURED:
//
//   the metal bank — cymbal and hats share ONE bank of six Schmitt-trigger
//     squares at 246.4 / 308.0 / 367.0 / 418.2 / 440.4 / 627.2 Hz (MIDI
//     59/63/66/68/69/75 — baratatronix circuit analysis). Much lower and
//     tighter-clustered than the 808's bank; the 418+440 pair BEATS, and the
//     sizzle comes from the filters, not the oscillators:
//   dual bandpass — the 606 splits the bank into TWO bridged-T bandpasses:
//     7100 Hz (hi-hats + cymbal shimmer) and 3440 Hz (cymbal body). Note:
//     BANDpass — the 808 cart's hats use a highpass; the 606's narrower
//     window is why its hats sound more delicate. Cymbal = both paths at
//     once through two VCAs (body long, shimmer shorter), TONE crossfades.
//   open hat — decay time is TEMPO-LINKED on the real unit (a service-notes
//     quirk: faster song = shorter ring). fire() derives it from step time.
//     The closed hat triggers the schematic's "shut off circuit" — here
//     instrument_choke(), which IS that circuit.
//   kick — the schematic is a DOUBLE twin-T: two damped resonators at
//     different tunings, started in phase by the trigger, beating as they
//     ring; the TONE pot crossfades their mix. No 808-style boom. Exact
//     tunings are unpublished — ours (65 + 87 Hz, MIDI 36 + 41) are
//     ear-tuned to reference recordings; the tiny pitch blip fakes the
//     trigger thump.
//   snare — ONE twin-T mode (unlike the 808's two) + the transistor noise
//     source gated through a highpass ("snappy"). Body ~233 Hz, ear-tuned;
//     noise brighter than the 808's (HP 2400 vs 1800) — the paper.
//   toms — resonator with a pitch drop + the documented LOW-passed noise
//     "rumble burst" (the 808 cart's tom thud is bandpassed; the 606's is
//     a low rumble).
//   accent — one global accent per step, like the hardware (its accent is
//     a hefty +5V→14V trigger swing; flagged steps play louder).
//   swing — anachronistic here too (the 606 never had it). Z/X, 50..66%.
//   MODS — the stock panel had NO per-voice controls: volume + ACCENT, done.
//     That austerity is the character, so the panel ships stock; the M key
//     reveals TUNE/DECAY/character knobs — the most-modded drum machine in
//     history (drilled pots), as a toggle.
//
//   Q W E R T Y U   play each voice by hand
//   LEFT / RIGHT  preset (or tap the < > arrows)   UP / DOWN  tempo
//   SPACE  start / stop      Z / X  swing down / up      M  mods on/off
//   MOUSE  click/drag cells, accent strip above, drag knobs (when modded)

// voices — named indices, never raw numbers (house rule)
enum { V_BD, V_SD, V_LT, V_HT, V_CY, V_OH, V_CH, NV };

// instrument slots (9..24 are ours; two kick resonators, two cymbal paths)
enum {
    SL_BD1 = 9, SL_BD2, SL_SDB, SL_SDN, SL_TOM, SL_TOMN,
    SL_CYH, SL_CYL, SL_HATO, SL_HATC
};

// the metal bank, MIDI: 246.4/308.0/367.0/418.2/440.4/627.2 Hz
enum { MB1 = 59, MB2 = 63, MB3 = 66, MB4 = 68, MB5 = 69, MB6 = 75 };

static const char *VNAME[NV] = {
    "KICK", "SNAR", "LTOM", "HTOM", "CYMB", "OHAT", "CHAT"
};
static const char VKEY[NV] = { 'Q', 'W', 'E', 'R', 'T', 'Y', 'U' };

#define STEPS 16

#define GX   96    // grid left edge
#define GY   56    // grid top edge
#define SX   13    // column stride
#define SY   15    // row stride (7 rows = finger-sized)
#define CH2  11    // cell height

// per-voice knobs (MODS only): 8×8 rotaries in the label column
#define K0X  60
#define K1X  70
#define K2X  80
#define KW    8

typedef struct {
    const char *name;
    int         tempo;
    const char *row[NV];   // 16 chars, 'x' = hit; NULL = silent row
    const char *accent;    // 16 chars, 'x' = louder step
    int         swing;     // 50 = straight .. 66 = triplet (0 = straight)
} Pat;

static const Pat PRESET[] = {
    { "DRUMATIX", 128, {           // the box's own demo groove
        [V_BD] = "x...x...x...x...",
        [V_SD] = "....x.......x...",
        [V_CH] = "x.x.x.x.x.x.x...",
        [V_OH] = "..............x.",
      }, "x.......x......." },
    { "ACID PAIR", 134, {          // the 303's other half — Hardfloor floor
        [V_BD] = "x...x...x...x...",
        [V_SD] = "....x.......x...",
        [V_CH] = "x...x...x...x...",
        [V_OH] = "..x...x...x...x.",
      }, "x...x...x...x..." },
    { "SKITTER", 156, {            // braindance — accents carve the 16ths
        [V_BD] = "x.....x..x......",
        [V_SD] = "....x......x..x.",
        [V_CH] = "xxxxxxxxxxxxxxxx",
        [V_OH] = ".......x........",
      }, "..x..x.x..x..x.." },
    { "ELECTRO", 118, {            // Detroit — tom cascade at the turn
        [V_BD] = "x..x......x.....",
        [V_SD] = "....x.......x...",
        [V_CH] = "x.x.x.x.x.x.x.x.",
        [V_HT] = ".............x..",
        [V_LT] = "..............x.",
      }, "x.......x......." },
    { "TOM DANCE", 124, {          // the sweet analog toms up front
        [V_BD] = "x.......x.......",
        [V_SD] = "....x.......x...",
        [V_LT] = "...x..x......x..",
        [V_HT] = ".x......x..x....",
        [V_CH] = "x.x.x.x.x.x.x.x.",
      }, "x.......x.......", 54 },
    { "SPARSE", 100, {             // dubby space — the cymbal gets room
        [V_BD] = "x.........x.....",
        [V_SD] = "....x.......x...",
        [V_CH] = "x..x..x..x..x..x",
        [V_OH] = "..............x.",
        [V_CY] = "x...............",
      }, "x.......x.......", 58 },
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int  pre      = 0;
static int  tempo    = 128;
static bool running  = true;
static bool modded   = false;  // stock panel = no knobs, like the hardware
static int  last16   = -1;
static int  playhead = 0;
static int  flash[NV];
static bool grid[NV][STEPS];   // the live pattern — editable, loaded from preset
static int  nextz_x = -1;      // the preset '>' tap zone, recorded by draw()
static bool gacc[STEPS];       // live accent row
static bool paint_val;
static int  swing = 50;

static float ktune[NV];        // 0..1, center=0.5 → ±12 semitones
static float kdecay[NV];       // 0..1, center=0.5 → 1× scale (0→0.25×, 1→4×)
static float kcolor[NV];       // 0..1, center=0.5 → voice-specific character
static int   drag_v = -1, drag_k = -1;
static int   drag_y0, drag_x0;
static int   hover_v = -1, hover_k = -1;

static const char *KLABEL[2] = { "TUNE", "DECAY" };
static const char *K2LABEL[NV] = {
    "TONE", "SNPY", "RMBL", "RMBL",  // BD (osc mix), SD (body/noise), toms
    "TONE", NULL,   NULL,            // CY (body/shimmer), OH, CH
};

// 8×8 rotary knob: circle outline + single tick pixel showing value position
static void draw_knob(int x, int y, float val, int col) {
    int cx = x + 3, cy = y + 3;
    circ(cx, cy, 3, CLR_DARK_GREY);
    float a = (135.0f + val * 270.0f) * (3.14159265f / 180.0f);
    pset(cx + (int)(cosf(a) * 2.5f + 0.5f),
         cy + (int)(sinf(a) * 2.5f + 0.5f), col);
}

// apply tune knob: ±12 semitones offset
static int k_midi(int v, int base) {
    return base + (int)((ktune[v] - 0.5f) * 24.0f + 0.5f);
}

// apply decay knob: powf scale centred on 1.0 at val=0.5 (range 0.25× to 4×)
static int k_dur(int v, int base) {
    float s = powf(4.0f, (kdecay[v] - 0.5f) * 2.0f);
    int d = (int)(base * s + 0.5f);
    return d < 5 ? 5 : d;
}

// crossfade a vol level using kcolor[v]: lo at kcolor=0, hi at kcolor=1
static int k_cv(int v, int lo, int hi) {
    int val = (int)(lo + (hi - lo) * kcolor[v] + 0.5f);
    return val < 0 ? 0 : val > 7 ? 7 : val;
}

static void load_preset(void) {
    const Pat *p = &PRESET[pre];
    for (int v = 0; v < NV; v++)
        for (int s = 0; s < STEPS; s++)
            grid[v][s] = p->row[v] && p->row[v][s] == 'x';
    for (int s = 0; s < STEPS; s++)
        gacc[s] = p->accent && p->accent[s] == 'x';
    tempo = p->tempo;
    swing = p->swing ? p->swing : 50;
    bpm(tempo);

    // reset knobs to neutral then apply preset character
    for (int v = 0; v < NV; v++) { ktune[v] = 0.5f; kdecay[v] = 0.5f; kcolor[v] = 0.5f; }
    switch (pre) {
    case 0: break;               // DRUMATIX — the stock voice, untouched
    case 1:                      // ACID PAIR — tighter kick, crisper hats
        kdecay[V_BD] = 0.42f;
        kdecay[V_CH] = 0.40f;
        kcolor[V_SD] = 0.60f;
        break;
    case 2:                      // SKITTER — short everything, noisy snare
        kdecay[V_BD] = 0.36f;
        kdecay[V_CH] = 0.34f;
        kcolor[V_SD] = 0.72f;
        kdecay[V_SD] = 0.44f;
        break;
    case 3:                      // ELECTRO — wider tom spread, dark kick
        ktune[V_LT]  = 0.38f;
        ktune[V_HT]  = 0.58f;
        kcolor[V_BD] = 0.30f;    // TONE toward the low resonator
        break;
    case 4:                      // TOM DANCE — rumbly toms, soft snare
        kcolor[V_LT] = 0.72f;
        kcolor[V_HT] = 0.72f;
        kcolor[V_SD] = 0.40f;
        break;
    case 5:                      // SPARSE — long cymbal, body-heavy
        kdecay[V_CY] = 0.66f;
        kcolor[V_CY] = 0.34f;    // TONE toward the 3440Hz body
        kdecay[V_OH] = 0.62f;
        break;
    }
}

static int vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice `delay` ms from now. Layered hits follow the schematic:
// kick = TWO twin-T resonators (TONE crossfades), snare = one mode + snappy,
// metal = bank members MB1-MB6 into the 7100/3440Hz bandpass paths.
static void fire(int v, int boost, int delay) {
    switch (v) {
    case V_BD: {  // double twin-T: 65Hz + 87Hz beat against each other
        int lo = k_cv(v, 7, 2), hii = k_cv(v, 2, 6);
        schedule_hit(delay, k_midi(v, 36), SL_BD1, vv(lo,  boost), k_dur(v, 150));
        schedule_hit(delay, k_midi(v, 41), SL_BD2, vv(hii, boost), k_dur(v, 105));
        break;
    }
    case V_SD: {  // one twin-T mode + bright snappy noise
        int body = k_cv(v, 8, 0), snpy = k_cv(v, 0, 8);
        schedule_hit(delay, k_midi(v, 58), SL_SDB, vv(body, boost), k_dur(v, 90));
        schedule_hit(delay, k_midi(v, 60), SL_SDN, vv(snpy, boost), k_dur(v, 120));
        break;
    }
    case V_LT: case V_HT: {  // resonator drop + the low rumble burst
        int m = v == V_LT ? 45 : 52;
        schedule_hit(delay, k_midi(v, m),  SL_TOM,  vv(5, boost), k_dur(v, 240));
        schedule_hit(delay, k_midi(v, 48), SL_TOMN, vv(k_cv(v, 0, 5), boost), 25);
        break;
    }
    case V_CY: {  // both bandpass paths at once; TONE fades body↔shimmer
        // body members chosen so their odd harmonics LAND in the 3440Hz band
        // (308×11=3388, 627×5=3135) — the circuit sums all six, we fire two
        int body = k_cv(v, 5, 2), shim = k_cv(v, 4, 7);
        schedule_hit(delay, k_midi(v, MB2), SL_CYL, vv(body, boost), k_dur(v, 800));
        schedule_hit(delay, k_midi(v, MB6), SL_CYL, vv(body - 1, boost), k_dur(v, 800));
        schedule_hit(delay, k_midi(v, MB5), SL_CYH, vv(shim, boost), k_dur(v, 600));
        schedule_hit(delay, k_midi(v, MB6), SL_CYH, vv(shim - 1, boost), k_dur(v, 600));
        break;
    }
    case V_OH: {  // the beating 418+440 pair; decay TEMPO-LINKED (hardware quirk)
        int base = 3 * (15000 / tempo);
        if (base < 150) base = 150; else if (base > 650) base = 650;
        schedule_hit(delay, k_midi(v, MB4), SL_HATO, vv(5, boost), k_dur(v, base));
        schedule_hit(delay, k_midi(v, MB5), SL_HATO, vv(4, boost), k_dur(v, base));
        break;
    }
    case V_CH:  // crisp top members; firing also chokes the open hat (shut-off)
        schedule_hit(delay, k_midi(v, MB5), SL_HATC, vv(4, boost), k_dur(v, 38));
        schedule_hit(delay, k_midi(v, MB6), SL_HATC, vv(3, boost), k_dur(v, 38));
        break;
    }
}

void init(void) {
    // kick — two twin-T resonators, clean (no drive: the 606 doesn't boom).
    // The tiny pitch blip on the low one fakes the trigger thump.
    instrument(SL_BD1, INSTR_SINE, 0, 140, 0, 40);
    instrument_filter(SL_BD1, FILTER_LOW, 300, 2);
    instrument_env(SL_BD1, 0, ENV_PITCH, 0, 14, 6.0f);
    instrument(SL_BD2, INSTR_SINE, 0, 100, 0, 30);
    instrument_filter(SL_BD2, FILTER_LOW, 350, 2);

    // snare body (single mode) + the bright "snappy" noise gate
    instrument(SL_SDB, INSTR_SINE, 0, 85, 0, 25);
    instrument_filter(SL_SDB, FILTER_LOW, 1400, 1);
    instrument_env(SL_SDB, 0, ENV_PITCH, 0, 15, 3.0f);
    instrument(SL_SDN, INSTR_NOISE, 0, 115, 0, 35);
    instrument_filter(SL_SDN, FILTER_HIGH, 2400, 2);

    // toms — resonator with a drop + the documented LOW-passed rumble burst
    instrument(SL_TOM, INSTR_SINE, 0, 230, 0, 45);
    instrument_env(SL_TOM, 0, ENV_PITCH, 0, 60, 5.0f);
    instrument(SL_TOMN, INSTR_NOISE, 0, 24, 0, 10);
    instrument_filter(SL_TOMN, FILTER_LOW, 400, 2);

    // the metal paths — BANDpass, per the schematic (the 808 cart uses
    // highpass; this narrower window is the 606's delicacy)
    // both metal paths are HIGHPASSES here, not the schematic's bandpasses:
    // we fire 2 bank members where the circuit sums 6, so a narrow band
    // starves between their sparse odd harmonics, and a wide band leaks the
    // square fundamentals until the hats turn to cowbells (both measured).
    // Each highpass keeps its window's upper skirt: shimmer ≥ ~7k, body ≥ ~3k
    // — the 7100 / 3440 Hz split survives as the two cutoffs.
    instrument(SL_CYH, INSTR_SQUARE, 0, 460, 0, 140);   // shimmer path
    instrument_filter(SL_CYH, FILTER_HIGH, 7000, 3);
    instrument(SL_CYL, INSTR_SQUARE, 0, 780, 0, 200);   // body path
    instrument_filter(SL_CYL, FILTER_HIGH, 3000, 3);

    instrument(SL_HATO, INSTR_SQUARE, 0, 320, 0, 80);   // hats ≥ ~7k too
    instrument_filter(SL_HATO, FILTER_HIGH, 7000, 3);
    instrument(SL_HATC, INSTR_SQUARE, 0, 38, 0, 13);
    instrument_filter(SL_HATC, FILTER_HIGH, 7000, 3);
    instrument_choke(SL_HATC, SL_HATO);  // the schematic's "shut off circuit"

    for (int v = 0; v < NV; v++) { ktune[v] = 0.5f; kdecay[v] = 0.5f; kcolor[v] = 0.5f; }
    load_preset();
}

// hit-test the step grid: row -1 = accent strip, -2 = miss; col via *col
static int grid_row(int mx, int my, int *col) {
    int s = (mx - GX) / SX;
    if (mx < GX || s < 0 || s >= STEPS) return -2;
    if (my >= GY - 9 && my < GY - 2) { *col = s; return -1; }   // accent strip
    int v = (my - GY) / SY;
    if (my < GY || v < 0 || v >= NV) return -2;
    *col = s;
    return v;
}

void update(void) {
    for (int v = 0; v < NV; v++)
        if (keyp(VKEY[v])) { fire(v, 1, 0); flash[v] = 5; }

    if (keyp(KEY_SPACE) || tapp(244, 24, 64, 12)) { running = !running; last16 = -1; }
    if (keyp('M') || tapp(268, 182, 40, 12)) modded = !modded;
    if (keyp(KEY_LEFT)  || tapp(10, 24, 20, 12))
        { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; }
    if (keyp(KEY_RIGHT) || (nextz_x >= 0 && tapp(nextz_x, 24, 20, 12)))
        { pre = (pre + 1) % NP;      load_preset(); last16 = -1; }
    if (keyp(KEY_UP)   || tapp(274, 9, 32, 12)) { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); }
    if (keyp(KEY_DOWN) || tapp(244, 9, 30, 12)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); }
    if (keyp('Z') || tapp(200, 9, 18, 12)) { swing -= 2; if (swing < 50) swing = 50; }
    if (keyp('X') || tapp(218, 9, 20, 12)) { swing += 2; if (swing > 66) swing = 66; }

    // knob hover + drag — MODS only (stock panel has no knobs, like 1981)
    int mx = mouse_x(), my = mouse_y();
    hover_v = -1; hover_k = -1;
    if (modded && my >= GY && my < GY + NV * SY) {
        int v = (my - GY) / SY;
        if      (mx >= K0X && mx < K0X + KW)                       { hover_v = v; hover_k = 0; }
        else if (mx >= K1X && mx < K1X + KW)                       { hover_v = v; hover_k = 1; }
        else if (mx >= K2X && mx < K2X + KW && K2LABEL[v] != NULL) { hover_v = v; hover_k = 2; }
    }
    if (mouse_pressed(MOUSE_LEFT) && hover_v >= 0) {
        drag_v = hover_v; drag_k = hover_k; drag_y0 = my; drag_x0 = mx;
    }
    if (drag_v >= 0 && mouse_down(MOUSE_LEFT)) {
        float dy = drag_y0 - my;          // up = increase (coarse)
        float dx = mx     - drag_x0;      // right = increase (fine)
        float delta = dy / 80.0f + dx / 600.0f;
        float *kp = drag_k == 0 ? &ktune[drag_v]
                  : drag_k == 1 ? &kdecay[drag_v]
                  :               &kcolor[drag_v];
        float nv2 = *kp + delta;
        *kp = nv2 < 0.0f ? 0.0f : nv2 > 1.0f ? 1.0f : nv2;
        drag_y0 = my; drag_x0 = mx;
    }
    if (!mouse_down(MOUSE_LEFT)) { drag_v = -1; drag_k = -1; }

    // mouse: press toggles a cell (drag paints), label click auditions
    bool on_knob = hover_v >= 0 || drag_v >= 0;
    int mc, mr = grid_row(mx, my, &mc);
    if (mouse_pressed(MOUSE_LEFT) && !on_knob) {
        if (mr >= 0) {
            paint_val = !grid[mr][mc];
            grid[mr][mc] = paint_val;
        } else if (mr == -1) {
            paint_val = !gacc[mc];
            gacc[mc] = paint_val;
        } else if (mx < GX && my >= GY && my < GY + NV * SY) {
            int v = (my - GY) / SY;
            fire(v, 1, 0); flash[v] = 5;
        }
    } else if (mouse_down(MOUSE_LEFT) && !on_knob) {
        if (mr >= 0)       grid[mr][mc] = paint_val;
        else if (mr == -1) gacc[mc]     = paint_val;
    }

    if (!running) return;

    // sixteenth clock off beat(); steps scheduled one ahead, sample-accurate
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (s16 != last16) {
        bool first = (last16 < 0);
        last16  = s16;
        playhead = s16 & 15;

        for (int v = 0; v < NV; v++)
            if (grid[v][playhead]) flash[v] = 5;

        float f = beat_pos() * 4.0f; f -= (int)f;
        int step_ms = 15000 / tempo;
        int delay   = (int)((1.0f - f) * step_ms);
        int nx      = (s16 + 1) & 15;
        if ((nx & 3) == 2) delay += (swing - 50) * 4 * step_ms / 100;
        int boost   = gacc[nx] ? 2 : 0;
        for (int v = 0; v < NV; v++)
            if (grid[v][nx]) fire(v, boost, delay);

        if (first) {   // fresh start: also sound the step we're already on
            int b0 = gacc[playhead] ? 2 : 0;
            for (int v = 0; v < NV; v++)
                if (grid[v][playhead]) fire(v, b0, 0);
        }
    }
}

void draw(void) {
    const Pat *p = &PRESET[pre];
    char buf[32];

    // the silver panel — the 303's sibling livery
    cls(CLR_BLACK);
    rectfill(6, 8, 308, 184, CLR_LIGHT_GREY);
    rect(6, 8, 308, 184, CLR_MEDIUM_GREY);

    int tx = print("TR-606", 14, 13, CLR_BLACK);
    print(" DRUMATIX", tx, 13, CLR_RED);
    sprintf(buf, "SW%2d", swing);
    print(buf, 204, 13, swing > 50 ? CLR_RED : CLR_DARK_GREY);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 248, 13, CLR_BLACK);

    // black display strip: preset + transport (tap targets live here)
    rectfill(10, 24, 300, 12, CLR_BLACK);
    int nx = print("< ", 14, 26, CLR_ORANGE);
    nx = print(p->name, nx, 26, CLR_ORANGE);
    print(" >", nx, 26, CLR_ORANGE);
    nextz_x = nx + 2;
    print(running ? "PLAYING" : "STOPPED", 248, 26, running ? CLR_GREEN : CLR_RED);

    // playhead column
    if (running)
        rectfill(GX + playhead * SX - 1, GY - 8, SX - 1, NV * SY + 8, CLR_MEDIUM_GREY);

    // accent strip (clickable)
    for (int s = 0; s < STEPS; s++)
        rectfill(GX + s * SX, GY - 7, SX - 4, 3,
                 gacc[s] ? CLR_RED : CLR_MEDIUM_GREY);

    for (int v = 0; v < NV; v++) {
        int y = GY + v * SY;
        sprintf(buf, "%c", VKEY[v]);
        print(buf, 14, y + 1, flash[v] > 0 ? CLR_RED : CLR_DARK_GREY);
        print(VNAME[v], 26, y + 1, flash[v] > 0 ? CLR_RED : CLR_BLACK);
        if (modded) {
            int base_col = flash[v] > 0 ? CLR_RED : CLR_DARKER_GREY;
            draw_knob(K0X, y + 1, ktune[v],  (drag_v==v&&drag_k==0) ? CLR_RED : base_col);
            draw_knob(K1X, y + 1, kdecay[v], (drag_v==v&&drag_k==1) ? CLR_RED : base_col);
            if (K2LABEL[v])
                draw_knob(K2X, y + 1, kcolor[v], (drag_v==v&&drag_k==2) ? CLR_RED : base_col);
        }
        for (int s = 0; s < STEPS; s++) {
            int x = GX + s * SX;
            if (grid[v][s])   // red LEDs on black caps — the 606 step look
                rectfill(x, y, SX - 4, CH2,
                         (flash[v] > 0 && s == playhead && running) ? CLR_WHITE : CLR_RED);
            else {
                rectfill(x, y, SX - 4, CH2, CLR_DARKER_GREY);
                if ((s & 3) == 0) rect(x, y, SX - 4, CH2, CLR_DARK_GREY);
            }
        }
        if (flash[v] > 0) flash[v]--;
    }

    // hover label — fixed in the header band (never overlaps a knob row)
    int hv = (drag_v >= 0) ? drag_v : hover_v;
    int hk = (drag_v >= 0) ? drag_k : hover_k;
    if (hv >= 0 && hk >= 0) {
        const char *lbl = hk == 2 ? K2LABEL[hv] : KLABEL[hk];
        if (lbl) {
            char buf2[24];
            sprintf(buf2, "%s %s", VNAME[hv], lbl);
            font(FONT_SMALL);
            print(buf2, K0X, 40, CLR_DARK_GREY);
            font(FONT_NORMAL);
        }
    }

    // below the grid: the tempo LED + the sibling nod
    int ledy = GY + NV * SY + 8;
    bool blink = running && beat_pos() < 0.25f;
    circfill(288, ledy + 3, 3, blink ? CLR_RED : CLR_DARKER_GREY);
    circ(288, ledy + 3, 3, CLR_DARK_GREY);
    font(FONT_SMALL);
    print("DIN SYNC: BRING YOUR OWN TB-303", 14, ledy + 1, CLR_MEDIUM_GREY);
    print("COMPUTER CONTROLLED", 196, ledy + 1, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    print("CLICK <> PRESET ^v BPM Z/X SW", 14, 186, CLR_DARK_GREY);
    print("MODS", 272, 186, modded ? CLR_RED : CLR_DARK_GREY);
}
