# Launch sequence — build → publish → broadcast, and the gift-first/live tension

STATUS: EXPLORING / strategy note (2026-07-18). The *order* you do a launch in, and the one
trap that order avoids. Companion to [`demand-generation.md`](demand-generation.md) (which ranks the
*levers*); this doc is about *timing* them. Written while planning acidcandy's (→ Tiny Jam) launch.

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

## For acidcandy specifically

- **Unit:** acidcandy is the standalone *web hook*; **"Get on iPhone" → Tiny Jam** (the bundle it
  ships inside — see [`share-panel.md`](share-panel.md): the App Store unit is a *set*). One killer
  free web toy is a better HN magnet than "here's a bundle," and it upsells the whole app.
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
