# Bossa rack — the chord-bloom rack: play the changes, the band follows

STATUS: READY (2026-07-17, design pass with the maker) — the first **chord-bloom** rack: turn the
read-only `bossa` radio into a playable instrument where **you drive the harmony and the band blooms
it live** (chordblossom's "pick a chord, it affects more" applied to a radio's genre brain). Not
built. **~70% already exists in `bossa.c`** — the band already blooms entirely from the chord
functions, and it already ships a chord-locked solo strip you jam over. The rack adds the ONE missing
piece: a **playable/editable chord chart** (the yachtrack "make the chart playable" gap), plus a
melody-cell disclosure layer, arrangement, and export.

> **★ BUILD FINDING (2026-07-17) — the winning direction is FLAVORS on chordblossom, NOT a radio
> turned inside-out.** We built the radio-rack first (`bossabloom` — the plan below) and the maker's
> play-test verdict was that it *"feels too much like just the radio + a tiny bit of UI"*: the band
> still composes and you only nudge a chord. The fix that landed (*"sounds right!"*) was the opposite
> shape — **`chordblossom2`**: an instrument you PLAY with your hands, wearing a genre **FLAVOR** (a
> backing band that follows the chords you play). A flavor = **auto-color** (extend every chord to
> its 7th/9th) + **comp rhythm** (a strummed 2-bar clave) + **rootless voicing** (3-5-7-9) + **timbre**
> (nylon) + **groove**. BOSSA shipped and sounds bossa; **YACHT / CITY POP are just more `Flavor`
> rows.** Happy accident: chordblossom's `PM_PATTERN` was already a comp engine, so a flavor is mostly
> *data*. **This supersedes the radio-rack design below** — kept for its still-valid analysis of the
> bloom band, the chord-vocab, and the "generation-first vs. play-first" fork that produced this
> finding. See [`genre-box-rosters.md`](genre-box-rosters.md) for the flavor generalization; the live
> cart is `tools/carts/chordblossom2.c`.

> **The thesis.** The earlier racks bracketed the design space and left a hole in the middle:
> [`acidrack`](rebirth-classic.md) is **cells all the way down** (great, but acid has no moving
> harmony); [`yachtrack`](yacht-rack.md) is a **chart + feel knobs** (musically honest, but the
> maker's verdict was *"a tad too indirect"* — knobs between finger and sound). A raw pianoroll is
> the opposite failure — too dumb to jam, it throws away the genre intelligence. The **chord-bloom**
> surface is the middle: you make one smart musical gesture — *play a chord* — and the radio's own
> band voices the guitar, walks the bass, and re-pitches the melody around it, in idiom, live. The
> notes aren't dumped in a grid and they aren't hidden behind knobs; **the harmony is the
> instrument.** bossa is the purest demo because its melody, bass, and comp all *bloom from the
> chord* at play time (survey: [`genre-box-rosters.md`](genre-box-rosters.md) §"chord-bloom").

Companion reading: [`tinyjam-racks.md`](tinyjam-racks.md) (the rack program — lane format, seed-code
handoff, export tiers), [`yacht-rack.md`](yacht-rack.md) (the chord-first rack this fixes the
directness of — its "make the CHART playable" open question is answered here),
[`rebirth-classic.md`](rebirth-classic.md) (the cells rack), [`genre-box-rosters.md`](genre-box-rosters.md)
(the radio-survey that picked bossa), [`device-face-paradigm.md`](device-face-paradigm.md) §1c
(**chordblossom** — the worked keybed body-plan this reuses) + [`candy-style.md`](candy-style.md)
(the skin option), [`../guides/game-music.md`](../guides/game-music.md) (bossa is its worked
example — the chord brain + voice-leading recipes), [`../guides/radio-station-howto.md`](../guides/radio-station-howto.md)
+ `runtime/radio.h` / `runtime/solo.h` (the chassis it's built on).

## 1 · Ground truth from `bossa.c` (research pass 2026-07-17)

### The band already blooms from the chord — this is the whole point

Bossa's genre intelligence is **entirely harmony-driven**. The only stored composition is a chord
function per bar + a melody *rhythm* cell; everything pitched is derived from the chord at playback:

| Part | Where | How it blooms from the chord |
|---|---|---|
| **Harmony spine** | `prog[16]` (`bossa.c:135`) — a function per bar, AABA form (`bar_to_prog` `bossa.c:231`) | a Markov walk over 13 jazz **functions** (`F_I…F_I7`, `bossa.c:92`) with a `TRANS[]` cheat-sheet (`bossa.c:102–118`): ii→V→I, secondary dominants, tritone subs, backdoor cadences. Each function = scale-degree offset + a **fixed quality** (`F_OFF`/`F_QUAL`) |
| **Guitar** | `lead_voices` (`bossa.c:252`) → `rad_lead_to` | rootless **3-voice voice-leading** (3rd/7th/9th color, `QVOICE` `bossa.c:79`) — each voice moves to the *nearest tone* of the next chord. "The single biggest 'sounds composed' trick" |
| **Bass** | `bossa.c:284–294` | surdo: root on 1, fifth on the and, **chromatic approach into the next bar's root** (`func_at(s+2)`) — reads the chord ahead |
| **Melody** | `pick_mel` (`bossa.c:257`) | One-Note-Samba trick: ONE stored **rhythm** cell (`cellOn[6]`), pitch **re-resolves to each chord** — picks the chord tone/9th nearest a drifting walker |
| **"Early change"** | `func_at` (`bossa.c:240`) | hits in the last 8th of a bar already play the *next* bar's chord — the signature bossa anticipation |

So the composition is tiny and **harmony-first**: change one function in `prog[]` and the guitar
re-voices, the bass re-walks, the melody re-pitches — automatically, in idiom. That is the
chord-bloom already working; it's just driven by a Markov chain instead of a finger.

### It already ships the jam layer

`bossa.c:529–540` draws a **chord-locked solo strip** (`solo.h`): the strip's notes ARE the current
chord's tones (root/3/5/9), *"re-shaped as the ii-V-I changes move under you"* — so every note stays
consonant over the secondary dominants and tritone subs. And `play_step` gates the band's flute on
`!solo_open()` (`bossa.c:334`): **when you take the lead, the band lays out** and lets your note ring
to its end (no mid-note chop). Live jamming over the changes is *already built* — it just has nothing
to jam over except a station you can't edit.

### What's read-only (the gap the rack closes)

- **You can't play or edit the chords.** `new_song` (`bossa.c:191`) Markov-generates `prog[16]`;
  the player can only re-roll (SPACE), replay the seed (R), or walk history (`[ ]`). The chart is a
  broadcast, not an instrument.
- **The melody cell is fixed** per song (`bossa.c:203–211`); no way to shape the topline.
- **No arrangement beyond AABA × choruses**, no export, no persistence of *your* version.

## 2 · The design

### The model in one line

**You play/edit the chord chart; the band blooms it live; the solo strip is the jam; the seed rolls
a starting chart you take over.** (yacht's chart/interpreter split, but the chart is now *played*,
not knob-biased — its open question, answered.)

### The five surfaces (device-face paradigm, keybed body-plan)

bossa is chord-first + a solo strip, so it's the [`device-face-paradigm`](device-face-paradigm.md)
**keybed body-plan** — the same one [chordblossom](device-face-paradigm.md) (§1c) is the worked
example for. The five zones:

1. **NAV** — transport (play/stop), face tabs **CHORD / MELODY / MIX / SONG**, key + seed readout,
   the AABA form dots (already drawn, `bossa.c:488–494`).
2. **ALWAYS-LIVE KNOBS** — kept few (the yacht lesson): **VOICING** (the cascade — re-voice the
   ringing chart live), **FEEL** (one "how loose is the band" macro folding the intensity layers
   `midnight/cafe/classic/festival` + humanize), **TEMPO**. That's the honest live set.
3. **THE CONTEXT DISPLAY = the playable chord chart** — the heart. The AABA progression as a row of
   **chord slots** (8 A-bars + 8 B-bars, laid out AABA). Each slot shows its chord name (`Cmaj7`,
   `A7`…). **Tap a slot → select + audition it immediately** (comp the chord *now*, not at the next
   hit — the direct fix). A faint **"→" ghost** shows what the Markov would pick next (the grammar as
   a guardrail, not a cage). Soft-keys inside: **CLR / GEN** (Markov-fill a fresh idiomatic chart) /
   **REC** (the looper, below).
4. **THE CHORD PALETTE (mode grid)** — the row you tap to *place* a chord in the selected slot.
   **DECIDED (2026-07-17): functions-only** — a **diatonic-plus palette of the 13 functions**
   (`F_I…F_I7`, `bossa.c:92`): the in-key chords (Imaj7 / ii m7 / iii m7 / IV maj7 / V7 / vi m7) +
   the color chords (secondary dominants II7/VI7, tritone sub bII7, backdoor iv/bVII7). NOT free
   root+type — that breaks the idiom; the whole point is that tapping *always* yields a chord that
   sounds like bossa (chordblossom's "Key mode" diatonic auto-harmony). **Architect it extensible**
   so free chords can be added later without a rewrite: the palette reads a **chord-vocabulary
   table** (`{degree-offset, quality, label}` — the `F_OFF`/`F_QUAL` arrays generalized), and a slot
   stores a *vocabulary index*, not a hard-coded function enum. Adding a free-chord "off-road" mode
   later = appending entries / a second table, not touching the chart or the bloom. In **MELODY**
   mode this row becomes the **melody-cell editor** (§the pianoroll disclosure).
5. **THE PERFORMANCE SURFACE = the chord-locked solo strip** — *already built* (`bossa.c:539`). Jam
   the flute over your own changes; the band lays out. Vertical = breath dynamics.

### Does it all fit? The face map (feasibility, 2026-07-17)

Yes — and it's *lighter* than [`acidcandy`](../../tools/carts/acidcandy.c), which already fits five
machines at 160×100. The reason: in the bloom model most instruments need **no UI of their own** —
guitar, bass and melody all bloom from the chord, so you drive six instruments through *one* chord
surface instead of a sequencer grid each. The parts distribute across the four tabs (the paradigm's
"scale by faces, not cramming"):

| Face (tab) | Holds | Instruments / controls it drives |
|---|---|---|
| **CHORD** | the chord chart + the function palette + the chord-locked solo strip + VOICE/FEEL/TEMPO | **guitar, bass, melody** (all bloom) + you jam the **solo flute** |
| **MEL** | the melody-cell editor (rhythm lane + pitch pins) | the **flute** topline (the disclosure layer) |
| **MIX** | ~6 compact channel strips (level / mute / voice-flavour) | guitar (TRI↔PLUCK chair), bass, flute, shaker, rim/clave (3-2↔2-3), solo — acidcandy's MST already shows a 4-channel mix at this size |
| **SONG** | AABA arrangement + seed / GEN + WAV / song-code export | the whole-song layer |

Every instrument and its right UI has a home; the depth is one tab-tap away (disclosure by mode, not
amputation). The **one tight screen is CHORD** — the mockup ([`bossaface`](../../tools/carts/bossaface.c))
crams chart + palette + solo + knobs + mascot and it's dense. Standard device-face discipline
relieves it: the **palette appears only when a slot is selected** (frees a row), the **mascot shrinks
to a corner pet** once the chart is hero, and chart + solo needn't both be full-width at once. The
escape hatch is the paradigm's own: on a **tablet, show more faces at once** (CHORD + MIX side by
side) rather than rearranging — so the phone tab-between is the *tight* case and it already fits at
acidcandy's density.

### The live-jam looper (chordblossom's killer, and the jam-first path)

Arm **REC** and play chords on the palette/keybed in real time → each chord is captured into the
chart at the current bar → it **loops** (the AABA is the loop) → and you **solo over yourself** on
the strip while the band blooms your progression. This is the "for live jamming" heart the maker
asked for: the fast path isn't editing 16 slots, it's *playing* a progression and having a band
appear under it. (Chordblossom already has this loop; bossa's `prog[]` + `solo.h` are the two halves
ready to wire.)

### The skin — a bossa take on the candy device-face (DECIDED 2026-07-17)

Borrow heavily from all the device work — this is a **jam cart**, and jam carts should look like their
**app icon**: small, toy-ish, a warm handheld you want to hold, the same instinct that produced
[`candy-style.md`](candy-style.md) (its north star is literally the TinyJam app icon) and shipped in
[`acidcandy`](../../tools/carts/acidcandy.c). So: the [candy device-face](device-face-paradigm.md)
chassis + chrome (TACTILE bevel, LCD context display, LCD-native widgets inside the screen, juice,
target **160×100 ×4** for the iconic chunk), reusing acidcandy's parts (the note-bar/lane idioms, the
gear-drag knobs, the soft-key screen).

But we're **another rack, so lean bossa** — don't clone the acid green-slime look; give this one its
*own* warm identity within the same grammar:
- a **bossa palette** — warm sand/teak/tropical (bossa.c's current shell is already `CLR_BROWN` +
  `CLR_ORANGE`; think beach-cassette / '60s Brazilian hi-fi), not the acid purple+lime.
- a **bossa mascot/character** on the LCD (candy's "something alive looking back at you" rule,
  [`candy-style.md`](candy-style.md) §3 / [`acid-pets.md`](acid-pets.md)) — a *bossa* creature (a
  swaying beach cat, a little bird on a wire, a cocktail-umbrella sprite) that bops to the clave and
  reacts to the chord you play, rather than the acid slime.
- the chord chart + solo strip get the **warm** treatment; the LCD is still a green phosphor readout
  (the paradigm's constant), but the chassis around it is bossa.

The device-face *grammar* is shared (that's the reuse); the *personality* is per-cart (that's the
"more bossa"). Exact palette + mascot = a mockup pass (cheap draw-only, the `facemock`/`tinyface`
route) before the functional build.

### The pianoroll as the disclosure layer (NOT the main event)

bossa's melody is a *rhythm* cell + a pitch *walker* — so the melody "pianoroll" is smart, not a note
dump: in **MELODY** mode you edit the **onset rhythm** (`cellOn`) on a 32-step lane (tap = toggle, the
acidcandy note-bar idiom), and optionally **PIN a pitch** per onset (an override array `cellPitch[6]`,
−1 = "let it bloom"). **Default: every note blooms** from the chord (`pick_mel`); you pin only the
one or two notes you care about. Same idea extends later to a bass-approach pin. This is the answer to
"we want something smarter than all the notes in a piano roll" — you edit the *few* notes that matter,
the rest follow the harmony.

## 3 · The truth state (the save blob, ~40 bytes)

The lane format the whole pipeline flows through (generator writes it, taps edit it, `save_bytes`
persists it, playback + export read it):

```
Chart {
    u8  keyPc;
    u8  prog[16];         // the chord chart — function per bar (was Markov-only, now editable)
    u8  comp, claveSwap;  // the groove selectors
    u8  cellOn_mask;      // melody onset rhythm (32-step → 4 bytes; or keep cellOn[6])
    s8  cellPitch[6];     // per-onset pitch PIN (−1 = bloom via pick_mel) — the disclosure layer
    u8  choruses, feel, tempo, voicing;
    u32 seed;             // the roll this chart grew from (for R / history / share)
}
```

Same seed → same starting chart (the `radio.h` seed rule holds); once you edit a slot, *your* version
is the truth. Export tiers per [`tinyjam-racks.md`](tinyjam-racks.md): **WAV** (arm `.bake/wav_request`),
the **8-hex song code** (the seed, shareable/typeable), and later **song.h** (a game cart `#include`s
your bossa as its soundtrack) / **MIDI** (chart → chord/lead/bass channels, chordblossom-style).

## 4 · Build order (most is wiring existing parts)

1. **The playable chart** — make `prog[16]` editable: the chart display (reuse the prev/CUR/next
   readout `bossa.c:480–495` grown to a full-row chart), tap-to-select + **tap-to-audition** (call
   `lead_voices` + a comp hit on tap), and the chord palette (the 13 functions). *This is the core
   deliverable — everything else already plays it.*
2. **The GEN/CLR + Markov ghost** — reuse `gen_section`/`markov_next` (`bossa.c:177–184`) for GEN and
   the "→" next-chord hint. CLR empties to a I-vi-ii-V default.
3. **The looper** — REC captures palette/keybed chords into the chart at the playhead bar; loops.
   The solo strip (`bossa.c:539`) is already the "over yourself" half.
4. **MELODY mode** — the `cellOn` rhythm lane + `cellPitch[]` pins; `pick_mel` reads the pin
   (`bossa.c:257`: if `cellPitch[i] >= 0` use it, else bloom).
5. **Persistence + export** — `save_bytes` the Chart; WAV button; song-code display/entry.
6. **Skin + body-plan pass** — candy device-face vs. bossa's own warm shell (open question below).
   Fold `spec()` acceptance (a golden chart→songsum, like yacht's 44-pair corpus).

Slot pressure: none — bossa uses 6 slots (`bossa.c:67–72`); the rack adds no voices, only a surface.

## 5 · Open questions

- ~~**Functions vs. free chords.**~~ **DECIDED (2026-07-17): functions-only** — most bossa; the
  palette is the 13 idiomatic functions and tapping always stays in idiom. Architected extensible (a
  chord-vocabulary *table*, slots store an index — §2 zone 4) so a free-chord "off-road" mode is a
  later append, not a rewrite.
- **How playable is the VOICING dial?** chordblossom's cascade shifts one chord tone at a time;
  bossa's guitar is a fixed rootless 3-voice set (`QVOICE`) voice-led by `rad_lead_to`. Map the dial
  to register/inversion window + a voicing-flavour swap (3-7-9 ↔ 3-5-6)? Or is voice-leading enough
  and the dial is really "spread"?
- **Per-section chords already exist** (AABA A vs B) — do we expose the *form* as editable too (add a
  bridge, change AABA→ABAC), or is the fixed AABA the honest bossa constraint? (yacht left the same
  question open.)
- ~~**Skin.**~~ **DECIDED (2026-07-17): a bossa take on the candy device-face** (§2b) — the shared
  device-face/candy grammar (toy-ish, app-icon-like, 160×100 ×4, reuse acidcandy's parts) with its
  own warm-bossa palette + mascot. Open sub-question: the exact palette + which bossa creature — a
  cheap draw-only mockup pass (`facemock`/`tinyface` route) settles it before the functional build.
- **`rack.h` — the third, maximally-different customer.** acidrack = cells; yacht = chart + feel
  knobs; bossa-rack = **a *played* chart + a bloom band + a solo strip**. Three shapes now — what
  survives all three is what belongs in `rack.h` (still deferred; this is the datapoint that could
  trigger it — see [`tinyjam-racks.md`](tinyjam-racks.md) §rack.h, [`yacht-rack.md`](yacht-rack.md) §5).
- **Naming / trademark** — bossa nova is a genre, not a brand (no Roland-style constraint); original
  faceplate name, maker's call (as with acidrack/yachtrack).
