# The sympathetic resonator: a pedal that costs no voices

> **STATUS: BUILDING (2026-11-09)** ‑ shipped: `resonator()` / `resonator_tune()` /
> `instrument_resonator()` in `runtime/sound.h` (state in `SympRes`, `runtime/sound_ctx.h`) and the
> `openstrings` cart (a dry short-note piano arpeggio, the strings retuned per chord, R = A/B).
> **First listen (2026-11-09): the maker prefers the SAMPLED Steinway + strings by far** over our
> synthesized INSTR_PIANO + strings. So openstrings' P key plays 13 Steinway notes (bitKlavier Grand,
> CC BY 4.0) from [`assets/samples/steinway/`](../../assets/samples/steinway/README.md), and
> `SOUND_SAMPLE_SLOTS` went 8 → 16 to hold them. Open: (1) a SHIPPABLE form — today the cart reads
> the WAVs at runtime, which works in the editor and play.js but not in web/app builds; bake them in,
> shrunk the Flyfish way (per-register length 3.1 s → 0.7 s, 22 kHz, maybe ADPCM: ~3.5 MB → ~0.25-1 MB);
> (2) a real multisample instrument (one slot, zones by key) instead of one instrument slot per
> zone; (3) velocity layers (one layer today: soft is only quieter, not darker); (4) whether
> `pianohand`'s hand-built halo gives way to the resonator.

## Where the idea comes from

Flyfish ([delirioux/flyfish](https://github.com/delirioux/flyfish)), a firmware for the M-VAVE FM-1
built on Tuna and Felucca ([hugelton/Felucca](https://github.com/hugelton/Felucca)), plays a pedalled
Chopin arpeggio on a device with **8 voices**. A pedalled arpeggio wants twenty. Their answer
(`tuna_reso.c`): every note lets go of its key at once, and the pedal's ring comes from ONE bank of
six delay-loop strings that the whole mix excites, retuned to the harmony at each pedal change.

**Licence: those repos are GPL-3.0. We took the IDEA, not code** — this implementation is written
from the textbook form (a Karplus-Strong loop with no pluck). Their sympathetic strings for the PHYS
engine (`phys_symp.c`) are a port of Mutable Instruments Rings (MIT); if we ever want Rings' richer
strings (dispersion, detuned copies), port from Rings itself, with attribution.

## Why it is a primitive and not a recipe (ADR-0015's gate)

Nothing shipped can be tuned to a HARMONY on a send. `INSTR_MODAL` is a resonator, but per voice:
it rings at the note played, which is the opposite of sympathetic. `reverb()` is untuned. INSTR_PIANO's
own sympathetic tap feeds a voice's 3rd partial back into itself, so it never rings at a note the
pianist did not strike. Faking it with a slow-attack second voice (what `pianohand`'s halo does) costs
voices, which is the very thing the resonator saves.

## How it works

Eight strings (`SYMP_STRINGS`), each a delay line of `SYMP_LINE` = 1024 samples with a one-pole
low-pass inside the loop. Slots send MONO into it (`instrument_resonator`), DC-blocked; even strings
lean left, odd lean right (75/25), so the ring is wide while the piano can stay narrow. Its output
goes back into the master send return stage and on into the reverb, at the largest master-reverb send
among the slots exciting it.

Two things the obvious build gets wrong, both handled in `symp_coeffs()`:

- **Tuning.** The loop low-pass delays the loop by its phase delay at the fundamental, so a line of
  exactly SR/hz samples plays flat. That delay is subtracted from the line. Measured in `openstrings`'
  tail: every string within ±4 cents of its note.
- **Decay.** The low-pass also loses level per pass. The loop gain is set so gain × |H(f0)| gives the
  asked RT60 at the fundamental, so `bright` only decides how fast the UPPER partials die. RT60
  shortens with pitch, √(220/hz), as a real string's does.

**Retuning is a pedal change, not a glissando.** A string asked for the note it already holds rings
on (common tones carry over). A changed one is damped for 25 ms (a 30 ms RT60), then moved. Notes
below ~43 Hz fold up an octave (the line's length).

## Measured (openstrings, 2026-11-09)

- At `resonator(0.8, …)` with the piano sending 0.6, the strings sit ~3.5 dB under the dry piano; the
  cart ships at amount 0.6.
- Dormant until its first API call: bossa, pianohand and drawall render byte-identical to HEAD.
  soundcheck silent, `ctx-gen --verify` green.
