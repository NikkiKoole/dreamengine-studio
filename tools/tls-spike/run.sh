#!/usr/bin/env bash
# run.sh — what does it COST to reach engine state through a per-instance context?
#
# The AUv3 lane's open decision (docs/HANDOFF.md): an AUv3 puts every plug-in instance in one
# process, so the engine's 545 file-scope statics have to become per-instance state. The macro
# technique is settled; what was NOT measured is how `ctx` should get into scope:
#
#   (b) a _Thread_local pointer   — no signature changes at all, but a lookup on every entry
#   (c) an explicit ctx parameter — 335 signatures + 707 call sites in sound.h, but free access
#
# This times both against the engine as it is today, using a loop shaped like the real per-sample
# block in sound.h: an 8-voice inner loop plus six stage functions called once per sample. Those
# per-sample calls are the whole question, because under (b) each pays its own lookup, ~300k
# times a second.
#
#   bash tools/tls-spike/run.sh
#
# The DSP text lives in body.h and is compiled unchanged into all three variants, so the timing
# difference is the access mechanism and nothing else. See tools/engine-statics.js for the counts.
set -euo pipefail
cd "$(dirname "$0")"

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

# -O2 to match the shipping engine build (build-nr.sh / the AUv3 target)
CFLAGS="-O2 -I."

build() {  # build <src> <entry> <setup> <inline_ok> <out.o>
  clang $CFLAGS -c "$1" -DENTRY="$2" -DSETUP="$3" -DINLINE_OK="$4" -o "$5"
}

echo "building six variants (three access shapes x inlinable or not) ..."
build bench_plain.c run_plain   setup_plain   0 "$OUT/p.o"
build bench_tls.c   run_tls     setup_tls     0 "$OUT/t.o"
build bench_arg.c   run_arg     setup_arg     0 "$OUT/a.o"
build bench_plain.c run_plain_i setup_plain_i 1 "$OUT/pi.o"
build bench_tls.c   run_tls_i   setup_tls_i   1 "$OUT/ti.o"
build bench_arg.c   run_arg_i   setup_arg_i   1 "$OUT/ai.o"

clang $CFLAGS main.c "$OUT"/*.o -o "$OUT/bench"
"$OUT/bench"

echo "the thread-local lookup, as clang emits it on this machine:"
clang $CFLAGS -S bench_tls.c -DENTRY=run_tls -DSETUP=setup_tls -DINLINE_OK=0 -o - 2>/dev/null \
  | awk '/^_stage_echo:/{p=1} p{print} p&&/ret/{exit}' \
  | grep -E 'TLVP|blr|ldr|str' | head -8
echo "  (a 'blr' through a TLVP pointer is the lookup; count how many run per sample)"
