# FM box — the blind band brief (Phase 1)

STATUS: SHIPPED — `fmbox.c` is built (2026-07-10): six INSTR_FM machines on drummachine's chassis +
per-step parameter locks (hold-cell-drag-up, fill-height readout). Phase-1 brief for an all-FM
percussion groovebox (the Elektron **Model:Cycles** reimagined), written *before* opening any cousin
cart (the [cart-authoring-prompt](../guides/cart-authoring-prompt.md) firewall). Built around FM-drums + p-locks exactly as the verdict below.

> Working name **fmbox** (mirrors `groovebox`/`pocketbox`/`drummachine`). Alternatives that read the
> metallic character better: **sparks**, **metalbox**, **sixmachines**. Pick at build; the doc uses `fmbox`.

Written to answer one question: *we already have grooveboxes and an FM cart — what does a
Model:Cycles bring that isn't on the shelf?* The honest answer is **the collision, not either half**:
FM as the *percussion* source, six macro-knob "machines" you dial in seconds, and **per-step
parameter locks** — an interaction no cart has. This doc names the ideal machines + the one headline
interaction, which become the parts and knobs we build.

## Why this vessel, in one line

You can't hear an FM kit *breathe across a bar* from one struck note — the whole point of
Model:Cycles is that a tiny macro move **per step** turns a static loop alive (a snare that brightens
step by step, a tone whose pitch marches). A looping step grid with per-step macro automation is the
only vessel that shows it. `fm.c` (the single-voice modeling rig) structurally can't.

## What's already on the shelf (so we don't rebuild it)

- **FM synthesis** → `fm.c` owns the single-voice DX modeling rig (3 macros + ADSR, prints the
  `instrument()` call). We reuse its *engine*, not its screen.
- **Summed-bus effects** (pump / glue / crush / master EQ) → `groovebox.c` owns that family. **This
  cart must not re-headline it.** A light master send is fine; the rack is *not* the point here.
- **Analog drum machines** → `tr808` / `tr909` / `cr78` own modeled-analog percussion. FM percussion
  is a **different sound** — metallic, glassy, DX — that none of them make.
- **Step grid** → `drummachine`/`groovebox` own the plain 16-step toggle grid. Ours must earn its
  grid by adding the p-lock layer on top.

So the two new things — and the entire reason to build — are **§FM-as-drums** and **§p-locks** below.

## The world, from the music (not the palette)

FM percussion is its own sonic world, and it owns genres the analog drum carts can't voice:

- **Electro / electro-funk** — Model:Cycles' spiritual home: snappy metallic snares, glassy toms,
  clangy accents (Drexciya, Dopplereffekt).
- **IDM / braindance** — inharmonic, tuned, evolving percussion where each hit is slightly different
  (Autechre, µ-Ziq) — this is what p-locks were *made* for.
- **Footwork / juke** — fast tuned FM stabs and tom rolls chopped into triplets.
- **Techno (metallic/Berlin)** — ringing metallic percussion and FM cowbell/clave over a four-floor
  pulse.

The machine should slide between these by what you dial into the six macros and lock per step, **not**
by switching kits. It's an instrument, not a preset jukebox — the Elektron ethos exactly.

## The ideal band (intent-first) — six FM machines

Model:Cycles' six "machines" are one FM engine reconfigured by macro presets. We already have the
engine (`INSTR_FM`) and its three macros map startlingly cleanly onto the machine characters:

| # | machine | role | FM voicing (intent) | maps to our `INSTR_FM` as |
|---|---|---|---|---|
| 1 | **KICK** | the clock + low anchor | sine-ish carrier, hard pitch-drop, tight decay | `h≈0.0` (sub ratio) · fast pitch-env down · short D/R |
| 2 | **SNARE** | the backbeat crack | bright noisy FM + a body tone | `h` mid · `t` high (bright) · `m` up (clang/noise edge); pairs with a `INSTR_NOISE` layer |
| 3 | **METAL** | ringing metallic accent | inharmonic bell/clang, long ring | `fm/bell` / `fm/clang` (`h≈0.55` off-integer, `t` high) |
| 4 | **PERC** | tuned blip / tom | short glassy tuned FM, per-step pitch | `fm/finger-cymbal` shortened, or a tuned mid-ratio blip |
| 5 | **TONE** | bass / lead line | the melodic FM voice — sub bass or plucky lead | `fm/bass` / `fm/ostinato-bass` (already on the shelf) |
| 6 | **CHORD** | harmonic stabs | a strummed FM chord (the one polyphonic track) | `fm/epiano`/`fm/brass` voicing, 3–4 notes |

The soul is that **all six are the same engine** — the demo "turn one COLOR knob and the whole kit
goes from wood to glass to metal" is only possible because they share the FM formula. That unity is
the thing a stranger immediately *gets*.

### The six macros (the Model:Cycles control soul)

Model:Cycles gives each track exactly six knobs. Our engine already exposes what they need:

| M:C knob | what it does | our mapping |
|---|---|---|
| **PITCH** | base tune | `instrument_tune(slot, semis)` per track |
| **DECAY** | how long the hit rings | note ADSR D/R (per-hit) |
| **COLOR** | brightness / FM amount | `instrument_timbre` (mod index) |
| **SHAPE** | which machine character (ratio) | `instrument_harmonics` (carrier:mod ratio) |
| **SWEEP** | pitch-envelope amount | pitch-env (the `fm/ostinato-bass` `pitch-env →N` mechanism) |
| **CONTOUR** | amp-envelope shape / feedback grit | `instrument_morph` (feedback) |

Six knobs, one row — the immediacy is the point. **Do not** expose operators/ratios as numbers
(that's `fm.c`'s job); expose the six macros and let FM stay a black box you *play*.

## Shopping the palette (grabbable recipes — listed for intent, grabbed at build)

The metallic FM percussion palette **already exists** — lean on it, this cart's value is the
p-lock interaction, not new voices:

- **METAL / accents** → `fm/bell` (`h0.55 t0.60 m0.15`), `fm/clang` (`h0.55 t0.95 m0.95`),
  `fm/finger-cymbal` (`A0 D700 S0 R500 · h0.55 t0.75 m0.55`) — reuse on purpose.
- **KICK** → `fm/bass` punch (`h0.00 t0.75 m0.30`) plus a hard pitch-env, or borrow the 909's FM hats
  recipe for the top end.
- **TONE / bass** → `fm/bass`, `fm/ostinato-bass`, `fm/rhodes-bass` (the pitch-env bass is already
  built — grab its `pitch-env →2` for SWEEP).
- **CHORD** → `fm/epiano` / `fm/brass` voicing, 3–4 notes.
- **SNARE** → the genuine **gap**: a bright FM crack + `INSTR_NOISE` body layer. This is likely the
  one new recipe the cart contributes (`fm/snare`), and per the CLAUDE.md rule it goes into
  [`instrument-recipes.md`](../guides/instrument-recipes.md) on ship.

## The headline that's actually NEW — parameter locks (p-locks)

This is the whole reason the cart isn't "groovebox with FM voices." **Every step can carry its own
macro values.** Hold a step, turn COLOR → *that step only* gets brighter. The loop now evolves without
you touching it — a snare that opens across the bar, a TONE that walks in pitch, a METAL hit that
rings longer only on the accent.

- **Legibility (game-feel rule):** a locked step must *look* different — a tinted/haloed cell, and a
  little value ghost on the knob as the playhead crosses a locked step (the knob visibly jumps to the
  locked value, then snaps back). The invisible automation becomes visible.
- **Verifiable core (ADR-0022):** "step N holds macro value V; when the playhead hits N the voice is
  configured to V, elsewhere to the track default." That's a clean, oracle-judgeable invariant a
  `spec()` can assert — and it's the honest simulation behind the lo-fi surface.
- Keep it to **one or two lockable macros** in v1 (COLOR + PITCH are the most dramatic). Locking all
  six is the Elektron rabbit hole; ship the gesture, not the whole workflow.

## The play model

- **6 tracks × 16 steps** grid, tap to toggle a hit (the familiar part).
- **One selected track** shows its **six macro knobs** in a row along the bottom — dial the machine
  live, hands-free loop running.
- **P-lock gesture:** hold a step + turn a macro = lock that macro for that step (tinted cell +
  knob-ghost feedback). Hold + tap again to clear.
- A visible **playhead** sweeping the grid off the `beat()` clock; locked cells light as it passes.
- Mobile-first per `ui.h` — grid taps + a knob row + a hold-gesture is a natural touch instrument.

## Explicitly out of scope (belongs to other carts / the rabbit hole)

- **Summed-bus master rack** (pump/glue/crush/EQ) — `groovebox` owns it; at most a single master
  reverb send here so the focus stays on FM sculpting.
- **Operator/ratio programming as numbers** — `fm.c` owns the modeling rig; here FM is a black box you
  *play* via six macros.
- **Full Elektron workflow** (song mode, conditional trigs, per-step retrig/microtiming, full 6-macro
  locks, parameter slides) — the deep end. Ship the *gesture* (toggle grid + one/two lockable macros +
  the six-knob machine), not the manual.

## Chassis (deferred to Phase 2, per firewall)

Machines + macros + the p-lock model above are designed *before* reading cousins. For the build, copy
the **chassis** from the nearest step-sequencer (`drummachine.c` / `groovebox.c` grid + `beat()` sweep),
reuse `ui.h` for the grid + the knob row, reuse `pointer.h` for the hold-a-step-while-turning gesture
(the multitouch p-lock), and keep the six macros + the lock table as **named-enum data** (CLAUDE.md's
data-driven rule) so "make macro N lockable" stays a one-row change. Carry a `spec()` for the p-lock
invariant from the start (`streetlab` is the reference).

---

_Phase 1 only — not built. The verdict this brief encodes: worthwhile **iff** built around FM-as-drums
+ p-locks; a plain FM-voiced step grid would just reskin `groovebox` and fail the ADR-0022 second half
(delightful-to-a-stranger). The new value is the metallic FM kit the shelf lacks and the per-step
macro-lock gesture no cart has._

_The brief's LCD dancer (`fmbox.c` `draw_viz`) grew into its own experiment:
[`glyph-puppet.md`](glyph-puppet.md) — animation without frames._
