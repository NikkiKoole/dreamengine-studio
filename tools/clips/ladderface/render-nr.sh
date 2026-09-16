#!/usr/bin/env bash
# Render the ladderface proof clips on Linux DE_NO_RAYLIB (no Raylib, no xxd, no window).
#
#   bash tools/clips/ladderface/render-nr.sh [/opt/cursor/artifacts]
#
# Mac / a machine with Raylib: use play.js instead —
#   node tools/play.js ladderface script tools/clips/ladderface/01-face-rest.script \
#        --headless --frames 180 --wav /opt/cursor/artifacts/ladderface-face-rest.wav
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

cp tools/carts/ladderface.c build/cart.c
# -lm AFTER the objects — gcc drops the library if it appears first.
CC="${CC:-clang}"
command -v "$CC" >/dev/null 2>&1 || CC=gcc
$CC -O2 tools/clips/ladderface/render-nr.c runtime/studio.c runtime/raylib_compat.c build/cart.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 -DDE_RESIZABLE=1 \
  -DSCALE=1 -DSCREEN_W=200 -DSCREEN_H=320 -DMAP_W=128 -DMAP_H=64 -DCELL_W=16 -DCELL_H=16 \
  -o build/ladderface-nr-script -lm -lpthread

# script:wav-stem:frames
pairs=(
  "01-face-rest:ladderface-face-rest:180"
  "02-play-ribbon:ladderface-play-ribbon:240"
  "03-res-scream:ladderface-res-scream:240"
)
shot=""
for pair in "${pairs[@]}"; do
  script="${pair%%:*}"
  rest="${pair#*:}"
  name="${rest%%:*}"
  frames="${rest##*:}"
  extra=()
  if [ "$script" = "01-face-rest" ] || [ "$script" = "02-play-ribbon" ] || [ "$script" = "03-res-scream" ]; then
    extra=("$OUT/${name}.ppm")
  fi
  if [ "$script" = "02-play-ribbon" ]; then
    shot="$OUT/${name}.ppm"
  fi
  build/ladderface-nr-script "$frames" "tools/clips/ladderface/${script}.script" "$OUT/${name}.wav" "${extra[@]+"${extra[@]}"}"
done

ppm_to_png() {
  local src="$1" dest="$2"
  python3 - "$src" "$dest" <<'PY'
import sys, zlib, struct
path, out = sys.argv[1], sys.argv[2]
data = open(path, "rb").read()
assert data.startswith(b"P6")
parts = data.split(b"\n", 3)
wh = parts[1].split()
w, h = int(wh[0]), int(wh[1])
raw = parts[3]
def chunk(tag, payload):
    crc = zlib.crc32(tag + payload) & 0xffffffff
    return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)
raw_lines = b"".join(b"\x00" + raw[i*w*3:(i+1)*w*3] for i in range(h))
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(raw_lines, 9))
png += chunk(b"IEND", b"")
open(out, "wb").write(png)
print("wrote", out, w, "x", h)
PY
}

for stem in ladderface-face-rest ladderface-play-ribbon ladderface-res-scream; do
  if [ -f "$OUT/${stem}.ppm" ]; then
    ppm_to_png "$OUT/${stem}.ppm" "$OUT/${stem}.png"
  fi
done

echo "wrote 3 WAVs under $OUT"
echo "regenerate: bash tools/clips/ladderface/render-nr.sh [/opt/cursor/artifacts]"
