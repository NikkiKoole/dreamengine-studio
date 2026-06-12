# Porting effects & instruments from navkit — a handoff

**Who this is for:** the next agent picking up the navkit-port program. Three bus effects landed in
one session this way — **tremolo, phaser, Leslie** — each a verbatim port, each A/B-verified to
0.99999 against navkit's *real* DSP, each shipped in a cart with docs. This file is the worn groove:
the sequence that worked, the gotchas that cost real time, the saved tooling, and the queue of what's
left. It complements two docs you must also read:

- [`porting-from-navkit.md`](porting-from-navkit.md) — the **instrument/oscillator** playbook (port
  the oscillator VERBATIM; the clav worked example; velocity×2, the ratio blend, amp-normalize).
- [`debug-harness.md`](debug-harness.md) → "A/B against navkit" / "A/B a bus EFFECT" — the **tooling**.

> **The one rule, restated for effects:** copy navkit's per-sample DSP **line for line**, don't
> paraphrase. Match the formula, the constants, the order of operations, the state update. Then A/B
> at the sample level. Every time I trusted a "close enough" paraphrase I paid for it; every verbatim
> copy A/B'd at 0.99999 first try.

---

## The sequence (what worked, in order)

1. **Pick the effect, read navkit's REAL DSP first.** It lives in `~/Projects/navkit/soundsystem/
   engines/effects.h`. For a bus effect you want the **bus** version: `setBusXxx(...)` (the setter +
   param struct) and the `processBusEffects` block that uses `bus->xxx` / `state->busXxx`. There's
   often a *global* `processXxx` too (uses `fx.xxx`) — they're usually identical DSP, but the harness
   exercises the **bus** path, so read that one. Grep: `grep -n "Xxx\|setBusXxx\|processXxx" effects.h`.

2. **Run it through the 0015 gate BEFORE writing code.** [decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md)
   closed the effects roster to *recipes-masquerading-as-primitives*. A new effect FUNCTION is only
   admitted if it **cannot be built from the rostered primitives** (the LFOs, filters, drive, the
   mod-delay buffer, the SVF). The test that matters, learned across wah → tremolo → Leslie:
   > **"X = a + b + c" does NOT mean "X is a recipe" — only if a, b, c are each reachable *today*.**
   Tremolo *bus* ≠ per-voice `LFO_VOLUME` (coherent phase across the instrument). Leslie ≠ 3 LFOs (an
   LFO can't band-split at a crossover, can't run a delay-line Doppler, can't drive two inertial
   rotors). Both cleared the gate. If yours clears it, **log a 0015 correction-history entry** saying
   why (this is required — the log lapsed once and four effects shipped undocumented; don't repeat that).

3. **Port verbatim into `runtime/sound.h`.** Pattern (copy an existing one — `trem`/`phaser`/`leslie`
   are the freshest references):
   - `static <state> xxx_*[SOUND_FX_BUSES];` per-bus arrays + `static bool xxx_used[SOUND_FX_BUSES];`
   - `static void xxx_process(int b, float *mixL, float *mixR)` — navkit's DSP, **per channel** for
     stereo (own buffers L/R, **shared** LFO/rotor phase — so a centered/mono source matches navkit's
     mono exactly, and stereo keeps its width; see phaser/Leslie).
   - `static void fx_set_xxx(int b, ...)` — clamp params, set state, `xxx_used[b] = (mix > 0.0f)`.
     **mix 0 (or depth 0) = off** is the universal convention — it's what keeps non-users
     **byte-identical**.
   - Reuse helpers: `moddel_hermite(buf, len, readPos)` is navkit's `hermiteInterp` already in the
     tree (use it for any fractional delay tap — chorus/flanger/tape/Leslie all do).

4. **Wire the FOUR API places (all in one change):** `studio.h` decl + a constant block if needed;
   `sound.h` request kind (`SR_XXX` / `SR_INSTR_XXX`) + dispatch + the API functions
   (`xxx()` master, `instrument_xxx(slot,…)` per-bus via `fx_bus_for`); `editor/src/studioDocs.js`
   (sig + doc + example); `editor/src/shell.js` (add the name to the `sound` section's `keys`). Then
   run `node tools/gen-tcc-symbols.js` so the libtcc symbol table lands in the same commit.

5. **Place it in the chain.** Reorderable tone effects join the `FX_*` enum (studio.h) + `apply_insert`
   (sound.h) — but the `fx_order` packing is **3 bits per kind (0–7), currently full**. An output-stage
   effect (a speaker/cabinet/limiter) should be **pinned** instead: an `if (xxx_used[b])
   xxx_process(...)` after the `apply_insert` loop in BOTH the per-bus loop and the master chain — no
   `FX_*` kind, so it never touches the packing (Leslie does this; the soft-clip is the precedent).

6. **A/B against navkit's real DSP** (the saved kit — see below). Sample-level correlation ≥ 0.9999 =
   verbatim. Anything lower, investigate before shipping.

7. **Integrate in ONE cart + write the docs.** Showcase it where it belongs (organ→Leslie,
   epiano→tremolo/phaser, motorik→phaser). Add the recipe to [`effects-recipes.md`](effects-recipes.md)
   (call + values + character + "used by"), a `§17` ledger entry in
   [`audio-notes.md`](../design/audio-notes.md), and the 0015 correction if it was gated.

8. **Gate before committing:** compile (`node tools/play.js soundcheck script /dev/null --headless
   --frames 2`) + the **soundcheck tripwire** (`--frames 900`, silence = dormancy/byte-identical held)
   + `tune-check.js --quiet` if you touched anything pitched. Commit by **explicit pathspec** (parallel
   agents — see CLAUDE.md), then sentinel-check it survived: `git show HEAD:runtime/sound.h | grep
   fx_set_xxx`.

---

## The saved A/B kit (built during these ports — reuse it)

| tool | what it does |
|---|---|
| `tools/navkit-fx-render.c` | renders navkit's **real** bus effect over a sine → mono WAV (links genuine `setBus*`+`processBusEffects`). `clang -O2 -o /tmp/navfx tools/navkit-fx-render.c -lm`, then e.g. `/tmp/navfx leslie 2 0 0.5 0.7 1 out.wav 6 220`. **Add a new effect = one `else if` calling its `setBus*`.** |
| `tools/wav-correlate.js` | **sample-level A/B** — normalized cross-correlation of two WAVs (1.000 = identical DSP up to gain + delay). The definitive check; robust to our engine's output-gain offset. |
| `tools/wav-modrate.js` | LFO **rate + depth** of an amplitude-modulating effect (tremolo / auto-pan), level-independent. |
| `tools/wav-analyze.js` | peak/RMS/crest + two-file compare (the pre-existing meter). |
| A test cart | a flat-top sine through the effect (`tremtest.c` / `phasertest.c` / `leslietest.c`) — source-only, unregistered. Render with `play.js … --det --wav`. |

**The honest A/B, not the fake one:** do **not** copy navkit's formula into a standalone harness and
diff it against your `sound.h` copy — that tests your copy against your copy. `navkit-fx-render.c`
links navkit's *genuine* functions; that's the only real reference.

---

## Gotchas that actually bit (each cost real time)

- **`#define fx` (and friends) in navkit's `effects.h`.** navkit does `#define fx (fxCtx->params)`
  (+ `delayBuffer`, `reverbComb1`, …). Name a local `fx` in your harness and it explodes into
  nonsense parse errors. Avoid those names (`navkit-fx-render.c` uses `effect`).
- **Our `--wav` is STEREO; navkit harness WAVs are mono.** An analyzer reading int16 as mono halves a
  stereo file's detected rate (reads L,R,L,R as 2×). Parse the `fmt ` channel count, take the left
  channel (the saved tools do).
- **A sub-0.9999 correlation may be OUR master soft-clip, not your port.** A driven/hot setting routes
  through our `±0.8` soft-clip limiter, which navkit's bare harness lacks — so the peaks diverge
  (Leslie drive read 0.992 for exactly this). Confirm by testing a quieter setting: if *that* hits
  0.9999, the port is verbatim and the gap is the limiter.
- **Allpass effects barely move a sine** (phaser). An allpass is magnitude-preserving — a pure sine
  hardly modulates (notches live in the dry+wet *sum*; a sine has energy at one frequency). Correct
  physics, weak demo. Use `wav-correlate.js` for the A/B, and show the swirl on broadband material.
- **Variable-name clashes when you add an API name.** Adding `leslie()` broke `organ.c`'s
  `static int leslie` (the "don't name a variable after a builtin" trap). After adding any API name,
  `grep -rl '\bnewname\b' tools/carts/*.c` and rename the colliding cart vars.
- **Stereo from a mono navkit effect:** run navkit's mono DSP **per channel** with a **shared** LFO/
  rotor phase but **per-channel** buffers/filter state. Mono source → both channels identical →
  matches navkit; stereo source → keeps width. (phaser allpass memory, Leslie crossover + Doppler.)
- **Hot shared files + parallel agents.** `sound.h`/`studio.h`/`studioDocs.js`/`shell.js` are edited by
  several agents at once. Re-`Read` the region right before each Edit (line numbers drift), commit by
  pathspec, sentinel-check survival. Expect your `studio.h`/editor edits to sometimes get swept into a
  *neighbour's* commit — fine, just verify HEAD ends up consistent (decl + impl + symbol + caller all
  present). Don't `--amend` a landed commit on this branch.

---

## Architecture facts you'll need

- **8 buses** (`SOUND_FX_BUSES`): bus 0 = master, 1–7 = per-instrument aux, auto-allocated one per slot
  by `fx_bus_for(slot)` on first insert use. All of one slot's inserts share its one bus.
- **Inserts are per-bus + series** (run via the reorderable `insert_order`/`apply_insert` loop, default
  = canonical order). **Sends** (echo, reverb) are parallel shared tanks, accumulated in the voice loop.
- **`fx_order` packs kinds at 3 bits each (0–7), and 8 kinds fill it.** A 9th *reorderable* kind needs a
  packing widen — or pin it (preferred for output-stage effects).
- **Byte-identical guarantee:** an untouched cart must produce identical bytes. Every effect is dormant
  (`*_used[]` false, zero-init) until first configured with mix/depth > 0. The soundcheck tripwire
  verifies it. This is load-bearing — never run a stage unconditionally.
- **navkit ↔ us:** velocity is `clampf(vel·2,0,1)`; **oscillator** numbers transfer exactly, **effect/
  knob amounts do NOT** (dial by ear — see `porting-from-navkit.md` Gotcha 5). SAMPLE_RATE 44100 both.

---

## The queue — what's left worth porting

**From navkit (it has a real implementation to copy):**
- **Compressor** (`processMasterCompressor`) — the genuine dynamics gap (we have only a soft-clip
  limiter). The **sidechain pump** would transform the dance stations (house/italo). Clears the gate
  (dynamics ≠ any recipe). Strong next pick — the cleanest verbatim port in the tremolo/phaser mold.
- **Ring modulator** (`processRingMod`/`setBusRingMod`) — cheap, characterful (metallic clang, bells,
  robot). 0015 says FM detents cover "metallic clang," so it's contested + niche — argue the gate first.
- **Octaver** — 0015 refuses it (we have polyphony — play the octave). Don't, unless you can beat that.
- **Comb / pitched delay**, **bus distortion** — mostly covered (flanger/tape mod-delay; per-voice drive).
- **Instruments:** open engine work is catalogued in [`instrument-engines.md`](../design/instrument-engines.md)
  + `audio-notes §18` (PIPE reads an octave low + flat; PLUCK/REED/BRASS flatten at the top of their
  range). Use the **oscillator-verbatim** playbook in `porting-from-navkit.md`, A/B with
  `preset_audition` (navkit's preset→WAV renderer) + an FFT, not `navkit-fx-render.c`.

**From our own findings this session:**
- **Chorus on a DX/FM electric piano** — the quintessential *chorused EP* is the FM Rhodes (80s
  ballad), which is `INSTR_FM`, not `INSTR_EPIANO`. We have `chorus`; it wants an FM-EP cart (or wire
  `instrument_chorus` onto one). Biggest sound payoff per effort.
- **Stereo auto-pan "suitcase vibrato"** — 0015 refused auto-pan as "the engine is mono," but the
  engine is **stereo now** (busL/busR, `instrument_pan`, `LFO_PAN`). That refusal is **stale** — a true
  suitcase-Rhodes vibrato is stereo pan, not amplitude tremolo. Revisit + (maybe) a bus auto-pan.
- **More stations want effects** (each a per-station *ear* call, don't splatter): `dub` → phaser over
  its echo; `cocktail`/`exotica` → vibraphone-motor tremolo; `citypop`/`yacht` → phased/chorused EP.
  `motorik` already got the phaser; the `epiano`/`organ` are the effect showcases.

---

*Three effects, one session, all verbatim. The method is boring on purpose: read the real DSP, clear
the gate, copy line-for-line, A/B at the sample level, gate, document. Boring is why it converges
first try. — handed off 2026-06-12.*
