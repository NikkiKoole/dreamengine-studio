# sloop — a production/process-optimization subsystem

STATUS: SHIPPED — thecut.c ships the mix/quality subsystem (v2 adds managed workers); sloop-world integration is later.
Shot-list context: [`showreel-teaser.md`](showreel-teaser.md).

Research note. The goal: a subsystem for **sloop** (the GTA1-style, infinite-world,
street-level game where you're a person who makes money from products you sell) that
"tickles the optimize-a-process-for-best-quality/most-output thing" — the Factorio /
Schedule 1 / Farming Sim / Free Enterprise itch.

> This note designs the **mix half** (the recipe/quality number, shipped as `thecut.c`). Its
> companion [`sloop-handwork.md`](sloop-handwork.md) designs the **embodied half** thecut
> skipped — the CDDA body-in-the-world handling loop (gather → carry → work-that-takes-time →
> load a machine and walk away → grow/wait), where a **machine is your first delegate** (one
> pure transform, three front-ends: you / machine / follower).

## The core arc: "first you do it yourself, then you manage"

The organizing principle of the whole subsystem. Execution → optimization → management:

- **Hand-operating each step first IS the tutorial**, hidden in the fantasy. You learn what
  "good" looks like by doing it, so you can later judge/manage a worker doing it. *You can't
  manage what you've never done* (very CDDA/DF — understand the craft before you hire it out).
- **The fun migrates as you scale:** early = execution (intimate, slow, high control), mid =
  optimization (tuning recipes/ratios), late = management (assignment, portfolio, risk). One
  subsystem carries all three phases.
- **Implementation:** every station is *hand-operable first, delegable second* — ONE pure-
  function model, TWO front-ends (you drive it / a worker drives it). A worker is "you, but at
  their skill level, no focus bonus." This is why the pure-function rule (below) is load-bearing.

### How DIY translates to manage (the bridge)

Both modes run the *same job* — one pure function `produce(recipe, material, operator,
equipment, focus)`. DIY and manage only fill the arguments differently:

| | Do it yourself | Manage |
|---|---|---|
| `operator` | you (your skill) | the assigned worker (their skill) |
| `focus` bonus | yes — earned by being present / nailing the hands-on bit | none (or a little, from worker morale/diligence) |
| the decision (recipe + materials) | made live, each time | a **standing order** set once |
| what it costs you | your **attention** (standing there, not out in the world) | **money** (wage) + **exposure** (heat/risk) |
| you act when… | every step | only on **exceptions** (ran dry, worker problem, demand shift) |

So "manage" is not a different system — it's the *configuration* of the job you used to do by
hand, plus exception-handling. Attention shifts from **per-action** → **per-policy + per-exception**.

- **The translation moment:** the recipe you discovered by hand becomes a **standing-order card
  you hand off** — *"I've made this enough times to write it down / show someone."* A hired (or
  watching, CDDA-style) worker can then run it. Ties back to "you can't manage what you haven't done."
- **What keeps manage from being fire-and-forget:** the permanent **quality gap** — a worker runs
  at *their* skill, no focus bonus, so worse than you at your best. Every session you choose: do
  premium orders yourself, train the worker up, or eat the quality hit to free your attention.
- **Worked micro-example (The Bench):** DIY — stand at the bench, pick base + additive sequence,
  nail a timing for a purity bonus → one high-quality batch. Translate — after a few makes it's a
  saved order; hire someone, assign them, point them at a materials crate. Manage — they churn
  batches at their skill while you drive off to sell; ledger on return: *"42 batches, avg quality
  6.1 (your hand-made avg 7.8), 1 spoiled, worker wants a raise"* → train / replace / hand-make
  the premium runs yourself.

- **Unifying lens:** the real currency you optimize is **your own attention** — one body, one
  place at a time. Management is converting *money → attention* (hire a body) and *attention →
  quality* (be present for the bonus). The whole DIY→manage arc is one attention-allocation problem.

## Build staging (v1 → v3)

The whole reason v1 is pure-function-first is that delegation becomes a *small addition to the
same cart*, not a new game. **Management is not its own cart** — a worker is literally
`brew_eval(seq, n, worker_skill, /*focus*/0)`; the bench, reagents, demand math and synergy rules
already exist and don't change. Cloning them into a second cart would duplicate everything and,
worse, break the arc — "first you do it yourself, then you manage" only lands if it's the *same
bench* you graduate from.

- **v1 — `tools/carts/thecut.c` ("The Cut"). BUILT.** The mix loop in isolation. Pure-function
  core `brew_eval(seq, n, skill, focus)` computes the whole product headless. Three legible numbers
  (quality/strength/effect) move live as you build an ordered reagent sequence; adjacency synergy/
  clash + the SOLVENT-cleans-the-next-thing rule make order matter; a hand STIR is the DIY focus
  bonus; a pizzatycoon-style demand curve matches the batch to clients; skill levels up with
  crafting and unlocks recipe slots. Isolates *"is the mix loop satisfying."*
- **v2 — extend `thecut` IN PLACE (the manage half). BUILT.** Hire from a candidate pool (skill +
  wage + hire fee); hand the worker a **standing order** (the current bench recipe, copied to a card
  they follow); a **resolve-on-return** time-skip (1–7 days) runs the SAME `brew_eval` at *their*
  skill with `focus=0`, auto-sells each day, and returns a ledger (batches, spoilage, avg quality vs
  your hand-made, gross − wages = net); morale decays with the grind → "wants a raise" (give one or
  FIRE); workers learn by doing (skill creeps up). Confirms the manage loop is satisfying — and
  surfaced a nice wrinkle: a skilled hire **out-classes your novice hands early** (the quality gap
  *inverts* until your skill+stir overtakes theirs), so the arc is "hire up while learning → surpass
  them as a master." The counterweights (hire fee, wage, spoilage, morale/raises) keep it from being
  free money. Still a bench cart, no world.
- **v3 — sloop integration (NOT a cart).** The fullest "systems run while I drive the infinite
  world doing other things," plus heat/risk/busts. The bench becomes a rentable room, selling
  becomes driving to clients, "do other things" becomes the actual game. Stops being a prototype
  and becomes the subsystem.

## What actually creates the optimization itch

Theme is irrelevant; the itch comes from a five-part structure shared across all of them:

1. **Legible transform with a visible number.** inputs → process → output, and ONE score
   (quality / output-per-min / profit) that moves the *instant* a knob is tweaked. Laggy or
   opaque feedback kills it. Non-negotiable.
2. **Non-linear interactions** so there's no obvious optimum: synergies, thresholds,
   diminishing returns, order-dependence. "Best = max everything" = no puzzle.
3. **A binding tradeoff.** Quality costs throughput, throughput costs quality, both cost
   money. You optimize *under a constraint*. (Factorio: quality modules slow the machine.
   Schedule 1: every added ingredient costs more.)
4. **A ratchet.** Keep meta-progress (recipes, equipment tiers, better base) so optimization
   compounds across runs — roguelike × tycoon.
5. **Discovery as content.** Finding the broken combo yourself is the joy (Schedule 1). Hide
   the math a little; a recipe book fills in as you experiment.

## Genre leanings

| Game | Optimization is about | Flavour |
|---|---|---|
| Factorio (Space Age) | throughput + ratios, killing bottlenecks; quality = probabilistic upgrade gamble (1–2.5%/tier per quality module, 5 tiers up to +150% stats, modules slow the machine) | systems that run themselves |
| Schedule 1 | combinatorial recipe discovery; base + additives one at a time, re-feed to stack; additives add traits/effects; input quality gates output quality; rare effects → big prices | emergent, hand-operated |
| Farming Sim | scheduling + machine utilization over a time cycle | rhythm + logistics |
| Free Enterprise / tycoon | market-facing: price, demand, competition, supply chain | reading the market |
| Cataclysm: DDA | **skill-gated crafting**: your fabrication/cooking/mechanics skill gates the success roll AND the outcome; recipes *learned* from books/disassembly/practice; component & tool *quality* requirements | deep, character-driven |
| Dwarf Fortress | **maker-skill quality tiers** (`-well-crafted-` → ☼masterwork☼) = f(skill, material, luck); skill rises with repetition (Legendary craftsdwarf); production produces *stories* | emergent, opaque |

### What CDDA + DF add beyond Factorio/Schedule 1

- **Skill-gated quality = a THIRD ratchet.** Distinct from "better recipe" and "better
  equipment": *the operator gets better with practice*. Output quality = f(player skill,
  material, recipe, luck). For sloop this is ideal — **you, the person, level a craft.**
- **Material/component quality propagates** through the chain (deeper than Schedule 1's
  input-gating): the specific material keeps its identity end to end.
- **Recipe knowledge is LEARNED**, not just bought/unlocked — from books, disassembly,
  watching, repetition. Reinforces *discovery as content*.
- **The sim produces stories, not just numbers** (DF) — emergent anecdotes. "Prefer emergent
  behavior" taken to its conclusion.
- **CAUTION: DF/CDDA trade legibility for depth** (dense menus, hidden math) — the *opposite*
  of principle #1. Take their depth ideas (skill, material propagation, learned recipes,
  emergent outcomes) but keep Factorio/Schedule-1's legible, immediately-visible feedback.
  Do NOT inherit the opacity in a cart.

## Why Schedule 1's model is the right spine for sloop

Sloop is street-level (you're a person, not a factory). So Factorio's throughput is a *depth
/ upgrade* layer, not the core. Schedule 1's mixing model is the core because it's: cart-sized,
**emergent/combinatorial** (matches "prefer emergent behavior" + "simple interacting rules"),
and it outputs *a product with quality + traits that plugs straight into a sell/demand loop we
already have*.

### Repo assets to reuse (most of it already exists)
- `tools/carts/pizzatycoon.c` — `sim_shop(d, price, mask)`: recipe-as-bitmask + appeal→fair-price
  →demand curve vs per-district taste. This *is* the mixing+demand math.
- `tools/carts/druglord.c` / `merchant.c` / `tradewinds.c` — `FAC[][]` per-district price flavour,
  daily re-roll, event cards, bank/heat, the "run" structure + sell loop.
- `tools/carts/modrack.c` — enum-indexed `param[]` registry → store additive/trait vectors without
  cross-wiring bugs (name the trait indices).
- `docs/design/streetlevel-content.md` selector + atoms (pure-fn-of-cell) → spawn buyers/production
  sites deterministically across the infinite world.
- `save_bytes` → the meta-ratchet.

## Delegation / automation — "systems run on their own while I do other things"

A core eventual goal: offload work to workers so production runs without the player present.
This has one architectural consequence that must be honoured from the FIRST prototype:

1. **The production transform must be a PURE FUNCTION, not a real-time minigame.** A delegated
   worker can't play your timing/heat minigame. So:
   `quality + traits = f(recipe, material, operator_skill, equipment, luck)` — fully evaluable
   headless. The player's hands-on involvement is an **optional bonus when present** (your
   higher skill + a focus bonus) vs **delegate** (runs at the worker's skill, frees you). That
   "do-it-yourself quality vs delegated throughput" is the binding tradeoff applied to
   delegation. **Do NOT make a hands-on minigame load-bearing for quality** — it forecloses
   delegation.
2. **Workers add two axes on top of the three ratchets:** *skill* (gates quality CDDA/DF-style;
   trains up with repetition) and *risk* (wage/upkeep, reliability, and — crime theme — can
   skim / get busted / snitch → **heat**). Scaling up your operation raises exposure. The risk
   axis is what stops "automate everything → win" from being solved.
3. **Offline sim = resolve-on-return, NOT continuous tick.** Don't simulate an empty room at
   60fps. The repo's deterministic pure-fn ethos means production over `[t0, t1]` is computable
   in closed form on return → you get output + a log of what happened (ran dry at hour 3, worker
   spooked, …). The report UI already exists: pizzatycoon/druglord's daily ledger.
4. **The autonomy ramp is the meta-game:** by-hand every step → hire out the boring step → the
   line runs without you → manage the managers. Each rung buys back player attention to spend
   on sloop's world (driving, selling, exploring).

## Three cart experiments (prototype A first)

**A — "The Bench" (mixing/quality, emergent).** Base good + a *sequence* of additives. Each
additive = trait vector `{potency, purity, effect_flag, cost}`. Output = base folded through the
sequence; **order matters**, certain pairs synergize/clash (the non-linearity). Quality = input
quality × process discipline (small timing/heat minigame, or pure synergy math). Demand layer
(lift pizzatycoon's formula): each buyer/district wants a trait profile → price = match. Itch:
hunt the combo; recipe book fills in. Nothing hardcoded as "best" — best emerges per seed from
trait math × buyer prefs. **Least new code; the piece sloop is missing. Build this first.**

**B — "The Line" (Factorio-lite throughput).** Fixed 3–4-stage chain, each stage a machine with
speed+quality knobs fed by a buffer; allocate a scarce resource (power/workers/cash) across them.
Fast→slow wastes; slow front starves the back. Score = rate × quality × price. The back-room
upgrade layer (more menu than world).

**C — the actual subsystem (hybrid of A+B).** Rent a room in the world. Recipe sets quality+traits
(A), equipment/throughput sets volume (B), street selling converts to money under demand + heat
(existing druglord loop). A "run" = days before heat forces a reset; keep meta-unlocks. The
couplings — quality→price, traits→which buyers, heat→access — are what make it a *subsystem*, not
two games stapled together.

## Traps
- **Spreadsheet fatigue** — cap at ~3–5 meaningful knobs on screen at once.
- **Solved meta** — one dominant recipe forever kills the itch post-discovery. Counter: per-seed
  buyer prefs, rotating/escalating demand.
- **Disconnected minigame** — if the bench doesn't feed driving+selling, it's a side toy. Keep
  the couplings.

## Sources
- Schedule 1 mixing: https://schedule-1.fandom.com/wiki/Mixing ,
  https://www.pcgamer.com/games/sim/best-schedule-1-mixing-recipes-guide/
- Factorio quality: https://wiki.factorio.com/Quality , https://factorio.com/blog/post/fff-375
