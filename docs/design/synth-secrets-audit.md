# Synth Secrets audit — the engine cross-checked against Gordon Reid's 63-part series

STATUS: EXPLORING — findings ledger, nothing approved. Every item below is a *candidate*, deliberately
not queued. The owner's rule (2026-07-28): **one small step at a time, and no engine change lands
without a cart where you can hear it.** So each item names its audible home before it names its code.

Companion to [`audio-notes.md`](audio-notes.md) (the sound HUB). This doc is the outside-in view: what a
canonical synthesis text says the machine should do, versus what `runtime/sound.h` actually does.
Nothing here is a bug report. Several divergences are deliberate and documented in the code; they are
recorded anyway so the choice stays a choice instead of decaying into an accident.

**Layout.** §A-§D are the **architecture** pass (is the engine the right *shape*?), read from the theory
chapters. §E, §F, §H and §I are the **recipe** passes, one instrument family at a time (does one
engine's *voicing* match the physical analysis?), and they carry measurements. **§E brass**, **§F
strings**, **§H plucked strings** and **§I pianos** are done; remaining families are listed in §D5.
**§G** is a design question the recipe passes raised: every patch in the book is subtractive and all our
imitative engines are physical models, which may mean we are missing a category rather than
mistranslating one — and §H then §I bounded it, since Reid says outright that subtractive cannot do a
guitar or a piano.

Two things a reader should know before trusting any single section. **§C12 was corrected** by §F: Reid
contradicts himself between Part 10 and Part 46 and explicitly retracts the earlier claim, which §C12
had quoted as law. And **§I found the best match in the audit** — `INSTR_PIANO` gets a piece of physics
right that is easy to get backwards — so the passes are not uniformly critical; where we are right,
that is recorded too, because the point is to keep choices from decaying into accidents in either
direction.

Note that §C12 was **corrected** by the strings pass. Reid contradicts himself between Part 10 and
Part 46 and explicitly retracts the earlier claim, which the original §C12 had quoted as law. Expect
more of this: the series ran five years and he revises himself.

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

> **⚠ CORRECTED 2026-07-28 by the strings pass (§F).** This item originally quoted Part 10's "every
> nth harmonic is missing" as "a hard, testable law". **Reid himself corrects that claim in Part 46**,
> and calls it "a long-standing mistake usually made in discussions of pulse waveforms". The oracle
> below is still worth building, but the assertion it makes had to change; the original version would
> have gated on something false. Details in §F3.

- **Book, the corrected version:** Part 46 (SOS March 2003, p.154-155). The *nulls* are evenly spaced
  as Part 10 says, but the surviving harmonics do **not** keep the sawtooth's 1/n amplitudes. A pulse
  wave's spectrum is a **sinc** envelope: "the amplitude of any pulse-wave harmonic is defined by the
  value of the sinc function at that point ... there can be no harmonics at the points where the sinc
  function crosses the 'X' axis, which explains why a pulse wave's missing harmonics are evenly
  spaced." At 25% duty, "every fourth harmonic is missing, but the amplitudes of the others no longer
  exhibit the 1/n relationship. This becomes increasingly apparent as the duty cycle becomes
  narrower." He also gives the consequence that kills the naive version outright: build "a sawtooth
  spectrum with holes in it" and you do not get a pulse wave, you get a **staircase** with as many
  steps as the null spacing, and at 1% duty that staircase "is all but indistinguishable from a
  sawtooth wave."
- **Engine:** `tools/harmonic-spec.js` already measures a harmonic series from a WAV.
- **What the oracle should assert:** null *positions* (every nth harmonic, from the duty cycle) and
  that surviving amplitudes track a **sinc** envelope, not 1/n. Asserting 1/n would fail a correct
  oscillator.
- **Why:** still no engine change, and still the cheapest thing here. Its real value is measuring how
  badly the un-BLEP'd pulse (§B5) smears the nulls, which decides whether §B5 is worth acting on.
- **Audible home:** none needed. A tool run.

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

**D5. What I did not check.** Parts 19 (duophony), 22 (springs/plates/buckets), 31-39 (drums other than
the timpani ring-mod passage and the 808 cymbal), **51-53 (flutes)**, 54-56 and 58 (the rest of the
Hammond), 59-62 (delays and effects) were skimmed or grep-targeted, not read end to end. They are the
remaining *recipe* chapters, and the most likely place to find per-engine tuning findings for
`MEMBRANE`/`PIPE`/`REED`/`ORGAN` against the physical analysis. Pair them with
[`../guides/instrument-recipes.md`](../guides/instrument-recipes.md).

Done so far: **brass (24-27) → §E**, **strings (45-50) → §F**, **plucked strings (28-30) → §H**,
**pianos (41-44) → §I**. §E is the template. Suggested next, by expected yield:

1. **Drums (31-40)** — ten chapters against `MEMBRANE`, `tr808`/`tr909`/`morphdrum`, `drumkit.h`. The
   seam is already proven: §C4 pulled the timpani ring-mod recipe out of Part 32 and §A5 checked the 808
   cymbal against Part 40, both by grep, and both landed. The other eight chapters (timpani ×3, bass
   drum ×2, snare ×2, metallic percussion ×3, bells, cowbells/claves) are unread. Comfortably the
   largest single haul left, and the one family where we have *machine* recipes (`tr808.h`, `tr909.h`,
   `morphdrum.h`) as well as a physical engine, so there are two independent things to check.
2. **Flutes (51-53)** against `PIPE`, which carries a known intonation caveat already recorded in
   `studio.h` — the only remaining family with a *specific* open question to aim at.
3. **The Hammond (54-58)** against `INSTR_ORGAN` — §C8 pulled leakage out of Part 57 by grep; five
   chapters on one instrument almost certainly hold more.
4. **Delays and effects (59-62)** against our echo/BBD/chorus/spring-reverb stack — the only remaining
   arc that is about the effects layer rather than an engine, so it would exercise a different part of
   the codebase than the four passes so far.
5. **Reeds** have no dedicated arc; `REED` is covered incidentally by Part 24 (already read for §E) and
   the clarinet material in Parts 28 and 47. Probably not worth a pass of its own.

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

## F. Recipe pass 2 — STRINGS (Parts 45-50)

STATUS: EXPLORING — nothing queued.

"Strings" is two threads in the series and both map onto things we ship, so both are here. Parts 45-46
are string **machines** (the Solina/Freeman ensemble lineage, and PWM), which land on `solina`, `juno`,
`supersaw` and our unison/detune/PWM surface. Parts 47-50 are **bowed** strings, which land on
`INSTR_BOWED`. **Plucked** strings are a separate arc (Parts 28-30, still unread, see §D5) covering
`PLUCK`/`GUITAR`/`PIANO`.

Sources: Part 45 "Synthesizing Strings • String Machines" (SOS February 2003), Part 46 "…PWM & String
Sounds" (SOS March 2003), Part 47 "Synthesizing Bowed Strings • The Violin Family" (SOS April 2003),
Parts 48-49 "Practical Bowed-string Synthesis" (SOS May, June 2003), Part 50 "Articulation &
Bowed-string Synthesis" (SOS July 2003). Engine: `sound_bowed_sample`
([`runtime/sound.h:3778`](../../runtime/sound.h)) + `sound_bowed_start`
([`runtime/sound.h:3708`](../../runtime/sound.h)).

### F1. What matches, and it is a lot on the machines side

The ensemble half is the best-matched area the audit has found. Part 45 walks a Jupiter 6 up a ladder
of thickening tricks, and we have every rung:

- **Detune two saws.** `instrument_unison` up to 7 (`SOUND_UNISON_MAX`,
  [`runtime/sound.h:122`](../../runtime/sound.h)) where the Jupiter 6 had two oscillators.
- **Then modulate the detune amount with an LFO**, which is his key move: "the LFO is altering the
  amount of detune between VCO1 and VCO2 ... It is this that our ears hear as the further thickening of
  the sound." That is exactly `LFO_DETUNE` ([`runtime/studio.h:426`](../../runtime/studio.h)), "the
  breathing/chorusing width wobble". We also have the one-shot version he doesn't, `ENV_DETUNE`, "THE
  bloom: one thin saw opening into a wall of N on the attack".
- **Then swap that LFO to Random to kill the regularity.** He is explicit that a triangle LFO on detune
  reads as "an unnaturally regular modulation" and that the Jupiter 6's Random setting (a sample & hold
  clocked at the LFO rate) fixes it: "the essential nature of the sound remains the same, but ... there
  is now no periodic modulation." We ship `LFO_SHAPE_SH` **and** `LFO_SHAPE_RANDOM`
  ([`runtime/studio.h:605-606`](../../runtime/studio.h)) — the stepped one he had, plus a smoothed
  random walk he didn't. With his caveat worth keeping: "If you increase the LFO Depth too far, you
  will hear the pitch of VCO1 jump around in a most unnatural fashion."
- **Aperiodic modulation from summed LFOs.** His "Multiple Sine Waves & Modulation" box proves that
  same-frequency sines always sum to a sine regardless of phase or amplitude, so complexity needs
  *different* frequencies, and three or more gives something that "loses all semblance of periodicity
  ... useful when we program sounds with a quasi-random or 'human' element". Our three per-instrument
  LFOs sum on a shared destination (§A4), so his aperiodic modulator is buildable today.
- **Chorus is the instrument's identity, not a send.** He notes "most string synths relied heavily on
  built-in chorus effects to thicken a weedy initial timbre", and `solina`'s own `de:meta` lineage
  already says it is "demonstrating that `chorus()` is the instrument's entire identity rather than a
  send effect". Independent agreement.

### F2. `solina` leaves the two tricks that matter unused

- **Book:** the ladder in §F1 is the whole point of Part 45, and the Random-LFO-on-detune rung is the
  one he says separates a "wobbly boring buzz" from something "thick and unstable ... 'analogue', or
  perhaps 'human'".
- **Cart:** `solina.c` uses one `instrument_lfo(s, 0, LFO_PITCH, 0.16f, 0.04f)` for slow tape wow
  (`solina.c:103`) and models the divide-down stack with per-tab detune. It uses **neither**
  `LFO_DETUNE` nor any of the `LFO_SHAPE_SH`/`_RANDOM` shapes. Same for the fixed detune it sets at
  note-on: it never breathes.
- **Why:** free. Zero engine change, two calls, and it targets the exact quality Reid says the
  instrument lives or dies on. This is the cheapest item in §F and possibly in the whole doc.
- **Audible home:** `solina`, then `supersaw` and `juno`.

### F3. Our PWM is probably missing the pitch modulation that makes PWM lush

This is the most interesting finding of the pass, and it is subtle.

- **Book:** Part 45 first states it as a curiosity: "Pulse waves whose widths are modulated by triangle
  waves have another, rarely appreciated characteristic; they exhibit pitch modulation that oscillates
  at the PWM rate above and below the true oscillator pitch ... a PWM wave generated by a single
  oscillator exhibits **two pitches**." Part 46 then spends two full boxes proving it, by
  differentiating the PWM waveform into its rising and falling edge trains and showing they are two
  independent signals at *different constant frequencies* (nine-eighths of each other in his worked
  example): "Now that we have shown PWM to be the sum of two signals, at least one of which is
  frequency-modulated with respect to the other". That, not the changing harmonic content, is his
  answer to why PWM sounds chorused: "the changing harmonic content is one of the visible consequences
  ... but there's more to it than that."
- **Engine:** `LFO_DUTY` adds the LFO to `duty` ([`runtime/sound.h:6410`](../../runtime/sound.h)) and
  the oscillator phase advances at a constant rate, so our duty modulation moves the pulse *edge*
  without altering either edge train's rate. **Unverified**, and it needs verifying rather than
  assuming: whether the two-pitch effect falls out of a phase-accumulator pulse for free, or whether it
  is an artifact of how analogue PWM circuits derive the edges, is exactly the sort of thing that
  deserves a measurement, not an argument. If it does not fall out for free, our PWM is thinner than a
  Juno's for a reason nobody has named.
- **The test:** render a single PWM voice at a low pitch with deep, slow modulation, and look for
  sidebands at the LFO rate around the fundamental. Reid says the effect is "easy to hear at low
  oscillator pitches and high modulation depths". `harmonic-spec.js` plus `wav-modrate.js` should
  settle it in one pass.
- **The consolation prize if it is missing:** Part 46's entire practical half is a recipe for
  *synthesizing PWM without PWM* — "you can mix two simple sawtooth oscillators, and, if one is
  frequency-modulated slightly with respect to the other, you will obtain a sound that is all but
  identical to that of a single, pulse-width modulated, pulse-wave oscillator. Sure, the waveform looks
  different, but the sound is the same." We can already do that with unison plus `LFO_DETUNE`. So the
  fix may be a cart recipe rather than an engine change.
- **Also from this chapter:** Reid's correction to Part 10's pulse-harmonic law, which invalidated the
  original §C12 (now fixed there). And a useful licence: sawtooth and ramp "have the same spectrum;
  there is merely a change of polarity", and the difference is "inaudible" in isolation.
- **Audible home:** `juno` (PWM is its identity), `solina`.

### F4. `INSTR_BOWED` has no body resonator, and the book says that is the difference

This is §F's headline, and it is a clean, sourced gap with the fix already in the same file.

- **Book:** Part 47 separates the two spectra explicitly. Figure 8 is "the force waveform measured at
  the bridge of a violin", a sawtooth. Figure 14 is the *radiated* spectrum after the body. On why you
  cannot ship the first and call it done: "The timbre of a violin is strongly linked to the dominant
  body resonances in the region of a few hundred Hertz, as well as the broad combination of resonances
  in the region of 2kHz to 4-5kHz or thereabouts. **Without these (or their equivalents for the viola
  or cello) the sound will not be realistic.**" He also measures the isolated body: flat across a few
  hundred Hz, a steep bass roll-off, and "a gentler roll-off of about 9dB per octave in the upper-mid
  and high frequencies" (Figure 13).
- **Engine:** `sound_bowed_sample` returns `toBridge * 0.8f` DC-blocked and gain-trimmed
  ([`runtime/sound.h:3857-3861`](../../runtime/sound.h)). The comment calls it "bridge-side signal
  (what the body radiates)" — but nothing models the body. We ship Reid's Figure 8 where the ear needs
  his Figure 14. Verified by inspecting the whole function for any biquad/formant/body term: there is
  none.
- **And the fix is already in the header.** `GUITAR` has `gt_body[4]` (four parallel body-resonator
  bandpasses, voiced from its harmonics macro) and `PIANO` has `pn_body[4]`, both built on the shared
  `SoundBiquad` ([`runtime/sound.h:126`](../../runtime/sound.h)). `BOWED` is the only stringed engine
  without one, which looks like an oversight rather than a decision — especially since it is the one
  whose real-world body is most famous.
- **Knock-on:** `bowed`'s own cart text says PIZZICATO "plucks the same string + **body**", and
  `studio.h:330` says pizz "plucks the same string + body instead of bowing it". Both overstate what
  the engine does. Reid even hands us the voicing target for pizz: "there are good reasons why
  pizzicato played on a violin or viola shares many of the sonic attributes of a banjo", and `GUITAR`'s
  harmonics macro already has a bright-banjo body at its top end.
- **Audible home:** `bowed` (its whole surface), plus `mariachi`, `tango`, `satie`, `carlos`.

### F5. The bow-pressure macro is pinned inside the clean wedge, so "surface sound" is unreachable

- **Book:** Part 47 names the sound and its cause. Insufficient bow pressure lets the string "slip twice
  in each cycle. This 'double-slip' motion does not change the pitch, but more often creates a new tone
  that violinists call 'surface sound'." Reid then makes the connection for us: "If they had ever
  studied hard sync on an analogue synth, they would understand what they were hearing!" Multiple slips
  beyond that are "best avoided by skilled players", so there is a real line between expressive and
  broken — but double-slip is on the musical side of it.
- **Engine:** `pressure = 0.10f + v->timb * 0.16f` → the range **[0.10, 0.26]**
  ([`runtime/sound.h:3782`](../../runtime/sound.h)), and the comment records why the top was pulled in:
  "recompressed 2026-06-16: the old top (0.32) bowed scratchy — >4kHz noise jumped 0.5%→3.6%". So the
  scratchy region was deliberately removed as a defect. Reid's analysis says part of that region is
  *sul tasto / surface sound*, an expressive colour real players use.
- **Why this is a judgement call, not a bug:** the recompression fixed a real loudness/noise problem and
  the STEP-0 wedge work exists for good reasons. But it is worth knowing that we traded away a named
  playing technique, and that a *separate* axis (rather than reopening `timbre`) could return it
  without destabilising the default voicing.
- **Audible home:** `bowed`.

### F6. Three bowed behaviours we don't model

Grouped because each is small and individually optional.

- **Louder goes slightly flat.** Part 47: "the pitch of the note goes slightly flat as it becomes
  louder." Our `morph` is bow speed and does not touch pitch. Measurable with `tune-check.js` swept
  across morph, which would currently show no deviation.
- **Per-period pitch jitter.** Part 47: "there is jitter in the pitch as the 'corner' of the wave ...
  passes under the bow." We have `bw_drift`, a slow random walk, which is a different thing. The
  primitive for the right thing exists: `INSTR_VOICE`'s `vox_jit_mul` is per-glottal-period pitch
  jitter ([`runtime/sound.h:270`](../../runtime/sound.h)).
- **Bow position should comb out harmonics, and this is testable today.** Part 47: bowing at the centre
  removes the even harmonics; at 1/3 from the bridge "there can be no third, sixth, ninth, and other
  'third' harmonics"; at 1/4, no fourth/eighth/twelfth. Our `harmonics` macro *is* bow position
  (`bw_nutlen`/`bw_brlen` split at note-on), so this comb should already fall out of the geometry. It
  is a free correctness check on the engine's physics, and a good one because a pass proves the
  waveguide is right where a listening test can't.
- **Audible home:** `bowed`; the comb check is a `brasspec`-style measurement.

### F7. Part 50 is not about DSP at all, and it is the finding with the widest reach

- **Book:** Part 50 abandons patch-building and argues that **control** beats components. Reid drives a
  two-module patch (one oscillator, one VCA) from an Ondes Martenot clone: a ring on a wire for
  continuous unquantised pitch, plus a pressure button for continuous loudness. Result: "With a little
  practice, the performance is no longer that of a soulless single-oscillator, unmodulated sawtooth
  buzz. You can add vibrato by wiggling your 'ring' finger from side to side, controlling both the speed
  and depth in a way that feels and sounds completely natural. Glide is merely a matter of pressing the
  button as you move to the next note." His closing claim is deliberately provocative: "two modules and
  a more appropriate method of controlling them can be far more expressive and create more realistic
  bowed string and brass sounds than any number of modules and facilities" driven from a keyboard.
- **Why this lands hard here:** a touchscreen ribbon *is* the wire, and we already have the ingredients
  — `note_on` plus `note_pitch`/`note_glide` for continuous pitch, `note_vol` for continuous loudness,
  multitouch, `pointer.h`, `gestures.h`, and a `solo.h` ribbon. We even ship the instrument by name
  (`martenot`), plus `otamatone`, `stylophone`, `monochord`, `ribbonpad`, `musicalsaw`, `acidtheremin`,
  `humtheremin`. So this is not a gap in capability. It is a **gap in emphasis**: the engine's whole
  documented centre of gravity is keyed notes with envelopes, and Reid's argument is that for bowed and
  brass the keyed-plus-envelope path is the *worse* one.
- **Two concrete things to lift, both cheap:**
  - **Loudness-to-brightness with no filter.** He morphs the waveform saw→triangle as level falls,
    "so you can reduce the amplitude or even eliminate harmonics by moving the wave from a sawtooth
    towards a triangle as you reduce the overall loudness of the sound. This relationship between
    loudness and high-frequency content is ... very much the behaviour of blown, bowed, strummed and
    struck instruments, and we're recreating it without a filter anywhere to be seen." Relevant to §B9
    (velocity does not touch brightness) and it suggests the cheap answer there is a *waveform* morph,
    not a filter.
  - **The filter as the gate.** His brass variant replaces the VCA with a lowpass and drives *cutoff*
    from the button, so at low cutoff nothing passes and "the filter is not only shaping the tone of
    the sound, it's also differentiating one note from the next. This is incredibly elegant!" Doable
    today with a held voice plus `note_cutoff`, and nothing demonstrates it.
- **Audible home:** `martenot` is the obvious one and it already exists — this would be about deepening
  it rather than building it. `bowed`, `monochord`, `ribbonpad` for the ribbon; `brass` for the
  filter-as-gate variant.

### F8. Smaller items from the ensemble chapters

- **Amp level should key-track *negatively* for warmth.** Part 46's Korg T2 string patch sets amplifier
  keyboard tracking to **-04**: "With a negative value, this weights the loudness of the sound to the
  bottom end of the keyboard, thus generating additional warmth." We have no amp key-tracking in either
  direction (§B2 covers cutoff; this is a second, independent destination).
- **String patches want zero resonance and real key-follow.** His JX10 string patch: `LPF Resonance 00`,
  `Key Follow 64`, `ENV Amount 14`. That is the **fourth** independent citation for §B2 keytracking,
  after Parts 6, 23, 24 and 26. It is comfortably the most-cited missing feature in the series.
- **No velocity sensitivity.** "String synths were not velocity-sensitive, so this patch should be
  likewise" — worth knowing before anyone wires §B9 globally rather than per-slot.
- **The VCA envelope is a trapezoid**: a crescendo in, a long tail, no filter modulation (Part 45
  Figure 9). Reid's Part 7 trapezoid, reappearing as the string-machine amp shape.
- **Layering is the ensemble.** Part 46: three layers at 16'/8'/4' "to emulate the Cello, Viola and
  Violin options offered by some of the better vintage ensembles", at the cost of polyphony. `solina`
  already stacks footages, so this is mostly confirmation.
- **Audible home:** `solina`, `juno`.

### Suggested strings step order

| # | Step | Kind | Where |
|---|---|---|---|
| 1 | F2 use `LFO_DETUNE` + a Random-shape LFO on it | cart only, free | `solina` |
| 2 | F3 measure whether our PWM has the two-pitch effect | measurement | `juno` |
| 3 | F6 measure the bow-position harmonic comb | measurement, proves the physics | `bowed` |
| 4 | F4 give `BOWED` a body resonator (reuse `gt_body`/`pn_body`) | engine, primitive exists | `bowed` |
| 5 | F4b fix the pizz "string + body" claim in cart + `studio.h` docs | docs | `bowed` |
| 6 | F7 loudness→brightness by waveform morph, and filter-as-gate | cart recipes | `martenot`, `brass` |
| 7 | F8 amp key-tracking (negative for warmth) | engine, small | `solina` |
| 8 | F5 a separate axis for surface sound / double-slip | engine, judgement call | `bowed` |
| 9 | F6b louder-goes-flat + per-period jitter | engine, small | `bowed` |

---

## G. The missing engine class — subtractive imitation

STATUS: EXPLORING — raised by the owner 2026-07-28, mid-audit. Nothing designed yet.

Both recipe passes so far have opened with the same caveat: **every patch in Synth Secrets is
subtractive, and all our imitative engines are physical models.** §E says it about brass, §F about
bowed strings. That caveat has been treated as a translation problem. It is worth asking whether it is
actually pointing at a missing *category*.

**The observation.** Reid's brass patch does not sound like a trumpet. It sounds like **a Minimoog
playing a trumpet**, and that is a beloved sound in its own right — the entire 1970s prog and funk horn
vocabulary, plus the string-machine sound that Parts 45-46 spend two months on and that people still
buy hardware to get. Meanwhile `INSTR_BRASS` chases the trumpet itself and gets judged against a
trumpet, which is a much harder bar and, per
[`brass-realism-handoff.md`](brass-realism-handoff.md), one we keep not clearing. Two different targets
have been collapsed into one engine id.

**Why this fits the project.** The north star is "deep, honest simulations behind a humble lo-fi
surface". An honest analogue-imitation engine is arguably *more* on-grain than photorealistic physical
modelling: it is a simulation of a **synthesizer**, which is a thing with a real, knowable architecture
and a 50-year recorded history, rather than a simulation of an orchestra. It is also far cheaper per
voice than a waveguide, and it degrades gracefully.

**What it probably is not: a new `INSTR_*`.** The pieces are mostly already primitives. A Minimoog
brass patch is a saw, a resonant lowpass, two envelopes and an audio-rate filter modulator, all of
which we ship or nearly ship. Which suggests the shape is one of:

1. **A cart-land header** (`subtractive.h`, sibling to `acid303.h` and `tr808.h`): a small struct plus
   a voicing table holding Reid's published patches as *data* (his Minimoog trumpet, tuba and jazz
   trombone from Part 26; the SH-101 and Axxe versions from Part 27; the Jupiter 6 and JX10 string
   patches from Parts 45-46). The header owns the recipe, the cart owns the performance. This is
   exactly the precedent `acid303.h` set — "cart owns the PATTERN, header owns the SOUND" — and it needs
   no engine surface at all.
2. **A thin engine id** that packages the pieces into three macros, if and only if the header proves
   the pieces need to be closer to the voice than cart-land can reach.
3. **Just the missing primitives plus a recipes doc**, if §B/§C's items (keytracking, audio-rate filter
   modulation, an attack-level envelope, a separate filter envelope) turn out to be all that was in the
   way.

**Option 1 looks right,** and the audit is unusually well-placed to feed it: Reid published *exact
parameter values* for every one of those patches, and §E10/§F8 already show our own presets drifting
from them. A voicing table with a citation per row would be both a feature and a regression test.

**Scope boundary, established by §H and widened by §I (2026-07-28).** Reid draws the line himself,
three times. Guitar: "you can not create authentic-sounding acoustic guitar patches using analogue
subtractive synthesis. **This is one occasion when only digital technology will do!**" (Part 28).
Electric guitar, after rejecting three factory patches: "the world does not permit practical analogue,
subtractive synthesis to reproduce a guitar sound" (Part 30). Piano: "there has never been a convincing
acoustic piano produced by subtractive synthesis, additive synthesis, or FM synthesis. Only samples
appear to do the trick", and "is it impossible to create an acoustic piano patch on an analogue synth?
The strict answer is 'yes'" (Part 41). In all three he points at physical modelling or samples, which is
what we already do.

So this engine class is right for **brass, string machines and leads** — the families where the analogue
imitation *is* the desirable sound — and explicitly wrong for **the struck and plucked strings**. §G
should not grow a guitar or a piano.

**And §I found Reid stating the §G thesis himself,** which is the strongest endorsement the idea has.
On the JX10's layered piano patch: "'H1: Acoustic Piano' has many of the characteristics of an acoustic
or electro-mechanical piano, without sounding anything like the former, or even quite like the latter.
It's responsive, it's expressive and, for many purposes, it's every bit as usable as a Fender Rhodes 73
or a Wurlitzer EP200. In fact, **there are times when I would still use it today, in preference to any
of the 'real' things.**" That is exactly the case for this engine class: the imitation is a worthwhile
instrument *because* of how it fails, not despite it. Note it also means the voicing table should carry
**layered** patches, not just single ones (§I9, §F8).

**Honest tension.** It overlaps `INSTR_SAW` + `instrument_filter` + envelopes, which a cart can already
wire by hand. The argument for doing it anyway is the same one that justified `acid303.h`: tb303 and
acidrack had each drifted their own copy of the same voice, and extracting it made them byte-identical.
If several carts are going to reach for "1970s brass section" or "Solina pad", they should reach for one
implementation.

**Prerequisites, all already in this doc.** §B2 keytracking (cited four separate times, and Part 26
pins the value: the filter should track "at slightly less than a 1:1 ratio ... say, to 190 percent" per
octave, ≈0.93). §E3's audio-rate filter modulation for the growl. §C6's attack-level envelope for the
spit-brass contour. A separate filter envelope, which the SH-101 famously lacked and Reid had to work
around. Build those and the header becomes mostly a table.

**Audible home:** whatever it produces, but the natural first two are a Minimoog-brass cart (Reid gives
three patches, so the acceptance test is that they sound like three different instruments) and pointing
`solina`/`juno` at the same table. Also catalogued as a candidate engine in
[`instrument-engines.md`](instrument-engines.md) §8.9.

---

## H. Recipe pass 3 — PLUCKED STRINGS (Parts 28-30)

STATUS: EXPLORING — nothing queued.

Three chapters against three engines: `INSTR_PLUCK` (bare Karplus-Strong), `INSTR_GUITAR` (KS +
body), `INSTR_PIANO` (StifKarp + dispersion + body). Part 28 is the physics, Part 29 builds the
"theoretical" patch, Part 30 tries the electric guitar and gives up. It is the most conclusive arc in
the series, and the conclusion is in our favour.

Sources: Part 28 "Synthesizing Plucked Strings" (SOS August 2001), Part 29 "The Theoretical Acoustic
Guitar Patch" (SOS September 2001), Part 30 "A Final Attempt To Synthesize Guitars" (SOS October
2001). Engine: `sound_pluck_start` ([`runtime/sound.h:2807`](../../runtime/sound.h)),
`sound_guitar_start` ([`:4431`](../../runtime/sound.h)), `sound_guitar_sample`
([`:4500`](../../runtime/sound.h)). Prior art: [`piano-engine.md`](piano-engine.md).

### H0. Measurements taken

An off-tree probe cart (the `brasspec` pattern, one `#define` per variant), deleted afterwards. No
committed cart was edited.

**Pick-position comb, `INSTR_PLUCK` at A2 (110 Hz), levels in dB relative to h1:**

| position | h2 | h3 | h4 | h5 | h6 | h7 | h8 | h9 | h10 | h11 | h12 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| centre (morph 1.00) | **-14.3** | +2.4 | **-14.9** | -7.4 | **-24.1** | -11.8 | **-14.0** | -0.3 | **-30.3** | -8.0 | **-27.0** |
| 1/3 (morph 0.638) | -3.2 | **-7.0** | -4.2 | -7.2 | **-21.9** | -11.5 | -2.7 | **-9.8** | -24.0 | -7.7 | **-23.9** |

Bold = the harmonics the physics predicts should be notched. Both rows put local minima where Reid
says they belong.

**`INSTR_GUITAR` open ring (harmonics 0.45, morph 0 → claimed t60 ≈ 5.9 s), 100 ms windows:**

| t | 0.00 | 0.10 | 0.20 | 0.30 | 0.40 | 0.50 | 0.70 | 1.00 | 1.20 |
|---|---|---|---|---|---|---|---|---|---|
| amplitude | 1.00 | 0.70 | 0.56 | 0.44 | 0.35 | 0.28 | 0.19 | 0.12 | 0.09 |
| brightness | 0.616 | 0.145 | 0.091 | 0.070 | 0.059 | 0.052 | 0.042 | 0.031 | 0.027 |
| centroid Hz | 6551 | 3284 | 2497 | 2129 | 1911 | 1760 | 1566 | 1401 | 1336 |

### H1. What matches

- **The pick-position comb is the right mechanism, and it is a macro.** Part 28 derives it: a string
  plucked at its centre cannot have even harmonics, because "you can't have a node at the point at
  which the string is plucked", so at 1/3 "every third harmonic will be missing", at 1/4 every fourth,
  and sweeping the picking hand gives "the distinctive flanging sound ... you're creating the same
  effect as a swept comb filter." `sound_pluck_start` implements exactly that on the excitation:
  `ks_buf[i] = tmp[i] - 0.55f * tmp[(i + pos) % period]` with `pos` scaled from the morph macro
  ([`runtime/sound.h:2825-2832`](../../runtime/sound.h)). Measured in H0 and it lands correctly.
- **The comb's 0.55 coefficient turns out to be defensible, for a reason the code doesn't state.** It
  attenuates the notched harmonics by 14-30 dB rather than nulling them. Reid supplies the
  justification: the body immediately puts them back — string/plate coupling "excit[es] new modes in
  the string itself, including modes that were not present in the original vibration ... within a cycle
  or two, the triangular waveform of the string changes to a new shape." A partial notch is closer to a
  real guitar than a perfect null. Worth writing into the comment so it stops looking like a magic
  number.
- **Brightness falls faster than amplitude.** Part 29: "the waveform of a real plucked string tends
  towards a sine wave as time passes, with nothing but the fundamental present as the oscillation
  decays to inaudibility", modelled as a lowpass driven by the same contour as the VCA. Measured:
  amplitude ×0.28 over 500 ms while brightness goes ×0.084 and the centroid drops 6551 → 1760 Hz. The
  KS loop filter gives us this for free. Part 30 confirms the direction from the other side, praising
  the Minimoog patch because "the decay of the filter cutoff frequency is somewhat faster than the
  decay of the amplifier ... produces a more natural-sounding tail."
- **The body's lowest mode is the air cavity.** `f_lo[0] = 110.0f` with the comment "the body Helmholtz
  (~110Hz, real guitar F#2-A2)" ([`runtime/sound.h:4465-4469`](../../runtime/sound.h)). Part 28's "Part
  4: The Hollow Body" is about exactly that resonance, and notes it behaves "analogous to that of a bass
  reflex loudspeaker."
- **The excitation lowpass is the pick's hardness.** Part 28: "neither your fingertips nor a plectrum
  are infinitely small and hard ... This acts as a low-pass filter, suppressing the higher harmonics."
  That is `timbre` in both engines.
- **The sub-oscillator weight has a name in the book.** Our `eng_p[0]` "fundamental reinforcement ... the
  low-end WEIGHT a bare KS string lacks (the 'thin' cure)" is doing the job Part 28 assigns to the body's
  low modes.

### H2. The pick comb is quantized to whole samples, so its notches drift off-harmonic

- **Measured:** at the 1/3 position the notches should sit exactly on h3, h6, h9, h12. `pos` is
  `(int)(period * frac)` ([`runtime/sound.h:2827`](../../runtime/sound.h)), so at A2 (period 401)
  `pos = 133` and the true nulls land at n = 3.02, 6.03, 9.05; at A4 (period 100) `pos = 33` gives
  3.03, 6.06, 9.09. The error compounds with harmonic number, which is visible in H0: h6 and h12 are
  deep (-21.9, -23.9) while h9 is shallow (-9.8) and the neighbouring h10 is *deeper* (-24.0) than the
  harmonic that should have been notched.
- **Why it matters:** the drift is worst for short periods, i.e. the high register, and it means the
  "flanging" sweep Reid describes is subtly mistuned as you move up the neck.
- **The fix is already in the header,** twice: `ks_tap_read` does linear-interpolated fractional reads
  ([`runtime/sound.h:4490`](../../runtime/sound.h)), and `PIANO` tunes its loop with a fractional-delay
  allpass (`pn_apc`/`pn_aps`).
- **Audible home:** `pluck` (its morph knob is the demo), `guitar`, `harp`-family presets.

### H3. `PLUCK` and `GUITAR` have a single-exponential decay; the book's guitar envelope is two-stage

- **Book:** Part 28's "Part 5: Amplitude Response" gives the cause, and it is physical rather than
  cosmetic. A string plucked *parallel* to the top plate "decays relatively slowly"; plucked
  *perpendicular*, "the initial level is greater, but the sound decays more quickly" (Figures 9a, 9b).
  Real plucks are neither: "you will rarely, if ever, pluck the string in exactly these fashions, so
  the true amplitude response will look more like that shown in Figure 10" — captioned "A realistic
  decay curve for a plucked guitar note", and distinctly two-stage.
- **Engine:** `sound_guitar_sample` computes one `fb = t60_to_fb(t60, f)` per note and uses it for the
  whole ring ([`runtime/sound.h:4509-4510`](../../runtime/sound.h)). Measured (H0): after the first
  100 ms window the ratio is a flat ~0.80 per window, i.e. mono-exponential. Only the first window
  (1.00 → 0.70) is faster, and that is largely the 700-sample onset click.
- **`PIANO` already has this and its comment mis-attributes it.** `pn_dd` is "DOUBLE-DECAY: extra
  per-period loss right after the strike, relaxes to 0 (~0.2s). The fast initial drop that says
  'struck', not 'plucked harp'" ([`runtime/sound.h:312`](../../runtime/sound.h)). Per Part 28 a plucked
  guitar has a two-stage decay *too* — so two-rate decay is not what distinguishes struck from plucked,
  it is common to both, and the harp-versus-piano difference lies in the proportions. Cheap to port,
  and the physical framing (two polarisation planes decaying at different rates) is more honest than a
  tuned envelope.
- **Audible home:** `guitar`, `pluck`, `upright`, `jangle`.

### H4. `GUITAR` has no sympathetic resonance, and Reid's demo is a guitar demo

- **Book:** Part 28's "Part 6: Other Factors" opens with it and gives a listening test: "Find an
  acoustic guitar and damp five of the strings. Then pluck the free one ... Now release the five damped
  strings, and play the same note on the sixth. It's different, isn't it?" The mechanism: energy passes
  through nut and bridge into the other strings, so you have "nine vibrating resonators (six strings,
  the top plate, the bottom plate and the air in the cavity) rather than four."
- **Engine:** `PIANO` carries `pn_symp` ("sympathetic-resonance level (per voicing)",
  [`runtime/sound.h:301`](../../runtime/sound.h)). `GUITAR` has no equivalent term. The engine that
  Reid uses to *teach* sympathetic resonance is the one of ours that lacks it.
- **Audible home:** `guitar`, and it is most audible on chords, so `jangle`/`mariachi`/`alleycat`.

### H5. The body is a filter on the output, not a coupled resonator

- **Book:** Part 28 "Part 3: Coupling The String & The Plate" is emphatic that the coupling runs *both
  ways*: "the plate absorbs energy from the string, thus sucking energy out of some of its modes ... The
  vibrating plate then passes some energy back, exciting new modes in the string itself, including modes
  that were not present in the original vibration ... the modified vibrations in the string now excite
  the plate in a new way ... and so on." Part 29 Figure 12 builds it as an explicit feedback loop, and is
  careful about *how*: not at audio rate, because "that ... is amplitude modulation. This will result in
  the creation of unwanted side bands". His answer is a slow side-chain (highpass, envelope-follow, S&H,
  slew, invert) so the waveform "change[s] more subtly over the course of a few cycles."
- **Engine:** `gt_body[4]` runs in **parallel** on the string output and is mixed in via `gt_bodymix`
  ([`runtime/sound.h:4519-4523`](../../runtime/sound.h)). Nothing returns to the delay line, so the body
  cannot put back the even harmonics the pick comb removed, and cannot evolve the timbre over the first
  few cycles.
- **This is the same class of gap as brass §E5.** There, the fix is "model the bell to fill the harmonic
  series natively rather than synthesize evens downstream" (handoff fix #3). Here it is "model the body's
  return path rather than paint its resonances on the output." Two engines, one structural idea, and
  doing either would probably teach us how to do the other.
- **Audible home:** `guitar`.

### H6. No pickup, therefore no electric guitar, and every amp cart we ship is driving an acoustic

This is §H's headline: a named gap, a precise spec, and a set of carts already waiting for it.

- **Book:** Part 30 identifies the pickup as *the* reason synth patches fail at electric guitar, and
  describes it implementably. Two facts: "the distance over which the pickup can detect the string's
  motion is very short, so it is only sensitive to the small part of the string that lies immediately
  above it", and "when the string is stationary along the length detected by the pickup, no signal is
  generated." Consequence one, position: a pickup a third of the way from the bridge combs the series,
  and "the result is much like that of passing the wave through a comb filter" (Figure 10). Consequence
  two, and this is the part nobody guesses: "the output of any harmonic is proportional to the
  **velocity** of its motion at the point on the string that lies immediately above the pickup", which
  makes the low end of the spectrum "much flatter than that of the common analogue waveforms (most of
  these conform to the 1/n amplitude relationship)."
- **Engine:** `INSTR_GUITAR` is documented "acoustic/nylon/banjo/koto/harp/uke/pizzicato"
  ([`runtime/studio.h:328`](../../runtime/studio.h)) and there is no electric guitar anywhere in the
  roster (`grep -i "electric guitar" runtime/studio.h` → nothing). `EPIANO` models a pickup nonlinearity
  for Rhodes/Wurli, so the *concept* exists in the file; the string engines read a single tap.
- **And here is the part that makes it worth doing:** `combo`, `pedalboard`, `tubescreamer`, `wba` and
  `mixbooth` all drive `INSTR_GUITAR`. So `ampcab.h`'s five amp voicings, the `drive_voice` Tube
  Screamer / RAT / Big Muff models, and the whole pedalboard are plugged into an **acoustic** guitar.
  The dirt chain has been built out ahead of the instrument that belongs in front of it.
- **Fix shape:** a second, position-dependent tap on the existing KS line, differenced (velocity) rather
  than read directly. `ks_tap_read` already exists. Two taps for a humbucker. This is a small change with
  a large, immediately audible payoff, and it is the only §H item that unlocks a genuinely new sound
  rather than improving an existing one.
- **Audible home:** `combo`, `pedalboard`, `tubescreamer` — all three currently misrepresent what they
  are demonstrating.

### H7. `GUITAR` spent its pick-position axis on mute

- **Engine:** "fixed pick comb at ~1/4 string — pick position is baked here (morph carries mute, not
  pick pos)" ([`runtime/sound.h:4448-4449`](../../runtime/sound.h)).
- **Book:** Part 28 lists plucking position as the **first** of its eight obstacles and the first thing
  it teaches, with an explicit listening test. `PLUCK` exposes it; `GUITAR`, the fuller instrument,
  cannot move the picking hand.
- **Why this is a real tension, not a bug:** the three-macro discipline
  ([ADR-0017](../decisions/0017-three-macro-core-plus-engine-aux-channel.md)) means something had to
  give, and mute earns its place. But `eng_p[]` is the documented aux channel for exactly this
  situation, and pick position is a note-on-only parameter, which is the cheapest kind to put there.
- **Audible home:** `guitar`.

### H8. The high register loses almost all its harmonics (measured, not diagnosed)

- **Measured:** the same 1/3-position pluck at A4 (440 Hz) reads h2 -7.1, h3 -31.4, h4 -38.3, h5 -54.3,
  and h6 through h9 between **-74 and -83 dB**. At A2 the series was still alive past h12. So the upper
  register is close to a pure fundamental.
- **Why I am not calling it a defect:** three things interact at short periods (the excitation lowpass,
  the KS loop filter's damping average, and the peak-normalize over a much shorter buffer), and a real
  plucked string genuinely does lose its upper harmonics faster up the neck. But the magnitude looks
  extreme and nothing in the docs predicts it, which is exactly the profile of an unexamined bug.
- **The test:** sweep `tune-check`-style across the register and plot harmonic count. If the count
  collapses faster than a real string's, the loop filter's fixed `0.5` averaging coefficient is the
  first suspect, since its cutoff is relative to sample rate, not to the note.
- **Audible home:** `guitar`, `pluck` — audible as high notes sounding thin or "plinky".

### H9. Guitar voice allocation is part of the instrument, and this is the third time the book has said so

- **Book:** Part 29 makes it the *first* difficulty, before any DSP. Notes on the same string curtail
  each other ("the plucking of each new note terminates the previous one, reinitialising the brightness
  and loudness contours"), while notes on different strings ring on freely. "How do we decide whether
  any given note in our guitar imitation should curtail a previous one and, if so, which one? This is a
  problem that needs a computer for its solution." He also insists on per-string voicing: "the initial
  tone and amplitude characteristics of, say, a 0.052-inch wound bottom 'E' string are quite different
  from a 0.009-inch top 'E' string."
- **Engine:** `instrument_choke(slot, target)` is a 1:1 pairing (built for open/closed hi-hat). A
  six-string model needs six choke groups plus a fret-to-string assignment, and one voicing per string
  rather than one voicing transposed across the keyboard.
- **The pattern:** this is the third independent place the series has said allocation is instrument
  design, not plumbing — §B3 (mono note priority), §B7 (poly voice assignment and per-voice character),
  and now per-string assignment. Worth treating as one theme rather than three items.
- **Audible home:** `guitar`, `fretboard`, `jangle`, `alleycat` — most audible on strums.

### H10. Attack level, cited for the third time

Part 29 models pluck *direction* as an envelope with independent attack level and decay time: "We use
an AD contour generator that offers simultaneous control over the amplitude of the Attack (AL, Attack
Level), as well as the Decay Time (DT). If the strum is parallel, the CV causes AL to decrease and DT
to increase. If it is perpendicular, AL increases and DT decreases", with velocity routed to both.
That is §C6 again, after Part 8's spit brass and Part 25's `A(AL)A2S`. Three chapters, three
instrument families, one missing envelope parameter.

### H11. Reid's verdict validates our approach, and narrows §G

The arc ends with an unusually flat conclusion, stated twice. Part 28: "these eight [reasons] give you
a good idea why you can not create authentic-sounding acoustic guitar patches using analogue
subtractive synthesis. **This is one occasion when only digital technology will do!**" Part 30, after
dissecting three factory patches from the Axxe, the SH-101 and the Minimoog and rejecting all three:
"Clearly, the world does not permit practical analogue, subtractive synthesis to reproduce a guitar
sound. Even the sound of the electric guitar has eluded us." On the stretched harmonics specifically:
"there's nothing we can do to model it using subtractive synthesis."

Two consequences:

1. **Our physical-model choice for `PLUCK`/`GUITAR`/`PIANO` is the one the book endorses.** Where §E
   and §F had to translate his subtractive patches, here he tells us to stop translating. Every §H item
   above is therefore about improving the physical model, not about adopting his signal chain.
2. **It bounds §G.** The subtractive-imitation idea is right for brass, string machines, and leads, and
   Reid says explicitly it is *wrong* for plucked strings. So §G should be scoped to the families where
   the analogue imitation is itself the desirable sound, and should not grow a guitar. Noted there.

### Suggested plucked-strings step order (see also §I for the piano half)

| # | Step | Kind | Where |
|---|---|---|---|
| 1 | H6 pickup tap → the electric guitar the amp carts want | engine, small, new sound | `combo`, `pedalboard` |
| 2 | H1 write the 0.55 comb's justification into the comment | comment only | none |
| 3 | H3 port `pn_dd`'s two-rate decay to GUITAR/PLUCK | engine, exists in file | `guitar` |
| 4 | H2 fractional-delay the pick comb | engine, primitive exists | `pluck` |
| 5 | H8 diagnose the high-register harmonic collapse | measurement first | `pluck` |
| 6 | H4 sympathetic resonance on GUITAR (port `pn_symp`) | engine, exists in file | `jangle` |
| 7 | H7 pick position onto `eng_p[]` | engine, small | `guitar` |
| 8 | H9 a per-string allocator (cart-land, with §B3) | probably `mono.h`'s sibling | `fretboard` |
| 9 | H5 body → string feedback (pairs with brass fix #3) | engine, hard | `guitar` |

---

## I. Recipe pass 4 — PIANOS (Parts 41-44)

STATUS: EXPLORING — nothing queued.

Four chapters: Part 41 is the physics, Parts 42-44 build a subtractive piano on a Roland JX10 (with
a hard-sync tutorial in the middle, because the patch needs it). **This is the best-matched engine in
the whole audit.** `INSTR_PIANO` gets a subtle piece of physics right that it could easily have got
wrong, and most of what Part 41 asks for is already shipped, largely thanks to
[`piano-engine.md`](piano-engine.md)'s fix round. So §I is mostly confirmations, with a short list of
real gaps and one genuinely counter-intuitive finding.

Sources: Part 41 "Synthesizing Pianos" (SOS October 2002), Parts 42-44 "Synthesizing Acoustic Pianos On
The Roland JX10" (SOS November, December 2002, January 2003). Engine: `sound_piano_start`
([`runtime/sound.h:4599`](../../runtime/sound.h)), `sound_piano_sample`
([`:4693`](../../runtime/sound.h)). Prior art: [`piano-engine.md`](piano-engine.md).

### I0. Measurement taken

`INSTR_PIANO`, grand voicing, no pedal, four octaves of A, amplitude as a fraction of peak (100 ms
windows, off-tree probe deleted afterwards):

| note | 0.5 s | 1.0 s | 2.0 s |
|---|---|---|---|
| A1 (midi 33) | 0.18 | 0.11 | 0.04 |
| A2 (midi 45) | 0.18 | 0.08 | 0.02 |
| A3 (midi 57) | 0.07 | 0.03 | 0.00 |
| A4 (midi 69) | **0.01** | 0.00 | 0.00 |

Taken to check I1's decay claim rather than assert it. It confirms the claim, and incidentally raises
I5.

### I1. What matches, including one thing that is easy to get backwards

- **The hammer comb is the INVERSE of the pluck comb, and we implement both correctly.** This is the
  finding I most expected to go the other way. Part 41 draws the distinction explicitly: "Whereas the
  position at which a guitar string is plucked determines its maximum displacement, **the piano hammer
  remains in contact with the string long enough to ensure that the position at which the string is
  struck is a node of zero displacement.**" So the excluded harmonics are the complementary set. His
  worked case: a hammer at the halfway point means "the fundamental is missing from the resulting
  sound", and "hammering at the centre will ensure that the sound contains no third harmonic, or fifth,
  or seventh or ninth… or any of the other odd harmonics."

  Our two engines use *different* combs, and the difference is exactly right:

  | engine | excitation comb | nulls at |
  |---|---|---|
  | `PLUCK` / `GUITAR` | **differencing**, `tmp[i] - 0.55·tmp[i+pos]` ([`:2831`](../../runtime/sound.h), [`:4453`](../../runtime/sound.h)) | `n·pos/len` = integer |
  | `PIANO` | **averaging**, `(tmp[i] + tmp[i+ps])·0.5` ([`:4651`](../../runtime/sound.h)) | `n·ps/len` = ½ + integer |

  The two null sets are exactly interleaved. And at `ps = len/2` the averaging comb nulls `n` = 1, 3,
  5, 7…, i.e. the fundamental and every odd harmonic, which is Reid's sentence verbatim. The code
  attributes it to "navkit `applyPickPosition`" and carries no note that the sign is load-bearing —
  worth adding one, because a future tidy-up that "unified" the two combs would silently break the
  piano's physics.
- **Stretched tuning, and by Reid's own mechanism.** `piano_stretch_freq` /`PIANO_STRETCH_K`
  ([`:4604`](../../runtime/sound.h)). Part 41 explains both the cause (the string needs a finite length
  to bend over bridge and nut, so its effective length shortens, stretching the series toward
  1:2:3:4.01:5.02:6.04) and the consequence, which is the good bit: play A440 with the A two octaves
  up and "because A440 exhibits stretched harmonics, the upper 'A' will, if tuned to 1760Hz, sound a
  fraction flat! Indeed, the human ear/brain is so accustomed to this that a perfectly tuned piano not
  only sounds out of tune, **it sounds dull**." His fix is Figure 14: "making the oscillators track the
  keyboard at a ratio just a fraction greater than 1:1", which is what we do.
- **Register-dependent decay is free, and measured.** Part 41 Figure 9 wants low notes to decay more
  slowly than high ones. Because the loop applies a fixed per-*period* loss, higher notes take more
  loop passes per second, so T60 falls with pitch structurally: I0 shows A1 still at 0.11 after a
  second while A4 is gone. Nothing to build. Worth recording precisely so nobody adds a redundant
  register term on top of it.
- **Two-rate decay, and Part 41 supplies the piano's own cause.** `pn_dd`
  ([`:4629`](../../runtime/sound.h), relaxing at `0.99975` per sample ≈ 90 ms). Part 41: "the tail can
  linger for tens of seconds, which tells us that the rate of the decay diminishes as the note
  progresses. This is because, **as the pairs and tricords interact**, the rate at which energy is
  transferred to the soundboard diminishes." Note this is a *different* mechanism from the guitar's
  (Part 28 attributes the guitar's two-rate decay to the string's two polarisation planes). Both
  instruments have the behaviour, for different reasons — which is exactly why §H3's correction to
  `pn_dd`'s comment matters: the comment claims two-rate decay is what says "struck, not plucked", and
  it isn't.
- **We attempt the stage Reid says nobody has managed.** Part 41 splits a piano note into three
  stages: the hammer blow, "the transition period during which the strings begin to oscillate
  harmonically", and the tail. On stage 2: "it is here that the nature of the waveforms is changing
  most rapidly. I suppose it's possible that we could invent a synth architecture to imitate this, but
  **I know of nobody who has succeeded**." Our `pn_ksb_cur` brightness bloom (τ ≈ 283 ms,
  [`:4706`](../../runtime/sound.h)) is a stage-2 model, and a waveguide gets much of the rest
  structurally, because redistribution among string modes is what the delay line *does*. Reid was
  writing about subtractive synthesis, so this is not a contradiction — but it is a case where our
  choice of method buys something he explicitly could not have.
- **Velocity drives timbre, not just level.** Part 41 Figures 11-13 want brightness to respond to note
  number, to velocity, and to a contour whose decay rate itself depends on note number. We do
  velocity → hammer hardness ([`:4624`](../../runtime/sound.h)) and velocity → knock amount
  ([`:4634`](../../runtime/sound.h)), plus the register-scaled bloom.
- **Soundboard, sympathetic resonance and pedal.** `pn_body[4]`, `pn_symp`, `pn_dampg` on morph. Part
  41's sustain-pedal section is a description of sympathetic resonance: "the energy then passes through
  the bridge and soundboard to excite other strings. Some will vibrate sympathetically, because they
  share modes of vibration with the initial note."
- **Our macro assignment is validated from an unexpected direction.** Part 44, on programming the JX10:
  aftertouch must be zero everywhere because "it's **not possible** to affect the nature of a piano
  note (other than to curtail it) once it has sounded. Any parameters that let you change the
  brightness, the loudness, or add vibrato by bearing down on a depressed key must be set to zero." Our
  PIANO fixes hammer/voicing at note-on and gives the one axis that *can* legitimately change mid-note
  — the pedal — to `morph`. That is the physically correct split, and it was chosen for the
  three-macro discipline rather than for this reason.

### I2. Hammer position is per-voicing, where the book says per-register

- **Book:** Part 41: "you would think that, to obtain a consistent tone, piano builders would position
  the hammers consistently from one end of the keyboard to the other. But this is not the case; you will
  find them **anywhere from one seventh of the way along the string to about one 15th**. This results in
  different initial frequency relationships, different interactions as the hammer leaves the string, and
  a different spectrum for the body of each note."
- **Engine:** `ps = (int)(pv->strike * len)` ([`:4647`](../../runtime/sound.h)) — one constant per
  voicing (grand / bright / harpsichord / dulcimer / clavichord / celesta), identical across all 88 keys.
- **Why it is cheap:** the comb already exists and already scales with `len`. This is a register term on
  one float, and Reid even gives the range to interpolate across (1/7 → 1/15).
- **Audible home:** `piano`, `upright` — audible as the top and bottom of the keyboard having distinct
  characters rather than one voicing transposed.

### I3. Two strings maximum, per-voicing, and they do not exchange energy

- **Book:** Part 41 lays out the register progression: "At the bottom end, single strings are wrapped to
  high thickness … Next come notes produced by pairs of wrapped strings, then notes produced by triads
  (or 'tricords') of wrapped strings, and finally tricords of unwrapped strings." And the character is
  in their imperfection: "it's all but impossible to tune these to the same pitch … the strings will
  soon become out of phase with one another … This leads to interference, with the strings **swapping
  energy**, reinforcing and at other times cancelling each others' modes."
- **Engine:** `pn_detune > 1.00001f` gives an optional *second* string with its own delay line and
  allpass ([`:4673`](../../runtime/sound.h)). It is chosen per voicing, not per register, capped at two,
  and it runs with the *same* `effDamp` and the *same* dispersion coefficients as string 1
  ([`:4743-4750`](../../runtime/sound.h)) with no cross-coupling — only a detune.
- **The theme:** this is the **third** place a recipe pass has found us modelling a coupled system as
  independent parts — §E5 (the bell that should fill the series natively), §H5 (the guitar body with no
  return path into the string), and now the tricord. Worth treating as one architectural question about
  coupling rather than three separate engine tweaks.
- **Audible home:** `piano`, `upright`.

### I4. Inharmonicity is fixed at note-on and never responds to level

- **Book:** Part 41 and Part 28 both say the stretching depends on *two* things: "the string appears
  shorter at high frequencies **and high amplitudes** than it does at low frequencies and low
  amplitudes. This sharpens higher, **louder** harmonics."
- **Engine:** `B = pv->stiff² · 0.015 · fscale` where `fscale` derives from `freq` only
  ([`:4654-4657`](../../runtime/sound.h)). Velocity feeds `hard` (the excitation lowpass) but not `B`.
  So a fortissimo note has exactly the inharmonicity of a pianissimo one, and it cannot relax as the
  note decays and the amplitude falls.
- **Audible home:** `piano` — the tell is that hard and soft strikes differ in brightness but not in
  the metallic "stretch" of the attack.

### I5. The top octave is gone in half a second

- **Measured (I0):** A4 is at 0.01 of peak by 500 ms and inaudible by 1 s, on the *grand* voicing with
  no pedal. A real grand's A4 rings for several seconds.
- **Why it is not simply "correct because Figure 9 says so":** Reid does want shorter decays up the
  keyboard, and T60 ∝ 1/f is what a fixed per-period loss gives. But 1/f is the naive result, and real
  pianos are deliberately built to beat it (longer treble strings, and on many designs no dampers at
  all in the top octave). The measured curve looks steeper than the instrument.
- **Possible shared cause with §H8.** That item found `GUITAR`/`PLUCK` high notes reduced to almost a
  pure fundamental (h6-h9 at −74 to −83 dB at A4). Both engines apply loop coefficients that are fixed
  per period rather than scaled to the note, so both would lose the top of the register for the same
  structural reason. Worth investigating as one thing.
- **Audible home:** `piano`, `upright` — audible as the treble sounding plinky or celesta-like.

### I6. Peak level does not fall with pitch

Part 41 Figure 9 shows high notes with **both** a shorter decay and a lower maximum gain, and the
architecture in Figure 10 routes keyboard tracking to "both the maximum gain of a VCA and the decay
rate of the contour that shapes it". We get the decay for free (I1) but nothing tapers the peak, so
our treble arrives at full level and then vanishes. Small, and it interacts with I5 — fixing the decay
without the level taper may be the wrong half.

### I7. The bottom octave's fundamental should be WEAK, and our sub-oscillator pushes the other way

The counter-intuitive one, and it cuts against a shipped fix.

- **Book:** Part 41: "for the lowest notes on a grand piano, **the fundamental pitch has very low
  amplitude**, and the note that you think you hear is to some extent implied by the harmonics. This
  suggests that we require a high-pass filter for the lowest notes in our synthesized sound." Reid then
  declines to bother, on diminishing returns.
- **Engine:** `eng_p[0]` is "fundamental reinforcement … a sub-oscillator at the note's pitch,
  envelope-following the string, mixed under it — adds the low-end WEIGHT a bare KS string lacks (the
  'thin' cure)" ([`runtime/sound.h:337-343`](../../runtime/sound.h)), and `PIANO` uses it.
- **Why this is worth flagging rather than acting on:** the "thin" complaint was real and the sub-osc
  fixed it, so this is not a call to remove it. But for the bottom octave specifically it is
  reinforcing the partial that a real grand has *least* of, which may be why our bass can read as
  synthetic-round rather than piano-huge. The physically honest version is register-dependent: sub-osc
  weight tapering off toward the bottom, with the body's low modes carrying the weight instead. That is
  a testable A/B, not an obvious win.
- **Audible home:** `piano`, `upright` — bottom two octaves.

### I8. Smaller items

- **Two bridges.** Part 41: pianos "generally have two of them — one for the treble strings, and one
  for the bass — and … they are coupled through the soundboard", and "using different bridges can
  change the sound of a piano by a remarkable degree". We have one body per voicing with no bass/treble
  split. Low priority; listed for completeness.
- **Soundboard modes are enharmonic and the plate is irregular.** Part 41: "piano soundboards have an
  irregular shape and are chamfered, so our previous discussions of vibrations in flat plates are, at
  best, approximations". Our `pn_body[4]` is four biquads, which is the standard coarse approximation
  and the same order as `gt_body[4]`.
- **Our hard sync is always ratio-locked; the JX10's is not.** Part 42's two rules: the output pitch is
  always the master's, and if the master is *lower* than the slave then moving the slave changes timbre.
  Then the aside that matters: with only the master tracking the keyboard, "the frequency relationship
  between the master and slave is different for each note, resulting in **different tones for each**".
  Our `sync_ratio` ([`runtime/sound.h:168`](../../runtime/sound.h)) is a ratio, so the slave tracks and
  the timbre is constant across the keyboard. There is no way to pin the slave in Hz. This is the exact
  mirror of §B2: there, nothing tracks when it should; here, something always tracks when you might not
  want it to.

### I9. Layering is the piano, and §G should encode it

Part 44's conclusion is the most transferable thing in the arc, and it is a *cart* pattern rather than
an engine one.

- **The claim:** "The secret — and it's an important one — lies in the combination of two sounds that
  are similar enough to be indistinguishable within the composite, but different enough to create a
  sound that is more interesting than either of the components in isolation."
- **The mechanism, and note what he is imitating:** "the detuned harmonics of the complex, sync'd
  waveforms sweep in and out of phase with one another, reinforcing and then interfering with one
  another destructively, **to imitate the energy interactions within an acoustic piano**." So the
  layering is standing in for exactly the tricord coupling of I3.
- **The role split:** "Piano 1B supplies the initial thunk, while Piano 1A has the richer spectrum and
  provides more of the body of the sound … Then, towards the end of the note, Piano 1B dominates again
  (thanks to the longer Decay and Release in ENV2) and the filter closes to leave just the fundamental
  and a few low harmonics in the tail." Two patches, one detune (`Dual Detune +13`), different
  envelopes, crossfading roles across the note.
- **And his honest verdict on it:** "'H1: Acoustic Piano' has many of the characteristics of an acoustic
  or electro-mechanical piano, without sounding anything like the former, or even quite like the latter.
  It's responsive, it's expressive and, for many purposes, it's every bit as usable as a Fender Rhodes
  73 or a Wurlitzer EP200. In fact, there are times when I would still use it today, in preference to
  any of the 'real' things." That is precisely the §G thesis stated by the author: the imitation is a
  worthwhile instrument even though it fails as an imitation.
- **For us:** two slots, a small detune, different envelopes, opposite role weighting across the note.
  Expressible today with no engine change, and it is what §G's voicing table should hold. Pairs with
  §F8's 16'/8'/4' string-machine layering.
- **Audible home:** a two-slot layered patch in `piano`, or `upright`.

### I10. Reid's verdict, for the third instrument in a row

Part 41 opens with it: "there has never been a convincing acoustic piano produced by subtractive
synthesis, additive synthesis, **or FM synthesis**. Only samples appear to do the trick." And closes:
"is it impossible to create an acoustic piano patch on an analogue synth? The strict answer is 'yes'."
He also notes the instruments that did crack it (Roland MKS20 / RD1000 / HP5600) were "based on an early
physical modelling concept". So: guitar (§H11), electric guitar (§H11), and now piano — three families
where the book sends you to physical modelling or samples, which is what we already do. Every §I item
is about improving the waveguide, not adopting his signal chain. §G's boundary widens accordingly:
**not brass-adjacent only, but specifically "not the struck and plucked strings."**

### Suggested piano step order

| # | Step | Kind | Where |
|---|---|---|---|
| 1 | I1 comment that the averaging comb's sign is load-bearing | comment only | none |
| 2 | I2 scale hammer position by register (1/7 → 1/15) | engine, one float | `piano` |
| 3 | I5 + §H8 investigate the shared high-register loss | measurement first | `piano`, `pluck` |
| 4 | I4 make inharmonicity level-dependent | engine, small | `piano` |
| 5 | I9 a two-slot layered piano patch | cart only, free | `piano` |
| 6 | I6 taper peak level with pitch | engine, small | `piano` |
| 7 | I7 A/B tapering sub-osc weight in the bottom octave | engine, judgement call | `upright` |
| 8 | I3 third string + inter-string coupling | engine, hard, with §E5/§H5 | `piano` |

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
