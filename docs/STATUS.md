# STATUS — what's shipped, what's open, what's decided-against

> **Single source of truth for project status.** The design docs
> ([`design/api-notes.md`](design/api-notes.md), [`design/audio-notes.md`](design/audio-notes.md), [`VISION.md`](VISION.md))
> hold the *rationale and proposed signatures*; this file is the *ledger* — the one place
> that says whether a thing is shipped, open, or cut. When status changes, update it
> **here**, then fix the prose in the relevant design doc. If a design doc and this file
> disagree, this file wins.

_Last updated: 2026-07-30 — **Synth Secrets phases 1 + 2**: PIANO's dispersion chain was inert and its stretched tuning was cancelled in the bass, both while every audio gate stayed green; fixed, and `tune-check` now gates stretched tuning as a **differential** so it cannot pass again by accident. See the first Shipped entry below, [`design/synth-secrets-plan.md`](design/synth-secrets-plan.md), and [`HANDOFF.md`](HANDOFF.md) for what is in flight._

> **This line is a headline, not an entry.** It reached **9,064 characters** and was the only place in the file that recorded `FILTER_DIODE`, `filter-spec.js` and `rebirth-classic.md` — three shipped things, invisible because nobody reads a shipped feature out of a `_Last updated:_` line. They have a real entry now (2026-07-02, above `sprite-draw.js`). Keep this to one date, one sentence, one link; `status-check --check` fails past 900 chars.

---

## Shipped ✓

- **SYNTH SECRETS PHASE 1 + PHASE 2 (2.1, 2.2, 2.3) — the engine gained keytracking, a monosynth key-assign
  header, and a piano that is finally a STIFF string** (2026-07-29 → 2026-07-30). The audit
  ([synth-secrets-audit.md](design/synth-secrets-audit.md), 98 findings) became an ordered plan
  ([synth-secrets-plan.md](design/synth-secrets-plan.md)); Phase 0 and Phase 1 are done and Phase 2 is 3.5
  of 4. What shipped as public API: **`instrument_keytrack(slot, amount)`** (cutoff follows the keyboard;
  `amount` 1 = true 1V/oct, 0.93 = Reid's taste value) plus **`ENV_CUTOFF_OCT`/`LFO_CUTOFF_OCT`** (sweep
  DEPTH in octaves, not Hz) — item 2.1; **`runtime/mono.h`** (note priority LAST/LOW/HIGH/FIRST × trigger
  SINGLE/MULTI/ANY, the thing that decides whether a monosynth feels playable) — item 2.2, driven by
  `sh101`; **`note_retrig(handle)`** (re-articulate a HELD voice — the envelopes and the engine's onset
  transient fire again on the voice you already have, click-free, keeping its pitch and glide) — the 2.2
  postscript, which closed audit **§K6** (the flute chiff needs to fire on every note *even when you play
  legato*, and until now a cart gliding one held voice could not tongue it at all); `pipe` and `brass` each
  gained a **T** slur/tongue toggle and `sh101` routes every articulation through one `articulate()` helper;
  **portamento now glides in PITCH, not linear Hz** — audit §B1, the audit's own "single clearest
  divergence", plan 3.26: the up/down asymmetry at one time constant went from **45 percentage points to
  4.5**, and a down-glide that used to sit 12.7 semitones sharp two seconds into a "1000 ms" slide now
  lands. **And then `ms` started meaning milliseconds:** the one-pole is gone, replaced by a
  **fixed-duration ramp with an exponential (RC-lag) shape**, so the curve still eases out like analog
  portamento but the slide arrives on schedule at *any* interval — measured at one `note_glide(600)`:
  fifth 0.59 s, octave 0.59 s, three octaves 0.60 s, three octaves downward 0.59 s. It costs less per
  sample than the one-pole did. The fidelity argument points the same way: a Minimoog's glide knob has no
  numbers on it because a one-pole has no duration to print, so the millisecond unit was always *ours*.
  ⚠ **BUILT, not shipped — 3.26 is LISTEN-gated and awaits the owner's ear**, and this one changes the
  FEEL of every glide in the 59 carts that call `note_glide`: their values now buy a real duration rather
  than a time constant, so slides are roughly 3× snappier (`acid303`'s classic 60 ms is now genuinely
  60 ms, which is closer to a real 303). Play `heldnotes`, `tb303`, `sh101`, `brass`, `pipe`; measure with
  `glideprobe`. Still open: the per-octave **GLIDE SCALE** axis, now an API-surface question, not DSP.
  **`MODE_PIANO_STRETCH`** (the Feynman/Railsback stretched tuning) and **`MODE_PIANO_STIFF`**
  (real stiff-string inharmonicity, B ≈ 1.1e-4 on the grand) — item 2.3; and **`MODE_BOW_BODY`** (three
  parallel 1–4 ms delay lines: `INSTR_BOWED` had no body resonator at all) — item 2.4, in progress.
  `eng_p[]` is now **six** wide with the bound at `idx >= 6`.
  **Two long-standing engine defects were found by MEASURING rather than reading**, both of the shape *a
  value computed correctly that never reaches the sound*: PIANO's dispersion chain was **inert** (allpass
  coefficient 0.9999948 = the identity; measured B ≈ 2e-6 against a grand's 1e-4) because a positive
  coefficient flattens partials and a `pt ≤ 0.9` clamp made the useful negative half unreachable; and its
  stretched-tuning seam was **cancelled in the bass** one frame after note-on (`v->freq` was written back,
  `v->freq_target` was not, so the glide slew undid it — while an `effLen > len` clamp happened to protect
  the treble). Fixing the second also removed an **unintended damping**: the piano had been running on the
  lossy fractional-interpolation read path on every note, so it is now ~4 dB hotter in the bass.
  **Four oracles came out of it**, each because nothing existing could see the bug:
  [`click-check.js`](../tools/click-check.js) (waveform splices), [`inharm-spec.js`](../tools/inharm-spec.js)
  (partial frequencies in cents + per-partial decay), [`disp-model.js`](../tools/disp-model.js) (dispersion
  and body design, analytic — model the sweep, never patch a shared engine to search one), and
  [`lint-aux-params.js`](../tools/lint-aux-params.js) (the silently-inert `instrument_mode` parameter — that
  bound has now bitten twice). **Recorded DROPs, with their measurements:** 1.4 brass envelope, 1.6 Hammond
  registrations, and **2.3(b) level-dependent inharmonicity** — the last because its "five families, one
  physical fact" ranking turned out to be arithmetic on a false premise (four of the five engines have no
  inharmonicity to modulate at all). Also fixed en route: `tune-check` could not see PIANO's stretched
  tuning at all, and now asserts it as a **differential** against a runtime stretch-off pass.

- **The app-icon mask, measured** (2026-07-28) — `tools/icon-mask.js` + a committed
  `tools/icon-masks/ios26-2048.png`. Two apps were in review with their icon corners visibly shaved,
  so we stopped guessing the squircle: **Xcode 26's Icon Composer ships `ictool`**, which renders a
  `.icon` document through the **real** iOS mask, and the alpha of a flat full-bleed render *is* the
  mask. `rebuild` drives it, keeps the alpha at 2048², symmetrises 8-fold (pre-symmetry asymmetry was
  1/255, so the shape really is 4-fold symmetric) and commits; `--check` re-derives and diffs, gated
  in `repo-doctor`, skipping cleanly without Xcode. Three findings changed how we draw: it is **not a
  superellipse** (best fit n=4.39 still misses by 42 px at 1024, so the folklore "n=5 / r=22.37%" is
  the wrong shape), iOS 26 is a **strict envelope** of the classic iOS 7 to 18 mask (checked per row,
  worst crossing 0.0 px, so one template covers both), and the **inscribed circle is entirely safe**
  (only the four corner slivers are at risk, which is the whole design rule). The trap it explains:
  a hand-drawn rounded-rect chassis has *circular* corners where the mask has *continuous* ones, so
  it pokes through near the diagonals and the border reads as broken at four points; draw it on
  `--inset` (an eroded offset of the mask) instead. `check <icon.png>` is the oracle: per corner,
  flat background (safe) or detail (loss), how far the lost ink reaches, a 3-up proof PNG, and
  `--quiet` to gate a release. Both icons in review measured: pedalboard loses 16 to 18% of each
  corner region, tinyacidjam 40 to 45%.
  **Then the prediction got verified against a real device and wired into the pipeline** (same day):
  `device <icon.png>` runs the whole proof end to end (borrow a simulator `.app`, swap the icon in
  with `actool`, install into a booted iOS 26 sim, screenshot the home screen, find the new icon by
  before/after diff polled until the install ring stops, crop, diff the silhouette). On iOS 26.5 /
  iPhone 17 the committed mask matched to **mean 0.13 px / max 0.70 px**, and a flat probe came back
  rgb 254-255, 0-1, 254-255 across the interior, so **iOS applies no gloss to the flat PNG we ship**
  and masking alone is faithful. (`ictool` *does* gloss its own render, because a `.icon` document is
  the layered Icon Composer format that opts into the glass treatment: a property of the format, not
  of our asset.) Two more measurements fell out: the home-screen icon is **192 px, not 180** (iOS 26
  draws 64pt @3x), and iOS's downscale is closest to **mitchell** (5.44/255 mean delta vs cubic 5.90,
  lanczos3 6.93, nearest 11.82, zero pixel offset), which is now what `preview` uses. `preview` is
  the everyday view: the icon masked and shrunk to all six real display sizes, light and dark, which
  is where a lo-fi icon actually dies (at 87 px a pixel-font wordmark turns to mush). It runs itself
  from `build-app.js` icon staging and from a new per-app **🎨 icon** button in the editor's Apps tab.
  Design: [`design/app-icon-mask.md`](design/app-icon-mask.md).

- **The contemporary ReBirth, rungs A + B** (2026-07-26) — a post-hardware rack clones **techniques,
  not machines**. ReBirth RB-338 cloned specific boxes because in 1997 the genre lived in unobtainable
  hardware; modern genres were never made on gear, so their identity lives in a *workflow* (the glide,
  the ratcheted hat, hard tune as an instrument, the always-on squash). The design record
  ([`design/contemporary-rebirth.md`](design/contemporary-rebirth.md)) audited eight boxes across two
  candidate racks against the real API and found **six need zero engine work**; the three gaps became
  rungs A/B/C, two of which shipped the same day.
  - **Rung A: `multiband(low, mid, high, up, mix)`** + `instrument_multiband(slot, …)`, insert kind
    **`FX_MULTIBAND`** (18, auto-placed on first call so `fx_order()` can put it before or after the
    drive stage). Three-band dynamics (120 Hz / 2.5 kHz, the house one-pole crossover idiom from
    `eq_process()`) with a shared **UPWARD** amount: that upward half is what makes a hyperpop master
    sound permanently "on", and it is exactly what `glue()` (one band, downward only) could never do.
    `mix` 0 = **bypass, byte-identical**, because the bands sum back to the input. Two things the first
    render taught us: with no makeup the full wall measured **1.8 dB quieter than dry** (backwards for
    an effect whose whole point is "louder"), so makeup now scales with the mean down amount (+5.7 dB,
    2% clip); and the upward half has to taper out near silence or it amplifies the noise floor.
    **Named `multiband`, not `squash`**: `build-all` caught five carts already declaring a local
    `squash` (squash-and-stretch is animation vocabulary, the CLAUDE.md `map` trap's second instance).
    Also unlocks the multiband-distortion gap parked in [`design/distortion-lab.md`](design/distortion-lab.md)
    (same crossover, different per-band stage).
  - **Rung B: `sample_shift(slot, semitones)`** (offline, in place, ±24, beside `sample_autotune`) +
    **`harmonize_mic(semitones, voices)`** (live: the shift, plus a fifth, plus an octave). Both
    transpose while **keeping the take's length**, which playing a sample slot at a higher note cannot
    do. One PSOLA core now wears both faces, snap and shift, and generalizing it was proven
    **bit-identical** for the already-shipped autotune by re-render with matching SHA (`DE_DEFINES=NO_SHIFT`),
    not by argument.
  - **Parked with numbers, which is the honest half:** "an octave up in the singer's *own* voice" is not
    a flag on this code. Re-spacing epochs while holding grain content does hold the formants (F2 991
    to 947 measured) but leaves the pitch unstable at *every* interval tried: f0 wobble 170-300 Hz
    against the raw take's 5 Hz. The reason is structural, a grain carrying the source periodicity
    still sounds at the source pitch however you re-space it. `sample_autotune` never exposed this
    because a correction moves the period a few percent, where the mismatch is inaudible; an interval
    halves or doubles it. So the transparent *shift* needs its own spike, exactly as the transparent
    *correction* got two ([`design/transparent-autotune.md`](design/transparent-autotune.md)).
  - **Carts: `hyperbox`** (the tiny hyperpop rack, four boxes: pitch-snapped voice, a seven-saw
    `instrument_unison` wall, a ratcheted `tr909.h` kick lane, and a master chain with **no bypass
    drawn** at all) and **`voxshift`** (the acceptance probe: one captured voice four ways, raw /
    snapped / +12 formants HELD / +12 formants FOLLOWING, source via `record_arm`+`record_grab` so it
    needs no mic and replays deterministically).
  - **The generalizable lesson: with a squashed master you mix for the GAPS, not the levels.** The
    first `hyperbox` render measured rms -1.5 dBFS at a 1.5 dB crest, a wall with no groove left in it,
    because OTT lifts the synth's sustain into every hole the kick needed. Both fixes were cart-side:
    the saw became a *decaying stab* rather than a pad, and the parts sit much lower than feels right
    dry (the master brings them back). Landed at rms -5.3 / crest 5.2, where real hyperpop masters sit.
  - **Open:** Rung C (`beatfx(mode, bars, mix)`, a beat-synced buffer re-reader for halftime / beat
    repeat; the biggest of the three and the one the hip-hop rack needs), `hyperbox` v2's real voice box
    (waiting on the parked spike; v1 stands in with `INSTR_VOICE`), a home cart for `harmonize_mic`, and
    the hip-hop rack itself (whose hard part is not DSP but *where the loop audio legally comes from*).
  - **Gates:** `soundcheck` silent · `fx-check` every other effect at Δpk/Δrms +0.0 (the byte-identical
    proof) · `level-check` in tolerance · `soak-check` stable · `web-audio-check` wasm parity ·
    `formant-check` at five intervals · `build-all` 566/566 then 568/568 · `lint-fx-frame` clean
    (both are ride-safe, so they stay out of the set-and-hold footgun set) · `ui-audit` clean, after it
    caught one off-screen caption and four title/caption collisions on the rack's first layout pass.
    Ledger: [`design/audio-notes.md`](design/audio-notes.md) §17 #34 + #35; settings in
    [`guides/effects-recipes.md`](guides/effects-recipes.md).

- **`walkbox` — a walking-bass step-sequencer, BUILT and PARKED** (2026-07-18). A TB-303 workflow driving
  the upright's real `INSTR_BOWED` pizzicato voice: draw a line on scale-locked note bars, sculpt a tabbed
  VEL/LEN lane (velocity → pluck attack, length → staccato gate, top = TIE), flip SLD/OCT rows, dial SWING.
  Core + articulation ship and play. **Parked with a wish list, not a blocker:** ghost notes, hammer-on/
  pull-off, presets. Design: [walkbox.md](design/walkbox.md).

- **Audio input — the mic seam + the vocoder** (2026-07-16/17) — the engine can HEAR and SPEAK.
  A **device-free microphone seam** behind the `platform.h` host contract, wired on **all four
  platforms** (desktop CoreAudio/`AudioQueue` · web `getUserMedia` · iOS `AVAudioSession` · Android
  `AAudio`+JNI perms): the host owns the capture device + permission flow and pushes frames into the
  engine (`de_audio_input`), which only analyzes them — so no capture library lives in the core.
  Cart API: `mic_start()`/`mic_stop()`/`mic_active()`, **`mic_level()`** (RMS), **`mic_pitch()`** (a
  **YIN** detector — octave-safe, replaced the zero-crossing estimate the `pitchscope` cart diagnosed),
  and **`mic_record()`** (capture-then-freeze PCM → `sample_load`). Then the **VOCODER**: a master-stage
  12-band carrier×modulator filterbank (`vocoder()`/`vocoder_send()`) fed live by the mic through a
  lock-free audio-thread PCM ring (`sound_extin_*` + `vocoder_mic()`) — the ring also opens the
  live-throughput/**pedal** tier. Carts: `mictest`, `pitchscope`, `humtheremin` (hum→theremin),
  `voxbox` (the real vocoder — sing and a chord speaks your words), `vocode` (deterministic synth
  modulator), + `breakchop` mic beatbox auto-chop. Determinism: [ADR-0032](decisions/0032-live-mic-effects-are-live-only.md)
  (live-mic-through is live-only; capture-then-freeze stays deterministic). Design:
  [`design/mic-and-sampling.md`](design/mic-and-sampling.md) + [`design/vocoder.md`](design/vocoder.md)
  (vocoder v2 open: sibilance band, mic-rate resample, on-device latency). Desktop + web live-verified.

- **LOCKSTEP NETPLAY RUNG 5b — browser WebRTC P2P, SHIPPED + PUBLISHED** (2026-07-10). `pong` plays
  peer-to-peer between two browsers over a DataChannel at ~12 ms on wifi, with the relay reduced to
  signaling only (`tools/net-relay.js`); live on github.io. Wire-side diagnostics landed the same week (RTT
  probe, rx-gap and wall-clock logs, web-tick stall detection). **Two follow-ups are PARKED, deliberately:**
  step 5 (adaptive `NET_DELAY` — the fixed 10-frame cushion holds) and step 7 (TURN, for peers that cannot
  connect directly). The remaining task is physical, not code: the office-wifi checklist at
  [multiplayer-research.md](design/multiplayer-research.md#next-office-session--the-checklist). Gate:
  `node tools/net-check.js`.

- **The per-cart PROMOTE tab + the shared-popup pattern, SHIPPED A–E** (2026-07-08). Record/bake clips,
  the reel builder with save/load (subject-scoped strip + cross-subject overview), multi-resolution export
  (output-ratio picker on reel-Build and clip-bake, Stage-2 per-ratio variants that FILL, App Store even
  half-sizes), and the popup pattern reused by the trailer and keyword-research surfaces from both the cart
  and app pages. Keyboard shortcuts were the enabler. Design: [promote-tab.md](design/promote-tab.md) +
  [export-ratios.md](design/export-ratios.md), both stages shipped. **Open, and the reason it is not
  finished-finished: an EYEBALL PASS — the stack was verified at pipeline/logic level and none of it was
  clicked live** — plus the fixed-layout composite gap.

- **`tools/leads.js` — the local marketeer, BUILT and PARKED** (2026-07-07). The demand-GENERATION twin of
  the `aso-*` capture tools: maps a cart's `de:meta` to its tribe(s) and the venues where that tribe
  gathers (`match`), hunts new venues (`discover`), scaffolds a gift-first post from the cart's own words
  (`draft`), and tracks outreach (`track add`). Ledger `tools/leads-ledger.json` is committed and
  hand-editable — **34 tribes / 14 cross-cutting** as of 2026-07-30 (`node tools/leads.js --check` for the
  live count). The taxonomy has kept growing through use; the tool itself has not changed since it was
  built. Open: the editor Apps-page surface. Design:
  [leads-marketeer.md](design/leads-marketeer.md#open-questions-resume-at).

**Tooling & environment**
- Code editor (CodeMirror 6, C syntax, autocomplete + hover + Cmd-click-to-help +
  cmd-click an `#include "x.h"` filename → opens the read-only engine source in the
  docs tab's "engine source" group), sprite editor, map editor — all in one
  PICO-8-style window.
- ▶ run (clang → native Raylib window), inline clang error markers.
- Cart format — `.cart.png` with source/sprites/map in zTXt chunks; save, load,
  drag-drop. Carts carry their own settings (screen/scale/cell/map).
- Tutorials gallery — **545** registered carts (tutorials, games, toys, instruments, probes;
  572 sources in `tools/carts/`, the difference being unregistered work-in-progress);
  all of them also playable on the web gallery (<https://nikkikoole.github.io/dreamengine/>,
  sortable by date-added/title/mobile-readiness, day/night, description toggle).
- **The RB-338 homage rack + `FILTER_DIODE`, the acid filter** (2026-07-02) — design → shipped in one
  day. **`FILTER_DIODE` (`studio.h`, filter mode 10)** is a real TB-303 diode-ladder lowpass and the
  first filter here whose *character* comes from its topology rather than its cutoff: **~18 dB/oct**
  (between `FILTER_LOW`'s 12 and a Moog ladder's 24), it **drains bass as resonance climbs**, and the
  resonance **saturates inside the loop** (the diodes) so it growls instead of ringing clean.
  Self-oscillates at the top; lowpass-only. `tb303.c` was upgraded onto it.
  Shipped with its own oracle — **`tools/filter-spec.js`**, which measures a per-voice filter's actual
  slope, resonance peak and bass drain per resonance step, so a filter claim is evidence and not an
  adjective. The rack itself is **`acidrack.c`**: a 320×240 accordion of 2× diode-ladder 303 + the
  full 909 + the full 16-voice 808, banks A–D + a 64-bar chain, per-DEVICE FX, and a seeded generator.
  Design record: [`design/rebirth-classic.md`](design/rebirth-classic.md); filter measurements in
  [`design/audio-notes.md`](design/audio-notes.md) §25.
  *(Entry written 2026-07-30. This whole lane shipped four weeks earlier and had **no entry** — it
  lived only inside the `_Last updated:_` headline, mashed together with the unrelated 2026-07-26
  contemporary-ReBirth work whose rung B is `sample_shift`, not this. That is what a headline used as
  an entry costs: `FILTER_DIODE`, `filter-spec.js` and `rebirth-classic.md` were each mentioned
  exactly once in this file, on line 10, where no reader looks for a shipped feature.)*
- **`sprite-draw.js` post-processing batch + `foundry.cart.png`** (2026-06-04) — five new ops for programmatic `.cart.js` sprites: `shade()` (auto light/shadow via the curated `RAMP_DARKER`/`RAMP_LIGHTER` palette LUTs — *the* "one step darker/lighter" table for the fixed palette), `rotate()`/`rotations()` (baked headings, still post-processable unlike runtime `spr_rot`), `scale2x()` (EPX: sketch 16×16, bake 32×32), `replace()`/`clone()` (bake-time variants); `split()` now grid-splits a 32×32 into 4 slots as its comment always claimed. Showcase: **SPRITE FOUNDRY** — "watch the code draw": `foundry.cart.js` snapshots the canvas into the next slot after every drawing step, and the cart plays each subject's time-lapse back with the code line per frame (dragon → `shade()`, ship → `mirror()`+`rotations()`, boss → `noise()`/`replace()`/`scale2x()`). Tutorial 15 (animation phase) also rebuilt on the library: its 6-frame walk cycle is one parametric `walker(t)` sampled over the stride. See [`guides/cart-authoring.md`](guides/cart-authoring.md) → "sprite-draw.js".
- **`ragdoll.cart.png`** — Verlet physics sandbox. Up to 50 stick-figure ragdolls hop autonomously across the screen, bounce off each other and off rolling balls. Grab + throw with mouse (whole-body drag), right-click to spawn balls, C to spawn characters, R to reset. Key techniques: Störmer-Verlet integration, distance constraints (12 sticks/character, 8 iterations), position springs chained bottom-up (feet → knees → hip → chest → head), angular springs per bone with 90° guard (cross-product direction inverts past 90°), per-character standing/ragdoll timer that only recovers when feet are on the floor, hop impulse that immediately releases the foot pin so it isn't erased, broad-phase character collision (hip-to-hip > 40px early-out then 12×12 point pairs with velocity impulse). Debug session used the play.js harness + DE_TRACE watches to trace `rtimer`, `whop`, `hip_y`, `knee_y` and diagnose: rest lengths mismatched standing pose (hip-knee was 9, actual √73 ≈ 8.54 → knee pushed to floor); hop velocity erased by foot pre-pin each frame (fixed by setting `rtimer=0` inside `hop_tick` and re-reading `sk`). See `tools/carts/ragdoll.c`.
- Web build — "Build for web" (emscripten → `cart.html/js/wasm`, local server on 8765).
- **Store / press / share toolchain + the editor share panel** (2026-07-03) — a full in-house
  App-Store-prep suite, all FREE (no account, no subscription). Tools: `aso-research` (keyword
  landscape, clickable App Store links), `aso-compose` (pack the 100-char field), `aso-lint`
  (char limits / waste / cross-field repeats), `store-shots` (device screenshots — composite-
  not-stretch), `store-contact` (hero-frame contact sheet), `press-kit` (a presskit()-style page
  from cart data). Editor surface: a topbar **⇪ Share** popover (per-cart exports, grouped by
  audience) + an **Apps** tab (app-less ASO lab · apps list from `apps/*/app.json` · per-app
  actions: 📸 screenshots → 📄 press kit, 🍎/📱 Mac/iOS builds, 📝 worksheet / 🔎 research / 💡 suggest / 🧩 compose / 🔬 analyze / 📊 score / ✅ lint / 🪞 check the app's
  `listing`). The app manifest gained a `listing` block (title/subtitle/keywords) as the
  machine-readable home. Design + rationale: [`design/store-agents.md`](design/store-agents.md),
  [`design/share-panel.md`](design/share-panel.md), [`design/press-kit.md`](design/press-kit.md);
  Tiny Jam's listing: [`marketing/tinyjam/app-store-listing.md`](marketing/tinyjam/app-store-listing.md).
  Still open (before a real submission): per-locale copy, the ASC upload/TestFlight step
  (ADR-0026), and the Search-Term-Rank popularity column (Apple beta).
- **`youtube-push.js` — video distribution, lever #2's last mile** (2026-07-20) — the in-house
  YouTube uploader (twin of `asc-push`, [ADR-0033](decisions/0033-youtube-first-video-distribution.md)):
  a committed recipe → bake an mp4 (`make-gif`) → composite a crisp 9:16 **Short** (integer-
  upscale + pad, never stretch; a >60s clip is refused) → resumable upload → a `youtube.com/
  shorts/…` URL back, from one command. `--landscape` = the full 16:10 video; `--reel <app>` =
  push an app trailer. Metadata (title/description/tags/category, `#Shorts` when vertical) is
  **derived** from cart `de:meta` / the app `listing` — no hand-typed copy. OAuth2 creds live in
  `~/.youtube/` (`--auth` one-time browser consent), never git; `--dry-run` prints the plan,
  `--check` is the offline gate. YouTube first because it's the only short-video venue with a
  usable official upload API (TikTok/Reels stay manual). PROVEN live (first real upload: the
  tinyjam reel → an unlisted Short). Design: [`design/video-distribution.md`](design/video-distribution.md).
- **Live (libtcc) backend + hot reload** — a "run mode" toggle (settings) switches ▶ run from the clang static build to a persistent `-DDE_TCC` host that JIT-compiles the cart in-process via vendored `runtime/libtcc/`. Editing the code auto-reloads it (debounced, no Run press) without restarting the window; compile errors mark the line and keep the last good cart running. State survives reloads via **`de_state()`** — promoted to a first-class `studio.h` API and fronted by the starter cart's friendly `STATE { ... }; / S->field` sugar (clickable to help). arm64-macOS only; sprite/screen changes relaunch. Full record + rationale: [`design/cart-as-script.md`](design/cart-as-script.md).
- 5-tab navbar (code · pixels · carts · docs · settings); in-app docs viewer renders
  this `docs/` set (with cross-links) in the Docs tab.
- Day/night theming, debug overlay (`watch`/`printh`/crash capture).
- **Live inspection** — on-demand screenshot + state snapshot while a cart runs. Write the desired output path into `build/.bake/screenshot_request` or `build/.bake/state_request`; the game captures the current frame on its next tick and deletes the request file as the handshake (gone = fresh, ready to read). Lets you bracket a specific moment: capture before + capture after = instant diff without a filmstrip. Works alongside `play.js run --headless` and any other harness mode. See [`guides/debug-harness.md` → Live inspection](guides/debug-harness.md).
- **Profiler** — one-click `⏱ profile` button (hidden behind a settings toggle). Compiles a profiling build (`-O1 -fno-inline -DDE_PROFILE`), runs the cart ~4s, and reports into the build log: frame CPU budget (ms vs the 16.6ms 60fps target), hottest functions **with the call paths that reach them** (macOS `sample` call-graph attribution, rolled up to `studio.h` primitives), and exact per-frame draw-call counts (in-engine `PROF` counters, re-entrancy-guarded). Behind the scenes — no Instruments GUI. macOS-only (uses the `sample` CLI). **`PROF` counters + frame timing are now always on in native builds** (not just `-DDE_PROFILE`) — `perf.json` is written every 30 frames in any normal run; snapshot it on demand with `profiler_request` (same trigger-file pattern as `screenshot_request`). `-DDE_RELEASE` strips all overhead (new settings toggle, see below). See [`guides/profiler.md`](guides/profiler.md).
- **Release build mode** — settings → run mode → "build mode" select. `normal` (default): profiler counters + `harness_inspect` polling on, `-Os`. `release`: `-O2 -DDE_RELEASE` — strips `PROF()`, timing measurement, and all per-frame trigger-file probes. For when you want to benchmark or ship without any instrumentation overhead.
- **Per-cart save folders** — `save()`/`save_int()`/`save_bytes()` files (`cart.sav`/`cart.kv`/`cart.blob`) live in `build/saves/<cart>/`, not one shared set in `build/`. Runtime takes `--save-dir DIR` (any native build, default cwd); the editor slugs `cartName` and `play.js` uses the cart's file stem, so editor saves and harness saves are separate folders per cart — a scripted test run can't clobber a real hiscore. Web build unchanged (no argv). See [`guides/debug-harness.md`](guides/debug-harness.md) flags table.
- **`monstermix.cart.png`** — the `sprite-draw.js` `stamp()` showcase cart. The `.cart.js` draws 9 parts (3 heads, 3 bodies, 3 legs, `mirror()`ed) and `stamp()`-composites all 27 combos into slots at bake time; magic `pal()` indices 28/29 recolor them into 4 schemes at draw time — 108 monsters from 9 parts. Also exercises `split()` (32-wide machine), concave `polyfill` (star), `noise()` tiles, `outlined()` with a custom outline color. Gameplay: assemble the customer's order, piston-stamp it (squash + dust + shake), combo chords climb with the streak. See `tools/carts/monstermix.c` / `.cart.js`.
- **`tools/font-bake.js` + `fontbake.cart.png`** (2026-06-05) — real-TTF text as sprites, at build time. Parses a TTF (vendored `tools/vendor/opentype.cjs`), flattens the glyph outlines and scanline-fills them (nonzero winding, 3×3 supersampled, optional darker AA-edge color) into sprite-draw 2D canvases — so any Google Font can be a cart's title with zero runtime font code. `measure()` for fitting a slot budget; border/shadow are plain sprite-draw composition (`outlined()`, offset-stamped recolored clone). Fonts live in `tools/fonts/` (Bungee + OFL included; new ones are one curl from github.com/google/fonts). Showcase: **font bake** — words baked centered into fixed slot-rects so the C side `sspr()`s constant sheet regions; title waves in 4px strips, `pal()`-recolors live (fill + AA edge remapped together — swapping only the fill leaves a clashing rim). Same-day follow-up: **`bakeBanner` promoted into the library** (fit + center + outline + shadow → ready tiles; second customer) and **high noon cart** (Smokum) — a quick-draw reaction duel where the baked words ARE the game (DRAW! signal, DEAD/EARLY!/YOU WIN verdicts), five words packed to exactly 64 slots; full championship/death/early paths verified via scripted play.js runs. Hard-won rules now in the guide: `colorkey(0)` in `init()` is mandatory (no default transparent color — banners drag an opaque black slot-rect without it), and every word needs two slot-rows (one row = ~11px glyphs after outline trim, too thin at 2x). See [`guides/cart-authoring.md`](guides/cart-authoring.md) → "font-bake.js".
- **THE BAND panel** (2026-06-05) — every chaired radio station gets a live timbre-audition overlay: press **B**, click a chair row (or press its number) to cycle that chair's instrument candidates mid-song. The G-key A/B pattern generalized: `runtime/radio.h` owns the registry + input + draw (`rad_chair` / `rad_band_input` / `rad_band_panel`), the cart applies each swap in its own `apply_chair()` — the toolkit never calls back in and never touches `rad_srnd`, so pinned seeds stay byte-identical. Picked chairs re-assert after `new_song`'s per-song rolls; chairs left at sel 0 keep the shipped sound and roll. Chaired so far: **cocktail** (solo chair hands improv.h's chorus to piano/vibes/guitar), **tango** (the three orquestas: troilo/d'arienzo/pugliese reed tables, arco/pizzicato violins, felt/dark piano), **yacht** (dx tine/rhodes/clavinet ep, three basses, three leads, pad), **roadhouse** (VOX/Gibson drawbar tables, rhodes/upright piano bass, guitar), **exotica** (vibes/marimba/denny-piano, fm-glass/celesta). Candidates sourced from [`design/radio-instrument-options.md`](design/radio-instrument-options.md) — ten stations there still unchaired.
- **`tools/sprite-draw.js`** — shared programmatic sprite-authoring library for `.cart.js` files. Exports a 2D pixel-canvas API aligned with the C drawing API names: `blank`, `pixel`, `rectfill`, `rrectfill`, `line`, `circlefill`, `ovalfill`, `trifill`, `polyfill`, `ngonfill`, `noise`, `outlined`, `mirror`, `stamp`, `flat`, `split`, `OUT`. All 14 programmatic `.cart.js` files `require('../sprite-draw.js')`. Three `.cart.js` authoring styles documented: (1) settings-only, (2) ASCII art with `DEFAULT_CHAR_MAP` (palette 0–15), (3) programmatic arrays via sprite-draw (palette 0–31, geometry). See `tools/sprite-draw.js` and `CLAUDE.md` → project structure.
- **Lockstep netplay, rung 1** (2026-07-02) — two native builds play the same 2-player cart over UDP (localhost or LAN by IP) with carts staying 100% network-unaware: under netplay `btn(player,…)` means "which machine" (host = player 0, joiner = player 1; all local input — either keymap, touch — is *my* player). `runtime/net.h`, runtime-flag-gated like the debug harness (`--net-host` / `--net-join <ip>` / `--net-port <n>`; a normal run touches none of it). Net implies `--det`; the host's seed rides the handshake so both sims roll the same `rnd()` stream. NET_DELAY=3 frames of input latency, GGPO-style redundant input packets (a dropped datagram never stalls), BYE on quit + a 10s barrier timeout (peer crash ≠ hang). **v1 syncs `btn()` only** — carts reading `key()`/`mouse_*()` in `update()` desync. Pause is disabled under net (would stall the peer). Demo + gate: **`play.js <cart> netdemo`** spawns a host+joiner pair side by side (optional per-side scripts) and diffs their per-frame traces — `LOCKSTEP OK` / `DESYNC` verdict, exits nonzero on desync; the blessed check lives in [`guides/checks-and-oracles.md`](guides/checks-and-oracles.md) with committed netcheck clips at `tools/clips/pong/`. Design + the rung ladder (LAN discovery, determinism proof, internet/browser): [`design/multiplayer-research.md`](design/multiplayer-research.md).
- **Multiplayer site UI — "play together" + the room bar** (2026-07-06, rung 5a step 4 v0.5) — multiplayer stops being invisible on the published site: the gallery gives 2-player carts a **👥 play together** button (mints a room code, opens the cart in it, `&relay=` auto-appended off the Render domain), and the cart page (both shells) gets a **room bar** — in a room: the code + a *copy invite link* button (the page URL is the invite); not in one: a *play together* offer, gated by a `players` field build-site.js stamps into each cart's `manifest.json`, derived at build time from the cart SOURCE's `de_players()` (same fn that gates the native lobby — zero hand-maintained metadata). Editor build-web previews unaffected (no manifest → bar stays hidden). Render-domain gallery shows it after the next manual deploy (`autoDeploy: false` is deliberate). Design: [`design/multiplayer-research.md`](design/multiplayer-research.md) §5a step 4.
- **The repo split: private-ready code repo, public site repo** (2026-07-06) — the published gallery moved out: code repo renamed **`dreamengine-studio`**, the 356MB `site/` (which had been re-committed 105 times into code history) became its own checkout of the new public **`NikkiKoole/dreamengine`** repo — same `nikkikoole.github.io/dreamengine/` URL because the *public* repo inherited the name. Branch-based Pages (no workflow; `pages.yml` retired), `site/` gitignored in the code repo, `publish-cart.sh` reworked into two legs (commit+push inside site/'s own repo; a by-pathspec write-back of published cart sources here) with a guard that prints the re-clone one-liner for machines where site/ is missing. **The code repo can now flip private with zero further work.** Other machines: the first pull past the split deletes `site/` → `git clone git@github.com:NikkiKoole/dreamengine.git site`. Design record: [`design/sharing-channels.md`](design/sharing-channels.md) §"Parked: private code repo".
- **Netplay relay: Render blueprint + cart-scoped rooms** (2026-07-06; **DEPLOYED + wss-verified same day** — `https://dreamengine-relay.onrender.com` is live, serves the gallery, and a two-client wss probe confirmed seating + verbatim forwarding through Render's proxy; a complete shareable match link is `https://dreamengine-relay.onrender.com/pong/?room=play`) — the internet-relay decision is made and committed: a **`render.yaml`** blueprint deploys the relay on Render's free tier (**moved same day into the public SITE repo after the repo split** — `site/render.yaml` + a publish-synced `site/net-relay.js` copy, so Render connects to `NikkiKoole/dreamengine` only and the code repo stays unshared; the service also `--serve`s the gallery, so games run off the Render domain with no `?relay=` param) (Frankfurt, wss:// handed out, build step overridden so the root `sharp` dep isn't compiled for nothing; bare-URL GET answers 200 = the cold-start warm-up). And **rooms are now cart-scoped by construction**: the web transport prepends the cart's URL path segment to the code (`?room=play` in pong → room `pong-play`), so ONE shared relay hosts every cart with the same friendly codes and can never cross-pair two different games — the relay stays blind. Walkthrough: [`design/multiplayer-research.md`](design/multiplayer-research.md) §"Hosting beyond the LAN".
- **The worldgen ladder, rungs 0–3 — the realistic-roadgen foundation** (2026-07-06, all four in
  one day; plan + per-rung detail: [`design/worldgen-plan.md`](design/worldgen-plan.md)).
  **Rung 0 · `tools/sndi-check.js`** — the street-network METRIC oracle (SNDi composite over a real
  `.rvb` or a generated-graph JSON, side-by-side A/B, `--check` self-test) + the first real-city
  target table (Manhattan/SF/Amersfoort/Rotterdam/Königssee, parked driftable in the plan).
  **Rung 1 · `runtime/worldnet.h`** (= [driving-world-program](design/driving-world-program.md) Track-A A1) — roadnet2's world core
  extracted to a shared library header the moment sloop became the second consumer: terrain +
  ranked hub/town lattice + terrain-aware spline links + the unified **`wn_road_at()`** nearest-edge
  query (per-anchor edge cache + bucket spatial hash; the **edge-type field road/rail/water pinned**
  before the model froze) + `wn_nearest_road_point()` spawn snap. **sloop's N key** swaps its stub
  grid for the infinite deterministic spine behind the same `road_at()` seam — real on/off-road grip
  (1.0/0.55), `spec.js sloop` stays 25/0, extraction proven regression-free by a byte-identical
  trace. Clips: `roadnet2/01-rung1-onoff`, `sloop/04-rn2-spine`.
  **Rungs 2+3 · the `citygrow` bench cart** (new; rungs 2–5's knobbed home per the plan's
  one-data-model guard). Rung 2: the **population-density field** D(x,y) (regional noise × people-
  live-low-and-flat × graded water-adjacency pull × a 500 km world-rim fade — **bound implemented,
  maker feel-check pending**); settlement presence/rank/size come from the field on the same
  suppression lattice, extent = a threshold contour; **O** = the field-vs-hash A/B (the rung's whole
  argument in one keypress). Rung 3: per-city **tensor-field arterials** (Chen 2008 — radial core +
  grid aligned to the entering highway read live from the worldnet cache + terrain contours + noise,
  weights hashed per city; both eigen-families traced with separation, cached — a city is bounded);
  **T** = city view, **X** = sndi-check JSON export. First generated-vs-real numbers: mean degree
  2.65 (coastal) / 2.95 (lakeside) vs Amersfoort 2.71; the gaps (rim-stub dead-ends, missing
  T-share) are now measured rung-5 calibration targets. Everything deterministic (pixel- and
  byte-identical across runs). Clips: `citygrow/01-field-vs-hash`, `citygrow/02-city-arterials`.
  **Rungs 4–7 + the junction grammar SHIPPED 2026-07-10** ([`design/roadkit.md`](design/roadkit.md) +
  the plan). **Rung 4:** per-district minor-street FILL — the arterial graph's planar faces →
  streetlab-pattern presets (grid/organic/cul-de-sac/superblock) → stitched onto the arterials.
  **Rung 5:** CALIBRATED to real Rotterdam via the new **`sndi-check --compare`** gate (five SNDi
  metrics dead-on; T-junction share 1.1%→64.6%; the residual deg-4+/circuity is a documented
  structural arterial-X ceiling, not fill tuning). **Rung 5.5:** the grammar extracted to
  **`runtime/citygen.h`** (behaviour-preserving) — **sloop's M key drives a generated CITY**
  (`citygen_road_at` behind its `road_at()` seam, a 3rd producer beside stub/OSM/spine); **Rung 7:**
  its streets are lined with **collidable procedural buildings** (`citygen` `cg_lots()` → sloop
  `OB_HOUSE`). Clips `citygrow/03-districts`, `sloop/06-citygen-city`, `sloop/07-citygen-buildings`;
  `spec.js sloop` 25/0. **The junction grammar — `runtime/roadkit.h` (Track-B):** **B2** the pure
  geometry (`curb_return`/`edge_corner`/`rk_count_corners`/`rk_cross_hw`) + **B3** the N-arm-native
  field renderer (`RkField`) extracted from streetlab **byte-identical** (spec 104/0, mirror-diff 68=68,
  `road-check --all` all PASS), and **citydrive draws curb-return junctions through it** (`J`, ground
  metres, projected; a `spec()` 11/0 added first as the render net). **B4 + Rung 6 SHIPPED 2026-07-10 —
  Track B complete:** `citygrow` emits `(legs,bearings,class,grade)` per arterial node (6 interchanges +
  154 overpasses on the test city), and roadlab's interchange grammar (ramp splines + `rk_make_junction`
  topology) is extracted into `roadkit.h` **byte-identical** (roadlab spec 25/0 + render 60/60); `citygrow`
  renders the grade-2 junctions as real cloverleaf/trumpet interchanges (new `I` key hops between them).
  Along the way, fixed a latent perf trap the `I` key exposed: the zoom-scaled extent/rim `circ()`s hit
  hundreds-of-thousands-px radii at max zoom (p95 1316ms, `circ` = 99.4% of wall) — `circ_capped()` now
  skips a ring once it's >4 screens across. **Next:** the N+M infinite-world reconciliation
  (worldgen-plan rung 5.5) + citydrive disc/field polish.
- **Lockstep netplay, rung 5a steps 2+3 — WebSocket transport + the relay** (2026-07-05; **LIVE-VERIFIED 2026-07-06** — two real browsers played a relay match on the home wifi; a leaving peer now shows an on-screen "PLAYER 2 LEFT — PLAYING SOLO" banner on the survivor's side, `net_notice` in net.h + `draw_net_notice()` in studio.c. Hosting beyond the LAN — the github.io cart + a `wss://` relay story — is written up in [`design/multiplayer-research.md`](design/multiplayer-research.md) §"Hosting beyond the LAN"). Step 2: the web transport arm in `net.h` — an `EM_JS` WebSocket shim (`de_ws_begin/state/send/recv`; **no strings cross the C/JS boundary** — JS reads `?room=`/`?relay=` from the page URL itself) + `net_web_poll()`, the ASYNC twin of `net_handshake` driven by the pre-init click screen (ROLE → HELLO → WELCOME{seed} through the relay, seed before `init()`, exactly the lobby's ordering); mid-game `net_ws_pump` handles INPUT/BYE, a dead socket = BYE, web drops to solo instead of exiting the tab. New `NET_PKT_ROLE` — the one relay-originated packet (first in room = host). Step 3: **`tools/net-relay.js`** — zero-dependency Node (hand-rolled RFC 6455): rooms of 2 by code, blind binary forwarding (never parses game packets — one relay serves every cart forever), BYE synthesis on disconnect, **`--serve <dir>`** = the one-wifi-box setup (static cart + relay in one process, prints the exact two-device URL), `--check` self-test. Lobby v0 = the URL itself (`?room=X`). Plus **`tools/net-check.js`**, the one-command lockstep gate (echo mirror + netdemo pair + relay wire-protocol sim — all PASS), and `--net-echo` passthrough in play.js. Try it: `node tools/build-site.js pong && node tools/net-relay.js --serve site/pong` → open the printed URL on two devices. Design: [`design/multiplayer-research.md`](design/multiplayer-research.md) §5a.
- **Lockstep netplay, rung 5a step 1 — the lockstep core compiles on WEB** (2026-07-05) — the "one lockstep core, two transports" split from [`design/multiplayer-research.md`](design/multiplayer-research.md) §5a, the gating work for browser multiplayer. `runtime/net.h` is now two tiers: **`DE_NET_CORE`** (input rings, `NET_PKT_*`, frame barrier — no sockets anywhere, compiles native AND web) and **`DE_NET_BUILD`** (UDP transport + `--net-*` flags + handshake/lobby, native-only exactly as before); every send/receive goes through the new `net_transport_send`/`net_transport_pump` seam, where step 2's WebSocket arm will land and nowhere else. New **non-blocking barrier `net_frame_try_sync()`**: web `loop_step` stalls the whole tick (no `sound_tick`, no update/draw, no clock advance) while the peer's byte is missing — a browser main thread can't block or WS messages never arrive; the native `net_frame_sync()` is now a blocking wrapper around it. `det_mode`/`det_clock`/`clk()` moved out of the native-only harness block so web runs the fixed-step deterministic sim under net. Proven end-to-end by the new **echo transport** (a loopback fake peer): `--net-echo` (native) / `-DDE_NET_ECHO_DEFAULT` (web) — P2 mirrors P1 through the real pack→send→ring→barrier→`btn(1)` path. Gates: netdemo `LOCKSTEP OK` ×300 traced frames (per-side scripts), echo trace `p2y==p1y` ×300, `build-all` 466/466, emcc compiles the core bare + echo-enabled. **LAN netplay (rungs 1–3) is unchanged — the web path is a second transport NEXT to UDP, not a replacement.** Next: step 2, the WebSocket transport + ~100-line relay.
- **Lockstep netplay, rung 3 — "Open to LAN" auto-discovery** (2026-07-02) — the joiner **finds the host with no typing**. The host broadcasts a small ANNOUNCE datagram to the subnet (`255.255.255.255:33446`) every ~1s while waiting (`net_announce()`, game socket + `SO_BROADCAST`); the lobby's Join screen listens (`net_discover_begin/poll/end` — a `SO_REUSEADDR` socket) and auto-fills the discovered IP ("found a game at 192.168.x.x — ENTER to join"), manual entry kept as fallback. Cross-platform (Winsock too). Verified: the announce is received on-box (sniffed `DN`+type 5+port 33445) and it cross-compiles for Windows; the full two-machine pick is a live test. Minecraft-style, no servers. Design: [`design/multiplayer-research.md`](design/multiplayer-research.md).
- **Lockstep netplay, rung 2.5 — in-game lobby** (2026-07-02) — an engine-owned **Host / Join / Solo boot menu** (`net_lobby_menu()` in `runtime/studio.c`, gated by `--net-lobby` or the compile-time `DE_NET_LOBBY_DEFAULT`) so a **standalone build with no editor** can start netplay with no CLI flags — the "send a friend an .exe" case. Reorders the net startup: the lobby draws after fonts load but before the cart's `init()` (the host's rnd() seed must reach the joiner first), so the handshake now runs with the window open, drawing a `HOSTING at <ip>` / `connecting…` status frame (this also fixes rung 2's "host waits with no window" rough edge). Join screen has an in-window IP text-entry. The direct `--net-host`/`--net-join` path (editor 🌐 button / CLI / netdemo) is unchanged. This is the design doc's "the shell owns host/join" — except for a standalone the *engine* is the shell. **Also (same day):** a **`MULTIPLAYER` item in the pause menu** that **self-restarts** the binary into the lobby (`net_restart_into_lobby()`, reusing the RESTART item's `execv(restart_argv)`) — so a player double-clicks the exe, plays solo, then pauses → MULTIPLAYER → lands in Host/Join/Solo; two launches on one Mac each do this to play locally. **And `net.h` is ported to Winsock** (winsock2-before-windows.h via `NOGDI`/`NOUSER`/`NOMINMAX`; `getifaddrs`→UDP-connect trick; `de_mkdir`/`SIGBUS` gaps fixed; `-lws2_32`), so netplay carts now **cross-compile to a real Windows `.exe`** — compile-verified on the dev box; Windows *runtime* test pending a real machine. Next: an "export exe" button, then rung 3 (multicast auto-discovery). Design: [`design/multiplayer-research.md`](design/multiplayer-research.md).
- **Lockstep netplay, rung 2 — LAN by IP** (2026-07-02) — the shipped path from "CLI-flag capability" to "playable from the editor". A **🌐 multiplayer button** next to ▶ opens a host / join-by-IP popover (`editor/src/shell.js`); `editor/electron/main.cjs` adds the `--net-*` flags to the run spawn and shows the host's LAN IPv4 (via `os.networkInterfaces()`), while the native binary resolves + prints it too (`net_local_ipv4()` in `runtime/net.h`, `getifaddrs()`, prefers a 192.168/10 private address). Host on one Mac, read the shown IP, type it into Join on the other — the wished-for "click host → get an address" UX for the home/classroom case, no NAT, no servers. **Deviation from the plan:** the address surfaces in the editor UI + console, not an in-window overlay — `net_handshake()` blocks *before* `InitWindow`, so there's no window to draw on during the host's wait; the shell (where the button is) is the better surface anyway. Known rough edge: a host with no joiner waits with no window until someone connects (or the editor quits). Next: rung 3 (UDP-multicast "Open to LAN" auto-discovery). Design: [`design/multiplayer-research.md`](design/multiplayer-research.md).

**API surface** — **373 functions** + ~90 constants in `runtime/studio.h` (count from
`node tools/api-usage.js`, which also cross-checks studio.h against studioDocs.js and shell.js).
*Was written as "~125" and stood while the surface tripled — re-run the tool, don't trust the prose.*
For the full grouped inventory see [`design/api-notes.md` → "What dreamengine has today"](design/api-notes.md).
Recently landed and worth calling out:
- Juice batch: `pal`/`pal_reset`, `fade`, `shake`, `print_scaled`, `text_width`.
- **Font system:** `font(FONT_NORMAL/FONT_SMALL/FONT_TINY)` state setter; two extra fonts baked (`font4x6.png` ~64 chars/320px, `font3x5.png` ~80 chars/320px); `print_shadow`, `print_outline`; all `print*` functions now return x-after-last-char for chaining and overflow detection. Demo: `fonts.cart.png`. See [`design/font-rendering.md`](design/font-rendering.md).
- Sprite transforms: `spr_rot`, `sspr_ex` (rotation/scale/flip around a pivot).
- **`sget(sx,sy)`** — palette index of a *spritesheet* pixel (companion to `pget`, which reads the canvas). Lets carts treat sprites as data: paint a level in the sprite editor (1 pixel = 1 block, color = type) and read it back at runtime, or build lookup tables. Shipped with two paired platformer tutorial carts — **`platform-rects`** (a pixel-perfect AABB mover: per-axis resolution + sub-pixel position, coyote time, jump buffering, variable jump height, one-way platforms; level as a hard-coded `Box[]`) and **`platform-paint`** (same mover, level read from a painted sprite via `sget`). Same engine, two level sources — the "level as code vs level as data" teaching pair from [`design/tutorial-curriculum.md`](design/tutorial-curriculum.md).
- Fill patterns: `fillp`/`fillp_reset` + `FILL_*` (PICO-8-style fillp).
- Shapes/helpers: `oval`/`ovalfill`, `bar`, `blink`, `fsqrt`.
- Pseudo-3D: `tritex` (affine texture-mapped triangle; used by `mode7`/`raycaster`/`cube3d`/`cityview`).
- **`cityview`** — GTA1-meets-Zelda pseudo-3D city bench: parallel-oblique projection (height goes
  straight up-screen), four building view modes, `tritex` wall textures, and drivable raised-highway
  flyovers (ramp/curve/spiral/stack) you climb while the camera rises. Folded in the former `overpass`
  experiment. Proves the projected-primitive helpers ([decision 0021](decisions/0021-road-geometry-in-2d-sandbox-render-is-an-adapter.md),
  mechanism in [design/pseudo-3d-city.md](design/pseudo-3d-city.md)). Next: pipe `roadlab`'s real `z(s)` deck through its projector.
- 3D leaf-helpers: `V3` + `rot3`/`project3`/`zsort`/`quadfill` — the rotate→project→sort→fill
  pipeline the solid-3D carts re-derived by hand. `cube3d`/`solid3d`/`textured3d`/`flyover`
  refactored onto them. [decision 0009](decisions/0009-small-3d-leaf-helpers.md).
- **`fade()` is now immediate-mode** — the runtime zeroes it each frame, so a cart re-asserts
  `fade()` only on the frames it wants the screen dimmed and never calls `fade(0)` by hand. Fixed
  the same stuck-dim bug in **27 carts** at once (conditional overlay fade that never cleared on
  exit). `camera`/`pal`/`fillp` remain sticky setters. [decision 0010](decisions/0010-fade-is-immediate-mode.md).
- **Removed:** turtle graphics (`turtle_*`/`pen_*`) — one user, trivially a cart; see Cut
  below + [decision 0008](decisions/0008-cut-turtle-graphics-api.md).
- Input: full mouse (`mouse_x/y/down/pressed/released/wheel`), keyboard
  (`key`/`keyp`/`text_input`).
- Time/persistence: `dt`, `epoch`, `save_bytes`/`load_bytes`.
- **`fps()`** — measured frames-per-second right now, `int`, smoothed over the last second
  (wraps Raylib `GetFPS()`). 60 = smooth; lower = the cart is too slow this frame. Independent
  of `dt()`/`det_mode`: even when the sim's `frame_dt` is pinned for deterministic replay,
  `fps()` still reports *real* render throughput, so scripted/headless runs stay reproducible
  while showing true speed. Intended as the read-out for the triangle-perf work (open item 14):
  `watch("fps", "%d", fps())` on a haze-heavy `podracer` frame, before/after a change. *(A
  dedicated profiling setup is in progress separately.)*

**Code-first sound** — **32-voice** synth (`SOUND_VOICES`; it went 8 → 16 → 32, and this line said
"8-voice" throughout); `note`/`hit`/`chord`/`strum`/`tone`/`degree`,
`bpm`/`beat`, `every`/`euclid`/`chance`, `schedule`, `schedule_hit` (delay **+** duration —
sample-accurate sub-frame sfx/arp steps). (The `sfx` bank plays built-in demo data only —
see "Open" below. **`music()` is CUT** as of 2026-06-04 — zero cart users, patterns were
never cart-authorable, the generative beat-clock path won;
[decision 0013](decisions/0013-cut-music-api.md).)
- **Modulation envelopes** — `instrument_env()`/`note_env()`: 2 routable one-shot AD
  envelopes per slot (`ENV_CUTOFF` = the pluck "pew", `ENV_PITCH` = drum punch/zap,
  `ENV_DUTY`), bipolar amount, exp decay — the second EG (audio-notes §11). Demo carts:
  `filter env`, `pitch env`; wired into `modrack` (onboard fenv/penv + a VCA `a` jack) and
  `dream synth` (AMP/FILTER/PITCH envelope tabs).
- **Drawable waveforms** — `wave_set()` + `INSTR_USER0..3`: four 64-sample single-cycle
  tables you can draw and play like any wave; live-morphable (a ringing note changes as the
  table is rewritten). Demo cart: `wave editor` (draw the cycle, SPACE-drone + live morph,
  seed shapes, exports `wave_set()` code). The cart-authorable half of
  [`design/instrument-engines.md`](design/instrument-engines.md) §8.4.
- **Sound-tool carts (draw → export-as-code)** — `sfx editor` (paint 32 steps),
  `sfx generator` (sfxp/bfxr-style: 17 sliders + RANDOM/MUTATE + sfxr category buttons),
  `wave editor`. All export paste-ready C (a data array + a tiny player) — the
  decision-0003 answer to sfx authoring, zero engine banks needed.
- **Sound tripwire + `soundcheck` self-test** — the engine counts dropped requests
  (ring buffer + delayed pen) and printh-screams `[sound] WARNING … DROPPED` when sound
  calls are lost (the silent class: defines that never land, notes that never play).
  `soundcheck` cart = worst-case burst + full-API walk; run after touching `sound.h`:
  `node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"`
  — silence = PASS. See [`guides/debug-harness.md`](guides/debug-harness.md) → "Sound tripwire".
- **Instrument synth** — `instr` is now an instrument slot (0–4 = the raw waves,
  unchanged; 5–15 cart-defined). Four expressive axes bundled per slot, all on the raw
  waveforms: `instrument()` (per-voice **ADSR**), `instrument_duty()` (pulse width),
  `instrument_lfo()` (**3 LFOs**/slot → vibrato/PWM/tremolo/wah), `instrument_filter()`
  (resonant SVF: low/high/band/notch). Demo carts: `instruments`, `lfo`, `filter`, and
  `dream synth` (a playable Moog-style patch panel + keyboard). See
  [`design/audio-notes.md`](design/audio-notes.md) §10.
- **Held notes** — `note_on()→handle`/`note_off()` plus live setters
  `note_pitch`/`note_vol`/`note_cutoff`/`note_res`/`note_duty`/`note_lfo`/`note_filter`/`note_glide`
  (+ `note_off_all`). A sustained voice
  you drive frame-by-frame, the opposite of fire-and-forget `note()`: hold-to-sustain,
  engine revs, sirens, theremins. Handles are `index + generation` so a stale handle
  safely no-ops; per-param slew smooths live writes (no zipper). `note()`/`hit()` keep
  their fixed-duration behavior. Demo cart: `held notes` (a theremin); `dream synth`
  retrofitted onto them (real hold-to-sustain + live filter sweep). See
  [`design/held-notes.md`](design/held-notes.md).
- **Stereo + panning** — the audio path is stereo. `instrument_pan(slot, pan)` (per-slot
  position, inherited at note-on), `note_pan(handle, pan)` (live, slewed — positional audio:
  map a sound to where it is on screen), and `LFO_PAN` (auto-pan as an LFO destination).
  `pan` is -1 left .. 0 center .. +1 right. **Linear pan law, center unchanged**: a centered
  voice is byte-for-byte the old mono mix, so existing carts are unaffected. Master soft-clip
  and the steal-declick tail are stereo-correct (peak-gain clip preserves the pan ratio).
  Echo stays a mono bus in v1; ping-pong + the effects layer's stereo width are next (§8.10).
  Demo cart: `pan`. Full design + the build-order + bite checklist: [`design/stereo.md`](design/stereo.md).
- **Input release edges** — `keyr(k)` / `btnr(player, button)`, the falling-edge partners
  to `keyp`/`btnp` (true on the frame a key/button is released). Needed for hold gestures
  (note_on on press, note_off on release).

---

### The numbered items that landed *(consolidated 2026-07-30)*

These were filed as numbered `## Open` items and are **done**. They sat in Open for weeks to months
after shipping — 20 of 53 numbered items, so the backlog advertised roughly **38% more open work than
existed**. That is the single thing this file was getting most wrong.

**They keep their numbers.** ~30 `STATUS #N` references across docs, `tune-check.js`, `sound.h` and
~10 cart sources resolve today, so a number is an address: it is recorded here, never reused, never
renumbered. Nine of these also left a genuinely-open remainder, which stays in `## Open` under the
same number — marked **tail → item N** below.

Detail lives in the linked design doc in every case; that is where it was always written.

| # | what landed | when | detail |
|---|---|---|---|
| **2** | **2D geometry helpers** — `ngon`/`star`/`poly`/`thickline`/`rrect`/`vgradient` + the outline siblings, so every filled shape has a matching boundary ring | 2026-06-04 | [geometry-helpers.md](design/geometry-helpers.md) (which also holds the parked `lerp_color` question) |
| **4** | **Pause overlay v1** — P/ENTER opens, ESC resumes, freezes `update()`, mutes sound, Continue/Restart, `paused()`; plus the 06-05 hardening (key claiming so a full-keyboard cart keeps P, `-DPAUSE_KEY` actually honored, ENTER no longer self-cancelling) | 2026-06-05 | [api-notes.md](design/api-notes.md) §16 · **tail → item 4** |
| **5** | **The whole sound build-out** — instrument bank (ADSR/duty/LFO/filter), held notes (`note_on`/`note_off` + live setters + slew), modulation envelopes, and all **14 modeled engines** PLUCK → BRASS | 2026-06-05 → 06-10 | [instrument-engines.md](design/instrument-engines.md) §8 · **tail → item 5** |
| **10** | **Browser URL-sharing** — the whole catalog | 2026-06-05 | [sharing.md](guides/sharing.md) |
| **14** | **Rasterization consistency** — every filled primitive on one pixel-centre coverage path (outline == boundary of fill), plus the off-screen **bbox clamp** that turned a 46.7 ms worst case into 2.7 ms | 2026-06-01/02 | [rasterization-consistency.md](design/rasterization-consistency.md) · **tail → item 14** |
| **15** | **Tiny fonts** — `font(FONT_SMALL)` / `font(FONT_TINY)` | 2026-06-01 | [font-rendering.md](design/font-rendering.md) |
| **20** | **TB-303 bassline cart** — non-refiring `note_glide` slides, accent, staccato gate, live CUT/RES on the ringing voice, piano roll with OCT/ACC/SLD rows | 2026-06-05 | the `tb303` cart · [rebirth-classic.md](design/rebirth-classic.md) |
| **24** | **Web phantom touch point** — own the touch truth on web (a JS mirror rebuilt from `event.touches`), plus the same-day **tap-as-mouse death** sequel (synthesize the mouse from the touch mirror once a real touch is seen) | 2026-06-06 | [touch-notes.md](design/touch-notes.md) §7–8 |
| **25** | **`ui.h` v1** — button / slider / knob across mouse + touch + keyboard/gamepad at once: per-contact capture, deferred press resolution, hit-pad inflation, opt-in focus ring | 2026-06-07 | [ui-widgets-notes.md](design/ui-widgets-notes.md) · **tail → item 25** |
| **27** | **Web debug overlay v1** — `?debug=1` or triple-tap the top-left corner: live touch rings, a console mirror, `onerror` lines, fps, the device touch ceiling | 2026-06-07 | [mobile-web-notes.md](design/mobile-web-notes.md) §6d · **tail → item 27** |
| **28** | **Library headers findable** — the read-only engine-source viewer moved out of its code-tab overlay into the docs tab as an "engine source" group; cmd-clicking an `#include` opens the same view | 2026-06-07 | `editor/src/navigate.js` · **tail → item 28** |
| **36** | **modrack MACRO reaches all 14 modeled engines** — `SOUND_INSTR_SLOTS` 32→48 so the 8 unreachable engines got dedicated slots (`eng` knob 0..13); Bandito reworked to MEMBRANE bongos, new BOWED Chamber preset | 2026-06-15 | [instrument-engines.md](design/instrument-engines.md) |
| **37** | **Polyphony `SOUND_VOICES` 16 → 32** (+ the coupled `SOUND_HANDLE_BITS` 4→5) | 2026-06-15 | [audio-notes.md](design/audio-notes.md) §15 — which now carries the RAM/CPU analysis |
| **39** | **One `LFO_SHAPE_*` enum across every LFO site** — voice LFOs, `tremolo`/`autopan`, `fx_lfo`, all through one `lfo_wave`/`lfo_eval` dispatcher; SINE stays byte-identical | 2026-06-15 | [decision 0018](decisions/0018-effects-keep-params-but-become-modulatable.md) |
| **41** | **Waveguide engines can bend pitch DOWN** — BRASS / REED / PIPE / BOWED (each had sized its delay line at the note-on pitch and clamped the read length to it) | 2026-06-16 | [waveguide-bend-handoff.md](design/waveguide-bend-handoff.md) · **tail → item 41** |
| **45** | **Attach-profile the running cart at its current state** — no cold respawn; an always-present editor `Debug` menu listing every running cart, plus a cart-lifetime `⌘⇧P` | 2026-07-10 | [cart-os.md](design/cart-os.md) · **tail → item 45** |
| **46** | **The editor cart-browser surfaces the `de:meta` facets** | 2026-06-29 | [cart-metadata.md](design/cart-metadata.md) |
| **49** | **`de_switch_cart()` + the per-cart sound context** — the umbrella-app seam that makes a multi-cart binary possible | 2026-07-03 | [share-panel.md](design/share-panel.md) · [decision 0027](decisions/0027-sound-state-flows-through-the-request-queue.md) |
| **50** | **The flight recorder v1** — always-on deterministic session capture | 2026-07-10 | [flight-recorder.md](design/flight-recorder.md) |
| **51** | **Cart COLLECTIONS** — doc-anchored cross-cutting threads; 62 carts tagged across radio(35)/road(9)/tinyjam(7)/physics(7)/responsive(4)/device-face(3) | 2026-07-14 | [cart-metadata.md](design/cart-metadata.md#collection-doc-anchored-cross-cutting-threads) · **tail → item 51** |


## Open — prioritized

> ### ✅ FIXED — `piano`'s two dead sliders were one bound in the engine (and the bug class remains)
>
> Found 2026-07-28 while building the Synth Secrets §I9 layer; fixed 2026-07-29. `piano.c`'s **"decay"** and
> **"knock"** sliders had never done anything, and **the cart was not at fault**. `instrument_mode()` in
> `runtime/sound.h` guarded with `idx >= 2`, so indices 2 and 3 were dropped **in the setter** — while the
> piano engine implements both end to end: `sound_piano_start` reads `eng_p[2]` as the double-decay scale and
> `eng_p[3]` as the hammer-knock scale, both are copied to the voice at note-on, and the instrument bank
> defaults them to 0.5 (= 1.0×). Nothing was missing; two finished engine parameters were simply unreachable.
>
> The guard is now `idx >= 6` (`eng_p` is six wide — it was four here, widened twice since: idx 4
> `MODE_PIANO_STRETCH`, idx 5 `MODE_PIANO_STIFF`; `tools/lint-aux-params.js` now gates the five places
> that width is written down). **It is a no-op at rest** — a slider at 0.5 sends
> exactly the bank default it was already using, so the shipped piano is byte-identical (sha `25cb93583e73`,
> verified against the pre-fix source) — and only bites once a slider moves. Proven live: sweeping idx 2 moves
> brightness 0.067 → 0.107 → 0.165 and the centroid 2127 → 2419 → 2755 Hz. Gates after the change: soundcheck
> silent, `tune-check` no new drift, `level-check` and `dc-check` clean, all carts compile (569 then, 570 now).
> `MODE_PIANO_DECAY` / `MODE_PIANO_KNOCK` now exist so no cart needs raw indices again.
>
> **Still open, and the reason this stays here: `instrument_mode` does not validate its index.** An
> out-of-range idx is silently ignored, which is how a dead user-facing control survived this long — it
> compiles, runs, and looks fine. Worth deciding whether the engine should complain (a `[sound] WARNING`
> would have surfaced this immediately, and `soundcheck` greps for exactly that). A sweep of every
> `instrument_mode` call site found `piano.c` was the only cart affected.
>
> **A measurement trap from the same hunt, worth remembering:** the first probe "proved" idx 2 dead by
> rendering byte-identical audio — but the probe set idx 2 and the cart's own `push_knobs()` line then
> *overwrote* it, so the probe was measuring itself being clobbered. When probing a value the cart also
> writes, **replace** the cart's write rather than adding a second one.
>
Ordered by leverage. Section refs point at the design doc that specs each item.
Which *carts* are probing these questions (and every verdict so far) →
[`design/probe-carts.md`](design/probe-carts.md); probe carts carry `"probe"` in
their `kind[]` tags.

1. **Cart-pattern helpers** — `hud()` and `game_over_screen()` cut (see Decided-against).
   - **AABB collision already SHIPPED as `boxes_touch()`** (+ `point_in_box`, `circles_touch`,
     `near`, `touching_map`, `tile_at`, `touching_color` — the full collision section;
     `bounce_at_edges` later cut for zero adoption, [decision 0014](decisions/0014-cut-unused-convenience-helpers.md)).
     *Not* a missing primitive. Open question is **discoverability, not API**: a rough
     grep finds ~30 carts still hand-rolling inline rectangle overlap even though `boxes_touch`
     exists and 15 carts use it — so the lever is teaching (a collision tutorial / converting
     example carts), and *maybe* a more-guessable alias name, not a new function. Adding a bare
     `overlap()` synonym would just grow the already-large surface (see VISION's prune note).
   - `explode()` / particle system is a **research topic** before any build: a no-param
     `explode()` risks making all carts look identical (same concern that killed `hud()`).
     Needs design work on color, shape, lifetime, and movement params first.
     See particle survey + open questions in [`design/api-notes.md`](design/api-notes.md) §C.
3. **Events** — `broadcast(msg_id)` / `received(msg_id)`. Confirmed demand (independently
   surfaced by the brainstorm review). Touches main-loop drain semantics.
   [`design/api-notes.md`](design/api-notes.md) §11.
4. **Pause — the Options submenu + `menuitem()`** *(v1 shipped 2026-06-05, see the landed table)*.
   What's left of the pause work:
   - **Options submenu**, matching PICO-8's pause → Options: Sound ON/OFF · Volume · Fullscreen
     (one `ToggleFullscreen()`) · Controls (read-only display of the current P1/P2 bindings —
     rebinding stays in the editor settings tab) · Back.
   - **`menuitem(index, label, callback)`** — let a cart add its own rows ("restart level", "toggle
     music", "easy mode") to the pause menu, zero layout work for the cart. **~30 carts currently
     hand-roll their own options screen**, which is the actual argument for this.
   - **Per-player pause key** — one shared `PAUSE_KEY` today. When gamepad support (item 7) lands,
     each player wants their own (`P0_PAUSE_KEY`/`P1_PAUSE_KEY`, same `-D` pattern as the other
     bindings). The architecture already supports it; it just isn't exposed.
   [`design/api-notes.md`](design/api-notes.md) §16.

5. **Sound expansion — what's left after the engines** *(the bank, held notes, mod envelopes and
   all 14 modeled engines shipped 2026-06-05 → 06-10; see the landed table)*. The remaining work:
   - **Zero-setup preset instruments** (`INSTR_PLUCK`/`PAD`/… as ready-made voices, no macro fiddling).
   - **Cart-side SFX authoring** — the `sfx` bank is hardcoded today. *Direction (2026-06-04):
     prototype as a PICO-8-style editor CART first* (draw the pitch contour, toggle per step) with
     **zero new engine API** — `hit()`/`schedule()` + the beat clock for playback, `save_bytes` for
     persistence, export-as-C into games. `sfx_def()` only if the prototype proves the engine should
     own banks. *Pattern* authoring is settled-no ([decision 0013](decisions/0013-cut-music-api.md)
     cut `music()`). See [`design/audio-notes.md`](design/audio-notes.md) §5.6.
   - **Next engine layer: formant + effects.** Additive stays deferred (`INSTR_SINE` + FM + MALLET
     cover its near corners; the MT70 family is its first named customer).
   🅿️ **PARKED — revisit when the effects-bus layer lands:** the per-voice wah (epiano AUTO/TOUCH)
   and the **envelope follower** (`instrument_follow`/`note_follow`) are *interim*. The realistic
   "woah woah" auto-wah is a BUS effect and will likely replace them; the follower's real home is
   bus-level. Kept because they may be handy, flagged so nothing more gets built on them — when
   [`design/instrument-engines.md`](design/instrument-engines.md) §8.10 is built, decide whether to
   fold them into the bus wah or remove them.
   ⚠️ This touches `runtime/sound.h`/`studio.c` — hot shared files; sync before starting.

6. **Sprite flags** — `fget`/`fset` (per-sprite 8-bit flags; 256 bytes). Pairs with an
   8-checkbox row in the sprite editor. [`design/api-notes.md`](design/api-notes.md) 2026-05-30 review.
7. **Gamepad** — `gp_axis(slot, axis)`, `gp_present(slot)`, internal `btn()` augment.
   [`design/api-notes.md`](design/api-notes.md) §15. *Groundwork landed via touch controls
   (2026-07-01):* `touch_layout(mode, n_buttons)` is the same "declare this cart's logical
   controls once, feed every input source" idea this item wants, and the button vocabulary grew
   from A/B-only to `BTN_A/B/X/Y` (numbered to append further) specifically so a real pad's face
   buttons have somewhere to land — see [`design/touch-controls.md`](design/touch-controls.md)
   "Synergy with the gamepad item."
8. **Strudel extras / Dilla groove timing** — `pitch`, `sometimes`/`often`/`rarely`,
   `arp`, `stutter`, `palindrome`, `off_beat`; `groove` + `groove_swing/jitter/push`,
   `tick`/`PPQ`. [`design/api-notes.md`](design/api-notes.md) §13–14.
9. **Per-game polish pass** — title/game-over screens, hi-scores, sound on every event,
   juice, difficulty curves, readable HUDs. (Reframed as a reference idea bank, not an
   active backlog — see [`POLISH_TODO.md`](POLISH_TODO.md).)
11. **iPad runtime** — touch is wired in the runtime; needs a build path. [`VISION.md`](VISION.md).
    *Product lens (2026-06-07):* if the tinyjam racks become a paid product, this item is
    the cash register — iOS is where music-app buyers live. Deliberately **waits for
    evidence** (a rack holding people's attention on the free web gallery) per the
    sketch-first decision in [`design/product-notes.md`](design/product-notes.md).
    *Update 2026-07-03:* the build path largely exists (8/9 [`design/ios-plan.md`](design/ios-plan.md)
    spikes ✅); the live map of what's still missing to the store is
    [`design/sharing-channels.md`](design/sharing-channels.md) §Channel B (product decision,
    palette, submission pipeline — the latter decided in-house, not Fastlane:
    [ADR-0026](decisions/0026-store-pipeline-in-house-not-fastlane.md)).
12. **Sound tracker UI** — open question; depends on whether code-first sound proves
    sufficient. *Direction 2026-06-04: leaning PICO-8-style, prototyped as a CART with
    zero new engine API (see #5 + audio-notes §5.6) — the cheap way to find out if the
    editor earns a place before any engine-side bank API exists.*
    [`VISION.md`](VISION.md), [`design/audio-notes.md`](design/audio-notes.md) §5.6, §9.

13. **Baked rotation atlas** *(exploratory — full design note, not yet started)* — an
    offscreen-canvas primitive (`make_canvas`/`target`/`blit`) plus pre-rotated sprite/shape
    "stamps" so bodies are fast translate-blits instead of per-frame rasterization (for
    crowds, rich shapes, low-end). Centerline/pivot model, `pal()` recolor for free color
    variety, parts capped at 16px (native slot size). The path to scaling the `bones`
    animator past realtime drawing. [`design/baked-rotation-atlas.md`](design/baked-rotation-atlas.md).
    - *Reframe (from the Picotron manual):* Picotron collapses "sprite sheet," "offscreen
      canvas," and "raw memory" into ONE primitive — `userdata(type,w,h)`, a typed 2D buffer
      that sprites/map/screen all are. Suggests the primitive here should be **general**, not
      rotation-specific: a draw target you can render into, read/write per-pixel (`sset`/`sget`),
      and stamp — the offscreen canvas, the rotation cache, and runtime sprite editing are then
      the *same feature*, not three. Worth designing the buffer primitive first; the rotation
      atlas becomes one use of it. (Still index-only — no RGB/alpha, unlike Picotron's userdata.)
14. **Rasterization consistency — the two verification leftovers** *(the coverage-path unification
    and the off-screen bbox clamp both shipped 2026-06-01/02; see the landed table)*. Neither of
    these is design work:
    - **Web GL ES confirmation** — the invariant detector uses `pget`, which is disabled on web, so
      the whole coverage path is unverified on the wasm build.
    - **An ADR** for the GPU→CPU behaviour change in `tri`/`trifill`/`thickline`. It was a real
      semantics shift (software per-pixel fills, so cost now scales with *visible* area) and it has
      no decision record.
    [`design/rasterization-consistency.md`](design/rasterization-consistency.md); the regression
    commands are in [`guides/checks-and-oracles.md`](guides/checks-and-oracles.md).

16. **Packaging & public distribution** *(not started)* — dreamengine only runs as a dev
    build today. A dev-only icon + app name stopgap landed this session (the running app was
    a generic "Electron"); real packaging (bundler, `.icns`, code-signing/notarization, load
    the built renderer instead of `localhost:5173`) is unaddressed. The actual blocker isn't
    Electron — it's that the ▶ run model shells out to a developer's `clang` + Homebrew raylib,
    which a consumer machine doesn't have; web/wasm is the likely public path. Full breakdown:
    [`design/packaging-distribution.md`](design/packaging-distribution.md). Related: browser
    URL-sharing (item 10), iPad runtime (item 11).

17. **Frame-spanning sequence scripts** *(idea — from the Picotron API comparison; needs design)* —
    the *useful half* of Lua's `yield`/coroutines: write time-based logic (cutscenes, scripted
    AI, juice sequences, dialog) as **linear top-to-bottom code** — `walk_to(100); wait(30);
    say("hi"); wait(60); …` — instead of a `switch(state)` shredded across `update()` calls.
    C has no native coroutines, but the pattern is emulable with **protothreads** (Dunkels'
    switch/case macro): stackless, so locals that must survive a `wait` go in `de_state()`.
    **Distinct from the cut "process / coroutine model"** below — that was a full DIV-style
    Level-2 *execution model* (every entity its own process); this is a *small optional helper*
    for sequencing, not a new way to structure carts. Open question is whether a macro trick fits
    dreamengine's "readable C" ethos, or whether it's better shipped as a documented example cart
    than as core API. Worth prototyping one `sequence`/`wait` helper to feel the ergonomics.

18. **Blend tables** *(idea — from the Picotron manual; the substantive capability gap)* —
    index-only translucency/fog/tinting/additive via a precomputed lookup `result = blend[src][dst]`,
    staying entirely in the 32-color palette (**no RGB, no real alpha** — just a table that says
    "drawing color `a` over existing color `b` resolves to `c`"). Unlocks the things carts fake
    with `fillp` dither today: translucent water/glass, fog, additive glows, drop shadows. This is
    a real *capability* dreamengine lacks — `pal()` swaps and `fillp` are the closest, neither
    blends with what's already on screen. Deliberately framed as a lookup table so it does **not**
    trip the "splits the color model" concern flagged on the `lerp_color`/`rgb` parked thought
    (item 2) — the output is always a palette index. Picotron pairs this with stencil clipping;
    that's a separate, lower-value follow-on. **Design note now exists →
    [`design/blend-tables.md`](design/blend-tables.md)**, and the concept is **validated in
    cart-space**: the `blend lab` tech-demo (`tools/carts/blendlab.c`, 2026-06-04) builds
    AVG/ADD/MUL tables and blends per-pixel against a cart-owned scene fn, zero engine API.
    Verdict: the look works (additive glow / glass / fog all read correctly, in-palette), and
    the engine crux is identified — dst must be read from the *in-progress* frame (a `pget`
    last-frame read feeds back and blooms; demonstrated by the cart's `P` mode). Candidate
    implementation: shader + per-scope canvas snapshot, the decision-0007 lane. Next step: ADR —
    **after the palette decision**: blend tables are computed *from* the palette, and the default
    palette (lifted verbatim from PICO-8) should become our own / settable first, or #18 bakes the
    borrowed palette one layer deeper. See [`design/palette-and-color.md`](design/palette-and-color.md)
    (Picotron findings + three-layer plan: new default → `palette_set` + `de:palette` chunk →
    tables-from-active-palette).

19. **Per-cell parameter locks in the cr-78 cart** *(cart-space idea, zero engine API — parked
    2026-06-05)* — Elektron-style p-locks for `tools/carts/cr78.c`: each lit step optionally
    carries a pitch offset (melodic bongos/congas/metal riffs, TR-727 style) and a cutoff
    override (hats opening across the bar). Historically cheeky on purpose: CR-78 voices (1978)
    + the cart's swing knob (LM-1, 1980) + p-locks (Machinedrum, 2001) in one box. Pitch is
    trivial (`fire()` already passes midi). **The known crux is the filter**: one-shot cutoff
    lives on the instrument slot and scheduled notes snapshot their slot at fire time
    ([`design/audio-notes.md`](design/audio-notes.md) §2.2) — per-step cutoff therefore needs
    the sfx-editor **rotating scratch-slot pattern** (cr78 uses slots 9–24; 25–31 free, pool of
    2-3 suffices at one-step lookahead). UI sketch in the cart header: hover+wheel = pitch,
    C+wheel = cutoff, notch markers on the 9×7px cells. Full design notes at the top of
    `tools/carts/cr78.c`.

21. **More classic boxes — the museum shortlist** *(cart-space, zero engine API — parked
    2026-06-05)*. **The ranked list moved 2026-07-30 to
    [`design/cart-library-direction.md`](design/cart-library-direction.md#2e-the-museum-shortlist--more-classic-boxes-moved-here-2026-07-30)
    §2e**, the doc that was already citing it four times — a curated backlog belongs with the other
    backlogs, not in a shipped/open ledger. The curatorial line (**analog-circuit machines only**,
    no sample-playback boxes) and the shipped family (cr78 + tr808 + tb303 + sh101 + tr909 + tr606)
    are recorded there. Still open; still ranked; `STATUS #21` still means this.


**Smaller open items (no design doc yet):** looping ambience (`drone`)/`volume`/mute. Noted
in [`POLISH_TODO.md`](POLISH_TODO.md).

**Noise is value-noise + the seeding idiom** *(new 2026-06-09, surfaced building the
`procplaces` procgen testbed)* — the finding, the axis-aligned-artifact caveat and the
**`noise3(x, y, (float)seed)` seeding idiom** moved 2026-07-30 to
[`design/api-notes.md`](design/api-notes.md) §5, beside the noise signatures themselves.
Still open there: a named `noise2_seeded()` helper and/or documenting the idiom in `studioDocs.js`.

**`sprite-draw.js` next steps** — the gap audit is DONE (`ovalfill`, `rrectfill`, `ngonfill`,
`polyfill`, `noise` added 2026-06-04); the remaining low-priority wishlist (Tier-2 primitives,
`hflip`/`rotate90`, a stress-test cart, migrating the terrain carts onto `noise()`) moved
2026-07-30 to [`guides/cart-authoring.md`](guides/cart-authoring.md#sprite-drawjs--the-programmatic-sprite-library).

22. **Mobile-web readiness** *(new 2026-06-05, born from the live gallery + first
    real-device session)* — desktop-authored carts meet phones now. Shipped from
    the worklist (both 2026-06-05): ~~`tools/mobile-lint.js`~~ static checker
    (verdicts rank by *best* phone-usable input path; first `--site` run over 50
    carts: 3 touch-ready, 34 tap-as-mouse, 5 fixable, 2 keyboard-only, 6 no-input)
    and ~~gallery input badges~~ (`build-site.js` requires `lint()` and stamps a
    colored chip per card — 🟢 Mobile Ready / 🟡 Mostly Playable / 🟠 Rough on
    Mobile / 🔴 Desktop Only; strict: any dead input path demotes, hover/wheel
    cores rank rough, and a hand-tested `"mobile":` field in index.json
    overrides the lint; `fixable` shows as desktop-only until touchControls
    lands). Still
    open: a per-cart `fit` setting (letterbox / stretch / integer-scale) flowing
    `.cart.js` → `de:settings` → shell (radios are the first `"stretch"`
    customers), the **device-facts trio** (`touch_available()` grown into
    `touch_available`/`device_class`/`touch_ceiling` — researched 2026-06-06
    incl. the iPad-pretends-to-be-a-Mac detection traps and the
    ceiling-safeguard pattern against the iPhone 6th-finger mass-cancel;
    **`touch_ceiling()` SHIPPED same day** — shell computes `Module.deTouchCeiling`,
    EM_JS read, 4-place wiring, touchpiano header prints "max N fingers";
    `touch_available`/`device_class` still open, design in mobile-web-notes §5),
    and the formal touchlab-on-iPhone
    checklist run (>5-touch assumptions — iPhone Safari caps at ~5 simultaneous
    touches, found on-device; tiny tap targets). Full design + device findings:
    [`design/mobile-web-notes.md`](design/mobile-web-notes.md).

23. **The sprite story — two sprite sources of truth** *(new 2026-06-06, surfaced by the
    editor publish button but it already bites without it)*. A cart's sprites can come from
    (a) the **sprite editor's canvas** (exported as `build/sprites.png` on every run — what
    you see is what ships) or (b) a **`.cart.js` generator** (ASCII art / sprite-draw.js
    programs, rebuilt by `make-cart.js`/`build-site.js` at bake time). They do not know
    about each other: repaint a generator-cart's sprites in the editor and your pixels ship
    in that build but the generator still owns the repo truth — the next CLI bake silently
    reverts them. Same applies to plain sprite touch-ups: there is no path from editor
    pixels back to `tools/carts/<name>.cart.js`. Options to explore: a pixels→`.cart.js`
    exporter (slot arrays, lossless), an explicit per-cart marker for which source owns
    sprites, or a publish-time conflict warning (the editor's publish log already warns
    when a `.cart.js` exists). A fourth option — a **patch/overlay layer** (diff your
    hand-edits against the generator output, store the diff, apply it after each bake;
    fingerprint-stale patches discard, blessed ones promote into source) — is the
    human-in-the-loop answer; all four are weighed in
    [`design/editor-cart-workflow.md`](design/editor-cart-workflow.md) §Gap 2 (options
    A–D). **Option D SHIPPED (2026-07-10) — the bake + persist halves.** `tools/lib/sprite-patch.js`
    is the slot-level overlay core (fingerprint the generator OUTPUT, not the source; per-slot
    stale-drop; wholesale-regenerate self-empties). `make-cart.js` composites a sibling
    `tools/carts/<name>.sprites.patch.json` over the generator on every bake and mirrors the
    surviving patch into the `.cart.png` as `de:spritepatch`. The editor's **save-to-source** now
    diffs the sprite canvas against the (re-run) generator and writes only the changed slots as that
    patch — hand-edits to a generator cart survive the next CLI bake, the generator stays a live
    program. **Discard + indicator SHIPPED (2026-07-10):** the pixels tab shows a bar naming the
    hand-owned slots on load (from `de:spritepatch`, threaded through the cart-load handlers) with a
    **"discard hand-edits"** button (`cart:discard-sprite-patch` — delete the sibling + re-run the
    generator + drop the chunk + reload the canvas). CLI discard (`rm …sprites.patch.json` + rebake)
    still works. Gates: `node tools/…` core + bake sims + a discard-logic sim (see the design doc);
    the editor UI needs a live eyeball (main.cjs/preload changed → `make`). Rule stands for
    *hand-drawn* carts (no generator; their pixels already live in the `.cart.png`); generator carts
    now round-trip. Edge: a cart whose committed `.cart.png` sprites already DRIFTED from its
    generator will capture that drift as a patch on first save — defensible (preserves what's shown),
    resolved by a clean `--run` rebake.
25. **`ui.h` — the on-device pass + the remaining retrofits** *(v1 shipped 2026-06-07; see the
    landed table)*.
    - **The on-device probe run has never happened**: two knobs at once, fat fingers, the 5-touch
      ceiling. Everything in v1 was verified on desktop and by scripted replay, so the touch claims
      are argued, not measured — which is the one thing a widget kit should not ship on.
    - **Further retrofits**: modrack's knob rows, `sfxed`, the wave editor.
    *(Cut from v1 and still cut: `panel` + `drag-from`. The per-widget second-customer rule found
    their named customers speculative; they wait for a cart that actually wants them.)*
    [`design/ui-widgets-notes.md`](design/ui-widgets-notes.md) §7.

26. **Editor hand-editing workflow** *(new 2026-06-06 — explored, sliced)* — three gaps when
    a human edits carts in the editor instead of via `tools/carts/` + CLI: (a) **no
    save-in-place** — every save is a Save-As dialog (`currentCartFile` only feeds the
    publish slug); fix = track the loaded cart's origin path, Cmd-S writes back (gallery
    carts excluded — shared registry + build products), keep the existing thumbnail. (b) the
    **sprite story** = item 23; lean recorded: ownership marker (`spriteSource:
    "editor"|"generator"`) now, lossless pixels→`.cart.js` exporter for hand-drawn carts
    later. (c) **gallery metadata**: descriptions CSS-clamped to 3 lines with no way to read
    the rest, and no UI to author `index.json` entries — fix slices: full-description detail
    view (no hazard), then a metadata form that emits a paste-ready index.json entry
    (registration deliberately stays a CLI act — the shared-registry rule holds); the
    self-describing `de:meta` chunk + generated index is parked as a direction. Proposals +
    priority table: [`design/editor-cart-workflow.md`](design/editor-cart-workflow.md).

27. **Web debug overlay v2** *(v1 shipped 2026-06-07; see the landed table)* — two readouts that
    would close the gap with the native debug workflow:
    - the cart's **`watch()` values pushed per frame via EM_JS**, so the native watch workflow works
      on a phone;
    - the **`web_tm_*` mouse-synth state** readout (which is what item 24's tap-as-mouse fix built).
    Both are one-file republishes by construction — the shell bakes only a ~25-line loader and all
    overlay logic lives in `runtime/debug-overlay.js`. Zero-code alternative for a deep dive: iPad +
    cable + Mac Safari remote Web Inspector.
    [`design/mobile-web-notes.md`](design/mobile-web-notes.md) §6d.

28. **Library headers — the two discoverability leftovers** *(the engine-source viewer moved into
    the docs tab 2026-06-07; see the landed table)*:
    - **(b)** autocomplete offering `ui.h`/`gestures.h`/… inside an `#include "` quote;
    - **(c)** *maybe* the starter cart mentioning the lane in a comment.
    Function-level autocomplete/hover for header symbols stays deliberately **out**: that is keyed to
    `studioDocs.js` and the four-places contract, which is engine-API surface only. A cart-land
    header's own top-comment is its manual, by the lane's contract.

29. **Sub-pixel camera — thin features shimmer when panning at fractional zoom** *(new
    2026-06-09, surfaced by procplaces' zoomable world)* — `camera_ex(int x, int y, float
    zoom, float angle)` takes **integer** world coordinates. At a fractional zoom (e.g.
    0.55×) a smoothly-moving camera can only snap to whole world units, and 1px features
    (road curbs, dashes, grid lines) land on different fractional screen pixels each frame
    as the world scrolls — so they crawl/shimmer while panning. Static, they're fine. Same
    *class* of problem as sloop's bright-curb jitter (commit fe5553f), but that was a
    velocity-lead wobble (fixed cart-side by low-passing the lead); this is the deeper
    integer-camera limitation underneath. **Cart-side workaround in place** (procplaces):
    suppress thin markings below ~0.55× zoom — wide solid fills are pixel-stable, and the
    detail is invisible zoomed out anyway. **A real fix is engine-level:** give the camera
    a sub-pixel float position and snap world→screen to the zoom pixel grid (so `zoom *
    cam` stays integer), or offer a `camera_exf(float x, float y, …)`. Until then any
    cart drawing thin lines in a zoomable/pannable world will shimmer at non-integer zoom.
    Probably belongs with the rasterization work — [`design/rasterization-consistency.md`](design/rasterization-consistency.md).

30. **Docs/context hygiene — shrink what every agent loads** *(new 2026-06-10, surfaced
    while the per-agent context cost started "being felt in more agents")*. The docs are
    tight and high-value; the problem is *load shape*, not volume. Diagnosis + the one move
    already taken:
    - ✅ **DONE — CLAUDE.md is now a router, not a reference.** The 197-line "Game feel"
      essay (~31% of the file) was always-loaded into *every* agent every session despite
      being task-specific. Extracted to [`guides/game-feel.md`](guides/game-feel.md), left a
      ~10-line pointer. CLAUDE.md 628 → 443 lines. **Next candidate:** the live-inspection
      recipe in CLAUDE.md's "Debugging carts" section largely duplicates
      [`guides/debug-harness.md`](guides/debug-harness.md) — could become a pointer too.
    - **Add a `stereo.md`-style "STATE + next bite" header to the big design docs.**
      `instrument-engines.md` (1,455 lines) and `audio-notes.md` (1,238) force a large read
      to orient; a 20–30 line top block (current state, ✅ shipped, next concrete step) lets
      an agent orient cheaply — the pattern that let stereo.md convey the whole stereo+effects
      gate in one small read. Split past ~1,500 lines, keeping stable §-anchors (lint-docs
      already guards refs).
    - **Consolidate layered corrections.** ✅ **DONE for the effects decision (2026-06-10).**
      It was the worst offender — 4 reads for 1 truth (0015 + its appended Correction +
      `instrument-engines.md` §8.10 + its banner). Folded the wah correction into 0015's "Why"
      lead (now correct in place; Correction shrunk to a dated tail) and trimmed the §8.10
      banner's redundant preamble. **General pattern stays open** but explicitly *not a project*:
      fold corrections into the primary text + push superseded rationale to a clearly-marked
      tail, opportunistically, never as a 154-file sweep. Append-only is honest but taxes every
      future reader — fix a doc's lead when it's outright *wrong*, not just append-decorated.
    - **Keep favoring queryable state over prose** — `cart-status.js` / `lint-docs.js` /
      `lint-carts.js` let an agent *ask* "what's stale / do refs resolve" instead of reading
      everything. That's the right direction; more of it beats more prose.

31. **Engine tuning — some modeled engines play out of tune** *(new 2026-06-10, found by
    the new `tune-check.js`)*. Run `node tools/tune-check.js` for the live per-engine cents
    report (SINE is the 0¢ control); full first-run audit + the *why* in
    [`design/audio-notes.md`](design/audio-notes.md) §18. **The to-do list, worst first:**
    - ~~**`INSTR_PIPE` (flute) — the bad one.**~~ **FIXED 2026-06-11.** Was an octave low and
      progressively flat (A2 −13¢ → A5 −159¢): the bore was sized a full wavelength but the
      inverting open-end reflection resonates at SR/(2·delay), so it rang an octave down, and the
      uncompensated jet+filter loop delay added the flatness. Fix in `sound_pipe_start`: half-
      wavelength bore minus a loop-delay term **derived from the note-on jet length** (`1.69 +
      0.308·jetLen`), sized with the bowed-string fractional-read trick to kill integer
      quantization. The jet-length term is the key: the embouchure macro (morph) sets the jet, and
      a constant left morph≠0 sharp by up to a semitone — deriving it keeps the flute in tune
      across the whole embouchure range. **Now in tune within ~±3¢ from C4 up to ~E6 at typical
      embouchure** (verified at morph 0.70, the showcase recipe; robust across seeds). First
      customer: `air.c`'s Cherry flute register reopened 67–83 → 64–86.
    - **Hollow presets (recorder/breathy/pan-pipe) — FIXED through A5, 2026-06-16 (commit 97a794e).**
      The jet loop-delay `1.69+0.308·jetLen` under-compensated at long jets (morph ≲ 0.5) → flat to
      ~−56¢ by G5. Added a clamped-linear jet-delay correction past jetLen 5 (measured need
      SATURATES at ~+0.8 sample), zero at jetLen ≤ 5 so flute/piccolo are byte-identical. All 5
      presets in tune through A5; morph-0 extreme improved (A5 −84¢→−32¢). audio-notes §18 #8.
    - **Residual (minor): the morph≈0 / hollow TOP OCTAVE (above ~A5) still mode-flips** — at a
      ~20-sample bore the jet ≈ the bore, so the oscillator sits on the overblow edge and flips mode
      (the `tune-check.js` default sweep, morph 0, still flags PIPE A5 — now −32¢, was −84¢). Any
      real recipe stays ≤ A5 here. Fully closing it needs a jet-length re-voicing (jet ∝ bore).
    - **The `--quiet` gate now waives the documented residuals (2026-07-10):** PIPE A4 −13.9¢ /
      A5 −32.2¢ (the morph-0 ramp above) + BRASS A5 −13.6¢ (the macro-0 remnant of e458af1) are
      blessed in `KNOWN_RESIDUALS` in `tools/tune-check.js` with a ±6¢ drift band — CI exits 0 in
      the accepted state and trips only on NEW drift (or a residual that got worse). They still
      print, marked waived. Fix one for real → delete its line there.
    - ~~**`INSTR_PLUCK` / `INSTR_REED` / `INSTR_BRASS` — flatten at the top** (A5 −17 to −25¢).~~
      **FIXED 2026-06-16 (commit e458af1).** The "integer-sample delay-length quantization, fix =
      fractional read tap" diagnosis was **wrong** — the reads already interpolate. Real fix:
      REED/BRASS sized the note-on bore from a truncated integer delay (→ sharp high notes, dense
      sweep showed BRASS C#6 +64.5¢) → use the true fractional delay as init reference; plus subtract
      the bell-LP loop group delay `(1−lpCoeff)/lpCoeff` (BRASS ×0.5, REED ×1.0). PLUCK: −0.5 on the
      tap for the Karplus averaging's exact half-sample delay. All in tune now — audio-notes §18 #7.
    - **In tune, no action:** SINE/MALLET/EPIANO/PD/PIANO/GUITAR/FM, and **BOWED** (≤ +3¢ —
      whatever's off about the bowed voice, it is *not* pitch; though its default bow PRESSURE
      wants a bump — [tuning-handoff.md](tuning-handoff.md) → NEXT). **`INSTR_ORGAN`** reads an octave low but is in
      tune (+3–7¢) — that's the 16′ sub-octave drawbar, expected.

32. **Split `runtime/sound.h` per-engine to cut the parallel-agent collision surface** *(new
    2026-06-11, surfaced when a parallel commit silently clobbered a PIPE tuning fix)*. `sound.h`
    is one **~8,800-line** file (it was ~4,300 when this was filed — the argument has doubled
    with it) every audio change touches, edited by several agents in one shared
    working tree — so a stale full-file edit committed over a neighbour's change silently reverts
    it (no git conflict; "different content" only). Two clobbers happened this session: a
    build-breaking half-finished refactor, and a PIPE `loopDelay` reverted to an older line (still
    compiled, just out of tune). **Cheap guards already shipped** — the `.githooks/pre-commit`
    compile-gate + the CLAUDE.md "Hot shared source files" protocol (no full-file `Write`,
    re-Read before edit, grep HEAD after commit, `tune-check.js --quiet` for engine edits).
    **The structural fix (this item):** carve each engine's `sound_<eng>_start`/`_sample` pair
    (and the FX processors) into their own `#include`d headers — `runtime/engines/pipe.h`,
    `brass.h`, …, `runtime/fx.h` — leaving only the shared `Voice` struct, the dispatch switch
    (`sound_engine_sample`), the mixer callback and the public API in `sound.h`. Then an agent
    voicing the flute edits `engines/pipe.h` only; a brass agent edits `brass.h` — engine work
    stops colliding. Notes: it's a **pure textual move, zero runtime change** (all stays
    `static`/`inline` in the one TU `studio.c` includes), so verify **byte-identical** after
    (`soundcheck` + a cart or two rendered to WAV, `wav-analyze.js` "bytes identical" + unchanged
    `tune-check.js`). The `Voice` struct stays shared (every engine's state lives in it), so adding
    a *new* engine still touches the struct + dispatch — collision surface drops a lot, not to
    zero. The refactor is itself one massive `sound.h` commit, so it only lands cleanly in a
    **quiet window** (freeze other audio agents, split, verify, unfreeze) — else it becomes the
    very clobber it prevents. High-effort; only worth it if you keep running several audio agents
    at once. Until then the cheap guards hold the line.

33. **Instrument bank + orchestra tuner** *(new 2026-06-11, **PARKED behind a prerequisite** —
    full spec in [`instrument-bank-plan.md`](instrument-bank-plan.md))*. Two complementary things
    on one source of truth: a machine-readable **registry** of the named dependable voices
    (engine + macros + register + tuning verdict) that carts `#include` instead of copy-pasting
    floats out of [`guides/instrument-recipes.md`](guides/instrument-recipes.md), and an
    **orchestra-tuner cart** — the audible/visual *audition* counterpart to the headless
    `tune-check.js` gate (plays a voice against a sine reference so you *hear* the beating, shows
    the baked cents needle so you *see* the verdict). Architecture: `tools/presets.json` →
    `gen-presets.js → runtime/presets.h` (mirrors `gen-tcc-symbols.js`). **Groundwork done:** a
    doc↔cart reconciliation audit found **zero float drift** (the doc is a clean seed) and pinned
    the vanilla anchor per family; PIPE is the only tuning hazard (only `pipe/flute` m0.70 safe).
    **Why parked:** the per-voice control surface beyond `h/t/m` isn't unified — `eng_tune` is
    EXPERIMENTAL ("not a final API"), VOICE uses bespoke `voice_nasal`/`voice_consonant`. **Land a
    clean 4th-axis/aux-param API first**, else the preset schema can't capture bowed-pizz /
    guitar-fundamental / voice-nasal. Locked decisions, schema, and build order live in the plan
    doc. *(The audit's "radio voices missing from the recipe docs" subtask was a false positive —
    resolved on inspection: they're all present and value-accurate in [`radio-voices.md`](guides/radio-voices.md) +
    [`instrument-presets.md`](guides/instrument-presets.md), where radio voices belong; the audit checked `instrument-recipes.md`.)*

34. **`line()` draws axis-aligned lines as unreliable `GL_LINES`** *(new 2026-06-11, found via the
    `raycaster` cart)*. `line()` in `runtime/studio.c` is a bare Raylib `DrawLine` → GPU `GL_LINES`.
    For perfectly **vertical** (and horizontal) lines at integer coords, the GL diamond-exit rule
    darkens/drops individual columns — a raycaster (one vertical line per screen column) shows it as
    regular dark vertical bars. GPU/driver-dependent, so it can surface without any code change.
    **Worked around per-cart** in `raycaster.c` (vertical wall slices now use `rectfill(x, y0, 1, h, …)`
    instead of `line`), but the **root-cause engine fix is deferred**: special-case axis-aligned lines
    inside `line()` to draw as a filled rect (`DrawRectangle`) — fixes every cart drawing vlines/hlines,
    but touches the hot shared `studio.c` so it needs a compile-gate + a look at any cart relying on
    the current 1px GL behavior. Deferred deliberately (cart fix unblocked the regression).
35. **CPU-shader demo polish** *(new 2026-06-12, after the shadelab → caustics → raymarch trilogy
    + `shadermath.h` shipped)*. Two parked ideas, both fully spec'd in
    [`design/cpu-shaders.md`](design/cpu-shaders.md): **#3 make the per-pixel cost visible** — a
    live evals/ms/FPS HUD line on the shader carts so dropping `ps` 3→1 viscerally shows why GPUs
    exist (cheap; profiler data already exists); **#5 a true offscreen buffer** for proper
    multipass/ping-pong (blur, bloom, clean previous-state sampling) — lower priority and an
    *engine* change (a `RenderTexture2D` carts can draw into + sample), since the feedback shader
    already fakes ~80% of the intuition on the live canvas.

38. **Boutique-effects leftovers (low priority)** *(2026-06-15)* — the boutique-pedal arc is essentially
    done: **shipped** grains-pitch, the modulation kit (`mod_randwalk`/`mod_sh`/`mod_optical`), Univibe,
    dropout, the `fx_order` 16→32 packing widen, Shallow Water, amp-noise, the noise gate, and the trophy
    **Shimmer** (octave-up bus pitch-shifter) — see [`design/boutique-pedals-roadmap.md`](design/boutique-pedals-roadmap.md)
    + `audio-notes §17` #19–#27. What's left is small/optional:
    - ✅ **MOOD varispeed — DONE 2026-06-15.** `varispeed(speed)` (navkit `half_speed` port): tape
      playback-speed dive/bend of the whole mix, slewed (tape inertia), exact octave at 0.5. Master-stage,
      byte-identical at 1.0. Showcase `varispeed`. **The original boutique-pedal lists are now done.**
      (`tape_stop`/`rewind` triggered-gesture cousins remain unported; the swept varispeed covers the dive.)
    - ✅ **Shimmer in the pedalboard — DONE 2026-06-15.** Added as a SHIMMER macro pedal (kind -3) driving
      master `shimmer()` — an output-stage ambience (like a reverb at the end of the rig; chain position
      cosmetic). Cart-side only (no engine); default board byte-identical. A *reorderable* per-bus
      `FX_SHIMMER` insert is still possible later but wasn't needed for "hear it in the rig."
    - **Engineering nits** (may never matter — measure first): `fast_tanh` Padé approximation for the
      soft-clip *only if* it shows up hot in the profiler; an **AC128 transfer-curve LUT** for a *named*
      vintage-germanium fuzz voicing beyond `DRIVE_ASYM`; exposing the Shimmer pitch-shifter as a
      standalone bus effect. Full detail: the roadmap's "Side quests" + "Follow-ups" sections.
    - **King Tubby multi-head dub delay** (`dub_loop.h` port) — dub *character* is already cart-side
      (`echo`+`tape`, the `dub` cart); a faithful port adds the **multi-head** taps + integrated
      degradation, which needs `sound.h` (echo is single-tap). Sized in
      [`navkit-porting-handoff.md`](guides/navkit-porting-handoff.md) → queue; gate on 0015 first.

39b. **`fx_mod` deferred targets — reverb/delay sends + wah** *(2026-06-15)* — `fx_mod`/`fx_lfo` shipped
    with 7 targets ([0018](decisions/0018-effects-keep-params-but-become-modulatable.md) "Shipped"). The
    ADR sketch also listed `FXMOD_REVERB_SEND`/`_DELAY_SEND`/`_WAH`, **dropped from v1 because each needs a
    NEW param before it can be a target**, not just an enum value: reverb/echo sends are per-*voice*
    (`v->rvb`/`v->eko`), so there's no bus-level return-gain to ride — add one first; wah is envelope/LFO-
    driven with no manual position — add a manual-sweep mode first. The `FXMOD_*` enum leaves room to append
    them (7+, no renumber). Small, additive `sound.h` work; do it when a cart actually wants these swept.

40. **Spatial audio v3 — acoustic zones** *(2026-06-15)* — v1 (per-voice) + v2 (emitter buses) SHIPPED;
    the remaining layer is environment: **inside/outside reverb zones, occlusion (a wall between you and
    the source → muffled, low-passed), and material absorption (carpet vs tile)**. The encouraging part:
    the DSP already exists — occlusion = a low-pass driven by "how blocked" (`FILTER_LOW`/`note_cutoff`),
    zones = reverb size/send by which zone the listener is in, materials = an EQ/decay tweak. So it's
    mostly **cart-side zone logic feeding existing knobs**, with maybe a thin convenience helper — *more
    game logic than engine DSP*. Full spec + the v1/v2 build record: [`design/spatial.md`](design/spatial.md)
    → "v3 — acoustic zones". Free-field (direct path only) until built.
    - **PROBE SHIPPED 2026-06-15 — `acoustics` cart** (`"probe"` kind): a top-down walker over
      tile/carpet/wood/grass rooms + a wall/doorway, occluding two emitters via a listener→source
      raycast. Confirmed the mechanic is cart-side over existing knobs (`note_cutoff` for the muffle
      is great — Hz + slewed). **Two real engine asks dropped out:** (a) **`note_gain(handle, float)`** —
      `note_vol`'s 0..7 is too coarse for smooth occlusion attenuation (audible level steps); (b) **a
      rideable / crossfadable zone reverb** — set-and-hold means you can't slew `reverb()` per frame, so
      crossing rooms jumps abruptly. Build plan: a cart-land **`acoustics.h`** helper (raycast occlusion
      + zone/material tables → the knobs) + those two engine conveniences. A tilemap **line-of-sight**
      primitive is an optional bonus (generalizes to vision/stealth). Findings in the cart header.
    - **Engine ask (a) SHIPPED 2026-06-15** (commit `5184a88`): **float `note_vol` + `note_res`** — both
      now `float`, ranges kept (0..7 / 0..15), input quantization dropped → byte-identical for every existing
      caller (int literals promote to the same float). Superseded the earlier separate-`note_gain` sketch.
      **Engine ask (b) still DESIGNED** (ready to build when `sound.h` is free) in
      [`design/spatial.md`](design/spatial.md) → "Designed solutions": rideable reverb = slew the tank's `fb`/`damp` toward targets, gated
      on a new `reverb_glide(ms)` (snap by default → byte-identical; `reverb()` re-call is already cheap —
      just no slew). Two-tank crossfade noted as the v3.1 higher-fidelity fallback.
    - **Floating `note_vol` cleared the second-customer bar independent of v3 — SHIPPED 2026-06-15**
      (commit `5184a88`; was overdue, not speculative — cart survey 2026-06-15). The live per-note surface is float *everywhere* (`note_cutoff` Hz,
      `note_pitch`/`duty`/`pan`/`drive`/sends, the macros) **except two integers: `note_vol` (0..7) and
      `note_res` (0..15)** — the stragglers from before the float-everything convention. **8 carts already
      compute a continuous level then crush it through `note_vol`** via `(int)(level*7+0.5f)` in a per-frame
      loop. Tier 1 (the level *is* the gesture): **`martenot`** (the *touche d'intensité* souffle, quantized
      to ~6!), **`glassharmonica`** (a lerp'd swell the code calls "the glass-harmonica swell"), **`bowed`**
      (bow energy), **`musicalsaw`** (bow speed), **`mouthharp`** (breath). Tier 2: **`modrack`** AMP CV (a
      modular VCA on a smooth CV → 8 steps). Tier 3 (modest): `trafficjam`/`trackgen` engine-by-speed.
      Retrofit = *opportunistically* drop the `(int)(…*7)` for `note_vol(h, level*7)` (no forced migration —
      old call sites stay valid). **Sibling: `note_res` float** (0..15 → continuous) — smaller (16 steps,
      subtler), 2 customers (**`modrack`** RES CV + **`brass`** mute sweep). One-shot velocity
      (`note`/`hit`/`tone` vol 0..7) stays int — transients don't perceptibly step. **Worth its own STATUS
      item** given it's justified beyond spatial — the cleanest, lowest-risk audio change on the board.

41. **Waveguide down-bend — the carts still built around the old limit** *(the engine fix shipped
    2026-06-16 for BRASS/REED/PIPE/BOWED; see the landed table)*.
    - **`upright.c`** (the upright bass, `INSTR_BOWED`) hard-codes an **up-only** pull-bend
      (`fabsf(dpx)` → always `+vbend`) and uses fret-walk re-articulation for downward motion
      *because the engine couldn't bend down*. Its description still says so. Now it can: make the
      pull-bend **signed** (pull down → smooth flatten) above ~E2, keeping the fret-walk as the
      bottom-of-range fallback *and* as the deliberate walking-bass articulation.
    - **`pdbass.c`** was spun off *only* to get a two-way slide (`INSTR_PD` oscillator). Still valid
      as the "buzzy CZ" variant, but the upright no longer needs the workaround.
    - Update both carts' descriptions when revisited.
    ⚠️ **Caveat that bounds the fix — BOWED's low register is buffer-capped.** A full wavelength is 2×
    a half-wave engine, so at the bottom of the range the buffer is already at `SOUND_KS_MAX` (1024)
    with no room to lengthen. Measured on the bass: down-bend works from ~**E2 up**, but **E1 (the
    open low E, ~41 Hz) cannot bend down at all**. A bigger `ks_buf` (more RAM per voice) or a coarser
    low-note bore would extend it — only worth it if a cart needs sub-E2 portamento.
    [`design/waveguide-bend-handoff.md`](design/waveguide-bend-handoff.md).

42. **Audio test coverage — the two leftovers** *(audit 2026-06-16; five gates shipped out of it)*.
    The audit asked "what isn't tested?" and produced **`level-check.js`** (loudness regression),
    **`fx-check.js`** (effect stability at documented extremes), **`lint-fx-frame.js`** (the
    set-and-hold footgun), **`soak-check.js`** (+ the denormal guard) and **`web-audio-check.js`**
    (native↔wasm codegen parity). All five are indexed by task in
    [`guides/checks-and-oracles.md`](guides/checks-and-oracles.md); the findings they produced — the
    uneven library (BOWED peaking −1.2 dBFS, the per-engine makeup trims), the phaser's and echo's
    persistent DC at extreme feedback, and the worklet sample-rate bug — are written up in
    [`design/audio-notes.md`](design/audio-notes.md) §20 and
    [`design/web-audio-parity.md`](design/web-audio-parity.md).
    What is still open:
    - **On-device confirmation of the worklet sample-rate fix.** The bug (the desktop-default worklet
      backend played **+147¢ sharp on any non-44.1k device**, because the context took the device rate
      with no resampler) was found *by reading the source* and fixed the same way. It has never been
      confirmed in a browser, and the resample quality has never been listened to.
    - **The varispeed/amp-noise post-clip seam.** `amp_noise_process` + `varispeed_process` run
      *after* the master soft-clip and the device output has no final clamp (only `sound_wav_write`
      clamps), so varispeed's interpolation can push the device signal just past ±1.0 into a hard
      driver clip. Tiny, but real.
    *(Was 106 lines, ~95 of them ✅-marked write-ups of shipped tools. A lesson from inside it worth
    keeping: **a "republish" follow-up written next to a source fix goes stale silently**, because the
    next unrelated mass publish quietly satisfies it — this entry carried one for six weeks, and an
    audit later reported it as a live user-facing bug when it had been fixed all along.)*

43. **VISUAL test-coverage blind spot — a golden-pixel-diff harness** *(2026-06-23, from the streetlab corner work)*.
    The `spec()` harness (`tools/spec.js`) tests **pure functions and state** — it pinned every geometric *quantity*
    in streetlab (`curb_return` tangency, `kerb_start`, `round_flare`, lane offsets; 70 assertions). But it can't
    test the **rendered pixels**: those are the output of `polyfill`/`line` scan-converting onto the framebuffer —
    engine-level rasterisation, not a pure function of the cart's inputs. The concrete case that exposed this: on a
    symmetric 4-way the four corner kerbs can land ≤1px apart in *staircase arrangement* (arc rasterisation on an
    even-width grid). Geometry is symmetric and spec'd; the difference is pure quantisation. To actually catch/fix
    that at the pixel level you'd need a **golden-image / pixel-diff** test — render a cart headless (the `--dump`
    path already exists, used for clips), compare regions (e.g. the four corner quadrants) pixel-for-pixel against a
    committed golden, or assert a symmetry invariant (corner A == rot180(corner B)). Visual-regression style, the
    twin of the audio `level-check`/`fx-check` baselines but for the framebuffer. We have `ui-audit.js` (draw-box
    *bounds* analysis from the DE_TRACE box log) but **no pixel-level diff**. Note we already keep some pixel-exact
    test/probe carts + deterministic `--dump`, so the rendering side is the missing piece, not the capture.
    **RESOLVED (2026-06-23) — harness built + the floor fixed.** The harness shipped as
    `tools/mirror-diff.js` (renders headless, reflects the framebuffer about cx/cy, counts mismatched
    mirror-pairs — the symmetry-invariant flavour; a committed-golden flavour is still future, see
    [`design/software-canvas.md`](design/software-canvas.md)'s determinism note). Using it, the streetlab
    floor was diagnosed and FIXED: the corner **fill** (`polyfill`) was *already* mirror-symmetric (it's
    CPU-decided); the whole floor was the kerb **stroke** (`line()` → GPU `DrawLine`, direction-dependent).
    A symmetric software line (`sline`) does NOT fix it — it *regresses* the kerb 7→27 because fills are
    point-mirrored (about cx=160) and 1px strokes are cell-mirrored (sum 319 vs 320) — an even-grid snap
    conflict. The fix that shipped is **`mirror_blit`** in `streetlab.c` (reflect the rendered junction-core
    pixels; guarded to the symmetric 4-way): kerb **7→0**. Full writeup +`sline` recipe:
    [`design/streetlab-corner-symmetry-plan.md`](design/streetlab-corner-symmetry-plan.md). Probes: `arcsym`
    (blit principle), `linesym`/`axissym` (primitive symmetry, any axis). **API candidates (not yet
    promoted):** `sline` is the reflection-symmetric CPU line the software canvas needs (the only
    *axis-aligned* primitive still letting GL pick pixels — the rest of that surface is the rotation/
    texture family: `rectfill_rot`/`spr_rot`/`sspr_ex`/`tritex`, see `design/software-canvas.md`);
    `mirror_blit` could become a reusable engine helper. Rule of
    thumb still holds for the un-fixed cases (skew/T/free-right): **spec the geometry, eyeball the pixels.**

44. **Tool-output hygiene — one predictable home for tool artifacts** *(2026-06-25, surfaced from the
    [cart-os](design/cart-os.md) discussion but independent of it)*. Tool outputs are scattered across
    ad-hoc locations with no naming convention: `build/.bake/<name>/screenshot.png`, `build/screenshot.png`,
    `--dump <dir>`, `--trace <f>`, `build/.bake/*_request` (the live-inspection mailbox). The concrete
    papercut: `build/.bake/<name>/screenshot.png` is the fresh baked frame, while **`build/screenshot.png`
    drifts** (it belongs to the running editor) — enough of a trap that CLAUDE.md carries it as an explicit
    "NEVER read this one" gotcha. That's a recurring "which file is the fresh one" mistake, and it's pure
    convention, not a missing capability. **Lever is naming/layout discipline, not a new system** — a single
    documented namespace (e.g. everything tool-emitted under `build/out/<tool>/…`, the editor's own scratch
    kept clearly separate) would retire the gotcha class. Explicitly **NOT** the cart-os shared-FS idea: the
    dev tools already compose fine through files; this is just tidying where they land. Low leverage, no rush.

45. **Two carts open at once, as an editor feature** *(attach-profiling the running cart shipped
    2026-07-10 and is the groundwork; see the landed table)*. The handle table now tracks every
    running cart, which unblocks the rest: a **side-by-side UI**, and **per-run build dirs** so
    concurrent runs stop sharing one `build/`. That is the cart-os Tier 3 project.
    (A new Run deliberately does **not** kill the previous native cart — several at once stays
    supported, the OS mixes them, and `play.js netdemo` relies on it.)
    [`design/cart-os.md`](design/cart-os.md) → "Why you can't open two carts today".

47. **✓ MOSTLY DONE 2026-06-29 — the engine runs WITHOUT Raylib (the `DE_NO_RAYLIB` platform seam)**, the
    foundation for iOS/Switch (iOS Phase 2 / Path B). The real `studio.c` + `sound.h` compile, render, and
    sound with zero Raylib / zero frameworks, verified headless on desktop: omnichord (2D) + **heroes**
    (tilemap+sprites) **pixel-identical** to Raylib; **tb303** audio **byte-identical** to the Raylib `--wav`.
    Built: `runtime/platform.h` (host↔engine seam) · `color.h` (universal `DeColor`) · `raylib_compat.{h,c}`
    (no-Raylib shim: types/enums + ~94 stubbed prototypes, a few real — text metrics, rng, decode) · baked
    ROM fonts (`tools/bake-fonts.c` → `fonts_baked.h`) · vendored `stb_image.h` sprite decode · `de_init`/
    `de_frame`/`de_framebuffer`/`de_audio_render` · `tools/headless-nr.c` (proof harness: frame→PPM, audio→WAV).
    Decision settled: **two renderers behind one seam** (software now, GPU/Metal later). Kept desktop
    bit-identical throughout (build-all, canvas-diff 0px) — every path is `#ifdef DE_NO_RAYLIB`. Full record:
    [`design/engine-portability.md`](design/engine-portability.md).
48. **✓ DONE 2026-06-29 — the REAL engine renders + sounds on iOS (Phase 2 / spike 8).** omnichord (real
    `studio.c`+`sound.h`, zero Raylib) renders **pixel-correct and upright** on the iPhone 15 simulator
    (`ios/history/spike8-omnichord.png`), CoreAudio pulls the real mixer, and UIKit touch drives it (a
    desktop strum through the same `de_touch_*` path goes silent→0.374 peak). Wiring: `ios/project.yml`
    compiles `studio.c`+`raylib_compat.c`+`build/cart.c` with `-DDE_NO_RAYLIB`/`SCALE=1`; `ios/build.sh`
    regenerates the cart via play.js (the "swap a cart" loop, extended to iOS); `CanvasView` flips the
    bottom-up `sw_cbuf` + maps touches to framebuffer px → `de_touch_*` (newly given bodies in
    `raylib_compat.c`); `AudioEngine` splits `de_audio_render`'s interleaved stereo. `tools/build-nr.sh`
    is the desktop recipe. **The AUv3 extension is now a playable instrument rack** hosting the real
    engine: its render block parses host MIDI → `de_midi_event()`, sample-clocks `de_frame()` (the
    cart's keybed plays the notes), and pulls `de_audio_render()`. `AUHostTests` proves it offline —
    silent with no MIDI (peak 0.000), then a host note-on → peak 0.106. Engine seam: `midi_input.h`
    gates CoreMIDI to desktop and exposes `de_midi_event`/`de_midi_bend` (portable host-feed, like the
    web bridge). **The renderer decision is settled** — on-device FPS measured on an iPhone SE 2nd-gen
    (`ios/measure-device.sh`): 2D holds 59–60fps (~5.6ms, ~⅓ budget), `tritex`/3D ~89ms→~10fps →
    [ADR-0024](decisions/0024-software-canvas-is-canonical-for-2d.md) (software canvas canonical for 2D,
    `tritex`/3D GPU-only). **GPU-parity audited + closed:** `pal()` (0px) and scaling were never gaps,
    and **camera rotation now works on the software canvas** (offscreen world layer → rotate-composite;
    a 25° probe is 0.04% off the GPU), so the rotation carts render on iOS. Only `smooth_zoom`'s AA
    degrades (→ plain zoom). **Input wired:** touch→mouse synthesis (the primary finger drives
    `GetMousePosition`/`IsMouseButton*`, so mouse carts play from touch + the harness can drive them)
    + a `de_key_event` key seam — proven by injecting a tap into `hotline` (mouse_pressed → gameplay).
    Open follow-ups (incl. the recommended next) in [`design/ios-plan.md`](design/ios-plan.md) →
    "Phase 2". Full record there too.

51. **Cart collections — the relationship half** *(collections shipped 2026-07-14, 62 carts tagged;
    see the landed table)*. This is field-notes 003's OTHER half:
    - the cart-to-cart **relationship fields** (`replaces` / `successor` / `related`);
    - an **editor gallery filter** by collection;
    - a generated **`docs/collections.html`** ★ page (the visual twin of the other generated pages).
    Schema: [`design/cart-metadata.md`](design/cart-metadata.md#collection-doc-anchored-cross-cutting-threads);
    [`tools/collections.js`](../tools/collections.js) is the CLI roll-up today.

52. **Synth Secrets — phases 3 and 4** *(audit 2026-07-28; phases 0-2 shipped 2026-07-29/30, see the
    first changelog entry above)*. The engine was cross-checked against Gordon Reid's 63-part **Synth
    Secrets** (SOS 1999-2004), supplied as a PDF — copyrighted, **not in the repo**, cite by part +
    issue. That produced 98 findings in three shapes: **§A what already matches** (recorded so nobody
    "fixes" it), **§B ten drifts** (each with a book ref, a `sound.h` line, and the cart it would be
    heard in), **§C twelve additions**.
    - Findings ledger: [`design/synth-secrets-audit.md`](design/synth-secrets-audit.md) (2,700 lines).
    - The ordered work: [`design/synth-secrets-plan.md`](design/synth-secrets-plan.md), which carries
      the live per-item state and is the file to read before picking this up. **Phase 0 done ·
      Phase 1 7/7 · Phase 2 3.5 of 4 · phases 3-4 untouched.** Two items are recorded DROPs and one
      was a false premise — the plan says which and why, because a dropped item that looks merely
      unfinished gets re-proposed.
    - In-flight detail (what to resume mid-lane) lives in [`HANDOFF.md`](HANDOFF.md), not here.
    *(This entry was 229 lines — 15% of the file — and titled "deliberately NOT queued" while the
    changelog above recorded two of its phases as shipped. A ledger row should not restate a 4,800-line
    pair of design docs; it should say where they are and whether the work is live.)*

---

## Decided-against / deferred ✗

These were considered and **cut** — kept here so the decision isn't relitigated.
Rationale lives in [`design/api-notes.md`](design/api-notes.md)'s "What to defer or skip" and its
[**External brainstorm review** (2026-05-30)](design/api-notes.md#external-brainstorm-review--divmmfsim-ideas-weighed-against-the-carts-2026-05-30)
— which is where the entries dated `2026-05-30` below were decided.

- **`pedalboard`: on-screen text labels** — nut string names, fret numbers, a chord-name readout.
  Cut 2026-07-30 ("i dont like that"): two of the three were already answered by the interface, and
  the one real gap (orientation) wasn't worth the space even text-free. The generalized rule —
  *check what the instrument already says before adding a label; a screenshot has no state and no
  hands* — is [`design/design-system.md`](design/design-system.md#66--legibility-floor) §6.6.
- **Fixing the 3×5 font's malformed `N`** (it's a Π — no diagonal fits at 3×5). Cut 2026-07-30: a
  shared asset in two places + a `canvas-diff` re-bless, for a glyph read from context anyway. The
  defect, the 74-cart blast radius and the **use `FONT_TIC` instead** pattern are recorded at
  [`design/font-rendering.md`](design/font-rendering.md#known-defect-font_tinys-n-is-a-π-and-why-were-not-fixing-it).
- **`pedalboard`: booting with the cabinet on / a default amp voicing.** Cut 2026-07-30 — it keeps
  booting `AMP: OFF` and the player picks. This resolved a long-standing open fork, so the decision
  lives with the fork, along with the four-way A/B measurement (the cabinet is ~1 kHz of centroid and
  ~17 dB RMS, i.e. the whole story) and why a clean power chord is *acoustically* thin:
  [`design/effects-bus-architecture.md`](design/effects-bus-architecture.md#e5-ui-shape--open-forks-for-the-pedalboard-cart) §E.5.

- **A cart-making PLATFORM (outside authors)** — creator accounts, cart upload/sharing from
  strangers, a browser IDE for others, a public compile-strangers'-C server. **Cut**
  (2026-06-22, [decision 0020](decisions/0020-in-house-tool-curated-showcase.md)): dreamengine
  is an in-house tool; the public surface is a **curated showcase** people view (clips) and
  play (wasm), not contribute to. Resolves the old "Sharing the work" open question. The
  tinyjam racks *product* line ([`design/product-notes.md`](design/product-notes.md)) is a separate
  question, unaffected.
- **Process / coroutine model (DIV-style)** (2026-05-30) — the would-be "Level-2" learning model.
  Every shipped cart works cleanly with plain typed static pools, so it's weeks
  of coroutine/transformer machinery for a model we don't need. [`VISION.md`](VISION.md) ·
  [decision 0001](decisions/0001-cut-coroutine-process-model.md).
- **Engine-owned entity system** (God-struct / `SELF` / `val[16]` / ECS) (2026-05-30) — "the big
  rabbit hole" of that review; per-game typed pools with *named* fields beat all of them for a
  learn-C console.
- **MMF movement/qualifier engines** (`move_platform`, `move_8dir`) (2026-05-30) — removes the lesson.
- **`move_and_collide`** (resolved tile push-out) (2026-05-30) — low demand; only `platform.c` does
  the full pattern, and `zelda`/`gta` test against their own data, not `mget`.
- **DS structures** (lists/maps/grids) (2026-05-30), **memory arenas**, **PS1 z-sort/ordering table**,
  **tools-as-carts / VFS / fantasy-OS / peek-poke**, a **3D *engine*** (scene graph / mat4
  stack / z-buffer / per-pixel depth) — out of scope. *(The small 3D leaf-helpers
  `rot3`/`project3`/`zsort`/`quadfill` + `V3` ARE shipped — see below and [decision 0009].)*
- **`hud()` and `game_over_screen()`** — one-call "draw a whole HUD / draw a whole game-over
  screen" helpers. **Cut** for the reason that a no-param convenience for a *look* makes every
  cart look the same: a shared `hud()` would give 572 carts one identical status bar, which is the
  opposite of the point. The same concern is what keeps a no-param `explode()` a research topic
  rather than a build item (item 1). *(This entry written 2026-07-30 — item 1 has said "see
  Decided-against" since the beginning and the target was never actually here.)*
- **Particle systems** and **pathfinding** (2026-05-30) — ship as *library carts* (seeds: `astar.c`,
  `boids.c`), not engine surface.
- **Pixel-perfect sprite collision** (2026-05-30) — eventually maybe; AABB covers 95% first.
- **Turtle graphics API** (`turtle_*`/`pen_*`) — shipped, then **removed 2026-06-01**: only
  `16-spirograph.c` used it, and a turtle is just `dx`/`dy` + `line()`, so it lives in the
  cart now. [decision 0008](decisions/0008-cut-turtle-graphics-api.md).
