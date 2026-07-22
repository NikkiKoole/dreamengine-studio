// collections-vocab.js — the single source of truth for the COLLECTION vocabulary: the
// cross-cutting RESEARCH THREADS a cart belongs to (the axis kind/genre/teaches can't reveal).
//
// A collection is orthogonal to kind/genre: a road cart is `kind:tech-demo, genre:racing`, a
// tinyjam rack is an `instrument`, yet both belong to a project/experiment FAMILY that cuts
// across the medium. That family is the thing you browse by ("show me the road stuff", "the
// radio stations") and it is ALWAYS a "sprawling experiment that has a doc" — so every
// collection is ANCHORED to a doc (the "read more" home). This is the still-open half of
// field note docs/field-notes/003-curation.md ("curated collections… still open").
//
// The vocabulary is defined ONCE here. Everything else derives from it:
//   • COLLECTIONS ({slug,title,doc,blurb}[]) — the registry.
//   • COLLECTION_SLUGS (Set) — used by lint-carts.js to hard-validate each cart's collection[].
//   • lint-carts.js also asserts every `doc` here EXISTS on disk (a dead anchor is a hard error).
// The per-cart membership lives IN each cart's de:meta `collection[]` (emitted to index.json by
// build-cart-index.js) — NOT in a list here, so a cart owns its own membership and it can't drift.
//
// To add a collection: it must be a real thread with a doc home. Add a row below (slug =
// kebab-case, title = human label, doc = repo-relative path that exists, blurb = one line).
// That single edit makes the slug valid (lint), nameable (collections.js), and linkable
// everywhere at once. Adding one is a deliberate, reviewable change — an off-vocabulary slug is
// a hard lint error, so the list grows on purpose, not by casual coinage. Keep it disciplined:
// a collection is a thread MANY carts share, not a home for a one-off.

const COLLECTIONS = [
  {
    slug: "road",
    title: "Driving world / roads",
    doc: "docs/design/driving-world-program.md",
    blurb: "the infinite driving world — worldnet spine, roadkit junction geometry, procedural city grammar.",
  },
  {
    slug: "tinyjam",
    title: "Tinyjam racks",
    doc: "docs/design/tinyjam-racks.md",
    blurb: "the multi-rack music app family — device-faces bundled into one shippable jam.",
  },
  {
    slug: "radio",
    title: "Radio stations",
    doc: "docs/guides/radio-station-howto.md",
    blurb: "seeded-song radio chassis (radio.h/improv.h/solo.h) — genre stations you tune and solo over.",
  },
  {
    slug: "responsive",
    title: "Device-adaptive / responsive",
    doc: "docs/design/device-adaptive-layout.md",
    blurb: "carts that live-reflow to the window (resizable + lay.h) instead of a fixed canvas.",
  },
  {
    slug: "physics",
    title: "Verlet physics playthings",
    doc: "docs/design/physics-notes.md",
    blurb: "carts built on the shared verlet toolkit (physics.h) — ropes, cloth, ragdolls, blobs, fluids.",
  },
  {
    slug: "device-face",
    title: "Device-face paradigm",
    doc: "docs/design/device-face-paradigm.md",
    blurb: "a whole cart IS one physical device's face — a playable instrument/tool front panel.",
  },
  {
    slug: "chord-bloom",
    title: "Chord-bloom",
    doc: "docs/design/bossa-rack.md",
    blurb: "the harmony-IS-the-instrument thread — one genre band brain behind three verbs: read (chordwise), play (chordblossom2), write (bandbox); band.h extraction pending.",
  },
];

const COLLECTION_SLUGS = new Set(COLLECTIONS.map(c => c.slug));

module.exports = { COLLECTIONS, COLLECTION_SLUGS };
