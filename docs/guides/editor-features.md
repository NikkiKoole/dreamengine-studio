# Editor features — what the editor can *do* (the capability index)

STATUS: LIVING (2026-07-20) — the discoverability counterpart to CLAUDE.md's `tools/` list, for
the **editor**. CLI tools are indexed in CLAUDE.md; editor *capabilities* lived only in `shell.js`
+ scattered design docs, so they were easy to miss (an agent nearly rebuilt the trailer builder
that already ships). This page is the one place that answers **"can the editor already do X?"** —
one line per capability, pointing at its design doc + where it lives in the UI. Keep it current
when an editor surface lands.

The editor is Electron + Vite (`make` / `cd editor && npm start`). The browser tab edits too, but
**▶ run** and the desktop-only actions (record, bake, builds) need the Electron window.

Structure follows the editor's own IA — **Make / Promote / Ship** ([`../design/editor-scopes-and-facets.md`](../design/editor-scopes-and-facets.md)).

## MAKE — author the cart

| capability | where | more |
|---|---|---|
| **Code editor** — CodeMirror 6, C syntax, autocomplete + hover + Cmd-click-a-symbol → help; inline clang error markers | Code tab | studioDocs.js drives the API docs |
| **Sprite editor** — 16×16 · 64 slots · pico32; pixel/fill/select/stamp/line/circ/rect + animation frames | Pixels tab | [`cart-authoring.md`](cart-authoring.md); code-drawn sprites → `tools/sprite-draw.js` |
| **Map editor** — tilemap over the sprite sheet | Pixels tab | — |
| **▶ run** — three backends: **native (clang)** default · **live (libtcc)** JIT hot-reload, state survives via `de_state()` · **web** (emcc → wasm) | ▶ button + Settings "run mode" | [`../design/cart-as-script.md`](../design/cart-as-script.md) |
| **Tutorials / carts gallery** — every registered cart, load to edit | Carts tab | generated from each cart's `de:meta` |

## PROMOTE — get the cart *seen* (the Promote tab)

The per-cart **media bin** — [`../design/promote-tab.md`](../design/promote-tab.md). Also part of the
🎬 **[video pipeline](../design/video-distribution.md)** breadcrumb.

| capability | where | more |
|---|---|---|
| **Clips & takes** — ● record a take, ▶ replay it, 🎬 bake it to a webm clip | Promote tab §A | takes live in `tools/clips/<cart>/`; [`cart-clips.md`](../design/cart-clips.md) |
| **Stills** — 📸 snapshot a mid-run frame | Promote tab §B | per-cart `editor/public/shots/<cart>/` |
| **Ratio variants incl. 9:16** — the **"bake at"** picker bakes a per-channel clip (16:9 · **9:16** · 1:1 · iPhone · iPad); a *resizable* cart **reflows to fill** the frame | Promote §A "bake at" | [`../design/export-ratios.md`](../design/export-ratios.md) — the full-bleed vertical clip `youtube-push` uploads |
| **Trailer builder** — a "humble CapCut over the `.reel`": timeline, clip library, reorder, transitions + durations at the joins, trim, speed, build → preview. Multi-cart | Promote / Apps → 🎬 trailer | [`../design/trailer-builder.md`](../design/trailer-builder.md); non-destructive (the `.reel` is committed text) |
| **Keyword research** — 🔑 aso-research (competition) + aso-suggest (demand), seeded from `de:meta` | Promote / Apps → 🔑 | [`../design/store-agents.md`](../design/store-agents.md) |
| **Find tribes** — 📣 the venues where this cart's tribe gathers, + a gift-first post scaffold | Promote §D | `tools/leads.js`; [`../design/demand-generation.md`](../design/demand-generation.md) |
| **Gallery link** — the deterministic web-gallery URL + copy | Promote §E | — |

## SHIP — publish the binary / the store surface

| capability | where | more |
|---|---|---|
| **⇪ Share popover** — per-cart exports, grouped by audience | topbar ⇪ Share | [`../design/share-panel.md`](../design/share-panel.md) |
| **Apps tab (ASO lab)** — app-less keyword lab + apps from `apps/*/app.json`; per-app: 📸 screenshots → 📄 press kit, 🍎/📱/🤖 Mac/iOS/Android builds, 📝 worksheet · 🔎 research · 💡 suggest · 🧩 compose · 🔬 analyze · 📊 score · ✅ lint · 🪞 check the `listing` | Apps tab | [`../design/store-agents.md`](../design/store-agents.md), [`../design/press-kit.md`](../design/press-kit.md) |

## Elsewhere in the window

- **Docs tab** — renders this `docs/` set (cross-linked) + a read-only **engine-source viewer** + the ★ generated pages (history, design board, reflections).
- **Settings tab** — screen/scale/cell/map dims (the `-D` compile flags), run mode, theme.

## Beyond the editor (CLI, but part of the same pipeline)

- **`tools/dress-clip.js`** — dress a baked clip into a 9:16 Short with **hand-typed on-screen text** in the letterbox bars (title card / hook / CTA). The "add text" step of record → bake → **dress** → post. Not yet a Promote-tab button (CLI for now). [`../design/export-ratios.md`](../design/export-ratios.md) "Dressed composite".
- **`tools/youtube-push.js`** — upload a Promote-baked clip (or an app reel) to YouTube as a Short. Consumes the Promote tab's **9:16 variant** for a full-bleed Short. [`../design/video-distribution.md`](../design/video-distribution.md).
- The full CLI toolbox is indexed in **CLAUDE.md** (the `tools/` list) — this page is its editor-side twin.
