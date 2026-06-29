/* de:meta
{
  "title": "tr-808 rhythm composer",
  "status": "active",
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
  "lineage": "Roland TR-808 (1980), sibling of cr78; voices modeled from reverse-engineered MEASURED circuit values — the shared six-oscillator metal bank, bridged-T kick with pitch-drop boom, dual-mode snare — with per-voice tune/decay/character knobs.",
  "homage": "Roland TR-808 Rhythm Composer (1980)",
  "description": "The big one — the TR-808 (1980), sibling cart to the cr-78 with the same editable grid, but built from the 808's actual reverse-engineered circuit values: the six-oscillator metal bank (800/540/522.7/369.6/304.4/205.3 Hz squares) shared by hats, cymbal and cowbell; the ~50Hz bridged-T kick with the famous decay-knob boom (+26 semitone pitch drop); the 180+330Hz dual-mode snare under snappy noise; rimshot/claves as one dual bridged-T circuit (1667+455 Hz vs 2500 Hz); and the handclap's three noise retriggers ~10ms apart plus a room tail. 16 voices, the iconic red/orange/yellow/white quarter-colored step buttons, six presets (Planet Rock, Healing, House, Electro, Boom Bap, Cowbell Jam), accents, and the anachronistic Z/X swing knob. Each voice has up to three 8×8 rotary knobs: T (tune ±12 semitones), D (decay 0.25×–4×), and a voice-specific character knob (SNPY noise/tone on snare, THUD on toms, TONE/RING on cowbell/cymbal/open hat). Drag Y=coarse, X=fine. Presets load with tuned knob settings. Closed hat chokes open hat. Q-H play voices, LEFT/RIGHT preset, UP/DOWN tempo, SPACE start/stop, click name to audition, drag knobs to shape."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── TR-808 RHYTHM COMPOSER ────────────────────────────────────────────────
// The big one. Roland's TR-808 (1980) — commercial flop, then the foundation
// of hip-hop, electro and house ("Planet Rock", "Sexual Healing", and the
// boom in every trunk since). Sibling cart to cr78.c (same editable grid,
// same swing knob); where the CR-78 is soft and round, the 808 is punchy and
// boomy. All-analog, and unlike the CR-78 it's been reverse-engineered to
// component level, so most voice values here are MEASURED, not guessed:
//
//   the metal bank — hats, cymbal and cowbell all share ONE bank of six
//     Schmitt-trigger square oscillators at 800 / 540 / 522.7 / 369.6 /
//     304.4 / 205.3 Hz. Hats = bank through a ~7kHz highpass (closed ~50ms,
//     open up to 600ms — same circuit, different decay). Cymbal = bank
//     through bandpasses at 7100/3440Hz, ringing up to 1.2s. Cowbell = just
//     oscillators 1+2 (540+800Hz) through a ~2.6kHz bandpass. We fire 2-3
//     squares per hit (full six would eat the 8-voice polyphony).
//   kick — bridged-T damped sine ~50Hz with the famous DECAY knob boom;
//     here: low sine, +26 semitone pitch drop over 50ms, ~500ms tail,
//     plus a touch of instrument_drive() — the saturated 808 sub that
//     Miami bass ran hot into the desk from day one.
//   snare — two bridged-T modes at 180Hz + 330Hz under highpassed
//     "snappy" noise. Three layers, just like the schematic.
//   rimshot / claves — the same dual bridged-T circuit, switched: rimshot
//     rings 1667Hz + 455Hz; claves retunes it to 2500Hz (MIDI 99 exactly).
//   toms / congas — shared circuits too: toms = sine + a low noise thud,
//     congas = the same sine cleaner, higher and shorter (tunings are the
//     one estimated thing here — the service manual doesn't list them).
//   handclap — bandpassed noise hit by THREE fast retriggers ~10ms apart,
//     then a softer ~140ms "room" tail. The retrigger spacing IS the clap.
//   accent — like the CR-78 cart: flagged steps play louder (orange strip).
//   swing — the 808 never had it either (TR-909 got shuffle in 1983, after
//     the LM-1 proved it). Z/X = our anachronistic knob, 50..66%.
//
// Known infidelity: real 808 closed hats CHOKE the open hat; our one-shot
// notes can't cut each other, so an open hat rings through a closed hit.
//
//   Q W E R T Y U I O P A S D F G H   play each voice by hand
//   LEFT / RIGHT  preset (or tap the < > arrows on the label)
//   UP / DOWN  tempo      SPACE  start / stop
//   Z / X  swing down / up
//   MOUSE  click/drag cells to edit, click the strip above for accents,
//          click a label to audition

// voices — named indices, never raw numbers (house rule)
enum {
    V_BD, V_SD, V_LT, V_MT, V_HT, V_LC, V_MC, V_HC,
    V_RS, V_CLV, V_CP, V_MA, V_CB, V_CY, V_OH, V_CH, NV
};

// instrument slots (5..8 are user waves; 9..24 ours)
enum {
    SL_BD = 9, SL_SDB, SL_SDN, SL_TOM, SL_TOMN, SL_CON, SL_RS, SL_CLV,
    SL_CP, SL_MAR, SL_CB, SL_CYT, SL_HATO, SL_HATC
};

static const char *VNAME[NV] = {
    "BASS", "SNRE", "LTOM", "MTOM", "HTOM", "LCGA", "MCGA",
    "HCGA", "RIM",  "CLAV", "CLAP", "MARA", "COWB", "CYMB",
    "OPHH", "CHHH"
};
static const char VKEY[NV] = {
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', 'A', 'S', 'D', 'F', 'G', 'H'
};

#define STEPS 16

#define GX   88    // grid left edge
#define GY   39    // grid top edge
#define SX   13    // column stride
#define SY   9     // row stride

// per-voice knobs: two 8×8 rotary knobs in the label column (x 60-87)
// K_TUNE: ±12 semitones (center = no change)   K_DCAY: gate duration scale
#define K0X  60    // tune knob x
#define K1X  70    // decay knob x
#define K2X  80    // character knob x (voice-specific: SNPY/THUD/TONE/RING)
#define KW    8    // knob size

// the iconic step-button colors: steps 1-4 red, 5-8 orange, 9-12 yellow, 13-16 white
static const int QCLR[4] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_WHITE };

typedef struct {
    const char *name;
    int         tempo;
    const char *row[NV];   // 16 chars, 'x' = hit; NULL = silent row
    const char *accent;    // 16 chars, 'x' = +2 volume on that step
    int         swing;     // 50 = straight .. 66 = triplet (0 = straight)
} Pat;

static const Pat PRESET[] = {
    { "PLANET ROCK", 126, {        // the electro blueprint
        [V_BD]  = "x.......x..x....",
        [V_SD]  = "....x.......x...",
        [V_CH]  = "x.x.x.x.x.x.x...",
        [V_OH]  = "..............x.",
        [V_CLV] = "..x..x....x.....",
        [V_CB]  = "..........x.....",
      }, "x.......x......." },
    { "HEALING", 94, {             // slow-jam 808: rim, clap, congas, open hat
        [V_BD]  = "x.....x.x.......",
        [V_RS]  = "....x.......x...",
        [V_CP]  = "....x.......x...",
        [V_CH]  = "x.x.x.x.x.x.x.x.",
        [V_OH]  = "..x.............",
        [V_HC]  = "..........x..x..",
      }, "x.......x......." },
    { "HOUSE", 122, {              // four on the floor, hats off the beat
        [V_BD]  = "x...x...x...x...",
        [V_CP]  = "....x.......x...",
        [V_CH]  = "x...x...x...x...",
        [V_OH]  = "..x...x...x...x.",
        [V_MA]  = "xxxxxxxxxxxxxxxx",
      }, "x.......x......." },
    { "ELECTRO", 110, {
        [V_BD]  = "x..x......x.x...",
        [V_SD]  = "....x.......x...",
        [V_CH]  = "x.x.x.x.x.x.x.x.",
        [V_CB]  = "..x.......x.....",
        [V_HT]  = ".............x..",
        [V_MT]  = "..............x.",
        [V_LT]  = "...............x",
      }, "x.......x......." },
    { "BOOM BAP", 90, {            // long kick boom, lazy swing
        [V_BD]  = "x......x..x.....",
        [V_SD]  = "....x.......x...",
        [V_CH]  = "x.x.x.x.x.x.x.x.",
        [V_OH]  = "..............x.",
      }, "....x.......x...", 56 },
    { "COWBELL JAM", 115, {        // needs more
        [V_CB]  = "x..x..x.x..x..x.",
        [V_BD]  = "x.......x.......",
        [V_SD]  = "....x.......x...",
        [V_CH]  = "..x...x...x...x.",
        [V_CLV] = "......x.......x.",
      }, "x.......x......." },
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int  pre      = 0;      // start on PLANET ROCK
static int  tempo    = 126;
static bool running  = true;
static int  last16   = -1;
static int  playhead = 0;
static int  flash[NV];
static bool grid[NV][STEPS];   // the live pattern — editable, loaded from preset
static int  nextz_x = -1;      // the preset '>' tap zone, recorded by draw() ('<' is fixed)
static bool gacc[STEPS];       // live accent row
static bool paint_val;         // what a click-drag writes (set on press)
static int  swing = 50;        // 50 = straight .. 66 = full triplet

static float ktune[NV];        // 0..1, center=0.5 → ±12 semitones
static float kdecay[NV];       // 0..1, center=0.5 → 1× scale (0→0.25×, 1→4×)
static float kcolor[NV];       // 0..1, center=0.5 → voice-specific character knob
static int   drag_v = -1, drag_k = -1;
static int   drag_y0, drag_x0;
static int   hover_v = -1, hover_k = -1;

// knob column labels — k=0,1 are the same for every voice; k=2 is per-voice
static const char *KLABEL[2] = { "TUNE", "DECAY" };
static const char *K2LABEL[NV] = {
    NULL,   "SNPY", "THUD", "THUD", "THUD",  // BD SD LT MT HT
    NULL,   NULL,   NULL,                     // LC MC HC — single layer
    NULL,   NULL,   NULL,   NULL,             // RS CLV CP MA
    "TONE", "TONE", "RING", NULL,             // CB CY OH CH
};

// 8×8 rotary knob: circle outline + single tick pixel showing value position.
// Sweep: 135° (lower-left) → 270° (top) → 405°=45° (lower-right) = 270° total.
static void draw_knob(int x, int y, float val, int col) {
    int cx = x + 3, cy = y + 3;
    circ(cx, cy, 3, CLR_DARKER_GREY);
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
    case 0: // PLANET ROCK — tight claves, medium kick
        kdecay[V_CLV] = 0.38f;
        kdecay[V_BD]  = 0.55f;
        break;
    case 1: // HEALING — slow jam; long open hat, softer kick
        kdecay[V_BD]  = 0.58f;
        kdecay[V_OH]  = 0.70f;
        kcolor[V_SD]  = 0.42f;  // more body, less snap
        break;
    case 2: // HOUSE — punchy kick, tight hats
        kdecay[V_BD]   = 0.38f;
        kdecay[V_CH]   = 0.36f;
        kcolor[V_SD]   = 0.60f;  // snappier clap
        break;
    case 3: // ELECTRO — wider tom tuning for the descending cascade
        ktune[V_LT]  = 0.35f;
        ktune[V_HT]  = 0.60f;
        kdecay[V_BD] = 0.60f;
        break;
    case 4: // BOOM BAP — long boomy kick, snappy snare, breathing hat
        kdecay[V_BD]  = 0.74f;
        kcolor[V_SD]  = 0.66f;
        kdecay[V_OH]  = 0.64f;
        break;
    case 5: // COWBELL JAM — bright cowbell, tight claves
        kcolor[V_CB]  = 0.70f;
        kdecay[V_CB]  = 0.55f;
        kdecay[V_CLV] = 0.36f;
        break;
    }
}

static int vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice `delay` ms from now. Layered hits follow the schematic:
// metal voices = members of the six-oscillator bank (MIDI 79=800Hz, 73=540,
// 72=522.7, 66=369.6, 63=304.4, 56=205.3), snare = 180+330Hz modes + noise.
static void fire(int v, int boost, int delay) {
    switch (v) {
    case V_BD:  // ~50Hz bridged-T with the decay-knob boom
        schedule_hit(delay, k_midi(v, 31), SL_BD, vv(6, boost), k_dur(v, 500));
        break;
    case V_SD: {  // 180Hz + 330Hz modes; SNPY knob fades body↔noise
        int body = k_cv(v, 8, 0), snpy = k_cv(v, 0, 8);
        schedule_hit(delay, k_midi(v, 54), SL_SDB, vv(body, boost), k_dur(v, 110));
        schedule_hit(delay, k_midi(v, 64), SL_SDB, vv(body, boost), k_dur(v, 110));
        schedule_hit(delay, k_midi(v, 60), SL_SDN, vv(snpy, boost), k_dur(v, 140));
        break;
    }
    case V_LT: case V_MT: case V_HT: {  // sine drop + THUD knob controls noise thud level
        int m = v == V_LT ? 40 : v == V_MT ? 45 : 52;
        schedule_hit(delay, k_midi(v, m),  SL_TOM,  vv(4, boost), k_dur(v, 280));
        schedule_hit(delay, k_midi(v, 60), SL_TOMN, vv(k_cv(v, 0, 5), boost), k_dur(v, 30));
        break;
    }
    case V_LC: case V_MC: case V_HC: {  // same circuit, cleaner + shorter
        int m = v == V_LC ? 52 : v == V_MC ? 57 : 63;
        schedule_hit(delay, k_midi(v, m), SL_CON, vv(4, boost), k_dur(v, 160));
        break;
    }
    case V_RS:  // dual bridged-T: 1667Hz (midi 92) + 455Hz (midi 70)
        schedule_hit(delay, k_midi(v, 92), SL_RS, vv(4, boost), k_dur(v, 50));
        schedule_hit(delay, k_midi(v, 70), SL_RS, vv(3, boost), k_dur(v, 50));
        break;
    case V_CLV: // the rimshot circuit retuned to 2500Hz = midi 99 exactly
        schedule_hit(delay, k_midi(v, 99), SL_CLV, vv(4, boost), k_dur(v, 45));
        break;
    case V_CP:  // three retriggers ~10ms apart + a soft room tail
        schedule_hit(delay,      k_midi(v, 60), SL_CP, vv(4, boost), 12);
        schedule_hit(delay + 10, k_midi(v, 60), SL_CP, vv(4, boost), 12);
        schedule_hit(delay + 20, k_midi(v, 60), SL_CP, vv(4, boost), 12);
        schedule_hit(delay + 28, k_midi(v, 60), SL_CP, vv(3, boost), k_dur(v, 140));
        break;
    case V_MA:  schedule_hit(delay, k_midi(v, 90), SL_MAR, vv(3, boost), k_dur(v, 30)); break;
    case V_CB:  // bank osc 1+2; TONE fades 540Hz↔800Hz emphasis
        schedule_hit(delay, k_midi(v, 73), SL_CB, vv(k_cv(v, 7, 0), boost), k_dur(v, 220));
        schedule_hit(delay, k_midi(v, 79), SL_CB, vv(k_cv(v, 0, 7), boost), k_dur(v, 220));
        break;
    case V_CY:  // three bank members; TONE fades warm↔bright
        schedule_hit(delay, k_midi(v, 79), SL_CYT, vv(k_cv(v, 0, 6), boost), k_dur(v, 900));
        schedule_hit(delay, k_midi(v, 72), SL_CYT, vv(2, boost), k_dur(v, 900));
        schedule_hit(delay, k_midi(v, 66), SL_CYT, vv(k_cv(v, 5, 0), boost), k_dur(v, 900));
        break;
    case V_OH:  // two bank members; RING fades warm↔bright
        schedule_hit(delay, k_midi(v, 79), SL_HATO, vv(k_cv(v, 0, 6), boost), k_dur(v, 360));
        schedule_hit(delay, k_midi(v, 72), SL_HATO, vv(k_cv(v, 5, 0), boost), k_dur(v, 360));
        break;
    case V_CH:  // same two, ~50ms
        schedule_hit(delay, k_midi(v, 79), SL_HATC, vv(3, boost), k_dur(v, 50));
        schedule_hit(delay, k_midi(v, 72), SL_HATC, vv(2, boost), k_dur(v, 50));
        break;
    }
}

void init(void) {
    // kick — the boom: low sine, lowpassed, +26st pitch drop over 50ms.
    // instrument_drive() runs the sub a little hot — the Miami-bass / trap
    // saturated 808 that's been half of hip-hop's low end since day one
    // (gentler than the 909's: the 808 booms, it doesn't punch).
    instrument(SL_BD, INSTR_SINE, 0, 480, 0, 60);
    instrument_filter(SL_BD, FILTER_LOW, 250, 3);
    instrument_env(SL_BD, 0, ENV_PITCH, 0, 50, 26.0f);
    instrument_drive(SL_BD, 0.28f);

    // snare body (fired twice: 180Hz + 330Hz modes)
    instrument(SL_SDB, INSTR_SINE, 0, 100, 0, 30);
    instrument_filter(SL_SDB, FILTER_LOW, 1200, 1);
    instrument_env(SL_SDB, 0, ENV_PITCH, 0, 20, 3.0f);
    // snare "snappy" — highpassed noise
    instrument(SL_SDN, INSTR_NOISE, 0, 130, 0, 40);
    instrument_filter(SL_SDN, FILTER_HIGH, 1800, 2);

    // toms — sine with a pitch drop + a separate low noise thud
    instrument(SL_TOM, INSTR_SINE, 0, 260, 0, 50);
    instrument_env(SL_TOM, 0, ENV_PITCH, 0, 80, 6.0f);
    instrument(SL_TOMN, INSTR_NOISE, 0, 28, 0, 12);
    instrument_filter(SL_TOMN, FILTER_BAND, 900, 2);

    // congas — the tom circuit without the noise, shorter
    instrument(SL_CON, INSTR_SINE, 0, 150, 0, 30);
    instrument_env(SL_CON, 0, ENV_PITCH, 0, 25, 4.0f);

    // rimshot — both bridged-T modes through a highpass (keeps 455 AND 1667)
    instrument(SL_RS, INSTR_TRI, 0, 45, 0, 15);
    instrument_filter(SL_RS, FILTER_HIGH, 500, 3);

    // claves — single 2500Hz ping
    instrument(SL_CLV, INSTR_TRI, 0, 40, 0, 14);
    instrument_filter(SL_CLV, FILTER_LOW, 4000, 5);

    // handclap — bandpassed noise; the retriggers in fire() make the clap
    instrument(SL_CP, INSTR_NOISE, 0, 110, 0, 50);
    instrument_filter(SL_CP, FILTER_BAND, 1100, 5);

    // maracas
    instrument(SL_MAR, INSTR_NOISE, 0, 24, 0, 10);
    instrument_filter(SL_MAR, FILTER_HIGH, 6500, 2);

    // cowbell — square pair through the classic ~2.6kHz bandpass
    instrument(SL_CB, INSTR_SQUARE, 0, 250, 0, 60);
    instrument_filter(SL_CB, FILTER_BAND, 2640, 5);

    // cymbal — bank squares through the 3440Hz region, very long ring
    instrument(SL_CYT, INSTR_SQUARE, 0, 850, 0, 200);
    instrument_filter(SL_CYT, FILTER_HIGH, 3440, 3);

    // hats — bank squares through ~7kHz highpass; open vs closed = decay
    instrument(SL_HATO, INSTR_SQUARE, 0, 340, 0, 90);
    instrument_filter(SL_HATO, FILTER_HIGH, 7000, 3);
    instrument(SL_HATC, INSTR_SQUARE, 0, 42, 0, 16);
    instrument_filter(SL_HATC, FILTER_HIGH, 7000, 3);
    instrument_choke(SL_HATC, SL_HATO);  // closed hat chokes the open hat

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

    if (keyp(KEY_SPACE) || tapp(244, 18, 64, 12)) { running = !running; last16 = -1; }   // tap PLAYING/STOPPED
    // < > switch presets — keys, or tap the label's arrows
    if (keyp(KEY_LEFT)  || tapp(8, 18, 20, 12))
        { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; }
    if (keyp(KEY_RIGHT) || (nextz_x >= 0 && tapp(nextz_x, 18, 20, 12)))
        { pre = (pre + 1) % NP;      load_preset(); last16 = -1; }
    if (keyp(KEY_UP)   || tapp(274, 9, 32, 12)) { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); }   // BPM halves
    if (keyp(KEY_DOWN) || tapp(244, 9, 30, 12)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); }
    if (keyp('Z') || tapp(200, 9, 18, 12)) { swing -= 2; if (swing < 50) swing = 50; }                      // SW halves
    if (keyp('X') || tapp(218, 9, 20, 12)) { swing += 2; if (swing > 66) swing = 66; }

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
        // swing: offbeat 8ths late (the 808 never had this knob either)
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

    // black face, thin silver trim — the 808 look
    cls(CLR_BROWNISH_BLACK);
    rectfill(6, 8, 308, 188, CLR_BLACK);
    rect(6, 8, 308, 188, CLR_DARK_GREY);
    line(6, 22, 313, 22, CLR_DARKER_GREY);

    print("RHYTHM COMPOSER TR-808", 14, 13, CLR_LIGHT_GREY);
    sprintf(buf, "SW%2d", swing);
    print(buf, 204, 13, swing > 50 ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 248, 13, CLR_LIGHT_PEACH);
    int nx = print("< ", 14, 22, CLR_ORANGE);      // the arrows are tap targets
    nx = print(p->name, nx, 22, CLR_ORANGE);       // (row nudged up 2px for finger room
    print(" >", nx, 22, CLR_ORANGE);               //  above the accent strip)
    nextz_x = nx + 2;
    print(running ? "PLAYING" : "STOPPED", 248, 22, running ? CLR_GREEN : CLR_RED);

    // playhead column
    if (running)
        rectfill(GX + playhead * SX - 1, GY - 7, SX - 1, NV * SY + 7, CLR_DARKER_GREY);

    // accent strip (clickable)
    for (int s = 0; s < STEPS; s++)
        rectfill(GX + s * SX, GY - 6, SX - 4, 3,
                 gacc[s] ? CLR_ORANGE : CLR_DARKER_GREY);

    for (int v = 0; v < NV; v++) {
        int y = GY + v * SY;
        sprintf(buf, "%c", VKEY[v]);
        print(buf, 14, y, flash[v] > 0 ? CLR_WHITE : CLR_YELLOW);
        print(VNAME[v], 26, y, flash[v] > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
        int base_col = flash[v] > 0 ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY;
        draw_knob(K0X, y, ktune[v],  (drag_v==v&&drag_k==0) ? CLR_WHITE : base_col);
        draw_knob(K1X, y, kdecay[v], (drag_v==v&&drag_k==1) ? CLR_WHITE : base_col);
        if (K2LABEL[v])
            draw_knob(K2X, y, kcolor[v], (drag_v==v&&drag_k==2) ? CLR_WHITE : base_col);
        for (int s = 0; s < STEPS; s++) {
            int x = GX + s * SX;
            int q = QCLR[s >> 2];   // step-button color by quarter
            if (grid[v][s])
                rectfill(x, y, SX - 4, 7,
                         (flash[v] > 0 && s == playhead && running) ? CLR_WHITE : q);
            else
                rect(x, y, SX - 4, 7, (s & 3) == 0 ? q : CLR_DARKER_GREY);
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
            print(buf2, K0X, 31, CLR_LIGHT_GREY);
            font(FONT_NORMAL);
        }
    }

    print("CLICK <> PRESET ^v BPM Z/X SWING DRAG KNOBS", 14, 186, CLR_MEDIUM_GREY);
}
