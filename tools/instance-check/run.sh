#!/usr/bin/env bash
# run.sh — can ONE process run N independent engines, from the context refactor?
#
# The acceptance test for the per-instance work (docs/design/engine-instance-seam.md). It is the one
# thing tools/refactor-guard.js structurally CANNOT check: the guard runs a single instance, so it
# proves a state move changed nothing and can never prove two instances are strangers — a variable
# that was wrongly left shared does not change a single-instance render.
#
#   bash tools/instance-check/run.sh              the gate
#   CART=epiano bash tools/instance-check/run.sh  a different cart in both engines
#   ./build/instance-check -bypass                NEGATIVE CONTROL for the destroy section: skip
#                                                 de_instance_destroy, and the heap meter must go red
#
# It also gates DESTROY (2026-08-14): 8 create/destroy rounds must leave the heap flat. That found
# two leaks the day it was written — de_instance_destroy freed the struct and none of its buffers,
# and de_init_impl re-decoded the SHARED sprite sheet + font tables per instance, orphaning the
# previous copy. Against the pre-fix engine this section reports ~1 MB leaked per rack opened.
#
# Sibling of tools/engine-dylib-spike, asserting the same things from a different mechanism: the
# spike got its separation from dyld (two copies of a dylib), this gets it from de_instance_create
# (one image, N instances) — which is what an AUv3 actually needs, without the instance cap.
set -euo pipefail
cd "$(dirname "$0")/../.."

CART="${CART:-acidcandy}"

# regenerate build/cart.c + the sprite/map data for this cart, exactly as build-nr.sh does
node tools/play.js "$CART" run --headless --frames 1 >/dev/null 2>&1 || true

SW=$(node -e "const m=require('./tools/make-cart.js');const c=m.loadConfig('tools/carts/$CART.c');process.stdout.write(String(c.screenW??320))")
SH=$(node -e "const m=require('./tools/make-cart.js');const c=m.loadConfig('tools/carts/$CART.c');process.stdout.write(String(c.screenH??200))")

out=build/instance-check
echo "building $CART with DE_NO_RAYLIB ..."
# CoreMIDI + CoreFoundation for the same reason build-nr.sh links them: midi_output.h publishes a
# virtual source with the same call on macOS and iOS, so it is not gated off under DE_NO_RAYLIB.
clang -O1 -g \
  tools/instance-check/probe.c runtime/studio.c runtime/raylib_compat.c build/cart.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W="$SW" -DSCREEN_H="$SH" \
  -o "$out" -lm -lpthread \
  -framework CoreMIDI -framework CoreFoundation

"$out"
