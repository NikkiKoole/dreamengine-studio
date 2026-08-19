# 0035 — Apps ship FREE with one "Pro" unlock (MIDI + export + AUv3); make the switch while sales are zero
Date: 2026-08-18 · Status: accepted, **amended 2026-08-19** (Pro is **$4.99**, and decision 5 is corrected: removing from sale is fine, only delete-and-resubmit is not. Tiny Jam's Master Pass and the two-axis reconciliation stay DEFERRED behind a five-app trigger. See the Update at the bottom, which wins over the body where they disagree.)

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
- **⏸ DEFERRED BY THE MAKER (2026-08-18), and it is a TRIGGER, not a task: the whole two-axis
  question waits until there are about FIVE music apps on the App Store, and the pricing gets fixed
  then, in one pass, with the real catalog in front of us.** The collision it is deferring:
  `apps/tinyjam/app.json` sells `masterpass` ("Unlock everything, now and future") at **$4.99**,
  which would be half the price of a $9.99 Pro while sounding strictly bigger, so content (which
  racks you own) and features (whether audio leaves the app) do not yet have one story.
  **Nothing is blocked by leaving it open**: Tiny Jam is v0.1, has never been submitted, and so
  nothing has ever been purchasable, while per-app Pro works standalone. ⚠ Note the products DO
  exist upstream: all three IAPs were created, localized, priced and left `READY_TO_SUBMIT` in App
  Store Connect on 2026-07-07 (the run that proved `asc-push --iap`), so this is a **reprice later**
  (`asc-push --iap --reprice`, which exists precisely because `--iap` leaves existing prices alone),
  not a blank slate. The manifest is deliberately untouched. Do not "tidy" that $4.99 in passing; it is parked, and the
  count is the signal. **Why a count and not a date:** the same reasoning as
  [`launch-sequence.md`](../design/launch-sequence.md) ("when the umbrella earns its keep", floor of
  3 toys, since a collection with two items reads as *we ran out*), and pricing a catalog you cannot
  see yet is guessing. Standing at **2** on 2026-08-18 (`pedalboard` live, `tinyacidjam` submitted).
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
- **Open sub-decisions**: the exact Pro price ($9.99 assumed here) is live now, since the first app
  to ship Pro has to name a number. Whether Tiny Jam's Pro is per-app or one unlock spanning the
  catalog is **parked behind the five-app trigger above**, together with the pass.

## Update (2026-08-19) — Pro is $4.99, and *Tiny Pedalboard* IS pulled from sale: decision 5 was answering a different question

Three things change. The first is the maker's call on the one live sub-decision; the second is a
correction to this ADR's own reasoning; the third is what has actually happened on the store.

**1. The Pro price is $4.99, not $9.99.** That closes the live sub-decision. The case above for $10
(AUM $19.99, Drambo $24.99, the Korg/Moog apps $15 to $30) is still true and is still the reason
$1.99 for an app containing an AUv3 was mispriced. What it does not settle is which number teaches
us more *first*. The open question these two releases exist to answer is **whether the wall is in
the right place**, and that is measured in conversions per install, not in revenue: at zero installs
neither number is informed, and the cheaper one produces the sample faster. $4.99 also matches the
impulse tier the tribe buys without thinking, which is what a first freemium funnel needs.

The asymmetry that makes this cheap to be wrong about: **raising a non-consumable's price later is
free and does not touch anyone who already owns it**, while cutting one reads as a fire sale. So the
mistake this direction is recoverable and the other is not. Whether $4.99 is permanent or a launch
price is deliberately NOT decided here; an ASC price schedule takes a start and an end date, so it
stays a one-command decision on the day there is data.

**2. Decision 5 ("never pull-and-re-release") conflated two different moves, and only one of them is
forbidden.** The Why bullet above argues them as one thing. They are not:

- **Removing from sale** (deselecting territories on the Availability page) keeps the app record,
  the bundle ID, the product page, its URL, the ratings and the approved binary. Coming back is
  re-selecting territories: **no new binary and no review**, because the version on file is already
  approved. It is reversible and it costs discovery, nothing else.
- **Deleting the app record, or resubmitting a near-identical app under a fresh bundle ID**, is the
  move that costs the listing and its reviews and is guideline-4.3 bait.

Decision 5 should read: never *delete or re-submit under a new bundle ID*. Removing from sale is an
ordinary, reversible lever. (Neither is required to change a price: that was and remains no binary,
no review. The correction is about what removing from sale COSTS, not about what it unlocks.)

**3. And there is a reason to pull that this ADR did not weigh: flipping to Free BEFORE Pro exists
is a feature takeaway.** Set the price to 0 today and every free downloader gets WAV export, MIDI
and the AUv3 with no wall, because the entitlement code does not exist yet. The version that
introduces Pro then *removes* three things people already have. **Taking a feature away is worse
than a wall that was always there** and it is exactly the shape that earns the 1-star "they
paywalled it" reviews this ADR's own wall-placement bullet is trying to avoid. Going dark for the
length of one build has no such cost at zero installs.

So the ordering is: **stay off sale, build Pro, then come back Free with the wall already in
place.** The first stranger to see the app sees the finished model.

### What is true on the store as of 2026-08-19

- **`pedalboard` is REMOVED FROM SALE in all 175 territories** (maker, 2026-08-19). Still $1.99 in
  ASC, still zero sales, listing and bundle ID intact. Nothing else about it changed.
- **`tinyacidjam` is APPROVED and READY FOR DISTRIBUTION, but has NOT been released** (maker,
  2026-08-19): it is sitting on an approved version awaiting developer release, so **nobody can buy
  it and it has never been on sale.** That spends the sequencing note above ("let Tiny Acid Jam
  finish its current review at $1.99" is done) and it needs no pulling, because there is nothing to
  pull. **The action is the opposite one: do not press release.**
- **So as of 2026-08-19 NEITHER app is purchasable, and that closes the grandfathering window for
  good** as long as it holds. `pedalboard` is off sale, `tinyacidjam` is unreleased, both at zero
  sales. The receipt check (`AppTransaction.originalAppVersion`) will never need to be written. This
  is the cleanest possible state to change a business model from, and the only way to lose it is to
  release or re-list something at $1.99 before Pro exists.
- **`tinyacidjam` gets a better option than `pedalboard` had, because it has no history to protect.**
  Two shapes, and the choice turns on one ASC mechanic worth confirming rather than trusting this
  line:
  - **Hold the approved version** until Pro is built, then flip the price to 0 and release. ⚠ The
    thing to check: an approved-but-unreleased version may have to be **released or developer-
    rejected before a 1.1 can be created**, in which case "hold" cannot reach a Pro build without
    passing through a public free-with-no-wall release first, which is the takeaway shape again (on
    a listing with zero downloads, so a small one).
  - **Developer-reject the approved version** and resubmit once Pro is real, as 1.0. Costs one
    review round trip, which the Pro build has to pay anyway, and buys the thing `pedalboard` cannot
    have: **the first public version of the app already has the wall in it.** Nothing is lost,
    because there is no listing, URL history, ranking or review to reset on an app that never
    shipped.
  Preferred: developer-reject, if ASC offers it on an approved-pending-release version.
- **The manifests now carry the model** (`apps/pedalboard/app.json`, `apps/tinyacidjam/app.json`):
  `"price": "0"` plus one non-consumable, `com.mipolai.<app>.pro` at **$4.99**, name *"Tiny
  Pedalboard Pro"* / *"Tiny Acid Jam Pro"*, desc *"WAV export, MIDI in/out, AUv3 plug-in."* Both
  pass `asc-push --check` and `build-app.js --check`.
  **⚠ This is deliberate drift: the manifest is intent, the store is unchanged until somebody runs
  `node tools/asc-push.js <app> --price`.** Do not run it as a tidy-up. It belongs to the
  re-release, alongside the build that ships Pro.
- **`unlocks` is `[]` on the Pro product, on purpose.** `build-app.js` reads `unlocks` as a list of
  **cart slugs** (and `["*"]` as the all-access master pass). Pro gates no rack, so an empty list is
  the honest value: the rack stays free and the launcher finds no master pass. The sibling
  `"features": ["export","midi","auv3"]` is documentation for now; nothing reads it yet, and the
  Swift entitlement will need to learn which product id means Pro.
- **Tiny Jam's `masterpass` stays at $4.99, untouched.** The five-app trigger above still governs
  it. Note it now sits *level* with a per-app Pro instead of below it, which is a different flavour
  of the same unresolved collision, and still not a reason to touch it early.

### The review-notes sentence, drafted but NOT applied

`apps/tinyacidjam/app.json`'s `review.notes` answer 4 still says *"It is a one-time paid app with no
in-app purchases"*. It is left alone on purpose: **notes must describe the build being submitted**,
and the Pro build does not exist, so writing them now would tell a reviewer about a purchase the
binary cannot make. It also cannot be parked as a placeholder, since `asc-push --review-contact`
refuses a notes body containing one. The replacement to paste when Pro is real, with the bracket
filled in from the shipped UI:

> 4. Setup and access: no account, login or credentials are required. The app opens straight into
> the groovebox with a pattern already loaded, so there is nothing to set up, and no sample files
> are needed to test it. The app is free to download. It offers one non-consumable in-app purchase,
> "Tiny Acid Jam Pro" (`com.mipolai.tinyacidjam.pro`), which unlocks WAV export, MIDI in and out,
> and the Audio Unit v3 extension. Everything else, including the full instrument, background audio
> and saving your own patterns, is free and unrestricted. To exercise the purchase: [WHERE THE PRO
> SHEET IS, AND WHAT UNLOCKS AFTERWARDS].

The store description of both apps also ends *"you buy it, you own it"*, which was written for a
paid app. It still reads correctly against a one-time unlock (and pointedly so, against
subscriptions), so it is a judgement call rather than a defect. A description edit needs a new
version anyway, so it costs nothing to decide with the Pro submission.

## See also
- **[`pro-unlock.md`](../design/pro-unlock.md) — HOW this gets built** (the entitlement seam, the
  four gaps in it, and the audit that found the paid FEATURES largely do not exist yet: no AUv3 in
  `pedalboard`, no user-facing WAV export in either app, MIDI in compiled out of every iOS build).
  Read it before starting the engineering this ADR describes. ·
- [`product-notes.md`](../design/product-notes.md) §🚩 (the trademark rule this must not break) ·
  [`product-notes-followup.md`](../design/product-notes-followup.md) §3–§4, §7 (App Group
  entitlements, the content-axis model this refines, in-house StoreKit) ·
  [`launch-sequence.md`](../design/launch-sequence.md) (the gift-first launch this feeds) ·
  [ADR-0026](0026-store-pipeline-in-house-not-fastlane.md) (`asc-push.js`, the tool that does the
  price change and the IAP registration).
