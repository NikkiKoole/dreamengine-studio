# 017 — Context Nudges

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Incorporated

**Date**
2026-07-02

**Confidence**
Medium

---

## Observation

Documentation often becomes stale not because people forget it exists, but because they are focused elsewhere when making changes.

The challenge is rarely finding documentation.

The challenge is remembering which documentation has become relevant.

---

## Discussion

Traditional documentation systems rely on contributors manually remembering to update related documents.

As repositories grow, this expectation becomes unrealistic.

Instead of enforcing documentation through rigid rules, DreamEngine increasingly uses context assembly and lightweight markers to bring relevant documents back into an agent's working context.

For example, a design document may indicate that certain values are expected to drift over time.

When an agent modifies a related cart, the context-building tools surface that document and encourage the agent to review it.

No attempt is made to automatically rewrite the document.

Instead, the repository provides a timely reminder while the relevant context is already active.

This approach works particularly well with AI agents.

Free-form design documents remain expressive, while small amounts of structured metadata make them discoverable at the right moment.

The goal is not perfect synchronization.

The goal is reducing accidental drift.

---

## Implications

Repositories should not only store knowledge.

They should actively help contributors remember what is likely to need attention.

Small contextual nudges are often more effective than strict validation.

Where documentation cannot be formally checked, it can still be surfaced at the right moment.

Context assembly therefore becomes more than file discovery.

It becomes a mechanism for keeping related knowledge alive during active work.

---

## Open Questions

* Which kinds of documents benefit most from contextual nudges?
* Which markers are useful without becoming noisy?
* How can context assembly balance completeness against cognitive overload?
* Which repository relationships should eventually become machine-discoverable?

---

## Related notes

- 002-context-assembly
- 007-the-evolution-of-documentation
- 011-tool-discovery
- 016-knowledge-drift

---

## Outcome

The original context tools evolved beyond gathering files for a task.

They now use lightweight document markers to identify information that may have become stale and encourage agents to revisit it during related work.

Rather than enforcing documentation updates, the repository increasingly guides contributors toward maintaining consistency through context-aware reminders.
