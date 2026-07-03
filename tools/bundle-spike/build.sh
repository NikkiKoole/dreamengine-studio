#!/usr/bin/env bash
# build.sh — the TWO CARTS, ONE BINARY spike (docs/design/share-panel.md, seam #1).
#
# Stages acidrack + yachtrack UNMODIFIED, compiles each TU with renamed entry points
# (-Ddraw=<slug>_draw -Dupdate=<slug>_update — everything else in a cart is `static`,
# so two carts link clean side by side), and links them with bundle.c (the ~30-line
# dispatcher shim that owns the real init/update/draw) + studio.c into ONE binary.
#
#   tools/bundle-spike/build.sh          # build → build/bundle-spike
#   build/bundle-spike                   # run: boots acidrack, TAB switches racks
#   DE_BUNDLE_AUTOSWITCH=60 build/bundle-spike --headless   # deterministic proof hook
#
# PASSED 2026-07-03 (proof-acid.png / proof-yacht.png — same binary, one env flag).
# Findings + what this spike deliberately does NOT cover (differing screen dims,
# per-cart sprite sheets, de_state carts, FX-bus bleed): share-panel.md §spike.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
S="$ROOT/build/.bundle-spike"; mkdir -p "$S"
RAYLIB="$(brew --prefix raylib)"

# stage the shared sheet/map (acidrack's — both racks are code-drawn, the sheet is inert)
node -e '
const fs = require("fs"), mk = require(process.argv[1] + "/tools/make-cart.js")
const cfg = mk.loadConfig(process.argv[1] + "/tools/carts/acidrack.c")
fs.writeFileSync(process.argv[2] + "/sprites.png",
  cfg.sprites ? mk.buildSpriteSheet(cfg.sprites, cfg.charMap) : mk.makeBlankSpritePng())
fs.writeFileSync(process.argv[2] + "/map.dat", Buffer.alloc(8192))
' "$ROOT" "$S"
(cd "$S" && xxd -i sprites.png) | sed \
  -e 's/unsigned char sprites_png\[\]/static const unsigned char SPRITES_DATA[]/' \
  -e 's/unsigned int sprites_png_len/static const unsigned int  SPRITES_DATA_LEN/' > "$S/sprites_data.h"
(cd "$S" && xxd -i map.dat) | sed \
  -e 's/unsigned char map_dat\[\]/static const unsigned char MAP_DATA[]/' \
  -e 's/unsigned int map_dat_len/static const unsigned int  MAP_DATA_LEN/' > "$S/map_data.h"

cp "$ROOT/tools/carts/acidrack.c"  "$S/acid.c"
cp "$ROOT/tools/carts/yachtrack.c" "$S/yacht.c"

COMMON=(-I"$ROOT/runtime" -I"$S" -I"$RAYLIB/include"
  -DSCREEN_W=320 -DSCREEN_H=240 -DSCALE=3 -DMAP_W=128 -DMAP_H=64 -DCELL_W=16 -DCELL_H=16
  -DTOUCH_CONTROLS_DEFAULT=0 -DSCALE_FILTER=0 -O2 -fno-delete-null-pointer-checks)

clang -c "$S/acid.c"   -o "$S/acid.o"   "${COMMON[@]}" -Ddraw=acid_draw  -Dupdate=acid_update  -Dinit=acid_init
clang -c "$S/yacht.c"  -o "$S/yacht.o"  "${COMMON[@]}" -Ddraw=yacht_draw -Dupdate=yacht_update -Dspec=yacht_spec
clang -c "$(dirname "$0")/bundle.c" -o "$S/bundle.o" "${COMMON[@]}"
clang "$S/bundle.o" "$S/acid.o" "$S/yacht.o" "$ROOT/runtime/studio.c" "${COMMON[@]}" \
  "$RAYLIB/lib/libraylib.a" \
  -framework OpenGL -framework Cocoa -framework IOKit \
  -framework CoreVideo -framework CoreFoundation -framework CoreMIDI \
  -Wl,-dead_strip -o "$ROOT/build/bundle-spike" 2> >(grep -v "was built for newer macOS version" >&2 || true)
echo "✓ built build/bundle-spike  (TAB switches racks; DE_BUNDLE_AUTOSWITCH=<frame> for headless proof)"
