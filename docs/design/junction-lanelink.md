# junction + laneLink — the formal junction data model (OpenDRIVE ↔ our DSL, 2026-06-17)

> Part of the road-geometry effort — map & how-we-got-here: [`road-geometry-handoff.md`](road-geometry-handoff.md).
> Reconciles the standard against our DSL: [`interchange-dsl.md`](interchange-dsl.md) (Layer 1).
> The geometry it omits lives in: [`road-geometry-refs.md`](road-geometry-refs.md) + roadlab M1–M5.

This is the **decided next step** from the handoff: read OpenDRIVE's `junction`/`laneLink`, reconcile it
against our junction DSL (matches + gaps), and sketch the C data types roadlab/roadnet2 would use. It
costs ~no geometry code — it's a *schema* — and the point is to **validate the DSL against the standard**
and pin the representation roadnet2 serialises junctions into.

## The 30-second version
OpenDRIVE stores a junction as pure **connectivity** — *which road's lane feeds which road's lane* — and
puts the **shape** of every ramp in that ramp's own road geometry (`planView`). That is **exactly our
two-layer split**: Layer 1 (topology DSL) = `junction`/`connection`/`laneLink`; Layer 2 (the curves) =
roadlab's `LINE|ARC|CLOTHOID` ref-line + lanes + width + elevation (M1–M5). The standard confirms the
architecture. We adopt `connection`+`laneLink` close to verbatim, and keep **one field the standard
doesn't have** — the ramp *primitive* (`loop`/`flyover`/…), because that's our generative input that
*produces* the connecting-road geometry. OpenDRIVE is the lowered/baked form of what our DSL authors.

---

## 1. The OpenDRIVE model (firsthand, spec v1.8.1 §12)
Source: ASAM OpenDRIVE §12 Junctions —
<https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/specification/12_junctions/12_01_introduction.html>
(§12.4 Connecting roads for `connection`/`laneLink`).

- **`<junction>`** — `id`, `name`, `type` ∈ {`common`/default (an *area* where roads meet; the normal
  case), `direct` (no separate connecting-road geometry — the ramp road links straight on; for simple
  highway on/off slips), `virtual` (spans overlapping roads; routing-only)}. A junction **groups
  connecting roads**; a connecting road is just a normal `<road>` whose `<link>` names this junction.
- **`<connection>`** (child of junction) — one path through the junction:
  - `id` — unique within the junction.
  - `incomingRoad` — the road id entering.
  - `connectingRoad` — the road id that *carries* you across (its `planView` **is the ramp shape**).
  - `contactPoint` ∈ {`start`, `end`} — which end of the connecting road touches the incoming road.
  - **Rule:** at most one `<connection>` per `(incomingRoad, connectingRoad)` pair.
- **`<laneLink>`** (child of connection) — the per-lane map:
  - `from` — lane id on the incoming road; `to` — lane id on the connecting road.
  - (`overlapZone`, metres — only `direct` junctions; skip.)
  - **Rules:** specify `laneLink` *only for lanes leading into* the junction; an unlinked lane = **no
    traversable path**; **several `laneLink`s in one connection ⇒ a lane change is allowed** across the
    junction; several single-link connections ⇒ no lane change.
- **Lane numbering** (§11, the context laneLink ids live in): per road, lane `0` = centre; **positive =
  left** of the reference line, **negative = right**, magnitude increasing outward. The sign is relative
  to the *reference-line direction*, **not** to the driving side — driving side is a separate fact.

## 2. The mapping to our DSL

| our DSL ([interchange-dsl](interchange-dsl.md)) | OpenDRIVE | note |
|---|---|---|
| **junction** = legs × {movement → primitive} | `<junction>` | the whole object |
| **leg** = `(bearing, class, terminates?)` | an **incoming road** (one per direction meeting the hub) | a 2-way leg = two incoming roads |
| **movement** `O → D` | a **`<connection>`** (`incomingRoad`=O, `connectingRoad` exits onto D) | road-level |
| **ramp primitive** (`loop`/`flyover`/`diverge`/…) | *(not stored)* — implicit in the connecting road's `planView` | **the gap; we keep it** |
| **port** = `leg.direction.lane` `(point, tangent, width)` | a **`<laneLink>`** endpoint `(road, lane id)` + `contactPoint` | port = roadlab's lane anchor |
| **port A is the ENTRY** (tangent points in; the roadlab gotcha) | `contactPoint` (`start`/`end`) | orientation of the connecting road |
| **completeness** (full vs partial: parclo, half-diamond) | omit the unserved `<connection>`s/`<laneLink>`s | partial = fewer connections, not "broken full" |
| **ring/circulate** (parked; British roundabout family) | `<junctionGroup type="roundabout">` | a *peer* construct, not a connection (matches our finding) |

## 3. Matches — what the standard validates
1. **The two-layer split is real and standard.** OpenDRIVE separates *connectivity* (`junction`) from
   *shape* (each road's `planView`). That is our Layer 1 (topology) vs Layer 2 (geometry) exactly. The
   architecture we reasoned to is the one the standard uses. ✅
2. **`laneLink` IS our movement layer — at finer grain.** Our "movement = leg→leg pair" is road-level;
   `laneLink` records lane→lane. Our claim that **"a port is a lane + its travel direction"** (roadlab's
   design) is precisely a `laneLink` endpoint. roadlab already anchors ports to lanes, so `laneLink` is
   just the *serialisation* of what roadlab ports already are. ✅
3. **The connecting road = our ramp; its primitive is its geometry.** A `loop` vs a `flyover` differ
   only in the connecting road's `planView` (+ elevation `z(s)`, our M5) — confirming the primitive
   belongs in the geometry layer, not the connectivity schema. ✅
4. **Partial junctions fall out for free.** A half-diamond/parclo is simply fewer `<connection>`s — no
   special "partial" flag — matching "a partial is a real type, a deliberate subset." ✅
5. **The ring family is a separate construct.** OpenDRIVE expresses roundabouts with
   `<junctionGroup type="roundabout">` grouping the junctions around a circulatory road — corroborating
   our parked conclusion that `ring`/`circulate` is a **peer** of the ramp grammar, not an assignment in
   it. When we build it, copy `junctionGroup`. ✅

## 4. Gaps / mismatches — where we differ on purpose
1. **Primitive type isn't in the standard — and we keep ours.** OpenDRIVE has no "this is a loop" tag;
   the shape is implicit in geometry. But the primitive is our *generative input*: `serve { O→D: loop }`
   is what *produces* the connecting road's `planView`. So our model sits **one layer above** OpenDRIVE:
   `DSL(primitive)` → *generate* → connecting-road geometry → *serialise* → OpenDRIVE. We retain a `prim`
   field; OpenDRIVE is the baked form that drops it. (Don't try to recover the primitive from geometry.)
2. **Handedness: signed lane id vs `DRIVE`.** OpenDRIVE lane ids are signed by *reference-line side*, not
   driving side. We collapse handedness into the `DRIVE` constant. Reconcile by storing the **signed lane
   id** in the link (so it's standard-faithful and serialisable) and letting `DRIVE` decide which sign is
   the driving side at *generation* time. Don't double-encode handedness (the chirality bug that already
   bit us — see handoff "What failed").
3. **`direct` junctions** — an optimisation for trivial slip ramps (no separate connecting road). We
   always use a connecting road (`common`), so we can ignore `direct` until a perf/serialisation reason
   appears.
4. **`virtual` junctions** — routing across overlapping roads; irrelevant to a 2D top-down drawer. Skip.
5. **Grain of our "movement."** Our DSL movement is leg→leg; the standard is road→connectingRoad with
   lane-level `laneLink`. **Recommendation: roadlab adopts `connection`+`laneLink` as-is** (road-level
   connection, lane-level links) — strictly more expressive than leg→leg, and it's the structure roadnet2
   will serialise and route on anyway.

## 5. Sketch — the C data types (roadlab → roadnet2)
The **topology layer** (interchange-dsl Layer 1), baked to plain C tables in the `JUNCS[]` spirit. The
*shape* of each connecting road lives in its `planView` (roadlab M1–M5), **not here**.

```c
// ── Junction connectivity (OpenDRIVE junction/laneLink, baked to C) ──
typedef enum { JUNC_COMMON, JUNC_DIRECT } JunctionType;       // skip `virtual` (routing-only)
typedef enum { CONTACT_START, CONTACT_END } ContactPoint;      // which END of the connecting road
                                                               //   touches the incoming road

// OUR field, not in OpenDRIVE: the generative primitive that PRODUCES the connecting
// road's planView. OpenDRIVE is the lowered form that drops it. (interchange-dsl Layer 1.)
typedef enum { RP_THROUGH, RP_DIVERGE, RP_MERGE, RP_LOOP, RP_FLYOVER, RP_DIRECT } RampPrim;

typedef struct {
    int from;   // lane id on the incoming road   — signed (0=centre, ±outward); DRIVE picks driving sign
    int to;     // lane id on the connecting road  — same convention
} LaneLink;     // == a roadlab PORT pair (lane anchor + travel dir)

typedef struct {
    int          id;
    int          incomingRoad;     // road id entering
    int          connectingRoad;   // road id carrying the movement across — its planView IS the ramp
    ContactPoint contactPoint;     // = roadlab's "port A is the ENTRY" orientation
    RampPrim     prim;             // OUR generative field (see RampPrim)
    LaneLink     links[8];
    int          nLinks;           // 1 link = no lane change; >1 = lane change allowed across
} Connection;

typedef struct {
    int          id;
    JunctionType type;
    Connection   conns[32];
    int          nConns;           // a PARTIAL junction is simply fewer conns — no flag
} Junction;
```

**How roadlab's current `Port`/`ramp()` maps in:** today a roadlab port is `(lane + travel dir)` at a leg
and a ramp is `ramp(portA → portB, type)`. In this model that becomes one `Connection`:
`incomingRoad = portA.road`, `connectingRoad = <the ramp road>`, `contactPoint` from portA's end,
`prim = type`, and one `LaneLink` per lane carried (`from`/`to` = the two ports' signed lane ids).

**The two directions of travel through the model:**
- **Author / generate:** DSL `serve { O→D : loop }` → `Connection{ prim=RP_LOOP, … }` → roadlab generates
  the connecting road's `planView` (M1 arc-spline → M2 clothoid → M3 lanes → M4 width → M5 z) → render.
- **Store / route:** roadnet2 persists `Junction[]` as the junction representation; `LaneLink.from→to`
  gives lane-level routing (which lane reaches which) **for free** — the thing the standard exists for.

## 6. What this unlocks / next
- **roadlab** can grow a `Junction`/`Connection` table (this sketch) and drive its existing
  `ramp(portA→portB, type)` drawer from it — i.e. re-express ad-hoc ramps as `Connection`s. This is the
  "re-express `JUNCS[]` as declarations" migration step (interchange-dsl §"Migration path" #4) made
  concrete with a standard-aligned schema.
- **roadnet2** gains the serialisation/routing target: store `Junction[]`, route on `laneLink`.
- Then the queued items (port roadlab into roadnet2 as the junction drawer; `ring`/`circulate` →
  `junctionGroup`) have a data model to attach to.
- **OpenDRIVE adoption inventory** ([`road-geometry-refs.md`](road-geometry-refs.md)) — move
  `junction`+`laneLink` from "worth taking next" to **adopted** once the schema lands in roadlab.

Sources: ASAM OpenDRIVE Specification v1.8.1 §12 (Junctions), §11 (Lanes) —
<https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/latest/specification/12_junctions/12_01_introduction.html>,
§12.4 Connecting roads.
