# Effects recipes — the grabbable-effect cookbook

The effects companion to [`instrument-recipes.md`](instrument-recipes.md). That file is the
supply-side palette of **instrument patches** (how to voice an `INSTR_*` engine); **this file
is the same shelf for the engine EFFECTS** — echo, reverb, chorus, flanger, tape, auto-wah,
drive (+ its waveshaper modes), bitcrush and EQ. It answers "I want a dub throw / a stone-hall
bloom / a Juno shimmer / an acid squelch / 8-bit grit / a low-end lift — what do I call, and who
already does it well?"

Three layers cover effects, each a different question:

- **[`studio.h`](../../runtime/studio.h) + [`studioDocs.js`](../../editor/src/studioDocs.js)
  = the API** — every effect function's signature + a one-line doc (drives autocomplete + the
  help tab). *What can I call?*
- **[`instrument-engines.md`](../design/instrument-engines.md) §8.10 + [`audio-notes.md`](../design/audio-notes.md) §17 = the why** — the design
  rationale: why echo/wah are shared buses, why drive sits post-filter, the DSP. *Why is it
  built this way?*
- **`effects-recipes.md` (this file) = the how-to** — named, copy-paste settings + the cart
  that proves each one. *What numbers actually sound good, and where do I crib them?*

Most effects are a **shared bus** (one `echo()`/`reverb()`/`chorus()`… for the whole mix) with
an optional **per-instrument** form (`instrument_echo(slot, …)` etc.) so you can effect one part
and leave the rest dry. `drive` is the exception — it's a **per-voice** insert (`instrument_drive`
/ `note_drive`). Full architecture: [`instrument-engines.md §8.10`](../design/instrument-engines.md).

> **⚠ Effects are SET-AND-HOLD — (re)configure them only when a value CHANGES, never every frame.**
> `crush()`/`tape()`/`eq()`/`chorus()`/`flanger()`/`reverb()`… reconfigure bus DSP (ring buffers,
> filter coefficients) on each call. Wiring a knob straight into `update()`/`draw()` so the configure
> fn fires 60×/s churns the audio thread and reads as **stutter / lag — not a crash** (so it's easy
> to miss and easy to blame on the wrong thing). Re-apply **only when the value actually moved**:
> keep a `last-applied` copy and compare, or gate on the `ui_*` widget's "changed" return. Same for
> the live per-note setters (`note_cutoff`/`note_reverb`/`note_vol`): push only when the target
> changed — a value that's genuinely sweeping every frame (a tape-speed wobble, a sidechain duck) is
> fine, but stop re-issuing it once it settles. **Pattern to copy: `groovebox`'s `apply_fx()`** (a
> change-gated re-apply; its pad cutoff only pushes while the pump is actually moving). This was a
> real lag bug in `groovebox` v1 — profiler-confirmed once the per-frame churn was removed.

> **…but the sweep-safe params have a sanctioned per-frame path: `fx_mod()` / `fx_lfo()`.** The
> SET-AND-HOLD warning is about *reconfiguring buffer DSP*. A curated set of *cheap* params (filter
> cutoff/res, drive amount, trem/pan depth, grains/shimmer mix) are fine to move continuously — and
> `fx_mod()` (per-frame CV sink) / `fx_lfo()` (fire-and-forget engine LFO, any `LFO_SHAPE_*`) are the **right way** to
> do it: the engine slews internally so it never zippers, and the `FXMOD_*` enum only names safe
> params, so the API *can't* be pointed at a buffer effect. **Configure the effect first, then
> modulate** — `fx_mod` rides an already-live effect's param, it doesn't enable one. See the
> "modulating effects" section below + the `fxmod` showcase.

> **The showcase carts ARE the de-facto effect library.** Each effect has one cart whose whole
> point is *that effect as the instrument* — crib from it first. Showcases:
> `spacecho` (echo) · `cathedral` (reverb) · `juno`/`solina` (chorus) · `mistress` (flanger) ·
> `tapeloop` (tape) · `clavinet` (wah) · `drivemodes` (drive modes) · `bitcrush` (bitcrush) · `eq` (EQ) ·
> `epiano` (tremolo + phaser) · `organ` (leslie) · `vowel` (formant) · `djfilter` (filter).
>
> **The two bus-effect *homes* (effects on a whole mix, not one voice):** `pedalboard` is the
> SERIAL-insert showcase (`fx_order` — one guitar → a reorderable chain); **`groovebox`** is the
> SUMMED-BUS showcase (six looping voices → one master → live CRUSH/EQ/TAPE/reverb + the master
> `fx_order` toggle + a faked sidechain PUMP). Crib the live-knob master rack from `groovebox`.

---

## echo — `echo(time_ms, feedback, tone)` · `instrument_echo(slot, send)` · `note_echo` · insert: `echo_insert(time_ms, feedback, tone, mix)`

THE shared tape-echo bus. `time_ms` 1–2000 (sweep it live → the tail pitch-bends like tape speed),
`feedback` 0–1.1 (>1.0 self-oscillates into saturation, not a blow-up), `tone` 0–1 (repeats darken).
Per-slot `send` 0–1 chooses how much each instrument throws into it. **Showcase: `spacecho`** (RE-201).

| recipe | call | character | used by |
|---|---|---|---|
| dub throw | `instrument_echo(I_SKANK, 0.18f)` base → `0.9f` on the throw | reggae skank chop that blooms on cue; ride feedback `0.45→0.8` for the meltdown | `dub` |
| dotted-8th tape loop | `echo(60000/(bpm*4)*3, 0.45f, 0.22f)` | tempo-locked dub repeats, dark tail | `dub` |
| slapback | `instrument_echo(slot, 0.10f–0.16f)` | tight doubling on a lead/EP/guitar, no wash | `air` (lead/EP/gtr), `sh101`, `wba` (clean guitar) |
| dynamic room echo | `echo(90+open*300, 0.10f+open*0.30f, 0.35f+open*0.25f)` | echo that opens with the space: corridor→hall driven by one `open` 0..1 | `deeper` |
| hard feed into a loop | `instrument_echo(PAD, 0.7f)` | a pad fed heavily into a long echo = an evolving wash (the tape-loop trick) | `tapeloop`, `wowflutter` |
| live delay pedal | `echo(rate_ms(), fb_x(), tone_x())` from knobs | the effect played as the instrument; feedback into the red = self-osc drone | `spacecho`, `tb303`, `kaoss` (ECHO program; sends opened on the loop only while ECHO is selected) |
| dub-siren throw | held voices at `instrument_echo(slot, 0.7f)`; ride `echo(t, fb, .55)` with `fb` past 1.0 | stab the siren, lift, the echo throws the tail; past the red it self-oscillates and HOWLS forever — pull `fb` back to dub it out | `dubsiren` (the FIRE PAD X axis IS the feedback) |
| master dub throw | ALL parts send into one `echo()`; the siren pad's X rides its feedback (base 0.3 → past 1.0) | the throw smears the WHOLE groove (drums + melody + siren) into the dub, not just the siren — a sound-system desk move | `dubdesk` (rhythm + LPG + siren all feed the one master echo) |
| delay as an in-line INSERT | `echo_insert(time_ms, fb, tone, mix)` + `FX_ECHO` in `fx_order(0,…)` | a real reorderable DELAY pedal whose chain POSITION is audible — `delay→drive` distorts the repeats, `drive→delay` makes clean echoes of a dirty signal | `pedalboard` (DELAY), `modrack` (ECHO module — cv inlet sweeps the mix for dub throws) |
| ANALOG bucket-brigade delay | `echo_insert(160, 0.7f, 0.6f, 0.5f)` + `echo_insert_bbd(1.0f)` | a Way Huge Aqua-Puss / EHX Memory Man: the repeats WOBBLE (clock wow+flutter) and a longer delay DARKENS the tail — coloured on the read tap / loop filter, so ONLY the echoes, never the dry | `aquapuss` (B toggles analog↔clean) |

> **`echo()` (send) vs `echo_insert()` (insert).** `echo()` is the shared parallel send — its wet
> always returns clean to master, so its position in a pedal chain is cosmetic (the same reason
> `reverb()` got `reverb_insert()`). `echo_insert(time, feedback, tone, mix)` is the same tape-delay
> DSP placed *in* the master `fx_order` chain (its own buffer, master-only), so reordering it against
> the other pedals changes the sound. The repeats sit on top of the full dry signal at `mix` level
> (a delay pedal's blend), `feedback` >1 self-oscillates into tape saturation. Put `FX_ECHO` in your
> `fx_order(0, …)` list to place it. **Showcase: `pedalboard`** (the DELAY pedal: TIM/FB/TON/MIX).
>
> **Analog voicing — `echo_insert_bbd(amount)`.** Call it after `echo_insert()` to give the insert a
> real bucket-brigade (BBD) character: the repeats gently WOBBLE (clock wow + flutter) and a longer
> `time_ms` makes the tail DARKER (a real BBD's clock slows as delay lengthens). Both live on the read
> tap / inside the loop filter, so ONLY the repeats are coloured — the dry signal is untouched (a master
> `tape()` insert, by contrast, wobbles everything). `amount` 0 = clean digital delay (default), 1 = full
> analog. **Showcase: `aquapuss`** (the Way Huge Aqua-Puss — `B` A/Bs analog vs clean).

## grains — `grains(grain_ms, density, position, scatter, feedback, mix)` · `instrument_grains(slot, …)` · `grains_freeze(on)` · `grains_pitch(semitones, spread, reverse)` · insert: `FX_GRAINS`

GRANULAR DELAY (navkit `processGranularDelay`, verbatim). Captures the live signal into a buffer,
then on a `density` schedule spawns overlapping Hanning-windowed GRAINS that read scattered slices of
the recent past — an evolving "Cosmos" cloud. `grain_ms` 5–1000 (short = shimmer, long = smear),
`density` 1–100 grains/sec (sparse → wash), `position` 0–1 (0 = deep past, 1 = live edge), `scatter`
0–1 (random spread — the *evolving* knob), `feedback` 0–0.95 (output re-fed → self-sustaining cloud),
`mix` 0–1. **`grains_freeze(1)` stops the capture so the buffer loops forever** — the headline move.
A reorderable insert (`FX_GRAINS`), auto-placed on first call; a small pool means master + one
instrument bus at once. Reads through the shared `moddel_hermite` (= navkit's `hermiteInterpolate`,
already in-tree). 3 s buffer (navkit's is 5 s). **Showcase: `grains`** (the freeze/cloud toy).

| recipe | call | character | used by |
|---|---|---|---|
| classic shimmer cloud | `grains(120, 12, 0.8f, 0.3f, 0.2f, 0.5f)` | mid-size grains near the live edge — a gentle ambient haze over a pad | `grains` |
| freeze pad | hold a chord → `grains_freeze(1)` | the captured moment loops into an infinite drone; solo over your frozen self | `grains` |
| dense glass wash | `grains(40, 40, 0.7f, 0.5f, 0.4f, 0.7f)` | tiny grains, high density + scatter = a sparkling glassy texture | `grains` |
| smeared time-stretch | `grains(400, 8, 0.5f, 0.2f, 0.0f, 0.6f)` | long grains deep in the past = a slow, blurred slow-motion smear | `grains` |
| self-feeding cloud | `grains(150, 16, 0.85f, 0.4f, 0.85f, 0.6f)` | high feedback re-grains the cloud → an ever-thickening evolving drone | `grains` |
| pads-only grain (per-instrument) | `instrument_grains(SL_PAD, 200, 20, 0.6f, 0.5f, 0.4f, 0.6f)` | grain the pads while drums/bass stay dry — its own bus | — |
| reorderable GRAINS pedal | `FX_GRAINS` in `fx_order(0,…)` + `grains(size, dens, 0.8, 0.4, 0.3, mix)` from knobs + `grains_freeze(frz)` | a texture/freeze stompbox whose chain position is audible (grain a fuzzed signal vs fuzz a grained one); FRZ knob loops the buffer | `pedalboard` (GRAINS: SIZE/DENS/MIX/FRZ) |
| octave-up shimmer | `grains(120,18,0.7,0.4,0.3,0.6)` + `grains_pitch(12, 0.2f, 0)` | the cloud rings an octave above — instant Microcosm/shimmer pad over a held note (call `grains()` first to allocate, then `grains_pitch`) | `grains` |
| reverse-grain wash | `grains_pitch(0, 0.0f, 1)` | grains play backwards → the classic sucked-in, blooming reverse texture | `grains` |
| detuned chord-cloud | `grains_pitch(0, 0.6f, 0)` | `spread` scatters each grain's pitch → from unison to a shimmering, chorused cloud with no transpose | `grains` |
| falling cloud | sweep `grains_pitch(semis--, 0.2f, 0)` live | automate the transpose down for a tape-slow / vinyl-stop dive (true pitch-shift, window length fixed) | — |

> **Stochastic, so A/B by character not samples.** `grains` scatters reads via a seeded LCG and
> spawns on a timer; with the intentional 3 s (vs navkit 5 s) buffer the `position` math diverges, so
> sample-identity isn't the target. Verified against navkit's genuine `processGranularDelay` (via
> `tools/navkit-fx-render.c grains …` + `tools/carts/grainstest.c`): crest factor matches within
> ~0.3 dB (7.29 navkit / 7.59 ours) — the level-independent texture fingerprint lines up.

## reverb — `reverb(size, damping)` · `instrument_reverb(slot, send)` · `note_reverb`

THE shared room/hall (Schroeder). `size` 0–1 (small room → endless hall), `damping` 0–1 (bright →
dark tail). Bus-only configure; per-slot `send`. **Showcase: `cathedral`** (organ into stone).

| recipe | call | character | used by |
|---|---|---|---|
| stone hall | `reverb(0.94f, 0.32f)` + sends 0.5–0.9 | vast bright cathedral bloom; the chord becomes the instrument | `cathedral`, `deeper` (hall) |
| lush plate | `reverb(0.62f, 0.38f)` | a roomy, slightly-dark pop verb that doesn't drown the mix | `air` |
| small warm room | `reverb(0.30f, 0.55f)` | tight dark ambience — upright bass, close jazz | `upright`, `wba` (the band room) |
| worn-tape ensemble | `reverb(0.55f, 0.45f)` | mid room under chorus+tape = the Mellotron "recording" | `mellotron` |
| dynamic openness | `reverb(0.12f+open*0.82f, 0.50f-open*0.18f)` | corridor→hall on one knob; sends 0.08→0.92 (10× range) | `deeper` |
| dry bus, wet parts | bass/kick send `0.0f`, mallet/pad `0.32–0.40f` | keep the low end tight + punchy while melody hangs in the hall | `polopan` |
| dub splash | `reverb(0.2f+verb*0.8f, 0.45f)` + held-siren sends ~0.4 | the spring-tank wash under a sound-system siren (set the send BEFORE `note_on` or a held voice never reaches the tank) | `dubsiren` |

### reverb send-buses — `reverb_bus(tank, size, damp)` · `instrument_reverb_bus(slot, tank, mix)`

**Two reverbs at once.** `reverb()` above is one shared room (tank 0); these carve **independent**
spaces on tanks 1–2, each on its own bus, so different instruments bloom into different rooms in the
same mix. Configure the space with `reverb_bus(tank, size, damp)`, route a slot in with
`instrument_reverb_bus(slot, tank, mix)`. (Tank 0 stays `reverb()`'s master send — use 1–2 here.)
**Showcase: `reverb spaces`** (a tight mallet room + a vast organ plate, ringing together).

| recipe | call | character | used by |
|---|---|---|---|
| tight bright room | `reverb_bus(1, 0.34f, 0.15f)` + send 0.6–0.8 | small, close, lively — percussion/plucks that need to stay crisp | `reverbspace` (ROOM) |
| vast dark plate | `reverb_bus(2, 0.95f, 0.55f)` + send 0.8 | endless, warm, swallowing — pads/organ that should drown | `reverbspace` (PLATE) |

#### effects AFTER the reverb — `reverb_bus_fx(tank, fx, a, b, c)`

The send-bus's signature trick: run a pedal **on the wet tail** (the reverb rings, then the effect
chews it — a plain send can't, its wet always returns clean). `fx` = `FX_CRUSH` / `FX_EQ` / `FX_TAPE`
/ `FX_CHORUS`; `a/b/c` are that effect's own params (same scale as its dedicated API — crush: bits,
rate, mix · eq: low/mid/high dB · tape: wow/flutter/sat · chorus: rate/depth/mix). Effects stack in
call order; `crush`/`chorus` `mix 0` = bypass. Call `reverb_bus(tank,…)` first.

| recipe | call | character | used by |
|---|---|---|---|
| crushed lo-fi tail | `reverb_bus_fx(2, FX_CRUSH, 5, 8, 1.0f)` | the plate decays into grainy, aliased 5-bit dust — shoegaze/vaporwave space | `reverbspace` (CRUSH toggle) |
| dark-tail EQ | `reverb_bus_fx(1, FX_EQ, 0, 0, -8.0f)` | roll the highs off the wet only — a warm tail without dulling the dry source | — |
| tape-worn reverb | `reverb_bus_fx(2, FX_TAPE, 0.4f, 0.2f, 0.5f)` | the tail wows + saturates like a reverb printed to tape | — |

#### reverb as an in-line pedal — `reverb_insert(size, damp, mix)`

The third reverb shape: a dry/wet-MIX insert **on the master bus**, so it's a real reorderable pedal
(unlike `reverb()`, a parallel send whose chain position is cosmetic). Put `FX_REVERB` in your
`fx_order(0, …)` list to place it; dragging it before/after another pedal is then audible. `mix 0` = bypass.

| recipe | call | character | used by |
|---|---|---|---|
| honest pedalboard reverb | `reverb_insert(0.7f, 0.3f, 0.45f)` + `FX_REVERB` in `fx_order(0,…)` | a guitar-pedal reverb whose position matters: before crush = crush the wet tail; after = reverb the crushed guitar | `pedalboard` (REVERB pedal), `groovebox` (SPACE knob — the ORDER toggle IS reverb↔crush on the summed mix), `modrack` (VERB module — on by default for a touch of space; cv inlet swells the mix) |

#### spring-tank voice — `reverb_spring(amount)` · `reverb_spring_tone(x)`

Turns ANY of the reverbs above (send, bus, or insert) into a Fender/surf/dub **spring tank**, not a smooth
digital hall: transients disperse (highs chirp ahead → the metallic *boing*) and the tone narrows to a mid
band. `reverb_spring(amount)` 0 = clean digital (byte-identical), 1 = full spring; `reverb_spring_tone(x)`
rides the "boing" character live (0 looser → 1 tighter/twangier). Global — call once; affects every tank.

| recipe | call | character | used by |
|---|---|---|---|
| kick the tank | a broadband transient (noise/membrane) into a spring reverb, high send | the raw dispersive *boing/drip* — knock the tank | `springtank` (KICK scene) |
| surf drip | `reverb(0.6f, 0.35f)` + `reverb_spring(1.0f)` on a clean twangy guitar | wet, metallic Dick-Dale surf | `springtank` (SURF) |
| dub spring | long `reverb(0.88f, …)` + `reverb_spring(0.9f)` on off-beat organ stabs | deep, boingy dub skank tail | `springtank` (DUB) |

### shimmer — `shimmer(size, damp, shimmer_amt, mix)` · `instrument_shimmer(slot, …)`

A **shimmer reverb**: a reverb with an **octave-up pitch-shifter inside its feedback loop**. Each pass,
the wet tail is tapped, pitched up an octave (a 2-grain overlap-add shifter), and re-injected — so a
held chord **climbs**, rising into a glassy, ascending crystalline pad (Strymon BlueSky / Eno). The
roadmap **trophy** (Primitive 2 — the engine's first real-time bus pitch-shifter). `size` 0–1 (decay),
`damp` 0–1 (darker tail), `shimmer_amt` 0–1 (the climb: 0 = a plain reverb, ~0.7 blooms-and-ascends,
1.0 = a near-infinite rising wall), `mix` 0–1 (0 = off). Master-stage. **Showcase: `shimmer`**; also a
SHIMMER pedal in **`pedalboard`** (a macro pedal driving master `shimmer()` — runs at the output stage,
like a reverb at the end of the rig, so its chain position is cosmetic).

| recipe | call | character | used by |
|---|---|---|---|
| classic shimmer pad | `shimmer(0.85f, 0.4f, 0.65f, 0.5f)` | hold a chord → it blooms and climbs an octave halo above — ambient/post-rock | `shimmer` |
| endless ascent | `shimmer(1.0f, 0.4f, 0.95f, 0.6f)` | near-infinite rising wall of crystal (loop self-sustains, tanh-governed so it can't blow up) | `shimmer` |
| subtle octave halo | `shimmer(0.7f, 0.5f, 0.35f, 0.4f)` | just a touch of rising sparkle over a normal reverb bloom | — |
| shimmer pad, dry drums | `instrument_shimmer(SL_PAD, 0.85f, 0.4f, 0.6f, 0.4f)` | shimmer ONE instrument's bus — the pad blooms while the drums/bass stay bone dry (the per-instrument pool: master + 1 instrument). Composes with spatial: the dry source still pans/Dopplers, the wash is an ambient tail | — |

> **Stability built in.** A pitch-shifter in a feedback loop wants to either die or explode (it did
> both in testing — runaway to full-scale + DC pileup). Tamed with three things: the octave-up output
> normalized to ~unity, the recirculated feedback **soft-clipped through `tanh`** (a governor — high
> settings plateau into the infinite climb instead of exploding, the echo-bus trick), and a **DC
> blocker** in the loop (the combs pump DC under recirculation). Verified: octave-up confirmed (a 110 Hz
> stab's tail climbs to ~500 Hz), and max settings stay bounded (~0.04% soft-clip at the absolute ceiling).

BBD/Juno ensemble widening. `rate` 0.1–5 Hz, `depth` 0–1, `mix` 0–1 (0 = off). Master (whole mix) or
per-instrument. **Showcase: `juno` (master) + `solina` (per-part ensemble).**

| recipe | call | character | used by |
|---|---|---|---|
| Solina ensemble II | `chorus(1.6f, 0.70f, 0.72f)` | THE deep lush string-machine swirl — the signature | `solina` |
| Solina ensemble I | `instrument_chorus(I_PAD, 0.9f, 0.50f, 0.60f)` | classic lush, less intense; per-part so only the pad swims | `solina`, `air` |
| Juno I | `chorus(0.1f, 0.45f, k_mix)` | slow gentle thickening — the Juno-6 mode I | `juno` |
| Juno II | `chorus(0.22f, 0.62f, k_mix)` | faster + wetter, more obvious wobble | `juno` |
| Rhodes width | `instrument_chorus(I_EP, 0.7f, 0.30f, 0.28f)` | gentle stereo on an EP, not a full swirl | `air`, `wba` |
| metallic shimmer | `instrument_chorus(slot, 1.4f, 0.55f, 0.55f)` | brighter, faster — the Martenot "métallique" diffuseur | `martenot` |

## flanger — `flanger(rate, depth, feedback, mix)` · `instrument_flanger(slot, …)`

Swept comb with feedback (the jet-whoosh). `feedback` is signed: + = jet/metallic, − = through-zero.
Needs a rich source (chords, noise). **Showcase: `mistress`** (on a strummed guitar).

| recipe | call | character | used by |
|---|---|---|---|
| jet guitar | `instrument_flanger(GTR, rate, depth, fb, mix)` live | classic swept-comb whoosh; sweep the rate by hand | `mistress` |
| metallic static wash | `instrument_flanger(NZ, 0.12f, 0.97f, 0.9f, 0.6f)` | slow, deep, high-feedback comb on a noise bed — a drone texture | `mistress` |

## tape — `tape(wow, flutter, saturation)` · `instrument_tape(slot, …)`

Vintage analog: `wow` (slow pitch drift) + `flutter` (fast warble) + `saturation` (warm clip + HF
rolloff). Master or per-instrument. **Showcase: `tapeloop`.**

| recipe | call | character | used by |
|---|---|---|---|
| worn tape | `tape(0.65f, 0.45f, 0.50f)` | pronounced wobble + warmth — the most degraded setting | `mellotron`, `wowflutter` |
| warm & drifting | `tape(0.22f, 0.11f, 0.34f)` | gentle vintage glue across the whole mix | `air`, `groovebox` (TAPE knob), `wba` |
| clean & tight | `tape(0.10f, 0.08f, 0.24f)` | barely-there warmth, no audible pitch drift | `air` |
| lo-fi just the drums | `instrument_tape(SL_DRUMS, 0.4f, 0.5f, 0.7f)` | wonky tape on the kit, synths stay clean | (per-instrument pattern) |

## auto-wah — `wah(sensitivity, resonance, mix)` · `instrument_wah(slot, …)` · LFO: `wah_lfo(rate, resonance, mix)` · `instrument_wah_lfo(slot, …)`

A resonant bandpass swept on the bus — two flavors of the same insert. **Envelope** (`wah`): the band
opens with how hard you play (the funk quack, dynamics-driven). **LFO** (`wah_lfo`): a sine rocks the
band on its own at `rate` Hz — the hands-free rhythmic "wah-wah", ignores dynamics. Both share the one
`FX_WAH` bus bandpass (so a bus is in *one* mode at a time — calling `wah` switches to envelope,
`wah_lfo` to LFO); both are set-and-hold (the LFO runs inside the engine). Best on ONE rich/percussive
part. **Showcase: `clavinet`** (envelope); **`pedalboard`** WAH pedal exposes both via its MOD knob.

| recipe | call | character | used by |
|---|---|---|---|
| talking clav | `instrument_wah(SL_CLAV, 0.6f, 0.7f, 0.85f)` | the Hohner D6 quack — opens on the stab, shuts between | `clavinet` |
| wah-bar sweep | `instrument_wah(slot, 0.4+amt*0.6, 0.45+amt*0.4, 0.75+amt*0.25)` | one knob `amt` 0→1 ramps the whole effect in | `epiano` |
| envelope auto-wah | `wah(0.5f, 0.55f, 0.7f)` | dynamics-driven funk quack on the whole bus — dig in and it opens | `pedalboard` (WAH, MOD→ENV) |
| LFO auto-wah | `wah_lfo(0.5f+rate*9.5f, 0.55f, 0.7f)` | hands-free rhythmic wah rocking on its own at the set rate | `pedalboard` (WAH, MOD→LFO) |

## formant — `formant(vowel, q, mix)` · `instrument_formant(slot, vowel, q, mix)`

The VOWEL filter — four bandpasses parked at the human formant frequencies (F1–F4), so any source
takes on an "ooh/aah/eee" vocal colour. `vowel` 0–1 sweeps **OO→OH→AH→EH→EE** (sweep it and a synth
talks); `q` 0–1 narrows the peaks (broad → pronounced/nasal); `mix` 0–1 (0 = off). A wah is the
one-peak version of this; this is the multi-peak vowel. **Single-input** (the talkbox family, knob
for a mouth) — *not* a vocoder (that needs a second carrier×modulator input — a future effect once
the sidechain lands). Reuses navkit's measured vowel table (shared with `INSTR_VOICE`). Master or
per-instrument. A sweeping `vowel` every frame is fine (it's a coefficient update, no buffer churn —
unlike crush/tape). **Showcase: `vowel`** (a saw chord that talks).

| recipe | call | character | used by |
|---|---|---|---|
| talking saw | `formant(swept 0→1, 0.6f, 0.95f)` | a buzzing chord that says its vowels as the knob moves — the talkbox move | `vowel`, `pedalboard` (MANUAL mode) |
| nasal "ee" lead | `instrument_formant(slot, 1.0f, 0.8f, 0.85f)` | a pinched, nasal vocal colour on one lead/pad, rest of the mix plain | (per-instrument pattern) |
| open "ah" pad | `instrument_formant(I_PAD, 0.5f, 0.5f, 0.7f)` | a soft choral "ah" colour on a held pad — broad peaks, gentle | (pad pattern) |
| randomized crowd vowels | `instrument_formant(slot, base[i]±jitter + excite·k, ~0.6f, 0.9f)` — base re-rolled per note-on, vowel re-pushed per frame | a few voices each on a different, slightly-random vowel that OPENS with intensity — a crowd going "ah… AAAH/eee" the scarier it gets, not one unison tone | `coaster` (roller-coaster riders) |
| syllable-per-pluck | `formant()` re-pushed each note-on with the next vowel in a list, glided | each strum speaks a new vowel — the guitar recites a "word" (cart-side: `formant()` has no trigger input, so the cart advances the vowel on each pluck) | `pedalboard` (STEP mode) |
| picking-driven open | `formant()` driven by a strum envelope (vowel opens on attack, relaxes) | hands-free vocal swell — the auto-wah gesture wearing a vowel | `pedalboard` (ENV mode) |
| auto-sweep LFO | `formant()` re-pushed each frame with `vowel = 0.5−0.5·cos(phase)` | the vowel rocks OO↔EE on its own at a set rate — the rhythmic "yoy-yoy" wobble, hands-free | `pedalboard` (LFO mode), `vowel` (AUTO) |

## drive — `instrument_drive(slot, amount)` · `note_drive` · `instrument_drive_mode(slot, mode)`

Per-voice saturation AFTER the filter (so resonance screams into it — the grit knob). `amount` 0–1;
**`instrument_drive_mode(slot, DRIVE_*)` picks the waveshaper.** All four bypass cleanly at 0 and hold
full-scale loudness (character, not volume). **Showcase: `drivemodes`.**

| recipe | call | character | used by |
|---|---|---|---|
| acid squelch | `FILTER_LOW` + high res + `instrument_drive(slot, 0.3–0.5f)`, live `note_drive` | the 303: resonant peak driven into saturation; ride DRV with RES | `tb303`, `moog`, `spacecho`, `modrack` (DRIVE module — drives the VOICE slots post-filter; mode knob = soft/hard/fold/asym) |
| distorted house lead | `instrument_drive(I_LEAD, 0.45f)` [SOFT] | the genre hook — a hard, present saw lead | `house` |
| warehouse kick | `instrument_drive(SL_KICK, 0.30–0.35f)` | saturated kick weight, the French-house pump | `tr909`, `tr808`, `italo`, `house` |
| bass grind | `instrument_drive(I_BASS, 0.40f + fuzz*0.0055f)` (0.40–0.95) | a fuzz knob mapped onto a bass, clean→wall | `air` |
| **guitar grit** (mode) | `instrument_drive_mode(slot, DRIVE_ASYM); instrument_drive(slot, 0.5f)` | asymmetric tube — fat **even** harmonics, the round single-ended-amp warmth | `drivemodes`, `combo` (CHIME/CRUNCH) |
| **digital fuzz** (mode) | `DRIVE_HARD` + amount 0.6+ | buzzy, square-edged, harsh — chiptune lead bite | `drivemodes`, `combo` (HI-GAIN) |
| **metallic / ring-ish** (mode) | `DRIVE_FOLD` + amount swept up | sine wavefolder — glassy, clangy as you push it | `drivemodes`, `combo` (LO-FI) |
| bark → drive | `note_drive(h, bark)` live, slewed | an EP/organ that digs in when you lean on a knob | `epiano`, `organ`, `modrack` |
| fuzz pedal | per-voice drive on the guitar slot: `DRIVE_ASYM` (germanium, warm) / `DRIVE_HARD` (silicon, bite), amount ~0.45–1.0 | the Fuzz Face — a stompbox dirt source. It's per-voice (pre-chain), and the amp cabinet now drives the BUS (`drive_insert`/`FX_DRIVE`, end of chain), so FUZZ → amp **stacks** (no lock) — Fuzz Face into a cranked amp | `pedalboard` (FUZZ) |

### mix-bus saturation — `drive_insert(amount, mode, mix)` · `FX_DRIVE`

The whole-MIX sibling of per-voice drive: it saturates the summed master bus, so the **drums grit up too**
(per-voice drive only touches the one voice it lives in). Reuses the same `DRIVE_*` shapers; place `FX_DRIVE`
in `fx_order(0,…)` (before crush/echo vs after is audible). `mix` 0 = bypass → byte-identical. Use it for
glue/cohesion, NOT as a substitute for per-voice tone-shaping (which keeps the post-filter acid scream).

| recipe | call | character | used by |
|---|---|---|---|
| tube glue | `drive_insert(0.25f, DRIVE_ASYM, 0.5f)` | gentle even-harmonic warmth that fuses a busy mix — the "everything through one amp" cohesion | `modrack` (SAT module) |
| lo-fi wall | `drive_insert(0.7f+, DRIVE_HARD, 0.85f)` | cranked square-edged crush on the whole bus, drums and all — noise-rock / breakbeat grit | `modrack` (SAT, "Sat bus" preset) |
| two bus drives (×2) | `drive_insert(...)` (inst 0) + `drive_insert_inst(1, ...)`; chain `{FX_INST(FX_DRIVE,1), …, FX_DRIVE}` | two INDEPENDENT bus drives at different chain spots (Increment F) — an overdrive pedal feeding the amp's own drive (boost → amp stack) | `pedalboard` (the OD pedal + the AMP cabinet) |
| **Tube Screamer** | `drive_insert(0.6f, DRIVE_ASYM, 1.0f)` + `drive_voice(DRIVE_VOICE_TS, tone)` | the Ibanez TS: bass stays clean/tight, mids soft-clip, the famous MID HUMP cuts through a band — the most-copied overdrive. `tone` 0..1 = dark→bright (the TONE knob), ride it live. `DRIVE_VOICE_NONE` = the plain clip (byte-identical) | `tubescreamer` (OVERDRIVE/TONE/LEVEL + B = A/B the hump) |

> **`drive_voice(voice, tone)` — famous-pedal VOICE on the drive insert.** The drive shapers give you the
> *clip*; a real overdrive's character is the *filtering around it*. `DRIVE_VOICE_TS` wraps `DRIVE_ASYM` with a
> pre-split (bass bypasses the clipper = tight lows) + a post-LP TONE control, yielding the mid hump. Global,
> gated (`DRIVE_VOICE_NONE` default → byte-identical). **Three voices ship:** `DRIVE_VOICE_TS` (mid hump),
> `DRIVE_VOICE_RAT` (full-range hard clip + low-pass FILTER — aggressive ProCo RAT distortion), and
> `DRIVE_VOICE_MUFF` (cascaded clip + mid SCOOP — the EHX Big Muff fuzz wall, the anti-TS). `tone` rides
> each voice's tone control live.

## bitcrush — `crush(bits, rate, mix)` · `instrument_crush(slot, bits, rate, mix)`

Lo-fi quantization: drop the **bit depth** (`bits` 1–16, floor to 2^bits levels) and the **sample
rate** (`rate` 1–64, sample-and-hold every Nth sample). Master or per-instrument. Dormant until first
call (mix 0 = off). **Showcase: `bitcrush`** (its scope doubles as an XY pad — X=rate, Y=bits).

| recipe | call | character | used by |
|---|---|---|---|
| 8-bit master | `crush(6.0f, 8.0f, 1.0f)` | crunchy downsampled whole-mix grit — the retro-console sound | `bitcrush`, `groovebox` (CRUSH knob — the summed SP-1200 grit on a full mix), `modrack` (CRUSH module — cv inlet gates the grit rhythmically) |
| gnarly destroy | `crush(2.0f, 16.0f, 1.0f)` | barely-recognizable, steppy quantization noise | `bitcrush` |
| subtle dirt | `crush(10.0f, 2.0f, 0.5f)` | a touch of aliasing whine + step, mostly clean | `bitcrush` |
| lo-fi just the bass | `instrument_crush(I_BASS, 5.0f, 6.0f, 1.0f)` | crush ONE part (the bass) while the lead stays crystal — the point of the per-instrument bus | `bitcrush` |

> Quantization is round-to-nearest (`floorf(x·levels + 0.5f)`), symmetric about zero — no DC bias,
> and a decaying note fades to true silence through the crusher (the old truncating `floorf` held the
> tail at a constant −1-LSB buzz until the voice gated). See [`audio-notes.md §17`](../design/audio-notes.md).

## dropout — `dropout(amount, depth)`

The VHS / **Generation-Loss "FAILURE" knob**: random momentary **tape-catches** on the whole mix — a
sample-&-hold clock (`mod_sh`, the modulation kit) rolls the dice each step and, when it hits, the mix
**stumbles**: drops in level AND goes dull (HF loss), then recovers in ~25 ms. `amount` 0–1 = how
**often** the tape catches (`P(catch)` per step), `depth` 0–1 = how **hard** (level dip + how dull).
A **master-stage** effect (runs at the sum, before the soft-clip — *not* a reorderable `FX_*` insert,
since the packing is full and a whole-mix failure belongs at the master). amount 0 = off (byte-identical).
**Showcase: `genloss`** (crush→tape→dropout, with a VHS tape-transport that tears its tracking on a catch).

| recipe | call | character | used by |
|---|---|---|---|
| worn cassette | `crush(6,4,0.4f)` + `tape(0.3f,0.2f,0.4f)` + `dropout(0.4f, 0.7f)` | the full degraded-tape chain: gritty, warbly, stumbling | `genloss` |
| occasional stumble | `dropout(0.15f, 0.6f)` | a catch every few seconds — subtle "this tape has been played a lot" | `genloss` |
| broken deck | `dropout(0.8f, 0.95f)` | constant gating/stuttering — the deck is eating the tape | `genloss` |

> **Not dither, not a gate.** Dither (a quantization fix) and a noise gate (cuts the floor *between*
> notes) are different tools; `dropout` *adds* failure — it removes signal at random. v1 is level + HF
> loss; the iconic **pitch-dip** on a catch waits for the bus pitch-shifter (Primitive 2). Built on the
> modulation kit's sample-&-hold — see [`boutique-pedals-roadmap.md`](../design/boutique-pedals-roadmap.md).

## amp_noise — `amp_noise(hiss, hum, mains_hz)`

The **optional rig-noise floor**: broadband **hiss** (tube/thermal) + a **50/60 Hz mains hum** (the
single-coil buzz) sitting under the whole mix — the "an electric guitar is never truly silent"
character. **Entirely opt-in** — `hiss 0 && hum 0` = silent and byte-identical (a fantasy console is
pristine by default; you add the grime only when a track wants it). A *constant* master-output floor:
added **after** the limiter so it never ducks or clips with the mix, present even in dead silence.
Stereo-decorrelated hiss (width) + centered mono hum. For realism, scale `hiss` with your amp's gain.
**Showcase: `ampnoise`** (an `N` toggle A/Bs the floor on/off — the whole point). Pairs with a noise
gate (next) to clamp the floor between notes.

| recipe | call | character | used by |
|---|---|---|---|
| humming tube amp | `amp_noise(0.3f, 0.2f, 60)` | a quiet, alive floor under a clean amp — lo-fi authenticity | `ampnoise` |
| noisy single-coil | `amp_noise(0.4f, 0.6f, 60)` | strong 60-cycle buzz (a Strat near a monitor) — switch `60→50` for EU | `ampnoise` |
| gentle tape-room air | `amp_noise(0.5f, 0.0f, 60)` | hiss only, no hum — a bed of "air" under ambient/lo-fi without the electrical buzz | `ampnoise` |
| pristine (off) | `amp_noise(0.0f, 0.0f, 60)` | the default — dead silent, byte-identical. The point: it's a choice | — |

> **Optional by design.** Unlike most effects this is a *floor*, not a transform — it's most musical
> as a quiet bed (`hiss`/`hum` ~0.2–0.4). Reach for it on lo-fi, ambient, and amp-sim tracks; leave it
> at 0 everywhere else. The companion **noise gate** clamps it (and real signal) between notes.

## gate — `gate(threshold, attack_ms, release_ms)` · `instrument_gate(slot, …)`

A NOISE GATE: an envelope follower + threshold that **clamps the signal shut** when it falls below
`threshold` and opens above it. The classic rig pedal — tame the hiss/hum and ringing tails of a noisy
or high-gain part between notes — and, placed **AFTER reverb** in `fx_order`, it chops the tail for the
iconic **80s gated reverb** (the Phil Collins snare). A reorderable insert (`FX_GATE` = kind 17).
`threshold` 0–1 (**0 = always open / bypass**, byte-identical; higher closes sooner), `attack_ms`
(open speed, ~1–10), `release_ms` (close speed / how fast the tail cuts, ~30–300). Master or per-instrument.

| recipe | call | character | used by |
|---|---|---|---|
| gated reverb | `FX_REVERB`→`FX_GATE` in `fx_order` + `reverb_insert(0.9,0.3,0.7)` + `gate(0.4f, 2, 60)` | the 80s big-snare bloom that slams shut — reverb tail energy drops ~5× | `pedalboard` |
| tighten a noisy lead | `instrument_gate(I_LEAD, 0.4f, 3, 100)` | clamp a driven/hissy part's tail so it's silent between phrases | `pedalboard` |
| stutter chop | `gate(0.5f, 1, 25)` on a sustained pad | fast release + a moving source = rhythmic gating | — |

> **Gate vs sidechain/glue.** All three are dynamics, but a gate is a *threshold switch* (open/shut on
> level), where `sidechain`/`glue` *duck* continuously. Use the gate to remove noise/tails; use
> sidechain for the pump. Pairs with `amp_noise` — though note the gate (a pre-output insert) clamps the
> signal path, not the post-limiter `amp_noise` floor; gate a noisy *part*, or use it for gated reverb.

## varispeed — `varispeed(speed)`

Variable **tape playback speed** of the whole mix (the Chase Bliss MOOD "clock" / a turntable brake;
navkit's `half_speed` ported). Writes the final output to a buffer and re-reads it at `speed`: **1.0 =
bypass** (byte-identical), **<1 = slower** (pitch DOWN + time-stretch — the tape-slowdown dive), **>1 =
faster** (chipmunk). 0.25–4 = two octaves down to two up. The applied speed **glides** (tape inertia,
no zipper), so it's built to be **swept** — not held at a fixed off-speed (a held off-speed eventually
laps the ~2 s buffer → a click). Master output stage; pitch is exact (A4→A3 at 0.5, measured 0¢).
**Showcase: `varispeed`** (a riff + SPACE tape-stop dive + a SPEED bend slider).

| recipe | call | character | used by |
|---|---|---|---|
| tape-stop divebomb | sweep `varispeed(1.0 → ~0.28)` then back to `1.0` | the whole mix brakes to a crawl + pitch dives, then spins back up — the SP-404 / turntable stop | `varispeed`, `kaoss` (TAPE program — pad x = `pow(4,(x−.5)·2)`, drag left to dive) |
| chipmunk / fast-forward | `varispeed(2.0f)` (momentary) | everything an octave up + double-time | `varispeed` |
| seasick bend | sweep `varispeed` ±a little around 1.0 | a wobbly tape warble on the whole mix | `varispeed`, `vapor` (the slowed-tape WOBBLE, ridden live every frame) |

> **Sweep it, don't hold it.** `varispeed` is a *gesture* (a dive/bend/spin), not a static pitch shift —
> hold a fixed off-speed and the read laps the write (a periodic click). For a clean *sustained* octave
> on one part, that's `grains_pitch` / shimmer territory; `varispeed` is the moving-tape effect.
>
> ✅ **FIXED (2026-06-22):** two bugs, both diagnosed + fixed + measured. (1) Moderate speeds
> (~0.33×–1.67×) didn't pitch-shift — the deadband dry/reset branch slammed the slewed speed back to
> 1.0 every sample, so the slew never accumulated out of the band for a moderate target; only the
> extremes escaped in one step. (2) Speeding up was silent for ~1 s — the read head ran ahead of the
> write head into the un-recorded region; the ring now rolls continuously once the cart uses varispeed,
> so a speed-up reads real recent audio. Validated end-to-end: 1.3× on A4 → 571.8 Hz, full level. Full
> writeup: [`../design/audio-notes.md`](../design/audio-notes.md) §24.

## modulating effects — `fx_mod(bus, target, value)` · `instrument_fx_mod(slot, …)` · `fx_lfo(bus, target, rate, depth, center, shape)`

Not an effect — the **modulation layer over** effects (ADR 0018). Effects keep their own set-and-hold
knobs; this RIDES a curated, *sweep-safe* one under a control signal, the way `LFO_TIMBRE` rides an
instrument macro. **Configure the effect first** (`filter()`/`drive_insert()`/`grains()`/`shimmer()`/
`tremolo()`/`autopan()`); the modulation moves its param on top — it does **not** enable the effect (a
mod on an un-configured bus is silent). Two entry points:

- **`fx_mod(bus, target, value)`** — the per-frame **CV sink**. Push your own LFO/envelope/sequencer
  value every frame (this is what `modrack` patches a CV node into); the engine slews internally so
  per-frame pokes never zipper. `instrument_fx_mod(slot, …)` addresses an instrument's private bus.
- **`fx_lfo(bus, target, rate_hz, depth, center, shape)`** — fire-and-forget **engine LFO**. Set once;
  runs on the audio thread (no per-frame calls). `depth` 0..1 = peak deviation around `center`; **`depth
  0` = detach** (the param freezes at its last value). `depth 0.5` + `center 0.5` = a full 0..1 swing.
  `shape` is any `LFO_SHAPE_*` (SINE/SQUARE/TRI/SAW/RAMP/OPTICAL/SH/RANDOM — STATUS #39); SQUARE on
  `FILTER_CUT` = a stepped filter, S&H = per-step jumps.

`value`/`center`/`depth` are all normalized 0..1, mapped per target. **Targets (`FXMOD_*`):**

| target | rides | mapping |
|---|---|---|
| `FXMOD_FILTER_CUT` | `filter()` cutoff — the marquee DJ sweep | exponential 40 Hz → 18 kHz |
| `FXMOD_FILTER_RES` | `filter()` resonance | linear 0..1 |
| `FXMOD_DRIVE` | `drive_insert()` amount (needs `mix>0`) | linear 0..1 |
| `FXMOD_TREM_DEPTH` | `tremolo()` depth | linear 0..1 |
| `FXMOD_PAN_DEPTH` | `autopan()` depth | linear 0..1 |
| `FXMOD_GRAINS_MIX` | `grains()` dry/wet (call `grains()` first) | linear 0..1 |
| `FXMOD_SHIMMER_MIX` | `shimmer()` dry/wet | linear 0..1 |

| recipe | call | character | used by |
|---|---|---|---|
| auto-wah-ish filter sweep | `filter(FILTER_LOW,…)` then `fx_lfo(0, FXMOD_FILTER_CUT, 0.3, 0.4, 0.5, LFO_SHAPE_SINE)` | a slow cyclic lowpass rock — the cheap wah | `fxmod` |
| stepped filter | `filter(FILTER_LOW,…)` then `fx_lfo(0, FXMOD_FILTER_CUT, 2, 0.4, 0.5, LFO_SHAPE_SH)` | random per-step cutoff jumps — the rhythmic S&H filter | `lfoshapes` |
| hand-driven filter (modrack) | per frame: `fx_mod(0, FXMOD_FILTER_CUT, cv)` | the DJ filter, swept by your own CV/knob/envelope | `fxmod` |
| breathing dirt | `drive_insert(0,SOFT,1)` then `fx_lfo(0, FXMOD_DRIVE, 0.2, 0.3, 0.5, LFO_SHAPE_SINE)` | saturation that swells and backs off | `fxmod` |
| blooming wash | `shimmer(…,0)` then `fx_mod(0, FXMOD_SHIMMER_MIX, env)` | swell the ascending shimmer in on a gesture | `fxmod` |

> **Deferred targets (not yet built):** reverb/delay **sends** (they're per-*voice* in this engine, not
> per-bus — modulating them needs a new bus-level return-gain knob first) and **wah position** (wah is
> envelope/LFO-driven — needs a manual-sweep mode first). The `FXMOD_*` enum leaves room to append them.
> Rationale: ADR 0018 "Shipped"; tracked in STATUS #39. **Showcase: `fxmod`** (both APIs × three targets).

## EQ — `eq(low_gain, mid_gain, high_gain)` · `instrument_eq(slot, low_gain, mid_gain, high_gain)`

3-band tone control, and **the library's only BOOST** — every other tone tool (the SVF filters)
can only cut. Three bands split at ~80 Hz and ~6 kHz — LOW (`<80 Hz`, body/sub) / MID (`80 Hz–6 kHz`,
the meat of most sounds) / HIGH (`>6 kHz`, presence/air) — each `±12 dB` (+ = boost, − = cut,
**0/0/0 = flat = off**, byte-identical to no EQ). Master (whole mix) or per-instrument. The classic
partner to `DRIVE_ASYM`: EQ before/after a clipper = a real **guitar-amp tone**. **Showcase: `eq`**
(drag the LOW/MID/HIGH nodes in the grid, the response curve bends to follow).

| recipe | call | character | used by |
|---|---|---|---|
| warm & dark | `eq(4.0f, 0.0f, -3.0f)` | lift the body, tame the fizz — cozy/lo-fi | `eq` |
| smile (loudness) | `eq(6.0f, -2.0f, 5.0f)` | boosted lows AND highs, scooped mids — the hi-fi "smile" curve | `eq` |
| mid scoop (metal) | `eq(3.0f, -8.0f, 4.0f)` | gut the mids for a scooped, aggressive rhythm tone | `eq` |
| air / presence | `instrument_eq(slot, 0.0f, 0.0f, 6.0f)` | open up a dull lead or pad with top-end sparkle, rest untouched | `eq` |
| fat bass | `instrument_eq(I_BASS, 7.0f, 0.0f, 0.0f)` | weight under one part while the mix stays clear (per-instrument bus) | `eq` |
| guitar-amp tone | `instrument_drive_mode(slot, DRIVE_ASYM); instrument_drive(slot, 0.55f); eq(3.0f, 2.0f, -4.0f)` | asymmetric clip + a mid-forward EQ around it — the cranked-amp move | `eq`, `combo` (the CRUNCH voicing) |
| mud cut | `eq(-5.0f, 1.0f, 2.0f)` | thin out a boomy low end, nudge clarity up top | (cut pattern) |
| live 3-band | `eq((lo-0.5f)*24, (mid-0.5f)*24, (hi-0.5f)*24)` | three knobs over the whole mix, ±12 dB each (0.5 = flat = off). A *louder* EQ is what makes the `fx_order` crush↔eq order audible (a gentle tilt isn't enough to hear) | `groovebox` (LO/MID/HI) |
| EQ before AND after dirt (×2) | `eq(...)` (inst 0) + `eq_inst(1, ...)`; chain `{FX_EQ, FX_CRUSH, FX_INST(FX_EQ,1)}` | two INDEPENDENT EQs in one chain (Increment F) — shape what hits the crush/drive vs shape its output. The pre-EQ changes how the nonlinearity bites; the post-EQ tames the result | `pedalboard` (the EQ2 pedal) |

## tremolo — `tremolo(rate, depth, shape)` · `instrument_tremolo(slot, rate, depth, shape)`

A volume LFO that ducks the level up and down — the Fender/Wurlitzer **amp wobble**, the "electric
piano" throb. `rate` 0.1–20 Hz, `depth` 0–1 (0 = off; only attenuates, never boosts — like a real
amp: `out = in·(1 − depth·(1 − lfo))`), `shape` `TREM_SINE` (smooth, default) / `TREM_SQUARE` (hard
chop) / `TREM_TRI` (ramp). A VERBATIM port of navkit's `processTremolo` (A/B-matched on a sine —
rate + depth dead-on). **One shared LFO phase per bus**, so a per-instrument tremolo wobbles that
instrument's WHOLE output in unison — the coherent amp wobble a per-voice `LFO_VOLUME` can't give
(there each note has its own phase = a shimmer, not a throb). Master or per-instrument. **Showcase:
`epiano`** (the Rhodes/Wurli throb, off on the clav). Dormant until depth>0 → non-users byte-identical.

| recipe | call | character | used by |
|---|---|---|---|
| Wurlitzer 200A throb | `instrument_tremolo(I_EP, 5.0f+amt*1.5f, amt*0.85f, TREM_SINE)` | the soul-ballad EP wobble — deeper than the Rhodes; the tremolo slider rides rate+depth together | `epiano` |
| suitcase Rhodes wobble | `instrument_tremolo(I_EP, 5.4f, 0.38f, TREM_SINE)` | the classic gentle ~5 Hz suitcase pulse | `epiano` |
| helicopter chop | `tremolo(8.0f, 0.9f, TREM_SQUARE)` | hard on/off gate on the whole mix — stutter/strobe | `kaoss` (GATE program — quantized rate 2..16 Hz, depth on y; re-applied only on change since `tremolo()` resets its LFO phase per call) |

> A real clav has **no** tremolo — the `epiano` clav preset sets depth 0 (bypass). Tremolo is the
> Rhodes/Wurli signature, not a universal EP effect.

## auto-pan — `autopan(rate, depth, shape)` · `instrument_autopan(slot, rate, depth, shape)`

The **stereo sibling of tremolo**: the same amplitude LFO, but applied **antiphase** to L/R, so the
sound sweeps side to side instead of throbbing in place — the **Rhodes Suitcase stereo vibrato**, the
auto-pan stompbox, rotary-style motion. The left gain is the plain tremolo gain `1 − depth·(1 − lfo)`,
the right its complement `1 − depth·lfo` (depth 1 = a channel hits silence = full L↔R). `rate` 0.1–20
Hz, `depth` 0–1 (0 = off, 1 = full width), `shape` `TREM_SINE` (smooth glide, default) / `TREM_SQUARE`
(hard ping-pong) / `TREM_TRI` (linear sweep). Only attenuates (never boosts), so the summed level
never exceeds the dry input. **Its OWN insert (`FX_PAN`), not a mode of tremolo** — so it STACKS with
`tremolo` on one bus (a fast amplitude throb AND a slow stereo drift at once, two independent LFOs),
and on the pedalboard it's a real reorderable pedal beside TREMOLO. Master or per-instrument. Dormant
until depth>0 → non-users byte-identical. (Verified: a centered mono source reads correlation +1.0;
with auto-pan, 0.33 / −1.78 dB mono-fold = real width, and a slow sweep swings the balance ±0.56 dB.)

| recipe | call | character | used by |
|---|---|---|---|
| Suitcase stereo vibrato | `instrument_autopan(I_EP, 3.0f+spd*4.0f, 0.35f+dep*0.55f, TREM_SINE)` | the Rhodes Suitcase L↔R sway — the whole instrument incl. the tine tails pans coherently | `epiano` (VIBE pedal) |
| slow stereo drift | `autopan(0.4f, 0.8f, TREM_SINE)` | a wide, lazy wash across the mix — great stacked under a `tremolo` throb | `pedalboard` (AUTOPAN) |
| hard ping-pong | `autopan(4.0f, 1.0f, TREM_SQUARE)` | the sound jumps L/R/L/R — a rhythmic stutter-pan | `pedalboard` (AUTOPAN, WAV→square) |

> **Why not a flag on `tremolo()`?** Because then a bus could only ever throb OR pan, never both, and a
> "separate function, same state" API would *look* combinable but silently isn't. A distinct `FX_PAN`
> insert is the honest, composable form — two LFOs, two pedals. See
> [`../design/effects-bus-architecture.md`](../design/effects-bus-architecture.md) §0.

## ring mod — `ringmod(freq_hz, mix)` · `instrument_ringmod(slot, freq_hz, mix)`

Multiply the signal by a sine **carrier** at `freq_hz` → inharmonic **sum/difference sidebands**: the
metallic, clangorous, robotic/bell texture (the Dalek voice, the sci-fi clang, Sabbath's "Paranoid"
solo). `out = in·((1−mix) + mix·carrier)`. The thing that makes it *not* tremolo: the carrier is
**bipolar** (−1..1), so it adds genuinely NEW frequencies instead of just wobbling the volume — a
440 Hz sine ring-modded at 150 Hz loses its 440 and becomes 290 + 590 Hz. `freq` 1–8000 Hz (**low**
~2–30 Hz = a throbby tremolo-ish AM; **mid/high** ~100–2000 Hz = the atonal clang), `mix` 0–1 (0 = off).
`|out| ≤ |in|`, so no added clip risk. A reorderable insert (`FX_RINGMOD`); master or per-instrument.
Dormant until mix>0 → non-users byte-identical. (Verified: dry passes a 440 Hz sine at conf 1.0; wet
moves the fundamental off 440 with confidence collapsing as the sidebands appear, RMS −20→−23 dB.)

| recipe | call | character | used by |
|---|---|---|---|
| metallic clang | `ringmod(440.0f, 0.8f)` | bright inharmonic bell/gong tone — atonal, alien | `pedalboard` (RINGMOD) |
| robot voice / Dalek | `ringmod(30.0f, 1.0f)` on a vocal/lead | the low-carrier buzz that turns any voice robotic | `pedalboard` (FRQ low) |
| throbby AM | `ringmod(6.0f, 0.7f)` | a very-low carrier = a rough tremolo-like pulse (the AM end of the range) | `pedalboard` (FRQ near 0) |

> **Ring mod vs tremolo:** both modulate amplitude, but tremolo's LFO is *unipolar* (gain 0..1, no new
> tones — a wobble) while ring mod's carrier is *bipolar* (adds sidebands — a new timbre). That's why
> it's its own effect, not a tremolo mode (it clears [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md):
> a recipe of the existing roster can't make sidebands).

## phaser — `phaser(rate, depth, feedback, mix, stages)` · `instrument_phaser(slot, …)`

A chain of allpass filters swept by an LFO carves moving NOTCHES in the spectrum — the **70s
electric-piano / Small Stone swirl** (vocal, hollow; softer + rounder than a flanger's metallic comb).
`rate` 0–10 Hz (sweep speed; ~0.3–0.7 for the classic slow swirl), `depth` 0–1 (how far the notches
travel), `feedback` −0.95..0.95 (resonance around the notches), `mix` 0–1 (**0 = off**, and note
**0.5 is the deepest** — an all-wet allpass has no notches, the notches form in the dry+wet sum),
`stages` 2–8 (notch count — **4 = the classic Phase-90**, more = thicker). A VERBATIM port of navkit's
`processPhaser` (A/B-matched sample-for-sample, 0.99999 correlation). Master or per-instrument.
**Showcase: `epiano`** (the `P` toggle; on by default on the `suitcase` preset). Dormant until mix>0.

| recipe | call | character | used by |
|---|---|---|---|
| phased suitcase Rhodes | `instrument_phaser(I_EP, 0.5f, 0.8f, 0.4f, 0.5f, 4)` | the slow 4-stage Phase-90 swirl on an EP — the 70s signature | `epiano` |
| krautrock phased drone | `instrument_phaser(I_ORG, 0.12f, 1.0f, 0.6f, 0.5f, 6)` | very slow (~8s/sweep), deep, resonant on a held organ drone — the Neu!/Cluster hypnotic swirl; extreme in depth, gentle in rate so it evolves *with* the motorik pulse, not against it | `motorik` |
| deep 6-stage swirl | `phaser(1.0f, 1.0f, 0.6f, 0.5f, 6)` | thicker, more notches, more resonant — a wider whole-mix sweep | (6-stage pattern) |
| through-zero-ish | `phaser(0.3f, 0.9f, -0.6f, 0.5f, 4)` | negative feedback hollows the notches differently — a more "inside-out" sweep | (neg-fb pattern) |

> **Phaser vs flanger:** both sweep and whoosh, but a phaser is allpass NOTCHES (vocal, smooth — the
> Rhodes/Small Stone) and a flanger is a swept COMB with delay (metallic, jet-like — needs a rich
> source). For an electric piano, reach for the phaser.

## univibe — `univibe(rate, depth, mix)` · `instrument_univibe(slot, …)`

The 60s photocell VIBE (Univibe / Shin-ei) — the **same 4-stage allpass chain as the phaser, but
swept by the OPTICAL LFO** (`mod_optical`, the modulation-kit shape) instead of a sine. A lightbulb
glued to a photocell glows *slowly bright then snaps dim*, so the sweep is **asymmetric and liquid**
where a sine phaser is even and clinical — that asymmetry is the whole sound (Machine Gun / Bridge of
Sighs). Classic **4-stage, no feedback** (it's photocell phase shift). `rate` 0–10 Hz, `depth` 0–1,
`mix` 0–1 (0 = off; like the phaser, 0.5 is deepest — notches form in the dry+wet sum). **Shares the
`FX_PHASER` insert** — don't run `phaser()` and `univibe()` on the same bus. First consumer of the
modulation kit (boutique-pedals roadmap Primitive 1). **Showcase: `univibe`** (with a `P` A/B toggle
vs the sine phaser, and a throbbing lamp + live LFO-curve so you SEE the asymmetry).

| recipe | call | character | used by |
|---|---|---|---|
| slow organ vibe | `univibe(4.0f, 0.7f, 0.5f)` | the liquid 60s throb on a held organ/chord bed | `univibe` |
| deep slow swirl | `univibe(0.6f, 1.0f, 0.5f)` | very slow, wide — a hypnotic asymmetric sweep that evolves with the music | `univibe` |
| per-part vibe | `instrument_univibe(I_ORGAN, 5.0f, 0.8f, 0.5f)` | vibe just the organ/EP, leave the rhythm dry | — |

> **Univibe vs phaser:** identical DSP, different LFO. The phaser's sine sweep is symmetric; the
> univibe's optical sweep eases up and drops fast. On chords/organ the vibe feels *liquid* and
> *vocal*; the phaser feels *even* and *mechanical*. The `univibe` cart's `P` toggle A/Bs them at
> matched rate/depth so you hear only the LFO-shape difference.

## shallow — `shallow(rate, depth, mix)` · `instrument_shallow(slot, …)`

Fairfield **Shallow Water**: a filtered-random ("K-field") short delay + a **Low Pass Gate**. A
random WALK (`mod_randwalk`, the modulation kit — *not* a periodic LFO) drifts the delay time, so the
pitch wobbles gently and **unpredictably** — the warped-tape / reflection-on-moving-water shimmer that
a sine LFO can't fake. Then a Buchla-style **LPG** opens cutoff AND level together: a steady signal
keeps the gate open (full + bright), but a quiet/decaying passage goes **dark + soft and blooms back**
as it swells — the "underwater" close. A reorderable insert (`FX_SHALLOW` = kind 16, first past the
old ceiling). `rate` 0.2–8 Hz (drift speed), `depth` 0–1 (warble amount), `mix` 0–1 (0 = off; 0.5
keeps the dry present with the watery wobble on top). **Showcase: `pedalboard`** (the SHALLOW pedal).

| recipe | call | character | used by |
|---|---|---|---|
| watery pad drift | `shallow(0.8f, 0.6f, 0.5f)` | slow unpredictable wobble + LPG bloom under a held pad/EP — the ambient default | `pedalboard` |
| seasick warble | `shallow(3.0f, 0.9f, 0.6f)` | faster, deeper drift — queasy detuned shimmer on clean guitar | `pedalboard` |
| subtle movement | `shallow(0.4f, 0.3f, 0.35f)` | a touch of organic drift, barely-there — "this isn't a static patch" | — |

> **Shallow vs chorus.** Chorus is a *periodic* swept delay (predictable, even). Shallow's delay is
> swept by a *random walk*, so it never repeats — and the Low Pass Gate ties brightness+level to the
> envelope (quiet = dark, the bloom), which chorus has no notion of. First insert built on the
> modulation kit's filtered-random source — see [`boutique-pedals-roadmap.md`](../design/boutique-pedals-roadmap.md).

## leslie — `leslie(speed, drive, balance, doppler, mix)` · `instrument_leslie(slot, …)`

THE rotary-speaker cabinet — the organ's voice. A spinning treble **HORN** (pitch wobble via a
real **Doppler** delay-line + a swirling volume) over a bass **DRUM**, split at an 800 Hz crossover.
The two rotors spin at independent speeds with **real spin-up/down inertia**, so flipping `speed`
gives the iconic **chorale↔tremolo swell** on its own — you don't ramp it, the engine does. `speed`
= `LESLIE_STOP` / `LESLIE_SLOW` (chorale ~0.7 Hz) / `LESLIE_FAST` (tremolo ~6 Hz); `drive` 0–1 (tube
preamp grit); `balance` 0=drum..1=horn; `doppler` 0–1 (horn pitch-wobble depth); `mix` 0–1 (**0 =
off**). A VERBATIM navkit `processLeslie` port (A/B 0.99999). **Pinned LAST** in the chain (the
cabinet output stage, not a reorderable pedal). **Showcase: `organ`** (the `L` footswitch).

| recipe | call | character | used by |
|---|---|---|---|
| jazz B3 swell | `instrument_leslie(I_ORG, LESLIE_SLOW, 0.0f, 0.5f, 0.7f, 1.0f)` → flip to `LESLIE_FAST` | chorale sway, then hit the switch for the spin-up to tremolo — the signature Hammond move | `organ` |
| overdriven rock organ | `instrument_leslie(I_ORG, LESLIE_FAST, 0.4f, 0.5f, 0.8f, 1.0f)` | cranked tube preamp into a fast horn — the cabinet screams | `organ` |
| horn-forward | `instrument_leslie(slot, LESLIE_FAST, 0.0f, 0.85f, 1.0f, 1.0f)` | balance toward the treble horn + deep Doppler = maximum shimmer/wobble | (balance pattern) |
| brake | `instrument_leslie(slot, LESLIE_STOP, …)` | rotors coast to a halt — the static, slightly-doubled "stopped" tone between songs | (stop pattern) |
| guitar cabinet (master) | `leslie(cab_speed, drive, balance, 0.5f, 1.0f)` | the rotary as a *guitar* cabinet — the Leslie tenant of pedalboard's CABINET slot (it's "pick your cabinet": amp **or** Leslie) | `pedalboard` (CABINET → Leslie) |

> **Leslie vs the recipe:** decision 0015 first refused this as "tremolo + `LFO_PITCH` + drive, a
> recipe" — then building it proved an LFO can't band-split at a crossover, can't run a delay-line
> Doppler, and can't drive two inertial rotors. The reversal (0015, 2026-06-12) is the model case of
> the gate working. Great on organ; also try it on electric piano, guitar, or a whole psych mix.

---

## filter — `filter(int mode, float cutoff_hz, float resonance)`

THE master resonant filter — the **DJ-filter / build-up sweep**. A state-variable filter on the whole
mix in a selectable mode; **RIDE the cutoff live** (cheap to re-call every frame, unlike the buffer
effects). A reorderable insert (`FX_FILTER`), so its chain position matters (filter→crush vs crush→filter).
mode = `FILTER_LOW`/`HIGH`/`BAND`/`NOTCH` (`FILTER_OFF` = bypass), cutoff 20..~20000 Hz, resonance 0..1.
Same SVF as wah/formant, reused as a plain swept filter (0015: filter-reuse, not a new primitive).

| recipe | call | character | used by |
|---|---|---|---|
| breakdown muffle | `filter(FILTER_LOW, cut, 0.5f)` ride cut 300→8000 | close to a thumping muffle, open for the build/drop — the four-floor breakdown | `djfilter` (the BUILD breakdown) |
| screaming build | `filter(FILTER_LOW, cut, 0.9f)` sweep cut up | high resonance = an acid scream riding up into the drop | `djfilter` (the BUILD riser) |
| thin it out | `filter(FILTER_HIGH, 800.0f, 0.3f)` | roll off the lows — a radio/telephone thinning, or a transition | `djfilter` (CW = high-pass) |
| bipolar DJ filter | one knob: center = `FILTER_OFF`, CCW → `FILTER_LOW` 18k→150 Hz, CW → `FILTER_HIGH` 20→6k (resonance rises as you close) | the mixer one-knob filter — muffle on the breakdown, thin on a transition | `djfilter` (THE knob), `groovebox` (the FILTER knob), `kaoss` (FILTER program — pad x bipolar, y = resonance) |
| the filter ride | `filter(FILTER_LOW, ride_hz(songpos), res)` per frame | a master cutoff curve AS the arrangement (the house "filter ride IS the song", for real) | — |
| filter pedal | `filter(act ? FM[mode] : FILTER_OFF, 40·450^cut, res)` — CUT exp 40..18k Hz, MOD picks LP/HP/BP/NCH | a stompbox resonant filter you set (or ride) and reorder against the other pedals | `pedalboard` (FILTER, MOD knob) |

**Showcases: `djfilter`** (the bipolar one-knob mixer filter over a self-playing house loop, with a live response curve and a breakdown→riser→DROP BUILD) and **`pedalboard`** (the FILTER pedal — LP/HP/BP/notch via its MOD knob, reorderable in the chain).

## sidechain & bus compression — `sidechain(victim_bus, key, amount, atk, rel)` · `sidechain_key(slot, key, send)` · `glue(victim_bus, amount, atk, rel)`

DYNAMICS on the SUMMED signal (a gain stage, **not** a chain insert — not in `fx_order`). `sidechain`
ducks a victim bus every time a TRIGGER fires; route the trigger (the kick) into a key with
`sidechain_key`. `glue` is the same envelope→gain with no trigger — a bus compressor reading its own
level. amount 0 = dormant (byte-identical). **One comp per bus** (sidechain OR glue on a given bus,
not both). **Showcase: `groovebox`** (PUMP + GLUE share the master comp). Effects-bus Increment D.

| recipe | call | character | used by |
|---|---|---|---|
| the house pump | `sidechain_key(SL_KICK, 0, 1.0f); sidechain(0, 0, 0.6f, 1, 140)` | the whole mix breathes against the four-floor kick — bass/pad duck and bloom across the beat | `groovebox` (PUMP) |
| gentle bus glue | `glue(0, 0.4f, 8, 150)` | the mix squashed as one lump — drum-bus/master "glue", no trigger | `groovebox` (GLUE), `combo` / `pedalboard` (the amp cabinet's SAG = power-amp compression), `wba` (band glue) |
| tight pump | `sidechain(0, 0, 0.8f, 1, 90)` | deeper duck + faster recovery — aggressive EDM pump | — |

---

## detune as an effect — `instrument_tune(slot, semitones)`

Not a pitch-shift but **unison/ensemble shimmer**: tiny fractional offsets make voices beat against
each other. (Read live by every sounding voice on the slot — fire-and-forget hits bend too.)

| recipe | call | character | used by |
|---|---|---|---|
| divide-down spread | `instrument_tune(slot, 0.05f + t*0.005f)` per voice | the string-machine cluster (0.05–0.10 across tabs) | `solina`, `air` |
| opposite-detuned pair | `instrument_tune(A, +dt); instrument_tune(B, -dt*1.4f)` | two layers pulled apart = wide unison | `braindance` |
| gong ombak | detuned gong pair (per-degree cents) | the gamelan beating shimmer | `gamelan` |

---

## combination "pedals" — recipes of existing inserts (no new effect)

The biggest beloved pedals aren't new DSP — they're **bundles of inserts we already have**, the
[0015](../decisions/0015-effects-are-recipes-not-primitives.md) "recipe" idea worn as one box. Build
these as a macro pedal / preset, not a new `FX_*` kind.

| combo | = | character | used by |
|---|---|---|---|
| **LO-FI / cassette** | `crush` + `tape` (wow/flutter/sat) + `filter` (lowpass roll-off) | the lo-fi-hip-hop / vinyl / bedroom sound: one AMT knob crunches bits + downsamples + saturates, WOW warbles the tape, TON darkens | `pedalboard` (the LO-FI macro pedal) |
| **amp / cab** | `drive` + `eq` (cab voicing) + `glue` (power-amp sag) | a guitar amp's voice — the Increment E output stage; one VOICING knob swaps the bundle (clean→chime→crunch→hi-gain→lo-fi). The 5 voicings live in `runtime/ampcab.h` (shared table) | `combo` (the playable combo amp), `pedalboard` (the pinned CABINET slot: none/amp/Leslie) |
| **Dyno Rhodes** | `chorus` + `eq` presence | the bright stereo Rhodes sheen | `epiano` (DYNO) |

> **The `pedalboard` LO-FI macro** drives three master-bus inserts (crush + tape + filter) at once.
> Since [Increment F](../design/effects-bus-architecture.md) (2026-06-14) it runs them on **instance 1**
> (`crush_inst(1,…)`/`tape_inst(1,…)`/`filter_inst(1,…)` + `FX_INST(FX_*,1)` in the chain), while the
> standalone **BITCRUSH / TAPE / FILTER** pedals stay on instance 0 — so **LO-FI and the standalones can
> now be on at the same time** (the old `pedal_locked` grey-out is gone). Two of the same effect in one
> chain, exactly what F enables.

---

## Adding to this book

Shipping or using an engine effect in a cart? Add the recipe here — name it, give the exact call +
values, one line of character, and the cart under **used by** (don't mint a duplicate of an existing
recipe; add your cart to its row instead). This is the effects analog of the rule for
[`instrument-recipes.md`](instrument-recipes.md); see [CLAUDE.md](../../CLAUDE.md) → "Shipping a sound
cart? Keep the recipe docs current."
