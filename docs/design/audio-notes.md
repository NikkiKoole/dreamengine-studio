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

A small PICO-8-style software synth. Facts, straight from `runtime/sound.h`:

**Engine**
- 8 voices (`SOUND_VOICES`), 44.1 kHz, **mono**, software-mixed.
- Mix = sum of voices × 0.2 gain each, then hard-clipped to [-1, 1].
- Runs on raylib's audio-stream callback (the **audio thread**).
- Voice allocation: first free → else steal a non-music voice → else steal voice 0.

**Waveforms** (the `INSTR_*` ids, defined in `studio.h`)
- `INSTR_SQUARE` — fixed **50% duty** pulse
- `INSTR_SAW`
- `INSTR_TRI` — true linear triangle
- `INSTR_NOISE` — LCG white noise (not a periodic LFSR like the NES)
- `INSTR_SINE`

**Envelope**
- A single *fixed* attack/release declick per step: ~5% ramp in, ~15% ramp out.
- **No real ADSR.** Decay/sustain shaping is faked by writing successive volume
  values into SFX steps.

**Data model**
- **SFX**: 32 slots × up to 32 steps. Each step = `{pitch (MIDI), instr, vol 0..7}`.
  `step_dur` is in 10ms units; per-SFX `loop` flag.
- **Music**: 16 patterns, each = 4 channels of SFX indices (`-1` = silent) + loop flag.
- ⚠️ **The SFX/music banks are hardcoded** in `sound_load_demo_data()`. **There is no
  cart-side API to author SFX or patterns yet.** Carts that call `sfx(n)`/`music(n)`
  play the built-in demo data; almost all real carts instead use the live one-shot
  calls below.

**Thread-safe control (the important architectural fact)**
- Main thread → audio thread via a 32-entry **request ring buffer** (`sound_push_req`).
  Kinds: `0=sfx`, `1=music`, `2=note`.
- Delayed requests (for `strum`/`schedule`) sit in a 16-entry holding pen on the audio
  thread and fire when their sample countdown expires.
- **This ring buffer is the one correct place to mutate sound state.** Any new control
  surface should ride it rather than poke shared structs directly (avoids tearing).

**Musical clock**
- `bpm` / `beat` / `beat_pos`, ticked once per frame in `sound_tick(dt)`.

**Public API today** (`studio.h`)
- One-shot voices: `note(midi,instr,vol)`, `hit(midi,instr,vol,dur_ms)`
- Banks: `sfx(n)`, `music(n)`
- Theory helpers: `tone(scale,oct,instr,vol)`, `chord(root,type,instr,vol)`,
  `strum(...,delay_ms)`, `degree(scale,oct,n)`
- Timing/rhythm: `schedule(delay_ms,...)`, `bpm`, `beat`, `beat_pos`, `every(n)`,
  `euclid(hits,steps,b)`, `chance(percent)`
- Scales: `SCALE_MAJOR/MINOR/PENTA/PENTA_MIN/BLUES/CHROMATIC`
- Chords: `CHORD_MAJ/MIN/DIM/AUG/MAJ7/MIN7/DOM7/SUS4/POWER`

**Two key constraints that shape every idea below**
1. **Voices snapshot at note-on.** `sound_set_step()` copies freq/vol/wave into the
   `Voice`; a playing voice holds no reference back to its source. Mutating a bank
   entry therefore affects only *future* notes, not the one currently ringing.
2. **Everything is fire-and-forget.** No held/sustained voice exists; the default note
   is 250 ms. Continuous control needs a new "held voice" concept (see §6).

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

### 5.6 Expose the SFX/pattern banks to carts — *fills a current hole*
Independently of the above, carts can't author `sfx`/`music` data today. A small
`sfx_def(slot, steps…)` / pattern-builder API would make the existing bank machinery
actually usable from a cart. Low-glamour, decent leverage (unlocks all data-driven
sound + a future tracker UI to match the sprite editor — already a stated VISION goal).

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
goes through the roof while the learner-facing surface stays at zero. The §5.4 "instrument
bank + ADSR" path is about letting carts *build* timbres; this path is about *shipping*
finished ones. For our audience, shipping finished ones wins — keep the 30 internal params
of each engine **hidden**, expose them only as fixed presets via id
(`INSTR_ORGAN_JAZZ`, `INSTR_EPIANO_RHODES`) at most.

### 8.2 The one architectural decision: buffer-free vs. buffered

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
  drawbars. The organ engine is ~160 B/voice; the Leslie is a **single ~1.5 KB buffer
  *shared* across all voices** (amplitude + slight Doppler mod, slow/fast speed toggle) —
  not per-voice. Tiny cost, and it's the line between "9 sines" and "church/theatre organ."
  An organ *wants to sustain*, so it pairs naturally with the held-channel model (§6).
- **Acoustic piano** (KS + dispersion) for the pianist themselves.
- **Formant filter** for "singing"/choir/vocal-organ color — 4 bandpass peaks, ~512 B of
  vowel tables + ~8 floats/voice, reusing a state-variable filter. Bonus: that **same SVF is
  the resonant `filter()` from §5.5** — build one filter primitive, get the SID sweep *and*
  vowel formants from it.

### 8.4 SCW tables — a complementary cheap lever (not a replacement)

navkit also embeds **single-cycle waveforms** (one cycle of a real instrument as a small
float table; ~600 samples ≈ 2.4 KB each). A hand-picked bank of ~6 (≈8–10 KB) plus ~100
lines of phase-accumulator + cubic-interpolation playback gives instantly richer *static*
timbres. But SCW gives a rich *snapshot*, not the *living behavior* — the EP pickup growl,
the organ key-click, the piano's inharmonic dispersion — that makes the modeled engines feel
alive. Treat SCW as a cheap timbre-source lever (it could even feed an organ drawbar), not as
a substitute for the modeled instruments. Aliasing at high notes is acceptable/retro here.

### 8.5 How this reframes the roadmap (§7)

A curated instrument set is a *better* first move than the instrument bank, because it needs
**no new concept** — beginners keep calling `note`/`chord`/`tone`. Suggested ordering, each
phase independently shippable:

1. **Buffer-free instruments** — `INSTR_ORGAN`, `INSTR_EPIANO`, `INSTR_MALLET`/`INSTR_CELESTA`,
   `INSTR_STRINGS`. Zero API/architecture change; four characterful instruments.
2. **Leslie (shared) + resonant SVF filter** — sells the organ; the filter is the reusable
   primitive that also gives §5.5 and §8.3's formant.
3. **The per-voice buffer decision** → `INSTR_PIANO`, `INSTR_GUITAR`/`INSTR_HARP`. The pianist.
4. **Formant filter** + a couple of cheap character effects (chorus, soft drive, bitcrush).
5. **Optional:** bowed strings; and/or the SCW bank (§8.4).

### 8.6 Cons / watch-outs

- **Hide the internal params.** Each engine has many knobs; expose fixed presets via id only.
  The moment a beginner sees "drawbar 4 = 6", the simplicity is gone.
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

---

## 9. Open questions

- **Channels vs. handles** for real-time: commit to fixed channels for simplicity, or
  pay for handles to support unbounded controllable emitters?
- **Filter scope:** global-only (simple, SID-like) forever, or eventually per-channel?
- **Where does duty live** once both exist — an `INSTR_PULSE_*` shortcut *and* an
  instrument/channel field? (Probably both: constant for the easy path, field for control.)
- **Stereo?** Everything is mono today. Panning is cheap to add but doubles the mix and
  raises "is it worth the surface?" — likely low leverage for this audience.
- **Tracker UI**: VISION mentions a sound tracker to match the sprite editor. The
  instrument bank + cart SFX authoring (§5.4, §5.6) are its prerequisites — design them
  with that editor in mind.
- **Voice budget:** with held channels reserving voices, do we raise `SOUND_VOICES`, or
  is 8 still plenty once some are long-lived?
- **Per-voice buffers (§8.2):** do we let a `Voice` carry a delay-line buffer (unlocks the
  whole Karplus-Strong / physical-modeling family at ~2 KB/voice), or stay buffer-free and
  ship only the additive/modal instruments? This is the gate for a real acoustic piano.
- **Curated instruments vs. instrument bank:** ship finished `INSTR_*` presets from navkit
  (§8) *and/or* build the cart-authorable bank (§5.4)? They're complementary, but which is
  the headline for the audience — sounding great out of the box, or authoring your own?
- **One filter primitive, two uses:** commit to a single state-variable filter that serves
  both the SID-style `filter()` sweep (§5.5) and the vowel formant bank (§8.3)?

---

## 10. The four axes, one container — SHIPPED

> **Status: shipped** (HEAD `141a0a7`). All four axes below are live on the raw waveforms,
> bundled per instrument slot: `instrument()` (ADSR), `instrument_duty()`, `instrument_lfo()`,
> `instrument_filter()`. Deltas from the original sketch: **3 LFOs per slot**, so
> `instrument_lfo()` takes a `which` (0–2) index; **held channels (§6) are NOT built**, so
> `ch_*` doesn't exist yet — `LFO_CUTOFF` is the filter-sweep/wah path for now, and the
> `dream synth` cart fakes hold-to-sustain with a fixed note gate. Carts: `instruments`,
> `lfo`, `filter`, `dream synth`. §2's "where we are now" predates this and is superseded
> for the instrument model. (The original proposal text is kept below as the rationale.)

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
