# Decisions (ADR-lite)

One file per **decision that has rationale worth freezing** — especially decisions to
*not* do something, which otherwise get relitigated. These are **append-only**: you
don't edit an accepted decision, you write a new one that supersedes it (and set the
old one's status to `superseded by NNNN`).

This is deliberately lightweight — no tooling, no review workflow. The value is just:
*the why has a home, so it stops leaking into the planning and reference docs.*

## Status legend

- **accepted** — in force.
- **superseded by NNNN** — replaced; kept for history.
- **proposed** — under consideration (rare here; usually we decide then record).

## Template

```markdown
# NNNN — <short imperative title>
Date: YYYY-MM-DD · Status: accepted

## Context
What situation forced a choice. The alternatives on the table.

## Decision
What we chose, in one or two sentences.

## Why
The reasoning — especially the evidence (cart corpus, audience, constraints).

## Consequences
What this implies elsewhere (docs to update, things now out of scope).
```

## Log

| # | Decision | Status |
|---|---|---|
| [0001](0001-cut-coroutine-process-model.md) | Cut the DIV-style process/coroutine model | accepted |
| [0002](0002-typed-static-pools-over-entity-system.md) | Typed static pools, not an engine-owned entity system | accepted |
| [0003](0003-code-first-sound.md) | Code-first sound; tracker UI deferred | accepted |
| [0004](0004-degree-based-trig.md) | Degree-based trig (not radians or turns) | accepted |
| [0005](0005-defer-tile-collision-helper.md) | Defer `move_and_collide` tile-collision helper | accepted |
| [0006](0006-library-carts-not-engine.md) | Pathfinding/particles ship as library carts, not engine API | accepted |
| [0007](0007-pal-recolors-sprites.md) | `pal()` recolors sprites too, via a palette-swap shader | accepted |
| [0008](0008-cut-turtle-graphics-api.md) | Cut the turtle-graphics API | accepted |
| [0009](0009-small-3d-leaf-helpers.md) | A small set of 3D leaf-helpers (`rot3`/`project3`/`zsort`/`quadfill` + `V3`) | accepted |
| [0010](0010-fade-is-immediate-mode.md) | `fade()` is immediate-mode, not a sticky setter | accepted |

> Back-fill candidates (decisions made but not yet written up): C as the language /
> no-heap globals+stack; the `.cart.png` zTXt format; carts-carry-their-own-settings
> (`de:settings`); the PICO-8-vocab-for-basics / lean-Scratch naming principles
> (currently in [`../design/api-notes.md`](../design/api-notes.md) "naming review").
