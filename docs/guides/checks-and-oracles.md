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
| **any** draw primitive (the everything-cart) | **`canvas-diff.js drawall`** | `drawall.c` exercises EVERY draw command with per-frame rotation — one run covers the whole draw layer. Budget is the cart's declared **`--max 64`** (a `// canvas-diff: max` directive canvas-diff reads): almost everything is byte-exact, and the one accepted ~63px is the FRACTIONAL-scale `sspr` (16→24) whose nearest-neighbour texel-boundary ties aren't byte-portable across GPUs. **This is a GPU↔SW PARITY oracle — it catches gross breakage, NOT a subtle ±1-texel sampler change** (a parity oracle can't: on a GPU that rounds the tie down, the old truncation bug even makes the diff *shrink*). **Now automated: the `sw canvas` row in `repo-doctor` runs `canvas-diff drawall --golden` every time**, because as a manual command it sat RED for 20 days (the golden was blessed three minutes before `drawall` gained its blend strips and nobody re-blessed). To lock the SW sampler's absolute output use **`canvas-diff.js <cart> --golden`** (SW render vs a committed golden PNG in `tools/canvas-golden/<cart>/`; deterministic across machines, so it fails on any sampler/rasterizer drift regardless of GPU tie-breaking; `--bless` to (re)record). **Adding a draw command? add a call there too** (CLAUDE.md "Adding a new API function" step 5). |
| `cls`/`pset`/`pset_rgb`/fills on the canvas (byte-exactness) | the `swcanvas_test` **cart** (`canvas-diff --bytecheck`, or the two-run `shasum` in its header) | byte-identical GPU↔SW for the integer primitives |
| anything that should be **left/right symmetric** | **`mirror-diff.js <cart>`** | the render mirrors about its centre (catches handed rasterizers) |
| a **coverage-based road / field** (streetlab, roadlab) | **`road-check.js [--all] [--overlay]`** | framebuffer invariants (no naked edges/strays/floating kerb) at any angle |
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
| **inharmonicity / dispersion / stretched tuning** — the allpass chain in a string engine, a partial-ratio table, anything claiming partials are "stretched" | **`inharm-spec.js`** — partial frequencies in cents vs the ideal `n·f0`, plus the fitted stiff-string `B` and its residual. `tune-check` measures the FUNDAMENTAL and `harmonic-spec` measures partial LEVELS, so between them a partial can drift 30¢ sharp and read only as slightly quieter — which is how PIANO shipped with an inert dispersion chain (**§I4b**) and a stretched-tuning seam working in the treble but cancelled in the bass (**§I4c**) while both gates stayed green. **Run `--check` first**: a broken tool and a genuinely harmonic engine print the same table. **A voice's SUSTAIN changed?** `inharm-spec <wav> --f0 <hz> --decay` gives per-partial decay in dB/s — `wav-envelope` measures the whole signal and so cannot separate "the fundamental dies faster" (a loop bug) from "only the upper partials do" (spectral), which is the distinction that settled §I4b |
| **levels / effects** | **`level-check.js`**, **`fx-check.js`**; feedback/voice-lifetime → **`soak-check.js`**; DC offset → **`dc-check.js`** |
| engine math / optimizer | **`web-audio-check.js`** (wasm-vs-native parity) |
| want a WAV A/B vs navkit | **`wav-analyze.js`** + **`wav-correlate/-envelope/-modrate.js`**, **`harmonic-spec.js`** |
| an effect wired into `update()`/`draw()` | **`lint-fx-frame.js --quiet`** (catches set-and-hold effects rebuilt every frame) |
| a **table/shape swapped mid-note** — `wave_set` while a voice runs, a re-quantised morph, a retuned envelope shape | **`click-check.js <wav> [--quiet]`** — first difference vs the LOCAL step-rms, so a saw's flyback is not a false positive. A cart's own waveform slope sits at 2-3x; an audible click is 6-20x. **Render the unaffected mode as the control** and compare, because the threshold is only meaningful against the same voice. `martenot`'s morph shipped with 13 splice-like events (worst 15.4x) under a source comment asserting the stepping was inaudible: an amplitude/centroid envelope cannot tell a clean ramp from a splice, so nothing we had could see it until an ear did |
| a voice/solo **drops out or is "cut off by another instrument"** | **`voice-trace.js <trace>`** (reads a `--trace` run's on/off/reuse/steal/choke — is it real voice loss, and by whom?) + **`play.js … --solo-slot <n>`** (stem render — or is it just masked/quiet?). Design: [`../design/audio-voice-debugging.md`](../design/audio-voice-debugging.md) |
| **anything STEREO** — `autopan`, `pan()`/`instrument_pan`, `pan_law`, chorus/flanger width, the stereo-linked master soft-clip | **`stereo-check.js <wav> [--expect mono\|wide\|decorrelated\|autopan] [--rate hz]`**. Every other gate on this page reads the WAV through a `readWavMono()` that averages L and R at the door, so **none of them can see any of this** — and a mono downmix is not merely lossy, it is *blind by construction*: antiphase panning has `gL+gR ≈ constant`, so summing removes the effect almost perfectly and the louder the pan the less a mono gate sees. Judge motion by the **pan trace** (excursion + rate), never the mean: a hard L↔R sweep and a dead-centre file have the same average pan. **Run `--check` first** — a broken analyser and a genuinely centred file print the same numbers. `--expect wide` gates WIDTH only, deliberately not correlation, because a mono source panned hard left is one signal at two gains and stays at `corr` 1.0 |
| **the PSOLA pitch engine** (`at_psola_slot`, `sample_autotune`, `sample_shift`, `autotune_mic`, `harmonize_mic`) | **`psola-check.js`** for artifacts (does it CLICK) **and** `formant-check.js` for correctness (is it in TUNE). Run psola-check BEFORE and after: it renders `voxshift`'s four takes through three detectors, and **no single one of them is sufficient** — a period-doubled take is still perfectly periodic, so a periodicity metric scores that regression as a 2x *improvement*. This gate exists because a full green sheet plus exact pitch numbers shipped audible popping anyway |

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

**So: if your check makes a judgement, give it a `--selfcheck`.** Three now do, all gated in
`repo-doctor` (`selftest: ledger` / `selftest: xrefs` / `selftest: doc refs`):

| tool | fixture | pins |
|---|---|---|
| `status-check.js --selfcheck` | `tools/fixtures/status-check/ledger.md` | 13 — every finding kind, plus the two shapes that made v1 cry wolf |
| `lint-xrefs.js --selfcheck` | `tools/fixtures/lint-xrefs/docs/` | 5 — both tiers, plus the HUB and fenced-code exempt classes |
| `stale-doc-check.js --selfcheck` | `tools/fixtures/stale-doc-check/docs/` | 7 — the four verdicts BROKEN REFERENCES rests on (gone / never-existed / foreign / present) |

Both doc-scanning tools take a **`DE_DOCS_DIR`** override so the fixture is scanned instead of
`docs/`, while `ROOT` stays the real repo — so the fixture is adjudicated against real `srcExists`
and real git history, exactly as in production.

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
