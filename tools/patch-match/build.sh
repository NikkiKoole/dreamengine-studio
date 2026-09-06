#!/usr/bin/env bash
# build.sh — compile a patch-match host against the REAL engine, headless.
# Same shape as tools/state-check/run.sh: DE_NO_RAYLIB pulls in studio.c without
# a window, CoreMIDI/CoreFoundation because midi_output.h is not gated off there.
#   usage: bash tools/patch-match/build.sh <main.c> <out>
set -euo pipefail
cd "$(dirname "$0")/../.."
clang -O2 -g "$1" tools/patch-match/pmcart.c \
  runtime/studio.c runtime/raylib_compat.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W=64 -DSCREEN_H=64 \
  -o "$2" -lm -lpthread -framework CoreMIDI -framework CoreFoundation
