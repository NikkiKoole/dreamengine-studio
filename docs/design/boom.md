# boom — the destruction petri dish (living wishlist)

`boom` (`tools/carts/boom.c`) is a destruction sandbox for a criminal city: a
**petri dish for blast effects** over two honest simulation layers. It's also a
**prototyping ground** — it knocked out sloop's deferred per-tile demolition
rung (`sloop.c:527`) early, and is where new destruction systems get proven
cheaply before they fold into the bigger world (see `big-game-backlog.md`).

> This file is the boom wishlist. Tick items off as they ship; record *why* a cut
> item was cut. Header `tools/carts/boom.c` is the source of truth for what EXISTS.

## The two honest layers (the invariant — don't cross them)

1. **HEAT** — a material grid (`Cell{mat,fuel,fire,hp}`): fire spreads cell-to-cell
   by probability (navkit's model flattened to 2D — resistance, wind bias, water
   chokes it, fuel burns to ash). Grass flashes, wood smoulders, oil/barrels/cars
   cook off into their own blasts.
2. **PRESSURE** — a blast wave: shatters glass, collapses concrete (`hp`), and
   knocks wall cells loose as physics blocks. Fire reacts to heat; walls react to
   pressure. **Keep these two causes distinct** — it's what makes the sim read.

Everything airborne (fireball, sparks, shrapnel, debris, smoke, wall blocks) is a
particle/body that cools along a blackbody ramp and settles under top-down friction.

## Shipped (2026-06-23)

- **Six detonation tools** (BOOM, keys 1-6): blast / grenade / molotov / car bomb /
  gas main (delayed upward eruption) / **ram car** (kamikaze vehicle that screams
  in from the edge and crashes into the wall at your click).
- **Reactive props**: barrels + cars cook off; glass shatters, concrete collapses
  under pressure.
- **Destructible walls → physics blocks**: a destroyed concrete/glass/wood cell
  detaches as a Block, flies, tumbles, settles back into rubble. (sloop demolition
  rung, prototyped.)
- Charge starts small (1); BUILD mode paints 10 materials.
- Demo clips: `tools/clips/boom/{01-blast,02-ramcar}.script`.

## Wishlist (next, roughly in order)

1. **Structural collapse** — *next up.* Walls fragment per-cell today; make a
   building *connected structure* so knocking out a base leaves the cells above
   **unsupported → they fall** (a flood-fill from a "ground/anchor" set; cells that
   lose their support path drop as blocks). A tower should come *down*, not just
   gain a hole. Stays in boom.
2. **Blasts shove loose objects** — *with #3 below.* The shockwave already pushes
   particles; extend it to push the Block bodies and cars on the ground, and make a
   blast-adjacent car launch as a body. "Set off a blast near loose stuff and
   watch it all get thrown." (First slice ships alongside the parked-cars work.)
3. **Feed demolition back into sloop** — the literal purpose of this prototype
   (`sloop.c:527`). Port the Block model onto sloop's per-cell mass/strength
   obstacles so a rammed/shot house sheds tiles. Closes the loop.
4. **City life — pedestrians** — NPCs that walk the block and flee/panic from fire
   and blasts. Turns the petri dish into a *scene*. Reuses the particle/body pool.
5. **Player presence** — make the ram car *drivable* (arrow keys): the bridge from
   sandbox toward the "roam-and-burn city game" boom's header promises. Pulls
   sloop's drive core toward boom.
6. **Counter-tools** — water/hose to fight fire, controllable wind — so it's build
   *and* destroy *and* save, a fuller sandbox.

## North star

boom's header calls it *"the sandbox seed for the roam-and-burn city game."* Every
item above is a step toward that — honest destruction systems first, the game
wrapped around them later. Companion backlog for the wider world:
[`big-game-backlog.md`](big-game-backlog.md).
