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

> **Trigger phrase:** when the maker says **"update mipolai website"** (or "update the website" /
> "put this on the site"), it means *this* — edit `content/`, rebuild `docs/`, commit both, push;
> usually to add a `makes` page showcasing a dreamengine cart (see "Showcasing a dreamengine cart"
> below). The repo has its own `CLAUDE.md` that mirrors this; for a real editing session, start
> Claude Code *in* that repo.

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

> **Worked example:** `content/makes/tinyacidjam.md` was added end-to-end via exactly the above
> (shot + hero video/poster + full SEO frontmatter, landed as a draft). It's the reference for
> the next page.

### Showcasing a dreamengine cart (the recurring case)

The most common reason to touch this site from a dreamengine session: **you shipped or polished
a cart and want a `makes` page for it.** This is a named path, not a re-derivation of the generic
steps above — it reuses artifacts dreamengine already generates.

1. **Get a hero clip + poster.** Bake a looping clip of the cart with `make-gif.js` (park a good
   input track at `tools/clips/<cart>/NN-label.*` first so it's reproducible), ask for `mp4`, then
   pull a poster frame:
   ```sh
   # in dreamengine/  — --recipe reads tools/clips/<cart>/<NN-label>.{script,beats,rec}
   #                     and writes editor/public/clips/<cart>/<NN-label>.mp4 (audio muxed in)
   node tools/make-gif.js <cart> --recipe <NN-label> --format mp4
   ffmpeg -ss 3 -i editor/public/clips/<cart>/<NN-label>.mp4 -frames:v 1 build/<cart>-frame.png
   ```
   (There is no `--mp4` flag — it's `--format mp4`, and `--recipe` already decides the output path,
   so don't pass `--out` as well. A dumped `play.js --dump` frame works as the poster source too.)
   No clip worth looping? A single crisp still is fine — the baked `.cart.png` thumbnail or a
   `play.js --dump` frame, brought in via `optimize-image.sh … shot` (crisp pixels).
2. **Copy the frontmatter from `content/makes/tinyacidjam.md`** — it's the end-to-end cart reference
   (shot + hero video/poster + full SEO + the playable embed). Change
   `title`/`slug`/`timestamp`/`metaDescription` etc. (That page is the **acidcandy** cart: the
   website slug does *not* have to match the cart name.)
3. **Land it as `draft=true`**, drop the `.mp4` + poster into `docs/assets/images/` (see the
   `<video>` pattern above), rebuild (`lua main.lua`), eyeball at localhost:8080.
4. **Ship the playable too** — see the next section. A makes page without it is a video of a
   thing nobody can touch, and the playable's URL is also the link that gets *posted* places.
5. **Flip `draft=false`, rebuild, commit both trees, push** — that's the commit that also adds the
   `makes/index.html` card + sitemap entry.

Prose voice for the write-up: [`guides/voice.md`](guides/voice.md) (synthesized from this
same site). Keep it short and honest — a stranger should get what the cart *is* and why it's fun.
**Do draft the copy, but expect Nikki to rewrite it** — the page needs words to react to, an empty
one is harder to start from. So keep the draft lean (short paragraphs rewrite more easily than
polished ones), get the FACTS right — exact key bindings, real preset names, counts read from the
cart source — because those survive the rewrite when the sentences don't, and never hold a page
back waiting for copy.

### Ship the playable — the `/play/<slug>/` link

The wasm build serves **two** jobs: the `<iframe>` embed on the makes page, *and* a standalone
shareable page at `mipolai.com/play/<slug>/` with nothing on it but the cart. That standalone URL
is what actually gets posted to a subreddit or a forum (Pedalboard went out to r/guitarpedals as
`mipolai.com/play/pedalboard/` on 2026-07-28). `docs/play/<slug>/` is the one directory under
`docs/` that the Lua build does *not* generate — it's a dreamengine build copied in by hand.

```sh
# in dreamengine/ — builds worklet + plain shells into site/<cart>/ (~2 min of emcc)
node tools/build-site.js <cart>

# then, in the website repo
cd /Users/nikkikoole/Projects/love/nikkikoole.github.io
mkdir -p docs/play/<slug>
cp /Users/nikkikoole/Projects/dreamengine/site/<cart>/* docs/play/<slug>/
tools/standalone-play.sh <slug> "<Title>" "<one-line description>" [<og-image.png>]
```

- **`standalone-play.sh` is not optional, and it must be re-run after every copy.** A raw copied
  build is an unbranded artifact: tab title from the engine shell, no description, no OpenGraph
  tags — so the link previews as a blank card wherever it's pasted. The script injects title,
  description, canonical, OG/Twitter card, PWA name and a title guard between `mipolai:standalone`
  markers, and is idempotent. Copying a fresh build over the top **reverts all of it**.
- The embed for the makes page:
  ```html
  <iframe src="../play/<slug>/" style="width:100%;max-width:720px;aspect-ratio:16/10;border:0;border-radius:6px;background:#000" allow="autoplay; fullscreen" title="<Title>, playable"></iframe>
  ```
  Sound needs one user gesture (browsers block autoplay), so say so next to the frame. Add
  `?audio=plain` only if silence gets reported — the shell picks its own audio backend and only
  forces the ScriptProcessor path on genuinely old iOS.
- **Why the tab title used to flip to "dreamengine" a beat after load:** raylib's `InitWindow()`
  writes its window title into `document.title`, and `build-site.js` never passed
  `-DDE_WINDOW_TITLE`, so every web cart overwrote its own HTML `<title>` with the `studio.c`
  default. Fixed 2026-07-28 (it now bakes the cart's `de:meta` title, like the editor does for
  exports). Carts built *before* that keep doing it until they're republished; the script's title
  guard covers the standalone page either way.
- Phone check before posting, since most traffic is mobile: `node tools/mobile-lint.js <cart>`.
  Landscape-only carts are fine (the shell shows a rotate hint), but watch for `tiny-target` on a
  control that gates the whole cart.

### Working on it from a dreamengine session

- Bash cwd resets back to dreamengine after every call — do website work as
  `cd /Users/nikkikoole/Projects/love/nikkikoole.github.io && <cmd>` inside one call.
- For a real editing session (not a quick touch-up), consider starting Claude Code *in* the
  website dir instead — cwd just works and dreamengine's rules don't load.
- Deeper notes: that repo's `README.md`, `PLAN.md`, `TODO.md`.
