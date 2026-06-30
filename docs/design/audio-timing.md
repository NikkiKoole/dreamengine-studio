# Audio timing — frame-quantized triggers, and why web drifts

> EXPLORATION (researched 2026-06-10). Source: a real phone-play session — web
> playback "feels a bit drunk," with **occasional drift** on web that isn't audible
> on native. This note traces the cause through the code and ranks the fixes.
> The latency half of the web-audio question lives in
> [`product-notes.md`](product-notes.md), the wasm-vs-native engine parity in
> [`web-audio-parity.md`](web-audio-parity.md); **this note owns timing jitter/drift**.
>
> **STATUS (2026-06-10): EXPLORING — investigation closed; decision pending.** Cause fully
> characterized and confirmed in the *shipped* artifact (web audio runs on the main
> thread via `ScriptProcessorNode` — see §3). Shipped: the `frame_dt` clamp + a 512
> web buffer — the **main-thread ceiling** ("tight with a small residual," reproducible
> on desktop *and* mobile). The only path to native-straight is the **AudioWorklet**,
> now **proven reachable on GitHub Pages** via `coi-serviceworker` (the spike played on
> an iPhone — see "feasibility spike"). **Decision (2026-06-10): greenlit via Path A**
> (worklet when isolated; the current ScriptProcessor build is the free auto-fallback).
> Path B (postMessage worklet) + the synth-extraction/plugin angle are parked for future
> research — the threading model, the A/B call, and the build plan live in
> [`audio-threading.md`](audio-threading.md). Next step: Stage 1 (atomics-harden the queue).

## Symptom

- **Native**: tight (any wobble is below the threshold of hearing).
- **Web**: a faint continuous wobble *plus* an **occasional** slip/stumble — the
  groove jumps and resettles. Tempo is right *on average* (no permanent speedup), so
  it's a **placement** problem, not a tempo problem.
- **Web on mobile (2026-06-10, `drift` on an iPhone):** even the **TRUTH** voice —
  the sample-accurate `schedule_hit` reference that *cannot* jitter from scheduling —
  is audibly wobbly. That rules scheduling out as the sole cause and indicts the
  **audio output clock itself** (see suspect #3, now confirmed).
- **Web on desktop (2026-06-10, same `drift` build):** TRUTH is *also* slightly
  not-straight — confirming the residual is **structural, not a mobile-load artifact**.
  Console on the live page: **59 fps** (rendering isn't stealing main-thread time),
  **`sampleRate` 44100** (no hidden resample — that variable is eliminated),
  `baseLatency` ≈ 5.8 ms, `outputLatency` 0. So on a *healthy* desktop (60 fps, no
  resample, low latency) TRUTH still isn't dead-straight → the main-thread
  `ScriptProcessor` ceiling, full stop. (Desktop browser is also the **fast test loop**
  — same backend as mobile, no deploy+phone round-trip needed.)

## How timing works today

The musical clock is a main-thread accumulator, ticked once per frame from
`studio.c` **before** `update()`:

```c
// sound.h:469
static void sound_tick(float dt) {
    beat_accum += dt * (sound_bpm / 60.0f);   // advance by this frame's wall-clock delta
    int new_beat = (int)beat_accum;
    beat_just_advanced = (new_beat > beat_now);   // ← a BOOL (see "dropped beats" below)
    beat_now = new_beat;
    ...
}
```

`dt` comes from the loop. **This is where native and web diverge** — and the
divergence is the whole story:

```c
// studio.c — WEB path (was, before this note's fix):
frame_dt = GetFrameTime();          // raw rAF delta — jittery AND unbounded
sound_tick(frame_dt);

// studio.c — NATIVE path:
double tn = GetTime(); frame_dt = (float)(tn - last_time); last_time = tn;
if (frame_dt > 0.1f) frame_dt = 0.1f; if (frame_dt < 0) frame_dt = 0;   // hitch clamp
sound_tick(frame_dt);
```

Native gets a steadier clock **and** clamps hitches to 100 ms. Web had neither.

### How a note's onset is decided

Two paths, and they explain why the radios are tighter than tutorial drum loops:

1. **Frame-triggered (the drunk path).** A cart reads `every(n)` ("true once per n
   beats — fire it on the beat", `studio.h:387`) or `beat_just_advanced`, then calls
   `hit()` that frame. The onset is therefore *whenever the frame that detected the
   beat happened to run* — quantized to frame timing. Users: `06-sound`,
   `20-instruments`, `21-lfo`, `beatcrypt`, `calgames`, `cannonfodder`, `crowd`, …
2. **Schedule-ahead (the tight path).** `radio.h:130-133` runs a step counter against
   the beat clock and schedules **one step ahead** with `schedule_hit(dly, …)`, which
   carries a sample-accurate `delay_samples` the **audio thread** fires precisely
   (`sound.h:443/486`, the `delayed[]` pen). Its own comment: *"Never trigger on the
   frame (up to 16ms of jitter = a drunk drummer)."* `schedule_hit`'s doc brags
   *"sample-accurate future time… no frame-rate jitter"* (`studio.h:383`).

## Diagnosis (ranked)

### 1. PRIME (web-only, "occasional"): unbounded `frame_dt` lurches the clock

Most frames are only slightly jittery (→ the faint continuous wobble). But every
browser hitch — GC, scroll, tab/background work, a heavy render frame — arrives as
one large `dt`, and with no clamp the beat clock advances by the **entire gap at
once**: `beat_accum += dt`. The sequence lurches forward, then resettles. Tied to
hitches → **intermittent**, exactly "occasional drift." Native clamps the same hitch
to 100 ms, so it just holds steady — which is why native sounds clean.

**Second-order amplifier — dropped beats.** `beat_just_advanced` is a *bool*
(`sound.h:472`). If a hitch jumps the clock 4 → 7 in one frame, it fires **once** —
beats 5 and 6 never trigger. So a web hitch on a frame-triggered cart doesn't merely
slip, it can **drop notes** and resume out of place. (Schedule-ahead carts instead
get a negative look-ahead delay and fire a clump of late notes to catch up — also a
stumble.)

### 2. Continuous: frame-rate quantization on a jittery clock

Even with no hitch, onsets on the frame-triggered path are pinned to the frame grid
(~16.7 ms even at a steady 60 fps = ±8 ms), and web's rAF clock jitters that grid
frame to frame. Below most listeners' threshold for slow material; audible on fast
rhythmic steps. This is the "drunk" baseline; native's steadier clock keeps it under
the threshold.

### 3. CONFIRMED (web, worst on mobile): the audio output clock runs on the main thread

**This is the one that wobbles TRUTH**, and it's the deeper problem. The web build
ships **no AudioWorklet and no worker threads** — neither `build-site.js` nor the
editor's `main.cjs` passes `-sAUDIO_WORKLET`/`-sWASM_WORKERS`. With those flags absent,
emscripten + raylib's miniaudio fall back to a **`ScriptProcessorNode`, whose
`onaudioprocess` runs on the main thread.**

**Confirmed in the shipped artifact (2026-06-10):** grepping the deployed
`site/drift/index.js` finds `createScriptProcessor`: **1**, `onaudioprocess`: **2**,
`audioWorklet`/`AudioWorkletNode`: **0** — the live code literally mixes audio in
`device.scriptNode = device.webaudio.createScriptProcessor(bufferSize, …)` on the main
thread, in our 512-sample blocks. Not an inference; the running cart proves it.

So `sound_callback` (`sound.h:3279`) — which both *generates* the samples and *counts
down* `schedule_hit`'s `delay_samples` — is interleaved with rendering and GC on the
single main thread. On mobile (weaker CPU, busy compositor) the callback fires late and
in bursts, producing audio in catch-up chunks rather than a smooth stream. The **output
clock is therefore unsteady**, and "sample-accurate" loses its meaning: a note booked N
samples out lands whenever the main thread next services the audio node. That smears
*every* voice, **including the schedule-ahead TRUTH** — which is exactly the
on-device finding (symptom §3). The `frame_dt` clamp (suspect #1) is a layer above this
and cannot help it.

### Contributing: the buffer size IS the swing amplitude (confirmed on-device)

The request queue drains **once per callback, at the buffer boundary**
(`sound_callback` step 1, `sound.h:2516`). Scheduled notes then fire sample-accurately
*within* the buffer (`:2534`) — but their countdown **originates at that boundary**.
So a `schedule_hit` note's timing origin snaps to the audio-buffer edge: the main
thread pushes it at frame-time, the audio thread starts counting from the next buffer
boundary, an error of 0…one-buffer per note. As the frame phase beats against the
buffer phase that error sweeps periodically → **swing**, with amplitude ≈ **one
buffer**. This hits *every* scheduled voice, TRUTH included.

**Proven on-device (2026-06-10):** bumping the web buffer 1024→4096 made TRUTH *more*
swingy, not less — at 138 bpm a 16th is 108 ms and a 4096 buffer is ~93 ms, so it ate
almost a whole step. So buffer size is a **no-win dial** on the main-thread backend:
smaller = tighter origin-snap but more underruns; bigger = fewer underruns but worse
swing. The genuine fix decouples the audio clock from the buffer cadence → AudioWorklet
(fix #7). (On a steady dedicated-thread clock the buffer is a pure *latency* knob — the
`product-notes.md` topic — not a swing source.)

## Fixes (ranked)

1. **DONE — clamp web `frame_dt`** (this note's change, `studio.c` web path): mirror
   native's `if (frame_dt > 0.1f) frame_dt = 0.1f`. One line. Converts a hitch from
   "lurch the music / drop beats" into "hold steady and resume in time." Targets
   suspect #1 — the *occasional drift you actually hear*. Does **not** fix the
   baseline frame-quantization wobble (#2).
2. **Engine cure — sample-clock beat.** Advance `beat_accum` from the **samples the
   audio callback actually produced**, not `frame_dt`. Then the beat position is
   rock-steady regardless of frame jitter, which de-jitters `every()`/`beat_just_advanced`
   *and* tightens the radios' look-ahead math. Bigger change; the real fix for #2.
3. **Cart-level — schedule ahead.** Carts on the frame-triggered path adopt the
   `radio.h` pattern (or a future shared helper) and trigger via
   `schedule`/`schedule_hit`/`strum` instead of `hit()` on the beat. Infrastructure
   already exists and is proven on the radios.
4. **Fix the bool** (cheap, complements #1/#2): make `beat_just_advanced` carry *how
   many* beats advanced (or have `every()` fire per crossed beat) so a multi-beat jump
   doesn't silently drop the in-between beats.

### Fixes for suspect #3 (the TRUTH-wobble — the deeper one)

5. **Cheap dial — SMALLER web audio buffer (not bigger).** The buffer length ≈ the
   worst-case swing (above), so on web shrink `SetAudioStreamBufferSizeDefault` rather
   than grow it: 1024→**512** (≈11.6 ms) tightens the origin-snap, at the cost of more
   underrun risk on the contended main thread. Currently set to 512 on web (native
   keeps 1024). This is a *trade*, not a cure — it caps the swing, it doesn't remove
   the buffer-boundary origin. Run the soundcheck tripwire after (CLAUDE.md: any
   `sound.h` change). The 4096 experiment that *worsened* it is what proved the
   mechanism.
6. **Less main-thread load** ([`frame-pacing.md`](frame-pacing.md)): reactive rendering
   / an `fps` cap frees main-thread time for the audio callback, so it starves less.
   Synergistic with #5 — fewer render frames competing with the ScriptProcessor.
7. **The real fix — AudioWorklet.** Build web audio on an `AudioWorklet`
   (`-sAUDIO_WORKLET=1 -sWASM_WORKERS=1`, a **custom backend** calling our mixer — not
   via raylib), so the callback runs on a **dedicated audio thread** with a steady clock,
   decoupled from rendering. This is what actually makes TRUTH tight on mobile, and the
   spike proved it's reachable on Pages. **The full threading model + the runtime
   backend pick + the staged build plan now live in
   [`audio-threading.md`](audio-threading.md)** — that's where the migration is specced.

## AudioWorklet feasibility spike (2026-06-10)

A throwaway minimal worklet (a sine via `emscripten/webaudio.h`, in
`build/.worklet-spike/`) answered the make-or-break questions:

- **Toolchain: ✅** emcc 5.0.7 compiles `-sAUDIO_WORKLET=1 -sWASM_WORKERS=1` cleanly;
  the AudioWorklet API is present.
- **Architecture: ✅** our sample generator + lock-free `req_queue`
  (main-thread producer / audio-thread consumer) is *already* the worklet shape — the
  hard design exists. (Caveat: the queue's `volatile` needs real **atomics/fences**
  under a genuinely parallel audio thread; correctness work, not redesign.)
- **raylib: ⚠️** `runtime/raylib-web/lib/libraylib.a` is **prebuilt** — its miniaudio
  is the default emscripten path (ScriptProcessor). A worklet via raylib means
  rebuilding raylib; the cleaner route is a **custom AudioWorklet that calls our mixer
  directly**, bypassing raylib audio on web (we own every sample anyway).
- **❌ BLOCKER — confirmed: shared memory ⇒ SharedArrayBuffer ⇒ COOP/COEP.** The
  worklet build emits `WebAssembly.Memory({… shared:true})`. Shared memory is backed by
  a `SharedArrayBuffer`, which browsers gate behind **cross-origin isolation**
  (COOP/COEP response headers). **GitHub Pages can't set those headers**, so the build
  won't even instantiate there as-is.

**Ways past the blocker** (the real decision before any migration):
1. **`coi-serviceworker`** — a service worker that injects COOP/COEP client-side,
   enabling isolation on Pages. Works, but adds a SW + a first-load reload + cache
   interplay (with our `?debug=1` overlay, etc.).
2. **Move hosting** (Netlify / Cloudflare Pages) where headers are settable.
3. **No-SAB restructure** (big): run the whole synth+clock *inside* the worklet with its
   own memory, main thread sends only note events via `postMessage` (latency-tolerant
   since notes are scheduled ahead). Avoids SAB, but it's a major audio-engine split.

**On-device result (2026-06-10): ✅ the worklet path is reachable on GitHub Pages.**
A deployable throwaway spike (`site/coi-spike/`, **since removed** after the rollout) was
published — `index.html` with `coi-serviceworker`, a `nocoi.html` baseline. On an iPhone the coi page registered the
service worker, self-reloaded once, flipped `crossOriginIsolated` → true, the
shared-memory wasm instantiated, and the **AudioWorklet sine played** — confirming
`coi-serviceworker` provides the isolation on Pages and the worklet runs there.

Caveats observed:
- **SW state is fragile across navigation/caching.** After bouncing between the coi and
  no-coi pages (same SW scope) the coi page came back in a mixed state
  (`crossOriginIsolated:false` yet the SAB wasm still ran). It's a service-worker
  lifecycle/cache wrinkle, not a worklet failure — a real integration must own SW scope,
  caching, and the one-time reload UX deliberately. (Added a RESET button + a
  `SharedArrayBuffer`-available readout to the spike to get a clean slate.)
- **Occasional tiny dropout in the bare sine.** A *click/dropout* (audio thread missing a
  render quantum under OS/thermal pressure), distinct from drift/swing. The spike has zero
  robustness tuning and a pure tone exposes any glitch maximally; musical content masks it.
  Worklet ≫ main-thread for steadiness, but mobile audio is never *guaranteed* glitch-free.

Bonus finding — **the RATCHET proves `schedule_hit`'s micro-timing survives the web
backend.** A rapid even burst *batch-scheduled in one frame* (24 hits in `drift`) sounds
**perfectly even on web**, native-tight: all hits share one buffer-drain origin, so the
single origin-snap shifts the whole burst while the *internal* spacing stays
sample-accurate. It's purely **cross-note placement** (separate notes, each scheduled on a
different frame → different origin-snap) that smears — which is exactly suspect #3's
buffer-origin mechanism, heard from the other side.

## Verification plan

- **The `drift` cart is the purpose-built A/B rig** ([`../guides/instrument-carts.md`](../guides/instrument-carts.md)
  → Sequencers): it plays the same 16th pulse on all three clocks at once
  (`schedule_hit` / `hit`-on-frame / every-N-frames), panned center/left/right with a
  per-voice error scope, plus a **GLITCH** button that stalls a frame on demand. Solo
  TRUTH = metronome; add the others to hear the wobble/drift; tap GLITCH to see the
  clamp hold the groove through a hitch.
  `drift` also has a **RATCHET** button (key `R`) — a batch-scheduled even burst that
  stays tight on web (see the spike's bonus finding) — and a **GLITCH** button.
- **Desktop browser is the fast loop** — it uses the *same* `ScriptProcessor`/main-thread
  backend as mobile, so it reproduces the timing character without a deploy+phone
  round-trip. Build → open in the browser → judge by ear. Use the `?debug=1` overlay for
  fps + console mirror.
- **Inspect the backend from the console** — paste this *before* the first click, then
  start audio, to read the real context:
  ```js
  (function(){ const O=window.AudioContext||window.webkitAudioContext;
    function W(...a){const c=new O(...a);window.__actx=c;
      console.log('sampleRate',c.sampleRate,'baseLatency',c.baseLatency,'state',c.state);return c;}
    W.prototype=O.prototype; window.AudioContext=window.webkitAudioContext=W; })();
  ```
  Measured on the live `drift` page: 59 fps, `sampleRate` 44100 (no resample),
  `baseLatency` ≈ 5.8 ms — a healthy context that *still* isn't dead-straight (the ceiling).
- **A/B the clamp**: provoke hitches (scroll / background a tab / GLITCH); the clamp should
  remove the hitch-driven slips, residual wobble = the structural buffer-origin snap (§3).
- **Quantify exact ms**: would need an output recording + onset-interval analysis (web has
  no native `--wav` path). Not done — the structural diagnosis didn't need it.

## Further reading — AudioWorklet & web-audio timing (for the fix #7 work)

Worklets vs workers: **ScriptProcessorNode** (what we ship now, by default) runs audio
on the *main* thread — deprecated, the cause of suspect #3. **AudioWorklet** runs it on
the dedicated audio thread ("separate thread, very low latency") — the fix. **Wasm
Workers / pthreads** are emscripten's threading primitives the worklet builds on
(usually `-sAUDIO_WORKLET=1` **and** `-sWASM_WORKERS=1`).

1. **Chris Wilson — "A Tale of Two Clocks: Scheduling Web Audio with Precision"** →
   `web.dev/articles/audio-scheduling`. The foundational read — our TRUTH (schedule
   ahead against the audio clock) is this pattern; explains why the playback clock, not
   the frame clock, governs timing.
2. **web.dev — "Enter Audio Worklet"** → `web.dev/articles/audio-worklet`. Why
   main-thread ScriptProcessor is bad and what the worklet fixes — suspect #3 in prose.
3. **MDN — `AudioWorklet`** (+ `AudioWorkletProcessor`/`AudioWorkletNode`) →
   `developer.mozilla.org/en-US/docs/Web/API/AudioWorklet`. API reference.
4. **MDN — `ScriptProcessorNode` (deprecated)** — what we're on today, and why it's gone.
5. **Emscripten — Wasm Audio Worklets** →
   `emscripten.org/docs/api_reference/wasm_audio_worklets.html`. Running C/wasm audio in
   a worklet — the implementation path for us.
6. **Emscripten — Wasm Workers** → `emscripten.org/docs/api_reference/wasm_workers.html`.
7. **miniaudio** → `miniaud.io`. raylib's audio *is* miniaudio; feasibility hinges on its
   emscripten worklet support (`MA_ENABLE_AUDIO_WORKLETS`).

**The make-or-break gotcha:** threaded wasm / `SharedArrayBuffer` usually needs
cross-origin isolation (COOP/COEP headers), which **GitHub Pages can't set**
(`web.dev/articles/coop-coep`). First feasibility question: does emscripten AudioWorklet
run *without* SAB / cross-origin isolation? If not, the Pages deploy needs a workaround
(service-worker header shim, or a different host).
