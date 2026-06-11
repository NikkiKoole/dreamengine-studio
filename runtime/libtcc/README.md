# Vendored libtcc (macOS)

Prebuilt from TinyCC **mainline** (the `mob` branch — the stable 0.9.27 release is
x86-only and deprecated). Used by the **live (libtcc) backend**: studio.c compiled with
`-DDE_TCC` JIT-compiles the cart in-process for instant hot-reload. See
`docs/design/cart-as-script.md`.

## Layout

`libtcc.h` is shared (arch-independent API header). The binary artifacts are
**per-arch**, in `arm64/` and `x64/` — `main.cjs` picks the dir by `process.arch`:

```
libtcc/
├── libtcc.h            # shared — the API header studio.c includes
├── arm64/              # Apple Silicon
│   ├── libtcc.dylib
│   └── tcclib/         # CONFIG_TCCDIR: libtcc1.a + include/
└── x64/                # Intel
    ├── libtcc.dylib
    └── tcclib/
```

- `libtcc.dylib` — the compiler as a shared lib (install_name `@rpath/libtcc.dylib`;
  built as a dylib so its internal globals — e.g. `load` — don't clash with studio's).
- `tcclib/` — CONFIG_TCCDIR: `libtcc1.a` (compiler runtime) + `include/` (tcc's own
  freestanding headers). System headers like `<stdio.h>` come from the macOS SDK path
  baked into the dylib at configure time — **this is why each arch needs its own build,
  produced on that arch's machine** (the SDK path differs and the codegen is native).

## Rebuild (per arch, on a machine of that arch)

```sh
git clone https://github.com/TinyCC/tinycc && cd tinycc
./configure && make
clang -dynamiclib -install_name @rpath/libtcc.dylib -o libtcc.dylib \
  libtcc.o tccpp.o tccgen.o tccdbg.o tccelf.o tccasm.o tccrun.o \
  <arch>-gen.o <arch>-link.o i386-asm.o tccmacho.o -lm -ldl -lpthread
```

Then copy `libtcc.dylib`, `libtcc1.a`, and `include/*` into the matching
`arm64/` or `x64/` subdir here. (`<arch>-gen.o`/`<arch>-link.o` are `arm64-*` on Apple
Silicon, `x86_64-*` on Intel.)
