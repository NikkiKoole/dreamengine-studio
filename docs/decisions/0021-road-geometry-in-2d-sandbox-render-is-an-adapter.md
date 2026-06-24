# 0021 — Road geometry is developed in the 2D top-down sandbox; the pseudo-3D view is a rendering adapter

Date: 2026-06-24 · Status: accepted

## Context
The road family — `streetlab` (at-grade junctions + the street network) and `roadlab` (grade-
separated interchanges) — is built and refined as **2D top-down** carts. Separately, a pseudo-3D
city look exists in `cityview`/`overpass`: a **parallel oblique projection** (`cityview.c` `project()`)
where the ground is the plan `(x,y)` map rotated + uniformly scaled, and height pushes pixels straight
up the screen (no perspective foreshortening). The maker likes that view and asked whether the road
work transfers to it — and whether it's fine to keep developing the carts here and use the logic there.

The answer hinged on one fact: **in that projection the ground plane is still a 2D map** — a flat road
keeps its exact plan shape, only rotated and scaled. So the geometry the carts compute is already in the
right coordinate space; only the *drawing* assumes axis-aligned screen space. `roadlab` is further along
than it looks: its M5 already computes a real elevation profile `z(s)` and fakes the oblique look with a
height-proportional drop-shadow (`z*0.45, z*0.85`) — i.e. it's already half-projecting. `overpass.c` is a
working proof of the deck/pillar/shadow/depth-sort backend that `roadlab`'s flyovers imply.

## Decision
**Road geometry is developed, specced, and gated in the 2D top-down sandbox. The pseudo-3D view is a
rendering adapter the geometry feeds — never a place geometry is re-authored.** The boundary that makes
this hold:

> **Geometry functions emit world-plane coordinates `(x,y[,z])`, never screen coordinates and never
> axis-aligned-screen assumptions.** A `project()` is the only thing that changes between the 2D and
> pseudo-3D front-ends.

This is the same three-layer contract `field-based-road-rendering.md` already states: **field source /
geometry** (per-cart, world-plane → reused verbatim) · **render rules** (thresholds/fills → gets a
`project()`-aware adapter) · **longitudinal/semantic** (dashes, arrows, stop bars → stay per-approach).

## Why
- **The gates live in 2D and validate the portable half.** `spec.js` (pure geometry), `road-check.js`
  (framebuffer invariants at any angle), `mirror-diff.js` all assert in the top-down view. Develop in the
  projected view and those become far harder to write. The 2D sandbox is the test rig; geometry is the asset.
- **The projection is strictly downstream.** Curb returns, lane offsets, cross-sections, the network graph
  don't care whether a `project()` sits before the screen, so refining them here improves the pseudo-3D
  view for free, later.
- **The work is an adapter, not a rewrite.** The expensive, design-heavy part (the grammars) is done and
  projection-agnostic. What ports is the draw layer.
- **The field route is the *better* fit for the projected view** (and is already in flight): inverse-project
  each screen pixel to the ground plane and evaluate `latDist`, instead of converting every primitive to a
  projected quad. So the field refactor and a future 3D view reinforce each other rather than compete.

## Consequences
- **Keep doing what we're doing** — refine `streetlab`/`roadlab` here, run the gates here. `streetlab` is
  at-grade (z=0) ⇒ it becomes the ground layer; `roadlab` already has `z(s)` ⇒ its grade-separation stops
  being a draw-order fake and becomes *real* geometry in the projected view (it improves).
- **The screen-space shortcuts are the only things that don't port, and they're already on the chopping
  block:** `mirror_blit` (reflects about x=160 — dies under rotation), axis-aligned `circfill` islands
  (become ellipses ⇒ projected polys), `rectfill` lane bands (⇒ projected quads). The field route deletes
  most of them anyway.
- **When the adapter is built, prefer a shared helper over per-cart copies.** The projected-primitive
  vocabulary (project-then-`polyfill`; a projected disc→ellipse; a projected dashed line) is wanted by
  `cityview`, `overpass`, and any `streetlab`/`roadlab` 3D front-end — so it belongs in a runtime header
  (the `ui.h`/`gestures.h` "cart-land capability" pattern, ADR 0006), not duplicated. Not built yet; this
  decision just fixes *where* it goes. (A projected line is nearly free — `line()` takes any coords; the
  disc→ellipse is the one genuinely new helper worth having early.)
- **Don't port full road detail into `cityview`/`overpass` before the adapter is proven.** The cheapest
  high-signal spike is to pipe `roadlab`'s M5 `z(s)` deck through `overpass`'s projector — if a trumpet
  renders as a real flyover with one coordinate adapter, the thesis holds for the whole family.
- **Not in scope:** true perspective (this view is parallel-oblique by design); the software canvas, which
  is where per-pixel field fill becomes cheap (its own track, `software-canvas.md`).
