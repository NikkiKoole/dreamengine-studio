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

## The backend pick + auto-fallback

Goal: a cart **uses the worklet when it can and falls back to ScriptProcessor when it
can't**, so it plays everywhere — isolated or not, old browser, embed.

**Correction (2026-06-10):** an earlier draft said "one wasm, runtime pick." That's wrong
for the shared-memory path: a worklet wasm emits `WebAssembly.Memory({shared:true})`, which
**cannot instantiate without cross-origin isolation** — exactly why `nocoi.html` aborts in
the spike. So you can't branch backends *inside* one module; the fallback is structural.
Two honest ways:

- **Path A — two builds + a loader (pragmatic; Stage 0 de-risked).** Build each cart twice:
  the worklet/shared-memory wasm AND the plain ScriptProcessor wasm (the latter is exactly
  what ships today). A small loader picks at load: `crossOriginIsolated ? worklet : plain`.
  Auto-fallback ✓. Cost ≈ 2× build time + storage per cart, but the fallback half is the
  current build (additive, not new). Needs coi-serviceworker for the worklet half.
- **Path B — synth-in-the-worklet, no shared memory (bigger).** Run the synth + clock
  inside the worklet with its own memory; main thread posts note events via `postMessage`
  (latency-tolerant — notes are scheduled ahead). No shared memory → no isolation, no
  coi-serviceworker, no two builds — works everywhere from one build. **Correction
  (don't re-believe the earlier draft):** this does **NOT** map better to native —
  A's shared-memory ring *is* native's transport (miniaudio's audio thread draining
  `req_queue`); B's `postMessage` is a **web-only** transport native would never use.
  B's only real edge is web *deployment* simplicity, not portability. Cost: a real
  restructure of where the synth runs.

Native is unaffected either way — always a real audio thread, no toggle. `?debug=1` can
surface which backend is live.

## Decision (2026-06-10) + the parked Path-B / plugin thread

**Web now → Path A.** It's the better *and* cheaper call: the fallback is the build we
already ship, it mirrors native's transport, it reuses emscripten's worklet helper, and
Stage 0 already de-risked it. Path B's one advantage (no isolation/coi-serviceworker) is
**mitigated** — the self-heal made coi reliable and the free fallback covers any context
where isolation fails. So we don't take on B's big rebuild to dodge a problem we've tamed.

**Parked for future research: Path B + the "synth-extraction" refactor.** There's a
third, more fundamental thing under B: **lift `sound.h` out of the engine into a
host-agnostic "push events / render samples" module** (decoupled from raylib's audio
device and the game binary). That refactor — *not* web Path B specifically — is what an
**AUv3 / plugin** would need (native iOS/macOS; the host gives you the audio thread). So:
- **AUv3 needs the extraction** (native, *not* web).
- **Path B needs the extraction** (web, separate-memory worklet).
- **Path A does NOT** — `sound.h` stays welded in, just runs on the worklet thread.

They're *correlated through the extraction*, not the same thing: do the extraction for
any reason (cleanliness, a plugin) and Path B gets cheap as a byproduct, and vice-versa.
Absent a real plugin/native-instrument push, neither is justified now. Gated on the
product direction in [`product-notes.md`](product-notes.md) (the AUv3/Link parked items).
**When we revisit, start from this section + `audio-timing.md` — the findings, the spike
(`site/coi-spike/`), and the COOP/COEP + thread-sleep + shared-memory facts are all here.**

Stage 1 (atomics) hardens the shared queue native uses *today* and Path A needs — worth
doing regardless of the eventual A/B call.

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
  Prefer the lean stub. **RUNTIME ✅ (2026-06-10):** the combo
  (`site/coi-spike/combo.html`) on GitHub Pages — the raylib GL window **animates**, the
  worklet **roll plays dead-even**, **CPU stays low** (so the lean stub is *never hit* —
  confirmed safe, no `-pthread` needed), and the page is isolated. **Graphics + a real
  audio thread coexist in one shared-memory build. Stage 0 fully retired.**
- **Stage 1 — atomics-harden the SPSC queue ✅ (2026-06-10).** `volatile` `req_head`/
  `req_tail`/`sound_dropped` → C11 `atomic_int` (`sound.h`): producer release-stores
  `req_head` *after* writing the entry, consumer acquire-loads it (so a fully-written entry
  is visible before the index advance), `sound_dropped` via relaxed atomic fetch-add (two
  writers). Compiles on clang (native) **and** emcc (web); soundcheck tripwire passes (no
  dropped requests). Platform-independent — hardens native *today*, ready for the worklet.
  (No tcc concern: `sound.h` is clang-compiled into the host even in the libtcc backend.)
- **Stage 2 — custom web worklet backend ✅ code (2026-06-10).** Built it: a
  `#if defined(PLATFORM_WEB) && defined(DE_AUDIO_WORKLET)` block in `sound.h` creates an
  emscripten AudioWorklet whose `process()` calls the existing `sound_callback` into a
  128-frame interleaved scratch and de-interleaves into the planar L/R quantum (the mixer
  is reused verbatim, draining the now-atomic `req_queue`). `sound_init` branches to
  `sound_worklet_init()`; `studio.c` resumes the context on the click gesture.
  **Key gotcha found + fixed:** linking the full engine pulled in raylib's `raudio.c`,
  which needs **pthreads** the lean `WASM_WORKERS` build doesn't provide
  (`undefined symbol: pthread_mutex_lock`). Fix = **fully bypass raylib audio in the
  worklet build**: guard `InitAudioDevice` (`#ifndef DE_AUDIO_WORKLET`) and route
  `SetMasterVolume` through a `de_master_volume()` macro that no-ops in the worklet build,
  so `raudio.c` is never linked. Builds + links clean on *both* targets; native tripwire
  passes. **TODO:** pause-mute (was `SetMasterVolume`) is a no-op in the worklet build —
  route it through the mixer later. A `build-site.js --worklet` flag builds the variant
  into `site/<name>-worklet/` (+ the `runtime/audio-worklet-stub.js` thread-sleep stub).
  **RUNTIME VERIFIED ✅ (2026-06-10):** on `site/drift-worklet/` (isolated via
  coi-serviceworker) **TRUTH is dead-straight**, vs the audibly wobbly ScriptProcessor
  `site/drift/`. End-to-end proof: dedicated audio thread (128-sample/~2.7ms quantum)
  removes the buffer-origin swing. The whole chain holds — diagnosis → worklet → fix.
- **Stage 3 — runtime backend pick** at `sound_init` (the `crossOriginIsolated` branch
  above); both backends compiled into one wasm; ScriptProcessor stays the fallback.
- **Stage 4 — coi-serviceworker in the real shell** (`web_shell.html`): the isolation +
  one-time reload, made to coexist with existing site caching and the `?debug=1` overlay.
  **SW lifecycle lesson (from the spike):** the worker is what *provides* isolation, so it
  must **stay registered** — never auto-unregister on load (that permanently un-isolates →
  no worklet). Happy path = register once → one take-control reload → isolated on every
  later visit, no reload. The mixed state (`crossOriginIsolated:false` but SAB present)
  came from two pages sharing one SW scope + bouncing between them + the once-per-session
  reload guard + caching. Fix = **self-heal, not always-reset**: on load, *only if*
  not-isolated AND a SW is already registered, do one clean unregister+clear+reload
  (guarded once/session to avoid loops); otherwise leave it alone. Better still for
  production: own a single SW scope + caching so it never breaks. (Spike pages now do the
  self-heal — `site/coi-spike/*.html`.)
- **Stage 5 — wire the build flags** (`build-site.js` + the editor's build-web) and
  rebuild the catalog; bigger artifacts (shared memory) are expected. Acceptance test:
  `drift` is **straight** on desktop *and* phone when isolated, and still plays (at the
  ceiling) when not.

Risks to watch: Stage 0 (graphics+threads coexistence); artifact-size/rebuild-time bump;
iOS Safari worklet quirks + the audio-unlock gesture; and keeping the ScriptProcessor
fallback path compiling alongside the worklet path.
