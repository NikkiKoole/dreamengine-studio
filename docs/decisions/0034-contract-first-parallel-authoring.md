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

## What to try next

Two of the Gauntlet Loop's five steps went unexecuted. In the order they would pay off:

1. **The concrete benchmark** (step 3 — the one this run could not do at all). Commit real reference
   screenshots to `docs/design/reference/prison-architect/` and run them through
   `tools/pixelsnap.js --palette pico32` so a critic compares like-for-like rather than being asked
   to penalise a 720×450 32-colour canvas for not being 1080p. Cheap, and it is the only thing
   standing between this cart and the blind side-by-side the brief asked for.
2. **Keep looping** (step 5). We stopped at one critic round for cost reasons. Stopping at N rounds
   is exactly what the method warns against — you stop when the critic stops finding faults, and
   with three blocking faults still open, one round demonstrably was not enough.

**The cheap experiment that would settle it:** with the screenshots in place, loop builder-and-critic
on **one fault only — the night lighting** — until the critic passes it. Night is both the worst
fault and the most self-contained, so it is the least expensive honest test of whether the loop
*converges* on this codebase or merely churns. If it converges on night, widen it. If it does not,
the loop is the wrong tool here, and that is worth learning for the price of a single fault rather
than a full sweep.

### One anecdote worth keeping, because it is the evidence for step 2 of the method

"Never let the builder grade itself" is not abstract advice; the failure it prevents happened here,
in one round. The visual-fix agent reported — in good faith and in detail — that it had fixed the
noisy ground, explaining the three scales it had restructured to do so. Both critics then *measured*
the result (≈19–25% fleck coverage at a +54 luminance step) and returned the same verdict
independently: **"changed, not fixed."** The owner's own eye agreed. Had the builder been trusted to
grade its own work, the run would have shipped believing its worst-known fault was closed. That is
the whole argument for builder/critic separation, in a single round, on a real codebase.

## Appendix — the original brief, verbatim

Kept as the honest record of what was actually asked for, typos and all. Reading it against the
Consequences above is the most useful part of this ADR: it is a good brief that names four things,
three of which the machinery delivered and one of which it could not.

```
I want you to recreate prison architect but in our dreamengine, use drawn sprites, make great
music utilitzing wht you can, make the music dynamic, The quality shoudl atleast be as good as
the real game, proeferably better.
It should be utterly perfect, visually beautiful, with every single thing done at AAA quality—from
textures to physics to anything you could think of.

Fan out sub-agents and have sub-agents tackle each one individually so that the game is utterly
perfect. You should /loop on each item and have a separate sub-agent check it visually to ensure
it looks triple A. That separate sub-agent should be a really harsh critic, and if it doesn't look
triple A, it should keep going.

Don't stop until each sub-agent is utterly wowed with the quality when compared with the actual
prison architect. It should literally compare them side by side blind and say which one looks
better. /loop until it's utterly perfect. Fan out sub-agents and ultracode.
```

What each ask actually produced:

| The ask | Outcome |
|---|---|
| "fan out sub-agents … tackle each one individually" | **Delivered, and it worked** — but only because of the contract, which the brief did not ask for and which turned out to be the whole trick. |
| "a separate sub-agent check it visually … a really harsh critic" | **Delivered.** Two critics, builder-separate, unable to edit. Both graded it *not* AAA and converged on the same three blocking faults, which is the strongest evidence the pattern works. |
| "make the music dynamic" | **Delivered** (layers/harmony/tempo tracking `lk.tension`) but **unverified** — the audio gates were never run. Listed in the cart's `todo[]`. |
| "literally compare them side by side blind and say which one looks better" | **Not possible, and not faked.** The real game is not in this environment. The critics judged against a written spec and said so. This is the one ask the machinery could not honour, and §Consequences explains why it is also the reason the visual half never converged. |
| "utterly perfect" / "AAA quality" / "don't stop until…" | **Not reached, and the cart says so** — `de:meta.todo[]` carries 19 items, three of them blocking. The loop was also deliberately stopped at one critic round for cost reasons, which is a divergence from the Gauntlet Loop's "keep looping". |

The lesson is not that the brief was wrong. It is that **"utterly perfect" is not a gate, and a
harsh critic is not an oracle.** `spec()` could settle whether a prisoner can reach a toilet;
nothing in the run could settle whether the prison looked good enough, so that judgement stayed
with the owner — which is where it belonged, and is why the cart was handed over to be played
rather than declared finished.

## See also

- [docs/design/lockup.md](../design/lockup.md) — the worked example, and its §4 visual bar
- [ADR-0022](0022-collaboration-is-the-north-star.md) — the two-part bar; the "legible to a
  stranger" half is precisely the half no oracle checks
- [ADR-0006](0006-library-carts-not-engine.md) — why these eight headers live with the cart rather
  than joining the shared `runtime/` shelf
