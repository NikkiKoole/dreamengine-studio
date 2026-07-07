# Editor scopes & facets — where media lands

> **STATUS: direction chosen** (2026-07-05 explore → 2026-07-07 resolved) — the IA seam is now
> resolved into a **Make / Promote / Ship** model with a new per-cart **Promote tab** (see
> "Resolution" below); still an observation-and-direction, not a built feature. Origin: adding a
> "drop a `.rec` and replay it" affordance surfaced there's **no logical home** for per-cart media —
> and pulling that thread showed a seam in the editor's whole information architecture. The leads
> **📣 find tribes** glance (2026-07-07) hit the exact same wall (app-scoped only, no cart-scoped
> home), a third data point that confirmed the pattern and forced the resolution.
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
| **reach** | tribes + venues + gift-post scaffold (`leads.js`) | + ASO keywords/listing/score · press kit |

The cart-media panel (takes → replay / bake) and the app-media view (clips → reel → trailer)
would **share components** — a clip tile, a "bake" action, a drop target — but each appears
**wherever its subject is**. So the maker's gut ("a per-cart media panel, like code/sprites") is
right *and* incomplete: there's a per-**app** twin, and it already exists as the trailer builder.

## The deeper point

The editor's mental model is quietly shifting from *"an editor for the open cart"* to *"an editor
with a **scope** (cart | app) and **facets** within each scope."* **Apps was crack #1; media is
crack #2; reach (leads/ASO) is crack #3.** Three data points is more than enough to see the pattern
— and enough to want to *not* solve it ad-hoc a third time.

### Crack #3: reach (leads / ASO) — added 2026-07-07

The `leads.js` **📣 find tribes** glance shipped app-scoped only (it lives on the Apps card,
mirroring the ASO 📊 score glance). But the maker immediately felt the gap: *"we can only find
tribes for an app — this ends up being about **where in the editor** we get to these panels."* Same
tell as media — the panel exists, but its **cart-scoped home is missing**. And the reach facet is
**scope-asymmetric**, which sharpens the seam:

- **ASO** (keywords / listing / score) is **genuinely app-only** — only an app has an App Store
  listing to research and grade.
- **leads** (which tribe, which venue, a gift-post scaffold) is **genuinely both** — *every* gallery
  cart, app-bundled or standalone, has a tribe to post to. `leads.js match <cart>` is already
  per-cart on the CLI; only the editor surface is app-bound.

So the honest fix isn't "add a cart 📣 button" in isolation — it's the same scope×facet decision
below. A cart-scoped leads entry (on the open cart, or in the tutorials list) is the concrete v2,
but it should fall out of answering Q3, not precede it.

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

## Resolution (2026-07-07): Make / Promote / Ship — Promote is a new per-cart tab

A conversation working through the seam landed a direction. The key move: there are **three
verbs, not two** — the editor's per-cart facets group by *what you're doing*, and the old word
"media" was blurring the middle one with the other two:

- **Make** — author the cart: code · sprites · map · sound. *Changes the cart.* (the existing tabs)
- **Promote** — the promo/meta layer: the cart's recorded replays (`.rec`), baked clips, screenshots,
  its trailer (clips → reel), and its tribes (`leads`). A **new per-cart tab.** It's a
  **browse-and-assemble *library*, not a publish button** — "here are this cart's takes and clips;
  stitch two into a trailer; here are its shots and its tribes."
- **Ship** — the **existing share popup**: export the exe / web / `.app` binary, publish to
  github.io or the store. *Distributes the runnable.* **Promote is NOT this** — the share popup
  stays exactly what it is (it acts on the *binary* and fires a one-shot publish).

Why "rename pixels → media" nagged: pixels is **Make**, the trailer/shots/tribes are **Promote**,
the exe/publish is **Ship** — three drawers, and "media" was trying to be the middle one while
sitting in the Make column.

### The order: Promote *brackets* Ship (which is why it's a tab, not a step)

The intuitive lifecycle is **Make → Promote → Ship**, but it's not a clean line — **Promote sits on
*both* sides of Ship**:

```
Make → Promote(produce assets) → Ship(publish, using them) → Promote(announce, with the URL)
```

- **Before Ship** — promo assets are an *input* to Ship, not an output: the App Store **requires**
  screenshots to submit, and you'd cut the trailer first. So you produce in Promote, then Ship *uses*
  what you produced.
- **After Ship** — a tribe gift-post needs a **link to point at**, and that link (gallery URL / store
  page) doesn't exist until you've shipped. So the *posting* half of Promote loops back after Ship.

That's the load-bearing consequence: **Make and Ship are events** (author it once, publish it), but
**Promote is the workspace you keep returning to** — before shipping (assemble) and after (announce).
So it earns a **persistent tab**, not a wizard step wedged between Make and Ship.

**The verbs are a LENS, not a top-bar to build (right-sized 2026-07-07).** Don't reorganize the editor
into three Make/Promote/Ship buttons — that's wishful thinking. The verbs are for *deciding where a
thing lives*, and the answer is small and concrete:

- **Make** needs no button — it's *already* the **code / pixels (sprites/map)** tabs. Leave them flat;
  there's no "Make" wrapper.
- **Ship** needs no button — it's *already* the **share popup**. Leave it.
- **Promote is the only thing with no home today** → the single net-new UI element is a **Promote
  tab/button sitting next to code/pixels.** That's the whole build.

So the model's payoff is: *add one tab.* Everything else already exists under a different name.

This answers all three open questions below:

1. **Where does the cart-media facet live** → **its own per-cart Promote tab**, sibling to
   sprites/map. (Not inside the share popup — that's Ship, a different verb.)
2. **How does the app-scoped twin relate** → **apps consume cart-Promote data.** The app trailer
   stitches per-cart clips, app screenshots come from per-cart dumps, app leads aggregate per-cart
   tribes — this **already happens behind the scenes** (`build-app-reel.js`, `store-shots.js`, the
   📣 find-tribes glance). So the dependency is **bottom-up: carts produce promo assets; apps
   assemble them.** The Apps view *is* app-scoped Promote; the new tab is its per-cart **source**,
   which never had a visible home. Shared components (clip tile, bake action, leads row), two homes.
3. **Does the editor grow an explicit scope switch** → **no — scope is emergent.** A **cart is
   opened** (from the library → cart scope, the Promote tab), an **app is selected** (in the Apps
   view → app scope). You never *edit* an app — there's no single artifact to load into
   code/sprites/map; you edit its carts and *promote the set*. The subject you last touched *is* the
   scope. (Wrinkle, and it's correct not a bug: a cart can belong to an app — you still edit it as a
   cart and promote it as part of the app from the Apps view. Same cart, two lives, two places.)

**What lands in the cart Promote tab** (all data already on disk per [`cart-clips.md`](cart-clips.md)):
the cart's `.rec` takes + `replay()` (built), baked clips (the 🎬 make-clip browser from
[`share-panel.md`](share-panel.md)), screenshots, a per-cart trailer, and the cart-scoped **leads**
📣 entry ([`leads-marketeer.md`](leads-marketeer.md) open-q #4). Mostly a **view** problem, not
plumbing — the sources exist; they lack a home. **Full contents spec + MVP:
[`promote-tab.md`](promote-tab.md)** (one scrolling panel, sections in produce → assemble → reach order).

### The zeroth verb: Dream (the loop closes)

The three verbs have a **zeroth** — and the engine is named for it. The full arc is a **cycle, not
a line**:

```
Dream → Make → Promote → Ship → Promote ⟳ Dream (the next cart)
```

- **Dream** is not decoration — it's an **already-enforced phase**: the authoring firewall
  ([`../guides/cart-authoring-prompt.md`](../guides/cart-authoring-prompt.md)) makes you *imagine the
  ideal instrument/game from the genre BEFORE reading any existing cart*, and de:meta is written
  intent-first. "Dream before Make" is house discipline, not a slogan.
- **Dream is the one verb that is NOT a tab.** You don't dream *inside* a cart — dreaming is the
  pre-work: your head, the design docs, and browsing the **compendium / gallery** for sparks ("what's
  possible? what hasn't been done?"). So the editor's *cart-independent* views (Docs, tutorials,
  gallery) are, loosely, the Dream surface. This fits the scope model: **Dream is scope-less** (about
  the whole space of possible carts), **Make/Promote are cart-scoped**, **Ship / app-Promote are
  app-scoped**.
- **The last Promote loops to Dream:** you announce, the tribe reacts, and that feeds the next idea.
  The *engine* in dreamengine is the thing that turns the dream all the way around to the next dream —
  which ties this whole IA back to the [VISION](../VISION.md).

## Open questions (now resolved above — kept for the trail)

1. **Where does the cart-media facet live** — its own per-cart panel/tab (like sprite/map), or a
   section inside the existing share popover? (Share is about *distributing*; media is about
   *producing* — record → replay → bake → assemble. That tension is the crux.)
   → **RESOLVED: its own per-cart Promote tab; the share popup is Ship, a separate verb.**
2. **How does the app-scoped twin relate** — is the trailer builder re-housed under one "media"
   umbrella with the cart panel, or do they stay separate views that share components?
   → **RESOLVED: separate homes, shared components; apps consume cart-Promote data bottom-up.**
3. **Does the editor grow an explicit scope switch** (cart | app), or does each facet just know
   its own scope? (i.e. is "scope" a real first-class concept in the UI, or an emergent one?)
   → **RESOLVED: emergent — a cart is opened, an app is selected; the last-touched subject is the scope.**
