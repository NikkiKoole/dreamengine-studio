# 0020 — dreamengine is an in-house tool with a curated public showcase, not a cart-making platform
Date: 2026-06-22 · Status: accepted

## Context
A standing open question (VISION "Sharing the work", STATUS item 10/11) was *what shape*
the public surface should take: itch.io, a one-click publish, a dreamengine-hosted gallery
— and, implicitly, whether outsiders should ever **author** carts (accounts, sharing, an
in-browser editor that compiles strangers' C). URL-sharing of *finished* carts shipped
(the GitHub Pages gallery, 2026-06-05), but the bigger "is this a platform?" question stayed
unanswered, and two built-but-unadopted features ([`cart-clips`](../design/cart-clips.md),
[`attract-mode`](../design/attract-mode.md)) were stalled on it — they're venue *amplifiers*
with no agreed venue to amplify.

## Decision
dreamengine is an **in-house creative tool** — the maker, Claude, and the maker's children
make carts together, with the tools that already exist. The public surface is a **curated
showcase**: a hand-picked subset people *view* (clips) and *play* (wasm), never a place they
*contribute to*. No creator accounts, no cart sharing/upload from outsiders, no
in-browser-editor-for-others, no compile-strangers'-C build server. People watch and play;
they don't make.

Two corollaries the maker called explicitly:
- **Curation, not the full index.** The public window is a chosen subset (the best 20–40),
  not all ~400 carts. Carts can all stay *live/published*; "published" ≠ "featured."
- **Both watch and play.** Clips are the come-on (work everywhere, phone-friendly, no
  autoplay-audio fuss); playable wasm is the payoff (the "it's a real little console" beat).

## Why
- The "people make carts" goal is the entire source of platform weight — accounts,
  moderation, sharing, discoverability *for creators*, an in-browser toolchain. Dropping it
  deletes the hardest, riskiest half of the roadmap and leaves a *showcase*, which the GH
  Pages gallery + wasm builds already are in skeleton.
- It matches how the tool is actually used and most enjoyed (in-house, with Claude and the
  kids) rather than a hypothetical creator audience that doesn't exist.
- It makes the parked features make sense: clips are the cheap, safe way to make the gallery
  we already publish stop reading as a file-listing; attract-mode's web embed (clip → "press
  start" → play) is the natural top of the *same* showcase ladder, no longer speculative.

## Consequences
- **Clips are the prioritized cheap win.** Bake the committed tracks (`--all`), wire the
  gallery/history page to show carts moving. Authoring demo tracks now has a visible payoff.
  (See [`cart-clips.md`](../design/cart-clips.md).)
- **Attract mode keeps its sequencing** (native prototype first), but its web-embed payoff is
  now a real target, not a parked maybe. (See [`attract-mode.md`](../design/attract-mode.md).)
- **Curation needs a mechanism** — a "featured" flag / front-page subset distinct from the
  full published index. Not built; the first concrete task this decision creates.
- **Cut for good** (don't relitigate): creator accounts, outsider cart upload/sharing, a
  public compile server, a browser IDE for other people. The personal/trusted build server
  in [`guides/sharing.md`](../guides/sharing.md) and goal B in
  [`design/cart-as-script.md`](../design/cart-as-script.md) are *in-house* conveniences, not
  steps toward a public platform.
- **Not in scope of this decision:** the tinyjam racks *product* line
  ([`design/product-notes.md`](../design/product-notes.md)) — selling finished, standalone
  music apps is a different question (a product made *with* the tool, not opening the tool to
  others) and is unaffected.

> Where this decision sits in the wider picture: [`design/sharing-channels.md`](../design/sharing-channels.md) maps all three sharing channels (this ADR governs channel A, the web showcase).
