#!/usr/bin/env bash
# run.sh — build + run the present/resize race gate (probe.c carries the full contract).
#
#   bash tools/present-race-check/run.sh           # exit 0 = PASS
#   bash tools/present-race-check/run.sh -tsan     # under ThreadSanitizer — the real gate
#   bash tools/present-race-check/run.sh -bypass   # NEGATIVE CONTROL: the naive host must fail
#
# Run after touching de_copy_frame / de_apply_pending / de_resize / the order of calls in de_frame.
# Builds the real engine the way tools/build-nr.sh does (DE_NO_RAYLIB, no Raylib, no frameworks).
set -e
cd "$(dirname "$0")/../.."
CART="${CART:-acidcandy}"

# generate build/cart.c for the cart, and verify we got the one we asked for: build/ is shared with
# every other agent in this tree, and staging someone else's cart here would silently change what the
# probe exercises (that exact race already cost a confusing red gate in ios/mac.sh).
node tools/play.js "$CART" run --headless --frames 1 >/dev/null 2>&1 || true
got=$(grep -m1 '"slug"' build/cart.c | sed 's/.*"slug"[^"]*"\([^"]*\)".*/\1/')
[ "$got" = "$CART" ] || { echo "✗ build/cart.c holds '$got', not '$CART' — another agent won the race; rerun"; exit 1; }

read -r SW SH <<< "$(node -e '
const mk = require("./tools/make-cart.js");
const c = mk.loadConfig("tools/carts/'"$CART"'.c");
process.stdout.write(`${c.screenW ?? 320} ${c.screenH ?? 200}`)')"

out=/tmp/de-present-race
flags=(-O2 -std=c11 -Wall)
mode=pass
case "$1" in
  -tsan)   flags+=(-fsanitize=thread -g -O1); out=$out-tsan;   echo "▸ ThreadSanitizer build" ;;
  -bypass) flags+=(-DDE_PRESENT_BYPASS);      out=$out-bypass; mode=expect-fail
           echo "▸ NEGATIVE CONTROL: the host blits the LIVE canvas from another thread" ;;
esac

clang "${flags[@]}" \
  tools/present-race-check/probe.c runtime/studio.c runtime/raylib_compat.c build/cart.c \
  -I runtime -I build -I tools -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W="$SW" -DSCREEN_H="$SH" -DDE_RESIZABLE=1 \
  -o "$out" -lm -lpthread

if [ "$mode" = expect-fail ]; then
  if "$out"; then
    echo "✗ the negative control PASSED — the gate cannot detect the bug it exists for"
    exit 1
  fi
  echo "✓ negative control failed as it must (crash or torn reads — that is the bug de_copy_frame fixes)"
  exit 0
fi
"$out"
