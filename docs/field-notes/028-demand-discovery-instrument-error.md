# 028 — Ten quiet rotations: when "well-served" was partly instrument error

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-31

**Confidence**
High on the two tool defects (both reproduced, both now pinned by known-answer fixtures that fail
when reverted). Medium on the supply-side read: 16 launches in a 30-day window across 24 caches is a
real count, but Reddit RSS still carries no upvote scores, so "this landed" remains unmeasurable.

---

## Observation

The drip had run unattended for a few days. Ten consecutive rotations
([r/synthrecipes](https://www.reddit.com/r/synthrecipes/) through
[r/teenageengineering](https://www.reddit.com/r/teenageengineering/), ~4,000 cached threads) each
printed the same last line:

> `no clean GAP this run — widen with --queries or --refresh, or the tribe is well-served on-grain.`

Note [025](025-demand-discovery-supply-side-showcase.md) taught us to read that as a *finding*, not a
null: the tribe is well-served because indie builders ship our grain constantly. That reframe is
still right. But this run it was also **partly instrument error** — three separate defects, each of
which made the table quieter than the data.

**1. Fake coverage.** `coverage()` matches a topic's keys against our own cart prose, and six keys
mean something different on each side of the comparison. `score` matched *game* high-scores (flappy,
hotline, catch-the-star); ` tab ` matched *UI* tabs (acidwire, acidfit, Chordblossom's "three tabs");
` deck` matched a *card* deck (Blackjack, Deckbuilder). Between them, those three words alone put
**40 carts under "Notation / sheet music"** — a topic where we honestly have 2. Same shape for
` pad ` (an xy/drum pad control surface, not a sustained synth pad), `convert` ("converts like
chipjam"), and `external` ("external vs finger" trig). The demand column was never affected: on
Reddit the surrounding sentence disambiguates. Only `ours` inflated, and inflated `ours` is exactly
what suppresses a gap. Fixed by a `COVER_BLIND` list, each entry hand-verified against the real
shelf; the keys deliberately *not* listed (`patch`, `analog`, ` pattern`, `transpose`, `hardware`,
`routing`, `export `, `scale`, `random`, `endless`) were checked the same way and are genuinely ours.

**2. The gap test could no longer fire.** `gap` was binary — `coverage === 0`. The musicish shelf is
now **255 carts**, so no core topic is ever 0, so `GAP` was structurally unreachable and every row
fell through to `covered (hot)` regardless of how loud the ask was. The sharpest example:
r/ipadmusic showed **demand 74 on MIDI routing against 23 carts** and printed "covered". Added a
`THIN` verdict (`◐`) for covered-on-paper-but-swamped, at `demand ≥ 3 × coverage`. It is
dimensionally loose on purpose — threads and carts are different units — and it flags a row for a
human to read rather than concluding anything.

**3. A third blind spot: brand-new launches.** Notes 022–024 mine what a tribe **asks for**; 025
added the showcase pass for what it **upvoted**. Neither can see *what shipped last week*. A new
launch sits in `new` only, so it scores `pop 0` and the showcase pass drops it; and it is an
announcement, not an ask, so no wish pattern fires. **16 on-grain launches from the last 30 days were
sitting uncounted in caches we already had.** Added `--launches` (date-windowed, on-grain only,
flags `[OSS]`/`[FREE]`/`[BETA]`, collapses cross-posts), and wired a 14-day window into the drip log
itself — supply news rots in days, and nobody re-reads a cache by hand.

**4. One rotation slot had been dead for 2.5 weeks.** `r/Tic_80` does not exist. Reddit 302s an
unknown name to its subreddit-*search*, so all 13 feeds "failed" every turn and the slot cached
nothing. It stayed invisible because `--drip` always saves a snapshot — deliberately, so a *throttled*
sub can't stay "stalest" forever and starve the rotation — which advanced the mtime and moved the
round-robin along. The live sub is **r/TIC80**. Renamed, and drip now shouts on the distinguishing
shape: 0 feeds **and** 0 threads, ever.

---

## Why this matters

A linter that judges is only as trustworthy as its own fixture — the lesson `status-check.js` and
`lint-aux-params.js` already carry, now paid for a second time. `reddit-gaps.js` printed a
confident, well-formatted, **green** verdict table for ten rotations while three of its signals were
degraded. Nothing looked broken; a saturated table and a well-served tribe render identically.

The specific trap is worth naming: **two of these defects only ever produce false NEGATIVES.**
Inflated coverage and an unreachable `GAP` can never invent a finding, they can only hide one — so
the tool got quieter and quieter and read as increasingly reassuring. That is the failure mode to
fear in a demand tool, because the reassurance is self-sealing: no finding means no reason to look.

The subs file already warned us (*"Read RAW wishes, not the verdict table"*), and reading raw is what
surfaced all of this. The warning was right, which is itself the evidence that the table had stopped
carrying information.

---

## Evidence

Reproduced, then pinned. `node tools/reddit-gaps.js --check` grew from 10 assertions to 33 (and is now
gated in `repo-doctor.js`, which it wasn't before — the reason this could rot unnoticed), and each
new one was **mutation-tested** — the fix reverted, the check confirmed failing — because three of
them assert a *negative* (coverage should be 0) and would otherwise pass vacuously:

| mutation | caught by |
|---|---|
| `coverage()` stops filtering `COVER_BLIND` | notation coverage 0 → 1, dj 0 → 1 |
| `thin` hardcoded false | sampler `THIN` → `covered (hot)` |
| launch window ignored | a Feb launch leaks into a 30-day view |
| launch off-grain filter dropped | a notation app counts as a competitor |
| `LAUNCH_NOT` dropped | a shipped *album* counts as a competitor |
| `LAUNCH_NOT` reads the body too | a real tool whose body mentions an album is lost |

Plus a positive control (the blind words really are present in the fixture cart text, so the
negative assertions can't pass by accident) and a staleness guard (every `COVER_BLIND` key must
still be used by some topic, or it silences nothing).

Effect of the coverage fix on the live shelf: Notation **40 → 2**, Synths 155 → 141, Hardware 18 →
14, DJ 12 → 10, Utilities 27 → 26. First `THIN` row in the repo's history: r/ipadmusic MIDI routing,
74 vs 23.

### The supply side, last 30 days (`--launches`)

The two that matter most:

- **[A free, open-source iPad app for exploring chords, intervals and scales with MIDI](https://www.reddit.com/r/ipadmusic/comments/1v4e15q/i_built_a_free_opensource_ipad_app_for_exploring/)**
  (2026-07-23, `[OSS][FREE]`) — real-time chord/interval detection off a MIDI keyboard, generated
  chords/inversions, scales, fingering, circle of fifths, metronome. No account, no subscription.
  This is note [024](024-demand-discovery-four-tribes.md)'s **"Candidate cart — chord toy
  (best-evidenced next build)"**, shipped by someone else, free, with a broader feature list than the
  candidate described.
- **[A $30 groovebox that samples itself — 3 acid synths + 808/909, runs on a pocket computer](https://www.reddit.com/r/synthesizers/comments/1uvcgre/i_built_a_30_groovebox_that_samples_itself_3_acid/)**
  (2026-07-13, `[OSS]`) — nearly a description of `acidcandy` + `Tiny Acid Jam`, on hardware, open
  source. The 025 finding (validated *and* crowded) restated in one post.

Also in the window: [GCS Model 8 Tape DAW](https://www.reddit.com/r/iosmusicproduction/comments/1uzvotp/i_just_released_gcs_model_8_tape_daw_for_iphone/)
(free, zero IAP, custom tape emulator with selectable cassette/¼"/½"/1" formats — the lo-fi tape
thesis at price zero), [RUN4 – Tape & Texture Machine](https://www.reddit.com/r/ipadmusic/comments/1uzcx5u/run4_tape_texture_machine_looking_for_testflight_beta/)
(iPad-first, pitched explicitly at the TORSO S-4 / Chase Bliss / monome norns crowd),
[Microtone](https://www.reddit.com/r/chiptunes/comments/1v0wj5l/introducing_microtone_an_online_music_tracker/)
(online tracker), a [free random YouTube sampler](https://www.reddit.com/r/sampling/comments/1uzlu92/i_made_a_free_random_youtube_sampler_and_its/),
and a [browser-based experimental instrument](https://www.reddit.com/r/sounddesign/comments/1v2db35/i_built_a_browserbased_experimental_instrument/).

Precision note: the first `--launches` run returned 19, of which 3 were *music* releases ("I made an
album…", "Just released this chiptune…", "a Techno performance patch"). These tribes announce music
with the same grammar they announce tools. `LAUNCH_NOT` filters those on the **title only**, because
a genuine tool post often mentions the album the author made with it.

---

## Implications

- **Re-read [024](024-demand-discovery-four-tribes.md)'s chord-toy candidate before building it.**
  The demand reading wasn't wrong; the window acquired a free open-source occupant. Our differentiator
  would have to be the *feeling* (note 022's reframe — playful, humble, a toy not a utility), not the
  feature list, because the feature list is now free.
- **`THIN` is where the next candidate comes from**, not `GAP`. On a 255-cart shelf, "we have nothing
  here" is nearly extinct as a signal; "the ask dwarfs what we have" is the live one.
- **The drip now reports supply, not just demand.** A launch in our lane is time-critical in a way the
  demand table isn't: by the next rotation (5 days) it's already old news.
- **Copy the mutation-test discipline to the other advisory linters.** A negative assertion that was
  never seen to fail is not a test. This is the third time the repo has paid for that.

---

## Open questions

- **Is `THIN_RATIO = 3` right?** It was chosen to make the r/ipadmusic MIDI row (74 vs 23) fire and
  to leave comfortably-covered rows alone. One data point is not a calibration. Watch which rows it
  lights over the next full cycle before trusting the threshold.
- **`ours` still counts carts, not usable products.** A cart on the shelf is not an app a stranger
  can play; the count conflates a mockup, a tech demo, and a shipped rack. That is the deeper version
  of defect 1 and it is *not* fixed — `COVER_BLIND` only removed the words that were plainly wrong.
- **How much of the 30-day launch window is us not being on the platforms these people ship to?**
  Every launch above is iOS/iPad or browser. Worth reading against
  [demand-generation.md](../design/demand-generation.md) lever #2.
- **r/TIC80's first real cache lands this rotation.** The open probe from
  [027](027-demand-discovery-sounddesign-matchpatch.md)/the subs file still stands: for the
  fantasy-console tribes, is audio *depth* a pull, or is the 4-channel limit a beloved constraint?

## Related notes

- [025 — Demand discovery, the supply side: the thesis is validated AND crowded](025-demand-discovery-supply-side-showcase.md)
- [024 — Demand discovery: four tribes](024-demand-discovery-four-tribes.md)
- [022 — Demand discovery: r/ipadmusic](022-demand-discovery-ipadmusic.md)
- [021 — Status labels cannot be linted](021-status-labels-cannot-be-linted.md)
