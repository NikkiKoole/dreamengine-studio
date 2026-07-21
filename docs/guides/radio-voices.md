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

## lowend — Jazz-rap boom bap (A Tribe Called Quest / *Low End Theory*)

Solo layer: **none**.

Minimalist by doctrine — bass first, drums, one or two elements. Its melodic voices are
lowend-original, but its **whole rhythm section is the shared synth kit** (kick/snare/hat
= the 3-station `drum/*` variant families), and its lead can opt into the well-travelled
`mallet/vibes`.

| slot | role | preset | engine |
|---|---|---|---|
| `I_RHODES` | tremolo EP stabs | `tri/tremolo-rhodes` | TRI |
| `I_BASS`   | the star — deep upright | `sine/boom-bap-bass` (kin: cocktail `sine/gut-bass`) | SINE |
| `I_LEAD`   | sparse hook | `tri/sparse-hook-lead` *(default)* · `mallet/vibes` *(opt-in)* | TRI / MALLET |
| `I_KICK`   | the "boom" | `drum/synth-kick` ⟳ (citypop · motorik) | SINE |
| `I_SNARE`  | the "bap" | `drum/noise-snare` ⟳ (citypop · motorik) | NOISE |
| `I_HAT`    | lo-fi closed hat | `drum/noise-hat` ⟳ (citypop · motorik) | NOISE |
| `I_VINYL`  | vinyl dust/crackle | `noise/vinyl-dust` | NOISE |

**Borrowing at a glance:** confirms the synth kit as a **3-station** pattern (⟳ citypop ·
motorik) — its kick/snare/hat are those families, boom-bap-tuned. The opt-in vibes is the
5th `mallet/vibes` use (here the shipped default is the TRI hook, not the vibraphone). Only
the tremolo Rhodes, sine bass, and vinyl dust are lowend-original. `I_LEAD` rolls between
two presets, but via a **code flag** (`leadVibes`), not a per-song seed roll.

---

## bossa — Bossa nova (João Gilberto / Jobim)

Solo layer: **`solo.h`** (a present flute the player drives over the changes).

The first station with **zero cross-cart preset reuse** — entirely its own acoustic/latin
world. Nylon guitar, breathy flute, caxixi, cross-stick clave; nothing shared with the
dance stations' synth kit. Its only "roll" is the guitar's fake↔real code toggle, and its
solo flute is the comp flute opened up.

| slot | role | preset | engine |
|---|---|---|---|
| `I_GTR`    | nylon guitar comp | `tri/nylon-fake` *(default)* · `pluck/nylon-guitar` *(opt-in)* | TRI / PLUCK |
| `I_BASS`   | fingered surdo bass | `tri/fingered-bass` (kin: the TRI-bass pile) | TRI |
| `I_FLUTE`  | breathy flute lead | `sine/breathy-flute` | SINE |
| `I_SHAKER` | caxixi 16ths | `noise/caxixi` | NOISE |
| `I_RIM`    | cross-stick clave | `noise/cross-stick` | NOISE |
| `I_SOLO`   | jam flute (solo.h) | `sine/breathy-flute` (solo stop) | SINE |

**Borrowing at a glance:** none cross-cart. bossa confirms a hunch — the acoustic/latin
station doesn't touch the dance-station kit; it builds its own percussion (caxixi,
cross-stick) and front line. The one thing to watch is `tri/fingered-bass` joining the
TRI-bass pile (3 now). The guitar's two voices are a *code* toggle (`gtrPluck`), not a
per-song seed roll.

---

## dub — Roots dub (King Tubby / Augustus Pablo)

Solo layer: **`solo.h`** (a scale-locked melodica the player sings over the riddim, into the
tape echo). The desk *is* the instrument — echo throws and live sends are performance.

Two clusters meet here. dub borrows the **synth kit** (kick + washing hat) and shares the
**cross-stick** with bossa — yet its melodic front (riddim bass, melodica, organ bubble,
siren) is its own, and its skank can be a real **organ.c** preset.

| slot | role | preset | engine |
|---|---|---|---|
| `I_SKANK` | offbeat chop | `tri/skank-chop` · `organ/reggae-chop` *(code choice)* | TRI / ORGAN |
| `I_BASS`  | the riddim (the song) | `sine/riddim-bass` (kin: SINE-bass pile) | SINE |
| `I_MELO`  | melodica (echoed) | `square/melodica` | SQUARE |
| `I_ORG`   | organ bubble | `tri/organ-bubble` (kin: `tri/tremolo-rhodes`) | TRI |
| `I_KICK`  | soft one-drop kick | `drum/synth-kick` ⟳ (citypop · motorik · lowend) | SINE |
| `I_RIM`   | cross-stick crack | `noise/cross-stick` ⟳ (bossa) | NOISE |
| `I_HAT`   | washing hat | `drum/noise-hat` ⟳ (citypop · motorik · lowend) | NOISE |
| `I_SIREN` | meltdown toy (FX) | `square/siren` | SQUARE |
| `I_SOLO`  | jam melodica (solo.h) | `square/melodica` (solo stop) | SQUARE |

**Borrowing at a glance:** dub is a *bridge* — not sealed like bossa, not a clone like the
dance stations. It takes the synth kit (⟳ ×3) and the cross-stick (⟳ bossa), but its
riddim bass, melodica, organ bubble and siren are dub-original (kin-linked to existing
families, not copies). The skank can be a literal **organ.c** preset — the 2nd showcase-cart
borrow after `mallet/vibes`/`organ/combo-drone`.

---

## jangle — Mixolydian slacker pop (Mac DeMarco / One Wayne G)

Solo layer: **`solo.h`** (a scale-locked whistle over the vamp).

A near-textbook **clone**: its whole kit is the shared synth kit, its bass is the SINE pile,
its lead a PWM-square whistle. Only the **chorused jangle guitar** — a constant 5.5 Hz pitch
warble — is its own, and even that uses bossa's fake↔real guitar toggle.

| slot | role | preset | engine |
|---|---|---|---|
| `I_GTR`   | chorused jangle guitar | `tri/jangle-guitar` *(default)* · `pluck/jangle-guitar` *(opt-in)* | TRI / PLUCK |
| `I_BASS`  | round simple bass | `sine/round-bass` (kin: SINE-bass pile) | SINE |
| `I_LEAD`  | thin slidey whistle | `square/whistle` (kin: PWM-square family) | SQUARE |
| `I_KICK`  | CR-78-ish thud | `drum/synth-kick` ⟳ (×4 others) | SINE |
| `I_SNARE` | boxy little snare | `drum/noise-snare` ⟳ (citypop · motorik · lowend) | NOISE |
| `I_HAT`   | tight hat | `drum/noise-hat` ⟳ (×4 others) | NOISE |
| `I_SOLO`  | jam whistle (solo.h) | `square/whistle` (solo stop) | SQUARE |

**Borrowing at a glance:** the most-borrowing station so far — kick & hat are now **5-station**
families, snare 4. Its bass and whistle are kin to the SINE-bass and PWM-square piles. The
chorused guitar (warble = the identity) is jangle's one original, built on the same fake↔real
toggle bossa uses. If anything is a clone of the dance-station template, it's jangle.

---

## jingle — Mac DeMarco songwriter, the delicate side (dusk sibling of jangle)

Solo layer: **`solo.h`** (a scale-locked lead over the progression).

The most-borrowing station of all — almost every voice joins an existing family. It's
*jangle re-tuned softer*: its guitar and bass are variants of jangle's, its kick/hat are the
6-station synth kit, and it uses a cross-stick (like bossa/dub) instead of a snare. Only its
soft singing lead is near-original.

| slot | role | preset | engine |
|---|---|---|---|
| `I_GTR`  | fingerpicked chorused guitar | `tri/jangle-guitar` ⟳ (jangle) | TRI |
| `I_BASS` | round simple bass | `sine/round-bass` ⟳ (jangle) | SINE |
| `I_MEL`  | soft singing lead | `sine/singing-lead` (kin: bossa `sine/breathy-flute`) | SINE |
| `I_KICK` | soft kick | `drum/synth-kick` ⟳ (×5 others) | SINE |
| `I_RIM`  | cross-stick | `noise/cross-stick` ⟳ (bossa · dub) | NOISE |
| `I_HAT`  | tight hat | `drum/noise-hat` ⟳ (×5 others) | NOISE |
| `I_SOLO` | jam lead (solo.h) | `sine/singing-lead` (solo stop) | SINE |

**Borrowing at a glance:** jingle owns almost nothing — guitar + bass are the jangle
songwriter-pair variants, the kit is the 6-station family, the rim is the latin/folk
cross-stick. Only `sine/singing-lead` is its own (and kin to bossa's flute).

---

## The `solo.h` cluster (now complete: bossa · dub · jangle · jingle · citypop)

The five player-jam stations don't form one shared palette — they split by **idiom**:
- **songwriter pair** jangle ↔ jingle share guitar + bass (`tri/jangle-guitar`, `sine/round-bass`).
- **latin/folk percussion** bossa ↔ dub ↔ jingle share `noise/cross-stick`.
- the **synth kit** cuts across citypop/dub/jangle/jingle (and the non-solo motorik/lowend).
- bossa stays most island-like; citypop sits with the dance clones.

So reuse tracks *genre adjacency*, not the `solo.h` membership itself.

---

## addis — Ethio-jazz (Mulatu Astatke)

Solo layer: **`improv.h`** (the vibraphone takes the solo).

The first **real-engine band** — five modelled engines, almost all new families. It does
**not** use the synth kit: its drums are real `INSTR_MEMBRANE` (kebero/conga/bongo). Its one
reuse is the shaker. Several slots roll between voicings/engines per song.

| slot | role | preset | engine |
|---|---|---|---|
| `I_VIBE`  | vibraphone lead + solo | `mallet/addis-vibes` (kin: `mallet/vibes`) | MALLET |
| `I_KEYS`  | Wurli/organ comp | `epiano/wurli-comp` · `organ/addis-comp` *(rolls)* | EPIANO / ORGAN |
| `I_HORN`  | synth-brass horn line | `pd/synth-horn` (kin: `pd/soaring-lead`) | PD |
| `I_BASS`  | ostinato bass | `fm/ostinato-bass` | FM |
| `SL_KEB`  | kebero / low drum | `membrane/kebero` | MEMBRANE |
| `SL_CONGA`| open conga | `membrane/conga` | MEMBRANE |
| `SL_BONGO`| bongo / slap | `membrane/bongo` | MEMBRANE |
| `SL_SHAK` | shaker 8ths | `noise/caxixi` ⟳ (bossa) | NOISE |

**Borrowing at a glance:** almost none — addis is a real-engine **island**. New families on
five engines (MALLET/EPIANO/ORGAN/PD/FM/MEMBRANE); the only ⟳ is the shaker (with bossa).
Crucially it voices its *own* vibraphone rather than copying `mallet/vibes` — the vibe cart
that didn't lift the shared recipe. Confirms the hunch: **the synth kit is a property of the
synthetic/dance idiom; real-engine stations build their own.**

---

## yacht — Yacht rock / AOR (Steely Dan)

Solo layer: **none** (a seeded session band, not a player jam).

A **gear-changing session band**: most slots seed-roll between *voicings* of a fixed part —
its "gear change." It shares nothing byte-identical, but is **kin to half the catalog**
(EPIANO/FM Rhodes, PLUCK guitars, PWM-square sax, SAW strings, the synth kit). Voicings are
listed in each preset's catalog entry.

| slot | role | preset | engine(s) |
|---|---|---|---|
| `I_EP`   | FM electric piano (center) | `fm/rhodes` (voicings: tremolo depth · PLUCK clav) | FM / PLUCK |
| `I_BASS` | fingered electric bass | `bass/yacht-fingered` (one part, TRI·SINE·SAW) | TRI/SINE/SAW |
| `I_GTR`  | clean Strat 9th-stabs | `pluck/strat-stab` | PLUCK |
| `I_SAX`  | chorus sax lead | `square/sax` (voicings: sax · synth · gtr solo) | SQUARE / PLUCK |
| `I_PAD`  | strings / syn-brass | `saw/yacht-strings` | SAW |
| `SL_*`   | studio kit (2 nights) | `kit/yacht-studio` | SINE / NOISE / SQUARE |

**Borrowing at a glance:** zero copies, all kin. yacht is a real/session-engine **island**
like addis, but where addis built brand-new families, yacht's voices are *kin* to existing
ones (its Rhodes is FM where italo's is EPIANO; its strat joins the PLUCK guitars; its sax
the PWM-squares; its studio kit the synth kit, produced). And the **gear-change** pattern —
seed-rolling voicings of one part (the bass on three oscillators) — is new here.

---

## roadhouse — Modal psych-rock (The Doors)

Solo layer: **`improv.h`** (organ solo, then guitar solo — improv.h was *born here*).

A jamming band: two keyboards played by one man, a guitar with a held drone, a live kit.
Mostly its own/kin voices, but with two notable points — the **first `INSTR_USER0` drawn-wave
organ**, and a bass that shares cocktail's session upright.

| slot | role | preset | engine |
|---|---|---|---|
| `I_ORG`   | combo organ (+ solo stop `I_ORGL`) | `user0/combo-organ` | USER0 (drawn) |
| `I_PBASS` | piano-bass left hand | `fm/rhodes-bass` · `tri/upright-bass` ⟳ (cocktail) *(rolls)* | FM / TRI |
| `I_GTR`   | Krieger guitar | `pluck/krieger-guitar` (clean/fuzz/flatpick voicings) | PLUCK |
| `I_DRONE` | open-string pedal | `pluck/drone` | PLUCK |
| `SL_*`    | live rock kit + tambourine | `kit/roadhouse-live` | SINE/NOISE/SQUARE |

**Borrowing at a glance:** real-engine band, mostly own/kin — but two real links: its
session-bassist bass night **is** cocktail's `tri/upright-bass` (near-byte-identical), and
its kit is kin to yacht's studio kit. The standout is `user0/combo-organ` — the first station
to draw its own drawbar wave via `wave_set`, a 4th distinct route to "organ" (after the three
`INSTR_ORGAN` registrations).

---

## satie — Solo piano gymnopédies (Erik Satie)

Solo layer: **none**.

The most minimal station — **one instrument**, solo piano, two hands. No kit, no bass.

| slot | role | preset | engine |
|---|---|---|---|
| `I_PNO`  | right hand (melody + chords) | `tri/felt-piano` | TRI |
| `I_PNOB` | left hand (longer, rounder) | `tri/felt-piano` (left voicing) | TRI |

**Borrowing at a glance:** nothing — its own felt piano. But notable for what it *doesn't*
use: like every charted station, it **fakes the piano on TRI** rather than the modeled
`INSTR_PIANO`. Kin to cocktail's `tri/felt-grand`; satie's is the most sustained voicing,
shaped for held gymnopédies. A minimalist island.

---

## gamelan — Indonesian gamelan (Java / Bali)

Solo layer: **none**.

The biggest new sonic world — and the first station on **microtonal tuning**. Tuned bronze,
kettle-gongs, hand drums, and a bamboo flute; nothing borrowed.

| slot | role | preset | engine |
|---|---|---|---|
| `I_BRONZE`+0..6 | tuned bronze bank (per scale degree) | `mallet/bronze` | MALLET |
| `I_GONG`/`I_GONG2` | great gong (ombak detuned pair) | `mallet/gong` | MALLET |
| `I_KENL`/`I_KENW` | kendang hand-drum pair | `membrane/kendang` | MEMBRANE |
| `I_SUL` | suling bamboo flute | `reed/suling` | REED |

**Borrowing at a glance:** none — a microtonal island, all kin (MALLET bronze ↔ the vibes;
MEMBRANE kendang ↔ addis's drums). But it introduces **three firsts**: `instrument_tune()`
microtonal scale banks (sléndro/pélog), the **ombak** detune-for-beating shimmer, and the
first `INSTR_REED` voice. The most idiomatic station yet — reuse is purely engine-level
family resemblance, nothing copied.

---

## tango — Golden-age orquesta típica (D'Arienzo / Pugliese / Troilo)

Solo layer: **none** (a per-frame rubato conductor drives the time instead).

Despite being orchestral, tango is a **bridge** — it shares the *acoustic* palette (fake
pianos, the session upright) with cocktail/roadhouse, while building its own bandoneón,
violins and tango percussion.

| slot | role | preset | engine |
|---|---|---|---|
| `I_BAND`/`I_BANDL` | bandoneón (2 hands) | `user0/bandoneon` | USER0 (drawn) |
| `I_VLN`  | violins, one desk | `saw/violins-arco` · `saw/violins-pizz` *(rolls by section)* | SAW |
| `I_PNO`  | piano (marcato/cantabile) | `tri/felt-grand` ⟳ · `sine/closed-lid-piano` ⟳ (cocktail) *(rolls)* | TRI / SINE |
| `I_BASS` | marcato upright | `tri/upright-bass` ⟳ (cocktail · roadhouse) | TRI |
| `SL_CHIC`| chicharra scratch | `noise/chicharra` | NOISE |
| `SL_GLP` | golpe (box knock) | `noise/golpe` | NOISE |

**Borrowing at a glance:** the surprise — an orchestral station that shares plenty, but with
the **acoustic cluster**, not the dance one. Its felt/closed pianos are cocktail's fake pianos;
its upright is the 3-station session bass; its bandoneón is the 2nd `INSTR_USER0` drawn-wave
voice (kin roadhouse). Only the violins (arco/pizz) and the chicharra/golpe percussion are
tango's own.

---

## carlos — Switched-On Bach (Wendy Carlos)

Solo layer: **none** (a two-voice species-counterpoint generator drives it).

Pure Moog — three `INSTR_SAW` voices, no drums, no chord table. A **synthetic island**: it
uses synths but shares nothing with the dance cluster — its kinship is to the dream-synth
cart, not the groove stations.

| slot | role | preset | engine |
|---|---|---|---|
| `I_UP`  | upper Moog voice (brighter) | `saw/fat-moog` (upper) | SAW |
| `I_LO`  | lower Moog voice (fatter) | `saw/fat-moog` (lower) | SAW |
| `I_PED` | sustained tonic pedal | `saw/moog-pedal` | SAW |

**Borrowing at a glance:** none copied — its fat-Moog patch *is* the `moog.c` signal path
(another showcase-cart lineage), and the pedal is kin to the SAW string pad. Useful data
point: **"synthetic" ≠ "dance cluster."** carlos is all synths but its own island, because the
dance cluster is about the *kit-driven groove idiom*, not the oscillator choice.

---

## exotica — Exotica lounge (Martin Denny / Les Baxter)

Solo layer: **none** (the band "answers the jungle" with aleatoric bird calls).

The most-connected station — a lounge **bridge** that touches a remarkable number of
families, mostly via the acoustic cluster + showcase-cart presets.

| slot | role | preset | engine |
|---|---|---|---|
| `I_VIBE`  | vibraphone lead | `mallet/exotica-vibes` (rolls to `tri/felt-grand` ⟳) | MALLET / TRI |
| `I_GTR`   | nylon rhythm comp | `pluck/exotica-nylon` | PLUCK |
| `I_BELL`  | glass sparkle | `fm/glass-bell` · `mallet/celesta` *(rolls)* | FM / MALLET |
| `I_BASS`  | two-feel upright | `tri/upright-bass` ⟳ (cocktail · roadhouse · tango) | TRI |
| `SL_RIM`  | clave tick | `noise/cross-stick` ⟳ (×3) | NOISE |
| `SL_SHAK` | shaker | `noise/caxixi` ⟳ (bossa · addis) | NOISE |
| `SL_CONGA`| boo-bam | `sine/boo-bam` | SINE |
| `SL_FCYM` | finger cymbal | `fm/finger-cymbal` | FM |
| `I_CHIRP`/`I_SWOOP`/`I_FROG` | the aviary (FX) | `fx/aviary` | SINE/SQUARE |

**Borrowing at a glance:** the densest web of links yet — its felt-piano roll is cocktail's
verbatim, its upright the 4-station session bass, its clave + shaker the acoustic-percussion
families, and its celesta the **4th showcase-cart lineage** (mallet.c's celesta, after the
vibes/organ/moog borrows). Its own: the motored vibes, glass bell, boo-bam, finger cymbal,
and the fully-aleatoric aviary. A lounge station sits squarely in the acoustic cluster.

---

## ymo — Techno-kayō (Yellow Magic Orchestra)

Solo layer: **none**.

The synthetic bookend — "square waves and white noise *are* the original instruments" (no
imitation). Its kit is the **CR-78 lifted whole from `cr78.c`**; its lead joins the big
PWM-square family.

| slot | role | preset | engine |
|---|---|---|---|
| `I_BASS` | Hosono counterpoint bass | `square/hosono-bass` | SQUARE |
| `I_ARP`  | sequencer "conveyor belt" | `tri/sequencer` | TRI |
| `I_LEAD` | yonanuki lead | `square/ymo-lead` (kin: PWM-square family) | SQUARE |
| `SL_*`   | CR-78 kit (kick, 2-layer snare, hats, metal, claves) | `kit/cr78` | SINE/NOISE/SQUARE/TRI |

**Borrowing at a glance:** synthetic and dance-adjacent, but it draws from `cr78.c` (the
**5th showcase-cart lineage**, and the first *whole-kit* borrow) rather than the generic synth
kit. Its lead is kin to the seven-strong PWM-square family; its SQUARE bass adds a new
oscillator to the bass piles. A clean bookend: we opened on the dance clones, close on a
synthetic station that builds from a machine recreation.

---

## ambient — Beatless ambient drift

Solo layer: **none** (beatless — no kit, no soloist).

The 20th and quietest — modal drift, held layers, **no beat at all**. Four sustained voices;
the closest the family comes to pure texture.

| slot | role | preset | engine |
|---|---|---|---|
| `I_PAD`  | filtered saw chord (4 held voices) | `saw/ambient-pad` (kin: `saw/string-machine`) | SAW |
| `I_SUB`  | sine sub-bass root (felt, not heard) | `sine/sub-drone` | SINE |
| `I_BELL` | glassy sine bell (sparse arps) | `sine/bell` | SINE |
| `I_WIND` | band-passed noise "weather" | `noise/wind` | NOISE |

**Borrowing at a glance:** none copied — a **drone island**. All four voices are held/sustained
(the only beatless station), so it shares nothing with the kit-driven clusters; its pad is kin
to the SAW string-machine pile, the rest is its own texture. The quiet end of the whole family.

---

## eno — "Music for Airports" (Brian Eno, 1978)

Solo layer: **none** (beatless — no kit, no soloist; ambient's quieter cousin).

The 2nd beatless station, and the first with **no chord brain at all**. Each voice is a LOOP of
its own length in *seconds*, the lengths mutually coprime, so they drift in and out of phase and
never line up the same way twice; each loop just HOLDS one note of a rolled in-mode pitch-set, so
the harmony is **emergent** — whatever loops coincide. The seed rolls one of four SETUPS (the
album's four pieces), which is the variety lever — every voice below is conditional on the setup.

| slot | role | preset | engine |
|---|---|---|---|
| `I_PIANO` | struck piano, rings down on its own (1/1, duet) — **first radio `INSTR_PIANO`** | `piano/airports` (grand→celesta voicing roll) | PIANO |
| `I_VOICE` | sustained sung-vowel choir (2/1, duet) — **first sustained radio `INSTR_VOICE`** | `voice/choir-swell` (A2400 R1400, vowel roll, `voice_nasal` 0.1) | VOICE |
| `I_PAD`   | warm saw drone / bed (synth, + faint bed under 1/1) | `saw/airports-pad` (kin: `saw/ambient-pad`) | SAW |
| `I_PAD2`  | morphing PD pad (the 2/2 drone partner) | `pd/airports-drift` (morph roll) | PD |

Per-voice **ombak** detune (`note_pitch` float) makes paired loops beat; held voices get a slow
pitch-LFO (tape wow) + cutoff-LFO (breathing). Bed: `reverb(0.92, 0.35)` big bloom + high
per-note sends.

**Borrowing at a glance:** none copied — a **second drone island**, kin to ambient only in being
beatless and holding voices. Its pad is kin to ambient's SAW pad; everything else (real piano,
the choir, the coprime-loop form brain) is new to the dial. Blind brief +
palette shop: [`../design/eno-blind-brief.md`](../design/eno-blind-brief.md).

---

## plantasia — "Music for Plants" (Mort Garson, 1976)

Solo layer: **none** — but for a new reason: the LEAD is the protagonist (the first
melody-forward station), so there's no separate solo strip; the whole song *is* the tune.

The band is the **dream-synth (`moog.c`) signal path four ways** — `INSTR_SAW` →
`FILTER_LADDER` + per-note `ENV_CUTOFF` "wow" + `DRIVE`, one slot per role (the `carlos.c`
loan, generalized). The headline is **the songwriter** (melody brain #3): a *seeded*
theme-and-variation hook, not a solo.

| slot | role | preset | engine |
|---|---|---|---|
| `I_LEAD` | the gliding mono Moog lead (held, portamento + vibrato) | `saw/moog-lead` (moog.c LEAD: LADDER 1600/4, drive .42, fenv wow, glide, `instrument_tune` .05) | SAW |
| `I_BASS` | springy filter-pluck bass | `saw/moog-bass` (moog.c BASS: LADDER 540/6, snappy `ENV_CUTOFF` 2/150/1700) | SAW |
| `I_ARP`  | burbling sequencer line | `saw/moog-burble` (resonant LADDER 1200/7, tight env) | SAW |
| `I_PAD`  | warm chord bed (slow filter swell) | `saw/moog-bed` (moog.c BRASS-ish: A60 LADDER 820/3) | SAW |
| `I_BELL` | celesta sparkle | `mallet/celesta` (h0.85 t0.40 m0.50) | MALLET |
| kit | light synth woodblock / soft kick / hat (per feel) | `sine/wood-kick` · `noise/soft-hat` | SINE/NOISE |

**Borrowing at a glance:** the Moog path is `carlos.c`'s loan, here spread across four roles
(carlos used it for two counterpoint voices). The MALLET celesta is kin to `cocktail`'s vibes
chair. Otherwise its identity — a seeded *tune* developed over a songform, the five track-feels,
the growing-plant face — is new. Blind brief + palette shop:
[`../design/plantasia-blind-brief.md`](../design/plantasia-blind-brief.md).

---

## squarepusher — bass-virtuoso drill'n'bass (Tom Jenkinson)

Solo layer: **the bass IS the soloist** — `improv.h` drives it in the SOLO sections (the
first station to put the improviser in the *bass* chair as the lead, not a sub).

The sibling of `braindance` but the BASS is the protagonist, over **moving** jazz-fusion
changes (not braindance's frozen pool). Reuses braindance's rhythm brains + ymo's Hosono
walker + cocktail's harmony tables; the new work is bass-as-lead + slap + the mode-lurch.

| slot | role | preset | engine |
|---|---|---|---|
| `I_BASS` | the lead/slap fusion bass (Hosono walker + improv solos) | `saw/slap-bass` (LADDER 700·sel/6, fast `ENV_CUTOFF` 2/130/1900, drive; sel1 = fuzz) | SAW/SQUARE |
| `I_SLAP` | the slap-pop transient (on accents) | `noise/slap-pop` (NOISE BP2600, 22ms) | NOISE |
| `I_RHODES` | fusion comping (extended voicings) | `epiano/fusion-rhodes` (h0/0.5 Rhodes/Wurli) | EPIANO |
| `I_PAD` | lush string bed (the tender interlude) | `saw/string-pad` (A220 LP1700, `instrument_tune` .05) | SAW |
| `I_ACID` | 303 squelch (manic/solo) | `saw/acid-303` (LP900/12, `ENV_CUTOFF` 0/110/2300, drive) | SAW |
| kit | amen/808 break kit + shred tom | (braindance's `kit/amen` / `808`) | SINE/NOISE |

**Borrowing at a glance:** the most reuse-dense station yet — braindance's `ratchet()` + drum
grammar, ymo's Hosono bass walker, cocktail's jazz harmony tables + `improv.h` bass solo. Its
identity is the **bass-as-lead** brain (Hosono cranked to virtuoso + slap transient) + the
**tender↔manic lurch** form. Blind brief + palette shop:
[`../design/squarepusher-blind-brief.md`](../design/squarepusher-blind-brief.md).

---

## plaid — warm melodic IDM (Andy Turner & Ed Handley)

Solo layer: **none** — the melody is the *weave*, not a soloist. The GENTLE pole of the IDM
wing (the anti-`braindance`/`squarepusher`): intricate but tender, never aggressive.

The headline is **THE INTERLOCKING ARPS** — 3–4 cells, each cycling at its own length
(5/7/8/9/11 steps) against the bar, weaving an emergent tune; all in one lush mode so every
overlap is consonant. Over a slow modal-root drift, in a rolled odd meter, on the `radio.h` clock.

| slot | role | preset | engine |
|---|---|---|---|
| `I_ARP1` | the glassy bell arp (anchor) | `fm/glass-bell` (h0.62 t0.55 m0.12) | FM |
| `I_ARP2` | the marimba/celesta arp | `mallet/marimba` (h0.35 t0.5 m0.3) | MALLET |
| `I_ARP3` | the soft pluck arp | `pluck/soft` (h0.5 LP2400) | PLUCK |
| `I_ARP4` | the high FM sparkle arp | `fm/sparkle` (h0.7 t0.65) | FM |
| `I_PAD`  | lush detuned chord bed (bittersweet maj7/9) | `saw/lush-pad` (A240 LP1500, `instrument_tune` .05) | SAW |
| `I_BASS` | round gentle bass (follows the modal drift) | `sine/round-bass` (LP600) | SINE |
| kit | soft broken-beat (kick/rim/hat/woodblock) | `kit/soft-broken` | SINE/NOISE |

**Borrowing at a glance:** reuses the bell/mallet/pluck/FM timbres and the metered `radio.h`
chassis (the IDM-wing wiring). Its identity is the **interlocking-arp brain** (different cycle
lengths → emergent melody, the melodic cousin of gamelan's kotekan + eno's coprime loops) and
**odd meters that flow**. Blind brief + palette shop:
[`../design/plaid-blind-brief.md`](../design/plaid-blind-brief.md).

---

## afrobeat — Afrobeat (Fela Kuti & Africa 70 / Tony Allen)

Solo layer: **`improv.h`** (the tenor sax takes the solo over the vamp).

The station that finally reaches the **untapped engine shelves** — three radio firsts at
once: `INSTR_GUITAR` (the two interlocking guitars), `INSTR_BRASS` (the trumpet), and the
horn **section** proper (`REED` sax + `BRASS` trumpet voiced a 3rd apart, panned wide to fake
the ensemble spread). Built on `euclid()` polyrhythm + a held modal vamp.

| slot | role | preset | engine |
|---|---|---|---|
| `I_GTR1` | muted "tenor" interlock guitar (panned L) | `guitar/afro-tenor` *(first radio GUITAR)* | GUITAR |
| `I_GTR2` | brighter rhythm guitar (panned R) | `guitar/afro-rhythm` | GUITAR |
| `I_BASS` | syncopated upright ostinato (was a farty FM electric — re-voiced) | `tri/upright-bass` (kin: cocktail) | TRI |
| `I_ORG`  | combo-organ comp | `organ/afro-comp` (kin: `organ/jimmy`) · `epiano` (chair) | ORGAN / EPIANO |
| `I_SAX`  | horn-section top + the solo (improv.h) | `reed/afro-tenor-sax` (kin: reed.c `tenor_sax`) | REED |
| `I_TPT`  | horn-section trumpet + stabs | `brass/afro-trumpet` *(first radio BRASS)* | BRASS |
| `SL_KICK`| syncopated kick | `sine/afro-kick` | SINE |
| `SL_SNR` | backbeat + ghost notes (Tony Allen feel) | `noise/afro-snare` | NOISE |
| `SL_CONGA`| open conga | `membrane/conga` ↗addis | MEMBRANE |
| `SL_SHK` | shekere 16ths | `noise/afro-shekere` (kin: `noise/caxixi`) | NOISE |
| `SL_BELL`| gankogui timeline (euclid 7-in-16, hi/lo) | `mallet/gankogui-lpg` (soft struck bar + a low-pass GATE; was a harsh FM bell) | MALLET |

**Borrowing at a glance:** almost all afrobeat-original, because most of its band lives on
engines nothing else uses. Its **one verbatim copy** is the conga (↗addis — now a *shared*
2-station entry). The sax is the 2nd radio `INSTR_REED` (after gamelan's suling); the guitars
and trumpet are the **first** of their engines on the dial. The synth-kit idiom is deliberately
*not* borrowed — it builds a bespoke broken-funk kick/snare. The genre's effects-blocked wants
(wah guitar, room reverb, section chorus, Leslie) are catalogued in
[`../design/afrobeat-effects-wants.md`](../design/afrobeat-effects-wants.md).

---

## air — AIR / Moon Safari (an ARTIST station, 5 song archetypes)

Solo layer: **`solo.h`** (the jam strip — play along on the song's scale).

Not a genre but an *artist*: the seed rolls one of **five song archetypes**, each a cited AIR
track (Sexy Boy · La Femme d'Argent · Playground Love · Cherry Blossom Girl · Kelly Watch the
Stars). The archetype fixes the **lead voice, groove, tempo, mood and form** — so `I_BASS` and
`I_LEAD` are *reconfigured per song* (the engine itself changes). Three radio firsts for AIR's
real lineup: `INSTR_VOICE` as a melodic **vocoder lead**, `INSTR_PIPE` flute, and the Solina
string-machine wash on a detuned-SAW pair. Everything sits in the **echo bus** (a reverb
stand-in — AIR is drenched).

| slot | role | preset | engine |
|---|---|---|---|
| `I_PAD`  | Solina string-machine wash (detuned saw + per-part `instrument_chorus` = the ENSEMBLE) | `saw/solina-ensemble` — kin `solina.c` (the canonical Solina showcase); ≈ italo/house/motorik `saw/string-machine` | SAW |
| `I_EP`   | Rhodes/Wurli comp | `epiano/air-rhodes` · `epiano/air-wurli` (chair) | EPIANO |
| `I_GTR`  | fingerpicked nylon (Cherry) | `guitar/air-nylon` (kin: afrobeat guitars, guitar.c `nylon`) | GUITAR |
| `I_BASS` | the bass — **rolls by archetype** | `saw/air-fuzz-bass` (Sexy/Kelly, +drive) · `tri/air-round-bass` (Argent rolling / Playground / Cherry) | SAW / TRI |
| `I_LEAD` | the star voice — **rolls by archetype** | `pd/air-moog-lead` (Sexy/Argent) · `reed/air-tenor-sax` (Playground; kin reed.c `tenor_sax`) · `pipe/air-flute` *(first radio PIPE)* · `voice/air-vocoder` *(first radio VOICE lead)* | PD / REED / PIPE / VOICE |
| `I_VIBE` | high twinkle on the hook | `mallet/vibes` ⟳ (lowend · motorik · cocktail · exotica; origin mallet.c) | MALLET |
| `I_KICK` | soft sine thump | `sine/air-kick` | SINE |
| `I_SNR`  | clap / brushed snare on 2&4 | `noise/air-clap` | NOISE |
| `I_HAT`  | off-beat hat / brush sweep | `noise/air-hat` | NOISE |
| `I_SOLO` | jam-strip lead (solo.h) | `sine/air-jam` | SINE |

**Borrowing at a glance:** mostly air-original because the lead lineup lives on engines barely
used elsewhere — `voice/air-vocoder` is the **first radio melodic use of `INSTR_VOICE`**, and
`pipe/air-flute` the **first radio `INSTR_PIPE`**. Reuses on purpose: `mallet/vibes` (the shared
struck-bar, ⟳) and the Solina pad as a *cousin* (≈) of the `saw/string-machine` pad three dance
stations share. The genre's effects-blocked wants (reverb, chorus, true vocoder, tape) — plus an
`INSTR_PIPE` intonation note — are catalogued in
[`../design/air-effects-wants.md`](../design/air-effects-wants.md).

## wba — The Whitest Boy Alive (an ARTIST station, 5 song archetypes)

Solo layer: **none** — the texture is the guitar↔bass DUET, not a soloist. The whole identity
is RESTRAINT and SPACE; the first station to use the **`ampcab.h` guitar amp/cab** + a clean-guitar
pedalboard. Five archetypes (Burning / Golden Cage / 1517 / Courage / Done With You).

| slot | role | preset | engine |
|---|---|---|---|
| `I_GTR`  | clean electric guitar (palm-mute chops) — **first station on the CLEAN amp/cab** | `guitar/steel` (h0.55 m0.38) → `ampcab` **CLEAN** (DRIVE_SOFT, scooped-bright, light sag) + slapback `instrument_echo .12` + `instrument_chorus .20` | GUITAR |
| `I_BASS` | the melodic co-lead bass (round, dry, prominent) | `tri/round-bass` (LP 560–720·arch) | TRI |
| `I_EP`   | warm Rhodes comp | `epiano/rhodes` + `instrument_chorus(0.7,0.28,0.26)` (Rhodes width, ⟳ air) | EPIANO |
| `I_LEAD` | soft vocal topline | `voice/soft-tenor` (O→A vowel, soft effort, low nasal) | VOICE |
| `I_ORG`  | a little analog synth/organ stab | `saw/organ-stab` (`instrument_tune` .04) | SAW |
| kit | tight clean machine kit (kick/clap/hat) | `sine/clean-kick` · `noise/clean-snare` · `noise/tight-hat` | SINE/NOISE |

Bus: `reverb(0.35,0.5)` small warm room (⟳) + `tape(0.10,0.07,0.22)` gentle glue + `glue(0,0.20)`
knitting the dry band. **No distortion** — CLEAN amp only; the restraint is the sound.

**Borrowing at a glance:** an artist-station cousin of `air`/`napoleon` (the archetype roll). Reuses
the Rhodes-width chorus (≈ air), the slapback echo, the small-warm-room verb. Its new ground: the
**clean `INSTR_GUITAR` through `ampcab.h`** (no station had used the amp/cab) and the **negative-space
guitar↔bass duet**. Blind brief: [`../design/wba-blind-brief.md`](../design/wba-blind-brief.md).

## thexx — The xx (an ARTIST station, 5 song archetypes) — the dark twin of wba

Solo layer: **none** — the structure IS the two voices trading + the silence between. The most
minimal station: dark-minor, nocturnal, reverb-and-space. Five archetypes (Crystalised / Islands
/ VCR / Angels / On Hold).

| slot | role | preset | engine |
|---|---|---|---|
| `I_VOXA` | Oliver — low male voice (one half of the trade) | `voice/oliver` (dark vowel h0.30, large tract t0.72, soft) | VOICE |
| `I_VOXB` | Romy — higher female voice (the other half) | `voice/romy` (A vowel h0.55, small tract t0.38, breathy) | VOICE |
| `I_GTR`  | icy single-note guitar, drenched | `guitar/icy` (clean steel + echo .22 + big reverb) | GUITAR |
| `I_SUB`  | deep dub sub bass (the anchor) | `sine/dub-sub` (SINE LP500) | SINE |
| `I_PAN`  | steel-pan / marimba dabs (Jamie) | `mallet/steelpan` (h0.55 t0.40) | MALLET |
| kit | pointillistic clicky kick + snap/clap + a tick | `sine/click-kick` · `noise/snap` | SINE/NOISE |

**THE NEW BRAIN — the boy/girl CALL-AND-RESPONSE:** the two `INSTR_VOICE`s **trade by phrase**
(A states, B answers) and meet in unison on the chorus; who-sings-when + the silence is the
structure. Plus **space-as-structure** (low density, reverb + rests). Big nocturnal `reverb(0.82,
0.30)`. Blind brief: [`../design/thexx-blind-brief.md`](../design/thexx-blind-brief.md).

## vapor — vaporwave (the first EFFECTS-FORWARD station)

Solo layer: **none** — the PROCESSING is the band. A generative lounge loop, *drowned*.

| slot | role | preset | engine |
|---|---|---|---|
| `I_EP`   | lounge Rhodes comp | `epiano/rhodes` + per-part chorus | EPIANO |
| `I_PAD`  | lush wash bed | `saw/vapor-pad` (`instrument_tune` .06) | SAW |
| `I_LEAD` | smooth sax / soft lead dab | `reed/sax` (chair → soft sine) | REED |
| `I_BELL` | mall-PA chime sparkle | `mallet/chime` | MALLET |
| `I_BASS` | smooth slow bass | `sine/round-bass` | SINE |
| `I_VINYL`| held vinyl-crackle / tape-hiss bed | `noise/vinyl-hiss` (BP3200, held) | NOISE |
| kit (opt)| slow gated reverbed kick/snare/hat | (mallsoft = none) | SINE/NOISE |

**THE DRENCH (set-and-hold):** `reverb` big + `chorus` + `tape` heavy wow/flutter + `echo` +
light `crush` — configured per song/mode, never per frame. **THE WOBBLE (live):** `varispeed`
ridden each frame, the slowed-tape sag. Sub-styles: classic / mallsoft / utopian / future-funk.
Blind brief: [`../design/vaporwave-blind-brief.md`](../design/vaporwave-blind-brief.md).

## lofi — lo-fi jazzy hip-hop (Nujabes / J Dilla)

Solo layer: **none**. The lo-fi/Dilla pole — dreamier than `lowend`'s boom-bap. The headline
is **THE DRUNK POCKET** (a dialable off-grid feel), the loose pocket `lowend` undersold.

| slot | role | preset | engine |
|---|---|---|---|
| `I_EP`   | lush Rhodes jazz comp | `epiano/rhodes` + chorus | EPIANO |
| `I_BASS` | round / upright bass, behind | `tri/upright-bass` (chair → round sine) | TRI |
| `I_DAB`  | muted-horn / vibe sampled dab | `reed/muted-horn` (chair → `mallet/vibe`) | REED |
| kit | dusty boom kick · fat dark snare · swung hat | `sine/boom-kick` · `noise/fat-snare` | SINE/NOISE |
| `I_VINYL`| held vinyl-crackle / hiss bed | `noise/vinyl-hiss` | NOISE |

**THE DRUNK POCKET:** per-voice `schedule_hit` dly offsets — the snare drags LATE, the kick
lazy, the hats swung, + a seeded humanize wobble — all scaled by the **pocket** knob (tight →
drunk, default moderate). The lo-fi rack (tape + echo + reverb, set-and-hold) is reused from
`vapor`. Blind brief: [`../design/lofi-blind-brief.md`](../design/lofi-blind-brief.md).

## napoleon — *Napoleon Dynamite* (an ARTIST station, 5 song archetypes)

Solo layer: **`solo.h`** — and the jam ribbon's *voice + behavior change per archetype* (the
player gets the instrument that fits the song they're jamming over).

Not a genre but the film's *musical world*: the seed rolls one of **five song archetypes**,
each a cited piece of the soundtrack/score (the dance "Canned Heat" · Kip's serenade "Always &
Forever" · John Swihart's deadpan score · "Forever Young" 80s-synth · "We're Going to Be
Friends" folk). The archetype fixes the **lead voice, groove, tempo, mood and form** — so most
melodic/harmonic slots are *reconfigured per song* (the engine itself changes). Its addition:
`INSTR_VOICE` as a **sung, vibrato falsetto croon** (Kip's serenade lead + the Jamiroquai "ooh")
— AIR reached the engine first, but as a vocoder; this is the first to *sing* on it.

| slot | role | preset (rolls by archetype) | engine |
|---|---|---|---|
| `I_BASS` | the low end | `tri/funk-slap` (DANCE) · `sine/round-bass` ⟳ (SERENADE/FRIENDS) · `square/toy-bass` (SWIHART) · `saw/synth-pulse` (FOREVER) | SAW / SINE / SQUARE |
| `I_COMP` | comp / pad | `epiano/funk-clav` (DANCE, auto-wah) · `epiano/napoleon-rhodes` (SERENADE) · `square/toy-organ`-twin (SWIHART detune pair) · `saw/string-machine` ≈ (FOREVER pad) | EPIANO / SQUARE / SAW |
| `I_LEAD` | the star topline | `brass/funk-stab` (DANCE horns) · `square/toy-organ` (SWIHART melody) · `square/forever-lead` (FOREVER) | BRASS / SQUARE |
| `I_GTR` | guitar | `guitar/pizz-mute` (SWIHART double) · `guitar/napoleon-nylon` (FRIENDS fingerpick) | GUITAR |
| `I_AUX` | sparkle / pad | `saw/string-machine` ≈ (SERENADE strings) · `mallet/glockenspiel` (SWIHART) · `pluck/forever-arp` (FOREVER) | SAW / MALLET / PLUCK |
| `I_VOX` | the SUNG croon | `voice/croon` *(first SUNG radio `INSTR_VOICE`)* — SERENADE lead · DANCE "ooh" · FRIENDS voice | VOICE |
| `I_KICK`/`I_SNARE`/`I_HAT`/`I_PERC` | kit — re-tuned per archetype | disco four-floor / soft-soul / stiff straight-8 / big-gated-80s / sparse-brush+clap | SINE / NOISE / SQUARE |
| `I_SOLO` | jam ribbon — voice & behavior per archetype | funk synth (quantized) · `voice/croon` (free) · glock (struck) · synth lead · nylon (struck) | SQUARE / VOICE / MALLET / GUITAR |

**Borrowing at a glance:** mostly napoleon-original because it spans five genres. Reuses on
purpose: the `saw/string-machine` pad pile (≈, for the SERENADE strings + FOREVER pad) and the
round SINE bass (⟳). New recipes it contributes: **`voice/croon`** (the sung falsetto — its
headline), **`square/toy-organ`** (the Swihart deadpan-score identity), **`tri/funk-slap`**, and
first radio voicings of `guitar/pizz`-mute and `mallet/glockenspiel`. The effects-blocked wants
(gated reverb, plate/spring reverb, tape wow, chorus, a noise gate) are catalogued in
[`../design/napoleon-effects-wants.md`](../design/napoleon-effects-wants.md).

## mariachi — Mariachi / son jalisciense (the SESQUIALTERA)

No solo layer; no drum kit (the strumming *is* the percussion, like tango). The seed rolls a
groove (son / vals / huapango), a major-or-minor key, an 8-bar functional progression, and the
form. **First radio station to reach `INSTR_BOWED`** — the violin section is the real bowed
string, not a saw fake. The violins (copla) and trumpets (respuesta) **trade** the melody in
parallel thirds; the time feel is the 6/8-against-3/4 cross-rhythm.

| slot | role | preset | engine |
|---|---|---|---|
| `I_VLN1` | violin 1 — melody lead | `bowed/mariachi-violin` *(first radio BOWED)* · `sinte` swaps to `saw/mariachi-strings` (chair) | BOWED / SAW |
| `I_VLN2` | violin 2 — the third below | `bowed/mariachi-violin` (a touch warmer) | BOWED / SAW |
| `I_TPT1` | trumpet 1 — section + fanfare | `brass/mariachi-trumpet` (kin afrobeat trumpet, brass.c `trumpet`) | BRASS |
| `I_TPT2` | trumpet 2 — the third below | `brass/mariachi-trumpet` · `una` drops it to a flugel (chair) | BRASS |
| `I_VIH`  | vihuela — the manico strum | `guitar/vihuela` (bright, short ring) | GUITAR |
| `I_GTR`  | guitarra de golpe — body strum | `guitar/mariachi-rhythm` · `nylon` warmer (chair) | GUITAR |
| `I_GTRON`| guitarrón — root-fifth bass | `guitar/guitarron` (woody, fretless-gut) | GUITAR |

**Borrowing at a glance:** mostly mariachi-original, because the lineup lives on the
real-string/brass engines barely used elsewhere. `bowed/mariachi-violin` is the **first radio
`INSTR_BOWED`**; `brass/mariachi-trumpet` is *kin* to afrobeat's horn-section trumpet (both real
`INSTR_BRASS`, voiced bright); the three `guitar/*` voices join afrobeat's and air's as the
`INSTR_GUITAR` users. The genre's effects-blocked wants (hall reverb, a true violin-*section*
unison beyond the two-desk pan) are noted in
[`../design/sound-next-steps.md`](../design/sound-next-steps.md).

---

## polopan — Polo & Pan (an ARTIST station, 5 song archetypes)

Solo layer: **`solo.h`** (a celesta-twinkle jam strip over the changes). The seed rolls one
of five archetypes — **Canopée / Ani Kuni / Nanga / Tunnel / Coeur Croisé** — each fixing its
groove, tempo, lead voice and form (stolen-playbook chord brain #4); the seed then varies
key / progression / patterns / timbre *within* it. The lineup lives on the mallet-and-
percussion + untapped-engine shelves: a marimba/balafon/glock/vibe **mallet chair**, a
**pizzicato** bounce on `INSTR_BOWED`, a per-archetype **topline STAR** that swaps engine
(flute / vocoder / glass / sung), and the **Korg MS-10** resonant bass.

| slot | role | preset | engine |
|---|---|---|---|
| `I_MAL`   | mallet chair — marimba / balafon / glock / vibe (per archetype) | `mallet/marimba` (mallet.c) · `mallet/balafon` *(new)* · `mallet/glockenspiel` / `mallet/vibes` (rolled by archetype) | MALLET |
| `I_PIZZ`  | the pizzicato bounce (the groove) | `bowed/pizzicato` *(BOWED pizz — `eng_tune(slot,0,1)`)* | BOWED |
| `I_STAR`  | the archetype's topline | `pipe/flute` (Nanga) · `voice/polopan-chant` (Canopée/Ani Kuni) · `sine/glass-stab` (Tunnel) · `sine/sung-lead` (Coeur) — **swapped per song**; the Nanga flute has a live `pipe`/`sine` A/B in the band panel | PIPE / VOICE / SINE |
| `I_BASS`  | bass — MS-10 riff (Tunnel/Ani Kuni) / round | `saw/ms10-bass` *(new: resonant LP + cut-env wow + drive)* | SAW |
| `I_PAD`   | the lush Solina string wash | `saw/string-machine` ⟳ | SAW |
| `I_ARP`   | the bright sampled-D50 arp build | `pluck/seq-arp` ≈ | PLUCK |
| `I_KICK`/`I_CLAP`/`I_HAT` | breezy Balearic four-floor + clap + swish/shaker | `drum/synth-kick` ⟳ · `drum/house-clap` ↗house · `drum/noise-hat` ⟳ | SINE / NOISE |
| `I_CONGA` | tropical hand percussion (Nanga/Canopée tumbao) | `membrane/conga` ↗addis | MEMBRANE |
| `I_SOLO`  | the jam-strip lead | `mallet/celesta`-ish twinkle | MALLET |

**Borrowing at a glance:** the kit / bass / pad reuse the dance-station shelf (`drum/house-clap`
verbatim, `drum/synth-kick` / `drum/noise-hat` variants, `saw/string-machine` pad, `membrane/
conga` shared with addis). The *new* recipes are `mallet/balafon` and `saw/ms10-bass`. polopan
joins **air** on `INSTR_PIPE` / `INSTR_VOICE` and **mariachi** on `INSTR_BOWED` (here the
*pizzicato* technique, where mariachi bows) — the cluster of stations reaching the engines that
sat untapped before this batch. Effects-blocked wants (plate reverb, tape/chorus on the pads,
ping-pong delay on flute/mallet) noted in
[`../design/sound-next-steps.md`](../design/sound-next-steps.md).

---

## modaljazz — Miles Davis "So What" (the first MODAL-generating station)

Solo layer: **`improv.h`** (the shared soloist, routed to the trumpet in its solo, the Rhodes
in its). The novelty isn't the band — it's the **chord brain**: the first station to run a
*modal* vocab from the shared harmony brain FORWARD. `HB_DORIAN_STYLE` (`runtime/harmony.h`)
floats the i↔IV dorian vamp (that bright major IV is dorian's whole tell), one chord held two
bars, home to i each chorus — the same brain `chordwise` *analyzes* with, here *composing*.
Over AABA it lifts a half-step for the B section (the So What key bump) and drops back.

| slot | role | preset | engine |
|---|---|---|---|
| `I_TPT`  | the Harmon-muted trumpet — head + lead solo | `brass/muted-trumpet` *(new: BAND 1700/2 mute over the afro-trumpet macros)* | BRASS |
| `I_RHO`  | the Rhodes — rootless 3-7-9 comping + its own solo | `epiano/rhodes` (h0.15 t0.40 m0.30, LP 2400/2) ↗epiano | EPIANO |
| `I_BASS` | the walking upright | `tri/upright-bass` ⟳ *(cocktail's session double-bass, verbatim: TRI + pitch-env thumb)* | TRI |
| `SL_RIDE`/`SL_HAT`/`SL_BRSH`/`SL_KICK` | brushes — swung ride, hat on 2&4, sweep, feathered kick | cocktail's brush kit ⟳ (SQUARE ride, NOISE hat/sweep, SINE kick) | SQUARE / NOISE / SINE |

**Borrowing at a glance:** the bass and the whole brush kit are cocktail's, verbatim — the
acoustic-jazz cluster. The one *new* recipe is `brass/muted-trumpet` (the Harmon mute = a tight
bandpass over afrobeat's `brass/afro-trumpet` macros). The Rhodes joins the `INSTR_EPIANO`
station voicings (air/italo). modaljazz is the second consumer of the harmony brain's church-mode
vocabs (`chordwise` was the first) — the proof the brain is genuinely shared.

---

# Findings — all 20 stations charted

The catalog answers the question this pair was built for: *what plays what, and where are we
reusing the same recipe?* The picture that emerged:

## Reuse clusters by **idiom**, not by engine or by `solo.h`/`improv.h` membership

| cluster | shared palette | stations |
|---|---|---|
| **dance / groove** | the synth kit (kick/snare/hat ×6), the 808 box, the disco bass, the PWM-square leads | house · italo · citypop · motorik · lowend · dub · jangle · jingle |
| **acoustic jazz / classical** | fake TRI/SINE pianos, the session upright bass, the cross-stick/clave, the shaker | cocktail · modaljazz (+ Rhodes/muted-trumpet) · roadhouse · tango · exotica (· bossa percussion) |
| **islands** (own voices, engine-level kin only) | — | gamelan · addis · carlos · satie · ambient |

`solo.h` membership predicts *nothing* (its five stations split across all three groups);
"synthetic" predicts nothing either (carlos is all-SAW yet shares nothing). The axis is the
**music idiom**.

## The big shared things (extract candidates, by evidence)

1. **The synth kit** — `drum/synth-kick` ×6, `drum/noise-hat` ×6, `drum/noise-snare` ×4. The
   single most-rebuilt thing; well past where a shared helper would pay for itself.
2. **The bass** — 8 hand-rolled basses across engine piles: TRI (disco/upright/fingered),
   SINE (gut/boom-bap/riddim/round), + SAW, FM, pulse, SQUARE one-offs. The session
   **`tri/upright-bass`** alone spans 4 stations.
3. **The PWM-square lead** — `square/` leads (da-funk, glossy, whistle, sax, melodica, siren,
   ymo-lead, hosono-bass): 8 voices on one SQUARE+duty+~5.5 Hz-vibrato *technique*, none a copy
   — reuse as craft, not duplication.

## Showcase carts are the de-facto preset library (5 lineages)

Stations lift presets from the instrument/showcase carts by hand:
**mallet.c** → `mallet/vibes` (×3 stations) + `mallet/celesta`; **organ.c** → three `INSTR_ORGAN`
registrations; **moog.c** → `saw/fat-moog`; **cr78.c** → `kit/cr78` (a whole kit). These hand-
copies drift — `mallet/vibes`' provenance comment was *already wrong* by the time motorik
copied it.

## Upgrade candidates (fake X where the engine now models X)

- **satie / cocktail / tango / exotica** fake the piano on TRI/SINE — none uses the modeled
  `INSTR_PIANO`. satie's is a *fossil* (it predates the engine).
- "Organ" has **four** routes (3× modeled `INSTR_ORGAN` + roadhouse's drawn `INSTR_USER0`);
  "Rhodes" has two (EPIANO vs yacht's FM) — worth knowing the carts don't converge on one engine.

## How variety is produced (three axes, all charted)

- **cross-cart** — shared / variant / cousin / kin (the clustering above).
- **nights** — a slot becomes a *different instrument* per song (cocktail's piano/vibes/guitar).
- **voicings / gear-change** — same part, different tone per song (yacht's bass on TRI/SINE/SAW).

> **The overlay:** this doc is *what each station plays*; [`../design/radio-genre-fidelity.md`](../design/radio-genre-fidelity.md)
> is *what each genre would ideally want vs what we built* — the holes (faked/missing
> instruments where a real engine sits unused, the generative brains we lack), researched
> blind to the source then diffed against these charts.

---

*(The Findings above are the original 20-station pass; **afrobeat** (charted above the
Findings) is the 21st, added on ship — the first to reach the untapped GUITAR/BRASS shelves.
All on `runtime/radio.h`. Keep the **used by**
lines current as new stations or recipes appear.)*
