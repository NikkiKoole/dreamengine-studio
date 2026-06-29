# iOS milestone screenshots — the visual changelog

Kept on purpose: one screenshot per spike/milestone as it first worked, so the journey from
"empty toolchain" to a shipping app is visible. When a spike lands, copy its simulator shot here as
`spikeN-<label>.png` (or `YYYY-MM-DD-<label>.png` for later app milestones) and add a row below.

Capture tip: screenshot **~1.5s after launch**, not immediately — `simctl io … screenshot` taken too
soon catches the iOS launch-zoom animation (washed-out, wallpaper showing through). See `../build.sh`.

| Shot | Date | What it first proved |
|---|---|---|
| `spike0-hello-toolchain.png` | 2026-06-29 | The agentic toolchain end-to-end: xcodegen → xcodebuild (no signing) → simulator boot/install/launch. A SwiftUI hello-world. |
| `spike1-canvas-loop.png` | 2026-06-29 | The software-canvas render path: C fills an RGBA framebuffer → CGImage → `CADisplayLink`-driven `de_update(t)` (the inverted loop) → crisp lo-fi pixels on screen. Pink ball animates; no GL/ANGLE. |
