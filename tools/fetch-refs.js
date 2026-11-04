#!/usr/bin/env node
// fetch-refs.js — rebuild build/refs/<set>/ (CC0 reference recordings) from a COMMITTED manifest.
//
// The references live in build/, which git ignores (audio does not belong in the repo), so on a fresh
// machine they are simply missing, and so was the list of what they were: build/refs/drums-cc0/
// SOURCES.txt sat in build/ too. The manifest is now tools/refs/<set>.txt (committed); this rebuilds
// the folder from it.
//
//   node tools/fetch-refs.js [set]           fetch what is missing (default set: drums-cc0)
//   node tools/fetch-refs.js [set] --force   re-fetch everything
//   node tools/fetch-refs.js [set] --check   offline: is every listed file present? exit 1 if not
//   node tools/fetch-refs.js --selfcheck     known answers for the manifest parser + the license test
//
// Manifest line: <freesound id> <local name> CC0 by <user> <hq preview mp3 url>   (# = comment)
// Per entry it writes <name>.mp3 (as downloaded), <name>.wav (44.1 kHz mono 16-bit: what
// oneshot-compare reads) and <name>.f32 (raw float32 44.1 kHz mono: what drumhand's recorded kit
// loads; capped at 6 s with a 0.6 s linear fade, byte-identical to the files that kit was built with).
//
// ⚠ LICENSE, every time: the license is re-read from the sound's own Freesound page before downloading,
// and anything that is not CC0 is REFUSED. A manifest that says "CC0" is a claim, the page is the fact,
// and an uploader can change it. (Measured while building the set: one author's cymbals are CC0 and
// the same author's kit drums CC BY 4.0, so "this uploader is CC0" is not a rule.) Network = freesound.org
// only, with a generic client name.
'use strict'
const fs = require('fs'), path = require('path'), { execFileSync } = require('child_process')
const ROOT = path.resolve(__dirname, '..'), UA = 'dreamengine-fetch-refs'
const CC0 = 'creativecommons.org/publicdomain/zero/1.0/'

function parseManifest(text) {
  const entries = [], bad = []
  text.split('\n').forEach((raw, i) => {
    const l = raw.trim(); if (!l || l.startsWith('#')) return
    const m = l.match(/^(\d+)\s+([A-Za-z0-9_.-]+)\s+(\S+)\s+by\s+(\S+)\s+(https:\/\/cdn\.freesound\.org\/previews\/\S+\.mp3)$/)
    if (!m) { bad.push({ line: i + 1, text: l }); return }
    entries.push({ id: m[1], name: m[2], claim: m[3], user: m[4], url: m[5] })
  })
  return { entries, bad }
}
// the license links on a Freesound sound page; CC0 only when THE license link is the CC0 deed and no
// other creativecommons license appears (a page can link several in its chrome, so all are read)
function pageLicense(html) {
  const links = [...new Set((html.match(/creativecommons\.org\/(?:licenses|publicdomain)\/[a-z0-9.+/-]+/g) || []).map(s => s.replace(/\/?$/, '/')))]
  return { links, cc0: links.length === 1 && links[0] === CC0 }
}

function curl(url, out) {
  const a = ['-s', '-L', '-f', '-A', UA, url]; if (out) a.push('-o', out)
  return execFileSync('curl', a, { maxBuffer: 1 << 26, encoding: out ? undefined : 'utf8' })
}

function fetchSet(set, force) {
  const man = path.join(ROOT, 'tools/refs', `${set}.txt`)
  const { entries, bad } = parseManifest(fs.readFileSync(man, 'utf8'))
  if (bad.length) { for (const b of bad) console.error(`  ✗ ${set}.txt:${b.line} unparsable: ${b.text}`); process.exit(1) }
  const dir = path.join(ROOT, 'build/refs', set); fs.mkdirSync(dir, { recursive: true })
  let got = 0, kept = 0, refused = 0
  for (const e of entries) {
    const wav = path.join(dir, `${e.name}.wav`), f32 = path.join(dir, `${e.name}.f32`), mp3 = path.join(dir, `${e.name}.mp3`)
    if (!force && fs.existsSync(wav) && fs.existsSync(f32)) { kept++; continue }
    const lic = pageLicense(curl(`https://freesound.org/people/${encodeURIComponent(e.user)}/sounds/${e.id}/`))
    if (!lic.cc0) { console.error(`  ✗ ${e.name} (${e.id} by ${e.user}): page license is ${lic.links.join(', ') || 'unreadable'}, not CC0. Refused.`); refused++; continue }
    curl(e.url, mp3)
    execFileSync('ffmpeg', ['-v', 'error', '-y', '-i', mp3, '-ac', '1', '-ar', '44100', '-c:a', 'pcm_s16le', wav])
    const dur = +execFileSync('ffprobe', ['-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', mp3], { encoding: 'utf8' })
    const cap = dur > 6 ? ['-t', '6', '-af', 'afade=t=out:st=5.4:d=0.6'] : []
    execFileSync('ffmpeg', ['-v', 'error', '-y', '-i', mp3, '-ac', '1', '-ar', '44100', ...cap, '-f', 'f32le', f32])
    console.log(`  ✓ ${e.name}  (${e.id} by ${e.user}, CC0 checked)`); got++
  }
  // the human-readable list travels with the files, as before
  fs.writeFileSync(path.join(dir, 'SOURCES.txt'), entries.map(e => `${e.id} ${e.name} ${e.claim} by ${e.user} ${e.url}`).join('\n') + '\n')
  console.log(`fetch-refs ${set}: ${got} fetched, ${kept} already present, ${refused} refused  ->  build/refs/${set}/`)
  if (refused) process.exit(1)
}

function checkSet(set) {
  const { entries, bad } = parseManifest(fs.readFileSync(path.join(ROOT, 'tools/refs', `${set}.txt`), 'utf8'))
  const missing = entries.filter(e => !fs.existsSync(path.join(ROOT, 'build/refs', set, `${e.name}.wav`)) || !fs.existsSync(path.join(ROOT, 'build/refs', set, `${e.name}.f32`)))
  console.log(`fetch-refs --check ${set}: ${entries.length - missing.length}/${entries.length} present${bad.length ? `, ${bad.length} unparsable line(s)` : ''}${missing.length ? `; missing: ${missing.map(e => e.name).join(' ')} (run node tools/fetch-refs.js ${set})` : ''}`)
  process.exit(missing.length || bad.length ? 1 : 0)
}

function selfcheck() {
  let pass = 0, fail = 0
  const ok = (c, w) => { if (c) pass++; else { fail++; console.log(`  ✗ ${w}`) } }
  const p = parseManifest([
    '# a comment',
    '93907 ride CC0 by pjcohen https://cdn.freesound.org/previews/93/93907_394391-hq.mp3',
    '',
    '566897 ref_kick CC0 by lennartgreen https://cdn.freesound.org/previews/566/566897_3661618-hq.mp3',
    '12345 broken line without a url',
    '777 evil;name CC0 by x https://cdn.freesound.org/previews/7/777_1-hq.mp3',
    '888 elsewhere CC0 by x https://example.com/888.mp3',
  ].join('\n'))
  ok(p.entries.length === 2, `two good entries parsed (got ${p.entries.length})`)
  ok(p.entries[1] && p.entries[1].name === 'ref_kick' && p.entries[1].user === 'lennartgreen' && p.entries[1].id === '566897', 'fields read right')
  ok(p.bad.length === 3, `three bad lines REPORTED, not skipped: no url, a shell-unsafe name, a non-freesound url (got ${p.bad.length})`)
  const page = l => `<html><a rel="license" href="https://${l}">license</a></html>`
  ok(pageLicense(page('creativecommons.org/publicdomain/zero/1.0/')).cc0 === true, 'a CC0 page reads CC0')
  ok(pageLicense(page('creativecommons.org/licenses/by/4.0/')).cc0 === false, 'a CC BY 4.0 page is REFUSED (the pjcohen kit drums)')
  ok(pageLicense(page('creativecommons.org/licenses/by-nc/4.0/')).cc0 === false, 'a CC BY-NC page is refused')
  ok(pageLicense(page('creativecommons.org/licenses/sampling+/1.0/')).cc0 === false, 'a Sampling+ page is refused')
  ok(pageLicense('<html>no license link at all</html>').cc0 === false, 'a page with NO license link is refused, not assumed')
  ok(pageLicense(page('creativecommons.org/publicdomain/zero/1.0/') + page('creativecommons.org/licenses/by/4.0/')).cc0 === false, 'two different licenses on one page is refused')
  // the committed manifest itself must parse completely
  const real = parseManifest(fs.readFileSync(path.join(ROOT, 'tools/refs/drums-cc0.txt'), 'utf8'))
  ok(real.bad.length === 0 && real.entries.length > 0, `tools/refs/drums-cc0.txt parses fully (${real.entries.length} entries, ${real.bad.length} bad)`)
  console.log(`fetch-refs selfcheck: ${fail ? 'FAIL' : 'PASS'} (${pass}/${pass + fail})`)
  process.exit(fail ? 1 : 0)
}

if (require.main === module) {
  const argv = process.argv.slice(2), set = argv.find(a => !a.startsWith('--')) || 'drums-cc0'
  if (argv.includes('--selfcheck')) selfcheck()
  else if (argv.includes('--check')) checkSet(set)
  else fetchSet(set, argv.includes('--force'))
}
module.exports = { parseManifest, pageLicense }
