# Sound research — actionable next steps

Distilled from the 2026-06-10 sound-docs pass (the recipe **palette**, the genre-fidelity
**gap ledger**, and the new-sound / new-demand scoring). A **menu, not a backlog** — nothing
here is urgent. Ordered roughly by *leverage* (impact ÷ effort); each item links to the doc
with the detail and the how-to.

## Start here — three cheap, high-confidence wins

All three are **wiring, not engine work** (the engine already models the thing):

1. **`satie` → `INSTR_PIANO`.** A one-engine swap on the *most-faithful* station; its only real
   gap, and a flagged fossil (satie predates the piano engine). The single clearest upgrade.
   → [`radio-genre-fidelity.md`](radio-genre-fidelity.md) (satie) · [`instrument-recipes.md`](../guides/instrument-recipes.md) (`piano/grand`).
2. **`citypop` → `INSTR_EPIANO` Rhodes bed.** "The single biggest win" for the least-faithful
   *sound* cart — the Rhodes is city pop's harmonic bed and is currently absent. → fidelity (citypop).
3. **`lowend` upright → `INSTR_BOWED` pizz, Rhodes → `INSTR_EPIANO`.** Two faked load-bearing
   voices with real engines ready. → fidelity (lowend).

## Upgrade wirings (fake X where a real engine sits unused)

The gap ledger's dominant theme. Beyond the three above: `cocktail` (PIANO + BOWED pizz),
`yacht` (EPIANO Rhodes + REED sax), `tango` (BOWED strings + PIANO), `addis` (REED sax),
`bossa` (default the real PLUCK nylon + PIPE flute), `roadhouse` (EPIANO Rhodes-bass). Every
station's list is in [`radio-genre-fidelity.md`](radio-genre-fidelity.md). The most-wanted
unused engine across all 20: **`INSTR_VOICE`** (~12 stations, used by none). And now that
**`INSTR_BRASS` has landed**, the brass *fakes* can move onto the real engine — `addis`'s PD
synth-brass horn, `dub`'s trombone stabs, `citypop`/`italo` brass stabs, `yacht`'s horns.

## New instruments — most *new sound* (now-unblocked engines → playable)

Scored in [`cart-library-direction.md`](cart-library-direction.md) § 2b (MEMBRANE/REED/BOWED
all shipped, so these are buildable today):

1. **Hand-drum** (MEMBRANE) — touch-position = strike-position macro, 1:1. ★★★★★
2. **Bellows / melodeon** (REED) — bisonoric: push vs pull = different note. ★★★★★
3. **Violin / cello** (BOWED) — drag = bow pressure. ★★★★☆

4. **Brass** (trombone / trumpet / brass section) — `INSTR_BRASS` **just landed** and the
   instrument cart is **in progress** (Nikki, 2026-06-10); the slide trombone's live slide is
   the natural showcase. This was the last engine-blocked instrument — now unblocked.

Plus the fresh ideas: **clanky pots & pans** (an FM-clang/detuned-square junk-metal kit),
**Juno** (the lush *poly* pad we lack; chorus waits on the effects bus), **arco upright bass**.
Build any with the [intent-first voice brief](../guides/cart-authoring-prompt.md).

## New stations — newest *demand*

Scored in [`future-stations.md`](future-stations.md) ("newest demand"):

1. **Eno "Music for Airports"** — forces `INSTR_PIANO` *and* `INSTR_VOICE` choir **plus** the
   prime-length phasing brain: three firsts at once.
2. **Plantasia (Mort Garson)** — a Mellotron → the whole `PIPE`/`BOWED`/`VOICE` shelf.
3. **Steve Reich minimalism** — real `INSTR_PIANO` + the phase brain.

## New generative brains (build once, reuse across stations)

From the gap ledger's aggregate:

1. **Section-terrace brain** — named breakdown / pre-chorus kick-drop / riser / ritard-out
   events. Closes *brain* holes in 6+ stations (italo, dub, roadhouse, exotica, jingle, addis,
   gamelan). The highest-leverage single brain.
2. **Prime-length loop phasing in absolute seconds** — the Eno/Reich engine; fixes `ambient`'s
   skeleton (today a finite 16-chord loop on a 60-BPM grid).
3. **Drum-led elastic tempo that lands on a structural beat** — extend `tango`'s live-`bpm()`
   TEMPO-AS-A-VOICE to gamelan (kendang→gong) and the Doors jam.

## Decisions to make (deliberate — don't auto-do)

- **Extract the synth kit?** Six stations hand-rebuild the same kick/snare/hat — the strongest
  extract candidate. Whether to make a shared `drum_*()` helper (or some other system) is a
  design call, not a mechanical one. → [`instrument-presets.md`](../guides/instrument-presets.md) (drum families).
- **Lock the `INSTR_VOICE` public macros.** VOICE is the most-wanted unused engine (~12 stations)
  but stays unbuilt-into partly because it's experimental. Locking its 3-macro mapping unlocks
  the single most-absent timbre on the dial. → [`instrument-engines.md`](instrument-engines.md).

*(Brass was the third decision here; resolved — `INSTR_BRASS` shipped and the cart is in
progress. With it, every modeled instrument family now has an engine.)*

---

*Menu distilled from the 2026-06-10 pass; not a commitment. Detail + how-to live in the linked
docs. The throughline of three independent analyses (palette, blind genre research, station
scoring): the richest opportunity is the engines that exist but nothing plays yet —
`VOICE`/`PIANO`/`EPIANO`/`REED`/`BOWED`.*
