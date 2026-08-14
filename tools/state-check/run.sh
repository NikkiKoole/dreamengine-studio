#!/usr/bin/env bash
# run.sh — SESSION STATE: does a saved rack come back?
#
# The acceptance test for de_save_state / de_load_state, which is what ios/AU/TinyjamAU.swift's
# fullState is built on. The promise being checked is the user-visible one: reopen a DAW project and
# your rack is as you left it, rather than at factory defaults.
#
#   bash tools/state-check/run.sh
#
# Why it needs its own probe rather than a real cart: the gate has to prove BOTH halves of the
# scratch/saved split, so it needs a rack holding one slice of each with known values. See probe.c's
# header for the four negative controls and what each of them stops from passing for the wrong reason.
#
# Siblings: tools/instance-check (are two instances strangers?) and tools/refactor-guard (did a state
# move change any output?). Neither ever saves anything, so neither can see this.
set -euo pipefail
cd "$(dirname "$0")/../.."

out=build/state-check
echo "building the session-state probe with DE_NO_RAYLIB ..."
# CoreMIDI + CoreFoundation for the same reason build-nr.sh links them: midi_output.h publishes a
# virtual source with the same call on macOS and iOS, so it is not gated off under DE_NO_RAYLIB.
clang -O1 -g \
  tools/state-check/probe.c tools/state-check/statecart.c \
  runtime/studio.c runtime/raylib_compat.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W=64 -DSCREEN_H=64 \
  -o "$out" -lm -lpthread \
  -framework CoreMIDI -framework CoreFoundation

"$out"
