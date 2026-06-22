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
    // harvested from carts another agent tagged via .c headers before we centralised on
    // index.json. Kept verbatim so their work isn't lost; some are granular (no-engine-dsp,
    // nested-ring-ui, held-voice-fx-sends, coupled-vol-cutoff) — candidates to normalise.
    'self-oscillation', 'feedback-throw', 'pitch-lfo', 'held-voice-fx-sends', 'gesture-instrument',
    'generative-sequencer', 'polymeter', 'nested-ring-ui', 'lowpass-gate', 'vactrol-envelope',
    'coupled-vol-cutoff', 'wavefolder', 'west-coast-synthesis', 'polyphonic-voice-pool',
    'combinational-logic', 'sequential-logic', 'signal-propagation', 'shift-register', 'probability',
    'scale-quantize', 'no-engine-dsp',
  ],
};
