# The N-band vocoder — real carrier × modulator cross-synthesis

STATUS: BUILDING (2026-07-17) — **Phases 1 + 2 SHIPPED**. Phase 1: the master-stage 12-band vocoder
(`vocoder()` / `vocoder_send()`) + the deterministic [`vocode`](../../tools/carts/vocode.c) showcase
(saw chord × `INSTR_VOICE` phrase). Phase 2: the **live mic** modulator — a lock-free SPSC
audio-thread ring (`sound_extin_*`, which also opens the live-throughput/pedal tier) + `vocoder_mic()`;
[`voxbox`](../../tools/carts/voxbox.c) upgraded from talkbox-lite to the REAL vocoder (sing → the chord
speaks your words). Determinism carve-out written: [ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md)
(live-mic-through = live-only). Gates green: soundcheck, build-all, wasm-parity, no-crash live path.
Phase 3 (v2 quality): the **unvoiced/sibilance noise band** SHIPPED (2026-07-17) — `vocoder_unvoiced()`;
consonants (s/t/sh/f) now cut through instead of turning to tonal mush ([`vocode`](../../tools/carts/vocode.c)
A/Bs it deterministically, [`voxbox`](../../tools/carts/voxbox.c) rides it on the live mic).
**Remaining (v2 quality):** mic-rate resample (non-44.1k device mics); latency tuning on device; more bands.

## Why this doc exists

The vocoder is the effect the engine could never do, and it's been named as blocked in three places:
[`audio-notes.md §13`](audio-notes.md) (*"NOT a vocoder — that's carrier×modulator (two inputs),
waiting on the sidechain path"*), [`effects-recipes.md`](../guides/effects-recipes.md) (*"not a
vocoder — that needs a second carrier×modulator input, a future effect once the sidechain lands"*),
and [`sound-next-steps.md`](sound-next-steps.md) (*"Vocoder and sidechain-comp both need a side-chain
input — bus plumbing… design that path once"*). Two of the three prerequisites are now met:

1. **A second-input routing path exists** — the sidechain `sc_key[]` accumulator shipped with
   effects-bus **Increment D** (2026-06-12). The docs already flagged it as *"the same plumbing the
   vocoder needs."*
2. **A live modulator exists** — the microphone. The Tier-1 mic seam (`de_audio_input` → `mic.h`,
   2026-07-16/17) is the live input channel the whole two-input effect family was blocked on.

What's still missing is the vocoder's own DSP **and** one piece of infrastructure the mic seam doesn't
yet provide: the mic samples reaching the **audio thread** in sync with the output. `mic.h` today only
derives windowed *analysis* (level, YIN pitch) on the game thread — a vocoder needs the raw mic PCM in
the per-sample mix loop.

> **This unlocks more than the vocoder.** The mic-PCM-on-the-audio-thread path is the same
> infrastructure every **live-throughput / pedal** effect needs (fuzz, live granular, looper — the
> most-demanded shape in [`mic-and-sampling.md` §"the live-throughput dimension"](mic-and-sampling.md)).
> Pay for it once here and the pedal tier opens.

## What a vocoder is (the DSP)

Carrier × modulator cross-synthesis: impose the **spectral envelope** of the modulator (your voice) onto
a **carrier** (a harmonically-rich synth), so the synth "speaks your words."

```
  MODULATOR (mic voice) ──┬─ bandpass₁ ─ rectify ─ env-follow ─┐   band 1 loudness
                          ├─ bandpass₂ ─ rectify ─ env-follow ─┤   band 2 loudness   (the "analysis")
                          └─ … bandpassₙ … ────────────────────┘   band N loudness
                                                                        │  (per band)
  CARRIER (synth chord) ──┬─ bandpass₁ ───────────── × env₁ ───┐        ▼
                          ├─ bandpass₂ ───────────── × env₂ ───┤   Σ → OUT   (the "synthesis")
                          └─ … bandpassₙ … ───────── × envₙ ───┘
```

Both signals are split by the **same** filterbank (N log-spaced bandpasses). Each modulator band's
loudness (a rectified, one-pole-smoothed envelope) multiplies the matching carrier band; the results
sum. Where your voice has energy (a vowel formant), the carrier gets loud there → the chord takes on
your vowel. Where you're silent, the carrier is silent → your rhythm comes through.

### Band plan
- **N = 12** to start (classic hardware is 16–20; a fantasy-console 8–12 is intelligible-enough and
  on-character — lo-fi as a feature, like the SP-1200's bit depth). Make N a compile constant so it's
  tunable.
- **Log-spaced** centers ~**180 Hz … 7 kHz** (speech intelligibility lives here). `f_k = 180 · (7000/180)^(k/(N-1))`.
- **Bandpass** via the existing `sound_svf` in BP mode, **Q ≈ 4–6** so adjacent bands overlap slightly
  (no spectral gaps). Same coefficients drive the modulator and carrier copies of each band.

### Envelope follower (per modulator band)
Reuse the auto-wah follower shape already in `sound.h` (`flw_atk`/`flw_rel`): `env += (|x| > env ?
atk : rel) · (|x| − env)`. **Attack ~3–5 ms, release ~15–30 ms** — fast enough to catch consonant
transients, slow enough not to buzz. A small **noise floor gate** per band kills hiss between words.

### Known hard bits (scope as v2)
- **Sibilance / unvoiced consonants** (s, t, sh, f) are broadband noise, not pitched — a tonal carrier
  can't render them, so speech sounds mushy. Classic fix: detect unvoiced modulator frames (high-band
  energy with no low-band pitch → reuse `mic_pitch()==0`) and **substitute white noise** into the top
  bands. Big intelligibility win; ship after the basic bank works.
- **Carrier must be harmonically rich + broadband** — a sine carrier vocodes to almost nothing (no
  energy in most bands). Saw/pulse chords, or add a noise layer to the carrier. Document this as a cart
  requirement; the showcase carrier is a saw chord (as `voxbox`/`vowel` already use).

## The genuinely-new engine infrastructure

The DSP above is "just" `2N` SVF + `N` followers per sample from rostered parts. The new work is the
plumbing:

### 1. Mic PCM on the audio thread (the crux)
Today: the host capture callback calls `de_audio_input()` → `mic_input_push()` which does **windowed
analysis** and publishes scalars (`mic_g_rms`, `mic_g_pitch`). A vocoder needs the **raw mic samples in
the output mix loop**, sample-aligned with the carrier.

- Add a **lock-free SPSC ring** (`mic_pcm_ring[]`) that `mic_input_push` writes and the audio/mix loop
  reads — the same shape as the existing `sc_key[]` / `reverb_in` accumulators and the `req` queue
  (atomic acquire/release; see `audio-threading.md`).
- **Rate + sync:** on **desktop** the mic (AudioQueue input, 44100) and output (raylib `AudioStream`)
  are **separate streams/clocks** → the ring absorbs drift; expect **20–40 ms latency** (a few buffers
  deep) and a slow producer/consumer imbalance to manage (drop/hold on under/overrun). On **iOS/Android**
  a **duplex** stream (AVAudioEngine / AAudio input+output on one device) gives tight sync and lower
  latency — worth using the duplex path there. `DE_NO_RAYLIB` hosts already own their audio device, so
  they add the input side of the same callback.
- **Latency is the whole game for a live vocoder** — a laggy one is unusable. This is the real-time
  engineering commitment `mic-and-sampling.md` flagged for the pedal tier. Budget it explicitly and
  measure on device.

### 2. The carrier tap
The carrier is a **bus** (the synth chord's summed output), read pre-master. Reuse the effects-bus
machinery (`bus[NBUS]`, effects-bus-architecture.md): the vocoder is a **bus stage** that takes the
carrier bus + the mic-ring modulator and writes the cross-synthesis to the master — architecturally a
sibling of `sidechain` (a special two-input stage, **not** an `fx_order` insert).

## Determinism — the doctrine call (must decide before building)

Live mic input breaks `.rec` replay / `spec()` (mic-and-sampling.md §"the deep constraint"). Two shippable modes:

- **A — capture-then-freeze (deterministic, ship FIRST).** Vocode a mic take that was already recorded
  via `mic_record()` (breakchop's path). The modulator is a frozen PCM buffer, so replay is exact and it
  reuses the shipped recording. Less magical (no live sing-and-hear) but on-doctrine and testable.
- **B — live pedal (the magic, needs the doctrine call).** Sing and hear it now. This is the
  live-throughput carve-out: either the harness **records the live mic PCM as an input track** (heavy —
  audio in the `.rec`) or vocoder carts are declared **live-only, no deterministic replay**. This is a
  real carve-out from the `.rec`/`spec()` guarantee and wants an **ADR**.

Recommendation: **build the DSP against mode A first** (deterministic — you can `spec()`/gate it with a
recorded take), then add the live ring (mode B) behind the doctrine decision.

## API surface (cart-facing, the 4-place add)

The mic is the implicit modulator; the cart supplies a carrier bus and turns the vocoder on:

```c
void vocoder(int carrier_bus, float mix);   // impose the mic's spectrum on carrier_bus → master. mix 0..1
void vocoder_bands(int n);                   // 4..16 bands (lo-fi ↔ intelligible). optional; default 12
// (mode A twin, deterministic): vocoder_source(int sample_slot) — modulate from a recorded take, not the live mic
```

Dormant + byte-identical until `vocoder(..., >0)` (the effects-family discipline). Set-and-hold except
`mix` (rideable). Add per the "Adding a new API function" rule (studio.h/.c + studioDocs.js + shell.js).

## Implementation note (2026-07-17) — the real Phase-1 first step, from reading the seam

Read the master/bus code before building; three findings sharpen the plan:

- **The seam already exists, marked.** `sound.h` ~L6170 (master FX section) literally says *"Side-chain
  seam (future vocoder / sidechain-comp): a control input would tap a source bus's level HERE, before
  the insert chain — not built."* That's where the vocoder stage goes.
- **`sc_key_lvl[N_SC_KEYS]` is the exact precedent** — a per-sample accumulator (reset each sample
  ~L5869, fed by each voice's pre-pan contribution ~L6109, consumed ~L6146). The vocoder's modulator
  input is the same shape, but it must keep the **signal** (to bandpass), not just the |level|.
- **Hidden prereq: there is NO per-slot bus routing yet.** L6172: *"Per-slot AUX routing
  (`instrument_bus`) is the deferred next increment; v1 is master-only."* Slots only reach an aux bus as
  a side effect of a per-instrument insert (`instrument_chorus` grabs a private bus). So "carrier bus ×
  modulator bus" can't be expressed cleanly today.

**So the cheapest real Phase 1 is a master-stage vocoder with an sc_key-style modulator SEND, not two
buses:**
- **1a — modulator send.** Add `voc_mod[]` (a per-sample MONO accumulator, twin of `sc_key_lvl` but
  storing the signal). A slot feeds it via a new `vocoder_send(slot, amount)` (mirror `sidechain_key`).
- **1b — the master stage.** At L6170: **carrier = the master mix** (everything else), **modulator =
  `voc_mod`**; run the N-band cross-synthesis and blend by `mix`.
- **Open design Q — send-only modulator.** To hear ONLY the vocoded output, the modulator slot's *dry*
  must be excluded from the master (a send-only flag, like an echo-only routing). Simplest v1: accept
  hearing the dry modulator too (still a valid, testable vocoder), add send-only muting as a follow-up.
- This keeps Phase 1 to a **master-only** feature (no `instrument_bus` dependency); the two-bus version
  becomes a nicety once per-slot routing lands.

## Build increments

1. ✅ **SHIPPED (2026-07-17) — filterbank DSP at the master stage, modulator SEND, NO mic** — per the implementation note above:
   `voc_mod[]` accumulator + `vocoder_send(slot, amount)` (carrier = master mix, modulator = a synth
   slot, e.g. an `INSTR_VOICE` phrase). Fully deterministic → soundcheck/`spec` testable. Proves the
   bank + followers + reconstruction in isolation, zero mic risk. Showcase: a saw chord vocoded by an
   `INSTR_VOICE` phrase. (Two-bus routing waits on `instrument_bus`; master-only is the cheap v1.)
2. ✅ **SHIPPED (2026-07-17) — Mic PCM ring** — `sound_extin_*` (a lock-free SPSC ring in sound.h; mic
   host writes from the capture thread, mixer reads per output sample) + `vocoder_mic()`. voxbox is the
   showcase. Desktop 1:1 (44.1k). Still to do: non-44.1k mic resample, on-device latency tuning, duplex.
3. **Capture-then-freeze mode** (`vocoder_source`) — vocode a `mic_record()` take (deterministic;
   `voxbox`/breakchop synergy).
4. **Quality** — ✅ **unvoiced/sibilance noise-substitution band SHIPPED (2026-07-17)**: `vocoder_unvoiced(amount)`.
   A source-agnostic detector (fraction of modulator energy in the top `VOC_UV_BANDS` bands — no `mic_pitch()`
   dependency, so it works for a synth modulator OR the live mic) crossfades the top bands' excitation from the
   tonal carrier to band-limited white noise when the voice goes broadband (an "s"). Dormant (`voc_uv_amt=0`) →
   byte-identical to the v1 bank. Acceptance: [`vocode`](../../tools/carts/vocode.c) renders a vowel+consonant
   phrase deterministically; the consonant windows jump from brightness ~0.03 / centroid ~3.8kHz (off) to ~0.2 /
   ~5.3kHz (on), with no peak increase (`wav-envelope`). Still open: N tuning, per-band gate, formant-shift knob.

## Prereqs, links, and the 0015 stance

- **Prereqs:** the mic seam ✅; the mic-PCM audio-thread ring (Increment 2, also unlocks the pedal tier);
  a determinism ADR before mode B.
- **0015 stance** ([`decisions/0015`](../decisions/0015-effects-are-recipes-not-primitives.md)): the
  formant filter cleared 0015 as *"the SVF reused four times."* A vocoder is *"the SVF reused 2N times +
  N followers + a cross-multiply"* — but it needs NEW engine infrastructure (mic-on-audio-thread +
  two-input bus stage), so it's an **engine feature**, not a cart recipe. It's the honest exception 0015
  anticipated ("two-input effects live in the engine").
- **CPU:** `~2N` SVF + `N` follower ops/sample. At N=12 that's ~24 biquad-ish evals/sample ≈ 1M/s — fine
  on desktop, **must be measured on the weakest mobile/wasm target** (it stacks on the audio thread with
  YIN + the mixer). Fewer bands is the lo-fi lever.
- **Showcase:** upgrade [`voxbox`](../../tools/carts/voxbox.c) from talkbox-lite to the real thing (it
  already has the carrier chord + mic UI), or a dedicated `vocode` cart.
- **Related:** [`mic-and-sampling.md`](mic-and-sampling.md), [`audio-notes.md`](audio-notes.md) §13,
  [`sound-next-steps.md`](sound-next-steps.md), [`effects-bus-architecture.md`](effects-bus-architecture.md),
  [`audio-threading.md`](audio-threading.md), [`audio-input-frontier.md`](audio-input-frontier.md)
  (the mic-seam map this bus feeds), [`transparent-autotune.md`](transparent-autotune.md)
  (the sibling pitch tool on the same input path).
