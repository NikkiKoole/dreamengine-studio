# dreamengine — audio design notes

A living document for the sound system: what exists in code today, where it sits
relative to the chips it evokes, the ideas on the table, and a guiding rule —
**get as much expressivity as possible from as few primitives as possible.**

Companion to [`../VISION.md`](../VISION.md). Implementation lives in `runtime/sound.h`
(header-only, included once from `runtime/studio.c`); the public surface is the `sound`
section of `runtime/studio.h`. **Genre: design exploration** — rationale + roadmap for the
sound subsystem; status lives in [`../STATUS.md`](../STATUS.md), settled choices in
[`../decisions/`](../decisions/README.md) (e.g. [0003 code-first sound](../decisions/0003-code-first-sound.md)).

## The audio doc family — where does an audio idea go?

This file is the hub; substantial investigations get their own file (an
investigation longer than ~a screen should be spun out, with a one-line pointer
left here — that rule is what keeps this doc from regrowing).

| An idea about… | goes to |
|---|---|
| engine/synthesis wants, parameter placement, gaps | **here** (§12 gaps, §4 placement, §9 open questions) |
| a new modeled instrument (organ, reed, membrane…) | [`instrument-engines.md`](instrument-engines.md) — the navkit port program: §8.9 catalog, §8.8.2 playbook, §8.10 effects layer |
| a musical style / generative recipe | [`../guides/game-music.md`](../guides/game-music.md) (recipes, brain catalog, style cheat-sheet) |
| a new radio station to build | [`future-stations.md`](future-stations.md) — the candidates parking lot + build-order axes |
| a per-station timbre swap / engine retrofit | [`radio-instrument-options.md`](radio-instrument-options.md) |
| genre racks / generate-play-export / song.h | [`tinydaws.md`](tinydaws.md) |
| holding + driving notes live (the shipped spec) | [`held-notes.md`](held-notes.md) |
| modrack / patching | [`modular-synth.md`](modular-synth.md) |
| recording the player, looping it back | [`input-recording-looper.md`](input-recording-looper.md) |
| a cart to build (instruments, toys) | [`cart-library-direction.md`](cart-library-direction.md) |
| settled "why we (didn't)" | decisions [0003](../decisions/0003-code-first-sound.md) code-first · [0013](../decisions/0013-cut-music-api.md) music() cut · [0015](../decisions/0015-effects-are-recipes-not-primitives.md) effects-are-recipes |

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

> **Refreshed 2026-06-07**, after the 2026-06-05 wave landed: the first three modeled
> engines (`INSTR_PLUCK/MALLET/FM` + the six macro functions —
> [`instrument-engines.md`](instrument-engines.md)), `instrument_choke` (§12),
> drive / master soft-clip / the echo bus (§17), and the 8→16 voice flip (§15).
> Prior refresh 2026-06-04 (the §11 mod-envelopes; `schedule_hit`, `wave_set` +
> `INSTR_USER0..3`; `music()` cut — [decision 0013](../decisions/0013-cut-music-api.md)).
> This is the authoritative map of the shipped sound surface: **45 functions +
> 39 constants** (recount with `node tools/api-usage.js` when in doubt). §5–§11 hold the
> rationale for how it got here; STATUS item 5 holds what's next (organ, SFX authoring).

**Engine**
- 16 voices (`SOUND_VOICES` — 8 until 2026-06-05, raised after the §15 starvation
  measurement), 44.1 kHz, **mono**, software-mixed (sum × 0.2 gain each, then the §17
  **master soft-clip**: linear below a ±0.8 knee, tanh-shaped above — quiet mixes
  bit-identical, hot mixes glue instead of slamming a wall). Runs on raylib's
  audio-stream callback (the **audio thread**).
- Voice allocation: first free → else a **non-held** voice → else voice 0. Held notes
  are stolen last — they're meant to last. Ringing engines get a ~3ms steal-declick tail.
- Held-note **handles** pack slot (low `SOUND_HANDLE_BITS` = 4 bits, guarded by a
  `_Static_assert` against `SOUND_VOICES`) + generation; a stale handle (its voice was
  stolen or the slot reused) silently no-ops. 16 handle slots. See `held-notes.md`.

**The surface, in four layers**

1. **Just make sound — zero setup.** `note` `hit` `chord`(+9 `CHORD_*`) `strum`
   `tone`(+6 `SCALE_*`) `schedule` `schedule_hit` `sfx`. Slots 0–4 come pre-filled
   with the raw waves (`INSTR_SQUARE/SAW/TRI/NOISE/SINE`), so the first
   `note(60, INSTR_SAW, 5)` needs nothing else. `schedule_hit` (delay **+** duration) fills
   the note-call quadrant that lets a cart sequence sub-frame sfx steps sample-accurately.
2. **Design an instrument.** A slot (5–31) is the container; one call per axis —
   `instrument` (timbre + **amp ADSR**) · `instrument_duty` (pulse width) ·
   `instrument_lfo` (**×3**, cyclic modulation) · `instrument_env` (**×2**, one-shot AD
   modulation, bipolar amount — §11) · `instrument_filter` (resonant SVF, 5 `FILTER_*`
   modes) · `instrument_drive` (post-filter tanh, §17) · `instrument_echo` (send to the
   one echo bus, §17) · `instrument_choke` (new note on a kills b — hats, §12).
   Timbre itself is extensible two ways: **`wave_set` + `INSTR_USER0..3`** are four DRAWN
   single-cycle waveforms (64 samples, live-morphable — the §8.4 SCW idea; draw them in
   the `wave editor` cart), and **`INSTR_PLUCK/MALLET/FM`** are modeled engines
   ([`instrument-engines.md`](instrument-engines.md)), each shaped by the three macros
   `instrument_harmonics/timbre/morph` — all work anywhere a wave id does.
3. **Play it live — held notes.** `note_on`→handle, `note_off`, `note_off_all`;
   performance gestures `note_pitch` / `note_vol` / `note_glide`; live twins of the
   defines: `note_duty` `note_lfo` `note_env` `note_filter` `note_cutoff` `note_res`
   `note_drive` `note_echo` `note_harmonics` `note_timbre` `note_morph`.
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

Three newer parameter families (2026-06-05) ride the define + live halves of this
pattern but are deliberately **not** LFO/env destinations: the engine macros
(`harmonics/timbre/morph` — modrack patches CV into the live setters instead),
`drive`, and the per-slot `echo` send.

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
- Main thread → audio thread via a 512-entry **request ring buffer**; kinds 0–28 cover
  play (0–2), define (3–6, 18, 20 = `wave_set` packed 4 samples/request, 21 macros,
  23 choke, 24 drive, 26–27 echo), and held-note live control (7–17, 19, 22, 25, 28).
  Delayed requests (`strum`/`schedule`/`schedule_hit`) sit in a 64-entry holding pen on
  the audio thread.
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

> **Status: HISTORICAL — the 2026-05 gap analysis; every ❌ below has since shipped.**
> Kept because this table is what motivated §5–§11. The ❌s became: variable duty + PWM
> (`instrument_duty`/`LFO_DUTY`), full ADSR (`instrument`), resonant multimode filter
> (`instrument_filter`, per-slot), vibrato + pitch sweep (`instrument_lfo`/`instrument_env`)
> — and voices are 16 now (§15), with three modeled engines on top
> ([`instrument-engines.md`](instrument-engines.md)). Still true today: no ring mod / hard
> sync (AM/ring sits in the engine catalog, instrument-engines §8.9) and no PCM samples
> (the FM-clang hat workaround covers the 909 case). The honest 2026 comparison is no
> longer SID/NES — §15 calls it SNES/Amiga-class. Current surface: §2.

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

## 8. Borrowing rich instruments from navkit → moved

**Moved to [`instrument-engines.md`](instrument-engines.md)** (2026-06-07 — it had grown
to 40% of this file). **Section numbers are preserved there verbatim**, so every `§8.x`
reference in this file and elsewhere (the §8.1.1 macro discipline, the §8.2 buffer
decision, the §8.5 phase ordering, the §8.8.2 engine-shipping playbook, the §8.9
candidate catalog, the §8.10 effects layer) resolves in that doc, same numbers.

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
- ~~**Voice budget:** with held channels reserving voices, do we raise `SOUND_VOICES`, or
  is 8 still plenty once some are long-lived?~~ — **Resolved (2026-06-05, measured): 16.**
  8 was hitting voice starvation *and* handle-slot exhaustion on the dense carts; the flip
  is measured-safe with the 0.2 scale kept. Full experiment + the handle-bits landmine: §15.
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

1. **Voice choke groups.**
   **→ SHIPPED 2026-06-05** as `instrument_choke(slot_a, slot_b)` (SR_INSTR_CHOKE,
   kind 23): a new note on slot `a` instantly kills any sounding voice on slot `b`
   (with steal-declick). `tr808.c` and `house.c` both wire
   `instrument_choke(SL_HATC, SL_HATO)` — the documented infidelity is closed.
   The bitmask design (`choke_mask` on `Instrument`) lets one slot choke several
   others, covering future cymbal-choke and hi-hat-pedal scenarios.

2. **A second oscillator per voice** (detune pair, or 2-op FM). The big
   timbre unlock, felt four distinct ways.
   **→ SPLIT 2026-06-05 — this gap conflated two architecturally opposite things:**
   - **2a — an FM *engine*** (`INSTR_FM`): the second oscillator is an *inaudible*
     phase-wiggler sealed inside one engine's sample function — you only ever hear
     the carrier. Self-contained behind one wave id, buffer-free, zero new API,
     zero shared plumbing. Covers the epiano/bell/brass/clang needs below.
     Design: §8.8.3. **Promoted to next engine.**
   - **2b — second-oscillator *infrastructure***: a second *audible* source on the
     generic voice path (Juno saw+square mix, the unison-detune flag) — touches
     `Voice`/mix-loop semantics for every wave and needs new API surface. **Stays
     deferred** until the Juno-60 cart defines the requirements (§14 gap A).
   The original four motivations, now tagged by which half they need:
   - **[2a] Electric piano doesn't exist.** The guide's recipe ("SINE + tremolo")
     is a placeholder, not a Rhodes. Italo disco, Steely Dan/AOR and any
     deeper citypop all *center* on epiano/DX keys — the planned batch runs
     straight into this wall.
   - **[2a] Bells and metal are out of reach.** FM's inharmonic partials are
     exactly gamelan's bronze (ombak beating *inside* one voice) and
     exotica's vibraphone.
   - **[2b] Pads cost double.** ambient.c burns 4 of `SOUND_VOICES 8` on one
     chord; house.c cut its string machine to 2 voices for budget. An
     instrument-level **unison-detune flag** would halve every pad's
     polyphony cost — arguably the cheapest 80% of this item.
   - **[2b] Juno-60 DCO mixer.** The Juno mixes SAW + SQUARE simultaneously per
     voice — that's a second oscillator with a different waveform, not an
     FM pair. The unison-detune flag alone doesn't cover it (detune = same
     wave, slightly offset pitch). A static approximation is possible via a
     drawn SCW (`INSTR_USER`) blending saw and pulse harmonics into one
     table; a live-mixable version needs the full second-oscillator path.
     The Juno is a concrete build target that makes both the unison-detune
     flag and the second-osc path more urgent (see §14).

3. **A plucked-string wave (Karplus-Strong).** Every guitar on the dial is a
   filtered TRI envelope: bossa's nylon, jangle's chorus-wobble, the planned
   Krieger drone guitar. One wave type covers guitar / harp / koto — and KS
   variants do metallic percussion too. Famously ~20 lines.
   **→ SHIPPED 2026-06-05** as `INSTR_PLUCK` + the §8.1.1 three-macro surface
   (six functions, ctrl kinds 21/22), showcased by the `pluck` cart. The
   jangle/bossa retrofit (the ear test against shipped stations) is the
   open follow-up.

4. **Finer dynamics.** `vol 0..7` is coarse: ghost notes jump audibly
   between 1 and 2, and satie.c — solo piano, where *touch is the whole
   arrangement* — strains hardest against 8 steps. Even 0..15 would let
   grooves breathe. (Mind the API-churn cost: every existing call site.)

5. **The contentious one: a tiny shared room.** The family is bone dry.
   dub.c proved echo can be *scheduled notes* — but diffuse tails can't be
   scheduled. satie/ambient/exotica would transform with one fixed mono
   plate + per-slot send level. Counter-argument: dryness IS the chip
   identity. A taste decision, not a clear gap — flagging, not advocating.

**SFX editor as percussion modeler — a partial bridge for gap 2/sample sounds.**
The SFX editor (`sfxed.c`, §5.6) exports per-step pitch + volume + filter-CUT
arrays as C code. That's more sculpting power than a single `instrument()` call —
you can draw the exact filter-close curve and volume shape of a hit. The TR-909
hihat is a real-world case: ROM samples in the hardware, but the *character* is a
fast-closing highpass burst. Authoring it in the SFX editor (NOISE wave, steep CUT
lane, short vol decay) and calling `sfx(n)` in fire() is not a perfect reproduction
but it's meaningfully closer than a static instrument definition. This is a useful
technique for any future drum machine that needs sounds more sculpted than a fixed
ADSR. See §14 for the 909 build-readiness assessment.

**What was NOT missing**, for the record: the per-note modulation system is
complete in practice — house.c's entire filter ride was `note_cutoff` +
`note_res` with zero friction, and the env/LFO routing covered every recipe
in game-music.md so far. Also `wave_set`/`INSTR_USER0..3` went **unused by
all ten stations** — a drawn single-cycle wave could fake organ drawbars or
a nasal clav today; worth a reminder in the guide before adding anything.
(Done — the guide now has a "Drawn waves" recipe section. But don't read
that observation as "the SCW bank is the next move" — it isn't; the
investment call is laid out in §13.)

## 13. The lay of the land — SCWs vs. engines (decided 2026-06-05)

§8.4 (single-cycle waves) and §8.5–8.9 (modeled engines) keep getting
discussed in the same breath, and §12's closing note about `wave_set` going
unused makes it worse: it *reads* like the cheap, shipped, underused lever
should be picked up first. This section fixes the map so a fresh session
doesn't re-derive (or re-confuse) the decision.

**Two different levers for two different problems:**

| | `wave_set` / SCW bank | modeled engines (§8) |
|---|---|---|
| what it is | a static single-cycle table — a *snapshot* of a timbre | per-voice DSP — *behavior over time* |
| what it buys | fixed harmonic recipes: organ drawbars, nasal clav, brass blend | epiano pickup growl, FM/bell inharmonic beating, plucked-string decay |
| §12 gaps it solves | **none** (the gaps are all temporal/relational) | gaps 2 and 3 — the blocking ones |
| stations it unblocks | none — it *colors* stations that already work | the whole parked batch: Italo, Steely Dan/AOR, gamelan, the Doors, ethio-jazz |
| cost | an afternoon | real work — but pluck is pre-designed (§8.2 buffer, §8.5 ordering, §8.8 sketch, §11 decoupled mod-env) |

**The decision: engines first, pluck specifically.** Three separate design
passes (§8.5, §8.8, §11) already converge on KS pluck as the first engine,
and §12 added the empirical evidence: every guitar on the dial is a faked
filtered TRI. Pluck also validates against *shipped* carts (retrofit
jangle/bossa) before any new station needs it — the cart-ships-with-the-API
rule is satisfied from day one.

**The trap this section exists to prevent:** building the SCW bank first
*because it's easy* and counting it as progress on §12. It wouldn't be —
the parked stations would still be parked. Cheap-and-shippable is exactly
how an afternoon lever displaces the week that moves the project.

**Order of work:**

1. **KS pluck** (§8.5 phase 1) — retrofit jangle or bossa as the proof.
   **→ SHIPPED 2026-06-05**: `INSTR_PLUCK` (per-voice KS buffer, §8.2 made
   concrete) + the full §8.1.1 macro surface (`instrument_harmonics/timbre/
   morph` + `note_*` twins — landed as ctrl kinds 21/22; the sketch's 18/19
   were taken by §11's mod-envelopes by ship time) + the `pluck` showcase
   cart. Same-day follow-ups: a **fractional read tap** (the whole pitch
   system — LFO_PITCH warble, ENV_PITCH, `note_pitch`/`note_glide` — now
   bends the string live, and pitch is exact instead of length-quantized)
   and the **steal-declick** tail. jangle.c and bossa.c carry live A/B
   toggles (G); the listening verdict + macro taste-tuning are the open
   tail. Porting lessons for engine #2: §8.8.1.
2. **Choke groups** (§12 gap 1) — **→ SHIPPED 2026-06-05**. See §12 above.
2½. **Mallet engine** (§8.5 phase 2) — **→ SHIPPED 2026-06-05** by the §8.8.2
   playbook; `mallet` cart. Taste-tuning + lowend retrofit are the open tail.
3. **FM engine — §12 gap 2a only** (split 2026-06-05; design §8.8.3) —
   **→ SHIPPED 2026-06-05** as `INSTR_FM` + the `fm` cart (§8.5 phase 3 has the
   ship note). Gap **2b** (the audible second osc: Juno saw+square mix,
   unison-detune flag) stays deferred until the Juno-60 cart defines it; the
   detune flag remains the cheapest 80% of 2b and may ship on its own as pure
   polyphony relief.
4. **The SCW bank — opportunistically**, the first time a station wants
   organ or clav color (the Doors' Vox Continental is the likely trigger).
   Shape is decided, see below.
5. **Reverb** (§8.10 / §12 gap 5) — stays deferred, as both docs agree.

**The SCW bank's decided shape** (so the debate doesn't rerun): a
hand-written **`runtime/waves.h`** as single source of truth — `wave_gen(kind,
float out[64])` fills a buffer, convenience `wave_seed(slot, kind)` calls
`wave_set()`; carts get one `#include` + one call. **`waveed.c` includes it**
and drives its seed buttons from the same table, becoming the bank's
interactive auditioner — one definition, no drift. Keep it small and
researched: 6–8 named shapes beats 20 speculative ones. Explicitly
rejected: a JS source of truth + codegen (the `gen-tcc-symbols` pattern
doesn't apply — waves aren't baked into `.cart.png`, they're set at runtime
from C, so the JS side would have **no consumer**), and dual hand-written
JS+C files (sync discipline purchasing nothing).

## 14. Classic Roland machine build readiness (2026-06-05)

Assessed against the current API and the §12/§13 gap map. Three machines that
fit the existing style of the classic-machine family (cr-78, tr-808, tb-303):

### SH-101 (1982) — monophonic lead/bass synth

**SHIPPED 2026-06-05** — `sh101` cart, panel-faithful (grey fader panel:
TUNE | MODULATOR | VCO | SOURCE MIXER | VCF | VCA | ENV + sequencer/arpeggio
LED row + bender). The original assessment ("build it now, no engine work")
held, with four build lessons worth recording:

- **The SOURCE MIXER is voice-stacking.** Pulse + saw + sub + noise each get
  their own slot and `note_on`; one held note = 4 of 8 voices, faders ride
  live via `note_vol()`. This answers §12 gap 2 ("simultaneous waveform mix")
  **for monosynths only** — a poly like the Juno-60 can't afford 4 voices per
  key, so gap 2 proper (a real second-oscillator path) stays open.
- **Engine LFOs are sine-only; the rest is a software LFO.** The 101's
  tri/square/random(S&H)/noise modulator: TRI rides the engine LFO, the other
  three are computed per-frame in `update()` and pushed through the live
  setters (`note_pitch`/`note_duty`/`note_cutoff`). Fine for held notes at
  60fps; scheduled arp hits (`schedule_hit`) only get the engine path — a
  per-hit LFO-shape param would close that if it ever matters.
- **KYBD tracking = per-note `note_cutoff` scaling** (cutoff × 2^((midi−60)/12·k)
  right after `note_on`). Same limitation: `schedule_hit` carries no per-note
  cutoff, so arp steps don't track. Audible only at high KYBD + wide arps.
- **LFO RATE doubles as the seq/arp clock** (the real panel's LFO/CLK slider).
  Self-scheduled with a ~100 ms `schedule_hit` lookahead off `now()` — no bpm
  grid needed, and tempo follows the slider live.

Runtime side-effect of the build: **key claiming** — a cart that reads a key
via `key()/keyp()/keyr()` claims it from the pause hotkey (the two-manual
keyboard needs P as a note; ENTER still pauses). Plus two pause bugfixes:
`-DPAUSE_KEY` is now honored (was hardcoded `KEY_P`) and ENTER can actually
open the overlay (the menu used to consume the same press as Continue).

### TR-909 (1983) — hybrid drum machine

**Build kick/snare/toms now; hihat/cymbal need a workaround or a future gap.**

The 909 is hybrid: kick, snare, toms, clap are analog circuits (same approach
as tr-808.c, just different component values). Hihat and cymbal are **ROM
samples** in the real hardware — that gap is noted in the feature comparison
table (PCM = ❌) and not yet closable via synthesis alone.

| 909 section | Status |
|---|---|
| Kick (analog, ENV_PITCH drop) | ✅ same as 808 kick, slightly different tuning |
| Snare (analog, body + noise) | ✅ same architecture as 808 snare |
| Toms (analog, sine + pitch drop) | ✅ identical to 808 toms |
| Clap (noise retriggers) | ✅ identical approach to 808 clap |
| Hihat / cymbal (ROM samples) | ⚠️ approximation only — see below |
| Shuffle (909 had it; 808 didn't) | ✅ `beat_pos()` swing, same as our Z/X knob |

**Hihat workaround — SFX editor as percussion modeler.** The SFX editor
(`sfxed.c`) authors per-step pitch + volume + filter-CUT curves and exports
them as C arrays called with `sfx(n)`. A 909 hihat is characteristically a
fast-closing highpass burst — exactly the shape the CUT lane can sculpt. Author
the sound in the SFX editor (NOISE wave, steep CUT lane decay, short volume
envelope), embed the exported array in the cart, fire with `sfx(n)`. Not a ROM
sample reproduction but substantially more shaped than a static `instrument()`
call. This technique applies to any future drum machine needing sounds beyond
a fixed ADSR (see §12 SFX modeler note).

### Juno-60 (1982) — 6-voice polyphonic pad/chord synth

**Architecture works; two gaps block authenticity. Build after §12 gap 2 + §8.10.**

The Juno-60's voice architecture (polyphonic `note_on`, LP filter, LFO, ADSR)
maps fine. Two things stand between a Juno-60 cart and the real instrument:

**Gap A — DCO SAW+PULSE simultaneous mix (§12 gap 2b).**
The Juno mixes saw and square waveforms per voice for thickness — that's a
second oscillator with a *different* waveform, distinct from both FM and
unison-detune. ⚠️ Note (2026-06-05): the **FM engine (§12 gap 2a / §8.8.3) does
NOT close this gap** — its second oscillator is an inaudible phase modulator,
not a second audible source. Current workaround: draw a blended SCW
(`INSTR_USER`) with partial saw + partial pulse harmonics — static mix, can't
sweep the ratio, but costs nothing. The full dynamic mix requires the
second-oscillator path in §12 gap 2b. The unison-detune flag (cheapest 80% of
gap 2b) covers the *thickness* aspect but not the waveform blend.

**Gap B — BBD chorus (§8.10).**
The Juno's chorus is *the* defining sound. Without it the cart is a dry generic
pad, not a Juno. The BBD chorus is a short pitch-modulated delay — the same
building block as the Leslie's doppler component (§8.3/§8.10). It falls out of
the effects-layer work rather than needing its own separate engine. Since §8.10
is deferred to phase 4 (after the first engine batch), the Juno-60 is blocked
until then.

**Build order implication:** the Juno-60 is the concrete hardware target that
makes the unison-detune flag, the second-oscillator path (§12 gap 2), and the
§8.10 effects layer all more urgent. If and when the Italo/gamelan/AOR batch
starts and §12 gap 2 opens, design the second-oscillator surface with the
Juno-60 in mind alongside the Rhodes and the bell engines.

## 15. Voice budget — what `SOUND_VOICES 8` actually costs (2026-06-05)

Analysis only — **no change made yet**; the radio family pressing the 8-voice
ceiling (octave doublings, two-voice brass spreads, ambient's 4-voice pad)
prompted the question "why 8, why not 16?". Findings from reading `sound.h`:

**Hardware is not the constraint.** Per voice per sample: one oscillator (or
modeled engine), an SVF, 3 LFOs, 2 mod-envelopes — order of ~100 flops. At
44.1 kHz that's ~35 MFLOPs for 8 voices, ~70 for 16; trivial on every target
(Apple Silicon native, Electron, the WASM web build). Memory is ~4.5 KB/voice
(dominated by `ks_buf[1024]`), so 16 voices ≈ 72 KB. Inactive voices skip the
mix loop — you only pay for what rings.

**The real coupling is loudness.** `sound.h` mixes each voice at a fixed
scale, then hard-clips the sum:

```c
float contrib = s * v->vol * env * trem * 0.2f;   // per-voice scale
...
if (mix >  1.0f) mix =  1.0f;                     // hard clip ±1
```

5 × 0.2 = 1.0 — six full-volume voices can already clip today (rare in
practice because envelopes/filters keep levels lower). The 0.2 constant is
where "how many simultaneous voices we expect" is really encoded:

- bump `SOUND_VOICES` to 16 and keep 0.2 → busy carts clip harshly;
- lower the scale to compensate → **every existing cart gets quieter and its
  mix balance shifts** (the real migration cost — not performance).

Everything else scales mechanically: handle slots, choke loops, and the
steal path all derive from `SOUND_VOICES`; stealing already degrades
gracefully (free → non-held → steal + declick tail).

**Recommendation (parked until measured):** if the radio carts genuinely need
headroom, go to 12–16 **paired with a soft-clip** (tanh-shaped) replacing the
hard clamp — graceful overload instead of digital clipping, without touching
the 0.2 scale or existing carts' loudness. Gate the change on: the soundcheck
tripwire, an ear test on the densest carts (radio stations, modrack), and a
measured before/after — which needs the WAV tooling below. The aesthetic
argument for keeping 8 ("fantasy consoles are scarce") is weak here: with 32
instrument slots, per-voice filters, and three modeled engines, this is
SNES/Amiga-class, not NES-class.

**MEASURED 2026-06-05** (via the §16 tooling, same day): deterministic 60 s
renders of the `house` cart (densest radio station, seed 7) at 8 vs 16 voices:

- **Starvation at 8 voices is real.** The renders diverge from t = 21 s (when
  the arrangement fills out); 25 of 60 seconds differ audibly, peaking at
  **+11% RMS restored** at 25 s — at 8 voices that part of the band was being
  silently voice-stolen. Peak rose only −5.49 → −4.82 dBFS.
- **No clipping at 16 with the 0.2 scale kept**: 0 clipped samples in the
  house render *and* in soundcheck's worst-case burst. Envelopes/filters keep
  real levels far below the theoretical 5-voice rail — the soft-clip is
  optional insurance, not a prerequisite.
- Soundcheck tripwire: PASS at both voice counts.

Conclusion: **16 voices is measured-safe and audibly restores starved parts.**
**FLIPPED TO 16 same day.** The flip surfaced a landmine the experiment alone
couldn't: note_on handles packed their slot in 3 hardcoded bits (`& 7` /
`>> 3` at 13 call sites) — handle slots 8–15 aliased onto 0–7, live setters
steering the wrong notes. Now `SOUND_HANDLE_BITS`/`SOUND_HANDLE_MASK` + a
`_Static_assert` that fails the build if `SOUND_VOICES` ever outgrows the
field.

The refactor's byte-diff was itself a measurement: the corrected 16-voice
house render differs from the pre-fix one **from t≈1s**, proving house holds
**more than 8 simultaneous note_on handles** — at 8 voices it was hitting
*two* ceilings at once: voice starvation (tails stolen) *and* handle-slot
exhaustion (`note_on` with all 8 slots live reuses slot 0, releasing the
oldest held note and disowning its handle — so live cutoff rides steered
nothing or the wrong voice). The corrected 8-vs-16 comparison is starker than
the first pass: **57 of 60 seconds differ**, divergence from t=0, and at 51 s
the 16-voice render is 26.5% *quieter* — correct note_off/note_cutoff control
means fewer wrongly-ringing notes, not just more restored tails. The filter
ride being the genre's whole form, house is the cart that suffered most.
Soundcheck tripwire: PASS.

## 16. WAV capture + analysis tooling — SHIPPED (2026-06-05)

Ear tests don't scale and don't diff. The §15 experiment (and mallet/fm macro
taste-tuning, and any future DSP change) wants: render a cart's audio to a
WAV, compute metrics, compare two runs. The sibling project already has the
working patterns — steal the shape, not the code:

- `~/Projects/navkit/soundsystem/tools/preset_audition.c` — headless preset
  renderer → WAV + audio metrics, `--ref` mode compares against a reference
  WAV and suggests parameter changes (directly relevant to mallet/fm tuning).
- `~/Projects/navkit/soundsystem/tools/golden_wav_gen.sh` — golden-WAV
  regression: render representative songs, checksum, re-render + verify after
  engine changes.
- `~/Projects/navkit/soundsystem/tools/audio_analyze.py` — musical analysis
  (tempo/key/chords via librosa), for transcription-level checks.

**Capture design (two modes, one tap point).** The whole engine mixes into a
single float in `sound_render()` — tap `mix` right before `out[i] = mix`:

1. **Live capture — `wav_request` trigger file**, same handshake as
   `screenshot_request`/`state_request` (write a file containing the output
   path + a duration; the engine records the next N seconds of callback
   output, writes a 44.1 kHz mono WAV, deletes the request). Works on any
   running cart, editor or `play.js`, no flags.
2. **Deterministic render — `--wav <file>` harness flag** (with `--det`):
   render the mixer in lockstep with frames (44100/60 samples per tick),
   no audio device needed — headless-safe, byte-reproducible. This is what
   makes **golden-WAV regression** possible: same cart + same script + same
   seed → identical WAV → checksum diff catches silent DSP regressions the
   soundcheck tripwire can't (it only catches dropped requests).

**Analysis tiers:**

- **Tier 1 (no deps, build first):** `tools/wav-analyze.js` — peak, RMS,
  crest factor, clipped-sample count, per-second envelope. Plain node reading
  the WAV; enough to run the §15 experiment (8 vs 16+softclip on a busy
  station: did clip count drop? did RMS hold?).
- **Tier 2 (borrow, don't port):** shell out to navkit's `audio_analyze.py`
  for tempo/key/chord checks, and crib `preset_audition.c`'s `--ref` metric
  comparison for engine taste-tuning against real-instrument reference WAVs.

Order of work: capture (1+2) → tier-1 analyzer → run §15 experiment → golden
WAVs as a by-product once `--wav` exists.

**Shipped same day, as proposed**: the `wav_request` trigger file, the `--wav`
flag (verified byte-reproducible: two identical runs → `cmp`-identical WAVs),
and `tools/wav-analyze.js` (single-file metrics + two-file compare with a
bytes-identical check — the golden-WAV primitive). The §15 experiment ran on
this tooling the same day. How-to lives in
[`guides/debug-harness.md`](../guides/debug-harness.md) → "WAV capture".
Tier 2 (navkit `audio_analyze.py` / `preset_audition --ref`) remains open.

## 17. Grit, darkness, weight — why everything still sounds clean (2026-06-05)

Modrack grew six modules in a day (MACRO/XPOSE/MIX/CMP/DIV/ADSR) and every patch
still comes out *polite*. That's not taste, it's an audit-able property of the
engine. Five findings, all verified against `sound.h`:

1. **Zero nonlinearity in the signal path.** No drive, no waveshaper, no
   clipper, no bitcrush — the only saturation anywhere is the FM modulator's
   self-feedback (`morph`). Grit *is* nonlinearity; a 303's squelch is the
   filter driven into saturation, not the filter. Ours is a clean SVF into a
   linear sum.
2. **Bone dry.** No delay, no reverb, no echo. "Dark" is mostly a space
   property — repeats that get darker each pass. Every patch plays into an
   anechoic void.
3. **One oscillator per voice, perfectly in tune.** No detune/unison/chorus.
   Two VOICEs fed the same pitch sum *louder*, not *fatter* — analog thickness
   is the beating between near-identical frequencies, and nothing in the
   system can be "a few cents off" (XPOSE deliberately moves whole octaves).
4. **The master section is a hard clip.** Voices sum into one float, then
   `if (mix > 1) mix = 1` (`sound_callback`, end of the mix loop). With
   SOUND_VOICES now 16, hot mixes will hit that wall harshly. There is no
   headroom stage, no glue.
5. **Cart-side: drums are bright ticks** (18ms noise + 90ms tri kick — no 909
   boom) **and the grid is dead straight** (no swing knob — though the engine
   already fires delayed requests sample-accurately *specifically* to keep
   "swing pushes" intact, so this is cart work waiting to happen).

### "So is this effect-bus stuff?" — the taxonomy (the actual design decision)

It looks like one feature ("add FX") but it's **four different layers**, and
the §4 question — *where does the parameter live?* — answers each differently.
Only one of the four is genuinely a bus.

| layer | what goes there | why there | API shape |
|---|---|---|---|
| **voice insert** | drive (tanh), bitcrush | stateless per-sample math, cheap per-voice; *musically per-part* (drive the bass, not the pad) | the §10 four-axes container grows: `instrument_drive(slot, x)` + live `note_drive(handle, x)`, slewed — exactly the filter/duty/macro pattern |
| **shared bus + per-slot send** | echo/delay (reverb later) | a delay line is KBs and wants musical coherence (one tempo'd echo per song) — this is why hardware mixers have sends | `echo(time_ms, feedback, tone)` configures the one bus; `instrument_echo(slot, send)` / `note_echo(handle, x)` set how much each part feeds it |
| **oscillator param** | detune (cents) | not an effect at all — belongs beside duty/glide in `Voice` | `instrument_detune(slot, cents)` + `note_detune`; unison = two notes a few cents apart, suddenly meaningful |
| **master stage** | soft-clip replacing the hard clip; maybe master drive/crush later | one place, whole-mix glue; also the §15 16-voice headroom answer | internal first (tanh at the sum); a `master_*` API only if carts ask |

Chain order inside a voice, decided by the 303 test: **osc → SVF → drive →
VCA(env)** — drive *after* the filter so resonance screams into it, *before*
the envelope so quiet tails don't pump the saturation.

**Modrack mapping** (the reason this section exists): drive/crush/detune become
VOICE/MACRO knobs or CV inlets — per-module character, no new module. The echo
bus surfaces as a **DELAY module** that is secretly a *front-end*: its knobs
write the global bus params, its input-side "send" is per-source. One DELAY in
the rack is honest (there *is* one bus); a second one fights over it — fine for
v1, document it on the panel.

### Order of work

1. **Drive** — biggest lever, smallest diff (one tanh + four-place API wiring +
   the §16 tooling regression-tests it: golden-WAV a driven acid line, crest
   factor should *drop* while RMS rises).
   **✓ SHIPPED 2026-06-05** — `instrument_drive(slot, x)` / `note_drive(handle, x)`,
   tanh after the SVF, pre-gain `g = x²·24` (so `tanh(s·g)/tanh(g) → s` as x → 0:
   drive 0 is a true bypass, verified bytes-identical on soundcheck + bossa WAVs),
   normalized so full-scale stays put. The §16 prediction held exactly: driven
   acid line crest 13.6 → 6.9 dB, RMS −20.6 → −17.2 dBFS, zero clipping.
   tb303 grew the DRV knob (live `note_drive` on the ringing voice — RES + DRV
   is the squelch §17.1 said we were missing).
2. **Master soft-clip** — three lines, pays for §15's 16 voices immediately.
   **✓ SHIPPED 2026-06-05** — linear below a ±0.8 knee (quiet mixes bit-identical),
   tanh-shaped above, slope-continuous, asymptote ±1.0.
3. **Echo bus** — the real architecture step (audio-thread-owned buffer, send
   field in `Instrument`, bus params via the request ring like everything else).
   **✓ SHIPPED 2026-06-05** — `echo(time_ms, feedback, tone)` / `instrument_echo(slot, send)`
   / `note_echo(handle, x)`, exactly the §17-table shape. One 2s delay line
   (~345 KB, audio-thread-owned), dormant until the first echo call ever arrives —
   old carts verified bytes-identical (soundcheck + bossa golden WAVs). Three
   design wins beyond the sketch: (a) **tone is a one-pole LP *inside* the
   feedback loop**, so repeats genuinely darken each pass; (b) **a tanh inside
   the loop lets feedback go to 1.1** — past 1.0 it self-oscillates into a
   tape-style saturation plateau (verified: 10s runaway grows 0.018→0.042 RMS,
   peak −12 dBFS, zero clipping) instead of exploding; (c) **the read tap is
   fractional and slews toward its target with a clamped per-sample step**, so
   sweeping `echo()` time live pitch-bends the ringing tail like varying tape
   speed. Showcase: `spacecho.c` (RE-201 — the effect as the instrument).
   Adoption: dub.c layers the bus's darkening wash under its scheduled-note
   taps and rides the skank's send hot on throws (the §12 "diffuse tails"
   gap, closed); tb303/sh101 get subtle slapback sends.
4. **Detune** — small, after drive (driven unison is the payoff).
5. **Bitcrush** — on-brand dessert; decide insert vs master when it lands.
6. **Cart-side, no engine change: swing knob on CLOCK** (`schedule_hit` already
   keeps the timing), **darker DRUM voices** (909 kick = longer sine + pitch
   env — the "Punch (VCA)" preset recipe, baked in).

One-line version: **we built a very good modular synth and forgot to build the
broken speaker it should play through.**

> **Update 2026-06-05:** the four-layer table above is now a frozen decision —
> [decisions/0015](../decisions/0015-effects-are-recipes-not-primitives.md) closes the
> primitive roster at ~12 functions forever. Everything else (wah, chorus, tape, the
> Leslie, sidechain…) is a recipe of these primitives or a refusal with the musical
> need covered elsewhere; the pedalboard audit lives in that ADR. A new primitive must
> prove it can't be a recipe.
