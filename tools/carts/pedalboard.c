/* de:meta
{
  "slug": "pedalboard",
  "title": "pedalboard",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "chord-voicing",
    "granular-synth"
  ],
  "lineage": "Showcase cart for fx_order() (the reorderable effect insert chain); guitar fretboard with moveable barre-chord shapes is new to the library, as is the GRAINS granular-delay pedal.",
  "description": "An electric guitar you PLAY through a CHAIN of stompboxes you BUILD - the showcase for fx_order(): the order pedals sit in the chain is the order the engine runs them, so moving a pedal actually changes the tone (bitcrush BEFORE vs AFTER eq sounds different). Tap '= PEDALS' (top-left) to open the palette - a tray of 9 effects drawn as little icon+name chips (BITCRUSH, EQ, CHORUS, PHASER, FLANGER, TAPE, TREMOLO, WAH, REVERB). Drag a chip UP into the chain to add it, drag a chain pedal by its label sideways to reorder, drag one DOWN out to remove; a scrollbar appears when the chain outgrows the screen. Each pedal has its real knob row (drag to dial) and footswitch (tap, or 1-9 by position). Below: a real six-string guitar (INSTR_GUITAR) - pick a chord on the ROOT (Z X C V B N M) + SHAPE (A S D F G) rows, then sweep the strings to strum (SPACE strums; M-row autoplay). Mouse and touch both work, every finger its own pointer. (Reverb is a send, so its chain position is cosmetic for now - the multiple-reverb-tanks step makes it a true insert.) The OD pedal's VOICE knob picks a famous dirt box via drive_voice() - RAW / Tube Screamer (mid hump) / RAT (hard clip + filter) / Big Muff (fuzz + scoop) - with TONE riding that voice."
}
de:meta */
// pedalboard — an electric guitar you PLAY, through a CHAIN of stompboxes you BUILD. The showcase
// for fx_order(): the order pedals sit in the chain is the order the engine runs them, so moving
// a pedal actually changes the tone.
//
// THE PEDALBOARD (top): a horizontal signal chain, left→right. Tap "≡ PEDALS" (top-left) to open
// the PALETTE in the lower half — a tray of every effect, drawn as a little icon + name. Then:
//   • DRAG a palette chip UP into the chain to ADD it.
//   • DRAG a chain pedal by its LABEL sideways to REORDER it (the sound reorders with it).
//   • DRAG a chain pedal DOWN out of the chain to REMOVE it (it returns to the palette).
//   • drag a KNOB to dial it; tap the FOOTSWITCH to toggle on/off; 1..9 toggle by position.
// The chain can outgrow the screen — a scrollbar appears; drag it to pan. (REVERB is now a real
// dry/wet INSERT (reverb_insert, Increment C), so its POSITION is audible — put it before the
// bitcrush to crush the wet tail, or after to reverb the crushed guitar. See effects-bus-architecture.md.)
// The VOWEL pedal (formant filter) makes the guitar TALK; its MOD knob picks what moves the vowel —
// MAN (drag VWL by hand), ENV (each pick opens it), STP (a new vowel per pick — a spoken syllable),
// LFO (auto-sweeps on its own; VWL becomes the rate).
//
// THE GUITAR (lower half, when the palette is closed):
//   FRETTING HAND — ROOT row (Z X C V B N M) moves up the neck (E F G A B C D); SHAPE row (A S D
//                   F G) sets the chord shape (5 / min / maj / sus4 / 7).
//   STRUMMING HAND — sweep across the strings over the body (the STRUM zone) to strum; tap a string
//                    on the neck to pick one; SPACE strums. M-row toggles autoplay.
//
// Mouse + touch both work — every contact is its own pointer. The mouse is merged in explicitly.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include "fxicons.h"      // shared effect icons + colours (also used by the epiano)
#include "ampcab.h"       // the shared amp/cab voicing table — the CABINET slot's "guitar amp" tenant
#include <math.h>

#define I_GTR  5
#define I_MUTE 6      // a choked, muted voice for picking the short nut-side string segment
#define NSTR   6
#define MAXK   4
#define NSHAPE 5
#define NROOT  7

// ── the effect catalog: every pedal you can drag into the chain ──────────────────────────────
// kind = the engine FX_* insert kind (its slot in the reorderable chain). Every pedal — REVERB
// included now (FX_REVERB via reverb_insert) — is a real insert, so chain order is audible.
enum { C_BIT, C_EQ, C_CHO, C_PHA, C_FLG, C_TAP, C_TRM, C_WAH, C_RVB, C_FMT, C_PAN, C_FIL, C_RNG, C_DLY, C_LOFI, C_FUZZ, C_GRN, C_EQ2, C_OD, C_SHW, C_GATE, C_SHMR, NCAT };
typedef struct {
    const char *name; int body, accent, kind, nk;
    const char *klabel[MAXK]; float kdef[MAXK];
} FxDef;
static const FxDef CAT[NCAT] = {
    { "BITCRUSH", CLR_DARK_BROWN,    CLR_DARK_ORANGE,  FX_CRUSH,   3, { "BIT","RTE","MIX" },   { 0.50f, 0.40f, 0.60f } },
    { "EQ",       CLR_DARKER_BLUE,   CLR_BLUE,         FX_EQ,      3, { "LO","MID","HI" },     { 0.50f, 0.50f, 0.50f } },
    { "CHORUS",   CLR_DARKER_PURPLE, CLR_PINK,         FX_CHORUS,  3, { "RTE","DEP","MIX" },   { 0.30f, 0.28f, 0.45f } },
    { "PHASER",   CLR_DARK_GREEN,    CLR_LIME_GREEN,   FX_PHASER,  4, { "RTE","DEP","FB","MX" },{ 0.30f, 0.70f, 0.65f, 0.55f } },
    { "FLANGER",  CLR_BLUE_GREEN,    CLR_MEDIUM_GREEN, FX_FLANGER, 4, { "RT","DP","FB","MX" }, { 0.20f, 0.70f, 0.60f, 0.50f } },
    { "TAPE",     CLR_DARK_RED,      CLR_PEACH,        FX_TAPE,    3, { "WOW","FLT","SAT" },   { 0.35f, 0.25f, 0.45f } },
    { "TREMOLO",  CLR_DARKER_GREY,   CLR_LIGHT_YELLOW, FX_TREM,    3, { "SPD","DEP","WAV" },   { 0.45f, 0.60f, 0.0f } },
    { "WAH",      CLR_DARK_PURPLE,   CLR_MAUVE,        FX_WAH,     4, { "SNS","RES","MIX","MOD" },{ 0.50f, 0.55f, 0.70f, 0.0f } },
    { "REVERB",   CLR_DARK_BLUE,     CLR_INDIGO,       FX_REVERB,  3, { "SIZ","DMP","MIX" },   { 0.70f, 0.40f, 0.45f } },
    { "VOWEL",    CLR_BROWN,         CLR_LIGHT_PEACH,  FX_FORMANT, 4, { "VWL","Q","MIX","MOD" },{ 0.50f, 0.60f, 0.90f, 0.0f } },
    { "AUTOPAN",  CLR_DARK_GREY,     CLR_LIGHT_YELLOW, FX_PAN,     3, { "SPD","DEP","WAV" },   { 0.35f, 0.70f, 0.0f } },
    { "FILTER",   CLR_TRUE_BLUE,     CLR_BLUE,         FX_FILTER,  3, { "CUT","RES","MOD" },   { 0.50f, 0.30f, 0.0f } },
    { "RINGMOD",  CLR_INDIGO,        CLR_GREEN,        FX_RINGMOD, 2, { "FRQ","MIX" },         { 0.30f, 0.80f } },
    { "DELAY",    CLR_DARK_PEACH,    CLR_ORANGE,       FX_ECHO,    4, { "TIM","FB","TON","MIX" },{ 0.35f, 0.40f, 0.55f, 0.45f } },
    // LO-FI is a MACRO pedal (kind -1, custom icon): one box drives crush+tape+filter together — a
    // recipe of existing inserts (decisions/0015), not a new effect. It OWNS those three when on.
    { "LO-FI",    CLR_DARK_BROWN,    CLR_PEACH,        -1,         3, { "AMT","WOW","TON" },   { 0.50f, 0.40f, 0.45f } },
    // FUZZ is a per-voice DRIVE pedal (kind -2, custom icon): a germanium/silicon fuzz on the guitar
    // slot. Like LO-FI it's a recipe, not an FX_* insert — and it OWNS the slot's one drive stage, so
    // it LOCKS while the cabinet's on AMP (the amp owns that drive too). MODE: germanium ↔ silicon.
    { "FUZZ",     CLR_DARK_RED,      CLR_ORANGE,       -2,         2, { "FUZZ","MODE" },       { 0.65f, 0.0f } },
    // GRAINS is the granular-delay INSERT (FX_GRAINS): a capture-and-scatter texture cloud. 6 engine
    // params but only 4 knobs — SIZE/DENS/MIX are continuous, FRZ is a discrete FREEZE toggle (like
    // FUZZ's MODE / WAH's MOD). position/scatter/feedback are fixed to a good "shimmer cloud" voicing.
    { "GRAINS",   CLR_INDIGO,        CLR_MAUVE,        FX_GRAINS,  4, { "SIZE","DENS","MIX","FRZ" }, { 0.30f, 0.30f, 0.50f, 0.0f } },
    // EQ·2 is a SECOND EQ instance (Increment F): same FX_EQ, but tagged FX_INST(FX_EQ,1) in the order
    // and configured via eq_inst(1,…), so you can EQ before AND after a dirt stage (e.g. EQ→CRUSH→EQ·2).
    { "EQ2",      CLR_DARKER_BLUE,   CLR_TRUE_BLUE,    FX_EQ,      3, { "LO","MID","HI" },     { 0.50f, 0.50f, 0.50f } },
    // OD is a 2nd mix-bus DRIVE (FX_DRIVE instance 1) — an overdrive/boost pedal IN the chain that
    // coexists with the amp cabinet's drive (instance 0). VOICE picks a famous pedal via drive_voice():
    // RAW (plain asym) / TS (Tube Screamer, mid hump) / RAT (hard clip + filter) / MUFF (fuzz + scoop).
    // TONE rides that voice's tone. (drive_voice is global, so it clears when OD is off/removed.)
    { "OD",       CLR_DARK_ORANGE,   CLR_PEACH,        FX_DRIVE,   4, { "DRV","VOICE","TONE","MIX" },  { 0.50f, 0.0f, 0.5f, 1.00f } },
    // SHALLOW is the Fairfield Shallow Water INSERT (FX_SHALLOW=16, the first kind past the old 16-kind
    // ceiling): a filtered-random short delay (warped-water warble) + a Low Pass Gate. RATE/DEPTH/MIX.
    { "SHALLOW",  CLR_DARKER_BLUE,   CLR_BLUE,         FX_SHALLOW, 3, { "RATE","DEP","MIX" },  { 0.30f, 0.60f, 0.50f } },
    // GATE is the noise gate INSERT (FX_GATE=17): clamps the signal shut below THRSH. Put it after the
    // REVERB pedal in the chain for gated reverb (the tail chops). THRSH · ATK (fast) · REL (tail cut).
    { "GATE",     CLR_DARK_GREEN,    CLR_GREEN,        FX_GATE,    3, { "THR","ATK","REL" },   { 0.35f, 0.10f, 0.40f } },
    // SHIMMER is a MACRO pedal (kind -3, custom icon): the master shimmer() — a reverb with an
    // octave-up pitch-shifter in its feedback loop (the ascending crystalline tail). It's a master
    // OUTPUT-STAGE effect (runs after the whole chain, like a reverb at the end of a pedalboard), so
    // its chain position is cosmetic — it always shimmers the finished guitar tone. SIZE/DAMP/SHIM/MIX.
    { "SHIMMER",  CLR_DARKER_BLUE,   CLR_INDIGO,       -3,         4, { "SIZE","DMP","SHM","MIX" }, { 0.85f, 0.40f, 0.60f, 0.45f } },
};

// ── the chain: an ordered list of DISTINCT catalog ids, each with its own knobs + on-state ──
typedef struct { int cat; float k[MAXK]; bool on; } Slot;
static Slot  chain[NCAT];
static int   chain_n   = 0;
static float scroll_x  = 0.0f;     // horizontal pan of the chain view
static bool  palette_open = false;
static int   dirty     = 1;

// ── the pinned CABINET output stage (Increment E): none / guitar amp / Leslie ──
// "none" is a true no-op — pedalboard byte-identical to before; the cabinet only colours the output
// once you opt in. Amp = the AMP_VC bundle (drive+eq+glue on the guitar slot + master glue); Leslie
// = the rotary cabinet (master leslie). Both run AFTER the insert chain, off the shared insert buses.
enum { CAB_NONE, CAB_AMP, CAB_LESLIE };
static int   cab_tenant  = CAB_NONE;
static bool  cabfuzz_applied = false;    // was the cabinet OR fuzz engaged last apply? (untouched session stays byte-identical)
static int   cab_voicing = 2;            // AMP_VC index (CRUNCH)
static int   cab_speed   = LESLIE_SLOW;  // Leslie rotor speed
static float cab_k[2]    = { 0.5f, 0.5f }; // amp: GAIN, SAG  ·  leslie: DRIVE, BALANCE

// ── RIG recall (Phase 3): named "legendary setups" that load the WHOLE board at once — which
// pedals (with their default knobs, all switched on) AND the cabinet tenant/voicing. The cabinet
// is the centrepiece each rig points at; the pedals just add the era flavour. Tweak after loading.
static bool rig_open = false;
typedef struct { const char *name, *sub; int n, cat[4]; int tenant, voicing, speed; float k0, k1; } Rig;
#define NRIG 6
static const Rig RIG[NRIG] = {
    { "CLEAN TWANG", "clean amp - slap + verb", 2, { C_DLY, C_RVB }, CAB_AMP,    0, 0,           0.35f, 0.25f },
    { "JANGLE",      "chime amp - chorus",      2, { C_CHO, C_RVB }, CAB_AMP,    1, 0,           0.45f, 0.35f },
    { "CRUNCH",      "plexi crunch - verb",     1, { C_RVB },        CAB_AMP,    2, 0,           0.60f, 0.45f },
    { "HI-GAIN",     "hot-rod - tight & dry",   0, { 0 },            CAB_AMP,    3, 0,           0.80f, 0.60f },
    { "PSYCH SWIRL", "phaser into Leslie",      1, { C_PHA },        CAB_LESLIE, 0, LESLIE_SLOW, 0.40f, 0.60f },
    { "LO-FI",       "broken cassette amp",     1, { C_LOFI },       CAB_AMP,    4, 0,           0.50f, 0.50f },
};

// ── the fretting hand: real guitar tab ──  standard tuning, E-shape MOVEABLE chords.
static const int OPEN[NSTR] = { 40, 45, 50, 55, 59, 64 };   // E A D G B E (low→high)
static const int SHAPE_F[NSHAPE][NSTR] = {
    // ordered so the THIRD climbs left→right (none → ♭3 → ♮3 → 4): a musical gradient
    { 0, 2, 2, -1, -1, -1 },   // 5    power — finger root/5th/octave; high strings ring open
    { 0, 2, 2,  0,  0,  0 },   // min  E-shape minor
    { 0, 2, 2,  1,  0,  0 },   // maj  E-shape major
    { 0, 2, 2,  2,  0,  0 },   // sus4 suspended fourth
    { 0, 2, 0,  1,  0,  0 },   // 7    E-shape dominant 7
};
static const char *SHAPE_NAME[NSHAPE] = { "5", "min", "maj", "sus4", "7" };
static const char  SHAPE_KEY[NSHAPE]  = { 'A', 'S', 'D', 'F', 'G' };
static const int   ROOT_FRET[NROOT]   = { 0, 1, 3, 5, 7, 8, 10 };         // barre fret for E F G A B C D
static const char *ROOT_NAME[NROOT]   = { "E", "F", "G", "A", "B", "C", "D" };
static const char  ROOT_KEY[NROOT]    = { 'Z', 'X', 'C', 'V', 'B', 'N', 'M' };
#define FRET_W 7
static int  sel_shape = 0;
static int  sel_root  = 0;
static int  str_midi[NSTR];

static float amp[NSTR];
static float vib_ph[NSTR];
static int   pend[NSTR];

static bool  autoplay = true;
static int   apos = 0;

// ── geometry ──
#define PED_Y 14
#define PED_H 70                     // a touch shorter (was 72) — trimmed padding feeds the neck
#define PED_W 54                     // a touch wider — room for the staggered knobs + side labels
#define PITCH 58
#define CHAIN_X0 4
#define VIEW_W  254                  // chain viewport: x 4..258 (shrunk to pin the CABINET box at right)
#define VIEW_R  (CHAIN_X0 + VIEW_W)
#define CAB_W   54                   // the pinned output-cabinet box (never scrolls) — "the chain plugs into it"
#define CAB_X   (SCREEN_W - CAB_W - 2)   // = 264; box spans 264..318
#define SB_Y    (PED_Y + PED_H)      // scrollbar track (only drawn on overflow)
#define ILLU_CY (PED_Y + 13)         // illustration center — pulled up, padding trimmed
#define PAL_Y   88                   // palette panel top (when open)
#define SX0   22                     // nut (neck end)
#define SX1   302                    // bridge (body end)
#define STRUMX 196                   // strum zone starts here (right side, over the body)
#define STR_Y0 93                    // strings spread WIDER now (10px, was 7) — easy to pick on a phone
#define STR_DY 10
#define STR_Y(s) (STR_Y0 + (s) * STR_DY)
#define CHORD_H 21                   // chord buttons ~1.5× taller (was 14), parked at the bottom
#define SHAPE_Y 154
#define SHAPE_W 56
#define SHAPE_X(i) (12 + (i) * 60)
#define ROOT_Y  177
#define ROOT_W  40
#define ROOT_X(i) (11 + (i) * 43)

// knobs are STAGGERED (zigzag, 2 columns) so each gets room to be bigger than a cramped row.
// even j → left column, odd j → right column; each on its own row down the pedal.
static int knob_cx(int x, int j)  { return (j & 1) ? (x + PED_W - 16) : (x + 16); }
static int knob_cy(int j, int nk) { return PED_Y + 24 + j * (nk <= 3 ? 13 : 9); }
static int knob_rad(int nk)       { return nk <= 3 ? 6 : 5; }
static int gate_ms(void) { return 1800; }

static int str_fret(int s) { return SHAPE_F[sel_shape][s] < 0 ? 0 : ROOT_FRET[sel_root] + SHAPE_F[sel_shape][s]; }
static void build_strings(void) {
    for (int s = 0; s < NSTR; s++) str_midi[s] = OPEN[s] + str_fret(s);
}

// ── chain helpers ──
static int  chain_index(int cat) { for (int i = 0; i < chain_n; i++) if (chain[i].cat == cat) return i; return -1; }
static bool pedal_on(int cat) { int i = chain_index(cat); return i >= 0 && chain[i].on; }
// LO-FI (a macro over crush+tape+filter) and the standalone BITCRUSH/TAPE/FILTER share one insert
// each per bus, so only ONE side can be ON at a time. Both may live in the chain freely — a pedal is
// just LOCKED (can't switch on, drawn dimmed) while a conflicting pedal is currently on; turn that
// one off and this frees up. The conflict is on-state, not chain membership (no dead pedals).
static bool pedal_locked(int cat) {
    // No pedal locks any more. LO-FI vs BITCRUSH/TAPE/FILTER → Increment F instances; FUZZ vs the amp
    // cabinet → fuzz is per-voice (pre-chain), the amp drives the bus (FX_DRIVE, end of chain), so they
    // stack. Kept as a no-op so the chain/draw call sites don't need touching.
    (void)cat;
    return false;
}
static int  content_w(void)      { return chain_n * PITCH; }
static float max_scroll(void)    { float m = (float)(content_w() - VIEW_W); return m < 0 ? 0 : m; }
static void clamp_scroll(void)   { float m = max_scroll(); if (scroll_x < 0) scroll_x = 0; if (scroll_x > m) scroll_x = m; }
static void chain_insert(int cat, int at) {
    if (chain_n >= NCAT || chain_index(cat) >= 0) return;
    if (at < 0) at = 0; if (at > chain_n) at = chain_n;
    for (int i = chain_n; i > at; i--) chain[i] = chain[i - 1];
    chain[at].cat = cat; chain[at].on = true;
    for (int j = 0; j < MAXK; j++) chain[at].k[j] = CAT[cat].kdef[j];
    chain_n++;
}
static void chain_remove(int idx) {
    if (idx < 0 || idx >= chain_n) return;
    for (int i = idx; i < chain_n - 1; i++) chain[i] = chain[i + 1];
    chain_n--;
}
static void chain_move(int from, int to) {   // move element `from` to FINAL index `to`; size unchanged
    if (from < 0 || from >= chain_n) return;
    if (to < 0) to = 0; if (to > chain_n - 1) to = chain_n - 1;
    if (to == from) return;
    Slot s = chain[from];
    if (to > from) for (int i = from; i < to; i++) chain[i] = chain[i + 1];   // others slide left
    else           for (int i = from; i > to; i--) chain[i] = chain[i - 1];   // others slide right
    chain[to] = s;
}

// ── the VOWEL pedal's MOD knob: MANUAL / ENV / STEP ──────────────────────────────────────────
// formant(vowel,q,mix) takes a STATIC vowel; the "talking" comes from MOVING it. The MOD knob
// (k[3], snapped to 4 like TREMOLO's WAV) picks WHAT moves it — each mode is a different clock:
//   MANUAL (0) — the VWL knob IS the vowel; drag it while you play = a hand-swept talkbox.
//   ENV    (1) — each strum pops the vowel OPEN (from the VWL floor) and it relaxes back — picking
//                dynamics drive it, hands-free (the auto-wah gesture, but a vowel).
//   STEP   (2) — each strum advances to the NEXT vowel in a little "word", glided in — the guitar
//                speaks a syllable per pluck (the move nothing else does; VWL is ignored here).
//   LFO    (3) — the vowel auto-sweeps OO↔EE on its own, free-running; here the VWL knob = RATE.
// The cart drives all of this (formant() has no trigger input) — fmt_on_attack() on each pluck/
// strum, formant_tick() every frame easing the vowel toward its target and re-pushing only when it
// moved (a swept vowel is the intended motion — a cheap coeff update, no buffer churn).
static const int   FMT_WORD[6] = { 0, 2, 4, 2, 3, 1 };   // OO AH EE AH EH OH — the spoken "word"
static float fmt_vowel = 0.5f, fmt_target = 0.5f, fmt_env = 0.0f;
static int   fmt_step = 0;
static float fmt_lfo_ph = 0.0f;                          // free-running LFO phase (LFO mode)
static float fmt_last_v = -2.0f;
static int   fmt_mode_of(Slot *sl) { return (int)(sl->k[3] * 3.99f); }   // k[3] 0..1 → 0/1/2/3
static float fmt_live_vowel(Slot *sl) {
    int m = fmt_mode_of(sl);
    if (m == 1) return sl->k[0] + fmt_env * (1.0f - sl->k[0]);   // ENV:  open from the VWL floor
    if (m == 2) return fmt_vowel;                                 // STEP: the glided current vowel
    if (m == 3) return 0.5f - 0.5f * cosf(fmt_lfo_ph * 6.2831853f); // LFO: a free sweep OO↔EE
    return sl->k[0];                                              // MANUAL: the VWL knob
}
static void fmt_on_attack(void) {   // a pluck/strum landed
    int idx = chain_index(C_FMT);
    if (idx < 0 || !chain[idx].on) return;
    int m = fmt_mode_of(&chain[idx]);
    if (m == 1) fmt_env = 1.0f;                                              // ENV: pop open
    else if (m == 2) { fmt_step = (fmt_step + 1) % 6; fmt_target = FMT_WORD[fmt_step] / 4.0f; }  // STEP: next vowel
}
static void formant_tick(void) {    // every frame: ease the moving modes, re-push on change
    int idx = chain_index(C_FMT);
    if (idx < 0 || !chain[idx].on) return;
    Slot *sl = &chain[idx];
    int m = fmt_mode_of(sl);
    if (m == 0) return;                                          // MANUAL: apply_fx() handles it on knob change
    if (m == 1) fmt_env *= 0.93f;                                // ENV decays back toward the floor
    else if (m == 2) fmt_vowel += (fmt_target - fmt_vowel) * 0.16f;  // STEP glides toward the target vowel
    else { fmt_lfo_ph += 0.003f + sl->k[0] * 0.04f;              // LFO: VWL knob = rate (~0.18..2.6 Hz @60fps)
           if (fmt_lfo_ph >= 1.0f) fmt_lfo_ph -= 1.0f; }
    float v = fmt_live_vowel(sl);
    if (fabsf(v - fmt_last_v) > 0.002f) { formant(v, sl->k[1], sl->k[2]); fmt_last_v = v; }
}

// add an FX_* kind to the order list, skipping duplicates (the LO-FI macro emits crush/tape/filter,
// which a standalone BITCRUSH/TAPE/FILTER pedal may also emit — never list a kind twice).
static void kinds_add(int *kinds, int *n, int kd) {
    for (int j = 0; j < *n; j++) if (kinds[j] == kd) return;
    kinds[(*n)++] = kd;
}

// restore the init() guitar baseline (so switching the cabinet OFF un-amps the string)
static void cab_reset_guitar(void) {
    instrument_drive_mode(I_GTR, DRIVE_SOFT);
    instrument_timbre(I_GTR, 0.85f);
    instrument_drive(I_GTR, 0.18f);
    instrument_eq(I_GTR, 0.0f, 0.0f, 0.0f);
    glue(0, 0.0f, 8, 120);
}

// load a whole RIG: rebuild the chain (listed pedals, on, default knobs) + set the cabinet.
static void apply_rig(int r) {
    const Rig *g = &RIG[r];
    chain_n = 0; scroll_x = 0.0f;
    for (int i = 0; i < g->n; i++) {
        chain[i].cat = g->cat[i]; chain[i].on = true;
        for (int j = 0; j < MAXK; j++) chain[i].k[j] = CAT[g->cat[i]].kdef[j];
        chain_n++;
    }
    cab_tenant = g->tenant; cab_voicing = g->voicing; cab_speed = g->speed;
    cab_k[0] = g->k0; cab_k[1] = g->k1;
    clamp_scroll(); dirty = 1;
}

// push every effect's state to the engine, then set the INSERT ORDER from the chain order.
// An effect not in the chain (or off) is pushed dry. REVERB is now a real dry/wet INSERT
// (reverb_insert → FX_REVERB in the chain), so its POSITION is audible — drag it before/after
// the bitcrusher and you crush the wet tail vs. reverb the crushed guitar (Increment C).
static void apply_fx(void) {
    for (int c = 0; c < NCAT; c++) {
        int idx = chain_index(c);
        bool act = (idx >= 0) && chain[idx].on;
        const float *k = (idx >= 0) ? chain[idx].k : CAT[c].kdef;
        switch (c) {
            case C_BIT: crush(16.0f - k[0] * 14.0f, 1.0f + k[1] * 15.0f, act ? k[2] : 0.0f); break;
            case C_EQ:  if (act) eq((k[0]-0.5f)*24.0f, (k[1]-0.5f)*24.0f, (k[2]-0.5f)*24.0f); else eq(0.0f, 0.0f, 0.0f); break;
            case C_EQ2: if (act) eq_inst(1, (k[0]-0.5f)*24.0f, (k[1]-0.5f)*24.0f, (k[2]-0.5f)*24.0f); else eq_inst(1, 0.0f, 0.0f, 0.0f); break;
            case C_OD: {  // 2nd bus drive (FX_DRIVE inst 1). VOICE k[1] → drive_voice: 0 RAW/1 TS/2 RAT/3 MUFF; TONE k[2]
                int voice = (int)(k[1] * 3.99f);                     // 0..3 == DRIVE_VOICE_NONE/TS/RAT/MUFF
                drive_voice(act ? voice : DRIVE_VOICE_NONE, k[2]);   // global — cleared when OD off/removed (this case always runs)
                drive_insert_inst(1, act ? k[0] : 0.0f, DRIVE_ASYM, act ? k[3] : 0.0f);   // MIX = k[3]
            } break;
            case C_SHW: shallow(0.2f + k[0] * 7.8f, k[1], act ? k[2] : 0.0f); break;   // RATE 0.2..8 Hz, DEP, MIX (off = bypass)
            case C_GATE: gate(act ? k[0] : 0.0f, 1 + (int)(k[1] * 15.0f), 20 + (int)(k[2] * 380.0f)); break;   // THR (off = bypass), ATK 1..16ms, REL 20..400ms
            case C_SHMR: shimmer(k[0], k[1], k[2], act ? k[3] : 0.0f); break;   // master shimmer (output stage): SIZE/DAMP/SHIM, MIX (off = bypass)
            case C_CHO: chorus(0.1f + k[0] * 4.9f, k[1], act ? k[2] : 0.0f); break;
            case C_PHA: phaser(0.1f + k[0] * 9.9f, k[1], (k[2]-0.5f) * 1.8f, act ? k[3] : 0.0f, 4); break;
            case C_FLG: flanger(0.05f + k[0] * 4.95f, k[1], k[2] * 0.95f, act ? k[3] : 0.0f); break;
            case C_TAP: tape(act ? k[0] : 0.0f, act ? k[1] : 0.0f, act ? k[2] : 0.0f); break;
            case C_TRM: tremolo(0.5f + k[0] * 11.5f, act ? k[1] : 0.0f, (int)(k[2] * 7.99f)); break;   // WAV picks any LFO_SHAPE_* (8)
            case C_WAH: if ((int)(k[3] * 1.99f)) wah_lfo(0.5f + k[0] * 9.5f, k[1], act ? k[2] : 0.0f);  // LFO: SNS knob = rate 0.5..10 Hz
                        else                     wah(k[0], k[1], act ? k[2] : 0.0f);                    // ENV: dynamics-driven follower
                        break;
            case C_RVB: reverb_insert(0.2f + k[0] * 0.78f, k[1], act ? k[2] : 0.0f); break;   // a real in-line dry/wet reverb pedal
            case C_FMT: { float v = (idx >= 0) ? fmt_live_vowel(&chain[idx]) : k[0]; formant(v, k[1], act ? k[2] : 0.0f); fmt_last_v = v; } break;
            case C_PAN: autopan(0.5f + k[0] * 11.5f, act ? k[1] : 0.0f, (int)(k[2] * 7.99f)); break;   // WAV picks any LFO_SHAPE_* (8)
            case C_FIL: { static const int FM[4] = { FILTER_LOW, FILTER_HIGH, FILTER_BAND, FILTER_NOTCH };  // CUT exp 40..18k Hz, MOD picks the mode, off = bypass
                          filter(act ? FM[(int)(k[2] * 3.99f)] : FILTER_OFF, 40.0f * powf(450.0f, k[0]), k[1]); } break;
            case C_RNG: ringmod(20.0f * powf(150.0f, k[0]), act ? k[1] : 0.0f); break;  // FRQ exp 20..3000 Hz
            case C_DLY: echo_insert((int)(20.0f + k[0] * 1480.0f), k[1], k[2], act ? k[3] : 0.0f); break;  // TIM 20..1500ms, FB, TON, MIX (off = bypass)
            case C_GRN: grains(20.0f + k[0] * 480.0f, 2.0f + k[1] * 48.0f, 0.8f, 0.4f, 0.3f, act ? k[2] : 0.0f);  // SIZE 20..500ms, DENS 2..50/s, MIX (off = bypass); pos/scatter/fb fixed
                        grains_freeze(act && k[3] > 0.5f); break;                                            // FRZ knob = freeze toggle (loop the captured buffer)
            case C_LOFI:  // the macro: AMT crunches bits + adds sample-rate + tape sat; WOW warbles; TON darkens.
                // Increment F: drives crush/tape/filter INSTANCE 1, so it COEXISTS with standalone
                // BITCRUSH/TAPE/FILTER (instance 0) — no lock. Off-branch turns its instance off.
                if (act) {
                    crush_inst(1, 16.0f - k[0] * 9.0f, 1.0f + k[0] * 5.0f, 0.45f + k[0] * 0.45f);  // 16→7 bits, gentle downsample, blended in
                    tape_inst(1, k[1] * 0.7f, k[1] * 0.5f, 0.2f + k[0] * 0.5f);                     // WOW = warble; AMT = saturation
                    filter_inst(1, FILTER_LOW, 700.0f * powf(16.0f, k[2]), 0.2f);                   // TON = cutoff: dark muffle → open
                } else {
                    crush_inst(1, 8.0f, 4.0f, 0.0f);          // mix 0 = off
                    tape_inst(1, 0.0f, 0.0f, 0.0f);           // off
                    filter_inst(1, FILTER_OFF, 0.0f, 0.0f);   // bypass
                }
                break;
        }
    }
    int kinds[NCAT + 3], n = 0;   // +2 LO-FI macro (3 kinds from 1 slot) +1 the amp cabinet's FX_DRIVE
    for (int i = 0; i < chain_n; i++) {
        int cat = chain[i].cat;
        if (cat == C_LOFI) { kinds_add(kinds, &n, FX_INST(FX_CRUSH, 1)); kinds_add(kinds, &n, FX_INST(FX_TAPE, 1)); kinds_add(kinds, &n, FX_INST(FX_FILTER, 1)); }   // instance 1: coexists with standalone
        else if (cat == C_EQ2) kinds_add(kinds, &n, FX_INST(FX_EQ, 1));   // 2nd EQ instance, distinct from FX_EQ
        else if (cat == C_OD)  kinds_add(kinds, &n, FX_INST(FX_DRIVE, 1)); // 2nd bus drive, distinct from the amp's FX_DRIVE (inst 0)
        else { int kd = CAT[cat].kind; if (kd >= 0) kinds_add(kinds, &n, kd); }
    }
    if (cab_tenant == CAB_AMP) kinds_add(kinds, &n, FX_DRIVE);   // the amp cabinet's drive = a bus insert at the END (output stage), so FUZZ (per-voice) sits BEFORE it
    fx_order(0, kinds, n);   // the chain order IS the insert order (Increment A)

    // ── FUZZ (per-voice, pre-chain) + the pinned CABINET output stage. Two SEPARATE drive stages now:
    // FUZZ is the per-voice instrument_drive (before the chain); the amp cabinet's drive is the bus
    // FX_DRIVE insert at the chain END (drive_insert, appended above). So fuzz feeds the amp — they
    // STACK, no lock (that's what the master/bus drive unlocked). EQ stays on the slot bus, glue/leslie master.
    int fi = chain_index(C_FUZZ);
    bool fuzz_on = (fi >= 0 && chain[fi].on);                           // fuzz works under ANY cabinet now
    bool engaged = (cab_tenant != CAB_NONE) || fuzz_on;
    if (engaged || cabfuzz_applied) {                                   // skip entirely while nothing's engaged (byte-identical)
        if (cab_tenant == CAB_AMP) {                                    // amp cabinet: EQ + glue (slot/master) + drive on the BUS
            const AmpVoicing *a = &AMP_VC[cab_voicing];
            leslie(LESLIE_STOP, 0.0f, 0.5f, 0.0f, 0.0f);
            instrument_timbre(I_GTR, a->timbre);
            instrument_eq(I_GTR, a->lo, a->mid, a->hi);
            glue(0, a->glue * cab_k[1], 8, 120);
            drive_insert(a->drive * (0.30f + 1.4f * cab_k[0]), a->mode, 1.0f);   // FX_DRIVE: amp drive on the summed bus
        } else if (cab_tenant == CAB_LESLIE) {                          // Leslie cabinet: clean slot + rotary
            cab_reset_guitar();
            drive_insert(0.0f, 0, 0.0f);                                // amp bus-drive off
            leslie(cab_speed, cab_k[0], cab_k[1], 0.5f, 1.0f);
        } else {                                                        // none: clean baseline
            cab_reset_guitar();
            drive_insert(0.0f, 0, 0.0f);
            leslie(LESLIE_STOP, 0.0f, 0.5f, 0.0f, 0.0f);
        }
        // the PER-VOICE drive = FUZZ (it precedes the amp). When the amp's on but fuzz isn't, keep the
        // voice clean into the amp (the amp drives on the bus); cab none/Leslie baseline is set above.
        if (fuzz_on) {
            float *fk = chain[fi].k;
            instrument_drive_mode(I_GTR, fk[1] < 0.5f ? DRIVE_ASYM : DRIVE_HARD);   // MODE: germanium ↔ silicon
            instrument_drive(I_GTR, 0.45f + fk[0] * 0.55f);                          // FUZZ amount → always saturated
        } else if (cab_tenant == CAB_AMP) {
            instrument_drive(I_GTR, 0.0f);                              // clean voice into the amp's bus drive
        }
        // the amp's rig-noise floor: hiss tracks the GAIN knob (a hot amp hisses; clean = silent) + a
        // touch of mains hum. Only the AMP tenant — Leslie/none are silent (a fantasy console is pristine).
        amp_noise(cab_tenant == CAB_AMP ? cab_k[0] * 0.45f : 0.0f, cab_tenant == CAB_AMP ? 0.10f : 0.0f, 60);
        cabfuzz_applied = engaged;
    }
}

// ── per-contact pointer pool ── (declared before init so init() can PTR_CLEAR it)
enum { PTR_IDLE, PTR_KNOB, PTR_PICK, PTR_DRAGSLOT, PTR_DRAGPAL, PTR_SCROLL, PTR_CABKNOB };
typedef struct { int id, mode, slot, knob, cat, prevY, x, y; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

void init(void) {
    instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 1200);
    instrument_harmonics(I_GTR, 0.55f);
    instrument_timbre(I_GTR, 0.85f);
    instrument_morph(I_GTR, 0.15f);
    instrument_drive_mode(I_GTR, DRIVE_SOFT);
    instrument_drive(I_GTR, 0.18f);
    instrument(I_MUTE, INSTR_GUITAR, 1, 0, 2, 180);
    instrument_harmonics(I_MUTE, 0.5f);
    instrument_timbre(I_MUTE, 0.95f);
    instrument_morph(I_MUTE, 0.92f);
    build_strings();
    PTR_CLEAR(ptr);   // a free slot's .id must be PTR_NONE (no longer relies on zero-init)
    bpm(100);
    // a tasteful starting chain (only TREMOLO ringing); the rest sit in the chain, off
    chain_insert(C_BIT, 0); chain[0].on = false;
    chain_insert(C_EQ, 1);  chain[1].on = false;
    chain_insert(C_CHO, 2); chain[2].on = false;
    chain_insert(C_TRM, 3);
    chain_insert(C_RVB, 4); chain[4].on = false;
    apply_fx();
    amp[0] = 0.8f; amp[2] = 1.0f; amp[4] = 0.6f;
}

static void pluck_str(int s, int vol) {
    if (s < 0 || s >= NSTR) return;
    hit(str_midi[s], I_GTR, vol, gate_ms());
    amp[s] = 1.0f; vib_ph[s] = 0.0f;
    fmt_on_attack();                          // VOWEL pedal: advance/open the vowel on each pick
}
static void strum_down(void) {
    for (int s = 0; s < NSTR; s++) {
        schedule_hit(s * 28, str_midi[s], I_GTR, 5, gate_ms());
        pend[s] = 1 + (s * 28 * 60) / 1000;
    }
    fmt_on_attack();                          // one vowel advance per strum (a syllable per chord)
}
static void set_shape(int sh) { sel_shape = sh; build_strings(); autoplay = false; }
static void set_root(int r)   { sel_root  = r;  build_strings(); autoplay = false; }
static int  near_string(int ty) { int s = (ty - STR_Y0 + STR_DY / 2) / STR_DY; return s < 0 ? 0 : s >= NSTR ? NSTR - 1 : s; }
static int dot_x(int s) {
    int f = str_fret(s);
    if (f == 0) return SX0 + 2;
    int dx = SX0 + 6 + f * FRET_W;
    return dx > STRUMX - 16 ? STRUMX - 16 : dx;
}
static void pick_string(int s, int px) {
    if (s < 0 || s >= NSTR) return;
    int f = str_fret(s);
    if (f > 0 && px < dot_x(s) - 1) {
        float seg = 1.0f - powf(2.0f, -f / 12.0f);
        int hi = (str_midi[s] - f) + (int)(12.0f * log2f(1.0f / seg) + 0.5f);
        if (hi > 103) hi = 103;
        hit(hi, I_MUTE, 4, 130);
        amp[s] = 0.7f; vib_ph[s] = 0.0f;
    } else {
        pluck_str(s, 6);
    }
}

// (per-contact pointer pool is declared above init(), where it's PTR_CLEAR'd)

// screen x of chain pedal i (may be off-screen); the slot under a point, or -1
static int ped_screen_x(int i) { return CHAIN_X0 + i * PITCH - (int)scroll_x; }
static int slot_under(int tx) {
    int i = (tx + (int)scroll_x - CHAIN_X0) / PITCH;
    if (i < 0 || i >= chain_n) return -1;
    int px = ped_screen_x(i);
    return (tx >= px && tx < px + PED_W) ? i : -1;
}

// build the list of palette-available cats (not in the chain) + the chip rect for the a-th of them
#define PAL_COLS 5
#define PAL_CW   58
#define PAL_CH   26
static int pal_avail(int *out) { int n = 0; for (int c = 0; c < NCAT; c++) if (chain_index(c) < 0) out[n++] = c; return n; }
static void pal_chip_rect(int a, int *x, int *y) { *x = 5 + (a % PAL_COLS) * 62; *y = PAL_Y + 16 + (a / PAL_COLS) * (PAL_CH + 2); }

// the RIG panel: a 2-column list of "legendary setup" buttons (lower half, when rig_open)
#define RIG_W 150
#define RIG_H 26
static void rig_rect(int r, int *x, int *y) { *x = 6 + (r % 2) * 156; *y = PAL_Y + 16 + (r / 2) * (RIG_H + 4); }

// where a dragged pedal would land in the chain — SHARED by the live caret preview and the actual
// drop, so what you see is exactly where it goes. DRAGPAL can append (≤ chain_n); reorder can't.
static int drop_index(Ptr *p) {
    int t = (p->x + (int)scroll_x - CHAIN_X0 + PITCH / 2) / PITCH;
    int hi = (p->mode == PTR_DRAGPAL) ? chain_n : chain_n - 1;
    if (hi < 0) hi = 0;
    if (t < 0) t = 0; if (t > hi) t = hi;
    return t;
}

static void commit_drop(Ptr *p) {
    if (p->mode == PTR_DRAGSLOT) {
        int idx = chain_index(p->cat);
        if (idx < 0) return;
        if (p->y >= PED_Y + PED_H) chain_remove(idx);              // dropped below the chain → remove
        else                       chain_move(idx, drop_index(p)); // dropped in the chain → reorder
        clamp_scroll(); dirty = 1;
    } else if (p->mode == PTR_DRAGPAL) {
        if (p->y < PED_Y + PED_H && chain_index(p->cat) < 0 && chain_n < NCAT) {   // into the chain → add
            chain_insert(p->cat, drop_index(p));
            clamp_scroll(); dirty = 1;
        }
    }
}

void update(void) {
    for (int i = 0; i < NSHAPE; i++) if (keyp(SHAPE_KEY[i])) set_shape(i);
    for (int i = 0; i < NROOT;  i++) if (keyp(ROOT_KEY[i]))  set_root(i);
    for (int i = 0; i < chain_n; i++) if (keyp('1' + i) && (chain[i].on || !pedal_locked(chain[i].cat))) { chain[i].on = !chain[i].on; dirty = 1; }
    if (keyp(KEY_SPACE)) { strum_down(); autoplay = false; }

    if (tapp(4, 2, 56, 11))           { palette_open = !palette_open; if (palette_open) rig_open = false; }
    if (tapp(64, 2, 46, 11))          { rig_open = !rig_open; if (rig_open) palette_open = false; }
    if (tapp(SCREEN_W - 70, 4, 66, 10)) autoplay = !autoplay;

    bool overflow = max_scroll() > 0;

    int cid[PTR_MAX], cxp[PTR_MAX], cyp[PTR_MAX], nc = 0;
    for (int i = 0; i < touch_count() && nc < PTR_MAX; i++) { cid[nc] = touch_id(i); cxp[nc] = touch_x(i); cyp[nc] = touch_y(i); nc++; }
    if (mouse_down(MOUSE_LEFT) && nc < PTR_MAX) {
        int mxp = mouse_x(), myp = mouse_y(), dup = 0;
        for (int i = 0; i < nc; i++) { int dx = cxp[i] - mxp, dy = cyp[i] - myp; if (dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2) dup = 1; }
        if (!dup) { cid[nc] = -100; cxp[nc] = mxp; cyp[nc] = myp; nc++; }
    }

    for (int i = 0; i < nc; i++) {
        int id = cid[i], tx = cxp[i], ty = cyp[i];
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {
            *p = (Ptr){ id, PTR_IDLE, -1, -1, -1, ty, tx, ty };

            // 1. the chain (always live, palette open or not) — only within the (shrunk) viewport
            if (ty >= PED_Y && ty < PED_Y + PED_H && tx < VIEW_R) {
                int s = slot_under(tx);
                if (s >= 0) {
                    int px = ped_screen_x(s); int nk = CAT[chain[s].cat].nk;
                    int hitk = -1;                                        // a generous fat-finger box per staggered knob
                    for (int j = 0; j < nk; j++)
                        if (point_in_box(tx, ty, knob_cx(px, j) - 11, knob_cy(j, nk) - 7, 22, 14)) { hitk = j; break; }
                    if (point_in_box(tx, ty, px + 8, PED_Y + 57, PED_W - 16, 14) && (chain[s].on || !pedal_locked(chain[s].cat))) { chain[s].on = !chain[s].on; dirty = 1; }
                    else if (hitk >= 0) { p->mode = PTR_KNOB; p->slot = s; p->knob = hitk; }
                    else { p->mode = PTR_DRAGSLOT; p->slot = s; p->cat = chain[s].cat; }   // anywhere else = drag handle
                }
            }
            // 1b. the pinned CABINET box (right of the chain): header taps cycle the tenant, the
            // selector row steps the voicing/speed, the two knobs drag.
            if (p->mode == PTR_IDLE && ty >= PED_Y && ty < PED_Y + PED_H && tx >= CAB_X) {
                if (ty < PED_Y + 13) { cab_tenant = (cab_tenant + 1) % 3; dirty = 1; }           // header → none/amp/leslie
                else if (cab_tenant != CAB_NONE) {
                    if (ty < PED_Y + 28) {                                                        // selector row → step
                        if (cab_tenant == CAB_AMP) cab_voicing = (cab_voicing + 1) % AMPCAB_N;
                        else                       cab_speed   = (cab_speed + 1) % 3;
                        dirty = 1;
                    } else if (point_in_box(tx, ty, CAB_X + 4, PED_Y + 34, CAB_W / 2 - 4, 24)) { p->mode = PTR_CABKNOB; p->knob = 0; }
                    else if (point_in_box(tx, ty, CAB_X + CAB_W / 2, PED_Y + 34, CAB_W / 2 - 4, 24)) { p->mode = PTR_CABKNOB; p->knob = 1; }
                }
            }
            // 2. scrollbar
            if (p->mode == PTR_IDLE && overflow && ty >= SB_Y && ty <= SB_Y + 6) p->mode = PTR_SCROLL;
            // 3. palette chips (only when open)
            if (p->mode == PTR_IDLE && palette_open && ty >= PAL_Y) {
                int avail[NCAT], na = pal_avail(avail);
                for (int a = 0; a < na; a++) { int cx2, cy2; pal_chip_rect(a, &cx2, &cy2);
                    if (point_in_box(tx, ty, cx2, cy2, PAL_CW, PAL_CH)) { p->mode = PTR_DRAGPAL; p->cat = avail[a]; } }
            }
            // 3b. rig panel (only when open) — tap a setup to load the whole board, then close
            if (p->mode == PTR_IDLE && rig_open && ty >= PAL_Y) {
                for (int r = 0; r < NRIG; r++) { int rx, ry; rig_rect(r, &rx, &ry);
                    if (point_in_box(tx, ty, rx, ry, RIG_W, RIG_H)) { apply_rig(r); rig_open = false; break; } }
            }
            // 4. the guitar (only when no overlay is open)
            if (p->mode == PTR_IDLE && !palette_open && !rig_open) {
                for (int i2 = 0; i2 < NSHAPE; i2++) if (point_in_box(tx, ty, SHAPE_X(i2), SHAPE_Y, SHAPE_W, CHORD_H)) set_shape(i2);
                if (p->mode == PTR_IDLE)
                    for (int i2 = 0; i2 < NROOT; i2++) if (point_in_box(tx, ty, ROOT_X(i2), ROOT_Y, ROOT_W, CHORD_H)) set_root(i2);
                if (p->mode == PTR_IDLE && ty >= STR_Y0 - 9 && ty <= STR_Y(NSTR - 1) + 9 && tx >= SX0 - 8 && tx <= SX1 + 8) {
                    p->mode = PTR_PICK; autoplay = false;
                    pick_string(near_string(ty), tx);
                    p->prevY = ty;
                }
            }
        } else if (p->mode == PTR_KNOB) {
            if (p->slot < chain_n) { chain[p->slot].k[p->knob] = clamp(chain[p->slot].k[p->knob] + (p->prevY - ty) * 0.012f, 0.0f, 1.0f); dirty = 1; }
            p->prevY = ty;
        } else if (p->mode == PTR_CABKNOB) {
            cab_k[p->knob] = clamp(cab_k[p->knob] + (p->prevY - ty) * 0.012f, 0.0f, 1.0f); dirty = 1;
            p->prevY = ty;
        } else if (p->mode == PTR_PICK) {
            for (int s = 0; s < NSTR; s++) { int ys = STR_Y(s); if ((p->prevY < ys && ty >= ys) || (p->prevY > ys && ty <= ys)) pick_string(s, tx); }
            p->prevY = ty;
        } else if (p->mode == PTR_SCROLL) {
            scroll_x += (tx - p->x) * (content_w() / (float)VIEW_W); clamp_scroll();
        }
        p->x = tx; p->y = ty;
    }
    for (int j = 0; j < PTR_MAX; j++) {
        if (ptr[j].id == PTR_NONE) continue;
        int present = 0;
        for (int i = 0; i < nc; i++) if (cid[i] == ptr[j].id) { present = 1; break; }
        if (!present) { commit_drop(&ptr[j]); ptr[j].id = PTR_NONE; }
    }

    if (dirty) { apply_fx(); dirty = 0; }
    formant_tick();   // VOWEL pedal: ease the moving modes (ENV/STEP) and re-push the vowel when it shifts

    if (autoplay && every(1)) {
        static const int prog[8] = { 0, 2, 6, 3, 0, 5, 3, 2 };
        if (beat() % 4 == 0) { sel_shape = 0; sel_root = prog[apos % 8]; build_strings(); strum_down(); apos++; }
    }
    for (int s = 0; s < NSTR; s++) if (pend[s] > 0 && --pend[s] == 0) { amp[s] = 1.0f; vib_ph[s] = 0.0f; }

#ifdef DE_TRACE
    watch("chain_n", "%d", chain_n); watch("pal", "%d", palette_open);
    watch("cab", "%d", cab_tenant); watch("voicing", "%d", cab_voicing);
#endif
}

// the little face graphic for an effect, centered at (cx,cy) — reused by the chain pedal AND the
// (smaller) palette chip.
// the effect icons now live in the shared runtime/fxicons.h (fx_icon, keyed by FX_* kind) so the
// epiano's "pedals" match this board's exactly. CAT[].kind maps each catalog box to its FX_* kind.

// LO-FI is a macro (no single FX_* kind), so it can't use fx_icon() — its own little cassette.
static void lofi_icon(int cx, int cy, int col) {
    rrect(cx - 12, cy - 7, 24, 14, 2, col);          // the cassette shell
    circ(cx - 5, cy - 2, 2, col);                    // left reel
    circ(cx + 5, cy - 2, 2, col);                    // right reel
    line(cx - 3, cy - 2, cx + 3, cy - 2, col);       // tape spanning the reels
    line(cx - 6, cy + 4, cx + 6, cy + 4, col);       // the label strip
}

// FUZZ is also a macro (no single FX_* kind) — a spiky distortion burst.
static void fuzz_icon(int cx, int cy, int col) {
    for (int a = 0; a < 8; a++) {                    // 8 spikes radiating out
        float ang = a * 0.7853982f;                  // 45° apart
        int r = (a & 1) ? 4 : 8;                     // alternating long/short = jagged
        line(cx, cy, cx + (int)(cosf(ang) * r), cy + (int)(sinf(ang) * r), col);
    }
    circfill(cx, cy, 2, col);
}

// SHIMMER is a macro (master shimmer(), no FX_* kind) — rising chevrons + a spark (the ascending tail).
static void shimmer_icon(int cx, int cy, int col) {
    for (int i = 0; i < 3; i++) {                    // stacked upward chevrons = the climb
        int y = cy + 6 - i * 5;
        line(cx - 7, y, cx, y - 5, col);
        line(cx, y - 5, cx + 7, y, col);
    }
    pset(cx, cy - 9, col); circfill(cx + 5, cy - 8, 1, col);   // a spark at the top
}

// OD (FX_DRIVE) — a soft-clipped, flat-topped waveform (saturation).
static void od_icon(int cx, int cy, int col) {
    line(cx - 12, cy + 4, cx - 8, cy - 4, col);   // rise
    line(cx - 8, cy - 4, cx - 2, cy - 4, col);    // flat top (clipped)
    line(cx - 2, cy - 4, cx + 2, cy + 4, col);    // fall
    line(cx + 2, cy + 4, cx + 8, cy + 4, col);    // flat bottom (clipped)
    line(cx + 8, cy + 4, cx + 12, cy - 4, col);   // rise
}

static void draw_chain_pedal(int i, int x) {
    Slot *sl = &chain[i]; const FxDef *d = &CAT[sl->cat];
    // a conflicting pedal is live → can't switch on (drawn dimmed). FUZZ also dims while still ON under
    // an AMP cabinet — the amp owns the one drive stage, so the fuzz is overridden, not silently lit.
    bool locked = pedal_locked(sl->cat) && (!sl->on || sl->cat == C_FUZZ);
    int cx = x + PED_W / 2;
    int body = locked ? CLR_BROWNISH_BLACK : d->body;  // dark body = disabled (vs a colored off pedal)
    rrectfill(x, PED_Y, PED_W, PED_H, 4, body);
    rrect(x, PED_Y, PED_W, PED_H, 4, sl->on ? d->accent : CLR_DARKER_GREY);
    font(FONT_SMALL);
    print_centered(d->name, cx, PED_Y + 3, sl->on ? CLR_WHITE : CLR_MEDIUM_GREY);
    if (d->kind == -1)      lofi_icon(cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY);
    else if (d->kind == -2) fuzz_icon(cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY);
    else if (d->kind == -3) shimmer_icon(cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY);
    else if (d->kind == FX_DRIVE) od_icon(cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY);
    else fx_icon(d->kind, cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY, body);
    int kr = knob_rad(d->nk);
    int lblcol = sl->on ? CLR_LIGHT_PEACH : CLR_DARK_GREY;
    for (int j = 0; j < d->nk; j++) {
        int kx = knob_cx(x, j), ky = knob_cy(j, d->nk);
        circfill(kx, ky, kr, CLR_BROWNISH_BLACK);
        circ(kx, ky, kr, sl->on ? d->accent : CLR_DARK_GREY);
        float a = (-135.0f + sl->k[j] * 270.0f) * 0.0174533f;
        line(kx, ky, kx + (int)(sinf(a) * (kr - 1)), ky - (int)(cosf(a) * (kr - 1)), sl->on ? CLR_WHITE : CLR_MEDIUM_GREY);
        const char *lbl = d->klabel[j];
        if (d->kind == FX_FORMANT && j == 3) {                    // the MOD knob shows its mode, like TREM's WAV
            static const char *MN[4] = { "MAN", "ENV", "STP", "LFO" };
            lbl = MN[fmt_mode_of(sl)];
        }
        if (d->kind == FX_WAH) {                                  // WAH MOD knob: ENV (follower) vs LFO; knob 0 = SNS or RATE
            int wm = (int)(sl->k[3] * 1.99f);
            if (j == 3)      lbl = wm ? "LFO" : "ENV";
            else if (j == 0) lbl = wm ? "RTE" : "SNS";
        }
        if (d->kind == FX_FILTER && j == 2) {                    // FILTER MOD knob shows the mode
            static const char *FN[4] = { "LP", "HP", "BP", "NCH" };
            lbl = FN[(int)(sl->k[2] * 3.99f)];
        }
        if (d->kind == -2 && j == 1) lbl = sl->k[1] < 0.5f ? "GER" : "SIL";   // FUZZ MODE: germanium ↔ silicon
        if (d->kind == FX_DRIVE && j == 1) { static const char *DN[4] = { "RAW","TS","RAT","MUF" }; lbl = DN[(int)(sl->k[1] * 3.99f)]; }   // OD VOICE (RAW / Tube Screamer / RAT / Big Muff via drive_voice)
        if (d->kind == FX_GRAINS && j == 3) lbl = sl->k[3] > 0.5f ? "FRZN" : "LIVE";   // GRAINS FRZ: freeze toggle
        font(FONT_TINY);                                          // label tucked beside the knob (the empty column)
        if (j & 1) print_right(lbl, kx - kr - 2, ky - 2, lblcol);   // right-column knob → label on its left
        else       print(lbl,       kx + kr + 2, ky - 2, lblcol);   // left-column knob  → label on its right
    }
    font(FONT_NORMAL);
    circfill(x + 7, PED_Y + 63, 2, sl->on ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    rrectfill(x + 12, PED_Y + 58, PED_W - 20, 12, 2, CLR_BROWNISH_BLACK);
    rrect(x + 12, PED_Y + 58, PED_W - 20, 12, 2, CLR_DARK_GREY);
    if (locked) { font(FONT_TINY); print_centered("LOCK", x + 12 + (PED_W - 20) / 2, PED_Y + 61, CLR_DARK_GREY); font(FONT_NORMAL); }
}

// a small palette chip / drag-ghost: the icon + a name, no knobs. Any pedal can be added freely now —
// the LO-FI/component conflict is handled at the on-switch (pedal_locked), not the palette.
static void draw_chip(int cat, int x, int y, int w, int h, bool ghost) {
    const FxDef *d = &CAT[cat];
    rrectfill(x, y, w, h, 3, d->body);
    rrect(x, y, w, h, 3, ghost ? CLR_WHITE : d->accent);
    if (d->kind == -1)      lofi_icon(x + w / 2, y + 9, d->accent);
    else if (d->kind == -2) fuzz_icon(x + w / 2, y + 9, d->accent);
    else if (d->kind == -3) shimmer_icon(x + w / 2, y + 9, d->accent);
    else if (d->kind == FX_DRIVE) od_icon(x + w / 2, y + 9, d->accent);
    else fx_icon(d->kind, x + w / 2, y + 9, d->accent, d->body);
    font(FONT_TINY); print_centered(d->name, x + w / 2, y + h - 6, CLR_WHITE); font(FONT_NORMAL);
}

static void draw_palette(void) {
    rectfill(0, PAL_Y, SCREEN_W, SCREEN_H - PAL_Y, CLR_BROWNISH_BLACK);
    line(0, PAL_Y, SCREEN_W, PAL_Y, CLR_DARK_GREY);
    font(FONT_TINY);
    print_centered("drag a pedal UP into the chain  ·  drag a chain pedal DOWN here to remove", SCREEN_W / 2, PAL_Y + 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    int avail[NCAT], na = pal_avail(avail);
    for (int a = 0; a < na; a++) { int cx2, cy2; pal_chip_rect(a, &cx2, &cy2); draw_chip(avail[a], cx2, cy2, PAL_CW, PAL_CH, false); }
}

// the RIG panel: tap a "legendary setup" to load the whole board (pedals + cabinet) at once.
static void draw_rigs(void) {
    rectfill(0, PAL_Y, SCREEN_W, SCREEN_H - PAL_Y, CLR_BROWNISH_BLACK);
    line(0, PAL_Y, SCREEN_W, PAL_Y, CLR_DARK_GREY);
    font(FONT_TINY);
    print_centered("RIGS — tap a setup to load the whole board (pedals + cabinet), then tweak", SCREEN_W / 2, PAL_Y + 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    for (int r = 0; r < NRIG; r++) {
        int rx, ry; rig_rect(r, &rx, &ry);
        const Rig *g = &RIG[r];
        int accent = (g->tenant == CAB_AMP) ? AMP_VC[g->voicing].col : CLR_LIGHT_GREY;
        rrectfill(rx, ry, RIG_W, RIG_H, 3, CLR_DARK_BROWN);
        rrect(rx, ry, RIG_W, RIG_H, 3, accent);
        print(g->name, rx + 6, ry + 4, CLR_WHITE);
        font(FONT_TINY); print(g->sub, rx + 6, ry + 16, CLR_LIGHT_PEACH); font(FONT_NORMAL);
    }
}

static void draw_guitar(void) {
    int by = STR_Y0 - 4, bh = (STR_Y(NSTR - 1) + 8) - by;   // clears the scrollbar above; the neck is taller now
    rrectfill(6, by, SCREEN_W - 12, bh, 6, CLR_BLUE_GREEN);
    rrect(6, by, SCREEN_W - 12, bh, 6, CLR_BLUE);
    rrectfill(SX0 - 8, by + 3, SX1 - SX0 + 28, bh - 6, 4, CLR_LIGHT_PEACH);
    rectfill(STRUMX, by + 3, SX1 - STRUMX + 4, bh - 6, CLR_PEACH);
    rectfill(SX0 + 60, by + 3, 5, bh - 6, CLR_DARKER_GREY);
    rectfill(STRUMX - 8, by + 3, 5, bh - 6, CLR_DARKER_GREY);
    rectfill(SX0 - 4, by + 2, 3, bh - 4, CLR_MEDIUM_GREY);
    for (int s = 0; s < NSTR; s++) {
        int y = STR_Y(s);
        amp[s] *= 0.93f; vib_ph[s] += 0.6f;
        int col = amp[s] > 0.5f ? CLR_LIGHT_YELLOW : amp[s] > 0.1f ? CLR_DARK_ORANGE : CLR_MEDIUM_GREY;
        int px = SX0, py = y;
        for (int xx = SX0 + 8; xx <= SX1; xx += 8) {
            float t  = (float)(xx - SX0) / (float)(SX1 - SX0);
            int   wy = y + (int)(amp[s] * 4.0f * sinf(t * 9.42f + vib_ph[s]) * sinf(t * 3.14f));
            line(px, py, xx, wy, col); px = xx; py = wy;
        }
    }
    for (int s = 0; s < NSTR; s++) {
        int f = str_fret(s), y = STR_Y(s);
        if (f == 0) { circ(SX0 + 2, y, 2, CLR_DARK_RED); }
        else { int dx = SX0 + 6 + f * FRET_W; if (dx > STRUMX - 16) dx = STRUMX - 16; circfill(dx, y, 2, CLR_DARK_RED); pset(dx - 1, y - 1, CLR_PEACH); }
    }
    font(FONT_TINY); print_centered("STRUM", (STRUMX + SX1) / 2, by + bh - 7, CLR_DARK_BROWN); font(FONT_NORMAL);
    for (int j = 0; j < PTR_MAX; j++)
        if (ptr[j].id != PTR_NONE && ptr[j].mode == PTR_PICK)
            trifill(ptr[j].x - 3, ptr[j].y - 4, ptr[j].x + 3, ptr[j].y - 4, ptr[j].x, ptr[j].y + 4, CLR_WHITE);
    for (int i = 0; i < NSHAPE; i++) {
        int x = SHAPE_X(i); bool on = (i == sel_shape);
        rrectfill(x, SHAPE_Y, SHAPE_W, CHORD_H, 3, on ? CLR_ORANGE : CLR_DARKER_GREY);
        rrect(x, SHAPE_Y, SHAPE_W, CHORD_H, 3, on ? CLR_WHITE : CLR_DARK_GREY);
        print_centered(SHAPE_NAME[i], x + SHAPE_W / 2, SHAPE_Y + 8, on ? CLR_BLACK : CLR_MEDIUM_GREY);
        font(FONT_TINY); print(str("%c", SHAPE_KEY[i]), x + 3, SHAPE_Y + 2, on ? CLR_BLACK : CLR_DARK_GREY); font(FONT_NORMAL);
    }
    for (int i = 0; i < NROOT; i++) {
        int x = ROOT_X(i); bool on = (i == sel_root);
        rrectfill(x, ROOT_Y, ROOT_W, CHORD_H, 3, on ? CLR_LIME_GREEN : CLR_DARKER_GREY);
        rrect(x, ROOT_Y, ROOT_W, CHORD_H, 3, on ? CLR_WHITE : CLR_DARK_GREY);
        print_centered(ROOT_NAME[i], x + ROOT_W / 2, ROOT_Y + 8, on ? CLR_BLACK : CLR_MEDIUM_GREY);
        font(FONT_TINY); print(str("%c", ROOT_KEY[i]), x + 3, ROOT_Y + 2, on ? CLR_BLACK : CLR_DARK_GREY); font(FONT_NORMAL);
    }
}

// the pinned CABINET box at the far right (never scrolls): the chain plugs into it. Tenant =
// none / guitar amp (a voicing) / Leslie; header taps cycle it, the selector row steps, two knobs.
static void draw_cabinet(void) {
    int x = CAB_X, cx = x + CAB_W / 2;
    bool amp = (cab_tenant == CAB_AMP), none = (cab_tenant == CAB_NONE);
    int accent = amp ? AMP_VC[cab_voicing].col : none ? CLR_DARKER_GREY : CLR_LIGHT_GREY;
    print(">", VIEW_R + 1, PED_Y + 33, CLR_DARK_GREY);                              // "...into the amp"
    rrectfill(x, PED_Y, CAB_W, PED_H, 4, none ? CLR_BROWNISH_BLACK : CLR_DARK_BROWN);
    rrect(x, PED_Y, CAB_W, PED_H, 4, accent);
    font(FONT_SMALL); print_centered("CABINET", cx, PED_Y + 2, none ? CLR_MEDIUM_GREY : CLR_WHITE); font(FONT_NORMAL);
    if (none) {
        font(FONT_TINY); print_centered("(off)", cx, PED_Y + 17, CLR_DARK_GREY);
        print_centered("tap", cx, PED_Y + 28, CLR_DARKER_GREY); font(FONT_NORMAL);
        for (int gy = PED_Y + 40; gy < PED_Y + 64; gy += 3) line(x + 6, gy, x + CAB_W - 6, gy, CLR_DARKER_GREY);  // dim grille
        return;
    }
    font(FONT_SMALL);                                                               // selector readout
    print_centered(amp ? AMP_VC[cab_voicing].name
                       : cab_speed == LESLIE_STOP ? "STOP" : cab_speed == LESLIE_SLOW ? "SLOW" : "FAST",
                   cx, PED_Y + 15, accent);
    font(FONT_NORMAL);
    int ky = PED_Y + 46, kx0 = x + 15, kx1 = x + CAB_W - 15;
    const char *l0 = amp ? "GAIN" : "DRV", *l1 = amp ? "SAG" : "BAL";
    for (int j = 0; j < 2; j++) {
        int kx = j ? kx1 : kx0;
        circfill(kx, ky, 6, CLR_BROWNISH_BLACK); circ(kx, ky, 6, accent);
        float a = (-135.0f + cab_k[j] * 270.0f) * 0.0174533f;
        line(kx, ky, kx + (int)(sinf(a) * 5), ky - (int)(cosf(a) * 5), CLR_WHITE);
        font(FONT_TINY); print_centered(j ? l1 : l0, kx, ky + 8, CLR_LIGHT_PEACH); font(FONT_NORMAL);
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // top bar — PEDALS palette (left), RIGS (next), AUTO (right)
    rrectfill(4, 2, 56, 11, 2, palette_open ? CLR_INDIGO : CLR_DARKER_GREY);
    rrect(4, 2, 56, 11, 2, palette_open ? CLR_WHITE : CLR_DARK_GREY);
    rrectfill(64, 2, 46, 11, 2, rig_open ? CLR_ORANGE : CLR_DARKER_GREY);
    rrect(64, 2, 46, 11, 2, rig_open ? CLR_WHITE : CLR_DARK_GREY);
    font(FONT_SMALL);
    print(palette_open ? "x CLOSE" : "= PEDALS", 9, 4, CLR_WHITE);
    print(rig_open ? "x RIGS" : "RIGS", 70, 4, rig_open ? CLR_WHITE : CLR_LIGHT_PEACH);
    print_right(autoplay ? "AUTO: on" : "AUTO: off", SCREEN_W - 6, 5, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // a chain pedal being dragged is LIFTED out of the row (the rest close up), so the caret lines
    // up with where it will actually land
    int lift = -1;
    for (int j = 0; j < PTR_MAX; j++) if (ptr[j].id != PTR_NONE && ptr[j].mode == PTR_DRAGSLOT) { lift = chain_index(ptr[j].cat); break; }
    int shown = chain_n - (lift >= 0 ? 1 : 0);

    // the chain (clipped to its viewport so half-scrolled pedals don't bleed over the bars)
    clip(CHAIN_X0, PED_Y, VIEW_W, PED_H);
    int disp = 0;
    for (int i = 0; i < chain_n; i++) {
        if (i == lift) continue;
        int x = CHAIN_X0 + disp * PITCH - (int)scroll_x;
        if (!(x > VIEW_R || x + PED_W < CHAIN_X0)) {
            draw_chain_pedal(i, x);
            if (disp < shown - 1) print(">", x + PED_W, PED_Y + 33, CLR_DARK_GREY);
        }
        disp++;
    }
    clip(0, 0, 0, 0);

    draw_cabinet();   // the pinned output cabinet (drawn unclipped, never scrolls)

    // drop caret — where the dragged pedal would land if released now (only while over the chain).
    // Drawn unclipped, x nudged inside the viewport, so the first/last gap is fully visible.
    for (int j = 0; j < PTR_MAX; j++) {
        Ptr *q = &ptr[j];
        if (q->id == PTR_NONE || (q->mode != PTR_DRAGSLOT && q->mode != PTR_DRAGPAL) || q->y >= PED_Y + PED_H) continue;
        int gx = CHAIN_X0 + drop_index(q) * PITCH - (int)scroll_x;
        if (gx < CHAIN_X0 + 2) gx = CHAIN_X0 + 2; if (gx > VIEW_R - 2) gx = VIEW_R - 2;
        rectfill(gx - 1, PED_Y, 3, PED_H, CLR_LIME_GREEN);
        trifill(gx - 3, PED_Y, gx + 3, PED_Y, gx, PED_Y + 5, CLR_LIME_GREEN);
        trifill(gx - 3, PED_Y + PED_H, gx + 3, PED_Y + PED_H, gx, PED_Y + PED_H - 5, CLR_LIME_GREEN);
    }
    if (chain_n == 0) { font(FONT_SMALL); print_centered("open PEDALS, drag effects in →", SCREEN_W / 2, PED_Y + 32, CLR_DARK_GREY); font(FONT_NORMAL); }

    // scrollbar (only on overflow)
    if (max_scroll() > 0) {
        rectfill(CHAIN_X0, SB_Y + 1, VIEW_W, 3, CLR_DARKER_GREY);
        int tw = VIEW_W * VIEW_W / content_w();
        int tx = CHAIN_X0 + (int)(scroll_x / max_scroll() * (VIEW_W - tw));
        rectfill(tx, SB_Y + 1, tw, 3, CLR_LIGHT_GREY);
    }

    if (palette_open)  draw_palette();
    else if (rig_open) draw_rigs();
    else               draw_guitar();

    // drag ghost (on top of everything)
    for (int j = 0; j < PTR_MAX; j++)
        if (ptr[j].id != PTR_NONE && (ptr[j].mode == PTR_DRAGSLOT || ptr[j].mode == PTR_DRAGPAL))
            draw_chip(ptr[j].cat, ptr[j].x - 22, ptr[j].y - 13, 44, 26, true);
}
