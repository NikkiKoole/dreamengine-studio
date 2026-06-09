# Oersoep — design notes

*Oersoep* (Dutch: "primordial soup") is the **cell stage of Spore (2008)** on dreamengine.
Swim a microbe through a pond, eat to earn DNA, evolve new body parts in a between-rounds
editor, and climb the food chain from prey to apex before crawling ashore.

## Why it exists (and isn't rollswarm)

Three carts already circle this territory:

- **rollswarm** — Katamari × boids: roll a ball, swallow critters *smaller* than you, hit a
  size goal before a timer. **No danger** — nothing eats *you*.
- **swarmturf** — Splatoon × boids: steer a flock that paints turf. Not predator/prey.
- **vampire** — Survivors: auto-fire, XP gems, pick-an-upgrade on level-up.

`rollswarm` already nails *"eat things smaller than you, grow by area, flee-boids."* Reskinning
it green ships rollswarm twice. Oersoep's identity is the two things rollswarm deliberately omits:

1. **Bidirectional predation.** Cells bigger than your mouth *hunt you*; a bite costs health and
   knocks you back. Health to zero = death. This is the agar.io knife-edge rollswarm has no
   version of.
2. **You redesign your body.** Eating fills a DNA meter; at the cap you mate and drop into a
   **cell editor** to spend DNA on parts that change *how you play* — flagellum (speed), spike
   (turn the tables on rammers), jaw (open new diet), poison (deterrent aura).

Reused from `rollswarm.c`: the boid flee loop, area-growth `fsqrt`, follow-camera, HUD,
over/replay. From `vampire.c`: the level-up pick-a-card screen. From `juice.c`: the
eat/bite/death feedback stack.

## Loop

```
choose mouth (herbivore / carnivore)
  → swim toward cursor → eat diet-matching food + smaller cells → DNA fills
  → CALL MATE: a same-species cell appears, swim to it
  → CELL EDITOR: spend DNA on one part → tier up, pond restocks bigger
  → repeat (3 evolutions) → final tier, fill DNA once more → CRAWL ASHORE (win)
  anytime: a bigger mouth bites you → health drains → death → instant retry
```

## Key rules

- **Steering:** swim toward the cursor with momentum + drag (agar.io feel), wiggling flagellum
  tail. `camera()`/`follow()` set in `update()` before reading `mouse_world_*` (sticky-state
  gotcha). Cursor-only — the cell always swims toward the pointer.
- **Diet gate:** herbivore eats plant flecks only; carnivore eats meat gobbets + smaller cells;
  the **jaw** part promotes you to omnivore (eat everything). Predation on *you* is
  diet-independent — big mouths eat anyone.
- **Predator / prey** is recomputed every frame against your current radius, so cells that
  hunted you become edible once you outgrow them. Smaller cells flee (rollswarm boid); bigger
  cells hunt; a middle band is neutral (bumps, neither).
- **Spike** flips a ramming predator: it recoils and *shrinks* instead of biting you. **Poison**
  shrinks + repels any predator inside the aura.
- **Tiers:** 4. Each evolution raises the radius cap and max health and restocks the pond with
  bigger average cells, so the pressure scales like vampire's spawn curve.

## Palette

Water `CLR_DARKER_BLUE` / `CLR_DARK_BLUE`; player membrane `CLR_BLUE_GREEN` rim `CLR_TRUE_BLUE`
nucleus `CLR_INDIGO`; plant `CLR_LIME_GREEN`, meat `CLR_DARK_RED`; prey cells green-family,
predator cells red-family (flash white on lunge); spike `CLR_LIGHT_GREY`, poison aura dithered
`CLR_LIME_GREEN`.

## Files

- `tools/carts/oersoep.c` — all primitives, no `.cart.js` (like rollswarm).
- Registered in `index.json` as `kind:["game"]`, `genre:"arcade"`, `homage:"Spore (2008)"`.
</content>
</invoke>
