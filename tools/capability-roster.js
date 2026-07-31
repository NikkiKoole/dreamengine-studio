// ============================================================================
// capability-roster.js — (lib, not CLI) the shared list of ENGINE CAPABILITIES,
// each pinned to the studio.h symbol that proves we ship it.
//
// Owned here, used by lint-capability-claims.js (docs that DENY a shipped
// capability) and wants-check.js (wants docs whose blockers already shipped).
// Same lib-not-CLI shape as doc-status.js.
//
// ONE roster, because two copies drift and the drift is invisible: a capability
// missing from one list makes that tool quietly blind, which is the exact failure
// mode both of these tools exist to catch.
//
// Fields:
//   cap       display name, and the key both tools report by
//   proof     a studio.h declaration (`void <proof>(`) that PROVES we ship it.
//             No declaration → the capability leaves the roster automatically,
//             so a doc saying "no octaver" is never flagged (it is TRUE).
//   words     how PROSE names it, for matching a doc's claims/table cells
//   wire      the call sites that count as a CART HAVING WIRED IT (defaults to
//             [proof] + instrument_<proof>). Several capabilities ship more than
//             one way — compression is glue/multiband/sidechain — and a cart that
//             used any of them has wired it.
//   ambiguous the word has a common non-audio meaning in this repo, so a claim
//             only counts with a qualifier noun. See lint-capability-claims §3.
// ============================================================================

const CAPS = [
  { cap: "reverb",     proof: "reverb",           words: ["reverb"],
    wire: ["reverb", "instrument_reverb", "reverb_bus", "reverb_insert", "reverb_spring"] },
  { cap: "chorus",     proof: "chorus",           words: ["chorus"] },
  { cap: "flanger",    proof: "flanger",          words: ["flanger"] },
  { cap: "phaser",     proof: "phaser",           words: ["phaser"] },
  { cap: "tremolo",    proof: "tremolo",          words: ["tremolo"] },
  { cap: "leslie",     proof: "leslie",           words: ["leslie", "rotary speaker"] },
  { cap: "univibe",    proof: "univibe",          words: ["univibe", "uni-vibe"] },
  { cap: "formant",    proof: "formant",          words: ["formant filter", "vowel filter"] },
  { cap: "vocoder",    proof: "vocoder",          words: ["vocoder"], wire: ["vocoder", "vocoder_send"] },
  { cap: "ring mod",   proof: "ringmod",          words: ["ring mod", "ringmod", "ring modulator"] },
  { cap: "auto-pan",   proof: "autopan",          words: ["auto-pan", "autopan"] },
  { cap: "multiband",  proof: "multiband",        words: ["multiband"] },
  { cap: "shimmer",    proof: "shimmer",          words: ["shimmer reverb"] },
  { cap: "bitcrush",   proof: "crush",            words: ["bitcrush", "bit crush", "bitcrusher"] },
  { cap: "granular",   proof: "grains",           words: ["granular"] },
  { cap: "sidechain",  proof: "sidechain",        words: ["sidechain", "side-chain"],
    wire: ["sidechain", "sidechain_key"] },
  { cap: "varispeed",  proof: "varispeed",        words: ["varispeed"] },
  { cap: "sampler",    proof: "sample_record",    words: ["sampler"],
    wire: ["sample_record", "sample_load", "instrument_sample"] },
  { cap: "pitch-shift",proof: "sample_shift",     words: ["pitch-shift", "pitch shifter"] },
  { cap: "mic input",  proof: "mic_level",        words: ["mic input", "microphone input", "audio input"] },
  // ambiguous: bare "no gate"/"no filter" are usually about something else entirely
  { cap: "gate",        proof: "gate",            words: ["gate"],        ambiguous: true },
  { cap: "filter",      proof: "filter",          words: ["filter"],      ambiguous: true },
  { cap: "drive",       proof: "instrument_drive",words: ["drive", "overdrive"], ambiguous: true,
    wire: ["instrument_drive", "drive_insert", "note_drive"] },
  { cap: "echo/delay",  proof: "echo",            words: ["echo", "delay"], ambiguous: true,
    wire: ["echo", "instrument_echo", "echo_insert", "note_echo"] },
  { cap: "EQ",          proof: "eq",              words: ["eq"],          ambiguous: true },
  { cap: "tape",        proof: "tape",            words: ["tape"],        ambiguous: true },
  { cap: "shallow",     proof: "shallow",         words: ["shallow"],     ambiguous: true },
  // compression ships THREE ways; any of them counts as wired
  { cap: "compression", proof: "glue",            words: ["compression", "compressor"], ambiguous: true,
    wire: ["glue", "multiband", "sidechain"] },
  // "no wah" is nearly always a TEST CONDITION here ("an FFT of both (middle C, no wah)"),
  // not a claim we lack the pedal — so it needs the qualifier like the other ambiguous words.
  { cap: "auto-wah",    proof: "wah",             words: ["auto-wah", "autowah", "wah"], ambiguous: true,
    wire: ["wah", "instrument_wah", "wah_lfo"] },
];

// a capability is in the roster iff studio.h declares its proof symbol
const shipsIn = (studioSrc, sym) =>
  new RegExp(`^\\s*(?:void|float|int|const char\\s*\\*)\\s+${sym}\\s*\\(`, "m").test(studioSrc);

// the call sites that mean "this cart wired it"
const wireSymbols = (c) => c.wire || [c.proof, `instrument_${c.proof}`];

module.exports = { CAPS, shipsIn, wireSymbols };
