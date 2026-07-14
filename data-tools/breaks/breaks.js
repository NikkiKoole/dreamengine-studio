#!/usr/bin/env node
// breaks.js — ingest a drum LOOP (URL or local file) into a frozen mono-PCM artifact the
// break-chopper cart loads at runtime via sample_load(). Source-AGNOSTIC on purpose: the whole
// point of the cart is "throw in your OWN loop and it gets cut correctly" — this is the desktop/
// build-time twin of the eventual on-device import (data-tools/roadview's osm-roads.js → .rvb is
// the same shape: acquire externally, freeze, the cart consumes it deterministically).
//
// ⚠ COPYRIGHT / RELEASE GATE. The amen fixture (The Winstons, "Amen, Brother", 1969) is a
// COPYRIGHTED recording — fine as a LOCAL dev placeholder, but it must NOT ship. Guardrails:
//   • artifacts land in cache/ which is gitignored (never committed, never baked into a .cart.png)
//   • the cart loads the artifact at RUNTIME and falls back gracefully if it's absent
//   • before any public release (gallery / iOS): ship no bundled audio (loops are user-supplied),
//     or swap to a CC0 / public-domain loop by re-running this tool with a clean source URL.
//
// Usage:
//   node data-tools/breaks/breaks.js <url|file> [--name amen] [--bpm 136] [--bars 4] [--sr 44100]
//
// Output (into data-tools/breaks/cache/, gitignored):
//   <name>.f32    raw little-endian float32 MONO PCM at --sr  (sample_load-ready, -1..1)
//   <name>.json   { name, samples, sampleRate, seconds, bpm, bars, source }
//
// Needs ffmpeg (already a repo dep — store-shots.js / make-gif.js use it). No node packages.

const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const argv = process.argv.slice(2);
const src = argv.find((a) => a[0] !== '-');
if (!src) {
  console.error('usage: node data-tools/breaks/breaks.js <url|file> [--name x] [--bpm n] [--bars n] [--sr n]');
  process.exit(1);
}
const flag = (name, def) => { const i = argv.indexOf(name); return i >= 0 ? argv[i + 1] : def; };
const isUrl = /^https?:\/\//i.test(src);
const name = flag('--name', path.basename(src).replace(/\.[a-z0-9]+$/i, '').replace(/[^a-z0-9_-]/gi, '_') || 'loop');
const bpm  = flag('--bpm', null);
const bars = flag('--bars', null);
const sr   = parseInt(flag('--sr', '44100'), 10);

const outDir = path.join(__dirname, 'cache');
fs.mkdirSync(outDir, { recursive: true });
const f32Path  = path.join(outDir, `${name}.f32`);
const jsonPath = path.join(outDir, `${name}.json`);

async function main() {
  let input = src;
  let tmp = null;
  if (isUrl) {
    process.stderr.write(`fetching ${src} …\n`);
    const res = await fetch(src);
    if (!res.ok) { console.error(`fetch failed: ${res.status} ${res.statusText}`); process.exit(1); }
    const buf = Buffer.from(await res.arrayBuffer());
    tmp = path.join(outDir, `.dl-${name}`);
    fs.writeFileSync(tmp, buf);
    input = tmp;
  } else if (!fs.existsSync(input)) {
    console.error(`no such file: ${input}`); process.exit(1);
  }

  // decode → mono, target rate, raw float32 LE (exactly what sample_load() wants)
  execFileSync('ffmpeg', ['-y', '-i', input, '-ac', '1', '-ar', String(sr), '-f', 'f32le', f32Path],
               { stdio: ['ignore', 'ignore', 'ignore'] });
  if (tmp) fs.unlinkSync(tmp);

  // peak-normalize to 0.95 (mp3 decode can overshoot ±1.0 → would clip; matches record_grab)
  const raw = fs.readFileSync(f32Path);
  const n = Math.floor(raw.length / 4);
  let peak = 0; for (let i = 0; i < n; i++) { const a = Math.abs(raw.readFloatLE(i * 4)); if (a > peak) peak = a; }
  if (peak > 0.0001 && Math.abs(peak - 0.95) > 0.001) {
    const g = 0.95 / peak;
    for (let i = 0; i < n; i++) raw.writeFloatLE(raw.readFloatLE(i * 4) * g, i * 4);
    fs.writeFileSync(f32Path, raw);
  }

  const bytes = fs.statSync(f32Path).size;
  const samples = Math.floor(bytes / 4);
  const seconds = samples / sr;
  const meta = {
    name, samples, sampleRate: sr, seconds: +seconds.toFixed(4),
    bpm: bpm != null ? +bpm : null, bars: bars != null ? +bars : null,
    source: src,
  };
  fs.writeFileSync(jsonPath, JSON.stringify(meta, null, 2) + '\n');
  process.stderr.write(
    `wrote ${path.relative(process.cwd(), f32Path)}  (${samples} samples · ${seconds.toFixed(2)}s @ ${sr}Hz` +
    `${bpm ? ` · ${bpm} bpm` : ''}${bars ? ` · ${bars} bars` : ''})\n`);
}
main().catch((e) => { console.error(e.message || e); process.exit(1); });
