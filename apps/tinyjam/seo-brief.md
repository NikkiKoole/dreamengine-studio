# SEO worksheet — Tiny Jam

> **This is a palette, not the page.** Write `press.md` and the App Store listing in your
> own voice; reach here for the words the world actually uses. Nothing here is copy — it is
> never rendered into the press page. Regenerate: `node tools/aso-brief.js tinyjam`. Check your
> finished copy against it: `node tools/aso-coverage.js tinyjam` (the mirror).

_generated 2026-08-16 · country us · seeds: step sequencer, subtractive synth, drum synthesis, generative melody, analog voice modeling, adsr envelope, chord voicing, granular synth_

---

## Char budgets & your current listing

| field | limit | yours | count |
|---|---|---|---|
| Title | 30 | Tiny Jam: Pocket Music Toys | 27/30 |
| Subtitle | 30 | Make grooves with micro synths | 30/30 |
| Keywords | 100 | guitar,piano,chord,808,909,303,machine,groovebox,drum,techno,operator,sequencer,harp,beat,keys,midi | 99/100 |

You rank on the UNION of the three — a word only needs to appear once. Title/subtitle read
like a person wrote them; the keyword field is the hidden word-soup.

## For the App Store keyword field — WORDS (priority order)

Apple auto-combines single words and ignores stopwords, so feed singles. ★ = the word is both
**searched** (Google demand) and **targeted** (a competitor uses it) — the strongest picks.

- ★ studio
- ★ synthesizer
- ★ machine
- ★ midi
- ★ sound
- ★ drums
- ★ piano
- song
- logic
- generator
- ableton
- vst
- changer
- plugin
- chords
- guitar
- hardware
- audio
- kit
- synthetic
- auv3
- diy
- songs
- eurorack

Paste into `aso-compose`:

```
node tools/aso-compose.js --title "Tiny Jam: Pocket Music Toys" --subtitle "Make grooves with micro synths" \
  --candidates "studio,synthesizer,machine,midi,sound,drums,piano,song,logic,generator,ableton,vst,changer,plugin,chords,guitar,hardware,audio,kit,synthetic,auv3,diy,songs,eurorack"
```

Already in your title/subtitle (don't repeat in keywords): music

## For your website / press kit — PHRASES people google

Google ranks natural-language phrases, not word-soup — so these belong in `press.md` prose,
the page `<title>`/headings, and the meta description. **Work in the ones that fit, in your
own words** — don't paste them. (This is the demand side; the store field above is where the
bare keywords go.)

- what is a subtractive synth
- how to synthesize drums
- what is a step sequencer
- diy 16 step sequencer
- a subtractive synth which circuit determines the initial pitch
- drum manufacturing process
- step sequencer fl studio
- step sequencer ableton
- step sequencer logic
- step sequencer vst
- step sequencer plugin
- step sequencer online
- step sequencer reaper
- step sequencer hardware
- step sequencer app
- step sequencer midi controller
- subtractive synthesis vs fm
- subtractive synth vst
- subtractive synth ableton
- subtractive synthesis sound design

## Competition — what's winnable

| seed | difficulty | strongest incumbent |
|---|---|---|
| step sequencer | MEDIUM 50/100 | Sequentia: MIDI Sequencer (0k ratings) |
| subtractive synth | MEDIUM 51/100 | Redshrike - AUv3 Plug-in Synth (0k ratings) |
| drum synthesis | HARD 66/100 | Drum Tuner - iDrumTune Pro (2k ratings) |
| generative melody | HARD 69/100 | AI Song Music Generator: Muzio (18k ratings) |
| analog voice modeling | HARD 67/100 | Voice Synth Modular (3k ratings) |
| adsr envelope | EASY 12/100 | Envelope Printer Labels (0k ratings) |
| chord voicing | MEDIUM 53/100 | Chord Analyser (Chord Finder) (0k ratings) |
| granular synth | MEDIUM 46/100 | Open Granular (0k ratings) |

EASY + relevant + low-authority = where a fresh app wins. HARD = crowded; skip unless core.

---
_worksheet regenerable; edit `press.md`, not this file. Terms drift — re-run before a launch pass._

<!-- de:driftable cmd="node tools/aso-brief.js tinyjam" as-of="2026-08-16" inputs="tools/carts,apps/tinyjam/app.json,tools/aso-brief.js,tools/aso-research.js,tools/aso-suggest.js" watch="numbers" -->

<!-- aso-coverage
{"generated":"2026-08-16","country":"us","seeds":["step sequencer","subtractive synth","drum synthesis","generative melody","analog voice modeling","adsr envelope","chord voicing","granular synth"],"phrases":["what is a subtractive synth","how to synthesize drums","what is a step sequencer","diy 16 step sequencer","a subtractive synth which circuit determines the initial pitch","drum manufacturing process","step sequencer fl studio","step sequencer ableton","step sequencer logic","step sequencer vst","step sequencer plugin","step sequencer online","step sequencer reaper","step sequencer hardware","step sequencer app","step sequencer midi controller","subtractive synthesis vs fm","subtractive synth vst","subtractive synth ableton","subtractive synthesis sound design"],"words":["studio","synthesizer","machine","midi","sound","drums","piano","song","logic","generator","ableton","vst","changer","plugin","chords","guitar","hardware","audio","kit","synthetic","auv3","diy","songs","eurorack"],"visible":["music"]}
-->
