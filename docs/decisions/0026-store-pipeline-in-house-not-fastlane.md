# 0026 — Store pipeline: in-house App Store Connect API tool, not Fastlane
Date: 2026-07-03 · Status: accepted

## Context
The store rung of the share panel needs a way to push the *non-cart* product surface —
listing copy (name/subtitle/description/keywords/promo text, per locale), screenshots,
preview videos, IAP registration, TestFlight — to App Store Connect.
[`product-notes-followup.md`](../design/product-notes-followup.md) §6 sketched **Fastlane**
(Ruby: Deliver for metadata/screenshots-as-local-files, Spaceship for IAPs), and
[`share-panel.md`](../design/share-panel.md) v3 parked the fork as "App Store Connect
API *or* Fastlane". The alternatives: adopt Fastlane, or hand-roll a thin tool against
the App Store Connect REST API directly.

## Decision
**Hand-roll it.** A repo tool (node, like everything in `tools/`) talks to the App Store
Connect API directly (JWT/ES256 auth from a `.p8` key — same shape as the notarytool
credential setup). **We steal Fastlane's `metadata/<locale>/*.txt` + `screenshots/<locale>/`
folder convention verbatim** — copy and assets live as versioned files next to the app
manifest — which also keeps an eject-to-Fastlane escape hatch open for free. No Ruby
dependency, no DSL runtime.

## Why
- **We've already hand-rolled the hard half.** `mac-app.sh` (codesign → notarize →
  staple), `ios/device.sh` (signed device deploy), and xcodegen cover Fastlane's
  cert/build/sign territory; the store upload is `xcodebuild -exportArchive` + one
  Transporter command. What's left of Fastlane is a REST client plus conventions.
- **In-house is the standing ethos** — same reasoning as the StoreKit 2 direct bridge
  (no RevenueCat) in `product-notes-followup.md` §7, and share-panel.md rule 2 (every
  action is a CLI tool first; the button streams its log). Wrapping a Ruby DSL runtime
  fits neither.
- **The decisive argument is what ours can do that Fastlane structurally can't.**
  Fastlane treats screenshots/videos/copy as opaque files produced elsewhere. Here they
  are *derivable artifacts of the cart*:
  - **Assets as build outputs** — `play.js` + a committed `tools/clips/` input track
    renders pixel-perfect screenshots, and `make-gif.js` mints preview videos *with the
    cart's real audio*, at exact device resolutions, headless — no simulator, no Swift
    UI-test target (Fastlane's snapshot path).
  - **Copy from `de:meta` / the app manifest** — the store description drafts from the
    same single source that feeds `index.json` and the compendium, not a fifth
    hand-maintained copy of the truth.
  - **A drift oracle in the existing family** — Fastlane only pushes; it has no concept
    of *stale*. Ours can GET the live listing and diff it against the repo
    (`store-status.js`, twin of `cart-status.js`): screenshots older than the cart
    source, local copy diverged from live, a locale missing a file.
  - **Limit + ASO linting at commit time** (`lint-carts.js` style): char limits
    (name/subtitle 30, keywords 100, promo 170, description 4000), per-locale
    completeness, keywords wasted on words already in the title/subtitle,
    rejection-bait words (Fastlane-precheck-alike).

## Consequences
- **The real effort budget is known and accepted**: the chunked asset-upload dance
  (reserve → presigned PUT parts → MD5 commit → poll), the per-device-class
  screenshot/preview spec matrix (encode current rules; they've shrunk), the TestFlight
  processing wait + export-compliance PATCH. None is a moat; each is a day-ish of node.
- **Localization is just more folders** — an agent writes `nl-NL/`/`de-DE/` copy as
  text files, the maker reviews the diff, the tool pushes all locales.
- The "or Fastlane" fork in share-panel.md v3 and the §6 sketch in
  product-notes-followup.md resolve to this ADR; the folder layout is the only part of
  Fastlane we keep.
- Still gated where [`sharing-channels.md`](../design/sharing-channels.md) §Channel B
  put it: the product decision (which app first) and the original-palette rule come
  before any of this runs in anger.
- This ADR owns the *plumbing*. The **judgment layer above it** — the agentic work that
  decides whether an app gets accepted and discovered (hero-frame selection, copy
  transcreation, ASO, guidelines pre-flight, review triage) — is brainstormed in
  [`store-agents.md`](../design/store-agents.md), which draws the script/agent boundary
  for each: scripts prepare → agent decides → scripts execute.
