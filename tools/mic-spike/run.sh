#!/usr/bin/env zsh
# build + run the Tier-1 mic capture spike (docs/design/mic-and-sampling.md).
# miniaudio's macOS backend needs the CoreAudio/AudioToolbox frameworks; no raylib needed.
#
# MIC PERMISSION (TCC): the first mic access by a given terminal app pops a macOS dialog and
# attributes the grant to THAT app (Terminal/iTerm/Claude Code). If the meter stays flat at
# silence, the grant was denied or never prompted — grant it in System Settings > Privacy &
# Security > Microphone, or run this from your own shell:  ! zsh tools/mic-spike/run.sh
set -e
here=${0:A:h}
out=$here/mic

# miniaudio.h is a 4 MB public-domain single header — fetched on demand, not committed.
if [[ ! -f "$here/miniaudio.h" ]]; then
  echo "fetching miniaudio.h (one-time)…"
  curl -fsSL https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h -o "$here/miniaudio.h"
fi

echo "compiling mic-spike…"
clang "$here/mic.c" -I "$here" -O2 -o "$out" \
  -framework CoreFoundation -framework CoreAudio -framework AudioToolbox \
  -lpthread -lm
echo "ok — running:"
echo
"$out"
