import { defineConfig } from 'vite'
import fs from 'node:fs'
import path from 'node:path'
import { execFileSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'

const here = path.dirname(fileURLToPath(import.meta.url))
const DOCS = path.resolve(here, '../docs')       // repo docs/ lives one level up from editor/
const RUNTIME = path.resolve(here, '../runtime') // engine sources, for the read-only viewer
const REPO = path.resolve(here, '..')            // repo root, for git queries

// Map each cart .cart.png to the commit date it was first added (git --diff-filter=A).
// Powers the carts tab "Newest/Oldest" sort. Cached for the dev-server lifetime
// (cheap to recompute on restart; new carts during a session need a restart to date).
let cartDatesCache = null
function cartAddedDates() {
  if (cartDatesCache) return cartDatesCache
  const dates = {}
  try {
    const out = execFileSync('git', [
      'log', '--diff-filter=A', '--format=C%cI', '--name-only', '--', 'editor/public/carts/',
    ], { cwd: REPO, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 })
    let cur = null
    for (const line of out.split('\n')) {
      if (/^C\d{4}-/.test(line)) { cur = line.slice(1); continue }
      const m = line.match(/([^/\s]+\.cart\.png)$/)
      if (m && cur && !dates[m[1]]) dates[m[1]] = cur   // log is newest-first; keep most recent A event
    }
  } catch {}
  cartDatesCache = dates
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

  return {
    name: 'serve-docs',
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        const url = decodeURIComponent((req.url || '').split('?')[0])

        if (url === '/carts/dates.json') {
          res.setHeader('Content-Type', 'application/json')
          res.end(JSON.stringify(cartAddedDates()))
          return
        }

        if (url === '/docs-list.json') {
          let files = []
          try { files = listMarkdown(DOCS) } catch {}
          res.setHeader('Content-Type', 'application/json')
          res.end(JSON.stringify(files))
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
            res.setHeader('Content-Type', 'text/plain; charset=utf-8')
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
