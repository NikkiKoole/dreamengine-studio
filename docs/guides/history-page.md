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
  *tags* on each commit come from the matchers in the spine. **Cart metadata —
  `description`, `lineage`, and `teaches[]` — is pulled straight from `index.json`** (the
  same fields that drive the ★ techniques compendium): the hero figure shows the cart's
  `lineage` one-liner ("what it descends from / what's new"), every *carts-born* hover card
  shows description + lineage + the `teaches` chips, and spotlight/thread thumbnails carry
  the lineage as a tooltip. So tagging a cart for the compendium enriches the timeline for
  free — keep `index.json` good and the page follows.
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
  design doc *born that era that mentions the most carts*. It shows the doc's friendly
  one-liner + the carts it mentions (click a thumb to load the cart, or open the
  full note). Picked automatically; override per era with `eras[].spotlightDoc:
  "<doc-name>"` in the spine when you want a specific note featured.
  - **Friendly summaries come from `docs/README.md`.** That file is the curated map —
    every design/guide doc has a hand-written one-line description there (often
    STATUS-tagged: `REFLECTION`, `SPEC`, `PROPOSAL`, `IN PROGRESS`…). The generator parses
    the layout tree into `name → {desc, status}` and reuses it verbatim. **So the single
    maintenance rule is: keep a doc's README line good and the page follows** — nothing is
    re-authored in the spine.
  - **"Carts these notes mention"** is a content scan — and note the label: it's **mentions,
    not lineage**. Cart names are matched against the real cart list from `index.json`, counted
    **only on a strong signal** — a `.c` suffix or a `` `backtick` `` wrap (how docs actually
    cite carts) — so bare prose words don't false-fire. Two extra precision rules (the scan
    used to read as nonsense — e.g. the optimization thread listing the carts it *benchmarked*
    as if they "grew from" it): **(1)** a cart name that also collides with a `studio.h` symbol
    (`eq`↔`eq()`, `varispeed`, `shimmer`…) or a concept word (`effects`) is **ambiguous** and
    attaches ONLY on the `.c` file-reference signal, never a bare backtick (a doc backticking
    the `eq()` function no longer drags in the `eq` cart); **(2)** a thread ranks its carts by
    **summed reference weight** (`.c` = 2, backtick = 1), not by how many docs mention them, so
    a deliberate file reference outranks an incidental mention. The same scan drives the
    spotlight ranking and the "🟡 grew" mark on the *Design notes written* tags (whose tooltips
    show the README one-liner). See `cartsReferencedIn()` in `build-history.js`.

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
cross-referenced docs as chips (notes / handoffs / decisions), and the carts those docs
mention as thumbnails.

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

### Deferred: a "frontier" band + the todo-debt number

Two present-tense sources now exist that the page doesn't yet read — worth folding in on the
**same "read it all back" pass as the retrospect** (they're reflection surfaces, not per-era facts):

- **A "frontier — where it stands now" band**, fed by `tools/handoff.js`'s active-lane parse of
  `docs/HANDOFF.md` (the ▶ ACTIVE THREAD lanes + ages + resume-at pointers). It's the natural
  complement to the Retrospect: the retrospect looks *back* at the whole arc; this shows the *open
  edge right now*. Fully derived, zero-authoring, and when a lane lands it just drops off. Place it
  as a capstone by the Retrospect. **Do NOT weave lanes into a dated past era** — `HANDOFF.md` is a
  rolling, pruned file (a lane vanishes when its work ships), so a lane pinned to a past era would
  silently disappear on the next regen and break the "honest with git" contract. Keep it a separate
  present-tense surface. (The existing per-era "⛏ Open threads" — filename-dated `design/*handoff*.md`
  — stays as is; that's the retrospective handoff view and it's fine.)
- **The polish-debt number as retrospect *evidence*, not a list.** `node tools/cart-todos.js --count`
  (the per-cart `de:meta.todo[]` punch-lists — 292 open across 153 carts at 2026-07-07) is a good
  `observations` evidence line ("the polish the surface hides"). Do **not** render the todos as a
  per-era or per-cart surface — hundreds of forward-looking items would drown the narrative; that
  view already lives in `cart-todos.js` / the compendium.

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
7. **New research thread — or an orphaned doc?** Re-check `threads.items[]`: a week can
   start (or advance to a milestone) a long investigation no thread yet catches — declare
   a new `{ id, title, seed, match, blurb }` (that's how `multiplayer` and `storefront`
   were added) — and it can spawn docs an *existing* thread should adopt but its `match`
   misses (that's how `device-adaptive-layout.md` / `device-matrix.md` got folded into
   `mobile`). Threads auto-collect by filename, but the topic + voiced blurb are hand-authored;
   eyeball the `design/` docs the week added against every thread's `match` — a design doc that
   no thread's terms catch is either a new thread or an orphan an existing thread should adopt.
8. `node tools/build-history.js` and check the console for any unmatched-milestone warnings.

> Heroes and growth stats compute themselves — you only describe the *shape*.

## Moving thumbnails — clip support (proposed wiring)

The page shows a still `.cart.png` in three slots — the **hero** (`heroByEra()`), the
**spotlight** carts (the design note's "carts these notes mention"), and **thread** carts.
Those are the natural homes for a short looping clip instead of a still. The exporter
(`tools/make-gif.js`, [`debug-harness.md`](debug-harness.md) → "Clip capture") already
targets the convention below, so the generator can light this up with no per-cart wiring.

**The convention (locked — the exporter writes to it):**

```
editor/public/clips/<cart>/NN-label.webm    e.g. clips/coaster/01-the-ride.webm
                                                 clips/coaster/02-scream-tuning.webm
```

- **A sibling of `carts/`, not nested inside it.** `carts/` stays a flat store of cart data
  (the picker reads `index.json`, two git-log scans filter to `*.cart.png`); `clips/` is the
  media root — a peer of `palettes/` under `editor/public/`. Keeps every "just glob `carts/`"
  tool correct and the data store homogeneous. The `.cart.png` stays the poster/fallback.
- A **per-cart subfolder** answers "multiple clips per cart" cleanly — one cart = one
  folder, any number of clips — and folders exist only for carts that *have* video, so the
  tree stays sparse (not 391 empty dirs).
- `NN-label` carries **order + caption with no sidecar metadata** — same trick the tools row
  uses: strip `NN-`, dash→space → "the ride", "scream tuning". Derive-from-structure, like
  everything else here.
- `node tools/make-gif.js coaster --clip the-ride --from ride.beats` files straight into
  place (`editor/public/clips/coaster/01-the-ride.webm`) and auto-assigns the next `NN`.

**Generator side (the sketch — not built yet):**

1. A helper `clipsFor(name)` that globs `editor/public/clips/<name>/*.{webm,gif}`, sorts by
   filename, and derives `{ src, caption }` for each (caption = `NN-label` → words). Empty
   array = fall back to the still, exactly like today.
2. At each of the three emit points (hero ~`build-history.js:953`, spotlight ~`:304`,
   thread ~`:339`): if `clipsFor(name)` is non-empty, emit a `<video>` with the existing
   inlined PNG as `poster=` (so autoplay-blocked viewers and the standalone-file case still
   see the still), `autoplay muted loop playsinline preload="none"`, lazy. Otherwise the
   current `<img>`. Keep `image-rendering:pixelated` on both.
3. **Path, not base64.** Stills are inlined as base64 data URIs so the HTML is
   self-contained; videos are far too heavy for that. Reference them by a **root-absolute**
   URL — `/clips/<name>/NN.webm` — which resolves because Vite serves `editor/public/` at the
   web root (so does the published `site/`). The poster PNG stays inlined, so the page still
   *renders* standalone; only the motion needs the files present.

**Two decisions still yours (they don't change the layout, only the plumbing):**

1. **Do the binaries go in git, and how?** Direct commit is simplest but adds real weight,
   and `site/` would carry copies. Alternatives: git-lfs, or keep them out of the repo and
   emit into `site/` only at publish time. Lean: direct commit, keep clips short/small until
   volume forces lfs.
2. **`webm` vs `gif` default.** webm (VP9) is smaller + crisper with the PNG poster, but gif
   is the safe fallback where autoplay is blocked. The glob accepts both; if a cart has a
   `.webm` *and* a `.gif`, decide whether webm wins or both are offered as `<source>`s.

## Possible automation (not built)

The derived half could be refreshed automatically (a hook that re-runs the generator on
commit, or a scheduled "scaffold next week" that pre-adds an empty week entry when a new
week boundary passes). The **eras, milestones and blurbs still need human judgment**, so a
fully-automatic "add a week" can't write good chapters on its own — the realistic
automation is *keep the derived data fresh + scaffold the empty week for a human to fill*.
Not wired up yet; see the conversation that built this.
