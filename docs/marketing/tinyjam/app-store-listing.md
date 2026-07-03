# Tiny Jam — App Store listing (US launch)

> **Status:** DRAFT (2026-07-03). Canonical display name is **Tiny Jam** (two words — better
> tokenisation than the compound, and no name collision; the `tinyjam` directory and
> `com.dreamengine.tinyjam` bundle id keep the compact form). The concrete App Store
> *metadata* for the Tiny Jam umbrella
> app — title, subtitle, keyword field, and the per-module IAP naming scheme. US-first (one
> market, one set of fields; localization is deliberately deferred). Companion to the
> go-to-market plan in [`tinyjam-marketing.md`](tinyjam-marketing.md) (tribes/channels) and
> the pricing/manifest work in [`../../design/share-panel.md`](../../design/share-panel.md).
> The ASO method + tooling behind these numbers:
> [`../../design/store-agents.md`](../../design/store-agents.md) §ASO.
>
> All fields below lint clean (`node tools/aso-lint.js`) and are grounded in a live
> `tools/aso-research.js` run (2026-07-03). Re-run before submitting — the landscape drifts.

## The strategy in one line

**The visible fields sell the soul (a collection of pocket music toys); the hidden keyword
field targets the *generic* instruments people search; each module ships as an IAP whose
display name carries that module's *specific* vibe.** A collection app that promotes 8
modules gets ~8 extra mini-listings in search — an edge single-app competitors don't have.

Why not chase the obvious head terms: they're unwinnable and (some) off-intent.

| Term | Difficulty | Verdict |
|---|---|---|
| `music maker` | 83 HARD | BandLab 490k / Groovepad 514k own it — skip |
| `beat maker` | 80 HARD | Drum Pad Machine 672k — skip |
| `pocket music` | 68 HARD | *and* full of mp3 **players** (wrong intent) — a tagline, not a keyword |
| `synthesizer` | 66 HARD | big but relevant — worth a token |
| `drum machine` | 61 MEDIUM | winnable + relevant ✓ |
| `groovebox` | 59 MEDIUM | winnable, and we have the module ✓ |
| ⭐ `pocket operator` | 50 MEDIUM (incumbents ~225 ratings) | **our tribe, weak incumbents — the prize** |

## App-level metadata (US)

```
Name/Title:  Tiny Jam: Pocket Music Toys        (27/30)
Subtitle:    Make grooves with synths          (24/30)
Keywords:    groovebox,drum,machine,sampler,sequencer,beat,studio,loop,mixer,auv3,operator,bass,pads,rhythm,arp   (98/100)
```
- Keyword field notes: singles only — Apple **auto-combines** them (`drum`+`machine`,
  and `pocket`(title)+`operator` → "pocket operator", the tribe term, for free). No
  stopwords, no cross-field repeats, no wasted comma-spaces. `synth` is *not* here because
  `synths` is already in the subtitle (the highest-value place for it).
- **Promotional text** (170 chars, editable anytime, not indexed): use the tagline here —
  e.g. *"A growing collection of pocket-sized music toys. New instruments added over time."*
  (Note: dropped "great" — filler adjectives read cheap, same family as "fun"/"happy".)

## Per-module IAP naming (each a promoted IAP = its own search surface)

Each in-app purchase has a **Display Name (~30)** + **Description (~45)**. Promote them (up
to 20) so each module gets its own product page and can surface in App Store search. Put the
*module-specific* keywords here, not in the app's shared 100-char field.

| Module | IAP display name (draft) |
|---|---|
| acidrack | `Acid Rack — 303 Bass Synth` |
| yachtrack | `Yacht Rack — Yacht Rock Kit` |
| groovebox | `Groovebox — Drum & Bass Box` |
| epiano | `Electric Piano` |
| mellotron | `Mellotron Tape Keys` |
| moog-style | `Analog Synth Ladder` *(renamed off the "Moog" trademark)* |

⚠️ **Trademark hygiene:** `Moog`, `Korg`, `Teenage Engineering`, `Pocket Operator` are
trademarks. Keep them out of *visible* names (IAP display names especially) — rename presets
for the store. `operator` alone in the hidden keyword field is defensible; `pocket operator`
spelled out on a visible tile is not.

## The one number we're still missing

Difficulty above is a **relative** proxy (crowding × incumbent strength), not absolute search
volume — so "winnable" is a bet until confirmed. The real popularity number comes from
Apple's **Search Term Rank report** (App Store Connect → Analytics → Insights), a phased beta
**not yet on this account** as of 2026-07-03. Re-check periodically; when it lands, feed it in
and the "MEDIUM/HARD" guesses become real popularity scores.

## Re-run the analysis

```
node tools/aso-research.js --country us "pocket operator" "groovebox" "drum machine"
node tools/aso-lint.js --title "…" --subtitle "…" --keywords "a,b,c" --research "x,y"
```
