# Launch sequence — build → publish → broadcast, and the gift-first/live tension

STATUS: EXPLORING / strategy note (2026-07-18). The *order* you do a launch in, and the one
trap that order avoids. Companion to [`demand-generation.md`](demand-generation.md) (which ranks the
*levers*); this doc is about *timing* them. Written while planning **Tiny Acid Jam**'s launch (the
standalone app around the `acidcandy` cart, renamed 2026-07-19 from "Acid Candy" — the old name
collided with candy-match-3 games *and* a real sour-candy brand, and "Tiny Acid Jam" pre-brands into
the future Tiny Jam umbrella).

## The core sequence

The web build and the store app are one funnel: **web = try (frictionless, shareable), app = keep
(pocket, native, offline, saves).** For a *cute pocket toy* the app's whole pitch is "I want this
thing *with me*," not "unlock more features." So:

1. **Make it as good as you can.** Iterate privately / on early web builds.
2. **Publish the iOS app — get it LIVE on the Store.** (`tools/asc-push.js` → App Store Connect.)
3. **ONLY THEN broadcast** on Hacker News (**Show HN**) + **r/InternetIsBeautiful** + Product Hunt,
   pointing at the **full wasm demo** with a tasteful **"Get on iPhone"** button → the live listing.

**Why this order — the broad-launch spike is ONE-SHOT.** You get one good launch day per thing on
HN/IIB/PH. If you post the web toy before the app is live, the "Get on iPhone" button 404s or says
"coming soon," and you've burned the spike with no way to rerun it. Live app first, *then* the megaphone.

Two rules that ride along with step 3:
- **The web demo must be GENEROUS, not crippled.** The HN/IIB crowds *punish* nagware — a demo
  that's obviously a download-trap gets torn apart in the comments and kills the launch. Let them
  actually play the real thing; the app still sells on pocket/native/offline/persistence.
- **The button is a bridge, not a blocker.** A small corner badge, or one that fades in after a bit
  of play — never a modal that interrupts. Restraint reads as confidence to these crowds.

## The tension: "what can I improve?" clashes with "it's live on the Store"

The dedicated tribes (acid-house forums, KVR, r/pocketoperators, the acid Discords — see
`tools/leads.js`) run on a **gift-first, collaborative** promise: *"I'm making this — what would you
change?"* That framing is honest **only while the thing is genuinely unfinished.** Once it's a
**live, paid** product, "help me improve it" reads as *disguised marketing* — and those tribes have
finely-tuned radar for exactly that. That's the clash: the broad launch wants a *finished* thing;
the tribe outreach wants an *unfinished* one.

**Resolution — the two audiences want opposite things at opposite times, so you SEQUENCE them, not
run them together:**

| audience | what they want | honest window | the ask |
|---|---|---|---|
| **Dedicated tribes** (acid-house, KVR, pocket-op) | to shape it, feel ownership | **BEFORE launch** — while it's real WIP (web beta / TestFlight) | *"I'm building an acid toy — what would make it feel right?"* |
| **Broad launch** (HN, IIB, Product Hunt) | a finished thing they can try now | **AT launch** — app live | *"Show HN: a pocket acid machine in your browser"* |
| **Tribes again, post-launch** | to see their input shipped | **AFTER launch** | *"you helped shape this — it's out"* + *"here's what I'm adding next"* |

So the gift-first *"what can I improve?"* is a **pre-launch move.** You take it to the acid-house
tribe *while the web build is still beta and the app isn't submitted yet* — their feedback is real,
they get invested, and they become your day-one advocates. After the app is live you **don't** open a
tribe with fake humility; the honest post-launch tribe move is **gratitude + roadmap** — "you helped
build this, it's out, here's what's next" — which stays honest because software genuinely keeps
evolving through updates. (Consistent with [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)'s
be-honest bar and store-agents' "no invented prose" rule: always be honest about the product's stage.)

### The timeline, end to end

```
 build it good ──► gift-first to the TRIBES (web beta / TestFlight: "what would you change?")
                        │  (feedback shapes it; advocates seeded)
                        ▼
                   publish iOS app  →  it's LIVE
                        │
                        ▼
                   BROADCAST: Show HN + r/InternetIsBeautiful + Product Hunt
                   (full wasm demo + "Get on iPhone" button → live listing)
                        │
                        ▼
                   back to the TRIBES: "you helped shape this — it's out" + roadmap
```

## Packaging: ship the "single" first, build the umbrella later

Separate decision from *timing*: what's the **store unit** — a standalone toy, or the Tiny Jam
umbrella (one app, many toys, small per-toy prices)? The answer for the FIRST launch is **a
standalone single, not the umbrella.**

**Don't launch the umbrella with one toy.** An umbrella's whole value is *the collection*; with one
toy inside, the per-toy IAP pitch ("buy more") points at nothing, it's a thin/weak App Store
proposition (mild review risk), and you pay the StoreKit/IAP complexity tax for zero benefit. If you
*do* go umbrella, the floor is **3, not 1 or 2** (two reads as "we ran out"; three+ reads as "a
growing collection").

**Ship Tiny Acid Jam (the `acidcandy` cart) standalone first** because it:
1. gets you **LIVE fastest** — the thing step 2 of the sequence gates everything on;
2. is the **simplest first trip through review** (certs, screenshots, guidelines, likely a rejection
   or two) — learn the gauntlet on one small toy, not a multi-cart app with IAP;
3. is a **cleaner product + cleaner Show HN story** ("a pocket acid machine" beats "an umbrella app");
4. **de-risks the umbrella** — validate people want the toys before investing in the catalog.

**The migration worry isn't real — single-then-album.** Shipping Tiny Acid Jam alone now doesn't trap
you: it can live *both* as a standalone "single" *and* later as one instrument inside Tiny Jam (a
song released as a single *and* on the album). You don't kill the standalone; no corner painted.

**When the umbrella earns its keep:** once you have **~3–4 toys you're genuinely proud of** (the
[field-note 026](../field-notes/026-demand-discovery-cross-tribe.md) demand list is the menu). That's
when the collection feel is real, the small-price IAP model makes sense, AND it becomes the right
home to **avoid Apple's guideline 4.3 "spam"** flag — a few standalone flagships is fine, but a pile
of near-identical single-cart apps gets dinged; the umbrella consolidates breadth without tripping it.

**Concretely:** (1) now → ship **Tiny Acid Jam standalone** (paid, or free + one unlock); run its launch.
(2) next → build 2–3 more toys you love. (3) then → launch **Tiny Jam** as the umbrella, folding
Tiny Acid Jam in as a "single." (`tools/build-app.js` grows from one cart to the set — adding a rack is
one manifest line — so the single→umbrella path is one codebase, not a rewrite.)

## For Tiny Acid Jam specifically

- **Unit (revised — see Packaging above):** for the FIRST launch the store unit is **standalone
  Tiny Acid Jam**, so **"Get on iPhone" → the Tiny Acid Jam listing**, not Tiny Jam. Tiny Jam (the *set* —
  [`share-panel.md`](share-panel.md)) comes later, once there are ~3–4 toys; then the button repoints
  to the umbrella. Tiny Acid Jam (the `acidcandy` cart) stays the free web hook throughout.
- **Pre-launch tribe pass:** the acid-house Discords + r/pocketoperators (added to
  `tools/leads-ledger.json`) get the *"what would make this squelch feel right?"* web-beta ask —
  before the Tiny Jam build is submitted.
- **Launch:** Show HN + r/InternetIsBeautiful, gift-first, playable link first. See the Show HN voice
  notes in [`store-agents.md`](store-agents.md) / draft via `node tools/leads.js draft acidcandy`.

## See also

- [`demand-generation.md`](demand-generation.md) — the *lever* hierarchy (what drives downloads); this doc *times* those levers.
- [`store-agents.md`](store-agents.md) — the store/ASO judgment layer + Show HN / listing voice.
- [`demand-discovery.md`](demand-discovery.md) — what to build *before* any of this (the reddit-gaps → build-list pipeline).
- `tools/leads.js` — the tribe→venue map the pre-launch and post-launch tribe passes run on.
