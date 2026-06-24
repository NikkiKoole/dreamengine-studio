# Field-based road rendering — one distance field, every feature a threshold

> **Genre: proven method (in `skewlab`) → queued basis for a streetlab refactor. NOT yet in
> streetlab.** This is the home for "how we draw roads correctly at skew/curve, and the plan to adopt
> it." The method is proven and committed in the `skewlab` cart; streetlab still uses the per-arm
> approach (plus the interim fixes in [`streetlab-corner-symmetry-plan.md`](streetlab-corner-symmetry-plan.md)).
> Companion to [`software-canvas.md`](software-canvas.md) (where per-pixel field rendering belongs)
> and [`rasterization-consistency.md`](rasterization-consistency.md) (the coverage-outline precedent).

## Why (the problem this solves)

streetlab draws road edges **per-arm**: each arm is a 1px-proud `BROWNISH_BLACK` casing band with
`DARK_GREY` asphalt on top, plus a per-corner kerb. At a **skew** or on a **curve**, arms overlap at
an angle, so one arm's casing lands *inside* another's asphalt → **interior stray pixels** and
**overpainted (missing) edges**; and per-arm markings don't align → **doubled lines**. Each symptom
has needed its own guarded special-case (`mirror_blit`, the casing fillet), which *accumulates*
ad-hoc code. The `skewlab` probe (and the 2026-06 corner work) traced every one of these to the same
root: **edges drawn per-arm instead of as a property of the whole road.**

## The method

Compute **one lateral distance field** and make every road feature a threshold on it:

```
latDist(x,y) = perpendicular distance from the pixel centre to the NEAREST road centreline
```

| feature | rule | notes |
|---|---|---|
| asphalt | `latDist < HW` | the road surface |
| kerb (N-px) | `latDist ∈ [HW−N, HW]` | **uniform width at ANY angle/curve** (per-arm casing can't) |
| sidewalk | `latDist ∈ [HW, HW+SW]` | another nested band |
| lane line @ offset d | `\|latDist − d\| < 0.5` | yellow centre (d=0), white dividers/edge |

It is **generator-agnostic** — it reads *coverage*, not geometry — so straight, skew and
**curved/Perlin** centrelines all render identically. It's the same "inside AND a neighbour outside"
coverage rule the engine already uses for `circ`/`poly` outlines, generalised to the road union.
Proven live in `skewlab` (`Z` per-arm↔union · `C` skew↔curve · `T` 1–3px kerb · `K` cross-section ·
`L` lane lines).

## The contract — the boundary that makes it PORT, not get rebuilt

This is the rule that keeps it from turning into "build it twice." Three layers, one interface:

| layer | what | lives where | on a streetlab port |
|---|---|---|---|
| **field source** | `latDist` from the centrelines | **per-cart** (skewlab: segments; streetlab: arms + curved edges) | recomputed from streetlab geometry |
| **render rules** | asphalt/kerb/sidewalk/lane-lines = thresholds on `latDist` | **portable** | **reused verbatim** |
| **longitudinal / semantic** | dashes, junction-box line-clearing, stop bars, turn arrows, crosswalks | **separate layer** (needs along-road arc-length, or is per-approach) | stays per-approach as today |

So the port is mechanical: *compute the field from streetlab's geometry, call the same rules.* The
render rules never fork. And the things that genuinely **can't** be field-driven are fenced off on
purpose — they were never coverage bands.

## Porting to streetlab (the refactor, when it's picked up)

1. **Build the field:** `latDist` = min perpendicular distance over all present arm centrelines
   (bearings through the hub; `HW = cross_hw()`), and over the curved-edge polylines in network view.
2. **Render from it:** fill `latDist < HW` (asphalt) → kerb band → sidewalk band (the `LIGHT_GREY`
   footprint already does a union-ish thing) → lane lines.
3. **Delete** the now-redundant special-cases: `mirror_blit` (a union fill is *already* symmetric),
   the casing fillet, and `stroke_corner` (the corner kerb falls out of the field). **Net removal of
   code** — the whole point.
4. **Keep separate** (longitudinal/semantic, unchanged): dashes (need along-road arc-length; real
   roads clear lane lines through the junction box anyway), stop bars, turn arrows, crosswalks.
5. **Reconcile the modes** (where the real work is — they all compose into the field/footprint):
   roundabout (the field includes the circulatory ring), free-right islands, T (fewer arms),
   sidewalks (already union-ish), bike lanes (another nested band).
6. **Verify:** `mirror-diff` 0 (now *without* the blit), `spec` 104/0, and a visual sweep at
   default / skew / T / bike / peds / roundabout / free-right / network-curve.

## Caveats (honest boundaries)

- **Perf:** per-pixel coverage is O(area) and today would be `pset`-per-pixel. That's the
  [`software-canvas.md`](software-canvas.md) direction — on a CPU canvas these become cheap `cbuf`
  writes; on today's GPU immediate-mode path a full-screen field is heavier. Measure before shipping
  it as streetlab's default; it may want to land *with* the canvas.
- **Thin features (<2px)** merge — a road pinching to nothing or a hairpin is a degenerate case.
- **Dashes / directional marks / junction-box clearing** are NOT field-driven (the separate layer).

## See also
- `skewlab` (the reference cart — the proven method, all toggles)
- [`streetlab-corner-symmetry-plan.md`](streetlab-corner-symmetry-plan.md) — the interim 4-way
  symmetry + notch fixes this refactor would supersede
- [`software-canvas.md`](software-canvas.md) — where per-pixel field rendering belongs (+ determinism)
- [`rasterization-consistency.md`](rasterization-consistency.md) — the CPU coverage-outline precedent
- [`road-program-state.md`](road-program-state.md) — whole-program road state
