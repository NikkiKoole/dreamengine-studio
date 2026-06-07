# Cart specs — Batch 2

Second wave of per-game cart prompts, built in parallel like the first batch. Same
process as [`cart-specs-index.md`](cart-specs-index.md): each row points at a `<spec>.md` design spec that
pairs under the reusable brief in [`cart-authoring-prompt.md`](cart-authoring-prompt.md).
Batch 1's table in `README.md` is left intact as the historic record of the first wave.

> Generated full-pipeline: one agent per game wrote both the spec `.md` *and* the cart
> (`tools/carts/<name>.c` + optional `.cart.js`), ran the parallel-safe CREATE step, and a
> coordinator baked thumbnails + merged `index.json` serially.

## Specs

Scope flags: 🟢 well-scoped · 🟡 sliced from a bigger game · 🔴 huge, cut hard.

| Spec | Game | Cart | Scope | Locked slice |
|---|---|---|---|---|
| `soccermgr.md` | Football Manager (FC-Mobile manager mode) | `soccermgr` | 🔴 | squad + drag-formation + transfer buy/sell vs budget + auto-sim scoreline & table |
| `infiniminer.md` | Infiniminer (proto-Minecraft, lo-fi) | `infiniminer` | 🟡 | first-person walk a small textured-cube voxel world; place/remove blocks under a crosshair |
| `jumpstar.md` | Jumpstar (endless vertical jumper) | `jumpstar` | 🟢 | auto-bounce up procedural platforms, steer L/R, grab stars, height score |
| `dwarffort.md` | Dwarf-Fortress-like (base builder) | `dwarffort` | 🔴 | top-down colony: designate dig/build jobs, dwarves auto-path (astar) to mine stone |
| `civ.md` | Civ 1 (4X strategy, tiny) | `civ` | 🔴 | fog-of-war tile map, found cities, move units per turn, one tech bar, end-turn |
| `strippoker.md` | Strip Poker (PG-13) | `strippoker` | 🟡 | 5-card draw vs CPU, bet/call/fold, winning advances a cheeky PG-13 reveal meter |
| `zak.md` | Zak McKracken (SCUMM adventure) | `zak` | 🟡 | 2–3 walkable scenes, verb bar + hotspots + inventory, one item-combo puzzle |
| `wintergames.md` | Winter Games (Epyx) | `wintergames` | 🟢 | shared event-shell + scoreboard: ski-jump (lean/timing) + speed-skating (rhythm) |
| `summergames.md` | Summer Games (Epyx) | `summergames` | 🟢 | reuses the event shell: 100m sprint (mash) + long jump (mash + angle) |
| `calgames.md` | California Games (Epyx) | `calgames` | 🟢 | reuses the event shell: half-pipe skate (pump + trick buttons) for air score |
| `transport.md` | Transport Tycoon Deluxe | `transport` | 🔴 | place 2 stations, lay a track path, a train auto-hauls cargo town→town for cash |
| `incmachine.md` | The Incredible Machine | `incmachine` | 🟡 | gravity sandbox: drag parts (ball/ramp/fan/seesaw) from a tray, RUN, ball→bucket; 2 levels |
| `lemmings.md` | Lemmings | `lemmings` | 🟡 | one level: assign jobs (dig/build/block/bash) to a walker stream; save N to exit; pixel terrain |
| `larry.md` | Leisure Suit Larry (PG-13 adventure) | `larry` | 🟡 | 2 lounge scenes, verb/inventory + dialogue, cheeky PG-13 innuendo; reuses `zak` engine |
| `monkey.md` | Monkey Island | `monkey` | 🟡 | SCUMM scene + verb/inventory + dialogue tree + insult-sword-fight (reuses `insult`) |
| `worms.md` | Worms | `worms` | 🟢 | 2-player turn artillery, walk + aim/power + wind, destructible pixel terrain (`pset` craters) |

**Reuse clusters** (build within a cluster so later carts can lean on earlier ones):
SCUMM adventure (`zak` → `larry`, `monkey`); Epyx sports (`wintergames` → `summergames`,
`calgames` — build the event shell in `wintergames` first); pseudo-3D / voxel
(`infiniminer` on `solid3d`/`cube3d`/`textured3d`); big sims 🔴 (`soccermgr`, `dwarffort`,
`civ`, `transport`); physics (`incmachine` on `pinball`/`sand`, `worms` on `gorillas`).
