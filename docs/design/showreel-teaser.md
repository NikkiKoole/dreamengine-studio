# showreel / teaser — the big game's trailer as a forcing function

STATUS: BRAINSTORM / planning (2026-06-22). No code. The point of this doc: deciding what a
teaser would *show* enumerates what we'd have to *build* — so the shot list doubles as a
prioritized gap list (systems and API). Pairs with [`transitions.md`](transitions.md) (how
shots are cut) and [`cart-clips.md`](cart-clips.md) (how shots are baked); the traffic shots ride [`traffic-ai.md`](traffic-ai.md).

## The game

A **lo-fi GTA1-but-better**: an infinite, deterministic, procedural world you drive around,
assembled from many small spec-locked sandbox carts rather than one monolith. The thesis
([`second-north-star.md`](second-north-star.md)): *author the honest rules, then let the
system judge you* — nothing faked. The original brief ([`roadnet-handoff.md`](roadnet-handoff.md)):
"GTA1-style + Cataclysm-DDA-flavoured driving/adventure — infinite, deterministic, good for
driving around a city and roaming."

**The loop (resolved 2026-06-22) — three equal pillars:**
1. **Drive** — build a car, roam the procedural world (sloop + the road tiers).
2. **On foot — explore, enter, talk, fight** — CDDA-flavoured: out of the car, *into buildings*
   (interiors — BSP floor-plans), *talking to NPCs* (branching conversation — `dialogue.c`
   proves it needs no engine; a general NPC conversation tool is in progress), and combat
   (flank is the enemy brain). Driving and on-foot are *equal*, not driving-with-cutscenes.
3. **Make & sell — the production subsystem.** You're a person who makes money from products
   you craft. The itch is *optimize-a-process-for-best-quality/most-money* (Schedule 1 mixing ×
   Dwarf Fortress maker-skill × a demand curve). Its organizing arc — **"first you do it
   yourself, then you manage"** (execution → optimization → management) — runs on ONE pure
   function with two front-ends (you, or a hired worker = "you at their skill, no focus bonus").
   *You can't manage a craft you've never done* (very CDDA/DF). Designed in
   [`sloop-production.md`](sloop-production.md) (the mix half, shipped as `thecut.c`) +
   [`sloop-handwork.md`](sloop-handwork.md) (the embodied gather→carry→work→delegate half).

## The pieces today (and how done)

| Cart | Tier | What it is | Status |
|---|---|---|---|
| **sloop** | street | Build a car from parts on a grid; how it drives is *computed* (mass/COM/inertia → accel/grip/drift). Collidable world (cones, houses, parked cars, chain reactions). BUILD↔DRIVE flip. | Rungs 1–2.7; drives, builds, collides |
| **streetlab** | at-grade | The grammar of ground-level streets — junctions, turn lanes, mini-roundabouts, sidewalks, crosswalks, typed cross-sections; 4 network patterns. Seam: `gen_network(pattern,seed)`. | **COMPLETE, spec-locked (60)** |
| **roadlab** | grade-sep | Interchange geometry — diamond/cloverleaf/loop/stack/trumpet/roundabout; arc-spline ramps, clothoids, elevation. Seam: `make_junction(legs,type)`. | **COMPLETE, spec-locked (25)** |
| **roadnet / roadnet2** | region | Infinite procedural roads over a deterministic heightmap; highways + towns; street-level lens with RCI zones + lots. Seams: `road_at(wx,wy)`, `building_at(wx,wy)`, `gnode[]`/`gedge[]`. | v1 rungs 1–3; **v2 vector rebuild not built** |
| **flank** | street | Top-down squad combat — comms, flanking, flow-field paths, cover, suppression, alertness, stealth. Explicitly the enemy brain for sloop's streets. | Building |
| **thecut** (+ handwork) | production | Make money from products you craft: build a recipe from an ordered reagent sequence, hand-stir for a live skill bonus, sell to the client whose taste fits. ONE pure fn `brew_eval()` → DIY *or* a hired worker (the manage seam). Mix half. | `thecut.c` **shipped** (prototype); embodied handwork half **designed, not built** |
| **lotfill** | content | The street-level CONTENT language + workbench — fills the partition (blocks/lots/rural) roadnet builds; each fill-rule a pure fn of (region,hash,zone) serving `draw()` AND collision from one path. Its WORLD tab is a driver composing the atoms. | Workbench shipped; world-compose pending |
| **interiors** | content | The indoor sibling of lotfill — fills INSIDE a footprint (house/shop/warehouse) via BSP room trees (bounded region ⇒ recursion legal). The on-foot interiors. | Workbench shipped; world-wire pending |
| **dialogue** (+ NPC tool) | npc | Branching conversation — `dialogue.c` (Lounge Larry) proves the technique: a node table + a position int + an affinity int, `print()`+`btnp()`, **no engine needed**. A general NPC conversation tool is in progress. | `dialogue.c` shipped; NPC tool **in progress** |
| **roadhouse** | audio | A radio station (modal psych-rock, procedural soloist). Not part of the world — but it's the **car radio** the teaser wants. | Shipped |

The architecture is **three tiers converging at a world level** that *doesn't exist yet*:
region (roadnet) → junctions (roadlab) + streets (streetlab) → street-level actors (sloop +
flank). The seams between them are designed and named; the composer is the frontier.

## The teaser — a shot list (= the gap list)

A ~30–60s trailer. Each shot: what it shows → what it draws on → can we record it **now**, or
what's the gap. Tagline candidate: *"Build a car. Drive an infinite city. Nothing is faked."*

| # | Shot | Draws on | Recordable now? / Gap |
|---|---|---|---|
| 1 | **Build a car** — bolt parts on the grid, COM crosshair + mass/top-speed readouts update live | sloop BUILD | ✅ **Now** (sloop) |
| 2 | **Drive off** — the rig pulls away, handling visibly from the build | sloop DRIVE | ✅ **Now** |
| 3 | **Drift** through a bend, grip breaking | sloop | ✅ **Now** |
| 4 | **Chaos** — chain-reaction collision through parked cars / cones | sloop collidable world | ✅ **Now** |
| 5 | **Road variety** — a cloverleaf, then a city grid with turn lanes | roadlab, streetlab | ⚠️ as *separate* carts now; one drivable view = integration gap |
| 6 | **Scale** — continental map zooms down to a single street | roadnet lens + `camera_ex` zoom | ⚠️ pieces exist; the smooth zoom in one cart = gap |
| 7 | **Get out & fight** — enemies flank and take cover as you roll up | flank | ⚠️ standalone now; in-world wiring = gap |
| 8 | **Enter a building** — step from street into a generated interior | interiors | ⚠️ workbench now; enter-from-world (a [transition](transitions.md)) = gap |
| 9 | **Talk to someone** — a branching NPC chat, their face reacting | dialogue / NPC tool | ⚠️ dialogue.c standalone now; in-world NPC tool in progress |
| 10 | **Craft & deal** — build a recipe at the bench, hand-stir, sell to a client | thecut | ✅ **Now** (standalone) |
| 11 | **The radio** — a station plays as you cruise | roadhouse + spatial audio | ⚠️ audio-while-driving not wired |
| 12 | **Title card** — name + tagline | a card cart / font-bake | ✅ **Now** |

The reveal this table makes: **most shots are recordable as standalone clips today** (the v0
stitched teaser); the ⚠️ rows need world-integration only for the continuous *v1* take. A teaser
is reachable in two horizons.

## Two horizons (this is the "what next" answer)

**v0 — the STITCHED teaser (reachable now, forces the clip pipeline).** Cut shots 1–4, 9 (and
rough versions of 5/7 from the standalone carts) into one reel. Requires no game integration —
it requires finishing the **clip/showreel pipeline** we already scoped:
1. Author good demo tracks per cart → `tools/clips/<cart>/NN-label.{script,beats,rec}`.
2. Bake them — `make-gif.js --all` (today only 1 of ~10 committed tracks is baked).
3. ~~**Audio in clips**~~ — **SHIPPED (2026-06-22):** webm/mp4 clips now carry sound (rendered
   in the same `play.js` pass, muxed as Opus/AAC). See [`cart-clips.md`](cart-clips.md) "Sound".
4. ~~**Stitch**~~ — **SHIPPED (2026-06-22):** `tools/compose-clips.js` glues baked clips with
   ffmpeg `xfade`/`acrossfade` from a committed `.reel` manifest → one reel (picture + sound
   dissolve together). Layer B in [`transitions.md`](transitions.md).

> **v0 teaser BUILT (2026-06-22): `tools/reels/teaser.reel` → `editor/public/reels/teaser.webm`**
> (27 s, with sound). Five shots across the pillars: **sloop** (drive) → **interiors** (explore)
> → **dialogue** (talk) → **thecut** (make) → **moog** (sound), with fade/wipe/circle-open/
> dissolve cuts. Tracks committed at `tools/clips/<cart>/01-*.{script}`. This is the *vision*
> cut, not a continuous game (see the honesty note below).
>
> **A second reel — `tools/reels/mixtape.reel` → `editor/public/reels/mixtape.webm`** (23 s,
> action + sound): flank → turfwar → beatcrypt → epiano → spatial. Notably **flank's combat
> shot is keyboard-only** — it boots straight into the fight, so walking in trips the squad's
> tactical AI (no mouse-aim needed; mouse is only for the player's own shooting).

v0 is honest if it's a *vision* teaser ("here's the world we're building") and dishonest if it
implies the shots are one continuous game — a call for the maker (below).

**v1 — the INTEGRATED teaser (needs Phase 2; one continuous in-engine take).** The real game:
a single world cart where you drive sloop through roadnet's streets, junctions render via
roadlab/streetlab, flank spawns as combat, and the map↔street flip is a [transition](transitions.md).
This is the actual frontier:
1. **roadnet2** vector-native rebuild (replaces v1's field+graph mess).
2. **The world composer** — one cart calling `road_at()` / `make_junction()` / `gen_network()`.
3. **sloop wired to `road_at()`** — drive the procedural surface (roadnet rung 4).
4. **flank wired** as street-level combat in that world.
5. **map↔street mode flip** — exactly an in-cart [transition](transitions.md) (the showcase
   payoff of that whole thread).
6. **Audio wiring** — engine/collision SFX + the radio reacting to driving state.
7. **Production in the world** — place `thecut`'s bench as a location; build the embodied
   handwork half ([`sloop-handwork.md`](sloop-handwork.md)): gather → carry → work-that-takes-
   time → machine-as-first-delegate. The DIY→manage arc is the late-game economy.
8. **Content in the world** — compose lotfill's atoms + interiors' floor-plans into the live
   streets (the workbench→driver step both carts already model) so blocks, lots, and interiors
   are populated, not empty.
9. **Enter & talk** — interiors entered from the street (an in-cart [transition](transitions.md)),
   and the in-progress NPC conversation tool wired to people in the world.

## What's actually missing — systems, not API

The primitive API is **largely already there** (verified against `studio.h`): `lerp`, `clamp`,
`angle_to`, `camera_ex` (zoom+angle — the map↔street shot rides this), `boxes_touch`/collision,
`save`/`load`, spatial audio (`listener`/`note_pos`/`hit_at`), `rnd`, `ease_*`. So the gap is
**not** missing draw/math primitives — it's:

- **The composition layer** (the world cart that wires the seams). The single biggest piece.
- **Street-level rendering of the procedural world at speed** — systems work, but note roadnet
  is *seam-true* (`road_at(wx,wy)` is a pure function of world coords), so it's "render the
  query," not "fit a giant map array." `MAP_W/H` (128/64) is not the world.
- **The clip/showreel pipeline finish** (bake + audio mux + `compose-clips`) — for v0.
- **Audio integration** into gameplay (engine note, collision hits, reactive radio).
- **Content & interiors composition** — lotfill (outdoor fill) and interiors (floor-plans) are
  *workbenches*; composing their atoms into the live world (indoor + outdoor) is the same
  composer gap, so the streets and buildings aren't empty.
- **The NPC conversation tool** (in progress) — `dialogue.c` proves no engine is needed; the
  general authoring/runtime + wiring NPCs into the world is the remaining work.

Genuinely-missing *API/engine* candidates (small, verify before building):
- **Analog gamepad steering** — only `BTN_A/B` map a pad today; a driving game wants an analog
  stick. (STATUS tracks gamepad.)
- **Events** (`broadcast`/`received`) — designed, listed open in STATUS; useful for
  spawns/triggers in the world. Not in `studio.h` yet.
- **`lengthdir_x/y`** (polar→cartesian) — `angle_to` exists, the inverse helper doesn't; minor.

## How the teaser gets made (ties the session's work together)

The teaser is the *reason* the clip/showreel pipeline matters, and it uses every piece we
touched: **demo tracks** (committed `.script`s) → **`make-gif.js`** bakes each shot → **audio
mux** gives them sound → **transitions** (LUMA/iris/wipe, and `compose-clips` for cross-shot
cuts) assemble the reel → it headlines the **curated showcase** ([ADR-0020](../decisions/0020-in-house-tool-curated-showcase.md)).

## Decisions & open questions

**Resolved (2026-06-22):**
- **The loop — three equal pillars:** drive, on-foot explore + fight (CDDA), and make-and-sell
  production (the DIY→manage arc). Combat and production *lead alongside* driving, not as
  garnish — so the teaser gives shots 7 (combat) and 8 (craft) real weight, not a token beat.
- **Build v0 first — yes.** A stitched vision-cut now, to de-risk the pipeline and test whether
  the *feel* sells before the integration spend (same native-first logic as attract mode).

**Still open (maker calls):**
- **Honest-now vs. vision teaser.** A v0 stitched cut of standalone carts reads as a *vision*
  reel — fine internally, but it must not imply one continuous game. Lean: v0 internal, v1 the
  real public teaser.
- **Format / length / home** — a single hero clip on the gallery front page? Length?

## Related

- [`big-game-backlog.md`](big-game-backlog.md) — the per-cart remaining-work list + the
  cross-cutting seams (the world composer, the on-foot↔car avatar, money, pressure, persistence).
- [`transitions.md`](transitions.md) — cutting shots (in-cart flips + the stitch layer).
- [`cart-clips.md`](cart-clips.md) — baking shots; the silent-audio gap.
- [`../decisions/0020-in-house-tool-curated-showcase.md`](../decisions/0020-in-house-tool-curated-showcase.md) — where the teaser lands.
- [`road-program-state.md`](road-program-state.md), [`roadnet2-plan.md`](roadnet2-plan.md),
  [`roadnet-handoff.md`](roadnet-handoff.md) — the road tiers + the integration frontier.
- [`sloop-production.md`](sloop-production.md), [`sloop-handwork.md`](sloop-handwork.md) — the
  make-and-sell subsystem (mix half shipped as `thecut.c`; embodied half designed).
- [`streetlevel-content.md`](streetlevel-content.md), [`interiors-brief.md`](interiors-brief.md)
  — the outdoor (lotfill) + indoor (interiors) content languages.
- [`second-north-star.md`](second-north-star.md) — the deep-sim-behind-lo-fi thesis.
