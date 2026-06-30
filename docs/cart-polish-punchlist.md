# Cart polish punch-list — MIGRATED to per-cart `de:meta.todo[]`

> **2026-06-30: the per-cart items moved into the carts.** Every cart now owns its polish
> list in its `de:meta.todo[]` block (validated by `lint-carts.js`, not emitted to
> index.json). Query the whole backlog with **`node tools/cart-todos.js`** (`--grep <term>`,
> `--count`, `<cart>`). The per-cart entries are no longer maintained in this file — **the
> carts are the source of truth**; clear an item by deleting it from the cart that owns it.
> Touch-control items were migrated too (the touch-controls agent reads `de:meta.todo[]`
> when it starts on a cart). The shared design + plan for the onscreen joystick/d-pad lives in
> [`design/touch-controls.md`](design/touch-controls.md).

## What this list's framing notes asked for — and what shipped since

- **Per-cart metadata in the header** (¶3 below) → **SHIPPED**: the `de:meta` block + generated
  `index.json` (`build-cart-index.js`); design in [`design/cart-metadata.md`](design/cart-metadata.md).
- **A "todo" tag/field** (¶4) → **SHIPPED**: `de:meta.todo[]` + `cart-todos.js` (this migration).
- **Resolution/orientation marking** (¶5) → **SHIPPED**: `orientation` is derived into index.json.
- **More tags · status · curated collections** (¶6–¶8) → tracked in
  [`design/action-plan.md`](design/action-plan.md) Tier 1 (cart lifecycle + curated views); the
  `status` field already exists and is validated by `lint-carts.js`.
- **Stale thumbnail after run+escape** (¶12) → [`design/action-plan.md`](design/action-plan.md)
  Tier 0 "rebake stale thumbnails".
- **Systemic polish patterns** (cute pixel buttons, drag-capture rule, shared end-state,
  adventure z-ordering, generative-music retrofit, tween-not-teleport) → the recurring shared
  pieces are summarised in [`design/action-plan.md`](design/action-plan.md)'s cart-polish backlog.

---

## The maker's original framing notes (kept verbatim — the origin of the above)

Hi since i'm out of tokens i will do a run through all my carts, and note down things we should improve :
the goals is basically to have every cart be as good as it can be without thinking about whole new features, basically just polish. Some carts were made when we didnt have the same api we do now, so some things can be done much better now :


also another note, before mass changing all the carts and tripping over our everlasting issue with multiple agents working in index.json, we need a startegy to fix this, i think the idea became to add the stuff we have in the json in the doc header of the cart itself, but im not entirely sure how good of an idea is that , when we wnat to look through the json to find anything or search in the editor.
well we probably need to atack this first before starting mass editing.. lets brainstorm and test this

another idea i'm having while making this list, maybe we need another tag on the carts, where we can mark things left todo

another note: im seeing some carts that have other resolutions some square , tall etc, we need to mark this on the josn/header so we can find them separately

another thought, when we started this repo and had a few carts we added tags for games, i kinda feel we wnat way more tgs so we canmore easily find games , like text adverture, or vertical oriented or well you know lets analize the list and see what sort of tags we might want more

also:
showcase     = belongs on the public gallery
teaching     = useful because it explains one thing clearly
pillar       = future-system seed
archive      = historically important, not for browsing
retired      = superseded by a better cart
internal     = API/probe/debug only

Best of DreamEngine
Learn the API
Sound Toys
Living Worlds
Arcade Homages
Technical Labs
Retired / Historical


another thing i noticed, when i play/run a cart and escape the thumb is empty and needs a second or soe to show up again, why is taht? we havent baked a new screnshot fro the cart?

ok and sice we have a slightly new north star, our collaboration is first, being tutorially not anymore, first off check and reason to think if we need more tools that will help you do this work below.

basically im walking through the carts selecting 'recently updated' backwards. (more note bass is an outlier becasue i just built it)
