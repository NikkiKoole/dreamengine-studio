# Patch matching: a sample in, a dreamengine patch out

> **STATUS: SHIPPED (2026-09-06)** ‑ [`tools/patch-match/`](../../tools/patch-match/) (1485 lines, no
> engine changes). Throw a WAV at it and it hands back eight patches whose renders sound as close to
> it as this engine can get, as pasteable `instrument()` / `instrument_tape()` blocks plus a WAV per
> candidate to A/B. Verified by a self-recovery oracle, not by assertion. CLI only: there is no cart,
> no editor button, and §10 explains why that is a design fork rather than a missing afternoon.

The ask was Synplant's Genopatch: drop in a sound, get parameters back. The answer needs no machine
learning, because the expensive half of that product is not the neural net.

## 1. Why this is a search, not a model

The engine is already a function from parameters to audio. Add a `distance(a, b)` and patch matching
becomes search: try parameter vectors, render them, keep what sounds closer. That is what Synplant 1
did with its breeding UI, and what Genopatch does underneath. Its neural net only *seeds* the
population; it does not replace the search.

The hard part is the distance function. Comparing waveforms sample by sample is useless, since two
identical-sounding saws a few samples out of phase score terribly. What works is a
**multi-resolution log-mel L1 distance**: four FFT sizes from 256 to 2048 (a short window sees the
attack, a long one sees pitch and detune beating, and no single size sees both), mel-binned so the
top octave's hiss does not outvote the harmonics, log-scaled so a quiet partial still counts, and
both signals RMS-normalised first so absolute level is not part of the comparison.

## 2. Running it

```bash
bash tools/patch-match/build.sh tools/patch-match/pm.c build/pm
./build/pm "some sample.wav" --jobs 4
```

Writes `build/patch-match/<name>/` containing `target.wav` (the prepared target, so you A/B against
what was actually scored), `cand-1..8.wav`, and `patches.txt`. Roughly 3 to 7 minutes per sample on
an M1 across 6 processes; `--quick` trades accuracy for about a third of that.

Flags: `--quick` `--window <s>` `--jobs <n>` `--seed <n>` `--note <midi>` `--out <dir>`
`--engine <NAME>` (force or deep-dive one engine) `--selftest <NAME>` (the oracle, §6) `--stage1`
(stop after the race) `--no-polish` (§7) `--race-pop` / `--race-gens`.

The other two binaries are gates: `bench.c` (the render protocol, §5a) and `detents.c` (the
snapped-macro map plus `--check`, §7).

## 3. Target preparation

Target and candidate must differ only in how they were made, so everything that is not about sound
is removed first: stereo folded to mono, the file's rate resampled to the engine's 44100, leading
silence trimmed so the attack lands at sample 0 in both (a target whose note starts 80ms late scores
every candidate on its own silence), and the pitch detected by autocorrelation so the search plays
the right note instead of spending dimensions rediscovering one.

That last step is worth its own line: measured across the 50 samples of one commercial toy-keyboard
pack, 44 landed within a few cents of a C, and the six that did not were exactly the files whose
names say they are not single steady notes (two glissandi and a chord). The detector reports 0
rather than guessing when nothing is clearly pitched.

## 4. The three stages

A patch here is not a smooth blob of knobs. The biggest decision is **which engine**, and that is
discrete; throwing every dimension at one optimiser makes it rediscover that choice through a wall
of continuous parameters that mean different things per engine.

1. **Engine race.** All 18 engines get a short, cheap search on a small window. The output is a
   *ranking*, which is often the more useful answer: "that is a MALLET, then EPIANO, then PLUCK".
2. **Voice refine.** The top three get the real budget, seeded with their own race winner, three
   seeds each. This is where the eight candidates come from.
3. **Fx fit,** with the voice **frozen**. The freezing is the point. An optimiser allowed to move
   both halves will smear reverb over a wrong voice to fake a decay, and then neither number means
   anything. Held apart, you can read which half did the work.

The four bare waves are in the race on purpose: a filtered saw or square *is* a toy keyboard sound,
and excluding them would force every such target onto a physical model that has to work to imitate
one. NOISE is out, since the targets are pitched.

## 5. Four things that had to be measured

Each one is a place where the obvious implementation is wrong, and none was guessable in advance.

### 5a. A candidate must render on a FRESH engine instance

The same patch rendered differently note to note inside one instance. The difference started at
sample 0 and a flush four times longer did not help, so it was never a ringing tail: it is per-voice
state carried across notes. A fresh `de_instance_create` per candidate fixes it byte-exactly **and
is cheaper**, 26.3ms against 32.9ms, because it skips the flush render entirely.

Without this, a candidate's score depends on what was evaluated before it, and the optimiser is
partly chasing noise. `bench.c` gates it and carries `-shared` as a negative control, which
reproduces the defect (6 of 6 assertions red) so a green run means something. Also measured there:
1000 create/destroy cycles leave RSS flat at 18.2 MB, so the pattern does not leak.

### 5b. The RENDER is the bottleneck, not the loss

26ms per second of rendered audio against 5.7ms for the distance. This is the opposite of the usual
advice for this kind of tool, and it means effort spent optimising the analysis is wasted. The loss
is already cheap because its twiddles are precomputed and each mel band stores only its non-zero bin
range (a triangular band touches about 1% of the spectrum, so the naive form spends 99% of its time
multiplying by zero).

### 5c. The fx stage must be handed BYPASS as a starting point

Every effect switched off is a point in the fx space and scores exactly the frozen voice, so the
stage should never be able to lose. The first version did: 0.478 became 0.491. Across 23 randomly
initialised dimensions the search simply never found the off switch. Seeding individual 0 of the
population with the bypass vector makes the stage mathematically unable to hurt, and it now returns
exactly the voice loss when the effects do not help.

### 5d. Dimensions split by what a dial DOES, not by which API owns it

`LFO_PITCH` is a voice call and `instrument_tape` wow is an effect call, but they are the same claim
about a sound: the pitch wobbles. With vibrato in the earlier stage it won by arriving first, and a
cassette's flutter came out described as an 0.85-semitone vibrato. Moving every periodic modulation
into the fx stage, priced against each other by the sparsity cost, fixed the story at no cost to the
number:

| | before | after |
|---|---|---|
| loss | 0.2439 | 0.2421 |
| pitch wobble attributed to | `LFO_PITCH` 0.849 semitones | `instrument_tape` wow 0.638, flutter 0.508 |

The fx stage also charges a small cost per effect switched on. Without it the optimiser leaves all
eight slightly engaged, which scores a hair better and tells you nothing you can act on.

The same rule moved **fold** the other way. `DRIVE_FOLD` is a wavefolder — an oscillator operation,
not a send — so `V_DRIVE` / `V_DRIVEMODE` live in the voice stage (engine-reach §3). Leaving them
in fx froze the voice before the folder could ride cutoff. The fx-enum slots stay so old `pm:vec`
lines still parse; they are no longer searched.

## 6. The oracle, and what it does not prove

`--selftest <ENGINE>` renders a patch we chose with the real engine, throws the parameters away, and
asks the search to find them again. It is the only thing that separates "this engine genuinely
cannot make that sample" from "the search is broken", because here the answer is reachable by
construction.

Measured over 24 runs (6 engines x 4 seeds): the correct engine is in the **top three 24/24** but
**first only 21/24**. `INSTR_PD` is never first, because its phase-distortion tones are reachable by
a filtered SAW and friends, so it is genuinely ambiguous rather than badly searched. Continuous
macros come back to about three decimals (FM timbre 0.280 to 0.278, morph 0.740 to 0.738).

That measurement found a real bug rather than just reassuring: PD ranked **third** on one seed while
`--quick` refined only the top **two**, so quick mode could discard the correct engine before stage 2
ever saw it. Stage 2 now refines three engines always, and quick mode buys its speed from population
and generations instead.

**What a passing oracle does not cover.** It says the search can find a patch the engine can make.
It says nothing about whether the loss function agrees with a human ear, which is the one failure
mode none of these gates can see. If a candidate that scores worse sounds obviously closer, that is
a fact about the loss, and only a person can report it.

**A degeneracy that reads as failure and is not.** On self-ringing engines the amp envelope barely
touches the sound, so decay, sustain and release come back "wrong" (0.44 recovered as 0.77) while
the render matches. Many settings there are genuinely identical.

## 7. The snapped macro axes, measured

`studio.h` says several macros are snapped but never says how many positions or where. The
measurement is exact rather than heuristic: a snapped axis quantises before the DSP, so every value
inside one detent renders **byte-identically**, and sweeping the macro and grouping identical renders
recovers the boundaries with no threshold to argue about. A continuous axis changes at every step and
reports one group per step, which is how the two are told apart.

| engine | axis | detents | what they are |
|---|---|---|---|
| FM | harmonics | 10 | carrier:mod ratios |
| ORGAN | harmonics | 8 | drawbar registrations |
| PD | harmonics | 8 | wavetypes |
| PIANO | harmonics | 6 | the six voicings |
| EPIANO | harmonics | 3 | Rhodes / Wurli / Clav |
| any wavetable | unison | 7 | voice count 1..7 (`instrument_unison`) |
| SAW | bandlimit | 2 | naive vs PolyBLEP |

The last two match `studio.h`'s prose exactly, which is a good sign the method reads the real thing.
`PLUCK` morph (79 groups), `PIANO` timbre (53) and `BOWED` harmonics (85) are quantised too finely to
enumerate and behave as continuous.

This matters because differential evolution moves by **differences** between parameter vectors: on a
stepped axis it is climbing a staircase while expecting a slope. With the centres known the search
simply tries each one. A/B on a fixed seed with `--no-polish`: three engines better, two worse, and
the one large win (PIANO 0.165 to 0.092) outweighs the two small losses. Per run the polish can only
tie or improve, since the incumbent is kept; the regressions come from the polished race winner
seeding stage 2 down a different trajectory.

**On a snapped axis a raw-value difference is not an error.** FM truth 0.620 against found 0.694 is
the same detent, so it is an exact hit that looks like a 12% miss. The oracle prints the detent index
for exactly this reason, and the polish snaps ties onto a centre so the emitted snippet names the
detent instead of a number that merely falls inside it.

`detents.c --check` re-measures and fails on drift, so the table in `pmpatch.h` stays a recorded
measurement rather than a copied constant.

## 8. Reading a loss number

A loss is meaningless without its scale. Against one target:

| | loss |
|---|---|
| itself | 0.000 |
| best match found | 0.18 to 0.42 depending on the sample |
| an unrelated real sample | 1.00 mean (0.74 to 3.2) |
| white noise | 1.65 |

So 0.18 is a good match, 0.25 is decent, and 0.42 is the engine telling you it does not have that
sound.

Measured on ten samples from a commercial toy-keyboard library (2026-09-06/07), best candidate:

| sample | loss | engine | what the fx stage did |
|---|---|---|---|
| `ambitone` | 0.175 | PIPE | 0.329 → 0.175, nearly halved |
| `analok` | 0.182 | SAW | nothing (voice alone won) |
| `elenet` | 0.186 | REED | 0.231 → 0.186 |
| `cassette square wave` | 0.244 | PIPE | 0.268 → 0.244 |
| `toytone` | 0.262 | FM | 0.291 → 0.262 |
| `lapharp-ebow` | 0.290 | BOWED | 0.334 → 0.290 |
| `wisp` | 0.350 | FM | 0.397 → 0.350 |
| `sklocken` | 0.418 | FM | barely |
| `birdtopper` | 0.534 | BRASS | **nothing at all** |
| `taplay` | 0.619 | PIPE | 0.644 → 0.619 |

Three things in that table are worth more than the numbers.

**A flat top eight is the failure signature.** `taplay` (0.62–0.68 across PIPE, FM and VOICE) and
`birdtopper` (0.53–0.59 across BRASS, REED and BOWED) both spread their eight candidates across a
band narrower than the gap to a good match, with unrelated engines scoring alike. That flatness says
"nothing here is distinctly right", which is a more useful signal than the loss value itself. On
`sklocken` the same conclusion was confirmed the harder way: MALLET can reach the target's decay but
scores 1.58 even at its best macros, so no search budget would have helped.

**`lapharp-ebow` is the case a number cannot settle.** BOWED took the top three places at 0.290 and
EPIANO the next three at 0.298. Those are two completely different physical stories about one sound
(a plucked string with an ebow sustaining it is genuinely both), separated by less than the run-to-run
spread. A person can tell which is right; this loss cannot.

**Effects earn their place unevenly, and that is informative.** They did roughly half the work on
`ambitone`, and *nothing whatsoever* on `birdtopper`, where a 0.14s transient gives reverb and tape
nothing to act on. `analok` was won by the voice alone. A run where every candidate improves a lot
with effects (`wisp`) usually means the voice is wrong and the effects are papering over it.

## 9. Two bugs this turned up

**A `MODE_*` dial has no neutral value.** A "neutral" 0.5 sits on the wrong side of
`MODE_BOW_PIZZ`, whose threshold is `>= 0.5`, so every BOWED candidate in the race was a
**pizzicato**, the one thing a violin is not. `MODE_ORGAN_PERC_THIRD` and `_SLOW` have the same
shape. Fixed by having a patch record how many MODE dials it *owns* and leaving the rest unwritten,
which is safe precisely because of §5a: with a fresh instance there is nothing to inherit, so
unwritten means the engine's own default.

**`INSTR_VOICE` ignores all three of its advertised macros when set on the slot.** Found here, not
fixed, written up as [`audio-notes.md` §31](audio-notes.md).

## 10. Open: everything past the command line

Option A of the cart fork is in the editor (2026-09-11): drop a WAV on the window, it spawns this
CLI, you audition the eight candidates and paste one. That is still a **tool**, not a cart — the
remaining fork (option B, your ear as the loss) has its own lifecycle in
[`patch-matching-cart.md`](patch-matching-cart.md). The short version: two of the four options need
no engine change at all, the tempting one (let a cart render candidates silently) is structurally
wrong rather than merely unbuilt, and the reframe that does most of the work is that Genopatch and
Synplant 1 are two different products with wildly different costs.

Smaller open items, none of them forks:

- A `node tools/patch-match.js` wrapper, so it behaves like the rest of the shelf. Every other tool
  is `node tools/x.js`; this one is a C tree you compile by hand, because it has to link the engine.
- Snapping the two coarse axes (`PLUCK` morph at 79 groups, `PIANO` timbre at 53) that are quantised
  but too fine to enumerate the way §7 does.
- **Velocity, currently pinned at 5.** On most of these engines velocity is a timbral parameter, not
  just a level, so it is a real dimension the search cannot currently reach.
- A macro-liveness gate across the whole engine surface, which is the corollary of
  [`audio-notes.md`](audio-notes.md) §31: an advertised knob that does nothing is invisible to every
  gate we have. The sweep that caught `INSTR_VOICE` is three lines.

## See also

- [`engine-reach.md`](engine-reach.md) — what the console cannot make yet; tier 1 (duty / unison / fold in the voice stage) shipped into this matcher.
- [`patch-matching-cart.md`](patch-matching-cart.md) — the cart fork (§10); A is in the editor, B is `patchbench` (keep / breed / undo).
- [`docs/guides/checks-and-oracles.md`](../guides/checks-and-oracles.md) for which gate to run when.
- [`docs/design/instrument-engines.md`](instrument-engines.md) for what each engine's macros mean.
- [`docs/design/audio-notes.md`](audio-notes.md) §31 for the `INSTR_VOICE` macro finding.
- [`docs/design/mic-and-sampling.md`](mic-and-sampling.md) for the other half of "the console hears":
  this tool matches a sample with the synth, that one plays the sample back.
