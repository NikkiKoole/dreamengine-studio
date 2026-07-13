// drumkit.h — a shared, PLAYABLE drum kit for cart-land.
//
// Why this exists (docs/design/mic-and-sampling.md): the engine has NO INSTR_KIT.
// Every drum cart (tr808 / tr909 / drummachine / fingerdrums) hand-builds its ~6-16
// voices from primitives (INSTR_SINE / INSTR_NOISE / INSTR_MEMBRANE) and re-implements
// the same trigger plumbing each time. This header owns the SKELETON once: a semantic
// ROLE vocabulary, a fixed slot layout, and a velocity + pitch trigger. The SOUND stays
// per-kit — an 808 kick and a 909 kick are two different build() callbacks, NOT one
// flattened voice. Nothing about a machine's character is lost by using this; only the
// wiring is shared (decision-0006 lane, beside keybed.h / cards.h / radio.h).
//
// A kit is NOT a keybed. keybed.h pitches ONE voice chromatically (every key = the same
// sound at a different pitch). A kit is the opposite: N DIFFERENT sounds, each nailed to
// one pad at a fixed pitch — a drum MAP. That's why drums live on the pad row, not the
// keybed. Pitch is still a first-class trigger arg (dk_fire(role, midi, vel)); v1 callers
// pass midi = 0 for the role's default pitch, so bending a voice later (play a tom across
// keys) is a NEW caller, not a refactor.
//
// Usage:
//   #include "drumkit.h"
//   void init(void)   { dk_use(&DK_ELECTRO, 20); }          // voice slots 20..20+DK_N-1
//   void update(void) { if (keyp('A')) dk_fire(DK_KICK, 0, 6); }
//
// Built-in kits: DK_ELECTRO (808/SP electronic) and DK_ACOUSTIC (struck bodies). A cart
// can supply its own DrumKitDef to voice a bespoke kit against the same trigger + roles.
//
// NOT owned here (yet): choke groups (a closed hat cutting the ringing open hat). The
// engine's one-shot notes can't cut each other (tr808's documented infidelity), so it's
// a known gap, not a bug — addable when the engine grows a per-slot note-off.

#ifndef DE_DRUMKIT_H
#define DE_DRUMKIT_H

#include "studio.h"

// ── roles: the semantic drum map. 8 voices → the A S D F G H J K pad row. ──
enum { DK_KICK, DK_SNARE, DK_HHC, DK_HHO, DK_CLAP, DK_TOM_LO, DK_TOM_HI, DK_CRASH, DK_N };

static const char *DK_ROLE_NAME[DK_N] = {
    "KICK", "SNARE", "HAT", "OPEN", "CLAP", "TOM LO", "TOM HI", "CRASH",
};
// default gate length per role (ms) — the slot's own release shapes the real tail
static const int DK_ROLE_DUR[DK_N]  = { 300, 160, 40, 260, 130, 300, 260, 700 };
// default pitch per role (0 = engine default). SINE/MEMBRANE bodies want a real note.
static const int DK_ROLE_MIDI[DK_N] = {  33,  57, 70,  70,  60,  48,  55,  70 };

typedef struct {
    const char *name;
    void (*build)(int base);   // voice slots base..base+DK_N-1 (this is the SOUND)
} DrumKitDef;

static int dk_base = 20;       // current kit's first sound slot (set by dk_use)

// fire a role at velocity 1..7 and pitch `midi` (0 = the role default), delayed
// `delay_ms` (0 = now). The slot's own envelope shapes the real tail.
static void dk_fire_at(int delay_ms, int role, int midi, int vel) {
    if (role < 0 || role >= DK_N) return;
    if (vel < 1) vel = 1; if (vel > 7) vel = 7;
    schedule_hit(delay_ms, midi ? midi : DK_ROLE_MIDI[role], dk_base + role, vel, DK_ROLE_DUR[role]);
}
static void dk_fire(int role, int midi, int vel) { dk_fire_at(0, role, midi, vel); }

static const char *dk_role_name(int role) { return (role >= 0 && role < DK_N) ? DK_ROLE_NAME[role] : ""; }

// ── built-in kits ─────────────────────────────────────────────────────────────
// ELECTRO — the 808/SP lineage: a boom-y sine kick with a pitch drop, noise snare /
// hats / clap / crash, sine toms. Clean and punchy — samples cleanly (the whole point).
static void dk_build_electro(int b) {
    instrument(b + DK_KICK, INSTR_SINE, 0, 300, 0, 60);        // low sine...
    instrument_env(b + DK_KICK, 0, ENV_PITCH, 0, 60, 26);      // ...+26st drop = the boom
    instrument_filter(b + DK_KICK, FILTER_LOW, 200, 2);

    instrument(b + DK_SNARE, INSTR_NOISE, 0, 130, 0, 40);      // band of noise = snare
    instrument_filter(b + DK_SNARE, FILTER_BAND, 1400, 4);

    instrument(b + DK_HHC, INSTR_NOISE, 0, 40, 0, 20);         // short highpassed noise
    instrument_filter(b + DK_HHC, FILTER_HIGH, 7000, 2);
    instrument(b + DK_HHO, INSTR_NOISE, 0, 260, 0, 120);       // ...longer = open hat
    instrument_filter(b + DK_HHO, FILTER_HIGH, 7000, 2);

    instrument(b + DK_CLAP, INSTR_NOISE, 0, 120, 0, 40);       // mid band burst
    instrument_filter(b + DK_CLAP, FILTER_BAND, 1200, 5);

    instrument(b + DK_TOM_LO, INSTR_SINE, 0, 260, 0, 120);     // sine + small pitch drop
    instrument_env(b + DK_TOM_LO, 0, ENV_PITCH, 0, 80, 8);
    instrument(b + DK_TOM_HI, INSTR_SINE, 0, 240, 0, 110);
    instrument_env(b + DK_TOM_HI, 0, ENV_PITCH, 0, 80, 8);

    instrument(b + DK_CRASH, INSTR_NOISE, 0, 700, 0, 400);     // bright, long
    instrument_filter(b + DK_CRASH, FILTER_HIGH, 5000, 1);
}
static const DrumKitDef DK_ELECTRO = { "ELECTRO", dk_build_electro };

// ACOUSTIC — struck drum bodies: INSTR_MEMBRANE kick + toms (rings on its own like a
// real head), noise snare / hats / clap / cymbals. Warmer, less synthetic than ELECTRO.
static void dk_build_acoustic(int b) {
    instrument(b + DK_KICK, INSTR_MEMBRANE, 0, 300, 0, 120);

    instrument(b + DK_SNARE, INSTR_NOISE, 0, 150, 0, 60);
    instrument_filter(b + DK_SNARE, FILTER_BAND, 1800, 3);

    instrument(b + DK_HHC, INSTR_NOISE, 0, 50, 0, 30);
    instrument_filter(b + DK_HHC, FILTER_HIGH, 8000, 2);
    instrument(b + DK_HHO, INSTR_NOISE, 0, 300, 0, 150);
    instrument_filter(b + DK_HHO, FILTER_HIGH, 8000, 2);

    instrument(b + DK_CLAP, INSTR_NOISE, 0, 140, 0, 50);
    instrument_filter(b + DK_CLAP, FILTER_BAND, 1500, 4);

    instrument(b + DK_TOM_LO, INSTR_MEMBRANE, 0, 300, 0, 160);
    instrument(b + DK_TOM_HI, INSTR_MEMBRANE, 0, 260, 0, 140);

    instrument(b + DK_CRASH, INSTR_NOISE, 0, 800, 0, 500);
    instrument_filter(b + DK_CRASH, FILTER_HIGH, 6000, 1);
}
static const DrumKitDef DK_ACOUSTIC = { "ACOUSTIC", dk_build_acoustic };

static void dk_use(const DrumKitDef *def, int base) {
    dk_base = base;
    if (def && def->build) def->build(base);
}

#endif // DE_DRUMKIT_H
