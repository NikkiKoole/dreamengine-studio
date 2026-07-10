# Building-blocks spec — drop-in plan for the small pedal primitives

Companion to [`boutique-pedals-roadmap.md`](boutique-pedals-roadmap.md). The roadmap is the WHAT
and WHY; this is the HOW — concrete signatures, DSP sketches, request layouts, and the four-place
wiring — so the moment `sound.h` is clear, implementation is mechanical, not exploratory.

STATUS: SHIPPED (2026-06-30) — Blocks A, B, and C all landed in `sound.h`/`studio.h`. This doc
served its purpose as the build-time spec; kept as the record of *how* they were built and for the
small tail of follow-on pedals still open (see "What shipped vs. what's left" below).

### What shipped vs. what's left

| spec item | state | API |
|---|---|---|
| **Block A** — pitched + reversed grains | ✅ shipped | `grains_pitch` / `instrument_grains_pitch` |
| **Block B** — modulation kit | ✅ shipped | `mod_randwalk` / `mod_sh` / `mod_optical` (wired into `LFO_SHAPE_OPTICAL/SH/RANDOM`) |
| Univibe (rides B) | ✅ shipped | `univibe` / `instrument_univibe` |
| Shallow Water (rides B) | ✅ shipped | `shallow` / `instrument_shallow` |
| Generation-Loss "FAILURE" (rides B) | ✅ shipped | `dropout` (showcase cart `genloss`) |
| **Block C** — amp realism | ✅ shipped | `amp_noise` + `gate` |
| Shimmer / bus pitch-shifter (Primitive 2) | — out of scope | its own spec; shimmer already landed per the roadmap |

**The two open tail items live in the roadmap, not here** (single home): the **Klon/OD bus-insert
cart slot** and **Germanium "living" drift** — see [`boutique-pedals-roadmap.md`](boutique-pedals-roadmap.md)
(update log + Step 1.5). The DSP sketches and wiring notes below are preserved as the as-built
record and as the recipe for that Germanium-drift pedal.

Scope = the two **small** building blocks (the bus pitch-shifter / Shimmer is the *big* Primitive 2,
specced separately when these land):
- **Block A — pitched + reversed grains** (XS, independent, do first)
- **Block B — the modulation kit** (the multiplier: random-walk / sample-&-hold / optical LFO)

> ⚠ Numbers that drift: the `SR_*` enum tail and the `FX_*` count move as other agents add effects
> (master drive took `FX_DRIVE = 15`, filling the `fx_order` 4-bit packing). **Pick the next free
> `SR_*` value at implement-time**, don't trust a literal here. Neither block below adds an `FX_*`
> insert, so both sidestep the packing ceiling.

---

## Block A — pitched + reversed grains

`grains` already captures + windows + scatters; the one thing it can't do is change a grain's
*playback speed*. Today `_grain_spawn` hard-sets `g->posInc = 1.0f`. Make it a pitch ratio (and
allow negative = reverse). That alone delivers most of Microcosm / Red Panda Particle's glitch
palette on the engine we already shipped.

### DSP change (in the existing grains block)
Add per-tank fields alongside the current grain params:
```c
float pitch;        // semitones, -24..+24 (0 = unchanged)
float pitch_spread; // 0..1 — random per-grain detune (the "cloud" spread)
bool  reverse;      // grains play backwards through the buffer
```
At spawn, set `posInc` from the ratio (with per-grain random detune from the existing seeded LCG):
```c
// in _grain_spawn(), replacing g->posInc = 1.0f:
gt->noiseSeed = gt->noiseSeed * 1103515245u + 12345u;
float det = ((int)((gt->noiseSeed >> 16) & 0xFFFF) / 32767.5f - 1.0f) * gt->pitch_spread; // ±spread
float ratio = powf(2.0f, (gt->pitch + det) / 12.0f);
g->posInc = gt->reverse ? -ratio : ratio;
if (gt->reverse) g->readPos = readStart + grainSamples;   // start at the grain's far end, read back
```
The read loop must wrap **both** directions (it currently only wraps the high end):
```c
g->readPos += g->posInc;
if (g->readPos >= GRAIN_BUF_LEN) g->readPos -= GRAIN_BUF_LEN;
if (g->readPos < 0)              g->readPos += GRAIN_BUF_LEN;   // NEW — reverse/down-pitch wrap
```
`moddel_hermite` already does fractional reads, so non-integer `posInc` (any pitch) just works.

### API (new — `grains()` is already at the 6-int payload max, so this is its own request)
```c
void grains_pitch(float semitones, float spread, int reverse);            // master grain cloud
void instrument_grains_pitch(int slot, float semitones, float spread, int reverse);
```
Request (pick the next free `SR_*`): `SR_GRAINS_PITCH` — `a=semitones*100`, `b=spread*1000`,
`c=reverse`. Instrument variant `SR_INSTR_GRAINS_PITCH` — `a=slot`, `b=semitones*100`,
`c=spread*1000`, `e0=reverse`. Comfortably inside the 6-int budget (no packing tricks needed).

### Set-and-hold
Pitch/spread/reverse are config → push only on change, like the rest of `grains`. A live pitch
*sweep* (e.g. a falling-tape effect) is fine to push per-frame while it's actually moving.

### Verify
- `soundcheck` compile-gate + 900-frame tripwire.
- `tune-check` is N/A (grains isn't a pitched *engine*), but sanity-check: `grains_pitch(12,0,0)`
  on a sine → the grain cloud should read an octave up (spectral peak doubles).
- Showcase: extend the `grains` cart with a PITCH slider + a REVERSE toggle (its sliders already
  work post-`ui_end` fix), re-bake, re-`ui-audit`.
- Recipe row in [`effects-recipes.md`](../guides/effects-recipes.md) (octave-shimmer grains,
  reverse-grain stutter) + the [`audio-notes`](audio-notes.md) `§17` #19 entry gets a "pitched grains" addendum.

---

## Block B — the modulation kit (the multiplier)

A small set of reusable, **deterministically-seeded** modulation sources. Internal `static`
helpers (like `moddel_hermite` — "write the technique once"); the pedals consume them. Each holds
its own `ModState` so multiple effects/buses don't share a phase.

### State + signatures
```c
typedef struct {
    unsigned int seed;   // per-instance LCG seed (set once → --det reproducible)
    float val;           // current output (random-walk: smoothed; S&H: held)
    float target;        // S&H: the value being held until the next step
    float phase;         // 0..1 step/cycle phase
} ModState;

// slow filtered random in ~[-1,1]; `rate` Hz = how fast it wanders. The "living"/drift source.
static float mod_randwalk(ModState *m, float rate, float dt);
// stepped random in ~[-1,1]; jumps to a fresh value every 1/rate sec, holds between (sample-&-hold).
static float mod_sh(ModState *m, float rate, float dt);
// optical/incandescent LFO SHAPE: phase 0..1 → 0..1, asymmetric (slow rise, fast fall — the bulb).
static float mod_optical(float phase);
```
DSP sketches (tune by ear at impl):
```c
static float mod_randwalk(ModState *m, float rate, float dt) {
    m->phase += rate * dt;
    if (m->phase >= 1.0f) {                                  // new random target each "step"
        m->phase -= 1.0f;
        m->seed = m->seed * 1103515245u + 12345u;
        m->target = (int)((m->seed >> 16) & 0xFFFF) / 32767.5f - 1.0f;
    }
    m->val += (m->target - m->val) * (4.0f * rate * dt);     // one-pole smooth toward target → filtered
    return m->val;
}
static float mod_sh(ModState *m, float rate, float dt) {
    m->phase += rate * dt;
    if (m->phase >= 1.0f) {
        m->phase -= 1.0f;
        m->seed = m->seed * 1103515245u + 12345u;
        m->val = (int)((m->seed >> 16) & 0xFFFF) / 32767.5f - 1.0f;   // hard jump, no smoothing
    }
    return m->val;
}
static float mod_optical(float phase) {                      // slow brighten, fast dim
    // rise over the first ~80% of the cycle (eased), then a quick fall — the incandescent throb
    return phase < 0.8f ? powf(phase / 0.8f, 0.6f)
                        : 1.0f - (phase - 0.8f) / 0.2f;
}
```
Seeding: each `ModState` gets a fixed seed at effect init (e.g. derive from the bus index, like
`grain.noiseSeed = 55555`) so `--det` renders byte-reproducibly. **No `Date`/`rand()`** — LCG only.

### What consumes it (each a separate, later pedal — NOT part of this block)
- **Univibe** (XS) — swap the phaser's `sinf(phase*2π)` LFO for `mod_optical(phase)`; ship as a
  `univibe()` wrapper or a "shape" arg on `phaser()`. Reuses the whole allpass chain.
- **Shallow Water** (S) — `mod_randwalk` modulates a short `moddel_hermite` delay read + an
  envelope-tracking Low Pass Gate (reuse the `wah`/`note_follow` follower for the LPG cutoff).
- **Generation Loss "Failure"** (XS) — `mod_sh` gates volume + nudges pitch occasionally → the
  random tape-catches. Folds into the existing LO-FI macro / a `vhs()` recipe (crush+tape+filter
  are already ours).
- **Germanium drift** (XS) — a slow `mod_randwalk` on the fuzz drive bias for a "living" feel.

### Verify
- `soundcheck` compile-gate + 900-frame tripwire after the helpers land.
- `--det` double-render → byte-identical (proves seeding is deterministic).
- Each consuming pedal verifies on its own (spectral/character; A/B vs navkit only where a
  reference exists — none of these have one, so it's ears + meters).

---

## Block C — amp realism: hiss + 60-cycle hum + a noise gate

The "an electric guitar is never truly silent" character (roadmap update log). Three distinct
generators — model them directly, **not** via dither (dither is quantization-specific). All
opt-in, default 0 = byte-identical silent, seeded LCG for `--det`.

### C.1 — the rig-noise generator (rides Primitive 1)
- **hiss** = broadband filtered noise (the same low-passed LCG as `mod_randwalk`, but audio-rate
  white → an HF rolloff to taste so it sits like amp/speaker hiss). **Amount scales with the amp
  GAIN** — that gain-tracking is what sells it as real.
- **hum** = a 50/60 Hz sine + one or two harmonics (the 120 Hz buzz), low level. `mains_hz` picks
  region (single-coil buzz; humbuckers would cancel it — we just model the audible result).
- **Home:** bake into the **AMP cabinet** as a `NOISE` knob (most authentic — the noise *is* the
  amp), or a standalone `void amp_noise(float hiss, float hum, int mains_hz)`. Either way default
  0 = silent.
```c
// sketch: per-output-sample, added to the cab/amp stage, scaled by gain
seed = seed*1103515245u + 12345u;
float white = ((int)((seed>>16)&0xFFFF)/32767.5f - 1.0f);
hiss_lp += (white - hiss_lp) * HISS_CUT;                 // HF-rolled hiss
hum_ph  += mains_hz / (float)SOUND_SAMPLE_RATE; if (hum_ph>=1) hum_ph-=1;
float hum = sinf(hum_ph*6.2831853f)*0.6f + sinf(hum_ph*2*6.2831853f)*0.4f;  // 50/60 + 2nd harmonic
out += (hiss_lp * hiss_amt + hum * hum_amt) * gain_track;
```

### C.2 — noise gate (new small effect: follower + threshold)
The real-world fix that clamps the floor between notes (every high-gain rig has one). Reuses the
envelope-follower machinery (`sound_follow_coef`) from `wah`/`note_follow`.
```c
void gate(float threshold, float attack_ms, float release_ms);   // master/cabinet-stage gate
// env-follow |signal|; gain → 1 above threshold (attack), → 0 below (release). Fast release =
// tight metal chop; slow = breathing. threshold 0 = always open (bypass, byte-identical).
```
**Slot question (important):** a brand-new `FX_*` insert kind is **blocked** — `FX_DRIVE = 15`
filled the 4-bit packing, and `FX_INST` only re-instances kinds 0..15, it doesn't add new ones. So
for v1 make the gate a **fixed master/cabinet-stage effect** (not reorderable, no `FX_*` slot) —
which is fine, a gate almost always sits last/at the amp, not mid-chain. Only widen the packing if a
later effect genuinely needs both a new kind *and* chain-position freedom.

### Verify (Block C)
`soundcheck` + tripwire · `--det` double-render byte-identical with noise on (proves seeding) ·
the gate's threshold-0 path must be byte-identical to no-gate (dormant discipline) · showcase: the
NOISE knob + gate fit naturally in `pedalboard`'s AMP cabinet.

---

## Sequencing (when `sound.h` is clear, one writer at a time)
1. **Block A — pitched/reversed grains.** Self-contained, immediate payoff, no `FX_*` slot.
2. **Block B — the modulation kit** (helpers only, no user-facing API yet).
3. Then the pedals that ride B, cheapest first: **Univibe → Generation Loss dropout → Shallow Water**.
4. **Block C — amp realism** (hiss+hum NOISE knob + noise gate); the hiss reuses B's noise source.
5. **Overdrive / Klon bus-insert pedal** — once Increment F's `drive_insert_inst` / `FX_INST` lands:
   a distinct OVERDRIVE pedal = `FX_DRIVE` instance 1 in `pedalboard.c` (keep FUZZ per-voice).
   No engine work beyond Increment F — it's cart + registry. See roadmap update log.
6. Later, the big one: bus pitch-shifter → **Shimmer reverb** (Primitive 2, its own spec).

## Per-step repo checklist (unchanged from the roadmap, restated for convenience)
4-place API wiring (`studio.h` + `sound.h` + `studioDocs.js` + `shell.js` + `gen-tcc-symbols.js`) ·
SET-AND-HOLD (configure on change only) · deterministic seeding (`--det`) · `soundcheck` +
tripwire (+ `tune-check` for anything pitched) · showcase cart + screenshot + `index.json` +
`lint-carts` · add to `pedalboard.c` if it's an `FX_*` insert · recipe row in `effects-recipes.md`
+ ledger entry in `audio-notes.md §17`.

## Hot-file protocol (because these all touch `sound.h`)
Re-read the exact region right before editing (line numbers drift) · **targeted `Edit`, never a
full-file `Write`** · compile-gate before commit · after commit, `git show HEAD:runtime/sound.h |
grep <sentinel>` to confirm a parallel commit didn't revert it · keep commits small and prompt.
