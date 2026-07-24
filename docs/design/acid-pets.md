# Acid pets — a mascot-per-instrument that IS the readout

STATUS: EXPLORING (2026-07-17) — prototyped in [`acidpets`](../../tools/carts/acidpets.c), a draw-only
vibe mockup for [`acidcandy`](../../tools/carts/acidcandy.c). This doc is the mapping spec the mockup (and,
if it graduates, the real cart's FACE flow) is built against. It answers acidcandy's parked SOUL todo
("the LCD screens have no MASCOT yet"). Part of the [device-face](device-face-paradigm.md) collection.

> **The idea.** Every machine on the rack gets a little creature, and the creature is a **live readout of
> that machine's whole state** — not decoration. Two 303s dialled differently read as two different-tempered
> animals; a muted machine sleeps; the sidechain pump is a literal heartbeat. If you can't tell the patch
> from the pet, the mapping is wrong. The nav strip becomes a **pet shelf** — five tiny critters always on
> screen — so you glance at the row and read the whole track.

## Why a mascot at all

acidcandy's LCD screens are honest but silent — they show the *data*, not the *feel*. A creature turns the
data into a face: the beginner-critic ([ADR-0022](../decisions/0022-collaboration-is-the-north-star.md)) can
read "the acid line is going feral, the mix is pumping hard" from a glance at animals, with no numbers. It is
**disclosure, not amputation** — the pet is a second, emotional view of the same state the panels already hold.

## The model — three kinds of mapping

Every visual on a pet is exactly one of:

- **STATE** — a knob's *value* → a persistent look (posture, proportion, colour). Turn it, the look holds.
  This is the honest-readout layer.
- **EVENT** — a note/hit *fires* → a momentary motion that decays. Beat-locked.
- **MODE** — a toggle → a transform (Devil Fish, mute, waveshape).

And one **global layer**, identical on all five:

| source | kind | response |
|---|---|---|
| the beat (16th) | global | the boiled-outline **wobble frame advances on the 16th** — the squishy "boil on the BPM"; the whole shelf shivers in time |
| idle | global | slow breathing that rides the tempo |
| machine mute | mode | pet **curls asleep** (dimmed, `zZ`); still ghost-twitches on its would-be hits (mirrors acidcandy's ghost-pulse) |
| focus | mode | drawn big in the **vivarium**; the others stay tiny on the shelf |
| overall activity (`energy`) | state | liveliness — bob height, how "awake" it looks |
| accented step | event | a sharper flinch than a plain hit |

Look = STATE (slow) + EVENT (fast, decaying) + MODE (transform), all drawn in the squishy house style:
**boiled hand-drawn outlines + a capped candy bevel rim** (see [`squishy`](../../tools/carts/squishy.c) for the
boil/bevel technique this borrows).

## Legibility rule (learned at 32px)

The pets live in cartridge-size slots. Tested on a size ladder (64 / 48 / 32 / 24 px):

- **Survives shrinking:** a single strong **silhouette + one or two big features** (the puffer's round face,
  the heart's outline). ✓ to 24px.
- **Dies:** **thin appendages** — the first 909's legs/antennae vanished and it collapsed into a spiky blob.
- **Borderline:** the 303 — its fin/tail steal height from the body that carries the "fish" read; clean ≥32px.

So: **design silhouette-first, carry reactions on the body, not on limbs.** Target 32px; 64px is the ceiling.

## The five creatures

### 303 — the acid SMILEY that morphs to a devil FISH

The 303 has **two faces**, switched by the Devil Fish mode:

- **Vanilla = the acid-house smiley** — the icon of 303 culture, and the strongest 32px silhouette (a round
  face + eyes + grin). This is the default form.
- **Devil Fish = the feral fish** — arming DF *morphs* the smiley into a red, fanged, finned fish (the literal
  *devil fish* pun). The fish is the reward for going feral, not the base form.

303a & 303b are the SAME species (one instrument; only `ac[1].base = 48` transposes 303b up an octave — in
ReBirth the two 303s are fully identical, octave is per-step, tune is a fine knob). Any difference **emerges**
from state (303b floats higher because its pitches are higher), never hardcoded.

**STATE:** CUT → eyes open (bright = wide) · RES → whole face jelly-wobble · ENV → how much a hit brightens ·
DEC → how long the squash holds · pitch → vertical height (bob).
**EVENT:** note → squash-bounce · accent → big open grin (O mouth) / bigger flinch · slide → face tilts toward
the target while gliding, eyes drift · tie → sustained stretch (no re-trigger snap).
**MODE:** Devil Fish → morph smiley→fish (red skin, fangs, spiky fin, sub-osc shadow-belly, angry brow) ·
WAVE saw/sqr → smooth vs. jagged-finned outline (fish form).

### 808 puffer / 909 bug — voice FAMILIES, not 16 voices

A drum machine has 16 (808) / 11 (909) voices; one creature can't show each. Bucket the roster into **event
families** so the pet has 3–4 distinct reactions, then overlay continuous modifiers. Each drum pet tracks
**~4 event envelopes + ~5 modifier floats** — not the full roster.

| family | 808 voices | 909 voices | reaction (must read *distinct* — different axes) |
|---|---|---|---|
| **KICK** | BD | BD | belly **squash + drop** — vertical |
| **SNARE** | SD, RS, CP, CLV | SD, RS, CP | sharp **jab / shudder** + eyes clamp — horizontal |
| **HATS** | CH, OH, CY | CH, OH, CC, RC | quick **top shiver / ears flick** — high-frequency |
| **PERC** *(opt 4th)* | LT/MT/HT, LC/MC/HC, MA, CB | LT/MT/HT | **cheek / tilt wiggle** — mid |

An envelope per family = max/sum of its members' triggers that step.

Continuous **modifiers** (shape the whole body, machine-aggregate so one creature can show them):

| modifier | source | look |
|---|---|---|
| TUNE | aggregate / kick tune | body **size** (low = fatter) |
| DECAY | aggregate | how long a thump **holds** |
| DRIVE / DIST | machine | **grit** — jaggeder outline, redder tint |
| SWING | machine | idle **sway / lilt** |
| METAL (909) | metal XY | outline **jaggedness** + eye glow |
| FLAM / STROKE (909) | per-hit | **stutter afterimage** on that hit |

- **808 puffer:** round warm face; KICK drops the belly, SNARE shudders + clamps the eyes, HATS flick the
  ears + shiver the top, PERC pulses the cheeks. TUNE=size, DRIVE=grit tint, SWING=sway.
- **909 bug** *(silhouette-first redesign):* a chunky beetle carapace + two big amber eyes, **no leg/antenna
  detail**. "Electric/metallic" comes from a **jagged outline** (raised by METAL) + eye glow, not radiating
  lines. KICK=vertical squash-pop, SNARE=horizontal jab-lurch, HATS=micro-buzz, PERC=tilt, FLAM=afterimage.

### MST — the beating heart (the whole mix)

**STATE:** PUMP → how hard it **ducks** on the kick (sidechain made visible) · FLT → **squint** as the DJ
filter closes off-centre · GLU → **arms hug** inward · FB → **echo rings** pulse out · mix energy → reddens.
**EVENT:** any machine's kick → the heart **beats** (quick duck + recover).

## Open items

- **Shelf-vs-vivarium simplification** — one renderer at all sizes (current), or drop fin/cheeks/rings below
  ~24px and let pure silhouette + eyes carry it?
- **Drum PERC family** — worth the 4th input, or is KICK/SNARE/HATS enough character?
- **Graduation path** — if it earns its keep, this becomes a FACE soft-key flow in acidcandy (a pet per
  focused machine, or the shelf woven into the nav cartridges). Kept as a separate mockup cart until then.

## The building blocks (prototyped as separate labs)

- [`petkit`](../../tools/carts/petkit.c) — the **Shape atom**: one parametric primitive (circle/oval/rect/rrect/poly)
  with every look as a scalar dial — w/h, squash-stretch, boil, bevel, thick squishy outline, dpaint dither, a
  two-colour gradient (angle/offset/band), fill + edge colours. A Mr-Men body is one of these; personality is
  its dials driven by state. All procedural — nothing is drawn by hand.
- [`peteyes`](../../tools/carts/peteyes.c) — the **eye emotion machine**: a face + two eyes on scalar levers
  (openness, squint, lid-tilt = angry/sad, pupil size, gaze X/Y, brow raise + angle, glint). Eyes + brows carry
  most of a character's feeling; happy is an ∩ laughing arc, not a slit. 8 emotion presets prove the range.

Both are pure scalar params, so a character = a few Shapes + a pair of eyes with their dials **hooked to the
machine's state** (a note-envelope → squash, a filter → gradient offset, the mix mood → the eye levers).

## See also

- [device-face-paradigm.md](device-face-paradigm.md) — the UI shape acidcandy (and the pet shelf) sits on.
- [`squishy`](../../tools/carts/squishy.c) — the boil + bevel rendering technique.
- acidcandy's `de:meta` SOUL todo — the parked mascot idea this spec picks up.
- [`bossa-rack.md`](bossa-rack.md) — the sibling chord-bloom rack whose "band follows" pattern the pets extend.
