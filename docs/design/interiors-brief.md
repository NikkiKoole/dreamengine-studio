# interiors — a fill language for building floor-plans (brief for the interiors agent, 2026-06-17)

> The indoor sibling of [`streetlevel-content.md`](streetlevel-content.md). lotfill fills the land
> *between and around* buildings; **interiors fills the space INSIDE one building's footprint** —
> believable house / shop / warehouse layouts. The design doc already names this cart as a known
> gap: *"A building's floor-plan is literally a bounded room+corridor atom filling the `footprint`
> rect."* This is that cart. Build it as a **workbench-of-atoms**, exactly like `lotfill.c`.

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
