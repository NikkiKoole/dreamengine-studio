# Sound work — session handoff (instrument engines)

> **Genre: working handoff.** Transient pickup note for the next session, not a permanent
> design doc — the durable knowledge lives in the docs this points at. Fold anything that
> proves out into §8.8.x and delete the stale bits. Last updated 2026-06-07.

## Where we are

Five modeled engines shipped + fully wired (studio.h id, sound.h, studioDocs.js, shell.js,
soundcheck slot, showcase cart): **PLUCK · MALLET · FM · ORGAN · EPIANO**. The latest two
this session:

- **`INSTR_ORGAN`** (§8.8.4) — tonewheel, shipped + **published** + timbre confirmed by ear
  ("sounds awesome"). Drive-fizz fix landed (pre-drive HF rolloff). ADR-0016 = branch B
  (combo organ → its own engine when a station proves the recipe; morph is a full axis).
- **`INSTR_EPIANO`** (§8.8.5) — Rhodes/Wurli/Clav, shipped + showcase `epiano.c` (now with a
  WAH recipe: toggle off/auto/env + depth slider, clav boots into envelope-wah). **NOT yet
  published, NOT yet ear-confirmed** — see the open items.

Canonical state lives here (don't duplicate it — update these):
[STATUS §5](../STATUS.md) · engine specs [§8.8.3 FM / §8.8.4 organ / §8.8.5 EP](instrument-engines.md) ·
roadmap [§8.5](instrument-engines.md) · [ADR-0016](../decisions/0016-combo-organ-recipe-then-macro-or-engine.md).

## OPEN — needs ears first (start here next session)

1. **EP preset timbre check** — play `epiano.c`'s 6 presets: do rhodes/wurli/clav nail their
   nameplates? Sweep **bark** (morph) under a held note. Try the **wah** (press `6` for the
   clav → envelope-wah, Stevie "Superstition"). Report *how* anything's wrong (dull? no
   attack? buzzy? rings too long?) → that translates to specific `sound.h` numbers.
2. If good → publish (`tools/publish-cart.sh epiano`) like organ.
3. Optional treat: a Rhodes comping under the organ (two engines together).

## OPEN — the Rhodes "doesn't sound Rhodesy" hypothesis (the real one to chase)

User flagged this by ear AND hit the same thing porting in navkit — so it's likely
**structural**, not a typo. Confirm the symptom first, then chase in this order. All in
`runtime/sound.h` → `sound_epiano_start` / `sound_epiano_sample`.

**Prime suspect — the tine/bell modes decay too slowly.** A Rhodes is a fast bell *ding*
(~50–150 ms) over a warm sustaining fundamental. Right now the per-mode decay is
`dfac = 1/(1 + (ratio-1)*0.25)` against a long `BASE_DEC[0]=3.5s` — so the inharmonic upper
modes (ratios 4.2, 9.5, 16.3…) ring ~1–2 s instead of pinging and dying. Result: a sustained
slightly-detuned **drone**, not *ding→mellow*. **Fix lever:** give the bell/tine modes (i≥1,
especially i=1..4) a SHORT independent decay (a fixed ~30–150 ms attack-transient time),
decoupled from the fundamental's long sustain. This is probably 80% of "Rhodesy."

**Supporting suspect — amp profile too fundamental-heavy.** `AC[0]` (Rhodes centered) is
`{1, .04, .03, .06, …}` — the bell modes are nearly silent, and navkit's own comment says
"most upper harmonic energy comes from the pickup intermodulation, not the modes." So the
*nonlinearity* is supposed to manufacture the bell. If it's under-driven at low bark
(`k=(0.25+bark*1.2)*regDist`, and `regDist=(1-freqNorm)²` shrinks it in the mid-range), there's
no tine. Lever: raise the Rhodes baseline `k`, and/or boost the attack-mode amps so there's
something for the pickup to bite on.

**Third suspect — register scaling too aggressive** in the played range: `regDist` /
`keep=(1-freqNorm)` cube on upper modes may be killing the bell by the mid-octaves. Verify
`freqNorm = (freq-80)/1200` lands where expected for C3–C5.

*(Don't pre-emptively rewrite — it's an ear-in-the-loop tuning job. Confirm the symptom, then
pull the tine-decay lever first.)* When fixed, record it as a §8.8.5 post-ship finding (the
FM §8.8.3 pattern) and delete this section.

## Roadmap after EP (decided, §8.5 steps 6–10)

**PD (Casio CZ — the cheap snack) → reed → membrane → bowed/pipe + the buffered piano/guitar
pair → formant + the effects layer.** The effects layer (master reverb + the bus, Leslie,
chorus, the Juno unblock) is sequenced LAST, after the engines; resolve **stereo** (§9) before
it. ~~DC blocker is a second-customer watch item~~ **DONE (2026-06-07): a DC blocker
graduated to the shared per-voice drive path** in `sound.h` (the tanh of an asymmetric
driven wave injected DC → a click on the env ramp; reported on the organ). One-pole HP ~7Hz,
runs only when `drv>0.001` so clean voices stay bit-identical. Fixes every driven engine at
once. (EP keeps its own internal blocker — different signal point, the pickup nonlinearity.)
