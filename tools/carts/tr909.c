/* de:meta
{
  "title": "tr-909 rhythm composer",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "subtractive-synth",
    "fm-synth",
    "drum-synthesis"
  ],
  "lineage": "Roland TR-909 (1983), completing the cr78/tr808/tb303 family; hybrid kit where analog voices are subtractive but the ROM-sample hats/cymbals are stood in by an FM voice on the 3.5 inharmonic detent through a closing highpass, plus a flam/drag/ratchet stroke family and period-correct shuffle.",
  "homage": "Roland TR-909 Rhythm Composer (1983)",
  "description": "The house and techno machine — the TR-909 (1983), fifth box in the classic-machine family (cr-78, tr-808, tb-303, sh-101) and the one that completes the ReBirth RB-338 rack. Same editable grid as the 808 cart, but the 909's hybrid voice architecture: analog kick/snare/toms/rim/clap (the kick is the HOUSE kick — fast +30-semitone sweep over 35ms plus a separate click layer on the famous ATTACK knob, punch where the 808 booms), while the hats, crash and ride — 6-bit ROM samples in the real hardware — are stood in by the FM engine: INSTR_FM parked on the 3.5 inharmonic clang detent with feedback cranked, through a highpass whose cutoff starts 5kHz low and rises via a negative ENV_CUTOFF amount = the fast-closing sizzle of a sampled hat. Closed hat chokes open, like the shared output stage of the hardware. And the swing knob is finally PERIOD CORRECT: the 909 is where Roland actually shipped shuffle (Z/X, even 16ths drag — the cr-78/tr-808 carts wear the same knob as an admitted anachronism). FLAM too — the panel's other humanize trick — and beyond: right-click CYCLES a cell through the stroke family — flam (one quiet grace note 22ms early, the Hardfloor clap signature), drag (two graces, the rudiment), ratchet (four even hits chopped across the step — not on the 1983 panel, but the fill techno lives on; Hardfloor's hat row ends on one). Cells draw their strokes as ticks. And one admitted impurity: a METAL-FILTER XY pad (bottom right) riding the highpass of all five metal slots — X = cutoff (left = darker/fuller), Y = resonance (up = the SVF peak rings) — because the FM stand-ins land bright and hissy without a tone control. And a modern-clone touch the 1983 panel never had: TRIG PROBABILITY (the RD-9 'Poly'-era move) — DRAG A CELL UP/DOWN to set its % chance to fire (100/75/50/25), so hats and fills breathe and the loop never repeats identically. A less-than-certain cell draws as a shorter bar in its full-height socket; the gesture is axis-locked so a sideways drag still paints on/off as before. The stroke family is touch-reachable too: tap the header STROKE button (or S) to arm it, then tapping a cell cycles flam/drag/ratchet instead of toggling (right-click still cycles on desktop). 11 voices with up to three 8×8 rotary knobs each (TUNE / DECAY / ATTK-SNPY-CLIK-TONE), red TOTAL ACCENT strip, six presets: Good Life, The Bells, Energy Flash, Hardfloor, Revolution 909, Gabber. Q-A play voices, LEFT/RIGHT preset, UP/DOWN tempo, SPACE start/stop, click a label to audition, right-click for flams, drag a cell vertically for its fire-chance, drag knobs Y=coarse X=fine."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── TR-909 RHYTHM COMPOSER ────────────────────────────────────────────────
// Roland's TR-909 (1983) — the house and techno machine. Commercial flop
// like the 808, then "Good Life", "The Bells", "Energy Flash" and every
// warehouse since. Third sibling in the grid family (cr78 → tr808 → tr909);
// with tb303 on the shelf this completes the ReBirth RB-338 rack.
//
// The 909 is HYBRID: kick/snare/toms/rim/clap are analog circuits (same
// architecture as tr808.c, hotter values), but hats and cymbals are 6-bit
// ROM SAMPLES — a thing this engine can't play. The workaround here is
// yesterday's FM engine: INSTR_FM parked on the 3.5 ratio detent (the
// inharmonic clang — "inharmonicity, not ratio height, is what reads as
// metal") with feedback cranked, then a HIGHPASS whose cutoff starts LOW
// and RISES via a negative ENV_CUTOFF amount = the fast-closing sizzle
// burst of a sampled hat. Not a ROM playback, but it's metal, not static.
//
//   kick  — the house kick: sine, FAST +30st sweep over 35ms (the 808
//           booms, the 909 punches), plus a separate click layer on the
//           panel's famous ATTACK knob — and instrument_drive() running
//           the body hot, the way every warehouse desk actually had it.
//   snare — two bridged-T modes (185/330Hz, same as the 808 — different
//           envelope) under brighter, longer noise. SNPY fades body↔noise.
//   toms  — sine + pitch drop + a noise CLIK at the attack.
//   rim   — short woody dual ping (the "Energy Flash" tick).
//   clap  — noise retriggered 3× ~9ms apart + a room tail.
//   hats  — FM clang through the closing highpass; closed CHOKES open,
//           which the real 909 also did (shared output stage).
//   crash — FM clang + a long noise wash (TONE knob fades clang↔wash).
//   ride  — cleaner FM ping (less feedback), the Jeff Mills 8th-note bell.
//   shuffle — PERIOD CORRECT AT LAST. The cr78/tr808 carts wear this knob
//           as an admitted anachronism; the 909 is where Roland actually
//           shipped it (after the LM-1 proved swing sells). 909-style:
//           the even 16ths drag, not the offbeat 8ths. Z/X, 50..66%.
//   metal pad — NOT on the 1983 panel: an XY rect (bottom right) riding
//           the highpass of all five FM/noise metal slots. X = cutoff
//           (left = darker/fuller, right = thin sizzle), Y = resonance
//           (up = the SVF peak whistles — body and ring). The FM stand-ins
//           land bright and hissy by default; this is their tone control.
//   accent — the TOTAL ACCENT row, red strip (orange on the 808 cart).
//   flam — the panel's OTHER humanize trick, right next to SHUFFLE: a
//           flammed step plays a quiet grace note ~22ms before the main
//           hit (the Hardfloor snare/clap signature). RIGHT-CLICK cycles
//           a cell through the stroke family: plain → FLAM (1 grace) →
//           DRAG (2 graces, the rudiment) → RATCHET (4 even hits chopped
//           across the step — not on the 1983 panel, but the fill its
//           children shipped and techno lives on). Cells draw their
//           strokes as ticks. Preset rows mark them 'f' / 'd' / 'r'.
//
//   Q W E R T Y U I O P A   play each voice by hand
//   LEFT / RIGHT  preset (or tap the < > arrows on the label)
//   UP / DOWN  tempo      SPACE  start / stop
//   Z / X  shuffle down / up
//   MOUSE  click/drag cells to edit (RIGHT-click = flam), click the strip
//          above for accents, click a label to audition, drag the knobs
//          (Y coarse, X fine)

// voices — named indices, never raw numbers (house rule)
enum { V_BD, V_SD, V_LT, V_MT, V_HT, V_RS, V_CP, V_CH, V_OH, V_CC, V_RC, NV };

// instrument slots (5..8 are user waves; 9..24 ours)
enum {
    SL_BD = 9, SL_BDC, SL_SDB, SL_SDN, SL_TOM, SL_TOMC,
    SL_RS, SL_CP, SL_HHC, SL_HHO, SL_CC, SL_CCN, SL_RC
};

static const char *VNAME[NV] = {
    "BASS", "SNRE", "LTOM", "MTOM", "HTOM", "RIM",
    "CLAP", "CHHH", "OPHH", "CRSH", "RIDE"
};
static const char VKEY[NV] = {
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'A'
};

#define STEPS 16

#define GX   88    // grid left edge
#define GY   46    // grid top edge
#define SX   13    // column stride
#define SY   9     // row stride

// per-voice knobs: 8×8 rotaries in the label column, same as tr808.c
#define K0X  60    // tune knob x
#define K1X  70    // decay knob x
#define K2X  80    // character knob x (voice-specific: ATTK/SNPY/CLIK/TONE)
#define KW    8    // knob size

typedef struct {
    const char *name;
    int         tempo;
    const char *row[NV];   // 16 chars: 'x' hit, 'f' flam, 'd' drag, 'r' ratchet; NULL = silent
    const char *accent;    // 16 chars, 'x' = +2 volume on that step
    int         swing;     // 50 = straight .. 66 = triplet (0 = straight)
} Pat;

#define FLAM_MS 22         // grace-note lead — the panel's fixed flam spacing

// metal-filter XY pad (bottom right, below the grid)
#define PADX 252
#define PADY 152
#define PADW 52
#define PADH 30

static const Pat PRESET[] = {
    { "GOOD LIFE", 118, {          // Inner City — the Detroit house blueprint
        [V_BD] = "x...x...x...x...",
        [V_CP] = "....x.......x...",
        [V_CH] = "x.x.x.x.x.x.x.x.",
        [V_OH] = "..x...x...x...x.",
      }, "x.......x......." },
    { "THE BELLS", 135, {          // Jeff Mills — ride-driven techno
        [V_BD] = "x...x...x...x...",
        [V_RC] = "x.x.x.x.x.x.x.x.",
        [V_SD] = "....x.......x...",
        [V_CH] = "..x...x...x...x.",
      }, "x.......x......." },
    { "ENERGY FLASH", 128, {       // Beltram — dark rolling toms + rim ticks
        [V_BD] = "x...x...x...x...",
        [V_OH] = "..x...x...x...x.",
        [V_LT] = ".......x......x.",
        [V_MT] = "..........x.....",
        [V_RS] = "..x..x....x...x.",
      }, "x.......x......." },
    { "HARDFLOOR", 130, {          // acid shuffle, flammed claps, ratchet fill
        [V_BD] = "x...x...x...x...",
        [V_CH] = "xxxxxxxxxxxxxxxr",
        [V_OH] = "..x...x...x...x.",
        [V_CP] = "....f.......f...",
        [V_RS] = "......x.......f.",
      }, "x...x...x...x...", 58 },
    { "REVOLUTION 909", 122, {     // Daft Punk — filtered french house
        [V_BD] = "x...x...x...x...",
        [V_CP] = "....x.......x...",
        [V_CH] = "x.xxx.xxx.xxx.xx",
        [V_OH] = "..x.......x.....",
        [V_RS] = ".x.....x.....x..",
      }, "x.......x......." },
    { "GABBER", 185, {             // Rotterdam — the kick IS the song
        [V_BD] = "x...x...x...x...",
        [V_SD] = "....x.......d...",
        [V_OH] = "..x...x...x...x.",
        [V_CC] = "x...............",
      }, "x...x...x...x..." },
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int  pre      = 0;      // start on GOOD LIFE
static int  tempo    = 118;
static bool running  = true;
static int  last16   = -1;
static int  playhead = 0;
static int  flash[NV];
// stroke kinds, cycled by right-click (desktop) or long-press (touch) — named, never raw numbers (house rule)
enum { ST_PLAIN, ST_FLAM, ST_DRAG, ST_RATCHET, NSTROKE };
static const char *SNAME[NSTROKE] = { "PLAIN", "FLAM", "DRAG", "RATCHET" };

static bool grid[NV][STEPS];   // the live pattern — editable, loaded from preset
static int  nextz_x = -1;      // the preset '>' tap zone, recorded by draw() ('<' is fixed)
static unsigned char gstroke[NV][STEPS];  // ST_* per cell (only meaningful when grid on)
static unsigned char gprob[NV][STEPS];    // per-cell % chance to fire (100 = always; the RD-9 trig-prob)
static bool gacc[STEPS];       // live accent row
static bool paint_val;         // what a click-drag writes (set on press)
static int  paint_stroke;      // the ST_* a right-drag paints (set on press)
// per-cell left-drag, axis-locked at press: horizontal paints on/off, vertical
// rides the fire-chance. STROKE mode (a header toggle) repurposes a plain tap to
// cycle flam/drag/ratchet instead — the touch twin of right-click, no hidden hold.
static int   gest_r = -1, gest_c;   // cell a left-press landed on (-1 = none)
static int   gest_x0, gest_y0;      // press / running-reference position
static int   gest_axis;             // 0=undecided 1=horizontal-paint 2=vertical-prob
static float probf;                 // working chance during a vertical drag (0..1)
static bool  gest_on0;              // was the cell on before the press (so a vertical drag keeps its state)
static unsigned char gest_stroke0;  // its stroke before the press-reset (a prob drag must not clobber the flam)
static bool  stroke_mode;           // header STROKE toggle: taps cycle the stroke family
static int  swing = 50;        // 50 = straight .. 66 = full triplet

static float ktune[NV];        // 0..1, center=0.5 → ±12 semitones
static float kdecay[NV];       // 0..1, center=0.5 → 1× scale (0→0.25×, 1→4×)
static float kcolor[NV];       // 0..1, center=0.5 → voice-specific character knob
static int   drag_v = -1, drag_k = -1;
static int   drag_y0, drag_x0;
static int   hover_v = -1, hover_k = -1;

// metal pad state — defaults a notch fuller than the first build, which
// shipped bright and hissy (cut 0.5 / res 0.25 = the original voicing)
static float mcut = 0.40f, mres = 0.33f;
static bool  pad_drag = false;

// re-aim the highpass on all five metal slots from the pad. X scales each
// slot's base cutoff ×0.3..×1.7, Y is shared resonance 0..12 (the SVF peak
// at the cutoff is what gives the clang body and ring back).
static void apply_metal_filter(void) {
    float m = 0.3f + 1.4f * mcut;
    int   r = (int)(mres * 12.0f + 0.5f);
    instrument_filter(SL_HHC, FILTER_HIGH, (int)(8500 * m), r);
    instrument_filter(SL_HHO, FILTER_HIGH, (int)(8000 * m), r);
    instrument_filter(SL_CC,  FILTER_HIGH, (int)(4500 * m), r);
    instrument_filter(SL_CCN, FILTER_HIGH, (int)(5200 * m), r);
    instrument_filter(SL_RC,  FILTER_HIGH, (int)(5000 * m), r);
}

// knob column labels — k=0,1 are the same for every voice; k=2 is per-voice
static const char *KLABEL[2] = { "TUNE", "DECAY" };
static const char *K2LABEL[NV] = {
    "ATTK",                     // BD — the famous click knob
    "SNPY",                     // SD — body↔noise fade
    "CLIK", "CLIK", "CLIK",     // toms — attack noise level
    NULL,   NULL,               // RS CP
    NULL,   NULL,               // CH OH — decay knob covers the panel's DECAY
    "TONE",                     // CC — clang↔wash fade
    NULL,                       // RC
};

// 8×8 rotary knob: circle outline + single tick pixel showing value position.
static void draw_knob(int x, int y, float val, int col) {
    int cx = x + 3, cy = y + 3;
    circ(cx, cy, 3, CLR_MEDIUM_GREY);
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

// crossfade a vol level using kcolor[v]: lo at kcolor=0, hi at kcolor=1, clamped 0..7
static int k_cv(int v, int lo, int hi) {
    int val = (int)(lo + (hi - lo) * kcolor[v] + 0.5f);
    return val < 0 ? 0 : val > 7 ? 7 : val;
}

// snap a 0..1 drag to the four RD-9-style trig-prob buckets (100/75/50/25)
static unsigned char snap_prob(float f) {
    return f >= 0.875f ? 100 : f >= 0.625f ? 75 : f >= 0.375f ? 50 : 25;
}

static void load_preset(void) {
    const Pat *p = &PRESET[pre];
    for (int v = 0; v < NV; v++)
        for (int s = 0; s < STEPS; s++) {
            char c = p->row[v] ? p->row[v][s] : '.';
            grid[v][s]    = c == 'x' || c == 'f' || c == 'd' || c == 'r';
            gstroke[v][s] = c == 'f' ? ST_FLAM : c == 'd' ? ST_DRAG
                          : c == 'r' ? ST_RATCHET : ST_PLAIN;
            gprob[v][s]   = 100;   // presets are certain patterns; drag a cell down to loosen it
        }
    for (int s = 0; s < STEPS; s++)
        gacc[s] = p->accent && p->accent[s] == 'x';
    tempo = p->tempo;
    swing = p->swing ? p->swing : 50;
    bpm(tempo);

    // reset knobs to neutral then apply preset character
    for (int v = 0; v < NV; v++) { ktune[v] = 0.5f; kdecay[v] = 0.5f; kcolor[v] = 0.5f; }
    switch (pre) {
    case 0: // GOOD LIFE — punchy kick, tight hats, open hat breathes
        kdecay[V_BD] = 0.42f;
        kcolor[V_BD] = 0.62f;   // some click
        kdecay[V_OH] = 0.58f;
        break;
    case 1: // THE BELLS — short dry kick under a long ringing ride
        kdecay[V_BD] = 0.36f;
        kdecay[V_RC] = 0.66f;
        kcolor[V_SD] = 0.64f;   // snappy
        break;
    case 2: // ENERGY FLASH — boomier kick, dark wide toms
        kdecay[V_BD] = 0.56f;
        ktune[V_LT]  = 0.40f;
        kdecay[V_LT] = 0.60f;
        kdecay[V_MT] = 0.60f;
        break;
    case 3: // HARDFLOOR — everything tight so the shuffle reads
        kdecay[V_BD] = 0.38f;
        kdecay[V_CH] = 0.40f;
        kcolor[V_BD] = 0.70f;
        break;
    case 4: // REVOLUTION 909 — mid kick, hats do the talking
        kdecay[V_BD] = 0.46f;
        kdecay[V_CH] = 0.46f;
        kdecay[V_OH] = 0.52f;
        break;
    case 5: // GABBER — ATTACK pinned, long overdriven boom, tuned up
        kcolor[V_BD] = 1.00f;
        kdecay[V_BD] = 0.72f;
        ktune[V_BD]  = 0.58f;
        kdecay[V_CC] = 0.62f;
        break;
    }
}

static int vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice `delay` ms from now. Analog voices follow the schematic
// (shared architecture with tr808.c, 909 component values); hats/cymbals
// are the FM-clang ROM stand-ins described in the header.
static void fire(int v, int boost, int delay) {
    switch (v) {
    case V_BD:  // the house kick: fast sweep body + ATTK click layer
        schedule_hit(delay, k_midi(v, 32), SL_BD, vv(6, boost), k_dur(v, 320));
        schedule_hit(delay, 60, SL_BDC, vv(k_cv(v, 0, 6), boost), 10);
        break;
    case V_SD: {  // 185Hz + 330Hz modes; SNPY fades body↔noise
        int body = k_cv(v, 8, 1), snpy = k_cv(v, 1, 8);
        schedule_hit(delay, k_midi(v, 54), SL_SDB, vv(body, boost), k_dur(v, 95));
        schedule_hit(delay, k_midi(v, 64), SL_SDB, vv(body - 1, boost), k_dur(v, 95));
        schedule_hit(delay, k_midi(v, 60), SL_SDN, vv(snpy, boost), k_dur(v, 180));
        break;
    }
    case V_LT: case V_MT: case V_HT: {  // sine drop + CLIK attack noise
        int m = v == V_LT ? 42 : v == V_MT ? 47 : 54;
        schedule_hit(delay, k_midi(v, m),  SL_TOM,  vv(5, boost), k_dur(v, 240));
        schedule_hit(delay, 70, SL_TOMC, vv(k_cv(v, 0, 5), boost), 14);
        break;
    }
    case V_RS:  // woody dual ping, lower + shorter than the 808's
        schedule_hit(delay, k_midi(v, 76), SL_RS, vv(5, boost), k_dur(v, 30));
        schedule_hit(delay, k_midi(v, 64), SL_RS, vv(3, boost), k_dur(v, 30));
        break;
    case V_CP:  // three retriggers ~9ms apart + a soft room tail
        schedule_hit(delay,      k_midi(v, 60), SL_CP, vv(5, boost), 11);
        schedule_hit(delay + 9,  k_midi(v, 60), SL_CP, vv(5, boost), 11);
        schedule_hit(delay + 18, k_midi(v, 60), SL_CP, vv(5, boost), 11);
        schedule_hit(delay + 26, k_midi(v, 60), SL_CP, vv(3, boost), k_dur(v, 130));
        break;
    case V_CH:  // FM clang through the closing highpass (ROM stand-in)
        schedule_hit(delay, k_midi(v, 97), SL_HHC, vv(4, boost), k_dur(v, 40));
        break;
    case V_OH:  // same metal, long burst; choked by a closed hit
        schedule_hit(delay, k_midi(v, 97), SL_HHO, vv(4, boost), k_dur(v, 400));
        break;
    case V_CC:  // TONE fades FM clang ↔ noise wash
        schedule_hit(delay, k_midi(v, 84), SL_CC,  vv(k_cv(v, 6, 1), boost), k_dur(v, 1100));
        schedule_hit(delay, k_midi(v, 60), SL_CCN, vv(k_cv(v, 1, 6), boost), k_dur(v, 1300));
        break;
    case V_RC:  // cleaner FM ping — bell + a quieter edge a fifth up
        schedule_hit(delay, k_midi(v, 76), SL_RC, vv(4, boost), k_dur(v, 600));
        schedule_hit(delay, k_midi(v, 83), SL_RC, vv(2, boost), k_dur(v, 600));
        break;
    }
}

// fire a step with its stroke. Flam/drag lead the main hit with 1/2 quiet
// grace notes; a ratchet replaces it with four even hits across the step.
static void fire_stroke(int v, int st, int boost, int delay, int step_ms) {
    int g;
    switch (st) {
    case ST_DRAG:
        g = delay - 2 * FLAM_MS; fire(v, boost - 4, g < 0 ? 0 : g);
        /* fall through — a drag is a flam with one more grace */
    case ST_FLAM:
        g = delay - FLAM_MS;     fire(v, boost - 3, g < 0 ? 0 : g);
        fire(v, boost, delay);
        break;
    case ST_RATCHET:
        // repeats DECAY in level (real rolls do), which also stops four equal
        // bright hat transients from stacking into an audible pop
        for (int k = 0; k < 4; k++)
            fire(v, boost - k, delay + k * step_ms / 4);
        break;
    default:
        fire(v, boost, delay);
    }
}

void init(void) {
    // kick — sine body, lowpassed, +30st sweep over 35ms (faster + bigger
    // than the 808's 26/50: punch, not boom)
    instrument(SL_BD, INSTR_SINE, 0, 300, 0, 50);
    instrument_filter(SL_BD, FILTER_LOW, 380, 2);
    instrument_env(SL_BD, 0, ENV_PITCH, 0, 35, 30.0f);
    // the warehouse kick: every classic 909 record runs the kick hot into the
    // desk — tanh on the sine body adds the odd harmonics that read as WEIGHT.
    // (Baked, not accent-linked: per-hit instrument_drive() would mis-apply to
    // delayed flam/shuffle hits — they snapshot the slot at fire time.)
    instrument_drive(SL_BD, 0.35f);
    // kick click — the ATTACK knob layer: a 10ms highpassed noise tick
    instrument(SL_BDC, INSTR_NOISE, 0, 9, 0, 4);
    instrument_filter(SL_BDC, FILTER_HIGH, 2500, 2);

    // snare body (fired twice: 185Hz + 330Hz modes), quick settle sweep
    instrument(SL_SDB, INSTR_SINE, 0, 90, 0, 25);
    instrument_filter(SL_SDB, FILTER_LOW, 1400, 1);
    instrument_env(SL_SDB, 0, ENV_PITCH, 0, 15, 4.0f);
    // snare noise — brighter + longer than the 808's snappy
    instrument(SL_SDN, INSTR_NOISE, 0, 170, 0, 50);
    instrument_filter(SL_SDN, FILTER_HIGH, 1400, 2);

    // toms — sine with a pitch drop + a short noise click at the attack
    instrument(SL_TOM, INSTR_SINE, 0, 220, 0, 45);
    instrument_env(SL_TOM, 0, ENV_PITCH, 0, 60, 7.0f);
    instrument(SL_TOMC, INSTR_NOISE, 0, 12, 0, 5);
    instrument_filter(SL_TOMC, FILTER_BAND, 2000, 3);

    // rimshot — short woody dual ping through a low highpass
    instrument(SL_RS, INSTR_TRI, 0, 28, 0, 10);
    instrument_filter(SL_RS, FILTER_HIGH, 400, 3);

    // handclap — bandpassed noise; the retriggers in fire() make the clap
    instrument(SL_CP, INSTR_NOISE, 0, 100, 0, 45);
    instrument_filter(SL_CP, FILTER_BAND, 1200, 5);

    // ── the ROM-sample stand-ins ─────────────────────────────────────────
    // hats: INSTR_FM on the 3.5 detent (harmonics 0.55 → inharmonic clang),
    // feedback high (morph) = chaotic metal, then a highpass whose cutoff
    // STARTS 5kHz LOW and rises back (negative ENV_CUTOFF) — the sampled
    // hat's fast-closing sizzle. Closed chokes open, like the hardware.
    instrument(SL_HHC, INSTR_FM, 0, 40, 0, 16);
    instrument_harmonics(SL_HHC, 0.55f);
    instrument_timbre(SL_HHC, 0.90f);
    instrument_morph(SL_HHC, 0.85f);
    instrument_env(SL_HHC, 0, ENV_CUTOFF, 0, 28, -5000.0f);

    instrument(SL_HHO, INSTR_FM, 0, 380, 0, 110);
    instrument_harmonics(SL_HHO, 0.55f);
    instrument_timbre(SL_HHO, 0.90f);
    instrument_morph(SL_HHO, 0.85f);
    instrument_env(SL_HHO, 0, ENV_CUTOFF, 0, 220, -4500.0f);
    instrument_choke(SL_HHC, SL_HHO);   // closed hat chokes the open hat

    // crash — FM clang + a separate long noise wash (TONE knob crossfades)
    instrument(SL_CC, INSTR_FM, 0, 1000, 0, 280);
    instrument_harmonics(SL_CC, 0.55f);
    instrument_timbre(SL_CC, 1.00f);
    instrument_morph(SL_CC, 0.90f);
    instrument(SL_CCN, INSTR_NOISE, 0, 1200, 0, 320);

    // ride — same metal, less feedback = a cleaner ringing ping
    instrument(SL_RC, INSTR_FM, 0, 550, 0, 160);
    instrument_harmonics(SL_RC, 0.55f);
    instrument_timbre(SL_RC, 0.72f);
    instrument_morph(SL_RC, 0.58f);

    apply_metal_filter();   // the XY pad owns all five metal highpasses

    for (int v = 0; v < NV; v++) { ktune[v] = 0.5f; kdecay[v] = 0.5f; kcolor[v] = 0.5f; }
    load_preset();
}

// hit-test the step grid: row -1 = accent strip, -2 = miss; col via *col
static int grid_row(int mx, int my, int *col) {
    int s = (mx - GX) / SX;
    if (mx < GX || s < 0 || s >= STEPS) return -2;
    if (my >= GY - 8 && my < GY - 1) { *col = s; return -1; }   // accent strip
    int v = (my - GY) / SY;
    if (my < GY || v < 0 || v >= NV) return -2;
    *col = s;
    return v;
}

void update(void) {
    for (int v = 0; v < NV; v++)
        if (keyp(VKEY[v])) { fire(v, 1, 0); flash[v] = 5; }

    if (keyp(KEY_SPACE) || tapp(248, 22, 64, 14)) { running = !running; last16 = -1; }   // tap PLAYING/STOPPED
    // < > switch presets — keys, or tap the label's arrows
    if (keyp(KEY_LEFT)  || tapp(8, 22, 20, 14))
        { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; }
    if (keyp(KEY_RIGHT) || (nextz_x >= 0 && tapp(nextz_x, 22, 20, 14)))
        { pre = (pre + 1) % NP;      load_preset(); last16 = -1; }
    if (keyp(KEY_UP)   || tapp(278, 7, 32, 12)) { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); }   // BPM halves
    if (keyp(KEY_DOWN) || tapp(248, 7, 30, 12)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); }
    if (keyp('Z') || tapp(196, 7, 22, 12)) { swing -= 2; if (swing < 50) swing = 50; }                      // SHF halves
    if (keyp('X') || tapp(218, 7, 24, 12)) { swing += 2; if (swing > 66) swing = 66; }
    if (keyp('S') || tapp(190, 23, 52, 13)) stroke_mode = !stroke_mode;   // STROKE mode: taps cycle flam/drag/ratchet

    // knob hover + drag (Y=coarse, X=fine, same as modrack)
    int mx = mouse_x(), my = mouse_y();
    hover_v = -1; hover_k = -1;
    if (my >= GY && my < GY + NV * SY) {
        int v = (my - GY) / SY;
        if      (mx >= K0X && mx < K0X + KW)                          { hover_v = v; hover_k = 0; }
        else if (mx >= K1X && mx < K1X + KW)                          { hover_v = v; hover_k = 1; }
        else if (mx >= K2X && mx < K2X + KW && K2LABEL[v] != NULL)    { hover_v = v; hover_k = 2; }
    }
    if (mouse_pressed(MOUSE_LEFT) && hover_v >= 0) {
        drag_v = hover_v; drag_k = hover_k; drag_y0 = my; drag_x0 = mx;
    }
    if (drag_v >= 0 && mouse_down(MOUSE_LEFT)) {
        float dy = drag_y0 - my;          // up = positive = increase (coarse)
        float dx = mx     - drag_x0;      // right = positive = increase (fine)
        float delta = dy / 80.0f + dx / 600.0f;
        float *kp = drag_k == 0 ? &ktune[drag_v]
                  : drag_k == 1 ? &kdecay[drag_v]
                  :               &kcolor[drag_v];
        float nv = *kp + delta;
        *kp = nv < 0.0f ? 0.0f : nv > 1.0f ? 1.0f : nv;
        drag_y0 = my; drag_x0 = mx;
    }
    if (!mouse_down(MOUSE_LEFT)) { drag_v = -1; drag_k = -1; }

    // metal pad: press inside starts a drag; keeps tracking outside the rect
    if (mouse_pressed(MOUSE_LEFT) &&
        mx >= PADX && mx < PADX + PADW && my >= PADY && my < PADY + PADH)
        pad_drag = true;
    if (!mouse_down(MOUSE_LEFT)) pad_drag = false;
    if (pad_drag) {
        float cx = (mx - PADX) / (float)(PADW - 1);
        float cy = 1.0f - (my - PADY) / (float)(PADH - 1);
        mcut = cx < 0.0f ? 0.0f : cx > 1.0f ? 1.0f : cx;
        mres = cy < 0.0f ? 0.0f : cy > 1.0f ? 1.0f : cy;
        apply_metal_filter();
    }

    // mouse: press toggles a cell; a horizontal drag paints on/off (as before),
    // a VERTICAL drag rides that cell's fire-chance (axis locked at first move).
    bool on_knob = hover_v >= 0 || drag_v >= 0;
    int mc, mr = grid_row(mx, my, &mc);
    if (mouse_pressed(MOUSE_LEFT) && !on_knob) {
        if (mr >= 0 && stroke_mode) {                    // STROKE mode: a tap cycles the stroke family
            if (!grid[mr][mc]) { grid[mr][mc] = true; gstroke[mr][mc] = ST_FLAM; }   // off → join at FLAM
            else gstroke[mr][mc] = (unsigned char)((gstroke[mr][mc] + 1) % NSTROKE);
        } else if (mr >= 0) {
            gest_on0 = grid[mr][mc]; gest_stroke0 = gstroke[mr][mc];   // remember before the toggle/reset
            paint_val = !grid[mr][mc];
            grid[mr][mc]    = paint_val;
            gstroke[mr][mc] = ST_PLAIN;
            gest_r = mr; gest_c = mc; gest_x0 = mx; gest_y0 = my; gest_axis = 0;
            probf  = gprob[mr][mc] / 100.0f;   // where a vertical drag would start from
        } else if (mr == -1) {
            paint_val = !gacc[mc];
            gacc[mc] = paint_val;
        } else if (mx < GX && my >= GY && my < GY + NV * SY) {
            int v = (my - GY) / SY;
            fire(v, 1, 0); flash[v] = 5;
        }
    } else if (mouse_down(MOUSE_LEFT) && !on_knob && gest_r >= 0) {
        int dx = mx - gest_x0, dy = my - gest_y0;
        int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
        if (gest_axis == 0 && (adx > 4 || ady > 4))     // moved → lock to an axis (paint / probability)
            gest_axis = ady >= adx ? 2 : 1;
        if (gest_axis == 2) {                            // vertical → probability of the START cell
            grid[gest_r][gest_c] = true;                 // a vertical drag means "keep this hit"
            gstroke[gest_r][gest_c] = gest_on0 ? gest_stroke0 : ST_PLAIN;  // keep the flam/drag/ratchet it had
            probf += (gest_y0 - my) / 40.0f;             // drag up = surer, down = flakier (~40px = full span)
            if (probf < 0.20f) probf = 0.20f;            // floor = the 25% bucket, never silent-off
            if (probf > 1.0f)  probf = 1.0f;
            gest_y0 = my;
            gprob[gest_r][gest_c] = snap_prob(probf);
        } else if (gest_axis == 1 && mr >= 0) {          // horizontal → paint on/off
            grid[mr][mc] = paint_val;
            if (!paint_val) gstroke[mr][mc] = ST_PLAIN;
            else            gprob[mr][mc]   = 100;        // a freshly-painted hit starts certain
        }
    } else if (mr == -1 && mouse_down(MOUSE_LEFT) && !on_knob) {
        gacc[mc] = paint_val;
    }
    if (!mouse_down(MOUSE_LEFT)) gest_r = -1;

    // RIGHT button cycles a cell through the stroke family (an off cell
    // joins at FLAM); right-drag paints the stroke the press landed on.
    if (mouse_pressed(MOUSE_RIGHT) && mr >= 0)
        paint_stroke = grid[mr][mc] ? (gstroke[mr][mc] + 1) % NSTROKE : ST_FLAM;
    if (mouse_down(MOUSE_RIGHT) && mr >= 0) {
        grid[mr][mc]    = true;
        gstroke[mr][mc] = (unsigned char)paint_stroke;
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
        // 909 shuffle: the EVEN 16ths drag (the 808 cart swings 8ths —
        // an anachronism there; here it's the real 1983 panel knob)
        if (nx & 1) delay += (swing - 50) * 2 * step_ms / 100;
        int boost   = gacc[nx] ? 2 : 0;
        for (int v = 0; v < NV; v++)
            if (grid[v][nx] && (gprob[v][nx] >= 100 || rnd(100) < gprob[v][nx]))
                fire_stroke(v, gstroke[v][nx], boost, delay, step_ms);   // trig-prob roll

        if (first) {   // fresh start: also sound the step we're already on
            int b0 = gacc[playhead] ? 2 : 0;
            for (int v = 0; v < NV; v++)
                if (grid[v][playhead] && (gprob[v][playhead] >= 100 || rnd(100) < gprob[v][playhead]))
                    fire(v, b0, 0);
        }
    }
}

void draw(void) {
    const Pat *p = &PRESET[pre];
    char buf[32];

    // cream face, orange stripe, black step section — the 909 look
    cls(CLR_DARK_GREY);
    rectfill(6, 8, 308, 188, CLR_LIGHT_GREY);
    rect(6, 8, 308, 188, CLR_WHITE);
    rectfill(6, 8, 308, 14, CLR_ORANGE);                  // the orange band
    rectfill(6, GY - 10, 308, NV * SY + 12, CLR_BROWNISH_BLACK);  // step section

    print("RHYTHM COMPOSER TR-909", 14, 11, CLR_BROWNISH_BLACK);
    sprintf(buf, "SHF%2d", swing);
    print(buf, 200, 11, swing > 50 ? CLR_DARK_RED : CLR_DARK_GREY);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 252, 11, CLR_BROWNISH_BLACK);
    int nx = print("< ", 14, 26, CLR_DARK_RED);    // the arrows are tap targets
    nx = print(p->name, nx, 26, CLR_DARK_RED);
    print(" >", nx, 26, CLR_DARK_RED);
    nextz_x = nx + 2;
    print(running ? "PLAYING" : "STOPPED", 252, 26, running ? CLR_MEDIUM_GREEN : CLR_RED);

    // STROKE-mode toggle: when lit, a tap on a cell cycles flam/drag/ratchet
    rectfill(190, 23, 52, 13, stroke_mode ? CLR_DARK_RED : CLR_DARKER_GREY);
    rect(190, 23, 52, 13, stroke_mode ? CLR_ORANGE : CLR_DARK_GREY);
    print("STROKE", 197, 26, stroke_mode ? CLR_WHITE : CLR_MEDIUM_GREY);

    // playhead column
    if (running)
        rectfill(GX + playhead * SX - 1, GY - 7, SX - 1, NV * SY + 7, CLR_DARKER_GREY);

    // accent strip (clickable) — TOTAL ACCENT, red on the 909
    for (int s = 0; s < STEPS; s++)
        rectfill(GX + s * SX, GY - 6, SX - 4, 3,
                 gacc[s] ? CLR_RED : CLR_DARK_GREY);

    for (int v = 0; v < NV; v++) {
        int y = GY + v * SY;
        sprintf(buf, "%c", VKEY[v]);
        print(buf, 14, y, flash[v] > 0 ? CLR_WHITE : CLR_ORANGE);
        print(VNAME[v], 26, y, flash[v] > 0 ? CLR_WHITE : CLR_LIGHT_GREY);
        int base_col = flash[v] > 0 ? CLR_WHITE : CLR_LIGHT_GREY;
        draw_knob(K0X, y, ktune[v],  (drag_v==v&&drag_k==0) ? CLR_YELLOW : base_col);
        draw_knob(K1X, y, kdecay[v], (drag_v==v&&drag_k==1) ? CLR_YELLOW : base_col);
        if (K2LABEL[v])
            draw_knob(K2X, y, kcolor[v], (drag_v==v&&drag_k==2) ? CLR_YELLOW : base_col);
        for (int s = 0; s < STEPS; s++) {
            int x = GX + s * SX;
            // 909 step buttons: uniform grey keys, orange when lit, the
            // first step of each quarter gets a white tick (beat marker)
            if (grid[v][s]) {
                int c = (flash[v] > 0 && s == playhead && running) ? CLR_WHITE : CLR_ORANGE;
                // trig-prob: a less-than-certain hit is a SHORTER bar, drained
                // from the top (its full-height socket stays outlined below).
                int bh = 2 + gprob[v][s] * 5 / 100;   // 25%→3px .. 100%→7px
                int by = y + 7 - bh;
                if (gprob[v][s] < 100) rect(x, y, SX - 4, 7, CLR_DARK_GREY);
                switch (gstroke[v][s]) {
                case ST_FLAM:     // grace tick + main bar
                    rectfill(x, by, 2, bh, c);
                    rectfill(x + 4, by, SX - 8, bh, c);
                    break;
                case ST_DRAG:     // two grace ticks + main bar
                    rectfill(x, by, 1, bh, c);
                    rectfill(x + 2, by, 1, bh, c);
                    rectfill(x + 4, by, SX - 8, bh, c);
                    break;
                case ST_RATCHET:  // four even chops
                    for (int k = 0; k < 4; k++)
                        rectfill(x + 1 + k * 2, by, 1, bh, c);
                    break;
                default:
                    rectfill(x, by, SX - 4, bh, c);
                }
            } else
                rect(x, y, SX - 4, 7, (s & 3) == 0 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
        }
        if (flash[v] > 0) flash[v]--;
    }

    // header label — a live probability readout while dragging a cell, else the hovered knob
    int hv = (drag_v >= 0) ? drag_v : hover_v;
    int hk = (drag_v >= 0) ? drag_k : hover_k;
    if ((gest_axis == 2 || gest_axis == 3) && gest_r >= 0) {
        char buf2[24];
        if (gest_axis == 2) sprintf(buf2, "%s %d%%", VNAME[gest_r], gprob[gest_r][gest_c]);
        else                sprintf(buf2, "%s %s", VNAME[gest_r], SNAME[gstroke[gest_r][gest_c]]);
        font(FONT_SMALL);
        print(buf2, 168, 27, CLR_DARK_RED);
        font(FONT_NORMAL);
    } else if (hv >= 0 && hk >= 0) {
        const char *lbl = hk == 2 ? K2LABEL[hv] : KLABEL[hk];
        if (lbl) {
            char buf2[24];
            sprintf(buf2, "%s %s", VNAME[hv], lbl);
            font(FONT_SMALL);
            print(buf2, 168, 27, CLR_DARK_RED);
            font(FONT_NORMAL);
        }
    }

    // metal-filter XY pad — crosshair dot, grid hairlines at the neutral point
    rectfill(PADX, PADY, PADW, PADH, CLR_WHITE);
    rect(PADX, PADY, PADW, PADH, CLR_MEDIUM_GREY);
    int px = PADX + (int)(mcut * (PADW - 1) + 0.5f);
    int py = PADY + (int)((1.0f - mres) * (PADH - 1) + 0.5f);
    line(px, PADY + 1, px, PADY + PADH - 2, CLR_LIGHT_GREY);
    line(PADX + 1, py, PADX + PADW - 2, py, CLR_LIGHT_GREY);
    rectfill(px - 1, py - 1, 3, 3, pad_drag ? CLR_RED : CLR_DARK_RED);
    font(FONT_SMALL);
    print("METAL", PADX + 2, PADY - 7, CLR_DARK_GREY);
    print("CUT>", PADX - 22, PADY + PADH - 7, CLR_DARK_GREY);
    print("RES^", PADX - 22, PADY, CLR_DARK_GREY);
    font(FONT_NORMAL);

    font(FONT_SMALL);
    print("<> PRESET  ^v BPM  Z/X SHFL   V-drag=PROB   STROKE btn: tap=flam", 14, 189, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
