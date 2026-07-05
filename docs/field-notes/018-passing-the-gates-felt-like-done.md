# 018 — Passing the Gates Felt Like Done

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-05

**Confidence**
High

---

## Observation

The acidrack Phase-3 reflow (device-adaptive layout) was committed across three sessions, each
commit message claiming Phase-3 progress. The maker then play-tested it on real devices and found
it far from done — not broken, but not *designed*: no iPad arrangement exists at all (a stretched
accordion, one panel open, is what a 12.9" tablet gets), controls threshold on raw pixels rather
than fingers, and none of the validated model from the prototypes was actually used.

The work had drifted from the plan in a specific, instructive way:

1. **The validated prototype never graduated, so the port bypassed it.** `acidfit.c` proved the
   priority + finger-footprint disclosure model. But it stayed a mock cart; the instruction in the
   backlog was "graft the arrangements from acidfit.c" — a one-line pointer with no mechanical
   path. Retrofitting a live 1600-line cart onto prototype logic is restructuring work; stretching
   the existing fixed layout with `screen_w()` arithmetic is incremental work. Under momentum, the
   incremental path won. The result: hand-rolled pixel math (`W() < 300`, `W() - 72`), no `lay.h`,
   no finger unit, no disclosure pass — the exact dialect `lay.h` exists to replace.

2. **"Done" was judged by the defect oracle, not the design bar.** `ui-audit --explore --resize`
   passes when nothing is clipped, overlapped, or dead. It cannot say "the iPad arrangement is
   missing" or "these knobs are sub-finger on an iPad mini" — it checks *breakage*, not
   *judgment*. With the defect oracle green and every size reflowing without overflow, the work
   *read* as done. This is [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)'s
   exact warning materialised: the verifiable half of the bar had a gate, the
   legible-and-delightful half had none, and the gated half quietly became the whole bar.

3. **No committed design artifact existed to diff against.** The owning doc deliberately left
   "which arrangements per rack" as "a per-rack taste call made during Phase 3" — so the taste
   call was never written down anywhere. Without a spec naming "iPad = N strips inline, phone
   portrait = accordion with X pinned, phone landscape = tabs", there was nothing for either the
   agent or a reviewer to notice was missing. Absent a brief, "reflows at several sizes without
   audit findings" is indistinguishable from "done".

4. **The phase was named design work but executed as patches.** The doc even warns: "the dense
   racks are real redesign work… genuine design, not automatic. Budget it per rack; it's the bulk
   of Phase 3." The commits that landed were patch-shaped — "landscape tabs", "knobs wrap",
   "content-aware threshold" — each locally sensible, none stepping back to the redesign the doc
   called for. A phase whose essence is design produced only small diffs; that mismatch is itself
   a signal, and nobody read it.

5. **The first real judgment pass was the maker's fingers on glass, after "done".** All prior
   verification was headless sweeps and simulator screenshots. The feedback that mattered —
   playing it — came last instead of continuously.

---

## Why this matters

The maker is planning more music apps. Every future rack (epiano, yachtrack, pocketbox, the
radio carts…) walks the same road: a dense instrument panel, a validated-but-generic layout
model, and pressure to call it done when the gates go green. Without a structural fix this
failure repeats per cart.

It also refines the repo's theory of oracles ([015](015-repositories-can-learn.md)): oracles are
excellent at *breakage* and structurally blind to *absence*. A missing arrangement, a missing
compact state, a missing design decision — no pixel-level check flags what was never drawn.
The complement to a defect oracle is a **committed intent artifact** (a brief) that turns
absence into a visible diff.

---

## Evidence

- `tools/carts/acidrack.c:459-519` — the Phase-3 block: raw-pixel thresholds, no `lay.h`, no
  finger unit, no `acidfit` disclosure pass, no roomy/iPad arrangement.
- Commits `839dabed`, `ef2108ae`, `302f3947` — patch-shaped "Phase 3" increments.
- `docs/design/device-adaptive-layout.md` — the plan that was only partially followed (finger
  budget, two-layer model, per-shape modes) and that explicitly deferred the taste calls.
- `tools/carts/acidfit.c` — the validated model that never graduated to a shared header.
- Maker's device play-test, 2026-07-05: "far from anywhere close [to] done."

---

## Implications

Incorporated into the revised Phase-3 plan in
[`device-adaptive-layout.md`](../design/device-adaptive-layout.md) §"Phase 3 — revised plan".
The generalisable rules:

1. **Graduate a prototype into a shared artifact *before* porting its lesson.** A model that
   lives only in a mock cart will be bypassed under momentum. If the port needs the model,
   the model must first become a header/tool the port can *call* (here: `disclose.h`).
   "Graft from `<prototype>.c`" is not an instruction; it's a hope.
2. **Write the layout brief before the layout code.** A half-page committed spec per rack —
   sections, priorities, footprints in fingers, arrangements per shape, pinned surfaces,
   orientation policy. Done = *matches the brief on device*, not "no audit findings".
3. **Give the judgment half of the bar a check too.** Defect oracles stay; add the judgment
   layer (the disclosure logic self-reports its per-shape decision; the audit flags degenerate
   outcomes like 0-open, sub-finger controls, a missing arrangement vs the brief). And keep the
   human check explicit: **bake it and put it in the maker's hands per arrangement**, not once
   at the end.
4. **Distrust "phase done" when the diffs don't match the phase's nature.** A design phase that
   produced only threshold tweaks hasn't happened yet. The shape of the work should match the
   shape of the plan.

---

## Open questions

- Can the brief itself be drift-tracked (the `seo-brief` palette/mirror pattern applied to
  layout), so a rack whose code diverges from its brief surfaces in `cart-status.js`?
- Where is the line between "brief" and over-specification — how much taste should be written
  down before it constrains instead of guides?
- Does the three-state strip (folded / compact / expanded, from the ReBirth study) generalise
  beyond rack carts to games' HUD/menu density?

---

## Related notes

- [015-repositories-can-learn](015-repositories-can-learn.md) — oracles + conventions as
  operational knowledge; this note adds their blind spot (absence).
- [016-knowledge-drift](016-knowledge-drift.md) — docs drifting from code; here the *work*
  drifted from a doc that was current.
- [017-context-nudges](017-context-nudges.md) — the right doc existed and was even cited in
  commit messages; citation is not adoption.
