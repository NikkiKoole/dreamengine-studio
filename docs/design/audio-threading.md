# Audio threading — the game-thread / audio-thread model (and how it ports)

> SPEC + RATIONALE (2026-06-10). The portable real-time-audio architecture dreamengine
> already uses, why it generalizes to native iOS / Switch / desktop, and the staged plan
> to give **web** a real audio thread (the AudioWorklet backend, runtime-selected).
> Companion to [`audio-timing.md`](audio-timing.md) (which owns *why web drifts*); this
> note owns *the thread structure and the backend migration*. Latency-as-product lives in
> [`product-notes.md`](product-notes.md).

## The one idea: two threads, one of them is sacred

Real-time audio on every platform is built on the same split:

- **Game thread** — runs `update()`/`draw()`. **Soft** real-time: a slow frame just drops
  fps, nobody dies.
- **Audio thread** — has a **hard deadline**: synthesize the next block of samples before
  the device drains the current one, *every time*. Miss it once → an audible click
  (underrun). So the audio thread must **never block, allocate, lock, or do I/O** — only
  "read pending events, synthesize samples, return."

Because the audio thread can't wait on anything, the game thread must **never call into
the synth directly** and they must **never share a lock** (a lock lets the RT thread wait
on the game thread = priority inversion = glitch). They communicate only by **handing off
messages**.

## How they talk: a lock-free SPSC queue (already in `sound.h`)

dreamengine is already built this way:

- The game thread **posts events** — `hit`, `note`, `note_on/off`, `schedule_hit`,
  parameter setters — as `SoundReq` structs into a **single-producer / single-consumer
  ring buffer** (`req_queue`; `req_head` written by the game thread, `req_tail` by the
  audio thread; `sound.h:448-456`).
- The audio thread **drains** the queue at the top of each callback (`sound_callback`
  step 1, `sound.h:2516`), moves delayed notes into the `delayed[]` pen, then mixes.
- **Timing rides inside the message.** `schedule_hit` carries `delay_samples`; the audio
  thread fires it at the exact sample offset (`sound.h:2534-2540`), so placement is
  decided on the audio side regardless of *when* the game thread posted it. (This is why
  a batch-scheduled ratchet is dead-even even on web — see `audio-timing.md`.)

This is the textbook real-time-audio pattern, and it's the load-bearing fact for going
cross-platform: **the synth + the queue are platform-independent C.**

## What changes per platform: only *who runs the audio thread*

Only a thin shim — "open a device, register a callback" — is per-platform. The callback
body (drain queue → mix) is shared.

| Platform | Who runs the audio callback | Memory sharing | Status |
|---|---|---|---|
| **Native desktop** (mac/win/linux) | miniaudio's high-priority audio thread | process RAM (free) | works today |
| **Web — today** | `ScriptProcessorNode` on the **main thread** ⚠️ | n/a (one thread) | the anomaly → drift |
| **Web — worklet** | `AudioWorklet` render thread | `SharedArrayBuffer` (COOP/COEP) | the migration below |
| **iOS native** | CoreAudio / AudioUnit render callback | process RAM | reachable via miniaudio |
| **Switch / consoles** | platform-SDK audio callback | process RAM | needs an NDA'd backend shim |

The key realization: **web-today is the only target without a real audio thread.** Native
*already* runs the queue across a real game/audio-thread boundary. The worklet doesn't
invent threading for the engine — it brings web up to what native already does.

## The correctness tax: atomics, not `volatile`

Across **genuinely parallel** threads the SPSC queue needs **atomic** index access with
**acquire/release ordering** — a memory fence so the consumer sees a *fully-written* entry
before it sees the advanced index, and vice-versa:

- Producer: write the entry, then `atomic_store_explicit(&head, next, memory_order_release)`.
- Consumer: `atomic_load_explicit(&head, memory_order_acquire)`, then read the entry.

Today the indices are `volatile` (`sound.h:455-456`), which is **not** a memory barrier.
It works on native largely by luck (strong-ordered x86/ARM + the access pattern). The
payoff of fixing it: **the same hardened queue is what iOS and Switch need too.** Doing it
once, correctly, serves every parallel-audio platform — so it's a down-payment on the
native ambition, not web-only busywork. (Wasm's shared-memory model in particular wants
real atomics; `volatile` is the most fragile there.)

## Web's *extra* tax: shared memory (a web-only wrinkle)

On web the game thread and the AudioWorklet are separate JS realms, so to share the one
queue the wasm heap must be a `SharedArrayBuffer` → which the browser gates behind
**cross-origin isolation** (COOP/COEP headers). GitHub Pages can't set headers, so we use
`coi-serviceworker` to inject them client-side (proven on-device — see
[`audio-timing.md`](audio-timing.md) feasibility spike). **On native this whole saga
disappears** — threads share the process address space for free. So COOP/COEP /
service-worker is a web deployment detail, not part of the architecture.

## The backend pick: runtime-detected, one wasm

Decided: **detect at runtime, don't fork the build.** At `sound_init` on web:

```
if (crossOriginIsolated)  → AudioWorklet backend   (real audio thread, tight)
else                      → ScriptProcessorNode    (main thread, the current ceiling)
```

One wasm ships both backends; only the *timing ceiling* differs by context. Benefits:
every cart Just Works everywhere (isolated or not, old browser, embed), and we never
maintain two artifacts per cart. Native always uses its real audio thread — no toggle
there. (A `?debug=1` line could surface which backend is live.)

## Cross-platform payoff & the not-free parts

**Payoff:** because the engine is already message-passing over a queue, each new platform
is a *thin audio-backend shim* behind the same queue + the same platform-independent
synth — not a rewrite. And hardening the queue with atomics for the worklet is exactly the
correctness native consoles need.

**Not free (and out of scope here):**
- **Switch** needs an NDA'd console audio backend (miniaudio doesn't cover it) — a shim,
  but a real one.
- **iOS native audio** is largely reachable via miniaudio (CoreAudio); the *hard* iOS
  questions are **packaging** and the **run model** — the `▶ run`/libtcc "compile a cart
  live" path can't ship on the App Store, so carts need AOT compilation. That's a separate
  topic ([`packaging-distribution.md`](packaging-distribution.md)), orthogonal to threading.

---

## Build plan — the web AudioWorklet backend, runtime-selected

Staged so each step is verifiable on its own; **Stage 0 is the real risk to retire first.**

- **Stage 0 — de-risk graphics + worklet in ONE build. LINK ✅ (2026-06-10).** A combined
  spike (raylib `InitWindow` + a worklet sine, in `build/.worklet-spike/combo.c`) **links
  against the prebuilt `libraylib.a` with `-sAUDIO_WORKLET=1 -sWASM_WORKERS=1`** — the
  single-threaded raylib lib is *not* an ABI blocker. The only snag: emscripten's
  wasm-workers libc (`libc-ww`) pulls `clock_nanosleep.o`, which references
  `emscripten_thread_sleep`, undefined under bare `WASM_WORKERS`. **Two fixes, both verified
  to link:** (a) **lean** — add a one-line JS-library stub
  `addToLibrary({ emscripten_thread_sleep: ()=>{} })` (safe: raylib on web yields via
  `emscripten_set_main_loop` and never actually sleeps, so the path is never hit — *confirm
  at runtime*); (b) **fallback** — add full `-pthread -sPTHREAD_POOL_SIZE=1` (heavier).
  Prefer the lean stub. **Still pending: the RUNTIME check** — deploy the combo (with
  coi-serviceworker for isolation) and confirm the GL window actually renders *and* the
  worklet plays in the same build, on desktop + phone.
- **Stage 1 — atomics-harden the SPSC queue** (`sound.h`): `volatile` head/tail →
  C11 `atomic_int` with acquire/release. Platform-independent; benefits native too. Verify
  with the soundcheck tripwire (CLAUDE.md) + a native A/B that nothing regresses.
- **Stage 2 — custom web worklet backend.** When isolated, instead of raylib's
  `ScriptProcessor` AudioStream, create an emscripten AudioWorklet whose `process()` calls
  our mixer (the existing `sound_callback` body) into the output buffer, draining the same
  `req_queue` from shared memory. Bypasses raylib audio on web (we own every sample).
- **Stage 3 — runtime backend pick** at `sound_init` (the `crossOriginIsolated` branch
  above); both backends compiled into one wasm; ScriptProcessor stays the fallback.
- **Stage 4 — coi-serviceworker in the real shell** (`web_shell.html`): the isolation +
  one-time reload, made to coexist with existing site caching and the `?debug=1` overlay.
- **Stage 5 — wire the build flags** (`build-site.js` + the editor's build-web) and
  rebuild the catalog; bigger artifacts (shared memory) are expected. Acceptance test:
  `drift` is **straight** on desktop *and* phone when isolated, and still plays (at the
  ceiling) when not.

Risks to watch: Stage 0 (graphics+threads coexistence); artifact-size/rebuild-time bump;
iOS Safari worklet quirks + the audio-unlock gesture; and keeping the ScriptProcessor
fallback path compiling alongside the worklet path.
