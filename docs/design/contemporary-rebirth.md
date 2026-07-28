# Contemporary ReBirth — the post-hardware genre rack (techniques, not machines)

STATUS: BUILDING (2026-07-26) — **Rung A shipped the same day** (`multiband()` /
`instrument_multiband()` / `FX_MULTIBAND`, the OTT box: [`audio-notes.md`](audio-notes.md) §17 #34,
recipes in [`../guides/effects-recipes.md`](../guides/effects-recipes.md)) **and `hyperbox` v1 shipped
as its showcase** — the tiny hyperpop rack: pitch-snapped voice, seven-saw wall, ratcheted 909 kick
lane, and a master chain with no bypass drawn. **Rung B then HALF shipped the same day**
(`sample_shift()` / `harmonize_mic()`, the length-preserving INTERVAL face, showcased by `voxshift`);
its other half, a *transparent* shift in the singer's own voice, failed honestly and is **parked with
measurements** as its own spike, which is what `hyperbox`'s voice box is still waiting on. Next: Rung C
(a beat-synced buffer re-reader). The engine audit below is done and precise: of the eight
boxes across the first two candidate racks, **six needed zero engine work**, and a **third rack
(amapiano, §1c / §2c) added on a second read of the origin conversation needs zero** — a tally of ten of
twelve before the rungs, twelve of twelve after A and B. One gap remains (the beat-synced buffer
re-reader), specced as a rung with an API shape. The pick for the first build was the **hyperpop** rack,
because its four boxes need no external audio at all and it forced the two gaps that are pure signal
processing; **amapiano is now next** (§6), ahead of the hip-hop rack, which still needs Rung C. Origin: a
maker + Claude conversation (2026-07-26) starting from "what would a contemporary ReBirth look like,
not for late-90s acid house but for music popular now."

> **The one idea.** ReBirth RB-338 cloned specific *machines* (TB-303, TR-808, TR-909) because in
> 1997 the genre lived in unobtainable boxes. Modern genres were not made on hardware: their identity
> lives in a **workflow** (the glide, the ratcheted hat, the always-on squash, hard tune as an
> instrument). So a contemporary version clones **techniques**. Nobody covets a box any more; they
> covet a move.

The second idea, which is the same one ReBirth had: the appliance's **constraint is the product**.
In 1997 the pitch was "finally affordable". Today plugins are cheap and cracked, so the pitch is
**"finally constrained"** — four boxes, one screen, no menu diving, no arrangement view, a fixed
master chain, and everything you touch already sounds like the genre.

> **This is an observation, not an invention — credit where it is due.** The originating conversation
> named the precedent: **Teenage Engineering** (the OP-1/PO line) and **Koala Sampler** already ship
> constraint-as-a-feature, and are arguably ReBirth's real spiritual successors, more than any specific
> emulation is. Worth stating plainly because it changes what is actually novel here. The novel part is
> **not** "a constrained music appliance"; that exists and sells. It is cloning a *technique set* rather
> than a *machine set*, which is what makes the box a genre rack instead of a generally nice groovebox.
> It also sets the bar: if a rack we ship feels worse to hold than a PO-33, the constraint was an excuse
> rather than a design.

Companion reading: [`genre-box-rosters.md`](genre-box-rosters.md) is the **hardware-era** catalogue
(which real machines to homage, filtered by what the engine can synthesize); this doc is its
post-hardware branch, where the roster is a list of *moves* rather than gear.
[`tinyjam-racks.md`](tinyjam-racks.md) is the rack program (lane format, generate → play → export,
the trademark rule), [`rebirth-classic.md`](rebirth-classic.md) is the RB-338 pilot itself (shipped
as `acidrack` / `acidcandy` — the chassis both racks below would reuse),
[`audio-input-frontier.md`](audio-input-frontier.md) + [`mic-and-sampling.md`](mic-and-sampling.md)
+ [`transparent-autotune.md`](transparent-autotune.md) are the audio-input veins the vocal boxes ride,
[`distortion-lab.md`](distortion-lab.md) holds the adjacent multiband-distortion gap, and
[`../guides/effects-recipes.md`](../guides/effects-recipes.md) is where a shipped effect's settings land.

## 1 · The two candidate racks (as sketched)

Each is four modules plus a locked master chain, in the ReBirth body plan.

### 1a · The hip-hop / trap rack

| Box | The move it clones |
|---|---|
| **SB-808 · bass engine** | the tuned, gliding, driven 808 sub as the *melodic lead*. Knobs: glide, drive, decay. Its sequencer has **per-step slide flags** — the direct descendant of the 303's slide, which is why this maps so cleanly |
| **Hat machine · drums** | rolls as a **first-class step property**, not hand-programmed. The atomic unit is not "on/off" but "this step is a 1/32 triplet burst": one button per step turns it into a ratchet. This is the equivalent of ReBirth putting accent + slide in the sequencer row |
| **Loop box · melody** | the pitched-down soul chop / pluggnb pluck. Deliberately breaks ReBirth's no-sampling rule (its biggest limitation) but stays minimal: pick a category, pitch it, darken it, reverse it. No waveform editing |
| **Ad-lib pad · vocal** | the vocal tag as an *instrument*: five one-shots, hard tune permanently on, one record slot |
| **master chain** | halftime · stutter gate · multiband squash · tape stop. The halftime/stutter punch-in is this generation's pattern-controlled filter: a tempo-locked effect you ride live |

### 1b · The hyperpop rack

| Box | The move it clones |
|---|---|
| **Voice engine** | the vocal chain *is* the lead instrument, sitting where ReBirth put the 303. The tune-speed knob only goes from "hard" to "harder": no natural setting exists. Presets: chipmunk / robot / choir stack |
| **Supersaw box** | seven detuned saws; knobs detune / bright / **toy** (degradation toward crushed 8-bit ringtone, because the genre flirts with cheap MIDI on purpose — a musical parameter, not a mistake) |
| **Blown-out drums** | clip always on; light steps are glitch stutters; the pattern is intentionally too fast at the end |
| **master destruction** | OTT at 100% with **no bypass** · clipper · pitch riser. Everything too loud by design |

The two mirror each other, which is the interesting part: the hip-hop rack's knobs are all about
**groove precision** (glide, ratchets, timing), the hyperpop rack's are all about **signal abuse**
(pitch, saturation, compression). Same four-box format, opposite philosophy of what a knob is for.

### 1c · The amapiano rack (added on a second read of the origin conversation)

**This one was raised in the originating conversation, dropped from the first pass, and should not have
been** — because the argument made for it was that it is *the closest structural analogue to acid house
of the three*, which is the strongest possible reason to include it in a ReBirth doc.

| Box | The move it clones |
|---|---|
| **Log drum · bass** | the pitched, plucky, gliding percussive sub that *is* the hook. This is the rack's 303: the melody lives in a drum. Knobs: glide (the signature slide between two pitches), knock (woody attack against sub body), decay |
| **Shaker groove · percussion** | the rolling shaker/rim/hat bed that carries the swing. The atomic unit is not a hit but a **feel**: the box owns the shuffle, and the shuffle is the genre. Knobs: swing depth, density, brightness |
| **Jazzy keys · chords** | the warm Rhodes/organ 7th-and-9th vamp, played as a slow loop under everything. Knobs: voicing richness, pad-vs-pluck, tone |
| **master chain** | wide reverb on the keys · a gentle bus glue · a lowpass you ride into and out of the drop. Notably **not** a destruction chain: this genre's master is *spacious*, which is the opposite of hyperpop's |

**Why it is the best structural fit.** Acid house was defined by a small, instantly recognizable
signature-sound set (303 squelch, 808 boom) played by a *groove*, not by a production chain. Amapiano is
the same shape: log drum, shaker, keys, and a specific swing at around 112 BPM. You can hear the genre
from three sounds and one feel. Hip-hop and hyperpop both smuggle in a *chain* (the vocal chain, the
destruction chain) as a de facto fifth box, which strains the ReBirth body plan; amapiano genuinely fits
in four. It is also the only one of the three whose master chain is about **space rather than damage**,
so it exercises a completely different half of the effects shelf.

So the three racks span the axis properly: **groove precision** (hip-hop), **signal abuse** (hyperpop),
and **feel and space** (amapiano). The third is the one that most needs a *human* playing it, which is
either its charm or its problem.

## 2 · Engine audit — what exists, exactly

Verdict first: **six of the eight boxes are pure assembly of shipped API.** Every claim below was
checked against `runtime/studio.h` and a proof cart, not from memory. (First pass = the two racks below,
eight boxes. The amapiano rack was audited the same way in a second pass, §2c: **four more boxes, zero
new engine gaps**, which takes the running total to **ten of twelve** needing no engine work, and to
twelve of twelve now that rungs A and B have shipped.)

### 2a · Hip-hop rack

| Box | Shipped API it is built from | Proof cart | Gap |
|---|---|---|---|
| SB-808 bass | `note_on` + `note_glide(handle, ms)` + `note_pitch(handle, float)` (non-retriggering slide, `acid303.h`'s exact model) · `INSTR_SINE` · `instrument_env(ENV_PITCH)` for the click · `instrument_drive` + `instrument_drive_mode(DRIVE_*)` + `note_drive` to ride it · `instrument_filter` · `tr808.h` `TR_BD` for the transient layer · `sidechain()` / `sidechain_key()` for the duck | `tb303`, `acidcandy`, `braindance` (`I_SUB`); 55 carts use `note_glide` | none |
| Hat machine | `schedule_hit(delay_ms, midi, instr, vol, dur_ms)` is **sample-accurate**, so sub-step bursts do not inherit frame jitter · `tr909.h` already ships the stroke family `tr909_fire_stroke(base, v, stroke, …)` with `TR9_ST_FLAM / DRAG / RATCHET` · `tr808.h` hats · `instrument_choke` for open/closed | `tr909`, `acidcandy` | none. "One button makes this step a triplet burst" is cart-side pattern data |
| Loop box | 8 PCM slots · `sample_load()` · `instrument_sample(slot, sample_slot, root_midi)` · `instrument_sample_region(start, end)` (the chop) · `instrument_sample_mode(SAMPLE_REVERSE / LOOP / PINGPONG)` · `instrument_grains_pitch()` (repitch that keeps slice length) · `instrument_eq` / `instrument_filter` for "dark" · `sample_peaks()` to draw it | `breakchop` (tempo-locked chopping, per-pad reverse/speed/tone, runtime loop via `de_data_path()`), `sampler`, `grainchop` | none in the engine. See §5 for the real blocker (where the audio comes from) |
| Ad-lib pad | `mic_start` → `mic_record(seconds)` → `mic_record_read` → `sample_load` → `sample_autotune(slot, root, SCALE_*, amount)` (formant-preserving snap, shipped 2026-07-17), then pads are `note_on` on the sample slots | `mictune`, `voxbox`, `hardtune`, `singsynth` | none. Ceilings: 8 sample slots, 8 s per mic take |
| master · tape stop | `varispeed(0.25..4)`, documented for exactly this dive | `kaoss` | none |
| master · stutter gate | `tremolo(rate_hz, depth, LFO_SHAPE_SQUARE)` with the rate derived from `bpm()`. (`gate()` is a *threshold* gate, a different thing) | `kaoss` GATE program, `breakchop` STUT | none, but see §5 on set-and-hold |
| master · halftime | nothing time-stretches the mix. `varispeed(0.5)` couples pitch and time (it is a tape). `grains()` + `grains_pitch()` can pitch-compensate, which is grainy and not beat-locked | `breakchop`'s TONE proves granular repitch holds duration, but on a slot bus | **GAP 1** |
| master · multiband squash | **`multiband(low, mid, high, up, mix)` — SHIPPED 2026-07-26** (Rung A). Was: `glue()` one band downward-only · `eq()` before/after · `drive_insert(…, DRIVE_HARD, …)` as the clipper | `fxcheck`; `hyperbox` next | closed |

### 2b · Hyperpop rack

| Box | Shipped API it is built from | Proof cart | Gap |
|---|---|---|---|
| Voice engine | `autotune_mic(root, scale, amount)` is **live and formant-preserving**; the tune-speed knob is its `amount` plus a retune slew, which `hardtune` already implements against `mic_pitch()` · `sample_autotune` for takes · chipmunk is free (play a sample above its `root_midi`: pitch and formants rise together) · robot = the vocoder carrier (`vocoder_mic` + `vocoder_unvoiced`) · choir stack = one sample slot triggered at three intervals | `hardtune`, `livetune`, `mictune`, `voxbox` | the **dial between** chipmunk and transparent is missing, as is a fixed-interval harmoniser. **GAP 3** |
| Supersaw box | `instrument_unison(slot, 1..7, detune)` (`SOUND_UNISON_MAX 7`) · `instrument_unison_detune` rides live · `LFO_DETUNE` / `ENV_DETUNE` for the bloom · `instrument_bandlimit` (PolyBLEP) as the honest "bright" · `instrument_crush` as "toy" | `supersaw`, `motionbox` | none. Unison sums **inside one voice**, so a 7-saw wall costs 1 of 32 voices |
| Blown-out drums | `tr808.h` / `tr909.h` / `drumkit.h` · `instrument_crush` · `instrument_drive` · `drive_insert(…, DRIVE_HARD, …)` · `glue()` · ratchets via `schedule_hit` | `tr909`, `morphbox`, `acidcandy` | none |
| master destruction | OTT = **`multiband()`, SHIPPED** (Rung A) · clipper = `drive_insert` + `DRIVE_HARD` (plus `drive_voice(DRIVE_VOICE_TS)` for a pedal character) · pitch riser = `ENV_PITCH` / `note_pitch` ramp or `varispeed` up · "no bypass" is a UI decision, not an engine feature | `distlab`, `pedalboard`, `fxcheck` | closed |

### 2c · Amapiano rack (second pass, 2026-07-26)

Audited the same way: read against `runtime/studio.h` + `tr808.h` + `drumkit.h` and a proof cart, not
from memory. **Result: zero engine gaps, and it is the cheapest of the three to build.**

| Box | Shipped API it is built from | Proof cart | Gap |
|---|---|---|---|
| Log drum | **`INSTR_MEMBRANE`** is a near-exact structural match and was the surprise of this audit: a struck drumhead of six decaying sine modes whose **`morph` macro is pitch-bend, documented as "the tabla bayan gliss"** — a pitched percussive body that slides is *precisely* what a log drum is. The 808 route also works and may voice better for the sub (`INSTR_SINE` + `instrument_env(ENV_PITCH)` + `note_glide`, the §2a SB-808 recipe verbatim), so this box is a voicing choice between two shipped engines rather than a build | `addis` (MEMBRANE percussion), `tb303` / `braindance` (the glide+pitch-env sub) | none |
| Shaker groove | `tr808.h` ships **`TR_MA`** (maracas, a 24 ms noise burst on the real circuit values) and `acidcandy` already places it as an off-beat shaker; `addis` runs a dedicated `INSTR_NOISE` shaker on 8ths with a level-gated density ladder, which is this box's mechanism already written. Swing is **cart-owned** (there is no `swing()` API): `acidrack`'s period-correct even-16th shuffle off one master knob is the pattern to copy | `addis`, `acidcandy`, `acidrack` | none |
| Jazzy keys | `INSTR_EPIANO` (12 decaying sine modes through a pickup nonlinearity, `harmonics` snapping Rhodes / Wurli / Clav) · `INSTR_ORGAN` (9 additive sines, `harmonics` snapping drawbar registrations up to full gospel) · `INSTR_PIANO` for a brighter voicing · chords from **`harmony.h`** (`hb_pick` is table-driven, so an amapiano weight set is new *data*, not new code) | `epiano`, `cocktail`, `bossa`, `chordwise` | none in the engine. An `HB_AMAPIANO` table is a data addition |
| master (space, not damage) | `reverb()` with 3 tanks · `glue()` is the correct gentle bus compressor here (single-band downward is *right* for this genre, where `multiband()` would be wrong) · `filter()` is built to be ridden live for the drop | `kaoss`, `acidrack`'s PCF | none |

**Two things this audit says that the other two do not.** First, `INSTR_MEMBRANE`'s pitch-bend morph
means the engine already had a log drum and nobody noticed, which is exactly the "it exists but no agent
finds it, so they hand-roll it again" failure `lint-docs` gates against. Second, this rack wants
`glue()` and **not** `multiband()`, which is a useful check on Rung A: the OTT box is a genre choice, not
a general improvement. A rack that reaches for it by default has stopped listening.

## 3 · The three gaps, as build rungs

### Rung A · GAP 2 — multiband squash (the OTT box) ★ SHIPPED 2026-07-26
Both racks need it, so it has the highest leverage of the three.

- **What is missing:** `glue()` is a single-band, downward-only bus compressor. OTT's signature is
  **three bands** and **upward** compression (the quiet detail gets pushed up, which is what makes a
  hyperpop master sound permanently "on").
- **API as shipped:** `void multiband(float low, float mid, float high, float up, float mix);` plus
  `instrument_multiband(int slot, …)`. Down-amount per band 0..1, one shared `up` amount, `mix` 0..1
  with **0 = bypass, byte-identical**. **Named `multiband`, not `squash`** — `squash` is unusable in
  the cart-land namespace because squash-and-stretch is animation vocabulary, and `build-all` caught
  five carts already declaring a local `squash` (the `map` trap from CLAUDE.md, second instance).
- **Where it landed:** insert kind `FX_MULTIBAND` (18, the next free past `FX_GATE` 17), auto-placed
  on first call, so `fx_order()` can put it before or after the drive stage. It reuses the house
  crossover idiom (`eq_process()`'s one-pole split, at 120 Hz / 2.5 kHz here) and a peak follower per
  band. The bands sum back to the input, which is what makes the bypass claim exact.
- **Two things the first render taught us:** with no output makeup the full wall measured **1.8 dB
  quieter** than dry (backwards for an effect whose entire point is "louder and always on"), so
  makeup scales with the mean down amount (now +5.7 dB, 2% clip); and the upward half has to taper
  out near silence or it amplifies the noise floor.
- **Also unlocks:** the multiband distortion gap already parked in
  [`distortion-lab.md`](distortion-lab.md) §Multiband — same crossover, different per-band stage.
- **Gates run (all green):** `soundcheck` silent · `fx-check` shows every *other* effect at Δpk/Δrms
  +0.0 (the byte-identical proof) and the new case finite/bounded/off-dry · `level-check` within
  tolerance · `soak-check` stable · `web-audio-check` wasm parity · `build-all` 566/566 ·
  `lint-fx-frame` clean (it is ride-safe, so it stays out of the footgun set). Recipes +
  §17 #34 ledger entry written.

### Rung B · GAP 3 — the interval face · HALF SHIPPED 2026-07-26, half turned out to be a spike

This rung predicted "no new DSP, just a re-facing". **The transpose half was; the formant-hold half
was not, and the measurement is what settled it.** Both outcomes are worth keeping.

**Shipped:** `sample_shift(slot, semitones)` (offline, in place, beside `sample_autotune`) and
`harmonize_mic(semitones, voices)` (live — the AM_SHIFT face of the same streaming corrector, with a
1–3 voice stack: the shift, plus a fifth, plus an octave). Both transpose while **keeping the take's
length**, which playing a sample slot at a higher note cannot do. Measured exact in
[`voxshift`](../../tools/carts/voxshift.c): f0 within 0.5 Hz of target at +3 / +5 / +7 / +12 / −12,
wobble 2–9 Hz against the raw take's 5.5 Hz. One PSOLA core now wears both faces (snap and shift),
and generalizing it was proven **bit-identical** for the shipped autotune by re-render, not by
argument (same cart, `DE_DEFINES=NO_SHIFT`, matching SHA).

**Parked, with numbers:** an octave up **in the singer's own voice** is not a flag on this code.
Re-spacing epochs while holding the grain content does hold the formants (F2 991 → 947 measured) but
leaves the pitch unstable at *every* interval tried: f0 wobble 170–300 Hz versus the raw take's 5 Hz.
The reason is structural — a grain that carries the source periodicity still sounds at the source
pitch however you re-space it. `sample_autotune` never exposed this because a correction moves the
period by a few percent, where the mismatch is inaudible; an interval halves or doubles it. So the
transparent *shift* needs its own spike, exactly like the transparent *correction* got two.

> **A read of Rubber Band Library (2026-07-26) says why, structurally**, and it changes what the spike
> should attempt: see [`rubberband-reference.md`](rubberband-reference.md) §2a. The short version is
> that a formant-preserving shift is **two independent stages, not one knob** — move the pitch for real
> (we have that, and `fstep` is its knob), then rescale the spectral envelope back by a separate
> factor (we have **no such stage**). Holding formants by turning `fstep` down cannot work, because
> `fstep` belongs to stage one. So the spike's real content is an envelope-rescale stage (cepstral,
> which needs an FFT, or short-time LPC, or a time-varying filter bank), after which the dial itself is
> one scalar. Budget it as that, not as plumbing `at_psola_slot`'s unused `formant` argument. Note the
> library is GPL and unusable in this engine (App Store); it is a reference only.
>
> **That claim was then challenged and held — see §2a-bis for the numbers.** The obvious cheap objection
> is that classic TD-PSOLA is formant-preserving *by construction*, since the pitch move comes from
> epoch re-spacing and `fstep` only resamples content, so `fstep = 1` ought to shift while holding the
> envelope for free. Tested by wiring the unused `formant` argument to
> `fstep = powf(2, (semis/12)*(1 - formant))` and forcing 1, **after** the same day's two grain-geometry
> fixes so the substrate was clean: the down-octave take held its formants exactly (F1 560, F2 991,
> identical to raw) and **its pitch did not move at all** (220.5 Hz, also identical to raw). So `fstep`
> is the *only* thing that moves pitch here, and the unused `formant` argument is a trap, not a
> half-built feature. `sample_autotune` misleads on this point because its corrections are fractions of
> a semitone; a working corrector at ±1 semitone is not evidence of a working shifter at ±12.

**What that costs the rack:** `hyperbox`'s CHIP preset is now real (a pitched-up, length-preserving
take), and so is the CHOIR stack (`harmonize_mic(12, 3)`, or `sample_read` + `sample_load` + a
`sample_shift` per slot). What is still stand-in is "same voice, higher" — the *transparent* end of
the sketch's formant knob.

**Gates run:** `formant-check.js` at five intervals (the numbers above) · the bit-identity re-render ·
`soundcheck` silent · `fx-check` / `level-check` / `web-audio-check` unmoved · `build-all` 568/568 ·
`ui-audit` clean on the probe.

#### Rung B, same-day postscript — THE GATES PASSED AND IT STILL POPPED

Worth writing down as a method lesson, not just a bugfix. Every gate above was green and the pitch
numbers were exact, and then the maker **listened to it** and reported popping: worst going down an
octave, a little on snapped, almost none going up. All three reports were correct, and that asymmetry
was the entire diagnosis. **One defect was found and fixed, a second was found and fixed, and the
third resisted two attempts and is parked** — the honest score, written out below because the two
failed attempts are the useful part. Defects one and two, neither of which any existing gate could see:

1. **Zero grain overlap on down-shift.** A grain is `2*Tg` wide and grains land `Tt` apart, so coverage
   is `2*Tg/Tt`. The width clamp only pulled `Tg` *down* (correct going up, wrong going down), so up an
   octave got a clean 2.0 while down an octave got **1.0** — Hann windows butt-joined with no crossfade,
   splicing unrelated source samples at full amplitude once per output period. Measured: 177
   discontinuities in 1.2 s, worst jump 0.1517 against a 0.160 peak, **a step 95% of peak**.
2. **A truncated fractional grain read.** `(int)(j * fstep)` is nearest-neighbour resampling: at
   `fstep` 0.5 every source sample is read twice, a zero-order-hold staircase whose every stair edge is
   a step discontinuity. LERPing it took the down-octave take from **122 hard splices to 1**.

Both fixes are guarded to the SHIFT face (`mode == AT_SHIFT`, and `fstep` 1.0 makes the LERP a
mathematical no-op), so `sample_autotune` is **byte-for-byte unchanged** — the `NO_SHIFT` re-render still
hashes to the same `1c053d22`. The bit-identity contract survived, which was not the case for either of
the two attempts below.

**The third defect resisted two attempts, and both failures are worth recording.** The popping on the
*snapped* and *up* takes is a phase problem, not an overlap problem: source epochs map to output epochs
by a decision that can flip-flop, so pulses occasionally duplicate or drop. Two textbook fixes, both
reverted:

- **Attempt A — port the streaming face's WSOLA correlation phase-lock into the offline epoch marking.**
  This looked like the obvious win: the streaming face already does it and its comment even says the raw
  peak "jitters period-to-period → pulsing". It **regressed the snap face into period doubling** — the
  snapped take went from a clean f0 220.4 Hz (wobble 1.1) to 178.8 Hz flipping over 110–220. Normalizing
  the correlation into a true coefficient, to kill the energy bias, did not rescue it (165.1, still
  flipping). Why the streaming face needs it and this does not: offline, step 1 has *already* produced a
  per-hop autocorrelation pitch track, so the period is known and peak refinement within ±28% of it is
  both sufficient and stable. The live face has no such track.
- **Attempt B — replace nearest-epoch selection with the classic monotone accumulator**
  (`acc += Tt; while (acc >= T) { acc -= T; ai++; }`). Also regressed snap into doubling (192.7 Hz,
  flipping): with `Tt ≈ T` it advances 0 or 2 epochs instead of 1 near the boundary and those events
  alternate. The rule it replaced is worse in theory — it *rounds* a drifting quantity — and better in
  practice, because it tracks the output position absolutely and is therefore self-correcting instead of
  accumulating error.

- **Attempt C — the same correlation lock but with a NARROW search window** (±8% of the period instead
  of ±25%, on the theory that the alternation came from the window being wide enough to let the mark hop
  by a quarter period, so spacing alternated 1.25T/0.75T). Run *after* `psola-check.js` existed, and the
  gate killed it **in one command**: snapped f0 spread 110.5 Hz, ratio 0.874, doubling again — and the
  periodicity error did not even improve (2.974 vs a 2.875 baseline). Total cost, about a minute,
  against several listen-and-report rounds for attempts A and B.

Attempts A, B and C are now **⚠ DO-NOT-DO comments in `sound.h`** with the measurements attached, which
is the only durable form of this knowledge: the next person to read that loop will have exactly the same
three good ideas.

**The pattern across three failures is itself the finding, and it points at where the real fix is.**
Every attempt broke the *snapped* take and none broke the shifted ones. That is not coincidence: for the
snap face the output period is within a few percent of the source period (`Tt ≈ T`), so *any* jitter in
epoch marking or selection is comparable in size to the correction itself and flips the mapping. At a 2:1
ratio the mapping has enormous margin and shrugs the same jitter off. So the near-unity-ratio case is the
constraint that any redesign has to satisfy first, and "make the epoch mapping smarter" is exactly the
wrong shape of fix — it needs to be *jitter-free*, not better-guessed. Three tries in that family is
enough evidence to stop tweaking and treat it as the dedicated spike.

**The measurement lesson, which is the real yield: three detectors, each blind to something the others
caught.** (1) A first-difference splice finder caught defects #1 and #2 but scored the snapped and up
takes at *zero*, so it certified as clean the very takes the maker could hear. (2) A periodicity-break
finder (`r[n] = x[n] − x[n−T]`, normalised, **with the unprocessed RAW take as control**) ranked `UP+12`
the worst of the four — and was itself blind to the staircase, which repeats identically every period.
(3) Only the **f0 reading** caught period doubling, because *a period-doubled signal is still perfectly
periodic* and the periodicity metric scores it as an improvement. Attempt A's regression measured as a
2x *better* periodicity error while sounding worse. **A metric that improves while the maker's report
gets worse is a metric that is measuring the wrong thing.**

**Where it landed:** the down-octave take is fixed (177 → 1 hard splices; f0 110.3 against a 110.25
target). Snapped and up are back to exactly their shipped behaviour, meaning **the faint popping the
maker hears on those two is still there and is not yet understood well enough to fix without breaking
something else.** A real fix wants a pitch-synchronous mapping designed against a harness rather than a
one-line swap, which is the same parked spike as the transparent shift.

**Three follow-ups worth doing** (parked in [`voxshift`](../../tools/carts/voxshift.c)'s `todo[]`):
promote **all three** detectors into a committed `psola-check.js` beside `formant-check.js` — splice,
periodicity *and* f0-doubling, since each caught what the others missed — so these numbers are
reproducible rather than living in a scratchpad; treat `voxshift` as the acceptance probe for
*artifact-freeness* rather than only for pitch and length (its four takes plus the RAW control are
exactly the A/B any `at_psola_slot` edit needs); and only then re-attempt the phase fix, because attempts
A and B both failed for want of a harness that would have caught the regression in one command instead of
by ear. The general point for
[`checks-and-oracles.md`](../guides/checks-and-oracles.md): **we had no gate for "does it click", only for
"is it in tune", and a clean gate sheet plus a maker's ear beat a clean gate sheet alone.**

### Rung C · GAP 1 — a beat-synced buffer re-reader (halftime / beat repeat)
- **What is missing:** any way to manipulate *time* on the master without dragging pitch along.
  `varispeed` is a tape; `grains` is a texture.
- **API shape:** `void beatfx(int mode, float bars, float mix);` where mode is
  HALFTIME / REPEAT / REVERSE / SCRATCH, over a captured trailing buffer of `bars`. The Gross Beat
  model is a *curve over a rolling buffer*, so a later `beatfx_curve()` is the natural extension.
- **Hard requirement:** it must be **ride-safe**. `kaoss` found that the buffer-rebuilding effects
  glitch when swept, so the parameters have to be read per sample, never re-allocated per call, or it
  lands on the wrong side of the set-and-hold rule (`lint-fx-frame.js`).
- **Cost:** the biggest of the three (a new buffered insert). Do it when the hip-hop rack is next.
- **One idea worth stealing before building it:** the industry answer to non-pitch-coupled time
  manipulation spends the stretch **non-uniformly**, against an onset curve, so transients keep their
  original timing and only the sustained regions absorb the change. Stretch uniformly and a break
  smears. See [`rubberband-reference.md`](rubberband-reference.md) §2c (that library's
  `StretchCalculator` is the study subject; it is GPL and unusable here, so the transferable part is
  the allocation idea, which needs no FFT and fits our existing grain machinery).

## 4 · Ceilings to design a four-box rack against

Checked, not assumed: 32 voices (`SOUND_VOICES`) · instrument slots 5..47 · **8** PCM sample slots ·
8 FX buses (master + 7 per-instrument) · **2** granular tanks (master + one instrument bus) · reverb
tanks 0..2 · two instances per master FX kind (`FX_INST`) · mic takes up to 8 s · unison is summed
inside one voice (a 7-saw stack costs one voice, not seven).

A four-box rack with ratcheted hats, a unison wall and two sample voices fits comfortably.

## 5 · The non-engine blockers (the ones that actually bite)

- **Where the loop-box audio comes from is the hard part of the hip-hop rack, not the DSP.** Three
  legal sources exist: engine-synthesised (`record_arm` + `record_grab`, which is also the only
  replay-deterministic one), a runtime `--data` file (`de_data_path()`, `breakchop`'s path), or the
  mic. There is no baked-into-the-cart audio, deliberately. `breakchop` already carries a RELEASE
  GATE todo about its copyrighted dev fixture; a "soul chop" module inherits that gate exactly.
- **The sequencer is cart-owned by design** ("cart owns the PATTERN, header owns the SOUND"), so
  per-step slide flags, ratchet zones and the pattern bank are hand-rolled in the cart, as
  `acidcandy` does. No engine work, but it is where the cart's line count goes.
- **Set-and-hold.** Every master effect here (`tremolo`, `crush`, `eq`, `tape`, and the new `squash`)
  must be re-applied **only on change**. Wiring a knob straight into `draw()` rebuilds the bus DSP
  60×/s and stutters silently. Copy `groovebox`'s `apply_fx()`; `lint-fx-frame.js` enforces it.
- **`autotune_mic` / `vocoder_mic` / `input_monitor` are LIVE** and break `.rec` replay (ADR-0032),
  so a vocal box cannot be demoed by a committed input track. Its clip has to be captured live.

## 6 · The pick: build the hyperpop rack first

> **Where amapiano (§1c) sits in this order.** It was not in the running when this pick was made,
> because it had been dropped from the first pass. Re-audited (§2c) it turns out to need **zero engine
> work and zero external audio**, which makes it *cheaper* than the hyperpop rack was, so it does not
> have to wait for a rung at all. It stays second rather than first for one honest reason: the hyperpop
> rack was picked precisely because it *forced* the two signal-processing gaps, and that leverage was
> real (both rungs shipped the day it was built). Amapiano would have forced nothing. Now that A and B
> are in, the argument flips: it is the next rack to build, ahead of the hip-hop rack, which still needs
> Rung C and still has the audio-provenance problem in §5. Its own open question is the interesting one:
> the genre's identity is a **feel**, so the rack lives or dies on whether the swing is playable rather
> than on whether the voices are right.

Both racks need two gaps and they share Rung A, so the tiebreakers are these:

- The hyperpop rack's other three boxes are **100% engine-complete** and need **no external audio at
  all**: supersaw, drums and master are pure synthesis. So the cart is small and self-contained.
- It forces the two gaps that are **pure signal processing** (Rung A, Rung B). The hip-hop rack's
  distinctive gap (Rung C, the buffer re-reader) is the biggest build of the three and its loop box
  drags in the sample-source question above.
- Its aesthetic *is* the master chain, which means the rack doubles as the showcase cart for the new
  `squash()` — the "prove the voice as its own cart first" rule from
  [`radiophonic-workshop.md`](radiophonic-workshop.md), satisfied for free.

**Tiny cart: `hyperbox`** — SHIPPED 2026-07-26 (the `*box` family: `voxbox` / `morphbox` / `fmbox` /
`motionbox`). Scope as built, deliberately small:

- **Supersaw box** — one `INSTR_SAW` slot, `instrument_unison(slot, 7, detune)`, three knobs
  (detune / bright / toy) mapped to `instrument_unison_detune` / `instrument_filter` cutoff +
  `instrument_bandlimit` / `instrument_crush`. Playable from a short keybed or a held chord.
  *(v3, 2026-07-27: it has that keybed now — one shared `keybed.h` that FOLLOWS FOCUS between the
  two melodic boxes, so one key = the whole seven-saw stack, an octave below the vocal.)*
- **Blown-out drums** — one 16-step kick lane on `tr909.h` with light steps as `schedule_hit`
  ratchets, `instrument_drive` + `instrument_crush` fixed hot. *(v2 made it four lanes —
  BD/SD/CH/CP — with each step cycling off → hit → ratchet; one lane read as a placeholder. v4 added
  the pad row + REC, so every box in the rack is now PLAYED as well as edited. Two details that make
  a tap-record usable, both worth copying: quantize to the NEAREST step, since a hand tap is late by
  definition and flooring puts every recorded hit one step behind what you heard; and flash the step
  you wrote, because a quantizer that silently moves your hit is indistinguishable from a bug.)*
- **Voice engine** — deferred to v2 (it is the one part needing Rung B and the mic). v1 puts
  `INSTR_VOICE` in the slot instead: deterministic, no permission prompt, and its SIZE macro is
  already a vocal-tract/formant axis, so the box reads correctly while Rung B is built.
- **master destruction** — `multiband()` at 100% with **no bypass control drawn**, `drive_insert` +
  `DRIVE_HARD` always on, and a pitch riser on a held note. One `apply_fx()`-style change-detector,
  per §5.

Build order: ~~Rung A~~ → ~~`hyperbox` v1~~ (both done 2026-07-26) → ~~`hyperbox` v2/v3~~ (the DEPTH
pass, 2026-07-26/27: real master knobs, a playable voice, four drum lanes, a playable saw wall)
**→ Rung B's transparent half → `hyperbox`'s mic voice box → Rung C → the hip-hop rack.**

**What building it taught us** (both worth carrying into the hip-hop rack):

1. **An always-on master fills every hole.** The first render measured rms −1.5 dBFS with a 1.5 dB
   crest: a wall with no groove left in it, because OTT lifts the synth's sustain into every gap the
   kick needed. Two fixes, both cart-side: the saw is a *decaying stab*, not a pad, and the parts sit
   much lower than feels right dry (the master brings them back up). Landed at rms −5.3 / crest 5.2,
   which is where real hyperpop masters sit. The lesson generalizes: with a squashed master you mix
   for the *gaps*, not the levels.
2. **`ui-audit.js` earns its keep on a rack.** Four labelled strips on a 320×200 canvas produced one
   off-screen caption and four title/caption collisions on the first pass. The fix that stuck was a
   `caption()` / `caption_r()` pair in the 4×6 font with `text_width()`-measured right alignment, so a
   reworded string cannot silently run off the edge again. **But it audits only the panel it can
   reach:** the device strips are hand-rolled `tapp()` targets, not `ui.h` widgets, so `--explore`
   can't discover them and a clean pass covered one quarter of the cart (four off-screen captions
   shipped through one). A focus-model rack needs a committed tour track —
   `ui-audit.js <cart> --script tools/clips/hyperbox/02-tour.script` — not `--explore`.
3. **"Constrained" has to mean deep, not sparse.** v1 was honest by its own thesis (no dead code,
   every knob wired) and still read as *unfinished*: "lots of UI that doesn't do anything". Three of
   the causes were structural, not cosmetic — three master bars whose heights were literals (sliders
   that ignore you), one drum lane, and a lead instrument you could not play. The verdict to carry
   into the amapiano rack: **constraint-as-feature only reads as opinionated if what REMAINS is
   deep**, which answers §7's bypass question below. Corollary: it must also be making noise when you
   meet it — v1 booted stopped, so every knob felt dead until you found SPACE.
4. **A control that does nothing gets fixed; a readout that is WRONG survives every pass.** After
   three depth passes, the question "which visible elements still don't work?" turned up six, and the
   two worst were not dead controls but lying labels, both untouched since v1 because nobody re-read
   the parts that were never rewritten. `RETUNE`'s word ran *hard → hardest* as the knob went up,
   while up means a longer `note_glide`, i.e. **gentler** — the label asserted the opposite of the
   sound. And the collapsed strips' mini-pattern was `(s & 3) == 0` for every box but the drums, so
   the saw strip advertised four stabs a bar where two fire, and MASTER, which has no pattern at all,
   displayed one. Same defect as v1's literal-sized bars, hiding in the one place each depth pass
   didn't touch. Two rules: **derive every readout from the thing it describes** (the fix was one
   shared `voice_hits()`/`saw_hits()` predicate used by both the sequencer and the strip, so drift is
   impossible, and MASTER showing its four amounts instead of a fake pattern), and when auditing a
   rack, **audit the labels, not the widgets** — `ui-audit.js` checks that text is *placed* well, and
   has nothing to say about whether it is *true*.
5. **A per-box MUTE must mute your hands too.** `muted[]` was read only by the sequencer, so the
   keybed, the chord pad and the drum pads all ignored it: a box went visibly dark and then played the
   instant you touched it. Muting now also releases what is already sounding, held fingers included —
   but per box, never one global panic, since dropping the vocal's held note because you muted the
   drums is an audible glitch from an unrelated tap.
6. **A shared keybed must retire its hit rect.** `keybed_layout()` is sticky and `keybed_update()`
   hit-tests it whether or not anything drew keys, so in an accordion the rect stays live *under the
   next panel* — v2's drum-grid taps also played vocal notes, silently. Two rules for any rack that
   shares one keybed: `keybed_layout(0,0,0,0)` when no melodic panel is open (QWERTY/MIDI don't use
   the rect, so a plugged-in keyboard keeps working), and `keybed_all_off()` BEFORE any
   `keybed_config()` re-point, because config wipes the held table without firing the off callbacks
   and orphans a voice per held finger.

## 7 · Open questions

- **Does a post-hardware rack still get a seeded generator?** The tinyjam magic is generate → play →
  export ([`tinyjam-racks.md`](tinyjam-racks.md)). A hyperpop generator is a real design question:
  the genre's "correctness" is partly *mistakes*, so the generator would need to deliberately
  overshoot (too fast, too loud, too pitched).
- **Faceplate + naming.** Homage names are free, original faceplate for anything paid
  ([`tinyjam-racks.md`](tinyjam-racks.md) §trademark). "SB-808" in the sketch is close to a live
  trademark; the hip-hop rack needs its own name before it ships.
- ~~**Is "no bypass" honest or hostile?**~~ **ANSWERED by v1's reception (2026-07-26).** It is
  neither: it is *invisible* either way, and what decides the reading is the rest of the box. v1 kept
  the bypass out AND shipped a shallow master panel, and the verdict was "feels very incomplete". v2
  kept the bypass out, put all four `multiband()` amounts on real knobs, and drew the three bands big
  as the one honest picture of what the chain does — same constraint, opposite reading. So: lock the
  chain on, but never let the locked box be the thin one. (Also: no fake output meter, however much
  room the panel has. The engine exposes no master output level, and a decorative one is exactly the
  defect v1 was pulled up for.) Precedent still `acidcandy`'s always-on machine FX.
- **Where does the AI-prompt answer sit?** The conversation raised a darker candidate: if the
  defining instrument of current pop is a generative model, the contemporary ReBirth is a text box.
  This repo's answer is the opposite bet (an appliance you *play*), which is worth stating out loud
  in the cart's own `de:meta.lineage` rather than leaving implicit.
