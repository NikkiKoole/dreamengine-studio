import { defineConfig } from 'vite'
import fs from 'node:fs'
import path from 'node:path'
import { execFileSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'

const here = path.dirname(fileURLToPath(import.meta.url))
const DOCS = path.resolve(here, '../docs')       // repo docs/ lives one level up from editor/
const RUNTIME = path.resolve(here, '../runtime') // engine sources, for the read-only viewer
const REPO = path.resolve(here, '..')            // repo root, for git queries

// Map each cart .cart.png to two git-history dates: when it was first ADDED
// (--diff-filter=A) and when it was last touched (most recent commit, = a rebake).
// Powers the carts tab "Newest/Oldest" (added) and "Recently updated" sorts. All
// derived from git, so nothing is stored in index.json — no shared-file contention
// when several agents rebake carts in parallel. Cached with a short TTL (the two
// git-log scans cost ~300ms total): live enough that a freshly-committed rebake
// shows up on the next carts-tab open, without re-shelling on every request.
let cartDatesCache = null, cartDatesAt = 0
const CART_DATES_TTL_MS = 3000
function cartDates() {
  if (cartDatesCache && Date.now() - cartDatesAt < CART_DATES_TTL_MS) return cartDatesCache
  const dates = {}   // file -> { added, updated } (ISO commit dates)
  const scan = (args, field) => {
    try {
      const out = execFileSync('git', args, { cwd: REPO, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 })
      let cur = null
      for (const line of out.split('\n')) {
        if (/^C\d{4}-/.test(line)) { cur = line.slice(1); continue }
        const m = line.match(/([^/\s]+\.cart\.png)$/)
        if (!m || !cur) continue
        const f = m[1]
        if (!dates[f]) dates[f] = {}
        if (!dates[f][field]) dates[f][field] = cur   // log is newest-first; keep the first (= most recent) hit
      }
    } catch {}
  }
  // added: first time each .cart.png entered the tree; updated: most recent commit touching it
  scan(['log', '--diff-filter=A', '--format=C%cI', '--name-only', '--', 'editor/public/carts/'], 'added')
  scan(['log', '--format=C%cI', '--name-only', '--', 'editor/public/carts/'], 'updated')
  // touched: working-tree mtime of a cart's tools/carts/<name>.c — but ONLY when that source
  // has uncommitted changes (git sees it dirty). This is what makes the cart you're actively
  // hacking on (edited but not yet re-baked/committed) float to the top of "recently updated",
  // while clean carts keep their commit-based order (so a fresh clone isn't scrambled by
  // checkout mtimes). Folded into the sort as max(updated, touched) on the client.
  try {
    const st = execFileSync('git', ['status', '--porcelain', '--', 'tools/carts/'],
                            { cwd: REPO, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 })
    for (const line of st.split('\n')) {
      const m = line.match(/tools\/carts\/([^/\s]+)\.c$/)   // top-level carts only (skip book/ etc.)
      if (!m) continue
      const f = `${m[1]}.cart.png`
      try {
        const mtime = fs.statSync(path.join(REPO, 'tools/carts', `${m[1]}.c`)).mtimeMs
        if (!dates[f]) dates[f] = {}
        dates[f].touched = new Date(mtime).toISOString()
      } catch {}
    }
  } catch {}
  cartDatesCache = dates
  cartDatesAt = Date.now()
  return dates
}

// Serve the repo's docs/ over the dev server so the in-editor "Docs" wiki view can
// fetch and render them live (no copy step, always fresh). Three routes:
//   GET /docs-list.json     → recursive list of *.md paths (for the sidebar nav)
//   GET /docs/<rel>.md      → the raw markdown
//   GET /runtime-src/<f>.h  → a runtime/ source file (the cmd-click include viewer);
//                             flat names only — no path separators, .h/.c only
function serveDocs() {
  const EXCLUDE = new Set(['guides/cart-specs'])
  const listMarkdown = (dir, base = '') => {
    let out = []
    for (const name of fs.readdirSync(dir).sort()) {
      const abs = path.join(dir, name)
      const rel = base ? `${base}/${name}` : name
      if (EXCLUDE.has(rel)) continue
      const stat = fs.statSync(abs)
      if (stat.isDirectory()) out = out.concat(listMarkdown(abs, rel))
      else if (name.endsWith('.md')) out.push(rel)
    }
    return out
  }

  // — unified docs search: rank every .md by a query, with per-line snippets —
  // an mtime-keyed cache so re-querying while editing only re-reads changed files.
  const searchCache = new Map()   // rel -> { mtime, title, group, lines: [{n, raw, lower, heading}] }
  const indexDoc = (rel) => {
    const abs = path.join(DOCS, rel)
    const mtime = fs.statSync(abs).mtimeMs
    const hit = searchCache.get(rel)
    if (hit && hit.mtime === mtime) return hit
    const raw = fs.readFileSync(abs, 'utf8')
    const rawLines = raw.split('\n')
    const lines = rawLines.map((l, i) => ({ n: i + 1, raw: l, lower: l.toLowerCase(), heading: /^#{1,6}\s/.test(l) }))
    const h1 = rawLines.find(l => /^#\s+/.test(l))
    const title = h1 ? h1.replace(/^#\s+/, '').trim()
                     : rel.split('/').pop().replace(/\.md$/i, '').replace(/[-_]/g, ' ')
    const group = rel.includes('/') ? rel.slice(0, rel.indexOf('/')) : 'guide'
    const entry = { mtime, title, group, lines }
    searchCache.set(rel, entry)
    return entry
  }
  const searchDocs = (query) => {
    const terms = query.toLowerCase().split(/\s+/).filter(Boolean)
    if (!terms.length) return []
    const phrase = query.toLowerCase().trim()
    const wantPhrase = phrase.includes(' ')
    const results = []
    for (const rel of listMarkdown(DOCS)) {
      let entry; try { entry = indexDoc(rel) } catch { continue }
      const titleLower = (entry.title + ' ' + rel).toLowerCase()
      let score = 0
      const seen = new Array(terms.length).fill(false)
      const matched = []
      for (const ln of entry.lines) {
        let lineScore = 0
        terms.forEach((t, ti) => {
          let idx = ln.lower.indexOf(t), c = 0
          while (idx !== -1) { c++; idx = ln.lower.indexOf(t, idx + t.length) }
          if (c) { seen[ti] = true; lineScore += 1 + Math.log2(c + 1) }   // freq, log-damped
        })
        if (!lineScore) continue
        if (ln.heading) lineScore += 8                                    // headings = doc skeleton
        if (wantPhrase && ln.lower.includes(phrase)) lineScore += 10      // exact phrase
        score += lineScore
        matched.push({ n: ln.n, text: ln.raw.replace(/^#{1,6}\s+/, '').trim().slice(0, 240), heading: ln.heading, s: lineScore })
      }
      if (!matched.length) continue
      terms.forEach(t => { if (titleLower.includes(t)) score += 40 })     // title/filename = big boost
      if (seen.every(Boolean)) score += 20                               // all query words present
      matched.sort((a, b) => b.s - a.s || a.n - b.n)
      results.push({ path: rel, title: entry.title, group: entry.group, score,
        lines: matched.slice(0, 8).map(({ n, text, heading }) => ({ n, text, heading })) })
    }
    results.sort((a, b) => b.score - a.score)
    return results.slice(0, 30)
  }

  return {
    name: 'serve-docs',
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        const url = decodeURIComponent((req.url || '').split('?')[0])

        if (url === '/carts/dates.json') {
          res.setHeader('Content-Type', 'application/json')
          res.end(JSON.stringify(cartDates()))
          return
        }

        if (url === '/docs-list.json') {
          let files = []
          try { files = listMarkdown(DOCS) } catch {}
          res.setHeader('Content-Type', 'application/json')
          res.end(JSON.stringify(files))
          return
        }

        if (url === '/docs-search') {
          let out = []
          try { out = searchDocs(new URL('http://x' + req.url).searchParams.get('q') || '') } catch {}
          res.setHeader('Content-Type', 'application/json')
          res.end(JSON.stringify(out))
          return
        }

        if (url.startsWith('/runtime-src/')) {
          const name = url.slice('/runtime-src/'.length)
          // strict whitelist: a flat .h/.c filename, nothing that can escape runtime/
          if (/^[A-Za-z0-9_.-]+\.(h|c)$/.test(name)) {
            const abs = path.join(RUNTIME, name)
            if (fs.existsSync(abs) && fs.statSync(abs).isFile()) {
              res.setHeader('Content-Type', 'text/plain; charset=utf-8')
              fs.createReadStream(abs).pipe(res)
              return
            }
          }
          res.statusCode = 404
          res.end('not found')
          return
        }

        if (url.startsWith('/docs/')) {
          // resolve safely inside DOCS — reject any path that escapes it
          const rel = url.slice('/docs/'.length)
          const abs = path.resolve(DOCS, rel)
          if (abs.startsWith(DOCS) && fs.existsSync(abs) && fs.statSync(abs).isFile()) {
            // .html (the generated history page) renders in an iframe; everything
            // else is markdown served raw for the in-editor wiki renderer.
            const html = abs.endsWith('.html')
            res.setHeader('Content-Type', html ? 'text/html; charset=utf-8' : 'text/plain; charset=utf-8')
            fs.createReadStream(abs).pipe(res)
            return
          }
        }

        next()
      })
    },
  }
}

export default defineConfig({
  plugins: [serveDocs()],
  server: {
    warmup: {
      // Pre-transform the entry chain so Electron's first page load is instant
      // rather than waiting for Vite to transform each import on demand.
      clientFiles: [
        './src/shell.js',
        './src/main.js',
        './src/sprite-editor.js',
        './src/map-editor.js',
        './src/studioDocs.js',
        './src/settings.js',
      ],
    },
  },
})
