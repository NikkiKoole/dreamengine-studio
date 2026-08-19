# 0035 — Apps ship FREE with one "Pro" unlock (MIDI + export + AUv3); make the switch while sales are zero
Date: 2026-08-18 · Status: accepted (the price POINT and Tiny Jam's Master Pass are the two open sub-decisions, below)

## Context
Two apps are on the store at **$1.99 paid**, both shipping an AUv3 extension:
`apps/pedalboard` (*Tiny Pedalboard*, `READY_FOR_SALE` since 2026-08-17, 1.0 and 1.1) and
`apps/tinyacidjam` (*Tiny Acid Jam*, submitted 2026-08-17). **As of 2026-08-18 neither has sold a
single copy.** `apps/tinyjam` (the umbrella) is v0.1 and has never been submitted.

[`product-notes-followup.md`](../design/product-notes-followup.md) §4 already answered
[`product-notes.md`](../design/product-notes.md)'s "pricing shape" question on the **content axis**:
a free hub app, per-rack IAP, plus a one-time Master Pass. `apps/tinyjam/app.json` implements a
cheaper version of that (racks at $1.49–1.99, `masterpass` at $4.99).

The maker's proposal (2026-08-18) is a different axis: **free download, one Pro unlock at about $10
carrying MIDI in/out, WAV export and AUv3.** That is the **feature axis**, and it is the axis this
market asks about first: `product-notes.md` already rates AUv3 *"the serious iOS-musician crowd asks
first"*. The alternatives weighed were: stay paid at $1.99 and let the wasm gallery be the free
tier; freemium with a subscription; and a feature ladder (an export pack, a MIDI pack, an AUv3 pack
sold separately).

## Decision
1. **The store unit is FREE.** One non-consumable **Pro** unlock per app, at one price (about
   $9.99, see the open item).
2. **Pro's wall is "it leaves the app."** Free gets the complete instrument, plus background audio,
   Ableton Link, and saving/loading your own patterns. Pro gets **WAV export, MIDI in/out, and the
   AUv3 extension**. Nothing about the free experience is degraded: no time limit, no nag screens,
   no reduced audio quality, no locked sounds.
3. **No subscription, and no feature ladder.** One unlock, one price. §4's community finding (the
   iOS-music crowd resents subscriptions for indie instruments) stands.
4. **Make the switch now, while sales are zero.** See Why.
5. **Never pull-and-re-release to do it.** Price is not part of a build.
6. The **content axis stays Tiny Jam's business alone.** A standalone single has no catalog, so it
   has no per-rack IAP; only the umbrella gets both axes.

## Why
- **AUv3 structurally needs try-before-buy.** The buyers who want a plug-in want to load it in AUM,
  Loopy Pro or GarageBand *on their own device, in their own project* before paying. A paid-upfront
  app cannot offer that; a free download is not a discount, it is the demo those buyers require.
- **$10 is the right price for that bundle, not a stretch.** The comparables in
  `product-notes.md`'s thesis price well above games (AUM $19.99, Drambo $24.99, the Korg/Moog apps
  $15–30), and MIDI + export + AUv3 is precisely the "this is a real instrument, not a toy" tier.
  $1.99 for an app *containing an AUv3* was the mispriced end of this, not $9.99.
- **Zero sales is a closing window, and that is why this is decided now rather than later.** The
  genuinely hard part of a paid-to-freemium migration is **grandfathering**: everyone who paid must
  keep the full feature set for nothing, which means reading the receipt
  (StoreKit 2 `AppTransaction.originalAppVersion`) and unlocking Pro for anyone whose original
  purchase pre-dates the switch. That is fiddly, easy to get wrong, and getting it wrong charges
  real people twice. **At zero sales that code does not need to exist.** Every sale between now and
  the switch adds it back.
- **The store mechanics are nearly free, so "pull it and reintroduce it" buys nothing.** A price
  change (including paid → Free) needs **no new binary and no review**; it is a price schedule, and
  we already drive it: `node tools/asc-push.js <app> --price` with `"price": "0"` in the manifest
  (the tool documents `0 = Free`). Removing from sale and re-releasing would cost the product page,
  the URL and a second trip through review, in exchange for nothing. **Deleting the app record or
  re-submitting under a new bundle ID would be worse**: a bundle ID is forever, the listing and its
  reviews reset, and a near-identical resubmission is exactly what guideline 4.3 looks for.
- **The wall is placed where reviews do not punish it.** Gating background audio or Ableton Link
  (the things people need to *use* the toy at all) reliably earns 1-star "everything is paywalled"
  reviews. Gating the paths that carry audio *out* of the app reads as fair to the same audience,
  because it is the same line the market already draws.
- **One price converts better than a ladder** and matches [ADR-0028](0028-sensible-defaults-optional-tweaks.md)
  (pick the option a stranger gets; do not build a settings wall). Three small unlocks would
  triple the StoreKit surface, the entitlement checks and the store-page clutter to earn the same
  $10 from the same person.
- **The free tier is also the shape the marketing plan already assumed.**
  [`launch-sequence.md`](../design/launch-sequence.md) commits to gift-first, playable-link-first
  outreach and explicitly leaves the door open ("paid, or free + one unlock"). A free app travels
  through Show HN, r/InternetIsBeautiful and the tribe venues in a way a $1.99 one does not.
- **The launch is unspent.** Both listings went up quietly and sold nothing, so the freemium
  versions still get a first-launch moment. What has actually been bought with these two releases is
  a *rehearsal of the review gauntlet*, which is what `launch-sequence.md` wanted from a first ship
  anyway.
- **Rejected: staying paid with the web gallery as the free tier.** It is a real position (the
  263-cart wasm gallery is a free funnel that already exists) and it avoids the StoreKit tax
  entirely, but it cannot answer the AUv3 try-before-buy point above, and it leaves the $1.99
  mispricing in place.

## Consequences
- **Sequencing, lowest risk first.** Let *Tiny Acid Jam* finish its current review at $1.99 rather
  than cancelling the submission (a cancellation costs a review round trip and the model change does
  not need one). Build the Pro plumbing once, then roll it out: `pedalboard` first (already live,
  zero sales, nobody watching), then `tinyacidjam`, then `tinyjam` inherits it before it ever ships.
- **The engineering is one mechanism, shared by all three apps**: a StoreKit 2 purchase (in-house,
  no RevenueCat, per followup §7) plus **an entitlement the AUv3 extension can read**. That second
  half is the part that bites: the appex is a separate sandboxed process and a host can load it
  **without the container app ever launching**, so a Pro flag living in the app's `UserDefaults`
  reads as *locked* inside AUM. followup §3 already specifies the fix (App Group container written
  by the app, read by the extension on load). It is also the first thing a paying customer tests.
- **A price collision to resolve before Tiny Jam ships.** `apps/tinyjam/app.json` sells
  `masterpass` ("Unlock everything, now and future") at **$4.99**, which would be half the price of
  a $9.99 Pro while sounding strictly bigger. The two axes need one story: content (which racks you
  own) below features (whether audio leaves the app). Not fixed here; it is a maker call on the
  price point, and the manifest is untouched.
- **`apps/tinyacidjam/app.json`'s review notes are now wrong.** They tell App Review *"It is a
  one-time paid app with no in-app purchases"* (answer 4). That sentence has to be rewritten for the
  submission that introduces Pro, and reviewers must be told how to exercise the paid feature.
- **The trademark rule does not relax; if anything it tightens.** `product-notes.md` §🚩 says nothing
  Roland-named crosses a paywall, and freemium keeps a paywall (the Pro unlock) while multiplying
  installs. ReBirth's death was a *commercial* homage with model numbers in its identity fields, and
  free-with-IAP is still commercial use with a much larger install base. The `rebirth`/`338` keywords
  and the model numbers in the title/subtitle stay the open exposure they already were.
- **What the promo-code gift becomes.** A promo code for a free app is worth nothing, so the gift has
  to be a code for the **Pro IAP** (Apple supports promo codes for non-consumables; confirm on the
  actual product in App Store Connect before promising it in a post). Budget shape to plan the tribe
  passes around, all worth re-confirming in ASC rather than trusting this line: about **100 codes per
  app, per platform, per version**, replenished by each new version, and codes expire (roughly four
  weeks from generation). Ten per communication, weekly, fits comfortably. Keep TestFlight for its
  own job (pre-launch "what would you change?", public link, 10,000 testers, builds expire after 90
  days): a beta build feels like unpaid work, a code feels like a present. Several music subreddits
  ban giveaways and self-promo outright, so lead with the playable web link (unlimited, instant, no
  permission needed) and hand codes out in replies or DMs to people who actually engaged. Log them
  with `node tools/leads.js track add` or the advocate list evaporates.
- **Open sub-decisions**: the exact Pro price ($9.99 assumed here), and whether Tiny Jam's Pro is
  per-app or a single unlock spanning the catalog.

## See also
- [`product-notes.md`](../design/product-notes.md) §🚩 (the trademark rule this must not break) ·
  [`product-notes-followup.md`](../design/product-notes-followup.md) §3–§4, §7 (App Group
  entitlements, the content-axis model this refines, in-house StoreKit) ·
  [`launch-sequence.md`](../design/launch-sequence.md) (the gift-first launch this feeds) ·
  [ADR-0026](0026-store-pipeline-in-house-not-fastlane.md) (`asc-push.js`, the tool that does the
  price change and the IAP registration).
