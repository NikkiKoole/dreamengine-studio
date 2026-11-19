# backroads — a 1970s cockpit-view drive through an endless, gritty countryside

STATUS: BUILDING (2026-11-19) — rungs 1 (cockpit over terrain) + 2 (roads + ground feel) built, awaiting the maker's play-test. Working title only — rename freely.

> **World (2026-11-19):** the endless worldnet countryside proved dull to explore; the plan is now a
> finite, grown island, see [`island-studio.md`](island-studio.md).
>
> **Genre: design doc + build brief.** This file is both the design record for a new cart and the
> brief for the agent (Claude Code) that builds it. It belongs at `docs/design/backroads.md`, linked
> from `docs/README.md`'s layout tree and from the driving-world program map
> ([`driving-world-program.md`](driving-world-program.md)) as a new consumer.
>
> The design came out of a brainstorm with the maker. Decisions marked **DECIDED** are settled;
> everything under "Open questions" is deliberately left to be answered by *playing* the thing.

---

## 0. For the agent: read this first

Before writing any code:

1. Follow **`CLAUDE.md`** exactly — especially the git rules (never branch, commit by explicit
   pathspec), the hot-shared-file rules for `runtime/studio.h` / `runtime/sound.h`, and "where things
   live" (designs in `docs/design/`, decisions as ADRs, status in `docs/STATUS.md`).
2. Read **`runtime/studio.h`** — it is the API truth. Never invent a signature.
3. Read **`docs/guides/cart-authoring.md`** and **`docs/guides/debug-harness.md`** (you'll verify
   every rung headless with `--det --frames N --dump DIR`).
4. Read the cousins this cart is assembled from (section 2). Skim them for idioms; don't copy blindly.
5. Read **ADR-0006** (library carts, not engine), **ADR-0009** (small 3D leaf helpers),
   **ADR-0021** (geometry in 2D, render is an adapter) and **ADR-0036** + `depth-tested-triangles.md`.
6. Then write a short plan for **rung 1 only** (section 9), show it to the maker, and wait for a go.

Working agreements:

- **Build one rung at a time and stop.** After each rung, dump a filmstrip, report what you saw, and
  let the maker play it before starting the next. The whole point of building now is that the design
  questions get answered by playing.
- **Don't edit shared headers (`worldnet.h`, `studio.h`, `sound.h`, `roadkit.h`) without asking.**
  If the cart needs something from them, propose it first. New engine API follows the "four places"
  rule in `CLAUDE.md`.
- **Cart-land first.** Anything reusable (the cockpit terrain renderer, say) starts inside the cart
  and is only extracted into a header when a second consumer appears (the roadkit trigger discipline).
- **Keep this doc alive.** Record what shipped, what changed and what you learned in section 10 —
  in the repo, not in per-machine memory.

---

## 1. The pitch

You're a nobody with a tired car, a little cash and a debt, somewhere in an endless 1970s
countryside. You drive — **from behind the dashboard, like Test Drive** — along winding roads between
villages, and nothing stops you from leaving the road and cutting across a field to a church tower
you saw on the horizon. In the villages you do your business on **Street Rod-style 2D screens**: fuel,
parts, a newspaper, a back room where goods change hands. You buy where things are cheap, haul them
to where they're dear, and spend the money on making the car better — faster, tougher, more
anonymous, with a panel the cops won't find.

It's quiet most of the time. The radio, the engine, dusk coming on. And then there's a car in your
mirror that's been there for three villages.

### The three games it braids together

| Ancestor | What we take | What we fix |
|---|---|---|
| **Test Drive** (Accolade, 1987) | Cockpit view, the dashboard, the feeling of a road trip | Its world was a painted corridor — you could never leave the road. Ours is real terrain. |
| **Street Rod** (1989) | Buy a junker, tweak it at the garage, earn money to improve it further; places as hand-drawn 2D screens | Its world was tiny. Ours is endless, so the upgrade loop has somewhere to go. |
| **Drug Wars** (1984) | Buy low / sell high across places, price swings, events, debt | Travel was a menu. Here travel *is* the game — you drive every kilometre. |

Mood references (films, 1970s): *The French Connection*, *The Driver*, *Two-Lane Blacktop*,
*Vanishing Point*, *Sorcerer*.

---

## 2. What already exists in this repo (the parts bin)

This cart is mostly a **new consumer** of things the repo already has. Verify each against the code —
this table was written from a skim.

| Need | Where it lives | Notes |
|---|---|---|
| Endless deterministic world: terrain, towns/hubs, spline roads | `runtime/worldnet.h` (home cart: `roadnet2`) | 1 unit = 1 m. `wn_road_at(x,y)` → on_road / class / dist. `wn_nearest_road_point()` to spawn on a road. |
| Car handling computed from parts | `sloop` + `docs/design/sloop.md` | Rigid body, gears, traction, per-part mass. Reuse the model or a simplified subset — don't fork 4000 lines. |
| Driving-feel bench | `drivemodes` (steering/grip) | |
| Trade economy | `druglord` (Drug Wars port), `tradewinds` (economy + upgrades + travel encounters) | Price-skew matrix, events, debt, bank. |
| Flat-shaded, dithered 3D | `solid3d`, `textured3d`, `infiniminer` | `rot3`, `project3`, `trifill`, `tritex`, `fillp` checker shading, painter's sort. |
| Pseudo-3D road ancestors | `racer`, `outrun` | Useful for sky/horizon/billboard tricks, not the projection. |
| Depth test + near-plane clip | `docs/design/depth-tested-triangles.md` (ADR-0036) | **Designed, not built.** This cart may be the consumer that justifies it — see §5.6. |
| Procedural faces | `facegen` | Shopkeepers and contacts. |
| Radio stations | the radio station family (e.g. `roadhouse`), `docs/guides/radio-station-howto.md` | The car radio. |
| Palette / colour | `pal()`, `fillp`, `blend()`, `docs/design/palette-and-color.md` | 32 colours; hex/RGB path exists but use sparingly. |

---

## 3. Pillars and non-goals

### Pillars (DECIDED)

1. **The drive is the game.** Cockpit view, first person, through an endless world you can leave the
   road in. If cruising isn't pleasant on its own, nothing else matters.
2. **Retro for real.** It should look like it could have shipped around 1990 — flat-shaded polygons,
   dither, chunky 320×200 pixels, a hand-drawn dashboard — with modern *features* underneath
   (procedural terrain, an endless road network, villages placed by the world). Not a modern renderer
   with a pixel filter on top.
3. **The car is yours.** Street Rod's garage loop: parts change how the car drives *and* how the
   dashboard looks.
4. **Gritty, not cheeky.** A 1970s crime film, not Smokey and the Bandit.

### Non-goals (DECIDED)

- **No walking.** You never leave the car in 3D. Everything off the road happens on 2D screens.
- **No top-down or chase camera.** Cockpit only (a rearview mirror is fine).
- **Not a racing game.** Racing may appear as a way to earn money, but it is not the spine.
- **No modern rendering**: no smooth shading, no high-res textures, no post-processing glow.
- **No on-screen gore.** Violence, when it exists at all, is implied — a scene card, a line of text.

---

## 4. Setting and tone (DECIDED: 1970s, gritty)

- **Era**: 1970s. Muscle cars, station wagons and tired sedans; CB radios and 8-tracks; the fuel
  crisis — fuel prices can spike, and every tank matters.
- **Place**: a fictional rural region — farmland, forest, hills, coast, the odd bigger town. Keep it
  unspecific enough that the world generator can be the author.
- **Look**: mostly browns, dark greys, muted greens, dark peach for sodium lights; save the bright
  colours for things that should pop — police lights, neon, a sunset. Overcast, dusk, rain and night
  should be common. Dither as film grain. 2D scenes look worn: peeling paint, flickering signs.
- **Writing**: terse. People don't explain themselves. Nervous contacts, shopkeepers who look away,
  a mechanic who notices the hidden compartment and says nothing — or wants money. No jokes, or only
  very dry ones.
- **Sound**: the engine is half the experience — pitch from RPM, load, gear changes, gravel rumble.
  The radio carries period-flavoured stations and, later, news bulletins (busts in the region,
  sometimes uncomfortably close to your route).

---

## 5. The drive (cockpit view)

### 5.1 The world under the wheels

- Consume `worldnet.h` as-is. Villages = worldnet town nodes; bigger towns = hubs. Roads = its spline
  edges, queried with `wn_road_at()`.
- Pick a **vertical scale** for worldnet's height so hills read as hills at driving speed. Tune by eye.
- **Scale check**: worldnet's town spacing is 3–8 km. At 90 km/h that's 2–5 minutes between villages,
  which may be about right — or may need the density slider. Tune by playing.
- **Surfaces**: asphalt (on a road), gravel/dirt (minor roads and tracks), grass, mud, sand, water
  (impassable or engine-killing). Derive from `wn_road_at()` class + terrain band.

### 5.2 Terrain rendering

- A **grid of triangles around the car**, sampled from the heightmap: fine cells close by, coarser
  rings further out (simple distance LOD), ending in **fog at ~1–2 km** (tune for performance).
- A heightmap grid can be drawn **back-to-front by cell order** relative to the camera, so a working
  first version needs no z-buffer and no per-frame sort.
- **Flat shading** per triangle from a fixed sun direction; in-between shades via `fillp` checker
  dither (see `solid3d`). Fog = blending toward the sky colour through dither steps, not alpha.
- **Sky**: a dithered gradient, sun/moon, a few cloud bands. **Horizon**: a hand-drawn mountain or
  treeline silhouette at infinity that only scrolls with yaw — cheap, and very Test Drive.

### 5.3 Roads on the terrain

- **Rung A (cheap)**: colour terrain cells by `wn_road_at()`. Chunky, period-appropriate, proves the
  loop.
- **Rung B (proper)**: drape worldnet's road polylines onto the terrain as their own strips, with
  centre-line dashes and edge posts. Dashes and posts rushing past are how old racers sold speed.
- Watch for road strips fighting with the terrain triangles they sit on — draw a cell's road after
  the cell, or lift it slightly.

### 5.4 Things in the world

- **Billboards**: trees, fence posts, telegraph poles, road signs — camera-facing sprites scaled by
  distance (see `outrun`/`racer` for the feel).
- **Villages from the road**: a handful of low-poly boxes with pitched roofs, and a **landmark you can
  see from far away** (church tower, water tower, grain silo) so you *want* to go there.
- **Traffic**: later and sparse. A few cars, a tractor, the occasional police cruiser.

### 5.5 The car and how it feels

- Start with a simple 2D rigid-body car (borrowing `sloop`'s model or a subset of it); the sim stays
  2D on the ground plane.
- **Pitch and roll** come from sampling terrain height under the four wheels — the camera tilts as
  you go over bumps, crests and ditches. Cheap, and sells the off-road drive.
- **Dash shake** scales with surface roughness and speed.
- Surface changes grip, rolling resistance and top speed — leaving the asphalt should feel like a
  choice with texture.
- Gears and an engine note driven by RPM from day one; it's half of the feel.

### 5.6 Depth and the near plane

- Terrain alone works with back-to-front order. Buildings, billboards and later traffic poking
  through terrain is where painter's order breaks, and getting close to a wall needs a near-plane clip.
- `depth-tested-triangles.md` (ADR-0036) designs exactly that — a per-pixel depth test and a
  near-plane clip, as leaf primitives in the ADR-0009 shape. **Don't build it on your own initiative.**
  When the cart hits the wall, write up the evidence (filmstrip of the artefact) and propose it to the
  maker as the trigger.

### 5.7 The dashboard

- Covers roughly the **bottom third** of the screen — which also means less 3D to draw.
- Hand-drawn, 1970s: woodgrain or vinyl, round gauges with real needles (speed, fuel; more later),
  a radio/8-track, maybe a CB handset, fuzzy dice. Hands on the wheel that turn with your steering.
- **Parts change the dash** (Street Rod's best trick, extended): install a tachometer and it appears;
  add a police scanner and a little box with a blinking light gets bolted underneath.
- **Rearview mirror**: a small strip at the top. First version can be a cheap fake (sky + a car
  sprite when something is behind you); a real low-res render is a later luxury.
- **Night**: headlights as a lit zone ahead with everything else dithered toward dark. Police lights
  flashing in the mirror at night is a moment worth building toward.

---

## 6. Villages — Street Rod-style 2D screens (DECIDED: no walking)

### 6.1 Arriving

You drive into a village; a sign shows its name. Pull up somewhere — a gas station forecourt, the
square, behind the bar — and stop. The screen cuts to a **pixel-art scene of that place, with your own
car parked out front** (its actual paint and parts). Business happens there; drive off when done.

### 6.2 Making endless villages distinct

Scenes are **composed per village seed** from a library of hand-drawn pieces: facades, awnings, signs
with generated names, props (bench, dog, pay phone, a parked car). `pal()` swaps for time of day and
weather — the same diner reads differently at noon, at dusk and in rain. `facegen` gives every
shopkeeper and contact a face, so people become recognisable across visits.

### 6.3 Businesses

Small villages have one or two; towns have more.

| Place | What you do there |
|---|---|
| **Gas station** | Fuel (price varies; fuel crisis events). A notice board: gossip, odd jobs. |
| **Garage** | Street Rod's heart: buy and fit parts. Stock varies per village. Labour costs money and time. |
| **Junkyard** | Used parts, cheap, condition varies. Sell parts you no longer need. |
| **Diner** | Newspaper: classifieds (cars for sale), price rumours, police news. Where you meet contacts. |
| **Bar / back room** | The shady trading — the druglord-style buy/sell screen. Mostly after dark. |
| **Motel** | Sleep, save, pass time. Or sleep in the car at a rest stop if you can't afford it. |
| **Phone booth** | Call contacts you've met: tips, deals, jobs. |

---

## 7. The trade and the law

### 7.1 Goods

A mix, so the player can run clean, dirty, or in between:

- **Legal cargo**: produce, machinery, mail/parcels — low margin, low risk. Deliveries between villages.
- **Contraband**, kept abstract in Drug Wars style (names, prices and risk levels, no process detail):
  bootleg liquor, untaxed cigarettes, stolen car parts, "hot" electronics, and a few drug categories
  as tradeable commodities.

### 7.2 Prices

- Each village has a **price skew per good** from its seed (what it produces cheap, what it pays
  dear), plus drift over time and **events** (a bust makes something scarce and expensive regionally,
  a glut tanks it). Start from `druglord`'s matrix and event cards.
- The endless world naturally creates **trade routes**: the further and riskier the run, the bigger
  the margin.
- **Information is currency.** You only know prices you've seen or heard. The **notebook** keeps the
  last-seen price per village *with its date*; old information quietly goes stale. A contact's phoned
  tip about a shortage three villages over is genuinely valuable.

### 7.3 The garage, smuggling edition

Besides engines, gearboxes, tyres, suspension and bodywork, the garage sells things that matter for
the trade: **hidden compartments**, a **CB radio** (hear chatter), a **police scanner**, a **bigger
fuel tank** for long runs through nowhere, and a **plain paint job**. The core tension: the flashy
muscle car is fast but conspicuous; the beige station wagon is slow and nobody looks twice.

Off-road ability is progression too: knobby tyres open muddy farm tracks, raised suspension opens
rough ground, headlights make night driving viable — and the back way around a checkpoint is often a
farm track.

### 7.4 Heat

- **Per-region heat**, raised by dealing (and by being seen doing it), decaying over time.
- **Shown, not metered**: in the 2D scenes — a cruiser parked outside the diner, a deputy at the
  counter, the back room's door closed. In the drive — roadblocks, cruisers at village entrances.
- **Traffic stops** become a tense 2D scene: the cop walks up; what's behind the panel, how clean the
  car looks, and what you say all matter.
- **Paranoia**: contacts carry hidden trust. Someone you squeezed may become an informant. A too-good
  deal might be a setup. An unfamiliar car in the mirror for three villages should make you nervous.
- **The world remembers**: villages you deal in heavily slowly change — more police, a shop closing,
  a contact who's gone. Nothing preachy; just marks left on places.

---

## 8. Time, fuel, weather, radio

- **Day/night cycle** that matters: some business only after dark; night driving is harder and lonelier.
- **Fuel** is the gentle leash that pushes you toward villages; the fuel crisis makes it bite.
- **Weather**: overcast and rain common; rain lowers grip and visibility.
- **Radio**: a few period stations built on the radio family; later, news bulletins that react to
  what's happening in the region.

---

## 9. Build ladder

Each rung ends with: a headless `--det` filmstrip, a short note in §10, and **a stop for the maker
to play it**. Don't start the next rung without a go.

| Rung | Goal | Done when |
|---|---|---|
| **0. Orient** | Read §0's list; write a rung-1 plan. | Maker approves the plan. |
| **1. Cockpit over terrain** | worldnet terrain as a flat-shaded grid around the camera, dithered sky, fog, horizon silhouette. A placeholder car you can drive anywhere (simple handling). | You can drive over hills at a stable frame rate; filmstrip shows terrain, fog and sky reading clearly at 320×200. Frame-time numbers recorded. |
| **2. Roads + ground feel** | Roads on the terrain (rung A colouring, then rung B strips if cheap). Surfaces with different grip. Four-wheel pitch/roll + dash shake. | Leaving the road *feels* different; a crest tilts the view; you can follow a road from one town to the next. |
| **3. Dashboard + sound** | Hand-drawn dash, speedo + fuel needles, hands that steer, gears, RPM-driven engine note. | It feels like sitting in a car. Maker verdict: "is cruising fun?" |
| **4. Villages you can reach** | Low-poly village silhouettes with a far-visible landmark. Arrive + stop → cut to one placeholder 2D scene → drive off. | You can see a village, drive to it across a field, and "enter" it. |
| **5. The loop** | Gas station (fuel) + back room (minimal druglord-style buy/sell) + notebook of dated prices. Per-village price skew from seed. | A full buy-drive-sell round trip is possible and you can tell whether it's worth it. |
| **6. Garage** | A handful of parts that change handling *and* the dashboard. One smuggling part (hidden compartment). | Money → part → the car drives/looks different. |
| **7. Heat** | Per-region heat, visible in scenes; one traffic-stop scene. | The first tense moment exists — and we can start answering §11. |

Later rungs (unordered): day/night + headlights, weather, contacts + phone booth + trust, radio
stations + bulletins, junkyard, traffic, rearview mirror render, procedural scene composition at full
richness, the world remembering, save/load.

---

## 10. Progress log

*(Agent: append dated entries here — what shipped, commits, measurements, surprises, decisions
taken with the maker.)*

### 2026-11-19 — rung 1: cockpit over terrain

**Built** `tools/carts/backroads.c` (320×200; shared headers untouched). Seed drive:
`tools/clips/backroads/01-first-drive.script`.

- **Terrain rings:** three square vertex grids around the car: 16 m cells (32×32), 64 m (32×32)
  and 256 m (24×24), reaching about 3 km. Each ring skips the square the finer ring covers. The
  squares are aligned to the next-coarser grid so they tile exactly, and the finer ring's border
  heights are interpolated from the coarse corners, so there are no gaps where rings meet.
- **No sort and no z-buffer:** outer ring first, then within a ring rows by distance from the
  camera row (far first), then columns the same way. This is a correct painter's order for a
  heightfield viewed from inside the grid.
- **Near clip, cart-local** (agreed with the maker): one Sutherland–Hodgman pass against z = 0.35 m
  in camera space before projecting. Not the ADR-0036 engine primitive; it only covers terrain,
  and buildings/billboards will still need ADR-0036.
- **Shading:** flat per triangle from a low south-west sun, through 4-anchor `fillp` checker ramps.
  Fog is 25/50/75 % haze dither from 900 m to 2.9 km. The sky is dithered bands from darker blue to
  mauve to haze, with a ridge silhouette made of circular noise in heading (so it wraps around).
- **Placeholder car:** steering scaled by speed, gravity along the slope, pitch from a 4 m
  front/back height sample, stops dead at water. No roll (rung 2).

**Surprises**
- **worldnet's terrain is flat from the driver's seat.** Its smallest noise octave is about 3 km
  across, so at any useful `vscale` the next few hundred metres are a tabletop. The cart adds
  **driving-scale detail**: about 700 m swells plus 180 m bumps (default ±45 m, keys `[`/`]`),
  faded to zero near sea level. It's a pure function of x and y, so it's still deterministic and
  the same everywhere. `vscale` (default 900 m per unit, keys `-`/`=`) still sets the big hills.
  Roads will have to drape over the *detailed* surface at rung 2.
- **Bands by elevation alone gave one colour from horizon to horizon** (worldnet's legend makes
  0.2–0.3 all forest). The cart uses its own countryside banding instead: woods from a 900 m noise
  blob, and below that a 120 m patchwork of plough/pasture/grass/hay. The four ramps land on
  *different* anchors on flat ground, so neighbouring fields read apart.
- `line()` on 1-pixel vertical columns dropped every fifth column in the ridge; it's drawn with
  `rectfill` now.

**Frame time** (Intel office Mac, native, live profiler during the seed drive): **3.6 ms average,
4.3 ms worst** of `update+draw` work, about 1.1–1.3k `trifill` a frame. Height sampling (~3k
`height_at` + detail per frame, uncached) is affordable, so there's no height cache. Not yet
measured on web or phone.

**Open / for the maker's play-test:** does the hill scale feel right (`-`/`=`, `[`/`]`)? Is the
16 m near cell too coarse? Field edges follow cell diagonals, so they look jaggy up close. With no
roads or posts yet there are few speed cues; that is rung 2's job.

### 2026-11-19 — rung 2: roads + ground feel

Went straight to **rung B** (strips draped on the terrain) with the maker's OK: rung A's
per-cell colouring would have been 16 m wide and crude.

- **Roads = worldnet's edges, redrawn locally.** For every cached edge segment within about
  1.15 km, the cart builds Catmull-Rom pieces between the samples (6 m pieces near the car,
  16–32 m further out). Each piece is binned into **every ring cell it touches** (rings 0–1),
  then at draw time clipped to that cell **and to the cell's triangle** (world-coord
  Sutherland–Hodgman). It is drawn on that exact plane right after the cell's own triangles. So
  roads sit exactly on the drawn ground, and the grid painter's order still hides a road behind a
  nearer hill. No depth buffer and no lift needed.
- **The cart's own widths** (`HW[]`: 7 / 4.5 / 3.6 / 3.0 / 2.0 m half-width, motorway → dirt).
  worldnet's widths read as runways from a 1.2 m eye. Surface detection uses the same widths
  (`wn_road_at().dist <= HW[cls]`), so what you see is what you drive on. Dirt roads use the
  gravel ramp. Spawn is one lane to the right.
- **Markings by arc length:** centre dashes (4 m on, 12 m gap) within 160 m, and edge posts
  (white, dark cap, every 50 m, both sides) within 420 m. They are placed by distance along the
  edge, so they stay fixed in the world while you drive past them. Dashes and posts are the first
  real speed cue.
- **Cracks, and how they were fixed:**
  1. Clipping in cell-local u,v gave slightly different border vertices from each side.
     Clipping in world coordinates plus one shared `ring_h()` (the cell is chosen by floor)
     makes them bit-identical.
  2. T-junction cracks remained wherever one fragment's edge has an extra vertex. Each road
     fragment is now outlined in its own colour. `line()` ignores `fillp`, so road shades are
     solid anchors only, and fogged fragments skip the outline.
  3. A frustum side-reject before the near clip dropped visible pieces (a point behind the camera
     has no meaningful side); it now runs after the clip.
  
  One faint dotted seam is still visible at mid-distance in some frames.
- **Ground feel:**
  - **Four wheels** (2.6 m wheelbase, 1.5 m track) sample the ground plus a small felt-not-drawn
    texture bump scaled by surface roughness. They give pitch, **roll** (the whole camera, sky
    included, rotates) and ride height, smoothed by a spring.
  - **Surfaces** (`SURF[]`) set grip, rolling drag and roughness:
    - asphalt: grip 1.0, drag 0.015
    - gravel: 0.7 / 0.04
    - grass and pasture: 0.55 / 0.15
    - plough: 0.4 / 0.4
    - woods: 0.45 / 0.6
  - Grip bleeds off sideways velocity (low grip lets the tail drift) and weakens steering.
    Drag sets top speed: about 135 km/h on asphalt, about 80 on grass, under 50 on plough.
  - The dash shakes with roughness × speed.
  - Water stops you unless you're on a road (bridges).
- **`shake` is an engine built-in.** The cart's variable was renamed `dash_shake`, which is the
  "don't name a variable after a built-in" rule again.

**Proof:** a test-only autopilot (`#ifdef DE_TRACE`, key P) follows the drawn centre-line one lane
right. `tools/clips/backroads/02-autopilot-cruise.script` runs 10,800 frames and covers about
5.1 km, **every frame on asphalt**. The `01-first-drive` seed now leaves the road onto grass,
pasture and plough, and the roll shows.

**Frame time** (same Mac, live profiler, mixed on/off-road drive): **5.0 ms average, 6.5 ms
worst** (rung 1 was 3.6 / 4.3), about 2.5k `trifill` + 1.7k `line` a frame. 

**For the play-test / next:**
- worldnet links between towns are often long and nearly straight, so a 5 km cruise can be one
  heading.
- Roads drape over side slopes, so the car banks noticeably on them (real roads are levelled; a
  cut-and-fill pass would fix it).
- Off-road at speed in one big parcel has no speed cue. Crop rows, tufts or billboards would help
  (later rung).
- The edge cache rebuilds once per node-cell crossing (worldnet's cost model); watch for a hitch.

### 2026-11-19: river pits fixed, TAB map

- **The "ravine" at pos 35383,27153 was a river.** worldnet stamps rivers as a flat `-0.5`
  height inside a thin noise band (a step, not a valley), and the cart mapped every `t <= 0` to
  sea level, so a river cutting 210 m high land became a 210 m slot canyon. The road bridges it
  in 2D, but here it draped down into the slot and the autopilot drove in (camh 212 m to 1.2 m).
  Fix: `worldnet.h` gained `wn_height_base()` (the land before the river carve; `height_at()`
  output is unchanged, and roadnet2/sloop/citygrow still compile). `hgt_xy()` beds a river
  `RIVER_CUT` (3 m) below that land, and `wheel_h()` rides the bridge deck (land height) when a
  wheel is on a road over the river, so the crossing doesn't jolt. The 10,800-frame autopilot
  cruise is now clean: max camera step 0.1 m/frame, max tilt 0.21 rad, every frame on asphalt.
  The drawn road still dips 3 m across the river (it drapes on the cells); real bridge decks are
  a later job.
- **TAB map:** north-up, car arrow at the centre, terrain sampled per 2 px block only while open,
  roads from the cached worldnet edges (whole links around the car), towns from the node
  lattice (pink = town, peach = hamlet), 1 km scale bar. Wheel zooms 4 to 120 m/px (default 60).
  Drag pans (the view then stays fixed in the world while the car moves on it); a "centre"
  button appears top-right and snaps back to following the car.
  Roads outside the edge cache aren't drawn, so the far corners of a wide view look emptier
  than the world is.

---

## 11. Open questions — to be answered by playing

- **How unforgiving?** Can a bust cost everything (start over), or is it always a painful,
  recoverable setback — losing cargo, the car, or months of in-game time while the world moves on?
- **How much violence**, if any — and does it stay purely implied on scene cards?
- **What's the arc?** A debt to clear, a rival to outgrow, a network to build — or pure sandbox?
- **Starting state**: which junker, how much cash, how much debt, which village?
- **Village density and spacing** — worldnet's defaults or tuned for this game?
- **Exact goods list** and how abstract to keep them.
- **Draw distance vs. frame rate** on the web build and on phones.
- **The name.**
