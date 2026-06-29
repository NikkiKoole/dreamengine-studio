#!/usr/bin/env bash
# build-nr.sh — compile + run the DE_NO_RAYLIB (no-Raylib) software engine on desktop.
#
# The desktop proof of the iOS Phase-2 path: the real studio.c + sound.h + a cart,
# built with ZERO Raylib / ZERO frameworks (only libc + libm), driven by the headless
# harness (tools/headless-nr.c) which calls the platform.h seam (de_init/de_frame/
# de_framebuffer/de_audio_render) exactly as the iOS CanvasView/CoreAudio will.
#
# It first runs play.js to (re)generate build/{cart.c,sprites_data.h,map_data.h} for the
# cart, then compiles them with studio.c + raylib_compat.c. This is also the canonical
# record of the source list + flags the iOS project.yml mirrors.
#
#   tools/build-nr.sh <cart> [frames] [out.ppm] [out.wav]
# e.g. tools/build-nr.sh omnichord 120 build/nr.ppm build/nr.wav
set -euo pipefail
cd "$(dirname "$0")/.."

CART="${1:-omnichord}"
FRAMES="${2:-1}"
PPM="${3:-build/nr.ppm}"
WAV="${4:-}"

# screen dims from the cart config (defaults 320x200, 16x16 cells, 128x64 map)
SW=$(node -e "const m=require('./tools/make-cart.js');const c=m.loadConfig('tools/carts/$CART.c');process.stdout.write(String(c.screenW??320))")
SH=$(node -e "const m=require('./tools/make-cart.js');const c=m.loadConfig('tools/carts/$CART.c');process.stdout.write(String(c.screenH??200))")
CW=$(node -e "const m=require('./tools/make-cart.js');const c=m.loadConfig('tools/carts/$CART.c');process.stdout.write(String(c.cellW??16))")
CH=$(node -e "const m=require('./tools/make-cart.js');const c=m.loadConfig('tools/carts/$CART.c');process.stdout.write(String(c.cellH??16))")
MW=$(node -e "const m=require('./tools/make-cart.js');const c=m.loadConfig('tools/carts/$CART.c');process.stdout.write(String(c.mapW??128))")
MH=$(node -e "const m=require('./tools/make-cart.js');const c=m.loadConfig('tools/carts/$CART.c');process.stdout.write(String(c.mapH??64))")

# (re)generate build/cart.c + sprites_data.h + map_data.h for the cart
node tools/play.js "$CART" run --headless --frames 1 >/dev/null 2>&1 || true

echo "compiling $CART ($SW x $SH) with DE_NO_RAYLIB ..."
clang -O2 \
  tools/headless-nr.c runtime/studio.c runtime/raylib_compat.c build/cart.c \
  -I runtime -I build \
  -DDE_NO_RAYLIB \
  -DSCREEN_W=$SW -DSCREEN_H=$SH -DSCALE=2 \
  -DMAP_W=$MW -DMAP_H=$MH -DCELL_W=$CW -DCELL_H=$CH \
  -lm \
  -o build/headless-nr

echo "running $FRAMES frame(s) ..."
build/headless-nr "$FRAMES" "$PPM" $WAV
echo "wrote $PPM ${WAV:+and $WAV}"
