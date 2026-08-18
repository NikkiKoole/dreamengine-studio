# Handoff / working notes

> Portable context for picking dreamengine up on another machine or in a fresh
> session. This is the stuff that isn't obvious from the code or git log. Keep it
> short; prune what goes stale.
>
> **For "what's shipped vs. open vs. cut" see [`STATUS.md`](STATUS.md)** — the single
> status ledger. This file is the running narrative + environment gotchas.

**How this file stays useful (the system).** The `## Where we are right now` section is
**ACTIVE LANES only** — one dated `▶ ACTIVE THREAD` callout per complex in-flight effort, each with
(1) what shipped, (2) a **Resume-at** pointer to the owning doc's pick-up point, (3) any hot files
to avoid colliding on. Rules that keep it honest: **refresh your lane's date whenever you touch it;
prune a lane the moment it ships or goes quiet** (its detail already lives in `STATUS.md` + the
doc's pick-up point — don't duplicate, point). **Write every Resume-at as a real anchor link —
`[text](path#section-slug)` pointing at the target `.md`'s heading, not prose like "→ §3" or "(doc §Foo)"** — so the pointer's
target is machine-checkable: when work ships and that section gets renamed, the anchor breaks and
`--check` catches it. Keep the *status* itself in the doc (point, don't restate) so a shipped slice
can't leave a stale sentence here (the trip-up: a "resume at trim+speed" line survived weeks after
trim+speed shipped, because the status was copied into the pointer instead of pointed-to). A lane
dated **>2 weeks** old is presumed stale — verify or prune. Everything below the lanes is history;
trust `STATUS.md` + the design board over it. **Tooling keeps this honest** (`tools/handoff.js`, the
[driftable two-door pattern](design/driftable-docs.md)): `node tools/handoff.js` lists the active lanes + age (and it's the first
thing `orient` prints — the front door); `node tools/handoff.js --check` flags a lane >2wk old, a
broken doc link, or a **broken `#section` anchor** (surfaced by `cart-status.js` — the back door).
So a forgotten stale lane *surfaces* instead of rotting.

_Last updated: 2026-08-18._ **The lane list is NOT maintained here — run `node tools/handoff.js`.**
*(This date covers the file itself. Per-lane freshness is `handoff.js --check`, which is the only
thing that can see a lane whose body was edited after its header date.)*
It prints every lane with its line number and age, read straight from this file, so it cannot drift.

---

## Where we are right now

**Active lanes, newest first.** `node tools/handoff.js` is the index (line · title · age);
`node tools/handoff.js --check` is the back door (>2wk old · a missing or unanchored Resume-at ·
a broken doc link or `#section`).

> Two things lived here until 2026-07-30 and were deleted because both had rotted: a prose
> re-summary of every lane (9.5 KB on a single line) and a hand-numbered index. The index said
> "sixteen lanes" when there were nineteen, and omitted three — including the newest and topmost.
> What a reader needs to *choose* a lane is in the front-door output; what they need to *resume*
> one is in the lane itself. A summary in between is a third copy, and it is the copy nobody
> updates. If you find yourself writing one again, teach `handoff.js` to print it instead.
> **▶ ACTIVE THREAD (2026-08-18) — `pedalboard` AS AN AUDIO EFFECT (`aumf`): IT WORKS IN GARAGEBAND, but it REPLACES the track instead of processing it. The boot-chain question is closed; passthrough is the new top blocker. Items listed first because this lane is long.**
>
> **▶▶ PICK UP HERE. Verified working by the maker 2026-08-18:** the cart plays its own guitar AND
> the host's piano runs through the pedal chain, in GarageBand on macOS. What is left, ranked:
>
> | # | Open item | Kind | Where |
> |---|---|---|---|
> | ~~1~~ | ~~**Was the chain DRY at boot?**~~ **✅ CLOSED 2026-08-18 — NO. Measured in GarageBand, maker touching nothing: the engine holds the default 12 at `pulls 0`, then the cart's own `5 · kinds 7,6,2,0,8` from the FIRST rendered block, unchanged thereafter, `dropped 0`.** The hypothesis (something clears the inserts after `init()`) is disproved. Seeing `12` before `5` is the reading's own liveness control — which is why the probe now sits ABOVE the `pulls == 0` return. | done | §4.1c |
> | **1b** | **THE EFFECT DOES NOT PASS ITS INPUT THROUGH.** `sound.h:6536` gates host audio on `extin_mon_on`, which boots OFF — so with `GTR: IN` off the plug-in REPLACES the track instead of processing it (insert it on a piano track and the piano is GONE). Unity passthrough is the contract of an insert slot. Also `shouldBypassEffect` is unimplemented (0 hits), so the host's off switch does nothing. **This is now the top blocker** and it reframes item 2. | **DEFECT, and the real one** | §4.1d |
> | 2 | **An insert cannot reach the amp EQ/TIMBRE or the FUZZ** — they are per-voice (`instrument_*(I_GTR,…)`) and the monitored input joins the mix after them. The pedals, amp DRIVE, amp GLUE and Leslie DO reach it. Both will read as broken controls on a track. ⚠ **Premature until 1b lands** — "which stages reach the insert" cannot be answered while the insert passes no audio. | **DESIGN CALL, the maker's** | §4.1c |
> | 3 | **`apps/pedalboard/app.json` sets no `auCart`**, so the shipping app contains no plug-in. Correct to hold until 1 and 2 close: a component triple is FOREVER and `aumf tpdl Mpla` is still free. | sequencing | §6.3 |
> | 4 | **`param_bind` is called ZERO times** (acidcandy: 15), so `parameterTree` is nil and a DAW can automate no knob on this rack. | separate job | — |
> | 5 | **Both probes are still in the code** (`de_fx_chain_probe`, `de_sound_dropped`, the INDIAG block). ⚠ **Unblocked by 1 closing — but do NOT delete yet:** they are the only instrument that can show 1b landing (the FXCHAIN + peak pair is exactly how you prove the input reaches the output). Delete after 1b. | cleanup, after 1b | §4.1c |
>
> ⚠ **A THIRD trap, learned today: do not let the maker insert the plug-in while a build is in flight.**
> The binary was replaced under a loaded plug-in at 15:17:37, 28 s after it was inserted — trap #1 by
> accident — and the instance ran the OLD code, printing `IDLE` with no `FXCHAIN` beside it. Confirm the
> install landed (hash the appex against `build-mac/.../TinyjamMacAU`) BEFORE saying "go". Also: `strings`
> cannot tell the builds apart — Swift packs short literals inline, so `· pulls ` is invisible to it. Hash, don't grep.
>
> ⚠ **THREE TRAPS THAT COST THE MORNING, all of them mine, none of them the engine:**
> 1. **NEVER rebuild while the DAW has the plug-in loaded.** It leaves GarageBand holding a plug-in
>    that no longer exists on disk: the slot keeps the panel and stops receiving audio. Quit the DAW,
>    then build. (Recovery if it happens: clear the slot → quit → reopen → re-insert. If still dead,
>    the AU cache may be stale — `~/Library/Caches/AudioUnitCache/com.apple.audiounits.sandboxed.cache`,
>    move it aside and `killall -9 AudioComponentRegistrar`.)
> 2. **READ THE PROBE PER PID.** A panel can live in its own process whose audio unit never renders
>    by design, so `IDLE` from that pid is not a verdict on the plug-in. Two pids in the log = look
>    for the one with `pulls` climbing. `TinyjamAUViewController` already prints "which is the orphan".
> 3. **Two kinds of log noise that are NOT ours:** `com.tinyjam.mac.AU` teardown chatter is a
>    DIFFERENT app's plug-in (Tiny Jam), and a `runningboardd` `RBSStateCapture` block lists every
>    process on the machine.
>
> ⚠ **An uninstalled change is pending:** the `IDLE` message was reworded (commit `d6ae16b9`) and
> NOT rebuilt, because the maker's DAW was open. First build on the next machine picks it up.
>
> **✅ THE SILENCE IS CLOSED (2026-08-18), and it was OURS.** `mData` is an IN-OUT field: we offered
> the host our scratch buffer, the host pointed `mData` at its own memory (legal, and how no-copy
> rendering works), and we read our scratch back. Fresh pages are kernel-zeroed, so we fed the engine
> perfect silence with every counter green. Confirmed in GarageBand: `peak 0.26050`, tracking the
> playing. ⚠ `GTR: IN` must be ON — `input_monitor()` is the tap and it boots off.
>
> ⚠ **The probe is the reusable part.** `peak 0` alone could not tell "the host wrote silence" from
> "the host wrote nothing" (zeroed pages read the same), and would have sent us to check GarageBand's
> routing. Poisoning the offered buffer and seeing the poison survive 100% of pulls is what named the
> culprit. It also killed §4.1b's load-bearing claim that **auval feeds real input** — it does not,
> so a green `auval` never covered this path at all.
>
> **▶ NEXT, and it is a DESIGN call not a fix:** an insert reaches the 9 pedals, the amp's DRIVE and
> GLUE and the Leslie, but **not** the amp's EQ/TIMBRE nor the FUZZ, because those are per-voice
> (`instrument_*(I_GTR, …)`) and the monitored input joins the mix after them. Both will read as
> broken controls on a track. Table + the two options:
> [`design/auv3-plugin-types.md` → §4.1c](design/auv3-plugin-types.md#41c-what-an-effect-build-can-and-cannot-process--the-amp-is-half-wired-for-it)
>
> **✅ SHIPPED (5 commits, `f62d4e64`..`8ef2c99e`).**
> 1. **Per-app CARRIER + identity** — auditioning pedalboard on macOS silently DEREGISTERED the Tiny
>    Acid Jam plug-in mid-review, because `project-mac.yml` hardcoded one carrier (`com.tinyjam.mac`,
>    product `TinyjamMac`) for every app and `mac.sh` installed it to one path with an `rm -rf`.
>    Renaming the `.app` would not have fixed it — LaunchServices keys by BUNDLE ID. Both derived now;
>    `TinyAcidJamMac.app` and `TinyPedalboardMac.app` coexist and both register.
> 2. **`pedalboard` is per-instance** — 43/43 mutable statics into generated `runtime/pedalboard_state.h`
>    via `ctx-gen.js --target cart` + `#define DE_CART_CTX`. Byte-identical at every stage.
> 3. **Insert latency MEASURED: 0 samples, constant, unity gain** (`tools/insert-latency.js`, probe cart
>    `inslat`, `--check` 10/10). This retired §8 Q1 and promoted Q2 to the real blocker.
> 4. **`aumf` WIRED** — `"auType"` derived per app (so an effect app and an instrument app share the
>    specs without re-typing each other), input bus declared when the unit's own componentType is an
>    effect, render block pulls host audio → `de_audio_input` BEFORE rendering. `auval -v aumf tpdl Mpla`
>    SUCCEEDS incl. effect render tests at 512/64/4096 frames and 11025–192000 Hz.
> 5. **Two gates stopped lying** — `au-transport-check`'s rig feeds an effect no input, so it rendered
>    silence: 5 red checks that meant nothing plus one GREEN ("STOPS when the host stops") passing
>    BECAUSE of the silence. Transport + `--panel` now SKIP for effect types; verified both directions
>    (instrument still passes, 16 onsets peak 0.607).
>
> **▶ THE ONE OPEN THING.** In GarageBand the plug-in appears under the E-Piano, the panel draws,
> `GTR: IN` is lit, `AUTO` is `off` — and playing the piano is SILENT. Because AUTO is off the cart
> makes no sound of its own, so the monitored input is the only possible source: this is a clean
> signal, not a mix problem. `auval` passing its effect render tests means pull→ring→output works
> *somewhere*, so the fault is between GarageBand and that path.
> **Start with the cheap split:** add a `deDiag` counter for pull failures + the peak of `inMono`, read
> it with `/usr/bin/log show --last 2m --predicate 'eventMessage CONTAINS "[tinyjam]"'` (⚠ `/usr/bin/log` —
> zsh shadows `log`). That separates "the host never gave us audio" from "we got it and lost it", which
> have disjoint fixes. Second suspect: `GTR: IN` lit only proves the cart's flag flipped; the cart is
> set-and-hold via an `ap_gtr_in` shadow, so anything zeroing `extin_mon_gain` after it applied
> (`sound.h:8334` does that in a reset path) leaves a lit lamp over a dead monitor.
>
> ⚠ **ONE INSTANCE ONLY** — the extin ring is still process-global (`sound_extin[]`/`extin_w`/`extin_r`/
> `extin_on` did NOT move; only `extin_mon_on`/`_gain` did), and it is single-producer/single-consumer,
> so two effect instances garble each other at ANY rate. Fix that group before shipping an effect.
> ⚠ **The component TYPE is part of the FOREVER triple.** pedalboard could be typed freely only because
> it has never shipped a plug-in — its manifest still sets **no `auCart`**, so nothing goes to the store
> yet. Decide before the first upload.
> ⚠ **iOS untouched** — Mac Catalyst only. And `auval` is not a DAW: nothing here was judged by ear.
> ⚠ **Quit the DAW before rebuilding a plug-in.** A hot swap makes GarageBand show "An Audio Unit
> plug-in reported a problem", which is indistinguishable from a real crash (verified benign that
> time: no `.ips` anywhere, and the unified log showed only clean teardowns).
>
> **Hot files:** `ios/AU/TinyjamAU.swift`, `ios/au-identity.sh`, `ios/mac.sh`, `ios/au-transport-check.swift`.
> **Runbook for all of this:** [`guides/cart-as-plugin.md`](guides/cart-as-plugin.md).
> **Resume-at:** [`design/auv3-plugin-types.md` → §4.1d the effect does not pass its input through](design/auv3-plugin-types.md#41d-the-effect-does-not-pass-its-input-through--and-that-is-the-real-blocker)

> **▶ ACTIVE THREAD (2026-08-17) — THE OTHER FOUR PLUG-IN SHAPES: we ship `aumu` and the ecosystem has five.**
>
> A survey, not built work. The AUv3 lane below got `acidcandy` behaving as an INSTRUMENT
> (`aumu`), which was the right first goal and is also the only shape anyone has looked at.
> `componentType` has five musically distinct values, each a different slot in the host's chain
> and a different shelf to compete on. Two findings drive the doc:
>
> 1. **`acidcandy` is ALREADY a complete MIDI sequencer that a DAW cannot hear.** Everything at
>    `acidcandy.c:2540`+ is finished (303a ch1 / 303b ch2 / 808 GM ch10 / 909 GM ch11, slides,
>    accents as velocity, drum gate lengths, 24ppqn clock, transport, a shutdown flush) and it
>    goes to a CoreMIDI VIRTUAL SOURCE, the standalone path. A plug-in's MIDI reaches its host
>    through `MIDIOutputEventBlock`, and the engine has NO outward seam: `ios/Sources/engine.h`
>    exports audio-in, notes, bend, CC and transport, nothing back. **Cart side is done**; the
>    work is a ring behind `midi_send_*` plus a `de_midi_out_drain(DeInstance*, …)` for the
>    render block. Payoff is an `aumi` MIDI processor that plays somebody else's instruments:
>    near-zero CPU in the host, emptiest shelf, and the PANEL becomes the whole product.
> 2. **`de_audio_input` already exists** (`runtime/platform.h:215`, exported in `engine.h`) and is
>    exactly the shape an `aufx`/`aumf` render block hands over, but the AU declares an EMPTY
>    input bus array (`TinyjamAU.swift:102`). Wiring it makes the visuals hear the host's track
>    and turns `pedalboard` into an insert effect. Declare `aumf`, not `aufx`. Three caveats in
>    the doc, chiefly that the mic ring was built for ANALYSIS and its insert latency is unmeasured.
>
> ⚠ **A ship-blocker found on the way: `apps/tinyacidjam/app.json` set no `auCart`, so
> `testflight.sh` STRIPPED the AU target and Tiny Acid Jam was going to the store with NO PLUG-IN.**
> **Manifest line added 2026-08-15; the identity it needed is DERIVED as of 2026-08-16** (doc §6.1).
> The iOS `project.yml` AU identity was *tinyjam's* (`tnyj`/`Tnyj`/`"Tinyjam: Demo"`) and the file is
> shared by both apps, so with `auCart` on both they would have shipped plug-ins claiming the SAME
> component triple, the collision `project-mac.yml`'s own comment warns about. Now four manifest keys
> (`auName`/`auSubtype`/`auManufacturer`/`auDisplayName`) are validated + substituted in
> `testflight.sh`; Tiny Acid Jam derives `aumu tacj Mpla` (matching the mac spec) and **Tiny Jam is a
> byte-identical no-op**, because a shipped triple is FOREVER (a DAW stores it to re-instantiate).
> `DERIVE_ONLY=1 APP=<name> ./testflight.sh` runs it in a second with no Xcode. Six validators,
> mutation-tested; one of them was green and BLIND (`*[A-Z]*` passes `mpla`, since a shell bracket
> range is collation-based) which is worth knowing for every `[A-Z]` in a shell script here.
> **✅ IT ARCHIVES, WITH THE PLUG-IN IN IT (2026-08-16).** `SKIP_UPLOAD=1 APP=tinyacidjam
> ./testflight.sh` produces `com.mipolai.tinyacidjam` + the extension
> `com.mipolai.tinyacidjam.TinyjamAU` carrying `aumu tacj Mpla` "Mipolai: Tiny Acid Jam" and
> `com.apple.AudioUnit-UI` (so the panel, not host sliders). The CHILD App ID was a non-issue:
> `-allowProvisioningUpdates` registered it, no portal work. **`device.sh` derives the same names
> now**, into a gitignored `project-dev.yml` copy so `project.yml` is never rewritten.
> ⚠ **What actually blocked the first archive was neither**: `build-app.js` staged NOTHING because
> `SOUND_CART_CTX` moved into the generated `sound_ctx.h` and the parser still grepped `sound.h`, so
> EVERY app build was dead at a message that blames the wrong file. Fixed (searches both homes).
> Worth remembering: no gate in the repo runs `build-app.js --ios`, so the store path can be fatally
> broken and green.
>
> **✅ SUBMITTED (2026-08-17): v1.0 is WAITING_FOR_REVIEW with build `202608162137`**, which carries
> the AUv3. The 2.1.0 rejection was Apple's standard new-app "Information Needed" form, not a defect:
> the seven answers now live in `apps/tinyacidjam/app.json` → `review.notes` (pushed by
> `asc-push --review-contact`, which now REFUSES a notes body containing a placeholder — the previous
> app's reply went out with a literal `[FILL IN …]` where the tested-devices answer belonged).
> **▶ NEXT: nothing until Apple answers.**
>
> ⚠ **Three traps from getting there, each worth an hour to the next app.** (1) **ITMS-90473** after
> the upload: the AU shipped `CFBundleVersion: 1` inside an app stamped with the build number, because
> the app uses `GENERATE_INFOPLIST_FILE` (so the command-line `CURRENT_PROJECT_VERSION` reaches it)
> while the AU has a hand-written plist, into which xcodegen bakes a LITERAL that no build setting can
> override. Both specs now reference `$(CURRENT_PROJECT_VERSION)`; fixed AFTER the submitted build, so
> it rides the next upload. (2) **QuickTime's iOS-device capture crashes on macOS 26.5.2** when the
> device rotates — AVKit's floating playback controls throw inside their own KVO teardown, and the
> crash log is 100% Apple frames with nothing of ours in it, so it reads like an app crash and is not.
> Use OBS, source set to Transform → Fit to screen, or the app records at a quarter of the frame.
> (3) **`device.sh` deployed to an OFFLINE iPad while the iPhone was plugged in** — devicectl lists
> every device it ever paired with, the pick filtered on NAME, and a remembered iPad has one. Now
> requires a live tunnel. And the ICON still cuts real detail (bottom-right 45.4%): it shaves the
> purple chassis border at the corners, legible but uneven,
> `node tools/icon-mask.js preview apps/tinyacidjam/icon.png` to judge.
>
> **✅ AND THE CLASS IS CLOSED, not just the instances (2026-08-17).** Every failure above shares one
> shape: **a per-app value hardcoded in a spec shared by every app**. So per-app facts are now
> DERIVED and the artifact is ASSERTED before upload. `ios/app-flags.sh` holds the resizable/
> orientation decision + the mic string + `app_preflight` (4 checks on the built archive, run before
> the upload AND before the `SKIP_UPLOAD` exit); `ios/au-identity.sh` holds the AU identity; both are
> SOURCED by build/device/testflight so the three cannot drift again. `build-app.js` derives the mic
> string from `mic_start()` calls in the carts and REFUSES to build a mic app with no wording.
> ⚠ **Two of these were caught only by running the thing**, and both are written up where they bit:
> restricting `UISupportedInterfaceOrientations` restricts iPad too (the `~ipad` variant never
> reaches the built plist), which is the upload rejection `project.yml` dates to 2026-07-06 — so a
> declared orientation is honoured at RUNTIME and WITHHOLDS the resizable define instead, and the
> preflight asserts that precondition. And `"${arr[@]}"` on an empty array is fatal under `set -u` in
> macOS's bash 3.2. **▶ Open, deliberately:** reflow and a locked orientation cannot both be had
> until the manifest's orientation reaches the runtime; and the mic purpose string is per-APP, so a
> multi-cart app wears one sentence for whichever rack listens.
> Same section covers the identity gap the maker noticed (wrong names on deploy):
> the app's bundle id / version / display name / icon ARE derived from the manifest, but the AU's
> `CFBundleDisplayName`, its **component name** `Tinyjam: Demo` (the string a DAW lists) and the
> manufacturer/subtype codes are hardcoded in `project.yml` for every app, and `device.sh` patches
> nothing, so a cable install is always `TinyjamHello`.
>
> **Hot files:** none (docs + `acidcandy` de:meta/comments only). Three OPEN todos are filed on the
> cart: `node tools/cart-todos.js acidcandy` (top three entries).
> **Resume-at:** [`design/auv3-plugin-types.md` → The three product shapes, ranked](design/auv3-plugin-types.md#4-the-three-product-shapes-ranked),
> and its §8 open questions (mic-ring latency · per-instance audio input · whether the MIDI ring
> replaces or joins the virtual source · one extension target per type or several).

> **▶ ACTIVE THREAD (2026-08-18) — ENGINE SIMPLIFICATION ROUND 2: the instruments were under-reporting, and three ports/paths were quietly broken behind them.**
>
> Four read-only sweeps (sound.h · studio.c + the host seam · ios/ · the ctx refactor) after the
> per-instance work. **The finding that reframes the rest: the code was CLEANER than expected** —
> 17 `#ifdef`s in 6606 lines of `studio.c`, zero dead statics / `#if 0` / unused params in
> `sound.h`. The residue was **half-moved state groups, and the instruments that should have seen
> them looking elsewhere.**
>
> SHIPPED: `engine-statics.js` silently dropped **30** statics (counted them, never printed it) —
> that is how `kv_data` shipped half-moved and how **`midi_input.h` was never measured at all**;
> now loud, `--check` 13→18, every new guard mutation-tested · `midi_input.h` 14 statics → **0**
> (`midi_ctx.h`), `de_midi_*` name their instance — its ring cursors are single-CONSUMER, so two
> racks were splitting one keyboard · `lint-engine-seam` scanned one file, required `extern`, and
> could not see a seam fn that never TOOK a handle → **new check D**, `--selfcheck` 14→22 · **the
> Android port could not link** (every declaration pre-refactor, `de_init` outliving its deletion)
> — migrated and verified ON THE EMULATOR, and the lint walks `android/` now · **`sound_reset_state`
> was resetting only part of the engine** — 34 config members leaked cart→cart through both restore
> paths · `pget`/loupe flipped about compile-time `SCREEN_H` on a resizable canvas · 4 `SR_*` kinds
> classified by neither switch, so the config log grew unbounded.
>
> ALSO SHIPPED (second pass, same day): **`blend_lut` is shared again** — 20,480 B of constant table
> was in every engine and rebuilt by ~1M iterations per boot; `DeVideo` 31,432 → 10,952 B ·
> **`de_instance_destroy` gives the memory back**, entering the instance first (on a host thread the
> macros name the DEFAULT engine, so freeing through them would free rack #1 while destroying rack #2)
> · and the leak that was found while gating that one: **`de_init_impl` re-decoded the SHARED sprite
> sheet and font tables per instance**, orphaning the previous copy in the shared slot — ~146 KB per
> rack, and a `recs`/`glyphs` pointer swapped under a sibling that may be inside `print()`.
> **`tools/instance-check` now gates destroy** (8 create/destroy rounds must leave the heap flat;
> `-bypass` control) and goes RED against a worktree at the previous `HEAD` at ~1 MB per rack opened.
>
> ══ **25 of 35 closed (2026-08-15, second session). THE TWO HEADLINE SWIFT ITEMS ARE WIRED AND ARE
> WAITING ON A DEVICE PASS — that pass is the single next action in this lane.** ══
>
> **▶ WHAT THE MAKER NEEDS TO CHECK, on the iPad with GarageBand.** Both fixes below landed WITHOUT
> a red-first reproduction — that was the maker's call ("fix what you can already, I'll hook up the
> iPad") and it is recorded here so nobody later mistakes them for proven. They compile
> (`zsh ios/mac.sh`, six gate sections green) and no repo gate can see either one.
>
> 1. **STOPPED-HOST PANEL.** Load the plug-in, leave the host transport STOPPED, and tap the cart's
>    own play button on the panel. Before: frozen picture, every tap swallowed, "I can't start it
>    from the host OR from the plug-in". After: the panel should be live and clickable at display
>    rate while the host sits still. ⚠ If it still freezes, **the wiring is the first suspect, not
>    the last** — check `c.onDisplayTick` is reached in `connectPanel()`, which is only entered on
>    whichever of `createAudioUnit`/`viewDidLoad` completes the pair.
> 2. **THE RACK IS GIVEN BACK.** Add the plug-in to a track and REMOVE it, several times, watching
>    memory. Before: nothing on the Swift side ever called `de_instance_destroy`, so ~1 MB per rack
>    stayed. After: it should be flat-ish across add/remove cycles. This is the fix that finally
>    spends the 08-14 destroy work.
> 3. **AND WHILE YOU ARE IN THERE, the ear check that was already waiting** (lane `L255`): the mod
>    wheel is verified but **the BEND never was** — both 303 lines boot muted, so unmute them, then
>    bend and confirm the two acid lines move ±2 semitones and the drums do NOT.
>
> **▶ FOUND ON THE DEVICE 2026-08-15, and neither needed the ledger to be readable.**
>
> **✅ (a) THE LEAK IS FIXED, AND IT IS A REAL RED-THEN-GREEN.** Second device run, ledger readable:
> `CREATE 1 · 1 live` / `DESTROY 1 · 0 live` / `CREATE 2 · 1 live` / `DESTROY 2 · 0 live` /
> `CREATE 3 · 1 live`. **Live bounces and never accumulates**, and the perf-logger rate holds at 1/s
> instead of climbing, so the VIEWS are released too. `de_instance_destroy` is finally being called
> on a device. The `deinit` item is closed; details in
> [engine-simplification](design/engine-simplification.md#open--swift--ios).
> ⚠ **`uiTick` is NOT closed by this.** The panel is responsive on the fixed build, but the
> stopped-host freeze was never captured on the broken one, so it has no red. Left as reading plus
> one observation, and marked that way.
>
> **(a-was) HOW THE RED WAS OBTAINED, because it was an accident worth repeating.** iOS redacts every dynamic value in an
> `NSLog` (see below), so the first device run came back as pages of `<private>`. But `CanvasView`'s
> perf logger emits one line per instance per second, so the LINE RATE is an instance counter:
> **1/s → 2/s → 3/s → 4/s across four panel opens, and it never came back down**, in a single
> extension process (GarageBand reuses it — *"A process already exists for this handle … :1643"*).
> ⚠ What that proves is that SOMETHING leaks per panel-open, not yet that it is the audio unit or
> that the fix cures it. The CREATE/DESTROY ledger is what settles it.
>
> **(b) THE CART'S SAVE FILE IS DENIED BY THE SANDBOX** — `System Policy: TinyjamAU deny(1)
> file-write-create /cart.blob` (and `/perf.json`). Both at the FILESYSTEM ROOT, because **nothing
> in `ios/` ever calls `de_set_save_dir`**, so the engine writes relative to a working directory
> that is `/` for a sandboxed appex. **acidcandy's rack does not persist inside the plug-in on
> iPad, silently.** macOS is fine (the Catalyst container gives it a writable cwd), which is why no
> gate and no earlier session saw it. NOT the same thing as the AU's `fullState`, which travels in
> the host's project file and works; this is the cart's own `save_bytes` layer, dead on device.
> Related to but distinct from "N racks share one `cart.sav`" below: there is no writable dir AT ALL.
>
> **(c) `NSLog` IS USELESS ON DEVICE.** iOS logging redacts dynamic values unless the format says
> `%{public}`, which `NSLog` cannot. Every `[tinyjam]` line arrives as `<private>` — including the
> PANEL diagnostic the `--panel` gate is built around. The new teardown ledger goes through `os_log`
> and is readable; the rest are not converted yet (see the note at the top of `TinyjamAU.swift`).
>
> ✅ **THE `--panel` GATE IS GREEN, AND THE CAUSE WAS THE LOGGING (2026-08-15).** It greps the
> unified log for `[tinyjam] PANEL`, and that log held **zero** such lines in three hours across six
> runs — the verdict was never retrievable, so the gate said "the view loaded somewhere else, or
> never loaded" about a panel that was fine all along. The control was already inside the same run:
> at 09:28 the teardown ledger was `os_log` while PANEL was still `NSLog`, and ONE process emitted
> one and not the other. Routing the verdict through `deDiag` fixed it; all four checks pass now,
> including the gate's own built-in can-it-go-red control. `connectPanel`'s silent
> `guard … else { return }` reports itself too now (`PANEL NOT CONNECTED YET · au=… canvas=…`), so
> "never opened" and "orphaned" are finally distinguishable — which is what open item 4 of this
> lane asked for. **Open item 4 is retired.**
> 
> ⚠ **KEPT FOR THE TRAIL, because it shaped a whole morning:** `--panel` was RED and it was NOT this change.** Verified by stashing both
> files and rebuilding at `HEAD`: **identical 3/3 failure, same messages.** It is the known-broken
> observation already recorded as open item 4 in this lane ("it cannot tell an orphaned panel from
> one that was never opened, yet its headline asserts the first"). Do not let it eat an hour.
>
> **WHAT IS LEFT AFTER THAT, and there are only two kinds.**
>
> **① Needs a real host (3, all Swift/iOS).** The `static let` canvas channel (latent — `remoteFrame`
> is deliberately nil, so the shared `owner` cannot bite until something wires it), the ARC/`malloc`
> on the audio thread bundle, and `AUProbeKit`.
>
> **② Legibility-only, remove ~0 lines (4).** `loop_step` and `sound_callback` splits, the twice-written
> shaders, the two boot sequences. My read: leave them until something makes them worth it — none is
> blocking anything, and `loop_step`'s `goto draw_window` makes the pause extraction non-trivial.
>
> So: **there is still no self-gateable engine work left in this lane.** Once the device pass above
> is signed off, this round is done and the lane should be pruned rather than continued.
>
> THIRD PASS (the `sound.h` dedups): the `DRIVE_*` waveshaper switch and the PLUCK/GUITAR
> Karplus-Strong excitation each existed twice and now exist once; **the 9 copies of "ensure FX_X is
> in bus b's chain" had DIVERGED** (two different bounds, and 7 of 9 cleared `insert_inst` while the
> two `FX_GRAINS` ones did not) and are one `fx_chain_ensure`. Each A/B'd byte-identical against a
> worktree at the previous `HEAD` on carts that actually reach the path, all verified non-silent
> first. And **`lint-aux-params` had been RED against the real source for weeks while `repo-doctor`
> showed green** — the refactor moved `eng_p[]` into `sound_ctx.h`, and `repo-doctor` only ever ran
> the lint's `--selfcheck` (a fixture), never the lint. Both fixed; the general rule is in the doc.
>
> FOURTH PASS (08-15) — and the theme of the day was that **three separate confident REDs were the
> measurement, not the engine**; the cross-cutting write-up is
> [checks-and-oracles.md → "The THIRD way a check lies"](guides/checks-and-oracles.md). `fb_w`/`fb_h` LANDED — its 08-14 "reverted as unsafe" verdict was a
> MEASUREMENT ERROR (the A/B compared two trees' `acidcandy` save blobs), and chasing that found
> **`refactor-guard` reading the same untracked save file**, which is now isolated per probe ·
> `midi-check` phase B diagnosed and fixed · `ctx-gen --verify` extended to FUNCTION-LOCAL statics
> the day after it shipped, because it failed to notice one I added · five write-only overflow
> counters got a reader · `ms_samp()` (the int form overflowed at ~48 s) · `at_psola_slot`'s dead
> `formant` param removed · one lock-free publish instead of two · and **`sw_tritex_legacy` +
> `DE_TRITEX_FAST` RETIRED** on the maker's call, which closes the soak-flag scaffolding entirely
> (⚠ `pset_batch` reads like the last sibling and is NOT one — a per-platform default). And **the
> `midi_out_on` refcount "finding" was WITHDRAWN** — the flag was right and I was not.
>
> **Resume-at: [the round-2 open list](design/engine-simplification.md#round-2--after-the-per-instance-refactor-2026-08-14)** — every open item names its gate, and the
> re-verified WON'T-DO list is there too (round 1's three calls still hold; I checked rather than
> assumed). Biggest remaining: **`TinyjamAU.uiTick()` is orphaned**, so a stopped host freezes the
> panel and swallows every tap · the shared-`static let` canvas channel · and
> `ctx-gen --verify`, which would close the half-moved-group CLASS rather than its instances. The
> Swift items need a real host to confirm; everything self-gateable in `studio.c`/`sound.h` that was
> open this morning is now landed or documented as reverted.
>
> ⚠ **THE ONE THING TO TAKE FROM THIS LANE: when a gate disagrees with every other gate, suspect
> the GATE.** Three separate times this round a confident red was the measurement, not the engine —
> `midi-check` phase B racing a compile it does not control, `refactor-guard` reading an untracked
> save file, and a hand-rolled A/B across two worktrees comparing their save blobs. The last two are
> the same trap: **a cart that persists state rewrites it every run**, so any A/B that does not wipe
> or isolate `build/saves/<cart>/` is comparing histories, not code. `acidcandy` carries 437 KB of
> it. `refactor-guard` isolates its own saves now; a hand-rolled comparison still has to.
>
> ⚠ ALSO: **the Android port had no baseline to A/B against** (the pre-migration code does not
> compile), so "was this already broken?" could only be answered by reading — which is how its ~13s
> JNI crash turned out to be pre-existing.
>
> ~~`midi-check` phase B is flaky~~ **DIAGNOSED AND FIXED 2026-08-14 — it was the gate, not the
> engine.** The sender lived a fixed 12s of wall clock while the cart's start time is a variable
> compile (measured 3.9s idle, 13.7s under three concurrent `build-all` sweeps), and the CC arrives
> at frame 1 — so past ~11.3s the cart booted after the sender exited and all six assertions failed
> together, looking exactly like a broken CC parser. Reproduced on demand, fixed, and it now reports
> `THE GATE RACED, not the engine` when it happens. ⚠ **And the old advice here was wrong**: a
> throwaway worktree at HEAD proved nothing about the code — it changed the machine load, not the
> tree's behaviour, and would have passed with a genuinely broken parser too.
>
> Hot files: `runtime/sound.h`, `runtime/studio.c` (targeted `Edit`s only), `tools/lint-engine-seam.js`.

> **▶ ACTIVE THREAD (2026-08-16) — GATES THAT CANNOT PROVE THEY GO RED.**
>
> `gate-controls` counts gates carrying a self-test or a negative control. **Twelve done, and ten of
> them turned out to be broken** — not "lacking a test", actually wrong in a way their own green
> output could not show. That hit rate is the reason this lane is worth continuing.
>
> **SHIPPED**, each its own commit, each mutation-tested and registered in `repo-doctor`:
> the audio block (`tune-check` · `click-check` · `dc-check` · `level-check` · `fx-check` ·
> `soak-check`), then `spec.js` (game logic), then the three pixel gates (`mirror-diff` ·
> `road-check` · `canvas-diff`), then `psola-check` and `web-audio-check`. Sound as they stood: only
> `tune-check` and `click-check`.
> The other ten, in one line each — `dc-check` measured its own window and not the engine ·
> `level-check` diffed PIANO against a *different* PIANO and had silently stopped rendering the last
> note · `fx-check` never checked the DRY reference everything is compared against · `soak-check`
> could not tell a 24-cycle run from no run · `spec.js` scored an empty `spec()` as a green tick, and
> its second control found `tenement` losing two assertion lines to an unescaped quote · all three
> pixel gates called an empty comparison a match · `psola-check` exited 0 having run half its checks
> when the baseline was missing, scored a SILENT take as perfect, and would bless a period-doubled
> render as the new reference · `web-audio-check` called an EMPTY wasm render bit-identical to a good
> native one, which was the strongest possible pass a build producing nothing could get.
> ⚠ **Do not quote a coverage tally here.** Several agents work this repo at once and the number
> moved twice while this lane was being written. `node tools/gate-controls.js` is the live count.
>
> ⚠ **TWO RULES THAT PAID.** *Measure before you assert* — several expected "known answers" were
> wrong (the octave-up asymmetry, the naked saw, the two silences, the window residual), and writing
> them down unmeasured would have moved an assumption into a file that then looks authoritative.
> *Mutate the control too, not just the analyser* — three plausible controls have now sat green
> through the very failure they were written for (level-check's SINE pair against a sliding window;
> canvas-diff's fixture re-implementing the predicate instead of calling it; and psola-check's two
> new liveness checks, which shared ONE assertion regex, so either alone satisfied it and deleting
> the other changed nothing). **Both rules fired again on `psola-check`, on the gate this lane had
> predicted they would** — its first synthetic "phase break" was a time reversal, which is still a
> sine at the same frequency, so all three detectors correctly read 0 and "the detector is blind to
> this" was one step from being written down as a known answer.
>
> **Resume-at: [the recipe + the two failure shapes](guides/checks-and-oracles.md#the-other-way-a-green-check-lies-it-was-never-measuring-the-thing)** —
> five steps and twelve worked examples. The reusable part is the two shapes to check any new gate for
> by name: **vacuity** (the verdict is about a set, and the empty set satisfies it) and **an unchecked
> reference** (everything is measured against something never measured itself). ⚠ A gate that
> COMPARES TWO THINGS is the acute vacuity case, and `web-audio-check` is the worked example: its
> subject is agreement, two nothings agree perfectly, so the empty input is the *strongest possible
> pass* rather than merely an accepted one.
>
> **What is left:** both hard ones are done. The remaining `--list` is mostly `build-*.js --check`
> staleness gates where failure is loud and a control buys almost nothing — **do not grind the list
> for the count.** Spend one where PASS is the steady state and failure would be silent. Candidates
> worth a look on that basis: `net-check.js`, `bundle-spike/proof-sound.sh`, `icon-mask.js`.
>
> ⚠ **`web-audio-check` needed TWO controls and that generalises.** A `--selfcheck` that synthesises
> its inputs never builds anything, so it is structurally unable to say the comparison reaches the
> real code — that is what `--bypass` (rebuild one side with `-ffast-math`, require red) covers. When
> a gate's subject is two *builds* or two *platforms*, expect to need both, and put only the
> toolchain-free one in `repo-doctor`.
>
> Hot files: none shared — each gate is its own file in `tools/`. Add the `repo-doctor` row in the
> same change (`selftest:` block) or the fixture exists and nothing runs it.

> **▶ ACTIVE THREAD (2026-08-14) — `tenement`: the item economy is switched on, and storage finally COSTS something.**
> The sim of several households sharing one building, built contract-first then fanned out to eight
> agents (ADR-0034). **SHIPPED:** the frozen contract `runtime/tenement/model.h` plus twelve modules
> (world/path/offer/agents/work/econ/store/social/art/hud/build/atlas), 242 `spec()` assertions,
> real BFS pathfinding over edge walls, the cutaway iso view, and a HUD where every number is a
> relation rather than a quantity.
> **THE LIVE QUESTION, and it is a design decision rather than a task.** design §1 promises "queues
> form, corridors jam" and §4 promises "one loom, four tenants, and a queue you can see". None of it
> was happening, and nothing in the repo could see that: every assertion describes a single DECISION,
> and only a distribution over time can tell you a building is dead. Measured, 99.6% of frames had
> somebody wanting a full object and 2.3% had anybody standing at one; the headline scarcity (one WC,
> four households) sat empty 91% of the time. **Four causes, stacked, so fixing only the top one moved
> nothing:** (a) waiting was banned rather than priced, FIXED, and the unit falls out for free because
> a tile of walking and a minute of waiting are the same quantity; (b) the score had no term for how
> long an action takes, so residents slept 62% of their lives; (c) nothing varied with the hour, so
> four residents never synchronised, and **contention in a building is about synchrony rather than
> utilisation** (four people failing to share one bathroom is a *morning*); (d) **the argmax had no
> floor**, so a resident always had exactly one best thing to do and always did it however bad it was,
> which is why (b) and (c) each did nothing alone. The hour was competing against nothing.
> (b) is the `D` key, (c)+(d) the `R` key, **both default OFF**. Together: contention 3.3% → 16.2% of
> frames, two residents heading for the same thing 6.0% → 22.7%, beds empty 06:00–13:00 and full
> 18:00–21:00 instead of flat across 24 hours. Two dead ends are kept in `offer.h` because both are
> the obvious thing to try: a rhythm on the DECAY RATE synchronises nobody (an 8-hour sleep resets a
> resident to the top of its own cycle, so each free-runs and drifts), and moving the hour into the
> BID was necessary but not sufficient without the floor.
> **▶ THE MAKER'S VERDICT IS IN (2026-08-13), and it reorders the lane: the cart reads as TOO MINIMAL
> and not meaningful enough, and D+R does not change that.** Reached with 242 assertions green,
> contention up from 3.3% to 16.2% of frames, and `canvas-diff` + `ui-audit` both clean, which is
> exactly the ADR-0022 failure mode: it cleared the verifiable half of the bar and not the other one.
> **The diagnosis, from reading real frames rather than the numbers.** (1) In a game about people
> sharing a building, the PEOPLE are the least visible thing on screen: one-tile blobs in the same
> colour family as the furniture and the floor, no rim, no contrast, so they read as more furniture,
> and everything a viewer knows comes from the text band. The picture is an architectural diagram with
> a caption. (2) **Nothing can go wrong** — `econ.h` says so in its own header ("no eviction, no game
> over, no score, no consequence") and arrears is "A RECORD, not a punishment" that nothing reads. And
> there is no MEMORY, so every minute is independent, nothing accumulates into a nameable situation,
> and no mess is anybody's fault. Both were principled deferrals (§8a), and principled deferrals add
> up to a cart with no stakes. **So D+R made the simulation more truthful than the picture can
> express, which is backwards for a cart whose thesis is that the sim reports whether your space
> works.**
> **▶ THE RENDERING WAS REPLACED (2026-08-13, later the same day), and it answers half the verdict.**
> Decided in a probe rather than argued about: `polyroom` builds the same flat twice — low-poly
> triangles against the baked voxel sprites, one key apart — and the maker picked low-poly with real
> hex palette ramps. That is now LANDED in `runtime/tenement/art.h`, with the sprite view kept on `V`
> so the comparison stays checkable. What shipped: a **depth buffer** instead of a painter's sort
> (draw order stops mattering, and it let subdivision be deleted, which paid for it — 3.02ms with,
> 2.28ms without); **free orbit + tilt** on the arrows, polygon-view only; **per-household colour**;
> and **three poses** where there had been one and a half — stand, **sit** (the contract had said
> `TN_POSE_SIT` since day one and only the art was missing, so a resident on the sofa had always been
> drawn standing on the backrest) and **work** (arms out, aimed at the object, which forced the
> instance rotation a symmetric figure had let us skip). Gates held throughout: **spec 242/242,
> ui-audit clean, canvas-diff 0px**, and the projection was left untouched so `build.h`'s
> click-to-tile inverse still lands where the player aimed.
>
> **▶ THE ITEM ECONOMY WAS SWITCHED ON (2026-08-14), and it was two lines.** `store.h` had shipped a
> full fetch/haul/put loop over BFS routes with thirty assertions, and NONE of it had ever run:
> `work.h`'s `tnk_deliver` minted a good, sold it at the machine and DELETED it in the same breath,
> under a comment reading "WHEN store.h CAN HAUL, DELETE THE TWO LINES BELOW". So no item ever
> persisted, `TnAgent.carrying` was never once non-negative in a running game, and `TN_ACT_HAUL` was a
> state nothing could enter. Deleting them, plus **one offer row** (the wardrobe also accepts
> `TN_STORE_GOODS` — which `store.h`'s own catalogue had predicted as "a missing table row in offer.h,
> not a missing code path") and **a BUYER in `econ.h`** that calls each day and takes only goods that
> are SHELVED, turned the whole chain on: made → carried → hauled → shelved → bought. `tn_sell` is
> still the one seam; only its caller moved.
> **THE POINT IS WHAT STORAGE NOW COSTS YOU.** A household with no cupboard, or a full one, keeps
> working and stops earning — `store.h` drops the bolt where it stands and the buyer only wants
> shelved goods, so the evidence piles up in the hall where you can see it. Nobody wrote that rule.
> It is the first thing in this sim that can go badly for you, which moves the "let something be LOST"
> conversation off zero (work.h case **W5b** pins it). A second one fell out free: **W8 stopped being a
> known gap** — two households now ALTERNATE at the one loom, because the finisher walks off to shelve
> its good and the machine is free while it is gone. Turn-taking out of an errand, with nothing
> scoring fairness.
> **Also landed:** posture carries the WORST NEED (a slump — z-squash, shoulders un-flaring, a forward
> shear, plus hollowed skin — scaled by how bad it is); the notable moments happen ABOVE THE HEAD as
> 7px chips (`!` blocked · `…` nothing on offer · a crate for hauling) instead of scrolling past in the
> news line; and a **person_haul** pose in both views. **Gates: spec 249/249 (was 242, six rewritten +
> five new), ui-audit clean, canvas-diff 0px, 1.69ms avg / 2.33ms max (was 2.66 — no regression).**
> **THREE FINDINGS WORTH NOT REDISCOVERING.** (1) **Hygiene was unserveable**, so every resident read
> "filthy" from day one forever — meaning the worst need was the SAME need for everyone at every hour,
> and posture keyed to it would have been a permanently stooped building. Fixed as one row: the WC
> gained a weak slow cold tap, deliberately on the ONE shared fixture rather than a new washbasin, so
> it aims more traffic at the scarcity the design is about. (2) **`TN_OFFER_N` is a second place the
> offer count lives** and nothing checks it — adding the wardrobe's row without bumping its count cost
> a debugging round in which goods were made, hauled, and then dropped on the floor because
> `tn_find_store` could not see a cupboard sitting right there. (3) **The first cut of the slump moved
> 15 pixels across an 1800-frame run**, because it was tuned for max distress and residents rarely
> bottom out; the fix came from TRACING the distribution of the worst need (median 96, p25 44, 23%
> under 32 — the `w0..w3` watches in the cart's `DE_TRACE` block) and sizing the constants against it.
> **Counter-intuitive, already measured, do not re-derive:** a SHARPER day makes a QUIETER building
> (REST appeal amp 70 → 95 drops contention 16.2% → 12.2%), because residents pinned into one narrow
> window stop overlapping at its edges. The phase is also still wrong (they sleep late afternoon).
> **▶ NEXT ACTION.** (a) **A RIM ON THE RESIDENTS** — the top item now, and hauling made it matter
> more, since more people are in the hall at once; two adjacent residents of the same household still
> read as one blob, and with the depth buffer this is a per-pixel job rather than a sort problem.
> (b) **Unpark D/R and read it against the new consequence:** the old objection was "contention with no
> CONSEQUENCE is just traffic", and there IS one now, so run D+R long and see whether contention costs
> anybody their income. (c) Then eviction / traces, the real stakes question.
> **Hot files:** `runtime/tenement/work.h` (`tnk_deliver`, the flip), `runtime/tenement/econ.h` (the
> buyer, `TNE_TRADE_*`), `runtime/tenement/offer.h` (the score, `TN_APPEAL`, `TN_BORED`, and the
> `TN_OFFERS`/`TN_OFFER_N` pair that must move together), `runtime/tenement/art.h` (the slump in
> `tnr_part`, the chips in `tnr_event_of`/`tnr_glyph`), `tools/carts/tenement.c` (the toggles + the
> `DE_TRACE` instrument — now traces the goods chain and the worst need too; re-run after any
> score/decay/offer-table change).
> **Resume-at:** [`design/tenement.md` → The building does not contend, and the four reasons are stacked](design/tenement.md#12-the-building-does-not-contend-and-the-four-reasons-are-stacked),
> plus the cart's own punch list, `node tools/cart-todos.js tenement` (the rim first).

> **▶ ACTIVE THREAD (2026-08-18) — EXTERNAL CLOCK + the AUv3 on macOS: a cart can be slaved, and acidcandy is a GarageBand plug-in.**
> **▶▶ START HERE IN A FRESH SESSION (rewritten 2026-08-13 end-of-day). GOAL: a well-behaved macOS
> AUv3.** No iPad — macOS IS the target. Everything you need to act is in THIS block; the shipped history
> follows it, and inside that history the block headed `▼ superseded` is factually WRONG and kept only so
> the trail of how it was believed is readable.
>
> **✅ TWO GARAGEBAND TRACKS SOUND CORRECT (maker, 2026-08-14).** Defect (B) — the live defect this
> whole lane existed to fix, "load the plug-in on two tracks and the sound goes weird" — is CLOSED.
> Every piece of shared state that made two tracks one rack is gone: the engine, all 8 cart-land
> headers (keybed.h included — it was the last and the doc said "deferred" for a while after it
> landed), and the cart's own 198. Measured, not inferred.
>
> ⚠ **This lane used to quote "601 → 148 statics". Do not trust a static COUNT written in prose —
> run `node tools/engine-statics.js`.** That 148 was measured by a tool which silently dropped 30
> rows it could not attribute AND had never been pointed at `runtime/midi_input.h` at all. Both are
> fixed (2026-08-14); the tool now names anything it cannot place and exits nonzero.
>
> **✅ AND THE PICTURE IS PER-INSTANCE TOO (2026-08-14).** The framebuffer group moved as one unit —
> `fb_w`/`fb_h`/`de_sw`/`de_sh`, `sw_dst`/`sw_world_buf`, the `de_pres_*` seqlock and the whole
> `de_pend_*` block. **That is what made two open panels flicker:** both views pushed their own size
> into the ONE pending slot (last writer won, every frame), and both blitted the ONE published buffer,
> which alternated between the two engines' renders. `instance-check` now asserts each instance
> publishes its own frame, which it structurally could not before.
>
> **✅ VERIFIED BY THE MAKER (2026-08-14): two panels open at once behave, no flickering, and the
> iPad-view HOME toggle now works both ways.** That toggle had NEVER worked; two of its plausible
> causes were both fixed in this lane (the panel used to create its own second engine, and the canvas
> dimensions the toggle changes were process-global until today). **The per-instance lane has met its
> goal in sound AND picture.**
>
> **✅ ON THE IPAD (2026-08-14).** `device.sh` deploys to a modern iPadOS 26 device and the app runs
> and SOUNDS. Getting there took four build/deploy fixes (`device.sh` header has them) and one real
> engine bug: **the app ran TWO engines and played the wrong one** — CanvasView drew and touched one,
> AudioEngine rendered another, so a strummed cart was silent with a normal-looking screen. That is a
> regression from the per-instance work: `de_instance_create` used to return a SINGLETON, so it never
> mattered who asked; now every caller gets its own rack. Three host components were doing it. Grep
> `de_instance_create` before trusting any host — one engine per rack, created by whoever owns the
> rack, PASSED to everything else.
>
> **✅ THE iOS AUv3 PANEL WORKS IN GARAGEBAND ON THE IPAD (maker, 2026-08-14): it draws, and TAPS
> LAND WHERE YOU PRESS.** Both things I expected to be wrong on first contact — view sizing in the
> plug-in area, and touch coordinate mapping — were right. **That closes the arc: the same cart is a
> plug-in on macOS and on iPadOS, with a working panel on both.** Until today the iOS extension was
> audio-only `epiano` with no interface at all.
>
> ⚠ **And build a gate for the iOS app's object graph.** Nothing in the repo instantiates it, which
> is exactly why two of the three double-engine bugs shipped: the AUv3 never had them, `instance-check`
> creates its own instances, and `refactor-guard` runs the desktop build, which has no `CanvasView`.
>
> **✅ SESSION STATE SHIPPED (2026-08-14) — the plug-in remembers, and the maker verified it in
> GarageBand** (toggled instruments + an added acid note came back after a save/reopen).
> `de_save_state`/`de_load_state` + `fullState`, serialising INTENT (the `ctx_log` config + cart slices
> marked `de_state_for_saved`), zlib-packed 589,032 → 3,560 bytes. Gated three ways:
> `bash tools/state-check/run.sh` (20, engine), `./au-transport-check --state` (12, the real
> out-of-process plug-in incl. the property-list round trip a host performs), `tools/lint-saved-state.js`.
>
> **✅ AND THE UPDATE CLIFF IS CLOSED (2026-08-14) — VERIFIED IN GARAGEBAND.** The maker saved a
> project, a field was APPENDED to `CartState` (the real "v1.1 added a knob"), the plug-in rebuilt, and
> the reopened project logged `STATE migrated … restored, new controls at their defaults` with the rack
> confirmed intact by eye. Session state MIGRATES. Format v2 hashes the
> SHAPE plus an ABI tag instead of folding each slice's SIZE into its identity — so a slice that GREW
> (you added a knob) is restored as a PREFIX with anything new left at its default, `== ` restores
> exactly, and a SHRUNK slice is refused. `de_load_state` returns **2** for a migrated load so it is
> reported, not silent; **v1 blobs still restore exactly** (they are in real saved projects). Gated by
> three paths in `tools/state-check/run.sh`, which builds the probe TWICE (`-DSC_GROWN` = the next
> release) and passes a blob between the builds — the cliff cannot be tested in one process.
> ⚠ A REORDER still cannot be caught at runtime (same size), which is why `tools/lint-saved-state.js`
> enforces **append-only** against a committed layout snapshot. Both halves are one design.
> Remaining leftovers (unverified `fullStateForDocument`, no `factoryPresets`, N racks sharing one
> on-disk `cart.blob`) are ranked in the [`STATUS.md`](STATUS.md) entry
> "AUv3 session state — SHIPPED, and four things left".
>
> **✅ MOD WHEEL + PITCH BEND SHIPPED (2026-08-14).** The plumbing was two lines (`case 0xB0:` keeping
> the channel nibble, and the `de_midi_cc` declaration `engine.h` never had) — and the gap was wider
> than the wheel: with no `0xB0` case, **every DAW automation lane was dead too**. Mapping, the maker's
> call: **mod wheel → the master DJ filter** (`mflt`) so one gesture moves all five machines, and
> **bend → the two 303 lines at ±2 semitones**, not the drums. ⚠ The trap that shaped it: the wheel
> RESTS AT 0 while `mflt`'s neutral is 0.5, so a naive 0..127→0..1 map would open every project fully
> lowpassed — indistinguishable from the day-old session-state restore being broken. 0 = neutral, and
> the wheel overrides rather than writes, so letting it back hands the knob over. Shown on the FLT knob
> in both faces via `host_knob_ext` (extracted from the external-clock `tempo_knob_ext`, which is now a
> wrapper), registering no widget so a drag cannot rotate a control the host holds. Gated through a
> REAL host: `./au-transport-check --wheel` in `ios/mac.sh` — peak 0.711 → 0.249 as the filter shuts,
> back to 0.667, with a no-op control first. Live MIDI state sits in `AcidScratch`, NOT `CartState`, so
> the saved layout did not move and projects saved earlier that day still load.
> **✅ AND THE KEYBED PLAYS IT (2026-08-15)** — the maker: *"there is a keybed but that isnt doing
> anything."* Same shape as the mod wheel one hop further along: the notes already reached the engine
> ring, and the CART called `midi_get()` zero times. Plumbing work zero; the job was deciding what a
> note MEANS on five machines, and four plausible answers collapse to **one axis** (is the PATTERN the
> instrument, or the VOICE?). Shipped the pattern row WHOLE so there is **no mode switch**: a drum face
> plays the KIT through the GM map the rack already sends on, anywhere else transposes both acid lines,
> routed by the focused face. Live MIDI step-record came free through the existing REC latch.
> **New engine flag `--midi-note`** pushes into the same ring an AUv3 feeds, because nothing could put
> a note into a headless run before — so this is gated with no DAW by
> `bash tools/midi-note-check/run.sh` (16 assertions, three in-gate negative controls, proven able to
> go red). Design + what is still open: [`design/host-midi-notes.md`](design/host-midi-notes.md).
>
> **✅ AND ROW 2 (2026-08-15, same day): the PITCH lens.** The open question was how to TOGGLE between
> the host-note behaviours, and the answer is that there are not four of them — **a note number names
> either a SOUND or a PITCH**, and everything falls out of that one binary. Off, the keyboard addresses
> the MACHINE (which drum / which key); on, it addresses the VOICE (play the line / one drum
> chromatically). That also explains why the 303's default is transpose instead of it being arbitrary,
> which the first framing never did. **It is the MPC's 16 LEVELS and Elektron's KEYBOARD mode**, so it
> adopts a control rather than inventing one. One `PTCH` latch (TAP=latch / HOLD=momentary, the
> MUT/REC grammar) in the PERF screen, **per machine** because the two kinds want opposite defaults.
> Engine side: `tr808_fire_semi` / `tr909_fire_semi`, since the TUNE knob is ±12 by construction and a
> keyboard is not; the old `_fire` are zero-offset wrappers, proven byte-identical. Gate is now 27
> assertions, mutation-tested twice. **▶ Left in that design: the `MidiEv` channel field** (all five
> machines addressable at once) and **`AUParameterTree`, which is the bigger lever** — the AU exposes
> ZERO parameters today, so no knob is automatable or recordable and the mod-wheel mapping was a
> workaround for that rather than a free choice.
>
> **✅ AND THE RACK IS AUTOMATABLE (2026-08-15) — `AUParameterTree`, which the plug-in never had.**
> The maker asked *"is any of this recordable or usable in a way? not really I guess"* and the answer
> was mostly YES — a host records the MIDI notes, which is ARRANGEMENT, the thing this rack never had
> — with one big NO: the AU exposed **zero parameters**, so nothing was automatable or recordable and
> the lane menu was empty. It also reframes the mod wheel: CC1 → master filter was a workaround for
> having no parameters, not a free choice.
> **The model is that the parameter IS the knob** — `param_bind(addr, &knob, name, lo, hi)` points at
> a float the cart already owns, so a host write and a finger drag land in the same place and there is
> no sync path to keep honest. The tree is built from what the CART declared, never a Swift table:
> swap the cart, the plug-in's parameters change with it. 21 exposed (the performance set), and the
> cart changed nothing but 21 bind calls.
> **Two traps, both paid for:** the drain must live in `loop_step` and NOT `de_frame` (the native loop
> bypasses `de_frame`, so it would work under a DAW and be invisible to every gate in the repo), and
> bind in `init()` BEFORE the autosave restore or the host is told last session's values are the
> factory defaults. **Addresses are FOREVER** — append, never renumber.
> Gated by `bash tools/param-check/run.sh` (engine half) and `./au-transport-check --params` (the real
> out-of-process plug-in). **▶ THAT DEFECT IS FIXED (2026-08-16):** host read-back returned the
> pre-write value because `de_param_set` only QUEUED, and the host reads back inside the same call;
> a `want` shadow in `param_ctx.h` closed it. Full record, including how it was actually found:
> [`design/host-parameters.md`](design/host-parameters.md) §The read-back bug.
>
> **▶ NEXT: the BEND is ungated** (both 303 lines are muted at boot, so a default render has nothing to
> bend — it needs the lines unmuted plus a pitch oracle, and an ear check; ⚠ `--midi-note` plus the
> `un303.script` trick in midi-note-check now make that easy, and note that `formant-check` is NOT a
> usable pitch oracle on an acid saw — it returns the ends of its own search range), and **CC74** is
> now nearly free for the focused machine's own cutoff. Full record + the rejected `varispeed`-on-bend
> idea: `node tools/cart-todos.js acidcandy`.
>
> **▼ superseded — the original next-action, kept for the trail:**
> **▶ NEXT ACTION (2026-08-14, the maker's call): WIRE THE HOST'S MOD WHEEL AND PITCH BEND.**
> GarageBand draws two controls above its keyboard — **modulatiewiel** (mod wheel = MIDI CC1) and
> **toonhoogte** (pitch bend) — on **both macOS and iPadOS**, and neither does anything. ⚠ **This one
> needs NO iPad**: mac GarageBand shows the same two controls and sends the same MIDI, so it is
> office work.
>
> **Where it is blocked, all three hops VERIFIED by reading the code (2026-08-14), not assumed:**
>
> | hop | pitch bend | mod wheel (CC1) |
> |---|---|---|
> | AU parses the host event list (`ios/AU/TinyjamAU.swift` ~L303) | ✅ `0xE0` → `de_midi_bend` | ❌ **`0xB0` is not parsed at all** — the switch handles only `0x90/0x80/0xE0` |
> | declared to Swift (`ios/Sources/engine.h` L68) | ✅ `de_midi_bend` | ❌ **not exported** (the engine HAS `de_midi_cc`, `runtime/midi_input.h:258` — only the declaration is missing) |
> | the cart reads it | ❌ `acidcandy` uses `midi_bend()` **zero** times | ❌ `midi_cc()` zero times |
>
> So bend already ARRIVES and the cart drops it on the floor; the mod wheel does not even reach the
> engine. The plumbing is two small edits (`case 0xB0:` + one line in `engine.h`); the cart side is
> where the actual thinking is.
>
> **THE REAL QUESTION IS MUSICAL, AND IT IS THE MAKER'S CALL:** on a five-machine rack, what do they
> move? The idiomatic acid answer is mod wheel → **filter cutoff** and bend → **pitch of the live 303
> line**, but which machine has focus, and whether a mod wheel should ride a knob the panel also shows
> (and visibly move it), is a design decision, not a wiring one. Cart API is ready: `midi_bend()`
> (-8192..8191), `midi_cc(ch, cc)`, `midi_cc_get()` — `node tools/api.js midi`. **Worked examples to
> copy:** `tools/carts/martenot.c` and `tools/carts/miditest.c` both read `midi_bend()`.
>
> **Gate it with** `zsh tools/midi-check/run.sh` (phase B covers CC input including channel isolation)
> and the AUv3 host test — `ios/Tests/AUHostTests.swift` already drives `scheduleMIDIEventBlock` for
> note-on, so it is the natural place to assert a CC and a bend arrive. ⚠ Run its tests with
> `-derivedDataPath build-test` (see `ios/README.md`, or the app crashes at next launch).
>
> **▶ THEN pick from [`design/per-instance-remaining.md`](design/per-instance-remaining.md);
> nothing there blocks two racks any more.** Ranked by impact:
> *(⚠ Which of these need the iPad: NONE. Every item below is Mac work — the mac AUv3 has the same
> panel, the same two host controls, and the same session-state gap. The iPad is only needed to
> re-verify iPadOS-specific behaviour after a change.)*
>
> 1. **Session state / `fullState`** — a reopened DAW project starts every rack at defaults, silently.
>    The biggest thing between this and a plug-in someone would keep, and it shows up the first time
>    anybody saves (the shared-engine defect needed two instances to appear). It is now UNBLOCKED:
>    the state it would serialise is finally per-instance, which is exactly why it could not be done
>    before. Filed with its route in [`STATUS.md`](STATUS.md) ("The AUv3 plug-in has NO session
>    state"); what gets serialised is INTENT, not the context struct — see
>    [`design/engine-instance-seam.md`](design/engine-instance-seam.md).
> 2. **N racks still share one `cart.sav`.** `de_set_save_dir` reaches the right instance now, but
>    nothing gives them distinct DIRECTORIES and the host must choose them. Last-writer-wins at FILE
>    granularity, not per key.
> 3. **No gate runs two instances on two THREADS** — which is exactly what a DAW does.
>    `present-race-check` covers one.
> 4. **The `--panel` gate's OBSERVATION is broken**: it cannot tell an orphaned panel from one that
>    was never opened, yet its headline asserts the first. Fix what it watches before trusting it.
>
> ⚠ **A LATENT BUG THIS TURNED UP, worth knowing before touching any cart's canvas:** `face.h` and 7
> carts declared their own `extern void de_resize(int, int)` against a function that has taken a
> `DeInstance *` since the seam landed. Undefined behaviour no compiler can see across translation
> units, harmless only while the parameter was `(void)in` — the moment `studio.c` dereferenced it, the
> cart crashed with `in = 0xa7`, which is 167, which was the canvas width it had asked for. The
> operation is now a real API, **`canvas_resize(w, h)`** in `studio.h`. Do not re-hand-roll an extern
> for an engine seam; if a cart needs one, the seam is missing an API.
>
> **STATE: it works, except for one thing.** The plug-in plays, follows the host's tempo and transport,
> survives a whole song, converts to any host sample rate, shows our own panel, and the panel is attached
> to the audio unit you can hear (confirmed in GarageBand by the maker). Seven gates in `bash ios/mac.sh`,
> 31 assertions, green. **Defect (B) — two tracks share one engine — was this lane's whole subject:**
> load the plug-in on two GarageBand tracks and "the sound goes weird", because both audio units are in
> ONE extension process and engine state was process-global, so both render blocks pushed
> `de_sync_position` into the same engine and both signalled the one frame worker. The rack was driven
> twice per host buffer by two transports. **State is now per-instance end to end (engine, cart-land
> headers, and the cart's own 198), so this is believed FIXED — but two tracks have not been played
> yet.** Until they have, treat it as unverified rather than shipped.
>
> **▶ THE JOB THIS LANE DID: give the engine PER-INSTANCE STATE — a context struct. THIS IS THE ROUTE.**
> It is what the platform expects: an AUv3 is designed to be instantiated many times in one process, and
> in Apple's own samples the DSP state lives in a per-instance kernel object owned by the audio unit.
> There is no "one engine per process" allowance. **The non-conforming part is OUR engine**, which keeps
> state in file-scope globals and is therefore a singleton by construction — fine for a fantasy console,
> awkward for a plug-in. Every other option on this page is a workaround for that.
>
> ⚠ **TWO earlier figures here were both wrong, in opposite directions.** "~204 statics" (which once
> said DO NOT DO THIS) was never looked at. The 91/109 that replaced it came from
> `grep -E '^static [^(]*;$'`, which requires the declaration to END at the semicolon — so it dropped
> every declaration carrying a trailing `// comment`, i.e. most of them. **It undercounted by 2.7×.**
> The real figures, from the compiler's own AST, and now a command you can re-run at any point to see
> the refactor's progress — `node tools/engine-statics.js` (`--quiet` · `--check` = 13 known answers):
>
> | | mutable file-scope statics | NON-ZERO initialisers | function-local statics |
> |---|---|---|---|
> | `runtime/sound.h` | **293** | 29 | 3 |
> | `runtime/studio.c` | **213** | 40 | 2 |
> | `runtime/sync.h` | 21 | 1 | 0 |
> | `runtime/mic.h` | 11 | 1 | 2 |
> | `runtime/midi_output.h` | 7 | 0 | 0 |
> | **TOTAL** | **545** | **71** | **7** |
>
> Cross-checked independently against the linker's view: `nm -m` on a compiled `studio.o` reports **639**
> non-external mutable data symbols for the whole TU (the extra ~90 are the vendored headers, `stb_image`
> and friends).
>
> **It is still days rather than a project — but for a reason that had not been measured until now.**
> · **The call sites do not change.** Move the declarations into one struct, add `#define name (ctx->name)`
>   per member, and every existing reference keeps compiling untouched. The hand work is only the **71**
>   non-zero initialisers, which become an init function — the other 474 are zero/NULL/absent and come
>   free from a calloc.
> · **The thing that could have made this miserable is measured at nearly zero: `#define` COLLISIONS.**
>   Across the entire translation unit only **2** static names are also used as a local or a parameter —
>   `voices` (a parameter in `sound.h`) and `palette` (inside `stb_image.h`) — and only **7**
>   function-local statics exist, which are the ones a `#define` cannot fix because the declaration
>   itself has to move. `engine-statics.js` prints both lists; check it is still ~0 before each file.
> · **The oracle is BYTE-EXACT, and that is the whole reason this is safe.** A pure state move MUST
>   produce identical output, and this repo can prove it: `node tools/tune-check.js --quiet`, the golden
>   WAVs, `canvas-diff --bytecheck`, `node tools/spec.js`, `tools/det-probes/run.sh`. Any semantic slip
>   shows up as a changed sha. **Treat a non-identical render as a bug in the refactor, never as "close
>   enough".** (An earlier draft listed these gates as a COST of the refactor. They are the safety net.)
> · **Carts come along for free IF they use `de_state()`** — *"a zero-filled block the engine owns; put
>   your whole cart state in it"*, already the documented idiom (`STATE {…}` / `S->x`, and it survives a
>   hot-reload). Put that block inside the engine context and a cart's state duplicates with it. One
>   mechanism covers engine and cart. acidcandy's 20 mutable statics move into `STATE`.
>
> **▶ THE ONE DESIGN DECISION TO SETTLE BEFORE WRITING ANY CODE: how does `ctx` get into scope?**
> The `#define name (ctx->name)` trick needs an in-scope `ctx` in all **304** functions in `sound.h`
> (and 303 in `studio.c`). Three shapes, and they are NOT equivalent:
> · **(a) a plain global pointer** — `static DeSound *snd_ctx;` + `#define echo_fb (snd_ctx->echo_fb)`.
>   Smallest possible diff, zero call-site changes. **But it is a data race by construction:** AUv3
>   permits a host to render two audio units on two threads at once, and both would be writing the one
>   "current instance" pointer. This just moves the shared-state bug from the engine to the pointer.
> · **(b) the same pointer, `_Thread_local`.** Identical diff, race-free: each thread (each render
>   thread, the frame worker) sets its own current context on entry. Costs a TLS load per access —
>   which needs measuring in the per-sample DSP loops, not assuming.
> · **(c) an explicit `ctx` first parameter.** The textbook answer, race-free, compiler-checked.
>   Measured edit surface: `sound.h` = **335 signatures + 707 call sites**; whole engine = 749 + 1494.
>   ⚠ It does **NOT** mean rewriting the variable references — `#define echo_fb (ctx->echo_fb)` works
>   the same whether `ctx` is a global or a parameter, so the thousands of uses are untouched either
>   way. An earlier draft of this block said otherwise and made (c) look far worse than it is.
>
> **MEASURED, 2026-08-13 — `bash tools/tls-spike/run.sh`** (a loop shaped like the real per-sample
> block, the same DSP text compiled all three ways so the delta IS the mechanism):
>
> | | ns/sample | vs today | verdict |
> |---|---|---|---|
> | plain statics (today) | 10.51 | — | |
> | **(b)** thread-local | 15.50 | **+47%** | **+0.83 ns per function entry** |
> | **(c)** parameter | 10.60 | +0.8% | **free** |
>
> · **(c) costs nothing at runtime.** That was not obvious and is now measured.
> · **(b)'s cost is entirely per-FUNCTION-ENTRY, not per access** — clang hoists the lookup out of
>   loops. When the stages are small enough to inline it disappears completely (all three within
>   0.1%). So the transferable number is **+0.83 ns per opaque call**: at N function entries per
>   sample it costs N × 0.0037% of a core, i.e. ~0.4% at N=100. Real, bounded, probably affordable.
> · **The stronger argument is not speed, it is that (c) is COMPILER-CHECKED.** Miss a call site and
>   it will not build. Under (b) everything compiles regardless, and a thread that never set the
>   pointer reads a NULL or — far worse — another instance's state, silently. That is the same class
>   of bug this whole refactor exists to remove.
> · **A boundary mechanism is needed either way**: the public API (`note_on` etc.) is called by carts
>   and cannot grow a parameter without changing every cart, so a thread-local lives at the door
>   regardless. The real question is only whether it stops there or goes all the way down.
>
> **Recommended: the HYBRID.** Thread-local at the public boundary; explicit `ctx` for the per-sample
> DSP functions, which is where 100% of (b)'s cost lives and where a silent wrong-instance read would
> be least visible. A few dozen functions, not 749. And the choice is **reversible per function**,
> because member access goes through the same macro either way — so start small, let the profiler
> (`profiler_request`) name the functions to promote, and let the byte-exact oracle prove each step.
>
> **✅ STEP 0 IS DONE — the guardrail exists: `node tools/refactor-guard.js`** (2026-08-13). A pure
> state move must produce byte-identical output, and this is the one command that says so: six probe
> carts fingerprinted across audio, frames and the `watch()` trace, per-chunk, so a failure names
> WHERE it started rather than only that a hash moved. The baseline is committed
> (`tools/refactor-guard-baseline.json`) and is green at HEAD. **Run it after every step; a red is a
> bug in the refactor, never a new baseline.** Three things it already taught, all of which apply to
> the work ahead: **(1)** two of the first six probes were VACUOUS (`omnichord` renders silence
> headless, `epiano`'s watched values never move) — a probe that proves nothing passes forever, which
> is why liveness assertions are part of the gate. **(2)** `acidcandy` alone is not enough: when the
> control perturbed the pan law, the other three audio probes went red and acidcandy did not, because
> it sets that value explicitly. **(3)** Two perturbation attempts printed a confident green while
> changing nothing (a `sed` that missed, and an edit to `sound_master_gain`, which is dead code on the
> native path under `#ifdef DE_AUDIO_WORKLET`). **Assert your perturbation landed before believing any
> verdict** — that is the same failure that cost yesterday three hours.
>
> **✅ THE CLASSIFICATION IS DONE — [`design/engine-context.md`](design/engine-context.md) +
> [`tools/ctx-classification.json`](../tools/ctx-classification.json)** (2026-08-13). Which statics must
> NOT simply become per-instance members, from three parallel read-only audits of `sound.h`. Default is
> per-instance (250+ of 293); only exceptions are recorded. **Read it before generating anything.** The
> four findings that justify it — none of which a byte-exact gate can see, because a single-instance run
> is identical either way: **(1)** `lfo_seed_ctr` (5822, function-local) is the `--det` reproducibility
> seed; shared, two instances BOTH lose determinism and `refactor-guard` stays green through it.
> **(2)** the scope + record rings are **public cart API**, not debug taps, despite sitting right after
> the WAV capture — calling them harness would have deleted a shipping feature. **(3)** several
> "flags" (`drop_used`, `vari_used`, `fxmod_any`, `shim_next`) are live DSP gates and pool cursors, not
> telemetry. **(4)** `atomic_int` does NOT mean shared: the request ring's SPSC invariant BREAKS if two
> instances share it. Also settled: the struct's **type-hoist** works (transitive closure = 14 types +
> 18 macros, compiles clean, tested on a copy), and **~3.4 MB of the 6.2 MB is lazy-allocatable**.
> ⚠ **Four open questions are logged in the JSON**, each with what breaks if guessed. None block the
> first batch. The sharpest is the MIC PATH: if host audio is process-wide, a naive per-instance
> `extin_on` means the capture thread reads whichever copy the linker picked and the mic is silently
> dead on every other instance.
>
> **✅ `sound.h` IS DONE — 327 of 340 statics moved, byte-identical (2026-08-13).** `node
> tools/ctx-gen.js --write` put them in `runtime/sound_ctx.h`'s `DeSound`; `engine-statics` reads
> **13** for that file and **every one is a recorded decision** (1 shared · 5 harness · 5 deferred ·
> 2 dead-weight), not a leftover. Green on `refactor-guard` (6/6), `spec.js` (1986/0), soundcheck,
> tune-check, dc-check, level-check, `build-all` (580/580), `build-nr`. Pure because the default
> instance is a `static` with DESIGNATED INITIALISERS — values still set at link time, no init
> function, no init-order risk; step B allocates by copying that template. **Next: `studio.c` (222).**
>
> ⚠ **FIVE silent failures on the way, every one of which produced a confident green. Read these
> before touching `studio.c` — three of them will recur there.**
> 1. The generated include landed inside `#if defined(__SSE__)`, so on arm64 it was never included.
> 2. **The probe passed four times without compiling the generated file at all** — a quoted
>    `#include "sound.h"` resolves relative to `studio.c`'s OWN directory before any `-I`. It now
>    carries an `#error` sentinel that proves it got there.
> 3. The generator swept in `sound_synth_mode` + the `extin_*` mic group because they were logged as
>    *open questions* rather than exclusions → the **`defer`** group: no decision = do not move.
> 4. **The collision check never looked at struct FIELDS.** The preprocessor does not know about
>    `->`, so `ins->rvb_tank` became `ins->(de_snd->rvb_tank)`. **`band_x/y/w/h` and `palette` are
>    the same trap waiting in `studio.c`** — rename one side first.
> 5. **`engine-statics` mis-attributed 47 of sound.h's statics** (clang names a file only when it
>    CHANGES; a stale cursor put `sound_bpm` in `stdbool.h`), so a batch reported success while
>    leaving four behind. It now verifies each row against the source, and **ctx-gen refuses to run
>    unless every static is either moved or explicitly skipped.** That accounting invariant is the
>    only thing that catches a silent drop — `refactor-guard` cannot, because a variable that did not
>    move cannot change the output.
>
> **✅ THREE OF THE FOUR OPEN QUESTIONS ARE CLOSED (2026-08-13), by reading the code.**
> · **MIC PATH — not applicable, leave `extin_*` SHARED.** The plug-in has no mic path at all (the mic
>   host is the STANDALONE app's `ios/Sources/AudioEngine.swift`; `ios/AU/TinyjamAU.swift` never
>   touches it). And it could not be per-instance anyway: we are **`aumu`, an instrument**, so the host
>   hands us no audio and there IS no per-instance input bus — and `mic.h` is device-free by design, so
>   the engine cannot open N capture devices. If it ever arrives: shared ring + per-instance read
>   cursor + `extin_on` as a REFCOUNT, never a bool. ⚠ The product question behind it, unanswered and
>   NOT a refactor decision: should the rack also PROCESS host audio (an `aufx` effect)?
> · **`sound_synth_mode` — SHARED, closed.** `de_init` sets it true unconditionally (studio.c:3015);
>   the AUv3 build has no device stream and `sound_stream` is dead code there.
> · **The four diagnostic counters — PER-INSTANCE, decided** (already moved; low stakes).
> · **STILL OPEN, not blocking: cart switching.** May two instances hold DIFFERENT carts? The 288 KB
>   `ctx_log` already moved per-instance assuming yes; if cart choice belongs to the umbrella app it
>   belongs to the shell. Settle it before the memory-trimming pass.
>
> **▶ NEXT IS `studio.c` — AND IT IS NOT A REPEAT OF `sound.h`. Read this before generating.**
> Its classification is done (123 exceptions vs sound.h's 13 — see
> [`design/engine-context.md`](design/engine-context.md) and the `studio_c` key in the JSON) and the
> two macro collisions are already cleared. But:
> · **THE PLATFORM SEAM HAS NO INSTANCE ARGUMENT.** The AUv3 runs ONE process-wide frame worker (its
>   own comment says so) and calls `de_frame(t)`, `de_resize`, `de_copy_frame`, `de_set_safe_area`,
>   `de_set_backing_scale`, `de_audio_render`, `de_set_save_dir` with no instance parameter. Once
>   studio.c is per-instance, **one `de_frame()` advances ONE rack and the others freeze.** ⚠ The macro
>   move COMPILES and passes every byte-exact gate while leaving this broken — `refactor-guard` runs a
>   single instance. Treat the mechanical pass as a prerequisite, never as completion.
> · **A THREAD-LOCAL POINTER DOES NOT WORK HERE** (a correction to the sound.h shape): the same
>   instance is touched from THREE threads — UI thread resizes, the frame worker draws, the XPC/view
>   thread copies the frame — and one worker serves many instances. The seam must take an EXPLICIT
>   handle. And do NOT use a mutable global "current instance": that is the exact race the
>   pending/seqlock machinery exists to prevent, one level up.
> · **`crash_handler` cannot reach a context** — it is registered with the OS and reads
>   `watches`/`watch_count`; a signal handler takes no argument. Design it, do not macro it.
> · **`save_dir` is already a live defect**: N instances share one `cart.sav`/`cart.kv`, and because
>   the mirrors are written back WHOLE it is last-writer-wins at FILE granularity, not per key.
> · Also logged: `colorkey()` destroys and rebuilds a SHARED GPU texture from a cart API;
>   `fp_cache`'s key omits the palette; and an unrelated latent `web_px` overflow after a canvas grow.
>
> **▶▶ WHAT IS LEFT, ON ONE PAGE: [`design/per-instance-remaining.md`](design/per-instance-remaining.md).**
> Read that first — it lists every remaining item in the order to do them, separates "blocks two
> racks" from "correctness gap" from "found along the way", and records what is DONE so nobody
> redoes it. Live progress number: `node tools/engine-statics.js` — and read it from the tool, not
> from here: every number this doc has quoted has been overtaken, and until 2026-08-14 the tool
> itself under-reported (30 dropped rows, `midi_input.h` unmeasured).
> **The state-sharing blockers are now CLEARED**; what remains is the shared published FRAME plus a
> list of gaps to decide, not gates to pass.
>
> **✅ ALL 8 CART-LAND HEADERS ARE DONE (2026-08-14).** `ui` · `keybed` · `solo` · `gestures` ·
> `radio` · `tr808` · `cursor` · `drumkit` · `tr909` all declare their state once and fork: DEFAULT
> into the statics that were there (580/580 build, refactor-guard byte-identical) or into a
> per-instance context under `DE_CART_CTX`. Both paths compile-checked by `run-uictx.sh`.
>
> **✅ AND SO IS THE CART (2026-08-14) — the last thing two racks were sharing.** All **198** of
> `acidcandy`'s statics (168 file-scope + 30 that lived inside function bodies) are per-instance, the
> cart TU measures **0**, and the cart defines `DE_CART_CTX` so its headers fork too. A cart has no
> seam — `draw()`/`update()` take no argument — so it asks for its slice by ADDRESS through
> `de_state_for`, like the headers. Default path folds `de_cart->x` to `de_cart_default.x`, the same
> storage as before, so the other 552 carts are untouched.
> **THREE THINGS WORTH NOT REDISCOVERING.** (1) **Brace-counting to scope a local FAILS, silently and
> three times.** Braces in strings and one-line function bodies drift the count, and a local you miss
> rebinds to the static of the same name and still compiles. What worked: make the **compiler** the
> oracle — rename the DECLARATION, then every use that was the static errors as *undeclared* while
> every use bound to a local stays quiet. 93 uses in one pass, no judgement. That is the sixth
> confident wrong answer a regex over C has produced in this lane. (2) **Function-local statics must
> be HOISTED first** — a `#define` rewrites uses, and a declaration inside a body is not a use.
> Hoisting is semantically nothing. (3) **`build/cart.c` is whatever cart compiled LAST**, so
> `ctx-gen --target cart` and `engine-statics --tu build/cart.c` silently measure the wrong cart right
> after a `refactor-guard` run. Recompile the cart you mean before believing either.
> ⚠ **`ctx-gen` refuses to re-run** on a processed file, correctly — it would rebuild the context from
> the handful of statics that remain and discard the rest. To redo a target, restore from git first.
>
> **✅ STEP 4 IS UNDER WAY — ROUTE (b) CHOSEN, PROVEN ON `ui.h` (2026-08-14).** The maker picked the
> declared-seam route over build-time copies, because with ~20 AUv3 apps the plug-in build IS the
> product and a permanent gap between the code you read and the code that ships is a tax paid forever.
> · **`ui.h` now declares its state ONCE, as a `UI_STATE(X)` list, expanded two ways:** by DEFAULT
>   into exactly the statics that were there (all 553 carts byte-identical, 580/580 build,
>   refactor-guard green), and into a PER-INSTANCE context when a cart defines `DE_CART_CTX`. **No
>   call site in ui.h changed.**
> · **New engine API: `de_state_for(key, bytes)`** — a slice of the instance's own `de_state()` block,
>   keyed by the ADDRESS of a file-scope sentinel (unique per TU, so no slot numbers and no registry).
>   The arena lives INSIDE the per-instance block, so nothing new had to be made per-instance.
>   ⚠ **Never cache what it returns**: registering another key can grow the block and move every slice.
> · **Gated by `bash tools/instance-check/run-uictx.sh`**, which builds the probe TWICE and asserts
>   OPPOSITE things — default shared, opted-in not. Measured: A writes 7, B reads 7 (default) vs B
>   reads 0 (opted in).
> **NEXT: the same treatment for ~29 more cart-land headers, then the cart's own 120 statics.** Only
> then do two racks actually work — this slice proves the MECHANISM, not the product. The recipe for
> the next header is in [`design/engine-context.md`](design/engine-context.md) → "Doing the next header".
>
> **▶ STEP 4 IS A DECISION, NOT A TASK — MEASURED 2026-08-14, read before starting it.** The plan
> said "acidcandy's statics → `de_state()`" on a figure of ~20. It is **209 statics in the cart's
> TU**: 120 in the cart, 19 in `ui.h`, 10 across `tr808/tr909/cursor.h`, 60 unattributed. **The
> CART-LAND HEADERS hold state too** — `ui.h`'s widget table, the drum banks, the cursor — and they
> are included by **553 carts**, so making one cart multi-instance-safe is not a local change.
> Three routes, costed in [`design/engine-instance-seam.md`](design/engine-instance-seam.md):
> **(a)** generate context-ified header COPIES for the plug-in build only (recommended — the AU build
> already stages `build/cart.c`, so no blast radius); **(b)** move the declarations in the shared
> headers, one mechanism for all 553 but it changes what a cart IS; **(c)** ship one rack per project,
> the honest single-instance fallback, keeping the engine work for the editor/offline/test cases it
> already unlocks. **Decide before writing: (a) and (b) are the same work aimed differently, and only
> (a) is cheap to undo.**
>
> **✅ VERIFIED IN GARAGEBAND BY THE MAKER (2026-08-14): ONE TRACK IS CLEAN.** *"one track works fine
> now, panel is stable"* — no regression from the whole per-instance refactor, which is the result
> that mattered. Two findings from that session, both now fixed or filed:
> · **FIXED — a hosted panel was booting its OWN engine.** Step 1 made `CanvasView` call
>   `de_instance_create` unconditionally, which it never used to do, so every open panel started a
>   second engine and ticked it from the display link. With the cart's state shared, two tracks with
>   panels open meant up to FOUR engines driving one sequencer: the maker saw a flickering panel, a
>   play button toggling by itself, and "wide and slow" audio. A hosted view now creates nothing; the
>   audio unit hands it the engine that makes the sound (`TinyjamAUViewController.connectPanel`).
> · **FILED, NOT A REGRESSION — the iPad-layout toggle has NEVER worked in GarageBand** (maker
>   confirmed). See STATUS.md. The CART is fine: `play.js acidcandy --resize …` reflows correctly in
>   both orientations, so it is the plug-in's resize path. Suspect: `de_pend*` is still process-wide
>   (it is inside `#ifdef DE_NO_RAYLIB`) while the layout state it feeds is per-instance.
> ⚠ **REVISED EXPECTATION for two tracks:** not "the second is silent". Both instances tick their own
> engine but share the CART's sequencer, so two tracks still interfere. Only the cart's statics →
> `de_state()` fixes that.
>
> **▶▶ THE ENGINE IS DONE; THE CART IS NOT. READ THIS BEFORE TESTING IN A DAW (2026-08-14).**
> Running `zsh ios/mac.sh` will show **6 of 7 gates green and the PANEL gate RED**, and two tracks
> will NOT both play. That is expected and the cause is known — it is not the engine.
> · **THE CART'S OWN STATE IS STILL SHARED.** `acidcandy` has **136 file-scope statics and calls
>   `de_state()` ZERO times**, so every instance shares one sequencer and only the engine it happens
>   to fire into makes sound. Driven ALONE each instance sounds correct (measured); interleaved, only
>   the first does. **This is plan step 4 ("the cart's statics → STATE") and it is CART work.**
>   ⚠ The handoff previously said acidcandy had ~20 statics. It has 136 — the same bad grep.
> · **A REAL CRASH was found and fixed** by giving `instance-check` resize coverage: `de_instance_create`
>   copied the context template AFTER instance 0 had booted and mutated it, so instance 2 inherited
>   LIVE HEAP POINTERS and two engines freed one framebuffer. malloc caught it in `de_ensure_fb`,
>   only under a host that RESIZES — which is why every earlier assertion passed while GarageBand
>   crashed. Fixed with a pristine snapshot taken before instance 0 boots.
> · **`sync.h` IS NOW PER-INSTANCE** (21 statics) and `de_sync_position` NAMES ITS INSTANCE. A push is
>   CONSUMED by whichever engine drains it, so while the transport was process-wide the first
>   instance swallowed the START edge and the rest joined mid-flow silent.
> · **`fb_w`/`fb_h`/`de_sw`/`de_sh` were taken BACK OUT of the context.** Their siblings
>   (`sw_cbuf`/`sw_dst`/`sw_world_buf`) are inside `#ifdef DE_NO_RAYLIB` and cannot move yet, and a
>   HALF-moved framebuffer group made `cls()` write `fb_w*fb_h` pixels into another instance's
>   smaller canvas. Coherence beats progress: they move when their siblings can.
>
> **✅ STEP 3 IS DONE — THE PLUG-IN GIVES EACH AUDIO UNIT ITS OWN ENGINE (2026-08-14).**
> `TinyjamAU` now holds `fileprivate let engine` created per instance, with its OWN frame worker,
> semaphore and frame counter. **`bootEngineOnce` is GONE** — it existed only because instances
> shared an engine. The canvas message channel gained an `owner`, so the panel blits the engine of
> the unit that handed it out rather than a process-wide one (that was the "panel shows an engine
> nobody can hear" bug, one level up). The stale ONE-ENGINE-PER-PROCESS comment block was rewritten:
> it confidently described behaviour that is no longer true.
> Verified: Mac Catalyst builds, `instance-check` PASS, refactor-guard 6/6, `build-nr` byte-identical,
> build-all 580/580.
> ⚠ **THE MAKER'S EYEBALL STEP IS NEXT AND NO GATE REPLACES IT:** `zsh ios/mac.sh`, then load the
> plug-in on TWO GarageBand tracks and confirm they are two racks rather than one. That is the defect
> this whole lane opened on ("the sound goes weird").
> ⚠ **Still process-wide: `de_sync_position`** (takes no instance). Mostly BENIGN — two tracks in one
> project share one host transport, so both push the same beat/tempo and the engines agree. It goes
> wrong only where two instances legitimately differ: an OFFLINE BOUNCE of one track while another
> plays realtime. Also still open: nothing runs two instances CONCURRENTLY on two threads
> (`present-race-check` covers one), and studio.c's 12 conditional + 15 function-local statics.
>
> **✅ THE ENGINE NOW RUNS N INDEPENDENT INSTANCES — PROVEN (2026-08-14).**
> `bash tools/instance-check/run.sh` creates two engines via `de_instance_create`, drives them with
> DIFFERENT transport, and their frames and audio DIFFER; the control (two fresh instances driven the
> SAME) comes back byte-identical, so the headline is the transport rather than noise. **This is the
> test `refactor-guard` structurally cannot be** — the guard runs one instance, so it proves a state
> move changed nothing and can never prove two instances are strangers.
> How: `DeInstance` owns a `DeSound` + `DeVideo`; `de_instance_create` calloc's and copies the
> generated templates (exactly a fresh process's state); the context pointers the ~800 macros expand
> through are now `_Thread_local`, set and restored by the seam. **Instance 0 keeps the templates**,
> which is why the desktop path, refactor-guard and every existing gate are untouched.
> ⚠ **WHAT IS STILL OWED — do not read more into that PASS than it earns:**
> · **`de_sync_position` is still PROCESS-WIDE** (takes no instance). Both engines read the same
>   transport push; they differ in the gate only because each was driven while it was being pushed.
> · **The Swift frame worker is still ONE per process**, so the plug-in cannot advance two racks yet
>   even though the engine now supports it. That plus per-instance `de_instance_create` in
>   `TinyjamAU` (deleting `bootEngineOnce`) is the remaining Swift half of step 3.
> · **Nothing runs two instances CONCURRENTLY on two threads.** `present-race-check` covers one.
>
> **✅ STEP 2 IS DONE — `studio.c`'s STATE IS PER-INSTANCE (2026-08-14).** 116 statics into `DeVideo`,
> byte-identical. `engine-statics` reads **13 for sound.h and 108 for studio.c** (from 340 and 222).
> Green on: refactor-guard 6/6, `build-nr` byte-identical, spec 1986/0, build-all 580/580, soundcheck,
> tune-check, canvas-golden, all three probes, and the Mac Catalyst plug-in builds.
> **Two generator lessons, both of which will recur:**
> · **One configuration's AST cannot rewrite conditionally-declared state.** studio.c forks on
>   `DE_NO_RAYLIB` throughout, with ~40 statics per side the other build never compiles. Moving one
>   gave a struct with a DUPLICATE member for one build and a MISSING member for the other. ctx-gen
>   now refuses anything inside a `#if` and reports it (12 such lines in studio.c — the seam, sw
>   rotation, netplay, desktop mic, CoreMIDI). **Those still need a home; they are the honest
>   remainder of studio.c**, along with 15 function-local statics.
> · **The probe was only building the easy half** — `DE_NO_RAYLIB` four ways, never Raylib. A batch
>   compiled four times, was applied, and failed in the build the probe never touched. It builds BOTH
>   renderers now, and says so loudly if raylib headers are missing instead of quietly testing less.
>
> **✅ STEP 1 IS DONE — THE SEAM TAKES A HANDLE (2026-08-14).** `DeInstance` +
> `de_instance_create`/`_destroy`, a scoped thread-local `de_cur`, and every seam function names its
> instance. All hosts migrated: `headless-nr`, the three probes, and the Swift (`CanvasView`,
> `AudioEngine`, `GameHost`, `TinyjamAU`). **There is still exactly ONE instance — nothing behaves
> differently yet**, which is the point: the shape changed where every existing gate still applies.
> Verified: `DE_NO_RAYLIB` render **byte-identical**, refactor-guard 6/6, spec 1986/0, build-all
> 580/580, all three probes + their negative controls green, Mac Catalyst plug-in builds.
> ⚠ **`de_process_init` deliberately did NOT land.** The shared-vs-per-instance split it formalises
> IS `studio.c`'s work; adding the name now would be a promise with nothing behind it.
> **Three things step 1 taught, all of which apply to step 2:**
> · **`refactor-guard` is BLIND to seam changes** — the seam is inside `#ifdef DE_NO_RAYLIB` and the
>   guard builds the Raylib path. `build-nr.sh` + the probes are the gate. Guard-green here proves
>   only that the CART-facing engine was untouched.
> · **`present-race-check` had been DEAD since `midi_output.h` landed**: it failed to LINK (CoreMIDI
>   missing from its line), which reads exactly like "not run". Fixed. A gate that cannot link cannot
>   fail, and nothing was watching.
> · **Engine-internal callers must not fake a handle.** `GetScreenWidth()` got `de_active_screen_w()`
>   rather than passing NULL — the handle pair is the HOST seam; internal code already runs inside an
>   instance.
>
> **✅ THE SEAM HANDLE IS DESIGNED — [`design/engine-instance-seam.md`](design/engine-instance-seam.md)
> (2026-08-13). BUILD IT BEFORE `studio.c`'s state move**, because the move is only correct if shaped
> for it. Five decisions, each with the rejected alternative and why:
> **(1)** an explicit opaque `DeInstance *` on every seam function — compiler-checked, so a missed
> call site does not build; **(2)** a thread-local that `de_frame`/`de_audio_render` SET AND RESTORE
> within the call, so the ~800 macro sites and the whole cart API need no change (this is not the
> global that was rejected: per-thread, scoped, never read outside a seam call); **(3)** `de_init`
> splits into `de_process_init` (fonts/shaders/sheet — the genuinely shared minority) and
> `de_instance_create` (memcpy of the generated default template), which is what `bootEngineOnce`
> becomes; **(4)** ONE FRAME WORKER PER INSTANCE — a semaphore signal carries no identity, so the
> shared worker cannot drive N racks; **(5)** `crash_handler` keeps a static "last instance to enter
> de_frame", deliberately, because a signal handler takes no argument.
> **Order: handle first on ONE instance (byte-identical) → `studio.c` state move → AUv3 to one
> instance per AU → gate with `engine-dylib-spike/probe.c`, whose assertions port unchanged and which
> is the test `refactor-guard` structurally cannot be.**
>
> **Order — do NOT take both engine files at once:**
> 1. **`runtime/sound.h` ALONE** (293 statics / 29 non-zero initialisers, self-contained, strongest
>    oracle in the repo). Prove the pattern here; if it does not work you have lost an afternoon on one
>    file with a clean bail-out instead of holding the whole engine broken.
> 2. **`runtime/studio.c`.**
> 3. **`acidcandy`'s statics → `STATE`.**
> 4. **Thread the context through `engine.h`** — `de_ctx_create()` / `de_ctx_select()`, or a ctx argument.
>    Then `TinyjamAU` makes one per instance, and `bootEngineOnce`'s idempotence goes away: it exists ONLY
>    because instances shared an engine.
> 5. **Three per-instance things the struct does NOT fix** (true for every route, found 2026-08-13): the
>    Swift-side frame worker (one `static` per process today) · the CoreMIDI virtual source (N instances
>    would publish N sources with the SAME name) · `save_bytes` (N instances, one `cart.blob` —
>    `de_set_save_dir` already exists to scope it).
> 6. **Gate it with a test that already exists.** `tools/engine-dylib-spike/probe.c` drives two engines
>    with different transport and asserts their frames differ, their audio differs, and — the control —
>    that a SHARED engine comes back byte-identical. Swap its `dlopen` for the context call and **the
>    assertions port unchanged, control included.** Plus `--panel`: a fixed build must never again print
>    *"instance N renders, this panel holds M"*.
>
> **Two real risks, neither fatal.** `#define` collisions with local variables of the same name — find
> them BEFORE starting, it is mechanical. And SCHEDULING: `sound.h` and `studio.c` are the two files
> parallel agents share, so CLAUDE.md's hot-files rule applies in full (targeted `Edit`s, never a
> full-file `Write`, and re-check your change survived the commit).
>
> **▶ SETTLED BY MEASUREMENT — do NOT re-derive any of these.**
> · **"Apple expects per-instance DSP state" is now SOURCED, not asserted** (checked 2026-08-13; it was
>   previously on one agent's word, which is why it is written down here). Two primary sources, both read:
>   **(1)** Apple's own shipped Xcode Audio Unit Extension template — on this Mac at
>   `/Applications/Xcode26_6.app/Contents/Developer/Library/Xcode/Templates/Project Templates/MultiPlatform/Application Extension/Audio Unit Extension.xctemplate/Instrument`
>   — declares `var kernel = ___DSPKernel()` as an **instance property of the `AUAudioUnit` subclass**,
>   holds every piece of DSP state (sample rate, gain, note envelope, musical-context block, bypass,
>   max frames) as **members of that kernel**, and contains **zero** file-scope statics
>   (`grep -rn '^static' <that dir>` returns nothing). **(2)** An Apple engineer on the developer forums
>   ([thread 65909](https://developer.apple.com/forums/thread/65909)) confirming the process model:
>   *"there's no way to force a new extension process for each instance of the same audio unit being
>   instantiated multiple times"*. So: one process is guaranteed, per-instance state is what Apple's own
>   code does, and there is no "one engine per process" allowance to appeal to. **The non-conforming part
>   is ours.** Do not re-open this.
> · **The panel is connected.** `PANEL CONNECTED — this panel's own audio unit is the one being rendered`,
>   in GarageBand. Defect (A) never existed; the finding that created it was a diagnostic whose
>   "connected" branch was unreachable by construction. The four-way fork it spawned (park / per-instance
>   state / parameter-bound UI / ship pixels over XPC) was answering a closed question — which is why
>   per-instance state is now the ONLY remaining work rather than one bet among four.
> · **Two AU instances always land in ONE process.** No host setting changes this, so there is no
>   configuration escape from (B), and no "it might be fine on another host".
> · **In-process AUv3 loading needs a NATIVE macOS code bundle.** `AudioComponentBundle` +
>   `factoryFunction` DO make a host dlopen your bundle — GarageBand said so while refusing the plug-in
>   outright: `incompatible platform (have 'MacCatalyst', need 'macOS')`. Ours is Catalyst because every
>   pixel of our UI is UIKit. In-process would cost an AppKit canvas view, the "second host view to
>   maintain forever" Catalyst was chosen to avoid. **Not needed for the struct route** — noted so nobody
>   re-opens it.
> · **The dylib fallback is proven** (`tools/engine-dylib-spike`, PASSES with a negative control): two
>   COPIES of one engine dylib are two dyld images, so every static duplicates with zero source changes,
>   and a sandboxed appex CAN dlopen a signed dylib from its own bundle. It CANNOT load one it wrote
>   itself — refused as *"library load disallowed by system policy"*, plus user-visible Gatekeeper MALWARE
>   dialogs — so that shape needs K pre-shipped signed copies.
>
> **⛔ DO NOT.**
> · **Reach for the dylib route first.** It works and it is measured, but it is a WORKAROUND for an engine
>   that should not be a singleton: K is a hard cap, it rests on dyld image-dedup behaviour rather than a
>   contract, and it is unusual enough to draw a question in review. Do the struct; keep this in your pocket.
>   ⚠ **Do NOT reuse the memory argument that used to sit in this bullet** ("~4.5 MB/engine"): it does not
>   discriminate, because the struct route costs the SAME. `size -m` on a compiled `studio.o` puts the
>   engine's mutable statics at **6.2 MB**, and a per-instance context is exactly that, calloc'd per
>   instance. What it does buy is control the dylib route cannot give: **83% of those bytes are 10
>   symbols** (`mic_rec` 1.4 MB · `grain_pool` 1 MB · the echo/varispeed buffers ~1.4 MB · `voices`
>   323 KB · plus two debug-only buffers), so per-instance cost can be cut by leaving the shared or
>   harness-only ones out of the context and allocating the big buffers lazily.
> · Point `AudioComponentBundle` at a Catalyst framework. It breaks the plug-in in every DAW while every
>   other gate stays green. `au-transport-check --loadable` (the FIRST gate in `mac.sh`) now catches it.
> · Copy a dylib at runtime and load it. It fails, and it accuses the user of running malware.
> · Trust an outcome without checking the mechanism's own error. This cost two wrong conclusions in one
>   day: `.loadInProcess` "was denied" (it was attempted, failed on the platform, then **silently fell
>   back**), and "the extension logged nothing" (`log` is a **zsh builtin** — use `/usr/bin/log`).
> · Trust a number in a handoff that has no command next to it. "~204 statics" and "acidcandy ~120" both
>   came from nowhere and between them nearly killed the right plan.
>
> **FALLBACKS, in order, if the struct pass hits something genuinely unexpected:**
> **(1) the dylib route** — measured, spike + oracle already written, see above. **(2) honest
> single-instance** — elect ONE instance to drive transport and the frame, so track 2 is a second window
> onto the SAME rack: coherent instead of garbled, nothing a reviewer blinks at, and a limitation you can
> state in the listing. It caps the plug-in at one rack per project, which is why it is last. ⚠ Note that
> **nothing forces this decision** — the panel works, so multi-instance is a QUALITY want, not a blocker.
>
> **Handy:** `/usr/bin/log stream --predicate 'eventMessage CONTAINS "[tinyjam] PANEL"'` gives the panel's
> live verdict in any host. Full arc, the results tables, and the twelve wrong turns:
> [`ios-plan.md → UNPARKED 2026-08-13`](design/ios-plan.md#-unparked-2026-08-13-later-the-same-day-the-panel-was-never-orphaned-and-the-diagnostic-that-said-it-was-could-not-have-said-anything-else).
>
> **▼ Everything from here down is SHIPPED HISTORY and the trail of how the above was reached.**
> Read it for the design points worth not re-deriving; nothing below is an open action.
>
> Started from "does Tiny Acid Jam do AUv3 or MIDI in?" (it did neither). **SHIPPED, both verified by the
> maker on real gear:** `runtime/sync.h` + the five `sync_*` API functions (an external clock a cart
> FOLLOWS — MIDI clock from Ableton over the IAC bus, *and* an AUv3 host's transport), and **acidcandy
> loaded as an Audio Unit in GarageBand on macOS**, stopping and tempo-following the host. `zsh ios/mac.sh`
> is the whole Mac build (Catalyst, own spec `ios/project-mac.yml`, own staging dirs, `auval` PASSES).
> **The design point worth not re-deriving:** one cart-facing API for three clock sources that arrive in
> TWO shapes — MIDI clock is *incremental* (measure the tempo, infer the transport), a host and Link are
> *absolute* (they state both). `sync_beats()` is the common currency and a cart DERIVES its step counter
> from it. That is why AUv3 transport needed **zero cart changes**: it was the other shape of the same seam.
> **▶ EXPORT IS POSTPONED — the maker redirected 2026-08-13 ("postpone the wav export, rather continue
> working on the plugin stuff, midi export all the jazz"). What shipped instead, that same session:
> the MIDI seam in BOTH directions.** `runtime/midi_output.h` — a CoreMIDI **virtual source** (the same
> call on macOS and iOS, so desktop and the phone→Ableton case are one implementation), seven
> `midi_send_note/cc/bend/clock/start/stop` + `midi_out_ready` functions in `studio.h`, **channel-first**
> — plus **MIDI CC *in***, the one dropped `else if` in `midi_input.h` that four docs kept calling "the
> cheapest missing piece" (`midi_cc(ch,cc)` polled + `midi_cc_get()` drained = the MIDI-learn primitive).
> Gated by `zsh tools/midi-check/run.sh` (BOTH directions, real CoreMIDI, second process, negative
> control). `midiout` is the demo cart. **The design point worth not re-deriving:** the maker asked
> whether a rack should send on "various channels" given acidcandy's four machines — the answer is
> **4 channels, not 4+27**, because a drum machine is ONE channel with its voices as GM **note numbers**
> (and both the 808 and 909 rosters land on exact GM homes, since GM's percussion map was modelled on
> these machines). That is what forced `ch` into every signature *before* it reached five places, and
> the same logic made CC-in channel-aware while notes stay omni. Full map + the still-open **slide
> encoding** question: [`midi-out.md` → The channel map](design/midi-out.md#the-channel-map-and-why-drum-voices-are-notes-not-channels).
> **✅ CONFIRMED IN A REAL DAW (GarageBand, the maker, 2026-08-13)** — the eyeball step no gate can
> do. `midiout` drove both an ePiano and a drum kit. Note for next time: only the SELECTED track
> monitors live MIDI, and GarageBand is NOT multi-timbral (every channel hits that one track's
> instrument), so channel ROUTING cannot be observed there — that needs Logic/Ableton. Hence `B`/`D`
> in the cart, to mute one part at a time.
> **That play-test found TWO defects the gate was structurally blind to, both now fixed:**
> **(1) zero-length drum notes** — on and off in the same frame (0.0ms on the wire), which is legal
> AND balanced, so the "every note-on has a note-off" check sat green while rewarding it. A DAW that
> RECORDS such a note normalises it on playback, so a take gains notes that were inaudible live —
> which is exactly how the maker noticed ("the recorded song sounds different"). Drums now hold
> ~50ms. **(2) stuck notes on exit**, which fix 1 exposed: a cart can quit on a frame where a note is
> held, and the off it would have sent next frame never happens. Fixed in the ENGINE
> (`midi_output.h` tracks live notes, releases them in `midi_output_shutdown`) because no cart should
> be *able* to leave a note droning in someone's DAW. **The transferable lesson: pair-counting is
> blind to defects that live in the TIMING.** The listener now timestamps every line and the gate
> asserts gate length ≥20ms.
> ⚠ **Nothing is wired to a RACK yet** — `acidcandy` neither sends nor reads CC, so none of this is
> reachable by a buyer. That is the next cart-side job, and it is where the slide decision gets made.
> **The one claim still unverified:** whether the beat LANDS correctly on a GM kit (kick on the
> downbeats, snare on 2 and 4, hats in eighths). The gate proves we send note 36 on ch 10; it cannot
> prove 36 *is* a kick in a real kit. Check that before wiring the rack.
>
> **⚠ A BUG THIS FOUND, fixed: a guard that was inert in exactly the runs it protected.**
> `sync_automated` — the flag stopping an automated run from consulting a real external clock — had its
> assignment sitting **inside `#ifdef DE_SPEC`**. Only `spec.js` defines that; `play.js`
> (`--headless`/`--script`/`--replay`) and the screenshot bake build with `-DDE_TRACE` alone, so the flag
> stayed 0 and those runs *did* read the real clock — the nondeterminism
> [`external-clock-sync.md`](design/external-clock-sync.md) documents as FIXED, still live, one `#ifdef`
> away from the fix. It hid because `--midi-clock` runs are unaffected (the synthetic branch wins first)
> and those are the runs the sync gates exercise. Moved out; both build modes recompile, spec still 104
> green. **The transferable lesson, and why the new gate carries a negative control:** a gate must
> include a case where the guard is the only thing that can produce the result.
>
> **Older next-action, still true — IN-APP EXPORT.** Nothing of ours can get a track out today, which is what ReBirth shipped *instead* of relying
> on a host, and it is the real answer to "recording it in GarageBand does nothing". Three pieces, only
> the first is cart work, full detail (incl. what `acidrack`'s existing "export" really is) at
> [`external-clock-sync.md` → what export would actually take](design/external-clock-sync.md#what-export-would-actually-take-and-what-acidrack-really-does):
> **(1)** promote the capture to the API — `capture_begin(path, secs)` in `studio.h` and the four
> places from CLAUDE.md's "Adding a new API function", so it stops depending on the debug harness's
> `.bake/wav_request` file and on the working directory (the engine side, `sound_wavcap_begin`, already
> exists and works — it is capped at 60s, which is a loop and not a track, so decide whether to raise
> it); **(2)** a writable destination via the `de_set_save_dir` seam the iOS host already supplies;
> **(3)** the last mile — a share sheet / Files hand-off in `ios/Sources/`, HOST work, which is what
> makes the file reachable at all inside a sandboxed container. Then wire the button in `acidcandy`
> (`acidrack` has the UI precedent: a WAV button in the MASTER strip that restarts the song from the top
> so the take covers the whole arrangement). ⚠ Gate it honestly — a capture is trivially assertable
> headlessly (render, then read the WAV back with `tools/wav-analyze.js`), so there is no excuse for
> shipping this one on a listen.
>
> **▶ THE STATE OF PLAY (2026-08-13, session closed here).** The plug-in RUNS in GarageBand: it plays,
> follows the host's tempo and transport, survives a whole song (the bar-33 wedge is gone), converts to
> any host sample rate, and shows our own panel. Five gates in `mac.sh` cover it. **But it is not
> honestly finished, and the reason is architectural — read
> [`ios-plan.md → THE OUT-OF-PROCESS WALL`](design/ios-plan.md#the-out-of-process-wall-the-open-fork-2026-08-13)
> BEFORE touching the view again.** GarageBand runs our UI and our audio in DIFFERENT PROCESSES, so a
> view that blits the engine's framebuffer is drawing a different engine than the one you hear. A live
> `sample` found three engines in the UI process (thread name `dreamengine.frame` is how you spot it).
> Booting is now idempotent (one engine + one shared worker per process), which stopped the corruption;
> the disconnect itself needs one of FOUR forks, and picking one is a product decision, not a cleanup:
> ship the app and park the plug-in · per-instance engine state (globals → a context struct) ·
> a parameter-bound UI (what commercial AUv3s do) · ship pixels over XPC. Do not start the last three
> without deciding which product the plug-in is for.
> **▶ THE FORK IS NOW MOSTLY DECIDED BY MEASUREMENT (2026-08-13, `ios/au-msgchannel-spike.swift`).**
> The cost that made "ship pixels" read as expensive was a guess; measured out-of-process, a **full
> 150KB canvas frame round-trips in 0.42ms** (~2300fps against the 20-30 a panel needs). It also
> kills the netplay-style alternative: payload grows 2400× while latency grows ~3×, so the channel is
> per-call-overhead-bound and shipping STATE buys nothing — while costing a determinism contract
> across two processes where drift silently draws the wrong rack. **Ship pixels.** Two seams it needs
> already exist and are TSan-gated (`de_copy_frame`, the input ring), so the remaining work is
> plumbing, not research. Caveats: the numbers are an ECHO (no engine work behind them), it is Mac
> Catalyst not iPad, and `AUMessageChannel` is Catalyst/iOS **16+** — an OS floor on the PLUG-IN only.
> Per-instance state is a SEPARATE want (two tracks = two racks), not a fix for the panel: two
> processes stay two processes. Scale if ever taken: ~204 engine statics (58 studio.c, 146 sound.h)
> plus each cart's own (acidcandy ~120); `de_switch_cart` does not help — it is a config-log replay
> that switches one cart at a time, not concurrent instances.
>
> **▼ superseded, kept for the trail (written earlier on 2026-08-13):**
> **TWO SEPARATE DEFECTS, do not conflate them:**
> **(A) THE PANEL IS ORPHANED.** It draws an engine nobody hears, and touches drive that same wrong
> engine. Measured in GarageBand: BOTH the message channel AND the parameter tree stayed inside the
> UI process (`PARAM writing 0.75 from UI pid 98759` → `PARAM observed … in pid 98759`;
> `PANEL TALKING TO ITSELF · channel engine pid 98759 · this UI process pid 98759`).
> **(B) TWO INSTANCES SHARE ONE ENGINE.** Maker-reported: load the plug-in on two GarageBand tracks
> and "it goes super weird". Cause is known and separate — engine state is process-global (~204
> file-scope statics in studio.c/sound.h, plus acidcandy's ~120), so two tracks are two front-ends
> fighting over one rack. Fixing (A) does NOT fix (B) and vice versa.
>
> **▶ THE LEAD FOR (A), and it is ordinary build config rather than research.** Diffing our extension
> against [bradhowes/LPF](https://github.com/bradhowes/LPF) (works in GarageBand + Logic on macOS):
> we match on `com.apple.AudioUnit-UI`, principal class, `sandboxSafe` — but LPF declares
> **`AudioComponentBundle` pointing at a separate FRAMEWORK holding the AU code** and a
> **`factoryFunction`**, and we declare neither properly (`ios/project-mac.yml`). Those two keys are
> what let the system load the AU's code as a bundle into another process = **in-process
> instantiation**, which is the case where `createAudioUnit` runs ONCE and `requestViewController`
> returns a controller holding THAT instance. It explains every symptom at once, and fits LPF's host
> code never setting an `audioUnit` property on the returned VC.
> ⚠ **Hypothesis, not measured** — four confident claims were wrong the same day (11 wrong turns are
> tabulated in the design doc; read them before trusting anything here).
> **DO:** factor the AU code into a framework · add `AudioComponentBundle` + `factoryFunction` ·
> rebuild (`zsh ios/mac.sh`, 5 gates) · load in GarageBand · read the ONE `[tinyjam] PANEL …` line.
> **DO NOT:** add more instrumentation to our own AU — the last three probes all said the same thing.
> If the lead fails, the next move is to build LPF itself and run the same pid probe on IT, to
> establish what a working one does before changing ours again.
>
> **Already in place, keep it:** `TinyjamCanvasChannel` (echo/nonce/frame) + `CanvasView.remoteFrame`,
> INERT (remoteFrame nil = the old path exactly, 5/5 `mac.sh` gates green), and the `[tinyjam] PANEL …`
> diagnostic that answers "is the panel connected?" in one Console line — it used to take `sample`ing
> a wedged host. The frame path itself is written and MEASURED (a real 160×100 frame crosses a process
> boundary in 0.32ms, byte-exact, non-black), so if in-process loading lands, the pixels are ready.
> A probe PARAMETER was deliberately REMOVED (a stray "Bridge Probe" shows in every host's automation
> list, and this app is on the store).
> Full arc + the 11 wrong turns:
> [`ios-plan.md` → PARKED 2026-08-13](design/ios-plan.md#-superseded-see-the-section-above--parked-2026-08-13-both-routes-measured-both-closed-as-configured--and-the-wrong-turns).
>
> **⛔ PARKED 2026-08-13 — both routes measured in GarageBand, both closed AS CONFIGURED.** The
> message channel AND the parameter tree (Apple's own supported route) each stayed inside the UI
> process: `PARAM writing 0.75 from UI pid 98759` → `PARAM observed … in pid 98759`, and
> `PANEL TALKING TO ITSELF · channel engine pid 98759 · this UI process pid 98759`. The view
> controller's AU is an orphan. ⚠ **NOT "AUv3 cannot do this"** — commercial AUv3s work through
> exactly that parameter mechanism, so the suspect is OUR configuration (the view controller is both
> principal class and factory). **Resume by (1) diffing against a KNOWN-WORKING AUv3 — Apple's
> AUv3FilterDemo or bradhowes/LPF — and running the same pid probe on THAT first, and (2) testing on
> iPad**, which is untested and has historically hosted the audio unit and the view in ONE process,
> so the problem may not exist there at all. The `[tinyjam] PANEL …` line is permanent, so (2) is a
> ten-minute check. The frame path + channel stay INERT (5/5 gates green) as the diagnostic; the probe
> PARAMETER was removed (a stray "Bridge Probe" would show in every host's automation list).
> **ELEVEN wrong turns from this stretch are written down in full** — they are the more useful
> artefact, and the pattern is that a measurement can be perfectly sound and still be of the wrong
> thing (the spike measured HOST→AU while the panel needs UI-extension→AU).
>
> **superseded — the earlier "option 4 is CLOSED" note (2026-08-13).** `PANEL TALKING TO ITSELF — channel
> engine pid 96523 · this UI process pid 96523`. The channel from the view controller's own AU loops
> back to the LOCAL instance; the UI extension makes its own `TinyjamAU` and no API hands it the
> rendering one. Apple's route for UI↔DSP is the PARAMETER TREE, which is why commercial AUv3s are
> built that way. **This overturns the "ship pixels, it's just plumbing" call made hours earlier** —
> the spike measured HOST→AU and the panel needs UI-extension→AU, a hop that does not exist. The
> transport numbers are real and were answering the wrong question. It also explains the free-running
> playhead: the panel's engine never sees host transport, so it boots playing=1 while the audible one
> sits stopped. **Live fork now: park (1) · parameter-bound UI (3, the only supported route, costs the
> pixel canvas in the plug-in) · or a hybrid syncing two engines through the parameter tree (keeps the
> canvas, carries silent determinism drift).** The channel + `CanvasView.remoteFrame` are KEPT and
> inert (5/5 gates green) for the one-line `[tinyjam] PANEL …` diagnostic, which now answers "is the
> panel connected?" in every case where it used to take sampling a wedged host.
>
> **▶ superseded — the LOOK described below has been done, and its answer is above (2026-08-13).** The panel is wired to pull its frame
> through the channel (`CanvasView.remoteFrame`, `TinyjamCanvasChannel` op=frame, measured 0.32ms for
> 160x100, byte-exact and non-black). **But the spike proved the HOST→AU hop, and the panel needs the
> UI-EXTENSION→AU hop, which is NOT the same**: `TinyjamAUViewController.createAudioUnit` builds a
> TinyjamAU *in the UI process*, and whether a channel taken from it reaches the RENDERING instance
> depends on the host wiring the view controller to its own proxy — AUv3 does not oblige it to.
> **Load it in GarageBand and read Console for `[tinyjam] panel channel live — engine nonce N pid P`.**
> Then compare that nonce with a host-side `./ios/build/au-msgchannel-spike` run. SAME nonce → the
> panel is genuinely connected and option 4 is done bar the input direction. DIFFERENT nonce → it is
> still two engines, this route is closed, and the parameter-bound UI (option 3) is the answer.
> One line of Console settles what previously took sampling a wedged host.
> Still open after that either way: INPUT back the other way (the ring exists), a TSan look at the
> view pulling at display rate, and iPad (this is all Mac Catalyst).
>
> Full table + the three corrections the spike made to its own author:
> [`ios-plan.md` → MEASURED 2026-08-13](design/ios-plan.md#-superseded-see-the-section-above--parked-2026-08-13-both-routes-measured-both-closed-as-configured--and-the-wrong-turns).
>
> **Smaller open items, all recorded, none started:** a HOVER seam (a Catalyst mouse-move is not a
> touch, so the cart only learns the pointer on click and `cursor.h` never sees a mouse — the pixel
> cursor is missing in the plug-in) · a RESET for the plug-in's saved rack (acidcandy persists banks to
> the extension's container, a silent state sticks with no recourse, and per-instance state belongs in
> the host's `fullState` anyway) · **VERIFY AN EXPORT/BOUNCE**: Share → Export Song to Disk exercises the
> OFFLINE path (frames run inline, deliberately), and a host that renders offline WITHOUT declaring
> `isRenderingOffline` would starve the sequencer — the headless gate's default mode is an offline render
> and passes, so this is unverified rather than suspected · recording a software-instrument track does
> nothing because acidcandy reads no MIDI (expected, not a bug: a 303 sequencer taking MIDI notes is its
> own design question).
>
> **⚠ TWO PRODUCT RISKS found by that research, neither fixed** (both in
> [`product-notes.md` → the trademark flag](design/product-notes.md#-the-rule-is-currently-broken-by-our-own-live-listing-found-2026-08-13)
> and the table below): **(1)** ReBirth was **pulled from the App Store in June 2017 after Roland
> claimed IP infringement** — so this is the documented cause of death of the app we are an homage to —
> while our own PAID listing, already pushed to ASC, is titled "**303** Groovebox" with `rebirth`, `338`
> and `tb303` in the keyword field, breaking a rule `product-notes.md` wrote down years earlier
> ("nothing Roland-named crosses a paywall"). The market's answer is finer than "avoid the numbers":
> Troublemaker uses "303/TB-303" freely in its DESCRIPTION (with a "this is not a 303" disclaimer, and
> never says Roland) while keeping its NAME and SUBTITLE clean — the marks stay out of the identity
> fields. Ours are in both. And the weakest item is `rebirth`/`338`: Reason Studios' marks, not
> descriptive of our sound at all, so no nominative-use defence. No tool enforces this — a trademark screen belongs in the `aso-*` pipeline.
> **(2)** `acidcandy` **cannot export a track**, and in-app export is what ReBirth shipped instead of
> relying on a host — which is the real answer to "recording it in GarageBand does nothing".
> The engine CAN capture its own output, but the only trigger is the debug harness's `.bake/wav_request`
> file, which does not exist in a shipped app — so nothing a buyer can reach exports today. A real one
> is three pieces (a `capture_*` API instead of a polled file · the app's Documents dir via
> `de_set_save_dir` · a share-sheet last mile in `ios/Sources/`), and only the first is cart work.
>
> **The outward surface in ONE table** (asked for 2026-08-13, was scattered over four docs):
> [`external-clock-sync.md` → The whole outward surface](design/external-clock-sync.md#the-whole-outward-surface--what-ships-what-is-open-and-the-rebirth-comparison)
> — MIDI clock in ✅ (macOS only) · host transport ✅ · Link ✗ (licence first) · notes in ✅ (but
> acidcandy reads none) · **CC in ✗ — one dropped `else if` in `midi_input.h`, and the cheapest missing
> piece there is** · clock/notes OUT ✗ (no output path at all) · audio into a DAW = the AUv3.
>
> **The gates, all in `zsh ios/mac.sh` (Release by default now; `CONFIG=Debug` to attach a debugger):**
> `rate-convert-check` (a 220 Hz sine through the real converter) · `au-transport-check` at 44.1k AND
> 48k (plays · follows tempo · stops · **restarts** · survives a 128-beat backward LOOP · plays after
> stop+rewind) · `--realtime` (the only mode that exercises the frame WORKER) · `--view` (the UI
> extension is wired). Plus, outside the AU: `bash tools/input-ring-check/run.sh` and
> `bash tools/present-race-check/run.sh` — **run both with `-tsan`, and know that `-bypass` must FAIL**
> (each carries a negative control because the plain runs pass even with the safety removed).
>
> **Cross-machine notes:** signing needs `TEAM` (defaults to this Mac's team in `mac.sh`) and
> `xcodegen`; the plug-in's saved state lives at
> `~/Library/Containers/com.tinyjam.mac.AU/Data/cart.blob` (delete it to boot a fresh rack — the
> checker now does); a wedged host is diagnosed with `sample <pid> 3 -file /tmp/x.txt` and by READING
> THE THREAD NAMES first.
> **Older next-action, still true: LOOK at the panel.** The AUv3 **VIEW is WIRED**
> (2026-08-12, `ios/AU/TinyjamAUViewController.swift` + `CanvasView(hosted: true)`, gated by
> `ios/au-transport-check --view`: the host is handed an `AUAudioUnitRemoteViewController` whose view
> loads at 492×308). What no gate can answer is whether the PICTURE is right — size, scaling, whether
> touches land where they look like they should. That eyeball is the next step, and everything below is
> behind it. Traps recorded in [`ios-plan.md → The plug-in view`](design/ios-plan.md#the-plug-in-view-phase-3):
> `com.apple.AudioUnit-UI` + a principal class that is BOTH the view controller and the factory (get it
> wrong and everything passes while the host shows generic sliders), a `-UI` extension loading on the
> MAIN QUEUE (a semaphore wait there deadlocks and blames registration), and
> `requestViewControllerWithCompletionHandler` living in **CoreAudioKit**, not AudioToolbox.
> **The three engine seams the view needed are all done and gated** — a plug-in inverts which thread is
> which, so every host call that touched engine state directly became a race, and two were crashes:
> **(1) the input event RING** (2026-08-12): `de_touch_*` and
> `de_key_event` append to an SPSC ring and `de_input_beginframe()` applies them on the frame's own
> thread, so an AUv3 frame running on the AUDIO thread no longer races the main thread's touches. It
> also fixed a bug the standalone app has always had — a press+release arriving between two frames was
> applied and undone before the cart looked, so `mouse_pressed` and `mouse_released` were BOTH never
> true. Design: [`audio-threading.md`](design/audio-threading.md) → "the INPUT ring"; gate:
> `bash tools/input-ring-check/run.sh` (`-tsan` is the real one, `-bypass` is the negative control —
> and note the plain run is NOT enough: with the safety removed the tear check still passed on arm64,
> because two adjacent float stores rarely interleave). **(2) RESIZE IS DEFERRED** — `de_resize` reallocs
> the framebuffer, so a layout pass landing mid-draw was a use-after-free; the host now records the
> request and `de_apply_pending()` applies it at the top of the frame. **(3) `de_copy_frame()`** — the
> view blits a SNAPSHOT of the last completed frame through a seqlock, never the live canvas (the
> seqlock exists because the draw is far too long to hold a reader off but the publishing memcpy is
> microseconds). Gate: `bash tools/present-race-check/run.sh`, and **TSan earned its keep on the first
> run**: the original published a grown buffer's pointer OUTSIDE the seqlock, so a reader could pair the
> old pointer with the new dimensions and read past the end. Nothing freed, so not a use-after-free, an
> out-of-bounds read instead — which no amount of running the plain probe would have caught.
> Known costs, deliberately: the present buffer is grow-only and leaks the old allocation (a reader may
> be mid-copy), and growing it mallocs on the audio thread (only when the canvas exceeds every previous
> size, i.e. while someone drags a window) · **Ableton
> Link** (the same `sync_push_pos()` call as the host path, so small — but the lib is dual-licensed, check
> that first) · MIDI clock **on iOS** + background audio (what ReBirth for iPad actually shipped) · clock
> **OUT** ([`midi-out.md`](design/midi-out.md)).
> **Two known gaps, deliberately recorded rather than fixed** (a third, the missing headless transport
> gate, was closed by `ios/au-transport-check.swift` — a fake AUv3 host with a `--free` negative
> control): the carrier app HANGS at launch under
> Catalyst (CoreAudio on the main thread; harmless to the plug-in, still wrong) · the engine is **compile-time 44.1 kHz**, so a host at another rate plays sharp and fast
> (fix = a resampler in the render block, never an engine refactor).
> **The rate gap is CLOSED** (2026-08-12): a host really does move our bus (48k accepted, nothing
> converts for us), so a compile-time-44.1k engine played sharp by the rate ratio — while all three
> transport checks **pass at 48k** (`--rate 48000`, now run by `mac.sh`), because the step comes from
> `sync_beats()` and the rate never enters that path. That is what confined the defect to the SOUND and
> kept the fix out of the engine: new **`ios/AU/RateConvert.swift`** (4-point Catmull-Rom + a four-pole
> anti-alias cascade engaged only when the host is BELOW 44.1k; **bit-identical at 44100**, where the
> original render path still runs), gated by **`ios/rate-convert-check.swift`** — a 220 Hz sine through
> the real struct: 220.000 Hz at 48k/96k/192k/22050/11025, level held, a 15 kHz tone REJECTED at 11025
> instead of folding to 3.9 kHz, and a nonsense host rate falling back to passthrough (`ratio = inf`
> would hang the audio thread in the caller's pull loop and take the host with it).
> **⚠ Read this before writing another audio oracle.** The first gate here measured pitch out of the
> RUNNING PLUG-IN and was wrong in the generic way: **a broken analyser and the real defect print the
> same number.** Per-window zero crossings of a ~50 Hz low band quantize to a grid at
> `crossings / 2 / windowSeconds`, and those grid positions MOVE WITH THE SAMPLE RATE — five crossings
> in 46.4 ms reads 53.83 Hz, five in 42.7 ms reads 58.59 Hz, ratio `48000/44100` exactly. So it printed
> "+147 cents, matching the prediction", which was the rate ratio in a costume; any signal gives it. Its
> A/A null passed at 0 cents and certified nothing (a same-rate null cannot see a rate-dependent
> estimator). What caught it: convert a 44.1k dump with `afconvert` and re-measure — same music, same
> pitch, estimator moved 53.83 → 46.88. **That figure is retracted** (ios-plan.md → "Retracted"), and
> the lesson is about REFERENCE SIGNALS: a cart with per-step probability and noise drums does not
> render the same audio twice (same-rate A/A correlated at 0.045), so it cannot be a converter's
> reference. A sine can. End-to-end corroboration that the fix landed, from the transport gate itself:
> at 48k, **86 onsets before vs 128 after** against 124 at 44.1k, with the 44.1k numbers unmoved.
> **Hot files:** `runtime/sync.h`, `runtime/midi_input.h`, `ios/AU/TinyjamAU.swift`, and note that
> `ios/project.yml` (iOS) and `ios/project-mac.yml` (Mac) are SEPARATE on purpose — don't merge them, and
> don't let a Mac build stage into `gen/app`/`gen/au`.
> **Resume-at:** the `▶▶ START HERE` block at the TOP of this lane — it carries the next job, what is
> already settled by measurement, and what not to do. Its owning section is
> [ios-plan.md → per-instance state is cheap after all](design/ios-plan.md#-the-route-a-context-struct--the-numbers-finally-measured-2026-08-13).
> Background, only if you need it: [ios-plan.md → macOS: hosting the AUv3 in GarageBand and Logic](design/ios-plan.md#macos-hosting-the-auv3-in-garageband-and-logic)
> for the AU arc (incl. the three signing/entitlement gates and their symptoms), and
> [external-clock-sync.md](design/external-clock-sync.md) for the clock seam itself.

> **▶ ACTIVE THREAD (2026-08-18) — `pedalboard`: the guitar rig, and the first app LIVE ON THE APP STORE.**
> **This lane did not exist until 2026-07-30, and it should have.** A handoff audit found `pedalboard` was
> the single most active thread in the repo — 18 commits since 07-28 (fret wires warmed into the board, the
> mute check tracking the hand, TRAVIS picking as a second autoplay style, autoplay keeping YOUR chord
> shape, boot on G major, `mouse_wheel_x()`) — while appearing in this file exactly once, as one of 23
> carts that use `INSTR_GUITAR`. A cold agent would not have known the effort existed.
> **✅ IT IS ON SALE (2026-08-17).** *Tiny Pedalboard* is the FIRST thing built here that a stranger can
> buy: **1.1 and 1.0 are both `READY_FOR_SALE`** at $1.99, confirmed against Apple's own API rather than
> from memory (`node tools/asc-push.js pedalboard --metadata --dry-run` reports "no editable App Store
> version", which is what a live app looks like). So the whole chain works end to end, cart to store
> page to purchase, and `tinyacidjam` is the second app walking the path this one proved.
> ⚠ **A live version is FROZEN**: nothing in the listing is editable until `--new-version` creates the
> next one, so a copy fix now costs a version bump. The engine seam it rides is the `input_monitor(gain)`
> pedal tier that shipped 07-22, so real GUITAR IN → amp → pedals works on desktop.
> **Resume-at:** the cart's own punch list — `node tools/cart-todos.js pedalboard` — plus
> [`design/effects-bus-architecture.md` → Increment E, the output stage](design/effects-bus-architecture.md#increment-e--the-output-stage-4th-zone-cabinets--ampcab--leslie)
> for the amp/cabinet model
> (`runtime/ampcab.h` is the shared voicing table; `fxicons.h` is the shared pedal LOOK).
> Hot files: `tools/carts/pedalboard.c`, `runtime/ampcab.h`, `apps/pedalboard/app.json`.

> **▶ ACTIVE THREAD (2026-07-31) — Synth Secrets: the audit is COMPLETE, the build plan is running (Phase 0 done, **PHASE 1 COMPLETE 7/7**, **PHASE 2: 2.1, 2.2 and 2.3(a) SHIPPED — PIANO now has real stiff-string inharmonicity + a completed Railsback curve; 2.3(b) DROPPED on measurement; 2.4's bowed body now SHARED PER SLOT with a size axis, and defaulting it on is the live item**).**
> The owner supplied Gordon Reid's **Synth Secrets** (Sound On Sound, 63 parts, 1999-2004) and asked for a
> cross-check against `runtime/sound.h`. **All 63 articles are now read**: an architecture pass plus eight
> per-family recipe passes, ~106 sub-findings, every one citing both sides (part + issue on the book side,
> `file:line` on ours). Findings live in [`design/synth-secrets-audit.md`](design/synth-secrets-audit.md)
> §A-§M; **work happens in [`design/synth-secrets-plan.md`](design/synth-secrets-plan.md)**, which is the
> ordered ledger. Do not add work items to the audit.
>
> **The full record is in the plan, not here.** ~400 lines of settled narrative lived in this lane until
> 2026-07-30 and were deleted after a block-by-block check confirmed every one is already in
> [`synth-secrets-plan.md`](design/synth-secrets-plan.md), usually with more tables and more numbers.
> Phase 0 + Phase 1 (7/7), items 2.1 / 2.2 / 2.3(a), the 2.3(b) DROP, the two PIANO defects, the four
> oracles and every measurement table: all there, and ledgered in [`STATUS.md`](STATUS.md). This lane
> now holds only what you need to *resume*.
>
> **Resume-at — the live queue, most-ready first.**
>
> 1. **§M2 / item 2.4 — `BOWED` HAS A BODY, AND IT IS NOW ONE SHARED BOX PER SLOT WITH A SIZE (2026-07-31).**
>    `MODE_BOW_BODY` ships: three parallel feedback combs at 1.3/2.3/3.7 ms (Reid Part 22), opt-in,
>    **default OFF**. Owner's ear: the body WINS (audit §F4's gap is real), and the combs were kept over a
>    bandpass-formant variant on §M2's own argument after the ear could not separate them.
>    **The sizing blocker is GONE.** The owner picked "one body per slot" from the three routes, and it
>    shipped: a pooled `bow_bodies[8]` claimed per slot (like `fx_bus_for`), plus **`MODE_BOW_SIZE`** on
>    `eng_p[2]` — whose `0.5f` bank default maps to exactly 1.0x, the violin box, so no existing cart moved.
>    1.0 = the cello set (3.19/5.65/9.09 ms). Default path verified **byte-identical** (`3de65baf5bd8`).
>    ⚠ **If you touch this: the box is clocked ONCE PER SAMPLE in `bow_body_advance()`, never from the voice
>    loop.** A comb's resonances come from its clock rate, so advancing it per sounding voice would make the
>    body's pitch track the chord size (a triad ringing ~a twelfth high) — silent, and no gate calls it an
>    error. `wet_share` is likewise divided by the reader count so the box radiates once, not N times.
>    **Both ear calls are IN (2026-07-31): the cello "sounds more like a real cello", the double-stop
>    coupling "reads well".** So the body is cleared to go on, and it is going on PER CART.
>    ⚠ **Do NOT default it on via the bank default of `eng_p[1]` — that index is also GUITAR's and PIANO's
>    pick-noise amount** (`sound.h:4866`/`:5211`), so raising it would switch on an attack click for every
>    guitar and piano cart. The per-engine index SPACE is free; the shared DEFAULTS are not (§L6's lesson,
>    one layer down). Per-cart is right anyway: each instrument needs its own SIZE.
>    The size range now reaches a **DOUBLE BASS** (`BOW_SIZE_BASS`, 4.50x, 736-sample line, `BOW_BODY_MAX`
>    768) because the three upright carts are basses and a cello box was still too small. Affordable only
>    because bodies are shared. `BOW_SIZE_CELLO` is `0.7086f` so widening did not re-voice the approved cello.
>    **NEXT HERE, in order:** (1) the owner listens to the three baked bass carts — `upright`, `walkbox`,
>    `walkroll` (body 0.85 at BOW_SIZE_BASS; peak drops ~2 dB, a real mix-balance change). (2) Then the rest
>    with their own sizes: `mariachi` (2 violins), `polopan` (pizz), `bandbox` (bass + arco pad), `portapop`,
>    `modrack` slot 38. (3) **Leave `soundcheck`/`tunecheck`/`voicestress`/`pipetune` body-OFF** — they feed
>    the audio gates, so re-voicing them moves the baselines.
>    ⚠ Measuring a BASS body? Use `harmonic-spec`, NOT brightness/centroid — a 60 Hz box works below 350 Hz,
>    so `wav-envelope` reported "no change" across a ±17 dB reshaping. Pick the gate by where the effect lives.
>    **Then `guitar`** — still a measurement cross-check ONLY, see the scoping below.
>    Two traps banked: a 1–4 ms body has a MILLISECOND RT60 so there is no audible tail to A/B on (the
>    "reverb" IS the frequency response — Reid's duality); and blend a body ADDITIVELY, never as a crossfade,
>    or you discard the string's own ringdown (a crossfade at 0.8 made a pizzicato note die twice as fast).
>    Ear sets: `build/ab/bowed-body-*.wav` (arco) and `build/ab/bowed-pizz-*.wav`.
> 1b. **`guitar` — the remaining half of §M2, and a measurement cross-check ONLY.**
>    Compare three short delay lines against the four `gt_body` biquads head to head, take the numbers, and
>    **do NOT flip its default.** `GUITAR` is in **23 carts** — essentially the whole amp/pedal shelf
>    (`combo`, `pedalboard`, `tubescreamer`, `mistress`, `springtank`, `wba`, `mixbooth`, `afrobeat`,
>    `thexx`, `mariachi`, `portapop`) — **all voiced by ear against the existing biquad body**, so replacing
>    what they were voiced against is not a neutral act and no oracle would flag the damage. If the delay
>    body also wins there, that is a separate conversation with 23 carts in it.
>    Also mind that §I3's piano tricord already has a weak 0.2% output→string-1 tap, so do not A/B that
>    against an assumed zero.
>    **⚠ `inharm-spec` is the WRONG oracle here** — a body changes partial LEVELS (a frequency response),
>    not partial FREQUENCIES. Use `harmonic-spec` + centroid/brightness from `wav-envelope`. And `BOWED`
>    self-oscillates, so there is no decay to measure: it is a steady-state spectrum, and the `effLen`
>    sustain trap does not apply as it did to PIANO.
> 2. **Re-voice the other five piano voicings' `B` by ear.** Cheap, no new mechanism. They currently scale
>    from `PianoVoicing.stiff` (celesta 2.4e-4 down to clavichord 4.4e-5 — plausible ordering) but **only
>    the grand was ear-checked**. Use the `stiff` slider in the `piano` cart.
> 3. **§I4d** — with no stretch the loop still runs +1.3→+4.0¢ sharp (its own uncompensated delay
>    bookkeeping). Note the offset is **window-dependent**, because the brightness bloom moves `ksb` and
>    hence the loop delay *within* a note, so any blessed residual is per-measurement-window.
> 4. **Two gates, both cheap and both earned by this thread.** An **A/B comparability gate** (refuse to call
>    two WAVs an A/B when peak, rms *or the decay curve* disagree — peak and rms passed on a pair whose
>    decay differed by 19 dB). And a **"pitch is invariant across `MODE_PIANO_STIFF`"** assertion, which
>    belongs in `inharm-spec` and NOT tune-check, because it needs the spectral method: YIN cannot track an
>    inharmonic string (it read +26¢ sharp at conf 0.65).
> 5. **Also open, lower value:** `B` is constant across the register where a real Railsback curve rises at
>    both ends; and extending the runtime-seam-plus-differential pattern across the `instrument_*`/`MODE_*`
>    surface, since *four* bugs in this thread were the same shape — a value computed correctly that never
>    reaches the sound.
>
> **Three lessons this thread paid for, worth carrying to any engine work:**
> (1) `sound.h` said *"tune-check flags PIANO by design — that IS the stretch, not a bug"* and tune-check
> **passed**. A comment that pre-emptively explains away a gate turns a green check into false confirmation.
> When a comment says a gate should be red, **verify that it is red.**
> (2) The first draft of §I4c said the stretch was cancelled outright — measured at **one note** and
> generalised, when the missing *flat bass* in the curve was the clue on screen the whole time. **Measure
> both halves of a signed curve before describing it.**
> (3) Ranking an item by how many findings it closes is sound **only while those findings are believed
> rather than measured**. 2.3(b) counted five families and had one. 2.4's check cost four greps and one
> render, and re-scoped it honestly. **Check the premise of any finding-count before trusting the rank.**
>
> Full write-ups, every table, and the cold-start reproduction recipes →
> [`design/synth-secrets-plan.md` §2.3(a)](design/synth-secrets-plan.md#the-premise-failed-three-defects-found-by-measuring-first-2026-07-29).
>
> **Orienting cold:** `node tools/orient.js`, then plan §1 (the four gate kinds, the A/B protocol, and the
> owner's standing constraints), §2 (the add-vs-change ladder), §4 (Phase 1, all seven write-ups), and
> §5 (Phase 2, where the live work is). The audit is reference only — never add work items to it, only
> ✅ verdict banners pointing back at the plan.
> **Two process traps already hit:** `ui-audit` passes **low-contrast** text (it only catches off-screen
> and overlapping), so read the baked PNG; and `--run` bakes only the thumbnail, so **re-embed after every
> source edit** or the pre-commit hook will (correctly) reject a stale `.cart.png`.
>
> **Hot files** (several agents share this tree): `runtime/sound.h` — targeted `Edit`s only, never a
> full-file `Write`, and confirm your change survived the commit (`git show HEAD:runtime/sound.h | grep`).
> Also `runtime/studio.h`, `editor/src/studioDocs.js`, `editor/src/shell.js` (the four-place API
> registration), `tools/tune-check.js` + `tools/carts/tunecheck.c`, and `tools/carts/piano.c`/`bowed.c`.
> **▶ ACTIVE THREAD (2026-07-26) — the CONTEMPORARY ReBirth: rungs A+B shipped, the PSOLA artifact hunt PARKED behind a new gate, amapiano next.**
> The post-hardware rack: ReBirth cloned unobtainable *machines*, but modern genres were never made on
> gear, so a contemporary version clones **techniques** (the glide, the ratchet, hard tune, the
> always-on squash). The audit found **ten of twelve boxes across three candidate racks need zero engine
> work**; the gaps became rungs.
>
> **Shipped:** Rung A `multiband()` / `instrument_multiband()` / `FX_MULTIBAND` (three-band dynamics with
> an UPWARD half, the OTT box, `mix` 0 = byte-identical bypass) with `hyperbox` as its showcase rack ·
> half of Rung B, `sample_shift()` + `harmonize_mic()` (length-preserving transpose) with `voxshift` as
> the probe · a **third rack audited** (amapiano §1c/§2c, zero engine gaps — `INSTR_MEMBRANE`'s morph
> macro is pitch-bend, so the engine already had a log drum) · and **`tools/psola-check.js`**, the
> artifact gate.
>
> **Two things are parked, and they are DIFFERENT problems — do not conflate them:**
> 1. **The audible glitch on the snapped and up takes** (an epoch-mapping phase bug). The maker hears it;
>    it is now measured, ~25-29 events/take against the RAW control's 1. **Three fixes tried, all three
>    reverted**, each regressing the snap face into period doubling: the WSOLA correlation lock (raw and
>    normalized), a monotone accumulator, and the lock with a narrow ±8% window. All three are
>    `⚠ DO NOT` comments in `at_psola_slot` with numbers. The pattern is the finding: every attempt broke
>    *snapped* and none broke the shifted takes, because snap's `Tt` is within a few percent of `T`, so
>    epoch jitter is comparable to the correction itself. **The fix must be jitter-FREE, not
>    better-guessed** — a dedicated spike, not a tweak.
> 2. **GAP 3, the chipmunk↔natural formant dial.** Not a clicking problem at all. Needs a **new
>    envelope-rescale stage**; `at_psola_slot`'s unused `formant` argument is a **trap, not a half-built
>    feature** (proven in `rubberband-reference.md` §2a-bis: forcing `fstep` to 1 held the formants
>    exactly and moved the pitch *not at all*). Not an FFT necessarily — LPC or a filter bank also work.
>
> **Before touching either, run `node tools/psola-check.js`** (before AND after). It renders `voxshift`'s
> four takes through three detectors and **no single one is sufficient**: a period-doubled take is still
> perfectly periodic, so a periodicity metric scores that regression as a 2x *improvement*. It killed
> attempt #3 in one command after #1 and #2 each cost several listen-and-report rounds.
>
> **NEXT, in order:** the **amapiano rack** (cheapest of the three — no rung, no external audio; its open
> question is whether the swing is playable, since the genre's identity is a *feel*), then **Rung C:
> `beatfx(mode, bars, mix)`**, a beat-synced buffer re-reader, which must be **ride-safe** (parameters
> read per sample, never re-allocated per call) or it lands on the wrong side of `lint-fx-frame.js` —
> and see `rubberband-reference.md` §2c first for the non-uniform-stretch idea. Resume at
> [Rung C in contemporary-rebirth.md](design/contemporary-rebirth.md#rung-c--gap-1--a-beat-synced-buffer-re-reader-halftime--beat-repeat).
> Smaller open ends: a home cart for `harmonize_mic` (live-only, so ADR-0032 says its clip must be
> captured live, not replayed from a `.rec`), `hyperbox` v2's real voice box (waits on GAP 3), and the
> hip-hop rack, whose hard part is not DSP but **where the loop audio legally comes from**.
>
> Hot files if you take this: `runtime/sound.h` (targeted `Edit`s only, never a full `Write`).
> Adjacent records: [`design/rubberband-reference.md`](design/rubberband-reference.md) (read §2a + §2a-bis
> before GAP 3; the library is **GPL and bars App Store distribution** — reading reference only, never a
> dependency, never a port), [`design/rebirth-classic.md`](design/rebirth-classic.md)
> (the RB-338 pilot whose chassis all three racks reuse), [`design/transparent-autotune.md`](design/transparent-autotune.md)
> (the correction half, which needed two spikes of its own), [`design/tinyjam-racks.md`](design/tinyjam-racks.md)
> (the rack program + the trademark rule: "SB-808" in the sketch is close to a live mark, so the
> hip-hop rack needs its own name before it ships).

> **▶ ACTIVE THREAD (2026-07-21) — `bandbox`: the chord-chart SEQUENCER — WIRED (spec 131 green); open = `band.h` extraction + richer per-voice editors.**
> **Resume-at:** [`design/bandbox.md` → Build plan](design/bandbox.md#build-plan-ordered) — steps 1–6 are
> done and the cart plays; what is left is the `band.h` extraction and richer per-voice editors.
> The standalone device-face instrument the `chordwise` analyzer pointed at: a 160×100 face where you
> compose a chord chart and a genre band (chords/bass/mel/drums/pad) follows it, every voice a lane of
> **lego-block cells** with per-cell **p-locks**. This session settled the whole LOOK (draw-only mockup,
> committed `26e58cb6`): glass 5-lane tracker + chassis (aligned voice rail / nav / keybed), FONT_TINY,
> screen-morphs-chassis-stays. **NEXT = the build phase — wire it.** Full brief (data model, ordered
> build plan, reuse map, gotchas): [`design/bandbox.md`](design/bandbox.md). Reuse the working engine in
> `chordwise.c` (genre band maps + playback) + harmony.h/radio.h/drumkit.h/keybed.h; the shared `band.h`
> extraction is a LATER call (third-customer rule). Chord-bloom context: [`design/bossa-rack.md`](design/bossa-rack.md).

> **▶ ACTIVE THREAD (2026-07-21) — responsive-first device faces: the `face.h` grammar (Layers 1–3 SHIPPED).**
> The "could a cart be responsive-and-opinionated *from the start*?" line
> ([`design/responsive-first-device-face.md`](design/responsive-first-device-face.md)) — a thread DISTINCT
> from the acidrack retrofit (the *device-adaptive layout* lane) and the epiano/scalegrid playbook (the
> *responsive instrument UI* lane). Layer 0 (runtime reflow) was already done; this session shipped the
> whole cart-land stack, all committed:
> **L1** a device-face STARTER, [`deviceface`](../tools/carts/deviceface.c) — the chunky route-2 canvas +
> five stacked bands + the shared 16-column register, with marked seams to drop an instrument in.
> **L2** `ui_button_cell`/`ui_knob_cell` (`runtime/ui.h`) + a `LayLane` register (`runtime/lay.h`) so a
> sequencer view and its step strip bind to ONE lane and align by construction, not by hand.
> **L3** the declarative grammar `runtime/face.h`: a `FaceZone[]` table (`FACE_BAND` top/bottom · `FACE_LANE`
> per-step · `FACE_HERO` = the remainder) that `face_layout()` carves, ENFORCING the principles — a
> width-stealing side-rail is not expressible. Demo = [`facedemo`](../tools/carts/facedemo.c).
> **Proven across three conversions, each driving a real refinement:** `chipjam` (landscape, input-in-draw)
> → the register escape hatch; `dubjam` (input-coupled `update()`) → the `relayout()`-in-`update()` pattern
> + `face_sublane`/`face_screen` (flanking the HERO is allowed — only top-level full-height rails are
> banned); `grooveface` (portrait, multi-skin, 320×400) → `face_resize_to` (design density is per-face;
> layout is orthogonal to skin). The grammar held every time and got better for the friction.
> **Tablet/iPad-Pro spread MOCKED, kept OPEN** ([`roomyface`](../tools/carts/roomyface.c), keys 1/2/3):
> **B** tile-the-rack 2×2 · **C** unhide-one-machine · **D** 2×2 + master (the ReBirth RB-338 classic).
> Decision with the maker: the arrangement stays a per-cart `device_class()==ROOMY` choice, NOT a baked
> default — face.h does all three with zero engine work (B/D = `face_layout` per grid cell + a master
> column; C = one wider zone table).
> **NEXT (the maker's call):** prove a ROOMY branch on a REAL cart — `acidcandy` (a 4-machine rack, D/B the
> likely pick) turns the mockup into a shipped tablet arrangement and hardens the grammar once more; OR
> **Layer 4** (make face.h the default), worth it only once L3 is judged proven enough.
> **acidcandy ROOMY — SHIPPED 2026-07-23, and this block used to say the opposite.** It described a
> COEXISTENCE plan ("app is under App Store review — do NOT delete the old"), a `rack_view==1` path, a NEW
> button, and an INERT soft-key row. All of that is gone: commit `a16b2527` **removed** the old 2×2
> `draw_rack` and promoted ROOMY to THE tablet view (`acidcandy.c:106` says so; `rack_view` is now `0` =
> phone / `2` = ROOMY, auto-selected from `device_class()` at `:3396`, and most of the soft-key row is
> wired — `r2_dpaint`/`r2_303panel`/`r2_drumpanel`/`r2_mstpanel`). **A reader following the old text would
> have gone to protect code that no longer exists**, which is why it is called out rather than quietly
> deleted. Current state and what is genuinely left:
> [`design/acidcandy-ipad-layout.md`](design/acidcandy-ipad-layout.md) (STATUS: shipped — its checklist is
> historical).
> **Resume-at:** [`design/responsive-first-device-face.md`](design/responsive-first-device-face.md#the-layers--cheapest-to-deepest)
> — Layers 1–3 shipped; **Layer 4 (make `face.h` the default) is the open one.** The acidcandy ROOMY proof
> above is done, so that branch of the NEXT above is closed.

> **⏸ PARKED (last touched 2026-07-18) — `walkbox`, a walking-bass step-sequencer.** Core + articulation
> play; what is left (ghost notes, hammer-on/pull-off, presets) is a wish list, not a blocker. Ledgered in
> [`STATUS.md`](STATUS.md). Resume at [`design/walkbox.md`](design/walkbox.md).

> **▶ ACTIVE THREAD (2026-07-18) — the CHORD-BLOOM rack (`chordblossom2`).**
> The winning answer to "make a radio song playable": NOT a radio turned inside-out (`bossabloom`
> felt like *"radio + a chord-picker"* — the machine still composes) but an **instrument you PLAY
> with your hands, wearing a genre FLAVOR** — a backing band that follows the chords you play. A
> fork of `chordblossom` (never branch — copy-to-new-cart). SHIPPED this session: a data-driven
> **Flavor table** (NEUTRAL / BOSSA / YACHT / CITYPOP; each row = comp+bass onsets · voicing ·
> timbre · groove · tempo · colour); **diatonic KEY mode** (the white keys ARE the in-key chords,
> labelled — a ii–V–I is just S/G/A); the clean **3-axis model** — **SPICE** strip (out-of-key
> chords you press) · **RICHNESS** (simple→lush) · **GROOVE** (sparse→busy), each ONE behaviour
> (killed the arm/toggle murk after a play-test rethink); a **living accompaniment** (4-bar phrase
> that breathes — swell + turnaround fill + human drops); a **tiny per-part PEDALBOARD** on the FX
> tab (HARP RVB/DLY/CHO · BASS DRV/CHO, per-slot FX, set-and-hold, lint-fx-frame clean); and
> **STOP + REST** (REST hushes the chord, the groove keeps going). BOSSA is tuned by ear
> ("sounds right"); YACHT/CITYPOP are plausible-but-untuned.
> **Resume-at: [`bossa-rack.md` → Open questions](design/bossa-rack.md#5--open-questions)** (its top
> carries the ★ BUILD FINDING that flavors-on-chordblossom supersedes the radio-rack plan) +
> [`genre-box-rosters.md`](design/genre-box-rosters.md).
> NEXT = tune YACHT/CITYPOP by ear · per-flavor SPICE sets + fills · the candy skin (per-flavor
> palette + a mascot) · RHYTHM pedals (needs a drum-slot refactor — drums are raw `INSTR_` hits) ·
> label cosmetics (Db7/Bb7, the `C MAjmaj7` readout). Hot files: `tools/carts/chordblossom2.c`,
> `docs/design/bossa-rack.md`, `docs/design/genre-box-rosters.md`. Related: `bossaface.c` (the candy
> vibe mockup) + the superseded `bossabloom.c`.

> **▶ ACTIVE THREAD (2026-08-18) — Android as a Google-Play build target.**
> The engine already runs frameworkless behind `platform.h` (`DE_NO_RAYLIB`), so Android is a **host
> shell + Gradle packaging**, not engine work. SHIPPED this session: the toolchain (NDK/SDK/emulator,
> sudo-free via Homebrew) + **spikes 0–3** — the real engine cross-compiles with the NDK and RENDERS
> (GLES2 fullscreen-quad blit of `de_framebuffer()`, host GPU) + SOUNDS (AAudio) + takes TOUCH on an
> arm64 emulator, all committed under `android/` with `android/build.sh` the one-command loop. Plus the
> editor's **🤖 export .apk** button (share popover, `EDITOR=1` build of the live buffer) — a
> sideloadable debug APK (arm64+arm32), no Play account/device needed.
> Emulator audio was flaky (a virtual-audio-device route conflict — BlackHole / Multi-Output device);
> the working recipe is **device-nudge-after-launch + landscape lock + a deep AAudio buffer**, all only
> needed on the emulator (real hardware is clean; audio survives rotation natively there).
> **Immersive fullscreen added 2026-07-17 (compiles, APK builds; NEEDS ON-DEVICE CHECK).** The
> Fullscreen theme only hid the status bar → the nav bar + edge-swipe system bars showed mid-game.
> `android_immersive()` in `cpp/main.c` now hides the bars via `WindowInsetsController` (API 30+;
> targetSdk 35 ignores the old flags) + `BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE`, RE-applied on
> `APP_CMD_GAINED_FOCUS`/rotation (the missing piece), + cutout draw-in via `res/values-v27/styles.xml`.
> The Android twin of iOS's edge-gesture deferral. JNI is exception-guarded (can't crash); verify the
> nav bar truly stays hidden on a real phone — see the immersive gotcha in `design/android-plan.md`.
> **Resume-at: [`design/android-plan.md` → the spike ladder](design/android-plan.md#the-spike-ladder)** —
> NEXT = spike 5 (signed `.aab` for Play) → 6 (Play Billing via the
> existing `Store_*` gate; needs a Play Console account) → 7 (`build-app.js --android` multi-cart app).
> (Spike 4, save → internal storage, **DONE 2026-07-16** via the new `de_set_save_dir(dir)` seam.)
> Hot files: `android/app/src/main/cpp/main.c` (the NativeActivity shell), `android/build.sh`,
> `docs/design/android-plan.md`.

> **▶ ACTIVE THREAD (2026-08-18) — the audio-input frontier (the engine HEARS *and SPEAKS*).**
> The reddit-gaps drip kept surfacing the SAME blocked wishes (hum→MIDI, pedals, live looping) — all
> one missing capability: the engine had no ear. Now it does, on every platform, and it vocodes. The
> arc, spike → ship → instruments → vocoder:
> - **The mic seam, all four platforms** — a device-free `platform.h` contract (host owns the device +
>   permission, pushes frames via `de_audio_input`; engine only analyzes): desktop CoreAudio
>   (`1cfe46aa`), WEB `getUserMedia` + iOS `AVAudioSession` (`2689ed68`), ANDROID `AAudio`+JNI
>   (`bc5599d2`). `mic_start/stop/active` + `mic_level` (RMS) + `mic_pitch` + `mic_record`.
> - **YIN pitch** (`2d552d25`) — `pitchscope` diagnosed the zero-crossing estimate octave-jumping; replaced
>   it with a YIN detector (octave-safe). `mic_pitch()` is now a real melody/controller axis.
> - **Carts**: `mictest`, `pitchscope`, `humtheremin` (hum→theremin), `voxbox`; `breakchop` gained mic
>   **beatbox auto-chop** (record → onset-slice onto pads) via `mic_record` (capture-then-freeze). Then two
>   more capture-then-freeze instruments: **`humseq`** (vein 2, hum→MIDI — a hysteresis note-tracker freezes
>   a hummed melody to a scale-locked loop on any INSTR) and **`singsynth`** (vein 3, voice sampler — hold a
>   vowel, loop it into a keybed instrument SK-1-style, draggable loop region). Both use the `ui.h` button bar
>   + `keybed.h`. (Gotcha banked: `rect`/`rectfill` are `(x,y,w,h)`, not corners.) Then **`hardtune`** (vein 3
>   flavour A, robot auto-tune) — a saw carrier locked to `snap_scale(mic_pitch)`, vocoded live, RETUNE slider.
> - **THE VOCODER** (`379fef80` Phase 1 + `46b45c35` Phase 2) — a master-stage 12-band carrier×modulator
>   filterbank in `sound.h` (`vocoder`/`vocoder_send`), fed live by the mic through a **lock-free
>   audio-thread PCM ring** (`sound_extin_*` + `vocoder_mic`). `vocode` = deterministic synth-modulator
>   showcase; `voxbox` upgraded to the REAL vocoder (sing → a chord speaks your words; "sounds like
>   Stevie Wonder"). Determinism carve-out: [ADR-0032](decisions/0032-live-mic-effects-are-live-only.md)
>   (live-mic-through = live-only; capture-then-freeze stays deterministic). All gates green; desktop +
>   web live-verified by the maker.
> - **VOCODER v2 — the unvoiced/sibilance band** (`e7d782fe`): `vocoder_unvoiced(amount)` — a source-agnostic
>   detector (top-band energy fraction) swaps the top bands' excitation from the tonal carrier to band-limited
>   noise, so consonants (s/t/sh/f) cut through instead of turning to mush. `vocode` A/Bs it deterministically
>   (consonant windows: brightness ~0.03→0.2, centroid ~3.8→5.3 kHz, no peak rise); `voxbox` rides it live.
> - **TRANSPARENT auto-tune — OFFLINE primitive SHIPPED + real-voice CONFIRMED** (`b5b26cf6`): `sample_autotune(slot,
>   root, scale, amount)` — formant-preserving pitch correction (whole-buffer TD-PSOLA) on a
>   recorded take, a game-thread sibling of `sample_read/load`. Proven in two offline spikes (v1 known-f0 formant
>   hold, v2 detector-epoch correct-to-scale), byte-faithful cart-C→engine, gated by **`tools/formant-check.js`**
>   (f0 + formant-peak oracle). Showcase = **`mictune`** (rebuilt as a clean app: sing → in-tune, R tunes your
>   voice; the old test-bench version was "confusing"). Maker sang in → "sounds pretty good". *Done.*
> - **LIVE real-time auto-tune — SPIKE BUILT + PARKED** (`d067f257` + phase-lock fix): `autotune_mic(root, scale,
>   amount)` — streaming two-pointer TD-PSOLA on the audio thread off the `sound_extin` ring, monitored into the
>   mix; cart **`livetune`**. Formants preserved + amplitude smooth (phase-lock epochs killed an initial "pulse"),
>   but **audible pitch WARBLE remains**. Diagnosed against a real vocal via the new **`DE_MIC_WAV` harness**
>   (`8c138015` — feed a WAV as the mic headlessly, `tools/testdata/vocal-8s.wav`): a fundamental-band jitter
>   metric shows octave-continuity / retune-glide / hysteresis all MEASURE as no help → the warble is the streaming
>   pitch/epoch **resolution**, needs a YIN-grade real-time tracker or the phase vocoder (a dedicated effort, not
>   spike tweaks). See `design/transparent-autotune.md` §"live real-time path" for the full write-up.
> **Resume-at: [`design/audio-input-frontier.md` → the frontier, ranked](design/audio-input-frontier.md#what-it-opens-next--the-frontier-ranked-by-juice-per-effort).** Auto-tune
> arc is COMPLETE for the offline feature; the LIVE path is feasible-and-parked (warble). Open frontier, ranked:
> (1) the **live looper** — the *pedal tier* half of this SHIPPED 2026-07-22 (`input_monitor(gain)`, and
>     `pedalboard` became its own app, **on sale since 2026-08-17**); the looper is the part still open;
> (2) **vocoder v2 tail** — mic-rate resample (non-44.1k device mics drift the ring) + on-device latency tuning;
> (3) **beatbox→live drum trigger**; (4) **live-autotune warble** if revisited (real-time YIN / phase vocoder).
> `voxroll` decouples formant/pitch but only on synth `INSTR_VOICE`, not a real-mic corrector.
> Hot files: `runtime/sound.h` (vocoder + extin ring + `sample_autotune`/`autotune_mic`), `runtime/mic.h`
> (analysis + record + ring write), `runtime/studio.h`/`.c` (the seam + `DE_MIC_WAV`), the per-host capture
> (`mic_desktop.h`, `ios/Sources/AudioEngine.swift`, `android/…/main.c`). Reference carts: `vocode`/`voxbox`
> (vocoder), `mictune` (offline auto-tune), `livetune` (live spike). Test harness: `DE_MIC_WAV=<wav>` + `tools/testdata/`.

> **▶ ACTIVE THREAD (2026-07-29) — the candy acid RACK (`acidcandy`).**
> `acidcandy` (160×100 ×4) packages `acidrack`'s guts as the **device-face paradigm** instead of the
> accordion: a colour-**cartridge** nav strip of five FOCUSABLE machines on ONE transport — **2×303**
> (`acid303.h`; 303b an octave up = the bass+lead duo), the full **808** (`tr808.h`) and **909**
> (`tr909.h`), and a **MST** master. Every voice is honest — `runtime/tr808.h` + `runtime/tr909.h` were
> EXTRACTED from the tr808/tr909 carts this session (the `acid303.h` move; refactor verified *within the
> run-to-run noise floor*, since the drum voices' noise isn't sample-reproducible), so acidcandy can't
> drift from the reference machines.
> SHIPPED: gear-drag knobs (vertical=value, pull sideways=fine gear, double-tap=reset); the cartridge nav
> (tap body=focus, LED=mute from any face) + a **drag-release-BOUNCE fix** (a stray 2nd click ~8 frames
> after a knob-drag release was landing on a mute LED → silence; now a `TAP_SETTLE` guard ignores nav taps
> for ~200ms after a drag lifts — repro was `build/.rec` replay); the **303 note-bars** (drag = free-draw
> the melody, height=pitch scale-snapped, bottom band=rest; tap=toggle) + a **FLAG palette** in the screen
> (arm ACC/SLD/TIE/OCT+/OCT-/LEN, paint-drag across) that wires per-step tie/octave + per-line
> **LENGTH/polymeter** into playback; the roll draws every flag (glide lines / tie bars / oct ticks); the
> **MST** face (GLU/FLT/RES/FB/PUMP + delay TIME + per-machine SEND, default glue tames the mix) with a
> **drawable PCF** lane.
> **LATEST (2026-07-19) — a LIVE-SET + MST-layout push (all shipped + committed):**
> the 808/909 TOOL-vs-VIEW cleanup + `draw_arms` deletion is long done (both drum faces, the far-right
> stacked-word tool selector). This session added, in order:
> — **master SWING** (one rack-wide `g_swing`, ReBirth model — drums AND both 303s lock; drums drag the
>   odd-16th fire, a 303 delays its step FLIP, same `sw=g_swing*0.60` fraction) + **master TEMPO** (`g_bpm`);
> — **queued bar-quantized bank switch** (tap a bank = arm + blink, lands on the next bar downbeat);
> — **303 PERF lenses** (a PERF soft-key: HALF speed — the 2X/8-12 speeds were later DROPPED · ACC accent-all ·
>   OCT · REV · STAC/GLIDE slide-flip · ROLL),
>   **non-destructive read-lenses**, each **TAP = latch (persists across faces) / HOLD = momentary** (`lcdlatch`);
> — **FLAG-screen NOTE-on** (FL_NOTE, the default armed flag — add notes without leaving for SEQ);
> — **SLIDE-GATE BUGFIX**: acidcandy's staccato `acid_gate` was cutting slid/tied steps at 70% → **manual
>   slides never actually glided**; fixed to tb303's `on && !slide` rule (proven: manual all-SLD == a pure
>   glide, 1.00000; no-slide patterns byte-identical). tb303/acidrack were always fine — this was acidcandy-only;
> — **LCD grown to h30 on ALL FOUR faces** (one consistent size; contains the soft-keys; 1px gap to the content
>   below); **MST redesign** — SWG + TEMPO are a matching **gutter knob pair** (right of the LCD; `gknob` shows
>   the real value), **DELAY division buttons moved UNDER the LCD**, SEND knobs pulled in to free the corner;
> — **three drawable master lanes** on the MST screen (MIX/PCF/CRU/GAT): **PCF** filter/tone (green) +
>   **CRUSH** bitcrush/texture (orange) + **GATE** chop/rhythm (pink), each `m*[16]` applied set-and-hold on step change;
> — ~~a DUB pad in the freed MST corner~~ — **REMOVED 2026-07-21** as gimmicky (its `dub_*` globals are gone;
>   the MST right column is now just SWG + TEMPO). Its old "needs an ear check" item is therefore moot.
>
> **2026-07-29 session — the FX HUB + a formant that SPEAKS.** The phone MST soft-key column was full and one
> key per effect does not scale, so the master DEVICES moved behind a single **FX** key (`mstflow 8`): a MENU of
> chips wearing their `fxicons.h` glyphs, with **SWP retired into it** and the three drawable lanes keeping their
> own keys (they are the rhythmic soul, not devices). Adding the rest of the MORE-MASTER-FX shelf
> (tape/wah/ringmod/drive/spring) is now one `FXK[]` entry + a knob branch each. **DRY** = a tap-latch /
> hold-momentary kill that holds every device dry WITHOUT clearing its arm flag, so lifting it restores the exact
> blend — verified **byte-exact** (two devices armed + DRY renders the everything-off baseline sha).
> New **VOWEL** device (master `formant()`), and **SPK** makes every fresh 303 note-on advance a vowel WORD with
> the vowel gliding to each syllable — the glide is what `formant()`'s ride-safety buys, and no
> buffer-rebuilding effect (crush/flanger/gate) can do it. Pauses came next (the maker: *"sometimes we need a
> little pause"*) and they are what turns a slur into speech: a gapless word moved the spectrum no more than a
> STATIC vowel (centroid std 1270 vs 1281), rests took it to 1964 (+55%). Both pause axes are knobs now —
> **DENS** (how many, via a per-slot threshold array so rests arrive in a fixed musical order rather than
> randomly) and **GAP** (how deep) — five knobs on the page while SPK is lit, which is safe because
> `lcdknob_cell` is height-bound here so 4→5 does not shrink the knob.
> Two bugs worth remembering, both inaudible as bugs and both caught by the `DE_TRACE` watches, not by ear:
> simultaneous notes on the two 303 lines **double-advanced** the word and skipped slots (a skipped rest is a
> pause that never happens), and the gate's **symmetric** close could not finish inside one note, so the pause
> was an inaudible dip (it is asymmetric now — snap shut, ease open, which is what a stop consonant is).
> **▶ TOP OPEN — needs an EAR CHECK (not code), the reason we stopped:**
> **(1) SPEAK IS HARD TO HEAR** — the maker's verdict 2026-07-29. The mechanism is PROVEN (clip 06 covers it) so
> do **not** re-verify it; this is voicing/routing. *Check the dumb thing first:* both 303s boot **muted** and SPEAK
> is driven entirely by 303 note-ons, so with both muted there are zero syllables and you hear only a static vowel
> on the drums. Then the top lead, which the measurement already predicted: speak-on vs speak-off left centroid std
> unchanged because the 808+909 kit dominates the variance — so move the formant OFF the master bus onto the 303s
> only (`instrument_formant` on slots 6/7 + subs 36/37), which also frees MIX to go well above 0.7 without eating
> the hats. `--solo-slot 6` decides it in one command: speech clear in the stem = masking (reroute); weak even solo
> = the vowel contour itself is too subtle. **(2) the GATE lane** — the maker's verdict was *"none is a
> clear win"*: `gate()` is a THRESHOLD/dynamics gate, not a pure per-step volume chop, so the cut is
> level-dependent (mushy). The real fix for a clean trance-gate = wire a **master VOLUME** the GATE lane can
> ride (the long-standing `level[]` master-vol TODO). CRUSH is the keeper of the two new lanes.
> **Resume at the cart's live punch-list — the `de:meta.todo[]` in
> [`tools/carts/acidcandy.c`](../tools/carts/acidcandy.c)** (`node tools/cart-todos.js acidcandy`); the newest
> entries (DUB / GATE / CRUSH / PERF / LCD-GROW) carry the mechanism + every caveat above.
> **303 realism pack — ALL WIRED (2026-07-20):** the `runtime/acid303.h` features are all now surfaced in
> acidcandy — continuous RES, analog DRIFT (FX-panel knob, rides live), per-303 classic⟷DF **voicing** switch
> (with SAW/SQR reachable in vanilla), and the per-303 **CLEAN/RAW** saw toggle (FX panel; opt-in PolyBLEP,
> raw is default). Engine: `instrument_bandlimit` API + `Acid.classic/clean/drift` (all non-destructive struct
> fields). Story + rationale: [`audio-notes.md §26`](design/audio-notes.md#26-303-realism--it-sounds-kinda-digital-2026-07-19).
> Other OPEN there:
> PERF follow-ups (the 2X **funny-accent-order**, octave-shove + reverse lenses, a drums PERF layer), the
> **REC/mode hint-outlines** teaching idea, the **mascot/soul** (deferred), SAVE/LOAD + the SONG layer.
> (PARKED: mute scenes — redundant with the mute LEDs; the LCD-toast feedback idea — maker didn't like it.)
> Hot files: `tools/carts/acidcandy.c`, `runtime/tr808.h`, `runtime/tr909.h`, `runtime/acid303.h`. Design:
> [`device-face-paradigm.md`](design/device-face-paradigm.md) · [`candy-style.md`](design/candy-style.md) ·
> [`control-vocabulary.md`](design/control-vocabulary.md). Cousin lane: the acidrack redesign (R5,
> `disclose.h`) below — acidcandy is the candy device-face *take*; that lane re-lands acidrack itself.

> **⏸ PARKED (last field note 2026-07-18) — demand discovery, the reddit-gaps drip.** The 6 h LaunchAgent
> keeps mining and the caches keep growing (24 tribes, 1,411 wishes clustered), so this is a STANDING
> PROCESS rather than in-flight work; the tombola toy it produced shipped 2026-07-14. Resume — including
> where the findings live and how to drip the next tribe — at
> [`design/demand-discovery.md`](design/demand-discovery.md#where-the-findings-live-and-grow).

> **⏸ PARKED (last touched 2026-07-07) — responsive instrument UI.** The playbook, ADR-0028, the epianofit
> mock and the `scalegrid` cart (device-tested, spec 71/0) all shipped. The single open step is extracting
> the grid into a `grid.h` library — **`runtime/grid.h` does not exist** and nothing has moved since, so
> this is dormant, not nearly-done. Resume at [`design/scale-grid.md`](design/scale-grid.md).

> **⏸ FOLDED INTO THE `face.h` LANE (2026-07-30) — device-adaptive layout / the acidrack redesign.**
> Phases 0–2 shipped (`runtime/lay.h`, the resizable canvas, iOS fill/safe-area/rotation), R2
> (`runtime/disclose.h`) and R3 (`finger_px()`/`device_class()`) shipped and verified on device, and the
> `acidwire` wireframe did its job (field note 020). **But `acidrack.c` has not moved since 2026-07-14 and
> `disclose.h` since 07-10, while the tablet answer actually shipped through `face.h` + ROOMY in acidcandy
> — so this and the faces lane were two lanes describing one thread.** R5 (port acidrack onto `disclose.h`
> + `finger_px()`) is still open and now rides the faces lane above. Scoreboard:
> [`device-adaptive-layout.md` → Where this stands](design/device-adaptive-layout.md#where-this-stands).

> **▶ ACTIVE THREAD (2026-08-18) — store / ASO + the app-trailer builder.**
> **LATEST (2026-07-19) — Tiny Acid Jam is LIVE ON ASC as a draft (the FIRST standalone single).**
> Per [`design/launch-sequence.md`](design/launch-sequence.md)'s single-first plan, `acidcandy` ships
> as its own app **Tiny Acid Jam** *before* the Tiny Jam umbrella. Renamed this session from "Acid
> Candy" (the old name collided with candy-match-3 games — poisoning the ASO seed — *and* a real
> sour-candy brand; "Tiny Acid Jam" pre-brands into the umbrella). The **cart slug stays `acidcandy`**
> (no provenance churn) — only the app + the cart's display title changed.
> Done + committed: `apps/acidcandy` → **`apps/tinyacidjam`**; bundle **`com.mipolai.tinyacidjam`**
> (registered in the portal); **ASC app record `6792504925` CREATED** (name reserved globally, private
> "Prepare for Submission" draft, v1.0 en-US); listing PUSHED live + verified in-sync via
> `node tools/asc-push.js tinyacidjam --metadata` — title **"Tiny Acid Jam: 303 Groovebox"** (28/30) /
> subtitle **"808, 909, house in your pocket"** (30/30) / the machine-numbers keyword field; `seo-brief.md`
> regenerated on **genre seeds** (acid house/303/groovebox/techno), dropping the candy vocabulary.
> **Price DECIDED = $1.99** (cheap-paid, one-time; .99 charm point, low friction for a no-reviews first
> launch) — recorded as `"price"` in `app.json`, BUT the base price is settable via `node tools/asc-push.js <app> --price` (was a manual ASC step) (Pricing &
> Availability tab — `asc-push` pushes IAP prices only, and this app has no IAPs).
> **▶ SUBMITTED 2026-08-17 — all four of the blockers this lane listed are closed.** v1.0 is
> `WAITING_FOR_REVIEW` with build `202608162137`. For the trail: (1) the description landed
> 2026-07-22, (2) five device screenshots the same day, (3) `ios/testflight.sh` archives it with the
> AU inside, (4) price/age-rating/privacy URL all set. **Resume-at:** nothing until Apple answers;
> then `apps/tinyacidjam/app.json` + [`design/launch-sequence.md`](design/launch-sequence.md)
> "For Tiny Acid Jam specifically".
>
> **Buy-screen crash FIXED (2026-07-06, commit `07690c9b`):** the "instant, random" abort on the
> Tiny Jam menu/purchase screen was a **data race** — `Store.unlockedIDs` (a Swift `Set`) read by the
> C entitlement gate every frame while a StoreKit `Task` reassigned it → nano-heap corruption surfacing
> later at an unrelated `malloc`. Never reproduced off-device (desktop stubs `Store_*`). Now
> `NSLock`-guarded. Full lesson in `ios/README.md` §Gotchas — any per-frame `@_cdecl` bridge must be a
> lock-guarded snapshot, never a bare Swift collection.
> **Store-identity day (2026-07-06), all committed:** the App Store name **"Tiny Jam: Pocket
> Music Toys" is RESERVED** on App Store Connect (record created, not public); shipping bundle id
> is **`com.mipolai.tinyjam`** (registered in the dev portal; `apps/tinyjam/app.json` updated —
> the `com.tinyjam.hello` in `ios/project.yml` is dev-loop-only, see the comment there); the
> manifest **`icon` key is live** (`build-app.js --ios` → single-size asset catalog, sim-verified
> in `Assets.car`); **`ios/testflight.sh` RAN TO COMPLETION (2026-07-06)** on the upgraded box
> (macOS 26.5 + Xcode 26.6 at `/Applications/Xcode26_6.app`): **v0.1 build 202607061929 uploaded
> to App Store Connect** (cloud-signed Release, name reservation cemented) — next store step is
> ASC → TestFlight once it clears Processing. Toolchain wobbles found + fixed on the way:
> (1) `open -a Simulator` launches the STALE Xcode 15.1 copy in ~/Downloads and dyld-crashes —
> open Xcode26_6's Simulator.app by path; (2) the **iOS 26 sim runtime killed in-app
> `SKTestSession`** (needs a real XCTest run context now, not just XCTest loaded — dlopen tricks
> don't help; the 17.2 runtime was auto-deleted in the upgrade) — `Store.swift` now skips local
> IAP testing gracefully (gated to iOS 26+); **the sim purchase dev-loop lives on an 18.x
> runtime device** — iOS 18.4 runtime installed + `DEVICE="iPhone 16 (18.4)" ./build.sh`
> VERIFIED purchases working (2026-07-06). Device IAP testing still waits on ASC IAP records
> (Monetization → In-App Purchases; the bundled .storekit only covers the sim).
> (A separate lane from the one above.) A big session. Shipped, all committed to `master`
> (local — **push to sync other machines**):
> - **The free ASO keyword loop** (CLI + Apps tab): `aso-research` (now mines competitor
>   *descriptions*, doc-frequency ranked) · `aso-suggest` (free Google-autocomplete demand proxy) ·
>   `aso-compose` · `aso-lint` · **`aso-brief`** (palette — a committed, drift-tracked
>   `seo-brief.md`) · **`aso-coverage`** (mirror — coverage + stuffing) · **`aso-score`** (terminal
>   scoreboard + A/B tweak, `--deep` = winnability). Loop: research/suggest → brief → *you write* →
>   coverage → compose/lint/score; **no step writes prose**.
> - **Apps-view surface:** the sell row (📝🔎💡🧩🔬📊✅🪞) + IAP copy (char badges) + clickable
>   keyword "keys" + all-keys→research + load-into-all-tools.
> - **Strategy reframe:** [`design/demand-generation.md`](design/demand-generation.md) — capture
>   (ASO, the tail) vs generation (video/tribe, the wave); grab a **tribe**, not the world.
> - **The trailer builder** ([`design/trailer-builder.md`](design/trailer-builder.md)): backbone
>   `tools/build-app-reel.js` (manifest carts → one reel; Tiny Jam = 3-rack) **+ editor v1 (A)** —
>   the Apps-card **🎞 trailer** section, a **non-destructive** click-to-edit timeline (library, ◀▶
>   reorder, transition-at-join, Build → bake+compose → preview; edits only the `.reel`).
>
> **Resume at:** the maker-gated **store submission track** (see the 2026-07-07 update at the foot of
> this lane for the live pick-up — trim + speed already SHIPPED, engine + editor + live preview, per
> [`trailer-builder.md`](design/trailer-builder.md)). **Full snapshot + next:** the pick-up point in
> [`store-agents.md`](design/store-agents.md#pick-up-point-next-session). Orient: `node tools/topic-brief.js "aso"
> "trailer" "demand"`.
> **Editor note:** this lane changed `editor/electron/main.cjs` + `preload.cjs` (new IPCs:
> aso-score, app-clips, build-reel, app-seeds, aso-suggest/brief/coverage) — **restart Electron
> (`make`) to light them up**; `shell.js`/CSS/`index.html` hot-reload. All CLI tools work now.
> **Update 2026-07-07 — the ASC upload tool is BUILT: `tools/asc-push.js`** (the store track's
> "one big unbuilt piece", ADR-0026). In-house against the App Store Connect API, zero deps, proven
> LIVE against Tiny Jam: **keywords + app screenshots pushed**, and **all 3 IAPs created →
> localized → priced → availability → review-shot → `READY_TO_SUBMIT`**. `--metadata`/
> `--screenshots`/`--iap`/`--dry-run`/`--check`. Auth in `~/.appstoreconnect/` (`.p8` + `config.json`,
> never git; `*.p8` gitignored). **Also this session:** the IAP product ids were renamed to the
> bundle-nested scheme **`com.mipolai.tinyjam.{acidrack,epiano,masterpass}`** (was `com.tinyjam.*`;
> rebirth→acidrack) across `app.json` + `Store.swift`/`canvas.c`/`Tinyjam.storekit` + the two iOS
> tests, and the `.storekit` was resynced to the manifest (dropped a phantom "funk", fixed the master
> pass $19.99→$5.00) — **purchase flow re-verified on the iPhone 16 (18.4) sim**. **Resume at:** the
> credentials are set up (Key `Z5DTR9TFW2`); next store moves are per-locale `metadata/<locale>/`
> folders + an editor button for `asc-push`, then the maker-gated submission. Snapshot in
> [`store-agents.md` → Pick-up point](design/store-agents.md#pick-up-point-next-session).
> **Update 2026-07-08 — metadata channel is submission-complete + has an editor button.** Scaffolded
> `apps/tinyjam/metadata/en-US/` (description + promo from press.md, `support_url` →
> https://mipolai.com/tinyjam/support/ live). Built the **☁︎ App Store panel** on the Apps card
> (`asc-push.js` gained `--json`/`--only`; `studio:asc-metadata` IPC): two-click ceremony — dry-run
> diff → per-field checklist → push only ticked fields. **Promoted-purchases channel DONE too**
> (2e35fa03 + f8bd613f + 4697b11c): the ☁︎ panel now has a second section for `asc-push --promote`
> (`--json`/`--only` backend + `studio:asc-promote` IPC), each IAP a row with its promoted state,
> its own ★ Promote button. All 3 tinyjam IAPs already promoted, so it reads "✓ all promotable IAPs
> already promoted". **NEEDS ELECTRON RESTART** (`make`) — the ☁︎ panel (both sections) is verified
> at the data layer but not yet eyeballed live. **Naming stays honest:** this is *promoted purchases*
> (App Store search), NOT the editor [Promote tab](design/promote-tab.md). **Parallel-agent note:**
> the editor JS half was swept into f8bd613f (the Promote-tab agent's commit) — reconciled clean, my
> full two-section panel is in HEAD. **Resume at:** eyeball the ☁︎ panel after `make`; screenshots
> channel is the next unbuilt one (deferred by the maker until there's more screenshot tooling).

> **⏸ PARKED (tool unchanged since 2026-07-07) — `leads.js`, the local marketeer.** Built and ledgered in
> [`STATUS.md`](STATUS.md); the taxonomy has grown through USE (34 tribes today) but the tool itself has
> not changed. Open: the editor Apps-page surface. Resume at
> [`design/leads-marketeer.md`](design/leads-marketeer.md#open-questions-resume-at).

## History & reference (pruned 2026-07-05)

The old session narratives, shipped-feature bullets, and todo list that lived here are ledgered
elsewhere — trust those homes, not a handoff file:

- **Shipped / open / cut** → [`STATUS.md`](STATUS.md) + the design board (`design-board.html`).
  Backlog → the board's READY-TO-BUILD column (`node tools/design-board.js`); the old todo items
  all live on it or in [`design/api-notes.md`](design/api-notes.md).
- **Web build deep reference** (web-specific behaviour, emcc flags + why, how `runtime/raylib-web/`
  was built from source) → moved to [`guides/exporting.md`](guides/exporting.md) §5.
- **Cart format** (`.cart.png` zTXt chunks) → [`design/cart-metadata.md`](design/cart-metadata.md) +
  [`guides/cart-authoring.md`](guides/cart-authoring.md). Editor internals worth remembering: chunk
  helpers are `embedCartChunks`/`extractCartChunks`/`makeZtxtChunk`/`crc32` in
  `editor/electron/main.cjs`, duplicated standalone in `tools/make-cart.js`; the
  `preload.cjs` IPC surface is `studio.saveCart/loadCart/loadCartFile/loadCartBuffer/getFilePath`
  (Electron 32+ for the last); dropping a `.png` on the window loads it as a cart; `--screenshot`
  on a cart binary renders 3 frames and exits.
- **Cart authoring quick reference** → [`guides/cart-authoring.md`](guides/cart-authoring.md)
  (and CLAUDE.md's "Adding a cart" steps).
- Debug-tools design notes archive → [`archive/debug-printh-watch.md`](./archive/debug-printh-watch.md).

---

## Gotchas / environment facts

- **Display asleep = every play.js/make-cart run SEGFAULTS in `rlglInit`** (signal 11, empty
  trace) — Raylib needs a live GL context even `--headless`, so late-night unattended renders
  suddenly "break the engine" when the screen locks. It's not your edit. Fix:
  **the harnesses now do this themselves** — `play.js`/`spec.js`/`make-cart.js` each fire `caffeinate -u -t 1` before the `-dims` wrapper, so you only need it by hand when driving a cart binary directly. Full note in [`guides/debug-harness.md`](guides/debug-harness.md#gotcha-a-sleeping-display-segfaults-every-harness-run). Discovered mid
  filter-spike 2026-07-02 (audio-notes §25), took out a parallel agent's runs too.

- **`main.cjs` / `preload.cjs` changes need an Electron restart** (`npm start`);
  Vite hot-reloads everything else.
- **`▶ run` only works in Electron** (it spawns clang); the browser tab edits but can't run.
- Use **Node 22** (`nvm use 22`) before any npm command.
- **`--screenshot` mode** opens a real window briefly (Raylib needs a display).
  3 frames is enough for static carts; carts that randomise initial state on frame 1
  will look fine; carts that need user input obviously won't show gameplay.
- **arm64 integer divide-by-zero does NOT trap** (returns 0) — SIGFPE won't fire on
  Apple Silicon. Use a `volatile` null read for reliable test crashes.
- **`save()`/`load()`** write `cart.sav`/`cart.kv`/`cart.blob` into a per-cart folder:
  the editor and `play.js` pass `--save-dir saves/<cart>`, so saves live under
  `build/saves/<cart>/` (2026-06-04; previously all carts shared `build/cart.sav`).
- **`print_centered`/`print_right`** use `strlen(text) * 8` for width — the dos_8x8
  font is exactly 8px per character with 0 spacing (`DrawTextEx` size=8, spacing=0).
- **`rnd_float()`** uses `rand()` from `<stdlib.h>` (added to studio.c includes).
  All carts share the same `rand()` seed — not seeded per-cart; same sequence every run.
- **CLAUDE_CODE_TMPDIR fills up occasionally** — compile/emcc output gets lost when the
  tmp partition fills. Workaround: redirect to a real file and read it back —
  `> build/compile-test.log` for clang, `emcc ... >/tmp/emcc.log 2>&1` for web builds.
- **`trifill()` winding order** — Raylib's `DrawTriangle` needs counter-clockwise winding
  in Y-down screen coords. In Y-down space, cross product > 0 means clockwise visually
  (opposite of math convention), so swap when `cross > 0`.
- **`rrectfill()`/`rrect()` 1px-narrow-on-right-and-bottom bug — FIXED 2026-07-17.** Root cause was
  a stray `-1` in `rrect_inside()` (`runtime/studio.c`): the right/bottom corner-centres were
  `x+w-1-r`/`y+h-1-r` while left/top were `x+r`/`y+r` — asymmetric, so with pixel-centre sampling the
  straight edges landed at `x+w-2`/`y+h-2` instead of `x+w-1`/`y+h-1`. Changed to `x+w-r`/`y+h-r`
  (inscribe in `[x,x+w]×[y,y+h]`), so `rrect`/`rrectfill` straight edges now coincide exactly with
  `rect`/`rectfill` at identical `w,h`. Verified: a probe reads matching right/bottom edges;
  `canvas-diff` PASS (GPU==SW) on `raster_test` + `acidcandy`. **MIGRATION:** any cart that
  compensated with the old `w-1` trick now renders 1px SHORT — remove it. Done: `acidcandy` voice-band
  (line 440, `153`→`154`). If you find another `w-1`-to-align-with-rrectfill comment, undo it.
- **`init()` fires after window + sprites are fully loaded** — safe to call `colorkey()`,
  `mset()`, etc. It does NOT run during `--screenshot` mode's 3-frame early exit, but
  that's fine since screenshot mode still calls it once before the loop.
- **Raylib auto-detected:** `/opt/homebrew/opt/raylib` (Apple Silicon) or
  `/usr/local/opt/raylib` (Intel). Both `main.cjs` and `tools/make-cart.js` do this.
- **Web build server stays alive** — `startWebServer()` in `main.cjs` keeps one
  Node HTTP server on port 8765 alive for the editor session. It reuses the same
  instance on subsequent builds (doesn't restart). If something is already on 8765
  externally, kill it: `kill $(lsof -ti:8765)`. emcc progress goes to the runtime log
  panel; errors to the build log panel.

## Working preferences observed

- **Respect day/night theming** — use CSS vars (`--bg`, `--bg2`, `--fg`, `--fg-dim`,
  `--border`, `--accent`, `--font`), never hardcode panel colors.
- **Panels auto-hide when empty** (build log 3s timer, runtime log on clean exit).
- **Optimize for beginner legibility** — visible mistakes are a first-class goal.
- Commits go **direct to `master`** (solo repo).
