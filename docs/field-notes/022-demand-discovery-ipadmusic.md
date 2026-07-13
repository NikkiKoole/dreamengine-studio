# 022 — Demand discovery: r/ipadmusic wants a vibe, not a feature

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-13

**Confidence**
Medium — thin sample (Reddit throttled the run to ~97 threads across 3 wish-probe feeds;
a fuller run from a fresh IP may shift the weights). Directional, not final.

---

## Observation

The first run of the new demand-discovery tool (`tools/reddit-gaps.js`, method:
[demand-discovery.md](../design/demand-discovery.md)) mined **57 "unmet-ask" threads** from
r/ipadmusic. The demand splits cleanly along **our grain**:

- **The LOUDEST demand is off our grain.** MIDI control/routing (25 threads), stem separation,
  time-stretch / tempo-sync, DAW features, DJ mixing, control surfaces. The tribe is
  hardware-and-workflow-centric; they want *pro utilities* a humble lo-fi toy shouldn't chase.
- **The on-grain, RECURRING asks are about a vibe, not a feature** — three clusters worth building
  toward:
  1. **A chord-play toy** — "store chords and play them like a drum pad", "chord progression apps",
     "analyze chords", auto-accompaniment. Recurs ~5×. Dead-centre for us.
  2. **A noodle playground** — literally *"an app where i can just fuck around… switch from
     instrument to instrument"* + the "1-minute groovebox". This is the DreamEngine soul, and it
     **validates the Tiny Jam multi-rack direction** ([008 — identity](008-the-identity-of-dreamengine.md)).
  3. **Classic-gear homages** — ARP Odyssey, gritty rompler, Beatstep, Digitakt-like, Launchpad.
     The instrument-cart homage lane we already run (acidrack = TB-303) has a waiting audience.

The one-line takeaway: **the feature gaps aren't for us; the vibe gap — playful, simple, "just
noodle" — is real, repeated, and exactly our lane.**

---

## Why this matters

This is the first tribe-mined input to **lever #1** of
[demand-generation.md](../design/demand-generation.md) — *what to build* — rather than a guess.
It also reframes the word "gap": for a niche lo-fi console the opening isn't a missing *feature*
(the tribe has GarageBand/Cubasis/AUM for those) but a missing *feeling*. That's a thesis the
gallery can be steered by, and re-tested each time the tool is run on a new tribe or date.

---

## Evidence

The tool run (`node tools/reddit-gaps.js ipadmusic`) + the actual threads. **Backlinks to read the
signal yourself**, grouped by cluster:

**Chord-play toy (on-grain):**
- [store chords and play them like a drum pad](https://www.reddit.com/r/ipadmusic/comments/5otv9p/looking_for_an_app_that_can_store_chords_and_be/)
- [Any Good Chord Progression Apps?](https://www.reddit.com/r/ipadmusic/comments/yc5zup/any_good_chord_progression_apps/)
- [Compact app to analyze chords](https://www.reddit.com/r/ipadmusic/comments/1i5tflg/compact_app_to_analyze_chords/)

**Noodle playground (on-grain — the soul):**
- [*"an app where i can just fuck around… switch from instrument to instrument"*](https://www.reddit.com/r/ipadmusic/comments/67sgvn/is_there_an_app_or_method_where_i_can_just_fuck/)
- [Battlestation: the 1-minute groovebox](https://www.reddit.com/r/ipadmusic/comments/1pj3xvt/battlestation_the_1minute_groovebox_promocodes/)
- [App similar to Cycles?](https://www.reddit.com/r/ipadmusic/comments/1d8b9va/app_similar_to_cycles/)

**Classic-gear homages (on-grain — the instrument-cart lane):**
- [like the Arturia Beatstep Pro?](https://www.reddit.com/r/ipadmusic/comments/1nemcy9/is_there_an_app_i_could_use_like_the_arturia/)
- [ARP Odyssey — anything similar?](https://www.reddit.com/r/ipadmusic/comments/1tbqk1y/arp_odyssey_anything_similar/)
- [gritty old-school rompler sounds?](https://www.reddit.com/r/ipadmusic/comments/isrynx/is_there_an_app_with_gritty_old_school_rompler/)
- [anything like a Digitakt?](https://www.reddit.com/r/ipadmusic/comments/xo8mle/any_ios_apps_similar_to_a_digitakt/)
- [behaves like Novation's Launchpad w/ MIDI sync?](https://www.reddit.com/r/ipadmusic/comments/2sh79e/is_there_an_app_that_behaves_like_novations/)

**Loud but OFF-grain (why the tool reports "no clean gap" — pro utility, not toy):**
- [loop librarian & arranger](https://www.reddit.com/r/ipadmusic/comments/1kk4xw5/is_there_an_ipad_app_that_can_used_as_a_loop/) ·
  [Auv3 stem separation](https://www.reddit.com/r/ipadmusic/comments/1b4ao0z/auv3_stem_separation/) ·
  [BPM detector](https://www.reddit.com/r/ipadmusic/comments/1euufis/is_there_an_app_that_can_determine_the_bpm_of/) ·
  [MIDI transpose](https://www.reddit.com/r/ipadmusic/comments/zbm6mr/is_there_an_app_to_transpose_incoming_midi/) ·
  [time-stretch in sync](https://www.reddit.com/r/ipadmusic/comments/3zc944/is_there_an_app_that_will_time_stretch_sample/) ·
  [MIDI controller](https://www.reddit.com/r/ipadmusic/comments/1b1gksx/best_ipad_app_as_a_midi_controller/)

> Raw signal is regenerable and NOT retained in the repo (Reddit policy) — it lives only in the
> gitignored `tools/reddit-gaps-cache/`. This note keeps the *interpretation* + citations; re-run
> the tool to refresh the underlying threads.

---

## Implications

- **Candidate carts** (not committed — a shortlist to weigh): a chord-pad/progression play toy; a
  "one-screen noodle" that leans harder into instrument-switching; more classic-synth/drum-machine
  homages in the acidrack mould.
- **Tool limitation confirmed:** with 468 carts the binary "★ GAP" verdict (coverage == 0) almost
  never fires — nearly every topic has *some* matching cart. The value is this ranked+cited wish
  list, not the automated flag. Sharper signals (demand-to-coverage ratio, sub-topic specificity)
  are noted in the design doc's Open section.

---

## Open questions

- Does a **fuller, un-throttled run** (all 13 feeds) change the cluster weights or surface a fourth
  on-grain cluster the thin sample missed?
- Do **sibling subs** (r/synthesizers, r/edmproduction, r/AudioProductionDeals) share the vibe gap,
  or is "just noodle" specific to the iPad-music tribe?
- Is the vibe gap best served by **a new cart** or by **sharpening how existing carts are framed**
  on the store (a listing/positioning fix, not a build)?

---

## Related notes

- [008 — the identity of DreamEngine](008-the-identity-of-dreamengine.md)
- [009 — the negative space of DreamEngine](009-the-negative-space-of-dreamengine.md)
- [014 — outside-agent brainstorms as a knowledge source](014-outside-agent-brainstorms-as-a-knowledge-source.md)
