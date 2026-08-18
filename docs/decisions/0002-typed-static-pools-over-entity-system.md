# 0002 — Typed static pools, not an engine-owned entity system
Date: 2026-05-30 · Status: accepted

## Context
A long external brainstorm proposed an engine-owned entity system — a God-struct / `SELF`
global / `val[16]` "alterable values" / ECS / union-entity — plus generic data structures
(lists, maps, grids, spatial hash) and memory arenas. The pitch was "pro engine" power.

## Decision
Keep entities as **per-game typed static pools with named fields** (`Enemy en[140]; bool on;`).
No engine-owned entity layer, no generic containers, no arenas.

> **Status: accepted; HALF SUPERSEDED 2026-08-14.** The entity half stands — there is still no ECS
> and no engine-owned entity layer, and that is not being revisited. The **"no arenas" clause is
> dead**: `de_state_for` / `de_state_for_saved` (`studio.h`) are a keyed, engine-owned, reallocating
> arena (`DeArena` in `studio.c`), and they are public cart-facing API used by `ui.h`, `gestures.h`,
> `tr909.h`, `cart_ctx.h`, `acidcandy` and `pedalboard`. It exists because an AUv3 runs several
> instances in ONE process and a cart is one translation unit, so per-instance state needs an
> address-keyed home — a need this ADR could not have seen. See
> [`../design/engine-context.md`](../design/engine-context.md). The "no cart calls `malloc`"
> observation below has also relaxed (a handful do).

## Why
This is a learn-C console — the data layout *is* the lesson. `e->hp` is strictly clearer
than `e->val[0]`. The carts already prove the pattern scales: `robotron.c`, `vampire.c`
(140 enemies), `boids.c` all use naive typed pools and nearest-loops with no performance
problem at our entity counts. Indices already serve as "handles"; `str()` already provides
the reusable scratch buffer arenas are pitched for. No cart calls `malloc`.

## Consequences
- DS structures, ECS, arenas, MMF movement/qualifier engines all out of scope.
- "Settlers/Sims power" is pursued via **library carts** (see [0006](0006-library-carts-not-engine.md)).
- `fget`/`fset` per-sprite flags remain a candidate (small, additive) — not an entity system.
