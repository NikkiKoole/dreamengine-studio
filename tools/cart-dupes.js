#!/usr/bin/env node
// cart-dupes.js — find ACTIONABLE code duplication across carts.
//
// Not a generic clone detector: this repo copies on purpose ("start from a
// cousin's chassis"), so raw clone counts are noise. This tool surfaces the two
// flavors that are actually worth a refactor:
//
//   1. EXTRACTION candidates — a normalized block repeated across N≥3 carts that
//      ISN'T already behind a library header. This is how the next ui.h/pointer.h
//      gets born. Ranked by tokens × cart-spread.
//   2. DRIFT — the same-named helper copied into several carts and then diverged.
//      The "silent revert" hazard CLAUDE.md keeps warning about. Flagged when
//      pairwise similarity is high-but-imperfect (0.5 ≤ sim < 1.0).
//
// The trick that makes it find REAL clones instead of every `for` loop: identifiers
// are canonicalized to "V" — EXCEPT the engine/library vocabulary (every identifier
// that appears in runtime/*.h) and C keywords, which stay literal. So the studio.h /
// header API calls (PTR_ACQUIRE, ui_button, note_on…) are the structural fingerprint.
//
// Usage:
//   node tools/cart-dupes.js                 # full report (extraction + drift)
//   node tools/cart-dupes.js --top 25        # top N extraction clusters (default 20)
//   node tools/cart-dupes.js --min-tokens 50 # min normalized tokens for a clone (default 40)
//   node tools/cart-dupes.js --drift         # only the drift report
//   node tools/cart-dupes.js --extract       # only the extraction report
//   node tools/cart-dupes.js --json          # machine-readable
//   node tools/cart-dupes.js --selfcheck     # assert the CHECKER (known-answer fixture, 20)
//
// Zero deps, plain node (CommonJS). Costs nothing to run — no LLM.

const fs = require('fs');
const path = require('path');

const args = process.argv.slice(2);
const flag = (name, def) => {
  const i = args.indexOf(name);
  if (i < 0) return def;
  const v = args[i + 1];
  return (v && !v.startsWith('--')) ? v : true;
};
const TOP = parseInt(flag('--top', 20), 10);
const MIN_TOKENS = parseInt(flag('--min-tokens', 40), 10);
const WINDOW = Math.min(MIN_TOKENS, 30);   // seed window length
const JSON_OUT = args.includes('--json');
const ONLY_DRIFT = args.includes('--drift');
const ONLY_EXTRACT = args.includes('--extract');

// Both inputs are overridable so --selfcheck can point the whole tool at a tiny FIXTURE repo
// (tools/fixtures/cart-dupes/) instead of the 573-cart shelf. CART_EXT exists so fixture carts
// can be `.c.txt`: a fixture cart is never compiled, and a real `.c` gets indexed by clangd and
// read as a cart by anything globbing for sources.
const runtimeDir = process.env.DE_DUPES_RUNTIME_DIR || path.join(__dirname, '..', 'runtime');
const cartDir    = process.env.DE_DUPES_CART_DIR    || path.join(__dirname, 'carts');
const CART_EXT   = process.env.DE_DUPES_CART_EXT    || '.c';

// ---- anchor vocabulary: every identifier that appears in runtime/*.h CODE ---------
// DECOMMENTED FIRST, and that is load-bearing, not tidiness. Harvesting the raw header text
// swept every English word in every header COMMENT into the "engine vocabulary": 8709 of 15456
// entries (56%) were prose — `the`, `and`, `shared`, `extracted`, `carts`, `drifting`. Since an
// anchored identifier stays LITERAL instead of collapsing to V, that silently defeated the
// core trick on any cart-local whose name happened to appear in a header comment, so two blocks
// differing only in a variable name were not recognised as clones. Found 2026-07-31 by the
// --selfcheck fixture below (the fixture's own anchor set contained the words of its own
// header comment, which is what made it visible).
//
// KNOWN LIMITATION, deliberately not fixed here: PARAMETER names in the prototypes are still
// harvested, so `i` `j` `k` `n` `x` `y` `w` `h` `tmp` `row` `col` `slot` `vel` remain anchored
// and a pure loop-variable rename can still hide a clone. Excluding those needs real declaration
// parsing rather than a word regex; the fixture pins today's behaviour so a future fix is visible.
const anchor = new Set();
for (const f of fs.readdirSync(runtimeDir).filter(f => f.endsWith('.h'))) {
  const src = fs.readFileSync(path.join(runtimeDir, f), 'utf8');
  for (const m of decomment(src).matchAll(/\b[A-Za-z_][A-Za-z0-9_]*\b/g)) anchor.add(m[0]);
}
// The normalization SENTINELS must never be vocabulary. `N` really is a header parameter name,
// so without this a cart-local named N and any numeric literal both normalize to "N" and compare
// EQUAL — a false match — and the report lists "N"/"V" among a cluster's api calls, which is
// visible nonsense. (`V` only reached the set via comment prose and is already gone above.)
anchor.delete('V'); anchor.delete('N');
const KEYWORDS = new Set(('auto break case char const continue default do double else enum extern ' +
  'float for goto if inline int long register restrict return short signed sizeof static struct ' +
  'switch typedef union unsigned void volatile while bool true false').split(' '));

// ---- decomment, preserving newlines so token line numbers stay accurate ----------
function decomment(src) {
  let out = '', i = 0, n = src.length;
  while (i < n) {
    const c = src[i], d = src[i + 1];
    if (c === '/' && d === '*') { i += 2; while (i < n && !(src[i] === '*' && src[i + 1] === '/')) { if (src[i] === '\n') out += '\n'; i++; } i += 2; continue; }
    if (c === '/' && d === '/') { while (i < n && src[i] !== '\n') i++; continue; }
    if (c === '"' || c === '\'') { const q = c; out += ' '; i++; while (i < n && src[i] !== q) { if (src[i] === '\\') i++; if (src[i] === '\n') out += '\n'; i++; } i++; continue; }
    out += c; i++;
  }
  return out;
}

// ---- tokenize C into {t: text, norm: normalized, line} --------------------------
const OPS = ['<<=', '>>=', '...', '->', '++', '--', '<<', '>>', '<=', '>=', '==', '!=',
  '&&', '||', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '::'];
function tokenize(src) {
  const toks = [];
  let i = 0, line = 1, n = src.length;
  while (i < n) {
    const c = src[i];
    if (c === '\n') { line++; i++; continue; }
    if (c === ' ' || c === '\t' || c === '\r') { i++; continue; }
    // identifier / keyword
    if (/[A-Za-z_]/.test(c)) {
      let j = i + 1; while (j < n && /[A-Za-z0-9_]/.test(src[j])) j++;
      const word = src.slice(i, j);
      let norm;
      if (KEYWORDS.has(word)) norm = word;
      else if (anchor.has(word)) norm = word;   // engine/library vocab stays literal
      else norm = 'V';                            // cart-local identifier
      toks.push({ t: word, norm, line }); i = j; continue;
    }
    // number
    if (/[0-9]/.test(c) || (c === '.' && /[0-9]/.test(src[i + 1] || ''))) {
      let j = i + 1; while (j < n && /[0-9a-fA-FxX.eE+\-]/.test(src[j]) && !/\s/.test(src[j])) {
        // stop +/- unless part of exponent
        if ((src[j] === '+' || src[j] === '-') && !/[eE]/.test(src[j - 1])) break;
        j++;
      }
      toks.push({ t: src.slice(i, j), norm: 'N', line }); i = j; continue;
    }
    // multi-char operator
    let op = null;
    for (const o of OPS) if (src.startsWith(o, i)) { op = o; break; }
    if (op) { toks.push({ t: op, norm: op, line }); i += op.length; continue; }
    // single char
    toks.push({ t: c, norm: c, line }); i++;
  }
  return toks;
}

// ---- load carts -----------------------------------------------------------------
const files = fs.readdirSync(cartDir).filter(f => f.endsWith(CART_EXT)).sort();
const carts = files.map(f => {
  const raw = fs.readFileSync(path.join(cartDir, f), 'utf8');
  const toks = tokenize(decomment(raw));
  return { name: f.slice(0, -CART_EXT.length), toks, norm: toks.map(t => t.norm) };
});

// ---- --selfcheck: assert the CHECKER against known answers -----------------------
// See docs/guides/checks-and-oracles.md "Self-test the checker". Runs the whole tool against a
// 10-cart FIXTURE repo (tools/fixtures/cart-dupes/) with its own tiny anchor header, via the
// DE_DUPES_* path overrides, and asserts what it must say.
//
// WHY. The tool's own header calls normalization "the trick that makes it find REAL clones
// instead of every `for` loop" — cart-local identifiers collapse to V while the engine/library
// vocabulary stays literal. Both halves of that are load-bearing IN OPPOSITE DIRECTIONS: lose
// the collapse and a renamed copy stops matching (a missed clone); lose the literal vocabulary
// and two blocks calling DIFFERENT engine functions match anyway (a false clone). Nothing
// measured either. The fixture's alpha/beta/gamma trio is built to pin exactly that pair, and
// writing it is what exposed the comment-prose bug fixed above.
if (args.includes('--selfcheck')) {
  const cp = require('child_process');
  const FX = path.join(__dirname, 'fixtures', 'cart-dupes');
  let raw;
  try {
    raw = cp.execFileSync(process.execPath, [__filename, '--json', '--min-tokens', '40'], {
      env: { ...process.env,
             DE_DUPES_RUNTIME_DIR: path.join(FX, 'runtime'),
             DE_DUPES_CART_DIR:    path.join(FX, 'carts'),
             DE_DUPES_CART_EXT:    '.c.txt' },
      encoding: 'utf8', maxBuffer: 1 << 26,
    });
  } catch (e) { raw = e.stdout; }
  const g = JSON.parse(raw);

  const T = [];
  const t = (n, ok) => T.push({ n, ok });
  const cartsOf   = (c) => c.occurrences.map(o => o.cart).sort();
  // Crash-safe accessors: a MISSING cluster must fail its assertion, not throw. The first draft
  // read `clusterOf(...).tokens` directly, so the mutation "stop collapsing cart-locals" (which
  // removes the alpha+beta cluster entirely) died with a TypeError instead of printing which of
  // the 20 expectations actually broke. A self-test that crashes tells you far less than one
  // that fails.
  const clusterOf = (...names) =>
    g.extraction.find(c => cartsOf(c).join('+') === names.sort().join('+')) || { tokens: -1, apiCalls: [], score: -1, spread: -1 };
  const driftOf   = (name) => g.drift.find(d => d.name === name);

  // ── the fixture loaded at all
  t('loads the fixture repo, not the real shelf  [10 carts, tiny anchor]',
    g.carts.length === 10 && g.anchorSize > 10 && g.anchorSize < 60)
  t('the normalization SENTINELS are not vocabulary  [V/N collision]',
    !g.extraction.some(c => c.apiCalls.includes('V') || c.apiCalls.includes('N')))

  // ── THE TRICK, both directions. alpha/beta/gamma are structurally identical; only the
  //    identifier CLASSES differ, so this trio isolates normalization from everything else.
  t('normalize: two carts differing ONLY in local names DO cluster  [cart-locals → V]',
    !!clusterOf('alpha', 'beta'))
  t('normalize: ...and the cluster is the whole shared block, extended in lockstep',
    clusterOf('alpha', 'beta').tokens === 178)
  t('normalize: a cart with the same locals but DIFFERENT engine calls does NOT cluster  ' +
    '[engine vocab stays literal]',
    !g.extraction.some(c => cartsOf(c).includes('gamma')))
  t('normalize: the engine calls are reported as the cluster fingerprint',
    ['rectfill', 'screen_w', 'ui_button', 'note_on', 'circfill']
      .every(fn => clusterOf('alpha', 'beta').apiCalls.includes(fn)))

  // ── extraction mechanics
  t('extraction: an identical helper in 2 carts is a cluster', !!clusterOf('eta', 'zeta'))
  t('extraction: a block shared by 3 carts reports spread 3  [cart-spread, not pair count]',
    g.extraction.some(c => c.spread === 3 && cartsOf(c).join('+') === 'iota+kappa+theta'))
  t('extraction: score is tokens x cart-spread',
    g.extraction.every(c => c.score === c.tokens * c.spread))
  t('extraction: every cluster clears --min-tokens  [threshold guard]',
    g.extraction.length > 0 && g.extraction.every(c => c.tokens >= g.config.MIN_TOKENS))
  t('extraction: results are ranked by score, descending',
    g.extraction.every((c, i) => i === 0 || g.extraction[i - 1].score >= c.score))
  t('extraction: anchorDensity is a fraction  [the "should use a header" signal]',
    g.extraction.every(c => c.anchorDensity >= 0 && c.anchorDensity <= 1))

  // ── DRIFT: copied THEN diverged, which is neither "identical" nor "unrelated"
  t('drift: a copied-then-diverged helper is reported', !!driftOf('apply_env'))
  t('drift: ...with the similarity inside the [DRIFT_LO, DRIFT_HI) band',
    driftOf('apply_env').medianDriftSim >= g.config.DRIFT_LO &&
    driftOf('apply_env').medianDriftSim <  g.config.DRIFT_HI)
  t('drift: ...and it names where every copy lives',
    driftOf('apply_env').where.join(' ') === 'delta:6 epsilon:7')
  t('drift: a BYTE-IDENTICAL copy is not drift  [exempt class: that is extraction]',
    !driftOf('clamp_all'))
  t('drift: a 3-copy helper separates identical pairs from drifted ones',
    driftOf('mix_bus') && driftOf('mix_bus').copies === 3 &&
    driftOf('mix_bus').identicalPairs === 1 && driftOf('mix_bus').driftedPairs === 2 &&
    driftOf('mix_bus').totalPairs === 3)
  t('drift: draw() is excluded as an engine HOOK, not flagged as a copy  [HOOK_CUTOFF]',
    !driftOf('draw') && g.config.HOOK_CUTOFF === 4)
  t('drift: results are ranked by drifted-pair count',
    g.drift.every((d, i) => i === 0 || g.drift[i - 1].driftedPairs >= d.driftedPairs))

  // ── the inert-fixture guard: these "must not appear" cases only mean something if the
  //    fixture actually contains the shapes that would otherwise appear.
  t('fixture is not inert: gamma and draw() really are present to be excluded',
    (() => {
      const dir = path.join(FX, 'carts');
      const gsrc = fs.readFileSync(path.join(dir, 'gamma.c.txt'), 'utf8');
      const longDraws = fs.readdirSync(dir)
        .filter(f => /void draw\(void\) \{[\s\S]{200,}/.test(fs.readFileSync(path.join(dir, f), 'utf8')));
      return /ui_button/.test(gsrc) && /note_off/.test(gsrc) && longDraws.length > g.config.HOOK_CUTOFF;
    })())

  const failed = T.filter(x => !x.ok);
  for (const x of T) console.log(`  ${x.ok ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m'} ${x.n}`);
  console.log(failed.length
    ? `\x1b[31mcart-dupes --selfcheck FAILED\x1b[0m — ${failed.length} of ${T.length} expectations broken`
    : `cart-dupes --selfcheck: ${T.length}/${T.length} known answers correct`);
  process.exit(failed.length ? 1 : 0);
}

// ================================================================================
// 1. EXTRACTION clusters — maximal normalized blocks shared across ≥2 carts
// ================================================================================
// rolling-hash every window; group by hash; verify; greedily extend in lockstep;
// mark covered token-starts consumed so sub-windows of a long match aren't re-reported.

const BASE = 1000003n, MOD = (1n << 61n) - 1n;
function hashWindow(norm, start, len) {
  let h = 0n;
  for (let k = 0; k < len; k++) h = (h * BASE + BigInt(hashStr(norm[start + k]))) % MOD;
  return h;
}
const strCache = new Map();
function hashStr(s) {
  let v = strCache.get(s);
  if (v !== undefined) return v;
  v = 0; for (let k = 0; k < s.length; k++) v = (v * 131 + s.charCodeAt(k)) >>> 0;
  strCache.set(s, v); return v;
}

// index windows by hash
const byHash = new Map();
for (let ci = 0; ci < carts.length; ci++) {
  const norm = carts[ci].norm;
  for (let s = 0; s + WINDOW <= norm.length; s++) {
    const h = hashWindow(norm, s, WINDOW);
    let arr = byHash.get(h); if (!arr) byHash.set(h, arr = []);
    arr.push([ci, s]);
  }
}

const consumed = carts.map(c => new Uint8Array(c.norm.length));
const eq = (a, b) => a.length === b.length && a.every((x, k) => x === b[k]);

const clusters = [];
// process larger hash groups first is not necessary; just iterate
for (const [, occ] of byHash) {
  if (occ.length < 2) continue;
  // distinct carts?
  const cartsSeen = new Set(occ.map(o => o[0]));
  if (cartsSeen.size < 2) continue;
  // pick a not-yet-consumed seed
  const seed = occ.find(([ci, s]) => !consumed[ci][s]);
  if (!seed) continue;
  const [sci, sstart] = seed;
  const seedNorm = carts[sci].norm.slice(sstart, sstart + WINDOW);
  // members = occurrences whose window matches the seed exactly and isn't consumed
  let members = occ.filter(([ci, s]) => !consumed[ci][s] && eq(carts[ci].norm.slice(s, s + WINDOW), seedNorm));
  if (new Set(members.map(m => m[0])).size < 2) continue;
  // extend forward in lockstep
  let endLen = WINDOW;
  for (;;) {
    const ref = carts[members[0][0]].norm[members[0][1] + endLen];
    if (ref === undefined) break;
    if (!members.every(([ci, s]) => carts[ci].norm[s + endLen] === ref)) break;
    endLen++;
  }
  // extend backward in lockstep
  let back = 0;
  for (;;) {
    const ref = carts[members[0][0]].norm[members[0][1] - back - 1];
    if (ref === undefined || members[0][1] - back - 1 < 0) break;
    if (!members.every(([ci, s]) => s - back - 1 >= 0 && carts[ci].norm[s - back - 1] === ref)) break;
    back++;
  }
  const total = endLen + back;
  if (total < MIN_TOKENS) {
    // still consume the seed window so we don't spin on it
    for (const [ci, s] of members) consumed[ci][s] = 1;
    continue;
  }
  // mark consumed across the full span
  for (const [ci, s] of members) {
    for (let k = -back; k < endLen; k++) { const idx = s + k; if (idx >= 0 && idx < consumed[ci].length) consumed[ci][idx] = 1; }
  }
  // dedupe member carts (keep first occurrence per cart) for reporting
  const perCart = new Map();
  for (const [ci, s] of members) {
    if (!perCart.has(ci)) {
      const a = s - back, b = s + endLen - 1;
      perCart.set(ci, { cart: carts[ci].name, l0: carts[ci].toks[a].line, l1: carts[ci].toks[b].line });
    }
  }
  if (perCart.size < 2) continue;
  // anchor density: fraction of normalized tokens that are literal vocab (not V/N/punct)
  const span = carts[members[0][0]].norm.slice(members[0][1] - back, members[0][1] + endLen);
  const anchorTok = span.filter(t => /^[A-Za-z_]/.test(t) && t !== 'V').length;
  const apiCalls = [...new Set(span.filter(t => anchor.has(t) && !KEYWORDS.has(t)))];
  clusters.push({
    tokens: total, spread: perCart.size,
    score: total * perCart.size,
    anchorDensity: +(anchorTok / total).toFixed(2),
    apiCalls,
    occurrences: [...perCart.values()].sort((a, b) => a.cart.localeCompare(b.cart)),
  });
}
clusters.sort((a, b) => b.score - a.score);

// ================================================================================
// 2. DRIFT — same-named top-level functions that diverged across carts
// ================================================================================
function extractFns(cart) {
  const fns = [];
  const toks = cart.toks;
  let depth = 0;
  for (let i = 0; i < toks.length; i++) {
    const t = toks[i].t;
    if (t === '{') {
      // is this a function head at depth 0?  ... name ( ... ) {
      if (depth === 0) {
        // walk back over a balanced (...) immediately preceding
        let j = i - 1;
        if (toks[j] && toks[j].t === ')') {
          let pd = 0;
          for (; j >= 0; j--) { if (toks[j].t === ')') pd++; else if (toks[j].t === '(') { pd--; if (pd === 0) break; } }
          const nameTok = toks[j - 1];
          if (nameTok && /^[A-Za-z_]\w*$/.test(nameTok.t) && !KEYWORDS.has(nameTok.t)) {
            // body span
            let d = 0, k = i;
            for (; k < toks.length; k++) { if (toks[k].t === '{') d++; else if (toks[k].t === '}') { d--; if (d === 0) break; } }
            fns.push({ name: nameTok.t, norm: cart.norm.slice(i, k + 1), line: toks[i].line });
            depth = 0; i = k; continue;
          }
        }
      }
      depth++;
    } else if (t === '}') depth--;
  }
  return fns;
}
function shingles(norm, k = 4) {
  const set = new Set();
  for (let i = 0; i + k <= norm.length; i++) set.add(norm.slice(i, i + k).join(''));
  return set;
}
function jaccard(a, b) {
  if (!a.size || !b.size) return 0;
  let inter = 0; const [s, l] = a.size < b.size ? [a, b] : [b, a];
  for (const x of s) if (l.has(x)) inter++;
  return inter / (a.size + b.size - inter);
}

const byName = new Map();
for (const cart of carts) for (const fn of extractFns(cart)) {
  if (fn.norm.length < 30) continue;  // ignore trivial helpers
  let arr = byName.get(fn.name); if (!arr) byName.set(fn.name, arr = []);
  arr.push({ cart: cart.name, line: fn.line, norm: fn.norm, sh: shingles(fn.norm) });
}
const HOOK_CUTOFF = carts.length * 0.4;   // names in >40% of carts are engine hooks (draw/update/init), not copies
const DRIFT_LO = 0.6, DRIFT_HI = 0.999;   // the "copied then diverged" band
const drift = [];
for (const [name, defs] of byName) {
  if (defs.length < 2) continue;
  if (new Set(defs.map(d => d.cart)).size < 2) continue;
  if (defs.length > HOOK_CUTOFF) continue;   // draw/update/init — independent hooks, not drift
  // pairwise similarity; count pairs sitting in the drift band
  let identical = 0, pairs = 0, driftedPairs = 0;
  const driftSims = [];
  for (let a = 0; a < defs.length; a++) for (let b = a + 1; b < defs.length; b++) {
    const s = jaccard(defs[a].sh, defs[b].sh); pairs++;
    if (s >= DRIFT_HI) identical++;
    else if (s >= DRIFT_LO) { driftedPairs++; driftSims.push(s); }
  }
  if (driftedPairs === 0) continue;          // no near-miss copies → not interesting
  driftSims.sort((a, b) => a - b);
  const median = driftSims[driftSims.length >> 1];
  drift.push({
    name, copies: defs.length,
    driftedPairs, identicalPairs: identical, totalPairs: pairs,
    medianDriftSim: +median.toFixed(2),
    where: defs.map(d => `${d.cart}:${d.line}`).sort(),
  });
}
// rank by how much drift there is, then how tight the drifted copies are
drift.sort((a, b) => b.driftedPairs - a.driftedPairs || b.medianDriftSim - a.medianDriftSim);

// ---- output ---------------------------------------------------------------------
if (JSON_OUT) {
  console.log(JSON.stringify({
    carts: carts.map(c => c.name),
    config: { MIN_TOKENS, WINDOW, DRIFT_LO, DRIFT_HI, HOOK_CUTOFF },
    anchorSize: anchor.size,
    extraction: clusters.slice(0, TOP), drift,
  }, null, 2));
  process.exit(0);
}

if (!ONLY_DRIFT) {
  console.log(`\n=== EXTRACTION CANDIDATES — top ${TOP} (min ${MIN_TOKENS} norm-tokens, ≥2 carts) ===`);
  console.log(`(${clusters.length} clusters total; score = tokens × cart-spread; high anchorDensity = mostly API calls = "should use a header")\n`);
  clusters.slice(0, TOP).forEach((c, i) => {
    console.log(`#${i + 1}  ${c.tokens} tok × ${c.spread} carts  (score ${c.score}, anchorDensity ${c.anchorDensity})`);
    if (c.apiCalls.length) console.log(`    api: ${c.apiCalls.slice(0, 12).join(' ')}${c.apiCalls.length > 12 ? ' …' : ''}`);
    for (const o of c.occurrences) console.log(`      ${o.cart}.c:${o.l0}-${o.l1}`);
    console.log('');
  });
}

if (!ONLY_EXTRACT) {
  console.log(`\n=== DRIFT — same-named helpers that diverged (${drift.length} found) ===`);
  console.log(`(same fn name in ≥2 carts, similarity high-but-imperfect → a copy that drifted)\n`);
  for (const d of drift.slice(0, 40)) {
    console.log(`${d.name}()  ×${d.copies} carts  ${d.driftedPairs} drifted pairs (median sim ${d.medianDriftSim}), ${d.identicalPairs}/${d.totalPairs} identical`);
    console.log(`    ${d.where.join('  ')}`);
  }
}
console.log('');
