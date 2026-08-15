#!/usr/bin/env bash
# ============================================================================
# param-check — the gate for HOST PARAMETERS reaching a cart's knobs.
#
#   bash tools/param-check/run.sh          # the gate
#   bash tools/param-check/run.sh -v       # keep the renders + print every number
#
# WHAT IT COVERS. Until 2026-08-15 the AUv3 exposed no AUParameterTree at all, so a DAW saw ZERO
# parameters: nothing on the rack was automatable or recordable and the lane menu was empty. A cart
# now binds floats it already owns (`param_bind`, runtime/param.h) and this asserts the ENGINE half
# of that chain: the declaration table, the queue, the frame drain, the range clamp, and that a
# written value reaches the DSP and not merely the variable.
#
# WHY IT CAN RUN AT ALL: `--param <addr>@<frame>=<value>` (runtime/studio.c → param_sched_add) goes
# through de_param_set, the same queue a host's automation lane writes into. Before that flag,
# "a DAW automated this knob" was testable only through a real DAW.
#
# ⚠ THIS IS THE ENGINE HALF ONLY. The AUParameterTree itself — that a host SEES the parameters, with
# names and addresses, and that writing one through the tree moves the mix — is proven out of process
# by `./au-transport-check --params` in ios/mac.sh, which is the only place the whole chain is real.
# A green run here says nothing about whether a host can see any of it.
#
# ⚠ EVERY RUN CLEARS build/saves/acidcandy FIRST — the rack AUTOSAVES, so run N+1 boots from run N's
# session and two "identical" renders differ. Same trap as midi-note-check; see its header.
#
# NEGATIVE CONTROLS, because "the audio differs" needs a floor:
#   · two untouched renders must be BYTE-IDENTICAL
#   · a write to an addr NOBODY BOUND must change nothing (else any traffic through the queue looks
#     like a working parameter)
# ============================================================================
set -u
cd "$(dirname "$0")/../.." || exit 1

VERBOSE=0; [ "${1:-}" = "-v" ] && VERBOSE=1
OUT=$(mktemp -d); trap '[ "$VERBOSE" = 1 ] || rm -rf "$OUT"' EXIT
PASS=0; FAIL=0
ok()  { PASS=$((PASS+1)); printf '  \033[32m✓\033[0m %s\n' "$1"; }
bad() { FAIL=$((FAIL+1)); printf '  \033[31m✗\033[0m %s\n' "$1"; }
note(){ [ "$VERBOSE" = 1 ] && printf '      %s\n' "$1"; return 0; }

render() { local wav=$1; shift; rm -rf build/saves/acidcandy
           node tools/play.js acidcandy script /dev/null --headless --frames 240 --wav "$wav" "$@" >/dev/null 2>&1; }
trace()  { rm -rf build/saves/acidcandy
           node tools/play.js acidcandy script /dev/null --headless --frames 120 "$@" >/dev/null 2>&1; }
sha()      { shasum "$1" | cut -d' ' -f1; }
centroid() { node tools/wav-envelope.js "$1" 2>/dev/null | grep -o 'centroid [0-9]* Hz' | head -1 | tr -cd '0-9'; }
w() { node -e '
const fs=require("fs");
const L=fs.readFileSync("build/acidcandy.trace.jsonl","utf8").trim().split("\n")
        .map(s=>{try{return JSON.parse(s)}catch(e){return null}}).filter(x=>x&&x.w);
const r=L.find(l=>l.f===+process.argv[1]);
process.stdout.write(r ? String(r.w[process.argv[2]]) : "?");' "$1" "$2"; }

echo "param-check — host parameters → acidcandy's knobs"
echo

echo "1. the cart declares its parameters"
trace
N=$(w 2 npar)
[ "$N" = "21" ] && ok "21 parameters bound" || bad "param_count()=$N (want 21 — did bind_params change?)"

echo
echo "2. a host write reaches the knob, and out-of-range clamps"
trace --param 1@30=0.05 --param 10@50=0.95 --param 1@70=2.5
[ "$(w 29 pflt)" = "0.500" ] && ok "untouched before the write"        || bad "pflt=$(w 29 pflt) before any write (want 0.500)"
[ "$(w 31 pflt)" = "0.050" ] && ok "addr 1 (master FLT) takes the value" || bad "pflt=$(w 31 pflt) after the write (want 0.050)"
[ "$(w 51 pcut)" = "0.950" ] && ok "addr 10 (303a CUT) is a different knob" || bad "pcut=$(w 51 pcut) after the write (want 0.950)"
[ "$(w 71 pflt)" = "1.000" ] && ok "2.5 CLAMPS to the declared max"    || bad "pflt=$(w 71 pflt) for an out-of-range write (want 1.000)"

echo
echo "3. it reaches the DSP, not just the variable"
render "$OUT/a.wav"
render "$OUT/b.wav"
A=$(sha "$OUT/a.wav"); B=$(sha "$OUT/b.wav")
[ "$A" = "$B" ] && ok "two untouched renders are byte-identical (the floor)" \
                || bad "two untouched renders DIFFER — nothing below means anything"
render "$OUT/unbound.wav" --param 999@30=1.0
[ "$(sha "$OUT/unbound.wav")" = "$A" ] && ok "a write to an UNBOUND addr changes nothing" \
                                       || bad "an unbound addr changed the render — the table is not being consulted"
render "$OUT/moved.wav" --param 1@30=0.05
[ "$(sha "$OUT/moved.wav")" != "$A" ] && ok "a bound write changes the render" \
                                      || bad "a bound write changed nothing — it never reached the DSP"

echo
echo "4. and it does the RIGHT thing (the master filter opens as the value rises)"
PREV=0; MONO=1; SEQ=""
for v in 0.05 0.20 0.35 0.50; do
  render "$OUT/sw.wav" --param 1@10=$v
  C=$(centroid "$OUT/sw.wav"); SEQ="$SEQ $C"
  [ -n "$C" ] || { MONO=0; break; }
  [ "$C" -gt "$PREV" ] || MONO=0
  PREV=$C
done
note "centroid Hz:$SEQ"
[ "$MONO" = 1 ] && ok "centroid rises monotonically as FLT opens:$SEQ Hz" \
                || bad "the filter did not open monotonically:$SEQ Hz"
# The whole-mix centroid IS the right measure here, unlike the per-voice questions in
# midi-note-check: this parameter is a MASTER filter, so the whole mix is exactly what it moves.

echo
if [ "$FAIL" -gt 0 ]; then printf '\033[31mFAIL\033[0m  %d passed, %d failed\n' "$PASS" "$FAIL"; exit 1; fi
printf '\033[32mPASS\033[0m  %d assertions\n' "$PASS"
[ "$VERBOSE" = 1 ] && echo "renders kept in $OUT"
exit 0
