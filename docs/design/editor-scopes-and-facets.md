# Editor scopes & facets — where media lands

> **STATUS: exploring** (2026-07-05) — an observation captured so it isn't lost, NOT a plan.
> No decision here. Origin: adding a "drop a `.rec` and replay it" affordance in the editor
> surfaced that there's **no logical home** for per-cart media yet — and pulling that thread
> showed a seam in the editor's whole information architecture.
>
> - "What's shipped / open / cut?" → [`../STATUS.md`](../STATUS.md).
> - Where clip/reel FILES live on disk (the storage layout) → [`cart-clips.md`](cart-clips.md).
>   This note is the complementary half: where the media UI lands in the editor.

---

## The seam

The editor's spine has always been **"the currently-open cart."** Three editors follow it —
**code / sprites / map** — and two views are cart-independent — **docs / settings**. Then two
things bolted a second, *group* scope onto that single-cart spine:

- **Apps** (newish) — a view over `apps/<name>/app.json` manifests: small **groups of carts**
  ([`build-app.js`](../../tools/build-app.js) builds one into a binary).
- **Media** (now knocking) — recording a `.rec` take, replaying it, baking it into a clip,
  stitching clips into a reel/trailer.

Media is the thing with no home. And the tell is that it's **not cleanly per-cart OR per-app**:

- a `.rec` take and a single clip are **cart-scoped**;
- a **reel / trailer is inherently cross-cart** — a `.reel` stitches clips from *several* racks.
  The [trailer builder](trailer-builder.md) is, in effect, **already app-scoped media** — shipped,
  but not framed as "media."

## The reframing: media is a FACET, not a new sibling

"Media" is not a third sibling next to code/sprites/map. It's a **facet** — like "code" is —
that simply **exists at both scopes** the editor already has:

| facet | cart scope | app / group scope |
|---|---|---|
| **code** | code · sprites · map | (the manifest — apps view) |
| **media** | takes (`.rec`) · clips · shots | reels/trailers · store screenshots · press kit |

The cart-media panel (takes → replay / bake) and the app-media view (clips → reel → trailer)
would **share components** — a clip tile, a "bake" action, a drop target — but each appears
**wherever its subject is**. So the maker's gut ("a per-cart media panel, like code/sprites") is
right *and* incomplete: there's a per-**app** twin, and it already exists as the trailer builder.

## The deeper point

The editor's mental model is quietly shifting from *"an editor for the open cart"* to *"an editor
with a **scope** (cart | app) and **facets** within each scope."* **Apps was crack #1; media is
crack #2.** Two data points is enough to see the pattern — and enough to want to *not* solve it
ad-hoc a second time.

## What exists today (so a plan starts rooted)

- **The replay plumbing is built** — `studio:replay` IPC + the `replay()` bridge (`main.cjs` /
  `preload.cjs` / `shell.js`). The *trigger* is a rootless stopgap: **drop a `.rec` anywhere on
  the editor** → it replays against the open cart (with a warning toast if the take's folder names
  a different cart). Whatever media UI we build reuses `replay()` unchanged — the drop target is
  swappable, so this isn't wasted.
- **The 🎬 make-clip button** is already sketched as a clip *browser and trigger* in
  [`share-panel.md`](share-panel.md) — that's the cart-scoped media panel under another name.
- **Storage layout is locked** — [`cart-clips.md`](cart-clips.md): takes at
  `tools/clips/<cart>/NN-*.rec`, baked clips at `editor/public/clips/<cart>/`, reels at
  `tools/reels/*.reel` → `editor/public/reels/`.
- **The cart-authoring hand-editing gaps** live in [`editor-cart-workflow.md`](editor-cart-workflow.md).

## Open questions (deliberately unanswered)

1. **Where does the cart-media facet live** — its own per-cart panel/tab (like sprite/map), or a
   section inside the existing share popover? (Share is about *distributing*; media is about
   *producing* — record → replay → bake → assemble. That tension is the crux.)
2. **How does the app-scoped twin relate** — is the trailer builder re-housed under one "media"
   umbrella with the cart panel, or do they stay separate views that share components?
3. **Does the editor grow an explicit scope switch** (cart | app), or does each facet just know
   its own scope? (i.e. is "scope" a real first-class concept in the UI, or an emergent one?)
