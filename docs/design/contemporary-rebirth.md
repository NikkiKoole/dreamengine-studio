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
was the entire diagnosis. **Three separate defects**, none of which any existing gate could see:

1. **Zero grain overlap on down-shift.** A grain is `2*Tg` wide and grains land `Tt` apart, so coverage
   is `2*Tg/Tt`. The width clamp only pulled `Tg` *down* (correct going up, wrong going down), so up an
   octave got a clean 2.0 while down an octave got **1.0** — Hann windows butt-joined with no crossfade,
   splicing unrelated source samples at full amplitude once per output period. Measured: 177
   discontinuities in 1.2 s, worst jump 0.1517 against a 0.160 peak, **a step 95% of peak**.
2. **Peak-picked epochs instead of phase-locked.** The mark was the local *peak* in a ±28% window, and a
   vowel with a strong F1 has two comparable peaks per period, so the mark hops and consecutive grains
   overlap-add at inconsistent phase. **The streaming face had already fixed this and left a comment
   saying the raw peak "jitters period-to-period → pulsing"** — the offline core simply never got the
   port. This one hit *every* face, which is why snapped and up popped too.
3. **A truncated fractional grain read.** `(int)(j * fstep)` is nearest-neighbour resampling: at
   `fstep` 0.5 every source sample is read twice, a zero-order-hold staircase whose every stair edge is
   a step discontinuity.

**The measurement lesson: one detector was not enough, and the first one lied.** A first-difference
splice finder caught #1 and #3 but scored the snapped and up takes at *zero* — it cannot see a phase
break that happens at the same amplitude. A periodicity-break finder (`r[n] = x[n] − x[n−T]`, normalised,
**with the unprocessed RAW take as the control**) caught #2 and ranked `UP+12` as the *worst* take of the
four, the one the first detector had called clean. Conversely #3 hid from the periodicity metric entirely,
because a staircase repeats identically every period. **Each detector missed a bug the other caught**, so
neither is the gate on its own.

| take | glitches/s before | after | worst error before | after | control (RAW) |
|---|---|---|---|---|---|
| SNAPPED | 51.3 | 36.5 | 2.88 | **1.32** | 0.87 |
| UP+12 | 99.1 | 94.8 | 2.46 | **0.95** | 0.87 |
| DOWN−12 | 37.4 | **20.0** | 2.78 | **0.80** | 0.87 |

**Where it landed:** `DOWN−12` now measures *cleaner than the unprocessed source take* (worst 0.795 vs
0.869, p95 1.02x) with hard splices down 122 → 1. `SNAPPED` and `UP+12` improved 2–3x in severity but
still sit above control (1.13x / 1.22x p95, and up-shift keeps ~5x the control glitch *rate*), so a faint
residual is expected there: inherent PSOLA pulse duplication plus un-filtered decimation at `fstep` 2.
Fixing that needs an anti-aliased grain read and a smarter duplicate-pulse rule, which is the same spike
as the transparent shift.

**This deliberately broke `sample_autotune`'s bit-identity.** That proof was evidence a *refactor* was
behaviour-neutral; it was never a promise to preserve a defect. Re-blessed by measurement instead.

**Two follow-ups worth doing** (both parked in [`voxshift`](../../tools/carts/voxshift.c)'s `todo[]`):
promote the two detectors into a committed `psola-check.js` beside `formant-check.js` so these numbers
are reproducible rather than living in a scratchpad, and treat `voxshift` as the acceptance probe for
*artifact-freeness* rather than only for pitch and length — its four takes plus the RAW control are
exactly the A/B any `at_psola_slot` edit needs. The general point for
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
- **Blown-out drums** — one 16-step kick lane on `tr909.h` with light steps as `schedule_hit`
  ratchets, `instrument_drive` + `instrument_crush` fixed hot.
- **Voice engine** — deferred to v2 (it is the one part needing Rung B and the mic). v1 puts
  `INSTR_VOICE` in the slot instead: deterministic, no permission prompt, and its SIZE macro is
  already a vocal-tract/formant axis, so the box reads correctly while Rung B is built.
- **master destruction** — `multiband()` at 100% with **no bypass control drawn**, `drive_insert` +
  `DRIVE_HARD` always on, and a pitch riser on a held note. One `apply_fx()`-style change-detector,
  per §5.

Build order: ~~Rung A~~ → ~~`hyperbox` v1~~ (both done 2026-07-26) **→ Rung B → `hyperbox` v2's voice
engine → Rung C → the hip-hop rack.**

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
   reworded string cannot silently run off the edge again.

## 7 · Open questions

- **Does a post-hardware rack still get a seeded generator?** The tinyjam magic is generate → play →
  export ([`tinyjam-racks.md`](tinyjam-racks.md)). A hyperpop generator is a real design question:
  the genre's "correctness" is partly *mistakes*, so the generator would need to deliberately
  overshoot (too fast, too loud, too pitched).
- **Faceplate + naming.** Homage names are free, original faceplate for anything paid
  ([`tinyjam-racks.md`](tinyjam-racks.md) §trademark). "SB-808" in the sketch is close to a live
  trademark; the hip-hop rack needs its own name before it ships.
- **Is "no bypass" honest or hostile?** The sketch locks OTT and the clipper on. That is the
  constraint-as-feature thesis, but it also means a cart where a knob does nothing. Precedent to
  follow: `acidcandy`'s always-on machine FX.
- **Where does the AI-prompt answer sit?** The conversation raised a darker candidate: if the
  defining instrument of current pop is a generative model, the contemporary ReBirth is a text box.
  This repo's answer is the opposite bet (an appliance you *play*), which is worth stating out loud
  in the cart's own `de:meta.lineage` rather than leaving implicit.
