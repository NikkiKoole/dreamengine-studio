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

_Last updated: 2026-07-19 (thirteen lanes; newest activity = **Tiny Acid Jam shipped to App Store Connect** — `acidcandy` renamed + published as the first $1.99 standalone single, ASC record `6792504925` created + listing pushed live; see the **store/ASO** lane. Newest lane = **walkbox — a walking-bass step-sequencer** (`walkbox`) — a TB-303 workflow on the upright's real `INSTR_BOWED` pizz voice: draw a line on scale-locked note-bars, sculpt a tabbed VEL/LEN lane (velocity → pluck attack; length → staccato gate, top = TIE), flip SLD/OCT rows, dial SWING; the "303 done as a real pluck". Core + articulation ship and play; resume [`design/walkbox.md`](design/walkbox.md). Older: **the chord-bloom rack** (`chordblossom2`) — an instrument you PLAY (chords with your hands) wearing a genre FLAVOR (a backing band that follows you), the winning direction over the radio-rack `bossabloom` (which felt like "radio + UI"); Flavor table NEUTRAL/BOSSA/YACHT/CITYPOP · diatonic KEY mode (keys = the in-key chords) · 3-axis SPICE/RICHNESS/GROOVE · living 4-bar-phrase accompaniment · tiny per-part PEDALBOARD (FX tab) · STOP+REST; NEXT = tune yacht/citypop + per-flavor spice + candy skin, resume `design/bossa-rack.md`. Older: **the Android build target** — the engine runs frameworkless behind `platform.h` (`DE_NO_RAYLIB`), so Android is a host shell + Gradle packaging: spikes 0–3 SHIPPED (NDK compile → GLES2 render → AAudio → touch on an arm64 emulator), the `android/` scaffold + `android/build.sh`, and the editor's **🤖 export .apk** button (sideloadable, no Play account/device); NEXT = save → signed `.aab` → Play Billing → multi-cart app; resume `design/android-plan.md`. Older: **the candy acid RACK** — `acidcandy` packages `acidrack`'s guts (2×303 + 808 + 909 + master) as focusable candy device-faces at 160×100; `tr808.h`/`tr909.h` extracted byte-honest; note-bar free-draw + a flag palette + a drawable PCF + a drag-release-bounce fix; NEXT = 808/909 voice completeness / drum depth / arrangement / the mascot — resume the cart's `de:meta.todo[]`. Older: **demand discovery** — `tools/reddit-gaps.js` mines a music tribe's RSS for unmet demand, 2 tribes done (notes 022/023; thesis = a cheap playful lo-fi toy in classic-gear clothes), a 6 h LaunchAgent drip grows caches in `tools/reddit-gaps-cache/`. Older: **editor↔cart workflow** — Gap 1b (`de:meta.slug`) + Gap 1 (code save-back) SHIPPED & committed: slug backfilled across all 455 carts + `lint-carts.js` REQUIRES it; the **"save to source" button** writes the Code-tab buffer back to `tools/carts/<slug>.c` + rebakes in place; and the **PIXEL side (Gap 2) Option D SHIPPED** — reversible slot-level sprite PATCHES for generator carts (`tools/lib/sprite-patch.js` core + `make-cart` composite-on-bake + editor save-to-source diff; `de:spritepatch` chunk), so hand-edits survive the next CLI bake; NEXT = eyeball live + a discard button → resume `design/editor-cart-workflow.md` Gap 2 · worldgen ladder — rungs 0–7 SHIPPED (district fill + Rotterdam calibration via `sndi-check --compare`; `citygen.h` extracted → sloop's M drives a generated city w/ collidable buildings) + the junction grammar `roadkit.h` (B2 pure geometry + B3 field renderer, streetlab byte-identical; citydrive draws curb-return junctions through it, J); NEXT = B4 grade dispatch → roadlab interchanges / the N+M infinite-world reconciliation · multiplayer — rung 5b WebRTC P2P BUILT + published (pong live on github.io; ~12ms direct over wifi, relay = signaling only); step 5 (adaptive NET_DELAY) + step 7 (TURN) PARKED; wire-side diagnostics SHIPPED 07-10 (RTT probe + rx-gap + wall-clock logs + web-tick stalls) → next office visit = the checklist in multiplayer-research.md · device-adaptive-layout — the acidwire WIREFRAME CART is built + INTERACTIVE + runs on the iOS sim (dual-mode desktop/device, 4 states incl. focus, touch+mouse, real 303 piano-roll + drum grids, iPad 2×2 grid both orientations, finger-honest); guide interactive-wireframes.md; next = play on glass → narrow-303 input model → R5 re-land into acidrack.c · store/ASO — the ASC upload tool BUILT (`tools/asc-push.js`, ADR-0026): keywords + screenshots pushed live, all 3 IAPs `READY_TO_SUBMIT`; product ids renamed to `com.mipolai.tinyjam.*` (sim-reverified) · editor media — the per-cart **Promote tab SHIPPED (A–E)** + the **shared-popup pattern** (trailer + keyword-research popups, opened from cart AND app) + reels save/load (subject-scoped strip + cross-subject overview) + multi-resolution export (output-ratio picker on reel-Build + clip-bake, Stage-2 per-ratio variants that FILL, App Store even half-sizes); `export-ratios.md` stages 1+2 SHIPPED + the `onetake` proof cart; keyboard shortcuts = the enabler; NEXT = an EYEBALL PASS (none clicked live) + the fixed-layout composite gap · responsive instrument UI — the playbook + ADR-0028 + the epianofit mock shipped; epiano brief re-scoped to the FAITHFUL piano; the scale-grid SHIPPED as the `scalegrid` cart (device-tested, spec 71/0), open step = extract it into a `grid.h` library then wire epiano's editor-swap · leads — the marketeer tool + ledger BUILT (`tools/leads.js`, 18 tribes): cart→tribe→venues, taxonomy being filled cart-by-cart, editor Apps-page surface next → resume `design/leads-marketeer.md`)_

---

## Where we are right now

**Thirteen lanes are active in parallel right now** (different areas — pick the one you're resuming):
(12) **walkbox — a walking-bass step-sequencer** (`walkbox` — a TB-303 workflow on the upright's real pizz voice),
(11) **the chord-bloom rack** (`chordblossom2` — play chords with your hands, a genre FLAVOR band follows),
(10) **Android build target** (host shell + Gradle → Play; spikes 0–3 + editor 🤖 export .apk shipped),
(0) **the candy acid rack** (`acidcandy` — `acidrack`'s guts repackaged as candy device-faces),
(1) **the worldgen ladder** (realistic procedural roadgen), (2) **multiplayer — WebRTC P2P (rung
5b)**, (3) **device-adaptive layout**, (4) **store / ASO + the app-trailer builder**, (5) **editor
media (record/replay + where it lands)**, (6) **responsive instrument UI + the scale-grid**,
(7) **leads — the local marketeer** (find venues to post + track outreach), (8) **editor↔cart
workflow: cart provenance (`de:meta.slug`) + the save-back round-trip**, and (9) **demand discovery
— the reddit-gaps drip** (mine a tribe's RSS for unmet demand; caches grow via a 6 h drip). All
below; none is "the" thread. Shipped/open ledger for all: [`STATUS.md`](STATUS.md) + the design board.

> **▶ ACTIVE THREAD (2026-07-18) — walkbox: a walking-bass step-sequencer ("303 done right, as a real pluck").**
> A TB-303 workflow driving the upright's real `INSTR_BOWED` pizz voice — the cell nobody had
> (`acidcandy`/`tb303` = the 303 workflow on a *synth* voice; `upright` = the pluck voice *played
> live*; **walkbox = 303 workflow + real pluck**). SHIPPED this session, all committed, cart plays:
> note-bars (draw the line, scale-locked E min-pent) + a **tabbed VEL | LEN drawable lane** (velocity
> → pluck attack `note_on` vol; length → staccato gate, top of lane = TIE/ring-on) + **SLD/OCT** rows
> + **SWING** (odd-16th shuffle, a port of tb303's clock) + GLIDE/TEMPO/TONE/RING + RND/CLR. Voice =
> mono pizz re-pitched per step; SLIDE = same-voice `note_glide` slur (both directions); TIE detaches
> the voice to ring on. Banked gotcha: `ui.h` `UI_MAX_WID=64` silently drops later widgets → use
> **one widget per row/lane**, compute the column from touch x (per-cell widgets killed the TIE row +
> knobs). **Resume-at:** [`design/walkbox.md`](design/walkbox.md) — the articulation roadmap (open:
> ghost-as-a-muted-timbre, hammer/pull, scoop/fall, presets + save/load) and the **modern-bass
> voice-swap** direction (the lanes are voice-agnostic; swap the pizz for an electric/sub voice).
> Hot file: `tools/carts/walkbox.c` (self-contained — no shared-header edits).

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

> **▶ ACTIVE THREAD (2026-07-16) — Android as a Google-Play build target.**
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
> NEXT = spike 4 (save → internal storage) → 5 (signed `.aab` for Play) → 6 (Play Billing via the
> existing `Store_*` gate; needs a Play Console account) → 7 (`build-app.js --android` multi-cart app).
> Hot files: `android/app/src/main/cpp/main.c` (the NativeActivity shell), `android/build.sh`,
> `docs/design/android-plan.md`.

> **▶ ACTIVE THREAD (2026-07-18) — the audio-input frontier (the engine HEARS *and SPEAKS*).**
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
> **Resume-at: [`design/audio-input-frontier.md`](design/audio-input-frontier.md) — the ranked map.** Auto-tune
> arc is COMPLETE for the offline feature; the LIVE path is feasible-and-parked (warble). Open frontier, ranked:
> (1) the **pedal tier / live looper** (the `sound_extin` ring is the built foundation — the loudest unmet wish);
> (2) **vocoder v2 tail** — mic-rate resample (non-44.1k device mics drift the ring) + on-device latency tuning;
> (3) **beatbox→live drum trigger**; (4) **live-autotune warble** if revisited (real-time YIN / phase vocoder).
> `voxroll` decouples formant/pitch but only on synth `INSTR_VOICE`, not a real-mic corrector.
> Hot files: `runtime/sound.h` (vocoder + extin ring + `sample_autotune`/`autotune_mic`), `runtime/mic.h`
> (analysis + record + ring write), `runtime/studio.h`/`.c` (the seam + `DE_MIC_WAV`), the per-host capture
> (`mic_desktop.h`, `ios/Sources/AudioEngine.swift`, `android/…/main.c`). Reference carts: `vocode`/`voxbox`
> (vocoder), `mictune` (offline auto-tune), `livetune` (live spike). Test harness: `DE_MIC_WAV=<wav>` + `tools/testdata/`.

> **▶ ACTIVE THREAD (2026-07-19) — the candy acid RACK (`acidcandy`).**
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
> — **303 PERF lenses** (a PERF soft-key: HALF/2X speed · ACC accent-all · STAC/GLIDE slide-flip),
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
> — **a DUB pad** in the freed MST corner — a momentary XY (X = delay time, Y = feedback) that throws the delay
>   while held, snaps back on release.
> **▶ TOP OPEN — needs an EAR CHECK (not code), the reason we stopped:**
> **(1) the DUB pad** — `echo()` is NOT ride-safe, so a time-sweep RE-TIMES the delay per step; a fast sweep may
> STEP/click instead of gliding. Listen: hold DUB + sweep X with feedback up. If it clicks → quantize `et` to
> fewer steps, or find a ride-safe delay-time path. **(2) the GATE lane** — the maker's verdict was *"none is a
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

> **▶ ACTIVE THREAD (2026-07-13) — demand discovery: the reddit-gaps drip is running.**
> A new tool (`tools/reddit-gaps.js`) mines a music tribe's public RSS for unmet demand → clusters →
> cross-references our cart shelf → ranks gaps. TWO tribes done
> ([note 022 · ipadmusic](field-notes/022-demand-discovery-ipadmusic.md),
> [023 · synthesizers](field-notes/023-demand-discovery-synthesizers.md)) — convergent thesis: the
> opening is a *cheap, playful, beginner lo-fi toy in classic-gear clothes*, NOT a feature.
> A macOS LaunchAgent **drips one sub every 6 h** (rotation: `tools/reddit-gaps-subs.txt`), so the
> caches KEEP GROWING between sessions.
> - **Where the freshly-downloaded data lands:** `tools/reddit-gaps-cache/<sub>.json` (gitignored —
>   regenerable, never committed per Reddit policy) + the run log `tools/reddit-gaps-cache/drip.log`.
> - **To see what's been collected next time:** `ls -lt tools/reddit-gaps-cache/` (newest first),
>   then `node tools/reddit-gaps.js <sub>` renders the gap report from cache, or `--raw` dumps the
>   mined wish list. `cat tools/reddit-gaps-cache/drip.log` shows the drip's history.
> - **Resume-at:** when a sub's cache looks rich, read `--raw` and write the next numbered field note
>   (the interpretation is a judgment step, not automated) — pick-up point in
>   [`design/demand-discovery.md`](design/demand-discovery.md#where-the-findings-live-and-grow).
> - Hot files: `tools/reddit-gaps.js`, `tools/reddit-gaps-subs.txt`. Drip stop/start/fire-once
>   commands are in that doc's "Continuous fetching — the drip" section.
> - **First build-output of the thread → the tombola cart** ([`design/tombola.md`](design/tombola.md)):
>   the demand's one genuinely-missing on-grain gap (tape=already `loopstation`, chord-toy=already
>   `chordblossom`), designed as a physics sequencer on the [device-face paradigm](design/device-face-paradigm.md)
>   (§1f = the fuller OP-1 device analysis for the paradigm-sharpening pass). **Not built.** When
>   building: reuse the new `runtime/physics.h` (verlet balls) + `circlemachine` (note wiring) — the
>   tombola only adds a rotating drum + trigger-line — resume at [`design/tombola.md`](design/tombola.md#prior-art-in-the-repo-reuse-and-what-s-already-covered).

> **▶ ACTIVE THREAD (2026-07-10) — editor↔cart workflow: CODE round-trip + the PIXEL side (Option D) SHIPPED.**
> Closing the gaps that bite when you hand-edit a cart in the editor instead of going through
> `tools/carts/` + CLI (all in [`design/editor-cart-workflow.md`](design/editor-cart-workflow.md)).
> **Gap 1b (provenance) + Gap 1 (code save-back) are DONE and committed:**
> - **`de:meta.slug`** backfilled across all 455 carts (`tools/backfill-slug.js --write`) and
>   **required** by `lint-carts.js` (missing or `slug !== filename` fails). Slug is the PNG→source
>   anchor. (Bug caught + fixed in the backfill: it built the `.replace()` replacement as a STRING, so
>   `$1`/`$&` in a cart's own de:meta expanded — `mule.c`'s `$100` → invalid JSON; now a replacement
>   FUNCTION. Blast radius was 1 cart, repaired.)
> - **"save to source" button** (editor): writes the Code-tab buffer back to `tools/carts/<slug>.c`
>   and re-bakes the gallery `.cart.png` IN PLACE (keeps thumbnail + sprites/map), regenerates
>   `index.json`. IPC `cart:save-to-source` in `main.cjs`; `saveToSource()` + busy-toast feedback in
>   `shell.js`. Source only; NOT a git commit (stays `node tools/cart-commit.js <slug>`). Key gotcha
>   baked in: it re-embeds like `cart:save` in-place — does NOT shell out to `make-cart.js <src> <png>`,
>   which rebuilds sprites from the `.cart.js` and would BLANK a hand-drawn sheet (make-cart.js ~378).
> - Also shipped: a Settings → "cart panel" toggle to hide the save/load/save-to-source row.
> **PIXEL side (Gap 2, the sprite story = STATUS item 23) — Option D SHIPPED (bake + persist halves).**
> The finding that **all generator carts are `.cart.js`-driven (ZERO hand-drawn)** meant the real need was
> *reversible touch-ups on a generator cart* → straight to D (not A's freeze / B's marker). Committed:
> - **`tools/lib/sprite-patch.js`** — the slot-level overlay CORE (palette-index space, no PNG decode):
>   `fingerprintSlot`/`fingerprintSheet` (over the generator OUTPUT, not the source), `applyPatch`
>   (fast-path → per-slot base-match → pure-addition-over-empty → else STALE: drop loudly + prune, so a
>   wholesale regenerate self-empties), `buildPatch`, stable serialize. Tested 26/26 core + 8/8 bake-int.
> - **`make-cart.js`** — `buildSpriteSheet` split into `genSlots`+`slotsToSheetPng` (byte-identical, verified
>   on 4 real carts); `bakeSprites()` composites a sibling `tools/carts/<name>.sprites.patch.json`; the
>   `<src> <png>` bake mirrors the surviving patch into the `.cart.png` as `de:spritepatch` (`--run` preserves it).
> - **Editor save-to-source** (`main.cjs cart:save-to-source` + `shell.js readSheetSlots`/`saveToSource`) —
>   diffs the sprite canvas vs the RE-RUN generator, writes only changed slots (deletes the file when nothing
>   differs). Stateless (generator re-runnable → no snapshot). Data-layer-validated on 5 real carts (untouched
>   sheet ⇒ empty patch, no spurious diffs); **NOT yet eyeballed live** (needs `make` to restart Electron).
> **Save-path VERIFIED LIVE (2026-07-10, maker)** — hand-edit → save-to-source → patch persists across `--run`.
> **Discard + indicator BUILT (2026-07-10, data-layer tested 5/5, NOT yet eyeballed live):** the pixels tab shows
> a `#sprite-patch-bar` naming the hand-owned slots on load (load handlers thread `de:spritepatch` →
> `applyCart` → `setSpritePatchBar`) + a **"discard hand-edits"** button (`cart:discard-sprite-patch`: delete
> sibling → re-run generator → drop the chunk → reload canvas). **NEXT:** (1) `make` (main.cjs/preload changed),
> open a patched cart → confirm the bar names the slot + discard restores the generator sprites; (2) A's freeze
> stays the future "promote a cart gone hand-drawn" path. **Edge:** a cart whose `.cart.png` already drifted from
> its generator captures the drift as a patch on first save (defensible; `--run` rebake cleans it).
> **Resume-at: [`design/editor-cart-workflow.md` → Gap 2 the sprite story](design/editor-cart-workflow.md#gap-2--the-sprite-story--status-open-item-23)** ("Option D — what shipped").
> Hot files: `editor/electron/main.cjs` (`cart:save-to-source`), `editor/src/shell.js`
> (`saveToSource`/`readSheetSlots`), `editor/src/sprite-editor.js` (the canvas), `tools/make-cart.js`
> (`bakeSprites` / the `<src> <png>` bake), `tools/lib/sprite-patch.js` (the core).

> **▶ ACTIVE THREAD (2026-07-10) — the worldgen ladder + the junction grammar (realistic roadgen).**
> The ladder (rungs 0–3 shipped earlier) is now built end-to-end AND DRIVABLE, and the road-junction
> grammar is extracted + consumed. All gated + committed:
> **Rungs 4–5 (in `citygrow`):** per-district minor-street FILL — the arterial graph's planar faces →
> streetlab-pattern presets (grid/organic/cul-de-sac/superblock) → stitched onto the arterials — then
> CALIBRATED to real Rotterdam via the new **`sndi-check --compare`** gate (five SNDi metrics dead-on;
> T-junction share 1.1%→64.6%). Clip `citygrow/03-districts`. The residual deg-4+/circuity gap is a
> documented STRUCTURAL ceiling (arterial X-crossings + straight minors), not fill tuning.
> **Rung 5.5:** the grammar extracted to **`runtime/citygen.h`** (behaviour-preserving); **sloop's M
> key drives a generated CITY** (`citygen_road_at` behind `road_at` — a 3rd producer beside stub / OSM /
> spine). **Rung 7:** its streets are LINED WITH COLLIDABLE BUILDINGS (`citygen` `cg_lots()` → sloop
> `OB_HOUSE`). Clips `sloop/06-citygen-city` + `07-citygen-buildings`; `spec.js sloop` 25/0.
> **The junction grammar — `runtime/roadkit.h` (Track-B):** **B2** the pure geometry (`curb_return`,
> `edge_corner`, `rk_count_corners`, `rk_cross_hw`) + **B3** the N-arm-native field renderer (`RkField`)
> extracted from streetlab byte-identical (spec 104/0, mirror-diff 68=68, road-check --all all PASS),
> and **citydrive draws curb-return junctions through it** (J key, ground metres, projected; a
> `spec()` 11/0 added first as the render safety net).
> **B4 SHIPPED 2026-07-10 — the interchange grammar.** roadlab's ramp splines + topology (`Port`,
> `rk_make_junction`, POLICY/classify_turn) extracted into `roadkit.h` (pure over `Port[]` + `present[]`,
> byte-identical: roadlab spec 25/0 + render 60/60). **Consumed:** `citygrow` draws citygen's 6 grade-2
> junctions as real cloverleaf/trumpet interchanges (I key hops between them; a clean cloverleaf renders).
> Track B (streetlab → roadkit at-grade field + grade-separated interchanges) is COMPLETE.
> **Resume-at (two open forks, both specced):**
> (1) **the N+M reconciliation** — unify citygen's world model with `worldnet.h`'s β-skeleton lattice
> so the N-spine + M-city are ONE infinite world (two gated spine edits: `get_node`/`get_hub` from
> citygen density; highways lead into citygen cities). See [`worldgen-plan.md`](design/worldgen-plan.md) rung 5.5.
> (2) **polish** — suppress citydrive's round-joint disc at near junctions so the curb-return fully
> replaces the blob (today it layers over it) + per-pixel field-fill for exact N-arm asphalt.
> Hot files: `tools/carts/{citygrow,sloop,citydrive,streetlab,roadlab}.c`, `runtime/{citygen,roadkit,worldnet}.h`
> (shared — targeted edits only). **`roadkit.h` is now the shared dependency of all five carts** — any edit
> to it must re-gate every consumer, not just the one you're touching. Gates to keep green: `spec.js sloop`
> 25/0 · `spec.js streetlab` 104/0 · `spec.js citydrive` 11/0 · **`spec.js roadlab` 25/0** · streetlab
> `mirror-diff` + `road-check --all` · `sndi-check --compare build/citygrow-city.json
> data/rotterdam-netherlands.rvb` PASS. For a roadkit interchange edit, the byte-identical render check is
> the real safety net (topology spec can't see geometry): dump the committed seed
> `tools/clips/roadlab/01-junction-cycle.script` BEFORE and AFTER the edit and diff — must be byte-identical
> (`node tools/play.js roadlab script tools/clips/roadlab/01-junction-cycle.script --frames 60 --dump <dir>`).

> **▶ ACTIVE THREAD (2026-07-07) — responsive instrument UI: playbook, epiano, scale-grid.**
> A research question ("what's the best responsive UI for a music cart?") turned into
> reusable process + two live design docs + a clearly-scoped new feature to build. **What shipped
> (docs/tools, all committed):**
> - [`design/acidrack-ui-research.md`](design/acidrack-ui-research.md) — external survey of the
>   303/909/808 + best clones + the touch/density numbers (48px floor, band table).
> - [`guides/responsive-instrument-ui.md`](guides/responsive-instrument-ui.md) — the reusable
>   **playbook**: sound→inventory→steal-IA→tier→**brief**→prototype→sweep→hands→ship, with the
>   field-note-018 traps baked in as guards.
> - [`decisions/0028-sensible-defaults-optional-tweaks.md`](decisions/0028-sensible-defaults-optional-tweaks.md)
>   — the rule: pick the stranger-legible default, ship it, leave a **seam**; don't agonize, don't
>   over-configure. Wired into [design-system](design/design-system.md) §5 + the playbook.
> - `tools/carts/epianofit.c` — the step-4 layout **MOCK** (no audio): device-fit + finger unit +
>   disclosure across all shapes. Keys: `1-5` lock device / `0` auto / `m` machine / `f` fx / `s`
>   scale / `r` key / `i` iso-layout / `g` force piano-or-grid / `n` native full-bleed.
> - [`design/epiano-layout-brief.md`](design/epiano-layout-brief.md) — **re-scoped** to the FAITHFUL
>   epiano (the classic `keybed.h` piano that scales with width + a disclosing sound panel).
> - [`design/scale-grid.md`](design/scale-grid.md) — the scale-locked isomorphic pad grid **split
>   out** as its own feature (a *general* note surface, not epiano's soul — the maker wants the piano
>   kept AND the grid, eventually).
> - **SHIPPED (2026-07-07): the `scalegrid` cart** (`tools/carts/scalegrid.c`) — the playable,
>   sound-bearing showcase, device-tested on multitouch, pinned by a **71-assertion `spec()`**. 11
>   scales (incl. blues + the SoundForest "FOREST" voicing), ROW = OCT↔4TH toggle, SQR↔HEX packing
>   (equidistant-neighbour Tonnetz grid, nearest-centre hit-test, pixel-correct regular hexagons),
>   fill-both-dims finger-first sizing + a SIZE cycle, and a VOICE cycle (PD/EPIANO/MALLET/ORGAN/PLUCK).
>   No-gap lattice proven across all scales × both modes. The maker's verdict on glass: "a very nice
>   musical toy."
>
> **Resume-at: the scale-grid "where does it live" question ([scale-grid.md §3](design/scale-grid.md#3-where-does-it-live-answered-b-c)) is ANSWERED B→C** —
> built as its own cart first, grid maths kept in self-contained pure fns (`compute_grid`/`pad_midi`/
> `pad_center`/`pad_at`/`hex_verts`). **The one open step: extract those into a `grid.h` library**
> (twin of `keybed.h`, reuse `solo.h`'s scale-lock) so the whole shelf reuses it — then wire epiano's
> optional **editor-swap** to it. Separately, epiano's faithful Phase-3 (piano scales with width) per
> its brief. **Both, eventually — the grid does not replace the beloved piano.**
>
> **Hot files:** `tools/carts/scalegrid.c` (the shipped grid — extract from here), `runtime/solo.h`
> (scale-lock to reuse for grid.h); `tools/carts/epianofit.c` (the earlier silent layout mock, still
> the epiano-brief reference). Gate: `node tools/spec.js scalegrid` (71/0).

> **▶ ACTIVE THREAD (2026-07-10) — multiplayer: WebRTC P2P (rung 5b) — SHIPPED + PUBLISHED; wire-side diagnostics added (follow-ups parked).**
> Steps 1–4 shipped (commit `05a5dc76`): the WebRTC DataChannel is now the WEB game
> transport (`de_rtc_*` EM_JS shim in `runtime/net.h`), the relay reused **unchanged**
> as signaling only. Play-tested Mac↔iPhone over wifi at LAN speed — the rung-5a
> problem (3 fps + freezes from tromboning through the Render relay at ~330 ms) is
> gone. Signaling + the seed handshake ride the channel; everything above the
> `net_transport_*` seam is untouched. The spike's two potholes are baked in
> (joiner-announces-first; binary signaling told from `ROLE` by the `DN` magic).
> **Jitter fix so far = a blunt one:** the phone's ~70 ms wifi radio-sleep spikes
> stalled the old 3-frame/50 ms cushion (a 1-frame hitch every 1–2 s), so `NET_DELAY`
> is bumped to a **fixed 10 frames (~165 ms)** — feels good, at the cost of input lag
> on clean links. **Pairing UI:** a Host/Join split (gallery + in-cart bar); Join via
> native `prompt()` because an inline `<input>` is blocked by the running cart's key
> handlers on iOS. **Resume-at:** [`multiplayer-research.md` → the step table](design/multiplayer-research.md#the-step-table)
> (rung 5b). **Published 2026-07-07** — pong is live on github.io (it's
> the only netplay cart, so that's the whole rollout; the Render relay needed no
> redeploy — it's signaling only now). **PARKED follow-ups** (not being worked):
> **step 5 (adaptive `NET_DELAY`)** — the fixed 10-frame cushion feels good but adds
> lag on clean links; adaptive sizes it to live jitter to claw that back (the
> maker's-call "when we want to sand off the floatiness"). **step 7 (TURN)** — for the
> un-punchable ~10–20% (today they see "connection failed - reload"); needs a free
> Cloudflare/Metered account. **Diagnostics pass shipped 2026-07-10** (`aaca7a36`): in-band
> RTT probe (`NET_PKT_PING`/`PONG`, ~2 Hz — step 5a's adaptive input now exists as
> `net_stat_rtt_ms`), rx inter-arrival gap counter (splits wire-outage from cushion
> starvation per STALL), wall-clock stamps on every net-debug log line (correlate freezes
> with a concurrent ping by time of day), and the web tick books stalls into the same
> counters (the WebRTC path was blind before — F2 now reads the same on both targets).
> **Next time both machines are on the office wifi:** run
> [the office-session checklist](design/multiplayer-research.md#next-office-session--the-checklist)
> (~15 min — seals or refutes the 07-09 shared-AP congestion verdict with direct
> correlation + first measured look at the phone-side stutter). Hot file if resumed:
> `runtime/net.h` (targeted `Edit`s, shared). Gate: `node tools/net-check.js`. Local
> play-test: `node tools/net-relay.js --serve site`.

> **▶ ACTIVE THREAD (2026-07-10) — device-adaptive layout (the acidrack redesign · Phase 3 = R1–R6).**
> Foundation is DONE (Phases 0–2: `runtime/lay.h` + a resizable/growable-framebuffer canvas + iOS
> fill/safe-area/rotation). The **`acidwire` wireframe did its job** — interactive, felt on glass across
> phone portrait/landscape + iPad, all four states; its lessons are field note
> [020](field-notes/020-the-fit-cart-earns-it-on-glass.md). **R1** (brief) captured, **R2**
> (`runtime/disclose.h` — shape + finger-budget accordion + stack) SHIPPED + proven in acidwire
> (`27637b26`/`d96c4404`); **R3** (`finger_px()`/`device_class()` — real backing-scale finger unit)
> SHIPPED + verified on device (`7102af8b`).
> **Status + what's-left + the sequence now live in ONE scoreboard — Resume at**
> [`device-adaptive-layout.md` → Where this stands](design/device-adaptive-layout.md#where-this-stands-scoreboard).
> Short version: **R5 next** (port acidrack onto `disclose.h` + `finger_px()` + make the deferred
> CONTENT calls on glass) → R4 alongside → R6 (`epiano`) last.
> Hot files: `runtime/disclose.h`, `tools/carts/acidwire.c`, `tools/carts/acidrack.c`. Ledger:
> [`STATUS.md`](STATUS.md) #2. Exemplar/guide: [`guides/interactive-wireframes.md`](guides/interactive-wireframes.md).

> **▶ ACTIVE THREAD (2026-07-19) — store / ASO + the app-trailer builder.**
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
> launch) — recorded as `"price"` in `app.json`, BUT the **base price is a MANUAL ASC step** (Pricing &
> Availability tab — `asc-push` pushes IAP prices only, and this app has no IAPs).
> OPEN before it can SUBMIT (all deferred by the maker for now): (1) a **description** — `app.json` has no
> `listing.description`; draft against `seo-brief.md` then `asc-push --metadata`; (2) **screenshots** — 0
> today; `store-shots.js` from a `play.js --dump` frame → `asc-push --screenshots`; (3) a **standalone iOS
> build** (`.ipa`) — needs wiring like Tiny Jam's `ios/testflight.sh APP=tinyacidjam`; (4) in ASC: set the
> **$1.99 tier**, age rating, privacy-policy URL. **Resume-at:** `apps/tinyacidjam/app.json` +
> [`design/launch-sequence.md`](design/launch-sequence.md) "For Tiny Acid Jam specifically".
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

> **▶ ACTIVE THREAD (2026-07-08) — the Promote tab + the shared-popup pattern (SHIPPED).**
> (Was "editor media"; shares editor files with the trailer + leads lanes — main/preload need an
> Electron restart.) The per-cart **Promote tab** is BUILT (A–E) — a tab next to code/pixels/apps,
> resolving the media-home seam from
> [`editor-scopes-and-facets.md`](design/editor-scopes-and-facets.md#resolution-2026-07-07-make-promote-ship-promote-is-a-new-per-cart-tab)
> (Dream → Make → Promote → Ship; Make/Ship already existed, Promote was the homeless verb).
> **Sections:** **A** clips & takes — list the cart's takes, click one to WATCH it (`.rec`/`.script`
> native via `studio:replay`, `.beats` via `studio:play-beats`), ● record a take, 🎬 **bake** a take →
> clip (`studio:bake-clip` → `make-gif --recipe`), auto-refresh on record; **B** stills — 📸
> `studio:cart-shot` → `editor/public/shots/<cart>/NN-snap.png` (a NEW sibling of `clips/`); **C**
> trailer; **D** find tribes (cart-scoped `studio:cart-leads`); **E** gallery link. The old toolbar
> `● rec` button + `showRecord` setting are **RETIRED** — recording is Promote-only.
> **The shared-popup pattern (the maker's idea):** a scope-neutral tool gets ONE popup opened from
> BOTH the Promote tab (cart) and the Apps card (app) — `open({kind,name})`, caller supplies scope,
> popup is scope-blind. Two shipped instances: **trailer builder** (lifted out of the Apps panel into
> a top-level `.modal`; `openTrailer({kind,name})` — cart stitches one cart's clips, app stitches
> across an app's) and **keyword research** (`openKeywords` — `aso-research`+`aso-suggest` seeded from
> de:meta/listing). Browse-y glances stay INLINE (`leadsHtml`/`researchHtml`/`suggestHtml` extracted so
> popup + inline render identically).
> **Reels save/load (the .reel *scenario*, not the heavy webm):** a **saved-reels strip** in the
> builder scoped to the subject (`<subject>.reel` + `<subject>--<variant>.reel`) + a **cross-subject
> Reels overview** at the top of the Apps page; click any reel → loads its scenario
> (`studio:list-reels`/`reel-load`; shared `parseReelFile`/`reelClipsFor` helpers). `tlSubject` (list
> scope) vs `tlApp` (build target) are now split → ready for named variants.
> **Multi-resolution export SHIPPED (export-ratios.md, both stages, resizable carts):** an **output-ratio
> picker** on reel-Build (`# size` → compose renders at that canvas) AND a **"bake at" ratio picker** on
> the Promote 🎬 bake. **Stage 2** = per-ratio clip **variants** (`<label>--<W>x<H>.webm`) that compose
> PREFERS at a matching size → the reel **FILLS** (else letterbox). Presets: social (16:9/9:16/1:1) +
> App Store **EVEN half-sizes** (444×960/960×444/600×800, ×2 on delivery — odd widths break ffmpeg pad;
> `# scale 1` so output = the small canvas). Enabler = **keyboard shortcuts** (position-free take → any
> ratio). **`onetake` cart** (committed) is the worked proof: keyboard-driven, %-positioned, resizable.
> **Docs:** [`promote-tab.md`](design/promote-tab.md) (A–E shipped) · [`export-ratios.md`](design/export-ratios.md)
> (BUILDING — stages 1+2 done) ↔ [`resolution-portable-input.md`](design/resolution-portable-input.md)
> (the input half) + a filmability note in [`cart-authoring.md`](guides/cart-authoring.md).
> **Resume at (open builds):** (1) **an EYEBALL PASS** — a big UI stack (Promote tab, popups, reels,
> ratio pickers, Stage 2) verified only at pipeline/logic level, **none clicked live**: restart Electron,
> run record→bake→trailer→reel-with-a-text-card→Build-at-a-ratio; flag breakage (likely spots: the ≥2-clips
> rule, an unbaked clip, or new-IPC wiring). (2) **fixed-layout composite** — the export-ratios gap: a
> non-resizable cart can't reflow, so it needs a dressed bg+caption letterbox (video `store-shots`).
> (3) **named reel variants** (💾 save-as; `tlSubject`/`tlApp` split ready) · delivery-exact upscale ·
> per-cart trailer pre-pop from its reel · delete affordances · published-state dot on E.
> Hot files: `editor/src/shell.js`(+`shell.css`), `editor/index.html`, `editor/electron/main.cjs`+`preload.cjs`,
> `tools/compose-clips.js` (variant preference), `tools/make-gif.js` (`--screen`).

> **▶ ACTIVE THREAD (2026-07-07) — leads: the local marketeer (demand generation).** A new
> tool that answers "where do I post about this cart?" — the generation twin of the `aso-*`
> capture tools. **What shipped (committed):** `tools/leads.js` (7 commands: `match` cart→tribe→
> venues · `discover` venue-hunt links + Google-autocomplete signals · `draft` a gift-first post
> scaffold from the cart's own words · `track` outreach log · `audit` whole-catalogue coverage,
> free/local · `list` · `--check`) + `tools/leads-ledger.json` (committed, hand-editable; **18
> tribes**/9 cross-cutting, seeded from [tinyjam-marketing](marketing/tinyjam/tinyjam-marketing.md) §3.9). The model is **buckets**: a tribe =
> tags + venues + hook; carts auto-match on `de:meta`; a `domain` (music/game/any) pre-filter keeps
> games off music-press venues. Reddit's free API is dead (403) — discover uses free Google
> autocomplete + search-url launchers.
> **Update 2026-07-07 — the MUSIC TAXONOMY is substantially filled: 18 → 32 tribes, music coverage
> 62% → 90%.** The big idea (from a maker insight): **the tribe is the SCENE, not the FORMAT.** The
> genre-radio carts each homage a specific artist (jingle=Mac DeMarco, eno=Brian Eno, afrobeat=Fela,
> house=Daft Punk) → scene tribes keyed on the identity word, with `generative` (r/generative·lines·
> Disquiet) as a cross-domain FORMAT amplifier. Added: ambient/citypop/afrobeat/frenchhouse/
> indie-jangle/microtonal/generative, `piano`+`vintage-poly` (homed the piano/juno misfiles),
> physical-modeling/guitar/world-folk (acoustic cluster), novelty-toy/vocal-synth. Fixed the `moog`
> subtractive over-match (31→14) and the `drone`/`vowel`/`vocoder` generic-adjective noise (tag on
> identity, never technique). Model refinement written into `leads-marketeer.md`.
> **PARKED (maker's call):** the 4 weak-room scenes (satie/bossa/mariachi/tango — stay on the
> generative amplifier) + games buckets (GTM: web-gallery-only; `arcade` is the one game tribe).
> **Update 2026-07-07 — the editor Apps-page surface is BUILT v1:** the Apps card gained a **reach**
> section + **📣 find tribes** button (mirrors the ASO 📊 glance). Per cart of the app it renders
> tribes + matched tags + hook + venues (clickable → browser) + a copyable **post scaffold**, plus
> cross-cutting once — "we prep, you post." New `leads.js match --json` + `studio:leads` IPC
> (app-scoped, loops the app's carts). **NEEDS AN ELECTRON RESTART** (`make`) — main.cjs/preload.cjs
> changed. Verified via CLI (tinyjam 3-cart aggregation), not yet eyeballed in the running UI.
> **Resume-at: [`leads-marketeer.md` → Open questions / resume-at](design/leads-marketeer.md#open-questions-resume-at)** (item #4) — (a)
> maker eyeballs 📣 on the tinyjam card after `make`; (b) v2 = the free-form per-cart **discover box**
> (autocomplete venue hunt) + a cart-scoped entry point (v1 is app-scoped like the ASO tools). Hot
> files: `editor/src/shell.js` (+shell.css), `editor/electron/main.cjs`/`preload.cjs`, `tools/leads.js`,
> `tools/leads-ledger.json` (hand-edit venues). Gate: `node tools/leads.js --check`.

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
  `caffeinate -du node tools/play.js …` (wakes + holds the display). Discovered mid
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
