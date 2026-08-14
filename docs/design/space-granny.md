# Space Granny — the mascot

**STATUS: EXPLORING (2026-08-14)** — character and head shape LOCKED (#3 squat bouffant); per-app
differentiation resolved (two crops of one artwork). Open: whether she goes on the store at all, the
in-cart palette question, and the final icon art.

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

## Crop the head, not the body — and which granny goes where

Four framings were run through the identical pipeline (crop → `pixelsnap --grid 48x48 --colors 16`
→ border flood-fill to alpha → dark tile at ~86% fill → `icon-mask.js preview`) and judged at the
60px end, which is the only size that decides anything.

**The head crop beats the full body, for both characters, by a lot.** The saucer eats ~40% of the
tile and contributes nothing below about 120px. This is the single biggest improvement found, and it
is independent of which granny wins.

| framing | at 1024 | at 60px |
|---|---|---|
| bun, full body (`06`) | good | readable, face small |
| **bun, head (`08`)** | **clean, graphic, glasses crisp** | **still a granny in cat-eye specs** ✓ |
| afro, head (`09`) | warmest, has the smile + antenna | purple mass, face lost |
| afro, head + hands (`10`) | glasses already breaking up | ✗ purple blob on a pink bar |

**The afro is the better character and the worse icon.** It is warmer and more alive, but it spends
roughly twice the pixel budget on HAIR where the bun spends it on FACE, and at icon sizes the face is
the identity. The bun also carries the teal collar, the only thing stopping her going monochrome.

**"Head and hands" (`10`) was tested as the compromise that keeps a gear hint, and it failed.** Once
anything below the chin is in frame the face loses the pixels it needs: the glasses break into a pink
smear, the handbag becomes a stray blob, and the saucer reads as a bar rather than a ship.

So: **afro = hero art** (store shots, press, splash, trailer), **bun head = icon and sprite.** They
are clearly the same person; the question was never which granny but which one goes where.

⚠ **Consequence for the family plan:** a head crop cannot show her at a 303, so "same nan, different
gear" cannot be carried by a portrait icon. **Resolved below — two crops of one artwork.**

### Two crops, one artwork (`15`, `16`)

The per-app scenes (`15` — Rhodes with a lamp and plant, Guitar with a starfield and pedalboard,
Acid House Jam with smileys and cable spaghetti) solve differentiation beautifully **at large sizes**
and fail at icon sizes in the worst possible direction: the gear is the smallest, most detailed
element in each, so **the differentiator dies first** and all three converge on "purple blob over
pink blob" exactly where they most need to be apart.

Measured (`16`, the same acid tile at 60px both ways):

- **whole scene** — the glasses reduce to a thin pink line and the face is a smudge; the sacred
  element is gone, and the `303`/`808`/`909` labels are unreadable by 128px
- **zoomed** — the glasses dominate, the white eyes read inside them, and the 808's LED row survives
  as an orange band

So each app gets a signature **colour band along the bottom edge** (orange LEDs = acid, black/white
keys = Rhodes, the neck's diagonal = guitar): big simple shapes, which survive precisely where the
detailed scene collapses.

**The split, no art redrawn:**

| crop | where it goes |
|---|---|
| **whole scene** | App Store product page, press kit, in-app rack-picker tiles, the trailer |
| **zoomed (top ~60%: head + the top edge of the gear)** | the app icon |

What the zoom discards — lamp, plant, pedals, cable spaghetti — was already invisible below 128px.

Correction to an earlier note in this doc: the antenna does **not** inherently vanish at small sizes.
It survives when the head fills the tile (`09`) and dies in any framing that also carries the body.

Also framing, not art: a **full-bleed** crop (hair running off the tile edges instead of fitted inside
it) rescues the afro at 60px, because the face gets the frame and the eye completes the hair. Twice
now a framing problem was first mistaken for a defect in the character.

## The spec — what is sacred, and the one rule

Priority order (the maker's, 2026-08-14), highest first:

1. **Cat-eye glasses** — sacred, never compromised
2. **Huge alien eyes inside them** — this is what makes the alien/granny collision work
3. **Compact head/hair mass** — exists to let 1 and 2 dominate
4. **Saucer** — the broad base of the silhouette, but **only as a wide THIN band** (see below)
5. **Antenna** — alien punctuation; survives on CONTRAST, not size (a bright dot at 12px, where a
   1px purple stalk died)
6. **Hairdo** — interchangeable, an expression of the character rather than the character

Deliberately stupid proportions: massive glasses → tiny smiling mouth → surprisingly small fluffy
hair → one ridiculous antenna → enormous flat saucer. A stack of primitive shapes, which is what
makes a thing mascot-able.

### The rule: hair may grow UP, never SIDEWAYS

**Everything is allowed to be wide. Only the glasses are allowed to be tall.**

The cat-eye wings are a *horizontal* feature, and so is an afro, so they compete for one axis — and
the glasses lose, because the hair is the bigger mass. This single line explains every result above,
including why the bun beat the afro: the afro was never the problem, *sideways* was. Likewise the
saucer, which is free as a wide thin band and ruinous the moment it grows vertically (that is what
killed the head-and-hands crop, `10`).

### Head-shape explorations (`12`)

Six hair shapes over an identical face, glasses and saucer — a clean experiment, so the hair really
is the only variable. Judged by magnifying the sheet's own 12px, 8px and 16px-silhouette rows:

| # | shape | axis | at 12px |
|---|---|---|---|
| 1 | tiny bun | up | clean glasses; nub survives |
| 2 | fluffy perm | **sideways** | hair starts competing with the wings |
| **3** | **squat bouffant** | **up** | **the boldest, widest-reading glasses of all six** |
| 4 | mini topknot | up | near-identical to #1 |
| 5 | side puff | **sideways** | the only asymmetric silhouette, but bought in the expensive currency |
| 6 | puffy puff | **sideways** | biggest mass, taxes the wings most |

### LOCKED: #3 SQUAT BOUFFANT (`13`)

The only one with a bold distinctive hair shape that spends none of the horizontal budget. Note the
glasses in #3 are drawn identically to the other five and simply *read* wider, which is the rule
above made visible.

**The relationship to protect through every redraw: the glasses are WIDER THAN THE HAIR.** The wings
clear the bouffant at all four sizes. If a revision loses that, it has lost the character.

Two silhouette features to keep (`14`): the **notch in the crown** and the **narrow neck** between
hair and saucer. They make the outline three beats (wide crown → narrow neck → flared base) instead
of one lump, which is what separates it from #2's plain round head.

⚠ **The silhouette row did NOT decide this**, and should not be over-trusted for this character: in
solid fill the glasses are interior to the head mass and therefore *invisible*, so that row only ever
tested hair-plus-saucer (which is why #3 and #6 look alike there and clearly differ in colour). The
**12px colour row** decided it.

It holds down to 12px, where the pink wings are still the first thing the eye lands on. At 8px it is
a purple blob with a pink smudge — as all six were — so **8px needs its own hand-drawn glyph**, never
a downscale. The polka dots on the wings die below 32px: fine as large-size decoration, never
load-bearing.

Not pursued (parked, not rejected): **#3 leaning**, the bouffant stacked up AND offset to one side.
Asymmetry is the rarest property in a grid of home-screen icons and #5 proves it is achievable;
doing it vertically would get it without paying the wings.

Two things this sheet fixed that are not about hair:

- **The eyes.** Big whites with highlights inside the frames. The earlier art had small dark lenses
  that turned into two smudges at 12px; this is what makes the alien half survive shrinking, and it
  matters more than the hair choice.
- **The pearl necklace**, which at 16px is a bright horizontal band separating face from saucer —
  the separation the earlier versions lacked.

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
| `08-icon-bun-head-1024.png` | **the winner** — bun, head crop, survives 60px |
| `09-icon-afro-head-1024.png` | afro head crop: best illustration, loses the face when small |
| `10-icon-afro-headhands-1024.png` | the failed compromise — gear hint costs the glasses |
| `11-icon-bun-head-sizes.png` | the winner at every real display size, light + dark |
| `12-head-shape-explorations.png` | six hair shapes over one identical face at 32/16/12/8px + silhouettes — the sheet the spec above is read off |
| `13-squat-bouffant-locked.png` | **the locked head shape**, #3, at 32/16/12/8px |
| `14-silhouettes-16px.png` | the six silhouettes at 16px (hair + saucer only — the glasses are invisible in solid fill) |
| `15-app-scenes-rhodes-guitar-acid.png` | the three per-app scenes: store/press/rack-tile art |
| `16-scene-vs-zoom-at-60px.png` | the same acid tile at 60px, whole scene vs zoomed — why the icon is the zoom |

Related: [`store-agents.md`](store-agents.md) (the ASO tooling) ·
[`app-icon-mask.md`](app-icon-mask.md) (the mask and what it shaves) ·
[`demand-generation.md`](demand-generation.md) (the tribe test).
