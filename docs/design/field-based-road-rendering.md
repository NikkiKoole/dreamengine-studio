# Field-based road rendering — one distance field, every feature a threshold

> **Status: SHIPPED in streetlab as OPT-IN (2026-06-24).** The whole junction renders on the field
> behind `DE_FIELD_ROADS` / the live `g` key — **default OFF**, so the per-arm `mirror_blit`/casing
> route stays the shipping path. Covered: plain · skew · T · turn-lanes · typed cross-section
> (median/bike/parking/sidewalk, incl. continuous bike corner-wrap) · mini-roundabout · free-right
> (generous corner + concentric slip + pork-chop island). Verified by three gates — `road-check --all`
> 16/16, `spec` 104/0, `mirror-diff` 0 kerb. Network view alone stays per-arm (its many curved edges
> are the genuinely canvas-dependent case). **The refactor is essentially done here.**
>
> **What remains is deliberately deferred, NOT abandoned:** flipping the default to field-on and
> deleting the old route (`mirror_blit`, casing pass, casing-fillet, `stroke_corner`/`fill_corner`
> for plain corners, the banked `sline`) — the doc's "net removal of code" — happens only once the
> [`software-canvas.md`](software-canvas.md) makes the per-pixel field fill cheap (the per-config A/B
> baseline below is the number to beat). Decision: keep the old route as the shipping default until
> then. (The software canvas is already in flight — `studio.c` Phase 0.)
>
> The method itself is proven in the `skewlab` cart (the reference); this doc is its home.
> Companion to [`software-canvas.md`](software-canvas.md) (where per-pixel field rendering belongs)
> and [`rasterization-consistency.md`](rasterization-consistency.md) (the coverage-outline precedent).
> The interim fixes this supersedes (when the old route is finally removed) are in
> [`streetlab-corner-symmetry-plan.md`](streetlab-corner-symmetry-plan.md).

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
6. **Verify — three gates, run all three:**
   - `node tools/road-check.js streetlab --all` — the **correctness oracle** (framebuffer invariants:
     no naked edges / strays / floating kerb), run MARKINGS OFF, across the whole config matrix. This is
     the gate that works *at any angle* — the one that catches the field's real bugs (mirror-diff can't
     see a skew problem). `--overlay`/`--zoom` show *where* a failure is.
   - `node tools/mirror-diff.js streetlab --band 20,110` (built `-DDE_FIELD_ROADS,-DDE_NO_MARKINGS`) =
     **0 kerb** *without* the blit — symmetry, only meaningful for the orthogonal case.
   - `node tools/spec.js streetlab` = **104/0** — the pure geometry fns.
   Plus a visual sweep at default / skew / T / bike / peds / roundabout / free-right / network-curve.

## Caveats (honest boundaries)

- **Perf — MEASURED (2026-06-24), and it's not the distance math.** A 2-segment-crossing probe
  (the shape a streetlab 4-way reduces to — collinear arm pairs merge to one line each) split the
  cost cleanly: a full-screen field is **2.58ms**, of which **only ~0.71ms is the whole scan +
  per-pixel distance math** — the other **~1.87ms is the per-pixel `pset` submission** (the
  `rlVertex3f` ceiling, [`software-canvas.md`](software-canvas.md)). The `sqrt`/`hypot` itself is
  **noise** (2.58→2.52ms swapping `hypotf` for a squared-distance compare), so don't bother
  micro-optimising the field math. Consequences for the streetlab port:
  - **The junction view is affordable on today's GPU path.** Full-field ≈ 2.58ms (under the 16.6ms
    budget); bounding the field to a **junction-core bbox** (the *core-box hybrid* — field only
    where arms overlap, cheap per-arm `polyfill` bands for the straight corridors) is **~0.45ms**,
    *cheaper than streetlab's current 0.92ms* because it deletes `mirror_blit`'s `pget` readback. So
    perf does NOT force a choice here — core-box vs full-field is a code-deletion/risk call, not a
    speed one (core-box keeps the straight-arm casing band; full-field deletes all per-arm casing).
  - **Only the network/curve view actually needs the canvas.** Its cost is pset-*count*-driven (road
    pixels across the whole screen + overdraw from overlapping curved edges), which is exactly what a
    `cbuf` makes cheap. So land the junction modes on the GPU behind `DE_FIELD_ROADS`; defer
    network-curve to land *with* [`software-canvas.md`](software-canvas.md).
- **Thin features (<2px)** merge — a road pinching to nothing or a hairpin is a degenerate case.
  (The arm↔disc *armpit* cusp on a skewed roundabout was exactly this; `fr_render`'s `fr_put`
  fills a 1px pinch — road on opposite sides — with asphalt instead of leaving an enclosed kerb.)
- **Dashes / directional marks / junction-box clearing** are NOT field-driven (the separate layer).

### Archive baseline — field-OFF vs field-ON, per config (MEASURED 2026-06-24, clean room)

The whole junction is now on the field (behind `DE_FIELD_ROADS`, runtime `g`). This is the "before"
snapshot ahead of the software-canvas phase. `node tools/play.js streetlab … --frames 180` per config
(perf.json `workMsAvg` + `pset`/frame); **must be run with the editor closed** — a concurrent
`build/cart` races `build/cart.c`/`perf.json` and the numbers go junk.

| config | OFF ms | ON ms | OFF pset | ON pset |
|---|---|---|---|---|
| default | 1.17 | 4.23 | 2062 | 14713 |
| skew 35 | 0.52 | 3.62 | 23 | 15586 |
| T | 0.61 | 4.10 | 23 | 12909 |
| bike | 1.83 | 5.49 | 2922 | 18535 |
| median | 1.34 | 4.70 | 2738 | 17786 |
| parking | 0.97 | 2.43 | 3308 | 20007 |
| pavement | 1.09 | 3.68 | 2738 | 14713 |
| roundabout | 0.82 | 3.60 | 23 | 14669 |
| free-right | 0.78 | 3.59 | 23 | 14834 |
| skew+bike+park+peds | 0.74 | 4.68 | 23 | 23881 |

**Reading it:** field-ON is **3–5ms** vs field-OFF **0.5–1.8ms**, and the delta tracks `pset`/frame
**1:1** — field-ON full-fills 13k–24k px/frame (the coverage surface+kerb) where field-OFF emits a few
thousand at most (`mirror_blit` readback, or near-zero at skew/T/roundabout/free-right where the blit
is skipped). So the field's added cost is **entirely per-pixel `pset → rlVertex3f` submission**, not
the distance math (confirmed by the 2-segment probe above). All still under the 16.6ms/60fps budget on
M1, but this is the exact cost the [`software-canvas.md`](software-canvas.md) `cbuf` removes (per-pixel
writes become stores). It's the strongest single argument for landing the canvas — and the number to
beat after it ships.

## See also
- `tools/road-check.js` — the **correctness oracle / gate** for this refactor (framebuffer invariants at
  any angle; `--all` config matrix; `--overlay` failure map). Run it after every mode you bring onto the field.
- `skewlab` (the reference cart — the proven method, all toggles)
- [`streetlab-corner-symmetry-plan.md`](streetlab-corner-symmetry-plan.md) — the interim 4-way
  symmetry + notch fixes this refactor would supersede
- [`software-canvas.md`](software-canvas.md) — where per-pixel field rendering belongs (+ determinism)
- [`rasterization-consistency.md`](rasterization-consistency.md) — the CPU coverage-outline precedent
- [`road-program-state.md`](road-program-state.md) — whole-program road state
