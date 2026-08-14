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

# ── MIGRATION: does an app UPDATE still load a project saved by the previous version? ────────────────
# This is the update cliff, and it cannot be tested in one process — a build cannot grow its own
# struct. So build the probe a SECOND time with one field APPENDED to the saved slice (-DSC_GROWN,
# standing in for "v1.1 added a knob") and move a blob between the two through a file.
echo
echo "building the GROWN probe (one field appended = the next release) ..."
clang -O1 -g \
  tools/state-check/probe.c tools/state-check/statecart.c \
  runtime/studio.c runtime/raylib_compat.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W=64 -DSCREEN_H=64 -DSC_GROWN=1 \
  -o "$out-grown" -lm -lpthread \
  -framework CoreMIDI -framework CoreFoundation

blob="build/state-check-v1.blob"
"$out" --save "$blob"                       # a blob from the SMALL (shipped) struct
"$out-grown" --load "$blob" --expect migrate # the NEXT build must still load it

# NEGATIVE CONTROL, the other direction: a blob from a LARGER struct must be REFUSED, never
# prefix-restored — we cannot know which fields were removed. Without this, "migration works" would
# also pass if the size check had simply been deleted.
blobg="build/state-check-v2.blob"
"$out-grown" --save "$blobg"
"$out" --load "$blobg" --expect refuse

# V1 COMPATIBILITY. Blobs written BEFORE migration existed are sitting in real saved projects, so the
# read path for them is not optional. Build a writer that emits the old format (test-only define) and
# require the shipping build to restore it EXACTLY, which is what v1 promised.
echo
echo "building a PRE-MIGRATION (v1) writer ..."
clang -O1 -g \
  tools/state-check/probe.c tools/state-check/statecart.c \
  runtime/studio.c runtime/raylib_compat.c \
  -I runtime -I build -DDE_NO_RAYLIB=1 -DSCALE=1 -DSCREEN_W=64 -DSCREEN_H=64 -DDE_SS_WRITE_V1=1 \
  -o "$out-v1" -lm -lpthread \
  -framework CoreMIDI -framework CoreFoundation
blob1="build/state-check-oldformat.blob"
"$out-v1" --save "$blob1"
"$out" --load "$blob1" --expect exact
