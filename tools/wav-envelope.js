// wav-envelope.js — print a fine-grained AMPLITUDE + BRIGHTNESS envelope of a mono 16-bit WAV.
//
// Companion to tools/wav-analyze.js (which gives peak/RMS/crest/clipping + a coarse per-SECOND
// RMS). This one shows the shape over time at 100ms resolution, with TWO curves:
//   • amplitude  — peak per window (the note's loudness contour)
//   • brightness — the HF/total energy ratio per window (a cheap, LEVEL-INDEPENDENT brightness
//                  proxy: how much of the window's energy is high-frequency). Plotting both is
//                  what cracked the Rhodes "ringy" bug and the funky-clav wah (2026-06-08): a
//                  Rhodes/clav should show brightness DROPPING FASTER than amplitude.
//
//   node tools/wav-envelope.js <file.wav> [windowMs=100]
//
// The brightness column is ABSOLUTE (the HF/total ratio), so it is comparable ACROSS files —
// e.g. clav at timbre 0.15 vs 0.90 (a brighter render reads a higher ratio). The amplitude
// column is normalized to THIS file's peak (it's about contour); the summary line prints the
// absolute peak dBFS + the mean brightness ratio for quick A/B between two renders.
//
// CAVEAT: the HF/total proxy conflates a resonant filter PEAK with perceptual brightness — a
// loud resonant peak reads as "bright" even when it isn't. Good for decay/transient shape and
// rough A/Bs; trust your ears for resonant-filter timbre. (Pairs with tools/navkit-render.c.)

const fs = require('fs');

function readWav(f) {
  const b = fs.readFileSync(f);
  let off = 12, data = null, sr = 44100, ch = 1;
  while (off + 8 <= b.length) {
    const id = b.toString('ascii', off, off + 4), len = b.readUInt32LE(off + 4);
    if (id === 'fmt ') { sr = b.readUInt32LE(off + 12); ch = b.readUInt16LE(off + 10); }
    if (id === 'data') data = { off: off + 8, len };
    off += 8 + len + (len & 1);
  }
  if (!data) throw new Error(`${f}: no data chunk`);
  // stereo (stereo.md): downmix L/R to mono — equals the old mono while voices are centered.
  const n = (data.len / 2 / ch) | 0, s = new Float32Array(n);
  for (let i = 0; i < n; i++)
    s[i] = ch === 1 ? b.readInt16LE(data.off + i * 2) / 32768
                    : (b.readInt16LE(data.off + i * 4) + b.readInt16LE(data.off + i * 4 + 2)) / 65536;
  return { sr, s };
}

const file = process.argv[2];
if (!file) { console.error('usage: node tools/wav-envelope.js <file.wav> [windowMs]'); process.exit(1); }
const winMs = process.argv[3] ? parseFloat(process.argv[3]) : 100;
const { sr, s } = readWav(file);
const W = Math.max(1, (sr * winMs / 1000) | 0);

let pmax = 0, gTot = 0, gHf = 0;        // global peak + global energies (for the absolute summary)
const pk = [], br = [];                 // per-window peak (amplitude) + HF/total ratio (brightness)
for (let i = 0; i < s.length; i += W) {
  let p = 0, tot = 0, hf = 0;
  for (let j = i; j < Math.min(i + W, s.length); j++) {
    const v = s[j]; p = Math.max(p, Math.abs(v));
    tot += v * v;
    if (j > 0) { const d = v - s[j - 1]; hf += d * d; }
  }
  pk.push(p); br.push(tot > 1e-12 ? hf / tot : 0);
  pmax = Math.max(pmax, p); gTot += tot; gHf += hf;
}
const db = (x) => x > 0 ? (20 * Math.log10(x)).toFixed(1) : '-inf';
const meanBr = gTot > 1e-12 ? gHf / gTot : 0;
const brmax = br.reduce((m, x) => Math.max(m, x), 0);   // for the within-file shape bar

console.log(`${file}  (${winMs}ms windows)`);
console.log(`  peak ${db(pmax)} dBFS · brightness(HF/total) mean ${meanBr.toFixed(3)}  ← absolute, compare across files`);
console.log('   t    AMPLITUDE (norm shape)       BRIGHTNESS (abs # · norm bar)');
for (let i = 0; i < pk.length; i++) {
  const a = pmax > 0 ? pk[i] / pmax : 0;          // amplitude: per-file normalized (contour)
  const bb = brmax > 0 ? br[i] / brmax : 0;        // brightness BAR: per-file normalized (contour)
  const t = (i * winMs / 1000).toFixed(2) + 's';
  console.log(t.padStart(6) + ' ' + a.toFixed(2) + ' ' + '#'.repeat(Math.round(a * 18)).padEnd(19) +
              ' ' + br[i].toFixed(3) + ' ' + '#'.repeat(Math.round(bb * 18)));   // number absolute, bar contour
}
