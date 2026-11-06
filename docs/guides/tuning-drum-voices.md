# Tuning a drum voice against a real recording

How the CR-78, TR-606, TR-77, TR-808, TR-909, Mini Pops MP-7, drumhand and drumkit.h's ACOUSTIC voices were made to sound like the
real things (2026-11-04), written down so the next machine does not start from scratch. The
short version: **pick the reference by ear, measure ours against it, fix the biggest difference,
listen, repeat.** The numbers find the fault; the ear decides whether it is fixed.

## The loop

1. **Get references.** Free CC0 one-shots: `node tools/fetch-refs.js` rebuilds
   `build/refs/drums-cc0/` from the committed manifest `tools/refs/drums-cc0.txt` (it re-checks
   every license and refuses anything that is not CC0). Sample packs that are only for learning
   by ear (never shipped) stay local; leads in [`tools/refs/README.md`](../../tools/refs/README.md).
   Best of all is a set taken from ONE machine at its knob marks (the 808 and 909 sets were):
   then you can check that our KNOBS behave like the real ones, not only one sound.
2. **Pick the reference by ear first.** A voice can match every number of a reference and still
   be wrong, because the reference was. drumhand's ride matched a dark, modified "wash" ride
   perfectly and sounded like "a train locomotive comes by". Listen to a few, choose the one that
   sounds like the instrument you mean, then match it.
3. **Measure.** `node tools/oneshot-compare.js <ref.wav> <ours.wav> [--at s --len s]`, or
   `--render <cart> --spec <hits.json> --rel kick` for a whole kit (example spec:
   `tools/oneshot-specs/drumhand.json`). It reports pitch, the 10/50/90% energy band, spectral
   peaks, flatness (0 tone .. 1 noise), time to -20 dB, the early envelope curve, and level
   relative to one hit.
4. **Fix the biggest difference, re-measure, then render a file for the ear.** Always put a real
   version next to ours: lay the real one-shots on the same pattern at the same tempo and
   loudness (a few lines of node; see the drum A/B renders in this session's history) so the only
   difference is the sound.

## What the numbers kept finding

| Symptom by ear | What was measured | Fix |
|---|---|---|
| hiss, harsh, "hard to listen to" | energy up to 15-20 kHz (a real cymbal or hat stops at ~11-14) | **low-pass** the METAL bank (its bright noise lives at 15-20 kHz); a high-pass keeps exactly that. For noise voices use a **resonant** band (a 2-pole band's 6 dB/oct skirt runs on to 20 kHz), centred ~0.75x the real median |
| a dark ride "in a certain pitch", "tones to it" (drumhand, 2026-11-05) | flatness 0.01: the energy in a dozen square modes, whatever their tuning | a cymbal is a **struck resonator**, not a chord: a short noise stroke sent fully into a private reverb tank (`reverb_bus` + `instrument_reverb_bus(slot, tank, 1)` + `reverb_bus_fx(FX_EQ)` to colour the tail). The tank's dense modes are the wash; nothing has a pitch |
| "too much hiss" in the groove though one hit matched | the level drops 6.5 dB between strokes, the real ride 10: the tails PILE UP into a bed | measure the **groove stem** (`--solo-slot`), not one hit: octave bands + the dip between strokes. A fast-dropping stroke with a quiet tail, not a swell |
| the struck-resonator ride is right in every band but "missing the real realistic sound" (drumhand, 2026-11-06) | a real ride is ~2000 decaying modes, ~300 of them in close beating PAIRS; a reverb tank has dense modes but not a plate's | take the cymbal's **own modes** from the recording and play them (see [Taking a cymbal's modes](#taking-a-cymbals-modes)): 50 was the first count that sounded like one by ear |
| a pitched ring / a chord in a hat | flatness ~0.02, peaks at single harmonics of one square | hats as **noise** (the real 808 mixes six oscillators into a dense hiss, not two) |
| "a locomotive", a drone on eighth notes | the ride stem louder than the kick, pulsing | **one physical cymbal = one voice** (`instrument_mono`); and a ride needs a bright shimmer layer, not only a low body |
| "woodblock" | a rim that rings a pitched tone for 10+ ms | a rim is a **click**: one swing and a damped ring, gone in ~5 ms |
| a kick/tom with no character | a missing strong mode ~2x up (real: 1-5 dB under the fundamental) | book the second mode as its own quieter voice |
| a dull boom under a tom/kick that matches on every other number | 5-17 dB more energy below 50 Hz than the real one: a switched-on decaying SINE leaves a DC thud a resonator does not have (TR-77) | a gentle **highpass** an octave under the note |
| the 909's ROM hats sounding like FM | FM sidebands at 8-21 kHz whatever the filter | the METAL bank, low-passed |
| a knob that "stops working" halfway | decay stopped growing above the middle | the slot's own decay envelope caps any gate: set the slot's decay **per hit** from a table measured at the real knob's marks |
| a tune knob that sweeps octaves | real tom/snare tune spans ~2-10 semitones, unevenly | a measured table per voice |

## Taking a cymbal's modes

When a cymbal has to sound *real* rather than plausible, stop building it out of generic parts and read
its modes off a recording: `python3 -I tools/mode-extract.py <one-hit.wav> --top 400 --out <dir> --wavs` (~24 min for a 6 s hit; `--fast` ~3 min, less exact, for exploring; `--from <dir>/modes.json` refits a different `--top` in ~20 s).
It prints a C table for `instrument_modes()` (Hz, amplitude at the strike, decay, starting phase) and one
for `instrument_modes_noise()` (half-octave decaying wash BEDS for the modes too weak to list, and 6 ms
stick TICKs where the modes leave one), then you play them on an `INSTR_MODAL` slot made mono, so one cymbal is one voice and every
stroke strikes the bank already ringing. `drumhand`'s ride (K) is the worked example.

What the ride (`build/refs/drums-cc0/ride.wav`) taught, 2026-11-06:

- **It is ~2000 modes, and the count you need is set by ear, not by the error.** Half the modal energy
  sits in 9 modes (a cluster at 400-550 Hz), 90% in 129, 99% in 474; but 25 modes sounded like a bank
  of oscillators, **50 was the first count that sounded like a cymbal, and 400 the first as RICH as the
  recording** (200 was not). Two different bars: ask which one you are tuning for. Render the counts
  (`--wavs`) and listen next to the original before choosing. 400 modes cost ~2% of a core.
- **The noise is what the modes leave, measured, not added for "air".** 50 modes hold almost
  nothing above 3.3 kHz; the recording's 1-16 kHz is 60-99% "noise" (the weak modes). Its decay rate
  changes STEEPLY with frequency (~6 dB/s at 4 kHz, 11 at 5.7, 17 at 8, 54 at 11), so **fit it in
  half-octave bands**: in octave bands, the slow 4 kHz wash's spill out-rang the real 8 kHz wash by
  10-20 dB late in the tail, and no fit could subtract it. The bands are also UNMIXED through the
  engine's own filter shapes (each band leaks into its neighbours), not fitted one at a time.
- **Keep every mode's measured starting PHASE.** A cymbal's close pairs start out of phase and swell
  into phase: the ride's 2 kHz band rises ~12 dB over the first second. Started all in phase, the same
  modes made a 2 kHz spike 10 dB hot in the first 0.3 s and no swell.
- **A soft bow stroke is LINEAR.** Its 2 kHz band swells around 0.5 s, which looked like the energy
  cascade of a nonlinear plate, but the linear modes reproduce it (given their phases): it is close pairs beating. Expect
  the cascade (a hit that blooms brighter) on a hard crash, which a linear bank cannot make.
- **Peak-picking an FFT lies on a cymbal**: it counts one beating pair two or three times. Use the tool.
- **Mind the filter skirts.** A 2-pole band-pass's 6 dB/oct skirt let the 250 Hz tick leak 10 dB of
  bass into 125 Hz (the recording has nothing there); two stages plus a 4-pole high-pass at 180 Hz
  fixed it. The engine's own high-pass at resonance 0 is critically damped (Q 0.5): use resonance ~4
  (Q ~0.7) or it eats the octave above the cutoff too.
- **Give every noise band its own noise.** Fed one shared sample, neighbouring band-passes (near opposite
  phase where their skirts overlap) cancelled, and the wash measured 3-6 dB thin between the octaves.
- **Compare a SINGLE hit through the same chain, not the groove.** In drumhand the recorded ride plays
  through a sample slot gated at 3 s, so in the groove its tails are cut and a voice that rings out
  naturally reads 2-4 dB "hot" in the slow bands. Render one stroke of each (`PROBE_TECH`) instead.
- Still off on the ride: the first 0.3 s is 3-5 dB bright at 1-4 kHz, and 8 kHz rings long after ~1.5 s
  (~50 dB down): one straight-line decay per band, where the real wash curves.

## The envelope SHAPE matters as much as the decay time

Two machines, opposite answers, both found by ear first:

- **Mini Pops MP-7 noise voices** (maracas, cymbal, snare; and every TR-606 voice, same fix): held at full level then released, they
  plateaued and smeared into a wash, and the bossa lost its swing. A real hit falls fast and then
  trails: a short gate into an **exponential** release (`instrument_release_shape(slot,
  RELEASE_EXP)`, release ~2.2x the -20 dB time).
- **TR-808 open hat**: the exponential fade was wrong here ("less impact, more tail"). The real
  one is a **loud steady burst** that sinks only ~10 dB over 300 ms, then **shuts** (-47 dB by
  360 ms). A straight-line decay fits it.

So compare the envelope **curve**, not only the -20 dB time: render one hit of each and print the
level in 20 ms steps side by side.

## Swing lives in the dynamics

On a busy line (maracas on 16ths, hats on 8ths) the groove is the **level pattern** of the 16ths:
a real bossa's high band pulses 0 / -7 / -4 / -7 per 16th. Ours once wobbled at random because
each noise hit came out at a random level (the engine never reseeds a voice's noise, and a very
resonant band turns that into big level jumps). Measure the peak of each 16th in the >4 kHz band
over one bar, ours against the real pattern; if the alternation is missing, it will not swing.

## Traps that cost a measurement

- **Normalized sample sets** (most packs, the 909 one, the MP-7 one) say nothing about level
  between voices. A set recorded at one gain (the 808 set, the CR-78 Boxed Ear set, lennartgreen's
  kit) does: use it with `--rel kick`.
- **Choke inside the window**: an open hat choked by the next closed hat reads as a huge
  "wobble". Measure single uncut hits, and A/B single hits against single hits.
- **One real hit vs a pattern**: compare like with like, or the ear hears the difference in the
  file, not the sound.
- **A long FFT window on a single hit hides its attack.** A Hann window of ~190 ms that starts at the
  onset weights the first 30 ms near zero, which is where a kick's thump lives: it once reported a real
  kick as 11-23 dB "more broadband" than ours when the band was within 3 dB. Compare a hit's bands as
  ENVELOPES in 10 ms steps, or compare whole grooves (many hits average the taper out).
- **The engine's per-voice filter** was broken above ~6 kHz until 2026-11-04 (fe643a60, baa291bd):
  any bright voice measured before that is suspect.

Related: [`checks-and-oracles.md`](checks-and-oracles.md) (the tool row),
[`instrument-recipes.md`](instrument-recipes.md) (the tuned recipes: cr78, tr808, tr909, minipops,
the METAL note), [`humanizing-sounds.md`](humanizing-sounds.md) (making a voice sound PLAYED).
