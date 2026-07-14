> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
>
> Later notes may refine, extend or replace it.

003 — Curation over Classification

**Status**
Working Theory
**Date**
2026-06-30
Observation

As the number of carts grows, the problem is no longer creating metadata.

The problem is using metadata to present the collection in meaningful ways.

Adding more tags alone is unlikely to improve discoverability.

Instead, the project should increasingly derive views from existing metadata.

Motivation

The collection has grown beyond the point where a flat list is useful.

Different audiences require different entry points.

Examples include:

first-time visitors
learners
engine development
historical reference

A single list cannot serve all of these equally well.

Direction

Rather than introducing many additional tags, improve curation.

Suggested evolution:

Introduce a simple lifecycle status.
active
showcase
retired
archive
hidden
Build curated collections.

Examples:

Showcase
Games
Tutorials
Audio
Procedural
Archive
Introduce explicit relationships.
replaces:
successor:
related:

Relationships are considered stronger than tags.

Generate multiple views from the same metadata.

Examples include:

Best of DreamEngine
Tutorials
Related carts
Historical evolution

The metadata should describe the project.

The generated views should describe the experience.

Retirement

Retiring a cart does not imply failure.

A retired cart has completed its purpose.

Its ideas now live more effectively elsewhere.

Relationship to existing philosophy

This follows DreamEngine's recurring pattern:

avoid manually maintaining multiple representations
maintain one source of truth
derive useful views automatically

The same philosophy already appears in:

generated indexes
planned per-cart metadata
compendium generation
API promotion

Curation is another derived representation.

Open questions
Should retirement become part of the cart lifecycle?
How should relationships be visualized?
Should generated collections become part of the website?
Confidence

High.

The need for curation naturally follows from the current size of the project.

---

## Outcome (2026-06-30)

The lifecycle-status + curation-over-classification direction shipped at the metadata layer: `tools/lint-carts.js` defines and validates the exact vocabulary this note proposed (`active/showcase/retired/archive/hidden`) and `docs/design/cart-metadata.md` documents it; 418 carts now carry a `status`, and the "derive views from one source of truth" philosophy is realized broadly (generated `index.json`, compendium, design-board, reflections). Still open: the curation payoff — `replaces`/`successor` relationship fields and the generated curated collections (Showcase, Best-of, Related) — so this stays a working theory.

**Update (2026-07-14) — the *collections* half shipped.** The "curated collections" idea is now a
first-class, **doc-anchored** per-cart field: `collection[]` in each cart's de:meta, with the
vocabulary + doc anchors owned by [`tools/collections-vocab.js`](../../tools/collections-vocab.js)
(schema: [`cart-metadata.md`](../design/cart-metadata.md#collection-doc-anchored-cross-cutting-threads)).
Every collection slug points at a design/guide doc — these threads are always "a sprawling
experiment that has a doc," so the collection *is* the browsable face of that doc. Seeded with
`road / tinyjam / radio / responsive / physics / device-face` (62 carts tagged); `lint-carts.js`
hard-validates slugs + asserts each anchor doc exists; [`tools/collections.js`](../../tools/collections.js)
is the roll-up view and `build-context.js` shows a cart's threads when you go cold on it. **Still
open** (the *other* half of this note): the cart-to-cart *relationship* fields
(`replaces`/`successor`/`related`) and the richer generated views (an editor gallery filter, a
`docs/collections.html` ★ page).
