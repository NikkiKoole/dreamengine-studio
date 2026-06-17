# Road geometry & the interchange DSL — master handoff (2026-06-16)

**★ START HERE** if you're picking up the interchange / road-junction-geometry work cold. This is the
map: how we got here, what we're solving, the research, what failed, and how to continue. The other
docs are the detail; this ties them together.

> **▶ Next session jumps to → the "★ NEXT SESSION — decided direction: `junction` + `laneLink`" section
> below.** `roadlab` now has the full vertical stack (geometry → clothoid → lanes → width → elevation,
> M1–M5, all committed). OpenDRIVE feature tracker: [`road-geometry-refs.md`](road-geometry-refs.md) →
> "OpenDRIVE adoption inventory".

## The 30-second version
roadnet2 needs to draw road junctions automatically (given crossing roads + a type → draw the ramps).
We hand-tuned one trumpet loop in `interchange.c`, realized **junctions can be described
declaratively** (a DSL), proved the idea in a sandbox (`rampkit`), **researched road-geometry
representations**, and concluded the geometry should be built on **arc-splines** (`LINE|ARC|CLOTHOID`),
not Béziers. `roadlab` is the clean foundation rebuilt on that finding. Nothing is wired into roadnet2
yet — it's all sandboxes + design docs, all committed.

## The cart lineage (how we got here)
1. **`roadnet`** (v1) — the original deterministic spline road-world + the street-level "sloop" drive
   idea. → [`roadnet.md`](roadnet.md), [`roadnet-handoff.md`](roadnet-handoff.md),
   [`roadnet-streetlevel.md`](roadnet-streetlevel.md).
2. **`roadnet2`** — the vector-native rebuild (one graph, one `road_at` query). Its **Part B** is
   hierarchy + **interchanges**, which is this whole thread. → [`roadnet2-plan.md`](roadnet2-plan.md),
   [`roadnet2-handoff.md`](roadnet2-handoff.md).
3. **`interchange.c`** — the junction-geometry **sandbox** spun off roadnet2 Part B: a highway + a
   crossing road + ramp drawers, fake-3D grade separation, a data-driven junction matrix. This session
   we hand-tuned the **trumpet outer loop** to good (candy-cane trunk, tangent entry, reverse-curve
   merge, a `loop end` slider) and started the **inner loop** (WIP). → [`interchange-handoff.md`](interchange-handoff.md).
4. **`rampkit.c`** — a **proof-of-concept** for the DSL idea: a ramp = `(portA, portB, type)`, built by
   one closed-form constructor (Béziers), + a nest-solver that proved nesting is tractable. Built
   **before** the research, so it's on Béziers + hand-placed ports.
5. **`roadlab.c`** — the **foundation**, rebuilt on the research: lane-accurate roads (drive-on-right
   via `DRIVE`), ports anchored to real lanes, ramps as **arc-splines**. **Supersedes `rampkit`**
   (which stays as the bezier proof). M1 (arc-spline) + **M2 (clothoid joints)** + **M3 (multi-lane via
   lateral offsets)** all done — and M3 *empirically confirmed arcs nest for free*, which likely retires
   the rampkit relaxation solver for the common case (see "How to continue").

## What we're solving
- **The concrete goal:** roadnet2 must draw ~12 junction types × topologies × angles × lane-counts
  automatically in worldgen. Hand-tuning each (like we tuned the one trumpet loop all session) doesn't
  scale.
- **The two geometry sub-problems** (the owner named these precisely):
  1. **"lines that can be driven easily"** = **curvature continuity** (G2). Curvature must change
     smoothly so you steer at a constant rate → **clothoids / Euler spirals**.
  2. **"roads snugly against each other"** = **parallel offset + clearance packing** → **arcs**
     (an arc offsets to an arc; concentric arcs nest with zero overlap for free).

## The two big ideas
1. **A junction DSL.** A junction = **legs × { movement → ramp-primitive }**. The primitive set is
   tiny (`through / diverge / merge / loop / flyover`) — exactly the `interchange.c` atoms. Every named
   interchange (diamond, cloverleaf, stack, trumpet…) is one assignment over a topology; they differ
   only in how the hard (left-equivalent) turns are served. → [`interchange-dsl.md`](interchange-dsl.md).
2. **Geometry as relations + a solver (a "language that isn't math").** Everything we said while
   tuning was a *relation* — "tangent", "fully on the far side", "nest", "merge along the lane" — never
   a coordinate. Two solver tiers: **closed-form per ramp** (tangent constructions) + **relaxation
   between ramps** (nesting/clearance). `rampkit`'s nest-solver proved Tier-2 converges. →
   [`interchange-dsl.md`](interchange-dsl.md) Layer 2.
3. **`DRIVE` constant** = single source of truth for handedness (+1 right / −1 left). Lets us read
   British reference diagrams and mirror in one place — and kills the chirality guesswork that bit us.

## The research (done this session)
Investigated OpenDRIVE, clothoids, SUMO/CommonRoad, Cities: Skylines, curve offsetting. Full report
+ sources + the recommendation: → **[`road-geometry-refs.md`](road-geometry-refs.md)**. Headlines:
- **Adopt an arc-spline model** (`LINE|ARC|CLOTHOID` segment list = OpenDRIVE's reference line, minus XML).
- **Arcs** offset/nest cleanly (deciding factor for "snug"); **Béziers don't** (a cubic's offset is a
  degree-10 curve → sample-and-refit + cusps).
- **Clothoid** via a **~15-line forward-integration loop** (no Fresnel, no solver) gives the drivable feel.
- **Cities: Skylines uses plain Béziers** between nodes — i.e. exactly where we started; arcs are an
  upgrade *for nesting*, not a fix for a mistake.

## What failed / dead-ends (so they're not re-walked)
- **Chirality flip-flop.** I claimed the reference GIF was drive-on-left — wrong; it's drive-on-right.
  The *actual* bug was over-rotating the loop: a single **270° tangent circle** forces a **cusp** at the
  exit. Fix: **180° bell + a reverse-curve** (bend one way, then the other to align with the road). The
  deeper lesson → make `DRIVE` explicit so handedness can't be argued by eye.
- **Béziers for ramps.** Fine for a single ramp, but they **fight offsetting/nesting** (degree-10). The
  research moved the foundation to **arcs** — hence `roadlab` over `rampkit`.
- **`rampkit`'s hand-placed ports weren't lane-accurate** (both highway ports on the same lane pointing
  opposite ways; the trunk "up" port floating above a road that only went down). Root cause: ports
  weren't anchored to lanes. **`roadlab` fixes this** — a port *is* a lane + its travel direction.
- **`interchange.c` inner loop** is still WIP/rough — the nesting wasn't dialed before we pivoted to the
  cleaner `roadlab` foundation.

## How to continue
- **`roadlab` M2 — clothoid joints** ✅ **DONE.** `clothoid_spline()` splices a spiral on each side of
  the arc (LINE→CLOTHOID→ARC→CLOTHOID→LINE) so κ ramps 0→1/R→0 — G2, "drives easily." Built on the §2
  forward-integration loop (no Fresnel): one local arm is integrated to measure the spiral **shift** `p`
  and **back-distance** `k`, the equivalent circle is offset inward by `p`, tangent dist
  `Ts=(R+p)·tan(Δ/2)+k`, then the real curve is integrated forward. Reduces exactly to `arc_spline` as
  Ls→0. `C` toggles it for A/B; `,`/`.` set the transition length Ls. Verified by A/B bake (the clothoid
  path sweeps wider + eases in/out; both close cleanly at the ports, no run-out kink).
- **`roadlab` M3 — multi-lane + nesting** ✅ **DONE, and it changed the plan.** Lanes are **lateral
  offsets of the one reference line** (OpenDRIVE's "same s, shifted in t", refs §1/§5): `offset_poly()`
  shifts each sample ⊥ to the local tangent; `draw_multilane()` strokes casing + asphalt + white interior
  dividers; `1`–`4` set lane count. **Key finding:** on arc ref-lines the offset lanes are concentric, so
  their gap is *constant by construction* — they **nest for free, no solver** (verified by a 4-lane bake:
  dividers stay parallel through the bend, no inner-edge pinch). Multi-lane lanes and nesting *separate*
  ramps are the **same operation** (concentric offsets at a spacing).
- **⇒ The `rampkit` relaxation solver is likely unnecessary** for the common case (cloverleaf loops,
  parallel lanes) — arcs dissolve the problem it was built (on Béziers) to solve. **Don't port it
  reflexively.** Reach for relaxation *only* if we hit a case arcs can't close-form: dense packing in a
  bounded footprint (a tight stack), or competing clearance from several neighbours. Until then, `rampkit`
  stays parked as the bezier-era proof.
- **`roadlab` M4 — lane add/drop tapers** ✅ **DONE.** `draw_multilane()` now takes a `taperFrac`: each
  lane has a `width(s)` (OpenDRIVE lane-width model), and the outer lanes taper full→0 over the ramp's
  tail in a **staggered staircase** (outermost drops first, `smoothstep` profile), so a fat ramp **fans
  down to one lane** — a real diverge with its gore filled — instead of peeling off as a slab. `offset_var()`
  is the per-sample (variable-distance) offset that makes it possible. `taper` 0-100% scrubs it (`t`
  cycles); `taperFrac=0` is exactly the M3 parallel lanes. (Fixes the "4-lane off-ramps look weird" slab.)
  *Open polish:* `keep` is hardwired to 1 (always fans to a single lane) — trivial to expose if we want
  N→2; and the taper is a lane *drop* at the tail (a lane *add* at the head is the mirror, not yet wired).
- **`roadlab` M5 — elevation `z(s)`** ✅ **DONE.** A ramp can **fly over** the junction: `z` rises from 0
  at port A, holds a deck height across the middle, falls to 0 at port B (rise/hold/fall hump, `smoothstep`).
  Depicted top-down via **option (a)**: the deck stays in plan position and draws on top; a **drop-shadow**
  offset ∝ `z` is cast on the ground first → reads as grade separation. `lift` px scrubs it (`e` cycles);
  `lift=0` ⇒ at grade. (We chose plan+shadow over oblique-lift to keep the flat-road identity + glued ports;
  draw-order-by-`z` is the rule that generalises to ramp-over-ramp.) *Open polish:* piers/supports under
  the deck; a real ramp-over-ramp crossing (needs two ramps + per-segment z-sort).
  - **Gotcha fixed (don't re-walk):** a ramp's **port A is the ENTRY** — its tangent must point *into* the
    junction. Using A's outward heading made the lead-in leave the wrong way and the curve loop back (a
    hook); offsetting that hook by ±14px (4 lanes) + a long spiral spiked into stray casing — the
    "spiral+multilane artifacts" bug. Also a tangent-room clamp fits R / trims the spiral / falls back to
    straight on degenerate (loop-needing) pairs.
  - **Port model evolved (2026-06-17, supersedes the `a.dir+180` flip above):** each leg now has TWO ports
    — an INBOUND (entry) and an OUTBOUND (exit) carriageway — and `port.dir` is the lane's *true* travel
    direction (drive-on-right). Entries sit on the inbound lane (arrow points *into* the junction), so the
    splines now leave A along `a.dir` directly — **no `+180`**. The old single-outbound-port-per-leg + flip
    put entries on the wrong carriageway (the ramp ran against traffic); 8 ports fix it. This is reflection
    #3 (the `Leg`/`Road` layer) surfacing early — see [`junction-lanelink.md`](junction-lanelink.md) §7.

## `junction` + `laneLink` (OpenDRIVE) ✅ DONE — reconciled 2026-06-17 → [`junction-lanelink.md`](junction-lanelink.md)
The decided next focus is **done as a design pass.** Read the OpenDRIVE §12 junction/laneLink spec
firsthand, reconciled it against the DSL (matches + gaps), and sketched the roadlab/roadnet2 C data
types — all in **[`junction-lanelink.md`](junction-lanelink.md)**. Headlines:
- **The standard validates our two-layer split:** OpenDRIVE puts *connectivity* in `junction` and *shape*
  in each ramp's own road `planView` — i.e. Layer 1 (topology DSL) vs Layer 2 (roadlab geometry M1–M5).
- **`laneLink` = our movement layer at lane grain**; a `laneLink` endpoint `(road, lane id)` **is** a
  roadlab port ("a port is a lane + its travel direction"). Adopt `connection`+`laneLink` near-verbatim.
- **One field we keep that the standard drops:** the ramp **primitive** (`loop`/`flyover`/…) — our
  *generative* input that produces the connecting-road geometry. OpenDRIVE is the lowered/baked form.
- **Ring family** → OpenDRIVE's `junctionGroup type=roundabout`, a peer construct — confirms it's parked
  correctly outside the ramp grammar.

**Adopted in code — roadlab M6 (2026-06-17).** The `Junction`/`Connection`/`LaneLink`/`RampPrim` schema is
now in `roadlab.c`: a table-driven `draw_junction()` strokes each connection with the M1–M5 splines, with a
`j` toggle between the single-ramp sandbox and the whole junction (a `DEMO` 4-way slip-turn table drives it).
This is the interchange-dsl "migration path" #4 made concrete. `prim` is wired but only `RP_DIRECT` has a
real drawer (LOOP/FLYOVER fall back to the spline).

**Hard-turn drawers (spec §8.1) DONE (2026-06-17).** `loop_spline()` — the long way (≈270°, cloverleaf):
LINE→ARC→LINE with the two line lengths solved (2×2) so a fixed-R loop lands on B. `scurve_spline()` — the
flyover: an equal-tangent **biarc** (semi-direct reverse curve) crossing the conflict on a raised deck. Both
wired into `draw_connection()` (`RP_LOOP`/`RP_FLYOVER`) + the sandbox (`l` cycles direct/loop/flyover);
`Connection` gained per-ramp `R`/`lift`. Also: ports now model **inbound + outbound carriageways** per leg
(entries on the inbound lane — see the M5 gotcha note above).

**▶ Next:** build the type→`Connection[]` *generator* (§8.2), then port into a world that emits `Junction[]`
deterministically from the seed. **Open reflections, the "retrofit roadnet2 vs grow a junction-first world"
fork, and the specs are in [`junction-lanelink.md`](junction-lanelink.md) §7 + §8.** **Full OpenDRIVE
roadmap — taken (M1–M6) vs left:** [`road-geometry-refs.md`](road-geometry-refs.md) → **"OpenDRIVE adoption
inventory"**.

Other roadlab continuations (not the chosen next, but queued):
- **Port `roadlab` into roadnet2** — bake the constants, call it as the junction drawer.
- **`ring`/`circulate` primitive** for the British roundabout family (out of the ramp grammar; see interchange-dsl).
- **Then** `roadlab` becomes the drawer that `interchange.c` / roadnet2 call (bake constants, port in).
- **Parked:** `interchange.c` task — the **half-diamond draw-order** (near ramps should merge at-grade,
  not duck under the highway); the **inner-loop nesting** in `interchange.c`; the **loop+loop vs
  loop+flyover** trumpet variant toggle.
- **Reference:** <https://www.roads.org.uk/interchanges> — clean colour-coded movement diagrams of every
  junction type. **British (drive-on-left) → mirror via `DRIVE`.** Shapes are identical, only handedness flips.

## Working method (what worked for the owner)
- **Annotated screenshots** drive the geometry work: bake a clean (isolated) screenshot, the owner draws
  the *ideal path* in one colour + **X**'s the broken spots. Far faster than prose for curve bugs. (Saved
  as the `annotated-screenshots-collab` memory.)
- **Isolation bakes:** in `interchange.c`, temporary `TEMP`-marked static defaults isolate one atom
  (`show_near/loop/fly`, hide panel) → bake → read `build/.bake/<cart>/screenshot.png`; revert all `TEMP`
  before committing (`grep -n TEMP` must be empty).

## Key commits this session
`interchange` loop+slider+inner-WIP · `interchange-dsl.md` · `rampkit` (ports + nest-solver) ·
`road-geometry-refs.md` · `roadlab` (arc-spline foundation). All by pathspec, lint-green.

## The doc map (everything cross-links from here)
- [`roadnet2-plan.md`](roadnet2-plan.md) — the parent plan; Part B = interchanges + the junction matrix.
- [`interchange-handoff.md`](interchange-handoff.md) — `interchange.c` cart state (the trumpet work).
- [`interchange-dsl.md`](interchange-dsl.md) — the DSL: topology layer + geometry relation-language/solver.
- [`road-geometry-refs.md`](road-geometry-refs.md) — the research: OpenDRIVE / clothoids / offsetting + recommendation.
- [`junction-lanelink.md`](junction-lanelink.md) — OpenDRIVE junction/laneLink ↔ our DSL: matches, gaps, the C data-type sketch.
- carts: `tools/carts/{interchange,rampkit,roadlab}.c`.
