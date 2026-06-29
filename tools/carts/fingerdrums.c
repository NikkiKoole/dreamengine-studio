/* de:meta
{
  "title": "finger drums",
  "status": "active",
  "kind": [
    "instrument",
    "toy"
  ],
  "teaches": [
    "drum-synthesis"
  ],
  "lineage": "Performance counterpart to the step-sequencer drum carts (808/909/cr-78) — five thematically matched kits (acoustic, MPC, Pikuniku, Mac DeMarco, lo-fi tape) all driven by hit position as a velocity proxy.",
  "description": "Play a drum kit BY HAND — the performance counterpart to the cabinet's step-sequencer boxes (808/909/cr-78). FIVE kits, TAB cycles them, each a matched LOOK + SOUND: ACOUSTIC KIT (a real drumset in perspective — kick, snare, INSTR_MEMBRANE rack + floor toms, hats, crash, ride); MPC PADS (a 4x4 grid of an 808-style electronic kit — boom kick, noise snare, square cowbell, retriggered clap); PIKUNIKU (the drums are little bean-creatures with big eyes and stretchy legs that KICK when struck, on pastel hills — bouncy toy/chiptune voices + an INSTR_MALLET sparkle); MAC DEMARCO (the kit re-themed warm & woody, voiced soft and dead, run through tape wow/flutter + chorus + detune — the woozy bedroom-tape warble); and MAC - TAPE (a lo-fi drum-machine version, same woze + a touch of cassette bitcrush, faded vintage palette). Touch hardware gives position, never pressure, so VELOCITY comes from WHERE you hit: high = soft, low = hard (8 levels). Every finger strikes independently (multitouch). Keyboard fallback hits mid-velocity, SHIFT accents: kit = F/G hats, T/Y cymbals, J snare, I/K/M toms, SPACE kick; pads = 1-4 / QWER / ASDF / ZXCV. A USB-MIDI pad controller plays any kit with real hardware velocity (GM drum map). TAB cycle kit, H help."
}
de:meta */
#include "studio.h"
#include "pointer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── FINGER DRUMS ───────────────────────────────────────────────────────────
// A play-it-by-hand drum cart — the missing PERFORMANCE modality (every other
// drum cart in the cabinet is a step-sequencer; here you actually hit the kit).
// FIVE kits, TAB cycles them; each is a matched LOOK + SOUND:
//
//   ACOUSTIC KIT  — a real drumset in perspective. Toms are the modeled
//                   INSTR_MEMBRANE (a tom IS a tuned drumhead); kick is a
//                   pitch-swept sine, snare + cymbals are filtered noise.
//   MPC PADS      — a 4x4 grid of an 808-style electronic kit (tr808 recipes).
//   PIKUNIKU      — the drums are little bean-creatures (Sectordub's game): big
//                   eyes, stretchy legs that KICK when struck, pastel hills.
//                   Bouncy toy/chiptune voices + an INSTR_MALLET sparkle.
//   MAC DEMARCO   — the acoustic kit re-themed warm & woody, voiced soft and
//                   dead, run through TAPE wow/flutter + CHORUS + detune: the
//                   woozy bedroom-tape warble that IS his sound.
//   MAC — TAPE    — a lo-fi bedroom drum-machine version, same woze + a touch
//                   of cassette bitcrush, in a faded vintage palette.
//
// VELOCITY without pressure: touch gives position, never force, so DYNAMICS
// come from WHERE you hit — high = soft, low = hard (8 levels, 1..7). A USB-MIDI
// pad controller plays any kit with REAL velocity (midi_get + GM drum map).
// Keyboard fallback hits mid-velocity; hold SHIFT to accent. Multitouch: every
// finger strikes independently (pointer.h pool; the mouse folds in as a touch).

// ── shared voice-slot pool (5..16) — reconfigured per kit on switch ─────────
enum {
    SL_KICK = 5, SL_SNR_B, SL_SNR_N, SL_HHC, SL_HHO, SL_TOM,
    SL_CRASH, SL_RIDE, SL_CLAP, SL_COW, SL_RIM, SL_AUX,
};

// ── the role a zone plays (slot routing is fixed; setup_kit voices the slots) ─
enum { R_KICK, R_SNARE, R_HHC, R_HHO, R_TOM, R_CRASH, R_RIDE, R_CLAP, R_COW, R_RIM, R_SPARK };

static bool snare_layered = true;   // false = single-layer snare (rim/cross-stick)

// fire a role at velocity 1..7. schedule_hit gives each role a generous gate;
// the slot's own envelope (set in setup_kit) shapes the real tail.
static void fire(int role, int midi, int vol) {
    if (vol < 1) vol = 1; if (vol > 7) vol = 7;
    switch (role) {
    case R_KICK:  schedule_hit(0, midi ? midi : 33, SL_KICK, vol, 420); break;
    case R_SNARE: schedule_hit(0, 57, SL_SNR_B, vol, 150);
                  if (snare_layered) schedule_hit(0, 60, SL_SNR_N, vol, 150); break;
    case R_HHC:   schedule_hit(0, 70, SL_HHC, vol, 70); break;
    case R_HHO:   schedule_hit(0, 70, SL_HHO, vol, 320); break;
    case R_TOM:   schedule_hit(0, midi ? midi : 55, SL_TOM, vol, 340); break;
    case R_CRASH: schedule_hit(0, 70, SL_CRASH, vol, 800); break;
    case R_RIDE:  schedule_hit(0, midi ? midi : 78, SL_RIDE, vol, 360); break;
    case R_CLAP:  schedule_hit(0,  60, SL_CLAP, vol, 12);          // 808 handclap:
                  schedule_hit(10, 60, SL_CLAP, vol, 12);          // 3 fast retriggers
                  schedule_hit(20, 60, SL_CLAP, vol, 12);          // + a body tail
                  schedule_hit(28, 60, SL_CLAP, vol > 1 ? vol - 1 : 1, 150); break;
    case R_COW:   schedule_hit(0, 73, SL_COW, vol, 220);           // square pair
                  schedule_hit(0, 79, SL_COW, vol, 220); break;
    case R_RIM:   schedule_hit(0, midi ? midi : 92, SL_RIM, vol, 70); break;
    case R_SPARK: schedule_hit(0, midi ? midi : 84, SL_AUX, vol, 600); break;
    }
}

// ── playable zones ──────────────────────────────────────────────────────────
enum { Z_DRUM, Z_CYM, Z_PAD, Z_BEAN };
typedef struct {
    int cx, cy, rx, ry;     // center + half-extents
    int shape;
    int role;               // R_*
    int midi;               // pitch for toms/cymbals/sparkle (0 = fire() default)
    const char *label;
    int col;                // accent (creature color / pad group color)
    int key;                // keyboard key (0 = none)
    float flash, vlast, squash;   // runtime
} Zone;

// — ACOUSTIC + MAC DEMARCO share this perspective layout (back -> front) —
static Zone zk[] = {
    {  92,  60, 34, 11, Z_CYM,  R_CRASH, 0,  "CRASH", CLR_YELLOW,     'T' },
    { 242,  70, 40, 13, Z_CYM,  R_RIDE, 78,  "RIDE",  CLR_ORANGE,     'Y' },
    {  40, 104, 22,  6, Z_CYM,  R_HHO,   0,  "OPEN",  CLR_YELLOW,     'G' },
    {  40, 120, 22,  7, Z_CYM,  R_HHC,   0,  "HAT",   CLR_YELLOW,     'F' },
    { 132, 116, 21, 17, Z_DRUM, R_TOM,  62,  "TOM",   CLR_RED,        'I' },
    { 186, 112, 23, 19, Z_DRUM, R_TOM,  56,  "TOM",   CLR_RED,        'K' },
    { 256, 166, 29, 26, Z_DRUM, R_TOM,  45,  "FLOOR", CLR_RED,        'M' },
    {  96, 164, 25, 20, Z_DRUM, R_SNARE, 0,  "SNARE", CLR_LIGHT_GREY, 'J' },
    { 162, 190, 50, 44, Z_DRUM, R_KICK,  0,  "KICK",  CLR_RED,  KEY_SPACE },
};
#define NZK ((int)(sizeof(zk) / sizeof(zk[0])))

// — MPC + MAC-TAPE share this 4x4 pad grid (row0 top -> row3 bottom) —
static Zone zp[] = {
    {  61,  55, 29, 21, Z_PAD, R_CRASH, 79, "CRASH",  CLR_INDIGO,     '1' },
    { 127,  55, 29, 21, Z_PAD, R_RIDE,  86, "RIDE",   CLR_INDIGO,     '2' },
    { 193,  55, 29, 21, Z_PAD, R_COW,    0, "COW",    CLR_LIME_GREEN, '3' },
    { 259,  55, 29, 21, Z_PAD, R_RIM,   92, "RIM",    CLR_PINK,       '4' },
    {  61, 105, 29, 21, Z_PAD, R_HHO,    0, "O-HAT",  CLR_YELLOW,     'Q' },
    { 127, 105, 29, 21, Z_PAD, R_TOM,   60, "TOM HI", CLR_BLUE,       'W' },
    { 193, 105, 29, 21, Z_PAD, R_TOM,   53, "TOM MID",CLR_BLUE,       'E' },
    { 259, 105, 29, 21, Z_PAD, R_TOM,   45, "TOM LO", CLR_TRUE_BLUE,  'R' },
    {  61, 155, 29, 21, Z_PAD, R_HHC,    0, "HAT",    CLR_YELLOW,     'A' },
    { 127, 155, 29, 21, Z_PAD, R_SNARE,  0, "SNARE",  CLR_ORANGE,     'S' },
    { 193, 155, 29, 21, Z_PAD, R_CLAP,   0, "CLAP",   CLR_DARK_ORANGE,'D' },
    { 259, 155, 29, 21, Z_PAD, R_RIM,   84, "PERC",   CLR_PINK,       'F' },
    {  61, 205, 29, 21, Z_PAD, R_KICK,  31, "KICK",   CLR_RED,        'Z' },
    { 127, 205, 29, 21, Z_PAD, R_KICK,  31, "KICK",   CLR_RED,        'X' },
    { 193, 205, 29, 21, Z_PAD, R_SNARE,  0, "SNARE",  CLR_ORANGE,     'C' },
    { 259, 205, 29, 21, Z_PAD, R_CLAP,   0, "CLAP",   CLR_DARK_ORANGE,'V' },
};
#define NZP ((int)(sizeof(zp) / sizeof(zp[0])))

// — PIKUNIKU: bean-creatures scattered on the hill —
static Zone zb[] = {
    { 120,  98, 12, 17, Z_BEAN, R_SPARK, 84, "ding",  CLR_ORANGE,     'D' },
    {  52, 120, 10, 14, Z_BEAN, R_HHC,    0, "tik",   CLR_YELLOW,     'F' },
    { 218, 128, 15, 20, Z_BEAN, R_TOM,   72, "boop",  CLR_GREEN,      'K' },
    { 268, 150, 17, 23, Z_BEAN, R_TOM,   64, "boop",  CLR_PINK,       'L' },
    {  96, 150, 16, 22, Z_BEAN, R_SNARE,  0, "bop",   CLR_BLUE,       'J' },
    { 172, 120, 13, 18, Z_BEAN, R_CLAP,   0, "pop",   CLR_INDIGO,     'G' },
    { 160, 178, 26, 34, Z_BEAN, R_KICK,  36, "PIKU",  CLR_RED,  KEY_SPACE },
};
#define NZB ((int)(sizeof(zb) / sizeof(zb[0])))

// ── kits ──────────────────────────────────────────────────────────────────
enum { KIT_ACOUSTIC, KIT_MPC, KIT_PIKU, KIT_MAC_AC, KIT_MAC_DIG, NKIT };
static const char *KIT_NAME[NKIT] = { "ACOUSTIC KIT", "MPC PADS", "PIKUNIKU", "MAC DEMARCO", "MAC - TAPE" };
static Zone *KZ[NKIT]; static int KN[NKIT];

static int  kit = KIT_ACOUSTIC;
static int  accent = CLR_RED;       // ripple/strike color for this kit
static Zone *zones(void)  { return KZ[kit]; }
static int   nzones(void) { return KN[kit]; }

// voice each kit's slots + master FX. Called in init() and on every TAB switch.
static void setup_kit(int k) {
    accent = CLR_LIGHT_GREY; snare_layered = true;
    instrument_tune(SL_TOM, 0);                              // default: in tune
    switch (k) {
    case KIT_ACOUSTIC:
        accent = CLR_RED;
        instrument(SL_KICK, INSTR_SINE, 0, 300, 0, 80);  instrument_filter(SL_KICK, FILTER_LOW, 220, 2);
        instrument_env(SL_KICK, 0, ENV_PITCH, 0, 55, 28.0f); instrument_drive(SL_KICK, 0.15f);
        instrument(SL_SNR_B, INSTR_SINE, 0, 90, 0, 40);  instrument_filter(SL_SNR_B, FILTER_LOW, 1500, 1);
        instrument_env(SL_SNR_B, 0, ENV_PITCH, 0, 18, 4.0f);
        instrument(SL_SNR_N, INSTR_NOISE, 0, 120, 0, 60); instrument_filter(SL_SNR_N, FILTER_BAND, 1900, 3);
        instrument(SL_TOM, INSTR_MEMBRANE, 1, 0, 7, 300);
        instrument_harmonics(SL_TOM, 0.28f); instrument_timbre(SL_TOM, 0.18f); instrument_morph(SL_TOM, 0.12f);
        instrument(SL_HHC, INSTR_NOISE, 0, 45, 0, 25); instrument_filter(SL_HHC, FILTER_HIGH, 8500, 2);
        instrument(SL_HHO, INSTR_NOISE, 0, 60, 2, 220); instrument_filter(SL_HHO, FILTER_HIGH, 8200, 2);
        instrument_choke(SL_HHC, SL_HHO);
        instrument(SL_CRASH, INSTR_NOISE, 0, 220, 0, 600); instrument_filter(SL_CRASH, FILTER_HIGH, 5000, 1);
        instrument(SL_RIDE, INSTR_NOISE, 0, 90, 0, 300); instrument_filter(SL_RIDE, FILTER_BAND, 7000, 3);
        tape(0, 0, 0); chorus(0, 0, 0); crush(0, 0, 0);
        break;
    case KIT_MPC:
        accent = CLR_LIME_GREEN;
        instrument(SL_KICK, INSTR_SINE, 0, 480, 0, 60); instrument_filter(SL_KICK, FILTER_LOW, 250, 3);
        instrument_env(SL_KICK, 0, ENV_PITCH, 0, 50, 26.0f); instrument_drive(SL_KICK, 0.28f);
        instrument(SL_SNR_B, INSTR_SINE, 0, 100, 0, 30); instrument_filter(SL_SNR_B, FILTER_LOW, 1200, 1);
        instrument_env(SL_SNR_B, 0, ENV_PITCH, 0, 20, 3.0f);
        instrument(SL_SNR_N, INSTR_NOISE, 0, 130, 0, 40); instrument_filter(SL_SNR_N, FILTER_HIGH, 1800, 2);
        instrument(SL_HHC, INSTR_NOISE, 0, 45, 0, 25); instrument_filter(SL_HHC, FILTER_HIGH, 9000, 2);
        instrument(SL_HHO, INSTR_NOISE, 0, 60, 2, 300); instrument_filter(SL_HHO, FILTER_HIGH, 9000, 2);
        instrument_choke(SL_HHC, SL_HHO);
        instrument(SL_TOM, INSTR_SINE, 0, 260, 0, 50); instrument_env(SL_TOM, 0, ENV_PITCH, 0, 80, 6.0f);
        instrument(SL_CLAP, INSTR_NOISE, 0, 110, 0, 50); instrument_filter(SL_CLAP, FILTER_BAND, 1100, 5);
        instrument(SL_COW, INSTR_SQUARE, 0, 200, 0, 30); instrument_filter(SL_COW, FILTER_BAND, 2640, 5);
        instrument(SL_CRASH, INSTR_NOISE, 0, 300, 0, 700); instrument_filter(SL_CRASH, FILTER_HIGH, 8000, 1);
        instrument(SL_RIDE, INSTR_NOISE, 0, 200, 0, 500); instrument_filter(SL_RIDE, FILTER_HIGH, 9000, 2);
        instrument(SL_RIM, INSTR_NOISE, 0, 40, 0, 30); instrument_filter(SL_RIM, FILTER_HIGH, 500, 3);
        tape(0, 0, 0); chorus(0, 0, 0); crush(0, 0, 0);
        break;
    case KIT_PIKU:
        accent = CLR_RED;
        // soft round boop kick
        instrument(SL_KICK, INSTR_SINE, 0, 190, 0, 70); instrument_filter(SL_KICK, FILTER_LOW, 600, 1);
        instrument_env(SL_KICK, 0, ENV_PITCH, 0, 70, 10.0f);
        // toy "bop" snare — a pitched triangle blip, single-layer
        snare_layered = false;
        instrument(SL_SNR_B, INSTR_TRI, 0, 80, 0, 50); instrument_filter(SL_SNR_B, FILTER_LOW, 2200, 2);
        instrument_env(SL_SNR_B, 0, ENV_PITCH, 0, 30, 8.0f);
        // tiny soft tick
        instrument(SL_HHC, INSTR_NOISE, 0, 28, 0, 18); instrument_filter(SL_HHC, FILTER_HIGH, 7000, 1);
        // melodic bouncy boops (toms)
        instrument(SL_TOM, INSTR_TRI, 0, 170, 0, 90); instrument_filter(SL_TOM, FILTER_LOW, 2600, 1);
        instrument_env(SL_TOM, 0, ENV_PITCH, 0, 55, 4.0f);
        // soft "pop" clap
        instrument(SL_CLAP, INSTR_NOISE, 0, 80, 0, 50); instrument_filter(SL_CLAP, FILTER_BAND, 1500, 4);
        // glockenspiel sparkle
        instrument(SL_AUX, INSTR_MALLET, 1, 0, 7, 400);
        instrument_harmonics(SL_AUX, 0.92f); instrument_timbre(SL_AUX, 0.6f); instrument_morph(SL_AUX, 0.42f);
        tape(0, 0, 0); chorus(1.3f, 0.25f, 0.22f); crush(0, 0, 0);   // a little playful width
        break;
    case KIT_MAC_AC:
        accent = CLR_DARK_ORANGE;
        // dead warm towel-kick
        instrument(SL_KICK, INSTR_SINE, 0, 220, 0, 70); instrument_filter(SL_KICK, FILTER_LOW, 150, 2);
        instrument_env(SL_KICK, 0, ENV_PITCH, 0, 55, 20.0f); instrument_drive(SL_KICK, 0.10f);
        // soft mellow snare
        instrument(SL_SNR_B, INSTR_SINE, 0, 110, 0, 50); instrument_filter(SL_SNR_B, FILTER_LOW, 1100, 1);
        instrument_env(SL_SNR_B, 0, ENV_PITCH, 0, 20, 3.0f);
        instrument(SL_SNR_N, INSTR_NOISE, 0, 90, 0, 60); instrument_filter(SL_SNR_N, FILTER_BAND, 1400, 2);
        // tuned warm toms (a hair detuned for charm)
        instrument(SL_TOM, INSTR_MEMBRANE, 1, 0, 7, 320);
        instrument_harmonics(SL_TOM, 0.35f); instrument_timbre(SL_TOM, 0.20f); instrument_morph(SL_TOM, 0.18f);
        instrument_tune(SL_TOM, 0.06f);
        // dark hats / cymbals
        instrument(SL_HHC, INSTR_NOISE, 0, 50, 0, 30); instrument_filter(SL_HHC, FILTER_HIGH, 6500, 1);
        instrument(SL_HHO, INSTR_NOISE, 0, 70, 2, 240); instrument_filter(SL_HHO, FILTER_HIGH, 6500, 1);
        instrument_choke(SL_HHC, SL_HHO);
        instrument(SL_CRASH, INSTR_NOISE, 0, 260, 0, 650); instrument_filter(SL_CRASH, FILTER_HIGH, 4200, 1);
        instrument(SL_RIDE, INSTR_NOISE, 0, 90, 0, 320); instrument_filter(SL_RIDE, FILTER_BAND, 6000, 2);
        tape(0.45f, 0.30f, 0.45f); chorus(0.8f, 0.35f, 0.30f); crush(0, 0, 0);   // the woze
        break;
    case KIT_MAC_DIG:
        accent = CLR_LIGHT_YELLOW;
        // warm pitched-down synth kick
        instrument(SL_KICK, INSTR_SINE, 0, 260, 0, 70); instrument_filter(SL_KICK, FILTER_LOW, 200, 2);
        instrument_env(SL_KICK, 0, ENV_PITCH, 0, 55, 18.0f); instrument_drive(SL_KICK, 0.12f);
        instrument(SL_SNR_B, INSTR_SINE, 0, 90, 0, 40); instrument_filter(SL_SNR_B, FILTER_LOW, 1000, 1);
        instrument_env(SL_SNR_B, 0, ENV_PITCH, 0, 20, 3.0f);
        instrument(SL_SNR_N, INSTR_NOISE, 0, 90, 0, 50); instrument_filter(SL_SNR_N, FILTER_BAND, 1500, 2);
        instrument(SL_HHC, INSTR_NOISE, 0, 40, 0, 25); instrument_filter(SL_HHC, FILTER_HIGH, 7000, 1);
        instrument(SL_HHO, INSTR_NOISE, 0, 55, 2, 260); instrument_filter(SL_HHO, FILTER_HIGH, 7000, 1);
        instrument_choke(SL_HHC, SL_HHO);
        instrument(SL_TOM, INSTR_SINE, 0, 240, 0, 60); instrument_env(SL_TOM, 0, ENV_PITCH, 0, 80, 6.0f);
        instrument(SL_CLAP, INSTR_NOISE, 0, 100, 0, 50); instrument_filter(SL_CLAP, FILTER_BAND, 1100, 4);
        instrument(SL_COW, INSTR_SQUARE, 0, 180, 0, 30); instrument_filter(SL_COW, FILTER_BAND, 2400, 4);
        instrument(SL_CRASH, INSTR_NOISE, 0, 280, 0, 650); instrument_filter(SL_CRASH, FILTER_HIGH, 6500, 1);
        instrument(SL_RIDE, INSTR_NOISE, 0, 180, 0, 460); instrument_filter(SL_RIDE, FILTER_HIGH, 7000, 2);
        instrument(SL_RIM, INSTR_NOISE, 0, 40, 0, 30); instrument_filter(SL_RIM, FILTER_HIGH, 500, 3);
        tape(0.50f, 0.35f, 0.50f); chorus(0.9f, 0.40f, 0.35f); crush(7.5f, 0.6f, 0.35f);   // woze + cassette grit
        break;
    }
}

// ── strike ripples ──────────────────────────────────────────────────────────
#define NRIP 28
typedef struct { float x, y, age, life, r0, grow; int col; bool on; } Rip;
static Rip rip[NRIP];
static void spawn_rip(int x, int y, float r0, float grow, float life, int col) {
    for (int i = 0; i < NRIP; i++) if (!rip[i].on) {
        rip[i] = (Rip){ (float)x, (float)y, 0, life, r0, grow, col, true }; return;
    }
}

// ── input plumbing ───────────────────────────────────────────────────────────
typedef struct { int id; } Ptr;        // first field MUST be id (pointer.h contract)
static Ptr ptrs[PTR_MAX];
#define FRAME (1.0f / 60.0f)
#define KEY_LSHIFT 340                 // raylib KEY_LEFT_SHIFT
static bool show_help = false;
static int  last_vol = 0;

static int vol_from_y(const Zone *z, int hy) {     // top = soft, bottom = hard
    float t = (float)(hy - (z->cy - z->ry)) / (float)(2 * z->ry);
    if (t < 0) t = 0; if (t > 1) t = 1;
    int v = 1 + (int)(t * 6.0f + 0.5f);
    return v < 1 ? 1 : v > 7 ? 7 : v;
}
static bool zone_contains(const Zone *z, int x, int y) {
    if (z->shape == Z_PAD)
        return x >= z->cx - z->rx && x <= z->cx + z->rx && y >= z->cy - z->ry && y <= z->cy + z->ry;
    int pad = (z->shape == Z_CYM) ? 6 : (z->shape == Z_BEAN) ? 4 : 0;   // forgiving on thin/small targets
    float dx = (float)(x - z->cx) / (float)z->rx, dy = (float)(y - z->cy) / (float)(z->ry + pad);
    return dx * dx + dy * dy <= 1.0f;
}
static int zone_at(int x, int y) {
    Zone *z = zones();
    for (int i = nzones() - 1; i >= 0; i--) if (zone_contains(&z[i], x, y)) return i;
    return -1;
}
static void strike(int zi, int vol) {
    Zone *z = &zones()[zi];
    z->flash = 1.0f; z->vlast = vol / 7.0f;
    z->squash = (z->shape == Z_CYM) ? 0.0f : 0.45f + 0.55f * z->vlast;
    float r0 = (z->shape == Z_PAD) ? 4 : 6;
    spawn_rip(z->cx, z->cy, r0, (z->rx + z->ry) * (0.4f + 0.5f * z->vlast), 0.45f, accent);
    if (z->role == R_KICK) shake(1.0f + 2.0f * z->vlast);
    last_vol = vol;
    fire(z->role, z->midi, vol);
}
// MIDI: a GM drum note -> a role in the current kit
static void midi_play(int note, int vol) {
    int role = -1, midi = 0;
    switch (note) {
    case 35: case 36: role = R_KICK; break;
    case 37: role = R_RIM; break;
    case 38: case 40: role = R_SNARE; break;
    case 39: role = R_CLAP; break;
    case 42: case 44: role = R_HHC; break;
    case 46: role = R_HHO; break;
    case 41: case 43: role = R_TOM; midi = 45; break;
    case 45: case 47: role = R_TOM; midi = 53; break;
    case 48: case 50: role = R_TOM; midi = 60; break;
    case 49: case 57: role = R_CRASH; break;
    case 51: case 59: role = R_RIDE; break;
    case 56: role = R_COW; break;
    default: return;
    }
    last_vol = vol;
    Zone *z = zones();
    for (int i = 0; i < nzones(); i++) if (z[i].role == role) {
        z[i].flash = 1.0f; z[i].vlast = vol / 7.0f;
        z[i].squash = (z[i].shape == Z_CYM) ? 0.0f : 0.4f + 0.5f * z[i].vlast;
        spawn_rip(z[i].cx, z[i].cy, 5, (z[i].rx + z[i].ry) * 0.5f, 0.45f, accent);
        fire(role, z[i].midi ? z[i].midi : midi, vol); return;
    }
    fire(role, midi, vol);   // role not on this kit's surface — still play it
}

void init(void) {
    PTR_CLEAR(ptrs);
    KZ[KIT_ACOUSTIC] = zk; KN[KIT_ACOUSTIC] = NZK;
    KZ[KIT_MAC_AC]   = zk; KN[KIT_MAC_AC]   = NZK;
    KZ[KIT_MPC]      = zp; KN[KIT_MPC]      = NZP;
    KZ[KIT_MAC_DIG]  = zp; KN[KIT_MAC_DIG]  = NZP;
    KZ[KIT_PIKU]     = zb; KN[KIT_PIKU]     = NZB;
    setup_kit(kit);
}

void update(void) {
    if (keyp(KEY_TAB)) { kit = (kit + 1) % NKIT; setup_kit(kit); }
    if (keyp('H')) show_help = !show_help;

    for (int i = 0; i < touch_count(); i++) {        // touch + mouse-as-touch
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh = false;
        Ptr *p = PTR_ACQUIRE(ptrs, id, &fresh);
        if (!p) continue;
        if (fresh) { int zi = zone_at(tx, ty); if (zi >= 0) strike(zi, vol_from_y(&zones()[zi], ty)); }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptrs, touch_ended_id(i)); if (p) p->id = PTR_NONE;
    }

    int kvol = key(KEY_LSHIFT) ? 7 : 5;              // keyboard fallback (SHIFT accents)
    Zone *z = zones();
    for (int i = 0; i < nzones(); i++) if (z[i].key && keyp(z[i].key)) strike(i, kvol);

    int note, vel, r;                                // MIDI: real velocity
    while ((r = midi_get(&note, &vel)) != 0) if (r > 0) midi_play(note, 1 + (vel * 6) / 127);

    for (int i = 0; i < NZK; i++) { zk[i].flash -= FRAME * 6; if (zk[i].flash < 0) zk[i].flash = 0;
                                    zk[i].squash -= FRAME * 5; if (zk[i].squash < 0) zk[i].squash = 0; }
    for (int i = 0; i < NZP; i++) { zp[i].flash -= FRAME * 6; if (zp[i].flash < 0) zp[i].flash = 0;
                                    zp[i].squash -= FRAME * 5; if (zp[i].squash < 0) zp[i].squash = 0; }
    for (int i = 0; i < NZB; i++) { zb[i].flash -= FRAME * 5; if (zb[i].flash < 0) zb[i].flash = 0;
                                    zb[i].squash -= FRAME * 4; if (zb[i].squash < 0) zb[i].squash = 0; }
    for (int i = 0; i < NRIP; i++) if (rip[i].on) { rip[i].age += FRAME; if (rip[i].age >= rip[i].life) rip[i].on = false; }
}

static void center(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }
static void draw_ripples(void) {
    for (int i = 0; i < NRIP; i++) if (rip[i].on) {
        float k = rip[i].age / rip[i].life;
        float R = rip[i].r0 + k * rip[i].grow;
        oval((int)rip[i].x, (int)rip[i].y, (int)R, (int)(R * 0.45f), k < 0.5f ? CLR_WHITE : rip[i].col);
    }
}

// ── perspective kit (ACOUSTIC + MAC DEMARCO, themed) ────────────────────────
typedef struct { int bg_top, bg_bot, floor, shell, shell_d, skin, skin_kick, rim, cym, cym2, stand, label; } PTheme;
static const PTheme TH_AC  = { CLR_DARKER_BLUE, CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_DARK_RED,
    CLR_BROWNISH_BLACK, CLR_LIGHT_GREY, CLR_LIGHT_PEACH, CLR_MEDIUM_GREY, CLR_ORANGE, CLR_YELLOW, CLR_DARK_GREY, CLR_DARK_BLUE };
static const PTheme TH_MAC = { CLR_DARK_BROWN, CLR_BROWNISH_BLACK, CLR_DARKER_GREY, CLR_BROWN,
    CLR_BROWNISH_BLACK, CLR_MEDIUM_GREY, CLR_LIGHT_YELLOW, CLR_MEDIUM_GREY, CLR_DARK_ORANGE, CLR_BROWN, CLR_DARKER_GREY, CLR_DARK_BROWN };

static void draw_cymbal(const Zone *z, const PTheme *t) {
    thickline(z->cx, z->cy, z->cx, 224, 2, t->stand);
    ovalfill(z->cx, z->cy, z->rx, z->ry, t->cym);
    ovalfill(z->cx, z->cy, z->rx, z->ry > 2 ? z->ry - 2 : 1, t->cym2);
    oval(z->cx, z->cy, z->rx, z->ry, t->shell_d);
    pset(z->cx, z->cy, t->stand);
    if (z->flash > 0.05f) { int g = (int)(z->rx * (0.4f + 0.7f * z->flash));
        oval(z->cx, z->cy, g, (int)(g * 0.45f), CLR_WHITE); ovalfill(z->cx, z->cy, z->rx, z->ry, CLR_WHITE); }
}
static void draw_drum(const Zone *z, const PTheme *t) {
    int rx = z->rx, ry = z->ry - (int)(z->ry * 0.18f * z->squash), side = z->ry;
    int shell = (z->role == R_SNARE) ? CLR_MEDIUM_GREY : t->shell;
    ovalfill(z->cx, z->cy + side, rx, ry, t->shell_d);
    rectfill(z->cx - rx, z->cy, 2 * rx, side, shell);
    for (int a = -1; a <= 1; a++) pset(z->cx + a * (rx - 3), z->cy + side / 2, CLR_LIGHT_GREY);
    int skin = z->flash > 0.05f ? CLR_WHITE : (z->role == R_KICK ? t->skin_kick : t->skin);
    ovalfill(z->cx, z->cy, rx, ry, skin);
    oval(z->cx, z->cy, rx, ry, CLR_WHITE); oval(z->cx, z->cy, rx - 2, ry - 2, t->rim);
    if (z->flash > 0.05f) { int g = (int)(rx * z->flash); ovalfill(z->cx, z->cy, g, (int)(g * (float)ry / rx), CLR_WHITE); }
}
static void draw_persp(const PTheme *t) {
    vgradient(0, 16, SCREEN_W, SCREEN_H - 16, t->bg_top, t->bg_bot);
    ovalfill(SCREEN_W / 2, 248, 200, 34, t->floor);
    Zone *z = zk;
    for (int i = 0; i < NZK; i++) (z[i].shape == Z_CYM) ? draw_cymbal(&z[i], t) : draw_drum(&z[i], t);
    draw_ripples();
    font(FONT_SMALL);
    for (int i = 0; i < NZK; i++) center(z[i].label, z[i].cx, z[i].cy - (z[i].shape == Z_CYM ? 3 : 4), t->label);
    font(FONT_NORMAL);
}

// ── pad grid (MPC + MAC-TAPE) ───────────────────────────────────────────────
static void draw_padgrid(bool vintage) {
    cls(CLR_BROWNISH_BLACK);
    if (vintage) {   // cassette-reel motif up top
        for (int s = -1; s <= 1; s += 2) { int rx = SCREEN_W / 2 + s * 70;
            circ(rx, 26, 7, CLR_DARK_BROWN); circfill(rx, 26, 2, CLR_MEDIUM_GREY);
            thickline(SCREEN_W / 2 - 70, 26, SCREEN_W / 2 + 70, 26, 1, CLR_DARK_BROWN); }
    }
    Zone *z = zp;
    for (int i = 0; i < NZP; i++) {
        int x = z[i].cx - z[i].rx, y = z[i].cy - z[i].ry, w = 2 * z[i].rx, h = 2 * z[i].ry;
        int base = vintage ? CLR_DARK_BROWN : CLR_DARKER_GREY;
        int idle = vintage ? CLR_DARKER_GREY : z[i].col;
        int litc = z[i].flash > 0.6f ? CLR_WHITE : (vintage ? CLR_DARK_ORANGE : z[i].col);
        rrectfill(x, y, w, h, 5, base);
        if (z[i].flash > 0.05f) rrectfill(x + 1, y + 1, w - 2, h - 2, 4, litc);
        else rrect(x, y, w, h, 5, idle);
        int bars = (int)(z[i].vlast * 6 + 0.5f);
        for (int b = 0; b < bars; b++) rectfill(x + w - 4, y + h - 4 - b * 3, 2, 2, vintage ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
    }
    draw_ripples();
    font(FONT_SMALL);
    for (int i = 0; i < NZP; i++) {
        int lit = z[i].flash > 0.6f;
        center(z[i].label, z[i].cx, z[i].cy - 3, lit ? CLR_BLACK : (vintage ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY));
        char k[2] = { (char)(z[i].key), 0 };
        if (z[i].key >= 32 && z[i].key < 127) center(k, z[i].cx, z[i].cy + 5, lit ? CLR_BLACK : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
}

// ── Pikuniku: bean-creatures on the hill ────────────────────────────────────
static void draw_bean(const Zone *z) {
    float sq = z->squash;
    int bx = z->cx, by = z->cy - (int)(sq * 4);                 // little hop on hit
    int brx = z->rx + (int)(z->rx * 0.12f * sq), bry = z->ry - (int)(z->ry * 0.14f * sq);
    int legY = by + bry - 2, footY = by + bry + 13, kick = (int)(sq * 9);
    // legs (kick out when struck)
    thickline(bx - brx / 2, legY, bx - brx / 2 - 2 - kick, footY, 2, CLR_BROWNISH_BLACK);
    thickline(bx + brx / 2, legY, bx + brx / 2 + 2 + kick, footY, 2, CLR_BROWNISH_BLACK);
    ovalfill(bx - brx / 2 - 2 - kick, footY, 4, 2, CLR_BROWNISH_BLACK);
    ovalfill(bx + brx / 2 + 2 + kick, footY, 4, 2, CLR_BROWNISH_BLACK);
    // body
    ovalfill(bx, by, brx, bry, z->flash > 0.35f ? CLR_WHITE : z->col);
    oval(bx, by, brx, bry, CLR_BROWNISH_BLACK);
    oval(bx, by, brx - 1, bry - 1, CLR_BROWNISH_BLACK);        // bold outline
    // eyes
    int ey = by - bry / 3, ex = brx / 3 + 1, er = z->rx > 13 ? 4 : 3;
    circfill(bx - ex, ey, er, CLR_WHITE); circfill(bx + ex, ey, er, CLR_WHITE);
    oval(bx - ex, ey, er, er, CLR_BROWNISH_BLACK); oval(bx + ex, ey, er, er, CLR_BROWNISH_BLACK);
    int pdy = sq > 0.2f ? 1 : 0;                                // pupils dip when struck
    circfill(bx - ex, ey + pdy, er - 2, CLR_BLACK); circfill(bx + ex, ey + pdy, er - 2, CLR_BLACK);
}
static void draw_piku(void) {
    vgradient(0, 16, SCREEN_W, 150, CLR_BLUE, CLR_LIGHT_PEACH);   // pastel sky
    circfill(286, 40, 13, CLR_LIGHT_YELLOW);                      // sun
    ovalfill(70, 250, 150, 70, CLR_MEDIUM_GREEN);                // back hill
    ovalfill(250, 256, 150, 64, CLR_MEDIUM_GREEN);
    ovalfill(160, 268, 220, 80, CLR_LIME_GREEN);                 // front hill
    Zone *z = zb;
    for (int i = 0; i < NZB; i++) draw_bean(&z[i]);
    draw_ripples();
    font(FONT_SMALL);
    for (int i = 0; i < NZB; i++) center(z[i].label, z[i].cx, z[i].cy + z[i].ry + 16, CLR_DARK_GREEN);
    font(FONT_NORMAL);
}

static void draw_help(void) {
    rrectfill(24, 36, SCREEN_W - 48, SCREEN_H - 72, 6, CLR_DARKER_BLUE);
    rrect(24, 36, SCREEN_W - 48, SCREEN_H - 72, 6, CLR_BLUE);
    font(FONT_SMALL); int y = 46;
    center("FINGER DRUMS", SCREEN_W / 2, y, CLR_YELLOW); y += 12;
    print("TAP a drum/pad/critter to play it. WHERE you", 34, y, CLR_LIGHT_GREY); y += 8;
    print("hit sets VELOCITY: top = soft, low = hard.", 34, y, CLR_LIGHT_GREY); y += 8;
    print("Every finger plays at once (multitouch).", 34, y, CLR_LIGHT_GREY); y += 11;
    print("TAB   cycle 5 kits:", 34, y, CLR_PEACH); y += 8;
    print("   acoustic / MPC pads / pikuniku /", 34, y, CLR_PEACH); y += 8;
    print("   mac demarco (kit) / mac - tape (box)", 34, y, CLR_PEACH); y += 11;
    print("KEYS  kit: F G hat  T Y cym  J snare", 34, y, CLR_LIGHT_GREY); y += 8;
    print("           I K M toms  SPACE kick", 34, y, CLR_LIGHT_GREY); y += 8;
    print("      pads: 1-4 / QWER / ASDF / ZXCV", 34, y, CLR_LIGHT_GREY); y += 8;
    print("SHIFT accents a key hit; MIDI = real velocity.", 34, y, CLR_LIGHT_GREY); y += 11;
    center("H  close", SCREEN_W / 2, y, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw(void) {
    switch (kit) {
    case KIT_ACOUSTIC: draw_persp(&TH_AC); break;
    case KIT_MAC_AC:   draw_persp(&TH_MAC); break;
    case KIT_MPC:      draw_padgrid(false); break;
    case KIT_MAC_DIG:  draw_padgrid(true); break;
    case KIT_PIKU:     draw_piku(); break;
    }
    rectfill(0, 0, SCREEN_W, 14, CLR_BLACK);
    print(KIT_NAME[kit], 4, 4, CLR_WHITE);
    print(str("%d/%d", kit + 1, NKIT), 150, 4, CLR_INDIGO);
    print(str("VEL %d", last_vol), 180, 4, CLR_LIME_GREEN);
    print("TAB kit  H help", SCREEN_W - 110, 4, CLR_DARK_GREY);
    if (show_help) draw_help();
}
