# galerijflat — an experimental/arty cart (design seed)

**Status: exploration, session 1. No cart exists yet.** This doc is the shared
understanding for a cart being designed across multiple sessions. Decisions so
far are marked ✓; everything else is open.

## The idea

A generative tableau of a **galerijflat** — the archetypal Dutch gallery-access
apartment slab of the wederopbouw / 1960s housing push. Not a game: an arty,
watchable cart in the spirit of `trafficjam` / `loveparade` — a procession of
ordinary life, except here the procession is *vertical and stacked* and mostly
stands still.

Decisions so far:

- ✓ **View: the gallery facade, straight on.** Flat elevation, no perspective.
  The whole screen is the building (plus a strip of sky and ground).
- ✓ **The archetype**, not a specific real building. Generic systeembouw slab:
  concrete bands, steel railings, repeated bays. Bijlmer-adjacent but anonymous.

## What a galerijflat is (the five-minute version)

A mid/high-rise slab (4–13 storeys) where every dwelling is reached via a
**galerij**: an open-air access gallery running the full length of the facade
on each floor. Lift/stair tower at one end (or the middle); you walk the windy
gallery past your neighbours' front doors and kitchen windows to your own door.
Counterpart of the *portiekflat* (stairwell access, no corridor).

Built en masse ±1955–1975 in prefab concrete (*systeembouw*) — Amsterdam
Nieuw-West, Utrecht Overvecht, Rotterdam-Zuid, the Bijlmermeer. Social arc:
"light, air, space" optimism → 80s/90s stigma → renovation/demolition →
present-day nostalgic re-appreciation. The cart should carry some of that
ordinary-life warmth, not the stigma.

## Anatomy of the gallery facade (what gets drawn)

The facade is a **grid of repeated bays** — one bay × one floor = one dwelling
= one cell. Reading top to bottom:

- **Roof**: flat, with a small lift machine-room box on top. Maybe an antenna
  forest / a gull.
- **Floor bands** (×N storeys), each band stacked from the same parts:
  - concrete gallery floor slab (a long horizontal line — *the* signature)
  - **railing** in front: thin steel bars or corrugated panels
  - per bay, behind the railing: **front door + small kitchen window**,
    repeating the whole width
  - incidental props per bay: bike against the railing, doormat, plant,
    drying rack, milk crate
- **Lift/stair tower** at one end: a closed vertical volume breaking the
  horizontal rhythm; lift indicator light that travels up/down; optionally a
  glazed stairwell with the zigzag visible.
- **Plinth** (ground floor): not dwellings — a row of **berging** doors
  (storage boxes), bike shed entrance, house numbers, maybe a strip of shops
  or open pilotis.
- **Ground strip**: pavement, a bit of grass ("kijkgroen"), lamppost, parked
  bikes.
- **Sky**: a generous Dutch sky above. (Day/night is an open question below.)

### The pixel core

A galerijflat facade **is already a pixel grid**. Every cell is one household.
The art is the *state* of each cell and how it changes slowly over time:

| cell event | visible as |
|---|---|
| someone home | kitchen window lit (warm yellow) |
| watching TV | window flickers blue, irregular |
| going to bed | light goes out |
| coming home | lift light climbs tower → figure walks gallery → door opens/closes → window lights |
| airing laundry | rack appears on the railing |
| nobody home | dark window, still door |
| curtains | window half-color, never changes |

A resident walking the gallery is the building's single biggest "alive" signal
— small figure, horizontal walk, one band at a time. Rare enough to be an event.

### Screen budget sketch (320×200, provisional — next session)

- ~10 storeys × ~14 px/band ≈ 140 px of facade
- plinth ~16 px + ground ~12 px + roof ~8 px → ~25 px left for sky
- bay width ~26–30 px → 10–12 bays (minus the tower) ≈ **100+ dwellings**
- per-bay parts at that size: door ≈ 6×10, kitchen window ≈ 8×6, railing bars
  1 px — all comfortably drawable with `rectfill`/`line`/`pset`, no sprites
  needed (matches the trafficjam "no sprite art" approach)

## Open questions (for the next sessions)

1. **Time**: a real slow day/night cycle (the facade as a clock — dawn greys,
   dusk patchwork of lights, deep-night near-dark)? Or fixed golden-hour/dusk?
   Dusk-into-night is where the window-patchwork shines.
2. **Simulation depth**: pure stochastic twinkle vs. tiny resident agents with
   schedules (leave for work, return, TV, bed). Agents give the lift/gallery
   walks meaning; trafficjam shows the per-entity-struct pattern.
3. **Interactivity**: pure ambient watch-piece, or light mouse play — hover a
   window to hear/see a hint of that household, click to ring a doorbell?
   (Mouse API is in: `mouse_x/y`, `mouse_pressed`; see orion for patterns.)
4. **Sound**: wind (the galerij is windy!), distant traffic, doors, lift ding,
   snippets of TV/radio from lit windows. The audio API has instruments +
   filters (see trafficjam's engine drone for the ambient-bed pattern).
5. **Generativity**: re-roll the building (storeys, bays, railing style,
   concrete tint, which households exist) on SPACE, like trafficjam re-rolls
   the fleet?
6. **Title**: working name `galerijflat`. Alternatives: `galerij`, `de flat`,
   `tien hoog` ("ten storeys up").

## References in-repo

- `tools/carts/trafficjam.c` — closest relative: generative rect-built
  entities, ambient sim, palette discipline, mouse honk
- `tools/carts/loveparade.c` — procession-of-life energy
- `docs/design/geometry-helpers.md` — the geometry-first drawing style this
  cart should follow
