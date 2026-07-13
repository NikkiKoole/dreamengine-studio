# tombola — the OP-1 slice: a physics sequencer you drop notes into

STATUS: EXPLORING (2026-07-13) — brainstormed with the maker. The concrete cart chosen from the
"cheap-OP-1 / just-noodle" demand gap (demand-discovery [note 022](../field-notes/022-demand-discovery-ipadmusic.md)
+ [023](../field-notes/023-demand-discovery-synthesizers.md)), laid onto the
[device-face paradigm](device-face-paradigm.md) and grounded in the
[second north star](second-north-star.md). Not built — this is the paper design + the case for it.
Working name **tombola** (the OP-1's own name for the module; a generic raffle-drum word — final
name open).

## Why this cart (the demand)

Two independently-mined music tribes converged on the same on-grain opening (notes 022/023): a
**cheap, playful, beginner lo-fi toy** — *"an app where I can just fuck around, switch instruments"*
(r/ipadmusic) and *"a cheap OP-1 for someone who knows nothing"* (r/synthesizers). The loud demand
(MIDI routing, stem separation, DAW features) is off-grain pro utility; **this** is the part a humble
cart does better than a DAW ever will. The tombola is the sharpest single expression of it.

## What we take from the OP-1 — the gift, not the baggage

Per the paradigm's [gift-vs-baggage rule](device-face-paradigm.md) (§2):

- **Gift (keep):** the four **colored encoders** (the purest "few knobs, deep by context" ever
  shipped), the **characterful, animated screen** (each module is its own little world), the
  **playful/emergent modules** — and the tombola *is* the OP-1's most-loved of those.
- **Baggage (drop):** the OP-1 is *cryptic to learn* (menu-diving, hidden hold-combos) and its screen
  **can't be touched**. The demand quote — *"for someone who knows nothing"* — is an instruction:
  keep the delight, throw out the learning curve. Lead with legible delight (the
  [ADR-0022](../decisions/0022-collaboration-is-the-north-star.md) stranger bar). And *make the
  screen touchable* — the one constraint we don't inherit.

## Why it's a second-north-star cart (not just a toy)

The tombola lines up with [the north star](second-north-star.md)'s "one idea, four times" almost
point-for-point — which is the real argument for building *this* slice over a generic synth:

- **One honest core, never faked.** A note fires **because a ball physically crossed the trigger
  line** — the same physics that draws the ball computes the hit. There is *no* hidden step-sequencer
  underneath. (This is verifiable: a `spec()` can assert every note-on corresponds to a collision
  event — the gate the north star wants.)
- **Emergence over authored content.** You author the *rules* (where balls sit, spin, gravity,
  scale) and the *pattern* falls out — never a written sequence. Squarely the maker's
  prefer-emergent grain and the "author the rules, then let the system judge you" spine.
- **An honest readout from the same code the sim runs.** What you *see* (balls approaching the line)
  *is* what you'll *hear* — no separate visualiser. In BUILD (below) we ghost the predicted
  trajectories from the identical physics, orbit-style: *the setup tells the truth before you commit.*
- **The second face is the spine** (§ below). Every legendary cart "comes alive the moment it gets
  its other face"; the tombola's is **BUILD ↔ SPIN**.
- **Documentary / homage.** It models something real and beloved (Teenage Engineering's tombola) —
  the tribute impulse the north star calls the engine's soul.

## The one honest core

> A ring of note-**balls** tumbles inside a slowly **rotating** container under **gravity**. When a
> ball crosses the **trigger line**, its note sounds. That's the whole instrument. Music is the
> *emergent* consequence of physics + where you placed the balls — nothing is sequenced.

## The second face — BUILD ↔ SPIN (the mode flip)

The north star: a one-faced thing is watchable; its second face makes it *playable with consequence*.
The tombola's flip lives in the nav spine (the paradigm's "the spine focuses/flips a face"):

- **SPIN (perform):** the drum turns, balls fall and trigger, the loop emerges and evolves. You ride
  the knobs, drop balls live, hear it change. The 95%-is-the-loop face.
- **BUILD (author):** freeze the spin. Place/tune balls precisely, set the drum's tilt and gaps, and
  see **ghost trajectory arcs** predicted from the same physics (the honest readout — where each
  ball *will* fall). Then release into SPIN and let the system judge your setup.

BUILD↔SPIN is `orbit`'s ship↔prediction and `sloop`'s build↔drive, in miniature: author the rules in
one face, watch them judged in the other.

## Onto the device-face — and the zones we DON'T need

The paradigm's five zones are a **template, not a checklist**: an instrument fills the slots it needs
and *omits the rest* (the new §2 principle this cart motivated — see [device-face-paradigm.md](device-face-paradigm.md)).
The tombola has **no step grid**, so **zone 4 simply isn't there.** Fewer visible controls is the
feature (Hick's Law), not a gap.

```
┌────────────────────────────────────────────────────┐
│ ▶ SPIN / ❚❚ BUILD    tombola   C min-pent   96 BPM   │ ① NAV SPINE — the BUILD↔SPIN flip + tempo/scale
├────────────────────────────────────────────────────┤
│  (SPIN) (GRAV) (SCALE) (GATE)                        │ ② FOUR ENCODERS — the OP-1 signature; ride live
├──────┬───────────────────────────────────┬──────────┤
│ voice│         ◜  ●   ○      ◝            │ CLR      │ ③ DISPLAY = THE DRUM (the hero, PERFORMED-on):
│ (bass│      ○      the spinning drum ●    │ RND      │   touchable — tap inside to drop a ball, flick
│  lead│   ●     ·─── trigger line ───· ●   │ SEED     │   one out; in-screen ops; BUILD shows ghost arcs
│  perc│         ◟   ○      ●   ◞           │          │
├──────┴───────────────────────────────────┴──────────┤
│                (zone 4 — none: no steps to toggle)   │ ④ DROPPED — the "omit what you don't need" case
├──────────────────────────────────────────────────────┤
│  A W S E D F T G Y H U J   (pick the next ball's note)│ ⑤ KEYBED — a SELECTOR (chooses, doesn't sound)
└────────────────────────────────────────────────────┘
```

- **① Nav spine** — the **BUILD↔SPIN** flip (this cart's second face), tempo, scale/key, seed, preset.
- **② Four encoders** — SPIN speed · GRAVITY · SCALE/range · GATE/decay. Contextual (paradigm p-lock
  rule): with a **ball selected**, the same four edit *that ball* (pitch, velocity, size/mass).
- **③ Display = the drum** — the **hero surface, performed upon** (the chordblossom "zone 3 can be
  *played*" rule; tombola is its second proof). Touchable: tap to drop, flick to remove, drag to
  reposition. Contextual ops live *in* the screen (CLR / RND / SEED). In BUILD it renders the ghost
  trajectories.
- **④ — nothing.** No step row. This is the cart the paradigm needed to state "omit unneeded zones."
- **⑤ Keybed** — a **selector**, not a performer (paradigm's selector-vs-performer test): it *chooses*
  the pitch of the ball you're about to drop; the drum *makes* the sound. (Optional: a slim
  voice-picker — bass/lead/perc — could ride the display's left flank rather than becoming a zone-4
  grid.)

## Scope discipline (one honest core — not "a whole OP-1")

A faithful *whole* OP-1 (synth + sampler + tape + sequencer + mixer) breaks "one cart = one honest
core" and is far too big. The whole pocket-studio is a **Tiny-Jam-style multi-rack app**, later — not
this cart. Slices considered, and why the tombola won (the device-level version of this table — OP-1
module → repo status — is in [`device-face-paradigm.md`](device-face-paradigm.md) §1f; keep the two in sync):

| slice | verdict |
|---|---|
| **tombola** (physics sequencer) | **chosen — the one genuine gap** (see prior art below). One honest core, emergent, hero played-display, legible to a stranger, distinctly OP-1 |
| the **tape** 4-track looper | **already built — `loopstation`** ("the first cart that records itself": 4 tracks share one loop, arm/play/layer). Don't rebuild; extend it if the OP-1 *tape metaphor* (scrub/reverse/reel) is wanted |
| four-encoder **character synth** (animated screen per engine) | later — hero gesture is weak ("play + watch"), overlaps the synth shelf; only the animated-character screen is novel |

## Prior art in the repo — reuse, and what's already covered

The cart-check (2026-07-13, the "verify by reading" pass) found the tombola is the *only* genuinely
missing core of the three, and that we have parts to build it from:

- **`circlemachine`** (Raymond Scott's Circle Machine) — the nearest cousin: a **rotating** arm sweeps
  rings of bulbs, each bulb's brightness = its note, scale-quantised. Rotational note-triggering +
  scale-lock + synth wiring, ready to borrow. *But it's a fixed-position step sequencer, not physics*
  — no gravity, no emergence-from-collision. The tombola's honest core (notes from *real falling-ball
  collisions*) is what it doesn't have, and that's the whole point.
- **`runtime/physics.h`** (shipped in parallel, 2026-07-13) — the shared **verlet** toolkit (points
  with inverse-mass + collision radius, links, integrate/relax); its demo `verlet.c` is literally *"a
  pile of bouncy balls."* This is the tombola's ball engine — **build the balls on `physics.h`, don't
  copy `pinball`'s inline math.** The tombola only adds a rotating container + the trigger-line hit
  test on top. (`pinball` stays a useful *wall-bounce* reference; `marble` also has verlet but no audio.)
- **`circlemachine`** gives the other half: the **scale-quantised note/synth wiring** to fire when a
  ball crosses the line. So the tombola = **`physics.h` balls** in a rotating drum driving
  **`circlemachine`'s note wiring** — "copy the closest relative" per
  [`../guides/instrument-carts.md`](../guides/instrument-carts.md), not a from-scratch build.

Already-served OP-1 cousins (don't rebuild): **`loopstation`** = the tape looper; **`chordblossom`** =
the chord-play toy (the note-022 chord gap is *already met*). This is why the tombola is the single new
experiment worth running, not a batch.

## Open questions / next steps

- **Container shape** — a circle (pure), a polygon (bounces are more characterful + more chaotic), or
  tiltable? The polygon is more OP-1 and more emergent; decide on feel.
- **How many voices** — one voice (purest one-core) vs a few (bass/lead/perc balls). Lean minimal
  first; a voice-picker as a display flow, never a step grid.
- **BUILD's ghost prediction** — how far ahead to draw the arcs, and whether they update live as you
  drag a ball (the honest-readout payoff is strongest if they do).
- **Scale-lock** — quantise ball pitches to a scale so "someone who knows nothing" can't play a wrong
  note (the legibility bar). Almost certainly yes.
- **The screen's character** — the OP-1's soul is a screen with *personality*; what's the tombola's
  look and motion feel? (The delightful-to-a-stranger half of the bar; no oracle checks it.)
- **`spec()` for the honest core** — assert note-ons ↔ collision events (proves nothing is faked).
- **Emergent depth later** — per-ball conditions/probability, ball collisions retriggering, gravity
  gradients — all fall out of the physics, no new chrome (the "conditional trigs are emergent" idea
  in the paradigm's §5).

## Related

[`device-face-paradigm.md`](device-face-paradigm.md) (the face shape this fills — and the
"omit unneeded zones" principle this cart motivated) · [`second-north-star.md`](second-north-star.md)
(one honest core / emergence / the second face) · [`demand-discovery.md`](demand-discovery.md) +
field notes [022](../field-notes/022-demand-discovery-ipadmusic.md) /
[023](../field-notes/023-demand-discovery-synthesizers.md) (where the demand came from) ·
[`../decisions/0022-collaboration-is-the-north-star.md`](../decisions/0022-collaboration-is-the-north-star.md)
(the two-part bar) · [`../guides/instrument-carts.md`](../guides/instrument-carts.md) (pick the closest
chassis when building) · [`../../runtime/ui.h`](../../runtime/ui.h) (`ui_knob` for zone 2) +
[`../../runtime/keybed.h`](../../runtime/keybed.h) (zone 5) + [`../../runtime/pointer.h`](../../runtime/pointer.h)
(touching the drum).
