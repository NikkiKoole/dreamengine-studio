# Store agents — the judgment layer above the App Store pipeline

STATUS: BUILDING (2026-07-03) — brainstorm + the first leg SHIPPED: `tools/aso-research.js`
v0.1 (the ASO Search-API leg, §ASO). Captures the *agentic* half of App Store deployment —
the work that decides whether an app gets **accepted** and **discovered**, which a REST
client can't do. [ADR-0026](../decisions/0026-store-pipeline-in-house-not-fastlane.md)
decided the plumbing (in-house ASC API tool, not Fastlane); this doc is the layer above it.
Sits under Channel B of [`sharing-channels.md`](sharing-channels.md) and the store rung of
[`share-panel.md`](share-panel.md); the ship-a-paid-app parts stay gated behind the same
product decision (which app first) and the original-palette rule, but keyword *research*
needs none of that and is live now.

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
candidates (competitor title words minus App-Store category labels). `--json` snapshots.
First real run on Mipo Puppetmaker found `marionette` EASY-and-unranked (a keyword to add)
and `puppet pals` EASY with the app already at #10 in its own Education genre (a keyword to
push) — while flagging `stop motion` as HARD (76k-rating incumbents) and low-relevance.
Still to add: ingest the Search Term Rank report for the real popularity column (once the
beta unlocks) and per-locale runs.

**v0.1 SHIPPED (2026-07-03): `tools/aso-lint.js`** — the deterministic half of the metadata
composer. Offline, no account: char limits (title/subtitle 30, keywords 100), wasted
comma-spaces + stopwords + multi-word keyword entries, cross-field repeats (a word only
ranks once), and `--research` coverage. Exit 1 on issues (CI-friendly). It caught a live
one on Mipo: `create` sat in both the subtitle *and* the keyword field. What it deliberately
does NOT do is write the copy — the agent owns the taste (the read-aloud test; the visible
fields read like a person wrote them, all SEO in the hidden keyword field).

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

> Sources (2026-07-03): live probes of `itunes.apple.com/search` + the `MZSearchHints`
> endpoint; Apple Search Term Rank Report writeup (aso.dev/aso/search-term-rank);
> Apple Ads API docs (developer.apple.com/documentation/apple_ads).

## Open questions

- **Back-translation** (#2): agent-in-the-loop, or a script calling a translation API? Stays
  in-repo either way; the *judgment* of meaning-drift is the agent's.
- Do the agent's structured artifacts (chosen frames, copy blocks) get committed as
  versioned files next to the manifest — same ethos as ADR-0026's `metadata/<locale>/`
  folders — so a re-run is a diff, not a regeneration?
- Which of these want a deterministic oracle *underneath* the agent as a backstop, the way
  `mirror-diff` backstops a render? (#1's frame-dedup heuristic is one candidate.)
