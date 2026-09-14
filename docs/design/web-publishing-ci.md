# Publishing to the web from CI, so an agent can build something you can hear

> **STATUS: READY TO BUILD (2026-09-14)** and the mechanism is committed
> ([`.github/workflows/publish-web.yml`](../../.github/workflows/publish-web.yml)), but it is
> **inert until two decisions are made**, both the owner's, both listed in §2. Until then every run
> still gates and still builds, and hands the site back as an artifact rather than publishing it.

## 1. The problem this solves

The desktop editor is the only place a cart normally runs. So an agent working through pull requests
can land a whole engine plus its showcase cart and nobody can hear any of it until someone sits at
the Mac. For a multi-day stretch of autonomous work, and for the
[`engine-reach-macro-mapping.md`](engine-reach-macro-mapping.md) forks specifically (which are
settled by ear, not by reading a table), that is the difference between progress and a pile of
unverified commits.

The pieces were already here and were simply never wired together:

- `tools/build-site.js <names>` turns carts into self-contained wasm pages.
- `runtime/raylib-web` is **vendored and committed**, so a runner needs no raylib build.
- `tools/build-all.js` compile-checks every cart and deliberately does not link raylib, so it runs on
  a bare Linux runner.
- `tools/web-audio-check.js` compiles the same engine native and wasm and compares the audio.

What was missing was CI: this repo had no `.github/workflows` at all.

## 2. The two blanks, and why they are not oversights

| Blank | What it is | Why it is yours |
|---|---|---|
| `vars.PUBLISH_REPO` | owner/name of the repo whose GitHub Pages serves the built site | It decides whether half-finished engines appear on a public shelf. See §3. |
| `secrets.PUBLISH_TOKEN` | a token or deploy key with push rights to that repo | Granting a workflow push rights to another repo is a security decision, not a config detail. |

With neither set, the workflow runs every gate, builds the site, uploads it as an artifact, and says
in the run summary exactly what to set. It never silently does nothing.

## 3. Where to publish: the actual choice

**The public gallery** (`NikkiKoole/dreamengine`, served at `nikkikoole.github.io/dreamengine/`) is
the existing target, and `tools/publish-cart.sh` already pushes there from a local `site/` checkout.
Pointing CI at it means work-in-progress engines land on the public shelf the moment they build.

**A separate preview repo** keeps the shelf clean at the cost of one more repo. Pages will serve any
repo, so this is a five-minute setup, and it is what I would pick for autonomous work: the gallery is
a curated thing and an auto-publisher is not curation.

Either way the workflow clones the target first, so `build-site.js` regenerates the gallery **over**
what is already published rather than dropping it.

## 4. The gates, and why each one is there

In order, cheapest first:

1. **`lint-carts.js` + `build-cart-index.js --check`** catch a malformed `de:meta` or a stale index
   before spending a compile on it.
2. **`build-all.js`** compile-checks the whole catalog. A publish is a good moment to notice that an
   engine change rotted an unrelated cart, and this is the only thing that sees that.
3. **`web-audio-check.js --quiet`** is the gate that is specifically about shipping to a browser.
   Emscripten's compiled DSP math is not guaranteed to match native clang (there is a documented
   `-ffast-math` trap), and **a brand-new engine is exactly where that would first show up**. It is
   skippable via a workflow input, but the input exists so that skipping is a stated choice rather
   than a silent omission.

`build-site.js` itself compiles each named cart with emcc, so a cart that does not build fails the
publish by construction. That is why there is no separate per-cart compile step.

## 5. The AFK loop

1. An agent lands an engine plus its showcase cart, and adds the cart's name to
   [`.github/publish-carts.txt`](../../.github/publish-carts.txt) **in the same PR**.
2. The merge to `master` triggers the workflow (it watches `tools/carts/**` and `runtime/**`).
3. The run summary prints the URL per cart.
4. You open it on a phone and listen.

`workflow_dispatch` also takes an explicit cart list, which is the path for "rebuild just this one",
and can be fired from the GitHub mobile app.

**Keep the list short.** Every entry is an emcc compile on every run.

## 6. What a phone listener needs from the cart, not from CI

Publishing a cart a phone cannot drive is a wasted round trip. Two requirements, both belonging to
the cart rather than to this workflow:

- **`node tools/mobile-lint.js <cart>` must pass.** It is the static report card for "can a phone
  play this".
- **Any A/B must be a tappable on-screen control**, not just a key, and the panel must say which
  route is live. This is why
  [`engine-reach-macro-mapping.md`](engine-reach-macro-mapping.md) §4 makes the mapping fork a
  runtime toggle instead of a compile-time `#define`: one build, both routes, judged with a thumb.

## 7. Deliberate limits

- **Web only.** Native, `.app` and iOS builds need signing identities and a Mac runner, and are not
  in scope here.
- **Emscripten is installed from source, not via a third-party action.** This job holds a push
  credential, so its dependency list is kept short and auditable. It is cached between runs.
- **One publish at a time** (`concurrency: publish-web`), or two runs race each other pushing the
  gallery.
- **This does not replace `publish-cart.sh`.** That remains the local path and is still the right
  tool when you are at the desk.

## See also

- [`sharing-channels.md`](sharing-channels.md) - the two-repo private-code/public-site split this
  publishes into
- [`engine-reach-macro-mapping.md`](engine-reach-macro-mapping.md) - §4, the forks that need an ear
- [`web-audio-parity.md`](web-audio-parity.md) - what the parity gate is actually checking
- [`engine-reach.md`](engine-reach.md) - the engine programme this exists to serve
