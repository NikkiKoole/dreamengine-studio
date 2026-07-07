# The Promote tab — a cart's media bin + mini press-kit, in the editor

> **STATUS: ready** (2026-07-07) — specced, nobody on it yet. The concrete build that falls out of
> the [`editor-scopes-and-facets.md`](editor-scopes-and-facets.md) resolution ("Make / Promote /
> Ship" — Promote is the one verb with no home today). This doc is *what the tab contains*; that doc
> is *why it exists*.

> **One-line pitch.** A per-cart tab that gathers everything you'd use to get this cart *seen* —
> replays, clips, stills, a trailer, and its tribes — as a **browse-and-assemble library**, not a
> publish button. (Publishing the binary is **Ship** = the share popup, a different verb.)

## Shape: one long scrollable panel (like the Apps view)

Not sub-tabs. **One vertical scroll**, sections top-to-bottom in the natural **produce → assemble →
reach** order (which is also the [Promote-brackets-Ship](editor-scopes-and-facets.md) order: you
produce assets *before* Ship, you reach out *after* it). It reads as a story you scroll down, the
same way an app card expands into a stack of actions. If a section gets dense later, it can collapse
(a `<details>`), but the default is everything visible in one scroll.

Scope: **the currently-open cart** (scope is emergent — see the scopes doc). The app-scoped twin of
each section already lives in the **Apps view** (trailer builder, store screenshots, the 📣
find-tribes glance); this tab is the per-cart **source** those aggregate from, bottom-up.

## The sections (top → bottom)

### A · Clips & replays — the raw material
- **Takes**: the cart's committed input tracks `tools/clips/<cart>/*.{rec,script,beats}` — each row
  a label + a ▶ that fires `studio:replay` (**built**) + delete. These are deterministic seeds.
- **● Record**: capture a new take live (**built** — the record button from the editor-media lane).
- **Baked clips**: a thumbnail grid of `editor/public/clips/<cart>/*.{webm,gif,mp4}` — play, show the
  recipe it came from, re-bake, delete.
- **Bake a take → clip**: run `make-gif.js --recipe <NN-label>` on a take (webm/gif/9:16 export). The
  "produce" verb made concrete — a deterministic replay becomes a shareable clip.

### B · Stills — a frame worth posting
- "Snapshot current frame" (the live `play.js --dump` / screenshot-request bridge already used for
  diagnosis) → a small gallery of stills.
- *Out of scope here:* device-sized **App Store** screenshots (`store-shots.js`) stay app-scope. This
  is just "a nice still / a loop for a post," not the 6.9″ store render.

### C · Trailer — assemble
- Stitch *this cart's* clips into a short reel (`compose-clips.js`) — the cart-scale sibling of the
  app trailer builder (`build-app-reel.js`). Reorder, pick a transition at the join, build → preview.

### D · Reach / tribes — the 📣, cart-scoped
- Reuse `renderLeads` pointed at the open cart: its tribes + clickable venues (open in browser) + the
  copyable **post scaffold**. It's the exact glance already on the Apps card, aimed at one cart.
  Enabler exists: `leads.js match <cart> --json`.
- *(v2)* a free-form **discover** box for on-the-fly venue hunting.

### E · The link + outreach — the "after Ship" hinge
- **Gallery URL**: if the cart is published, show it with a copy button (paste it into a tribe post);
  if not, a "publish it first →" nudge pointing at **Ship** (the share popup). This is the
  Promote-brackets-Ship seam made visible — a gift-post needs a live link, which only Ship produces.
- **Outreach board**: this cart's `leads track` rows (○ todo / ● posted / ★ responded) — where you've
  posted and what came back.

## MVP — mostly wiring existing pieces into one panel

The cheap, high-value slice (all reuse what's built): **D (tribes glance) + A (takes list + replay +
record + a clips list) + E's gallery link**. That alone delivers the whole Promote loop in miniature
— *here's my cart's stuff · here's where to post it · here's the link*.

| section | build cost | why |
|---|---|---|
| **D reach** | cheap | `renderLeads` + `match --json` already exist; point at one cart |
| **A takes/replay/record** | cheap | `studio:replay` + record button already built |
| **E gallery link** | cheap | URL/state from `cart-status.js`; outreach from `leads track` |
| **A bake take → clip** | medium | glue to `make-gif.js --recipe` |
| **A clips grid** | medium | list + thumbnail `editor/public/clips/<cart>/` |
| **C trailer** | medium | `compose-clips.js` UI (a cart-scale trailer builder) |
| **B stills** | medium | capture + gallery |

Everything the tab shows **already exists on disk** ([`cart-clips.md`](cart-clips.md) locks the
storage layout) — so this is largely a **view** problem, not new plumbing.

## Hot files (when building)
- `editor/src/shell.js` (+ its CSS) — the panel + render fns; reuse `renderLeads` and the app-card
  scroll pattern.
- `editor/electron/main.cjs` + `preload.cjs` — `studio:replay` (built), `studio:leads` (built),
  + new IPCs for bake/compose/snapshot. **main/preload changes need an Electron restart.**
- `tools/leads.js`, `tools/make-gif.js`, `tools/compose-clips.js`, `tools/cart-status.js` — the CLIs
  each section triggers (every action is a CLI first; the button is a thin trigger — the
  [`share-panel.md`](share-panel.md) commitment).

## Related
- [`editor-scopes-and-facets.md`](editor-scopes-and-facets.md) — *why* (Make/Promote/Ship, emergent scope).
- [`cart-clips.md`](cart-clips.md) — where takes/clips/reels live on disk.
- [`share-panel.md`](share-panel.md) — Ship (the share popup) + the 🎬 make-clip browser sketch.
- [`leads-marketeer.md`](leads-marketeer.md) — the tribes section's tool + ledger.
- [`trailer-builder.md`](trailer-builder.md) — the app-scoped trailer twin the per-cart trailer mirrors.
