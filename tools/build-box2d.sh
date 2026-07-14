#!/usr/bin/env bash
# build-box2d.sh — build the vendored Box2D v3 (runtime/box2d/) into a per-target
# static lib, the same "one lib per platform" model raylib already uses in this repo.
#
# Box2D v3 is pure C, MIT, zero OS deps (no windowing/GL/I/O — just math), so the
# SAME source compiles through every toolchain we ship; only the flags differ. This
# script bakes in the flags proven to compile 35/35 objects on 2026-07-14 (see
# docs/design/box2d-integration.md for the evidence + the wasm SIMD story).
#
# Usage:
#   tools/build-box2d.sh              # macOS host arch (default)
#   tools/build-box2d.sh --win        # Windows via x86_64-w64-mingw32-gcc
#   tools/build-box2d.sh --wasm       # web via emcc (SSE2->wasm-simd emulation)
#   tools/build-box2d.sh --wasm-scalar# web, SIMD disabled (portable, a touch slower)
#   tools/build-box2d.sh --ios        # iOS arm64 (iphoneos SDK)
#   tools/build-box2d.sh --check      # macOS build + run the drop-box smoke test
#
# Output: build/box2d/<target>/libbox2d.a  (build/ is gitignored — rebuild on demand).
# A cart links it with:  -I runtime/box2d/include  build/box2d/<target>/libbox2d.a
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$REPO/runtime/box2d/src"
INC="$REPO/runtime/box2d/include"
STD="-std=gnu17"          # gnu (not c17) so timer.c's clock_gettime is exposed
OPT="-O2"

target="${1:---mac}"
out="$REPO/build/box2d/mac"
CC=(clang "$STD" "$OPT" -I"$INC" -I"$SRC")

case "$target" in
  --mac|"")     out="$REPO/build/box2d/mac" ;;
  --win)        out="$REPO/build/box2d/win";   CC=(x86_64-w64-mingw32-gcc "$STD" "$OPT" -I"$INC" -I"$SRC") ;;
  --wasm)       out="$REPO/build/box2d/wasm";  CC=(emcc "$STD" "$OPT" -msimd128 -msse2 -I"$INC" -I"$SRC") ;;
  --wasm-scalar)out="$REPO/build/box2d/wasm";  CC=(emcc "$STD" "$OPT" -DBOX2D_DISABLE_SIMD -I"$INC" -I"$SRC") ;;
  --ios)        out="$REPO/build/box2d/ios";   IOS_SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
                CC=(clang -arch arm64 -isysroot "$IOS_SDK" -miphoneos-version-min=13.0 "$STD" "$OPT" -I"$INC" -I"$SRC") ;;
  --check)      out="$REPO/build/box2d/mac" ;;   # handled after the build below
  *) echo "unknown target: $target"; exit 2 ;;
esac

rm -rf "$out"; mkdir -p "$out"
echo "[box2d] compiling 35 sources -> $out/libbox2d.a"
( cd "$out" && "${CC[@]}" -c "$SRC"/*.c )
ar rcs "$out/libbox2d.a" "$out"/*.o
n=$(ls "$out"/*.o | wc -l | tr -d ' ')
echo "[box2d] OK: $n objects, $(du -h "$out/libbox2d.a" | cut -f1)"

if [ "$target" = "--check" ]; then
  echo "[box2d] smoke test: drop a box, it should settle at y ~= 0.5"
  clang "$STD" "$OPT" -I"$INC" "$REPO/runtime/box2d/smoke-check.c" "$out/libbox2d.a" -o "$out/smoke"
  "$out/smoke"
fi
