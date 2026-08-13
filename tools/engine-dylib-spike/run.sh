#!/usr/bin/env bash
# run.sh — SPIKE: can we have N independent engines in ONE process by loading the engine dylib N times?
#
#   bash tools/engine-dylib-spike/run.sh              # acidcandy twice + epiano as the bonus
#   CART=tb303 CART2=moog bash tools/engine-dylib-spike/run.sh
#
# THE QUESTION IT DECIDES. An AUv3 on two GarageBand tracks is two audio units in ONE extension
# process (measured — docs/design/ios-plan.md), and engine state is process-global (~204 file-scope
# statics in studio.c/sound.h, plus every cart's own), so the two tracks fight over one rack: "the
# sound goes weird". The textbook fix is globals → a context struct, which is unlandable here (it
# touches the two hottest shared files plus 553 carts plus every determinism gate downstream).
#
# This tests the alternative. dyld keys loaded images by FILE, not by symbol, so two COPIES of one
# dylib are two images with two data segments — every static duplicated, including each cart's own,
# with ZERO changes to studio.c, sound.h, or any cart. If that holds for the real engine, the
# multi-instance fix is packaging rather than a refactor.
#
# WHY COPIES SHIPPED IN THE BUNDLE rather than copied at runtime: a sandboxed, hardened-runtime app
# extension may not be allowed to dlopen a file it just wrote. Pre-built, pre-signed copies
# (engine1.dylib … engineK.dylib) remove that question entirely, and cap instances at K — instance
# K+1 refuses politely instead of garbling. This spike builds one dylib and COPIES it, which is
# exactly that shape.
#
# WHAT IT DOES NOT COVER, and both need their own step before this is a plan:
#   · dlopen from inside the sandboxed .appex with library validation on (this runs unsandboxed).
#   · the per-instance things that are NOT C globals — the Swift-side frame worker (one static per
#     process today), the CoreMIDI virtual source (K instances would publish K same-named sources),
#     and save_bytes (K instances, one cart.blob; de_set_save_dir exists to fix it).
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../.. && pwd)"

CART="${CART:-acidcandy}"
CART2="${CART2:-epiano}"

dims() {   # $1 = cart → "SW SH CW CH MW MH"
  ( cd "$ROOT" && node -e '
    const m = require("./tools/make-cart.js");
    const c = m.loadConfig("tools/carts/'"$1"'.c");
    process.stdout.write([c.screenW??320, c.screenH??200, c.cellW??16, c.cellH??16,
                          c.mapW??128, c.mapH??64].join(" "))' )
}

# Stage a cart's generated C into our OWN dir. play.js writes $ROOT/build/cart.c, a path shared with
# every other agent in this tree — so VERIFY the slug we got is the cart we asked for and retry, the
# same guard mac.sh carries after a sibling's build silently swapped the cart under it.
stage() {   # $1 = cart, $2 = dest dir
  local got
  for attempt in 1 2 3; do
    ( cd "$ROOT" && node tools/play.js "$1" run --headless --frames 1 >/dev/null 2>&1 ) || { sleep 2; continue; }
    got=$(grep -m1 '"slug"' "$ROOT/build/cart.c" | sed 's/.*"slug"[^"]*"\([^"]*\)".*/\1/')
    [ "$got" = "$1" ] && break
    echo "  attempt $attempt: build/cart.c holds '$got', not '$1' — another agent won the race; retrying"
    sleep 2
  done
  [ "$got" = "$1" ] || { echo "✗ could not stage '$1'"; exit 1; }
  mkdir -p "$2"
  find "$2" -maxdepth 1 -type f \( -name '*.c' -o -name '*.h' \) -delete
  cp "$ROOT/build/cart.c" "$ROOT/build/sprites_data.h" "$ROOT/build/map_data.h" "$2/"
}

# The engine as a SHARED LIBRARY: exactly build-nr.sh's translation units minus tools/headless-nr.c,
# which only supplies main(). Nothing about the engine changes to be loadable this way.
build_dylib() {   # $1 = cart, $2 = gen dir, $3 = output
  read -r SW SH CW CH MW MH <<< "$(dims "$1")"
  echo "  building $3 from '$1' (${SW}x${SH})"
  clang -O2 -dynamiclib -o "$3" \
    "$ROOT/runtime/studio.c" "$ROOT/runtime/raylib_compat.c" "$2/cart.c" \
    -I "$ROOT/runtime" -I "$2" \
    -DDE_NO_RAYLIB \
    -DSCREEN_W="$SW" -DSCREEN_H="$SH" -DSCALE=2 \
    -DMAP_W="$MW" -DMAP_H="$MH" -DCELL_W="$CW" -DCELL_H="$CH" \
    -lm -framework CoreMIDI -framework CoreFoundation 2>&1 | grep -v '^$' || true
  # ⚠ The two frameworks are NOT optional and NOT decoration: midi_output.h publishes a CoreMIDI
  # virtual source and, unlike midi_input.h, is deliberately NOT gated off under DE_NO_RAYLIB. The
  # same omission had build-nr.sh failing to link at HEAD. If K engines each publish a source they
  # will collide by NAME — one of the not-covered items in this file's header.
  [ -f "$3" ] || { echo "✗ $3 did not build"; exit 1; }
}

echo "▸ staging '$CART' and '$CART2'…"
stage "$CART"  gen1
stage "$CART2" gen2

echo "▸ building the engine as a dylib…"
build_dylib "$CART"  gen1 libengine_a.dylib
cp libengine_a.dylib libengine_b.dylib      # THE SHIPPING SHAPE: one build, K signed copies
build_dylib "$CART2" gen2 libengine_c.dylib

echo "▸ building the probe host…"
clang -O2 -o probe probe.c -I "$ROOT/ios/Sources"

echo
./probe ./libengine_a.dylib ./libengine_b.dylib ./libengine_c.dylib
