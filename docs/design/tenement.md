# tenement — several households, one building, and not enough of anything

STATUS: BUILDING (2026-08-12) — the THIN VERTICAL SLICE is in and the contract's centrepiece is
proven (`tools/carts/tenement.c`, `spec()` 21/21). No fan-out yet; see §10 for what the slice found.
Originally READY, designed, contract frozen. The renderer it
needs is done and measured ([`iso-rooms.md`](iso-rooms.md), SHIPPED). Contract:
`runtime/tenement/model.h`. Method: [ADR-0034](../decisions/0034-contract-first-parallel-authoring.md),
contract first, then fan out.

> **Working name.** `tenement` says the three things that matter (several households, one building,
> shared everything) and can be renamed freely before anyone writes code.

> **Homage:** The Sims (Maxis, 2000) for smart objects and the needs loop. Dwarf Fortress for work
> orders and storage. Lineage runs through this repo's `sims` (need decay, urgency sort, BFS to
> object), `dwarffort` (designate work, agents claim jobs) and `lockup` (the regime as the spine),
> and through the maker's sibling project **navkit**, whose production-system mistakes this design
> exists to not repeat (see §3).

---

## 1. The honest core, and the verb

Three need-decay sims already ship here. What separates them is not their mechanics, it is the
player's **verb**:

| cart | the player's verb |
|---|---|
| `dwarffort` | you designate **work** |
| `lockup` | you write the **timetable** |
| `sims` | you watch **one agent** |
| **`tenement`** | you shape **space**, and the simulation reports whether your space works |

That verb is chosen to fit the renderer. [`iso-rooms.md`](iso-rooms.md) built a machine for reading a
floor plan and the movement through it, so the game should be about floor plans and movement.

**The honest core: the building is a market for attention, and space is the scarce good.** Two rules
that interact:

1. **Smart objects, dumb agents.** Objects advertise what they offer. An agent takes the best offer.
   Intelligence lives in the furniture, not in the person. (`sims` does the opposite: it sorts needs
   by urgency, then finds the nearest object of the required type. The inversion is unclaimed here.)
2. **Contention.** More residents than objects. Queues form, corridors jam, and a badly planned
   building becomes visible as a **traffic pattern** rather than as a number.

**You never touch a person.** You build, you buy, you place. Every bad outcome is legibly your
fault. This follows `lockup` and `dwarffort` rather than The Sims, which lets you queue a sim's
actions, and it is what makes the verb unambiguously architectural.

**The soul, which no oracle checks** ([ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)):
four people failing to share one bathroom is funny. The Sims' charm was never the needs, it was the
small legible disasters. Design toward a building that generates tiny comedies a stranger can read
without a tooltip.

## 2. THE ONE PRINCIPLE

> **Nothing enumerates instances. Everything declares properties and matches on tags.**

This is the spine. It is also the same sentence as "smart objects", which is why the game gets it
for free rather than as a migration.

**One matching index, three consumers:**

| consumer | asks | of |
|---|---|---|
| a **need** | "who serves hunger, how well, at what cost?" | objects |
| a **work order** | "where can I do this, given what it requires?" | workspots |
| an **item** | "where can I be put down?" | storage |

An object declares one **offer set**. A fridge offers `serve_hunger` *and* `store_food`. A stove
offers `serve_hunger` slowly and `capability_heat`. Nothing needs to know that a fridge is a fridge.

**The payoff:** a new object, a new recipe, a new item or a whole expansion is a **table row**. Never
a new code path, never a new checkbox, never a new keyboard key.

**One argmax, not two levels.** This is the easiest part of the design to get subtly wrong, and the
first draft of the contract got it wrong. An agent does **not** pick its most urgent need and then
look for an object: that is urgency-sort, it is what `sims` already does, and it is the thing being
inverted. Instead there is a single argmax over every (object, need) pair, with the deficit as one
*term* in the score rather than a pre-filter:

    score = deficit(need) * offer.strength / (travel + queue_penalty)

The difference is observable, which is why it matters. Under urgency-sort a hungry resident walks
past a free toilet to reach a distant fridge. Here an adjacent nearly-free toilet can outbid the
fridge, which is the behaviour people recognise as Sims-like. And because every term is a number, the
choice is **oracle-able**: given this building and these needs, the agent MUST pick X. That is the
`spec()` this cart carries.

## 3. Why: three navkit mistakes, all the same mistake

The maker's sibling project `navkit` (a Dwarf Fortress-lineage colony sim) has this written down
already. Read as a set, the three failures are one failure.

**(a) Workshops enumerated PLACES and gave each one recipes.**
`navkit/docs/vision/workshops-and-jobs.md` defines nine dedicated workshops, each an ASCII template
with fixed tile roles, each owning its recipe list. `navkit/docs/todo/workshop-evolution-plan.md`
then catalogues the cracks: one work tile per workshop, one output tile, fixed template shape,
**workshop = recipe source**, max 5x5 footprint, one crafter. Which blocks power transmission, blocks
any recipe needing two independent things near each other, and blocks two people working in one
space. The maker's own trigger for abandoning it: *"when you're about to add a workshop that feels
wrong as a template."*

**(b) Stockpile filters enumerated ITEM TYPES and gave each one a checkbox.**
`navkit/docs/todo/11-stockpile-filter-redesign.md`: a flat array with one entry per item type, 30
entries, 34 of 36 available keyboard keys consumed, no grouping, and the observation that adding
curing states per item type "would explode it."

**(c) Containers were nearly a special entity.** `navkit/docs/done/feature-1b-containers-storage.md`
gets this one right and states the fix cleanly: *"Containers are just items with a CAPACITY property.
There is no separate 'container' entity."*

**The alternative navkit arrived at** (`navkit/docs/todo/discussions with ai/notdfworkshops.md`) is
the model adopted here: *make the tools the boss, not the workshop.* A workshop is "a collection of
tools near each other." Recipes declare required **capabilities**; the game scans for a cluster
satisfying them. Plus a tier ladder worth keeping in view:

1. **The Spot** — bare ground, hand-work only.
2. **The Furniture** — a table with tool slots.
3. **The Machine** — needs power, has built-in function.
4. **The Building** — many stations, the worker walks between them.

**What navkit says to protect, and this design protects:** the recipe table, the work-order
abstraction, the fetch → carry → work → output state machine, and material matching. The mistake was
never the recipes. It was binding them to a named building with fixed tiles.

**The gate, stolen outright from `navkit/docs/vision/autarky.md`:** nothing ships without a
**source**, a **sink**, and **feedback** into an existing loop. That discipline is what stops the
object catalogue ending as a 30-row table with eight `[OPEN LOOP]` markers at the bottom.

## 4. Work: visible labour at dumb machines

**Work happens in the world, in view, at a place.** A resident walks to a machine, stands at it for
a shift, and comes back with something. Nobody vanishes off-screen to earn money.

This is deliberately Tier 1 of the ladder above: a **dumb machine** converts *time* into a **good**,
with no input chain yet. That buys four things at once:

- Labour is **visible**, which is what the isometric view is for.
- The machine is just another **offer** (`capability_work`), so it is already on the one seam.
- It creates **contention**: one loom, four tenants, and a queue you can see.
- The **building layout starts to matter**, because where the workshop sits relative to the flats
  decides everyone's commute.

**Deepening it later changes no structure.** Give the machine an input requirement and it becomes
real production. Give it a power requirement and it becomes Tier 3. The work order, the state
machine and the offer index are unchanged.

## 5. Money, and the ONE place value enters

Money is a **per-household** quantity. Sources and sinks, nothing more.

**The honest problem, marked rather than fudged.** A machine that turns 8 hours into money from
nothing is an open loop by §3's gate: value appears from nowhere. It is *more* honest than off-lot
wages, because the labour and the place are both real, but it is the same hole. So:

> **There is exactly ONE external seam: goods are SOLD.** Machines produce goods; goods are sold;
> that sale is the only place money enters the world. It is labelled `TN_SEAM_EXTERNAL` in the
> contract so it can be found and, eventually, narrowed.

**The autarky trajectory, which is the point of the heavy seam.** `OFF_LOT` exists as a state, but it
is the degenerate case: work whose capabilities are not yet modelled inside the world. As
capabilities are added, work migrates **on-lot** and off-lot traffic shrinks. The seam does not
change shape, only how much goes through it. The success condition for the architecture is that
`OFF_LOT` can eventually be deleted without anything else moving.

**Sinks for money:** rent, bills, and buying objects. Which is where §8's open question lives.

## 6. Storage: tagged containers, owned by a household

In a building, storage is **furniture**, not floor zones. A fridge, a wardrobe, a cupboard, a shelf.
Each is an object with a **capacity** and an **accepted tag set**, so it falls out of the same offer
interface with no separate system: a fridge offers `serve_hunger` and `store_food`.

**v1 scope, held deliberately tight:** capacity, accepted tags, ownership. No nesting, no stack
sizes, no per-item filters, no reservation locking. The person carries the item to the store, never
the store to the item, because `navkit/docs/done/feature-1b-containers-storage.md` flags DF's
carry-the-container-to-the-item pattern as a source of locking and reservation problems.

**Storage is owned per household, and that is a feature.** Whose fridge is it? What happens when a
tenant with nothing to eat and a bottomed-out hunger need walks past a neighbour's full one? That is
emergent, legible without explanation, and exactly the small disaster §1 asks for. It also gives
money a reason to matter beyond bills.

## 7. The six seams

A **seam** is a shape decision that costs nothing today. **Speculative generality** is code you do
not need yet. The repo has form on the distinction: `hud()` was cut because a convenience for a
*look* makes every cart identical, and [ADR-0006](../decisions/0006-library-carts-not-engine.md)
keeps capabilities in cart-land rather than growing the engine. Every seam below is a *shape*, and
its cost today is a table or two fields.

| # | seam | shape | what it unlocks later |
|---|---|---|---|
| 1 | **needs are data** | `need[NEED_*]` over an enum + table, never `float hunger, energy;` | pet care, homework, hygiene variants: one table row each |
| 2 | **the offer interface** | objects declare offers; agents/orders/items match on tags | every future object and pack, with no new code path |
| 3 | **agents carry a species** | species + needs vector, and the matcher asks "does this serve *this* agent" | pets, toddlers, visitors |
| 4 | **`OFF_LOT` with a return time** | two fields on an agent | careers, school, holidays, hospital |
| 5 | **households are entities** | own purse, dwelling, members | a block is N households, not a special case |
| 6 | **a calendar, not a clock** | date, not a bare hour counter | seasons, weather, rent day |

**Explicitly refused:** no event bus, no plugin registry, no scripting layer "for the packs." That
shape eats projects and it fails ADR-0022's second bar, because a cart that is all seam and no core
delights nobody.

## 8. Open, on purpose

Two things are deliberately undecided. The contract must not foreclose either.

**(a) The win condition.** Money is currently friction, not a scoreboard. Rent collected, a block
fully tenanted and solvent, nobody evicted for a month, or nothing at all. Recording it as open
rather than missing: whether anything *scores* money is a layer on top of a per-household quantity,
addable whenever the answer is known.

**(b) What the reclaimed hours are for.** This is the sharp version of an observation the maker made,
that The Sims 1 and 2 accidentally become anti-consumerist, because you were happier striving than
arriving. The mechanism underneath it is specific: **objects buy back TIME.** A better stove feeds
you faster; a better bed rests you quicker. Money does not buy happiness, it buys *hours*. The late
game empties out because once needs are trivially met there is nothing for the reclaimed hours to go
into, and the striving was the only thing consuming them.

That turns a mood into a design question: **what are the hours for?** The Sims answered "careers and
aspirations", thinly. This setup has an answer a single-household game cannot reach: the hours can
only mean something **in relation to the other households**. A tenant whose needs are handled has
time, and time is the only thing that can be spent on someone else. That would give an
anti-consumerist reading that *emerges* rather than being preached.

Not a build item. Recorded so the economy leaves room for it.

## 9. What v1 is, and is not

**Is:** one floor, several households, tagged offers, needs decay, contention for objects and space,
dumb machines producing goods, goods sold, rent and bills, tagged containers owned per household,
fully hands-off.

**Is not:** multi-storey (the renderer defers it, and it changes both the depth sort and the wall-cut
rules), production chains with real inputs, pets, careers, relationships, a win condition. All of
those are table rows or layers on the seams above, and none of them is v1.

**Four or five needs, not nine.** `lockup` has nine and they blur. Needs that visibly compete for the
same objects and the same floor read better than needs that each own an appliance.

## 10. What the thin slice proved, and what it found

Built before any fan-out, on purpose: the contract's centrepiece had never been exercised by a line
of code, and ADR-0034's own ledger says the contract is the whole trick. Eight agents building
against a subtly wrong contract is the expensive failure mode.

**It was already subtly wrong, twice, and neither compiler nor linter could see it.**

1. **`tn_best_offer(agent, tag)` was urgency-sort in disguise**, found by tracing how the first
   module would call it. Requiring the caller to pick a need first *is* the thing this design claims
   to invert. Replaced by `tn_best_action()`, one argmax over every (object, need) pair.
2. **`minutes` was `unsigned char`**, so 480 wrapped to 224 and both an 8-hour shift and a night's
   sleep were silently impossible. Caught by *compiling* the contract rather than only writing it.

**What the slice proves (`spec()` 21/21).** Case 1 is the whole argument in one assertion: with
hunger the more urgent need but the fridge across the building, the near toilet wins, and with travel
equalised the hungrier need wins again. Both directions, so it cannot pass by accident. Case 2 shows
contention living in the *score* rather than in queue-handling code (an occupied capacity-1 object
stops attracting anyone; a half-full capacity-2 object still does). Case 4 pins the boundary that
lets one index serve three consumers: a loom and a wardrobe never bid for attention even at zero
needs, but both remain findable by tag.

**What it found by running, which is the part worth having.** Residents from household 1 walk across
the building and eat out of household 0's fridge, because `household` is not a term in the score.
§6 treats "whose fridge is it" as a *feature*, the engine of the comedy this game is supposed to
generate, so this is a real gap in the model rather than a bug in the code. It is now asserted in
`spec()` case 8 as **current** behaviour with a note to flip it, because a gap a test describes is a
work item and a gap in a comment is folklore.

**Still deliberately absent from the slice:** households as anything but a number, money, work
orders, storage, rent, building, and pathfinding (distance is straight-line, which is enough to prove
the decision mechanism but not enough to judge §1's claim that corridors jam). One file, so the
contract's per-module owner comments are unfulfilled. The iso projection is copied from `isoroom`;
extract `runtime/isoview.h` when a second consumer proves the shape, per
[ADR-0006](../decisions/0006-library-carts-not-engine.md).

## See also

- [`iso-rooms.md`](iso-rooms.md) — the renderer this is built on, its settled geometry, and the
  five things it cannot do
- [ADR-0034](../decisions/0034-contract-first-parallel-authoring.md) — contract first, then fan out;
  and the honest ledger of what a builder/critic loop does and does not catch
- [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) — the two-part bar; §1's "soul"
  paragraph is the half no oracle checks
- [`lockup.md`](lockup.md) — the closest sibling, and the verb to differentiate from
- [ADR-0006](../decisions/0006-library-carts-not-engine.md) — why the seams stay in cart-land
- navkit, for the reasoning behind §3 (sibling repo, not linkable):
  `docs/vision/workshops-and-jobs.md` · `docs/todo/workshop-evolution-plan.md` ·
  `docs/todo/discussions with ai/notdfworkshops.md` · `docs/vision/autarky.md` ·
  `docs/todo/11-stockpile-filter-redesign.md` · `docs/done/feature-1b-containers-storage.md`
