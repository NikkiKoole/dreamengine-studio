# pseudo-3D city — the cityview render bench

STATUS: bench shipped (cart `cityview`), 2026-06-24. The "how it works" note behind
[decision 0021](../decisions/0021-road-geometry-in-2d-sandbox-render-is-an-adapter.md) (which fixes the
*policy* — geometry stays 2D, this view is a downstream adapter). This file is the *mechanism*: the
projection, the one invariant, the findings, and the projected-primitive helpers. Cart header in
`tools/carts/cityview.c` is the living source; this outlives it.

## The look: GTA1 meets Zelda
Top-down, but you see the **fronts** of things. A **parallel oblique projection**: the ground stays a
plan `(x,y)` map (rotated by the camera + uniformly scaled), and **height pushes pixels straight up the
screen** — no perspective foreshortening. So a flat road keeps its exact plan shape; only a `project()`
sits between the world and the screen.

```c
// cityview.c — the one adapter. world (x,y,z) → screen.
sx = SCREEN_W/2 + (dx·cosθ − dy·sinθ)·zoom;          // θ = camera rotation
sy = SCREEN_H/2 + (dx·sinθ + dy·cosθ)·zoom − (z − camz)·zoom;   // height & camera-height go UP
```

### The one invariant
> A wall's **footprint edge may go diagonal** under a rotated camera (so does the road grid — that's
> fine, it's the heading-up turn). But its **UP must always point straight up the screen.** Break that
> and you get the melty "diagonal walls everywhere" look (mode 4 below). This single rule is the whole
> trick — it's why the look stays crisp and pixel-clean instead of reading as cheap isometric skew.

## Findings — four building view modes (press M)
A box is a footprint + walls extruded up + a roof cap. Which walls show = the ones whose screen-space
outward normal points **opposite the roof's shift direction**. One test covers all geometric modes:

| mode | camera | result | verdict |
|---|---|---|---|
| 1 NORTH-UP | locked north | always the south fronts, zero diagonals | calm map view |
| 2 HEADING-UP | rotates to heading | the 1–2 walls you drive toward, UP still screen-up | **the keeper** |
| 3 BILLBOARD | heading-up | flat cards facing you | props (trees/people), not boxes |
| 4 LEAN-OUT | heading-up | roofs pushed off-centre → diagonal walls | the cost, kept for contrast |

Headless A/B (same corner, same heading) settled it: **heading-up is the look.** Lean-out was the
instructive failure; billboard is for thin props, not buildings.

### Textured walls
Walls texture-map via `tritex`, tiled by **world size** (each ~16px tile maps to the full texture slot)
so bricks stay crisp under rotation *and* each affine triangle stays small (no PS1 swim). Side-facing
walls get a sparse dot/quarter dither shadow (a 50% checker collapses into diagonal lines on a sheared
quad — don't). Textures are code-drawn in `cityview.cart.js` (brick / concrete block / window glass /
stucco). Toggle with **T**.

## Raised highways (flyovers) — press F
A flyover is just a **road whose nodes carry a `z`**. Because height extrudes up-screen, a deck whose
`z` ramps up reads as an elevated structure: running surface + dashed centre line + dark **fascia**
(thickness) + **pillars** to the ground + a dithered **ground shadow**. Layouts: straight / s-curve /
spiral on-ramp / two-level stack interchange.

- **Drive up onto it.** The car's `z` snaps to the nearest deck *within a reachable step* of its current
  height (`|deckz − carz| < REACH`), so you board at a ramp end (z≈0) and climb — and you drive **under**
  high decks without popping onto them (the bug that taught us the reach gate).
- **The camera rises with you** (`camz` eases to `carz`) so the city sinks below — the GTA flyover feel.
- **Depth** is painter's-order by base footprint: correct for separated structures incl. the stack
  crossing; a building tucked right under a deck edge can mis-layer. Fine for a bench; a real build wants
  finer segment sorting or per-pixel depth.

## The projected-primitive vocabulary (ADR 0021)
The reusable half — proven in `cityview`, destined for a runtime header once a second 3D front-end wants
them (the `ui.h` "cart-land capability" pattern). **That trigger has FIRED (2026-07-10):** `citydrive`
is the second front-end and carries its own copy of `project`/`wpt`/`pdisc`/`pproj_poly`
(`citydrive.c`), already diverged (its `pdisc` samples terrain `ground_z()` and dropped the z param;
its `project()` reads a different state struct). Graduation is now a build item — see Next.

- **`project(x,y,z)`** — the one adapter. Everything else calls it.
- **`pdisc(cx,cy,r,z)`** — a ground circle is an **ellipse** once rotated; fill a projected N-gon, never
  `circfill`. (ponds, the car's ground shadow, future roundabout islands.) *The one genuinely new helper.*
- **`pline` / `pdash`** — projected (dashed) line; project the endpoints, draw. (the deck centre line.)
- **`pproj_poly`** — project-then-`polyfill`, respects `fillp`. (lot quads, deck ground shadows.)

## Next
- **Graduate the projected primitives to `runtime/proj3d.h`** (ADR-0021's condition is met —
  `citydrive`'s copy is drifting from `cityview`'s). Design points the divergence already exposed:
  camera state must be a *parameter* (a small struct: camx/y/z, rot, zoom, hscale + the pinhole
  fields pitch/eye/setback), `pdisc` takes explicit z (callers pass `ground_z()` themselves), and
  the hard-coded N=14 should scale with projected radius so big discs don't show facets. Migrate
  both carts onto it in the same change.
- **roadlab → cityview data spike (preferred).** Pull `roadlab`'s real computed geometry — lane
  boundaries + the M5 elevation `z(s)` (`z*0.45,0.85` drop-shadow today) — out of the 2D cart and render
  it through `cityview`'s `project()` as an actual flyover. If a trumpet renders right with one coordinate
  adapter, the whole road family transfers (ADR 0021). (Alternative: a `project()` toggle inside roadlab.)
  Today cityview's flyovers use *synthetic* decks — this swaps in real ones.
- **The hybrid:** mode-2 textured boxes + mode-3 billboard props (trees/people/signs) in one scene.
- **Building collision** so it's a street, not a diorama; then wire to the world composer's `road_at()`
  (big-game seam #2).

## Related
- [decision 0021](../decisions/0021-road-geometry-in-2d-sandbox-render-is-an-adapter.md) — the policy this implements.
- [`field-based-road-rendering.md`](field-based-road-rendering.md) — the three-layer contract (geometry / render rules / longitudinal) this view is an adapter for.
- [`big-game-backlog.md`](big-game-backlog.md) — seam #2, the cityscape renderer this bench serves.
- [`road-program-state.md`](road-program-state.md) — authoritative road-tier status (where roadlab's `z(s)` lives).
