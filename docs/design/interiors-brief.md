# interiors — a fill language for building floor-plans (brief for the interiors agent, 2026-06-17)

> The indoor sibling of [`streetlevel-content.md`](streetlevel-content.md). lotfill fills the land
> *between and around* buildings; **interiors fills the space INSIDE one building's footprint** —
> believable house / shop / warehouse layouts. The design doc already names this cart as a known
> gap: *"A building's floor-plan is literally a bounded room+corridor atom filling the `footprint`
> rect."* This is that cart. Build it as a **workbench-of-atoms**, exactly like `lotfill.c`.

> **Status (2026-06-22):** the workbench below is BUILT — `tools/carts/interiors.c` generates
> believable R/C/I floor plans (shell → BSP partition → circulation → emergent labels → furnish →
> program driver). This brief was then **extended with [Round 2 — the gameplay layers](#round-2--gameplay-layers-interiors-with-a-purpose-extends-this-brief-2026-06-22)**:
> making interiors *worth entering* — places you want to **be in, steal from, fight in, and OWN.**

> **Merged (2026-06-23):** `tools/carts/cityplan.c` STACKS this workbench onto lotfill's city —
> lotfill and interiors were always the same fill-language at two scales, so cityplan reads both by
> ZOOM: out → a city of ROOFS (lotfill's block→varied-lot subdivision, terraces, yards); in → each
> roof LIFTS to its floor-plan (this brief's BSP rooms); in further → the furniture. Pure level-of-
> detail: a roof lifts only once its building is big enough on screen to read, and the floor-plan is
> generated LAZILY (far buildings cost a roof rect). `F` force-lifts all roofs (dollhouse). The
> footprint scale is bumped to ~55–80px so a lot is room-capable. `lotfill`/`interiors` stay as the
> standalone workbenches; `cityplan` is the unified read.

## Your mission, in one sentence

Build `tools/carts/interiors.c` — a workbench cart that, given a building footprint rect + a
building type (**residential / commercial / industrial**), generates a **believable floor plan**:
exterior shell → rooms → circulation (doors/corridors) → fixtures (windows) → furnishing. Same
philosophy, same UX shell, same seams as lotfill — a new TAB per atom, free-fly explorer,
layer-peel, debug grid, point-probe.

## Read these first (in order)

1. **`tools/carts/lotfill.c`** — your template. Copy its *shell* almost verbatim: the `Atom` vtable
   registry (`atoms[]`), the free-fly camera (`camera_ex` + pan/zoom), `layer_on[]` peel, `dbg`
   grid, `ovl` overlay, the `*_probe()` HUD, the top/bottom bars. You are writing new `*_fill` /
   `*_draw` / `*_probe` functions and registering them as tabs — the harness is already designed.
2. **`docs/design/streetlevel-content.md`** — the parent thesis. Pay special attention to:
   - **"The one idea"**: bounded region + seeded fill-rule, pure fn of `(region, hash, type)` that
     answers `draw()` AND the collision/query call from the same code.
   - **"When something genuinely IS a new atom — the bounded-vs-open rule"** and the maze/dungeon
     table. **This is the key liberation for you:** a floor-plan is a **bounded** region, so unlike
     lotfill's open-world scatter, you MAY use growth / recursion / BSP / backtracking / random-walk
     freely — the region is self-contained, generated wholesale from one hash-seed, never spans a
     seam. The whole "lattice instances only, no grown sets" rule that constrains lotfill's scatter
     **does not bind you.** Use BSP.
   - **"Atoms vs biomes vs recipes"**: rooms are NOT enumerated types — they EMERGE from a selector.
3. **`docs/design/procgen-places.md`** → the R/C/I zone table (residential/commercial/industrial).
   That three-way is your top-level building-type selector input, same vocabulary as `footprint`'s
   `zone_at` (ZN_RES / ZN_COM / ZN_TOWN).

## The thesis you must hold: rooms EMERGE, they're not a type list

This is the same move `footprint` made for house/shop/tower, and you must repeat it for rooms.
**You never write a "kitchen" generator.** You write a selector that reads context and lets the
label fall out:

- a **bedroom** = a private room off the circulation, doesn't front the entry, medium, no plumbing
- a **kitchen** = a medium room adjacent to a living space / back wall, tagged plumbing
- a **bathroom** = the smallest room, plumbing, off a corridor
- a **living room** = the largest room, fronts the entry / the street-facing wall
- a **shop floor** (commercial) = the large room fronting the entry; **back-of-house / storage** =
  the rooms behind it
- a **warehouse bay** (industrial) = one big undivided volume + a small office box in a corner +
  loading doors on the access wall

So the room *program* is `f(building_type, room_area, adjacency, frontage, hash)` — a rule table,
not an enum of room kinds. Believability comes from the **rules** (private rooms cluster away from
the entry, wet rooms share a plumbing wall, the entry opens into a hall not a bedroom), not from
hand-placed set-pieces. This is the "prefer emergent behavior" house style — let layouts arise from
simple interacting rules.

## The atom decomposition (your tabs)

Each is a pure `fn(region/room, hash, type, show[]) → geometry` that also answers a query. Proposed
set — mirror lotfill's "keep the set tiny" discipline:

| atom (tab) | verb | fills | notes |
|---|---|---|---|
| **shell** | the building envelope | exterior walls + the entry gap on the frontage wall | the bounded region every other atom fills; `outward` = street side (reuse footprint's idea) |
| **partition** | recursively split shell → rooms | BSP / slice-and-dice into a room tree | **bounded ⇒ recursion is legal.** Stop on min-area + a hash roll. This is the heart. |
| **circulation** | connect rooms | doors through shared walls; a hall/corridor spine when rooms don't chain | the "room+corridor" atom. Must guarantee **every room reachable from the entry** (a theorem, like footprint's perimeter-fronting — no orphan rooms) |
| **fixtures** | stroke openings on walls | windows on exterior walls, doors on interior walls | the wall-edge decorate verb (lotfill's `border`/`ribbon` analogue, indoor) |
| **furnish** | scatter props per room | bed/table/counter/shelving keyed by the room's emergent label | a per-room `scatter` flavor — one furnish atom, many flavors by room type |
| **program** | the DRIVER (default tab) | composes shell→partition→circulation→fixtures→furnish over one building, type chosen by a selector | lotfill's `world` tab analogue — the integration view |

**The selector** (`room_label_at` / `program_for(type)`) is the Layer-2 grammar — inline C tables
like lotfill's `FLAVORS[]` + `flavor_for()`. Building type → room program; room geometry +
adjacency → emergent label → furnish flavor.

## Seams you MUST honor (non-negotiable, same as lotfill)

1. **Pure fn of the region's hash-seed.** One building = one seed → the whole plan, deterministic
   and byte-reproducible. Inside that bounded region, grow freely.
2. **Draw == query.** Write `solid_at(x,y)` / `room_at(x,y) → {room_id, label}` as the renderer's
   code run as a query — this is the eventual collision/interaction seam for sloop (a character
   walking the building, a camera entering it). Don't generate a separate collision structure.
3. **LOD gates drawing, never generating.** Same `zoom >=` draw-gates lotfill uses.
4. **Atoms read no cam/seed globals** — they take the rect + inspection flags. So the identical
   `*_fill` runs in the workbench (one room on stage) and in the eventual world (composed over every
   building). What you tune here is bit-for-bit what ships.

## Where this sits in the bigger picture (don't build these, just leave the seams)

- **Upstream:** `footprint` (in lotfill) produces the building **shell rect + zone**. Interiors is
  the natural downstream — eventually `footprint` hands you the rect and `outward` (frontage side),
  you fill inside. For the workbench, stub it: tile a grid of footprint rects with a per-cell
  type, exactly as lotfill stubs `world_kind_at` and the block grid. **Integration is a provider
  swap, not a rewrite** — keep that seam clean.
- **`building_at()` / `interior_at()`** is the collision seam the design doc has wanted since
  `roadnet-streetlevel.md` item 5. Your `solid_at`/`room_at` IS that, written query-first.
- **Multi-storey** is out of scope for the MVP — one floor, top-down. Note it as a refinement.

## Build plan (lens-first — same playbook as lotfill)

1. **shell + partition** — the BSP room-split of one footprint rect. Highest value, proves the
   bounded-recursion rule end-to-end. Get believable room *sizes/proportions* first (split ratio
   jitter, min-area, aspect guards so you don't get slivers).
2. **circulation** — doors + a corridor spine; prove **every room reachable from the entry**.
   This is what separates "a believable plan" from "a subdivided rectangle."
3. **the selector + emergent labels** — `room_label_at` reading area/adjacency/frontage. Probe
   should report the label so you can eyeball whether the emergence reads right.
4. **furnish (per-label scatter flavors)** + **fixtures (windows/doors)** — the dressing that makes
   it legible as a home/shop/warehouse.
5. **program driver tab** — compose all of it for R/C/I, layer-peel the phases.

Ship it like any cart: `tools/carts/interiors.c` (+ optional `.cart.js` — likely settings-only, you
draw everything with primitives like lotfill does), build + bake a screenshot thumbnail, register in
`editor/public/carts/index.json` with `"kind": ["tech-demo","generative"]`, run `lint-carts.js`. If
you author a good demo fly-through, park the input track in `tools/clips/interiors/`. Add a sibling
design doc (`docs/design/interiors-content.md`) once the atom set settles, and cross-link it from
`streetlevel-content.md` and `docs/README.md`.

## Round 2 — gameplay layers: interiors with a PURPOSE (extends this brief, 2026-06-22)

The cart above generates *believable* floor plans. Round 2 makes them *worth entering* — places
you want to **be in, steal from, fight in, and OWN.** Same philosophy as everything above: the
gameplay **emerges from the room labels** via selectors (never hand-placed set-pieces), stays
**seam-true** (a query mirroring `room_at`/`solid_at`), and is a **pure fn of (plan, hash)**. Three
new layers, each a tab + a query.

### LOOT — "steal from"
Emergent valuables keyed by room label — the reason to break in. `loot_at(x,y) → {kind, value,
secured}` + a LOOT overlay.
- shop → register (cash, low, open); office / back-of-house → **safe** (high, needs cracking, alarm);
  bedroom → cash + jewelry; store → contraband; bay → tools/parts (feeds sloop's scrap economy).
- A **selector, not a placement list** — e.g. *"the smallest secured back room holds the safe; the
  entry-fronting room holds the register."* Value scales with the building's ROLE (below): a
  target's safe ≫ a home's drawer.
- Hooks: grabbed loot → the money economy; `secured`/alarm → flank guards + a wanted response.

### COVER — "fight in"
The bones already exist (BSP walls = sightlines, door gaps = chokepoints). Two additions:
- **classify** each prop + wall as cover *height* — *waist* (desk/counter/sofa → shoot over / duck
  behind) vs *full* (walls). `cover_at(x,y) → height` is what flank's tactical AI reads.
- **a back entrance** — an emergent second door on a non-frontage wall, so a squad can *flank*
  instead of funneling the front. (Reuses the entry-gap machinery on another edge.)

### ROLE — "want to be in" AND "own"
A building **purpose** above R/C/I, layered on the footprint, driving loot density, occupants,
alarm, and acquisition. The selector reads zone + size + hash → a role:
- **shop** — buy/sell (the economy storefront; a clerk NPC → dialogue).
- **home** — an occupant lives here (their stuff to steal; a schedule; dialogue).
- **stash** — production + guards + contraband (a thecut bench as a room; high risk).
- **target** — a heist: high-value secured loot + alarm + a guard squad (flank).
- **for-sale → YOUR base** — see below.

### The player-owned base (the progression hook)
Some buildings are **acquirable**: a `for_sale` role with a price → on purchase it becomes **yours**,
and its rooms map to **base functions** (emergently, by label/area):
- a big room / bay → **garage** (store + swap vehicles — sloop).
- a secured back room → **stash** (store your loot; that safe is now yours).
- a bench spot → **production** (run thecut here; systems tick while you're away — the DIY→manage arc).
- a bed → **save / sleep / time-skip** (the save point + the pressure clock's rest).
- (later) wardrobe, planning board, trophy wall.

This is the CDDA/GTA base-building spine: buy a place → it's your home + storage + workshop + garage
→ upgrade it. **Interiors' only job is to generate the rooms and *tag which would make which base
function*** (the same emergent selector — "this bay would make a good garage"). Ownership state +
the function logic live in the game/composer, **not** the interiors atom — keep that seam clean.

### New atoms / queries
| atom (tab) | verb | query |
|---|---|---|
| **loot** | place valuables by label | `loot_at(x,y) → {kind,value,secured}` |
| **cover** | classify cover height (+ a back door) | `cover_at(x,y) → height` (waist/full) |
| **role** | building purpose + base-function tagging | `role_of(plan) → {role, price?}` · `base_fn_at(x,y) → fn` |

Same seams as the rest (pure-fn, draw==query, LOD-gated, no globals). A **GAME overlay** (one tab)
draws all three at once, so you can reseed and read, per building: *what's worth robbing, where the
cover is, what it'd cost to own and what base it'd make.* Build order: **loot first** (the reason to
enter, self-contained, visible, defines the economy hook) → **cover** (small: classify what's drawn
+ one back door) → **role + base** (the connective tissue, best once loot/cover give it something to
vary). These tie interiors into the economy, flank, thecut, dialogue, and save —
see [`big-game-backlog.md`](big-game-backlog.md).

## Repo gotchas (will bite you — they're in CLAUDE.md but worth restating)

- **Short PICO-8 draw names** in the C API: `rectfill` / `rect` / `circfill` / `line` / `pset` /
  `print` — NOT the long `sprite-draw.js` names.
- **British greys only:** `CLR_DARK_GREY` / `CLR_LIGHT_GREY` / `CLR_MEDIUM_GREY` / `CLR_DARKER_GREY`.
  `CLR_*_GRAY` does not exist (recurring compile error). There is **no `CLR_LAVENDER`** — the
  lavender colour is `CLR_INDIGO`.
- **Don't name a variable after a builtin.** `map`, `line`, `rect`, `circ`, `room` is fine but
  `print`/`spr`/`timer`/`now` are not. lotfill dodges this by calling its grid `Rect r`, not `rect`.
- **Commit by explicit pathspec, never branch** (multiple agents share `master`): `git add <files>`
  then `git commit -m "…" -- <the same files>`. List every file you touch (`.c` + `index.json` +
  `.cart.png`).
- Compile-check carts with `node tools/build-all.js` (or `clang -fsyntax-only -I runtime -DDE_TRACE
  tools/carts/interiors.c`); navigate with the LSP tool, not grep.
