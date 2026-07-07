# Store agents — the judgment layer above the App Store pipeline

STATUS: BUILDING (2026-07-03) — the free ASO/press/share **toolchain + its editor surface
SHIPPED** today (ledger: [`../STATUS.md`](../STATUS.md)). Captures the *agentic* half of App
Store deployment — the work that decides whether an app gets **accepted** and **discovered**,
which a REST client can't do. [ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md)
decided the plumbing (in-house ASC API tool, not Fastlane); this doc is the layer above it.
Sits under Channel B of [`sharing-channels.md`](sharing-channels.md) and the store rung of
[`share-panel.md`](share-panel.md).

> **Perspective check:** everything here is demand **capture** (get found by people already
> searching) — the *tail*, not the wave. Where downloads actually come from (demand
> *generation* — video, tribe, word-of-mouth) and how these tools rank against it is
> [`demand-generation.md`](demand-generation.md). Read that first if the question is "how do we
> get downloads," not "how do we rank."

## Pick-up point (next session)

**Shipped (2026-07-03) — a big session, four threads:**
- **The ASO keyword loop (CLI):** `aso-research` (now mines competitor **descriptions** too,
  document-frequency ranked) · `aso-suggest` (the free **Google-autocomplete demand proxy** —
  §"the free trick"; splits into store WORDS + website PHRASES) · `aso-compose` · `aso-lint` ·
  **`aso-brief`** (the *palette* — a committed, drift-tracked `apps/<app>/seo-brief.md` worksheet)
  + **`aso-coverage`** (the *mirror* — phrase/word coverage + a stuffing check) · **`aso-score`**
  (terminal scoreboard + A/B tweak; `--deep` = **winnability** from per-keyword difficulty). The
  honest loop: **research/suggest → brief → *you write* → coverage → compose/lint/score.** None
  writes prose (§"palette + mirror").
- **The Apps-view surface:** app-less research lab + per-app card — 📸 screenshots → 📄 press kit,
  🍎/📱 builds, and the sell row 📝 worksheet · 🔎 research · 💡 suggest · 🧩 compose · 🔬 analyze ·
  📊 score · ✅ lint · 🪞 check. Plus **IAP copy shown** (name ≤30 / desc ≤45 char badges),
  **clickable keyword "keys"** (fill the research box), **all keys → research**, and **load into
  all tools**. Manifest carries `listing` + surfaces `iap.products`.
- **The strategy reframe:** [`demand-generation.md`](demand-generation.md) — demand *capture* (ASO,
  the tail) vs *generation* (video/tribe/word-of-mouth, the wave); the lever hierarchy; grab a
  **tribe**, not the world. (All of this ASO doc is the *capture* layer.)
- **The trailer builder** ([`trailer-builder.md`](trailer-builder.md)): backbone `build-app-reel.js`
  (manifest `carts[]` → clip per rack → one reel; Tiny Jam is a 3-rack reel — acidrack got a clip)
  **+ editor v1 (A):** the Apps-card 🎞 **trailer** section — click-to-edit timeline (library,
  ◀▶ reorder, transition-at-join dropdown + secs, Build → bake+compose → inline preview),
  **non-destructive** (writes only the `.reel`).

**Orient:** `node tools/topic-brief.js "app store" "aso" "trailer" "demand"`.

**Next — the active build thread (trailer):** per-clip **trim + speed** (a `compose-clips`
`setpts`/`trim` bump + `.reel` syntax + block fields — fixes "the 34s reel is too long"), then the
staged layer (drag polish B · 9:16 social export · IAP-tease ordering). See
[`trailer-builder.md`](trailer-builder.md).

**Next — the store track (maker-gated), in priority order:**
1. **The real submission gates** — which app first, price, the original-palette rule
   ([`sharing-channels.md`](sharing-channels.md) §Channel B). *Nothing ships until these are
   the maker's call.*
2. **The ASC upload + TestFlight tool** — ~~the one big unbuilt piece~~ **BUILT (2026-07-07):
   `tools/asc-push.js`** ([ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md)):
   in-house against the App Store Connect API (JWT/ES256 from a `.p8`, zero deps), proven live
   against Tiny Jam. `--metadata` (title/subtitle/keywords/desc/promo/URLs/copyright, GET-and-diff
   on `--dry-run`) · `--screenshots` (the chunked reserve→PUT→commit→poll dance) · `--iap`
   (create→localize→price→availability→review-shot→1024² promo image → READY_TO_SUBMIT, idempotent;
   images from `apps/<app>/iap-images/<slug>.png`) · `--check` offline gate. Auth lives in
   `~/.appstoreconnect/` (`.p8` + `config.json`), never git; `*.p8` gitignored as a backstop. Four
   API gotchas baked in as fixes: ES256 needs the JOSE raw signature (`dsaEncoding:'ieee-p1363'`, not
   DER); IAP localization/review-shot relationships traverse `/v2/` and are named `inAppPurchaseV2`
   but the *image* relationship is `inAppPurchase`; inline price creation needs the `${local-id}` id
   format; "Missing Metadata" on an IAP is the *availability* resource, not the review screenshot; and
   images signal readiness via `imageAsset`, screenshots via `assetDeliveryState`. **`--promote` BUILT
   (2026-07-07):** creates a `promotedPurchase` per IAP (`visibleForAllUsers`, relationship named
   `inAppPurchaseV2`) and sets the display order via `PATCH /v1/apps/{id}/relationships/promotedPurchases`
   = the ordered list Apple shows; paired app-side with `Store.swift`'s `PurchaseIntent.intents`
   listener (iOS 16.4+) so a tap from the store page / search lands in the purchase. **Still open:**
   per-locale `metadata/<locale>/` folders (the tool reads them but multi-locale isn't exercised) +
   an editor button + **`--custom-page` (Custom Product Pages)**. The copy *prose* (description/promo)
   stays maker-written per the two-part bar — the tool only pipes it.

   > **Custom Product Pages — a future `--custom-page <slug>` slice (deferred; growth-stage).** ASC
   > lets you publish up to **35** alternate product-page variants, each with its own screenshots,
   > preview videos, and promotional text (NOT title/subtitle/keywords/description/icon — those stay
   > shared) and its own **URL**. They're reached only by that link (an ad/tweet/newsletter), NOT App
   > Store search, so they're a **conversion/targeting** lever, not ASO — you point per-audience
   > traffic at a tailored page and measure conversion per page in App Analytics. The Tiny Jam fit:
   > one page per rack/tribe (an acid page for a techno channel, an e-piano page for a keys crowd) —
   > exactly the per-tribe campaign-link idea in
   > [`../marketing/tinyjam/tinyjam-marketing.md`](../marketing/tinyjam/tinyjam-marketing.md) §6.2.
   > API-manageable (`appCustomProductPages` + versions + localizations + their own screenshot sets),
   > so it mirrors the `--screenshots` dance under a CPP localization. **Deferred until there's traffic
   > to point at it** — nail the default page + launch first. NB: distinct from **Product Page
   > Optimization** (A/B test the *default* page, icon variants allowed) — that's a separate feature.

   > **IAP + ASO (the discovery angle):** Apple indexes an IAP's **display name** (≤30) for search —
   > "Acid Rack"/"E-Piano"/"Master Pass" extend the app's keyword surface for free, so spend IAP-name
   > taste on findable words, not the app's 100-char keyword field. The IAP **description** (≤45) is
   > NOT indexed (purchase-sheet blurb only) — same as the app description, which the App Store also
   > doesn't index (unlike Google Play). Only title/subtitle/keywords (+ IAP names) rank. A **promoted**
   > IAP (the 1024² image + the promote flag) becomes its own tappable search result — an extra surface.
3. **Search-Term-Rank popularity column** — feed `aso-research` real 1–100 popularity once
   Apple's beta reaches the account (App Store Connect → Analytics → Insights; §ASO deep-dive).
   *Interim free stand-in SHIPPED:* `aso-suggest.js` (Google-autocomplete demand proxy — §"the
   free trick"). Swap to Apple's real number when it unlocks.
4. **Parked niceties:** per-locale copy · a make-clip button + the recorder / live-loop
   ([`input-recording-looper.md`](input-recording-looper.md)) · the `0-apps → empty/banned`
   research flag. (~~app-scoped `store-shots`~~ + ~~app-scoped research~~ both SHIPPED —
   the Apps card's 🔎 research seeds `aso-research` from the carts' `de:meta`.)
5. **Post-launch: your OWN app metrics in the Apps view** — the honest counterpart to all the
   *proxy* signals above (research difficulty, Google demand): real downloads / impressions /
   conversion / %-from-search pulled from App Store Connect onto the app card, the one volume
   number that's truly yours (closes the ASO loop). **DEFERRED until after ship (maker's call,
   2026-07-03):** an empty repo app (Tiny Jam isn't live) has no data, and it needs the same ASC
   API key (`.p8`, JWT/ES256) as #2 — so it comes *after* the upload tool stands up that auth, not
   before. Reader-first is then the low-risk way to exercise the ASC auth.

## The one rule: scripts prepare → agent decides → scripts execute

ADR-0026 covers the deterministic plumbing: the upload dance, char-limit lint, the
`store-status.js` drift oracle. Those are **scripts** — deterministic given their inputs,
zero tokens, gate-able in CI. Keep everything there that can live there.

An **agent** is invoked *only* for the irreducible core: judgment, language, and
world-knowledge — the parts no oracle can score. And even then it runs in a sandwich:

1. **A script prepares** the input — renders the frames, assembles the page, diffs the
   versions, fetches the reviews. The agent never spends tokens *gathering*.
2. **The agent decides** — and emits its decision as a small structured artifact (a JSON
   of chosen frame indices, a copy block, a verdict). Minimal, auditable, reviewable.
3. **A script executes** the decision — deterministically consumes that artifact, renders
   the final asset, pushes it. The maker still reviews the diff before anything ships.

This keeps the agent's role small and honest, keeps the whole flow a CLI tool that streams
its log (share-panel rule 2), and keeps the maker-reviews-the-diff loop intact.

> **The test for "is this an agent job?":** if the output is deterministic given the inputs
> — a count, a diff, a format check, an asset render, an API call — it's a script. The
> agent is the *only* thing that can look at five frames and say which one sells the app.

## The features, each split at the boundary

Ordered pre-submission → submission → post-launch. For each: what the **script** does, and
the irreducible **agent** core.

### Pre-submission — author the product surface

**1. Hero-frame director** — the most on-theme one. Which screenshots tell the story?
- *Script:* render candidate frames headless via `play.js` + a committed `tools/clips/`
  track, at exact device resolutions; drop near-duplicate / loading / blank frames with
  cheap frame-diff heuristics; assemble a labelled contact sheet PNG. Then, once chosen:
  apply device frames, caption overlays, resize to the store spec matrix, push.
- *Agent:* look at the contact sheet, pick the 3–5 frames that read to a stranger who's
  never seen the cart, order them, draft captions. No oracle scores "which frame sells
  this." This is the ADR-0022 *legible-and-delightful-to-a-stranger* critic pointed at the
  store page.

**v0.1 SHIPPED (2026-07-03): `tools/store-shots.js`** — the deterministic asset leg. Takes a
native cart frame (`play.js --dump`) → App Store screenshots at exact device sizes
(iphone69 1290×2796, ipad13 2048×2732, …). ffmpeg-based, no node deps. **The full required-size
matrix (+ the "at least" upload set + video note) lives in
[`device-matrix.md`](device-matrix.md) §3** — the carried reference the store pipeline renders to.
> **The aspect-ratio gap, solved without engine work.** Carts render at one fixed lo-fi ratio
> (`SCREEN_W/H` are compile-time); App Store devices are other ratios (tall iPhone, 4:3 iPad),
> and responsive layout ([`responsive-layout.md`](responsive-layout.md)) isn't built. The fix:
> **composite, don't stretch** — integer-upscale the cart crisply (nearest-neighbor) and centre
> it on a designed background with the caption (default Comic Mono Bold — the editor's own
> font, so the store page ties back to the tool's identity; `--font` swaps it) in the breathing
> room. For a pixel console this
> reads *premium* (the canvas as a framed artifact), and it sidesteps responsive layout
> entirely. Two useful facts that shrink the problem: Apple only needs **one iPhone 6.9″ set**
> (auto-scales to smaller iPhones) + **iPad 13″** if you ship iPad — not a ratio matrix.
> Still to add (v0.2): frame-diff dedup (mpdecimate/fingerprint), palette-derived backgrounds,
> multi-line/bottom captions, optional device bezels.

**v0.1 SHIPPED (2026-07-03): `tools/store-contact.js`** — the hero-picker's deterministic half.
Turns a `play.js --dump` clip into a labelled contact sheet (evenly-sampled numbered tiles +
a printed tile→frame map), so the **agent leg works**: I read the sheet, pick the frames that
sell it to a stranger, and the map points each pick straight at a `store-shots.js --in`. The
full pipeline is now end-to-end: `dump → store-contact (sheet) → agent picks → store-shots
(render)`. Proven on groovebox. **Hero-director lesson from the first run:** the picker is only
as good as the clip — steady playback makes every tile look alike; a committed `tools/clips/`
track that *exercises* the cart (flip a page, sweep a knob, change the chord) yields genuinely
different, more sellable candidates. (v0.2: fingerprint dedup so near-identical tiles collapse.)

> These same asset generators (`store-shots.js`, `make-gif.js`) also feed the **press kit**
> ([`press-kit.md`](press-kit.md), a Channel-A own-domain artifact) — screenshots and a real-
> capture trailer are wanted by journalists exactly as they are by the store.

> **Compliance — composed screenshots are allowed; preview *videos* are stricter.** A common
> misconception is that the App Store bans "composed" (caption + background) screenshots. It
> doesn't — they're standard. The real rule is **Guideline 2.3.3: a screenshot must show the
> app *in use*** (actual UI/gameplay), not pure marketing art / title cards / splash screens.
> Our composites pass: the real cart render is always the focal point; the caption + bg are
> just the frame. Guardrails: make the **first 1–2** shots clearly show the app (they appear in
> search results), and keep captions **truthful** (2.3.1/2.3.7 — don't claim features the cart
> lacks). The stricter rule people half-remember is about **app preview videos**: those must be
> **real on-device capture**, not a marketing-composited edit — which our `make-gif` previews
> already are (actual runtime + real audio), so we're compliant there by default. (Guidelines
> reword over time and App Review is the final arbiter, but this split is long-standing.) This
> also seeds the §4 guidelines-pre-flight checks.

**2. Copy transcreation, not translation** — per-locale listing copy.
- *Script:* extract source copy from `de:meta` / the manifest; enforce char limits
  (name/subtitle 30, keywords 100, promo 170, desc 4000); assert per-locale completeness;
  assemble `metadata/<locale>/*.txt`; push all locales.
- *Agent:* write and *transcreate* the copy per locale — cultural adaptation of tone and
  idiom, not literal translation — plus a **back-translation review** (translate the nl→en,
  judge the meaning against the source, flag drift). Voice is irreducibly agentic.

**3. ASO keyword research** — fill the 100-char keyword field well. Full feasibility study
in [§ASO deep dive](#aso-deep-dive-what-keyword-data-actually-exists) below (what data
exists, what's a paywalled moat, what we build).
- *Script:* sweep the iTunes Search API per candidate → competitors + difficulty proxy +
  competitor-keyword mining; ingest a downloaded **Search Term Rank Report** (Apple's own
  1–100 popularity) to score candidates; char-count the field, flag words wasted because
  they already appear in the title/subtitle; snapshot it all to a dated JSON.
- *Agent:* seed terms by world-knowledge (what would *this* cart's player actually type?),
  relevance-filter to our cart, read the merged popularity×difficulty×relevance table and
  pick the final 100-char set per locale.

### Submission — the gate before you hit send

**4. Guidelines pre-flight — "will this pass review?"**
- *Script:* the mechanical compliance checks that map to a repo fact — export-compliance /
  encryption key present, privacy-manifest fields filled, no downloaded-code path (grep for
  the App-Store-Rule-2.5.2 / JIT / downloaded-wasm traps already scattered through the
  docs), IAP registered if referenced in copy.
- *Agent:* reason the cart against Apple's **prose** App Review Guidelines — content,
  metaphor, 4.3 design-spam, "does this read as a clone" — the parts that require reading
  rules that change and judging against them. Preventive twin of #7.

**5. Whole-page stranger-legibility audit** — `ui-audit.js` for the product page.
- *Script:* assemble the exact page as it will appear — title + subtitle + hero shots +
  description — into one preview render.
- *Agent:* role-play the stranger scrolling the store. Is the value clear in 3 seconds? Do
  the screenshots match the copy? Pure judgment, no lint possible. This is the store-page
  expression of the two-part bar's *soul* half.

**6. Release notes / "What's New" in the humble voice.**
- *Script:* diff the two versions — list changed carts and commits between tags, categorize
  new/fixed mechanically → a changelog.
- *Agent:* write the player-facing sentence in the project's actual tone from that
  changelog. The script lists; only the agent writes the line.

### Post-launch — deployment doesn't end at submit

**7. Rejection triage + appeal drafting.**
- *Script:* fetch the rejection from Resolution Center via the ASC API; extract the cited
  guideline refs; store it.
- *Agent:* read the prose reason, map it to a fix, and draft either a fix plan (routed into
  `de:meta.todo[]` / `cart-todos`) or an appeal. Reactive twin of #4.

**8. Review-response + bug-mining.**
- *Script:* fetch store reviews via the ASC API; filter by rating/date/locale; dedupe.
- *Agent:* draft replies in the honest/humble voice, and mine the free-text for actual bug
  reports → new `de:meta.todo[]` entries. Closes the loop from "player complains" back to
  the cart.

## Where the value concentrates

Both-highest-value-and-impossible-without-an-agent: **#1 (hero-frame director)** and
**#4/#5 (the will-it-pass + does-it-read critics)** — they're the store-page expression of
the project's two-part bar (*verifiable* AND *legible-to-a-stranger*). #2/#3 are clearly
agentic and high-value too. #6–#8 are real but can wait until after the first submission.

<a id="aso-deep-dive-what-keyword-data-actually-exists"></a>
## ASO deep dive — what keyword data actually exists (feasibility, 2026-07-03)

The question that kicked this off: can we build our own keyword tool instead of paying for
the complex ASO subscriptions (Sensor Tower, data.ai, AppTweak…)? Yes — for ~80% of their
value. Here's the honest map of what's a moat and what's free.

### The moat we can't cheaply cross: absolute search volume

The paid tools sell *one* thing we structurally can't get: **absolute search volume +
install/revenue estimates.** Apple knows every search; it sells almost none of it. The
whole industry exists to *reverse-engineer numbers Apple already has*, from two assets:

- **User panels** — a sample of real users' device behaviour, extrapolated to the whole
  market (Nielsen TV-ratings style). Acquired by **owning data-collecting consumer apps** —
  VPNs, ad-blockers, "data saver" utilities, market-research reward apps — or free
  developer-analytics dashboards that see their users' real numbers. (Sensor Tower ran VPN +
  ad-blocker apps; Facebook's Onavo did the same; several got pulled once exposed.)
- **SDK data** — a measurement SDK embedded in *thousands of other* apps, each streaming
  install/session telemetry back. Aggregate across the network = a giant behavioural panel
  living inside other people's apps.

Both are years-long, scale-and-surveillance **distribution assets**. A single indie with no
shipped cart has no sample to extrapolate and no SDK network — and the way you'd build one
is exactly what this project won't do. **So we accept: no absolute volume. Relative signals
only.** (Same posture as the rest of this repo — in-house and honest over a rented moat.)

### What Apple gives us free — three tiers (all tested 2026-07-03)

1. **iTunes Search API** ✅ — works, no key, no auth.
   `itunes.apple.com/search?term=X&entity=software&country=us&limit=200`. Per term → the
   apps that rank + `userRatingCount` (a solid competition-strength proxy) + full metadata
   (mine competitor titles/subtitles/descriptions for keywords we're missing). A **difficulty
   proxy** falls out: result density + summed top-10 rating counts = "how crowded, how
   strong." This is the workhorse.

2. **Autocomplete / search hints** ⚠️ — historically the *golden* source (real queries
   people type, ordered ≈ by popularity). The old public `MZSearchHints` endpoints now
   return **empty** (Apple locked them — verified); the modern `amp-api` suggestions need a
   bearer token scraped from the web App Store — fragile + ToS-gray. **Don't build on it.**

3. **Search Term Rank Report** ✅ ← the real find. Apple's **new** report (launched Oct
   2025, still beta), native to **App Store Connect → Reports → Insights (Beta) → "Monthly
   Search Term Rank Report"**, opened with your normal ASC login — no API, no ad account, no
   extra Apple fee (it's part of the paid Developer Program you already need to ship). *Note:*
   the "Indie / Pro / Marketing tiers" some blogs mention are **aso.dev's** re-selling
   product, not Apple's — the report itself is native ASC. Per search term, by genre +
   country, updated **monthly** (data back to July 2024). It returns:
   - **Rank in Genre** — position in the category (~1–500)
   - **Search Popularity in Genre (1–100)**
   - **Search Popularity overall (1–100)**
   - legacy Search Popularity (1–5) — the old Apple Ads scale (does *not* correlate with the
     new 1–100; different algorithm)
   - **Suggestions Count**

   First-party, free, the relative-popularity number the paid tools resell. **Key
   constraint: the query list is FIXED — Apple curates which terms appear per genre/country;
   you cannot add your own.** That's a feature for us, not just a limit: a candidate that's
   *absent* from the list is, by definition, long-tail / low-popularity — which is exactly
   what a brand-new cart with no ranking authority should target first. Monthly cadence = for
   *choosing* keywords, not A/B-ing them. Beta → structure/values may still shift.
   **Export path is unconfirmed:** the Analytics Reports API export announced in March 2026
   covers the two new *subscription* reports, not this one — so assume a manual CSV/UI export
   feeds our script until proven otherwise.

   **Real-world check (2026-07-03), the maker's account:** the report is **not release-gated**
   — Mipo Puppetmaker is live (123 first-time downloads, 14.2K impressions, 2.18% conv.) and
   its App Analytics is fully populated, yet the sidebar reads only *Overview · Acquisition ·
   Monetization · Retention · Benchmarks · Metrics* — **no "Insights."** So the Search Term
   Rank report is purely a **phased beta not yet on this account**; it will appear on its own,
   re-check periodically. What the account *does* prove: **Acquisition → Sources shows ~80%
   of product-page views come from App Store Search** (vs Browse) — i.e. for this app,
   keywords are the single highest-leverage lever. That's the whole justification for this
   tool, on live data.

### The Apple Ads (Search Ads) API — and why it's the *harder* path here

- **Auth:** OAuth2 **client-credentials**. Generate a P-256 EC key, register an API client
  in the Apple Ads UI, sign a client-secret **JWT (ES256)** with the private key, exchange
  it at Apple's OAuth token endpoint for a ~1-hour bearer token, then call with that token +
  an org-context header. Same crypto shape as the ASC / notarytool `.p8` setup. Creating an
  Apple Ads account is free; you do **not** have to run (pay for) campaigns to authenticate.
- **But it's a campaign-management API** — campaigns, ad groups, keywords, negative
  keywords, reports. It has **no clean public keyword-popularity / search-volume endpoint**;
  the popularity figure the tools show is scraped from the ASA *UI*, not a documented API
  field. So for pure *research*, the **Search Term Rank Report beats the ASA API**: same
  first-party data, zero OAuth plumbing, no ad account. Revisit the ASA API only if we ever
  actually run Search Ads campaigns.

### So the tool, revised

**v0.1 SHIPPED (2026-07-03): `tools/aso-research.js`** — the Search-API leg, no account
needed. Per seed term: a difficulty proxy (crowding × incumbent strength), the top
competitors, your app's own rank (`--app`), a genre mix, and pooled mined keyword
candidates. First real run on Mipo Puppetmaker found `marionette` EASY-and-unranked (a
keyword to add) and `puppet pals` EASY with the app already at #10 in its own Education genre
(a keyword to push) — while flagging `stop motion` as HARD (76k-rating incumbents) and
low-relevance. Still to add: ingest the Search Term Rank report for the real popularity
column (once the beta unlocks) and per-locale runs.

> **Mining upgraded (2026-07-03, "richer mining"):** candidates now come from competitor
> **TITLES *and* DESCRIPTIONS** (the API exposes `description`; the App Store *subtitle* it
> does NOT, so description is the recall source). Ranked by **document frequency** — how many
> *distinct* competitors use a word, so one app repeating it 30× can't win — with title hits
> weighted ~4× (the title is curated keyword real-estate) and a ★ marking title-sourced words.
> Descriptions get an expanded marketing/device/URL stoplist (else DF just surfaces
> `love/easy/beautiful/iphone`); category nouns (`synth`, `sampler`, `marionette`) are absent
> from every stoplist so they survive. JSON gains `minedDetail:[{word,apps,title,desc}]`;
> `mined:[[word,appCount]]` stays back-compat for the editor. Verified on `groovebox`/`drum
> machine` (surfaces `synth·sequencer·mixer·sampler·korg` that title-only mining missed).

**v0.1 SHIPPED (2026-07-03): `tools/aso-lint.js`** — the deterministic half of the metadata
composer. Offline, no account: char limits (title/subtitle 30, keywords 100), wasted
comma-spaces + stopwords + multi-word keyword entries, cross-field repeats (a word only
ranks once), and `--research` coverage. Exit 1 on issues (CI-friendly). It caught a live
one on Mipo: `create` sat in both the subtitle *and* the keyword field. What it deliberately
does NOT do is write the copy — the agent owns the taste (the read-aloud test; the visible
fields read like a person wrote them, all SEO in the hidden keyword field).

**v0.1 SHIPPED (2026-07-03): `tools/aso-compose.js`** — the *packer*: given the agent-written
title + subtitle and a **priority-ordered** candidate pool, it deterministically fills the
≤100-char keyword field (drops stopwords, drops words already visible so nothing ranks twice,
splits multi-word entries since Apple auto-combines singles, greedy-fills to the limit) and
reports **what got cut**. This is the whole composer loop: *agent drafts the visible fields
(taste) → `aso-compose` packs the keyword field (math) → `aso-lint` confirms clean*. A useful
emergent property surfaced on Tinyjams: the "didn't fit" list (`techno, acid, 808, chiptune,
lofi`) is exactly the set of **module-specific** terms that belong on the per-module **IAP
display names**, not the shared app field — the packer's overflow doubles as the
what-goes-on-the-IAP-tiles signal.

- **Script** `aso-research.js` (deterministic, snapshots a dated JSON): iTunes-Search-API
  sweep → competitor list + difficulty proxy + competitor-keyword mining; **ingest a
  downloaded Search Term Rank Report** and **match each candidate against the fixed list** —
  in-list → attach the real 1–100 popularity + genre rank; absent → tag "long-tail (low
  pop)". Either way the Search API supplies the competition side. Then char-count +
  title/subtitle-overlap lint on the 100-char field.
- **Agent:** seed terms by world-knowledge; relevance-filter to *our* cart; read the merged
  table (popularity × difficulty × relevance, presence-in-list as its own signal) and pick
  the final 100-char set per locale — biasing a fresh cart toward high-relevance long-tail
  terms it can actually rank for over crowded head terms it can't.
- **Ceiling:** relative popularity + competition strength, never absolute volume — and once
  a cart ships, **your own** real impressions/downloads/conversion in ASC closes the loop
  (the one volume number that's truly yours).

**First real application:** the Tinyjams US launch fields (title/subtitle/keywords + the
per-module IAP naming scheme) are worked out from this method in
[`../marketing/tinyjam/app-store-listing.md`](../marketing/tinyjam/app-store-listing.md) —
including the collection-app move of distributing keywords across promoted IAPs.

### The free trick while Apple's beta is dark: `tools/aso-suggest.js` (2026-07-03)

Re-probed the endpoints (2026-07-03): Apple's old `MZSearchHints` is **`<array></array>` —
dead**; the modern `amp-api` suggestions endpoint is **401 (bearer-token-gated)**. But
**Google Autocomplete** (`suggestqueries.google.com/complete/search?client=firefox`) is
**live, free, no auth**. `aso-suggest.js` is the **demand-side** twin of `aso-research.js`
(which only measures *competition*): for each seed it queries the bare term, `<seed> app`,
and the a–z **"alphabet soup"** expansion, harvesting the phrases real people type. Google
orders suggestions ~by query popularity, so a phrase's best position + appearance frequency =
a **demand proxy** — the free stand-in for the Search-Term-Rank popularity column until that
beta reaches the account. Honest ceiling (same as always): this is **Google / cross-platform
intent, not App Store volume** — and for broad seeds it leaks web/gaming/brand noise
(`xbox`, `minecraft`, `chrome`), killed by an expanded stoplist; the agent filters the rest.

**Phrases vs. words — two search engines, two outputs (maker's insight, 2026-07-03).** The
App Store and your own website are *different search engines with different ranking models*,
so the same harvest splits into two audiences:

| output | engine | why | consumed by |
|---|---|---|---|
| single **WORDS** (`beat, jam, sampler`) | **App Store** | Apple auto-combines singles + ignores stopwords; the 100-char field ranks on a word-soup | `aso-compose --candidates` → the keyword field |
| natural **PHRASES** (`drum machine app for ipad`) | **Google** (your site / press page) | Google ranks *phrases* / semantic intent, and these literally ARE Google queries | `press-kit` meta description, page `<title>`/headings, gallery SEO copy |

So `aso-suggest` emits both (`candidates` + `phrases` in `--json`). This is the ASO seam of
the channel split ([`sharing-channels.md`](sharing-channels.md)): **Channel B (App Store) eats
the words; Channel A (own domain — [`press-kit.md`](press-kit.md), the gallery) eats the
phrases.** Open follow-on: feed `phrases` into `press-kit.js` (a `<meta name="description">` +
og tags built from the top phrases) and `build-site.js` gallery pages — turning free Google
demand data into own-domain SEO for the same zero cost.

> Sources (2026-07-03): live probes of `itunes.apple.com/search`, `MZSearchHints` (empty),
> `amp-api` suggestions (401), and `suggestqueries.google.com` (live);
> Apple Search Term Rank Report writeup (aso.dev/aso/search-term-rank);
> Apple Ads API docs (developer.apple.com/documentation/apple_ads).

## Palette + mirror — surfacing keywords WITHOUT AI-generating the press kit

The maker's concern (2026-07-03), and the right one: keyword tooling must **never write the
press page** — that road ends in soulless, keyword-stuffed AI copy. The `press.md` prose stays
100% human (it has a voice; that's the ADR-0022 *legible-to-a-stranger* soul). So the tools give
a **palette and a mirror, never the painting**:

- **`tools/aso-brief.js` — the palette (before you write).** Generates a committed worksheet,
  `apps/<name>/seo-brief.md`, that you write *against*. It derives seeds from each cart's
  `de:meta` (teaches → category terms), runs `aso-research` + `aso-suggest`, and lays out: char
  budgets + your current listing with counts · **store WORDS** (priority order, ★ = both searched
  AND targeted) for the keyword field → `aso-compose` · **website/press PHRASES** people google →
  work into `press.md` in your own words · a **competition** table (difficulty + strongest
  incumbent per seed, so you know what's winnable). It emits *vocabulary + demand ranking only,
  never a sentence*. Committed + regenerable (ADR-0026's "copy/assets live next to the manifest");
  a trailing `<!-- aso-coverage … -->` block (invisible in rendered markdown) is the contract the
  mirror reads.
- **`tools/aso-coverage.js` — the mirror (after you write).** Reads your finished `press.md` +
  listing against the brief and reports (a) **phrase coverage** (which demand phrases your prose
  naturally hits; high-value misses first — you decide which fit), (b) **word coverage** (store
  words present anywhere), and (c) **stuffing** — any content word over-repeated in the prose,
  which reads spammy. Coverage gaps are advisory; **it exits non-zero only on stuffing** — i.e. it
  fails you for sounding robotic, not for leaving a keyword on the table. This is the ADR-0022
  two-part bar aimed at the store page: *discoverable* AND *still reads human*.

**Drift.** `seo-brief.md` is generated from *live* Google/App-Store data, so it rots on its own
clock — re-run before a launch pass. It carries a `de:driftable` marker
([`driftable-docs.md`](driftable-docs.md)); `stale-doc-check --driftable` (and the `cart-status`
back door) now scan `apps/*/seo-brief.md` and flag when the seeds/manifest move, though the
live-search drift is invisible to any file check (hence the standing "re-run" rule).

- **`tools/aso-score.js` — the scoreboard (the tweak loop).** Scores a listing
  (title/subtitle/keywords) on **budget · hygiene · reach · winnability** and **A/Bs a tweak against
  the committed listing with deltas**, so you tweak → score → tweak in the terminal.
  **Split surface (maker's call, 2026-07-03):** the *iterative A/B loop* lives in the CLI **with the
  agent** — I read the scorecard, propose a tweak, we re-score together (a fire-once button can't do
  that). But the editor Apps card gets a **📊 score glance** (`studio:aso-score` → `--deep --json`,
  rendered as bars + evidence) so you can *see where a listing stands* without the terminal — and,
  crucially, **the gotchas below render inline with the number**, because a bare score in a GUI reads
  as gospel in a way a terminal dump doesn't. Budget = are you using the
  char space; hygiene = the `aso-lint` waste rules as penalties; reach = worksheet demand-word
  coverage; **winnability (`--deep`, hits the network) = per-keyword difficulty from `aso-research` —
  `100 − difficulty`, so it flags HARD head terms a brand-new app can't rank for.** It prints the
  evidence (missed words, per-keyword difficulty), never a magic grade; **relevance and
  does-it-read-human stay the agent's + maker's call**, not the script's.
  - **Winnability is the trustworthy signal; reach is the weak one** (proven on Tiny Jam, 2026-07-03):
    the committed field scored winnability 33 — *7 of 15 keywords HARD* (`machine·83, studio·83,
    beat·79, drum·73` — generic singles); a specific rewrite (`acid, 303, 808, 909, chiptune, techno`)
    took winnability to 49 while reach *fell* (those terms aren't in the noisy brief word-list). The
    read: winnability + relevance beat a reach number you can game by stuffing brief-words. So reach
    wants denoising before its % is trusted; winnability + the missed-word list you filter by hand is
    where the meaningful tweaks come from. (Caveat: difficulty is scored per *single* word, but Apple
    combines your singles into phrases — so a HARD single may still pull a winnable long-tail in
    combination; use the bands as a lean, not a hard cut.)

The loop: **`aso-brief` (palette) → you write, in your voice → `aso-coverage` (mirror)**, with
**`aso-score` the terminal scoreboard** you + the agent iterate a listing on. No step generates prose.

### Apps view: IAP copy + clickable keyword "keys" (2026-07-03)

Two Apps-card additions so the *whole* searchable surface is visible and drillable:
- **IAP copy is shown, not just counted.** Each `iap.products[]` entry's **name (≤30) + description
  (≤45)** is its own App-Store-indexed search surface, so the card renders them with live char-count
  badges (red when over). This is where the keyword packer's *overflow* lands — the module-specific
  terms that don't fit the shared app field (`techno, acid, 808`) belong on the per-module IAP tiles.
- **Clickable keyword keys.** Every keyword-field entry and IAP name renders as a little clickable
  chip; clicking one **drops that term into the research box** (doesn't auto-run — you hit research
  to see its competition). Per-term drill-down, the manual companion to 🔬 analyze's batch. `listApps`
  now returns `iapProducts[]`; chips carry `data-term`.
- **Three "get it into the tools" granularities:** one key (click a chip) · **all keys → research
  box** (a button that gathers every app keyword + IAP name at once — the select-all companion) ·
  **load into all tools** (fills EVERY lab input from the manifest — lint's + compose's
  title/subtitle/keywords, the research box, the suggest seeds — and opens the advanced section).
  All three *fill, never run* — you drive.
- **📝 brief output is a clickable link.** The 📝 worksheet action renders a `📄 open
  apps/<name>/seo-brief.md` link (the `aso-brief` IPC returns the abs path; `open-path` re-checks
  it's inside the repo) so you jump straight to the file — same pattern as build-app's Finder link.

### Parked — smarter scoring (brainstorm 2026-07-03, not built)

Two directions surfaced while proving `aso-score`, worth building *only if* the loop earns it:

1. **Authority-aware winnability.** HARD isn't absolute — it's `difficulty vs. your app's ranking
   strength`. For a brand-new app HARD = unrankable (penalise it, as now); for an established app
   HARD head terms are winnable and *should* be chased. So winnability wants a `--stage
   new|growing|established` knob (or, post-launch, derive it from your *real* ASC rank data — the
   loop closing yet again: proxy difficulty × your actual authority). Today's score implicitly
   assumes `new`, which is correct for now but will mislead once an app has authority.
2. **A pack-and-score search.** Given a large candidate pool, loop `aso-compose` (greedy-pack) →
   `aso-score` (grade) over many packings and keep the best few. **Honest limit that keeps it out
   of autopilot:** it can only optimise the *scorable* dims (winnability/hygiene/budget/reach) —
   **relevance, the dimension that actually matters, isn't scorable**, so a pure maximiser would
   produce high-scoring keyword-stuffed *irrelevant* fields. The honest shape is *generate the top-N
   candidate sets → agent + maker pick* (the same palette-and-mirror rule; never full auto). Editor: per-app **📝 seo worksheet** + **🪞 check copy** buttons on the Apps card.
Open follow-on (was "feed phrases into press-kit"): the phrases now reach the press page *through
the human*, via the worksheet — the honest version of that idea.

## Open questions

- **Back-translation** (#2): agent-in-the-loop, or a script calling a translation API? Stays
  in-repo either way; the *judgment* of meaning-drift is the agent's.
- Do the agent's structured artifacts (chosen frames, copy blocks) get committed as
  versioned files next to the manifest — same ethos as ADR-0026's `metadata/<locale>/`
  folders — so a re-run is a diff, not a regeneration?
- Which of these want a deterministic oracle *underneath* the agent as a backstop, the way
  `mirror-diff` backstops a render? (#1's frame-dedup heuristic is one candidate.)
