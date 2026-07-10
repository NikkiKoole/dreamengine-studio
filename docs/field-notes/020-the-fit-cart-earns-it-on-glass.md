# 020 — The fit-cart earns it on glass (and knows when to stop)

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-10

**Confidence**
High

---

## Observation

A full day was spent driving the `acidwire` fit-cart (the responsive-layout mock for the acidrack
redesign, step 4 of the [responsive-instrument-ui](../guides/responsive-instrument-ui.md) playbook)
on a real iPhone SE, tweak by tweak. It was the opposite of field note
[018](018-passing-the-gates-felt-like-done.md), where a layout *read* as done because the defect
oracle was green but nobody had held it. Here almost every decision was made — and several reversed —
by feeling it under a thumb. The session produced a coherent header system and, more usefully, a set
of **methodology lessons about wireframing itself**:

1. **Make the mock's controls actually work — no dead pixels.** A *drawn* knob/button/step lies about
   finger-ergonomics: you cannot tell whether a control is grabbable until you can grab it. Wiring the
   real `ui.h` widgets + tap handlers to dummy state (no sound) is what turned "does this fit?" from a
   guess into an answer. The knob-that-turns and the step-that-toggles are the *point* of the mock.

2. **Decide on glass; distrust desktop arithmetic.** Width math said six pattern buttons in the header
   were sub-finger (~18px) and wouldn't work. On the device they were fine — the fat-finger hit pads
   make ~18px cells comfortably tappable. The device is the oracle; the calculation is a hypothesis.

3. **The fit-cart answers LAYOUT, not CONTENT — and that boundary tells you when it's done.** It proves
   a *mechanism* fits (a compact strip, a page-cycle button, a mute-that-is-also-a-fader). It cannot
   tell you *which* parameters or pages an instrument actually needs — that is a "play the real engine
   and notice what you reach for" question. We hit this wall twice (which 3 knobs a compact 303 shows;
   what the N/K/F pages should hold). When the open questions stop being layout questions and become
   instrument questions, **the wireframe has done its job** — move to the real cart.

4. **Place a control by which direction has SLACK, not by one global rule.** RND/CLR felt impossible to
   place because we were solving *all* states at once. The unlock: landscape is width-rich → the ops go
   in the header row; portrait-fullscreen is height-rich → a sub-row under the header. Same control, two
   homes, one principle: *use the slack the shape gives you.*

5. **"Put it where you use it" collapses placement.** RND/CLR belong in the *editor* (expanded/focus),
   not in every state — which reduced a dozen per-state placement questions to one. (It also matches
   acidrack's existing rule that CLR/RND "act on what's open.")

6. **Park ideas with the WHY — they come back in a different context.** Knob-*pages* were parked because
   a tab strip costs a vertical row the stacked phone strips can't spare. But the maker then re-derived
   the mechanism as a single header *cycle button* (zero row cost), and the parked note's own text said
   pages fit "a single dense panel with vertical room to spare" — which is exactly the fullscreen view.
   A parked idea plus its reason is reusable; a bare "no" is not.

7. **Solve the tightest shape first.** The SE portrait is the smallest cell in the device matrix — get it
   right and the larger shapes come nearly free. It is also the only shape you can physically hold, so
   it is where "decide on glass" is honest.

8. **Harvest the idioms the mock throws off.** Two reusable patterns fell out that outlive this cart: the
   **dual-purpose control** (mute = tap-to-toggle *and* vertical-drag-to-set-volume, reusing tr808's
   tap-vs-drag axis gesture) and **the whole non-button area of a container is the container's primary
   tap target** (tap anywhere on a header = open it). Both are `ui.h`/design-system candidates.

---

## Why this matters

The maker is building many more racks (epiano, yachtrack, pocketbox, the radios). Step 4 of the playbook
— the fit-cart — is where each one's layout is either genuinely designed or quietly faked (018). These
lessons make the *doing* of step 4 concrete: it is an **interactive, on-device sketch**, not a static
picture, and it has a **known edge** (layout, not content) so you stop wireframing at the right moment
instead of trying to design the instrument inside the mock.

It also sharpens the theory of oracles from [015](015-repositories-can-learn.md)/[018](018-passing-the-gates-felt-like-done.md):
the human thumb is the judgment oracle for finger-ergonomics, and no headless check substitutes for it —
but it only works if the mock is *live*, which is lesson 1.

---

## Evidence

- `tools/carts/acidwire.c` and its session commits (`0098e104` interactive controls → `a81773b3`
  landscape ops) — every commit message is a decision reached on glass.
- The six-pattern width surprise (lesson 2): the arithmetic-vs-device disagreement, resolved by the SE.
- The RND/CLR placement arc (lessons 4–5): bottom-row → parked → left strip → landscape-header /
  portrait-sub-row, converging on "edit-in-the-editor, placed by slack."
- The pages arc (lessons 3, 6): mechanism proven, content deferred; tab-strip parked then re-derived as
  a cycle button. Captured in [`acidrack-layout-brief.md`](../design/acidrack-layout-brief.md) §2.

---

## Implications

Fold the actionable half into the playbook's step 4
([responsive-instrument-ui.md](../guides/responsive-instrument-ui.md) §4) so the next rack's fit-cart
starts from these. The generalisable rules:

1. A fit-cart's controls must be **functional (wired to dummy state), not drawn** — or it can't answer
   the finger question it exists to answer.
2. **On-device beats the calculation** — build→deploy→feel→tweak in tiny reversible steps; treat commits
   as checkpoints of what survived glass.
3. **Stop wireframing when the questions turn into instrument questions** (content, which params, how a
   voice behaves) — those belong to the real cart, not the mock.
4. **Place by slack; put it where it's used** — don't solve every shape at once.

---

## Open questions

- Can a fit-cart cheaply *fake* enough sound (a click, a pitch) to sharpen the content calls, or does
  that pull it across the layout/content line and back into 018 territory?
- Which of the harvested idioms (dual-purpose mute/fader, whole-header-tap) should graduate into `ui.h`
  vs stay per-cart?

---

## Related notes

- [018-passing-the-gates-felt-like-done](018-passing-the-gates-felt-like-done.md) — the failure this
  session is the constructive answer to: 018 said "hold it on glass"; this says *how* to.
- [015-repositories-can-learn](015-repositories-can-learn.md) — oracles as operational knowledge; the
  thumb is one, but only for a live mock.
