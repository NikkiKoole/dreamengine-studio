# SEO worksheet — Tiny Jam

> **This is a palette, not the page.** Write `press.md` and the App Store listing in your
> own voice; reach here for the words the world actually uses. Nothing here is copy — it is
> never rendered into the press page. Regenerate: `node tools/aso-brief.js tinyjam`. Check your
> finished copy against it: `node tools/aso-coverage.js tinyjam` (the mirror).

_generated 2026-07-08 · country us · seeds: step sequencer, subtractive synth, drum synthesis, generative melody, chord voicing, song arrangement, swing timing, analog voice modeling_

---

## Char budgets & your current listing

| field | limit | yours | count |
|---|---|---|---|
| Title | 30 | Tiny Jam: Pocket Music Toys | 27/30 |
| Subtitle | 30 | Make grooves with micro synths | 30/30 |
| Keywords | 100 | groovebox,drum,machine,sampler,sequencer,beat,studio,loop,mixer,auv3,operator,bass,pads,rhythm,arp | 98/100 |

You rank on the UNION of the three — a word only needs to appear once. Title/subtitle read
like a person wrote them; the keyword field is the hidden word-soup.

## For the App Store keyword field — WORDS (priority order)

Apple auto-combines single words and ignores stopwords, so feed singles. ★ = the word is both
**searched** (Google demand) and **targeted** (a competitor uses it) — the strongest picks.

- ★ guitar
- ★ studio
- ★ piano
- ★ midi
- ★ machine
- ★ drums
- generator
- voicings
- golf
- logic
- chords
- songs
- ableton
- vst
- sound
- jazz
- whether
- plugin
- changer
- pdf
- book
- auv3
- synthesizer
- audio

Paste into `aso-compose`:

```
node tools/aso-compose.js --title "Tiny Jam: Pocket Music Toys" --subtitle "Make grooves with micro synths" \
  --candidates "guitar,studio,piano,midi,machine,drums,generator,voicings,golf,logic,chords,songs,ableton,vst,sound,jazz,whether,plugin,changer,pdf,book,auv3,synthesizer,audio"
```

Already in your title/subtitle (don't repeat in keywords): music

## For your website / press kit — PHRASES people google

Google ranks natural-language phrases, not word-soup — so these belong in `press.md` prose,
the page `<title>`/headings, and the meta description. **Work in the ones that fit, in your
own words** — don't paste them. (This is the demand side; the store field above is where the
bare keywords go.)

- what is a subtractive synth
- what is chord voicing
- what is a step sequencer
- drum manufacturing process
- 8 step sequencer diy
- a subtractive synth which circuit determines the initial pitch
- open voicing chords
- every chord voicing has its own
- chord voicing vs inversion
- step sequencer fl studio
- step sequencer logic
- step sequencer ableton
- step sequencer vst
- step sequencer plugin
- step sequencer online
- step sequencer reaper
- step sequencer hardware
- step sequencer app
- step sequencer garageband
- subtractive synth ableton

## Competition — what's winnable

| seed | difficulty | strongest incumbent |
|---|---|---|
| step sequencer | MEDIUM 52/100 | Drum Machine - Music Maker (6k ratings) |
| subtractive synth | MEDIUM 51/100 | Redshrike - AUv3 Plug-in Synth (0k ratings) |
| drum synthesis | HARD 74/100 | DrumKnee 3D Drums - Drum set (16k ratings) |
| generative melody | HARD 69/100 | AI Song Music Generator: Muzio (17k ratings) |
| chord voicing | MEDIUM 51/100 | Keytionary: Chords & Voicings (0k ratings) |
| song arrangement | HARD 70/100 | MyTunes : AI Music Generator (37k ratings) |
| swing timing | MEDIUM 60/100 | Swing Timing (— ratings) |
| analog voice modeling | MEDIUM 65/100 | AI Voice Clone Generator (5k ratings) |

EASY + relevant + low-authority = where a fresh app wins. HARD = crowded; skip unless core.

---
_worksheet regenerable; edit `press.md`, not this file. Terms drift — re-run before a launch pass._

<!-- de:driftable cmd="node tools/aso-brief.js tinyjam" as-of="2026-07-08" inputs="tools/carts,apps/tinyjam/app.json,tools/aso-brief.js,tools/aso-research.js,tools/aso-suggest.js" watch="numbers" -->

<!-- aso-coverage
{"generated":"2026-07-08","country":"us","seeds":["step sequencer","subtractive synth","drum synthesis","generative melody","chord voicing","song arrangement","swing timing","analog voice modeling"],"phrases":["what is a subtractive synth","what is chord voicing","what is a step sequencer","drum manufacturing process","8 step sequencer diy","a subtractive synth which circuit determines the initial pitch","open voicing chords","every chord voicing has its own","chord voicing vs inversion","step sequencer fl studio","step sequencer logic","step sequencer ableton","step sequencer vst","step sequencer plugin","step sequencer online","step sequencer reaper","step sequencer hardware","step sequencer app","step sequencer garageband","subtractive synth ableton"],"words":["guitar","studio","piano","midi","machine","drums","generator","voicings","golf","logic","chords","songs","ableton","vst","sound","jazz","whether","plugin","changer","pdf","book","auv3","synthesizer","audio"],"visible":["music"]}
-->
