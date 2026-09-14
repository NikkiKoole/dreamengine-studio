#!/usr/bin/env bash
# build.sh — compile a patch-match host against the REAL engine, headless.
# Same shape as tools/state-check/run.sh: DE_NO_RAYLIB pulls in studio.c without
# a window, CoreMIDI/CoreFoundation because midi_output.h is not gated off there.
#   usage: bash tools/patch-match/build.sh <main.c> <out>
set -euo pipefail
cd "$(dirname "$0")/../.."
# Headless studio.c needs the baked sheet/map headers. A clean tree (and this
# Linux agent image) has no xxd and no prior play.js run, so emit blanks here.
if [ ! -f build/sprites_data.h ] || [ ! -f build/map_data.h ]; then
  mkdir -p build
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
# midi_output.h is a no-op stub off __APPLE__; the two frameworks are only
# needed on Darwin. Linux (this agent's host) links the same sources without them.
LIBS="-lm -lpthread"
if [ "$(uname)" = Darwin ]; then
  LIBS="$LIBS -framework CoreMIDI -framework CoreFoundation"
fi
clang -O2 -g "$1" tools/patch-match/pmcart.c \
  runtime/studio.c runtime/raylib_compat.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W=64 -DSCREEN_H=64 \
  -o "$2" $LIBS
