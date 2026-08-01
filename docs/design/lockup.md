# LOCKUP — the prison as a machine for meeting needs on a schedule

status: BUILDING — a Prison Architect demake at full simulation depth. Design + module contract
written 2026-07-31; modules under parallel construction. Cart: `tools/carts/lockup.c`.
The authoring method (a frozen contract, then eight agents in parallel) is
[ADR-0034](../decisions/0034-contract-first-parallel-authoring.md).

> **Homage:** Prison Architect (Introversion, 2015). Lineage runs through this repo's own
> [`dwarffort`](../guides/cart-specs/dwarffort.md) (designation → A\* → job FSM) and `sims`
> (need-decay → urgency sort → seek object → satisfy).

---

## 1. The honest core

One cart, one honest core ([VISION](../VISION.md)). For LOCKUP the core is a single sentence:

> **A prison is a machine for meeting needs on a schedule — and the player never touches a
> prisoner, only the machine.**

Everything in the sim is downstream of that. A prisoner's need can *only* fall if:

1. a **room** exists that serves that need, and
2. it is **valid** (enclosed, and holding the objects its type demands), and
3. it is **reachable** from where the prisoner is, through doors they're allowed through, and
4. the **regime** currently grants an activity that sends them there, and
5. there is **capacity** — a bed, a bench, a shower head, a phone — not already taken.

Break *any one* of those five and the need climbs. That is the whole game, and it is why the
failure is always legible: a starving block is never "bad luck", it is a missing canteen, an
unfinished door, a timetable with no Eat slot, or forty prisoners and twelve benches.

**Danger is the integral of unmet need.** Not a random roll. A prisoner whose needs sit high grows
volatile; volatile prisoners near each other with no guard in sight start a fight; enough
simultaneous fights *is* a riot. So a riot is always the player's own arithmetic coming back — and
the post-mortem is readable in the same five conditions above.

### What this means for the build

- **No hidden dice on the important things.** Need decay, satisfaction rates, capacity, path length,
  guard response time are all deterministic given the seed. Randomness is texture (which prisoner
  arrives, cosmetic variation), never the load-bearing wall.
- **Every number on screen must be explainable by pointing at a tile.** If the UI says a cell block
  is at risk, the player must be able to find the reason on the map. This is the beginner-as-critic
  bar from [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md): legible to a stranger.
- **The player designates; the staff act.** Never a directly-driven character. Workmen build what
  you queue, cooks cook, guards patrol what you deploy. Player agency is entirely architectural
  and administrative.

---

## 2. Simulation model

### 2.1 The grid

A tile world, one tile ≈ one person-square, over a map larger than the viewport with a scrolling
camera. Each tile carries:

| field | meaning |
|---|---|
| `floor` | material (dirt / concrete / tile / grass / gravel / wood) — dirt = unimproved land |
| `wall` | none / brick / concrete / fence / perimeter — plus a **join mask** derived from neighbours |
| `door` | none / plain / jail / staff-only / gate, + open state + locked state |
| `object` | index into the object table (bed, toilet, sink, table, bench, …) or none |
| `room` | room id (0 = outdoors / unassigned) |
| `zone` | deployment sector (unrestricted / staff-only / secure) |
| `job` | pending construction/demolition designation + progress + material reservation |
| `var` | cosmetic variation seed (texture speckle, brick offset) — never gameplay |

**Walls join.** A wall tile draws from a 4-bit (plus corner bits) neighbour mask so corners, tees
and ends read as one continuous structure, not a grid of separate blocks. This is the single
biggest difference between "tile game" and "a building".

### 2.2 Rooms

A room is **discovered, never placed**. The player paints a room *intent* over floor; a flood fill
bounded by walls and doors then decides:

- **Enclosure** — does the fill escape to outdoors? If yes the room is invalid ("not enclosed").
- **Type requirements** — a Cell needs a bed and a toilet; a Canteen needs tables, benches and a
  serving table; a Kitchen needs a cooker, a fridge and a sink; a Shower needs shower heads;
  Solitary needs a toilet only; an Office needs a desk and a chair. Missing → invalid, and the UI
  says *which object is missing*.
- **Capacity** — derived from the objects present, not from area. Beds = prisoner slots. Benches =
  eating slots. Shower heads = washing slots. This is what makes the contention real.

Room validity is recomputed on any wall/door/object/floor change to the affected fill only, not
globally every frame.

### 2.3 Reachability

Two layers, because they answer different questions at very different cost:

1. **Connected-component labelling** over walkable tiles, rebuilt on structural change. Answers
   "can *anyone* get from A to B at all?" in O(1) — this is what makes "unreachable" an *instant*
   UI warning rather than a failed path 40 frames later.
2. **A\* with a binary heap** for the actual walk, plus **nearest-facility flow fields** (one
   multi-source BFS per facility type, cached and invalidated on change) so a hundred prisoners
   asking "where is the nearest free toilet" costs one field, not a hundred searches.

Doors are traversable-but-permissioned: a jail door is walkable for staff always, for prisoners only
when unlocked (i.e. not during lockdown). Permission is part of the path cost, so a locked-down
prison genuinely re-routes.

### 2.4 Prisoners

Needs, each a 0..1 float with its own decay rate and its own satisfier:

| need | satisfied by | starves into |
|---|---|---|
| Sleep | a bed, during a Sleep regime slot | exhaustion, then collapse |
| Food | a served canteen meal | hunger, then anger |
| Bladder | a toilet | soiling, which spikes Hygiene |
| Hygiene | a shower head | filth, misery |
| Recreation | yard, TV, pool table | boredom, volatility |
| Family | a phone during Free Time | despair |
| Comfort | a bench/chair/bed to sit on | irritation |
| Safety | guards nearby, no recent violence | fear, then pre-emptive violence |
| Privacy | a cell of one's own | resentment (shared/dormitory penalty) |

Each prisoner also has a **security category** (min / normal / max), a **temperament**, a
**grudge list**, and a **contraband inventory**. Their state machine: `IDLE → SEEK(target) →
WALK → USE(object) → …` with interrupts for `ESCORTED`, `FIGHTING`, `RESTRAINED`, `SOLITARY`,
`INJURED`, `RIOTING`, `ESCAPING`.

**Regime.** A 24-slot day timetable of activities (Sleep / Eat / Yard / Work / Free Time /
Shower / Lockup). At each slot boundary every prisoner re-picks a goal: the regime says *what they
are permitted to want*, their needs say *which permitted thing they want most*. A regime with no
Eat slot starves the prison no matter how good the kitchen is — and that's the lesson.

### 2.5 Staff

- **Guards** — patrol assigned sectors, escort arrivals to cells, unlock doors, break up fights
  (suppression rises near them), shake down cells for contraband, man solitary. Response time is a
  path length, so deployment is a real spatial problem.
- **Workmen** — consume the construction queue: haul material, build wall/floor/door/object,
  demolish. Materials cost money on delivery.
- **Cooks** — cook in a valid Kitchen, carry trays to Canteen serving tables. No cook, no meal,
  no matter how many tables.
- **Doctors** — treat the injured; without one, injuries become deaths.

### 2.6 Danger, incidents, riot

`volatility = f(unmet needs, grudges, recent violence, suppression)`. Two volatile prisoners
co-located with no guard in line of sight → **fight**. Fights raise fear across the room, which
raises volatility, which is the feedback loop that turns an incident into a **riot**: above a
threshold of simultaneous incidents the prison enters RIOT — doors get forced, guards fall back,
objects break. **Lockdown** is the player's lever: locks every jail door, collapses the regime to
Lockup, kills the need satisfaction that caused it in the first place. Buying calm with debt.

### 2.7 Economy

Daily balance: per-prisoner federal fee in, staff wages and utilities out. Intake grants pay for
capacity you've actually built. Construction bills on material delivery. A **prison grade** scores
safety / hygiene / feeding / recreation / reform, which gates intake. Going broke is a lose
condition; so is a riot you never regain control of.

---

## 3. Module contract

Prison-specific, so these live *with the cart*, not in the shared `runtime/` cart-land shelf —
LOCKUP is a consumer of the shelf (`lay.h`, `ui.h`, `physics.h` for debris, `harmony.h` for the
score's progression), not a new shelf entry. Each file has exactly one owner during parallel
construction, so no two agents ever touch the same bytes.

| file | owns |
|---|---|
| `lockup_model.h` | **the contract** — every type, enum, table and function declaration. Hand-written first, then frozen. |
| `lockup_grid.h` | tiles, materials, wall join masks, doors, objects, room flood-fill + typing + capacity, construction queue |
| `lockup_path.h` | heap A\*, component labelling, facility flow fields, door permissions |
| `lockup_actors.h` | needs, regime, prisoner + staff FSMs, contraband, fights, riot, escape |
| `lockup_econ.h` | money, intake, grants, wages, grading, reports |
| `lockup_art.h` | tile/wall/object/actor rendering, shadows, day-night light, weather, overlays |
| `lockup_score.h` | the adaptive score + diegetic audio |
| `lockup_hud.h` | toolbar, pickers, regime editor, inspector, panels, tooltips |
| `lockup.c` | the cart: state, `update()`, `draw()`, `spec()` |
| `lockup.cart.js` | every drawn sprite (via `tools/sprite-draw.js`) |

**Rule for every module:** it includes only `lockup_model.h` and the engine headers. Modules never
include each other — the contract is the only shared surface. The cart includes all of them, in
dependency order, exactly once.

---

## 4. The visual bar

The reference is Prison Architect's actual look, which is not "pixel art" — it's clean flat
top-down architecture with a warm, desaturated, institutional palette and a *lot* of readability
discipline. Translating that honestly to a 32-colour lo-fi console means:

- **Floors read as surfaces, not tiles** — per-material texture (concrete speckle, tile grout lines,
  grass tufts, gravel) with variation that doesn't visibly repeat at grid pitch.
- **Walls read as structure** — joined, with a consistent light direction and a hard shadow cast
  onto the floor beside them. A room should look *built*.
- **People read at a glance by role** — jumpsuit orange, guard navy, workman hi-vis, cook white —
  and by state: an escorted prisoner, a fighting pair, a sleeping body are each identifiable without
  reading a word.
- **Night is real light, not a blue tint over everything** — lit windows spill onto the floor, guard
  torches cone, the yard goes dark, the block glows.
- **The UI is chrome, not clutter** — the map is the game; panels come in on demand and get out.

The gate on all of this is an adversarial one: independent critic passes judge baked frames against
this written spec and against their own knowledge of the real game, and keep finding faults until
they can't. Noted honestly: **a literal blind side-by-side is not possible in this environment** —
the real game's frames aren't here, and a fabricated comparison would be worthless. The critique is
spec- and knowledge-based, and says so.

---

## 5. Gates

| change | gate |
|---|---|
| any | `node tools/build-all.js` (compiles), `node tools/lint-carts.js` |
| sim logic | `node tools/spec.js lockup` — the honest-core assertions of §1 |
| render | `node tools/canvas-diff.js lockup`, `node tools/ui-audit.js lockup` |
| music | `tune-check` / `level-check` / `fx-check` / `click-check`, `play.js --wav` |
| effects wiring | `node tools/lint-fx-frame.js` (set-and-hold, §"Key things to know") |
| phone playability | `node tools/mobile-lint.js lockup` |

See [checks-and-oracles.md](../guides/checks-and-oracles.md) for the reverse index.
