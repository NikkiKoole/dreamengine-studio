# Synth Secrets audit — the engine cross-checked against Gordon Reid's 63-part series

STATUS: EXPLORING — findings ledger, nothing approved. Every item below is a *candidate*, deliberately
not queued. The owner's rule (2026-07-28): **one small step at a time, and no engine change lands
without a cart where you can hear it.** So each item names its audible home before it names its code.

Companion to [`audio-notes.md`](audio-notes.md) (the sound HUB). This doc is the outside-in view: what a
canonical synthesis text says the machine should do, versus what `runtime/sound.h` actually does.
Nothing here is a bug report. Several divergences are deliberate and documented in the code; they are
recorded anyway so the choice stays a choice instead of decaying into an accident.

**Two passes so far.** §A-§D are the **architecture** pass (is the engine the right *shape*?), read
from the theory chapters. §E onward are the **recipe** passes, one instrument family at a time (does
one engine's *voicing* match the physical analysis?), and they carry measurements. Brass is done;
the remaining families are listed in §D5.

---

## The source

Gordon Reid, **Synth Secrets**, *Sound On Sound*, 63 monthly parts, **May 1999 to July 2004**. The
owner supplied a 335-page compiled PDF. It is a copyrighted magazine series, so **it is not in the
repo** and must not be committed. It arrived in a macOS drag-cache directory
(`~/Library/Caches/com.apple.SwiftUI.Drag-*/Synth Secrets.pdf`) which the OS will eventually purge; if
you need it again, ask the owner, or read the individual articles, which SOS still hosts free at
`soundonsound.com` (URL pattern `/techniques/synth-secrets-partN`).

**Citations here are stable regardless**: part number plus issue. Part N maps to consecutive monthly
issues from Part 1 = May 1999, so Part 18 = October 2000, Part 63 = July 2004. Page numbers are given
where the article's own footer supplied one. Text was extracted with `pdftotext -layout`; the PDF has a
real text layer, so the quotes below are verbatim, not OCR guesses.

Part index, for orienting: 1-2 harmonics and the physics of percussion · 3 signals/modifiers/controllers ·
4-6 filters · 7-9 envelopes, gates, triggers, VCAs · 10 modulation · 11 AM · 12-13 FM · 14 additive ·
15 ESPs and vocoders · 16-17 sample and hold to sample-rate conversion · 18 note priority and triggers ·
19 duophony · 20-21 polyphony and voice assignment · 22 springs/plates/buckets to physical modelling ·
23 formant synthesis · 24-27 wind and brass · 28-30 plucked strings · 31-35 drums (timpani, bass drum,
snare) · 36-40 cymbals, bells, cowbells · 41-43 pianos · 44-46 strings, string machines, PWM ·
47-50 bowed strings and articulation · 51-53 flutes and pan pipes · 54-58 tonewheel organs and the
Hammond · 59-62 analogue-to-digital effects and delays · 63 the conclusion.

---

## A. Where we already match the book

Recorded first so nobody "fixes" a thing that is right.

**A1. Ring mod is literally Reid's amplitude-modulation equation.**
`rm_process` ([`runtime/sound.h:1434`](../../runtime/sound.h)) computes `in * ((1-mix) + mix*cos)`.
Part 11 (SOS March 2000, p.90) derives AM as `A1 = (a1 + A2)cos(w1t)`, giving carrier plus sum plus
difference; a ring modulator is "merely a special case of an Amplitude Modulator" that removes both
inputs, which needs the modulator "precisely centred on zero volts" (AC-coupled). Our `mix` knob *is*
his `a1`/`a2` balance: it sweeps dry, through AM, to true ring mod at `mix=1`. The ARP 2600 needed a
DC/AC-coupled switch for that; we get a continuous control. Nothing to do.

**A2. FM gets right the one thing Reid calls impractical on analogue.**
`sound_fm_sample` ([`runtime/sound.h:2934`](../../runtime/sound.h)) is phase modulation with the index
`beta` in radians and the modulator at `freq * RATIO`. Part 12 (SOS April 2000, p.88) defines the
modulation index as `ß = Δwc / wm`, notes that "as the Modulator frequency increases, ß decreases", and
gives Figure 11 as a whole sub-patch built to hold ß constant across the keyboard, calling it "almost
impossibly difficult to calibrate perfectly ... This is the reason why FM is almost always implemented
using digital technology." A constant-radian index gives us that for free.

Our ratio detents (`RATIO[10]`, [`runtime/sound.h:2938`](../../runtime/sound.h)) also land on his named
cases from Part 13 (SOS May 2000, p.80-82): 1:1 gives "a 1/n harmonic series ... a filtered sawtooth
wave"; 1:2 gives "a truncated harmonic series with just the odd harmonics ... what you get when you
filter a square wave"; 1:3 is "reminiscent of the spectrum of a 33% pulse wave"; 1:4 is "again similar
to that of a square wave". We ship 1, 2, 3, 4, 5, 7 plus 0.5/1.5/3.5 as the deliberately clangorous ones.

**A3. The vowel formant table is verifiably correct.**
`vox_vowel_f` ([`runtime/sound.h:4048`](../../runtime/sound.h)) against Part 23's adult-male table
(SOS March 2001, p.122):

| vowel | ours (F1/F2/F3) | Reid | |
|---|---|---|---|
| "oo" | 300 / 870 / 2240 | 300 / 870 / 2250 | match |
| "ee" | 270 / 2290 / 3010 | 270 / 2300 / 3000 | match |
| "eh" | 530 / 1840 / 2480 | 530 / 1850 / 2500 | match |
| "cat" | 660 / 1720 / 2410 | 660 / 1700 / 2400 | match |
| "cup" | 520 / 1190 / 2390 | 640 / 1200 / 2400 | F2/F3 match, F1 differs |

Our bandwidths (`vox_vowel_bw`, [`runtime/sound.h:4074`](../../runtime/sound.h), 65 to 180 Hz rising
with Fc) match his "a bandwidth for all the formants of around 100Hz ... although the bandwidth
increases somewhat with formant frequency." He also confirms the three-formant floor: "your ears can
differentiate one vowel from another with only the first three formants present," and we run four.

**A4. Composite modulation sums the way he insists it must.**
Part 7's first and loudest "Synth Secret" (SOS November 1999, p.128) is: "What we usually call an
envelope generator may be only one contributor to the true envelope of a given parameter." Our mod
block ([`runtime/sound.h:6398-6440`](../../runtime/sound.h)) does exactly that: three LFOs, three
mod-envelopes and the envelope follower all accumulate into shared `cutoff` / `harm_mod` / `pitch_mul`
locals, additively for offsets and multiplicatively for pitch. His Figure 4 (two AD contours summing
into a 4-stage shape "you cannot obtain from what is commonly called a 4-stage ADSR") is reachable today.

**A5. The 808 cymbal is architecturally right.**
`tr808.h` runs the six-oscillator square bank through a highpass. Part 40 (SOS August 2002) dissects the
real TR-808 cymbal as pulse-wave oscillators into bandpass and highpass paths with AD and AHD contours.
Same family.

**A6. Voice stealing is arguably better than either of his options.**
Part 21 (SOS January 2001, p.160) gives two policies: rotate-and-steal (Figure 10, "note stealing on a
4-voice polysynth") or first-note priority, which "will delay the onset of later notes, and the results
may be even less desirable than note stealing" (Figure 11). `sound_find_voice`
([`runtime/sound.h:5022`](../../runtime/sound.h)) does a third thing: steal the *quietest* non-held
voice and pay its last output into a 3 ms declick tail. That is a deliberate improvement, not a drift.
(But see **B7** for the part of voice assignment we did *not* get right.)

---

## B. Drift

Ranked by audible cost. Each carries the book reference, the engine reference, and the cart where a fix
would be heard.

### B1. Portamento glides in linear Hz, not in pitch

- **Book:** Part 16 (SOS August 2000, p.187): "if you insert a simple Slew Generator into the keyboard
  CV signal path, you smooth the transitions at the oscillator's CV input, thus making the pitch glide
  from one note to the next. This, of course, is portamento ... I've depicted the slew as an
  exponential glide between voltages, as it would be on most vintage synths." Part 15 (SOS July 2000,
  p.193) adds the mechanism: a slew generator "is simply a 6dB/oct low-pass filter". The signal being
  lagged is the **pitch CV**, which is 1V/octave, so hardware portamento is exponential in *voltage*,
  i.e. close to even in semitones.
- **Engine:** [`runtime/sound.h:6348`](../../runtime/sound.h) —
  `v->freq += (v->freq_target - v->freq) * v->freq_slew`. A one-pole lag on **linear frequency**.
  Default coefficient `0.006` at [`runtime/sound.h:5233`](../../runtime/sound.h); `note_glide` sets it
  at [`runtime/sound.h:5489`](../../runtime/sound.h).
- **Why it matters:** with a one-pole in Hz, the instantaneous *pitch* rate is `(target-f)·k/f`. Glide
  C2 (65 Hz) to C5 (523 Hz) and the pitch shoots up almost immediately then crawls into the target.
  Play the same interval downward and it creeps at the start and eases at the end. The two do not
  mirror, and neither sounds like a hardware glide. This is the single clearest divergence in the audit.
- **Fix shape:** slew a semitone value, convert to Hz once per sample. Note the two knock-on risks: it
  is not byte-identical for any cart that glides, and the waveguide engines re-derive delay lengths from
  `freq` every sample, so `tune-check` and `psola-check` both want re-running.
- **Audible home:** `heldnotes` (its glide button is the whole demo). Secondary: `moog`, `sh101`, `tb303`.

### B2. Nothing tracks the keyboard

- **Book:** Part 6 (SOS October 1999, p.140): "If you use a resonant filter with moderate Q and make the
  cutoff frequency track the pitch, you can create a characteristic 'emphasised' quality that remains
  tonally consistent as you play up and down the keyboard." And at maximum Q: "If the filter tracks the
  keyboard CV accurately, you can then play it as if it were an extra oscillator." Part 23 defines the
  opposite case as the thing that makes formants work: the filter bank's response "is independent of the
  pitch of the source. To see how this differs from conventional synthesizer filtering (in which the
  filter cutoff frequency often tracks the pitch of the note being played) ..." His own patch listings
  carry it as a first-class control: the JX10 tables in Part 42-43 (SOS November-December 2002) list
  "56 Key Follow 24", and Part 33 (SOS February 2002) tells you to reach for "the 'VCF Kybd' (keyboard
  tracking)" slider.
- **Engine:** there is no keytrack anywhere in `sound.h`. Cutoff is absolute Hz at every entry point:
  `instrument_filter(slot, mode, cutoff_hz, res)`, `note_cutoff(handle, hz)`
  ([`runtime/studio.h:347`](../../runtime/studio.h)), `ENV_CUTOFF` "amount in Hz"
  ([`runtime/studio.h:452`](../../runtime/studio.h)), `LFO_CUTOFF` "depth in Hz"
  ([`runtime/studio.h:421`](../../runtime/studio.h)), applied additively at
  [`runtime/sound.h:6411`](../../runtime/sound.h) and [`:6427`](../../runtime/sound.h).
- **Why it matters:** two separate costs. First, a patch's character is not constant across the
  keyboard: a `+2000 Hz` pluck envelope is a huge sweep on a bass note and nearly inaudible two octaves
  up, so every one of our filtered patches is only voiced correctly in one register. Second, we ship
  three self-oscillating filters (`FILTER_LADDER`, `FILTER_DIODE`, `FILTER_STEINER`,
  [`runtime/studio.h:440-445`](../../runtime/studio.h)) and none of them can be played as a pitched
  voice, because their pitch does not follow the note. Reid's Juno 60 Figure 10 in Part 63 uses exactly
  that trick to get a third oscillator out of a one-oscillator synth.
- **Fix shape:** two independent pieces, and they are worth separating into two steps.
  (a) `instrument_keytrack(slot, amount)` where 0 is today's behaviour and 1 is full 1V/oct tracking.
  (b) express `ENV_CUTOFF`/`LFO_CUTOFF` depth in **octaves** instead of Hz, which is a surface change
  with a compatibility question attached (probably a new dest constant rather than a redefinition).
- **Audible home:** `22-filter` for the mechanism, `filterenv` for the envelope-depth half. The
  self-oscillation trick wants its own tiny cart or a mode in `22-filter`.

### B3. No note priority, no single/multi trigger

- **Book:** Part 18 (SOS October 2000) is dedicated to this. Four priority schemes: lowest, highest,
  last, first, each producing a *different* output from the same played sequence (his Figures 2a-4d).
  Crossed with triggering: single-trigger "retriggers only when you release all other notes" (Figure 8,
  "this, by the way, is exactly how a Minimoog works") versus multi-trigger, which "retriggers every
  time that you press a key" (Figure 9, the ARP behaviour), plus a third real variant that "retriggers
  on any transition between notes" (Figure 11). He counts it up: "We have four pitch-priority schemes,
  and six or more permutations of triggering/contouring. This means that there are at least 24 keyboard
  characteristics that you might encounter." Part 7 adds the reset-to-zero-versus-continue question:
  a contour that resets on every trigger "can lead to a very disjointed sound indeed ... It's horrible,
  and sounds like the instrument is swallowing its tongue. Glump!"
- **Engine:** nothing. `grep -riE "note_priority|mono_mode|legato|highest.note|lowest.note"` over
  `runtime/*.h` returns only unrelated prose. The engine is polyphonic with stealing; `solo.h` glides a
  single held voice, which is effectively last-note priority with legato, but as a cart-side convention
  rather than an engine policy.
- **Why it matters:** Reid's framing is that this is what decides whether a synth *feels* playable
  ("my playing sounded punchier on the Odyssey, and I could play at higher speeds than I could on the
  Minimoog. The reason for this was nothing to do with my playing ... The answer lay in the engineering
  within the instruments"). Concretely for us: every monosynth cart we ship (`tb303`, `acidrack`,
  `moog`, `sh101`) hand-rolls its own answer, so they silently disagree with each other about the one
  behaviour that most defines how a monosynth responds.
- **Fix shape:** this is the largest item in the audit and the one most worth breaking up. A first step
  could be a cart-land header (`mono.h`, alongside `keybed.h`) holding a held-note stack plus a
  priority policy plus a trigger policy, so it costs zero engine surface and the existing monosynth
  carts converge on one implementation. Only promote to `sound.h` if the header proves it needs to be
  there.
- **Audible home:** `sh101` or `moog` (a priority/trigger switch on the panel is itself the demo, and
  Reid's own A/B is "hold a low note and solo above it"). `heldnotes` for the mechanism.

### B4. Amp envelope is linear, mod envelope is exponential

- **Book:** weaker support than I expected, and I want that on the record rather than overstated. The
  series never devotes an article to envelope curvature. What it does say: the GX1's filter contour
  "decreases exponentially to zero volts" (Part 8, SOS December 1999, p.64), and portamento is
  "an exponential glide between voltages, as it would be on most vintage synths" (Part 16). Every
  hardware ADSR is an RC charge/discharge, so linear is a divergence from the machines, but **the book
  does not make this argument**, so treat it as a known difference rather than a defect the text
  supports.
- **Engine:** `sound_adsr_gated` ([`runtime/sound.h:4888`](../../runtime/sound.h)) is a linear attack
  ramp and a linear decay to sustain; the release at [`runtime/sound.h:6393`](../../runtime/sound.h) is
  linear from `rel_start`. `sound_ad_env` immediately below
  ([`runtime/sound.h:4898`](../../runtime/sound.h)) is a linear attack with an **exponential** decay,
  and its own comment states the asymmetry is intentional: "Exp decay (vs the amp ADSR's linear) is
  what makes the pluck 'pew' and the drum punch feel snappy."
- **Why it matters, if at all:** a linear amplitude decay against a roughly logarithmic ear reads as
  hanging then vanishing. But this is the highest-blast-radius change in the whole audit (it alters
  every note of every cart and every audio gate baseline), and the book does not demand it. Strong
  candidate for **opt-in per slot**, never a default flip.
- **Audible home:** `06-sound` or `20-instruments` as an A/B toggle. Do not touch this before the
  cheaper items are done.

### B5. Anti-aliasing covers one waveform on one code path

- **Book:** Part 17 (SOS September 2000, p.\[6206 in extract\]): "The answer lies in an effect called
  'aliasing' ... the 'aliased' frequency is (10kHz minus ...) ... Unfortunately, you can't remove
  aliasing once it \[occurs\]." Parts 16-17 are the whole sampling-theory pair, and Parts 59-62 keep
  returning to anti-alias and reconstruction filters as required parts of any delay line.
- **Engine:** `sound_polyblep` ([`runtime/sound.h:2771`](../../runtime/sound.h)) exists and works, but
  it is (a) opt-in per slot via `bandlimit` ([`runtime/sound.h:98`](../../runtime/sound.h)), (b)
  applied only when `v->wave == INSTR_SAW` ([`runtime/sound.h:6480-6481`](../../runtime/sound.h)), and
  (c) skipped entirely on the unison path, with the comment "unison saws stay raw by design".
- **Why it matters:** square and pulse alias freely, and PWM is the worst case because the duty edge is
  *moving*, which is precisely the waveform Part 46 (SOS March 2003) says is "ideal for creating string
  ensemble sounds". For a lo-fi console some aliasing is legitimately the aesthetic. The issue is that
  right now it is not a *choice*, it is an absence: there is no way to ask for a clean pulse.
- **Fix shape:** extend the existing `bandlimit` flag to the square/pulse path (a second BLEP at the
  duty edge). Cheap, opt-in, byte-identical when off.
- **Audible home:** `solina` or `juno` (PWM string machine, where the aliasing sits on top of the
  chorus). `lofi`/`bitcrush` are the counter-demo for keeping it raw.

### B6. Formant amplitudes can never be non-monotonic

- **Book:** Part 23 (SOS March 2001, p.124): "the relative gains of the formants can swap ... sometimes
  the lowest formant is the loudest, and sometimes it's the second or third." His own worked "ee" table
  on p.122 has F1 at 0 dB, **F2 at -15 dB and F3 at -9 dB**, so F3 is louder than F2.
- **Engine:** every row of `vox_vowel_a` ([`runtime/sound.h:4060`](../../runtime/sound.h)) begins at
  `1.0` and decreases monotonically. The shape he describes is unreachable.
- **Why it matters:** small, but it is the difference between four bandpasses and a voice. His other
  claim in the same passage is the actionable one: "the second formant is often the one that moves most,
  which suggests \[it\] is the most important clue to understanding speech."
- **Audible home:** `vowel`, `vox`, `voxlab`.

### B7. Voice assignment is fully deterministic and every voice is identical

- **Book:** Part 21 (SOS January 2001, p.162), the "Random Voice Assignment" box. Verbatim: "if the
  voices always play in the same order, you may occasionally hear a disturbing consistency as you
  perform, especially if you're playing a solo line ... If the voices speak in strict rotation, you'll
  hear your solo doing something like this: 'do-do-dee-do-do-do'. This will place your performance
  firmly within electronic territory. But if the synth's voices do not cycle in a predictable fashion,
  the same line may go: 'do-dee-do-do-dee-do' which will be much closer to the natural variations of
  tone and tuning of a 'real' musical performance." And on why the voices differ at all: "Each of the
  voices in an analogue instrument will sound slightly different from the others, maybe with a different
  amount of detune, or with filters that are slightly more open or closed. These differences, if they
  are not too extreme, are a major source of the so-called 'organic' warmth of vintage polysynths."
  He closes: "Nowadays, of course, digital synths have 'analogue feel' parameters that add small random
  fluctuations to the sound, giving rise to much the same effect."
- **Engine:** `sound_find_voice` ([`runtime/sound.h:5022`](../../runtime/sound.h)) scans from index 0
  and returns the first inactive voice. Play single notes and it returns voice 0 every time, which is
  *more* consistent than the strict rotation Reid is already complaining about. And every voice runs
  bit-identical DSP, so there is no per-voice variation of any kind to hear.
- **Why it matters:** this is a direct, sourced answer to the recurring complaint in
  [`audio-notes.md`](audio-notes.md) §17 ("Grit, darkness, weight — why everything still sounds
  clean"). We have been looking for that cleanliness in the effects chain; Reid points at the
  *allocator*.
- **Fix shape:** genuinely cheap and it is two separable steps.
  (a) A per-voice character offset seeded once per voice index (a few cents of detune, a small cutoff
  and level trim), scaled by one global `analog_feel(amount)` with 0 as the default so everything stays
  byte-identical until asked for.
  (b) Optionally round-robin the allocator instead of first-free, which alone makes (a) audible on
  single-note lines.
  Caution: (a) must be per-voice-index and deterministic, not `rand()`, or every audio gate in the repo
  loses reproducibility.
- **Audible home:** `polystress` or `voicestress` for the mechanism; any solo-line cart (`jangle`,
  `moog`) for the actual point. `voice-trace.js` and `--solo-slot` already exist to inspect it.

### B8. Blending a filtered wet against dry is a comb, not a gentler filter

- **Book:** Part 4 (SOS August 1999, p.121), and it is that article's headline "Synth Secret":
  "Filters not only change a waveform by attenuation, but distort it by individually phase-shifting the
  harmonics within it." Earlier on p.120: "when you mix two offset but otherwise identical signals, the
  phases of the individual frequencies define a filter. This, because of its characteristic shape, is
  called a Comb Filter." Part 6 repeats the warning for the parallel band-reject construction: "the
  phase shifts introduced by the two separated filters can cause all manner of side-effects when the
  signals are mixed together again."
- **Engine:** `formant_process` ([`runtime/sound.h:1287`](../../runtime/sound.h)) and `wah_process`
  ([`runtime/sound.h:1169`](../../runtime/sound.h)) both dry/wet blend a resonant bandpass against the
  unfiltered signal via `fmt_mix` / `wah_mix`. `FX_FILTER` takes no mix at all, so it is unaffected.
- **Why it matters:** this is not a bug and there is nothing to fix in the DSP. It is a **documentation**
  drift: "turn the mix down to soften it" is the wrong mental model, and anyone reaching for a 50% wet
  formant is getting notches they did not ask for. Belongs as a line in
  [`../guides/effects-recipes.md`](../guides/effects-recipes.md).
- **Audible home:** already audible in `vowel` at partial mix. No engine change.

### B9. Velocity does not touch brightness

- **Book:** Part 12 (SOS April 2000, p.90), stated as a plain fact about the physical world while
  criticising naive FM: "This is in marked contrast to natural sounds, where increased loudness almost
  always goes hand-in-hand with increased brightness."
- **Engine:** `hit(midi, instr, vol, dur_ms)` ([`runtime/studio.h:340`](../../runtime/studio.h)). `vol`
  scales amplitude only. Nothing couples it to cutoff or to the timbre macros. Some engines do their own
  velocity-to-brightness work internally (`ep_click_amp` scales with strike velocity, BRASS couples
  brassiness to level), which is exactly why the *absence* at the generic layer is easy to miss.
- **Fix shape:** an opt-in `instrument_veltrack(slot, to_cutoff, to_timbre)`. Or, cheaper and possibly
  better: leave the engine alone and document the two-line recipe, since the carts that care can
  already compute it.
- **Audible home:** `20-instruments`, `fingerdrums`.

### B10. Envelope times do not scale with pitch

- **Book:** Part 7 (SOS November 1999, p.132), listing what better contour generators offer: synths
  "that allow you to control each stage of the contour using Control Voltages as well as the
  generator's own knobs or sliders (a common use for this is to make a transient quicker at high
  pitches than at low ones, just as happens on acoustic instruments such as pianos and guitars)."
- **Engine:** `v->a_samp/d_samp/r_samp` are copied verbatim from the instrument at note-on
  ([`runtime/sound.h:5199-5202`](../../runtime/sound.h)) with no key scaling.
- **Fix shape:** one scale factor applied at note-on. Trivially cheap, opt-in.
- **Audible home:** `piano` or `upright` (where the real instrument's behaviour is most obvious),
  `20-instruments` for the A/B.

---

## C. Candidate additions

### C1. LFO delay / fade-in (delayed vibrato)

- **Book:** Part 8 (SOS December 1999, p.63), on the Korg MS-20's DAR envelope: "the DAR allows you to
  program a Delay that determines a length of time before the contour is initiated after the start of a
  note. This is particularly useful, for example, when creating delayed vibrato."
- **Engine:** no delay or fade field on the LFOs (`lfo_rate/depth/phase/shape/mod`,
  [`runtime/sound.h:146-148`](../../runtime/sound.h)). Because LFO depth is also not a modulation
  destination (see C2), **delayed vibrato is currently inexpressible in the engine at all.**
- **Why it is first:** one `int` per LFO, and delayed vibrato is the single most common expressive
  gesture on every wind, bowed, and voice patch we ship. Highest ratio of audible payoff to surface area
  in the whole audit.
- **Audible home:** `21-lfo` (add a delay knob), then `reed`, `pipe`, `bowed`, `brass`, `vox`.

### C2. LFO rate and depth as modulation destinations

- **Book:** Part 7 (SOS November 1999, p.132): "Another useful destination is the modulation speed.
  'Real' players do not add vibrato, growl or tremolo with electronic regularity, and changing the
  effect by applying contours to the LFO speed and depth can be most affective (this was one of the
  rare facilities that made the Yamaha GX1 and CS80 so revered)." Part 10 (SOS February 2000, p.94)
  makes the depth half structural: every modulation path in his Figure 11 runs through a VCA fed by a
  wheel, aftertouch or pedal, "because, without them, there would be no simple way to control the
  amount of vibrato, which would be particularly unmusical."
- **Engine:** the destination list is `LFO_PITCH/DUTY/VOLUME/CUTOFF/HARMONICS/TIMBRE/MORPH/PAN/DETUNE`
  ([`runtime/studio.h:418-426`](../../runtime/studio.h)) and `ENV_*` is the same set minus a few
  ([`runtime/studio.h:452-458`](../../runtime/studio.h)). Neither can target an LFO. `note_lfo()` can
  change depth live from cart code, but `lfo_depth` is not in the per-sample slew list
  ([`runtime/sound.h:6348-6362`](../../runtime/sound.h)), so a live sweep can zipper.
- **Why:** subsumes C1 (an env to LFO depth *is* delayed vibrato) and kills the mechanical-vibrato tell
  Reid names. Two new dest constants.
- **Audible home:** `21-lfo`, `lfoshapes`.

### C3. Resonance as a modulation destination

- **Book:** Part 6 (SOS October 1999, p.140): "The best filters also allow you to control Q using a CV
  source, giving voltage-controlled resonance."
- **Engine:** `flt_q_target` is already slewed per sample
  ([`runtime/sound.h:6351`](../../runtime/sound.h)) and `note_res()` already drives it. There is simply
  no `LFO_RESONANCE` / `ENV_RESONANCE` constant.
- **Why:** one enum value and one line each in the two mod loops. The plumbing exists.
- **Audible home:** `tb303`, `acidrack`, `djfilter`.

### C4. Ring-mod carrier waveform (the inharmonic-percussion unlock)

- **Book:** Part 32 (SOS December 2001, p.188), on getting the dense enharmonic partials of a struck
  membrane: "The solution is surprisingly simple. You can use a ring modulator ... If the modulator and
  carrier signals are merely sine waves (which, remember, have no harmonics), the result is not very
  useful; it's simply the carrier, plus two signals of frequency (w1 + w2) and (w1 - w2). **However, if
  you set both the modulator and the carrier to be harmonically rich sawtooth waves, the result is two
  complete harmonic series**". His Table 3 enumerates the resulting 25 partials from 100 Hz against
  87 Hz, then he adds a highpass "to remove the offending low frequencies" and a short decay, giving
  "the desired burst of closely spaced, enharmonic modes."
- **Engine:** `fx_set_ringmod(b, freq, mix)` ([`runtime/sound.h:1442`](../../runtime/sound.h)) and
  `rm_process` ([`runtime/sound.h:1434`](../../runtime/sound.h)) hard-code `sinf()` as the carrier.
  Reid's recipe is therefore not reachable: we can ring-mod a rich source with a *sine*, which gives
  only two sidebands per input harmonic.
- **Why:** roughly five lines (reuse `sound_osc` for the carrier), and it opens the entire analogue
  metal-percussion family the engine currently has no engine for. We have modal bars (`MALLET`, four
  modes at bar ratios) and circular membranes (`MEMBRANE`, Bessel ratios) but nothing that produces a
  dense inharmonic cloud, which is what cymbals, bells and gongs need.
- **Audible home:** `tr808`/`tr909` cymbal slots, `tabla`, `gamelan`, `glassharmonica`, `handpan`.

### C5. A 6 dB/oct one-pole filter (which is also the slew generator)

- **Book:** Part 5 (SOS September 1999, p.\[1279 in extract\]): "Filters with a 6dB/octave
  characteristic are used as tone controls in stereo systems, and occasionally within synthesizers as
  supplementary brightness \[controls\] ... because they don't modify the \[signal too drastically\]."
  Part 15 (SOS July 2000, p.193) then reveals it doubles as the slew generator / lag processor: "the
  slew generator is simply a low-pass filter, albeit one with a handful of specialised uses. (On most
  analogue synthesizers, it's a 6dB/oct low-pass filter with cutoff frequency variable in the range 0Hz
  to approximately 1kHz.)"
- **Engine:** `FILTER_*` ([`runtime/studio.h:435-445`](../../runtime/studio.h)) starts at 12 dB/oct
  (`FILTER_LOW`, a TPT SVF) and goes up: 18 (`FILTER_DIODE`), 24 (`FILTER_LADDER`). There is no
  6 dB option, so there is no way to ask for a *gentle tilt* rather than a filter.
- **Why:** one-pole per channel, near-free, and it is a genuinely different sound from a 12 dB filter at
  low resonance. Pairs with B1: the same primitive is the honest portamento circuit.
- **Audible home:** `22-filter` (add the slope), `eq`, `combo`.

### C6. Attack level and a break point on the mod envelopes

- **Book:** Part 8 (SOS December 1999) is one long argument that ADSR cannot make a real brass contour.
  He enumerates nine structural limitations, and identifies which two bite: "it's the third and fifth
  limitations that are most damaging ... the level at the end of the Attack stage is not the maximum,
  and the Sustain Level is not the level at the end of the Decay!" The fix he lands on is the Roland
  Alpha Juno's five-stage generator with "four time settings, and no fewer than three levels, making it
  dramatically superior to the three time settings and one level of the ADSR." And the reassurance that
  it costs nothing: "By choosing the parameters of a 5-stage contour carefully, you can make it look
  similar or even identical to a 4-stage ADSR."
- **Engine:** the mod envelopes are AD with a single amount (`env_a_samp`, `env_d_samp`, `env_amount`,
  [`runtime/sound.h:150-151`](../../runtime/sound.h)), so they hit his limitations 3 and 5 exactly.
- **Why:** an attack-level parameter alone (his `L1`) unlocks the swelled/spit-brass shape, which we
  have chased before (see [`brass-realism-handoff.md`](brass-realism-handoff.md) and audio-notes §19).
- **Audible home:** `brass`, `brasspec`, `filterenv`.

### C7. Carrier self-feedback on FM

- **Book:** Part 13 (SOS May 2000, p.84), Figure 15: "Feedback in an FM system turns a sine wave
  generator into a sawtooth generator ... it is also receiving a 100Hz sine wave (its own output) as a
  Modulator, thus making it produce a complete harmonic series at its output. You can then use an input
  level control or a VCA within the feedback loop to control the brightness of the output waveform.
  Neat, huh?"
- **Engine:** our `morph` macro feeds the **modulator** back into itself
  ([`runtime/sound.h:2952-2953`](../../runtime/sound.h)), which self-saturates the modulator toward a
  saw. Reid's canonical configuration feeds the *carrier* back, which yields a continuous sine-to-saw
  oscillator with one brightness knob.
- **Why:** essentially one line, and it gives a genuinely new oscillator (a variable-brightness saw with
  no filter involved) rather than a variation on an existing one.
- **Audible home:** `fm`, `fmbox`.

### C8. Hammond tonewheel leakage

- **Book:** Part 57 (SOS January 2004, p.118): "Another characteristic of the tonewheel generator
  (which, like key-click, Laurens Hammond considered to be a fault) is 'leakage', a mixture of drawbar
  pitches and noise that gives the A100 a characteristic, throaty quality."
- **Engine:** `INSTR_ORGAN`'s per-voice state ([`runtime/sound.h:207-216`](../../runtime/sound.h)) has
  nine drawbar phases, key click, percussion ping, scanner chorus and a pre-drive lowpass. No leakage
  floor.
- **Why:** a small always-present bleed of the other drawbar pitches plus noise, under the played
  registration. It is the last named Hammond character element we are missing, and Reid's own attempt to
  fake it on a Juno failed for reasons that do not apply to us ("on the Juno, the noise passes through
  the self-oscillating filter, and emerges tuned to the 5 2/3' pitch. Bah!").
- **Audible home:** `organ`.

### C9. PWM on the sawtooth

- **Book:** Part 10 (SOS February 2000, p.96): "Although Pulse Width Modulation is usually applied to
  pulse waves, there are a handful of synths that allow you to apply it to sawtooth waves. Of course,
  you can't describe this in terms of duty cycles ... Nevertheless, those synths that offer it (such as
  the Roland Alpha Junos) provide yet another range of subtly different timbres."
- **Engine:** `LFO_DUTY` is documented "square-wave slots only"
  ([`runtime/studio.h:419`](../../runtime/studio.h)).
- **Why:** small, and it is a real Alpha Juno string-machine colour.
- **Audible home:** `solina`, `supersaw`, `juno`.

### C10. Paraphonic mode

- **Book:** Part 20 (SOS December 2000, p.82-84) names and defines it: a shared post-mixer filter and
  contour means "the second note ... does not follow the ADS contour stages, because there is only one
  contour generator, and it has already reached the Sustain level." He lists the instruments built this
  way ("the Roland Paraphonic RS505 ... the earlier Roland RS202, the Korg Polyphonic Ensembles, the ARP
  Omnis, and even the revered Solina String Ensemble") and his Figure 9 draws the resulting note shapes.
- **Engine:** we are always truly polyphonic (per-voice filter, per-voice ADSR, `Voice` struct
  [`runtime/sound.h:128-373`](../../runtime/sound.h)), so the authentic string-machine articulation is
  not reachable.
- **Why:** low priority and genuinely questionable. It is a *worse* behaviour deliberately reproduced.
  But `solina` is named after one of his examples and currently cannot sound like it in the one respect
  Reid says defines it.
- **Audible home:** `solina`. If it cannot be made to earn its keep there, drop the item.

### C11. Pitch-to-CV needs a bandpass and a slew, per the book

- **Book:** Part 15 (SOS July 2000, p.193-194). Reid states the failure mode and both fixes:
  "pitch/CV converters can be fooled by stray signals and background noise, causing glitching. To
  overcome this, we add two sub-modules" — an input gain stage, and a slew generator whose "raison
  d'être is to remove the inevitable glitches that occur when the pitch detector loses lock on the
  desired signal. (Without the slew generator, the output CV would jump around wildly until lock was
  re-established.)" Then Figure 5 adds the third piece: "we add a band-pass filter to create a narrow
  'pass band' of accepted frequencies. This reduces the risk of extraneous signals or high-amplitude
  harmonics confusing the pitch detector."
- **Engine:** `tools/mic-spike/` is confirmed live on Mac, and CLAUDE.md records the exact symptom Reid
  is describing: "level clean, zero-crossing pitch is octave-noisy." Reid's three-part cure (bandpass
  pre-filter, gain normalise, slew the output CV) is a direct prescription for that spike, written in
  2000.
- **Why:** this is the most immediately *useful* find in the audit for work already in flight. It costs
  a bandpass and a one-pole and it is aimed at a known, reproduced defect.
- **Audible home:** `mictune`, `mictest`, `humtheremin`, `pipetune`.

### C12. A pulse-width harmonic oracle we can actually run

- **Book:** Part 10 (SOS February 2000, p.94) gives a hard, testable law: "A pulse wave has the same
  harmonic distribution as a sawtooth wave except that, for a duty cycle of 1:n (where n is an integer)
  every nth harmonic is missing from the spectrum." He works it: 50% kills the evens (odd harmonics
  only), 33.3% kills every third, 25% kills every fourth, and a non-integer duty such as 28.5%
  attenuates the third and fourth "but no harmonics are completely eliminated".
- **Engine:** `tools/harmonic-spec.js` already measures a harmonic series from a WAV.
- **Why:** this is not an engine change at all. It is a free correctness gate for the pulse oscillator
  and, more usefully, a **measurement of how badly the un-BLEP'd pulse (B5) smears the nulls.** Doing
  this first would tell us whether B5 is worth acting on.
- **Audible home:** none needed. This one is a tool run, which makes it the cheapest possible first step.

---

## D. Remarks

**D1. Reid says "enharmonic" where we say "inharmonic".** Grep accordingly, or you will miss the entire
percussion-analysis thread.

**D2. The book's framing is closer to our architecture than our docs are.** Part 63 (SOS July 2004,
p.110-112) explicitly rejects the oscillator-filter-amplifier model as "rather limiting" and proposes
**sources, modifiers, controllers**, where the classification is decided by position in the patch rather
than by what the module is: "the classification of the oscillator is not determined by its operation, but
by its position in the patch!" He shows a filter acting as a source (self-oscillating, Juno 60) and an
oscillator acting as a controller (Minimoog Osc3). Our aux buses, `fx_order`, and mod-destination system
already work this way. This would be a better organising frame for the audio docs than the current
per-engine layout, and it is worth considering next time `audio-notes.md` is restructured.

**D3. The hardware is dirtier than us in ways we have chosen not to model.** Part 21 notes the Prophet
600 scans at 200 Hz, so "your playing may be delayed by up to 5mS", and that stepping between quantised
knob values is "one source of the famous 'digital zipper noise'". We slew everything specifically to
avoid that (`SLEW_FAST/MED/MACRO`, [`runtime/sound.h:6348-6362`](../../runtime/sound.h)). Correct
call. Recorded so it stays a call.

**D4. Part 9 leaves the LIN-versus-LOG question open, and so do we.** It ends on the two CV inputs of a
real VCA module, "CV1-IN LIN and CV2-IN LOG", and defers: "This leads us to a whole new chapter regarding
the ways that signals exist and respond in the real world. Consequently, it too will have to wait till
another time." As far as I can find, he never returns to it. Our B2 (Hz versus octaves) and B4 (linear
versus exponential envelopes) are both instances of exactly that unfinished chapter, which is some
comfort: the canonical text does not settle it either.

**D5. What I did not check.** Parts 19 (duophony), 22 (springs/plates/buckets), 28-30 (plucked
strings), 31-39 (drums other than the timpani ring-mod passage and the 808 cymbal), 41-43 (pianos,
the JX10 patch listings), 44-50 (strings and bowed strings), 51-53 (flutes), 54-56 and 58 (the rest
of the Hammond), 59-62 (delays and effects) were skimmed or grep-targeted, not read end to end. They
are the *recipe* chapters, and they are the most likely place to find per-engine tuning findings for
`REED`/`PIPE`/`BOWED`/`PIANO`/`GUITAR` against the physical analysis. Anyone picking this up: those
chapters plus [`../guides/instrument-recipes.md`](../guides/instrument-recipes.md) is the obvious
continuation, and it is a different *kind* of audit (per-engine voicing, not engine architecture).
**Brass (24-27) is now done** — see §E, which is the template for the rest.

---

## E. Recipe pass 1 — BRASS (Parts 24-27)

STATUS: EXPLORING — same rule as §B/§C, nothing queued.

The first per-family pass, and the template for the others. Different in kind from §B/§C: those are
about the engine's *shape*, this is about whether one engine's *voicing* matches the physical
analysis. It is also the first section with **measurements**, because a voicing claim that isn't
measured is just an opinion.

Sources: Part 24 "Synthesizing Wind Instruments" (SOS April 2001), Part 25 "Synthesizing Brass
Instruments" (SOS May 2001), Part 26 "Brass Synthesis On A Minimoog" (SOS June 2001), Part 27
"Roland SH101/ARP Axxe Brass Synthesis" (SOS July 2001). Engine: `sound_brass_sample`
([`runtime/sound.h:3906`](../../runtime/sound.h)) + `sound_brass_start`
([`runtime/sound.h:3870`](../../runtime/sound.h)). Prior art, and this section is additive to it, not
a replacement: [`brass-realism-handoff.md`](brass-realism-handoff.md) (fixes #1+#2 shipped, #3 open)
and [`audio-notes.md`](audio-notes.md) §19.

**A note on Reid's method versus ours.** Every patch in Parts 25-27 is *subtractive*: a sawtooth
through a resonant lowpass, because that is what a Minimoog has. `INSTR_BRASS` is a *waveguide* — a
bore delay closed by a bell reflection, driven by a lip valve. So his knob settings are not
directly portable, and it would be a category error to "fix" our engine to match his signal chain.
What ports is the **acoustic target**: the spectra, the timings, and the behaviours he measured off
real instruments. That is what §E checks against.

### E0. Measurements taken

Reproducible; `brasspec` is the existing verification cart, unchanged and uncommitted-to.

```
node tools/play.js brasspec script /dev/null --headless --frames 180 --wav /tmp/brass_ff.wav
node tools/harmonic-spec.js /tmp/brass_ff.wav 220
node tools/wav-envelope.js  /tmp/brass_ff.wav
```

`brasspec` committed defaults = forte trumpet A3 (harmonics 0.15, timbre 0.80, morph 0.55). The two
variants below were rendered from an **off-tree copy** with one `#define` changed, then deleted; no
committed cart was edited.

| render | highest harmonic within 20 dB of f1 | energy >4 kHz | centroid |
|---|---|---|---|
| timbre 0.80, morph 0.55 (committed default) | **h9** (~2.0 kHz) | 1.7% | 6434 Hz |
| timbre **1.00**, morph 0.55 | **h23** (~5.1 kHz) | 2.2% | — |
| timbre 0.80, morph **0.00** | **h15** (~3.3 kHz) | 0.4% | 4220 Hz |

Harmonic levels relative to f1, at timbre 1.00 (the loudest, brassiest case we can produce):

| h2 | h3 | h4 | h5 | h6 | h7 | h8 | h9 | h10 |
|---|---|---|---|---|---|---|---|---|
| -31.3 | -8.4 | -21.9 | -7.8 | -11.1 | **-3.3** | -13.5 | -14.6 | -23.1 |

### E1. What matches, and one place we beat the book

- **The full harmonic series comes out of the physics, not a waveform choice.** Part 24 explains that
  a cylindrical closed pipe (clarinet) gives odd harmonics only, while a cone or a flare gives the
  complete series, which is why Reid must reach for a sawtooth. Our bore produces it structurally.
- **The asymmetric shaper's rationale is Reid's argument, arrived at independently.** The code comment
  at [`runtime/sound.h:3998-4003`](../../runtime/sound.h) says a plain `tanh` is an odd nonlinearity
  so "the spectrum stays clarinet-like (hollow, no even partials, doesn't read as 'brass')". That is
  Part 24's clarinet-versus-trumpet distinction, restated from the DSP side. Nice convergence.
- **The formant is deliberately fixed per instrument, not swept.** `fmtHz = 900 + (1-dark)*700`
  ([`runtime/sound.h:3922`](../../runtime/sound.h)), with the comment "a formant that SWEEPS with the
  macro reads as a synth filter sweep (a big part of the 'synthy' tell)". Part 23's whole thesis is
  that formants are fixed and pitch-independent. Correct and for the right reason.
- **Brightness rises with playing level.** `driveOut` and `brite` both scale with `lvl`
  ([`runtime/sound.h:3996`](../../runtime/sound.h), [`:4022`](../../runtime/sound.h)). Part 24: "as
  the note gets louder, it contains more harmonics."
- **Vibrato rate.** 5.4 Hz with a slow wander ([`runtime/sound.h:3926`](../../runtime/sound.h)).
  Part 25: "modulating frequencies in the region of 5Hz sound the most realistic."
- **We beat the book on breath noise.** Reid has to *omit* noise on both the Minimoog and the SH-101,
  twice, in identical words: their noise generator "lacks the formant shaping of the turbulent noise
  in a real brass instrument, and sounds very unnatural. As on the Minimoog, it is best omitted." Our
  noise is injected into the mouth pressure `Pm` ([`runtime/sound.h:3938`](../../runtime/sound.h)) and
  therefore circulates through the bore and gets bore-shaped for free. This is a real win over the
  hardware he is working with, and worth keeping in mind: it is the *reason* our brass can afford
  audible air where his can't.

### E2. Vibrato is never delayed and cannot be switched off

- **Book:** Part 25 is unambiguous. "Since vibrato does not occur during the transient stage of the
  note, you can't simply apply an LFO to the oscillator. Delayed vibrato is what is required, and
  it's usually implemented as an AR ramp controlling the amount of modulation." Also: "the amplitude
  of the modulation must be very low, otherwise the timbre will sound electronic."
- **Engine:** `br_vib_ph` starts at 0 in `sound_brass_start` and `vibd = 0.08f + v->mor * 0.35f`
  ([`runtime/sound.h:3913`](../../runtime/sound.h)) has a **floor of 0.08** with no path to zero. So
  every brass note vibratos through its own attack transient, and there is no way to defeat it.
- **And the cart stacks a second one.** `brass.c:171` adds `note_lfo(h[i], 0, LFO_PITCH, 5.5f, ...)`
  on top of the engine's own 5.4 Hz. Two vibratos ~0.1 Hz apart will slowly beat against each other.
- **Why it matters:** this is the same item as §C1, but note it lands *inside* the engine here, and
  the identical pattern is in `REED`, `PIPE`, `BOWED` (`rd_vib_ph`, `pp_vib_ph`, `bw_vib_ph`). One
  fix, four engines. It also blocks measurement, see §E9.
- **Audible home:** `brass` (its VIBRATO slider is already the control surface), then `reed`, `pipe`,
  `bowed`.

### E3. There is no onset growl, and Reid's mechanism is one we can't currently build

- **Book:** after the envelopes, this is the element Part 25 pushes hardest. Brass needs a settling
  period, "for a note of, say, 256Hz (middle C), this 'settling' takes about a dozen cycles ... a
  period of pitch instability lasting about 50mS". The synthesis answer is explicitly **not** pitch
  modulation: "Any form of periodic or even quasi-periodic modulation applied to the frequency of the
  oscillator will result in frequency modulation (FM), and therefore lead to the production of
  side-bands ... This would destroy the timbre of the brass patch." Instead, modulate the **filter**:
  "a triangle wave is an acceptable source for this modulation, and a frequency in the region of
  **80Hz** does the trick nicely", gated through "a VCA whose gain is controlled by an AD contour
  generator". Part 26 patches it as Osc3 at 32' into the filter; Part 27 sets the SH-101's LFO to
  maximum rate and rides the VCF Mod fader from ~60% down to 0% as the note settles. Tom Rhea's
  factory variant uses the **noise generator** into the filter instead, "and therefore risking FM
  side-bands ... This proves to be extremely effective."
- **Engine:** our onset is an 18 ms breath-noise burst into bore pressure (`br_attack`,
  [`runtime/sound.h:3890`](../../runtime/sound.h) and [`:3933-3936`](../../runtime/sound.h)). That is
  a *noise* transient. Reid's rasp is an audio-rate *tonal* roughness, which noise cannot substitute
  for, and it lasts ~50 ms not 18 ms.
- **The blocker, and it is the third time the book has said this:** Reid names the LFO ceiling as the
  reason the ARP Axxe *cannot* produce this sound at all. "One thing I can't do, however, is produce
  the filter rasp that was so successful on both the Minimoog and the SH101. This is because the LFO
  has a maximum frequency of just 20Hz, which is not fast enough." He makes the same complaint in
  Part 25 as a general reviewing criterion ("this, by the way, is one of the reasons why I point out
  that a maximum LFO frequency of, say, 25Hz is inadequate when reviewing synths"). **I have not
  verified our LFO ceiling** — `lfo_rate` is a float in Hz with no clamp I could find at the eval
  site ([`runtime/sound.h:6405`](../../runtime/sound.h)), so 80 Hz may already work. That is a
  five-minute check and it gates this whole item.
- **Audible home:** `brass`. The measurable claim is that the growl should read as a *rasp*, not a
  *breath*, and it should stop by ~50 ms.

### E4. The amplitude/brightness timing relationship is absent, and this may be the big one

- **Book:** the structural core of Reid's patch, stated three times. Part 25 Figure 11: "the harmonics
  beneath the instrument's natural cutoff frequency ... reach their sustain levels together, and more
  quickly than the harmonics above the cutoff point", and "some researchers believe that the
  differences in the development rates of the harmonics are the most important audible clue you have
  as to the identity of an instrument when you hear it." Part 26 gives the numbers: loudness contour
  **Attack 100 ms**, filter contour **Attack 600 ms**, "because the filter opens more slowly than the
  amplifier ... the higher harmonics are let through one by one over the course of about half a
  second." Roughly a **1:6 ratio**.
- **Measured:** at 100 ms resolution the note arrives finished. First window: amplitude 0.97 of peak,
  centroid 5921 Hz against a 6434 Hz mean. There is no attack development to see.
- **Engine + cart, and the two halves are separable:** the amp attack is cart-side and trivially
  wrong (`brasspec` and `brass.c` both use 1 ms). But the brightness half is engine-side and
  structural: `lvl` derives from `br_env`, a one-pole level **follower** with coefficient `0.0016`
  ([`runtime/sound.h:3977`](../../runtime/sound.h)) = a **14 ms** time constant, not the ~5 ms its
  comment claims. A follower tracks level, so even with a correct 100 ms amp attack the brightness
  would complete at ~115 ms, not 600 ms. Reid's shape needs a genuine per-note brightness *ramp*
  (his AR contour), not a level follower.
- **Why this ranks first in §E:** it is a *structural* miss rather than a tuning one, it is the thing
  Reid's own sources call the most important identity cue, and it is not among the items
  [`brass-realism-handoff.md`](brass-realism-handoff.md) has considered. It is a credible answer to
  that doc's standing complaint that the engine is "very obviously not real brass."
- **Audible home:** `brass`. Measure with `wav-envelope.js`, which should show the centroid still
  climbing at 300-500 ms.

### E5. The fundamental never stops dominating, and even harmonics are still weak

- **Book:** Part 24 Figure 8 compares the same note played quietly and loudly. Quiet: "the
  fundamental is the dominant harmonic ... contains just six harmonics, making it 'soft'". Loud: "the
  **eighth harmonic is dominant** in the over-blown note; you would hear this as a squawk three
  octaves higher than the fundamental", with "significant amplitudes of at least 15 harmonics". The
  mechanism is named too: "the relative amplitude of the lower harmonics **decrease** as the note
  gets louder", which is why he then introduces resonance.
- **Measured (timbre 1.00, the most extreme we can produce):** h1 is at 0 dB and stays the loudest
  partial. The strongest overtone is h7 at **-3.3 dB**. Evens remain suppressed: h2 -31.3, h4 -21.9,
  h10 -23.1, against odds h3 -8.4, h5 -7.8, h7 -3.3. So the spectrum is still both
  fundamental-dominated and noticeably odd-dominated, which is the clarinet-like signature the
  asymmetric shaper was added to cure. It moved the needle; it did not land the shape.
- **Nothing in the engine reduces h1 as blow rises.** Every brightness path is additive (`driveOut`,
  the `brite` high-shelf), so overtones are lifted but the fundamental is never traded away.
- **Relationship to prior work:** this is the measurable target for
  [`brass-realism-handoff.md`](brass-realism-handoff.md)'s still-open fix #3 ("model the bell to fill
  the harmonic series natively"). §E5 gives that item a **pass/fail number** it previously lacked:
  at forte, h8 should approach or exceed h1.
- **Audible home:** `brasspec` for the measurement, `brass` for the verdict.

### E6. The brassiness macro is badly tapered at the top

- **Measured:** timbre 0.80 gives h9; timbre 1.00 gives h23. The last fifth of the knob's travel does
  more spectral work than the first four fifths combined.
- **Why it matters:** `sound.h`'s own macro rule (§8.8.1, echoed in the comment at
  [`runtime/sound.h:2889`](../../runtime/sound.h)) is that a knob should be "exponential so every
  quarter-turn is an audible step". Brass violates it in the opposite direction: almost nothing
  happens, then everything does. It also explains why brass presets feel hard to voice, and why the
  handoff doc's headline number is so sensitive to where the macro sits (see §E9).
- **Audible home:** `brass` — sweep TIMBRE with SPACE (the auto-swell it already has) and the step
  sizes should be even.

### E7. No sub-oscillator for the low brass

- **Book:** Part 27 dissects Roland's own SH-101 Tuba patch: sawtooth at 60% plus "a square wave
  sub-oscillator present, one octave down and at 100 percent of its full loudness", and concludes
  "the combination of the waveforms defines the sound, almost as much as the filter and amplitude
  settings." He also has you A/B it: "listen to the patch with the sawtooth alone (it lacks body)."
- **Engine:** `sound_brass_sample` never reads `v->eng_p[]` (verified by inspection of the whole
  function). The "fundamental reinforcement" sub-oscillator that `GUITAR` and `PIANO` use for exactly
  this complaint ([`runtime/sound.h:337-343`](../../runtime/sound.h), "adds the low-end WEIGHT a bare
  KS string lacks (the 'thin' cure)") is simply not wired for brass.
- **Why:** the primitive exists, the plumbing exists (`instrument_mode` / `eng_p`), and tuba is one of
  the six hardware presets `brass.c` treats as acceptance tests.
- **Audible home:** `brass` preset 6 (tuba) and preset 4 (trombone).

### E8. Partials are not stretched

- **Book:** Part 25's closing caveats: "the partials are not, strictly speaking, harmonics at all.
  Their frequencies are stretched out (sharpened) as the harmonic number increases."
- **Engine:** the bore is a plain delay line plus a one-pole bell LP, so its modes land on integer
  multiples. `PIANO` already owns the primitive that fixes this (a dispersion allpass chain,
  `pn_disp_c` / `pn_disp_n`, [`runtime/sound.h:296-297`](../../runtime/sound.h)) — for precisely the
  analogous reason, inharmonic piano partials.
- **Why it is last:** subtle, and it overlaps fix #3 in the handoff doc. Listed for completeness.
- **Audible home:** `brasspec` (measurable as harmonic frequencies drifting sharp), `brass`.

### E9. Two problems with the way we have been measuring brass

Not engine findings, but they undermine the others if left unsaid.

- **The committed harness does not reproduce the doc's headline number.**
  [`brass-realism-handoff.md`](brass-realism-handoff.md) records "Measured (forte trumpet A3):
  highest harmonic within 20dB h9→h17 ... energy >4kHz 0.2%→2.3%". Running `brasspec` at its
  **committed defaults** today I get **h9 / 1.7%**; I only reach h23 / 2.2% by pushing timbre to
  1.00. So that measurement was taken at a different macro position than the cart now ships.
  Checked for a regression and found none: no commit to the brass region of `sound.h` since
  `8dfd12a`, and the only later brass-adjacent change is a uniform makeup-gain trim, which cannot
  move relative dB. **Conclusion: not a regression, but the number is not reproducible from the
  committed harness**, which is worse than it sounds for a doc whose whole job is to be the as-built
  record. Fix: pin the macros the number was taken at, in the cart or the doc.
- **The oracle is confounded by the engine's own vibrato.** `harmonic-spec.js` analyses a fixed
  16384-sample window ([`tools/harmonic-spec.js:56`](../../tools/harmonic-spec.js)) = **372 ms**,
  which spans ~2 cycles of the engine's 5.4 Hz vibrato. Frequency modulation smears each harmonic's
  energy across bins, and the absolute deviation grows with harmonic number, so **higher harmonics
  smear more** and read low. That predicts exactly what I measured: dropping morph from 0.55 to 0.00
  (which lowers vibrato depth *and* blow pressure) *raised* the harmonic extent from h9 to **h15**,
  the opposite of what less blow should do. Two candidate explanations remain (a real spectral change
  from reduced blow, versus vibrato smearing of the analysis) and **they cannot be separated today
  because §E2 means the vibrato cannot be turned off.** So: §E2 is not just a realism item, it is a
  prerequisite for trusting any brass spectral measurement, including the ones in this section and
  the ones in the handoff doc. Treat every harmonic number here as a lower bound.
- **Audible home:** none; this is a tooling item. But it should probably be step 1 of any brass work.

### E10. The `brass` cart preset contradicts the book on two numbers

Cart-side, no engine change, cheapest thing in §E.

`brass.c:143` is `instrument(I_BRASS, INSTR_BRASS, 1, 0, 4, 1200)`:

| | ours | Reid (Part 26, Minimoog) |
|---|---|---|
| amp attack | **1 ms** | 100 ms |
| sustain | 4 of 7 | maximum |
| release | **1200 ms** | effectively instantaneous |

On release he is explicit about why: "Because I know that a real brass sound ends very rapidly once
you stop blowing the instrument, I want the synthesized sound to do likewise, so I set the Decay
switch to Off." A 1.2 second release is a pad, not a horn. He also supplies a ready-made audit
checklist by tearing into Roland's own factory SH-101 Trumpet: "The Attack/Decay stages of the Env
are too short, the amount of Env control in the filter is too low, and the higher initial cutoff
frequency allows too many harmonics through when you first press a key. Furthermore, there's no
modulation, so there's no movement in any portion of the note. Yurgh!" Three of those four apply to
our preset.

Caveat before anyone "fixes" it: our release also governs how the *bore* rings down, so shortening it
is an audible change to the engine's tail, not only to the envelope. Worth an A/B rather than a
straight edit. And the six hardware presets in `brass.c` are declared acceptance tests, so they are
the thing to judge against.

**Audible home:** `brass`, and it also touches `afrobeat`, `mariachi`, `modaljazz`, `napoleon`,
`pasture`, `lurk` (the other carts using `INSTR_BRASS`).

### Suggested brass step order

| # | Step | Kind | Where |
|---|---|---|---|
| 1 | E9 pin the handoff's macros; note the vibrato/window caveat | docs + tooling | `brasspec` |
| 2 | E3 verify whether `instrument_lfo` already accepts 80 Hz | 5-minute check | none |
| 3 | E2 delayed + defeatable vibrato (also unblocks measurement) | engine, 4 engines share it | `brass` |
| 4 | E10 preset attack/release/sustain A/B | cart only | `brass` |
| 5 | E4 per-note brightness ramp (~600 ms), not a level follower | engine, structural | `brass` |
| 6 | E3 onset rasp, ~50 ms, audio-rate, AD-gated | engine or cart | `brass` |
| 7 | E7 wire `eng_p` weight/sub for the low bores | engine, small | `brass` presets 4 + 6 |
| 8 | E6 retaper the brassiness macro | engine, small | `brass` |
| 9 | E5 trade the fundamental away at forte (handoff fix #3) | engine, hard | `brasspec` |
| 10 | E8 stretched partials | engine, reuses PIANO's allpass | `brasspec` |

---

## Suggested step order (§B/§C, the architecture pass)

One at a time, each with its cart. Ordered by cost-to-payoff, not by section. The brass family has its
own order at the end of §E; the two lists are independent and either can go first.

| # | Step | Kind | Cart |
|---|---|---|---|
| 1 | C12 pulse-width harmonic oracle | tool run, no code | none |
| 2 | C1 LFO delay | one field | `21-lfo` → `reed`/`bowed`/`vox` |
| 3 | C3 resonance as a mod dest | one enum | `tb303`, `djfilter` |
| 4 | C2 LFO rate/depth as mod dests | two enums | `21-lfo`, `lfoshapes` |
| 5 | C4 ring-mod carrier waveform | ~5 lines | `tr808` cymbal, `gamelan` |
| 6 | C11 pitch-to-CV bandpass + slew | spike work | `mictune` |
| 7 | B1 glide in semitones | rework + regate | `heldnotes` |
| 8 | B7 per-voice character + round-robin | seeded, opt-in | `polystress`, `jangle` |
| 9 | C7 FM carrier feedback | one line | `fm` |
| 10 | B8 the comb-filter caveat | docs only | `vowel` |
| 11 | C5 6 dB one-pole filter | new mode | `22-filter` |
| 12 | B2a `instrument_keytrack` | new call | `22-filter` |
| 13 | C6 mod-env attack level | new param | `brass` |
| 14 | B10 key-scaled env times | note-on scale | `piano` |
| 15 | C8 Hammond leakage | engine tweak | `organ` |
| 16 | B6 non-monotonic formant amps | table | `vowel` |
| 17 | B5 BLEP the pulse | opt-in flag | `solina` |
| 18 | C9 saw PWM | small | `solina` |
| 19 | B9 velocity to brightness | opt-in or docs | `20-instruments` |
| 20 | B2b cutoff depth in octaves | surface change | `filterenv` |
| 21 | B3 note priority and triggers | probably `mono.h` | `sh101` |
| 22 | B4 exponential amp envelope | opt-in only, high blast radius | `06-sound` |
| 23 | C10 paraphonic mode | questionable, may drop | `solina` |
