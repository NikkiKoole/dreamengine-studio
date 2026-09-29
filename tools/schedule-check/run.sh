#!/usr/bin/env bash
# run.sh — THE GATE for schedule_at() / audio_time(): do booked notes land on their sample when the
# frames and the audio callbacks do not line up? (docs/design/audio-timing.md)
#
#   bash tools/schedule-check/run.sh
#
# Builds each probe cart (schedcheck = the bare engine calls, radclockcheck = radio.h's step clock) TWICE against the real engine (DE_NO_RAYLIB, like instance-check)
# and drives it the way a device does — frames at 60 Hz, audio in fixed 1024-sample blocks:
#   SC_MODE=1  schedule_at on audio_time: every click gap must be exactly 11025 samples (±1)
#   SC_MODE=0  NEGATIVE CONTROL, the old way (a schedule_hit delay from a frame clock): the gaps must
#              swing by a good part of a buffer. If they do not, this host is not reproducing the
#              misalignment and mode 1's green proves nothing.
# Why a host and not play.js --wav: the harness pumps exactly 735 samples per frame, so audio and
# frames stay locked and EVERY scheduling method looks perfect there.
set -euo pipefail
cd "$(dirname "$0")/../.."

# two probes through the same host: schedcheck = the bare engine calls, radclockcheck = radio.h's step
# clock (the path the 39 stations use; phase 0 of docs/design/radio-arranger-lessons.md)
fail=0
for CART in schedcheck radclockcheck; do
  echo "── $CART"
  node tools/play.js "$CART" run --headless --frames 1 >/dev/null 2>&1 || true   # regenerate build/cart.c
  for mode in 1 0; do
    out=build/schedule-check-$CART-$mode
    clang -O1 -g \
      tools/schedule-check/probe.c runtime/studio.c runtime/raylib_compat.c build/cart.c \
      -I runtime -I build -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W=320 -DSCREEN_H=200 -DSC_MODE=$mode \
      -o "$out" -lm -lpthread -framework CoreMIDI -framework CoreFoundation
    "$out" || fail=1
  done
done
if [ $fail -eq 0 ]; then echo "schedule-check: PASS"; else echo "schedule-check: FAIL"; fi
exit $fail
