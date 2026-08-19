# The Wurlitzer Side Man: a drum machine with no memory and no speaker

> **STATUS: SHIPPED (2026-08-19)** ‑ the `sideman` cart + the shared voice bank
> [`runtime/sideman.h`](../../runtime/sideman.h). Voicing measured and tuned separately, see §7.

The Rudolph Wurlitzer Company, 1959. The first drum machine ever sold, twenty years before
anything else in this studio, and the reason a certain sound gets described as a *plock* rather
than as a beat.

---

## 1. What the machine is

Electro-mechanical, not electronic. A motor spins a disc with metal contacts across its face,
wipers read them, and each closure fires one of **ten vacuum-tube circuits**. A rotary knob on the
lid selects which of **twelve rhythms** the wipers read; a slider sets the motor speed, which is the
tempo. Ten front-panel buttons play the voices by hand, and a remote unit let an organist trigger it
from the keyboard.

There is no pattern memory, no accent, no per-voice knob and no programming. **The disc IS the
pattern and the tube IS the sound.**

The ten voices, as named on the panel:

| | |
|---|---|
| membranes (2) | BASS DRUM, TOM TOM I, TOM TOM II |
| **struck wood (4)** | **WOOD BLOCK, TEMPLE BLOCK I, TEMPLE BLOCK II, CLAVES** |
| noise (3) | BRUSH, MARACAS, CYMBAL |

Read that list again, because it is the whole reason this cart exists. Four of the ten voices are
struck wood and only three are membranes. This box is mostly a **wooden percussion section**, which
is why the era's rhythm sound is remembered the way it is. A wood block circuit is also the simplest
honest thing a 1959 tube stage can do: kick a resonant network with a pulse and let it ring down.

The twelve rhythms, in dial order: Beguine, Bolero, Cha Cha, Foxtrot 2 Beat, Foxtrot 4 Beat, March,
Rhumba, Samba, Shuffle, Tango, Waltz, Western.

## 2. Why the cart is a disc and not a grid

Every other drum machine cart here (`cr78`, `tr808`, `tr909`, `drummachine`, `groovebox`) is a
left-to-right step grid with a playhead sweeping across it. That is the right shape for those
machines, because that is what they are.

It is the wrong shape for this one. On a Side Man the playhead **cannot move**: the wiper is bolted
at one place and the pattern turns underneath it. So the cart is the mechanism:

- one **disc revolution is one bar**, and the disc's angle is taken straight off the synth's own
  beat counter, so the picture and the sound cannot drift (asserted, see §5)
- the ten tracks are **concentric rings**, innermost = BASS DRUM, outermost = CYMBAL
- contacts turn **clockwise into a fixed wiper arm at twelve o'clock**
- a stamped contact is drawn as a brass strip lying along its track (`rectfill_rot`, which is GPU
  geometry: a `ring()` sector fill would rescan the whole disc's bounding box per contact, 160 times
  a frame)
- an empty slot is still drawn, as a hairline, because otherwise nobody discovers the disc is
  stampable. Click a slot to stamp or lift a contact; drag to paint.
- the tempo slider is the **motor speed**, and the panel reads out both BPM and disc RPM

A pleasant consequence of a stamped disc: **the track layout can differ per rhythm, for free.**
WALTZ is three beats, so its disc carries 12 slots. SHUFFLE is four beats of **triplets**, so its
disc also carries 12, and its shuffle is therefore a *real* triplet rather than a swing knob's
approximation of one. Everything else carries 16. `cr78` needed a swing knob (which the CR-78 never
had) to get at the same feel; here the geometry gives it.

## 3. The split: the header owns the sound, the cart owns the machine

Same shape as [`acid303.h`](../../runtime/acid303.h) / `tr808.h` / `tr909.h`. The voice bank is
[`runtime/sideman.h`](../../runtime/sideman.h) and its header comment is the source of truth for
the voicing. The two claims it rests on:

- **the plock** is one damped ring through a resonant band: no noise, no layers, a hard front from
  the contact pulse leaking through before the network settles, and a fast clean decay (roughly
  25 ms for claves up to 90 ms for the hollowest temple block). The four wooden voices are tuned as
  a **set** so they read as one section.
- **the fullness is not reverb.** It is (1) every voice through `DRIVE_ASYM` at amount 0.45, which
  turns a damped sine into something with a body, and (2) the machine's own band limit.

  **The first half of that was originally written wrong, and measuring it is what corrected it.**
  The claim was that the fullness is the EVEN harmonics, on the reasoning that a single-ended tube
  stage saturates asymmetrically. Measured across five drive amounts and every tonal voice, the
  **odd partials lead the evens by 25 to 30 dB without exception**, because `DRIVE_ASYM` is a tanh
  (an odd function) with an asymmetric pre-gain. What survives is the weaker and more useful
  statement: the evens *exist* where they otherwise would not (h2 goes from -94 dB bypassed to
  -39 dB driven, a 55 dB rise), and that is what separates `DRIVE_ASYM` from `DRIVE_SOFT`. **The
  fullness is the whole harmonic ladder, not its even half.** 0.45 is where the ladder arrives:
  0.30 is 20 dB thinner at h5, and past 0.45 the ladder gains 2 dB while the claves' energy above
  6 kHz doubles (a 2.2 kHz voice's third harmonic is already at 6.7 kHz).

## 4. The cabinet, and why it is not baked in

**The Side Man had no speaker.** It fed the organ's amplifier and came out of a wooden cabinet, and
that stage is a real part of the remembered sound: mid-forward, top rolled off, gently saturated.

So it is not baked into the voices. The cart pins it as a rack from
[`runtime/outboard.h`](../../runtime/outboard.h): the console EQ on its **WARM** curve into the
asymmetric **IRON** stage, plus a modest reverb send with the bass drum sending nothing. One key
switches the whole cabinet out, and because every outboard stage bypasses to a byte-identical null
([analog-outboard-chain.md](analog-outboard-chain.md) §4), that switch is a **true A/B** rather than
an approximate one.

**Measured here, and it is not instantaneous.** Method: two renders of this cart with the plate
parked out (the plate's tail carries a difference for over a second on its own, §4's corollary), one
holding the cabinet in throughout, one switching it out at 4.000 s and back in at 5.000 s. They are
**bit-identical up to sample 4.0000 s**, differ across the gap, and **reconverge 0.304 s after the
switch back**. So the stages themselves null exactly, but a *stage switch* reconverges only once the
chain's own memory has decayed. Switching a stage **out** and switching it back **in** are therefore
two different questions, and the ledger's §4 table had only ever asked the first.

**And the cause is not what this doc first guessed.** The obvious hypothesis was the console EQ's low
band: its corner is 80 Hz, one period is 12.5 ms, so hundreds of ms of settling is what that filter
should have. [`bypass-check.js`](../../tools/bypass-check.js), built to answer exactly this, measured
the EQ at **0.0 ms in both directions** and refuted it. `eq_process`'s state is driven by its
*input*, and its gains only scale bands it has already split; `eq_inst(0)` is first in the chain, so
its input never differed between the two runs. **A filter's settling time only appears when
something upstream of it changed.**

The 0.304 s is **IRON**, the stage everything (this doc included) had called a memoryless
waveshaper. `drive_process` runs a DC blocker on the wet path, because asymmetric clipping is
one-sided and has to, and its `amount <= 0.001` early-out returns *before* that filter. So the
blocker's state **freezes** while the stage is out and discharges when it comes back. That is
identified rather than inferred: the residual decays 0.6433 per 10 ms, and the blocker's
`R = 0.999` gives `0.999^441 = 0.6433` to four figures. Arguably correct behaviour, since a real
pedal's coupling cap holds charge too. The only thing that was wrong was calling the stage
memoryless.

**A spring, not a plate.** The outboard send stage is voiced as a studio plate, and since 2026-08-19
a real one: `reverb_plate()` gives it two pickups, so the tail comes back wide (correlation 0.71 on
the send, against 1.0000 for a mono tank). That is the right sound for a mix bus and the wrong sound
for this box. A plate in 1959 was an EMT the size of a wardrobe, in a broadcast studio; an organ
console's reverb was a **spring tank**. So the cart sets `tank_plain` and calls `reverb_spring()`
itself, keeping the rack's send structure (per-slot sends, the bit-exact switch) while owning the
tank's character. Measured: this cart's render is **byte-identical L and R**, correlation 1.0000,
which is exactly right for one speaker in a wooden cabinet, and it is also the proof that the studio
plate does not leak in.

That field's polarity is a scar worth reading before adding another one. It first went in as
`plate_voice`, where 0 meant "no plate" — and because carts build preset tables with **positional
initializers**, appending it left all five of the bench cart's presets silently without the plate,
with no compiler warning. It was caught only because the reconvergence oracle reported the plate's
timing **unchanged to 0.1 ms** after a change that should have moved it (it moved 490 ms once the
wiring actually landed). Inverted, so zero means the stage does what its name says.

The rack's **COMP stage is deliberately left out**: a 1959 organ amplifier had no bus compressor.
Using three quarters of a shared table honestly beats using all of it dishonestly, and this is the
first cart to exercise `outboard.h` as a *subset*, which is the test of whether that table is a real
shared voicing or just one cart's settings in a header.

## 5. The one invariant worth asserting

The cart's whole correctness claim is that **the slot that fires is the slot standing under the
arm**. Picture and sound agree, or the machine is a lie.

`spec()` asserts exactly that, across every rhythm and every slot, on two pure functions
(`rot_at`, `slot_at`) that the sequencer and the renderer both call. It has to be done on pure
functions rather than on the live clock, and the reason is worth writing down because it is a trap
for any beat-driven cart's spec: **`beat()` is advanced by the audio device, which the spec runner
never starts.** So under `-DDE_SPEC` the beat clock is frozen at 0, and a "the disc turned" assertion
passes vacuously (this one failed honestly first, which is how the pure split got made). The same
block also checks the slot-to-angle mapping round-trips at arbitrary rotations, which is what the
click-to-stamp hit test depends on.

1647 assertions, `node tools/spec.js sideman`.

## 6. The two knobs the original never had, declared as ours

The house rule from `cr78`'s swing knob: an addition is fine, saying it is period-correct is not.

- **CONTACT WEAR.** A stamped disc's contacts do not sit on a perfect grid, so each one fires a
  hair early or late. The part that matters musically is that it is the **same hair every
  revolution**: it is a static property of the disc, not random jitter, so the groove gets a
  consistent lilt instead of a wobble. Hashed from (rhythm, voice, slot), bounded to ±4 ms, so a
  replay is identical and `spec()` can assert it. Switchable, which the real disc's tolerance
  obviously was not.
- **the cabinet A/B** (§4). The real machine could not be unplugged from the organ.

Everything else is faithful, including the omissions: no accent, no per-voice knob, no pattern
memory beyond the disc in front of you.

## 6b. What the ear said, twice, against the numbers

The brush is the only voice that failed the owner's listen, and getting it right took three probe
rounds. Both times the measurements and the ear disagreed, and both times the ear was right in the
same direction, which makes it a usable prior rather than an anecdote.

**Round one: a perfectly measured arc, in the wrong place.** The voice was rebuilt so its band
travelled 1700 cents in a time-ordered arc, up from 900 cents of unordered jitter. Verdict: "sounds
like a woosh". The arc was real and it was travelling entirely inside the nasal mid, where the
shipped voice's mid band sat 11.8 dB above the quieter of its two ends. A brush is spectrally
extreme, a low body and a high fizz with the middle scooped, and no amount of movement inside the
hump fixes the hump.

**Round two: a clean 4x measured separation that was perceptually null.** Three candidates swept
grain (envelope texture 2.06x / 3.71x / 3.98x against 1.00x for the control, with a hard threshold
between 90 and 130 Hz). Verdict: "the other 3 sound very similar". The reason was in a column nobody
was reading: all three shared a **375 ms stroke with a 63 to 89 ms attack**, so they were one
gesture at three grain settings, and the gesture dominated everything the modulation did. The
transferable form: **when a listener says two things sound the same, the differing parameter is not
the problem, the shared structure is.**

**Round three: the ear picked the candidate the numbers bet against.** Given a body-to-tail axis, the
agent's bet was the loud 455 ms sizzle; the owner picked the **285 ms tight tail**, which was in the
set only to bound the axis. So "fizzles out a bit more" meant *dies away sooner*, not *sizzles on
longer*. Twice now the ear has preferred the shorter, simpler gesture over the one with the better
numbers.

**Round four was offered and declined, which is also an answer.** The one lever never swept was how
present the tonal SHELL is, the difference between "noise with a tail" and "a drum being brushed": a
sweep from 41% to 91% of the voice being drum pitch rather than noise, with groove loudness held
inside 0.04 dB so level could not do the persuading. The owner kept the shipped voice. **So the brush
is DONE and the shell axis is settled at its shipped value**, and the variants stay in `smprobe`
behind keys 2 to 4 only as the record of what was asked and answered. Do not reopen it as if it were
untried.

Two engine facts fell out of shipping it, and the first one is not specific to this cart:

- **`instrument()` does not clear what else is attached to a slot.** It replaces the wave and the
  ADSR; `instrument_env` / `_lfo` / `_filter` / `_drive` / `_level` / `_pan`, the sends and the duty
  all survive. Redefining a slot for a different voice silently inherits the old one's modulation,
  which left two stale `ENV_CUTOFF` sweeps on the promoted brush. Now documented on the function
  itself (`node tools/api.js instrument`).
- **Byte-identity is unachievable downstream of a note-count change.** `noise_state` lives per
  voice-pool slot and is never reset at note-on, so a noise voice's waveform depends on which slot it
  lands on and how many samples that slot has already produced. The promoted brush takes 3 slots and
  4 notes where the old one took 2 and 2, so every noise note after it is a different realisation of
  the same process (decays within 3 ms, band peak within 6%, centroid within 1.5%). The seven tonal
  voices *are* byte-identical, and with the note count held equal the whole render is bit-exact, so
  the check still works: it just has to hold notes constant to mean anything.

## 6c. Accepted limits

Written down as ACCEPTED rather than left on an open list, because the owner has listened to all ten
voices and passed nine of them, and the tenth was fixed by ear (§6b). A measured imperfection that
survives a listen is a limit, not a defect:

- **The maracas leak 22% and the cymbal 33% of their energy above the machine's 6 kHz ceiling.** Both
  are `FILTER_BAND`, which is 2-pole, and the only lever that would tighten them turns noise into a
  whistle (at resonance 15 the maracas measures 72% of its energy inside ±150 cents, which is a pitch,
  not a shaker). A steeper per-slot filter or a cascaded second band would fix it properly. Note this
  is already the SECOND pass on these two: as first written they were highpassed and sat at 96% and
  97% above 6 kHz, peaking at 19 and 21 kHz, which was a real defect and is gone.
- **There is no upward headroom left in the bank.** It sits at the top of the per-slot gain range
  (vol 7 x level 1.0 x trim 1.0) because nothing upstream of the tube shaper can add level: sweeping
  the wooden family's band resonance 9 to 13 moved the output 0.4 dB, since the shaper normalises
  full-scale to full-scale. `SIDEMAN_TRIM` is therefore a **down-only** lever, and a cart stacking more
  than four voices on one step should pull it down (ten fired together measure -0.6 dBFS).
- **The tube is velocity-invariant.** Drive runs pre-VCA, so the harmonic ladder is identical at every
  velocity and `boost` is a down-only trim. For THIS machine that is faithful: a contact disc closes
  the same way every time and the Side Man has no accent anywhere. It is recorded as the first thing
  to fix for a keybed cart borrowing the bank, and deliberately not fixed here.

## 7. Gates

| after touching | run |
|---|---|
| the cart's logic | `node tools/spec.js sideman` |
| the cart's layout | `node tools/ui-audit.js sideman`, `node tools/mobile-lint.js sideman` |
| the voicing (`sideman.h`) | `harmonic-spec` (the harmonic ladder, and mind that it starts its window 35% into the file, so on a 45 ms voice in a 200 ms cut it measures the tail's quantisation noise), `inharm-spec --decay` (per-partial decay: does the fundamental or the top die first), `wav-envelope`, `click-check --quiet`, `level-check`, `dc-check`, and `ab-render` to A/B the tube amount (it exits 2 if the value never reached the DSP) |
| the cabinet | `node tools/bypass-check.js --rack sideman` (the committed oracle: both directions, per-stage tolerance in TIME plus a residual ceiling, and it refuses to pass vacuously on a byte-identical pair). Plus `fx-check`, `level-check` |
| anything | `node tools/lint-carts.js`, `node tools/lint-fx-frame.js --strict` |

## 8. Sources

The mechanism, the voice roster and the rhythm list are from
[120years.net](https://120years.net/the-side-manwurlitzerusa1959/) and the machine's own
documentation as summarised there and by [Sound Bridge](https://www.soundbridge.io/early-drum-machines).
Neither source carries a schematic, so the *circuits* here are reasoned from what a 1959 tube stage
can do and from how the surviving sound behaves, not copied from a service manual. That distinction
matters: `tr808.h` is built from reverse-engineered circuit values and this is not.

## See also

- [cart-library-direction.md](cart-library-direction.md) ‑ the pre-Roland wing this ticks off, and the machines still on the list (EKO ComputeRhythm, Maestro Rhythm King, Raymond Scott's Circle Machine)
- [analog-outboard-chain.md](analog-outboard-chain.md) ‑ the shared output rack the cabinet is pinned from
- [../guides/instrument-carts.md](../guides/instrument-carts.md) ‑ the chassis index for sound carts
- [../guides/cart-authoring.md](../guides/cart-authoring.md) ‑ the cart-land header table
