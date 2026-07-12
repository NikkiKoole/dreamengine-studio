#!/usr/bin/env node
'use strict'
// voice-trace.js — read the voice-allocation events a --trace run records and answer
// "which voice stopped, and why". The reader half of docs/design/audio-voice-debugging.md;
// its twin (the stem render) is `play.js --solo-slot`.
//
// The engine (runtime/sound.h, DE_TRACE-only) emits one JSONL line per voice lifecycle
// event into the --trace file — every play.js run writes one, so the events are always
// there for an audio cart:
//   {"vev":"on|off|reuse|steal|choke","f":<frame>,"slot":<instr slot>,"midi":<note>,"voice":<idx>,"victim":<slot|handle>}
// A voice can go silent three ways that count as "cut off by another instrument":
//   reuse  — a note_on reused a handle slot, releasing the held voice on it
//   steal  — polyphony (32 voices) ran out; the quietest voice was taken
//   choke  — a new note's choke group silenced this voice (closed hat cuts open hat, …)
// `on`/`off` are the clean lifecycle (off = a real note_off, not a cut).
//
// Usage:
//   node tools/voice-trace.js <trace.jsonl>            # summary + every cut, in order
//   node tools/voice-trace.js <trace.jsonl> --slot 6   # only events touching instrument slot 6
//   node tools/voice-trace.js <trace.jsonl> --cuts     # ONLY the cuts (reuse/steal/choke)
// Get a trace with:  node tools/play.js <cart> run --headless --frames <n> --trace <trace.jsonl>
// (or any play.js run — it always passes --trace to build/<cart>.trace.jsonl)

const fs = require('fs')

const args = process.argv.slice(2)
const file = args.find(a => !a.startsWith('--'))
const onlySlot = args.includes('--slot') ? parseInt(args[args.indexOf('--slot') + 1], 10) : null
const cutsOnly = args.includes('--cuts')
if (!file) { console.error('usage: node tools/voice-trace.js <trace.jsonl> [--slot N] [--cuts]'); process.exit(2) }

const CUT = new Set(['reuse', 'steal', 'choke'])
const events = []
for (const line of fs.readFileSync(file, 'utf8').split('\n')) {
  if (!line.includes('"vev"')) continue
  let e; try { e = JSON.parse(line) } catch { continue }
  if (onlySlot != null && e.slot !== onlySlot && e.victim !== onlySlot) continue
  events.push(e)
}

if (!events.length) {
  console.log(onlySlot != null
    ? `No voice events touching slot ${onlySlot} in ${file}.`
    : `No voice events in ${file} — the cart made no sound (or wasn't built with -DDE_TRACE).`)
  process.exit(0)
}

// histogram
const hist = {}
for (const e of events) hist[e.vev] = (hist[e.vev] || 0) + 1
const order = ['on', 'off', 'reuse', 'steal', 'choke']
console.log(`voice-trace: ${events.length} events in ${file}`)
console.log('  ' + order.filter(k => hist[k]).map(k => `${k} ${hist[k]}`).join('   '))

// per-slot rollup: notes started, and cuts SUFFERED (this slot was the victim)
const bySlot = {}
for (const e of events) {
  const s = (bySlot[e.slot] ||= { on: 0, victimOf: 0 })
  if (e.vev === 'on') s.on++
  if (CUT.has(e.vev)) (bySlot[e.slot] ||= { on: 0, victimOf: 0 }).victimOf++  // slot field on a cut = the victim
}
console.log('\nper instrument slot:')
for (const s of Object.keys(bySlot).map(Number).sort((a, b) => a - b)) {
  const r = bySlot[s]
  console.log(`  slot ${String(s).padStart(2)}  ${String(r.on).padStart(4)} notes` +
    (r.victimOf ? `   ⚠ cut ${r.victimOf}×` : ''))
}

// the cuts, in order — the smoking gun for "the solo got cut off"
const cuts = events.filter(e => CUT.has(e.vev))
if (cuts.length) {
  console.log(`\ncuts (${cuts.length}) — a voice silenced by another note:`)
  for (const e of cuts) {
    const how = e.vev === 'choke' ? `choked by a note on slot ${e.victim}`
      : e.vev === 'reuse' ? `handle slot ${e.victim} reused (note_on)`
      : `polyphony exhausted (stolen)`
    console.log(`  f${String(e.f).padStart(5)}  slot ${String(e.slot).padStart(2)} (voice ${e.voice}) — ${e.vev}: ${how}`)
  }
} else if (!cutsOnly) {
  console.log('\nNo engine-level cuts (no steal / reuse / choke).')
  console.log('A voice that still "disappears" is then either cart-level sequencing (the cart')
  console.log('stops/retriggers it) or perceptual masking under the mix — render the part alone')
  console.log('with:  node tools/play.js <cart> run --wav out.wav --solo-slot <slot>')
}
