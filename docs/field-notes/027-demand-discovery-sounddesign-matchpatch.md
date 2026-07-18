# 027 · Demand discovery — r/sounddesign (+ the r/audioprogramming prune) → the "match-the-patch" candidate

*2026-07-18. Method: [`design/demand-discovery.md`](../design/demand-discovery.md). Two tribes added
to the drip this run; one kept, one probed-and-pruned. The keeper surfaces a build candidate that is
NOT the same as note [026](026-demand-discovery-cross-tribe.md)'s #1.*

## What was run

Two subs added to `tools/reddit-gaps-subs.txt`:

- **r/sounddesign** — kept. Full fetch: 13 feeds · **114 threads · 50 wishes**.
- **r/audioprogramming** — probed one rotation (47 threads · 9 wishes), then **pruned**. It's a
  maker/DSP-dev + career tribe, not consumer-demand: every wish read *"how do I become an audio
  programmer / career advice / feedback on my JUCE plugin / help with my autotune VST,"* and **all
  nine "is there an app…" queries returned 0**. No build-this gap will ever surface there. Better as
  a **leads.js venue** (show the *engine* to fellow makers) than a demand-discovery sub. Left off the
  rotation; the subs.txt comment records the verdict.

## The finding: "recreate a sound" is a distinct axis from note 026's ear-training

r/sounddesign has no *uncovered* topic in the verdict table (our shelf covers synths/FX/samplers
heavily), but the tribe runs on one wish, stated almost verbatim over and over — **recreate a
specific sound you've heard**:

- [This reverse bass is magical and i want to recreate it, but how?](https://www.reddit.com/r/sounddesign/comments/1m5wdfg/this_reverse_bass_is_magical_and_i_want_to/)
- [How do i recreate this synth sound?](https://www.reddit.com/r/sounddesign/comments/1tz9bvn/how_do_i_recreate_this_synth_sound/)
- [How do I make this bass in Serum?](https://www.reddit.com/r/sounddesign/comments/1rbgz23/how_do_i_make_this_bass_in_serum/)
- [How do I recreate this specific effect in this song?](https://www.reddit.com/r/sounddesign/comments/1uqaseq/how_do_i_recreate_this_specific_effect_in_this/)
- [how do i emulate this effect?](https://www.reddit.com/r/sounddesign/comments/1t2terq/how_do_i_get_my_808_to_sound_distorted_and/) · [808 distorted & saturated this clean?](https://www.reddit.com/r/sounddesign/comments/1t2terq/how_do_i_get_my_808_to_sound_distorted_and/)

**Why this is not note 026 #1.** Note 026's "ear-training game" is a **pitch** axis — *what note /
chord / interval is this*. "Recreate a sound" is a **timbre** axis — *what patch / knob-settings make
this*. They're siblings (both "learn by ear," both zero-coverage as a *game*), but a chord-ID toy
does nothing for someone chasing a reverse bass. This is a separate candidate.

## The candidate: "match the mystery patch" (a sound-design ear-training *game*)

The recreate-a-sound wish, turned into play — the honest fantasy-console move (a *feeling*, not a
utility, per note 022):

- **Easy version — no new engine work.** The cart plays a randomized synth/FX patch (osc + filter +
  a couple of effects, all params we already own). You twist your own knobs to match by ear; it
  scores you on **parameter distance** and reveals the target. Pure synthesis — no mic, no analysis.
  Teaches sound design *by ear*, which is exactly what the tribe is asking for, and it's a *game*.
  Zero shelf coverage (topic-brief "recreate/match/reference patch" → 0 carts).
- **Hard version — frontier.** Bring a *real* reference (mic or clip) and the cart guides you toward
  it. Needs live **timbre** analysis (spectral centroid / formants — we have `harmonic-spec.js` +
  `formant-check.js` as *offline* oracles, not live). This joins note 026's **frontier pile** (all
  blocked on live audio-in via [`mic-and-sampling.md`](../design/mic-and-sampling.md) /
  [`mic-spike`](../../tools/mic-spike/)) — landing that capability converts this too.

## Where it fits the 026 backlog

Slot it alongside note 026's #1 as a **second ear-training game on the timbre axis** — same
zero-coverage / strong-demand / genuine-game profile, different sense being trained. Both are the
"build a *game*, not another utility" honest fit. Outreach venue is the source: **r/sounddesign**
(and r/synthesizers, r/edmproduction) — `tools/leads.js draft <cart>` once built.

Watch-item carried forward: **SFX / game sound** on r/sounddesign showed demand 3 vs our coverage 1
(lone `sfx generator`) — the thinnest coverage-vs-demand ratio on the board. Not a gap yet; revisit
as the cache fattens.

## Evidence

`node tools/reddit-gaps.js sounddesign` / `audioprogramming` (caches 2026-07-18). Raw wishes via
`--raw`. Coverage check: `node tools/topic-brief.js "recreate sound match reference patch"` → 0 carts.
