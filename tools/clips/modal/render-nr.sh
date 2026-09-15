#!/usr/bin/env bash
# Render the modal proof clips on Linux DE_NO_RAYLIB (no Raylib, no xxd, no window).
#
#   bash tools/clips/modal/render-nr.sh [/opt/cursor/artifacts]
#
# Mac / a machine with Raylib: use play.js instead —
#   node tools/play.js modal script tools/clips/modal/01-marimba-strike.script \
#        --headless --frames 180 --wav /opt/cursor/artifacts/modal-marimba-strike.wav
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="${1:-/opt/cursor/artifacts}"
mkdir -p "$OUT" build

if [ ! -f build/sprites_data.h ] || [ ! -f build/map_data.h ]; then
  node -e '
    const fs = require("fs");
    const mk = require("./tools/make-cart.js");
    function cHeader(symbol, buf) {
      const lines = [];
      for (let i = 0; i < buf.length; i += 12)
        lines.push("  " + [...buf.slice(i, i + 12)].map(b => "0x" + b.toString(16).padStart(2, "0")).join(", "));
      fs.writeFileSync("build/" + (symbol === "SPRITES_DATA" ? "sprites_data.h" : "map_data.h"),
        "static const unsigned char " + symbol + "[] = {\n" + lines.join(",\n") + "\n};\n" +
        "static const unsigned int  " + symbol + "_LEN = " + buf.length + ";\n");
    }
    cHeader("SPRITES_DATA", mk.makeBlankSpritePng());
    cHeader("MAP_DATA", Buffer.alloc(8192));
  '
fi

cp tools/carts/modal.c build/cart.c
# -lm AFTER the objects — gcc drops the library if it appears first.
CC="${CC:-clang}"
command -v "$CC" >/dev/null 2>&1 || CC=gcc
$CC -O2 tools/clips/modal/render-nr.c runtime/studio.c runtime/raylib_compat.c build/cart.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 \
  -DSCALE=1 -DSCREEN_W=320 -DSCREEN_H=200 -DMAP_W=128 -DMAP_H=64 -DCELL_W=16 -DCELL_H=16 \
  -o build/modal-nr-script -lm -lpthread

# script:wav-stem:frames
pairs=(
  "01-marimba-strike:modal-marimba-strike:180"
  "02-breath-blow:modal-breath-blow:180"
  "03-bowed:modal-bowed:240"
  "04-map-a:modal-map-a:280"
  "05-map-b:modal-map-b:280"
  "06-walk:modal-walk:360"
)
for pair in "${pairs[@]}"; do
  script="${pair%%:*}"
  rest="${pair#*:}"
  name="${rest%%:*}"
  frames="${rest##*:}"
  build/modal-nr-script "$frames" "tools/clips/modal/${script}.script" "$OUT/${name}.wav"
done
echo "wrote 6 WAVs under $OUT"
