# SEO worksheet — Acid Candy

> **This is a palette, not the page.** Write `press.md` and the App Store listing in your
> own voice; reach here for the words the world actually uses. Nothing here is copy — it is
> never rendered into the press page. Regenerate: `node tools/aso-brief.js acidcandy`. Check your
> finished copy against it: `node tools/aso-coverage.js acidcandy` (the mirror).

_generated 2026-07-18 · country us · seeds: 303 emulator, acid house, rebirth, bassline, 808, techno_

---

## Char budgets & your current listing

| field | limit | yours | count |
|---|---|---|---|
| Title | 30 | Acid Candy: Pocket 303 | 22/30 |
| Subtitle | 30 | Squelchy acid house machine | 27/30 |
| Keywords | 100 | 303,acid,house,techno,808,909,emulator,bassline,groovebox,synth,drum,machine,sequencer,rave,338 | 95/100 |

You rank on the UNION of the three — a word only needs to appear once. Title/subtitle read
like a person wrote them; the keyword field is the hidden word-soup.

## For the App Store keyword field — WORDS (priority order)

Apple auto-combines single words and ignores stopwords, so feed singles. ★ = the word is both
**searched** (Google demand) and **targeted** (a competitor uses it) — the strongest picks.

- ★ deep
- bass
- apple
- radio
- music
- gba
- retro
- bassliner
- drum
- apparel
- sound
- festival
- booster
- vst
- beat
- 2026
- classic
- junkie
- fusion
- live
- smiley
- guitar
- appliance
- favorite

Paste into `aso-compose`:

```
node tools/aso-compose.js --title "Acid Candy: Pocket 303" --subtitle "Squelchy acid house machine" \
  --candidates "deep,bass,apple,radio,music,gba,retro,bassliner,drum,apparel,sound,festival,booster,vst,beat,2026,classic,junkie,fusion,live,smiley,guitar,appliance,favorite"
```

Already in your title/subtitle (don't repeat in keywords): machine

## For your website / press kit — PHRASES people google

Google ranks natural-language phrases, not word-soup — so these belong in `press.md` prose,
the page `<title>`/headings, and the meta description. **Work in the ones that fit, in your
own words** — don't paste them. (This is the demand side; the store field above is where the
bare keywords go.)

- acid house app
- rebirth app
- bassline app
- 808 apparel
- 303 emulator vst
- acid house movie
- deep house app
- rebirth 2026
- rebirth apparel
- bassliner fusion
- bassline apple music
- 8080
- 808 appliance repair
- 303 emulator online
- acid house music
- deep house apple music playlist
- rebirth festival
- rebirth application
- bassliner
- bassline junkie apple music

## Competition — what's winnable

| seed | difficulty | strongest incumbent |
|---|---|---|
| 303 emulator | HARD 70/100 | Emu — Game Consoles App (10k ratings) |
| acid house | MEDIUM 35/100 | Pure Acid (0k ratings) |
| rebirth | MEDIUM 60/100 | Seven Knights Re:BIRTH (4k ratings) |
| bassline | MEDIUM 44/100 | Bass Line Composer (0k ratings) |
| 808 | MEDIUM 60/100 | Score808 - Sport App & IPTV (1k ratings) |
| techno | MEDIUM 57/100 | Techno Media Player (0k ratings) |

EASY + relevant + low-authority = where a fresh app wins. HARD = crowded; skip unless core.

---
_worksheet regenerable; edit `press.md`, not this file. Terms drift — re-run before a launch pass._

<!-- de:driftable cmd="node tools/aso-brief.js acidcandy" as-of="2026-07-18" inputs="tools/carts,apps/acidcandy/app.json,tools/aso-brief.js,tools/aso-research.js,tools/aso-suggest.js" watch="numbers" -->

<!-- aso-coverage
{"generated":"2026-07-18","country":"us","seeds":["303 emulator","acid house","rebirth","bassline","808","techno"],"phrases":["acid house app","rebirth app","bassline app","808 apparel","303 emulator vst","acid house movie","deep house app","rebirth 2026","rebirth apparel","bassliner fusion","bassline apple music","8080","808 appliance repair","303 emulator online","acid house music","deep house apple music playlist","rebirth festival","rebirth application","bassliner","bassline junkie apple music"],"words":["deep","bass","apple","radio","music","gba","retro","bassliner","drum","apparel","sound","festival","booster","vst","beat","2026","classic","junkie","fusion","live","smiley","guitar","appliance","favorite"],"visible":["machine"]}
-->
