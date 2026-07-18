# 026 — Demand discovery: what 1,411 wishes across 24 tribes actually ask for

> This note captures a discovery made during the evolution of DreamEngine.
>
> It records our understanding at the time it was written.
> Later notes may refine, extend or replace it.

**Status**
Observed

**Date**
2026-07-18

**Confidence**
Medium-high — the largest run yet (24 cached subs, 1,411 unique wish-tagged posts), cross-checked
against Google-autocomplete demand and our own shelf. Directional: Reddit RSS has no upvote scores,
so demand = cluster size (many distinct threads asking the same thing), not weighted popularity.

---

## Observation

The [drip](../design/demand-discovery.md#continuous-fetching--the-drip-set-up-2026-07-13) has now
accumulated **24 tribe caches**. Aggregating every post tagged by a wish-probe query ("is there an
app…", "wish there was…", "why isn't there…") across all of them yields **1,411 unique unmet-ask
threads** — enough to cluster the whole music-maker tribe at once instead of one sub at a time
(notes [022](022-demand-discovery-ipadmusic.md)–[025](025-demand-discovery-supply-side-showcase.md)).

`reddit-gaps.js`'s own coarse taxonomy reports "well-served" almost everywhere (our shelf is broad).
The signal is in the **raw wishes**, clustered by hand and validated against Google. Two findings:

**1. The loudest meta-signal is a feeling, not a feature.** Underneath every cluster is the same
sentence: *"I'm a beginner and every tool is too complicated — I just want something simple."* This
confirms and generalises note 022's r/ipadmusic finding across the whole tribe: for a humble lo-fi
console the opening is a missing *feeling* (playful, simple, "just noodle"), not a missing pro
feature. It is also, precisely, the north star.

**2. Seven concrete build candidates, ranked by demand × buildability × gap.** "Buildable" =
our touch-first, offline, lo-fi engine can do it *well* — which cleanly separates the ready ideas
from a **frontier pile** blocked on one shared capability (live audio-in / pitch detection).

| # | Idea | Reddit (cluster) | Google autocomplete | Shelf coverage | Buildable now? |
|---|------|-----------------|---------------------|----------------|----------------|
| 1 | **Ear-training *game*** | ~24 wishes | `ear training app` 22 · `ear training game` 11 | **none** (0 carts) | ✅ easy — and it's a *game* (most on-grain) |
| 2 | **Chord identifier** (tap notes → name) | 14+ | `chord finder app` 22 · `…from notes` 11 | ½ (we *play* chords: chordblossom, omnichord) | ✅ easy; *sung/from-audio* variant = frontier |
| 3 | **Chord-progression generator** | 18 | `chord progression generator` 19 · `…app` 21 | ½ (chordblossom voices, doesn't sequence) | ✅ emergent rule engine |
| 4 | **Sample chopper / loop toy** | **87 — biggest cluster** | `sampler app` 21 · `beat maker app` 22 | ½ (breakchop + ~14 sampler carts) | ⚠️ chop a *built-in* break = easy; import *your own* audio = frontier |
| 5 | **Dead-simple beatmaker for phone** | 11+ | `beat maker app` 22 · `melody maker app` 21 | ½ (groovebox/acidface exist, not framed *beginner*) | ✅ have groovebox + drumkit.h; needs a simple face |
| 6 | **Scale / mode + microtonal explorer** | scale + microtonal | `scale trainer app` 21 | ½ (scalegrid is scale-*locked*, not an explorer) | ✅ easy; microtonal = niche but passionate |
| 7 | **Generative idea / inspiration prompter** | 23 | modest | none | ✅ very on-grain + emergent; novel |

**The frontier pile** — high, recurring demand, all blocked on *the same* capability (robust
pitch/onset detection from live audio; the [`mic-spike`](../../tools/mic-spike/) is confirmed-live
but pitch is octave-noisy): **hum/sing → MIDI** (14), **chord-from-*audio***, **BPM-from-a-file/tap**.
Worth tracking together — landing the audio-input frontier converts all three at once.

---

## Why this matters

This is the first *cross-tribe* input to **lever #1** of
[demand-generation.md](../design/demand-generation.md) (*what to build*), and it turns the per-tribe
notes into a prioritised backlog. The read:

- **Build #1 (ear-training game) first** — the only candidate with *zero* coverage, strong demand,
  and a genuine *game* rather than "another utility app" (the honest fit for a fantasy console).
- **#2 + #3 (chord identify / progression) are the highest-leverage pair** — chordblossom already
  gets you halfway, so it's extension not cold-start.
- **#4 (sampler) has the most raw demand but the worst effort/payoff** — the wanted part (import
  your own audio) is exactly the hard part.

It also re-confirms note 022's reframe at scale: the "gap" for a niche lo-fi console is a missing
*feeling*, not a missing feature.

## Where to outreach (each idea's tribe)

The subs the wishes came from *are* the venues; `tools/leads.js draft <cart>` scaffolds a gift-first
post from the cart's own words once built.

- **Ear-training / chord / theory / scale (#1,2,3,6):** r/musictheory, r/WeAreTheMusicMakers, r/GarageBand (beginners), r/edmproduction.
- **Sampler / beatmaker (#4,5):** r/Sampling, r/koalasampler, r/makinghiphop, r/GarageBand.
- **Idea prompter (#7):** r/WeAreTheMusicMakers, r/musicproduction.

---

## Evidence

`node tools/reddit-gaps.js --drip` accumulated the 24 caches; aggregation over every wish-tagged
post produced the clusters above. Google figures from `node tools/aso-suggest.js --quick`.
Representative threads per cluster (read the signal yourself):

**Ear-training / learn by ear (#1):**
- [Looking for app to help play by ear](https://www.reddit.com/r/WeAreTheMusicMakers/comments/l5toah/looking_for_app_to_help_play_by_ear/)
- [Complete beginner; how do I learn turning melodies from my brain into…](https://www.reddit.com/r/WeAreTheMusicMakers/comments/n69o4l/complete_beginner_here_how_do_i_learn_turning/)
- [How do I start learning music theory in general?](https://www.reddit.com/r/chiptunes/comments/qm5bju/how_do_i_start_learning_music_theory_in_general/)

**Chord identify (#2):**
- [play or sing a note and find out what chord it is](https://www.reddit.com/r/WeAreTheMusicMakers/comments/8j7h1g/is_there_an_app_site_or_software_where_i_can_play/)
- [iPad app that tells you the chord name when you play](https://www.reddit.com/r/WeAreTheMusicMakers/comments/7o7rat/anyone_know_of_an_ipad_app_that_tells_yiu_the/)
- [identify chords as a combination of scale degrees](https://www.reddit.com/r/musictheory/comments/pcb01x/is_there_an_apptool_that_can_identify_chords_as_a/)

**Chord progression (#3):**
- [Any Good Chord Progression Apps?](https://www.reddit.com/r/ipadmusic/comments/yc5zup/any_good_chord_progression_apps/)
- [piano roll where I can lay down a melody or chord progression](https://www.reddit.com/r/edmproduction/comments/158168/looking_for_an_app_with_a_piano_roll_where_i_can/)
- [complete beginner — how do I know if the chord…](https://www.reddit.com/r/musicproduction/comments/1ubgk7r/im_a_complete_beginner_and_this_is_dumb_question/)

**Sampler / chop / loop (#4):**
- [How do I chop samples](https://www.reddit.com/r/GarageBand/comments/1t15k8n/how_do_i_chop_samples/)
- [mobile apps that can change the bpm of a sample?](https://www.reddit.com/r/GarageBand/comments/c0tsre/question_are_there_any_mobile_apps_that_can/)
- [how do I add drums to this sample?](https://www.reddit.com/r/sampling/comments/1tryhym/feedback_how_do_i_add_drums_to_this_sample/)

**Simple beatmaker (#5):**
- [an app for iPads to make beats on?](https://www.reddit.com/r/WeAreTheMusicMakers/comments/uzhack/is_there_an_app_for_ipads_to_make_beats_on/)
- [create music on a very simplified level?](https://www.reddit.com/r/WeAreTheMusicMakers/comments/7j6g50/is_there_an_app_in_existence_which_lets_you/)

**Scale / microtonal (#6):**
- [an app that lets me work with microtones?](https://www.reddit.com/r/musictheory/comments/nw2gow/is_there_an_app_that_lets_me_work_with_microtones/)
- [alternatives to the 440 hz tuning system](https://www.reddit.com/r/musictheory/comments/1h62ujs/are_there_any_other_alternatives_to_the_440_hz/)

**Generative idea prompter (#7):**
- [app that prompts new words to help write a song](https://www.reddit.com/r/WeAreTheMusicMakers/comments/fw2rvh/is_there_an_appwebsite_that_prompts_new_words/)
- [Audio inspiration board app?](https://www.reddit.com/r/WeAreTheMusicMakers/comments/zwm61j/audio_inspiration_board_app/)

**Frontier — hum/sing → MIDI (blocked on audio-in):**
- [Humming to GarageBand App?](https://www.reddit.com/r/GarageBand/comments/e0mizw/humming_to_garageband_app/)
- [MIDI keyboard for programming always… (wants sung input)](https://www.reddit.com/r/WeAreTheMusicMakers/comments/3z9qe6/as_a_trained_guitarist_using_a_midi_keyboard_for/)
