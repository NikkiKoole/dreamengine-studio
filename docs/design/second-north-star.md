# the north star — the honest, adult pursuit (built by the maker + Claude)

STATUS: REFLECTION (2026-06-09; promoted to *the* north star 2026-06-26 by
[decision 0022](../decisions/0022-collaboration-is-the-north-star.md)). The philosophy doc
for [`../VISION.md`](../VISION.md) — not a roadmap, not a spec, not a status. VISION.md
carries the principle in a paragraph; this note is the longer thinking behind it. Companion
reading: the legendary carts' build-logs ([`sloop.md`](sloop.md), [`galerijflat.md`](galerijflat.md)),
the audio direction ([`tinyjam-racks.md`](tinyjam-racks.md), [`product-notes.md`](product-notes.md)).

## The pursuit (it used to be the *second* star; now it's the only one)

> dreamengine is a tool for **building deep, honest simulations hidden behind a humble lo-fi /
> SNES-ish surface** — built by the maker *and Claude, together,* across many sessions.

For a while this ran *alongside* an original learn-to-code star ("a teenager makes something
that moves in under an hour"). [Decision 0022](../decisions/0022-collaboration-is-the-north-star.md)
made the honest call: nobody is actually learning C here — the users are the maker and Claude,
the kids *play* the carts — so the teaching star **retires to lineage.** This isn't a pivot;
it's dropping a cover story. The console is the same, aimed at a harder target, by an adult who
knows what they want working with a model that holds the detail across sessions.

**But the beginner is kept as a *critic*, not as an *audience.*** "Could a stranger pick this
up and want to keep touching it?" is a brutal external test, and it's the cheapest enforcer of
the two things no `spec`/`tune-check` oracle can check: that a cart stays **legible in six
months** and stays **fun to a human.** So the bar Fork A leaves us is two-part — **verifiable**
(focus + correctness, the new strength) **and beginner-legible-and-delightful** (the soul). The
standing danger is letting "verifiable" quietly become the whole bar; a cart green on every gate
that delights no one has failed the half that matters most. (This is the same worry as
"legibility is the standing problem" below, named as a discipline rather than a hope.)

## The one move, everywhere: set it up, and the system tells you the truth

The carts that feel most alive — call them the **legendary series** (`orbit`, `coaster`,
`galerijflat`, `sloop`) — are all the same move:

- **One honest core, never faked, never special-cased.** orbit: "the SAME integrator runs
  the live ship AND the predicted path, so the dots never lie." sloop: "the vehicle is not a
  sprite with stats — it is a grid of parts, and how it drives is *computed* from the parts
  you bolted on. Nothing is faked." galerijflat: "no light flips without a resident plausibly
  there — the patchwork is testimony, not random twinkle."
- **Emergence over authored content.** Behaviour falls out of the rules ("no per-rig
  tuning"); you author the rules, never the outcomes.
- **A mode flip — the second face.** build↔drive, ship↔prediction, gallery↔balcony,
  coaster↔slide. One model wearing two faces; the flip is the spine.
- **An honest readout drawn from the *same code the sim runs*** — the COM crosshair, the
  prediction dots, the support polygon. The setup tells the truth *before* you commit.

That's not four ideas. It's one idea four times: **author the rules, then let the system
judge you.** The 95% where it betrays you is the loop, not the rare success.

## The audio collection is the same soul at scale

The ~60 sound carts (`modrack`, the synths, the machine homages, the radios) are the same
pursuit shown wide instead of deep: **one real synthesis engine (`sound.h` + `INSTR_*`)
wearing many faces,** with a strict "copy the closest relative, never hand-roll the chassis"
library discipline (see [`../guides/instrument-carts.md`](../guides/instrument-carts.md)).
It's a *real* engine — analog circuits rebuilt, Karplus-Strong, FM, phase distortion, a
formant voice — not chiptune approximations; `mt70` exists only to prove the engine's range
with zero new engine code.

The radios are the one branch still missing its second face: today you *tune* them and
watch, but you can't grab the wheel. [`tinyjam-racks.md`](tinyjam-racks.md) is that missing DRIVE
mode — generate on the radio, then play and edit the song in a rack. Same lesson as the
legendary carts: a one-faced thing comes alive the moment it gets its other face.

## The throughline: documentary / homage

Every cart models **something real and beloved, honestly, in miniature** — KSP, Cataclysm's
vehicle grid, Dutch systeembouw housing, the TR-808, Satie, Wendy Carlos's counterpoint.
That tribute impulse is the engine's soul as much as any feature.

## Why the combination is load-bearing (not a compromise)

"Deep sim behind a humble surface" is the *right* pairing, not a limitation we tolerate — and
the reasons are about **art and focus**, not teaching (the cross-justification "constraints make
it teachable, which makes it honest" retired with the teaching star; [0022](../decisions/0022-collaboration-is-the-north-star.md)):

1. **The humble surface leaves the system nowhere to hide.** A photoreal renderer invites
   you to look *at* the picture; 320×200 and 32 colours can only carry a cart if the system
   underneath is *true*. The lo-fi skin is what lets the simulation be the star. (Same instinct
   as CLAUDE.md's game-feel rule: anti-flash, pro-clarity — "what does the player need to feel?",
   not "what looks cool?")
2. **The contained cart is a forcing function for focus.** It makes you render *only what the
   sim needs you to feel*, and it keeps each cart to **one honest core** — a scope small enough
   to fully build, verify, and grasp cold later. That focus is the value the small cart always
   had; the beginner was never the reason, just the old excuse. The same smallness is what makes
   the cart **legible to a stranger** (the kept critic) *and* judgeable by a deterministic oracle
   (the kept gate) — one property serving both halves of the bar.
3. **Surface and depth grew up together.** PICO-8 → SNES isn't just nicer pixels — SNES was
   the era of the deep genre game under a charming skin. The graphics ceiling rising tracks
   the simulation ambition rising. A coherent arc, not drift.

## The collaboration is itself an artifact

The design docs with their ✓-marked decisions, rung-by-rung build logs, and "next agent
starts here" handoffs (`sloop.md`, `galerijflat.md`) are the *product* of the human+model
way of working: the **thinking is captured as durably as the code**, so a system too big for
one session gets built across many without losing the thread. That method is part of what
dreamengine has become.

## Honest tensions and forks (the part that isn't triumphant)

- **Fantasy console vs. deep-sim authoring tool.** ✓ *Chosen on purpose* —
  [decision 0022](../decisions/0022-collaboration-is-the-north-star.md). Carts are well past
  beginner size (galerijflat 1440 lines); the "learn to program" framing is now lineage, the
  fantasy-console *constraints* are kept as deliberate art, and the API/cart-length tension is
  resolved by letting depth grow while keeping each cart to one contained core. Not a drift — a
  decision.
- **The depth is invisible by design.** The best thing about each cart is the thing a
  passerby can't see in a 3-second thumbnail — galerijflat's whole social sim vanishes in a
  screenshot. So **legibility is the standing problem**: making the depth *felt fast*, the
  moment a hand touches it. (The second face is the answer: interaction is how the honest core
  reveals itself.)
- **The product fork.** [`product-notes.md`](product-notes.md) sketches a real road where the
  audio line ships to strangers (Pocket-Operator-shaped, the web gallery as funnel). Hobby/art
  body of work vs. a thing people pay for — the decision there is already "sketch first, get it
  in hands, watch."

## Where it points

Lightly, because the work will say more than this note can: from **watchable** honest systems
toward **playable** ones with consequence (sloop's ladder is exactly that move); and toward
**convergence** — a sim whose soundtrack is as honest as its physics, every system one honest
core feeding the others (galerijflat already reaches for wind, the lift ding, TV glow; see
[`../guides/game-music.md`](../guides/game-music.md)).

The thing that *doesn't* fit the trajectory is stopping at watchable. Everything built so far
wants its second face.
