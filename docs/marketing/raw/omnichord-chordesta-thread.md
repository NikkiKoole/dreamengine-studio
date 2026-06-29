# RAW SOURCE: the CHORDESTA / "HarpiChord" omnichord thread (r/synthesizers)

> **Why this is kept:** a real-world natural experiment of *almost exactly our plan* — someone built a
> free browser-then-Android Omnichord and posted it to r/synthesizers. The reactions are the single
> most useful field data we have for the Strumharpy launch. Distilled lessons:
> [`../research-insights.md`](../research-insights.md). This file is the raw-ish source (trimmed of
> Reddit UI chrome/ads), captured 2026-06-29.
>
> **Source:** https://www.reddit.com/r/synthesizers/comments/1t0ayoz/couldnt_afford_an_omnichord_this_is_what_happened/
> (note the thread title itself: *"couldn't afford an omnichord, this is what happened"* — the exact
> pitch our draft copy used.)

## The post (OP: foamfriend)

> So I've had an Omnichord problem for a few years. Not an Omnichord, I don't have one of those. I have
> an Omnichord problem. Every few months I check eBay, see the prices, make a face, and close the tab.
>
> At some point I thought: how hard can it be to just... build one. In JavaScript. In a browser tab.
> Like a normal person. It worked! Kind of. It had the latency of a tired sloth but it worked…
>
> [moved to Android → NDK → JNI → Google's Oboe engine] … now I have a full app with reverb, delay,
> chorus, phaser, flanger, tube saturation, a Leslie rotary sim, vibrato, MIDI out, all 12 keys, 9 chord
> types, and gold glitter trails on the strum plate…
>
> I just wanted an instrument I couldn't afford. Instead I learned C++. … It's on Android. It's free.
> Only the extended FX can be bought, but it's free.
>
> [later edit] the app is now called **CHORDESTA**. I rebranded because Google Play's autocorrect kept
> mangling "HarpiChord" into "HarpiSchord" (the instrument).

Score: ~102 up / ~91 comments. Posted as an ad ("Promoted").

## The reaction — by theme (verbatim-ish quotes)

**1. Smelled as AI-coded marketing (the dominant reaction):**
- *Egg_Crust:* "I posted a comment to an ad that obviously used AI to code and write their marketing
  story." / "the obvious marketing in the writing pisses me off so bad. The short sentences … the
  problem/solution narrative of every scam snake oil product in history, and the casual mention of 'I
  have a family' … they brag it's free to TRY, meaning they purposely left off the price."
- *Birdy_num_nums:* "patting himself on the back with a 1 day old sock puppet account."
- *igorski81:* "the sales pitch of 'I wanted but couldn't so I did it myself' is a little stale … it
  screams vibe coding." (also: JS→C++ + platform switch without validating *why* = the tells)
- *Cbjfan1:* "built the web app using Vercel which markets using AI all over it."

**2. Communities are guilty-until-proven-innocent on free plugins:**
- *djostreet:* "Any music sub has been flooded with low effort bullshit free plugins. You're guilty
  until proven innocent tbh."
- *impablomations:* "We're getting loads of them in r/VSTi. Every other person seems to be vibe coding
  shitty VSTs."

**3. "Share the source" expectation (open-source norm in this scene):**
- *KudzuPlant / Evening-Notice-7041:* "If you are going to share vibe coded junk you might as well share
  the code." / asks for GitHub + license.

**4. The counter-camp (don't-care-if-it's-good-and-free):**
- *zendogsit:* "If it's free and useful I don't see what the problem is?"
- *AistoB:* "The vibe coding hate is weird, does it make the noises you want? Then who cares how it was
  created?"
- Several who *tried* it liked it: "actually sounds good", "my daughter had a blast", "sounds
  beautiful", *SabreSour:* "The web browser based Harpsichord is by far my favorite I've tried."

**5. Even *offering* a vibe-coded build got flamed (2nd data point):**
- A commenter offered a GPT-5.5-made VST3 ("OmniStrumXL.vst3", Dropbox link). Score **−12**; reply from
  *djostreet:* "Fuck off"; *Bshazer:* "hahahah". (Dropping unsolicited AI-made binaries into the thread
  = instant rejection.)

**6. The competitive landscape (named by commenters):**
- **minichord** (minichord.com) — open-source hardware, *beloved*: "frigging awesome", "dope esp w
  aftermarket diagonal touch plate", people hacking/extending it (CoPilot-added scale-lock, bass note).
- **Chordion** (olympianoiseco.com/apps/chordion) — the go-to **iOS** option, named repeatedly.
- **MidiStrum** (f-droid.org/packages/com.gnethomelinux.midistrum/) — open-source Android, "solid choice
  … hooked up to Dexed + delay/reverb and having a ball."
- **Chordion / the web Harpsichord** — quality bar exists; a *web* omnichord can be someone's favorite.

**7. The iOS gap = unmet demand:**
- Multiple "iOS release?" / "please make it for iphone!". OP: "I have no MAC :( Like I have no
  Omnichord :D". (Chordion is the main iOS answer; the field is thin.)

**8. The curator contact:**
- *SabreSour:* "I have tested a LOT of these soft synth omnichord virtual instruments for **r/Omnichord
  and OmnichordContentHub.com**." → a real reviewer/curator in exactly our niche.

**9. Naming-collision lesson:**
- "HarpiChord" → Google Play autocorrected to "HarpiSchord" (harpsichord) → confusion → rebrand to
  CHORDESTA. (Check store search/autocorrect collisions before naming.)
