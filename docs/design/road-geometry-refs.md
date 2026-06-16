# road geometry — references & recommendation (research, 2026-06-16)

> Part of the road-geometry effort — map & how-we-got-here: [`road-geometry-handoff.md`](road-geometry-handoff.md).
> What this research feeds: the geometry layer of [`interchange-dsl.md`](interchange-dsl.md).

What curve representation should the interchange/roadnet ramps use, to get the two things we actually
want: **(a) lines you can "drive easily"** and **(b) ramps that nest "snugly"** at a fixed clearance?
Researched OpenDRIVE, clothoids, the open sims (SUMO/CommonRoad), Cities: Skylines, and curve
offsetting. Feeds the geometry layer of [`interchange-dsl.md`](interchange-dsl.md).

## TL;DR recommendation
**Adopt an arc-spline model: a ramp = an ordered list of segments tagged `LINE | ARC | CLOTHOID`**
(OpenDRIVE's reference-line model, minus the XML). Not free cubic Béziers.
- **Snug nesting wants ARCS** — an arc offsets to an arc (radius ± clearance), and **concentric arcs
  at radial spacing = clearance never overlap, for free.** This is the deciding factor, and it's
  exactly why `rampkit`'s arc-based nest solver converges cleanly. Bézier offsets are *degree-10*
  algebraic curves (not Béziers) → sample-and-refit + cusp-fighting. Avoid.
- **"Easily driven" wants CLOTHOIDS** — curvature linear in arc length ⇒ constant steering rate (G2,
  no curvature jump). Splice a clothoid between line↔arc with a ~15-line forward-integration loop
  (below). Optional/additive — ship arcs+lines first, add clothoid joints later without changing the model.
- We can defer all this: **`rampkit` already uses arc loops and the nest solver works.** This doc is
  the "what to build the geometry layer on" decision when we generalise.

---

## 1. OpenDRIVE = the standardized road-geometry DSL (the model to copy)
Each road has ONE **reference line** (`<planView>` = ordered `<geometry>` elements by ascending `s`);
lanes/elevation are defined *relative* to it. Each `<geometry>` stores `s, x, y, hdg, length` + **one**
curve child:
- **line** — straight.
- **arc** — `@curvature` (1/R), **constant** curvature (+ = left/CCW).
- **spiral** — a **clothoid**: `@curvStart`→`@curvEnd`, curvature **linear** along length. Its job:
  transition line (κ=0) ↔ arc (κ=1/R) with no curvature leap.
- **poly3** (deprecated — single-valued, can't double back) / **paramPoly3** (current: two cubics of a
  shared param p; can loop).

**s/t coordinates:** `s` along the reference tangent, `t` lateral (⊥). **Lanes = "same s, shifted in
t"**, not metric parallel curves: `<laneOffset>` (cubic in s) + per-lane `<width>` (cubic). This s/t
shift is the trick that sidesteps true curve-offsetting (see §5).
Sources: ASAM OpenDRIVE spec §9 (geometries), §8 (coord systems), §11 (lanes) —
<https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/>

### OpenDRIVE adoption inventory — what we've taken, what's left (the "goodies")
OpenDRIVE is the gold mine: the standardized data model for everything a road network needs. The whole
roadlab arc IS a progressive port of it. Tracked here so we know what's mined and what's still in the seam.

**Adopted (in `roadlab`):**
| feature | what it is | where |
|---|---|---|
| reference line `LINE\|ARC\|CLOTHOID` | the `<planView>` geometry list (minus XML) | M1 + M2 |
| lanes = s/t lateral shift | "same s, shifted in t" — offset the one ref-line, don't re-curve | M3 |
| lane `width(s)` cubic | per-lane width along s → add/drop tapers, gores | M4 |
| elevation `z(s)` | height profile along s → grade separation / flyovers | M5 |

**Worth taking next (ranked):**
- **`junction` + `laneLink`** — OpenDRIVE's junction element groups connecting roads and records, per
  connection, *which incoming lane maps to which outgoing lane*. This is **literally our junction DSL's
  movement layer** (`legs × {movement → primitive}` in [`interchange-dsl.md`](interchange-dsl.md)) — the
  formal schema roadnet2 would serialise into. Reading/aligning to it costs no code and validates the DSL.
- **`roadMark`** — per-boundary lane markings: type (solid / broken / solid-solid / solid-broken), colour
  (white vs yellow), width, lane-change rule. We hardcode white dividers + a yellow median; this is the
  principled version (double-yellow = no crossing). Cheap, big believability gain.
- **lane *types*** — every lane tagged `driving / shoulder / median / sidewalk / biking / parking / curb`.
  Slots into our per-lane loop; lets us draw shoulders/medians/sidewalks differently.
- **`junctionGroup` (type `roundabout`)** — groups the junctions around a roundabout into one logical
  interchange. **This is OpenDRIVE's answer to the ring/roundabout family our ramp grammar can't express**
  (see interchange-dsl). The model to copy when we build the `ring`/`circulate` primitive.
- **lane predecessor/successor links** — lane continuity across road segments (routing). Matters once
  roadnet2 needs through-routing, not before.

**Flavour / later:** `objects` & `signals` (traffic lights, signs, poles, barriers, crosswalks, trees —
world dressing); road `type`/speed limits (only if we simulate driving).

**Skip for a 2D top-down fantasy console:** superelevation / crossfall / lateral `shape` (banking — invisible
top-down), CRG surface (friction/bumps), georeference/datum/projection, rail switches.

## 2. Clothoids — why, and the implementable recipe
Line κ=0 butted to arc κ=1/R = a **step** in curvature → lateral accel (v²κ) jumps → jerk. That join
is G1 (heading continuous) but not G2 (curvature continuous). A **clothoid** has κ **linear in arc
length** → smooth steering ramp (G2). Standard rail/highway transition curve. (Fresnel integrals;
κ(s)=κ₀+σ·s, θ(s)=θ₀+κ₀s+½σs².)

**Tiny-footprint recipe — forward integration (THE takeaway). No Fresnel, no solver, ~15 lines:**
```c
// line(k=0) -> arc(k=1/R) over length L in N steps:  k0=0; sigma=(1.0/R)/L;
void clothoid(double x,double y,double theta,double k0,double sigma,double L,int N){
    double ds=L/N, k=k0; emit(x,y);
    for(int i=0;i<N;i++){
        double tmid = theta + k*ds*0.5;        // midpoint variant = low drift, free
        x += cos(tmid)*ds;  y += sin(tmid)*ds;
        theta += (k + sigma*ds*0.5)*ds;
        k     += sigma*ds;                      // curvature linear in s
        emit(x,y);
    }
}
```
Use this when we *specify* the curvature program (laid-out ramps — the common case). Only if we need
to connect arbitrary endpoint+heading pairs we *don't* control, use the **Bertolazzi–Frego G1 fit**
(reduces to the zero of ONE scalar fn, Newton; lib `ebertolazzi/G1fitting`). Skip Fresnel entirely
for a tiny program.
Sources: en.wikipedia.org/wiki/Euler_spiral, /Track_transition_curve · arXiv 1305.6644, 1209.0910 ·
github.com/ebertolazzi/Clothoids · /G1fitting

## 3. SUMO & CommonRoad — both store POLYLINES
- **SUMO:** edge/lane `shape` = polyline (x,y pts); OpenDRIVE arcs/spirals are **sampled to segments**
  on import (`--opendrive.curve-resolution`). No native splines.
- **CommonRoad / lanelets:** left-bound + right-bound polylines, center = midpoints. (OSM/Lanelet2 too.)
Takeaway: the sim world stores **polylines** and treats analytic curves as import convenience — so
sampling-to-polyline (what our `ribbon`/`gen_loop` already do) is the universal LCD for rendering.
Sources: sumo.dlr.de/docs/Networks/Import/OpenDRIVE.html · commonroad lanelet2 docs

## 4. Cities: Skylines — cubic Béziers between nodes (= our current approach)
Network of **nodes + segments**; each segment = a **cubic Bézier** (`Bezier3`, ColossalFramework.Math).
Persists **two node positions + two end directions**; the interior control points are **derived** by
`CalculateMiddlePoints` (project end directions along the chord — the standard tangent-handle move).
Long/curved drags subdivide into more Bézier segments; ~96 m auto-node spacing; snapping/angle-snap.
**So a shipped, beloved city builder uses exactly our current Bézier-between-ports approach** and
handles complexity by subdividing. We're not behind — arcs are an upgrade *for nesting*, not a fix.
Sources: cslmodding.info/asset/network · CS mapper/exporter mods (lxteo, PropaneDragon) on GitHub ·
skylines.paradoxwikis.com/Modding_API

## 5. Offsetting & nesting (why arcs win)
- **Exact offset of a cubic Bézier is NOT a Bézier** — it's a degree-(4n−2) = **degree-10** algebraic
  curve (the unit normal carries a √ of a polynomial). Practical Bézier offset = approximate:
  Tiller–Hanson, or **sample→push along normal→least-squares refit**, or split into arcs/clothoids
  (Levien — handles the cusps where source curvature = 1/d, exactly where naive offsets self-intersect).
- **Arc offsets to an arc** (same centre, r±d) — exact, one line. Lines too. **Clothoids don't offset
  to clothoids** → road tools use the **s/t shift** (§1) instead of true perpendicular offset.
- **Nesting:** **concentric arcs at radial spacing = clearance never intersect** — snug, collision-free
  by construction. (General shapes need Minkowski/no-fit-polygon; ramps get the arc property for free.)
Sources: en.wikipedia.org/wiki/Parallel_curve · pomax.github.io/bezierinfo · raphlinus.github.io
(parallel-beziers / parallel-curves) · Tiller–Hanson 1984

---

## How this lands for us
- **`rampkit` is already on the right path** — its loops are **arcs**, and the nest solver converged to
  the exact target gap *because* arcs nest cleanly. The research says: keep going with arcs.
- **Geometry layer (interchange-dsl Layer 2)** → build it as the `LINE|ARC|CLOTHOID` segment list.
  Arc + line = trivial closed-form; clothoid joint = the §2 loop, added only when steering-feel matters.
- **Snug parallel lanes/edges** → use OpenDRIVE's **s/t shift**, not Bézier offsetting.
- **Open caveat (from the research):** the sub-agents searched without WebFetch, so exact pull-quotes
  (Heald's Fresnel coefficients, the verbatim `Bezier3.Position` body, the literal §11.6 width
  polynomial) are corroborated across multiple primary sources rather than fetched word-for-word; the
  structural facts above are each multiply-sourced.
