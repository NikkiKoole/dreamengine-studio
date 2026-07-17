# Transparent auto-tune — correct the pitch, keep the voice

STATUS: SHIPPED (2026-07-17) — the **offline engine primitive is in**: `sample_autotune(slot, root,
scale, amount)` (`sound.h`) — formant-preserving pitch correction, ported from the proven spike and
byte-faithful to it (the engine render matches the cart-C numbers exactly). Two offline spikes
established it: v1 (known-f0 formant preservation) and v2 (detector-derived epochs + correct-to-scale
on a wobbly source); the DSP now lives in the engine and [`mictune`](../../tools/carts/mictune.c) just
calls it. Gate committed: [`tools/formant-check.js`](../../tools/formant-check.js) (f0 + formant-peak
oracle). Flavour B of the audio-input frontier's auto-tune split; robot flavour already shipped
([`hardtune`](../../tools/carts/hardtune.c)). **Real-voice check: PASSED** (2026-07-18) — the maker
sang into `mictune` and confirmed the tuned playback holds up ("sounds pretty good"): pitch corrected,
voice kept. So the offline feature is *done*. **Live real-time path: SPIKE BUILT** (2026-07-18) —
`autotune_mic(root, scale, amount)` (streaming TD-PSOLA on the audio thread, reading the `sound_extin`
ring) + the [`livetune`](../../tools/carts/livetune.c) cart. **Feel test (2026-07-18):** structurally
it works — formants preserved, amplitude smooth (the phase-lock fix tamed an initial "pulse") — **but
there's audible pitch WARBLE**. Diagnosed against a real vocal via the new `DE_MIC_WAV` harness (feed a
WAV as the mic, `tools/testdata/`): a constrained fundamental-band jitter metric reads ~2.6 Hz on a
held note (input ~1.6 Hz). Quick principled tweaks each MEASURED as no help — octave-continuity, a
retune-speed glide, note-hysteresis — so the warble is the streaming pitch/epoch **resolution** itself,
not a boundary/octave bug. Clean live correction needs a dedicated pass (a YIN-grade real-time pitch
tracker / sub-sample epochs, or the phase-vocoder route), not spike tweaks. Also: the headless render
is synchronous, so it may not fully reproduce the editor's real-time-audio timing warble.
**Live path = feasible + parked** (the offline feature stands on its own). Live-only (ADR-0032). Rolls
up from [`audio-input-frontier.md`](audio-input-frontier.md) §2.

## Spike result (2026-07-17) — PROVEN, formants stay put

The make-or-break question — *can TD-PSOLA move the pitch while the formants (spectral envelope) stay
still?* — is **yes.** Method (deliberately confound-free): synthesize an "ah" (sawtooth ×
fixed 3-formant bank) at a **known** f0=110 Hz so epochs are exact, shift it **+5 semitones**
(ratio 1.335) two ways — PSOLA vs a naive linear resample (the chipmunk control) — and measure f0 +
formant peaks with an FFT oracle (autocorrelation f0 + Welch-averaged, smoothed spectral envelope):

| | pitch f0 | F1 | F2 | F3 |
|---|---|---|---|---|
| RAW | 110 Hz | 668 | 1109 | 2530 |
| **PSOLA** | **147 Hz** ✓ | 700 | 1141 | 2476 |
| NAIVE resample | 147 Hz | 947 | — | — |

Both hit the target pitch exactly (110·1.335 ≈ 147). But at that same pitch, **PSOLA held every
formant within ±5 % of RAW** (F1 668→700, F2 1109→1141, F3 2530→2476 — the vowel stays "ah"), while
the **naive resample scaled F1 up 42 %** (668→947 — a different, chipmunked vowel). That gap *is* the
formant preservation. One implementation gotcha the spike caught: **keep the period a `float`
everywhere** — an `(int)T` in the epoch-index math drifts the grains off the glottal pulses over ~165
periods and smears the result (centroid 1935→1820 Hz once fixed). PSOLA is the right algorithm for our
monophonic voice; no need to escalate to the phase vocoder.

### v2 (2026-07-17) — the real-world path also holds

v1 used a *known* f0 (the easy case). v2 takes on the actual risk that guards an engine primitive:
**epochs from a detector, not told** (autocorrelation pitch track + peak-refined glottal marks), a
**hard, voice-like source** (5.5 Hz vibrato + slow drift + breath noise, sitting ~35 cents *sharp* of
a scale note), and real **pitch-CORRECTION** (time-varying PSOLA re-spacing each frame's grains at the
`snap_scale(detected f0)` period). Oracle over the whole take (f0 sampled at 8 sub-windows for a
stability/wobble number):

| | f0 (mean) | wobble (range) | F1 | F2 | F3 |
|---|---|---|---|---|---|
| RAW (wobbly, sharp) | 118.9 Hz | **2.9 Hz** | 711 | 1184 | 2476 |
| **TUNED** | **116.5 Hz** (= A#2) | **0.3 Hz** | 711 | 1152 | 2466 |

Three wins at once, from a detector-driven pipeline: the pitch **snapped** onto the scale note
(118.9→116.5 Hz, exactly A#2), it **de-wobbled** (2.9→0.3 Hz, ~10× steadier — flat and in-tune), and
the **formants held** (F1 identical, F2/F3 within 3 % — the vowel/identity survives). That is
transparent auto-tune working end to end, offline. The one thing the oracle can't cover is a **real
human voice** (the source is synthetic): `mictune`'s R key runs the identical pipeline on a live
`mic_record` take — the confirmation a person has to hear, and the gate before engine promotion.

---

## The goal, and how it differs from the robot we already shipped

Two very different things hide under the name "auto-tune":

- **Robot auto-tune (the T-Pain/Cher flavour) — SHIPPED** as [`hardtune`](../../tools/carts/hardtune.c).
  It throws your voice *away*: a saw carrier is locked to `snap_scale(mic_pitch)` and the 12-band
  vocoder paints your vowels back on. The output is a synth in the vocoder's robotic timbre. Cheap,
  because it never had to move your actual voice — it re-synthesized one. That's why it was a cart
  afternoon.

- **Transparent auto-tune (this doc) — UNBUILT.** Keep your *real* voice — your timbre, your breath,
  your identity — and only **nudge the pitch** onto the scale. This is the modern-pop vocal: you
  can't hear it working, the singer just sounds impossibly in-tune. It is a genuine DSP build because
  you have to move the pitch of a real recording *without moving the formants* — and the engine has
  no primitive that can do that.

## Why it's hard: formants, and why everything we have moves them

A voice is a **source** (the buzzing vocal folds — this sets the *pitch*) filtered by a **resonant
tube** (the throat/mouth — its resonances, the **formants**, set the *vowel and the identity*).
Change the pitch, the formants must stay put, or the vowel changes and you get the chipmunk.

Every pitch-shift the engine owns today does the opposite — it moves pitch and formants **together**:

- **`varispeed`** — literally changes playback rate. Pitch up = formants up = chipmunk. (The frontier
  doc's one-line warning.)
- **`grains_pitch` / the granular cloud** (`fx_set_grains_pitch`, `sound.h` ~L1766) — grain read-speed
  `= 2^(semitones/12)`. Resampling each grain moves its formants with it.
- **`shimmer` / `octaveup_process`** (`sound.h` ~L2256) — a 2-grain overlap-add octave-up. This is the
  closest prior art in the repo: the exact *granular overlap-add machinery* PSOLA needs. But the read
  tap sweeps at the pitch ratio, so it's a classic granular shifter — **formants ride along.** It's a
  great reference for the grain plumbing and the constant-power crossfade, **not** a shortcut for
  preservation.

So: we have granular pitch-shift machinery, capture-then-freeze plumbing, and the correction-target
math. The **one genuinely missing piece** is a *formant-preserving* pitch-shift.

## The one new primitive: formant-preserving pitch-shift on PCM

Two algorithm families do this. We pick the cheaper one first because the voice is monophonic and we
already track its pitch.

### (a) TD-PSOLA — the recommended first cut
**Time-Domain Pitch-Synchronous Overlap-Add.** The trick that keeps formants still:

1. **Find the pitch epochs** (glottal marks) — one per pitch period. We already have the pitch:
   `mic_pitch()`/YIN gives `f0`, so period `T = sr/f0`; place marks `T` apart, refined to a local
   waveform peak. (Unvoiced/silent frames: no marks → pass through unshifted, same voiced/unvoiced
   idea the vocoder v2 band uses.)
2. **Window a grain around each epoch**, ~2·T long (a Hann window). Each grain is one glottal pulse
   *with its formant ringing intact* — because a grain is a slice of the real waveform, its spectral
   envelope (the formants) is baked in and untouched.
3. **Re-space the grains at the *target* period** `T' = sr/(f0·ratio)` and overlap-add. Closer spacing
   → higher pitch; **grain length unchanged → formants unchanged.** That decoupling *is* the whole
   trick: spacing carries pitch, grain content carries timbre.

Cost: cheap — time-domain, no FFT. Failure mode: gets *gargly* on breathy / very low / noisy input
where epochs are hard to place. Monophonic only (fine — it's a voice).

### (b) Phase vocoder — the fallback
STFT → shift bins with phase re-alignment → **re-apply the original spectral envelope** (extract it
via cepstral liftering or LPC, divide it out before the shift, multiply it back after). More general
and more robust on messy input, but heavier and needs an **FFT on the audio thread** for the live
version. Reach for it only if PSOLA's artifacts are unacceptable.

**Decision:** PSOLA-offline first. If the spike's oracle shows the formants smearing and tuning it
doesn't fix it, escalate to the phase vocoder — but learn that from the cheap build, not after wiring
into the mixer.

## Strategy: offline capture-then-freeze FIRST (the safe spike)

The frontier doc flags this and it's the crux of the plan. **Do not start with the live mic.** Correct
a *recorded* take instead. This single choice removes the two hardest problems at once:

- **No realtime budget.** PSOLA runs offline over a frozen buffer — no audio-thread deadline, no
  allocation-free constraint, no latency tuning. Just get the algorithm right.
- **It stays deterministic.** A frozen `mic_record` take is just PCM data — it replays, saves, and
  `spec()`s like any sample ([`mic-and-sampling.md`](mic-and-sampling.md) §"the deep constraint").
  So the spike is **fully gateable by a WAV oracle** — which is the entire point, because
  formant-preservation is a claim you must *measure*, not trust.

The plumbing for this pipeline is **already shipped**: `mic_record(seconds)` →
`mic_record_read(out, max)` gives the raw PCM; `sample_load(slot, data, n)` puts a processed buffer
back into a playable slot; `sample_read` reads one out. Record → get the buffer → PSOLA-correct →
`sample_load` → play. The live audio-thread version (reading the `sound_extin` ring the vocoder built)
is a *follow-on* once the algorithm is proven — and it's live-only per
[ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md), like every live-mic effect.

## The spike, concretely

**1 — The primitive.** A pure, offline C function in `sound.h` (non-realtime — it runs over a whole
buffer, not per-sample), roughly:
```c
// Formant-preserving pitch-correct a MONO PCM buffer in place (offline, TD-PSOLA).
// f0_hz: per-frame pitch track (from YIN), or 0 where unvoiced. target: a fn/table giving the
// desired f0 per frame (snap_scale). amount 0..1 = partial..full correction; speed = retune glide.
void sample_pitch_correct(float *pcm, int n, const float *f0_hz, int hop,
                          /* target */ ..., float amount, float speed);
```
Exact signature settles during the build; the shape is: PCM in place + a pitch track + a target + the
two Antares knobs (**amount** = how far toward the note, **speed** = how fast it gets there).

**2 — Epoch marks** from the pitch track (`mic_pitch`/YIN → period → refined peak). Unvoiced frames
pass through.

**3 — Correction target.** `snap_scale(f0)` for the nearest in-scale note — the helper already exists,
copy-pasted in [`hardtune`](../../tools/carts/hardtune.c) and [`humseq`](../../tools/carts/humseq.c);
the spike either copies it again or we finally extract it to a shared header. `amount` interpolates
raw→target; `speed` slews the target so fast = hard snap, slow = natural glide (same knobs as
`hardtune`, but shifting the *real voice* instead of a carrier).

**4 — The cart: `mictune`** (capture-then-freeze, so **deterministic** — carries a `.rec` and a
`spec()`). Record a sung phrase (or, for the gate, a synth test tone via a sample), correct it,
**A/B raw vs corrected**, and a pitch scope drawing raw pitch vs the scale grid with the corrected
line snapping on. The stranger-legible core: "sing slightly off, hear yourself land on the note — and
still sound like *you*."

**5 — The acceptance oracle (make-or-break).** This is why offline-first: a deterministic A/B WAV
check, the same shape as the [`filter-spec`](../../tools/filter-spec.js) "measure the real response via
a generated probe" pattern from the 303 spike ([`audio-notes.md`](audio-notes.md) §25):
- Feed a **known off-pitch tone** (say +40 cents of a scale note).
- Assert the output pitch **lands on the scale note** — `tune-check` / `harmonic-spec` (the pitch moved).
- **AND** assert the **spectral centroid barely moves** — `wav-envelope`'s centroid (the formants
  *didn't* move). This is the formant-preservation proof, and it's the exact trick that caught the
  sampler's silent crush no-op. A shifter that fails preservation passes the first assert and **fails
  this one** — the oracle's whole job is to catch a chipmunk masquerading as auto-tune.

Only when both asserts pass is the primitive real.

## Determinism

- **Offline (this spike) = deterministic.** Frozen PCM in, frozen PCM out; `.rec`/`spec()`/WAV-gate all
  hold. This is the reason to build it first.
- **Live (the follow-on) = live-only**, [ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md).
  The realtime PSOLA reads the `sound_extin` ring and writes shifted samples into the mix under the
  block deadline (profile with `profile-fleet`). Same carve-out as `voxbox`/`hardtune`.

## Risks / where this can genuinely fail

- **PSOLA gargle** on breathy, very low, or noisy takes (bad epochs). Mitigations: better peak
  refinement, cross-fade smoothing, or escalate to the phase vocoder.
- **Formants smear anyway** — if the oracle's centroid assert fails and tuning doesn't recover it, the
  time-domain approach is wrong for our input and we switch to (b). Cheap to discover offline.
- **Latency (live only, later)** — a laggy corrector is unusable; that's a realtime problem the offline
  spike deliberately doesn't take on.

This is a research spike with a real chance of a "the timbre doesn't survive, rethink" moment — which
is exactly why the deliverable is *an offline A/B you can hear + oracle numbers* before any realtime
commitment.

## API sketch (cart-facing, once proven)

```c
// offline: correct a recorded take in a sample slot to a scale, keeping timbre. amount/speed = the knobs.
void sample_autotune(int slot, int scale, float amount, float speed);   // wraps the primitive over a slot
// live (follow-on, ADR-0032): correct the mic in real time onto a scale
void autotune_mic(int scale, float amount, float speed);
```
Dormant/byte-identical discipline as ever; add per the "Adding a new API function" rule.

---

## The live real-time path (the next build) — DESIGN (2026-07-18)

The offline primitive is done + confirmed. The frontier is **sing and hear yourself corrected *now***.
This is a genuinely separate, harder build — the offline version mallocs freely and takes as long as
it likes; the live one must run **on the audio thread, allocation-free, per output sample, under the
block deadline**, and the whole experience lives or dies on **latency**. Scoped here before any code.

### What already exists (the infra is mostly built)
- **The mic on the audio thread** — the `sound_extin` SPSC ring (2048 samples ≈ 46 ms), the vocoder's
  Phase-2 work. `sound_extin_read()` gives one mic sample per output sample; `extin_on` gates it;
  `sound_extin_reset()` drops stale latency on start. A live autotune stage reads it exactly like
  `vocoder_mic` does (the master-stage seam ~`sound.h` L6264).
- **The correction target** — `snap_scale` / `sound_scale_table` + the `at_*` helpers from
  `sample_autotune`. The *target*-picking math ports as-is.
- **A coarse pitch** — game-thread `mic_pitch()` (YIN, ~21/sec). Fine for the *target* (it snaps to a
  note and moves slowly); **not** enough for sample-accurate epochs (see the crux).

### The algorithm: streaming TD-PSOLA
The offline PSOLA had the whole buffer; the live one is a **sliding** version — the classic two-pointer
pitch-synchronous overlap-add:
- Keep an **input ring** of the last few periods of mic (the `sound_extin` ring already is one, or a
  small copy for random-access windowing).
- An **analysis pointer** `ta` walks the input at the *source* period `T` and snaps to the local
  glottal peak (the epoch).
- A **synthesis pointer** `ts` emits a Hann grain (length ~2·T) copied from the input around the
  nearest analysis epoch, advancing by the **target** period `T' = SR/snap(f0)` and overlap-adding
  into the output. `T/T'` carries the pitch shift; grain content unchanged → **formants preserved**,
  same trick as offline. Fixed, bounded work per sample; a couple of small static grain buffers, no
  malloc.

### The crux — real-time epoch tracking (this is the spike's risk)
Offline, epochs came from a full-buffer pitch track. Live, we must find glottal pulses *as samples
arrive*, cheaply. Options, cheapest first:
1. **Running period + peak-snap.** Maintain `T` from a short autocorrelation refreshed every ~256
   samples on the audio thread (bounded cost); place the next epoch ~`T` ahead, snapped to the local
   waveform peak. Likely good enough for a monophonic voice — the recommended first cut.
2. **Feed the target from `mic_pitch()`** (game thread) to avoid audio-thread pitch cost, but keep the
   *epoch* peak-snap local. Splits the concern: slow target from the game thread, sample-accurate
   marks on the audio thread.
3. If both gargle → the phase-vocoder fallback (STFT), heavier, an FFT on the audio thread.

### Latency budget — the whole game
Inherent: ~1 grain half-length of lookahead (≈ one period, ~9 ms at 110 Hz) + the `sound_extin` ring
depth (a few buffers, ~20–40 ms on desktop's separate in/out clocks). Target **< ~40 ms** to feel
live; measure on device. iOS/Android duplex (one clock) is tighter and the real home — desktop is
where we get the algorithm right. **Acceptance is not the offline WAV oracle** (there's no frozen
output): it's (a) a **loopback latency measurement** (click in → corrected click out, count samples),
(b) `formant-check` on a *recording of the live output* to confirm formants still hold, and (c) the
honest one — **does it feel responsive and sound good sung live** (the maker's ears, like this pass).

### API
`autotune_mic(int root, int scale, float amount)` — mirror `vocoder_mic()`: turn on a master-stage
stage that reads the mic ring, corrects, and monitors the corrected voice into the mix. Dormant until
called (byte-identical). Live-only, [ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md).

### The first spike
A `livetune` cart: `mic_start()` + `autotune_mic(0, SCALE_MINOR, 1.0)`, sing and hear it corrected,
with an on-screen latency readout + the pitch ribbon. Build the streaming PSOLA behind option 1;
if latency or gargle is bad, that's the finding — retreat to 2, then the phase vocoder. Same
build-it-behind-a-spike discipline as the offline rounds, but the deliverable is a *feel* test, not
an oracle number.

## See also

- [`audio-input-frontier.md`](audio-input-frontier.md) §2 — the auto-tune split (robot shipped, this is flavour B)
- [`hardtune`](../../tools/carts/hardtune.c) — the robot flavour we shipped; the `snap_scale` + retune-knob reference
- [`voxroll`](../../tools/carts/voxroll.c) — formant decoupled from pitch on the **synthesised** `INSTR_VOICE` (the Melodyne-UX reference, not a real-mic corrector)
- [`vocoder.md`](vocoder.md) — the live-mic ring (`sound_extin`) the realtime follow-on rides; the v2 voiced/unvoiced idea reused for epoch gating
- [`mic-and-sampling.md`](mic-and-sampling.md) — capture-then-freeze (the deterministic path this spike lives on) + the `mic_record`/`sample_load` plumbing
- [`boutique-pedals-roadmap.md`](boutique-pedals-roadmap.md) §"Primitive 2" + [`audio-notes.md`](audio-notes.md) §17 #27 — `octaveup_process`, the granular overlap-add prior art (moves formants; the machinery, not the answer)
- [ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md) — live-mic-through is live-only
