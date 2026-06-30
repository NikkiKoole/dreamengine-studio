# 004 — Roads as a Convergence Layer

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

The road work began as rendering and movement experiments but has gradually become one of the primary convergence points within DreamEngine.

Roads are no longer simply something to draw or drive on.

They have become the substrate on which multiple independent systems meet.

---

## Why this matters

The road domain now combines several previously separate areas of research:

* procedural generation
* computational geometry
* OpenStreetMap integration
* data import pipelines
* rendering
* city generation
* future traffic simulation
* future pedestrian systems

Rather than representing a single feature, roads appear to be evolving into the first shared world representation.

---

## Evidence

The road-related carts appear to evolve roughly through several phases:

1. Roads as movement.
2. Roads as procedural geometry.
3. Roads as geometry research.
4. Roads as external data.
5. Roads as the substrate for living cities.

The introduction of OpenStreetMap import marks an especially significant transition, as DreamEngine begins consuming real-world data rather than generating everything internally.

---

## Implications

Roads may no longer be the real abstraction.

The deeper abstraction may be **networks**.

Roads become the first example of a general pattern that may later include:

* pedestrians
* trains
* rivers
* utilities
* logistics
* social networks
* missions

Roads therefore represent more than graphics or gameplay—they represent connected systems.

---

## Open questions

* Should roads remain their own subsystem?
* Is the real abstraction "network"?
* Which future systems naturally attach themselves to road graphs?

---

## Related notes

* 002-context-assembly
* 003-curation
* 005-evolution-through-inflection-points
