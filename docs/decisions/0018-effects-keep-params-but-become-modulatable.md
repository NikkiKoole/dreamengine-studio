# 0018 — Effects keep their own params (no instrument-style macros), but their params should be modulatable

Date: 2026-06-15 · Status: accepted

## Context

Instruments have the **three-macro core** ([0017](0017-three-macro-core-plus-engine-aux-channel.md)):
`harmonics`/`timbre`/`morph` — a fixed, normalized 0..1 knob set whose meaning each engine maps for
itself, and (the real power) which are **uniform modulation targets**: `LFO_TIMBRE`, `ENV_HARMONICS`,
`note_macro`, CV all work on *any* engine, and the macro is engine-smoothed so sweeping it is artifact-free.

After the boutique-effects arc (grains, univibe, dropout, shallow, amp-noise, gate, shimmer — all with
bespoke named params), the question came up: **should effects get the same 3-macro treatment?**

## Decision

**No instrument-style macro abstraction for effects. Keep each effect's bespoke, named params.** Three
reasons:

1. **Heterogeneous param counts.** Instruments genuinely converged on ~3 axes, so a fixed 3-macro shape
   *fit*. Effects don't: ring-mod has 2 params (freq/mix), phaser/leslie have 5. A forced 3-macro shape
   would either pad the small effects with invented knobs or hide controls on the big ones — i.e. **lose
   params we need or invent ones we don't.** That's the opposite of the macro discipline's goal.
2. **The "amount" macro already exists, and so does the preset layer.** Nearly every effect has a 0..1
   `mix` that *is* the universal more/less. And [`effects-recipes.md`](../guides/effects-recipes.md) is the
   effects' preset/"good starting values" layer — the recipe half of what instrument macros provide
   (consistent with [0015](0015-effects-are-recipes-not-primitives.md): effects are recipes, not a new
   primitive abstraction).
3. **SET-AND-HOLD.** The instrument macro's value is *smoothed modulation*. Effects are set-and-hold:
   reconfiguring buffer/coefficient DSP per frame (crush bit-depth, tape/chorus ring buffers) churns the
   audio thread → stutter. So a blanket "modulate any effect macro" layer can't exist safely — not all
   params are cheap to sweep.

## But the instinct is right — effects should be modulatable (the real want)

The macro idea points at a genuine gap: **you can't `LFO`/envelope/CV an effect param generically** the
way you can a voice macro. `modrack`'s "CV inlet into a module" is the hand-built version of exactly this,
and **we want effect params to be LFO-able in modrack.**

So the accepted *direction* (future work, not built here):
- Give the **cheaply-sweepable** effect params a normalized, **slewed modulation target** — filter cutoff,
  `mix`/sends, rate/depth LFOs — explicitly **not** the buffer-reconfiguring ones (bit-depth, ring-buffer
  lengths). The already-slewed `note_cutoff`/`note_reverb`/`note_vol` setters are the proven pattern.
- This is "a modulatable control + shared mappings," **not** "3 macros." Don't copy the macro *shape*;
  copy the *modulation plumbing*.
- Also DRY the per-cart `0..1 → param` mappings (`pedalboard`/`modrack`/`groovebox` each re-derive them) —
  document the canonical maps in `effects-recipes.md`.

## Consequences

- The effect roster stays as bespoke named functions + `mix` + recipes. No `fx_macro()` / 3-macro family.
- Future modrack work: expose the **sweep-safe** effect params as LFO/CV targets (per-param opt-in by
  "is this cheap to modulate"), so an effect can wobble under an LFO like an instrument macro can — without
  hitting the set-and-hold stutter.
- Cross-refs: [0015](0015-effects-are-recipes-not-primitives.md) (recipes not primitives),
  [0017](0017-three-macro-core-plus-engine-aux-channel.md) (the instrument macro core this contrasts with),
  and the effects modulation note in [`audio-notes.md`](../design/audio-notes.md) / the SET-AND-HOLD callout
  in [`effects-recipes.md`](../guides/effects-recipes.md).

## Proposed API (sketch — not built; for the modrack implementer)

A **curated target enum** (the safety boundary — only params cheap to update continuously; it is
*impossible* to modulate a buffer-reconfiguring param, so the API can't be misused into a stutter):

```c
#define FXMOD_FILTER_CUT   0   // the DJ-filter cutoff — the marquee modrack sweep
#define FXMOD_FILTER_RES   1
#define FXMOD_DRIVE        2   // drive amount (a tanh shaper — cheap, great under an LFO)
#define FXMOD_REVERB_SEND  3   // a send LEVEL (not the tank)
#define FXMOD_DELAY_SEND   4
#define FXMOD_TREM_DEPTH   5
#define FXMOD_PAN_DEPTH    6
#define FXMOD_WAH          7   // wah position (auto-wah-by-CV)
#define FXMOD_GRAINS_MIX   8
#define FXMOD_SHIMMER_MIX  9
// … closed set; grows only when a param proves BOTH useful AND cheap to sweep.
// Deliberately NO crush bit-depth, NO chorus/flanger/tape ring buffers.
```

Two entry points:

```c
// CV / per-frame — the one modrack needs. Engine SLEWS internally → per-frame updates don't zipper.
void fx_mod(int bus, int target, float value);              // bus 0 = master; value normalized 0..1
void instrument_fx_mod(int slot, int target, float value);  // resolve slot → its FX bus (fx_bus_for)

// Engine-side LFO — set-and-forget convenience for carts with no CV graph (no per-frame request spam).
void fx_lfo(int bus, int target, float rate_hz, float depth, float center);   // depth 0 = detach
```

> `fx_lfo`'s waveform should come from the unified **`LFO_SHAPE_*`** enum proposed in STATUS #39
> (sine/tri/square/saw/S&H/random/optical — DSP mostly already in the modulation kit). Same shape
> vocabulary for `instrument_lfo`/`note_lfo` and `fx_lfo`.

modrack usage (a FILTER module patched from a CV node):
```c
fx_mod(0, FXMOD_FILTER_CUT, cv_value);   // per frame; engine slews → smooth sweep, no stutter
```

Notes for whoever builds it:
- **`fx_mod` is the core** (modrack produces its own CV per frame → it wants a *sink*). `fx_lfo` is sugar.
- **Values normalized 0..1**, mapped per-target internally (uniform + modrack-friendly, matches the macro
  feel) rather than natural units.
- Mirrors the static-vs-modulated split that already exists for voices: `filter()`/`drive_insert()` =
  set-once config; `fx_mod`/`fx_lfo` = the continuous layer on top (like `instrument_timbre()` vs `LFO_TIMBRE`).
- Internally each safe target reuses the existing slew (the `note_cutoff`/`note_reverb` one-pole), and is
  applied on the audio thread — no new per-frame request flood for the `fx_lfo` path.
