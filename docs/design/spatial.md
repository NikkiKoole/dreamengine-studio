# Spatial / positional audio — design plan

> **Status: v1 + v2 SHIPPED (2026-06-15).** v1 = per-voice spatialization
> (`listener`, `listener_vel`, `spatial_model`, `spatial_speed`, `note_pos`,
> `note_motion`, `hit_at`). v2 = **emitter buses** (`instrument_pos`,
> `instrument_motion`): position an instrument's whole effected bus so its FX tail
> (shimmer/reverb) moves *with* it. Showcase cart `spatial` (a UFO flies past as a v2
> emitter — a sustained shimmer tone, so the Doppler reads through the whole pass;
> car = v1 per-voice). **v3 (acoustic zones) remains PROPOSED.** The v1/v2 sections
> below are the record of what was built; see also the [STATUS.md](../STATUS.md) ledger.
>
> Verified at ship: soundcheck compile-gate ok, 900-frame tripwire silent,
> tune-check exit 0 (non-positioned note reads 0¢ → bypass byte-identical). Isolated
> moving source: v1 per-voice +101.8¢ approaching; v2 emitter bus +99.8¢ / −94.1¢.

## The dream (what we're aiming at)

A top-down GTA1-style world. Sound *sources* sit at world coordinates — a car
engine, a gunshot, footsteps, and a **radio blaring a full music mix from a
little speaker on a corner**. The player drives around; the engine derives how
each source *sounds* from geometry: pan from bearing, volume from distance, a
Doppler whoosh as you blow past the speaker — and the speaker's whole *produced*
sound (its echo/reverb tail included) moves as one object, not just its dry
beeps.

That dream is **three layers**, and they are not the same size:

| You want… | Layer | Cost |
|---|---|---|
| Engine drone / gunshot / footstep, panned + Doppler as you pass | **v1** — per-voice spatialization | small; reuses existing pan/pitch path |
| A full radio mix through bus+FX, moving as one object — wet tail and all | **v2** — emitter buses (post-FX bus spatialization + bus Doppler) | medium; bounded to ~7 emitters by bus count |
| Inside / outside / tunnel / carpet — occlusion, reverb zones, materials | **v3** — acoustic zones | mostly *cart-side* logic over existing filter+reverb |

**v1 is the right first brick** and unlocks a lot on its own. But if the
radio-driving-past scene is the actual target, the design we care about is
**v2** — so v1's API and internals are shaped *now* so v2 slots in without a
rewrite (see "Shared machinery" below).

Grounding facts that decide everything here (current `sound.h`):
- `SOUND_VOICES 32` (recently 8→16→32), `SOUND_INSTR_SLOTS 48`, `SOUND_FX_BUSES 8`
  (**bus 0 master + 7 aux**, `sound.h:551`). `sizeof(Voice) = 10,064 B` (~9.83 KB).
- The echo insert (`sound.h:431`) is a **fractional-tap delay with tape-speed
  slew** — i.e. a variable-rate delay line. v2's bus Doppler reuses exactly this
  mechanism; it is not new DSP.
- Per-voice mix hooks already exist: `contrib = s * v->vol * env * trem * 0.2f`
  (`~sound.h:4775`), `float pan = v->pan + pan_mod` (`~:4783`), and `pitch_mul`
  in the phase increment (`~:4806`). The reverb/echo sends are **mono, pre-pan**
  (`~:4806`) — this is *why* v1 alone can't spatialize a wet radio mix.

---

## Shared machinery (the part that makes v2 cheap)

Both a **voice** and an **emitter bus** are "a thing positioned in the world."
Factor the geometry once and reuse it:

```c
// a positionable thing — embedded in Voice (v1) and in an Emitter/bus (v2)
typedef struct {
    float x, y;            // world position (cart pixel coords)
    float vx, vy;          // velocity, world-units/sec (Doppler)
    bool  on;              // false = not spatialized → bypass (gain 1, pan untouched, doppler 1)
    float gain;            // distance attenuation, 0..1   (default 1.0 = bypass)
    float pan_target;      // bearing pan, -1..1
    float doppler_target;  // pitch/varispeed factor       (default 1.0 = bypass)
} Spatial;
```

One pure function does all the math from the **listener + model globals**
(shared by every layer — set once, drive everything):

```c
// globals: listener lx,ly,lvx,lvy ; model ref_dist,max_dist,rolloff ; speed_of_sound c
static void spatial_recompute(Spatial *s);   // writes gain, pan_target, doppler_target
```

What differs between layers is **only the application point**, not the math:

| | v1 voice | v2 emitter bus |
|---|---|---|
| `gain` applied | into `contrib` (pre-bus, per sample) | onto the bus's **post-FX** stereo output |
| pan applied | feeds the existing slewed `v->pan_target` | L/R gain split on the bus output (per `pan_law`) |
| Doppler applied | cheap: multiply `pitch_mul` (phase increment) | varispeed **delay line** on bus output (reuse echo tap, `sound.h:431`) |

This is the whole trick: **v2 is "v1's geometry, applied to a bus's summed
output after its effects."** Design v1's `Spatial` + `spatial_recompute` as
standalone now, and v2 is additive — no v1 rewrite.

---

## v1 — per-voice spatialization (SHIPPED 2026-06-15)

**Scope:** 2D (x,y). Doppler included. Sources = held notes (`note_pos` /
`note_motion`, continuously tracked) **plus** a one-shot snapshot helper
(`hit_at`). Mirrors the OpenAL listener/source model in pixel space.

**Hard guarantee:** dormant and **byte-identical** until a cart calls
`listener()`. `gain` and `doppler_target` default to `1.0` (true bypass), same
discipline as `autopan` / `pan_law` ([stereo.md](stereo.md)). No existing cart
regresses.

### The model (fixed for v1)

World units = cart pixel coordinates. Geometry recomputed **per request (per
frame), never per sample.**

- **Distance** `d = hypot(sx-lx, sy-ly)`.
- **Gain** — inverse-distance, clamped (OpenAL `INVERSE_CLAMPED`):
  `gain = ref / (ref + rolloff·(clamp(d, ref, max) − ref))`, **culled to 0 past
  `max`** so distant emitters are free.
- **Pan** — `pan_target = (sx − lx) / maxf(d, ε)` = the sine of the bearing
  angle, naturally in [−1,1]. Hard-left source → −1; dead-ahead/behind → 0. No
  extra tunable, no front/back ambiguity (correct for stereo).
- **Doppler** — relative velocity `rv = (svx−lvx, svy−lvy)`; radial component
  `vr = dot(rv, unit(source−listener))` (positive = receding).
  `doppler_target = c / (c + clamp(vr, −0.9c, +∞))` — the −0.9c clamp prevents
  the blow-up as `vr → −c`. Per-sample one-pole slew of the applied
  `doppler_mul → doppler_target` kills zipper on fast flybys. `c = 0` disables
  Doppler.

**Defaults** (tunable via `spatial_model` / `spatial_speed`): `ref ≈ 24px`,
`max ≈ 400px` (≈ off-screen on a 320-wide canvas), `rolloff = 1`, `c ≈ 340`
units/sec. Chosen so a first `listener()` + `note_pos()` sounds reasonable with
zero tuning.

### API (studio.h)

```c
// spatial audio — place sources in the world; pan, distance-volume & Doppler derive automatically (spatial.md)
void listener(float x, float y);                  // where the player's ears are (world units); sources position relative to this
void listener_vel(float vx, float vy);            // listener velocity (units/sec) for Doppler; 0 = still (default)
void spatial_model(float ref, float max, float rolloff); // distance falloff: full vol ≤ ref, silent ≥ max
void spatial_speed(float c);                      // speed of sound (units/sec) — Doppler strength; 0 = off
void note_pos(int handle, float x, float y);      // place a held voice in the world → auto pan + distance-volume, slewed
void note_motion(int handle, float vx, float vy); // held voice velocity (units/sec) → Doppler pitch
void hit_at(int midi, int instr, int vol, int dur_ms, float x, float y); // one-shot positioned at trigger (snapshot pan+vol+doppler)
```

Naming: `note_motion` (not `note_vel`) — CLAUDE.md flags `note_vel`/`note_vol`
as a confusion magnet. Adjustable if you'd rather match OpenAL's "velocity."

### File-by-file (v1)

1. **`runtime/sound.h`** — surgical `Edit`s only (re-Read each region first; line
   numbers drift under parallel edits). Add the `Spatial` struct, embed it in
   `Voice`, add the listener/model/speed globals + `spatial_recompute()`. Append
   request kinds (**never renumber** — kinds 2+ are load-bearing, see
   [decision 0013](../decisions/0013-cut-music-api.md)): `SR_LISTENER`,
   `SR_LISTENER_VEL`, `SR_SPATIAL_MODEL`, `SR_SPATIAL_SPEED`, `SR_NOTE_POS`,
   `SR_NOTE_MOTION`. Listener/model/speed handlers re-sweep all `on` voices;
   per-handle handlers recompute one. In the per-sample mix:
   `contrib *= v->sp.gain;` and `pitch_mul *= v->doppler_mul;` (both 1.0 =
   bypass) + the one-pole Doppler slew. `hit_at` = existing one-shot path, then
   snapshot once (no tracking).
2. **`runtime/studio.h`** — declare the 7 funcs, house-style `//` one-liners.
3. **`runtime/studio.c`** — thin request-enqueue wrappers (mirror `note_pan`).
   Then `node tools/gen-tcc-symbols.js` so the regenerated
   `studio_tcc_symbols.h` lands in the same commit (libtcc backend).
4. **`editor/src/studioDocs.js`** — `sig`+`doc` (one-line example) for all 7;
   `sig` matches `studio.h` exactly.
5. **`editor/src/shell.js`** — new **"spatial"** section listing the 7.
6. **Flagship cart** `tools/carts/spatial.c` — a held emitter you drag around a
   fixed listener (hear pan sweep + distance falloff) **plus** a sprite that
   flies straight past the listener on a timer (the unmistakable Doppler whoosh +
   L→R sweep, velocity fed via `note_motion`). `watch()` distance/pan/doppler
   under `#ifdef DE_TRACE`. Build → `--run` (bake screenshot) → register in
   `index.json` with tags → `lint-carts.js` → `cart-status.js`.
7. **Docs** — promote this file's v1 section to shipped; STATUS ledger entry;
   `audio-notes.md` §17 ledger line.

### Verification gates (in order)
1. `node tools/play.js soundcheck script /dev/null --headless --frames 2` → `ok`, no `[sound] WARNING`.
2. 900-frame `[sound]` tripwire silent.
3. `node tools/tune-check.js --quiet` exit 0 — **a non-positioned note must still read 0¢** (proves Doppler bypass is truly 1.0). Mandatory: Doppler touches the pitch path.
4. `play.js … --wav` of the flyby → pitch rises-then-falls through the pass, energy peaks at closest approach (`tools/wav-analyze.js`).
5. `ui-audit` / `mobile-lint` on the demo cart.
6. After every `sound.h` commit: `git show HEAD:runtime/sound.h | grep '<sentinel>'` — confirm the change survived a parallel commit.

---

## v2 — emitter buses (SHIPPED 2026-06-15) — the radio-driving-past dream

> **Built as specced.** `instrument_pos(slot,x,y)` / `instrument_motion(slot,vx,vy)`
> position an instrument's whole effected aux bus at the fold (after the per-instrument
> shimmer line, order `inserts → FX → shimmer → spatial → fold`). Shared geometry:
> `spatial_geom()` extracted from v1 (v1 stayed byte-identical). Bus Doppler = a 2-grain
> variable-ratio pitch shifter (the generalized octave-up) — sustained shift, bounded
> ~70 ms buffer, crossfaded to dry near unity so a still emitter is transparent. Dormant
> until `instrument_pos()` (`emit_on[b]=false` → fold untouched). ~192 KB `.bss`
> (grain buffers × 8 buses), 0 download. `bus_pos`/`bus_motion` stayed internal as planned
> (no shared-bus routing yet). The design notes below are the build record.

**The problem v1 can't solve.** A radio groove is many voices (kick + bass +
lead) summed into an FX bus with its own echo/reverb/drive. v1 spatializes
**per-voice, pre-bus**, and the reverb/echo sends are **mono and pre-pan**. So
driving past, the dry oscillators would pan/Doppler but the **reverb tail and
the whole produced sound would sit dead-center at full volume** — the illusion
breaks exactly where it matters most.

**The fix: spatialize the bus, not the voice.** An **emitter = one aux bus.** Run
the station's groove + its effect chain on that bus, then apply the *same
`Spatial`* (distance gain + pan + Doppler) to the bus's **stereo output,
post-FX**, before it sums into master. Now the whole produced sound — wet tails
and all — moves as one object.

> **The per-instrument FX buses are the emitters-in-waiting.** `instrument_shimmer`
> / `instrument_grains` / `instrument_chorus` (and the per-instrument shimmer pool in
> [`boutique-pedals-roadmap.md`](boutique-pedals-roadmap.md) → Follow-ups) already put
> one part's *whole effected signal* on its own aux bus. v1 + those gives "shimmered
> pad, dry drums, both positioned (dry pans/Dopplers, the wash is ambient/centered)" —
> which is usually what you want. v2 is precisely the step that makes such a bus's **wet
> tail** move/Doppler with the source too: the same aux bus, now carrying a `Spatial`.
> So per-instrument FX (isolation) and emitter buses (spatialization) are orthogonal and
> compose — build the FX-on-a-bus first, position the bus in v2.

### What v2 adds (additive — no v1 rewrite)

- An `Emitter` owning a `Spatial` (the struct from "Shared machinery") bound to
  an aux bus index. `spatial_recompute()` is reused verbatim.
- **Application at bus mixdown** (where each aux bus sums to master): multiply
  the bus's L/R by `gain`, split by `pan_target` under the active `pan_law`,
  and run the bus output through a **varispeed delay line** keyed by
  `doppler_target` — the same fractional-tap + slew mechanism as the echo insert
  (`sound.h:431`). One delay buffer per emitter bus.

> **The exact hook (as of `5561f8d`, 2026-06-15).** The aux-bus fold loop
> `for (int b = 1; b < SOUND_FX_BUSES; b++)` now runs, per bus:
> `apply_insert chain → leslie → sidechain → per-instrument shimmer → busL[0] += busL[b]`.
> Drop the v2 spatialization **right after the per-instrument shimmer line, before the
> `busL[0] += busL[b]` fold** — i.e. on the fully-produced `busL[b]/busR[b]`. That ordering
> is the point: you position the *finished wet source*, so its shimmer/reverb tail moves
> *with* it (the thing v1 can't do). The emitter bus **is** an instrument's claimed aux bus
> (`instr_bank.fx_bus` via `fx_bus_for`) — `instrument_shimmer`/`instrument_grains`/
> `instrument_chorus` are the emitters-in-waiting: one part's whole effected signal already
> lives on its own bus. Note master shimmer is bit-exact-locked (`shimmertest --det` md5
> `9587acf…`) — it runs master-stage (tank 0), *not* in this loop, so it's independent of v2.
- **API — prefer the slot-facing form** (decided 2026-06-15): expose
  `instrument_pos`/`instrument_motion`, **not** raw `bus_pos`/`bus_motion`:
  ```c
  void instrument_pos(int slot, float x, float y);      // place an instrument's bus (emitter) in the world
  void instrument_motion(int slot, float vx, float vy); // its velocity → bus Doppler
  ```
  **Why slot-facing:** symmetrical with the whole `instrument_*` family (`instrument_shimmer`/
  `instrument_reverb`/`instrument_chorus` — all take a slot, resolve to a bus via `fx_bus_for`);
  carts never touch a bus index, so it's just another `instrument_*` member, no new concept. Make
  it a thin wrapper over an internal **by-bus** function (`spatial_set_bus(busIdx, …)`) so the
  plumbing is bus-addressed under the hood. A *public* `bus_pos(bus, …)` waits until there's a way
  to route *multiple* instruments onto *one* shared emitter bus (the "whole radio mix moves as one
  object" case) — until then it's just `instrument_pos` with the internal bus index leaked. So:
  `instrument_pos` now; `bus_pos` when shared-emitter-bus routing exists.
  Same `listener()` / `spatial_model()` / `spatial_speed()` globals drive both
  layers — set once.

### Hard limits (state them; don't let them surprise a cart author)
- **~7 simultaneous fully-effected emitters** — the 7 aux buses, which
  instruments *also* compete for. A GTA world **culls to nearby emitters**; 7
  *audible* radios is plenty, but it is a hard ceiling. `log()` when a cart
  oversubscribes rather than silently dropping one.
- Each emitter bus with Doppler costs one delay buffer (~the echo buffer size).
- Bus Doppler varispeeds a full mix — heavier and subtler than per-voice
  Doppler; expect to tune the slew so a fast drive-by bends musically, not
  glitchily.

### The "radio cart" conceptual snag
You can't embed another cart — **only one cart runs at a time.** The engine is
generative-first (the radio stations are *code*, see
[decision 0006](../decisions/0006-library-carts-not-engine.md)). So an in-world
radio is *your game cart generating that station's groove* into an emitter bus,
not "drop in the radio cart." On-brand, but worth saying out loud in the v2
guide: a reusable "station generator" helper (a `radio.h`-style header a game
can call to fill an emitter bus) is the natural companion deliverable.

---

## v3 — acoustic zones (inside / outside / soft carpet)

v1 and v2 are both **free-field**: direct line-of-sight only, no walls, rooms, or
materials. v3 is environmental acoustics — and the encouraging part is **the DSP
already exists**; v3 is wiring + game-side zone logic, not a new audio engine.

- **Occlusion** (wall between you and source → muffled): a low-pass on the
  emitter driven by "how blocked." `FILTER_LOW` + `note_cutoff` exist. The game
  raycasts listener→source against walls and sets cutoff + extra attenuation. A
  closed door = darker + quieter.
- **Inside vs outside / tunnel boom** (the big one): set **reverb size + send by
  the zone the listener is in.** Reverb buses + size exist. Step indoors → master
  reverb blooms; step out → dry and open. Mostly a zone lookup feeding existing
  knobs.
- **Materials (carpet vs tile)**: HF absorption + reflection amount → per-zone
  EQ/LP + reverb-decay tweak. Carpet = short, dark tail; tile = bright, long.

The key realization: **the game already knows where the walls, rooms, and floor
materials are.** v3 is a thin cart-side "acoustic zone" model feeding the
engine's existing filter + reverb knobs — arguably *more game logic than engine
work* — with maybe a small convenience API so every cart doesn't hand-roll it.
Out of scope until v1 (and probably v2) ship; recorded here so v1's API doesn't
accidentally foreclose it.

### The industry vocabulary (use the real terms)

- **Occlusion / obstruction / exclusion** (Creative EAX → FMOD/Wwise): *obstruction* =
  direct path blocked, same room (muffle dry, reverb intact); *occlusion* = wall to
  another space (muffle dry **and** wet); *exclusion* = direct open, reverb path blocked.
- **Rooms + portals** (Wwise Spatial Audio, Steam Audio, Resonance) — the scalable model:
  don't model individual walls, partition into **rooms** joined by **portals** (doorways).
  A source next door is heard *through the portal* (localized at the doorway, muffled).
  "Wall thickness" generalizes to **transmission loss** (sound *through* the wall) vs the
  cheaper path *around* via a portal — sound takes whichever is louder.
- **Geometry-query ladder**: raycast(s) listener→source against the tilemap → room/zone
  membership → baked propagation (Steam Audio; overkill here).

### Absorption — where the physics gives a formula

Every surface has an **absorption coefficient α** (0..1) **per frequency band**; α≈0 =
reflector (tile/stone/glass), α≈1 = eats everything (drape/open window). Crucially **most
materials absorb highs far more than lows** → soft rooms sound *dark + dead*, hard rooms
*bright + long*. The decay time follows **Sabine**:

```
RT60 = 0.161 · V / Σ(Sᵢ·αᵢ)     (room volume ÷ total surface absorption)
```

→ bigger room = longer tail; more/softer surfaces = shorter, deader tail; and because α is
larger for highs, the tail **darkens as it decays**. This maps straight onto knobs we have:
- **`reverb(size, damping)`**: `size` ≈ RT60 (derivable from zone area ÷ α via Sabine),
  `damping` ≈ **HF absorption** (bright tile ↔ dark carpet — literally the high-freq α).
- **per-band** absorption (if ever needed) = `reverb_bus_fx(tank, FX_EQ, …)` on the tail —
  already expressible, no new API.

So a "material" = a `(size, damping[, EQ])` preset (tile = long/bright, carpet = short/dark,
wood = mid). Absorption is **the physical justification for the reverb knobs we already
shipped**, not a new primitive.

### The clean split + the one convergent gap

**The cart computes "how blocked" (occlusion 0..1) and "which zone"; the engine just makes
it sound blocked.** The engine stays **geometry-agnostic** — walls/thickness/rooms/portals/
raycasts all live in the cart's model, feeding `note_cutoff` (muffle), gain (attenuate),
and a zone reverb. That keeps the engine surface tiny.

All three axes — occlusion, zones, materials — converge on **one** engine need: a reverb
you can **ride/crossfade per frame** as the listener crosses rooms. But effects are
**set-and-hold** (CLAUDE.md): re-calling `reverb()` every frame churns the DSP → *stutter*.
So the likely API that "drops out" of v3 is a **slewed/crossfadable zone reverb** (a
reverb-target that glides, or crossfading two tanks — the tank pool helps), **not** anything
about walls. A second candidate that *generalizes beyond audio*: a **tilemap line-of-sight /
raycast** primitive (occlusion *and* enemy vision/stealth/lighting want exactly "can A see B").

> **PROBE DONE (2026-06-15) — `acoustics` cart.** Built the walker below; the trace confirms
> zone (TILE→WOOD→GRASS) and occlusion (radio↔machine) both track the player. **Findings:** the
> mechanic *is* cart-side over existing knobs (`note_cutoff` Hz+slewed = great for the muffle);
> two real engine asks dropped out — **(a) `note_gain(handle, float)`** (note_vol's 0..7 steps
> audibly under occlusion) and **(b) a rideable/crossfadable zone reverb** (set-and-hold blocks
> per-frame `reverb()` → rooms jump, don't crossfade). Plan: a cart-land **`acoustics.h`** helper
> (raycast occlusion + zone/material `(size,damping)` tables → the knobs) + those two conveniences;
> a tilemap line-of-sight primitive is an optional generalizing bonus.

### The probe (DONE — `acoustics` cart, findings above)

A `"probe"`-tagged top-down cart — a little guy walking rooms — to *hear* where it hurts and
let the API drop out: **different floor materials** (footstep tone/reverb per ground: tile/
carpet/wood/grass — the player's own sound carries the material + room), **walls/obstacles**
that occlude in-room **emitters** (raycast listener→emitter, drive `note_cutoff` + gain),
**multiple rooms with different reverbs** (the set-and-hold stutter shows here), a **doorway**
(does it need portal localization or is muffle+attenuate enough?), and **thick vs thin walls**
(transmission). Hand-wired against existing knobs, no new engine API — the report is *what
dropped out* (expected: a cart-land `acoustics.h` helper + the rideable-reverb gap, maybe a
line-of-sight primitive).

### Designed solutions for the two engine asks (ready to build when `sound.h` is free)

Both are small, byte-identical-when-dormant, and mirror existing idioms. Append the request
kinds after the current enum max (re-read the tail first — it drifts under parallel edits).

**(a) `note_gain(int handle, float g)` — a continuous per-voice gain.**
The problem: `note_vol` quantizes to 0..7, so occlusion attenuation steps audibly. Internally
`v->vol` is *already* a float (0..1); the fix is a **separate** float multiplier so musical
velocity (`note_vol`) and continuous attenuation (`note_gain`, e.g. occlusion/ducking) compose
instead of fighting — exactly how v1's `sp_gain` was layered in.
- Voice: add `float gain, gain_target;` (init `1.0` in `sound_setup_note`).
- Per-sample slew next to the others: `v->gain += (v->gain_target - v->gain) * 0.003f;`
- Fold into the contrib line (now): `contrib = s * v->vol * env * trem * v->sp_gain * v->gain * 0.2f;`
  (`gain` defaults 1.0 → multiplying by exactly 1.0 is identity → **byte-identical** until used.)
- `SR_NOTE_GAIN` handler: `v->gain_target = clamp(r.a/1000, 0, 1)` (e0/e1 = handle). Wrapper mirrors
  `note_vol`. Cost ~2 floats × 32 voices, negligible CPU. 4-place wiring + tcc + a `studioDocs`/`shell`
  entry. Verify: an occlusion sweep reads smooth where `note_vol` stepped.

**(b) Rideable zone reverb — slew the tank's `fb`/`damp`.**
Key finding from the probe: `reverb()` is **cheap to re-call** (`SR_REVERB` just sets two floats,
`rvb_tank[0].fb` + `.damp` — no buffer rebuild), so it is *not* a churn. The abruptness is purely
that `fb`/`damp` **step instantly** (no slew) → the tail's decay/brightness jumps when you cross a
zone. So the fix is just a slew, gated so non-users stay byte-identical:
- `ReverbTank`: add `float fb_target, damp_target, glide;` (`glide` = per-sample slew coef, default 0).
- **All existing setters snap both** (`fb = fb_target = …; damp = damp_target = …`) → instant, unchanged.
- In `reverb_process`, gate the slew: `if (t->glide > 0) { t->fb += (t->fb_target - t->fb) * t->glide; t->damp += (t->damp_target - t->damp) * t->glide; }` — non-gliding tanks skip it entirely → **byte-identical + zero overhead**.
- New **`reverb_glide(int ms)`** sets the master tank's `glide` (0 = snap, the default; >0 = glide over
  ~ms, reusing `note_glide`'s `k = 1000/(ms·SR)` idiom). When `glide > 0`, `reverb()` sets the *targets
  only* (doesn't snap), so the tank morphs. Usage: `reverb_glide(250)` once, then `reverb(zonePreset)`
  on each room change → smooth morph instead of a step.
- Caveat: this **morphs one tank** (decay/brightness glide), not a true crossfade. Great for adjacent
  rooms; for an extreme jump (closet → cathedral) the morph reads as a swell. The higher-fidelity
  fallback is **crossfading two tanks** (we have a 3-tank pool) — note it as the v3.1 option, don't
  build it unless the morph proves insufficient.
- Still **listener-centric** (master tank = the listener's room). An emitter carrying *its own* room's
  reverb through a portal is the deferred rooms+portals refinement, unchanged by this.

The rest of v3 (occlusion raycast, zone/material `(size,damping)` tables) stays a cart-land
`acoustics.h` helper over these knobs — no further engine API.

---

## Cost estimates (guesstimate)

Grounding numbers: 44100 Hz stereo, 32 voices (`sizeof(Voice) = 10,064 B`),
8 buses, echo line = 2 s ≈ 352 KB per buffer (`sound.h:407,551`).

### v1 — per-voice
| Resource | Estimate | Why |
|---|---|---|
| **RAM** | **~1 KB static .bss, 0 download** | `Spatial` in `Voice` ≈ 32 B × 32 voices = 1 KB + listener/model globals ≈ 40 B. Nothing dynamic. (Negligible against the existing ~314 KB voice array.) |
| **CPU / sample** | **negligible — < 0.1 % of one core** | ~3–4 extra flops per active voice (`contrib *= gain`, `pitch_mul *= doppler_mul`, the one-pole Doppler slew). 32 × 44100 × 4 ≈ 5.6 M flops/s — lost next to the oscillator + SVF + drive each voice already runs. |
| **CPU / frame** | **microseconds** | `spatial_recompute` = 1 `hypot` + 1 divide + 1 dot, re-swept over ≤32 voices when the listener moves (≤1920/s). |

**v1 is effectively free at runtime — the cost is implementation, not cycles.**

### v2 — emitter buses
| Resource | Estimate | Why |
|---|---|---|
| **RAM** | **~250 KB static .bss, 0 download** (or per-moving-emitter) | The only real cost. Bus Doppler needs a varispeed delay line per emitter — *milliseconds*, not the echo line's 2 s. ~100 ms **stereo** = 4410 × 2 × 4 ≈ 35 KB; × 7 aux buses ≈ 247 KB — less than one echo buffer's worth, spread thin. Allocate only for emitters that move and it drops. |
| **CPU** | **negligible — ~6 M flops/s** | Per bus per sample: gain (2 mul), pan split (2–4 mul, + trig only on `PAN_POWER`), fractional-tap delay read (~6 flops × 2 ch) ≈ 20 flops × 7 × 44100. The bus FX chains already cost far more. |

**v2's real cost is two budgets, not flops:**
- **7 emitters max** (the 7 aux buses, shared with per-instrument FX).
- **32 voices total** (recently bumped from 16), shared across the player's own
  sounds *and* every audible emitter's groove (~3–5 voices each). The 16→32 bump
  roughly doubled the headroom, but a few nearby stations + player SFX still
  exhaust polyphony. **This is the binding constraint** — culling distant emitters
  (and not generating their grooves at all) is mandatory. See the voice-count
  lever below.

### v3 — acoustic zones
**~0 engine cost** — occlusion reuses the existing per-voice/bus low-pass, zones
reuse the existing reverb, materials are an EQ tweak. The only real cost is
**game-side raycasting** (listener→source vs walls), scaling with
emitters × wall segments, fully cullable. Cart CPU, not audio-thread CPU.

### Implementation effort (the other cost)
- **v1** — a focused session: ~80–120 lines in `sound.h` (struct + recompute +
  6 request kinds + 3 per-sample hooks), ~30 lines `studio.h`/`.c` wiring, docs,
  one demo cart, the verification gates.
- **v2** — a few sessions: emitter abstraction, post-FX bus application, the
  varispeed delay (reuses the echo machinery), a voice-budget/culling helper,
  the `radio.h`-style station-generator companion.
- **v3** — open-ended, mostly cart-side; a small engine helper at most.

### The voice-count lever (`SOUND_VOICES`, `sound.h:22`) — measured
Because v2 is **polyphony-bound**, raising `SOUND_VOICES` (currently **32**, after
an 8→16→32 history) is the single change that most relaxes its ceiling. Measured
costs (`sizeof(Voice) = 10,064 B` ≈ 9.83 KB):
- **RAM** scales linearly: **+10,064 B (~9.83 KB) per added slot, 0 download.**
  Current 32 voices = ~314 KB; the 16→32 bump added ~157 KB; **32→64 would add
  another ~314 KB** (→ ~628 KB). RAM is never the blocker — even 64 voices is
  sub-MB .bss. (~40% of each `Voice` is the `SOUND_KS_MAX = 1024`-float Karplus
  line, 4 KB; shrinking it raises the ~43 Hz low-frequency floor.)
- **CPU** scales linearly *in active voices only* — idle slots cost nothing, so
  the worst case (every voice sounding) is what to budget. **CPU is the real
  cost**, per the `sound.h:22` comment. 32→64 ≈ doubles the audio-thread peak.
- **Hard gate going past 32:** `SOUND_HANDLE_BITS` (`sound.h:317`) must hold
  `SOUND_VOICES − 1`. It's `5` now (0..31). **64 voices needs ≥6 bits** — the
  `_Static_assert` at `sound.h:319` fails the build loudly if you bump
  `SOUND_VOICES` without it. The handle packs slot (low bits) + generation
  (above), so widening the slot field narrows the generation counter (still
  ample at 26 bits).
- **No other structural blocker** seen: voices are a fixed handle-indexed array;
  the steal/declick + `sound_unclaim_held` paths already loop `SOUND_VOICES`.
  Re-run the soundcheck tripwire + tune-check after bumping (handle/steal logic
  is sensitive). A bump pairs naturally with v2 — more emitters need more voices.

---

## Open calls / risks
- **Hot-file contention** is the #1 reason v1 is on hold — coordinate the window.
- **`note_motion` vs `note_vel`** naming — see v1 API note.
- **Listener orientation/facing** — out of scope for 2D (stereo has no
  front/back); a `listener_facing()` only matters if we ever go 3D.
- **One rolloff model** (inverse-clamped). A future linear/exponential mode can
  be a `spatial_model` mode arg without breaking the signature.
- **Emitter ceiling (7)** — if a game genuinely needs more simultaneous
  effected emitters, that's a `SOUND_FX_BUSES` bump (RAM cost, `sound.h:551`),
  not a v2 redesign.
