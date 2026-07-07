# 0028 — Sensible defaults, optional tweaks (don't agonize)

Date: 2026-07-07 · Status: accepted

## Context
Designing `epiano`'s scale-grid ([`epiano-layout-brief.md`](../design/epiano-layout-brief.md))
surfaced a recurring trap: the isomorphic grid could offset rows by a fourth (chord-friendly) or an
octave (legible). Both are defensible; **reasonable players genuinely differ**, and no amount of
staring at a silent mock settles it — it's a feel/sound call. The instinct was to keep the decision
"open" until certain. That stalls the work, and the maker named the fix outright: *"make it a
preference, ship a sensible default, and stop agonizing."*

The pattern is everywhere in this project — `GRID_MAX_OCT`, the default scale, which knobs earn a
compact slot, a hundred small numeric constants. Two failure modes bracket it: **agonizing** (block
on the one true value that doesn't exist) and **over-configuring** (expose every value as a setting,
building a settings wall that violates the beginner-as-critic bar of
[ADR-0022](0022-collaboration-is-the-north-star.md) — "would a stranger get it?").

## Decision
**Pick a sensible default, ship it, and leave a seam — don't agonize, and don't pre-build
configuration.**

- **Default first.** For any value where reasonable users could differ, choose the option a stranger
  gets *immediately* (the legible one, not the power-user one) and ship it. The default carries the
  soul; it must clear the ADR-0022 two-part bar on its own.
- **Leave a seam, not a setting.** Keep the alternative reachable in the code — an enum + a single
  switch point (like `epianofit`'s `isolayout` cycled by `i`), not necessarily a surfaced UI
  control. "Change it later" should be a one-line change, not a rewrite.
- **Surface a tweak only on evidence of need** — when real use (or a real user) shows the default
  doesn't fit a legitimate second audience. Then the tweak lands behind progressive disclosure
  (device-adaptive-layout.md's disclosure model), never on the main performing surface.
- **Drop the defective option** — a "choice" that's just worse (e.g. a layout that breaks on reflow)
  isn't a preference; cut it rather than offer it.
- **Feel/sound defaults get confirmed on glass** — ship the *provisional* default, then play it;
  don't stay open waiting to be sure.

## Consequences
- **Momentum** — decisions close instead of lingering as open questions; a brief can mark a call
  *decided (provisional)* and move on.
- **Lean surfaces** — the main UI shows a default, not a settings panel; tweaks are opt-in and
  disclosed, so the beginner still gets a great cold-open.
- **Cheap reversibility** — the seam means the default can flip or become a real setting the moment
  it earns it, at one-line cost.
- **The bar shifts to the default's quality** — since the default is what almost everyone lives with,
  the taste goes into *picking it well*, not into building knobs.

Worked example (the one that prompted this): `epiano`'s grid layout → **default ISO-OCT** (legible,
stable across reflow), **ISO-4TH kept as a seam/optional tweak** for chord players, **LINEAR dropped**
(reflow-defective). Same shape for `GRID_MAX_OCT = 4`, the default scale, the compact-knob picks.

Related: [ADR-0022](0022-collaboration-is-the-north-star.md) (the two-part bar the default must
clear) · [`../design/design-system.md`](../design/design-system.md) §5 (house conventions) ·
[`../guides/responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) (the playbook that
applies this) · [`../design/device-adaptive-layout.md`](../design/device-adaptive-layout.md) (where
disclosed tweaks live).
