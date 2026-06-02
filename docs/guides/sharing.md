# Cart sharing ideas

Web builds already work — `build/cart.html + cart.js + cart.wasm` runs in any
browser. What's missing is a URL you can send someone.

---

## First, two different "web" goals — don't conflate them

1. **Share a finished game as a playable link** — build the cart to wasm on the desktop
   app, host the static files, send the URL. **No backend to run.** This is what the rest
   of this doc is about, and it's the right fit if you're not running servers.
2. **Edit + run a cart inside a browser tab** (a no-install browser IDE) — a different,
   much bigger problem: the browser itself has to *compile*. Out of scope here; see "goal
   B" in [`../design/cart-as-script.md`](../design/cart-as-script.md). It needs either a
   build server or an in-browser emscripten toolchain.

Running a *finished* cart in a browser is already solved — `studio.c`'s `PLATFORM_WEB`
build produces wasm that runs anywhere (verified with a `de_state` cart). For goal 1 the
only thing missing is **a URL**, which is just static hosting — no compiler-in-the-cloud
needed.

### Why "build server" sounds scary (and when it isn't)

A server that compiles carts is only complex when it's **public** — compiling untrusted C
from strangers needs sandboxing, resource limits, ops. A **personal** one (you + a few
trusted people) is trivial: it's the existing Build-for-web logic behind a ~30-line HTTP
wrapper, no sandboxing. And if you'd rather run no backend at all, you don't need either:
goal 1 (static publishing, below) covers sharing, and goal 2's no-backend route
(in-browser emscripten) is a research project for later. **Recommendation for a
non-backend setup: lean on static publishing.**

---

## Options explored

### Netlify Drop
Go to netlify.com/drop, drag the `build/` folder, get a public URL in ~30s.
No account needed for temporary links.
- ✓ Zero setup
- ✗ Manual, temporary link, not integrated with the editor

### itch.io
Create a free account, upload as an HTML5 game (zip the 3 build files).
Permanent page, looks good, standard for indie games.
- ✓ Permanent, shareable game page, familiar to players
- ✗ Manual upload, account required, not integrated with the editor

### surge.sh (most promising for editor integration)
Free static hosting CLI — no account needed, `npx surge` is all it takes.
Would generate a URL like `mycart.surge.sh` or `dreamengine-[name].surge.sh`.

Could be wired directly into the editor as a "Publish" button:
1. Build for web (already works, ~10s)
2. Run `npx surge build/ --domain [name].surge.sh`
3. Toast shows the URL, maybe copies to clipboard

Open questions:
- Do we prompt for a cart name / domain, or auto-generate one?
- Does surge.sh require login for persistent URLs? (yes, free account for custom domains)
- Surge has a free tier with unlimited projects — good enough for sharing carts

### Cloudflare Pages / GitHub Pages
More setup, better for permanent hosting of a known collection of carts.
Not a great fit for one-click sharing of individual user carts.

### Self-hosted dreamengine server
Full control — users upload to dreamengine.io (or similar), get a short URL,
can browse others' carts. The "PICO-8 splore" equivalent.
- ✓ Best user experience long-term
- ✗ Requires building + maintaining a server, storing wasm files, auth, etc.
- Probably the right end goal but months of work

> Note: this (public, multi-user) is the *hard* kind of server. A server that just
> *compiles* a cart for the browser-IDE goal (goal 2 above) is a separate thing — trivial
> if personal, hard only if public. Either way, neither is needed for plain link-sharing.

---

## Recommended path

**Short term:** document that users can drag `build/` to netlify.com/drop or
zip + upload to itch.io. No editor changes needed.

**Medium term:** add a "Publish" button using surge.sh. ~20 lines in `main.cjs`,
one button in the cart panel. Returns a URL in the toast.

**Long term:** a dreamengine-hosted sharing platform — users paste a URL in the
tutorials panel to load someone else's cart, and there's a public gallery.
