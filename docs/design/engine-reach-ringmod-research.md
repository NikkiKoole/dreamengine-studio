# Research round: note-tracking ring modulation

> **STATUS: RESEARCH COMPLETE (2026-09-14)**, **DECISION A — not an engine.** The §5
> round owed by [`engine-reach.md`](engine-reach.md) §7.3. The cheap path works: a
> ratio parameter on the shipped `FX_RINGMOD` bus insert. The clang that stays
> harmonic across the keyboard is last-note tracking of a carrier, not a new
> oscillator. Tier 3 stays **two engines**.

## 1. What industry code does

Ring / AM is two products that share a multiply:

| Shape | What it is | Carrier | Where it lives |
|---|---|---|---|
| **Effect** | one sine times the mix | absolute Hz (or MIDI-follow of *one* pitch) | after the voice, on a bus |
| **Voice architecture** | oscillator × oscillator | a ratio, because both oscs track the keyboard | inside the voice, before the filter |

The effect is the Bode box and every DAW insert after it. **Bode, H. (1961),
"A new tool for the exploration of unknown electronic music instrument
performances"** and the later Bode Frequency Shifter / Ring Modulator: a
standalone carrier oscillator in Hertz. Ableton Frequency Shifter, Eventide,
Soundtoys, our own `ringmod(freq_hz, mix)` — same object. MIDI-follow on those
boxes is last-note CV into that one oscillator, not a per-voice multiply.

The voice architecture is two tracking oscillators into a multiplier. **ARP
Odyssey (1972)**: VCO1 × VCO2, both 1 V/oct. **EMS VCS3 (1969)**: ring mod as a
matrix node; the oscillators can track the keyboard. Digital synths that expose
"oscillator RM" (Serum, Vital, Phase Plant, SuperCollider's audio-rate
`*` of two Oscs) are this second shape. The ratio is not a special ring-mod
parameter. It is what you get when both inputs already track.

Csound `ringmod`, Roads' *Computer Music Tutorial* (the AM/RM chapter), and
Chowning 1973 (FM, not AM, but the same carrier/modulator vocabulary) all treat
the *algorithm* as a multiply. None of them require a new engine to change the
carrier from Hz to a ratio of the played pitch. The algorithm does not move.
Only the frequency input does.

## 2. Hz vs ratio vs voice-local

**Fixed Hz** (shipped). One carrier for the bus. A C and an E through 440 Hz
both pick up the same sum/difference tones. That is the Dalek, the robot, the
atonal clang. It is also why a scale through `instrument_ringmod(slot, 440, 0.8)`
stops being harmonic the moment you leave A4.

**Ratio on the bus** (this round). `f_carrier = ratio × f0`, where f0 is the
last-started voice on that slot (or, on the master, the last-started voice
anywhere). A C and then an E each get a carrier at the same musical interval.
Sidebands stay in the harmonic series of the *played* note: ratio 1.5 on C4
(261.6 Hz) puts energy at 0.5× and 2.5×, and the same multipliers on E4. That
is the catalog's "clang stays harmonic per note."

**Voice-local** (the engine that was on the table). Each sounding voice
multiplies by its own `sin(2π · ratio · f0 · t)` before the mix. A *chord*
of C+E+G would then keep a harmonic clang on every note at once. A bus cannot
do that: one carrier, one pitch.

The stated job is **a scale / a keyboard**, not a chord of independent clangs.
Analog practice agrees. The Odyssey is duo/mono. The Bode box is one oscillator.
Polyphonic analog rarely put a ring mod per voice (the CS-80 did not). A chord
of independently-ringed notes is a different product — AM as a *voice*, which
`INSTR_FM` already reaches for inharmonic spectra, and which §7.1's sustaining
exciter would also reach. It is not what §7.3 asked.

Last-note on a bus is also the honest analog of "two VCOs into a ring, last-note
CV." Glide rides `v->freq`, so the carrier follows a slide. Vibrato on the
carrier would need the per-sample `pitch_mul` inside the voice loop; that is
the one refinement that is voice-local, and it is not the job.

## 3. Per-voice cost, if it had to sit inside the voice

One extra phase accumulator and one `de_sin_turns` per sample per voice. Buffer-free.
The catalog's "≈10 lines, free" was right. The cost is not why this is not an
engine. The cost is API: a third engine for a multiply we already run, to buy
polyphonic independent clang that the ticket did not ask for, against
[ADR-0015](../decisions/0015-effects-are-recipes-not-primitives.md) (a new
primitive must prove it cannot be a recipe) and the tiny-API bias.

Scanning 32 voices for last-started, once per sample on a bus that has ratio
mode on, is cheaper than a sine.

## 4. Gotchas (the §5 list)

- **Aliasing.** Same as the shipped insert: a naive sine carrier, no new
  bandlimit. Not introduced by tracking.
- **Level.** `|carrier| ≤ 1` and the dry/wet blend still keep `|out| ≤ |in|`.
  Ratio 1.0 on a sine produces DC (sin·sin has a constant term); that is AM at
  1:1, inherent, already true of `ringmod(f0, 1)` in Hz mode. Mix < 1 keeps
  the dry fundamental.
- **DC / clicks / tuning.** Carrier phase is continuous across note changes
  (same as a Hz sweep today). `click-check` cares about splices, not a new
  frequency. The carrier is post-mix; `tune-check` on an un-modulated voice
  is untouched. Deterministic: `de_sin_turns`, no libm.
- **Denormals / feedback.** None. It is a multiply, not a loop.
- **Polyphony.** Last-started voice wins. A held chord shares one carrier.
  Written down so it is not rediscovered as a bug.

## 5. Second implementation / licence

No borrow. The multiply is the one we shipped on 2026-06-14. There is nothing
to port from STK or navkit and nothing to attribute. A ref-render of someone
else's ring mod would compare a *carrier frequency*, not a mechanism, and
would not change the decision.

## 6. Measured (C-major scale, INSTR_SAW, mix 0.8)

Headless `DE_NO_RAYLIB` renders of `ringtrack` (`-DRINGTRACK_HZ` vs default ratio 1.5).
Peaks via a 1 Hz Goertzel scan on the C4 and C5 windows:

| mode | note | strongest peaks | as × f0 |
|---|---|---|---|
| Hz 440 | C4 261.6 | 178, 702 Hz | **0.68, 2.68** |
| Hz 440 | C5 523.3 | 83, 963 Hz | **0.16, 1.84** |
| ratio 1.5 | C4 261.6 | 131, 654 Hz | **0.50, 2.50** |
| ratio 1.5 | C5 523.3 | 262, 1308 Hz | **0.50, 2.50** |

Fixed Hz: the interval between the note and the clang **changes** with the key
(178 = \|261.6−440\|; 83 = \|523.3−440\|). Ratio: the multipliers **hold**.
That is the catalog sentence, as a table. WAVs: regenerate with
`tools/clips/ringtrack/01-hz.script` / `02-ratio.script` (or `-DRINGTRACK_HZ`
into `tools/headless-nr.c`).

## 7. The decision

**A: parameter-only.** `ringmod_ratio(ratio, mix)` and
`instrument_ringmod_ratio(slot, ratio, mix)` on the shipped `FX_RINGMOD`
insert. Hz API unchanged (Dalek / robot / atonal clang stay). Ratio 0.25..16,
same neighbourhood as `instrument_sync`. Showcase / proof: the `ringtrack`
cart. [`engine-reach.md`](engine-reach.md) §7.3 / §7.5: count stays 2.

Escalate to a voice-local engine only if a later product needs a *chord* of
independent harmonic clangs. That would still not be a new mechanism — it
would be this multiply moved before the mix — and it would owe its own
ticket. Do not reopen §7.3 from vibes.
