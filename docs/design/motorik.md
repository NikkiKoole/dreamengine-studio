# motorik — the krautrock driver (Neu! × Stereolab) — build spec

> **Status: spec'd, unbuilt** (2026-06-08). The one krautrock entry in
> [`future-stations.md`](future-stations.md) (the parking lot), promoted to a
> full spec because `INSTR_ORGAN` (shipped 2026-06-07) unblocks its signature
> timbre — the Farfisa drone — and because it proves a genuinely new arrangement
> brain. Engine-side companions: [`../guides/game-music.md`](../guides/game-music.md)
> (the brain catalog + `radio.h` chassis), [`radio-instrument-options.md`](radio-instrument-options.md)
> (the per-song timbre roll), [`instrument-engines.md`](instrument-engines.md) §8.8.4
> (the organ engine). House (`house.c`) is the direct ancestor — read its
> `ride_at()` first; motorik is that idea with the sections removed.

## The essence

The motorik beat is Klaus Dinger's relentless 4/4 (Neu!'s *Hallogallo*, Can's
*Mother Sky*): kick on every quarter, snare understated on 2&4, straight-8th hat,
**no fills, no swing, no let-up** — hypnotic forward motion. Stereolab is its
retro-futurist descendant: a thin **Farfisa/combo-organ drone**, fuzzy **maj7
chords planing** on top, a bright topline, lounge-pop sweetness over the motor.

Two reasons it earns the slot (the two build-order axes):

1. **It proves a new brain — THE PROCESS FORM** (see below). Every shipped
   station has *sections* (8×8, verse/chorus, head/solo). Motorik has **none**.
2. **Sneaky-high taste fit + the best game-loop music on the dial.** Farfisa +
   Moog + lounge sits at exotica ∩ Plantasia ∩ ymo. And because it has no section
   seams, it's the one station that loops as game background *with nothing to
   interrupt* — the form is a smooth function, not a sequence of blocks.

Tempo **132–150**, machine-tight (±0 ms — the relentlessness IS the point; this
is the anti-`tango`, the station with the *least* performance jitter on the dial).

---

## The heart: THE PROCESS FORM (the new brain)

House proved "the filter ride is the form" — but house still keys its ride off
`sect_of(bar)` (a `FORM[8]` of named sections). **Motorik deletes the sections.**
There is no `FORM[]` array and no `sect_of()`. Instead:

```c
// seeded once per song
static int   songLenBars;      // e.g. 48..96 bars (srnd) — the form's denominator
// derived every step
double prog = (double)songStep / (songLenBars * 16);   // 0..1 over the WHOLE song
```

Everything that other stations switch on `section` becomes a **continuous
function of `prog`**:

- **Accumulation / subtraction.** Each layer has a seeded enter threshold (and
  some a leave threshold near the end) on `prog` — the band assembles itself and
  then strips back down as one long arc, never on a bar boundary:

  | layer | enters ~ | leaves ~ | notes |
  |---|---|---|---|
  | motorik kit (kick+hat) | 0.00 | 0.92 | the pulse is always there first |
  | Farfisa organ drone | 0.05 | 0.95 | the bed |
  | Moog octave bass | 0.12 | 0.88 | |
  | snare/backbeat | 0.18 | 0.90 | understated |
  | maj7 planing pad | 0.30 | 0.80 | the Stereolab "oohs" |
  | topline lead (re-pitched cell) | 0.50 | 0.78 | enters late, the payoff |

  (Thresholds jittered ±0.05 per song by `srnd`, so no two builds assemble in
  exactly the same place. The feel knob shifts them earlier/later as a group —
  the `rad_level`-style intensity offset, applied to `prog` not to a section
  base.)

- **The intensity arc** is `smooth(prog)` — a rise to a plateau then a gentle
  fall — replacing `level_of(bar)`. Adapt house's `ride_at` shape but feed it
  `prog` directly instead of the per-phrase `t`:

  ```c
  static float arc_at(double prog) {            // 0..1, the whole-song envelope
      if (prog < 0.25) return prog / 0.25 * 0.7;             // build in
      if (prog < 0.75) return 0.7 + 0.3 * (prog - 0.25)/0.5; // long plateau, still rising
      return 1.0 - (prog - 0.75)/0.25 * 0.85;                // ebb out
  }
  ```

- **No fill grammar — deliberately.** The kit never fills, never turns around.
  Invariance is the brain; resist the urge to add motion.

The acceptance test for the brain (below) is literally "prove there are no
seams": the layer-enter events are monotonic in `prog` and the loop has no
bar-quantized discontinuity.

## The once-per-song modulation EVENT (the second, smaller new brain)

Krautrock's other trick: the single key change that lands like a *story beat*,
not a grid position. One seeded modulation per song:

```c
static double modAt;     // srnd → ~0.55..0.78
static int    modIv;     // srnd over {+2, +3, +5, +7} semitones (whole step / m3 / 4th / 5th)
// in the step player, fire ONCE:
if (!modDone && prog >= modAt) { sng.keyPc = (sng.keyPc + modIv) % 12; modDone = true; }
```

The whole drone lifts under the topline — the lift is the event. (Performance vs
composition: `modAt`/`modIv` are seeded, so a pinned seed lifts at the same place
every time. The lift itself is a composition fact, not a performance roll.)

---

## Brain pick (shop the catalog first)

| slot | brain | source | motorik's twist |
|---|---|---|---|
| **form** | **THE PROCESS FORM** | **NEW** (this cart) | continuous accumulation on `prog`, no sections |
| chord | #2 modal drift + planing | `ambient.c` (drift) | one-chord drone + **maj7 planing** voice on top; lydian/ionian |
| time | machine-tight ±0 | `citypop.c`/`house.c` | even harder — no humanize, no fills |
| bass | octave pulse | `house.c` (octave disco pop) | straight-8th root pump, not disco-funky |
| melody | #1 the re-pitched cell | everywhere | enters at `prog ≈ 0.5`, sweet/simple |
| band | **INSTR_ORGAN** Farfisa + SAW Moog + SAW/SQUARE pad | organ.c, house strings | the organ drone is what ORGAN unblocked |
| modulation | **once-per-song key lift** | **NEW** (this cart) | the story-beat event |

Reused chassis (verbatim from `radio.h`): `rad_srnd` + seed/history, `RadioClock`
schedule-ahead, `rad_lead_to` (for the planing voice), `rad_input`, the whole
draw chassis (`rad_body`/`rad_dial`/`rad_knob_*`/`rad_help_panel`/`rad_footer`/
`rad_power_led`), the tone knob (`RAD_TONEMUL`/`apply_voicing`). Optional:
`solo.h` jam strip (a melodica over the drone — the scale never changes, so it's
the easiest jam target on the dial).

---

## The band (instruments)

```c
#define I_ORG   5   // Farfisa drone — INSTR_ORGAN, held
#define I_BASS  6   // Moog octave pulse
#define I_PAD   7   // maj7 planing pad (string-machine-ish)
#define I_LEAD  8   // the topline cell
#define I_KICK  9
#define I_SNR  10
#define I_HAT  11
#define I_SOLO 12   // optional solo.h jam melodica
```

- **Farfisa drone — `INSTR_ORGAN`.** A thin, bright combo registration: start
  from organ.c preset #1 "combo" (`harmonics 0.19, timbre 0.45, morph 0.30`) and
  brighten — Stereolab's Farfisa buzzes. Per-song roll between a **bright Farfisa
  night** (`timbre ~0.6`, a little morph chorus) and a **darker Vox night**
  (`timbre ~0.35`, morph 0). Held voices (`note_on`/`note_glide`, like `ambient.c`)
  — this is the bed, not a chop.
- **Moog octave bass.** `INSTR_SAW`+LP (or SQUARE), straight 8ths on the root,
  occasional octave jump. Roll the wave per song (round saw vs buzzy square).
- **maj7 planing pad.** 2 held `SAW`+LP voices (house's string-machine canvas),
  gliding parallel maj7 shapes up/down a few scale degrees and back via
  `rad_lead_to` on a maj7 interval set `{0,4,7,11}`. The "harmony" is this voice
  wandering over the static drone.
- **topline lead.** Bright `SQUARE`/`TRI` (or a thin Moog `SAW`), a simple
  re-pitched cell in the mode; enters late. Stereolab's wordless "ba-ba" sweetness.
- **motorik kit — a LIVE kit, quantized dead to the grid.** The motorik beat is
  a *human drummer* (Dinger on Neu!, Liebezeit in Can; Andy Ramsay in Stereolab),
  not a drum machine — the relentlessness is a person, and the machine-tightness
  comes from snapping that kit hard to the grid, not from a box. So roll its own
  acoustic-ish kit: kick `SINE`+pitch-env on every quarter, snare `NOISE`+band on
  2&4 (understated), hat `NOISE`+HP straight 8ths. No open-hat breaths, no fills.
  > **Do NOT borrow `cr78.c`.** The CR-78 (1978) is the wrong era/genre — it's a
  > late-70s pop/new-wave box (Blondie, Phil Collins, OMD), already on loan to
  > `yacht` (Hall & Oates machine-soul). It does not belong on a krautrock cart.

---

## The face / window art

Two candidate directions (pick one in the build):

1. **The autobahn at night** (recommended — it literally visualizes "motor" +
   the no-seams form): a road vanishing to a horizon, centerline dashes streaming
   toward the viewer at the pulse rate; each layer that enters lights another
   element (taillights, a skyline band, stars), so the *accumulation arc is the
   scene filling in*, and the road never stops — the visual has no seam either.
2. **The op-art sleeve** — a Stereolab/Marimekko geometric pattern (concentric
   rings or a grid) that rotates/breathes with the arc. Prettier, less legible as
   "what is the music doing right now."

The modulation event = a visible beat (headlights flare / the horizon shifts hue)
the moment the key lifts.

---

## Seed: composition vs performance

**Composition (`rad_srnd`, = the song):** key, mode, `songLenBars`, the per-layer
enter/leave thresholds, `modAt` + `modIv`, the lead cell, the planing path, the
organ registration roll, the bass wave, title, dial freq.

**Performance (`rnd`, never seeded):** almost nothing — this is the point.
Maybe a ±1 hat velocity wobble. Motorik has the **least** performance variation
of any station; the invariance is sacred. (Contrast `dub`'s desk, which re-rolls
everything every listen.)

---

## Build plan (the checklist)

1. `tools/carts/motorik.c` — settings-only `.cart.js` not needed (draws with
   primitives like the other radios; if the autobahn wants sprites, a small
   `.cart.js` with a couple of slots).
2. Build + bake: `node tools/make-cart.js tools/carts/motorik.c editor/public/carts/motorik.cart.png`
   then `--run` to bake the screenshot.
3. Register in `editor/public/carts/index.json` with the radio family's tags
   (match a sibling like `house`/`ymo`: `kind` + `genre`, `homage:
   "Neu! / Stereolab"`); `node tools/lint-carts.js`.
4. Docs: flip the `future-stations.md` motorik entry to ✅ (point at this spec),
   add a `game-music.md` cheat-sheet row, and mark **THE PROCESS FORM** ✅ in the
   brain catalog's Form-brains line (and the time-brains "*future:* the process
   form (motorik)" note).

## Verification

- **Seed-compat** — the standard `radio.h` trace-diff: pin a seed, run
  `--det --seed 7 --frames 3000`, the `.w` stream must be reproducible.
- **PROCESS FORM proof (the new one)** — `watch()` the layer-on bitmask + `prog`
  + `modDone`; assert in the trace that (a) each layer's first on-frame is
  monotonic in `prog`, (b) no layer toggles on a bar boundary specifically (no
  seam), (c) the modulation fires exactly once, at `modAt`.
- **Sound tripwire** — `soundcheck`-style headless run, silence = PASS (the
  organ drone is held voices; watch the voice count doesn't leak).
- **mobile-lint** — radios rank touch-ready via the `rad_knob_*` widgets; if the
  jam strip is included it's touch-native too.

## Open questions (decide at build time)

- Tempo band (132–150?) and whether the feel knob also nudges tempo.
- Layer count — the table lists 6; is the topline lead worth it or does the
  drone+planing carry it?
- Kit: live-feel acoustic kit is the call (NOT `cr78.c` — wrong era); open
  question is just how understated the snare/hat sit.
- `modIv` set — {+2,+3,+5,+7} or narrower (krautrock often just goes up a step).
- Face: autobahn vs op-art sleeve.
- Include the `solo.h` jam strip? (Cheap, and the unchanging scale makes it the
  most foolproof jam on the dial.)
