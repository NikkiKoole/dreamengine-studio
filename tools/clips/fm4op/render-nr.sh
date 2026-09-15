#!/usr/bin/env bash
# Render the fm4op proof clips on Linux DE_NO_RAYLIB (no Raylib, no xxd, no window).
#
#   bash tools/clips/fm4op/render-nr.sh [/opt/cursor/artifacts]
#
# Mac / a machine with Raylib: use play.js instead —
#   node tools/play.js fm4op script tools/clips/fm4op/01-tine.script \
#        --headless --frames 180 --wav /opt/cursor/artifacts/fm4op-tine.wav
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

cp tools/carts/fm4op.c build/cart.c
# -lm AFTER the objects — gcc drops the library if it appears first.
CC="${CC:-clang}"
command -v "$CC" >/dev/null 2>&1 || CC=gcc

# spec() first (no Raylib / no xxd). Fail lines print {"pass":0,...}.
$CC -O2 tools/clips/fm4op/spec-nr.c runtime/studio.c runtime/raylib_compat.c build/cart.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 -DDE_SPEC=1 -DDE_TRACE=1 \
  -DSCALE=1 -DSCREEN_W=320 -DSCREEN_H=200 -DMAP_W=128 -DMAP_H=64 -DCELL_W=16 -DCELL_H=16 \
  -o build/fm4op-nr-spec -lm -lpthread
build/fm4op-nr-spec | tee "$OUT/fm4op-spec.jsonl"
if grep -q '"pass":0' "$OUT/fm4op-spec.jsonl"; then
  echo "spec FAILED — see $OUT/fm4op-spec.jsonl" >&2
  exit 1
fi
echo "spec ok ($(grep -c '"pass":1' "$OUT/fm4op-spec.jsonl") assertions)"

$CC -O2 tools/clips/fm4op/render-nr.c runtime/studio.c runtime/raylib_compat.c build/cart.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 \
  -DSCALE=1 -DSCREEN_W=320 -DSCREEN_H=200 -DMAP_W=128 -DMAP_H=64 -DCELL_W=16 -DCELL_H=16 \
  -o build/fm4op-nr-script -lm -lpthread

# script:wav-stem:frames
pairs=(
  "01-tine:fm4op-tine:180"
  "02-bell:fm4op-bell:180"
  "03-brass:fm4op-brass:240"
  "04-map-a:fm4op-map-a:280"
  "05-map-b:fm4op-map-b:280"
  "06-walk:fm4op-walk:360"
)
for pair in "${pairs[@]}"; do
  script="${pair%%:*}"
  rest="${pair#*:}"
  name="${rest%%:*}"
  frames="${rest##*:}"
  build/fm4op-nr-script "$frames" "tools/clips/fm4op/${script}.script" "$OUT/${name}.wav"
done
echo "wrote 6 WAVs under $OUT"
