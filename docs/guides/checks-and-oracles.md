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

**So: if your check makes a judgement, give it a `--selfcheck`.** Fifteen now do — **278 assertions**
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

**The one question that catches all three:** *what would I have to break to make this go red?* If you
cannot name a specific edit, the assertion is decoration. Run it on the three above: break the gate
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
