# dreamengine — audio design notes

A living document for the sound system: what exists in code today, where it sits
relative to the chips it evokes, the ideas on the table, and a guiding rule —
**get as much expressivity as possible from as few primitives as possible.**

Companion to `VISION.md`. Implementation lives in `runtime/sound.h` (header-only,
included once from `runtime/studio.c`); the public surface is the `sound` section
of `runtime/studio.h`.

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

## 8. Open questions

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
