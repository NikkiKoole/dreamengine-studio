#!/usr/bin/env bash
# run.sh — build the two MIDI-clock probes, then (with no args) run the self-test that
# proves the whole external-clock path end to end WITHOUT a DAW: midisend generates a
# clock onto the IAC bus, synccheck receives it, and the trace is asserted.
#
#   zsh tools/sync-spike/run.sh                    # build + the end-to-end self-test
#   tools/sync-spike/midimon 20                    # LISTEN: name every transport byte for 20s
#   tools/sync-spike/midisend 128 6 start          # SEND: START, 6s of clock at 128, then STOP
#   tools/sync-spike/midisend 128 6                # SEND: BARE clock, no START (a DAW already playing)
#
# Needs the IAC bus online: Audio MIDI Setup → Window → MIDI Studio → IAC Driver →
# "Device is online". Both probes find the first destination/source whose name contains "IAC",
# which is language-independent (it reads "IAC-besturingsbestand" on a Dutch system).
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../.. && pwd)"

FRAMEWORKS=(-framework CoreMIDI -framework CoreFoundation)
clang -O1 -o midimon  midimon.c  "${FRAMEWORKS[@]}"
clang -O1 -o midisend midisend.c "${FRAMEWORKS[@]}"
echo "built midimon + midisend"
[ $# -gt 0 ] && exit 0

cd "$ROOT"
TRACE="build/sync-spike.jsonl"
# Pre-warm the compile FIRST. Without this the cart is still compiling when the clock starts,
# it joins mid-flow, misses the START, and the run looks like a transport bug that isn't one —
# which cost two confusing test rounds the day this was written.
node tools/play.js synccheck script /dev/null --headless --frames 2 >/dev/null 2>&1
echo "listening with synccheck; sending START + 4s of 128 BPM clock + STOP…"
( node tools/play.js synccheck run --frames 900 --headless --trace "$TRACE" >/dev/null 2>&1 & )
sleep 7
tools/sync-spike/midisend 128 4 start >/dev/null 2>&1
sleep 7

node -e '
const fs = require("fs");
// NB: --trace stores every watch() value as a STRING ("1", not 1), because watch takes a printf
// format. Coerce, or a `=== 1` assertion silently never fires (it silently never fired here first).
const rows = fs.readFileSync(process.argv[1], "utf8").trim().split("\n")
  .map(JSON.parse).filter(r => r.w && r.w.xport !== undefined)
  .map(r => ({ f: r.f, w: { act: +r.w.act, play: +r.w.play, xport: +r.w.xport,
                            bpm: +r.w.bpm, beats: +r.w.beats } }));
const hit = (f) => rows.some(f);
const checks = [
  ["clock arrived (sync_active)",            hit(r => r.w.act === 1)],
  ["START seen (sync_transport)",            hit(r => r.w.act === 1 && r.w.xport === 1)],
  ["ran while playing",                      hit(r => r.w.xport === 1 && r.w.play === 1)],
  ["STOP followed (play went 0, clock live)", hit(r => r.w.act === 1 && r.w.xport === 1 && r.w.play === 0)],
  ["beats advanced past 2",                  hit(r => r.w.beats > 2)],
  ["tempo measured near 128 (±15, the sender jitters)",
                                             hit(r => Math.abs(r.w.bpm - 128) < 15)],
  ["control handed back after the clock left", hit(r => r.f > 0 && r.w.act === 0 && r.w.beats > 2)],
];
let bad = 0;
for (const [name, ok] of checks) { console.log(`  ${ok ? "✓" : "✗"} ${name}`); if (!ok) bad++; }
console.log(bad ? `\n${bad} check(s) FAILED — is the IAC bus online?` : "\nPASS — the real MIDI-clock path works end to end.");
process.exit(bad ? 1 : 0);
' "$TRACE"
