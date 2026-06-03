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
