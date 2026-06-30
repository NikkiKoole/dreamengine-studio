# 006 — The Evolution of the Workflow

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

DreamEngine's workflow did not evolve primarily by adding more AI.

Instead, it evolved by progressively removing recurring decisions from future work.

Whenever a decision proved valuable repeatedly, it was promoted into the system.

The result is not simply faster development.

It is a workflow that gradually preserves its own knowledge.

---

## Why this matters

Many software projects attempt to automate repetitive work.

DreamEngine appears to go one step further.

It attempts to eliminate the need to repeatedly make the same design decisions.

Examples include:

* API promotion.
* Verification tools.
* Per-cart metadata.
* Generated indexes.
* Generated compendiums.
* Context assembly (planned).
* Field notes.

Each reduces future decision-making rather than merely reducing typing.

---

## Workflow evolution

The workflow increasingly resembles the following cycle:

```text
Curiosity
    ↓
Direction
    ↓
Experiment
    ↓
Agent implementation
    ↓
Verification
    ↓
Knowledge capture
    ↓
Promotion into the system
```

Rather than ending with code, the workflow ends with improved capability for future work.

---

## Two interacting feedback loops

DreamEngine appears to evolve through two reinforcing loops.

### Creative loop

```text
Curiosity
    ↓
Experiment
    ↓
Understanding
    ↓
New curiosity
```

This loop expands the project's creative horizon.

---

### Workflow loop

```text
Friction
    ↓
Tool
    ↓
Reduced friction
    ↓
Larger experiments
    ↓
New friction
```

This loop expands the project's production capability.

Each loop strengthens the other.

---

## Implications

The rapid growth of DreamEngine is unlikely to be explained by AI alone.

AI accelerated development, but the sustained increase in velocity appears to come from continuously capturing improvements into the workflow itself.

The workflow becomes progressively more capable after each solved problem.

---

## Open questions

Which decisions should always remain human?

Current candidates include:

* curiosity
* taste
* direction
* long-term vision
* changing the North Star

Everything else may gradually become part of the system.

---

## Related notes

* 000-review-process
* 001-protocol
* 002-context-assembly
* 004-roads-as-convergence-layer
* 005-evolution-through-inflection-points
