# Checks & oracles — *I changed X, what do I run?*

> **Why this exists.** The repo has dozens of verification tools (37 carry a `--selfcheck` alone),
> but the tool list (CLAUDE.md) is a
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
| **any** draw primitive (the everything-cart) | **`canvas-diff.js drawall`** | `drawall.c` exercises EVERY draw command with per-frame rotation — one run covers the whole draw layer. Budget is the cart's declared **`--max 64`** (a `// canvas-diff: max` directive canvas-diff reads): almost everything is byte-exact, and the one accepted ~63px is the FRACTIONAL-scale `sspr` (16→24) whose nearest-neighbour texel-boundary ties aren't byte-portable across GPUs. **This is a GPU↔SW PARITY oracle — it catches gross breakage, NOT a subtle ±1-texel sampler change** (a parity oracle can't: on a GPU that rounds the tie down, the old truncation bug even makes the diff *shrink*). **Now automated: the `sw canvas` row in `repo-doctor` runs `canvas-diff drawall --golden` every time**, because as a manual command it sat RED for 20 days (the golden was blessed three minutes before `drawall` gained its blend strips and nobody re-blessed). To lock the SW sampler's absolute output use **`canvas-diff.js <cart> --golden`** (SW render vs a committed golden PNG in `tools/canvas-golden/<cart>/`; deterministic across machines, so it fails on any sampler/rasterizer drift regardless of GPU tie-breaking; `--bless` to (re)record). **Adding a draw command? add a call there too** (CLAUDE.md "Adding a new API function" step 5). |
| `cls`/`pset`/`pset_rgb`/fills on the canvas (byte-exactness) | the `swcanvas_test` **cart** (`canvas-diff --bytecheck`, or the two-run `shasum` in its header) | byte-identical GPU↔SW for the integer primitives |
| anything that should be **left/right symmetric** | **`mirror-diff.js <cart>`** | the render mirrors about its centre (catches handed rasterizers). A bare run MEASURES and exits 0; `--quiet` gates at zero, and **`--expect <n>` gates a nonzero accepted floor** — which is what a "68=68" gate needs and could not express before, so it was a person reading two terminal outputs. A count *below* the expectation fails too. |
| a **coverage-based road / field** (streetlab, roadlab) | **`road-check.js [--all] [--overlay]`** | framebuffer invariants (no naked edges/strays/floating kerb) at any angle. Note `unknown` is info-only unless `--strict`; the control now refuses a frame that could not have failed at all (no asphalt / no kerb / no open colour / an empty play area) |
| a **procedural street-network generator** (citygen.h, the worldgen rungs) | **`sndi-check.js <real.rvb \| graph.json>…`** | THE realism number: the SNDi composite (degree shares/dendricity/circuity/sinuosity/orient-entropy) vs real cities — generated matches real = done |
| `fill`/`outline`/`dither` of a shape | the `raster_test` **cart**: `node tools/play.js raster_test script tools/raster_test.script --headless --trace build/raster_trace.jsonl --frames 60` | fill, outline, dither and solid all agree pixel-for-pixel. **PASS = every `fs=2` frame reports `mismatches:"0"` and the `eq` line shows `total=0`.** Interactively instead: drag `editor/public/carts/raster_test.cart.png` into the editor (Z outline · X dither · C cycles 4 pages · SPACE analyse). ([`rasterization-consistency.md`](../design/rasterization-consistency.md)) |
| the **off-screen bbox clamp** on `tri`/`trifill`/`thickline` (a perf cliff, not a wrong pixel) | the `trifill_stress` **cart** + the ⏱ profiler | a pinwheel of thin tris reaching ~1100px off-screen. It should hold 60fps **with reach cranked to max**; if the clamp regresses, pushing reach tanks the fps (was **~46.7 ms** unclamped vs **~2.7 ms** clamped at the defaults). It runs `raster_test` for correctness, so this is the *budget* half only — the pixels are that row's job |
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
| a **pitched** engine | **`tune-check.js --quiet`** (SINE = 0¢ control). An engine that is *supposed* to leave equal temperament declares its curve in `INTENDED_DETUNE` (PIANO's stretched tuning) and is gated as a **differential**: the sweep renders it twice, once with the curve switched off at runtime (`MODE_PIANO_STRETCH`), and asserts the *difference* equals the intended curve to ±0.6¢. Deviation from ET cannot decide this — the ET reading sat inside tolerance with the feature fully working, half working and not working at all, which is how §I4c survived. The differential also cancels the loop's own delay error (§I4d), which is why it needs no blessed baseline. **Adding a sweep entry? raise `renderSweep`'s frame count** or the last entry truncates silently |
| a **filter** (topology/resonance/dispatch) | **`filter-spec.js`** — slope + res peak + bass drain vs the blessed table in audio-notes §25 |
| **designing** a dispersion / allpass cascade (before writing any engine code) | **`disp-model.js`** — solves the coefficient for a target inharmonicity `B` analytically, plus the phase delay it adds at the fundamental (which must be subtracted from the delay line) and whether the line survives at that pitch. **Do not patch `sound.h` to search a grid**: it is slow, and it holds a shared engine broken while parallel agents compile against it — and a timeout's SIGTERM skips your `finally` restore, which left the engine patched twice on 2026-07-30. Model the sweep, patch only to confirm one chosen point |
| edited **`docs/STATUS.md`** (the shipped/open/cut ledger) | **`status-check.js --check`** — catches what no linter can, because none of it is a broken link: a **DONE marker inside `## Open`** (38% of the backlog when the check was written), an entry over the 25-line budget (rationale belongs in the owning design doc), an undated entry, Shipped out of reverse-chronological order, numbering inversions, a bloated `_Last updated:_`, and a dead "see Decided-against" pointer. Bare (no `--check`) prints the index the file lacks. **⚠ Never renumber to fix an inversion** — ~30 `STATUS #N` references across docs, tools and cart sources resolve today; reorder the entries instead |
| added/changed an **`instrument_mode` aux param** or a `MODE_*` constant | **`lint-aux-params.js --quiet`** — the width is written in five places (both `eng_p[]` decls, BOTH `idx >= N` bounds, the note-on copy, the MODE_* constants + their 4-place registration) and missing one makes the parameter **silently inert**: the setter accepts the value, queues it, the handler drops it. Has bitten twice — piano's decay/knock sliders dead for months, then `MODE_PIANO_STRETCH` reading 0¢ everywhere |
| bound a knob to a **host parameter** (`param_bind`), or touched the parameter queue | **`bash tools/param-check/run.sh`** — 9 assertions over the declaration table, the queue, the frame drain, the range clamp, and that a written value reaches the DSP and not merely the variable. TWO negative controls: two untouched renders byte-IDENTICAL, and a write to an address NOBODY BOUND changing nothing (else any traffic through the queue looks like a working parameter). ⚠ **ENGINE HALF ONLY** — that a HOST sees the tree is proven out of process by `./au-transport-check --params` in `ios/mac.sh`, and a green run here says nothing about it |
| **inharmonicity / dispersion / stretched tuning** — the allpass chain in a string engine, a partial-ratio table, anything claiming partials are "stretched" | **`inharm-spec.js`** — partial frequencies in cents vs the ideal `n·f0`, plus the fitted stiff-string `B` and its residual. `tune-check` measures the FUNDAMENTAL and `harmonic-spec` measures partial LEVELS, so between them a partial can drift 30¢ sharp and read only as slightly quieter — which is how PIANO shipped with an inert dispersion chain (**§I4b**) and a stretched-tuning seam working in the treble but cancelled in the bass (**§I4c**) while both gates stayed green. **Run `--check` first**: a broken tool and a genuinely harmonic engine print the same table. **A voice's SUSTAIN changed?** `inharm-spec <wav> --f0 <hz> --decay` gives per-partial decay in dB/s — `wav-envelope` measures the whole signal and so cannot separate "the fundamental dies faster" (a loop bug) from "only the upper partials do" (spectral), which is the distinction that settled §I4b |
| **levels / effects** | **`level-check.js`**, **`fx-check.js`**; feedback/voice-lifetime → **`soak-check.js`**; DC offset → **`dc-check.js`** |
| engine math / optimizer | **`web-audio-check.js`** (wasm-vs-native parity) |
| want a WAV A/B vs navkit | **`wav-analyze.js`** + **`wav-correlate/-envelope/-modrate.js`**, **`harmonic-spec.js`** |
| an effect wired into `update()`/`draw()` | **`lint-fx-frame.js --quiet`** (catches set-and-hold effects rebuilt every frame) |
| a **table/shape swapped mid-note** — `wave_set` while a voice runs, a re-quantised morph, a retuned envelope shape | **`click-check.js <wav> [--quiet]`** — first difference vs the LOCAL step-rms, so a saw's flyback is not a false positive. A cart's own waveform slope sits at 2-3x; an audible click is 6-20x. **Render the unaffected mode as the control** and compare, because the threshold is only meaningful against the same voice. `martenot`'s morph shipped with 13 splice-like events (worst 15.4x) under a source comment asserting the stepping was inaudible: an amplitude/centroid envelope cannot tell a clean ramp from a splice, so nothing we had could see it until an ear did |
| a voice/solo **drops out or is "cut off by another instrument"** | **`voice-trace.js <trace>`** (reads a `--trace` run's on/off/reuse/steal/choke — is it real voice loss, and by whom?) + **`play.js … --solo-slot <n>`** (stem render — or is it just masked/quiet?). Design: [`../design/audio-voice-debugging.md`](../design/audio-voice-debugging.md) |
| **anything STEREO** — `autopan`, `pan()`/`instrument_pan`, `pan_law`, chorus/flanger width, the stereo-linked master soft-clip | **`stereo-check.js <wav> [--expect mono\|wide\|decorrelated\|autopan] [--rate hz]`**. Every other gate on this page reads the WAV through a `readWavMono()` that averages L and R at the door, so **none of them can see any of this** — and a mono downmix is not merely lossy, it is *blind by construction*: antiphase panning has `gL+gR ≈ constant`, so summing removes the effect almost perfectly and the louder the pan the less a mono gate sees. Judge motion by the **pan trace** (excursion + rate), never the mean: a hard L↔R sweep and a dead-centre file have the same average pan. **Run `--check` first** — a broken analyser and a genuinely centred file print the same numbers. `--expect wide` gates WIDTH only, deliberately not correlation, because a mono source panned hard left is one signal at two gains and stays at `corr` 1.0 |
| a stage/pedal/rack you can **switch OUT**, and any claim that its bypass is clean | **`bypass-check.js [--stage NAME] [--quiet]`** — the only gate that asks *does the mix come back*. **Do not reach for a sha:** every stage differs while it is IN, so `sha(A) != sha(B)` is true of every working effect and says nothing. The number is the **last differing sample**, and the tolerance is a **time plus a residual ceiling per stage**, never a boolean: `eq 0/0/0` and `drive_insert(0)` return on the switching sample, but `eq_inst` reconstructs its input as `lo + mid + (hi - mid)`, an **algebraic** null and not a float-exact one, so a nulled EQ whose retained state was charged by something upstream leaves ≤1 LSB for ~17 ms; a reverb send returns only after the tail decays (measured 3.1 s). Its own failure mode is a **vacuous pass**, so it refuses four of them: a byte-identical pair reads INCONCLUSIVE (the toggle never reached the DSP), a difference still running when the render ENDS is the render length, a sub-audible or 0.2%-of-samples in-window difference means the stage was inert, and the baseline is rendered **twice** as a determinism control. It also fingerprints `runtime/` before and after and says **THE ENGINE MOVED** rather than reporting a finding, because a parallel agent landing in `sound.h` between two renders compares two different builds |
| **the PSOLA pitch engine** (`at_psola_slot`, `sample_autotune`, `sample_shift`, `autotune_mic`, `harmonize_mic`) | **`psola-check.js`** for artifacts (does it CLICK) **and** `formant-check.js` for correctness (is it in TUNE). Run psola-check BEFORE and after: it renders `voxshift`'s four takes through three detectors, and **no single one of them is sufficient** — a period-doubled take is still perfectly periodic, so a periodicity metric scores that regression as a 2x *improvement*. This gate exists because a full green sheet plus exact pitch numbers shipped audible popping anyway |
| a **per-voice body / resonator / blend** you can turn up, and the question is whether the note's own TAIL survived | **crest**, out of `wav-analyze.js` — not a decay measurement. An eaten tail **raises** crest, because the peaks survive and the tail energy goes; if crest is flat while rms tracks peak, the loss is honest attenuation and the ringdown is intact. Reach for this instead of trying to isolate one note's decay inside a 16th groove, which is the situation you are usually in. The §M2 trap it covers: blending a body as a crossfade rather than additively discards the string's own ringdown, and at 0.8 made a pizzicato die twice as fast. ⚠ **A body's level change also depends on the slot's POLYPHONY, not on the instrument** — see [`../design/audio-notes.md`](../design/audio-notes.md) §28 before reading a level drop as a failure |
| **A/B-ing ONE VOICE inside a busy cart** (a groovebox, a radio station, a band) | **`ab-render.js <cart> --set <ident>=<v1,v2,…> --play-arg --solo-slot --play-arg <n>`** — flip one file-scope value, render each variant, table the shas. **A/B on the voice's STEM, never the mix**: a groovebox peaking at −0.4 dBFS with an 8.3 kHz centroid moved 25 Hz of centroid when one bass voice changed, and the same change on that voice's own stem was a full dB of peak. Same lesson as `drumkit.h`'s 28 dB of intrinsic loudness spread, one layer up. Two traps `ab-render` now names for you: a **SILENT** render is byte-identical too (carts whose transport boots STOPPED, and played instruments with no transport at all, render digital silence from the default `script /dev/null` — drive them from `tools/clips/<cart>/`; the tell-tale is two DIFFERENT carts returning the same sha), and it **shouts if two variants render byte-identical audio**, because that means the flag never reached the DSP and every number in the table is meaningless |
| handing a **LISTEN item to the owner's ear** (the amount of a thing, a voicing, anything no oracle can judge) | Not a gate, a handover rule, and both halves were learned by getting them wrong: **TRIM the render to where the instrument actually plays** (a radio station's violins entered at ~15s because the entrada is trumpets, so the stem opened with 15 seconds of digital silence and the honest response was "I don't hear anything"), and **LIFT the level with the SAME gain on every variant** — a stem sits ~20 dB below a mix, and normalising per file erases the level difference that is often half of what is being judged. Render a **ladder** (`--set X=0.0f,0.3f,0.6f,1.0f`), not a pair, when the question is "how much" |

## A pure REFACTOR (the output must not change at all)

| You changed… | Run | What it proves |
|---|---|---|
| moved state around without meaning to change behaviour — the per-instance-context work, an extraction, a reorder | **`refactor-guard.js --bless`** first, then **`refactor-guard.js`** after every step | six probe carts are **byte-identical** across audio, frames and the `watch()` trace, with the divergence **located** ("audio diverges at 3.1s", "frame 137"). ⚠ **Every other gate on this page is the wrong shape for this job**: they assert semantic properties and therefore have tolerances, and a state move can slip inside all of them at once. Use `--full` to run those too, as a second opinion, not as the gate. Its own hazard is a *vacuous* pass, so probes carry liveness assertions and a dead probe fails loudly — that caught 2 of the first 6 probes chosen. **If your change touches something no probe reaches, add a probe and re-bless**; a green over a probe set that misses your edit is the same as no run at all |
| touched a **host↔engine seam function** (`de_frame`/`de_resize`/`de_copy_frame`/`de_audio_render`/…), or added a component that creates an engine | **`lint-engine-seam.js --selfcheck`**-gated run (a `repo-doctor` row) | three bugs shipped in one day because the per-instance refactor changed what host code MEANS without changing what it SAYS: two components each booting their own rack (silence on device), six seam fns that take a `DeInstance *` and then drop it for the thread-local (never fails loudly, and the handle in the signature makes it READ as done), and a cart declaring its own 2-arg `extern` against a 3-arg definition (UB no compiler sees across TUs; it surfaced as a crash with `in = 0xa7`). A file may create an engine only if it says `de:engine-owner` |
| **per-instance engine work** (a state move into a context, a new `*_ctx.h`) | **`bash tools/instance-check/run.sh`** | ONE process runs N INDEPENDENT engines: two instances driven with different transport must differ in frames AND audio. ⚠ This is what `refactor-guard` structurally CANNOT check — it runs one instance, so it proves a state move changed nothing and can never prove two instances are strangers (a variable wrongly left SHARED does not change a single-instance render). Negative control: two fresh instances driven the SAME must be byte-identical. Also gates destroy (heap flat over 8 rounds) and sample-exact interleaving |
| added or reordered a **saved-state slice** (`de_state_for_saved` / `DE_CTX_BLOCK_SAVED`) | **`node tools/lint-saved-state.js`** then **`bash tools/state-check/run.sh`** | the lint: nothing in a saved slice may be meaningful only to the instance that wrote it (a POINTER is an error, a live VOICE HANDLE is advisory and matched by name, because a handle is a plain `int`), plus **APPEND-ONLY** against `tools/saved-state-layout.json` — the engine matches a blob to its slice by (index, SIZE), so it is structurally blind to a REORDER and only a committed record of the old order can see that. The gate: a saved rack comes back, scratch does NOT travel, a mangled or truncated blob is refused, and an older shorter blob prefix-restores (the update cliff) |
| a cart's **host-MIDI note mapping** (what a note from the host MEANS) | **`bash tools/midi-note-check/run.sh`** | 27 assertions, headless, no DAW: a note names a SOUND or a PITCH per machine. Three negative controls, each stopping a different way of passing for the wrong reason — two untouched renders byte-IDENTICAL, holding the panel's OWN root byte-identical (the override is EXACT, not merely present), and a note outside the kit changing NOTHING. Direction is measured on the 303 **stem**, never the mix: a whole-rack centroid sits at the hats and drifts the wrong way |
| **`runtime/sync.h`** or the external-clock seam | **`bash tools/sync-spike/run.sh`** | the whole arc over REAL CoreMIDI (arrive → START → run → STOP → hand back), which the synthetic `--midi-clock` path cannot cover because a deterministic run ignores real MIDI by design. Needs the IAC bus online, macOS only |

## Cart logic, registration, docs

| You changed… | Run |
|---|---|
| game logic in a cart with a `spec()` | **`spec.js [cart] --quiet`** (the gameplay-logic gate) |
| **lockstep netplay** (`runtime/net.h`, the `inp_*`/`btn` seam, `tools/net-relay.js`, anything that could desync) | **`net-check.js --quiet`** — the one-command gate, three legs: **echo** (pong vs the loopback fake peer: P2 must mirror P1 exactly — the remote-input-injection path with no sockets), **netdemo** (a real host+joiner pair over UDP loopback, different per-side scripts, per-frame trace diff → `LOCKSTEP OK`/`DESYNC`), **relay** (two simulated carts through a real `net-relay.js` speaking the web wire protocol: ROLE→HELLO→WELCOME{seed}→INPUT→BYE). Exits nonzero on any failure. The netdemo leg alone: `play.js pong netdemo --headless --frames 600 --host-script tools/clips/pong/01-netcheck-host.script --join-script tools/clips/pong/02-netcheck-join.script` |
| a squishy brush or a rim/fill feature (does every brush still apply the features it should?) | **`squishy-features.js`** (renders the `SQUISHY_MATRIX` grid + pixel-diffs each brush×feature cell vs baseline against a declared support matrix — catches a feature that silently no-ops for one brush) |
| `index.json` (tags / registration), or **any cart `.c`** (the promoted source hazards: `watch()` non-string format arg, a local shadowing `map`/`line`/`spr`/… ) | **`lint-carts.js`** |
| anything under `docs/` | **`lint-docs.js`** (links resolve / §-refs / tool-index) |
| reorganized docs, want gaps not breaks | **`lint-xrefs.js [topic]`** (docs that should cross-link but don't) |
| **shipped a new effect/capability** (and a doc somewhere swears we can't do it) | **`lint-capability-claims.js`** — the INVERSE of stale-doc-check: prose denying a capability that now EXISTS ("missing (no reverb engine)", "a future effect once the sidechain lands"). Advisory; fix the claim or add a paragraph-scope "✓ SHIPPED" acknowledgement |
| added an **`FX_*` insert kind** | **`lint-fxicons.js --strict`** — the kind must have a glyph + colours + name in `runtime/fxicons.h`, or it silently draws the REVERB pedal labelled "FX" |
| a tool a doc describes (is the prose now stale?) | **`stale-doc-check.js [scope]`** (broken-refs tier = real issues; mtime tiers = nudges; `--driftable` = the declared snapshots) |
| **not sure which meta-check to run / is the self-doc layer healthy?** | **`repo-doctor.js`** — the ONE health strip over all of the above + the generated-page `--check` gates, counts only (embedded in bare `orient`) |
| a cart's source/sprites (is it baked/published?) | **`cart-status.js --quiet`** |
| **laying out / sizing a cart's UI** (fit a phone, multiple screens, touch-target size) | reach for **`runtime/lay.h`** (immediate-mode layout vocabulary — split/at/cell/grid/wrap/fluid; see [`../design/device-adaptive-layout.md`](../design/device-adaptive-layout.md)); then **`mobile-lint.js`** flags **tiny touch-targets** and names the remedy — **`ui_loupe(1)`** (the `ui.h` fat-finger magnifier, [`../design/loupe-notes.md`](../design/loupe-notes.md)) or reflow |
| effects/UI placement, want a "can a phone play this" report | **`mobile-lint.js`**, **`ui-audit.js`** |

## Store / listing (the ASO capture tools' deterministic halves)

| You changed… | Run |
|---|---|
| App Store metadata fields (title/subtitle/keywords) | **`aso-lint.js`** (char limits, wasted stopwords/comma-spaces, cross-field repeats; offline) |
| `press.md` or the listing prose (vs the committed brief) | **`aso-coverage.js`** (phrase/word coverage vs `seo-brief.md` + a STUFFING warning — fails only for reading robotic) |
| `apps/<name>/app.json` store fields, before pushing | **`asc-push.js --check`** (offline gate) / **`--dry-run`** (GETs live + diffs) |
| **anything an app BUILD reads** — a manifest, a cart rename, an engine constant like `SOUND_CART_CTX`, an icon path | **`build-app.js --check`** — stages every `apps/*/app.json` through the real front half. ⚠ It stops before the first `clang`, so it does NOT cover per-cart compiles (that is `build-all.js`); it covers what build-all cannot see. Added 2026-08-16 after the store path sat **fatally broken for weeks behind a fully green board**, because nothing here ran `build-app.js` except a human about to ship |
| **the AU's identity** (name / 4-char codes) or which cart ships as the plug-in | **`DERIVE_ONLY=1 APP=<name> ios/testflight.sh`** — runs the whole store-spec derivation and stops before xcodegen (~1s, no Xcode), printing the `AudioComponents` block it produced. ⚠ The subtype/manufacturer are **FOREVER** (a DAW stores the triple to re-instantiate a saved plug-in): [`../design/auv3-plugin-types.md`](../design/auv3-plugin-types.md) §6 |
| an **app icon** (or you're drawing a new one) | **`icon-mask.js check <icon.png>`** — per corner, does the iOS squircle mask cut flat background (safe) or real detail? `--quiet` = release gate. Then **`preview`** (how it reads at all six real display sizes, light + dark) and **`device`** (what a booted iOS 26 simulator actually draws). Design + the measured mask: [`../design/app-icon-mask.md`](../design/app-icon-mask.md) |

## Self-test the checker (the asymmetry that cost a whole day)

**A check that judges the repo needs a known-answer fixture just as much as one that judges a
waveform.** On 2026-07-30 three checks were trusted and wrong within hours of each other:

| check | how it was wrong |
|---|---|
| `docs/STATUS.md` itself | 20 of 53 "open" items were shipped; both linters carved it out, so nothing ever measured it |
| `status-check`'s first `done-in-open` heuristic | 2 false positives — items that *mentioned* a shipped thing while their own work was open |
| `stale-doc-check`'s BROKEN REFERENCES tier | 47 findings, **0** true positives — every one a proposal or another repo's path |

Each was *declared* authoritative and none was ever measured against a hand count.

The repo had already solved this — **for audio only.** `stereo-check --check`, `inharm-spec --check`,
`sndi-check --check` and `disp-model --check` all assert themselves against synthetic known-answer
signals first, and CLAUDE.md says why in as many words: *"a broken analyser and a mono file print the
same thing."* That discipline never crossed over: at the time of writing, **0 of 10 meta-linters had a
self-test** while 4 of 7 audio analysers did.

The reason for the gap is worth knowing, because it will recur. When a tool measures the physical
world you can *imagine* the analyser being wrong. When it measures the repo, "does this link resolve"
feels self-evidently correct — and it is, right up until the check starts encoding a **judgement**
("is this item done?", "is this reference a claim or a proposal?"). At that moment it is exactly as
fallible as an FFT, and nobody has built the fixture.

**So: if your check makes a judgement, give it a `--selfcheck`.** 37 tools now carry one and
**32 are gated in `repo-doctor` as a `selftest:` row** — always take the live figure from
`node tools/gate-controls.js --list` or a `repo-doctor` run, never from a number in this prose.
The fifteen that started it, and their counts at the time (several have grown since)
— every one gated in `repo-doctor` as a `selftest:` row:

| tool | fixture | pins |
|---|---|---|
| `status-check.js --selfcheck` | `tools/fixtures/status-check/ledger.md` | 13 — every finding kind, plus the two shapes that made v1 cry wolf |
| `lint-docs.js --selfcheck` | `tools/fixtures/lint-docs/docs/` | 6 — broken links, and the **hard-vs-soft §-ref split** (a parent-resolved ref is a note, not an error) |
| `lint-xrefs.js --selfcheck` | `tools/fixtures/lint-xrefs/docs/` | 5 — both tiers, plus the HUB and fenced-code exempt classes |
| `stale-doc-check.js --selfcheck` | `tools/fixtures/stale-doc-check/docs/` | 7 — the four verdicts BROKEN REFERENCES rests on (gone / never-existed / foreign / present) |
| `handoff.js --selfcheck` | `tools/fixtures/handoff/HANDOFF.md` | 11 — all five lane judgements, plus **all three** of the false positives that tool shipped with |
| `lint-capability-claims.js --selfcheck` | `tools/fixtures/lint-capability-claims/` | 15 — the discriminators that take it from prose-guessing to precise |
| `lint-fxicons.js --selfcheck` | `tools/fixtures/lint-fxicons/` | 7 — per-dispatcher registration, plus the derived-fallback exempt class |
| `lint-aux-params.js --selfcheck` | `tools/fixtures/lint-aux-params/` | 14 — every finding kind, and the three checks that pass **vacuously** on zero regex matches |
| `lint-carts.js --selfcheck` | `tools/fixtures/lint-carts/` | 48 — all three source hazards **and each one's exempt class**, plus a 26-case `de:meta` table |
| `lint-fx-frame.js --selfcheck` | `tools/fixtures/lint-fx-frame/` | 30 — the footgun shapes, all six gated constructs, the ride-live exclusion list, the waiver **and its no-leak regression**, and line numbers surviving comment stripping |
| `ui-audit.js --selfcheck` | `tools/fixtures/ui-audit/` | 31 — every finding kind and **every exempt class** (clip / occlusion / identical strings / the ≤3px widget threshold / slivers / transients), plus the waiver subsystem, which has no other coverage anywhere |
| `cart-dupes.js --selfcheck` | `tools/fixtures/cart-dupes/` | 20 — the normalization trick in **both** directions (a renamed copy must match; different engine calls must not), the drift band vs identical copies, and `HOOK_CUTOFF` |
| `mobile-lint.js --selfcheck` | `tools/fixtures/mobile-lint/` | 27 — all five verdicts, the **precedence chain** (best input path wins, not worst), the three source transforms incl. the `studio.h` skip, and every warning class |
| `cart-analyze.js --selfcheck` | `tools/fixtures/cart-analyze/` | 23 — one cart per verdict branch, the **chain order** (`simple` beats `reactive`), the two anti-inflation counting rules, and the score formula recomputed from the metrics |
| `squishy-features.js --selfcheck` | `tools/fixtures/squishy-features/` | 21 — synthetic grid PNGs: all four verdicts (and `MISS`/`UNEXP` in isolation), the `APPLIED_MIN` boundary, cell origin + inset + alpha, and **all five PNG scanline filters** decoding identically |

Fixtures are fed in by **path override, not by restructuring the tool**: doc scanners take
`DE_DOCS_DIR` (`handoff` takes `DE_HANDOFF_FILE`), source scanners take one env var per input
(`DE_AUX_SOUND_H`, `DE_FX_ICONS_H`, …), while `ROOT` stays the real repo — so the fixture is
adjudicated against real `srcExists`, real link targets and real git history, exactly as in
production. Where the judgement is already a **pure function** (`lint-carts`' hazard scanner and
`de:meta` validator), `--selfcheck` just calls it directly and needs no fixture repo at all;
extracting that function was the whole cost of fixturing it.

**Name the fixture files `.c.txt` / `.h.txt` / `.js.txt`, never `.c` / `.h` / `.js`.** A fixture is
never compiled, and a real header here gets indexed by clangd, which then reports phantom
"undeclared FX_ALPHA" diagnostics at you in unrelated files. A real `.c` also risks being picked
up by anything globbing for cart sources.

**Fixture more than one case when the findings are mutually exclusive.** `lint-aux-params` needs
three (`broken/` `clean/` `stale/`): a channel whose `eng_p[]` declarations *disagree* cannot
simultaneously be the one demonstrating two that agree.

**A fixture whose expectations depend on *today* must template its dates.** `handoff`'s lane-staleness
judgement is relative to now, so a hard-coded "fresh" date would quietly become a stale one and the
expectation would invert. The fixture writes `__TODAY__` / `__ANCIENT__` and `--selfcheck` substitutes
real dates into a temp copy — the fixture cannot rot.

The pattern:

- a **tiny synthetic input** carrying one instance of every finding kind;
- plus a **regression guard for every false positive you actually hit** — the fixture holds the two
  shapes that fooled v1, each commented with why it must *not* fire;
- assertions on the exact expected set, `--selfcheck` exiting nonzero on any mismatch;
- **gated** in `repo-doctor` (unlike the findings themselves, which are advisory): a red findings row
  is a backlog, but a red self-test means the tool's output cannot be believed at all.

Two rules that fall out of the same day:

1. **Verify by hand before you believe a count.** Every one of the three failures above was found by
   reading the findings, not by tuning the heuristic. Reading 22 items by hand took minutes and
   overturned the tool twice — once for crying wolf, once for missing five real hits.
2. **Never suppress silently.** When a check learns to ignore a class, it must still *count* what it
   ignored and be able to list it (`stale-doc-check --all`). A linter that quietly drops findings
   rots in the opposite direction, and nobody notices because the report looks clean.
3. **Watch every assertion fail once, or it isn't evidence.** Break the heuristic on purpose and
   confirm the expectation goes red. This is not ceremony — **two of the guards written on the day
   this page was added were vacuous on the first attempt.** `lint-xrefs`'s fenced-mention guard
   passed green with fence tracking fully disabled, because the fenced mention named a doc that
   already had an unfenced mention and was deduped away regardless; the fix (a doc named *only*
   inside the fence) then fired for the wrong reason, because the sentence introducing the fence
   named it too. Only the third version actually failed when broken. A green self-test proves
   nothing until you have seen it go red.
4. **A "must NOT be flagged" assertion has to be *able* to fail.** The commonest vacuous shape:
   marking the good lines in a fixture (`OK_fmt`, `OK_waived`) and then asserting
   `!errors.some(e => e.includes("OK_fmt"))` — which is true whether or not that line was flagged,
   because the hazard message quotes its own advice, not the offending call. Six of `lint-carts`'
   negative assertions were vacuous this way on the first pass. **Resolve the finding back to the
   line it points at** and assert on *that* text; then a wrongly-flagged `OK_` line fails, and as a
   bonus you get a line-number guard for free.
5. **When you break the heuristic on purpose, assert the patch applied.** A `sed`/`python` mutation
   whose pattern doesn't match leaves the tool untouched, the self-test passes, and it reads exactly
   like "the fixture has no teeth here." That happened twice while writing rule 4's fixtures — once
   from heredoc escaping eating a backslash. `assert old in src` before writing, every time. It is
   the same failure `ab-render.js` exits 2 over: two variants that render identically mean the flag
   never reached the code, so the numbers are meaningless.
6. **A fixture case can pass for the wrong reason — check by deleting the exemption.**
   `lint-carts`' `pool-ok.c.txt` was meant to prove the `pointer.h` exemption works, but its first
   draft used a bare local (`int id = touch_id(i); if (id < 0)`) that the hazard regex never matched
   in the first place. Removing the exemption outright still scored a clean 48/48. If an assertion
   is about an *exempt class*, the fixture must contain something that would otherwise be **caught**
   — and the mutation that deletes the exemption is how you prove it does.
   *Three* separate cases hit this in one session: `pool-ok.c.txt` (a bare local the regex never
   matched), the `AAA`/`BBB` overlap pair (already in sorted order), and `cart-dupes`' sentinel
   assertion (its fixture header contained no `N` or `V`, so deleting the exclusion was a no-op).
   It is the single most common way a green fixture means nothing.
7. **A check reporting zero findings while wired into no gate is the most suspicious state there
   is** — it is indistinguishable from one that has gone blind, and nothing will ever tell you
   which. `lint-fx-frame` sat at *"✓ 0 findings across 573 carts"*, in no gate, behind a
   hand-rolled C parser with six exempt classes. It turned out to be working; the fixture also
   turned up a **doc/behaviour mismatch** its header had carried all along (a `for` body was
   documented as exempt and never was). Both halves are the fix: the fixture, so a future
   regression is loud, and the gate, so someone runs it at all.
8. **Probe each construct before you write the assertion.** Writing the `lint-fx-frame` fixture
   from its header comment would have baked the wrong expectation for `for` straight into the
   self-test — freezing the bug as the contract. Feed the tool one tiny input per construct first
   and record what it *actually* does; then decide which behaviour is right, and fix the code or
   the prose accordingly. Here the behaviour was right and the prose was wrong.

## The OTHER way a green check lies: it was never measuring the thing

Everything above is about the checker being **wrong** — a heuristic with false positives, an
analyser that misreads — and its remedy is a known-answer fixture. This section is about the
opposite and sneakier case: the checker is **completely correct and entirely irrelevant**. Nothing
is miscomputed, so no fixture can help; the assertion is simply true whether or not the property it
names holds.

Three of these landed in one session (2026-08-13, the MIDI-out work), each for a *different*
reason, and every one was caught by a **control** rather than by the assertion itself:

| what was asserted | why it was green while broken |
|---|---|
| "every note-on is matched by a note-off" | **Wrong axis.** The defect was that drum notes were ZERO-LENGTH (on and off in the same millisecond, which records wrong in a DAW). A zero-length note is perfectly balanced, so the pair count was not merely blind to the bug — it *rewarded* it. Found by the maker's ear in GarageBand, not by the gate. |
| `sync_automated` (ignore a real clock in automated runs) | **The code never ran.** Its assignment sat inside `#ifdef DE_SPEC`, which only `spec.js` defines — so in every `play.js` run it was meant to protect, the flag stayed 0. Every sync gate passed, because `--midi-clock` runs take an earlier branch and those are the runs the gates exercise. |
| "quitting never leaves a note droning" | **True by luck.** `MIDIReceived` is asynchronous, and the shutdown flush disposed the endpoint on the next line, dropping the note-offs in flight. It passed once, then failed with exactly the two notes still held. A pass that depends on a race is not a pass. |
| `param-check` green on the host-PARAMETER seam (2026-08-16) | **It measured a different BUILD's code path.** `struct DeInstance` and every context resolver live inside `studio.c`'s `#ifdef DE_NO_RAYLIB`, so the *native* build has no instances at all: a headless run exercises the DEFAULT shared table through the thread-local, while a host exercises the PER-INSTANCE one. Every assertion was true and none of them was about the path a DAW takes. It surfaced only by accident, when a seam function grew a body `-O2` could not fold and the native build stopped **linking** on code that runs fine under the plug-in. |

**The one question that catches the first three:** *what would I have to break to make this go red?*
If you cannot name a specific edit, the assertion is decoration.

**And a second one, for the fourth:** *does this gate run the same BUILD the user runs?* A repo with
a `#ifdef` at the platform seam has two programs in one tree, and a headless harness reaches for the
convenient one. Where a feature only exists on the other side of that fork, the honest answer is a
gate that crosses it — which is what `au-transport-check --params` is to `param-check`, and why the
latter's header says ENGINE HALF ONLY in capitals. Run it on the three above: break the gate
length → the pair counter stays green. Delete `sync_automated` → every sync gate stays green. No-op
the shutdown flush → green, most of the time.

This is rule 3 above ("watch every assertion fail once"), but the fixture framing hides it — these
are **end-to-end gates over live behaviour**, not linters with a fixture directory, so nobody reaches
for a `--selfcheck` and the discipline quietly does not apply. The cheap form for that kind of gate
is a **negative control in the gate itself**: a second run with the feature disabled, asserted to
produce the opposite result. `input-ring-check` and `present-race-check` have carried one for
exactly this reason (`-bypass` rebuilds without the safety and **must FAIL**); `midi-check` now runs
three (no `--midi-out` must publish nothing · CC 74 read on an unused channel must be `-1` · the
receiving cart with autoplay off must render silence).

**Where a control earns its cost** — it is not free, and blanket application is the wrong lesson.
`midi-check`'s cart-to-cart control doubles that phase's runtime. Spend it where **PASS is the
steady state and failure is silent**: timing, threading, guards that suppress behaviour, and
anything whose "correct" output looks identical to its broken one. A link checker does not need one;
its failures are loud and self-describing.

**Two shapes worth recognising before you write the assertion:**

- **A control must vary ONLY the mechanism.** The first cart-to-cart control ran the receiver while
  the previous sender was still alive — `play.js` spawns the cart as a child, so killing the node
  parent orphans it — and measured 0.407 where silence was required. A control that does not
  actually remove the thing proves nothing, and it fails *safe-looking*.
- **Watch for the confounder that makes noise for you.** The same receiver (`epiano`) autoplays a
  triad every two beats by default. A naive run makes plenty of sound while proving nothing about
  MIDI; turning autoplay off is what produced a `-inf` baseline against which `0.39` means one
  thing only.

Coverage is tracked: **`node tools/gate-controls.js`** lists gates that have neither a self-test nor
a negative control (advisory row in `repo-doctor`). It cannot tell you a gate is *good* — only that
nothing in it has ever been shown to fail.

### Giving a RENDER-OR-AUDIO gate a known-answer fixture (the recipe, and twelve worked examples)

> Started with the audio block and kept going: the audio gates, then `spec.js` (game logic), then
> the three pixel gates, then `psola-check` and `web-audio-check`. **Twelve gates, and ten of them
> were broken.** The two failure shapes at the bottom of this section are the reusable part — check
> any gate you write for both, by name.

The audio gates were the largest block on `gate-controls`' list, and for a bad reason: the
discipline above *reads* as being for linters, so "it's audio, you need a render" became an excuse.
It is not true. **Every audio gate splits into a pure ANALYSER and the thing that produced the
sound**, and the analyser can be fed a signal you synthesise, whose answer you know from
arithmetic, with no cart, no engine and no WAV on disk that anyone had to bless.

That matters most here because of *which way* these gates fail. A pitch detector that quietly
returns the pitch it was asked about, or a splice detector that stops finding discontinuities,
prints output **identical to a healthy engine**. There is no red. Everything passes forever.

**The recipe:**

1. **Find the pure function.** `measurePitch`/`octaveFold`/`verdict`; `analyze()`. If the tool only
   exposes a whole-file path, synthesise a WAV and feed that — it puts the reader in the path too,
   which is free coverage.
2. **MEASURE BEFORE YOU ASSERT.** This is the step people skip and it is the one that pays. Writing
   down what you *expect* and calling it a known answer just moves your assumption into a file that
   now looks authoritative. Three of the expectations across these two tools were wrong (below).
3. **Assert BOTH directions.** Exact input reads zero AND detuned input reads its detune. A
   one-sided test is passed by a detector hard-wired to agree, which is precisely the blindness
   worth fearing.
4. **Pin the characteristics you found, do not "fix" them.** Every metric has inputs that make it
   shout or go quiet for structural reasons. Recorded as known answers they are documentation;
   left out, the next person "fixes" one and blinds the gate.
5. **Mutate it — including the CONTROL.** Break the analyser three ways and watch which assertions
   go red; an unmutated fixture is just more untested code. Then break the analyser in the specific
   way the control was added to catch, and confirm the control is what goes red. `level-check` below
   is the cautionary case: two plausible controls sat green through the very failure they were
   written for.

**`tune-check --selfcheck`** (20 answers). The fixture was the smaller half: the real find was that
**the SINE control was held to the engine thresholds** (`warn >12¢`), when it is exact by
construction and should sit at 0. A 3-cent analyser bias was injected to prove it: the sweep
printed *"no new tuning drift"* and exited 0, and it *lowered* the waived-residual count from 3 to 1
because the bias nudged PIPE and BRASS toward nominal — a broken analyser would have read as
"we fixed some tuning". The control has its own 1-cent bound now and says *the measurement is off,
not the engine*, so nobody hunts a DSP bug that is not there.

**`click-check --selfcheck`** (20 answers). Catches the wavetable-swap-at-continuous-phase case the
tool was born for, localised to within a millisecond. Pointing it at a real `acidcandy` render is
what taught the most: **44 events, worst 1834x, every one a kick drum.** An onset after a quiet
passage divides by an almost-silent baseline, so on sparse percussive material the gate is close to
useless as pass/fail and should be used to compare a before and after of the same take. Now in its
header, and pinned.

**The three expectations that were wrong, all caught by step 2:**

| I expected | what it actually does | why |
|---|---|---|
| a tone an octave HIGH reports `octaves: +1` | reports the played octave | a signal periodic at *f* is also periodic at *f/2*, *f/3* … but **never at 2f**. Measured: an 880 Hz sine autocorrelates **0.9996** at the 440 Hz lag; a 110 Hz sine at the 220 Hz lag is **−1.0**. Octave-down is unambiguous, octave-up is invisible to the method, and the tie-break resolves toward the played octave deliberately — "fixing" it breaks the sub-octave protection |
| a naked saw stays silent (its header says a flyback is not a click) | trips at ~15x | that claim is about **real engine output**, which is band-limited and enveloped. An un-bandlimited saw genuinely *is* a train of discontinuities |
| an onset out of silence scores huge | out of EXACT digital silence it is **skipped** | the `rms > 0` divide-by-zero guard. Out of a quiet *tail* it explodes instead. The two silences behave oppositely |

**`dc-check --selfcheck`** (16 answers). The third, and the one where the fixture work *changed the
measurement*. Its `SINE (control)` was reading **−67 dBFS while GUITAR read −103** — the control
dirtier than the engines — and nearly every engine's worst note was A2, the lowest. Both are the
signature of one thing: a **rectangular window leaves a residual bounded by A/(π·cycles)**, worst at
a half-integer cycle count and scaling as 1/f. Measured: a pure sine over 38.5 cycles has a plain
mean of **+0.004134**, which is essentially `WARN_DC` (0.004) — *the threshold had been calibrated
to clear an artifact*. Worse, it adds to real DC rather than merely limiting sensitivity: a true
0.010 offset at 110 Hz measured **+0.014134**, a 41% overestimate. A **Hann-weighted mean** removes
it (needs no knowledge of the note's frequency, unlike trimming to whole cycles): the same pure sine
reads −0.0000028, and a true 0.010 offset reads +0.009997. Every shipping engine turned out to be
clean all along — SINE −67 → −inf, PIPE −62 → −108, ORGAN −66 → −114. The engine thresholds were
deliberately **left alone**: with a 55x margin they could be tightened ~4x, but that changes what
the gate accepts, which is a judgement call and not a mechanical follow-on.

**`level-check --selfcheck`** (44 answers). Expected to be the easy one, and the arithmetic half was.
The rest taught two things worth carrying to the next gate.

*A control can be blind to the exact class you built it for.* Level has no absolute truth, so the
controls were taken from **shape**: SINE's crest is 3.0103 dB by arithmetic, and SINE's peak is
identical at all four octaves. Both looked like they must catch a window reading the wrong samples.
Step 5 said otherwise: **a 2% samples-per-frame error left SINE reading crest 3.0 dB, peak −14.0
dBFS and zero drift while it wrecked eleven other engines** — SINE is the *first* entry in the
sweep, where the accumulated offset is a couple of frames, and a stationary sine is the least
window-sensitive signal there is. The fix was three more controls taken from the sweep's **geometry**
rather than its sound (a frame is exactly `sr/60` samples · every note is gated the same number of
frames · the render outlasts the sweep by a period), each verified against a live mutation. The
lesson generalises: *test the control against the failure it is supposed to cover*, or it is
decoration. Mutation testing is not only for the assertions.

*And the fixture found two real defects, both inherited.* This file's `noteWindows` is a copy of
`tune-check`'s made before that one learned about `tunecheck.c`'s **differential pass** — PIANO
rendered a second time with the stretch off, same `INSTR_*` id, which is why the cart publishes an
`et` field. Ignoring it collapsed both passes onto the baseline key `27:45`, last-write-won, and the
normal pass was silently diffed against the stretch-off one: **a standing phantom drift of +0.5/+0.7
dB peak and 1.2 dB rms on PIANO, 82% of the warn budget spent on a difference between two renders.**
The same copy carried `--frames 3400` with a comment counting 13 sweep entries when there are 14, so
the last note *was never rendered* — and a note that never plays leaves nothing behind to notice.
Blessing the corrected baseline also surfaced un-blessed drift from two committed BOWED body changes
(`cc1bcd43`, `b661748a`), sitting at 0.28 dB under a 1.5 dB threshold for two weeks. Two follow-ons
now in the tool: `--save` **refuses to bless** while a control is red (a fault frozen into the golden
file stops looking like a fault), and duplicate baseline keys are reported rather than silently
overwritten.

**`fx-check --selfcheck`** (38 answers). The cheapest of the four so far, because level-check had
already found the shape: same buffer statistics, same trace windows, same golden baseline, so the
three structural controls ported straight across. Two of its controls are its own, and both cover a
*silent skip* rather than a wrong number. **DRY is this gate's control engine** — every "no-op"
verdict is measured against it — and `dry && …` means a missing DRY makes that check evaporate
without a word; DRY is now required to exist, sound, not clip, and carry no DC (with nothing in the
path, an offset there cannot be an effect). And `FX_NAMES` and `fxcheck.c`'s switch are two
hand-maintained lists that must agree, so **every roster entry must have produced a window**: a gap
does not lose one effect, it mislabels every effect after it.

Two things it did *not* do, both worth copying. The DC statistic here is a plain mean, and the
reflex after dc-check was to move it to a Hann window — **measured instead: plain-vs-Hann differs by
at most 0.009 across all 20 effects against a limit of 0.03**, and the two largest readings stay
large under both, so they are real drive-stage asymmetry and the change would have been churn.
The residual is real but shrinks as 1/cycles, and an fx window is 40k samples of a dense chord
rather than dc-check's 38.5 cycles of one sine. It is pinned in the fixture with its arithmetic
(`A/(π·cycles)` = 0.000722 for the synthetic case, measured 0.000722) — *the first draft of that
assertion demanded < 1e-6 and went red, which is how the bound got checked rather than assumed*.
The frame count was hardcoded at `19 * 84` under a comment counting 19 tests when the roster held
20; slack absorbed it, so unlike level-check nothing had broken yet. Derived from the roster now,
and the baseline needed no re-blessing.

**`soak-check --selfcheck`** (24 answers). The one that found the worst green in the set, and the
cleanest illustration of why this section exists. Almost none of its surface is a measurement — it
is a *judgement over a set of cycles* — and *every assertion it makes is vacuously true of the empty
set*. Measured: a run whose rows came back empty printed **"✓ stable over the long run — no drift,
leak, accumulation, or blowup"** and exited 0. So did a run that measured 3 cycles of 24.

For a gate whose entire subject is duration, "no soak happened" and "the soak was clean" were the
same output, and **no threshold could ever have caught it** — that is the part worth internalising.
A shorter run drifts less, decays just as well and accumulates less, so the evidence disappearing
makes every check *easier*. The fixture pins that directly: 3 cycles score no worse than 24 on all
five assertions. Only a control that asks *did the run happen* can see it, so the cart's geometry
(`STRESS`/`IDLE`/`NCYC`) is now mirrored as constants used for both the frame budget and the check,
and a cycle count that is not exactly 24 is a red with its own sentence.

It also carried a **statistic that did not measure what its message said**. Accumulation was
`last - min` over the idle-tail floors: a claim about the whole run resting on one sample of it, so
a floor that ramped for twenty cycles and dipped on the twenty-fourth read as 1.5 dB and passed a
4 dB limit. Now the mean of the final three cycles, same threshold, with the old form's blind spot
pinned as a regression guard. Distinguish this from a tolerance choice — the threshold was fine, the
statistic was wrong.

**`spec.js --selfcheck`** (23 answers). Not an audio gate — the *gameplay* one — but it turned out to
carry soak-check's hole in a worse place, so it is written up here with its siblings. Every verdict
it makes is "no assertion failed", which is trivially true of a run that made no assertions.
Measured with a throwaway cart whose `spec()` body was empty: it compiled, ran, and printed a green
**`✓ 0 passed`**, and the process exited 0.

The scale of that is the point. `{"done":1,"pass":0,"fail":0}` is exactly what a **weak-stubbed**
`spec()` prints, so if the stub ever won over a cart's own definition — a rename, a linker change,
`-DDE_SPEC` not reaching the build — all 32 carts would have reported `✓ 0 passed` and CI would have
stayed green with the entire gameplay gate doing nothing at all. Two more shapes of the same thing
were also exit-0: an empty sweep ("no carts with a spec() found") and naming a cart that has no
`spec()`. All three are now refusals with their own sentence.

**And its second control found a live defect on its first full sweep.** `expect()` interpolated the
human-written message into JSON *raw*, so `tenement`'s `... never cut mid-word ("bursting")` emitted
a line `JSON.parse` rejected, which the runner's `catch { continue }` dropped in silence — 247
assertion lines parsed against 249 reported. Both happened to pass, so the only symptom was a report
two lines short. The dangerous case is the other one: **a FAILING assertion whose message contains a
quote disappears from the output while still counting toward `fail`**, so the gate goes red and says
nothing about what broke. Fixed at the root (`spec_esc()` in studio.c, inside `#ifdef DE_SPEC`), and
the control is a genuine red-then-green rather than a synthetic one.

Worth copying: a control that cross-checks *two independently produced numbers* — here the parsed
line count against the count the binary reported — costs almost nothing and catches a class no
threshold can, because both numbers stay individually plausible.

#### The three PIXEL gates, taken together

`mirror-diff`, `road-check` and `canvas-diff` were done as one batch, and they are worth reading as
one entry because **all three had the identical defect in the identical place**. Each one's verdict
is "no bad pixel/frame was found", which is trivially true when nothing was examined:

| gate | the measured vacuous pass |
|---|---|
| `mirror-diff` | `--band 500,600` on a 200-tall frame compared **0 of 0 pixel-pairs**, exit 0 |
| `road-check` | the wrong palette made **all 36480 play-area pixels unrecognised** — `unknown` is info-only without `--strict` — verdict PASS |
| `canvas-diff` | both runs dumping nothing printed a green **"PASS — canvas matches GPU within budget"** under an empty frame table |

Every one is a plausible accident rather than a contrived one: a typo'd band, a changed palette or a
cart that did not draw its road, a dump that renamed its frames. And in all three the mistake makes
the gate *easier* rather than louder, so nothing downstream can notice.

The controls differ in shape, and the choice is the interesting part. `mirror-diff` and
`canvas-diff` can just count what they compared. `road-check` cannot — a frame with pixels in it is
not evidence that any *invariant could have fired* — so its control asserts one necessary condition
per invariant, each naming the check it silently disables ("no kerb anywhere, so *stray* and
*floating* are checks on a class that is absent"). No invented percentages: all of them are `> 0`.

Each also had a defect of its own, and two are worth copying elsewhere:

- **`mirror-diff` could not hold the number it was gating.** streetlab's accepted symmetry floor is
  nonzero, but `--quiet` can only assert zero — so the roadkit extraction's *"mirror-diff 68=68"*
  gate was **a person reading two terminal outputs**, and nothing ran it again afterwards. `--expect
  <n>` makes it runnable, and a count *below* the expectation fails too: an accepted floor is a
  value you re-bless on purpose, not a ceiling to come in under. Separately, its PNG decoder
  answered on images it cannot read — a maximally asymmetric **palette** PNG reported *2 mismatches
  of 16 instead of 16 of 16*, because palette data is 1 byte per pixel read at a 4× stride. Refused
  now, along with 16-bit, interlaced and truncated.
- **`canvas-diff` passed unreadable frames.** `ae()` returns `NaN` when magick cannot read a frame,
  and every comparison with `NaN` is false, so `d > maxPx` said *within budget*. Its own `--golden`
  mode already had this right with `d === 0` — **the two modes disagreed on the same condition**,
  which is a smell worth looking for in any tool with more than one comparison path.

**A trap this batch fell into, having documented it two commits earlier.** Mutating canvas-diff's
real per-frame predicate left the whole suite green, because the fixture had re-implemented
`d > maxPx` locally instead of calling it — a fixture pinning *a copy of* the logic. Extracted as one
shared `withinBudget()` used by both. The same round caught the mirror image in `mirror-diff`:
loosening `--expect` from `===` to `<=` passed all 27 assertions, because none of them tested a count
below the expectation. **Step 5 is not optional, and it earns its keep on the assertions you were
most confident about.**

**`psola-check --selfcheck`** (53 answers). The artifact oracle for the PSOLA pitch engine, and the
sharpest case in the set for *why* a fixture: its three detectors are **deliberately blind to each
other's defects** — that is the tool's whole design — so one of them dying is invisible **by
construction**. The other two carry on printing plausible numbers and the verdict still says "no
artifact regression". Measured on a healthy render, `SPLICE` reads exactly `0 / 0.0000` on **all four
takes**: a live detector and a deleted one print the identical four rows.

Four defects, one per tier of the thing:

- **A missing baseline dropped half the gate, silently.** Both relative checks sat behind `if (b)`,
  and the "no baseline yet" hint was inside `if (!quiet)` — so a renamed or absent golden file left
  `--quiet` exiting **0** having run two of four checks and printed nothing at all. Refused now (exit
  2), under `--quiet` too.
- **Vacuity, in the shape soak-check taught.** An unvoiced window is skipped, so a take that went
  silent produces no errors and scores a **perfect** `{p95: 0, worst: 0}` — strictly better than a
  clean one — and zero splices besides. Only the f0 detector noticed, and only for *total* silence.
  Both detectors now report `voiced/windows`, and the verdict has a liveness tier that reads them.
- **An unchecked reference, in the prose rather than the code.** The header stated RAW was "the
  control in every comparison" and that "no worse than RAW" was the bar. Nothing implemented it, and
  the numbers refute it: the processed takes measure **1.50x–1.92x RAW** on a good render, so the
  documented bar would have been red every run since the tool was written. Corrected to what is
  actually enforced.
- **`--save` would bless a fault.** It wrote the golden file before any verdict ran, so a
  period-doubled take could be frozen in as the reference — after which every later run is measured
  against the defect. It now refuses while an absolute check is red (`level-check`'s follow-on,
  applied one gate over).

**And both of the traps this section warns about fired, on the gate the lane had flagged as the one
where they would.** The prediction was that PSOLA needs synthetic signals carrying a *specific*
artifact, and that mis-synthesising one pins your fixture rather than the detector — which is exactly
what happened. The first equal-amplitude phase break was a **time reversal**, and all three detectors
read 0 on it. Correctly: a reversed sine is still a sine at the same frequency, so the probe had no
artifact in it at all, and "SPLICE is blind to this" was one step from being written down as a known
answer. Only running `--measure` first caught it. (The real probe is a polarity flip on an exact zero
crossing: value continuous, every later period negated. SPLICE 0, PERIODICITY 2.835.)

Then, mutating the **control**: the two new liveness checks were covered by a single assertion
matching `/NOTHING WAS MEASURED/`, and silence trips both — so **either check alone satisfied it, and
deleting the other stayed green**. Same shape as `level-check`'s SINE controls, one section up, found
only because step 5 was run on the assertions that had just been written *to fix a vacuity hole*. The
fix is a discriminating assertion per check. Two more useful mutants: the fixture's own `verdict()`
tolerances (extracted as one function so the fixture cannot pin a copy — canvas-diff's lesson) and
the debounce, which turned out to make `count` **not a density measure**: `last` advances on every
trip, so a continuous artifact collapses to one event however long it runs (a 0.8 s staircase counts
1, same as a single pop). Pinned rather than fixed — real grain splices sit above the 2 ms gate,
which is how the down-shift bug counted 177 — because widening it changes what the gate *accepts*.

**`web-audio-check --selfcheck`** (31 answers) **+ `--bypass`**. The wasm-vs-native parity gate, and
the one that needed **two** controls, because neither can cover the other's half. It compares two
builds of the same engine — so its subject is *agreement*, and **two nothings agree perfectly**. All
four of these returned `BIT-IDENTICAL, ok` before the fix, and the second is the one to remember:

| what was compared | why it read as perfect parity |
|---|---|
| two **empty** renders | `n = 0`, so every statistic is `NaN` — but `maxLsb === 0` is checked first and short-circuits |
| an **empty wasm render** vs a good native one | `Math.min(a.length, b.length)` makes `n = 0`. **A wasm build that produced nothing was the strongest possible pass.** |
| two **silent** renders | if the host stopped making audio, all 16 engines report bit-identical at once |
| a **truncated** render vs a full one | the extra samples are simply never looked at |

The fix is a tier *below* the existing ones (`classify()` now starts by asking whether anything was
compared, whether the two renders are the same length, and whether the reference has signal at all)
with the floor at **-80 dBFS** — measured, against the quietest real engine, GUITAR at **-42.7 dBFS**.

**`--bypass` is the other half, and it is what a fixture structurally cannot give you here.**
`--selfcheck` judges the analyser on synthesised signals and never builds anything, so it can say the
arithmetic is right and *nothing* about whether the comparison reaches the DSP. So: rebuild the wasm
side with `-ffast-math` — which re-enables exactly the contraction the `FP_CONTRACT` pragma turns off
— and require the gate to go red. Measured: **16 of 16 engines diverge**, BOWED worst at 2.8 dB below
signal on 96% of samples. A green `--bypass` would mean every clean run this gate has ever printed
means nothing. It needs the toolchain, which is why the repo-doctor row is `--selfcheck` and this one
is run by hand.

Two more worth copying. The **fixture's own arithmetic was wrong** and the tool was right — a 1-LSB
offset sits `-20·log10(A/√2)` = **-81.28 dB** below signal, not the -78.28 the first draft asserted
from a mangled expression. And **mutation found two assertions that could not fail for the reason
they named**: the silence probes were *exact digital zero*, so `db()` returns `-Infinity`, which is
below any finite floor — `SILENCE_FLOOR = -400` passed every one of them. Quiet-but-nonzero probes
computed *from* the constant fixed it. Same for the reader's bounds clamp, whose probe declared zero
bytes and so agreed with the unguarded version; the case it protects is a chunk claiming **more**
than the file holds.

It also had **canvas-diff's two-paths-one-condition defect**: `--quiet` failed on anything not `ok`
(via `report()`), while `--json --quiet` counted only severity `bad` — so a 1-LSB regression, which
is `warn`, exited 1 in one mode and 0 in the other, against a header that says the bar is
bit-identical. Under `--bypass`, 7 of the 16 diverged engines land in `warn`, so that mode would have
passed a build with real drift in half its engines. One shared `isOff()` now, asserted.

#### The pattern across all twelve

Only `tune-check` and `click-check` turned out to be sound as they stood. The other ten were each
broken in a way their own output could not show: `dc-check` measured its window rather than the
engine, `level-check` diffed PIANO against a different PIANO and silently dropped a note, `fx-check`
never checked the reference everything else was compared against, `soak-check` could not tell a long
run from no run, `spec.js` counted an empty spec as a pass, the three pixel gates each called an
empty comparison a match, `psola-check` scored a silent take as perfect, and `web-audio-check` called
an empty render bit-identical to a good one. **None of these was found by reading the code** — each
came out of writing down what the tool *should* answer for an input whose answer was already known.

Two failure shapes account for almost all of it, and both are worth checking for by name in any gate
you write:

- **Vacuity** — the verdict is a statement about a set, and the empty set satisfies it. `soak-check`,
  `spec.js`, all three pixel gates, `psola-check`, `web-audio-check`. The tell is a verdict phrased as
  *"no X was found"* with nothing asserting that X *could* have been found. No threshold can ever
  catch it, because the counts are honestly zero; only a separate question — *did the measurement
  happen?* — can. **A gate that compares two things is the acute case**: its subject is agreement,
  and two nothings agree perfectly, so the vacuous input is the *strongest possible pass*.
- **An unchecked reference** — everything is measured relative to something that is itself never
  measured. `fx-check`'s DRY, `level-check`'s baseline key, `dc-check`'s window, `mirror-diff`'s PNG
  decoder, `web-audio-check`'s native render, and `psola-check`'s RAW — where the reference was
  unchecked *in the prose*: the header named a bar that no code implemented and the numbers refuted.
  The tell is a value that appears in every comparison and in no assertion.

A third, rarer but nastier: **a statistic that does not measure what its message says** (`soak-check`
accumulation as `last - min`). Distinguish it from a tolerance choice — the threshold was fine.

**Still without one** — always take the list from `node tools/gate-controls.js --list` rather than
from a count written in prose; several agents work this repo at once and this tally moved twice in
one afternoon. *(`psola-check` and `web-audio-check` were named here as the two audio/render holdouts, and both
got their fixture later the same day — `psola-check --selfcheck` is 53 answers, `web-audio-check`
is 31 plus a `--bypass` control. The hard part each faced is worth keeping: `psola-check` needed
synthetic signals carrying a *specific* artifact (a splice, a staircase, a period doubling), where
a mis-synthesis pins the fixture rather than the detector, and `web-audio-check` compares two
platforms, so its control is a deliberate divergence and its fixture needs a toolchain.)* Most of the
rest of the list is `build-*.js --check` staleness gates, where failure is loud and immediate and a
control buys little — **spend one where PASS is the steady state and failure would be silent.**

## The THIRD way a check lies: it goes RED about something you did not change

The two sections above are about a green that means nothing. This one is the mirror, and it is more
expensive, because a red check is *believed* — you go and look for the bug it names, and if you are
diligent you find something plausible and "fix" it.

Three of these landed in one session (2026-08-14/15, the
[engine-simplification](../design/engine-simplification.md) round), and all three
had the same shape: **the assertion was fine, the run was not fair.** Something outside the code
differed between the two sides of the comparison.

| what went red | what actually differed |
|---|---|
| `midi-check` phase B, all six assertions at once | the sender published for a fixed 12s of wall clock; the cart starts after a *variable* compile (3.9s idle → 13.7s under three `build-all` sweeps). On a busy machine the cart booted after the sender exited. Nothing was measured |
| `refactor-guard`: *"audio diverges at 0.0s · state diverges at frame 0"* | `build/saves/<cart>/` — untracked, mutable, **rewritten by every run**. `acidcandy` persists a 437 KB `cart.blob`; deleting it produced that message with identical numbers on an unmodified tree |
| a hand-rolled `--resize` A/B across two worktrees | the same save blob, in two trees with different run histories. It cost a correct refactor a day and a "reverted as unsafe" verdict that was wrong |
| `refactor-guard`, red *immediately after its own bless* (2026-08-16) | **HEAD moved between the bless and the compare.** Several agents commit to this branch and one landed a `runtime/studio.c` change mid-loop, so the baseline described a tree that no longer existed. Caught in the act on the run that finally worked: HEAD went `5873a465` → `dc35b110` *while the bless was still running*. Four bless/compare cycles were spent before `git log` explained it. The guard now compares its recorded `blessed_at_commit` against HEAD and says so — in `--quiet` too, since that is the form repo-doctor shows |
| `au-transport-check`: *"the same 8 beats fire the same notes at 2x tempo"*, ratio 0.87 → 0.69 (2026-08-16) | **The assertion was fine and its PROXY stopped being valid.** Onsets are detected as an envelope rise against the envelope 6 ms earlier, so anything *sustained* lifts the floor a transient must clear. The cart had just been changed to boot with an audible legato 303, which masked drum onsets — and more so at the faster tempo, where the line is denser. The sequencer was measured at exactly **4.00 steps/beat at both tempos**. ⚠ Then the obvious repair made it worse: lowering the detection floor to count more onsets dropped the ratio to **0.52**, the exact signature of a free-running sequencer, because 8 beats at 180 BPM is *half the seconds* of 8 beats at 90 and the extra events were reverb tails scaling with WALL TIME. Fixed by high-passing the detector, not by moving the threshold |

What to do about it:

- **When one check disagrees with every other check, suspect that check first.** The tell in the
  worst of these was written down and read as a strength: *"the first two gates are green and only
  the third catches it."* The third was the only one comparing across two trees — the only one
  exposed to the confounder. Extra sensitivity that no other gate shares is usually noise, not power.
- **Run the null A/B.** Before believing any difference, compare the thing to *itself*: same source,
  both sides, through the identical path. If that is not identical, you have no oracle yet. This one
  control would have caught all three, and it is one command.
- **A cart that persists state is not a pure function of its source.** Any A/B on one must wipe or
  isolate `build/saves/<cart>/`. `refactor-guard` does this for its own probes now
  (`saves/.refguard/<cart>`, wiped per run); a comparison you write by hand still has to.
- **Timing is an input too.** If a gate coordinates two processes by wall clock, the slow side is
  whatever the machine is doing — and this repo runs several agents on one tree, so "somebody else
  is compiling" is the normal state. Bound the *waiter*, not the *worker*: let the helper outlive
  any plausible compile and stop it when the real work finishes.
- **Make the unfair run say so.** `midi-check` now reports `THE GATE RACED, not the engine` when the
  sender died first, instead of six parse failures. A gate that can tell "I was not measured" from
  "you broke it" is worth more than one that is merely sensitive.

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
