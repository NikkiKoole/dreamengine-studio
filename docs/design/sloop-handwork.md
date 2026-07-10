# sloop — handwork: the embodied production loop

STATUS: BUILDING — handwork.c probes the hand-vs-machine production loop; v1 verified headless, awaiting sloop-v3 integration.
Shot-list context: [`showreel-teaser.md`](showreel-teaser.md).

Research note. Companion to [`sloop-production.md`](sloop-production.md). That doc designed
the **mix half** of sloop's production subsystem — `brew_eval(seq, n, skill, focus)`, a pure
function, a quality number, a demand curve — and shipped it as `tools/carts/thecut.c`. It is
all *menu*: no body, no space, no time you feel in your hands.

This note designs the **other half** thecut deliberately skipped: the **embodied, spatial,
temporal handling loop**. The CDDA body-in-the-world. Get grondstoffen → pick them up → into a
container → carry to a table → do work that *takes real time* → make something → put it away →
load a machine and walk away → (later) grow plants, water daily, wait days. It is **navkit**'s
item / work-tile / passive-timer model **minus the colony AI, plus one walking avatar** —
because sloop is street-level: you ARE the person, one body, one place at a time. (navkit = the
sibling DF-clone at `~/Projects/navkit`; the synth came from there too.)

The two halves meet at sloop v3. This is the missing one.

## The load-bearing insight: a machine is your first delegate

sloop-production.md's delegation rule is "one pure function, two front-ends: you drive it / a
worker drives it." A worker is just `produce(recipe, material, skill, /*focus*/0)` — the same
job, run worse, but it frees your attention.

**A machine is exactly that, minus the wage and the AI.** When you drop plants into a trimmer
that cuts them for you, you are running the same transform at fixed quality with no focus bonus,
and you walk away. A machine is a delegate made of metal. So the **embodied-vs-light** axis
(your hands vs. a machine) is *the same axis* as the **DIY-vs-manage** arc — which means we can
prototype delegation **now, with zero follower code**:

> **One pure transform, three front-ends:**
>
> | front-end | quality | what it costs you | unlocked |
> |---|---|---|---|
> | **you, hands-on** | top — your skill **+ focus bonus** (presence) | your **attention** (locked in place, not in the world) | v1 |
> | **a machine** | fixed, lower — your skill, **no focus** | **power / upkeep** + the buy | v1 |
> | **a follower** | their skill (grows), no focus | **wage + risk** (skim, bust, snitch → heat) | phase 2 |

The follower slots into the front-end the machine already defined. Nothing gets rewritten — the
navkit lesson (*protect the job pipeline, grow the front-ends*) applied before a single line of
colonist AI exists. The machine teaches the player the delegation tradeoff in its purest form:
**hand-make the premium batch (quality, costs your time-in-the-world) or feed the machine and go
drive/sell (throughput, costs quality).** That live choice — one avatar, one machine — *is* the
game.

## What carries over from navkit (the embodied mechanics, de-AI'd)

navkit is god-view (you command colonists), but its production internals are exactly what a
single-avatar embodied loop needs. The reusable ideas, and the scars:

- **Items live in four states** — on ground / in a container / **carried** / reserved. The
  carried-state is the whole embodied loop in miniature. Keep it.
- **Active vs. passive work is ONE recipe flag.** Active = an operator is committed at the work
  tile, progress ticks only while present. Passive = a timer counts down with nobody there
  (drying rack, charcoal pit). *Same struct.* This is the cleanest idea in navkit and it maps
  1:1 onto our hands-vs-machine and onto grow-and-wait. **Adopt it literally:** a job is
  `{ transform, input, output, work_total, mode: ACTIVE|PASSIVE, operator }`.
- **Self-fetch makes layout matter.** navkit crafters walk to fetch their own inputs, so
  stockpile *placement* is strategy. Our avatar and our machines do the same — a far input pile
  is a long walk. This is what turns "a bit of stockpiling business" into actual decisions.
- **Containers were deferred and it bit them** — the farming/cooking loop stalled waiting for "a
  basket holds N." We make containers **first-class from v1**. navkit's hindsight endorses this.
- **Autarky:** never add an output with no sink. No dead-end items. ("why do I have 500 ash?")
- **The phase transition is real, not confusion.** navkit's owner wrote: *"I have quite a few
  dwarf fortress workshops but I'm slowly thinking I want them more like cataclysm constructions,
  more freeform… at the same time we need to give jobs in a colony game so I might just be
  confused."* The resolution: it's not confusion, it's **manual/freeform first → managed/
  structured later** — the same arc as ours.

## Decisions locked for v1

- **Quality model.** Reuse the *shape* of thecut's `brew_eval` — `quality = f(operator_skill,
  focus)` — but drop the reagent-sequence math entirely (this cart is not about the mix). The
  embodied skill is **trim precision**: at the bench you make clean cuts (a small presence/timing
  act) → `focus` bonus → higher quality. A machine runs the identical transform with `focus = 0`.
  Keeping the *shape* identical to thecut means the delegation seam is literally the same seam.
- **Two lanes, not one number.** Output splits into **FINE** (quality ≥ threshold) and **COARSE**.
  Your *timed hands* can hit FINE; the **machine stamps a fixed COARSE quality and can never reach
  FINE** — so the machine permanently cannot replace your hands for the premium lane. That fixed
  ceiling is load-bearing: it's what stops the machine from strictly dominating (see below).
- **The sink is a buyer at the door, not a stash** (added in the same v1, after the bench/machine
  A/B was felt — see "The opportunity cost"). A premium buyer wants a few FINE flowers and pays by
  quality; a bulk buyer wants a cheap pile of anything. Cash is the legible score
  (principle #1). Full street-selling/demand stays `thecut`'s job; this is the minimal on-ramp.

## The opportunity cost — why the machine is a relief, not a "win"

The naive loop — *grind handwork → save money → buy machine → automate → repeat* — is **boring**,
and for a precise reason: the machine ends up strictly dominating, so it **discards the embodied
verb we just built**. A loop whose reward is "you stop playing the good part" is self-defeating.
Money as a single gate makes the optimum known from minute one. Two rules keep this cart out of
that trap:

1. **The machine opens a *second lane*, it never closes the first.** Hands = FINE (premium lane);
   machine = COARSE (bulk lane); demand has **both**. You run a *portfolio* forever — hand-make the
   premium order, let the machine churn the bulk. "Getting a machine" is a phase *change* (you open
   the bulk business), not an *end*. The fixed COARSE ceiling above guarantees the premium lane
   stays hand-only.
2. **The reason to delegate is "I can't be in two places," not "I can finally afford it."** Buyers
   arrive at the door and **wait, then leave**. Standing locked at the bench *costs you a sale* —
   one body, one place (the attention currency from sloop-production.md, made physical). The machine
   earns its keep by trimming the bulk *while your hands chase the premium order*. That converts
   *money → attention* (the doc's unifying lens) **before** sloop's world even exists to spend the
   freed attention on. It's the pull that makes "I need to delegate" a feeling, not a spreadsheet.

## v1 — the vertical slice (`tools/carts/handwork.c`)

One cart that **grows in place** (sloop-production.md's rule: extend, never clone — the arc only
lands if it's the same cart you graduate from). v1 is the smallest slice that proves the
hand-vs-machine A/B is satisfying:

1. **One avatar**, walk a room (`btn` movement).
2. **A plant source** — press near it to harvest → raw stalks (an item).
3. **A carried container** — pick up a basket; haul N stalks per trip instead of 1. Core, cheap,
   in from the start.
4. **The bench (embodied / ACTIVE)** — feed stalks, **TIME each cut** on a bouncing meter; you are
   **locked there**. A centred (well-timed) cut → a **FINE** flower; a sloppy one → COARSE.
5. **The trimmer (light / PASSIVE / the automaton)** — load stalks, it ticks on its own and stamps
   a **fixed COARSE quality**; walk away, come back to finished flowers. Never reaches FINE.
6. **A door + buyers** — premium (wants FINE, pays by quality) and bulk (wants any, flat rate)
   arrive on a clock and **leave if not served in time**. Cash is the score.
7. **A felt clock** so "this took time" reads on screen.

The v1 experiment is the A/B you feel in your hands — **same flowers two ways** — *under the
opportunity cost*: a buyer is at the door ticking down while you're locked at the bench. Which
you reach for depends on whether the order needs quality (your hands) or your time back (the
machine). If that tension lands, the missing half of sloop is proven.

> **Verified headless** (the four mechanics + the market clock all drive correctly under
> `tools/play.js`): harvest with a regrow gate, timed hand-cuts producing FINE, the passive
> machine producing COARSE, buyer spawn/patience-expiry, and serving for cash. The build also
> caught a real bug — `max(0, float)` truncating a timer every frame, since this engine's
> `max`/`min` are *integer* functions (use `clamp` for floats).

## Far sight — what grows in the same cart, in order

The point of building v1 pure-function-first and front-end-shaped is that each of these is a
*small addition to the same cart*, not a new game:

1. **Stockpiles / drop-zones** (next after the basket). A tray the bench/machine pulls from and
   dumps into → *layout becomes strategy* (navkit's "put the input pile next to the workshop").
   This is the "bit of stockpiling business," kept small: a few placed zones, filtered by item.
2. **Growing + watering + a day cycle.** The slow PASSIVE process on a calendar: plant a seed,
   water it each day or growth stalls, harvest when ripe over a few days. Needs the felt clock to
   roll into **days** and a **leave-and-return resolution** (compute what happened over `[t0,t1]`
   in closed form — never tick an empty room; sloop-production.md §"Offline sim"). This is the
   first real test of resolve-on-return, the spine of phase-2 delegation.
3. **Processing chains.** trim → dry → press, where **each step is independently hand-or-machine.**
   This is where automaton *planning* gets deep: which steps do you keep by hand (quality) and
   which do you mechanize (attention)? The chain is the playground for the core tradeoff.
4. **Followers (phase 2).** Drop into the machine's front-end: a follower is the machine with a
   skill that grows, a wage, and risk (skim / bust / snitch → heat). Resolve-on-return already
   exists from step 2. This is where the cart stops being "do it yourself" and becomes "manage."
5. **Buy machines / machine tiers** (now that cash exists from the door). The economic gate, but
   kept honest by the rules above: a machine never reaches FINE, so buying one *opens the bulk
   business*, it doesn't end the game. Higher tiers raise the fixed COARSE ceiling, add hopper
   size or speed — never a free quality win. The *reason* to buy stays "I can't be in two places."
6. **Richer demand.** Grow the door's two buyer kinds toward `thecut`'s demand model
   (`sim_shop`-style appeal→price vs. per-district taste); a queue of waiting buyers; reputation.
   Do **not** rebuild thecut — wire to it. With sloop v3 this becomes driving the world to sell.

**End state:** the cart is the embodied half of sloop's production subsystem — you gather, carry,
craft-or-mechanize, grow-and-wait, then delegate to followers, then drive off and resolve on
return. The same pure `produce()` runs throughout, driven by you, then a machine, then a worker.
That single transform under three front-ends is the whole "first you do it yourself, then you
manage" arc, made physical.

## Traps (inherited + new)

- **Don't let the bench minigame become load-bearing for quality.** The precision act is an
  *optional focus bonus when present*, never a gate — or you foreclose delegation
  (sloop-production.md §"Delegation"). A machine/follower must be able to run the job headless.
- **Resolve-on-return, not continuous tick** for anything that runs while you're away (machines,
  growth, later followers). Closed-form over the interval; the ledger UI is the report.
- **Keep it legible** — CDDA/DF depth, Factorio/Schedule-1 legibility. ≤3–5 meaningful things on
  screen at once. A visible quality number per output is non-negotiable.
- **Autarky** — every item gets a source AND a sink before it ships. Defer features whose output
  has nowhere to go (navkit's discipline).
- **Same cart, not clones** — the manage arc only lands if it's the bench you graduated from.

## See also
- [`sloop-production.md`](sloop-production.md) — the mix half (`thecut.c`), the DIY→manage arc,
  the five-part optimization itch, the pure-function delegation rule.
- navkit (`~/Projects/navkit`) — the DF-clone the embodied mechanics are de-AI'd from:
  `src/simulation`, `src/world`, `docs/done/{workshops,jobs}`, `docs/vision/entropy-and-simulation`.
