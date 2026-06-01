# The game you're building: DWARFFORT (lite)

## Premise
A pocket-sized **Dwarf Fortress** — a top-down colony base-builder on **one screen**.
You don't move dwarves directly; you **designate work** and they get on with it. Drag
the mouse over solid rock to mark it for **digging**, or over open floor to queue a
**wall**. A handful of stubby dwarves wake up, **A\*-path** to the nearest pending job,
swing a pick (mining yields **stone**), or haul a block into place to raise a wall. A
**day counter** ticks the whole time. It should feel busy and alive — picks tapping,
dust puffing, little guys threading the corridors you carve. No combat, no food, no
moods, no z-levels, no tech tree: just the pure designate → path → work loop.

## The core loop
1. **Designate.** Number keys pick the job type — **1 = DIG** (mine out rock),
   **2 = WALL** (build a block on open floor). Left-drag paints designations over a
   region; right-drag erases them.
2. **Dwarves work.** Each idle dwarf scans for the **nearest reachable pending job**,
   paths to a tile adjacent to it (A\* over walkable floor), and works:
   - **DIG**: chips the rock down (a `hard` timer); when it breaks, the tile becomes
     floor and the colony **stone** counter goes up. Dust puffs, pick taps.
   - **WALL**: needs **1 stone** from the stockpile; the dwarf carries a block over and
     raises the wall (rock again, blocking pathing).
3. **The day ticks.** A clock advances; each new **day** a short banner flashes and the
   tempo of the ambient bed nudges. Reaching a target day count = a calm "the fortress
   endures" win screen; you can keep playing or restart.
4. **Pause** with space to plan your designations without the clock running.

## Engine features it leans into (and why)
- **A\* pathfinding** — the heart of it. Same weighted-grid A\* idiom as `astar.c` /
  `dungeonkeeper.c`: each dwarf finds the shortest floor path to a tile beside its job,
  re-paths on a cooldown so newly-dug corridors open up routing.
- **Mouse drag designation** — `mouse_pressed`/`mouse_down`/`mouse_released` paint
  dig/wall marks across a dragged region; right-button erases. The drag idiom is lifted
  straight from `dungeonkeeper.c`.
- **A real tile grid** — solid rock / open floor / queued-dig / queued-wall, each drawn
  as chunky primitives with a cosmetic speckle so the stone reads as stone. Fog isn't
  needed for the slice; the whole fort is visible.
- **`dt()` movement** — dwarves walk and dig in real seconds, framerate-independent.
- **Sound where it belongs** — a custom noise instrument for the **pickaxe tap** (fires
  on a euclidean rhythm only while someone is actually mining), a soft chime when a
  stone is mined, a wood-block thunk when a wall goes up, and a slow low **cavern bed**
  on the beat. A new-day bell.
- **Juice at earned moments** — dust `boom` puffs when rock breaks, a tiny `shake` on a
  finished wall, the stone counter **lerps** to its new value, a day-banner that eases
  in. `fillp` checker overlays mark designations like DF's yellow stripes.

## Data model (suggested)
- `static Tile grid[GW*GH]` — `type` (ROCK / FLOOR), `job` (NONE / DIG / WALL), `hard`
  (dig progress), `var` (speckle). NB: **`grid`, never `map`** (clash with `map()`).
- `static Dwarf dw[N]` — `x,y` floats, `state` (IDLE / GOING / WORKING), `job` cell,
  `path[]/plen/pi`, `repath`, `carry` (a block for a wall), `face`, `hue` for recolor.
- globals: `stone`, `shown_stone` (lerped), `day`, `day_t`, `paused`, `tool` (DIG/WALL),
  drag state, `over`.
- A\* scratch arrays sized to the grid (`g_g`, `g_came`, `g_open`, `g_closed`).

## Controls
**Mouse-primary.** Left-drag over the fort to designate the current tool; right-drag to
erase a designation. **1** = DIG tool, **2** = WALL tool. **Space** = pause/unpause.
Click (or space) on the end screen to start a fresh fort.

## Lean into / read
`dungeonkeeper.c` (dig/claim/job + drag + A\* + the work-state machine — the closest
cousin), `astar.c` / `pathfinding.c` (the routing core), `sims.c` and `towerdefense.c`
(many little autonomous agents on a grid). Skip everything the slice cuts: no fighting,
no needs, no z-levels, no research.
