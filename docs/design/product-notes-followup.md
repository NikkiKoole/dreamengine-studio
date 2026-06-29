# product-notes — follow-up brainstorm: persistence, native iOS, the App Store, marketing

STATUS: IDEA / exploration (2026-06). A **third outside-agent brainstorm** (Gemini, "Database
Solutions for GitHub Pages"), same arrangement as the tinyjam racks ones: outside agent supplied the
commercial/how-to-ship vibes under the name "Tinyjam"; mapping to this codebase + sanity-checking
the claims is ours. Read [`product-notes.md`](product-notes.md) **first** — this is the
product-stage detail that doc was deliberately deferring ("decide at app stage, not before"). It
**answers two of that doc's open questions** (pricing shape; where feedback arrives) and fills in
its parking-lot items (native iOS, AUv3). Design side: [`tinyjam-racks.md`](tinyjam-racks.md) +
[`tinyjam-racks-followup.md`](tinyjam-racks-followup.md).

> ⚠️ **Numbers are LLM-supplied, not verified.** Market size, free-tier limits, Apple's cut, and
> tool capabilities below come from the source conversation — treat as leads to confirm, not facts.
> Anything load-bearing (Apple guideline numbers, AUv3/App-Group mechanics, Fastlane scope) should
> be checked against Apple/tool docs before it drives a build decision.

## 0. The naming tension to resolve 🚩

The maker picked **"Tinyjam"** in this conversation and argued it beats **"Tiny daws"** (DAW
over-promises multi-track/mixing; "jam" sets the cozy-loop expectation; cleaner to say/spell). The
first brainstorm also used "Tiny Jam". **The repo's working name is `tinyjam racks`.** This is an open
maker decision, not something to silently rename — but worth flagging that two independent
brainstorms + the maker landed on *Tinyjam*. Sub-naming convention from the conversation, if it's
adopted: app singular **Tinyjam**, user creations plural **tinyjams** ("share your tinyjam"), and
modules as **Tinyjam: <Genre>** (Korg-Gadget / Roland-Cloud style — the same `[series]: [vibe]`
shape the rack table already uses). Convergent with our trademark rule (`product-notes.md` §🚩):
the agent independently warned off "Pocket"-anything (Teenage Engineering) and Roland marks.

## 1. Save / share persistence (applies to the web gallery NOW, not just the product)

This is the part that isn't product-stage — it's relevant to the current wasm gallery
([`build-site.js`], GitHub Pages). The static-host reality: no backend to hide secrets in. Options,
cheapest first:

- **URL codec (no DB, 100% free, do this first).** Songs are tiny structured data — a lane blob is
  well under 1 KB (`tinyjam-racks.md` confirms: 8 bars × 16 × 4 instr ≈ 512 steps < 1 KB). Serialize →
  compress (lz-string / pako) → URL-safe Base64 → `…github.io/<cart>/?song=…`. Click = decompress,
  populate, play. **This is literally the seed-as-song-code handoff in `tinyjam-racks.md`, generalized
  to full edited state** — for *unedited* songs the u32 seed is even smaller (8 hex chars). Pasteable
  in Discord/Reddit; no accounts. Downside: no central gallery/browse.
- **IndexedDB / localForage** for local autosave (work-in-progress drafts survive a refresh).
  ⚠️ Browser storage is per-**origin**, not per-subdirectory — every cart on `nikkikoole.github.io`
  shares one store. **Namespace keys** (`tinyjam:rebirth:current` not `current`) or carts clobber
  each other. (Our saves are already per-cart on native via `save_dir`; the web gallery needs the
  same discipline in JS.)
- **Supabase / Firebase** only if a *public* browse-everyone's-songs gallery is wanted. One project,
  one `songs` table with an `app_id` column (don't burn a free project per cart). Secure via Row
  Level Security / Security Rules since the API key ships in client source. Claimed free tiers: ~500 MB
  (Supabase) / ~1 GB (Firebase) — ample for tens of thousands of <5 KB songs. Note the Supabase
  free-tier 7-day-inactivity pause.

**Takeaway:** the URL codec is a near-free win for the existing gallery and is the same machinery
tinyjam racks already needs — build the lane→string codec once, reuse for share-link *and* save.

## 2. Native iOS — porting C + raylib (the parking-lot "native app", made concrete)

> **Decided ([ADR-0023](../decisions/0023-ship-carts-as-apps-not-the-editor.md)):** what ships is
> a **precompiled native app** (a cart or a curated collection), never the editor. So this section
> is the *shippable* build plan — and the reason it's tractable at all: no on-device compiler is
> needed (the thing iOS forbids), because carts are baked native on the dev box.

`product-notes.md` parks this as "the big one." The conversation makes it feel tractable for *our*
exact stack (pure C + raylib + `sound.h` rendering headless):

- **Keep the code split** so web and iOS share one core: `core/` (engine, sequencer, draw — pure
  portable C, 95%), `web/` (emcc `main` loop + HTML shell), `ios/` (callback loop + tiny Obj-C
  bridges). We already have most of this shape (`runtime/` is portable; the editor owns the web
  build).
- **Game loop must invert.** iOS forbids the `while(!WindowShouldClose())` loop — the OS drives a
  callback lifecycle (`ready()`/`update()`/`destroy()`) so it can suspend on a phone call. Our
  `studio.c` owns `main()` with exactly that loop, so this is a real (if mechanical) port task.
- **OpenGL → Metal via ANGLE.** raylib is GLES; Apple deprecated GL. The community `raylib-iOS` /
  ANGLE path translates to Metal. A build-system task, not an engine rewrite.
- **Sandbox save paths.** Can't write next to the binary; query the Documents dir via a 3-line
  Obj-C helper returning a `const char*` to the C side. Maps cleanly onto our `save_*` path layer.
- **Safe areas + 44 pt touch targets.** Notch/Dynamic-Island insets + min 44 pt hit areas. This is
  exactly the current touch pass ([`touch-notes.md`], [`mobile-web-notes.md`]) — the fat-finger pads
  in `ui.h` already encode the hit-pad idea.
- **Bundle the wasm, never download it.** If a native shell ran *downloaded* wasm it risks App Store
  guideline 2.5.2/4.7 (no downloaded executable code). For us this is moot if we go raylib-native
  (compile C straight to the binary) rather than wrapping wasm — the better path anyway.

## 3. AUv3 — the killer feature, and how the multi-rack catalog fits it

`product-notes.md` rates AUv3 "high cost, the serious crowd asks first." The conversation adds the
architecture for *our* multi-rack plan and reconfirms feasibility (our `sound.h` already renders to
a buffer headless — the `--wav` path is the proto render-callback):

- **One hub app, many Audio Unit Extension targets.** Container "Tinyjam" app + one extension per
  rack (`TinyjamRebirthExtension`, …). Host DAWs (AUM, Loopy Pro, GarageBand) list each as its own
  plugin; users load several at once → literally "jam multiple racks at once."
- **App Groups for entitlements.** Extensions run in a separate sandboxed process and can't query
  the store directly. Main app writes `unlocked:<rack>=true` into a shared App Group container; each
  extension reads it on load (unlocked → init the engine; locked → "unlock in the main app" screen).

## 4. Monetization model (answers product-notes.md open Q "pricing shape")

The conversation lands a concrete, community-validated shape (the iOS-music crowd's stated
preferences — verify against current norms but directionally sound):

- **Free base app** with one standalone rack (no AUv3) = try-before-buy hook.
- **À la carte IAP** per rack (~$3–5); buying unlocks **both** standalone *and* AUv3 use.
- **One-time "Master Pass"** (~$20) unlocks everything incl. future racks. The community **strongly
  prefers one-time over subscriptions** for indie instruments — subscriptions are resented except
  for big suites (Logic, Roland Cloud).
- **Climbing early-adopter price**: launch the Pass cheap with few racks, raise it for *new* buyers
  as racks ship (existing owners keep lifetime access; App Store Connect price scheduling handles it,
  no code change — the app always asks StoreKit for the live localized price, **never hardcode price
  text**).
- **Temporary sales** via App Store Connect "Temporary Price Change" (auto-revert); promote IAPs on
  the store page.
- This directly resolves `product-notes.md`'s "one app with rack IAPs vs app-per-rack" — **one hub
  app with IAPs** (Pocket-Operator-catalog feel), and reframes its "does paid web exist" question:
  paid lives native; web stays the free funnel + share/codec layer.

## 5. Module-of-the-Week — free rotation via remote config (cheap, clever)

Rotate one rack free-standalone each week to keep people opening the app + act as a rolling AUv3
trial. **Don't touch App Store prices** — host a tiny `weekly_deal.json` on the GitHub Pages site
(`{free_module_id, start_date, end_date}`); a scheduled **GitHub Action** (cron) rewrites it weekly
and commits. App fetches it on launch, unlocks that module *in memory* if within the window. Cache
it for offline; ignore clock-tampering (not worth defending a $3 unlock). Apple allows this
"promotional feature flagging" as long as payments stay in IAP. Same JSON can carry a `sale_active`
banner flag. (We already run GitHub-Pages-from-Actions for the gallery — this is the same muscle.)

## 6. Xcode-free agentic pipeline (matches how this repo already works)

The maker dislikes the Xcode GUI and develops agentically — the conversation maps a terminal-only,
agent-drivable flow, all tools claimed free (verify scope):

- **CMake** generates the `.xcodeproj` (`cmake -G Xcode`) — agent maintains `CMakeLists.txt`; never
  drag files in Xcode.
- **`xcodebuild`** compiles/packages `.ipa` from a `build.sh`.
- **Fastlane** automates signing + upload: **Spaceship** hits the App Store Connect API to register
  IAPs programmatically; **Deliver** manages metadata/screenshots/release-notes as local text files;
  `fastlane deploy` does compile → sign → upload → submit in one command.
- **Agentic "add a new rack"** becomes: agent writes `jam_<genre>.c`, edits CMake, appends the module
  id to the config array, registers the IAP via Spaceship, rewrites release notes, runs `fastlane
  deploy`. This is the same agentic loop the rest of dreamengine uses, extended to shipping.
- Only **$99/yr Apple Developer** is a hard cost — **the maker already has an account with one app
  live**, so no extra fee, banking/tax already cleared.
- Xcode GUI still needed ~twice: first-run signing setup, and LLDB for on-device C crashes
  (segfaults/pointer bugs that only repro on hardware).
- **Attract mode** (idle demo) → do it as a *generative* sequence (sequencer auto-plays a seeded
  groove + procedural visualizer) rather than decoding mp4 in C. We already have everything for
  this (a cart *is* a generative demo; `make-gif.js` bakes the App Store preview videos). Ties to the
  visualizer/dancer in `tinyjam-racks-followup.md` §3.

## 7. StoreKit 2, in-house (maker wants zero third-party SDKs)

StoreKit 2 needs no backend — Apple verifies purchases on-device (cryptographic JWS),
`Transaction.currentEntitlements` lists what's owned, restoration is automatic. Bridge to C with
three small files: `tinyjam_store.h` (plain C API: `Store_IsModuleUnlocked("rebirth")` etc.), a
Swift manager (queries entitlements, writes them to the App Group for the AUv3 extensions), and an
Obj-C++ `.mm` bridge. RevenueCat exists (free < ~$10k/mo) but the maker wants all code in-house —
StoreKit 2 direct is the clean fit, no SDK bloat / no third-party server dependency.

## 8. Android verdict: iOS first, Android maybe later

Don't let Android delay launch. Reasons given: **no native AUv3 equivalent** (AAP is a research
project, not in mainstream Android DAWs) so the plugin business model can't exist there; **audio
latency is inconsistent** across the device spectrum (fine on flagships, sluggish on mid/budget);
and the mobile-music-buying audience + willingness-to-pay is heavily iOS (claimed 80–90% of
revenue). Our C/raylib core stays portable, so a standalone Android build is a later option, not a
rewrite.

## 9. Bar-limit design (feeds the tinyjam racks lane format)

For a thumb-first phone UI: show **16 steps (1 bar)** at a time (wider = mis-taps). Song length sweet
spot = **4-bar loop** via a `[1][2][3][4]` page switcher, **or** the **Pocket-Operator pattern-chain**
(edit 1-bar patterns A–D…, chain them `A A B A`). The latter is exactly `tinyjam-racks.md`'s "sections
become pattern banks A/B/C/D" — independent convergence again. Either way data stays < 1 KB, so
save/DB/URL sizes are trivial. This is a direct input to the tinyjam racks lane-count open question.

## 10. Market + marketing (leads to chase, numbers to verify)

- **Market size (claimed):** mobile music-production software ~$150M global, ~9% CAGR, ~5M active
  users, NA ~45%. Small vs streaming, but a focused, high-willingness-to-pay niche. **Verify before
  citing.**
- **Why this niche pays:** users anchor on hardware/desktop-plugin prices ($150 PO, $100–300
  plugins), so a $5 / $20 app reads as cheap. Same thesis as `product-notes.md` (PO / Koala / Korg
  precedents) — this conversation adds Koala Sampler + Loopy Pro as solo-dev existence proofs.
- **The marketing town square = the Loopy Pro forum** (`forum.loopypro.com`, formerly the Audiobus
  forum — renamed late 2024 when Michael Tyson kept the forum after selling the Audiobus app). This
  *answers `product-notes.md`'s open question* "where does feedback from gallery players arrive?" —
  it's the channel: post a TestFlight build, get expert beta testers + word-of-mouth + YouTube
  reviewers (Sound Test Room etc.). Indie-dev-friendly, AUv3-hungry, one-time-pricing-loving.
- **Email "wishlist"** (Steam-style — Apple has no IAP wishlist): a *"Notify me of sales"* button by
  the Master Pass, optional only (App Store guideline 5.1.1 — no email walls, needs a privacy
  policy), POSTed to a free-tier list (MailerLite/Brevo/Mailchimp), tagged `wishlist_master_pass`;
  email that segment when a sale starts. Targets warm leads who *want* it but are price-waiting.

## 11. Smaller items worth keeping

- **IAP deep links** (e.g. promoting a Drum&Bass rack on Reddit): **don't** use
  `itms-services://…purchaseIntent` (dev/sandbox only). Use **Promoted IAPs** (show on the App Store
  page, buyable before install — iOS auto-installs the free app then checks out) + the plain App
  Store URL; for already-installed users, a **Universal Link** (`https://tinyjam.app/jams/dnb`) opens
  straight to the unlock screen (or falls back to the web preview).
- **Promote a standout instrument as its own rack** — the maker's E-Piano → *Tinyjam: E-Piano*. Keys
  are a popular AUv3 sound-module category; phone-first UI = **chord pads** (8–16 big pads, each a
  voiced chord) or **scale-locked grids** (no wrong notes), not tiny piano keys. We already have the
  pieces: `INSTR_FM` epiano shipped, `keybed.h` / `solo.h` for scale-locked input, the
  tremolo/chorus effects. A non-sequencer "sound module" rack widens the catalog beyond grooveboxes.

## Net effect on product-notes.md

Nothing in the "sketch first" decision changes — ship the pilot on the free web gallery and watch.
But this conversation **pre-answers the questions that fire *after* a trigger**: pricing shape
(§4), feedback channel (§10, Loopy Pro forum), and a concrete, agent-drivable native-iOS/AUv3 build
plan (§2–§7) for when "a rack demonstrably holds attention" un-parks the native app. The save/share
codec (§1) is the one item worth doing **now**, independent of the product question, because the web
gallery needs it and tinyjam racks needs the same codec anyway.

Full raw transcript of the source conversation is parked in scratch (not committed); its substance
is captured above.
