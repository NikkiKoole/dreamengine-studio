# Physics — design exploration (a tiny in-engine physics layer?)

> **Status: BUILDING — Layer 0 (`runtime/physics.h`) SHIPPED 2026-07-13; Layer 1 (the
> managed-world / rigid-body seam) still a sketch, nothing decided.** Began as a brainstorm
> sparked by the ragdoll cart (`tools/carts/ragdoll.c`) feeling good and prompting the
> question: should dreamengine roll a tiny physics layer into the runtime — "a bit like a
> simple Box2D"? The verlet toolkit half is now real (used by `verlet`/`ragdoll`/`linerider`/
> `coaster`/`jelly`/`sloop`/`tentacle`/`waterjar`); the deeper rigid-body route is still just the sketch below.

---

## The fork we've already navigated once: code-first vs black-box

[`0003-code-first-sound`](../decisions/0003-code-first-sound.md) said: *"Making music is
programming — which fits the learn-to-code mission."* We deliberately chose a code-driven
synth over a tracker UI. **Physics hits the identical fork.**

A `world_step()` that silently moves everything is the *tracker* of physics — convenient,
but it hides the thing worth learning. The ragdoll cart being hand-written verlet is, by
our own philosophy, a *feature*: the learner can read every line of why the little guy
falls over.

So the honest first question isn't "Box2D or not" — it's **"is physics something the
runtime does for you, or something you compose?"** Our track record leans **code-first**.
That doesn't kill the idea — it shapes it (see the middle path below).

Relevant open VISION question: *"How much to keep adding to the API vs. teaching learners
to write their own helpers"* — and the still-open *"whether to allow `#include` of other
files."* Physics is a strong test case for both.

---

## The technical fork: rigid bodies vs particles

There are genuinely two different worlds here, and they *feel* different.

### Rigid bodies (the Box2D family)
Objects have mass, velocity, **rotation**, and inertia. Collisions generate contact
manifolds, solved with sequential impulses. This is what you need for **stacking crates,
billiards, dominoes, a spinning crank**.

- **Catch:** rotation + inertia + stable stacking + friction + fast-object tunneling are
  exactly the hard, fiddly parts. Failure modes (jitter, sink-through, explode) live here.
- **API surface is big:** Box2D makes you think in World / Body / Fixture / Shape / Joint.
  Even **Box2D-Lite** (Erin Catto's teaching version, ~1500 lines, boxes only, sequential
  impulses) is a lot of machinery.
- **Prepared (2026-07-14):** Box2D **v3** (the pure-C rewrite) is vendored at `runtime/box2d/`
  and the four-target compile gate (mac/win/wasm/ios) PASSES — so it's a viable cart-land lib.
  Evidence, flags, the coordinate-mapping + cross-platform-determinism gotchas, and the demo to
  build (crate stacking / dominoes): [box2d-integration.md](box2d-integration.md). No cart yet.

### Particles + constraints (Verlet / PBD) — what the ragdoll already is
Points have a position and an *implied* velocity (current − previous). Everything is:
integrate, then relax constraints a few times. No rotation math, no inertia tensor, no
manifolds. You get **ropes, cloth, ragdolls, blobs, chains, soft bodies** almost for free,
with only a handful of concepts.

- The "easy physics library with a few functions" half-memory is almost certainly **this**
  genre.
- Canonical reference: **Matthias Müller's "Ten Minute Physics"** (PBD / XPBD).
- Collision-only reference: **`cute_c2.h`** (Randy Gaul — single header, just
  `circle / AABB / capsule / poly` overlap tests that hand back a manifold; you write the
  response).

**Instinct for *this* engine: PBD, not rigid bodies.** ~80% of the toy-game joy for ~20%
of the engine complexity, and it's basically "promote the ragdoll's inner loop into a
shared place." Existing seeds already in the repo: `ragdoll.c`, `rope.c`, `pendulum.c`,
and the `tritex` work.

### Fluids — a third world, and why the vendor/cart choice *flips*

The obvious off-the-shelf answer for fluids is **LiquidFun** (Google, ~2013): a particle
system bolted onto Box2D for water, viscous/elastic goo, and soft bodies (it even shipped a
`liquidfun.js` emscripten port). **Don't reach for it here.** It's a trap for this engine on
three counts: it's **C++** (built on *old* Box2D 2.x — so it doesn't drop into the C cart-land
model the way pure-C Box2D **v3** does, and it drags the C++ runtime across all four build
targets); it's **abandoned** (Google archived it ~a decade ago, pinned to Box2D 2.3 — exactly
the unmaintained-dependency portability risk the four-target gate exists to avoid); and it's a
**different lineage** from v3 (v3 has no particle system, LiquidFun rides v2 — you can't get
modern rigid bodies *and* LiquidFun fluids from one lib).

The reframe: **fluids fit the PBD family we already chose better than rigid bodies do.** A
particle fluid (PBD / SPH — neighbour density + a pressure constraint) is the *same shape* as
the jelly blob and cloth already built on `physics.h` — points + constraints, no new machinery
class. **Ten Minute Physics** (already cited above) covers a PBD fluid directly. So the
vendor-vs-cart call **inverts by phenomenon**:

| | off-the-shelf lib | hand-rolled |
|---|---|---|
| **Rigid bodies** | Box2D v3 — pure C, MIT, alive → *worth vendoring* | hard (SAT, manifolds, stacking) — the reason to vendor |
| **Fluids** | LiquidFun — C++, dead, old Box2D → **avoid** | PBD/SPH on `physics.h` — on-grain, code-first ✓ |

Rigid bodies are the case *for* a vendored lib (hand-rolling is the genuinely hard part — the
whole Box2D experiment). Fluids are the case *for* a cart (hand-rolling is squarely inside the
PBD family we own, and the only lib on offer is a dead C++ fork). So a **PBD-fluid demo cart**
is a candidate next to — or before — the Box2D rigid-body experiment, and it stays entirely
code-first with zero new dependency.

**Shipped: `waterjar`** — this is now real. A Position-Based Fluid (Macklin & Müller) on
`physics.h`: `phys_integrate` for the gravity predict + `phys_bounds` for the walls, and the
cart adds the fluid-specific parts — a uniform-grid spatial hash for O(n) neighbours, a
per-particle density constraint (incompressibility → a flat resting surface, avg density holds
at ~1.0 rest), XSPH viscosity, and a velocity limiter so a corner pinch can't spark. ~820
particles at ~0.9ms/frame. Tip the jar and it pours to the low corner; drag to stir. Zero new
engine code — exactly the "fluids are a cart, not a lib" bet, proven.

---

## How hard, exactly? The difficulty ladder

The difficulty cliff is **not** "shapes" in general — it's the three things that all
arrive together the moment a shape can be **oriented**:

1. **Rotation.** A circle has no orientation (roll it, it looks identical) → you never
   track an angle, never compute torque, never need a *moment of inertia*. A box/triangle
   has a facing, so an off-centre hit must spin it → now every body carries `angle` +
   `angular velocity` + inertia.
2. **"Where exactly did they touch?"** Two circles touching is the easiest collision in
   physics: centres closer than `r1+r2`, push direction = the line between centres (one
   subtraction). Two *polygons* need a real algorithm — **SAT** (Separating Axis Theorem)
   or **GJK** — just to find the contact point(s) and normal. That's the bulk of a real
   engine.
3. **Stable stacking** (the sneaky one). A box resting on the ground touches at **two**
   corners, not one — resolve only one and it rocks/jitters forever. So polygon contacts
   need *manifolds* (multiple contact points) solved together. Friction lives here too
   (a ball mostly rolls; a box needs friction or it slides off every ramp).

Easiest → hardest:

| Tier | What | Difficulty |
|---|---|---|
| Points | positions + constraints only | trivial |
| **Circles** (point + radius) | circle-circle, circle-vs-static-line | **easy — the sweet spot** |
| Static solids (ramps/walls) | only *immovable* boxes/segments, balls bounce off | still easy |
| Dynamic boxes/triangles | oriented, moving, colliding with each other, stacking | **big jump — full Box2D machinery** |

Two ways to cheat the cliff:

- **Static-only solids are cheap.** If triangles/boxes are *walls and ramps that don't
  move* and only balls move against them, you stay in easy territory (circle-vs-segment is
  a few lines). You only pay the full price for *dynamic* oriented shapes hitting *each
  other*.
- **A rigid box is free in the particle world:** 4 points + 6 sticks (rectangle + two
  cross-braces). It rotates, tumbles, and collides using *only* the point/stick code we
  already have — zero new concepts. A bit soft, stacks wobbily, but for a toy it reads as
  solid. The "have your cake" move: pseudo-solids without leaving the easy tier.

**Takeaway:** balls + static ramps/walls + stick-built pseudo-boxes gets surprisingly far
while staying entirely in the cheap, code-first tier. *Crisp* rigid polygons with proper
stacking is the part that turns it into a real project.

---

## The fantasy API (PBD flavour)

~9 calls. A kid could build a rope *or* a ragdoll *or* bouncy balls with the same
vocabulary:

```c
// physics — a tiny verlet world. you make points, link them, and step.
int   pt(float x, float y);            // a physics point; returns its id
void  pt_pin(int id);                  // nail it in place (anchor, wall)
void  pt_radius(int id, float r);      // give it size → points collide as circles
void  stick(int a, int b);             // rigid link, locked at current spacing
void  spring(int a, int b, float k);   // springy link (k 0..1)
void  world_gravity(float gx, float gy);
void  world_bounds(int x, int y, int w, int h);  // floor + walls everything bounces off
void  world_step(void);                // integrate + relax (call once in update)
float pt_x(int id);  float pt_y(int id);         // read back to draw
void  pt_kick(int id, float vx, float vy);       // throw / impulse
```

A whole rope:

```c
int prev = pt(160, 0); pt_pin(prev);
for (int i = 1; i < 20; i++) { int p = pt(160, i*6); stick(prev, p); prev = p; }
// in update(): world_step();  then draw lines between consecutive pts
```

If we later wanted the Box2D feel, the rigid-body version would add `rb_box`,
`rb_restitution`, `rb_friction`, `rb_angle` — but that's where the engine code (and the
"why does it jitter / explode" bug reports) balloons.

---

## The middle path actually pitched

Given code-first-sound and the open "API vs write-your-own-helpers" question, the first
move probably isn't baking `world_step()` into `studio.c`. Candidates, cheapest last to
most-committed first:

1. **A "physics starter" cart** (like a tutorial cart) — the ragdoll's guts generalized
   into a copy-me template. Cheapest, ships today, teaches the most, commits to nothing.
2. **Just the sharp primitives in the runtime** — vector helpers + collision tests
   (`circle_hit`, `seg_hit` returning a manifold, à la `cute_c2`) — and let carts write
   the response. Keeps "physics is programming," removes the tedious math.
3. **A readable `physics.h` mini-lib** the cart `#include`s. Learners can open it and see
   the verlet loop; black-box only if they want it to be. Would force a decision on the
   open `#include` question.

**Suggested order:** start with **#1** to feel out the API and let the function names stop
churning, then promote the winning shape into **#2 / #3** once a few carts have used it.

### No-regrets next step
Prototype a `physics.h` (option 3) with the ~9-function PBD API **plus** a demo cart — a
rope + a few bouncy balls + a draggable blob — so we can *feel* whether the vocabulary is
right before deciding if any of it earns a place in the runtime.

---

## What building the playground cart taught us

We did the no-regrets step as **option #1** — a readable starter cart,
[`tools/carts/physics.c`](../../tools/carts/physics.c) (committed; in the tutorials list as
"Physics playground"). The whole engine is three commented functions — `integrate`,
`relax_stick`, `collide` — plus `collide_seg`. Findings, mapped onto the difficulty ladder:

- **Soft bodies and cloth really are free.** A jelly blob is a ring of points + a hub
  (spokes keep it round, it squashes and springs back); cloth is a grid of points each
  linked to its right + down neighbour, pinned at the top corners. *Zero* new engine
  code — same point+stick primitives, just arranged differently. This is the strongest
  evidence yet that PBD is the right family for dreamengine.

- **`collide_seg` (point-vs-segment) buys solid faces cheaply.** ~20 lines made balls
  rest on box *faces* (not just corners) and let boxes stack. Confirms the "circles +
  static segments" rung is both cheap and high-value.

- **Tunnelling through thin surfaces → substeps, not more iterations.** A box's corner
  (radius 3) is a *thin catch-band*; a fast/heavy corner leaps through it in one frame and
  lodges in the interior (where deep overlap is invisible). Splitting the frame into a few
  **substeps** (gravity divided across them, so net fall is unchanged) fixed it — verified
  in the harness: the box-penetration `watch()` metric settled at exactly `0.0` (touching,
  not sunk), deterministically. `ITERS` (re-relaxing the same positions) does **not** help
  tunnelling — different knob. The cart exposes `substeps` live on `←/→` so you can feel it.
  Cheap radius widening (fatter catch-band) is the other lever.

- **Deep overlap / "point inside a shape" is the wall we did NOT climb.** Our thin-edge
  collision can keep separated things apart and catch shallow contacts, but it cannot
  *detect* a point buried well inside a polygon (it's > radius from every edge), so it
  can't eject it. Substeps mostly prevent getting there in the first place; truly resolving
  deep interpenetration needs point-in-polygon + minimum-translation ejection — i.e. SAT,
  the top rung. We deliberately stopped below it.

**Verdict so far:** the vocabulary (`pt` / `stick` / `collide` / step, with pinning and
grabbing both expressed as inverse-mass `w = 0`) held up across rope, cloth, soft body, and
pseudo-solids without churn. That's the signal to consider promoting it to option #2/#3 —
*if* a second cart wants the same code. Still no reason to bake an opaque `world_step()`
into the runtime.

---

## Update (2026-07-13): the trigger fired — enough carts want the same code

> Still a scratchpad, but the "if a second cart wants it" condition above is now
> emphatically met, so we're acting on it (option #3, the readable `physics.h` mini-lib).

The condition for promoting #1 → #3 was *"if a second cart wants the same code."* It's not a
second cart — a handful each hand-roll their own `{x,y,px,py}` point + integrate +
distance-constraint with **zero sharing** (`cart-dupes.js` territory), confirmed against each
cart's `verlet-integration`/`spring-damper` meta tag: `ragdoll`, `linerider`, `coaster`,
`growballs`, `inkrunner`, `calgames`, `marble`, `orbit` — plus `physics` itself. (Honest split:
some are true constraint solvers — ragdoll/linerider/coaster/growballs — some just integrate —
marble/orbit.)

> **Correction (2026-07-14):** an earlier draft of this list said "~ten" and included `acidrack`
> and `lotfill`. Both were false positives — the first pass grepped for `px`/`py`, which also
> matches *pointer* coords (`touch_x`/`touch_y`) and pixel vars, not just verlet
> previous-position. `acidrack` is a 303 rack, `lotfill` a noise-terrain demo; neither does
> physics. The real backlog is the eight above. **Migrated so far:** `ragdoll`, `linerider`,
> `coaster` (from the list) + `jelly`/`sloop`/`tentacle`/`verlet`/`waterjar` (newer). **Still hand-rolling:**
> `growballs`, `inkrunner`, `calgames`, `marble`, `orbit`.

### The shape we settled on — two layers, seam only in Layer 1

- **Layer 0 — transparent primitives (this is what we're building now).** A `PhysPt
  {x,y,px,py,w,r}` the cart still *owns*, plus the proven verbs lifted near-verbatim from
  `physics.c`: `integrate → phys_integrate`, `relax_stick → phys_link`, a springy
  `phys_spring`, `collide → phys_collide`, `collide_seg → phys_collide_seg`, `clamp_bounds →
  phys_bounds`, plus ragdoll's angular spring `angsp → phys_aim` and grab/throw helpers. The
  cart keeps its own arrays and its own loop; it just stops copy-pasting the math. Stays plain
  Verlet forever — reads end-to-end, code-first intact. The biggest-win-now dedup.
- **Layer 1 — a managed world (sketch only, NOT built).** The `pt()/stick()/world_step()`
  fantasy API above, but with *opaque* body handles + *model-neutral* observables
  (pos/vel/angle — never `px/py`). Built *on* Layer 0 today via a "verlet" backend, leaving
  room for a "rigid"/Box2D backend later that bypasses Layer 0 entirely. **The seam lives at
  Layer 1's backend interface — Layer 0 is never replaced.** This is how the "deeper route"
  (proper rigid bodies, à la the [dreamengine ⇄ playtime thought
  experiment](dreamengine-playtime-convergence.md)) stays *open* without committing to it now.

Three escalating ways to consume, pick per cart: raw `PhysPt` + helpers (toys, transparent) →
Layer 1 / verlet backend (managed, portable API) → Layer 1 / rigid backend (real Box2D, same
API, flip a flag). **Honest caveat:** the Layer-1 seam preserves the *API* across a backend
swap, not the *feel* — Verlet is soft/jittery, Box2D is stiff with real resting contact.

Struct composition keeps carts with extra per-point data clean: either a parallel array
(`physics.c` keeps colours in `pcol[]`) or embed `PhysPt` as the first member
(`struct { PhysPt p; int col; }` → pass `&thing.p`).

**Compatibility check (why the extraction is safe):** for equal masses (`w=1`), `phys_link`
reduces *exactly* to ragdoll's `solvesk` (both split the correction 50/50: `dx*(d-rest)/(2d)`),
and `phys_integrate(p,0,g,damp)` is byte-identical to `steppt`. So the verb-swaps in the
uniform-mass carts are behaviour-preserving by construction — the migration risk is only in
carts whose *bounds* convention differs from `physics.c`'s `clamp_bounds` (e.g. ragdoll's
softer `clpt`), which keep their bespoke bounds inline.
