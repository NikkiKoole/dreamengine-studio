# iOS build plan — the spike ladder + the reusable scaffold

STATUS: IN PROGRESS (2026-06-29). Spike 0 (toolchain) ✅ done. This is the *execution*
companion to the strategy docs — it tracks what's actually been built and proven, vs the
parked product thinking in those:

- **What & why we ship** → [`../decisions/0023-ship-carts-as-apps-not-the-editor.md`](../decisions/0023-ship-carts-as-apps-not-the-editor.md):
  finished apps (a cart, or a curated collection), **precompiled native on a dev box**; never the editor.
- **Product strategy** (monetization, AUv3 catalog, agentic Fastlane pipeline, StoreKit 2) →
  [`product-notes-followup.md`](product-notes-followup.md) §2–§7.
- **The music product** the apps host → [`tinydaws.md`](tinydaws.md) (Tinyjam racks).

This doc owns the *how-do-we-actually-build-it* ladder. The live scaffold lives in the repo at
[`../../ios/`](../../ios/) (its `README.md` is the operational how-to; this is the rationale + roadmap).

## Decisions settled (2026-06-29)

- **Project generator: xcodegen.** A 7-line `project.yml` is the only hand-edited project file;
  `ios/build.sh` regenerates the `.xcodeproj` (never committed, never opened to manage files). Chosen
  over `cmake -G Xcode` for being lighter when juggling app + AUv3-extension targets, and the iOS
  community default for terminal/agentic work.
- **First target: a dead-simple hello-world** (proves the toolchain before any rack). ✅ done.
- **Render path: software canvas → on-screen texture, initially.** The cart draws into a CPU
  framebuffer (the existing `det-probes/` software rasterizer, already bit-identical across
  arm64/x86/wasm); iOS just uploads it as a texture and draws one fullscreen quad. **No GL, no ANGLE,
  no raylib-on-iOS** — which dodges the single biggest porting risk and fits the own-the-stack ethos.
  - **HW alternative, investigated and deferred:** [`ghera/raylib-iOS`](https://github.com/ghera/raylib-iOS)
    is a live (monthly-synced, last release 2026-06-28) fork on raylib 5.5/6.0 with prebuilt
    Apple-Silicon ANGLE, an Xcode template, and the `ios_ready/update/destroy` callback loop already
    solved — plus an in-flight *official* iOS PR ([raylib #5881](https://github.com/raysan5/raylib/pull/5881),
    building on [#3880](https://github.com/raysan5/raylib/pull/3880)). It's more turnkey than the
    brainstorm assumed, but pulls a large ANGLE/Chromium dependency and self-labels "experimental, not
    production-ready." **Revisit if** a rack needs full GL fidelity or the SW blit can't hold 60fps.
  - This supersedes the "spike 1 = render bake-off" plan: we go SW first to keep momentum; the bake-off
    becomes a later contingency, not a blocker.

## The spike ladder

Each spike is a small throwaway that kills one unknown. Riskiest cheap-thing first. Spikes 0–6 run
**simulator-only: zero code-signing, zero Apple-account interaction** — all free. Only spike 7
(AUv3, which needs a host) forces a physical device + signing.

| # | Spike | Proves | Needs | Status |
|---|---|---|---|---|
| 0 | xcodegen + hello-world → simulator (the whole agentic loop) | toolchain works end-to-end | sim | ✅ **done** (`ios/`) |
| 1 | software-canvas framebuffer → on-screen texture, driven by the iOS callback loop | the render path + loop inversion | sim | next |
| 2 | audio: `sound.h` filling a CoreAudio render callback | the audio path | sim | — |
| 3 | save: a `save_bytes` blob in the Documents dir via an Obj-C path bridge | the save layer | sim | — |
| 4 | StoreKit 2 + a local `.storekit` config (StoreKitTest): buy / entitlements / restore, queried from C | the IAP model, no account/network | sim | — |
| 5 | App Group: app writes `unlocked:<rack>`, a second target reads it | entitlement sharing for AUv3 | sim | — |
| 6 | CloudKit sync of a saved tinyjam across devices (native-only nicety) | free cross-device sync | sim | — |
| 7 | minimal AUv3 extension makes sound in a host | the killer feature | **device + signing** | — |

## The reusable loop (what "add an app" becomes)

`ios/` is a toolkit, not a per-app project. Today it's the hello-world; the durable shape is:
`project.yml` (targets) + `Sources/` (the Swift/Obj-C shell + the cart's C) + `build.sh` (one command:
generate → build → boot → install → launch → screenshot). Adding an app later ≈ swap the cart C,
append a target + module id, re-run `build.sh` — the same "swap a `.c`" loop the rest of dreamengine
already uses, extended to iOS.

## Open / next

- **Spike 1**: pick the on-screen-texture mechanism (Metal `CAMetalLayer` + one textured quad is the
  60fps-correct choice; a `CGImage`/`UIImageView` updated by `CADisplayLink` is the 20-minute version
  to prove the pixels first, then upgrade). Wire the `ios_ready/update/destroy` callbacks around it.
- Then spikes 2–4 (audio, save, StoreKit-local) — all still simulator-free-tier.
- `ios/` is a new top-level dir; if it grows, give it a one-line entry in the `docs/README.md` layout
  tree (and CLAUDE.md project-structure) so it stays discoverable.
