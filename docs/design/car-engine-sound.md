# Car engine sound: INSTR_MOTOR

STATUS: SHIPPED (2026-11-20), first cut. The voice, its gate and the enginelab A/B are in; tuning by ear is open.

An engine is not a note. The first car voice (`runtime/carengine.h`, born in enginelab) built one from
synth layers ridden once per frame: a saw at the firing rate through a resonant low-pass, a tremolo at the
same rate for the "chug", rasp and intake on top. Three things in that recipe read as a synthesizer, and no
amount of tuning fixes them, because they are the mechanism:

1. **Every firing was identical.** A real engine's cylinders never fire alike, and the pattern repeats once
   per CYCLE (two crank turns). That puts energy at the cycle's harmonics BETWEEN the firing harmonics: the
   rumble, the lope, a crossplane V8's burble. A perfectly periodic pulse train has none: a buzzer.
2. **The resonance moved with the revs.** The pipes and muffler resonate at FIXED frequencies and the firing
   harmonics sweep through them. A cutoff that tracks the pitch is a synth's habit, not a pipe's.
3. **60 Hz control can't make 150 Hz events,** and a cart has no audio-rate path, so this had to be an engine.

## The model (sound.h, `INSTR_MOTOR`)

Physically informed, after Baldan et al. 2015 ("Physically informed car engine sound synthesis for virtual
and augmented environments") and Farnell's *Designing Sound*:

- **Crank:** the note's frequency is the crank speed, rpm / 60 (800 rpm = 13.3 Hz). A glide is the revs.
- **Cylinders:** each fires at its layout's crank angle in the firing order. Its exhaust pulse is a fixed
  ~2 ms BLOWDOWN spike (gas dynamics, so an idle still cracks) plus a displacement tail in crank angle (it
  shortens as the revs climb). Each cylinder has a fixed strength imbalance and a fixed arrival offset (its
  own header length); each firing a little random strength and timing slip (`MODE_MOTOR_ROUGH`).
- **Banks:** a V engine's banks run separate pipes and the far one is quieter. A crossplane V8 fires each
  bank unevenly (90/180/270/180 degrees); summed into one pipe that would be an even 90 and the burble
  would vanish.
- **Exhaust:** per bank a waveguide pipe (round-trip delay in `ks_buf`, wall-loss low-pass, an inverting
  open end reflecting 0.80 to 0.92), so it RINGS from one firing to the next; the listener hears the wave
  that travelled the pipe, then a muffler low-pass and the tailpipe's radiation (the change in flow).
- **Intake / mech:** air roar while each intake valve is open, scaled by load; a high quiet tick per valve.
- **Level:** idle to cruise as is, 2:1 above cruise, a soft ceiling. Uncompressed, a straight pipe at
  redline measured +24 dB over a stock idle, which is honest and more than a game mix can hold.

Macros: harmonics = LAYOUT (single, inline-4, flat-4, inline-6, crossplane V8, flatplane V8, V12),
timbre = EXHAUST (stock to straight pipe), morph = LOAD (live). Aux, 0.5 = the layout's own: PIPE, ROUGH,
INTAKE, MECH.

## What it was measured against

CC0 recordings, `tools/refs/engines-cc0.txt` (`node tools/fetch-refs.js engines-cc0`). Findings that set
the model, in the order they changed it:

| finding | reference | what it changed |
|---|---|---|
| inline-6 at ~1400 rpm: in-between cycle harmonics 14-25 dB under the firing ones | `i6_inline_six` | per-cylinder fixed imbalance AND arrival offset (strength alone gave -35..-45) |
| lumpy V8 idle: nearly every cycle harmonic within a few dB | `v8_idle` | the near bank louder than the far one (0.45), each bank its own pipe |
| a 200-250 Hz hump at two different idle speeds | `v8_idle` | the pipe resonance is fixed, not tracking |
| the real V8 idle is a CONTINUOUS ring; ours was isolated clicks | `v8_idle` waveform | open-end reflection 0.3 → 0.8 (rang ~5 ms, now ~75 ms) |
| peak-to-RMS 11-18 dB; ours 20-27 | all | the blowdown spike as its own ~2 ms shape; output = the travelled wave, not the raw pulse |

Band-by-band spectra were NOT chased past that: they depend on the car and on where the mic stood (one
inline-6 recording's 317 Hz band is a pipe mode meeting a firing harmonic at that one rpm).

## The gate and the bench

- `node tools/motor-check.js` (13 checks, ~1.5 min): rumble on an inline-6 and a V8 within the measured
  ranges; the NEGATIVE CONTROL (ROUGH 0 must be an even machine, < -45 dB) and the two 20 dB apart; the
  V8 hump stays within a third of an octave as the revs double; load brightens; every layout audible at
  idle and under 0 dBFS at a straight-pipe redline. Renders `tools/carts/motorprobe.c`.
- `enginelab`: the dyno. N flips a combustion kind back to the old synth layers for the A/B.
- `carengine.h`: gas = inline-6 stock (backroads' 1970s sedan), diesel = rough inline-4, race = flatplane V8
  straight pipe + turbo, tractor = a big single, muscle = crossplane V8, boxer = flat-4. Electric, steam and
  nuclear have no cylinders and stay on the layers.

## Open

- **Ears.** Everything above is measured; the maker has not yet listened. Expect retuning of the spike
  width, the pipe lengths per kind and the intake/mech levels.
- **Overrun:** lifting at speed only weakens the pulses (load 0.12). Real overrun has misfires and
  afterfire; carengine's BURBLE pops stand in for it.
- **Diesel** is an inline-4 with a sharp, rough firing. A real diesel's clatter is combustion knock, a
  separate high band per firing; not modelled.
- **Doppler / distance / interior** (the in-car boom of a cabin resonance) belong to the cart or the spatial
  layer, not this voice.
