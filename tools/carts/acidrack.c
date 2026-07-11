/* de:meta
{
  "slug": "acidrack",
  "title": "acid rack",
  "status": "active",
  "created": "2026-07-02",
  "resizable": true,
  "kind": [
    "instrument",
    "generative"
  ],
  "teaches": [
    "step-sequencer",
    "subtractive-synth",
    "drum-synthesis",
    "generative-melody"
  ],
  "lineage": "Propellerhead ReBirth RB-338 (1997) — the 2×TB-303 + drum-machine acid rack — rebuilt as one cart from the shipped machine carts (tb303 ×2 on the new FILTER_DIODE, tr909 voices), with Reason-style rack-fold accordion panels and pattern banks + a chain row = song mode. All four increments of docs/design/rebirth-classic.md — the tinyjam-racks pilot: generate, play, export.",
  "homage": "Propellerhead ReBirth RB-338 (1997)",
  "todo": [
    "touch stroke entry: right-click cycles 909 strokes — needs a long-press path on phones",
    "no undo — CLR/RND overwrite the scoped lane/bank irreversibly (autosaved, so it sticks); ReBirth was merciless too but an undo slot would be kind",
    "303 lines don't shuffle (held voice can't schedule ahead) — revisit if straight-303-vs-swung-drums clashes at high swing",
    "device-adaptive redesign (Phase 3 — design/device-adaptive-layout.md): reflow the machine-strip accordion per device — iPad more strips inline, phone-portrait accordion, phone-landscape tabs (accordion degenerates on a short screen). The disclosure model already exists in-cart; graft the arrangements from acidfit.c. Build resizable: RESIZABLE=1 CART=acidrack ./ios/build.sh",
    "compact drum view — the authentic Roland x0x model: instead of a full N-voice matrix, show ONE big editable 16-step lane for the selected voice + an instrument-select knob/buttons to switch which voice the 16 cells map to (that's exactly how the real 808/909 solved the panel-space problem — never a grid). Pairs well with a thin per-voice density overview (each voice one row of filled/empty dots, tap to promote to the big lane). The 808's 16 voices make this most valuable there; would also make phone-portrait trivially fit. Alt/bolder mode discussed: Euclidean per-voice (hits, rotation) knobs — a whole acid beat in ~5 voices × 2 knobs, no step-drawing. See the chat where the full-16 expansion landed.",
    "SHIPPED (2026-07-11): 303 DEEP-page — the Devil Fish / TD-3-MO depth mods, each a true engine port (sources: firstpr.com.au/rwi/dfish, Robin Whittle's manual). The P1/P2 button on each 303 seq panel flips the knob row between the classic 7 and the DEEP 5: ADEC (separate accent-decay — the signature two-decay; accented steps get their own ENV_CUTOFF decay time), SLDT (variable slide time 20-300ms, was fixed 60), ATK (soft attack 0.5-30ms = the amp-env attack, rounds the click), TRK (filter tracking — higher notes open the cutoff), SUB (an octave-down INSTR_TRI sub voice on slots 34/35, gated/slid in lockstep — off = no voice spent). Plus the accent-SWEEP mode button (off/slow/med/fast onset) and a cutoff-range bump to ~5kHz. All DEEP defaults are no-ops (ADEC=DEC, SLDT≈60ms, ATK≈2ms, TRK/SUB/SWEEP off) so the stock sound is unchanged until you turn a knob. SAVE_MAGIC bumped to v8. Deliberately SKIPPED (still true): the clones' MIDI/USB/CV-gate/patch-point/PC-editor + self-resonance/filter-FM (hardware wiring, moot in one cart); env-mod-to-3x (SQL already does it) and overdrive (DRV, though ours is post-filter vs the Devil Fish pre-filter overdrive + post-VCA Muffler — a future 'Muffler' distortion could revisit that). FOLLOW-UP TUNING (2026-07-11): ATK now ALSO rounds the filter onset (0 at stock → ~28ms) and TRK steepened (170 Hz/semitone) so both bite harder; note that full-mix WAV correlation understates per-knob impact because the 303 sits under the 909+808 drums (measure the 303 in isolation).",
    "SHIPPED (2026-07-11): vanilla tb303 parity for the 303 lines — OCTAVE DOWN (the OCT flag row now tap-CYCLES off→up→down→off; up=yellow, down=indigo; -12 via a new octd mask) and TIE (a 4th flag row: green = hold the previous note through this step — the sustain a slide can't give; step_303 sustains it, gate_303 won't cut into a tie). The generator sprinkles both (octave up/down ~15%, ties on ~18% of rests). Swing was already present (master SHUF). Also SHIPPED: per-line pattern LENGTH — a LEN strip under the flag rows sets each 303 line's length 1..16 (Line.len, 0=full); the transport steps each line at s16 % lnlen so a short line DRIFTS against the 16-step bar (true polymeter, continuous counter across bar/bank seams); the roll shows the line's OWN playhead (lpos[]), dims the dead zone, and draws a red loop-end divider. The tb303-vs-acidrack gap is now closed. Also added a STD button on P2 that resets that 303's Devil Fish knobs to stock (reset_deep: ADEC=DEC, SLDT/ATK stock, TRK/SUB/SWEEP off) — 'as if no Devil Fish'. SAVE_MAGIC v10 (Line grew: +octd,+tie,+len). BUGFIX (2026-07-11, found in playtest): filter TRACKING on an octave-DOWN note computed a NEGATIVE cutoff → NaN in the diode ladder → poisoned the master bus = sudden permanent silence; now clamped (cut>=30) in step_303. Any new cutoff math near the ladder MUST stay positive.",
    "song-level transpose — a per-song-cell semitone offset (int8_t songTranspose[64]) added into BOTH 303s' pitches[] at the bar boundary (drums ignore it), so one bank = one riff reused at multiple pitches. It's the pitch twin of the existing per-bank PCF lane, and applying it at the bar boundary sidesteps the slide/tie/shuffle scheduling caveat above (no mid-pattern pitch mutation). This is the arrangement feature the stock 303 lacked and every clone workflow added via MIDI-note transpose — see the chat on modern-303 usage. A per-303 override (one plays a third/fifth above) is a later nicety; default is shared.",
    "more pattern memory (minor) — x0xb0x carried 128 patterns vs the 303's handful; acidrack has 4 banks A-D. If song-building starves for raw material, add user-savable pattern slots. Low priority: the song chain already reuses A-D across 64 bars.",
    "drum step PROBABILITY — the biggest modern-drum-machine feature acidrack lacks (Elektron → Behringer RD-8/RD-9 all added it): each active drum step gets a % chance to fire, so hats/fills breathe and the loop never repeats identically. THE house/techno move for a living pattern out of a static grid. Cheap: a per-cell 0-100 (or 4-5 buckets) rolled against a per-step RNG at trigger time. Highest bang-for-buck here. Pairs with per-track length below. NOT greenfield — GRAFT IT FROM tr808.c / tr909.c, which SHIPPED exactly this and are the closest reference (same editable-grid family as acidrack, not the pocketbox sequencer model): `gprob[NV][STEPS]` per-cell % + `snap_prob()` to the RD-8/RD-9 buckets (100/75/50/25), driven by an AXIS-LOCKED gesture — a VERTICAL drag on a cell rides its fire-chance while a horizontal drag still paints on/off (one gesture, axis decided at first move); a sub-100 cell draws as a shorter bar in its full-height socket. tr909.c also proved the STROKE FAMILY worth grafting alongside it — flam/drag/ratchet per cell (right-click cycles on desktop; a STROKE header toggle arms tap-to-cycle on touch) — the humanize trick that pairs naturally with probability. Older/secondary references only: pocketbox.c's Elektron trigger conditions (CONDS[]/cond_ok(), incl. 50% + X:Y loop conditions) and turing.c's pure per-step flip-probability engine (flip_prob()).",
    "per-track step LENGTH / polymeter (RD-9 'Poly') — let each drum voice run a length other than 16 (e.g. a 15-step hat against a 16-step kick) so lanes drift in and out of phase — long-form techno evolution from one loop. acidrack is locked to STEPS=16 per voice today; needs a per-voice len + its own wrap counter off the shared transport. Watch the interaction with shuffle and the 16-tick mini-pattern header rendering.",
    "sidechain PUMP — duck the non-drum buses (the two 303s + returns) to the kick for the classic house/techno breathing. acidrack has the master GLU glue-comp but no kick-triggered duck; a single per-machine (or master) PUMP amount knob keyed off the 909/808 kick step is the gesture. Set-and-hold envelope, re-armed on each kick hit; mind the fx-frame rule (configure on change, not every frame).",
    "motion / parameter-LOCK lanes (TR-8S motion · Elektron p-locks) — the deep one: automate any per-voice knob (TUNE/DEC/CHAR, 303 CUT/RES) per-step or by recording knob moves, played back as a delta over the base value. acidrack ALREADY has the concept via the per-bank PCF lane — this generalizes it to per-voice lanes (draw a lane like PCF, or 'Swedish method': hold a step + turn a knob to imprint). Biggest build; do probability + polymeter first. References: pocketbox.c already has literal PER-STEP p-locks (SP_LK1..SP_LK4 lock pages + the Step struct); motionbox.c is the continuous MOTION-RECORD flavor (record knob moves, played back as a delta) — the TR-8S sibling.",
    "MASTER-STRIP LIVE PERFORMANCE FX (house/techno live-set moves) — the current MASTER strip is all PROGRAMMED (delay TIME/FB, GLU glue-comp, PCF drawn-per-bank filter lane) with no hands-on momentary FX; a live set lives on the stuff you grab mid-bar to build tension and drop it. In rough value order: (1) LIVE DJ FILTER — a bipolar HP<->LP sweep knob on the master, center=bypass, left=highpass (strip lows for a breakdown), right=lowpass (muffle into a drop). This is THE most-used live knob in the genre and acidrack lacks it — the PCF is programmed, not performed. filter() is ride-live-safe (excluded from the fx-frame lint). Compose it with the existing PCF lane (both touch the master lowpass — decide whether the live knob offsets or overrides the drawn lane). Highest bang-for-buck. (2) BEAT-REPEAT / STUTTER / gate-retrigger (momentary) — hold a pad to loop the last 1/16 or 1/8 of the master out, or gate it rhythmically; the build/drop electrifier, pairs with the filter sweep. If built on crush/tape it's set-and-hold — arm/disarm on the button EDGE, don't reconfigure per frame (fx-frame rule). (3) MASTER DRIVE / saturation lift — GLU tames the drop, a drive knob LIFTS it (motionbox's 'Valve' over the summed bus); cheap, the push-into-the-red move. (4) TAPE-STOP / brake (momentary) — varispeed the master toward zero while held, snap back on release; varispeed is ride-live-safe. (5) DELAY THROW (momentary) — on top of the existing shared delay + per-device sends, a live throw: momentarily crank FB toward self-oscillation + slam the send, pull back. (6) REVERB (palette-widener, not a performance knob) — delay is the only shared space today; a reverb hall opens up breakdowns. IMPORTANT: reverb is a SEND, not an insert, so it is NOT blocked on the effects-bus Increment G (confirmed still open 2026-07-11) and needs NO per-machine bus work — that whole single-bus-per-voice wall only bites INSERT fx (crush/filter/comp grouped over a machine). Per-slot reverb already ships: reverb(size,damping) configures the one shared hall + instrument_reverb(slot,send) sends any slot's wet copy in (dry stays full-volume; grabs no aux bus). A 303 is ONE slot (SLOT_A=5/SLOT_B=6; the octave-down sub SLOT_SUB_A=34 is a separate slot you'd keep DRY, same logic as keeping the kick out of the verb), so 'a wet 303 line in space' = literally `instrument_reverb(SLOT_A, send_knob)` today, cost = the already-allocated shared tank. Do it per-instrument via sends (a SEND knob on each machine's [fx] view alongside the existing delay SEND — kick/low toms at 0, snare/clap/303s to taste), NOT as a master insert. A momentary reverb THROW can layer on top like the delay throw. Top two = LIVE FILTER + BEAT-REPEAT: what your hands are on the last 4 bars before every drop. From the chat exploring house-liveset master knobs (2026-07-11)."
  ],
  "description": {
    "summary": "Two 303s, the full 909, the full 16-voice 808 and per-device FX in one cart — and a SONG CODE: 8 hex characters that generate a whole arranged acid track (banks A-D + the chain), ready to edit while it plays, then export as WAV.",
    "detail": "The RB-338 homage, increment 2: two full TB-303 voices (the engine's FILTER_DIODE diode-ladder squelch, authentic non-retriggering slide, accent into the filter env, live CUT/RES/DRV knobs per machine) + the COMPLETE tr909 kit (all 11 voices, the real 909 roster — ordered for acid: kick/hats/clap/snare up top, rim + ride/crash next, the fill toms at the bottom — analog kick/snare/toms/rim/clap, FM-clang metal, the stroke family: right-click a cell for flam/drag/ratchet, the METAL-FILTER XY pad riding all five metal highpasses, TOTAL ACCENT row) + the COMPLETE tr808 (all 16 voices: boom kick, snare, 3 toms + 3 congas on the shared bridged-T circuit, rim/clave, clap, maracas, cowbell, cymbal, open/closed hats — ordered for acid: the always-on kick/hats/clap/snare up top, cowbell + accents next, the fill voices at the bottom; the old 9-voice curation was based on a slot budget that never really applied — the engine has 48 slots, not 32), all clocked off one transport with the 909's period-correct even-16th SHUFFLE (one master knob + Z/X). Effects are PER-DEVICE like the real RB-338: every machine strip has an [fx] view (the header button swaps the grid for that machine's effects) with DIST (per-voice drive on that machine only — drums scream while the rest stays clean) and SEND (its level into THE one shared tempo-synced delay unit, per-device routing like the hardware; the 909's fx view also hosts the METAL pad and SHUFFLE). The MASTER strip keeps what genuinely needs a bus: the delay unit's TIME (snaps 1/16, 1/8, dotted-8th, 1/4) and FB, GLU (the glue compressor that tames the drop), and the PCF: a pattern-controlled filter whose 16-step level lane is drawn per BANK, so the arrangement itself rides the master lowpass — the demo's build bank is literally a drawn ramp that opens the filter over one bar. (True per-device PCF/comp waits on machine buses — effects-bus-architecture.md Increment G.) The rack is an ACCORDION: each machine is a slim strip showing its name, a live 16-tick mini pattern with the playhead, and a MUTE — tap a strip to expand its full editor (piano roll + flag rows + knobs for a 303, trigger grid for the drums). Sound never depends on what's open. Patterns live in four BANKS (A-D, the transport buttons); the SONG row at the bottom chains up to 64 bars of banks into a real track — tap a cell to cycle A→B→C→D→empty. Everything autosaves.",
    "controls": "SPACE run/stop · tap a strip header to expand · A-D buttons pick the edit bank (also LEFT/RIGHT) · SONG button toggles chain playback · tap SONG-row cells to write the arrangement · UP/DOWN tempo · Z/X shuffle · roll: tap/drag paints notes, tap a note to erase · 303 flag rows toggle per step: OCT (tap-cycles up→down→off), ACC, SLD, and TIE (holds the previous note through the step — sustain a slide can't give) · LEN strip under the flags sets each line's length 1-16 (short = polymeter drift; red divider = loop end) · 303 P1/P2 button flips the knob page (P2 = DEEP: ADEC separate-accent-decay / SLDT slide-time / ATK soft-attack / TRK filter-tracking / SUB sub-osc, + the accent-SWEEP mode button + STD = reset this 303's Devil Fish knobs to stock) · drums: tap cells, right-click a 909 cell for flam/drag/ratchet, AC row = accent, drag the METAL pad for hat tone · MASTER strip: GEN new song code (also G), tap code digits to nudge, [ ] history, WAV = export the arrangement · drum grids: per-voice TUNE/DEC/CHAR mini-knobs beside the rows (drag up=coarse right=fine, rclick=reset) · [fx] button on a machine strip: its DIST + delay SEND (909: also METAL pad + SHUF) · MASTER strip: delay TIME/FB, GLU, PCF/RES + drag the lane to draw the filter pattern · CPY/CLR/RND act on WHAT'S OPEN: a machine strip = just its lane of the edit bank, MASTER = the whole bank (CPY arms — tap the target bank to paste)"
  }
}
de:meta */
#include "studio.h"
#include "ui.h"          // widgets: ui_button/ui_knob — per-finger capture, focus ring
#include <stdio.h>
#include <math.h>
#include <string.h>

// ── ACID RACK ─────────────────────────────────────────────────────────────
// ReBirth RB-338 (1997) put "acid techno in a box": two TB-303s and Roland
// drum machines wired to one transport, pattern banks, a song chain. This
// cart rebuilds that shape from the machines this console already ships:
// the tb303 cart's voice (upgraded to FILTER_DIODE — audio-notes §25) twice,
// and the tr909 cart's kick/clap/hats as the starter kit (the full curated
// 808 + 909 panels are increment 2 — see docs/design/rebirth-classic.md).
//
// The two design invariants (from the design doc):
//   1. sound NEVER depends on what's expanded — the sequencer runs globally;
//      the accordion is pure view. Fold the drums mid-roll, nothing changes.
//   2. folded ≠ blind — every strip header carries a live 16-tick mini
//      pattern with the playhead, so the whole rack visibly breathes.
//
// Data model: a PATTERN is a whole-rack snapshot (both 303 lines + the drum
// grid). Four pattern BANKS (A-D); the SONG row chains up to 64 bank picks,
// one per bar. The generator (increment 4) will fill A sparse → D drop.

#define STEPS   16
#define NBANK   4
#define CHAINN  64
#define BASE    36            // C2 — bottom row of the 303 roll

// instrument slots — the rack's slot plan (rebirth-classic §1). The engine's
// table is SOUND_INSTR_SLOTS=48 (0-4 raw waves, 5-47 cart-defined), so there's
// ample room; the old "808 curated to fit 5..31" budget was based on studio.h's
// stale doc line and no longer applies (self-contained cart — nothing else
// defines slots here).
// 303s: 5,6 · 909 complete: 7..19 (13) · 808 complete: 20..33 (14) · 34,35 = 303 subs · 36..47 free
#define SLOT_A    5           // 303-A
#define SLOT_B    6           // 303-B
#define SLOT_SUB_A 34         // 303-A sub-osc layer (Devil Fish / TD-3-MO depth — octave-down voice)
#define SLOT_SUB_B 35         // 303-B sub-osc layer
// tr909 (complete kit — recipes verbatim from tr909.c)
#define SL9_BD    7
#define SL9_BDC   8           // kick click layer (the ATTACK knob sound)
#define SL9_SDB   9
#define SL9_SDN   10
#define SL9_TOM   11
#define SL9_TOMC  12
#define SL9_RS    13
#define SL9_CP    14
#define SL9_HHC   15
#define SL9_HHO   16
#define SL9_CC    17
#define SL9_CCN   18
#define SL9_RC    19
// tr808 (COMPLETE 16-voice kit — recipes verbatim from tr808.c)
#define SL8_BD    20
#define SL8_SDB   21
#define SL8_SDN   22
#define SL8_TOM   23
#define SL8_TOMN  24
#define SL8_CP    25
#define SL8_MAR   26
#define SL8_CB    27
#define SL8_HATO  28
#define SL8_HATC  29
#define SL8_CON   30          // congas — the tom circuit, cleaner + shorter
#define SL8_RS    31          // rimshot — dual bridged-T through a highpass
#define SL8_CLV   32          // claves — the rim circuit retuned to 2500Hz
#define SL8_CYT   33          // cymbal — metal-bank squares, long 3440Hz ring

// drum voice tables
// acid order — always-on kick/hats/clap/snare up top, rim + ride/crash next,
// the fill toms at the bottom (the standalone tr909 cart keeps the kit order).
enum { V9_BD, V9_CH, V9_OH, V9_CP, V9_SD, V9_RS, V9_RC, V9_CC, V9_LT, V9_MT, V9_HT, N909 };
// acid order — the 5 always-on acid voices up top (kick, hats, clap, snare),
// signature cowbell + accents next, fill voices (cymbal/toms/congas) at the
// bottom where they're rarely touched.
enum { V8_BD, V8_CH, V8_OH, V8_CP, V8_SD, V8_CB, V8_RS, V8_CLV,
       V8_MA, V8_CY, V8_LT, V8_MT, V8_HT, V8_LC, V8_MC, V8_HC, N808 };
static const char *NAME909[N909] = { "BD", "CH", "OH", "CP", "SD", "RS", "RC", "CC", "LT", "MT", "HT" };
static const char *NAME808[N808] = { "BD", "CH", "OH", "CP", "SD", "CB", "RS", "CL",
                                     "MA", "CY", "LT", "MT", "HT", "LC", "MC", "HC" };
enum { ST_PLAIN, ST_FLAM, ST_DRAG, ST_RATCHET, NSTROKE };   // 909 stroke family
#define FLAM_MS 22

// ── pattern data ──────────────────────────────────────────────────────────
typedef struct {                       // one 303 line in one bank
    unsigned char  pitch[STEPS];       // semitone 0..12 above BASE
    unsigned short on, oct, acc, sld;  // per-step bitmasks
    unsigned short octd, tie;          // oct DOWN (-12) · TIE = hold prev note through this step (sustain)
    unsigned char  len;                // pattern LENGTH 1..16 (0 = full 16); short = polymeter drift vs the bar
} Line;
typedef struct {                       // one bank = a whole-rack snapshot
    Line           ln[2];              // [0]=303-A [1]=303-B
    unsigned short d909[N909];         // 909 trigger masks
    unsigned char  st909[N909][STEPS]; // 909 stroke per cell (ST_*)
    unsigned short acc909;             // 909 TOTAL ACCENT row
    unsigned short d808[N808];         // 808 trigger masks
    unsigned short acc808;             // 808 accent row
    unsigned char  pcf[STEPS];         // pattern-controlled-filter lane, level 0..7
} Pattern;

static Pattern bank[NBANK];
static void gen_song(unsigned seed);   // the increment-4 generator (defined below)
#define DEFAULT_SEED 0xAC1D5EEDu       // the first-boot song code
static unsigned cur_seed = 0;          // the song code on display
static unsigned char chain[CHAINN];    // bank index per bar; 0xFF = empty (song ends)
static int  chainN     = 0;            // bars in the song (first empty cell)

// ── transport / play state ────────────────────────────────────────────────
static int  tempo = 130;
static bool running = true;
static bool songmode = true;           // play the chain (else: loop the edit bank)
static int  editBank = 0;              // which bank A-D the panels edit
static int  playBank = 0;              // which bank is sounding this bar
static int  barpos = 0;                // position in the chain
static int  last16 = -1, playhead = 0;
static int  lpos[2] = { 0, 0 };        // each 303 line's OWN sounding step (per-line LENGTH → polymeter drift)
static int  swing = 50;                // master shuffle 50..66 — 909-style, even 16ths drag
static float swingf = 0.0f;            // the SHUF slider's 0..1 view of it

// ── the machines ──────────────────────────────────────────────────────────
// K_CUT..K_SQL = the classic 303 knobs (page 0). K_ADEC..K_SUB = the Devil Fish /
// TD-3-MO depth knobs (page 1, the DEEP page). All live in one knob[] array.
enum { K_CUT, K_RES, K_ENV, K_DEC, K_ACC, K_DRV, K_SQL,
       K_ADEC, K_SLDT, K_ATK, K_TRK, K_SUB, NK };
static const char *KNAME[NK] = { "CUT", "RES", "ENV", "DEC", "ACC", "DRV", "SQL",
                                 "ADEC", "SLDT", "ATK", "TRK", "SUB" };
// the two knob PAGES drawn on the 303 seq panel (both fit ONE row; roll stays put)
static const int KPAGE0[] = { K_CUT, K_RES, K_ENV, K_DEC, K_ACC, K_DRV, K_SQL };
static const int KPAGE1[] = { K_ADEC, K_SLDT, K_ATK, K_TRK, K_SUB };
#define NKP0  (int)(sizeof KPAGE0 / sizeof *KPAGE0)
#define NKP1  (int)(sizeof KPAGE1 / sizeof *KPAGE1)
#define NKMAX NKP0                         // the roll reserves space for the taller page
static int kpage[2];                       // per-303 knob page: 0 = classic, 1 = DEEP

typedef struct {
    int   slot;
    const char *name;
    float knob[NK];         // 0..1 (ui_knob's range)
    int   wave;             // INSTR_SAW / INSTR_SQUARE
    int   h;                // held note handle (-1 = none)
    bool  prev_slide;
    int   hsub;             // sub-osc held handle (-1 = none)
    int   sweep;            // accent-sweep mode 0=off 1=slow 2=med 3=fast
} M303;
// DEEP-page defaults chosen to be NO-OPS out of the box: ADEC = that machine's DEC
// (accent decays like a normal note until moved), SLDT≈60ms, ATK≈2ms (the stock
// values), TRK/SUB = 0 (off). So adding the page changes nothing until you turn a knob.
static M303 m[2] = {
    { SLOT_A, "303-A", { 0.45f, 0.70f, 0.60f, 0.40f, 0.60f, 0.35f, 0.33f,
                         0.40f, 0.14f, 0.05f, 0.0f, 0.0f }, INSTR_SAW,    -1, false, -1, 0 },
    { SLOT_B, "303-B", { 0.38f, 0.75f, 0.55f, 0.45f, 0.60f, 0.45f, 0.33f,
                         0.45f, 0.14f, 0.05f, 0.0f, 0.0f }, INSTR_SQUARE, -1, false, -1, 0 },
};
static int sub_slot(M303 *s) { return s->slot == SLOT_A ? SLOT_SUB_A : SLOT_SUB_B; }

// knob value mappings — the classic curves are tb303.c's verbatim; the DEEP-page
// curves are the Devil Fish / TD-3-MO ranges (see the de:meta todo for provenance).
static int   cut_hz(M303 *s)  { return (int)(60.0f * powf(2.0f, s->knob[K_CUT] * 6.38f)); } // 60..~5100 (Devil Fish doubled the range to 5kHz)
static int   res_q(M303 *s)   { return (int)(s->knob[K_RES] * 15.0f); }
static float env_hz(M303 *s)  { return s->knob[K_ENV] * 3000.0f; }
static int   dec_ms(M303 *s)  { return 30 + (int)(s->knob[K_DEC]  * 500.0f); }
static int   adec_ms(M303 *s) { return 30 + (int)(s->knob[K_ADEC] * 500.0f); }  // accent's OWN filter-env decay (the two-decay)
static int   slide_ms(M303 *s){ return 20 + (int)(s->knob[K_SLDT] * 280.0f); }  // portamento 20..300ms (stock ≈60)
static int   atk_ms(M303 *s)  { return (int)(0.5f + s->knob[K_ATK] * 29.5f); }  // amp attack 0.5..30ms (soft attack)
static float sub_lvl(M303 *s) { return s->knob[K_SUB]; }                        // 0 = no sub voice at all
static float acc_mul(M303 *s) { return 1.0f + s->knob[K_ACC] * 1.5f; }
static float sq_mul(M303 *s)  { return 1.0f + s->knob[K_SQL] * 2.0f; }
static int   sweep_atk(M303 *s) { static const int A[4] = { 0, 10, 5, 2 }; return A[s->sweep & 3]; } // accent-sweep onset ms

// the 303 voice — tb303.c's recipe verbatim, on the diode ladder. The sub slot
// (Devil Fish / TD-3-MO depth) is a clean triangle an octave down; it sits UNDER
// the filtered voice (post-filter weight) and is gated in lockstep in step_303.
static void define_303(M303 *s) {
    instrument(s->slot, s->wave, atk_ms(s), 60, 6, 25);    // ATK = soft-attack (the amp-env attack)
    instrument_duty(s->slot, 0.48f);
    instrument_filter(s->slot, FILTER_DIODE, cut_hz(s), res_q(s));
    instrument_drive(s->slot, s->knob[K_DRV]);
    instrument(sub_slot(s), INSTR_TRI, atk_ms(s), 60, 6, 25);
}
static void knob_changed_303(M303 *s, int k) {
    if (k == K_CUT || k == K_RES) {
        instrument_filter(s->slot, FILTER_DIODE, cut_hz(s), res_q(s));
        if (s->h >= 0) { note_cutoff(s->h, cut_hz(s)); note_res(s->h, (float)res_q(s)); }
    } else if (k == K_DRV) {
        instrument_drive(s->slot, s->knob[K_DRV]);
        if (s->h >= 0) note_drive(s->h, s->knob[K_DRV]);
    } else if (k == K_ATK) {
        define_303(s);                               // soft attack = amp-env attack; set-and-hold, on change only
    }                                                // ENV/DEC/ACC/SQL/ADEC/SLDT/TRK/SUB apply at next trigger
}
// STD button on P2: reset the DEEP page to no-ops → sounds exactly like a stock 303
// with no Devil Fish at all (accent decays like a normal note, stock slide/attack, no
// tracking/sub/sweep). Classic P1 knobs + the vanilla flag rows are left untouched.
static void reset_deep(M303 *s) {
    s->knob[K_ADEC] = s->knob[K_DEC];   // accent decays like a normal note (two-decay off)
    s->knob[K_SLDT] = 0.14f;            // ≈60ms stock slide
    s->knob[K_ATK]  = 0.05f;            // ≈2ms stock attack
    s->knob[K_TRK]  = 0.0f;             // no filter tracking
    s->knob[K_SUB]  = 0.0f;             // no sub voice
    s->sweep = 0;                       // accent-sweep off
    define_303(s);                      // re-apply the stock amp attack
}
static void off_303(M303 *s) {
    if (s->h    >= 0) { note_off(s->h);    s->h    = -1; }
    if (s->hsub >= 0) { note_off(s->hsub); s->hsub = -1; }
    s->prev_slide = false;
}

static int lnlen(const Line *ln) { return ln->len ? ln->len : STEPS; }   // per-line LENGTH (0 = full 16)

// step trigger — the authentic circuit, from tb303.c: a slid step does NOT
// retrigger (glide carries the pitch, the filter env keeps decaying); accent
// is louder AND a harder env kick; non-slid gates drop at ~70% of the step.
// DEEP page adds: separate ACCENT DECAY, variable SLIDE TIME, FILTER TRACKING,
// the accent-SWEEP onset, and the octave-down SUB voice (all no-ops at default).
// `st` is the line's OWN step (s16 % lnlen) — a short line drifts against the bar.
static void step_303(M303 *s, Line *ln, int st) {
    int b = 1 << st;
    if (ln->on & b) {
        int  midi = BASE + ln->pitch[st] + ((ln->oct & b) ? 12 : (ln->octd & b) ? -12 : 0);
        int  vol  = (ln->acc & b) ? 7 : 5;
        bool acc  = (ln->acc & b) != 0;
        if (s->h >= 0 && s->prev_slide) {
            int g = slide_ms(s);                     // variable SLIDE TIME (was fixed 60)
            note_glide(s->h, g);
            note_pitch(s->h, (float)midi);
            note_vol(s->h, vol);                     // env does NOT refire — authentic
            if (s->hsub >= 0) { note_glide(s->hsub, g); note_pitch(s->hsub, (float)midi - 12); }
        } else {
            if (s->h    >= 0) note_off(s->h);
            if (s->hsub >= 0) { note_off(s->hsub); s->hsub = -1; }
            float e    = env_hz(s) * sq_mul(s) * (acc ? acc_mul(s) : 1.0f);
            int   dec  = acc ? adec_ms(s) : dec_ms(s);         // separate ACCENT DECAY (the two-decay)
            int   soft = atk_ms(s) - 2; if (soft < 0) soft = 0;// SOFT ATTACK also rounds the FILTER onset (0 at stock, ~28ms wide open)
            int   atk  = acc ? sweep_atk(s) : 0;               // accent-SWEEP onset (0 = off/instant)
            if (soft > atk) atk = soft;                        // whichever rounds more wins
            if (s->knob[K_TRK] > 0.001f) {                     // FILTER TRACKING: higher notes open the cutoff
                int cut = cut_hz(s) + (int)(s->knob[K_TRK] * (midi - BASE) * 170.0f);
                if (cut < 30) cut = 30;                        // musical floor (engine ladder_core now also guards, but keep the tracked cutoff sane)
                instrument_filter(s->slot, FILTER_DIODE, cut, res_q(s));
            }
            instrument_env(s->slot, 0, ENV_CUTOFF, atk, dec, e);
            s->h = note_on(midi, s->slot, vol);
            note_glide(s->h, 0);
            float sl = sub_lvl(s);                             // SUB-OSC: an octave down, only when dialed in
            if (sl > 0.001f) {
                s->hsub = note_on(midi - 12, sub_slot(s), 0);
                note_vol(s->hsub, vol * sl);                    // SUB level 0..1 (note_vol takes fractions)
                note_glide(s->hsub, 0);
            }
        }
        s->prev_slide = (ln->sld & b) != 0;
    } else if (ln->tie & b) {
        // TIE: hold the previous note through this step (sustain, no retrigger,
        // no gate). A SLD flag on a tie step glides the held note into the NEXT.
        s->prev_slide = (ln->sld & b) != 0;
    } else {
        off_303(s);
    }
}
static void gate_303(M303 *s, Line *ln, int st) {   // staccato release mid-step
    if (ln->tie & (1 << ((st + 1) % lnlen(ln)))) return;  // next step ties → let it ring on (sustain)
    int b = 1 << st;
    if (s->h >= 0 && (ln->on & b) && !(ln->sld & b)) {
        float f = beat_pos() * 4.0f; f -= (int)f;
        if (f > 0.7f) {
            note_off(s->h); s->h = -1;
            if (s->hsub >= 0) { note_off(s->hsub); s->hsub = -1; }
        }
    }
}

// ── the drum machines — tr909.c / tr808.c recipes verbatim ───────────────
// Per-voice tune/decay/color knobs are the standalone carts' depth; the rack
// bakes their center values (a knob at 0.5 is a no-op in both source carts).
static int vv(int base, int boost) { int v = base + boost; return v < 0 ? 0 : (v > 7 ? 7 : v); }

// per-voice TUNE / DEC / CHAR knobs (the standalone carts' three mini-knob
// columns, ported): 0..1, center 0.5 = the recipe as shipped. Rack-global
// (kit voicing, not pattern data). CHAR is the per-voice character knob —
// only voices the hardware gave one (K2 label non-NULL) draw it.
static float kt9[N909], kd9[N909], kc9[N909];
static float kt8[N808], kd8[N808], kc8[N808];
static const char *K2LAB9[N909] = { "ATTK", 0, 0, 0, "SNPY", 0, 0, "TONE", "CLIK", "CLIK", "CLIK" };
static const char *K2LAB8[N808] = { 0, 0, "RING", 0, "SNPY", "TONE", 0, 0,
                                    0, "TONE", "THUD", "THUD", "THUD", 0, 0, 0 };
static int   k_midi(float *kt, int v, int base) { return base + (int)((kt[v] - 0.5f) * 24.0f + 0.5f); }
static int   k_dur(float *kd, int v, int base) {
    int d = (int)(base * powf(4.0f, (kd[v] - 0.5f) * 2.0f) + 0.5f);
    return d < 5 ? 5 : d;
}
static int   k_cv(float *kc, int v, int lo, int hi) {
    int val = (int)(lo + (hi - lo) * kc[v] + 0.5f);
    return val < 0 ? 0 : val > 7 ? 7 : val;
}

// 909 metal-filter XY pad state — X scales the five metal slots' highpass
// cutoffs, Y is their shared resonance (tr909's admitted impurity, kept:
// the FM stand-ins land bright and hissy without a tone control)
static float mcut = 0.40f, mres = 0.33f;
static void apply_metal_filter(void) {
    static const int msl[5]   = { SL9_HHC, SL9_HHO, SL9_CC, SL9_CCN, SL9_RC };
    static const int mbase[5] = { 8500, 8000, 4500, 5200, 5000 };
    float sc = 0.3f + 1.4f * mcut;
    int   rq = (int)(mres * 12.0f);
    for (int i = 0; i < 5; i++)
        instrument_filter(msl[i], FILTER_HIGH, (int)(mbase[i] * sc), rq);
}

static void define_909(void) {
    // kick — sine body, +30st sweep over 35ms (punch, not boom), run hot
    instrument(SL9_BD, INSTR_SINE, 0, 300, 0, 50);
    instrument_filter(SL9_BD, FILTER_LOW, 380, 2);
    instrument_env(SL9_BD, 0, ENV_PITCH, 0, 35, 30.0f);
    instrument_drive(SL9_BD, 0.35f);
    instrument(SL9_BDC, INSTR_NOISE, 0, 9, 0, 4);         // the ATTACK click
    instrument_filter(SL9_BDC, FILTER_HIGH, 2500, 2);
    // snare — 185+330Hz body modes + bright noise
    instrument(SL9_SDB, INSTR_SINE, 0, 90, 0, 25);
    instrument_filter(SL9_SDB, FILTER_LOW, 1400, 1);
    instrument_env(SL9_SDB, 0, ENV_PITCH, 0, 15, 4.0f);
    instrument(SL9_SDN, INSTR_NOISE, 0, 170, 0, 50);
    instrument_filter(SL9_SDN, FILTER_HIGH, 1400, 2);
    // toms — sine drop + a short click at the attack
    instrument(SL9_TOM, INSTR_SINE, 0, 220, 0, 45);
    instrument_env(SL9_TOM, 0, ENV_PITCH, 0, 60, 7.0f);
    instrument(SL9_TOMC, INSTR_NOISE, 0, 12, 0, 5);
    instrument_filter(SL9_TOMC, FILTER_BAND, 2000, 3);
    // rim — short woody dual ping
    instrument(SL9_RS, INSTR_TRI, 0, 28, 0, 10);
    instrument_filter(SL9_RS, FILTER_HIGH, 400, 3);
    // clap — bandpassed noise; retriggers in fire make the clap
    instrument(SL9_CP, INSTR_NOISE, 0, 100, 0, 45);
    instrument_filter(SL9_CP, FILTER_BAND, 1200, 5);
    // hats/cymbals — the FM-clang ROM stand-ins: 3.5 detent, feedback high,
    // closing highpass (negative ENV_CUTOFF) = the sampled sizzle
    instrument(SL9_HHC, INSTR_FM, 0, 40, 0, 16);
    instrument_harmonics(SL9_HHC, 0.55f);
    instrument_timbre(SL9_HHC, 0.90f);
    instrument_morph(SL9_HHC, 0.85f);
    instrument_env(SL9_HHC, 0, ENV_CUTOFF, 0, 28, -5000.0f);
    instrument(SL9_HHO, INSTR_FM, 0, 380, 0, 110);
    instrument_harmonics(SL9_HHO, 0.55f);
    instrument_timbre(SL9_HHO, 0.90f);
    instrument_morph(SL9_HHO, 0.85f);
    instrument_env(SL9_HHO, 0, ENV_CUTOFF, 0, 220, -4500.0f);
    instrument_choke(SL9_HHC, SL9_HHO);
    instrument(SL9_CC, INSTR_FM, 0, 1000, 0, 280);        // crash: clang + wash
    instrument_harmonics(SL9_CC, 0.55f);
    instrument_timbre(SL9_CC, 1.00f);
    instrument_morph(SL9_CC, 0.90f);
    instrument(SL9_CCN, INSTR_NOISE, 0, 1200, 0, 320);
    instrument(SL9_RC, INSTR_FM, 0, 550, 0, 160);         // ride: cleaner ping
    instrument_harmonics(SL9_RC, 0.55f);
    instrument_timbre(SL9_RC, 0.72f);
    instrument_morph(SL9_RC, 0.58f);
    apply_metal_filter();                                  // the pad owns the metal highpasses
}

static void define_808(void) {
    // kick — the boom: low sine, +26st drop over 50ms, run a little hot
    instrument(SL8_BD, INSTR_SINE, 0, 480, 0, 60);
    instrument_filter(SL8_BD, FILTER_LOW, 250, 3);
    instrument_env(SL8_BD, 0, ENV_PITCH, 0, 50, 26.0f);
    instrument_drive(SL8_BD, 0.28f);
    // snare — 180+330Hz modes + snappy noise
    instrument(SL8_SDB, INSTR_SINE, 0, 100, 0, 30);
    instrument_filter(SL8_SDB, FILTER_LOW, 1200, 1);
    instrument_env(SL8_SDB, 0, ENV_PITCH, 0, 20, 3.0f);
    instrument(SL8_SDN, INSTR_NOISE, 0, 130, 0, 40);
    instrument_filter(SL8_SDN, FILTER_HIGH, 1800, 2);
    // toms — sine drop + a low noise thud
    instrument(SL8_TOM, INSTR_SINE, 0, 260, 0, 50);
    instrument_env(SL8_TOM, 0, ENV_PITCH, 0, 80, 6.0f);
    instrument(SL8_TOMN, INSTR_NOISE, 0, 28, 0, 12);
    instrument_filter(SL8_TOMN, FILTER_BAND, 900, 2);
    // clap
    instrument(SL8_CP, INSTR_NOISE, 0, 110, 0, 50);
    instrument_filter(SL8_CP, FILTER_BAND, 1100, 5);
    // maracas
    instrument(SL8_MAR, INSTR_NOISE, 0, 24, 0, 10);
    instrument_filter(SL8_MAR, FILTER_HIGH, 6500, 2);
    // cowbell — square pair through the classic ~2.6kHz bandpass
    instrument(SL8_CB, INSTR_SQUARE, 0, 250, 0, 60);
    instrument_filter(SL8_CB, FILTER_BAND, 2640, 5);
    // hats — bank squares through ~7kHz highpass; open vs closed = decay
    instrument(SL8_HATO, INSTR_SQUARE, 0, 340, 0, 90);
    instrument_filter(SL8_HATO, FILTER_HIGH, 7000, 3);
    instrument(SL8_HATC, INSTR_SQUARE, 0, 42, 0, 16);
    instrument_filter(SL8_HATC, FILTER_HIGH, 7000, 3);
    instrument_choke(SL8_HATC, SL8_HATO);
    // congas — the tom circuit without the noise, cleaner + shorter
    instrument(SL8_CON, INSTR_SINE, 0, 150, 0, 30);
    instrument_env(SL8_CON, 0, ENV_PITCH, 0, 25, 4.0f);
    // rimshot — both bridged-T modes through a highpass (keeps 455 AND 1667)
    instrument(SL8_RS, INSTR_TRI, 0, 45, 0, 15);
    instrument_filter(SL8_RS, FILTER_HIGH, 500, 3);
    // claves — single 2500Hz ping
    instrument(SL8_CLV, INSTR_TRI, 0, 40, 0, 14);
    instrument_filter(SL8_CLV, FILTER_LOW, 4000, 5);
    // cymbal — bank squares through the 3440Hz region, very long ring
    instrument(SL8_CYT, INSTR_SQUARE, 0, 850, 0, 200);
    instrument_filter(SL8_CYT, FILTER_HIGH, 3440, 3);
}

static int flash909[N909], flash808[N808];    // header/panel activity flashes

// 8×8 rotary knob (tr909.c draw_knob): circle + a tick at the value angle
static void draw_miniknob(int x, int y, float val, int col) {
    int cx = x + 3, cy = y + 3;
    circ(cx, cy, 3, CLR_MEDIUM_GREY);
    float a = (135.0f + val * 270.0f) * (3.14159265f / 180.0f);
    pset(cx + (int)(cosf(a) * 2.5f + 0.5f), cy + (int)(sinf(a) * 2.5f + 0.5f), col);
}
// one mini-knob drag at a time (Y = coarse, X = fine — the tr909 feel)
static int   kdrag_id = -1, kdrag_mach, kdrag_v, kdrag_k, kdrag_x0, kdrag_y0;
static float *kdrag_val(void) {
    if (kdrag_mach == 9) return kdrag_k == 0 ? &kt9[kdrag_v] : kdrag_k == 1 ? &kd9[kdrag_v] : &kc9[kdrag_v];
    return kdrag_k == 0 ? &kt8[kdrag_v] : kdrag_k == 1 ? &kd8[kdrag_v] : &kc8[kdrag_v];
}

static void fire909(int v, int boost, int delay) {
    switch (v) {
    case V9_BD:  // ATTK = the click layer's level
        schedule_hit(delay, k_midi(kt9, v, 32), SL9_BD, vv(6, boost), k_dur(kd9, v, 320));
        schedule_hit(delay, 60, SL9_BDC, vv(k_cv(kc9, v, 0, 6), boost), 10);
        break;
    case V9_SD: {  // SNPY fades body <-> noise
        int body = k_cv(kc9, v, 8, 1), snpy = k_cv(kc9, v, 1, 8);
        schedule_hit(delay, k_midi(kt9, v, 54), SL9_SDB, vv(body, boost), k_dur(kd9, v, 95));
        schedule_hit(delay, k_midi(kt9, v, 64), SL9_SDB, vv(body - 1, boost), k_dur(kd9, v, 95));
        schedule_hit(delay, 60, SL9_SDN, vv(snpy, boost), k_dur(kd9, v, 180));
        break;
    }
    case V9_LT: case V9_MT: case V9_HT: {  // CLIK = the attack noise level
        int mm = v == V9_LT ? 42 : v == V9_MT ? 47 : 54;
        schedule_hit(delay, k_midi(kt9, v, mm), SL9_TOM, vv(5, boost), k_dur(kd9, v, 240));
        schedule_hit(delay, 70, SL9_TOMC, vv(k_cv(kc9, v, 0, 5), boost), 14);
        break;
    }
    case V9_RS:
        schedule_hit(delay, k_midi(kt9, v, 76), SL9_RS, vv(5, boost), k_dur(kd9, v, 30));
        schedule_hit(delay, k_midi(kt9, v, 64), SL9_RS, vv(3, boost), k_dur(kd9, v, 30));
        break;
    case V9_CP:
        schedule_hit(delay,      k_midi(kt9, v, 60), SL9_CP, vv(5, boost), 11);
        schedule_hit(delay + 9,  k_midi(kt9, v, 60), SL9_CP, vv(5, boost), 11);
        schedule_hit(delay + 18, k_midi(kt9, v, 60), SL9_CP, vv(5, boost), 11);
        schedule_hit(delay + 26, k_midi(kt9, v, 60), SL9_CP, vv(3, boost), k_dur(kd9, v, 130));
        break;
    case V9_CH: schedule_hit(delay, k_midi(kt9, v, 97), SL9_HHC, vv(4, boost), k_dur(kd9, v, 40));  break;
    case V9_OH: schedule_hit(delay, k_midi(kt9, v, 97), SL9_HHO, vv(4, boost), k_dur(kd9, v, 400)); break;
    case V9_CC: {  // TONE fades FM clang <-> noise wash
        schedule_hit(delay, k_midi(kt9, v, 84), SL9_CC,  vv(k_cv(kc9, v, 6, 1), boost), k_dur(kd9, v, 1100));
        schedule_hit(delay, 60, SL9_CCN, vv(k_cv(kc9, v, 1, 6), boost), k_dur(kd9, v, 1300));
        break;
    }
    case V9_RC:
        schedule_hit(delay, k_midi(kt9, v, 76), SL9_RC, vv(4, boost), k_dur(kd9, v, 600));
        schedule_hit(delay, k_midi(kt9, v, 83), SL9_RC, vv(2, boost), k_dur(kd9, v, 600));
        break;
    }
    flash909[v] = 6;
}

// the 909 stroke family: flam = one quiet grace 22ms early, drag = two,
// ratchet = four even hits chopped across the step (tr909.c fire_stroke)
static void fire_stroke909(int v, int st, int boost, int delay, int step_ms) {
    switch (st) {
    case ST_RATCHET:
        for (int k = 0; k < 4; k++) fire909(v, k ? boost - 2 : boost, delay + k * step_ms / 4);
        return;
    case ST_DRAG: fire909(v, boost - 4, delay - 2 * FLAM_MS); /* fall through */
    case ST_FLAM: fire909(v, boost - 3, delay - FLAM_MS);     /* fall through */
    default:      fire909(v, boost, delay);
    }
}

static void fire808(int v, int boost, int delay) {
    switch (v) {
    case V8_BD: schedule_hit(delay, k_midi(kt8, v, 31), SL8_BD, vv(6, boost), k_dur(kd8, v, 500)); break;
    case V8_SD: {  // SNPY fades body <-> noise
        int body = k_cv(kc8, v, 8, 0), snpy = k_cv(kc8, v, 0, 8);
        schedule_hit(delay, k_midi(kt8, v, 54), SL8_SDB, vv(body, boost), k_dur(kd8, v, 110));
        schedule_hit(delay, k_midi(kt8, v, 64), SL8_SDB, vv(body, boost), k_dur(kd8, v, 110));
        schedule_hit(delay, 60, SL8_SDN, vv(snpy, boost), k_dur(kd8, v, 140));
        break;
    }
    case V8_LT: case V8_MT: case V8_HT: {  // THUD = the noise thud level
        int mm = v == V8_LT ? 40 : v == V8_MT ? 45 : 52;
        schedule_hit(delay, k_midi(kt8, v, mm), SL8_TOM, vv(4, boost), k_dur(kd8, v, 280));
        schedule_hit(delay, 60, SL8_TOMN, vv(k_cv(kc8, v, 0, 5), boost), 30);
        break;
    }
    case V8_LC: case V8_MC: case V8_HC: {  // congas — same circuit, cleaner + shorter
        int mm = v == V8_LC ? 52 : v == V8_MC ? 57 : 63;
        schedule_hit(delay, k_midi(kt8, v, mm), SL8_CON, vv(4, boost), k_dur(kd8, v, 160));
        break;
    }
    case V8_RS:  // dual bridged-T: 1667Hz (midi 92) + 455Hz (midi 70)
        schedule_hit(delay, k_midi(kt8, v, 92), SL8_RS, vv(4, boost), k_dur(kd8, v, 50));
        schedule_hit(delay, k_midi(kt8, v, 70), SL8_RS, vv(3, boost), k_dur(kd8, v, 50));
        break;
    case V8_CLV: // the rimshot circuit retuned to 2500Hz = midi 99 exactly
        schedule_hit(delay, k_midi(kt8, v, 99), SL8_CLV, vv(4, boost), k_dur(kd8, v, 45));
        break;
    case V8_CY:  // TONE fades the three metal detents (bright = brighter)
        schedule_hit(delay, k_midi(kt8, v, 79), SL8_CYT, vv(k_cv(kc8, v, 0, 6), boost), k_dur(kd8, v, 900));
        schedule_hit(delay, k_midi(kt8, v, 72), SL8_CYT, vv(2, boost), k_dur(kd8, v, 900));
        schedule_hit(delay, k_midi(kt8, v, 66), SL8_CYT, vv(k_cv(kc8, v, 5, 0), boost), k_dur(kd8, v, 900));
        break;
    case V8_CP:
        schedule_hit(delay,      k_midi(kt8, v, 60), SL8_CP, vv(4, boost), 12);
        schedule_hit(delay + 10, k_midi(kt8, v, 60), SL8_CP, vv(4, boost), 12);
        schedule_hit(delay + 20, k_midi(kt8, v, 60), SL8_CP, vv(4, boost), 12);
        schedule_hit(delay + 28, k_midi(kt8, v, 60), SL8_CP, vv(3, boost), k_dur(kd8, v, 140));
        break;
    case V8_MA: schedule_hit(delay, k_midi(kt8, v, 90), SL8_MAR, vv(3, boost), k_dur(kd8, v, 30)); break;
    case V8_CB:  // TONE fades 540Hz <-> 800Hz emphasis
        schedule_hit(delay, k_midi(kt8, v, 73), SL8_CB, vv(k_cv(kc8, v, 7, 0), boost), k_dur(kd8, v, 220));
        schedule_hit(delay, k_midi(kt8, v, 79), SL8_CB, vv(k_cv(kc8, v, 0, 7), boost), k_dur(kd8, v, 220));
        break;
    case V8_OH: {  // RING fades warm <-> bright
        schedule_hit(delay, k_midi(kt8, v, 79), SL8_HATO, vv(k_cv(kc8, v, 0, 6), boost), k_dur(kd8, v, 360));
        schedule_hit(delay, k_midi(kt8, v, 72), SL8_HATO, vv(k_cv(kc8, v, 5, 0), boost), k_dur(kd8, v, 360));
        break;
    }
    case V8_CH:
        schedule_hit(delay, k_midi(kt8, v, 79), SL8_HATC, vv(3, boost), k_dur(kd8, v, 50));
        schedule_hit(delay, k_midi(kt8, v, 72), SL8_HATC, vv(2, boost), k_dur(kd8, v, 50));
        break;
    }
    flash808[v] = 6;
}

// ── the accordion ─────────────────────────────────────────────────────────
enum { STRIP_A, STRIP_B, STRIP_909, STRIP_808, STRIP_MST, NSTRIP };
static const char *SNAME[NSTRIP] = { "303-A", "303-B", "909", "808", "MASTER" };
static int  expanded = STRIP_A;
static bool mute[NSTRIP];
static bool fxview[NSTRIP];            // machine strips: panel shows FX instead of the grid

// layout — transport bar / strips / song chain row
// 24 + 5×18 + 114 = 228 = CHAIN_Y, chain ends 240: exactly flush
#define TB_H    22
#define HDR_H   18       // the OPEN strip's header — carries the fx + mute buttons
#define HDRC    15       // a FOLDED strip's slim header — name + mini-pattern + mute square
#define FX_BTN_X 62      // open-strip fx/seq toggle — tucked between the name and the mini-pattern
#define TAB_H   16       // tab-strip height on a short (landscape) screen
#define PANEL_H 114
#define CHAIN_Y 228

// ── device-adaptive (Phase 3, design/device-adaptive-layout.md) ─────────────
// The rack was authored at a fixed 320×240; these make the HORIZONTAL layout
// fill screen_w() live (+ a 2-row transport when narrow), and pin the chain row
// to the real bottom. Vertical model unchanged for now (single accordion); the
// short-landscape "tabs" arrangement is the next increment.
static int W(void) { return screen_w(); }
// device safe-area insets (notch / home-bar); 0 on desktop + fixed builds → layout unchanged there.
// The chrome inset by these: transport drops below the notch, the chain row lifts above the home bar.
static int saf_t(void) { int t = 0; safe_rect(0, &t, 0, 0); return t; }
static int saf_b(void) { int y = 0, h = 0; safe_rect(0, &y, 0, &h); return screen_h() - y - h; }
static int tb_h(void) { return saf_t() + (W() < 300 ? TB_H * 2 : TB_H); }   // + top inset; narrow → 2 rows
static int chain_y(void) { return screen_h() - saf_b() - 12; }             // pinned above the home bar
// px per step to fill a grid from x0 up to (W - right_margin); floored so it never vanishes
static int spitch(int x0, int right_margin) { int p = (W() - right_margin - x0) / STEPS; return p < 4 ? 4 : p; }
// shared layout anchors — draw() AND update() both call these so visuals and
// hit-tests can never drift apart as the canvas resizes.
static bool rack_wide(void) { return W() >= 300; }              // room for the drum voice-knob columns
static int drum_pitch(void) { return spitch(36, rack_wide() ? 74 : 4); }  // 909/808 step cell pitch
static int roll_pitch(void) { return spitch(40, 4); }           // 303 piano-roll / flag step pitch
static int pcf_pitch(void) { return spitch(36, 76); }           // master PCF lane step pitch
static int chain_pitch(void) { int p = (W() - 38) / CHAINN; return p < 2 ? 2 : p; }
static int code_x(void) { return W() - 72; }                    // song-code + GEN/WAV column (master)
static int vknob_x(void) { return W() - 72; }                   // drum per-voice TUNE/DEC/CHAR columns
static int mpad_x(void) { return W() - 130; }                   // 909 metal XY pad
// expanded-panel height: fills the space left after transport + all headers +
// the chain row, so a tall portrait screen gives the open panel real room (and
// the knob row can wrap) — clamped so it never collapses on a short screen.
static int hdr_h(int i) { return i == expanded ? HDR_H : HDRC; }   // open header full; folded rows slim
static int cmute_x(void) { return W() - 30; }                      // folded-row mute tap zone (left edge)
// A short screen (a landscape phone) can't fit the stacked accordion headers PLUS
// an open panel, so below this height we switch to a TAB STRIP (device-adaptive-
// layout.md: "accordion degenerates on a short screen → tabs").
// switch to tabs when the accordion's open panel can't actually fit its content
// (the 303 is tallest: knob block — 1 or 2 rows by width — + roll + flag rows).
// Content-aware, so an awkward narrow-and-short size flips to tabs instead of
// overflowing its neighbours (caught by ui-audit --explore --resize).
static int accordion_panel_h(void) { return screen_h() - tb_h() - (HDR_H + (NSTRIP - 1) * HDRC) - 14 - saf_b(); }
static int panel_content_need(void) { int knobs = (W() - 12 >= NKMAX * 26) ? 13 : 35; return knobs + 13 * 5 + 4 * 7 + 6 + 8; }  // roll + 4 flag rows + LEN strip
static bool use_tabs(void) { return accordion_panel_h() < panel_content_need(); }
static int  tab_rail(void) { return 30; }                     // right end of the tab strip = the fx toggle
static int  tab_w(void)    { return (W() - 2 - tab_rail()) / NSTRIP; }
static int panel_h(void) {
    if (use_tabs()) return screen_h() - tb_h() - TAB_H - 14 - saf_b();  // transport + tab strip + chain
    int headers = HDR_H + (NSTRIP - 1) * HDRC;
    int h = screen_h() - tb_h() - headers - 14 - saf_b(); return h < 60 ? 60 : h;
}
// knob row → grid when the panel is too narrow for one comfortable row (§6.5).
// One comfortable row if it fits; otherwise TWO rows (ceil(n/2) columns), which
// is what a phone-portrait rack wants — not an emergent column count.
static int knob_cols(int n) { if (W() - 12 >= n * 26) return n; int c = (n + 1) / 2; return c < 1 ? 1 : c; }
static int knob_block_h(int n) { int cols = knob_cols(n); int rows = (n + cols - 1) / cols; return (rows - 1) * 22 + 13; }
static int roll_top(int y0) { return y0 + 12 + knob_block_h(NKMAX); }   // 303 roll sits below the (taller) knob page

static int strip_y(int i) {            // header top of strip i
    int y = tb_h() + 2;
    for (int k = 0; k < i; k++) y += hdr_h(k) + (k == expanded ? panel_h() : 0);
    return y;
}

// the expanded panel's top-left Y in the CURRENT layout mode — both draw() and
// update() use this so the panel visuals and hit-tests stay in lockstep.
static int panel_y0(void) {
    if (use_tabs()) return tb_h() + 2 + TAB_H;     // below the tab strip
    return strip_y(expanded) + HDR_H;              // accordion: below the open header
}

// ── drum-grid vertical fit ──────────────────────────────────────────────────
// The 808 is 16 voices; at the fixed 10px stride those + the AC row overflow a
// 240-tall screen. So while the 808 panel is open we HIDE the song chain row
// (song playback is unaffected — it reads chain[], not the row) to reclaim its
// ~14px, and shrink the row height to fit whatever's left. The 909 (11 voices)
// still fits above the chain row, so it keeps both. Both draw() and update()
// call these so visuals and hit-tests stay in lockstep.
static bool chain_hidden(void) { return expanded == STRIP_808; }
static int  drum_gy_of(int mach) { return panel_y0() + (mach == STRIP_909 ? 4 : 6); }
static int  drum_rh_of(int mach, int nv) {
    int bottom = (mach == STRIP_808 ? screen_h() - saf_b() : chain_y()) - 2;
    int rh = (bottom - drum_gy_of(mach)) / (nv + 1);   // +1 leaves a row for AC
    int cap = mach == STRIP_909 ? 9 : 10;
    return rh < 7 ? 7 : rh > cap ? cap : rh;
}

// ── the FX rack — per-DEVICE like the RB-338, master where it must be ─────
// Each machine strip has an [FX] view: DIST = per-voice instrument_drive on
// that machine's slots (no bus grab), SEND = its level into THE one shared
// delay unit (instrument_echo — ReBirth had one delay unit + per-device
// routing too). The MASTER strip keeps what genuinely needs a bus today:
// the delay unit's TIME/FB, the GLUE comp, and the PCF + its lane. True
// per-device PCF/comp needs machine/group buses — banked as Increment G in
// effects-bus-architecture.md (maker thinking on it, 2026-07-02).
enum { F_TIME, F_FB, F_GLU, F_PCF, F_RES, NFX };   // the MASTER strip's knobs
static const char *FXNAME[NFX] = { "TIME", "FB", "GLU", "PCF", "RES" };
static float fxk[NFX] = { 0.50f, 0.35f, 0.30f, 0.40f, 0.35f };
// master knobs live in the LEFT column (the song code owns the right); wrap them
// to a 2nd row when that column is too narrow — mirrors the 303's knob wrap.
// mst_lane_y() drops the PCF lane below the knob block; draw + update share it.
static int mst_cols(void)     { int avail = W() - 84; return avail >= NFX * 26 ? NFX : (NFX + 1) / 2; }
static int mst_lane_y(int y0) { int rows = (NFX + mst_cols() - 1) / mst_cols(); return y0 + 12 + (rows - 1) * 22 + 24; }
static float send[4] = { 0.17f, 0.17f, 0.00f, 0.00f };   // per-machine delay send (A, B, 909, 808)
static float dist9 = 0.0f, dist8 = 0.0f;                 // per-drum-machine drive
static bool  pcf_on = false;           // is the master filter currently engaged

static const int SLOTS909[13] = { SL9_BD, SL9_BDC, SL9_SDB, SL9_SDN, SL9_TOM, SL9_TOMC,
                                  SL9_RS, SL9_CP, SL9_HHC, SL9_HHO, SL9_CC, SL9_CCN, SL9_RC };
static const int SLOTS808[14] = { SL8_BD, SL8_SDB, SL8_SDN, SL8_TOM, SL8_TOMN,
                                  SL8_CP, SL8_MAR, SL8_CB, SL8_HATO, SL8_HATC,
                                  SL8_CON, SL8_RS, SL8_CLV, SL8_CYT };

// re-apply set-and-hold effects ONLY when a value moved (groovebox idiom)
static void apply_fx(void) {
    static float aTime = -1, aFb = -1, aGlue = -1, aS[4] = { -1, -1, -1, -1 }, aD9 = -1, aD8 = -1;
    static int   aTempo = -1;
    // THE delay unit — the shared echo SEND bus (one unit, per-device routing,
    // like the hardware). fb capped ≤0.72: near-unity + heavy sends made a
    // sustained wall that held the glue in full gain-reduction (2026-07-02)
    if (fxk[F_TIME] != aTime || fxk[F_FB] != aFb || tempo != aTempo) {
        static const float DIV[4] = { 0.25f, 0.5f, 0.75f, 1.0f };   // 1/16 · 1/8 · dotted · 1/4
        int ms = (int)(60000.0f / tempo * DIV[(int)(fxk[F_TIME] * 3.999f)]);
        echo(ms, fxk[F_FB] * 0.72f, 0.45f);
        aTime = fxk[F_TIME]; aFb = fxk[F_FB]; aTempo = tempo;
    }
    // per-machine sends into it
    // sends capped ×0.6 (like the drums): a full-wet mono 303 into fb 0.72
    // sustains a ~3.5× energy loop that slams the master limiter
    if (send[0] != aS[0]) { instrument_echo(SLOT_A, send[0] * 0.6f); aS[0] = send[0]; }
    if (send[1] != aS[1]) { instrument_echo(SLOT_B, send[1] * 0.6f); aS[1] = send[1]; }
    if (send[2] != aS[2]) { for (int i = 0; i < 13; i++) instrument_echo(SLOTS909[i], send[2] * 0.6f); aS[2] = send[2]; }
    if (send[3] != aS[3]) { for (int i = 0; i < 14; i++) instrument_echo(SLOTS808[i], send[3] * 0.6f); aS[3] = send[3]; }
    // per-drum-machine DIST — per-voice saturation layered over the baked
    // kick drives (909 BD 0.35, 808 BD 0.28), no bus involved at all
    if (dist9 != aD9) {
        for (int i = 0; i < 13; i++) {
            float baked = SLOTS909[i] == SL9_BD ? 0.35f : 0.0f;
            instrument_drive(SLOTS909[i], baked + dist9 * (0.85f - baked));
        }
        aD9 = dist9;
    }
    if (dist8 != aD8) {
        for (int i = 0; i < 14; i++) {
            float baked = SLOTS808[i] == SL8_BD ? 0.28f : 0.0f;
            instrument_drive(SLOTS808[i], baked + dist8 * (0.85f - baked));
        }
        aD8 = dist8;
    }
    // GLUE — master (needs a bus; Increment G would make it per-device).
    // knob maxes at HEAVY GLUE, never a mute (amount 1.0 could duck the mix
    // to silence under a sustained loud tail — measured 2026-07-02)
    float glu = mute[STRIP_MST] ? 0.0f : fxk[F_GLU];
    if (glu != aGlue) {
        glue(0, glu < 0.02f ? 0.0f : glu * 0.55f, 8, 150);
        aGlue = glu;
    }
}

// the PCF — master filter() ridden from the playing bank's lane (ride-safe;
// only the OFF↔ON mode flip is gated). Per-device PCF waits on Increment G.
static void ride_pcf(void) {
    float depth = mute[STRIP_MST] ? 0.0f : fxk[F_PCF];
    if (depth < 0.02f) {
        if (pcf_on) { filter(FILTER_OFF, 0.0f, 0.0f); pcf_on = false; }
        return;
    }
    // anchored OPEN, depth pulls DOWN: at low depth the filter hovers near
    // transparent and the lane barely tugs it; at full depth a low lane
    // level reaches ~250Hz. (The first mapping anchored at 250Hz and opened
    // upward — engaging the knob was an instant muffle cliff, maker-reported)
    int lvl = bank[playBank].pcf[playhead];
    float cut = 9000.0f / powf(2.0f, (1.0f - lvl / 7.0f) * depth * 5.2f);
    filter(FILTER_LOW, cut, 0.15f + 0.75f * fxk[F_RES]);
    pcf_on = true;
}

// ── save / load (autosaves the whole song) ────────────────────────────────
#define SAVE_MAGIC 0xAC1D000Au   // v10: 303 octave-DOWN + TIE + per-line LENGTH (vanilla tb303 parity) — Line struct grew, old saves ignored
typedef struct {
    unsigned magic;
    Pattern  bank[NBANK];
    unsigned char chain[CHAINN];
    int      chainN, tempo, editBank, swing;
    unsigned cur_seed;
    float    knob[2][NK], mcut, mres, fxk[NFX], send[4], dist9, dist8;
    float    kt9[N909], kd9[N909], kc9[N909], kt8[N808], kd8[N808], kc8[N808];
    int      wave[2], sweep[2];
    bool     songmode, mute[NSTRIP];
} SaveBlob;
static int  save_cooldown = 0;         // >0 → a save is pending
static void mark_dirty(void) { save_cooldown = 45; }
static void save_song(void) {
    SaveBlob sb; memset(&sb, 0, sizeof sb);
    sb.magic = SAVE_MAGIC;
    memcpy(sb.bank, bank, sizeof bank);
    memcpy(sb.chain, chain, sizeof chain);
    sb.chainN = chainN; sb.tempo = tempo; sb.editBank = editBank; sb.swing = swing;
    sb.cur_seed = cur_seed;
    sb.mcut = mcut; sb.mres = mres;
    memcpy(sb.fxk, fxk, sizeof fxk);
    memcpy(sb.send, send, sizeof send); sb.dist9 = dist9; sb.dist8 = dist8;
    memcpy(sb.kt9, kt9, sizeof kt9); memcpy(sb.kd9, kd9, sizeof kd9); memcpy(sb.kc9, kc9, sizeof kc9);
    memcpy(sb.kt8, kt8, sizeof kt8); memcpy(sb.kd8, kd8, sizeof kd8); memcpy(sb.kc8, kc8, sizeof kc8);
    for (int i = 0; i < 2; i++) { memcpy(sb.knob[i], m[i].knob, sizeof m[i].knob); sb.wave[i] = m[i].wave; sb.sweep[i] = m[i].sweep; }
    sb.songmode = songmode;
    memcpy(sb.mute, mute, sizeof mute);
    save_bytes(&sb, sizeof sb);
}
static bool load_song(void) {
    SaveBlob sb;
    if (load_bytes(&sb, sizeof sb) != sizeof sb || sb.magic != SAVE_MAGIC) return false;
    memcpy(bank, sb.bank, sizeof bank);
    memcpy(chain, sb.chain, sizeof chain);
    chainN = sb.chainN; tempo = sb.tempo; editBank = sb.editBank; swing = sb.swing;
    cur_seed = sb.cur_seed;
    swingf = (swing - 50) / 16.0f;
    mcut = sb.mcut; mres = sb.mres;
    memcpy(fxk, sb.fxk, sizeof fxk);
    memcpy(send, sb.send, sizeof send); dist9 = sb.dist9; dist8 = sb.dist8;
    memcpy(kt9, sb.kt9, sizeof kt9); memcpy(kd9, sb.kd9, sizeof kd9); memcpy(kc9, sb.kc9, sizeof kc9);
    memcpy(kt8, sb.kt8, sizeof kt8); memcpy(kd8, sb.kd8, sizeof kd8); memcpy(kc8, sb.kc8, sizeof kc8);
    for (int i = 0; i < 2; i++) { memcpy(m[i].knob, sb.knob[i], sizeof m[i].knob); m[i].wave = sb.wave[i]; m[i].sweep = sb.sweep[i]; }
    songmode = sb.songmode;
    memcpy(mute, sb.mute, sizeof sb.mute);
    return true;
}

// ── init ──────────────────────────────────────────────────────────────────
void init(void) {
    for (int v = 0; v < N909; v++) kt9[v] = kd9[v] = kc9[v] = 0.5f;
    for (int v = 0; v < N808; v++) kt8[v] = kd8[v] = kc8[v] = 0.5f;
    if (!load_song()) gen_song(DEFAULT_SEED);   // first boot: a generated song
    define_303(&m[0]);
    define_303(&m[1]);
    define_909();
    define_808();
    bpm(tempo);
    // master insert chain — fx_order REPLACES the chain, so every pedal the
    // rack uses must be listed. Only the PCF filter lives on master now;
    // dist + delay are per-device (apply_fx configures the delay UNIT + sends)
    static const int kinds[1] = { FX_FILTER };
    fx_order(0, kinds, 1);
    apply_fx();
}

// ── the composition seed (lifted from runtime/radio.h — provenance comment;
// no cross-cart seed compat needed: acidrack codes only reproduce acidrack,
// but changing the generator's DRAW ORDER orphans every noted code — same
// discipline as the radio.h seed-compatibility rule) ──────────────────────
typedef struct {
    unsigned rngState, seed;
    unsigned hist[64];
    int      histN, histPos;
} RadioSeed;
static unsigned rad_srnd_u(RadioSeed *r) {
    r->rngState ^= r->rngState << 13;
    r->rngState ^= r->rngState >> 17;
    r->rngState ^= r->rngState << 5;
    return r->rngState;
}
static int rad_srnd(RadioSeed *r, int n) { return (int)(rad_srnd_u(r) % (unsigned)n); }
static unsigned rad_seed_begin(RadioSeed *r, unsigned seed) {
    if (!seed) seed = ((unsigned)rnd(0x10000) << 16) ^ (unsigned)rnd(0x10000)
                      ^ (unsigned)frame() * 2654435761u;
    if (!seed) seed = 1;
    r->rngState = r->seed = seed;
    return seed;
}
static void rad_hist_log(RadioSeed *r) {
    if (r->histN == 64) { for (int i = 1; i < 64; i++) r->hist[i - 1] = r->hist[i]; r->histN--; }
    r->hist[r->histN++] = r->seed;
    r->histPos = r->histN - 1;
}
static unsigned rad_hist_back(RadioSeed *r) { return r->histPos > 0 ? r->hist[--r->histPos] : 0; }
static unsigned rad_hist_fwd(RadioSeed *r)  { return r->histPos < r->histN - 1 ? r->hist[++r->histPos] : 0; }

static RadioSeed rs;

// ── THE GENERATOR — one song code fills all four banks + the chain ────────
// The arc the banks encode: A sparse intro → B groove → C build → D drop
// (tinyjam-racks.md "sections become pattern banks"). Musicality is tb303's
// gen_random recipe; discipline is the radio idiom: every compositional
// draw comes from the seeded stream, in this fixed order.
static int schance(int pct) { return rad_srnd(&rs, 100) < pct; }

static void gen_line(Line *ln, int density) {      // one 16-step acid line
    static const int pool[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };
    memset(ln, 0, sizeof *ln);
    int prev = 0;
    for (int st = 0; st < STEPS; st++) {
        if (!schance(density)) continue;
        int b = 1 << st;
        ln->on |= b;
        int pc = schance(35) ? prev : pool[rad_srnd(&rs, 8)];
        ln->pitch[st] = (unsigned char)pc; prev = pc;
        if (schance(15)) { if (schance(40)) ln->octd |= b; else ln->oct |= b; }  // octave down OR up
        if (schance(30)) ln->acc |= b;
        if (schance(25)) ln->sld |= b;
    }
    ln->on |= 1; ln->pitch[0] = 0;                 // land on the root
    for (int st = 1; st < STEPS; st++)             // sprinkle ties onto rests → the prior note sustains (a 303 signature)
        if (!((ln->on >> st) & 1) && schance(18)) ln->tie |= 1 << st;
}
static Line thin_line(const Line *src, int keep_pct) {   // per-bank density derive
    Line ln = *src;
    for (int st = 1; st < STEPS; st++)             // step 0 (the root) always survives
        if ((ln.on >> st) & 1 && !schance(keep_pct)) ln.on &= ~(1 << st);
    return ln;
}

enum { G_INTRO, G_GROOVE, G_BUILD, G_DROP };
static void gen_drums(Pattern *P, int stage) {
    for (int v = 0; v < N909; v++) { P->d909[v] = 0; memset(P->st909[v], 0, STEPS); }
    for (int v = 0; v < N808; v++) P->d808[v] = 0;
    P->acc909 = 1; P->acc808 = 0;
    for (int st = 0; st < STEPS; st += 4) P->d909[V9_BD] |= 1 << st;      // the floor
    if (stage == G_INTRO) {                        // kick + one garnish, air everywhere
        if (schance(30)) P->d808[V8_CB] = 0x4949;
        return;
    }
    bool clap = schance(65);                       // the groove core (shared B/C/D)
    P->d909[clap ? V9_CP : V9_SD] = (1 << 4) | (1 << 12);
    bool off8 = schance(60);
    if (off8) for (int st = 2; st < STEPS; st += 4) P->d909[V9_CH] |= 1 << st;
    else      for (int st = 0; st < STEPS; st += 2) P->d909[V9_CH] |= 1 << st;
    if (schance(45)) P->d808[V8_MA] = 0xFFFF;
    if (schance(30)) P->d808[V8_CB] = 0x4949;
    if (stage == G_BUILD) {
        P->d909[V9_CH] = 0xFFFF;                   // hats go 16ths
        P->d909[V9_BD] |= (1 << 14) | (schance(50) ? 1 << 15 : 0);   // the turn doubles
        int rst = 12 + rad_srnd(&rs, 4);           // ratchet into the drop
        P->d909[V9_CH] |= 1 << rst; P->st909[V9_CH][rst] = ST_RATCHET;
        if (schance(50)) P->d909[V9_LT] |= 1 << (12 + rad_srnd(&rs, 4));
        if (schance(35)) P->d909[V9_HT] |= 1 << (12 + rad_srnd(&rs, 4));
        P->acc909 = (1 << 0) | (1 << 8);
    }
    if (stage == G_DROP) {
        for (int st = 2; st < STEPS; st += 4) P->d909[V9_OH] |= 1 << st;
        if (schance(40)) for (int st = 0; st < STEPS; st += 8) P->d909[V9_RC] |= 1 << st;
        P->d909[V9_CC] |= 1;                       // crash the downbeat
        if (schance(30)) { int st = 12 + rad_srnd(&rs, 3); P->st909[clap ? V9_CP : V9_SD][st] = ST_FLAM;
                           P->d909[clap ? V9_CP : V9_SD] |= 1 << st; }
        P->acc909 = 0x1111;                        // every downbeat leans in
    }
}
static void gen_pcf(Pattern *P, int stage) {
    for (int st = 0; st < STEPS; st++) {
        int lvl = stage == G_INTRO  ? 2 + rad_srnd(&rs, 2)
                : stage == G_GROOVE ? 4 + ((st & 2) ? 1 : 0)
                : stage == G_BUILD  ? st * 7 / 15
                :                     7;
        P->pcf[st] = (unsigned char)lvl;
    }
}

static void gen_song(unsigned seed) {
    cur_seed = rad_seed_begin(&rs, seed);
    if (!seed) rad_hist_log(&rs);                  // only FRESH rolls enter history
    memset(bank, 0, sizeof bank);
    // 1. global choices (fixed draw order — changing it orphans noted codes)
    tempo = 124 + rad_srnd(&rs, 17);
    m[0].wave = schance(70) ? INSTR_SAW : INSTR_SQUARE;
    m[1].wave = schance(45) ? INSTR_SAW : INSTR_SQUARE;
    swing = schance(25) ? 54 : 50;
    swingf = (swing - 50) / 16.0f;
    // 2. the two master lines: A carries the song, B answers sparser
    Line la, lb;
    gen_line(&la, 72);
    gen_line(&lb, 45);
    // 3. the four banks — the arrangement made visible
    bank[0].ln[0] = thin_line(&la, 45);                          // A: intro murmur
    gen_drums(&bank[0], G_INTRO);  gen_pcf(&bank[0], G_INTRO);
    bank[1].ln[0] = la;                                          // B: the groove
    bank[1].ln[1] = thin_line(&lb, 70);
    gen_drums(&bank[1], G_GROOVE); gen_pcf(&bank[1], G_GROOVE);
    bank[2].ln[0] = la;                                          // C: the build
    bank[2].ln[0].oct |= 0xFF00 & bank[2].ln[0].on;              //   A jumps the octave into the turn
    bank[2].ln[1] = lb;
    gen_drums(&bank[2], G_BUILD);  gen_pcf(&bank[2], G_BUILD);
    bank[3].ln[0] = la;                                          // D: the drop
    bank[3].ln[0].acc |= 0x1111 & bank[3].ln[0].on;
    bank[3].ln[1] = lb;
    gen_drums(&bank[3], G_DROP);   gen_pcf(&bank[3], G_DROP);
    // 4. the chain — intro · groove · build · drop · breath · build · big drop
    memset(chain, 0xFF, sizeof chain);
    chainN = 0;
    int put; 
    for (put = 2 + rad_srnd(&rs, 3); put-- > 0;) chain[chainN++] = 0;
    for (put = 4; put-- > 0;) chain[chainN++] = 1;
    for (put = 2; put-- > 0;) chain[chainN++] = 2;
    for (put = 4; put-- > 0;) chain[chainN++] = 3;
    for (put = 2; put-- > 0;) chain[chainN++] = schance(50) ? 0 : 1;
    for (put = 2; put-- > 0;) chain[chainN++] = 2;
    for (put = 2 + rad_srnd(&rs, 4); put-- > 0;) chain[chainN++] = 3;
    // 5. make it sound
    define_303(&m[0]);
    define_303(&m[1]);
    bpm(tempo);
    barpos = 0;
    mark_dirty();
}

// ── WAV export — arms the engine's live capture (.bake/wav_request:
// line 1 = path, line 2 = seconds; studio.c polls it in any native build)
static int  export_flash = 0;          // frames left on the "exporting" note
static char export_name[40];
static void export_wav(void) {
    float bar_s = 240.0f / tempo;
    float secs = (songmode && chainN > 0 ? chainN : 4) * bar_s + 2.0f;
    if (secs > 60.0f) secs = 60.0f;    // the engine cap
    snprintf(export_name, sizeof export_name, "acidrack-%08X.wav", cur_seed);
    FILE *f = fopen(".bake/wav_request", "w");
    if (!f) return;
    fprintf(f, "%s\n%.1f\n", export_name, secs);
    fclose(f);
    export_flash = 240;
    // start the song from the top so the capture gets the whole arrangement
    barpos = 0; last16 = -1; running = true;
}

// ── pattern ops (CPY/CLR/RND) — scope is WHAT YOU'RE LOOKING AT: a machine
// strip expanded = that machine's lane of the edit bank; MASTER expanded =
// the whole bank. CPY arms, then tapping a bank button pastes there.
// (ReBirth had true per-instrument pattern banks + per-machine song lanes;
// this gives the per-instrument EDITING inside the scene model — the chain
// format can still grow to per-machine entries later, see rebirth-classic.md)
static bool copyArm = false;

static void rnd_303(Line *ln) {          // tb303.c gen_random's recipe
    static const int pool[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };
    memset(ln, 0, sizeof *ln);
    int prev = 0;
    for (int st = 0; st < STEPS; st++) {
        if (!chance(72)) continue;
        int b = 1 << st;
        ln->on |= b;
        int pc = chance(35) ? prev : pool[rnd_between(0, 8)];
        ln->pitch[st] = (unsigned char)pc; prev = pc;
        if (chance(15)) ln->oct |= b;
        if (chance(30)) ln->acc |= b;
        if (chance(25)) ln->sld |= b;
    }
    ln->on |= 1; ln->pitch[0] = 0;       // land on the root
}
static void rnd_909(Pattern *P) {
    for (int v = 0; v < N909; v++) { P->d909[v] = 0; memset(P->st909[v], 0, STEPS); }
    for (int st = 0; st < STEPS; st += 4) P->d909[V9_BD] |= 1 << st;          // the floor
    if (chance(25)) P->d909[V9_BD] |= 1 << 14;                                // turn kick
    bool clap = chance(60);
    P->d909[clap ? V9_CP : V9_SD] = (1 << 4) | (1 << 12);                     // 2 & 4
    if (chance(50)) for (int st = 2; st < STEPS; st += 4) P->d909[V9_CH] |= 1 << st;   // offbeats
    else            for (int st = 0; st < STEPS; st += 2) P->d909[V9_CH] |= 1 << st;   // 8ths
    if (chance(40)) for (int st = 2; st < STEPS; st += 4) P->d909[V9_OH] |= 1 << st;
    if (chance(20)) for (int st = 0; st < STEPS; st += 8) P->d909[V9_RC] |= 1 << st;
    if (chance(15)) P->d909[V9_LT] |= 1 << rnd_between(12, 16);               // a turn tom
    if (chance(12)) { int st = rnd_between(12, 16); P->d909[V9_CH] |= 1 << st; P->st909[V9_CH][st] = ST_RATCHET; }
    P->acc909 = (1 << 0) | (chance(50) ? 1 << 8 : 0);
}
static void rnd_808(Pattern *P) {
    for (int v = 0; v < N808; v++) P->d808[v] = 0;
    if (chance(40)) P->d808[V8_MA] = 0xFFFF;                                  // shaker 16ths
    if (chance(35)) P->d808[V8_CB] = 0x4949;                                  // the cowbell clave
    if (chance(20)) P->d808[V8_CH] = 0x5555 & ~P->d909[V9_CH];
    P->acc808 = 0;
}
static void rnd_pcf(Pattern *P) {
    int shape = rnd(3);
    for (int st = 0; st < STEPS; st++)
        P->pcf[st] = (unsigned char)(shape == 0 ? 3 + (st & 1) * 2            // gentle pump
                                   : shape == 1 ? st * 7 / 15                 // the ramp
                                   : 5 + ((st & 2) ? 2 : 0));                 // open wave
}
static void clr_scope(Pattern *P) {
    if (expanded < 0) return;              // nothing open → no scope, no-op
    switch (expanded) {
    case STRIP_A: case STRIP_B: memset(&P->ln[expanded], 0, sizeof(Line)); break;
    case STRIP_909: for (int v = 0; v < N909; v++) { P->d909[v] = 0; memset(P->st909[v], 0, STEPS); } P->acc909 = 0; break;
    case STRIP_808: for (int v = 0; v < N808; v++) P->d808[v] = 0; P->acc808 = 0; break;
    default: memset(P, 0, sizeof *P); break;                                  // MASTER = the whole bank
    }
}
static void rnd_scope(Pattern *P) {
    if (expanded < 0) return;              // nothing open → no scope, no-op
    switch (expanded) {
    case STRIP_A: case STRIP_B: rnd_303(&P->ln[expanded]); break;
    case STRIP_909: rnd_909(P); break;
    case STRIP_808: rnd_808(P); break;
    default: rnd_303(&P->ln[0]); rnd_303(&P->ln[1]); rnd_909(P); rnd_808(P); rnd_pcf(P); break;
    }
}
static void copy_scope(Pattern *dst, const Pattern *src) {
    if (expanded < 0) return;              // nothing open → no scope, no-op
    switch (expanded) {
    case STRIP_A: case STRIP_B: dst->ln[expanded] = src->ln[expanded]; break;
    case STRIP_909:
        memcpy(dst->d909, src->d909, sizeof dst->d909);
        memcpy(dst->st909, src->st909, sizeof dst->st909);
        dst->acc909 = src->acc909; break;
    case STRIP_808: memcpy(dst->d808, src->d808, sizeof dst->d808); dst->acc808 = src->acc808; break;
    default: *dst = *src; break;                                              // MASTER = the whole bank
    }
}

// ── per-finger surface routing (the dubdesk fix, dubdesk.c:97) ────────────
// owned = the finger was captured by a ui.h widget → raw surfaces skip it
// until it lifts. seen = the finger existed last frame → !seen = a fresh tap.
#define NFING 12
static int  own_ids[NFING], own_n = 0;
static int  seen_ids[NFING], seen_n = 0;
static bool is_owned(int id)  { for (int i = 0; i < own_n; i++)  if (own_ids[i]  == id) return true; return false; }
static bool was_seen(int id)  { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) return true; return false; }
static void own_add(int id)   { if (!is_owned(id) && own_n < NFING) own_ids[own_n++] = id; }
static void seen_add(int id)  { if (!was_seen(id) && seen_n < NFING) seen_ids[seen_n++] = id; }
static void own_drop(int id)  { for (int i = 0; i < own_n; i++)  if (own_ids[i]  == id) { own_ids[i]  = own_ids[--own_n];   return; } }
static void seen_drop(int id) { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) { seen_ids[i] = seen_ids[--seen_n]; return; } }

// ── update ────────────────────────────────────────────────────────────────
static void stop_all(void) { off_303(&m[0]); off_303(&m[1]); }

void update(void) {
    // keys
    if (keyp(KEY_SPACE)) { running = !running; last16 = -1; if (!running) stop_all(); }
    if (keyp(KEY_UP))    { tempo += 2; if (tempo > 200) tempo = 200; bpm(tempo); mark_dirty(); }
    if (keyp(KEY_DOWN))  { tempo -= 2; if (tempo < 60)  tempo = 60;  bpm(tempo); mark_dirty(); }
    if (keyp(KEY_LEFT))  { editBank = (editBank + NBANK - 1) % NBANK; }
    if (keyp(KEY_RIGHT)) { editBank = (editBank + 1) % NBANK; }
    if (keyp('S'))       { songmode = !songmode; mark_dirty(); }
    if (keyp('G'))       gen_song(0);
    if (keyp('['))       { unsigned sd = rad_hist_back(&rs); if (sd) gen_song(sd); }
    if (keyp(']'))       { unsigned sd = rad_hist_fwd(&rs);  if (sd) gen_song(sd); }
    if (keyp('Z'))       { swing -= 2; if (swing < 50) swing = 50; swingf = (swing - 50) / 16.0f; mark_dirty(); }
    if (keyp('X'))       { swing += 2; if (swing > 66) swing = 66; swingf = (swing - 50) / 16.0f; mark_dirty(); }
    if (keyp('1')) expanded = STRIP_A;      // 1-5 jump straight to a strip (literal keys so
    if (keyp('2')) expanded = STRIP_B;      // tools/ui-audit.js --explore can discover + drive them)
    if (keyp('3')) expanded = STRIP_909;
    if (keyp('4')) expanded = STRIP_808;
    if (keyp('5')) expanded = STRIP_MST;
    if (keyp('0')) expanded = -1;           // 0 folds everything (nothing open)

    // ── raw-tap surfaces (roll / flags / grid / chain / headers) ──────────
    // sticky widget-ownership (the dubdesk fix): once ui.h captures a finger
    // (a knob/button grab) it stays owned until it LIFTS, so a knob drag that
    // wanders down over the roll never paints it. Capture is visible from the
    // frame after the press — geometry covers the press frame (no ui widget
    // sits on a raw surface). Every unowned finger paints independently.
    for (int i = 0; i < touch_count(); i++) if (ui_captured(touch_id(i))) own_add(touch_id(i));
    for (int i = 0; i < touch_ended_count(); i++) {
        own_drop(touch_ended_id(i)); seen_drop(touch_ended_id(i));
        if (touch_ended_id(i) == kdrag_id) kdrag_id = -1;
    }

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (is_owned(id)) continue;            // finger belongs to a knob/button
        int px = touch_x(i), py = touch_y(i);
        bool tap = !was_seen(id);              // first frame of this finger
        seen_add(id);
        Pattern *P = &bank[editBank];

        // an active mini-knob drag owns its finger outright (tr909 feel:
        // up = coarse, right = fine — wander anywhere, keep turning)
        if (id == kdrag_id) {
            float delta = (kdrag_y0 - py) / 80.0f + (px - kdrag_x0) / 600.0f;
            float *kp = kdrag_val();
            float nv = *kp + delta;
            *kp = nv < 0.0f ? 0.0f : nv > 1.0f ? 1.0f : nv;
            kdrag_x0 = px; kdrag_y0 = py;
            mark_dirty();
            continue;
        }
        // start one: the columns sit beside the grid in the SEQ view
        if (tap && (expanded == STRIP_909 || expanded == STRIP_808) && !fxview[expanded] && rack_wide()) {
            int nv = expanded == STRIP_909 ? N909 : N808;
            int rh = drum_rh_of(expanded, nv);
            int gy = drum_gy_of(expanded);
            if (px >= vknob_x() && px < vknob_x() + 36 && py >= gy && py < gy + nv * rh) {
                int v = (py - gy) / rh, k = (px - vknob_x()) / 12;
                const char **k2 = expanded == STRIP_909 ? K2LAB9 : K2LAB8;
                if (k != 2 || k2[v]) {
                    kdrag_id = id; kdrag_mach = expanded == STRIP_909 ? 9 : 8;
                    kdrag_v = v; kdrag_k = k; kdrag_x0 = px; kdrag_y0 = py;
                    continue;
                }
            }
        }

        // strip selection — TAB strip on a short screen, accordion headers otherwise.
        if (tap && use_tabs()) {
            int ty = tb_h() + 2, tw = tab_w();
            if (py >= ty && py < ty + TAB_H && px >= 2 && px < 2 + NSTRIP * tw) {
                int i = (px - 2) / tw; if (i >= NSTRIP) i = NSTRIP - 1;
                int tx = 2 + i * tw, sq = TAB_H - 8, sx = tx + tw - sq - 3;
                if (px >= sx) { mute[i] = !mute[i]; mark_dirty(); }   // per-tab mute square
                else expanded = i;                                    // pick the machine
            }
            // the fx/seq toggle on the rail is a ui.h button (handled at ui_end)
        } else if (tap) {
            // accordion headers: right zone toggles mute (folded OR open); else a
            // tap toggles open ↔ closed. Skip the open strip's fx/seq button zone.
            for (int i = 0; i < NSTRIP; i++) {
                int y = strip_y(i), h = hdr_h(i);
                if (py < y || py >= y + h) continue;
                if (px >= cmute_x()) { mute[i] = !mute[i]; mark_dirty(); }
                else if (i == expanded && i != STRIP_MST && px >= FX_BTN_X - 2 && px < FX_BTN_X + 32) {
                    /* fx/seq toggle zone — ui.h owns this tap */
                } else {
                    expanded = (i == expanded) ? -1 : i;   // tap header toggles open ↔ closed
                }
                break;
            }
        }

        // song chain row: tap a cell to cycle A→B→C→D→empty (hidden while the
        // 808 panel borrows its space)
        int cp = chain_pitch();
        if (tap && !chain_hidden() && py >= chain_y() && py < chain_y() + 14 && px >= 34 && px < 34 + CHAINN * cp) {
            int c = (px - 34) / cp;
            chain[c] = (chain[c] == 0xFF) ? 0 : (chain[c] + 1 > 3 ? 0xFF : chain[c] + 1);
            chainN = 0; while (chainN < CHAINN && chain[chainN] != 0xFF) chainN++;
            mark_dirty();
        }

        // expanded 303 panel: roll + flag rows (paint on drag, erase on tap)
        if ((expanded == STRIP_A || expanded == STRIP_B) && !fxview[expanded]) {
            Line *ln = &P->ln[expanded];
            int y0 = panel_y0();
            int rx = 40, ry = roll_top(y0), pp = roll_pitch();
            if (px >= rx && px < rx + STEPS * pp && py >= ry && py < ry + 13 * 5) {
                int st = (px - rx) / pp, row = 12 - (py - ry) / 5, b = 1 << st;
                if (tap && (ln->on & b) && ln->pitch[st] == row) ln->on &= ~b;   // tap the note = erase
                else { ln->on |= b; ln->pitch[st] = (unsigned char)row; ln->tie &= ~b; }  // a fresh note clears any tie here
                mark_dirty();
            }
            int fy = ry + 13 * 5 + 2;
            if (tap && px >= rx && px < rx + STEPS * pp && py >= fy && py < fy + 28) {
                int st = (px - rx) / pp, row = (py - fy) / 7, b = 1 << st;
                if      (row == 0) {                                 // OCT: off → up → down → off
                    if      (ln->oct  & b) { ln->oct &= ~b; ln->octd |= b; }
                    else if (ln->octd & b)   ln->octd &= ~b;
                    else                     ln->oct  |= b;
                }
                else if (row == 1) ln->acc ^= b;
                else if (row == 2) ln->sld ^= b;
                else               { ln->tie ^= b; if (ln->tie & b) ln->on &= ~b; }  // a tie carries no fresh note
                mark_dirty();
            }
            int ly = fy + 4 * 7 + 1;                                  // LENGTH strip
            if (tap && px >= rx && px < rx + STEPS * pp && py >= ly && py < ly + 6) {
                int c = (px - rx) / pp + 1;                           // tapped cell → length
                ln->len = (c >= STEPS) ? 0 : (unsigned char)c;        // last cell = full 16 (stored as 0)
                mark_dirty();
            }
        }

        // expanded 909 panel: trigger grid + accent row (SEQ view) or the
        // metal XY pad (FX view — a drag surface)
        if (expanded == STRIP_909 && !fxview[STRIP_909]) {
            int y0 = panel_y0();
            int rh = drum_rh_of(STRIP_909, N909);
            int gx = 36, gy = y0 + 4, pp = drum_pitch();
            if (tap && px >= gx && px < gx + STEPS * pp && py >= gy && py < gy + N909 * rh) {
                int st = (px - gx) / pp, v = (py - gy) / rh;
                P->d909[v] ^= 1 << st;
                if (!(P->d909[v] & (1 << st))) P->st909[v][st] = ST_PLAIN;
                mark_dirty();
            }
            if (tap && px >= gx && px < gx + STEPS * pp && py >= gy + N909 * rh + 2 && py < gy + N909 * rh + 10) {
                P->acc909 ^= 1 << ((px - gx) / pp);
                mark_dirty();
            }
        }
        if (expanded == STRIP_909 && fxview[STRIP_909]) {
            int y0 = panel_y0();
            if (px >= mpad_x() && px < mpad_x() + 60 && py >= y0 + 30 && py < y0 + 70) {
                mcut = (px - mpad_x()) / 60.0f;
                mres = 1.0f - (py - (y0 + 30)) / 40.0f;
                apply_metal_filter();
                mark_dirty();
            }
        }

        // expanded MASTER panel: drag the PCF lane (bar-graph levels per step)
        // + tap a song-code digit to nudge it (regenerates live)
        if (expanded == STRIP_MST) {
            int y0 = panel_y0();
            int xc = code_x();
            if (tap && px >= xc && px < xc + 64 && py >= y0 + 22 && py < y0 + 33) {
                int d = (px - xc) / 8, shift = (7 - d) * 4;
                unsigned nib = ((cur_seed >> shift) + 1u) & 15u;
                gen_song((cur_seed & ~(15u << shift)) | (nib << shift));
            }
            int gx = 36, ly = mst_lane_y(y0), lh = 72, pp = pcf_pitch();
            if (px >= gx && px < gx + STEPS * pp && py >= ly && py < ly + lh) {
                int st = (px - gx) / pp;
                int lvl = 7 - (py - ly) * 8 / lh; if (lvl < 0) lvl = 0; if (lvl > 7) lvl = 7;
                P->pcf[st] = (unsigned char)lvl;
                mark_dirty();
            }
        }

        // expanded 808 panel: trigger grid + accent row
        if (expanded == STRIP_808 && !fxview[STRIP_808]) {
            int y0 = panel_y0();
            int rh = drum_rh_of(STRIP_808, N808);
            int gx = 36, gy = y0 + 6, pp = drum_pitch();
            if (tap && px >= gx && px < gx + STEPS * pp && py >= gy && py < gy + N808 * rh) {
                int st = (px - gx) / pp, v = (py - gy) / rh;
                P->d808[v] ^= 1 << st;
                mark_dirty();
            }
            if (tap && px >= gx && px < gx + STEPS * pp && py >= gy + N808 * rh + 2 && py < gy + N808 * rh + 10) {
                P->acc808 ^= 1 << ((px - gx) / pp);
                mark_dirty();
            }
        }
    }

    // right-click on a matrix knob = reset to center (the shipped recipe)
    if (mouse_pressed(MOUSE_RIGHT) && (expanded == STRIP_909 || expanded == STRIP_808) && !fxview[expanded] && rack_wide()) {
        int nv = expanded == STRIP_909 ? N909 : N808;
        int rh = drum_rh_of(expanded, nv);
        int gy = drum_gy_of(expanded);
        int px = mouse_x(), py = mouse_y();
        if (px >= vknob_x() && px < vknob_x() + 36 && py >= gy && py < gy + nv * rh) {
            int v = (py - gy) / rh, k = (px - vknob_x()) / 12;
            float *kt = expanded == STRIP_909 ? kt9 : kt8;
            float *kd = expanded == STRIP_909 ? kd9 : kd8;
            float *kc = expanded == STRIP_909 ? kc9 : kc8;
            (k == 0 ? kt : k == 1 ? kd : kc)[v] = 0.5f;
            mark_dirty();
        }
    }

    // right-click cycles a 909 cell through the stroke family (off cells join
    // at FLAM, like tr909) — desktop-only for now; long-press is the touch todo
    if (mouse_pressed(MOUSE_RIGHT) && expanded == STRIP_909 && !fxview[STRIP_909]) {
        Pattern *P = &bank[editBank];
        int y0 = panel_y0();
        int rh = drum_rh_of(STRIP_909, N909);
        int gx = 36, gy = y0 + 4, pp = drum_pitch();
        int px = mouse_x(), py = mouse_y();
        if (px >= gx && px < gx + STEPS * pp && py >= gy && py < gy + N909 * rh) {
            int st = (px - gx) / pp, v = (py - gy) / rh;
            if (!(P->d909[v] & (1 << st))) { P->d909[v] |= 1 << st; P->st909[v][st] = ST_FLAM; }
            else P->st909[v][st] = (unsigned char)((P->st909[v][st] + 1) % NSTROKE);
            mark_dirty();
        }
    }

    // deferred autosave (don't write the file on every painted cell)
    if (save_cooldown > 0 && --save_cooldown == 0) save_song();

    apply_fx();     // all gated on-change inside — cheap to call every frame
    ride_pcf();     // filter() is the one effect built to be ridden

#ifdef DE_TRACE
    watch("step", "%d", playhead);
    watch("bar",  "%d", barpos);
    watch("bank", "%d", playBank);
#endif

    // ── the clock — sixteenths off beat(), like tb303/drummachine ─────────
    if (!running) return;
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    int st  = s16 & 15;
    if (s16 != last16) {
        bool first_flip = (last16 < 0);
        bool newbar = (st == 0 && !first_flip) || first_flip;
        last16 = s16;
        playhead = st;
        if (newbar) {                              // pick the bar's bank
            if (songmode && chainN > 0) {
                playBank = chain[barpos % chainN];
                barpos = (barpos + 1) % (chainN > 0 ? chainN : 1);
            } else playBank = editBank;
        }
        Pattern *P = &bank[playBank];
        // 303s trigger on the flip (a held voice can't be scheduled ahead;
        // they also don't shuffle — noted in the design doc, revisit if the
        // straight-303-against-swung-drums clash bothers the ears at 60+)
        lpos[0] = s16 % lnlen(&P->ln[0]);            // each line's OWN step (short len → polymeter drift vs the bar)
        lpos[1] = s16 % lnlen(&P->ln[1]);
        if (!mute[STRIP_A]) step_303(&m[0], &P->ln[0], lpos[0]); else off_303(&m[0]);
        if (!mute[STRIP_B]) step_303(&m[1], &P->ln[1], lpos[1]); else off_303(&m[1]);

        // drums: schedule the NEXT step sample-accurate (the tr909 pattern) —
        // that's what makes shuffle and flam graces land between the frames
        float f = beat_pos() * 4.0f; f -= (int)f;
        int step_ms = 15000 / tempo;
        int delay   = (int)((1.0f - f) * step_ms);
        int nx      = (s16 + 1) & 15;
        if (nx & 1) delay += (swing - 50) * 2 * step_ms / 100;   // even 16ths drag
        // the next step's bank: at a bar seam, peek the chain
        Pattern *N = P;
        if (nx == 0 && songmode && chainN > 0) N = &bank[chain[barpos % chainN]];
        else if (nx == 0 && !songmode) N = &bank[editBank];
        if (!mute[STRIP_909]) {
            int boost = (N->acc909 >> nx) & 1 ? 2 : 0;
            for (int v = 0; v < N909; v++)
                if (N->d909[v] & (1 << nx)) fire_stroke909(v, N->st909[v][nx], boost, delay, step_ms);
        }
        if (!mute[STRIP_808]) {
            int boost = (N->acc808 >> nx) & 1 ? 2 : 0;
            for (int v = 0; v < N808; v++)
                if (N->d808[v] & (1 << nx)) fire808(v, boost, delay);
        }
        if (first_flip) {   // fresh start: also sound the step we're already on
            int b9 = (P->acc909 >> st) & 1 ? 2 : 0, b8 = (P->acc808 >> st) & 1 ? 2 : 0;
            if (!mute[STRIP_909]) for (int v = 0; v < N909; v++)
                if (P->d909[v] & (1 << st)) fire909(v, b9, 0);
            if (!mute[STRIP_808]) for (int v = 0; v < N808; v++)
                if (P->d808[v] & (1 << st)) fire808(v, b8, 0);
        }
    } else {
        Pattern *P = &bank[playBank];
        gate_303(&m[0], &P->ln[0], lpos[0]);          // the line's OWN current step (per-line length)
        gate_303(&m[1], &P->ln[1], lpos[1]);
    }
}

// ── draw ──────────────────────────────────────────────────────────────────
static const int BANKCLR[NBANK] = { CLR_ORANGE, CLR_GREEN, CLR_BLUE, CLR_RED };

static void draw_header(int i, int y) {
    bool open = (i == expanded);
    int h = hdr_h(i);
    rectfill(2, y, W() - 4, h - 2, open ? CLR_DARKER_GREY : CLR_DARK_GREY);
    int ty = y + (h - 8) / 2;
    print(open ? "v" : ">", 8, ty, CLR_LIGHT_GREY);
    print(SNAME[i], 18, ty, mute[i] ? CLR_MEDIUM_GREY : CLR_WHITE);

    // Uniform header, folded OR open: name · full-length mini-pattern · mute square.
    // The mini-pattern runs the SAME length in both states; the fx/seq toggle (open
    // machine strips only) tucks into the gap between the name and the pattern so it
    // never shortens the preview.
    Pattern *P = &bank[playBank];
    int led_x = cmute_x() - 10;
    int mp0 = 96, mpp = (led_x - 4 - mp0) / STEPS; if (mpp < 3) mpp = 3;
    int mpw = mpp - 1; if (mpw > 4) mpw = 4;
    for (int s = 0; s < STEPS; s++) {
        int tx = mp0 + s * mpp;
        bool onn = false;
        if      (i == STRIP_909) { for (int d = 0; d < N909; d++) if (P->d909[d] & (1 << s)) { onn = true; break; } }
        else if (i == STRIP_808) { for (int d = 0; d < N808; d++) if (P->d808[d] & (1 << s)) { onn = true; break; } }
        else if (i == STRIP_MST) onn = P->pcf[s] > 0;
        else onn = (P->ln[i].on >> s) & 1;
        int c = onn ? (mute[i] ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_DARKER_GREY;
        if (running && s == playhead) c = CLR_WHITE;
        rectfill(tx, y + 3, mpw, h - 6, c);
    }
    // activity LED
    bool lit = false;
    if      (i == STRIP_909) { for (int d = 0; d < N909; d++) if (flash909[d] > 0) lit = true; }
    else if (i == STRIP_808) { for (int d = 0; d < N808; d++) if (flash808[d] > 0) lit = true; }
    else if (i == STRIP_MST) lit = pcf_on || fxk[F_GLU] > 0.02f;
    else lit = (m[i].h >= 0);
    circfill(led_x, y + h / 2, 2, lit && !mute[i] ? CLR_GREEN : CLR_DARKER_GREY);

    // fx/seq toggle — OPEN machine strip only, tucked before the mini-pattern
    if (open && i != STRIP_MST && ui_button(FX_BTN_X, y + 2, 30, h - 4, fxview[i] ? "seq" : "fx"))
        fxview[i] = !fxview[i];

    // mute square — green = live, red = muted; SAME affordance folded or open.
    // The whole right zone (cmute_x..edge) is the tap target (handled in update()).
    int sq = h - 5, sx = cmute_x() + (30 - sq) / 2, sy = y + 2;
    rectfill(sx, sy, sq, sq, mute[i] ? CLR_RED : CLR_GREEN);
    rect(sx, sy, sq, sq, CLR_MEDIUM_GREY);
}

static void draw_303_panel(int i, int y0) {
    M303 *s = &m[i];
    Line *ln = &bank[editBank].ln[i];
    rectfill(2, y0, W() - 4, panel_h() - 2, CLR_BLACK);

    // piano roll: 13 rows (root..octave), playhead column, root rows tinted.
    // ry sits below the knob block (1 row wide → flush at +26; 2 rows → pushed down)
    int rx = 40, ry = roll_top(y0), pp = roll_pitch(), cw = pp - 1;
    int L = lnlen(ln);                                   // per-line LENGTH; steps >= L are the dead zone
    for (int st = 0; st < STEPS; st++) {
        int cx = rx + st * pp;
        bool dead = st >= L;
        int bg = (running && st == lpos[i]) ? CLR_DARKER_GREY : CLR_BLACK;   // the LINE's own playhead (drifts if short)
        rectfill(cx, ry, cw, 13 * 5 - 1, bg);
        for (int r = 0; r <= 12; r += 12) {  // root + octave guide rows
            rectfill(cx, ry + (12 - r) * 5, cw, 4, bg == CLR_BLACK ? CLR_DARKER_GREY : CLR_DARK_GREY);
        }
        if ((ln->on >> st) & 1) {
            int row = ln->pitch[st];
            int c = dead ? CLR_DARK_GREY : (((ln->acc >> st) & 1) ? CLR_RED : BANKCLR[editBank]);   // dead steps dimmed
            rectfill(cx, ry + (12 - row) * 5, cw, 4, c);
            if ((ln->sld >> st) & 1) rectfill(cx + cw - 2, ry + (12 - row) * 5 + 1, 3, 2, CLR_WHITE);
        }
    }
    if (L < STEPS) rectfill(rx + L * pp - 1, ry, 1, 13 * 5 - 1, CLR_RED);   // LENGTH divider — the loop end (tb303-style)
    font(FONT_SMALL);
    print("C3", 2, ry + 4, CLR_MEDIUM_GREY);
    print("C2", 2, ry + 12 * 5 - 5, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // flag rows: OCT (up=yellow / down=indigo) · ACC · SLD · TIE (green = hold)
    static const char *FLAG[4] = { "OCT", "ACC", "SLD", "TIE" };
    int fy = ry + 13 * 5 + 2;
    font(FONT_SMALL);
    for (int r = 0; r < 4; r++) {
        print(FLAG[r], 14, fy + r * 7, CLR_MEDIUM_GREY);
        for (int st = 0; st < STEPS; st++) {
            int b = 1 << st, c;
            if      (r == 0) c = (ln->oct & b) ? CLR_YELLOW : (ln->octd & b) ? CLR_INDIGO : CLR_DARKER_GREY;
            else if (r == 1) c = (ln->acc & b) ? CLR_RED    : CLR_DARKER_GREY;
            else if (r == 2) c = (ln->sld & b) ? CLR_YELLOW : CLR_DARKER_GREY;
            else             c = (ln->tie & b) ? CLR_GREEN  : CLR_DARKER_GREY;
            rectfill(rx + st * pp, fy + r * 7, cw, 6, c);
        }
    }
    // LENGTH strip: tap a cell to set the loop length (last cell = full 16). A short
    // line drifts against the 16-step bar (polymeter). The red divider in the roll marks it.
    int ly = fy + 4 * 7 + 1;
    print("LEN", 14, ly, CLR_MEDIUM_GREY);
    for (int st = 0; st < STEPS; st++)
        rectfill(rx + st * pp, ly, cw, 5, st < L ? CLR_INDIGO : CLR_DARKER_GREY);
    font(FONT_NORMAL);

    // knob row LAST — drawn on top. TWO PAGES: P1 = the classic 7, P2 = the DEEP
    // page (the Devil Fish / TD-3-MO depth knobs). Labels are tiny vertical text.
    // Both pages fit ONE row; the roll below is sized for the taller page (NKMAX)
    // so it never shifts when you flip. (Right-aligned buttons assume a wide panel
    // — phone-portrait wrapping is the device-adaptive-redesign todo's job.)
    const int *pg = kpage[i] ? KPAGE1 : KPAGE0;
    int npg  = kpage[i] ? NKP1 : NKP0;
    int cols = knob_cols(NKMAX), kp = (W() - 52) / cols;
    font(FONT_TINY);
    for (int j = 0; j < npg; j++) {
        int k = pg[j];
        int cx = 26 + (j % cols) * kp, cy = y0 + 12 + (j / cols) * 22;
        if (ui_knob(&s->knob[k], cx, cy, "")) { knob_changed_303(s, k); mark_dirty(); }
        print_rot(KNAME[k], cx - 12, cy, 270.0f, CLR_MEDIUM_GREY, 1);   // vertical tiny label, left of the knob
    }
    font(FONT_NORMAL);
    // top-right controls (above the roll): [wave] [page]  + [STD reset][accent-sweep] on P2
    if (ui_button(W() - 32, y0 + 4, 28, 16, s->wave == INSTR_SAW ? "SAW" : "SQR")) {
        s->wave = (s->wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW;
        define_303(s); mark_dirty();
    }
    if (ui_button(W() - 60, y0 + 4, 24, 16, kpage[i] ? "P2" : "P1")) { kpage[i] = !kpage[i]; mark_dirty(); }
    if (kpage[i]) {
        if (ui_button(W() - 86, y0 + 4, 24, 16, "STD")) { reset_deep(s); mark_dirty(); }   // reset DEEP → stock 303 (no Devil Fish)
        static const char *SWL[4] = { "SW:off", "SW:slo", "SW:med", "SW:fst" };            // accent-sweep onset (Devil Fish: 3 speeds + off)
        if (ui_button(W() - 134, y0 + 4, 44, 16, SWL[s->sweep & 3])) { s->sweep = (s->sweep + 1) & 3; mark_dirty(); }
    }
}

// one grid implementation, two voice tables — the 808/909 shared skeleton.
// strokes draws tr909's tick marks; row pitch rh differs (11 vs 9 voices).
static void draw_drum_grid(const char **names, int nv, unsigned short *rows,
                           unsigned char st_rows[][STEPS], unsigned short accmask,
                           int *flash, int gx, int gy, int rh) {
    int pp = drum_pitch(), cw = pp - 1;
    font(FONT_SMALL);
    for (int v = 0; v < nv; v++) {
        print(names[v], 12, gy + v * rh + 2, flash[v] > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
        for (int st = 0; st < STEPS; st++) {
            int cx = gx + st * pp;
            bool onn = (rows[v] >> st) & 1;
            int c = onn ? BANKCLR[editBank] : ((st & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
            if (running && st == playhead && onn) c = CLR_WHITE;
            rectfill(cx, gy + v * rh, cw, rh - 1, c);
            if (onn && st_rows && st_rows[v][st] != ST_PLAIN)   // stroke ticks
                for (int t = 0; t <= (int)st_rows[v][st] - ST_FLAM; t++)
                    rectfill(cx + 2 + t * 3, gy + v * rh + rh - 3, 2, 2, CLR_BLACK);
        }
    }
    // TOTAL ACCENT row
    print("AC", 12, gy + nv * rh + 3, CLR_RED);
    for (int st = 0; st < STEPS; st++)
        rectfill(gx + st * pp, gy + nv * rh + 2, cw, 6,
                 (accmask >> st) & 1 ? CLR_RED : CLR_DARKER_GREY);
    font(FONT_NORMAL);
}

// the per-voice TUNE/DEC/CHAR columns — row-aligned beside the grid, like
// the source carts' panels (always visible while you sequence)
static void draw_voice_knobs(int mach, const char **names, const char **k2lab, int nv,
                             float *kt, float *kd, float *kc, int gy, int rh, int kx) {
    for (int v = 0; v < nv; v++) {
        int ry = gy + v * rh;
        draw_miniknob(kx, ry, kt[v], kt[v] == 0.5f ? CLR_LIGHT_GREY : CLR_YELLOW);
        draw_miniknob(kx + 12, ry, kd[v], kd[v] == 0.5f ? CLR_LIGHT_GREY : CLR_YELLOW);
        if (k2lab[v]) draw_miniknob(kx + 24, ry, kc[v], kc[v] == 0.5f ? CLR_LIGHT_GREY : CLR_ORANGE);
    }
    font(FONT_SMALL);
    print("T", kx + 2, gy + nv * rh + 2, CLR_DARK_GREY);
    print("D", kx + 14, gy + nv * rh + 2, CLR_DARK_GREY);
    print("C", kx + 26, gy + nv * rh + 2, CLR_DARK_GREY);
    // the dragged knob names itself, floating on its own row (left of the columns)
    if (kdrag_id != -1 && kdrag_mach == mach) {
        const char *lab = kdrag_k == 0 ? "TUNE" : kdrag_k == 1 ? "DEC" : k2lab[kdrag_v];
        char buf[24];
        snprintf(buf, sizeof buf, "%s %s %.2f", names[kdrag_v], lab ? lab : "", *kdrag_val());
        int ry = gy + kdrag_v * rh, lx = kx - 102;
        rectfill(lx, ry - 1, 98, 9, CLR_BLACK);
        print(buf, lx + 4, ry + 1, CLR_YELLOW);
    }
    font(FONT_NORMAL);
}

static void draw_909_panel(int y0) {
    Pattern *P = &bank[editBank];
    rectfill(2, y0, W() - 4, panel_h() - 2, CLR_BLACK);
    int rh9 = drum_rh_of(STRIP_909, N909);
    draw_drum_grid(NAME909, N909, P->d909, P->st909, P->acc909, flash909, 36, y0 + 4, rh9);
    if (rack_wide()) draw_voice_knobs(9, NAME909, K2LAB9, N909, kt9, kd9, kc9, y0 + 4, rh9, vknob_x());
}

// ── the per-machine FX views (the [fx] header button) — RB-338 style: ─────
// each device owns its dist + its send into THE shared delay unit
static void draw_303_fx(int i, int y0) {
    M303 *s = &m[i];
    rectfill(2, y0, W() - 4, panel_h() - 2, CLR_BLACK);
    if (ui_knob(&s->knob[K_DRV], 26, y0 + 12, "DIST")) { knob_changed_303(s, K_DRV); mark_dirty(); }
    if (ui_knob(&send[i], 64, y0 + 12, "SEND")) mark_dirty();
    font(FONT_SMALL);
    print("DIST = this box's drive (same as DRV on the seq view)", 12, y0 + 40, CLR_DARK_GREY);
    print("SEND = its level into the shared delay unit", 12, y0 + 50, CLR_DARK_GREY);
    print("delay TIME/FB live on the MASTER strip", 12, y0 + 60, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
static void draw_909_fx(int y0) {
    rectfill(2, y0, W() - 4, panel_h() - 2, CLR_BLACK);
    if (ui_knob(&dist9, 26, y0 + 16, "DIST")) mark_dirty();
    if (ui_knob(&send[2], 64, y0 + 16, "SEND")) mark_dirty();
    // the metal-filter XY pad (X = five metal highpass cutoffs, Y = resonance)
    int padx = mpad_x(), pady = y0 + 30;
    rectfill(padx, pady, 60, 40, CLR_DARKER_GREY);
    rect(padx, pady, 60, 40, CLR_DARK_GREY);
    circfill(padx + (int)(mcut * 60), pady + (int)((1.0f - mres) * 40), 2, CLR_YELLOW);
    // master shuffle — the 909 is where Roland shipped it (Z/X keys too)
    if (ui_slider(&swingf, W() - 66, y0 + 44, 56, "SHUF")) { swing = 50 + (int)(swingf * 16.0f); mark_dirty(); }
    font(FONT_SMALL);
    print("METAL", padx + 18, pady + 42, CLR_MEDIUM_GREY);
    print("DIST rides every 909 voice; SEND feeds the delay", 12, y0 + 84, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
static void draw_808_fx(int y0) {
    rectfill(2, y0, W() - 4, panel_h() - 2, CLR_BLACK);
    if (ui_knob(&dist8, 26, y0 + 16, "DIST")) mark_dirty();
    if (ui_knob(&send[3], 64, y0 + 16, "SEND")) mark_dirty();
    font(FONT_SMALL);
    print("DIST rides every 808 voice; SEND feeds the delay", 12, y0 + 84, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

static void draw_808_panel(int y0) {
    Pattern *P = &bank[editBank];
    rectfill(2, y0, W() - 4, panel_h() - 2, CLR_BLACK);
    int rh8 = drum_rh_of(STRIP_808, N808);
    draw_drum_grid(NAME808, N808, P->d808, NULL, P->acc808, flash808, 36, y0 + 6, rh8);
    if (rack_wide()) draw_voice_knobs(8, NAME808, K2LAB8, N808, kt8, kd8, kc8, y0 + 6, rh8, vknob_x());
}

static void draw_master_panel(int y0) {
    Pattern *P = &bank[editBank];
    rectfill(2, y0, W() - 4, panel_h() - 2, CLR_BLACK);
    // knob row: TIME/FB (the shared delay unit) · GLU · PCF/RES
    int avail = W() - 84, cols = mst_cols(), kp = avail / cols;   // wraps to 2 rows when narrow
    font(FONT_TINY);
    for (int k = 0; k < NFX; k++) {
        int cx = 22 + (k % cols) * kp, cy = y0 + 12 + (k / cols) * 22;
        if (ui_knob(&fxk[k], cx, cy, "")) mark_dirty();
        print_rot(FXNAME[k], cx - 12, cy, 270.0f, CLR_MEDIUM_GREY, 1);   // vertical tiny label, left of the knob
    }
    font(FONT_NORMAL);
    // the PCF lane — bar-graph levels, one per step, per BANK (it's part of
    // the arrangement: the demo song's build is a ramp drawn here)
    int gx = 36, ly = mst_lane_y(y0), lh = 72, pp = pcf_pitch(), cw = pp - 1;
    for (int st = 0; st < STEPS; st++) {
        int cx = gx + st * pp;
        rectfill(cx, ly, cw, lh, (st & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
        int lvl = P->pcf[st];
        int bh = lvl * lh / 7;
        int c = (running && st == playhead) ? CLR_WHITE : BANKCLR[editBank];
        if (bh > 0) rectfill(cx, ly + lh - bh, cw, bh, c);
    }
    font(FONT_SMALL);
    print("PCF", 12, ly + 2, CLR_MEDIUM_GREY);
    print("lane", 12, ly + 10, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    // the song code — GEN rolls a fresh one ([ ] walk history), tapping a
    // hex digit nudges it (rclick down) and regenerates live, WAV exports
    int xc = code_x();
    if (ui_button(xc, y0 + 2, 32, 15, "GEN")) gen_song(0);
    if (ui_button(xc + 36, y0 + 2, 32, 15, "WAV")) export_wav();
    font(FONT_SMALL);
    for (int d = 0; d < 8; d++) {
        char hx[2] = { "0123456789ABCDEF"[(cur_seed >> ((7 - d) * 4)) & 15], 0 };
        rectfill(xc + d * 8, y0 + 22, 7, 11, CLR_DARKER_GREY);
        print(hx, xc + 2 + d * 8, y0 + 25, CLR_YELLOW);
    }
    print("song code", xc + 4, y0 + 36, CLR_MEDIUM_GREY);
    print("acid rack", xc, y0 + 84, CLR_INDIGO);
    print("send lives in", xc, y0 + 94, CLR_DARK_GREY);   // (shortened to fit the 72px column)
    print("the fx views", xc, y0 + 102, CLR_DARK_GREY);
    if (export_flash > 0) {
        print("exporting >", xc, y0 + 50, CLR_GREEN);
        print(export_name, xc, y0 + 58, CLR_GREEN);
        print("(build/)", xc, y0 + 66, CLR_MEDIUM_GREY);
    }
    font(FONT_NORMAL);
}

// dispatch: draw whichever strip is expanded (its seq view, its fx view, or master)
static void draw_selected_panel(int y0) {
    int i = expanded;
    if (i < 0) return;
    if      (i == STRIP_MST) draw_master_panel(y0);
    else if (fxview[i]) {
        if      (i == STRIP_909) draw_909_fx(y0);
        else if (i == STRIP_808) draw_808_fx(y0);
        else                     draw_303_fx(i, y0);
    }
    else if (i == STRIP_909) draw_909_panel(y0);
    else if (i == STRIP_808) draw_808_panel(y0);
    else                     draw_303_panel(i, y0);
}

// tab strip for a short (landscape) screen: 5 machine tabs (name + mute square)
// + one fx/seq toggle for the selected machine on the right rail.
static void draw_tabs(int y) {
    int tw = tab_w();
    for (int i = 0; i < NSTRIP; i++) {
        int tx = 2 + i * tw;
        bool sel = (i == expanded);
        rectfill(tx, y, tw - 1, TAB_H, sel ? CLR_DARKER_GREY : CLR_DARK_GREY);
        if (sel) rect(tx, y, tw - 1, TAB_H, CLR_TRUE_BLUE);
        font(FONT_SMALL);
        print(SNAME[i], tx + 3, y + (TAB_H - 6) / 2, mute[i] ? CLR_MEDIUM_GREY : CLR_WHITE);
        font(FONT_NORMAL);
        int sq = TAB_H - 8, sx = tx + tw - sq - 3, sy = y + 4;
        rectfill(sx, sy, sq, sq, mute[i] ? CLR_RED : CLR_GREEN);      // green = live, red = muted
    }
    if (expanded >= 0 && expanded != STRIP_MST &&                     // fx/seq toggle (master has none)
        ui_button(W() - tab_rail() + 2, y, tab_rail() - 4, TAB_H, fxview[expanded] ? "sq" : "fx"))
        fxview[expanded] = !fxview[expanded];
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    ui_begin();

    // ── transport bar ─────────────────────────────────────────────────────
    // sT drops the CONTROLS below the notch; the black bar (tb_h() includes the top inset) bleeds
    // full-width up under the status bar, so the chrome dodges the safe area without a letterbox seam.
    int sT = saf_t();
    bool tworow = W() < 300;
    rectfill(0, 0, W(), tb_h(), CLR_BLACK);
    if (ui_button(4, 2 + sT, 36, 18, running ? "STOP" : "RUN")) {
        running = !running; last16 = -1; if (!running) stop_all();
    }
    char buf[24];
    snprintf(buf, sizeof buf, "%d", tempo);
    print(buf, 42, 8 + sT, CLR_WHITE);
    if (ui_button(66, 2 + sT, 12, 18, "-")) { tempo -= 2; if (tempo < 60) tempo = 60; bpm(tempo); mark_dirty(); }
    if (ui_button(80, 2 + sT, 12, 18, "+")) { tempo += 2; if (tempo > 200) tempo = 200; bpm(tempo); mark_dirty(); }
    for (int bk = 0; bk < NBANK; bk++) {
        char nm[2] = { (char)('A' + bk), 0 };
        int x = 98 + bk * 20;
        if (ui_button(x, 2 + sT, 17, 18, nm)) {
            if (copyArm) { copy_scope(&bank[bk], &bank[editBank]); copyArm = false; mark_dirty(); }
            editBank = bk;
        }
        if (bk == editBank) rect(x, 2 + sT, 17, 18, BANKCLR[bk]);
        if (running && bk == playBank) rectfill(x + 6, 17 + sT, 5, 2, CLR_WHITE);
    }
    // pattern ops — scope follows the expanded strip (machine lane; MASTER = whole
    // bank). One row at 320; wraps to a 2nd row when the bar is too narrow to fit.
    int ox = tworow ? 4 : 180, oy = (tworow ? 24 : 2) + sT;
    if (ui_button(ox, oy, 26, 18, "CPY")) copyArm = !copyArm;
    if (copyArm) rect(ox, oy, 26, 18, CLR_YELLOW);
    if (ui_button(ox + 28, oy, 26, 18, "CLR")) { clr_scope(&bank[editBank]); copyArm = false; mark_dirty(); }
    if (ui_button(ox + 56, oy, 26, 18, "RND")) { rnd_scope(&bank[editBank]); copyArm = false; mark_dirty(); }
    if (ui_button(ox + 86, oy, 42, 18, songmode ? "SONG" : "LOOP")) { songmode = !songmode; barpos = 0; mark_dirty(); }

    // ── strips: accordion (tall screen) or a tab strip (short/landscape) ────
    if (use_tabs()) {
        draw_tabs(tb_h() + 2);
        draw_selected_panel(panel_y0());
    } else {
        for (int i = 0; i < NSTRIP; i++) {
            draw_header(i, strip_y(i));
            if (i == expanded) draw_selected_panel(panel_y0());
        }
    }

    // ── song chain row (hidden while the 808 panel borrows its space) ───────
    if (!chain_hidden()) {
        int cy = chain_y();
        print("SONG", 4, cy + 3, songmode ? CLR_WHITE : CLR_MEDIUM_GREY);
        int cpitch = (W() - 38) / CHAINN; if (cpitch < 2) cpitch = 2;
        int cw = cpitch < 3 ? cpitch : 3;
        for (int c = 0; c < CHAINN; c++) {
            int x = 34 + c * cpitch;
            int bk = chain[c];
            int col = (bk == 0xFF) ? CLR_DARK_GREY : BANKCLR[bk];
            rectfill(x, cy + 2, cw, 10, col);
            if (songmode && running && chainN > 0 && c == (barpos + chainN - 1) % chainN)
                rectfill(x, cy, cw, 2, CLR_WHITE);
        }
    }

    if (export_flash > 0) export_flash--;
    for (int d = 0; d < N909; d++) if (flash909[d] > 0) flash909[d]--;
    for (int d = 0; d < N808; d++) if (flash808[d] > 0) flash808[d]--;
    ui_end();
}
