# Vendored libtcc (arm64 macOS)

Prebuilt from TinyCC **mainline** (the `mob` branch — the stable 0.9.27 release is
x86-only and deprecated). Used by the **live (libtcc) backend**: studio.c compiled with
`-DDE_TCC` JIT-compiles the cart in-process for instant hot-reload. See
`docs/design/cart-as-script.md`.

- `libtcc.dylib` — the compiler as a shared lib (install_name `@rpath/libtcc.dylib`;
  built as a dylib so its internal globals — e.g. `load` — don't clash with studio's).
- `libtcc.h` — the API header (studio.c includes it).
- `tcclib/` — CONFIG_TCCDIR: `libtcc1.a` (compiler runtime) + `include/` (tcc's own
  freestanding headers). System headers like `<stdio.h>` come from the macOS SDK path
  baked into the dylib at configure time.

Rebuild: clone https://github.com/TinyCC/tinycc, `./configure && make`, then
`clang -dynamiclib -install_name @rpath/libtcc.dylib -o libtcc.dylib <the libtcc .o files>`.
arm64-macOS only for now; other platforms need their own build.
