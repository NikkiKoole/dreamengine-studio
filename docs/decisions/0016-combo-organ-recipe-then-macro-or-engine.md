# 0016 — Combo organ: a recipe now; its own macro axis or engine only when a station proves it
Date: 2026-06-07 · Status: proposed (morph-fill verdict in — branch B; combo build still gated on a station, see Consequences)

## Context
`INSTR_ORGAN` is designed ([instrument-engines.md §8.8.4](../design/instrument-engines.md)):
a 9-drawbar **tonewheel additive** engine (Hammond), three macros — `harmonics` = snapped
registration, `timbre` = brightness tilt + key-click, `morph` = "animation" (scanner V/C chorus
depth + percussion). That covers gospel / soul / jazz-organ-trio / rock.

It does **not** cover the **transistor combo organ** (Vox Continental, Farfisa Compact) — a
genre-critical sound for **ska/reggae skank, 60s garage, The Doors** (the `roadhouse` station
already auditions "VOX/Gibson drawbar tables" in its THE BAND panel), and much of exotica/lounge.
The problem: a combo organ's character is its **non-sine divider source waveform** (filtered
pulse/square — the buzzy, reedy edge). A pure-sine additive engine can only **approximate** it
(a bright registration + `drive` grit), not model it.

So: is combo organ its own `INSTR_*`, its own macro axis *inside* `INSTR_ORGAN`, or a cart recipe?
[Decision 0015](0015-effects-are-recipes-not-primitives.md): *a primitive must prove it can't be a
recipe.* And a bounding observation that keeps this from being a slope — only **two** synthesis
methods say "organ": tonewheel-additive (Hammond) and transistor-divider (combo). Pipe/theatre
organ ≈ Hammond's additive territory or the planned **PIPE** engine; reed organ/harmonium *is* the
planned **REED** engine. The "organ" set is **closed at two**.

## Decision
Ship in this order, each step **gated on evidence, not built speculatively**:

1. **NOW — build `INSTR_ORGAN` (Hammond) per §8.8.4, and ship combo organ as a CART RECIPE:** a
   bright odd-harmonic registration + a baked `drive` nudge in the showcase cart. (Precedent: an FM
   *patch* is "macros + an ADSR" — §8.8.3; an organ patch is likewise macros + drive/ADSR.) Combo
   is a deliberate **approximation** at this stage — recognizable in a ska context, not an
   authentic Farfisa.

2. **TRIGGER A → one engine, repurposed macro.** *If* a built ska/garage/Doors station finds the
   bright+drive recipe isn't convincingly transistor, **and** building the Hammond shows the `morph`
   "animation" axis is **thin** (scanner-mode + percussion fold acceptably into baked per-preset
   patch state, freeing the macro): promote `morph` to a **consistent tonewheel↔transistor
   source-blend** axis inside the one `INSTR_ORGAN` (the oscillator swaps its partial waveform
   sine→divider along morph). One engine, O(1) surface, authentic combo, **clean macro semantics**
   (morph means one thing engine-wide). **Preferred over a second engine — when the macro is free.**

3. **TRIGGER B → a second engine.** *If* the recipe isn't enough **and** `morph` is genuinely needed
   for animation (no spare macro): add `INSTR_COMBO` (transistor divider) as its own engine. Each
   engine's three macros then mean exactly one thing. This is the **legitimate, bounded end-state**
   (two organ engines — the closed set above), not a slippery slope.

**Rejected in every branch:** the *meaning-shifting* hybrid — `morph` = scanner+perc on the Hammond
detents but something else on the combo detents. A macro whose meaning changes across the dial is
the §8.1.1 sin; off the table regardless of how the triggers resolve.

## Why
- **Second-customer rule** (the discipline that produced `radio.h`/`improv.h`, and promoted
  membrane): build the engine when a *real* customer proves the recipe insufficient, not because
  authenticity is attractive. `roadhouse` is customer #1; a ska/garage station would be #2.
- **0015's "prove it can't be a recipe":** the non-sine-waveform argument *suggests* combo is
  engine-level, but the bright+drive approximation may be good enough in context — only a built cart
  settles it. Don't pre-judge from the spec.
- **Engine ids are cheap; clean macro semantics are expensive.** Adding an `INSTR_*` costs zero API
  surface (§8.1.1). So a second engine (B) beats the meaning-shift hybrid; and repurposing a
  *genuinely spare* macro (A) beats a second engine, because it's cheaper still and keeps one engine.
- **The `morph`-fill test is the tell.** If `morph` can't find an honest engine-wide meaning, the
  animation axis was thin and combo-blend is its better use → branch A. If `morph` is a full axis,
  combo needs its own engine → branch B. The build decides; this record just fixes *how* it decides.

## Consequences
- §8.8.4 ships the Hammond engine + a combo *recipe* preset in `organ.c`; the spec gets a one-line
  pointer here.
- The combo question is **pre-reasoned**: a future context asking "should combo be its own engine?"
  reads this instead of re-deriving it from scratch.
- **Morph-fill verdict (2026-06-07, preliminary): FULL — no spare macro → branch B.** Playing the
  `organ.c` showcase (scanner chorus + percussion on `morph`), the third macro is a *full,
  distinct axis* — it is not free to repurpose. So **branch A (one engine, morph = combo blend) is
  ruled out**: when combo organ earns promotion (a built station proving the bright+drive recipe
  insufficient, per the second-customer rule), it lands as **branch B — its own `INSTR_COMBO`
  engine**, not a repurposed macro. *Still preliminary on the timbre side:* the per-registration
  sounds haven't been fully auditioned yet (organ-type familiarity), so the preset/mapping
  ear-tuning (§8.8.2 step 6) is open — but that tuning won't free up `morph`, so the branch-B
  verdict stands regardless.
- Does not supersede 0015; it is an **application** of it to a specific engine.
