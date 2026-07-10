# The game you're building: PIZZA TYCOON

## Premise
A light business sim with a shady underbelly. Buy shops across a city, set your pizza
recipes and prices, watch customers (and profits) roll in or stay away, and take the odd
**mafia job** for quick cash and favors. Out-earn your rival to dominate the city by the
deadline. Menu + map driven, characterful, juicy.

## Slice (locked)
**Districts you buy + recipe/price + a daily revenue sim + mafia job event cards + one
rival.** Win = highest cash / most shops owned by day N. 🔴 Trim the full management
sprawl: no staff micro, no decoration mini-game — focus the buy→tune→earn→sabotage loop.
Shares an economy backbone with `druglord.md` / [`trade-winds.md`](trade-winds.md).

## Core mechanics
- **City map:** ~6 districts (varying rent, foot traffic, taste). Buy a shop in one;
  each owned shop is a node you manage.
- **Recipe & price:** pick toppings (cost vs appeal) and a price; **appeal vs price vs
  district taste** drives daily customers → revenue. A simple, legible formula.
- **Daily sim:** advance a day → each shop computes customers/income/costs; show a tidy
  ledger. Random demand swings via `noise`.
- **Mafia jobs (the flavor):** event cards offer risky gigs (deliver a "package",
  sabotage the rival, protection) for cash/favors — with a chance of heat/penalty.
- **Rival:** a CPU magnate buying shops and undercutting you; race for market share.

## Sprites & maps
City = tile `map()` of districts (recolored per district via `pal()`, `crowd.c`-style
NPC foot traffic for life). Shop interiors/exteriors as small sprite scenes; pizza/
topping icons; ledger + menus as `fillp` panels. A map screen with owned-shop pins.

## Juice
Cash-register ka-ching + money rollup (`lerp`), customers ticking in as little sprites,
a star-rating popup when a recipe sings, mafia card slide-in with a tense sting, rival
"undercut!" alert, day/night tint sweep, end-of-day ledger tally fanfare. Audio: jaunty
Italian-café bed, register chime, ominous mafia motif, rival sting.

## Data model
`struct Shop { int district, price; int recipe[]; float rep; }` list; `player` cash +
shops + heat; `rival` cash + shops; district tables (rent/traffic/taste); event-card
enum + resolver. Persist best run with `save`/`save_bytes`.

## Controls
Mouse-primary: click a district/shop to manage, click toppings/price arrows, click
"open shop"/"next day", resolve mafia cards by clicking choices. Keyboard fallback.

## Lean into / read
`mule.c` (economy/turn sim), `papers.c` (panel UI + day cadence), `sims.c`/`city.c`
(district/agent life), `fourx.c` (map + rival), `crowd.c` (pal NPCs), `effects.c`
(money/flash juice). Skip: staff/HR, building decoration, real-time service — it's a
tune-and-tally tycoon.
