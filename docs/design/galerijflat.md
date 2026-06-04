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
- ✓ **A flip button shows the balcony side** — same building seen from the
  other side, NOT a second facade. One model, two views (system 4 below).
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

## Systems (running list — being collected across sessions, not complete)

The cart decomposes into a few systems we'll design/build one at a time:

### 1. Time of day & sky

Day, night, weather — the sky as a slow clock behind/above the slab.

- **Keyframed sky gradients**: a set of (top, bottom) palette pairs along the
  day — night / dawn / morning / flat grey noon / golden hour / dusk / deep
  night — drawn with `vgradient()` (trafficjam's sky is the one-liner version).
- **Transitions**: with 32 fixed colors there's no smooth lerp between
  keyframes; the candidate technique is a **dithered crossfade** — `fillp()`
  patterns blending the outgoing gradient into the incoming one over a few
  steps (the tint-to-white sequencing trick from the juice notes, applied to
  sky). `geometry-helpers.md` parks the true-color-lerp idea; we stay
  palette-pure.
- **Weather**: overcast (grey gradient set, flatter light), rain (streaks +
  wet-dark concrete?), big Dutch cumulus? Weather modulates which keyframe
  set is active — it's a *filter on the day*, not a separate clock.
- Time also drives the *building*: household wake/sleep windows, when lights
  make sense. Other systems read the clock; the clock reads nothing.
- **Build plan (decided): the sky gets its own standalone cart first**
  (`sky.c` / `dutchsky.c`, own session) — the full cycle night → dawn → grey
  noon → golden hour → dusk → deep night, the dithered-crossfade transition,
  weather variants, all tunable in isolation. House pattern: one technique,
  one cart (doomfire-shaped). Constraints so it lifts cleanly: same 320×200,
  same palette discipline, **keyframes as a data table** (time → top/bottom
  color pairs) + a `draw_sky(t)` function — importing into galerijflat is
  then a copy-paste, not a rewrite. It also produces evidence for the
  blend-API investigation (sys 6 note). Until then galerijflat carries a
  fixed-dusk placeholder sky.

### 2. Windows — what we see in them

Every dwelling's window gets a **window treatment**, rolled per household as
part of its generative identity. Very Dutch axis: from bare glass to fully
dressed.

- **Treatment types**: none (bare glass) / **vitrage** (net curtains — the
  Dutch staple) / fabric curtains (open ↔ closed, household color) / roller
  blind (down to a per-household height) / venetian blinds (slat stripes).
- **Patterns & colors**: treatments carry color and pattern variation —
  `fillp()` is the natural tool (dither = lace/vitrage, horizontal stripe
  patterns = venetian slats, solid = roller blind).
- **Interaction with light** (this is where it gets arty): the treatment
  *shapes* the lit window at night — venetians = striped glow, roller blind
  half down = glow with a hard top edge, closed curtains = soft colored
  wash, vitrage = dimmed dithered glow, bare glass = full warm rectangle
  (+ TV flicker visible). By day the treatments read as pattern/color on
  dark glass instead.
- State can change slowly: blinds come down at dusk, curtains close before
  bed — cheap, legible "the building is alive" signals tied to the clock.

### 3. Windowsills — things on them

The **vensterbank**: Dutch windowsill culture, visible from the street, rolled
per household alongside the treatment.

- **Items**: tiny plants (green pixel atop a pot pixel), pots, jars/vases,
  clutter/trash, or deliberately **nothing**. At kitchen-window scale
  (~8×6 px) each item is 1–2 px — a sill row is a handful of psets.
- **Arrangement is character**: **symmetrical** (mirrored around the window
  center — the tidy, deliberate household; classic vensterbank) vs **random
  scatter** (lived-in) vs **empty** (just moved in? doesn't care?). The
  arrangement style says as much about the household as the items do.
- **Night payoff**: against a lit window the sill items become little black
  **silhouettes** in the glow — drawn after the light, before the treatment's
  shaping. By day they're colored specks on the facade rhythm.
- Mostly static (unlike treatments) — sills change on re-roll, not on the
  clock. Maybe the rare exception: a plant appearing one day.

### 4. The flip — one building, two faces

A button flips the view to the **balcony side**. The non-negotiable: it must
read as *the same flat seen from behind*, never as another building.

- **One model, two renderers.** All state lives in the building/household
  model; the gallery facade and the balcony facade are two views of it.
  Nothing is rolled per-view.
- **Mirrored, like walking around it**: flipping reverses the horizontal
  order — the dwelling at the left end seen from the gallery side is at the
  right end seen from the balcony side. The lift tower swaps ends. Getting
  this wrong would silently break the "same building" illusion.
- **Each dwelling has two rooms showing**: kitchen + front door on the
  gallery side, living room + balcony on the other. One household state
  drives both — and they can *differ meaningfully by time*: morning = kitchen
  lit / living room dark; evening = TV glow in the living room, kitchen dark.
  Anyone who flips at dusk should see the same households awake, with the
  light in the other room. Household identity carries across (curtain
  colors/taste, tidy-vs-messy applies to balcony clutter too).
- **Balcony-side anatomy**: balcony slabs + railings/privacy panels in the
  same band rhythm, bigger living-room windows, laundry racks, parasols,
  plants, the occasional stored bike — the *private* face vs the gallery's
  *shared* face. No front doors, no walkers; quieter.
- **The flip itself**: instant cut vs a tiny transition (sky stays put,
  building swaps) — open. Sky/weather/time are of course identical on both
  sides (sun side might warrant a brightness bias later).

### 5. The inhabitants — households as the generative root

Who lives there. Composition ranges from **one old person** to a **young
family of five** and everything in between — and crucially, **the household
explains the window**: sill and curtains must *correlate* with who lives
behind them. A stranger reading the facade should be able to guess.

- **Roll order inverts**: the household is rolled FIRST; treatments (sys 2),
  sill (sys 3), light schedule (sys 1) and balcony clutter (sys 4) are
  *expressed traits* of that household — derived with correlated
  distributions, not independent rolls. (Same idea as trafficjam's
  type-then-jitter: archetype sets the center, jitter keeps it alive.
  Archetypes **bias**, never dictate — the occasional chaotic granny is
  the point.)
- **Archetype sketches** (the correlation table to tune later):
  - *single elderly* — vitrage + symmetrical sill (the full vensterbank),
    warm light, early to bed, slow rare gallery walks
  - *young family (3–5)* — closed curtains at night, cluttered/random sill
    (toys), early mornings, most laundry on the balcony, most walkers
  - *student / young single* — bare glass or a half-stuck roller blind,
    empty-or-trash sill, TV flicker, lights on past midnight
  - *couple, middle* — venetians, modest tidy sill, regular hours
  - *vacant* — no treatment, empty sill, never lit (a few per building;
    silence is part of the rhythm)
- **Composition shows**: number of residents = how many figures can walk the
  gallery / appear on the balcony; family bustle vs single-person stillness
  is visible in event frequency, not just decor.
- **Day rhythm per archetype**: who's lit at 7:00 (family kitchen) vs 01:00
  (student) — the facade patchwork becomes *legible* once you know the codes.

### 6. The daily round — residents on the clock

The inhabitants (sys 5) aren't decor states; they're **little agents on the
day clock (sys 1), going about their business** — and the building's state
changes because *they* change it.

- **A routine per resident** (state machine, archetype-flavored): asleep →
  wake → kitchen → out the door → **walk the gallery** → **lift down** →
  away → lift up → gallery walk home → living room / TV → **sit on the
  balcony** (other side, nice weather) → curtains closed → bed.
- **Visible behaviors** (the full vocabulary so far):
  - *moving inside* — a figure silhouette crossing a lit window, room to
    room (kitchen ↔ living room = the two faces of sys 4)
  - *opening/closing curtains & blinds* — sys 2's state changes get an
    actor: a figure at the window just before it happens
  - *turning lights on/off* — ditto; rooms go lit/dark because someone did it
  - *gallery walks* — out the front door, along the band, to the lift
  - *balcony sitting* — the back-side counterpart; a chair, a still figure
  - *entering/leaving the building* — at the plinth, via the **elevator**;
    the lift indicator climbing to floor 7 *means someone is coming home*
- **Causality as the design law**: ideally no light flips and no curtain
  moves without a resident plausibly there. The patchwork stops being random
  twinkle and becomes *testimony* — you saw her walk the gallery, the door
  closed, the kitchen lit up.
- **Engine gap, parked**: lights/glow want some kind of **blend / additive
  API** for soft light falloff — none exists yet; another agent is looking
  into it. Until then: hard palette fills + fillp dithers (which may be
  plenty, and is period-appropriate anyway).

### 7. The elevator — a real one

Promoted out of sys 6 because it **has to actually work**: a simulated lift,
not an animation. People **queue for it, wait, board, and ride to the floor
they want** — it's the building's circulation pump and its metronome.

- **A real state machine**: the car has a floor, a direction, a door state
  (opening/open/closing/closed), and a **call queue**. Residents press a
  call; the car serves calls in a sane order (classic elevator algorithm:
  keep going in one direction while there are calls ahead, then reverse).
- **Queueing is visible**: residents wait *at the lift door on their floor*
  (gallery side) or at the plinth — one figure standing, then two, a little
  cluster at 8:00 rush. Boarding: doors open, waiters step in, doors close.
  Someone arriving as doors close waits for the next round trip.
- **Riding**: a boarded resident is *in* the car (invisible), gets out at
  their destination floor and continues their routine (gallery walk home,
  or out the plinth into the world).
- **Readable from the facade**: the indicator light climbing the tower, the
  pause at floor 7, the cluster dissolving — regulars (the player) learn to
  read who's coming and going. Morning down-rush and evening up-rush emerge
  from sys 5 schedules for free.
- **Sound hooks**: the ding, door rumble, the hum while travelling (sys 1's
  audio bed). The ding at night when everything is quiet = pure atmosphere.
- Capacity, a second car, or a broken-lift day ("DEFECT" sign, everyone
  takes the stairs) are flavor options, not core.

*(list to be continued in a future session)*

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

## Build log

### Step 1 — the static facade (2026-06-04, WIP)

Cart: `tools/carts/galerijflat.c` — **exists, renders, deliberately NOT yet
registered in `index.json`** (stays out of the tutorials panel until it looks
right). Contains temporary debug guards (a bands-only early return + an
NF/NB/BW/baseY/wallTop debug print) — remove when finding #2 below is closed.

**Validated:** the screen budget holds exactly as designed — debug bake reads
`NF10 NB10 BW26 base174 top34`. Slab, floor bands (slab line / railing bars or
colored corrugated panel / door + kitchen window per bay), lift tower with
glazed stair zigzag + landing lights, berging plinth with lit entrance, ground
strip, lampposts: all render and the silhouette unmistakably reads
*galerijflat*. Household roll (archetype-first, correlated treatment/sill/lit
per sys 5) is in and produces legible variety.

**Findings — three real issues uncovered, one still open:**

1. **ENGINE BUG (confirmed): `vgradient` renders inverted.** The dithered
   interior of every vertical gradient puts `c_bot` at the top and `c_top` at
   the bottom; only the exact first/last rows (early-return paths in
   `gradient_band`) are correct, leaving a 1px discontinuity nobody noticed.
   Mechanism: `gradient_band` encodes blend ratio `t` as a dither-threshold
   pattern but applies the table backwards relative to `fillp`'s
   bit-semantics (`t→0` yields pattern `0x0000` = all draw-color = `c_bot`).
   **Evidence it's engine-wide, not this cart:** trafficjam's sky
   (`vgradient(... CLR_BLUE, CLR_LIGHT_PEACH)`) bakes with peach at the top.
   Fix is ~one line (use `(1-t)` or swap the fillp/rectfill color roles), BUT
   it visibly flips every existing vgradient cart → needs a quick audit of
   `vgradient`/`hgradient` call sites + re-bake of affected thumbnails.
   Logged in `docs/STATUS.md` (under the shipped geometry-helpers item).
   **The sky cart (sys 1 build plan) must land after this fix.**
2. **OPEN MYSTERY: the "phantom band".** The full bake shows a full-width
   band at y≈3–24 (vitrage-like peach dither + railing-like 3px bars on
   white + grey checker) that no code on paper draws there. Bisect state:
   sky-only bake → band absent (not the sky); bands-only bake → absent (not
   the bands). Remaining suspects: `draw_tower` / `draw_plinth` / ground /
   title — yet all are coordinate-bounded on paper. Next test: re-enable
   those one at a time. (Lesson from trafficjam applies: "can't happen on
   paper" is where the bug lives.)
3. **Self-bug, trivial: the crescent moon** is a near-total eclipse — the
   cover circle (same radius, offset 3px) hides almost the whole disc,
   leaving a black blob. Draw a thinner crescent (smaller/farther cover).
4. **Look pass needed (not a bug): too confetti for dusk.** Saturated doors/
   curtains everywhere + too many lit windows read as carnival, washing out
   at thumbnail scale; wall color barely visible between elements. Muting
   plan: doors mostly from the dark palette row, fewer lit windows, curtain
   *dark* variants dominant at dusk, lit warm windows as the scarce accent.
   Also: house numbers currently print at `baseY-4` (on the floor-0 band) —
   move them onto the plinth.

**Going forward (order):** (a) bisect the phantom band to its source;
(b) moon + house numbers + muting pass; (c) the `vgradient` engine fix as its
own commit with cart audit; (d) then the sky cart session builds on the fixed
primitive; (e) register the cart in `index.json` only when the dusk facade is
worth a thumbnail.

## References in-repo

- `tools/carts/trafficjam.c` — closest relative: generative rect-built
  entities, ambient sim, palette discipline, mouse honk
- `tools/carts/loveparade.c` — procession-of-life energy
- `docs/design/geometry-helpers.md` — the geometry-first drawing style this
  cart should follow
