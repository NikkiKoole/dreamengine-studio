# Press kit — an in-house presskit()-style page, generated from cart data

STATUS: BUILDING (2026-07-03) — **v0.1 SHIPPED** (`tools/press-kit.js`): generates a
self-contained press page + assets zip from `apps/<name>/app.json` + `apps/<name>/press.md` +
`studio.md` + `store-shots`/`make-gif` assets. No node deps (flat frontmatter + a tiny markdown
renderer). Proven on Tinyjam (acidrack/yachtrack/groovebox shots + trailer). Mirrors the
journalist-recognised **presskit()** section format, generated ourselves from data we own. Sits
in **Channel A** (own-domain showcase) of [`sharing-channels.md`](sharing-channels.md), a
[`build-site.js`] artifact — *not* the App Store. Shares asset generators with
[`store-agents.md`](store-agents.md). Same "mirror the convention, build our own" call as
ADR-0026 (Fastlane). v0.2: icon/logo assets, per-module kits, `build-site.js` wiring, a
`/press/` route on the own domain.

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
| **Factsheet** — developer, based-in, release date, platforms, price, website, contact, social | app manifest (`apps/<name>/app.json`) + a studio-wide `studio.md` |
| **Description** / **Features** | `de:meta` of the app + its modules (reuse the store copy from [`../marketing/tinyjam/app-store-listing.md`](../marketing/tinyjam/app-store-listing.md)) |
| **Images** (gallery + download-all `.zip`) | `store-shots.js` output |
| **Trailer / videos** | `make-gif.js` output (real cart audio; a *real capture*, so it's also App-Store-preview-compliant — see store-agents §1 compliance note) |
| **Logo & Icon** (downloadable) | the app icon from the manifest |
| **History** (origin story) | `press.md` (hand-written) seeded from `de:meta` `lineage`/`homage` |
| **Awards** / **Selected articles & quotes** | `press.md` (hand-maintained; starts empty) |
| **Monetization permission** (streamers/YouTubers may monetise videos) | boilerplate — we grant it (a gift to the tribe; ties to `tinyjam-marketing.md` §9 gift-not-ask) |
| **About the studio** / **Credits** / **Contact** | `studio.md` |
| **Request press copy** | a mailto / TestFlight link (no `distribute()` dependency) |

## New data files (where you WRITE copy)

**Prose → Markdown; structured fields → frontmatter.** Paragraphs in JSON are miserable
(escaping, no line breaks, no markdown), and the whole point is that *writing more copy is
pleasant*. So the copy lives in Markdown files with a YAML frontmatter head, next to the
manifest (`apps/<name>/`, per ADR-0026's metadata-next-to-manifest layout):

```
apps/tinyjam/
  app.json      manifest (bundleId, carts, iap, icon, price) — already exists
  press.md      ← the per-app press copy you WRITE
studio.md       ← studio-wide identity + "about" prose, reused by every app
```

`apps/<name>/press.md` — open it and write:
```markdown
---
releaseDate: 2026-09-01
price: Free + IAP
monetizationPermission: true
quotes:
  - text: "Tiny, and completely addictive."
    source: Reviewer, Outlet
    url: https://…
awards: []
additionalLinks: []
---

## Description
Real markdown, as long as you like…

## History
The origin story…

## Features
- one
- two
```

`studio.md` — same shape: frontmatter (developer, basedIn, website, pressContact, social,
credits) + a body paragraph for **About the studio**. Written once, shared by all apps.

**Fallback rule:** prose comes from `press.md`; any section it omits falls back to the cart's
`de:meta` description — so the kit generates sensibly with *zero* copy, and writing more is
just editing one Markdown file (never JSON). Everything else (platforms, IAP, icon) is read
from the existing `app.json` manifest + generated assets.

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
