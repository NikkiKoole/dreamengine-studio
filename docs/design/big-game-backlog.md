# big-game backlog — what's left, per cart + the seams that bind them

STATUS: LIVING BACKLOG / planning (2026-06-22). Companion to
[`showreel-teaser.md`](showreel-teaser.md) (the teaser is the *lens*; this is the *list*). Per-cart
deep detail lives in each cart's own design doc — this consolidates "what's left" in one place,
organized so you can see both the cart-level depth work and the **cross-cutting seams no single
cart owns**. Legend: **✓** = remaining item documented in the cart/its doc · **＋** = brainstormed
addition (not yet written down elsewhere). Status claims for the road carts follow the authority,
[`road-program-state.md`](road-program-state.md).

## Read this first — the seams (highest leverage)

The per-cart lists below are mostly *depth you add forever*. The things that turn five great
sandboxes into one **game** are the connective seams that **no cart owns today**. These are the
real "what to build next":

1. **The on-foot player + car↔foot seam.** sloop is the *car*; flank has *operatives*; nothing is
   the **person** who walks → enters a car → drives → gets out → enters a building → talks → fights.
   This avatar + the car↔foot handoff (a [transition](transitions.md)) is the *spine* of "equal
   driving and on-foot." It is the single most load-bearing missing piece, and it's nobody's cart.
   **◑ Rung F0 LANDED (2026-07-02, in sloop, fenced):** the avatar exists — the occupant of the
   SEAT CELL (one 7px rig tile) steps out at a stop (F), walks the same collidable world, and
   re-enters by walking back within reach of that seat cell. **◑ Rung F1 landed same day:** the
   surface drives the walk via the same `road_at()` seam as the rig's grip — tarmac + the
   `RoadHit.on_pave` pavement strip walk full pace, grass drags ~0.6×, identical over the stub
   grid and OSM Delft. Pinned by sloop's first `spec()` (16 assertions). Remaining rungs: F2
   enter a building (`interiors`), F3 lift flank's brain as a library header → *fight*. Ladder +
   design decisions:
   [`driving-world-program.md`](driving-world-program.md) → the player-fantasy scoreboard.
2. **The world composer.** One seed-driven cart that wires the sandboxes via their named seams —
   `road_at()` (roadnet2) · `make_junction(legs,type)` (roadlab) · `gen_network(pattern,seed)`
   (streetlab) · lotfill atoms · interiors floor-plans · flank spawns. Phase 2 in
   [`road-program-state.md`](road-program-state.md). The biggest build; everything else plugs in here.
3. **Money as connective tissue.** Driving, production, and combat-loot should all feed ONE economy.
   Nothing links them yet — money is what makes the three pillars one loop instead of three toys.
4. **A pressure clock.** Fuel / heat / time-of-day / NPC schedules. CDDA lives on pressure; right
   now nothing pushes the player. Several carts have a *half* (sloop fuel §6, thecut heat v3) — none
   are wired to a global clock.
5. **Persistence.** The world, your car, your production, what an NPC remembers — all need to
   survive a session. Designed in pieces (sloop chunk persistence §9a) but built nowhere.

These five are sequenced after the world composer exists (1, 3, 4, 5 all assume a world to live in),
but #1 (the avatar/seam) can prototype *now* against any single cart.

## Status at a glance

| Cart | Pillar | Sandbox status | Biggest thing left |
|---|---|---|---|
| **sloop** | drive | rungs 1–2.7 + collidable world ✓ + **drives real OSM Delft ✓** (2026-07-01) | NPC traffic + breakage; **`road_at()` over roadnet2** (the proc world — Rung B-proc) |
| **roadlab** | world | ✅ done, spec-locked (25) | port into the composer |
| **streetlab** | world | ✅ M1–M7 done, spec-locked | production-fidelity generator + integration |
| **roadnet2** | world | partial (highway tier) | the whole street-level build (the composer's core) |
| **flank** | fight | core AI shipped, "building" | wire combat into the real world; player squad |
| **thecut** | make | v1+v2 (bench + delegation) ✓ | v3 world integration + embodied handwork |
| **lotfill** | content | makers + ruins ✓ | provider-seam swap (needs the world) |
| **interiors** | content | workbench (BSP plans) ✓ | enter-from-world + interactivity inside |
| **dialogue / NPC** | social | dialogue.c proof ✓ | the NPC tool + schedules/quests/factions |

## Per-cart backlog

### sloop (drive) — [`sloop.md`](sloop.md)
Rungs 1–2.7 (drive/drift/build/drivetrain/gears) + §9 collidable world done.
- ✓ **NPC-driven traffic** — "sloop has no NPC-driven traffic; rung 4 = wire `road_at()`." *(needs the world)* — the *behaviour* (lane-keep, car-following, right-of-way) is being prototyped now as a TRAFFIC mode in trackgen: [`traffic-ai.md`](traffic-ai.md). Only the line it follows changes (cl[] there → road_at() here).
- ✓ **Breakage — §9d "alles kan kapot"**: tile-detach demolition + loose debris + persistence (phase 3, parked).
- ✓ Fuel & range (§6) · overheat derate (§4) · muscle engines (§2.8) · trailers/articulation (§7) · per-wheel contact model (§8) · noise/stealth (§6, "only matters once things hear you").
- ＋ **Visible damage model** — deformation / smoke / performance-loss, so breakage *reads* as GTA, not just disappearing tiles.
- ＋ **Police / chase response** — the GTA staple; bridges to flank + thecut's heat layer.
- ＋ **Scavenge → carry → bolt-on** loop — find broken parts in the world, repair your rig (ties drive ↔ production ↔ on-foot).

### roadlab (world) — [`road-program-state.md`](road-program-state.md), [`road-geometry-handoff.md`](road-geometry-handoff.md)
✅ **Done + spec-locked (25).** The full catalogue (diamond/cloverleaf/stack/trumpet/fork/triangle/
roundabout) generates from `(legs,type)` at any skew/lane-count.
- ✓ **Port into the world composer** — a world that EMITS `(legs,type)` per crossing and calls
  `make_junction()`/`draw_junction()`. The only real remaining work; it's a Phase-2 integration, not cart work.
- Note: roadnet2 is independently still finalizing some interchange *geometry* (cloverleaf/trumpet/stack "tangled") — see roadnet2 below.

### streetlab (world) — [`road-program-state.md`](road-program-state.md)
✅ **M1–M7 done + spec-locked (60).** Curb returns · skew/T · turn lanes + median · street web (grid/
organic/radial/cul-de-sac) · sidewalks + crosswalks · mini-roundabout · typed cross-section.
- ✓ **Production-fidelity generator** — today's network is *toy-scale* (54 nodes, jitter/spanning-tree),
  **not** the real Parish-Müller L-system / Chen tensor-field generators. The §8.4 two-tier generator
  (major arterials, then local streets per region, stitched at boundaries) is the headline gap.
- ✓ **Unbuilt Facet-A primitives** the research named: staggered junctions, free-right slip lanes,
  signalized-vs-priority control, pedestrian refuge islands.
- ✓ Per-pattern numeric metric table; superblock / fused-grid pattern (open research).
- ＋ **Traffic-signal *state*** (not just geometry) — phases/lights as gameplay (running a red, the chase).
- ＋ **Pedestrian crossing *logic*** — peds actually using the crosswalks (ties to the on-foot world).

### roadnet / roadnet2 (world) — [`roadnet2-plan.md`](roadnet2-plan.md), [`roadnet.md`](roadnet.md)
roadnet v1: rungs 1–3 (seam-true highway splines, terrain-aware). roadnet2: the vector-native rebuild,
**L0 core only** (curving highways between ranked cities; metre-locked; MAP↔DRIVE harness).
- ✓ **Unified `road_at` = nearest-edge** + a spatial index (the new piece vectorland needs).
- ✓ **Collector tier** (warped grid, deterministic spline edges) → **access + cul-de-sacs** (finest tier).
- ✓ **Buildings on edges** — `building_at()` collision boxes, footprints in metres.
- ✓ **Interchange geometry finalize** — cloverleaf/trumpet/stack/parclo still first-draft (tangled).
- ✓ **Top-down town placement** (parked) — settlement-relationship-driven, not "driveway onto a freeway."
- ✓ Polish: reference-grid faintness, road-density tuning, node-marker size, ~500 km scope to commit.
- ✓ **rung 4 — wire `road_at()` into sloop** (the moment the rig drives the real world). Tunnels/landmarks parked.
      **✅ CONSUMER SIDE DONE (2026-07-01):** sloop's `road_at()` + spatial index + render/grip/collision all shipped, proven over **OSM Delft** — so roadnet2 only has to *expose* its own `road_at()` (nearest-edge over its graph) and it swaps straight in behind the same seam ([`driving-world-program.md`](driving-world-program.md) Rung B-proc).

### flank (fight) — [`flank-tactical-ai.md`](flank-tactical-ai.md)
Core AI shipped (comms, flow-field, flanking, cover, alertness, weapon abstraction, scenarios, ammo).
- ✓ Sniper-telegraph + **grenades + cover destruction** · **morale / panic** · per-enemy skill rating ·
  lean / quick-peek · aggression/discipline trait · overwatch · more archetypes (heavy/grenadier/leader) ·
  coordinated room search · **player squad** (control one operative + AI teammates) · broader scenario palette.
- ＋ **Procgen arenas = real world geometry** — fights should use **lotfill blocks / interiors floor-plans**
  as cover, not bespoke maps. This is "more levels / procgen" done right: the world *is* the level.
- ＋ **Vehicles in combat** — drive-bys, roadblocks, using a car as cover. The sloop×flank bridge.
- ＋ **Wire into the world** — combat triggered in sloop's streets/interiors, loot feeds the economy.

### thecut (make) — [`sloop-production.md`](sloop-production.md), [`sloop-handwork.md`](sloop-handwork.md)
v1 (the bench, pure-fn mix) + v2 (delegation: workers, standing orders, morale, learning) **built**.
- ✓ **v3 — world integration** (not a cart): bench becomes a rentable room, selling = driving to clients,
  systems run while you drive elsewhere, **heat / risk / busts** (the crime layer).
- ✓ **Embodied handwork half** ([`sloop-handwork.md`](sloop-handwork.md)): gather → carry →
  work-that-takes-time → **machine as your first delegate** → grow/wait. The CDDA body-in-world loop.
- ＋ **Scrap-economy tie** — source reagents/parts from the world (links to sloop scavenge + on-foot).
- ＋ **Recipe discovery** — how players *learn* good combos (experiment, NPC tips, found notes).

### lotfill (content) — [`streetlevel-content.md`](streetlevel-content.md)
6 fill atoms + the ruins modifier pipeline, all pure-fn/seam-true/deterministic.
- ✓ **Makers (phase 4)**: `relief`-as-readable-slope · `massing` (density→height) · `square`/`quay` ·
  `blob`/`carve` partitions · `line`/`cliff`/`meander` flow verbs · `cluster` (bounded hamlet).
- ✓ **Modifiers (phase 5)**: `age` (weather/stain) · `dress` (finishing scatter).
- ✓ **Integration seams** (stubbed, need provider swap): `road_at`/`frontage`, `zone_at`, `world_kind_at`.
- ＋ **Interactive/breakable street furniture** — the lampposts, stalls, fences, glass sloop §9d crashes
  through. lotfill places them; sloop breaks them. The "alles kan kapot" world dressing.

### interiors (content) — [`interiors-brief.md`](interiors-brief.md), [`building-blocks-spec.md`](building-blocks-spec.md)
Workbench exists — BSP floor-plans (shell + partition), bounded-recursion (footprint ⇒ a room tree).
- ✓ Per the brief's build plan (verify which are done in-cart): **circulation** (doors + corridor spine,
  every room reachable) · **emergent labels** (`room_label_at` from area/adjacency/frontage) ·
  **furnish + fixtures** (per-label scatter + windows/doors) · **program driver tab** (R/C/I compose).
- ＋ **Enter-from-world** — step off the street into the interior (a [transition](transitions.md)); the
  on-foot pillar's payoff. *(seam #1 territory.)*
- ＋ **Interactivity inside** — loot, hiding spots, cover (feeds flank), NPCs at home/work.

### dialogue / NPC (social) — [`showreel-teaser.md`](showreel-teaser.md)
`dialogue.c` (Lounge Larry) proves branching conversation needs **no engine** (a node table + a position
int + an affinity int). It's a worked example, not a system — and the system is the gap.
- ✓ **The NPC conversation tool** (in progress) — a general authoring/runtime over the dialogue.c pattern.
- ＋ **NPC schedules / routines** — where people are, when (the world feeling alive; pressure-clock food).
- ＋ **Quest-givers + missions** — objectives, multi-step chains, consequences.
- ＋ **Shops / transactions** — buy/sell dialogue (the economy's storefront).
- ＋ **Reputation / factions** + **persistent dialogue state** (what you said before).

## The leverage read (where to actually start)

Per-cart depth is endless; the **seams** convert it into a game. Sequence:
1. **The world composer** (seam #2) — unblocks NPC traffic, breakage-in-world, combat-in-streets,
   production-in-world. Everything waits on this. roadnet2's street tiers + `road_at`→sloop is its core.
2. **The on-foot avatar + car↔foot seam** (seam #1) — can prototype *now*, independent of the composer,
   and it's the spine of the whole "equal driving/on-foot" pitch.
3. **Money** (seam #3) — the moment two pillars share a wallet, it's a loop, not a demo.
4. Then per-cart depth + pressure (#4) + persistence (#5) as the world fills in.

Your two examples (sloop NPC traffic + breakage) are real and documented — and both are downstream of
seam #2 (they need a world to drive). That's the backlog telling you the composer comes first.

## Related
- [`showreel-teaser.md`](showreel-teaser.md) — the teaser (the lens this backlog serves).
- [`road-program-state.md`](road-program-state.md) — the authoritative road-tier status.
- Per-cart docs: [`sloop.md`](sloop.md) · [`sloop-production.md`](sloop-production.md) ·
  [`sloop-handwork.md`](sloop-handwork.md) · [`flank-tactical-ai.md`](flank-tactical-ai.md) ·
  [`streetlevel-content.md`](streetlevel-content.md) · [`interiors-brief.md`](interiors-brief.md) ·
  [`roadnet2-plan.md`](roadnet2-plan.md).
- [`second-north-star.md`](second-north-star.md) — the deep-sim-behind-lo-fi thesis.
