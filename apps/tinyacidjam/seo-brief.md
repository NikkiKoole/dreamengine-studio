# SEO worksheet — Tiny Acid Jam

> **This is a palette, not the page.** Write `press.md` and the App Store listing in your
> own voice; reach here for the words the world actually uses. Nothing here is copy — it is
> never rendered into the press page. Regenerate: `node tools/aso-brief.js tinyacidjam`. Check your
> finished copy against it: `node tools/aso-coverage.js tinyacidjam` (the mirror).

_generated 2026-07-19 · country us · seeds: acid house, 303 bassline, groovebox, acid techno, drum machine_

---

## Char budgets & your current listing

| field | limit | yours | count |
|---|---|---|---|
| Title | 30 | Tiny Acid Jam · 303 | 19/30 |
| Subtitle | 30 | House groovebox in your pocket | 30/30 |
| Keywords | 100 | 808,909,emulator,techno,bassline,drum,synth,sequencer,rave,rebirth,338,tb303,dance,operator,loop | 96/100 |

You rank on the UNION of the three — a word only needs to appear once. Title/subtitle read
like a person wrote them; the keyword field is the hidden word-soup.

## For the App Store keyword field — WORDS (priority order)

Apple auto-combines single words and ignores stopwords, so feed singles. ★ = the word is both
**searched** (Google demand) and **targeted** (a competitor uses it) — the strongest picks.

- ★ synth
- ★ deep
- ★ guitar
- bass
- radio
- kit
- beat
- roland
- sound
- park
- beats
- music
- electronic
- baseline
- sounds
- old
- nottingham
- pad
- yamaha
- audio
- events
- drums
- vst
- dance

Paste into `aso-compose`:

```
node tools/aso-compose.js --title "Tiny Acid Jam · 303" --subtitle "House groovebox in your pocket" \
  --candidates "synth,deep,guitar,bass,radio,kit,beat,roland,sound,park,beats,music,electronic,baseline,sounds,old,nottingham,pad,yamaha,audio,events,drums,vst,dance"
```

## For your website / press kit — PHRASES people google

Google ranks natural-language phrases, not word-soup — so these belong in `press.md` prose,
the page `<title>`/headings, and the meta description. **Work in the ones that fit, in your
own words** — don't paste them. (This is the demand side; the store field above is where the
bare keywords go.)

- acid house examples
- what is acid techno
- drum machine app
- acid house movie
- acid house t shirt
- acid house smiley face
- acid house clothing
- acid house book
- acid house artists
- 303 bassline patterns
- 303 bassline vst
- 303 bassline synth
- 303 bassline midi
- 303 bassline generator
- tb 303 bassline
- 303 acid bassline
- groovebox festival
- groovebox at the beach
- groovebox middelburg
- groovebox nottingham

## Competition — what's winnable

| seed | difficulty | strongest incumbent |
|---|---|---|
| acid house | MEDIUM 35/100 | Pure Acid (0k ratings) |
| 303 bassline | MEDIUM 35/100 | Bassline (— ratings) |
| groovebox | MEDIUM 59/100 | Groovebox - Beat Synth Studio (5k ratings) |
| acid techno | MEDIUM 43/100 | DI.FM - Electronic Music Radio (2k ratings) |
| drum machine | HARD 67/100 | EGDR808 Drum Machine lite (1k ratings) |

EASY + relevant + low-authority = where a fresh app wins. HARD = crowded; skip unless core.

---
_worksheet regenerable; edit `press.md`, not this file. Terms drift — re-run before a launch pass._

<!-- de:driftable cmd="node tools/aso-brief.js tinyacidjam" as-of="2026-07-19" inputs="tools/carts,apps/tinyacidjam/app.json,tools/aso-brief.js,tools/aso-research.js,tools/aso-suggest.js" watch="numbers" -->

<!-- aso-coverage
{"generated":"2026-07-19","country":"us","seeds":["acid house","303 bassline","groovebox","acid techno","drum machine"],"phrases":["acid house examples","what is acid techno","drum machine app","acid house movie","acid house t shirt","acid house smiley face","acid house clothing","acid house book","acid house artists","303 bassline patterns","303 bassline vst","303 bassline synth","303 bassline midi","303 bassline generator","tb 303 bassline","303 acid bassline","groovebox festival","groovebox at the beach","groovebox middelburg","groovebox nottingham"],"words":["synth","deep","guitar","bass","radio","kit","beat","roland","sound","park","beats","music","electronic","baseline","sounds","old","nottingham","pad","yamaha","audio","events","drums","vst","dance"],"visible":[]}
-->
