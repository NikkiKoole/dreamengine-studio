# 014 — Outside-Agent Brainstorms as a Knowledge Source

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-06-29

**Confidence**
Medium–High

---

## Observation

A new knowledge input has appeared and proven itself repeatedly: long brainstorming conversations
held with an *external* AI (Gemini, ChatGPT), exported and **distilled into the repository** — not
pasted in.

The valuable act is not capturing the conversation verbatim. It is the distillation: strip the
model's filler, map its loose "vibes" onto actual codebase ground truth, dedupe against documents
that already exist, flag unverified claims, and record only the *delta* — what is genuinely new or
what the conversation independently confirmed.

---

## Why this matters

It extends the knowledge lifecycle already described in this journal (`README.md`'s Code↔Knowledge
table). The "Conversation" rung now explicitly includes **external-AI conversations**, and they
behave differently from in-house discussion:

- The outside agent supplies *divergent* vibes — names, market framings, adjacent ideas a
  codebase-anchored agent might not surface.
- But it has no ground truth. Mapping vibes to the real engine, and sanity-checking its facts, is
  ours. (`tinydaws.md` already named this "firewall": outside agent supplies vibes, mapping is ours.)

So the input is valuable precisely *because* it is uninformed about the repo — and dangerous for the
same reason. Distillation is the membrane between the two.

---

## Evidence

This session (2026-06-29). Two exported Gemini conversations —
*"A Guide To Pocket Operators"* and *"Database Solutions for GitHub Pages"* — became three
`docs/design/` artifacts:

- `tinydaws-followup.md` (bias knobs, the agentic RLHF loop, the visualizer)
- `product-notes-followup.md` (save/share codec, native iOS, AUv3, monetization)
- `action-plan.md` (a tiered, checkable synthesis across these + the field-notes wishlist)

The same distillation pattern repeated three times, and the raw transcripts were deliberately *not*
committed — only the distilled deltas.

---

## Implications

- **The transcript is not the artifact.** The distilled delta is. Raw exports stay external/scratch.
- A brainstorm-derived doc should always: credit the source, separate "independently confirmed" from
  "genuinely new," map each idea to repo ground truth, and mark LLM-supplied facts as unverified.
- This is itself a candidate for tooling — an "ingest a brainstorm export → draft a delta doc" step
  — which overlaps the knowledge-index idea in `distillator.md` and `tools-we-need.md`.
- Watch for over-capture. Not every external conversation earns a doc; the Promotion Principle
  (`009-the-negative-space-of-dreamengine`) applies to imported ideas too.

---

## Open questions

- Where is the line between a **design-doc delta** (`docs/design/`, the *idea*) and a **field note**
  (here, a *change in understanding*)? This session's split: the product/design ideas went to
  `docs/design/`; this workflow realization went here. Is that the durable rule?
- Should brainstorm transcripts be archived in-repo at all, or always stay external?
- Does the same membrane apply to *any* imported knowledge (papers, other projects), or is the
  external-AI case special?

---

## Related notes

- 007-the-evolution-of-documentation
- 008-the-identity-of-dreamengine
- 009-the-negative-space-of-dreamengine
- 100-first-synthesis
