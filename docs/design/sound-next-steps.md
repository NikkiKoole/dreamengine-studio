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

4. ~~**Brass**~~ **→ SHIPPED 2026-06-10** as the `brass` cart (lip-reed `INSTR_BRASS`, the
   draggable trombone slide). This was the last engine-blocked instrument — **every modeled
   timbre now has an engine.** Follow-up: move the horn *fakes* (addis/dub/citypop/yacht) onto it.

Plus the fresh ideas: **clanky pots & pans** (an FM-clang/detuned-square junk-metal kit) and
**arco upright bass**. Build any with the [intent-first voice brief](../guides/cart-authoring-prompt.md).

> **Juno — build it *after* the chorus bus.** A Juno-106 needs no new engine (saw/pulse + sub +
> resonant LP + LFO, all built — it's `sh101`/`tb303`/`moog` territory), and polyphony is free.
> Its *one* distinctive bit is the **BBD chorus** — the lush shimmer that makes a Juno a Juno —
> which is § 8.10 effects-bus work. Without it, a Juno is just "sh101 with more voices"; with it
> it's a distinctively lush poly machine nothing else does. So **the Juno is the chorus bus's
> natural showcase cart** — build them together, the way `spacecho` showed off the echo bus and
> `brass` showed off the brass engine. That's the sequencing: chorus lands → Juno proves it.

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

## Engine & effects gaps (core audio-engine work)

Surfaced by the **completeness audit** (Sound Blaster FM · General MIDI · the Roland MT-32 ·
the real orchestra, 2026-06-10). They split by *where they live in the signal graph* — and the
bus side is **already designed and queued**: it's the **▶ NEXT BITE** in
[`instrument-engines.md`](instrument-engines.md) § 8.10 (the gate opened 2026-06-10 once stereo
shipped), disciplined by [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md).
So this is *build* work on a settled architecture, not a fresh design problem.

**Effects bus — § 8.10, designed, not built.** We already have exactly one bus (the shared
`echo()` + per-slot `instrument_echo` sends); § 8.10 generalizes it to `bus[NBUS]` + per-bus FX
chains, master = bus 0. Start with one master reverb. The effects that live here:

1. **Reverb** — the biggest. Orchestra hall, `ambient`'s 8–20 s tails, the MT-32's reverb, GM
   atmosphere/halo pads. The epiano's *temporary* TREM/WAH
   ([`../guides/instrument-recipes.md`](../guides/instrument-recipes.md)) are § 8.10.1 "PARKED
   per-voice stand-ins" explicitly slated to **migrate onto a real bus** here.
2. **Chorus / unison width** — the lush detuned-width of a Juno / stereo Rhodes / string
   *section*. Partly fakeable by layering (see "section blend" below); a real chorus packages it.
   **Its showcase cart is the Juno** (see New instruments) — build them together.
3. **Formant filter** — 4-bandpass-peak vowel filter (navkit-spec'd, reuses a state-variable
   filter). Run **any** instrument through it → choir / vocal-organ / **talkbox** (vowel) color.
   A *complement* to the maturing `INSTR_VOICE` synth, not a substitute: cheap per-instrument
   vowel color for non-voice timbres. Today faked with a single `FILTER_BAND` peak (mouthharp).
   Per-voice filter-mode *or* bus insert — see the placement note below.
4. **Ring modulation / AM** — *an effect, not an engine* (settled). It's a signal × signal
   **operation**, so like the formant filter it's "write once, place as insert or bus." The
   *synth-timbre* version (two internal oscillators → metallic/bell) is **redundant with
   `INSTR_FM`'s clang** — skip the engine. The unique use is ring-modding a **real signal**:
   the **robot / Dalek** vocal (the famous ~30 Hz ring-mod), now reachable as `INSTR_VOICE`
   (the `vox` carts) nears completion. (Distinct from the formant filter's vowel/talkbox color —
   different FX.) Build it as a bus/insert; no new engine.

> **Per-voice vs bus — already settled in principle (decision 0015 + § 8.10).** Several of
> these (formant, chorus, wah, drive) can live *either* as a per-voice insert *or* on a shared
> bus; **reverb is bus-only**; filter/pitch are per-voice. The discipline: write each DSP unit
> **once** and treat placement as a *routing* choice (insert chain vs send bus), not two
> implementations — generalizing today's one hardcoded echo bus. 0015 already paid for this
> lesson once (the **wah detour**: a per-voice follower turned out to be a *bus* effect). Rule
> of thumb from it: default the "either" effects to a **bus** unless there's a reason not to.
> Nothing new to design — read **0015 + § 8.10 + § 8.10.1** and build.

### Each effect → its showcase cart (the build-list)

The project's flywheel: **an effect/engine ships → a flagship cart proves it** (echo bus →
`spacecho`, brass engine → `brass`, chorus → Juno). The same "famous box built around the
effect" homage works for the whole bus roster — which turns "build the effects bus" into a
concrete sequenced build-list, each cart doubling as the effect's acceptance test *and* an
upgrade to existing stations:

| effect | showcase cart | also rescues |
|---|---|---|
| chorus | **Juno** (poly synth — see above) | jingle haze, yacht stereo Rhodes |
| reverb | a **"cathedral" infinite-space pad** (a chord blooms into an endless hall) — or a spring-reverb tank | `ambient` tails, the orchestra hall, glassharmonica, dub's spring-crash |
| leslie (rotary) | a **Hammond B3 + Leslie** organ (slow/fast footswitch) | roadhouse, yacht |
| wah / auto-wah | a **funk clavinet / wah-guitar** (the pedal quack) | citypop funk guitar, the clav |
| formant filter | a **vocoder / talkbox** (carrier shaped through vowels) | the vocal gap for non-voice timbres |
| ring-mod | a **ring-modulator robot-voice toy** (Dalek the VOX) + metallic bells | the Dalek/robot vocal |
| tape (wow/flutter/sat) | a **Frippertronics tape-loop** looper (Eno/Fripp endless tape) | motorik's Conny-Plank echo, jangle/jingle tape-wow |
| bitcrush / decimate *(if rostered)* | a **lo-fi SP-1200 / 8-bit degrader** boombox | lowend's 12-bit grit |

Two properties make this the right sequencing: the cart is the effect's **acceptance test**
(you don't know the reverb works till you've played the cathedral through it), and each one
**pays twice** (the leslie cart also hands roadhouse/yacht their organ).

## Free recipe wins (no engine work — layering techniques, pieces already exist)

The cheapest realism upgrades on the whole list — no engine, no decision, just technique:

- **Attack-transient layering** — glue a short percussive onset (noise tick / click / pluck)
  onto a sustaining engine voice. The attack is what *sells* a faked instrument (it's LA
  synthesis's whole trick; we already do it in the 909 kick / cr78 snare). Pairs with the
  upgrade-wirings above — the cheapest way to make a thin voice convincing.
- **Section / unison blend** — layer N slightly-detuned voices (the gamelan `ombak` trick) so
  one solo `BOWED`/`PIPE`/`REED` reads as a *section* — the gap between one violinist and the
  strings. The only cost is voice budget.

## Decisions to make (deliberate — don't auto-do)

- **Extract the synth kit?** Six stations hand-rebuild the same kick/snare/hat — the strongest
  extract candidate. Whether to make a shared `drum_*()` helper (or some other system) is a
  design call, not a mechanical one. → [`instrument-presets.md`](../guides/instrument-presets.md) (drum families).
- **Lock the `INSTR_VOICE` public macros.** VOICE is the most-wanted unused engine (~12 stations)
  but stays unbuilt-into because it's experimental — **though the `vox` carts are nearing
  completion (2026-06-10)**, so this is closing. Locking its 3-macro mapping unlocks the
  single most-absent timbre on the dial (choir/vocals) — and, ring-modded, the robot/Dalek
  voice (see #4 above). → [`instrument-engines.md`](instrument-engines.md).

*(Brass was the third decision here; resolved — `INSTR_BRASS` shipped *and* the `brass` cart
shipped (2026-06-10). With it, every modeled instrument family now has an engine.)*

---

*Menu distilled from the 2026-06-10 pass; not a commitment. Detail + how-to live in the linked
docs. The throughline of three independent analyses (palette, blind genre research, station
scoring): the richest opportunity is the engines that exist but nothing plays yet —
`VOICE`/`PIANO`/`EPIANO`/`REED`/`BOWED`.*
