# 0015 — Effects are recipes, not primitives: the roster is closed
Date: 2026-06-05 · Status: accepted

## Context
Drive shipped (`instrument_drive`/`note_drive`, [audio-notes §17](../design/audio-notes.md))
and the echo + reverb buses are queued behind it. That raised the obvious worry: **does
every effect cost a new API family?** Drive cost 2 functions; echo will cost 3, reverb
2–3. Extrapolated across a DAW-style effect roster (chorus, flanger, phaser, wah,
compressor, ring mod, octaver, …) that's unbounded API growth — the thing the engine-macro
discipline ("three knobs, six functions, forever", §8.1.1) exists to prevent.

Two shapes were on the table:

1. **Named functions per primitive** (the shipped path): `instrument_drive`, `instrument_echo`, …
2. **One generic family** that never grows: `instrument_fx(slot, FX_DRIVE, x)` /
   `instrument_send(slot, BUS_ECHO, x)`.

## Decision
Keep **named functions**, and close the primitive roster at the §17 taxonomy:

| layer | primitives | API cost |
|---|---|---|
| voice insert | drive ✓ · bitcrush (maybe master-internal) | 2 + 0–2 |
| shared bus | echo · reverb — **capped at two buses by design** (one rhythm, one room) | 3 + 2–3 |
| oscillator param | detune | 2 |
| master stage | soft-clip ✓ · (wow/flutter buffer, if tape ever lands) | 0 — internal |

**~12 functions, forever, for the entire effects story.** Everything else people call an
"effect" is either a *recipe* of these primitives or a *refusal with the musical need
covered elsewhere*. A new primitive must prove it cannot be a recipe.

> **The concrete proof is the pedalboard test:** *would you want it as a pedal on the pedalboard?*
> If yes it MUST be a real bus effect — a per-voice recipe (`LFO_PAN`, a `note_*` env) categorically
> can't be an `fx_order` insert, so the pedal requirement is itself the "can't be a recipe" proof.
> Full rule + worked examples (auto-pan, Dyno, bark): [`effects-bus-architecture.md`](../design/effects-bus-architecture.md) §0.

## Why
The pedalboard audit. Running the classic effects through the four layers:

**Already covered today** — distortion/overdrive/fuzz = the drive knob's range; wah-wah
**in its simple forms** (pedal wah = `FILTER_BAND` + res + live `note_cutoff`; rhythmic
auto-wah = `LFO_CUTOFF`; the funky-clav *quack* = a fast per-note `ENV_CUTOFF` snap, ~100ms,
on a resonant filter — the `epiano.c` clav uses this) — **but the realistic "woah woah"
auto-wah is NOT covered here; it's a deferred bus effect** (a bandpass on the *summed mix*
with an exponential sweep + an envelope follower tracking the whole performance — a per-voice
filter can do neither: it can't sweep a chord coherently or pump with the groove;
[instrument-engines §8.10](../design/instrument-engines.md), confirmed 2026-06-08 by rendering
navkit). NB a per-note `ENV_CUTOFF` is **not** an envelope follower — per-note retrigger ≠
continuous amplitude-tracking. tremolo splits like wah: *simple* tremolo = `LFO_VOLUME` (per-voice),
**but the realistic AMP tremolo is a bus effect** (one LFO phase across the whole instrument — a
per-voice `LFO_VOLUME` gives each note its own phase = a shimmer, not a coherent throb; shipped
`tremolo()`/`instrument_tremolo`, see 2026-06-12 correction); vibrato = `LFO_PITCH`;
whammy = `note_pitch`/`note_glide`; amp/cab sim = drive + lowpass; limiter = the master
soft-clip. **EQ** splits in two: the *cut* half (and the whole DJ low/mid/high **kill**
set) is exactly the four filter modes — kill lows = `FILTER_HIGH`, kill highs =
`FILTER_LOW`, scoop mids = `FILTER_NOTCH` — and a band *boost* has a crude recipe in the
SVF's resonance peak (`FILTER_LOW` at 120Hz, res 8 = a low shelf-ish bump; the 303
squelch is "EQ boost as instrument").

**Unlocked by the queued §17 work** — slapback/dub/tape echo → echo bus; room/hall →
reverb bus; chorus/unison/supersaw → **detune** (two notes a few cents apart *is* chorus);
lo-fi → bitcrush; sidechain pump → cart recipe (`note_vol` ducked on the kick step);
stutter/slicer → `note_vol` chopped by the beat clock.

**Worked example: the Leslie.** Rotating speaker = tremolo (`LFO_VOLUME`) + Doppler
(`LFO_PITCH`) + brightness swirl (`LFO_CUTOFF`) + pushed-tube growl (`instrument_drive`).
A slot has exactly three LFOs — the full budget, spent on one effect. The iconic
chorale↔tremolo spin-up is a cart-side `lerp` of the shared LFO rate via `note_lfo()`,
which keeps phase and so never clicks — the lerp *is* the horn's mechanical inertia.
No `instrument_leslie`, ever. **[SUPERSEDED 2026-06-12 — see the correction below. The 3-LFO
recipe can't split the band at an 800 Hz crossover, can't do a delay-line Doppler (`LFO_PITCH`
bends pitch, it doesn't model a moving source), and can't run two rotors with independent inertia.
So the Leslie CLEARS this decision's own gate, and `leslie()`/`instrument_leslie` shipped — a
verbatim navkit port. The example stands as the *reasoning pattern*; its verdict flipped.]**

**Deliberate refusals** — flanger (the one true gap: needs a modulated short delay;
if the master wow/flutter buffer ever lands for tape, flanger falls out of it free);
ring mod (its musical job — metallic inharmonic clang — is already INSTR_FM's
off-integer detents); octaver (a pedal exists because a guitar has one string — we
have polyphony: play the octave); auto-pan/stereo width (the engine is mono — console
identity, not a missing feature); granular/shimmer/freeze (DAW-tier; a cart has
`wave_set()` and ambition); **channel-strip EQ** (boost-this-band-leave-the-rest:
2–3 extra biquads per voice + more API for a tool whose job is carving *dense* mixes —
ours are 16 simple waves, and **register discipline is the real EQ at this fidelity**:
bass plays low, lead plays high, waveform/duty/drive set the brightness. If the cart
corpus ever proves otherwise — several music carts hand-rolling double-filter tricks,
the 0014 evidence pattern in reverse — the admitted shape is one function,
`instrument_eq(slot, low, mid, high)`, and a new interrogation here).

**Why named functions despite the growth?** Because `studio.h`'s one-liner-per-function
style *is the documentation system* — autocomplete, hover and the help tab all hang off
the bare name. `instrument_drive` teaches at the moment of typing; `instrument_fx(slot,
FX_DRIVE, …)` teaches enums. A 12-function ceiling doesn't justify giving up legibility,
and the engine macros set the same precedent (named `instrument_harmonics/timbre/morph`,
not `instrument_macro(slot, which, x)`). The defense against bloat is not a calling
convention — it's **refusing to admit new primitives**.

## Consequences
- The moment anyone is tempted to add `instrument_flanger` / `instrument_chorus` /
  `instrument_wah`, the answer is this document, not four-place wiring.
- Chorus, tape, Leslie, wah, sidechain et al. belong in **docs/guides or carts as
  recipes** (a hammond cart with the Leslie ramp would exercise drive + all three LFOs).
- Echo and reverb, when they land, complete the roster; their API shapes are sketched
  in [audio-notes §17](../design/audio-notes.md) and bounded by this decision.
- Supersedes nothing; extends the §8.1.1 macro discipline from engines to effects.

## Correction history

- **2026-06-08 — wah classification (now folded into "Why" above).** The original audit filed
  wah as fully "already covered today" and called a per-note `ENV_CUTOFF` an envelope follower.
  Rendering navkit's actual wah (`tools/navkit-render.c`) split it: **simple** wah is a per-voice
  recipe, but the realistic "woah woah" **auto-wah is a deferred bus effect** (summed-mix bandpass
  + follower — [instrument-engines §8.10](../design/instrument-engines.md)), and a per-note env is
  not a follower. The "Why → Already covered today" wah clause now states this directly; the
  *decision itself* (effects are recipes, roster closed) was never affected.

- **2026-06-10 — the modulated-delay buffer landed early (via chorus), and the decision held.**
  This doc reserved a single master "wow/flutter buffer" (the master-stage row) as the home for a
  real modulated-delay chorus + flanger + tape-wow, deferred until tape, and ruled plain chorus =
  `detune`. The Juno showcase brought the *real* BBD chorus forward, so that buffer **shipped now,
  via chorus instead of tape** (`chorus(rate, depth, mix)`, a navkit BBD port). Crucially it landed
  exactly as the roster prescribes — **a MASTER-STAGE INSERT (the reserved buffer), NOT a third
  send bus** — so the "shared buses capped at two (echo, reverb)" line is untouched, and there is
  **no `instrument_chorus`** (master-wide; per-part waits for aux routing). Flanger (= same buffer +
  feedback + short delay) and tape wow/flutter (= same buffer + slow LFO) are now queued *uses of
  this one buffer*, not new primitives — the "write the mod-delay once, place it many ways"
  discipline. So the decision didn't bend: chorus is a recipe/use of a rostered primitive, the
  buffer was always sanctioned, only its *timing* moved (chorus-first, not tape-first). The
  detune-as-cheap-chorus note still stands as the zero-cost stand-in. Build: [instrument-engines
  §8.10](../design/instrument-engines.md) build-state; showcase: the `juno` cart.

- **2026-06-10 — the "one true gap" (flanger) is closed, exactly as predicted.** The Deliberate-
  refusals list called flanger "the one true gap: needs a modulated short delay; if the master
  wow/flutter buffer ever lands for tape, flanger falls out of it free." Chorus landed that
  modulated-delay technique (above), so flanger shipped as its **second use** —
  `flanger(rate, depth, feedback, mix)`, a MASTER INSERT, no new primitive. One refinement to the
  earlier wording: chorus and flanger don't share one *physical buffer* (different sizes, both can
  run at once); they share the modulated-delay **technique** — the Hermite read was generalized
  (`moddel_hermite(buf, len, pos)`) so each effect reads its own buffer through one helper ("write
  the technique once, instantiate per effect," like the SVF serving filter/formant/wah). The
  decision is *vindicated*, not bent: flanger was always a use of the rostered mod-delay primitive,
  and it cost zero new roster entries. **Tape** is the queued third use (slow wow/flutter LFO +
  saturation). Build: [instrument-engines §8.10](../design/instrument-engines.md); showcase: the
  `mistress` cart (EHX Electric Mistress).

- **2026-06-11 — the AUX-routing step landed; the send/insert split is now physical.** Per-instrument
  inserts shipped (`instrument_chorus`/`instrument_flanger(slot,…)`, an 8-bus pool). This makes the
  send-vs-insert distinction the roster always implied **concrete**: **inserts (chorus/flanger) get
  per-bus state** (each routed instrument its own comb buffer — that's what "the signal passes
  *through* it" requires), while **sends (echo/reverb) stay ONE shared tank** with per-slot send
  coefficients. Per-bus reverb/echo was explicitly **rejected**, not deferred: it would mean N
  separate rooms/echoes (musically wrong — one room is the point — and the echo line ×8 ≈ 2.7 MB) and
  would break the per-slot `instrument_echo`/`instrument_reverb` a dozen carts use. So the two-bus
  *send* cap still holds (echo + reverb, one tank each); the bus *pool* is an insert-routing
  mechanism, a different axis. No new primitives — routing, not roster. Build:
  [instrument-engines §8.10](../design/instrument-engines.md).

- **2026-06-11 — the reserved "wow/flutter buffer" landed, as tape.** The master-stage roster row
  listed "soft-clip ✓ · (wow/flutter buffer, if tape ever lands)" — and tape ever landed.
  `tape(wow, flutter, saturation)` is the **third use of the modulated-delay technique** (after
  chorus + flanger), so the buffer the chorus/flanger story kept invoking now has its namesake
  customer. Zero new roster entries: tape = the rostered mod-delay primitive (wow/flutter) +
  saturation (a soft-clip nonlinearity, already rostered as the master-stage entry) + a baked HF
  rolloff (a filter, rostered). The effects roster is now essentially built out — echo, reverb,
  chorus, flanger, tape — and the remaining items (auto-wah bus, formant/vocoder, ring-mod,
  bitcrush, leslie recipe, compression) are all either recipes, deferrals, or unrostered candidates
  exactly as this decision laid out. Build: [instrument-engines §8.10](../design/instrument-engines.md);
  showcase: the `tapeloop` cart (Frippertronics).

- **2026-06-11 — the wah detour is fully closed: the corrected bus auto-wah is BUILT.** The
  2026-06-08 correction (above) re-filed the realistic "woah-woah" auto-wah as a bus effect (a
  resonant bandpass on the *summed* signal swept by an envelope follower). It is now shipped:
  `wah(sensitivity, resonance, mix)` + `instrument_wah(slot,…)`, a per-bus insert — an envelope
  follower on the bus signal opening a TPT state-variable bandpass (the SVF, reused at bus level; the
  "4th use of the one filter" the roster table predicted). No new primitive — a filter + a follower,
  both rostered; the simple per-voice wah and the per-note clav ENV-quack remain valid recipes. So the
  scar that *named* this whole effects-discipline (the per-voice-vs-bus mistake) is now not just
  understood but resolved in code, with `clavinet` as the funk-clav proof. The §8.10 effects roster
  is essentially complete. Build: [instrument-engines §8.10](../design/instrument-engines.md).

- **2026-06-12 — four more effects shipped (bitcrush, EQ, tremolo, phaser); the GATE held, the
  "essentially complete" framing did not — and this log fell behind.** Catching up honestly:
  - **bitcrush** + **EQ** were never violations — bitcrush is in the §17 taxonomy this decision
    closes *around* (line 23/54), and EQ shipped through this doc's *own named escape hatch*
    (`instrument_eq(slot, low, mid, high)`, the "channel-strip EQ" interrogation above). Both just
    landed without a log entry. (EQ: `eq()`/`instrument_eq` + `crush()`/`instrument_crush`, per-bus
    inserts; showcases `eq`, `bitcrush`.)
  - **tremolo** splits *exactly* like the 2026-06-08 wah correction: simple tremolo = `LFO_VOLUME`
    (still true, still a recipe), **but** the realistic **amp** tremolo is a bus effect — one LFO
    phase shared across the whole instrument, which a per-voice `LFO_VOLUME` (each note its own phase
    = a shimmer, not a coherent throb) cannot do. So `tremolo()`/`instrument_tremolo` is the same
    "per-voice recipe vs summed-bus effect" shape as wah, not a new axis. Line 44's bare
    "tremolo = `LFO_VOLUME`" is now refined in place to say so. Verbatim navkit `processTremolo`;
    showcase `epiano` (the Wurli throb).
  - **phaser** is the genuine new entry — and it's an *admitted exception under this decision's own
    rule*, not a breach. It is **not** a recipe (no allpass primitive exists; the flanger note's
    "falls out of the mod-delay buffer free" does NOT apply — a phaser is a cascaded allpass chain,
    a different structure than the chorus/flanger/tape delay buffer). It **passes the gate** ("a new
    primitive must prove it cannot be a recipe"): it can't, so it's in. `phaser(rate, depth,
    feedback, mix, stages)` + `instrument_phaser`, a per-bus insert; verbatim navkit `processPhaser`;
    showcase `epiano` (the 70s phased-Rhodes swirl — the one EP effect the library lacked).
  - **The honest meta-point.** What actually held across all of this is the **named-function style**
    and the **"prove it can't be a recipe" gate** — both intact, both still doing their job (the gate
    correctly admitted phaser and still refuses octaver/ring-mod/granular). What did **not** hold is
    the literal *"the roster is frozen / essentially complete"* rhetoric: porting from navkit has
    become a steady, legitimate source of new bus inserts, each cheap (a per-bus `*_used[]` insert,
    byte-identical when off) and each clearing the gate. So this decision's enduring value is the
    **gate, not a fixed count** — read "closed roster" as "closed to *recipes-masquerading-as-
    primitives*," not "no function will ever be added." The routing-layer companion
    [`effects-bus-architecture.md`](../design/effects-bus-architecture.md) (2026-06-12) still cites
    this decision correctly — its reorder/multi-reverb work is routing, not new effects.

- **2026-06-12 — the Leslie refusal is reversed (the gate flipped its own verdict).** The "Worked
  example: the Leslie → No `instrument_leslie`, ever" above was this decision's flagship refusal: a
  rotary speaker = tremolo + `LFO_PITCH` Doppler + `LFO_CUTOFF` swirl + drive, spent across a slot's
  three LFOs. Building the **real** thing (navkit's `processLeslie`, the Leslie 122 model) proved the
  recipe can't reach it on three counts, each a thing an LFO simply can't do: (1) an **800 Hz
  crossover** splitting the signal into a bass DRUM and treble HORN that rotate at *different* speeds —
  one LFO can't band-split; (2) a **physical Doppler** via a modulated **delay line** (Hermite-tapped)
  — `LFO_PITCH` bends an oscillator's pitch, it doesn't model a moving sound source, and a mix has no
  single pitch to bend anyway; (3) **two rotors with independent inertia** (horn light/fast, drum
  heavy/slow — different accel *and* decel time constants), where the recipe had one shared lerp. So
  the Leslie **fails the recipe test and clears the gate** — exactly like the realistic auto-wah and
  the bus tremolo before it. Shipped: `leslie(speed, drive, balance, doppler, mix)` +
  `instrument_leslie(slot, …)`, a verbatim navkit port, **pinned LAST** in the insert chain (the
  speaker/cabinet output stage — like the soft-clip, not a reorderable `FX_*` pedal, so it never
  touches the `fx_order` packing). Per-bus + `leslie_used[]`-gated → dormant carts byte-identical
  (soundcheck silent). A/B vs navkit's real `processLeslie`: **0.99999 sample-level correlation**
  (FAST & SLOW); a driven setting reads 0.992 only because *our* master soft-clip catches the hotter
  signal that navkit's bare harness doesn't have — not a Leslie discrepancy. This is the cleanest case
  yet OF the gate working: the refusal wasn't wrong *as reasoning* (a Leslie really is "modulation +
  Doppler + drive"), it was wrong that those could be *assembled from the rostered primitives* — and
  the gate is precisely the test that catches that. The pattern is now explicit across wah/tremolo/
  Leslie: **"X = a + b + c" does not imply "X is a recipe" unless a, b, c are each reachable today.**

- **2026-06-12 — formant (vowel) filter shipped as a bus insert; the "SVF reused N times" pattern holds.**
  `formant(vowel, q, mix)` + `instrument_formant(slot, …)` is the talkbox-family vowel filter — four
  bandpasses at the human formant frequencies, on a per-bus insert (`FX_FORMANT`, gated → byte-identical
  when off). It's **not a new primitive shape**: it's the state-variable filter reused four times in
  parallel (wah was the SVF's 4th use, the realistic auto-wah; this is the same SVF, four copies, parked
  at vowel frequencies instead of one envelope-swept peak). So it clears the gate the way EQ/wah did — a
  *filter*, already rostered, placed a new way — not a phaser-style admitted exception. Two clarifications
  it nails down: (1) **per-voice vs bus** — navkit runs its formant *effect* per-voice for one reason,
  per-note vowel articulation (each note says its own phoneme), which is a *singing-synth* feature owned by
  `INSTR_VOICE`; as an **effect** ("push any sound through a vowel") it's inherently a bus/insert (a plugin
  only ever sees the summed track), and the bus form covers everything except polyphonic per-note vowels,
  which aren't an effect's job. (2) **formant filter ≠ vocoder** — the vocoder is carrier×modulator (two
  inputs) and still waits on the sidechain path; the formant filter is single-input and complete now. It
  even reused navkit's measured vowel table verbatim (already in `sound.h` for `INSTR_VOICE`), so the port
  was routing, not DSP. Showcase: the `vowel` cart. §17 ledger item 13.
