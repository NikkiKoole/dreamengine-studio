import { defineConfig } from 'vite'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const here = path.dirname(fileURLToPath(import.meta.url))
const DOCS = path.resolve(here, '../docs')   // repo docs/ lives one level up from editor/

// Serve the repo's docs/ over the dev server so the in-editor "Docs" wiki view can
// fetch and render them live (no copy step, always fresh). Two routes:
//   GET /docs-list.json   → recursive list of *.md paths (for the sidebar nav)
//   GET /docs/<rel>.md    → the raw markdown
function serveDocs() {
  const listMarkdown = (dir, base = '') => {
    let out = []
    for (const name of fs.readdirSync(dir).sort()) {
      const abs = path.join(dir, name)
      const rel = base ? `${base}/${name}` : name
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

        if (url === '/docs-list.json') {
          let files = []
          try { files = listMarkdown(DOCS) } catch {}
          res.setHeader('Content-Type', 'application/json')
          res.end(JSON.stringify(files))
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
})
