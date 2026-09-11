// patch-match.cjs — parse what `pm` already writes (patches.txt + cand-N.wav).
//
// The editor drop UI (option A of docs/design/patch-matching-cart.md) is a TOOL
// seam, not an engine one: Electron spawns `build/pm`, this file turns the
// existing report into something the renderer can audition and paste. ADR-0006.
//
//   node editor/electron/patch-match.cjs --selfcheck
//
'use strict'

const fs = require('fs')
const path = require('path')
const os = require('os')

const ANSI = /\x1b\[[0-9;]*m/g

function stripAnsi(s) {
  return String(s || '').replace(ANSI, '')
}

// pm prints "stage 1/2/3" then "candidates" then "wrote …". Mapped onto a coarse
// bar so a 3–7 minute search still reads as progress, not a hung spawn.
function progressFromLine(raw) {
  const line = stripAnsi(raw).replace(/^\s+/, '')
  if (/^stage 1\b/i.test(line)) return { stage: 1, pct: 0.12, label: 'stage 1 — engine race' }
  if (/^stage 2\b/i.test(line)) return { stage: 2, pct: 0.42, label: 'stage 2 — voice refine' }
  if (/^stage 3\b/i.test(line)) return { stage: 3, pct: 0.72, label: 'stage 3 — fx fit' }
  if (/^candidates\b/i.test(line)) return { stage: 4, pct: 0.92, label: 'writing candidates' }
  if (/^wrote\b/i.test(line)) return { stage: 5, pct: 1, label: 'done' }
  return null
}

// Split patches.txt on the candidate banners pm writes. Each block is the
// pasteable instrument() snippet (plus the hit() the CLI includes so you can
// hear it immediately). Header lines above candidate 1 are kept as `header`.
function parsePatchesTxt(text) {
  const src = String(text || '')
  const re = /^\/\/ ---- candidate (\d+):\s+(\S+)\s+voice\s+([0-9.]+)\s+->\s+fx\s+([0-9.]+)\s*$/gm
  const hits = []
  let m
  while ((m = re.exec(src))) {
    hits.push({
      n: parseInt(m[1], 10),
      engine: m[2],
      voice: parseFloat(m[3]),
      fx: parseFloat(m[4]),
      at: m.index,
      end: m.index + m[0].length,
    })
  }
  if (!hits.length) return { header: src.trim(), candidates: [] }
  const header = src.slice(0, hits[0].at).trim()
  const candidates = hits.map((h, i) => {
    const from = h.end
    const to = i + 1 < hits.length ? hits[i + 1].at : src.length
    const snippet = src.slice(from, to).replace(/^\n+/, '').replace(/\s+$/, '')
    return { n: h.n, engine: h.engine, voice: h.voice, fx: h.fx, snippet }
  })
  return { header, candidates }
}

function wavDataUrl(abs) {
  const buf = fs.readFileSync(abs)
  return 'data:audio/wav;base64,' + buf.toString('base64')
}

// Read a finished pm --out dir. Missing cand-N.wav is not fatal (paste still
// works); a missing or empty patches.txt is.
function collectResults(outdir) {
  const dir = path.resolve(outdir)
  const report = path.join(dir, 'patches.txt')
  if (!fs.existsSync(report)) return { ok: false, error: 'pm finished but wrote no patches.txt' }
  const parsed = parsePatchesTxt(fs.readFileSync(report, 'utf8'))
  if (!parsed.candidates.length) return { ok: false, error: 'pm wrote patches.txt with no candidates' }
  const targetPath = path.join(dir, 'target.wav')
  const candidates = parsed.candidates.map(c => {
    const wavPath = path.join(dir, `cand-${c.n}.wav`)
    const hasWav = fs.existsSync(wavPath)
    return {
      ...c,
      wavPath: hasWav ? wavPath : null,
      wavDataUrl: hasWav ? wavDataUrl(wavPath) : null,
    }
  })
  return {
    ok: true,
    dir,
    header: parsed.header,
    targetWav: fs.existsSync(targetPath) ? wavDataUrl(targetPath) : null,
    candidates,
  }
}

function failMsg(code, stderr, stdout) {
  const tail = stripAnsi((stderr || '') + '\n' + (stdout || ''))
    .split('\n')
    .map(s => s.trimEnd())
    .filter(Boolean)
    .slice(-12)
    .join('\n')
  if (code === null) return tail ? `pm timed out.\n${tail}` : 'pm timed out.'
  if (code === 1 && /only .* after onset trim/i.test(tail))
    return `that WAV is too short for the analyser.\n${tail}`
  if (code === 2 && /usage:/i.test(tail)) return `pm refused the arguments.\n${tail}`
  return tail ? `pm exited ${code}.\n${tail}` : `pm exited ${code}.`
}

function selfcheck() {
  let n = 0, failed = 0
  const t = (name, cond) => {
    n++
    if (cond) console.log(`  ✓ ${name}`)
    else { failed++; console.log(`  ✗ ${name}`) }
  }

  t('stripAnsi drops colour', stripAnsi('\x1b[1mstage 1\x1b[0m') === 'stage 1')
  t('stage 1 progress', progressFromLine('  \x1b[1mstage 1\x1b[0m  engine race (18 engines)')?.pct === 0.12)
  t('stage 2 progress', progressFromLine('  stage 2  voice refine (top 3 engines x 3 seeds)')?.stage === 2)
  t('stage 3 progress', progressFromLine('  stage 3  fx fit (voice frozen)')?.stage === 3)
  t('candidates progress', progressFromLine('  candidates  (42s)')?.pct === 0.92)
  t('wrote progress', progressFromLine('  wrote build/patch-match/  (target.wav, cand-1..8.wav, patches.txt)')?.pct === 1)
  t('noise is not progress', progressFromLine('    1. INSTR_SAW      0.12345') === null)

  const fixture = fs.readFileSync(path.join(__dirname, '../../tools/fixtures/patch-match/patches.txt'), 'utf8')
  const parsed = parsePatchesTxt(fixture)
  t('fixture has 3 candidates', parsed.candidates.length === 3)
  t('candidate 1 is PIANO', parsed.candidates[0].engine === 'INSTR_PIANO' && parsed.candidates[0].n === 1)
  t('voice/fx numbers', parsed.candidates[0].voice === 0.12345 && parsed.candidates[0].fx === 0.09876)
  t('snippet starts with instrument(', parsed.candidates[0].snippet.startsWith('    instrument(5, INSTR_PIANO'))
  t('snippet keeps hit()', /hit\(60, 5, 5, 1000\);/.test(parsed.candidates[0].snippet))
  t('candidate 2 has tape', /instrument_tape/.test(parsed.candidates[1].snippet))
  t('header keeps the target line', /piano\.wav/.test(parsed.header))
  t('empty text → no candidates', parsePatchesTxt('').candidates.length === 0)
  t('banner-less text → no candidates', parsePatchesTxt('// just a comment\n').candidates.length === 0)

  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'de-pm-'))
  try {
    t('missing dir fails', collectResults(tmp).ok === false)
    fs.writeFileSync(path.join(tmp, 'patches.txt'), fixture)
    const emptyWav = Buffer.alloc(44)   // header-sized placeholder; we only check the file exists
    fs.writeFileSync(path.join(tmp, 'cand-1.wav'), emptyWav)
    const got = collectResults(tmp)
    t('collectResults ok', got.ok === true)
    t('cand 1 has a data URL', typeof got.candidates[0].wavDataUrl === 'string' && got.candidates[0].wavDataUrl.startsWith('data:audio/wav'))
    t('cand 2 wav is optional', got.candidates[1].wavDataUrl === null)
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true })
  }

  t('timeout message names timeout', /timed out/.test(failMsg(null, '', 'stage 2')))
  t('short-target message is specific', /too short/.test(failMsg(1, 'pm: only 0.100s after onset trim; the 2048-point scale needs 0.058s\n', '')))

  console.log(failed ? `\n${failed}/${n} failed` : `\n${n}/${n} ok`)
  return failed === 0
}

if (require.main === module && process.argv.includes('--selfcheck')) {
  process.exit(selfcheck() ? 0 : 1)
}

module.exports = {
  stripAnsi,
  progressFromLine,
  parsePatchesTxt,
  collectResults,
  failMsg,
  wavDataUrl,
}
