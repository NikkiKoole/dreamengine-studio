// statecart.c — the CART half of the session-state gate (tools/state-check/run.sh).
//
// A purpose-built rack rather than acidcandy, because the assertions have to be exact. It holds one
// slice of each kind, so a single run can prove BOTH halves of the split:
//   · a SAVED slice (de_state_for_saved) — what the player chose. Must come back.
//   · a SCRATCH slice (de_state_for)     — what a fresh init() rebuilds. Must NOT come back.
// A gate that only checked the saved half would pass just as happily if EVERYTHING were saved, which
// is the bug that restores a stale voice handle into another instance.
//
// The slices carry NON-ZERO defaults on purpose. de_state_for hands back zeroed memory, so a rack
// that booted with every level at 0 is a real failure mode this shape can see: "restored" and
// "zeroed" look identical if the default is 0.
#include "studio.h"

// ── the SAVED slice: knob + pattern, i.e. intent ─────────────────────────────────────────────────
typedef struct { int knob; int pattern[4]; int inited; } ScSaved;
static ScSaved sc_saved_default = { .knob = 3, .pattern = { 1, 0, 1, 0 } };
static char sc_saved_key_;
static ScSaved *sc_saved_(void) {
    ScSaved *c = (ScSaved *)de_state_for_saved(&sc_saved_key_, (int)sizeof(ScSaved));
    if (c && !c->inited) { *c = sc_saved_default; c->inited = 1; }
    return c;
}
#define SC (sc_saved_())

// ── the SCRATCH slice: a per-frame counter, i.e. derived ─────────────────────────────────────────
typedef struct { int ticks; int inited; } ScScratch;
static char sc_scratch_key_;
static ScScratch *sc_scratch_(void) {
    ScScratch *c = (ScScratch *)de_state_for(&sc_scratch_key_, (int)sizeof(ScScratch));
    if (c && !c->inited) { c->ticks = 500; c->inited = 1; }   // non-zero: see the header note
    return c;
}
#define SCR (sc_scratch_())

// ── handshake with the host probe ────────────────────────────────────────────────────────────────
// Plain globals, exactly as tools/instance-check/uicart.c does it: the host writes a request and
// reads what the cart saw, and everything that TOUCHES instance state happens inside update()/draw(),
// which is where de_frame has selected the instance. Reading a slice from main() would resolve to
// whichever instance that thread last entered — the mistake the seam exists to make impossible.
int sc_write_knob = -1;   // host → cart: set the knob to this on the next frame
int sc_fire       = 0;    // host → cart: play ONE note on the next frame
int sc_seen_knob  = -1;   // cart → host: the knob this frame
int sc_seen_p0    = -1;   // cart → host: pattern[0] this frame
int sc_seen_ticks = -1;   // cart → host: the SCRATCH counter this frame

void init(void) { }

void update(void) {
    if (sc_write_knob >= 0) {
        SC->knob = sc_write_knob;
        SC->pattern[0] = 1 - SC->pattern[0];   // a second saved field, so the gate is not one int wide
        sc_write_knob = -1;
        // the SOUND half of intent: a set-and-hold config call whose value comes from the knob, so it
        // lands in ctx_log and has to survive the round trip. Fired ONLY on change — wiring a config
        // call into every frame rebuilds the bus DSP 60×/s (CLAUDE.md, and tools/lint-fx-frame.js).
        instrument_level(0, SC->knob / 8.0f);
    }
    sc_seen_knob = SC->knob;
    sc_seen_p0   = SC->pattern[0];
    SCR->ticks++;
    sc_seen_ticks = SCR->ticks;
    // Host-triggered rather than every(8): `every` is BEAT-based, so at the default tempo nothing
    // fires inside the handful of frames this gate runs, and the sound half silently went untested.
    // INSTR_SQUARE is slot 0 — the same slot instrument_level trims above — so the level that
    // travelled in ctx_log is directly audible in this note's amplitude.
    if (sc_fire) { note(60, INSTR_SQUARE, 7); sc_fire = 0; }
}

void draw(void) { cls(SC->knob); }   // the knob IS the frame, so a pixel compare tests the restore
