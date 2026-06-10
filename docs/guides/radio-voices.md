# Radio voices — what instrument plays what part, per station

For each radio station: which voice slots it defines, what **musical role** each plays,
and which **named preset** ([`instrument-presets.md`](instrument-presets.md)) the recipe
is. This is the reading view — small, because it references preset names instead of
repeating `instrument()` blocks. The catalog is the inverse index: "which carts use this
sound."

Together they answer the question this pair was built for: *what are we using in what
radio, and where are we (knowingly or not) reusing the same recipe?*

> **Status: pilot.** Only **italo** is charted. We started here because it's the station
> that prompted the question — it *feels* like a collage of other stations' sounds, and
> charting it tests whether the format makes that borrowing visible. (It does: italo's
> whole drum kit turned out to be `house`'s, lifted verbatim.) Grow one station at a time;
> add each voice to the [preset catalog](instrument-presets.md) as you go.

## How to read a chart

- **slot** — the `#define I_*` / `SL_*` from the cart source.
- **role** — what it plays in the arrangement (comp, bass, lead, perc, solo…).
- **preset** — the catalogued name. A trailing **↗cart** means the recipe is *shared
  (byte-identical)* with that cart; **⟳** means a *variant* (same instrument re-tuned)
  others also use; **≈** means a *cousin* (same recipe skeleton, different character). No
  mark = unique to this station.
- **rolls between** — when a slot lists several presets, the seed picks one *per song*
  (the cart's "nights"). One name per genuine engine choice; pure param tweaks stay folded
  into a single preset's variations table (see `instrument-presets.md` → naming rule).
- **solo layer** — `solo.h` (player jam strip) or `improv.h` (auto-soloist), or none.

---

## italo — Italo disco (Gazebo / Den Harrow)

Solo layer: **none** (neither `solo.h` nor `improv.h`).

The melodic voices are italo-original; the rhythm section is **French house's kit,
copied whole** (kick/clap/hats+choke/crash byte-identical to `house`), minus the maraca,
plus a Simmons tom.

| slot | role | preset | engine |
|---|---|---|---|
| `I_BASS`  | sequencer bass (octave bounce) | `saw/italo-seq-bass` | SAW |
| `I_STAB`  | brass-stab punctuation | `fm/brass-stab` | FM |
| `I_KEYS`  | Rhodes comp bed | `epiano/rhodes-detent` | EPIANO |
| `I_ARP`   | high sequencer arpeggio | `pluck/seq-arp` | PLUCK |
| `I_LEAD`  | soaring chorus topline | `pd/soaring-lead` | PD |
| `I_STR`   | string-machine pump pad | `saw/string-machine` ⟳ (house · motorik) | SAW |
| `SL_KICK` | kick | `drum/french-house-kick` ↗house | SINE |
| `SL_CP`   | handclap | `drum/house-clap` ↗house | NOISE |
| `SL_HATC`/`SL_HATO` | closed/open hats (choke) | `drum/house-hats` ↗house | SQUARE |
| `SL_TOM`  | Simmons electronic tom | `drum/simmons-tom` | SINE |
| `SL_CYM`  | crash | `drum/house-crash` ↗house | SQUARE |

**Borrowing at a glance:** 4 of 6 drum recipes (↗house) are verbatim copies — a *shared*
tier that today is a hand-copy, not a shared symbol, so editing house's kick won't move
italo's. The string pad (⟳) is a re-tuned *variant* shared with house and motorik. Six
melodic + tom voices are italo's own.

---

## house — French house / filter disco (Daft Punk / Stardust / Cassius)

Solo layer: **none**.

The defining trick isn't a voice — it's the **filter ride**: held string-machine voices
swept live with `note_cutoff`/`note_res`, and the stab/bass/lead filters re-aimed *every
frame* (`I_STAB`/`I_BASS`/`I_LEAD`), with a kick-synced sidechain pump. The presets below
are the *static* starting points; the ride is what plays them. This is the **origin** of
italo's borrowed drum kit.

| slot | role | preset | engine |
|---|---|---|---|
| `I_STAB` | chopped-sample stab (filter-ridden) | `saw/house-stab` | SAW |
| `I_BASS` | disco bass (pluck snap) | `tri/disco-bass` ⟳ (citypop?) | TRI |
| `I_LEAD` | Da Funk mono lead (distorted) | `square/da-funk-lead` | SQUARE |
| `I_STR`  | string-machine ride canvas (held) | `saw/string-machine` ⟳ (italo · motorik) | SAW |
| `SL_KICK` | kick | `drum/french-house-kick` ↗italo | SINE |
| `SL_CP`   | handclap | `drum/house-clap` ↗italo | NOISE |
| `SL_HATC`/`SL_HATO` | closed/open hats (choke) | `drum/house-hats` ↗italo | SQUARE |
| `SL_CYM`  | crash | `drum/house-crash` ↗italo | SQUARE |
| `SL_MAR`  | maraca | `drum/house-maraca` | NOISE |

**Borrowing at a glance:** house is the *source* — its drum kit (↗italo) is the one italo
copied verbatim. Its melodic voices are house-original except the string pad (⟳), a
re-tuned variant shared with italo/motorik. The `tri/disco-bass` is flagged as a likely
variant of citypop's bass — to confirm when citypop is charted.

---

## citypop — Japanese pop-funk (Tatsuro Yamashita / Mariya Takeuchi)

Solo layer: **`solo.h`** (a glossy solo-synth jam strip over the changes).

Almost entirely citypop-original — its only catalogued overlap is the **bass**, a
confirmed variant of house's. The comp lead (`square/glossy-lead`) is a clean *cousin* of
house's distorted Da Funk lead, and the jam-strip solo is that same glossy lead opened up.
Its drum kit is its own (punchy synth kit, *not* house's box).

| slot | role | preset | engine |
|---|---|---|---|
| `I_GTR`   | 16th-note funk guitar chucks | `tri/funk-guitar` | TRI |
| `I_BASS`  | octave-pop disco bass | `tri/disco-bass` ⟳ (house) | TRI |
| `I_LEAD`  | glossy chorus comp synth | `square/glossy-lead` ≈ (house da-funk) | SQUARE |
| `I_BRASS` | saw-stack horn section | `saw/brass-section` | SAW |
| `I_KICK`  | punchy synth kick | `drum/synth-kick` ⟳ (motorik) | SINE |
| `I_SNARE` | bright 80s crack | `drum/noise-snare` ⟳ (motorik) | NOISE |
| `I_HAT`   | airy washing hat | `drum/noise-hat` ⟳ (motorik) | NOISE |
| `I_SOLO`  | jam-strip solo (solo.h) | `square/glossy-lead` (solo stop) ≈ | SQUARE |

**Borrowing at a glance:** one ⟳ (bass, shared with house) and one ≈ (the PWM-square lead
trick, voiced clean here vs distorted in house). Everything else is unique. Note the
*intra-cart* reuse too: `I_SOLO` is `I_LEAD` opened up — one voice, two stops.

---

## motorik — Krautrock driver (Neu! × Stereolab)

Solo layer: **`improv.h`** (the topline is a vibraphone *taking a solo* — man-vs-motor).

Built on relentless 4/4: a held Farfisa drone, a maj7 planing string pad, a Moog octave
pulse, and a vibraphone soloist. Two of its voices are reused recipes — the string pad and,
most notably, the **vibraphone**, which is the mallet-cart "vibes" preset propagated through
lowend (see the `mallet/vibes` entry — motorik's own comment mis-credits its source).

| slot | role | preset | engine |
|---|---|---|---|
| `I_ORG`  | Farfisa/Vox drone (held) | `organ/combo-drone` | ORGAN |
| `I_BASS` | Moog octave pulse | `pulse/moog-bass` | SAW/SQUARE |
| `I_PAD`  | maj7 planing pad (held) | `saw/string-machine` ⟳ (italo · house) | SAW |
| `I_SOLO` | vibraphone soloist (improv.h) | `mallet/vibes` ↗lowend ≈ (origin: mallet.c) | MALLET |
| `I_KICK` | four-on-the-floor thud | `drum/synth-kick` ⟳ (citypop) | SINE |
| `I_SNR`  | understated backbeat | `drum/noise-snare` ⟳ (citypop) | NOISE |
| `I_HAT`  | straight-8th tick | `drum/noise-hat` ⟳ (citypop) | NOISE |

**Borrowing at a glance:** the most-borrowed station so far. Its string pad closes the
3-cart `saw/string-machine` lineage; its whole synth drum kit (⟳ citypop) is the variant
family; and its vibraphone is a verbatim copy of mallet.c's preset (↗lowend marks the
byte-identical macros). Only the organ drone and Moog bass are motorik-original.

---

## cocktail — Piano-trio lounge (Vince Guaraldi / Oscar Peterson)

Solo layer: **`improv.h`** (the piano solos over two arcs, then the bass takes a solo on
the *same* brain). The whole cart is built on seed-rolled **"nights"** — several voices
*roll between different instruments* per song, so two playthroughs are two different trios.
Where a slot lists **several presets**, the seed picks one each night.

| slot | role | rolls between (one per night) |
|---|---|---|
| `I_PNO`   | comp + melody piano | `tri/felt-grand` · `sine/closed-lid-piano` |
| `I_PSOLO` | solo stop (improv.h plays here) | `tri/piano-solo-stop` · `mallet/vibes` ≈ · `pluck/archtop` |
| `I_BASS`  | walking upright | `tri/upright-bass` · `sine/gut-bass` |
| drums (`SL_RIDE`/`SL_HAT`/`SL_BRSH`/`SL_KICK`) | jazz kit | `kit/cocktail-brushes` · `kit/cocktail-sticks` |

**Borrowing at a glance:** almost all cocktail-original — its *one* cross-cart link is the
MJQ night of the solo stop, which is `mallet/vibes` (the 5th use of that preset). The rest
of its variety is **internal**: the seed rolling each voice through a different instrument
each night — a different piano, a different solo voice, a different bass, and brushes vs
sticks on the kit. Every one of those choices is a name you can point at.

---

## Stations not yet charted

ambient · addis · bossa · carlos · dub · exotica · gamelan ·
jangle · jingle · lowend · roadhouse · satie · tango · ymo · yacht

(20 stations total; all on `runtime/radio.h`. Charted: italo · house · citypop · motorik ·
cocktail. **lowend** is still referenced-only by `mallet/vibes` — charting it confirms that
cluster. The `solo.h` group — bossa/dub/jangle/jingle — is still untouched.)
