# The engine can hear — the audio-input frontier map

STATUS: ★ MAP (2026-07-17) — the audio-input *capability* SHIPPED (the mic seam on all
platforms + the live vocoder). This doc is the roll-up above [`mic-and-sampling.md`](mic-and-sampling.md)
(the sampler doctrine + four-tier spectrum) and [`vocoder.md`](vocoder.md) (the DSP): what
the ear actually **opened**, and the frontier it points at (the pedal tier, auto-tune, beatbox
trigger, vocoder v2, on-glass — veins 2–3 hum→MIDI + voice-sampler now shipped). Detail lives in
those two docs + [ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md);
this page is the strategic picture and the ranked "what follows".

---

## The headline

The microphone didn't add a *feature*. It added **capabilities** — new kinds of thing the
engine can now do at all — and one of them is quietly bigger than the vocoder that paid for it.

Before this, the engine was deaf: every input was a button, a touch, or a knob, and every sound
came from its own synthesis. Now a cart can take the most personal input a phone has — a voice —
and use it as a control signal, run it *through* the engine's DSP in real time, or freeze it into
playable PCM. That's three doors, not one.

## What the ear IS — the three capabilities

**1. The mic as a live CONTROLLER axis.** `mic_level()` (RMS loudness, rock-solid) and
`mic_pitch()` (a YIN detector — octave-safe, tracks a hummed voice cleanly, ~21 readings/sec).
These are inputs like `btn()` and touch: continuous signals a cart can wire to *anything* —
pitch, volume, a filter cutoff, a game control. Nothing else on the shelf has a pitch-as-control
axis. `humtheremin` is the first taste (hum → a synth follows your pitch + loudness).

**2. The live audio-thread PCM ring** (`sound_extin_*`) — *the sleeper.* Built to feed the
vocoder its modulator, this lock-free SPSC ring is the general mechanism for **live mic audio
flowing through the engine's DSP**. It is the foundation the whole **live-throughput / "pedal"
tier** stands on — every "make a sound, hear it processed *right now*" effect (fuzz, live
granular, a looper, delay/reverb pedals) needs exactly this and nothing more. It exists, it's
paid for, and today only the vocoder rides it.

**3. Capture-then-freeze sampling** (`mic_record()` → `sample_read`/`sample_load`). Record a few
seconds of mic, freeze it to a PCM buffer, then chop/pitch/loop it like any sample. This is the
*deterministic-safe* path (see below) and it already feeds `breakchop`'s beatbox auto-chop.

## What already shipped (the proof the doors are real)

| Cart | What it proves | Capability |
|------|----------------|-----------|
| [`mictest`](../../tools/carts/mictest.c) | The engine hears at all — VU meter + pitch readout, the Tier-1 seam end to end | controller |
| [`pitchscope`](../../tools/carts/pitchscope.c) | The pitch axis is *honest* — a raw, unsmoothed scope of `mic_pitch()` on a log axis (the detector's acceptance test) | controller |
| [`humtheremin`](../../tools/carts/humtheremin.c) | The first instrument you play with your **voice** — hum → pitch + loudness drive a singing sine | controller |
| [`vocode`](../../tools/carts/vocode.c) | The 12-band vocoder DSP, with two *synth* sources so it's fully deterministic (the DSP's own acceptance test) | audio-thread ring (send) |
| [`voxbox`](../../tools/carts/voxbox.c) | The **real** vocoder — sing/talk and a saw chord speaks your vowels + rhythm ("sounds like Stevie Wonder") | audio-thread ring (live mic) |
| [`breakchop`](../../tools/carts/breakchop.c) | Beatbox → onset-sliced pad kit | capture-then-freeze |
| [`humseq`](../../tools/carts/humseq.c) | **hum→MIDI** — hum a melody; a hysteresis note-tracker freezes it to a scale-locked loop played on any `INSTR_*` (vein 2) | capture-then-freeze |
| [`singsynth`](../../tools/carts/singsynth.c) | **Voice sampler** — hold a vowel, loop it into a keybed instrument you play polyphonically, SK-1-style (vein 3) | capture-then-freeze |
| [`hardtune`](../../tools/carts/hardtune.c) | **Robot auto-tune** — a saw carrier locked to `snap_scale(mic_pitch)`, vocoded by the live mic; RETUNE slider = hard T-Pain robot ↔ natural glide (vein 3, flavour A) | audio-thread ring (live mic) |

The engine seam (host owns the device behind `platform.h`; engine analyses + exposes the API)
is live on **desktop + web**; the vocoder ring runs on the audio thread. Full ship log:
[`mic-and-sampling.md`](mic-and-sampling.md) and the audio-input `▶ ACTIVE THREAD` in
[`HANDOFF.md`](../HANDOFF.md).

## The determinism spine — why hearing stays honest

The engine's deep constraint is determinism (a `.rec` replays bit-identically; `spec()` runs
headless). Live mic input is *live by nature* and can't replay. The doctrine that keeps both:

- **Capture-then-freeze is deterministic.** Once mic audio is frozen into a PCM buffer, it's just
  data — replays, saves, and `spec()`s like any sample. This is why sampling/`breakchop` stay in
  the deterministic world.
- **Live-through is live-only** — [ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md).
  A `voxbox` performance (or any pedal) does *not* replay deterministically, and that's declared,
  not a bug. The pedal tier lives on this side of the line by design.

The practical rule for anything below: if you want it to replay/save, freeze first; if the magic
*is* the real-time processing, accept live-only and mark it.

## What it opens NEXT — the frontier, ranked by juice-per-effort

*Shipped since this doc was written:* **hum→MIDI** ([`humseq`](../../tools/carts/humseq.c) — hum → a
scale-locked note loop on any instrument, vein 2) and the **voice sampler**
([`singsynth`](../../tools/carts/singsynth.c) — hold a vowel, play your own voice polyphonically,
vein 3). Both capture-then-freeze (deterministic). What's left:

**★ 1 — The pedal tier — the SEAM SHIPPED (2026-07-22).** The generic "route the live mic through
*any* effect" path is now a one-liner: **`input_monitor(gain)`** sums the `sound_extin_*` ring into
master bus 0's DRY mix just before the `fx_order` insert chain — so every master insert
(crush/filter/chorus/eq/tape/drive **and** `reverb_insert`/`echo_insert`, the reorderable reverb +
delay pedals) processes your own sound. Dormant unless armed (existing carts byte-identical);
live-only per ADR-0032. Proven by the spike [`micfuzz`](../../tools/carts/micfuzz.c) (latency
confirmed good on Mac), then wired into [`pedalboard`](../../tools/carts/pedalboard.c) as the
**GUITAR IN** toggle — a guitar/voice now runs through the chain you build, order and all. *Still
open:* the **live looper** (record → overdub → stacked layers) — the loudest unmet wish the demand
tool keeps surfacing — off the same ring; and live granular. *Prereq: done (ring + `input_monitor`).*
Detail: [`vocoder.md`](vocoder.md) §"the pedal tier" + [`sound-next-steps.md`](sound-next-steps.md).

**★ 2 — Auto-tune / pitch-correction.** Two very different builds hide under one name:
- *Robot auto-tune (the T-Pain flavour) — **SHIPPED** as [`hardtune`](../../tools/carts/hardtune.c).*
  A single saw carrier locked to `snap_scale(mic_pitch)`, vocoded by the live mic → your melody,
  corrected to a scale, in the vocoder's robotic timbre; a RETUNE slider runs from instant-snap
  (hard robot) to a slow glide (natural). Reuses `vocoder`/`vocoder_mic` + humseq's scale-snap;
  live-only per ADR-0032. This is the *whole* cheap flavour — done.
- *Transparent auto-tune (your natural voice, just corrected) — the offline primitive SHIPPED.*
  Keeps your real timbre and only nudges the pitch (the modern-pop vocal). The one primitive the
  engine lacked — a **formant-preserving pitch-shift** (every other shifter — `varispeed`, the
  granular cloud, `octaveup` — moves formants *with* pitch → chipmunk) — is now
  `sample_autotune(slot, root, scale, amount)`: TD-PSOLA, built **offline / capture-then-freeze**
  (deterministic; gated by [`formant-check.js`](../../tools/formant-check.js) — proved pitch snaps to
  scale + de-wobbles while formants hold ±5%). Showcase [`mictune`](../../tools/carts/mictune.c) (press
  R to tune your own voice). Scoped + logged in **[`transparent-autotune.md`](transparent-autotune.md)**.
  *Open tail:* the live real-time path off the `sound_extin` ring + a per-frame key API.
  **NB:** [`voxroll`](../../tools/carts/voxroll.c) decouples formant from pitch too — but on the
  *synthesised* `INSTR_VOICE`, not a real recording, so it's the Melodyne-UX reference, not a shortcut.
  *Prereq: a formant-preserving pitch-shift primitive (new); the mic ring (done).*

**3 — Beatbox → live drum trigger.** `breakchop` records-then-chops; the *controller* version
classifies onsets by spectral tilt (kick/snare/hat) and fires [`drumkit.h`](../../runtime/drumkit.h)
pads in real time. Tier-1 in [`mic-and-sampling.md`](mic-and-sampling.md) specs exactly this.
*Prereq: onset detection + a cheap classifier (new).*

**4 — Vocoder v2 quality.** The **unvoiced/sibilance noise band SHIPPED** (2026-07-17) —
`vocoder_unvoiced()`: consonants ("s", "t", "sh", "f") now come through as noise instead of tonal
mush, turning `voxbox` from "vowels" into a proper vocoder (press V to A/B it; [`vocode`](../../tools/carts/vocode.c)
A/Bs it deterministically). *Remaining:* mic-rate resample for non-44.1k phone mics, per-band gate,
more bands. Scoped in [`vocoder.md`](vocoder.md) §"v2 quality".

**5 — Make it real on glass.** Everything above runs on desktop + web; iOS/Android need the
on-device permission-prompt run (the backends are written to the `platform.h` contract; they need
a device build to verify). The mic is *most* compelling on a phone — that's the whole reason it
ranked in the demand data. This is the "land it on the product surface" follow-through.

> **iOS mic host — two gotchas the `pedalboard` GUITAR IN device run cost real time (2026-07-22):**
> 1. **Capture on a SEPARATE `AVAudioEngine` from output.** Installing an input tap on the *output*
>    engine's `inputNode` makes iOS reconfigure that engine's I/O (`AVAudioEngineConfigurationChange`),
>    which drops the output render edge → **the whole app goes silent the instant the mic turns on**.
>    A config-change reconnect handler could NOT reliably keep it alive. The fix that works: a
>    dedicated capture engine (tap → `de_audio_input`, no output wiring); its reconfiguration is its
>    own and can't touch playback. Both engines share one `.playAndRecord` session (set at launch —
>    that alone doesn't prompt; only reading input does). See [`ios/Sources/AudioEngine.swift`](../../ios/Sources/AudioEngine.swift).
> 2. **Resample non-44.1k phone mics to the engine rate** *before* the `sound_extin` ring. iOS taps
>    the mic at its native 48k; the ring is drained at 44.1k, so raw writes overflow → dropped samples
>    → garbled near-silence, ~9% sharp. Desktop hid this (CoreAudio forces 44.1k capture). Fixed in
>    [`mic.h`](../../runtime/mic.h)'s `mic_input_push` (linear resample; `sr==engine` → byte-identical
>    fast path). Also fixes `voxbox`/`hardtune` on 48k phone mics.

## The through-line

The mic turned the engine from a thing that *only speaks* into a thing that *hears, transforms,
and remembers* sound. The deterministic side (freeze → sample → chop → arrange) is well down its
road in [`mic-and-sampling.md`](mic-and-sampling.md). The **live side is the young frontier**: one
ring, already built, waiting for the pedals and the voice-driven instruments to be plugged into it.
If we chase one thing next, the live looper is the sweet spot — loudest demand, foundation done,
honest core.

## See also

- [`mic-and-sampling.md`](mic-and-sampling.md) — the sampler doctrine, the four-tier spectrum, the capture-then-freeze rule (this map's deterministic half)
- [`vocoder.md`](vocoder.md) — the vocoder DSP + the pedal-tier infrastructure notes (this map's live half)
- [ADR-0032](../decisions/0032-live-mic-effects-are-live-only.md) — live-mic-through is live-only; capture-then-freeze stays deterministic
- [`voxroll`](../../tools/carts/voxroll.c) — a Melodyne-style vocal piano roll (formant decoupled from pitch) on the **synthesised** `INSTR_VOICE`; the Melodyne-UX reference for the auto-tune work (not a real-mic corrector)
- [`sound-next-steps.md`](sound-next-steps.md) — where the pedal/side-chain work sits in the audio backlog
- [`midi-out.md`](midi-out.md) — carts as sequencers (the hum→MIDI cousin)
