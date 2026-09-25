# Talking to real hardware synths: CV/gate

STATUS: EXPLORING (2026-09-25): an idea round, nothing decided. It came from a neighbour with a
Moog who saw the sound apps and brought up "a USB converter that sends and receives 5V". This doc
lists what that could mean and what we could do with it. No code, no commitment.

## What "a USB box that sends and receives 5V" probably is

Analog synths (Moog, Eurorack, most semi-modulars) talk to each other with **CV/gate**: plain
voltages on patch cables.

- **Pitch CV**: usually **1 V per octave** (+1 V = one octave up). Some older Korg/Yamaha gear
  uses Hz/V instead; Moog is 1 V/oct.
- **Gate**: high (~5 V, sometimes up to 10 V) while a key is held, 0 V when released.
  **Trigger** is a short pulse of the same kind.
- **Modulation CV**: any slowly changing voltage (LFO, envelope, random) patched into a filter
  cutoff, a VCA, a rate. Ranges vary (0..5 V, ±5 V, 0..10 V).
- **Clock**: a stream of gate pulses (e.g. 24 or 4 ppqn, or one per step).

There are two families of box that get a computer onto those cables:

| | MIDI → CV converter | DC-coupled audio interface |
|---|---|---|
| examples | Expert Sleepers FH-2, Kenton Pro Solo, Doepfer A-190, Mutable Yarns, Befaco, many semi-modulars' own MIDI input | Expert Sleepers ES-8 / ES-9, some MOTU interfaces (UltraLite, 828) |
| how the computer talks to it | sends **MIDI** over USB; the box turns notes/CCs/clock into voltages | sends **audio samples**; the interface does not filter out DC, so a sample value of 0.5 comes out as a steady voltage |
| direction | mostly out; some boxes also turn CV into MIDI | **both**: DC-coupled inputs read voltages as audio |
| what the software has to do | nothing new (we already send MIDI) | generate the voltages itself, per channel, at audio rate |
| flexibility | limited to what the box maps (notes, a few CCs, clock) | anything: audio-rate modulation, arbitrary envelopes, CV *input* |

"Sends and receives 5V" sounds more like the second family, but it's worth asking which box he has.

## What we already have (no new engine work)

- **MIDI out** (`docs/design/midi-out.md`, gated by `tools/midi-check/`): notes, CCs, clock. Paired
  with a MIDI→CV box, or plugged straight into a MIDI-capable Moog (Mother-32, DFAM,
  Subharmonicon, Grandmother, Matriarch all take MIDI), a cart can **sequence his synth today**.
- **MIDI clock sync** (`docs/design/external-clock-sync.md`, `runtime/sync.h`): a cart can follow
  his tempo, or lead it.
- **Audio input** (`mic_start` + friends, `de_audio_input()`, `docs/design/mic-and-sampling.md`):
  the engine can already hear what comes in from an interface. We haven't checked whether a
  DC-coupled input gets through our path without being filtered.
- **A virtual Eurorack**: `modrack` (`docs/design/modular-synth.md`). Its cables already carry
  gate / pitch / CV at control rate, and it has LFO, S&H, Turing-style random, quantizer, envelope,
  slew and logic modules.

So the cheapest possible experiment is: MIDI out → his synth, clock-synced, from an existing cart.

## Ideas, from small to big (none chosen)

1. **Play his Moog from a cart over MIDI.** Zero engine work. A sequencer cart (tb303/acidcandy
   style) drives his synth instead of our voice. Good for a first evening together.
2. **A "hardware out" module in modrack**: a jack that sends its gate/pitch/CV as MIDI
   notes/CCs, so a virtual patch can reach a real synth through a MIDI→CV box. Small, cart-land.
3. **Real CV out through a DC-coupled interface.** A modrack cable can leave the computer as an
   actual voltage. Needs engine work (see open questions).
4. **Real CV in.** His LFO or envelope comes back in and shows up in modrack as a module output,
   modulating *our* sounds. Possibly the easiest half of the DC-coupled story, since audio input exists.
5. **Hybrid clock**: our transport clocks his gear with gate pulses on a CV channel, or we follow
   his clock pulses from a CV input (a pulse train, not MIDI clock, so `sync.h` would need a new source).

The honest-core angle: idea 3+4 make modrack a bridge between a simulated rack and a physical one,
where the same cable model carries both.

## Open questions (for when this gets picked up)

- **Which Moog, which box?** This decides whether ideas 1–2 are enough.
- **Output channel count.** The engine renders **stereo** today. CV wants several dedicated
  channels (ES-8 = 8 out / 4 in), kept separate from the music mix.
- **Nothing on a CV channel may be processed.** The mix path has DC blockers
  (`dc_block` in `runtime/sound.h`, several places), soft-clipping (`de_tanhf`), master
  effects, pan laws. Any of those on a CV channel breaks it: a DC blocker makes a held pitch drift
  back to 0 V, and a soft-clip bends the pitch curve so notes go out of tune. CV channels would
  need a raw path that skips all of it.
- **Calibration.** 1 V/oct only stays in tune if we know exactly what sample value = what voltage
  on *that* interface. That means a tuning step (Expert Sleepers ship a calibration routine for
  this) and a stored per-interface table. `tools/tune-check.js` style gate on the far side?
- **Voltage range.** An interface outputs maybe ±10 V at full scale. Pitch over several octaves,
  gates at 5 V, modulation at ±5 V all have to be scaled per jack type.
- **Latency and jitter** between a MIDI note and our own audio, or between a CV gate and our
  audio (`tools/insert-latency.js` measured 0 samples on the audio input path; MIDI is a different path).
- **Platform.** Desktop native only at first? Web Audio and iOS both have their own story for
  multichannel, DC-coupled I/O.
- **How to test without the hardware.** A render-to-WAV of the CV channels plus a checker (held
  levels exact, gates square, 1 V/oct steps exact) would let us gate it headless, like the other
  audio oracles.

## Related

- [`modular-synth.md`](modular-synth.md): the modrack patcher, the natural home for CV jacks
- [`midi-out.md`](midi-out.md): what already leaves the engine as MIDI
- [`external-clock-sync.md`](external-clock-sync.md): following / leading a tempo
- [`mic-and-sampling.md`](mic-and-sampling.md): the audio-input path CV-in would ride
- [`stereo.md`](stereo.md): the current two-channel output model
