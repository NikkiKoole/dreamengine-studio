# 025 — Demand discovery, the supply side: the thesis is validated AND crowded

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-17

**Confidence**
Medium — one platform (Reddit RSS), a coarse taxonomy, and a *first* full sweep; but 22 caches and
708 celebrated on-grain posts converge hard, and the finding is corroborated by the demand side
already recorded in [022](022-demand-discovery-ipadmusic.md)–[024](024-demand-discovery-four-tribes.md).

---

## Observation

`reddit-gaps.js` (022–024) mined tribes for what they **ask for**. It has a blind spot: the
wish-miner scores a thread only if it matches an *ask* pattern ("is there an app…", "wish there
was…"), so **build announcements score 0 and drop out** — the tool literally could not see "someone
made this."

We found it the hard way. The maker spotted r/pico8's
[**P8-38P — a 303, 808 and poly-synth groovebox**](https://www.reddit.com/r/pico8/comments/1uvss9d/p838p_a_303_808_and_polysynth_groovebox/)
— dead on-grain (basically `acidcandy`, on the rival console), upvoted into `top-month`+`hot` — and
asked why it never surfaced. It was in the cache the whole time; the miner was blind to it.

The fix is a **supply-side twin** of the wish-miner: `reddit-gaps.js --showcase`
([demand-discovery.md → "the showcase pass"](../design/demand-discovery.md#the-blind-spot--the-showcase-pass-added-2026-07-17)).
RSS carries no upvote count, but **presence in a popularity listing feed** (`top-year`/`top-month`/
`hot`) *is* the upvote proxy — it re-reads the `feeds` array every cache already records, keeps posts
that also match an on-grain topic, and ranks them (`[show]` = a build, `[ask]` = a question that
trended). No fetch.

The first full sweep — **22 caches, 708 celebrated on-grain posts** — reframes the single most
repeated verdict of notes 022–024. Every gap report printed *"no clean GAP — the tribe is well-
served."* The supply side shows **why the tribe is well-served: indie builders ship exactly-our-grain
lo-fi music toys constantly, and the crowd rewards them.** "Well-served" was never a null result —
it was a *finding we were reading from the wrong side.*

---

## Why this matters

Notes 022–024 gave the thesis (a *cheap, playful, beginner lo-fi instrument toy in classic-gear
clothes*) **conviction** and **durability** across nine tribes. This note supplies the missing half
of the picture and changes what the thesis *implies*:

- **The category is proven, not hypothetical.** People don't just ask for these toys — they build
  them and the toys **land** (top-year/top-month). Demand + supply + reward all present. Stop
  treating a gap report's "no GAP" as discouraging; it is the sound of a *healthy, wanted* category.
- **The category is crowded.** r/musictheory is literally debating
  [**"Should we just outright ban 'I built an App' posts?"**](https://www.reddit.com/r/musictheory/comments/1up2ebx/should_we_just_outright_ban_i_built_an_app_posts/)
  (top-year+top-month) — the surest sign a niche is saturated is the community policing the flood.
- **So the moat is not the toy category — it's differentiation and distribution.** The edge is the
  [identity](008-the-identity-of-dreamengine.md) 022/024 already pointed at (the *honest-C
  fantasy-console / collaboration* angle, not "another web music toy"), and the go-to-market is the
  one every landed showcase already demonstrates: **a free web/iOS toy dropped straight into the
  tribe** (the [demand-generation.md](../design/demand-generation.md) lever, now evidenced by 708
  competitors' behaviour, not asserted).

---

## Evidence

Run: `node tools/reddit-gaps.js --showcase` (bare = across every cache). Representative `[show]`
posts the tribe upvoted, all dead on-grain for a lo-fi music toy:

**Direct competition for our own concepts:**
- [I built a **$30 groovebox that samples itself — 3 acid synths + 808/909**, runs on a pocket computer](https://www.reddit.com/r/synthesizers/comments/1uvcgre/i_built_a_30_groovebox_that_samples_itself_3_acid/) — r/synthesizers, top-month+hot. This *is* `acidcandy`'s pitch, and it landed. Study it.
- [**P8-38P — a 303, 808 and poly-synth groovebox**](https://www.reddit.com/r/pico8/comments/1uvss9d/p838p_a_303_808_and_polysynth_groovebox/) — r/pico8, top-month+hot. The origin of this note; the same concept on the rival fantasy console.

**The exact "cheap-playful-gear" thesis, built by others:**
- [Always wanted an **OP-1**, never got one, so I built my own **tape 4-track for the phone**](https://www.reddit.com/r/iosmusicproduction/comments/1tyqehf/always_wanted_an_op1_never_got_one_so_i_built_my/) — r/iosmusicproduction, top-year. Word-for-word 023's OP-1 thesis.
- [I made a **free chiptune tracker you can use in the browser**](https://www.reddit.com/r/chiptunes/comments/1u6d009/i_made_a_free_chiptune_tracker_you_can_use_in_the/) — r/chiptunes, top-month.
- [I made a **game boy sequencer**](https://www.reddit.com/r/chiptunes/comments/1u8k73c/i_made_a_game_boy_sequencer/) — r/chiptunes, top-month.

**Music toys shaped as GAMES (dead-centre our grain — a playable instrument):**
- [I made a **game where you can practice matching synth sounds**](https://www.reddit.com/r/synthrecipes/comments/1sizwwk/i_made_a_game_where_you_can_practice_matching/) — r/synthrecipes, top-year.
- [**Sampledle** — a daily Wordle-style game where you guess hits from their original samples](https://www.reddit.com/r/sampling/comments/1t7hfs0/sampledle_a_daily_wordlestyle_game_where_you/) — r/Sampling, top-year.
- [I built a **motion-controlled drum app**](https://www.reddit.com/r/iosmusicproduction/comments/1rxe8vt/i_built_a_motioncontrolled_drum_app/) — r/iosmusicproduction, top-year.
- [I made an **iOS 8-bit music sketchpad** with AUv3 and MIDI export](https://www.reddit.com/r/iosmusicproduction/comments/1uf9kp3/i_made_an_ios_8bit_music_sketchpad_with_auv3_and/) — r/iosmusicproduction, top-month.

**Corroborates 024's chord-toy candidate (built by others, upvoted):**
- [Made a **free tool for when you have a chord or two and no idea where to go next**](https://www.reddit.com/r/ipadmusic/comments/1ttxe7n/made_a_free_tool_for_when_you_have_a_chord_or_two/) — r/ipadmusic, top-year. This is 024's best-evidenced next build — and someone already shipped a version that landed.
- [I made a **tuner that looks for the scale instead of one note**](https://www.reddit.com/r/ipadmusic/comments/1un90r0/i_made_a_tuner_that_looks_for_the_scale_instead/) — r/ipadmusic, top-year+top-month.
- [I made this little **paper synthesizer** for my boyfriend](https://www.reddit.com/r/synthesizers/comments/1o2bsyn/i_made_this_little_paper_synthesizer_for_my/) — r/synthesizers, top-year (the *playful, gift-shaped* end of the grain).

> Raw threads are regenerable and NOT committed (Reddit policy) — the caches live only in the
> gitignored `tools/reddit-gaps-cache/`. This note keeps the *interpretation* + citations; re-run
> `--showcase` to refresh the underlying posts.

---

## Implications

- **Reframe every prior "no GAP" verdict.** 022–024's recurring "tribe well-served" is not a dead
  end — it is confirmation of a proven, wanted, *crowded* category. Read gap reports for the
  *boundary* signals (vocal-overdub, hum-to-MIDI — 024) and the showcase for *what lands*.
- **`--showcase` is now the second lens, run alongside the gap report.** The gap report answers "what
  do they ask for that's missing"; the showcase answers "what do they build that wins." Together they
  are the demand-discovery pair. Documented + self-test-gated in
  [demand-discovery.md](../design/demand-discovery.md).
- **Competitive intelligence, not just demand.** The `$30 groovebox` and the pico8 groovebox are
  direct competitors to `acidcandy`. The next groovebox move should be made *knowing what already
  landed* — a manual study of a handful of top `[show]` posts (what they do, how they were presented,
  what the comments asked for) is a cheap, high-value follow-up.
- **Distribution is settled by example.** Nearly every landed showcase is a **free web or iOS toy
  posted directly to its tribe** — the exact play [demand-generation.md](../design/demand-generation.md)
  and [leads.js](../design/leads-marketeer.md) describe. We now have 708 worked examples of the GTM,
  not a hypothesis.
- **Differentiation is the real work.** In a crowded category, "a lo-fi music toy" is not a wedge.
  The wedge is the [identity](008-the-identity-of-dreamengine.md): honest simulation, a real
  fantasy-console you write C for, the collaboration story — the things a web-toy-of-the-week can't
  copy. This note is the empirical case for spending effort *there*, not on the Nth groovebox.

---

## Open questions

- **Does "landed" (popularity-feed presence) actually track success?** With no upvote count in RSS,
  top-feed membership is a proxy. It's directionally sound (those feeds *are* the vote-ranked ones)
  but can't distinguish a 5k-upvote hit from a modest one. A manual spot-check of a few would
  calibrate it.
- **Crowded → pick a quieter on-grain niche, or lean harder into differentiation in a loud one?**
  Two valid strategies; the data says the category wins either way, but doesn't choose for us.
- **What makes the winners win?** The titles say *what* was built; the comments say *why it resonated*.
  That reading (a judgment step, not automatable) is the natural next demand-discovery task — and
  the input to any acidcandy competitive pass.
- **Taxonomy coarseness.** Single-keyword matching tags gear-jam videos as "Synths." A tighter
  build-vs-chatter classifier (positive "I made / built / released" signal) would sharpen `--showcase`
  — but risks re-importing the wish-pattern brittleness it was built to avoid.

---

## Related notes

- [022 — demand discovery: r/ipadmusic wants a vibe, not a feature](022-demand-discovery-ipadmusic.md)
- [023 — demand discovery: r/synthesizers confirms the thesis](023-demand-discovery-synthesizers.md)
- [024 — demand discovery: four more tribes + chop-to-pads + the vocal-overdub boundary](024-demand-discovery-four-tribes.md)
- [008 — the identity of DreamEngine](008-the-identity-of-dreamengine.md) (the differentiation this note argues for)
- [009 — the negative space of DreamEngine](009-the-negative-space-of-dreamengine.md)
- [demand-discovery.md — the tool + the showcase pass](../design/demand-discovery.md)
- [demand-generation.md — the free-toy-in-the-tribe distribution these showcases all use](../design/demand-generation.md)
