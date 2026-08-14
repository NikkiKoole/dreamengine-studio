# Space Granny — the mascot

**STATUS: EXPLORING (2026-08-14)** — character chosen, first pixel conversion done and measured.
Open: whether she goes on the store at all, the in-cart palette question, and the final icon art.

An ancient, intergalactic grandmother who sits in a flying saucer and plays every instrument in the
shelf, badly, with opinions about your mix. She came out of a memory the maker has of being a
reincarnated alien grandmother, which is why she is not a focus-grouped cute animal and why she is
worth keeping.

The tone rule, from the concept sheet, verbatim: **keep it stupid, keep it iconic.** The machine
underneath stays serious (a real diode-ladder filter, a real 808 bank); the pilot is silly. The
contrast is the joke. A silly machine would not be funny.

## What she is for

The apps are a family with no face. `Tiny Jam` / `Tiny Acid Jam` / `Tiny Pedalboard` look like three
unrelated apps in a store grid. Granny is the family resemblance: **same nan, different gear**, one
icon per app, which the concept sheet already drew as its PLAYS EVERYTHING row.

That also settles the naming problem, because the word cannot carry the family:

- App Store names must be **unique across the whole store**, so only one app can ever be called
  "Space Granny".
- The home screen truncates around **12 characters**, so `Space Granny Acid` and `Space Granny Piano`
  both render `Space Gran…`.

So the **picture** carries the family and the **label** says what the app is. Four independent slots:

| layer | job | budget |
|---|---|---|
| icon | family resemblance | granny + the instrument |
| home screen name (`CFBundleDisplayName`) | tell them apart | ~12 chars |
| store title | search | 30 chars |
| developer name | the studio | one string |

`Granny` at six characters is the family word that actually fits a home screen. "Space" lives in the
icon, the splash and the marketing, where the flying saucer says it anyway.

## Naming proposal (not applied)

Measured with `node tools/aso-score.js tinyacidjam --title … --keywords …`. The full A/B is in the
session that produced this doc; the load-bearing results:

- **The rename is ASO-neutral.** Every `Space Granny` title variant scored within 1 point of the
  committed `Tiny Acid Jam: 303 Groovebox` (57), and the only movement was the budget sub-score
  reacting to a shorter string. "Tiny" was never earning anything either, so the name is a free
  brand decision, not a trade.
- **The points are in the keyword field, not the title.** The committed field repeats `808` and `909`
  from the subtitle; a word ranks once, so those are dead characters. Rebalancing takes the listing
  **57 → 66** with hygiene at 100.
- **`bassline` beats `groovebox` on the demand data.** Six of the twenty phrases in
  `apps/tinyacidjam/seo-brief.md` contain "bassline" (`303 bassline vst`, `tb 303 bassline`, …) while
  every `groovebox` phrase is the festival and the venue (`middelburg`, `nottingham`, `at the beach`).
  Also worth having: **`baseline`**, which is in the data because people mistype "bassline".

The best-scoring listing found (66, no hygiene waste):

```
name (home screen)  Space Granny
title               Granny Acid Bassline Groovebox     30/30
subtitle            808, 909, house in your pocket     30/30
keywords            tb303,rebirth,338,space,baseline,drum,machine,beat,
                    techno,rave,dance,synth,sequencer,vst
```

⚠ **`apps/tinyacidjam/seo-brief.md` is polluted** and its REACH score should not be chased until it
is re-cut: the demand words include `park`, `nottingham` and `events` because Google autocomplete
found **Groovebox the venue**, and the phrases include `acid house t shirt` / `movie` / `book`, which
are the culture and not people shopping for an app. The other three sub-scores are trustworthy.

⚠ `rebirth`, `338` and `operator` in the committed keyword field are deliberate competitor targeting
(ReBirth RB-338, Ableton Operator), not typos. Competitor **trademarks in metadata are a real
rejection risk** at review; same applies to adding `roland`.

## The pixel conversion — what was measured

Source: `03-space-granny-sheet.png`, hero crop, through `tools/pixelsnap.js`.

- **Her native logical resolution is ~75×96** (auto-detect: cell ≈ 3.3 source px, weak grid signal).
  The generator drew her at that size, so she was never drawn at icon scale.
- **48×61 is the sweet spot** for automatic conversion. 75×96 keeps noise; **32×41 destroys the
  face** entirely, which confirms the concept sheet's own instinct: the small sizes (8×8, 12×12) must
  be **hand-drawn glyphs, not downscales**.
- **pico32 does not fit her.** Two gaps, both visible in the output:
  - **no light teal** — the mint dress, which is the only colour breaking her pink/purple monochrome,
    maps to light grey (`6 #c2c3c7`). The only teal in the palette is `19 #125359`, and it is dark.
  - **no vivid mid purple** — the hair lands on `13 #83769c`, a grey-lavender, and she goes washed out.

  This matters for the **in-cart sprite** (index-based, must be pico32) and **not for the app icon**
  (a plain PNG, free palette). If she goes in-cart, the dress is a deliberate recolour decision:
  `19` for a darker but honest teal, or move the accent somewhere else entirely.
- **A colour key cannot separate her from the sheet background.** The character's darks (glasses
  lenses, saucer underside, dress shadow) are the same values as the sheet's near-black backdrop, so
  `colorkey` at any useful tolerance eats her. What works: snap first, then **flood-fill to alpha from
  the border only**, which keeps identical darks that are interior.
- **A pink background hides her glasses.** The frame is close enough to a pink tile that her single
  most distinctive feature disappears and only the lenses read. The dark tile (`rgb(26,16,38)`) is
  much stronger and is what `06-icon-dark-1024.png` uses.

### The icon candidate

`06-icon-dark-1024.png` — granny at 72% of tile height on the dark tile, integer-upscaled ×16 from a
64×64 logical tile, so every pixel stays square.

- `node tools/icon-mask.js check` → **clean**, all four corners flat background, the mask takes only
  backdrop. There is headroom to scale her up further before the bun is at risk.
- `node tools/icon-mask.js preview` → `07-icon-size-preview.png`. **She survives to 60px**: bun,
  glasses and saucer all still read at notification size, light and dark.

It is a first pass from generated art, not final: the saucer rim is muddy and the face wants a hand.
But it clears the bar, which is what it was for.

## Open

- Ship the granny at all, or keep the knob-on-dark-grey. Cheap test: **post the character sheet**
  (not the app) to the pixel-art and acid-house tribes that `tools/leads.js` points at. It is a
  signal and free marketing in the same move.
- Claim the plain name `Space Granny` in App Store Connect before the singles ship, since it is
  unique-per-store and would be wanted for a later flagship.
- Draw the icon set **as a set** (303, rhodes, guitar) even for unshipped apps — the cheapest moment
  to notice two of them look alike.
- Decide the in-cart palette question above if she is ever drawn inside a cart rather than only on
  the store.
- Reactive granny on the panel: the sheet's `idle / bounce / groove / spin / excited` frames map onto
  transport state, filter movement and accents. Six 16×16 frames is six of the 64 sprite slots.
- `UFO CUSTOMIZATION` on the sheet is a cosmetic IAP that gates no features, which fits the
  "you buy it, you own it" line in the store description better than locking an instrument.

## Files

| file | what |
|---|---|
| `01-granny-pixx-nan9.png` | first concepts: Granny Pixx + NAN-9 (rejected: bald head reads as generic alien at small size) |
| `02-alien-grandma.png` | purple-hair concept, afro variant |
| `03-space-granny-sheet.png` | **the chosen character**, full bible: scales, expressions, animations, UFO skins, scenes |
| `04-silhouette-icon-mask.png` | the 1-bit reduction + icon-size study |
| `05-hero-35x46-alpha.png` | first pixel conversion, her own 16-colour palette, alpha cut |
| `06-icon-dark-1024.png` | icon candidate, mask-clean |
| `07-icon-size-preview.png` | how iOS shows it, 1024 down to 60, light + dark |

Related: [`store-agents.md`](store-agents.md) (the ASO tooling) ·
[`app-icon-mask.md`](app-icon-mask.md) (the mask and what it shaves) ·
[`demand-generation.md`](demand-generation.md) (the tribe test).
