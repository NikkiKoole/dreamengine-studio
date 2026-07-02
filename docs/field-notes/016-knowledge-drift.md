# 016 — Knowledge Drift

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Incorporated

**Date**
2026-07-02

**Confidence**
High

---

## Observation

As repositories grow, the greatest threat is not missing knowledge.

It is knowledge that exists in multiple places and slowly diverges.

Metadata becomes separated from the artifacts it describes.

TODOs migrate into separate files.

Indexes require manual updates.

Design documents continue to describe an earlier version of the system.

Over time, every duplicate representation becomes another opportunity for drift.

---

## Discussion

Many attempts to improve documentation focus on creating more documentation.

The more important problem is reducing the number of places where the same knowledge must be maintained.

Every artifact should own the information that is fundamentally about itself.

A cart should describe itself.

Metadata should live with the cart.

TODOs should live with the cart.

Design documents should describe designs, not duplicate implementation details.

Global views should be generated from these local sources rather than edited directly.

This reduces coordination between contributors and removes shared files that become merge hotspots.

Structured metadata also provides a stable interface for tooling and AI agents without requiring fragile parsing of free-form text.

---

## Implications

When information belongs to one artifact, it should have one authoritative home.

Everything else should be derived.

Generated indexes are preferable to manually maintained indexes.

Repository-wide summaries should be built rather than edited.

Automation should discover information instead of asking contributors to duplicate it.

Reducing duplication is often more valuable than writing additional documentation.

---

## Open Questions

* Which information genuinely belongs to an artifact?
* Which global views should always be generated?
* Where is duplication still unavoidable?
* How can repository tooling continue to reduce knowledge drift without becoming intrusive?

---

## Related notes

- 002-context-assembly
- 007-the-evolution-of-documentation
- 012-self-describing-artifacts
- 013-generated-cart-index
- 017-context-nudges

---

## Outcome

This observation led to the introduction of `de:meta` blocks, generated cart indexes, per-cart TODO ownership, and tooling that derives repository-wide views from local truth.

The immediate motivation was parallel-agent merge conflicts around shared files.

The longer-term result is a repository whose knowledge is increasingly owned locally and assembled globally.
