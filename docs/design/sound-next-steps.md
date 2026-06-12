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

> **Juno — ✅ SHIPPED 2026-06-10 as the `juno` cart, with chorus.** The sequencing held exactly:
> chorus landed (the BBD modulated-delay, master insert) and the **juno** cart proved it — a
> saw+sub+resonant-LP poly synth (`sh101`/`tb303`/`moog` territory, polyphony free) whose one
> distinctive bit is the BBD chorus. Dry it's "sh101 with more voices"; the OFF/I/II switch flips
> it to a lush poly machine nothing else does — the same effect→showcase flywheel as `spacecho`
> (echo) and `cathedral` (reverb).

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

**Effects bus — § 8.10, STARTED (reverb SHIPPED 2026-06-10).** We had exactly one bus (the shared
`echo()` + per-slot `instrument_echo` sends); the first bite added **reverb as a second SEND bus**
the same way, and formalized the per-sample **master FX section** (send-returns → insert-chain →
soft-clip) so it's send/insert-aware. The first bite deliberately deferred per-slot aux routing —
**now shipped (2026-06-11): an 8-bus pool + `instrument_chorus`/`instrument_flanger(slot,…)`** put
inserts on one instrument. (Sends stayed one shared tank — per-bus reverb/echo was rejected: 8
rooms / 2.7 MB.) The effects that live here:

1. **Reverb** — ✅ **SHIPPED 2026-06-10.** `reverb(size, damping)` + `instrument_reverb`/
   `note_reverb`, a SEND bus mirroring echo; navkit Schroeder core (4 comb + 2 allpass + pre-delay),
   mono v1, dormant-until-called (byte-identical for old carts). Showcase: the **cathedral** cart.
   The epiano's *temporary* TREM/WAH
   ([`../guides/instrument-recipes.md`](../guides/instrument-recipes.md)) are § 8.10.1 "PARKED
   per-voice stand-ins" still slated to **migrate onto a real bus** here.
2. **Chorus / unison width** — ✅ **SHIPPED 2026-06-10** as `chorus(rate, depth, mix)`: the real
   BBD modulated-delay (navkit port, antiphase stereo width), the lush Juno/Solina swirl — NOT the
   detune fake. It landed as a **master INSERT on the shared mod-delay buffer** (the 0015 path: not
   a third send bus; flanger + tape reuse the same technique). Per-instrument since 2026-06-11
   (`instrument_chorus(slot,…)`); `chorus()` is the master/whole-mix variant.
   Showcase: the **juno** cart (the OFF/I/II switch is the whole point).
   - **▶ RETROFIT — `air.c`'s Solina pad (NOW UNBLOCKED 2026-06-11).** AIR's `I_PAD`
     (`saw/solina-ensemble`) fakes the ARP/Eminent String Ensemble with a static **detune-pair**
     (`instrument_tune +0.07`) + a volume LFO — body, but no swirl. A Solina/Juno pad *is literally
     a BBD ensemble chorus* on a saw stack, so **`instrument_chorus(I_PAD, …)`** now choruses the
     pad alone (drums/bass stay dry) — drop the tune-pair hack. Per-instrument inserts shipped, so
     the master-wide caveat is gone.
     Pairs with [`air-effects-wants.md`](air-effects-wants.md).
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

#### Placement sanity-check — the call on each effect, before building (2026-06-10)

The principle above is settled; this is it *applied per effect*, because the wah detour proved
the principle isn't self-executing — the trap is the effect that **looks** per-voice but isn't.
**The wah-test:** does the effect need to act on a *summed/coherent* signal (a chord moving as
one, the groove pumping, the whole performance) **or take a second input signal**? If yes → bus.
If it's genuinely self-contained per-note → per-voice. Run this before committing to *any* build.

**✅ Safe — no placement judgment to get wrong:**
- **Reverb** — ✅ **SHIPPED 2026-06-10** as a master SEND bus (`reverb`/`instrument_reverb`/
  `note_reverb`). Bus-only, unambiguous (one shared tail; a chord must bloom into *one* space) —
  which is exactly why it was the safe first build, and it landed with no placement surprises.
- **Tape (wow/flutter/sat)** — ✅ **SHIPPED 2026-06-11** as `tape()`/`instrument_tape()`: a per-bus
  stereo insert (the 3rd use of the mod-delay technique, after chorus/flanger), whole-mix glue or
  per-instrument lo-fi. Landed exactly as the "safe insert" prediction — no placement surprise.
- **Ring-mod** — single-input (carrier × an *internal* sine), "either," low risk. Just don't
  confuse it with the vocoder.

**⚠️ Real traps — same shape as wah; decide before building:**
- **Chorus splits into two effects.** ✅ **RESOLVED / SHIPPED 2026-06-10.** *Unison detune* (a few
  cents apart) = per-voice, shipped as `instrument_tune` (the cheap stand-in). *BBD/string-machine
  chorus* (the Solina/Juno swirl) = a modulated short delay on the **summed** signal — and it
  landed as a **master INSERT** (`chorus(rate, depth, mix)`), NOT a third send bus: per
  [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md) it's the first use of
  the shared master modulated-delay buffer (the same buffer flanger + tape-wow reuse). So the
  earlier guess that it'd be a *send bus* was off — the 0015-correct home is a master insert on the
  reserved buffer, and that's where it shipped. Showcase: the **juno** cart. The trap the split
  warned against (ship the detune fake, call chorus done) was avoided.
- **Vocoder ≠ formant filter (conflation trap).** *Formant filter* (4 vowel bandpass peaks on one
  instrument) is a **filter** → per-voice-capable (the SVF's 4th use) or bus insert; single input.
  *Vocoder* is carrier × modulator — a synth/chord shaped by a **separate** voice → **two inputs**,
  so **bus with a side-chain**, never a plain per-voice insert. AIR's "Kelly vocoder lead" want is
  the *vocoder*, not the formant filter.
- **Compression / sidechain (if rostered) — wah-shaped.** A per-voice compressor misses everything
  afrobeat asked for: the band "pumping glue" + sidechain pump need the **summed/bus** signal, and
  sidechain needs a **side-chain trigger input** → **bus**, same plumbing as the vocoder.

**🟡 Soft traps — per-voice is defensible, but a coherence gotcha:**
- **Bitcrush** — cheap/stateless like drive, so per-voice is *legal*, but crushing each voice then
  summing ≠ crushing the sum: the authentic SP-1200/sampler grit is the **summed crush**
  (inter-voice intermodulation). Per-part crush is a valid tool; the lo-fi-box *showcase* wants it
  on bus/master.
- **Leslie** — 0015's per-voice-recipe ruling is right for block chords, but carries the **same
  phase-drift caveat as the PARKED epiano tremolo** (§ 8.10.1): per-voice LFOs reset phase at
  note-on, so a *rolled/staggered* organ figure swirls out of phase where one real rotating cabinet
  stays locked. A truly-locked Leslie wants the bus's shared phase-coherent LFO.

**The throughline + the one thing to design up front.** Every trap has the same tell: a **second
input**, or a need for the **summed signal to move as one**. Vocoder and sidechain-comp both need a
**side-chain input** — bus plumbing we don't have yet. Design that side-chain path **once**, during
the `bus[NBUS]` refactor (even though reverb won't use it), so it isn't bolted on twice. That's
where the next wah is hiding.

### Each effect → its showcase cart (the build-list)

The project's flywheel: **an effect/engine ships → a flagship cart proves it** (echo bus →
`spacecho`, brass engine → `brass`, chorus → Juno). The same "famous box built around the
effect" homage works for the whole bus roster — which turns "build the effects bus" into a
concrete sequenced build-list, each cart doubling as the effect's acceptance test *and* an
upgrade to existing stations:

| effect | showcase cart | also rescues |
|---|---|---|
| chorus ✅ **SHIPPED** (+ per-instrument) | **juno** cart (Roland Juno-6 — OFF/I/II chorus switch) ✅ built | jingle haze, yacht stereo Rhodes, **air's Solina ensemble** (`instrument_chorus(I_PAD,…)` — per-part since 2026-06-11) |
| reverb ✅ **SHIPPED** | **cathedral** cart (a chord blooms into an endless hall) ✅ built | `ambient` tails, the orchestra hall, glassharmonica, dub's spring-crash, **air's whole drenched mix** — now wirable via `instrument_reverb` |
| flanger ✅ **SHIPPED** | **mistress** cart (EHX Electric Mistress — guitar + the JET button) ✅ built | the pure-effect showcase (0015's "one true gap," closed); thin station-rescuer (some krautrock/psych/80s texture) |
| leslie (rotary) | a **Hammond B3 + Leslie** organ (slow/fast footswitch) | roadhouse, yacht |
| wah / auto-wah ✅ **SHIPPED** | **clavinet** cart (Hohner D6 + auto-wah — the funk wakka-wakka) ✅ built | citypop funk guitar, the clav — now `instrument_wah()` |
| formant filter | a **vocoder / talkbox** (carrier shaped through vowels) | the vocal gap for non-voice timbres, **air's Kelly-vocoder lead (faked on raw INSTR_VOICE today)** |
| ring-mod | a **ring-modulator robot-voice toy** (Dalek the VOX) + metallic bells | the Dalek/robot vocal |
| tape (wow/flutter/sat) ✅ **SHIPPED** | **tapeloop** cart (Frippertronics — pad → long echo loop → `tape()` degrades each pass) ✅ built | motorik's Conny-Plank echo, jangle/jingle tape-wow, **air's vintage analog warmth** — now wirable via `tape()`/`instrument_tape()` |
| bitcrush / decimate *(if rostered)* | a **lo-fi SP-1200 / 8-bit degrader** boombox | lowend's 12-bit grit |

Two properties make this the right sequencing: the cart is the effect's **acceptance test**
(you don't know the reverb works till you've played the cathedral through it), and each one
**pays twice** (the leslie cart also hands roadhouse/yacht their organ).

> **Demand witness — Afrobeat.** The blind-band brief for the Afrobeat station voted for
> **six** of these boxes at once (wah, reverb, tape, chorus, leslie, drive) and surfaced
> **compression** as a *missing* roster entry — the strongest single-station case for the bus
> so far. The itemized wants → effect mapping (incl. the "organ isn't there without Leslie"
> and "electric bass needs amp+comp" notes) is in
> [`afrobeat-effects-wants.md`](afrobeat-effects-wants.md); `afrobeat.c` is a ready
> acceptance test once the bus ships. **Possible new roster entry: compression / sidechain**
> (dbx/1176 showcase) — not currently listed above; revisit if a second station votes for it.
>
> **Demand witness — AIR.** The Moon Safari station (artist station, five song archetypes)
> votes hardest for **reverb** — AIR's entire sound is *drenched*, and `air.c` fakes it with
> the echo bus (a delay, not a room) — and **chorus**, the Solina/ARP string-machine ensemble
> faked today on a detuned-SAW pair + LFO. Also wants **tape sat** (vintage analog warmth) and
> a true **vocoder/formant filter** (the Kelly-Watch-the-Stars robotic lead is currently a raw
> `INSTR_VOICE` vowel, not a vocoder carrying the song's harmony). Itemized:
> [`air-effects-wants.md`](air-effects-wants.md). One engine *intonation* caveat surfaced
> while building it, not an effects-bus issue — see that doc's "Engine note" on `INSTR_PIPE`.

## Free recipe wins (no engine work — layering techniques, pieces already exist)

The cheapest realism upgrades on the whole list — no engine, no decision, just technique:

- **Attack-transient layering** — glue a short percussive onset (noise tick / click / pluck)
  onto a sustaining engine voice. The attack is what *sells* a faked instrument (it's LA
  synthesis's whole trick; we already do it in the 909 kick / cr78 snare). Pairs with the
  upgrade-wirings above — the cheapest way to make a thin voice convincing.
- **Section / unison blend** — layer N slightly-detuned voices (the gamelan `ombak` trick) so
  one solo `BOWED`/`PIPE`/`REED` reads as a *section* — the gap between one violinist and the
  strings. The only cost is voice budget.

> **These two are the heart of [`recorded-timbres.md`](recorded-timbres.md)** — the recipe for
> making a *synthesized* source read as a *recording* (attack transient + ensemble + movement +
> noise + tape/reverb), and the design notes for a more-real **Mellotron** cart (drawn voices + the
> ~8s tape limit). `tapeloop` is the first lean on it.

## Parked — effects bus architecture (engine, wanted) — 2026-06-12

Three *independent* increments to the master-FX / aux-bus layer, fully mapped (sketches, cost
ledger, open decisions) in **[`effects-bus-architecture.md`](effects-bus-architecture.md)**.
The throughline: the engine is **already a mixing console** (per-instrument aux buses + a master
strip); these are *routing* changes, not new effects (decision 0015 holds). Short version:

- **A — reorderable inserts** (lowest cost, no new buffers). Make the per-bus INSERT order an
  array the audio thread walks, + a small `fx_order(bus, kinds, n)` API. Today the order is
  **hardcoded** (`…chorus→flanger→tape→eq→crush`). **The natural first move** — self-contained,
  byte-identical when unused, and it makes the [`pedalboard`](../../tools/carts/pedalboard.c)
  drag mean something (the pedal row is currently only a visual reading of the fixed order).
- **B — multi-reverb send bus** (costs ~24 KB/tank). Lift the single shared reverb tank into a
  small **sparse pool + `bus→tank` map**, so the drums can be in a tight room while the keys
  ring in a long plate — two reverbs at once.
- **C — reverb as an insert on a dedicated bus** (= B's tank pool + the wet returns through the
  insert chain). Unlocks effects *after* the space (reverb→bitcrush). Composes with A, and keeps
  B's simple send-knob as its friendly face. Its one tax — moving the FP summation order — is a
  **one-time re-baseline, not ongoing wobble**, and this repo has **no committed golden-wav suite
  to break** (`tune-check` is pitch-based and won't notice). See the detail doc §6.
- **D — sidechain & bus compression** ✅ **SHIPPED 2026-06-12** (independent of A/B/C; the **cheapest** — no buffers, no
  bit-repro tax). Adds the one genuinely-new thing: a **second input path** — a `sc_key[]` send
  accumulator a trigger (the kick) feeds, read by a compressor that ducks the *victim* bus. Ends the
  per-voice pump **fake** that `house` + `groovebox` ship today, and the side-chain input path is
  **the same plumbing the vocoder needs**, so it pays a second debt. `sidechain()`/`sidechain_key()`/
  `glue()`; the `groovebox` PUMP/GLUE knobs are a 1:1 rewire + ready acceptance test. Full sketch:
  detail doc **Increment D**.

- **Scope of A:** the **5–6 inserts** (bitcrush / eq / chorus / flanger / tape / wah). **echo +
  reverb are parallel SENDS** — not reorderable until C makes reverb a bus insert.
- **A + C + D SHIPPED 2026-06-12** — A: `fx_order(bus, kinds, n)` + `FX_*` (pedalboard builder is the
  showcase). C: multi-reverb tank pool + reverb-as-bus-insert + `reverb_bus_fx` (effects after the
  reverb); B was subsumed as planned. D: `sidechain()`/`sidechain_key()`/`glue()` — the summed-bus
  pump + bus comp (`groovebox` PUMP/GLUE is the showcase); the `sc_key` input path is waiting for the
  vocoder to reuse. **The effects-bus layer is complete** (the vocoder's carrier×modulator is the only
  remaining two-input effect). See STATUS.md + [`effects-bus-architecture.md`](effects-bus-architecture.md).

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
