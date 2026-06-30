> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
>
> Later notes may refine, extend or replace it.

003 — Curation over Classification
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
