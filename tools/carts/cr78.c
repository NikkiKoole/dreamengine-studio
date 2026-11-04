/* de:meta
{
  "slug": "cr78",
  "title": "cr-78 compurhythm",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "subtractive-synth",
    "additive-synth",
    "drum-synthesis",
    "analog-voice-modeling",
    "swing-timing"
  ],
  "lineage": "Roland CR-78 (1978) homage; extends drummachine's grid by rebuilding each analog voice from its circuit description — one shared noise source filtered per voice, beating detuned squares for metal, and INSTR_SCRAPE for the guiro (it began as a 36Hz volume-LFO chop on noise; measured against a real CR-78 guiro sample that had the wrong rate, body and no gaps). 2026-11-04: every voice retuned against a real CR-78 sample set (pitch, band, decay and level), once an engine filter fix made its narrow 8 kHz hat band reachable.",
  "homage": "Roland CR-78 CompuRhythm (1978)",
  "description": "All the voices of the Roland CR-78 (1978) rebuilt from their analog circuits with the synth API: damped-sine membranes (kick/bongos/congas), one noise source shaped per voice by different filters (the famous swishy hats are a narrow band at 8.4 kHz, the tambourine an even narrower ring at 4.65 kHz), a detuned square pair for the cowbell and three high tones for the metallic beat, every voice tuned and levelled against a real CR-78 sample set, and the guiro's ratcheting scrape — 75 teeth a second, each its own click ringing a 6 kHz gourd, plus the machine's second, LONG guiro: one stroke held 820 ms (INSTR_SCRAPE, both fitted to real CR-78 guiro samples). Plays eight classic presets including Disco-2 (In the Air Tonight), the Mambo+Beguine blend (Heart of Glass), and the triplet-feel Shuffle and Swing — plus the knob the original never got: Z/X sets swing 50-66% by scheduling the offbeat 8ths late (adjustable swing didn't exist until the LM-1 in 1980). The grid is editable: click/drag cells to toggle, click the orange strip to move accents, click a label to audition; accent steps are a volume boost, just like the real knob. Q-H play voices by hand, LEFT/RIGHT preset, UP/DOWN tempo, SPACE start/stop."
}
de:meta */
#include "studio.h"
#include <stdio.h>

// ── CR-78 COMPURHYTHM ─────────────────────────────────────────────────────
// An homage to the Roland CR-78 (1978) — the wood-panelled preset rhythm box
// behind "In the Air Tonight" (Disco-2) and "Heart of Glass" (Mambo+Beguine).
// Every voice on the original is pure analog circuitry, no samples — which
// means every voice can be rebuilt with this synth API. This cart does that,
// one instrument slot per voice, and plays the classic preset patterns.
//
// How the real machine makes each sound, and how we copy it:
//
//   tuned voices (kick, bongos, congas, rim, claves)
//     a "bridged-T" network — a damped sine that rings at a fixed pitch when
//     kicked by a trigger pulse.  →  INSTR_SINE/TRI, fast-decay amp envelope.
//     Pitches measured off the real machine: kick 65 Hz, conga 196, bongos
//     587 and 392 (the low one with a second mode at 2.5 kHz), claves 2.46 kHz.
//
//   noise voices (hats, cymbal, maracas, tambourine, snare rattle)
//     ONE white-noise source, shaped per voice by a different filter.
//     →  INSTR_NOISE through instrument_filter(); the filter IS the voice.
//        The CR-78 "swish" is NARROW, not broad hiss: hats 7.4-9.7 kHz,
//        cymbal the same band held long, maracas ~8.7 kHz, tambourine a ring
//        at 4.65 kHz. (Before 2026-11-04 the engine's filter could not reach
//        these: it screamed at ~20 kHz above ~6 kHz, see svf_step.)
//
//   metallic voices (cowbell, metallic beat)
//     the cowbell is a detuned square pair (554 / 784 Hz) lowpassed until it
//     is nearly two sines. The metallic beat rings HIGH: tones at 3.9, 5.4 and
//     5.9 kHz on the real machine, so three triangle hits up there.
//
//   how the numbers were found: each voice rendered once and measured
//     against its real CR-78 one-shot (Boxed Ear's free pack): pitch, the
//     10/50/90% energy band, time to -20 dB, and peak level relative to the
//     kick (the pack was recorded at one gain: each accent hit is ~4.5 dB
//     over its normal hit), then the levels matched with instrument_level().
//
//   guiro — the strangest circuit in the box
//     a noise burst whose loudness is chopped into rapid "teeth" to make the
//     ratcheting scrape.  →  INSTR_SCRAPE (guiro recipe: body 0, rate 0.87,
//     gesture 0). This cart first read that as bandpassed noise + LFO_VOLUME
//     at 36Hz under a rising ENV_CUTOFF, and a real CR-78 guiro sample
//     (Puppetmaker 2's cr78/guiro1, tools/scrape-check.js --ref) says that was
//     wrong three ways: the machine scrapes at ~75 teeth/s not 36, through a
//     narrow ~6 kHz body (the LFO patch spread 2.8-17 kHz), with real gaps
//     between teeth (-55 dB; the LFO patch never got below -8.5 dB).
//
//   accent — not a sound! On the real unit it's a knob that just makes the
//     flagged steps louder. Here: orange ticks above the grid, vol +2.
//
//   swing — on the real CR-78, "Swing" and "Shuffle" were canned PRESETS
//     with the triplet feel baked into the pattern data; adjustable swing
//     timing didn't exist until the LM-1 (1980). This cart has the knob the
//     original never got: Z/X sets swing 50 (straight) to 66 (full triplet)
//     by scheduling the offbeat 8ths late. Shuffle/Swing presets load at 66.
//
//   Q W E R T Y U I O P A S D F G H   play each voice by hand
//   LEFT / RIGHT  preset (or tap the < > arrows on the label)
//   UP / DOWN  tempo      SPACE  start / stop
//   Z / X  swing down / up
//   MOUSE  click/drag cells to edit the pattern (presets are starting
//          points — switching preset reloads it), click a label to
//          audition, click the orange strip to move the accents
//
// ── PARKED IDEA: per-cell pitch + filter locks ───────────────────────────
// (also ledgered in docs/STATUS.md → Open). Elektron-style parameter locks:
// each lit cell optionally carries a pitch offset and a cutoff override, so
// the bongo row plays a melody and a hat line opens across the bar. That
// would stack three eras in one box: CR-78 voices (1978) + the swing knob
// (LM-1, 1980) + p-locks (Machinedrum, 2001). Design notes from the session:
//   - pitch is trivial: cells become a small struct {on, semi, cut}; fire()
//     adds `semi` to the voice's base midi. Done.
//   - filter is the catch: cutoff lives on the INSTRUMENT SLOT, and a
//     scheduled note snapshots its slot AT FIRE TIME (audio-notes §2.2) —
//     redefining the slot per step would retroactively bend the note already
//     queued one step ahead. Cure = the sfx-editor rotating-slot pattern:
//     copy the voice's recipe into a scratch slot (25..31 are free), set the
//     cell's cutoff, fire that slot. Pool of 2-3 suffices at one-step
//     lookahead.
//   - UI on 9x7px cells: hover + wheel = pitch lock, hold C + wheel = cutoff
//     lock, right-click clears; notch markers (top = pitched up, bottom =
//     down, off-color = filtered) since there's no room for digits.

// voices — named indices, never raw numbers (house rule)
enum {
    V_KICK, V_SNARE, V_RIM, V_HATC, V_HATO, V_CYM, V_METAL, V_COW,
    V_TAMB, V_MAR, V_CLV, V_GUIRO, V_BONH, V_BONL, V_CONGA, V_GUIROL, NV
};

// instrument slots (5..31 are free; 5-8 are the user waves, start at 9)
enum {
    SL_KICK = 9, SL_SNB, SL_SNN, SL_RIM, SL_HATC, SL_HATO,
    SL_CYMN, SL_TAMB, SL_COW, SL_CLV, SL_MAR, SL_BON, SL_CON, SL_GUI, SL_MET,
    SL_BONP, SL_RIMT, SL_GUIL
};

static const char *VNAME[NV] = {
    "KICK", "SNARE", "RIM", "HAT CL", "HAT OP", "CYMBAL", "METAL", "COWBELL",
    "TAMB", "MARACAS", "CLAVES", "GUIRO", "BONGO H", "BONGO L", "CONGA", "GUIRO L"
};
static const char VKEY[NV] = {
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'A', 'S', 'D', 'F', 'G', 'H'
};
static const int VCLR[NV] = {
    CLR_RED, CLR_ORANGE, CLR_PEACH, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_WHITE,
    CLR_BLUE, CLR_GREEN, CLR_PINK, CLR_LIGHT_PEACH, CLR_DARK_ORANGE,
    CLR_LIME_GREEN, CLR_INDIGO, CLR_INDIGO, CLR_DARK_PEACH, CLR_LIME_GREEN
};

#define STEPS 16

#define GX   88    // grid left edge
#define GY   44    // grid top edge
#define SX   13    // column stride
#define SY   9     // row stride

typedef struct {
    const char *name;
    int         tempo;
    const char *row[NV];   // 16 chars, 'x' = hit; NULL = silent row
    const char *accent;    // 16 chars, 'x' = +2 volume on that step
    int         swing;     // 50 = straight, 66 = triplet feel (offbeat 8ths late)
} Pat;

static const Pat PRESET[] = {
    { "ROCK-1", 118, {
        [V_KICK]  = "x.......x.x.....",
        [V_SNARE] = "....x.......x...",
        [V_HATC]  = "x.x.x.x.x.x.x.x.",
      }, "x.......x......." },
    { "DISCO-2", 100, {            // the "In the Air Tonight" preset
        [V_KICK]  = "x...x...x...x...",
        [V_HATC]  = "xx.xxx.xxx.xxx.x",
        [V_HATO]  = "..x...x...x...x.",
        [V_TAMB]  = "....x.......x...",
        [V_BONH]  = ".........x.....x",
        [V_GUIRO] = "............x...",
      }, "x.......x......." },
    { "BOSSA NOVA", 132, {
        [V_KICK]  = "x.....x.x.....x.",
        [V_RIM]   = "x..x..x...x..x..",   // bossa clave on the rim
        [V_HATC]  = "x.x.x.x.x.x.x.x.",
        [V_MAR]   = "xxxxxxxxxxxxxxxx",
      }, "x.......x......." },
    { "MAMBO", 176, {              // half of the "Heart of Glass" blend
        [V_KICK]  = "x.......x.......",
        [V_COW]   = "x..x..x.x..x..x.",
        [V_CONGA] = "..x...x...x...x.",
        [V_BONH]  = ".x......x.x.....",
        [V_CLV]   = "x..x..x...x.x...",   // son clave 3-2
      }, "x.......x......." },
    { "CHA-CHA", 118, {
        [V_GUIRO] = "x...x...x...x...",
        [V_COW]   = "x...x...x...x...",
        [V_KICK]  = "....x.......x...",
        [V_CONGA] = "......x.......xx",
        [V_BONL]  = "..x.......x.....",
      }, "x.......x......." },
    { "BEGUINE", 104, {            // the other half of "Heart of Glass"
        [V_KICK]  = "x.....x...x.....",
        [V_METAL] = "x...x...x...x...",   // the Kraftwerkian clang
        [V_RIM]   = "...x...x.....x..",
        [V_HATC]  = "x.x.x.x.x.x.x.x.",
        [V_BONL]  = "..............x.",
      }, "x.........x....." },
    { "SHUFFLE", 96, {             // triplet feel baked in — swing 66
        [V_KICK]  = "x.......x.......",
        [V_SNARE] = "....x.......x...",
        [V_HATC]  = "x.x.x.x.x.x.x.x.",
      }, "x.......x.......", 66 },
    { "SWING", 138, {              // jazz ride on the closed hat, rim on 2+4
        [V_KICK]  = "x.......x.......",
        [V_HATC]  = "x...x.x.x...x.x.",
        [V_RIM]   = "....x.......x...",
      }, "x.......x.......", 66 },
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int  pre      = 1;      // start on DISCO-2, obviously
static int  tempo    = 100;
static bool running  = true;
static int  last16   = -1;
static int  playhead = 0;
static int  flash[NV];
static bool grid[NV][STEPS];   // the live pattern — editable, loaded from preset
static int  nextz_x = -1;      // the preset '>' tap zone, recorded by draw() ('<' is fixed)
static bool gacc[STEPS];       // live accent row
static bool paint_val;         // what a click-drag writes (set on press)
static int  swing = 50;        // 50 = straight .. 66 = full triplet

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
}

static int vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice, sample-accurately `delay` ms from now. Layered voices are
// several hit()s — the detuned pairs/triples ARE the metallic timbres.
static void fire(int v, int boost, int delay) {
    switch (v) {
    case V_KICK:  schedule_hit(delay, 36, SL_KICK, vv(5, boost), 220); break;   // 65 Hz
    case V_SNARE: // tonal shell (311 Hz) + a rattle as loud as the shell, 2-10 kHz
        schedule_hit(delay, 63, SL_SNB, vv(3, boost), 100);
        schedule_hit(delay, 60, SL_SNN, vv(6, boost), 120);
        break;
    case V_RIM:   // a click: a 1.4 kHz ping over a low knock, both gone in ~10 ms
        schedule_hit(delay, 89, SL_RIM,  vv(7, boost), 30);
        schedule_hit(delay, 58, SL_RIMT, vv(2, boost), 30);
        break;
    case V_HATC:  schedule_hit(delay, 90, SL_HATC, vv(3, boost), 40); break;
    case V_HATO:  schedule_hit(delay, 90, SL_HATO, vv(3, boost), 360); break;
    case V_CYM:   schedule_hit(delay, 90, SL_CYMN, vv(3, boost), 600); break;   // a long 8.5 kHz band
    case V_METAL: // three tones up where the real one rings: 3.9 / 5.3 / 5.9 kHz
        schedule_hit(delay, 114, SL_MET, vv(4, boost), 120);
        schedule_hit(delay, 112, SL_MET, vv(4, boost), 120);
        schedule_hit(delay, 107, SL_MET, vv(1, boost), 120);
        break;
    case V_COW:   // detuned pair 554 / 784 Hz (the sample: 549 / 775) — the beating IS the cowbell
        schedule_hit(delay, 79, SL_COW, vv(4, boost), 150);
        schedule_hit(delay, 73, SL_COW, vv(3, boost), 150);
        break;
    case V_TAMB:  schedule_hit(delay, 80, SL_TAMB, vv(3, boost), 300); break;
    case V_MAR:   schedule_hit(delay, 90, SL_MAR, vv(3, boost), 30); break;
    case V_CLV:   schedule_hit(delay, 99, SL_CLV, vv(4, boost), 60); break;    // 2.46 kHz
    case V_GUIRO: schedule_hit(delay, 60, SL_GUI, vv(4, boost), 270); break;   // C4 = SCRAPE's body as voiced; ~270 ms = the sample's scrape
    case V_BONH:  schedule_hit(delay, 74, SL_BON, vv(4, boost), 140); break;   // 587 Hz
    case V_BONL:  // 392 Hz with a bright second mode at 2.5 kHz, 5 dB down
        schedule_hit(delay, 67, SL_BON,  vv(3, boost), 80);
        schedule_hit(delay, 99, SL_BONP, vv(3, boost), 40);
        break;
    case V_CONGA: schedule_hit(delay, 55, SL_CON, vv(4, boost), 300); break;   // 196 Hz
    case V_GUIROL: schedule_hit(delay, 60, SL_GUIL, vv(4, boost), 820); break; // the LONG scrape: ~820 ms, 83 teeth/s
    }
}

void init(void) {
    // ── the voice circuits ──────────────────────────────────────────────
    // Every number below was measured off a real CR-78 one-shot (the Boxed Ear
    // pack): pitch, band and decay per voice. See the header for the method.
    // kick — a soft round 65 Hz sine, barely any pitch drop (NOT an 808 punch)
    instrument(SL_KICK, INSTR_SINE, 0, 140, 0, 40);
    instrument_filter(SL_KICK, FILTER_LOW, 320, 2);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 30, 2.0f);

    // snare shell — a short 311 Hz ring
    instrument(SL_SNB, INSTR_SINE, 0, 60, 0, 20);
    instrument_env(SL_SNB, 0, ENV_PITCH, 0, 15, 2.0f);
    instrument_level(SL_SNB, 0.15f);      // the rattle carries the snare: its midpoint sits at ~5 kHz
    // snare rattle — noise up to ~10 kHz, far louder than the shell. A LOWPASS
    // with a resonant edge, not a bandpass: a low-Q bandpass leaves a long tail
    // to 16-20 kHz, which the real rattle does not have
    instrument(SL_SNN, INSTR_NOISE, 0, 70, 0, 25);
    instrument_filter(SL_SNN, FILTER_LOW, 8000, 6);

    // rim — a 1.4 kHz ping plus a low knock, both gone in ~10 ms
    instrument(SL_RIM, INSTR_TRI, 0, 10, 0, 5);
    instrument_filter(SL_RIM, FILTER_BAND, 1400, 6);
    instrument(SL_RIMT, INSTR_SINE, 0, 12, 0, 5);

    // hats — the CR-78 "swish" is a NARROW band, 7.4-9.7 kHz (resonance 14),
    // not broad hiss. closed and open are the same band, only the decay differs
    instrument(SL_HATC, INSTR_NOISE, 0, 22, 0, 10);
    instrument_filter(SL_HATC, FILTER_BAND, 8400, 14);
    instrument(SL_HATO, INSTR_NOISE, 0, 320, 0, 80);
    instrument_filter(SL_HATO, FILTER_BAND, 8400, 14);

    // cymbal — the same narrow band a little higher (8.5 kHz), long
    instrument(SL_CYMN, INSTR_NOISE, 0, 400, 0, 150);
    instrument_filter(SL_CYMN, FILTER_BAND, 8600, 14);

    // tambourine — a very narrow ring at 4.65 kHz, shaken in (soft attack)
    instrument(SL_TAMB, INSTR_NOISE, 8, 200, 0, 60);
    instrument_filter(SL_TAMB, FILTER_BAND, 4650, 15);

    // cowbell — square slot, fired twice detuned (see fire()), lowpassed until
    // it is nearly two sines: the real one's 3rd harmonics sit 11-16 dB down
    instrument(SL_COW, INSTR_SQUARE, 0, 55, 0, 25);
    instrument_filter(SL_COW, FILTER_LOW, 1100, 2);

    // claves — a pure 2.46 kHz ping
    instrument(SL_CLV, INSTR_SINE, 0, 40, 0, 12);
    instrument_tune(SL_CLV, -0.23f);

    // maracas — the shortest noise burst in the box, a narrow band at 8.7 kHz
    instrument(SL_MAR, INSTR_NOISE, 0, 15, 0, 8);
    instrument_filter(SL_MAR, FILTER_BAND, 8700, 14);

    // bongos / conga — bare damped sines at membrane pitches
    instrument(SL_BON, INSTR_SINE, 0, 40, 0, 25);
    instrument(SL_BONP, INSTR_SINE, 0, 25, 0, 10);       // low bongo's 2.5 kHz mode
    instrument(SL_CON, INSTR_SINE, 0, 160, 0, 60);

    // guiro — teeth through a gourd: the scrape-check fit to a real CR-78
    // guiro sample (75 teeth/s, 6 kHz body)
    instrument(SL_GUI, INSTR_SCRAPE, 0, 0, 7, 30);
    instrument_harmonics(SL_GUI, 0.0f);    // BODY: gourd
    instrument_timbre(SL_GUI, 0.87f);      // RATE: ~75 teeth/s
    instrument_morph(SL_GUI, 0.0f);        // GESTURE: a steady scrape
    instrument_level(SL_GUI, 0.81f);      // 0.9 matched the old LFO patch; -0.9 dB more to sit at the sample's level
    // the long guiro — the machine's second guiro sound (Guiro 2 in the pack):
    // the same gourd, one steady stroke held ~820 ms, a touch faster (83/s)
    instrument(SL_GUIL, INSTR_SCRAPE, 0, 0, 7, 30);
    instrument_harmonics(SL_GUIL, 0.0f);   // BODY: gourd
    instrument_timbre(SL_GUIL, 0.92f);     // RATE: 10 * 10^0.92 = 83 teeth/s
    instrument_morph(SL_GUIL, 0.0f);       // GESTURE: a steady scrape
    instrument_level(SL_GUIL, 0.56f);     // the real long scrape sits 1.9 dB under the short one

    // metallic beat — one triangle slot, fired three times up at 3.9-5.9 kHz,
    // through a band that keeps the fundamentals and drops their harmonics
    // (a square's 3rd harmonic of 5.9 kHz landed at 17.7 kHz, 7 dB down)
    instrument(SL_MET, INSTR_TRI, 0, 60, 0, 25);
    instrument_filter(SL_MET, FILTER_BAND, 5400, 6);

    // ── the mix: each voice's peak relative to the kick, matched to the real
    // samples (recorded at one gain: every accent hit is ~4.5 dB over its
    // normal hit). Cuts only: instrument_level is 0..1.
    instrument_level(SL_HATC, 0.51f);    // -5.9 dB
    instrument_level(SL_HATO, 0.51f);
    instrument_level(SL_CYMN, 0.25f);    // -11.9 dB
    instrument_level(SL_MET,  0.36f);    // -8.8 dB
    instrument_level(SL_COW,  0.35f);    // -9.2 dB
    instrument_level(SL_TAMB, 0.18f);    // -14.7 dB
    instrument_level(SL_MAR,  0.66f);    // -3.6 dB

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

    if (keyp(KEY_SPACE) || tapp(244, 24, 64, 11)) { running = !running; last16 = -1; }   // tap PLAYING/STOPPED
    // < > switch presets — keys, or tap the label's arrows
    if (keyp(KEY_LEFT)  || tapp(10, 24, 20, 11))
        { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; }
    if (keyp(KEY_RIGHT) || (nextz_x >= 0 && tapp(nextz_x, 24, 20, 11)))
        { pre = (pre + 1) % NP;      load_preset(); last16 = -1; }
    if (keyp(KEY_UP)   || tapp(274, 12, 32, 12)) { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); }   // BPM halves
    if (keyp(KEY_DOWN) || tapp(244, 12, 30, 12)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); }
    if (keyp('Z') || tapp(200, 12, 18, 12)) { swing -= 2; if (swing < 50) swing = 50; }                      // SW halves
    if (keyp('X') || tapp(218, 12, 20, 12)) { swing += 2; if (swing > 66) swing = 66; }

    // mouse: press toggles a cell (and remembers the value so a drag PAINTS
    // it across cells); a click on a row's label auditions the voice
    int mc, mr = grid_row(mouse_x(), mouse_y(), &mc);
    if (mouse_pressed(MOUSE_LEFT)) {
        if (mr >= 0) {
            paint_val = !grid[mr][mc];
            grid[mr][mc] = paint_val;
            if (paint_val) { fire(mr, 0, 0); flash[mr] = 5; }
        } else if (mr == -1) {
            paint_val = !gacc[mc];
            gacc[mc] = paint_val;
        } else if (mouse_x() < GX && mouse_y() >= GY && mouse_y() < GY + NV * SY) {
            int v = (mouse_y() - GY) / SY;
            fire(v, 1, 0); flash[v] = 5;
        }
    } else if (mouse_down(MOUSE_LEFT)) {
        if (mr >= 0)       grid[mr][mc] = paint_val;
        else if (mr == -1) gacc[mc]     = paint_val;
    }

    if (!running) return;

    // sixteenth-note clock off the synth's own beat counter; each step is
    // scheduled ONE STEP AHEAD with schedule_hit() so hits land sample-
    // accurate, free of frame jitter (the bossa.c pattern)
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
        // swing: push each offbeat 8th (steps 2,6,10,14) late. At 66 the
        // offbeat lands 2/3 into the quarter — the triplet shuffle the real
        // CR-78 could only fake with pre-swung preset patterns
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

    // walnut cabinet + black panel
    cls(CLR_DARK_BROWN);
    for (int y = 3; y < 200; y += 7) line(0, y, 319, y, CLR_BROWNISH_BLACK);
    rectfill(8, 10, 304, 186, CLR_BLACK);
    rect(8, 10, 304, 186, CLR_DARK_GREY);

    print("COMPURHYTHM CR-78", 16, 16, CLR_LIGHT_GREY);
    sprintf(buf, "SW%2d", swing);
    print(buf, 204, 16, swing > 50 ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 248, 16, CLR_LIGHT_PEACH);
    int nx = print("< ", 16, 28, CLR_ORANGE);      // the arrows are tap targets
    nx = print(p->name, nx, 28, CLR_ORANGE);
    print(" >", nx, 28, CLR_ORANGE);
    nextz_x = nx + 2;
    print(running ? "PLAYING" : "STOPPED", 248, 28, running ? CLR_GREEN : CLR_RED);

    // playhead column
    if (running)
        rectfill(GX + playhead * SX - 1, GY - 7, SX - 1, NV * SY + 8, CLR_DARKER_GREY);

    // accent ticks above the grid (the real unit's accent = louder steps);
    // off slots stay faintly visible so you can see they're clickable
    for (int s = 0; s < STEPS; s++)
        rectfill(GX + s * SX, GY - 6, SX - 4, 3,
                 gacc[s] ? CLR_ORANGE : CLR_DARKER_GREY);

    for (int v = 0; v < NV; v++) {
        int y = GY + v * SY;
        sprintf(buf, "%c", VKEY[v]);
        print(buf, 14, y, flash[v] > 0 ? CLR_WHITE : CLR_YELLOW);
        print(VNAME[v], 26, y, flash[v] > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
        for (int s = 0; s < STEPS; s++) {
            int x   = GX + s * SX;
            bool on = grid[v][s];
            int  c  = on ? ((flash[v] > 0 && s == playhead && running) ? CLR_WHITE : VCLR[v])
                         : ((s & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
            if (on) rectfill(x, y, SX - 4, 7, c);
            else    rect(x, y, SX - 4, 7, c);
        }
        if (flash[v] > 0) flash[v]--;
    }

    print("CLICK TO EDIT  <> PRESET  ^v BPM  SPC", 14, 187, CLR_MEDIUM_GREY);
}
