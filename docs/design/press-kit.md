# Press kit — an in-house presskit()-style page, generated from cart data

STATUS: READY (2026-07-03) — specced, not built. A static press-kit generator for a shipped
app/cart, mirroring the journalist-recognised **presskit()** section format but generating it
ourselves from data we already own. Sits in **Channel A** (own-domain showcase) of
[`sharing-channels.md`](sharing-channels.md), a [`build-site.js`] artifact — *not* the App
Store. Shares the asset generators with [`store-agents.md`](store-agents.md) (`store-shots.js`,
`make-gif.js`). Same "mirror the convention, build our own" call as ADR-0026 (Fastlane).

## Why in-house

[presskit()](https://dopresskit.com/) (Rami Ismail / Vlambeer) is the de-facto indie press
kit: a single-file PHP tool rendering a page journalists recognise on sight. But it's **old,
effectively unmaintained PHP (~2013)**. So — exactly like Fastlane in
[ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md) — the *format* is a
valuable standard worth mirroring, while the *tool* is stale. We generate the same page as a
self-contained static site, from cart data.

And the fit is even better than ASO: a press kit is **almost entirely derivable from data we
already own** — the same "assets are build outputs of the cart" thesis. We don't hand-maintain
a fifth copy of the truth; we render it.

## The standard sections (mirror presskit()) → our source

| Section | Source |
|---|---|
| **Factsheet** — developer, based-in, release date, platforms, price, website, contact, social | app manifest (`apps/<name>/app.json`) + a studio-wide `studio.json` |
| **Description** / **Features** | `de:meta` of the app + its modules (reuse the store copy from [`../marketing/tinyjam/app-store-listing.md`](../marketing/tinyjam/app-store-listing.md)) |
| **Images** (gallery + download-all `.zip`) | `store-shots.js` output |
| **Trailer / videos** | `make-gif.js` output (real cart audio; a *real capture*, so it's also App-Store-preview-compliant — see store-agents §1 compliance note) |
| **Logo & Icon** (downloadable) | the app icon from the manifest |
| **History** (origin story) | `press.json` (hand-written) seeded from `de:meta` `lineage`/`homage` |
| **Awards** / **Selected articles & quotes** | `press.json` (hand-maintained; starts empty) |
| **Monetization permission** (streamers/YouTubers may monetise videos) | boilerplate — we grant it (a gift to the tribe; ties to `tinyjam-marketing.md` §9 gift-not-ask) |
| **About the studio** / **Credits** / **Contact** | `studio.json` |
| **Request press copy** | a mailto / TestFlight link (no `distribute()` dependency) |

## New data files (small, committed)

`studio.json` — one per studio, reused across every app:
```json
{
  "developer": "…", "basedIn": "…", "website": "https://…",
  "pressContact": "…@…", "social": { "mastodon": "…", "youtube": "…" },
  "about": "one honest paragraph about the studio",
  "credits": [ { "name": "…", "role": "…" } ]
}
```
`press.json` — per app, only the bits that aren't derivable:
```json
{
  "releaseDate": "2026-…", "price": "Free + IAP",
  "history": "the origin-story prose",
  "awards": [], "quotes": [ { "text": "…", "source": "…", "url": "…" } ],
  "additionalLinks": [], "monetizationPermission": true
}
```
Everything else comes from the existing `apps/<name>/app.json` manifest + `de:meta` + the
generated assets. Home: alongside the manifest (`apps/<name>/`), per ADR-0026's
metadata-next-to-manifest layout.

## The tool

`node tools/press-kit.js <app>` →
`site/press/<app>/index.html` (self-contained) + `assets/` + `<app>-presskit.zip`.

**Script / agent split** (the house rule, [`store-agents.md`](store-agents.md)):
- *Script:* assemble the static page, gather + zip the assets (screenshots/trailer/icon/logo),
  pull description/features from `de:meta`, and **lint factsheet completeness** (every required
  field present, contact reachable, at least N screenshots, a trailer).
- *Agent:* write the **Description / History / Features / About** in the honest voice — the
  same read-aloud taste rules as the store copy (`store-agents.md` §2). A lint can't write
  "this is the game"; the agent can.

## Where it lives

Channel A, the own-domain showcase — a `/press/<app>/` route built by (or beside)
`build-site.js`, published with the gallery. Not the App Store; a press kit is for *reaching
journalists and streamers directly*, the third leg alongside the store listing (paying
strangers) and the web demo (the tribe).

## Open questions

- One press kit per **app** (Tinyjams as a whole) or also per **module**? Likely app-level
  first (the umbrella is the story); a module can get its own later if a tribe warrants it.
- Reuse `build-site.js` or a standalone generator that `build-site.js` calls? Lean: a
  standalone `press-kit.js` that `build-site.js` invokes, so it also runs solo.
- The download-all `.zip`: generate on build, or on-demand? Build-time is simplest and cache-friendly.
