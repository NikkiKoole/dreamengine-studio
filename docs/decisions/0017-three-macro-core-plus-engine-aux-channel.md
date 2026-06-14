# 0017 — Keep the 3-macro core; a blessed per-engine aux channel for the exceptions (not a 4th macro)
Date: 2026-06-10 · Status: accepted

## Context
Every modeled engine exposes the same three live, normalized macros — `harmonics` / `timbre` /
`morph` — and nothing else ([instrument-engines.md §8.1.1](../design/instrument-engines.md), the
Plaits/MicroFreak discipline). The surface is `O(1)`: it never grows with the engine count.

But real engines keep wanting *something past three*, and each has improvised its own hatch:
- **GUITAR / PIANO** want fundamental-reinforcement weight + an attack click → `eng_tune(slot, idx, value)`, read **at note-on**.
- **BOWED** wants a pizzicato/arco **excitation** switch → also `eng_tune` (`eng_tune(slot,0,1)`), note-on.
- **VOICE** (`INSTR_VOICE`) wants **four-plus continuous timbre axes** (the `vox4` by-ear audition landed on vowel / size / effort / nasal) and a pile of probe params → it invented `voice_param(handle, idx, value)`, read **live on a held note**, plus `voice_consonant`/`voice_coda` for timed articulation.
- **BRASS** wants a mute / plunger wah.

So the recurring question — "should we relax 3 → 4, or add a generic engine hook?" — came to a
head with brass (the mute) and voice (genuinely four axes). The forcing observation: **we already
have two un-blessed aux channels** — `eng_tune` (note-on) and `voice_param` (live) — that are the
*same concept at two timing points*, mirroring how each macro has both `instrument_X(slot,…)`
(note-on) and `note_X(handle,…)` (live).

## Decision
**Keep the universal core at exactly three live macros. Do NOT add a universal 4th.** Route every
"past three" need into one of four lanes, by *what the thing actually is*:

| lane | what it is | timing | examples | mechanism |
|---|---|---|---|---|
| **3 macros** | universal **intrinsic timbre** | live | bore · brassiness · breath; vowel/size/effort | `instrument_X` / `note_X` (the fixed six) |
| **per-voice output controls** | shape the **output**, every engine | live | mute/wah, vibrato, slide, pan | `note_cutoff`/`note_res`/`note_pitch`/`note_glide`/`note_lfo`/`note_pan`/`note_drive`/`note_echo` |
| **per-engine aux params** | extras **past three**, indexed, per-engine | note-on **and** live | pizz (note-on) · voice's nasal/effort + raw probe params (live) | the blessed union of `eng_tune` (note-on) + `voice_param` (live) |
| **articulation events** | a **timed gesture** injected into a held note — not a knob | event | "bah", "ahh-m" | `voice_consonant` / `voice_coda` |

And **promote the per-engine aux channel from "EXPERIMENTAL, bake-to-constants later" to a
permanent, documented mechanism** — recognizing `eng_tune` and `voice_param` as its note-on and
live faces. It is explicitly *not* a continuous performance macro for everyone; it is an indexed,
per-engine channel for engines that genuinely exceed the three-macro timbre core.

**The rule of thumb** (the dividing line, so the next engine doesn't relitigate):
- Does it **shape the output**? → a universal per-voice control. *(mute = a swept filter on the
  output; it never touches the engine. Vibrato = `note_lfo`. The slide = `note_pitch`.)*
- Is it **continuous intrinsic timbre**? → one of the three macros. If an engine seems to need a
  *fourth* continuous timbre axis, first suspect it's really **two engines** (HARP folded into
  GUITAR; Rhodes/Wurli/Clav into EPIANO) — and only if it truly isn't, use the live aux channel.
- Does it **change how the sound is generated** (excitation / mode / structure)? → per-engine aux
  (`eng_tune`, note-on). *(You can't filter a bow into a pluck.)*
- Is it a **timed gesture** mid-note? → an articulation event, its own call.

There is a built-in tell for the lane: macros + per-voice controls are read **live**; aux *mode*
config is read **at note-on**. So anything that must move *while a note rings* (the plunger wah!)
literally cannot be the note-on hatch — which is why the mute belongs on `note_cutoff`, and why
voice needed the *live* `voice_param`, not `eng_tune`.

## Why
- **A universal 4th macro is paid everywhere, forever, to serve ~3 engines.** Every engine would
  have to define its 4th (PLUCK/SINE/etc. have no meaningful one → dead knobs), every preset grows
  a value, every showcase cart a slider, plus the four-places wiring and new `LFO_`/`ENV_`
  destinations. The 3-macro rule's whole value is that it's small and universal.
- **The evidence says the strain was never a missing 4th *knob*.** Where engines actually pushed
  past three: bowed wanted a *mode* (pizz), guitar/piano wanted *refinements*, brass wants
  *output filtering*. Only VOICE wants genuine extra continuous axes — and voice is exactly the
  engine that should use the live aux channel, not force a 4th macro on the other ten.
- **Engine ids and aux indices are cheap; clean macro semantics are expensive** (the same logic as
  [0016](0016-combo-organ-recipe-then-macro-or-engine.md): a meaning that shifts across the dial is
  the §8.1.1 sin). A per-engine indexed channel keeps each engine's three macros meaning exactly
  one thing.
- **Mutable themselves weren't dogmatic.** Plaits is 3 macros **+ a model selector**; **Rings is
  four** (Structure/Brightness/Damping/Position). "Three" was always "three *plus* model/config,"
  not a hard ceiling. `eng_tune`/`voice_param` is our version of that escape valve — we just hadn't
  named it as one mechanism.

## Consequences
- **VOICE has a sanctioned home.** `voice_param` stops being "EXPERIMENTAL, will-be-removed"; when
  the voice workstream locks its public axes (the `vox4`/`voxab`/`voxpad` probes), the eventual 3
  live macros land on the standard surface and the remaining axes (nasal/effort beyond the three,
  the probe levers) live on the live aux channel. **This ADR fixes the *mechanism*, not voice's
  specific axis mapping — that stays the voice workstream's call.**
- **BRASS mute is settled: not an engine concern at all** — it's `note_cutoff`+`note_res` in the
  cart (a swept resonant filter = the plunger wah), the same lane as the vibrato/slide controls
  already in `brass.c`. No 4th macro, no engine hook.
- **`eng_tune` doc status upgrades** from throwaway scaffolding to a documented per-engine channel.
  Open implementation question (gated — `sound.h` is mid-refactor and the unification touches it):
  whether `eng_tune` (note-on) and `voice_param` (live) literally merge into one named pair, or
  stay two entry points sharing the contract. The *principle* (this ADR) is decided regardless.
- **§8.1.1 gets a pointer here** so a future "should we add a 4th macro?" reads the considered
  answer (and the four-lane table) instead of re-deriving it.
- Does not supersede §8.1.1; it **extends** it — the three-macro core is unchanged; this records
  where everything *else* goes.

## Open question — what "locked, non-experimental" looks like
The principle above is decided. What's still open (gated on the `sound.h` refactor) is the **final
shape** of the channel. Two candidate shapes, and the test that decides between them.

### Shared across both shapes
- **Indexed, named, per-engine.** Whatever the function shape, indices are addressed by `AUX_*`/`MODE_*`
  names, never a bare number (VOICE already does this via `VP_*`; only `eng_tune` still uses bare `0`/`1`).
- **Non-experimental, concretely:**
  1. `studio.h` comments drop "EXPERIMENTAL / not a final API" → "the blessed per-engine aux channel (this ADR)".
  2. The entry points **and** their constants get `studioDocs.js` + `shell.js` entries — in an **advanced
     "engine aux"** grouping, *discoverable but not pushed on beginners* (the §8.1.1 "not advertised" stays
     a placement call, not an absence). Clears the standing `api-usage` "missing from studioDocs/shell" nag honestly.
  3. Migrate the callers — `guitar`/`piano`/`bowed`/`upright`/`polopan` (note-on) and
     `say`/`vox`/`voxab`/`voxpad`/`voxlab` (live) — to the new names. Mechanical.
  4. `sound.h` keeps its `eng_p[]`/`vox_p[]` storage; only the public entry points rename.
  5. The VOICE workstream still owns *which* of its axes graduate to the three standard macros vs. stay
     on the live channel — this ADR fixes the mechanism, not voice's axis map.

### Shape A — one pair, two timing faces (the macro mirror)
```c
void instrument_aux(int slot,   int idx, float value);  // note-on face — replaces eng_tune
void note_aux      (int handle, int idx, float value);  // live face    — replaces voice_param
```
Mirrors how every macro has an `instrument_X` (note-on) and a `note_X` (live) face over **one shared
per-engine index namespace**.

### Shape B — two distinct channels, split by what the thing IS (recommended)
```c
void instrument_mode(int slot,   int idx, float value);  // engine STRUCTURE / mode / config — note-on
void note_aux       (int handle, int idx, float value);  // engine LIVE EXPRESSION past the 3 macros — live
```
| channel | what it is | timing | enum | examples |
|---|---|---|---|---|
| `instrument_mode` | engine **structure / mode / config** | note-on | `MODE_*` | pizz vs arco, string weight, attack click, (model select) |
| `note_aux` | engine **live expression** past the 3 macros | live | `AUX_*` | voice vowel/size/breath/effort |

```c
// MODE_* — note-on, instrument_mode()
#define MODE_STRING_WEIGHT 0   // GUITAR/PIANO fundamental reinforcement
#define MODE_STRING_CLICK  1   // GUITAR/PIANO attack click
#define MODE_BOW_PIZZ      0   // BOWED: >= 0.5 = pizzicato (pluck) instead of arco (bow)
// AUX_* — live, note_aux()  (the current VOICE VP_VOWEL/VP_SIZE/VP_BREATH/VP_OPENQ/VP_TILT/… set)
#define AUX_VOICE_VOWEL    0
#define AUX_VOICE_SIZE     1
// … one block per engine that needs it; engines with no aux declare none
```

### The deciding test — single-face vs dual-face
Shape A copies the macro pair. But that pattern is honest for macros **only because macros are
dual-face**: `harmonics` is the *same* param you set at note-on *and* sweep live — two doors into one
thing. **Aux params are single-face**: pizz is note-on *only* (you can't bow-vs-pluck a ringing
string), vowel is live *only*. There is **no** aux param today that wants both.

So Shape A's shared namespace advertises a duality that doesn't exist — `instrument_aux(voiceSlot,
AUX_VOICE_VOWEL,…)` and `note_aux(h, AUX_BOW_PIZZ,…)` would both **silently do nothing**. Shape B's two
enums make the wrong-face call **unwriteable** (a category error you can't type), turning the worst
con — an invisible no-op that only surfaces on the next note — into a can't-happen. That is exactly
what the "name your indices" hazard wants.

**Recommendation: Shape B.** Cost is one extra name + one extra enum; the "O(1), one mechanism"
elegance frays slightly, but it stays O(1) in the sense that matters (neither enum grows with engine
count). **The hinge:** Shape A becomes the right call the moment any engine wants the *same* aux param
on *both* faces (a note-on default you can also sweep live, as macros do). Today **zero** engines do —
so ship Shape B; collapse to A only when a real dual-face param appears.

### The trade, weighed (applies to either shape)
**Pros** — `O(1)` API surface forever (a new engine quirk = pick an index, zero four-place wiring, no
dead knob on the engines that don't want it); keeps the 3-macro core pristine; proven prior art
(Plaits model-selector / Rings 4th knob).

**Cons** — an indexed channel is inherently expert-only and magic-number-prone (only survivable
*because* of the named constants — a bare `note_aux(h, 2, 0.7f)` is unreadable); per-engine semantics
mean the same index means different things on different slots (the "meaning shifts across the dial" sin
the macro discipline forbids — accepted **only** because it's quarantined to this lane). Shape A adds
the silent wrong-face no-op above; Shape B is the mitigation.

### Implementation checklist (Shape B) — ✅ IMPLEMENTED 2026-06-14
The full caller set is bounded and known, so a clean rename (no back-compat alias) was feasible —
this is a self-contained corpus, no carts in the wild. All steps below are done.

1. **Rename the two entry points** — `eng_tune` → `instrument_mode` (note-on), `voice_param` →
   `note_aux` (live). Identical behavior; the existing two functions already split exactly along the
   note-on/live line, so this is a pure rename, no logic moves. Touch the `studio.h` decls + the
   `sound.h` defs (targeted `Edit`s — both are hot shared files).
2. **Add named indices to `studio.h`** (the `MODE_*` enum for `instrument_mode`):
   `MODE_STRING_WEIGHT 0` / `MODE_STRING_CLICK 1` (GUITAR/PIANO) · `MODE_BOW_PIZZ 0` (BOWED, ≥0.5 = pizz).
   `note_aux` indices are **not** frozen into `studio.h` — the VOICE axis map is the voice workstream's
   `vox_p` set (cart-side `VP_*`), still evolving; `note_aux`'s doc just says "idx per-engine; VOICE = the probe carts' `VP_*`".
3. **Regenerate `studio_tcc_symbols.h`** (`node tools/gen-tcc-symbols.js`) so the libtcc live backend
   resolves the renamed symbols; land it in the same commit.
4. **Four-place wiring** — `studioDocs.js` + `shell.js` entries for `instrument_mode` / `note_aux` /
   the `MODE_*` constants, in an advanced "engine aux" grouping (discoverable, not pushed on beginners).
   Clears the standing `api-usage` "missing from studioDocs/shell" nag.
5. **Migrate callers** (rename + use the `MODE_*` names): note-on → `instrument_mode`: `guitar`,
   `piano`, `bowed`, `upright`, `polopan`; live → `note_aux`: `say`, `vox`, `voxab`, `voxpad`, `voxlab`.
6. **Rebake all 10 thumbnails** — the editor compiles each cart's *embedded* source, so a stale
   `de:source` calling the old name would fail to compile once it's gone. Mandatory, not optional.
7. **Gate**: soundcheck compile-gate (`node tools/play.js soundcheck script /dev/null --headless
   --frames 2`). Not a pitched change → no tune-check needed.

This resolved the "final shape" open question in favor of **Shape B** — shipped as `instrument_mode`
(note-on, `MODE_*`) + `note_aux` (live). The old `eng_tune`/`voice_param` names are gone (no alias);
all 11 carts migrated and rebaked. The dual-face hinge stays the documented trigger to revisit
(collapse to Shape A only if a real both-faces param ever appears). Remaining tidy-up (non-blocking):
a handful of internal `sound.h` implementation comments still say `eng_tune`/`voice_param` by their
old names, and the storage fields stay `eng_p[]`/`vox_p[]` — the sound workstream's call to rename.
