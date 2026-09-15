# Paper round: the macro mappings for both new engines

> **STATUS: PROPOSED (2026-09-14); modal A/B LIVE in the `modal` cart; FM4 A/B LIVE
> in the `fm4op` cart; ear still open on both.** Hearable fork (2026-09-15): A/B
> keeps the same three knobs (no rematch). Modal key `0` is bowed ring vs short
> strike. FM4 key `0` is √2 tube-bell vs a 1:1 harmonic stack. Autoplay walks a
> scale while the macros step.
> Step 1 of the [`instrument-engines.md`](instrument-engines.md) §8.8.2 playbook for the two
> engines in [`engine-reach.md`](engine-reach.md) §7. Each fork carries a recommendation AND
> its alternative, and **both routes are meant to be heard, not read**: §4's runtime toggle
> ships in `modal` and `fm4op` (tappable A/B, not a `#define`). Pinned-Hz
> wrinkle (§2c): **option 1** — geometry only moves ratios; v1 does not implement `MODE_PIN`.

The constraint is three macros (`harmonics` / `timbre` / `morph`), no per-engine named params ever
([`instrument-engines.md`](instrument-engines.md) §8.1.1), each a percept that audibly steps every
quarter turn on every key, and every position striking at the same loudness.

## 1. The house grammar, which settles more than it looks

Across the thirteen shipped engines the three macros are not arbitrary:

- **`harmonics` = structure or identity** (PLUCK ring, MALLET material, ORGAN registration, EPIANO
  instrument, PIANO voicing, REED bore, BRASS bore, PD wavetype)
- **`timbre` = brightness**, in ten of thirteen
- **`morph` = expression, how you play it** (BOWED bow speed, BRASS breath, MEMBRANE bend, PIANO
  pedal, GUITAR mute, REED breath)

Treat this as a constraint rather than a coincidence: a stranger who learns one engine's knobs has
learned all of them, which is the legibility half of the ADR-0022 bar. Both proposals below land on
it without being forced.

## 2. §7.1 modal: the proposal

Research turned up eight candidate axes (geometry, brightness, damping, position, three exciter
levels, direct gain, mode count) for three macros, so something compounds and something moves to
`MODE_*`.

| macro | meaning | range |
|---|---|---|
| `harmonics` | **GEOMETRY**, the resonator's identity | plate → string → bar/tube → bell/bowl |
| `timbre` | **MATERIAL DAMPING**, brightness and decay tilt moving together | bright and ringing → dull and short |
| `morph` | **EXCITER**, how you play it | strike → blow → bow |

`MODE_*`: excitation position, direct gain (how much raw exciter you hear), mode-count budget.

**The one real move is compounding brightness and damping into `timbre`.** They physically co-vary:
a damped material is both duller at the strike and loses its highs faster, so it is one percept
rather than two parameters. That is the same compounding REED's timbre already does (stiffness plus
aperture, because stiffness alone was too weak), and it is what frees `morph` for the exciter.

**Why the exciter deserves a macro at all.** It is half the engine's name, and
[`engine-reach-modal-research.md`](engine-reach-modal-research.md) argued the exciter is what makes
this one engine instead of three. Putting it on `morph` makes that claim *audible*: sweeping one knob
walks marimba → breath → bowed. An engine called "exciter into resonator" with no exciter knob is a
strange thing to hand someone.

### 2a. The alternative, recorded

**The Elements mapping**: `harmonics` = geometry, `timbre` = brightness, `morph` = damping, with the
exciter baked per voicing. It mirrors a shipping product and every macro is a clean single percept.
Its cost is the one above: no live exciter.

**Ruled out, with the reason.** `harmonics` = material, compounding geometry *and* brightness into
one wood → glass → metal → bell axis, is the most legible option of the three. It is also wrong for
this programme: compounding those two means you cannot have a bright wooden bar, and a xylophone is
exactly that. It buys legibility by spending reach, in the tier whose name is reach.

### 2b. Geometry should be SMOOTH, breaking our own precedent

Our snapped axes (FM ratio, PD wavetype, PIANO voicing) are snapped because intermediate values are
*bad*, and `sound.h` says so for FM: "a continuous ratio is out-of-tune clang everywhere except the
integers." **That reasoning does not transfer.** An object halfway between a bar and a tube is a real
object. Geometry should be the first genuinely smooth structure axis we ship, which is also the whole
point of §7.1 being a surface instead of four islands: a snapped geometry recreates the discrete
islands the engine exists to replace.

### 2c. The wrinkle that only exists because we measured

If geometry crossfades between mode tables, **what happens to a mode pinned at absolute Hz?** (The
2443 Hz marimba mode, measured in the research note §7.) It cannot crossfade toward a ratio. Two
candidate answers, neither yet chosen:

1. **Pinned modes leave the geometry axis.** They are a per-voicing `MODE_*` property, and geometry
   only ever moves ratios. Simple, and it means geometry cannot reach a preset that needs one.
2. **Geometry crossfades their GAIN, not their frequency.** A pinned mode fades out as the geometry
   moves away from the voicing that wanted it. Keeps everything on one axis; costs a gain lane per
   mode.

**Chosen in the port (2026-09-14): option 1.** Geometry only ever moves ratios. A pinned-Hz
mode is a per-voicing property that v1 does not implement (`MODE_PIN` is not in the aux
channel). The STK marimba 2443 Hz partial is therefore not on this engine's geometry axis.

## 3. §7.2 four-op FM: the proposal

**The crux: a 4-op patch's identity is four ratios, four levels and an algorithm, and no single
continuous axis captures that.** The algorithm alone is the weakest candidate for `harmonics`,
because swapping algorithms at fixed ratios is far less audible than changing a ratio, which breaks
the quarter-turn rule immediately.

| macro | meaning | range |
|---|---|---|
| `harmonics` | **VOICING**, snapped: each detent is a whole patch (algorithm + 4 ratios + 4 levels) | tine epiano → tube bell → metal → brass → bass → wood → glass |
| `timbre` | **BRIGHTNESS**, a master index scaling every modulator level | clean → bright → screaming |
| `morph` | **FEEDBACK**, clean → growl → clang | unchanged from the 2-op |

This is exactly the EPIANO (snapped Rhodes/Wurli/Clav) and PIANO (snapped grand/harpsichord/
dulcimer) precedent, and it is how a DX is actually used: pick a patch, then tweak. Every quarter
turn is a different instrument. It keeps the 2-op mapping recognisable, so the new engine reads as a
bigger version of the one people know. And it makes the √2 lesson **data**: the bell voicings carry
irrational ratios, in the table, where the research round put them.

### 3a. The objection, and the reframe that answers it

A fixed voicing list caps reach at whatever we bake, in the reach programme. But **the macros are not
the whole surface.** `instrument_mode` and `note_aux` exist for exactly this, and `INSTR_VOICE`
already set the precedent: three advertised macros, seven raw params behind the aux channel.

So the split is: **the macros serve the player and the showcase cart; `MODE_*` and aux serve depth
and the patch matcher.** The algorithm and the four ratios live underneath, reachable by a search and
by a cart that wants them, absent from the beginner surface. Legibility and reach, without trading
one for the other.

### 3b. The alternative, recorded

**`harmonics` = algorithm** (8 snapped, mirroring the hardware exactly), ratios via `MODE_*`. Purer
as a model of the chip. Expect it to fail the audibility rule, which is the thing to listen for when
A/B-ing the two.

### 3c. A consequence for the patch vector

If FM wants algorithm plus four ratios under `MODE_*`, that is five, and `PmPatch` has **four**
`V_MODE` slots (`pmpatch.h`). Modal wants position, direct gain and mode count, which fits at three.
So the FM decision may force a one-line vector widening plus a re-bless. Small, but it belongs in the
plan rather than arriving mid-build.

## 4. Both routes must be AUDIBLE from one build

The forks above are taste calls, and taste calls are settled by ear, not by reading a table. So:

**The mapping fork is a RUNTIME TOGGLE in the showcase cart, not a compile-time choice.** One key
swaps route A for route B live, the way `bowed` toggles its body with `B` and the way §8.8.2 step 7
wires a live A/B "so the verdict is one keypress". A compile-time `#define` would need two builds,
which makes the comparison a chore and makes it impossible to judge on a phone.

Requirements that follow:

- **One binary, one web build, both routes reachable.** The toggle is a key and a tappable on-screen
  control, since the judging may happen on a phone rather than at the desk.
- **The toggle is visible.** The panel says which route is live, or a blind A/B becomes a guess.
- **Presets are the acceptance test**, per §8.8.2 step 5: name them after hardware (marimba, glass
  bowl, steel drum, tube bell, tine epiano) and the verdict is whether pressing the name sounds like
  the name, under *each* route.
- **Run `node tools/mobile-lint.js <cart>`** before calling either cart done. If the judging happens
  on a phone, "can a phone play this" is part of the deliverable, not a follow-up.

## 5. What is still open

1. Modal: is `morph` the exciter (recommended) or damping (the Elements answer)? **Both
   live in the `modal` cart; ear still owns the verdict.**
2. FM: is `harmonics` a voicing list (recommended) or the algorithm?
   **Both live in the `fm4op` cart; ear still owns the verdict.**
3. ~~Modal: which answer to the pinned-mode wrinkle (§2c)?~~ **Option 1.** Geometry only
   moves ratios; pinned-Hz modes are not on that axis. v1 has no `MODE_PIN`.
4. ~~Whether widening the patch vector to five `MODE_` slots happens with the FM engine.~~
   **No.** The race searches the three macros (8 voicing detents on `V_HARM`).
   `pm_engine_modes(32)` returns 0 — `MODE_FM4_*` is cart depth, not a matcher dim.

Items 1 and 2 are for the ear, and §4 exists so they can be answered that way.

## See also

- [`engine-reach.md`](engine-reach.md) - the root doc, §7 the two engines
- [`engine-reach-modal-research.md`](engine-reach-modal-research.md) - the §5 round behind §2 here
- [`engine-reach-fm-research.md`](engine-reach-fm-research.md) - the §5 round behind §3 here
- [`instrument-engines.md`](instrument-engines.md) - §8.8.2 the playbook, §8.1.1 the three-macro rule
- [`../guides/instrument-carts.md`](../guides/instrument-carts.md) - showcase cart form factors
