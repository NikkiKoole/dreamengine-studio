# Checks & oracles — *I changed X, what do I run?*

> **Why this exists.** The repo has ~25 verification tools, but the tool list (CLAUDE.md) is a
> *forward* index (tool → what it does). When you're mid-change you need the *reverse* map
> (**what you touched → what proves it still works**). Agents kept missing the right oracle because
> nothing answered that question in one place. This is that place. Run the matching check after the
> matching edit — all are deterministic and headless, so they're cheap and they belong in the commit.

Full contract for any tool is in its file header (`node tools/<name>.js --help` or read the top).
This page is only the routing layer.

## Render / draw primitives (`studio.c` draw layer)

| You changed… | Run | What it proves |
|---|---|---|
| a software-canvas primitive (`spr`/`map`/fills/`line`/blits) | **`canvas-diff.js <cart>`** | GPU vs `DE_SOFTWARE_CANVAS` render match (handles the `sw_force_gpu` + `DE_CPU_RASTER` gotchas for you) |
| **any** draw primitive (the everything-cart) | **`canvas-diff.js drawall`** | `drawall.c` exercises EVERY draw command with per-frame rotation — one run covers the whole draw layer, at **budget 0** (byte-exact since the 2026-07-02 dest-pixel-centre sampling fix; was `--max 80`). **Adding a draw command? add a call there too** (CLAUDE.md "Adding a new API function" step 5). |
| `cls`/`pset`/`pset_rgb`/fills on the canvas (byte-exactness) | the `swcanvas_test` **cart** (`canvas-diff --bytecheck`, or the two-run `shasum` in its header) | byte-identical GPU↔SW for the integer primitives |
| anything that should be **left/right symmetric** | **`mirror-diff.js <cart>`** | the render mirrors about its centre (catches handed rasterizers) |
| a **coverage-based road / field** (streetlab, roadlab) | **`road-check.js [--all] [--overlay]`** | framebuffer invariants (no naked edges/strays/floating kerb) at any angle |
| `fill`/`outline`/`dither` of a shape | the `raster_test` **cart** (`tools/raster_test.script`) | fill, outline, dither, solid all agree pixel-for-pixel (`rasterization-consistency.md`) |
| **any** draw API signature / new API fn | **`build-all.js`** | every one of 400+ carts still compiles against `studio.h` (catches API rot) |
| **`studio.c` / `studio.h` itself** (not just carts) | **`build-all.js` AND `bash tools/build-nr.sh <cart>`** | build-all only compiles the **Raylib** path — it MISSES the `DE_NO_RAYLIB` software build (iOS / `build-nr`). A raylib-only symbol (e.g. `GetScreenWidth`, `ImageCrop`) added to a shared path compiles under build-all but breaks iOS. `build-nr.sh` (or a quick `clang -c runtime/studio.c -DDE_NO_RAYLIB=1 …`) catches it. (Bit the device-adaptive `--resize`/overlay work, 2026-07-04.) |
| a software-rasterizer's **float math** (cross-device determinism) | **`bash tools/det-probes/run.sh`** | the rasterizer picks bit-identical pixels on arm64 / x86-64 / wasm (replays/ghosts/lockstep precondition) |

> **Before *researching* a software-rasterizer question, check `tools/det-probes/` first** — its README
> is the index. Beyond the `run.sh` determinism gate, it holds the **design-exploration probes** that
> already settled the SW conventions with measured evidence (`rotfill` = inverse-map fills are gap-free;
> `rotline`/`rotstroke` = crisp rotated strokes are correct, shimmer is the only residual; `rotspr`/
> `textrot` = nearest == GPU quality, RotSprite is the ≥16px opt-in; `stritex` = SW tritex tiles gap-free).
> This is where "has someone already studied rotated-X?" is answered — don't re-derive it.

## Performance (no output change intended)

| You changed… | Run |
|---|---|
| an engine primitive, want the fleet-wide cost | **`profile-fleet.js [carts] [--frames N]`** — `workMsAvg` + draw-call counts from `perf.json` |
| optimizing one cart, want a keep-both A/B | the env-flag loop in **[`engine-optimization.md`](engine-optimization.md)** (byte-identical dump oracle, then measure) |
| want hot functions / call paths (not just cost) | the ⏱ profile button — **[`profiler.md`](profiler.md)** (the `sample` call-graph; `profile-fleet` is cost-only) |

> A/B a software-canvas cart cleanly: `DE_CPU_RASTER=on` (reference) vs `DE_SOFTWARE_CANVAS=on` — and
> pick a **rotation-free** cart, or the `=on` build silently falls back to GPU (`sw_force_gpu`). The
> `canvas-diff` tool encodes both. Full recipe + the map-cart numbers: [`../design/software-canvas.md`](../design/software-canvas.md).

## Audio (run the one matching the edit — findings in [`../design/audio-notes.md`](../design/audio-notes.md))

| You touched… | Run |
|---|---|
| `runtime/sound.h` (queues/requests) | `play.js soundcheck script /dev/null --headless --frames 900 \| grep "\[sound\]"` — silence = PASS |
| a **pitched** engine | **`tune-check.js --quiet`** (SINE = 0¢ control) |
| a **filter** (topology/resonance/dispatch) | **`filter-spec.js`** — slope + res peak + bass drain vs the blessed table in audio-notes §25 |
| **levels / effects** | **`level-check.js`**, **`fx-check.js`**; feedback/voice-lifetime → **`soak-check.js`**; DC offset → **`dc-check.js`** |
| engine math / optimizer | **`web-audio-check.js`** (wasm-vs-native parity) |
| want a WAV A/B vs navkit | **`wav-analyze.js`** + **`wav-correlate/-envelope/-modrate.js`**, **`harmonic-spec.js`** |
| an effect wired into `update()`/`draw()` | **`lint-fx-frame.js --quiet`** (catches set-and-hold effects rebuilt every frame) |

## Cart logic, registration, docs

| You changed… | Run |
|---|---|
| game logic in a cart with a `spec()` | **`spec.js [cart] --quiet`** (the gameplay-logic gate) |
| **lockstep netplay** (`runtime/net.h`, the `inp_*`/`btn` seam, `tools/net-relay.js`, anything that could desync) | **`net-check.js --quiet`** — the one-command gate, three legs: **echo** (pong vs the loopback fake peer: P2 must mirror P1 exactly — the remote-input-injection path with no sockets), **netdemo** (a real host+joiner pair over UDP loopback, different per-side scripts, per-frame trace diff → `LOCKSTEP OK`/`DESYNC`), **relay** (two simulated carts through a real `net-relay.js` speaking the web wire protocol: ROLE→HELLO→WELCOME{seed}→INPUT→BYE). Exits nonzero on any failure. The netdemo leg alone: `play.js pong netdemo --headless --frames 600 --host-script tools/clips/pong/01-netcheck-host.script --join-script tools/clips/pong/02-netcheck-join.script` |
| a squishy brush or a rim/fill feature (does every brush still apply the features it should?) | **`squishy-features.js`** (renders the `SQUISHY_MATRIX` grid + pixel-diffs each brush×feature cell vs baseline against a declared support matrix — catches a feature that silently no-ops for one brush) |
| `index.json` (tags / registration) | **`lint-carts.js`** |
| anything under `docs/` | **`lint-docs.js`** (links resolve / §-refs / tool-index) |
| reorganized docs, want gaps not breaks | **`lint-xrefs.js [topic]`** (docs that should cross-link but don't) |
| a cart's source/sprites (is it baked/published?) | **`cart-status.js --quiet`** |
| **laying out / sizing a cart's UI** (fit a phone, multiple screens, touch-target size) | reach for **`runtime/lay.h`** (immediate-mode layout vocabulary — split/at/cell/grid/wrap/fluid; see [`../design/device-adaptive-layout.md`](../design/device-adaptive-layout.md)); then **`mobile-lint.js`** flags **tiny touch-targets** and names the remedy — **`ui_loupe(1)`** (the `ui.h` fat-finger magnifier, [`../design/loupe-notes.md`](../design/loupe-notes.md)) or reflow |
| effects/UI placement, want a "can a phone play this" report | **`mobile-lint.js`**, **`ui-audit.js`** |

## Orienting *before* a change (don't dive in blind)

| You want to know… | Run |
|---|---|
| what's designed and where it stands across the whole repo (ready/building/shipped) | **`design-board.js`** (`ready` = the unstarted backlog; `--lint` = unmarked) |
| everything about a cross-cutting FEATURE/theme (docs + code + carts) | **`topic-brief.js "<theme>"`** |
| everything about ONE cart (context + source map) | **`orient.js <cart>`** |
| how often an API fn is used / its blast radius | **`api-usage.js`** (also cross-checks `studio.h` ↔ docs ↔ `shell.js`) |
| one cart's screen/dims/source-drift/registration | **`cart-info.js <cart>`** |
| which carts are complex / spec-worthy | **`cart-analyze.js`** |
| what cart teaches technique X | **`cart-index.js`** |
| cross-cart duplication (refactor candidates) | **`cart-dupes.js`** |

> **Answer a CLAIM by reading, not by grepping.** A raw `grep` finds *candidates* — it does **not**
> establish a **count** ("N carts do X") or a **capability** ("the engine has no Y"). Both mislead: a
> keyword match hits prose and unrelated uses, and an API-*name* search misses capabilities that are a
> *composition* of existing calls, not a single symbol. Two misses that cost real time (2026-07-09,
> motionbox):
> - *"the engine has no supersaw / chord-from-one-note primitive"* — from an `api.js` name search → no
>   match. **Wrong:** `chord()` is built in, and a supersaw is the *mt70 multi-slot trick* (`moog.c` had
>   already built it, zero engine code). A capability is often a recipe, not a symbol.
> - *"many carts hand-roll unison"* — from `grep -l 'detune|unison'` → ~8 hits. **Wrong:** reading each,
>   only 3 genuinely stack detuned slots; the rest matched "detune"/"fat" in prose.
>
> So: **grep (or the tools above) to find candidates → open each and read it → *then* assert.** For
> "does capability X exist?" prefer **`api.js`** + **`topic-brief.js`** (they point at the recipe, not
> just the name); for "which / how-many carts do X?" use **`cart-index.js`** and confirm the shortlist
> by reading. The number, or the "there's no such thing," you're about to write down is exactly the
> claim most worth double-checking.

---

*Adding a check?* Put the contract in the tool's header + one line in CLAUDE.md's tool list, then add
a row here so the next person finds it from the task, not just the tool name.
