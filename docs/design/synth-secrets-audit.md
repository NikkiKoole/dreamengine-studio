# Synth Secrets audit — the engine cross-checked against Gordon Reid's 63-part series

STATUS: EXPLORING — findings ledger, nothing approved. Every item below is a *candidate*, deliberately
not queued. The owner's rule (2026-07-28): **one small step at a time, and no engine change lands
without a cart where you can hear it.** So each item names its audible home before it names its code.

Companion to [`audio-notes.md`](audio-notes.md) (the sound HUB). This doc is the outside-in view: what a
canonical synthesis text says the machine should do, versus what `runtime/sound.h` actually does.
Nothing here is a bug report. Several divergences are deliberate and documented in the code; they are
recorded anyway so the choice stays a choice instead of decaying into an accident.

**Layout.** §A-§D are the **architecture** pass (is the engine the right *shape*?), read from the theory
chapters. §E-§F and §H-§M are the **recipe** passes, one family at a time (does one engine's *voicing*
match the physical analysis?), and they carry measurements: **§E brass**, **§F strings**, **§H plucked
strings**, **§I pianos**, **§J drums**, **§K flutes**, **§L the Hammond**, **§M the effects layer**.

**The sieve is COMPLETE (2026-07-28).** All 63 articles have been read, the instrument families and the
effects layer both. What is left is not more reading but the step-by-step guide — see §D5.
**§G** is a design question the recipe passes raised: every patch in the book is subtractive and all our
imitative engines are physical models, which may mean we are missing a category rather than
mistranslating one — and §H then §I bounded it, since Reid says outright that subtractive cannot do a
guitar or a piano.

Two things a reader should know before trusting any single section. **§C12 was corrected** by §F: Reid
contradicts himself between Part 10 and Part 47 and explicitly retracts the earlier claim, which §C12
had quoted as law. And **§I found the best match in the audit** — `INSTR_PIANO` gets a piece of physics
right that is easy to get backwards — so the passes are not uniformly critical; where we are right,
that is recorded too, because the point is to keep choices from decaying into accidents in either
direction.

Note that §C12 was **corrected** by the strings pass. Reid contradicts himself between Part 10 and
Part 47 and explicitly retracts the earlier claim, which the original §C12 had quoted as law. Expect
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

> **Numbering fixed 2026-07-28.** The mapping above is now **validated**: the PDF prints an explicit
> `PART N:` label on the first thirty articles, and all eleven checkable anchors (Parts 18, 21, 23-30
> and 63) agree with consecutive-monthly. Before that check, §F and §I were each **off by one** — the
> strings arc was labelled Parts 45-50 and is really **46-51**; the pianos arc was labelled 41-44 and is
> really **42-45**. The *issue months* were right throughout (they came from the articles' own page
> footers), so only the numbers moved. Corrected everywhere, including in the docs that cite back here.
> If you have an earlier revision open, trust the month, not the number.

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
`tr808.h` runs the six-oscillator square bank through a highpass. Part 39 (SOS July 2002) dissects the
real TR-808 cymbal as six enharmonically-tuned square oscillators split into bands by bandpass and
highpass filters. Same family. *(Citation corrected 2026-07-28: this said "Part 40, August 2002", which
is "Synthesizing Bells". The cymbal dissection is Part 39. And §J5 shows the match is only partial.)*

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

> **✅ SHIPPED 2026-07-30** (plan 3.26). The slew now runs on `log2(freq)`, in
> `sound_glide_step()`. **The asymmetry was 45 percentage points and is now 4.5.** A one-pole is 63.2%
> through after one time constant *in whatever domain it slews*, which turns this finding into a single
> scale-invariant number — measured on the new [`glideprobe`](../../tools/carts/glideprobe.c) over three
> octaves at a 1000 ms time constant:
>
> | at one time constant | before | after | even-in-pitch ideal |
> |---|---|---|---|
> | glide **up** | 82.4% through the interval | **64.8%** | 63.2% |
> | glide **down** | 37.2% | **60.3%** | 63.2% |
>
> The residual ±1.5 points is measurement bias, not engine error: a sine's spectral centroid reads
> slightly high, which moves the up and down readings in *opposite* directions in "percent through", so
> the two bracket 63.2% rather than both sitting above it. The tail number is the one that was really
> damning, though: 2.0 s into a 1000 ms **down**-glide the old curve was still **12.7 semitones sharp**,
> because in Hz the remaining distance looks tiny while in pitch it is enormous.
>
> Two things came with it that are not the domain change and matter as much. **A SNAP:** a one-pole
> approaches asymptotically and never arrives, so `freq` never again equalled `freq_target` — which both
> left the glide audibly unfinished and (once the fast path was gated on that equality) would have pinned
> the expensive path permanently hot. It now lands exactly on target once inside 0.0002 cent.
> **A GATE:** the log/exp only run while a voice is actually gliding, so a non-gliding voice pays one
> float compare and a cart that never glides is byte-identical. Cost is bounded analytically at ~60
> cycles per gliding voice-sample (≈2.6% of one core with all 32 voices gliding, under 0.4% for a
> monosynth); the wall-clock renders were dominated by compile time and measured nothing.
>
> Re-gated as the finding asked: `tune-check` **no new drift** (the waveguides re-derive delay length
> from `freq` every sample, so this was the real risk), `psola-check` no artifact regression,
> `dc-check`/`level-check`/`soak-check`/`soundcheck` clean, and `web-audio-check` still sample-identical
> native-vs-wasm — worth noting because `log2f`/`exp2f` are exactly where the two could have diverged.
>
> **AND THEN THE UNIT, resolved the same day.** The domain fix left `ms` naming a *time constant*, so a
> knob reading 1000 ms kept moving for up to 6.6 τ — and by an amount that depended on the interval, which
> meant a one-pole could not honestly implement "constant time" either. The one-pole is now gone: portamento
> is a **fixed-duration ramp with an exponential shape**, so it keeps the RC-lag curve *and* lands on time.
> Measured at one `note_glide(600)` setting: fifth 0.59 s, octave 0.59 s, three octaves 0.60 s, three
> octaves downward 0.59 s. It is also cheaper than the one-pole it replaced. Full argument, including why
> this is *more* hardware-faithful rather than less (a Minimoog's glide knob has no numbers on it because a
> one-pole has no duration to print — the millisecond unit was always ours), in the plan's 3.26 subsection.
> **GLIDE SCALE shipped the same day too**, so this finding is fully closed:
> `note_glide_scale(h, amount)` sets how much a slide's time depends on the distance, as
> `gl_len = ms × |Δoctaves| ^ amount`. It shipped as a two-way switch first and that was wrong — asking what
> the hardware does turns up a THIRD answer. An RC lag's perceived duration grows with the interval
> logarithmically, so the spread from a semitone to three octaves is **1x** for constant time, **~2.2x**
> for a real analog lag, and **36x** for per-octave: **analog sits between the two panel settings and much
> closer to constant**, and a binary switch cannot reach it. Hence a dial, with `GLIDE_CONSTANT` /
> `GLIDE_ANALOG` / `GLIDE_PER_OCT` as named stops. `ms` is always the time for a one-octave slide at every
> setting (the octave is the pivot, since `1^anything == 1`); measured in the engine at amount 0.2, the
> octave leg is 0.58 s against 600 ms predicted and three octaves 0.76 s against 747 ms. Both endpoints are
> byte-identical to the switch version. It needed the fixed-duration ramp to exist first — a one-pole's
> perceived duration already varied with the interval, so there was no constant to scale away from.
> `instrument_glide` / `instrument_glide_scale` are the per-slot twins, so glide feel is a patch property.

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

> **✅ BOTH HALVES SHIPPED 2026-07-29.** Half (a): `instrument_keytrack(slot, amount)` exists (plan 2.1a). 0 = absolute
> Hz (default, byte-identical), 1 = true 1V/oct, 0.93 = Reid's musical value; with tracking on the cutoff you
> set is the value at **C4**. Measured across four octaves on a resonant ladder, a fixed cutoff moves the
> spectral centroid only 391→686 Hz while full tracking moves it 259→1535 Hz, landing on the intended
> 200/400/800/1600. Applied at note-on only — `note_cutoff()` stays absolute on purpose. New cart
> [`keytrack`](../../tools/carts/keytrack.c) plots pitch against cutoff so the tracking is *visible*.
> Half (b) (plan 2.1b): **`ENV_CUTOFF_OCT` / `LFO_CUTOFF_OCT`** express a sweep's depth in octaves, reaching
> `instrument_env`/`note_env`, `instrument_lfo`/`note_lfo` and `instrument_follow`/`note_follow`. New
> constants rather than a redefinition, so the **59 carts** on the Hz forms are untouched (four of them
> render byte-identical). Octave modulation multiplies and is applied *after* the additive Hz terms, so the
> two units compose in a defined order. Measured on a keytracked ladder with the depth set to +2 octaves at
> C4 in both units, the octave form's attack centroid doubles per octave (352/696/1379/2746 Hz, ratios
> 1.98/1.98/1.99) while the Hz form drifts *and inverts* the contour (604/805/1163/1919 — the biggest sweep
> on the bass note, the smallest on top). `keytrack`'s row 4/5/6 switches units and draws the sweep top.

- **Book:** Part 6 (SOS October 1999, p.140): "If you use a resonant filter with moderate Q and make the
  cutoff frequency track the pitch, you can create a characteristic 'emphasised' quality that remains
  tonally consistent as you play up and down the keyboard." And at maximum Q: "If the filter tracks the
  keyboard CV accurately, you can then play it as if it were an extra oscillator." Part 23 defines the
  opposite case as the thing that makes formants work: the filter bank's response "is independent of the
  pitch of the source. To see how this differs from conventional synthesizer filtering (in which the
  filter cutoff frequency often tracks the pitch of the note being played) ..." His own patch listings
  carry it as a first-class control: the JX10 tables in Part 43-44 (SOS November-December 2002) list
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

> **✅ SHIPPED 2026-07-29** as [`runtime/mono.h`](../../runtime/mono.h) (plan item 2.2) — a cart-land header,
> zero engine surface: a `Mono` held-key stack + `mono_press`/`mono_release` returning START/GLIDE/RETRIG/
> STOP, with priority LAST/LOW/HIGH/FIRST and triggering SINGLE/MULTI/ANY (Reid's Figures 8/9/11). `sh101`
> drives it from PRIO/TRIG panel switches whose defaults are byte-identical to the pre-change cart, and the
> header carries its own 47-assertion `mono_selfcheck()` built from Part 18's four-priority table.
> **The finding: the SH-101's PORTAMENTO switch is secretly a trigger switch.** At the SAME PORTA setting,
> OFF renders byte-identically to Reid's ANY and AUTO/ON to his SINGLE, while MULTI (re-attack on every press
> but glide on a hand-over) is **unreachable on the real machine's controls** — a measured answer to what the
> engine's conflation of the two axes actually costs. `tb303`, `acidrack` and `moog` still hand-roll theirs;
> converting them is where the §L4-vs-§K6 argument will decide whether this earns promotion into `sound.h`.


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
  *moving*, which is precisely the waveform Part 47 (SOS March 2003) says is "ideal for creating string
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
> nth harmonic is missing" as "a hard, testable law". **Reid himself corrects that claim in Part 47**,
> and calls it "a long-standing mistake usually made in discussions of pulse waveforms". The oracle
> below is still worth building, but the assertion it makes had to change; the original version would
> have gated on something false. Details in §F3.

- **Book, the corrected version:** Part 47 (SOS March 2003, p.154-155). The *nulls* are evenly spaced
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

**D5. The sieve is complete; what is left is the guide.** Eight passes: **brass (24-27) → §E**,
**strings (46-51) → §F**, **plucked strings (28-30) → §H**, **pianos (42-45) → §I**, **drums (31-41) →
§J**, **flutes (52-54) → §K**, **the Hammond (55-59) → §L**, **delays/reverb/effects (22, 60-62) → §M**.
Plus the architecture chapters in §A-§D (1-13, 14-21, 23, 63) and Part 19 closed out in §M8. **All 63
articles read.** Nothing in the series is now unexamined against the engine.

> **➜ THE PLAN NOW EXISTS: [`synth-secrets-plan.md`](synth-secrets-plan.md)** (2026-07-28). It orders every
> finding below into one ledger, classifies each as **FACT / VERIFY / LISTEN / DESIGN** (so "just do it" is
> separated from "needs your ear"), names the cart each lands in, and answers when a finding justifies a new
> engine versus changing an existing one — using this repo's own ADRs 0006/0015/0016/0017 rather than a new
> rule. **Work happens there; findings stay here.**

**Next, and the only thing outstanding: one ordered step-by-step guide** (owner, 2026-07-28: "after we've
sieved through everything we will spend some time to add a step by step guide, but let's first make it
complete"). Its job is *collection*, not authoring — there are now **nine** per-section step tables and the
cheapest items are scattered across all of them. Roughly in ascending cost, the free and near-free ones:

- **No code at all:** §C12's pulse-width oracle (a tool run); §K3 and §E9's doc corrections; §I1 and §H1's
  comment fixes (the load-bearing comb sign, and why 0.55 is defensible).
- **Cart-only, no engine change:** §F2 (`solina` never uses `LFO_DETUNE` or the Random LFO shapes, the two
  rungs Part 46 says the ensemble sound lives on); §J5 (the 808 cymbal's three unequal decays, a `tr808.h`
  edit); §J9 (velocity → snare tone/noise); §E10 (the brass preset's 1 ms attack and 1200 ms release);
  §I9 (a two-slot layered piano).
- **Two table rows:** §L5 (the sawtooth-ish and square-ish Hammond registrations).
- **One field or one enum:** §C1 (LFO delay — and delayed vibrato is currently *inexpressible*); §C3
  (resonance as a mod destination).

And four **cross-cutting themes** the guide should state once rather than nine times: **keytracking**
(§B2, requested in six separate chapters, and Part 26 pins the value at ≈0.93/octave); **level-dependent
inharmonicity** (§E8, §H, §I4, §J8, §K8 — five families, one physical fact); **trigger policy** (§B3, and
§L4 versus §K6 show two shipped instruments needing *opposite* settings); and **coupling** (§E5's bell,
§H5's guitar body, §I3's tricord, §M2's alternative implementation — one architectural question, four
faces).

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

> ⚠ **2026-07-29 — "`PIANO` already owns the primitive that fixes this" is false.** That primitive was
> measured and it is **inert** (allpass coefficient 0.9999948 = the identity; PIANO's own partials are
> harmonic to within 0.4¢). So this item cannot be closed by reusing it; the primitive has to be made to
> work first. Also: the "measurable as harmonic frequencies drifting sharp" home now has a tool —
> `tools/inharm-spec.js --instr BRASS`. See
> [`synth-secrets-plan.md` §2.3(a)](synth-secrets-plan.md#the-premise-failed-three-defects-found-by-measuring-first-2026-07-29).

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

> **❌ DROPPED 2026-07-28 — this section was wrong on all three counts, and that is the finding** (plan item
> 1.4; built as a toggle, A/B'd, envelope left byte-identical). **None of Reid's numbers transfer to a
> waveguide**, each for a structurally different reason:
> - **attack**: a note already reaches full level in ~40 ms with the amp attack at 1 ms — the *bore* makes
>   the onset. A Minimoog has no bore, which is exactly why he needed an envelope to fake one. Literally
>   applied it becomes a swell.
> - **sustain**: with `decay_ms` 0 there is no decay stage, so sustain is a pure level trim. 4 → 7 is
>   exactly +4.86 dB (= 20·log₁₀(7/4)) — "sustain maximum" is arithmetically "turn it up".
> - **release**: the one I expected to win. Swept level-matched, the owner picked the **shipped 1200 ms**;
>   every shorter tail read as *cut off*. The release truncates the **bore's ring-down**, so 1200 ms is not
>   a pad envelope by mistake — it is about how long this bore takes to stop ringing.
>
> And the short releases were provably **clean** (largest sample step 61% of peak, identical to the shipped
> voice; a smooth 6 ms ramp at 5 ms) and still wrong — no oracle here can tell "a clean short decay" from
> "the resonator was cut off". The best §G specimen in the audit. Full numbers and the two calls I got wrong:
> [plan §4 "1.4"](synth-secrets-plan.md).

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

## F. Recipe pass 2 — STRINGS (Parts 46-51)

STATUS: EXPLORING — nothing queued.

"Strings" is two threads in the series and both map onto things we ship, so both are here. Parts 46-47
are string **machines** (the Solina/Freeman ensemble lineage, and PWM), which land on `solina`, `juno`,
`supersaw` and our unison/detune/PWM surface. Parts 48-51 are **bowed** strings, which land on
`INSTR_BOWED`. **Plucked** strings are a separate arc (Parts 28-30, still unread, see §D5) covering
`PLUCK`/`GUITAR`/`PIANO`.

Sources: Part 46 "Synthesizing Strings • String Machines" (SOS February 2003), Part 47 "…PWM & String
Sounds" (SOS March 2003), Part 48 "Synthesizing Bowed Strings • The Violin Family" (SOS April 2003),
Parts 49-50 "Practical Bowed-string Synthesis" (SOS May, June 2003), Part 51 "Articulation &
Bowed-string Synthesis" (SOS July 2003). Engine: `sound_bowed_sample`
([`runtime/sound.h:3778`](../../runtime/sound.h)) + `sound_bowed_start`
([`runtime/sound.h:3708`](../../runtime/sound.h)).

### F1. What matches, and it is a lot on the machines side

The ensemble half is the best-matched area the audit has found. Part 46 walks a Jupiter 6 up a ladder
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

- **Book:** the ladder in §F1 is the whole point of Part 46, and the Random-LFO-on-detune rung is the
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

- **Book:** Part 46 first states it as a curiosity: "Pulse waves whose widths are modulated by triangle
  waves have another, rarely appreciated characteristic; they exhibit pitch modulation that oscillates
  at the PWM rate above and below the true oscillator pitch ... a PWM wave generated by a single
  oscillator exhibits **two pitches**." Part 47 then spends two full boxes proving it, by
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
- **The consolation prize if it is missing:** Part 47's entire practical half is a recipe for
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

- **Book:** Part 48 separates the two spectra explicitly. Figure 8 is "the force waveform measured at
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

- **Book:** Part 48 names the sound and its cause. Insufficient bow pressure lets the string "slip twice
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

- **Louder goes slightly flat.** Part 48: "the pitch of the note goes slightly flat as it becomes
  louder." Our `morph` is bow speed and does not touch pitch. Measurable with `tune-check.js` swept
  across morph, which would currently show no deviation.
- **Per-period pitch jitter.** Part 48: "there is jitter in the pitch as the 'corner' of the wave ...
  passes under the bow." We have `bw_drift`, a slow random walk, which is a different thing. The
  primitive for the right thing exists: `INSTR_VOICE`'s `vox_jit_mul` is per-glottal-period pitch
  jitter ([`runtime/sound.h:270`](../../runtime/sound.h)).
- **Bow position should comb out harmonics, and this is testable today.** Part 48: bowing at the centre
  removes the even harmonics; at 1/3 from the bridge "there can be no third, sixth, ninth, and other
  'third' harmonics"; at 1/4, no fourth/eighth/twelfth. Our `harmonics` macro *is* bow position
  (`bw_nutlen`/`bw_brlen` split at note-on), so this comb should already fall out of the geometry. It
  is a free correctness check on the engine's physics, and a good one because a pass proves the
  waveguide is right where a listening test can't.
- **Audible home:** `bowed`; the comb check is a `brasspec`-style measurement.

### F7. Part 51 is not about DSP at all, and it is the finding with the widest reach

> **✅ BOTH TRICKS BUILT 2026-07-29** (plan item 1.7, awaiting the owner's ear). Key **0** in `martenot`
> cycles filter / morph / gate. The **filter-as-gate** is the one the numbers settle: with `note_vol` pinned
> constant, the touche driving cutoff alone yields **30 dB of range** (−40.4 → −10.0 dBFS), so the filter
> really does differentiate one note from the next. The **waveform morph** is ear-only — the HF/total
> brightness proxy reads 0.000 on this voice and the centroid is fundamental-dominated, so it moves the
> wrong way; no oracle here can adjudicate it. Caveat on the gate: a 12 dB/oct lowpass attenuates rather
> than mutes, so Reid's "at low cutoff nothing passes" is approached, not reproduced. Full numbers:
> [plan §4 "1.7"](synth-secrets-plan.md).

- **Book:** Part 51 abandons patch-building and argues that **control** beats components. Reid drives a
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

- **Amp level should key-track *negatively* for warmth.** Part 47's Korg T2 string patch sets amplifier
  keyboard tracking to **-04**: "With a negative value, this weights the loudness of the sound to the
  bottom end of the keyboard, thus generating additional warmth." We have no amp key-tracking in either
  direction (§B2 covers cutoff; this is a second, independent destination).
- **String patches want zero resonance and real key-follow.** His JX10 string patch: `LPF Resonance 00`,
  `Key Follow 64`, `ENV Amount 14`. That is the **fourth** independent citation for §B2 keytracking,
  after Parts 6, 23, 24 and 26. It is comfortably the most-cited missing feature in the series.
- **No velocity sensitivity.** "String synths were not velocity-sensitive, so this patch should be
  likewise" — worth knowing before anyone wires §B9 globally rather than per-slot.
- **The VCA envelope is a trapezoid**: a crescendo in, a long tail, no filter modulation (Part 46
  Figure 9). Reid's Part 7 trapezoid, reappearing as the string-machine amp shape.
- **Layering is the ensemble.** Part 47: three layers at 16'/8'/4' "to emulate the Cello, Viola and
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

> **Hard evidence arrived from plan item 1.4 (2026-07-28), and it is stronger than this section assumed.**
> §E10 lifted three envelope numbers from one worked Reid brass patch. Built as toggles, level-matched and
> A/B'd, **all three failed on `INSTR_BRASS`** — and the reasons were structurally *different*, which is
> what makes it evidence about the category rather than about one patch:
> 1. the model **already does it** (the bore supplies a ~40 ms onset, so his 100 ms attack double-counts);
> 2. the parameter **isn't what it is on his hardware** (with `decay_ms` 0, "sustain maximum" is just
>    +4.86 dB of gain);
> 3. the value **destroys the model's own behaviour** (a short release truncates the bore's ring-down —
>    measurably clean, audibly *cut off*).
>
> The lesson for this section: a subtractive-imitation engine is not needed because his patches sound
> *different*, but because his patch *parameters have no faithful translation* into a waveguide's controls.
> A translation layer is not the answer; a machine that has an envelope, a filter and a VCA in the first
> place is. Corollary for the rest of the plan: **never port numbers from a Reid patch by editing — always
> A/B**, and expect the physical model to already own whatever the envelope was faking.

Both recipe passes so far have opened with the same caveat: **every patch in Synth Secrets is
subtractive, and all our imitative engines are physical models.** §E says it about brass, §F about
bowed strings. That caveat has been treated as a translation problem. It is worth asking whether it is
actually pointing at a missing *category*.

**The observation.** Reid's brass patch does not sound like a trumpet. It sounds like **a Minimoog
playing a trumpet**, and that is a beloved sound in its own right — the entire 1970s prog and funk horn
vocabulary, plus the string-machine sound that Parts 46-47 spend two months on and that people still
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
   patches from Parts 46-47). The header owns the recipe, the cart owns the performance. This is
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
The strict answer is 'yes'" (Part 42). In all three he points at physical modelling or samples, which is
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

## I. Recipe pass 4 — PIANOS (Parts 42-45)

STATUS: EXPLORING — nothing queued.

Four chapters: Part 42 is the physics, Parts 43-45 build a subtractive piano on a Roland JX10 (with
a hard-sync tutorial in the middle, because the patch needs it). **This is the best-matched engine in
the whole audit.** `INSTR_PIANO` gets a subtle piece of physics right that it could easily have got
wrong, and most of what Part 42 asks for is already shipped, largely thanks to
[`piano-engine.md`](piano-engine.md)'s fix round. So §I is mostly confirmations, with a short list of
real gaps and one genuinely counter-intuitive finding.

Sources: Part 42 "Synthesizing Pianos" (SOS October 2002), Parts 43-45 "Synthesizing Acoustic Pianos On
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
  finding I most expected to go the other way. Part 42 draws the distinction explicitly: "Whereas the
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
  ([`:4604`](../../runtime/sound.h)). Part 42 explains both the cause (the string needs a finite length
  to bend over bridge and nut, so its effective length shortens, stretching the series toward
  1:2:3:4.01:5.02:6.04) and the consequence, which is the good bit: play A440 with the A two octaves
  up and "because A440 exhibits stretched harmonics, the upper 'A' will, if tuned to 1760Hz, sound a
  fraction flat! Indeed, the human ear/brain is so accustomed to this that a perfectly tuned piano not
  only sounds out of tune, **it sounds dull**." His fix is Figure 14: "making the oscillators track the
  keyboard at a ratio just a fraction greater than 1:1", which is what we do.
- **Register-dependent decay is free, and measured.** Part 42 Figure 9 wants low notes to decay more
  slowly than high ones. Because the loop applies a fixed per-*period* loss, higher notes take more
  loop passes per second, so T60 falls with pitch structurally: I0 shows A1 still at 0.11 after a
  second while A4 is gone. Nothing to build. Worth recording precisely so nobody adds a redundant
  register term on top of it.
- **Two-rate decay, and Part 42 supplies the piano's own cause.** `pn_dd`
  ([`:4629`](../../runtime/sound.h), relaxing at `0.99975` per sample ≈ 90 ms). Part 42: "the tail can
  linger for tens of seconds, which tells us that the rate of the decay diminishes as the note
  progresses. This is because, **as the pairs and tricords interact**, the rate at which energy is
  transferred to the soundboard diminishes." Note this is a *different* mechanism from the guitar's
  (Part 28 attributes the guitar's two-rate decay to the string's two polarisation planes). Both
  instruments have the behaviour, for different reasons — which is exactly why §H3's correction to
  `pn_dd`'s comment matters: the comment claims two-rate decay is what says "struck, not plucked", and
  it isn't.
- **We attempt the stage Reid says nobody has managed.** Part 42 splits a piano note into three
  stages: the hammer blow, "the transition period during which the strings begin to oscillate
  harmonically", and the tail. On stage 2: "it is here that the nature of the waveforms is changing
  most rapidly. I suppose it's possible that we could invent a synth architecture to imitate this, but
  **I know of nobody who has succeeded**." Our `pn_ksb_cur` brightness bloom (τ ≈ 283 ms,
  [`:4706`](../../runtime/sound.h)) is a stage-2 model, and a waveguide gets much of the rest
  structurally, because redistribution among string modes is what the delay line *does*. Reid was
  writing about subtractive synthesis, so this is not a contradiction — but it is a case where our
  choice of method buys something he explicitly could not have.
- **Velocity drives timbre, not just level.** Part 42 Figures 11-13 want brightness to respond to note
  number, to velocity, and to a contour whose decay rate itself depends on note number. We do
  velocity → hammer hardness ([`:4624`](../../runtime/sound.h)) and velocity → knock amount
  ([`:4634`](../../runtime/sound.h)), plus the register-scaled bloom.
- **Soundboard, sympathetic resonance and pedal.** `pn_body[4]`, `pn_symp`, `pn_dampg` on morph. Part
  41's sustain-pedal section is a description of sympathetic resonance: "the energy then passes through
  the bridge and soundboard to excite other strings. Some will vibrate sympathetically, because they
  share modes of vibration with the initial note."
- **Our macro assignment is validated from an unexpected direction.** Part 45, on programming the JX10:
  aftertouch must be zero everywhere because "it's **not possible** to affect the nature of a piano
  note (other than to curtail it) once it has sounded. Any parameters that let you change the
  brightness, the loudness, or add vibrato by bearing down on a depressed key must be set to zero." Our
  PIANO fixes hammer/voicing at note-on and gives the one axis that *can* legitimately change mid-note
  — the pedal — to `morph`. That is the physically correct split, and it was chosen for the
  three-macro discipline rather than for this reason.

### I2. Hammer position is per-voicing, where the book says per-register

- **Book:** Part 42: "you would think that, to obtain a consistent tone, piano builders would position
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

- **Book:** Part 42 lays out the register progression: "At the bottom end, single strings are wrapped to
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

> ⚠ **MEASURED 2026-07-29 — this understates it: there is no inharmonicity to respond.** Item 2.3's
> "prototype on PIANO, which already has the machinery" does not hold. `tools/inharm-spec.js` (built for
> this, `--check` green) measures **B ≈ 2e-6** on the grand where a real one is ~1e-4, with h16 at **+0.2¢**
> instead of ~+22¢ — the partials are harmonic to within the measurement. `pt = B·(i+1)·freq/SR` comes out
> at 2.6e-6 at C3, so the dispersion allpass coefficient is 0.9999948, i.e. the identity. GUITAR
> (−3.1e-7) and PLUCK (1.6e-8) too. **Step 1 of the fix is measured (2026-07-30):** the 4-stage cascade
> IS capable of a real grand's B = 1e-4 at every pitch C2–C6 for 3–7% of the delay line, so this is
> tractable — but the coefficient needs the **opposite SIGN** (a positive `c` flattens partials; stiffness
> needs `c < 0`, which the `pt ≤ 0.9` clamp makes unreachable) and the **opposite pitch dependence**. A second, independent defect found alongside it: the
> `piano_stretch_freq` seam works in the **treble only** — `v->freq` is written back but
> `v->freq_target` is not, so the glide slew undoes it, except that an `effLen > len` clamp happens to
> block the undo in the sharp direction. So PIANO has been playing **half a Railsback curve**: correct
> treble stretch, no bass stretch. **§I4c is FIXED (2026-07-30)** — one line, `v->freq_target = freq` — and
> `tune-check` now models PIANO's intended curve and gates on the *residual* against it, because the ET
> reading was inside tolerance whether the stretch worked, half-worked or did nothing. §I4b (dispersion)
> stays open as DESIGN. And a third, smaller one, §I4d, still open: with no stretch at all the loop runs
> +1.3→+4.0¢ sharp, its own uncompensated delay bookkeeping. Full write-up and evidence:
> [`synth-secrets-plan.md` §2.3(a)](synth-secrets-plan.md#the-premise-failed-three-defects-found-by-measuring-first-2026-07-29).

- **Book:** Part 42 and Part 28 both say the stretching depends on *two* things: "the string appears
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

Part 42 Figure 9 shows high notes with **both** a shorter decay and a lower maximum gain, and the
architecture in Figure 10 routes keyboard tracking to "both the maximum gain of a VCA and the decay
rate of the contour that shapes it". We get the decay for free (I1) but nothing tapers the peak, so
our treble arrives at full level and then vanishes. Small, and it interacts with I5 — fixing the decay
without the level taper may be the wrong half.

### I7. The bottom octave's fundamental should be WEAK, and our sub-oscillator pushes the other way

The counter-intuitive one, and it cuts against a shipped fix.

- **Book:** Part 42: "for the lowest notes on a grand piano, **the fundamental pitch has very low
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

- **Two bridges.** Part 42: pianos "generally have two of them — one for the treble strings, and one
  for the bass — and … they are coupled through the soundboard", and "using different bridges can
  change the sound of a piano by a remarkable degree". We have one body per voicing with no bass/treble
  split. Low priority; listed for completeness.
- **Soundboard modes are enharmonic and the plate is irregular.** Part 42: "piano soundboards have an
  irregular shape and are chamfered, so our previous discussions of vibrations in flat plates are, at
  best, approximations". Our `pn_body[4]` is four biquads, which is the standard coarse approximation
  and the same order as `gt_body[4]`.
- **Our hard sync is always ratio-locked; the JX10's is not.** Part 43's two rules: the output pitch is
  always the master's, and if the master is *lower* than the slave then moving the slave changes timbre.
  Then the aside that matters: with only the master tracking the keyboard, "the frequency relationship
  between the master and slave is different for each note, resulting in **different tones for each**".
  Our `sync_ratio` ([`runtime/sound.h:168`](../../runtime/sound.h)) is a ratio, so the slave tracks and
  the timbre is constant across the keyboard. There is no way to pin the slave in Hz. This is the exact
  mirror of §B2: there, nothing tracks when it should; here, something always tracks when you might not
  want it to.

### I9. Layering is the piano, and §G should encode it

Part 45's conclusion is the most transferable thing in the arc, and it is a *cart* pattern rather than
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

Part 42 opens with it: "there has never been a convincing acoustic piano produced by subtractive
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

## J. Recipe pass 5 — DRUMS AND PERCUSSION (Parts 31-41)

STATUS: EXPLORING — nothing queued.

Eleven chapters, the longest arc in the series, and the only family where we ship **both** a physical
engine (`INSTR_MEMBRANE`) and faithful *machine* recipes (`tr808.h`, `tr909.h`, `morphdrum.h`,
`drumkit.h`), so there are two independent things to check against each other.

Sources: Part 31 "…pitched drums" / timpani physics (SOS November 2001), Part 32 timpani practical
(December 2001), Part 33 unpitched membranophones (January 2002), Part 34 bass drum (February 2002),
Parts 35-36 snare theory and synthesis (March, April 2002), Part 37 "Analysing Metallic Percussion"
(May 2002), Part 38 cymbal analysis (June 2002), Part 39 "Practical Cymbal Synthesis" — the TR-808 and
TR-909 dissection (July 2002), Part 40 "Synthesizing Bells" (August 2002), Part 41 "Synthesizing
Cowbells & Claves" (September 2002).

**Read fully:** 31 (membrane physics), 37 (metallic physics), 39 (the machine dissection). **Targeted:**
32, 34, 35, 36, 38, 40, 41. Parts 32 and 39 had already been mined by grep for §C4 and §A5 before this
pass; §A5's citation was wrong and is now fixed.

### J1. What matches, and one exact numeric hit

- **Our membrane mode ratios are Reid's Table 1, to three decimals.** `RD[6]` in
  `sound_membrane_sample` ([`runtime/sound.h:3441`](../../runtime/sound.h)), labelled "navkit Bessel":

  | our `RD` | 1.0 | 1.594 | 2.136 | 2.296 | 2.653 | 2.918 |
  |---|---|---|---|---|---|---|
  | Reid Table 1 | 1.00 (0,1) | 1.59 (1,1) | 2.14 (2,1) | 2.30 (0,2) | 2.65 (3,1) | 2.92 (1,2) |

  Six for six. The engine really is on circular-membrane Bessel ratios and the mode identities are
  recoverable: indices **0 and 3 are the circular modes** (0,1 and 0,2), indices **1, 2 and 4 are radial**
  (1,1, 2,1, 3,1), and index 5 (1,2) is mixed. That mapping matters for J4.
- **The snare's two head modes match.** Part 35 gives "approximately 180Hz and 330Hz"; `tr808.h`'s own
  comment reads "snare = 180+330Hz modes + noise". Independently arrived at, same numbers.
- **The 808's six-oscillator metal bank is real and we model it.** Part 39: "The initial sound generator
  comprises six square-wave oscillators tuned **enharmonically**, and mixed to create a complex
  spectrum. If you remove all the low harmonics from the mix, this produces a moderately dense cluster
  of partials in the mid and high frequencies." That is `tr808.h`'s bank exactly, down to the approach.
- **The membrane bend has a physical basis.** Our morph is the pitch chirp settling over ~90 ms (the
  tabla *bayan* gliss). Part 31's fourth factor is membrane stiffness making the head "appear slightly
  smaller at higher frequencies (and higher amplitudes)", and Part 33 covers the pitch drop as tension
  redistributes. A one-sine 808 kick cannot bend six modes together; we can.
- **`MEMBRANE` exists at all, which the book says analogue could not do.** Part 33's whole argument is
  that unpitched membranophones need mode counts subtractive synthesis cannot reach.

### J2. The "tuned" end of the membrane ratio crossfade is a harmonic series, and no drum has one

This is §J's headline, and it is a specific, sourced, cheap fix.

- **Engine:** `harmonics` crossfades between `RD` (Bessel, above) and
  `RT[6] = { 1, 2, 3, 4, 5, 6 }` ([`runtime/sound.h:3440`](../../runtime/sound.h)) — an *integer
  harmonic* series, described in the comment as "tuned head (tabla — its loaded skin pulls the modes to
  a near-HARMONIC series, a pitched drum)".
- **Book:** Part 31 explains what actually makes a drum pitched, and it is **not** an integer series. Air
  loading raises the **radial** modes toward quasi-harmonic ratios, and he tabulates the measured result
  (Table 2, relative to the principal):

  | mode | ideal, rel. principal | **measured in air** | shift |
  |---|---|---|---|
  | 1,1 | 1.00 | **1.00** | 0 |
  | 2,1 | 1.34 | **1.47** | +10% |
  | 3,1 | 1.66 | **1.91** | +15% |
  | 4,1 | 1.98 | **2.36** | +19% |

  So a real pitched drum's series is roughly **1 : 1.5 : 2 : 2.5**, not 1 : 2 : 3 : 4. And he is explicit
  that it stops short of harmonic: "They're still too flat to fool the ear into thinking that it's
  listening to a true harmonic oscillator, but the sound nevertheless conveys a strong sense of tonality
  without ever quite sounding like a pure tone."
- **Why it matters:** our `harm = 0` end is a string/pipe spectrum. It will read as *pitched* but not as
  a *drum* — it is the sound of a struck harmonic oscillator, which is a mallet, not a tabla. The
  physically honest target sits between our two endpoints and Reid supplies the numbers.
- **Also, the principal is not the fundamental.** "the 1,1 mode was producing the principal frequency …
  the principal is not the fundamental, which lies at approximately 63 percent of the principal
  frequency." So on a real timpani the *perceived pitch* is our mode index 1, at 1.59×, not index 0. If a
  cart plays MIDI note N expecting to hear N, our membrane is currently a fifth-and-a-bit out from what a
  timpanist would call the note. Worth checking `tune-check` against a real timpani expectation.
- **Audible home:** `tabla`, `handpan`, `gamelan`, and the toms in `tr808`/`tr909`.

### J3. Our loudest mode is the one a real membrane suppresses

- **Book:** Part 31's fifth factor: "stretched membranes don't like to wrinkle, so they are resistant to
  the vibrational modes that attempt to make them do so (**the 0,1 mode is one of these**)". That is why
  the principal ends up being 1,1 rather than the fundamental.
- **Engine:** `BASE[6] = { 1.0f, 0.60f, 0.45f, 0.32f, 0.22f, 0.13f }`
  ([`runtime/sound.h:3418`](../../runtime/sound.h)) — a monotonic falloff putting the **most** energy in
  index 0, the 0,1 mode, the one the real instrument resists most.
- **Fix shape:** one array. Weighting index 1 (the 1,1 principal) above index 0 would make the engine
  pitched-sounding for a physical reason rather than by crossfading toward a harmonic series (J2). The two
  items are the same fix approached from opposite ends and should be done together.
- **Audible home:** `tabla`, `handpan`.

### J4. Strike position ignores the distinction a timpanist actually uses

- **Book:** Part 31 describes the technique precisely: pitched playing comes from "striking the membrane
  almost precisely a quarter the way from the edge to the centre. In doing so, the timpanist **suppresses
  the circular modes**, ensuring that the quasi-harmonic radial modes dominate the sound." So strike
  position is not a brightness tilt; it *selects which mode family speaks*, and that is the difference
  between a pitched note and a thump.
- **Engine:** `pos = (1 - timb)·(1/(m+1)) + timb·m·0.15` for `m > 0`, with **mode 0 pinned at 1.0**
  ([`runtime/sound.h:3469-3470`](../../runtime/sound.h), commented "the fundamental, mode 0, is
  unaffected — it's there wherever you strike"). Two consequences: it is a generic low-to-high tilt that
  does not distinguish circular (indices 0, 3) from radial (1, 2, 4), and the one mode a timpanist most
  wants *out* of the way can never be reduced.
- **Fix shape:** weight by mode *family* rather than by index. We already know which is which (J1), so
  this is a lookup table and a lerp, and it makes `timbre` mean "centre thump ↔ pitched timpani stroke"
  instead of "dull ↔ bright".
- **Audible home:** `tabla` (its strike-position control is the demo).

### J5. The 808 cymbal's three unequal decays are missing, so our cymbal's spectrum never moves

> **✅ FIXED 2026-07-28 — three bands is now the DEFAULT** (plan item 1.2; the owner's ear picked it over
> the stock voice, which stays reachable on key **C**). `runtime/tr808.h`, runtime-toggled by
> `tr808_cym3`. Measured: the centroid now walks **14895 → 11844 Hz** over
> the first 200ms and then converges bit-exactly onto the stock tail, where the one-band version was flat
> to within 1.5% for the whole ring. Two corrections to what this section assumed, both from measurement:
> the upper bands must be **band-pass** (a highpass at 7800 on `INSTR_SQUARE` amplifies *aliasing* — it
> stem-measured −0.0 dBFS with a centroid of 21942 Hz against Nyquist 22050), and the real 808's upper
> corner (**7100 Hz**) was already recorded in `tr808.c`'s own docblock and had simply never been built.
> Full write-up incl. the level-matching dead end: [plan §4 "1.2"](synth-secrets-plan.md).

- **Book:** Part 39 gives the full schematic, and the important part is the *decay structure*. The six
  mixed oscillators are "split into two bands by a pair of band-pass filters". The lower band gets a VCA
  and AR contour, and "the TR808's Decay control affects the decay rate of this envelope". Then: "The
  upper band is further split into two signal paths that pass through independent VCAs controlled by
  their own contour generators. The upper of the two … has the shortest Decay. The lower of the upper
  bands has a Decay that lies somewhere near the centre of the range of the low band's variable AR
  control." And the payoff sentence: **"This inequality of decay times allows the TR808 to change the mix
  of lower-, mid- and higher-frequency components as the sound progresses."** Three bands, three
  different decay rates, three highpasses, then "a user-controlled mixer recombines them" — which is what
  the TR-808's tone knob is.
- **Engine:** `tr808.h` fires three bank voices (MIDI 79, 72, 66) into **one** slot with **one** highpass
  at 3440 Hz and **one** decay ([`runtime/tr808.h:111-113`](../../runtime/tr808.h) and
  [`:174-176`](../../runtime/tr808.h)). So the spectrum decays uniformly and the timbre is static across
  the ring.
- **Why this is the best item in §J:** Part 37 independently says spectral migration is what a real
  cymbal *does* — "The high-frequency modes are the shortest-lived … the last of the cymbal's energy lies
  in the modes at a few hundred Hertz, and — at the end — it's the mid-frequencies that again dominate."
  The 808's three-decay trick is how the machine fakes it, and we left the trick out. **And it needs no
  engine change**: three slots at different decays, three highpass corners, mixed. That is a `tr808.h`
  edit, which fits the "no engine change without a cart to hear it" rule perfectly.
- **Audible home:** `tr808` (its CYMBAL decay and TONE knobs become meaningful), `cr78`, `tr606`.

### J6. The 909's metal is a ROM sample, we already know it, and Reid adds one gotcha worth keeping

- **Book:** Part 39 is blunt: "Forget analogue FM synthesis, dynamic band-pass filters, and all the other
  paraphernalia I employed to try to recreate the cymbal's complex spectrum — the TR909 dispenses with
  all of this by incorporating a **digital sample** of a genuine cymbal." Its 6-bit ROM table runs to a
  30 kHz clock, then an analogue VCA and lowpass.
- **Engine:** `tr909.h` calls its hats/crash/ride "ROM stand-ins … FM-clang metal"
  ([`runtime/tr909.h:5`](../../runtime/tr909.h), [`:96`](../../runtime/tr909.h)) — so this is a documented,
  deliberate substitution, not a drift. Recorded because it means "faithful 909 metal" is unreachable by
  synthesis *by construction*, and we do ship `INSTR_SAMPLE` plus sample slots if that ever matters.
- **The gotcha, if anyone ever does swap in samples:** the 909 derives the amplitude envelope from the
  sample's **address counter** (through a DAC and an antilog converter) rather than a free-running AR,
  specifically so that tuning the cymbal cannot truncate it: "if a conventional AR contour generator
  proceeds at the same rate, no matter what the speed of the digital clock, it's quite possible that the
  end of the samples will occur before the VCA is fully closed … So Roland made the decay rate of the AR
  envelope dependent upon the progress of the address counter." Pitch and envelope must stay locked. Our
  `INSTR_SAMPLE` plays at `freq/smp_root` with the amp ADSR running independently, so a transposed sample
  would hit exactly the bug Roland engineered around.
- **Audible home:** `tr909`, `sampler`.

### J7. Cymbals have hundreds of modes, which is the case for §C4

Part 37: "There can be **hundreds** of energised modes in an excited cymbal." And the modes bear no
usable relationship to flat-plate theory — his Table 1 compares a calculated flat plate against a
measured domed cymbal at the same 50 Hz fundamental and the deviations run **+3% to +112%**, non-monotonic
in mode index. So a modal engine is the wrong tool at any realistic mode count, which is exactly why
§C4's ring-mod-with-a-rich-carrier matters: it is the cheap way to a *dense* inharmonic cloud. §J and §C4
are the same recommendation reached from the physics and from the synthesis side.

### J8. Amplitude-dependent mode sharpening, for the third time

Part 31's fourth factor: the membrane "appears slightly smaller at higher frequencies (**and higher
amplitudes**) than at lower ones, thus sharpening the partials produced by the higher modes." That is the
same claim as §I4 (piano) and §H's stretched-harmonics note (guitar), now for drums. Three families, one
missing behaviour: **our inharmonicity is never a function of playing level.** `MEMBRANE`'s ratios come
from a static table with no velocity term at all. Worth treating as one cross-engine item.

### J9. The snare's tone-to-noise balance should move with velocity and over the note

> **✅ FIXED 2026-07-28 — velocity-dependent is now the DEFAULT on both machines** (plan item 1.3; the
> owner's ear picked it, and the fixed-balance original stays on key **N**). Key **N** in both `tr808` and `tr909`;
> `tr808_snare_dyn` / `tr909_snare_dyn`. An accented hit now tips body→noise instead of just getting
> louder: +37% noise share and −1.7 dB peak on one accented hit. **The second half of this finding was
> already true and I was wrong to list it as missing** — the noise layer outlives the body in both machines
> (130/100ms and 170/90ms), so a single hit's centroid already climbs 10890 → 12279 Hz across its own
> decay. Only the velocity half needed building. Note per-hit *decay* scaling is impossible here: with
> `sustain 0` the slot's `decay_ms` owns the ring, so the gate length cannot shorten it.

- **Book:** Part 35 gives two dynamics that our fixed layering doesn't express: harder strikes make "the
  spectrum become more noise-like", and over the course of a note the sound evolves toward noise,
  "eventually changing into a complex noise". So the tone/noise ratio is a function of both velocity and
  time.
- **Engine:** `tr808.h` layers the 180/330 Hz modes with noise at fixed relative levels per hit; velocity
  scales the whole voice. So a soft hit and a hard hit have the same tone-to-noise balance, and the
  balance is constant while the note rings.
- **Fix shape:** cart-side in `tr808.h`/`tr909.h` — scale the noise layer's level and decay against
  velocity. No engine change.
- **Audible home:** `tr808`, `tr909`, `fingerdrums`.

### J10. Smaller items

- **The 808 kick is a Bridged T-Network,** and Part 34 dissects it: a self-oscillating filter ringing at
  close to a sine, "quite unlike any [other]" kick circuit. `tr808.h`'s header already cites the
  reverse-engineered circuit values and the ~50 Hz ring, so this looks covered; a closer read of Part 34
  against `tr808_fire`'s kick path is the obvious follow-up if the kick is ever revisited.
- **Bells and cowbells (Parts 40-41) were only skimmed.** Part 40 builds on the cymbal patch for tuned
  bells; Part 41 covers cowbells and claves. We have `MALLET` (four modes, bar ratios) and the 808
  cowbell (`TRS_CB`, a bandpass at 2640 Hz), so there are two specific things to check and I did not
  check them. Flagged rather than claimed.
- **Timpani practical (Part 32)** was mined for §C4's ring-mod recipe and not otherwise read.

### Suggested drums step order

| # | Step | Kind | Where |
|---|---|---|---|
| 1 | ✅ J5 three-band unequal-decay 808 cymbal — **SHIPPED as the default 2026-07-28** | `tr808.h` only, no engine change | `tr808` |
| 2 | ✅ J9 velocity → snare tone/noise balance — **BUILT 2026-07-28**, awaiting the ear | `tr808.h`/`tr909.h` only | `tr808` + `tr909` |
| 3 | J2 + J3 retarget the "tuned" ratios and mode weights together | engine, two arrays | `tabla` |
| 4 | J4 strike position by mode *family* | engine, small | `tabla` |
| 5 | J2b check the principal-vs-fundamental pitch offset | measurement | `tabla` |
| 6 | J8 level-dependent inharmonicity (with §I4) | engine, cross-family | `tabla`, `piano` |
| 7 | J10 read Parts 34, 40, 41 properly against the kick, `MALLET`, the cowbell | reading | — |

---

## K. Recipe pass 6 — FLUTES AND PAN PIPES (Parts 52-54)

STATUS: EXPLORING — nothing queued.

Three chapters against `INSTR_PIPE`, and the reason this was ranked next despite being the shortest arc
left: `studio.h` already carries a **documented open question** about this engine's tuning, and Parts
52-54 turn out to explain it, correct it, and tell us what real instruments do about it.

Sources: Part 52 "Synthesizing Pan Pipes" (SOS August 2003), Part 53 the recorder (September 2003),
Part 54 the orchestral flute (October 2003). Engine: `sound_pipe_sample`
([`runtime/sound.h:3649`](../../runtime/sound.h)).

> **Numbering cross-check:** Part 54 cites "Synth Secrets 49 (see SOS May 2003)" in its own text, and the
> corrected map in §"The source" puts Part 49 at May 2003. The document confirms its own numbering.

### K0. Measurements taken

`tune-check.js` in recipe mode, which exists for exactly this engine
(`--engine PIPE --macros h,t,m --range lo-hi`). Errors in cents, `harm` and `timb` held at the showcase
flute's values where noted.

**Embouchure (morph) versus intonation,** `harm 0, timb 0.38`:

| morph | C4 | F#4 | C5 | F#5 | C6 |
|---|---|---|---|---|---|
| **0.70** (focused) | +0.7¢ | +0.7¢ | +1.0¢ | +0.0¢ | −1.5¢ |
| **0.40** | — | — | **+13.4¢** | **+16.7¢** | +10.4¢ |
| **0.20** (hollow) | — | — | −5.6¢ | **−19.2¢** | **−330¢** |

**Overblow (harmonics) versus pitch,** `timb 0.38, morph 0.70`:

| harm | C4 | F#4 | C5 |
|---|---|---|---|
| 0.5 | −8.5¢ | −13.2¢ | −18.1¢ |
| **1.0** | −11.1¢ | −19.4¢ | **−26.2¢** |

### K1. What matches, including a second place we beat the hardware

- **Our breath noise is bore-coloured, which is the hard part of a flute patch.** Part 52 is emphatic
  that a plain noise generator will not do: "the tonal part of the sound and the noise are not
  independent of one another. The noise has a breathy quality with a distinct pitch related to the note
  being played … The turbulence occurs within the pipe and at its boundaries, so it must be **coloured by
  the acoustics of the pipe itself**." Mixing in raw noise "proves no more satisfying … The square wave
  and the noise seem to be disassociated from one another, and this sounds wrong." His fix needs a formant
  bank tuned to the note's harmonics — ideally 40 bandpasses, "fortunately … just six bands on the edge of
  self-oscillation, tuned to octaves and fifths" will do. We inject noise into the *breath* term
  ([`runtime/sound.h:3669`](../../runtime/sound.h)) so it circulates through the bore and is shaped by it
  for free. **Second time the audit has found this** — §E1 recorded the same win for brass, where Reid has
  to omit noise entirely on both the Minimoog and the SH-101. Injecting noise into the excitation rather
  than the output is quietly one of the best decisions in the engine.
- **The chiff exists, and Reid rates it the single most important cue.** "The first thing we hear is a
  noisy 'chiff' that sounds independent of the tone and the breathy noise … Skilled pan pipe players make
  great use of this, and it is perhaps **the most defining characteristic of the instrument**." We have
  `pp_attack`, "the tongued 'tu' onset" ([`runtime/sound.h:3668`](../../runtime/sound.h)).
- **Vibrato lives in pitch, and at about the right rate.** `pp_vib_ph` runs at 5.0 Hz with a wander,
  commented "a flute's vibrato is pitch, not amp".
- **Dark tone, bright noise.** Part 52: "the tonal part of the sound is not rich in high-frequency
  harmonics. In contrast, the noise is most audible at higher frequencies … strong, low harmonics
  accompanied by a halo of noise". Our bore's open-end radiation lowpass plus a separately-scaled noise
  term gives that shape.
- **The engine exists at all.** Part 53 on the recorder: "a simple VCO/VCF/VCA patch will never capture
  the nuances of the instrument", and he finds *no* factory recorder patch in the books for the Odyssey,
  Axxe, SH-101, Korg 700/700S/800DV or MS20.

### K2. The `harmonics` macro does not overblow, and the documentation says it does

- **Documented as:** "harmonics = overblow (fundamental → **octave flageolet** + bright)"
  ([`runtime/studio.h:326`](../../runtime/studio.h)).
- **Measured (K0):** at `harm 1.0` the pitch stays on the fundamental and goes progressively *flat*
  (−11¢ at C4 worsening to −26¢ at C5). There is no register jump at any macro value I tested.
- **What it actually does:** `gain = 2.0f + v->harm * 8.0f` feeds `tanhf(jetOut * gain)`
  ([`runtime/sound.h:3653`](../../runtime/sound.h), [`:3690`](../../runtime/sound.h)) — it drives the jet
  nonlinearity harder, which adds harmonic content and pulls the oscillation flat. So it is a
  **brightness-and-flatten** control, not an overblow.
- **What the book says overblowing is:** a genuine register jump, and Part 54 is precise about the
  interval depending on bore topology. A flute: "you will eventually 'overblow' the flute and cause its
  pitch to **jump an octave**." A cylindrical pipe with one closed end: "cylindrical pipes with one closed
  end have no even harmonics, so overblowing jumps to the **third harmonic, one-and-a-half octaves** above
  the fundamental."
- **Care on what I am claiming:** the *sound* may well read as "blown harder" — more harmonic content is
  genuinely what harder blowing does (K5). The finding is narrower and firmer: the pitch behaviour does
  not match either the macro's documentation or the physics, and the doc promises a flageolet the engine
  cannot produce. Either the implementation or the docstring should move.
- **Audible home:** `pipe`, `air`.

### K3. The tuning caveat is real physics, the doc has its direction wrong, and real flutes compensate

This is the item the pass was run for.

- **What `studio.h` says today:** "intonation tracks the morph (embouchure) macro — in tune for focused
  embouchure (morph ≳ 0.5) … but a low/hollow embouchure (morph ≲ 0.4) and overblow (harmonics) drift
  **flat**/unstable at the top."
- **Measured (K0):** the focused end is confirmed excellent (worst −1.5¢ across two octaves). But at
  `morph 0.40` the drift is **sharp**, +13 to +17¢, not flat. Only at `morph 0.20` does it go flat
  (−19¢ at F#5) and then collapse at C6 (−330¢, and 1046.5/864.71 is not a clean interval, so that is the
  oscillator failing to lock rather than a register jump). So the drift is **non-monotonic**: sharp
  through the middle of the range, flat and then unstable at the bottom. Anyone voicing a flute at
  `morph 0.4` is currently told to expect flat and will hear sharp.
- **Why the drift exists at all, and this is the satisfying part.** Part 54 gives the mechanism: because
  the embouchure hole sits a short distance from the bore end with a pressure maximum at the cork, "the
  **effective length of the flute increases for higher harmonics!** This is analogous to the 'overshoot'
  I mentioned when we discussed brass instruments, and its effect is to make the harmonic frequencies
  more and more approximate with increasing harmonic number." So a jet-driven pipe whose effective length
  depends on jet geometry *genuinely* drifts, and it drifts worse the higher you play — which is exactly
  the shape of our measurement.
- **And what real instruments do about it:** "**Flute manufacturers try to compensate for this** by making
  tiny adjustments to the position of the cork, the shape of the embouchure chimney, and the sizes and
  positions of the holes." Part 53 adds the player's half: a recorder has several fingerings per nominal
  note, "some of these pitches will lie almost exactly on a desired note, while some will be a little
  sharp, and others will be a little flat … a skilled player will pick the right fingering according to
  the demands of the music."
- **We already do the compensation, and I can see exactly why it still misses.** This is the useful part,
  and it corrects my first reading. `sound_pipe_start` already carries the software cork adjustment
  ([`runtime/sound.h:3605-3619`](../../runtime/sound.h)): the bore is a half wavelength *minus* a
  jet-derived loop-delay term, `loopDelay = 1.69f + 0.308f * jetLen0`, plus a second-stage ramp for long
  jets, `ex = 0.40 * (jetLen0 - 5)` **clamped at 0.80**. The comments record the history: a *constant*
  left `morph ≠ 0` sharp by up to a semitone, and the hollow presets at jetLen 7-8 "ran flat (a ramp to
  ~-56¢ by G5)" until the ramp was added, because they "need a near-CONSTANT ~+0.8 extra (it SATURATES)".

  `jetLen = 3 + (int)((1 - morph) * 8)`, so my three measurements sit at jetLen **5, 7 and 9**, and the
  clamp explains all three. Since `targetBore = SR/(2f) - loopDelay`, over-subtracting shortens the bore
  and reads **sharp**; under-subtracting reads **flat**:

  | morph | jetLen | `ex` | measured | mechanism |
  |---|---|---|---|---|
  | 0.70 | 5 | 0.00 | in tune (−1.5¢ worst) | the calibration point |
  | 0.40 | 7 | **0.80** (clamped) | **+13…+17¢ sharp** | the saturated ramp now over-corrects |
  | 0.20 | 9 | **0.80** (still clamped) | −19¢, then mode collapse | the clamp under-corrects, and the comment already predicts the mode-flip ("needs jet∝bore") |

  So the ramp saturates at jetLen ≥ 7 and is asked to serve both 7 and 9, fitting neither. It is not a
  missing feature, it is **a two-point fit carrying a three-point problem**.
- **Two doc claims need narrowing.** `studio.h` says the drift is *flat*, and at the most plausible
  mid setting it is *sharp*. And [`audio-notes.md`](audio-notes.md) §18 concludes PIPE is "in tune ~±3¢
  from C4 to ~E6 **at any sane embouchure**" — morph 0.40 is certainly sane and measures +17¢. Same class
  of finding as §E9: a recorded number that does not survive re-measurement, which matters most in the
  docs whose job is to be the as-built record.
- **Fix shape:** replace the clamped-linear `ex` with a form that has room for three points (a second
  slope past jetLen 7, or a small table indexed by `jetLen0`), fitted and verified with
  `tune-check --engine PIPE --macros h,t,m`. Cheap, and the tool and the calibration points already exist.
- **Audible home:** `pipe`, `air`, and `pipetune` (which exists for this).

### K4. One engine covers three bore topologies that have different harmonic series

- **Book:** the flute family splits along a physical line Parts 52-54 keep returning to.
  A **pan pipe** is cylindrical and closed at the bottom by a wax plug, so it "can produce **only odd
  harmonics**", sharing its tonality with square and triangle waves (Part 52). A **recorder** is
  effectively open, and its spectrum has "a dominant fundamental, with a handful of weak overtones", with
  "odd and even harmonics … present" — plus the specific quirk that "the recorder's second harmonic is so
  weak" (Part 53). A **flute** is open at both ends and overblows to the octave (Part 54).
- **Engine:** `INSTR_PIPE` is documented "flute/recorder/pan pipe" with one bore model and one reflection
  topology (a single inverting open-end reflection at
  [`runtime/sound.h:3682`](../../runtime/sound.h)). There is no open-versus-closed axis, so the pan-pipe
  preset cannot have an odd-only spectrum, and per §K2 no preset overblows at the correct interval either.
- **Why it may not be worth "fixing" as such:** three macros cannot carry bore topology as a fourth axis,
  and `eng_p[]` exists for precisely this kind of note-on-only structural switch
  ([ADR-0017](../decisions/0017-three-macro-core-plus-engine-aux-channel.md)). A one-bit
  open/closed flag on `eng_p` would give the family its missing dimension cheaply. Worth measuring our
  current spectrum first to see which of the three it is actually closest to.
- **Audible home:** `pipe` (its presets claim all three).

### K5. Blowing harder should brighten, not amplify, and loudness sits on the wrong macro

- **Book:** Part 54, and it is counter-intuitive enough to be worth quoting exactly: "as you blow harder,
  higher harmonics appear and, as their amplitudes increase, the flute's tone becomes increasingly
  complex and more sonorous. **Strangely, the flute does not get much louder when you blow harder, but
  does so when you relax your lips to allow a greater cross-section of air to pass.**" So blowing pressure
  → brightness; lip aperture → loudness. Two separate controls.
- **Engine:** `breath = 0.55f + v->timb * 0.35f` multiplies the excitation
  ([`runtime/sound.h:3657`](../../runtime/sound.h), [`:3690`](../../runtime/sound.h)), so **`timbre`
  ("breath air") is our loudness axis**, while `morph` ("embouchure") controls jet length and mouth-end
  coupling but not level. Relative to the physics the two are close to swapped: the aperture control does
  not set loudness and the air control does.
- **Held lightly:** this is a three-macro compression of a five-parameter instrument and `timbre` is
  already doing double duty (excitation energy *and* noise amount), so there is no clean assignment. But
  it is worth knowing that the loudness axis is not the physical loudness control, especially for anyone
  wiring expression to it.
- **Audible home:** `pipe`, `air`.

### K6. The chiff must fire on every note, which needs multi-triggering

> **✅ SHIPPED 2026-07-30** via `note_retrig(handle)` (plan 2.2 postscript). The chiff, the reed's breathy
> onset and the brass "tah" are `pp_attack` / `rd_attack` / `br_attack` — sample countdowns that are
> *separate* from the resonator, which is exactly the seam this needed: `note_retrig` re-arms those and the
> envelopes on the voice you already hold, and deliberately does NOT re-excite the bore (that would be a
> new breath, i.e. `note_on`). So a cart gliding one held voice can now tongue it. `pipe` and `brass` each
> got a **T** toggle (slur vs tongued legato); brass defaults ON, because before this its mono mode could
> not re-attack *at all* — every note after the first was a slur, permanently. Measured on `retrigprobe`
> with a flat envelope, so the envelope rewind is a provable no-op and only the onset can move the
> spectrum: brightness **0.036** / centroid **4953 Hz** at the retrig vs a 0.016-0.020 / ~3500 Hz sustain
> baseline. Note the analysis-window trap that cost one wrong "the chiff never fired" reading: the probe's
> retrig lands at ~0.98s, not 1.00s, so a 25ms window starting at 1.000 misses it entirely.
> **§L4 is NOT closed by this** — it needs the mirror image (*suppress* a transient when a note is already
> held), which is the per-instrument declaration §L4 argues for.

Part 52, in the list of things the patch requires from its keyboard: "it's important that the keyboard
offers **multi-triggering**. This ensures that the chiff occurs at the start of every note, **even when
you play legato**." That is §B3 again, and note what kind of argument it is: not about feel or
playability, but about whether the instrument's defining transient happens at all. Our `pp_attack` is
armed in `sound_pipe_start`, so it fires per *voice*; a cart doing legato by gliding one held voice
(the `solo.h` pattern) gets no chiff on subsequent notes. That is the right behaviour for a slurred
phrase and the wrong one for a tongued line, and nothing currently lets a cart choose.

### K7. Keyboard tracking, sixth citation, with a value

Part 54, on the patch's lowpass: "The cutoff frequency of the low-pass filter resides — as discussed — at
2kHz, although I have found that **pitch tracking of a few percent** is necessary to ensure that high
notes are reproduced with the correct brightness relative to low notes." Part 52 says the same thing more
loosely: "an attenuated pitch CV to open the filter … brighter as the pitch rises, but not necessarily in
a 1:1 relationship. This is, of course, **variable keyboard tracking**." §B2 has now been independently
requested in Parts 6, 23/24, 26, 46 and 54 — five or six separate chapters depending on how you count —
which makes it comfortably the most-asked-for missing feature in the series.

### K8. Stretched partials, the fifth family in a row

Part 52: because "the wavefront overshoots the end of the pipe by a small distance … higher modes of
vibration become progressively inharmonic". Part 53: "the higher harmonics are 'stretched' sharp of their
mathematical ideal", and on doing anything about it in subtractive synthesis, "there's nothing we can do
about it". Part 54: the effective length grows with harmonic number. That is brass (§E8), piano (§I4),
guitar (§H), drums (§J8) and now flutes — **five families, one physical fact**, and our engines model it
statically at best. It has earned promotion from a per-family footnote to a single cross-engine item.

### Suggested flutes step order

| # | Step | Kind | Where |
|---|---|---|---|
| 1 | K3 narrow two doc claims: `studio.h`'s "flat" (it is sharp at morph 0.4) and audio-notes §18's "any sane embouchure" | docstrings only | none |
| 2 | K2 decide whether `harmonics` overblows or is renamed; fix the docstring either way | docstring, or engine | `pipe` |
| 3 | K3b refit the clamped `ex` ramp so it serves jetLen 5, 7 and 9 | engine, small, tool exists | `pipetune` |
| 4 | K4 measure which of the three bore spectra we actually produce | measurement first | `pipe` |
| 5 | K4b an open/closed bore flag on `eng_p` | engine, small | `pipe` |
| 6 | K5 A/B moving loudness onto the aperture axis | engine, judgement call | `air` |
| 7 | K8 level-dependent inharmonicity, as one cross-engine item (with §I4, §J8) | engine, cross-family | many |

---

## L. Recipe pass 7 — THE HAMMOND (Parts 55-59)

STATUS: EXPLORING — nothing queued.

Five chapters on one instrument, against `INSTR_ORGAN` plus the `leslie()` effect. Like §I, this comes
out strongly matched: the drawbar table is exact, and two things Reid says almost nobody gets right are
things we happen to do. The gaps are small, specific, and cluster around the **percussion**.

Sources: Part 55 "Synthesizing Tonewheel Organs" (SOS November 2003), Part 56 "More On…" (December
2003), Parts 57-59 "Synthesizing The Rest Of The Hammond Organ" I-III (January, February, March 2004).
Engine: `sound_organ_start` ([`runtime/sound.h:3065`](../../runtime/sound.h)), `sound_organ_sample`
([`:3083`](../../runtime/sound.h)), and the Leslie at [`:1521`](../../runtime/sound.h).

### L1. What matches, including two things Reid says emulators get wrong

- **The drawbar ratios are exact, nine for nine, in the awkward panel order.** Part 55 tabulates the
  drawbars against harmonic number relative to the **16'**, which is the true fundamental: 16'=1,
  5⅓'=3, 8'=2, 4'=4, 2⅔'=6, 2'=8, 1⅗'=10, 1⅓'=12, 1'=16. Divide by 2 to reference the played 8' unison
  and you get **0.5, 1.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0** — which is `RAT[9]`
  ([`runtime/sound.h:3086`](../../runtime/sound.h)) character for character, including the 16'/5⅓'/8'
  ordering that catches people out. Note the series is *not* harmonics 1-9: it skips 5, 7, 9, 11, 13,
  14, 15, and we skip them too.
- **"ORGAN reads an octave low" is correct behaviour, and Reid explains why.** Part 55: "despite
  Hammond's strange decision to call the 8' the fundamental (or 'Unison') and the 16' drawbar the
  sub-octave, **the 16' pitch is the fundamental** of a series that includes the first, second, third,
  fourth, sixth, eighth, 10th, 12th and 16th harmonics." So a registration leaning on the 16' *sounds*
  an octave down while being in tune, which is exactly how [`audio-notes.md`](audio-notes.md) §18
  classified it ("transposed, not detuned"). Confirmed from the physics rather than assumed.
- **Two of Reid's four named registrations are in our snapped table exactly.** `REG[8][9]`
  ([`runtime/sound.h:3089`](../../runtime/sound.h)) row 3 is `1,1,1,0,0,0,0,0,0` = **88 8000 000**, which
  Reid singles out as "one of the simplest but most important of these, beloved of Jimmy Smith, Keith
  Emerson, and heavy rock players the world over … you will immediately recognise its punchy timbre" —
  and our row is labelled "jimmy smith — fat jazz B3". Row 8 is all-ones = **88 8888 888**, his "all nine
  harmonics present at maximum amplitude, and is very full and bright", ours labelled "gospel full".
- **The scanner chorus is right in the way Reid says almost nothing is right.** Part 57 explains the two
  mode families: on a **V** setting "all of the audio is routed through the scanner, and the signal
  suffers unadulterated pitch modulation"; on a **C** setting "the output from the scanner unit is
  **mixed with the unaffected output**", and that is chorus. Then the sting: "This is the key to the best
  Hammond sounds yet, despite its apparent simplicity, **only a couple of Hammond emulators manage to
  get it right**", because "The Hammond chorus mixes the straight-through signal with **just a single
  instance** of the pitch-modulated signal, so Roland's three-stage chorus/ensemble is far too lush." Our
  comment reads "the scanner CHORUS (C-mode, dry+wet) deepens with morph" and it is a single delay-line
  instance, not a multi-stage chorus. We are in the couple.
- **The scanner mechanism and rate both match.** Reid: "a tapped delay line which, if we look closely at
  the electronics, is a type of phase-shifter constructed from low-pass filters", swept by "a rotating
  pickup **driven by the tonewheel generator**". Ours is a short delay line with `org_scan_ph` at 6.9 Hz,
  commented "gear-locked to the motor" — and Reid's own ear-matched figure on the Juno is "six and a bit".
- **All the drawbars bend together, which is the failure mode he warns about.** Part 57, on faking the
  scanner with an LFO: "try to ensure that identical amounts of modulation are applied to the DCO and the
  VCF. **If you don't, the 16' and 8' pitches will deviate more (or less) than the 5 2/3' pitch, which
  leads to some very unconvincing effects.**" Our nine drawbars all derive from one `freq * pitch_mul`
  per sample (§8.8.1), so that desync is impossible by construction.
- **The Leslie matches on every point he makes.** Part 58-59: a treble unit "above 800Hz" and a bass unit
  below, and "The rotor's chorale speed was different from the horn's, as was its tremolo speed, **and the
  transition rates**." Our `leslie()` is a navkit Leslie 122 port with "a 1-pole crossover at **800 Hz**",
  a drum band with gentle sine AM, a horn band with shaped AM plus delay-line Doppler, and "two rotors
  [spinning] at INDEPENDENT rates with asymmetric spin-up/down inertia (horn light/fast, drum
  heavy/slow)" ([`runtime/sound.h:1521-1528`](../../runtime/sound.h)). Three for three, including the
  inertia, on the effect Reid spends a chapter calling intractable.
- **Key click is a "fault" we model deliberately.** Reid notes both key click and leakage are things
  "Laurens Hammond considered to be a fault". We have the click; see L6 for the leakage.

### L2. Percussion is second-harmonic only; the real instrument has a Second/Third selector

- **Book:** Part 57 lists the A100's **four** percussion controls — On/Off, **Second/Third**, Normal/Soft,
  Fast/Slow — and describes the mechanism as "diverting part of the **4' or 2 2/3'** signal through a VCA
  controlled by an AD contour generator". Since 4' is ratio 2.0 and 2⅔' is ratio 3.0, Second percussion is
  the 2nd harmonic and Third is the 3rd.
- **Engine:** `v->org_perc_ph += f * 2.0f * dt` ([`runtime/sound.h:3129`](../../runtime/sound.h)) — the
  2nd harmonic, hard-coded. There is no third-harmonic option.
- **Why it is worth having:** Third percussion is a distinctly different colour and a standard part of the
  vocabulary. It is one multiplier, and the natural home is `eng_p[]` (a note-on structural choice, the
  documented aux channel) rather than a fourth macro.
- **Audible home:** `organ`.

### L3. Percussion decay is fixed where the real one has Fast/Slow

`v->org_perc *= 1.0f - dt / 0.2f` ([`runtime/sound.h:3132`](../../runtime/sound.h)) — a single ~200 ms
decay. Reid's Fast/Slow switch is the fourth of the four controls, and it is one of the two things
organists actually reach for mid-performance. Our morph-driven amount covers On/Off and Normal/Soft
between them, so this is the one genuinely missing axis. Also one `eng_p` slot.

### L4. Hammond percussion is SINGLE-triggering, and this is the strongest argument yet for §B3

- **Book:** Part 57, stated plainly: "Hammond percussion is polyphonic, but **of the single-triggering
  variety, so if a previous note is held, the percussion does not sound**." That is why organists play the
  percussive attack staccato — legato deliberately suppresses it.
- **Engine:** `org_perc` is armed in `sound_organ_start`
  ([`runtime/sound.h:3072-3073`](../../runtime/sound.h)), i.e. per **voice**, so every new note chips
  regardless of what is held. That is the ARP multi-trigger behaviour applied to an instrument that is
  famously the opposite.
- **And here is why this matters beyond the organ.** §K6 found that the flute's chiff *requires*
  multi-triggering: "it's important that the keyboard offers multi-triggering. This ensures that the chiff
  occurs at the start of every note, even when you play legato." So **two instruments we ship need
  opposite settings of the same switch**, and in both cases it is not a matter of feel but of whether the
  instrument's defining transient happens at all. That is the clearest case the audit has made that §B3
  (single versus multi trigger) is an **engine-level policy** rather than a per-cart convention — it needs
  to be a property an instrument declares, because the correct value differs per instrument.
- **Audible home:** `organ` (play legato and the chip should vanish), against `pipe` (play legato and the
  chiff should stay).

### L5. Two of Reid's four named registrations are missing, and they are the interesting two

> **❌ DROPPED 2026-07-29** (plan item 1.6). **And this section under-priced it badly:** "two rows in
> `REG[8][9]`" is wrong, because the table is **8 snapped detents** in the engine — adding rows re-maps
> `instrument_harmonics` for the **13 carts** that set it on an organ slot, silently. The safe route
> (`MODE_ORGAN_XREG` behind ORGAN's aux channel) works but spends permanent public API on two novelty
> presets, and §L1 already verified all nine footages against Part 55, so nothing here is *wrong*. Not built.

- **Book:** beyond 88 8000 000 and 88 8888 888 (both of which we have, L1), Part 55 names two more and
  says what they are *for*: **83 4211 100** is "the closest approximation available to a '1/n' harmonic
  series", and **00 8030 200** is "an approximation to a '1/n' series with all the even harmonics
  missing". Together: "they are the closest a vintage Hammond can come to producing a **sawtooth wave**
  and a **square wave**, respectively."
- **Engine:** our eight recipes are all drawbar-cluster voicings from the 16'/5⅓'/8'/4' family, plus
  all-bars-out. None grades the upper mutations (1⅗', 1⅓', 1') the way a 1/n approximation does.
- **Why:** two rows in `REG[8][9]` (or a widened table), and they add two colours the macro genuinely
  cannot currently reach — an organ imitating a saw and an organ imitating a square, which is a lovely
  thing to have on a fantasy console and is exactly the kind of "the instrument pretending to be a synth"
  register §G is about.
- **Audible home:** `organ`.

### L6. Leakage, with its composition confirmed

§C8 already proposed this from a grep of Part 57. The full quote confirms the composition rather than
just the existence: leakage is "a mixture of **drawbar pitches and noise** that gives the A100 a
characteristic, **throaty** quality", and like key click it is something "Laurens Hammond considered to be
a fault". So a leakage model bleeds the *unpulled* drawbar pitches plus noise under the played
registration — not simply added hiss, which is what §C8 already said. Reid's own attempt to fake it on a
Juno failed for a reason that does not apply to us ("the noise passes through the self-oscillating filter,
and emerges tuned to the 5 2/3' pitch. Bah!"), and he notes it works "far better on the Prophet 10"
because its filters have zero resonance. Ours has no filter in the way at all.

### L7. Smaller items

- **No V-mode vibrato.** We implement C-mode (dry+wet) only; the real thing offers V-1/V-2/V-3 as 100%
  wet pitch modulation, six settings in total with the C modes. Very low priority, and Reid agrees:
  "I never use any of my A100's 'V' settings."
- **Percussion should steal from the sustain.** "adding percussion also reduces the loudness of the
  sustained part of the note, but we're going to overlook this." We also overlook it. One multiply.
- **The scanner has a little AM.** "there is also a small amount of amplitude modulation as the scanner
  sweeps round the taps, but we should be able to ignore this." Ours is pitch-only. Reid says ignore, so
  noted only for completeness.
- **Resonance drains bass, again.** Part 57's reason his Juno patch lands at 67 8321 000 rather than
  88 8000 000: using the filter to synthesize the 5⅓' drawbar costs low-end because "high filter
  resonance usually suppresses lower frequencies". Same physical fact as §B/§E's bass-drain note, and a
  reminder that it is why we model `FILTER_LADDER` and `FILTER_DIODE` that way.

### Suggested Hammond step order

| # | Step | Kind | Where |
|---|---|---|---|
| 1 | L5 add the saw-ish and square-ish registrations | two table rows | `organ` |
| 2 | L2 + L3 Second/Third selector and Fast/Slow decay on `eng_p` | engine, small | `organ` |
| 3 | L4 percussion single-trigger (with §B3, and against §K6's opposite need) | engine policy | `organ` vs `pipe` |
| 4 | L6 tonewheel leakage (§C8) | engine, small | `organ` |
| 5 | L7 percussion steals from the sustain | one multiply | `organ` |

---

## M. Recipe pass 8 — DELAYS, REVERB AND THE EFFECTS LAYER (Parts 22, 60-62)

STATUS: EXPLORING — nothing queued. **This completes the sieve.**

The only arc about the *effects layer* rather than an engine, so it exercises a different part of the
codebase than the seven instrument passes: `echo`/`echo_insert` (+ the BBD voicing), the three reverb
tanks, `reverb_spring`, `chorus`, `flanger`, `phaser`, `tape`, and `grains`. It pairs with
[`../guides/effects-recipes.md`](../guides/effects-recipes.md) rather than [`../guides/instrument-recipes.md`](../guides/instrument-recipes.md).

Sources: Part 22 "From Springs, Plates & Buckets To Physical Modelling" (SOS February 2001), Part 60
"From Analogue To Digital Effects" (April 2004), Part 61 "Creative Synthesis With Delays" (May 2004),
Part 62 "More Creative Synthesis With Delays" (June 2004). Part 19 "Duophony" (November 2000) was also
read to close out the series; it contributes only to §M8.

### M1. What matches, including a capability Reid says nothing has

- **We can put the reverb *inside* the patch, which is his whole thesis and his closing regret.** Part 22
  builds to this: move the reverb from the end of the chain to before the filter and VCA, and "the reverb
  imposes its complex frequency response upon the output from the oscillator, emphasising some harmonics
  while suppressing others. Therefore, as you play up and down the keyboard, **the characters of the
  individual notes change, much like those of an acoustic instrument**." He then closes the article
  regretting that almost nobody can try it: "I doubt that you'll be able to test this with your latest
  digital workstation, because it's unlikely that it will allow you to place its reverbs at the correct
  point within the signal chain." We have `reverb_bus()`, per-instrument aux buses, and `fx_order()` —
  arbitrary effect placement per instrument is a first-class feature. Third time the audit has found us
  holding a capability he treats as out of reach (after §E1 and §K1's bore-coloured noise).
- **Three reverb tanks, and Part 22 asks for exactly three.** His recipe for a "3-dimensional" response is
  three parallel short reverbs at *different* times: "with a single BBD reverb, we're still limited to a
  single dimension. So let's add another two parallel reverbs to our signal path … provided that the
  reverb times are different for each of the BBDs, we will obtain **three families of modes**." We ship
  `SOUND_REVERB_TANKS 3` with independent parameters. Whether that was the reason or a coincidence, the
  count and the independence are right.
- **The spring's "boing" is modelled by dispersion, which is the physically correct mechanism.**
  `reverb_spring` runs an eight-stage stretched-allpass cascade with a live dispersion coefficient
  (`rvb_spring_disp`, the `reverb_spring_tone` BOING knob) plus a mid-band limit
  ([`runtime/sound.h:711-717`](../../runtime/sound.h)). Part 22 explains why a spring boings at all —
  the round-trip reinforcement gives it a comb response, and "a single spring reverb always exhibits a
  characteristic, metallic **'boinggg'**" — and notes that makers fight it by "incorporating two or even
  three **dissimilar** springs", or even by assembling "dissimilar sections into a single spring". Our
  eight *different* allpass lengths (`{89, 113, 67, 97, 127, 71, 107, 83}`) are that decorrelation trick.
- **A room is not a comb, a spring is — and our two reverb voicings split the same way.** Part 22: rooms
  "do not act as comb filters. This is because the thousands of modes are distributed unevenly throughout
  the spectrum, so the overall response is far flatter", whereas the spring's regular round trip gives it
  a genuine comb. Our default tank is dense/flat Schroeder and `reverb_spring` is the comby dispersive
  one, which is the right architectural split rather than one voicing with a knob.
- **BBD saturation is on the delay taps, not bolted on afterwards.** `cho_bbdsat` is applied to each
  chorus read ([`runtime/sound.h:895-896`](../../runtime/sound.h)), and the LFO is a *rounded* triangle
  explicitly "models the BBD capacitor rounding". Part 61's box on why BBDs sound unlike digital delays is
  about exactly this class of per-stage capacitor limitation.
- **Anti-aliasing and reconstruction filters are assumed present.** Reid removes them from his diagrams
  "for the sake of simplicity" but insists "I'd like you to assume that they are in place". Our BBD echo
  carries a loop tone filter and the time-darkening; §B5 records the oscillator-side gap separately.

### M2. Part 22 hands §F4 a second, cheaper way to give `BOWED` a body

The most useful cross-link in the pass, and it arrives from the effects side.

- **Book:** Part 22's leap is that an instrument body *is* a small reverberant room: "Ignoring the holes,
  these are all resonant spaces, like rooms, but with smaller dimensions … the body will exhibit reverb,
  have a value for RT60 and, because of that ol' time/frequency duality, impose a frequency response upon
  the sound." He gives the required delay range — "we're not quite where we need to be, which is in the
  delay range of **about 1mS to 4mS**" — notes a spring is far too long ("the spring would synthesize a
  'violin' with a cavity over four metres long!"), and lands on three short parallel BBD lines with
  different times.
- **Why it matters here:** §F4 found `INSTR_BOWED` shipping with no body resonator at all, and §H5 found
  `GUITAR`'s body is a parallel filter with no return path. Both were framed as "add biquad formants like
  `gt_body[4]`". Part 22 offers the alternative: **three short parallel delay lines at 1-4 ms with
  feedback**, which produces a modal response *and* a decay, is cheaper than four biquads, and is what the
  hardware actually did. We already have the primitives (`moddel_hermite`, the comb helpers, per-instrument
  aux buses).
- **Worth an A/B rather than a decision:** biquad formants give you exact control of named resonances;
  short delay combs give you a physically-derived modal *family* and a tail. For `BOWED`, whose body Reid
  says is the difference between a sawtooth and a violin, trying both is the honest route.
- **Audible home:** `bowed`, `guitar`.

### M3. The reverb has a predelay but no early reflections

- **Book:** Part 22's Figure 4 splits an impulse response into **three** temporal regions: the direct
  sound, then "the so-called 'early reflections' … the first, distinct, reflected sounds you hear — that
  is, the ones that bounce off just one or two of the available surfaces", and only then "the thousands of
  reflected clicks" that fuse into the tail (fusing because "the human brain is not usually capable of
  perceiving echoes separated by less than 30 milliseconds as distinct sounds").
- **Engine:** `reverb_process` is predelay → four parallel combs → two series allpasses
  ([`runtime/sound.h:707-728`](../../runtime/sound.h)). Classic Schroeder. The predelay gives the *gap*
  before the tail but nothing produces the discrete middle region, so we go from dry straight to dense.
- **Fix shape, and Reid supplies it twice:** a handful of taps off the existing predelay buffer before the
  combs. Part 22's Figure 2 uses four delays to build the reflection paths off one wall; Part 61's
  Figures 2-3 are the multi-tap delay line as a primitive. The buffer already exists — this is a tap list
  and a mix.
- **Audible home:** `reverbspace`, `cathedral`.

### M4. We have one chorus topology, and three instruments want three different densities

- **Book:** Part 62 walks up the ladder: Figure 6 "A simple, two-path chorus unit", Figure 7 "Using a
  single LFO to modulate three delay lines" with 120°/240° phase offsets, Figure 13 "**The classic
  three-phase chorus unit**", Figure 10 a four-path variant, and Figure 15 "A 1978 chorus design by
  Roland Corporation" modulating the clocks of four BBDs.
- **Engine:** `chorus_process` is one modulated buffer read at **two antiphase taps** (`d1 = base + mod`
  → L, `d2 = base − mod` → R), described as "a line-for-line port of navkit's BBD chorus (the Juno-6
  hardware model)". So it is deliberately Reid's Figure 6, and deliberately a specific Roland design —
  this is not an oversight.
- **The finding is the mismatch across instruments, not that ours is wrong.** Three things we ship want
  three different densities of the same effect:
  - §L1: the **Hammond scanner** must mix dry with a *single* modulated instance, because "Roland's
    three-stage chorus/ensemble is far too lush". We get this right.
  - A **chorus pedal** wants Reid's classic three-phase.
  - A **string ensemble** wants the densest version — and `solina`'s own `de:meta` lineage says it is
    "demonstrating that `chorus()` is the instrument's **entire identity** rather than a send effect",
    while `chorus()` is giving it the two-path Juno-6 design rather than the four-BBD ensemble design.
    §F1 also recorded Reid saying string synths "relied heavily on built-in chorus effects to thicken a
    weedy initial timbre".
- **Fix shape:** the buffer and the fractional reader already exist; three taps at 120° offsets is a
  parameter and a loop, not a new effect. A `chorus_paths(n)` or a voicing selector would let one
  implementation serve all three.
- **Audible home:** `solina` (its identity), `juno`, `organ` (which must stay single).

### M5. Three delay architectures we cannot express

Part 61 builds these from taps and feedback, and none is reachable with a single-tap line:

- **Ping-pong** (Figures 11-12). Our echo is documented mono in v1 ([`stereo.md`](stereo.md)), so the
  alternating L/R repeat is out.
- **Multi-tap** (Figures 2-3): several taps off one line, mixed. This is also the M3 fix and the Part 22
  reflection-path primitive, so it earns its keep three times over.
- **Cascaded delays, echoes *of* echoes** (Figures 14-16): "Using Delay Line 2 to echo a series of echoes
  produced by Delay Line 1", then all three lines together for "even denser streams of echoes", which
  Figure 17 turns into reverb by adding regeneration. We have one echo insert plus a master send; they do
  not feed each other.
- **Why it is worth noting even though each is small:** Reid's point in Part 61 is explicitly about
  *architecture* over presets — modern units "offer a fixed architecture, and turning the knobs just
  changes the values of the parameters within that architecture … this is still not the same as having
  access to the basic building blocks of effects synthesis, and being able to build new, innovative
  effects structures." Our `fx_order()` already grants ordering freedom; taps and cross-feeding would
  grant topology freedom, which is the thing he is actually arguing for.
- **Audible home:** `spacecho`, `dub`, `aquapuss`, `tapeloop`.

### M6. BBD degradation is cumulative per stage; ours is a lumped colouration

Part 61's box answers "why do analogue and digital delays sound so different" precisely: in a digital line
the samples emerge unchanged, but in a BBD "each Sample & Hold stage will be affected by the limitations
of the capacitors and by electronic noise, so each stage will add or subtract a small voltage from each
sample. **These errors are cumulative** … more often than not, there will be some form of systematic error
introduced." Our BBD voicing (`echo_ins_bbd`, `cho_bbdsat`) colours the *tap*, so the character does not
compound with delay length the way the hardware's does — a long BBD delay should be dirtier than a short
one for structural reasons, not just darker. Cheap approximation: scale the saturation and noise with the
tap distance rather than holding them fixed.

### M7. The spring has exactly three modes, and we model the dispersion but not the structure

Part 22 is specific: "whereas the room has thousands of such modes, **the spring has just three**. The
first is the longitudinal 'compression' mode. The second is the transverse wave … The third is torsional."
And the consequence: "Since each of these modes has different transmission speeds, the true frequency
response of the spring is slightly **smoother** than Figure 10 would suggest. Nevertheless, the
longitudinal mode dominates." He also gives the echo pattern for a 12.5 ms spring: "echoes at 25mS, 75mS,
125mS… and so on" — odd multiples of the one-way time.

Our eight-stage stretched-allpass cascade produces the dispersive chirp, which is the audible signature,
but it is one path. Three parallel paths at different propagation speeds (longitudinal dominant) would
give the smoothing Reid describes *for the physical reason*, and it is the same "three parallel lines"
shape as M2 and his own three-spring assemblies. Low priority; recorded because it is the honest structure.

### M8. Duophony, and the last chapter in the series

Part 19 read only to close the sieve. Its content is one allocation rule, and it belongs to the §B3 theme:
a duophonic keyboard assigns the **lowest and highest** held notes, not the most recent two, "because it's
simpler to design a keyboard that detects the highest and lowest notes than it is to design one that
recognises the middle two notes". So a Dm7 (D F A C) sounds **D and C**; release the F and nothing changes;
release the C and you hear D and A. That is a fifth allocation policy alongside §B3's lowest/highest/
last/first and §H9's per-string choking — worth having in one place whenever §B3 is finally built, and
worth nothing on its own.

### Suggested effects step order

| # | Step | Kind | Where |
|---|---|---|---|
| 1 | M3 early-reflection taps off the existing predelay buffer | engine, small; also unlocks M5 | `reverbspace` |
| 2 | M4 three-phase chorus option (buffer + reader already exist) | engine, small | `solina` |
| 3 | M2 A/B a 1-4 ms three-line body against `gt_body`-style biquads | experiment, answers §F4 | `bowed` |
| 4 | M6 scale BBD saturation with tap distance | engine, small | `aquapuss` |
| 5 | M5 ping-pong and cross-fed delays | engine, medium | `dub`, `spacecho` |
| 6 | M7 three-mode spring instead of one dispersive chain | engine, low priority | `springtank` |

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
