# Sound work — session handoff (instrument engines)

> **Genre: working handoff.** Transient pickup note for the next session, not a permanent
> design doc — the durable knowledge lives in the docs this points at. Fold anything that
> proves out into §8.8.x and delete the stale bits. Last updated 2026-06-08.

## Where we are

Five modeled engines shipped + fully wired (studio.h id, sound.h, studioDocs.js, shell.js,
soundcheck slot, showcase cart): **PLUCK · MALLET · FM · ORGAN · EPIANO**. The latest two
this session:

- **`INSTR_ORGAN`** (§8.8.4) — tonewheel, shipped + **published** + timbre confirmed by ear
  ("sounds awesome"). Drive-fizz fix landed (pre-drive HF rolloff). ADR-0016 = branch B
  (combo organ → its own engine when a station proves the recipe; morph is a full axis).
- **`INSTR_EPIANO`** (§8.8.5) — Rhodes/Wurli/Clav, shipped + showcase `epiano.c`. **Rhodes ring
  FIXED** (2026-06-08): the inharmonic tine/bell modes now decay fast (body-vs-bell split) over a
  warm fundamental — "sounds much better". BAKED to constants (2026-06-08): RHO_BODY/RHO_BELL/RHO_BLVL (0.7 / 0.13 / 12). **WAH reworked** (2026-06-08, after rendering navkit — see bottom): toggle off/auto/
  **env**/touch (one per mod source), clav boots into **env** = a fast per-note filter-env quack
  (navkit's real funky-clav recipe), clav decay shortened. PUBLISHED 2026-06-08. The 3 macros all bite now (timbre hammer-hardness fix; bark folds in drive); wah toggle kept but flagged TEMP! (per-voice, provisional).
- **Envelope follower** SHIPPED (2026-06-08) — `instrument_follow`/`note_follow`, the 3rd
  modulation source (tracks the voice's own amplitude → cutoff/vol/pitch). **PARKED/provisional**
  (see PARKED below): its real home is bus-level (§8.10); no audio-notes section while it may be
  cut or moved. Discoverable via studioDocs + 0015 Correction meanwhile. Full 4-place wiring + tripwire.

Canonical state lives here (don't duplicate it — update these):
[STATUS §5](../STATUS.md) · engine specs [§8.8.3 FM / §8.8.4 organ / §8.8.5 EP](instrument-engines.md) ·
roadmap [§8.5](instrument-engines.md) · [ADR-0016](../decisions/0016-combo-organ-recipe-then-macro-or-engine.md).

## Start here next session

Organ + epiano are shipped **and published**; their macro/wah/Rhodes tuning is done (see "Where
we are" + the FIXED/PARKED sections). Nothing on EP is blocking. Next:

1. **`INSTR_PD` (Casio CZ)** ← the next engine (roadmap §8.5 step 6, decision b079874): the cheap
   2-float, buffer-free snack, deeply chiptune-adjacent. **Step-1 design DONE — see §8.8.6**
   (2026-06-08): the engine is genuinely thin (navkit = wavetype + one distortion knob), so the
   3-macro mapping is harmonics=wavetype(snapped) / timbre=static distortion / **morph=DCW-envelope
   depth** — the CZ "wowww" navkit omits, built from our second EG. **Run playbook STEP 0 before any
   code** (§8.8.2): render navkit presets 52/53 with `tools/navkit-render.c` and A/B — the macro
   *architecture* is solid, but the `d` ranges + DCW decay + reso cap are ear-tuned. (STEP 0 exists
   because of the wah detour — don't skip it.)
2. Optional treat: a **Rhodes comping under the organ** (two engines together).
3. When the effects-bus layer (§8.10) eventually opens: revisit the PARKED per-voice wah +
   envelope follower + epiano tremolo (fold into shared phase-coherent bus modulators, or remove).

## ~~OPEN~~ FIXED (2026-06-08) — the Rhodes "doesn't sound Rhodesy" (prime suspect was right)

**Resolved.** WAV envelope analysis (amplitude + a brightness/HF measure) confirmed the prime
suspect: the bell/tine modes decayed in lockstep with the fundamental (no ding). Fix = split the
Rhodes decay into a long BODY (mode 0) and a short BELL (modes ≥1) + a bell-level boost, so the
tine *dings* (brightness drops in ~250ms) over a sustaining warm body. BAKED to constants 2026-06-08 (RHO_BODY/RHO_BELL/RHO_BLVL). The
detail below is kept as the method record (the brightness-envelope trick is reusable).

User flagged this by ear AND hit the same thing porting in navkit. Method that found it, all in
`runtime/sound.h` → `sound_epiano_start` / `sound_epiano_sample`:

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

## Comparing against navkit — the render A/B (a reusable process, 2026-06-08)

When ours sounds different from navkit, **render navkit's actual output and A/B it** rather
than reading its source and guessing. Tools (committed):
- `tools/navkit-render.c` — renders any navkit preset to a WAV (needs the `~/Projects/navkit`
  sibling; build cmd in its header). e.g. preset 180 = "Clav Funky".
- `tools/wav-envelope.js` — plots a 100ms amplitude + brightness(HF) envelope; a real wah/tine
  shows **brightness dropping faster than amplitude**.

**Funky-clav wah — solved (2026-06-08).** Findings from the A/B:
1. **Filter topology is a non-factor.** Chamberlin vs TPT/Cytomic SVF were objectively + audibly
   identical at the resonances we use → the dual-filter scaffold was removed. Don't chase this.
2. **The clav's "wah" is a fast per-note FILTER-ENVELOPE quack** baked into the patch — a resonant
   lowpass whose cutoff snaps shut in ~100ms (brightness LEADS the body down). NOT the bus wah
   (a bus effect, not saved in the patch, off by default) and NOT an envelope follower (which
   hangs the filter open). epiano's clav now boots into this (wah mode "env").
3. **The envelope follower hangs the filter open** — wrong for a percussive clav, but right for
   sustained bass/leads. Kept as a general primitive. *(Open question the user raised: do we even
   need the follower, given the clav wanted the env-snap? — to discuss, don't delete yet.)*
4. The wah **sweep range/curve** matters more than anything: keep it musical (~400–2200), don't
   sweep into the ~20Hz clamp (a bandpass passes nothing there → mud).

> **Watch:** `tools/wav-envelope.js`'s brightness proxy conflates a resonant-filter PEAK with
> perceptual brightness, so it under-shows a resonant snap — trust ears for resonant timbre.

**The "woah woah" wah is a BUS effect — stop chasing it per-voice (2026-06-08).** Rendering
navkit's clav *with its bus wah on* settled it: the deep vocal "woah woah" is the **bus wah** —
a bandpass on the *summed mix*, exponential sweep (300→2500), envelope follower on the *whole
performance*. A per-voice filter can't sweep a chord coherently or pump with the groove. So:
- The funky-clav **quack** (per-note env snap) is done per-voice — that's correct and shippable.
- The great **auto-wah** belongs in the **effects-bus layer (§8.10)**, deferred — alongside
  reverb/Leslie/chorus/the Juno. The envelope follower's real home is there (bus-level), not
  per-voice. This is the strongest reason to **park the per-voice follower** until §8.10.
- **Doc lesson:** the docs *contradicted themselves* — 0015 filed wah as a done per-voice recipe
  (and wrongly equated a per-note env with an envelope follower), while §8.10 listed wah as an
  aux-bus effect. Both now corrected (0015 Correction note + §8.10 wah line). The render is what
  resolved it — confident *doc* claims need the same empirical check as confident reasoning.

## PARKED — provisional, kept but NOT the end goal (2026-06-08, user call)

Both are kept (might be handy soon) but explicitly interim — flagged so we don't mistake them
for finished intent or build more on top of them:
- **Per-voice wah (epiano AUTO/TOUCH flavours)** — a *simple* wah; the real "woah woah" is the
  bus effect above (§8.10). The clav ENV quack stays (it's genuinely per-voice). AUTO/TOUCH will
  likely be replaced by the bus wah.
- **The envelope follower (`instrument_follow`/`note_follow`)** — its real home is bus-level. No
  shipped customer yet. Keep, but it's a re-evaluate-when-§8.10-lands item, not a proven primitive.
- **Per-voice epiano tremolo (`epiano.c` SL_TREM → `LFO_VOLUME` on LFO slot 1, added 2026-06-08)**
  — the suitcase/Wurli amp wobble, the "electric" signature the dry tine+pickup model was missing.
  But on real hardware tremolo is a **bus effect**: one LFO in the suitcase preamp, downstream of
  where the tines sum, so the whole keyboard pulses in lockstep. Our per-voice version resets each
  voice's LFO phase to 0 at note-on (`sound.h:1369`), so **block chords struck on one frame wobble
  coherently** (the common case, sounds right) but **staggered/rolled notes drift out of phase** —
  hardware keeps them locked regardless of strike time. **Per-variant target when this graduates:**
  - **Rhodes SUITCASE wants a stereo AUTO-PAN** — its amp tremolo is really the signal panning
    between two speaker/amp pairs (left↔right movement, not just a level dip). We're a MONO engine
    today, so we can only do amplitude tremolo; the suitcase's defining wobble needs stereo — itself
    a reason its real home is the bus/output layer (§8.10), where a stereo field can exist. Until
    then the mono amp-trem is a stand-in for the suitcase.
  - **Wurlitzer wants mono amplitude tremolo** (the 200A's built-in trem genuinely is level
    modulation, deeper/faster than the Rhodes) — so our per-voice amp-trem already matches the Wurli
    closely; it's only the Rhodes it under-serves.
  - **Clav**: none (the preset zeroes it; a real clav has no tremolo).
  Move it to the bus layer (§8.10) **alongside the auto-wah** — one shared phase-coherent LFO on the
  summed mix, and the home for the suitcase stereo auto-pan. Good interim; not the end goal.

**Hygiene rule (so this happens less):** see the **engine playbook step 0** (§8.8.2) — *render
the reference and locate its LAYER before building*. The wah detour was a per-voice build chasing
a bus-layer target; one render up front would have caught it. Treat confident claims (ours or the
docs') about a reference sound as hypotheses until rendered.
