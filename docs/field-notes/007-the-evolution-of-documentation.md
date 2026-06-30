# 007 — The Evolution of Documentation

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-06-26

**Confidence**
High

---

## Observation

DreamEngine's documentation appears to evolve from describing the engine toward describing how decisions should be made.

As the project matures, documentation shifts from implementation to intent.

Rather than merely explaining *what* exists, it increasingly explains *why* it exists and *how future decisions should be made*.

---

## Why this matters

Documentation is no longer a passive record of the project.

It becomes an active part of the development process.

Rather than supporting development from the outside, it helps guide the evolution of DreamEngine itself.

This changes documentation from reference material into part of the project's decision-making system.

---

## Evolution

The documentation appears to move through several stages.

### Implementation

Early documentation primarily explains:

* APIs
* engine behaviour
* features
* implementation details

---

### Design

Later documents increasingly discuss:

* abstractions
* engine boundaries
* trade-offs
* design decisions

---

### Governance

Recent documents focus on questions such as:

* When should something become API?
* What belongs inside the engine?
* How should agents collaborate?
* What is the project's North Star?
* How should knowledge evolve?

The documentation begins to describe principles rather than mechanisms.

---

## Promotion of knowledge

Documentation itself appears to follow the same promotion philosophy seen elsewhere in DreamEngine.

Small ideas gradually become larger project artifacts.

For example:

```text
Observation
    ↓
Design note
    ↓
ADR
    ↓
Workflow
    ↓
Project philosophy
```

Rather than documenting finished conclusions, the repository captures progressively more refined understanding.

---

## Implications

The documentation increasingly preserves intent instead of facts.

Facts change as the implementation evolves.

Intent remains valuable for much longer.

This explains why newer documents increasingly resemble research papers rather than traditional software documentation.

The documentation becomes part of the engine's long-term memory.

---

## Open questions

* When should implementation details be promoted into design principles?
* How should evolving understanding be distinguished from stable documentation?
* How should field notes eventually feed back into the project's knowledge system?

---

## Related notes

* 000-review-process
* 001-protocol
* 002-context-assembly
* 005-evolution-through-inflection-points
* 006-the-evolution-of-the-workflow
