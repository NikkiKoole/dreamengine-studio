# 023 — Demand discovery: r/synthesizers confirms the "cheap playful toy + homage" thesis

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-13

**Confidence**
Medium — a fuller (unthrottled) run got all 8 wish-probe feeds for this sub (120 threads, 109
wishes), so the sample is better than [022](022-demand-discovery-ipadmusic.md); still one sub, one day.

---

## Observation

Second tribe mined with `tools/reddit-gaps.js` (method:
[demand-discovery.md](../design/demand-discovery.md)). r/synthesizers is a **hardware** tribe — the
loudest demand is *hardware utility* (MIDI routing 37, hardware-companion 24, "app that tells the
notes being played"), which is off our grain, same shape as [022](022-demand-discovery-ipadmusic.md).

But the **on-grain asks rhyme with r/ipadmusic's**, and one is almost a spec for what Tiny Jam
already is:

> *"Is there a cheap alternative to the OP-1 for someone who knows nothing?"*

Three on-grain clusters, all echoing 022:

1. **Cheap, playful, beginner lo-fi toy** — the OP-1-for-a-beginner ask, plus "how do I get started
   in the synth hobby", "an app or software to start", "stupid simple app". Pairs directly with
   022's "just fuck around, switch instruments". **This is the sharpest cross-tribe signal.**
2. **Classic-gear emulation / homage** — "iOS synth like the ASR-10", "like the Gecho Loopsynth",
   "synth that can do karplus", "iOS emulations?". Confirms 022's homage lane (ARP Odyssey / Beatstep
   / Digitakt) — and note we already do karplus (`waveguide-synth`) and the TB-303 (acidrack).
3. **Learn-by-recreating** — "I wish there was a standard list of patches to work through
   (recreate)", "how do I start learning music for video-game composition". A learn-synthesis-by-
   doing angle that fits the learn-to-code *lineage* (ADR-0022) — carts as playable patch lessons.

---

## Why this matters

Two independent tribes now point at the **same** on-grain opportunity: a **cheap, playful, beginner-
friendly lo-fi instrument toy, dressed as classic gear**. That is not a new direction — it is what
Tiny Jam already is — so the value here is *conviction + positioning*: the tribe's own words
("cheap alternative to the OP-1 for someone who knows nothing") are ready-made store/press copy, and
the recurring hardware names (ASR-10, Gecho, OP-1, ARP, Digitakt) are a **homage backlog** with a
proven audience. The off-grain half (MIDI/hardware utility) is equally useful: it tells us what NOT
to chase.

---

## Evidence

`node tools/reddit-gaps.js synthesizers` (120 threads, 109 wishes). Representative threads:

**Cheap / playful / beginner (on-grain — the thesis):**
- [cheap alternative to the OP-1 for someone who knows nothing](https://www.reddit.com/r/synthesizers/comments/1cnnk4y/is_there_a_cheap_alternative_to_the_op1_for/)
- [how do I get started in the analogue synth hobby?](https://www.reddit.com/r/synthesizers/comments/1ttgcbk/how_do_i_get_started_in_the_analogue_synthesizer/)
- [looking for an app or software to start](https://www.reddit.com/r/synthesizers/comments/fr330y/looking_for_an_app_or_software_to_start/)

**Classic-gear emulation (on-grain — homage lane):**
- [iOS synth similar to ASR-10](https://www.reddit.com/r/synthesizers/comments/o3ljkq/ios_synth_similar_to_asr10/)
- [an app like the Gecho Loopsynth](https://www.reddit.com/r/synthesizers/comments/1gdzgd4/looking_for_an_app_like_the_gecho_loopsynth/)
- [synth that can do karplus like this](https://www.reddit.com/r/synthesizers/comments/1kishcr/synth_that_can_do_karplus_like_this/)
- [iOS emulations/apps?](https://www.reddit.com/r/synthesizers/comments/fx9d8f/ios_emulationsapps/)

**Learn-by-recreating (on-grain — the lineage angle):**
- [wish there was a standard list of patches to work through (recreate)](https://www.reddit.com/r/synthesizers/comments/4gfkjm/i_kinda_wish_there_was_a_standard_list_of_patches/)
- [8+ knobs and nothing else](https://www.reddit.com/r/synthesizers/comments/2ehvj9/are_there_any_controllers_that_just_have_like_8/) ·
  [learning music for video-game composition](https://www.reddit.com/r/synthesizers/comments/1u3gtjr/how_do_i_start_learning_music_for_video_game/)

> Raw threads are regenerable and not committed (Reddit policy) — cache lives in the gitignored
> `tools/reddit-gaps-cache/`.

---

## Cross-tribe synthesis (022 + 023)

| | r/ipadmusic (022) | r/synthesizers (023) |
|---|---|---|
| **Loudest demand** | MIDI control, stem sep, time-stretch (off-grain utility) | MIDI routing, hardware companion (off-grain utility) |
| **On-grain #1** | "just fuck around, switch instruments" | "cheap OP-1 for a beginner" |
| **On-grain #2** | homages: ARP, Beatstep, Digitakt, rompler | homages: ASR-10, Gecho, karplus, OP-1 |
| **On-grain #3** | chord-play toy | learn-by-recreating patches |

**Convergent thesis:** the opening for a lo-fi console isn't a *feature* (both tribes have pro apps
for those) — it's a **cheap, playful, beginner lo-fi instrument toy wearing classic-gear clothes.**
Two tribes, same answer. It validates Tiny Jam and hands us both the copy and a homage backlog.

---

## Open questions

- A **third data point** — r/iosmusic and r/pocketoperators were throttled out this session (shared
  IP burned). Do they confirm or complicate the thesis? (Fetch when the IP budget resets.)
- The **game/procgen tribes** (r/pico8, r/proceduralgeneration) need a *game-tuned* wish-pattern +
  topic taxonomy before the tool says anything useful there — the current config is music-tuned.
- Is the win a **new cart, a homage backlog, or a positioning/copy change**? (Same open question as 022.)

---

## Related notes

- [022 — demand discovery: r/ipadmusic wants a vibe, not a feature](022-demand-discovery-ipadmusic.md)
- [008 — the identity of DreamEngine](008-the-identity-of-dreamengine.md)
- [014 — outside-agent brainstorms as a knowledge source](014-outside-agent-brainstorms-as-a-knowledge-source.md)
