# Demand discovery — mining a tribe for what to BUILD next

STATUS: BUILDING (2026-07-13): v0.1 SHIPPED (`tools/reddit-gaps.js`). The third demand tool.
Mines a tribe's public RSS for unmet asks and cross-references them against what this repo could
ship. Sits *above* [`demand-generation.md`](demand-generation.md) — it feeds that doc's **lever #1**
(a delightful product), the only lever that saves everything below it.

## The reframe: capture, generation, and now discovery

[`demand-generation.md`](demand-generation.md) split demand into *capture* (get found) and
*generation* (make people want it). This is a third, earlier job that both of those presuppose:

| | job | tool | when |
|---|---|---|---|
| **Capture** | get found by people already searching | `aso-*` | after you've built it |
| **Generation** | make people who weren't looking want it | `leads.js` | after you've built it |
| **Discovery** | decide *what to build* from what a tribe keeps asking for | **`reddit-gaps.js`** | *before* you build it |

Capture and generation both operate on a *finished* cart. Discovery operates on the *empty slot* —
it reads a tribe (per the "grab a tribe, one per module" rule) and asks: **what does this tribe
repeatedly ask for that (a) nobody's built well and (b) a humble lo-fi cart could actually ship?**

## Why RSS, not the API

Reddit's API is the obvious pipe and it's **shut**: the 2026 crackdown made app creation
approval-gated, the self-serve `prefs/apps` form is broken for everyone (redirects to the
Responsible-Builder-Policy, loops CAPTCHA, or silently rejects), and the `.json` endpoints `403`.
Its policy also flags *"non-commercial mining, scraping"* as needing written approval — so even a
granted key would be an awkward fit for a scraper.

But Reddit still serves **public Atom feeds** at `200`, and RSS is a documented reader feature — the
open front door, not circumvention:

- **listing feeds** — `/r/<sub>/top/.rss?t=year`, `/hot/.rss`, `/new/.rss` — the tribe's recent centre of gravity.
- **search feeds** — `/r/<sub>/search.rss?q="is there an app"&restrict_sr=1` — **this is the wish-miner**: aim it at a request phrase, get matching threads back.

Three honest limits, each designed around:

1. **Aggressive throttling.** Reddit 429s RSS on bursts. The tool fetches sequentially with `--delay`
   spacing, exponential backoff (4→8→16→32s), and an on-disk cache (`tools/reddit-gaps-cache/`,
   gitignored — we don't retain scraped data in the repo). Heavy testing can exhaust the per-IP
   budget; wait it out or raise `--delay`.
2. **No scores or comment counts in RSS.** Can't rank by upvotes. Instead rank by **cluster size** —
   many distinct threads asking the same thing = strong demand. Arguably sturdier than one viral post.
3. **Reddit may block RSS someday.** The fetch layer is the *only* Reddit-specific code; the
   `mine → cluster → cross-reference` engine is source-agnostic. If RSS dies, a paste-bridge (human
   copies threads) or open-web puller (App Store review RSS, forums) drops into the same engine.

## The pipeline

```
scan (RSS, cached) → wish-mine (regex) → cluster (topic taxonomy) → cross-reference cart shelf → rank GAPS
```

- **scan** — pull the built-in listing + search feeds (plus `--queries`), dedupe by post id, record
  which feeds surfaced each thread (a thread found by several probes is stronger signal).
- **wish-mine** — score each thread against `WISH_PATTERNS` (request / wish / frustration /
  alternative). Non-asks (chit-chat, show-offs) score 0 and drop out.
- **cluster** — assign each wish to topics via the `TOPICS` taxonomy (synths, samplers, generative,
  tape/lo-fi, chords, …). Cluster size per topic = demand strength.
- **cross-reference** — build a "what we cover" corpus from the music carts in
  `editor/public/carts/index.json` (title + description + `teaches[]` + genre) and count overlap per
  topic. **Low coverage + high demand = a gap we could fill.**
- **rank** — `gap-score = demand × grain × (uncovered ? 2 : 0.4)`, where **grain** is the judgment
  call baked into each topic: `core` (dead-centre — Tiny Jam territory), `edge` (plausible but off),
  `off` (not us — notation/DJ/utilities). A GAP = on-grain, ≥2 threads, uncovered.

Everything is deterministic: no model tokens, no auth, no deps. It **prepares the evidence**; the
human/agent reads the ranked gaps and decides (the store-agents "scripts prepare → human decides"
rule). The taxonomy and patterns are editable constants at the top of the tool — retune per tribe.

## Usage

```bash
node tools/reddit-gaps.js ipadmusic            # full run → ranked gap report (uses cache)
node tools/reddit-gaps.js ipadmusic --refresh --delay 4000   # re-fetch, wider spacing if throttled
node tools/reddit-gaps.js ipadmusic --raw      # also dump every mined wish thread
node tools/reddit-gaps.js ipadmusic --json     # machine-readable (editor glance / piping)
node tools/reddit-gaps.js --check              # offline self-test on a fixture (CI gate)
```

## Open / next

- **Coverage is coarse with a big library (the main v1 limitation, found on the first live run).**
  A GAP requires `coverage == 0`, but with 468 carts almost every topic has *some* matching cart, so
  binary coverage rarely hits zero and the automated "★ GAP" verdict rarely fires. The real value
  turned out to be the **ranked + clustered wish list** (`--raw`) for a human/agent to read, not the
  GAP flag. Two sharper signals to try: a **demand-to-coverage ratio** (not binary), and
  **sub-topic specificity** — a cart matching "synth" doesn't mean we serve the *specific* ask ("a
  live-performance all-in-one"). The r/ipadmusic run bore this out: loudest demand (MIDI control,
  stem separation, time-stretch) is off-grain *utility*; the on-grain, recurring asks were a
  **chord-play toy** ("store chords, play like a drum pad"), a **noodle playground** ("just fuck
  around, switch instruments") and **classic-gear homages** (ARP Odyssey, rompler) — none of which
  the binary GAP flag caught, but all of which the raw wish list surfaced.
- **The grain call is ours, not the tribe's.** A topic marked `off` never surfaces as a GAP even if
  demand is huge — that's deliberate (we won't build a notation app), but re-check the taxonomy's
  grain flags before trusting a "no gap" result.
- **Second source pipe** — an App Store *review* miner (Apple's public review RSS): 1-star "I wish it
  did X" reviews are concentrated gap signal and Apple *welcomes* that feed. Same engine, cleaner
  source. Scoped, not built.
- **Editor glance** — a `--json` payload exists; an Apps-card "gaps" glance (like leads.js `match
  --json`) could surface this in the IDE. Not wired.
- **Cross-tribe** — the tool is sub-agnostic; running it across several music subs (r/synthesizers,
  r/AudioProductionDeals, r/edmproduction) and merging clusters would harden the signal.

## Continuous fetching — the drip (set up 2026-07-13)

Reddit throttles *bursts*, not volume, so the way to keep data flowing is **one sub at a time,
generously spaced, never in a loop**. That's what `--drip` is for:

```
node tools/reddit-gaps.js --drip [subs.txt]   # fetch the STALEST sub in the rotation, once
```

It reads `tools/reddit-gaps-subs.txt` (one sub per line, committed + hand-editable), picks the sub
with the oldest cache, and fetches just that one. Successive runs round-robin the list and
accumulate caches — no burst, self-healing (a sub that fails stays "stalest" and is retried next
run). Keep the list to **music subs**: the wish-patterns + taxonomy are music-tuned.

A macOS **LaunchAgent** runs the drip every 6 h (≈ a full 7-sub rotation every ~2 days):
- `tools/reddit-gaps-drip.sh` — the runner (resolves node under launchd's bare env, `cd`s in).
- `~/Library/LaunchAgents/com.dreamengine.reddit-gaps-drip.plist` — the schedule (machine-local,
  NOT in the repo; `StartInterval 21600`, `RunAtLoad` off so the first fetch waits out the budget).
- Log: `tools/reddit-gaps-cache/drip.log` (gitignored).
- Manage: `launchctl bootout gui/$(id -u)/com.dreamengine.reddit-gaps-drip` to stop;
  `launchctl bootstrap gui/$(id -u) <plist>` to (re)start; `zsh tools/reddit-gaps-drip.sh` to fire once by hand.

The drip grows the **data** hands-off. Turning fresh caches into a **field note** stays a human/agent
step (interpretation is judgment, not automation — we don't auto-write prose): when a sub's cache
looks rich, read `--raw` and write the next numbered note.

## Where the findings live (and grow)

This doc is the **method** (the tool + how it works). The **findings** — what a run actually
discovered — go in the append-only research journal, **one numbered note per run**, so the record
grows without rewriting: [`field-notes/022-demand-discovery-ipadmusic.md`](../field-notes/022-demand-discovery-ipadmusic.md)
is the first (r/ipadmusic, 2026-07-13). Each note keeps the *interpretation* + Reddit thread
backlinks; the raw scraped threads stay in the gitignored cache, never committed. Run the tool on a
new tribe or a later date → write the next note (`023-…`); `tools/build-field-notes.js` re-indexes
them (timeline + related-note graph) automatically.

Related: [`demand-generation.md`](demand-generation.md) (the lever hierarchy this feeds),
[`store-agents.md`](store-agents.md) (the capture layer), `tools/leads.js` (the generation twin).
