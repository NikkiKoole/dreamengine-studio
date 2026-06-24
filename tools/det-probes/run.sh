#!/usr/bin/env bash
# Determinism oracles for the software-canvas rasterizers (docs/design/software-canvas.md).
# Each probe hashes a rendered pixel buffer in-C (FNV-1a) and prints a one-line signature.
# The POINT: the same source compiled for arm64 / x86-64 / wasm must print the SAME hash —
# that bit-identity is the precondition for goal-B (replays/ghosts/lockstep on any device).
# Requires: clang, emcc (emscripten), node. x86-64 row runs under Rosetta on Apple Silicon.
# Usage: bash tools/det-probes/run.sh   (CWD-independent). Exit 0 = all bit-identical & match.
set -e
cd "$(dirname "$0")"

# expected signatures (standard builds, NO -ffast-math) — the regression gate.
expect() {
  case "$1" in
    detstress) echo "hash=3b33d06bd74009f3 set=57786";;
    stritex)   echo "hash=96b35a16a8b0927f set=25406 | tiling: once=23681 twice(OVERLAP)=0 gap=0";;
    rotstroke) echo "hash=5e00a7b6d35f00e3 angles=360 worst_components=1 broken_angles=0";;
    rotfill)   echo "hash=3d0119d344272813 | INVERSE: area 6122..6168 comps<=1 | FORWARD worst_holes=1137";;
    rotline)   echo "hash=f93ea939dda443a3 | worst_excess=0 worst_components=1 max_churn_per_deg=268";;
    rotspr)    echo "hash=e7bdd988fd0b7357 | NEAREST drop=16 frame_comps<=7 dot=308/360 | SUPER dot=252/360 | ROTSPRITE frame_comps<=2 dot=156/360";;
    textrot)   echo "hash=b57953ef3ef3aca3 | worst components (1=intact): NEAREST=1 SUPER=1 ROTSPRITE=1";;
  esac
}

fail=0
for p in detstress stritex rotstroke rotfill rotline rotspr textrot; do
  echo "== $p =="
  clang -O2 "$p.c" -o "/tmp/$p.arm"  2>/dev/null
  clang -arch x86_64 -O2 "$p.c" -o "/tmp/$p.x86" 2>/dev/null || true
  emcc  -O2 "$p.c" -o "/tmp/$p.js"   2>/dev/null
  a="$(/tmp/$p.arm)"
  x="$(arch -x86_64 "/tmp/$p.x86" 2>/dev/null || echo '(no x86)')"
  w="$(node "/tmp/$p.js")"
  exp="$(expect "$p")"
  printf '  arm64   %s\n  x86-64  %s\n  wasm    %s\n' "$a" "$x" "$w"
  if [ "$a" != "$exp" ] || [ "$w" != "$a" ] || { [ "$x" != "(no x86)" ] && [ "$x" != "$a" ]; }; then
    echo "  ^ MISMATCH (expected: $exp)"; fail=1
  else echo "  ^ OK (bit-identical across targets)"; fi
done
exit $fail
