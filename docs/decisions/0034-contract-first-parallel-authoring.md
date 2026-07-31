# 0034 — Freeze a contract before fanning out parallel authors

status: accepted
date: 2026-07-31

## Context

`lockup` ([design](../design/lockup.md)) is the largest cart in the library: a Prison Architect
demake, ~12,250 lines across eight files. It was written by **eight agents in parallel, none of
which could see any other's code**, and it **linked on the first attempt** with `spec()` green at
55/55.

That outcome is not normal, and the reason is worth writing down: it was **not** the fan-out. It
was what happened *before* the fan-out.

Prior art the owner supplied, which names this family of technique:
**["The Gauntlet Loop", Matt Shumer, 2026-07-27](https://somethingbig.ai/gauntlet-loop)** — a
methodology for driving agents toward a high standard. Its five steps: decompose into the smallest
independently-improvable pieces; separate builder and critic agents (the critic inspects the output
without the builder's justifications); give a **concrete benchmark** (real reference material — his
example is Call of Duty screenshots) rather than vague quality words; the critic compares against
that reference and sends work back; and keep looping rather than stopping after N rounds. Its core
principle, quoted:

> "Never let the builder grade itself."

## Decision

**Before fanning out parallel authors, freeze a contract header. Then give each agent exactly one
file.**

The four rules that made it work, all of which are load-bearing:

1. **The contract is frozen first, and it holds the DATA, not just the signatures.**
   `runtime/lockup/model.h` declares every type, enum, global and function signature — *and* defines
   the tables (what a cell requires, what a bed costs, the nine needs and their decay rates, the
   sprite slot map). Putting the data in the contract is what stops two modules disagreeing about
   the world. Eight authors could not drift on what a room *is*.
2. **One file per agent, and no agent may touch another's bytes.** Not "coordinate carefully" —
   mechanically disjoint ownership.
3. **Every internal symbol is `static` AND module-tag-prefixed** (`lkg_`, `lkp_`, `lka_`, `lke_`,
   `lkr_`, `lks_`, `lkh_`). The whole cart is one translation unit, so an unprefixed
   `static int idx(...)` in two modules is a build break. This single rule removed the entire class.
4. **Modules include the contract and never each other.** The contract is the only shared surface,
   so include order can't matter and there is no dependency graph to get wrong.

Separately, and per the Gauntlet Loop: **the builder never grades itself.** The visual pass and the
critics were different agents, and the critics could not edit files — only bake frames and judge.

## Consequences

The honest ledger, including the parts that did not work.

**What it bought.** Eight modules, 12,252 lines, first-attempt link, `spec()` 55/55, 2.05ms/frame.
Two agents independently arrived at correct solutions to problems nobody assigned them (the art
agent bucketed its `pal()` swaps by role, avoiding a measured 105ms/frame trap; the path agent
replaced a specified downhill walk with cheaper source-propagation and said why).

**Debugging does not parallelise.** Every fix after integration was serial and mine: seeding the
population, the spec failures, the glyph bug. Fan-out compresses *authoring*, not *convergence*.
Budget accordingly — the fan-out was roughly half the total effort, not most of it.

**A flaw in the contract propagates to every agent at once.** My capacity rule ("sum the slots of
objects matching `cap_obj`") was wrong: desks, cookers and workshop tables all have `slots == 0`,
so every office, kitchen and workshop would have been *permanently full at capacity zero*. The grid
agent overrode it and documented why. Contract-first concentrates authority, and therefore
concentrates author error — so invite the override explicitly, as the brief did.

**Agents cannot see each other, so cross-cutting facts need a human holding them.** Two findings
(the `pal()` batching cost; that `mouse_world_x()` already exists and requires the camera to be set
in `update()`) arrived from a scout *after* launch. Nothing in the architecture could have
distributed them. The orchestrator has to carry them and reconcile at integration.

**The loop is weakest exactly where you most want it: judging quality.** Both critics graded the
result *not AAA* ("an impressively engineered renderer wearing the wrong colours"), and their top
three faults were real. But the fix pass before them improved one fault of four, and it took the
owner's own eye to settle direction. Treat critics as fault-finders, not as a quality ratchet.

**We could not do step 3, and that was the run's real weakness.** The Gauntlet Loop's concrete
benchmark was unavailable: Prison Architect's frames are not in this environment, so the critics
judged against a *written* description of the game's visual language
([lockup.md §4](../design/lockup.md)) rather than the artifact. **The blind side-by-side the brief
originally asked for was therefore impossible, and no comparison verdict was fabricated** — both
critics were instructed to state that plainly, and did. This is why the visual bar stayed the
softest gate in the whole exercise, while the *simulation* bar — which had a real oracle in
`spec()` — held firmly.

**The generalisable lesson:** a dimension with a deterministic oracle converges; a dimension judged
by description does not. If a future run wants the visual half to converge like the logic half did,
commit real reference material (`docs/design/reference/`) and run it through
`tools/pixelsnap.js --palette pico32` so a critic judges like-for-like instead of penalising a
720×450 32-colour canvas for not being 1080p.

## See also

- [docs/design/lockup.md](../design/lockup.md) — the worked example, and its §4 visual bar
- [ADR-0022](0022-collaboration-is-the-north-star.md) — the two-part bar; the "legible to a
  stranger" half is precisely the half no oracle checks
- [ADR-0006](0006-library-carts-not-engine.md) — why these eight headers live with the cart rather
  than joining the shared `runtime/` shelf
