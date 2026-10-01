# What we can take from CHOMPI's open-source release

> **STATUS: SHIPPED (2026-10-01)** ‑ surveyed the full release (three firmwares + card profiles); shipped:
> §2 `tape_warble()` · §3 `grains_repeat()` (the REPEAT half of [contemporary-rebirth](contemporary-rebirth.md)
> Rung C) · §4 `runtime/patgen.h` (TEMPO's pattern generator) · §5 the cart-land ideas, all in the `latchbox`
> cart. Evidence in §8. Still open: §6 (wavetable data, a second reverb to A/B against) and §7, a measured
> finding about our own `tape()` saturation that needs the maker's call.

CHOMPI was a chromatic sampler / tape-music instrument by CHOMPI Club (now part of Chase Bliss). At
its discontinuation in September 2026 the whole thing was released at
`github.com/CHOMPI-Club/CHOMPI`: three firmwares (TAPE 2.0 sampler+looper, TEMPO 1.0 pattern
generator, WAVE 1.0 eight-voice wavetable synth), the bootloader, the PCB and enclosure files, and the
factory SD-card contents. Everything runs on an Electrosmith Daisy Seed, in C++.

## 0 · Survey: most of it we already have

The DSP is a conventional Daisy build. Mapped against our shelf:

| CHOMPI | ours | verdict |
|---|---|---|
| stereo delay, ping-pong cross-feed | `echo()` / `echo_insert()` (+ BBD) | have it |
| Mutable/Clouds-style reverb | `reverb()` + spring + plate | have it (§6: keep as an A/B reference) |
| one-knob DJ filter, LP below 12 o'clock, HP above | `FX_FILTER` | have it |
| wow/flutter + soft-clip saturation | `tape()` | have it, plus §2 and §7 |
| slices, sampler, looper w/ overdub + varispeed | `instrument_sample_region`, `liveloop`, `loopstation` | have it |
| wavetable voice (2048-sample frames, linear interp, no anti-aliasing) | `wave_set` + unison + sync | ours is better; only the DATA is worth having (§6) |
| granular delay, clock-locked freeze | `grains()` + `grains_freeze()`, free-running | **gap → §3** |
| random-event warble | `tape()` (sine LFOs), `shallow()` (continuous random walk) | **gap → §2** |
| TEMPO's pattern generator | hand-rolled per cart | **gap → §4** |

## 1 · License, and what not to touch

- **Everything is MIT** (repo `LICENSE`). Vendored parts are MIT as well: DaisySP and libDaisy
  (Electrosmith), coreJSON (Amazon), and the FX code ported from Émilie Gillet's Mutable Instruments
  work (`reverb.h`, `fx_engine.h`, `limiter.h`).
- **Keep the notices.** When code is ported, credit it in the comment the way `BOW_BODY_HZ` credits STK.
  `reverb.h` carries the full Mutable notice and must keep it.
- **The name and artwork are NOT licensed** (`TRADEMARKS.md`). No cart, app or rack may be called
  CHOMPI or look like it. This doc's name is a description of a source, which is fine; a cart's is not.

## 2 · Warble: wobble that happens at random moments → `tape_warble(amount)`

Source: `tape/Warble.h` (~60 lines). Every sample it draws a random number; with probability
`rate/samplerate` it starts an **event**: pick a new delay target (100..980 samples at 48 kHz), and a
random glide speed. The delay tap then slides there with a one-pole (`fonepole`). One knob sets how
often events happen *and* how much of the effect is mixed in.

Why it isn't a duplicate:
- `tape()`'s wow and flutter are **sine** LFOs. Periodic, so they sound like a machine.
- `shallow()` is a **continuous** filtered random walk, always moving, like light on water.
- Warble is **sparse**: it sits still, then sags, then sits still. That's what a worn cassette or a
  warped record does, and neither of the other two can produce it.

How it lands: an extra term on the tape read head (same buffer, same `moddel_hermite`), so it rides
`tape()`'s transport and HF rolloff. Changes from the original:
- A **per-instance LCG**, not their function-local `static` seed (which is shared by every instance and
  would break determinism + the AUv3 multi-instance rule).
- **Wet-only.** Their `mix` crossfades dry against the delayed copy, which at partial mix is a comb
  filter (a chorus). Inside `tape()` the tap *is* the signal, so the amount scales depth and rate instead.
- **Bypass is exact.** At `amount = 0` the tap glides home and the term is skipped once it lands, so
  carts that never call it render byte-identically.

## 3 · Beat-locked freeze → `grains_repeat(beats)`

Source: `tempo/granularDelay.h` (the shipped delay; `granularDelay2.h` is an uncompiled alternative).
When frozen, the loop length snaps to a musical division of the clock (1/8, 1/6, 1/4, 1/3, 3/8, 1/2,
3/4, 1, 2 bars). Changing the division **crossfades to a second reader** instead of sliding the read
head, so the pitch never swoops.

Ours: `grains_freeze()` loops the captured cloud, but nothing is tied to the tempo. This is the
**REPEAT** mode of [contemporary-rebirth.md](contemporary-rebirth.md) Rung C ("a beat-synced buffer
re-reader"). Building it on the grain tank instead of a new insert is cheaper and gives it everything the
tank already has: a 3 s capture ring, a freeze toggle, `mix`, and master + per-instrument routing.

Semantics:
- `grains_repeat(beats)`: `beats > 0` turns the tank into a **beat repeater**. While not frozen it
  passes the signal through untouched (no cloud). On `grains_freeze(1)` it loops the last `beats` beats
  (at the engine's `bpm()`), and on `grains_freeze(0)` it fades back to live. `beats = 0` = the normal
  granular cloud, exactly as before.
- **Grid-locked by construction, whenever you press.** The output at time *t* is the input from
  *t − k·L* (L = loop length), so a kick that landed on the beat repeats on the beat even if freeze was
  pressed late. The seam falls at the freeze moment, not on a grid line; it's crossfaded.
- **Seam crossfade from the real audio before the loop**, not from stale buffer: the last X samples of
  each pass blend into the audio that led into the loop start, which is continuous by construction.
- **Changing `beats` while frozen** crossfades the two loop lengths over ~23 ms (both readers are pure
  functions of the samples-since-freeze counter), which is CHOMPI's trick.
- Not yet: HALFTIME / REVERSE / SCRATCH (the rest of Rung C), pitch on the repeat.

## 4 · TEMPO's pattern generator → a cart-land header

Source: `tempo/ArpeggiatorSequencer.h` + `clockManager.h`. Two engines (chromatic + slice), each with
its own copy of this:

- **A note list** = the latched keys, kept in *press order*.
- **Five orders:** SEQUENCE (press order) · UP · DOWN · PING-PONG · RANDOM.
- **Five rest masks**, 20 steps each: none · `XXX.` · `XX..` · `X.` · `X.XX.X.XX.` (period 10).
- **The honest core:** a rest consumes a *clock step* but does **not** advance the note index. So the
  gap *drifts* through the line whenever the number of held notes doesn't divide by the mask's plays per
  period. Hold 4 notes against `XXX.` (3 plays per 4 steps) and the gap lands after a different note every
  pass; the phrase takes 4·4/gcd(4, 3) = 16 steps to come round. Hold 3 and it's a plain 4-step phrase
  with the gap at the end. Adding or lifting one key reshapes the whole line, and nobody programmed it.
  (A first draft of this doc said 3 notes vs `XXX.` = 12 steps. That is what you'd get if rests *skipped*
  notes; TEMPO's don't, and the self-check now pins both cases.)
- **Octave probability:** each note has a chance (`randomness` knob) to jump ±12.
- **Clock divisions:** synced 1/2 · 1/4 · 1/8 · 1/16 · 1/32; free-running ×2 · ×1.5 · ×1 · ×0.66 · ×0.5
  (so dotted and triplet feels). A division change is **deferred to the next edge**, so it never stutters.
- **Latch / sustain:** hold LATCH for 1 s and the held keys become a sustained drone instead.
- **The slice engine's last key** alternates between slices 15 and 16 on every hit (an A/B fill).

Shipped as `runtime/patgen.h`, pure logic like `mono.h`: no sound, no UI, its own `patgen_selfcheck()`
(24 known answers, run by `latchbox`'s `spec()`). The cart owns the clock and the voices (cart owns the
PATTERN, the header owns the RULES). Fixes on the way in: the original's ping-pong direction is a function-local `static` shared by both engines (and a
separate one in `peekNextNote`, so the LED preview drifts from what plays); RANDOM uses `rand()`. Ours
keeps direction in the struct and takes a seed. **And a bug of our own the self-check caught:** the first
version took an LCG's LOW bits (`rand & 1` for the octave direction, `rand % n` for RANDOM order). Bit 0 of
an LCG alternates, and two draws per note meant every octave jump went the same way; `% 4` cycles with
period 4, so "random" was a fixed loop. Fixed by taking high bits; mutation-tested (put the low-bit version
back → two assertions go red).

## 5 · Small cart-land ideas (no engine change)

All in `latchbox` (TAPE → DELAY → STUTTER chain; boots with four keys latched so a stranger hears the drift at once):
- **One-knob delay** (`DSPEngine.h` `ApplyFx`): feedback alone also sets the mix. Wet rises fast to 50%
  (`fb > .25 ? .5 : 2·fb`), dry falls slowly to 50% (`fb > .83 ? .5 : 1 − .6·fb`).
- **Pitch knob that snaps to fifths and octaves**, with a piecewise speed range: ×0.01–0.5 · ×0.5–1 ·
  ×1–2 across the knob.
- **Loudness-compensated drive:** `gain = 1 − SoftClip(0.4·(sat − 1))·0.7` after the clipper. See §7.

## 6 · Parked

- **The wavetables** (`card-profiles/wave-1.0/wavetable01..07.wav`: 33 frames × 2048 samples each, MIT).
  Ready-made material for a multi-frame wavetable engine; we have no such engine today. The voice code
  itself isn't worth porting. **Measured** in [open-source-audio-toys.md](open-source-audio-toys.md) §4.1: 1.89 MB on
  disk, but no frame passes the 64th harmonic, so 256 samples/frame (~118 KB for all seven) holds them exactly.
- **The factory samples** (168 TAPE + 29 TEMPO). Covered by the repo-wide MIT on paper. Confirm with
  the authors that the *recordings* are meant to be included before any of them ships in a cart, and it
  still runs into the "no baked-in audio" rule in contemporary-rebirth §5.
- **A second reverb to A/B against.** Their `reverb.h` (Mutable) is an independent implementation, which
  is exactly what [`tools/ref-render/`](../../tools/ref-render/) exists for.

## 7 · Finding: our `tape()` saturation makes the mix louder

`tape_process()` normalises its clipper as `tanh(g·x) / tanh(g)` (g = 1 + 2·sat). That keeps a
FULL-SCALE peak at 1.0, but multiplies small signals by `g / tanh(g)`. **Measured** (2026-10-01, an A440
sine at −28 dBFS RMS through `tape(0, 0, sat)`, nothing else in the chain):

| sat | RMS out | change |
|---|---|---|
| 0 | −27.87 dBFS | (reference) |
| 0.4 (the default) | −22.32 dBFS | **+5.55 dB** |
| 1.0 | −18.36 dBFS | **+9.51 dB** |

That matches the formula (+5.6 / +9.6 dB). So turning saturation up mostly turns the mix *up*, and an A/B
between two saturation settings is really an A/B between two loudnesses. CHOMPI's line in §5 is the fix
shape: compensate after the clipper.

**Not changed.** 22 carts call `tape()` (35 calls) and 2 more call `instrument_tape()`, all tuned
against today's gain, so fixing it silently would re-voice all of them. Options for the maker: (a) leave
it and document it in [effects-recipes.md](../guides/effects-recipes.md) (done: a ⚠ note + a level-matched recipe), (b) a `tape_trim()` / opt-in compensation flag, (c) fix it and
re-tune the 24 carts with `level-check.js`.

## 8 · What shipped, and how it was checked (2026-10-01)

- **Bypass is exact.** 8 live carts that use `tape()` / `grains()` (wowflutter, tapeloop, grains, lofi,
  pedalboard, groovebox, vapor, fxcheck) render **byte-identical** WAVs on HEAD's engine and on this one.
  (cloudhold and grainchop were in the set too, but render silence without input, so they prove nothing
  and are not counted.)
- **Warble** (`warbleprobe` cart: a sine through a transparent `tape()`, read back by its pitch track):
  off = 0.00¢ (dead flat); `tape_warble(0.5)` = ±17¢ in 33 separate sags over 8 s, still in between;
  off again = glides home. Deterministic (two renders, same hash). **One bug found and fixed by the
  oracle:** with wow and flutter both 0, switching warble on jumped the read head to the 320-sample
  playback head in one sample, a splice at 22× the local step (`click-check.js`). Warble-alone now rides
  its own head with no base lag and blends the first 2 samples of lag in from the dry signal, so it grows
  from 0 and lands back on 0 continuously. click-check: nothing at/above 4×.
- **Beat repeat** (`repeatprobe` cart: an 8th-note line at 120 bpm, frozen **117 ms late**, then the line
  stops so everything after is the loop): every frozen onset keeps exactly the live line's grid phase;
  it repeats the last beat (64, 60, 64, 60…), and after the change to ½ beat a single 8th (60, 60…);
  unfreezing brings the line back. click-check finds nothing at the freeze, the length change, the thaw or
  any loop seam: the only flags are the instrument's own note onsets, which are identical in size in the
  live section before the freeze (that section is the control).
- Gates: soundcheck 900 frames silent · `ctx-gen --verify` · `lint-engine-seam` · `api-usage`
  (studio.h ↔ studioDocs ↔ shell.js agree) · the libtcc symbol table regenerated.
- **Limits:** the repeat is mono (the grain tank is a mono core) and has no pitch. Changing `beats` back
  to 0 while frozen switches straight to the cloud. A loop is capped at ~2.9 s (the ring is 3 s). Warble
  on an instrument bus needs `instrument_tape()` first, like every other tape voice.
