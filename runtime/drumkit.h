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
#include "cart_ctx.h"   // per-instance state seam (docs/design/engine-context.md)
//   void init(void)   { dk_use(&DK_ELECTRO, 20); }          // voice slots 20..20+DK_SLOTS-1 (9, not 8)
//   void update(void) { if (keyp('A')) dk_fire(DK_KICK, 0, 6); }
//
// Built-in kits: DK_ELECTRO (808/SP electronic) and DK_ACOUSTIC (a real kit, measured). A cart
// can supply its own DrumKitDef to voice a bespoke kit against the same trigger + roles; its
// optional fire() hook plays one stroke as several notes (a snare's head + wires).
// Set a role's volume with dk_level(role, x), never instrument_level(base + role): a layered role
// has more than one slot. A cart that books hits itself (rootwalk) goes through dk_play_at().
// Choke: DK_ACOUSTIC's closed hat cuts its open hat (instrument_choke); DK_ELECTRO has none yet.

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

// A kit occupies DK_SLOTS voice slots: one per role, plus LAYER slots a role may need on top (a real
// snare is a drum head AND its wires, two different engines, so it cannot live in one slot). Reserve
// base..base+DK_SLOTS-1 for a kit, never only DK_N.
enum { DK_L_WIRES = DK_N, DK_SLOTS };

typedef struct DrumKitDef {
    const char *name;
    void (*build)(int base);   // voice slots base..base+DK_SLOTS-1 (this is the SOUND)
    // OPTIONAL: how one stroke of `role` is played (NULL = one note on the role's slot). A kit whose
    // strokes are several voices (a kick's second mode, a snare's wires) books them here, at audio
    // time t; midi 0 = the role's own pitch, else a GM-style note that TRANSPOSES it (see dk_gm_anchor).
    void (*fire)(int base, double t, int role, int midi, int vel);
} DrumKitDef;

// the GM note each role answers to (kick 36, snare 38 ...). Callers that map a pattern library onto the
// kit pass these as pitches (groovebook, rootwalk); a kit with a fire() hook reads a note as an offset
// from its role's anchor, so GM 41 / 48 on TOM_LO are a floor and a rack tom 7 semitones apart.
static const int DK_GM_ANCHOR[DK_N] = { 36, 38, 42, 46, 39, 41, 50, 49 };

#define DRUMKIT_STATE(X) \
    X(int,      dk_base,           ,              20)
// the current kit (for its fire() hook): a POINTER, so scratch, never saved session state
#define DRUMKIT_SCRATCH(X) \
    X(const struct DrumKitDef *, dk_kit, ,       0)

#ifndef DE_CART_CTX
DE_CTX_STATICS(DRUMKIT_STATE)
DE_CTX_STATICS(DRUMKIT_SCRATCH)
#else
DE_CTX_BLOCK_SAVED(dk, Dk, DRUMKIT_STATE)   // dk_base has a cart-facing setter → session state
// defined BEFORE the access macros below: after them, `c->name` would expand to
// `c->(ctx_()->name)`, because the member names are macros from that point on.
static void dk_ctx_init_(DkCtx *c) {
    c->dk_base = 20;
}
#define dk_base (dk_ctx_()->dk_base)
DE_CTX_BLOCK(dkk, Dkk, DRUMKIT_SCRATCH)
static void dkk_ctx_init_(DkkCtx *c) { c->dk_kit = 0; }
#define dk_kit (dkk_ctx_()->dk_kit)
#endif       // current kit's first sound slot (set by dk_use)

// fire a role at velocity 1..7 and pitch `midi` (0 = the role default), delayed
// `delay_ms` (0 = now). The slot's own envelope shapes the real tail.
// play one stroke of `kit` (whose slots start at `base`) at audio time t. For a cart that keeps two
// kits voiced at once (rootwalk) and so cannot go through the one current kit.
static void dk_play_at(const DrumKitDef *kit, int base, double t, int role, int midi, int vel) {
    if (role < 0 || role >= DK_N) return;
    if (vel < 1) vel = 1; if (vel > 7) vel = 7;
    if (kit && kit->fire) kit->fire(base, t, role, midi, vel);
    else schedule_at(t, midi ? midi : DK_ROLE_MIDI[role], base + role, vel, DK_ROLE_DUR[role]);
}
static void dk_fire_at(int delay_ms, int role, int midi, int vel) {
    if (role < 0 || role >= DK_N) return;
    if (vel < 1) vel = 1; if (vel > 7) vel = 7;
    if (dk_kit && dk_kit->fire) { dk_kit->fire(dk_base, audio_time() + delay_ms / 1000.0, role, midi, vel); return; }
    schedule_hit(delay_ms, midi ? midi : DK_ROLE_MIDI[role], dk_base + role, vel, DK_ROLE_DUR[role]);
}
// a role's volume (0..1), on every slot that role plays (a snare's head AND its wires). Use this, not
// instrument_level(base + role), or a layered role's extra slots keep their old level.
static void dk_level_at(int base, int role, float lvl) {
    if (role < 0 || role >= DK_N) return;
    instrument_level(base + role, lvl);
    if (role == DK_SNARE) instrument_level(base + DK_L_WIRES, lvl);
}
static void dk_level(int role, float lvl) { dk_level_at(dk_base, role, lvl); }
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
static const DrumKitDef DK_ELECTRO = { "ELECTRO", dk_build_electro, 0 };

// ACOUSTIC — a real drum kit, voiced (2026-11-04) against a CC0 recording of one: lennartgreen's
// "Vintage Hip Hop" kit, recorded at one gain, so it also gives the LEVELS (build/refs/drums-cc0,
// node tools/fetch-refs.js). The voices are drumhand's, which was tuned against the same kit by
// measurement and by ear: a membrane kick with its strong second mode (54 + 118 Hz) and a beater
// slap, a membrane snare head over noise WIRES (its own slot, DK_L_WIRES), toms with the mode ~2.1x
// up that a real tom has and a membrane leaves 17-20 dB down, the METAL bank as hats (played high,
// low-passed: real hats stop at ~14 kHz) and crash. The balance between drums is in the per-note
// gains below, so a caller's dk_level() trims the kit as a whole the way a mixer would.
static void dk_build_acoustic(int b) {
    instrument(b + DK_KICK, INSTR_MEMBRANE, 0, 0, 7, 70);
    instrument_harmonics(b + DK_KICK, 0.80f); instrument_timbre(b + DK_KICK, 0.12f); instrument_morph(b + DK_KICK, 0.30f);
    // A HIGHPASS under every drum (2026-11-04): a struck membrane rings around zero, but a voice
    // switched on and left to decay leaves a DC THUD under the hit (measured 3-26 dB more energy
    // below each drum's note than the real kit; the TR-77 taught it, tuning-drum-voices.md). The
    // kick's old LOWPASS 6000 did nothing a membrane kick needed, so the slot's filter went here.
    instrument_filter(b + DK_KICK, FILTER_HIGH, 28, 1);

    instrument(b + DK_SNARE, INSTR_MEMBRANE, 0, 0, 7, 60);            // the head
    instrument_harmonics(b + DK_SNARE, 0.70f); instrument_timbre(b + DK_SNARE, 0.15f); instrument_morph(b + DK_SNARE, 0.10f);
    instrument_filter(b + DK_SNARE, FILTER_HIGH, 100, 1);        // no DC thud (see the kick)
    instrument(b + DK_L_WIRES, INSTR_NOISE, 0, 170, 0, 50);            // the wires
    instrument_filter(b + DK_L_WIRES, FILTER_LOW, 7000, 2);
    instrument_accent(b + DK_L_WIRES, ACC_CUTOFF, 1.0f);
    instrument_accent(b + DK_L_WIRES, ACC_VOL, 0.0f);

    for (int h = DK_HHC; h <= DK_HHO; h++) {                           // closed / open: one pair of cymbals
        instrument(b + h, INSTR_METAL, 0, 0, 7, 60);
        instrument_harmonics(b + h, 0.70f); instrument_timbre(b + h, 0.85f); instrument_morph(b + h, 0.85f);
        instrument_mode(b + h, MODE_METAL_DECAY, 0.66f);
        instrument_mode(b + h, MODE_METAL_SPREAD, 0.20f);
        instrument_env(b + h, 0, ENV_PITCH, 0, 80, 1.4f);
        instrument_filter(b + h, FILTER_LOW, 12500, 4);
    }
    instrument_mono(b + DK_HHO, 1);
    instrument_choke(b + DK_HHC, b + DK_HHO);                          // closing the hat stops it ringing

    instrument(b + DK_CLAP, INSTR_NOISE, 0, 140, 0, 50);               // (no clap in the reference kit)
    instrument_filter(b + DK_CLAP, FILTER_BAND, 1500, 4);

    instrument(b + DK_TOM_LO, INSTR_MEMBRANE, 0, 0, 7, 80);
    instrument_harmonics(b + DK_TOM_LO, 0.30f); instrument_timbre(b + DK_TOM_LO, 0.50f); instrument_morph(b + DK_TOM_LO, 0.15f);
    instrument_filter(b + DK_TOM_LO, FILTER_HIGH, 45, 1);        // an octave under the note
    instrument(b + DK_TOM_HI, INSTR_MEMBRANE, 0, 0, 7, 60);
    instrument_harmonics(b + DK_TOM_HI, 0.30f); instrument_timbre(b + DK_TOM_HI, 0.55f); instrument_morph(b + DK_TOM_HI, 0.15f);
    instrument_filter(b + DK_TOM_HI, FILTER_HIGH, 65, 1);

    instrument(b + DK_CRASH, INSTR_METAL, 0, 0, 7, 400);
    instrument_harmonics(b + DK_CRASH, 0.55f); instrument_timbre(b + DK_CRASH, 0.30f); instrument_morph(b + DK_CRASH, 0.30f);
    instrument_mode(b + DK_CRASH, MODE_METAL_DECAY, 0.68f);
    instrument_mode(b + DK_CRASH, MODE_METAL_SPREAD, 0.15f);
    instrument_filter(b + DK_CRASH, FILTER_LOW, 7000, 4);
    instrument_mono(b + DK_CRASH, 1);
}
static void dk_note_(double t, int midi, int slot, int vel, int gate, float gain, float tb, float h, float mo, float damp, float acc) {
    NoteOpts o = { .gain = gain < 0.02f ? 0.02f : gain, .timbre = tb, .harmonics = h, .morph = mo, .damp = damp, .accent = acc };
    schedule_note_at(t, midi, slot, vel, gate, &o);
}
static void dk_fire_acoustic(int b, double t, int role, int midi, int vel) {
    int tr = midi ? midi - DK_GM_ANCHOR[role] : 0;                     // a GM note transposes the drum
    switch (role) {
    case DK_KICK:   // a short bent attack + the 118 Hz mode + the beater's slap, over a long 40 Hz BOOM
        // The real kick is two things (re-measured 2026-11-04): an attack that starts near 90 Hz and
        // drops to ~58 within 30 ms (the membrane BEND, morph 0.5), then a deep boom at ~40 Hz that
        // carries most of its energy (its spectrum PEAKS at 40 Hz) and is still ringing at -29 dB
        // after 400 ms. The boom is its own note, with a harmonics offset of -0.40 for a longer
        // membrane ring (t60 ~1 s against the slot's 0.6). Measure a kick in TIME (band envelopes
        // in 10 ms steps), not with a long tapered FFT window: that window all but erases the first
        // 30 ms, and once read the real kick as "broadband 90-350 Hz, 11-23 dB over ours" when its
        // 90-350 Hz band is in fact within ~3 dB of this one.
        dk_note_(t, 31 + tr, b + DK_KICK, vel, 60, 0.57f, -0.06f, 0, 0.5f, 0, 0);
        dk_note_(t, 44 + tr, b + DK_KICK, vel,  80, 0.65f, 0, 0, 0, 0, 0);
        dk_note_(t, 72 + tr, b + DK_KICK, vel,  10, 0.15f, 0.85f, 0.2f, -0.3f, 0, 0);
        dk_note_(t, 27 + tr, b + DK_KICK, vel, 900, 0.40f, 0.0f, -0.40f, 0, 0, 0);
        break;
    case DK_SNARE:  // head + wires
        dk_note_(t, 55 + tr, b + DK_SNARE,   vel, 55, 0.36f, -0.03f, 0, 0, 0, 0);
        dk_note_(t, 60,      b + DK_L_WIRES, vel, 55, 0.057f, 0, 0, 0, 0, 0.25f);
        break;
    case DK_HHC:    // the tip, closed: -20 dB in ~75 ms
        dk_note_(t, 81 + tr, b + DK_HHC, vel, 40, 0.028f, -0.03f, 0, 0, 0.92f, 0);
        break;
    case DK_HHO:    // the tip, open
        dk_note_(t, 77 + tr, b + DK_HHO, vel, 1400, 0.039f, -0.25f, 0, 0, 0.46f, 0);
        break;
    case DK_TOM_LO: case DK_TOM_HI: {   // fundamental + the strong mode ~2.1x up
        bool hi = role == DK_TOM_HI;
        int n = (hi ? 47 : 40) + tr;
        dk_note_(t, n,      b + role, vel, hi ? 120 : 80, hi ? 0.95f : 0.63f, 0, 0, 0, 0, 0);
        dk_note_(t, n + 13, b + role, vel, hi ? 70 : 70,   hi ? 0.62f : 0.68f, 0, 0, 0, 0, 0);
        break;
    }
    case DK_CRASH:
        dk_note_(t, 96 + tr, b + DK_CRASH, vel, 2600, 0.042f, 0, 0, 0, 0, 0);
        break;
    default:
        schedule_at(t, midi ? midi : DK_ROLE_MIDI[role], b + role, vel, DK_ROLE_DUR[role]);
    }
}
static const DrumKitDef DK_ACOUSTIC = { "ACOUSTIC", dk_build_acoustic, dk_fire_acoustic };

static void dk_use(const DrumKitDef *def, int base) {
    dk_base = base;
    dk_kit = def;
    if (def && def->build) def->build(base);
}

#endif // DE_DRUMKIT_H
