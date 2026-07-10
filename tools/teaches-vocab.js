// teaches-vocab.js — the single source of truth for the TEACHES vocabulary: the
// CONCEPTUAL techniques a cart teaches (the axis the API-call families can't reveal).
//
// The vocabulary is defined ONCE here, grouped by CATEGORY (with the label + colour the
// ★ techniques compendium uses). Everything else derives from it:
//   • TEACHES_VOCAB (flat list) — used by lint-carts.js (hard-validates index.json's
//     teaches[]) and cart-index.js.
//   • CATEGORIES (group → label/colour/tags) — used by build-compendium.js to colour and
//     group the page. No second category map to drift out of sync.
//
// To add a tag: put it under the right category below. That single edit makes it valid
// (lint), grouped, and coloured (compendium) everywhere at once. Adding one is a
// deliberate, reviewable change — an off-vocabulary tag is a hard lint error, so the list
// grows on purpose, not by casual coinage.
//
// teaches[] + lineage live IN editor/public/carts/index.json, per cart (alongside
// kind/genre) — NOT in a separate file, so they can't drift from the registry.

const CATEGORIES = {
  world: {
    label: 'World & procgen', color: '#00E436',
    tags: [
      'gene-based-procgen', 'wave-function-collapse', 'marching-squares', 'autotiling', 'noise-terrain',
      'maze-generation', 'dungeon-generation', 'l-system', 'cellular-automata',
      'road-network', 'at-grade-junction',
    ],
  },
  ai: {
    label: 'Agents & AI', color: '#FFA300',
    tags: [
      'state-machine', 'finite-state-ai', 'pathfinding', 'flocking', 'car-following', 'steering-behaviors',
      'traffic-sim', 'schedule-driven-agents', 'combinational-logic', 'sequential-logic',
    ],
  },
  physics: {
    label: 'Physics', color: '#29ADFF',
    tags: [
      'verlet-integration', 'spring-damper', 'rigid-body', 'particle-system', 'fluid-sim', 'soft-body',
      'segment-collision',
    ],
  },
  render: {
    label: 'Rendering', color: '#FF77A8',
    tags: [
      'raycasting', 'mode7', 'parallax', 'camera-follow', 'viewport-clipping', 'dithering-gradient', 'palette-cycling',
      'software-rasterizer', 'procedural-mesh', 'sprite-stacking', 'no-sprite-vehicles', 'radial-symmetry',
      'isometric-projection', 'voronoi', 'velocity-brush', 'blend-modes',
    ],
  },
  game: {
    label: 'Game structure', color: '#FFEC27',
    tags: [
      'title-play-gameover-loop', 'tilemap-collision', 'inventory-system', 'dialogue-tree', 'turn-based-loop',
      'grid-movement', 'save-load-persistence', 'screen-shake-juice', 'genetic-crossover', 'algorithm-visualization',
      'collision-detection', 'factory-automation',
    ],
  },
  audio: {
    label: 'Audio', color: '#FF004D',
    tags: [
      'subtractive-synth', 'granular-synth', 'waveguide-synth', 'fm-synth', 'additive-synth', 'step-sequencer',
      'euclidean-rhythm', 'adsr-envelope', 'generative-melody', 'chord-voicing', 'drum-synthesis',
      'analog-voice-modeling', 'swing-timing', 'sonification', 'audio-occlusion', 'positional-audio',
      'wavetable-drawing', 'self-oscillation', 'generative-sequencer', 'polymeter', 'lowpass-gate', 'wavefolder',
      'west-coast-synthesis', 'shift-register', 'scale-quantize', 'per-instrument-fx-bus',
      'gesture-loop', 'song-arrangement', 'motion-sequencing',
    ],
  },
};

const TEACHES_VOCAB = Object.values(CATEGORIES).flatMap(c => c.tags);

module.exports = { CATEGORIES, TEACHES_VOCAB };
