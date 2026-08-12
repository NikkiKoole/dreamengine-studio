#!/usr/bin/env bash
# run.sh — build + run the input-ring thread-safety gate (probe.c carries the full contract).
#
#   bash tools/input-ring-check/run.sh           # exit 0 = PASS
#   bash tools/input-ring-check/run.sh -tsan     # again under ThreadSanitizer — the real gate
#   bash tools/input-ring-check/run.sh -bypass   # NEGATIVE CONTROL: the pre-ring design must FAIL
#
# Run this after touching the host input seam in runtime/raylib_compat.c (de_touch_*, de_key_event,
# de_input_beginframe) or the order of calls in de_frame().
#
# WHY THREE MODES. The plain run catches a race only on a run that happens to lose, and races are
# shy: on arm64 the tear check below stayed clean even with the safety removed, because a pair of
# adjacent float stores rarely interleaves. -tsan is what actually proves it — it instruments every
# access and reports the two conflicting stacks even when the timing worked out. And -bypass proves
# the gate can fail at all: it rebuilds the probe with the producer writing engine state directly,
# which is what the code did before the ring, and then the shift check must report thousands of bad
# reads and TSan must name `de_touch` and `de_mouse_down`.
set -e
cd "$(dirname "$0")"
out=/tmp/de-input-ring-check
flags=(-O2 -std=c11 -DDE_NO_RAYLIB=1 -Wall)
mode=pass

case "$1" in
  -tsan)   flags+=(-fsanitize=thread -g -O1); out=$out-tsan;     echo "▸ ThreadSanitizer build" ;;
  -bypass) flags+=(-DDE_IN_RING_BYPASS);      out=$out-bypass;   mode=expect-fail
           echo "▸ NEGATIVE CONTROL: producer writes the pool directly (the pre-ring design)" ;;
esac

cc "${flags[@]}" -o "$out" probe.c -lpthread

if [ "$mode" = expect-fail ]; then
  if "$out"; then
    echo "✗ the negative control PASSED — the gate cannot detect the bug it exists for"
    exit 1
  fi
  echo "✓ negative control failed as it must (the checks above are real)"
  exit 0
fi
"$out"
