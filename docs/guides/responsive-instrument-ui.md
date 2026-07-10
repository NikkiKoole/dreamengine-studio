# Responsive instrument UI — the playbook for a new Tinyjam rack

The reusable **order of operations** for taking a new instrument/rack cart to a UI that's beautiful
and playable from a 320-wide phone up to a desktop window. It ties together the pieces that already
exist — the soul-first firewall, the homage research pass, the layout brief, `lay.h`/`ui.h`, the
oracles, and the bake-to-hands loop — into one sequence, and bakes in the lessons of field note
[018](../field-notes/018-passing-the-gates-felt-like-done.md) (the acidrack reflow that passed the
gates but wasn't *designed*).

> **The one idea.** Responsive UI is not "make it resize." It's **decide what earns space at each
> size, on paper, then build to that decision and check the build against it.** Every step below
> produces an artifact and clears a gate before the next — that's what stops "reflows without
> overflow" from masquerading as "done".

## The sequence at a glance

| # | Step | Produces | Gate before moving on |
|---|---|---|---|
| 0 | **Sound first → then inventory the controls** | a control inventory (type + cadence) | the instrument sounds right; every control listed |
| 1 | **Steal the homage's information architecture** | a research note (zones · hierarchy · what clones changed) | you know the canonical layout, not just your guess |
| 2 | **Rank controls into disclosure tiers** | the compact / expanded / drawer tiering | each control has a tier + a survival order |
| 3 | **Write the layout brief — BEFORE code** | `docs/design/<cart>-layout-brief.md` | the maker signs off the taste calls |
| 4 | **Prototype arrangements in a fit-cart with `lay.h`** | a `<cart>fit.c` mock (or extend `acidfit.c`) | arrangements branch on measured shape, no raw `screen_w()` math |
| 5 | **Sweep the device matrix: defects + judgment** | a clean matrix + per-shape self-report | no breakage AND no degenerate outcome vs the brief |
| 6 | **Bake it into the maker's hands, per arrangement** | baked `.cart.png` per shape | the maker played each on glass |
| 7 | **Ship — done = brief-match + delightful** | brief STATUS → shipped | the two-part bar, not "no findings" |

## The three laws that ride along every step

(Full evidence + numbers: [`../design/acidrack-ui-research.md`](../design/acidrack-ui-research.md).)

1. **Reflow between bands, scale within a band.** Different *aspect* → reflow at a breakpoint; same
   aspect / bigger window → uniform-scale a fixed layout. Scaling across an aspect change is what
   forces the cram-or-inflate choice.
2. **When space grows, reveal more — never enlarge fewer.** A bigger canvas gets *more per screen*
   (unfold strips, more voices, meters), not the same 3 knobs inflated. Three giant knobs on an iPad
   is the "too big / meaningless" smell.
3. **48 px hit area — the visual may be smaller.** Every tappable control captures ≥48 px (Apple
   44 pt + Material 48 dp); the drawn knob can be 32 px with a transparent fat-finger pad (`ui.h`).
   Size earns rank by hierarchy, not leftover room.

---

## 0 · Sound first — then inventory the controls

The instrument's soul is its **sound**; get that right before any UI. Imagine the ideal instrument
*before* reading existing carts (the firewall:
[`cart-authoring-prompt.md`](cart-authoring-prompt.md)), then pick the closest chassis via
[`instrument-carts.md`](instrument-carts.md). You cannot lay out a UI until you know the control set,
so the first *UI* act is to enumerate it.

**Produce — a control inventory.** For every control: `name · type · cadence`.
- **type** — knob (continuous rotary) · drag-lane (ribbon/XY) · step-grid · toggle (2–3 state) ·
  selector (pick-one-of-N).
- **cadence** — *ride-live* (you're always touching it: a 303's cutoff) · *set-once* (dial and
  leave: tuning) · *rare* (calibration, fx routing).

Cadence is what drives tiering in step 2 — write it down now, it's the whole game.

## 1 · Steal the homage's information architecture

Someone spent decades solving this instrument's layout. Do a short research pass on the real
hardware + the best software clones: **the canonical zones, the size hierarchy, what's shown
one-lane-at-a-time, the color coding, the hero row** — and what the clones *changed* (they usually
leave the identity controls alone and innovate in the sequencer).
[`../design/acidrack-ui-research.md`](../design/acidrack-ui-research.md) is the worked example for
the 303/909/808.

**Steal the *information architecture*, never the mouse-precise control sizes** — ReBirth crammed
~200 controls into 1024×768 because it had a pixel-precise mouse; you have fingers, so the 48 px
floor governs sizing (brief §0 house rule).

**Produce** — a short note (its own `docs/design/<cart>-ui-research.md`, or a section in the brief):
zones + hierarchy + the transferable moves.

## 2 · Rank controls into disclosure tiers

Sort the inventory into the **survival order** — which controls die first as space shrinks:

- **Compact tier** — the 2–3 you *ride live* (the instrument's core gesture). These survive to the
  smallest size.
- **Expanded tier** — the shape-it controls + the full editor (piano roll / drum grid). Appear when
  there's room.
- **Drawer tier** — rare/utility (calibration, fx routing). A second page / loupe / "more" reveal.

This is the direct answer to "too big": **a control earns a large size by being high in the tier,
not because the screen had room.** Sameness-of-identity still wins where it's the brand (a 303's six
equal knobs stay equal — don't tier *within* the signature row).

## 3 · Write the layout brief — BEFORE any layout code

Copy [`../design/acidrack-layout-brief.md`](../design/acidrack-layout-brief.md) as the template
(it *is* the template — keep it ~half a page per section);
[`../design/epiano-layout-brief.md`](../design/epiano-layout-brief.md) is a worked second example
for the single-instrument (keybed) shape. Fill:

- **§0 house rules** (carry them verbatim — they apply to every rack).
- **§1 the three-state strip** — folded → compact → expanded per instrument.
- **§2 the compact strip, per instrument** — the tier-2 output; the ride-live knobs + the one lane.
  *This table is where the maker's taste calls live.* Write your recommendations as "guesses to
  react to," not decisions.
- **§3 editor-swap by shape** — roomy keeps the rich editor; phone swaps to a one-lane editor.
- **§4 arrangements per shape** — roomy / tall (portrait) / short-wide (landscape). Portrait stacks
  (accordion); short-wide wants tabs (accordions degenerate short — the `acidfit` finding).
- **§5 footprints in fingers** — folded/compact/expanded sized so the *controls inside* stay
  finger-comfortable (comfort threshold, not fit threshold).
- **§6 done-means** + **§7 the device matrix** it's designed against
  ([`../design/device-matrix.md`](../design/device-matrix.md) §2).

**Gate: the maker signs off the taste calls here, on paper.** This is the committed intent artifact
018 found missing — without it, "reflows at several sizes" is indistinguishable from "done".

**Don't agonize over a taste call** ([ADR-0028](../decisions/0028-sensible-defaults-optional-tweaks.md)):
where reasonable users could differ (a grid layout, a range cap, a scale), pick the option a
*stranger* gets immediately, ship it as the default, and leave a **seam** (an enum + one switch
point) so it can flip or become a tweak later. Record the call as *decided (provisional)* and move
on — a feel/sound default gets confirmed on glass, not debated on paper.

## 4 · Prototype the arrangements in a fit-cart — never raw `screen_w()` math

Build the arrangements as a **mock cart first** (the `rackfit` / `acidfit` lineage), using:

- **`runtime/lay.h`** — `split · at · cell · grid · wrap · aspect · fluid · clamp · pad` — reflowing
  rect-in/rect-out. This *replaces* hand-rolled rect math.
- **`runtime/ui.h`** — widgets with per-finger capture + fat-finger hit pads + `ui_loupe()`.
- **Branch on the measured shape** (min-side + orientation), **not a device name** — a short
  landscape phone defers more than a tall portrait one.

> **The 018 trap to avoid (its #1 lesson).** Stretching a live cart's fixed layout with incremental
> `screen_w()` arithmetic (`W() < 300`, `W() - 72`) is the path of least resistance under momentum —
> and it's the exact dialect `lay.h` exists to replace. **Reuse the model; don't re-hand-roll it.**
> The disclosure model (priority + finger-footprint + folded/compact/expanded) currently lives in
> the prototype `tools/carts/acidfit.c`; **extend that**. When it graduates to a shared header
> (`disclose.h` + a `finger_px()` unit, per device-adaptive-layout.md's Phase 3), *call* the header
> instead of reimplementing the pass.

**Produce** — a `<cart>fit.c` (or the extended `acidfit.c`) that renders each §4 arrangement.

> **How to actually run this step — lessons from the `acidwire` pass (field note
> [020](../field-notes/020-the-fit-cart-earns-it-on-glass.md), 2026-07-10).** The fit-cart is an
> *interactive, on-device sketch*, not a picture:
> - **Wire the controls to dummy state — no dead pixels.** A *drawn* knob/step lies about
>   finger-ergonomics; you can't tell if it's grabbable until you can grab it. Use the real `ui.h`
>   widgets + tap handlers against throwaway values (no sound). The knob-that-turns *is* the point.
> - **Decide on glass; distrust the arithmetic.** Width math will say a control is sub-finger when the
>   fat-finger hit pads make it fine (or vice-versa). Build→deploy→feel→tweak in tiny reversible steps;
>   commits are checkpoints of what survived the thumb, on the **tightest shape first** (iPhone SE).
> - **Place by the slack the shape gives you, and put it where it's used.** Don't solve every shape at
>   once — landscape has width (controls in the header), portrait-fullscreen has height (a sub-row). And
>   a control lives where you *edit* (expanded/focus), not in every state.
> - **Park ideas with the WHY.** "No, because it costs a row here" is reusable (it may fit elsewhere —
>   a parked tab-strip became a zero-cost header cycle button). A bare "no" isn't.
> - **Stop when the questions turn into instrument questions.** The fit-cart proves a *mechanism* fits
>   (a compact strip, a page button, a fader); it can't tell you *which* params/pages you need — that's
>   a play-the-real-engine call. When the open questions are content, not layout, the mock is done →
>   graduate to the real cart (step 5 onward / the R5 port).

## 5 · Sweep the device matrix — defects AND judgment

Two different checks; you need both (018 #2: the defect oracle is structurally blind to *absence*).

- **Defect oracle** — `node tools/ui-audit.js <cart> --explore --resize "<matrix>"` (clipped /
  overlapped / off-screen / dead widgets) + `node tools/mobile-lint.js <cart>` (tiny `tap` targets →
  recommends `ui_loupe`). Reference matrix: [`../design/device-matrix.md`](../design/device-matrix.md) §2.
- **Judgment pass** (today manual; graduating onto `ui-audit --explore` per the plan). Per shape,
  ask: does it **match the brief's arrangement**? Any **degenerate outcome** — 0 sections open,
  sub-finger controls, a *missing* arrangement (the iPad all-compact 018 never built)? Is **density
  held roughly constant** across bands (law 2)?

**Produce** — a clean matrix + a one-line-per-shape self-report ("iPad: 5 compact, density OK; phone
portrait: 1 expanded + 2 compact + 2 folded, min control 1.1 finger"). That report *is* the judgment
artifact until the tool owns it.

## 6 · Bake it into the maker's hands — per arrangement, continuously

Not once at the end. **The first real judgment pass is fingers on glass** (018 #5). Bake so the maker
loads it in the editor / on device and *plays* each arrangement:

```
node tools/make-cart.js tools/carts/<cart>.c editor/public/carts/<cart>.cart.png   # re-embed source
node tools/make-cart.js --run editor/public/carts/<cart>.cart.png                  # bake the thumbnail
```

Bake for the maker to check — don't hand over dumped frames or screenshots (that's for your own
diagnosis). Iterate 3→6 until each arrangement plays right.

## 7 · Ship — done means the two-part bar

Done is **matches the brief on device AND legible-and-delightful to a stranger**
([ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)), *not* "no audit findings":

- Matches §4 on device — including the roomy all-compact arrangement.
- Every control ≥ 1 finger-unit at every shape, or a declared remedy (loupe / editor swap).
- `ui-audit --explore --resize` clean + the judgment pass clean.
- **The maker played each arrangement on glass and calls it delightful.**

Then flip the brief's STATUS to shipped and keep it as the diff-target — a rack whose code later
drifts from its brief is a regression you can now *see*.

## The smell test (carry it through every step)

**Distrust "done" when the diffs don't match the work's nature** (018 #4). A responsive-design pass
that produced only threshold tweaks (`W() < 300`) hasn't happened yet — the shape of the diff should
match the shape of a redesign (new arrangements, a disclosure pass, a brief). If it's all small
patches, step back to step 3.

---

**Related:** [`../design/device-adaptive-layout.md`](../design/device-adaptive-layout.md) (the engine
+ product plan this operationalizes) · [`../design/acidrack-layout-brief.md`](../design/acidrack-layout-brief.md)
(the brief template) · [`../design/acidrack-ui-research.md`](../design/acidrack-ui-research.md) (the
worked research pass + the numbers) · [`cart-authoring.md`](cart-authoring.md) (the general cart
walkthrough) · [`instrument-carts.md`](instrument-carts.md) (pick the sound chassis) · field note
[018](../field-notes/018-passing-the-gates-felt-like-done.md) (why this order exists).
