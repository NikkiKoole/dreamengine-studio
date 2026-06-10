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

## Stations not yet charted

carlos · exotica · gamelan ·
roadhouse · satie · tango · ymo · yacht

(20 stations total; all on `runtime/radio.h`. Charted: italo · house · citypop · motorik ·
cocktail · lowend · bossa · dub · jangle · jingle · addis (11). addis confirms real-engine
stations add new families rather than reuse the synth kit. Remaining: the other `improv.h`
soloist roadhouse, and the acoustic/orchestral stations satie/tango/gamelan/carlos/exotica/
yacht/ymo.)
