/* de:meta
{
  "slug": "pedalboard",
  "title": "pedalboard",
  "resizable": true,
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
  "description": "An electric guitar you PLAY through a CHAIN of stompboxes you BUILD - the showcase for fx_order(): the order pedals sit in the chain is the order the engine runs them, so moving a pedal actually changes the tone (bitcrush BEFORE vs AFTER eq sounds different). Tap '= PEDALS' (top-left) to open the palette - a tray of 9 effects drawn as little icon+name chips (BITCRUSH, EQ, CHORUS, PHASER, FLANGER, TAPE, TREMOLO, WAH, REVERB). Drag a chip UP into the chain to add it. A pedal's LABEL STRIP (the grip dots) is its handle: drag it sideways to reorder, or DOWN out of the rack to remove. Dragging a pedal's BODY pans the chain sideways instead (the mouse wheel does it too), so a downward swipe over the rack never grabs a pedal by accident and never steals a strum aimed at the strings; a thin position bar sits along the TOP of the rack. Each pedal has its real knob row (drag to dial) and footswitch (tap, or 1-9 by position). Below: a real six-string guitar (INSTR_GUITAR) - pick a chord on the ROOT (Z X C V B N M) + SHAPE (A S D F G) rows, then sweep the strings to strum (SPACE strums; the AUTO button top-right cycles the self-player off / STRUM / TRAVIS, boots on STRUM, and stops the moment you play; TRAVIS is Merle Travis fingerpicking - a metronomic alternating thumb bass under syncopated treble notes, which on the '5' power-chord shape goes sparse because its treble strings are damped). Mouse and touch both work, every finger its own pointer. (REVERB and DELAY are real dry/wet INSERTS (reverb_insert/echo_insert), so their chain position is audible - crush the wet tail or reverb the crushed guitar.) The OD pedal's VOICE knob picks a famous dirt box via drive_voice() - RAW / Tube Screamer (mid hump) / RAT (hard clip + filter) / Big Muff (fuzz + scoop) - with TONE riding that voice."
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
// The chain can outgrow the screen — pan it by dragging a pedal BODY sideways (or the wheel); a
// thin position bar sits along the top of the rack. (REVERB is now a real
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
//                    on the neck to pick one; SPACE strums. AUTOPLAY is the AUTO button top-right,
//                    a 3-way that NAMES its state: off / strum / travis. It boots on STRUM, and any
//                    chord key or strum switches it off — there is no key that turns it back on.
//                    (The docs used to claim "M-row toggles autoplay"; M is the D root, and pressing
//                    it only ever DISABLES autoplay via set_root.)
//                    TRAVIS = the fingerstyle: thumb alternating root/fifth on the beat, fingers
//                    between. Sparse on the "5" shape, whose treble strings are damped.
//
// Mouse + touch both work — every contact is its own pointer. The mouse is merged in explicitly.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include "fxicons.h"      // shared effect icons + colours (also used by the epiano)
#include "ampcab.h"       // the shared amp/cab voicing table — the CABINET slot's "guitar amp" tenant
#include <math.h>
extern void de_resize(int w, int h);   // engine seam: set the active canvas (acidcandy's chunky-canvas trick)

// DEVICE REFLOW (device-adaptive-layout.md) — LANDSCAPE-only, authored at 320x200. We never scale
// the render (that desyncs ui.h/tapp; see CLAUDE.md); instead de_resize() to a small canvas that
// MATCHES the window's aspect ratio so the design scales up crisp with NO letterbox bars, then the
// layout below reads screen_w()/screen_h() and spreads into the leftover. Base 320x200 (ratio 1.6):
// a wider window locks HEIGHT 200 + widens (more of the pedal chain shows); a narrower/taller window
// (e.g. iPad 4:3) locks WIDTH 320 + grows the guitar downward. At exactly 320x200 it's a no-op.
// The SAFE-AREA layout frame: the whole layout lives inside (saX,saY,saW,saH) so controls dodge the
// notch / Dynamic Island / home-bar, while the background (cls) bleeds to the full canvas. On desktop
// safe_rect == the whole canvas, so saX/saY = 0 and saW/saH = screen_w()/h() → the base is unchanged.
static int saX, saY, saW, saH;
static void fit_canvas(void) {
    int cw = screen_w(), ch = screen_h();
    if (cw <= 0 || ch <= 0) return;
    float r = (float)cw / (float)ch;
    int tw, th;
    if (r >= 1.6f) { th = 200; tw = (int)(200.0f * r + 0.5f); }   // wide  → lock height, widen the board
    else           { tw = 320; th = (int)(320.0f / r + 0.5f); }   // narrow→ lock width, grow downward
    if (tw != cw || th != ch) de_resize(tw, th);
    safe_rect(&saX, &saY, &saW, &saH);   // read AFTER the resize (it rescales to the reflowed canvas)
}

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
    { "BITCRUSH", CLR_DARK_BROWN,    CLR_DARK_ORANGE,  FX_CRUSH,   3, { "BIT","RTE","MIX" },   { 0.50f, 0.60f, 0.60f } },
    { "EQ",       CLR_DARKER_BLUE,   CLR_BLUE,         FX_EQ,      3, { "LO","MID","HI" },     { 0.50f, 0.50f, 0.50f } },
    { "CHORUS",   CLR_DARKER_PURPLE, CLR_PINK,         FX_CHORUS,  3, { "RTE","DEP","MIX" },   { 0.30f, 0.28f, 0.45f } },
    { "PHASER",   CLR_DARK_GREEN,    CLR_LIME_GREEN,   FX_PHASER,  4, { "RTE","DEP","FB","MX" },{ 0.30f, 0.70f, 0.65f, 0.55f } },
    { "FLANGER",  CLR_BLUE_GREEN,    CLR_MEDIUM_GREEN, FX_FLANGER, 4, { "RT","DP","FB","MX" }, { 0.20f, 0.70f, 0.60f, 0.50f } },
    { "TAPE",     CLR_DARK_RED,      CLR_PEACH,        FX_TAPE,    3, { "WOW","FLT","SAT" },   { 0.35f, 0.25f, 0.45f } },
    { "TREMOLO",  CLR_DARKER_GREY,   CLR_LIGHT_YELLOW, FX_TREM,    3, { "SPD","DEP","WAV" },   { 0.45f, 0.60f, 0.0f } },
    { "WAH",      CLR_DARK_PURPLE,   CLR_MAUVE,        FX_WAH,     4, { "SNS","RES","MIX","MOD" },{ 0.50f, 0.55f, 0.70f, 0.0f } },
    { "REVERB",   CLR_DARK_BLUE,     CLR_INDIGO,       FX_REVERB,  3, { "SIZ","DMP","MIX" },   { 0.70f, 0.40f, 0.45f } },
    { "VOWEL",    CLR_BROWN,         CLR_LIGHT_PEACH,  FX_FORMANT, 4, { "VWL","Q","MIX","MOD" },{ 0.50f, 0.60f, 0.90f, 0.0f } },
    { "AUTOPAN",  CLR_DARK_GREY,     CLR_LIGHT_YELLOW, FX_PAN,     3, { "SPD","DEP","WAV" },   { 0.45f, 0.85f, 0.0f } },
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

// ── the output stage as ONE FLAT LIST: OFF · the five amps · ROTARY ─────────────────────────────
// It used to be TWO STACKED HIDDEN CYCLES — tapping the header cycled NONE→AMP→LESLIE and tapping
// the row below stepped within that kind — with nothing on screen saying either cycle existed. A
// player hit exactly that: "clicking Cabinet adds amp sims? … clicking Cabinet again goes to
// modulation? It's surprising / counterintuitive that this cycles through these modes." Third time
// this repo has paid for a hidden cycle (acidcandy's PICK/PLAY/MUTE/REC pads and its hold-to-open
// FX hub both cost a session), so: one flat list, arrows you can see, a pip per entry.
//
// ROTARY sits among the amps rather than beside them because a Leslie 122 CONTAINS its own power
// amp — you plug into it INSTEAD of a Marshall. It is a peer output stage, not an effect. (It also
// cannot be a chain pedal: sound.h pins leslie_process last, "cabinet output stage, not a
// reorderable pedal", and a pedal that silently always sits last would be a lie in the one cart
// whose whole premise is that chain order is audible.)
//
// tenant+voicing stay the INTERNAL representation — apply_fx and the RIG table speak that language
// and are untouched. The flat index is purely the UI's view of them.
#define CAB_SEL_N (AMPCAB_N + 2)         // OFF + 5 amps + ROTARY = 7
static int cab_sel(void) {
    return cab_tenant == CAB_NONE ? 0 : cab_tenant == CAB_AMP ? cab_voicing + 1 : AMPCAB_N + 1;
}
static const char *cab_sel_name(int i) {
    return i == 0 ? "OFF" : i <= AMPCAB_N ? AMP_VC[i - 1].name : "ROTARY";
}
static float cab_k[2]    = { 0.5f, 0.5f }; // amp: GAIN, SAG  ·  leslie: DRIVE, BALANCE
static const float CAB_KDEF[2] = { 0.5f, 0.5f };   // …their double-click-to-default targets
// DOUBLE-TAP TO DEFAULT. Semantics lifted from acidcandy's _knobx (tools/carts/acidcandy.c), which
// already solved this: the reset fires on RELEASE, and only if the press was a genuine TAP — the
// value barely moved AND the press was short. The first cut here fired on PRESS with no such test,
// which resets the knob whenever you grab it twice in quick succession to nudge it twice: a normal
// thing to do, and it throws away the value you were dialling in.
// Keyed by pedal CATEGORY, not slot index — slots reorder when you drag a pedal, and an index match
// would reset whichever pedal slid under your finger. cat KM_CAB = a cabinet knob (outside the chain).
//
// The reset TARGET is CAT[].kdef[] / CAB_KDEF[], i.e. literally the same array chain_insert seeds a
// new pedal from. acidcandy's todo records the bug that avoids: it passes `def` as a separate
// argument per call site, and one went stale when the real default moved (0.9 vs 0.7) so double-tap
// silently reset to a value the knob never had. Sourcing the reset from the initialiser makes that
// unrepresentable here.
// GEAR DRAG, ported from acidcandy's _knobx: vertical = value, but PULL SIDEWAYS to shift into a
// finer gear (further out = finer), so one gesture gives both coarse and precise. Base step stays
// pedalboard's own 0.012/px; the gear only ever DIVIDES it. A gear change never jumps the value
// because this drag is incremental (delta from prevY) rather than anchored to the grab point.
#define KNOB_STEP    0.012f   // value per canvas px at 1x (a full 0..1 sweep = ~83px)
#define KNOB_GEAR    22.0f    // sideways px per +1x of fine gear
#define KNOB_GEARMAX 2.5f     // cap, so FINE still covers real ground
// which knob a finger is on RIGHT NOW + whether it is in fine gear — draw() reads these, because a
// mode with no visible sign is not a control (the rule acidcandy's FX-hub entry paid for twice).
static int  drag_cat = -99, drag_knob = -1;
static bool drag_fine = false;
static float knob_gear(int px, int cx) {           // horizontal distance from the knob → gear ratio
    int ox = px - cx; if (ox < 0) ox = -ox;
    float g = 1.0f + ox / KNOB_GEAR;
    return g > KNOB_GEARMAX ? KNOB_GEARMAX : g;
}

#define KM_N   8
#define KM_CAB (-2)
#define TAP_FRAMES 15                              // longer press than this = a drag, not a tap
#define DBL_FRAMES 22                              // ~0.36s between the two taps (comfortable on touch)
#define KM_MOVE    0.02f                           // moved more than this = a drag, not a tap
static int frame_no = 0;
static struct { int used, cat, knob, gf, ltf; float gval; } kmeta[KM_N];
static int kmeta_i(int cat, int knob) {
    for (int i = 0; i < KM_N; i++) if (kmeta[i].used && kmeta[i].cat == cat && kmeta[i].knob == knob) return i;
    for (int i = 0; i < KM_N; i++) if (!kmeta[i].used) {
        kmeta[i].used = 1; kmeta[i].cat = cat; kmeta[i].knob = knob;
        kmeta[i].gf = 0; kmeta[i].ltf = -1000; kmeta[i].gval = 0.0f;
        return i;
    }
    return 0;                                      // ring full (never: 4 knobs max on screen at once)
}
static void km_grab(int cat, int knob, float v) {  // ON PRESS: remember where and when we grabbed
    int i = kmeta_i(cat, knob); kmeta[i].gval = v; kmeta[i].gf = frame_no;
}
static void km_release(int cat, int knob, float *v, float def) {   // ON RELEASE: was it a tap? a double?
    int i = kmeta_i(cat, knob);
    float dv = *v - kmeta[i].gval; if (dv < 0) dv = -dv;
    if (dv > KM_MOVE || frame_no - kmeta[i].gf > TAP_FRAMES) return;   // a drag — not a tap at all
    if (frame_no - kmeta[i].ltf < DBL_FRAMES) { *v = def; kmeta[i].ltf = -1000; dirty = 1; }  // second tap → reset
    else kmeta[i].ltf = frame_no;                                      // first tap → arm
}

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
// SHAPE_F[shape][string] = frets ABOVE the barre, or FRET_MUTE for a string the fretting hand
// damps. MUTE IS NOT FRET 0 — see str_fret() below for the bug that cost us every power chord.
#define FRET_MUTE (-1)
static const int SHAPE_F[NSHAPE][NSTR] = {
    // ordered so the THIRD climbs left→right (none → ♭3 → ♮3 → 4): a musical gradient
    { 0, 2, 2, FRET_MUTE, FRET_MUTE, FRET_MUTE },   // 5    power — root/5th/octave, top three DAMPED
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
// Neck geometry. NFRETS is the highest the chord table can reach (root D = fret 10, + a shape
// offset of 2), and FRET_W is DERIVED so all of them always land clear of the strum zone. It used
// to be a fixed 7px with a `dx > STRUMX-16 → clamp` catch, which silently piled several dots onto
// the same x once the canvas dropped under ~250px wide (the cart is resizable). Deriving the pitch
// removes the clamp by construction and lets the drawn frets span the whole neck like a real one.
#define NFRETS   12
#define FRET_X0  (SX0 + 6)                                   // fret 0 = the nut
#define FRET_X1  (STRUMX - 16)                               // fret NFRETS, clear of the strum zone
#define FRET_W   (((FRET_X1 - FRET_X0) / NFRETS) < 2 ? 2 : (FRET_X1 - FRET_X0) / NFRETS)
#define FRET_WIRE(f) (FRET_X0 + (f) * FRET_W)                // x of fret f's WIRE
// …and where a FINGER goes: behind the wire, in the middle of the fret space. Putting the dot on
// FRET_WIRE(f) draws it sitting on the wire itself, which is not how anyone frets a note.
#define FRET_X(f) ((FRET_WIRE(f) + FRET_WIRE((f) - 1)) / 2 + 1)
// Boot on G MAJOR, not the E5 power chord it used to open with. A first strum should sound like a
// chord: maj rings all six strings (barre at fret 3 = G B D), where the "5" shape damps three of
// them, so the old default led with a bare fifth AND with half the neck muted. It also puts dots
// out at frets 3-5 instead of parking everything on the nut, so the fretboard shows what it does.
static int  sel_shape = 2;   // SHAPE_NAME[2] = "maj"
static int  sel_root  = 2;   // ROOT_NAME[2]  = "G"  (ROOT_FRET 3)
static int  str_midi[NSTR];

static float amp[NSTR];
static float vib_ph[NSTR];
static int   pend[NSTR];

// AUTOPLAY STYLES. Was a bool. TRAVIS is Merle Travis's fingerstyle: the thumb keeps a metronomic
// alternating bass on the low strings while the fingers pick syncopated notes BETWEEN those beats,
// so one hand sounds like two players. It lives here rather than as new DSP because the engine
// already does the hard part — schedule_hit fires SAMPLE-ACCURATELY inside the mixer loop (sound.h:
// block-edge firing "quantized away exactly the micro-timing that makes grooves feel played"), and
// Travis picking IS micro-timing: quantise thumb and fingers onto one grid and the independence
// that defines the technique collapses.
// It also demos a pedalboard better than strumming does: a constant bass gives delay/reverb
// something to chew on, and the offbeat trebles expose modulation a chord-every-4-beats cannot.
enum { AP_OFF, AP_STRUM, AP_TRAVIS };
static const char *AP_NAME[3] = { "off", "strum", "travis" };
static int   autoplay = AP_STRUM;
static int   cart_bpm = 100;        // mirrors the bpm() call in init — the API has no getter
static int   apos = 0;
static bool  guitar_in = false;   // GUITAR IN: route the live mic THROUGH the built chain (input_monitor)
static int   ap_gtr_in = -1;      // applied-state shadow — push input_monitor() only on a change (set-and-hold)

// ── geometry ──
// The top strip (bar + pedal chain) is FIXED-height and top-anchored; only WIDTH-dependent anchors
// (cabinet, chain viewport, guitar span, control rows) read screen_w()/screen_h(). Every macro below
// is exact at 320x200 (byte-identical base), and spreads into the leftover on other ratios.
// All positions are measured inside the safe frame: left origin saX, right edge saX+saW, top saY,
// bottom saY+saH. Each is exact at 320x200/full-safe (byte-identical base) and dodges the notch/home-bar.
#define PED_Y (saY + 14)
#define PED_H 70                     // a touch shorter (was 72) — trimmed padding feeds the neck
#define PED_W 54                     // a touch wider — room for the staggered knobs + side labels
#define PITCH 58                     // pedal size is FIXED → a wider canvas simply shows MORE of the chain
// Reorder GRAB HANDLE height. 12px was a comfortable mouse target and about a third of a fingertip;
// this runs it down to where the first knob row's hit box begins (knob_cy(0,·) - 7 = PED_Y + 17), so
// it is as tall as it can be without stealing a knob. Knobs are hit-tested BEFORE this anyway, so
// the overlap at the boundary resolves in the knob's favour, and the body below only pans — costing
// nothing if a press lands one pixel either side.
#define GRAB_H 17
#define CHAIN_X0 (saX + 4)
#define CAB_W   54                   // the pinned output-cabinet box (never scrolls) — "the chain plugs into it"
#define CAB_X   (saX + saW - CAB_W - 2)    // right-pinned to the safe-area edge (clears the notch)
#define VIEW_W  view_w()             // chain viewport width: fills from CHAIN_X0 up to the cabinet
#define VIEW_R  (CHAIN_X0 + view_w())
// (SB_Y retired: the scroll bar was a 6px GRAB TARGET here, directly in the strum's landing zone.
//  It is now a 2px indicator at PED_Y-2 and not a press target at all.)
#define ILLU_CY (PED_Y + 13)         // illustration center — pulled up, padding trimmed
#define PAL_Y   (saY + 88)           // palette panel top (when open); a taller canvas just fits more rows
#define SX0   (saX + 22)             // nut (neck end)
#define SX1   (saX + saW - 18)       // bridge (body end) — tracks the safe right edge (= 302 at 320w)
#define STRUMX (SX1 - 106)           // strum zone keeps its width, rides the bridge (= 196 at 320w)
#define STR_Y0 (saY + 93)            // strings spread WIDER now (10px, was 7) — easy to pick on a phone
#define STR_DY ((SHAPE_Y - STR_Y0 - 11) / (NSTR - 1))   // SPAN-based → fills down to the chord rows (= 10 at 200h)
// TAB ORDER: string 0 is low E but draws at the BOTTOM, high e (string 5) on top — the convention
// every guitarist reads (higher pitch, higher on the page). Drawing index 0 on top mirrors the neck
// and reads as a LEFT-handed guitar facing you. Only the RENDER + hit-test flip: the string arrays
// (OPEN/SHAPE_F/str_midi/pend) and the low→high strum order stay index-ordered.
#define STR_Y(s) (STR_Y0 + (NSTR - 1 - (s)) * STR_DY)
#define NECK_MID (((STR_Y(2)) + (STR_Y(3))) / 2)   // centre line, BETWEEN the two middle strings
#define STR_TOP  STR_Y(NSTR - 1)     // high e — the top string on screen
#define STR_BOT  STR_Y(0)            // low E  — the bottom string on screen
#define CHORD_H 21                   // chord buttons ~1.5× taller (was 14), parked at the bottom
#define SHAPE_Y (saY + saH - 46)     // the two chord rows sit at the BOTTOM (above the home-bar; grow with height)
#define SHAPE_W (56 * saW / 320)     // rows stretch to fill the safe width (exact 56 at 320w)
#define SHAPE_X(i) (saX + (12 + (i) * 60) * saW / 320)
#define ROOT_Y  (saY + saH - 23)
#define ROOT_W  (40 * saW / 320)
#define ROOT_X(i) (saX + (11 + (i) * 43) * saW / 320)
static int view_w(void) { return CAB_X - CHAIN_X0 - 6; }   // = 254 at 320w

// knobs are STAGGERED (zigzag, 2 columns) so each gets room to be bigger than a cramped row.
// even j → left column, odd j → right column; each on its own row down the pedal.
static int knob_cx(int x, int j)  { return (j & 1) ? (x + PED_W - 16) : (x + 16); }
static int knob_cy(int j, int nk) { return PED_Y + 24 + j * (nk <= 3 ? 13 : 9); }
static int knob_rad(int nk)       { return nk <= 3 ? 6 : 5; }
static int gate_ms(void) { return 1800; }

// ⚠ A DAMPED string is FRET_MUTE, and fret 0 is the OPEN string — they are not the same thing.
// This used to fold FRET_MUTE into 0, so the "5" shape's three damped strings sounded as open
// G/B/e under EVERY root: E5 came out as E minor (the open G is a ♭3), C5 as C major 7. A power
// chord is defined by having no third, and this one always had one. Reported from the wild
// 2026-07-30 as "the fret markers don't correspond to the chord voicings".
static int str_fret(int s) { int f = SHAPE_F[sel_shape][s]; return f < 0 ? FRET_MUTE : ROOT_FRET[sel_root] + f; }
static bool str_muted(int s) { return str_midi[s] < 0; }
static void build_strings(void) {
    for (int s = 0; s < NSTR; s++) { int f = str_fret(s); str_midi[s] = f < 0 ? -1 : OPEN[s] + f; }
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
static void cab_set_sel(int i) {                     // wraps both ways; the UI's only write path
    i = (i % CAB_SEL_N + CAB_SEL_N) % CAB_SEL_N;
    if      (i == 0)          cab_tenant = CAB_NONE;
    else if (i <= AMPCAB_N) { cab_tenant = CAB_AMP; cab_voicing = i - 1; }
    else                      cab_tenant = CAB_LESLIE;
    dirty = 1;
}
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
            // BIT and RTE are QUANTITIES, not amounts: knob UP = more bits / higher sample rate =
            // CLEANER. They used to run the other way (up = more destruction) while being labelled
            // with the physical quantity, so "RTE" turned up meant a LOWER sample rate — reported as
            // "if RTE is sample rate, I would intuit a higher knob position to be higher fidelity".
            // He is right, and that is also how real bitcrusher pedals are marked. MIX still goes
            // up = more effect, so the pedal as a whole keeps "turn it up for more". The knob labels
            // below show the live VALUE (16b/9b/2b, 44k/11k/3k) so the direction is self-evident.
            case C_BIT: crush(2.0f + k[0] * 14.0f, 16.0f - k[1] * 15.0f, act ? k[2] : 0.0f); break;
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
            // SPD is EXPONENTIAL and tops out at 9 Hz, unlike TREMOLO's linear 0.5+k*11.5 right
            // below. The two knobs looked interchangeable and were copy-pasted; they are not.
            // Tremolo wants 3-8 Hz (that IS tremolo) so linear-to-12 is right for it. Panning is
            // heard as POSITION only below ~2 Hz and as plain amplitude wobble above ~4 Hz, so the
            // same map buried autopan's entire useful range in the bottom ~13% of the knob and put
            // the DEFAULT at 4.5 Hz, already past it. Measured: at the old default, maxing DEP gave
            // a full hard-L-to-hard-R sweep (excursion 2.000) that a listener correctly described as
            // "slight changes in amplitude in both channels" — the pan was real, the RATE hid it.
            // 0.12·75^k → 0.12 Hz fully CCW · ~1 Hz centred · 9 Hz fully CW (audio-notes §27.3).
            case C_PAN: autopan(0.12f * powf(75.0f, k[0]), act ? k[1] : 0.0f, (int)(k[2] * 7.99f)); break;   // WAV picks any LFO_SHAPE_* (8)
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
enum { PTR_IDLE, PTR_KNOB, PTR_PICK, PTR_DRAGSLOT, PTR_DRAGPAL, PTR_SCROLL, PTR_CABKNOB, PTR_PAN };
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
    bpm(cart_bpm);
    // a tasteful starting chain (only TREMOLO ringing); the rest sit in the chain, off
    chain_insert(C_BIT, 0); chain[0].on = false;
    chain_insert(C_EQ, 1);  chain[1].on = false;
    chain_insert(C_CHO, 2); chain[2].on = false;
    chain_insert(C_TRM, 3);
    chain_insert(C_RVB, 4); chain[4].on = false;
    apply_fx();
    amp[0] = 0.8f; amp[2] = 1.0f; amp[4] = 0.6f;
}

// A damped string still gets HIT — you just hear the pick, not a note. That percussive "chk" is
// what a muted string sounds like on a real guitar, and it's also the honest feedback: silence
// would read as a dead widget. I_MUTE is the same engine at a 180ms gate, so it's a pick, not a pitch.
// …and the thud has a PITCH, which tracks the hand. A damped string is stopped by the BARRE, so
// the bit that still speaks runs from the barre fret to the bridge: a chk with your hand at the nut
// is lower and duller than the same chk at the 8th fret. This was pinned to OPEN[s] + 12 no matter
// where you were on the neck, so every chord's mute sounded identical — which is what the maker
// heard. The +12 stays (it keeps the thing reading as a click rather than a note); only the
// tracking is new. pick_string() already does the mirror of this for the NUT-side segment, so the
// cart was half modelling string position and half ignoring it.
static int mute_note(int s) { return OPEN[s] + ROOT_FRET[sel_root] + 12; }
#define MUTE_CHK(s) hit(mute_note(s), I_MUTE, 3, 60)
static void pluck_str(int s, int vol) {
    if (s < 0 || s >= NSTR) return;
    if (str_muted(s)) { MUTE_CHK(s); amp[s] = 0.3f; vib_ph[s] = 0.0f; fmt_on_attack(); return; }
    hit(str_midi[s], I_GTR, vol, gate_ms());
    amp[s] = 1.0f; vib_ph[s] = 0.0f;
    fmt_on_attack();                          // VOWEL pedal: advance/open the vowel on each pick
}
// Pluck ONE string at a future time. Extracted from strum_down so the Travis pattern below can
// reuse the mute handling and the pend[] vibration trigger instead of duplicating them.
// pend is in FRAMES, hence the ms·60/1000.
static void pluck_at(int s, int delay_ms, int vol, int dur_ms) {
    if (s < 0 || s >= NSTR) return;
    if (str_muted(s)) schedule_hit(delay_ms, mute_note(s), I_MUTE, 3, 60);   // the pick, not the note
    else              schedule_hit(delay_ms, str_midi[s], I_GTR, vol, dur_ms);
    pend[s] = 1 + (delay_ms * 60) / 1000;
}
static void strum_down(void) {
    for (int s = 0; s < NSTR; s++) pluck_at(s, s * 28, 5, gate_ms());
    fmt_on_attack();                          // one vowel advance per strum (a syllable per chord)
}

// ONE BEAT of Travis picking, as two eighths:
//   ON  the beat — the THUMB, alternating bass: string 0 (the root) on beats 1 and 3, string 1
//                  (the fifth) on 2 and 4. Shorter gate than a strummed note because a Travis thumb
//                  is palm-damped; it thumps, it does not ring through the next one.
//                  On beats 1 and 3 a treble is struck WITH it — the "pinch".
//   OFF the beat — a single treble note. This is the whole technique: the thumb never moves off the
//                  grid, and everything interesting happens between its notes.
// The bass alternation needs no chord knowledge: on any barre shape, strings 0 and 1 ARE root and
// fifth, so str_midi[] hands us the right two pitches at any position for free.
// On the "5" power-chord shape the three treble strings are damped, so the fingers come out as
// chks and the pattern goes sparse — which is honest, and what the maker asked for.
static void travis_beat(int b) {
    static const int PINCH  [4] = {  5, -1,  3, -1 };   // treble struck WITH the thumb, beats 1 and 3
    static const int OFFBEAT[4] = {  4,  5,  4,  3 };   // treble on the "&"
    int half = 30000 / cart_bpm;                        // ms per eighth (60000/bpm/2)
    pluck_at((b % 2) ? 1 : 0, 0, 6, 420);               // thumb: root · fifth · root · fifth
    if (PINCH[b] >= 0) pluck_at(PINCH[b], 0, 4, gate_ms());
    pluck_at(OFFBEAT[b], half, 5, gate_ms());
    fmt_on_attack();                          // one vowel advance per beat, not per note
}
static void set_shape(int sh) { sel_shape = sh; build_strings(); autoplay = AP_OFF; }
static void set_root(int r)   { sel_root  = r;  build_strings(); autoplay = AP_OFF; }
// screen y → string index (row 0 on screen is the HIGH e, so the row inverts back to an index)
static int  near_string(int ty) { int r = (ty - STR_Y0 + STR_DY / 2) / STR_DY; r = r < 0 ? 0 : r >= NSTR ? NSTR - 1 : r; return NSTR - 1 - r; }
// x of string s's fingered dot. Open (0) sits just past the nut; muted has no dot, but return the
// nut so any caller that ignores the mute still gets a sane x rather than a negative one.
static int dot_x(int s) {
    int f = str_fret(s);
    return f <= 0 ? SX0 + 2 : FRET_X(f);
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
#define PAL_COLS 6
#define PAL_CH   24    // shaved so all 22 effects (4 rows, empty chain) clear the bottom edge
#define PAL_PITCH ((saW - 8) / PAL_COLS)          // 6 chips spread to fill the safe width (= 52 at 320w)
#define PAL_CW   (PAL_PITCH - 2)                   // (= 50 at 320w)
static int pal_avail(int *out) { int n = 0; for (int c = 0; c < NCAT; c++) if (chain_index(c) < 0) out[n++] = c; return n; }
// grid starts just under the panel border (help text now lives up in the top bar, not here)
static void pal_chip_rect(int a, int *x, int *y) { *x = saX + 4 + (a % PAL_COLS) * PAL_PITCH; *y = PAL_Y + 3 + (a / PAL_COLS) * (PAL_CH + 2); }

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
    if (p->mode == PTR_KNOB && p->slot >= 0 && p->slot < chain_n) {          // tap-tap on a chain knob → default
        int cat = chain[p->slot].cat;
        km_release(cat, p->knob, &chain[p->slot].k[p->knob], CAT[cat].kdef[p->knob]);
    } else if (p->mode == PTR_CABKNOB && p->knob >= 0 && p->knob < 2) {      // …and on a cabinet knob
        km_release(KM_CAB, p->knob, &cab_k[p->knob], CAB_KDEF[p->knob]);
    }
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
    frame_no++;
    drag_cat = -99; drag_knob = -1; drag_fine = false;   // re-set below by whichever knob is held
    fit_canvas();   // reflow the canvas to the window ratio BEFORE any hit-testing (keeps tapp 1:1)
    for (int i = 0; i < NSHAPE; i++) if (keyp(SHAPE_KEY[i])) set_shape(i);
    for (int i = 0; i < NROOT;  i++) if (keyp(ROOT_KEY[i]))  set_root(i);
    for (int i = 0; i < chain_n; i++) if (keyp('1' + i) && (chain[i].on || !pedal_locked(chain[i].cat))) { chain[i].on = !chain[i].on; dirty = 1; }
    if (keyp(KEY_SPACE)) { strum_down(); autoplay = AP_OFF; }

    if (tapp(saX + 4, saY + 2, 56, 11))   { palette_open = !palette_open; if (palette_open) rig_open = false; }
    if (tapp(saX + 64, saY + 2, 46, 11))  { rig_open = !rig_open; if (rig_open) palette_open = false; }
    if (tapp(saX + 114, saY + 2, 54, 11)) { guitar_in = !guitar_in; if (guitar_in) mic_start(); else mic_stop(); }
    if (tapp(saX + saW - 70, saY + 4, 66, 10)) autoplay = (autoplay + 1) % 3;   // off → strum → travis, and it SAYS which

    // GUITAR IN — feed the live mic through the chain you built. Set-and-hold: push only on change.
    // Gain is deliberately SUB-UNITY (0.8): monitoring your own mic through the device speaker feeds
    // back (mic hears the speaker); <1 makes the loop DECAY instead of howl. Headphones kill it fully.
    if ((int)guitar_in != ap_gtr_in) { input_monitor(guitar_in ? 0.8f : 0.0f); ap_gtr_in = guitar_in; }

    bool overflow = max_scroll() > 0;

    // WHEEL SCROLL. The chain scrolls sideways, so a trackpad two-finger swipe (or a tilt/second
    // wheel) is the obvious gesture and until now the ONLY way to pan was dragging the little
    // scrollbar — reported from the wild as "doesn't seem to accept horizontal scroll inputs".
    // Read BOTH axes and take whichever moved: a plain single-wheel mouse emits no x at all, and on
    // a horizontally-scrolling panel "wheel down" unambiguously means "further along the chain",
    // so falling back to y makes the ordinary mouse work instead of doing nothing.
    if (overflow) {
        float wx = mouse_wheel_x(), wy = mouse_wheel();
        float w  = wx != 0.0f ? wx : -wy;          // wheel DOWN (negative) → move right along the chain
        if (w != 0.0f) { scroll_x += w * 16.0f; clamp_scroll(); }
    }

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
                    else if (hitk >= 0) {
                        km_grab(chain[s].cat, hitk, chain[s].k[hitk]);
                        p->mode = PTR_KNOB; p->slot = s; p->knob = hitk;
                    }
                    // REORDER is now the HEADER STRIP only; the body PANS the chain sideways.
                    // Before, the whole pedal body was the reorder handle AND the only touch scroll
                    // was a 6px bar at PED_Y+PED_H — which is exactly where a downward strum starts.
                    // So a strum aimed at the strings either grabbed the scrollbar or picked up a
                    // pedal. Splitting them fixes both: a vertical swipe on a body is a pan with
                    // ~zero horizontal delta, i.e. harmless, and the strum band is no longer stolen.
                    // It also makes the cart's own description true ("drag a pedal by its LABEL").
                    else if (ty < PED_Y + GRAB_H) { p->mode = PTR_DRAGSLOT; p->slot = s; p->cat = chain[s].cat; }
                    else                          { p->mode = PTR_PAN; }
                }
            }
            // 1b. the pinned CABINET box (right of the chain): header taps cycle the tenant, the
            // selector row steps the voicing/speed, the two knobs drag.
            if (p->mode == PTR_IDLE && ty >= PED_Y && ty < PED_Y + PED_H && tx >= CAB_X) {
                if (ty < PED_Y + 11) { /* header is a LABEL now, not a control */ }
                else if (ty < PED_Y + 30) {                                                       // the one selector
                    if (tx < CAB_X + 14) cab_set_sel(cab_sel() - 1);                              // ‹ prev
                    else                 cab_set_sel(cab_sel() + 1);                              // › next (also the wide middle)
                }
                else if (cab_tenant == CAB_LESLIE && ty >= PED_Y + 58) {                          // STOP/SLOW/FAST
                    int z = (tx - CAB_X) * 3 / CAB_W; if (z < 0) z = 0; if (z > 2) z = 2;
                    cab_speed = z; dirty = 1;                                                     // = LESLIE_STOP/SLOW/FAST
                }
                else if (cab_tenant != CAB_NONE) {
                    if (point_in_box(tx, ty, CAB_X + 4, PED_Y + 34, CAB_W / 2 - 4, 24)) {
                        km_grab(KM_CAB, 0, cab_k[0]);
                        p->mode = PTR_CABKNOB; p->knob = 0;
                    }
                    else if (point_in_box(tx, ty, CAB_X + CAB_W / 2, PED_Y + 34, CAB_W / 2 - 4, 24)) {
                        km_grab(KM_CAB, 1, cab_k[1]);
                        p->mode = PTR_CABKNOB; p->knob = 1;
                    }
                }
            }
            // 2. (the scroll bar is an INDICATOR now, not a target — it used to sit in the strum's
            //     way and got tested BEFORE the guitar, so it swallowed the gesture. Pan the chain
            //     by dragging a pedal body, or use the wheel.)
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
                if (p->mode == PTR_IDLE && ty >= STR_TOP - 9 && ty <= STR_BOT + 9 && tx >= SX0 - 8 && tx <= SX1 + 8) {
                    p->mode = PTR_PICK; autoplay = AP_OFF;
                    pick_string(near_string(ty), tx);
                    p->prevY = ty;
                }
            }
        } else if (p->mode == PTR_KNOB) {
            if (p->slot < chain_n) {
                int   kcx  = knob_cx(ped_screen_x(p->slot), p->knob);
                float gear = knob_gear(tx, kcx);
                float *kv  = &chain[p->slot].k[p->knob];
                float nv   = clamp(*kv + (p->prevY - ty) * KNOB_STEP / gear, 0.0f, 1.0f);
                if (nv != *kv) { *kv = nv; dirty = 1; }   // only on a REAL change: dirty every frame
                drag_cat = chain[p->slot].cat;            // re-ran apply_fx 60x/s while merely HOLDING
                drag_knob = p->knob; drag_fine = gear > 1.5f;   // a knob (the set-and-hold stutter hazard)
            }
            p->prevY = ty;
        } else if (p->mode == PTR_CABKNOB) {
            int   kcx  = CAB_X + (p->knob ? CAB_W * 3 / 4 : CAB_W / 4);
            float gear = knob_gear(tx, kcx);
            float nv   = clamp(cab_k[p->knob] + (p->prevY - ty) * KNOB_STEP / gear, 0.0f, 1.0f);
            if (nv != cab_k[p->knob]) { cab_k[p->knob] = nv; dirty = 1; }
            drag_cat = KM_CAB; drag_knob = p->knob; drag_fine = gear > 1.5f;
            p->prevY = ty;
        } else if (p->mode == PTR_PICK) {
            for (int s = 0; s < NSTR; s++) { int ys = STR_Y(s); if ((p->prevY < ys && ty >= ys) || (p->prevY > ys && ty <= ys)) pick_string(s, tx); }
            p->prevY = ty;
        } else if (p->mode == PTR_SCROLL) {
            scroll_x += (tx - p->x) * (content_w() / (float)VIEW_W); clamp_scroll();
        } else if (p->mode == PTR_PAN) {
            scroll_x -= (tx - p->x); clamp_scroll();     // 1:1, content follows the finger
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
        static const int prog[8] = { 0, 2, 6, 3, 0, 5, 3, 2 };   // E G D A · E C A G
        // Autoplay walks the ROOTS and leaves the SHAPE alone. It used to force sel_shape = 0, so
        // hitting M yanked you to the power chord whatever you had picked — and left you there when
        // you switched it off. Now the progression plays in whatever you selected, which makes the
        // shape row worth touching while it runs: same changes as maj, min, sus4 or 7.
        int b = beat() % 4;
        if (b == 0) { sel_root = prog[apos % 8]; build_strings(); apos++; }   // one chord per bar
        if (autoplay == AP_STRUM) { if (b == 0) strum_down(); }               // …strummed once
        else                        travis_beat(b);                          // …or picked all four beats
    }
    for (int s = 0; s < NSTR; s++) if (pend[s] > 0 && --pend[s] == 0) { amp[s] = str_muted(s) ? 0.3f : 1.0f; vib_ph[s] = 0.0f; }   // a damped string barely moves

#ifdef DE_TRACE
    watch("chain_n", "%d", chain_n); watch("pal", "%d", palette_open);
    watch("cab", "%d", cab_tenant); watch("voicing", "%d", cab_voicing);
    watch("sel", "%d", cab_sel()); watch("spd", "%d", cab_speed);   // the flat amp index + rotary speed
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

// OD (FX_DRIVE)'s flat-topped clipped waveform now lives in the shared runtime/fxicons.h with every
// other FX_* glyph (promoted 2026-07-30 — it had been copy-pasted here and in pedalicon.c), so
// fx_icon() handles it like the rest and the special-case below is gone.

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
    for (int g = 0; g < 5; g++) {                                // grip dots down BOTH edges: the
        pset(x + 3,          PED_Y + 4 + g * 3, CLR_DARK_GREY);  // strip they bracket is the reorder
        pset(x + PED_W - 4,  PED_Y + 4 + g * 3, CLR_DARK_GREY);  // handle (drag it sideways)
    }
    if (d->kind == -1)      lofi_icon(cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY);
    else if (d->kind == -2) fuzz_icon(cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY);
    else if (d->kind == -3) shimmer_icon(cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY);
    else fx_icon(d->kind, cx, ILLU_CY, sl->on ? d->accent : CLR_DARKER_GREY, body);
    int kr = knob_rad(d->nk);
    int lblcol = sl->on ? CLR_LIGHT_PEACH : CLR_DARK_GREY;
    for (int j = 0; j < d->nk; j++) {
        int kx = knob_cx(x, j), ky = knob_cy(j, d->nk);
        circfill(kx, ky, kr, CLR_BROWNISH_BLACK);
        circ(kx, ky, kr, sl->on ? d->accent : CLR_DARK_GREY);
        bool turning = (drag_cat == sl->cat && drag_knob == j);   // drag_cat is -99/KM_CAB when idle, so never matches
        float a = (-135.0f + sl->k[j] * 270.0f) * 0.0174533f;
        int ptcol = (turning && drag_fine) ? CLR_ORANGE            // amber = fine gear, the only sign it is on
                  : sl->on ? CLR_WHITE : CLR_MEDIUM_GREY;
        line(kx, ky, kx + (int)(sinf(a) * (kr - 1)), ky - (int)(cosf(a) * (kr - 1)), ptcol);
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
        // TREMOLO / AUTOPAN WAV: 8 DISCRETE LFO shapes behind a continuous-looking knob, and until
        // now the only readout was the pointer angle — so you could be on SQUARE and have no way to
        // know. That is not cosmetic here: SINE sweeps (max pan step 0.27 per 40ms) while SQUARE
        // SNAPS hard L↔R (1.49) and S&H jumps to random spots (0.61). A listener hit exactly this
        // and reported autopan as "discrete, like in big steps — first no stereo pan, then suddenly
        // there is", which is a perfect description of SQUARE that nothing on screen explained.
        // (FX_FORMANT's MOD label above already carried a "like TREM's WAV" comment — TREM's WAV
        // never actually had one.)
        if (d->kind == FX_CRUSH && j <= 1) {          // show the VALUE: bit depth, and the actual kHz
            if (j == 0) lbl = str("%db", (int)(2.0f + sl->k[0] * 14.0f));
            else        lbl = str("%dk", (int)(44100.0f / (16.0f - sl->k[1] * 15.0f) / 1000.0f + 0.5f));
        }
        if ((d->kind == FX_TREM || d->kind == FX_PAN) && j == 2) {
            static const char *WN[8] = { "SINE","SQR","TRI","SAW","RAMP","OPT","S&H","RND" };   // = LFO_SHAPE_*
            lbl = WN[(int)(sl->k[2] * 7.99f)];
        }
        if (turning && lbl == d->klabel[j])                       // still the static name → show the number instead
            lbl = str("%d", (int)(sl->k[j] * 99.0f + 0.5f));
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
    else fx_icon(d->kind, x + w / 2, y + 9, d->accent, d->body);
    font(FONT_TINY); print_centered(d->name, x + w / 2, y + h - 6, CLR_WHITE); font(FONT_NORMAL);
}

static void draw_palette(void) {
    rectfill(0, PAL_Y, screen_w(), screen_h() - PAL_Y, CLR_BROWNISH_BLACK);
    line(0, PAL_Y, screen_w(), PAL_Y, CLR_DARK_GREY);
    int avail[NCAT], na = pal_avail(avail);
    for (int a = 0; a < na; a++) { int cx2, cy2; pal_chip_rect(a, &cx2, &cy2); draw_chip(avail[a], cx2, cy2, PAL_CW, PAL_CH, false); }
}

// the RIG panel: tap a "legendary setup" to load the whole board (pedals + cabinet) at once.
static void draw_rigs(void) {
    rectfill(0, PAL_Y, screen_w(), screen_h() - PAL_Y, CLR_BROWNISH_BLACK);
    line(0, PAL_Y, screen_w(), PAL_Y, CLR_DARK_GREY);
    font(FONT_TINY);
    print_centered("RIGS — tap a setup to load the whole board (pedals + cabinet), then tweak", saX + saW / 2, PAL_Y + 4, CLR_MEDIUM_GREY);
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
    int by = STR_TOP - 4, bh = (STR_BOT + 8) - by;   // the old scroll bar sat here; the neck owns it now
    rrectfill(saX + 6, by, saW - 12, bh, 6, CLR_BLUE_GREEN);
    rrect(saX + 6, by, saW - 12, bh, 6, CLR_BLUE);
    rrectfill(SX0 - 8, by + 3, SX1 - SX0 + 28, bh - 6, 4, CLR_LIGHT_PEACH);
    rectfill(STRUMX, by + 3, SX1 - STRUMX + 4, bh - 6, CLR_PEACH);
    rectfill(STRUMX - 8, by + 3, 5, bh - 6, CLR_DARKER_GREY);   // neck → strum-zone divider
    rectfill(SX0 - 4, by + 2, 3, bh - 4, CLR_MEDIUM_GREY);      // the NUT
    // FRET WIRES + position inlays. There used to be no frets at all: one fat decorative bar sat at
    // SX0+60 (fret 7.7 — not a fret), so the dots had nothing to line up against and the neck read
    // as wrong even when the dot was right. Frets are drawn on the SAME FRET_X() the dots use, so
    // the two cannot disagree.
    // FRET WIRES: ONE pixel wide, but graded down its length rather than flat. A real nickel fret
    // is lit from above, so bright at the top and falling off toward the board; three vertical
    // bands are enough to read as metal at this size. (A 2px lit-edge + shadow-edge version was
    // tried and was too heavy — at 12px fret spacing it turned the neck into a ladder.)
    for (int f = 1; f <= NFRETS; f++) {
        int wx = FRET_WIRE(f), y0 = by + 4, hh = bh - 8;
        // …and warmed INTO the board. The middle band used to be CLR_LIGHT_GREY, a COOL grey on a
        // salmon fretboard, and that clash was the whole of the "noisy" read — the wires looked like
        // a grid drawn over the wood rather than hardware sitting in it. CLR_PEACH keeps the same
        // top-lit gradient while sharing the board's hue. The bottom stays CLR_MEDIUM_GREY (which is
        // already warm, 162/136/121) rather than going fully salmon, because a salmon shadow reads
        // too close to the ORANGE of a ringing string.
        //
        // NB: the obvious way to do this — wrap the wires in blend(BLEND_AVG) so they mix with
        // whatever is under them — is NOT SAFE. canvas-diff goes 0px → 192px with it: BLEND_* is
        // implemented on both paths with the same palette and the same 2r²+4g²+3b² metric, but the
        // GPU blends against a FROZEN SNAPSHOT of the canvas while the software path blends against
        // the LIVE canvas (studio.c says so at blend_gpu_begin), and the software side averages in
        // truncating integers where the shader uses floats. Either is enough to flip a
        // nearest-palette tie. So a blended cart renders differently on the SOFTWARE canvas, which
        // is the iOS path. Picking the colours by hand costs nothing and stays byte-identical.
        static const int LIT[3] = { CLR_WHITE, CLR_PEACH, CLR_MEDIUM_GREY };
        for (int b = 0; b < 3; b++) {
            int ya = y0 + hh * b / 3, yb = y0 + hh * (b + 1) / 3;
            rectfill(wx, ya, 1, yb - ya, LIT[b]);
        }
    }
    // INLAYS. They belong on the neck's CENTRE LINE, between the two middle strings — the old
    // `by + bh/2` was 2px low, which put a fret-5 inlay's top pixel on the D-string note dot and
    // read as "two stacked dots" (reported). NECK_MID is derived from the strings themselves so it
    // cannot drift again. The 12th's pair sits one string-gap either side, i.e. in the gaps rather
    // than on the outer strings: the bottom one used to land EXACTLY on the low E line.
    for (int f = 3; f <= NFRETS; f += (f == 9 ? 3 : 2)) {       // inlays at 3 5 7 9 12
        int ix = FRET_X(f);                                     // same lane as the finger dot
        if (f == 12) { circfill(ix, NECK_MID - STR_DY, 1, CLR_DARK_BROWN);
                       circfill(ix, NECK_MID + STR_DY, 1, CLR_DARK_BROWN); }
        else circfill(ix, NECK_MID, 1, CLR_DARK_BROWN);
    }
    // THE VIBRATING LENGTH STARTS AT THE FINGER, not the nut. A fretted string is dead between the
    // nut and the fret — that is what fretting IS — so the wave has to be anchored at the dot and
    // run to the bridge. It used to span the whole board from SX0 regardless of where you fretted,
    // which read as the note sounding from the wrong end of the neck. Open strings still run the
    // full length; a barre now visibly kills the whole left side of the board at once.
    for (int s = 0; s < NSTR; s++) {
        int y = STR_Y(s);
        amp[s] *= 0.93f; vib_ph[s] += 0.6f;
        int col = amp[s] > 0.5f ? CLR_LIGHT_YELLOW : amp[s] > 0.1f ? CLR_DARK_ORANGE : CLR_MEDIUM_GREY;
        int f   = str_fret(s);
        int vx0 = (f > 0) ? dot_x(s) : SX0;                    // muted (f < 0) has no dot → nut
        if (vx0 > SX0) line(SX0, y, vx0, y, CLR_MEDIUM_GREY);  // the damped stretch, always at rest
        int span = SX1 - vx0; if (span < 1) span = 1;
        int px = vx0, py = y;
        for (int xx = vx0 + 8; xx <= SX1; xx += 8) {
            float t  = (float)(xx - vx0) / (float)span;
            int   wy = y + (int)(amp[s] * 4.0f * sinf(t * 9.42f + vib_ph[s]) * sinf(t * 3.14f));
            line(px, py, xx, wy, col); px = xx; py = wy;
        }
        if (px < SX1) line(px, py, SX1, y, col);               // close to the bridge (a node)
    }
    // Chord-chart notation, so the neck says the same thing a songbook would:
    //   ✗ at the nut = DAMPED · hollow ring = OPEN · filled dot = fingered at that fret.
    for (int s = 0; s < NSTR; s++) {
        int f = str_fret(s), y = STR_Y(s);
        if (f == FRET_MUTE) {                                   // ✗ — was drawn as an open ring, and
            line(SX0 - 1, y - 2, SX0 + 3, y + 2, CLR_DARK_RED); // sounded as one too (see str_fret)
            line(SX0 - 1, y + 2, SX0 + 3, y - 2, CLR_DARK_RED);
        } else if (f == 0) {
            circ(SX0 + 2, y, 2, CLR_DARK_RED);
        } else {
            int dx = FRET_X(f);                                 // same lane as the drawn fret wire
            circfill(dx, y, 2, CLR_DARK_RED); pset(dx - 1, y - 1, CLR_PEACH);
        }
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
        font(FONT_TIC); print(str("%c", SHAPE_KEY[i]), x + 3, SHAPE_Y + 2, on ? CLR_BLACK : CLR_MEDIUM_GREY); font(FONT_NORMAL);
    }
    for (int i = 0; i < NROOT; i++) {
        int x = ROOT_X(i); bool on = (i == sel_root);
        rrectfill(x, ROOT_Y, ROOT_W, CHORD_H, 3, on ? CLR_LIME_GREEN : CLR_DARKER_GREY);
        rrect(x, ROOT_Y, ROOT_W, CHORD_H, 3, on ? CLR_WHITE : CLR_DARK_GREY);
        print_centered(ROOT_NAME[i], x + ROOT_W / 2, ROOT_Y + 8, on ? CLR_BLACK : CLR_MEDIUM_GREY);
        font(FONT_TIC); print(str("%c", ROOT_KEY[i]), x + 3, ROOT_Y + 2, on ? CLR_BLACK : CLR_MEDIUM_GREY); font(FONT_NORMAL);
    }
}

// the pinned CABINET box at the far right (never scrolls): the chain plugs into it. Tenant =
// none / guitar amp (a voicing) / Leslie; header taps cycle it, the selector row steps, two knobs.
static void draw_cabinet(void) {
    int x = CAB_X, cx = x + CAB_W / 2;
    int sel = cab_sel();
    bool amp = (cab_tenant == CAB_AMP), none = (cab_tenant == CAB_NONE), rot = (cab_tenant == CAB_LESLIE);
    int accent = amp ? AMP_VC[cab_voicing].col : none ? CLR_DARKER_GREY : CLR_LIGHT_GREY;
    // "…into the amp". FONT_TINY and parked between the header and the first knob-label row: at the
    // old FONT_NORMAL/+33 it sat right on the last visible pedal's right-column label (ui-audit's
    // long-standing "DMP overlaps >").
    font(FONT_TINY); print(">", VIEW_R + 1, PED_Y + 15, CLR_DARK_GREY); font(FONT_NORMAL);
    rrectfill(x, PED_Y, CAB_W, PED_H, 4, none ? CLR_BROWNISH_BLACK : CLR_DARK_BROWN);
    rrect(x, PED_Y, CAB_W, PED_H, 4, accent);
    // AMP, not "CABINET": the five entries are ampcab.h AMP voicings (drive + EQ + glue), i.e. the
    // preamp, not the speaker box. A player read the old label and reasonably expected speakers.
    font(FONT_SMALL); print_centered("AMP", cx, PED_Y + 2, none ? CLR_MEDIUM_GREY : CLR_WHITE);

    // ONE selector, with the arrows DRAWN so the list is visibly a list (the whole point of the
    // rework — the old version cycled invisibly). Wide middle = next, so a plain tap still works.
    print("<", x + 2, PED_Y + 14, CLR_MEDIUM_GREY);          // x+2/-6, not x+4/-8: "HI-GAIN" is the
    print(">", x + CAB_W - 6, PED_Y + 14, CLR_MEDIUM_GREY);  // widest entry and was touching them
    print_centered(cab_sel_name(sel), cx, PED_Y + 14, none ? CLR_MEDIUM_GREY : accent);
    font(FONT_NORMAL);
    for (int i = 0; i < CAB_SEL_N; i++) {                                           // a pip per entry: where am I, how many are there
        int px = cx - (CAB_SEL_N * 3) / 2 + i * 3 + 1;
        pset(px, PED_Y + 24, i == sel ? (none ? CLR_MEDIUM_GREY : accent) : CLR_DARKER_GREY);
    }
    if (none) {
        for (int gy = PED_Y + 36; gy < PED_Y + 64; gy += 3) line(x + 6, gy, x + CAB_W - 6, gy, CLR_DARKER_GREY);  // dim grille
        return;
    }
    int ky = PED_Y + (rot ? 42 : 46), kx0 = x + 15, kx1 = x + CAB_W - 15;   // rotary lifts the knobs:
                                                                            // the speed row needs the floor
    const char *l0 = amp ? "GAIN" : "DRV", *l1 = amp ? "SAG" : "BAL";
    for (int j = 0; j < 2; j++) {
        int kx = j ? kx1 : kx0;
        bool turning = (drag_cat == KM_CAB && drag_knob == j);
        circfill(kx, ky, 6, CLR_BROWNISH_BLACK); circ(kx, ky, 6, accent);
        float a = (-135.0f + cab_k[j] * 270.0f) * 0.0174533f;
        line(kx, ky, kx + (int)(sinf(a) * 5), ky - (int)(cosf(a) * 5),
             (turning && drag_fine) ? CLR_ORANGE : CLR_WHITE);
        font(FONT_TINY);
        print_centered(turning ? str("%d", (int)(cab_k[j] * 99.0f + 0.5f)) : (j ? l1 : l0),
                       kx, ky + 8, CLR_LIGHT_PEACH);
        font(FONT_NORMAL);
    }
    // ROTARY's speed on the FRONT PANEL, as the three-way it physically is (the Leslie half-moon
    // switch). It used to hide inside the same invisible cycle as the amp choice, which buried the
    // engine's best trick: the rotors take SECONDS to spin up/down, so flipping SLOW<->FAST is the
    // chorale-to-tremolo swell. That is the most recognisable thing a Leslie does; it should be one
    // obvious tap, not a discovery.
    if (rot) {
        static const char *SPD[3] = { "STP", "SLW", "FST" };
        font(FONT_TINY);
        for (int i = 0; i < 3; i++) {
            int bw = CAB_W / 3, bx = x + i * bw;
            bool on = (cab_speed == i);
            rrectfill(bx + 1, PED_Y + 58, bw - 2, 10, 2, on ? CLR_LIGHT_GREY : CLR_BROWNISH_BLACK);
            print_centered(SPD[i], bx + bw / 2, PED_Y + 61, on ? CLR_BLACK : CLR_MEDIUM_GREY);
        }
        font(FONT_NORMAL);
    }
}

void draw(void) {
    fit_canvas();   // idempotent with update()'s call — guarantees draw + hit-test share one canvas
    cls(CLR_BROWNISH_BLACK);

    // top bar — PEDALS palette (left), RIGS (next), GTR IN, AUTO (right) — all inside the safe frame
    int bx = saX, by0 = saY + 2;
    rrectfill(bx + 4, by0, 56, 11, 2, palette_open ? CLR_INDIGO : CLR_DARKER_GREY);
    rrect(bx + 4, by0, 56, 11, 2, palette_open ? CLR_WHITE : CLR_DARK_GREY);
    rrectfill(bx + 64, by0, 46, 11, 2, rig_open ? CLR_ORANGE : CLR_DARKER_GREY);
    rrect(bx + 64, by0, 46, 11, 2, rig_open ? CLR_WHITE : CLR_DARK_GREY);
    font(FONT_SMALL);
    print(palette_open ? "x CLOSE" : "= PEDALS", bx + 9, saY + 4, CLR_WHITE);
    print(rig_open ? "x RIGS" : "RIGS", bx + 70, saY + 4, rig_open ? CLR_WHITE : CLR_LIGHT_PEACH);
    // GUITAR IN — route the live mic through the built chain (green = live, red = armed/awaiting mic)
    bool mic_live = guitar_in && mic_active();
    rrectfill(bx + 114, by0, 54, 11, 2, guitar_in ? (mic_live ? CLR_DARK_GREEN : CLR_DARK_RED) : CLR_DARKER_GREY);
    rrect(bx + 114, by0, 54, 11, 2, guitar_in ? CLR_WHITE : CLR_DARK_GREY);
    print(guitar_in ? "GTR: IN" : "GTR IN", bx + 120, saY + 4, guitar_in ? CLR_WHITE : CLR_LIGHT_PEACH);
    print_right(str("AUTO: %s", AP_NAME[autoplay]), saX + saW - 6, saY + 5, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    if (palette_open) { font(FONT_TINY); print_centered("UP add   DOWN remove", (saX + 168 + saX + saW - 70) / 2, saY + 5, CLR_MEDIUM_GREY); }
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
    if (chain_n == 0) { font(FONT_SMALL); print_centered("open PEDALS, drag effects in →", saX + saW / 2, PED_Y + 32, CLR_DARK_GREY); font(FONT_NORMAL); }

    // SCROLL INDICATOR — moved to the TOP of the rack and thinned to 2px. It used to be a 6px
    // grabbable bar directly above the strings, i.e. in the strum's landing zone; up here it is out
    // of the way and, being 2px, plainly a readout rather than something to grab.
    if (max_scroll() > 0) {
        rectfill(CHAIN_X0, PED_Y - 2, VIEW_W, 2, CLR_DARKER_GREY);
        int tw = VIEW_W * VIEW_W / content_w();
        int tx = CHAIN_X0 + (int)(scroll_x / max_scroll() * (VIEW_W - tw));
        rectfill(tx, PED_Y - 2, tw, 2, CLR_LIGHT_GREY);
    }

    if (palette_open)  draw_palette();
    else if (rig_open) draw_rigs();
    else               draw_guitar();

    // drag ghost (on top of everything)
    for (int j = 0; j < PTR_MAX; j++)
        if (ptr[j].id != PTR_NONE && (ptr[j].mode == PTR_DRAGSLOT || ptr[j].mode == PTR_DRAGPAL))
            draw_chip(ptr[j].cat, ptr[j].x - 22, ptr[j].y - 13, 44, 26, true);
}
