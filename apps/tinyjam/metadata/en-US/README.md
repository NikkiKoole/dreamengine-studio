# App Store listing copy — en-US

Fastlane-style per-locale overrides for `tools/asc-push.js`. **Each `.txt` file's entire
contents becomes that App Store field, verbatim.** A file here WINS over `../../app.json`'s
`listing` block. `asc-push` only reads `*.txt` — this README (and any other non-`.txt`) is
ignored, so it's safe to keep notes here.

Push flow (the maker gates it — dry-run first, always):
```
node tools/asc-push.js tinyjam --metadata --dry-run   # GET live + diff, no writes
node tools/asc-push.js tinyjam --metadata             # PATCH live
```

## Field → file → source of truth → limit

| Field | Where it lives | ASC char limit |
|---|---|---|
| name (title) | `../../app.json` → `listing.title` | 30 |
| subtitle | `../../app.json` → `listing.subtitle` | 30 |
| keywords | `../../app.json` → `listing.keywords` | 100 |
| **description** | `description.txt` (here) | 4000 |
| **promotional_text** | `promotional_text.txt` (here) | 170 |
| **support_url** | `support_url.txt` (here) → https://mipolai.com/tinyjam/support/ | — |
| whats_new | *(unmanaged — see below)* | 4000 |
| marketing_url | *(unmanaged — see below)* | — |

name/subtitle/keywords stay in the manifest (don't duplicate them here). `description.txt` +
`promotional_text.txt` are the maker's own words, assembled from
[`../../press.md`](../../press.md) (`## Description` + `## Features`) — press.md is the
human-readable master; these are the store-push copies. If you change one, run
`node tools/aso-coverage.js tinyjam` to re-check the listing against
[`../../seo-brief.md`](../../seo-brief.md).

## Deliberately left unmanaged (need a maker decision, not AI prose)

- **marketing_url** — optional; add `marketing_url.txt` if an about/landing page goes live (distinct
  from the support URL). See [`../../support-page-brief.md`](../../support-page-brief.md).
- **whats_new** — only matters for an *update*; a first release doesn't need it. Add `whats_new.txt`
  before shipping v1.x.

Any field with no `.txt` here and no manifest entry is simply not touched by `asc-push` — whatever
is already live on ASC stays. So adding a file = bringing that field under repo control.
