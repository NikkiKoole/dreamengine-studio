// mdcompare.js — A/B a Mutable Dreams BIOME, track by track: the upstream engine (mdrender,
// built by `bash tools/ref-render/run.sh mutable`) against the `dreamatlas` cart's translation.
//
//   node tools/ref-render/mdcompare.js <biome_id> [--secs 12] [--wet] [--tracks 0,3]
//
// Each track is rendered ALONE (mdrender --solo n / -DDA_SOLO=n) and DRY by default (--dry /
// -DDA_DRY: no fx return, no grain tape), so the row compares the VOICE, not the room. --wet keeps
// the room on. A last row renders the whole biome. WAVs land in build/ref-render/md/<id>/.
//
// Columns, upstream → ours:
//   rms     dBFS over the render (LEVEL: the mix balance between tracks)
//   crest   peak − rms in dB (SHAPE: a short hit reads high, a drone low)
//   duty    % of 10 ms windows within 30 dB of the file's own peak (how much of the time it SOUNDS)
//   cent    spectral centroid in Hz over the sounding windows (BRIGHTNESS)
// The two do not line up in time (upstream's first step is at sample 0, ours ~0.1 s in), so
// nothing here is sample-aligned and nothing should be: these are character numbers.
// Δ flags: level off by > 6 dB, centroid off by > 1.5×, duty off by > 2× — a QUESTION to listen
// to, not a verdict. The original being louder does not make it right for our mix; it makes it
// the reference for the BALANCE between tracks, which is the one thing the scene data encodes.

const fs = require('fs'), path = require('path'), { execFileSync } = require('child_process')
const ROOT = path.resolve(__dirname, '../..')
process.chdir(ROOT)
const argv = process.argv.slice(2)
const MODEL = ['KICK', 'SNARE', 'HAT', 'FM', 'VA', 'RINGS', 'HARMONIC', 'CHOIR', 'BASS', 'ELEMENTS', 'MELLOW', 'SATIN', 'VELVET', 'SPECTRAL', 'BREATH']
if (argv[0] === '--fit') fit(argv.slice(1))
const id = parseInt(argv[0], 10)
if (!(id >= 0 && id < 157)) { console.error('usage: mdcompare.js <biome_id 0..156> [--secs n] [--wet] [--tracks 0,3]'); process.exit(1) }
const opt = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : d }
const secs = parseFloat(opt('--secs', '12')), wet = argv.includes('--wet')
const MDR = 'build/ref-render/mdrender'
if (!fs.existsSync(MDR)) { console.error('no ' + MDR + ': run `bash tools/ref-render/run.sh mutable` first'); process.exit(1) }
const OUT = `build/ref-render/md/${id}`; fs.mkdirSync(OUT, { recursive: true })

// the biome's six track models, read from the generated header (row order = biome id)
const rows = fs.readFileSync('runtime/dreamatlas/biomes.h', 'utf8').split('\n').filter(l => /^\s*\{"/.test(l))
const row = rows[id], name = row.match(/^\s*\{"([^"]*)"/)[1]
const tracks = [...row.matchAll(/\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g)].map(m => m.slice(1).map(Number))
const want = opt('--tracks', null)
const list = want ? want.split(',').map(Number) : [0, 1, 2, 3, 4, 5]

function readWav(f) {           // 16-bit PCM or 32-bit float, any channels → mono float
  const b = fs.readFileSync(f); let p = 12, fmt, ch, bits, data
  while (p < b.length - 8) {
    const idc = b.toString('ascii', p, p + 4), sz = b.readUInt32LE(p + 4)
    if (idc === 'fmt ') { fmt = b.readUInt16LE(p + 8); ch = b.readUInt16LE(p + 10); bits = b.readUInt16LE(p + 22) }
    if (idc === 'data') { data = b.subarray(p + 8, p + 8 + sz); break }
    p += 8 + sz + (sz & 1)
  }
  const bps = bits / 8, n = Math.floor(data.length / (bps * ch)), x = new Float32Array(n)
  for (let i = 0; i < n; i++) {
    let s = 0
    for (let c = 0; c < ch; c++) { const o = (i * ch + c) * bps; s += (fmt === 3 || bits === 32) ? data.readFloatLE(o) : data.readInt16LE(o) / 32768 }
    x[i] = s / ch
  }
  return x
}
function fftMag(re) {           // radix-2, returns |X| for the first half
  const n = re.length, im = new Float64Array(n)
  for (let i = 1, j = 0; i < n; i++) { let bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { [re[i], re[j]] = [re[j], re[i]] } }
  for (let len = 2; len <= n; len <<= 1) {
    const a = -2 * Math.PI / len
    for (let i = 0; i < n; i += len) for (let k = 0; k < len / 2; k++) {
      const wr = Math.cos(a * k), wi = Math.sin(a * k), ur = re[i + k], ui = im[i + k]
      const vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi, vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr
      re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi
    }
  }
  const m = new Float64Array(n / 2); for (let k = 0; k < n / 2; k++) m[k] = Math.hypot(re[k], im[k]); return m
}
function measure(f) {
  const x = readWav(f), W = 441
  let pk = 0, ss = 0; for (const v of x) { pk = Math.max(pk, Math.abs(v)); ss += v * v }
  const rms = Math.sqrt(ss / x.length), wins = []
  for (let i = 0; i + W <= x.length; i += W) { let p = 0; for (let k = 0; k < W; k++) p = Math.max(p, Math.abs(x[i + k])); wins.push(p) }
  const duty = wins.filter(p => p > pk * 0.0316).length / Math.max(1, wins.length)
  let num = 0, den = 0; const N = 4096
  for (let i = 0; i + N <= x.length; i += N) {
    let p = 0; for (let k = 0; k < N; k++) p = Math.max(p, Math.abs(x[i + k]))
    if (p < pk * 0.0316) continue
    const re = new Float64Array(N); for (let k = 0; k < N; k++) re[k] = x[i + k] * (0.5 - 0.5 * Math.cos(2 * Math.PI * k / N))
    const m = fftMag(re); for (let k = 1; k < N / 2; k++) { num += m[k] * k * 44100 / N; den += m[k] }
  }
  const db = v => v > 0 ? 20 * Math.log10(v) : -Infinity
  return { rms: db(rms), crest: db(pk) - db(rms), duty: duty * 100, cent: den ? num / den : 0, silent: pk < 1e-4 }
}
function renderUp(solo, f) {
  const a = [String(id), String(secs)]; if (solo >= 0) a.push('--solo', String(solo)); if (!wet) a.push('--dry')
  // through a FILE, not a pipe: feeding ffmpeg 3.5 MB on stdin hung it once mid-sweep (21 min)
  const rawf = f.replace(/\.wav$/, '.f32')
  fs.writeFileSync(rawf, execFileSync(MDR, a, { maxBuffer: 1 << 30, stdio: ['ignore', 'pipe', 'ignore'] }))
  execFileSync('ffmpeg', ['-v', 'error', '-nostdin', '-y', '-f', 'f32le', '-ar', '44100', '-ac', '2', '-i', rawf, '-c:a', 'pcm_s16le', f], { stdio: 'ignore', timeout: 60000 })
  fs.unlinkSync(rawf)
}
function renderOurs(solo, f) {
  const defs = [`DA_ID=${id}`]; if (solo >= 0) defs.push(`DA_SOLO=${solo}`); if (!wet) defs.push('DA_DRY')
  execFileSync('node', ['tools/play.js', 'dreamatlas', 'run', '--headless', '--frames', String(Math.round(secs * 60)), '--wav', path.resolve(f)],
    { env: { ...process.env, DE_DEFINES: defs.join(',') }, stdio: 'ignore' })
}
const fmt = (v, d = 1) => !isFinite(v) ? '  -inf' : v.toFixed(d).padStart(6)
console.log(`biome ${id} "${name}"  ${secs}s ${wet ? 'WET' : 'dry'}   upstream → ours`)
console.log('trk model     rms dB          crest dB        duty %          centroid Hz        flags')
const ups = {}, ours = {}
for (const t of [...list, -1]) {
  const tag = t < 0 ? 'mix' : 't' + t
  const fu = `${OUT}/up_${tag}${wet ? '_wet' : ''}.wav`, fo = `${OUT}/da_${tag}${wet ? '_wet' : ''}.wav`
  renderUp(t, fu); renderOurs(t, fo)
  const u = measure(fu), o = measure(fo); ups[tag] = u; ours[tag] = o
  const flags = []
  if (u.silent !== o.silent) flags.push(u.silent ? 'UPSTREAM SILENT' : 'OURS SILENT')
  else if (!u.silent) {
    if (Math.abs(u.rms - o.rms) > 6) flags.push(`level ${o.rms > u.rms ? '+' : ''}${(o.rms - u.rms).toFixed(0)}dB`)
    if (u.cent && o.cent && Math.max(u.cent / o.cent, o.cent / u.cent) > 1.5) flags.push(o.cent > u.cent ? 'brighter' : 'darker')
    if (u.duty && o.duty && Math.max(u.duty / o.duty, o.duty / u.duty) > 2) flags.push(o.duty > u.duty ? 'rings longer' : 'shorter')
  }
  const model = t < 0 ? 'MIX' : (MODEL[tracks[t][0]] || '?') + (tracks[t][12] ? '(m)' : '')
  console.log(`${tag.padEnd(4)}${model.padEnd(10)}${fmt(u.rms)} →${fmt(o.rms)}   ${fmt(u.crest)} →${fmt(o.crest)}   ${fmt(u.duty, 0)} →${fmt(o.duty, 0)}   ${fmt(u.cent, 0)} →${fmt(o.cent, 0)}    ${flags.join(', ')}`)
}
// balance: each track's level relative to the loudest track on the same side — the mix shape
const rel = side => { const v = list.map(t => side['t' + t]).filter(m => !m.silent); const top = Math.max(...v.map(m => m.rms)); return list.map(t => side['t' + t].silent ? null : side['t' + t].rms - top) }
const ru = rel(ups), ro = rel(ours)
console.log('balance (dB under the loudest track): ' + list.map((t, k) => `t${t} ${ru[k] == null ? '-' : ru[k].toFixed(0)}→${ro[k] == null ? '-' : ro[k].toFixed(0)}`).join('  '))
console.log(`wavs: ${OUT}/`)
fs.writeFileSync(`${OUT}/result${wet ? '_wet' : ''}.json`, JSON.stringify({ id, name, secs, wet,
  tracks: list.map(t => ({ t, model: MODEL[tracks[t][0]], up: ups['t' + t], ours: ours['t' + t] })), mix: { up: ups.mix, ours: ours.mix } }, null, 1))

// --fit <ids…>: read the result.json of biomes ALREADY compared (dry) and print, per voice model,
// how far its level sits from the COMMON offset between the two engines (the median over every
// sounding track). That residual is the trim the cart's VOICE_TRIM table wants: a model at +6 is
// 6 dB too loud for its place in the balance. Centroid ratio printed beside it (ours / upstream).
function fit(ids) {
  const rows = []
  for (const i of ids) {
    const f = `build/ref-render/md/${i}/result.json`
    if (!fs.existsSync(f)) { console.error(`no ${f}: run mdcompare.js ${i} first`); continue }
    for (const r of JSON.parse(fs.readFileSync(f, 'utf8')).tracks) if (!r.up.silent && !r.ours.silent) rows.push({ ...r, id: i, d: r.ours.rms - r.up.rms, c: r.ours.cent / r.up.cent })
  }
  const med = a => { const s = [...a].sort((x, y) => x - y); return s.length ? (s.length % 2 ? s[s.length >> 1] : (s[s.length / 2 - 1] + s[s.length / 2]) / 2) : NaN }
  const G = med(rows.map(r => r.d))
  console.log(`${rows.length} sounding tracks over ${ids.length} biomes · common offset ours−upstream = ${G.toFixed(1)} dB`)
  console.log('model       n   level residual dB (median, range)     centroid ours/up (median)')
  for (const m of MODEL) {
    const r = rows.filter(x => x.model === m); if (!r.length) continue
    const d = r.map(x => x.d - G)
    console.log(`${m.padEnd(10)}${String(r.length).padStart(3)}   ${med(d).toFixed(1).padStart(6)}  (${Math.min(...d).toFixed(0)} … ${Math.max(...d).toFixed(0)})${''.padEnd(16)}${med(r.map(x => x.c)).toFixed(2)}×`)
  }
  process.exit(0)
}
