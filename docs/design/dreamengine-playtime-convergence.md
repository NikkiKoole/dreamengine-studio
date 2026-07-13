# dreamengine ⇄ playtime — a thought experiment (are these the same thing?)

> **Status: brainstorm / scratchpad, nothing decided — a little thought experiment, not a plan.**
> Sparked by a rambling session that started with "should we port love2d back to a C library
> for iPad/console apps?" and kept collapsing inward — reuse LÖVE's C libs → we already did that
> (raylib + sound.h) → which static language suits agent+human writing → we're already writing C
> here → *"wait, what is this other than dreamengine + Box2D + more graphics?"* This file just
> parks the idle question so it isn't lost. Nothing here is proposed, costed, or committed. Treat
> every number as a gut feeling and every option as one of several.

Related loose notes: [`physics-notes.md`](physics-notes.md) (the "tiny in-engine physics?" scratchpad),
[`external-data-carts.md`](external-data-carts.md) (a cart loads data at runtime),
[`cart-os.md`](cart-os.md) (carts as a shared environment).

---

## What the wondering is

Over in a sibling tree (`~/Projects/love/vector-sketch/experiments/`) there's a large LÖVE
project, **`playtime`** (~45k lines of Lua). Poking at it, it looks less like a game and more
like *the same kind of thing dreamengine is*: an **editor + runtime that loads external "carts"**
— here `.playtime.json` scenes + hot-reloadable `.playtime.lua` behaviour scripts, drag-drop
loading, and even an HTTP "Claude Bridge" for agent poking. It's the successor to the
`puppet-maker` experiments, and its own MANIFESTO frames it as *infrastructure* for a line of
small kids' apps (Mipo etc.), "the foundation, not the deliverable."

So the idle thought: **two platforms of the same shape may have been built twice, in two
languages** — and their strengths look oddly complementary. This doc is just holding that
observation up to the light. It might be nothing.

## The rough shape of the two halves (impressionistic, unverified)

- **dreamengine** — C. Static language (nice for agent+human writing), native perf, the
  `sound.h` synthesis engine, the tooling/oracle harness, an iOS pipeline that ships today, and
  `tritex()` (a textured-UV triangle) already in the rasterizer. *Seems to lack:* physics, a
  skeletal-skinning layer, a character system, an asset pipeline for many loose files.
- **playtime** — Lua. A mature **Box2D physics editor**, **dual-quaternion skinning** over
  physics "bones", a **DNA-driven character system**, the scene/cart data model — *and* a product
  vision. *Seems to lack:* the static-language agent-collab ergonomics, native/no-JIT perf (JIT is
  off on iOS/console), and any console path.

The thing that makes this more than a coincidence: the capabilities playtime has look almost
exactly like the holes in dreamengine, and vice-versa.

## A quick (unverified, gut-feel) sense of the runtime gap

Just a first-glance impression from one read-through — **not a costed estimate, could be very
wrong once anyone actually tries:**

| Capability a puppet-ish app wants | dreamengine today (rough sense) | Gut-feel effort |
| --- | --- | --- |
| Textured mesh drawing (`tritex`, RGBA sheet, blend, camera) | mostly there already | small |
| Large / retina resolution | already `-D` flags, 4096² ceiling | trivial |
| iOS shipping | pipeline exists (on the software backend) | trivial-ish |
| Rich audio | `sound.h` already huge | there |
| **Box2D physics** | nothing (only a brainstorm doc + one hand-rolled Verlet cart) | *feels big* |
| **Skeletal skinning layer** (bones → weights → DQS) | rasterizer ready, layer absent | *feels big* |
| **Asset pipeline for ~hundreds of loose PNGs** | a cart is one 64-slot pico32 sheet | *feels big* |
| GPU on iOS (mesh-heavy retina may miss 60fps on the SW backend) | SW today; GPU path is raylib-bound | *maybe* dodgeable via raylib-GLES rather than a from-scratch Metal backend — unclear |

Loose read: a few "already basically there" items sitting next to three "this is where the real
work would be" items. Whether those three are weeks or seasons is exactly the thing this doc
does **not** claim to know.

## Ways it *could* go, held loosely (none chosen)

- **A — absorb into dreamengine (C).** Add physics + a skinning module + an asset pipeline; re-port
  playtime's character/DNA/skinning work into C. One platform, all the C upsides — but "reuse code"
  mostly means reuse the *engine* while *re-porting* a big pile of hard-won Lua content.
- **B — converge at the data format, not the editors.** Keep playtime as the (mature, Lua) authoring
  tool; let dreamengine be a C *runtime* that loads its scenes/assets and plays them fast on
  iOS/console. Still needs physics + a skinning runtime in C, but skips re-porting the editor UI.
- **C — stay in LÖVE.** Ship via love-ios, FFI the hot kernels for perf, accept no console. Cheapest;
  gives up the perf/console/static-language reasons that started the whole wondering.

If anything, the least-silly-sounding version today is some flavour of **B with the core/surface
idea**: a shared C *core* (physics, skinning runtime, `sound.h`, backends, tooling) under two
distinct *surfaces* that keep their own souls — dreamengine's lo-fi cart gallery (lo-fi is a
curation stance, the runtime is already resolution-agnostic) and playtime's kids-app character
platform. But that's a hunch, not a recommendation.

## Reasons this might be a bad idea (kept honest)

- The real gap between the two isn't tech, it's **product soul** — "honest lo-fi sims" vs "kids-app
  character platform." Forcing one vision onto the other could wreck what's nice about each. Shared
  *core*, separate *surfaces* is the only version that obviously avoids that, and even that adds a
  seam to maintain.
- Scope-creep could quietly turn a lovingly-focused cart console into a general-purpose engine and
  lose the constraint that makes it special.
- Consoles remain their own wall regardless of any of this (licensed SDK + backend); none of these
  options makes that cheaper.
- Re-porting ~45k lines of subtle skinning/DNA math from Lua to C is the kind of thing that always
  looks smaller from a distance.

## Open questions (for if this is ever picked up again)

- What does playtime's `.playtime.json` scene format actually look like, and how much of it maps
  cleanly onto a dreamengine-flavoured cart?
- How married is the DQS/skinning math to LÖVE specifics vs. portable geometry?
- Is raylib-on-iOS (GLES) real enough to dodge a Metal backend, or not?
- Does any of this actually serve a want, or is it just tidiness talking? (The most important one.)

*— left here to point back to later; not on any roadmap.*
