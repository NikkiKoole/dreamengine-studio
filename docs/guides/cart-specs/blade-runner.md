# The game you're building: BLADE RUNNER

## Premise
A noir point-and-click adventure. You're a detective in a rain-soaked neon city working
a single case: visit scenes, examine clues, question characters through dialogue trees,
collect evidence in your inventory, and reach a deduction. Carried by **atmosphere** —
rain, neon, synth haze — more than mechanics.

## Slice (locked)
**One case across ~5–6 hand-built scenes**, mouse hotspots (look/use/go), an inventory,
dialogue trees with a few branches, and a final accusation/resolution. No combat, no
free movement — scene-to-scene navigation. Mood is the headline deliverable.

## Core mechanics
- **Scene graph:** each scene is a painted backdrop with **hotspots** (exits, objects,
  people). Hover highlights a hotspot + a verb/label; click to examine, take, use, or
  travel.
- **Inventory:** collected evidence/items; use an item on a hotspot (combine to progress).
- **Dialogue:** branching trees with characters; choices reveal clues and flags; a
  notebook tracks discovered facts.
- **Case logic:** flags gate progress (some scenes/dialogue unlock once you've found X);
  the finale lets you name the culprit based on evidence → win/lose ending.

## Sprites & maps
Scenes = composed pixel art: a tile `map()` or layered sprites for the backdrop
(street, apartment, bar, alley, office), characters as sprites (`pal()` for lighting/
recolor), hotspot glints. UI (verbs, inventory bar, dialogue box, notebook) as `fillp`
panels. Heavy `pal()` use for neon and time-of-day.

## Juice (atmosphere first)
**Rain** (particle streaks + puddle ripples), **neon signs** that `blink`/flicker and
tint the scene (`pal`), volumetric fog bands (`fillp` dither), a slow camera drift or
parallax, smoke curls, reflective wet streets. Dialogue text types out with blips; a
clue-found chime + notebook flourish. Audio: a brooding synth pad bed (slow-attack
instrument + lowpass), distant city/rain ambience, a saxophone-ish motif, UI clicks,
a tense sting on a key revelation.

## Data model
`struct Scene { hotspots[], exits[] }`; `struct Hotspot { rect, verb, target/flag }`;
inventory list; a `flags` bitset for progress; dialogue trees as node tables. Persist
progress with `save_bytes` (optional).

## Controls
Mouse-primary: move pointer over hotspots (label shows), left-click to act, click an
inventory item then a hotspot to use it, click dialogue options. Minimal keyboard.

## Lean into / read
`adventure.c` (scene/hotspot/inventory adventure — read it), `papers.c` (document/desk
UI + branching), `crowd.c` (pal-recolored characters), `effects.c`/`particles.c` (rain/
fog/flicker), `omnichord.c`/`22-filter.c` (the synth-noir bed). Skip: action sequences,
real-time anything, a huge world — a tight, gorgeous, moody case.
