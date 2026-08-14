#!/usr/bin/env bash
# Does a cart-land header's state become PER-INSTANCE when a cart opts in? (docs/design/engine-context.md)
#
# Builds the same probe TWICE: once plain, once with -DDE_CART_CTX. Both must pass, and they assert
# OPPOSITE things — the default path must stay shared (that is what all 553 carts compile, and it
# must not change), the opted-in path must not. A seam checked only in its enabled state is half a
# seam, and the half nobody checks is the one every cart uses.
set -euo pipefail
cd "$(dirname "$0")/../.."
node tools/play.js acidcandy run --headless --frames 1 >/dev/null 2>&1 || true   # for sprites_data.h
fail=0
for mode in default optin; do
  extra=("-DDE_UICTX_UNUSED=1"); [ "$mode" = optin ] && extra=(-DDE_CART_CTX)
  clang -O1 tools/instance-check/uictx.c tools/instance-check/uicart.c \
        runtime/studio.c runtime/raylib_compat.c \
        -I runtime -I build -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W=160 -DSCREEN_H=100 \
        "${extra[@]}" -o "build/uictx-$mode" -lm -lpthread \
        -framework CoreMIDI -framework CoreFoundation
  "build/uictx-$mode" || fail=1
done
[ $fail = 0 ] && echo "PASS — the seam works opted in, and the default path is unchanged." || echo "FAILED"
exit $fail
