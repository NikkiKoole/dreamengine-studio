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
| a pitched ring / a chord in a hat | flatness ~0.02, peaks at single harmonics of one square | hats as **noise** (the real 808 mixes six oscillators into a dense hiss, not two) |
| "a locomotive", a drone on eighth notes | the ride stem louder than the kick, pulsing | **one physical cymbal = one voice** (`instrument_mono`); and a ride needs a bright shimmer layer, not only a low body |
| "woodblock" | a rim that rings a pitched tone for 10+ ms | a rim is a **click**: one swing and a damped ring, gone in ~5 ms |
| a kick/tom with no character | a missing strong mode ~2x up (real: 1-5 dB under the fundamental) | book the second mode as its own quieter voice |
| the 909's ROM hats sounding like FM | FM sidebands at 8-21 kHz whatever the filter | the METAL bank, low-passed |
| a knob that "stops working" halfway | decay stopped growing above the middle | the slot's own decay envelope caps any gate: set the slot's decay **per hit** from a table measured at the real knob's marks |
| a tune knob that sweeps octaves | real tom/snare tune spans ~2-10 semitones, unevenly | a measured table per voice |

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
- **The engine's per-voice filter** was broken above ~6 kHz until 2026-11-04 (fe643a60, baa291bd):
  any bright voice measured before that is suspect.

Related: [`checks-and-oracles.md`](checks-and-oracles.md) (the tool row),
[`instrument-recipes.md`](instrument-recipes.md) (the tuned recipes: cr78, tr808, tr909, minipops,
the METAL note), [`humanizing-sounds.md`](humanizing-sounds.md) (making a voice sound PLAYED).
