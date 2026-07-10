# 021 — A doc's STATUS label can't be linted against its body

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-11

**Confidence**
High

---

## Observation

A design doc has two truths: the one-line `STATUS:` label at the top (the only
part `tools/doc-status.js` parses, so it drives the design board) and the body.
They drift apart in one direction: work lands, someone updates the body's
`✅ SHIPPED` markers, and the STATUS line is left saying `DESIGNED`. The doc then
sits on the board's READY-TO-BUILD backlog long after it shipped.

This bit three docs at once — `piano-engine`, `stereo`, `audio-threading` were all
labelled `DESIGNED`/pre-shipped while their bodies recorded the work as finished
(a specific instance of the general [016 — Knowledge Drift](016-knowledge-drift.md)).
Fixing the labels was trivial. The tempting next move — a lint that flags "STATUS
says not-done but body reads done" — is what this note is about: **it was built,
tested, and abandoned as unbuildable with the signals available.**

---

## Why this matters

It's a clean worked example of *when NOT to build the obvious tool*. The failure
is not a tuning problem you can grind away — it's semantic, so no amount of regex
refinement fixes it. Recognising that shape early saves the next agent from
re-attempting it (and from shipping a linter everyone learns to ignore, which is
worse than no linter — see [018](018-passing-the-gates-felt-like-done.md) on
mistaking a green check for the real bar).

---

## Evidence

Empirical, against the live corpus (~60 design docs) — two failing directions:

1. **Count `✅`/done markers vs open markers, flag "all done, zero open."**
   Recall was ~0. Mature *shipped* docs legitimately carry "remaining / next /
   risks / optional-polish" prose, so the real cases (`piano` had "Remaining
   polish (optional)", `stereo` bundled the next effects layer, `audio-threading`
   listed iOS risks) all had `open > 0` and were missed. Loosening the gate to
   catch them then false-positived on big *partial* plans (a 15-lesson tutorial
   curriculum with 2 open items reads "mostly done").

2. **Fire only on an explicit "whole deliverable done" declaration** (an
   `ALL SHIPPED` roadmap heading, "all N stages shipped"). On the current repo:
   **4 flags, 4 false positives, 0 real**, and recall on the three known cases was
   1/3 (caught `piano`'s "Fix roadmap — ALL SHIPPED", missed the other two, which
   have no summary line — they're done only because every bullet is individually
   `✅`).

The false positives reveal the core obstruction. A regex cannot tell:
- **whole vs part** — `piano`'s "Fix roadmap — ALL SHIPPED" (real) is the same
  shape as `trailer-builder`'s "slices 1–4 all shipped" (one sub-feature of an
  actively-building doc);
- **subject flip** — `sound-next-steps`' "all shipped, so these are buildable
  today" means the *prerequisites* are done, i.e. the doc is *not* done;
- **checklist vs prose** — `worldgen-plan`'s `[x]` checklist was 7/7 complete while
  its prose said "remaining / deferred / still open".

The distinguishing fact is the *referent* of the completion phrase, which is
exactly what surface-pattern matching can't recover.

---

## Implications

- **Keep the human board-read as the mechanism.** It's cheap: skimming
  `node tools/design-board.js` and comparing each pre-shipped doc's label to its
  body cleared all five remaining READY docs in one pass. Do it when the board
  feels stale, not on a schedule enforced by a gate.
- **Write STATUS lines lead-with-the-phase** (`doc-status.js` already documents
  this: a leading phase word wins). That's the cheap prevention — the drift is
  authored in, so the fix is an authoring habit, not a detector.
- **General lesson:** when a would-be lint's decision hinges on *what a phrase
  refers to* rather than *whether a token is present*, it's a judgement call, not
  a check. Leave it to the periodic human read and don't ship the noise.

---

## Open questions

- Would an LLM-judge pass (read STATUS + body, "does the label match?") clear the
  bar where regex can't? Plausibly yes — it's a semantic call — but it's not free
  or deterministic, so it only earns its place if status-drift becomes frequent
  rather than the N=3 it is today. Revisit only if it bites again.

---

## Related notes

- 016-knowledge-drift
- 017-context-nudges
- 018-passing-the-gates-felt-like-done
