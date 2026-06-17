# Web/wasm audio parity — scoping (2026-06-17)

**Status: SCOPING + a CONFIRMED-on-paper bug (no fix applied yet).** This is the one remaining audio
blind spot from the 2026-06-16/17 audit ([STATUS](../STATUS.md) #42, [audio-notes §20](audio-notes.md)).
Every audio gate we have — `tune-check`, `dc-check`, `level-check`, `fx-check`, `soak-check` — renders the
**native** build. The emscripten/wasm build players actually hear is verified by **ear only**. This doc
scopes what "parity" means, what's feasible — and (2026-06-17 source dig) confirms a real pitch bug.

## ⚠ CONFIRMED on paper (2026-06-17): the WORKLET backend plays sharp on non-44.1k devices

A source dig (no browser needed — the code path is unambiguous) settles the SR question, and the two web
backends behave **differently**:

- **Worklet backend (default on desktop).** `sound_worklet_init()` (`sound.h` ~6465) calls
  `emscripten_create_audio_context(0)` — **NULL options**, so emscripten does `new AudioContext()` with no
  `sampleRate` → the browser/device **default rate** (commonly **48000** on desktop). `sound_aw_process`
  then fills the 128-sample AudioWorklet output **with no resampler** from audio the synth generated under
  `SOUND_SAMPLE_RATE = 44100` (every phase increment / delay length / coefficient). The browser plays those
  samples at the context rate → **48000/44100 = 1.0884× too fast → +146.7 cents sharp** (≈ 1.5 semitones)
  **and 8.8% fast tempo.** No resampling anywhere in this path.
- **Plain backend (iOS fallback + the desktop "plain" toggle).** `LoadAudioStream(SOUND_SAMPLE_RATE, 32, 2)`
  → raylib/miniaudio's data converter **resamples** 44100 → the device rate. **Pitch-correct.**

**Why it could ship unnoticed — it's device-dependent.** A `new AudioContext()` adopts the output device's
rate. macOS built-in output often runs at **44100** (so on the owner's Mac the worklet sounds *fine*), while
most 48 kHz outputs — Windows/Linux, external DACs, Bluetooth, many laptops — play it ~1.5 semitones sharp.
Classic "works on my machine." The `plain` audio toggle would (accidentally) sound *correct* where worklet
is sharp, which is a strong tell if anyone's noticed "the plain mode sounds more in tune."

**The fix (one line, low-risk):** force the worklet context to 44100 so it matches native and is
device-independent — browsers honor an explicit rate and resample to the hardware themselves:

```c
EmscriptenWebAudioCreateAttributes attrs = { .latencyHint = "interactive", .sampleRate = SOUND_SAMPLE_RATE };
sound_aw_ctx = emscripten_create_audio_context(&attrs);   // was emscripten_create_audio_context(0)
```

The alternative — making the synth sample-rate-agnostic (read the context rate, drop the `SOUND_SAMPLE_RATE`
`#define`) — is a huge change (it sizes constant arrays and hundreds of coefficients) and unnecessary: a
fantasy console synth at a fixed 44.1k that the browser resamples is correct and simplest.

**✅ FIX APPLIED 2026-06-17** (`sound.h` `sound_worklet_init`): the context is now created with
`EmscriptenWebAudioCreateAttributes{ .latencyHint="interactive", .sampleRate=SOUND_SAMPLE_RATE }`. Compiles
clean native + emcc-worklet (validated by building `epiano --worklet`). **Two follow-ups before it reaches
players:** (1) **on-device confirm** — the bug + fix are verified by *source reading*, not a browser; open a
worklet cart and listen (and ideally read `AudioContext.sampleRate`, see Phase 0). Safari accepts an explicit
44100 (a universal rate), but the resample-to-hardware quality/latency wants an ear. (2) **republish** — the
fix only reaches shipped carts after a web rebuild (`tools/publish-cart.sh` / build-site.js); the committed
`site/` carts still have the old `(0)` worklet until then.

## Why it matters

The web build is a whole platform with zero automated audio coverage. 69 carts are "engine-stale" on
the web (their published audio predates a `sound.h` change). More fundamentally: a tuning or level fix
we verified natively could be **wrong on the web and no tool would know**. [`audio-timing.md`](audio-timing.md)
covers the *timing* ("drunk playback") side; this is about the **samples themselves**.

## How the web audio path actually works (the facts that decide feasibility)

- **Native:** `SOUND_SAMPLE_RATE = 44100`, the main loop pumps `sound_callback()` **735 samples/frame**
  (44100/60). `--wav` runs this synchronously (`sound_synth_mode`) and is **byte-reproducible** under
  `--det` — this is our deterministic reference render (`runtime/studio.c` `wav_stream_pump`).
- **Web has two backends** (`tools/build-site.js`): **worklet** (AudioWorklet + `WASM_WORKERS` + shared
  memory; default on desktop) and **plain** (ScriptProcessor fallback; iOS). The chooser is auto.
- **Worklet block size = 128 frames.** `sound.h` bridges emscripten's planar 128-frame quantum to our
  interleaved mixer: `sound_callback(itl, 128)` per quantum (`sound.h` ~line 6432). Native uses 735.
- **⚠ The web `AudioContext` sample rate is set NOWHERE** in `web_shell.html` / `web_shell_worklet.html`
  / `audio-worklet-stub.js`. So it inherits the browser/emscripten default — **commonly 48000 Hz on
  desktop**, while the synth hardcodes 44100 everywhere (every phase increment, delay-line length, filter
  coefficient). **This is the first thing to verify and may itself be a latent bug** (see below).

## The three axes of divergence (and what each needs)

Parity isn't one question. Three independent things can make web ≠ native, in increasing difficulty:

### Axis 1 — codegen / float determinism (emcc vs clang)
*Same SR, same block size, same C — does the emscripten-compiled math produce the same floats as the
native clang build?* Divergence sources: different FP contraction (FMA), `-O` level, any `-ffast-math`,
libm differences (`sinf`/`tanhf`/`powf` implementations), wasm's strict IEEE vs x87/NEON.
- **Testable OFFLINE, byte-comparable, cheapest.** Compile a tiny audio-only harness (`sound.h` + a main
  that seeds a cart's requests and pumps `sound_callback` in 735-blocks, writing a WAV) with **emcc
  targeting Node** (NODERAWFS for file I/O), run under `node`, compare bytes to the native `--wav` of the
  same sequence. Reuses the existing deterministic synth pump.
- **Highest value for least work** — it catches the silent, scary case: the optimizer quietly changing
  the DSP math so web sounds subtly different everywhere, uniformly. **Recommended first build.**

### Axis 2 — sample rate & block size (the real runtime's shape)
*The browser runs at 48000 in 128-quantum blocks; native at 44100 in 735-blocks.* If the context SR ≠
44100 and nothing resamples, **byte parity is impossible** and — worse — **everything plays ~+147 cents
sharp** (48000/44100) on the web. Two sub-questions:
- **Does the AudioContext actually run at 48k?** If yes, is there resampling, or is the synth's 44100
  assumption simply wrong on the web? **A `tune-check`-style measurement of a real browser render would
  answer it — and might be the highest-impact find of the whole effort** (a library-wide web pitch error).
- Block size (128 vs 735) only changes *when* queued requests fire within a frame — a tiny timing jitter,
  not a sample-value difference, once SR matches. Lower priority.
- Comparison here must be **spectral / pitch-based, not byte-wise** (resampling guarantees byte mismatch).
  Reuse `tune-check.js`'s YIN/autocorrelation and `level-check`'s RMS on a browser-captured WAV.

### Axis 3 — real browser runtime (the deployed path)
*The actual worklet under a real engine: scheduling, shared-memory races, GC pauses, the ScriptProcessor
fallback.* Needs a **headless browser** (Playwright/Puppeteer) loading a built cart, capturing audio via
`OfflineAudioContext` (deterministic, fast — if the build can be driven offline) or `MediaStreamDestination`
+ `captureStream` (real-time, non-deterministic). Hardest, least deterministic, most plumbing. This is
where the worklet-vs-plain backends would be A/B'd. **Lowest priority** — Axes 1+2 catch the likely bugs.

## Recommended phasing

1. **Phase 0 — answer the SR question (hours, no new tooling).** Build one cart for web, open it, and read
   `AudioContext.sampleRate` in the console (or add a one-line `console.log`). If it's 48000 and the synth
   is producing 44100-rate samples with no resample, that's a **confirmed web-wide pitch bug** — stop and
   fix that first (force the context to 44100, or make `SOUND_SAMPLE_RATE` follow the context). This gates
   everything: if SR doesn't match, Axes 1–3 are moot until it does.
2. **Phase 1 — offline emcc parity harness (~half-day).** The Axis-1 Node render → byte/near-byte diff vs
   native `--wav`. A `tools/web-audio-check.js` twin of the other gates. Catches codegen/float drift.
   Main wrinkle: `sound.h` includes `raylib.h` for the `AudioStream` *type* — the harness either links
   raylib-web headers (no window) or stubs the handful of referenced symbols.
3. **Phase 2 — spectral parity on a real browser render (~1–2 days).** Playwright + `OfflineAudioContext`
   render of a built cart → `tune-check`/`level-check` analysis. Confirms SR/pitch and gross level match.
4. **Phase 3 — worklet-vs-plain + deployed-path checks (optional, open-ended).** Only if 0–2 surface
   nothing and we want full coverage.

## Risks / unknowns to resolve first

- **The SR mismatch (Phase 0)** — the single highest-leverage unknown; possibly a real shipped bug.
- **Can the wasm build be driven headless/offline at all?** The worklet path assumes a real AudioContext;
  an offline harness sidesteps it (Axis 1) but doesn't test the worklet itself.
- **`OfflineAudioContext` + AudioWorklet + shared memory** may not cooperate headless — Phase 2 might be
  forced onto real-time `captureStream` (non-deterministic → spectral-only, not byte).
- **emcc optimization flags** in `build-site.js` (`-O?`, any fast-math) directly drive Axis 1; check them.

## Effort summary

| Phase | What | Effort | Determinism | Catches |
|---|---|---|---|---|
| 0 | Read the web `AudioContext.sampleRate` | hours | n/a | a possible web-wide pitch bug |
| 1 | Offline emcc render → byte-diff vs native | ~½ day | byte-exact | emcc/clang float & codegen drift |
| 2 | Headless-browser render → spectral/pitch diff | 1–2 days | spectral | SR/pitch + gross level on the real path |
| 3 | Worklet-vs-plain + deployed runtime | open | low | scheduling / backend-specific issues |

**Recommendation:** do **Phase 0 now** (cheap, and it might find the biggest bug); then Phase 1 as the
durable CI gate (the codegen twin of `build-all`). Phases 2–3 only if those turn something up or full
web coverage becomes a priority.
