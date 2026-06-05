# dreamengine — audio design notes

A living document for the sound system: what exists in code today, where it sits
relative to the chips it evokes, the ideas on the table, and a guiding rule —
**get as much expressivity as possible from as few primitives as possible.**

Companion to [`../VISION.md`](../VISION.md). Implementation lives in `runtime/sound.h`
(header-only, included once from `runtime/studio.c`); the public surface is the `sound`
section of `runtime/studio.h`. **Genre: design exploration** — rationale + roadmap for the
sound subsystem; status lives in [`../STATUS.md`](../STATUS.md), settled choices in
[`../decisions/`](../decisions/README.md) (e.g. [0003 code-first sound](../decisions/0003-code-first-sound.md)).

---

## 1. Guiding principle: maximum range, minimum surface

The engine is for people learning C by making games. Every new function is a new
thing to read, hover, document, and misuse. So the bar for adding to the audio API
is not "is this useful?" but **"what is the leverage?"** — how much new sonic range
does one new concept unlock.

We rank ideas by *leverage*, not by feature-completeness. A single primitive that
turns into engines, sirens, sliders, and theremins beats five one-trick calls.

Three corollaries:

- **Prefer one reusable concept over many special cases.** One "instrument" concept
  unlocks ADSR + vibrato + duty + cart-authored sound at once. That's high leverage.
- **Keep the easy path zero-setup.** Beginners should still be able to `note(60, INSTR_SAW, 5)`
  with no ceremony. Richer features are opt-in, never mandatory.
- **Match the hardware mental model where it's also the simplest model.** Real chip
  audio *is* writing registers to fixed channels every frame. Leaning into that is
  both authentic and beginner-friendly — a rare alignment.

---

## 2. Where we are now (in code)

> **Refreshed 2026-06-04**, after the §11 mod-envelopes shipped (same-day additions:
> `schedule_hit`, `wave_set` + `INSTR_USER0..3`; same-day cut: `music()` —
> [decision 0013](../decisions/0013-cut-music-api.md)). This is the authoritative map of
> the shipped sound surface: **33 functions + 36 constants**. §5–§11 hold the rationale
> for how it got here; STATUS item 5 holds what's next (engines, SFX authoring).

**Engine**
- 8 voices (`SOUND_VOICES`), 44.1 kHz, **mono**, software-mixed (sum × 0.2 gain each,
  hard-clipped to [-1, 1]). Runs on raylib's audio-stream callback (the **audio thread**).
- Voice allocation: first free → else a **non-held** voice → else voice 0. Held notes
  are stolen last — they're meant to last.
- Held-note **handles** pack slot (low 3 bits) + generation; a stale handle (its voice was
  stolen or the slot reused) silently no-ops. 8 handle slots. See `held-notes.md`.

**The surface, in four layers**

1. **Just make sound — zero setup.** `note` `hit` `chord`(+9 `CHORD_*`) `strum`
   `tone`(+6 `SCALE_*`) `schedule` `schedule_hit` `sfx`. Slots 0–4 come pre-filled
   with the raw waves (`INSTR_SQUARE/SAW/TRI/NOISE/SINE`), so the first
   `note(60, INSTR_SAW, 5)` needs nothing else. `schedule_hit` (delay **+** duration) fills
   the note-call quadrant that lets a cart sequence sub-frame sfx steps sample-accurately.
2. **Design an instrument.** A slot (5–15) is the container; one call per axis —
   `instrument` (timbre + **amp ADSR**) · `instrument_duty` (pulse width) ·
   `instrument_lfo` (**×3**, cyclic modulation) · `instrument_env` (**×2**, one-shot AD
   modulation, bipolar amount — §11) · `instrument_filter` (resonant SVF, 5 `FILTER_*`
   modes). Five axes that multiply: timbre × amp-env × LFOs × mod-envs × filter.
   Timbre itself is extensible: **`wave_set` + `INSTR_USER0..3`** are four DRAWN
   single-cycle waveforms (64 samples, live-morphable — the §8.4 SCW idea; draw them in
   the `wave editor` cart) that work anywhere a wave id does.
3. **Play it live — held notes.** `note_on`→handle, `note_off`, `note_off_all`;
   performance gestures `note_pitch` / `note_vol` / `note_glide`; live twins of the
   defines: `note_duty` `note_lfo` `note_env` `note_filter` `note_cutoff` `note_res`.
   Live writes slew per-sample (no zipper).
4. **Musical time & theory.** `bpm` `beat` `beat_pos` `every` (the clock, ticked in
   `sound_tick(dt)`) · `euclid` `chance` `degree` (rhythm + scale math).

**The modulation matrix is complete** — every parameter is reachable three ways
(cyclically, per-note, and by hand). This table *is* the synth:

| Destination | LFO (cycles) | mod-env (one-shot) | live (`note_*`) |
|---|---|---|---|
| pitch  | `LFO_PITCH` vibrato   | `ENV_PITCH` punch / zap | `note_pitch` (+ `note_glide`) |
| cutoff | `LFO_CUTOFF` wah      | `ENV_CUTOFF` the "pew"  | `note_cutoff` / `note_res` |
| duty   | `LFO_DUTY` PWM shimmer| `ENV_DUTY` PWM sweep    | `note_duty` |
| volume | `LFO_VOLUME` tremolo  | — *(the amp ADSR — deliberate, no `ENV_VOLUME`)* | `note_vol` |

**Data model**
- **SFX**: 32 slots × up to 32 steps `{pitch (MIDI), instr, vol 0..7}`; `step_dur` in
  10ms units; per-SFX loop flag. (**Music patterns are gone** — `music()` cut 2026-06-04,
  [decision 0013](../decisions/0013-cut-music-api.md): zero cart users, the generative
  beat-clock path won.)
- ⚠️ **The SFX bank is still hardcoded** in `sound_load_demo_data()` (6 demo slots) — no
  cart-side authoring. Settled direction: carts author sound *as code* (the sfx-editor /
  sfx-generator carts export paste-ready C); `sfx(n)` exists for first-contact demos and
  `sfx(-1)` as the "silence ringing sounds" verb. A live-performance answer (play music
  in, capture it as control events) is explored in
  [`input-recording-looper.md`](input-recording-looper.md).

**Thread-safe control (the important architectural fact)**
- Main thread → audio thread via a 512-entry **request ring buffer**; kinds 0–20 cover
  play (0–2), define (3–6, 18, 20 = `wave_set`, packed 4 samples/request), and held-note
  live control (7–17, 19). Delayed requests (`strum`/`schedule`/`schedule_hit`) sit in a
  64-entry holding pen on the audio thread.
- **The ring buffer is the one correct place to mutate sound state.** Every new control
  surface rides it rather than poking shared structs (the §11 mod-envelopes are kinds 18/19).
- **Overflow is a tripwire, not silence**: dropped requests are counted and `sound_tick`
  printh-screams `[sound] WARNING … DROPPED` (editor log / bake / play.js output). The
  `soundcheck` cart is the worst-case self-test — run it after touching `sound.h`.
- ⚠ **Ordering subtlety**: define kinds apply when *drained* (next callback), but a
  *delayed* note snapshots its instrument slot **at fire time** — per-step instrument
  changes for scheduled notes therefore need a **rotating slot per pending step** (see the
  sfx editor's CUT lane), or the last define wins for every queued note.

**Rules that shape everything**
1. **Wave + amp-ADSR snapshot at note-on** — you can't re-attack or re-timbre a ringing
   voice. Everything else (filter, duty, LFOs, mod-envs, pitch, volume) is live on a held note.
2. **One-shots stay fire-and-forget**; continuous control is the held-note layer (§6 → §10.3,
   shipped). The two models coexist; handles make the live layer safe.

**Known warts (accepted, documented)**
- LFO `depth` / env `amount` **units depend on the destination** (semitones / Hz / 0..1) —
  the §10.2 wart; named sugar wrappers are the known escape if it ever bites.
- Mixed ranges (vol 0–7, res 0–15, sustain 0–7) — each fits its domain; small cognitive tax.
- The mod-env is **AD-only** (no sustain/release stage) — widening noted in §11.2.
- No master volume / mute / looping ambience yet (STATUS "smaller open items").

---

## 3. Where that sits vs. the chips

Short version: it's chiptune-family, but **cleaner and more generic** than either the
C64 SID or the NES APU, because the *expressive* tools that define those chips are the
ones we're missing.

| Feature | C64 SID | NES APU | dreamengine now |
|---|---|---|---|
| Voices | 3 | ~5 | **8** (more than either) |
| Waveforms | tri/saw/pulse/noise | pulse/tri/noise/DPCM | square/saw/tri/noise/sine |
| Duty / PWM | ✅ variable (signature) | ✅ 4 duties | ❌ square locked at 50% |
| Envelope | ✅ full ADSR | ✅ volume decay | ❌ fixed declick only |
| Filter | ✅ resonant multimode (its identity) | ~fixed | ❌ none |
| Pitch sweep / vibrato | via envelope | ✅ sweep unit | ❌ hand-stepped in SFX |
| Ring mod / hard sync | ✅ | ❌ | ❌ |
| PCM samples | — | ✅ DPCM | ❌ |

Takeaways:
- **C64 is the least accurate comparison** — the SID's identity is filter + PWM + ring
  mod, none present.
- **Closest real chip is the NES APU minus duty cycles, sweep, and DPCM.**
- **Polyphony exceeds the real hardware** (8 vs 3/5), so we don't impose authentic
  voice-stealing scarcity.
- Timbre is "clean chiptune"; the sine is something neither chip truly had.

---

## 4. The decision that drives all additions: *where does a parameter live?*

Every new knob (duty, attack, cutoff, vibrato…) must attach somewhere. Three homes:

| Home | Shape | Right for | Wrong for |
|---|---|---|---|
| **On the note** | `note(midi,instr,vol,duty,attack…)` | one-off tweaks | bloats fast — unusable past ~2 extra args |
| **On an instrument** | `instrument(slot,…); note(midi,slot,vol)` | timbre: duty, ADSR, vibrato — set once, reuse | adds a setup step + a concept |
| **Global / master bus** | `filter(mode,cutoff,res)` | one shared effect (the SID filter) | can't differ per voice |

**The extension point already exists:** `instr` is currently a *waveform id* (0–4).
Reinterpret it as an **instrument slot**: pre-fill 0–4 with today's waveforms + the
default declick envelope, let carts define 5+. Every existing cart keeps working
unchanged, and duty/ADSR/vibrato get a natural home without touching `note()`'s
signature. **Bonus:** the same bank mechanism is what carts need to author their own
sounds at all (see the §2 gap) — one concept closes two holes.

---

## 5. Ideas, ranked by leverage

### 5.1 Pulse duty — *highest leverage, near-zero cost*
The oscillator already branches on wave; square is `phase < 0.5f`. Make it `phase < duty`.

```c
// Option A — just more waveform ids (zero signature change, most NES/PICO-8-like)
#define INSTR_PULSE_12  5   // thin / nasal
#define INSTR_PULSE_25  6
// INSTR_SQUARE (50%) already exists
```
- **Leverage:** ~3 lines + 2 constants → a big jump in timbral range. Covers ~90% of
  "chip" character.
- **Limit:** fixed duties can't *animate* (no PWM shimmer) — that needs duty as an
  instrument field or a live channel (§6).
- **Verdict:** ship first.

### 5.2 Slide / portamento — *easy, self-contained*
A one-shot glide. Fits the existing `note`/`hit` family; no new concept.
```c
void slide(int midi_from, int midi_to, int instr, int vol, int dur_ms);
```
- Implementation: lerp `freq` across the voice's life. Small.

### 5.3 Preset instruments — *easy, beginner-facing*
Before the full bank, ship 2–3 baked envelopes as named ids:
```c
#define INSTR_PLUCK  …   // fast attack, short decay
#define INSTR_PAD    …   // slow attack, long release
```
- **Leverage:** real envelopes with *zero* params for the learner. Buys time before the
  instrument concept lands.

### 5.4 Instrument bank (ADSR + vibrato + custom duty) — *the keystone*
The one real concept jump; pays for itself many times.
```c
// attack/decay/release in ms, sustain 0..7
void instrument(int slot, int wave, int attack_ms, int decay_ms, int sustain, int release_ms);
void instrument_vibrato(int slot, float depth_semitones, float rate_hz);
// (duty becomes a field here too)
instrument(5, INSTR_SAW, 5, 120, 3, 200);   // plucky
note(60, 5, 7);
```
- **Leverage:** one concept unlocks ADSR **+** vibrato **+** animated duty **+**
  cart-authored timbres simultaneously. Highest feature-per-concept ratio on the list.
- **Costs:** "define then play" is a mental step; release means voices outlive their
  nominal duration → voice-stealing gets slightly smarter. Bank is read by the audio
  thread, written via the ring buffer (same pattern as `sfx_bank`).

### 5.5 Master filter — *biggest "SID" payoff, do last*
DSP is trivial (a state-variable filter is ~6 lines). Do **one global** filter, not
per-voice.
```c
#define FILTER_OFF 0
#define FILTER_LOW 1
#define FILTER_HIGH 2
#define FILTER_BAND 3
void filter(int mode, int cutoff_hz, int resonance);   // res 0..15

// sweep it per frame for the classic SID "wow"
filter(FILTER_LOW, 400 + (int)(noise(now()) * 3000), 10);
```
- **Leverage:** one call → the single most C64-sounding result available; cheap (one
  filter, not eight); sweepable for free by calling it each frame.
- **Risks:** hits everything (drums/noise too); resonance self-oscillation must be
  clamped; the one feature that can "sound broken" for beginners.

### 5.6 Expose the SFX bank to carts — *fills a current hole*
Independently of the above, carts can't author `sfx` data today. A small
`sfx_def(slot, steps…)` API would make the existing bank machinery actually usable
from a cart. Low-glamour, decent leverage (unlocks data-driven sound + a future
tracker UI to match the sprite editor — already a stated VISION goal).
(The *pattern* half of this idea is dead: `music()` was cut 2026-06-04,
[decision 0013](../decisions/0013-cut-music-api.md) — any future pattern playback is
an export-as-code editor cart, not engine banks.)

> **Direction (2026-06-04, leaning — not locked):** prototype this as a **PICO-8-style
> editor CART first, with zero new engine API.** Draw the pitch contour over steps with
> the mouse, toggle wave/vol per step (instrument slots 5–15 give each "column" a full
> voice — richer than PICO-8's fixed waves). Everything needed already ships:
> **playback** = the cart sequences its own steps with `hit()`, `schedule()`-ed ahead for
> sample-accurate timing off the beat clock; **persistence** = `save_bytes`/`load_bytes`
> (the modrack pattern); **into a game** = export-as-C-code (a step array + a ~10-line
> player), which is the decision-0003 "code-first" answer anyway. Only if the prototype
> proves the *engine* should own banks (determinism for play.js, a real tracker UI) does
> `sfx_def()` get built — and the prototype then informs its shape.

### 5.7 Quick effort/payoff table
| Idea | Effort | Payoff | New concept? |
|---|---|---|---|
| Pulse duties (constants) | tiny | high | no |
| `slide()` | small | medium | no (1 flat call) |
| Preset instruments | small | medium | no |
| Instrument bank (ADSR/vib/duty) | medium | **high** | yes (the keystone) |
| Master `filter()` | medium | high (most "SID") | no (1 global call) |
| Cart SFX authoring | medium | medium | yes (data model) |

---

## 6. Real-time / continuously-adjustable sound

> **Status: SHIPPED — but as handles, not channels.** This section sketched fixed *held
> channels* (`ch_play`/`ch_pitch`/…). We instead shipped a **handle** model —
> `note_on()→handle` / `note_off()` + `note_pitch`/`note_vol`/`note_cutoff`/`note_duty` —
> because handles give polyphony for free and match the MIDI mental model. The §6.2 slew
> insight below is exactly what we built. Full rationale + the shipped API:
> [`held-notes.md`](held-notes.md). The channel sketch below is kept as the original
> exploration.

Goal: sounds the *game* drives frame-by-frame — an engine that revs with speed, a UI
slider that pitches up, a charging laser, wind that swells, a siren, a theremin.

**The trap: "just re-set the instrument every frame" does not work** — for three
reasons rooted in §2:
1. Voices **snapshot** at note-on, so mutating a bank entry won't move the sound you
   currently hear.
2. One-shots **end**; re-triggering `note()` every frame resets phase → machine-gun
   clicking, not a morph.
3. An instrument is a **shared template** — two emitters on one instrument can't differ.

The right target is a **live voice you write to continuously**, not the template. This
is also exactly how a SID/NES program works: keep poking a channel's registers and it
sings until you change them.

### 6.1 Models
| Model | Shape | Independent emitters? | Beginner cost | Authenticity |
|---|---|---|---|---|
| Mutate instrument | `instrument(slot,…)`/frame | ❌ shared | low | — (fights it) |
| **Held channels (fixed N)** | `ch_pitch(0,…)`/frame | ✅ per channel | low–med | **high (= real chip)** |
| Voice handles | `int v=sound_play(); set(v,…)` | ✅ per handle | med (lifetime mgmt) | high |

**Recommended: held channels.** A "channel" is just one of the 8 voices, reserved,
sustaining, and marked non-stealable. Fixed channel numbers mean no allocation and no
handle-lifetime problem — finite and predictable, like the hardware.

```c
void ch_play (int ch, int midi, int instr, int vol);  // start a held tone (vol 0 = silent)
void ch_pitch(int ch, float midi);   // float → glides
void ch_vol  (int ch, int vol);      // 0..7; 0 gates off without freeing the channel
void ch_duty (int ch, float duty);   // real-time PWM
void ch_cutoff(int ch, int hz);      // when there's a filter
void ch_stop (int ch);
```
```c
void init(void)  { ch_play(0, 40, INSTR_SAW, 0); }      // start silent
void update(void){
    ch_pitch(0, 36 + speed * 0.6f);                      // pitch tracks speed
    ch_vol  (0, speed > 0.1f ? 5 : 0);                   // fade at a stop
}
```

The `sound_play() → handle` variant is the alternative when you want *many* transient
controllable sounds without assigning channel numbers; its cost is handle invalidation
on voice-steal (needs a generation counter). For a learning engine, fixed channels win.

### 6.2 The gotcha that bites *every* continuous approach: smoothing
Parameters change at 60 fps; audio runs at 44,100. If a voice reads the latest value
raw, that's **60 discrete jumps/sec** → stairstepping on pitch and outright **zipper
noise** on volume/cutoff. Naive "change it continuously" sounds *bad*.

Fix: **per-parameter slew.** Store `target` and `current` per knob; in the audio
callback glide `current += (target - current) * k` each sample. (Generalizes the
existing per-step declick.) Pitch wants light smoothing or it feels laggy; volume and
cutoff want more.

**So the real work in real-time audio is not the API — it's (a) a persistent voice that
doesn't auto-free, and (b) the per-param slew.** Both are small; (b) is the line between
"sounds like a synth" and "sounds broken," and is the only genuinely new DSP.

### 6.3 Coexistence
Keep both worlds; they share the 8 voices:
- **Events** (fire-and-forget): `note/hit/chord/sfx` + the instrument bank → blips, hits, music.
- **States** (held/continuous): the channel model → engines, sliders, sirens, ambience.

---

## 7. Suggested roadmap

A path that front-loads zero-setup wins, then introduces the two real concepts:

1. **Pulse duties** (`INSTR_PULSE_*`) — tiny, high payoff, no new concept.
2. **`slide()`** — easy, self-contained.
3. **Preset instruments** (`INSTR_PLUCK/PAD/…`) — easy, real envelopes for free.
4. **Held channels + per-param slew** (§6) — the real-time foundation; also proves the
   smoothing that everything dynamic depends on.
5. **Instrument bank** (ADSR + vibrato + custom duty) — the keystone; also unlocks
   cart-authored timbres.
6. **Master `filter()`** — the cherry; biggest "SID" payoff, riskiest, benefits from
   4–5 existing.
7. **Cart SFX/pattern authoring** — fills the §2 gap; groundwork for a tracker UI.

Steps 1–3 are flat additions that fit the current "just call it" vibe with no new mental
model. Step 4 and step 5 are the two concept jumps; everything dynamic and everything
rich, respectively, hangs off them.

---

## 8. Borrowing rich instruments from navkit

> **Status: QUEUED — next sound feature, not started (as of 2026-06-03).** The plan below is
> agreed; the trigger just hasn't been pulled. **First bite when we do:** port **Karplus-Strong
> pluck** as `INSTR_PLUCK` — ~20 lines, sounds unmistakably like a real plucked string the moment
> it runs, needs no companion effect, and drops onto the existing fire-and-forget `note()` path.
> Crucially it exercises the small per-voice delay buffer (§8.2 — *decided*), proving the riskiest
> new infrastructure first so the whole buffered family (piano / guitar / strings) then rides a
> validated path. It ships with the fixed **3-macro surface** (harmonics / timbre / morph —
> §8.1.1) so it's live-tweakable from frame one, not a frozen preset. Then **`INSTR_MALLET`**
> (buffer-free, the celesta / music-box voice), then **`INSTR_ORGAN` + Leslie** as a complete
> package (drawbars → scanner on the proven buffer → shared rotary), then the EP / acoustic-piano /
> guitar family. (Organ *was* the original first bite for being buffer-free/zero-risk; once the
> buffer decision landed that risk argument dissolved, and organ's best self needs the Leslie —
> so it moved to the package bite.) **Coordinate:** this touches `runtime/sound.h`/`studio.c`,
> which the live/libtcc runtime work also lives in — sync before starting. modrack would expose
> these as a PLAITS-style MACRO voice (or just more `wav` options). Full sketch: §8.8.

There's a sibling project, **navkit** (`~/Projects/navkit/soundsystem`), with a large,
mature software synth (~53k LOC of header-only engines). It contains fully-built,
*self-contained* (no malloc, no heap in the hot path) oscillator engines for rich,
non-chiptune instruments: a Hammond tonewheel organ, Rhodes/Wurlitzer electric piano,
Karplus-Strong acoustic piano/guitar, bowed strings, mallets, formant/vowel voices, FM,
additive, and more. Each is a per-sample `processXxxOscillator(voice)` function — exactly
the shape our audio callback already uses.

The point of pulling from it: **"limited games, but with gorgeous sounds."** A silent-movie
platformer scored with a real upright piano + a theatre organ beats any amount of bleepy
breadth. We don't need general synthesis or a tracker for that — we need ~6–8 hand-tuned
instruments that sound like real money.

### 8.1 The key realization: `instr` is already the delivery vehicle

Adding a Hammond organ needs **no new API and no new concept.** It's one more `INSTR_*`
id wired to a richer oscillator in the mix loop:

```c
note(60, INSTR_ORGAN, 6);     // plays a B3 — same call shape as everything else
chord(48, CHORD_MAJ7, INSTR_EPIANO, 5);
```

This is the highest-leverage move in the whole doc against the §1 rule: the timbre ceiling
goes through the roof while the learner-facing surface stays tiny. The §5.4 "instrument
bank + ADSR" path is about letting carts *build* timbres; this path is about *shipping*
finished ones. For our audience, shipping finished ones wins — keep each engine's ~30 internal
params out of the API, but **not** behind a wall of frozen presets: expose a *fixed, tiny,
engine-agnostic* macro set (§8.1.1) so carts can still play the timbre live. Named per-engine
presets (`INSTR_ORGAN_JAZZ`, `INSTR_EPIANO_RHODES`) then become just baked macro positions.

### 8.1.1 The macro refinement — 3 live knobs per engine, not 0

> Amends §8.1/§8.6's original "expose 0 params, ship frozen presets" stance. The fear that
> stance guards against is real but narrower than "0 params": it's the *per-engine named param*
> explosion (`organ_drawbar()`, `epiano_bark()`, `piano_hardness()` …), which grows the API
> `O(engines × params)`. The escape is to keep the count **fixed and engine-agnostic** — not
> to expose nothing.

Borrow the discipline from Mutable Instruments **Plaits**: dozens of wildly different synthesis
engines, and *every one* exposes the **same three normalized controls** — **HARMONICS, TIMBRE,
MORPH** — and nothing else. Each engine internally maps those 0..1 inputs onto whichever 3–5 of
its own internal params matter most. The surface for engine #1 and engine #20 is identical.

**This is not theoretical — it's the MicroFreak.** Arturia's MicroFreak digital oscillators are
built on Mutable's open-source code (the Plaits/Braids lineage; Émilie Gillet's engines are
MIT-licensed), and the front panel surfaces exactly this idea as three continuous knobs
(**Wave / Timbre / Shape**) that mean something different per engine. A hardware synth people
love to *play* settled on three macros per engine — that's the strongest validation we could
ask for. We'll use the Plaits names (harmonics / timbre / morph). Three it is.

So the rule for the navkit port becomes: **every `INSTR_*` engine exposes the same three
normalized macros — `harmonics`, `timbre`, `morph` — and no per-engine params.** The surface
is `O(1)`: it does not grow when the 8th engine lands.

#### The surface (reuses the existing handle + slew + CV path)

We already ship live, slew-smoothed, CV-able setters on held voices (`note_cutoff`, `note_res`,
`note_duty`, `note_pitch`, …) — modrack's VOICE drives them off a knob and a CV cable today. The
macros are three more destinations on that *same* machinery; no new plumbing concept:

```c
// baked into a slot (the instrument template)
void instrument_harmonics(int slot, float x);   // 0..1
void instrument_timbre   (int slot, float x);   // 0..1
void instrument_morph    (int slot, float x);   // 0..1

// live on a held voice — the game / a knob / a CV cable is the modulator
void note_harmonics(int handle, float x);        // 0..1, slewed
void note_timbre   (int handle, float x);        // 0..1, slewed
void note_morph    (int handle, float x);        // 0..1, slewed
```

Three macros × {slot, live} = six functions, **forever** — bounded, because the count never
scales with the number of engines. (If even six bites, the `which`-index form already used by
`instrument_lfo` is an option — `note_macro(h, MACRO_TIMBRE, x)` — but named reads better for
beginners and there are only ever three.)

#### Where the taste lives

The richness of the ~30 internal params doesn't vanish — it moves from *API* into a tiny
**per-engine mapping function** the porter writes once, clamped to the good-sounding range so
the instrument stays "impossible to make ugly":

| Engine | harmonics | timbre | morph |
|---|---|---|---|
| **Organ** | drawbar registration (sub / 8′ / 4′ / 2′ mix) | brightness / drawbar tilt | percussion + internal vibrato/chorus scanner depth *(Leslie is a separate shared effect — §8.3, not a macro)* |
| **Electric piano** | partial spread (Rhodes↔Wurli) | pickup growl / bark amount | bell↔tine balance |
| **Acoustic piano** | unison detune / string spread | strike hardness | dispersion ("expensive"-ness) + damping |
| **Strings** | section size (1 player↔ensemble) | bow brightness | bow pressure / attack bite |
| **Mallet** | inharmonicity (marimba↔bell) | strike position | decay length (celesta↔vibe) |

That table is *code, curated by taste* — not API surface. The beginner gets three knobs that
always "make it more interesting"; the porter decides which internal params each one sweeps.
**Infinite internal richness, constant external surface** — the actual middle ground.

#### Composes with the four axes (§10), and with modrack

The macros sit *on the engine/oscillator*; the shipped ADSR / LFO / `instrument_filter` axes
still wrap *around* it. So an organ can have `note_morph` ramp the Leslie up **and** an
`LFO_CUTOFF` wah **and** a live `note_cutoff` — all on one handle, all slewed. In modrack the §8
engines become a Plaits-style **MACRO voice** with three CV inlets (HARMONICS / TIMBRE / MORPH);
patch an LFO or envelope into MORPH and the organ swells on its own.

This does not change the first-bite plan: `INSTR_ORGAN` still ships buffer-free — it just ships
*with* its three-macro mapping instead of as a single frozen preset.

### 8.2 The one architectural decision: buffer-free vs. buffered

> **Decided (2026-06-03): yes — a `Voice` may carry a small per-voice delay line (~2 KB).** The
> organ's internal vibrato/chorus scanner wants one anyway, and the *same field* is reused across
> the whole buffered family below (pluck / piano / guitar), so committing once pays for all of
> them. This retires the "decide one thing first" gate — the §8.5 ordering still holds (ship the
> buffer-free drawbar/modal cores first), but the buffered engines are no longer blocked on a
> pending decision.

The only thing that divides "drop in tomorrow" from "decide one thing first" is whether a
voice has to carry a **delay-line buffer**.

**Buffer-free** — just a few phase accumulators; fits today's tiny `Voice` unchanged, no
architecture change, no API change. Band-limitable (cap harmonics at Nyquist → no aliasing):

| Instrument | navkit technique | Per-voice | Character |
|---|---|---|---|
| **Hammond B3 organ** | 9 additive drawbar sines + key-click noise + percussion + tonewheel crosstalk | ~160 B | ★★★★★ the bargain |
| **Electric piano** (Rhodes/Wurli/Clav) | 12 inharmonic sine modes + pickup nonlinearity (the growl) | ~280 B | ★★★★★ warm/vintage |
| **Mallet** (marimba/vibes/celesta) | 4 modal sines + strike weighting | ~60 B | ★★★★ celesta = music-box, very silent-movie |
| **Additive** (bell/strings/choir/brass) | up to 16 sines, per-partial decay | ~120 B | ★★★ versatile pad |
| **FM** (2–3 op) | DX-style bells/chimes | ~50 B | ★★★ cheap but expert to dial |

**Buffered** — needs a per-voice Karplus-Strong/waveguide buffer. Cost is **memory only**
(8 voices × 2 KB = 16 KB — trivial on desktop); CPU is fine (~30% worst case at 8 voices).
Say "a Voice may hold a small buffer" once and the whole physical-modeling family unlocks:

| Instrument | navkit technique | Per-voice | Notes |
|---|---|---|---|
| **Acoustic piano** | Stiff-Karplus + allpass dispersion chain | ~2–4 KB | **the literal "silent-movie pianist."** Dispersion is the secret that makes it sound expensive; buffer shrinkable to 1024 |
| **Guitar / harp / sitar** | Karplus-Strong + body biquads + jawari buzz | ~2.2 KB | one engine, many presets |
| **Bowed violin/cello** | dual waveguide + stick-slip friction | ~8 KB | gorgeous sustain; heaviest; optional |

### 8.3 What specifically sells the theatre-organ / silent-film vibe

- **Hammond organ + a simplified Leslie.** The Hammond sound is ~70% Leslie rotary, ~25%
  drawbars. The organ engine itself is ~160 B/voice. **The Leslie is an *effect*, not part of
  the oscillator** — that's how navkit factors it (`engines/effects.h`) and it's the right call
  here too: physically it's a speaker sim that sits *after* the tonewheels, it's a single
  ~1.5 KB buffer **shared across all voices** (amplitude + slight Doppler mod, slow/fast speed),
  not per-voice, and it'd run just as well on a Rhodes or a guitar. So: implement it as a
  standalone shared DSP block, decoupled from the engine.
  - *The open decision is exposure, not structure.* A general cart-facing **effects bus** is a
    new concept, and the §8 thesis is "no new concept." So for now the organ slot **auto-routes**
    through the Leslie and its speed rides a control we already have (a macro, or a dedicated
    slow/fast flag) — the cart never sees an "effect." When the §8.5-phase-4 character effects
    (chorus / drive / bitcrush) land as a real layer, this block folds straight into it.
  An organ *wants to sustain*, so it pairs naturally with the held-channel model (§6).
- **Acoustic piano** (KS + dispersion) for the pianist themselves.
- **Formant filter** for "singing"/choir/vocal-organ color — 4 bandpass peaks, ~512 B of
  vowel tables + ~8 floats/voice, reusing a state-variable filter. Bonus: that **same SVF is
  the resonant `filter()` from §5.5** — build one filter primitive, get the SID sweep *and*
  vowel formants from it.

### 8.4 SCW tables — a complementary cheap lever (not a replacement)

> **Partially SHIPPED (2026-06-04) as `wave_set` + `INSTR_USER0..3`:** four 64-sample
> single-cycle tables a cart can fill (and a `wave editor` cart that lets you DRAW them —
> with a live-morph drone, since the table is read per-sample). That's the cart-authorable
> half of this idea. The other half — a curated bank of ~600-sample SCWs *sampled from real
> instruments* (richer, less aliased) — remains open and would slot in the same way.

navkit also embeds **single-cycle waveforms** (one cycle of a real instrument as a small
float table; ~600 samples ≈ 2.4 KB each). A hand-picked bank of ~6 (≈8–10 KB) plus ~100
lines of phase-accumulator + cubic-interpolation playback gives instantly richer *static*
timbres. But SCW gives a rich *snapshot*, not the *living behavior* — the EP pickup growl,
the organ key-click, the piano's inharmonic dispersion — that makes the modeled engines feel
alive. Treat SCW as a cheap timbre-source lever (it could even feed an organ drawbar), not as
a substitute for the modeled instruments. Aliasing at high notes is acceptable/retro here.

### 8.5 How this reframes the roadmap (§7)

A curated instrument set is a *better* first move than the instrument bank, because it needs
**no new concept** — beginners keep calling `note`/`chord`/`tone`. The per-voice buffer is now
decided (§8.2), so the old buffer-free/buffered gate no longer drives the sequence; instead, lead
with the engine that proves the most for the least code. Suggested ordering, each phase
independently shippable:

1. **`INSTR_PLUCK`** (Karplus-Strong) + the fixed macro surface (§8.1.1 — 6 fns total, independent
   of engine count). Smallest engine, exercises the per-voice buffer, no companion effect — proves
   macros + engine-fork + buffer end to end.
2. **`INSTR_MALLET`/`INSTR_CELESTA`** — buffer-free modal; the music-box / silent-movie voice.
3. **`INSTR_ORGAN` + Leslie (shared) + resonant SVF filter** — the organ as a complete package
   (drawbars → scanner on the buffer → shared rotary). The SVF is the reusable primitive that also
   gives §5.5 and §8.3's formant.
4. **`INSTR_EPIANO` / `INSTR_PIANO` / `INSTR_GUITAR`/`INSTR_HARP`** — the rest of the buffered
   family, riding the path pluck validated. The pianist.
5. **Formant filter** + the **effects layer** (§8.10 — buses vs. master; reverb / delay / tape /
   leslie / wah, starting with one master reverb + the formalized bus concept).
6. **Optional:** bowed strings; and/or the SCW bank (§8.4).

### 8.6 Cons / watch-outs

- **Don't expose the *named* internal params** (no `organ_drawbar()`), but **do** expose the
  fixed three-macro set (§8.1.1) — harmonics / timbre / morph, identical across every engine.
  The moment a beginner sees "drawbar 4 = 6" the simplicity is gone; three normalized "make it
  more interesting" knobs keep it. Frozen `INSTR_*_JAZZ` presets are just baked macro positions.
- **Porting = lift the oscillator fn + its tuning constants.** The engines are self-contained
  (no malloc, no dependency on navkit's sequencer/effects bus), so each is a clean
  copy → shrink → rename into `sound.h`. Buffered ones bring their buffer — the only
  structural change.
- **Pairs with held channels (§6).** Sustained organ chord + Leslie underneath a fire-and-forget
  piano melody = a whole silent-film score using both the event and state models.

### 8.7 navkit source pointers (for when we port)

- Organ: `engines/synth.h` (`OrganSettings`) + `engines/synth_oscillators.h`
  (`processOrganOscillator`); plan in `docs/done/organ-engine-plan.md`
- Electric piano: `processEPianoOscillator`; Rhodes/Wurli/Clav presets in `instrument_presets.h`
- Acoustic piano: `processStifKarpOscillator`
- Guitar: `processGuitarOscillator`; Bowed: `processBowedOscillator`; Mallet: `processMalletOscillator`
- Leslie: `engines/effects.h`; Formant spec: `docs/vocoder-formant-effect.md`
- SCW data + embed tool: `engines/scw_data.h`, `tools/scw_embed.c`
- Calling convention: set `voice.wave`/`frequency`/envelope, then per sample
  `sample = processXxxOscillator(&voice, sampleRate)` — no heap, all state in-struct.

### 8.8 Port sketch — macro plumbing + three engines + the Leslie effect

Concrete shape for the first bite, grounded in today's `sound.h` (mix loop ~`:467–550`, the
stateless `sound_osc` `:176`, ctrl-kind dispatch `:360–437`). **Three touch points; everything
else is unchanged.**

**(a) Macros** — 3 floats on `Instrument`, current+target on `Voice` (mirrors
`flt_cutoff`/`cutoff_target`), two new ctrl kinds, six named functions, three lines in the
existing slew block (`:478`):

```c
// Instrument:  float harmonics, timbre, morph;            // 0..1, meaning is per-engine
// Voice:       float harm,timb,mor,  harm_target,timb_target,mor_target;
// kind 18 = instrument_macro(slot, which 0/1/2, val*1000);  kind 19 = note_macro(handle, which, val*1000)
void instrument_timbre(int slot, float x);   void note_timbre(int handle, float x);   // + harmonics / + morph
// slew:  v->harm += (v->harm_target - v->harm) * 0.002f;   // and timb, mor
```

Named API (only ever six functions), one `which`-indexed code path inside — rides the handle +
slew + CV machinery `note_cutoff` already uses; modrack gets three CV inlets for free.

**(b) Engine fork** — `sound_osc` keeps the 5 raw waves; engine ids dispatch to stateful
generators that read the macros. One line at `:534`:

```c
float s = (v->wave >= INSTR_ENGINE_BASE) ? sound_engine(v, v->harm, v->timb, v->mor)
                                         : sound_osc(v->wave, v->phase, duty, &v->noise_state);
```

**Buffer-free engines** (drop in now, no `Voice` growth — phase 1):

```c
// ORGAN — 9 drawbars derived from v->phase (fixed Hammond ratios). ~25 lines.
//   harmonics = registration  (mellow 8′  ↔  bright full spread)  — REG_MELLOW→REG_BRIGHT lerp
//   timbre    = brightness     (per-partial expf rolloff)
//   morph     = percussion strike + internal vibrato/chorus scanner depth
//               (drawbar core is buffer-free → ships first; the scanner taps the small per-voice
//                delay from §8.2 — the same buffer field the pluck/piano engines reuse)
float sound_organ(Voice *v, float harm, float timb, float mor);

// MALLET (marimba / vibes / celesta) — 4 inharmonic modal sines + a strike-noise transient. ~15 lines.
//   harmonics = inharmonicity  (wood ratios 1·3.9·9.2  ↔  metal/bell 1·4.1·10.7)
//   timbre    = strike hardness (weights upper partials + the noise-click amount)
//   morph     = ring length     (celesta short  ↔  vibe long; the long end adds the vibe motor tremolo)
float sound_mallet(Voice *v, float harm, float timb, float mor);   // decay from step_samples; reuses v->phase + noise_state
```

**Buffered engine** — `PLUCK` (the guitar/harp family) is the *first* engine that needs a
per-voice delay line: **this is the one §8.2 architectural decision, made concrete.**
Karplus-Strong = excite a delay line of length `SR/freq` with noise, run it through a feedback
lowpass:

```c
// Voice gains:  float ks_buf[KS_MAX]; int ks_len, ks_idx; bool ks_init;  // ~2KB/voice (KS_MAX≈1024 caps the low end)
// PLUCK — pitch comes from the buffer LENGTH, not v->phase.
//   harmonics = damping / sustain (short pluck  ↔  long ring)  — feedback coefficient
//   timbre    = pick brightness    (lowpass on the initial noise burst)
//   morph     = pick position      (comb/allpass tap into the line)
float sound_pluck(Voice *v, float harm, float timb, float mor);
```

Modeled engines treat the slot's ADSR as an **optional override, not a replacement** — they bake
their own decay (mallet ring, pluck damping), and a user ADSR layered on top flattens the very
motion that sells them (§10).

**(c) Leslie** — a shared *effect*, not per-voice (matches navkit's `engines/effects.h`, per
§8.3). Split organ voices into a sub-bus, run it through one shared rotary, add back:

```c
// #define LESLIE_LEN 512   // one shared ~2KB delay line — NOT per voice
typedef struct { float buf[LESLIE_LEN]; int widx; float horn_ph, drum_ph, speed, speed_target; } Leslie;
float leslie_process(Leslie *L, float in);   // 2 rotors → amplitude tremolo + doppler delay-mod;
                                              // speed slews slowly (the ~1s spin-up/down IS the magic)
// in the voice loop (:540):   if (v->wave == INSTR_ORGAN) organ_bus += out_s; else mix += out_s;
// after the loop   (:547):    mix += leslie_process(&leslie, organ_bus);
```

`leslie.speed_target` stays internal (auto, defaults slow) — no cart-facing effects API until the
§8.5-phase-4 effects layer, into which `leslie_process` folds cleanly (and `organ_bus`
generalizes to a per-instrument send). Mono today; stereo horn/drum panning is the upgrade when
§9's stereo question resolves.

**Cost:** macros ≈ 6 floats + 2 ctrl kinds + 6 one-liners; organ drawbars + mallet are
buffer-free; the organ scanner and the `PLUCK`/piano/guitar family share one small per-voice
delay line (§8.2 — decided). Leslie is a separate *shared* ~2KB buffer.

### 8.9 Candidate engine catalog (running wishlist)

The set we'd *like*, beyond the first-bite engines (§8.5). Adding one is mostly: port the
oscillator (§8.7), mark buffer-free vs. buffered (the per-voice delay from §8.2 is only for
Karplus/waveguide), and define its **3-macro mapping** — because the §8.1.1 discipline holds no
matter how the engine synthesizes, every row exposes the same `harmonics` / `timbre` / `morph`;
the table's only job is to say what those three mean for each. Grow it freely.

| Engine | navkit src (§8.7) | buffer | harmonics | timbre | morph | character |
|---|---|---|---|---|---|---|
| **Additive / sine** (bell, choir, brass, strings) | additive osc | free | # / spread of partials | spectral tilt (brightness) | per-partial decay + inharmonicity | rich multi-partial pads. (A *bare* sine is already `INSTR_SINE` — see the MT70 note below) |
| **FM** (2–3 op, DX) | `processFm…` | free | carrier:modulator ratio | mod index (FM amount) | feedback / 2nd-op depth | DX bells, chimes, e-pianos, clang. Macros *are* the cure for "expert to dial" |
| **AM / ring mod** | trivial (≈10 lines native) | free | modulator ratio | AM ↔ ring depth | modulator detune / wave | metallic, robotic, clangorous bells |
| **Voice / formant** | formant SVF + buzz (§8.3) | free (reuses SVF) | vowel (a→e→i→o→u) | breathiness / brightness | formant shift (size/gender) | choir "aah", vocal-organ, talkbox. Comes near-free with the §8.3 filter |

> **MT70 — resolved (verified in navkit `demo/songs/*.song`, 2026-06-03).** The "MT70" presets
> (Flute, Bells, Organ, Vibes, JzOrg2, …) are **all `waveType = sine`** — *not* a synthesis engine.
> Each is one pure sine wave shaped by ADSR + lowpass filter + (optional) vibrato; the names differ
> only in those settings. **So this needs no engine port — dreamengine already makes it** with
> shipped API. e.g. MT70 Flute ≈ `instrument(slot, INSTR_SINE, 50,100,5,120)` +
> `instrument_filter(slot, FILTER_LOW, 3500, 0)`. → Belongs as a **shipped preset / demo cart**
> (zero-setup `INSTR_*` presets, §5.4 / §10.1), not in this engine catalog.

> **Sine = Additive at `harmonics = 0`.** `INSTR_SINE` and the Additive engine aren't two things —
> additive *is* a sum of N sine partials, so one partial is a sine. That means the MT70/sine family
> are the harmonics-≈0 corner of the Additive row above, and the whole bank is a path through its
> three macros: **harmonics** (0 = pure sine → up = organ/string body), **timbre** (spectral tilt /
> brightness — the lowpass), **morph** (inharmonicity: integer-ratio = organ/flute ↔ stretched =
> bell/metallic, optionally + per-partial decay). So: Flute ≈ (harm 0, mellow, harmonic);
> Organ ≈ (harm up, harmonic); Bells ≈ (mid, bright, **inharmonic**); Vibes ≈ (inharmonic + fast
> decay). Ship the bare sine as `INSTR_SINE` now (no port); when the Additive engine lands it
> subsumes the family with live macros — same engine, just `harmonics` turned up off zero.

*Format for adding more:* just name the sound + a reference patch/track if you have one; the
buffer flag, navkit source, and macro mapping get filled in here.

### 8.10 Effects layer — buses vs. master (the §8.5-phase-4 plan)

> The effects wishlist + the routing model. Still **deferred** to §8.5 phase 4 (after the first
> engines ship) — begin small. The Leslie (§8.3/§8.8) is the first instance and sets the pattern.

**The routing model: unify on "bus"; master is just the default bus.** Rather than two concepts
(master FX vs. bus FX), there's *one* — a **bus** carrying a small fixed FX chain — and "master"
is bus 0:

- An instrument slot names a **bus** (default = master).
- **Aux buses (1..N):** voices route in → the bus's FX chain → summed into master.
- **Master bus (0):** the full sum → master FX → output.

This gives both behaviours from one mechanism: Leslie/wah on a per-instrument **aux** bus (only the
organ gets the rotary, only the lead gets wah); tape/reverb/limiter on **master** (everything
shares them). The `organ_bus` already in the §8.8 sketch *is* aux bus #1 — the general version just
lets any instrument pick a bus and any bus carry a chain. Generalizing §8.8's organ sub-bus:

```c
float bus[NBUS];                          // small fixed N; accumulators zeroed per sample
// voice loop:   bus[v->bus] += out_s;             // was: organ_bus += … / mix += …
// post-loop:    for (aux b) master += fx_chain(b, bus[b]);
//               out[i] = fx_chain(MASTER, master + bus[MASTER]);
```

**Surface discipline** (same as macros/Leslie — tiny, global-first): the smallest cart-facing step
is global **master** effects — `reverb(amount)`, `delay(ms, fb)`, `drive(amount)` — i.e. "effects
on master," matching the global `filter()` precedent (§5.5). Per-bus routing
(`instrument_bus(slot, b)` + per-bus setters) is the *next* increment, when a cart wants the lead
on its own wah without washing the whole mix.

**The wishlist.** "Shared" below means *one* processor instance (not 8 private ones), **not**
"applied to everything" — you pick *which* sounds feed it by routing them to a bus. So a delay on
just the lead = route the lead to an aux bus carrying delay (`instrument_bus(LEAD,1)`); everything
else stays dry. That's per-*instrument*, achieved with one shared delay line + bus routing — the
buffer-hungry effects (delay/reverb/tape) are shared-and-routed rather than per-voice purely on
memory cost (a per-voice echo line would be ~384 KB × 8 voices for an effect nobody wants 8 of).
The cheap exception is **wah** — it's just the per-voice SVF, so it genuinely *is* per-note.

| Effect | granularity | DSP | buffer | home | insert/send | notes |
|---|---|---|---|---|---|---|
| **Reverb** | bus / master | Freeverb (8 comb + 4 allpass) | shared ~20–40 KB | master / aux | send (dry+wet) | the big "space" |
| **Delay / echo** | bus / master | delay line + feedback + tone | shared (~SR·2) | aux / master | send | route one instrument to it; tempo-syncable to the beat clock |
| **Tape** | master | wow/flutter (modulated delay) + soft-clip drive + HF rolloff | shared small | master | insert | lo-fi character over the whole mix |
| **Leslie** | bus | 2 rotors: tremolo + doppler delay-mod (§8.3) | shared ~2 KB | aux (organ) | insert | already specced |
| **Wah** | **per-voice** | resonant SVF bandpass swept by LFO / envelope / expression | none — reuses SVF | per voice | insert | **4th use of the one SVF** — filter (§5.5) + formant (§8.3) + wah, build it once |

**Begin small:** when this layer opens, the minimal first bite is **one master reverb + formalizing
the bus concept** (so the Leslie stops being a hardcoded organ special-case). Delay / tape / wah
follow. Stereo (a §9 open question) matters more here than for engines — reverb/delay/Leslie are
where width lives — so resolve stereo before or alongside this layer.

## 9. Open questions

- ~~**Channels vs. handles** for real-time~~ — **Resolved (shipped): handles won.**
  `note_on()` returns a slot+generation handle; stale handles silently no-op. See
  [`held-notes.md`](held-notes.md).
- ~~**Filter scope:** global-only or per-channel?~~ — **Resolved (shipped): per-instrument.**
  Each slot carries its own SVF (`instrument_filter`); 8 SVFs are negligible on desktop.
- ~~**Where does duty live?**~~ — **Resolved (shipped): on the instrument** (`instrument_duty`)
  **+ live** (`note_duty`). The `INSTR_PULSE_*` named shortcuts were not added — folded into
  the zero-setup preset-bank idea (§5.3) if it ever ships.
- **Stereo?** Everything is mono today. Panning is cheap to add but doubles the mix and
  raises "is it worth the surface?" — likely low leverage for this audience.
- **Tracker UI**: VISION mentions a sound tracker to match the sprite editor.
  *Direction (2026-06-04):* leaning **PICO-8-style and prototyped as a CART** — draw the
  pitch contour over steps, toggle wave/vol per step — which needs **no new engine API**
  (see §5.6). The cart prototype is the cheap way to find out if the editor earns a place
  before any engine-side bank API is built.
- **Voice budget:** with held channels reserving voices, do we raise `SOUND_VOICES`, or
  is 8 still plenty once some are long-lived?
- ~~**Per-voice buffers (§8.2):**~~ **Resolved (2026-06-03): yes** — a `Voice` may carry a small
  ~2 KB delay-line buffer. Wanted by the organ scanner and reused across the whole Karplus-Strong /
  physical-modeling family (pluck / piano / guitar). See §8.2.
- **Curated instruments vs. instrument bank:** ship finished `INSTR_*` presets from navkit
  (§8) *and/or* build the cart-authorable bank (§5.4)? They're complementary, but which is
  the headline for the audience — sounding great out of the box, or authoring your own?
- **One filter primitive, two uses:** commit to a single state-variable filter that serves
  both the SID-style `filter()` sweep (§5.5) and the vowel formant bank (§8.3)?

---

## 10. The four axes, one container — SHIPPED

> **Status: shipped** (originally HEAD `141a0a7`; updated since). All four axes below are
> live on the raw waveforms, bundled per instrument slot: `instrument()` (ADSR),
> `instrument_duty()`, `instrument_lfo()`, `instrument_filter()` — **plus a fifth axis
> since: `instrument_env()` (×2 mod-envelopes, §11)**. Deltas from the original sketch:
> **3 LFOs per slot**, so `instrument_lfo()` takes a `which` (0–2) index; **held voices
> shipped as `note_on()` + handle-based `note_*` setters** (see `held-notes.md`), not the
> `ch_*` channel API sketched in §10.3 — same capability, handle-safe. The `dream synth`
> cart now plays real held notes (hold-to-sustain) with live filter/LFO control and the
> §11 filter-contour/pitch-env editors. Carts: `instruments`, `lfo`, `filter`,
> `dream synth`, `filterenv`, `pitchenv`. §2 ("where we are now") was refreshed 2026-06-04
> and is the current surface map. (The original proposal text is kept below as rationale.)

A concrete API sketch for the whole expressive space. The realization that drives it: the
range we want isn't a *list* of features — it's **four orthogonal axes that multiply**, and
an **instrument is just the container that bundles them**. It is not a fifth axis.

```
instrument (slot) = timbre × envelope × LFO × filter
       └─ played by → note() / chord() / ch_play()
```

| Axis | What it controls | Lives on |
|---|---|---|
| **Timbre** | which oscillator/engine (raw wave → pulse → §8 modeled) | instrument `wave` field |
| **Envelope** | amplitude shape over time (ADSR, or the engine's baked default) | instrument |
| **LFO / modulation** | motion over time — *one routable LFO* | instrument |
| **Filter** | resonant SVF (subtractive **+** formant) | instrument (or global — see open Q) |

8 timbres × a few envelopes × a few LFO routings × a few filter modes is enormous range from
~4 small concepts — exactly the §1 thesis. Crucially, the four axes don't all *compose*: duty
is pulse-only (a no-op on organ/piano), and a user ADSR layered on a §8 modeled instrument
tends to flatten the very motion that makes it sound expensive. So the modeled engines treat
ADSR as optional override, never replacement (see below).

### 10.1 The easy path stays zero-setup

`instr` becomes a **slot id**, not a waveform id. Slots 0–4 are pre-filled with today's waves
+ the declick envelope; presets we ship (`INSTR_PLUCK`, `INSTR_PAD`, `INSTR_ORGAN`, …) are just
pre-baked slots. Every existing cart keeps working unchanged, and most carts never define an
instrument at all.

```c
note(60, INSTR_SAW, 5);                      // raw wave — unchanged
note(60, INSTR_PLUCK, 5);                    // a baked preset, zero setup
chord(48, CHORD_MAJ7, INSTR_ORGAN, 5);       // a §8 modeled engine, same call shape
```

### 10.2 Defining an instrument — base call + one modifier per axis

```c
// axis 1+2: timbre (which engine) + ADSR envelope.  a/d/r in ms, sustain 0..7
void instrument(int slot, int wave, int attack_ms, int decay_ms, int sustain, int release_ms);

// axis 1 detail: pulse width (only meaningful for pulse/square timbres; no-op otherwise)
void instrument_duty(int slot, float duty);                 // 0..1

// axis 3: ONE routable LFO — this single call IS vibrato / tremolo / PWM / wah
void instrument_lfo(int slot, int dest, float rate_hz, float depth);
//   LFO_PITCH    depth = semitones   → vibrato
//   LFO_DUTY     depth = 0..1        → PWM / duty sweep
//   LFO_VOLUME   depth = 0..1        → tremolo
//   LFO_CUTOFF   depth = Hz          → filter wah

// axis 4: resonant state-variable filter (the §5.5 SID filter AND the §8.3 formant)
void instrument_filter(int slot, int mode, int cutoff_hz, int resonance);   // res 0..15
//   FILTER_OFF / LOW / HIGH / BAND / NOTCH / FORMANT
```

Four functions define *any* instrument. Each call is short, maps to exactly one axis, and is
optional. The LFO destination-dependent `depth` units are a small wart — named sugar wrappers
(`instrument_vibrato`, `instrument_tremolo`, …) are an option if it bites, at the cost of more
surface.

```c
void init(void) {
    instrument(5, INSTR_PULSE_25, 2, 80, 4, 120);   // timbre + ADSR
    instrument_duty(5, 0.25f);                       // thin pulse
    instrument_lfo (5, LFO_DUTY, 4.0f, 0.15f);       // ...that shimmers (PWM)
    instrument_lfo (5, LFO_PITCH, 6.0f, 0.2f);       // ...with a touch of vibrato
    instrument_filter(5, FILTER_LOW, 1800, 9);       // ...rounded off
}
void draw(void) { if (every(60)) note(60, 5, 6); }
```

One timbre × ADSR × two LFO routings × a filter, from four lines — the multiply in action.

### 10.3 Real-time control — held channels play an instrument live (§6)

The instrument is the *template*; a channel is a held voice you write to per-frame. Same four
axes — the difference is the *game* is the modulator instead of the LFO.

```c
void ch_play  (int ch, int midi, int instr, int vol);   // start held (vol 0 = silent)
void ch_pitch (int ch, float midi);                      // float → glides (slewed, §6.2)
void ch_vol   (int ch, int vol);                         // 0 gates off, keeps the channel
void ch_duty  (int ch, float duty);
void ch_cutoff(int ch, int hz);
void ch_stop  (int ch);
```

```c
void init(void)  { ch_play(0, 40, INSTR_SAW, 0); }                  // start silent
void update(void){ ch_pitch(0, 36 + speed*0.6f);                    // engine rev
                   ch_vol  (0, speed > 0.1f ? 5 : 0); }             // fade at a stop
```

### 10.4 How the §8 engines slot in

`INSTR_ORGAN`/`INSTR_EPIANO`/… are just more `wave` values — no new API, no new concept:

```c
instrument(6, INSTR_ORGAN, 0, 0, 7, 80);    // instant attack, sustain, fast release
```

Rule that resolves the ADSR-vs-baked-envelope conflict: **all-zero ADSR = use the engine's
baked default.** A non-zero user ADSR overrides *amplitude only*, never the internal timbral
motion (key-click, pickup growl, dispersion). `instrument_duty` is a no-op on non-pulse engines.

### 10.5 Surface count and the one open decision

The entire grid costs: **4 define-calls + 6 channel-calls + a handful of `INSTR_*` / `LFO_*` /
`FILTER_*` constants.** Tiny, for the whole space.

The one real decision baked into this sketch: **filter scope — per-instrument (as above) or one
global master bus?** Per-instrument composes cleanly and gives formant vowels for free, but costs
8 SVFs instead of 1; global is cheaper and more SID-authentic but can't differ per voice. On
desktop 8 SVFs is negligible, so this sketch leans per-instrument — but it's the live question
from §9 ("filter scope", "one filter primitive, two uses") and wants a deliberate call before
building.

## 11. Modulation envelopes — the second EG the synth is missing

> **Status: AGREED, next sound feature — built BEFORE the §8 instrument engines.** Surfaced while
> ear-testing navkit's pluck (2026-06-03): the **filter envelope** and **pitch envelope** are
> sonically huge, and they are not two features — they are **one primitive** we don't have yet.
> Decided to ship this first; it then makes every §8 engine (and every raw wave) better for free,
> and keeps the engine macros (§8.1.1) clean.

### 11.1 The realization

We ship exactly one envelope today — the **amplitude ADSR** (`instrument()`, driving volume via
`sound_adsr_gated`). navkit's "filter env" and "pitch env" are nothing exotic: they are **second
and third envelope generators routed to a destination other than volume.** That's the textbook
subtractive layout — every Minimoog/MS-20 has a VCA envelope *and* a VCF envelope.

And we **already have the routing pattern**: `instrument_lfo(slot, which, dest, rate, depth)` is a
*cyclic* modulator pointed at a destination (`LFO_PITCH/DUTY/VOLUME/CUTOFF`). An envelope is its
**one-shot twin** — same routing idea, fires once per note instead of looping. So we don't invent a
concept; we add the missing half of one we have.

| Envelope | Routed to | navkit shape | amount units | what it buys |
|---|---|---|---|---|
| **amp** *(shipped)* | volume | ADSR | 0..1 | loudness contour |
| **filter** *(missing)* | cutoff | **AD** | Hz, **bipolar** | the pluck "pew / dwow" sweep |
| **pitch** *(missing)* | pitch | **D** (+curve) | semitones, **bipolar** | drum punch, attack snap, zaps |

navkit grounding: filter env is `cutoff = base + amount·level`, `level` a bare AD (attack→1, linear
decay→0; `synth.h:3516–3542,3603`). Pitch env is decay-only — pitch starts `amount` semitones off
and settles back over `decay`, with a linear/exponential `curve` (`synth.h:3023–3051`); the
exponential curve is what makes a kick *punch*.

### 11.2 The API — one routable call, mirroring `instrument_lfo`

```c
// the one-shot twin of instrument_lfo(). 2 env slots per instrument (which 0..1) so a
// filter env and a pitch env can run at once. amount units depend on dest (like LFO depth).
void instrument_env(int slot, int which, int dest, int attack_ms, int decay_ms, float amount);
//   ENV_CUTOFF   amount = Hz         (bipolar; + opens then closes — the pluck sweep)
//   ENV_PITCH    amount = semitones  (bipolar; + starts sharp and settles — punch/snap/zap)
//   ENV_DUTY     amount = 0..1       (PWM sweep — near-free; ship or defer)
// NB: there is deliberately NO ENV_VOLUME — that is the amp ADSR (instrument()). Don't duplicate it.
```

- **Count:** 1 function · 2 destinations to ship (`ENV_CUTOFF`, `ENV_PITCH`; `ENV_DUTY` optional) ·
  2 slots per instrument. ("2 envelopes — filter and pitch" = the two destinations; the slots are
  what let them coexist.)
- **Shape: AD now** (attack + decay, no sustain/release). AD covers every killer use (pluck filter,
  pitch blip, drum punch). Full ADSR only matters for routing a *held pad's* filter to stay open
  while the key is down — a later widening, not now.
- **Decay curve: exponential by default** — it punches harder than linear and is what sells the
  kick / the pluck attack. (navkit's pitch env exposes a curve knob; we hardcode exp to start.)
- **Held-note twin:** `note_env(handle, which, dest, attack_ms, decay_ms, amount)` mirrors
  `note_lfo` — gives modrack a per-voice env modulator and live control for free.

### 11.3 Why this is the right order (engines wait)

It **decouples two layers** that were getting tangled in the pluck-macro discussion:

- **Engine macros** (`harmonics`/`timbre`/`morph`, §8.1.1) → the *oscillator's own character*.
- **Modulation layer** (amp env + these mod-envelopes + LFOs + filter) → the standard subtractive
  wrapper that works on *any* engine or raw wave.

Concretely it **resolves the pluck `morph` fight**: with the filter sweep living in its own envelope
layer, `morph` no longer has to be "filter-env decay" — it can be **inharmonicity** (harp↔metallic),
the engine-character knob that also pre-builds the acoustic-piano engine. You get the pluck-bass
"pew" *and* the character knob, in different layers. Hence: build mod-envelopes first, then the
pluck engine sits cleanly on top.

### 11.4 Implementation shape (grounds in today's `sound.h`)

Reuses machinery that already exists — no new subsystem:
- `Instrument`/`Voice` gain a small `env[2]` (dest, a_samp, d_samp, amount, + per-voice
  stage/timer/level) — mirrors the existing `lfo_*[3]` arrays.
- New ctrl kinds for `instrument_env` (define) + `note_env` (live), next free after the held-note
  set (kinds ≥18). The §8.8 macro ctrl-kinds renumber to sit after these.
- The cutoff destination feeds the **same per-sample cutoff** the SVF + `LFO_CUTOFF` already drive
  (`sound.h:518,528,535`): `cutoff += env_level·amount`, summed alongside the LFO term — one extra
  addend, not a new code path. The pitch destination multiplies `freq` next to `pitch_mul`
  (`sound.h:525,542`).
- Per-note retrigger: stamp env stage/timer at note-on in `sound_setup_note` (`sound.h:262`).

**Demo carts (one each, the "tweak rig" pattern):** a `pitchenv` cart (drum punch + zap) and a
`filterenv` cart (pluck-bass "pew" + sweep), each with attack/decay/amount sliders and a keyboard —
mirroring `heldnotes`/`instruments` cart style. These double as the by-ear tuning rig.

---

## 12. Instrument gaps — found by building the radio family (2026-06-05)

**Empirical, not speculative:** this list is what was actually missing while
building and planning the ten radio stations (bossa → house) plus the next
batch's designs (gamelan, Italo, Steely Dan, the Doors). Ranked by how often
the wall was hit, and scored against §1's leverage rule.

1. **Voice choke groups.** The family's one *documented* infidelity:
   `tr808.c`'s header admits "real 808 closed hats CHOKE the open hat; our
   one-shot notes can't cut each other," and house.c inherits the flaw (the
   offbeat open hat rings through the next closed hat). Every drum-machine
   genre wants it. Tiny surface — `instrument_choke(slot_a, slot_b)`: a note
   on `a` kills sounding notes on `b`. High leverage for one call.

2. **A second oscillator per voice** (detune pair, or 2-op FM). The big
   timbre unlock, felt three distinct ways:
   - **Electric piano doesn't exist.** The guide's recipe ("SINE + tremolo")
     is a placeholder, not a Rhodes. Italo disco, Steely Dan/AOR and any
     deeper citypop all *center* on epiano/DX keys — the planned batch runs
     straight into this wall.
   - **Bells and metal are out of reach.** FM's inharmonic partials are
     exactly gamelan's bronze (ombak beating *inside* one voice) and
     exotica's vibraphone.
   - **Pads cost double.** ambient.c burns 4 of `SOUND_VOICES 8` on one
     chord; house.c cut its string machine to 2 voices for budget. An
     instrument-level **unison-detune flag** would halve every pad's
     polyphony cost — arguably the cheapest 80% of this item.

3. **A plucked-string wave (Karplus-Strong).** Every guitar on the dial is a
   filtered TRI envelope: bossa's nylon, jangle's chorus-wobble, the planned
   Krieger drone guitar. One wave type covers guitar / harp / koto — and KS
   variants do metallic percussion too. Famously ~20 lines.

4. **Finer dynamics.** `vol 0..7` is coarse: ghost notes jump audibly
   between 1 and 2, and satie.c — solo piano, where *touch is the whole
   arrangement* — strains hardest against 8 steps. Even 0..15 would let
   grooves breathe. (Mind the API-churn cost: every existing call site.)

5. **The contentious one: a tiny shared room.** The family is bone dry.
   dub.c proved echo can be *scheduled notes* — but diffuse tails can't be
   scheduled. satie/ambient/exotica would transform with one fixed mono
   plate + per-slot send level. Counter-argument: dryness IS the chip
   identity. A taste decision, not a clear gap — flagging, not advocating.

**What was NOT missing**, for the record: the per-note modulation system is
complete in practice — house.c's entire filter ride was `note_cutoff` +
`note_res` with zero friction, and the env/LFO routing covered every recipe
in game-music.md so far. Also `wave_set`/`INSTR_USER0..3` went **unused by
all ten stations** — a drawn single-cycle wave could fake organ drawbars or
a nasal clav today; worth a reminder in the guide before adding anything.
