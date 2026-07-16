# mic-spike — can the engine hear? (Tier-1 microphone probe)

The smallest possible proof for the **audio-input frontier**
([`docs/design/mic-and-sampling.md`](../../docs/design/mic-and-sampling.md) → §"the live-throughput
dimension"). Before touching the hot runtime files (`sound.h`/`studio.c`), prove mic capture works
on this desktop and that the two Tier-1 surfaces a cart would read are feasible:

- **`mic_level()`** — RMS loudness (VU + dBFS) → the beatbox / envelope-follower input
- **`mic_pitch()`** — a crude zero-crossing pitch estimate (Hz + nearest note) → the hum→theremin input

It captures the default *physical* mic (skipping virtual loopback devices like BlackHole), prints a
live meter for ~6 s, then exits. No storage, no playback — the "mic as CONTROLLER" shape.

```sh
zsh tools/mic-spike/run.sh        # fetches miniaudio.h once, builds, runs
```

## Why miniaudio

It's the single-header, public-domain lib **raylib already links internally** — so it's exactly the
cross-platform path the design doc names: mac now (CoreAudio), WASM (`getUserMedia`) and iOS
(AVAudioSession + the permission prompt) later. `run.sh` fetches `miniaudio.h` on demand; it isn't
committed (4 MB) and neither is the binary.

## macOS mic permission (TCC) — the gotcha this spike surfaced

If only a **virtual** device (BlackHole/Aggregate) enumerates and the meter sits at −99 dB, the
physical mics aren't broken — macOS **hides physical input devices from a process that lacks
Microphone permission**. The grant attaches to the *terminal app* that first requests it, so run it
from your own shell (in Claude Code: `! zsh tools/mic-spike/run.sh`), click **Allow**, then hum —
the meter moves and the pitch locks to a note. That permission model is itself a finding for the
real engine seam (every backend needs its own permission flow).

STATUS: SPIKE (2026-07-16) — pipeline proven (device opens, callback delivers frames steadily);
awaiting a permission-granted run to confirm live signal. Not wired into the engine.
