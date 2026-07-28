# Synth Secrets audit — the engine cross-checked against Gordon Reid's 63-part series

STATUS: EXPLORING — findings ledger, nothing approved. Every item below is a *candidate*, deliberately
not queued. The owner's rule (2026-07-28): **one small step at a time, and no engine change lands
without a cart where you can hear it.** So each item names its audible home before it names its code.

Companion to [`audio-notes.md`](audio-notes.md) (the sound HUB). This doc is the outside-in view: what a
canonical synthesis text says the machine should do, versus what `runtime/sound.h` actually does.
Nothing here is a bug report. Several divergences are deliberate and documented in the code; they are
recorded anyway so the choice stays a choice instead of decaying into an accident.

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

**D5. What I did not check.** Parts 19 (duophony), 22 (springs/plates/buckets), 24-30 (wind, brass,
plucked strings), 31-39 (drums other than the timpani ring-mod passage and the 808 cymbal), 41-43
(pianos, the JX10 patch listings), 44-50 (strings and bowed strings), 51-53 (flutes), 54-56 and 58
(the rest of the Hammond), 59-62 (delays and effects) were skimmed or grep-targeted, not read end to
end. They are the *recipe* chapters, and they are the most likely place to find per-engine tuning
findings for `REED`/`PIPE`/`BOWED`/`BRASS`/`PIANO`/`GUITAR` against the physical analysis. Anyone
picking this up: those chapters plus [`../guides/instrument-recipes.md`](../guides/instrument-recipes.md) is the obvious
next pass, and it is a different *kind* of audit (per-engine voicing, not engine architecture).

---

## Suggested step order

One at a time, each with its cart. Ordered by cost-to-payoff, not by section.

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
