# The dreamengine design language — the paradigms that make the shelf feel like one thing

STATUS: EXPLORING (2026-07-19) — a synthesis, not a new plan. Names and clusters the design
paradigms already lived across the shelf (most crystallized in
[`device-face-paradigm.md`](device-face-paradigm.md)) so the through-line is legible and a new cart
can be *checked against* it. Born from a design-focus session with the maker + a second agent reading
the device-face doc; this doc is the index, the detailed docs it links are the source of truth.

> **Why this doc exists.** The individual paradigms are already written down — but scattered across
> one big doc (device-face-paradigm.md), the north-star ADR ([0022](../decisions/0022-collaboration-is-the-north-star.md)),
> VISION, and the field-notes journal. Read together they describe a recognizable *philosophy*, and
> naming that philosophy is itself useful: it turns a flat list of rules into a **shape**, and it gives
> the answer to "what kind of thing is dreamengine becoming?" — **a design language for tiny creative
> devices.** Not a game engine, not just a fantasy console: a coherent way to build small, joyful,
> single-purpose creative instruments. This doc keeps that observation honest — it clusters what
> exists, re-grades a couple of maturities, and flags the few principles that are *lived everywhere but
> never elevated to a named principle with a home.*

## The one-line thesis

Every cart is **one honest core** wearing the charm of a **dedicated little device**, designed
**smallest-canvas-first**, kept simple by **spending visibility, not capability**, and discovered by
**prototype → play → correct → generalize** rather than designed up front. The point is **joy**, not
realism.

## The five families (interface paradigms)

These twelve are the *interface* paradigms — how a face is built and how a hand meets it. They cluster
into five families. Maturity is graded against the repo (SHIPPED/worked-through vs stated-but-thin);
the last column is where the detail actually lives.

### 1 · Identity — *what kind of thing are we building?*

| Paradigm | Core idea | Maturity | Home |
|---|---|---|---|
| **One Honest Core** | one cart = one idea, never a workstation | mature (repo-wide) | [VISION](../VISION.md) · [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) |
| **Device Face** | an instrument should feel like a purpose-built physical device, not a generic app | mature | [device-face-paradigm.md](device-face-paradigm.md) |
| **Instrument vs Workstation** | know when a cart should *not* become a face (empty zone 2 + plural heroes + staged workflow = workstation) | mature — has a *declined* worked case (`sampler`, 2026-07-14) | [device-face-paradigm.md §2 fit test](device-face-paradigm.md) |

### 2 · Simplicity — *how do we keep complexity under control?*

The shared belief: **complexity is allowed; visibility is expensive.**

| Paradigm | Core idea | Maturity | Home |
|---|---|---|---|
| **Progressive Disclosure** | show only what's needed now — depth is one tap away, not amputated | mature | device-face-paradigm.md §2 · [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) |
| **Design by Modes** | surfaces change *meaning* instead of multiplying controls (one button row = steps/voices/patterns by mode) | mature | device-face-paradigm.md §2 |
| **Continuous vs Contextual** | a few controls are permanently live; everything else is contextual | mature | device-face-paradigm.md §2 |
| **Scope Ladder** | every feature acts at a scope (rack › machine › pattern › voice › step); the scope is its home, and the home decides the UI class | mature — full section w/ the "flags → automation lanes" consequence | [device-face-paradigm.md §2c](device-face-paradigm.md) |

### 3 · Interaction — *how should a hand meet it?* (the ergonomic laws)

| Paradigm | Core idea | Maturity | Home |
|---|---|---|---|
| **Hero Surface** | every instrument has ONE defining gesture; it earns the largest surface (the display, even) | mature | device-face-paradigm.md §2 |
| **Selector vs Performer** | separate controls that *choose* what will sound from controls that *make* the sound | mature | device-face-paradigm.md §2 |
| **Usage over Widget** | a control's zone is set by whether you *ride it live* or *set-and-leave*, not by its shape (a knob isn't zone-2 *because* it's a knob) | mature | device-face-paradigm.md §2 |
| **Comfort before Density** | optimize for fingers and confidence, not for squeezing everything on (Fitts + Hick — `finger_px()` is non-negotiable) | mature — the acidfit "comfort threshold, not fit threshold" lesson | device-face-paradigm.md §2 |

### 4 · Hardware inspiration — *emulate the experience, not the engineering compromises*

| Paradigm | Core idea | Maturity | Home |
|---|---|---|---|
| **Hardware Spirit, not Hardware Constraints** | keep the charm (the buttons-only feel, the little-OLED look, the pocket intimacy); drop the baggage forced by physical knobs + untouchable glass. *The aesthetic is a gift, the limitations are baggage.* | mature | device-face-paradigm.md §2 (pocketbox = the worked case) |

This is the rare position most retro software gets wrong — it copies *both* the look and the paging /
no-knobs / hidden-hold limitations. We take only the gift.

### 5 · Family resemblance — *why they all feel related without looking identical*

| Paradigm | Core idea | Maturity | Home |
|---|---|---|---|
| **Grammar over Layout** | instruments share a *grammar* (knobs on top, a screen in the middle, play at the bottom) — a stranger recognizes the grammar, not a fixed silhouette | **emerging** — stated as "one grammar, two body-plans" but only ~2 faces prove it yet | device-face-paradigm.md §2 |

This is what lets an OP-1-inspired instrument, a 303 clone, and an Omnichord all read as dreamengine
apps without looking the same. It graduates from *emerging* to *mature* when a third and fourth face
land and the grammar holds.

> **A note on the "emerging" grades.** An earlier pass filed *Scope Ladder* and *Instrument vs
> Workstation* as emerging too. They're not — each has a full worked section and (for the latter) a
> named *declined* case. Grammar-over-Layout is the one paradigm here that's genuinely still thin on
> evidence. Grade against what's SHIPPED, not against how recently it was written down.

## The method behind the language — three principles lived everywhere, never named

These aren't interface paradigms — they're *how the language gets made*. They're practiced constantly
but have never been elevated to a first-class named principle with a home. That's the real gap this
doc surfaces (the paradigms above were already written; these were only ever *done*).

- **Discover, don't design.** The final UI is almost never invented up front — it's prototyped, played,
  corrected, and the paradigm *emerges*. The device-face doc is full of "we tried… that was wrong… the
  correction produced…"; that's not history, it's the process. **This is the one genuinely unnamed
  principle** — it lives in practice in the whole [`field-notes/`](../field-notes/) journal and in
  [`responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) ("the process"), and field notes
  [018](../field-notes/018-passing-the-gates-felt-like-done.md) / [020](../field-notes/020-the-fit-cart-earns-it-on-glass.md)
  are it learning the hard way (a layout *read* as done because a gate passed).

- **Generalize after repetition** (the rule of three). Never generalize first: three carts need it → extract
  a header; three patterns appear → name the pattern. This is the repo's **most-practiced** rule, not an
  absent one — every `runtime/*.h` cart-land header exists because of it (`acid303.h` says in its own header
  it was *"extracted because tb303/acidrack had drifted their own copies"*; `tr808.h`/`tr909.h`, `roadkit.h`,
  the pending `grid.h`). The device-face doc even cites it by name: *"build in a cart first, extract when a
  second wants it."* Codified as *practice*, never elevated to a *principle*.

- **Delight over realism.** Dreamengine chases joy, not fidelity — candy style, screen mascots, animated
  scopes, friendly instruments, the kids' apps. This one is arguably the *most* written down: it's the soul
  half of [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)'s two-part bar
  ("legible-and-delightful to a stranger… no oracle checks it") and it has a dedicated home in
  [`candy-style.md`](candy-style.md). Naming it as a first-class principle just makes the priority explicit:
  when realism and delight conflict, delight wins.

## Fixed Canvas — the constraint that feeds all of it

> **A dreamengine app is first designed to work beautifully in 160×100. Larger screens reveal more, but
> the smallest canvas defines the design.** The wording matters: *not* "dreamengine is 160×100" — **"design
> starts at 160×100."**

This is the discipline underneath the whole language, and it's not nostalgia — it's forced clarity. At
160×100 there's physically no room for eight menus and twenty buttons, so **every pixel has to earn its
place**: prioritization is mandatory, a hero *must* emerge (there can't be six equal things), graphics
*must* be iconic (16×16 icons, 8×8 emojis, tiny mascots — all consequences of this one rule), and feature
creep is checked at the door — *if you can't say where a feature lives in 160×100, maybe it doesn't belong,
or the design is wrong.* **The canvas becomes a reviewer.** It's the physical enforcement of Progressive
Disclosure and the Scope Ladder — without it, it's too easy to cheat (need another option? another panel).

**Why the ratio is safe:** 160×100 is 16:10 (= 1.6), the same ratio as the 320×200 base but one chunk-size
down. In landscape it's the *tightest* you'll ever be — real phones are *wider* (~2.16), iPads are *less
wide but bigger overall* (1.33 with vertical headroom) — so every real device hands you **more** room in at
least one axis, never less. That's why the responsive ladder runs **160×100 → phone → tablet**, expanding
*outward* (showing more), not the usual desktop → tablet → phone *compression*. Tablets become **expanded**,
never "compressed."

This is [ADR-0029](../decisions/0029-320x200-is-the-base-resolution.md) ("design at the base, reflow
outward — the base is a design canvas, not a device bet") stated from the *discipline* side rather than
the *pixel* side. **Candidate to graduate into its own ADR** ("Fixed Canvas — the smallest canvas defines
the design") with 0029 pointing at it, because the name foregrounds the discipline, and the discipline —
not the number — is the load-bearing idea. It's also the seam to the engine work in
[`device-adaptive-layout.md`](device-adaptive-layout.md).

## Constraints create quality — the shared DNA

Several of these are the same move seen from different angles: *one honest core*, *160×100 first*, *one
hero surface*, *few always-live controls*, *one cart / one idea*. Each deliberately makes the design
problem **harder up front** so the result is **simpler and stronger**. That's the deepest theme running
through the shelf — the constraints are *intentional*, not inherited, and they're what make the ecosystem
feel coherent even as it grows.

## Open questions

- **The tablet route is NOT settled.** The device-face doc's §2 line — *"scale phone→tablet by showing
  MORE, not by rearranging"* (Pure Acid's iPad shows bass knobs + sequencer + drum pads at once) — is the
  paradigm's *stated* answer, but the maker is **not committed to the "show more faces on iPad" route yet**
  (2026-07-19). Fixed Canvas + "expand outward" is settled *as a principle*; **what expanding actually
  produces on a tablet is open** (more faces at once · one face with more breathing room · something else).
  Don't treat "more faces on iPad" as decided — it's a candidate, and the near-term responsiveness work
  (landscape-lock + a screen-filling stopgap) doesn't depend on resolving it.
- **Should this doc own a checklist?** A "score a new cart against the language" pass (which family does
  each control serve? is there a hero? does zone 2 have anything to ride?) could become a lightweight
  design gate — but only if it stays a *conversation aid*, never a box to tick (the field-note-018 trap).
- **Does "Discover, don't design" deserve its own home** (a field-note or ADR), given it's the one
  genuinely unnamed principle and it governs *how every other principle was found*?

## Related

[`device-face-paradigm.md`](device-face-paradigm.md) (the deep source for the interface paradigms — the
five zones + the §2 principles + §2c scope ladder) · [`candy-style.md`](candy-style.md) (Delight, made
concrete) · [`design-system.md`](design-system.md) (the VISUAL contract the language is dressed in) ·
[`device-adaptive-layout.md`](device-adaptive-layout.md) (the engine plan Fixed Canvas rides on) ·
[`responsive-instrument-ui.md`](../guides/responsive-instrument-ui.md) ("Discover, don't design", as a
process) · [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) (One Honest Core + the
two-part delight bar) · [ADR-0029](../decisions/0029-320x200-is-the-base-resolution.md) (Fixed Canvas,
from the pixel side).
