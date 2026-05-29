# Cart sharing ideas

Web builds already work — `build/cart.html + cart.js + cart.wasm` runs in any
browser. What's missing is a URL you can send someone.

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

---

## Recommended path

**Short term:** document that users can drag `build/` to netlify.com/drop or
zip + upload to itch.io. No editor changes needed.

**Medium term:** add a "Publish" button using surge.sh. ~20 lines in `main.cjs`,
one button in the cart panel. Returns a URL in the toast.

**Long term:** a dreamengine-hosted sharing platform — users paste a URL in the
tutorials panel to load someone else's cart, and there's a public gallery.
