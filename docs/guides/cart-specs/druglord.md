# The game you're building: DRUGLORD

## Premise
A Drugwars-style economy game. You're a small-time dealer with **30 days** to turn a
small stake — and a loan-shark debt — into as much cash as possible, trading **6
substances** across **6 city districts** whose prices swing daily. Buy low, sell high,
dodge cops and muggers, manage debt and a limited coat capacity. It's a menu + map
game, NOT a walking/combat game — but it should look and feel alive.

## The core loop (one "turn" = traveling to a district = one day)
1. You're standing in a district. Its **market** shows today's 6 prices (fluctuating;
   each district skews cheap/expensive for certain goods, plus daily `noise`).
2. **Buy/sell** limited by cash and **carry capacity** (coat slots). Stash cash in the
   **bank** (safe from muggers) and pay down the **loan shark** (debt grows ~10%/day).
3. **Travel** to another district on the city map → advances the day, may trigger a
   **random event**, and re-rolls prices.
4. After day 30: settle debt, final score = cash. `save()` the best run.

## Random events (fire on travel / sometimes on entering a market)
Price shocks ("crackdown — COCAINE soars", "shipment floods town — WEED crashes"),
**mugging** (lose cash or goods), **cop bust** (lose goods or pay a fine / chance to
run), **lucky find** (free goods), **loan-shark shakedown**. Each is an event card.

## Screens / states
- **MARKET** (default): district scene as the backdrop + a market table + HUD + action
  buttons (Travel / Bank / Loan / Buy / Sell).
- **CITY MAP**: the 6 districts laid out; click one to travel (costs a day).
- **EVENT** overlay: a card that slides in over the market.
- **GAME OVER**: settle-up + score + best.
- Tiny **TITLE** is fine; don't over-build it.

## Per-location visual variation (the priority — sprites + maps)
Each district is a distinct **tile `map()` backdrop** behind translucent UI panels.
**Budget-smart approach (work with the 64-slot sheet):** author ONE small shared tile
vocabulary (ground, building, window, door, accent, prop) and give each district its
own **identity via `pal()` recolor + 1–2 signature props**, rather than 6 full unique
tilesets. Six moods:
1. **Downtown** — glass towers, cool blues, suited NPCs (pricey).
2. **The Docks** — warehouses, crates, cranes, water tiles, grey/teal.
3. **Suburbs** — houses, lawns, picket fences, warm greens.
4. **The Blocks** — tower blocks, graffiti, dim purples.
5. **Chinatown** — market stalls, lanterns, neon pinks/reds (animated `blink` signs).
6. **Uptown Park** — trees, benches, bright greens.
Add **wandering NPC sprites** in each scene reusing the `crowd.c` idiom (a couple of
16×16/16×32 bodies, `pal()`-recolored per passer-by). A **day/night sky tint** that
sweeps as days pass (`fade`/`pal`) ties it together.

## Make it juicy
- **Money counter rolls** to its new value (`lerp`) with a coin/cash sound on every deal.
- **Price arrows**: green ▼ / red ▲ with a flash when a price moves a lot; blink a
  "HOT" tag on an extreme deal.
- **Event cards slide/ease in** with a sting; bad events (mugging/bust) `shake` the
  screen + red flash.
- **Travel transition**: quick fade / map-zoom between districts.
- **Cop bust**: siren = an instrument with an `LFO_PITCH` sweep; flashing red/blue.
- Per-district **ambient sound bed** (different scale/instrument/bpm feel per mood,
  via `every`/`euclid` + a filtered custom instrument) so each place sounds different.

## Data model (suggested)
- `static int price[6][6]` (district × substance), re-rolled daily from base × skew × noise.
- player: `cash, bank, debt, day, here (district), inv[6], capacity`.
- substances + districts as `const char* name[]` arrays; base prices + skews as tables.
- a small event enum + a `fire_event()` that mutates state and queues a card.

## Controls
Mouse-primary: click goods to buy/sell (e.g. left = buy 1 / shift or right = sell),
click a district on the map to travel, click Bank/Loan buttons. Keyboard fallback:
arrows to select a good, Z buy / X sell, TAB or M for map, ENTER confirm.

## Lean into / read
`mule.c` (economy loop), `papers.c` (panel/menu UI + day structure), `crowd.c`
(`pal()`-recolored NPCs + tile map), `effects.c`/`juice.c` (shake/flash/ease),
`omnichord.c`/`drummachine.c` (sound beds). Skip: real-time movement, combat,
free-roam — the district scenes are ambiance, not playable spaces.

## Notes / open choices
- Substances = the classic Drugwars set (weed/shrooms/speed/acid/coke/heroin). Could be
  abstracted/re-themed as contraband or spices to share an economy core with a future
  trading cart (e.g. Trade Winds).
- Slot strategy = shared-tiles-recolored-per-district (the engine-grain move); 6 bespoke
  tilesets would blow the 64-slot budget.
