// teaches-vocab.js — the single source of truth for the TEACHES vocabulary: the
// CONCEPTUAL techniques a cart teaches (the axis the API-call families can't reveal).
// Required by cart-index.js (--lint / index), build-compendium.js, and lint-carts.js
// (which validates index.json's teaches[] against it) — one list so it can't drift.
//
// Grown like lint-carts.js's KINDS: add a tag here when a cart genuinely needs one.
// A tag not listed is a soft warning (catches typos + prompts a deliberate add).
//
// teaches[] + lineage live IN editor/public/carts/index.json, per cart (alongside
// kind/genre) — NOT in a separate file, so they can't drift from the registry.

module.exports = {
  TEACHES_VOCAB: [
    // procgen / world
    'gene-based-procgen', 'wave-function-collapse', 'marching-squares', 'autotiling', 'noise-terrain',
    'maze-generation', 'dungeon-generation', 'l-system', 'cellular-automata',
    // roads / civil geometry
    'road-network', 'at-grade-junction',
    // agents / ai
    'state-machine', 'finite-state-ai', 'pathfinding', 'flocking', 'car-following', 'steering-behaviors',
    'traffic-sim', 'schedule-driven-agents',
    // physics
    'verlet-integration', 'spring-damper', 'rigid-body', 'particle-system', 'fluid-sim', 'soft-body',
    'segment-collision',
    // rendering
    'raycasting', 'mode7', 'parallax', 'camera-follow', 'dithering-gradient', 'palette-cycling',
    'software-rasterizer', 'procedural-mesh', 'sprite-stacking', 'no-sprite-vehicles', 'radial-symmetry',
    'isometric-projection',
    // game structure
    'title-play-gameover-loop', 'tilemap-collision', 'inventory-system', 'dialogue-tree', 'turn-based-loop',
    'grid-movement', 'save-load-persistence', 'screen-shake-juice', 'genetic-crossover', 'algorithm-visualization',
    // audio
    'subtractive-synth', 'granular-synth', 'waveguide-synth', 'fm-synth', 'additive-synth', 'step-sequencer',
    'euclidean-rhythm', 'adsr-envelope', 'generative-melody', 'chord-voicing', 'drum-synthesis',
    'analog-voice-modeling', 'swing-timing', 'sonification', 'audio-occlusion', 'positional-audio',
    'wavetable-drawing',
    // synthesis / sequencer / logic concepts from carts another agent tagged via .c
    // headers; normalised to the reusable ones (granular/one-off coinages dropped or
    // folded: vactrol-envelope + coupled-vol-cutoff → lowpass-gate, etc).
    'self-oscillation', 'generative-sequencer', 'polymeter', 'lowpass-gate', 'wavefolder',
    'west-coast-synthesis', 'combinational-logic', 'sequential-logic', 'shift-register',
    'scale-quantize',
  ],
};
