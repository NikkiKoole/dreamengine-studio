# 0023 — Ship carts as apps (precompiled native), never the editor
Date: 2026-06-29 · Status: accepted

## Context
The Tinyjam/native-iOS exploration ([`product-notes-followup.md`](../design/product-notes-followup.md))
and the older [`packaging-distribution.md`](../design/packaging-distribution.md) read as in
tension. The packaging doc treats consumer distribution as nearly intractable because it
assumes the *thing shipped* is the dreamengine **editor** — and a consumer machine has no
`clang`, no Homebrew `libraylib.a`, no SDK headers, so "how does anyone compile/run a cart?"
has no good answer (and on iOS literally none — iOS can't JIT or spawn a compiler). The
followup, meanwhile, argues native iOS is tractable for the *product*. Both are right about
different products. This decision states which product we ship.

This is the product-line complement to [ADR-0020](0020-in-house-tool-curated-showcase.md)
(in-house tool, not a cart-making platform) — that ADR explicitly left the for-sale product
line out of scope; this one fills it.

## Decision
We ship **finished apps**, not the editor. A shippable unit is a cart, or a curated
**collection of carts** bundled into one app (with iOS extras around them — AUv3, StoreKit,
App Groups, safe-area/touch chrome). Every cart is **precompiled to a native binary on a dev
box before it leaves our machine.** The consumer downloads a *result*, never a toolchain and
never the ability to build carts.

The editor (`editor/`, the ▶ run button, `make-cart.js`, `play.js`) is and stays an
**in-house authoring tool on a dev box only.** It is never packaged for, sold to, or run by
an end user.

## Why
- It **dissolves the "real blocker"** in `packaging-distribution.md`. That doc's hard problem
  — get a compiler onto a consumer machine — only exists if we ship the build loop. We don't.
  Carts are baked native before distribution, so there is nothing to compile on-device. This
  is *exactly* why native iOS becomes tractable: no on-device JIT/compiler (the thing iOS
  forbids), which was the wasm-or-nothing wall.
- It matches how the tool is actually used (ADR-0020): we author in-house; the public gets
  curated *output*. Selling a music app is "a product made *with* the tool," not "opening the
  tool to others."
- It deletes the heaviest, riskiest packaging work (bundling a toolchain, an in-browser
  recompile path, a compile-strangers'-C server) as a **non-goal**, not an unsolved task.

## Consequences
- **`packaging-distribution.md`'s consumer-editor problem is a non-goal** — marked as such at
  the top of that doc so it stops getting re-derived. Its option 1 (bundle a toolchain) and
  option 2 (precompile only) collapse to: we always precompile; the user never builds.
- **The web/wasm gallery stays in scope** as the free funnel — it also ships *results*
  (playable precompiled wasm), not the editor, so it's consistent with this decision (and with
  ADR-0020's "watch + play, never contribute"). It is the try-before-you-buy layer; the App
  Store apps are the paid payoff.
- **The native-iOS/AUv3/StoreKit plan in `product-notes-followup.md` §2–§7 is the build plan**
  for the shippable side — precompiled C→native, callback-loop port, AUv3 extension per rack,
  in-house StoreKit 2 bridge, agentic CMake/xcodebuild/Fastlane pipeline.
- **Out of scope for good** (don't relitigate): shipping the editor to end users; any on-device
  or in-browser cart-build/recompile path for consumers; a public compile server.

> Where this decision sits in the wider picture: [`design/sharing-channels.md`](../design/sharing-channels.md) maps all three sharing channels (this ADR governs channel B, the App Store).
