# Other projects on this machine

A small **hub** for repos *other than dreamengine* that get worked on from this machine —
so an agent in a dreamengine session can figure out how to touch them without re-discovering
the whole layout each time. This is deliberately a pointer doc: the operational detail for
each project lives in that project's own repo (README/PLAN/TODO); here we keep just enough to
get in and out.

> Not part of the dreamengine docs "genre" tree — it points *outward*. Paths below are
> absolute filesystem paths, not repo-relative links (they live in other repos), so the doc
> linters don't try to resolve them.

Add a new `##` section per project as more of them start needing this.

---

## mipolai.com — the personal website

- **Repo:** `/Users/nikkikoole/Projects/love/nikkikoole.github.io`
- **What:** a hand-rolled **Lua static-site generator**. Write Markdown in `content/`, run the
  build, it renders HTML into `docs/`, which **GitHub Pages serves** (published at mipolai.com).
- **Origin:** `git@github.com:NikkiKoole/nikkikoole.github.io.git` (branch `master`).
- **Prereqs (present on this machine):** `lua` at `/opt/homebrew/bin/lua`; `love` aliased to
  the LÖVE app.

### Make a new page

1. Create `content/<section>/<slug>.md` (existing sections: `about`, `apps`, `blog`, `makes`,
   `mipo`, `stuff`, `tinyjam`; a top-level page is `content/<slug>.md`). **Copy the frontmatter
   from an existing page** rather than typing it fresh — `content/makes/achtbaan.md` is the full
   pattern (incl. SEO), `content/blog/vegetation.md` is the minimal one.
2. Frontmatter fields:
   - `timestamp=` unix epoch seconds (`date +%s`) · `date='DD Mon YYYY'` · `title='…'`
   - `draft=false` **publishes** it; `draft=true` keeps it out of the build
   - `thumb=` / `score=` / `order=` — listing thumbnail + sort weight within a section
   - `meta=true` + `metaDescription` / `metaImg` / `metaUrl` — SEO/OpenGraph tags (see achtbaan)
3. Body is plain Markdown (raw HTML like `<iframe>` is allowed — see achtbaan's YouTube embed).

### Add an image

```sh
cd /Users/nikkikoole/Projects/love/nikkikoole.github.io
tools/optimize-image.sh <src> <name> [photo|shot] [maxwidth]
```

`photo` (default) → WebP ~900px for real photographs; `shot` → colour-quantized PNG ~1000px for
UI/pixel screenshots (crisp text). Writes into `docs/assets/images/` and prints the exact
`![](...)` line to paste. Never upscales — and the width is a **max, not a target**: hand it a
small PNG (e.g. a 1280×800 native cart frame at ~25KB) and it keeps it as-is, which is exactly
right for crisp pixels.

### Add a looping video (the `<video>` pattern)

`optimize-image.sh` is images-only — video + poster go in by hand. The house pattern (see
`content/makes/puppetmaker.md`) is a muted autoplay loop with a WebP poster:

```html
<video autoplay loop muted playsinline poster="../assets/images/NAME-poster.webp"
       style="width:640px;max-width:100%"><source src="../assets/images/NAME.mp4" type="video/mp4"></video>
```

Drop the `.mp4` into `docs/assets/images/` and make the poster from a still frame:
`ffmpeg -i frame.png -c:v libwebp -q:v 80 docs/assets/images/NAME-poster.webp`. For a dreamengine
cart, pull a hero frame from a baked clip: `ffmpeg -ss 3 -i <clip>.mp4 -frames:v 1 frame.png`.

### Build + preview locally (nothing is published until you push)

- `lua run.lua` — serves `docs/` at **http://localhost:8080** *and* rebuilds on every save.
- `lua main.lua` — one-shot build.

### Publish (Pages serves `/docs` on `master`)

1. edit `content/`
2. **rebuild `docs/`** (`lua main.lua`, or leave `run.lua` running)
3. commit **both** the `content/` change **and** the regenerated `docs/*.html`, then push.

- **Gotcha — the stale-site trap:** committing a `content/` edit *without rebuilding `docs/`*
  publishes the OLD HTML. Always rebuild first, and stage both trees.
- **The diff is usually SMALL, not large** (the README overstates this): the build is
  **idempotent** — it rewrites every page but git only sees bytes that actually changed. Adding
  one page touches just that page's `.html` (+ the listing/sitemap once it's published, see
  drafts below). A *big* diff only happens when you change a **template or shared chrome**, which
  legitimately regenerates every page.

### Drafts — preview before you list it

`draft=true` still **renders the page's own `.html`** (reachable by direct URL), but keeps it
**out of the section listing and the sitemap**. So a new draft page shows up in git as *only*
its one `.html` + assets. Flip to `draft=false` and rebuild → that's the commit that also touches
`makes/index.html` (the card) and `sitemap`. Workflow: land it as a draft, eyeball it locally,
then flip + rebuild + commit to publish.

### SEO checklist (verify, don't assume)

`meta=true` + `metaDescription`/`metaImg`/`metaUrl` wire the `<title>`, `<meta description>`,
canonical, OpenGraph (`og:*`) and Twitter (`summary_large_image`) tags. Confirm after a build:
`grep -iE "<title>|og:|twitter:|canonical|description" docs/<section>/<slug>.html`.

> **Worked example:** `content/makes/acidcandy.md` was added end-to-end via exactly the above
> (shot + hero video/poster + full SEO frontmatter, landed as a draft). It's the reference for
> the next page.

### Working on it from a dreamengine session

- Bash cwd resets back to dreamengine after every call — do website work as
  `cd /Users/nikkikoole/Projects/love/nikkikoole.github.io && <cmd>` inside one call.
- For a real editing session (not a quick touch-up), consider starting Claude Code *in* the
  website dir instead — cwd just works and dreamengine's rules don't load.
- Deeper notes: that repo's `README.md`, `PLAN.md`, `TODO.md`.
