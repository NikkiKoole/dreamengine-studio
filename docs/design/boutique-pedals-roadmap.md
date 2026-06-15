# Boutique-pedal roadmap — squeezing the most out of the "$400 pedal" lists

Two crowd-sourced lists of "highest bang-for-buck boutique pedals to clone in C" landed
(Shallow Water, Microcosm, MOOD, Generation Loss, Polymoon, Klon, Sun Face, Univibe,
Shimmer, Red Panda Particle). This is the plan for harvesting them.

**The key insight:** almost every pedal on both lists collapses onto **two missing engine
primitives**. Build those two brain-pieces and the pedals fall out cheaply on top. Everything
else, we already ship. So this roadmap is ordered by *primitive*, not by *pedal* — each primitive
is a multiplier that unlocks a cluster.

See also: [`effects-recipes.md`](../guides/effects-recipes.md) (the cookbook each new effect
must land a row in), [`audio-notes.md §17`](audio-notes.md) (the effect ledger), and
[`navkit-porting-handoff.md`](../guides/navkit-porting-handoff.md) (the DJ-FX family that
covers MOOD).

---

## Already on the shelf — DO NOT rebuild

| pedal (list) | what we already have |
|---|---|
| Klon transparent OD (B1) | the *tone* via `instrument_drive` + `DRIVE_SOFT` + a parallel dry blend (mix) — same topology. **But the bus-INSERT version (a Klon stompbox you place in the chain) is a real, now-buildable want** — see the update log below. *(Their `fast_tanh` Padé approx is a CPU micro-opt, not a new effect — see "Side quests".)* |
| Sun Face germanium fuzz (B2) | `DRIVE_ASYM` is asymmetric clipping; it's already the `pedalboard` FUZZ pedal's germanium mode. |
| Microcosm / Red Panda Particle granular engine (A2, B5) | `grains` — the capture-buffer + Hanning-windowed grain engine, shipped. The lists' own thesis ("a circular buffer + a grain struct = the brain of every $400 digital pedal") is the thing we just built. |
| Polymoon diffuse delay (A5) | `echo → reverb` approximates it; our reverb is Schroeder (comb + allpass diffusers) already. |

---

## Primitive 1 — a richer modulation source  ⭐ the cheap multiplier

**The gap:** we have *no* clean random / sample-and-hold / non-sine LFO for effects. Tape
wow/flutter are plain sines (noise-LFO deliberately skipped for `--det`), and the only
random-walk `drift` we own is baked *inside* instrument voices (reed/brass breath), not exposed
as a modulator. One small primitive unlocks four pedals.

### Step 1.1 — build the modulation kit (the foundation)
A tiny set of reusable, **deterministically-seeded** modulation sources in `sound.h` (so
`--det` / `tune-check` stay reproducible):
- **random-walk** — slow filtered noise (a low-passed LCG), the "living"/drift source.
- **sample-and-hold** — a value that jumps at a rate and holds (the stepped random).
- **optical / incandescent ramp** — an asymmetric LFO shape: slow attack, fast release (the
  lightbulb throb). Add as an LFO *shape* alongside `TREM_SINE/SQUARE/TRI`.

No user-facing pedal yet — this is the shared building block 1.2–1.5 each consume. Keep it a
`static` helper block, same spirit as `moddel_hermite` ("write the technique once").

### Step 1.2 — Univibe (A-list #3 / B-list #3)  · effort: XS
The cheapest possible win: feed the **optical LFO** into the *existing phaser allpass chain*.
Almost no new DSP — it's a phaser with a different-shaped LFO + the classic 4-stage voicing.
Ship as a `univibe()` wrapper or a "shape" arg on `phaser()`.

### Step 1.3 — Shallow Water (A-list #1)  · effort: S
Random-walk modulating a *short* delay line (reuse the chorus/`moddel_hermite` buffer) → the
warped-tape/water warble. Add the signature **Low Pass Gate**: a lowpass whose cutoff *tracks
the envelope* (darker as the signal decays) — we have the envelope-follower machinery in
`wah`/`note_follow`, so this is a recombination, not new math.

### Step 1.4 — Generation Loss "Failure" knob (A-list #4)  · effort: XS
We **already ship the whole VHS rig** — crush + tape + filter, even the `pedalboard` LO-FI macro
pedal. The *only* missing spice is the **random tape-catch dropouts** (momentary vol + pitch
drops), which is just the S&H source gating volume/pitch. Completes a $400 pedal with one small
modulator. Fold into the LO-FI macro or ship a dedicated `vhs()` recipe.

### Step 1.5 — germanium "living" bias drift (B-list #2)  · effort: XS
A slow random-walk on the fuzz drive bias so it breathes over time. Tiny nicety on the existing
FUZZ pedal — do it only if 1.1 is already in.

---

## Primitive 2 — a bus pitch-shifter  ⭐ the trophy  ✅ SHIPPED 2026-06-15

**✅ Done:** `shimmer(size, damp, shimmer_amt, mix)` — an octave-up granular pitch-shifter
(`octaveup_process`, 2-grain overlap-add) recirculated in a private reverb's feedback loop → the
ascending crystalline tail. Stability was the hard part (tanh-governed feedback + DC blocker +
gain-normalized shifter; see `audio-notes §17` #27). Showcase: the `shimmer` cart. The pitch-shifter
is currently internal to shimmer — exposing it as a standalone bus effect / per-instrument is a future
extension if a cart wants it.

**The gap (original):** we can retune a *voice* (`instrument_tune`), but can't pitch-shift an arbitrary
*bus signal*. That's the one genuinely new engine on these lists, and it's the prize.

### Step 2.1 — pitched + reversed grains  · effort: XS  *(independent — can ship anytime)*
Doesn't actually need the bus pitch-shifter — grain pitch is just resampling the read. Our
`grains` hard-wires `posInc = 1.0`; make it variable (and allow negative = reverse). That alone
gets most of Microcosm / Red Panda Particle's glitch-synth palette. **Lowest-effort headroom on
an engine we already own — a strong early win.**

### Step 2.2 — the pitch-shifter itself  · effort: M
A granular / overlap-add (PSOLA-ish) pitch-shifter operating on a bus signal. The real build.
Reference: DaisySP / Signalsmith Stretch for the overlap-add window logic. A/B nothing in navkit
(navkit has no bus pitch-shift) — so verify by ear + spectral check, not correlation.

### Step 2.3 — Shimmer reverb (B-list #4)  · effort: S *(once 2.2 lands)*
Octave-up pitch-shift **inside the reverb feedback loop** + a lowpass = the crystalline,
continuously-ascending tail. The showpiece. Add the "jitter the delay times" trick (uses
Primitive 1's random source) to de-metallic the FDN. This is the one that makes people go *whoa*.

---

## Primitive-free port — MOOD varispeed (A-list #3)  · effort: S

Independent of both primitives. MOOD's "clock" = varispeed buffer playback (pitch + time shift
together). This maps **directly onto navkit's already-written `half_speed.h` / `tape_stop.h` /
`rewind.h`** — the DJ-FX family flagged as untouched in the porting handoff. Ready DSP to port
verbatim, same playbook as `grains`. The "always listening" half is just `grains`' capture buffer
(never-stop-recording) which we already have.

---

## Recommended order (by effort→payoff, respecting dependencies)

1. **2.1 pitched/reversed grains** — XS, independent, immediate Microcosm/Particle expansion.
2. **1.1 modulation kit** — the multiplier; nothing user-facing but everything below rides it.
3. **1.2 Univibe** — XS, reuses the phaser whole.
4. **1.4 Generation Loss dropout** — XS, completes the VHS rig we already have.
5. **1.3 Shallow Water** — S, a new characterful effect + the LPG.
6. **2.2 pitch-shifter → 2.3 Shimmer** — M then S, the trophy build.
7. **MOOD varispeed** — S, a verbatim navkit port whenever it fits.
8. *(1.5 germanium drift — XS, opportunistic once 1.1 exists.)*

## Per-step repo checklist (every effect, no exceptions)
- **4-place API wiring**: `studio.h` decl + `sound.h` impl + `studioDocs.js` + `shell.js`, then
  `gen-tcc-symbols.js` (see CLAUDE.md "Adding a new API function").
- **SET-AND-HOLD**: configure only on value change (copy `groovebox`'s `apply_fx()`); never 60×/s.
- **Determinism**: seed every random source so `--det` renders byte-reproducibly; run
  `tune-check` for anything touching pitch (2.x).
- **Verify**: `soundcheck` compile-gate + 900-frame tripwire; A/B vs navkit where a reference
  exists (`navkit-fx-render.c`), else spectral/character check.
- **Showcase cart** + baked screenshot + `index.json` + `lint-carts`, and **add the pedal to
  `pedalboard.c`** (it's the serial-insert home) when it's an `FX_*` insert.
- **Docs**: a row in [`effects-recipes.md`](../guides/effects-recipes.md) + a ledger entry in
  [`audio-notes.md §17`](audio-notes.md); if ported, update
  [`navkit-porting-handoff.md`](../guides/navkit-porting-handoff.md).

## Update log — items unlocked since first draft

### `FX_INST` — instance an existing insert, don't mint a new `FX_*` kind  ⭐ new infrastructure
Master drive took `FX_DRIVE = 15`, closing the `fx_order` 4-bit packing (0..15). The follow-on
`drive_insert_inst(instance, amount, mode, mix)` + `FX_INST(FX_DRIVE, instance)` in `fx_order`
(Increment F) is the escape valve: a 2nd/3rd **instance** of an *existing* insert at different chain
spots, no new kind. It generalises — a second crush, filter, EQ, etc. Both moves coexist: instance an
existing effect, OR mint a new kind (there's now room — see below).

### `fx_order` packing widened — 16 → 32 kinds  ⭐ done 2026-06-15
`FX_DRIVE = 15` had filled the old 4-bit-per-slot packing, and the FX_INST work spent the spare
request ints (kind-lo/hi + instance-lo/hi). Repacked each chain slot as **one byte: 5 bits kind +
3 bits instance** (4 slots per int, across the same 4 payload ints) → **kinds 0..31 (16 new), 8
instances/kind, 16 chain slots**. Pure repack of `fx_order()` + the `SR_FX_ORDER` handler + the
`FX_INST` macro (`inst << 5` now); no `SoundReq` growth. Verified transparent: `pedalboard`,
`groovebox`, `epiano` render byte-identical, and an `FX_INST(FX_DRIVE,1)` A/B confirms instances
still route. **This unblocks Shallow Water, the noise gate, and any future insert as a real
reorderable pedal.** `FX_ORDER_SLOTS` (16) caps one chain; bump it (needs `SoundReq` growth) only if
a chain ever needs >16 pedals.

### Overdrive / Klon as a reorderable BUS insert  ⭐ now buildable (was "do not rebuild")
Per-voice drive covers the Klon *tone* but can't be a reorderable **stompbox** whose position vs the
amp is audible. `drive_insert_inst` makes it real: an OD pedal = `FX_DRIVE` instance 1, placed
before/after the amp's own drive — **"OD → amp" vs "amp → OD" now differ.** Add as a *distinct*
OVERDRIVE pedal in `pedalboard.c` (keep FUZZ per-voice — fuzz-into-amp and OD-into-amp are different
rigs players both want). Effort: **S**, once Increment F lands. Validates the "instance only when
demanded" discipline — a cart wanted it, so it got built.

### Amp realism — gain-tracked hiss + 60-cycle hum + a noise gate  · effort: S
The "an electric guitar is never truly silent" character. It's **three things**: broadband **amp
hiss** (scales with gain), **mains hum** (50/60 Hz + harmonics — the single-coil buzz humbuckers
cancel), and interference (ignorable). **NOT dither** — dither is quantization-specific; model hiss +
hum *directly* (signatures in [`building-blocks-spec.md`](building-blocks-spec.md) Block C). Bake into
the **AMP cabinet** as an opt-in, **gain-tracked NOISE knob** (default 0 = byte-identical silent,
seeded LCG for `--det`). Pair with a **NOISE GATE** — the real-world fix that clamps the floor between
notes (the recognisable "snap to silence when you stop playing"). The hiss rides **Primitive 1**'s
filtered-noise source; the gate is a small new follower+threshold effect, broadly useful (gated
reverb, tight drums). Natural sibling to the cab/amp work.

## Follow-ups (small, deferred)
- ✅ **Wire `amp_noise()` into the `pedalboard` cabinet.** Done 2026-06-15 — the AMP cabinet now adds a
  rig-noise floor whose hiss tracks the GAIN knob (clean = silent, hot = hisses) + a touch of mains hum;
  Leslie/none stay pristine. Cart-side only; default board byte-identical (dormant until AMP is chosen).
- ✅ **Shimmer on the bus — `instrument_shimmer` DONE 2026-06-15.** The 2-tank pool shipped: shimmer one
  instrument's bus (pad blooms, drums dry), master = tank 0 (bit-exact md5-verified), tanks 1+ per-aux-bus,
  ~92 KB. Took the master-stage-not-insert path below (did NOT mint `FX_SHIMMER`).
  ✅ **Also a SHIMMER pedalboard pedal (DONE 2026-06-15)** — a macro pedal (kind -3) driving master
  `shimmer()`, accepted as a non-reorderable output-stage ambience (a reverb at the end of the rig). Still
  open: a *reorderable* `FX_SHIMMER` insert (if chain position ever needs to matter), and spatializing the
  bus = spatial v2 (now shipped — `instrument_pos`). Original analysis ↓
- **Shimmer in the pedalboard / on the bus.** `shimmer()` is master-stage (a single private reverb
  tank), so it's not a draggable chain pedal — only the standalone `shimmer` cart has it. Cost is mostly
  **memory**: one instance = a `ReverbTank` (~30 KB) + the octave-up buffer (`SHIM_PBUF` 4096 = ~16 KB) ≈
  **~47 KB**. CPU is pay-per-use (a reverb + a 2-tap granular read per sample, gated on `_used` — idle
  buses cost nothing). Sized options:

  | approach | instances | memory | reorderable / per-instrument |
  |---|---|---|---|
  | cabinet "ambience" slot (drives the existing master `shimmer()`) | 1 (reuse) | ~0 extra | no — fixed master output, like the amp_noise cabinet wiring |
  | **small pool (recommended)** — the grains/reverb-tank pattern | 2–3 | **~94–141 KB** | yes — master + 1–2 instruments at once |
  | full per-bus | 8 (`SOUND_FX_BUSES`) | ~376 KB | yes everywhere, but overkill |

  **Recommended: the 2-instance pool (~94 KB)** as `instrument_shimmer(slot, size, damp, shimmer_amt,
  mix)` — claim-on-first-call aux bus (the `instr_bank.fx_bus` machinery, copy `instrument_grains`).
  Master `shimmer()` keeps tank 0; instruments claim tank 1+; pool exhaustion → `[sound] WARNING` (no
  silent drop). `SR_INSTR_SHIMMER` (re-read the enum tail — it drifts). Payload `a=slot, b=size, c=damp,
  e0=shimmer, e1=mix` (×1000) — slot+4 **fits the 6-int budget cleanly, no bit-packing** (simpler than
  `SR_INSTR_GRAINS`, which had to pack feedback+mix).

  ⚠ **The one non-obvious trap — shimmer is MASTER-STAGE, not an `FX_*` insert.** Unlike `grains`
  (`FX_GRAINS`, runs inside `apply_insert`), master `shimmer()` runs as a post-chain master-stage call
  (before the soft-clip, like `dropout`/`amp_noise`). So do **NOT** mint `FX_SHIMMER` + route it through
  `apply_insert` — that *moves* the master shimmer to a chain position and **breaks the master-bit-exact
  guarantee.** Instead: **tank 0 stays the existing master-stage call, untouched** (→ trivially bit-exact),
  and **tanks 1+ run per-aux-bus** (applied on `busL[b]/busR[b]` after that bus's `apply_insert` chain,
  before it sums to master). One pool, two call sites. Make **all** per-tank state arrays — not just the
  tank+`OctaveUp` but the **DC blocker (`shim_dcx`/`shim_dcy`), `shim_prev`, `shim_fb`, `shim_mix`** (the
  DC state is the easy one to leave shared → cross-talk). Gate each on `shim_used[tank]`. **Verify master
  bit-exact** with a `--det` md5 of an existing shimmer cart before/after the singleton→pool refactor
  (the reverb-bus tank-pool commit did exactly this).

  If you just want it *playable in the rig* cheaply (no reorder/per-instrument), the cabinet ambience slot
  is ~free. Deferred — bigger than a quick win.

  **Composes with spatial — but know the boundary** (it surfaced this gap). `instrument_shimmer` is
  **isolation, not spatialization**: shimmer the pad, leave the drums dry. It layers correctly with
  spatial **v1** (per-voice `listener`/Doppler is applied at the *voice*, before the FX bus): the dry
  source still pans + Dopplers, and at `mix < 1` the dry passthrough keeps its position while the shimmer
  wash sits as an *ambient, centered* tail under the moving source — which is musically right (reverb is
  room ambience, not a point source). What it does **not** do is make the wet *tail* move/Doppler with the
  source as one object — that's spatial **v2 emitter buses** ("a full radio mix through bus+FX, moving as
  one object — wet tail and all", [`spatial.md`](spatial.md)). `instrument_shimmer` is orthogonal to v2
  and pairs with it: the shimmer aux bus is exactly the kind of thing that *becomes* a positionable emitter
  in v2. So: pads-shimmered/drums-dry + positioned sources = today; the shimmer tail flying past = v2.

## Side quests (engineering nits from the lists, not effects)
- **`fast_tanh` Padé approximation** (B1 tip): if `DRIVE_SOFT`/the soft-clip is ever hot in the
  profiler, swap `tanhf` for the rational approx. Measure first — only worth it if it shows up.
- **AC128 transfer-curve LUT** (B2 tip): a lookup table + lerp for a specific germanium curve, if
  we ever want a *named* vintage fuzz voicing beyond `DRIVE_ASYM`.
