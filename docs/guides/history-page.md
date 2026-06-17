# the project history page (`docs/history.html`)

A generated, readable timeline of how dreamengine grew — a neo-brutalist "magazine"
view grouped **week by week**, opened from the editor's **Docs tab → ★ history**. It
exists so the whole arc (what API landed when, which carts it spawned, which subsystems
belong together, what mattered more than what) is legible at a glance instead of buried
in 1400+ commits.

It is **structure-first**: a small hand-authored file supplies the *judgment* (chapters,
groupings, importance, the marked days, an editorial seam), and a generator derives every
*fact* (dates, commit counts, ADRs, carts born, the per-era hero cart) from git +
`index.json`. So it stays honest with the repo and barely rots.

## The three pieces

| File | Role |
|---|---|
| [`docs/history-spine.json`](../history-spine.json) | **hand-authored structure** — weeks, eras, subsystems + their commit-matchers, importance tiers, marked days, milestones, the (mostly empty) editorial seam, and the **`observations` retrospect** (cross-week conclusions). The only file you edit by hand. |
| [`tools/build-history.js`](../../tools/build-history.js) | the **generator** — reads the spine + git + `index.json`, emits `docs/history.html`. Plain node. |
| `docs/history.html` | the **generated page** (self-contained: inline CSS/JS, fonts from Google Fonts, hero thumbnails inlined as data URIs). Don't hand-edit — regenerate. |

Editor wiring (so it shows up): a `.html` content-type case in `editor/vite.config.js`,
and the **★ history** sidebar item + iframe + cart-loading + scroll-memory in
`editor/src/shell.js` / `shell.css`.

## Regenerate

```bash
node tools/build-history.js          # rewrites docs/history.html
```

Re-run after editing the spine (or to refresh derived data — new commits/carts/heroes).
The page is served fresh by the dev server; reopen **★ history** to reload the iframe.

## What's derived vs authored

- **Derived (never hand-write):** every date, the per-day and per-week commit counts, the
  growth bars, ADRs landed per era, carts born per era, the **hero cart** (the cart
  with the most *source* `.c` commits in that era's window — sized bigger the more it was
  worked), and the **design-note spotlight + "roads not taken"** (see below). Subsystem
  *tags* on each commit come from the matchers in the spine.
- **Authored (the spine):** the week boundaries + taglines, the era titles + date ranges +
  blurbs, the subsystem list + matchers + tiers, the marked-day callouts, and the
  milestone cards (each a title + tier + a `match` substring that the generator resolves to
  a real commit's date — a build warning prints if a matcher hits nothing).
- **The editorial seam:** each era has an `editorial` field, empty by default. Fill it with
  voiced prose (following [`voice.md`](voice.md)) when you want a chapter to read in
  Nikki's voice; leave empty and the neutral `blurb` carries it.

## Mining design-doc content (automatic)

The generator reads docs as *content*, not just names — so the design notes enrich the
story on their own. Two surfaces, both derived, both zero-authoring:

- **The design-note spotlight** — a foldout sibling to the hero cart, one per era: the
  design doc *born that era that spawned the most carts*. It shows the doc's friendly
  one-liner + the carts that grew from it (click a thumb to load the cart, or open the
  full note). Picked automatically; override per era with `eras[].spotlightDoc:
  "<doc-name>"` in the spine when you want a specific note featured.
  - **Friendly summaries come from `docs/README.md`.** That file is the curated map —
    every design/guide doc has a hand-written one-line description there (often
    STATUS-tagged: `REFLECTION`, `SPEC`, `PROPOSAL`, `IN PROGRESS`…). The generator parses
    the layout tree into `name → {desc, status}` and reuses it verbatim. **So the single
    maintenance rule is: keep a doc's README line good and the page follows** — nothing is
    re-authored in the spine.
  - **"Carts that grew from this"** is a content scan: cart names are matched against the
    real cart list from `index.json`, and counted **only on a strong signal** — a `.c`
    suffix or a `` `backtick` `` wrap (how docs actually cite carts). Bare prose words are
    ignored, so common-word cart names (`needs`, `house`, `patterns`, `effects`…) don't
    false-fire. The same scan drives the spotlight ranking and the "🟡 grew" mark on the
    *Design notes written* tags (whose tooltips show the README one-liner).

- **"Roads not taken"** — the **cut decisions**, split out of *Decisions landed* into their
  own row, dashed + struck. Detected automatically by the `cut-` ADR filename; each shows
  the ADR's real H1 title with its first-paragraph rationale on hover. This is the
  deliberate negative space — features removed to keep the lesson honest — and it's some of
  the richest narrative the timeline carries (see observation 2 in the retrospect).

**The cut ADRs get a voiced line.** The "roads not taken" cards show a readable, in-voice
sentence instead of the dry ADR context paragraph — authored in the spine's
`decisionNotes` map, keyed by ADR basename (no `.md`). No entry = auto-fallback to the
ADR's first-paragraph summary. Follows [`voice.md`](voice.md).

**Handoffs split out as "⛏ Open threads".** Any design note whose filename contains
`handoff` is pulled out of *Design notes written* into its own blue row per era — work
parked mid-thread.

## Research threads

A band of **long investigations** (before the retrospect): a topic that spawned a trail of
docs, handoffs and carts — roads-&-world-gen, the instrument-engine ports, the effects bus,
the radio stations, mobile/touch, spatial audio. Each is a foldout with a voiced blurb, its
cross-referenced docs as chips (notes / handoffs / decisions), and the carts that grew from
it as thumbnails.

Declared in the spine's `threads.items[]`, modelled exactly like `subsystems` —
**automatic membership:**
```json
{ "id": "roads", "title": "Roads & a world to drive on", "seed": "procgen-places.md",
  "match": ["roadnet","road-geometry","streetlevel","procgen","interchange","sloop", …],
  "blurb": "<voiced — follows voice.md>" }
```
The generator collects every doc/ADR whose **filename** matches a `match` term, splits them
into notes / handoffs / decisions, unions the carts (from the per-doc strong-signal scan)
ranked by how many member docs cite each, and derives the date span + counts. **A new
matching doc, handoff or cart folds in on the next regenerate — no editing.** The only
hand-authored parts are `title`, `seed`, and the voiced `blurb` (empty `blurb` → a derived
structural sentence). Threads draw from the same doc universe as the rest of the page
(`design/` + `guides/`), so a top-level handoff like `docs/tuning-handoff.md` won't appear —
move it under `design/` if it should.

> Still untapped when refreshing the retrospect: `STATUS.md`'s "Decided-against / deferred"
> ledger (a fuller list than the cut-ADRs).

> **Coming: moving thumbnails.** A video exporter is in the works; when it lands, cart
> clips (webm/gif) will upgrade the still thumbnails in the hero / spotlight / thread-carts
> slots, PNG as poster. The path + naming convention is fixed in
> [`../design/cart-clips.md`](../design/cart-clips.md) so the auto-detection drops straight
> in (glob `carts/clips/<cart>/*`, first of webm > mp4 > gif, caption from the filename).

## Tools built + milestone commit detail (automatic)

Two more derived surfaces, both zero-authoring:

- **🔧 Tools built** — a green row per era listing the `tools/*.js|sh` files born that era
  (the toolsmith trail: `make-cart` → the linters → the web build → the audio-analysis
  suite). Excludes `*.cart.js` configs and `tools/carts/`. Each chip's tooltip is the
  tool's **own header comment** (the first `//`/`#` line after any shebang, with a leading
  `toolname —` stripped) — so keep that line good and the page follows. Chips aren't
  clickable (no in-editor route serves `tools/`).
- **Milestone hover** — every milestone card resolves to its first matching commit; hover
  shows a **friendly blurb** about what landed: the commit's own subject as the headline,
  plus a cleaned-up summary of its body (the `- filename.ext:` file-scaffolding and the hash
  are stripped, bullets flowed into one line). Commit bodies are captured with a
  record-separator git format so their newlines survive. A true prose summary needs a human,
  so each milestone takes an optional authored **`note`** (voiced, follows [`voice.md`](voice.md))
  that replaces the derived blurb — no note = the derived fallback, which self-refreshes from git.

## The retrospect — cross-week observations

The spine's `observations` block renders as a **Retrospect** capstone at the foot of the
page (and a ★ entry in the contents): a numbered card grid of conclusions that only
surface from reading the *whole* window back at once — things invisible commit-by-commit
or even era-by-era (e.g. "this became a synthesizer with a screen"; "one spec set three
weeks' trajectory"). It's distinct from the per-era `editorial` seam: editorials annotate
one chapter from inside it; observations look down on the entire arc.

Each item is `{ title, body, evidence }` — and **`evidence` must cite a number the page
can re-derive** (a milestone-by-subsystem ratio, a per-day commit peak, a cart total), so
the retrospect stays as honest with the repo as the rest of the page. The block also
carries `asOf` (the date it was last refreshed) and `recipe` (how it was produced).

### The recipe — how to refresh it (rerun in due time)

This is the move, kept so it can be repeated as the window grows. **After** the page +
spine are written, read them *back* as a finished whole and ask the question that started
it:

> "since we've written the docs/history code and text, can you now read it back and come
> to insights/conclusions you couldn't before?"

Concretely:
1. Read the rendered `docs/history.html`, the spine, and the raw counts together:
   ```bash
   git log --since=<from> --date=short --pretty=%ad | sort | uniq -c   # per-day density
   grep -o '"subsystem": "[a-z0-9]*"' docs/history-spine.json | sort | uniq -c | sort -rn  # milestone mix
   ```
2. Look for what's only visible in aggregate: where the project's *revealed* identity
   diverges from its stated one, single artifacts with outsized downstream reach, the
   shape of the throughput curve, recurring patterns (e.g. abstraction trailing
   repetition), and long threads that span weeks.
3. Ground every claim in a derived number → that's the `evidence` line.
4. Rewrite `observations.items`, bump `asOf`, `node tools/build-history.js`.

> Caveat learned the hard way: **per-day commit counts shift with timezone / day-boundary
> choices** — cross-check before calling any single day a peak or a lull. (An apparent
> "06-13 lull" was a date-grouping artifact, not a real quiet day.)

## Adding a new week

Once a week's worth of work has landed, extend the spine and regenerate:

1. **Widen the window** — `window.to` → the new end date; bump `window.label` (e.g.
   `"Weeks 1–4"`).
2. **Add the week** to `weeks[]` — `{ id, label, from, to, tagline }`. (Empty weeks are
   skipped, so you can pre-declare future weeks harmlessly.)
3. **Reconstruct its eras** from git — the quick way:
   ```bash
   git log --reverse --format="%ad %s" --date=format:"%m-%d %H:%M" \
     --since="<weekstart> 00:00" --until="<weekend+1> 00:00"
   ```
   Read the arc, carve it into ~4 eras, add them to `eras[]` (each `{ id, title, from,
   to, blurb }` — date ranges must tile the week with no gaps).
4. **Add milestones** for the prominent landings — `{ title, tier, era, subsystem?, match }`
   where `match` is a unique substring of the real commit subject. Foundational tier = the
   big filled cards.
5. **Add marked days** for the standout days (`markedDays[date] = "why it mattered"`); days
   over `denseDayThreshold` also auto-get a 🔥 badge.
6. **New subsystems?** Add to `subsystems[]` with matchers if the week introduced a new
   through-line (that's how `radio`, `effects`, `roads` got added).
7. `node tools/build-history.js` and check the console for any unmatched-milestone warnings.

> Heroes and growth stats compute themselves — you only describe the *shape*.

## Possible automation (not built)

The derived half could be refreshed automatically (a hook that re-runs the generator on
commit, or a scheduled "scaffold next week" that pre-adds an empty week entry when a new
week boundary passes). The **eras, milestones and blurbs still need human judgment**, so a
fully-automatic "add a week" can't write good chapters on its own — the realistic
automation is *keep the derived data fresh + scaffold the empty week for a human to fill*.
Not wired up yet; see the conversation that built this.
