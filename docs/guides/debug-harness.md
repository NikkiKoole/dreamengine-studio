# Debug harness — play a cart "together"

A reproducible way to watch a cart run input-by-input: record a play session and
replay it deterministically, or script exact inputs yourself, while a per-frame
trace tells you precisely what the engine did. Built for the class of bugs you
can't see by reading source — timing, input, "why did nothing happen when I
pressed the key."

The whole thing is **off by default and native-only**. A normal run (the editor's
▶ button, the web build) touches none of it.

---

## Why it's possible

The game clock is frame-driven, not wall-clock-driven where it counts: the musical
beat advances from the `dt` fed to `sound_tick()` once per frame (`runtime/sound.h`).
So pinning the timestep makes the *entire* game — falling notes, beat, judge
windows — reproducible. The only other non-determinism is `now()`/`dt()` (the wall
clock) and the two RNGs; the harness pins all three.

---

## Runtime flags (`runtime/studio.c`)

The compiled cart binary accepts:

| Flag | Effect |
|------|--------|
| `--det` | fixed 1/60 timestep + seeded RNG → byte-for-byte reproducible |
| `--seed N` | RNG seed (default 1) |
| `--record FILE` | log live key changes as `<frame> <keycode> <down>` (one per change) |
| `--replay FILE` | drive input from a recording, deterministically (implies `--det`) |
| `--script FILE` | drive input from a hand-authored frame script (implies `--det`) |
| `--trace FILE` | one JSONL line of state per frame (see "Telemetry" below) |
| `--uiaudit FILE` | one JSONL line of every draw call's bounding box per frame (+ each `ui.h` widget rect). Powers `tools/ui-audit.js` — see "UI audit" below |
| `--frames N` | stop after N frames (use for batch/headless runs) |
| `--dump DIR` | export a screenshot filmstrip |
| `--dump-every N` | … every Nth frame (default 1 with `--dump`) |
| `--headless` | hidden window (for batch replay/script) |
| `--save-dir DIR` | where `cart.sav`/`cart.kv`/`cart.blob` live (default: cwd). The editor and `play.js` pass `saves/<cart>` automatically, so every cart gets its own save folder under `build/saves/` — harness runs can't clobber (or inherit) another cart's hiscores |

`--record` and `--trace` flush every frame, so a live session can be tailed.

Note this flag is **not** harness-only — it works in any native build, like the
`.bake/` trigger files. A normal editor ▶ run uses it too.

A recording (`--record`) replays under `--replay`; both feed the same injection
engine as `--script`, so they behave identically.

---

## Telemetry: the `DE_TRACE` convention

`--trace` writes the auto fields every frame — `{"f":frame,"t":seconds,"beat":n,
"bpos":fraction,"w":{…}}` — plus every `watch()` value the cart set that frame, in
`w`. Reading a session is reading this file.

To make a cart's internals legible, wrap `watch()` calls in `#ifdef DE_TRACE`:

```c
#ifdef DE_TRACE
    watch("songpos", "%.3f", songpos);
    watch("press", "L%d e=%.3f r=%s arm=%d", L, err, rating_name, armed);
#endif
```

They cost nothing in a normal build (the macro isn't defined) and feed the trace
in a harness build. `runtime/studio.c`'s `watch()` already drives the on-screen F1
overlay and the crash dump; this just tees it to a file. See `tools/carts/smooch.c`

> **⚠️ `watch()` is printf-style: `watch(name, fmt, ...)` — NEVER `watch(name, value)`.**
> A bare-value call (`watch("x", x)`, no format string) is the old, dead signature and is a
> **compile error**. The trap that makes this recur (it's bitten loderunner, galerijflat, …):
> watch calls live under `#ifdef DE_TRACE`, so the **normal editor build never compiles them** —
> a wrong call sails through the editor and only fails under `play.js`/trace, *silently shipping a
> cart that can't be debugged.* Catch them across the whole catalog with **`node tools/build-all.js`**
> (it compiles every cart with `-DDE_TRACE`). Format like printf: `%d` int, `%.1f`/`%.3f` float,
> `%s` string — e.g. `watch("px", "%.1f", px)`.
for a worked example.

---

## The driver: `tools/play.js`

Compiles a cart from its source of truth (`tools/carts/<name>.c` + optional
`.cart.js`) **with `-DDE_TRACE`**, then runs a harness mode. Beat-scripts are
compiled to frame events here, so you author in musical time, not frame counts.

```bash
node tools/play.js <name> run                  # windowed live play
node tools/play.js <name> record <out.rec>     # play live, capture inputs
node tools/play.js <name> replay <in.rec>      # replay a recording
node tools/play.js <name> beats  <in.beats>    # compile a beat-script + run (you drive)
node tools/play.js <name> script <in.script>   # run a raw frame-script as-is
```

Options pass through: `--trace --frames --dump --dump-every --headless --seed --bpm`.

### Beat-script format

```
bpm 96            # tempo for beat->frame conversion (or --bpm)
start 2           # press SPACE at this frame; beat 0 lives here (default 2)
beat 20 tap S 3   # at beat 20 press S, release 3 beats later (a hold)
beat 4  tap A     # at beat 4 tap A briefly (default 0.12 beat)
frame 900 down L  # raw frame events still work and pass through
```

### Mouse (point-and-click carts)

Record/replay capture the **mouse** too (canvas-space pointer + L/R/M buttons), and
a raw frame-script (`<name> script file.script`) can drive it:

```
move  100 160 120      # move the pointer to canvas (160,120) at frame 100
click 120 204 135      # at frame 120: move there + left-click (a 4th arg: 1=right 2=middle)
down  130 N            # keys still work alongside (open the notebook)
up    132 N
click 165 160 100      # click to close it
```

That makes mouse-driven carts (neon rain, the economy game, …) fully scriptable and
replayable — `mouse_x/y/mouse_pressed` all read the injected pointer under replay.

**Touch-path carts too (2026-06-07):** the injected pointer also becomes the
synthetic mouse-touch, so `tap()`/`tapp()`/`touch_*` and per-finger-pointer carts
(touchpiano, handpan, sh101) are equally script-drivable — one finger's worth.
A drag needs the pointer to move *while the button is down*; `click` holds for
3 frames, so put `move` lines on the frames right after it:

```
click 60 300 70        # land on the VCF cutoff fader (capture)
move  61 300 40        # drag up while the click is still held → the fader follows
```

### Gotcha: injected keys can hit the pause overlay

Scripted input goes through the same path as real input, so the **runtime pause
overlay** reacts to it: `ENTER` opens it (freezing `update()` — your trace's
`watch()` fields go empty `{}` from that frame), and while it's open **Z/ENTER
confirm** and **UP/DOWN move the selection** — an injected `Z` meant as a cart key
will instead select *Continue* and unpause. Keys the cart itself reads are
*claimed* and never trigger pause (`P` is safe in a cart that polls `P`), but a key
the cart does **not** read is fair game for the overlay. If a scripted run
mysteriously freezes mid-trace, check the script for a stray `ENTER`.

---

## The three ways to "play together"

1. **Record → replay & inspect** — you play once, then
   `node tools/play.js <name> replay you.rec --headless --frames N --dump shots/`
   re-runs your exact session; read the trace / filmstrip to autopsy any moment.
2. **Scripted (you drive)** — author a `.beats` file to hit an exact scenario
   (a perfectly-timed hold, a 1-frame-late press) and read the result.
3. **Live tail** — `node tools/play.js <name> record live.rec` in one terminal,
   `tail -f build/<name>.trace.jsonl` in another, narrate as you play.

---

## Live inspection — on-demand snapshot

While any cart runs (any mode, any backend), you can pull a screenshot or state
snapshot at the exact moment you care about. Write the desired output path into a
trigger file; the game captures the current frame on its next tick, then deletes
the request as the handshake — **gone = fresh and ready to read.**

```bash
# screenshot at the exact moment you want
echo "/abs/path/to/frame.png"  > build/.bake/screenshot_request

# state snapshot (frame + time + all active watch() values as JSON)
echo "/abs/path/to/state.json" > build/.bake/state_request

# profiler snapshot (same schema as build/perf.json — frame budget + draw-call counts)
echo "/abs/path/to/perf.json"  > build/.bake/profiler_request
```

All three work simultaneously. The game creates `build/.bake/` on startup, so the
directory always exists. `profiler_request` works in any native build — you do not
need `-DDE_PROFILE` or the `⏱ profile` button; the counters run in every normal run.

## Clip capture — an animated GIF / WebM of a cart (`tools/make-gif.js`)

The visual twin of `--wav`: render a cart's *motion* to a file. It's a thin wrapper over
`play.js --dump` (which exports the **native-resolution canvas** as one PNG per frame —
the real pixels, *before* the on-screen window scale) followed by an ffmpeg assemble. So a
clip costs no new runtime code, and because the harness is deterministic, **a clip is a
reproducible build artifact** of a committed input file — re-run it after an engine change
and get the identical animation.

```bash
node tools/make-gif.js boids                      # self-animating cart — no input needed
node tools/make-gif.js dinorun --from you.rec      # replay a recording (a human playing)
node tools/make-gif.js epiano  --from solo.beats   # scripted showcase (authored inputs)
```

The `--from` file's extension picks the harness mode (`.rec`→replay, `.beats`→beats,
`.script`→script); with no `--from` it does a headless `run`, which is all a self-animating
cart needs. Options: `--frames N`, `--fps` (default 30; carts run at 60, so it dumps every
2nd frame), `--scale` (integer upscale, **default 1 = native**), `--crf` (webm/mp4 quality,
default 30), `--start N` (drop boot/title frames), `--format webm|webp|gif|mp4|apng`,
`--seed/--bpm/--screen` (passthrough), `--keep` (keep `build/.gif/<name>/` for inspection).

**Capture native, scale at the consumer.** Capture is always native res; the file stays
native unless you ask for `--scale`. The web page upscales crisply with
`image-rendering:pixelated`, so a per-game loop wants `--scale 1` (smallest). Bump to `2–3`
only for a **standalone** GIF/README where nothing else does the scaling — that upscale is
**nearest-neighbour**, so edges stay hard. **Never capture the 4× window**: 16× the pixels,
already upscaled, re-scaling fights the grid.

**Crisp by construction.** GIF uses a single global palette with `dither=none` (dithering
would shimmer our flat ≤32-colour fills); WebM is VP9 at `yuv444p` (full chroma — no colour
bleed on pixel edges); mp4 is `yuv420p` for reach. The weight levers, in order, are
**duration → `--crf` → scale** — *not* resolution. Native + crf 30, a sloop city-drive
(8s, full-screen scroll = worst case): **webm 556 KB**; trim to 3–4 s and it's ~200 KB. Use
`--crf 16` for a near-lossless hero clip, `--crf 40` for max-tiny grid loops.

**Reproducible clips — the `tools/clips/` recipe home.** A clip's input track is the
*source of truth*; commit it under `tools/clips/<cart>/NN-label.{script,beats,rec}` and bake
with `--recipe`:

```bash
node tools/make-gif.js sloop --recipe 01-autodrive   # reads tools/clips/sloop/01-autodrive.script
  → editor/public/clips/sloop/01-autodrive.webm       # the history-page convention, verbatim name
node tools/make-gif.js --all                          # rebuild EVERY committed clip from its recipe
```

A recipe is **self-describing**: `# frames N`, `# fps N`, `# scale N`, `# crf N` comment
lines (ignored by the runtime script parser) travel with the track, so each clip rebuilds at
its own length — a CLI flag still overrides. Without `# frames`, a `.script`'s length is
derived from its last event + a tail. `--all` is what makes the whole `clips/` tree
regenerable from committed sources, the same way carts rebuild from `tools/carts/`.

**The output convention.** `--clip <label>` (or `--recipe`, verbatim) writes to
`editor/public/clips/<name>/NN-label.<ext>` — a **sibling of `carts/`**, not nested:
`carts/` stays a flat store of cart data, `clips/` is the media root (a peer of `palettes/`
under `editor/public/`). One cart = one folder, any number of clips; the `NN-label`
filename carries order + caption with no sidecar metadata (strip `NN-`, dash→space). The
history generator globs `clips/<name>/*.{webm,gif}` and uses the `.cart.png` as the
poster/fallback — so a clip needs zero per-cart wiring. (`--clip` without a leading `NN-`
auto-assigns the next number; `--from` ad-hoc with no `--clip`/`--out` lands in
`docs/media/<name>.<ext>`.)

## WAV capture — hear what the engine actually rendered

Two ways to get the engine's audio output as a WAV (16-bit mono 44.1 kHz), one
analysis tool. Design + measurements: `docs/design/audio-notes.md` §15–16.

**Live capture** — same trigger-file handshake as the snapshots above. Line 1 =
output path, optional line 2 = seconds (default 5, cap 60). Records the *next* N
seconds from the running cart's audio thread:

```bash
printf "/abs/path/out.wav\n10\n" > build/.bake/wav_request
# request file vanishes immediately; the WAV appears N seconds later
```

**Deterministic render** — the `--wav` flag (play.js passes it through). The
audio device stream is never started; the main loop renders exactly 735 samples
(44100/60) per frame, so same cart + same script + same seed + same frames →
**byte-identical WAV**. Headless-safe; this is the golden-WAV primitive:

```bash
node tools/play.js house script /dev/null --headless --frames 3600 --seed 7 --wav /tmp/a.wav
```

**Analysis** — `tools/wav-analyze.js` (no deps): peak/RMS/crest dBFS, clipped
samples + runs, DC offset, per-second RMS envelope. Two files = compare mode
with a bytes-identical check (regression diffing):

```bash
node tools/wav-analyze.js /tmp/a.wav            # report (add --json for machines)
node tools/wav-analyze.js /tmp/a.wav /tmp/b.wav # compare; "bytes identical: true" = no DSP change
```

The worked example: the §15 voice-budget experiment rendered the house cart at
8 vs 16 voices with identical scripts — the byte-diff *is* the starvation
measurement (identical = never starved; different = voices were being stolen).

**Stereo / panning / effects-bus work — `--stereo`.** The default report downmixes
L+R→mono, so it's *blind to the stereo field*. `--stereo` reads the channels
separately and reports per-channel peak/RMS, **balance** (R−L dB), inter-channel
**correlation** (+1 mono · 0 wide · <0 anti-phase), a per-second **pan trajectory**
(a `L··@··R` meter), and the **mono-fold test** — sum to mono and measure power lost:

```bash
node tools/wav-analyze.js /tmp/a.wav --stereo   # per-channel + stereo-field (add --json)
```

The mono-fold loss is the [`stereo.md`](../design/stereo.md) **bite #6** check: ~0 dB =
mono-compatible, ~−3 dB = naturally wide/decorrelated (fine — where amplitude panning
lands), **< −6 dB = WARN**: a phase/delay width trick (Haas, ping-pong) that'll
comb-cancel to near-silence on a mono phone speaker. Amplitude panning is phase-safe and
never trips it; this is how you catch a width effect that *isn't*. The pan trajectory is
how the `pan_law()` A/B was measured — a steady tone swept −1→+1 traces straight across
the meter. (Ear-checking a 3 dB pan-law difference failed; this measured it cold.)

### A/B against navkit (the reference synth)

Most of our instrument engines are **ports of navkit** (`~/Projects/navkit/soundsystem`),
the sibling synth they came from — so when a ported voice sounds "off," the fastest
diagnosis is to render navkit's *original* and compare the actual audio, not to theorize.
(Porting a *new* instrument/effect over, rather than debugging one? The full playbook —
port the oscillator verbatim, the gotchas, the checklist — is
[`porting-from-navkit.md`](porting-from-navkit.md); this section is the A/B *tooling* it leans on.)
navkit ships a pile of headless sound tooling that needs no raylib/audio device:

| navkit tool | what it does |
|---|---|
| `tools/preset_audition.c` | **THE one to know** — headless preset→WAV: `preset_audition <idx> -n <midi> -d <on s> -t <total s> -o out.wav`. `-a` = analyze-only, `--ref <wav>` = compare against a reference, `--all` = every preset |
| `tools/wav_analyze.h` | navkit's WAV metrics (peak/RMS/crest/…), the analog of our `tools/wav-analyze.js` |
| `tools/golden_wav_gen.sh` + `golden_wavs/` | reference renders of demo songs + checksums (regression catch) |
| `tools/song_render.c` / `daw_render.c` | render a whole song / DAW project offline |
| `engines/instrument_presets.h` | the preset bank — search it for the index (e.g. `instrumentPresets[180].name = "Clav Funky"`) |

`preset_audition` has no raylib dependency, so it builds standalone:
```bash
cd ~/Projects/navkit/soundsystem
clang -O2 -o /tmp/nav_audition tools/preset_audition.c -lm
/tmp/nav_audition 180 -n 60 -d 1.5 -t 2.0 -o /tmp/nav_clav.wav   # navkit "Clav Funky", middle C
```
Then render *our* equivalent voice (a cart, `play.js … --wav`), and compare both —
`wav-analyze`'s two-file mode for levels, or an FFT for spectrum. A real worked example:
diagnosing why our INSTR_EPIANO clav sounded "messier" than navkit's, an FFT of both
(middle C, no wah) showed our **noise floor 28 dB higher** (−90 vs −118 dB) + spurious
inharmonic partials — the kind of thing ears flag but only a spectrum pins down.
**Gotcha:** isolate the note — our epiano's preset-select fires an *audition* note that
rings ~1 s and contaminated the first measurement (a phantom partial at the audition's
pitch); play the analyzed note seconds after any audition, with autoplay off.

#### A/B a bus EFFECT (not a preset voice) — drive navkit's *real* effect code

`preset_audition` renders **voices**, not the **bus effect chain** — it never calls
`processBusEffects`, so there's no ready-made navkit tool to render echo/chorus/tremolo/phaser/etc.
The temptation is to copy navkit's formula into a standalone harness and diff it against your
`sound.h` copy — **don't**: that tests your copy against your copy and never touches navkit. The
honest A/B drives navkit's *genuine* functions, and there's a **saved kit** for exactly this (built
during the tremolo + phaser ports):

| tool | what it does |
|---|---|
| `tools/navkit-fx-render.c` | renders navkit's REAL bus effect over a pure sine → mono WAV. Links navkit's actual `setBus*`+`processBusEffects` (via `synth.h` **then** `effects.h` — `effects.h` needs `applyDistortion` from `synth.h`). `clang -O2 -o /tmp/navfx tools/navkit-fx-render.c -lm` then `/tmp/navfx tremolo 5.5 0.7 0 out.wav 3 587` or `/tmp/navfx phaser 0.5 0.7 0.3 0.5 4 out.wav 6 587`. Add a new effect = one more `else if` calling its `setBus*` |
| `tools/wav-correlate.js` | **sample-level A/B** — normalized cross-correlation of two WAVs (1.000 = identical DSP up to a constant gain + small delay). The definitive check; robust to our engine's output-gain offset |
| `tools/wav-modrate.js` | extracts an amplitude-modulating effect's **LFO rate + depth** from a WAV, level-independent (for tremolo / auto-pan / a phaser's wobble on a steady tone) |

> **Gotcha (cost an hour):** navkit's `effects.h` does `#define fx (fxCtx->params)` (+ `delayBuffer`,
> `reverbComb1`, …). Name a local `fx` in your harness and it explodes into nonsense parse errors.
> Avoid those names — `tools/navkit-fx-render.c` uses `effect`.

Render **ours** by exercising the matching API in a tiny cart (a flat-top sine + the effect — see
`tremtest.c` / `phasertest.c`) with `play.js … --det --wav`, then compare. Two comparison modes,
pick by effect:
- **sample-level** (`wav-correlate.js`) — the strongest: same deterministic sine in ⟹ identical DSP
  gives ~1.0 correlation regardless of level. The phaser port hit **0.99999** at two settings.
- **characteristic** (`wav-modrate.js`, or an FFT) — when you want the *number*: tremolo matched
  **5.513 Hz / depth 0.700** on both, at two settings (also 8 Hz / 0.4 / square).

Three gotchas that bit:
- **our `--wav` is STEREO** (`byteRate = sr·4`), navkit harness WAVs are mono — an analyzer that
  reads int16 as mono halves the detected rate on the stereo file (reads L,R,L,R as a 2× stream).
  Parse the `fmt ` channel count; take the left channel (the saved tools do).
- **don't measure off a busy source** — an autoplaying/decaying cart's envelope is dominated by note
  dynamics, not the effect. Use a steady, flat-top tone (`tremtest.c`: `INSTR_SINE`, full sustain).
- **allpass effects barely move a sine** (phaser): an allpass is magnitude-preserving, so a pure sine
  through a phaser hardly modulates (the notches live in the dry+wet *sum*, and a sine has energy at
  one frequency). That's correct physics, not a weak effect — use `wav-correlate.js` (sample-level)
  for the A/B, and test the *audible* swirl on broadband material (the EP itself), not a sine.

**Gotcha — navkit's velocity is `clampf(velocity·2, 0, 1)`.** Its volume range is ~0–0.5,
so `preset_audition -v 0.5` already clamps to full velocity. To velocity-match *our* note
(`note_on(…,6)` = 6/7 ≈ 0.857), render navkit at `-v 0.43`. Skip this and your A/B lies —
velocity drives mode amplitudes *and* nonlinearity depth.

**Gotcha — knob values do NOT transfer 1:1 between navkit and us; match by ear.** Same word,
different math. The clearest case is **drive**: navkit `tanh(sample · (1 + drive·3))` (linear,
no normalize) vs ours `tanh(s · drive²·24) / tanh(drive²·24)` (quadratic, loudness-preserving).
So navkit's panel "0.10" ≈ our 0.20 by ear — and navkit's panel number *also* hides a
`velToDrive · velocity²` term added on top. Lesson: port the **oscillator** verbatim (the modes
+ nonlinearity — those MUST be exact), but treat **effect/knob amounts as targets to dial by
ear**, not numbers to copy. (Decision recorded: we deliberately did NOT swap our global drive to
navkit's model — 24 carts depend on ours + its loudness-preservation, and navkit's hidden
velToDrive means a swap wouldn't transfer 1:1 anyway. **Option B if navkit ports get frequent:**
add an opt-in `DRIVE_NAVKIT` waveshaper mode — navkit's exact `1 + x·3` raw tanh — so ported
instruments can use navkit drive values directly while the 24 existing carts, default
`DRIVE_SOFT`, stay untouched. Not built yet; reach for it only when match-by-ear gets annoying.)

### Tuning — is each engine in tune?

`wav-analyze.js` measures *level*, not *pitch* — it can't tell you an engine is
playing sharp. **`tools/tune-check.js`** does (no deps). It renders the
[`tunecheck`](../../tools/carts/tunecheck.c) harness cart — a sweep of every
non-standard engine across four octaves of A — reads the trace to learn what each
note *should* be, then measures the actual fundamental and reports the error in
**cents** (100¢ = one semitone). `SINE` is rendered first as a control: it's
mathematically exact, so a non-zero `SINE` reading would mean the *measurement* is
off, not the engine.

```bash
node tools/tune-check.js                 # render the sweep + per-engine cents report
node tools/tune-check.js --json          # machine-readable
node tools/tune-check.js --quiet         # exit 1 if anything is out of tune (CI gate)
node tools/tune-check.js --keep          # keep the render (build/.tune/sweep.wav + trace)
node tools/tune-check.js note.wav --note 69   # measure ONE wav against a MIDI note (A4)
```

How it stays honest (the pitch-detection gotchas it sidesteps, learned the hard way):
- **Frame-indexed, not time-indexed.** The audio is frame-locked at SR/60 samples per
  frame; the trace's `t` advances at a slightly different rate, so the WAV is indexed by
  the trace **frame number** (`f`), not `t` — using `t` drifts ~18 ms/s and lands on the
  wrong note within seconds.
- **No octave errors.** Pitch is measured by autocorrelation constrained to ±18% around
  *known candidates* {expected×2, ×1, ÷2}, not a wide global-max search (which picks
  subharmonics — a sharp tone correlates better at a multiple of its period). The played
  octave wins unless another is *clearly* stronger, so a Hammond's 16′ sub-octave drawbar
  or a flute overblowing is reported as "in tune, 1 oct low/high" — separated from real
  detuning, not flagged as a fault.

The first-run findings (the why-this-exists) live in
[`audio-notes.md` §18](../design/audio-notes.md). Building a new modeled engine? Run
this — a waveguide/Karplus engine that quantizes its delay length goes flat at the top
of its range, and this is how you see it before a player does.

### Level — is each engine at the right loudness?

The twin of tune-check for *level*. **`tools/level-check.js`** renders the same `tunecheck`
sweep (`--det`, deterministic) and measures each note's peak / RMS / crest in dBFS against a
committed golden baseline (`tools/level-baseline.json`). It catches the silent regression a
compile + tune-check + dc-check all miss: an engine that got louder/quieter, or one now
slamming the master soft-clip (crest collapse → squashed dynamics).

```bash
node tools/level-check.js              # render + per-engine peak/RMS/crest report
node tools/level-check.js --save       # re-bless the golden baseline (after an INTENDED change)
node tools/level-check.js --quiet      # CI gate: exit 1 on >4 dB drift, new silence, or clip
node tools/level-check.js note.wav     # measure one WAV
```

Why a baseline (tune-check needs none): pitch has an exact target (A440); level has no absolute
truth, so the gate compares against the last blessed render. Three absolute checks need no
baseline though — SILENT (broken voice), HOT-on-its-own (a single note near full-scale → two
clip), and loudness-outlier-vs-library-median. **Run after any `sound.h` edit that could touch
levels** (envelopes, gains, an engine's amp normalize, a new shaper). First-run findings (BOWED
~+12 dB, BRASS hot at the bottom): [`audio-notes.md` §20](../design/audio-notes.md).

### Effect stability — does an effect blow up at its extremes?

**`tools/fx-check.js`** (harness `fxcheck.c`) drives a loud sustained chord into the master bus
and sets one effect at a time to its documented EXTREME (echo fb 1.1, flanger/phaser ±0.95,
filter res 0.99, …), then asserts the output stays finite/bounded: no collapse-to-silence (a NaN
through a feedback loop reads as silence in the 16-bit render), no DC runaway, no permanent
limiter-pinning, and that it actually moves the signal off the DRY reference.

```bash
node tools/fx-check.js                 # render + per-effect peak/rms/dc/clip report
node tools/fx-check.js --save          # bless the baseline (records known extremes as accepted)
node tools/fx-check.js --quiet         # CI gate: exit 1 only on a REGRESSION (got worse / drifted)
```

The DC test is the subtle bit: a finite-window *mean* mistakes a sub-sonic resonant oscillation
(what max-feedback combs/allpass produce) for DC, so it integrates over the full window AND each
half and requires both to agree in sign — true DC is a persistent bias, a wobble averages out.
It's a STABILITY gate, not a character gate (whether the reverb is *beautiful* is still by ear).
**Run after any `sound.h` effect edit.** First-run findings (the phaser/echo missing a DC blocker
in their feedback loops): [`audio-notes.md` §20](../design/audio-notes.md).

### Set-and-hold lint — an effect reconfigured every frame

**`tools/lint-fx-frame.js`** is a static check (no render) for the silent-stutter footgun: a
buffer-rebuilding effect (`crush`/`tape`/`eq`/`chorus`/`reverb`/`flanger`/`phaser`/…) called
UNCONDITIONALLY every frame in `update()`/`draw()`, which rebuilds the bus DSP 60×/s. Calls inside
an `if`/`?:` guard pass; `filter()`/`varispeed()`/`note_*` are excluded (built to ride live).

```bash
node tools/lint-fx-frame.js            # report every unconditional per-frame effect call
node tools/lint-fx-frame.js --quiet    # exit 1 if any (CI gate)
```

Waive a confirmed-safe line with `// fx-lint-ignore` (on it, or a standalone comment line above).
It inspects `update()`/`draw()` directly, not helper-routed calls (the `apply_fx()` pattern — the
right structure anyway). Fix a real hit by moving the call to `init()` or gating it on the value
changing (copy `groovebox`'s `apply_fx()`).

### Soak — long-run stability (leak / drift / denormals)

**`tools/soak-check.js`** (harness `soak.c`) renders ~64s of stress/idle cycles — dense notes through
a big reverb+echo tail, then silence — and asserts what a few-second test can't: stress level STABLE
across all cycles (no gain drift, no progressive voice-pool starvation from a leak), idle tails DECAY
well below stress (no stuck/leaked voice ringing), the idle floor doesn't CLIMB run-long (no energy/DC
accumulation), and nothing blows up. Thresholds are decay-relative, not an absolute silence floor.

```bash
node tools/soak-check.js              # render the long run + per-cycle stability table
node tools/soak-check.js --quiet      # CI gate: exit 1 on drift / leak / accumulation / blowup
```

Pairs with the **denormal flush-to-zero** in `sound.h` (`sound_set_denormal_ftz()` at the top of
`sound_callback`): a long feedback tail decays into the denormal range where FP ops are 10–100× slower
→ audio-thread CPU stutter on some CPUs, invisible in the output. The soak proves the tails decay (the
audible side); FTZ handles the CPU side. **Run after any feedback-effect or voice-lifetime edit.**

### Web parity — does the wasm build's audio match native?

**`tools/web-audio-check.js`** compiles the engine BOTH ways (clang `-O2` native vs emcc `-O2` → Node, via
the audio-only `web-audio-host.c` + a raylib shim — no graphics), renders each engine solo with identical
input, and compares the WAVs. Isolates one variable: does emscripten's compiled DSP math match native?

```bash
node tools/web-audio-check.js              # per-engine parity report
node tools/web-audio-check.js --quiet      # CI gate: exit 1 if any engine diverges audibly
```

Two-tier verdict (the lesson: sample-diff is wrong for a *chaotic* engine): an engine passes if its
native↔wasm diff sits ≥60 dB below the signal (sample-parity), OR — for a chaotic engine like BOWED, whose
stick-slip friction diverges from a 1-ULP difference — if the two renders still match in RMS level (same
note, micro-phase only). Finding (2026-06-17): 15/16 engines are sample-faithful; BOWED is perceptual-only;
the wasm math is faithful — the one real web bug was the worklet sample rate (see web-audio-parity.md).
**Run after engine-math edits that an optimizer/fast-math/libm change could compile differently.**

### Web perf A/B — does a graphics change help on WebGL? (browser GPU)

The native profiler (`perf.json`, `profile-fleet.js`) can't answer "is this faster *in the browser*"
— wasm runs under a different GL backend (ANGLE→Metal on a Mac). To A/B a draw-path change on real
WebGL you work around three things, none obvious:

1. **No env vars in a browser** → the native `DE_*=on` A/B toggles are unreachable. Add a
   **compile-time** mirror (`#ifndef DE_FOO_DEFAULT … -DDE_FOO_DEFAULT=1`) and build the cart twice.
2. **The file profiler is compiled out on web** (`#if !defined(PLATFORM_WEB)`). Measure frame time in
   JS instead: the delta between consecutive `requestAnimationFrame` callbacks ≈ the full frame period
   (it stretches above the refresh interval once the wasm frame work exceeds it). Median delta = frame
   time; lower = faster.
3. **Headless Chrome on macOS silently falls back to software GL (SwiftShader)** — *not* representative.
   Drive the **system Chrome headful** (real GPU) and assert the renderer string
   (`WEBGL_debug_renderer_info` → `UNMASKED_RENDERER_WEBGL`) is your GPU, not "SwiftShader".

> **The cap gotcha that wasted a run:** rAF is vsync-capped (~16.7ms / 60fps, or 8.3ms on 120Hz
> ProMotion). If *both* builds finish a frame under the cap, both read ~16.7ms and you measure the
> **cap, not the work** — a false "0% / identical". Make the workload clearly **exceed** the budget
> (crank a `-DSTRESS_PASSES`-style knob until fps drops well below 60) so you're comparing real work.

**The flow** (one-time `npm i puppeteer-core` — it drives the *system* Chrome, no Chromium download):

```bash
# 1. build both variants with emcc (mirror tools/build-site.js's baseArgs), e.g. the pset A/B:
WORK=/tmp/ab; mkdir -p $WORK; printf 'static const unsigned char SPRITES_DATA[1]={0};\nstatic const unsigned int SPRITES_DATA_LEN=0;\n' >$WORK/sprites_data.h; cp $WORK/sprites_data.h $WORK/map_data.h  # (rename symbols to MAP_DATA)
BASE="tools/carts/psetstress.c runtime/studio.c -I runtime -I $WORK -I runtime/raylib-web/include \
  -DPLATFORM_WEB -DSCREEN_W=320 -DSCREEN_H=200 -DSCALE=4 -DMAP_W=128 -DMAP_H=64 -DCELL_W=16 -DCELL_H=16 \
  -DTOUCH_CONTROLS_DEFAULT=0 -DRENDER_EVERY=1 -Os -fno-delete-null-pointer-checks \
  runtime/raylib-web/lib/libraylib.a -s USE_GLFW=3 -s TOTAL_MEMORY=67108864 \
  -s EXPORTED_RUNTIME_METHODS=ccall,HEAPF32 --post-js runtime/web_midi.js -DSTRESS_PASSES=12"
emcc $BASE                          -o web/off/index.js     # baseline
emcc $BASE -DDE_BATCH_PSET_DEFAULT=1 -o web/on/index.js     # the change
# 2. drop the measurement page (below) in each dir as index.html, serve, drive headful Chrome:
python3 -m http.server 8099 &       # serve web/
node drive.js                       # → "OFF median=…ms  ON median=…ms  delta %"  + renderer string
```

The **measurement page** (`index.html` next to each `index.js`) — rAF timer publishing `window.__result`:

```html
<canvas id="canvas"></canvas><script>
  var Module = { canvas: document.getElementById('canvas'), print:function(){}, printErr:function(){} };
  var d=[], last=0, WARMUP=120, KEEP=500;
  (function tick(t){ if(last) d.push(t-last); last=t;
    if(d.length<WARMUP+KEEP){ requestAnimationFrame(tick); return; }
    var s=d.slice(WARMUP).sort((a,b)=>a-b);
    window.__result={ medianMs:+s[s.length>>1].toFixed(2), p90:+s[Math.floor(s.length*0.9)].toFixed(2), fps:+(1000/s[s.length>>1]).toFixed(1) };
    document.title="DONE"; })(requestAnimationFrame(arguments.callee));
</script><script src="index.js"></script>
```

The **driver** (`drive.js`, puppeteer-core → system Chrome, headful for real GPU):

```js
const P=require('puppeteer-core'), CHROME='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
(async()=>{ const b=await P.launch({executablePath:CHROME, headless:false,
    args:['--ignore-gpu-blocklist','--use-angle=metal','--window-position=2000,2000']});
  async function m(url){ const p=await b.newPage(); await p.goto(url,{waitUntil:'load'});
    await p.waitForFunction('window.__result!==undefined',{timeout:120000,polling:200});
    const r=await p.evaluate('window.__result');
    const gl=await p.evaluate(()=>{const g=document.createElement('canvas').getContext('webgl');
      const e=g.getExtension('WEBGL_debug_renderer_info'); return e?g.getParameter(e.UNMASKED_RENDERER_WEBGL):'?';});
    await p.close(); return {...r, gl}; }
  const off=await m('http://localhost:8099/off/'), on=await m('http://localhost:8099/on/');
  console.log('renderer:', off.gl);
  console.log('OFF', off.medianMs, 'ms   ON', on.medianMs, 'ms   delta',
    (((on.medianMs-off.medianMs)/off.medianMs)*100).toFixed(1)+'%');
  await b.close(); })();
```

First use of this flow (2026-06): the pset draw-call-batching A/B — **0% on M1 WebGL** (off=on=50ms at
768k psets), matching native and falsifying "WebGL draw-call overhead is the bottleneck" for ANGLE-Metal.
Full result + interpretation: [`../design/software-canvas.md`](../design/software-canvas.md) → "Cheaper
alternatives → Measured".

### Before/after diff

```bash
# launch (compile takes ~1-2s before the game actually starts)
node tools/play.js <name> run --headless --frames 600 &

sleep 2.5   # wait for compilation + a few frames to run
echo "$(pwd)/build/.bake/before.json" > build/.bake/state_request

# ... let the interesting event happen ...
sleep 1.0
echo "$(pwd)/build/.bake/after.json"  > build/.bake/state_request

wait
diff build/.bake/before.json build/.bake/after.json
```

The two files have different `f` values — proof they were captured at distinct
moments in the same live run.

### Pre-staged (frame 0)

Write the request files *before* launching — they survive until the game picks them
up, so frame 0 is always captured even if compilation is slow:

```bash
echo "$(pwd)/build/.bake/snap.png"  > build/.bake/screenshot_request
echo "$(pwd)/build/.bake/snap.json" > build/.bake/state_request
node tools/play.js <name> run --headless --frames 3
```

### What the state JSON contains

```json
{"f": 38, "t": 0.6333, "beat": 2, "bpos": 0.8, "w": {"px": "160", "py": "88"}}
```

`w` holds all currently active `watch()` values — not just those set this exact
frame, but everything still alive (age < 60 frames). For carts without `#ifdef
DE_TRACE` instrumentation `w` will be empty, but `f`/`t` still land correctly.

---

## Worked example — the smooch-lounge hold mechanic

The question: *"do I press when the ribbon touches and release when it leaves?"*

```bash
printf 'bpm 96\nstart 2\nbeat 20 tap S 3\n' > /tmp/hold.beats
node tools/play.js smooch beats /tmp/hold.beats --headless --frames 900 --bpm 96
grep '"press"\|"holddone"' build/smooch.trace.jsonl
```

```
f752  press: L1 e=0.000 r=SMOOCH arm=1 hold=3.0      # head hit on the beat -> hold armed
f865  holddone: L1 tail=23.00 sp=23.013 +bonus       # held to the tail -> bonus
```

Press the head one window late instead (`beat 20.35 …`) and the trace shows
`press: L1 WHIFF` with **no `arm`, no `holddone`** — proving the hold never starts
unless the *head* lands as a hit, no matter how long you hold the key. That's a
discoverability finding you can't get by reading the source.

---

## Recipes — using this *today* to find & fix issues (for the next agent)

The harness is open-loop (you pre-bake inputs, then run), but with determinism that
is already enough to find and fix real bugs. The core loop is always the same:

> **reproduce → inspect the trace → locate in source → fix → re-run the *same*
> script and diff the trace.** Because runs are byte-for-byte deterministic, a trace
> diff is your regression test.

A few things to internalize before you start:

- **`--headless` is realtime-capped today** (hidden window, `SetTargetFPS(60)`). A
  900-frame run takes ~15 s; a full song (~5000 frames) ~80 s. Always set `--frames`
  to the smallest window that covers the moment you care about. (Speeding this up is
  Stage 1 in [`../design/headless-autoplay.md`](../design/headless-autoplay.md).)
- **beat ↔ frame:** `frame = start_frame + round(beat * 3600/bpm)`. With the default
  `start 2` and bpm 96, that's `2 + beat*37.5`. Let `play.js` do it — author in beats.
- **Each trace line shows only the watches set *that* frame.** `grep` by watch key
  (`'"press"'`, `'"miss"'`); absence of a key means that branch didn't run.
- A cart must opt into the trace with `#ifdef DE_TRACE watch(...)` (see smooch). If a
  cart you're debugging has none, **add them first** at the suspect code path.

### Recipe 1 — "X feels wrong at moment Y"
Script the moment, run headless, grep the relevant watch:
```bash
printf 'bpm 96\nstart 2\nbeat 20 tap S 3\n' > /tmp/r.beats
node tools/play.js <name> beats /tmp/r.beats --headless --frames 900
grep '"press"\|"holddone"\|"miss"' build/<name>.trace.jsonl
```
The trace tells you the engine's decision (error, verdict, whether a branch fired) —
not your guess about it.

### Recipe 2 — reproduce a human's exact session
```bash
node tools/play.js <name> record build/u.rec      # they play, then close the window
node tools/play.js <name> replay build/u.rec --headless --frames 5000 --dump build/u
```
Now you have their inputs *and* a frame-by-frame trace + filmstrip to autopsy.

### Recipe 3 — find the exact edge of a timing window (sweep)
Iterate-on-replay: vary one number, diff the verdict. Finds off-by-one window bugs.
```bash
for off in 19.90 20.00 20.10 20.28 20.30 20.35; do
  printf "bpm 96\nstart 2\nbeat %s tap S\n" "$off" > /tmp/s.beats
  node tools/play.js <name> beats /tmp/s.beats --headless --frames 800 >/dev/null 2>&1
  echo -n "offset $off -> "; grep -o '"press":"[^"]*"' build/<name>.trace.jsonl | head -1
done
```

### Recipe 4 — catch a crash
The runtime's `crash_handler` dumps the last `watch()` values to **stderr** on
SIGSEGV/SIGFPE/etc. Run *without* redirecting stderr and read the dump — the watches
are your "last known state" before the fault. Pair with `--seed N` so it's repeatable.

### Recipe 5 — detect a soft-lock / stall (the navkit move)
A stall = the clock advances but nothing the player cares about changes. Grep for a
flat-line: e.g. in `ST_PLAY`, `songpos` climbing while `score`/`combo` never move and
no `miss`/`press` fires for a long stretch. navkit does this exact before/after
snapshot in `RunHeadless` (`src/main.c`, the `=== HEADLESS RESULTS ===` block) — copy
the idea: run N frames, then assert "state changed in the way it should have."

### Recipe 6 — verify a fix without regressing
```bash
node tools/play.js <name> beats fix.beats --headless --frames N --trace /tmp/before.jsonl
# ...apply the fix...
node tools/play.js <name> beats fix.beats --headless --frames N --trace /tmp/after.jsonl
diff /tmp/before.jsonl /tmp/after.jsonl     # exactly what your change moved, nothing else
```

### Recipe 7 — a poor-man's autoplayer
You can play a chart *perfectly* by emitting a `beat <b> tap <key>` for each note in
the cart's chart (read it from the source). A flawless run that *doesn't* earn the
grade/award the code promises is a real bug. (Demonstrated on smooch's verse: 16
notes + 1 hold → combo 17, all SMOOCH — the engine delivered exactly what the chart
implied.) This is the seed of the fuzz/autoplay driver in the growth doc.

### Recipe 8 — live visual check (no filmstrip needed)
Run the cart, trigger a screenshot at the moment you're curious about, read it:
```bash
node tools/play.js <name> run &
# ... play until the suspicious frame ...
echo "$(pwd)/build/.bake/suspicious.png" > build/.bake/screenshot_request
wait
```
No `--dump` needed. One PNG, the exact frame you asked for, nothing wasted.

> If `jq` is available, it makes traces far nicer to slice, e.g.
> `jq -c 'select(.w.press) | {f, p:.w.press}' build/<name>.trace.jsonl`.

## Sound tripwire + the `soundcheck` cart

The sound engine counts every request it has to **drop** (ring buffer or delayed-pen full)
and printh-screams `[sound] WARNING: request queue overflow — N sound call(s) DROPPED` —
in the editor log, bake output, and play.js output. Dropped requests are the *silent* bug
class: notes that never play, `instrument()` defines that never land (the "every wav-knob
position sounds like square" incident, 2026-06-04).

`tools/carts/soundcheck.c` is the matching self-test: its `init()` issues the worst-case
define burst (27 slots × 11 calls — ADSR/duty/LFOs/filter/envs **plus the three engine
macros** — + 4 `wave_set` tables in one frame) and `update()` walks the whole sound API,
including the `INSTR_PLUCK` engine, the live `note_harmonics/timbre/morph` setters, and a
40-shot `schedule_hit` burst that stresses the delayed pen. **Grow this cart in the same
commit as any new sound surface** — a kind the walk never pushes is a kind the tripwire
never guards.

```bash
node tools/play.js soundcheck script /dev/null --headless --frames 900 2>&1 | grep "\[sound\]"
# no output = PASS. Any WARNING line = sound calls were lost — fix the engine, not the cart.
```

Run it after touching `runtime/sound.h` (queue sizes, request kinds, new `instrument_*`/
`wave_set`-style bulk APIs). Ears version: run the cart normally — the 10 waves it plays in
sequence (the last is the KS pluck engine) must all sound *different*.

## UI audit — off-screen text, overlaps & hidden panels (`tools/ui-audit.js`)

The bugs a screenshot won't tell you about: a label that runs off the screen edge (silently
clipped to nothing), two labels drawn on the same spot, or a panel that only opens on an
input you didn't try. `ui-audit.js` finds them by running the cart headless under
`--uiaudit` — which logs **every primitive's bounding box per frame** (plus each `ui.h`
widget rect via the `de_ui_audit` hook in `ui_end()`) — and reasoning over the geometry. No
cart instrumentation needed; the capture lives in the shared draw path in `studio.c`.

**Occluded text is discounted.** A panel opening over the buttons behind it is intentional,
not a bug — so text that a *later* fill (a solid modal panel, or a full-screen `fillp` dim)
fully covers is dropped before the off-screen/overlap checks run. A widget's own label is
drawn *after* its fill, so it survives; only background text painted over by something on top
is silenced. This is what stops modal-over-content screens from crying wolf.

**Animation transients are filtered too.** A real layout bug sits still; text on screen
for only a frame or two is mid-motion (a card dealing in, a number sliding into place). A
finding must persist `--min-frames` frames (default 3) to be reported — off-screen findings
are keyed by position, so a moving element makes many 1-frame entries that all fall below the
bar, while a static clip keeps one high-count entry. `--min-frames 1` shows everything.

```bash
node tools/ui-audit.js <name>                 # text off-screen + overlapping text, default play
node tools/ui-audit.js <name> --explore       # ALSO press each key the cart reads + tap each ui.h widget
node tools/ui-audit.js <name> --overlay [f.svg] [--frame N]   # SVG: screenshot + every box, colour-coded
node tools/ui-audit.js <name> --min-frames N    # a finding must persist >= N frames (default 3)
node tools/ui-audit.js <name> --json           # machine output; exit 1 if any live finding
```

- **`--explore`** is how you reach interaction-only UI. It scans the cart `.c` for the keys
  it reads (`key('x')`, `keyp(KEY_TAB)`, …), scripts a press of each, and harvests `ui.h`
  widget centres from a baseline pass to tap them — then reports which input made new UI
  appear (e.g. *`L` → +3 labels "0%" "C" "LOADED"*). The normal audit runs over every
  revealed state too. Limits: top-level widgets only, one press per key, state can carry
  between taps — so it complements, not replaces, a hand-authored `--beats` for deep menus.
- **`--overlay`** writes a self-contained SVG (open in a browser): the cart screenshot with
  every box on top — widgets yellow, panels blue, sprites grey, text green, **off-screen red,
  overlap orange**. This is how you *see* the panel/button layout the auditor reasons over.
- **`ui_get_widgets(const UiWid **out)`** (in `runtime/ui.h`) is the public accessor that
  exposes the live widget rects; a cart or debug code can read it between `ui_begin()`/`ui_end()`.

### UI lifecycle tripwire — forgot `ui_end()` (clicks silently dead)

`ui.h` widgets must be wrapped `ui_begin()` … draw widgets … **`ui_end()`** — `ui_end()` is
where this frame's presses get *resolved* to a widget (and where the `de_ui_audit` rects are
emitted). **Forget it and the buttons still draw and still light up on hover — but no click
ever registers** (no capture is ever formed). It's a silent, easy-to-miss bug; flank's start
menu shipped with it.

A `DE_TRACE`-only tripwire in `ui.h` catches it: if a frame draws widgets but `ui_end()` never
runs, it prints once to stdout —

```
[ui] WARNING: 3 widget(s) drawn but ui_end() was not called — clicks won't register (only hover shows). Call ui_end() last in draw().
```

It fires in any `DE_TRACE` build (`play.js`, `ui-audit.js`) but **not** the editor's ▶ native
build (which omits `DE_TRACE` to avoid forcing `<stdio.h>` on every cart) — so if clicks feel
dead in the editor, run the cart through either tool to see the diagnosis. `ui-audit.js`
surfaces it as a `✘` lifecycle finding (and exits 1); coverage follows the run, so use
`--explore` to reach widgets behind a trigger (a panel that only opens on a key/tap).

### Suppressing a false positive (linter-style, in the cart source)

Intentional bleed or a deliberately stacked label is waived with a comment in the cart `.c`:

```c
// ui-audit-ignore off "SCOPE" bottom    — scope label deliberately bleeds off the bottom
// ui-audit-ignore overlap "env" "vb"    — stacked readouts, intentional
```

Waivers match by **finding identity** (kind + text + side), never pixel position — so they
survive layout jitter, but a genuinely *new* off-screen string still trips. `off "X"` with no
side waives any edge. Waived findings drop out of the `✘`/`⚠` lists and the exit code (shown
as `· N waived`). A waiver that matches nothing is reported as a **stale waiver** (`⚑`) so it
gets deleted rather than rotting into a hidden regression; malformed directives are flagged too.

> Coverage is only as good as the frames seen — "0 findings" always prints the frame count.
> The whole `--uiaudit` path is `-DDE_TRACE`-gated, so it adds nothing to editor/web/libtcc builds.
