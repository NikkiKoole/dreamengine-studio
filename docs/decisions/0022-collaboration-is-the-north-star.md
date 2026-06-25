# 0022 — the collaboration (maker + Claude) is the north star; learn-to-code retires to lineage
Date: 2026-06-25 · Status: accepted

## Context
VISION.md opened with one north star — *a teenager picks this up, makes something that moves
on screen in under an hour, learns real programming without hitting a wall* — and later named
a "second north star" ([`design/second-north-star.md`](../design/second-north-star.md)):
dreamengine as a tool for the maker *and Claude, together,* to build deep, honest simulations
behind a humble surface. The docs kept the teaching star **first** and treated the beginner
constraints (shielded loop, no `malloc`, a deliberately small API, no `#include`,
beginner-readable house comments) as *load-bearing* — the claim that they're what make the
deep sims read clearly.

After ~a month of daily work the honest retrospective ([decision 0020](0020-in-house-tool-curated-showcase.md)
already moved most of the way here): **nobody is learning to code on this.** The day-to-day
users are the maker and Claude; the kids *play* the carts, they don't author C. The teaching
framing had become a story we told to keep the origin lit, and it was distorting design
choices — every "should we prune the API / forbid the heap / agonize over Level 2" question
was downstream of an absent learner.

## Decision
The north star is the **collaboration**: dreamengine is a tool for the maker and Claude to
build **deep, honest simulations hidden behind a humble lo-fi surface**, one true core per
cart. The learn-to-code framing is **retired as a goal and an audience** — nobody is learning C
here, and we don't design the surface *for* a beginner. But the beginner is **kept as a design
critic** (see below): the only goal we drop is *serving* the learner; we keep *being judged by*
one.

The move that makes this clean is splitting one bundled commitment into two:

- **Aesthetic constraints — KEPT, as deliberate art direction.** 320×200, 32 colours, 8
  voices, the lo-fi SNES skin. Justified by *legibility of the simulation* ("the humble
  surface leaves the system nowhere to hide"), **not** by teachability.
- **Pedagogical constraints — RELAXED, demoted from principle to breakable default.** No
  `malloc`, the shielded `update`/`draw` loop, the small-API ceiling, no `#include`,
  beginner-readable comments. Each stays only as long as it earns its keep for *us*; break it
  when an honest sim wants to.

## Why
- It matches how the tool is actually used and most enjoyed — same honesty move as 0020, one
  layer deeper (0020 settled the *public*; this settles the *maker-facing mission*).
- It dissolves a cluster of open questions that only existed to protect a learner who isn't
  here: defensive API-pruning, the no-heap rule, the Level-2 "drop the curtain" drama, the
  `#include` hesitation.
- It promotes what the tool has *actually* grown — the verification/handoff layer
  (`spec`, `tune-check`, `canvas-diff`, `road-check`, the build-logs, the "next agent starts
  here" docs) — from overhead to first-class product. None of that was ever for a teen; it's
  the machine that lets a human and a model build something too big for one session.

## The beginner: retired as audience, KEPT as critic
The real worry with dropping the beginner is drift into sprawling, clever, lifeless carts. The
refinement (the maker's, and load-bearing): **the beginner retires as an *audience*, but is
kept, deliberately, as a *critic*.**

- **Retired as audience** — nobody's learning C here; we don't ship to them, design the API
  surface for them, or prune to fit their working memory.
- **Kept as critic** — *"could a stranger pick this up and want to keep touching it?"* is a
  brutal external test, and it's the **cheapest enforcer of the two things no oracle checks:**
  that the cart stays legible in six months, and that it stays *fun to a human* (not merely
  green to a `spec`).

So the bar is **two-part, not a swap**:

> **(1) verifiable** — scoped to one honest core, small enough that a deterministic oracle can
> judge it (the new strength: focus + correctness). **(2) beginner-legible-and-delightful** —
> would a fresh mind get it, and want to keep touching it (the soul: legibility + fun, the part
> no oracle checks).

The small, contained cart serves *both*: its value was never the beginner-as-learner, it's
*focus* — one cart = one honest core = a scope small enough to fully build, verify, and
understand cold later — and that same smallness is what keeps it legible and graspable to a
stranger. **Don't let "verifiable" quietly become the whole bar;** a cart that passes every
gate and delights no one has failed the half that matters most. The "ship a small cart that
exercises each new API function" rule keeps its full force — re-read as **spec-by-example**,
and still held to the beginner-critic's *delight* test, not just the oracle's *green* test.

## Consequences
- **The API can grow toward what we reach for.** Stop pruning defensively; each function earns
  its place by having a focused cart that exercises it (the existing rule, un-hedged). `malloc`
  / `#include` / a raw-loop opt-out (`STUDIO_NO_MAIN`) become "add when a cart needs it," not
  matters of principle.
- **Aesthetic constraints are not on the table** — they're art, and they stay.
- **cart-os un-hedges** ([`design/cart-os.md`](../design/cart-os.md)): its "no new public API"
  contortion was beginner-protection (kernel verbs as "a category error next to `circfill`")
  and is now taste-not-mandate; its north-star-fit anxiety softens (composition plumbing is
  core-adjacent, the same category as the oracle layer); and the focused-cart reframe
  *strengthens* its own demotion of the live Workbench (a sprawling concurrent desktop is the
  opposite of one contained core).
- **Substrate-tidying earns a small priority bump** — STATUS #44 (output convention) / #45
  (process table) are no longer apologetic "not the real mission" chores; smoothing what we
  build on is now mission-adjacent.
- **Lineage is preserved, not erased.** The teen-on-ramp stays in the docs as the origin story
  and the source of the focus instinct. This is not a licence to abandon legibility — it's a
  swap of *which* legibility we're held to.
- **Not relitigated:** the fantasy-console spec (resolution/palette/voices) and the in-house +
  curated-showcase shape (0020) are unchanged; this decision is about the maker-facing *why*,
  not the *what* or the *who-sees-it*.
