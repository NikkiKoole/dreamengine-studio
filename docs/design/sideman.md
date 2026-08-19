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
asymmetric **IRON** stage, plus a modest **PLATE** send with the bass drum sending nothing. One key
switches the whole cabinet out, and because every outboard stage bypasses to a byte-identical null
([analog-outboard-chain.md](analog-outboard-chain.md) §4), that switch is a **true A/B** rather than
an approximate one.

**Measured here, and it is not instantaneous.** Method: two renders of this cart with the plate
parked out (the plate's tail carries a difference for over a second on its own, §4's corollary), one
holding the cabinet in throughout, one switching it out at 4.000 s and back in at 5.000 s. They are
**bit-identical up to sample 4.0000 s**, differ across the gap, and **reconverge 0.304 s after the
switch back**. So the stages themselves null exactly, but a *stage switch* reconverges only once the
chain's own memory has decayed. The likeliest cause is the console EQ's low band: its corner is at
80 Hz, one period of which is 12.5 ms, so a settling time in the hundreds of ms is what that filter
should have. Worth flagging because the ledger's §4 table records EQ as bit-exact *at* the switch,
which is the OUT direction measured against a never-on run; re-engaging a stage is a different
question and this is the number for it. A bypass oracle needs a per-stage tolerance in TIME, not a
boolean.

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

## 7. Gates

| after touching | run |
|---|---|
| the cart's logic | `node tools/spec.js sideman` |
| the cart's layout | `node tools/ui-audit.js sideman`, `node tools/mobile-lint.js sideman` |
| the voicing (`sideman.h`) | `harmonic-spec` (the harmonic ladder, and mind that it starts its window 35% into the file, so on a 45 ms voice in a 200 ms cut it measures the tail's quantisation noise), `inharm-spec --decay` (per-partial decay: does the fundamental or the top die first), `wav-envelope`, `click-check --quiet`, `level-check`, `dc-check`, and `ab-render` to A/B the tube amount (it exits 2 if the value never reached the DSP) |
| the cabinet | `fx-check`, `level-check`, and the byte-identical-bypass method in [analog-outboard-chain.md](analog-outboard-chain.md) §4 (the LAST DIFFERING SAMPLE, not a sha) |
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
