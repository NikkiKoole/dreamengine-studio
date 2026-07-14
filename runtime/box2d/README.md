# Box2D v3 (vendored)

**Box2D v3.1.1** — Erin Catto's rigid-body physics engine, the **pure-C** rewrite
(2024). Vendored here because it has *zero* OS dependencies (no windowing, GL, or
I/O — just math), so the same source compiles through every toolchain this repo
ships. MIT licensed (see `LICENSE`).

- **Source:** https://github.com/erincatto/box2d — tag `v3.1.1`
- **What's here:** `include/` (6 public headers) + `src/` (35 `.c` + internal headers),
  copied **unmodified**. Dropped: `samples/`, `test/`, `benchmark/`, `docs/`,
  `extern/`, `shared/`, the CMake build (we don't use it).
- **Do not hand-edit** these files — to update, re-copy `include/` + `src/` + `LICENSE`
  from a newer tag so the diff is a clean vendor bump.

## Building

Don't link a prebuilt `.a` — a static archive is locked to one OS+arch (same reason
raylib ships as one lib per platform). Build per target from this source:

```bash
tools/build-box2d.sh --check     # macOS + drop-box smoke test
tools/build-box2d.sh --win       # mingw-w64
tools/build-box2d.sh --wasm      # emcc
tools/build-box2d.sh --ios       # iphoneos arm64
```

Output lands in `build/box2d/<target>/libbox2d.a` (gitignored). A cart links it with
`-I runtime/box2d/include  build/box2d/<target>/libbox2d.a`.

Full integration notes, the four-target compile evidence, and the coordinate-mapping +
determinism gotchas: [`docs/design/box2d-integration.md`](../../docs/design/box2d-integration.md).
Why rigid bodies get a vendored lib while fluids get a cart:
[`docs/design/physics-notes.md`](../../docs/design/physics-notes.md).
